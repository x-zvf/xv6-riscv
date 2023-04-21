#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int res = nsc(12, 10);
    printf("nsc = %d\n", res);
    return 0;
}