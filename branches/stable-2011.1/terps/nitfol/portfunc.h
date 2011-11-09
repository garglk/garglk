/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i portfunc.c' */
#ifndef CFH_PORTFUNC_H
#define CFH_PORTFUNC_H

/* From `portfunc.c': */
void * n_malloc (int size );
void * n_calloc (int nmemb , int size );
void * n_realloc (void *ptr , int size );
void n_free (void *ptr );
void * n_rmmalloc (int size );
void n_rmfree (void);
void n_rmfreeone (void *m );
BOOL n_strmatch (const char *target , const char *starting , unsigned len );
int n_to_decimal (char *buffer , unsigned n );
const char * n_static_number (const char *preface , glui32 n );
char * n_strdup (const char *s );
void n_memswap (void *a , void *b , int n );
strid_t n_file_prompt (glui32 usage , glui32 fmode );
strid_t n_file_name (glui32 usage , glui32 fmode , const char *name );
strid_t n_file_name_or_prompt (glui32 usage , glui32 fmode , const char *name );
void w_glk_put_string (const char *s );
void w_glk_put_string_stream (strid_t str , const char *s );
void w_glk_put_buffer (const char *buf , glui32 len );
void w_glk_put_buffer_stream (strid_t str , const char *buf , glui32 len );
void w_glk_put_char (int ch );

#ifndef NULL
#define NULL 0

#endif

#ifndef NO_LIBC
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

#ifndef n_strlen
unsigned n_strlen (const char *s );

#endif

#ifndef n_strcpy
char * n_strcpy (char *dest , const char *src );

#endif

#ifndef n_strncpy
char * n_strncpy (char *dest , const char *src , int len );

#endif

#ifndef n_memcpy
void * n_memcpy (void *dest , const void *src , int n );

#endif

#ifndef n_memmove
void * n_memmove (void *dest , const void *src , int n );

#endif

#ifndef n_memset
void * n_memset (void *s , int c , int n );

#endif

#ifndef n_strcmp
int n_strcmp (const char *a , const char *b );

#endif

#ifndef n_strncmp
int n_strncmp (const char *a , const char *b , int n );

#endif

#ifndef n_memcmp
int n_memcmp (const void *s1 , const void *s2 , int n );

#endif

#ifndef n_strcasecmp
int n_strcasecmp (const char *a , const char *b );

#endif

#ifndef n_strncasecmp
int n_strncasecmp (const char *a , const char *b , int n );

#endif

#ifndef n_strlower
char * n_strlower (char *s );

#endif

#ifndef n_strupper
char * n_strupper (char *s );

#endif

#ifndef n_strchr
char * n_strchr (const char *s , int c );

#endif

#ifndef n_lfind
void * n_lfind (const void *key , const void *base , int *nmemb , int size , int ( *compar ) ( const void * , const void * ) );

#endif

#ifndef n_qsort
void n_qsort (void *basep , int nelems , int size , int ( *comp ) ( const void * , const void * ) );

#endif

#ifndef n_strcat
char * n_strcat (char *dest , const char *src );

#endif

#ifndef n_strpbrk
char * n_strpbrk (const char *s , const char *accept );

#endif

#ifndef n_strspn
int n_strspn (const char *s , const char *accept );

#endif

#ifndef n_strtok
char * n_strtok (char *s , const char *delim );

#endif

#ifndef n_strstr
char * n_strstr (const char *haystack , const char *needle );

#endif

#ifndef n_strtol
long int n_strtol (const char *nptr , char **endptr , int base );

#endif

#ifndef n_strrchr
char * n_strrchr (const char *s , int c );

#endif

#ifndef n_bsearch
void * n_bsearch (const void *key , const void *base , int nmemb , int size , int ( *compar ) ( const void * , const void * ) );

#endif

#endif /* CFH_PORTFUNC_H */
