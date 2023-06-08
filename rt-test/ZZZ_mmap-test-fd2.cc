/*!
 * Stolen from https://github.com/AlbinoBoi/rxv6pp
 * \file
 * \brief test that invalid m(un)map calls fail
 */

#include <rt-test/assert.h>
#include <user/user.h>
#include <kernel/fcntl.h>
#include <user/mmap.h>

void main() {
  int fd = open("tes_t.txt", O_CREATE | O_RDWR);
  assert(fd >= 0 || "Invalid fildes");

  char string[]  = "Halo ich mag mmap seeehr dolll";
  char string2[] = "Halo ich mag mmBBBBBBBBBBBBBBB";
  char string3[] = "AAAAAAAAAAAAAAABBBBBBBBBBBBBBB";
  int size       = strlen(string) + 1;
  if (write(fd, string, strlen(string) + 1) != (int)strlen(string) + 1) {
    printf("write failed\n");
    return;
  }

  close(fd);


  int pid = fork();
  if (pid > 0) // parent
  {
    int keine_ahnung;
    if (wait(&keine_ahnung) < 0) {
      printf("HÄ?\n");
      assert(0);
      return;
    }

    fd       = open("tes_t.txt", O_RDONLY);
    void *va = mmap(0, size, PROT_RW, MAP_SHARED, fd, 0);
    assert(strcmp((char *)va, string2) == 0);
    memset(va, 'A', 15);
    assert(strcmp((char *)va, string3) == 0);
    close(fd);

    assert(!unlink("tes_t.txt"));

  } else if (pid == 0) // child
  {
    fd       = open("tes_t.txt", O_RDONLY);
    void *va = mmap(0, size, PROT_RW, MAP_SHARED, fd, 0);
    assert(strcmp((char *)va, string) == 0);

    memset((char *)va + 15, 'B', 15);
    assert(strcmp((char *)va, string2) == 0);
    close(fd);
    exit(0);
  } else {
    printf("dod weil fork\n");
    assert(0);
  }
}
