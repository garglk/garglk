#include <stdio.h>

void main()
{
    int v[] = { 10, 20, 30, 40 };
    int i = 0;

    printf("before: i=%d, v=%d,%d,%d,%d\n", i, v[0], v[1], v[2], v[3]);
    v[i++]++;
    printf("after: i=%d, v=%d,%d,%d,%d\n", i, v[0], v[1], v[2], v[3]);
}
