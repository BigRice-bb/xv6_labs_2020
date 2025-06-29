#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

//定义sys_sigalarm 和 sys_sigreturn
uint64
sys_sigalarm(void)
{
  //printf("sys_sigalarm\n");
  int interval;
  uint64 handler;
  if(argint(0, &interval) < 0)
    return -1;
  if(argaddr(1, &handler) < 0)
    return -1;
  myproc()->interval=interval;
  myproc()->handler=(void (*)(void))handler;
  myproc()->interval_count=0;
  return 0;
}
//定义sys_sigreturn
uint64
sys_sigreturn(void)
{
  //printf("sys_sigreturn\n");
  //报警处理函数执行完毕 恢复寄存器状态
  myproc()->alarm_on=0;//关闭报警
  //恢复寄存器状态,返回用户态会回到之前的epc继续执行
  memmove(myproc()->trapframe, myproc()->alarm_trapframe, sizeof(struct trapframe));//恢复寄存器状态
  //myproc()->alarm_trapframe=0;//清空
  //printf("报警关闭\n");
  return 0;
}

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  
  //在sys_sleep中调用backtrace
  backtrace();//打印回溯

  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
