/*  Nitfol - z-machine interpreter using Glk for output.
    Copyright (C) 1999  Evin Robertson

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
#ifndef NITFOL_H
#define NITFOL_H

#include <stdlib.h>      /* For NULL, rand, srand */
#include <time.h>        /* For time() */
#include <ctype.h>       /* for isspace, isgraph, etc. */
#include <limits.h>
#include "glk.h"
#define GLK_EOF ((glsi32) -1)

#define NITFOL_MAJOR 0
#define NITFOL_MINOR 5

/* Change these next few typedefs depending on your compiler */
#if UCHAR_MAX==0xff
typedef unsigned char zbyte;
#else
#error "Can't find an 8-bit integer type"
#endif

#ifdef FAST_SHORT

#if SHRT_MAX==0x7fff
typedef unsigned short zword;
#elif INT_MAX==0x7fff
typedef unsigned int zword;
#else
#error "Can't find a 16-bit integer type"
#endif

#if INT_MAX==0x7fffffff
typedef unsigned int offset;
#elif LONG_MAX==0x7fffffff
typedef unsigned long offset;
#else
#error "Can't find a 32-bit integer type"
#endif

#ifdef TWOS16SHORT
#define FAST_TWOS16SHORT
#endif

#else

#ifdef FAST_SIGNED
#if INT_MAX==0x7fffffff
typedef int zword;
typedef int offset;
#elif LONG_MAX==0x7fffffff
typedef long zword;
typedef long offset;
#else
#error "Can't find a 32-bit integer type"
#endif

#else

#if INT_MAX==0x7fffffff
typedef unsigned int zword;
typedef unsigned int offset;
#elif LONG_MAX==0x7fffffff
typedef unsigned long zword;
typedef unsigned long offset;
#else
#error "Can't find a 32-bit integer type"
#endif

#endif

#endif

#ifndef BOOL
#define BOOL int
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif
#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

#ifdef PASTE
#undef PASTE
#endif
#ifdef XPASTE
#undef XPASTE
#endif
#define PASTE(a, b) a##b
#define XPASTE(a, b) PASTE(a, b)


#if defined(__cplusplus) || defined(USE_INLINE)
#define N_INLINE inline
#elif defined(INLINE)
#define N_INLINE INLINE
#elif defined(__GNUC__)
#define N_INLINE __inline__
#else
#define N_INLINE
#endif



#ifdef ZVERSION_GRAHAM_9

#define ZWORD_SIZE 4
#define ZWORD_MAX  0x7fffffffL
#define ZWORD_MASK 0xffffffffL
#define ZWORD_WRAP 0x100000000L
#define ZWORD_TOPBITMASK 0x80000000L

#else

#define ZWORD_SIZE 2
#define ZWORD_MAX  0x7fff
#define ZWORD_MASK 0xffff
#define ZWORD_WRAP 0x10000L
#define ZWORD_TOPBITMASK 0x8000

#endif


#ifdef FAST_TWOS16SHORT

#define ARITHMASK(n) (n)
#define is_neg(a)     ((short) (a) < 0)
#define neg(a)       (-((short) (a)))

#else

#define ARITHMASK(n) ((n) & ZWORD_MASK)
#define is_neg(a) ((a) > ZWORD_MAX)
#define neg(a) ((ZWORD_WRAP - (a)) & ZWORD_MASK)

#endif


#ifdef TWOS16SHORT

#define is_greaterthan(a, b) (((short) (a)) > ((short) (b)))
#define is_lessthan(a, b)    (((short) (a)) < ((short) (b)))

#else

#define is_greaterthan(a, b) (((b) - (a)) & ZWORD_TOPBITMASK)
#define is_lessthan(a, b)    (((a) - (b)) & ZWORD_TOPBITMASK)

#endif


#ifdef FAST

#define LOBYTE(p)         z_memory[p]
#define LOBYTEcopy(d, s)  z_memory[d] = LOBYTE(s)
#define LOBYTEwrite(p, n) z_memory[p] = (n)
#define LOWORD(p)         MSBdecodeZ(z_memory + (p))
#define LOWORDcopy(d, s)  BYTEcopyZ(z_memory + (d), z_memory + (s))
#define LOWORDwrite(p, n) MSBencodeZ(z_memory + (p), n)

/* If you have a segmented memory model or need to implement virtual memory,
   you can change the next three lines, and the corresponding three in the
   not FAST section. */
#define HIBYTE(p)         z_memory[p]
#define HIWORD(p)         MSBdecodeZ(z_memory + (p))
#define HISTRWORD(p)      HIWORD(p)

#else /* not FAST */

/* FIXME: these tests may not work on 16 bit machines */

#define LOBYTE(p)         ((p) >= ZWORD_WRAP ? z_range_error(p) : \
                           z_memory[p])
#define LOBYTEcopy(a, b)  ((void) \
                           ((a) >= dynamic_size ? z_range_error(a) : \
                           (z_memory[a] = LOBYTE(b))))
#define LOBYTEwrite(p, n) ((void) \
                           ((p) >= dynamic_size ? z_range_error(p) : \
                           (z_memory[p] = (n))))
#define LOWORD(p)         ((p) + ZWORD_SIZE > ZWORD_WRAP ? z_range_error(p) : \
                           MSBdecodeZ(z_memory + (p)))
#define LOWORDcopy(d, s)  ((void)  \
                          ((d) + ZWORD_SIZE > dynamic_size ? z_range_error(d) : \
                           BYTEcopyZ(z_memory + (d), z_memory + (s))))
#define LOWORDwrite(p, n) ((void) \
                          ((p) + ZWORD_SIZE > dynamic_size ? z_range_error(p) : \
                           MSBencodeZ(z_memory + (p), n)))

#define HIBYTE(p)         ((p) >= game_size ? z_range_error(p) : z_memory[p])
#define HIWORD(p)         ((p) + ZWORD_SIZE > total_size ? z_range_error(p) : \
                           MSBdecodeZ(z_memory + (p)))
#define HISTRWORD(p)      HIWORD(p)


#endif /* not FAST */



/* Probably your system has more efficient ways of reading/writing MSB values,
   so go ahead and plop it in here if you crave that extra bit of speed */

#define MSBdecode1(v) ((v)[0])
#define MSBdecode2(v) ((((zword) (v)[0]) << 8) | (v)[1])
#define MSBdecode3(v) ((((offset) (v)[0]) << 16) | (((offset) (v)[1]) << 8) \
                       | (v)[2])
#define MSBdecode4(v) ((((offset) (v)[0]) << 24) | (((offset) (v)[1]) << 16) \
                     | (((offset) (v)[2]) <<  8) | (v)[3])

#define MSBencode1(v, n) ((v)[0] = (char) (n))
#define MSBencode2(v, n) (((v)[0] = (char) ((n) >> 8)), ((v)[1] = (char) (n)))
#define MSBencode3(v, n) (((v)[0] = (char) ((n) >> 16)), \
                          ((v)[1] = (char) ((n) >> 8)), \
                          ((v)[2] = (char) (n)))
#define MSBencode4(v, n) (((v)[0] = (char) ((n) >> 24)), \
                          ((v)[1] = (char) ((n) >> 16)), \
                          ((v)[2] = (char) ((n) >>  8)), \
                          ((v)[3] = (char) (n)))

#define BYTEcopy1(d, s) ((d)[0] = (s)[0])
#define BYTEcopy2(d, s) ((d)[0] = (s)[0], (d)[1] = (s)[1])
#define BYTEcopy3(d, s) ((d)[0] = (s)[0], (d)[1] = (s)[1], (d)[2] = (s)[2])
#define BYTEcopy4(d, s) ((d)[0] = (s)[0], (d)[1] = (s)[1], \
                         (d)[2] = (s)[2], (d)[3] = (s)[3])


#define MSBdecodeZ(v)    XPASTE(MSBdecode, ZWORD_SIZE)(v)
#define MSBencodeZ(v, n) XPASTE(MSBencode, ZWORD_SIZE)(v, n)
#define BYTEcopyZ(d, s)  XPASTE(BYTEcopy, ZWORD_SIZE)(d, s)



#define UNPACKR(paddr) (paddr * granularity + rstart)
#define UNPACKS(paddr) (paddr * granularity + sstart)

#define PACKR(addr) ((addr - rstart) / granularity)
#define PACKS(addr) ((addr - sstart) / granularity)

/* Byte offsets into the header */
#define HD_ZVERSION   0x00
#define HD_FLAGS1     0x01
#define HD_RELNUM     0x02
#define HD_HIMEM      0x04
#define HD_INITPC     0x06
#define HD_DICT       0x08
#define HD_OBJTABLE   0x0a
#define HD_GLOBVAR    0x0c
#define HD_STATMEM    0x0e
#define HD_FLAGS2     0x10
#define HD_SERNUM     0x12
#define HD_ABBREV     0x18
#define HD_LENGTH     0x1a
#define HD_CHECKSUM   0x1c
#define HD_TERPNUM    0x1e
#define HD_TERPVER    0x1f
#define HD_SCR_HEIGHT 0x20
#define HD_SCR_WIDTH  0x21
#define HD_SCR_WUNIT  0x22
#define HD_SCR_HUNIT  0x24
#define HD_FNT_WIDTH  0x26
#define HD_FNT_HEIGHT 0x27
#define HD_RTN_OFFSET 0x28
#define HD_STR_OFFSET 0x2a
#define HD_DEF_BACK   0x2c
#define HD_DEF_FORE   0x2d
#define HD_TERM_CHAR  0x2e
#define HD_STR3_WIDTH 0x30
#define HD_STD_REV    0x32
#define HD_ALPHABET   0x34
#define HD_HEADER_EXT 0x36
#define HD_USERID     0x38
#define HD_INFORMVER  0x3c


enum zversions { v1 = 1, v2, v3, v4, v5, v6, v7, v8, vM };
enum opcodeinfoflags { opNONE = 0, opSTORES = 1, opBRANCHES = 2, opJUMPS = 4,
		       opTEXTINLINE = 8 };

typedef struct {
  const char *name;
  int minversion, maxversion;
  int minargs, maxargs;
  int flags;
  int watchlevel;
} opcodeinfo;


typedef enum { OBJ_GET_INFO, OBJ_RECEIVE, OBJ_MOVE } watchinfo;


#include "portfunc.h"    /* For miscellaneous string functions, etc. */
#include "hash.h"
#include "linkevil.h"
#include "struct.h"
#include "globals.h"
#include "binary.h"
#include "errmesg.h"
#include "iff.h"
#include "init.h"
#include "decode.h"
#include "main.h"

#include "nio.h"
#include "z_io.h"

#include "no_snd.h"
#include "gi_blorb.h"
#include "no_graph.h"
#include "no_blorb.h"

#include "infix.h"
#include "debug.h"
#include "inform.h"
#include "copying.h"
#include "solve.h"
#include "automap.h"

#include "zscii.h"
#include "tokenise.h"
#include "op_call.h"
#include "op_jmp.h"
#include "op_math.h"
#include "quetzal.h"
#include "undo.h"
#include "op_save.h"
#include "op_table.h"
#include "op_v6.h"
#include "objects.h"
#include "stack.h"
#include "oplist.h"


strid_t startup_findfile(void);

strid_t intd_filehandle_open(strid_t savefile, glui32 operating_id,
                             glui32 contents_id, glui32 interp_id,
                             glui32 length);

void intd_filehandle_make(strid_t savefile);

glui32 intd_get_size(void);

strid_t startup_open(const char *name);


#endif

