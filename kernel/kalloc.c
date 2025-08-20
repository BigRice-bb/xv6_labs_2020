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
// 这里只有单一链表

struct run {
  struct run *next;
};

// 初始化kmem数组 每个cpu都有自己的内存链表和锁
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void kinit() {
  char kmem_name[6];
  for (int i = 0; i < NCPU; i++) {
    snprintf(kmem_name, sizeof(kmem_name), "kmem%d", i);
    initlock(&kmem[i].lock, kmem_name);
  }
  //第一个cpu获得所有内存
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  // 判断是否是合法的物理地址
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  // 填充垃圾数据 防止悬空引用
  memset(pa, 1, PGSIZE);
  // 将内存块转换为run结构体
  struct run *r;
  r = (struct run *)pa;
  // 关闭中断
  push_off();
  // 获取当前cpu的id
  int id = cpuid();
  // 获取当前cpu的内存池锁 防止并发访问
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  // 打开中断
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  //关闭中断
  push_off();
  //获取当前cpu的id
  int id = cpuid();
  //获取当前cpu的内存池锁 防止并发访问
  acquire(&kmem[id].lock);
  struct run *r;
  //获取内存链表头结点
  r = kmem[id].freelist;
  if (r)
  {
    kmem[id].freelist = r->next;
    release(&kmem[id].lock);
    memset(r, 5, PGSIZE); // fill with junk
    pop_off();
    return (void *)r;
  }
  else//当前cpu的内存链表为空
  {
    release(&kmem[id].lock);//释放原来的锁
    pop_off();//打开中断
    for (int i = 1; i < NCPU; i++) 
    {
      int steal_id = (id + i) % NCPU;
      acquire(&kmem[steal_id].lock);
      if (kmem[steal_id].freelist)
      {
        r = kmem[steal_id].freelist;    
        kmem[steal_id].freelist = r->next;
        release(&kmem[steal_id].lock);
        memset(r, 5, PGSIZE); // fill with junk
        //pop_off();
        return (void *)r;
      }
      release(&kmem[steal_id].lock);
    }
  }
  //pop_off();
  return 0;
}
