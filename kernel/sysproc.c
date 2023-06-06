#include "defs.h"
#include "mmap.h"

uint64 sys_exit(void) {
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
}

uint64 sys_getpid(void) {
  return myproc()->pid;
}

uint64 sys_fork(void) {
  return fork();
}

uint64 sys_wait(void) {
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64 sys_sbrk(void) {
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_mmap(void) {
  uint64 addr;
  int len, prot, flags, fildes, off;
  argaddr(0, &addr);
  argint(1, &len);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fildes);
  argint(5, &off);

  if (len == 0 || fildes != -1 || off != 0 ||
      (flags | MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_POPULATE) !=
        (MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_POPULATE)) {
    printf("[K] sys_mmap: unsupported parameter(s)\n");
    return -1;
  }
  if (addr % PGSIZE != 0) {
    printf("[K] sys_mmap: addr not page aligned\n");
    return -1;
  }
  if (flags & MAP_FIXED && (addr < MMAP_BASE || addr > MAXVA)) {
    printf("[K] sys_mmap: MAP_FIXED: invalid address\n");
    return -1;
  }
  // printf("[K] sys_mmap: mapping length %d with flags %x and prot %x\n", len, flags, prot);

  uint64 npages  = PGROUNDUP(len) / PGSIZE;
  struct proc *p = myproc();

  uint64 start_va = addr > MMAP_BASE ? addr : MMAP_BASE;

  uint64 ret_addr = uvmmap(p->pagetable, start_va, npages, prot, flags);

  p->max_mmaped = (ret_addr + npages * PGSIZE > p->max_mmaped) ? ret_addr + npages * PGSIZE : p->max_mmaped;
  // printf("[K] sys_mmap: ret_addr=%p npages=%d\n", ret_addr, npages);
  return ret_addr;
}

uint64 sys_munmap(void) {
  uint64 addr;
  int len;
  argaddr(0, &addr);
  argint(1, &len);
  // printf("[K] sys_munmap: addr=%p len=%d\n", addr, len);
  if (addr % PGSIZE != 0 || len % PGSIZE != 0) { return -1; }
  uint64 npages = len / PGSIZE;
  // printf("[K] sys_munmap: len=%d npages=%d\n", len, npages);
  // TODO:
  struct proc *p = myproc();
  for (int i = 0; i < npages; i++) {
    if (walkaddr(p->pagetable, addr + i * PGSIZE) == 0) { return 0; }
  }
  // printf("[K] sys_munmap: unmapping addr=%p pages=%d\n", addr, npages);
  uvmunmap(myproc()->pagetable, addr, npages, 1);
  // printf("[K] sys_munmap: unmapped addr=%p npages=%d\n", addr, npages);
  return 0;
}
