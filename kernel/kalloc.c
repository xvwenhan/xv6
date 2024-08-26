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
} kmem[NCPU];


char *kmem_lock_names[NCPU] = {
    "kmem_0_lock",
    "kmem_1_lock",
    "kmem_2_lock",
    "kmem_3_lock",
    "kmem_4_lock",
    "kmem_5_lock",
    "kmem_6_lock",
    "kmem_7_lock",
};


void
kinit()
{
  for(int i=0;i<NCPU;i++) { 
    initlock(&kmem[i].lock, kmem_lock_names[i]);
  }
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

  push_off(); //调用cpuid()必须关中断
  int cpu_index = cpuid();
  pop_off();//开中断
  r = (struct run*)pa;

  acquire(&kmem[cpu_index].lock);
  r->next = kmem[cpu_index].freelist;//在空闲链表头部插入pa
  kmem[cpu_index].freelist = r;//空闲链表头指针指向pa
  release(&kmem[cpu_index].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off(); //调用cpuid()必须关中断
  int cpu_index = cpuid();
  pop_off();//开中断
  acquire(&kmem[cpu_index].lock);
  r = kmem[cpu_index].freelist;//从全局变量kmem.freelist中取出第一个空闲内存块的指针
  if(r){;
  }
  else{//需要去偷别的CPU的空闲列表
    int steal_amount=0;
    for(int i=0;i<NCPU;i++) {
      if(i == cpu_index) continue; 
      acquire(&kmem[i].lock);
      struct run *target_r = kmem[i].freelist;
        for(;steal_amount<1024&&target_r;steal_amount++){
        kmem[i].freelist = target_r->next;//目标CPU失去一个，头指针后移一位
        target_r->next = kmem[cpu_index].freelist;//尾指针连接上链头
        kmem[cpu_index].freelist = target_r;//链头更新
        target_r = kmem[i].freelist;//target_r更新到目标CPU的下一个
      }
      release(&kmem[i].lock);
      if(steal_amount==1024){
        break;
      }
    }
    r = kmem[cpu_index].freelist;
  }
  if(r){
    kmem[cpu_index].freelist = r->next;//不要忘记
    memset((char*)r, 5, PGSIZE); //将分配的内存块填充随机的“垃圾”数据
  }
  release(&kmem[cpu_index].lock);
  return (void*)r;//返回分配的内存块指针
}
