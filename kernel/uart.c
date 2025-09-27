//
// low-level driver routines for 16550a UART.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// the UART control registers are memory-mapped
// at address UART0. this macro returns the
// address of one of the registers.
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))

// the UART control registers.
// some have different meanings for
// read vs write.
// see http://byterunner.com/16550.html
#define RHR 0                 // receive holding register (for input bytes)输入寄存器
#define THR 0                 // transmit holding register (for output bytes)输出寄存器
#define IER 1                 // interrupt enable register 中断使能
#define IER_RX_ENABLE (1<<0) //分别控制   发送/接收   中断使能
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFO control register   FIFO队列控制
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
#define ISR 2                 // interrupt status register  中断状态寄存器
#define LCR 3                 // line control register  线路控制寄存器,控制通信参数
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
#define LSR 5                 // line status register  线路状态寄存器,查看各寄存器状态
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define LSR_TX_IDLE (1<<5)    // THR can accept another character to send

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// the transmit output buffer.
struct spinlock uart_tx_lock; //发送锁
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];//发送缓冲区  发送环形缓冲区
uint64 uart_tx_w; // write next to uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] 写入位置
uint64 uart_tx_r; // read next from uart_tx_buf[uar_tx_r % UART_TX_BUF_SIZE] 读取位置

extern volatile int panicked; // from printf.c

void uartstart();

void
uartinit(void)//通过写寄存器配置串口
{
  // disable interrupts.
  WriteReg(IER, 0x00);//关闭串口中断

  // special mode to set baud rate.
  WriteReg(LCR, LCR_BAUD_LATCH);//设置波特率

  // LSB for baud rate of 38.4K.
  WriteReg(0, 0x03);//设置波特率低8位

  // MSB for baud rate of 38.4K.
  WriteReg(1, 0x00);//设置波特率高8位

  // leave set-baud mode,
  // and set word length to 8 bits, no parity.
  WriteReg(LCR, LCR_EIGHT_BITS);//设置数据位为8位

  // reset and enable FIFOs.
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);//开启FIFO队列

  // enable transmit and receive interrupts.
  WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);//开启发送/接收中断

  initlock(&uart_tx_lock, "uart");//初始化发送锁
}

// add a character to the output buffer and tell the
// UART to start sending if it isn't already.
// blocks if the output buffer is full.
// because it may block, it can't be called
// from interrupts; it's only suitable for use
// by write().
void
uartputc(int c)//将字符c写入到发送缓冲区 并调用uartstart()发送一个字符
{
  acquire(&uart_tx_lock);//获取发送锁

  if(panicked){
    for(;;)
      ;
  }

  while(1){
    if(uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE){//写位置==读位置+缓冲区大小 代表缓冲区满
      // buffer is full.
      // wait for uartstart() to open up space in the buffer.
      sleep(&uart_tx_r, &uart_tx_lock);//等待发送缓冲区有空间
    } else {
      uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;//将字符c写入发送缓冲区
      uart_tx_w += 1;//写位置+1
      uartstart();//通知UART开始发送
      release(&uart_tx_lock);//释放发送锁
      return;
    }
  }
}

// alternate version of uartputc() that doesn't 
// use interrupts, for use by kernel printf() and
// to echo characters. it spins waiting for the uart's
// output register to be empty.
void
uartputc_sync(int c)
{
  push_off();

  if(panicked){
    for(;;)
      ;
  }

  // wait for Transmit Holding Empty to be set in LSR.
  while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);

  pop_off();
}

// if the UART is idle, and a character is waiting
// in the transmit buffer, send it.
// caller must hold uart_tx_lock.
// called from both the top- and bottom-half.
void
uartstart()//发送缓冲区有字符时,发送一个字符
{
  while(1){
    if(uart_tx_w == uart_tx_r){//写位置==读位置 代表缓冲区空
      // transmit buffer is empty.
      return;
    }
    
    if((ReadReg(LSR) & LSR_TX_IDLE) == 0){//发送寄存器空
      // the UART transmit holding register is full,
      // so we cannot give it another byte.
      // it will interrupt when it's ready for a new byte.
      return;
    }
    
    int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];//从发送缓冲区中读取一个字符
    uart_tx_r += 1;
    
    // maybe uartputc() is waiting for space in the buffer.
    wakeup(&uart_tx_r);
    
    WriteReg(THR, c);//将字符c发送到UART的输出寄存器  
  }
}

// read one input character from the UART.
// return -1 if none is waiting.
int
uartgetc(void)
{
  if(ReadReg(LSR) & 0x01){
    // input data is ready.
    return ReadReg(RHR);
  } else {
    return -1;
  }
}

// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from trap.c.
void
uartintr(void)
{
  // read and process incoming characters.
  while(1){
    int c = uartgetc();
    if(c == -1)
      break;
    consoleintr(c);
  }

  // send buffered characters.
  acquire(&uart_tx_lock);
  uartstart();
  release(&uart_tx_lock);
}
