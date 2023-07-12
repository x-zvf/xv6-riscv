#include "user/user.h"

void baz(int z) {
    int i,j,k;
    (void) i;
    (void) j;
    (void) k;
    
    if(z > 0) {
        baz(z - 1);
        return;
    }
    printf("Backtracing now from baz:\n");
    backtrace();
}

void bar() {
    int i;
    (void) i;
    baz(3);
}

void foo() {
    int i,j,k,l,m;
    (void) i;
    (void) j;
    (void) k;
    (void) l;
    (void) m;
    bar();
}

int main() {
    int i,j;
    (void) i;
    (void) j;
    foo();
    return 0;
}