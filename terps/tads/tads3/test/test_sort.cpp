#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_sort.cpp - sorting test
Function
  
Notes
  
Modified
  05/14/00 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "t3std.h"
#include "vmglob.h"
#include "vmsort.h"
#include "t3test.h"

class CVmQSortInt: public CVmQSortData
{
public:
    CVmQSortInt() { arr_ = 0; }
    CVmQSortInt(int *arr) { arr_ = arr; }
    int compare(VMG_ size_t a, size_t b) { return arr_[a] - arr_[b]; }
    void exchange(VMG_ size_t a, size_t b)
    {
        int tmp = arr_[a];
        arr_[a] = arr_[b];
        arr_[b] = tmp;
    }

    int *arr_;
};

class CVmQSortStr: public CVmQSortData
{
public:
    CVmQSortStr() { arr_ = 0; }
    CVmQSortStr(const char **arr) { arr_ = arr; }
    int compare(VMG_ size_t a, size_t b)
        { return strcmp(arr_[a], arr_[b]); }
    void exchange(VMG_ size_t a, size_t b)
    {
        const char *tmp = arr_[a];
        arr_[a] = arr_[b];
        arr_[b] = tmp;
    }

    const char **arr_;
};

int main()
{
    static int ints1[] = { 1, 5, 2, 10, 2, 7, 4, 3, 9 };
    static int ints2[] = { 1, 5, 6, 10, 2, 7, 4, 3, 9, 8 };
    static int ints3[] = { 11 };
    static int ints4[] = { 9, 1 };
    static int ints5[] = { 9, 1, 3 };
    static int ints6[] = { 17, 17, 17, 17, 1 };
    static const char *strs[] =
    {
        "hello",
        "there",
        "how",
        "are",
        "you",
        "today"
    };
    vm_globals *vmg__ = 0;
    CVmQSortInt int_sorter;
    CVmQSortStr str_sorter;
    int i;

    /* initialize for testing */
    test_init();

    int_sorter.arr_ = ints1;
    int_sorter.sort(vmg_ 0, countof(ints1) - 1);
    printf("ints1:\n");
    for (i = 0 ; i < countof(ints1) ; ++i)
        printf("   %d\n", ints1[i]);

    int_sorter.arr_ = ints2;
    int_sorter.sort(vmg_ 0, countof(ints2) - 1);
    printf("ints2:\n");
    for (i = 0 ; i < countof(ints2) ; ++i)
        printf("   %d\n", ints2[i]);

    int_sorter.arr_ = ints3;
    int_sorter.sort(vmg_ 0, countof(ints3) - 1);
    printf("ints3:\n");
    for (i = 0 ; i < countof(ints3) ; ++i)
        printf("   %d\n", ints3[i]);

    int_sorter.arr_ = ints4;
    int_sorter.sort(vmg_ 0, countof(ints4) - 1);
    printf("ints4:\n");
    for (i = 0 ; i < countof(ints4) ; ++i)
        printf("   %d\n", ints4[i]);

    int_sorter.arr_ = ints5;
    int_sorter.sort(vmg_ 0, countof(ints5) - 1);
    printf("ints5:\n");
    for (i = 0 ; i < countof(ints5) ; ++i)
        printf("   %d\n", ints5[i]);

    int_sorter.arr_ = ints6;
    int_sorter.sort(vmg_ 0, countof(ints6) - 1);
    printf("ints6:\n");
    for (i = 0 ; i < countof(ints6) ; ++i)
        printf("   %d\n", ints6[i]);

    str_sorter.arr_ = (const char **)strs;
    str_sorter.sort(vmg_ 0, countof(strs) - 1);
    printf("strs:\n");
    for (i = 0 ; i < countof(strs) ; ++i)
        printf("   %s\n", strs[i]);

    return 0;
}


