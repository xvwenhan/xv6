#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"
uint64 freemem(void);  // 声明 freemem 函数
int nproc(void);       // 声明 nproc 函数
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
//下面都是新加入的
uint64
sys_trace(void)
{
  int mask;//使用mask接受整型参数
  struct proc *p = myproc();
  if(argint(0, &mask) < 0)//获取从用户空间获取传递给系统调用的整型参数
    return -1;
  p->trace_mask = mask;//这里trace_mask是新添加的一个成员
  return 0;
}

// 实现 sys_sysinfo 函数
uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  struct proc *p = myproc();//定义一个指向proc结构体的指针p，用于遍历进程数组
  uint64 addr;

  // 获取用户传入的地址参数
  if(argaddr(0, &addr) < 0)
    return -1;

  // 获取系统信息
  info.freemem = freemem();
  info.nproc = nproc();

  // 将 sysinfo 结构体从内核空间复制到用户空间
  if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
  //copyout 函数用于将数据从内核空间复制到用户空间
  //int copyout(pagetable_t pagetable, uint64 addr, char *src, uint64 len);
  //pagetable：用户进程的页表，用于确定用户地址如何映射到物理内存。
  //addr：用户空间中的地址，数据将被写入到这个地址。
  //src：指向内核空间中待复制数据的指针。
  //len：要复制的数据长度。
    return -1;

  return 0;
}