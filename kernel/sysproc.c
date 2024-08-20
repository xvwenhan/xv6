#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 addr; // 起始虚拟地址
  int num;     // 页面数
  uint64 mask_addr; // 用于存储结果的用户缓冲区地址
  uint64 mask = 0;  // 临时缓冲区，用于存储结果

  // 解析参数
  if (argaddr(0, &addr) < 0 || argint(1, &num) < 0 || argaddr(2, &mask_addr) < 0)
    return -1;
  for (int i = 0; i < num; i++) {
    // 查找对应页表项
    pte_t *pte = walk(myproc()->pagetable, addr + i * PGSIZE, 0);
    if (pte == 0)
      return -1;
    // 检查访问位，并设置位掩码
    if (*pte & PTE_A) {
      mask |= (1L << i);
      // 清除访问位
      *pte &= ~PTE_A;//按位取反 按位与
    }
  }
  // 将结果复制到用户空间
  if (copyout(myproc()->pagetable, mask_addr, (char *)&mask, sizeof(mask)) < 0)
    return -1;

  return 0;
}
#endif

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


