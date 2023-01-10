/*----------------------------------------------------------------------*\
  sysdep.c

  This file contains necessary system dependent routines.

  Notes on string handling:

  - Native - means the "natural" character set/encoding for the
    platform, nowadays often UTF-8, but that was not the case in the
    beginning.

  - Internal - is always ISO8859-1 in which encoding everything
    internal should use (even dictionary entries, which is not certain
    it does currently. TODO!

  - Current - the compiler (currently) has options for different
    charsets, if that is used all input files are considered to be in
    that encoding, which might be different from the native encoding.
    It (will) also auto-detect an UTF BOM and enforce UTF-8 for that
    single file, which again might be different from native or the one
    given using the -charset option.

\*----------------------------------------------------------------------*/

#include <time.h>
#include "sysdep.h"
#include "syserr.h"


#ifdef HAVE_GLK
#include "glk.h"
#endif


#ifdef HAVE_GLK

/* Note to Glk maintainers: 'native' characters are used for output, in this
   case, Glk's Latin-1.  ISO characters are Alan's internal representation,
   stored in the .A3C file, and must be converted to native before printing.
   Glk could just use the ISO routines directly, but its safer to maintain
   its own tables to guard against future changes in either Alan or Glk (ie. a
   move to Unicode).
 */

static char spaceCharacters[] =
{
  0x0A, /* linefeed */
  0x20, /* space */
  0xA0, /* non-breaking space */
  0x00
};

static char lowerCaseCharacters[] =
{
  0x61, /* a */  0x62, /* b */  0x63, /* c */  0x64, /* d */
  0x65, /* e */  0x66, /* f */  0x67, /* g */  0x68, /* h */
  0x69, /* i */  0x6A, /* j */  0x6B, /* k */  0x6C, /* l */
  0x6D, /* m */  0x6E, /* n */  0x6F, /* o */  0x70, /* p */
  0x71, /* q */  0x72, /* r */  0x73, /* s */  0x74, /* t */
  0x75, /* u */  0x76, /* v */  0x77, /* w */  0x78, /* x */
  0x79, /* y */  0x7A, /* z */  0xDF, /* ss <small sharp s> */
  0xE0, /* a grave */           0xE1, /* a acute */
  0xE2, /* a circumflex */      0xE3, /* a tilde */
  0xE4, /* a diaeresis */       0xE5, /* a ring */
  0xE6, /* ae */                0xE7, /* c cedilla */
  0xE8, /* e grave */           0xE9, /* e acute */
  0xEA, /* e circumflex */      0xEB, /* e diaeresis */
  0xEC, /* i grave */           0xED, /* i acute */
  0xEE, /* i circumflex */      0xEF, /* i diaeresis */
  0xF0, /* <small eth> */       0xF1, /* n tilde */
  0xF2, /* o grave */           0xF3, /* o acute */
  0xF4, /* o circumflex */      0xF5, /* o tilde */
  0xF6, /* o diaeresis */       0xF8, /* o slash */
  0xF9, /* u grave */           0xFA, /* u acute */
  0xFB, /* u circumflex */      0xFC, /* u diaeresis */
  0xFD, /* y acute */           0xFE, /* <small thorn> */
  0xFF, /* y diaeresis */       0x00
};

/* FIXME: ss <small sharp s> and y diaeresis have no UC analogues
   Are they really considered LC?
 */

static char upperCaseCharacters[] =
{
  0x41, /* A */  0x42, /* B */  0x43, /* C */  0x44, /* D */
  0x45, /* E */  0x46, /* F */  0x47, /* G */  0x48, /* H */
  0x49, /* I */  0x4A, /* J */  0x4B, /* K */  0x4C, /* L */
  0x4D, /* M */  0x4E, /* N */  0x4F, /* O */  0x50, /* P */
  0x51, /* Q */  0x52, /* R */  0x53, /* S */  0x54, /* T */
  0x55, /* U */  0x56, /* V */  0x57, /* W */  0x58, /* X */
  0x59, /* Y */  0x5A, /* Z */
  0xC0, /* A grave */           0xC1, /* A acute */
  0xC2, /* A circumflex */      0xC3, /* A tilde */
  0xC4, /* A diaeresis */       0xC5, /* A ring */
  0xC6, /* AE */                0xC7, /* C cedilla */
  0xC8, /* E grave */           0xC9, /* E acute */
  0xCA, /* E circumflex */      0xCB, /* E diaeresis */
  0xCC, /* I grave */           0xCD, /* I acute */
  0xCE, /* I circumflex */      0xCF, /* I diaeresis */
  0xD0, /* <capital eth> */     0xD1, /* N tilde */
  0xD2, /* O grave */           0xD3, /* O acute */
  0xD4, /* O circumflex */      0xD5, /* O tilde */
  0xD6, /* O diaeresis */       0xD8, /* O slash */
  0xD9, /* U grave */           0xDA, /* U acute */
  0xDB, /* U circumflex */      0xDC, /* U diaeresis */
  0xDD, /* Y acute */           0xDE, /* <capital thorn> */
  0x00
};

#else

/* These work on internal (ISO8859-1) character sets */

static unsigned char spaceCharacters[] = " \t\n";

/*                                        "abcdefghijklmnopqrstuvwxyz   à   á   â   ã   ä   å   æ   ç   è   é   ê   ë   ì   í   î   ï   ð   ñ   ò   ó   ô   õ   ö   ø   ù   ú   û   ü   ý   þ   ÿ"; */
static const char lowerCaseCharacters[] = "abcdefghijklmnopqrstuvwxyz\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF8\xF9\xFA\xFB\xFC\xFF\xFE\xFF";

/*                                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ   À   Á   Â   Ã   Ä   Å   Æ   Ç   È   É   Ê   Ë   Ì   Í   Î   Ï   Ð   Ñ   Ò   Ó   Ô   Õ   Ö   Ø   Ù   Ú   Û   Û   Ý   Þ   ß"; */
static const char upperCaseCharacters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF";

#endif

int isSpace(unsigned int c)              /* IN - Internal character to test */
{
    return (c != '\0' && strchr((char *)spaceCharacters, c) != 0);
}


int isLower(unsigned int c)              /* IN - Internal character to test */
{
    return (c != '\0' && strchr((char *)lowerCaseCharacters, c) != 0);
}


int isUpper(unsigned int c)              /* IN - Internal character to test */
{
    return (c != '\0' && strchr((char *)upperCaseCharacters, c) != 0);
}

int isLetter(unsigned int c)             /* IN - Internal character to test */
{
  return(c != '\0' && (isLower(c)? !0: isUpper(c)));
}


int toLower(unsigned int c)              /* IN - Internal character to convert */
{
#ifdef HAVE_GLK
  return glk_char_to_lower(c);
#else
  return (isUpper(c)? c + ('a' - 'A'): c);
#endif
}

int toUpper(unsigned int c)              /* IN - Internal character to convert */
{
#ifdef HAVE_GLK
  return glk_char_to_upper(c);
#else
  return (isLower(c)? c - ('a' - 'A'): c);
#endif
}

void stringToLowerCase(char string[]) /* INOUT - Internal string to convert */
{
  char *s;

  for (s = string; *s; s++)
    *s = toLower(*s);
}


/*----------------------------------------------------------------------*/
bool equalStrings(char *str1, char *str2)
{
  char *s1 = str1, *s2 = str2;

  while (*s1 != '\0' && *s2 != '\0') {
    if (toLower(*s1) != toLower(*s2))
        return false;
    s1++;
    s2++;
  }
  return toLower(*s1) == toLower(*s2);
}


/*======================================================================*/
int littleEndian() {
  int x = 1;
  return (*(char *)&x == 1);
}


/*======================================================================*/
char *baseNameStart(char *fullPathName) {
  static char *delimiters = "\\>]/:";
  int i;

  for (i = strlen(fullPathName)-1; i > 0; i--)
    if (strchr(delimiters, fullPathName[i]) != NULL)
      return &fullPathName[i+1];
  return(fullPathName);
}
