#include <stdio.h>
#include <stdlib.h>

int f(int x, int y) { printf("f(%d)\n", x); return y; }

void main(int argc, char **argv)
{
    int a;
    
    a = f(1, 0) ? f(2, 1) ? f(3, 1) : f(4, 1) : f(5, 1);
    printf("\n");
    a = f(1, 1) ? f(2, 1) ? f(3, 1) : f(4, 1) : f(5, 1);
    printf("\n");
    a = f(1, 1) ? f(2, 1) : f(3, 1) ? f(4, 1) : f(5, 1);
    printf("\n");
    a = f(1, 0) ? f(2, 1) : f(3, 1) ? f(4, 1) : f(5, 1);
}
