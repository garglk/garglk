/* 
 *   Copyright (c) 2002, 2002 Michael J Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tclibprs.cpp - tads compiler: library parser
Function
  Parses .tl files, which are text files that reference source files
  for inclusion in a compilation.  A .tl file can be used in a compilation
  as though it were a source file, and stands for the set of source files
  it references.  A .tl file can reference other .tl files.
Notes
  
Modified
  01/08/02 MJRoberts  - Creation
*/

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "os.h"
#include "t3std.h"
#include "tclibprs.h"
#include "tcsrc.h"


/*
 *   Instantiate.  We'll note the name of the library, but we don't attempt
 *   to open it at this point - we won't open the file until parse_lib() is
 *   called.  
 */
CTcLibParser::CTcLibParser(const char *lib_name)
{
    char lib_path[OSFNMAX];

    /* 
     *   Extract the library's directory path.  Since files within a library
     *   are always relative to the directory containing the library itself,
     *   we use the full path to the library as the starting point for each
     *   file found within the library.  
     */
    os_get_path_name(lib_path, sizeof(lib_path), lib_name);

    /* remember the library path */
    lib_path_ = lib_copy_str(lib_path);

    /* remember the name of the library */
    lib_name_ = lib_copy_str(lib_name);

    /* no errors yet */
    err_cnt_ = 0;

    /* no lines read yet */
    linenum_ = 0;
}

CTcLibParser::~CTcLibParser()
{
    /* delete our library filename and path */
    lib_free_str(lib_name_);
    lib_free_str(lib_path_);
}

/*
 *   Parse a library file. 
 */
void CTcLibParser::parse_lib()
{
    CTcSrcFile *fp;

    /* open our file - we only accept plain ASCII here */
    fp = CTcSrcFile::open_ascii(lib_name_);

    /* if that failed, abort */
    if (fp == 0)
    {
        err_open_file();
        ++err_cnt_;
        return;
    }

    /* keep going until we run out of file */
    for (;;)
    {
        char buf[1024];
        char expbuf[1024];
        size_t len;
        char *p;
        char *dst;
        char *sep;
        char *var_name;
        char *var_val;

        /* read the next line of text */
        if ((len = fp->read_line(buf, sizeof(buf))) == 0)
            break;

        /* count the line */
        ++linenum_;

        /* un-count the terminating null byte in the length */
        --len;

        /* check for a terminating newline sequence */
        while (len != 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            --len;

        /* if we didn't find a newline, the line is too long */
        if (buf[len] == '\0')
        {
            char buf2[128];
            size_t len2;
            
            /* 
             *   If we can read anything more from the file, this indicates
             *   that the line was simply too long to read into our buffer;
             *   skip the rest of the line and log an error in this case.
             *   If we can't read any more text, it means we've reached the
             *   end of the file. 
             */
            if ((len2 = fp->read_line(buf2, sizeof(buf2))) != 0)
            {
                /* there's more, so the line is too long - flag an error */
                err_line_too_long();
                ++err_cnt_;

                /* skip text until we find a newline */
                for (;;)
                {
                    /* un-count the null terminator */
                    --len2;

                    /* if we've found our newline, we're done skipping */
                    if (len2 != 0
                        && (buf2[len2-1] == '\n' || buf2[len2-1] == '\r'))
                        break;
                    
                    /* read another chunk */
                    if ((len2 = fp->read_line(buf2, sizeof(buf2))) == 0)
                        break;
                }

                /* we've skipped the line, so go back for the next */
                continue;
            }
        }

        /* remove the newline from the buffer */
        buf[len] = '\0';

        /* scan for and expand preprocessor symbols */
        for (p = buf, dst = expbuf ; *p != '\0' ; ++p)
        {
            /* check for the start of a preprocessor symbol */
            if (*p == '$')
            {
                /* 
                 *   We have a dollar sign, so this *could* be a preprocessor
                 *   symbol.  If the next character is '(', then find the
                 *   matching ')', and take the part between the parentheses
                 *   as a symbol to expand.  If the next character is '$',
                 *   replace the pair with a single dollar sign.  If the next
                 *   character is anything else, the '$' is not significant,
                 *   so leave it as it is.  
                 */
                if (*(p+1) == '(')
                {
                    char *closep;

                    /* find the close paren */
                    for (closep = p + 2 ; *closep != '\0' && *closep != ')' ;
                         ++closep) ;

                    /* 
                     *   if we found the close paren, process it; if not,
                     *   just ignore the whole thing and treat the '$(' as a
                     *   pair of ordinary characters 
                     */
                    if (*closep == ')')
                    {
                        int vallen;
                        size_t symlen;
                        size_t rem;

                        /* skip to the start of the symbol name */
                        p += 2;

                        /* get the length of the symbol */
                        symlen = closep - p;

                        /* figure out how much space we have left */
                        rem = sizeof(expbuf) - (dst - expbuf);

                        /* look up the symbol's value */
                        vallen = get_pp_symbol(dst, rem, p, symlen);

                        /* note overflow or undefined symbol errors */
                        if (vallen < 0)
                        {
                            /* it's undefined - note it */
                            err_undef_sym(p, symlen);
                        }
                        else if ((size_t)vallen > rem)
                        {
                            /* it's too long - note it and give up now */
                            err_expanded_line_too_long();
                            break;
                        }
                        else
                        {
                            /* success - advance past the value */
                            dst += vallen;
                        }

                        /* 
                         *   Skip to the close paren and continue with the
                         *   main loop.  We don't need to copy any input for
                         *   the symbol, because we've already copied the
                         *   expansion instead; so just go back to the start
                         *   of the input loop.  
                         */
                        p = closep;
                        continue;
                    }
                    else
                    {
                        /* ill-formed macro substitution expression */
                        err_invalid_dollar();
                    }
                }
                else if (*(p+1) == '$')
                {
                    /* 
                     *   we have a double dollar sign, so turn it into a
                     *   single dollar sign - simply skip the first of the
                     *   pair so we only keep one 
                     */
                    ++p;
                }
                else
                {
                    /* invalid '$' sequence - warn about it */
                    err_invalid_dollar();
                }
            }

            /* make sure we have room for this character plus a null byte */
            if ((dst - expbuf) + 2 > sizeof(expbuf))
            {
                err_expanded_line_too_long();
                break;
            }

            /* copy this character to the destination string */
            *dst++ = *p;
        }

        /* null-terminate the expanded buffer */
        *dst = '\0';

        /* skip leading spaces */
        for (p = expbuf ; isspace(*p) ; ++p) ;

        /* if the line is empty or starts with '#', skip it */
        if (*p == '\0' || *p == '#')
            continue;

        /* the variable name starts here */
        var_name = p;

        /* the variable name ends at the next space or colon */
        for ( ; *p != '\0' && *p != ':' && !isspace(*p) ; ++p) ;

        /* note where the variable name ends */
        sep = p;

        /* skip spaces after the end of the variable name */
        for ( ; isspace(*p) ; ++p) ;

        /* 
         *   if we didn't find a colon after the variable name, it's an
         *   error; flag the error and skip the line 
         */
        if (*p != ':')
        {
            /* check for stand-alone flags */
            if (stricmp(var_name, "nodef") == 0)
            {
                /* scan the "nodef" flag */
                scan_nodef();
            }
            else
            {
                /* 
                 *   it's not a valid stand-alone flag; the definition must
                 *   be missing the value portion 
                 */
                err_missing_colon();
                ++err_cnt_;
            }

            /* we're done with this line now */
            continue;
        }

        /* put a null terminator at the end of the variable name */
        *sep = '\0';

        /* skip any spaces after the colon */
        for (++p ; isspace(*p) ; ++p) ;

        /* the value starts here */
        var_val = p;

        /* scan the value */
        scan_var(var_name, var_val);
    }

    /* we're done - close the file */
    delete fp;
}

/*
 *   Scan a variable 
 */
void CTcLibParser::scan_var(const char *name, const char *val)
{
    /* call the appropriate routine based on the variable name */
    if (stricmp(name, "name") == 0)
        scan_name(val);
    else if (stricmp(name, "source") == 0)
        scan_source(val);
    else if (stricmp(name, "library") == 0)
        scan_library(val);
    else if (stricmp(name, "resource") == 0)
        scan_resource(val);
    else if (stricmp(name, "needmacro") == 0)
        scan_needmacro(val);
    else
        err_unknown_var(name, val);
}

/*
 *   Scan a source filename.  We build the full filename by combining the
 *   library path and the given filename, then we call scan_full_source().  
 */
void CTcLibParser::scan_source(const char *val)
{
    char rel_path[OSFNMAX];
    char full_name[OSFNMAX];

    /* convert the value from a URL-style path to a local path */
    os_cvt_url_dir(rel_path, sizeof(rel_path), val);

    /* build the full name */
    os_build_full_path(full_name, sizeof(full_name), lib_path_, rel_path);

    /* call the full filename scanner */
    scan_full_source(val, full_name);
}

/*
 *   Scan a library filename.  We build the full filename by combining the
 *   enclosing library path and the given filename, then we call
 *   scan_full_library().  
 */
void CTcLibParser::scan_library(const char *val)
{
    char rel_path[OSFNMAX];
    char full_name[OSFNMAX];

    /* convert the value from a URL-style path to a local path */
    os_cvt_url_dir(rel_path, sizeof(rel_path), val);

    /* build the full name */
    os_build_full_path(full_name, sizeof(full_name), lib_path_, rel_path);

    /* call the full library filename scanner */
    scan_full_library(val, full_name);
}

/*
 *   Scan a resource filename.  We build the full filename by combining the
 *   enclosing library path and the given filename, then we call
 *   scan_full_resource().  
 */
void CTcLibParser::scan_resource(const char *val)
{
    char rel_path[OSFNMAX];
    char full_name[OSFNMAX];

    /* convert the value from a URL-style path to a local path */
    os_cvt_url_dir(rel_path, sizeof(rel_path), val);

    /* build the full name */
    os_build_full_path(full_name, sizeof(full_name), lib_path_, rel_path);

    /* call the full resource filename scanner */
    scan_full_resource(val, full_name);
}

/*
 *   scan a "needmacro" definition 
 */
void CTcLibParser::scan_needmacro(const char *val)
{
    const char *p;
    const char *macro_name;
    size_t macro_len;

    /* skip leading whitespace */
    for (p = val ; isspace(*p) ; ++p) ;

    /* the macro starts here */
    macro_name = p;

    /* find the next whitspace, which separates the token */
    for ( ; *p != '\0' && !isspace(*p) ; ++p) ;

    /* note the length of the macro name */
    macro_len = p - macro_name;

    /* skip the whitspace */
    for ( ; isspace(*p) ; ++p) ;

    /* process the parsed needmacro definition */
    scan_parsed_needmacro(macro_name, macro_len, p);
}

/*
 *   Scan a parsed "needmacro" definition.  This default implementation shows
 *   an error and the library-defined warning text.  
 */
void CTcLibParser::scan_parsed_needmacro(const char *macro_name,
                                         size_t macro_len,
                                         const char *warning_text)
{
    char buf[100+128+128];

    /* look up the symbol; if it's not defined, show a warning */
    if (get_pp_symbol(buf, sizeof(buf), macro_name, macro_len) < 0)
    {
        /* limit the symbol length to avoid overflowing the error buffer */
        if (macro_len > 128)
            macro_len = 128;

        /* show a suitable warning */
        src_err_msg("library requires preprocessor symbol \"%.*s\" to be "
                    "defined; use -D %.*s=value to define it (%s)",
                    (int)macro_len, macro_name, (int)macro_len, macro_name,
                    warning_text);
    }
}

/* 
 *   log an error in a source line 
 */
void CTcLibParser::src_err_msg(const char *msg, ...)
{
    char buf[1024];
    va_list args;

    /* format the caller's message and its arguments */
    va_start(args, msg);
    t3vsprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    /* display the message with the source name and line number */
    err_msg("%s (%lu): %s", lib_name_, linenum_, buf);
}
