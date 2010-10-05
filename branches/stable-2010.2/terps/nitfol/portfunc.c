/*  Various functions for people who are lacking/have a poor libc
    Copyright (C) 1999  Evin Robertson.  The last part Copyright (C) FSF

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

    The author can be reached at nitfol@deja.com
*/


#include <stdlib.h>            /* For malloc */
#include <limits.h>            /* For LONG_MAX, LONG_MIN, ... */
#include <ctype.h>             /* For isspace, isdigit, isalpha */
#include "nitfol.h"

/* Nitfol malloc/realloc wrappers - never return NULL */
void *n_malloc(int size)
{
  if(size != 0) {
    void *m = malloc(size);
    if(m != NULL)
      return m;
    while(free_undo()) {
      m = malloc(size);
      if(m)
	return m;
    }
    n_show_fatal(E_MEMORY, "not enough memory for malloc", size);
  } else {
    n_show_fatal(E_MEMORY, "malloc(0)", size);
  }

  glk_exit();
  return NULL;
}

void *n_calloc(int nmemb, int size)
{
  int totalsize = nmemb * size;
  void *m = n_malloc(totalsize);
  n_memset(m, 0, totalsize);
  return m;
}

void *n_realloc(void *ptr, int size)
{
  void *m;
  if(size == 0) {
    n_free(ptr);
    return NULL;
  }
  m = realloc(ptr, size);
  if(m != NULL || size == 0)
    return m;
  while(free_undo()) {
    m = realloc(ptr, size);
    if(m)
      return m;
  }
  n_free(ptr);

  glk_exit();
  return NULL;
}

void n_free(void *ptr)
{
  free(ptr);
}

typedef struct rmmalloc_entry rmmalloc_entry;

struct rmmalloc_entry {
  rmmalloc_entry *next;
  void *data;
};

static rmmalloc_entry *rmmalloc_list = NULL;

/* This malloc maintains a list of malloced data, which can all be freed at
   once */
void *n_rmmalloc(int size)
{
  rmmalloc_entry newentry;
  newentry.data = n_malloc(size);
  LEadd(rmmalloc_list, newentry);
  return newentry.data;
}

void n_rmfree(void)
{
  rmmalloc_entry *p;
  for(p=rmmalloc_list; p; p=p->next)
    n_free(p->data);
  LEdestroy(rmmalloc_list);  
}

void n_rmfreeone(void *m)
{
  rmmalloc_entry *p, *t;
  LEsearchremove(rmmalloc_list, p, t, p->data == m, n_free(p->data));
}


/* Returns true if target is a null-terminated string identical to the
   first len characters of starting */
BOOL n_strmatch(const char *target, const char *starting, unsigned len)
{
  if(target &&
     n_strlen(target) == len &&
     n_strncasecmp(target, starting, len) == 0)
    return TRUE;
  return FALSE;
}

/* Write 'n' in decimal to 'dest' - assume there is enough space in buffer */
int n_to_decimal(char *buffer, unsigned n)
{
  int i = 0;
  if(n == 0) {
    buffer[0] = '0';
    return 1;
  } 
  while(n) {
    unsigned c = n % 10;
    buffer[i++] = '0' + c;
    n = (n - c) / 10;
    if(i >= 12)
      return i;
  }
  return i;
}

const char *n_static_number(const char *preface, glui32 n)
{
  static char *buffer = NULL;
  char number[12];
  int preflen = n_strlen(preface);
  int numlen;
  int i;

  buffer = (char *) n_realloc(buffer, preflen + 12 + 2);
  n_strcpy(buffer, preface);
  numlen = n_to_decimal(number, n);
  for(i = 0; i < numlen; i++)
    buffer[preflen + i] = number[numlen - i - 1];
  buffer[preflen + i] = 0;
  return buffer;
}

/* n_strdup(NULL) works, unlike strdup(NULL) which segfaults */
char *n_strdup(const char *s)
{
  char *n;
  if(s == NULL)
    return NULL;
  n = (char *) n_malloc(n_strlen(s) + 1);
  n_strcpy(n, s);
  return n;
}

/* Swap n bytes between a and b */
void n_memswap(void *a, void *b, int n)
{
  int i;
  unsigned char *c = (unsigned char *) a;
  unsigned char *d = (unsigned char *) b;
  unsigned char t;

  for(i = 0; i < n; i++) {
    t = d[i];
    d[i] = c[i];
    c[i] = t;
  }
}

/* Wrappers to hide ugliness of Glk file opening functions */
strid_t n_file_prompt(glui32 usage, glui32 fmode)
{
  frefid_t r = glk_fileref_create_by_prompt(usage, fmode, 0);
  if(r) {
    strid_t s;
    if((fmode & filemode_Read) && !glk_fileref_does_file_exist(r))
      return NULL;
    s = glk_stream_open_file(r, fmode, 0);
    glk_fileref_destroy(r);
    return s;
  }
  return NULL;
}

strid_t n_file_name(glui32 usage, glui32 fmode, const char *name)
{
  frefid_t r = glk_fileref_create_by_name(usage, (char *) name, 0);
  if(r) {
    strid_t s;
    if((fmode & filemode_Read) && !glk_fileref_does_file_exist(r))
	return NULL;
    s = glk_stream_open_file(r, fmode, 0);
    glk_fileref_destroy(r);
    return s;
  }
  return NULL;
}

/* If given name is more than whitespace, open by name; else by prompt */
strid_t n_file_name_or_prompt(glui32 usage, glui32 fmode, const char *name)
{
  const char *c;
  for(c = name; *c; c++) {
    if(*c != ' ')
      return n_file_name(usage, fmode, name);
  }
  return n_file_prompt(usage, fmode);
}



/* Trivial wrappers to fix implicit (const char *) to (char *) cast warnings */
void w_glk_put_string(const char *s)
{
  glk_put_string((char *) s);
}

void w_glk_put_string_stream(strid_t str, const char *s)
{
  glk_put_string_stream(str, (char *) s);
}

void w_glk_put_buffer(const char *buf, glui32 len)
{
  glk_put_buffer((char *) buf, len);
}

void w_glk_put_buffer_stream(strid_t str, const char *buf, glui32 len)
{
  glk_put_buffer_stream(str, (char *) buf, len);
}

void w_glk_put_char(int ch)
{
  glk_put_char(ch);
}



/* The rest of the functions in this conform to ANSI/BSD/POSIX/whatever and
   can be replaced with standard functions with appropriate #defines
   They are included here only for systems lacking a proper libc.  No effort
   has been made to tune their speeds. */

#ifdef HEADER

#ifndef NULL
#define NULL 0
#endif

/* FIXME: use autoconf for these someday */

#ifndef NO_LIBC
/* ISO 9899 functions */
#include <string.h>
#define n_strlen(s)        strlen(s)
#define n_strcpy(d, s)     strcpy(d, s)
#define n_strncpy(d, s, n) strncpy(d, s, n)
#define n_memcpy(d, s, n)  memcpy(d, s, n)
#define n_memmove(d, s, n) memmove(d, s, n)
#define n_memset(s, c, n)  memset(s, c, n)
#define n_strcmp(a, b)     strcmp(a, b)
#define n_strncmp(a, b, n) strncmp(a, b, n)
#define n_memcmp(a, b, n)  memcmp(a, b, n)
#define n_strchr(a, c)     strchr(a, c)
#define n_strcat(d, s)     strcat(d, s)
#define n_strpbrk(s, a)    strpbrk(s, a)
#define n_strspn(s, a)     strspn(s, a)
#define n_strtok(s, d)     strtok(s, d)
#define n_strstr(h, n)     strstr(h, n)
#define n_strtol(n, e, b)  strtol(n, e, b)
#define n_strrchr(s, c)    strrchr(s, c)

#include <stdlib.h>
#define n_qsort(b, n, s, c) qsort(b, n, s, c)
#define n_bsearch(k, b, n, s, c) bsearch(k, b, n, s, c)
#endif

#if defined(__USE_BSD) || defined(__USE_GNU)
#define n_strcasecmp(a, b) strcasecmp(a, b)
#define n_strncasecmp(a, b, n) strncasecmp(a, b, n)
#endif

#ifdef __USE_GNU
#define n_lfind(k, b, n, s, c) lfind(k, b, n, s, c)
#endif

#endif /* HEADER */


#ifndef n_strlen
unsigned n_strlen(const char *s)
{
  int i = 0;
  while(*s++)
    i++;
  return i;
}
#endif

#ifndef n_strcpy
char *n_strcpy(char *dest, const char *src)
{
  while(*src) {
    *dest++ = *src++;
  }
  *dest = 0;
  return dest;
}
#endif

#ifndef n_strncpy
char *n_strncpy(char *dest, const char *src, int len)
{
  while(*src && len) {
    *dest++ = *src++;
    len--;
  }
  while(len--)
    *dest++ = 0;
  return dest;
}
#endif

#ifndef n_memcpy
void *n_memcpy(void *dest, const void *src, int n)
{
  int i;
  unsigned char *a = (unsigned char *) dest;
  unsigned const char *b = (const unsigned char *) src;
  for(i = 0; i < n; i++)
    a[i] = b[i];
  return dest;
}
#endif

#ifndef n_memmove
void *n_memmove(void *dest, const void *src, int n)
{
  int i;
  unsigned char *a = (unsigned char *) dest;
  unsigned char *b = (unsigned char *) src;
  if(a < b)
    for(i = 0; i < n; i++)
      a[i] = b[i];
  else
    for(i = n-1; i >= 0; i--)
      a[i] = b[i];
  return a;
}
#endif

#ifndef n_memset
void *n_memset(void *s, int c, int n)
{
  int i;
  unsigned char *a = (unsigned char *) s;
  for(i = 0; i < n; i++)
    a[i] = c;
  return s;
}
#endif

#ifndef n_strcmp
int n_strcmp(const char *a, const char *b)
{
  for(;;) {
    if(*a != *b)
      return *a - *b;
    if(*a == 0)
      break;
    a++; b++;
  }
  return 0;
}
#endif

#ifndef n_strncmp
int n_strncmp(const char *a, const char *b, int n)
{
  for(; n; n--) {
    if(*a != *b)
      return *a - *b;
    if(*a == 0)
      break;
    a++; b++;
  }
  return 0;
}
#endif

#ifndef n_memcmp
int n_memcmp(const void *s1, const void *s2, int n)
{
  const unsigned char *a = (unsigned char *) s1;
  const unsigned char *b = (unsigned char *) s2;
  for(; n; n--) {
    if(*a != *b)
      return *a - *b;
    a++; b++;
  }
  return 0;
}
#endif

#ifndef n_strcasecmp
int n_strcasecmp(const char *a, const char *b)
{
  for(;;)
  {
    if(*a != *b) {
      char c1 = glk_char_to_lower(*a);
      char c2 = glk_char_to_lower(*b);
      if(c1 != c2)
	return c1 - c2;
    }
    if(*a == 0)
      break;
    a++; b++;
  }
  return 0;
}
#endif

#ifndef n_strncasecmp
int n_strncasecmp(const char *a, const char *b, int n)
{
  for(; n; n--)
  {
    if(*a != *b) {
      char c1 = glk_char_to_lower(*a);
      char c2 = glk_char_to_lower(*b);
      if(c1 != c2)
	return c1 - c2;
    }
    if(*a == 0)
      break;
    a++; b++;
  }
  return 0;
}
#endif

#ifndef n_strlower
char *n_strlower(char *s)
{
  char *b = s;
  while(*b) {
    *b = glk_char_to_lower(*b);
    b++;
  }
  return s;
}
#endif

#ifndef n_strupper
char *n_strupper(char *s)
{
  char *b = s;
  while(*b) {
    *b = glk_char_to_upper(*b);
    b++;
  }
  return s;
}
#endif

#ifndef n_strchr
char *n_strchr(const char *s, int c)
{
  const unsigned char *a = (const unsigned char *) s;
  while(*a != c) {
    if(*a == 0)
      return NULL;
    a++;
  }
  return (char *) a;
}
#endif

#ifndef n_lfind
void *n_lfind(const void *key, const void *base, int *nmemb, int size,
	      int (*compar)(const void *, const void *))
{
  int i;
  char *t = (char *) base;
  for(i = 0; i < *nmemb; i++) {
    if((*compar)(t, key) == 0)
      return (void *) t;
    t += size;
  }
  return NULL;
}
#endif


#ifndef n_qsort

/* Modified by Evin Robertson for nitfolness Aug 4 1999 */

/******************************************************************/
/* qsort.c  --  Non-Recursive ANSI Quicksort function             */
/*                                                                */
/* Public domain by Raymond Gardner, Englewood CO  February 1991  */
/*                                                                */
/* Usage:                                                         */
/*     qsort(base, nbr_elements, width_bytes, compare_function);  */
/*        void *base;                                             */
/*        size_t nbr_elements, width_bytes;                       */
/*        int (*compare_function)(const void *, const void *);    */
/*                                                                */
/* Sorts an array starting at base, of length nbr_elements, each  */
/* element of size width_bytes, ordered via compare_function,     */
/* which is called as  (*compare_function)(ptr_to_element1,       */
/* ptr_to_element2) and returns < 0 if element1 < element2,       */
/* 0 if element1 = element2, > 0 if element1 > element2.          */
/* Most refinements are due to R. Sedgewick. See "Implementing    */
/* Quicksort Programs", Comm. ACM, Oct. 1978, and Corrigendum,    */
/* Comm. ACM, June 1979.                                          */
/******************************************************************/

/* prototypes */
static void swap_chars(char *, char *, int);

#define  SWAP(a, b)  (swap_chars((char *)(a), (char *)(b), size))


#define  COMP(a, b)  ((*comp)((void *)(a), (void *)(b)))

#define  T           7    /* subfiles of T or fewer elements will */
                          /* be sorted by a simple insertion sort */
                          /* Note!  T must be at least 3          */

void n_qsort(void *basep, int nelems, int size,
                            int (*comp)(const void *, const void *))
{
   char *stack[40], **sp;       /* stack and stack pointer        */
   char *i, *j, *limit;         /* scan and limit pointers        */
   int thresh;               /* size of T elements in bytes    */
   char *base;                  /* base pointer as char *         */

   base = (char *)basep;        /* set up char * base pointer     */
   thresh = T * size;           /* init threshold                 */
   sp = stack;                  /* init stack pointer             */
   limit = base + nelems * size;/* pointer past end of array      */
   for ( ;; ) {                 /* repeat until break...          */
      if ( limit - base > thresh ) {  /* if more than T elements  */
                                      /*   swap base with middle  */
         SWAP((((limit-base)/size)/2)*size+base, base);
         i = base + size;             /* i scans left to right    */
         j = limit - size;            /* j scans right to left    */
         if ( COMP(i, j) > 0 )        /* Sedgewick's              */
            SWAP(i, j);               /*    three-element sort    */
         if ( COMP(base, j) > 0 )     /*        sets things up    */
            SWAP(base, j);            /*            so that       */
         if ( COMP(i, base) > 0 )     /*      *i <= *base <= *j   */
            SWAP(i, base);            /* *base is pivot element   */
         for ( ;; ) {                 /* loop until break         */
            do                        /* move i right             */
               i += size;             /*        until *i >= pivot */
            while ( COMP(i, base) < 0 );
            do                        /* move j left              */
               j -= size;             /*        until *j <= pivot */
            while ( COMP(j, base) > 0 );
            if ( i > j )              /* if pointers crossed      */
               break;                 /*     break loop           */
            SWAP(i, j);       /* else swap elements, keep scanning*/
         }
         SWAP(base, j);         /* move pivot into correct place  */
         if ( j - base > limit - i ) {  /* if left subfile larger */
            sp[0] = base;             /* stack left subfile base  */
            sp[1] = j;                /*    and limit             */
            base = i;                 /* sort the right subfile   */
         } else {                     /* else right subfile larger*/
            sp[0] = i;                /* stack right subfile base */
            sp[1] = limit;            /*    and limit             */
            limit = j;                /* sort the left subfile    */
         }
         sp += 2;                     /* increment stack pointer  */
      } else {      /* else subfile is small, use insertion sort  */
         for ( j = base, i = j+size; i < limit; j = i, i += size )
            for ( ; COMP(j, j+size) > 0; j -= size ) {
               SWAP(j, j+size);
               if ( j == base )
                  break;
            }
         if ( sp != stack ) {         /* if any entries on stack  */
            sp -= 2;                  /* pop the base and limit   */
            base = sp[0];
            limit = sp[1];
         } else                       /* else stack empty, done   */
            break;
      }
   }
}

/*
**  swap nbytes between a and b
*/

static void swap_chars(char *a, char *b, int nbytes)
{
   char tmp;
   do {
      tmp = *a; *a++ = *b; *b++ = tmp;
   } while ( --nbytes );
}

#endif




/* These last several were adapted from glibc, GNU LGPLed, copyright FSF */
/* For nitfol, these are licensed under the GPL per section 3 of the LGPL */

#ifndef n_strcat
char *n_strcat(char *dest, const char *src)
{
  char *s1 = dest;
  const char *s2 = src;
  char c;

  /* Find the end of the string.  */
  do
    c = *s1++;
  while (c != '\0');

  /* Make S1 point before the next character, so we can increment
     it while memory is read (wins on pipelined cpus).  */
  s1 -= 2;

  do
    {
      c = *s2++;
      *++s1 = c;
    }
  while (c != '\0');

  return dest;
}
#endif

#ifndef n_strpbrk
char *n_strpbrk(const char *s, const char *accept)
{
  while (*s != '\0')
    if (n_strchr(accept, *s) == NULL)
      ++s;
    else
      return (char *) s;

  return NULL;
}
#endif

#ifndef n_strspn
int n_strspn(const char *s, const char *accept)
{
  const char *p;
  const char *a;
  int count = 0;

  for (p = s; *p != '\0'; ++p)
    {
      for (a = accept; *a != '\0'; ++a)
        if (*p == *a)
          break;
      if (*a == '\0')
        return count;
      else
        ++count;
    }

  return count;
}
#endif

#ifndef n_strtok
char *n_strtok(char *s, const char *delim)
{
  static char *olds = NULL;
  char *token;

  if (s == NULL)
    {
      if (olds == NULL)
	{
	  /* errno = EINVAL; */
	  return NULL;
	}
      else
	s = olds;
    }

  /* Scan leading delimiters.  */
  s += n_strspn(s, delim);
  if (*s == '\0')
    {
      olds = NULL;
      return NULL;
    }

  /* Find the end of the token.  */
  token = s;
  s = n_strpbrk(token, delim);
  if (s == NULL)
    /* This token finishes the string.  */
    olds = NULL;
  else
    {
      /* Terminate the token and make OLDS point past it.  */
      *s = '\0';
      olds = s + 1;
    }
  return token;
}
#endif

#ifndef n_strstr
char *n_strstr(const char *haystack, const char *needle)
{
  const char *needle_end = n_strchr(needle, '\0');
  const char *haystack_end = n_strchr(haystack, '\0');
  const unsigned needle_len = needle_end - needle;
  const unsigned needle_last = needle_len - 1;
  const char *begin;

  if (needle_len == 0)
    return (char *) haystack;	/* ANSI 4.11.5.7, line 25.  */
  if ((size_t) (haystack_end - haystack) < needle_len)
    return NULL;

  for (begin = &haystack[needle_last]; begin < haystack_end; ++begin)
    {
      const char *n = &needle[needle_last];
      const char *h = begin;

      do
	if (*h != *n)
	  goto loop;		/* continue for loop */
      while (--n >= needle && --h >= haystack);

      return (char *) h;

    loop:;
    }

  return NULL;
}
#endif

#ifndef n_strtol
long int n_strtol (const char *nptr, char **endptr, int base)
{
  int negative;
  unsigned long int cutoff;
  unsigned int cutlim;
  unsigned long int i;
  const char *s;
  unsigned char c;
  const char *save;
  int overflow;

  if (base < 0 || base == 1 || base > 36)
    base = 10;

  s = nptr;

  /* Skip white space.  */
  while (isspace (*s))
    ++s;
  if (*s == '\0')
    goto noconv;

  /* Check for a sign.  */
  if (*s == '-')
    {
      negative = 1;
      ++s;
    }
  else if (*s == '+')
    {
      negative = 0;
      ++s;
    }
  else
    negative = 0;

  if (base == 16 && s[0] == '0' && glk_char_to_upper (s[1]) == 'X')
    s += 2;

  /* If BASE is zero, figure it out ourselves.  */
  if (base == 0)
    if (*s == '0')
      {
	if (glk_char_to_upper (s[1]) == 'X')
	  {
	    s += 2;
	    base = 16;
	  }
	else
	  base = 8;
      }
    else
      base = 10;

  /* Save the pointer so we can check later if anything happened.  */
  save = s;

  cutoff = ULONG_MAX / (unsigned long int) base;
  cutlim = ULONG_MAX % (unsigned long int) base;

  overflow = 0;
  i = 0;
  for (c = *s; c != '\0'; c = *++s)
    {
      if (isdigit (c))
	c -= '0';
      else if (isalpha (c))
	c = glk_char_to_upper (c) - 'A' + 10;
      else
	break;
      if (c >= base)
	break;
      /* Check for overflow.  */
      if (i > cutoff || (i == cutoff && c > cutlim))
	overflow = 1;
      else
	{
	  i *= (unsigned long int) base;
	  i += c;
	}
    }

  /* Check if anything actually happened.  */
  if (s == save)
    goto noconv;

  /* Store in ENDPTR the address of one character
     past the last character we converted.  */
  if (endptr != NULL)
    *endptr = (char *) s;

  /* Check for a value that is within the range of
     `unsigned long int', but outside the range of `long int'.  */
  if (i > (negative ?
	   -(unsigned long int) LONG_MIN : (unsigned long int) LONG_MAX))
    overflow = 1;

  if (overflow)
    {
      /* errno = ERANGE; */
      return negative ? LONG_MIN : LONG_MAX;
    }

  /* Return the result of the appropriate sign.  */
  return (negative ? -i : i);

noconv:
  /* There was no number to convert.  */
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}
#endif

#ifndef n_strrchr
char *n_strrchr(const char *s, int c)
{
  const char *found, *p;

  c = (unsigned char) c;

  /* Since strchr is fast, we use it rather than the obvious loop.  */
  
  if (c == '\0')
    return n_strchr(s, '\0');

  found = NULL;
  while ((p = n_strchr(s, c)) != NULL)
    {
      found = p;
      s = p + 1;
    }

  return (char *) found;
}
#endif

#ifndef n_bsearch
void *n_bsearch(const void *key, const void *base, int nmemb, int size,
		int (*compar)(const void *, const void *))
{
  int l, u, idx;
  void *p;
  int comparison;

  l = 0;
  u = nmemb;
  while (l < u)
    {
      idx = (l + u) / 2;
      p = (void *) (((const char *) base) + (idx * size));
      comparison = (*compar)(key, p);
      if (comparison < 0)
	u = idx;
      else if (comparison > 0)
	l = idx + 1;
      else
	return (void *) p;
    }

  return NULL;
}
#endif

