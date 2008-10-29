/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */

/*
 *   Unicode Case Mapping Generator
 *   
 *   Read the Unicode database text file, and use the information in the
 *   file to generate C++ source code for functions that perform
 *   lower-to-upper and upper-to-lower case mapping and character
 *   classifications for upper-case, lower-case, alphabetic, and numeric
 *   characters.
 *   
 *   We generate our tables as a two-level array, since the mappings are
 *   sparse (i.e., the majority of character positions have no mappings,
 *   because they do not contain letters or contain letters with no case
 *   conversions).
 *   
 *   The size of the lower-level array page can be set via the
 *   CASE_PAGE_SIZE macro defined below.  This value is arbitrary, but
 *   it's probably most efficient to choose a power of two so that the
 *   compiler can generate code that computes quotients and remainders
 *   using bit mask and shift operations.  Most importantly, though,
 *   CASE_PAGE_SIZE should be chosen to yield the smallest overall data
 *   size.  It's not trivial to guess what might give us the smallest
 *   overall data size, because the arrangement of the characters in the
 *   Unicode character set is somewhat arbitrary.  Experimenting with
 *   different CASE_PAGE_SIZE values is the easiest way to find a good
 *   value.  To facilitate choosing a good size, we insert a comment at
 *   the end of our derived output with the total static data size.
 *   Whenever we generate a new mapping file, we should experiment with a
 *   few values of CASE_PAGE_SIZE (powers of two from 32 to 2048 are good
 *   things to try) to find an efficient size.  As of this writing, the
 *   value chosen below seems to produce the smallest overall table data.
 *   
 *   THE UNICODE CHARACTER LISTING SOURCE FILE IS NOT PART OF THE T3
 *   DISTRIBUTION.  Obtain the character listing file from the Unicode web
 *   site at:
 *   
 *   ftp://ftp.unicode.org/Public/UNIDATA/UnicodeData-Latest.txt 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmuni.h"

#define TRUE 1
#define FALSE 0

#define CASE_PAGE_SIZE 128
#define CASE_PAGE_COUNT (65536/CASE_PAGE_SIZE)
#define ATTR_PAGE_SIZE 128
#define ATTR_PAGE_COUNT (65536/ATTR_PAGE_SIZE)

/* 
 *   read a hex field 
 */
static void get_hex_field(char **p, unsigned long *val)
{
    /* presume failure */
    *val = 0;

    /* read the field value */
    sscanf(*p, "%x", val);

    /* skip to the next field */
    while (**p != '\0' && **p != ';')
        ++(*p);

    /* skip the separator */
    if (**p == ';')
        ++(*p);
}

/* 
 *   read a decimal field 
 */
static void get_dec_field(char **p, unsigned int *val)
{
    /* presume failure */
    *val = 0;

    /* read the field value */
    sscanf(*p, "%d", val);

    /* skip to the next field */
    while (**p != '\0' && **p != ';')
        ++(*p);

    /* skip the separator */
    if (**p == ';')
        ++(*p);
}

/*
 *   read a string field 
 */
/* 
 *   read a hex field 
 */
static void get_str_field(char **p, char *dst, size_t dstlen)
{
    /* 
     *   find the end of the field, copying characters as we go (up to the
     *   result buffer size) 
     */
    for ( ; **p != '\0' && **p != ';' ; ++(*p))
    {
        /* copy this character if we have room for it */
        if (dstlen > 1)
        {
            /* copy the character */
            *dst++ = **p;

            /* that's one less character we have room for */
            --dstlen;
        }
    }

    /* skip the separator */
    if (**p == ';')
        ++(*p);

    /* terminate the result if possible */
    if (dstlen > 0)
        *dst = '\0';
}


int main()
{
    size_t i;
    static char *ch_name[65536];
    static unsigned int *to_upper_page[CASE_PAGE_COUNT];
    static unsigned int *to_lower_page[CASE_PAGE_COUNT];
    static unsigned char *attr_page[ATTR_PAGE_COUNT];
    size_t data_size;

    /* clear the name list */
    for (i = 0 ; i < sizeof(ch_name)/sizeof(ch_name[0]) ; ++i)
        ch_name[i] = 0;

    /* start the file */
    /* copyright-date-string */
    printf("/* Copyright (c) 1999, 2007 Michael J. Roberts */\n"
           "/*\n"
           " *  TADS 3 Case Conversion Table\n"
           " *\n"
           " *  THIS IS A MECHANICALLY DERIVED FILE.  DO NOT EDIT.\n"
           " *\n"
           " *  This file is mechanically derived from the Unicode\n"
           " *  character database listing.\n"
           " */\n"
           "\n"
           "#include <stdlib.h>\n"
           "\n"
           "#include \"vmuni.h\"\n"
           "\n");

    /* so far, we haven't found any characters, so no pages are used */
    for (i = 0 ; i < CASE_PAGE_COUNT ; ++i)
        to_upper_page[i] = to_lower_page[i] = 0;

    /* likewise for the attribute lists */
    for (i = 0 ; i < ATTR_PAGE_COUNT ; ++i)
        attr_page[i] = 0;

    /* no data allocated yet */
    data_size = 0;

    /* read from standard input until we exhaust the data source */
    for (;;)
    {
        char buf[256];
        char *p;
        unsigned long ch;
        char cname[40];
        char chartype[10];
        unsigned int combining;
        char bidi[10];
        char decomp[128];
        unsigned int decimal_val;
        unsigned int digit_val;
        char numeric_val[20];
        char mirror[5];
        char uni1name[40];
        char comment[20];
        unsigned long upper;
        unsigned long lower;
        unsigned long title;
        unsigned char attr;
        
        /* read the next line; stop at EOF */
        if (fgets(buf, sizeof(buf), stdin) == 0)
            break;

        /* start parsing at the beginning of the buffer */
        p = buf;

        /* read the fields */
        get_hex_field(&p, &ch);
        get_str_field(&p, cname, sizeof(cname));
        get_str_field(&p, chartype, sizeof(chartype));
        get_dec_field(&p, &combining);
        get_str_field(&p, bidi, sizeof(bidi));
        get_str_field(&p, decomp, sizeof(decomp));
        get_dec_field(&p, &decimal_val);
        get_dec_field(&p, &digit_val);
        get_str_field(&p, numeric_val, sizeof(numeric_val));
        get_str_field(&p, mirror, sizeof(mirror));
        get_str_field(&p, uni1name, sizeof(uni1name));
        get_str_field(&p, comment, sizeof(comment));
        get_hex_field(&p, &upper);
        get_hex_field(&p, &lower);
        get_hex_field(&p, &title);

        /* 
         *   If the character code is zero, it means either that this is the
         *   null character definition or that this is a comment line or a
         *   blank line - ignore all of these cases.  If the character is
         *   over 0xffff, ignore it as well, since we only handle 16-bit
         *   unicode values.  
         */
        if (ch == 0 || ch > 0xffff)
            continue;

        /* save this character name */
        ch_name[ch] = (char *)malloc(strlen(cname) + 1);
        strcpy(ch_name[ch], cname);

        /* presume we won't find any special attributes */
        attr = T3_CTYPE_NONE;

        /* check the character type and set the attributes accordingly */
        switch(chartype[0])
        {
        case 'L':
            /* get the case */
            switch(chartype[1])
            {
            case 'u':
            case 't':
                /* upper or title case - mark these as upper-case */
                attr = T3_CTYPE_UPPER;
                break;

            case 'l':
                /* lower-case letter */
                attr = T3_CTYPE_LOWER;
                break;

            case 'o':
                /* letter without case */
                attr = T3_CTYPE_ALPHA;
            }
            break;

        case 'N':
            /* if it's a decimal digit, note it */
            if (chartype[1] == 'd')
                attr = T3_CTYPE_DIGIT;
            break;

        case 'Z':
            /* it's a space of some kind */
            attr = T3_CTYPE_SPACE;
            break;

        case 'P':
            /* punctuation of some kind */
            attr = T3_CTYPE_PUNCT;
            break;

        case 'C':
            /* 
             *   control character - check the bidi class to see if it's a
             *   spacing character of some kind 
             */
            if ((bidi[0] == 'B' && bidi[1] == '\0')
                || (bidi[0] == 'W' && bidi[1] == 'S' && bidi[2] == '\0')
                || (bidi[0] == 'S' && bidi[1] == '\0'))
                attr = T3_CTYPE_SPACE;
            break;
        }

        /* if the character has an attribute, add it to the table */
        if (attr != T3_CTYPE_NONE)
        {
            /* if the page isn't mapped, map it now */
            if (attr_page[ch / ATTR_PAGE_SIZE] == 0)
            {
                /* allocate the page */
                attr_page[ch / ATTR_PAGE_SIZE] =
                    (unsigned char *)malloc(ATTR_PAGE_SIZE
                                            * sizeof(unsigned char));

                /* clear it out */
                memset(attr_page[ch / ATTR_PAGE_SIZE],
                       T3_CTYPE_NONE, ATTR_PAGE_SIZE * sizeof(unsigned char));
            }

            /* add the mapping */
            attr_page[ch / ATTR_PAGE_SIZE][ch % ATTR_PAGE_SIZE] = attr;
        }

        /* if this character has an upper-case mapping, add the entry */
        if (upper != 0)
        {
            /* if the page isn't mapped, map it now */
            if (to_upper_page[ch / CASE_PAGE_SIZE] == 0)
            {
                /* allocate the page */
                to_upper_page[ch / CASE_PAGE_SIZE] =
                    (unsigned int *)malloc(CASE_PAGE_SIZE
                                           * sizeof(unsigned int));

                /* clear it out */
                memset(to_upper_page[ch / CASE_PAGE_SIZE],
                       0, CASE_PAGE_SIZE * sizeof(unsigned int));
            }

            /* add the mapping */
            to_upper_page[ch / CASE_PAGE_SIZE][ch % CASE_PAGE_SIZE] = upper;
        }

        /* if this character has a lower-case mapping, add the entry */
        if (lower != 0)
        {
            /* if the page isn't mapped, map it now */
            if (to_lower_page[ch / CASE_PAGE_SIZE] == 0)
            {
                /* allocate the page */
                to_lower_page[ch / CASE_PAGE_SIZE] =
                    (unsigned int *)malloc(CASE_PAGE_SIZE
                                           * sizeof(unsigned int));

                /* clear it out */
                memset(to_lower_page[ch / CASE_PAGE_SIZE],
                       0, CASE_PAGE_SIZE * sizeof(unsigned int));
            }

            /* add the mapping */
            to_lower_page[ch / CASE_PAGE_SIZE][ch % CASE_PAGE_SIZE] = lower;
        }
    }

    /* generate the attribute lists */
    for (i = 0 ; i < ATTR_PAGE_COUNT ; ++i)
    {
        /* if this page is used, generate it */
        if (attr_page[i] != 0)
        {
            size_t j;

            /* generate the header */
            printf("static const wchar_t attr_pg_%02x[%d] =\n{\n",
                   i, (int)ATTR_PAGE_SIZE);

            /* generate the entries */
            for (j = 0 ; j < ATTR_PAGE_SIZE ; ++j)
            {
                char *nm;
                char *attr_name;

                /* generate the attribute name macro */
                switch(attr_page[i][j])
                {
                case T3_CTYPE_NONE:
                    attr_name = "T3_CTYPE_NONE";
                    break;
                    
                case T3_CTYPE_ALPHA:
                    attr_name = "T3_CTYPE_ALPHA";
                    break;
                    
                case T3_CTYPE_UPPER:
                    attr_name = "T3_CTYPE_UPPER";
                    break;
                    
                case T3_CTYPE_LOWER:
                    attr_name = "T3_CTYPE_LOWER";
                    break;
                    
                case T3_CTYPE_DIGIT:
                    attr_name = "T3_CTYPE_DIGIT";
                    break;
                    
                case T3_CTYPE_SPACE:
                    attr_name = "T3_CTYPE_SPACE";
                    break;
                    
                case T3_CTYPE_PUNCT:
                    attr_name = "T3_CTYPE_PUNCT";
                    break;

                default:
                    attr_name = "???";
                    break;
                }

                /* get the name */
                nm = ch_name[i * ATTR_PAGE_SIZE + j];
                if (nm == 0)
                    nm = "(unused)";

                /* add the listing */
                printf("    %-15s,  /* %04x  %s */\n",
                       attr_name, i * ATTR_PAGE_SIZE + j, nm);
            }

            /* count the table in the data size (8-bit entries) */
            data_size += ATTR_PAGE_SIZE;

            /* generate the footer */
            printf("};\n\n");
        }
    }

    /* generate the attribute master page table */
    printf("static const wchar_t *t3_attr_main[%d] =\n{\n",
           (int)ATTR_PAGE_COUNT);
    for (i = 0 ; i < ATTR_PAGE_COUNT ; ++i)
    {
        if (attr_page[i] != 0)
            printf("    attr_pg_%02x,  /* %04x - %04x */\n",
                   i, i * ATTR_PAGE_SIZE,
                   i * ATTR_PAGE_SIZE + ATTR_PAGE_SIZE - 1);
        else
            printf("    0,           /* %04x - %04x */\n",
                   i * ATTR_PAGE_SIZE,
                   i * ATTR_PAGE_SIZE + ATTR_PAGE_SIZE - 1);
    }

    /* generate the get-character-type function itself */
    printf("};\n"
           "\n"
           "unsigned char t3_get_chartype(wchar_t ch)\n"
           "{\n"
           "    unsigned int pg = (ch / %d);\n"
           "    unsigned int ofs = (ch %% %d);\n"
           "\n"
           "    return (t3_attr_main[pg] != 0\n"
           "            && t3_attr_main[pg][ofs] != 0\n"
           "            ? t3_attr_main[pg][ofs]\n"
           "            : T3_CTYPE_NONE);\n"
           "}\n"
           "\n", ATTR_PAGE_SIZE, ATTR_PAGE_SIZE);

    /* generate the to-upper mappings */
    for (i = 0 ; i < CASE_PAGE_COUNT ; ++i)
    {
        /* if this page is used, generate it */
        if (to_upper_page[i] != 0)
        {
            size_t j;

            /* generate the header */
            printf("static const wchar_t to_upper_pg_%02x[%d] =\n{\n",
                   i, (int)CASE_PAGE_SIZE);

            /* generate the entries */
            for (j = 0 ; j < CASE_PAGE_SIZE ; ++j)
            {
                char *nm;

                /* get the name */
                nm = ch_name[i * CASE_PAGE_SIZE + j];
                if (nm == 0)
                    nm = "(unused)";

                /* add the listing */
                if (to_upper_page[i][j] != 0)
                    printf("    0x%04x,  /* %04x  %s */\n",
                           to_upper_page[i][j],
                           i * CASE_PAGE_SIZE + j, nm);
                else
                    printf("    0x0000,  /* %04x  %s */\n",
                           i * CASE_PAGE_SIZE + j, nm);
            }

            /* count the table in the data size (assume 16-bit entries) */
            data_size += CASE_PAGE_SIZE * 2;

            /* generate the footer */
            printf("};\n\n");
        }
    }

    /* generate the to-upper master page table */
    printf("static const wchar_t *t3_to_upper_main[%d] =\n{\n",
           (int)CASE_PAGE_COUNT);
    for (i = 0 ; i < CASE_PAGE_COUNT ; ++i)
    {
        if (to_upper_page[i] != 0)
            printf("    to_upper_pg_%02x,  /* %04x - %04x */\n",
                   i, i * CASE_PAGE_SIZE,
                   i * CASE_PAGE_SIZE + CASE_PAGE_SIZE - 1);
        else
            printf("    0,               /* %04x - %04x */\n",
                   i * CASE_PAGE_SIZE,
                   i * CASE_PAGE_SIZE + CASE_PAGE_SIZE - 1);
    }

    /* generate the to-upper function itself */
    printf("};\n"
           "\n"
           "wchar_t t3_to_upper(wchar_t ch)\n"
           "{\n"
           "    unsigned int pg = (ch / %d);\n"
           "    unsigned int ofs = (ch %% %d);\n"
           "\n"
           "    return (t3_to_upper_main[pg] != 0\n"
           "            && t3_to_upper_main[pg][ofs] != 0\n"
           "            ? t3_to_upper_main[pg][ofs]\n"
           "            : ch);\n"
           "}\n"
           "\n", CASE_PAGE_SIZE, CASE_PAGE_SIZE);

    /* generate the to-lower mappings */
    for (i = 0 ; i < CASE_PAGE_COUNT ; ++i)
    {
        /* if this page is used, generate it */
        if (to_lower_page[i] != 0)
        {
            size_t j;

            /* generate the header */
            printf("static const wchar_t to_lower_pg_%02x[%d] =\n{\n",
                   i, (int)CASE_PAGE_SIZE);

            /* generate the entries */
            for (j = 0 ; j < CASE_PAGE_SIZE ; ++j)
            {
                char *nm;

                /* get the name */
                nm = ch_name[i * CASE_PAGE_SIZE + j];
                if (nm == 0)
                    nm = "(unused)";

                /* add the listing */
                if (to_lower_page[i][j] != 0)
                    printf("    0x%04x,  /* %04x  %s */\n",
                           to_lower_page[i][j],
                           i * CASE_PAGE_SIZE + j, nm);
                else
                    printf("    0x0000,  /* %04x  %s*/\n",
                           i * CASE_PAGE_SIZE + j, nm);
            }

            /* count the data size (assume 16-bit entries) */
            data_size += CASE_PAGE_SIZE * 2;

            /* generate the footer */
            printf("};\n\n");
        }
    }

    /* generate the to-lower master page table */
    printf("static const wchar_t *t3_to_lower_main[%d] =\n{\n",
           (int)CASE_PAGE_COUNT);
    for (i = 0 ; i < CASE_PAGE_COUNT ; ++i)
    {
        if (to_lower_page[i] != 0)
            printf("    to_lower_pg_%02x,  /* %04x - %04x */\n",
                   i, i * CASE_PAGE_SIZE,
                   i * CASE_PAGE_SIZE + CASE_PAGE_SIZE - 1);
        else
            printf("    0,               /* %04x - %04x */\n",
                   i * CASE_PAGE_SIZE,
                   i * CASE_PAGE_SIZE + CASE_PAGE_SIZE - 1);
    }

    /* generate the to-lower function itself */
    printf("};\n"
           "\n"
           "wchar_t t3_to_lower(wchar_t ch)\n"
           "{\n"
           "    unsigned int pg = (ch / %d);\n"
           "    unsigned int ofs = (ch %% %d);\n"
           "\n"
           "    return (t3_to_lower_main[pg] != 0\n"
           "            && t3_to_lower_main[pg][ofs] != 0\n"
           "            ? t3_to_lower_main[pg][ofs]\n"
           "            : ch);\n"
           "}\n"
           "\n", CASE_PAGE_SIZE, CASE_PAGE_SIZE);

    /* 
     *   Make a note of the total data size.  Count the page data plus the
     *   4-byte pointers to the tables (the null pointers count in the
     *   master page table, too, of course). 
     */
    printf("/* total static data size (32-bit pointers) = %d bytes */\n",
           data_size + (CASE_PAGE_COUNT * 4)*2 + (ATTR_PAGE_COUNT * 4));

    /* success */
    return 0;
}

