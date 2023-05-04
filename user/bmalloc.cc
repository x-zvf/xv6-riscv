#include "user/bmalloc.h"
#include "user/user.h"

extern "C" {
  
block block_alloc(uint32_t size, uint32_t align) __attribute__((weak));

void block_free(block block) __attribute__((weak));
void setup_balloc(void) __attribute__((weak));
void setup_malloc() __attribute__((weak));

}

static struct MallocMeta malloc_meta;

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
      malloc_meta.fixed_size_buckets[i] = bucket;
  }

}


void *malloc(uint nbytes) {
  if(nbytes == 0) return;
  if(nbytes < 16) nbytes = 16;


}

struct MallocHeader *malloc_internal(uint nbytes, uint alignment) {

}

void free(void *ptr) {
  if(ptr == 0) return;

}