#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/* Possible states of a thread: */
#define FREE        0x0
#define RUNNING     0x1
#define RUNNABLE    0x2

#define STACK_SIZE  8192
#define MAX_THREAD  4

struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

struct thread {
  //线程栈需要存放线程的上下文，包括寄存器
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct context context;
};//线程结构体
struct thread all_thread[MAX_THREAD];//线程数组
struct thread *current_thread;//当前线程
extern void thread_switch(struct context*, struct context*);//切换线程
              
void 
thread_init(void)
{
  // main() is thread 0, which will make the first invocation to
  // thread_schedule().  it needs a stack so that the first thread_switch() can
  // save thread 0's state.  thread_schedule() won't run the main thread ever
  // again, because its state is set to RUNNING, and thread_schedule() selects
  // a RUNNABLE thread.
  current_thread = &all_thread[0];//初始化当前线程为线程0
  current_thread->state = RUNNING;
  //设置线程0的上下文
  uint64* stack_top = (uint64*)(current_thread->stack + STACK_SIZE);
  // 初始化主线程的上下文
  current_thread->context.ra = (uint64)0;  // ra = 0（表示主线程）
  current_thread->context.sp = (uint64)stack_top;  // sp = 栈顶
  current_thread->context.s0 = 0;  // s0-s11 = 0
  current_thread->context.s1 = 0;
  current_thread->context.s2 = 0;
  current_thread->context.s3 = 0;
  current_thread->context.s4 = 0;
  current_thread->context.s5 = 0;
  current_thread->context.s6 = 0;
  current_thread->context.s7 = 0;
  current_thread->context.s8 = 0;
  current_thread->context.s9 = 0;
  current_thread->context.s10 = 0;
  current_thread->context.s11 = 0;

}

void 
thread_schedule(void)
{
  struct thread *t, *next_thread;

  /* Find another runnable thread. */
  next_thread = 0;
  t = current_thread + 1;//从当前线程的下一个线程开始查找
  for(int i = 0; i < MAX_THREAD; i++){
    if(t >= all_thread + MAX_THREAD)
      t = all_thread;//如果t已经遍历到数组末尾，则从头开始遍历
    if(t->state == RUNNABLE) {
      next_thread = t;//找到下一个可运行的线程
      break;
    }
    t = t + 1;//循环查找下一个可运行的线程
  }

  /* 如果找不到下一个可运行的线程，则退出 */
  if (next_thread == 0) {
    printf("thread_schedule: no runnable threads\n");
    exit(-1);
  }

  //如果需要切换线程，则切换线程
  if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
    //thread_switch(a,b)  a是旧的上下文，b是新的上下文
    //将当前上下文保存在a中,将b中的上下文加载到当前上下文
    thread_switch(&t->context, &next_thread->context);
    next_thread = 0;
  } else
    next_thread = 0;
}

void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;//找到一个空闲的线程，并设置为可运行状态
  // YOUR CODE HERE
  //设置上下文,让线程在自己的栈上运行
  //设置栈指针指向栈顶
  uint64* stack_top = (uint64*)(t->stack + STACK_SIZE);
  //将线程上下文保存在栈顶预留空间
  t->context.ra = (uint64)func;
  t->context.sp = (uint64)stack_top;
  t->context.s0 = 0;
  t->context.s1 = 0;
  t->context.s2 = 0;
  t->context.s3 = 0;
  t->context.s4 = 0;
  t->context.s5 = 0;
  t->context.s6 = 0;
  t->context.s7 = 0;
  t->context.s8 = 0;
  t->context.s9 = 0;
  t->context.s10 = 0;
  t->context.s11 = 0;
  //返回主函数继续执行
}

//线程让出CPU，进入就绪状态
void 
thread_yield(void)
{
  current_thread->state = RUNNABLE;
  thread_schedule();
}

volatile int a_started, b_started, c_started;
volatile int a_n, b_n, c_n;

void 
thread_a(void)
{
  int i;
  printf("thread_a started\n");
  a_started = 1;
  while(b_started == 0 || c_started == 0)
    thread_yield();//等待，直到b和c线程都启动
  
  for (i = 0; i < 100; i++) {
    printf("thread_a %d\n", i);
    a_n += 1;
    thread_yield();//每次加一
  }
  printf("thread_a: exit after %d\n", a_n);

  current_thread->state = FREE;
  thread_schedule();
}

void 
thread_b(void)
{
  int i;
  printf("thread_b started\n");
  b_started = 1;
  while(a_started == 0 || c_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_b %d\n", i);
    b_n += 1;
    thread_yield();
  }
  printf("thread_b: exit after %d\n", b_n);

  current_thread->state = FREE;
  thread_schedule();
}

void 
thread_c(void)
{
  int i;
  printf("thread_c started\n");
  c_started = 1;
  while(a_started == 0 || b_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_c %d\n", i);
    c_n += 1;
    thread_yield();
  }
  printf("thread_c: exit after %d\n", c_n);

  current_thread->state = FREE;
  thread_schedule();
}

int 
main(int argc, char *argv[]) 
{
  a_started = b_started = c_started = 0;
  a_n = b_n = c_n = 0;
  thread_init();//初始化线程
  thread_create(thread_a);//初始化线程a,栈的分配
  thread_create(thread_b);//初始化线程b,栈的分配
  thread_create(thread_c);//初始化线程c,栈的分配
  thread_schedule();//开始调度线程
  exit(0);
}
