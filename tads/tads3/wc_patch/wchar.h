/*
Name
  wchar.h - Wide-character functions.
Function
  Provide definitions of wcscpy and wcslen (wide-character copy
  and len functions) for systems that don't have them in the standard
  library.
Notes

Modified
  09/03/00 MJRoberts - Creation
*/

#ifndef WCHAR_H
#define WCHAR_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

wchar_t *wcscpy(wchar_t *dst, const wchar_t *src);
size_t   wcslen(const wchar_t *wstr);

#ifdef __cplusplus
}
#endif

#endif /* WCHAR_H */

