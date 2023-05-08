#include "user/user.h"

int main() {
	printf("Testing mmap:\n");
    char *addr = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    addr[0] = 0xf1;
    addr[2] = addr[3];
    munmap(addr, 4096);
    munmap((char *)0, 1 << 28);
	return 0;
}