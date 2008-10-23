/* 
 *   Copyright (c) 1998 by Christopher Nebel.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name:
  argize.c

Function:
  Break a string into argc/argv-style arguments.

Notes:
  none

Modified:
  <2>07/04/98  CNebel   - Fixed so that args wouldn't pick up trailing 
                          whitespace.
  <1>07/04/98  CNebel   - Created based on older Mac-specific argize source
*/

#include "argize.h"

#include <stddef.h>
#include <ctype.h>


/*
 *   Return what argc would be if this string were fed to argize.  
 */
int countargs(const char *cmdline)
{
    const char *p = cmdline;
    int                     state = 0;
    int                     argc = 0;

    while (*p) {
        switch (state) {
        case 0:                                 /* skip leading white space */
            while (isspace((unsigned char)*p)) p++;
            if (!*p) break;            /* If there's nothing left, get out. */
            state = 1;
            /* fall through */;

        case 1:                                                /* begin arg */
            argc++;
            state = 2;
            /* fall through */

        case 2:                            /* collect chars for current arg */
            if (isspace((unsigned char)*p))
                p++, state = 0;                       /* Terminate this arg */
            else if (*p == '"')
                p++, state = 3;                              /* Open quote. */
            else if (*p == '\\')
                p++, state = 4;                         /* Escape sequence. */
            else
                p++;                                    /* Just keep going. */

            break;

        case 3:                             /* collect chars inside quotes. */
            switch (*p) {
            case '"' : p++; state = 2; break;                /* Close quote */
            case '\\': p++; state = 5; break;           /* Escape sequence. */
            default:   p++; break;
            }
            break;

        case 4:                        /* escaped character outside quotes. */
            p++; state = 2;
            break;

        case 5:                         /* escaped character inside quotes. */
            p++; state = 3;
            break;
        }
    }

    return argc;
}


/*
 *   Break a string <cmdline> into argc/argv arguments, removing quotes in
 *   the process.  Returns 0 on success, 1 if there were too many
 *   arguments.  
 */
int argize(char *cmdline, int *const argc, char *argv[], const size_t argvlen)
{
    char   *p, *q;
    int             state;

    p = q = cmdline;
    *argc = 0;
    state = 0;

    while (*p) {
        switch (state) {
        case 0:                                 /* skip leading white space */
            while (isspace((unsigned char)*p)) p++, q++;
            if (!*p) break;            /* If there's nothing left, get out. */
            state = 1;
            /* fall through */;

        case 1:                                                /* begin arg */
            if ((size_t)*argc < argvlen)
                argv[(*argc)++] = q;
            else
                return 1;
            state = 2;
            /* fall through */

        case 2:                            /* collect chars for current arg */
            if (isspace((unsigned char)*p))
                *q++ = *p++ = '\0', state = 0;        /* Terminate this arg */
            else if (*p == '"')
                p++, state = 3;                              /* Open quote. */
            else if (*p == '\\')
                p++, state = 4;                         /* Escape sequence. */
            else
                *q++ = *p++;                            /* Just keep going. */

            break;

        case 3:                             /* collect chars inside quotes. */
            switch (*p) {
            case '"' : p++; state = 2; break;                /* Close quote.*/
            case '\\': p++; state = 5; break;           /* Escape sequence. */
            default:   *q++ = *p++; break;
            }
            break;

        case 4:                        /* escaped character outside quotes. */
            *q++ = *p++; state = 2;
            break;

        case 5:                         /* escaped character inside quotes. */
            *q++ = *p++; state = 3;
            break;
        }
    }
    *q = '\0';
    return 0;
}
