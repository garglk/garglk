#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/vmerr.cpp,v 1.4 1999/07/11 00:46:59 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmerr.cpp - VM error handling
Function
  This module contains global variables required for the error handler.
Notes
  
Modified
  10/20/98 MJRoberts  - Creation
*/

#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmerr.h"


/*
 *   Global error context pointer, and reference count for the error
 *   subsystem 
 */
err_context_t *G_err = 0;
int G_err_refs = 0;

/*
 *   Initialize the global error context 
 */
void err_init(size_t param_stack_size)
{
    /* count the reference */
    ++G_err_refs;
    
    /* ignore this if we're already initialized */
    if (G_err_refs > 1)
        return;
    
    /* 
     *   allocate the error context, adding in room for the parameter
     *   stack 
     */
    G_err = (err_context_t *)t3malloc(sizeof(err_context_t)
                                      + param_stack_size);

    /* initialize the parameter stack pointers */
    G_err->param_free_ = G_err->param_stack_;
    G_err->param_stack_size_ = param_stack_size;
    G_err->cur_exc_ = 0;

    /* we have no active frame yet */
    G_err->cur_frame_ = 0;
}

/*
 *   Delete the global error context 
 */
void err_terminate()
{
    /* decrease the error system reference counter */
    --G_err_refs;

    /* if that leaves no references, delete the error context */
    if (G_err_refs == 0)
    {
        /* free the global error structure */
        t3free(G_err);
        G_err = 0;

        /* delete external messages, if we loaded them */
        err_delete_message_array(&vm_messages, &vm_message_count,
                                 vm_messages_english,
                                 vm_message_count_english);
    }
}

/*
 *   Throw the error currently on the stack 
 */
static void err_throw_current()
{
    err_state_t new_state;

    /*
     *   Figure out what state the enclosing frame will be in after this
     *   jump.  If we're currently in a 'trying' block, we'll now be in an
     *   'exception' state.  Otherwise, we must have been in a 'catch' or
     *   'finally' already - in these cases, the new state is 'rethrown',
     *   because we have an exception within an exception handler.  
     */
    if (G_err->cur_frame_->state_ == ERR_STATE_TRYING)
        new_state = ERR_STATE_EXCEPTION;
    else
        new_state = ERR_STATE_RETHROWN;

    /* jump to the enclosing frame's exception handler */
    longjmp(G_err->cur_frame_->jmpbuf_, new_state);
}

/*
 *   Throw an exception 
 */
void err_throw(err_id_t error_code)
{
    /* throw the error, with no parameters */
    err_throw_a(error_code, 0);
}

/*
 *   Allocate space in the exception parameter stack. 
 */
static void *err_stack_alloc(size_t siz)
{
    void *ret;
    
    /* round the size up to the OS allocation alignment boundary */
    siz = osrndsz(siz);

    /* if we don't have room in the parameter stack, it's a fatal error */
    if ((G_err->param_stack_size_
         - (G_err->param_free_ - G_err->param_stack_)) < siz)
        err_abort("no space for exception parameters");

    /* 
     *   get the current free pointer - this is where we'll allocate the
     *   requested space 
     */
    ret = G_err->param_free_;

    /* advance the free pointer to consume the requested space */
    G_err->param_free_ += siz;

    /* return a poiner to the allocated space */
    return ret;
}

/*
 *   Store an exception parameter string.  Allocates space for the string in
 *   the exception parameter stack, stores a null-terminated copy, and
 *   returns a pointer to the copy of the string. 
 */
static char *err_store_param_str(const char *str, size_t len)
{
    char *new_str;
    
    /* allocate space for the string and a null terminator */
    new_str = (char *)err_stack_alloc(len + 1);

    /* store a copy of the string */
    memcpy(new_str, str, len);

    /* null-terminate the copy */
    new_str[len] = '\0';

    /* return a pointer to the newly-allocated copy */
    return new_str;
}

/*
 *   Throw an exception with parameters in va_list format
 */
static void err_throw_v(err_id_t error_code, int param_count, va_list va)
{
    size_t siz;
    int i;
    CVmException *exc;
    CVmExcParam *param;

    /*
     *   Assert that the size of the err_param_type enum is no larger than
     *   the size of the native 'int' type.  Since this routine is called
     *   with a varargs list, and since ANSI requires compilers to promote
     *   any enum type smaller than int to int when passing to a varargs
     *   function, we must retrieve our err_param_type arguments (via the
     *   va_arg() macro) with type 'int' rather than type 'enum
     *   err_param_type'.
     *   
     *   The promotion to int is mandatory, so retrieving our values as
     *   'int' values is the correct, portable thing to do; the only
     *   potential problem is that our enum type could be defined to be
     *   *larger* than int, in which case the compiler would pass it to us
     *   as the larger type, not int, and our retrieval would be incorrect.
     *   The only way a compiler would be allowed to do this is if our enum
     *   type actually required a type larger than unsigned int (in other
     *   words, we had enum values higher than the largest unsigned int
     *   value for the local platform).  This is extremely unlikely, but
     *   we'll assert our assumption here to ensure that the problem is
     *   readily apparent should it ever occur.  
     */
    assert(sizeof(err_param_type) <= sizeof(int));

    /*
     *   If the current stack frame is already handling an error, pop the
     *   current error, since the current error will no longer be
     *   accessible after this throw.  
     */
    if (G_err->cur_frame_->state_ == ERR_STATE_EXCEPTION
        || G_err->cur_frame_->state_ == ERR_STATE_CAUGHT
        || G_err->cur_frame_->state_ == ERR_STATE_RETHROWN)
        err_pop_exc();

    /* 
     *   Figure out how much space we need.  Start with the size of the base
     *   CVmException class, since we always need a CVmException structure.
     *   This structure has space for one argument descriptor built in, so
     *   subtract that off from our base size, since we'll add in space for
     *   the argument descriptors separately.  
     */
    siz = sizeof(CVmException) - sizeof(CVmExcParam);

    /* 
     *   Add in space the parameter descriptors.  We need one CVmExcParam
     *   structure to describe each parameter.  
     */
    siz += param_count * sizeof(CVmExcParam);

    /* 
     *   allocate space for the base exception structure in the exception
     *   parameter stack 
     */
    exc = (CVmException *)err_stack_alloc(siz);

    /* fill in our base structure */
    exc->error_code_ = error_code;
    exc->param_count_ = param_count;

    /* 
     *   link to the previous exception on the stack, so that we can pop
     *   this exception when we're done with it 
     */
    exc->prv_ = G_err->cur_exc_;

    /* this is now the current exception */
    G_err->cur_exc_ = exc;
    
    /* 
     *   We now have our base exception structure, with enough space for the
     *   parameter descriptors, but we still need to store the parameter
     *   values themselves.
     */
    for (i = 0, param = exc->params_ ; i < param_count ; ++i, ++param)
    {
        err_param_type typ;
        const char *sptr;
        size_t slen;
        
        /* get the type indicator, and store it in the descriptor */
        typ = (err_param_type)va_arg(va, int);
        
        /* store the type */
        param->type_ = typ;

        /* store the argument's value */
        switch(typ)
        {
        case ERR_TYPE_INT:
            /* store the integer */
            param->val_.intval_ = va_arg(va, int);
            break;

        case ERR_TYPE_ULONG:
            /* store the unsigned long */
            param->val_.ulong_ = va_arg(va, unsigned long);
            break;

        case ERR_TYPE_TEXTCHAR:
            /* 
             *   It's a (textchar_t *) string, null-terminated.  Get the
             *   string pointer and calculate its length. 
             */
            sptr = va_arg(va, textchar_t *);
            slen = get_strlen(sptr);

            /* store it in parameter memory */
            param->val_.strval_ = err_store_param_str(sptr, slen);
            break;

        case ERR_TYPE_TEXTCHAR_LEN:
            /* 
             *   It's a (textchar_t *) string with an explicit length given
             *   as a separate size_t parameter. 
             */
            sptr = va_arg(va, textchar_t *);
            slen = va_arg(va, size_t);

            /* store it in parameter memory */
            param->val_.strval_ = err_store_param_str(sptr, slen);

            /* 
             *   change the type to a regular textchar string now, since
             *   we've converted the value to a null-terminated string 
             */
            param->type_ = ERR_TYPE_TEXTCHAR;
            break;

        case ERR_TYPE_CHAR:
            /* it's a (char *) string, null-terminated */
            sptr = va_arg(va, char *);
            slen = strlen(sptr);

            /* store it */
            param->val_.charval_ = err_store_param_str(sptr, slen);
            break;

        case ERR_TYPE_CHAR_LEN:
            /* it's a (char *) string with an explicit size_t size */
            sptr = va_arg(va, char *);
            slen = va_arg(va, size_t);

            /* store it */
            param->val_.charval_ = err_store_param_str(sptr, slen);
            
            /* skip the string */
            va_arg(va, char *);

            /* 
             *   change the type to a regular char string, since we've added
             *   null termination 
             */
            param->type_ = ERR_TYPE_CHAR;
            break;
        }
    }

    /* throw the error that we just pushed */
    err_throw_current();
}

#ifdef MICROSOFT
/*
 *   Microsoft Visual C++ optimizer workaround - not applicable to other
 *   systems.
 *   
 *   For MSVC, we need to turn off warning 4702 ("unreachable code").  MSVC
 *   is too clever by half for our implementation here of err_throw_a(), and
 *   the only recourse (empirically) seems to be to turn off this warning for
 *   the duration of this function.
 *   
 *   The issue is that err_throw_a() makes a call to err_throw_v() within a
 *   va_start()...va_end() pair.  The MSVC optimizer recognizes that
 *   err_throw_v() never returns, so it marks any code following a call to
 *   same as unreachable.  But we *have to* include the va_end() call after
 *   the err_throw_v() call.  The va_end() is *required* for portability -
 *   some compilers expand va_end() to structural C code (for example, to
 *   insert a close brace to balance an open brace inserted by va_start()).
 *   So we can't omit the va_end() call regardless of its run-time
 *   reachability.
 *   
 *   So, we have code that's both unreachable and required.  The only
 *   apparent solution is to disable the unreachable-code warning for the
 *   duration of this function.
 *   
 *   (Conceivably, we could trick the optimizer by moving the err_throw_a()
 *   implementation to a separate module; the optimizer probably couldn't
 *   track the does-not-return status of err_throw_v() across modules.  But
 *   that wouldn't be any cleaner in any sense - it would just trick the
 *   compiler in a different way.  Better to explicitly turn off the warning;
 *   at least that way the workaround is plain and explicit.)  
 */
#pragma warning(push)
#pragma warning(disable: 4702)
#endif

/*
 *   Throw an exception with parameters 
 */
void err_throw_a(err_id_t error_code, int param_count, ...)
{
    va_list marker;

    /* build the argument list and throw the error */
    va_start(marker, param_count);
    err_throw_v(error_code, param_count, marker);
    va_end(marker);
}

/* MSVC - restore previous warning state (see above) */
#ifdef MICROSOFT
#pragma warning(pop)
#endif

/*
 *   Re-throw the current exception.  This is valid only from 'catch'
 *   blocks.  
 */
void err_rethrow()
{
    /* throw the error currently on the stack */
    err_throw_current();
}

/*
 *   Pop the current exception from the exception stack. 
 */
void err_pop_exc()
{
    /* 
     *   if there's anything on the stack, remove it and replace it with
     *   the previous item on the stack 
     */
    if (G_err->cur_exc_ != 0)
    {
        /* 
         *   since we're removing this element, its space can now be
         *   re-used for the next exception allocation (since exceptions
         *   are always stacked, the space we use to consume them can be
         *   treated as a stack as well) 
         */
        G_err->param_free_ = (char *)G_err->cur_exc_;

        /* remove the top element of the stack */
        G_err->cur_exc_ = G_err->cur_exc_->prv_;
    }
    else
    {
        /* no errors are on the stack, so the parameter space is all free */
        G_err->param_free_ = G_err->param_stack_;
    }
}

/*
 *   Abort the program with a serious, unrecoverable error
 */
void err_abort(const char *message)
{
    printf("%s\n", message);
    exit(2);
}

/*
 *   Determine the current error stack depth 
 */
int err_stack_depth()
{
    int cnt;
    CVmException *exc;

    /* count exceptions in the stack */
    for (cnt = 0, exc = G_err->cur_exc_ ; exc != 0 ; exc = exc->prv_)
    {
        /* count this exception */
        ++cnt;
    }

    /* return the number of exceptions in the stack */
    return cnt;
}

/* ------------------------------------------------------------------------ */
/*
 *   Try loading a message file.  Returns zero on success, non-zero if an
 *   error occurred.  
 */
int err_load_message_file(osfildef *fp,
                          const err_msg_t **arr, size_t *arr_size,
                          const err_msg_t *default_arr,
                          size_t default_arr_size)
{
    char buf[128];
    size_t i;
    err_msg_t *msg;
    
    /* read the file signature */
    if (osfrb(fp, buf, sizeof(VM_MESSAGE_FILE_SIGNATURE))
        || memcmp(buf, VM_MESSAGE_FILE_SIGNATURE,
                  sizeof(VM_MESSAGE_FILE_SIGNATURE)) != 0)
        goto fail;

    /* delete any previously-loaded message array */
    err_delete_message_array(arr, arr_size, default_arr, default_arr_size);

    /* read the message count */
    if (osfrb(fp, buf, 2))
        goto fail;

    /* set the new message count */
    *arr_size = osrp2(buf);

    /* allocate the message array */
    *arr = (err_msg_t *)t3malloc(*arr_size * sizeof(err_msg_t));
    if (*arr == 0)
        goto fail;

    /* clear the memory */
    memset((err_msg_t *)*arr, 0, *arr_size * sizeof(err_msg_t));

    /* read the individual messages */
    for (i = 0, msg = (err_msg_t *)*arr ; i < *arr_size ; ++i, ++msg)
    {
        size_t len1, len2;
        
        /* read the current message ID and the length of the two messages */
        if (osfrb(fp, buf, 8))
            goto fail;

        /* set the message ID */
        msg->msgnum = (int)t3rp4u(buf);

        /* get the short and long mesage lengths */
        len1 = osrp2(buf + 4);
        len2 = osrp2(buf + 6);

        /* allocate buffers */
        msg->short_msgtxt = (char *)t3malloc(len1 + 1);
        msg->long_msgtxt = (char *)t3malloc(len2 + 1);

        /* if either one failed, give up */
        if (msg->short_msgtxt == 0 || msg->long_msgtxt == 0)
            goto fail;

        /* read the two messages */
        if (osfrb(fp, (char *)msg->short_msgtxt, len1)
            || osfrb(fp, (char *)msg->long_msgtxt, len2))
            goto fail;

        /* null-terminate the strings */
        *(char *)(msg->short_msgtxt + len1) = '\0';
        *(char *)(msg->long_msgtxt + len2) = '\0';
    }

    /* success */
    return 0;

fail:
    /* revert back to the built-in array */
    err_delete_message_array(arr, arr_size,
                             default_arr, default_arr_size);

    /* indicate failure */
    return 1;
}

/*
 *   Determine if an external message file has been loaded for the default
 *   VM message set 
 */
int err_is_message_file_loaded()
{
    /* 
     *   if we're not using the compiled-in message array, we must have
     *   loaded an external message set 
     */
    return vm_messages != &vm_messages_english[0];
}

/*
 *   Delete the message array, if one is loaded 
 */
void err_delete_message_array(const err_msg_t **arr, size_t *arr_size,
                              const err_msg_t *default_arr,
                              size_t default_arr_size)
{
    /* 
     *   If the message array is valid, and it's not set to point to the
     *   built-in array of English messages, we must have allocated it, so
     *   we must now free it.  We don't need to free it if it points to
     *   the English array, because that's static data linked into the VM
     *   executable.  
     */
    if (*arr != 0 && *arr != default_arr)
    {
        size_t i;
        err_msg_t *msg;
        
        /* delete each message in the array */
        for (i = 0, msg = (err_msg_t *)*arr ; i < *arr_size ; ++i, ++msg)
        {
            /* delete the strings for this entry */
            if (msg->short_msgtxt != 0)
                t3free((char *)msg->short_msgtxt);
            if (msg->long_msgtxt != 0)
                t3free((char *)msg->long_msgtxt);
        }

        /* delete the message array itself */
        t3free((err_msg_t *)*arr);
    }

    /* set the messages array back to the built-in english messages */
    *arr = default_arr;
    *arr_size = default_arr_size;
}

/* ------------------------------------------------------------------------ */
/*
 *   Find an error message 
 */
const char *err_get_msg(const err_msg_t *msg_array, size_t msg_count,
                        int msgnum, int verbose)
{
    int hi, lo, cur;

    /* perform a binary search of the message list */
    lo = 0;
    hi = msg_count - 1;
    while (lo <= hi)
    {
        /* split the difference */
        cur = lo + (hi - lo)/2;

        /* is it a match? */
        if (msg_array[cur].msgnum == msgnum)
        {
            /* it's the one - return the text */
            return (verbose
                    ? msg_array[cur].long_msgtxt
                    : msg_array[cur].short_msgtxt);
        }
        else if (msgnum > msg_array[cur].msgnum)
        {
            /* we need to go higher */
            lo = (cur == lo ? cur + 1 : cur);
        }
        else
        {
            /* we need to go lower */
            hi = (cur == hi ? cur - 1 : cur);
        }
    }

    /* no such message */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Format a message with the parameters contained in an exception object.
 *   Suports the following format codes:
 *   
 *   %s - String.  Formats an ERR_TYPE_CHAR or ERR_TYPE_TEXTCHAR value,
 *   including the counted-length versions.
 *   
 *   %d, %u, %x - signed/unsigned decimal integer, hexadecimal integer.
 *   Formats an ERR_TYPE_INT value or an ERR_TYPE_ULONG value.
 *   Automatically uses the correct size for the argument.
 *   
 *   %% - Formats as a single percent sign.  
 */
void err_format_msg(char *outbuf, size_t outbuflen,
                    const char *msg, const CVmException *exc)
{
    int curarg;
    const char *p;
    char *dst;
    int exc_argc;

    /* if there's no space at all, ignore the request */
    if (outbuf == 0 || outbuflen == 0)
        return;

    /* get the number of parameters in the exception object */
    exc_argc = (exc == 0 ? 0 : exc->get_param_count());

    /* start with the first parameter */
    curarg = 0;

    /* start at the beginning of the buffer */
    dst = outbuf;

    /* if there's no message, there's nothing to return */
    if (msg == 0)
    {
        *dst = '\0';
        return;
    }

    /* scan the format string for formatting codes */
    for (p = msg ; *p != '\0' ; ++p)
    {
        /* 
         *   If we're out of space, stop now.  Make sure we leave room for
         *   the terminating null byte. 
         */
        if (dst + 1 >= outbuf + outbuflen)
            break;
        
        /* if it's a format specifier, translate it */
        if (*p == '%')
        {
            const char *src;
            char srcbuf[30];
            err_param_type typ;
            size_t len;
            int use_strlen;
            
            /* 
             *   if no more parameters are available, ignore the
             *   formatting code entirely, and leave it in the string as
             *   it is 
             */
            if (curarg >= exc_argc)
            {
                *dst++ = *p;
                continue;
            }

            /* get the type of the current parameter */
            typ = exc->get_param_type(curarg);
            
            /* 
             *   presume we'll want to use strlen to get the length of the
             *   source value 
             */
            use_strlen = TRUE;
            
            /* skip the '%' and determine what follows */
            ++p;
            switch (*p)
            {
            case 's':
                /* get the string value using the appropriate type */
                if (typ == ERR_TYPE_TEXTCHAR)
                    src = exc->get_param_text(curarg);
                else if (typ == ERR_TYPE_CHAR)
                    src = exc->get_param_char(curarg);
                else if (typ == ERR_TYPE_CHAR_LEN)
                {
                    /* get the string value and its length */
                    src = exc->get_param_char_len(curarg, &len);

                    /* 
                     *   src isn't null terminated, so don't use strlen to
                     *   get its length - we already have it from the
                     *   parameter data 
                     */
                    use_strlen = FALSE;
                }
                else
                    src = "s";
                break;

            case 'd':
                src = srcbuf;
                if (typ == ERR_TYPE_INT)
                    sprintf(srcbuf, "%d", exc->get_param_int(curarg));
                else if (typ == ERR_TYPE_ULONG)
                    sprintf(srcbuf, "%ld", exc->get_param_ulong(curarg));
                else
                    src = "d";
                break;

            case 'u':
                src = srcbuf;
                if (typ == ERR_TYPE_INT)
                    sprintf(srcbuf, "%u", exc->get_param_int(curarg));
                else if (typ == ERR_TYPE_ULONG)
                    sprintf(srcbuf, "%lu", exc->get_param_ulong(curarg));
                else
                    src = "u";
                break;

            case 'x':
                src = srcbuf;
                if (typ == ERR_TYPE_INT)
                    sprintf(srcbuf, "%x", exc->get_param_int(curarg));
                else if (typ == ERR_TYPE_ULONG)
                    sprintf(srcbuf, "%lx", exc->get_param_ulong(curarg));
                else
                    src = "x";
                break;

            case '%':
                /* add a single percent sign */
                src = "%";
                break;

            default:
                /* invalid format character; leave the whole thing intact */
                src = srcbuf;
                srcbuf[0] = '%';
                srcbuf[1] = *p;
                srcbuf[2] = '\0';
                break;
            }

            /* get the length, if it's null-terminated */
            if (use_strlen)
                len = strlen(src);

            /* figure out how much of the value we can copy */
            if (len > outbuflen - (dst - outbuf) - 1)
                len = outbuflen - (dst - outbuf) - 1;

            /* copy the value and advance past it in the output buffer */
            memcpy(dst, src, len);
            dst += len;

            /* consume the argument */
            ++curarg;
        }
        else
        {
            /* just copy the current character as it is */
            *dst++ = *p;
        }
    }

    /* add the trailing null */
    *dst++ = '\0';
}

