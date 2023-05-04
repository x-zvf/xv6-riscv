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

inline static uint log2(uint v) {
  switch(v) {
    case 1024:
      return 10;
    case 512:
      return 9;
    case 256:
      return 8;
    case 128:
      return 7;
    case 64:
      return 6;
    case 32:
      return 5;
    case 16:
      return 4;
    default:
      exit(1);
      return -1;
  }
}

#define ALL_BUT_64 0xFFFFFFFFFFFFFFC0

static void initialize_bucket(struct BucketPage *bucket, uint64_t size) {
    bucket->element_size = size;
    for(int j = 0; j<4; j++)
      bucket->free_bitset[j] = 0;
    int num_entries = NUM_ITEMS_IN_BUCKETPAGE(size);
    for(int i = num_entries; i < 256; i++) {
      bucket->free_bitset[i & ALL_BUT_64] |= (1 << (i & 0x3f));
    }
    bucket->next = 0;
}

static struct MallocMeta malloc_meta;

static struct MallocHeader *freelist_allocate(uint nbytes, uint alignment) {
  MallocHeader *head, *prev;
  
  return 0;
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



static void *allocate_in_bucket(BucketPage *bucket) {
  BucketPage *prev = bucket;
  while(bucket) {
    printf("  <M> trying bucket at %p\n", prev);
    if((  bucket->free_bitset[0]
        & bucket->free_bitset[1]
        & bucket->free_bitset[2]
        & bucket->free_bitset[3]) != -1ULL) { //check if not full
      uint64_t index = find_first_unset(bucket->free_bitset);
      // if(index < NUM_ITEMS_IN_BUCKETPAGE(bucket->element_size)) {
      printf("  <M> Bucket has free slot at index %d\n",index);
      bucket->free_bitset[index & ALL_BUT_64] |= (1 << (index & 0x3f));
      void *result = &(bucket->data[bucket->element_size * index]);
      return result;
    }
    prev = bucket;
    bucket = bucket->next;
  };
  // no more allocated buckets, get a new bucket and allocate first slot
  printf(" <M> No more buckets, allocating new.\n");
  bucket = (BucketPage *)freelist_allocate(PGSIZE, PGSIZE);
  if(bucket == 0) return 0;
  
  initialize_bucket(bucket, prev->element_size);
  bucket->free_bitset[0] = 1LL;
  prev->next = bucket;
  
  return &(bucket->data[0]);
}


void *_malloc(uint nbytes) {
  printf("<M> Malloc called with nbytes=%d\n", nbytes);
  if(nbytes == 0) return 0;
  if(nbytes < 16) nbytes = 16;
  
  if(nbytes <= 1024) {
    printf("  <M> Allocating nbytes=%d\n in bucket\n", nbytes); 
    nbytes = next_highest_power_of_two(nbytes);
    uint idx = log2(nbytes) - 4;
    BucketPage *bucket = malloc_meta.fixed_size_buckets[idx];
    if(bucket == 0) {
      printf("  <M> Bucket at index=%d does not exist. allocating\n", idx);
      bucket = (BucketPage *)freelist_allocate(PGSIZE, PGSIZE);
      if(bucket == 0) return 0;
      initialize_bucket(bucket, nbytes);
      malloc_meta.fixed_size_buckets[idx] = bucket;
    }
    return allocate_in_bucket(bucket);
  }
  // TODO: make sure, it is never a special page,
  return freelist_allocate(nbytes, 16)+sizeof(MallocHeader);
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
      malloc_meta.fixed_size_buckets[i] = 0;
  }

}


void _free(void *ptr) {
  // if(ptr == 0) return;
  // Look at page beginning, is it a special page?

}