/*
 *   Test of our custom SHR macros.  This tests that our SHR macros produce
 *   correct treatment of the sign bit for the logical vs arithmetic shifts.
 */

#define OS_CUSTOM_ASHR
#define OS_CUSTOM_LSHR

#include <stdlib.h>
#include <stdio.h>
#include "t3std.h"

#define INT32_BIT (sizeof(int32_t) * CHAR_BIT)

static void native_ashr(int32_t a, int32_t b, int32_t target)
{
    int32_t actual = a >> b;
    printf("signed %ld >> %ld = %ld", a, b, actual);
    if (actual != target)
        printf(" *** Expected %ld ***", target);
    printf("\n");
}

static void native_lshr(int32_t a, int32_t b, int32_t target)
{
    int32_t actual = (int32_t)(((uint32_t)a) >> ((uint32_t)b));
    printf("unsigned %ld >> %ld = %ld", a, b, actual);
    if (actual != target)
        printf(" *** Expected %ld ***", target);
    printf("\n");
}

static void custom_ashr(int32_t a, int32_t b, int32_t target)
{
    int32_t actual = t3_ashr(a, b);
    printf("%ld ASHR %ld = %ld", a, b, actual);
    if (actual != target)
        printf(" *** Expected %ld ***", target);
    printf("\n");
}

static void custom_lshr(int32_t a, int32_t b, int32_t target)
{
    int32_t actual = t3_lshr(a, b);
    printf("%ld LSHR %ld = %ld", a, b, actual);
    if (actual != target)
        printf(" *** Expected %ld ***", target);
    printf("\n");
}

void main()
{
    /* test our ASHR detection macro on this platform */
    printf("ASHR preprocessor test:\n");
#if ((-1 >> 1) != -1)
    printf("Your compiler's signed>>signed does NOT appear to use ASHR, so"
           "\nTADS 3 will use the CUSTOM ASHR code by default.");
#else
    printf("Your compiler's signed>>signed appears to use ASHR, so"
           "\nTADS 3 will use the native >> for ASHR by default.");
#endif

    /* test our LSHR detection macro on this platform */
    printf("\n\nLSHR preprocessor test:\n");
#if ((ULONG_MAX >> 1UL) != ULONG_MAX/2)
    printf("Your compiler's unsigned>>unsigned does NOT appear to use LSHR, so"
           "\nTADS 3 will use the CUSTOM LSHR code by default.");
#else
    printf("Your compiler's unsigned>>unsigned appears to use LSHR, so"
           "\nTADS 3 will use native >> for LSHR by default.");
#endif

    /* test native ASHR on this platform */
    printf("\n\nNative signed >> signed -> ASHR tests:\n");
    native_ashr(0, 1, 0);
    native_ashr(1, 1, 0);
    native_ashr(2, 1, 1);
    native_ashr(LONG_MAX-1, 1, LONG_MAX/2);
    native_ashr(~0, 1, ~0);
    native_ashr(-1, 1, -1);
    native_ashr(-2, 1, -1);
    native_ashr(-3, 1, -2);
    native_ashr(-4, 2, -1);
    native_ashr(LONG_MIN+1, 1, -1073741824);
    native_ashr(LONG_MIN, 1, -1073741824);

    /* test our custom ASHR macro */
    printf("\nCustom ASHR tests:\n");
    custom_ashr(0, 1, 0);
    custom_ashr(1, 1, 0);
    custom_ashr(2, 1, 1);
    custom_ashr(LONG_MAX-1, 1, LONG_MAX/2);
    native_ashr(~0, 1, ~0);
    native_ashr(-1, 1, -1);
    custom_ashr(-2, 1, -1);
    custom_ashr(-3, 1, -2);
    custom_ashr(-4, 2, -1);
    custom_ashr(LONG_MIN+1, 1, LONG_MIN/2);
    custom_ashr(LONG_MIN, 1, LONG_MIN/2);

    /* test native LSHR on this platform */
    printf("\nNative unsigned >> unsigned -> LSHR tests:\n");
    native_lshr(0, 1, 0);
    native_lshr(1, 1, 0);
    native_lshr(2, 1, 1);
    native_lshr(LONG_MAX-1, 1, LONG_MAX/2);
    native_lshr(~0, 1, ~0 & ~(1 << (INT32_BIT-1)));
    native_lshr(LONG_MIN+1, 1, ((ulong)LONG_MAX+1)/2);
    native_lshr(LONG_MIN, 1, ((ulong)LONG_MAX+1)/2);

    /* test our custom LSHR macro */
    printf("\nCustom LSHR tests:\n");
    custom_lshr(0, 1, 0);
    custom_lshr(1, 1, 0);
    custom_lshr(2, 1, 1);
    custom_lshr(LONG_MAX-1, 1, LONG_MAX/2);
    custom_lshr(LONG_MIN+1, 1, ((ulong)LONG_MAX+1)/2);
    custom_lshr(LONG_MIN, 1, ((ulong)LONG_MAX+1)/2);
}
