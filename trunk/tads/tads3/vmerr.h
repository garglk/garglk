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
 *   err_throw() Return Handling
 *   
 *   Some compilers (such as MSVC 2007) are capable of doing global
 *   optimizations that can detect functions that never return.  err_throw()
 *   is one such function: it uses longjmp() to jump out, so it never returns
 *   to its caller.
 *   
 *   Most compilers can't detect this automatically, and C++ doesn't have a
 *   standard way to declare a function that never returns.  So on most
 *   compilers, the compiler will assume that err_throw() returns, and thus
 *   will generate a warning if err_throw() isn't followed by some proper
 *   control flow statement.  For example, in a function with a return value,
 *   a code branch containing an err_throw() would still need a 'return
 *   <val>' statement - without such a statement, the compiler would generate
 *   an error about a branch without a value return.
 *   
 *   This creates a porting dilemma.  On compilers that can detect that
 *   err_throw() never returns, the presence of any statement in a code
 *   branch after an err_throw() will cause an "unreachable code" error.  For
 *   all other compilers, the *absence* of such code will often cause a
 *   different error ("missing return", etc).
 *   
 *   The only way I can see to deal with this is to use a compile-time
 *   #define to select which type of compiler we're using, and use this to
 *   insert or delete the proper dummy control flow statement after an
 *   err_throw().  So:
 *   
 *   --- INSTRUCTIONS TO BASE CODE DEVELOPERS ---
 *   
 *   - after each err_throw() call, if the code branch needs some kind of
 *   explicit termination (such as a "return val;" statement), code it with
 *   the AFTER_ERR_THROW() macro.  Since err_throw() never *actually*
 *   returns, these will be dummy statements that will never be reached, but
 *   the compiler might require their presence anyway because it doesn't know
 *   better.
 *   
 *   --- INSTRUCTIONS TO PORTERS ---
 *   
 *   - if your compiler CAN detect that err_throw() never returns, define
 *   COMPILER_DETECTS_THROW_NORETURN in your compiler command-line options;
 *   
 *   - otherwise, leave the symbol undefined.  
 */
#ifdef COMPILER_DETECTS_THROW_NORETURN
#define AFTER_ERR_THROW(dummy)
#else
#define AFTER_ERR_THROW(dummy)   dummy
#endif


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
 *   Format a message with the parameters contained in an exception
 *   object.  Suports the following format codes:
 *   
 *   %s - String.  Formats an ERR_TYPE_CHAR, ERR_TYPE_TEXTCHAR, or
 *   ERR_TYPE_TEXTCHAR_LEN value.
 *   
 *   %d, %u, %x - signed/unsigned decimal integer, hexadecimal integer.
 *   Formats an ERR_TYPE_INT value or an ERR_TYPE_ULONG value.
 *   Automatically uses the correct size for the argument.
 *   
 *   %% - Formats as a single percent sign.  
 */
void err_format_msg(char *outbuf, size_t outbuflen,
                    const char *msg, const struct CVmException *exc);

/* 
 *   exception ID - this identifies an error 
 */
typedef uint err_id_t;

/*
 *   Error parameter type codes 
 */
enum err_param_type
{
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
    ERR_TYPE_CHAR_LEN
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
    
    /* previous exception in the exception stack */
    CVmException *prv_;
    
    /* the error code */
    err_id_t error_code_;

    /* number of parameters stored in the exception */
    int param_count_;

    /* parameters (actual array size is given by param_cnt_) */
    CVmExcParam params_[1];
};


/* error states */
enum err_state_t
{
    /* no exception */
    ERR_STATE_OKAY = 0,

    /* trying */
    ERR_STATE_TRYING = 1,
    
    /* exception in progress, and has not been caught */
    ERR_STATE_EXCEPTION = 2,
    
    /* exception has been caught */
    ERR_STATE_CAUGHT = 3,

    /* error thrown while exception handler in progress */
    ERR_STATE_RETHROWN = 4
};

/* 
 *   Error frame item - allocated by err_try.  This object is not
 *   manipulated directly by the client; this is handled automatically by
 *   the error macros. 
 */
struct err_frame_t
{
    /* enclosing error frame */
    err_frame_t *prv_;

    /* jmpbuf for this handler */
    jmp_buf      jmpbuf_;

    /* current state */
    err_state_t  state_;

    /* 
     *   Flag: processing the 'finally' clause.  If an exception is thrown
     *   while we're processing this, we will not process the 'finally'
     *   clause again, nor will we execute the 'catch' clause. 
     */
    int          in_finally_;
};

/*
 *   Global error context structure
 */
struct err_context_t
{
    /* current active error frame */
    err_frame_t *cur_frame_;

    /* current exception in the exception stack */
    CVmException *cur_exc_;

    /* next free byte of parameter stack */
    char *param_free_;

    /* size of parameter stack */
    size_t param_stack_size_;

    /* parameter stack - actual size given by param_stack_size */
    char param_stack_[1];
};

/* 
 *   global static error context 
 */
extern err_context_t *G_err;

/* reference count for error context */
extern int G_err_refs;

/*
 *   Initialize the global error context.  Should be called at program
 *   initialization.  'param_stack_size' is the size in bytes of the error
 *   parameter stack; this space is used to store all error parameters in
 *   err_throw_a() calls.  Generally, only one or two errors are active at
 *   a given time, so it should be safe to make this three or four times
 *   sizeof(CVmException) plus sizeof(CVmExcParam) plus the total
 *   parameter sizes of the largest parameterized exception codes; if
 *   still in doubt, one or two kbytes should be adequate in any case.  
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

/* pop the top exception from the exception stack */
void err_pop_exc();

/*
 *   Serious error - abort program 
 */
void err_abort(const char *message);

/* 
 *   determine the error stack depth 
 */
int err_stack_depth();

#define err_try \
    { \
        err_frame_t err_cur__; \
        err_cur__.prv_ = G_err->cur_frame_; \
        err_cur__.in_finally_ = FALSE; \
        G_err->cur_frame_ = &err_cur__; \
        if ((err_cur__.state_ = \
            (err_state_t)setjmp(err_cur__.jmpbuf_)) == 0) \
        { \
            err_cur__.state_ = ERR_STATE_TRYING;

#define err_catch(exc) \
        } \
        if (!err_cur__.in_finally_ && \
            err_cur__.state_ == ERR_STATE_EXCEPTION) \
        { \
            CVmException *exc; \
            exc = G_err->cur_exc_; \
            err_cur__.state_ = ERR_STATE_CAUGHT;

#define err_catch_disc \
        } \
        if (!err_cur__.in_finally_ && \
            err_cur__.state_ == ERR_STATE_EXCEPTION) \
        { \
           err_cur__.state_ = ERR_STATE_CAUGHT;


#define err_finally \
        } \
        if (!err_cur__.in_finally_) \
        { \
            err_cur__.in_finally_ = TRUE;

#define err_end \
        } \
        G_err->cur_frame_ = err_cur__.prv_; \
        if (err_cur__.state_ == ERR_STATE_EXCEPTION \
            || err_cur__.state_ == ERR_STATE_RETHROWN) \
            err_rethrow(); \
        if (err_cur__.state_ == ERR_STATE_CAUGHT) \
            err_pop_exc(); \
    }

#endif /* VMERR_H */

