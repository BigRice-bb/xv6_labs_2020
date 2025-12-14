// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");//初始化空闲列表
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);//按页对齐  4k为单位
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);//每一页调用kfree函数将其加入空闲链表
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;//头插,将r插入空链表头部
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
//内核物理内存分配核心函数
//从空闲的物理中分配一个4K的物理页   返回物理页的起始地址
void *
kalloc(void)//取出空闲链表的第一个元素
{
  struct run *r;

  acquire(&kmem.lock);//获取自旋锁  
  r = kmem.freelist;//全局空闲链表
  if(r)//若r存在
    kmem.freelist = r->next; //将空闲链表头指针指向r的下一个节点  -- 代表当前页被使用 不再空闲
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk  将该页所有的数据都赋值为5 -- 代表该页被使用
  return (void*)r;//将该页的起始地址返回给调用者 分配成功
}
