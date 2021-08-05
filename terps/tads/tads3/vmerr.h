/* $Header: d:/cvsroot/tads/tads3/vmerr.h,v 1.3 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmerr.h - VM exception handling
Function
  Defines error macros for try/throw exception handling.

  To throw an exception, use err_throw(exception_object), where the
  exception_object is an object describing the exception.  This object
  must be allocated with 'new'.  Control will immediately transfer to
  the nearest enclosing err_catch() block.

  Protected code is defined as shown below.  The blocks must occur
  in the order shown, but the err_catch and err_finally blocks are
  optional (although it would be pointless to have an err_try that
  omits both).

    err_try
    {
        // protected code
    }
    err_catch(exception_variable) // or just err_catch_disc
    {
        // Exception handler - this code is executed only if an
        // exception occurs in the protected block.
        //
        // If an exception is thrown here (with no nested exception
        // handler to catch it), the err_finally block will be executed
        // and then the exception will be thrown to the enclosing block.
    }
    err_finally
    {
        // Code that is executed regardless whether exception occurs
        // or not.  If no exception occurs, this code is executed as
        // soon as the protected block finishes.  If an exception
        // occurs, this code is executed, then the exception is
        // re-thrown.
        //
        // If an exception is thrown here, the finally block will
        // be aborted and the enclosing error handler will be activated.
        // Care should be taken to ensure that code within this block
        // is properly protected against exceptions if necessary.
    }
    err_end;

  err_catch automatically defines the given exception_variable as
  a local variable of type (CVmException *) in the scope of the
  err_catch block.  The referenced CVmException object is valid
  *only* within the err_catch block; after the err_catch block exits,
  the CVmException object will no longer be present.

  If you don't need to access the CVmException information, use
  err_catch_disc instead of err_catch - this has the same effect as
  err_catch, but does not declare a local variable to point to the
  exception object.

  To re-throw the exception being handled from an err_catch() block,
  use err_rethrow().

  IMPORTANT: control must NOT be transferred out of an err_try, err_catch,
  or err_finally block via break, goto, or return.  Actual execution of
  the err_end is required in order to properly unwind the error stack.
  The easiest way to leave one of these blocks prematurely, if necessary,
  is with a goto:

     err_try
     {
        // some code

        if (done_with_this_block)
            goto done;

        // more code

     done: ;
     }
     err_catch_disc
     {
         // etc...
     }
     err_end;
     
Notes
  
Modified
  10/20/98 MJRoberts  - Creation
*/

#ifndef VMERR_H
#define VMERR_H

#include <setjmp.h>
#include <stdarg.h>

#include "t3std.h"
#include "vmerrnum.h"



/* ------------------------------------------------------------------------ */
/*
 *   Error Message Definition structure 
 */
struct err_msg_t
{
    /* message number */
    int msgnum;

    /* concise message text */
    const char *short_msgtxt;

    /* verbose message text */
    const char *long_msgtxt;

#ifdef VMERR_BOOK_MSG
    const char *book_msgtxt;
#endif
};

/* VM error message array */
extern const err_msg_t *vm_messages;
extern size_t vm_message_count;

/* 
 *   VM error message array - English version.  This version of the
 *   messages is linked directly into the VM; at run time, we can attempt
 *   to replace this version with another language version obtained from
 *   an external file.  We link in the English version so that we will
 *   always have a valid set of messages even if the user doesn't have a
 *   message file installed.  
 */
extern const err_msg_t vm_messages_english[];
extern size_t vm_message_count_english;

/* external message file signature */
#define VM_MESSAGE_FILE_SIGNATURE "TADS3.Message.0001\n\r\032"


/*
 *   load an external message file - returns zero on success, non-zero on
 *   failure 
 */
int err_load_message_file(osfildef *fp,
                          const err_msg_t **arr, size_t *arr_size,
                          const err_msg_t *default_arr,
                          size_t default_arr_size);

/* load default message file */
#define err_load_vm_message_file(fp) \
    err_load_message_file((fp), &vm_messages, &vm_message_count, \
                          vm_messages_english, vm_message_count_english)

/* 
 *   check to see if an external message file has been loaded for the
 *   default VM message set 
 */
int err_is_message_file_loaded();

/* 
 *   delete messages previously loaded with err_load_message_file (this is
 *   called automatically by err_terminate, so clients generally will not
 *   need to call this directly) 
 */
void err_delete_message_array(const err_msg_t **arr, size_t *arr_size,
                              const err_msg_t *default_arr,
                              size_t default_arr_size);


/*
 *   Search an array of messages for a given message number.  The array
 *   must be sorted by message ID.  
 */
const char *err_get_msg(const err_msg_t *msg_array, size_t msg_count,
                        int msgnum, int verbose);

/*
 *   Format a message with the parameters contained in an exception object.
 *   Returns the size in bytes of the formatted message.  If the output
 *   buffer is null or too small, we'll fill it up as much as possible and
 *   return the actual length required for the full message text.
 *   
 *   Suports the following format codes:
 *   
 *   %s - String.  Formats an ERR_TYPE_CHAR, ERR_TYPE_TEXTCHAR, or
 *   ERR_TYPE_TEXTCHAR_LEN value.
 *   
 *   %d, %u, %x - signed/unsigned decimal integer, hexadecimal integer.
 *   Formats an ERR_TYPE_INT value or an ERR_TYPE_ULONG value.  Automatically
 *   uses the correct size for the argument.
 *   
 *   %% - Formats as a single percent sign.  
 */
size_t err_format_msg(char *outbuf, size_t outbuflen,
                      const char *msg, const struct CVmException *exc);

/* 
 *   Format a message, allocating a buffer to store the result.  The caller
 *   must free the buffer with t3free(). 
 */
char *err_format_msg(const char *msg, const struct CVmException *exc);

/* ------------------------------------------------------------------------ */
/* 
 *   exception ID - this identifies an error 
 */
typedef uint err_id_t;

/*
 *   Error parameter type codes 
 */
enum err_param_type
{
    /* ---------------- Display Parameters ---------------- */
    /*
     *   The following parameter types are for display purposes.  These are
     *   substituted into the message string in err_format_msg().  
     */

    /* parameter is a native 'int' value */
    ERR_TYPE_INT,

    /* parameter is a native 'unsigned long' value */
    ERR_TYPE_ULONG,

    /* parameter is a 'textchar_t *' value (null terminated) */
    ERR_TYPE_TEXTCHAR,

    /* parameter is a 'char *' value (null terminated) */
    ERR_TYPE_CHAR,

    /* 
     *   parameter is a 'textchar_t *' value followed by a 'size_t' value
     *   giving the number of bytes in the string 
     */
    ERR_TYPE_TEXTCHAR_LEN,

    /* parameter is a 'char *' value with a separate length */
    ERR_TYPE_CHAR_LEN,

    /* ---------------- Non-Display Parameters ---------------- */
    /*
     *   The following parameter types are stored in the exception but are
     *   NOT used in formatting the error message. 
     */

    /* 
     *   a char* value giving the ID/Version string for the metaclass
     *   involved in the error (for example, "metaclass missing" or
     *   "metaclass version not available") 
     */
    ERR_TYPE_METACLASS,

    /* a char* value the ID/Version string for the function set involved */
    ERR_TYPE_FUNCSET,

    /* 
     *   void (no parameter): this is a flag indicating that the error is a
     *   VM version error.  This type of error can usually be solved by
     *   updating the interpreter to the latest version.  
     */
    ERR_TYPE_VERSION_FLAG
};

/*
 *   Exception parameter 
 */
struct CVmExcParam
{
    /* type of this parameter */
    err_param_type type_;

    /* value of the parameter */
    union
    {
        /* as an integer */
        int intval_;

        /* as an unsigned long */
        unsigned long ulong_;

        /* as a text string */
        const textchar_t *strval_;

        /* as a char string */
        const char *charval_;

        /* as char string with separate length counter */
        struct
        {
            const char *str_;
            size_t len_;
        } charlenval_;
    } val_;
};

/*
 *   Exception object 
 */
struct CVmException
{
    /* get the error code */
    int get_error_code() const { return error_code_; }

    /* get the number of parameters */
    int get_param_count() const { return param_count_; }

    /* get the type of the nth parameter */
    err_param_type get_param_type(int n) const { return params_[n].type_; }

    /* get the nth parameter as an integer */
    int get_param_int(int n) const { return params_[n].val_.intval_; }

    /* get the nth parameter as an unsigned long */
    unsigned long get_param_ulong(int n) const
        { return params_[n].val_.ulong_; }

    /* get the nth parameter as a string */
    const textchar_t *get_param_text(int n) const
        { return params_[n].val_.strval_; }

    /* get the nth parameter as a char string */
    const char *get_param_char(int n) const
        { return params_[n].val_.charval_; }

    /* get the nth parameter as a counted-length string */
    const char *get_param_char_len(int n, size_t *len) const
    {
        /* set the length return */
        *len = params_[n].val_.charlenval_.len_;

        /* return the string pointer */
        return params_[n].val_.charlenval_.str_;
    }


    /* set a parameter - null-terminated string value */
    void set_param_str(int n, const char *str)
    {
        params_[n].type_ = ERR_TYPE_CHAR;
        params_[n].val_.charval_ = str;
    }

    /* set a parameter - counted-length string value */
    void set_param_str(int n, const char *str, size_t len)
    {
        params_[n].type_ = ERR_TYPE_CHAR_LEN;
        params_[n].val_.charlenval_.str_ = str;
        params_[n].val_.charlenval_.len_ = len;
    }

    /* set a parameter - integer value */
    void set_param_int(int n, int val)
    {
        params_[n].type_ = ERR_TYPE_INT;
        params_[n].val_.intval_ = val;
    }
    
    /* the error code */
    err_id_t error_code_;

    /* 
     *   Is this a version-related error?  If this is true, the error is due
     *   to an out-of-date interpreter, and can usually be solved by
     *   upgrading to the latest version. 
     */
    int version_flag_;

    /* 
     *   Some version-related errors are due to component versions, namely
     *   metaclasses or function sets.  For errors due to component versions,
     *   we set the version flag AND set the appropriate identifier here to
     *   indicate the required component version.  (This is the component
     *   version that's set as a dependency in the byte-code program we're
     *   trying to run.)  
     */
    const char *metaclass_;
    const char *funcset_;

    /* number of parameters stored in the exception */
    int param_count_;

    /* parameters (actual array size is given by param_cnt_) */
    CVmExcParam params_[1];
};


/* ------------------------------------------------------------------------ */
/* 
 *   Error states.  The states are bit flags.  The following combinations are
 *   possible:
 *   
 *   ERR_STATE_TRYING - the initial call to setjmp() in the err_try has just
 *   returned
 *   
 *   ERR_STATE_EXCEPTION - we've just longjmp()'d into the frame from a
 *   throw, and we haven't handled the exception yet
 *   
 *   ERR_STATE_EXCEPTION | ERR_STATE_FINALLY - we've longjmp()'d into the
 *   frame from a throw, and we've entered the err_finally without
 *   encountering an err_catch
 *   
 *   ERR_STATE_CAUGHT - we've longjmp()'d into the frame from a throw, and
 *   we've entered the err_catch clause
 *   
 *   ERR_STATE_CAUGHT | ERR_FINALLY - we've longjmp()'d into the frame from a
 *   throw, gone through the err_catch, and entered the err_finally
 *   
 *   ERR_STATE_RETHROWN - a new throw occurred within our err_catch or
 *   err_finally clause
 *   
 *   ERR_STATE_RETHROWN | ERR_STATE_FINALLY - a new error occurred within our
 *   err_catch or err_finally clause, and we've entered the err_finally
 */
typedef int err_state_t;

/* 
 *   Trying - initial state, before any exception has been thrown.  Note that
 *   this must have value 0, because we initialize to this state from the
 *   setjmp return going into the error frame, and setjmp always returns 0 on
 *   the initial call.
 */
const int ERR_STATE_TRYING = 0;
    
/* exception in progress, and has not been caught */
const int ERR_STATE_EXCEPTION = 0x0001;
    
/* exception has been caught */
const int ERR_STATE_CAUGHT = 0x0002;

/* error thrown while an err_catch or err_finally is in progress */
const int ERR_STATE_RETHROWN = 0x4000;

/*
 *   'finally' block entered.  This is OR'd into the other states when we
 *   first enter the 'finally', so that we'll know not to re-enter the block
 *   if another exception occurs within.
 */
const int ERR_STATE_FINALLY = 0x8000;


/* ------------------------------------------------------------------------ */
/* 
 *   Error frame - allocated by err_try.  This object is not manipulated
 *   directly by the client; this is handled automatically by the error
 *   macros. 
 */
struct err_frame_t
{
    /* current state */
    err_state_t state_;

    /* enclosing error frame */
    err_frame_t *prv_;

    /* exception being handled in this frame */
    CVmException *exc_;

    /* jmpbuf for this handler */
    jmp_buf jmpbuf_;
};

/* 
 *   The current error frame active in this thread.  Each time we enter an
 *   err_try, we set up a new frame on the stack and set this thread-global
 *   to point to the new frame, and we store the old value (the enclosing
 *   frame) within the new frame, forming a stack via a linked list of
 *   frames.  When we leave the stack at err_end or via a throw, we pop the
 *   frame stack by setting G_err_frame to the enclosing element.
 */
extern OS_DECL_TLS(err_frame_t *, G_err_frame);

/*
 *   Initialize the global error context.  Should be called at program
 *   initialization.  'param_stack_size' is no longer used.
 */
void err_init(size_t param_stack_size);

/*
 *   Delete the global error context.  Should be called at program
 *   termination. 
 */
void err_terminate();

/*
 *   Throw an exception.  The first form takes no parameters except the
 *   error code; the second form takes a parameter count followed by that
 *   number of parameters.  Each parameter requires two arguments: the
 *   first is a type code of type err_param_type, and the second the
 *   value, whose interpretation depends on the type code.  
 */
void err_throw(err_id_t error_code);
void err_throw_a(err_id_t error_code, int param_count, ...);

/*
 *   Rethrow the current exception.  This is valid only in 'catch' blocks.
 */
void err_rethrow();

/* 
 *   Get the current exception being handled in the nearest enclosing
 *   err_catch block.  This searches the error frame stack for a frame in the
 *   'caught' state, and returns the exception object from that frame.  This
 *   allows retrieving the current exception being handled even when we enter
 *   a new err_try block within an err_catch handler.
 */
CVmException *err_get_cur_exc();

/*
 *   Fatal error - abort program 
 */
void err_abort(const char *message);


#define err_try \
    { \
        err_frame_t err_cur__; \
        err_cur__.prv_ = os_tls_get(err_frame_t *, G_err_frame); \
        os_tls_set(G_err_frame, &err_cur__); \
        if ((err_cur__.state_ = \
            (err_state_t)setjmp(err_cur__.jmpbuf_)) == ERR_STATE_TRYING) \
        { \

#define err_catch(exc) \
        } \
        if (err_cur__.state_ == ERR_STATE_EXCEPTION) \
        { \
            CVmException *exc; \
            exc = err_cur__.exc_; \
            err_cur__.state_ = ERR_STATE_CAUGHT;

#define err_catch_disc \
        } \
        if (err_cur__.state_ == ERR_STATE_EXCEPTION) \
        { \
           err_cur__.state_ = ERR_STATE_CAUGHT;


#define err_finally \
        } \
        if ((err_cur__.state_ & ERR_STATE_FINALLY) == 0) \
        { \
            err_cur__.state_ |= ERR_STATE_FINALLY;

#define err_end \
        } \
        os_tls_set(G_err_frame, err_cur__.prv_); \
        if (err_cur__.state_ & (ERR_STATE_EXCEPTION | ERR_STATE_RETHROWN)) \
        { \
            if (os_tls_get(err_frame_t *, G_err_frame)->state_ \
                & ERR_STATE_CAUGHT) \
                t3free(os_tls_get(err_frame_t *, G_err_frame)->exc_); \
            os_tls_get(err_frame_t *, G_err_frame)->exc_ = err_cur__.exc_; \
            err_rethrow(); \
        } \
        if (err_cur__.state_ & ERR_STATE_CAUGHT) \
            t3free(err_cur__.exc_); \
    }

#endif /* VMERR_H */

