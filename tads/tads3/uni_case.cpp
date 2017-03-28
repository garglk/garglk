/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */

/*
 *   Unicode Case Mapping Generator
 *   
 *   Read the Unicode character database files, and use the information in
 *   the file to generate C++ source code for functions that perform
 *   lower-to-upper and upper-to-lower case mapping and character
 *   classifications for upper-case, lower-case, alphabetic, and numeric
 *   characters.
 *   
 *   We read the following files:
 *   
 *.     UnicodeData.txt - the main character database file
 *.     SpecialCasing.txt - special upper/lower case conversions
 *.     CaseFolding.txt - case folding table
 *   
 *   The command line must provide the path prefix to the Unicode source
 *   files, in a format where we can simply append the filenames to the
 *   prefix to get the full path.  E.g., on Windows, specify "unidata\", on
 *   Unix "unidata/".
 *   
 *   We generate our tables as a two-level array, since the mappings are
 *   sparse (i.e., the majority of character positions have no mappings,
 *   because they do not contain letters or contain letters with no case
 *   conversions).
 *   
 *   The size of the lower-level array page can be set via the CASE_PAGE_SIZE
 *   macro defined below.  This value is arbitrary, but it's probably most
 *   efficient to choose a power of two so that the compiler can generate
 *   code that computes quotients and remainders using bit mask and shift
 *   operations.  Most importantly, though, CASE_PAGE_SIZE should be chosen
 *   to yield the smallest overall data size.  It's not trivial to guess what
 *   might give us the smallest overall data size, because the arrangement of
 *   the characters in the Unicode character set is somewhat arbitrary.
 *   Experimenting with different CASE_PAGE_SIZE values is the easiest way to
 *   find a good value.  To facilitate choosing a good size, we insert a
 *   comment at the end of our derived output with the total static data
 *   size.  Whenever we generate a new mapping file, we should experiment
 *   with a few values of CASE_PAGE_SIZE (powers of two from 32 to 2048 are
 *   good things to try) to find an efficient size.  As of this writing, the
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
#include <stdarg.h>
#include <ctype.h>
   
#include "vmuni.h"

#define TRUE 1
#define FALSE 0
#define countof(x) (sizeof(x)/sizeof((x)[0]))

#define PAGE_SIZE 128
#define PAGE_COUNT (65536/PAGE_SIZE)
#define PAGE_SHIFT 7
#define PAGE_MASK 0x7F

#define POINTER_BYTES 4

/* strip comments from an input line */
static void strip_comments(char *buf)
{
    char *p = strchr(buf, '#');
    if (p != 0)
    {
        /* skip any spaces before the comment */
        while (p > buf && isspace(*(p-1)))
            --p;

        /* end the line at the first space or '#' */
        *p = '\0';
    }
}

/* read a hex field */
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

/* read a multi-hex field - a list of hex character codes */
static void get_multihex_field(char **p, unsigned long *val, size_t valcnt)
{
    /* read fields */
    for (;;)
    {
        /* skip spaces */
        for ( ; isspace(**p) ; ++*p) ;

        /* if we're not at a hex digit, we're done */
        if (!isxdigit(**p))
            break;

        /* if we're out of space, flag an error */
        if (valcnt <= 1)
        {
            /* 
             *   not very good diagnostics, but it should never happen; if it
             *   ever does we can upgrade this as needed 
             */
            printf("#error get_multihex_field() value too long!\n");
            exit(1);
        }

        /* read the value */
        sscanf(*p, "%x", val);

        /* advance past this item in the output */
        ++val, --valcnt;

        /* skip to the next field */
        for ( ; isxdigit(**p) ; ++*p) ;
    }

    /* add a null terminator */
    *val = 0;

    /* if we're at the semicolon separator, skip it */
    if (**p == ';')
        ++(*p);
}

/* read a decimal field */
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

/* read a string field */
static void get_str_field(char **p, char *dst, size_t dstlen)
{
    /* skip leading spaces */
    for ( ; isspace(**p) ; ++(*p)) ;

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

/* terminate with a printf-style formatted error message */
static void errexit(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, "\n");
    exit(1);
}

/* character table entry */
struct chardef
{
    /* character name */
    char *name;

    /* type of this character as a T3_CTYPE_xxx value */
    unsigned char attr;

    const char *attr_name() const
    {
        static const char *names[] = { T3_CTYPE_NAMES };
        return names[attr];
    }

    /* lower-case conversion */
    wchar_t *lower;

    /* upper-case conversion */
    wchar_t *upper;

    /* title case conversion */
    wchar_t *title;

    /* case folding conversion */
    wchar_t *fold;

    /* does this entry have a simple folding? */
    int has_simple_folding;

    void set_name(const char *str)
    {
        name = new char[strlen(str) + 1];
        strcpy(name, str);
    }

    int name_ends_with(const char *str)
    {
        size_t slen = strlen(str);
        size_t nlen = strlen(name);
        return (nlen > slen && stricmp(name + nlen - slen, str) == 0);
    }

    void derive_name(const char *tpl, int ch)
    {
        char buf[256];
        sprintf(buf, "%s-%04X", tpl, ch);
        set_name(buf);
    }
};

/* the character table */
static chardef ctab[65536];

static int page_has_cases(int pg)
{
    chardef *c = ctab + pg*PAGE_SIZE;
    for (int i = 0 ; i < PAGE_SIZE ; ++i, ++c)
    {
        if (c->lower != 0 || c->upper != 0 || c->title != 0 || c->fold != 0)
            return TRUE;
    }
    return FALSE;
}

/* 
 *   allocate a wchar_t string from a string of 32-bit characters; if the
 *   string contains any invalid characters, we'll fail and return null 
 */
static wchar_t *alloc_wstr(const unsigned long *s, size_t len)
{
    /* check for invalid characters */
    size_t i;
    for (i = 0 ; i < len ; ++i)
    {
        /* if this is a null terminator, stop here */
        if (s[i] == 0)
        {
            len = i;
            break;
        }

        /* if this is an invalid character, return failure */
        if (s[i] > 0xffff)
            return 0;
    }

    /* if the length is zero, do nothing */
    if (len == 0)
        return 0;

    /* allocate space */
    wchar_t *ret = new wchar_t[len+1];

    /* copy the characters */
    for (i = 0 ; i < len ; ++i)
        ret[i] = (wchar_t)s[i];

    /* add the null terminator */
    ret[i] = 0;

    /* return the new string */
    return ret;
}

/* get the length in characters of a null-terminated wchar_t string */
static int wstr_len(const wchar_t *s)
{
    if (s == 0)
        return 0;
    
    int l;
    for (l = 0 ; *s != 0 ; ++s, ++l);
    return l;
}

/* compare two null-terminated wchar_t strings for equality */
static int wstr_eq(const wchar_t *a, const wchar_t *b)
{
    for ( ; *a != 0 && *b != 0 ; ++a, ++b)
    {
        if (*a != *b)
            return FALSE;
    }
    return *a == *b;
}

/* keep a case mapping if it's not empty and it's not a self reference */
static int keep_case(unsigned long *str, unsigned long ch)
{
    return (str[0] != 0
            && !(str[0] == ch && str[1] == 0));
}

/* copy a case expansion */
static int copy_case_exp(wchar_t *caseexp, int &caseofs, const wchar_t *str)
{
    /* if there's no string, or it's empty, there's no expansion */
    if (str == 0 || str[0] == 0)
        return 0;

    /* the return value will be the current offset */
    int ret = caseofs;

    /* copy the string into the expansion table */
    do {
        caseexp[caseofs++] = *str;
    } while (*str++ != 0);

    /* return the start of the expansion copy */
    return ret;
}

/*
 *   Main entrypoint 
 */
int main(int argc, char **argv)
{
    /* check arguments */
    if (argc < 2)
        errexit("usage: uni_case <database_dir_prefix>");

    /* get the path prefix for the database files */
    const char *path = argv[1];

    /* clear the character table */
    memset(ctab, 0, sizeof(ctab));

    /* start the output file */
    /* copyright-date-string */
    printf("/* Copyright (c) 1999, 2012 Michael J. Roberts */\n"
           "/*\n"
           " *  TADS 3 Case Conversion Table\n"
           " *\n"
           " *  THIS IS A GENERATED FILE.  DO NOT EDIT - any changes will\n"
           " *  be lost on the next build.\n"
           " *\n"
           " *  This file is mechanically derived from the Unicode\n"
           " *  character database listing.\n"
           " */\n"
           "\n"
           "#include <stdlib.h>\n"
           "\n"
           "#include \"vmuni.h\"\n"
           "\n");

    /* no data allocated yet */
    size_t data_size = 0;

    /* start with the main character database file */
    char fname[256];
    sprintf(fname, "%s%s", path, "UnicodeData.Txt");
    FILE *fp = fopen(fname, "r");
    if (fp == 0)
        errexit("unable to open main database file %s", fname);

    /* process the file */
    for (chardef *prv = 0 ; ; )
    {
        
        /* read the next line; stop at EOF */
        char buf[256];
        if (fgets(buf, sizeof(buf), fp) == 0)
        {
            fclose(fp);
            break;
        }

        /* start parsing at the beginning of the buffer */
        char *p = buf;
        
        /* read the fields */
        unsigned long ch, upper, lower, title;
        unsigned int decimal_val, digit_val, combining;
        char cname[40], chartype[40], bidi[10], decomp[128], numeric_val[20];
        char mirror[5], uni1name[40], comment[20];
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
         *   over 0xffff, ignore it as well, since we only handle the "base
         *   multilingual plane" characters, in range 0-ffff. 
         */
        if (ch == 0 || ch > 0xffff)
            continue;

        /* get the character table entry */
        chardef *cd = &ctab[ch];

        /* save this character name */
        cd->set_name(cname);

        /* presume we won't find any special attributes */
        cd->attr = T3_CTYPE_OTHER;

        /* check the character type and set the attributes accordingly */
        switch(chartype[0])
        {
        case 'L':
            /* get the case */
            switch(chartype[1])
            {
            case 'u':
                /* upper-case letter */
                cd->attr = T3_CTYPE_UPPER;
                break;

            case 't':
                /* 
                 *   title-case letter (actually, letters - I'm pretty sure
                 *   these are all ligatures of an upper-case and lower-case
                 *   letter) 
                 */
                cd->attr = T3_CTYPE_TITLE;
                break;
                
            case 'l':
                /* lower-case letter */
                cd->attr = T3_CTYPE_LOWER;
                break;

            case 'o':
                /* caseless letter */
                cd->attr = T3_CTYPE_ALPHA;
                break;
            }
            break;

        case 'N':
            /* if it's a decimal digit, note it */
            if (chartype[1] == 'd')
                cd->attr = T3_CTYPE_DIGIT;
            break;

        case 'Z':
            /* Zp, Zl -> vertical space; otherwise it's a horizontal space */
            cd->attr = (chartype[1] == 'p' || chartype[1] == 'l'
                        ? T3_CTYPE_VSPAC : T3_CTYPE_SPACE);
            break;

        case 'P':
            /* punctuation of some kind */
            cd->attr = T3_CTYPE_PUNCT;
            break;

        case 'C':
            /* 
             *   Control character - check the bidi class to see if it's a
             *   spacing character of some kind.
             *   
             *.     WS - ordinary horizontal whitespace
             *.     S - horizontal separator (tab, etc); counts as whitespace
             *.     B - vertical whitespace (CR, LF, etc)
             */
            if (ch == 0x0B)
            {
                /* TADS-specific \b character -> vertical whitespace */
                cd->attr = T3_CTYPE_VSPAC;
            }
            else if (ch == 0x0E || ch == 0x0F)
            {
                /* TADS-specific \v and \^ -> device controls */
                cd->attr = T3_CTYPE_OTHER;
            }
            else if (ch == 0x15)
            {
                /* TADS-specific '\ ' quoted space -> ordinary character */
                cd->attr = T3_CTYPE_OTHER;
            }
            else if ((bidi[0] == 'W' && bidi[1] == 'S' && bidi[2] == '\0')
                || (bidi[0] == 'S' && bidi[1] == '\0'))
            {
                /* WS, S -> whitespace */
                cd->attr = T3_CTYPE_SPACE;
            }
            else if (bidi[0] == 'B' && bidi[1] == '\0')
            {
                /* B -> vertical whitespace */
                cd->attr = T3_CTYPE_VSPAC;
            }
            break;
        }

        /* set the case conversion mappings */
        cd->upper = alloc_wstr(&upper, 1);
        cd->lower = alloc_wstr(&lower, 1);
        cd->title = alloc_wstr(&title, 1);

        /*
         *   If the name ends in "last>", and the previous item's name ends
         *   in "first>", we have a range of identical character definitions.
         *   Repeat the attributes over the range. 
         */
        if (prv != 0 && cd->name_ends_with(", last>")
            && prv->name_ends_with(", first>"))
        {
            char *name = prv->name + 1;
            name[strlen(name) - 8] = '\0';
            for (chardef *cur = prv ; cur <= cd ; ++cur)
            {
                *cur = *prv;
                cur->derive_name(name, cur - ctab);
            }
        }

        /* this is now the previous item */
        prv = cd;
    }

    /* open the special case mapping file */
    sprintf(fname, "%s%s", path, "SpecialCasing.txt");
    if ((fp = fopen(fname, "r")) == 0)
        errexit("unable to open special case mapping file %s", fname);

    /* process it */
    for (;;)
    {
        /* read the next line */
        char buf[256];
        if (fgets(buf, sizeof(buf), fp) == 0)
        {
            fclose(fp);
            break;
        }

        /* remove any trailing comment (and spaces leading up to it) */
        strip_comments(buf);

        /* decode the fields */
        unsigned long ch, lower[40], title[40], upper[40];
        char cond[128];
        char *p = buf;
        get_hex_field(&p, &ch);
        get_multihex_field(&p, lower, countof(lower));
        get_multihex_field(&p, title, countof(title));
        get_multihex_field(&p, upper, countof(upper));
        get_str_field(&p, cond, sizeof(cond));

        /* skip blank lines and characters outside the BMP */
        if (ch == 0 || ch > 0xffff)
            continue;

        /* if this item has a condition, skip it - we don't handle these */
        if (cond[0] != '\0')
            continue;

        /* 
         *   set the updated case conversion fields; omit self-references to
         *   save space 
         */
        chardef *cd = &ctab[ch];
        if (keep_case(lower, ch))
            cd->lower = alloc_wstr(lower, countof(lower));
        if (keep_case(title, ch))
            cd->title = alloc_wstr(title, countof(title));
        if (keep_case(upper, ch))
            cd->upper = alloc_wstr(upper, countof(upper));
    }

    /* open the case folding file */
    sprintf(fname, "%s%s", path, "CaseFolding.txt");
    if ((fp = fopen(fname, "r")) == 0)
        errexit("unable to open special case mapping file %s", fname);

    /* process it */
    for (;;)
    {
        /* read the next line */
        char buf[256];
        if (fgets(buf, sizeof(buf), fp) == 0)
        {
            fclose(fp);
            break;
        }

        /* remove any trailing comment (and spaces leading up to it) */
        strip_comments(buf);

        /* decode the fields */
        unsigned long ch, fold[40];
        char status[40];
        char *p = buf;
        get_hex_field(&p, &ch);
        get_str_field(&p, status, sizeof(status));
        get_multihex_field(&p, fold, countof(fold));

        /* skip blank lines and charaters outside the BMP */
        if (ch == 0 || ch > 0xffff)
            continue;

        /* get the character table entry */
        chardef *cd = &ctab[ch];

        /* 
         *   If it's a "simple" folding, skip it, but verify our assumption
         *   that all simple foldings are identical to the single-character
         *   lower-case conversion in the main UnicodeData.txt table.  At
         *   run-time, we use the lower-case conversion in instances where we
         *   want a 1:1 folding.
         */
        if (status[0] == 'S')
        {
            /* validate our assumption that Simple Folding == lower case */
            if (cd->lower == 0
                || wstr_len(cd->lower) != 1
                || cd->lower[0] != fold[0])
            {
                printf("#error Simple folding ('S') for %04x in "
                       "CaseFolding.txt doesn't match "
                       "1:1 lower-case conversion in UnicodeData.txt\n", ch);
            }

            /* mark the entry as having a simple folding */
            cd->has_simple_folding = TRUE;

            /* otherwise ignore simple foldings */
            continue;
        }

        /* 
         *   if it's a Common entry, it counts as both a Simple and Full
         *   folding - mark the entry as having a Simple folding 
         */
        if (status[0] == 'C')
            cd->has_simple_folding = TRUE;

        /* 
         *   Ignore the Turkic-only folding rules.  We don't currently have a
         *   mechanism to select rules by language, so we can't use these.
         */
        if (status[0] == 'T')
            continue;

        /* store it */
        cd->fold = alloc_wstr(fold, countof(fold));
    }
    
    /* 
     *   Generate the character type table.  We only have 9 character types
     *   currently, so a character type fits in 4 bits; we can thus pack two
     *   types in a byte.  So pack each pair of characters, with the even
     *   character in the low nybble and the odd character in the high
     *   nybble.  (This lets us do the decoding entirely with bit masks and
     *   bit shifts, which might be a little quicker than a conditional.)
     */
    data_size += 0x8000;
    printf("static const unsigned char chartype[32768] =\n{\n");
    int i;
    for (i = 0 ; i < 0x10000 ; i += 2)
    {
        /* pack the next two characters */
        printf("    (%s << 4) | %s%c   /* %04x %s | %04x %s */\n",
               ctab[i+1].attr_name(), ctab[i].attr_name(),
               i + 2 == 0x10000 ? ' ' : ',',
               i+1, ctab[i+1].name, i, ctab[i].name);
    }
    printf("};\n"
           "int t3_get_chartype(wchar_t ch)\n"
           "{\n"
           "    return (chartype[ch >> 1] >> ((ch & 1) << 2)) & 0x0F;\n"
           "}\n"
           "\n");

    /*
     *   Build the case conversion tables.  Break these up into pages, since
     *   the case conversions are only used sparsely.
     *   
     *   Each case conversion entry is a 16-bit offset into the case
     *   expansion array, which consists of null-terminated strings of
     *   wchar_t's (explicitly as uint16's, since we don't currently support
     *   anything outside the BMP).  E.g., the upper-case conversion for 'a'
     *   will point via an array offset to the string 'A', '\0'.  Empirically
     *   we need well under 64k entries, which is why we can get away with a
     *   16-bit offset; we want to use the smallest type for the offset to
     *   minimize space to store the table.  Since we know this is a hard
     *   upper bound, we can pre-allocate the expansion table now at this
     *   size; when we write it, of course, we'll only write as much as we
     *   actually end up populating.
     */
    wchar_t *caseexp = new wchar_t[65536];

    /* 
     *   start the table with a null - we use index 0 to mean null, so we'll
     *   never point anything here, and in addition we want to ensure there's
     *   a null character before the start of each entry, so zeroing this
     *   element satisfies both purposes 
     */
    caseexp[0] = 0;

    /* start the table */
    printf("struct case_table\n"
           "{\n"
           "    uint16_t lower;\n"
           "    uint16_t title;\n"
           "    uint16_t upper;\n"
           "    uint16_t fold;\n"
           "};\n"
           "\n");
    int caseofs = 1;
    for (i = 0 ; i < PAGE_COUNT ; ++i)
    {
        /* build this page only if it has any entries */
        if (page_has_cases(i))
        {
            data_size += 4*2*PAGE_SIZE;

            printf("static case_table case_pg_%03x[%d] =\n{\n", i, PAGE_SIZE);
            chardef *ch = &ctab[i*PAGE_SIZE];
            for (int j = 0 ; j < PAGE_SIZE ; ++j, ++ch)
            {
                /* 
                 *   Check our assumption that all lower-case mappings are
                 *   single characters, with a single exception (see below).
                 *   The reason we make this assumption is that it allows us
                 *   to use the lower mapping as the simple case folding, so
                 *   that we don't have to store a separate field (and more
                 *   data) for the simple folding.
                 *   
                 *   U+0130, capital I with dot above, is the only entry in
                 *   the current database that violates this assumption.  It
                 *   does so because the regular lower case i already has a
                 *   dot above in its visual rendering, and there's no such
                 *   character as "lower case i with the normal dot above and
                 *   also an accent dot".  We need such a character in order
                 *   to retain the information that there's this dot accent
                 *   as opposed to the ordinary 'i' dot.  The information is
                 *   important for comparisons, conversions back to upper
                 *   case, etc.  Since there's no such character, we have to
                 *   compose one out of an 'i' and a combining dot above
                 *   character.  Anyway, U+0130 doesn't have a simple folding
                 *   at all, meaning it can only match itself in folded text
                 *   (i.e., there's no lower-case character it can match).
                 *   We handle this by treating the lower-case mapping for a
                 *   character as its simple folding expansion only when the
                 *   lower-case mapping is one character long; if the mapping
                 *   is longer, we treat the character as having no simple
                 *   folding.
                 */
                if (ch->lower != 0 && ch->lower[0] != 0 && ch->lower[1] != 0
                    && ch != &ctab[0x130])
                    printf("#error Lower-case mapping for %04x is longer "
                           "than one character\n", (int)(ch - ctab));

                /*
                 *   Now verify our second assumption, that any character
                 *   with a simple single-character lower-case mapping has a
                 *   simple case folding. 
                 */
                if (ch->lower != 0 && ch->lower[0] != 0 && ch->lower[1] == 0
                    && !ch->has_simple_folding)
                    printf("#error %04x has a one-character lower-case "
                           "mapping but no simple folding\n",
                           (int)(ch - ctab));

                int upper = 0, title = 0, lower = 0, fold = 0;
                lower = copy_case_exp(caseexp, caseofs, ch->lower);
                title = copy_case_exp(caseexp, caseofs, ch->title);

                /* 
                 *   in many cases (most, actually), the title and upper
                 *   strings are identical - if so, point them both to the
                 *   same table entry to save space
                 */
                if (ch->title != 0 && ch->upper != 0
                    && wstr_eq(ch->title, ch->upper))
                    upper = title;
                else
                    upper = copy_case_exp(caseexp, caseofs, ch->upper);

                /*
                 *   similarly, most case folding strings are identical to
                 *   the lower-case strings
                 */
                if (ch->fold != 0 && ch->lower != 0
                    && wstr_eq(ch->fold, ch->lower))
                    fold = lower;
                else
                    fold = copy_case_exp(caseexp, caseofs, ch->fold);
                
                printf("    { 0x%04x, 0x%04x, 0x%04x, 0x%04x }%c"
                       "  /* %04x %s */\n",
                       lower, title, upper, fold,
                       j + 1 < PAGE_SIZE ? ',' : ' ',
                       ch - ctab, ch->name);
            }
            printf("};\n\n");
        }
    }

    /* build the case conversion page lookup table */
    data_size += POINTER_BYTES * PAGE_COUNT;
    printf("static case_table *case_tab[%d] =\n{\n", PAGE_COUNT);
    for (i = 0 ; i < PAGE_COUNT ; ++i)
    {
        char comma = (i + 1 < PAGE_COUNT ? ',' : ' ');
        if (page_has_cases(i))
            printf("    case_pg_%03x%c\n", i, comma);
        else
            printf("    0%c\n", comma);
    }
    printf("};\n\n");

    /* write the case conversion expansion table */
    data_size += caseofs*2;
    printf("static wchar_t case_expansion[] =\n{\n    /* 0000 */ ");
    for (i = 0 ; i < caseofs ; ++i)
    {
        printf("0x%04x", caseexp[i]);
        if (i + 1 < caseofs)
        {
            if (caseexp[i] == 0)
                printf(",\n    /* %04x */ ", i+1);
            else
                printf(", ");
        }
    }
    printf("\n};\n\n");

    /* generate the case conversion lookup functions */
    static const char *case_name[] = { "lower", "title", "upper", "fold" };
    for (i = 0 ; i < countof(case_name) ; ++i)
    {
        printf("const wchar_t *t3_to_%s(wchar_t ch)\n"
               "{\n"
               "    case_table *e = case_tab[ch >> %d];\n"
               "    if (e == 0)\n"
               "        return 0;\n"
               "    int ofs = e[ch & %d].%s;\n"
               "    return ofs == 0 ? 0 : &case_expansion[ofs];\n"
               "}\n\n",
               case_name[i],
               PAGE_SHIFT, PAGE_MASK,
               case_name[i]);
    }

    /* generate the simple folding lookup function */
    printf("wchar_t t3_simple_case_fold(wchar_t ch)\n"
           "{\n"
           "    case_table *e = case_tab[ch >> %d];\n"
           "    if (e == 0)\n"
           "        return 0;\n"
           "    int ofs = e[ch & %d].lower;\n"
           "    return ofs == 0 ? ch :\n"
           "           case_expansion[ofs + 1] != 0 ? ch :\n"
           "           case_expansion[ofs];\n"
           "}\n\n",
           PAGE_SHIFT, PAGE_MASK);

    /* 
     *   Make a note of the total data size.  Count the page data plus the
     *   4-byte pointers to the tables (the null pointers count in the
     *   master page table, too, of course). 
     */
    printf("/* total static data size (%d-bit pointers) = %d bytes */\n",
           POINTER_BYTES*4, data_size);

    /* success */
    return 0;
}

