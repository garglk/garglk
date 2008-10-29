/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_regex.cpp - regular expression parser tester
Function
  Simple interactive regular expression tester.  Asks for patterns and
  strings to test from the keyboard and displays the results.
Notes
  
Modified
  11/11/01 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>

#include "vmregex.h"
#include "t3test.h"

int main()
{
    CRegexParser regex;
    CRegexSearcherSimple searcher(&regex);

    /* initialize for testing */
    test_init();

    /* read patterns and match strings interactively */
    for (;;)
    {
        char pat[128];
        char str[128];
        size_t len;

        /* read the pattern - stop on EOF */
        printf("Enter pattern: ");
        if (fgets(pat, sizeof(pat), stdin) == 0)
            break;

        /* trim the trailing newline, if any */
        if ((len = strlen(pat)) != 0 && pat[len-1] == '\n')
            pat[len-1] = '\0';

        /* read strings to match against the pattern */
        for (;;)
        {
            int idx;
            int reslen;

            /* read the string - stop on EOF or an empty line */
            printf("Enter string:  ");
            if (fgets(str, sizeof(str), stdin) == 0
                || str[0] == '\0'
                || str[0] == '\n')
                break;

            /* trim the trailing newline, if any */
            if ((len = strlen(str)) != 0 && str[len-1] == '\n')
                str[len-1] = '\0';

            /* match it */
            idx = searcher.compile_and_search(pat, strlen(pat),
                                              str, str, strlen(str), &reslen);

            /* report the results */
            if (idx == -1)
                printf("[Not found]\n");
            else
                printf("Found: index=%d, %.*s\n", idx, reslen, str + idx);
        }
    }

    return 0;
}
