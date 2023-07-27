#include "user/user.h"

void baz(int z) {
  volatile int i = 0, j = 1, k = 2;
  (void)i;
  (void)j;
  (void)k;

  if (z > 0) {
    baz(z - 1);
    return;
  }
  printf("Backtracing now from baz:\n");
  backtrace();
}

void bar() {
  volatile int i = 0;
  (void)i;
  baz(3);
}

void foo() {
  volatile int i = 0, j = 1, k = 2, l = 3, m = 4;
  (void)i;
  (void)j;
  (void)k;
  (void)l;
  (void)m;
  bar();
}

int main() {
  volatile int i = 0, j = 1;
  (void)i;
  (void)j;
  foo();
  return 0;
}