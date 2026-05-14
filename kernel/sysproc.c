#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "pinfo.h"

extern struct proc proc[NPROC];
extern struct spinlock wait_lock;

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
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
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
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
  return kkill(pid);
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
sys_getprocs(void)
{
  uint64 buf;
  int max_count;
  struct proc *p;
  struct pinfo pi;
  int count = 0;

  argaddr(0, &buf);
  argint(1, &max_count);

  if (max_count <= 0)
    return -1;

  acquire(&wait_lock);
  for (p = proc; p < &proc[NPROC] && count < max_count; p++) {
    acquire(&p->lock);
    if (p->state == UNUSED) {
      release(&p->lock);
      continue;
    }
    pi.pid = p->pid;
    pi.state = p->state;
    safestrcpy(pi.name, p->name, sizeof(pi.name));
    pi.ppid = (p->parent != 0) ? p->parent->pid : -1;
    release(&p->lock);

    if (copyout(myproc()->pagetable, buf + count * sizeof(pi), (char *)&pi, sizeof(pi)) < 0) {
      release(&wait_lock);
      return -1;
    }
    count++;
  }
  release(&wait_lock);
  return count;
}
