#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/FIOXOR.C,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  fioxor.c - file i/o encryption (XOR) routine
Function
  Encrypts/decrypts a block of memory using a simple function that
  generates a stream of pseudo-random characters and xors them with
  a data block.  Used to hide the data within a game object.  This
  is a terribly insecure code, intended only to slow down would-be
  prying eyes.  Fortunately, it isn't obvious what the plaintext is,
  so the simplicity of the encryption algorithm should be offset
  somewhat by the obscurity of the unencrypted data.
Notes
  None
Modified
  03/13/93 MJRoberts   - parameterize encryption scheme
  04/16/92 MJRoberts   - creation
*/

#include "os.h"
#include "std.h"
#include "fio.h"

void fioxor(uchar *p, uint siz, uint seed, uint inc)
{
    for ( ; siz ; seed += inc, --siz)
        *p++ ^= (uchar)seed;
}
