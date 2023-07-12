#include "defs.h"
#include "mmap.h"
#include "proc.h"

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

/**
 * Helper function to sys_mmap and sys_munmap
 * Fills the given mapping according to the remaining parameters.
 */
static void _fill_mapping(struct mmap_mapping *mapping, const uint64 npages, const uint64 va, const uint8 is_shared) {
  mapping->is_valid  = 1;
  mapping->npages    = npages;
  mapping->va        = va;
  mapping->is_shared = is_shared;
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

  if (len <= 0 || off != 0) {
    printf("[K] sys_mmap: unsupported parameter(s): invalid length or offset\n");
    return -1;
  }
  if ((flags) & ~(MAP_ANONYMOUS | MAP_PRIVATE | MAP_SHARED | MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_POPULATE)) {
    printf("[K] sys_mmap: unsupported parameter(s): unsupported flags\n");
    return -1;
  }
  if ((flags & MAP_SHARED) ? fildes < 0 : 0) {
    printf("[K] sys_mmap: unsupported parameter(s): MAP_SHARED + invalid file descriptor\n");
    return -1;
  }
  if ((flags & MAP_SHARED) && (flags & MAP_PRIVATE)) {
    printf("[K] sys_mmap: unsupported parameter(s): MAP_SHARED + MAP_PRIVATE\n");
    return -1;
  }
  // printf("[K] sys_mmap: mapping length %d with flags %x and prot %x\n", len, flags, prot);

  uint64 npages    = PGROUNDUP(len) / PGSIZE;
  struct proc *p   = myproc();
  struct inode *in = 0;
  if (fildes >= 0) {
    struct file *f = p->ofile[fildes];

    if (f == 0 || !f->readable || f->type != FD_INODE) return -1;

    in = f->ip;
    ilock(in);
    if (in->size < len) {
      iunlockput(in);
      return -1;
    }
  }
  uint64 start_va = addr > MMAP_BASE ? addr : 
     (p->last_mmap_location > 0 && p->last_mmap_location >= MMAP_BASE && p->last_mmap_location < MAXVA ?
      p->last_mmap_location : MMAP_BASE);

  uint64 ret_addr = uvmmap(p->pagetable, p->mmap_mappings, start_va, npages, prot, flags, in);
  // printf("[K] sys_mmap: uvmmap ret addr %p\n", ret_addr);

  if (fildes >= 0) { iunlock(in); }
  if (p->mmap_mappings == 0) {
    p->mmap_mappings = kalloc();
    memset(p->mmap_mappings, 0, PGSIZE);
  }
  struct mmap_mapping_page *cpage = p->mmap_mappings;
  struct mmap_mapping_page *prev  = 0;

  p->last_mmap_location = ret_addr + len;

  // Look for an fitting place to put the mapping
  while (cpage != 0) {
    for (uint j = 0; j < MMAP_MAPPING_PAGE_N; j++) {
      if (!cpage->mappings[j].is_valid) {
        // printf("[K] sys_mmap: cpage=%p\n", cpage);
        _fill_mapping(&cpage->mappings[j], npages, ret_addr, fildes >= 0 ? 1 : 0);
        return ret_addr;
      }
    }
    prev  = cpage;
    cpage = cpage->next;
  }

  // Nothing found. Allocate new place and put mapping there.
  prev->next = kalloc();
  memset(prev->next, 0, PGSIZE);
  // printf("[K] sys_mmap: cpage=%p\n", cpage);
  _fill_mapping(&cpage->mappings[0], npages, ret_addr, fildes >= 0 ? 1 : 0);

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
  // TODO: Huh?
  struct proc *p = myproc();
  for (int i = 0; i < npages; i++) {
    if (walkaddr(p->pagetable, addr + i * PGSIZE) == 0) { return 0; }
  }

  int should_free                 = -1;
  struct mmap_mapping_page *cpage = p->mmap_mappings;
  //TODO: decrease ref count for shared mappings

  while (cpage != 0) {
    // printf("[K] sys_munmap: cpage=%p\n", cpage);
    for (uint32 i = 0; i < MMAP_MAPPING_PAGE_N; i++) {
      const uint8 is_shared = cpage->mappings[i].is_shared;

      // printf("[K] sys_munmap: i=%d is_valid=%d va=%p\n", i, cpage->mappings[i].is_valid, cpage->mappings[i].va);

      if (cpage->mappings[i].is_valid && cpage->mappings[i].va == addr) {
        should_free = is_shared ? 0 : 1;
        if (npages < cpage->mappings[i].npages) {
          // printf("[K] sys_munmap: partial unmap\n");
          cpage->mappings[i].npages -= npages;
          cpage->mappings[i].va += npages * PGSIZE;
          cpage = 0;
          break;
        }
        cpage->mappings[i].is_valid = 0;
        cpage                       = 0;
        // printf("[K] sys_munmap: found mapping\n");
        break;
      } else if (cpage->mappings[i].is_valid && cpage->mappings[i].va < addr &&
                 cpage->mappings[i].va + cpage->mappings[i].npages * PGSIZE > addr) {
        // printf("[K] sys_munmap: partial unmap\n");

        should_free = is_shared ? 0 : 1;

        const uint32 currently_mapped_pages = cpage->mappings[i].npages;
        const uint32 prev_mapped_pages      = (addr - cpage->mappings[i].va) / PGSIZE;
        const uint32 next_mapped_pages      = currently_mapped_pages - prev_mapped_pages - npages;

        cpage->mappings[i].npages = prev_mapped_pages;

        if (next_mapped_pages > 0) {
          struct mmap_mapping_page *page_for_new = p->mmap_mappings;
          struct mmap_mapping_page *prev         = p->mmap_mappings;
          int found                              = 0;

          const uint64 va = addr + prev_mapped_pages * PGSIZE;

          while (page_for_new != 0) {
            for (uint j = 0; j < MMAP_MAPPING_PAGE_N; j++) {
              if (!page_for_new->mappings[j].is_valid) {
                _fill_mapping(&page_for_new->mappings[j], npages, va, is_shared);
                found = 1;
                break;
              }
            }
            if (found) break;
            prev = page_for_new;
            page_for_new = page_for_new->next; // Hier cpage->next zu verwenden, macht irgendwie keinen Sinn, da das nur einmal zu einer Veränderung führt.
          }
          if (!found) {
            prev->next = kalloc();
            memset(prev->next, 0, PGSIZE);
            _fill_mapping(&page_for_new->mappings[i], npages, va, is_shared);
          }
        }
      }
    }
    if (cpage != 0) cpage = cpage->next;
  }

  if (should_free == -1) {
    printf("[K] sys_munmap: The address was never mapped\n");
    return -1;
  }

  // printf("[K] sys_munmap: unmapping addr=%p pages=%d\n", addr, npages);
  uvmunmap(myproc()->pagetable, addr, npages, should_free);
  // printf("[K] sys_munmap: unmapped addr=%p npages=%d\n", addr, npages);
  return 0;
}

uint64 sys_backtrace(void) {
  struct proc *p = myproc();
  proc_backtrace(p);
  return 0;
}