#include "kernel/defs.h"
#include "kernel/colors.h"

uint64 read_user_va(uint64 va) {
  struct proc *p   = myproc();
  uint64 page      = PGROUNDDOWN(va);
  uint64 page_addr = walkaddr(p->pagetable, va);
  if (page_addr == 0) { return -2; }
  uint64 offset    = va - page;
  uint64 phys_addr = page_addr + offset;
  return *(uint64 *)phys_addr;
}

void proc_backtrace(struct proc *p) {
  uint64 pc = p->trapframe->epc;
  uint64 ra = p->trapframe->ra;
  uint64 fp = p->trapframe->s0;

  printf(C_RESET "\n" C_BG_BLACK C_FG_BLUE "====================" C_FG_YELLOW
                 " stacktrace (pid " C_FG_RED "%d %s" C_FG_YELLOW ") " C_FG_BLUE
                 "====================" C_RESET "\n",
    p->pid, p->name);
  printf(C_RESET C_FG_GREEN "[@] " C_FG_WHITE "pc = " C_FG_BLUE "%p\n", pc);
  int frame = 1;
  while (1) {
    printf(C_RESET C_FG_GREEN "[%d] " C_FG_WHITE "ra = " C_FG_BLUE "%p" C_FG_WHITE
                              "\t\tfp = " C_FG_YELLOW "%p\n",
      frame++, ra, fp);
    if (fp == -1ULL) break;
    ra = read_user_va(fp - 8);
    fp = read_user_va(fp - 16);
    if (ra == -2 || fp == -2) {
      printf("corrupt stack: last stack frame is not mapped\n");
      break;
    }
  }
  printf(
    C_RESET C_BG_BLACK C_FG_BLUE "====================" C_FG_YELLOW " end stacktrace " C_FG_BLUE
                                 "===================" C_RESET "\n");
}