/*! \file user.h
 * \brief header for userspace standard library
 */

#ifndef INCLUDED_user_bmalloc_h
#define INCLUDED_user_bmalloc_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "kernel/riscv.h"

/*!
 * \brief block allocator struct
 * wraps pointer, size and alignment
 */
struct block {
  void *begin;
  uint32_t size;
  uint32_t align;
};
typedef struct block block;

#define MAX_MEM 1024 * 1024 * 128U // 128 MB

//#define DEBUG_MALLOC 0

#define BUDDY_MIN_ORDER 3  // we allocate at least 8 bytes
#define BUDDY_MAX_ORDER 12 // we allocate at most 4096 bytes (a page)

/*
 0
 1 2
 3 4 5 6
 7 8 9 A B C D E

 0
 0       0
 0   0   0   0
 0 0 0 0 0 0 0 0
*/

// 8 = 2**3 ==> order

struct buddy_page_metadata {
  void *address; // the address of the page, for which this mapping applies.
  uint64_t in_use_bitset[16];
};

struct mmap_page_metadata {
  void *address;
  uint32_t npages : 31;
  uint32_t is_valid : 1;
};



#define METADATA_PAGE_HEADER_SIZE (3 * sizeof(void *) + 2 * sizeof(uint32))
#define NUM_BUDDY_MAPPINGS_PER_METADATA_PAGE                                                       \
  ((PGSIZE - METADATA_PAGE_HEADER_SIZE) / sizeof(struct buddy_page_metadata))
#define NUM_MMAP_MAPPINGS_PER_METADATA_PAGE                                                        \
  ((PGSIZE - METADATA_PAGE_HEADER_SIZE) / sizeof(struct mmap_page_metadata))

struct metadata_page {
  void *min_address_mapped;
  void *max_address_mapped;
  struct metadata_page *next;
  uint32 num_mappings_free;
  uint32 _padding;
  union {
    struct buddy_page_metadata buddy_mappings[0];
    struct mmap_page_metadata mmap_mappings[0];
  };
};


#define FREELIST_INDEX(order) (order - BUDDY_MIN_ORDER)
#define NUM_FREE_LISTS (BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1)
#define ROUND_UP_TO_PAGE_SIZE(x) ((((x) + (PGSIZE - 1))) & ~(PGSIZE - 1))
#define PAGE_BASE(ptr) ((void *)((uint64_t)ptr & ~(PGSIZE - 1)))
#define PAGE_OFFSET(ptr) ((void *)((uint64_t)ptr & (PGSIZE - 1)))

struct malloc_metadata {
  struct metadata_page *first_buddy_metadata_page; //for lookup
  struct metadata_page *last_buddy_metadata_page;  //for appending a new segment
  struct metadata_page *first_mmap_metadata_page;
  struct metadata_page *last_mmap_metadata_page;

  uint32 num_mmap_metadata_free;
  uint32 num_buddy_metadata_free;

  void *buddy_free_lists[NUM_FREE_LISTS];
  void *end_of_sbrk_segment;
};

#ifndef __cplusplus
#define BALLOC(T, N) block_alloc(sizeof(T) * (N), _Alignof(T))
#else
#define BALLOC(T, N) block_alloc(sizeof(T) * (N), alignof(T))
#endif

void *_malloc(uint32_t size);
void _free(void *ptr);

// #define malloc _malloc
// #define free _free

//block block_alloc(uint32_t size, uint32_t align);
//void block_free(block block);
//void setup_balloc(void);
//void setup_malloc(void);


block block_alloc(uint32_t size, uint32_t align) __attribute__((weak));

void block_free(block block) __attribute__((weak));
void setup_balloc(void) __attribute__((weak));
void setup_malloc() __attribute__((weak));

#ifdef __cplusplus
}

template<typename T>
struct typed_block {
  T *begin;
  uint32_t size;
  uint32_t align;
  explicit operator block() { return {static_cast<void *>(begin), size, align}; }
  auto untyped() { return static_cast<block>(*this); }
  static typed_block make_typed(block b) { return {static_cast<T *>(b.begin), b.size, b.align}; }
};

template<typename T>
inline typed_block<T> block_alloc_typed(uint32_t num_elements) {
  return typed_block<T>::make_typed(block_alloc(num_elements * sizeof(T), alignof(T)));
};

#endif

#endif
