#include "user/user.h"

int main() {
	printf("Testing basic mmap:\n");
    char *addr = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    addr[0] = 0xf1;
    addr[2] = addr[3];
    printf("mapping writable\n Unmapping: \n");
    munmap(addr, 4096);
    printf("unmapped. Unmapping invalid data:\n");
    munmap((char *)0, 1 << 28);
    printf("unmapped. Mapping multiple pages\n");

    char *multi_page = mmap(0, 4096 * 10, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    printf("mapped. testing writabiliy\n");
    for(int i = 0; i < 4096 * 10; i++) {
    	multi_page[i] = 0xf1;
    }
    printf("wrote. unmapping middle pages\n");
    munmap(multi_page + 4096 * 2, 4096 * 7);
    printf("unmapped. testing writability\n");
    multi_page[0] = 0xf1;
    multi_page[4096 * 9] = 0xf1;
    printf("wrote. unmapping first page\n");
    munmap(multi_page, 4096);
    printf("unmapped. testing writability\n");
    multi_page[4096 * 9] = 0xf1;
    printf("wrote. unmapping last page\n");
    munmap(multi_page + 4096 * 9, 4096);
    printf("unmapped. testing writability\n");
    multi_page[4096 + 10] = 0xf1;
    printf("wrote\n Mmapping multiple pages again\n");
    multi_page = mmap(0, 4096 * 10, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    printf("mapped. testing writability\n");
    multi_page[0] = 0xf1;
    multi_page[4096 * 9] = 0xf1;
    printf("wrote. unmapping all pages\n");
    munmap(multi_page, 4096 * 10);
    printf("Exiting. Testing if system can handle still mapped pages\n");
	return 0;
}