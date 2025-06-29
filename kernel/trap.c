#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"


struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt-中断, exception-异常, or system call-系统调用 from user space.
// called from trampoline.S  来自 trampoline.S 文件  用户态陷入内核态执行陷阱处理函数
//
void usertrap(void)
{
  int which_dev = 0;

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // save user program counter.
  p->trapframe->epc = r_sepc();

  if (r_scause() == 8)
  {
    // system call  !!!!!!!!

    if (p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall(); // 执行系统调用函数
  }
  else if ((which_dev = devintr()) != 0)
  { // 处理外部设备中断  包括时钟中断
    // ok
  }
  else
  {
    // 其他异常情况  打印错误信息,终止异常
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if (p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
  {
    //如果报警没有开启,并且报警间隔大于0,则开启报警
    if (p->alarm_on == 0 && p->interval > 0)
    {
      // 添加alarm定时中断代码
      p->interval_count++;
      if (p->interval_count >= p->interval)
      {
        //printf("报警开启\n");
        p->alarm_on = 1;
        // 说明我当前正在执行报警处理函数,就不会再次进入报警处理函数
        memmove(p->alarm_trapframe, p->trapframe, sizeof(struct trapframe));//保存当前的寄存器状态
        //跳转到报警处理函数
        p->trapframe->epc = (uint64)(p->handler);
        //重置报警间隔计数器
        p->interval_count = 0;
      }
    }
    yield();
  }
  usertrapret();//返回用户态,实际是调到了报警处理函数
}

//
// return to user space
//
void usertrapret(void)
{
  struct proc *p = myproc();

  // 1. 关中断
  intr_off();

  // 2. 重新设置陷阱向量
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // 3. 为下一次陷阱准备好内核信息
  p->trapframe->kernel_satp = r_satp();
  p->trapframe->kernel_sp = p->kstack + PGSIZE;
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();

  // 4. 设置返回用户模式所需的状态  -- 设置sstatus寄存器
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // 清除SPP位，表示下一特权级是User
  x |= SSTATUS_SPIE; // 设置SPIE位，允许用户态响应中断
  w_sstatus(x);

  // 5. 设置返回地址
  w_sepc(p->trapframe->epc);

  // 6. 准备用户页表
  uint64 satp = MAKE_SATP(p->pagetable);

  // 7.通过函数指针 跳转到trampoline.S中的userret  -- 跳转到用户态
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if ((which_dev = devintr()) == 0)
  {
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void clockintr() // 时钟中断处理函数
{
  acquire(&tickslock); // 获取tickslock锁
  ticks++;             // ticks加1
  wakeup(&ticks);      // 唤醒ticks
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr()
{
  uint64 scause = r_scause();

  if ((scause & 0x8000000000000000L) &&
      (scause & 0xff) == 9)
  {
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if (irq == UART0_IRQ)
    {
      uartintr();
    }
    else if (irq == VIRTIO0_IRQ)
    {
      virtio_disk_intr();
    }
    else if (irq)
    {
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if (irq)
      plic_complete(irq);

    return 1;
  }
  else if (scause == 0x8000000000000001L)
  { // 时钟中断
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if (cpuid() == 0)
    {
      clockintr(); // 时钟中断处理函数
    }

    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2); // 清除软件中断标志

    return 2;
  }
  else
  {
    return 0;
  }
}
