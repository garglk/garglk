/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tccmdutl.cpp - TADS 3 Compiler command-line parsing utilities
Function
  Defines some utility functions for command-line parsing.
Notes
  
Modified
  04/03/02 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "os.h"
#include "t3std.h"
#include "tccmdutl.h"

/* ------------------------------------------------------------------------ */
/* 
 *   Get an option argument.  We take a argument vector, the index of the
 *   vector entry containing the option whose argument we're to fetch, and
 *   the length of the option string.  We'll return a pointer to the option
 *   string, or null if no argument is present.
 *   
 *   We'll look for the argument's option first in the same vector entry
 *   containing the option, appended directly to the option string; if
 *   there's nothing there, we'll look for the argument in the next vector
 *   entry.  This lets us find option arguments that use syntax like
 *   "-Itest" or "-I test".  
 */
char *CTcCommandUtil::get_opt_arg(int argc, char **argv,
                                  int *curarg, int optlen)
{
    /* 
     *   if it's jammed up against the option letter, get it from the
     *   current array entry; otherwise, get it from the next array entry 
     */
    if (argv[*curarg][optlen + 1] != '\0')
    {
        /* it's attached to the same option entry */
        return argv[*curarg] + optlen + 1;
    }
    else if (*curarg + 1 < argc)
    {
        /* it's by itself as the next option entry */
        ++(*curarg);
        return argv[*curarg];
    }
    else
    {
        /* it's not present at all */
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Simple option helper implementation for counting arguments.  Some
 *   callers will want to let us scan an option file to get an argument count
 *   before they allocate space for the arguments.  On these count-only
 *   passes, the caller doesn't usually care about the contents of the file,
 *   so they don't want to bother performing the full set of operations that
 *   the helper normally defines.  For these cases, callers can pass us a
 *   null helper, in which case we'll use this simple helper by default. 
 */
class CTcOptFileHelperDefault: public CTcOptFileHelper
{
public:
    /* allocate an option string */
    virtual char *alloc_opt_file_str(size_t len)
    {
        return (char *)t3malloc(len + 1);
    }

    /* free a string allocated with alloc_opt_file_str() */
    virtual void free_opt_file_str(char *str)
    {
        t3free(str);
    }

    /* process a comment line */
    virtual void process_comment_line(const char *) { }

    /* process a non-comment line */
    virtual void process_non_comment_line(const char *) { }

    /* process a configuration line */
    virtual void process_config_line(const char *, const char *, int) { }
};


/* ------------------------------------------------------------------------ */
/*
 *   Parse an options file.  If argv is null, we'll simply count the
 *   arguments and return the count.  If argv is non-null, it must be
 *   allocated with the proper number of slots for the number of arguments
 *   in the file (as obtained from a call to this function with argv = null).
 *   
 *   Each string returned in argv[] is separately allocated with a call to
 *   helper->alloc_opt_file_str().  
 */
int CTcCommandUtil::parse_opt_file(osfildef *fp, char **argv,
                                   CTcOptFileHelper *helper)
{
    char *buf;
    char config_id[128];
    size_t buflen;
    int argc;
    CTcOptFileHelperDefault default_helper;

    /* if they didn't give us a helper object, use our default */
    if (helper == 0)
        helper = &default_helper;

    /* allocate our initial buffer */
    buflen = 512;
    buf = helper->alloc_opt_file_str(buflen);

    /* we're not in a configuration section yet */
    config_id[0] = '\0';

    /* keep going until we run out of text in the file */
    for (argc = 0 ; ; )
    {
        size_t len;
        char *p;

        /* read the next line - stop if we're done */
        if (osfgets(buf, buflen, fp) == 0)
            break;

        /* check for proper termination */
        for (;;)
        {
            /* get the buffer length */
            len = strlen(buf);

            /* check for a newline of some kind at the end */
            if (len != 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            {
                /* 
                 *   remove consecutive newline/return characters - in case
                 *   we are using a file moved from another system with
                 *   incompatible newline conventions, simply remove all of
                 *   the newlines we find 
                 */
                while (len != 0
                       && (buf[len-1] == '\n' || buf[len - 1] == '\r'))
                    --len;

                /* null-terminate the line */
                buf[len] = '\0';

                /* we now have our line */
                break;
            }
            else if (len == buflen - 1)
            {
                char *new_buf;
                size_t new_buf_len;
                
                /* 
                 *   The buffer was completely filled, and wasn't
                 *   null-terminated - the line must be too long for the
                 *   buffer.  Expand the buffer and go back for more.  
                 */
                new_buf_len = buflen + 512;
                new_buf = helper->alloc_opt_file_str(new_buf_len);

                /* copy the old buffer into the new buffer */
                memcpy(new_buf, buf, buflen);

                /* discard the old buffer */
                helper->free_opt_file_str(buf);

                /* switch to the new buffer */
                buf = new_buf;
                buflen = new_buf_len;

                /* append more data into the line buffer and try again */
                if (osfgets(buf + len, buflen - len, fp) == 0)
                    break;
            }
            else
            {
                /* 
                 *   The buffer wasn't completely full, and we didn't find a
                 *   newline.  This must mean that we've reached the end of
                 *   the file, so we've read as much as we can for this
                 *   line.  
                 */
                break;
            }
        }

        /* skip leading spaces */
        for (p = buf ; isspace(*p) ; ++p) ;

        /*
         *   Check to see if we're entering a new configuration section.  If
         *   the line matches the pattern "[Config:xxx]", then we're starting
         *   a new configuration section with identifier "xxx". 
         */
        if (strlen(p) > 8 && memicmp(p, "[config:", 8) == 0)
        {
            char *start;
            size_t copy_len;
            
            /* 
             *   note the part after the colon and up to the closing bracket
             *   - it's the ID of this configuration section 
             */
            for (p += 8, start = p ; *p != ']' && *p != '\0' ; ++p) ;

            /* limit the ID size to our buffer maximum */
            copy_len = p - start;
            if (copy_len > sizeof(config_id) - 1)
                copy_len = sizeof(config_id) - 1;

            /* copy and null-terminate the ID string */
            memcpy(config_id, start, copy_len);
            config_id[copy_len] = '\0';

            /* process the configuration ID line itself as a config line */
            helper->process_config_line(config_id, buf, TRUE);

            /* we're done processing this line */
            continue;
        }

        /* 
         *   if we're in a configuration section, simply process the line
         *   through the helper as a configuration line - this doesn't
         *   contain any options we can parse, since configuration sections
         *   are private to their respective definers 
         */
        if (config_id[0] != '\0')
        {
            /* process the configuration line through the helper */
            helper->process_config_line(config_id, buf, FALSE);

            /* we're done with this line */
            continue;
        }
        
        /* 
         *   If we've reached the end of the line or a command character
         *   ('#'), we're done with this line.  Note that we check for
         *   comments AFTER checking to see if we're in a config section,
         *   because we want to send any comment lines within a config
         *   section to the helper for processing as part of the config -
         *   even comments are opaque to us within a config section.  
         */
        if (*p == '\0' || *p == '#')
        {
            /* tell the helper about the comment */
            helper->process_comment_line(buf);

            /* we're done with the line - go back for the next one */
            continue;
        }

        /* tell the helper about the non-comment line */
        helper->process_non_comment_line(buf);

        /* parse the options on this line */
        for (p = buf ; *p != '\0' ; )
        {
            char *start;
            size_t arg_len;

            /* skip leading spaces */
            for ( ; isspace(*p) ; ++p) ;

            /* check to see if we reached the end of the line */
            if (*p == '\0')
                break;

            /* 
             *   if the argument is quoted, find the matching quote;
             *   otherwise, look for the next space 
             */
            if (*p == '"')
            {
                char *dst;

                /* find the matching quote */
                for (++p, start = dst = p ; *p != '\0' ; ++p)
                {
                    /* 
                     *   if this is a quote, check for stuttering - if it's
                     *   stuttered, treat it as a single quote (and convert
                     *   it to same), otherwise we've reached the end of the
                     *   argument 
                     */
                    if (*p == '"')
                    {
                        /* check for stuttering */
                        if (*(p+1) == '"')
                        {
                            /* 
                             *   it's stuttered - skip the extra quote, so
                             *   that we copy only a single quote 
                             */
                            ++p;
                        }
                        else
                        {
                            /* this is our closing quote - skip it */
                            ++p;

                            /* we're done with the argument */
                            break;
                        }
                    }

                    /* copy this character to the output */
                    *dst++ = *p;
                }

                /* note the length of the argument */
                arg_len = dst - start;
            }
            else
            {
                /* find the next space */
                for (start = p ; *p != '\0' && !isspace(*p) ; ++p) ;

                /* note the length of the argument */
                arg_len = p - start;
            }

            /* store the argument if we have an output vector */
            if (argv != 0)
            {
                /* allocate the argument */
                argv[argc] = helper->alloc_opt_file_str(arg_len + 1);

                /* copy it into the result */
                memcpy(argv[argc], start, arg_len);
                argv[argc][arg_len] = '\0';
            }

            /* count the argument */
            ++argc;
        }
    }

    /* done with our line buffer - delete it */
    helper->free_opt_file_str(buf);

    /* return the argument count */
    return argc;
}

