#include "user/bmalloc.h"
#include "user/user.h"

extern "C" {
  
block block_alloc(uint32_t size, uint32_t align) __attribute__((weak));

void block_free(block block) __attribute__((weak));
void setup_balloc(void) __attribute__((weak));
void setup_malloc() __attribute__((weak));

}

// http://graphics.stanford.edu/%7Eseander/bithacks.html#RoundUpPowerOf2
inline static uint next_highest_power_of_two(uint v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

inline static uint index_of_p2(uint v) {
  switch(v) {
    case 1024:
      return 6;
    case 512:
      return 5;
    case 256:
      return 4;
    case 128:
      return 3;
    case 64:
      return 2;
    case 32:
      return 1;
    case 16:
      return 0;
  }
  return -1;
}

static struct MallocMeta malloc_meta;

static struct MallocHeader *malloc_internal(uint nbytes, uint alignment) {
  
}

//array length 4
static uint64_t find_first_unset(uint64_t bitset[]) {
  for(int i = 0; i<4; i++) {
    for(int j = 0; j < 64; j++){
      if((bitset[i] & (((uint64_t)1) << j)) == 0)
        return i*j;
    }
  }
  return 256;
}

#define ALL_BUT_64 0xFFFFFFFFFFFFFFC0

static void *allocate_in_bucket(uint bucketIndex) {
  BucketPage *bucket = malloc_meta.fixed_size_buckets[bucketIndex];
  BucketPage *prev = 0;
  do {
    if( bucket->free_bitset[0]
      & bucket->free_bitset[1]
      & bucket->free_bitset[2]
      & bucket->free_bitset[3] != -1ULL) { //check if full
      int index = find_first_unset(bucket->free_bitset);
      if(index < NUM_ITEMS_IN_BUCKETPAGE(bucket->element_size)) {
        bucket->free_bitset[index & ALL_BUT_64] |= (1 << (index & 0x3f));
        void *result = bucket->data[bucket->element_size * index];
        return result;
      }
    }
    prev = bucket;
  }while((bucket = bucket->next));
  // no more allocated buckets, get a new bucket and allocate first slot
  bucket = (BucketPage *)malloc_internal(PGSIZE, PGSIZE);
  if(bucket == 0) return 0;
  
  bucket->free_bitset[0] = 1LL;
  for(int i = 1; i<4; i++)
    bucket->free_bitset[i] = 0LL;
  bucket->element_size = prev->element_size;
  bucket->next = 0;
  prev->next = bucket;
  
  return bucket->data[0];
}


void *_malloc(uint nbytes) {
  if(nbytes == 0) return;
  if(nbytes < 16) nbytes = 16;
  
  if(nbytes <= 1024) {
    nbytes = next_highest_power_of_two(nbytes);
    uint idx = index_of_p2(nbytes);
    return allocate_in_bucket(idx);
  }
  return malloc_internal(nbytes, 16);
}


block block_alloc(uint32_t size, uint32_t align) {
  printf("block_alloc default called\n");
  return {malloc(size), size, align};
}

void block_free(block block) {
  printf("block_free default called\n");
  free(block.begin);
}
void setup_balloc() {
  printf("setup_balloc default called\n");
}

void setup_malloc() {
  printf("setup_malloc default called\n");
  for(int i = 0; i < MALLOC_NUM_BUCKETS; i++) {
      BucketPage *bucket = (BucketPage *)malloc_internal(PGSIZE, PGSIZE);
      if(bucket == 0) {
        exit(-1); // should never happen
      }
      bucket->size = (1 << (4+i));

      for(int j = 0; j<4; j++)
        bucket->free_bitset[j] = 0;
      
      int num_entries = NUM_ITEMS_IN_BUCKETPAGE(bucket->size);
      for(int i = num_entries; i < 256; i++) {
        bucket->free_bitset[index & ALL_BUT_64] |= (1 << (index & 0x3f));
      }
      
      bucket->next = 0;
      malloc_meta.fixed_size_buckets[i] = bucket;
  }

}





// void free(void *ptr) {
//   if(ptr == 0) return;

// }