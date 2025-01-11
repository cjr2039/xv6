#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
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
  // lab pgtbl: your code here.
  struct proc *p;
  pagetable_t pagetable;
  p = myproc();
  pagetable = p->pagetable;

  // 拿到用户空间系统调用的传入参数（起始地址，检查数量，地址）
  uint64 va_start;//用户空间的起始虚拟地址
  int page_num;
  uint64 abits;//就是用户空间的abits虚拟地址
  argaddr(0,&va_start);
  argint(1,&page_num);
  argaddr(2,&abits);

  //检查当前页表所有叶子PTE的PTE_A位，并填充到Kernel_abits
  uint64 Kernel_abits = 0;
  for(int i = 0; i < page_num; i++){
    pte_t *pte = walk(pagetable, va_start + i*PGSIZE, 0);//由用户空间虚拟地址得到叶子节点PTE
    if (*pte & PTE_A)
    {
      Kernel_abits = Kernel_abits | (1L << i);
    }
    //所有的page_num个PTE_A置为0
    *pte = (*pte) & (~PTE_A);
  }

  //从内核空间复制到用户空间地址
  if(copyout(pagetable, abits, (char *)&Kernel_abits, sizeof(Kernel_abits)) < 0)
    panic("sys_pgacess copyout error");
  
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
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
