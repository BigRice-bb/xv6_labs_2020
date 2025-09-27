//
// simple PCI-Express initialization, only
// works for qemu and its e1000 card.
//

// 简单PCI-Express初始化,仅适用于qemu和其e1000卡。xv6引导时在PCI总线上搜索e1000卡的代码
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

void
pci_init()
{
  // we'll place the e1000 registers at this address.
  // vm.c maps this range.
  uint64 e1000_regs = 0x40000000L;//内核为e1000分配的寄存器映射的内存地址  
  // 这里只是通告,还未映射到物理地址 vm.c会将映射到虚拟页表中

  // qemu -machine virt puts PCIe config space here.
  // vm.c maps this range.
  uint32  *ecam = (uint32 *) 0x30000000L;//为PCI配置空间预留的地址 , 用于识别PCI设备
  
  // look at each possible PCI device on bus 0.
  for(int dev = 0; dev < 32; dev++){
    int bus = 0;
    int func = 0;
    int offset = 0;
    uint32 off = (bus << 16) | (dev << 11) | (func << 8) | (offset);
    volatile uint32 *base = ecam + off;//防止编译器优化 使用volatile
    uint32 id = base[0];//读取了配置空间的第一个32位字,这个字包含了VenderId 和 DeviceId
    
    // 100e:8086 is an e1000
    if(id == 0x100e8086){//找到e1000卡
      // command and status register.
      // bit 0 : I/O access enable
      // bit 1 : memory access enable
      // bit 2 : enable mastering
      base[1] = 7;//第二个32位字 对应配置空间中的命令寄存器 控制设备权限
      __sync_synchronize();//内存屏障 保证base[1]的写操作先于base[4+i]的写操作

      for(int i = 0; i < 6; i++){
        uint32 old = base[4+i];

        // writing all 1's to the BAR causes it to be
        // replaced with its size.
        base[4+i] = 0xffffffff;
        __sync_synchronize();

        base[4+i] = old;
      }

      // tell the e1000 to reveal its registers at
      // physical address 0x40000000.
      base[4+0] = e1000_regs;//将e1000的寄存器映射到物理地址0x40000000  基地址

      e1000_init((uint32*)e1000_regs);//初始化e1000卡
    }
  }
}
