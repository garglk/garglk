#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/charmap.cpp,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  charmap.cpp - character mapper
Function
  
Notes
  
Modified
  10/17/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "os.h"
#include "utf8.h"
#include "resload.h"
#include "charmap.h"
#include "vmdatasrc.h"


/* ------------------------------------------------------------------------ */
/*
 *   Basic Mapper Class 
 */

/*
 *   match a name prefix (for the various Latin-X synonym names) 
 */
static int prefixeq(const char *table_name, size_t digit_ofs,
                    const char *prefix)
{
    return (digit_ofs == strlen(prefix)
            && memicmp(table_name, prefix, digit_ofs) == 0);
}

/*
 *   Open a mapping file, applying synonyms for character set names 
 */
osfildef *CCharmap::open_map_file_syn(class CResLoader *res_loader,
                                      const char *table_name,
                                      charmap_type_t *map_type)
{
    osfildef *fp;
    
    /* try opening the file/resource with the exact name given */
    if ((fp = open_map_file(res_loader, table_name, map_type)) != 0)
        return fp;
    
    /*     
     *   We didn't find a file with the exact name given.  Try various
     *   synonyms for the Latin-X character sets.  
     */
    const char *p;
    for (p = table_name + strlen(table_name) ;
         p > table_name && is_digit(*(p-1)) ; --p) ;
    size_t ofs = p - table_name;
    int num = atoi(p);
    int found_suffix = (ofs != strlen(p) && num != 0);
    if (found_suffix)
    {
        if (prefixeq(table_name, ofs, "latin")
            || prefixeq(table_name, ofs, "latin-")
            || prefixeq(table_name, ofs, "iso")
            || prefixeq(table_name, ofs, "iso-")
            || prefixeq(table_name, ofs, "8859-")
            || prefixeq(table_name, ofs, "iso8859-")
            || prefixeq(table_name, ofs, "iso-8859-")
            || prefixeq(table_name, ofs, "iso_8859-")
            || prefixeq(table_name, ofs, "iso_8859_")
            || prefixeq(table_name, ofs, "l"))
        {
            /* rebuild the name as "isoX" */
            char buf[25];
            sprintf(buf, "iso%d", num);
            
            /* try loading it */
            if ((fp = open_map_file(res_loader, buf, map_type)) != 0)
                return fp;
        }
    }

    /* 
     *   if that didn't work, and the whole string looks like just a number,
     *   try putting "cp" in front of it - i.e., allow loading cp1252 as just
     *   "1252" 
     */
    if (found_suffix && p == table_name)
    {
        /* it's all digits - try making it "cpXXXX" */
        char buf[25];
        sprintf(buf, "cp%d", num);
        if ((fp = open_map_file(res_loader, buf, map_type)) != 0)
            return fp;
    }
    
    /* 
     *   if we still don't have a file, check for "winXXXX" and "dosXXXX", as
     *   synonyms for cpXXXX 
     */
    if (found_suffix
        && (prefixeq(table_name, ofs, "win")
            || prefixeq(table_name, ofs, "win-")
            || prefixeq(table_name, ofs, "windows")
            || prefixeq(table_name, ofs, "windows-")
            || prefixeq(table_name, ofs, "dos")
            || prefixeq(table_name, ofs, "dos-")))
    {
        /* try making it "cpXXXX" */
        char buf[25];
        sprintf(buf, "cp%d", num);
        if ((fp = open_map_file(res_loader, buf, map_type)) != 0)
            return fp;
    }

    /* we couldn't find an alternative name; give up */
    return 0;
}

/*
 *   Open and characterize a mapping file 
 */
osfildef *CCharmap::open_map_file(class CResLoader *res_loader,
                                  const char *table_name,
                                  charmap_type_t *map_type)
{
    osfildef *fp;
    char respath[100];
    ulong startpos;
    uchar buf[256];
    uint entry_cnt;
    int found_single;
    int found_double;

    /*
     *   Generate the full resource path - character mapping resource paths
     *   always start with "charmap/" followed by the table name, plus the
     *   ".tcm" extension.  We use lower-case names for the mapping files, so
     *   explicitly convert to lower, in case we're on a case-sensitive file
     *   system like Unix.  
     */
    t3sprintf(respath, sizeof(respath), "charmap/%s.tcm", table_name);
    t3strlwr(respath);

    /* open the file for the character set */
    fp = res_loader->open_res_file(respath, "charmap/cmaplib", "CLIB");

    /* if we couldn't open the mapping file, return failure */
    if (fp == 0)
        return 0;

    /* note the initial seek position */
    startpos = osfpos(fp);

    /* read the header and the local-to-unicode header */
    if (osfrb(fp, buf, 6))
        goto fail;

    /* get the number of entries from the local-to-unicode header */
    entry_cnt = osrp2(buf + 4);

    /* 
     *   Scan the entries to determine if we have single-byte,
     *   double-byte, or both.  
     */
    found_single = found_double = FALSE;
    while (entry_cnt > 0)
    {
        size_t cur;
        const uchar *p;

        /* read up to a buffer-full or the remaining size */
        cur = sizeof(buf)/4;
        if (cur > entry_cnt)
            cur = entry_cnt;

        /* read it */
        if (osfrb(fp, buf, cur*4))
            goto fail;

        /* deduct the amount we just read from the amount remaining */
        entry_cnt -= cur;

        /* scan the entries */
        for (p = buf ; cur > 0 ; --cur, p += 4)
        {
            /* 
             *   Note whether this is a single-byte or double-byte entry.
             *   If the high-order byte is non-zero, it's a double-byte
             *   entry; otherwise, it's a single-byte entry.
             *   
             *   Note that we read the UINT2 at (p+2), because that's the
             *   local character-set code point in this tuple.  
             */
            if (((uint)osrp2(p + 2)) > 0xFF)
                found_double = TRUE;
            else
                found_single = TRUE;
        }

        /* 
         *   if we've found both single- and double-byte characters so
         *   far, there's no need to look any further, since we know
         *   everything about the file now 
         */
        if (found_single && found_double)
            break;
    }

    /* 
     *   create the appropriate mapper, depending on whether we found
     *   single, double, or mixed characters 
     */
    if (found_single && found_double)
    {
        /* it's mixed */
        *map_type = CHARMAP_TYPE_MB;
    }
    else if (found_double)
    {
        /* it's all double-byte */
        *map_type = CHARMAP_TYPE_DB;
    }
    else if (found_single)
    {
        /* it's all single-byte */
        *map_type = CHARMAP_TYPE_SB;
    }
    else
    {
        /* no mappings found at all - presume it's a single-byte mapper */
        *map_type = CHARMAP_TYPE_SB;
    }

    /* seek back to the start of the table */
    osfseek(fp, startpos, OSFSK_SET);

    /* return the file pointer */
    return fp;

fail:
    /* close the file and return failure */
    osfcls(fp);
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Special built-in mapper to 7-bit ASCII.  This is available as a last
 *   resort when no external mapping file can be found.  
 */

/*
 *   create a plain ascii translator 
 */
CCharmapToLocalASCII::CCharmapToLocalASCII()
{
    unsigned char *dst;
    wchar_t *exp_dst;
    size_t siz;
    size_t exp_siz;
    struct ascii_map_t
    {
        wchar_t uni;
        char asc[5];
    };
    ascii_map_t *p;
    static ascii_map_t ascii_mapping[] =
    {
        /* regular ASCII characters */
        { 1, { 1 } },
        { 2, { 2 } },
        { 3, { 3 } },
        { 4, { 4 } },
        { 5, { 5 } },
        { 6, { 6 } },
        { 7, { 7 } },
        { 8, { 8 } },
        { 9, { 9 } },
        { 10, { 10 } },
        { 11, { 11 } },
        { 12, { 12 } },
        { 13, { 13 } },
        { 14, { 14 } },
        { 15, { 15 } },
        { 16, { 16 } },
        { 17, { 17 } },
        { 18, { 18 } },
        { 19, { 19 } },
        { 20, { 20 } },
        { 21, { 21 } },
        { 22, { 22 } },
        { 23, { 23 } },
        { 24, { 24 } },
        { 25, { 25 } },
        { 26, { 26 } },
        { 27, { 27 } },
        { 28, { 28 } },
        { 29, { 29 } },
        { 30, { 30 } },
        { 31, { 31 } },
        { 32, { 32 } },
        { 33, { 33 } },
        { 34, { 34 } },
        { 35, { 35 } },
        { 36, { 36 } },
        { 37, { 37 } },
        { 38, { 38 } },
        { 39, { 39 } },
        { 40, { 40 } },
        { 41, { 41 } },
        { 42, { 42 } },
        { 43, { 43 } },
        { 44, { 44 } },
        { 45, { 45 } },
        { 46, { 46 } },
        { 47, { 47 } },
        { 48, { 48 } },
        { 49, { 49 } },
        { 50, { 50 } },
        { 51, { 51 } },
        { 52, { 52 } },
        { 53, { 53 } },
        { 54, { 54 } },
        { 55, { 55 } },
        { 56, { 56 } },
        { 57, { 57 } },
        { 58, { 58 } },
        { 59, { 59 } },
        { 60, { 60 } },
        { 61, { 61 } },
        { 62, { 62 } },
        { 63, { 63 } },
        { 64, { 64 } },
        { 65, { 65 } },
        { 66, { 66 } },
        { 67, { 67 } },
        { 68, { 68 } },
        { 69, { 69 } },
        { 70, { 70 } },
        { 71, { 71 } },
        { 72, { 72 } },
        { 73, { 73 } },
        { 74, { 74 } },
        { 75, { 75 } },
        { 76, { 76 } },
        { 77, { 77 } },
        { 78, { 78 } },
        { 79, { 79 } },
        { 80, { 80 } },
        { 81, { 81 } },
        { 82, { 82 } },
        { 83, { 83 } },
        { 84, { 84 } },
        { 85, { 85 } },
        { 86, { 86 } },
        { 87, { 87 } },
        { 88, { 88 } },
        { 89, { 89 } },
        { 90, { 90 } },
        { 91, { 91 } },
        { 92, { 92 } },
        { 93, { 93 } },
        { 94, { 94 } },
        { 95, { 95 } },
        { 96, { 96 } },
        { 97, { 97 } },
        { 98, { 98 } },
        { 99, { 99 } },
        { 100, { 100 } },
        { 101, { 101 } },
        { 102, { 102 } },
        { 103, { 103 } },
        { 104, { 104 } },
        { 105, { 105 } },
        { 106, { 106 } },
        { 107, { 107 } },
        { 108, { 108 } },
        { 109, { 109 } },
        { 110, { 110 } },
        { 111, { 111 } },
        { 112, { 112 } },
        { 113, { 113 } },
        { 114, { 114 } },
        { 115, { 115 } },
        { 116, { 116 } },
        { 117, { 117 } },
        { 118, { 118 } },
        { 119, { 119 } },
        { 120, { 120 } },
        { 121, { 121 } },
        { 122, { 122 } },
        { 123, { 123 } },
        { 124, { 124 } },
        { 125, { 125 } },
        { 126, { 126 } },
        { 127, { 127 } },

        /* Latin-1 accented characters and symbols */
        { 353, "s" },
        { 352, "S" },
        { 8218, "\'" },
        { 8222, "\"" },
        { 8249, "<" },
        { 338, "OE" },
        { 8216, "\'" },
        { 8217, "\'" },
        { 8220, "\"" },
        { 8221, "\"" },
        { 8211, "-" },
        { 8212, "--" },
        { 8482, "(tm)" },
        { 8250, ">" },
        { 339, "oe" },
        { 376, "Y" },
        { 162, "c" },
        { 163, "L" },
        { 165, "Y" },
        { 166, "|" },
        { 169, "(c)" },
        { 170, "a" },
        { 173, " " },
        { 174, "(R)" },
        { 175, "-" },
        { 177, "+/-" },
        { 178, "2" },
        { 179, "3" },
        { 180, "\'" },
        { 181, "u" },
        { 182, "P" },
        { 183, "*" },
        { 184, "," },
        { 185, "1" },
        { 186, "o" },
        { 171, "<<" },
        { 187, ">>" },
        { 188, "1/4" },
        { 189, "1/2" },
        { 190, "3/4" },
        { 192, "A" },
        { 193, "A" },
        { 194, "A" },
        { 195, "A" },
        { 196, "A" },
        { 197, "A" },
        { 198, "AE" },
        { 199, "C" },
        { 200, "E" },
        { 201, "E" },
        { 202, "E" },
        { 203, "E" },
        { 204, "I" },
        { 205, "I" },
        { 206, "I" },
        { 207, "I" },
        { 209, "N" },
        { 210, "O" },
        { 211, "O" },
        { 212, "O" },
        { 213, "O" },
        { 214, "O" },
        { 215, "x" },
        { 216, "O" },
        { 217, "U" },
        { 218, "U" },
        { 219, "U" },
        { 220, "U" },
        { 221, "Y" },
        { 223, "ss" },
        { 224, "a" },
        { 225, "a" },
        { 226, "a" },
        { 227, "a" },
        { 228, "a" },
        { 229, "a" },
        { 230, "ae" },
        { 231, "c" },
        { 232, "e" },
        { 233, "e" },
        { 234, "e" },
        { 235, "e" },
        { 236, "i" },
        { 237, "i" },
        { 238, "i" },
        { 239, "i" },
        { 241, "n" },
        { 242, "o" },
        { 243, "o" },
        { 244, "o" },
        { 245, "o" },
        { 246, "o" },
        { 247, "/" },
        { 248, "o" },
        { 249, "u" },
        { 250, "u" },
        { 251, "u" },
        { 252, "u" },
        { 253, "y" },
        { 255, "y" },
        { 710, "^" },
        { 732, "~" },

        /* math symbols */
        { 402, "f" },

        /* other symbols */
        { 8226, "*" },

        /* arrows */
        { 8592, "<-" },
        { 8594, "->" },

        /* several capital Greek letters look a lot like Roman letters */
        { 913, "A" },
        { 914, "B" },
        { 918, "Z" },
        { 919, "H" },
        { 921, "I" },
        { 922, "K" },
        { 924, "M" },
        { 925, "N" },
        { 927, "O" },
        { 929, "P" },
        { 932, "T" },
        { 933, "Y" },
        { 935, "X" },

        /* Latin-2 accented characters */
        { 260, "A" },
        { 321, "L" },
        { 317, "L" },
        { 346, "S" },
        { 350, "S" },
        { 356, "T" },
        { 377, "Z" },
        { 381, "Z" },
        { 379, "Z" },
        { 261, "a" },
        { 731, "o" },
        { 322, "l" },
        { 318, "l" },
        { 347, "s" },
        { 351, "s" },
        { 357, "t" },
        { 378, "z" },
        { 733, "\"" },
        { 382, "z" },
        { 380, "z" },
        { 340, "R" },
        { 258, "A" },
        { 313, "L" },
        { 262, "C" },
        { 268, "C" },
        { 280, "E" },
        { 282, "E" },
        { 270, "D" },
        { 272, "D" },
        { 323, "N" },
        { 327, "N" },
        { 336, "O" },
        { 344, "R" },
        { 366, "U" },
        { 368, "U" },
        { 354, "T" },
        { 341, "r" },
        { 259, "a" },
        { 314, "l" },
        { 263, "c" },
        { 269, "c" },
        { 281, "e" },
        { 283, "e" },
        { 271, "d" },
        { 273, "d" },
        { 324, "n" },
        { 328, "n" },
        { 337, "o" },
        { 345, "r" },
        { 367, "u" },
        { 369, "u" },
        { 355, "t" },
        { 0, { 0 } }
    };

    /* determine how much space we'll need in the translation array */
    for (p = ascii_mapping, siz = 0, exp_siz = 0 ; p->uni != 0 ; ++p)
    {
        /* we need space for this mapping string, plus a length prefix byte */
        siz += strlen(p->asc) + 1;

        /* 
         *   if this is a multi-character expansion, count it in the
         *   expansion array size 
         */
        if (strlen(p->asc) > 1)
            exp_siz += strlen(p->asc) + 1;
    }

    /* add in space for the default entry */
    siz += 2;

    /* allocate the translation array */
    xlat_array_ = (unsigned char *)t3malloc(siz);

    /* 
     *   allocate the expansion array; allocate one extra entry for the null
     *   mapping at index zero 
     */
    exp_array_ = (wchar_t *)t3malloc((exp_siz + 1) * sizeof(wchar_t));

    /* 
     *   start at element 1 of the expansion array (element zero is reserved
     *   to indicate the null mapping) 
     */
    dst = xlat_array_;
    exp_dst = exp_array_ + 1;

    /* 
     *   Add the zeroeth entry, which serves as the default mapping for
     *   characters that aren't otherwise mappable.  
     */
    set_mapping(0, 0);
    *dst++ = 1;
    *dst++ = '?';

    /* set up the arrays */
    for (p = ascii_mapping ; p->uni != 0 ; ++p)
    {
        size_t len;

        /* set the mapping's offset in the translation array */
        set_mapping(p->uni, dst - xlat_array_);

        /* get the length of this mapping */
        len = strlen(p->asc);

        /* set this mapping's length */
        *dst++ = (unsigned char)len;

        /* copy the mapping */
        memcpy(dst, p->asc, len);

        /* move past the mapping in the translation array */
        dst += len;

        /* add the expansion mapping if necessary */
        if (len > 1)
        {
            size_t i;

            /* add an expansion mapping */
            set_exp_mapping(p->uni, exp_dst - exp_array_);

            /* set the length prefix */
            *exp_dst++ = (wchar_t)len;

            /* add the mapping */
            for (i = 0 ; i < len ; ++i)
                *exp_dst++ = (wchar_t)p->asc[i];
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Special built-in mapper to ISO-8859-1.  Because of the widespread use
 *   of this character set, we make this mapping available even when no
 *   external mapping file is available.  
 */

/*
 *   create an 8859-1 mapper
 */
CCharmapToLocal8859_1::CCharmapToLocal8859_1()
{
    unsigned char *dst;
    size_t siz;
    wchar_t c;

    /* 
     *   Determine how much space we'll need in the translation array - we
     *   need one byte for each character, plus one byte for the length of
     *   each character.  We also need two bytes for the default entry.  
     */
    siz = 256 + 256 + 2;

    /* allocate the mapping */
    xlat_array_ = (unsigned char *)t3malloc(siz);

    /* start at the start of the array */
    dst = xlat_array_;

    /* 
     *   Add the zeroeth entry, which serves as the default mapping for
     *   characters that aren't otherwise mappable.  
     */
    set_mapping(0, 0);
    *dst++ = 1;
    *dst++ = '?';

    /* 
     *   Set up the mappings - this is easy because each Unicode code point
     *   from 0 to 255 maps to the same ISO 8859-1 code point.  
     */
    for (c = 0 ; c < 256 ; ++c)
    {
        /* set the mapping's offset in the translation array */
        set_mapping(c, dst - xlat_array_);

        /* store the length (always 1) and translated character */
        *dst++ = 1;
        *dst++ = (unsigned char)c;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Character mapping for Unicode to Local 
 */

/*
 *   create the translator 
 */
CCharmapToLocal::CCharmapToLocal()
{
    /* no mapping sub-tables yet */
    memset(map_, 0, sizeof(map_));
    memset(exp_map_, 0, sizeof(exp_map_));

    /* no translation or expansion arrays yet */
    xlat_array_ = 0;
    exp_array_ = 0;
}

/*
 *   delete the translator 
 */
CCharmapToLocal::~CCharmapToLocal()
{
    size_t i;

    /* delete the translation array */
    if (xlat_array_ != 0)
        t3free(xlat_array_);

    /* delete the expansion array */
    if (exp_array_ != 0)
        t3free(exp_array_);

    /* delete any mapping tables we've allocated */
    for (i = 0 ; i < sizeof(map_)/sizeof(map_[0]) ; ++i)
    {
        /* delete this mapping if allocated */
        if (map_[i] != 0)
            t3free(map_[i]);
    }

    /* delete any expansion mapping tables */
    for (i = 0 ; i < sizeof(exp_map_)/sizeof(exp_map_[0]) ; ++i)
    {
        /* delete this expansion mapping if allocated */
        if (exp_map_[i] != 0)
            t3free(exp_map_[i]);
    }
}

/*
 *   Set a mapping 
 */
void CCharmapToLocal::set_mapping(wchar_t unicode_char,
                                  unsigned int xlat_offset)
{
    int master_idx;
    
    /* get the master table index for this unicode character */
    master_idx = (int)((unicode_char >> 8) & 0xff);
    
    /* if there's no sub-table here yet, create one */
    if (map_[master_idx] == 0)
    {
        int i;
        
        /* allocate it */
        map_[master_idx] =
            (unsigned int *)t3malloc(256 * sizeof(unsigned int));
        
        /* 
         *   Set each entry to the default character, so that it will
         *   produce valid results if no mapping is ever specified for the
         *   character.  The default character is always at offset zero in
         *   the translation array.  
         */
        for (i = 0 ; i < 256 ; ++i)
            map_[master_idx][i] = 0;
    }
    
    /* set the mapping for the character's entry in the sub-table */
    map_[master_idx][unicode_char & 0xff] = xlat_offset;
}

/*
 *   Set an expansion mapping 
 */
void CCharmapToLocal::set_exp_mapping(wchar_t unicode_char,
                                      unsigned int exp_offset)
{
    int master_idx;

    /* get the master table index for this unicode character */
    master_idx = (int)((unicode_char >> 8) & 0xff);

    /* if there's no sub-table here yet, create one */
    if (exp_map_[master_idx] == 0)
    {
        int i;

        /* allocate it */
        exp_map_[master_idx] =
            (unsigned int *)t3malloc(256 * sizeof(unsigned int));

        /* 
         *   Set each entry to the default character, so that it will produce
         *   valid results if no mapping is ever specified for the character.
         *   The default character is always at offset zero in the expansion
         *   array.  
         */
        for (i = 0 ; i < 256 ; ++i)
            exp_map_[master_idx][i] = 0;
    }

    /* set the mapping for the character's entry in the sub-table */
    exp_map_[master_idx][unicode_char & 0xff] = exp_offset;
}

/*
 *   Map a UTF-8 string of known byte length to the local character set
 */
size_t CCharmapToLocal::map_utf8(char *dest, size_t dest_len,
                                 utf8_ptr src, size_t src_byte_len,
                                 size_t *src_bytes_used) const
{
    utf8_ptr src_start;
    size_t cur_total;
    char *srcend;
        
    /* remember where we started */
    src_start = src;

    /* compute where the source buffer ends */
    srcend = src.getptr() + src_byte_len;
    
    /* copy characters until we reach the end of the source string */
    for (cur_total = 0 ; src.getptr() < srcend ; src.inc())
    {
        char mapbuf[10];
        size_t maplen = sizeof(mapbuf);
        char *mapp = mapbuf;

        /* map this character */
        maplen = map(src.getch(), &mapp, &maplen);

        /* determine how to store the character */
        if (dest == 0)
        {
            /* we're just counting */
        }
        else if (dest_len >= maplen)
        {
            /* we have room for it - add it in */
            memcpy(dest, mapbuf, maplen);

            /* advance past it */
            dest += maplen;
            dest_len -= maplen;
        }
        else
        {
            /* there's no more room - stop now */
            break;
        }

        /* add this into the total */
        cur_total += maplen;
    }

    /* if the caller wants to know how much space we used, tell them */
    if (src_bytes_used != 0)
        *src_bytes_used = src.getptr() - src_start.getptr();

    /* return the total length of the result */
    return cur_total;
}

/*
 *   Map a null-terminated UTF-8 string to the local character set
 */
size_t CCharmapToLocal::map_utf8z(char *dest, size_t dest_len,
                                  utf8_ptr src) const
{
    size_t cur_total;
    
    /* copy characters until we find the terminating null */
    for (cur_total = 0 ; src.getch() != 0 ; src.inc())
    {
        /* 
         *   map this character into the output, if it will fit, but in
         *   any case count the space it needs in the output 
         */
        cur_total += map(src.getch(), &dest, &dest_len);
    }

    /* 
     *   add a null terminator if there's room, but don't count it in the
     *   result length 
     */
    map(0, &dest, &dest_len);
    
    /* return the total length of the result */
    return cur_total;
}

/*
 *   Map a string, allocating a new buffer if the caller doesn't provide one.
 */
size_t CCharmapToLocal::map_utf8_alo(
    char **buf, const char *src, size_t srclen) const
{
    /* figure out how much space we need */
    size_t buflen = map_utf8(0, 0, src, srclen, 0);

    /* allocate the buffer, adding space for null termination */
    *buf = (char *)t3malloc(buflen + 1);
    
    /* if that failed, return null */
    if (buf == 0)
        return 0;

    /* do the mapping */
    buflen = map_utf8(*buf, buflen, src, srclen, 0);

    /* fill in the null terminator */
    (*buf)[buflen] = '\0';

    /* return the mapped length */
    return buflen;
}

/*
 *   Map a null-terminated UTF-8 string to the local character set, escaping
 *   characters that aren't part of the local character set.  
 */
size_t CCharmapToLocal::map_utf8z_esc(
    char *dest, size_t dest_len, utf8_ptr src,
    size_t (*esc_fn)(wchar_t, char **, size_t *)) const
{
    size_t cur_total;

    /* copy characters until we find the terminating null */
    for (cur_total = 0 ; src.getch() != 0 ; src.inc())
    {
        wchar_t ch = src.getch();
        
        /* if this character is mappable, map it; otherwise, escape it */
        if (is_mappable(src.getch()))
        {
            /* map the character */
            cur_total += map(ch, &dest, &dest_len);
        }
        else
        {
            /* we can't map it, so let the escape callback handle it */
            cur_total += (*esc_fn)(ch, &dest, &dest_len);
        }
    }

    /* 
     *   add a null terminator if there's room, but don't count it in the
     *   result length 
     */
    map(0, &dest, &dest_len);

    /* return the total length of the result */
    return cur_total;
}

/*
 *   Escape callback for map_utf8z_esc() - prepares source-code-style
 *   'backslash' escape sequences for unmappable characters.  
 */
size_t CCharmapToLocal::source_esc_cb(wchar_t ch, char **dest, size_t *len)
{
    char buf[7];
    size_t copylen;
    
    /* prepare our own representation */
    sprintf(buf, "\\u%04x", (unsigned int)ch);

    /* copy the whole thing if possible, but limit to the available space */
    copylen = 6;
    if (copylen > *len)
        copylen = *len;

    /* copy the bytes */
    memcpy(*dest, buf, copylen);

    /* advance the buffer pointers */
    *dest += copylen;
    *len -= copylen;

    /* return the full space needed */
    return 6;
}

/*
 *   Map to UTF8 
 */
size_t CCharmapToLocal::map_utf8(char *dest, size_t dest_len,
                                 const char *src, size_t src_byte_len,
                                 size_t *src_bytes_used) const
{
    utf8_ptr src_ptr;

    /* set up the source UTF-8 pointer */
    src_ptr.set((char *)src);

    /* map it and return the result */
    return map_utf8(dest, dest_len, src_ptr, src_byte_len, src_bytes_used);
}

/*
 *   Create a mapper and load a mapping file 
 */
CCharmapToLocal *CCharmapToLocal::load(CResLoader *res_loader,
                                       const char *table_name)
{
    osfildef *fp;
    CCharmapToLocal *mapper;
    charmap_type_t map_type;

    /* if they want a trivial UTF-8 translator, return one */
    if (name_is_utf8_synonym(table_name))
        return new CCharmapToLocalUTF8();

    /* if they want a Unicode 16-bit encoding, return one */
    if (name_is_ucs2le_synonym(table_name))
        return new CCharmapToLocalUcs2Little();

    if (name_is_ucs2be_synonym(table_name))
        return new CCharmapToLocalUcs2Big();

    /* presume failure */
    mapper = 0;

    /* open and characterize the mapping file */
    fp = open_map_file_syn(res_loader, table_name, &map_type);

    /* if we didn't find a map file, check for built-in mappings */
    if (fp == 0)
    {
        /* if they want a plain ASCII translator, return a default one */
        if (name_is_ascii_synonym(table_name))
            return new CCharmapToLocalASCII();

        /* if they want a plain ISO-8859-1 translator, return a default one */
        if (name_is_8859_1_synonym(table_name))
            return new CCharmapToLocal8859_1();

        /* no map file - return failure */
        return 0;
    }

    /* create an appropriate mapper */
    switch(map_type)
    {
    case CHARMAP_TYPE_SB:
        /* create a single-byte mapper */
        mapper = new CCharmapToLocalSB();
        break;

    case CHARMAP_TYPE_DB:
        /* create a double-byte mapper */
        mapper = new CCharmapToLocalDB();
        break;

    case CHARMAP_TYPE_MB:
        /* create a mixed multi-byte mapper */
        mapper = new CCharmapToLocalMB();
        break;

    default:
        /* other mapper types are currently unknown */
        break;
    }

    /* if we successfully created a mapper, tell it to load the table */
    if (mapper != 0)
    {
        /* load the table */
        mapper->load_table(fp);
    }

    /* close the file */
    osfcls(fp);

    /* return the mapper, if any */
    return mapper;
}

/*
 *   Load the character set translation table 
 */
void CCharmapToLocal::load_table(osfildef *fp)
{
    ulong startpos;
    ulong ofs;
    uchar buf[256];
    uint cnt;
    ulong xbytes;
    ulong xchars;
    uint next_ofs;
    
    /* note the initial seek position */
    startpos = osfpos(fp);

    /* read the first entry, which gives the offset of the to-local table */
    if (osfrb(fp, buf, 4))
        return;
    ofs = t3rp4u(buf);

    /* seek to the to-local table */
    osfseek(fp, startpos + ofs, OSFSK_SET);

    /* read the number of entries and number of bytes needed */
    if (osfrb(fp, buf, 6))
        return;
    cnt = osrp2(buf);
    xbytes = t3rp4u(buf + 2);

    /* 
     *   Allocate space for the translation table.  Note that we cannot
     *   handle translation tables bigger than the maximum allowed in a
     *   single allocation unit on the operating system. 
     */
    if (xbytes > OSMALMAX)
        return;
    xlat_array_ = (unsigned char *)t3malloc(xbytes);
    if (xlat_array_ == 0)
        return;

    /*
     *   Read each mapping 
     */
    for (next_ofs = 0 ; cnt > 0 ; --cnt)
    {
        wchar_t codept;
        uint xlen;
        
        /* read the code point and translation length */
        if (osfrb(fp, buf, 3))
            return;

        /* decode the code point and translation length */
        codept = osrp2(buf);
        xlen = (unsigned int)buf[2];

        /* assign the mapping */
        set_mapping(codept, next_ofs);

        /* store the translation length */
        xlat_array_[next_ofs++] = buf[2];

        /* read the translation bytes */
        if (osfrb(fp, xlat_array_ + next_ofs, xlen))
            return;

        /* skip past the translation bytes we've read */
        next_ofs += xlen;
    }

    /*
     *   Next, read the expansions, if present.
     *   
     *   If we find the $EOF marker, it means it's an old-format file without
     *   the separate expansion definitions.  Otherwise, we'll have the
     *   expansion entry count and the aggregate number of unicode characters
     *   in all of the expansions.  
     */
    if (osfrb(fp, buf, 6) || memcmp(buf, "$EOF", 4) == 0)
        return;

    /* decode the expansion entry count and aggregate length */
    cnt = osrp2(buf);
    xchars = t3rp4u(buf + 2);

    /* 
     *   add one entry so that we can leave index zero unused, to indicate
     *   unmapped characters 
     */
    ++xchars;

    /* add one array slot per entry, for the length prefix slots */
    xchars += cnt;

    /* allocate space for the expansions */
    exp_array_ = (wchar_t *)t3malloc(xchars * sizeof(wchar_t));
    if (exp_array_ == 0)
        return;

    /* 
     *   read the mappings; start loading them at index 1, since we want to
     *   leave index 0 unused so that it can indicate unused mappings 
     */
    for (next_ofs = 1 ; cnt > 0 ; --cnt)
    {
        wchar_t codept;
        uint xlen;
        size_t i;

        /* read this entry's unicode value and expansion character length */
        if (osfrb(fp, buf, 3))
            return;

        /* decode the code point and expansion length */
        codept = osrp2(buf);
        xlen = (uint)buf[2];

        /* assign the expansion mapping */
        set_exp_mapping(codept, next_ofs);

        /* set the length prefix */
        exp_array_[next_ofs++] = (wchar_t)xlen;

        /* read and store the expansion characters */
        for (i = 0 ; i < xlen ; ++i)
        {
            /* read this translation */
            if (osfrb(fp, buf, 2))
                return;

            /* decode and store this translation */
            exp_array_[next_ofs++] = osrp2(buf);
        }
    }
}

/*
 *   Write to a file 
 */
int CCharmapToLocal::write_file(CVmDataSource *fp,
                                const char *buf, size_t bufl)
{
    utf8_ptr p;

    /* set up to read from the buffer */
    p.set((char *)buf);
    
    /* map and write one buffer-full at a time */
    while (bufl > 0)
    {
        char conv_buf[256];
        size_t conv_len;
        size_t used_src_len;

        /* map as much as we can fit into our buffer */
        conv_len = map_utf8(conv_buf, sizeof(conv_buf), p, bufl,
                            &used_src_len);

        /* write out this chunk */
        if (fp->write(conv_buf, conv_len))
            return 1;

        /* advance past this chunk in the input */
        p.set(p.getptr() + used_src_len);
        bufl -= used_src_len;
    }

    /* no errors */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Character mapper - trivial UTF8-to-UTF8 conversion
 */

/*
 *   map a character 
 */
size_t CCharmapToLocalUTF8::map(wchar_t unicode_char, char **output_ptr,
                                size_t *output_len) const
{
    /* get the character size */
    size_t map_len = utf8_ptr::s_wchar_size(unicode_char);
    
    /* if we don't have room for one more character, abort */
    if (*output_len < map_len)
    {
        *output_len = 0;
        return map_len;
    }

    /* store the mapping */
    utf8_ptr::s_putch(*output_ptr, unicode_char);

    /* increment the pointer by the number of characters we copied */
    *output_ptr += map_len;

    /* adjust the remaining output length */
    *output_len -= map_len;

    /* return the size of the result */
    return map_len;
}

/*
 *   Map a UTF-8 string of known byte length
 */
size_t CCharmapToLocalUTF8::map_utf8(char *dest, size_t dest_len,
                                     utf8_ptr src, size_t src_byte_len,
                                     size_t *src_bytes_used) const
{
    /* 
     *   if they didn't give us a destination buffer, tell them how much
     *   space is needed for the copy - this is identical to the length of
     *   the source string since we make no changes to it 
     */
    if (dest == 0)
    {
        if (src_bytes_used != 0)
            *src_bytes_used = 0;
        return src_byte_len;
    }

    /* assume we'll be able to copy the whole string */
    size_t copy_len = src_byte_len;

    /* if there's not enough space for the whole string, limit it */
    if (copy_len > dest_len)
    {
        /* copy no more than the output buffer length */
        copy_len = dest_len;

        /* 
         *   If this splits a character into two pieces, back up until we're
         *   at a character boundary - that is, the first byte of the next
         *   chunk is a non-continuation character.
         */
        while (copy_len > 0
               && utf8_ptr::s_is_continuation(src.getptr() + copy_len))
            --copy_len;

        /* 
         *   If that left us with zero bytes, just do the partial copy to
         *   ensure we don't get stuck in a zero-at-a-time infinite loop.
         */
        if (copy_len == 0)
            copy_len = dest_len;
    }

    /* if we have an output buffer, copy the data */
    if (dest != 0)
        memcpy(dest, src.getptr(), copy_len);

    /* set the amount we copied, if the caller is interested */
    if (src_bytes_used != 0)
        *src_bytes_used = copy_len;

    /* return the number of bytes we put in the destination buffer */
    return copy_len;
}

/*
 *   Map a null-terminated UTF-8 string
 */
size_t CCharmapToLocalUTF8::map_utf8z(char *dest, size_t dest_len,
                                      utf8_ptr src) const
{
    /* get the source length */
    size_t src_len = strlen(src.getptr());

    /* copy the bytes */
    map_utf8(dest, dest_len, src, src_len, 0);

    /* 
     *   if there's room for the null terminator (which takes up just one
     *   byte in UTF-8), add it 
     */
    if (dest_len > src_len)
        *(dest + src_len) = '\0';

    /* 
     *   return the amount of space needed to copy the whole string --
     *   this is identical to the source length, since we don't make any
     *   changes to it 
     */
    return src_len;
}


/* ------------------------------------------------------------------------ */
/*
 *   Character mapper - Unicode to Single-byte 
 */

/*
 *   map a character 
 */
size_t CCharmapToLocalSB::map(wchar_t unicode_char, char **output_ptr,
                              size_t *output_len) const
{
    /* get the mapping */
    size_t map_len;
    const unsigned char *mapping = get_xlation(unicode_char, &map_len);
    
    /* if we don't have room for one more character, abort */
    if (*output_len < map_len)
    {
        *output_len = 0;
        return map_len;
    }
    
    /* copy the mapping */
    memcpy(*output_ptr, mapping, map_len);
    
    /* increment the pointer by the number of characters we copied */
    *output_ptr += map_len;

    /* adjust the remaining output length */
    *output_len -= map_len;
    
    /* return the size of the result */
    return map_len;
}

/*
 *   Map a UTF-8 string of known byte length to the local character set 
 */
size_t CCharmapToLocalSB::map_utf8(char *dest, size_t dest_len,
                                   utf8_ptr src, size_t src_byte_len,
                                   size_t *src_bytes_used) const
{
    /* remember where we started */
    utf8_ptr src_start = src;

    /* compute where the source buffer ends */
    char *srcend = src.getptr() + src_byte_len;

    /* copy characters until we reach the end of the source string */
    size_t cur_total;
    for (cur_total = 0 ; src.getptr() < srcend ; src.inc())
    {
        const unsigned char *mapping;
        size_t map_len;
        
        /* get the mapping for this character */
        mapping = get_xlation(src.getch(), &map_len);

        /* 
         *   if we have room, add it; otherwise, zero the output length
         *   remaining so we don't try to add anything more 
         */
        if (dest == 0)
        {
            /* we're just counting */
        }
        else if (map_len <= dest_len)
        {
            /* add the sequence */
            memcpy(dest, mapping, map_len);

            /* adjust the output pointer and length remaining */
            dest += map_len;
            dest_len -= map_len;
        }
        else
        {
            /* it doesn't fit - stop now */
            break;
        }

        /* count the length in the total */
        cur_total += map_len;
    }

    /* if the caller wants to know how much space we used, tell them */
    if (src_bytes_used != 0)
        *src_bytes_used = src.getptr() - src_start.getptr();

    /* return the total length of the result */
    return cur_total;
}

/*
 *   Map a null-terminated UTF-8 string to the local character set 
 */
size_t CCharmapToLocalSB::map_utf8z(char *dest, size_t dest_len,
                                    utf8_ptr src) const
{
    size_t cur_total;

    /* copy characters until we find the terminating null */
    for (cur_total = 0 ; src.getch() != 0 ; src.inc())
    {
        const unsigned char *mapping;
        size_t map_len;

        /* get the mapping for this character */
        mapping = get_xlation(src.getch(), &map_len);

        /* 
         *   if we have room, add it; otherwise, zero the output length
         *   remaining so we don't try to add anything more 
         */
        if (map_len <= dest_len)
        {
            /* add the sequence */
            memcpy(dest, mapping, map_len);

            /* adjust the output pointer and length remaining */
            dest += map_len;
            dest_len -= map_len;
        }
        else
        {
            /* it doesn't fit - zero the output length remaining */
            dest_len = 0;
        }

        /* count the length in the total */
        cur_total += map_len;
    }

    /* 
     *   add a null terminator, if there's room, but don't count it in the
     *   output length 
     */
    if (dest_len > 0)
        *dest = '\0';
    
    /* return the total length of the result */
    return cur_total;
}


/* ------------------------------------------------------------------------ */
/*
 *   Character mapper - Unicode to 16-bit Wide Unicode local character set 
 */

/*
 *   map a character 
 */
size_t CCharmapToLocalWideUnicode::map(wchar_t unicode_char,
                                       char **output_ptr,
                                       size_t *output_len) const
{
    /* if we don't have room for another wchar_t, abort */
    if (*output_len < sizeof(wchar_t))
    {
        *output_len = 0;
        return sizeof(wchar_t);
    }
    
    /* 
     *   Set the wide character to the unicode value, with no translation
     *   - unicode is the same everywhere.
     *   
     *   Note that the need to perform this trivial translation for this
     *   character set is a secondary reason that this routine is virtual
     *   (the primary reason is to handle the default ASCII translation).  
     */
    **(wchar_t **)output_ptr = unicode_char;
    
    /* increment the pointer by the size of a wide character */
    ++(*(wchar_t **)output_ptr);
    
    /* return the size of the result */
    return sizeof(wchar_t);
}

/*
 *   Map a UTF-8 string of known byte length to the local character set 
 */
size_t CCharmapToLocalWideUnicode::
   map_utf8(char *dest, size_t dest_len,
            utf8_ptr src, size_t src_byte_len,
            size_t *src_bytes_used) const
{
    utf8_ptr src_start;
    size_t cur_total;
    char *srcend;
    wchar_t *destw;

    /* remember where we started */
    src_start = src;

    /* compute where the source buffer ends */
    srcend = src.getptr() + src_byte_len;

    /* set up a wchar_t output pointer for convenience */
    destw = (wchar_t *)dest;

    /* copy characters until we reach the end of the source string */
    for (cur_total = 0 ; src.getptr() < srcend ; src.inc())
    {
        /* 
         *   if we have room, add it; otherwise, zero the output length
         *   remaining so we don't try to add anything more 
         */
        if (dest == 0)
        {
            /* we're just counting - don't store anything */
        }
        else if (dest_len >= sizeof(wchar_t))
        {
            /* add the sequence */
            *destw++ = src.getch();

            /* adjust the length remaining */
            dest_len -= sizeof(wchar_t);
        }
        else
        {
            /* it doesn't fit - stop now */
            break;
        }

        /* count the length in the total */
        cur_total += sizeof(wchar_t);
    }

    /* if the caller wants to know how much space we used, tell them */
    if (src_bytes_used != 0)
        *src_bytes_used = src.getptr() - src_start.getptr();

    /* return the total length of the result */
    return cur_total;
}

/*
 *   Map a null-terminated UTF-8 string to the local character set 
 */
size_t CCharmapToLocalWideUnicode::
   map_utf8z(char *dest, size_t dest_len, utf8_ptr src) const
{
    size_t cur_total;
    wchar_t *destw;

    /* set up a wchar_t output pointer for convenience */
    destw = (wchar_t *)dest;

    /* copy characters until we find the terminating null */
    for (cur_total = 0 ; src.getch() != 0 ; src.inc())
    {
        /* 
         *   if we have room, add it; otherwise, zero the output length
         *   remaining so we don't try to add anything more 
         */
        if (dest_len >= sizeof(wchar_t))
        {
            /* add the sequence */
            *destw++ = src.getch();

            /* adjust the length remaining */
            dest_len -= sizeof(wchar_t);
        }
        else
        {
            /* it doesn't fit - zero the output length remaining */
            dest_len = 0;
        }

        /* count the length in the total */
        cur_total += sizeof(wchar_t);
    }

    /* 
     *   if there's room for a null terminator character (not byte - we need
     *   to add an entire wide character), add it, but don't count it in the
     *   return length 
     */
    if (dest_len >= sizeof(wchar_t))
        *destw = '\0';
    
    /* return the total length of the result */
    return cur_total;
}


/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for 16-bit Wide Unicode, big-endian.  Stores the
 *   characters in big-endian UCS-2 representation.  
 */
size_t CCharmapToLocalUcs2Big::map(wchar_t unicode_char, char **output_ptr,
                                   size_t *output_len) const
{
    /* 
     *   If we don't have room for another byte pair, abort.  Note that we
     *   really do want to store exactly two bytes, not sizeof(anything),
     *   since we're storing to the UCS-2 file format, which encodes each
     *   character in two bytes.  
     */
    if (*output_len < 2)
    {
        *output_len = 0;
        return 2;
    }

    /*
     *   Store the big-endian 16-bit value with no translation - unicode
     *   is the same everywhere.
     *   
     *   Note that the need to perform this trivial translation for this
     *   character set is a secondary reason that this routine is virtual
     *   (the primary reason is to handle the default ASCII translation).
     *   
     *   Store the high-order 8 bits in the first byte, and the low-order
     *   8 bits in the second byte.  
     */
    **output_ptr = ((unicode_char >> 8) & 0xff);
    *(*output_ptr + 1) = (unicode_char & 0xff);

    /* skip two bytes in the output */
    *output_ptr += 2;
    *output_len -= 2;

    /* return the size of the result */
    return 2;
}

/*
 *   Map a UTF-8 string of known byte length to the local character set 
 */
size_t CCharmapToLocalUcs2Big::
   map_utf8(char *dest, size_t dest_len,
            utf8_ptr src, size_t src_byte_len,
            size_t *src_bytes_used) const
{
    utf8_ptr src_start;
    size_t cur_total;
    char *srcend;

    /* remember where we started */
    src_start = src;

    /* compute where the source buffer ends */
    srcend = src.getptr() + src_byte_len;

    /* copy characters until we reach the end of the source string */
    for (cur_total = 0 ; src.getptr() < srcend ; src.inc())
    {
        /* 
         *   if we have room, add it; otherwise, zero the output length
         *   remaining so we don't try to add anything more 
         */
        if (dest == 0)
        {
            /* we're not storing anything */
        }
        else if (dest_len >= 2)
        {
            wchar_t unicode_char;

            /* get the current character */
            unicode_char = src.getch();

            /* add the sequence */
            *dest++ = ((unicode_char >> 8) & 0xff);
            *dest++ = (unicode_char & 0xff);

            /* adjust the length remaining */
            dest_len -= 2;
        }
        else
        {
            /* it doesn't fit - stop now */
            break;
        }

        /* count the length in the total */
        cur_total += 2;
    }

    /* if the caller wants to know how much space we used, tell them */
    if (src_bytes_used != 0)
        *src_bytes_used = src.getptr() - src_start.getptr();

    /* return the total length of the result */
    return cur_total;
}

/*
 *   Map a null-terminated UTF-8 string to the local character set 
 */
size_t CCharmapToLocalUcs2Big::
   map_utf8z(char *dest, size_t dest_len, utf8_ptr src) const
{
    size_t cur_total;

    /* copy characters until we find the terminating null */
    for (cur_total = 0 ; src.getch() != 0 ; src.inc())
    {
        /* 
         *   if we have room, add it; otherwise, zero the output length
         *   remaining so we don't try to add anything more 
         */
        if (dest_len >= 2)
        {
            wchar_t unicode_char;

            /* get the current character */
            unicode_char = src.getch();

            /* add the sequence */
            *dest++ = ((unicode_char >> 8) & 0xff);
            *dest++ = (unicode_char & 0xff);

            /* adjust the length remaining */
            dest_len -= 2;
        }
        else
        {
            /* it doesn't fit - zero the output length remaining */
            dest_len = 0;
        }

        /* count the length in the total */
        cur_total += 2;
    }
    
    /* return the total length of the result */
    return cur_total;
}


/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for 16-bit Wide Unicode, little-endian.  Stores the
 *   characters in little-endian UCS-2 representation.  
 */
size_t CCharmapToLocalUcs2Little::map(wchar_t unicode_char,
                                      char **output_ptr,
                                      size_t *output_len) const
{
    /* 
     *   If we don't have room for another byte pair, abort.  Note that we
     *   really do want to store exactly two bytes, not sizeof(anything),
     *   since we're storing to the UCS-2 file format, which encodes each
     *   character in two bytes.  
     */
    if (*output_len < 2)
    {
        *output_len = 0;
        return 2;
    }

    /*
     *   Store the little-endian 16-bit value with no translation -
     *   unicode is the same everywhere.
     *   
     *   Note that the need to perform this trivial translation for this
     *   character set is a secondary reason that this routine is virtual
     *   (the primary reason is to handle the default ASCII translation).
     *   
     *   Store the low-order 8 bits in the first byte, and the high-order
     *   8 bits in the second byte.  
     */
    **output_ptr = (unicode_char & 0xff);
    *(*output_ptr + 1) = ((unicode_char >> 8) & 0xff);

    /* skip two bytes in the output */
    *output_ptr += 2;
    *output_len -= 2;

    /* return the size of the result */
    return 2;
}

/*
 *   Map a UTF-8 string of known byte length to the local character set 
 */
size_t CCharmapToLocalUcs2Little::
   map_utf8(char *dest, size_t dest_len,
            utf8_ptr src, size_t src_byte_len,
            size_t *src_bytes_used) const
{
    utf8_ptr src_start;
    size_t cur_total;
    char *srcend;

    /* remember where we started */
    src_start = src;

    /* compute where the source buffer ends */
    srcend = src.getptr() + src_byte_len;

    /* copy characters until we reach the end of the source string */
    for (cur_total = 0 ; src.getptr() < srcend ; src.inc())
    {
        /* 
         *   if we have room, add it; otherwise, zero the output length
         *   remaining so we don't try to add anything more 
         */
        if (dest == 0)
        {
            /* we're just counting - don't store anything */
        }
        else if (dest_len >= 2)
        {
            wchar_t unicode_char;

            /* get the current character */
            unicode_char = src.getch();

            /* add the sequence */
            *dest++ = (unicode_char & 0xff);
            *dest++ = ((unicode_char >> 8) & 0xff);

            /* adjust the length remaining */
            dest_len -= 2;
        }
        else
        {
            /* it doesn't fit - stop now */
            break;
        }

        /* count the length in the total */
        cur_total += 2;
    }

    /* if the caller wants to know how much space we used, tell them */
    if (src_bytes_used != 0)
        *src_bytes_used = src.getptr() - src_start.getptr();

    /* return the total length of the result */
    return cur_total;
}

/*
 *   Map a null-terminated UTF-8 string to the local character set 
 */
size_t CCharmapToLocalUcs2Little::
   map_utf8z(char *dest, size_t dest_len, utf8_ptr src) const
{
    size_t cur_total;

    /* copy characters until we find the terminating null */
    for (cur_total = 0 ; src.getch() != 0 ; src.inc())
    {
        /* 
         *   if we have room, add it; otherwise, zero the output length
         *   remaining so we don't try to add anything more 
         */
        if (dest_len >= 2)
        {
            wchar_t unicode_char;

            /* get the current character */
            unicode_char = src.getch();

            /* add the sequence */
            *dest++ = (unicode_char & 0xff);
            *dest++ = ((unicode_char >> 8) & 0xff);

            /* adjust the length remaining */
            dest_len -= 2;
        }
        else
        {
            /* it doesn't fit - zero the output length remaining */
            dest_len = 0;
        }

        /* count the length in the total */
        cur_total += 2;
    }
    
    /* 
     *   if there's room for a null terminator character (which takes two
     *   bytes in UCS-2), add it, but don't count it in the return length 
     */
    if (dest_len >= 2)
    {
        *dest++ = '\0';
        *dest++ = '\0';
    }
    
    /* return the total length of the result */
    return cur_total;
}


/* ------------------------------------------------------------------------ */
/*
 *   Character mapper - local to UTF-8 
 */

/*
 *   create an appropriate mapping object for the given mapping file 
 */
CCharmapToUni *CCharmapToUni::load(class CResLoader *res_loader,
                                   const char *table_name)
{
    osfildef *fp;
    CCharmapToUni *mapper;
    charmap_type_t map_type;

    /* if they want a trivial UTF-8 translator, return one */
    if (name_is_utf8_synonym(table_name))
        return new CCharmapToUniUTF8();

    /* if they want a 16-bit Unicode mapping, return one */
    if (name_is_ucs2le_synonym(table_name))
        return new CCharmapToUniUcs2Little();
    if (name_is_ucs2be_synonym(table_name))
        return new CCharmapToUniUcs2Big();

    /* presume failure */
    mapper = 0;

    /* open and characterize the mapping file */
    fp = open_map_file_syn(res_loader, table_name, &map_type);

    /* check to make sure we opened a file */
    if (fp == 0)
    {
        /* 
         *   if there was no file, and they want a plain ASCII translator,
         *   return a default ASCII translator 
         */
        if (name_is_ascii_synonym(table_name))
            return new CCharmapToUniASCII();

        /* if they want an ISO-8859-1 translator, return a default one */
        if (name_is_8859_1_synonym(table_name))
            return new CCharmapToUni8859_1();

        /* return failure */
        return 0;
    }

    /* create an appropriate mapper */
    switch(map_type)
    {
    case CHARMAP_TYPE_SB:
        /* create a single-byte mapper */
        mapper = new CCharmapToUniSB();
        break;

    case CHARMAP_TYPE_DB:
        /* create a double-byte mapper */
        mapper = new CCharmapToUniDB();
        break;

    case CHARMAP_TYPE_MB:
        /* create a mixed multi-byte mapper */
        mapper = new CCharmapToUniMB();
        break;

    default:
        /* other mapper types are currently unknown */
        break;
    }

    /* if we successfully created a mapper, tell it to load the table */
    if (mapper != 0)
    {
        /* load the table */
        mapper->load_table(fp);
    }

    /* close the file */
    osfcls(fp);

    /* return the mapper, if any */
    return mapper;
}


/*
 *   load a mapping table 
 */
void CCharmapToUni::load_table(osfildef *fp)
{
    uchar buf[256];
    uint entry_cnt;

    /* read the header and the local table header */
    if (osfrb(fp, buf, 6))
        return;

    /* get the local table size from the local table header */
    entry_cnt = osrp2(buf + 4);

    /* read the mappings */
    while (entry_cnt > 0)
    {
        size_t cur;
        const uchar *p;

        /* figure out how many entries we can read this time */
        cur = sizeof(buf)/4;
        if (cur > entry_cnt)
            cur = entry_cnt;

        /* read the entries */
        if (osfrb(fp, buf, cur*4))
            return;

        /* deduct this number from the remaining count */
        entry_cnt -= cur;

        /* scan the entries */
        for (p = buf ; cur > 0 ; p += 4, --cur)
        {
            /* map this entry */
            set_mapping(osrp2(p), osrp2(p+2));
        }
    }
}

/*
 *   Map a null-terminated string into a buffer 
 */
size_t CCharmapToUni::map_str(char *outbuf, size_t outbuflen,
                              const char *input_str)
{
    /* map the string to the output buffer */
    size_t output_len = map(&outbuf, &outbuflen, input_str, strlen(input_str));

    /* if there's space remaining in the output buffer, add a null byte */
    if (outbuflen != 0)
        *outbuf = '\0';

    /* return the number of bytes needed for the conversion */
    return output_len;
}

/*
 *   Map a counted-length string into a buffer 
 */
size_t CCharmapToUni::map_str(char *outbuf, size_t outbuflen,
                              const char *input_str, size_t input_len)
{
    /* map the string to the output buffer and return the result */
    return map(&outbuf, &outbuflen, input_str, input_len);
}

/*
 *   Map a null-terminated string, allocating space for the result 
 */
size_t CCharmapToUni::map_str_alo(char **outbuf, const char *input_str)
{
    /* figure the space needed */
    size_t outlen = map_str(0, 0, input_str);

    /* allocate the buffer */
    *outbuf = (char *)t3malloc(outlen + 1);

    /* map the sring */
    return map_str(*outbuf, outlen, input_str);
}

/*
 *   Validate a buffer of utf-8 characters 
 */
void CCharmapToUni::validate(char *buf, size_t len)
{
    for ( ; len != 0 ; ++buf, --len)
    {
        /* check the type of the character */
        char c = *buf;
        if ((c & 0x80) == 0)
        {
            /* 0..127 are one-byte characters, so this is valid */
        }
        else if ((c & 0xC0) == 0x80)
        {
            /* 
             *   This byte has the pattern 10xxxxxx, which makes it a
             *   continuation byte.  Since we didn't just come from a
             *   multi-byte intro byte, this is invalid.  Change this byte to
             *   '?'.  
             */
            *buf = '?';
        }
        else if ((c & 0xE0) == 0xC0)
        {
            /* 
             *   This byte has the pattern 110xxxxx, which makes it the first
             *   byte of a two-byte character sequence.  The next byte must
             *   have the pattern 10xxxxxx - if not, mark the current
             *   character as invalid, since it's not part of a valid
             *   sequence, and deal with the next byte separately.  
             */
            if (len > 1 && (*(buf+1) & 0xC0) == 0x80)
            {
                /* we have a valid two-byte sequence - skip it */
                ++buf;
                --len;
            }
            else
            {
                /* 
                 *   the next byte isn't a continuation, so the current byte
                 *   is invalid 
                 */
                *buf = '?';
            }
        }
        else
        {
            /* 
             *   This byte has the pattern 111xxxxx, which makes it the first
             *   byte of a three-byte sequence.  The next two bytes must be
             *   marked as continuation bytes.  
             */
            if (len > 2
                && (*(buf+1) & 0xC0) == 0x80
                && (*(buf+2) & 0xC0) == 0x80)
            {
                /* we have a valid three-byte sequence - skip it */
                buf += 2;
                len -= 2;
            }
            else
            {
                /* this is not a valid three-byte sequence */
                *buf = '?';
            }
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Basic single-byte character set to UTF-8 mapper 
 */

/*
 *   read from a single-byte file and translate to UTF-8
 */
size_t CCharmapToUniSB_basic::read_file(CVmDataSource *fp,
                                        char *buf, size_t bufl)
{
    size_t inlen;

    /* 
     *   Compute how much to read from the file.  The input file is
     *   composed of single-byte characters, so only read up to one third
     *   of the buffer length; this will ensure that we can always fit
     *   what we read into the caller's buffer.  
     */
    inlen = bufl / 3;

    /* in any case, we can't read more than our own buffer size */
    if (inlen > sizeof(inbuf_))
        inlen = sizeof(inbuf_);

    /* read from the file */
    inlen = fp->readc(inbuf_, inlen);

    /* 
     *   Map data to the caller's buffer, and return the result.  We're
     *   certain that the data will fit in the caller's buffer: we're
     *   mapping only a third as many characters as we have bytes
     *   available, and each character can take up at most three bytes,
     *   hence the worst case is that we fill the buffer completely.
     *   
     *   On the other hand, we may only fill the buffer to a third of its
     *   capacity, but this is okay too, since we're not required to give
     *   the caller everything they asked for.  
     */
    return map(&buf, &bufl, inbuf_, inlen);
}


/* ------------------------------------------------------------------------ */
/*
 *   Plain ASCII local to UTF-8 mapper 
 */

/*
 *   map a string from the single-byte local character set to UTF-8 
 */
size_t CCharmapToUniASCII::map(char **outp, size_t *outlen,
                               const char *inp, size_t inlen) const
{
    size_t tot_outlen;

    /* we haven't written any characters to the output buffer yet */
    tot_outlen = 0;

    /* scan each character (character == byte) in the input string */
    for ( ; inlen > 0 ; --inlen, ++inp)
    {
        wchar_t uni;
        size_t csiz;

        /* 
         *   map any character outside of the 7-bit range to U+FFFD, the
         *   Unicode REPLACEMENT CHARACTER, which is the standard way to
         *   represent characters that can't be mapped from an incoming
         *   character set 
         */
        if (((unsigned char)*inp) > 127)
            uni = 0xfffd;
        else
            uni = ((wchar_t)(unsigned char)*inp);

        /* get the size of this character */
        csiz = utf8_ptr::s_wchar_size(uni);

        /* add it to the total output length */
        tot_outlen += csiz;

        /* if there's room, add it to our output buffer */
        if (*outlen >= csiz)
        {
            /* write it out */
            *outp += utf8_ptr::s_putch(*outp, uni);

            /* deduct it from the remaining output length */
            *outlen -= csiz;
        }
        else
        {
            /* there's no room - set the remaining output length to zero */
            *outlen = 0;
        }
    }

    /* return the total output length */
    return tot_outlen;
}


/* ------------------------------------------------------------------------ */
/*
 *   Single-byte mapped local to UTF-8 mapper 
 */

/*
 *   map a string from the single-byte local character set to UTF-8 
 */
size_t CCharmapToUniSB::map(char **outp, size_t *outlen,
                            const char *inp, size_t inlen) const
{
    size_t tot_outlen;

    /* we haven't written any characters to the output buffer yet */
    tot_outlen = 0;

    /* scan each character (character == byte) in the input string */
    for ( ; inlen > 0 ; --inlen, ++inp)
    {
        wchar_t uni;
        size_t csiz;
        
        /* get the unicode mapping for this character */
        uni = map_[(unsigned char)*inp];

        /* get the size of this character */
        csiz = utf8_ptr::s_wchar_size(uni);

        /* add it to the total output lenght */
        tot_outlen += csiz;

        /* if there's room, add it to our output buffer */
        if (*outlen >= csiz)
        {
            /* write it out */
            *outp += utf8_ptr::s_putch(*outp, uni);
            
            /* deduct it from the remaining output length */
            *outlen -= csiz;
        }
        else
        {
            /* there's no room - set the remaining output length to zero */
            *outlen = 0;
        }
    }

    /* return the total output length */
    return tot_outlen;
}

/* ------------------------------------------------------------------------ */
/*
 *   Trivial UTF8-to-UTF8 input mapper 
 */

/*
 *   map a string 
 */
size_t CCharmapToUniUTF8::map2(char **outp, size_t *outlen,
                               const char *inp, size_t inlen,
                               size_t *partial_len) const
{
    size_t copy_len;

    /* 
     *   Make sure we copy only whole characters, by truncating the string
     *   to a length that includes only whole characters.  
     */
    copy_len = utf8_ptr::s_trunc(inp, inlen);

    /* 
     *   note the length of any partial characters at the end of the buffer
     *   for the caller - this is simply the difference between the original
     *   length and the truncated copy length, since the truncation length
     *   is simply the length excluding the partial last character bytes 
     */
    *partial_len = inlen - copy_len;

    /* limit the copying to what will fit in the output buffer */
    if (copy_len > *outlen)
    {
        /* don't copy more than will fit, and don't copy partial characters */
        copy_len = utf8_ptr::s_trunc(inp, *outlen);

        /* we don't have enough room, so set the output size to zero */
        *outlen = 0;
    }
    else
    {
        /* we have room, so decrement the output size by the copy size */
        *outlen -= copy_len;
    }

    /* copy the data, if we have an output buffer */
    if (outp != 0)
    {
        /* copy the bytes */
        memcpy(*outp, inp, copy_len);

        /* validate that the bytes we copied are well-formed UTF-8 */
        validate(*outp, copy_len);
        
        /* advance the output pointer past the copied data */
        *outp += copy_len;
    }

    /* 
     *   return the total input length -- the total output length is
     *   always identical to the input length, because we don't change
     *   anything 
     */
    return inlen;
}

/*
 *   read a file 
 */
size_t CCharmapToUniUTF8::read_file(CVmDataSource *fp,
                                    char *buf, size_t bufl)
{
    size_t read_len;
    char *last_start;
    size_t last_got_len;
    size_t last_need_len;
    
    /* 
     *   Read directly from the file, up the buffer size minus two bytes.
     *   We want to leave two extra bytes so that we can read any extra
     *   continuation bytes for the last character, in order to ensure
     *   that we always read whole characters; in the worst case, the last
     *   character could be three bytes long, in which case we'd need to
     *   read two extra bytes.
     *   
     *   If the available buffer size is less than three bytes, just read
     *   the number of bytes they asked for and don't bother trying to
     *   keep continuation sequences intact.  
     */
    if (bufl < 3)
    {
        read_len = fp->readc(buf, bufl);
        validate(buf, read_len);
        return read_len;
    }

    /* 
     *   read up to the buffer size, less two bytes for possible
     *   continuation bytes 
     */
    read_len = fp->readc(buf, bufl - 2);

    /* 
     *   if we didn't satisfy the entire request, we're at the end of the
     *   file, so there's no point in trying to finish off any
     *   continuation sequences - in this case, just return what we have 
     */
    if (read_len < bufl - 2)
    {
        validate(buf, read_len);
        return read_len;
    }

    /* 
     *   Check the last byte we read to see if there's another byte or two
     *   following. 
     *   
     *   If the last byte is a continuation byte, this is a bit trickier.
     *   We must back up to the preceding lead byte to figure out what we
     *   have in this case.  
     */
    last_start = &buf[read_len - 1];
    last_got_len = 1;
    if (utf8_ptr::s_is_continuation(last_start))
    {
        /* 
         *   if we only read one byte, simply return the one byte - we
         *   started in the middle of a sequence, so there's no way we can
         *   read a complete sequence 
         */
        if (read_len == 1)
        {
            validate(buf, read_len);
            return read_len;
        }

        /* back up to the byte we're continuing from */
        --last_start;
        ++last_got_len;

        /* 
         *   if this is another continuation byte, we've reached the maximum
         *   byte length of three for a single character, so there's no way
         *   we could need to read anything more 
         */
        if (utf8_ptr::s_is_continuation(last_start))
        {
            validate(buf, read_len);
            return read_len;
        }
    }

    /* 
     *   Okay: we have last_start pointing to the start of the last
     *   character, and last_got_len the number of bytes we actually have for
     *   that last character.  If the needed length differs from the length
     *   we actually have, we need to read more.  
     */
    last_need_len = utf8_ptr::s_charsize(*last_start);
    if (last_need_len > last_got_len)
    {
        /* 
         *   we need more than we actually read, so read the remaining
         *   characters 
         */
        read_len += fp->readc(buf + read_len, last_need_len - last_got_len);
    }

    /* validate the buffer - ensure that it's well-formed UTF-8 */
    validate(buf, read_len);

    /* return the length we read */
    return read_len;
}

/* ------------------------------------------------------------------------ */
/*
 *   Basic UCS-2 to UTF-8 mapper 
 */

/*
 *   Read from a file, translating to UTF-8 encoding 
 */
size_t CCharmapToUniUcs2::read_file(CVmDataSource *fp,
                                    char *buf, size_t bufl)
{
    size_t inlen;

    /* 
     *   Compute how much to read from the file.  The input file is composed
     *   of two-byte characters, so only read up to two thirds of the buffer
     *   length; this will ensure that we can always fit what we read into
     *   the caller's buffer.
     *   
     *   Note that we divide by three first, then double the result, to
     *   ensure that we read an even number of bytes.  Each UCS-2 character
     *   is represented in exactly two bytes, so we must always read pairs of
     *   bytes to be sure we're reading whole characters.  
     */
    inlen = bufl / 3;
    inlen *= 2;

    /* in any case, we can't read more than our own buffer size */
    if (inlen > sizeof(inbuf_))
        inlen = sizeof(inbuf_);

    /* read from the file */
    inlen = fp->readc(inbuf_, inlen);

    /* 
     *   Map data to the caller's buffer, and return the result.  We're
     *   certain that the data will fit in the caller's buffer: we're
     *   mapping only a third as many characters as we have bytes
     *   available, and each character can take up at most three bytes,
     *   hence the worst case is that we fill the buffer completely.
     *   
     *   On the other hand, we may only fill the buffer to a third of its
     *   capacity, but this is okay too, since we're not required to give
     *   the caller everything they asked for.  
     */
    return map(&buf, &bufl, inbuf_, inlen);
}

/* ------------------------------------------------------------------------ */
/*
 *   UCS-2 little-endian to UTF-8 mapper 
 */

/*
 *   map a string 
 */
size_t CCharmapToUniUcs2Little::map(char **outp, size_t *outlen,
                                    const char *inp, size_t inlen) const
{
    size_t tot_outlen;

    /* we haven't written any characters to the output buffer yet */
    tot_outlen = 0;

    /* scan each character (character == byte pair) in the input string */
    for ( ; inlen > 1 ; inlen -= 2, inp += 2)
    {
        wchar_t uni;
        size_t csiz;

        /* 
         *   read the little-endian two-byte value - no mapping is
         *   required, since UCS-2 uses the same code point assignments as
         *   UTF-8 
         */
        uni = ((wchar_t)(unsigned char)*inp)
              + (((wchar_t)(unsigned char)*(inp + 1)) << 8);

        /* get the size of this character */
        csiz = utf8_ptr::s_wchar_size(uni);

        /* add it to the total output lenght */
        tot_outlen += csiz;

        /* if there's room, add it to our output buffer */
        if (*outlen >= csiz)
        {
            /* write it out */
            *outp += utf8_ptr::s_putch(*outp, uni);

            /* deduct it from the remaining output length */
            *outlen -= csiz;
        }
        else
        {
            /* there's no room - set the remaining output length to zero */
            *outlen = 0;
        }
    }

    /* return the total output length */
    return tot_outlen;
}

/* ------------------------------------------------------------------------ */
/*
 *   UCS-2 big-endian to UTF-8 mapper 
 */

/*
 *   map a string 
 */
size_t CCharmapToUniUcs2Big::map(char **outp, size_t *outlen,
                                 const char *inp, size_t inlen) const
{
    size_t tot_outlen;

    /* we haven't written any characters to the output buffer yet */
    tot_outlen = 0;

    /* scan each character (character == byte pair) in the input string */
    for ( ; inlen > 1 ; inlen -= 2, inp += 2)
    {
        wchar_t uni;
        size_t csiz;

        /* 
         *   read the big-endian two-byte value - no mapping is required,
         *   since UCS-2 uses the same code point assignments as UTF-8 
         */
        uni = (((wchar_t)(unsigned char)*inp) << 8)
              + ((wchar_t)(unsigned char)*(inp + 1));

        /* get the size of this character */
        csiz = utf8_ptr::s_wchar_size(uni);

        /* add it to the total output lenght */
        tot_outlen += csiz;

        /* if there's room, add it to our output buffer */
        if (*outlen >= csiz)
        {
            /* write it out */
            *outp += utf8_ptr::s_putch(*outp, uni);

            /* deduct it from the remaining output length */
            *outlen -= csiz;
        }
        else
        {
            /* there's no room - set the remaining output length to zero */
            *outlen = 0;
        }
    }

    /* return the total output length */
    return tot_outlen;
}

/* ------------------------------------------------------------------------ */
/*
 *   Multi-byte character set translation to Unicode 
 */

/*
 *   construct the mapper 
 */
CCharmapToUniMB::CCharmapToUniMB()
{
    int i;
    cmap_mb_entry *p;

    /* clear out the mapping table */
    for (i = 0, p = map_ ; i < 256 ; ++i, ++p)
    {
        /* assume this lead byte won't have a sub-table */
        p->sub = 0;

        /* 
         *   we don't have a mapping for this lead byte yet, so use U+FFFD
         *   (the Unicode REPLACEMENT CHARACTER) as the default mapping in
         *   case we never assign it any other mapping 
         */
        p->ch = 0xFFFD;
    }
}

/* 
 *   delete the mapper 
 */
CCharmapToUniMB::~CCharmapToUniMB()
{
    int i;
    cmap_mb_entry *p;

    /* delete all of our sub-tables */
    for (i = 0, p = map_ ; i < 256 ; ++i, ++p)
    {
        /* if this sub-table was allocated, delete it */
        if (p->sub != 0)
            t3free(p->sub);
    }
}

/*
 *   Set a mapping 
 */
void CCharmapToUniMB::set_mapping(wchar_t uni_code_pt, wchar_t local_code_pt)
{
    /* 
     *   Check to see if it's a one-byte or two-byte mapping.  If the local
     *   code point is in the range 0-255, it's a one-byte character;
     *   otherwise, it's a two-byte character.  
     */
    if (local_code_pt <= 255)
    {
        /* it's a single-byte character, so simply set the mapping */
        map_[(unsigned char)local_code_pt].ch = uni_code_pt;
    }
    else
    {
        cmap_mb_entry *entp;
        wchar_t *subp;

        /* 
         *   Get the mapping table entry for the lead byte.  The lead byte of
         *   the local code point is given by the high-order byte of the
         *   local code point.  (Note that this doesn't have anything to do
         *   with the endian-ness of the local platform.  The generic Unicode
         *   mapping tables are specifically designed this way, independently
         *   of endian-ness.)  
         */
        entp = &map_[(unsigned char)((local_code_pt >> 8) & 0xff)];

        /* 
         *   It's a two-byte character.  The high-order byte is the lead
         *   byte, and the low-order byte is the trailing byte of the
         *   two-byte sequence.  
         *   
         *   If we haven't previously set up a sub-table for the lead byte,
         *   do so now.  
         */
        if ((subp = entp->sub) == 0)
        {
            size_t i;
            wchar_t *p;

            /* allocate a new sub-mapping table for the lead byte */
            subp = entp->sub = (wchar_t *)t3malloc(256 * sizeof(wchar_t));

            /* initialize each entry to U+FFFD, in case we never map them */
            for (i = 256, p = subp ; i != 0 ; --i, *p++ = 0xFFFD) ;
        }

        /* set the mapping in the sub-table for the second byte */
        subp[(unsigned char)(local_code_pt & 0xff)] = uni_code_pt;
    }
}

/* 
 *   map a string, providing partial character info 
 */
size_t CCharmapToUniMB::map2(char **output_ptr, size_t *output_buf_len,
                             const char *input_ptr, size_t input_len,
                             size_t *partial_len) const
{
    size_t needed_out_len;

    /* presume we won't have a partial last character */
    *partial_len = 0;

    /* we haven't found anything to store in the output yet */
    needed_out_len = 0;

    /* keep going until we've mapped each character */
    while (input_len != 0)
    {
        unsigned char c;
        const cmap_mb_entry *entp;
        wchar_t wc;
        size_t wlen;

        /* get the lead byte of the next input character */
        c = *input_ptr;

        /* get the primary mapping table entry for the lead byte */
        entp = &map_[c];

        /* check for a one-byte or two-byte mapping */
        if (entp->sub == 0)
        {
            /* it's a one-byte character - get the mapping */
            wc = entp->ch;

            /* skip the single byte of input */
            ++input_ptr;
            --input_len;
        }
        else
        {
            /* 
             *   it's a two-byte character lead byte - make sure we have a
             *   complete input character 
             */
            if (input_len < 2)
            {
                /* we have an incomplete last character - tell the caller */
                *partial_len = 1;

                /* we're done mapping it */
                break;
            }

            /* get the second byte of the sequence */
            c = input_ptr[1];

            /* get the translation from the sub-table */
            wc = entp->sub[c];

            /* skip the two-byte sequence */
            input_ptr += 2;
            input_len -= 2;
        }

        /* we have the translation - note its stored UTF-8 byte size */
        wlen = utf8_ptr::s_wchar_size(wc);

        /* check for room to store the output character */
        if (wlen > *output_buf_len)
        {
            /* 
             *   there's no room to store this character - zero out the
             *   output buffer length so that we know not to try storing
             *   anything else in the buffer 
             */
            *output_buf_len = 0;
        }
        else
        {
            /* there's room - store it */
            wlen = utf8_ptr::s_putch(*output_ptr, wc);

            /* consume output buffer space */
            *output_ptr += wlen;
            *output_buf_len -= wlen;
        }

        /* count the needed length, whether we stored it or not */
        needed_out_len += wlen;
    }

    /* return the required output length */
    return needed_out_len;
}

/* 
 *   read from a multi-byte input file, translating to UTF-8 
 */
size_t CCharmapToUniMB::read_file(CVmDataSource *fp, char *buf, size_t bufl)
{
    size_t inlen;
    size_t outlen;
    size_t partial;

    /*
     *   Compute how much to read from the file.  The input file is composed
     *   of one-byte or two-byte characters, so only read up to one-third of
     *   the caller's buffer length; this will ensure that in the worst case
     *   we can always fit what we read into the caller's buffer.  (The worst
     *   case is that the input is entirely single-byte local characters that
     *   translate into three-byte UTF-8 characters.)  
     */
    inlen = bufl / 3;

    /* in any case, we can't read more than our own buffer size */
    if (inlen >= sizeof(inbuf_))
        inlen = sizeof(inbuf_);

    /* read raw bytes from the file */
    inlen = fp->readc(inbuf_, inlen);

    /* 
     *   Map data to the caller's buffer.  Note if we have a partial
     *   character at the end of the buffer (i.e., the last byte of the
     *   buffer is a lead byte that requires a second byte to make up a
     *   complete two-byte local character), so that we can read an
     *   additional byte to complete the two-byte final character if
     *   necessary.  
     */
    outlen = map2(&buf, &bufl, inbuf_, inlen, &partial);

    /* 
     *   if we have a partial trailing character, read the other half of the
     *   final character 
     */
    if (partial != 0)
    {
        /* move the lead byte to the start of our buffer */
        inbuf_[0] = inbuf_[inlen - 1];

        /* read the extra byte to form a complete character */
        inlen = 1 + fp->readc(inbuf_ + 1, 1);

        /* if we got the second byte, map the complete final character */
        if (inlen == 2)
            outlen += map(&buf, &bufl, inbuf_, inlen);
    }

    /* return the result length */
    return outlen;
}

