#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCMAIN.CPP,v 1.4 1999/07/11 00:46:53 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcmain.cpp - TADS 3 Compiler - main compiler driver
Function
  
Notes
  
Modified
  04/22/99 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmerr.h"
#include "tcglob.h"
#include "tcmain.h"
#include "tcerr.h"
#include "tctok.h"
#include "utf8.h"
#include "charmap.h"
#include "tchost.h"
#include "tcprs.h"
#include "tcgen.h"
#include "tctarg.h"
#include "charmap.h"
#include "resload.h"
#include "tcunas.h"


/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* references to the error subsystem */
int CTcMain::err_refs_ = 0;

/* flag: we have failed to load external messages */
int CTcMain::err_no_extern_messages_ = FALSE;

/* console output character mapper */
CCharmapToLocal *CTcMain::console_mapper_ = 0;


/* ------------------------------------------------------------------------ */
/*
 *   Initialize the error subsystem for the compiler 
 */
void CTcMain::tc_err_init(size_t param_stack_size,
                          CResLoader *res_loader)
{
    /* initialize the error stack */
    err_init(1024);

    /* if this is the first initializer, set things up */
    if (err_refs_ == 0)
    {
        /* if we haven't loaded external compiler messages, load them */
        if (!err_no_extern_messages_
            && tc_messages == &tc_messages_english[0])
        {
            osfildef *fp;
            
            /* try finding a message file */
            fp = res_loader->open_res_file("t3make.msg", 0, "XMSG");
            if (fp != 0)
            {
                /* try loading it */
                err_load_message_file(fp, &tc_messages, &tc_message_count,
                                      &tc_messages_english[0],
                                      tc_message_count_english);
                
                /* done with the file */
                osfcls(fp);
            }
            else
            {
                /* note the failure, so we don't try again */
                err_no_extern_messages_ = FALSE;
            }
        }
    }

    /* count the reference depth */
    ++err_refs_;
}

/*
 *   terminate the error subsystem for the compiler 
 */
void CTcMain::tc_err_term()
{
    /* reduce the reference count */
    --err_refs_;

    /* delete the error stack */
    err_terminate();

    /* if this is the last reference, clean things up */
    if (err_refs_ == 0)
    {
        /* if we loaded an external message file, unload it */
        err_delete_message_array(&tc_messages, &tc_message_count,
                                 &tc_messages_english[0],
                                 tc_message_count_english);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Initialize the compiler 
 */
void CTcMain::init(CTcHostIfc *hostifc, CResLoader *res_loader,
                   const char *default_charset)
{
    /* initialize the error subsystem */
    tc_err_init(1024, res_loader);
    
    /* remember the host interface */
    G_hostifc = hostifc;

    /* perform static initializations on the parser symbol table class */
    CTcPrsSymtab::s_init();

    /* 
     *   Set the image file structure sizes for the current T3 VM spec we're
     *   targeting.  When we're doing dynamic compilation, the DynamicFunc
     *   object will overwrite these with the actual settings for the loaded
     *   image file before it invokes the code generator.  
     */
    G_sizes.mhdr = TCT3_METHOD_HDR_SIZE;
    G_sizes.exc_entry = TCT3_EXC_ENTRY_SIZE;
    G_sizes.dbg_hdr = TCT3_DBG_HDR_SIZE;
    G_sizes.dbg_line = TCT3_LINE_ENTRY_SIZE;
    G_sizes.lcl_hdr = TCT3_DBG_LCLSYM_HDR_SIZE;
    G_sizes.dbg_fmt_vsn = TCT3_DBG_FMT_VSN;
    G_sizes.dbg_frame = TCT3_DBG_FRAME_SIZE;

    /* assume we won't have a loaded image file metaclass table */
    G_metaclass_tab = 0;

    /* we don't have a dynamic compiler interface yet */
    G_vmifc = 0;

    /* create the compiler main object */
    G_tcmain = new CTcMain(res_loader, default_charset);
}

/*
 *   Terminate the compiler 
 */
void CTcMain::terminate()
{
    /* delete the tokenizer */
    delete G_tok;
    G_tok = 0;

    /* delete the compiler main object */
    delete G_tcmain;
    G_tcmain = 0;

    /* forget any object and property fixups */
    G_objfixup = 0;
    G_propfixup = 0;
    G_enumfixup = 0;

    /* 
     *   make sure we explicitly turn the fixup flags on again if we want
     *   them in a future run 
     */
    G_keep_objfixups = FALSE;
    G_keep_propfixups = FALSE;
    G_keep_enumfixups = FALSE;

    /* perform static termination on the parser symbol table class */
    CTcPrsSymtab::s_terminate();

    /* forget the host interface */
    G_hostifc = 0;

    /* terminate the error subsystem */
    tc_err_term();
}

/* ------------------------------------------------------------------------ */
/*
 *   set up the compiler 
 */
CTcMain::CTcMain(CResLoader *res_loader, const char *default_charset)
{
    char csbuf[OSFNMAX];
    
    /* 
     *   if the caller didn't provide a default character set, ask the OS
     *   what we should use
     */
    if (default_charset == 0)
    {
        /* 
         *   ask the OS what to use for file contents, since we use this
         *   character set to translate the text we read from source files 
         */
        os_get_charmap(csbuf, OS_CHARMAP_FILECONTENTS);
        
        /* use our OS-provided character set */
        default_charset = csbuf;
    }

    /* if there's no static console output character map, create one */
    if (console_mapper_ == 0)
    {
        char mapname[32];

        /* get the console character set name */
        os_get_charmap(mapname, OS_CHARMAP_DISPLAY);

        /* create a resource loader for the console character map */
        console_mapper_ = CCharmapToLocal::load(res_loader, mapname);

        /* if that failed, create an ASCII mapper */
        if (console_mapper_ == 0)
            console_mapper_ = CCharmapToLocal::load(res_loader, "us-ascii");
    }
    
    /* remember our resource loader */
    res_loader_ = res_loader;
    
    /* 
     *   set default options - minimum verbosity, no numeric error codes,
     *   show standard warnings but not pedantic warnings, not test mode 
     */
    err_options_ = TCMAIN_ERR_WARNINGS;

    /* we have no warning suppression list yet */
    suppress_list_ = 0;
    suppress_cnt_ = 0;

    /* remember our default character set */
    default_charset_ = lib_copy_str(default_charset);

    /* create the tokenizer */
    G_tok = new CTcTokenizer(res_loader_, default_charset_);
    
    /* 
     *   Create the parser and node memory pool.  Create the memory pool
     *   first, because the parser allocates objects out of the pool. 
     */
    G_prsmem = new CTcPrsMem();
    G_prs = new CTcParser();

    /* create the generator data stream (for constant data) */
    G_ds = new CTcDataStream(TCGEN_DATA_STREAM);

    /* create the primary generator code stream */
    G_cs_main = new CTcCodeStream(TCGEN_CODE_STREAM);

    /* create the static initializer code stream */
    G_cs_static = new CTcCodeStream(TCGEN_STATIC_CODE_STREAM);

    /* make the primary code stream active */
    G_cs = G_cs_main;

    /* create the generator object data stream */
    G_os = new CTcDataStream(TCGEN_OBJECT_STREAM);

    /* create the intrinsic class modifier object data stream */
    G_icmod_stream = new CTcDataStream(TCGEN_ICMOD_STREAM);

    /* create the dictionary object data stream */
    G_dict_stream = new CTcDataStream(TCGEN_DICT_STREAM);

    /* create the grammar-production object data stream */
    G_gramprod_stream = new CTcDataStream(TCGEN_GRAMPROD_STREAM);

    /* create the BigNumber object data stream */
    G_bignum_stream = new CTcDataStream(TCGEN_BIGNUM_STREAM);

    /* create the RexPattern object data stream */
    G_rexpat_stream = new CTcDataStream(TCGEN_REXPAT_STREAM);

    /* create the IntrinsicClass object data stream */
    G_int_class_stream = new CTcDataStream(TCGEN_INTCLASS_STREAM);

    /* create the static initializer identifier stream */
    G_static_init_id_stream = new CTcDataStream(TCGEN_STATIC_INIT_ID_STREAM);

    /* create the local variable name stream */
    G_lcl_stream = new CTcDataStream(TCGEN_LCL_VAR_STREAM);

    /* create the target-specific code generator */
    G_cg = new CTcGenTarg();

    /* initialize the parser */
    G_prs->init();

    /* no errors or warnings yet */
    error_count_ = 0;
    warning_count_ = 0;
    first_error_ = 0;
    first_warning_ = 0;

    /* set a fairly liberal maximum error limit */
    max_error_count_ = 100;

    /* there's no disassembly output stream yet */
    G_disasm_out = 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   delete the compiler driver 
 */
CTcMain::~CTcMain()
{
    /* if there's a disassembly stream, delete it */
    if (G_disasm_out != 0)
        delete G_disasm_out;

    /* delete the various data streams */
    delete G_cs_main;
    delete G_cs_static;
    delete G_ds;
    delete G_os;
    delete G_icmod_stream;
    delete G_dict_stream;
    delete G_gramprod_stream;
    delete G_bignum_stream;
    delete G_rexpat_stream;
    delete G_int_class_stream;
    delete G_static_init_id_stream;
    delete G_lcl_stream;

    /* delete the console output character map, if there is one */
    if (console_mapper_ != 0)
    {
        /* release our reference on it */
        console_mapper_->release_ref();

        /* forget it (since it's static) */
        console_mapper_ = 0;
    }

    /* delete the target-specific code generator */
    delete G_cg;

    /* delete the parser and node memory pool */
    delete G_prs;
    delete G_prsmem;

    /* delete the parser */
    delete G_tok;

    /* delete our default character set name string */
    lib_free_str(default_charset_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Log an error, part 1: show the message prefix.  This should be called
 *   before formatting the text of the message, and part 2 should be called
 *   after formatting the message text.  Returns a pointer to the message
 *   template text.  
 */
static const char *log_msg_internal_1(
    CTcTokFileDesc *linedesc, long linenum,
    int *err_counter, int *warn_counter, int *first_error, int *first_warning,
    unsigned long options, const int *suppress_list, size_t suppress_cnt,
    tc_severity_t severity, int err)
{
    const char *msg;
    const char *prefix;

    /*
     *   If this is a warning or a pedantic warning, and it's in the list of
     *   suppressed messages, ignore it entirely.  
     */
    if (severity == TC_SEV_PEDANTIC || severity == TC_SEV_WARNING)
    {
        size_t rem;
        const int *p;

        /* scan the suppress list */
        for (p = suppress_list, rem = suppress_cnt ; rem != 0 ; ++p, --rem)
        {
            /* check for a match */
            if (*p == err)
            {
                /* it's in the suppress list - ignore the error */
                return 0;
            }
        }
    }

    /* increment the appropriate counter */
    switch(severity)
    {
    case TC_SEV_INFO:
        /* 
         *   we don't need to count informational messages, and no prefix is
         *   required 
         */
        prefix = "";
        break;

    case TC_SEV_PEDANTIC:
        /* 
         *   if we're not in "pedantic" mode, or we're not even showing
         *   regular errors, ignore it 
         */
        if (!(options & TCMAIN_ERR_PEDANTIC)
            || !(options & TCMAIN_ERR_WARNINGS))
            return 0;

        /* if this is the first warning, remember the code */
        if (*warn_counter == 0 && first_warning != 0)
            *first_warning = err;

        /* count it */
        ++(*warn_counter);

        /* set the prefix */
        prefix = "warning";
        break;

    case TC_SEV_WARNING:
        /* if we're suppressing warnings, ignore it */
        if (!(options & TCMAIN_ERR_WARNINGS))
            return 0;

        /* if this is the first warning, remember the code */
        if (*warn_counter == 0 && first_warning != 0)
            *first_warning = err;

        /* count the warning */
        ++(*warn_counter);

        /* use an appropriate prefix */
        prefix = "warning";
        break;

    case TC_SEV_ERROR:
        /* if this is the first error, remember the code */
        if (*err_counter == 0 && first_error != 0)
            *first_error = err;

        /* count the error */
        ++(*err_counter);

        /* use an appropriate prefix */
        prefix = "error";
        break;

    case TC_SEV_FATAL:
        /* if this is the first error, remember the code */
        if (*err_counter == 0 && first_error != 0)
            *first_error = err;

        /* count this as an error */
        ++(*err_counter);

        /* use an appropriate prefix */
        prefix = "fatal error";
        break;

    case TC_SEV_INTERNAL:
        /* if this is the first error, remember the code */
        if (*err_counter == 0 && first_error != 0)
            *first_error = err;

        /* count this as an error */
        ++(*err_counter);

        /* use an appropriate prefix */
        prefix = "internal error";
        break;
    }

    /* display the current parsing position, if available */
    if (linedesc != 0)
    {
        const char *fname;
        char qu_buf[OSFNMAX*2 + 2];

        /* get the filename from the source descriptor */
        fname = linedesc->get_fname();

        /* 
         *   if we're in test reporting mode, show only the root part of the
         *   filename 
         */
        if ((options & TCMAIN_ERR_TESTMODE) != 0)
            fname = os_get_root_name((char *)fname);

        /* if they want quoted filenames, quote the filename */
        if ((options & TCMAIN_ERR_FNAME_QU) != 0)
        {
            const char *src;
            char *dst;

            /* quote each character of the filename */
            for (src = fname, qu_buf[0] = '"', dst = qu_buf + 1 ;
                 *src != '\0' ; )
            {
                /* if this is a quote character, stutter it */
                if (*src == '"')
                    *dst++ = '"';

                /* add this character */
                *dst++ = *src++;
            }

            /* add a closing quote and trailing null */
            *dst++ = '"';
            *dst = '\0';

            /* use the quoted version of the filename */
            fname = qu_buf;
        }

        /* show the filename and line number prefix */
        G_hostifc->print_err("%s(%ld): ", fname, linenum);
    }

    /* display the error type prefix */
    G_hostifc->print_err("%s", prefix);

    /* add the error number, if we're showing error numbers */
    if ((options & TCMAIN_ERR_NUMBERS) != 0 && severity != TC_SEV_INFO)
        G_hostifc->print_err(" %u", (unsigned int)err);

    /* add a colon and a space for separation */
    G_hostifc->print_err(": ");

    /* get the error message */
    msg = tcerr_get_msg(err, (options & TCMAIN_ERR_VERBOSE) != 0);
    if (msg == 0)
        msg = "[Unable to find message text for this error code.  "
              "This might indicate an internal problem with the compiler, "
              "or it might be caused by an installation problem that is "
              "preventing the compiler from finding an external message "
              "file that it requires.]";

    /* return the message text, so the caller can format it with arguments */
    return msg;
}

/*
 *   Log an error, part 2: show the message suffix.
 */
static void log_msg_internal_2(unsigned long options, tc_severity_t severity)
{
    /* 
     *   if we're in verbose mode, and this is an internal error, add the
     *   internal error explanation text 
     */
    if (severity == TC_SEV_INTERNAL && (options & TCMAIN_ERR_VERBOSE) != 0)
    {
        const char *msg;
        
        /* get the internal error explanation text and display it */
        msg = tcerr_get_msg(TCERR_INTERNAL_EXPLAN, TRUE);
        if (msg != 0)
            G_hostifc->print_err("\n%s", msg);
    }

    /* end the line */
    G_hostifc->print_err((options & TCMAIN_ERR_VERBOSE) != 0 ? "\n\n" : "\n");
}

/* ------------------------------------------------------------------------ */
/*
 *   Print an error message.  Word-wrap the message if we're in verbose mode.
 */
static void format_message(const char *msg, unsigned long options)
{
    /*
     *   if we're in verbose mode, word-wrap to 80 columns; otherwise just
     *   print it as-is 
     */
    if ((options & TCMAIN_ERR_VERBOSE) != 0)
    {
        const char *p;
        const int line_wid = 79;

        /* start on a new line, to skip any prefix text */
        G_hostifc->print_err("\n");

        /* word-wrap to 80 columns */
        for (p = msg ; *p != '\0' ; )
        {
            const char *start;
            const char *sp;

            /* find the next word break */
            for (sp = 0, start = p ; ; )
            {
                /* if this is a space, note it */
                if (*p == ' ')
                    sp = p;

                /* 
                 *   if we're at the end of the line, or we're over the line
                 *   width and we found a space, break here 
                 */
                if (*p == '\0' || (p - start >= line_wid && sp != 0))
                {
                    /* if we've reached the end, print the rest */
                    if (*p == '\0')
                        sp = p;

                    /* trim off trailing spaces */
                    for ( ; sp > start && *(sp-1) == ' ' ; --sp) ;

                    /* show this part */
                    G_hostifc->print_err("%.*s\n", sp - start, start);

                    /* skip leading spaces */
                    for ( ; *sp == ' ' ; ++sp) ;

                    /* if this is the end of the line, we're done */
                    if (*p == '\0')
                        break;

                    /* start over here */
                    start = p = sp;
                }
                else
                {
                    /* this one fits - skip it */
                    ++p;
                }
            }
        }
    }
    else
    {
        /* display it */
        G_hostifc->print_err("%s", msg);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   log an error from an exception object 
 */
void CTcMain::S_log_error(CTcTokFileDesc *linedesc, long linenum,
                          int *err_counter, int *warn_counter,
                          unsigned long options,
                          const int *suppress_list, size_t suppress_cnt,
                          tc_severity_t severity, CVmException *exc)
{
    const char *msg;
    char msgbuf[1024];
    
    /* show the prefix */
    msg = log_msg_internal_1(linedesc, linenum, err_counter, warn_counter,
                             0, 0, options, suppress_list, suppress_cnt,
                             severity, exc->get_error_code());

    /* if the message is suppressed, we're done */
    if (msg == 0)
        return;

    /* format the message using arguments stored in the exception */
    err_format_msg(msgbuf, sizeof(msgbuf), msg, exc);

    /* print the error */
    format_message(msgbuf, options);

    /* show the suffix */
    log_msg_internal_2(options, severity);
}

/* ------------------------------------------------------------------------ */
/*
 *   Log an error using va_list arguments
 */
void CTcMain::S_v_log_error(CTcTokFileDesc *linedesc, long linenum,
                            int *err_counter, int *warn_counter,
                            int *first_error, int *first_warning,
                            unsigned long options,
                            const int *suppress_list, size_t suppress_cnt,
                            tc_severity_t severity, int err, va_list args)
{
    const char *msg;
    char msgbuf[2048];

    /* show the prefix */
    msg = log_msg_internal_1(linedesc, linenum, err_counter, warn_counter,
                             first_error, first_warning, options,
                             suppress_list, suppress_cnt, severity, err);

    /* if the message is suppressed, we're done */
    if (msg == 0)
        return;

    /* format the message using the va_list argument */
    t3vsprintf(msgbuf, sizeof(msgbuf), msg, args);

    /* display it */
    format_message(msgbuf, options);

    /* show the suffix */
    log_msg_internal_2(options, severity);
}

/* ------------------------------------------------------------------------ */
/*
 *   Check the current error count against the maximum error limit, and throw
 *   a fatal error if we've reached the limit.  
 */
void CTcMain::check_error_limit()
{
    /* check the error count against the limit */
    if (error_count_ > max_error_count_)
    {
        /* 
         *   raise the maximum error count a bit so that we don't encounter
         *   another maximum error situation and loop on flagging the
         *   too-many-errors error while trying to display a too-many-errors
         *   error 
         */
        max_error_count_ = error_count_ + 100;

        /* display a message explaining the problem */
        log_error(G_tok->get_last_desc(), G_tok->get_last_linenum(),
                  TC_SEV_FATAL, TCERR_TOO_MANY_ERRORS);

        /* throw the generic fatal error, since we've logged this */
        err_throw(TCERR_FATAL_ERROR);
    }
}

