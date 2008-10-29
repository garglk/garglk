/*
$Header: d:/cvsroot/tads/TADS2/LIB.H,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  lib.h - standard definitions
Function
  Various standard definitions
Notes
  None
Modified
  01/02/93 SMcAdams - add bit operations
  12/30/92 MJRoberts     - converted to lib.h (from TADS std.h)
  08/03/91 MJRoberts     - creation
*/

#ifndef LIB_INCLUDED
#define LIB_INCLUDED

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifndef OS_INCLUDED
# include "os.h"
#endif

/* short-hand for various types */
#ifndef OS_UCHAR_DEFINED
typedef unsigned char  uchar;
#endif

#ifndef OS_USHORT_DEFINED
typedef unsigned short ushort;
#endif

#ifndef OS_UINT_DEFINED
typedef unsigned int   uint;
#endif

#ifndef OS_ULONG_DEFINED
typedef unsigned long  ulong;
#endif

/* some alternative types */
typedef unsigned char  ub1;
typedef signed char    sb1;
typedef char           b1;
typedef unsigned int   ub2;
typedef signed int     sb2;
typedef int            b2;
typedef unsigned long  ub4;
typedef signed long    sb4;
typedef long           b4;
typedef int            eword;

/* maximum/minimum portable values for various types */
#define UB4MAXVAL  0xffffffffUL
#define UB2MAXVAL  0xffffU
#define UB1MAXVAL  0xffU
#define SB4MAXVAL  0x7fffffffL
#define SB2MAXVAL  0x7fff
#define SB1MAXVAL  0x7f
#define SB4MINVAL  (-(0x7fffffff)-1)
#define SB2MINVAL  (-(0x7fff)-1)
#define SB1MINVAL  (-(0x7f)-1)

/* clear a struture */
#define CLRSTRUCT(x) memset(&(x), 0, (size_t)sizeof(x))
#define CPSTRUCT(dst,src) memcpy(&(dst), &(src), (size_t)sizeof(dst))

/* TRUE and FALSE */
#ifndef TRUE
# define TRUE 1
#endif /* TRUE */
#ifndef FALSE
# define FALSE 0
#endif /* FALSE */

/* bitwise operations */
#define bit(va, bt) ((va) & (bt))
#define bis(va, bt) ((va) |= (bt))
#define bic(va, bt) ((va) &= ~(bt))

/*
 *   noreg/NOREG - use for variables changed in error-protected code that
 *   are used in error handling code.  Use 'noreg' on the declaration like
 *   a storage class qualifier.  Use 'NOREG' as an argument call, passing
 *   the addresses of all variables declared noreg.  For non-ANSI
 *   compilers, a routine osnoreg(/o_ void *arg0, ... _o/) must be
 *   defined.
 */
#ifdef OSANSI
# define noreg volatile
# define NOREG(arglist)
#else /* OSANSI */
# define noreg
# define NOREG(arglist) osnoreg arglist ;
#endif /* OSANSI */

/* 
 *   Linting directives.  You can define these before including this file
 *   if you have a fussy compiler.  
 */
#ifdef LINT
# ifndef NOTREACHED
#  define NOTREACHED return
# endif
# ifndef NOTREACHEDV
#  define NOTREACHEDV(t) return((t)0)
# endif
# ifndef VARUSED
#  define VARUSED(v) varused(v)
# endif
void varused();
#else /* LINT */
# ifndef NOTREACHED
#  define NOTREACHED
# endif
# ifndef NOTREACHEDV
#  define NOTREACHEDV(t)
# endif
# ifndef VARUSED
#  define VARUSED(v)
# endif
#endif /* LINT */

/* conditionally compile code if debugging is enabled */
#ifdef DEBUG
# define IF_DEBUG(x) x
#else /* DEBUG */
# define IF_DEBUG(x)
#endif /* DEBUG */

#ifndef offsetof
# define offsetof(s_name, m_name) (size_t)&(((s_name *)0)->m_name)
#endif /* offsetof */

/*
 *   Define our own version of isspace(), so that we don't try to interpret
 *   anything outside of the normal ASCII set as spaces. 
 */
#define t_isspace(c) \
    (((unsigned char)(c)) <= 127 && isspace((unsigned char)(c)))

#endif /* LIB_INCLUDED */
