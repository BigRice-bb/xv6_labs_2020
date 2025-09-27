#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"
//#include <cstdio>
//e1000 的初始化代码 和 发送和接收数据包的空函数 

#define TX_RING_SIZE 16
//环形缓冲区 大小16 用于存储待发送的数据包  字节对齐16 数据单元的起始地址必须为16的倍数
//tx_desc 结构体 用于描述待发送的数据包 
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];//包裹箱指针数组 用于存储待发送的数据包

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().//初始化pci总线
// xregs is the memory address at which the
// e1000's registers are mapped.//xregs是e1000寄存器映射的内存地址
void
e1000_init(uint32 *xregs)//该函数完成e1000卡的初始化配置
//xregs 为e1000寄存器映射的内存地址,通过该基址可直接访问e1000的寄存器
{
  int i;

  initlock(&e1000_lock, "e1000");//初始化e1000自旋锁

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts 禁用中断
  regs[E1000_CTL] |= E1000_CTL_RST;//复位控制寄存器
  regs[E1000_IMS] = 0; // redisable interrupts 重新禁用中断
  __sync_synchronize();//内存屏障 确保网卡复位后 才执行后面的代码

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));//清空发送环形缓冲区 
  // 这里的缓冲区存的是数据包信息 而不是直接的数据包

  //发送相关
  for (i = 0; i < TX_RING_SIZE; i++) {//清空发送缓冲区
    tx_ring[i].status = E1000_TXD_STAT_DD;//清空所有发货单 -- 发货单
    tx_mbufs[i] = 0;//清空所有具体货物   --具体货物   一一对应
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;//发送缓冲区基地址
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);//传输描述的长度 tx_ring 里面的个数
  regs[E1000_TDH] = regs[E1000_TDT] = 0; //头尾
  
  // [E1000 14.4] Receive initialization
  //接收相关
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);//分配一个包裹箱
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;//接收缓冲区基地址
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0; //头
  regs[E1000_RDT] = RX_RING_SIZE - 1; //尾
  regs[E1000_RDLEN] = sizeof(rx_ring); //接收缓冲区长度

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;//物理地址
  regs[E1000_RA+1] = 0x5634 | (1<<31);//物理地址
  //52 54 00 12 34 56  这是qemu的MAC地址 这里是小端对齐
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  //1 加锁 防止多个进程同时访问e1000
  acquire(&e1000_lock);
  //2 检查发送清单是否满了
  int i = 0;
  i=regs[E1000_TDT];//获取当前发送描述符的索引 代表可用描述符的索引
  if ((tx_ring[i].status & E1000_TXD_STAT_DD) == 0) {
    return -1; //上次传输尚未完成 返回1
  }// 否则代表可用
  //3 检查包裹箱是否为空
  if (tx_mbufs[i] != 0) {
    mbuffree(tx_mbufs[i]);
    tx_mbufs[i] = 0;
  }
  tx_mbufs[i] = m;
  //4 将m的信息写入描述符
  tx_ring[i].addr = (uint64) m->head;
  tx_ring[i].length = m->len;
  tx_ring[i].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  tx_ring[i].status = 0;
  regs[E1000_TDT] = (i + 1) % TX_RING_SIZE;
  //5 释放锁
  release(&e1000_lock);
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  
  return 0;
}

static void
e1000_recv(void)
{
  // 应该在一个循环里处理所有已接收的包
  while(1){
    // 1. 获取我们下一个要检查的描述符的索引
    int i = (regs[E1000_RDT] + 1) % RX_RING_SIZE;

    // 2. 检查这个描述符是否真的已经被硬件处理完毕
    if ((rx_ring[i].status & E1000_RXD_STAT_DD) == 0) {
      // 如果 DD 位为 0, 表示硬件还没处理到这里，没有新包了
      return; 
    }

    // 3. 更新 mbuf 的长度，这是硬件告诉我们的实际包长
    rx_mbufs[i]->len = rx_ring[i].length;

    // 4. 将收满数据的 mbuf 递交给上层协议栈
    // 注意：我们将 rx_mbufs[i] 这个指针本身传递出去，
    // 为了防止 net_rx 释放它后我们无法使用，我们先把它存到局部变量里
    struct mbuf *m = rx_mbufs[i];

    net_rx(m);
    
    // 5. "以旧换新": 申请一个新的空 mbuf 来补充到环形缓冲区中
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000_recv"); // 如果内存耗尽，系统崩溃
    
    // 6. 更新描述符，为下一次接收做准备
    rx_ring[i].addr = (uint64)rx_mbufs[i]->head;
    rx_ring[i].status = 0; // 清空状态，表示它现在是干净的

    // 7. 更新 RDT 指针，告诉硬件我们已经处理完一个，可以复用这个位置了
    regs[E1000_RDT] = i;


  }
}

void
e1000_intr(void)//中断接收
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
