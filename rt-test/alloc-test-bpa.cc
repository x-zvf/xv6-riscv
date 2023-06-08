/*!
 * \file
 * \brief page aligned allocations
 */

#include "rt-test/assert.h"
#include "user/bmalloc.h"

void main() {
  // TODO: Find out, why sys_munmap fails here!
  // TODO: Find out, why freewalk panics!
  // assert(0==1);

  setup_balloc();
  for (int i = 4096; i < 1 << 16; i <<= 1) {
    auto block = block_alloc(i, 4096);
    assert(block.begin);
    assert(!(reinterpret_cast<uint64>(block.begin) & ((1 << 12) - 1)));
    block_free(block);
  }
}
