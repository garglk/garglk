/* 
 *   Copyright (c) 2002, 2002 Michael J Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tclibprs.h - tads compiler: library parser
Function
  Parses .tl files, which are text files that reference source files for
  inclusion in a compilation.  A .tl file can be used in a compilation as
  though it were a source file, and stands for the set of source files it
  references.  A .tl file can reference other .tl files.
Notes

Modified
  01/08/02 MJRoberts  - Creation
*/

#ifndef TCLIBPRS_H
#define TCLIBPRS_H

#include "os.h"

/*
 *   Library parser class.  This class calls virtual methods to process the
 *   particular items found in a library file; we do nothing with the
 *   information we find in a library by default, so subclasses are expected
 *   to override the scan_xxx methods to carry out the desired processing. 
 */
class CTcLibParser
{
public:
    CTcLibParser(const char *lib_name);
    virtual ~CTcLibParser();

    /* 
     *   Parse a library file.  We'll open the file and parse its contents.
     *   If any errors occur, we'll call one of the err_xxx methods to
     *   indicate the error.  
     */
    void parse_lib();

    /* get the error count */
    int get_err_cnt() const { return err_cnt_; }

protected:
    /* error: could not open the library */
    virtual void err_open_file()
    {
        err_msg("unable to open library file \"%s\"", lib_name_);
    }

    /* error: line of text in file is too long */
    virtual void err_line_too_long()
    {
        src_err_msg("line is too long");
    }

    /* error: line of text is too long after symbol substitution */
    virtual void err_expanded_line_too_long()
    {
        src_err_msg("line is too long after symbol substitution");
    }

    /* error: missing colon */
    virtual void err_missing_colon()
    {
        src_err_msg("missing colon");
    }

    /* error: unknown field definition (the "xxx" in "xxx: val") */
    virtual void err_unknown_var(const char *name, const char *val)
    {
        src_err_msg("unknown definition \"%s\"", name);
    }

    /* error: undefined preprocessor symbol */
    virtual void err_undef_sym(const char *sym_name, size_t sym_len)
    {
        src_err_msg("undefined symbol \"%.*s\"", (int)sym_len, sym_name);
    }

    /* error: invalid '$' sequence */
    virtual void err_invalid_dollar()
    {
        src_err_msg("invalid '$' sequence - use '$$' for a literal "
                    "dollar sign character");
    }

    /* log an error message */
    virtual void err_msg(const char *msg, ...)
    {
        /* do nothing by default */
    }

    /* log an error in a source line */
    virtual void src_err_msg(const char *msg, ...);

    /*
     *   Scan a variable.  This is the main scanning dispatcher: we'll call
     *   one of the specific scan_xxx routines based on the variable name.
     *   Most subclasses will want to override the specific variable scan
     *   routines rather than this one, but some subclasses might wish to
     *   override this routine in order to write a catch-all that applies
     *   some operation uniformly to all variable names. 
     */
    virtual void scan_var(const char *name, const char *val);

    /* 
     *   Scan the "name" variable - this gives a human-readable name for the
     *   library that can be used to describe the library in a user
     *   interface.
     */
    virtual void scan_name(const char *val) { }

    /* 
     *   Scan a full source filename - this is the high-level scanner for
     *   the "source" variable, which receives the fully-expanded name of
     *   the referenced file.  
     */
    virtual void scan_full_source(const char *val, const char *full_fname)
        { }

    /*
     *   Scan a full library filename - this is the high-level scanner for
     *   the "library" variable, which receives the fully-expanded name of
     *   the referenced library file. 
     */
    virtual void scan_full_library(const char *val, const char *fname) { }

    /*
     *   Scan a full resource filename - this is the high-level scanner for
     *   the "resource" variable, which receives the fully-expanded name of
     *   the referenced resource file. 
     */
    virtual void scan_full_resource(const char *val, const char *fname) { }

    /*
     *   Scan a "needmacro" definition.  This is the high-level scanner that
     *   subclasses will normally override.  By default, we'll simply show a
     *   suitable warning message.  Note that the macro name is not
     *   necessarily null-terminated (macro_len gives the number of bytes in
     *   the name), but the warning text is always null-terminated.  
     */
    virtual void scan_parsed_needmacro(const char *macro_name,
                                       size_t macro_len,
                                       const char *warning_text);

    /* 
     *   Scan a "source" variable - this gives the name of a file that the
     *   library includes.  This variable can appear multiple times. 
     *   
     *   This is the low-level scanner for the "source" variable.  Our
     *   default implementation builds the full filename by combining the
     *   path to the library itself with the value of this variable, then
     *   calls scan_full_source().  Normally, subclasses will not override
     *   this routine, but will override scan_full_source() instead.  
     */
    virtual void scan_source(const char *val);

    /*
     *   Scan a "library" variable - this gives the name of a sub-library
     *   that the library includes.  This variable can appear multiple times.
     *   
     *   This is the low-level scanner for the "library" variable.  Our
     *   default implementation builds the full filename by combining the
     *   path to the enclosing library with the name of the sublibrary file,
     *   then calls scan_full_library().  Normally, subclasses will override
     *   scan_full_library() instead of this routine.  
     */
    virtual void scan_library(const char *val);

    /*
     *   Scan a "resource" variable - this gives the name of a resource or
     *   resource directory to bundle into the build.  This variable can
     *   appear multiple times.
     *   
     *   This is the low-level scanner for the "resource" variable.  Our
     *   default implementation builds the full filename by combining the
     *   path to the enclosing library with the name of the resource file or
     *   directory, then calls scan_full_resource().  Normally, subclasses
     *   will override scan_full_resource() instead of this routine. 
     */
    virtual void scan_resource(const char *val);

    /*
     *   Scan a "needmacro" variable - this gives the name of a required
     *   macro symbol, and a warning message to display if the symbol isn't
     *   defined.
     *   
     *   This is the low-level scanner for this definition.  Our default
     *   implementation parses out the macro name (which is the first token,
     *   where whitespace is the token separator) and the warning message
     *   (which is the literal text of the rest of the line after the token
     *   separator), then calls scan_parsed_needmacro().  In most cases,
     *   subclasses will override scan_parsed_needmacro() instead of this
     *   routine.  
     */
    virtual void scan_needmacro(const char *val);

    /*
     *   Look up a preprocessor symbol by name.  Copy the value to the given
     *   result buffer, and return the length of the value (not including any
     *   null termination).  Null termination is NOT required on the result.
     *   If the value is too long for the result buffer, then DO NOT copy the
     *   result into the buffer, but simply return the required length; the
     *   caller will realize that nothing's been copied when it sees the
     *   overflowing length.
     *   
     *   Note that the symbol name string that 'sym_name' points to might not
     *   be null-terminated, as its length in bytes is given by 'sym_len'.
     *   
     *   By default, we return failure, since we don't provide any symbol
     *   management generically.  Subclasses should override if they provide
     *   symbols.  
     */
    virtual int get_pp_symbol(char *dst, size_t dst_len,
                              const char *sym_name, size_t sym_len)
    {
        /* by default, we don't have any symbols defined */
        return -1;
    }

    /*
     *   scan a "nodef" flag - this indicates that the library is a
     *   replacement for the compiler's default library inclusions, and thus
     *   the compiler should exclude those files it would otherwise include
     *   by default 
     */
    virtual void scan_nodef()
    {
        /* do nothing by default */
    }

    /* our library name */
    char *lib_name_;

    /* the path to our library */
    char *lib_path_;

    /* error count */
    int err_cnt_;

    /* current line number in source file */
    unsigned long linenum_;
};

#endif /* TCLIBPRS_H */

