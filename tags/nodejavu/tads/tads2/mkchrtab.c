#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/mkchrtab.c,v 1.2 1999/05/17 02:52:14 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  mkchrtab.c - TADS character table generator
Function
  
Notes
  
Modified
  05/31/98 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "os.h"
#include "cmap.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/*
 *   Read a number.  Returns zero on success, non-zero on error.
 */
static int read_number(unsigned int *result, char **p,
                       char *infile, int linenum, int is_char)
{
    unsigned int base;
    unsigned int acc;
    int digcnt;
    
    /* skip any leading spaces */
    while (isspace(**p))
        ++(*p);

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
        *result = (unsigned int)c;
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
            unsigned int dig;
            
            /* get this digit's value */
            dig = (unsigned int)(isdigit(**p)
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

    /* make sure the result isn't too large */
    if ((is_char && acc > (unsigned int)(unsigned char)0xff)
        || acc > (unsigned int)0xffff)
    {
        printf("%s: line %d: value out of range\n", infile, linenum);
        return 2;
    }

    /* return the accumulated value */
    *result = acc;

    /* success */
    return 0;
}


/*
 *   Check to see if an identifier matches a given string 
 */
static int id_matches(const char *idp, size_t idlen, const char *str)
{
    return (idlen == strlen(str) && memicmp(idp, str, idlen) == 0);
}

/*
 *   HTML Entity mapping structure 
 */
struct entity_t
{
    const char *ename;
    unsigned int charval;
};

/*
 *   List of HTML TADS entity names and the corresponding character codes 
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
typedef struct entity_map_t entity_map_t;
struct entity_map_t
{
    /* next mapping entry in the list */
    entity_map_t *nxt;

    /* HTML entity character code (Unicode value) */
    unsigned int html_char;

    /* length of the expansion */
    size_t exp_len;

    /* local character set expansion */
    unsigned char expansion[1];
};

/*
 *   Parse an entity name mapping 
 */
static entity_map_t *parse_entity(char *p, char *infile, int linenum)
{
    const char *start;
    const struct entity_t *entp;
    unsigned char buf[CMAP_MAX_ENTITY_EXPANSION];
    unsigned char *dstp;
    entity_map_t *mapp;
    
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

    /* skip any intervening whitespace and check for the '=' sign */
    for ( ; isspace(*p) ; ++p) ;
    if (*p != '=')
    {
        printf("%s: line %d: expected '=' after entity name\n",
               infile, linenum);
        return 0;
    }

    /* skip the '=' */
    ++p;

    /* read the series of numbers */
    for (dstp = buf ; *p != '\0' ; )
    {
        unsigned int intval;
        unsigned char c;

        /* skip any leading whitespace */
        while (isspace(*p))
            ++p;

        /* if it's the end of the line or a comment, stop */
        if (*p == '\0' || *p == '#')
            break;

        /* scan the character code */
        if (read_number(&intval, &p, infile, linenum, TRUE))
            return 0;
        c = (unsigned char)intval;

        /* make sure we haven't overflowed our buffer */
        if (dstp >= buf + sizeof(buf))
        {
            printf("%s: line %d: entity mapping is too long (maximum of %d "
                   "characters are allowed\n",
                   infile, linenum, sizeof(buf));
            return 0;
        }

        /* add it to the output list */
        *dstp++ = c;
    }

    /* if we didn't get any characters, it's an error */
    if (dstp == buf)
    {
        printf("%s: line %d: no local character codes found after '='\n",
               infile, linenum);
        return 0;
    }

    /* create a new mapping structure and set it up */
    mapp = (entity_map_t *)malloc(sizeof(entity_map_t) + dstp - buf);
    mapp->nxt = 0;
    mapp->html_char = entp->charval;
    mapp->exp_len = dstp - buf;
    memcpy(mapp->expansion, buf, dstp - buf);

    /* return the new mapping structure */
    return mapp;
}


/*
 *   Unicode mapping table entry 
 */
typedef struct unicode_map_t unicode_map_t;
struct unicode_map_t
{
    /* corresponding Unicode code point */
    unsigned int unicode_val;
};

/*
 *   Generate a mapping table by associating two Unicode mapping tables 
 */
static void gen_unicode_mapping(unicode_map_t map_from[256],
                                unicode_map_t map_to[256],
                                unsigned char result_fwd[256],
                                unsigned char result_fwd_set[256],
                                unsigned char result_rev[256],
                                unsigned char result_rev_set[256])
{
    int i;
    
    /*
     *   For each item in the 'from' map, find the entry in the 'to' map
     *   with the same Unicode value.  This gives the value that we store
     *   in the result table at the 'from' character index location. 
     */
    for (i = 0 ; i < 256 ; ++i)
    {
        int j;
        
        /* find this 'from' Unicode value in the 'to' table */
        for (j = 0 ; j < 256 ; ++j)
        {
            /* if they match, map it */
            if (map_to[j].unicode_val == map_from[i].unicode_val)
            {
                /* set the forward mapping, if it's not already set */
                if (!result_fwd_set[i])
                {
                    result_fwd[i] = j;
                    result_fwd_set[i] = TRUE;
                }

                /* set the reverse mapping, if it's not already set */
                if (!result_rev_set[j])
                {
                    result_rev[j] = i;
                    result_rev_set[j] = TRUE;
                }

                /* no need to look any further */
                break;
            }
        }
    }
}

/*
 *   Load a Unicode mapping file 
 */
static void load_unicode_file(char *filename, unicode_map_t map[256],
                              char *infile, int linenum)
{
    osfildef *fp;
    int linenum_u;
        
    /* open the unicode file */
    fp = osfoprs(filename, OSFTTEXT);
    if (fp == 0)
    {
        printf("%s: line %d: unable to open unicode mapping file \"%s\"\n",
               infile, linenum, filename);
        return;
    }

    /* read it */
    for (linenum_u = 1 ;; ++linenum_u)
    {
        char buf[256];
        char *p;
        unsigned int n1;
        unsigned int n2;

        /* read the next line */
        if (osfgets(buf, sizeof(buf), fp) == 0)
            break;

        /* skip leading spaces */
        for (p = buf ; isspace(*p) ; ++p) ;

        /* if it's blank or starts with a comment, ignore it */
        if (*p == 0x1a || *p == '#' || *p == '\n' || *p == '\r' || *p == '\0')
            continue;

        /* read the first number - this is the native character value */
        if (read_number(&n1, &p, filename, linenum_u, TRUE))
            break;

        /* read the second number */
        if (read_number(&n2, &p, filename, linenum_u, FALSE))
            break;

        /* set the association */
        map[n1].unicode_val = n2;
    }

    /* done with the file */
    osfcls(fp);
}

/*
 *   Generate the HTML named entity mappings for the native character set 
 */
static void gen_unicode_html_mapping(unicode_map_t map_native[256],
                                     entity_map_t **entity_first,
                                     entity_map_t **entity_last)
{
    unsigned int i;
    
    /* go through the native characters and find the named entity for each */
    for (i = 0 ; i < 255 ; ++i)
    {
        const struct entity_t *entity;
        
        /* 
         *   scan the named entity table to find an entity with the same
         *   Unicode value as this native character's Unicode mapping 
         */
        for (entity = entities ; entity->ename != 0 ; ++entity)
        {
            /* if this one matches the Unicode value, map it */
            if (entity->charval == map_native[i].unicode_val)
            {
                entity_map_t *mapp;

                /* create a new mapping structure */
                mapp = (entity_map_t *)malloc(sizeof(entity_map_t) + 1);

                /* set it up */
                mapp->nxt = 0;
                mapp->html_char = map_native[i].unicode_val;
                mapp->exp_len = 1;
                mapp->expansion[0] = (unsigned char)i;

                /* 
                 *   link it at the head of the list, so that any explicit
                 *   mapping specified by the user for the same entity
                 *   already or subsequently will override it (this will
                 *   happen because the users's entries always go at the
                 *   end of the list) 
                 */
                mapp->nxt = *entity_first;
                if (*entity_last == 0)
                    *entity_last = mapp;
                *entity_first = mapp;
                
                /* no need to look any further */
                break;
            }
        }
    }
}


/*
 *   Parse Unicode mapping files 
 */
static void parse_unicode_files(char *val, size_t vallen,
                                char *infile, int linenum,
                                unsigned char input_map[256],
                                unsigned char input_map_set[256],
                                unsigned char output_map[256],
                                unsigned char output_map_set[256],
                                entity_map_t **entity_first,
                                entity_map_t **entity_last)
{
    char fn_native[OSFNMAX];
    char fn_internal[OSFNMAX];
    char *dst;
    unicode_map_t map_native[256];
    unicode_map_t map_internal[256];
    
    /* retrieve the filenames */
    fn_native[0] = '\0';
    fn_internal[0] = '\0';
    while (vallen != 0)
    {
        char *id;
        size_t idlen;
        char qu;

        /* if we're at a comment, we're done */
        if (*val == '#')
            break;
        
        /* find the end of the current identifier */
        for (id = val ; vallen > 0 && isalpha(*val) ; ++val, --vallen) ;
        idlen = val - id;

        /* see what we have */
        if (id_matches(id, idlen, "native"))
            dst = fn_native;
        else if (id_matches(id, idlen, "internal"))
            dst = fn_internal;
        else
        {
            printf("%s: line %d: expected 'internal' or 'native', "
                   "found '%.*s'\n", infile, linenum, idlen, id);
            return;
        }

        /* if the current name has already been matched, it's an error */
        if (*dst != '\0')
        {
            printf("%s: line %d: '%.*s' specified more than once\n",
                   infile, linenum, idlen, id);
            return;
        }

        /* scan ahead to the '=' */
        while (vallen > 0 && isspace(*val))
            ++val, --vallen;

        /* make sure we have the '=' */
        if (vallen == 0 || *val != '=')
        {
            printf("%s: line %d: expected '=' after '%.*s'\n",
                   infile, linenum, idlen, id);
            return;
        }

        /* skip intervening spaces */
        ++val, --vallen;
        while (vallen > 0 && isspace(*val))
            ++val, --vallen;

        /* make sure we have a filename */
        if (vallen == 0)
        {
            printf("%s: line %d: expected filename after '='\n",
                   infile, linenum);
            return;
        }

        /* check for a quote */
        if (*val == '"' || *val == '\'')
        {
            /* remember the quote */
            qu = *val;

            /* skip it */
            ++val, --vallen;
        }
        else
        {
            /* we have no quote */
            qu = '\0';
        }

        /* scan the filename */
        for ( ; vallen > 0 ; ++val, --vallen)
        {
            /* check for a quote */
            if (qu != '\0' && *val == qu)
            {
                /* skip the quote */
                ++val;
                --vallen;
                
                /* 
                 *   if it's stuttered, keep a single copy and keep going;
                 *   otherwise, we're done 
                 */
                if (vallen > 1 && *(val+1) == qu)
                {
                    /* just skip the first quote, and keep the second */
                }
                else
                {
                    /* that's the matching close quote - we're done */
                    break;
                }
            }

            /* copy this character */
            *dst++ = *val;
        }

        /* null-terminate the filename */
        *dst = '\0';

        /* skip trailing spaces */
        while (vallen > 0 && isspace(*val))
            ++val, --vallen;
    }

    /* make sure we got both filenames */
    if (fn_internal[0] == '\0' || fn_native[0] == '\0')
    {
        printf("%s: line %d: must specify both 'native' and 'internal'"
               " with 'unicode'\n", infile, linenum);
        return;
    }

    /* load the two files */
    load_unicode_file(fn_internal, map_internal, infile, linenum);
    load_unicode_file(fn_native, map_native, infile, linenum);

    /* generate the forward and reverse mappings */
    gen_unicode_mapping(map_native, map_internal, input_map, input_map_set,
                        output_map, output_map_set);
    gen_unicode_mapping(map_internal, map_native, output_map, output_map_set,
                        input_map, input_map_set);

    /* generate the HTML named entity mappings */
    gen_unicode_html_mapping(map_native, entity_first, entity_last);
}


/*
 *   Main entrypoint 
 */
int main(int argc, char **argv)
{
    int curarg;
    unsigned char input_map[256];
    unsigned char output_map[256];
    unsigned char input_map_set[256];
    unsigned char output_map_set[256];
    unsigned char *p;
    int i;
    osfildef *fp;
    char *infile;
    char *outfile;
    int linenum;
    static char sig[] = CMAP_SIG_S100;
    int strict_mode = FALSE;
    char id[5];
    char ldesc[CMAP_LDESC_MAX_LEN + 1];
    size_t len;
    unsigned char lenbuf[2];
    char *sys_info;
    entity_map_t *entity_first;
    entity_map_t *entity_last;

    /* no parameters have been specified yet */
    memset(id, 0, sizeof(id));
    ldesc[0] = '\0';
    sys_info = 0;

    /* we have no entities in our entity mapping list yet */
    entity_first = entity_last = 0;

    /* scan options */
    for (curarg = 1 ; curarg < argc && argv[curarg][0] == '-' ; ++curarg)
    {
        if (!stricmp(argv[curarg], "-strict"))
        {
            /* they want extra warnings */
            strict_mode = TRUE;
        }
        else
        {
            /* consume all remaining options so we get a usage message */
            curarg = argc;
            break;
        }
    }

    /* check for required arguments */
    if (curarg + 1 >= argc)
    {
        printf("usage: mkchrtab [options] <source> <dest>\n"
               "  <source> is the input file\n"
               "  <dest> is the output file\n"
               "Options:\n"
               "  -strict   warn if any codes 128-255 are unassigned\n");
#if 0
/* 
 *   The information about what goes in the file made the message way too
 *   long, so this has been removed.  Users will want to the documentation
 *   instead of the usage message for information this detailed, so it
 *   didn't seem useful to keep it in here.  
 */
        printf("\n"
               "The source file contains one entry per line, as follows:\n"
               "\n"
               "Set the internal character set identifier, which can be up "
               "to four letters long\n"
               "(note that the mapping file MUST contain an ID entry):\n"
               "   ID = id\n"
               "\n");
        printf("Set the internal character set's full display name:\n"
               "   LDESC = full name of character set\n"
               "\n"
               "Set system-dependent extra information (the meaning varies "
               "by system):\n"
               "   EXTRA_SYSTEM_INFO = info-string\n"
               "\n"
               "Set the native default character:\n"
               "   NATIVE_DEFAULT = charval\n"
               "Set the internal default character:\n"
               "   INTERNAL_DEFAULT = charval\n");
        printf("Load Unicode mapping files:\n"
               "   UNICODE NATIVE=native-mapping INTERNAL=internal-mapping\n"
               "\n"
               "Reversibly map a native character code to an internal code:\n"
               "   native <-> internal\n"
               "\n"
               "Map a native code to an internal code, and map the internal "
               "code back\nto a different native code:\n"
               "   native -> internal -> native\n"
               "\n"
               "Map a native code to an internal code, where the internal "
               "code is already\nmapped to a native code by a previous line:\n"
               "   native -> internal\n"
               "\n");
        printf("Map an internal code to a native code, where the native "
               "code is already\nmapped to an internal code by a previous "
               "line:\n"
               "   native <- internal\n"
               "\n"
               "Map an HTML entity name to a native code or string:\n"
               "   &entity = internal-code [internal-code ...]\n"
               "\n"
               "Numbers can be specified in decimal (default), octal (by "
               "prefixing the number\nwith a zero, as in '037'), or hex (by "
               "prefixing the number with '0x', as in\n'0xb2').  A number "
               "can also be entered as a character by enclosing the\n"
               "character in single quotes.\n"
               "\n"
               "Blank lines and lines starting with a pound sign ('#') are "
               "ignored.\n");
#endif /* 0 */
        os_term(OSEXFAIL);
    }

    /* get the input and output filenames */
    infile = argv[curarg];
    outfile = argv[curarg + 1];

    /* 
     *   initialize the tables - by default, a character code in one
     *   character set maps to the same code in the other character set 
     */
    for (p = input_map, i = 0 ; i < sizeof(input_map)/sizeof(input_map[0]) ;
         ++i, ++p)
        *p = (unsigned char)i;

    for (p = output_map, i = 0 ;
         i < sizeof(output_map)/sizeof(output_map[0]) ; ++i, ++p)
        *p = (unsigned char)i;

    /* 
     *   initialize the "set" flags all to false, since we haven't set any
     *   of the values yet -- we'll use these flags to detect when the
     *   user attempts to set the same value more than once, so that we
     *   can issue a warning (multiple mappings are almost certainly in
     *   error) 
     */
    for (i = 0 ; i < sizeof(input_map_set)/sizeof(input_map_set[0]) ; ++i)
        input_map_set[i] = output_map_set[i] = FALSE;

    /* open the input file */
    fp = osfoprs(infile, OSFTTEXT);
    if (fp == 0)
    {
        printf("error: unable to open input file \"%s\"\n", infile);
        os_term(OSEXFAIL);
    }

    /* parse the input file */
    for (linenum = 1 ; ; ++linenum)
    {
        char buf[256];
        char *p;
        unsigned int n1, n2, n3;
        int set_input;
        int set_output;

        /* presume we're going to set both values */
        set_input = set_output = TRUE;

        /* read the next line */
        if (osfgets(buf, sizeof(buf), fp) == 0)
            break;

        /* scan off leading spaces */
        for (p = buf ; isspace(*p) ; ++p) ;

        /* if this line is blank, or starts with a '#', ignore it */
        if (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#')
            continue;

        /* check for special directives */
        if (isalpha(*p) || *p == '_')
        {
            char *sp;
            char *val;
            size_t vallen;
            size_t idlen;
            
            /* find the end of the directive name */
            for (sp = p ; isalpha(*sp) || *sp == '_' ; ++sp) ;
            idlen = sp - p;

            /* find the equals sign, if present */
            for (val = sp ; isspace(*val) ; ++val) ;
            if (*val == '=')
            {
                /* skip the '=' and any spaces that follow */
                for (++val ; isspace(*val) ; ++val) ;

                /* find the end of the value */
                for (sp = val ; *sp != '\n' && *sp != '\r' && *sp != '\0' ;
                     ++sp) ;

                /* note its length */
                vallen = sp - val;
            }
            else
            {
                /* there's no value */
                val = 0;
            }

            /* see what we have */
            if (id_matches(p, idlen, "id"))
            {
                /* this directive requires a value */
                if (val == 0)
                    goto val_required;

                /* ID's can never be more than four characters long */
                if (vallen > 4)
                {
                    printf("%s: line %d: ID too long - no more than four "
                           "characters are allowed\n", infile, linenum);
                }
                else
                {
                    /* remember the ID */
                    memcpy(id, val, vallen);
                    id[vallen] = '\0';
                }
            }
            else if (id_matches(p, idlen, "ldesc"))
            {
                /* this directive requires a value */
                if (val == 0)
                    goto val_required;

                /* make sure it fits */
                if (vallen > sizeof(ldesc) - 1)
                {
                    printf("%s: line %d: LDESC too long - no more than %u "
                           "characters are allowed\n", infile, linenum,
                           sizeof(ldesc) - 1);
                }
                else
                {
                    /* remember the ldesc */
                    memcpy(ldesc, val, vallen);
                    ldesc[vallen] = '\0';
                }
            }
            else if (id_matches(p, idlen, "extra_system_info"))
            {
                /* this directive requires a value */
                if (val == 0)
                    goto val_required;

                /* allocate space for it */
                sys_info = (char *)malloc(vallen + 1);
                memcpy(sys_info, val, vallen);
                sys_info[vallen] = '\0';
            }
            else if (id_matches(p, idlen, "native_default"))
            {
                unsigned int nval;
                int i;

                /* this directive requires a value */
                if (val == 0)
                    goto val_required;

                /* parse the character value */
                if (read_number(&nval, &val, infile, linenum, TRUE))
                    continue;

                /* apply the default */
                for (i = 128 ; i < 256 ; ++i)
                {
                    /* set the default only if we haven't mapped this one */
                    if (!output_map_set[i])
                        output_map[i] = nval;
                }
            }
            else if (id_matches(p, idlen, "internal_default"))
            {
                unsigned int nval;
                int i;

                /* this directive requires a value */
                if (val == 0)
                    goto val_required;

                /* parse the character value */
                if (read_number(&nval, &val, infile, linenum, TRUE))
                    continue;

                /* apply the default */
                for (i = 128 ; i < 256 ; ++i)
                {
                    /* apply the default only if we haven't set this one */
                    if (!input_map_set[i])
                        input_map[i] = nval;
                }
            }
            else if (id_matches(p, idlen, "unicode"))
            {
                /* skip the 'unicode' string and any intervening spaces */
                for (p += idlen ; isspace(*p) ; ++p) ;

                /* parse the unicode files */
                parse_unicode_files(p, strlen(p), infile, linenum,
                                    input_map, input_map_set,
                                    output_map, output_map_set,
                                    &entity_first, &entity_last);
            }
            else
            {
                /* unknown directive */
                printf("%s: line %d: invalid directive '%.*s'\n",
                       infile, linenum, idlen, p);
            }

            /* done processing this line */
            continue;

            /* come here if the directive needs a value and there isn't one */
        val_required:
            printf("%s: line %d: '=' required with directive '%.*s'\n",
                   infile, linenum, idlen, p);
            continue;
        }

        /* check for an entity name */
        if (*p == '&')
        {
            entity_map_t *mapp;

            /* skip the '&' */
            ++p;

            /* 
             *   parse the entity - if it succeeds, link the resulting
             *   mapping entry into our list 
             */
            mapp = parse_entity(p, infile, linenum);
            if (mapp != 0)
            {
                if (entity_last == 0)
                    entity_first = mapp;
                else
                    entity_last->nxt = mapp;
                entity_last = mapp;
            }

            /* done */
            continue;
        }

        /* read the first number */
        if (read_number(&n1, &p, infile, linenum, TRUE))
            continue;

        /* determine which operator we have */
        if (*p == '<')
        {
            /* make sure it's "<->" or "<-" */
            if (*(p+1) == '-' && *(p+2) != '>')
            {
                /* skip the operator */
                p += 2;

                /* 
                 *   This is a "from" translation - it only affects the
                 *   output mapping from the internal character set to the
                 *   native character set.  Read the second number.  There
                 *   is no third number, since we don't want to change the
                 *   input mapping.
                 */
                if (read_number(&n2, &p, infile, linenum, TRUE))
                    continue;

                /* 
                 *   The forward translation is not affected; set only the
                 *   output translation.  Note that the first number was
                 *   the output (native) value for the internal index in
                 *   the second number, so move the first value to n3.  
                 */
                n3 = n1;
                set_input = FALSE;
            }
            else if (*(p+1) == '-' && *(p+2) == '>')
            {
                /* skip it */
                p += 3;

                /* 
                 *   this is a reversible translation, so we only need one
                 *   more number - the third number is implicitly the same
                 *   as the first 
                 */
                n3 = n1;
                if (read_number(&n2, &p, infile, linenum, TRUE))
                    continue;
            }
            else
            {
                printf("%s: line %d: invalid operator - expected <->\n",
                       infile, linenum);
                continue;
            }
        }
        else if (*p == '-')
        {
            /* make sure it's "->" */
            if (*(p+1) != '>')
            {
                printf("%s: line %d: invalid operator - expected ->\n",
                       infile, linenum);
                continue;
            }

            /* skip it */
            p += 2;

            /* get the next number */
            if (read_number(&n2, &p, infile, linenum, TRUE))
                continue;

            /* 
             *   we may or may not have a third number - if we have
             *   another -> operator, read the third number; if we don't,
             *   the reverse translation is not affected by this entry 
             */
            if (*p == '-')
            {
                /* make sure it's "->" */
                if (*(p+1) != '>')
                {
                    printf("%s: line %d: invalid operator - expected ->\n",
                           infile, linenum);
                    continue;
                }

                /* skip it */
                p += 2;

                /* read the third number */
                if (read_number(&n3, &p, infile, linenum, TRUE))
                    continue;
            }
            else
            {
                /*
                 *   There's no third number - the reverse translation is
                 *   not affected by this line.  
                 */
                set_output = FALSE;
            }
        }
        else
        {
            printf("%s: line %d: invalid operator - expected "
                   "-> or <-> or <-\n",
                   infile, linenum);
            continue;
        }

        /* make sure we're at the end of the line, and warn if not */
        if (*p != '\0' && *p != '\n' && *p != '\r' && *p != '#')
            printf("%s: line %d: extra characters at end of line ignored\n",
                   infile, linenum);

        /* set the input mapping, if necessary */
        if (set_input)
        {
            /* warn the user if this value has already been set before */
            if (input_map_set[n1])
                printf("%s: line %d: warning - native character %u has "
                       "already been\n    mapped to internal value %u\n",
                       infile, linenum, n1, input_map[n1]);
            
            /* set it */
            input_map[n1] = n2;

            /* note that it's been set */
            input_map_set[n1] = TRUE;
        }

        /* set the output mapping, if necessary */
        if (set_output)
        {
            /* warn the user if this value has already been set before */
            if (output_map_set[n2])
                printf("%s: line %d: warning - internal character %u has "
                       "already been\n    mapped to native value %u\n",
                       infile, linenum, n2, input_map[n2]);

            /* set it */
            output_map[n2] = n3;

            /* note that it's been set */
            output_map_set[n2] = TRUE;
        }
    }

    /* we're done with the input file */
    osfcls(fp);

    /*
     *   It's an error if we didn't get an ID or LDESC 
     */
    if (id[0] == '\0')
    {
        printf("Error: No ID was specified.  An ID is required.\n");
        os_term(OSEXFAIL);
    }
    else if (ldesc[0] == '\0')
    {
        printf("Error: No LDESC was specified.  An LDESC is required.\n");
        os_term(OSEXFAIL);
    }

    /* open the output file */
    fp = osfopwb(outfile, OSFTCMAP);
    if (fp == 0)
    {
        printf("error: unable to open output file \"%s\"\n", outfile);
        os_term(OSEXFAIL);
    }

    /* write our signature */
    if (osfwb(fp, sig, sizeof(sig)))
        printf("error writing signature to output file\n");

    /* write the ID and LDESC */
    len = strlen(ldesc) + 1;
    oswp2(lenbuf, len);
    if (osfwb(fp, id, 4)
        || osfwb(fp, lenbuf, 2)
        || osfwb(fp, ldesc, len))
        printf("error writing ID information to output file\n");

    /* write the mapping tables */
    if (osfwb(fp, input_map, sizeof(input_map))
        || osfwb(fp, output_map, sizeof(output_map)))
        printf("error writing character maps to output file\n");

    /* write the extra system information if present */
    if (sys_info != 0)
    {
        /* write it out, with the "SYSI" flag so we know it's there */
        len = strlen(sys_info) + 1;
        oswp2(lenbuf, len);
        if (osfwb(fp, "SYSI", 4)
            || osfwb(fp, lenbuf, 2)
            || osfwb(fp, sys_info, len))
            printf("error writing EXTRA_SYSTEM_INFO to output file\n");

        /* we're done with the allocated buffer now */
        free(sys_info);
    }

    /*
     *   Write the entity mapping list, if we have any entities 
     */
    if (entity_first != 0)
    {
        entity_map_t *entp;
        entity_map_t *next_entity;
        char lenbuf[2];
        char cvalbuf[2];

        /* write out the entity list header */
        if (osfwb(fp, "ENTY", 4))
            printf("error writing entity marker to output file\n");

        /* run through the list, writing out each entry */
        for (entp = entity_first ; entp != 0 ; entp = next_entity)
        {
            /* write out this entity */
            oswp2(lenbuf, entp->exp_len);
            oswp2(cvalbuf, entp->html_char);
            if (osfwb(fp, lenbuf, 2)
                || osfwb(fp, cvalbuf, 2)
                || osfwb(fp, entp->expansion, entp->exp_len))
            {
                printf("error writing entity mapping to output file\n");
                break;
            }

            /* remember the next entity before we delete this one */
            next_entity = entp->nxt;

            /* we're done with this entity, so we can delete it now */
            free(entp);
        }

        /* 
         *   write out the end marker, which is just a length marker and
         *   character marker of zero 
         */
        oswp2(lenbuf, 0);
        oswp2(cvalbuf, 0);
        if (osfwb(fp, lenbuf, 2)
            || osfwb(fp, cvalbuf, 2))
            printf("error writing entity list end marker to output file\n");
    }

    /* write the end-of-file marker */
    if (osfwb(fp, "$EOF", 4))
        printf("error writing end-of-file marker to output file\n");

    /* done with the output file */
    osfcls(fp);

    /* if we're in strict mode, check for unassigned mappings */
    if (strict_mode)
    {
        int in_cnt, out_cnt;
        int cnt;

        /* count unassigned characters */
        for (i = 128, in_cnt = out_cnt = 0 ; i < 256 ; ++i)
        {
            if (!input_map_set[i])
                ++in_cnt;
            if (!output_map_set[i])
                ++out_cnt;
        }

        /* if we have any unassigned native characters, list them */
        if (in_cnt != 0)
        {
            printf("\nUnassigned native -> internal mappings:\n    ");
            for (i = 128, cnt = 0 ; i < 256 ; ++i)
            {
                if (!input_map_set[i])
                {
                    /* go to a new line if necessary */
                    if (cnt >= 16)
                    {
                        printf("\n    ");
                        cnt = 0;
                    }

                    /* display this item */
                    printf("%3d ", i);
                    ++cnt;
                }
            }
            printf("\n");
        }

        /* list unassigned internal characters */
        if (out_cnt != 0)
        {
            printf("\nUnassigned internal -> native mappings:\n    ");
            for (i = 128, cnt = 0 ; i < 256 ; ++i)
            {
                if (!output_map_set[i])
                {
                    /* go to a new line if necessary */
                    if (cnt >= 16)
                    {
                        printf("\n    ");
                        cnt = 0;
                    }

                    /* display this item */
                    printf("%3d ", i);
                    ++cnt;
                }
            }
            printf("\n");
        }
    }

    /* success */
    os_term(OSEXSUCC);
    return OSEXSUCC;
}

