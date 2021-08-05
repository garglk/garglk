/*
Name
  wchar.c - Wide-character functions.
Function
  Provide definitions of wcscpy and wcslen (wide-character copy
  and len functions) for systems that don't have them in the standard
  library.
Notes

Modified
  09/03/00 MJRoberts - Creation
*/

#include <stdio.h>
#include <stdlib.h>

#include "wchar.h"

/*
 *   wcscpy - this is simply a wide character version of strcpy 
 */
wchar_t *wcscpy(wchar_t *dst, const wchar_t *src)
{
    wchar_t *ret;

    /* save the destination pointer as the return value */
    ret = dst;

    /* copy characters from src to dst until we reach a null */
    for ( ; *src != L'\0' ; *dst++ = *src++) ;

    /* add the terminating null to the output string */
    *dst = L'\0';

    /* return the original output buffer pointer */
    return ret;
}

/*
 *   wcslen - this is a wide character version of strlen 
 */
size_t wcslen(const wchar_t *wstr)
{
    size_t cnt;

    /* count the characters until we encounter a null */
    for (cnt = 0 ; *wstr != L'\0' ; ++cnt, ++wstr) ;

    /* return the number of characters we counted */
    return cnt;
}

