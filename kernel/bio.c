// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define MAPBUF_HASH(dev,blockno) ((((dev)<<27)|(blockno))%NBUCKET)

//buffer cache 缓冲区缓存
struct {
  struct spinlock eviction_lock;//驱逐自旋锁
  struct buf buf[NBUF];//缓冲区数组 大小为NBUF=30

  //哈希桶
  struct buf mapbuf[NBUCKET];
  struct spinlock mapbuf_lock[NBUCKET];
} bcache;//锁争用严重

void
binit(void)
{
  //初始化桶的锁
  char lock_name[8];
  for(int i=0 ; i<NBUCKET ; i++)
  {
    snprintf(lock_name, sizeof(lock_name), "bcache%d", i);
    initlock(&bcache.mapbuf_lock[i], lock_name);
    bcache.mapbuf[i].next = 0;
  }
  //初始化缓存区块
  for(int i=0 ; i<NBUF ; i++)
  {
    initsleeplock(&bcache.buf[i].lock, "buffer");
    bcache.buf[i].lastuse = 0;
    bcache.buf[i].refcnt = 0;

    //将所有buf都添加到桶0中
    bcache.buf[i].next = bcache.mapbuf[0].next;
    bcache.mapbuf[0].next = &bcache.buf[i];
  }
  //初始化驱逐自旋锁
  initlock(&bcache.eviction_lock, "bcache_eviction");

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  //第一步在桶内查找是否有对应buf
  struct buf *b;
  uint key=MAPBUF_HASH(dev, blockno);//获取桶号
  acquire(&bcache.mapbuf_lock[key]);
  //第二步桶内寻找目标块
  for (b=bcache.mapbuf[key].next; b; b=b->next) 
  {
    //如果找到了
    if (b->dev==dev && b->blockno==blockno) 
    {
      b->refcnt++;
      release(&bcache.mapbuf_lock[key]);
      acquiresleep(&b->lock);//获取该块的锁
      return b;
    }
  }
  //如果没找到
  //先释放锁,再获取驱逐锁
  release(&bcache.mapbuf_lock[key]);
  acquire(&bcache.eviction_lock);//两个访问到同一个桶的进程需要互斥
  //再次桶内寻找一遍
  for (b=bcache.mapbuf[key].next; b; b=b->next) 
  {
    //如果找到了
    if (b->dev==dev && b->blockno==blockno) 
    {
      acquire(&bcache.mapbuf_lock[key]);
      b->refcnt++;
      release(&bcache.mapbuf_lock[key]);
      
      release(&bcache.eviction_lock);
      acquiresleep(&b->lock);//获取该块的锁
      return b;
    }
  }
  //如果任然没找到
  //记录最老的块
  struct buf *before_least=0;//LRU的前一个块
  uint holding_bucket = -1;//记录当前持有哪个桶锁

  //循环查询所有桶
  for (int i=0; i<NBUCKET; i++)
  {
    acquire(&bcache.mapbuf_lock[i]);
    int new_found=0;//记录是否找到新的块
    for (b=&bcache.mapbuf[i]; b->next; b=b->next)
    {
      //找到LRU的前一个
      if (b->next->refcnt==0 && (before_least==0 || b->next->lastuse<before_least->next->lastuse))
      {
        before_least=b;
        new_found=1;
      }
    }
    if (!new_found) {
      release(&bcache.mapbuf_lock[i]);
    }
    else {
      if (holding_bucket!=-1) {
        release(&bcache.mapbuf_lock[holding_bucket]);//释放上一个桶的锁继续找
      }
      holding_bucket=i;
    }
  }
  //如果没找到,代表没有空闲的缓存块了
  if (!before_least) {
    panic("bget: no buffers");
  }
  //如果找到了LRU
  b=before_least->next;
  if (holding_bucket!=key) {
    //如果该块不在key桶中,需要驱逐该块,加入到key桶中
    before_least->next=before_least->next->next;
    release(&bcache.mapbuf_lock[holding_bucket]);
    acquire(&bcache.mapbuf_lock[key]);
    b->next=bcache.mapbuf[key].next;
    bcache.mapbuf[key].next=b;
  }
  //设置新的块
  b->dev=dev;
  b->blockno=blockno;
  b->refcnt=1;
  b->valid=0;
  release(&bcache.mapbuf_lock[key]);
  
  release(&bcache.eviction_lock);
  acquiresleep(&b->lock);
  return b;

}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)//释放buf缓冲块
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  //获取桶号
  uint key=MAPBUF_HASH(b->dev, b->blockno);
  acquire(&bcache.mapbuf_lock[key]);
  b->refcnt--;
  if (b->refcnt==0) {
    b->lastuse=ticks;
  }
  release(&bcache.mapbuf_lock[key]);

}

void
bpin(struct buf *b) {
  uint key=MAPBUF_HASH(b->dev, b->blockno);
  acquire(&bcache.mapbuf_lock[key]);
  b->refcnt++;
  release(&bcache.mapbuf_lock[key]);
}

void
bunpin(struct buf *b) {
  uint key=MAPBUF_HASH(b->dev, b->blockno);
  acquire(&bcache.mapbuf_lock[key]);
  b->refcnt--;
  release(&bcache.mapbuf_lock[key]);
}


