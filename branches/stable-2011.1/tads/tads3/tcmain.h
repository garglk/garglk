/* $Header: d:/cvsroot/tads/tads3/TCMAIN.H,v 1.3 1999/07/11 00:46:53 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcmain.h - TADS 3 Compiler - main compiler driver
Function
  
Notes
  
Modified
  04/22/99 MJRoberts  - Creation
*/

#ifndef TCMAIN_H
#define TCMAIN_H

#include <stdarg.h>

#include "t3std.h"
#include "tcerr.h"

/* 
 *   error display option flags
 */
#define TCMAIN_ERR_VERBOSE  0x00000001              /* use verbose messages */
#define TCMAIN_ERR_NUMBERS  0x00000002       /* include numeric error codes */
#define TCMAIN_ERR_WARNINGS 0x00000004                     /* show warnings */
#define TCMAIN_ERR_PEDANTIC 0x00000008           /* show pendantic warnings */
#define TCMAIN_ERR_TESTMODE 0x00000010                      /* testing mode */
#define TCMAIN_ERR_FNAME_QU 0x00000020                   /* quote filenames */


/* 
 *   main compiler driver class 
 */
class CTcMain
{
public:
    /* Initialize the compiler.  Creates all global objects. */
    static void init(class CTcHostIfc *hostifc,
                     class CResLoader *res_loader,
                     const char *default_charset);

    /* Terminate the compiler.  Deletes all global objects. */
    static void terminate();

    /* initialize/terminate the error subsystem */
    static void tc_err_init(size_t param_stack_size,
                            class CResLoader *res_loader);
    static void tc_err_term();
    
    /* log an error - varargs arguments - static routine */
    static void S_log_error(class CTcTokFileDesc *linedesc, long linenum,
                            int *err_counter, int *warn_counter,
                            unsigned long options,
                            const int *suppress_list, size_t suppress_cnt,
                            tc_severity_t severity, int err, ...)
    {
        va_list args;

        /* pass through to our va_list-style handler */
        va_start(args, err);
        S_v_log_error(linedesc, linenum, err_counter, warn_counter, 0, 0,
                      options, suppress_list, suppress_cnt,
                      severity, err, args);
        va_end(args);
    }

    /* log an error - arguments from a CVmException object */
    static void S_log_error(class CTcTokFileDesc *linedesc, long linenum,
                            int *err_counter, int *warn_counter,
                            unsigned long options,
                            const int *suppress_list, size_t suppress_cnt,
                            tc_severity_t severity,
                            struct CVmException *exc);

    /* show or don't show numeric error codes */
    void set_show_err_numbers(int show)
    {
        if (show)
            err_options_ |= TCMAIN_ERR_NUMBERS;
        else
            err_options_ &= ~TCMAIN_ERR_NUMBERS;
    }

    /* set verbosity for error messages */
    void set_verbosity(int verbose)
    {
        if (verbose)
            err_options_ |= TCMAIN_ERR_VERBOSE;
        else
            err_options_ &= ~TCMAIN_ERR_VERBOSE;
    }

    /* turn all warnings on or off */
    void set_warnings(int show)
    {
        if (show)
            err_options_ |= TCMAIN_ERR_WARNINGS;
        else
            err_options_ &= ~TCMAIN_ERR_WARNINGS;
    }

    /* turn pedantic warnings on or off */
    void set_pedantic(int show)
    {
        if (show)
            err_options_ |= TCMAIN_ERR_PEDANTIC;
        else
            err_options_ &= ~TCMAIN_ERR_PEDANTIC;
    }

    /* 
     *   Get/set regression test reporting mode.  In test mode, we'll only
     *   show the root name of each file in our status reports and error
     *   messages.  This is useful when running regression tests, because it
     *   allows us to diff the compiler output against a reference log
     *   without worrying about the local directory structure.  
     */
    int get_test_report_mode() const
        { return (err_options_ & TCMAIN_ERR_TESTMODE) != 0; }
    void set_test_report_mode(int flag)
    {
        if (flag)
            err_options_ |= TCMAIN_ERR_TESTMODE;
        else
            err_options_ &= ~TCMAIN_ERR_TESTMODE;
    }

    /* 
     *   Set the quoted filenames option.  If this option is set, we'll quote
     *   the filenames we report in our error messages; this makes it easier
     *   for automated tools to parse the error messages, because it ensures
     *   that the contents of a filename won't be mistaken for part of the
     *   error message format.  
     */
    void set_quote_filenames(int flag)
    {
        if (flag)
            err_options_ |= TCMAIN_ERR_FNAME_QU;
        else
            err_options_ &= ~TCMAIN_ERR_FNAME_QU;
    }

    /*
     *   Set an array of warning codes to suppress.  We will suppress any
     *   warnings and pedantic warnings whose error numbers appear in this
     *   array; errors of greater severity will be shown even when they
     *   appear here.
     *   
     *   The memory of this list is managed by the caller.  We merely keep a
     *   reference to the caller's array.  The caller is responsible for
     *   ensuring that the memory remains valid for as long as we're around.
     */
    void set_suppress_list(const int *lst, size_t cnt)
    {
        /* remember the list and its size */
        suppress_list_ = lst;
        suppress_cnt_ = cnt;
    }

    /* log an error - va_list-style arguments - static routine */
    static void S_v_log_error(class CTcTokFileDesc *linedesc, long linenum,
                              int *err_counter, int *warn_counter,
                              int *first_error, int *first_warning,
                              unsigned long options,
                              const int *suppress_list, size_t suppress_cnt,
                              tc_severity_t severity, int err, va_list args);

    /* log an error - varargs */
    void log_error(class CTcTokFileDesc *linedesc, long linenum,
                   tc_severity_t severity, int err, ...)
    {
        va_list args;

        /* pass through to our va_list-style handler */
        va_start(args, err);
        v_log_error(linedesc, linenum, severity, err, args);
        va_end(args);
    }

    void v_log_error(class CTcTokFileDesc *linedesc, long linenum,
                     tc_severity_t severity, int err, va_list args)
    {
        /* call our static routine */
        S_v_log_error(linedesc, linenum, &error_count_, &warning_count_,
                      &first_error_, &first_warning_, err_options_,
                      suppress_list_, suppress_cnt_,
                      severity, err, args);

        /* if we've exceeded the maximum error limit, throw a fatal error */
        check_error_limit();
    }

    /* get the error/warning count */
    int get_error_count() const { return error_count_; }
    int get_warning_count() const { return warning_count_; }

    /* reset the error and warning counters */
    void reset_error_counts()
    {
        /* clear the counters */
        error_count_ = 0;
        warning_count_ = 0;

        /* clear the error/warning memories */
        first_error_ = 0;
        first_warning_ = 0;
    }

    /* 
     *   get the first compilation error/warning code - we keep track of
     *   these for times when we have limited error reporting capabilities
     *   and can only report a single error, such as when we're compiling
     *   an expression in the debugger 
     */
    int get_first_error() const { return first_error_; }
    int get_first_warning() const { return first_warning_; }

    /* 
     *   check the error count against the error limit, and throw a fatal
     *   error if we've exceeded it 
     */
    void check_error_limit();

    /* get the console output character mapper */
    static class CCharmapToLocal *get_console_mapper()
        { return console_mapper_; }

private:
    CTcMain(class CResLoader *res_loader, const char *default_charset);
    ~CTcMain();

    /* error and warning count */
    int error_count_;
    int warning_count_;

    /* first error/warning code we've encountered */
    int first_error_;
    int first_warning_;

    /*
     *   Maximum error limit - if we encounter more than this many errors
     *   in a single compilation unit, we'll abort the compilation with a
     *   fatal error.  This at least limits the amount of garbage we'll
     *   display if we run into a really bad cascading parsing error
     *   situation where we just can't resynchronize.  
     */
    int max_error_count_;

    /* error options - this is a combination of TCMAIN_ERR_xxx flags */
    unsigned long err_options_;

    /*
     *   An array of warning messages to suppress.  We'll only use this to
     *   suppress warnings and pedantic warnings; errors of greater severity
     *   will be displayed even if they appear in this list.  The memory used
     *   for this list is managed by our client; we just keep a reference to
     *   the client's list.  
     */
    const int *suppress_list_;
    size_t suppress_cnt_;

    /* resource loader */
    class CResLoader *res_loader_;

    /* default character set name */
    char *default_charset_;

    /* 
     *   flag: we have tried loading an external compiler error message
     *   file and failed; if we fail once during a session, we won't try
     *   again, to avoid repeated searches for a message file during
     *   compilations of multiple files 
     */
    static int err_no_extern_messages_;

    /* count of references to the error subsystem */
    static int err_refs_;

    /* 
     *   The console character mapper.  We use this to map error messages
     *   to the console character set.  This is a static so that we can
     *   access it from the static error message printer routines.  
     */
    static class CCharmapToLocal *console_mapper_;
};

#endif /* TCMAIN_H */

