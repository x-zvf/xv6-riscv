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

static struct buddy_mappings_segment *allocate_metadata_until_next_power_of_two() {
  uint64_t nbytes =
    ROUND_UP_TO_PAGE_SIZE(sizeof(struct buddy_mappings_segment) + (uint64_t)metadata.end_of_sbrk_segment) -
    (uint64_t)metadata.end_of_sbrk_segment;
  char *new_break = sbrk(nbytes);
  if (bmalloc_enable_printing) { /*REMOVE*/
    printf("allocated %d bytes of metadata at %p\n", nbytes, new_break);
    if ((uint64_t)new_break % PGSIZE != 0) {
      printf("FATAL: sbrk returned an address that is not page aligned");
      exit(-1);
    }
  } /*REMOVE*/
  metadata.end_of_sbrk_segment               = new_break + nbytes;
  struct buddy_mappings_segment *new_segment = (struct buddy_mappings_segment *)new_break;
  new_segment->next_segment                  = 0;
  new_segment->min_address_mapped            = metadata.end_of_sbrk_segment;
  new_segment->max_address_mapped            = (void *)-1;
  new_segment->mappings_populated            = 0;
  new_segment->mappings_capacity =
    (nbytes - sizeof(struct buddy_mappings_segment)) / sizeof(struct buddy_page_metadata);

  if (bmalloc_enable_printing) { /*REMOVE*/
    printf("allocated new metadata segment at %p with size %d, holding %d metadata entries\n",
      new_segment, nbytes, new_segment->mappings_capacity);
  } /*REMOVE*/
  return new_segment;
}

static void *free_list_pop(int order) {
  if (bmalloc_enable_printing)
    printf("popping from freelist order=%d, index=%d, nfreelists=%d\n", order, FREELIST_INDEX(order), NUM_FREE_LISTS);
  void *ret = metadata.free_lists[FREELIST_INDEX(order)];
  if (bmalloc_enable_printing) printf(" =>popped: ret is %x \n", ret);
  if (bmalloc_enable_printing) printf(" =>end_of_sbrk=%x\n", metadata.end_of_sbrk_segment);

  if (ret != 0) { metadata.free_lists[FREELIST_INDEX(order)] = *(void **)ret; }
  return ret;
}

static void free_list_push(int order, void *blockptr) {
  *(void **)blockptr                         = metadata.free_lists[FREELIST_INDEX(order)];
  metadata.free_lists[FREELIST_INDEX(order)] = blockptr;
}

static struct buddy_page_metadata *find_page_metadata(void *blockptr) {
  void *page_base                        = PAGE_BASE(blockptr);
  struct buddy_mappings_segment *segment = metadata.last_metadata_segment;
  // search first segment in reverse order, because it is very likely, that the last mapped block
  // is accessed again soon
  for (int64_t i = segment->mappings_populated - 1; i > 0; i--) {
    if (segment->mappings[i].address == page_base) return &segment->mappings[i];
  }
  segment = metadata.first_metadata_segment;
  while (segment != 0) {
    if (page_base >= segment->max_address_mapped) {
      segment = segment->next_segment;
      continue;
    }
    for (uint32_t i = 0; i < segment->mappings_populated; i++) {
      if (segment->mappings[i].address == page_base) return &segment->mappings[i];
    }
    segment = segment->next_segment;
  }
  if (bmalloc_enable_printing) { /*REMOVE*/
    printf("FATAL: could not find page metadata for block %p\n", blockptr);
    exit(-1);
  } /*REMOVE*/
  return 0;
}

static int is_block_marked(struct buddy_page_metadata *pm, void *blockptr, uint32_t order) {
  uint32_t nth_of_row = (uint64_t)PAGE_OFFSET(blockptr) >> order;
  uint32_t row        = (BUDDY_MAX_ORDER - order);
  uint32_t bit        = (1 << row) - 1 + nth_of_row;
  if (bmalloc_enable_printing) { /*REMOVE*/
    printf("block %p (order %d) is %dth in row %d, bit %d\n", blockptr, order, nth_of_row, row, bit);
    if (bit > 64 * 16 - 1) {
      printf("FATAL: bit %d is out of range. ptr=%p, order=%d\n", bit, blockptr, order);
      exit(-1);
    }
    printf("checking if block %p (order %d) is in use => bit %d is %s\n", blockptr, order, bit,
      (pm->in_use_bitset[bit / 64] & (1ULL << (bit % 64))) != 0 ? "set" : "not set");
  } /*REMOVE*/
  return (pm->in_use_bitset[bit / 64] & (1ULL << (bit % 64))) != 0;
}

static void mark_block(struct buddy_page_metadata *pm, void *blockptr, uint32_t order, char in_use) {
  uint32_t nth_of_row = (uint64_t)PAGE_OFFSET(blockptr) >> order;
  uint32_t row        = (BUDDY_MAX_ORDER - order);
  uint32_t bit        = (1 << row) - 1 + nth_of_row;
  if (bmalloc_enable_printing) { /*REMOVE*/
    printf("marking block %p (order %d) as %s => bit %d set\n", blockptr, order, in_use ? "in use" : "free", bit);
  } /*REMOVE*/
  if (in_use)
    pm->in_use_bitset[bit / 64] |= (1ULL << (bit % 64));
  else
    pm->in_use_bitset[bit / 64] &= ~(1ULL << (bit % 64));
}

static void *split_and_allocate_in_block(void *blockptr, uint64_t block_order_size, uint32_t order) {
  buddy_page_metadata *page_metadata = find_page_metadata(blockptr);
  mark_block(page_metadata, blockptr, order, 1);

  // split the block into two halves, allocating in the left half and pushing the right half onto the free list
  for (uint32_t i = block_order_size - 1; i >= order; i--) {
    char *right_half = ((char *)blockptr) + (1 << i);
    if (bmalloc_enable_printing) { /*REMOVE*/
      printf("splitting block %p (order %d) into left %p (order %d) and right %p (order %d)\n",
        blockptr, i, blockptr, i - 1, right_half, i - 1);
    } /*REMOVE*/
    free_list_push(i, right_half);
  }
  return blockptr;
}

static struct buddy_mappings_segment *get_or_allocate_metadata_segment() {
  struct buddy_mappings_segment *segment = metadata.last_metadata_segment;
  if (segment == 0) {
    segment = allocate_metadata_until_next_power_of_two();
    if (segment == 0) return 0;
    metadata.first_metadata_segment = segment;
    segment->min_address_mapped     = metadata.end_of_sbrk_segment;
    metadata.last_metadata_segment  = segment;
  } else if (segment->mappings_populated == segment->mappings_capacity) {
    segment->next_segment = allocate_metadata_until_next_power_of_two();
    if (segment->next_segment == 0) return 0;
    segment                        = segment->next_segment;
    segment->min_address_mapped    = metadata.end_of_sbrk_segment;
    metadata.last_metadata_segment = segment;
  }
  return segment;
}

static void *allocate_top_level_block() {
  // allocate Metadata

  struct buddy_mappings_segment *segment = get_or_allocate_metadata_segment();

  uint64_t bytes_to_allocate = 1ULL << BUDDY_MAX_ORDER;

  if (bmalloc_enable_printing) { /*REMOVE*/
    if (bytes_to_allocate != PGSIZE) {
      printf("FATAL: top level block size is not a page size. Peter can't do maths\n");
      exit(-1);
    }
  } /*REMOVE*/

  char *new_break = sbrk(bytes_to_allocate);
  if (new_break == (char *)-1) {
    if (bmalloc_enable_printing) { /*REMOVE*/
      printf("FATAL: sbrk failed\n");
    } /*REMOVE*/
    return 0;
  }

  if (bmalloc_enable_printing) { /*REMOVE*/
    printf("allocated top level block at %p (allocated %d bytes) \n", new_break, bytes_to_allocate);
  } /*REMOVE*/

  segment->mappings[segment->mappings_populated].address = metadata.end_of_sbrk_segment;
  metadata.end_of_sbrk_segment                           = new_break + bytes_to_allocate;

  segment->mappings_populated++;
  if (segment->mappings_populated == segment->mappings_capacity) {
    segment->max_address_mapped = (char *)metadata.end_of_sbrk_segment - 1;
  }

  for (int i = 0; i < 16; i++) segment->mappings[segment->mappings_populated].in_use_bitset[i] = 0;

  return new_break;
}

void *_buddy_malloc_split_or_allocate(uint32_t order, uint32_t start_search_order) {
  void *ret;

  for (uint64_t i = start_search_order; i < BUDDY_MAX_ORDER; i++) {
    ret = free_list_pop(i);
    if (ret != 0) {
      if (bmalloc_enable_printing) { /*REMOVE*/
        printf("found free block %p (of order %d) in free list, splitting\n", ret, i);
      } /*REMOVE*/
      ret = split_and_allocate_in_block(ret, i, order);
      return ret;
    }
  }
  if (bmalloc_enable_printing) { /*REMOVE*/
    printf("no free blocks found, allocating new top level block\n");
  } /*REMOVE*/
  void *new_block = allocate_top_level_block();
  if (new_block == 0) return 0;
  return split_and_allocate_in_block(new_block, BUDDY_MAX_ORDER, order);
}


void *_buddy_malloc(uint32_t order) {
  if (order < BUDDY_MIN_ORDER) order = BUDDY_MIN_ORDER;

  if (bmalloc_enable_printing) { /*REMOVE*/
    printf("buddy allocating 2**%d bytes\n", order);
  } /*REMOVE*/

  void *ret = free_list_pop(order);
  if (ret != 0) {
    struct buddy_page_metadata *pm = find_page_metadata(ret);
    if (bmalloc_enable_printing) { /*REMOVE*/
      printf("found free block %p (of exact order %d) in free list\n", ret, order);
    } /*REMOVE*/
    mark_block(pm, ret, order, 1);
    return ret;
  }
  return _buddy_malloc_split_or_allocate(order, order + 1);
}

void *_buddy_highalign_malloc(uint32_t order, uint32_t alignment_order) {
  if (order < BUDDY_MIN_ORDER) order = BUDDY_MIN_ORDER;
  if (bmalloc_enable_printing) { /*REMOVE*/
    printf("buddy allocating 2**%d bytes with high alignment 2**%d\n", order, alignment_order);
  } /*REMOVE*/
  void *ret = metadata.free_lists[FREELIST_INDEX(order)];
  void *prev;
  while (ret != 0 || ((uint64_t)ret % (1 << alignment_order) != 0)) {
    prev = ret;
    ret  = *(void **)ret;
  }
  if (ret != 0) {
    if (bmalloc_enable_printing) { /*REMOVE*/
      printf("found free block %p (of exact order %d) with correct alignment in free list\n", ret, order);
    } /*REMOVE*/
    struct buddy_page_metadata *pm = find_page_metadata(ret);
    mark_block(pm, ret, order, 1);

    *(void **)prev = *(void **)ret;
    return ret;
  }
  return _buddy_malloc_split_or_allocate(order, alignment_order);
}

static void *_malloc_large(uint32_t size) {
  uint64 npages = PGROUNDUP(size) / PGSIZE;
  void *mem     = mmap(0, npages * PGSIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  if (mem == 0) return 0;

  struct buddy_mappings_segment *segment = get_or_allocate_metadata_segment();
  if (segment == 0) {
    munmap(mem, npages * PGSIZE);
    return 0;
  }
  segment->mappings[segment->mappings_populated].address     = mem;
  segment->mappings[segment->mappings_populated].mmap_npages = npages;
  segment->mappings_populated++;

  char *maxmem = (char *)mem + npages * PGSIZE;
  if (segment->max_address_mapped == (void *)-1 || segment->max_address_mapped < maxmem) {
    segment->max_address_mapped = maxmem;
  }
  return mem;
}

void *malloc(uint32_t size) {
  if (size == 0) return 0;
  if (size >= PGSIZE) return _malloc_large(size);
  return _buddy_malloc(ord_of_next_power_of_two(size));
}

void _free_large(void *ptr) {
  struct buddy_page_metadata *pm = find_page_metadata(ptr);
  if (pm == 0) {
    printf("     DID NOT FIND PM FOR %p\n", ptr);
    return;
  }
  munmap(pm->address, pm->mmap_npages * PGSIZE);
}


void _buddy_free(void *ptr) {
  struct buddy_page_metadata *pm = find_page_metadata(ptr);
  for (int order = BUDDY_MIN_ORDER; order <= BUDDY_MAX_ORDER; order++) {
    if ((uint64_t)ptr % (1 << order) != 0) { continue; }
    if (!is_block_marked(pm, ptr, order)) { continue; }
    mark_block(pm, ptr, order, 0);
    if (order == BUDDY_MAX_ORDER) {
      if (bmalloc_enable_printing) printf("push maximal order=%d ptr=%x to freelist\n", order, ptr);
      free_list_push(order, ptr);
      return;
    }
    int is_left_buddy = ((uint64_t)PAGE_OFFSET(ptr) >> order) % 2 == 0;
    char *buddy_ptr   = (char *)ptr + (is_left_buddy ? (1 << order) : -(1 << order));

    if (bmalloc_enable_printing) { /*REMOVE*/
      printf("freeing block %p (order %d), buddy is %p\n", ptr, order, buddy_ptr);
    } /*REMOVE*/

    if (is_block_marked(pm, buddy_ptr, order)) {

      if (bmalloc_enable_printing) { /*REMOVE*/
        printf("buddy is marked, can not coalesce; adding ptr to freelist\n");
      } /*REMOVE*/
      free_list_push(order, ptr);
    } else {

      if (bmalloc_enable_printing) { /*REMOVE*/
        printf("buddy is not marked, coalescing\n");
      } /*REMOVE*/
      //printf("coalesce push order=%d ptr=%x buddy_ptr=%x to freelist, ptr is %s\n", order + 1, ptr,
      //  buddy_ptr, (is_left_buddy ? " left buddy" : "right buddy"));
      free_list_push(order + 1, is_left_buddy ? ptr : buddy_ptr);
    }
    break;
  }
}

void free(void *ptr) {
  if (ptr == 0) return;
  if ((uint64_t)ptr >= MAX_MEM)
    _free_large(ptr);
  else
    _buddy_free(ptr);
}

int bmalloc_enable_printing = 0;

block block_alloc(uint32_t size, uint32_t align) {
  if (bmalloc_enable_printing) printf("block_alloc(%d, %d)\n", size, align);

  if (size == 0) return {0, 0, 0};

  if (align > PGSIZE) {
    if (bmalloc_enable_printing) { /*REMOVE*/
      printf("NOT SUPPORTED: align %d > PGSIZE %d, returning 0\n", align, PGSIZE);
    } /*REMOVE*/
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
  if (bmalloc_enable_printing) { /*REMOVE*/
    printf("end of sbrk segment is %p\n", metadata.end_of_sbrk_segment);
  } /*REMOVE*/
  for (int i = 0; i < NUM_FREE_LISTS; i++) metadata.free_lists[i] = 0;
}
