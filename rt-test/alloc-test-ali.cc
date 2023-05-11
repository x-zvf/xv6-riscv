/*!
 * \file
 * \brief test allocation alignments
 */

#include "rt-test/assert.h"
#include "user/bmalloc.h"

void test_align(void *ptr, int alignment) {
	auto val = reinterpret_cast<uint64>(ptr);
	if (alignment & 1)
		return;
	if (alignment & 2) {
		assert((val & 1) == 0);
		return;
	}
	if (alignment & 4) {
		assert((val & 3) == 0);
		return;
	}
	if (alignment & 8) {
		assert((val & 7) == 0);
		return;
	}
	assert(!(val & 15));
}

void main() {
	for(int i = 1; i < 16; ++i) {
		printf("i = %d\n", i);
		auto malloced = malloc(i);
		assert(malloced);
		test_align(malloced, i);
		free(malloced);

		auto block = block_alloc(i, i & 1 ? 1 : i & 2 ? 2 : i & 4 ? 4 : 8);
		assert(block.begin);
		test_align(block.begin, i);
		block_free(block);
	}
	printf("passed 1\n");

	for(int i = 16; i < 4096; ++i) {
		auto malloced = malloc(i);
		assert(malloced);
		test_align(malloced, 16);
		free(malloced);

		printf("here ");
		auto block1 = block_alloc(i, 16);
		printf(" 1 ");
		assert(block1.begin);
		test_align(block1.begin, 16);
		printf(" 2 ");
		block_free(block1);

		printf(" 3 ");
		auto block2 = block_alloc(i, 8);
		printf(" 4 ");
		assert(block2.begin);
		test_align(block2.begin, 8);
		printf(" 5 ");
		block_free(block2);

		printf(" 6 ");
		auto block3 = block_alloc(i, 4);
		assert(block3.begin);
		test_align(block3.begin, 4);
		printf(" 7 ");
		block_free(block3);

		printf(" 8 ");
		auto block4 = block_alloc(i, 2);
		assert(block4.begin);
		test_align(block4.begin, 2);
		printf(" 9 ");
		block_free(block4);

printf(" 10 ");
		auto block5 = block_alloc(i, 1);
		assert(block5.begin);
		test_align(block5.begin, 1);
		printf(" 11 ");
		block_free(block5);
		printf(" _ \n");
	}
}
