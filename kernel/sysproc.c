#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
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

//这个函数纯粹是为了想要多一层调用的栈帧
void
cjr(void)
{
  printf("a");
  backtrace();
  printf("b");
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  backtrace();
  // cjr(); //calls backtrace()
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

uint64
sys_sigreturn(void)
{
  struct proc* proc = myproc();
  // re-store trapframe so that we can return and continue executing the interrupted code
  // 恢复为之前保存的、发生定时器中断时刻的trapframe。就好像定时器中断从未发生一样
  *proc->trapframe = proc->saved_trapframe;
  proc->can_be_called = 1;      // 定时器中断处理完成，可以再次进入中断处理函数
  return proc->saved_trapframe.a0;   // sys_sigreturn系统调用返回的时候，它会将其返回值存到 a0 寄存器中，那这样就改变了之前 a0 的值,破坏了定时器中断透明性，因此直接返回定时器终端发生时刻的a0,好像定时器中断从未发生一样
}
uint64
sys_sigalarm(void)
{
  int ticks;
  uint64 handler_va;
  //拿到两个传入的参数
  argint(0, &ticks);
  argaddr(1, &handler_va);
  struct proc* proc = myproc();
  proc->alarm_interval = ticks;   
  proc->handler_va = handler_va;  // 设置定时器中断处理函数的入口虚拟地址
  proc->can_be_called = 1;        // 定时器中断处理的入口地址刚被设置a，可以进入中断处理函数
  return 0;
}