#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/mkchrtab.cpp,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  mkchrtab.cpp - TADS character table generator
Function
  
Notes
  
Modified
  10/17/98 MJRoberts  - creation (from TADS 2 mkchrtab.c)
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "t3std.h"

/*
 *   Read a number.  Returns zero on success, non-zero on error.
 */
static int read_number(unsigned short *result, char **p,
                       char *infile, int linenum, int optional)
{
    unsigned short base;
    unsigned short acc;
    int digcnt;
    
    /* skip any leading spaces */
    while (isspace(**p))
        ++(*p);

    /* 
     *   if the entry is optional, and we've reached a comment or the end
     *   of the line, return failure, but don't print an error (since the
     *   number is optional, it's not an error if it's not present) 
     */
    if (optional &&
        (**p == '\0' || **p == '\n' || **p == '\r' || **p == '#'))
        return 2;

    /*
     *   Check for a character value 
     */
    if (**p == '\'')
    {
        unsigned char c;
        
        /* get the next character */
        ++(*p);

        /* if it's a backslash, escape the next character */
        if (**p == '\\')
        {
            /* skip the backslash */
            ++(*p);

            /* the next character gives the value */
            switch(**p)
            {
            case 'n':
                c = '\n';
                break;

            case 'r':
                c = '\r';
                break;

            case 't':
                c = '\t';
                break;

            case '\\':
                c = '\\';
                break;

            case '\'':
                c = '\'';
                break;

            default:
                printf("%s: line %d: invalid backslash sequence '\\%c'\n",
                       infile, linenum, **p);
                return 4;
            }
        }
        else
        {
            /* use this value as the character code */
            c = (unsigned char)**p;
        }

        /* skip the character, and make sure we have the closing quote */
        ++(*p);
        if (**p != '\'')
        {
            printf("%s: line %d: missing close quote\n", infile, linenum);
            return 5;
        }

        /* skip the close quote */
        ++(*p);

        /* skip trailing spaces */
        while (isspace(**p))
            ++(*p);

        /* return the result */
        *result = (unsigned short)c;
        return 0;
    }

    /* 
     *   determine the base - if there's a leading zero, it's hex or
     *   octal; otherwise, it's decimal 
     */
    if (**p == '0')
    {
        /* skip the leading zero */
        ++(*p);
        
        /* if the next character is 'x', it's hex, otherwise it's octal */
        if (**p == 'x' || **p == 'X')
        {
            /* skip the 'x' */
            ++(*p);

            /* it's hex */
            base = 16;
        }
        else
        {
            /* it's octal */
            base = 8;
        }
    }
    else
    {
        /* no leading zero - it's a decimal number */
        base = 10;
    }

    /* read digits */
    for (acc = 0, digcnt = 0 ;; ++(*p), ++digcnt)
    {
        /* see if we still have a digit */
        if (isdigit(**p) || (base == 16 && isxdigit(**p)))
        {
            unsigned short dig;
            
            /* get this digit's value */
            dig = (unsigned short)(isdigit(**p)
                                   ? **p - '0'
                                   : **p - (isupper(**p) ? 'A' : 'a') + 10);

            /* make sure it's in range for our radix */
            if (dig >= base)
            {
                printf("%s: line %d: invalid digit for radix\n",
                       infile, linenum);
                return 3;
            }

            /* accumulate this digit */
            acc *= base;
            acc += dig;
        }
        else
        {
            /* that's the end of the number */
            break;
        }
    }

    /* if we didn't get any valid digits, it's an error */
    if (digcnt == 0)
    {
        printf("%s: line %d: invalid number\n", infile, linenum);
        return 1;
    }

    /* skip any trailing spaces */
    while (isspace(**p))
        ++(*p);

    /* return the accumulated value */
    *result = acc;

    /* success */
    return 0;
}


/*
 *   HTML Entity mapping structure 
 */
struct entity_t
{
    const char *ename;
    wchar_t charval;
};

/*
 *   List of HTML TADS entity names and the corresponding Unicode
 *   character codes 
 */
static const struct entity_t entities[] =
{
    { "nbsp", 160 },
    { "iexcl", 161 },
    { "cent", 162 },
    { "pound", 163 },
    { "curren", 164 },
    { "yen", 165 },
    { "brvbar", 166 },
    { "sect", 167 },
    { "uml", 168 },
    { "copy", 169 },
    { "ordf", 170 },
    { "laquo", 171 },
    { "not", 172 },
    { "shy", 173 },
    { "reg", 174 },
    { "macr", 175 },
    { "deg", 176 },
    { "plusmn", 177 },
    { "sup2", 178 },
    { "sup3", 179 },
    { "acute", 180 },
    { "micro", 181 },
    { "para", 182 },
    { "middot", 183 },
    { "cedil", 184 },
    { "sup1", 185 },
    { "ordm", 186 },
    { "raquo", 187 },
    { "frac14", 188 },
    { "frac12", 189 },
    { "frac34", 190 },
    { "iquest", 191 },
    { "times", 215 },
    { "divide", 247 },
    { "AElig", 198 },
    { "Aacute", 193 },
    { "Acirc", 194 },
    { "Agrave", 192 },
    { "Aring", 197 },
    { "Atilde", 195 },
    { "Auml", 196 },
    { "Ccedil", 199 },
    { "ETH", 208 },
    { "Eacute", 201 },
    { "Ecirc", 202 },
    { "Egrave", 200 },
    { "Euml", 203 },
    { "Iacute", 205 },
    { "Icirc", 206 },
    { "Igrave", 204 },
    { "Iuml", 207 },
    { "Ntilde", 209 },
    { "Oacute", 211 },
    { "Ocirc", 212 },
    { "Ograve", 210 },
    { "Oslash", 216 },
    { "Otilde", 213 },
    { "Ouml", 214 },
    { "THORN", 222 },
    { "Uacute", 218 },
    { "Ucirc", 219 },
    { "Ugrave", 217 },
    { "Uuml", 220 },
    { "Yacute", 221 },
    { "aacute", 225 },
    { "acirc", 226 },
    { "aelig", 230 },
    { "agrave", 224 },
    { "aring", 229 },
    { "atilde", 227 },
    { "auml", 228 },
    { "ccedil", 231 },
    { "eacute", 233 },
    { "ecirc", 234 },
    { "egrave", 232 },
    { "eth", 240 },
    { "euml", 235 },
    { "iacute", 237 },
    { "icirc", 238 },
    { "igrave", 236 },
    { "iuml", 239 },
    { "ntilde", 241 },
    { "oacute", 243 },
    { "ocirc", 244 },
    { "ograve", 242 },
    { "oslash", 248 },
    { "otilde", 245 },
    { "ouml", 246 },
    { "szlig", 223 },
    { "thorn", 254 },
    { "uacute", 250 },
    { "ucirc", 251 },
    { "ugrave", 249 },
    { "uuml", 252 },
    { "yacute", 253 },
    { "thorn", 254 },
    { "yuml", 255 },

    /* TADS extensions to the standard characters */
    { "lsq", 8216 },
    { "rsq", 8217 },
    { "ldq", 8220 },
    { "rdq", 8221 },
    { "endash", 8211 },
    { "emdash", 8212 },
    { "trade", 8482 },

    /* HTML 4.0 character extensions */
    { "ndash", 8211 },
    { "mdash", 8212 },
    { "lsquo", 8216 },
    { "rsquo", 8217 },
    { "ldquo", 8220 },
    { "rdquo", 8221 },
    { "sbquo", 8218 },
    { "bdquo", 8222 },
    { "lsaquo", 8249 },
    { "rsaquo", 8250 },
    { "dagger", 8224 },
    { "Dagger", 8225 },
    { "OElig", 338 },
    { "oelig", 339 },
    { "permil", 8240 },
    { "Yuml", 376 },
    { "scaron", 353 },
    { "Scaron", 352 },
    { "circ", 710 },
    { "tilde", 732 },

    /* Greek letters */
    { "Alpha", 913 },
    { "Beta", 914 },
    { "Gamma", 915 },
    { "Delta", 916 },
    { "Epsilon", 917 },
    { "Zeta", 918 },
    { "Eta", 919 },
    { "Theta", 920 },
    { "Iota", 921 },
    { "Kappa", 922 },
    { "Lambda", 923 },
    { "Mu", 924 },
    { "Nu", 925 },
    { "Xi", 926 },
    { "Omicron", 927 },
    { "Pi", 928 },
    { "Rho", 929 },
    { "Sigma", 931 },
    { "Tau", 932 },
    { "Upsilon", 933 },
    { "Phi", 934 },
    { "Chi", 935 },
    { "Psi", 936 },
    { "Omega", 937 },
    { "alpha", 945 },
    { "beta", 946 },
    { "gamma", 947 },
    { "delta", 948 },
    { "epsilon", 949 },
    { "zeta", 950 },
    { "eta", 951 },
    { "theta", 952 },
    { "iota", 953 },
    { "kappa", 954 },
    { "lambda", 955 },
    { "mu", 956 },
    { "nu", 957 },
    { "xi", 958 },
    { "omicron", 959 },
    { "pi", 960 },
    { "rho", 961 },
    { "sigmaf", 962 },
    { "sigma", 963 },
    { "tau", 964 },
    { "upsilon", 965 },
    { "phi", 966 },
    { "chi", 967 },
    { "psi", 968 },
    { "omega", 969 },
    { "thetasym", 977 },
    { "upsih", 978 },
    { "piv", 982 },

    /* general punctuation marks */
    { "bull", 8226 },
    { "hellip", 8230 },
    { "prime", 8242 },
    { "Prime", 8243 },
    { "oline", 8254 },
    { "frasl", 8260 },

    /* letter-like symbols */
    { "weierp", 8472 },
    { "image", 8465 },
    { "real", 8476 },
    { "alefsym", 8501 },

    /* arrows */
    { "larr", 8592 },
    { "uarr", 8593 },
    { "rarr", 8594 },
    { "darr", 8595 },
    { "harr", 8596 },
    { "crarr", 8629 },
    { "lArr", 8656 },
    { "uArr", 8657 },
    { "rArr", 8658 },
    { "dArr", 8659 },
    { "hArr", 8660 },

    /* mathematical operators */
    { "forall", 8704 },
    { "part", 8706 },
    { "exist", 8707 },
    { "empty", 8709 },
    { "nabla", 8711 },
    { "isin", 8712 },
    { "notin", 8713 },
    { "ni", 8715 },
    { "prod", 8719 },
    { "sum", 8721 },
    { "minus", 8722 },
    { "lowast", 8727 },
    { "radic", 8730 },
    { "prop", 8733 },
    { "infin", 8734 },
    { "ang", 8736 },
    { "and", 8743 },
    { "or", 8744 },
    { "cap", 8745 },
    { "cup", 8746 },
    { "int", 8747 },
    { "there4", 8756 },
    { "sim", 8764 },
    { "cong", 8773 },
    { "asymp", 8776 },
    { "ne", 8800 },
    { "equiv", 8801 },
    { "le", 8804 },
    { "ge", 8805 },
    { "sub", 8834 },
    { "sup", 8835 },
    { "nsub", 8836 },
    { "sube", 8838 },
    { "supe", 8839 },
    { "oplus", 8853 },
    { "otimes", 8855 },
    { "perp", 8869 },
    { "sdot", 8901 },
    { "lceil", 8968 },
    { "rceil", 8969 },
    { "lfloor", 8970 },
    { "rfloor", 8971 },
    { "lang", 9001 },
    { "rang", 9002 },

    /* geometric shapes */
    { "loz", 9674 },

    /* miscellaneous symbols */
    { "spades", 9824 },
    { "clubs", 9827 },
    { "hearts", 9829 },
    { "diams", 9830 },

    /* Latin-extended B */
    { "fnof", 402 },

    /* Latin-2 characters */
    { "Aogon", 260 },
    { "breve", 728 },
    { "Lstrok", 321 },
    { "Lcaron", 317 },
    { "Sacute", 346 },
    { "Scedil", 350 },
    { "Tcaron", 356 },
    { "Zacute", 377 },
    { "Zcaron", 381 },
    { "Zdot", 379 },
    { "aogon", 261 },
    { "ogon", 731 },
    { "lstrok", 322 },
    { "lcaron", 318 },
    { "sacute", 347 },
    { "caron", 711 },
    { "scedil", 351 },
    { "tcaron", 357 },
    { "zacute", 378 },
    { "dblac", 733 },
    { "zcaron", 382 },
    { "zdot", 380 },
    { "Racute", 340 },
    { "Abreve", 258 },
    { "Lacute", 313 },
    { "Cacute", 262 },
    { "Ccaron", 268 },
    { "Eogon", 280 },
    { "Ecaron", 282 },
    { "Dcaron", 270 },
    { "Dstrok", 272 },
    { "Nacute", 323 },
    { "Ncaron", 327 },
    { "Odblac", 336 },
    { "Rcaron", 344 },
    { "Uring", 366 },
    { "Udblac", 368 },
    { "Tcedil", 354 },
    { "racute", 341 },
    { "abreve", 259 },
    { "lacute", 314 },
    { "cacute", 263 },
    { "ccaron", 269 },
    { "eogon", 281 },
    { "ecaron", 283 },
    { "dcaron", 271 },
    { "dstrok", 273 },
    { "nacute", 324 },
    { "ncaron", 328 },
    { "odblac", 337 },
    { "rcaron", 345 },
    { "uring", 367 },
    { "udblac", 369 },
    { "tcedil", 355 },
    { "dot", 729 },
    { 0, 0 }
};


/*
 *   Entity mapping list entry.  We store each entity mapping we find in
 *   the file in one of these structures, and link the structures together
 *   into a list. 
 */
struct entity_map_t
{
    /* 
     *   Flag: this mapping is a display expansion only (not a round-trip
     *   mapping).  If this is set, then we contain a display expansion;
     *   otherwise, we contain a round-trip mapping.  
     */
    int disp_only;

    /* length in bytes of the local mapping for this character */
    size_t local_len;

    union
    {
        /* if disp_only is false, this gives the local character value */
        unsigned short local_char;

        /* if disp_only is true, this gives the display mapping */
        struct
        {
            /* length of the display expansion */
            size_t exp_len;

            /* display expansion, in unicode characters */
            unsigned short expansion[1];
        } disp;
    } mapping;
};

/*
 *   Mapping pages.  Each page maps 256 unicode characters; the page is
 *   selected by the high 8 bits of the unicode code point.  We don't
 *   allocate a page until we set a character on the page for the first
 *   time.
 *   
 *   Each page contains 256 slots for entity_map_t pointers, so each page
 *   is an array of (entity_map_t *) elements.  
 */
static entity_map_t **G_mapping[256];

/*
 *   Construct a unicode character given a page number and index on the
 *   page 
 */
static inline unsigned int make_unicode(int pagenum, int idx)
{
    return ((unsigned int)pagenum * 256) + (unsigned int)idx;
}

/*
 *   Get the mapping for a particular unicode character value 
 */
static entity_map_t *get_mapping(unsigned int unicode_char)
{
    unsigned int pagenum;

    /* get the page */
    pagenum = ((unicode_char >> 8) & 0xff);

    /* if there's no page allocated here, there's no mapping */
    if (G_mapping[pagenum] == 0)
        return 0;

    /* return the entry at the correct location in this page */
    return G_mapping[pagenum][unicode_char & 0xff];
}

/*
 *   Set the mapping at a given unicode code point.  If there is no page
 *   allocated at the appropriate slot yet, we'll allocate a page here.
 */
static void set_mapping(unsigned int unicode_char, entity_map_t *mapping)
{
    unsigned int pagenum;

    /* get the page number - it's the high 8 bits of the unicode value */
    pagenum = ((unicode_char >> 8) & 0xff);

    /* if the page is not allocated, allocate it now */
    if (G_mapping[pagenum] == 0)
    {
        /* allocate the page and clear it out */
        G_mapping[pagenum] =
            (entity_map_t **)t3malloc(256 * sizeof(entity_map_t *));
        memset(G_mapping[pagenum], 0, 256 * sizeof(entity_map_t *));
    }

    /* set the mapping */
    G_mapping[pagenum][unicode_char & 0xff] = mapping;
}


/*
 *   Read a translation string.  This reads a unicode-to-unicode translation
 *   that gives the expansion of a unicode character to a string of unicode
 *   characters for display.  The characters of the expansion can be
 *   specified with ASCII character enclosed in single quotes, or as a series
 *   of numbers giving Unicode code points.  
 */
static entity_map_t *read_translation(char *p, char *infile, int linenum)
{
    unsigned short buf[256];
    unsigned short *dstp;
    entity_map_t *mapp;

    /* skip any intervening whitespace */
    for ( ; isspace(*p) ; ++p) ;

    /* read the series of numbers or characters */
    for (dstp = buf ; *p != '\0' ; )
    {
        unsigned short intval;

        /* skip any leading whitespace */
        while (isspace(*p))
            ++p;

        /* if it's the end of the line or a comment, stop */
        if (*p == '\0' || *p == '#')
            break;

        /* if it's a string, read it */
        if (*p == '\'')
        {
            /* scan until the closing quote */
            for (++p ; *p != '\'' && *p != '\0' ; ++p)
            {
                /* if it's an escape, skip it and read the next char */
                if (*p == '\\')
                    ++p;

                /* if we don't have room, abort */
                if (dstp >= buf + sizeof(buf))
                {
                    printf("%s: line %d: entity mapping is too long "
                           "(maximum of %d characters are allowed\n",
                           infile, linenum, sizeof(buf));
                    return 0;
                }

                /* 
                 *   store this character - since it's given as an ASCII
                 *   character, it has the same representation in Unicode as
                 *   it does in ASCII 
                 */
                *dstp++ = *p;
            }

            /* if we didn't find a closing quote, it's an error */
            if (*p != '\'')
            {
                printf("%s: line %d: no closing quote found in string\n",
                       infile, linenum);
                return 0;
            }

            /* skip the closing quote */
            ++p;
        }
        else
        {
            /* scan the character code */
            if (read_number(&intval, &p, infile, linenum, FALSE))
                return 0;

            /* make sure we haven't overflowed our buffer */
            if (dstp + 1 > buf + sizeof(buf))
            {
                printf("%s: line %d: entity mapping is too long "
                       "(maximum of %d characters are allowed\n",
                       infile, linenum, sizeof(buf)/sizeof(buf[0]));
                return 0;
            }

            /* add it to the output list */
            *dstp++ = intval;
        }
    }

    /* if we didn't get any characters, it's an error */
    if (dstp == buf)
    {
        printf("%s: line %d: no local character codes found after '='\n",
               infile, linenum);
        return 0;
    }

    /* create a new mapping structure and set it up */
    mapp = (entity_map_t *)t3malloc(sizeof(entity_map_t)
                                    + (dstp - buf)*sizeof(buf[0]));
    mapp->mapping.disp.exp_len = dstp - buf;
    memcpy(mapp->mapping.disp.expansion, buf, (dstp - buf)*sizeof(buf[0]));

    /* return the mapping */
    return mapp;
}

/*
 *   Parse an entity name mapping 
 */
static entity_map_t *parse_entity(char *p, char *infile, int linenum,
                                  unsigned short *unicode_char)
{
    const char *start;
    const struct entity_t *entp;
    
    /* find the end of the entity name */
    start = p;
    for ( ; isalpha(*p) || isdigit(*p) ; ++p) ;

    /* scan the list for the entity name */
    for (entp = entities ; entp->ename != 0 ; ++entp)
    {
        /* see if this one is a match - note that case is significant */
        if (strlen(entp->ename) == (size_t)(p - start)
            && memcmp(entp->ename, start, p - start) == 0)
            break;
    }

    /* if we didn't find it, it's an error */
    if (entp->ename == 0)
    {
        printf("%s: line %d: unknown entity name \"%.*s\"\n",
               infile, linenum, p - start, start);
        return 0;
    }

    /* tell the caller what the unicode character is */
    *unicode_char = entp->charval;

    /* read and return the translation */
    return read_translation(p, infile, linenum);
}

/*
 *   Store a local character mapping in a buffer, using the variable-length
 *   notation.  If the character value is in the range 0-255, we'll store one
 *   byte; otherwise, we'll store two bytes, high-order 8 bits first.
 *   Returns the number of bytes stored.
 *   
 *   If the output buffer is null, we'll simply return the storage length
 *   without actually storing anything.  
 */
size_t local_to_buf(unsigned char *dst, unsigned short c)
{
    /* if it's in the 0-255 range, it takes one byte, otherwise two */
    if (c <= 255)
    {
        /* store it as a single byte */
        if (dst != 0)
            dst[0] = (unsigned char)c;

        /* we stored one byte */
        return 1;
    }
    else
    {
        /* store it as two bytes, high-order byte first */
        if (dst != 0)
        {
            dst[0] = (unsigned char)(c >> 8);
            dst[1] = (unsigned char)(c & 0xFF);
        }

        /* we stored two bytes */
        return 2;
    }
}


/*
 *   Main entrypoint 
 */
int main(int argc, char **argv)
{
    int curarg;
    osfildef *fp;
    char *infile;
    char *outfile;
    int linenum;
    int display_mappings;
    entity_map_t *entp;
    int roundtrip_count;
    int disp_count;
    unsigned long ofs;
    uchar valbuf[50];
    ulong disp_bytes;
    int pagenum;
    int i;
    size_t len;
    unsigned short default_char;
    int default_char_set;
    int fatal_err;
    size_t disp_exp_cnt;
    ulong disp_exp_len;

    /* no fatal errors yet */
    fatal_err = FALSE;

    /* no mappings yet */
    roundtrip_count = 0;
    default_char_set = 0;
    disp_exp_cnt = 0;
    disp_exp_len = 0;

    /* start out in the round-trip unicode<->local section */
    display_mappings = FALSE;

#if 1
    /* 
     *   there currently are no options, so there's no option scanning
     *   needed; just start at the first argument 
     */
    curarg = 1;
    
#else
    /* scan options */
    for (curarg = 1 ; curarg < argc && argv[curarg][0] == '-' ; ++curarg)
    {
        /* else */
        {
            /* consume all remaining options so we get a usage message */
            curarg = argc;
            break;
        }
    }
#endif

    /* check for required arguments */
    if (curarg + 1 >= argc)
    {
        printf("usage: mkchrtab [options] <source> <dest>\n"
               "  <source> is the input file\n"
               "  <dest> is the output file\n"
              );
        return 1;
    }

    /* get the input and output filenames */
    infile = argv[curarg];
    outfile = argv[curarg + 1];

    /* open the input file */
    fp = osfoprs(infile, OSFTTEXT);
    if (fp == 0)
    {
        printf("error: unable to open input file \"%s\"\n", infile);
        return 2;
    }

    /* parse the input file */
    for (linenum = 1 ; ; ++linenum)
    {
        char buf[256];
        char *p;
        unsigned short unicode_char;
        entity_map_t *mapp;

        /* read the next line */
        if (osfgets(buf, sizeof(buf), fp) == 0)
            break;

        /* scan off leading spaces */
        for (p = buf ; *p != '\0' && (isspace(*p) || iscntrl(*p)) ; ++p) ;

        /* if this line is blank, or starts with a '#', ignore it */
        if (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#')
            continue;

        /* check for directives */
        len = strlen(p);
        if (len >= 16
            && memicmp(p, "display_mappings", 16) == 0
            && (!isalpha(p[16]) && !isdigit(p[16])))
        {
            /* note that we're in display mapping mode */
            display_mappings = TRUE;

            /* no need to look any further at this line */
            continue;
        }
        else if (len > 15
                 && memicmp(p, "default_display", 15) == 0
                 && isspace(p[15]))
        {
            char *nump;

            /* read the local character value for the default mapping */
            nump = p + 16;
            if (!read_number(&default_char, &nump, infile, linenum, FALSE))
            {
                /* if we already have a default character, warn about it */
                if (default_char_set)
                    printf("%s: line %d: duplicate default_display\n",
                           infile, linenum);

                /* note that we have the default character value */
                default_char_set = TRUE;
            }

            /* no need to look any further at this line */
            continue;
        }

        /* check our mode */
        if (display_mappings)
        {
            /* 
             *   We're doing display mappings: each mapping specifies a
             *   sequence of one or more unicode characters to substitute for
             *   a single unicode character on display.  These translations
             *   can use SGML entity names for the unicode characters.  
             */

            /* check for an entity name */
            if (*p == '&')
            {
                /* skip the '&' */
                ++p;
                
                /* parse the entity */
                mapp = parse_entity(p, infile, linenum, &unicode_char);
            }
            else
            {
                /* read the Unicode character number */
                if (read_number(&unicode_char, &p, infile, linenum, FALSE))
                    continue;
                
                /* read the translation */
                mapp = read_translation(p, infile, linenum);
            }

            /*
             *   If the entry is already set, flag an error and delete the
             *   entry - if it's set as a round-trip entry, we don't want to
             *   set it in the display-only section 
             */
            if (get_mapping(unicode_char) != 0)
            {
                /* flag the error */
                printf("%s: line %d: mapping previously defined - "
                       "new mapping ignored\n", infile, linenum);
                
                /* delete the mapping and forget it */
                t3free(mapp);
                mapp = 0;
            }

            /* if we created a mapping, mark it as display-only */
            if (mapp != 0)
            {
                /* mark it as display-only */
                mapp->disp_only = TRUE;

                /* 
                 *   if this is an expansion into multiple unicode characters
                 *   for display, count it among the expansions 
                 */
                if (mapp->mapping.disp.exp_len > 1)
                {
                    ++disp_exp_cnt;
                    disp_exp_len += mapp->mapping.disp.exp_len;
                }
            }
        }
        else
        {
            unsigned short local_char;
            unsigned short extra_char;   

            /*
             *   Doing regular translations.  The format must be two
             *   columns of numbers; the first number is the local
             *   character code, and the second is the unicode character
             *   code.
             *   
             *   Some mapping files contain variations on this format:
             *   
             *   - Some files use three columns, where the first column is
             *   another character set translation (for example, JIS0208
             *   uses the first column to give the Shift-JIS code for each
             *   character).  In these cases, we'll skip the first column,
             *   and use only the last two columns.
             *   
             *   - Some files for multi-byte (i.e., mixed single- and
             *   double-byte characters) have entries for DBCS leading
             *   bytes with no Unicode character specified.  In these
             *   cases, only one column appears on some lines, with DBCS
             *   lead byte value in the local character set, and no
             *   Unicode value at all.  We'll simply ignore these lines
             *   entirely, since they contribute no information that we
             *   need.  
             */

            /* 
             *   read two numbers - the second one is optional, since
             *   we'll skip any single-column entries as explained above 
             */
            if (read_number(&local_char, &p, infile, linenum, FALSE)
                || read_number(&unicode_char, &p, infile, linenum, TRUE))
                continue;

            /* 
             *   check for a third column - if it's present, drop the
             *   first column and keep only the second and third columns 
             */
            if (!read_number(&extra_char, &p, infile, linenum, TRUE))
            {
                /* shift everything over one column */
                local_char = unicode_char;
                unicode_char = extra_char;
            }

            /* if the mapping is already in use, don't overwrite it */
            if (get_mapping(unicode_char) != 0)
            {
                /* flag the error */
                printf("%s: line %d: mapping previously defined - "
                       "new mapping ignored\n", infile, linenum);

                /* skip this mapping */
                continue;
            }

            /* create a reversible mapping entry */
            mapp = (entity_map_t *)t3malloc(sizeof(entity_map_t));
            mapp->disp_only = FALSE;
            mapp->mapping.local_char = local_char;

            /* count the round-trip mapping */
            ++roundtrip_count;
        }

        /* add the mapping into our mapping list */
        if (mapp != 0)
            set_mapping(unicode_char, mapp);
    }

    /* we're done with the input file */
    osfcls(fp);

    /* open the output file */
    fp = osfopwb(outfile, OSFTCMAP);
    if (fp == 0)
    {
        printf("error: unable to open output file \"%s\"\n", outfile);
        return 2;
    }

    /*
     *   Calculate where the unicode-to-local display mapping appears.  This
     *   mapping immediately follows the local-to-unicode round-trip mapping;
     *   each entry in the round-trip mapping is of a fixed size, so we can
     *   easily figure out how much space it will take up.
     *   
     *   Each round-trip record takes up 4 bytes (two UINT2's)
     *.  A UINT4 is needed for this header itself.
     *   
     *.  The round-trip section contains at least one UINT2 (for its
     *   own count header) 
     */
    ofs = (roundtrip_count * 4) + 4 + 2;
    oswp4(valbuf, ofs);

    /* add in the count of round-trip records */
    oswp2(valbuf + 4, roundtrip_count);

    /* write out the header */
    if (osfwb(fp, valbuf, 6))
    {
        printf("error writing header\n");
        fatal_err = TRUE;
        goto done;
    }

    /* 
     *   Write the round-trip mappings.  Scan the entire set of pages, and
     *   write out each entry not marked as "display-only".  
     */
    for (pagenum = 0 ; pagenum < 256 ; ++pagenum)
    {
        /* if there's no page here, skip it */
        if (G_mapping[pagenum] == 0)
            continue;

        /* go through the mappings in this page */
        for (i = 0 ; i < 256 ; ++i)
        {
            /* get the mapping */
            entp = G_mapping[pagenum][i];

            /* 
             *   if we have no mapping, or this is a display-only mapping,
             *   skip it - we're only interested in round-trip mappings here 
             */
            if (entp == 0 || entp->disp_only)
                continue;

            /* 
             *   the entry has a UINT2 for the unicode value, followed by a
             *   UINT2 for the local character code point 
             */
            oswp2(valbuf, make_unicode(pagenum, i));
            oswp2(valbuf + 2, entp->mapping.local_char);

            /* write it out */
            if (osfwb(fp, valbuf, 4))
            {
                printf("error writing mapping to file\n");
                fatal_err = TRUE;
                goto done;
            }
        }
    }
    
    /*
     *   Compile information on the display mappings.  We must determine the
     *   number of display mappings we have, and the number of bytes we'll
     *   need for the display mappings in the output file.
     *   
     *   Every entry is a display mapping, since a round-trip mapping
     *   provides a unicode-to-local mapping.
     *   
     *   For a round-trip mapping, we simply store the one-byte or two-byte
     *   local character sequence given by the local mapping.
     *   
     *   For a display-only mapping, we must scan the expansion, which is a
     *   list of unicode characters.  For each unicode character in the
     *   expansion, we must find the round-trip mapping for that unicode
     *   character to get its local mapping, and then count the one-byte or
     *   two-byte local character sequence for that round-trip mapping.
     *   
     *   Note that each entry stores a one-byte length prefix, so we must
     *   count one additional byte per entry for this prefix.  
     */
    for (disp_count = 0, disp_bytes = 0, pagenum = 0 ;
         pagenum < 256 ; ++pagenum)
    {
        /* if there's no page here, skip it */
        if (G_mapping[pagenum] == 0)
            continue;

        /* go through the mappings in this page */
        for (i = 0 ; i < 256 ; ++i)
        {
            size_t local_len;
            
            /* get the mapping */
            entp = G_mapping[pagenum][i];

            /* if we don't have a mapping here, skip this unicode point */
            if (entp == 0)
                continue;

            /* count the entry */
            ++disp_count;

            /* count the length-prefix byte, which all entries have */
            ++disp_bytes;

            /* we don't have any bytes in the local length yet */
            local_len = 0;

            /* check the mapping type */
            if (entp->disp_only)
            {
                size_t idx;
                unsigned short *expp;
                
                /*
                 *   This is a display-only mapping, so we have a list of
                 *   unicode characters that we substitute for this unicode
                 *   character, then translate to the local character set
                 *   for display.  Scan the expansion list and include the
                 *   bytes for each character in the expansion list. 
                 */
                for (idx = 0, expp = entp->mapping.disp.expansion ;
                     idx < entp->mapping.disp.exp_len ;
                     ++idx, ++expp)
                {
                    unsigned short uc;
                    entity_map_t *entp_uc;
                    
                    /* get this unicode expansion character */
                    uc = *expp;

                    /* look up the mapping for this expansion character */
                    entp_uc = get_mapping(uc);

                    /* 
                     *   if we don't have a mapping, or it's not a
                     *   round-trip mapping, complain; otherwise, count the
                     *   local character expansion in our output length 
                     */
                    if (entp_uc == 0)
                    {
                        /* the unicode character is unmapped */
                        printf("display mapping for unicode character 0x%X "
                               "refers to unmapped unicode character 0x%X "
                               "in its expansion\n",
                               (unsigned)make_unicode(pagenum, i),
                               (unsigned)uc);

                        /* note the error */
                        fatal_err = TRUE;

                        /* don't scan any further in this expansion */
                        break;
                    }
                    else if (entp_uc->disp_only)
                    {
                        /* 
                         *   the unicode character itself has a display
                         *   mapping - recursive display mappings are not
                         *   allowed 
                         */
                        printf("display mapping for unicode character 0x%X "
                               "refers to unicode character 0x%X, which "
                               "itself has a display mapping - a display "
                               "mappings must refer only to round-trip "
                               "characters in its expansion\n",
                               (unsigned)make_unicode(pagenum, i),
                               (unsigned)uc);

                        /* note the error */
                        fatal_err = TRUE;

                        /* don't scan any further in this expansion */
                        break;
                    }
                    else
                    {
                        /* add this local mapping to the total length */
                        local_len +=
                            local_to_buf(0, entp_uc->mapping.local_char);
                    }
                }
            }
            else
            {
                /* 
                 *   this is a round-trip mapping, so we simply store the
                 *   one or two bytes given for the local character in the
                 *   mapping 
                 */
                local_len = local_to_buf(0, entp->mapping.local_char);
            }

            /* 
             *   add local length of this mapping to the total display bytes
             *   we've computed so far 
             */
            disp_bytes += local_len;

            /* 
             *   store the local length of this mapping for easy reference
             *   when we write out the mapping 
             */
            entp->local_len = local_len;
        }
    }

    /* if we have a default character, add it to the display mapping count */
    if (default_char_set)
    {
        /* count an additional display mapping */
        ++disp_count;

        /* add the expansion bytes, including the length prefix */
        disp_bytes += 1 + local_to_buf(0, default_char);
    }

    /* if we found errors compiling the display mappings, abort */
    if (fatal_err)
        goto done;

    /* 
     *   Write out the number of display mappings, and the number of bytes
     *   of display mappings.  
     */
    oswp2(valbuf, disp_count);
    oswp4(valbuf + 2, disp_bytes);
    if (osfwb(fp, valbuf, 6))
    {
        printf("error writing display translation header\n");
        fatal_err = TRUE;
        goto done;
    }

    /* check for a default display mapping */
    if (default_char_set)
    {
        size_t len;
        
        /* 
         *   Write out the default display entity.  We store this mapping at
         *   the translation for Unicode code point zero. 
         */
        oswp2(valbuf, 0);
        len = local_to_buf(valbuf + 3, default_char);
        valbuf[2] = (uchar)len;

        /* write it out */
        if (osfwb(fp, valbuf, 3 + len))
        {
            printf("error writing default_display mapping to output file\n");
            fatal_err = TRUE;
            goto done;
        }
    }
    else
    {
        printf("error: no 'default_display' mapping specified\n");
        fatal_err = TRUE;
        goto done;
    }

    /* 
     *   write all of the mappings - both the display and round-trip
     *   mappings provide unicode-to-local mappings 
     */

    /* go through all of the pages */
    for (pagenum = 0 ; pagenum < 256 ; ++pagenum)
    {
        /* if this page isn't present, skip it */
        if (G_mapping[pagenum] == 0)
            continue;
        
        /* go through the page */
        for (i = 0 ; i < 256 ; ++i)
        {
            size_t len;

            /* get the mapping */
            entp = G_mapping[pagenum][i];

            /* if the mapping doesn't exist, skip it */
            if (entp == 0)
                continue;
                
            /* write out this entity's unicode value and local length */
            oswp2(valbuf, make_unicode(pagenum, i));
            valbuf[2] = (uchar)entp->local_len;
            if (osfwb(fp, valbuf, 3))
            {
                printf("error writing display mapping to output file\n");
                break;
            }

            /* check the mapping type */
            if (entp->disp_only)
            {
                size_t idx;
                unsigned short *expp;
                
                /* 
                 *   display-only mapping - translate the local expansion
                 *   into unicode character 
                 */
                for (idx = 0, expp = entp->mapping.disp.expansion ;
                     idx < entp->mapping.disp.exp_len ;
                     ++idx, ++expp)
                {
                    unsigned short uc;
                    entity_map_t *entp_uc;
                    
                    /* get this unicode expansion character */
                    uc = *expp;
                    
                    /* look up the mapping for this expansion character */
                    entp_uc = get_mapping(uc);

                    /* write this character's local mapping */
                    len = local_to_buf(valbuf, entp_uc->mapping.local_char);
                    if (osfwb(fp, valbuf, len))
                    {
                        printf("error writing display mapping to file\n");
                        fatal_err = TRUE;
                        goto done;
                    }
                }
            }
            else
            {
                /* it's a round-trip mapping - store the local mapping */
                len = local_to_buf(valbuf, entp->mapping.local_char);
                if (osfwb(fp, valbuf, len))
                {
                    printf("error writing display mapping to file\n");
                    fatal_err = TRUE;
                    goto done;
                }
            }
        }
    }

    /*
     *   Write out the display-only expansions.  This is different than the
     *   display mappings we just wrote in that this time we're going to
     *   write out the *unicode* expansions for any sequences that turn into
     *   multiple displayed unicode characters for a single unicode input
     *   character.
     */
    oswp2(valbuf, disp_exp_cnt);
    oswp4(valbuf + 2, disp_exp_len);
    if (osfwb(fp, valbuf, 6))
    {
        printf("error writing display expansion to file\n");
        fatal_err = TRUE;
        goto done;
    }

    /* scan the pages and write out the expansions */
    for (pagenum = 0 ; pagenum < 256 ; ++pagenum)
    {
        /* if this page isn't present, skip it */
        if (G_mapping[pagenum] == 0)
            continue;

        /* go through the page */
        for (i = 0 ; i < 256 ; ++i)
        {
            size_t idx;

            /* get the mapping */
            entp = G_mapping[pagenum][i];

            /* 
             *   if the mapping doesn't exist, or it's not a display-only
             *   mapping, or it doesn't expand to multiple unicode
             *   characters, skip it 
             */
            if (entp == 0
                || !entp->disp_only
                || entp->mapping.disp.exp_len < 2)
                continue;

            /* write the unicode value and the expansion length */
            oswp2(valbuf, make_unicode(pagenum, i));
            valbuf[2] = (unsigned char)entp->mapping.disp.exp_len;
            if (osfwb(fp, valbuf, 3))
            {
                printf("error writing display expansion to file\n");
                fatal_err = TRUE;
                goto done;
            }

            /* write the expansion characters */
            for (idx = 0 ; idx < entp->mapping.disp.exp_len ; ++idx)
            {
                oswp2(valbuf, entp->mapping.disp.expansion[idx]);
                if (osfwb(fp, valbuf, 2))
                {
                    printf("error writing display expansion to file\n");
                    fatal_err = TRUE;
                    goto done;
                }
            }
        }
    }

    /* write out the end-of-file marker */
    if (osfwb(fp, "$EOF", 4))
        printf("error writing end-of-file marker\n");

done:
    /* delete the entity list */
    for (pagenum = 0 ; pagenum < 256 ; ++pagenum)
    {
        /* if there's no mapping here, ignore it */
        if (G_mapping[pagenum] == 0)
            continue;

        /* delete each entry on the page */
        for (i = 0 ; i < 256 ; ++i)
        {
            if (G_mapping[pagenum][i] != 0)
                t3free(G_mapping[pagenum][i]);
        }

        /* delete the page itself */
        t3free(G_mapping[pagenum]);
    }

    /* done with the output file */
    osfcls(fp);

    /* if we had a fatal error, delete the file */
    if (fatal_err)
        osfdel(outfile);

    /* success */
    return 0;
}

