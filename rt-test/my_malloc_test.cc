#include "user/user.h"
#include "kernel/types.h"

/**
 * Stolen from https://github.com/AlbinoBoi/rxv6pp
*/

int main(int argc, char **) {
  int i;
  void *ps[260];

  printf("testing allocations... ");
  for (i = 0; i < 260; i++) {
    void *p = malloc(i + 1);
    ps[i]   = p;
  }
  printf("OK\n");

  printf("testing freeing... ");
  for (i = 0; i < 260; i++) { free(ps[i]); }
  printf("OK\n");

  i = 0;
  printf("An error is expected to occur here:\n");
  while (1) {
    void *ptr = malloc(1);
    if (!ptr) break;
    i++;
  }
  printf("got %d allocations for 1B\n", i);

  return 0;
}
