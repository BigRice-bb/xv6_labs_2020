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

// 物理页引用计数结构
struct {
  struct spinlock lock;//加锁  每次只能有一个进程改变物理页引用计数
  int refcount[PHYSTOP / PGSIZE];  // 每个物理页的引用计数
} kref;

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kref.lock, "kref");//初始化引用计数的信号量
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// 增加物理页引用计数
void
krefpage(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    return;
  
  acquire(&kref.lock);
  kref.refcount[(uint64)pa / PGSIZE]++;//物理页数组对应的物理页引用计数++
  release(&kref.lock);
}

// 减少物理页引用计数，如果计数为0则释放
void
kunrefpage(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    return;
  
  acquire(&kref.lock);
  int idx = (uint64)pa / PGSIZE;
  if(kref.refcount[idx] > 0) {
    kref.refcount[idx]--;
    if(kref.refcount[idx] == 0) {
      release(&kref.lock);
      kfree(pa);//若引用计数减为0 释放内存
      return;
    }
  }
  release(&kref.lock);
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
  r->next = kmem.freelist;//头插
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

//在分配内存时,将引用计数+1
  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // 初始化引用计数为1
    acquire(&kref.lock);
    kref.refcount[(uint64)r / PGSIZE] = 1;
    release(&kref.lock);
  }
  return (void*)r;
}
