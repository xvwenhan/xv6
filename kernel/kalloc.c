// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
//新增加：
#define MAX_PAGES (PHYSTOP / PGSIZE)
static int page_ref[MAX_PAGES];//记录引用次数的数组
struct spinlock page_ref_lock;//锁

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
  initlock(&kmem.lock, "kmem");
  initlock(&page_ref_lock, "ref_cnt");//新增加
  memset(page_ref, 0, sizeof(page_ref));//新增加
  freerange(end, (void*)PHYSTOP);
}

// 增加引用计数
void
incref(uint64 pa)
{
  int index = (pa / PGSIZE);
  acquire(&page_ref_lock);
  page_ref[index]++;
  release(&page_ref_lock);
}

// 减少引用计数
void
decref(uint64 pa)
{
  int index = (pa / PGSIZE);
  acquire(&page_ref_lock);//获取锁,保证对 page_ref 数组的访问是原子的
  --page_ref[index];
  release(&page_ref_lock);
}
//查看引用次数
int
chcref(uint64 pa){
  acquire(&page_ref_lock);
  int index = (pa / PGSIZE);
  release(&page_ref_lock);
  return page_ref[index];
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP){
    panic("kfree");}
  //新增加
  if(chcref((uint64)pa) > 1){
    decref((uint64)pa);
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
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
  if(r){
    kmem.freelist = r->next;
    int index = ((uint64)r / PGSIZE);
    acquire(&page_ref_lock);
    page_ref[index]=1;//初始化为1
    release(&page_ref_lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
