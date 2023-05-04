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


struct MallocHeader {
    struct MallocHeader *next;
    // struct MallocHeader *prev;
    uint32_t size;
    // unsigned int:1 used;
};

// yes, this could be made more memory efficient, having a dynamic bitset size, and hiding the bucket size in the pointer
// For 16 byte Buckets, that could reduce the header size from 48-bytes to 40 bytes, at the cost of code complexity.
// As it stands, the header takes up about 1.2% of the total size.
struct BucketPage {
    struct MallocHeader malloc_header;
    struct BucketPage *next;
    uint64_t element_size;
    uint64_t free_bitset[4]; // there are at most PG_SIZE / 16 = 256 allocations/page
    unsigned char data[]; // at least alignof(uint64_t) == 8 bytes aligned
};

#define NUM_ITEMS_IN_BUCKETPAGE(size) ((PGSIZE-sizeof(struct BucketPage)/size))


#define MALLOC_NUM_BUCKETS 7 // 16, 32, 64, 128, 256, 512, 1024
struct MallocMeta {
    struct BucketPage *fixed_size_buckets[MALLOC_NUM_BUCKETS];
    struct BucketPage *fixed_size_buckets_with_free[MALLOC_NUM_BUCKETS]; 
    struct MallocHeader *free_list;
};

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

#define BALLOC(T,N) block_alloc(sizeof(T)*(N), alignof(T))

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
};

template<typename T>
union typed_block_u {
    block untyped;
    typed_block<T> typed;
};

template<typename T>
inline typed_block_u<T> block_alloc_typed(uint32_t num_elements) {
    return {block_alloc(num_elements * sizeof(T), alignof(T))};
};

#endif

#endif

