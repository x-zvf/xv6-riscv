/*!
 * \file
 * \brief test general mmap
 */


#include <rt-test/assert.h>
#include <user/mmap.h>
#include <user/bmalloc.h>

// void main() {
// 	for (int i = 1; i < 64; ++i) {
// 		auto size = i * 4096;
// 		void *data = mmap(0, i * size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
// 		assert(data != MAP_FAILED);
// 		auto ptr = reinterpret_cast<volatile char*>(data);
// 		for (int j = 0; j < size; ++j) {
// 			ptr[j] = j;
// 			assert(ptr[j] == char(j));
// 		}
// 		assert(!munmap(data, size));
// 	}
// }

void main() {
    printf("Large malloc:");
    int *ptr = (int *)malloc(4096 * 64);
    *ptr = 0x12345678;
    printf(" wrote to addr. freeing\n");
    free(ptr);

    // for(int i = 0; i < (1 << 16); i++) {
    //     auto block2 = block_alloc(2048, 8);
    //     if(block2.begin == 0)
    //         printf("failed\n");
    //     //printf("block2: %p\n", block2.begin);
    //     ((uint32 *)block2.begin)[0] = 0x12345678;
    // }
    // term();
}