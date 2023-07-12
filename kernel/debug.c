#include "kernel/defs.h"

uint64 read_user_va(uint64 va) {
  struct proc *p = myproc();
  uint64 page = PGROUNDDOWN(va);
  uint64 page_addr      = walkaddr(p->pagetable, va);
  if (page_addr == 0) {
    printf("[K] read_user_va: va=%p page_addr=%p\n", va, page_addr);
    return -1;
  }
  uint64 offset = va - page;
  uint64 phys_addr = page_addr + offset; 
  return *(uint64 *)phys_addr;
}

void proc_backtrace(struct proc *p) {
  uint64 pc    = p->trapframe->epc;
  uint64 ra    = p->trapframe->ra;
  uint64 fp    = p->trapframe->s0;

  printf("========== stacktrace (pid %d \"%s\") ==========\n", p->pid, p->name);
  printf("@pc = %p\n", pc);
  while(1) {
    printf("@ra = %p\t\tfp=%p\n", ra, fp); 
    if(fp == -1ULL) break;
    ra = read_user_va(fp - 8);
    fp = read_user_va(fp - 16);
  }
  printf("========== end stacktrace ==========\n");
}