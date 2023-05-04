#include "user/bmalloc.h"
#include "user/user.h"

extern "C" {
  
block block_alloc(uint32_t size, uint32_t align) __attribute__((weak));

void block_free(block block) __attribute__((weak));
void setup_balloc(void) __attribute__((weak));
void setup_malloc() __attribute__((weak));

}
#define NULL 0

static MallocMeta metadata;

static Header *moremem(uint32_t nheaders) {
  Header *newmem = (Header *)sbrk(nheaders * sizeof(Header));
  if(newmem == (void *)-1)
    return NULL;
  newmem->size = nheaders;
  return newmem;
}

void *_malloc(uint32_t nbytes, uint32_t align) {
  // printf(" <M> Malloc called with %d\n", nbytes);
  if(nbytes == 0)
    return NULL;
  // round up
  uint32_t nheaders = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  

  if(metadata.free_list == NULL) {
    Header *newmem = moremem(sizeof(Header) * nheaders <PGSIZE ? (2*PGSIZE)/sizeof(Header) : PGSIZE + nheaders);
    if(newmem == NULL)
      return NULL;
    newmem->next = NULL;
    newmem->prev = NULL;
    metadata.free_list = newmem;
    //metadata.free_list->prev = newmem;
  }
  Header *possible_space = metadata.free_list;
  Header *prev = possible_space;

  while(1) {
    if(possible_space == NULL) {
      Header *newmem = moremem(sizeof(Header) * nheaders < PGSIZE ? (2*PGSIZE)/sizeof(Header) : PGSIZE + nheaders);
      if(newmem == NULL)
        return NULL;
      prev->next = newmem;
      newmem->prev = prev;
      newmem->next = NULL;
      //metadata.free_list->prev = newmem;
    }
    void *the_address = possible_space + 1;
    uint32_t size_in_bytes = ((nheaders - 1) * sizeof(Header));
    uint32_t alignment_bytes = ((align - ((uint64_t)the_address % align)) % align);
    uint32_t aligned_nheaders = ((size_in_bytes + alignment_bytes + sizeof(Header) - 1) / sizeof(Header)) + 1;
    // printf(" <M> the_address=%p, nheaders=%d size_in_bytes=%d, alignment_in_bytes=%d, aligned_nheaders=%d", the_address, nheaders, size_in_bytes, alignment_bytes, aligned_nheaders);
    if(possible_space->size < aligned_nheaders) {
      prev = possible_space;
      possible_space = possible_space->next;
      continue;
    }
    if(possible_space->size == aligned_nheaders) {
      possible_space->prev->next = possible_space->next;
      possible_space->next->prev = possible_space->prev;
      possible_space->size = 0;
    } else {
      uint32_t old_size = possible_space->size;
      possible_space->size = nheaders;

      Header *new_header = possible_space + nheaders;
      new_header->size = old_size - aligned_nheaders;
      
      possible_space->prev->next = new_header;
      new_header->prev = possible_space->prev;
      new_header->next = possible_space->next;
      possible_space->next->prev = new_header;
    }

    possible_space->next = metadata.allocated_list;
    metadata.allocated_list = possible_space;
    return (void*)(((uint64_t)(possible_space + 1)) + alignment_bytes);
  }
}

void *malloc(uint32_t nbytes) {
  return _malloc(nbytes, 16);
}

void free(void *data) {
  // printf(" <X> free called\n");
  if(data == 0)
    return;
  Header *header = (Header*)data - 1;
  if(header->prev != NULL)
    header->prev->next = header->next;
  if(header->next != NULL)
    header->next->prev = header->prev;
  
  Header *insert_at = metadata.free_list;
  Header *prev = NULL;
  while(insert_at != NULL && insert_at->size < header->size) {
    prev = insert_at;
    insert_at = insert_at->next;
  }
  header->prev = insert_at;
  if(insert_at != NULL) {
    header->next = insert_at->next;
    header->prev = insert_at;
  }
  if(prev != NULL) {
    prev->next = header;
    header->prev = prev;
  } else {
    metadata.free_list = header;
    header->prev = NULL;
    header->next = NULL;
  }
}



block block_alloc(uint32_t size, uint32_t align) {
  // printf("block_alloc default called\n");
  if(align < 16) align = 16;
  return {_malloc(size, align), size, align};
}

void block_free(block block) {
  // printf("block_free default called\n");
  free(block.begin);
}
void setup_balloc() {
  // printf("setup_balloc default called\n");
  setup_malloc();
}

void setup_malloc() {
  // printf("setup_malloc default called\n");
  metadata.allocated_list = NULL;
  metadata.free_list = NULL;
}
