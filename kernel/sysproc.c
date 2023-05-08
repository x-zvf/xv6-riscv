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
sys_mmap(void) {
  uint64 addr;
  int len, prot, flags, fildes, off;
  argaddr(0, &addr);
  argint(1, &len);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fildes);
  argint(5, &off);

  if(addr != 0 || fildes != -1 || off != 0 
  || flags != (MAP_ANONYMOUS | MAP_PRIVATE)
  || prot != (PROT_READ | PROT_WRITE)) {
    printf("[K] sys_mmap: unsupported parameter(s)\n");
    return -1;
  }
  printf("[K] sys_mmap: mapping length %d with flags %x and prot %x\n", len, flags, prot);

  uint64 npages = PGROUNDUP(len) / PGSIZE;
  struct proc *p = myproc();
  uint64 ret_addr = uvmmap(p->pagetable, npages, prot);
  printf("[K] sys_mmap: ret_addr=%p npages=\n", addr, npages);
  return ret_addr;
}

uint64
sys_munmap(void) {
  uint64 addr;
  int len;
  argaddr(0, &addr);
  argint(1, &len);
  printf("[K] sys_munmap: addr=%p len=%d\n", addr, len);
  if(addr % PGSIZE != 0 || len % PGSIZE != 0) {
    printf("[K] sys_munmap: addr or len not page aligned\n");
    return -1;
  }
  // TODO:
  struct proc *p = myproc();
  if(walkaddr(p->pagetable, addr) == 0) {
    printf("[K] sys_munmap: addr not mapped\n");
    return 0;
  }
  uvmunmap(myproc()->pagetable, addr, len, 1);
  return 0;
}
