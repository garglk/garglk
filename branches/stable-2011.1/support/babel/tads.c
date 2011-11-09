/* 
 *   tads.c - Treaty of Babel common functions for tads2 and tads3 modules
 *   
 *   This file depends on treaty_builder.h
 *   
 * This file has been released into the public domain by its author.
* The author waives all of his rights to the work
* worldwide under copyright law to the maximum extent allowed by law
* , but note that any changes to this file may
 *   render it noncompliant with the Treaty of Babel
 *   
 *   Modified
 *.   04/08/2006 LRRaszewski - changed babel API calls to threadsafe versions
 *.   04/08/2006 MJRoberts  - initial implementation
 */


#include "treaty.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tads.h"
#include "md5.h"

#define ASSERT_OUTPUT_SIZE(x) \
    do { if (output_extent < (x)) return INVALID_USAGE_RV; } while (0)

#define T2_SIGNATURE "TADS2 bin\012\015\032"
#define T3_SIGNATURE "T3-image\015\012\032"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
/* ------------------------------------------------------------------------ */
/*
 *   private structures 
 */

/*
 *   resource information structure - this encapsulates the location and size
 *   of a binary resource object embedded in a story file 
 */
typedef struct resinfo resinfo;
struct resinfo
{
    /* pointer and length of the data in the story file buffer */
    const char *ptr;
    int32 len;

    /* tads major version (currently, 2 or 3) */
    int tads_version;
};

/*
 *   Name/value pair list entry 
 */
typedef struct valinfo valinfo;
struct valinfo
{
    const char *name;
    size_t name_len;

    /* value string */
    char *val;
    size_t val_len;

    /* next entry in the list */
    valinfo *nxt;
};


/* ------------------------------------------------------------------------ */
/*
 *   forward declarations 
 */

static valinfo *parse_sf_game_info(const void *story_file, int32 story_len,
                                   int *version);
static valinfo *parse_game_info(const char *gi_file, int32 gi_len);
static int find_resource(const void *story_file, int32 story_len,
                         const char *resname, resinfo *info);
static int find_cover_art(const void *story_file, int32 story_len,
                          resinfo *resp, int32 *image_format,
                          int32 *width, int32 *height);
static int t2_find_res(const void *story_file, int32 story_len,
                       const char *resname, resinfo *info);
static int t3_find_res(const void *story_file, int32 story_len,
                       const char *resname, resinfo *info);
static valinfo *find_by_key(valinfo *list_head, const char *key);
static void delete_valinfo_list(valinfo *head);
static int32 generate_md5_ifid(void *story_file, int32 extent,
                               char *output, int32 output_extent);
static int32 synth_ifiction(valinfo *vals, int tads_version,
                            char *buf, int32 bufsize,
                            void *story_file, int32 extent);
static int get_png_dim(const void *img, int32 extent,
                       int32 *xout, int32 *yout);
static int get_jpeg_dim(const void *img, int32 extent,
                        int32 *xout, int32 *yout);



/* ------------------------------------------------------------------------ */
/*
 *   Get the IFID for a given story file.  
 */
int32 tads_get_story_file_IFID(void *story_file, int32 extent,
                               char *output, int32 output_extent)
{
    valinfo *vals;
    
    /* if we have GameInfo, try looking for an IFID there */
    if ((vals = parse_sf_game_info(story_file, extent, 0)) != 0)
    {
        valinfo *val;
        int found = 0;
        
        /* find the "IFID" key */
        if ((val = find_by_key(vals, "IFID")) != 0)
        {
            char *p;
            
            /* copy the output as a null-terminated string */
            ASSERT_OUTPUT_SIZE((int32)val->val_len + 1);
            memcpy(output, val->val, val->val_len);
            output[val->val_len] = '\0';

            /* 
             *   count up the IFIDs in the buffer - there might be more than
             *   one, separated by commas 
             */
            for (found = 1, p = output ; *p != '\0' ; ++p)
            {
                /* if this is a comma, it delimits a new IFID */
                if (*p == ',')
                    ++found;
            }
        }

        /* delete the GameInfo list */
        delete_valinfo_list(vals);

        /* if we found an IFID, indicate how many results we found */
        if (found != 0)
            return found;
    }

    /* 
     *   we didn't find an IFID in the GameInfo, so generate a default IFID
     *   using the MD5 method 
     */
    return generate_md5_ifid(story_file, extent, output, output_extent);
}

/*
 *   Get the size of the ifiction metadata for the game 
 */
int32 tads_get_story_file_metadata_extent(void *story_file, int32 extent)
{
    valinfo *vals;
    int32 ret;
    int ver;
    
    /*
     *   First, make sure we have a GameInfo record.  If we don't, simply
     *   indicate that there's no metadata to fetch.  
     */
    if ((vals = parse_sf_game_info(story_file, extent, &ver)) == 0)
        return NO_REPLY_RV;

    /*
     *   Run the ifiction synthesizer with no output buffer, to calculate the
     *   size we need. 
     */
    ret = synth_ifiction(vals, ver, 0, 0, story_file, extent);

    /* delete the value list */
    delete_valinfo_list(vals);

    /* return the required size */
    return ret;
}

/*
 *   Get the ifiction metadata for the game
 */
int32 tads_get_story_file_metadata(void *story_file, int32 extent,
                                   char *buf, int32 bufsize)
{
    valinfo *vals;
    int32 ret;
    int ver;

    /* make sure we have metadata to fetch */
    if ((vals = parse_sf_game_info(story_file, extent, &ver)) == 0)
        return NO_REPLY_RV;

    /* synthesize the ifiction data into the output buffer */
    ret = synth_ifiction(vals, ver, buf, bufsize, story_file, extent);

    /* if that required more space than we had available, return an error */
    if (ret > bufsize)
        ret = INVALID_USAGE_RV;

    /* delete the value list */
    delete_valinfo_list(vals);

    /* return the result */
    return ret;
}

/*
 *   Extended interface: turn a GameInfo record into an iFiction record.
 *   Call with a null buffer and zero buffer len to figure the size of the
 *   output buffer required.  
 */
int32 xtads_gameinfo_to_ifiction(int tads_version,
                                 const char *gi_txt, int32 gi_len,
                                 char *buf, int32 bufsize)
{
    valinfo *vals;
    int32 ret;

    /* make sure we have metadata to fetch */
    if ((vals = parse_game_info(gi_txt, gi_len)) == 0)
        return NO_REPLY_RV;

    /* synthesize the ifiction data into the output buffer */
    ret = synth_ifiction(vals, tads_version, buf, bufsize, 0, 0);

    /* if that required more space than we had available, return an error */
    if (bufsize != 0 && ret > bufsize)
        ret = INVALID_USAGE_RV;

    /* delete the value list */
    delete_valinfo_list(vals);

    /* return the result */
    return ret;
}

/*
 *   Get the size of the cover art 
 */
int32 tads_get_story_file_cover_extent(void *story_file, int32 story_len)
{
    resinfo res;
    
    /* look for the cover art resource */
    if (find_cover_art(story_file, story_len, &res, 0, 0, 0))
        return res.len;
    else
        return NO_REPLY_RV;
}

/*
 *   Get the format of the cover art 
 */
int32 tads_get_story_file_cover_format(void *story_file, int32 story_len)
{
    int32 typ;

    /* look for CoverArt.jpg */
    if (find_cover_art(story_file, story_len, 0, &typ, 0, 0))
        return typ;
    else
        return NO_REPLY_RV;
}

/*
 *   Get the cover art data 
 */
int32 tads_get_story_file_cover(void *story_file, int32 story_len,
                                void *outbuf, int32 output_extent)
{
    resinfo res;

    /* look for CoverArt.jpg, then for CoverArt.png */
    if (find_cover_art(story_file, story_len, &res, 0, 0, 0))
    {
        /* got it - copy the data to the buffer */
        ASSERT_OUTPUT_SIZE(res.len);
        memcpy(outbuf, res.ptr, res.len);

        /* success */
        return res.len;
    }

    /* otherwise, we didn't find it */
    return NO_REPLY_RV;
}

/* ------------------------------------------------------------------------ */
/*
 *   Generate a default IFID using the MD5 hash method 
 */
static int32 generate_md5_ifid(void *story_file, int32 extent,
                               char *output, int32 output_extent)
{
    md5_state_t md5;
    unsigned char md5_buf[16];
    char *p;
    int i;

    /* calculate the MD5 hash of the story file */
    md5_init(&md5);
    md5_append(&md5, story_file, extent);
    md5_finish(&md5, md5_buf);

    /* make sure we have room to store the result */
    ASSERT_OUTPUT_SIZE(39);

    /* the prefix is "TADS2-" or "TADS3-", depending on the format */
    if (tads_match_sig(story_file, extent, T2_SIGNATURE))
        strcpy(output, "TADS2-");
    else
        strcpy(output, "TADS3-");

    /* the rest is the MD5 hash of the file, as hex digits */
    for (i = 0, p = output + strlen(output) ; i < 16 ; p += 2, ++i)
        sprintf(p, "%02X", md5_buf[i]);

    /* indicate that we found one result */
    return 1;
}

/* ------------------------------------------------------------------------ */
/*
 *   Some UTF-8 utility functions and macros.  We use our own rather than the
 *   ctype.h macros because we're parsing UTF-8 text.  
 */

/* is c a space? */
#define u_isspace(c) ((unsigned char)(c) < 128 && isspace(c))

/* is c a horizontal space? */
#define u_ishspace(c) (u_isspace(c) && (c) != '\n' && (c) != '\r')

/* is-newline - matches \n, \r, and \u2028 */
static int u_isnl(const char *p, int32 len)
{
    return (*p == '\n' 
            || *p == '\r'
            || (len >= 3
                && *(unsigned char *)p == 0xe2
                && *(unsigned char *)(p+1) == 0x80
                && *(unsigned char *)(p+2) == 0xa8));
}

/* skip to the next utf-8 character */
static void nextc(const char **p, int32 *len)
{
    /* skip the first byte */
    if (*len != 0)
        ++*p, --*len;

    /* skip continuation bytes */
    while (*len != 0 && (**p & 0xC0) == 0x80)
        ++*p, --*len;
}

/* skip to the previous utf-8 character */
static void prevc(const char **p, int32 *len)
{
    /* move back one byte */
    --*p, ++*len;

    /* keep skipping as long as we're looking at continuation characters */
    while ((**p & 0xC0) == 0x80)
        --*p, ++*len;
}

/*
 *   Skip a newline sequence.  Skips all common conventions, including \n,
 *   \r, \n\r, \r\n, and \u2028.  
 */
static void skip_newline(const char **p, int32 *rem)
{
    /* make sure we have something to skip */
    if (*rem == 0)
        return;

    /* check what we have */
    switch (**(const unsigned char **)p)
    {
    case '\n':
        /* skip \n or \n\r */
        nextc(p, rem);
        if (**p == '\r')
            nextc(p, rem);
        break;

    case '\r':
        /* skip \r or \r\n */
        nextc(p, rem);
        if (**p == '\n')
            nextc(p, rem);
        break;

    case 0xe2:
        /* \u2028 (unicode line separator) - just skip the one character */
        nextc(p, rem);
        break;
    }
}

/*
 *   Skip to the next line 
 */
static void skip_to_next_line(const char **p, int32 *rem)
{
    /* look for the next newline */
    for ( ; *rem != 0 ; nextc(p, rem))
    {
        /* if this is a newline of some kind, we're at the end of the line */
        if (u_isnl(*p, *rem))
        {
            /* skip the newline, and we're done */
            skip_newline(p, rem);
            break;
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   ifiction synthesizer output context 
 */
typedef struct synthctx synthctx;
struct synthctx
{
    /* the current output pointer */
    char *buf;

    /* the number of bytes remaining in the output buffer */
    int32 buf_size;

    /* 
     *   the total number of bytes needed for the output (this might be more
     *   than we've actually written, since we count up the bytes required
     *   even if we need more space than the buffer provides) 
     */
    int32 total_size;

    /* the head of the name/value pair list from the parsed GameInfo */
    valinfo *vals;
};

/* initialize a synthesizer context */
static void init_synthctx(synthctx *ctx, char *buf, int32 bufsize,
                          valinfo *vals)
{
    /* set up at the beginning of the output buffer */
    ctx->buf = buf;
    ctx->buf_size = bufsize;

    /* we haven't written anything to the output buffer yet */
    ctx->total_size = 0;

    /* remember the name/value pair list */
    ctx->vals = vals;
}

/* 
 *   Write out a chunk to a synthesized ifiction record, updating pointers
 *   and counters.  We won't copy past the end of the buffer, but we'll
 *   continue counting the output length needed in any case.  
 */
static void write_ifiction(synthctx *ctx, const char *src, size_t srclen)
{
    int32 copy_len;

    /* copy as much as we can, up to the remaining buffer size */
    copy_len = srclen;
    if (copy_len > ctx->buf_size)
        copy_len = ctx->buf_size;

    /* do the copying, if any */
    if (copy_len != 0)
    {
        /* copy the bytes */
        memcpy(ctx->buf, src, (size_t)copy_len);

        /* adjust the buffer pointer and output buffer size remaining */
        ctx->buf += copy_len;
        ctx->buf_size -= copy_len;
    }

    /* count this source data in the total size */
    ctx->total_size += srclen;
}

/* write a null-terminated chunk to the synthesized ifiction record */
static void write_ifiction_z(synthctx *ctx, const char *src)
{
    write_ifiction(ctx, src, strlen(src));
}

/*
 *   Write a PCDATA string to the synthesized ifiction record.  In
 *   particular, we rewrite '<', '>', and '&' as '&lt;', '&gt;', and '&amp;',
 *   respectively; we trim off leading and trailing spaces; and we compress
 *   each run of whitespace down to a single \u0020 (' ') character.
 *   
 *   If 'desc' is TRUE, it means that we're writing Description contents, in
 *   which case we convert \n to <br/>.  
 */
static void write_ifiction_pcdata(synthctx *ctx, const char *p, size_t len,
                                  int desc)
{
    /* first, skip any leading whitespace */
    for ( ; len != 0 && u_ishspace(*p) ; ++p, --len) ;

    /* keep going until we run out of string */
    for (;;)
    {
        const char *start;
        
        /* scan to the next whitespace or markup-significant character */
        for (start = p ;
             len != 0 && !u_ishspace(*p)
                 && *p != '<' && *p != '>' && *p != '&' && *p != '\\' ;
             ++p, --len) ;

        /* write the part up to here */
        if (p != start)
            write_ifiction(ctx, start, p - start);

        /* if we've reached the end of the string, we can stop */
        if (len == 0)
            break;

        /* check what stopped us */
        switch (*p)
        {
        case '<':
            write_ifiction_z(ctx, "&lt;");
            ++p, --len;
            break;

        case '>':
            write_ifiction_z(ctx, "&gt;");
            ++p, --len;
            break;

        case '&':
            write_ifiction_z(ctx, "&amp;");
            ++p, --len;
            break;

        case '\\':
            /* skip the '\\' */
            ++p;
            --len;

            /* in Description fields, \n becomes <br/> */
            if (len > 0 && *p == 'n')
            {
                /* write the translation */
                write_ifiction_z(ctx, "<br/>");

                /* skip the 'n' */
                ++p;
                --len;
            }
            else if (len > 1 && *(p+1) == '\\')
            {
                /* '\\' is just a single '\' */
                write_ifiction_z(ctx, "\\");

                /* skip the second '\' */
                ++p;
                --len;
            }
            else
            {
                /* no 'n' after the '\', so it's just a backslash */
                write_ifiction_z(ctx, "\\");
            }
            break;

        default:
            /* 
             *   The only other thing that could have stopped us is
             *   whitespace.  Skip all consecutive whitespace. 
             */
            for ( ; len != 0 && u_ishspace(*p) ; ++p, --len);

            /* 
             *   if that's not the end of the string, replace the run of
             *   whitespace with a single space character in the output; if
             *   we've reached the end of the string, we don't even want to
             *   do that, since we want to trim off trailing spaces 
             */
            if (len != 0)
                write_ifiction_z(ctx, " ");
            break;
        }
    }
}

/*
 *   Translate a GameInfo keyed value to the corresponding ifiction tagged
 *   value.  We find the GameInfo value keyed by 'gameinfo_key', and write
 *   out the same string under the ifiction XML tag 'ifiction_tag'.  We write
 *   a complete XML container sequence - <tag>value</tag>.
 *   
 *   If the given GameInfo key doesn't exist, we use the default value string
 *   'dflt', if given.  If the GameInfo key doesn't exist and 'dflt' is null,
 *   we don't write anything - we don't even write the open/close tags.
 *   
 *   If 'html' is true, we assume the value is in html format, and we write
 *   it untranslated.  Otherwise, we write it as PCDATA, translating markup
 *   characters into '&' entities and compressing whitespace.  
 */
static void write_ifiction_xlat_base(synthctx *ctx, int indent,
                                     const char *gameinfo_key,
                                     const char *ifiction_tag,
                                     const char *dflt, int html, int desc)
{
    valinfo *val;
    const char *valstr;
    size_t vallen;
    
    /* look up the GameInfo key */
    if ((val = find_by_key(ctx->vals, gameinfo_key)) != 0)
    {
        /* we found the GameInfo value - use it */
        valstr = val->val;
        vallen = val->val_len;
    }
    else if (dflt != 0)
    {
        /* the GameInfo value doesn't exist, but we have a default - use it */
        valstr = dflt;
        vallen = strlen(dflt);
    }
    else
    {
        /* there's no GameInfo value and no default, so write nothing */
        return;
    }

    /* write the indentation */
    while (indent != 0)
    {
        static const char spaces[] = "          ";
        size_t cur;

        /* figure how much we can write on this round */
        cur = indent;
        if (cur > sizeof(spaces) - 1)
            cur = sizeof(spaces) - 1;

        /* write it */
        write_ifiction(ctx, spaces, cur);

        /* deduct it from the amount remaining */
        indent -= cur;
    }

    /* write the open tag */
    write_ifiction_z(ctx, "<");
    write_ifiction_z(ctx, ifiction_tag);
    write_ifiction_z(ctx, ">");

    /* write the value, applying pcdata translations */
    if (html)
        write_ifiction(ctx, valstr, vallen);
    else
        write_ifiction_pcdata(ctx, valstr, vallen, desc);

    /* write the close tag */
    write_ifiction_z(ctx, "</");
    write_ifiction_z(ctx, ifiction_tag);
    write_ifiction_z(ctx, ">\n");
}

#define write_ifiction_xlat(ctx, indent, gikey, iftag, dflt) \
    write_ifiction_xlat_base(ctx, indent, gikey, iftag, dflt, FALSE, FALSE)

#define write_ifiction_xlat_html(ctx, indent, gikey, iftag, dflt) \
    write_ifiction_xlat_base(ctx, indent, gikey, iftag, dflt, TRUE, FALSE)

#define write_ifiction_xlat_desc(ctx, indent, gikey, iftag, dflt) \
    write_ifiction_xlat_base(ctx, indent, gikey, iftag, dflt, FALSE, TRUE)


/*
 *   Retrieve the next author name from the GameInfo "Author" format.  The
 *   format is as follows:
 *   
 *   name <email> <email>... ; ...
 *   
 *   That is, each author is listed with a name followed by one or more email
 *   addresses in angle brackets, and multiple authors are separated by
 *   semicolons.  
 */
static int scan_author_name(const char **p, size_t *len,
                            const char **start, const char **end)
{
    /* keep going until we find a non-empty author name */
    for (;;)
    {
        /* skip leading spaces */
        for ( ; *len != 0 && u_ishspace(**p) ; ++*p, --*len) ;

        /* if we ran out of string, there's definitely no author name */
        if (*len == 0)
            return FALSE;

        /* 
         *   Find the end of this author name.  The author name ends at the
         *   next semicolon or angle bracket.  
         */
        for (*start = *p ; *len != 0 && **p != ';' && **p != '<' ;
             ++*p, --*len) ;

        /* trim off any trailing spaces */
        for (*end = *p ; *end > *start && u_ishspace(*(*end - 1)) ; --*end) ;

        /* now skip any email addresses */
        while (*len != 0 && **p == '<')
        {
            /* skip to the closing bracket */
            for (++*p, --*len ; *len != 0 && **p != '>' ; ++*p, --*len) ;

            /* skip the bracket */
            if (*len != 0)
                ++*p, --*len;

            /* skip whitespace */
            for ( ; *len != 0 && u_ishspace(**p) ; ++*p, --*len) ;

            /* 
             *   if we're not at a semicolon, another angle bracket, or the
             *   end of the string, it's a syntax error 
             */
            if (*len != 0 && **p != '<' && **p != ';')
            {
                *len = 0;
                return FALSE;
            }
        }

        /* if we're at a semicolon, skip it */
        if (*len != 0 && **p == ';')
            ++*p, --*len;

        /* 
         *   if we found a non-empty name, return it; otherwise, continue on
         *   to the next semicolon section 
         */
        if (*end != *start)
            return TRUE;
    }
}


/*
 *   Synthesize an ifiction record for the given GameInfo name/value pair
 *   list.  Returns the number of bytes required for the result, including
 *   null termination.  We'll copy as much as we can to the output buffer, up
 *   to bufsize; if the buffer size is insufficient to hold the result, we'll
 *   still indicate the length needed for the full result, but we're careful
 *   not to actually copy anything past the end of the buffer.  
 */
static int32 synth_ifiction(valinfo *vals, int tads_version,
                            char *buf, int32 bufsize,
                            void *story_file, int32 extent)
{
    char default_ifid[TREATY_MINIMUM_EXTENT];
    valinfo *ifid = find_by_key(vals, "IFID");
    const char *ifid_val;
    size_t ifid_len;
    valinfo *author = find_by_key(vals, "AuthorEmail");
    valinfo *url = find_by_key(vals, "Url");
    synthctx ctx;
    const char *p;
    size_t rem;
    int32 art_fmt;
    int32 art_wid, art_ht;
    const char *format_id = 0;

    /* initialize the output content */
    init_synthctx(&ctx, buf, bufsize, vals);

    /* make sure the tads version is one we know how to handle */
    if (tads_version == 2)
        format_id = "tads2";
    else if (tads_version == 3)
        format_id = "tads3";
    else
        return NO_REPLY_RV;

    /* 
     *   The IFID is mandatory.  If there's not an IFID specifically listed
     *   in the GameInfo, we need to generate the default IFID based on the
     *   MD5 hash of the game file. 
     */
    if (ifid != 0)
    {
        /* use the explicit IFID(s) listed in the GameInfo */
        ifid_val = ifid->val;
        ifid_len = ifid->val_len;
    }
    else if (story_file != 0)
    {
        /* generate the default IFID */
        generate_md5_ifid(story_file, extent,
                          default_ifid, TREATY_MINIMUM_EXTENT);

        /* use this as the IFID */
        ifid_val = default_ifid;
        ifid_len = strlen(default_ifid);
    }

    /* write the header, and start the <identification> section */
    write_ifiction_z(
        &ctx,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<ifindex version=\"1.0\" "
        "xmlns=\"http://babel.ifarchive.org/protocol/iFiction/\">\n"
        "  <!-- Bibliographic data translated from TADS GameInfo -->\n"
        "  <story>\n"
        "    <colophon>\n"
        "     <generator>Babel</generator>\n"
        "     <generatorversion>" TREATY_VERSION "</generatorversion>\n"
        "      <originated>2006-04-14</originated>\n"
        "     </colophon>\n"
        "    <identification>\n");

    /* write each IFID (there might be several) */
    for (p = ifid_val, rem = ifid_len ; rem != 0 ; )
    {
        const char *start;
        const char *end;

        /* skip leading spaces */
        for ( ; rem != 0 && u_ishspace(*p) ; ++p, --rem) ;
        
        /* find the end of this IFID */
        for (start = p ; rem != 0 && *p != ',' ; ++p, --rem) ;

        /* remove trailing spaces */
        for (end = p ; end > start && u_ishspace(*(end-1)) ; --end) ;

        /* if we found one, write it out */
        if (end != start)
        {
            write_ifiction_z(&ctx, "      <ifid>");
            write_ifiction(&ctx, start, end - start);
            write_ifiction_z(&ctx, "</ifid>\n");
        }

        /* skip the comma */
        if (rem != 0 && *p == ',')
            ++p, --rem;
    }

    /* add the format information */
    write_ifiction_z(&ctx, "      <format>");
    write_ifiction_z(&ctx, format_id);
    write_ifiction_z(&ctx, "</format>\n");

    /* close the <identification> section and start the <bibliographic> */
    write_ifiction_z(&ctx,
                     "    </identification>\n"
                     "    <bibliographic>\n");

    /* write the various bibliographic data */
    write_ifiction_xlat(&ctx, 6, "Name", "title", "An Interactive Fiction");
    write_ifiction_xlat(&ctx, 6, "Headline", "headline", 0);
    write_ifiction_xlat_desc(&ctx, 6, "Desc", "description", 0);
    write_ifiction_xlat(&ctx, 6, "Genre", "genre", 0);
    write_ifiction_xlat(&ctx, 6, "Forgiveness", "forgiveness", 0);
    write_ifiction_xlat(&ctx, 6, "Series", "series", 0);
    write_ifiction_xlat(&ctx, 6, "SeriesNumber", "seriesnumber", 0);
    write_ifiction_xlat(&ctx, 6, "Language", "language", 0);
    write_ifiction_xlat(&ctx, 6, "FirstPublished", "firstpublished", 0);

    /* if there's an author, write the list of author names */
    if (author != 0)
    {
        int cnt;
        int i;
        const char *start;
        const char *end;

        /* start the <author> tag */
        write_ifiction_z(&ctx, "      <author>");
        
        /* 
         *   first, count up the number of authors - authors are separated by
         *   semicolons, so there's one more author than there are semicolons
         */
        for (p = author->val, rem = author->val_len, cnt = 1 ;
             scan_author_name(&p, &rem, &start, &end) ; ) ;

        /* 
         *   Now generate the list of authors.  If there are multiple
         *   authors, use commas to separate them. 
         */
        for (p = author->val, rem = author->val_len, i = 0 ; ; ++i)
        {
            /* scan this author's name */
            if (!scan_author_name(&p, &rem, &start, &end))
                break;
            
            /* write out this author name */
            write_ifiction_pcdata(&ctx, start, end - start, FALSE);

            /* if there's another name to come, write a separator */
            if (i + 1 < cnt)
            {
                /* 
                 *   write just "and" to separate two items; write ","
                 *   between items in lists of more than two, with ",and"
                 *   between the last two items 
                 */
                write_ifiction_z(&ctx,
                                 cnt == 2 ? " and " :
                                 i + 2 < cnt ? ", " : ", and ");
            }
        }

        /* end the <author> tag */
        write_ifiction_z(&ctx, "</author>\n");
    }
    else
        write_ifiction_z(&ctx, "      <author>Anonymous</author>\n");

    /* end the biblio section */
    write_ifiction_z(&ctx, "    </bibliographic>\n");

    /* if there's cover art, add its information */
    if (story_file != 0
        && find_cover_art(story_file, extent, 0, &art_fmt, &art_wid, &art_ht)
        && (art_fmt == PNG_COVER_FORMAT || art_fmt == JPEG_COVER_FORMAT))
    {
        char buf[200];
        
        sprintf(buf,
                "    <cover>\n"
                "        <format>%s</format>\n"
                "        <height>%lu</height>\n"
                "        <width>%lu</width>\n"
                "    </cover>\n",
                art_fmt == PNG_COVER_FORMAT ? "png" : "jpg",
                (long)art_ht, (long)art_wid);

        write_ifiction_z(&ctx, buf);
    }

    /* if there's an author email, include it */
    if (author != 0 || url != 0)
    {
        const char *p;
        size_t rem;
        int i;
        
        /* open the section */
        write_ifiction_z(&ctx, "    <contacts>\n");

        /* add the author email, if provided */
        if (author != 0)
        {
            /* write the email list */
            for (i = 0, p = author->val, rem = author->val_len ; ; ++i)
            {
                const char *start;
                
                /* skip to the next email address */
                for ( ; rem != 0 && *p != '<' ; ++p, --rem) ;
                
                /* if we didn't find an email address, we're done */
                if (rem == 0)
                    break;
                
                /* find the matching '>' */
                for (++p, --rem, start = p ; rem != 0 && *p != '>' ;
                     ++p, --rem) ;

                /* 
                 *   if this is the first one, open the section; otherwise,
                 *   add a comma 
                 */
                if (i == 0)
                    write_ifiction_z(&ctx, "      <authoremail>");
                else
                    write_ifiction_z(&ctx, ",");
                
                /* write this address */
                write_ifiction(&ctx, start, p - start);
                
                /* 
                 *   skip the closing bracket, if there is one; if we're out
                 *   of string, we're done 
                 */
                if (rem != 0)
                    ++p, --rem;
                else
                    break;
            }

            /* if we found any emails to write, end the section */
            if (i != 0)
                write_ifiction_z(&ctx, "</authoremail>\n");
        }

        /* if there's a URL, add it */
        if (url != 0)
        {
            write_ifiction_z(&ctx, "      <url>");
            write_ifiction(&ctx, url->val, url->val_len);
            write_ifiction_z(&ctx, "</url>\n");
        }

        /* close the section */
        write_ifiction_z(&ctx, "    </contacts>\n");
    }

    /* add the tads-specific section */
    write_ifiction_z(&ctx, "    <");
    write_ifiction_z(&ctx, format_id);
    write_ifiction_z(&ctx, ">\n");
    
    write_ifiction_xlat(&ctx, 6, "Version", "version", 0);
    write_ifiction_xlat(&ctx, 6, "ReleaseDate", "releasedate", 0);
    write_ifiction_xlat(&ctx, 6, "PresentationProfile",
                        "presentationprofile", 0);
    write_ifiction_xlat(&ctx, 6, "Byline", "byline", 0);

    write_ifiction_z(&ctx, "    </");
    write_ifiction_z(&ctx, format_id);
    write_ifiction_z(&ctx, ">\n");

    /* close the story section and the main body */
    write_ifiction_z(&ctx, "  </story>\n</ifindex>\n");
    
    /* add the null terminator */
    write_ifiction(&ctx, "", 1);

    /* return the total output size */
    return ctx.total_size;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check a data block to see if it starts with the given signature. 
 */
int tads_match_sig(const void *buf, int32 len, const char *sig)
{
    /* note the length of the signature string */
    size_t sig_len = strlen(sig);
    
    /* if matches if the buffer starts with the signature string */
    return (len >= (int32)sig_len && memcmp(buf, sig, sig_len) == 0);
}


/* ------------------------------------------------------------------------ */
/*
 *   portable-to-native format conversions 
 */
#define osbyte(p, ofs) \
    (*(((unsigned char *)(p)) + (ofs)))

#define osrp1(p) \
    ((unsigned int)osbyte(p, 0))

#define osrp2(p) \
    ((unsigned int)osbyte(p, 0) \
    + ((unsigned int)osbyte(p, 1) << 8))

#define osrp4(p) \
    (((unsigned long)osbyte(p, 0)) \
    + (((unsigned long)osbyte(p, 1)) << 8) \
    + (((unsigned long)osbyte(p, 2)) << 16) \
    + (((unsigned long)osbyte(p, 3)) << 24))


/* ------------------------------------------------------------------------ */
/*
 *   Parse a story file and retrieve the GameInfo data.  Returns the head of
 *   a linked list of valinfo entries.  
 */
static valinfo *parse_sf_game_info(const void *story_file, int32 story_len,
                                   int *tads_version)
{
    resinfo res;

    /* 
     *   first, find the GameInfo resource - if it's not there, there's no
     *   game information to parse 
     */
    if (!find_resource(story_file, story_len, "GameInfo.txt", &res))
        return 0;

    /* if the caller wants the TADS version number, hand it back */
    if (tads_version != 0)
        *tads_version = res.tads_version;

    /* parse the GameInfo record */
    return parse_game_info(res.ptr, res.len);
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a GameInfo text record 
 */
static valinfo *parse_game_info(const char *ptr, int32 len)
{
    const char *p;
    int32 rem;
    valinfo *val_head = 0;

    /* parse the data */
    for (p = ptr, rem = len ; rem != 0 ; )
    {
        const char *name_start;
        size_t name_len;
        const char *val_start;
        valinfo *val;
        const char *inp;
        int32 inlen;
        char *outp;

        /* skip any leading whitespace */
        while (rem != 0 && u_isspace(*p))
            ++p, --rem;

        /* if the line starts with '#', it's a comment, so skip it */
        if (rem != 0 && *p == '#')
        {
            skip_to_next_line(&p, &rem);
            continue;
        }

        /* we must have the start of a name - note it */
        name_start = p;

        /* skip ahead to a space or colon */
        while (rem != 0 && *p != ':' && !u_ishspace(*p))
            nextc(&p, &rem);

        /* note the length of the name */
        name_len = p - name_start;

        /* skip any whitespace before the presumed colon */
        while (rem != 0 && u_ishspace(*p))
            nextc(&p, &rem);

        /* if we're not at a colon, the line is ill-formed, so skip it */
        if (rem == 0 || *p != ':')
        {
            /* skip the entire line, and go back for the next one */
            skip_to_next_line(&p, &rem);
            continue;
        }

        /* skip the colon and any whitespace immediately after it */
        for (nextc(&p, &rem) ; rem != 0 && u_ishspace(*p) ; nextc(&p, &rem)) ;

        /* note where the value starts */
        val_start = p;

        /*
         *   Scan the value to get its length.  The value runs from here to
         *   the next newline that's not followed immediately by a space. 
         */
        while (rem != 0)
        {
            const char *nl;
            int32 nlrem;
            
            /* skip to the next line */
            skip_to_next_line(&p, &rem);

            /* if we're at eof, we can stop now */
            if (rem == 0)
                break;

            /* note where this line starts */
            nl = p;
            nlrem = rem;

            /* 
             *   if we're at a non-whitespace character, it's definitely not
             *   a continuation line 
             */
            if (!u_ishspace(*p))
                break;

            /* 
             *   check for spaces followed by a non-space character - this
             *   would signify a continuation line
             */
            for ( ; rem != 0 && u_ishspace(*p) ; nextc(&p, &rem)) ;
            if (rem == 0 || u_isnl(p, rem))
            {
                /* 
                 *   we're at end of file, we found a line with nothing but
                 *   whitespace, so this isn't a continuation line; go back
                 *   to the start of this line and end the value here 
                 */
                p = nl;
                rem = nlrem;
                break;
            }

            /* 
             *   We found whitespace followed by non-whitespace, so this is a
             *   continuation line.  Keep going for now.
             */
        }

        /* remove any trailing newlines */
        while (p > val_start)
        {
            /* move back one character */
            prevc(&p, &rem);

            /* 
             *   if it's a newline, keep going; otherwise, keep this
             *   character and stop trimming 
             */
            if (!u_isnl(p, rem))
            {
                nextc(&p, &rem);
                break;
            }
        }

        /* 
         *   Allocate a new value entry.  Make room for the entry itself plus
         *   a copy of the value.  We don't need to make a copy of the name,
         *   since we can just use the original copy from the story file
         *   buffer.  We do need a copy of the value because we might need to
         *   transform it slightly, to remove newlines and leading spaces on
         *   continuation lines. 
         */
        val = (valinfo *)malloc(sizeof(valinfo) + (p - val_start));

        /* link it into our list */
        val->nxt = val_head;
        val_head = val;

        /* point the name directly to the name in the buffer */
        val->name = name_start;
        val->name_len = name_len;

        /* point the value to the space allocated along with the valinfo */
        val->val = (char *)(val + 1);

        /* store the name, removing newlines and continuation-line spaces */
        for (outp = val->val, inp = val_start, inlen = p - val_start ;
             inlen != 0 ; )
        {
            const char *l;
            
            /* find the next newline */
            for (l = inp ; inlen != 0 && !u_isnl(inp, inlen) ;
                 nextc(&inp, &inlen)) ;

            /* copy this line to the output */
            memcpy(outp, l, inp - l);
            outp += inp - l;

            /* if we're out of input, we're done */
            if (inlen == 0)
                break;

            /* we're at a newline: replace it with a space in the output */
            *outp++ = ' ';

            /* skip the newline and subsequent whitespace in the input */
            for (skip_newline(&inp, &inlen) ;
                 inlen != 0 && u_ishspace(*inp) ; nextc(&inp, &inlen)) ;
        }

        /* set the length of the parsed value */
        val->val_len = outp - val->val;

        /* skip to the next line and continue parsing */
        skip_to_next_line(&p, &rem);
    }

    /* return the head of the linked list of value entries */
    return val_head;
}

/* ------------------------------------------------------------------------ */
/*
 *   Portable memicmp implementation 
 */
static int tmemicmp(const char *a, const char *b, size_t len)
{
    /* scan each character */
    for ( ; len != 0 ; ++a, ++b, --len)
    {
        /* get the lower-case version of each current character */
        char ca = isupper(*a) ? tolower(*a) : *a;
        char cb = isupper(*b) ? tolower(*b) : *b;

        /* check for mismatch */
        if (ca < cb)
            return -1;
        else if (ca > cb)
            return 1;
    }

    /* all bytes match */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Given a valinfo list obtained from parse_game_info(), find the value for
 *   the given key 
 */
static valinfo *find_by_key(valinfo *list_head, const char *key)
{
    valinfo *p;
    size_t key_len = strlen(key);
    
    /* scan the list for the given key */
    for (p = list_head ; p != 0 ; p = p->nxt)
    {
        /* if this one matches the key we're looking for, return it */
        if (p->name_len == key_len && tmemicmp(p->name, key, key_len) == 0)
            return p;
    }

    /* no luck */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Delete a valinfo list obtained from parse_game_info() 
 */
static void delete_valinfo_list(valinfo *head)
{
    /* keep going until we run out of entries */
    while (head != 0)
    {
        /* remember the next entry, before we delete this one */
        valinfo *nxt = head->nxt;

        /* delete this one */
        free(head);

        /* move on to the next one */
        head = nxt;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Find the cover art resource.  We'll look for .system/CoverArt.jpg and
 *   .system/CoverArt.png, in that order. 
 */
static int find_cover_art(const void *story_file, int32 story_len,
                          resinfo *resp, int32 *image_format,
                          int32 *width, int32 *height)
{
    resinfo res;
    int32 x, y;

    /* if they didn't want the resource info, provide a placeholder */
    if (resp == 0)
        resp = &res;

    /* look for .system/CoverArt.jpg first */
    if (find_resource(story_file, story_len, ".system/CoverArt.jpg", resp))
    {
        /* get the width and height */
        if (!get_jpeg_dim(resp->ptr, resp->len, &x, &y))
            return FALSE;

        /* hand back the width and height if it was requested */
        if (width != 0)
            *width = x;
        if (height != 0)
            *height = y;

        /* tell them it's a JPEG image */
        if (image_format != 0)
            *image_format = JPEG_COVER_FORMAT;

        /* indicate success */
        return TRUE;
    }

    /* look for .system/CoverArt.png second */
    if (find_resource(story_file, story_len, ".system/CoverArt.png", resp))
    {
        /* get the width and height */
        if (!get_png_dim(resp->ptr, resp->len, &x, &y))
            return FALSE;

        /* hand back the width and height if it was requested */
        if (width != 0)
            *width = x;
        if (height != 0)
            *height = y;

        /* tell them it's a PNG image */
        if (image_format != 0)
            *image_format = PNG_COVER_FORMAT;

        /* indicate success */
        return TRUE;
    }

    /* didn't find it */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Find a resource in a TADS 2 or 3 story file that's been loaded into
 *   memory.  On success, fills in the offset and size of the resource and
 *   returns TRUE; if the resource isn't found, returns FALSE.
 */
static int find_resource(const void *story_file, int32 story_len,
                         const char *resname, resinfo *info)
{
    /* if there's no file, there's no resource */
    if (story_file == 0)
        return FALSE;

    /* check for tads 2 */
    if (tads_match_sig(story_file, story_len, T2_SIGNATURE))
    {
        info->tads_version = 2;
        return t2_find_res(story_file, story_len, resname, info);
    }

    /* check for tads 3 */
    if (tads_match_sig(story_file, story_len, T3_SIGNATURE))
    {
        info->tads_version = 3;
        return t3_find_res(story_file, story_len, resname, info);
    }

    /* it's not one of ours */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Find a resource in a tads 2 game file 
 */
static int t2_find_res(const void *story_file, int32 story_len,
                       const char *resname, resinfo *info)
{
    const char *basep = (const char *)story_file;
    const char *endp = basep + story_len;
    const char *p;
    size_t resname_len;

    /* note the length of the name we're seeking */
    resname_len = strlen(resname);

    /* 
     *   skip past the tads 2 file header (13 bytes for the signature, 7
     *   bytes for the version header, 2 bytes for the flags, 26 bytes for
     *   the timestamp) 
     */
    p = basep + 13 + 7 + 2 + 26;

    /* 
     *   scan the sections in the file; stop on $EOF, and skip everything
     *   else but HTMLRES, which is the section type that 
     */
    while (p < endp)
    {
        unsigned long endofs;

        /*
         *   We're pointing to a section block header, which looks like this:
         *   
         *.    <byte> type-length
         *.    <byte * type-length> type-name
         *.    <uint32> next-section-address
         */

        /* read the ending offset */
        endofs = osrp4(p + 1 + osrp1(p));

        /* check the type */
        if (p[0] == 7 && memcmp(p + 1, "HTMLRES", 7) == 0)
        {
            unsigned long found_ofs;
            int found;
            unsigned long entry_cnt;

            /* we haven't found the resource yet */
            found = FALSE;

            /* 
             *   It's a multimedia resource block.  Skip the section block
             *   header and look at the index table - the index table
             *   consists of a uint32 giving the number of entries, followed
             *   by a reserved uint32, followed by the entries.  
             */
            p += 12;
            entry_cnt = osrp4(p);

            /* skip to the first index entry */
            p += 8;

            /* scan the index entries */
            for ( ; entry_cnt != 0 ; --entry_cnt)
            {
                unsigned long res_ofs;
                unsigned long res_siz;
                size_t name_len;

                /*
                 *   We're at the next index entry, which looks like this:
                 *
                 *.    <uint32>  resource-address (bytes from end of index)
                 *.    <uint32>  resource-length (in bytes)
                 *.    <uint2> name-length
                 *.    <byte * name-length> name
                 */
                res_ofs = osrp4(p);
                res_siz = osrp4(p + 4);
                name_len = osrp2(p + 8);
                p += 10;

                /* check for a match to the name we're looking for */
                if (name_len == resname_len
                    && tmemicmp(resname, p, name_len) == 0)
                {
                    /* 
                     *   it's the one we want - note its resource location
                     *   and size, but keep scanning for now, since we need
                     *   to find the end of the index before we'll know where
                     *   the actual resources begin 
                     */
                    found = TRUE;
                    found_ofs = res_ofs;
                    info->len = res_siz;
                }

                /* skip this one's name */
                p += name_len;
            }

            /* 
             *   if we found our resource, the current seek position is the
             *   base of the offset we found in the directory; so we can
             *   finally fix up the offset to give the actual file location
             *   and return the result 
             */
            if (found)
            {
                /* fix up the offset with the actual file location */
                info->ptr = p + found_ofs;

                /* tell the caller we found it */
                return TRUE;
            }
        }
        else if (p[0] == 4 && memcmp(p + 1, "$EOF", 4) == 0)
        {
            /* 
             *   that's the end of the file - we've finished without finding
             *   the resource, so return failure 
             */
            return FALSE;
        }

        /* move to the next section */
        p = basep + endofs;
    }

    /* 
     *   reached EOF without an $EOF marker - file must be corrupted; return
     *   'not found' 
     */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Find a resource in a T3 image file 
 */
static int t3_find_res(const void *story_file, int32 story_len,
                       const char *resname, resinfo *info)
{
    const char *basep = (const char *)story_file;
    const char *endp = basep + story_len;
    const char *p;
    size_t resname_len;

    /* note the length of the name we're seeking */
    resname_len = strlen(resname);

    /* 
     *   skip the file header - 11 bytes for the signature, 2 bytes for the
     *   format version, 32 reserved bytes, and 24 bytes for the timestamp 
     */
    p = basep + 11 + 2 + 32 + 24;

    /* scan the data blocks */
    while (p < endp)
    {
        unsigned long siz;

        /*
         *   We're at the next block header, which looks like this:
         *
         *.    <byte * 4> type-name
         *.    <uint32> block-size
         *.    <uint16> flags
         */

        /* get the block size */
        siz = osrp4(p + 4);

        /* check the type */
        if (memcmp(p, "MRES", 4) == 0)
        {
            unsigned int entry_cnt;
            unsigned int i;
            const char *blockp;

            /* skip the header */
            p += 10;

            /* 
             *   remember the location of the base of the block - the data
             *   seek location for each index entry is given as an offset
             *   from this location 
             */
            blockp = p;

            /* the first thing in the table is the number of entries */
            entry_cnt = osrp2(p);
            p += 2;

            /* read the entries */
            for (i = 0 ; i < entry_cnt ; ++i)
            {
                unsigned long entry_ofs;
                unsigned long entry_siz;
                size_t entry_name_len;
                char namebuf[256];
                char *xp;
                size_t xi;

                /* 
                 *   Parse this index entry:
                 *   
                 *.    <uint32> address (as offset from the block base)
                 *.    <uint32> size (in bytes)
                 *.    <uint8> name-length
                 *.    <byte * name-length> name (all bytes XORed with 0xFF)
                 */
                entry_ofs = osrp4(p);
                entry_siz = osrp4(p + 4);
                entry_name_len = (unsigned char)p[8];

                /* unmask the name */
                memcpy(namebuf, p + 9, resname_len);
                for (xi = resname_len, xp = namebuf ; xi != 0 ; --xi)
                    *xp++ ^= 0xFF;

                /* if this is the one we're looking for, return it */
                if (entry_name_len == resname_len
                    && tmemicmp(resname, namebuf, resname_len) == 0)
                {
                    /* 
                     *   fill in the return information - note that the entry
                     *   offset given in the header is an offset from data
                     *   block's starting location, so fix this up to an
                     *   absolute seek location for the return value 
                     */
                    info->ptr = blockp + entry_ofs;
                    info->len = entry_siz;

                    /* return success */
                    return TRUE;
                }

                /* skip this entry (header + name length) */
                p += 9 + entry_name_len;
            }

            /* 
             *   if we got this far, we didn't find the name; so skip past
             *   the MRES section by adding the section length to the base
             *   pointer, and resume the main file scan 
             */
            p = blockp + siz;
        }
        else if (memcmp(p, "EOF ", 4) == 0)
        {
            /* 
             *   end of file - we've finished without finding the resource,
             *   so return failure 
             */
            return FALSE;
        }
        else
        {
            /* 
             *   we don't care about anything else - just skip this block and
             *   keep going; to skip the block, simply seek ahead past the
             *   block header and then past the block's contents, using the
             *   size given the in block header 
             */
            p += siz + 10;
        }
    }

    /* 
     *   reached EOF without an EOF marker - file must be corrupted; return
     *   'not found' 
     */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   JPEG and PNG information extraction (based on the versions in
 *   babel_story_functions.c) 
 */
static int get_jpeg_dim(const void *img, int32 extent,
                        int32 *xout, int32 *yout)
{
    const unsigned char *dp=(const unsigned char *) img;
    const unsigned char *ep=dp+extent;
    unsigned int t1, t2, w, h;

    t1 = *dp++;
    t2 = *dp++;
    if (t1 != 0xff || t2 != 0xD8)
        return FALSE;

    while(1)
    {
        if (dp>ep) return FALSE;
        for(t1=*(dp++);t1!=0xff;t1=*(dp++)) if (dp>ep) return FALSE;
        do { t1=*(dp++); if (dp>ep) return FALSE;} while (t1 == 0xff);

        if ((t1 & 0xF0) == 0xC0 && !(t1==0xC4 || t1==0xC8 || t1==0xCC))
        {
            dp+=3;
            if (dp>ep) return FALSE;
            h=*(dp++) << 8;
            if (dp>ep) return FALSE;
            h|=*(dp++);
            if (dp>ep) return FALSE;
            w=*(dp++) << 8;
            if (dp>ep) return FALSE;
            w|=*(dp);

            *xout = w;
            *yout = h;
            return TRUE;
        }
        else if (t1==0xD8 || t1==0xD9)
            break;
        else
        {
            int l;

            if (dp>ep) return FALSE;
            l=*(dp++) << 8;
            if (dp>ep) return FALSE;
            l|= *(dp++);
            l-=2;
            dp+=l;
            if (dp>ep) return FALSE;
        }
    }
    return FALSE;
}

static int32 png_read_int(const unsigned char *mem)
{
    int32 i4 = mem[0],
    i3 = mem[1],
    i2 = mem[2],
    i1 = mem[3];
    return i1 | (i2<<8) | (i3<<16) | (i4<<24);
}


static int get_png_dim(const void *img, int32 extent,
                       int32 *xout, int32 *yout)
{
    const unsigned char *dp=(const unsigned char *)img;

    if (extent<33 ||
        !(dp[0]==137 && dp[1]==80 && dp[2]==78 && dp[3]==71 &&
          dp[4]==13 && dp[5] == 10 && dp[6] == 26 && dp[7]==10)||
        !(dp[12]=='I' && dp[13]=='H' && dp[14]=='D' && dp[15]=='R'))
        return FALSE;

    *xout = png_read_int(dp+16);
    *yout = png_read_int(dp+20);
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Testing main() - this implements a set of unit tests on the tads
 *   version.  
 */

#ifdef TADS_TEST

#include "babel_handler.h"

void main(int argc, char **argv)
{
    FILE *fp;
    int32 siz;
    void *buf;
    valinfo *head;
    int32 rv;
    int tadsver;
    char outbuf[TREATY_MINIMUM_EXTENT];

    /* check arguments */
    if (argc != 2)
    {
        printf("usage: tads <game-file>\n");
        exit(1);
    }

    /* initialize the babel subsystems */
    babel_init(argv[1]);

    /* open the story file */
    if ((fp = fopen(argv[1], "rb")) == 0)
    {
        printf("error opening input file\n");
        exit(2);
    }

    /* check the file size */
    fseek(fp, 0, SEEK_END);
    siz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* allocate space for it */
    if ((buf = malloc(siz)) == 0)
    {
        printf("error allocating space to load file\n");
        exit(2);
    }

    /* load it */
    if ((int32)fread(buf, 1, siz, fp) != siz)
    {
        printf("error reading file\n");
        exit(2);
    }

    /* done with the file */
    fclose(fp);



    /* ===== test 1 - basic parse_sf_game_info() test ===== */

    /* parse the gameinfo record and print the results */
    if ((head = parse_sf_game_info(buf, siz, &tadsver)) != 0)
    {
        valinfo *val;

        printf("found GameInfo - tads major version = %d\n", tadsver);
        for (val = head ; val != 0 ; val = val->nxt)
        {
            printf("%.*s=[%.*s]\n",
                   (int)val->name_len, val->name,
                   (int)val->val_len, val->val);
        }
        printf("\n");
    }
    else
        printf("no GameInfo found\n\n");



    /* ===== test 2 - test the get_story_file_IFID generator ===== */
    rv = tads_get_story_file_IFID(buf, siz, outbuf, TREATY_MINIMUM_EXTENT);
    if (rv == 1)
        printf("IFID = [%s]\n\n", outbuf);
    else
        printf("IFID return code = %ld\n", rv);



    /* ===== test 3 - test the ifiction synthesizer ===== */
    if ((rv = tads_get_story_file_metadata_extent(buf, siz)) > 0)
    {
        char *ifbuf;

        /* try allocating the space */
        if ((ifbuf = malloc((size_t)rv)) != 0)
        {
            /* synthesize the story file */
            rv = tads_get_story_file_metadata(buf, siz, ifbuf, rv);
            if (rv > 0)
                printf("ifiction metadata:\n=====\n%.*s\n=====\n\n",
                       (int)rv, ifbuf);
            else
                printf("tads_get_story_file_metadata result = %ld\n", rv);
        }
        else
            printf("unable to allocate %ld bytes for metadata record\n", rv);
    }
    else
        printf("tads_get_story_file_metadata_extent result code = %ld\n", rv);
    

    /* free the loaded story file buffer */
    free(buf);
}


#endif //TADS_TEST

