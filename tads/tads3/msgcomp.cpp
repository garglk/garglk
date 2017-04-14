#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  msgcomp.cpp - T3 Message Compiler
Function
  
Notes
  
Modified
  05/14/00 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "vmerr.h"
#include "charmap.h"
#include "resload.h"
#include "vmregex.h"
#include "vmhash.h"
#include "vmimage.h"


/* ------------------------------------------------------------------------ */
/*
 *   hash entry for a #define symbol 
 */
class CVmHashEntryDef: public CVmHashEntryCS
{
public:
    CVmHashEntryDef(const char *str, size_t len, int val)
        : CVmHashEntryCS(str, len, TRUE)
    {
        val_ = val;
    }

    /* the integer value of the #define symbol */
    int val_;
};

/* ------------------------------------------------------------------------ */
/*
 *   read a header file 
 */
static int read_header_file(int line_num, const char *fname,
                            CRegexSearcherSimple *searcher,
                            CVmHashTable *hashtab)
{
    osfildef *fphdr;

    /* open the header file */
    fphdr = osfoprt(fname, OSFTTEXT);
    if (fphdr == 0)
    {
        printf("line %d: unable to open header file \"%s\"\n",
               line_num, fname);
        return 1;
    }

    /* read the header file */
    for (;;)
    {
        static const char def_pat[] = "<space>*#<space>*define<space>+"
                                      "((?:<alpha>|_)(?:<alphanum>|_)*)"
                                      "<space>+"
                                      "(<digit>+)";
        static size_t def_pat_len = sizeof(def_pat) - 1;
        char buf[512];

        /* read the next line */
        if (osfgets(buf, sizeof(buf), fphdr) == 0)
            break;

        /* 
         *   if the line is of the form "#define SYMBOL number", not it;
         *   otherwise, ignore it 
         */
        if (searcher->compile_and_match(def_pat, def_pat_len,
                                        buf, buf, strlen(buf)) > 0)
        {
            const re_group_register *sym_reg;
            const re_group_register *val_reg;
            const char *sym;
            size_t sym_len;
            int val;
            CVmHashEntryDef *entry;

            /* get the group register for the symbol being defined */
            sym_reg = searcher->get_group_reg(0);
            sym = buf + sym_reg->start_ofs;
            sym_len = sym_reg->end_ofs - sym_reg->start_ofs;

            /* get the group register for the value of the symbol */
            val_reg = searcher->get_group_reg(1);
            val = atoi(buf + val_reg->start_ofs);

            /* add the symbol to our hash table */
            entry = new CVmHashEntryDef(sym, sym_len, val);
            hashtab->add(entry);
        }
    }

    /* close the file */
    osfcls(fphdr);

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Message structure 
 */
struct msg_t
{
    msg_t(int id, const char *short_msg, const char *long_msg)
    {
        /* remember the ID */
        id_ = id;

        /* remember the messages */
        short_msg_ = lib_copy_str(short_msg);
        long_msg_ = lib_copy_str(long_msg);

        /* not in a list yet */
        nxt_ = 0;
    }

    ~msg_t()
    {
        /* delete the message text buffers */
        lib_free_str(short_msg_);
        lib_free_str(long_msg_);
    }

    /* 
     *   compare two messages to determine which comes first in message
     *   number order 
     */
    static int compare(const void *a0, const void *b0)
    {
        const msg_t *a = *(const msg_t **)a0;
        const msg_t *b = *(const msg_t **)b0;

        /* compare the message numbers and return the result */
        if (a->id_ > b->id_)
            return 1;
        else if (a->id_ < b->id_)
            return -1;
        else
            return 0;
    }

    /* numeric ID of the message */
    int id_;

    /* short message text */
    char *short_msg_;

    /* long message text */
    char *long_msg_;

    /* next in list */
    msg_t *nxt_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Context structure for strict_check enumeration callback 
 */
struct strict_check_ctx
{
    /* the array of messages, sorted by message ID */
    msg_t **msg_array;

    /* the number of messages in the message array */
    int msg_count;

    /* number of symbols with no messages */
    int not_found_count;
};

/*
 *   Hash table symbol enumeration callback for strict checking.  For each
 *   symbol, we verify that the symbol's message ID has a message defined
 *   in the message array. 
 */
static void strict_check(void *ctx0, CVmHashEntry *entry0)
{
    strict_check_ctx *ctx = (strict_check_ctx *)ctx0;
    CVmHashEntryDef *entry = (CVmHashEntryDef *)entry0;
    int hi, lo, cur;

    /* 
     *   search for this message ID - the array is in sorted order, so we
     *   can perform a binary search to make the search reasonably quick
     */
    lo = 0;
    hi = ctx->msg_count - 1;
    while (lo <= hi)
    {
        /* split the difference */
        cur = lo + (hi - lo)/2;

        /* is it a match? */
        if (entry->val_ == ctx->msg_array[cur]->id_)
        {
            /* 
             *   found it - we don't need to warn about this symbol, so we
             *   can just return and get on with the rest of the
             *   enumeration 
             */
            return;
        }
        else if (entry->val_ > ctx->msg_array[cur]->id_)
        {
            /* we need to go higher */
            lo = (cur == lo ? cur + 1 : cur);
        }
        else
        {
            /* we need to go lower */
            hi = (cur == hi ? hi - 1 : cur);
        }
    }

    /* 
     *   We didn't find it - show this symbol.  If this is the first one,
     *   also show the initial warning message, which prefixes the
     *   not-found symbol list. 
     */
    if (ctx->not_found_count == 0)
        printf("warning: the following #define symbols have no "
               "corresponding messages:\n");

    /* show the symbol */
    printf("   %.*s (%d)\n",
           (int)entry->getlen(), entry->getstr(), entry->val_);

    /* count it */
    ++(ctx->not_found_count);
}


/* ------------------------------------------------------------------------ */
/*
 *   message compiler main entrypoint 
 */
int main(int argc, char **argv)
{
    char *fname_in;
    char *fname_out;
    osfildef *fpin = 0;
    osfildef *fpout = 0;
    CCharmapToUni *mapper = 0;
    CResLoader *res_loader = 0;
    CVmHashTable *hashtab = 0;
    CRegexParser *regex = 0;
    CRegexSearcherSimple *searcher = 0;
    int stat;
    int line_num;
    int error_count;
    msg_t *first_msg = 0;
    msg_t *last_msg = 0;
    int msg_count;
    msg_t **msg_array = 0;
    msg_t *cur_msg;
    msg_t **arrp;
    char writebuf[128];
    int i;
    int strict = FALSE;
    char exe_path[OSFNMAX];

    /* presume failure */
    stat = OSEXFAIL;

    /* show the banner */
    printf("T3 Message Compiler v1.0.0  Copyright 2000, 2012 "
           "Michael J. Roberts\n");                /* copyright-date-string */

    /* check options */
    for (i = 1 ; i < argc && argv[i][0] == '-' ; ++i)
    {
        /* check which option we have */
        if (strcmp(argv[i], "-strict") == 0)
        {
            /* note STRICT mode */
            strict = TRUE;
        }
        else
        {
            /* anything else is invalid */
            printf("invalid option \"%s\"\n", argv[i]);
            return OSEXFAIL;
        }
    }

    /* check arguments */
    if (i + 2 != argc)
    {
        printf("usage: t3msgc [options] infile outfile\n"
               "infile = your message text source file\n"
               "outfile = the compiled binary output file to be created\n"
               "options:\n"
               "   -strict  warn for any #define symbols with no associated "
               "message\n");
        return OSEXFAIL;
    }

    /* create the resource loader and set the exe path if possible */
    res_loader = new CResLoader();
    if (os_get_exe_filename(exe_path, sizeof(exe_path), argv[0]))
        res_loader->set_exe_filename(exe_path);

    /* get the arguments */
    fname_in = argv[i];
    fname_out = argv[i + 1];

    /* create our hash table */
    hashtab = new CVmHashTable(256, new CVmHashFuncCS(), TRUE);

    /* create a regular expression parser */
    regex = new CRegexParser();
    searcher = new CRegexSearcherSimple(regex);

    /* open the input file */
    fpin = osfoprt(fname_in, OSFTTEXT);
    if (fpin == 0)
    {
        printf("unable to open input file \"%s\"\n", fname_in);
        goto done;
    }

    /* 
     *   read the source file 
     */
    for (line_num = 0, error_count = 0, msg_count = 0 ;; )
    {
        char buf[512];
        size_t len;
        char shortbuf[512];
        char longbuf[2048];
        size_t longbuf_len;
        char *p;
        char *idp;
        int msg_id;
        static const char charset_pat[] = "<space>*#<space>*charset<space>+"
                                          "\"([^\"]*)\"";
        static size_t charset_pat_len = sizeof(charset_pat) - 1;
        static const char inc_pat[] = "<space>*#<space>*include<space>+"
                                      "\"([^\"]*)\"";
        static size_t inc_pat_len = sizeof(inc_pat) - 1;
        static const char comment_pat[] = "<space>*(//|$)";
        static size_t comment_pat_len = sizeof(comment_pat) - 1;

        /* count the new line */
        ++line_num;

        /* read the next line - stop if we've reached the end of the file */
        if (osfgets(buf, sizeof(buf), fpin) == 0)
            break;

        /* get the length of the line */
        len = strlen(buf);

        /* remove trailing newline characters */
        while (len != 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
            --len;

        /* add a null terminator replacing the last newline we found */
        buf[len] = '\0';

        /* check for a comment or a blank line */
        if (searcher->compile_and_match(comment_pat, comment_pat_len,
                                        buf, buf, len) >= 0)
        {
            /* 
             *   it's a comment, so no further processing is required -
             *   move on to the next line 
             */
            continue;
        }

        /* check for a #charset directive */
        if (searcher->compile_and_match(charset_pat, charset_pat_len,
                                        buf, buf, len) > 0)
        {
            char charset[128];
            const re_group_register *cs_reg;
            size_t cs_len;
            
            /* if we already have a character mapping, it's an error */
            if (mapper != 0)
            {
                /* note the error */
                printf("line %d: error: redundant #charset directive "
                       "(only one is allowed in the file) - ignored\n",
                       line_num);

                /* count the error */
                ++error_count;

                /* simply ignore the redundant directive and keep going */
                continue;
            }

            /* get the group register for the character set name */
            cs_reg = searcher->get_group_reg(0);

            /* make sure it'll fit in our buffer */
            cs_len = cs_reg->end_ofs - cs_reg->start_ofs;
            if (cs_len > sizeof(charset) - 1)
            {
                /* 
                 *   show an error and give up - failure to load a
                 *   character mapping is fatal, since we can't interpret
                 *   the remainder of the file without it 
                 */
                printf("line %d: character set name \"%.*s\" is too long\n",
                       line_num, (int)cs_len, buf + cs_reg->start_ofs);
                goto done;
            }

            /* 
             *   copy the character set name into our buffer and
             *   null-terminate it 
             */
            memcpy(charset, buf + cs_reg->start_ofs, cs_len);
            charset[cs_len] = '\0';

            /* create the mapper for the character set */
            mapper = CCharmapToUni::load(res_loader, charset);
            if (mapper == 0)
            {
                /* show an error and abort */
                printf("line %d: unable to load character set "
                       "mapping for character set \"%s\"\n",
                       line_num, charset);
                goto done;
            }

            /* we're done processing this line - move on to the next */
            continue;
        }

        /* check for a #include directive */
        if (searcher->compile_and_match(
            inc_pat, inc_pat_len, buf, buf, len) > 0)
        {
            const re_group_register *fname_reg;
            char fname[OSFNMAX];
            size_t fname_len;

            /* get the header file register from the regular expression */
            fname_reg = searcher->get_group_reg(0);

            /* note its length - make sure it's not too long for our buffer */
            fname_len = fname_reg->end_ofs - fname_reg->start_ofs;
            if (fname_len > sizeof(fname) - 1)
            {
                /* it's too long - warn about it */
                printf("line %d: #include filename \"%.*s\" is too long - "
                       "#include directive ignored\n",
                       line_num,
                       (int)fname_len, buf + fname_reg->start_ofs);

                /* count the error */
                ++error_count;

                /* ignore this line */
                continue;
            }

            /* copy the name into our buffer and null-terminate it */
            memcpy(fname, buf + fname_reg->start_ofs, fname_len);
            fname[fname_len] = '\0';
            
            /* read the header file */
            if (read_header_file(line_num, fname, searcher, hashtab))
            {
                /* an error occurred reading the file - abort */
                goto done;
            }
            
            /* we're done with this line - proceed to the next one */
            continue;
        }

        /* 
         *   We seem to have a message definition.  We must have a
         *   character set defined at this point; if we don't, it's an
         *   error.
         */
        if (mapper == 0)
        {
            printf("line %d: a character set must be defined with #charset "
                   "before the first message definition\n", line_num);
            goto done;
        }

        /* skip leading spaces */
        for (p = buf ; is_space(*p) ; ++p) ;

        /* find the end of the message identifier */
        for (idp = p ; *p != '\0' && !is_space(*p) ; ++p) ;

        /* determine if the message ID is numeric or symbolic */
        if (is_alpha(*idp) || *idp == '_')
        {
            CVmHashEntryDef *entry;
            
            /* look up the message ID in the hash table */
            entry = (CVmHashEntryDef *)hashtab->find(idp, p - idp);

            /* if we couldn't find the ID, it's an error */
            if (entry == 0)
            {
                /* note the error */
                printf("line %d: symbolic message identifier \"%.*s\" "
                       "not found\n", line_num, (int)(p - idp), idp);

                /* count the error */
                ++error_count;

                /* we don't know the message ID, so use zero */
                msg_id = 0;
            }
            else
            {
                /* get the message ID from the symbol table entry */
                msg_id = entry->val_;
            }
        }
        else if (is_digit(*idp))
        {
            /* get the numeric message ID */
            msg_id = atoi(idp);
        }
        else
        {
            /* note the error */
            printf("line %d: invalid message identifier \"%.*s\"\n",
                   line_num, (int)(idp - p), idp);

            /* count the error */
            ++error_count;

            /* we don't know the message ID, so use zero */
            msg_id = 0;
        }

        /* skip spaces after the message ID */
        for ( ; is_space(*p) ; ++p) ;

        /* if there's nothing left, the short message is on a separate line */
        if (*p == '\0')
        {
            /* count the next line */
            ++line_num;

            /* read the next line */
            if (osfgets(buf, sizeof(buf), fpin) == 0)
            {
                printf("line %d: unexpected end of file before short message "
                       "for message number %d\n", line_num, msg_id);
                goto done;
            }

            /* get the length of the line */
            len = strlen(buf);

            /* remove trailing newline characters */
            while (len != 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
                --len;

            /* add a null terminator replacing the last newline we found */
            buf[len] = '\0';

            /* use the whole line */
            p = buf;
        }

        /* 
         *   the remainder of the line is the short message - copy it into
         *   the short-message buffer, mapping it to utf-8 
         */
        mapper->map_str(shortbuf, sizeof(shortbuf), p);

        /* 
         *   subsequent lines up to the next blank line are the long
         *   message - read them and collect the entire message (with the
         *   newlines removed) into longbuf 
         */
        longbuf_len = 0;
        for (;;)
        {
            size_t utf8_len;
            size_t rem_len;
            
            /* count the next line */
            ++line_num;
            
            /* 
             *   read the next line - if we reach the end of the file,
             *   that's fine, as long as the message isn't empty 
             */
            if (osfgets(buf, sizeof(buf), fpin) == 0)
            {
                /* make sure the message isn't empty */
                if (longbuf_len == 0)
                {
                    printf("line %d: unexpected end of file in long message "
                           "for message number %d\n", line_num, msg_id);
                    goto done;
                }

                /* that's clearly the end of this message */
                break;
            }

            /* get the length of the line */
            len = strlen(buf);

            /* remove trailing newline characters */
            while (len != 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
                --len;
            
            /* add a null terminator replacing the last newline we found */
            buf[len] = '\0';

            /* skip leading spaces and check for a blank line */
            for (p = buf ; is_space(*p) ; ++p) ;
            if (*p == '\0')
                break;

            /* calculate how much space we have left in our buffer */
            rem_len = sizeof(longbuf) - longbuf_len;

            /* 
             *   append the line to the long message buffer, mapping it to
             *   utf-8 
             */
            utf8_len = mapper->map_str(longbuf + longbuf_len, rem_len, buf);

            /* 
             *   if that required more space than we had, it's an error
             *   (note that it's too much if we consumed exactly the
             *   amount we had left, because that means we didn't have
             *   room for the null terminator byte) 
             */
            if (utf8_len >= rem_len)
            {
                printf("line %d: long message for message ID %d is too "
                       "long\n", line_num, msg_id);
                goto done;
            }

            /* count this new section in the total message length so far */
            longbuf_len += utf8_len;
        }

        /* create a new message structure for this message */
        cur_msg = new msg_t(msg_id, shortbuf, longbuf);

        /* link it in at the end of our list */
        if (last_msg != 0)
            last_msg->nxt_ = cur_msg;
        else
            first_msg = cur_msg;
        last_msg = cur_msg;

        /* count it */
        ++msg_count;
    }

    /* make sure we have at least one message */
    if (msg_count == 0)
    {
        printf("source file contains no messages\n");
        goto done;
    }

    /*
     *   Next, we must put the messages into sorted order by message
     *   number.  Allocate an array so we can perform the sorting.
     */
    msg_array = (msg_t **)t3malloc(msg_count * sizeof(msg_array[0]));

    /* 
     *   set up the array with pointers to the messages in source file
     *   order (i.e., in the order in which they appear in our linked list
     *   of messages) 
     */
    for (cur_msg = first_msg, arrp = msg_array ; cur_msg != 0 ;
         cur_msg = cur_msg->nxt_, ++arrp)
        *arrp = cur_msg;

    /* sort the array of messages by message number */
    qsort(msg_array, msg_count, sizeof(msg_array[0]), &msg_t::compare);

    /* open the output file */
    fpout = osfopwb(fname_out, OSFTERRS);
    if (fpout == 0)
    {
        printf("unable to open output file \"%s\"\n", fname_out);
        goto done;
    }

    /* prepare the message count in portable format */
    oswp2(writebuf, msg_count);

    /* write the signature and message count */
    if (osfwb(fpout, VM_MESSAGE_FILE_SIGNATURE,
              sizeof(VM_MESSAGE_FILE_SIGNATURE))
        || osfwb(fpout, writebuf, 2))
    {
        printf("error writing message file header to output file\n");
        goto done;
    }

    /* write the messages */
    for (arrp = msg_array, i = 0 ; i < msg_count ; ++i, ++arrp)
    {
        size_t short_len;
        size_t long_len;
        
        /* get the lengths of the two messages */
        short_len = strlen((*arrp)->short_msg_);
        long_len = strlen((*arrp)->long_msg_);

        /* 
         *   prepare the message header: message ID, short message length,
         *   long message length 
         */
        oswp4(writebuf, (*arrp)->id_);
        oswp2(writebuf + 4, short_len);
        oswp2(writebuf + 6, long_len);

        /* 
         *   write the message header, followed by the short message's
         *   text, followed by the long message's text 
         */
        if (osfwb(fpout, writebuf, 8)
            || osfwb(fpout, (*arrp)->short_msg_, short_len)
            || osfwb(fpout, (*arrp)->long_msg_, long_len))
        {
            printf("error writing message to output file\n");
            goto done;
        }
    }

    /* if we're in strict mode, check for symbols with no messages */
    if (strict)
    {
        strict_check_ctx ctx;
        
        /* build our strict-check-callback context */
        ctx.msg_array = msg_array;
        ctx.msg_count = msg_count;
        ctx.not_found_count = 0;
        
        /* enumerate all entries through our check callback */
        hashtab->enum_entries(strict_check, &ctx);
    }

    /* show statistics */
    printf("success - %d messages\n", msg_count);

    /* success! */
    stat = OSEXSUCC;

done:
    /* delete all of the messages */
    while (first_msg != 0)
    {
        msg_t *nxt;
        
        /* remember the next one */
        nxt = first_msg->nxt_;

        /* delete this one */
        delete first_msg;

        /* move on to the next one */
        first_msg = nxt;
    }
    
    /* close any files we opened */
    if (fpin != 0)
        osfcls(fpin);
    if (fpout != 0)
        osfcls(fpout);
    
    /* delete any objects we created */
    if (mapper != 0)
        mapper->release_ref();
    if (res_loader != 0)
        delete res_loader;
    if (searcher != 0)
        delete searcher;
    if (regex != 0)
        delete regex;
    if (hashtab != 0)
        delete hashtab;
    if (msg_array != 0)
        t3free(msg_array);

    /* show any unfreed memory (if we're in a debug build) */
    t3_list_memory_blocks(0);

    /* terminate with failure status */
    return OSEXFAIL;
}

