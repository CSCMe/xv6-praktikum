#include "defs.h"

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
sys_printPT(void)
{
  print_pt(myproc()->pagetable);
  return 0;
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
sys_futex_init(void)
{
  uint64* futex;
  argaddr(0, (uint64*)&futex);
  return __futex_init(futex);
}

uint64
sys_futex_wait(void)
{
  uint64* futex;
  int val;
  argaddr(0, (uint64*)&futex);
  argint(1, &val);
  return __futex_wait(futex, val);
}

uint64
sys_futex_wake(void)
{
  uint64* futex;
  int num_wake;
  argaddr(0, (uint64*)&futex);
  argint(1, &num_wake);
  return __futex_wake(futex, num_wake);
}