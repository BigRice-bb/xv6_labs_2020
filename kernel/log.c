#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;//日志区大小
  log.dev = dev;
  recover_from_log();//恢复之前未完成的commit 若有的话
}

// Copy committed blocks from log to their home location
static void
install_trans(int recovering)//将日志区中的块复制到磁盘上
//recovering==1的时候是恢复日志区,recovering==0的时候是提交日志区
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // 读取日志区中的块--缓存的数据
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // 读取磁盘上的块
    memmove(dbuf->data, lbuf->data, BSIZE);  // 将日志区中的块复制到磁盘上的块
    bwrite(dbuf);  // 将磁盘上的块写入磁盘   buffer_cahe-disk
    if(recovering == 0)
      bunpin(dbuf);//将磁盘上的块的引用计数减一,让buffercahe可以回收这个块
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)//将日志区的头块读入内存中的头块副本
{
  struct buf *buf = bread(log.dev, log.start);//读取日志区的第一个块 也就是头块
  struct logheader *lh = (struct logheader *) (buf->data);//将头块的数据转换为logheader结构体
  int i;
  log.lh.n = lh->n;//日志区中记录的块数
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];//日志区中记录的块号
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)//从日志区中读出头块,并写入磁盘
{
  struct buf *buf = bread(log.dev, log.start);//读出日志区的头块
  struct logheader *hb = (struct logheader *) (buf->data);//将头块的数据转换为logheader结构体
  int i;
  hb->n = log.lh.n;//日志区中记录的块数,将内存中的日志区头块写入磁盘
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];//日志区中记录的块号
  }
  bwrite(buf);//将头块写入磁盘
  brelse(buf);//释放头块
}

static void
recover_from_log(void)
{
  read_head();
  //recovering==1的时候代表一次崩溃返回,将日志区未完成的块复制到磁盘上
  //n只有0和非0
  install_trans(1); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);//获取日志区的锁
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);//如果日志区正在提交,则等待
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);//如果日志区满了,则等待
    } else {
      log.outstanding += 1;//日志区中记录的正在执行的系统调用数加一
      release(&log.lock);//释放日志区的锁
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);//获取日志区的锁
  log.outstanding -= 1;//日志区中记录的正在执行的系统调用数减一
  if(log.committing)//如果日志区正在提交,则等待
    panic("log.committing");
  if(log.outstanding == 0){
    do_commit = 1;//如果日志区中记录的正在执行的系统调用数为0,则可以提交
    log.committing = 1;//日志区正在提交
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    wakeup(&log);//唤醒等待日志区的进程
  }
  release(&log.lock);//释放日志区的锁

  if(do_commit){//如果可以提交,则提交--outstanding==0
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // 读取日志区中的块
    struct buf *from = bread(log.dev, log.lh.block[tail]); // 读取内存中的数据
    memmove(to->data, from->data, BSIZE); // 将内存中的数据复制到日志区中的块
    bwrite(to);  // 将日志区中的块写入磁盘---这时候写入到了日志区
    brelse(from);//释放内存中的数据
    brelse(to);//释放日志区中的块
  }
}

static void
commit()//提交日志区
{
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log--将缓存中的块写入日志区
    write_head();    // 将内存log中的logheader写入日志区的头块--这里是提交点,将内存中的n写入磁盘日志区
    install_trans(0); // Now install writes to home locations--将日志区中的块写入对应的磁盘去
    log.lh.n = 0;//将日志区中记录的块数清零
    write_head();    // 将内存log中的logheader写入日志区的头块--清空日志区
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  acquire(&log.lock);
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n) {  // Add new block to log?
    bpin(b);//给这个块加一个引用计数,防止被提前回收
    log.lh.n++;
  }
  release(&log.lock);
}

