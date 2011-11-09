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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

    The author can be reached at nitfol@deja.com
*/
#include "nitfol.h"
#include <limits.h>
#include <stdio.h>

/* string.c - decode and encode strings */


#define NUM_CACHE 4
#define CACHE_SIZE 4096

/*
static offset recent[4];
static unsigned uses[4];

static int is_cached;
*/

int getstring(zword packedaddress)
{
  return decodezscii(UNPACKS(packedaddress), output_char);
}


/* Returns character for given alphabet, letter pair */
static unsigned char alphabetsoup(unsigned spoon, unsigned char letter)
{
  const char *alphabet;

  if(letter == 0)
    return 32;     /* space */

  if(letter < 6 || letter > 31)
    return 32;     /* gurgle */

  if(zversion == 1) {
    alphabet = "abcdefghijklmnopqrstuvwxyz"
               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
               " 0123456789.,!?_#'\"/\\<-:()";
    if(letter == 1)
      return 13;   /* newline */
  } else {
    alphabet = "abcdefghijklmnopqrstuvwxyz"
               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
               " ^0123456789.,!?_#'\"/\\-:()";

    if(spoon == 2 && letter == 7)
      return 13;   /* newline */

    if(zversion >= 5) { /* v5 can use its own bowl of soup */
      zword t = LOWORD(HD_ALPHABET);
      if(t != 0)
	return LOBYTE(t + spoon*26 + (letter-6));   /* alphabet = z_memory+t */
    }
  }

  return alphabet[spoon * 26 + (letter-6)];
}


#define END -1


/* Text is arranged in triplets - this function takes a reference to location,
 * and to shift_amt which says how many bits are left at that location. Each
 * letter in 5 bits, shift_amt is decremented by this each time.  When a
 * location runs out, we move to another location.
 */
static char untriplet(offset *location, int *shift_amt)
{
  unsigned triplet;
  unsigned char result;
  if(*shift_amt == END) {
    if(!testing_string)
      n_show_error(E_STRING, "attempt to read past end of string", *location);
    string_bad = TRUE;
    return 0;
  }
  triplet = HISTRWORD(*location);
  result = (triplet >> *shift_amt) & b00011111;

  *shift_amt -= 5;       /* next character is 5 bits to the right */

  if(*shift_amt < 0) {   /* Reached end of triplet; go on to next */
    *shift_amt = 10;
    *location += 2;

    if(triplet & 0x8000) /* High bit set - reached the end of the string */
      *shift_amt = END;
  }
  return result;
}


/* Decodes a zscii string at address 'zscii', sending the decoded characters
   into putcharfunc.  Returns number of zscii characters it ate until it
   reached the end of the string. */
int decodezscii(offset zscii, void (*putcharfunc)(int))
{
  const int alphaup[3] = { 1, 2, 0 };
  const int alphadn[3] = { 2, 0, 1 };

  int shift_amt = 10;
  int alphalock = 0;
  int alphashift = 0;
  int alphacurrent;
  offset startzscii = zscii;
  static int depth = 0;
  depth++;

  if(depth > 2) { /* Nested abbreviations */
    if(!testing_string) {
      int remdepth = depth;
      depth = 0;
      n_show_error(E_STRING, "nested abbreviations", zscii);
      depth = remdepth;
    }
    string_bad = TRUE;
    depth--;
    return 0;
  }

  do {
    unsigned char z, x;

    if(zscii > total_size) {
      if(!testing_string)
	n_show_error(E_STRING, "attempt to print string beyond end of story", zscii);
      string_bad = TRUE;
      depth--;
      return 0;
    }

    z = untriplet(&zscii, &shift_amt);

    alphacurrent = alphashift;
    alphashift = alphalock;

    if(z < 6) {
      if(zversion <= 2) {
	switch(z) {
	case 0: putcharfunc(32); break;                 /* space */
	case 1:
	  if(zversion == 1) {
	    putcharfunc(13);                            /* newline */
	  } else {                                      /* abbreviation */
	    x = untriplet(&zscii, &shift_amt);
	    decodezscii(((offset) HIWORD(z_synonymtable + x*ZWORD_SIZE)) * 2,
			putcharfunc);
	  }
	  break;
	case 2: alphashift = alphaup[alphashift]; break;
	case 3: alphashift = alphadn[alphashift]; break;
	case 4: alphalock = alphashift = alphaup[alphalock]; break;
	case 5: alphalock = alphashift = alphadn[alphalock]; break;
	}
      } else {
	switch(z) {
	case 0: putcharfunc(32); break;       /* space */
	case 1: case 2: case 3:                         /* abbreviations */
	  x = untriplet(&zscii, &shift_amt);
	  decodezscii((offset) 2 * HIWORD(z_synonymtable +
					  (32*(z-1) + x) * ZWORD_SIZE),
		      putcharfunc);

	  break;
	case 4: alphashift = alphaup[alphashift]; break;
	case 5: alphashift = alphadn[alphashift]; break;
	}
      }
    } else {

      if(alphacurrent == 2 && z == 6) {
	int multibyte;
	if(shift_amt == END)
	  break;

	multibyte = untriplet(&zscii, &shift_amt) << 5;

	if(shift_amt == END)
	  break;
	multibyte |= untriplet(&zscii, &shift_amt);

	putcharfunc(multibyte);
      } else {
	putcharfunc(alphabetsoup(alphacurrent, z));
      }
    }
  } while(shift_amt != END);
  
  depth--;
  return zscii - startzscii;
}


static void tripletize(zbyte **location, unsigned *triplet, int *count,
		       char value, BOOL isend)
{
  if(*location == NULL)
    return;                  /* stop doing stuff if we're already done. */

  *triplet = ((*triplet) << 5) | value;
  *count += 1;

  if(isend) {
    while(*count < 3) {
      *triplet = ((*triplet) << 5) | 5;   /* 5 is the official pad char */
      *count += 1;
    }
    *triplet |= 0x8000;                   /* end bit */
  }

  if(*count == 3) {
    (*location)[0] = *triplet >> 8;        /* high byte first */
    (*location)[1] = *triplet & 255;       /* then lower */
    *triplet = 0;
    *count = 0;
    *location += 2;

    if(isend)
      *location = NULL;
  }
}


static BOOL search_soup(zbyte c, int *rspoon, int *rletter)
{
  int spoon, letter;
  for(spoon = 0; spoon < 3; spoon++)
    for(letter = 0; letter < 32; letter++)
      if(c == alphabetsoup(spoon, letter)) {
	*rspoon = spoon;
	*rletter = letter;
	return TRUE;
      }
  return FALSE;
}


int encodezscii(zbyte *dest, int mindestlen, int maxdestlen,
		const char *source, int sourcelen)
{
  int alphachanger[3];
  int i;
  int destlen = 0;
  int done = FALSE;
  unsigned triplet = 0; int count = 0;

  if(zversion <= 2) {
    alphachanger[1] = 2;   /* Shift up */
    alphachanger[2] = 3;   /* Shift down */
  } else {
    alphachanger[1] = 4;   /* Shift up */
    alphachanger[2] = 5;   /* Shift down */
  }
  mindestlen *= 3; maxdestlen *= 3; /* Change byte sizes to zscii sizes */
  mindestlen /= 2; maxdestlen /= 2;

  for(i = 0; i < sourcelen && !done && dest != NULL; i++) {
    int spoon, letter;
    if(search_soup(source[i], &spoon, &letter)) {
      if(spoon != 0) {   /* switch alphabet if necessary */
	destlen++;
	tripletize(&dest, &triplet, &count,
		   alphachanger[spoon], destlen >= maxdestlen);
      }

      destlen++;
      done = ((destlen >= maxdestlen) || (i == sourcelen - 1)) &&
	(destlen >= mindestlen);

      tripletize(&dest, &triplet, &count, letter, done);
    } else {    /* The character wasn't found, so use multibyte encoding */
      destlen++;
      tripletize(&dest, &triplet, &count, alphachanger[2],destlen>=maxdestlen);
      destlen++;
      tripletize(&dest, &triplet, &count, 6, destlen >= maxdestlen);
      destlen++;
                                 /* Upper 5 bits (really 3) */
      tripletize(&dest, &triplet, &count, source[i] >> 5, destlen>=maxdestlen);
      
      destlen++;
      done = ((destlen >= maxdestlen) || (i == sourcelen - 1)) &&
	(destlen >= mindestlen);
                                           /* Lower 5 bits */
      tripletize(&dest, &triplet, &count, source[i] & b00011111, done);
    }

  }

  if(!done) {                                     /* come back here */
    while(destlen < mindestlen - 1) {              /* oh yeah you pad me out */
      tripletize(&dest, &triplet, &count, 5, FALSE);/* uh huh */
      destlen++;                                    /* uh yup */
    }                                              /* uh */
    tripletize(&dest, &triplet, &count, 5, TRUE); /* uh huh, done */
  }
  return i;
}

void op_encode_text(void)
{
  encodezscii(z_memory + operand[3], 6, 6,
	      (char *) z_memory + operand[0] + operand[2], operand[1]);
}
