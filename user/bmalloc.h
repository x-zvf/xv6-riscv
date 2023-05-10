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

#define MAX_MEM 1024*1024*128U // 128 MB

#define DEBUG_MALLOC 0

#define BUDDY_MIN_ORDER 3 // we allocate at least 8 bytes
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
    union {
        uint64_t in_use_bitset[16];
        uint64_t mmap_npages;
    };
};

struct buddy_mappings_segment {
    void *min_address_mapped;
    void *max_address_mapped;
    struct buddy_mappings_segment *next_segment;
    uint32_t mappings_capacity;
    uint32_t mappings_populated;
    struct buddy_page_metadata mappings[];
};

#define FREELIST_INDEX(order) (order - BUDDY_MIN_ORDER)
#define NUM_FREE_LISTS (BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1)
#define ROUND_UP_TO_PAGE_SIZE(x) ((((x) + (PGSIZE - 1))) & ~(PGSIZE - 1))
#define PAGE_BASE(ptr) ((void *)( (uint64_t)ptr & ~(PGSIZE - 1)))
#define PAGE_OFFSET(ptr) ((void *)( (uint64_t)ptr & (PGSIZE - 1)))

struct malloc_metadata {
    struct buddy_mappings_segment *first_metadata_segment; //for lookup
    struct buddy_mappings_segment *last_metadata_segment; //for appending a new segment

    void *free_lists[NUM_FREE_LISTS];
    void *end_of_sbrk_segment;
};

#ifndef __cplusplus
#define BALLOC(T,N) block_alloc(sizeof(T)*(N), _Alignof(T))
#else
#define BALLOC(T,N) block_alloc(sizeof(T)*(N), alignof(T))
#endif

void *_malloc(uint32_t size);
void _free(void *ptr);

// #define malloc _malloc
// #define free _free

block block_alloc(uint32_t size, uint32_t align);

void block_free(block block);
void setup_balloc(void);



void setup_malloc(void);



#ifdef __cplusplus
}

template<typename T>
struct typed_block {
    T *begin;
    uint32_t size;
    uint32_t align;
    explicit operator block() {
        return {static_cast<void*>(begin), size, align};
    }
    auto untyped() {
        return static_cast<block>(*this);
    }
    static typed_block make_typed(block b) {
        return {static_cast<T*>(b.begin), b.size, b.align};
    }
};

template<typename T>
inline typed_block<T> block_alloc_typed(uint32_t num_elements) {
    return typed_block<T>::make_typed(block_alloc(num_elements * sizeof(T), alignof(T)));
};

#endif

#endif

