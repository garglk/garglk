#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/test/test_utf8.cpp,v 1.2 1999/05/17 02:52:31 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_utf8.cpp - test for UTF8 class
Function
  
Notes
  
Modified
  10/16/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <stdio.h>

#include "utf8.h"
#include "t3test.h"

int main()
{
    static wchar_t test1[] =
    {
        0x0010, 0x0020, 0x0030, 0x0040, 0x0050, 0x0060, 0x0070, 0x007f,
        0x0080, 0x0100, 0x0200, 0x0400, 0x0500, 0x0600, 0x0700, 0x07ff,
        0x0800, 0x1000, 0x2000, 0x4000, 0x8000, 0xA000, 0xF000, 0xFFFF,
        0
    };
    wchar_t *wp;
    char buf[256];
    utf8_ptr p;
    size_t len;
    size_t i;
    wchar_t ch;

    /* initialize for testing */
    test_init();

    /* encode the test string */
    p.set(buf);
    p.setwcharsz(test1, sizeof(buf));

    /* go back to the start of the buffer */
    p.set(buf);

    /* compute the length */
    len = p.lenz();
    printf("test1: len = %lu - %s\n",
           (unsigned long)len,
           (len == sizeof(test1)/sizeof(test1[0]) - 1) ? "ok" : "ERROR");

    /* 
     *   run through the string, decode characters, and compare the
     *   results to the original 
     */
    printf("-- incrementing --\n");
    for (i = 0, wp = test1 ; i < len ; p.inc(), ++wp, ++i)
    {
        /* decode the current character */
        ch = p.getch();

        /* check it */
        printf("ch[%lu] = %04x - %s\n",
               (unsigned long)i, ch, (ch == *wp ? "ok" : "ERROR"));
    }

    /* go backwards through the buffer */
    printf("-- decrementing --\n");
    do
    {
        /* move back one character */
        --wp;
        p.dec();
        --i;

        /* decode the current character */
        ch = p.getch();

        /* check it */
        printf("ch[%lu] = %04x - %s\n",
               (unsigned long)i, ch, (ch == *wp ? "ok" : "ERROR"));
    } while (i != 0);

    /* success */
    return 0;
}

