#include "user/bmalloc.h"
#include "user/user.h"
#include "user/mmap.h"

extern "C" {

block block_alloc(uint32_t size, uint32_t align) __attribute__((weak));

void block_free(block block) __attribute__((weak));
void setup_balloc(void) __attribute__((weak));
void setup_malloc() __attribute__((weak));

extern int bmalloc_enable_printing;
}

static struct malloc_metadata metadata;

static uint32_t clz(uint32_t x) { //__builtin_clz does not link, grrrr.
  /* We can trust, that this is never called with zero
  if(x == 0)
    return 32; */
  uint32_t res = 0;
  while ((1ULL << (31 - res)) > (uint64_t)x) res++;
  return res;
}

static uint32_t ord_of_next_power_of_two(uint32_t x) {
  uint32_t ord_of_highest_set_bit = 32 - clz(x);
  if (x == (1U << (ord_of_highest_set_bit - 1)))
    return ord_of_highest_set_bit - 1;
  else
    return ord_of_highest_set_bit;
}
enum MappingType { IS_BUDDY, IS_MMAP };

static struct metadata_page *allocate_metadata_page() {
  uint64_t nbytes = PGSIZE;
  char *new_break = sbrk(nbytes);
  if (bmalloc_enable_printing) {
    printf("allocated %d bytes of metadata at %p\n", nbytes, new_break);
  }
  metadata.end_of_sbrk_segment      = new_break + nbytes;
  struct metadata_page *new_segment = (struct metadata_page *)new_break;
  new_segment->next                 = 0;
  new_segment->min_address_mapped   = (void *)-1ULL;
  new_segment->max_address_mapped   = 0;
  return new_segment;
}

static void *free_list_pop(int order) {
  if (bmalloc_enable_printing)
    printf("popping from freelist order=%d, index=%d, nfreelists=%d\n", order, FREELIST_INDEX(order), NUM_FREE_LISTS);
  void *ret = metadata.buddy_free_lists[FREELIST_INDEX(order)];
  if (bmalloc_enable_printing) printf(" =>popped: ret is %x \n", ret);
  if (bmalloc_enable_printing) printf(" =>end_of_sbrk=%x\n", metadata.end_of_sbrk_segment);

  if (ret != 0) { metadata.buddy_free_lists[FREELIST_INDEX(order)] = *(void **)ret; }
  return ret;
}

static void free_list_push(int order, void *blockptr) {
  *(void **)blockptr = metadata.buddy_free_lists[FREELIST_INDEX(order)];
  metadata.buddy_free_lists[FREELIST_INDEX(order)] = blockptr;
}

static int is_block_marked(struct buddy_page_metadata *pm, void *blockptr, uint32_t order) {
  uint32_t nth_of_row = (uint64_t)PAGE_OFFSET(blockptr) >> order; // 511
  uint32_t row        = (BUDDY_MAX_ORDER - order);                // 9
  uint32_t bit        = (1 << row) - 1 + nth_of_row;
  if (bmalloc_enable_printing) {
    printf("block %p (order %d) is %dth in row %d, bit %d\n", blockptr, order, nth_of_row, row, bit);
    if (bit > 64 * 16 - 1) {
      printf("FATAL: bit %d is out of range. ptr=%p, order=%d\n", bit, blockptr, order);
      exit(-1);
    }
    printf("checking if block %p (order %d) is in use => bit %d is %s\n", blockptr, order, bit,
      (pm->in_use_bitset[bit / 64] & (1ULL << (bit % 64))) != 0 ? "set" : "not set");
  }
  return (pm->in_use_bitset[bit / 64] & (1ULL << (bit % 64))) != 0;
}

static void mark_block(struct buddy_page_metadata *pm, void *blockptr, uint32_t order, char in_use) {
  uint32_t nth_of_row = (uint64_t)PAGE_OFFSET(blockptr) >> order;
  uint32_t row        = (BUDDY_MAX_ORDER - order);
  uint32_t bit        = (1 << row) - 1 + nth_of_row;
  if (bmalloc_enable_printing) {
    printf("marking block %p (order %d) as %s => bit %d set\n", blockptr, order, in_use ? "in use" : "free", bit);
    printf("  => idx = %d, bit = %d\n", bit / 64, bit % 64);
  }
  if (in_use)
    pm->in_use_bitset[bit / 64] |= (1ULL << (bit % 64));
  else
    pm->in_use_bitset[bit / 64] &= ~(1ULL << (bit % 64));
}

static inline int is_buddy_mapping_valid(struct buddy_page_metadata *mapping) {
  return mapping->in_use_bitset[15] & (1ULL << 63) ? 1 : 0;
}

static inline void make_buddy_mapping_valid(struct buddy_page_metadata *mapping) {
  mapping->in_use_bitset[15] |= (1ULL << 63);
}


static struct metadata_page *find_buddy_page_metadata(uint32 *idx, void *blockptr) {
  void *page_base            = PAGE_BASE(blockptr);
  struct metadata_page *page = metadata.first_buddy_metadata_page;
  while (page != 0) {
    if (page_base < page->min_address_mapped || page_base > page->max_address_mapped) {
      if (bmalloc_enable_printing)
        printf("ptr %p is not in range %p-%p\n", blockptr, page->min_address_mapped, page->max_address_mapped);
      page = page->next;
      continue;
    }
    for (uint32_t i = 0; i < NUM_BUDDY_MAPPINGS_PER_METADATA_PAGE; i++) {
      if (bmalloc_enable_printing)
        printf("checking mapping %d: %p (valid= %d)\n", i, page->buddy_mappings[i].address,
          is_buddy_mapping_valid(&page->buddy_mappings[i]));
      if (page->buddy_mappings[i].address == page_base && is_buddy_mapping_valid(&page->buddy_mappings[i])) {
        *idx = i;
        return page;
      }
    }
    page = page->next;
  }
  if (bmalloc_enable_printing) {
    printf("FATAL: could not find page metadata for block %p\n", blockptr);
    exit(-1);
  }
  return 0;
}

static struct metadata_page *find_mmap_page_metadata(uint32 *idx, void *ptr) {
  struct metadata_page *page = metadata.first_mmap_metadata_page;
  while (page != 0) {
    if (ptr < page->min_address_mapped || ptr > page->max_address_mapped) {
      if (bmalloc_enable_printing)
        printf("ptr %p is not in range %p-%p\n", ptr, page->min_address_mapped, page->max_address_mapped);
      page = page->next;
      continue;
    }
    for (uint32_t i = 0; i < NUM_MMAP_MAPPINGS_PER_METADATA_PAGE; i++) {
      struct mmap_page_metadata *mapping = &page->mmap_mappings[i];
      if (bmalloc_enable_printing)
        printf("checking mapping %d: %p valid=%d\n", i, mapping->address, mapping->is_valid);

      if (mapping->is_valid && mapping->address == ptr) {
        *idx = i;
        return page;
      }
    }
    page = page->next;
  }
  if (bmalloc_enable_printing) {
    printf("FATAL: could not find page metadata for %p\n", ptr);
    exit(-1);
  }
  *idx = -1U;
  return 0;
}

static void *split_and_allocate_in_block(void *blockptr, uint64_t block_order_size, uint32_t order) {
  uint32 idx                          = -1U;
  struct metadata_page *page_metadata = find_buddy_page_metadata(&idx, blockptr);
  mark_block(&page_metadata->buddy_mappings[idx], blockptr, order, 1);

  // split the block into two halves, allocating in the left half and pushing the right half onto the free list
  for (uint32_t i = block_order_size - 1; i >= order; i--) {
    char *right_half = ((char *)blockptr) + (1 << i);
    if (bmalloc_enable_printing) {
      printf("splitting block %p (order %d) into left %p (order %d) and right %p (order %d)\n",
        blockptr, i, blockptr, i - 1, right_half, i - 1);
    }
    free_list_push(i, right_half);
  }
  if (bmalloc_enable_printing) {
    printf("split_and_allocate_in_block: allocated block %p (order %d)\n", blockptr, order);
  }
  return blockptr;
}

static struct metadata_page *get_or_allocate_metadata_page(uint32 *idx, MappingType type) {
  if (type == IS_BUDDY) {
    if (metadata.num_buddy_metadata_free > 0) {
      struct metadata_page *last_mp  = metadata.last_buddy_metadata_page;
      struct metadata_page *first_mp = metadata.first_buddy_metadata_page;
      struct metadata_page *mp       = last_mp->num_mappings_free > 0 ? last_mp : first_mp;
      while (mp) {
        for (uint32 i = 0; i < NUM_BUDDY_MAPPINGS_PER_METADATA_PAGE; i++) {
          if (!is_buddy_mapping_valid(&mp->buddy_mappings[i])) {
            *idx = i;
            return mp;
          }
        }
        mp = mp->next;
      }
    }
  } else {
    if (metadata.num_mmap_metadata_free > 0) {
      struct metadata_page *last_mp  = metadata.last_mmap_metadata_page;
      struct metadata_page *first_mp = metadata.first_mmap_metadata_page;
      struct metadata_page *mp       = last_mp->num_mappings_free > 0 ? last_mp : first_mp;
      while (mp) {
        for (uint32 i = 0; i < NUM_MMAP_MAPPINGS_PER_METADATA_PAGE; i++) {
          if (!mp->mmap_mappings[i].is_valid) {
            *idx = i;
            return mp;
          }
        }
        mp = mp->next;
      }
    }
  }
  if (bmalloc_enable_printing) printf("allocating new metadata page\n");
  // allocate a new metadata page
  struct metadata_page *page = allocate_metadata_page();
  if (bmalloc_enable_printing) printf("allocated new metadata page at page = %p\n", page);
  if (page == 0) return 0;

  if (type == IS_BUDDY) {
    metadata.num_buddy_metadata_free += NUM_BUDDY_MAPPINGS_PER_METADATA_PAGE;
    if (metadata.first_buddy_metadata_page == 0) metadata.first_buddy_metadata_page = page;
    if (metadata.last_buddy_metadata_page == 0) {
      metadata.last_buddy_metadata_page = page;
    } else {
      metadata.last_buddy_metadata_page->next = page;
      metadata.last_buddy_metadata_page       = page;
    }
  } else {
    metadata.num_mmap_metadata_free += NUM_MMAP_MAPPINGS_PER_METADATA_PAGE;
    if (metadata.first_mmap_metadata_page == 0) metadata.first_mmap_metadata_page = page;
    if (metadata.last_mmap_metadata_page == 0) {
      metadata.last_mmap_metadata_page = page;
    } else {
      metadata.last_mmap_metadata_page->next = page;
      metadata.last_mmap_metadata_page       = page;
    }
  }
  *idx = 0;
  return page;
}

static void *allocate_top_level_block() {
  // allocate Metadata

  uint32 idx               = -1U;
  struct metadata_page *mp = get_or_allocate_metadata_page(&idx, IS_BUDDY);

  uint64 bytes_to_allocate = 1ULL << BUDDY_MAX_ORDER;

  if (bmalloc_enable_printing) {
    if (bytes_to_allocate != PGSIZE) {
      printf("FATAL: top level block size is not a page size. Peter can't do maths\n");
      exit(-1);
    }
  }

  char *new_break = sbrk(bytes_to_allocate);
  if (new_break == (char *)-1) {
    if (bmalloc_enable_printing) { printf("FATAL: sbrk failed\n"); }
    return 0;
  }

  if (bmalloc_enable_printing) {
    printf("allocated top level block at %p (allocated %d bytes) \n", new_break, bytes_to_allocate);
  }


  mp->buddy_mappings[idx].address = new_break;

  if (metadata.end_of_sbrk_segment != new_break) {
    printf("FATAL: metadata.end_of_sbrk_segment != new_break\n");
    exit(-1);
    term();
  }

  metadata.end_of_sbrk_segment = new_break + bytes_to_allocate;

  mp->num_mappings_free--;
  metadata.num_buddy_metadata_free--;

  for (int i = 0; i < 16; i++) mp->buddy_mappings[idx].in_use_bitset[i] = 0;

  make_buddy_mapping_valid(&mp->buddy_mappings[idx]);
  if (bmalloc_enable_printing) {
    printf("New mapping is valid = %d\n", is_buddy_mapping_valid(&mp->buddy_mappings[idx]));
  }

  if (mp->max_address_mapped < new_break + bytes_to_allocate)
    mp->max_address_mapped = metadata.end_of_sbrk_segment;

  if (mp->min_address_mapped > new_break) mp->min_address_mapped = new_break;

  return new_break;
}

void *_buddy_malloc_split_or_allocate(uint32_t order, uint32_t start_search_order) {
  void *ret;

  for (uint64_t i = start_search_order; i < BUDDY_MAX_ORDER; i++) {
    ret = free_list_pop(i);
    if (ret != 0) {
      if (bmalloc_enable_printing) {
        printf("found free block %p (of order %d) in free list, splitting\n", ret, i);
      }
      ret = split_and_allocate_in_block(ret, i, order);
      return ret;
    }
  }
  if (bmalloc_enable_printing) { printf("no free blocks found, allocating new top level block\n"); }
  void *new_block = allocate_top_level_block();
  if (bmalloc_enable_printing) { printf("Top level block allocated at %p\n", new_block); }
  if (new_block == 0) return 0;
  return split_and_allocate_in_block(new_block, BUDDY_MAX_ORDER, order);
}


void *_buddy_malloc(uint32_t order) {
  if (order < BUDDY_MIN_ORDER) order = BUDDY_MIN_ORDER;

  if (bmalloc_enable_printing) { printf("buddy allocating 2**%d bytes\n", order); }

  void *ret = free_list_pop(order);
  if (ret != 0) {
    uint32 idx               = -1U;
    struct metadata_page *mp = find_buddy_page_metadata(&idx, ret);
    if (bmalloc_enable_printing) {
      printf("found free block %p (of exact order %d) in free list\n", ret, order);
    }
    mark_block(&mp->buddy_mappings[idx], ret, order, 1);
    return ret;
  }
  return _buddy_malloc_split_or_allocate(order, order + 1);
}

void *_buddy_highalign_malloc(uint32_t order, uint32_t alignment_order) {
  if (order < BUDDY_MIN_ORDER) order = BUDDY_MIN_ORDER;
  if (bmalloc_enable_printing) {
    printf("buddy allocating 2**%d bytes with high alignment 2**%d\n", order, alignment_order);
  }
  void *ret = metadata.buddy_free_lists[FREELIST_INDEX(order)];
  void *prev;
  while (ret != 0 || ((uint64_t)ret % (1 << alignment_order) != 0)) {
    prev = ret;
    ret  = *(void **)ret;
  }
  if (ret != 0) {
    if (bmalloc_enable_printing) {
      printf("found free block %p (of exact order %d) with correct alignment in free list\n", ret, order);
    }
    uint32 idx;

    struct metadata_page *mp = find_buddy_page_metadata(&idx, ret);
    mark_block(&mp->buddy_mappings[idx], ret, order, 1);

    *(void **)prev = *(void **)ret;
    return ret;
  }
  return _buddy_malloc_split_or_allocate(order, alignment_order);
}

static void *_malloc_large(uint32_t size) {
  uint64 npages = PGROUNDUP(size) / PGSIZE;
  if (bmalloc_enable_printing) {
    printf("malloc_large allocating %d bytes (%d pages)\n", size, npages);
  }
  void *mem = mmap(0, npages * PGSIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  if (mem == 0) return 0;

  uint32 idx                    = -1U;
  struct metadata_page *md_page = get_or_allocate_metadata_page(&idx, IS_MMAP);
  if (md_page == 0) {
    munmap(mem, npages * PGSIZE);
    return 0;
  }
  if (md_page == 0) {
    printf("FATAL: md_page == 0\n");
    exit(-1);
    term();
  }
  if (bmalloc_enable_printing) { printf("md_page = %p, idx = %d\n", md_page, idx); }

  md_page->mmap_mappings[idx].address  = mem;
  md_page->mmap_mappings[idx].npages   = npages;
  md_page->mmap_mappings[idx].is_valid = 1;
  md_page->num_mappings_free--;
  metadata.num_mmap_metadata_free--;


  char *maxmem = (char *)mem + npages * PGSIZE;
  if (md_page->max_address_mapped < maxmem) md_page->max_address_mapped = maxmem;
  if (md_page->min_address_mapped > mem) md_page->min_address_mapped = mem;

  if (bmalloc_enable_printing) {
    printf("malloc_large allocated %d bytes (%d pages) at %p\n", size, npages, mem);
  }
  return mem;
}

void *malloc(uint32_t size) {
  if (size == 0) return 0;
  if (size >= PGSIZE) return _malloc_large(size);
  if (bmalloc_enable_printing) {
    printf("buddy malloc %d bytes\n", size);
    void *ret = _buddy_malloc(ord_of_next_power_of_two(size));
    printf("buddy malloc returned %p\n", ret);
    *(char *)ret = 0xab;
    printf("test write to %p succeeded\n", ret);
    *(char *)ret = 0x00;
    return ret;
  }
  return _buddy_malloc(ord_of_next_power_of_two(size));
}

void _free_large(void *ptr) {
  uint32 idx;
  struct metadata_page *mp = find_mmap_page_metadata(&idx, ptr);

  if (mp == 0) {
    printf("     DID NOT FIND PM FOR %p\n", ptr);
    return;
  }
  mp->mmap_mappings[idx].is_valid = 0;
  mp->num_mappings_free++;
  metadata.num_mmap_metadata_free++;


  mp->min_address_mapped = (void *)-1ULL;
  mp->max_address_mapped = 0;
  for (uint32 i = 0; i < mp->num_mappings_free; i++) {
    if (!mp->mmap_mappings[i].is_valid) continue;
    if (mp->mmap_mappings[i].address < mp->min_address_mapped)
      mp->min_address_mapped = mp->mmap_mappings[i].address;
    if ((uint64)mp->mmap_mappings[i].address + (uint64)mp->mmap_mappings[i].npages * PGSIZE >
        (uint64)mp->max_address_mapped)
      mp->max_address_mapped =
        (void *)((uint64)mp->mmap_mappings[i].address + (uint64)mp->mmap_mappings[i].npages * PGSIZE);
  }

  munmap(mp->mmap_mappings[idx].address, mp->mmap_mappings[idx].npages * PGSIZE);
}


void _buddy_free(void *ptr) {
  uint32 idx               = -1U;
  struct metadata_page *mp = find_buddy_page_metadata(&idx, ptr);
  for (int order = BUDDY_MIN_ORDER; order <= BUDDY_MAX_ORDER; order++) {
    if ((uint64_t)ptr % (1 << order) != 0) { continue; }
    if (!is_block_marked(&mp->buddy_mappings[idx], ptr, order)) { continue; }
    mark_block(&mp->buddy_mappings[idx], ptr, order, 0);
    if (order == BUDDY_MAX_ORDER) {
      if (bmalloc_enable_printing) printf("push maximal order=%d ptr=%x to freelist\n", order, ptr);
      free_list_push(order, ptr);
      return;
    }
    int is_left_buddy = ((uint64_t)PAGE_OFFSET(ptr) >> order) % 2 == 0;
    char *buddy_ptr   = (char *)ptr + (is_left_buddy ? (1 << order) : -(1 << order));

    if (bmalloc_enable_printing) {
      printf("freeing block %p (order %d), buddy is %p\n", ptr, order, buddy_ptr);
    }

    if (is_block_marked(&mp->buddy_mappings[idx], buddy_ptr, order)) {

      if (bmalloc_enable_printing) {
        printf("buddy is marked, can not coalesce; adding ptr to freelist\n");
      }
      free_list_push(order, ptr);
    } else {

      if (bmalloc_enable_printing) { printf("buddy is not marked, coalescing\n"); }
      //printf("coalesce push order=%d ptr=%x buddy_ptr=%x to freelist, ptr is %s\n", order + 1, ptr,
      //  buddy_ptr, (is_left_buddy ? " left buddy" : "right buddy"));
      free_list_push(order + 1, is_left_buddy ? ptr : buddy_ptr);
    }
    break;
  }
}

void free(void *ptr) {
  if (ptr == 0) return;
  if ((uint64_t)ptr >= MAX_MEM) {
    if (bmalloc_enable_printing) printf("free: large size\n");
    _free_large(ptr);
  } else {
    if (bmalloc_enable_printing) printf("free: buddy size\n");
    _buddy_free(ptr);
  }
}

int bmalloc_enable_printing = 0;

block block_alloc(uint32_t size, uint32_t align) {
  if (bmalloc_enable_printing) printf("block_alloc(%d, %d)\n", size, align);

  if (size == 0) return {0, 0, 0};

  if (align > PGSIZE) {
    if (bmalloc_enable_printing) {
      printf("NOT SUPPORTED: align %d > PGSIZE %d, returning 0\n", align, PGSIZE);
    }
    return {0, 0, 0};
  }
  if (size >= PGSIZE) { return {_malloc_large(size), ROUND_UP_TO_PAGE_SIZE(size), PGSIZE}; }

  uint32_t order = ord_of_next_power_of_two(size);

  if (align <= size) return {_buddy_malloc(order), (1U << order), (1U << order)};

  uint32_t alignment_order = ord_of_next_power_of_two(align);
  if (bmalloc_enable_printing) printf("Buddy allocating order = %d\n", order);
  return {_buddy_highalign_malloc(order, alignment_order), (1U << order), (1U << alignment_order)};
}

void block_free(block block) {
  if (bmalloc_enable_printing) printf("block_free default called\n");
  free(block.begin);
}
void setup_balloc() {
  if (bmalloc_enable_printing) printf("setup_balloc default called\n");
  setup_malloc();
}

void setup_malloc() {
  if (bmalloc_enable_printing) printf("setup_malloc default called\n");

  metadata.end_of_sbrk_segment = sbrk(0);
  if (bmalloc_enable_printing) {
    printf("end of sbrk segment is %p\n", metadata.end_of_sbrk_segment);
  }
  for (int i = 0; i < NUM_FREE_LISTS; i++) metadata.buddy_free_lists[i] = 0;
  metadata.num_mmap_metadata_free    = 0;
  metadata.num_buddy_metadata_free   = 0;
  metadata.first_buddy_metadata_page = 0;
  metadata.last_buddy_metadata_page  = 0;
  metadata.first_mmap_metadata_page  = 0;
  metadata.last_mmap_metadata_page   = 0;

  if (bmalloc_enable_printing) {}
}
