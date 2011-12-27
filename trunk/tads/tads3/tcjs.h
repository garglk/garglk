/* Copyright (c) 2009 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tcjs.h - tads 3 compiler - javascript code generator
Function
  
Notes
  
Modified
  02/17/09 MJRoberts  - Creation
*/

#ifndef TCJS_H
#define TCJS_H

#include <assert.h>

/* include the Javascript code generator final parse node classes */
#include "tcjsdrv.h"

// $$$ deprecated - build scaffolding only
#include "vmop.h"


/* ------------------------------------------------------------------------ */
/*
 *   JS expression stack element 
 */
class js_expr_ele
{
public:
    js_expr_ele(class js_expr_buf &expr, js_expr_ele *tos);

    ~js_expr_ele()
    {
        t3free(buf);
    }

    const char *getbuf() const { return buf; }
    size_t getlen() const { return len; }

    js_expr_ele *getnxt() const { return nxt; }

protected:
    /* store a string */
    void init(const char *txt, size_t txtlen)
    {
        /* allocate our buffer */
        buf = (char *)t3malloc(txtlen + 1);
        if (buf == 0)
            err_throw(TCERR_CODEGEN_NO_MEM);

        /* copy the data */
        memcpy(buf, txt, txtlen);

        /* null-terminate it */
        buf[txtlen] = '\0';

        /* remember the length */
        len = txtlen;
    }

    /* take ownership of a buffer */
    void assume_buffer(char *buf, size_t len)
    {
        this->buf = buf;
        this->len = len;
    }

    /* our buffer */
    char *buf;

    /* length of our string */
    size_t len;

    /* next deeper stack element */
    js_expr_ele *nxt;
};


/*
 *   Javascript expression generation buffer 
 */
class js_expr_buf
{
public:
    js_expr_buf()
    {
        /* start with the initial buffer */
        buf = init_buf;
        alo = init_buf_size;

        /* there's nothing in the buffer yet */
        buf[0] = '\0';
        used = 0;
    }

    ~js_expr_buf()
    {
        /* if the buffer is allocated, free it */
        if (buf != init_buf)
            t3free(buf);
    }

    /* append data */
    void append(const char *txt, size_t len)
    {
        /* ensure we have enough space */
        if (used + len + 1 > alo)
        {
            /* expand the buffer to make room, and then some */
            size_t new_alo = used + len + alo/2;

            /* allocate a new buffer */
            char *new_buf = (char *)t3malloc(new_alo);

            /* throw an error on failure */
            if (new_buf == 0)
                err_throw(TCERR_CODEGEN_NO_MEM);

            /* copy the existing data (including the trailing null) */
            memcpy(new_buf, buf, used + 1);

            /* if we had an allocated buffer, delete it */
            if (buf != init_buf)
                t3free(buf);

            /* switch to the new buffer */
            buf = new_buf;
            alo = new_alo;
        }

        /* copy the text */
        memcpy(buf + used, txt, len);

        /* adjust the length counter */
        used += len;

        /* keep the buffer null-terminated */
        buf[used] = '\0';
    }

    void append(char c) { append(&c, 1); }
    void append(const char *txt) { append(txt, txt != 0 ? strlen(txt) : 0); }
    void append(const js_expr_buf *j) { append(j->buf, j->used); }

    /* some operator overloading for convenient notation */
    js_expr_buf *const operator +=(const char *txt)
    {
        append(txt);
        return this;
    }
    js_expr_buf *const operator += (char c)
    {
        append(&c, 1);
        return this;
    }
    js_expr_buf *const operator += (const js_expr_buf &j)
    {
        append(j.buf, j.used);
        return this;
    }
    js_expr_buf *const operator += (const js_expr_buf *j)
    {
        append(j->buf, j->used);
        return this;
    }
    js_expr_buf *const operator += (const js_expr_ele *ele)
    {
        append(ele->getbuf(), ele->getlen());
        return this;
    }
    operator const char *()
    {
        return buf;
    }

    /* get the buffer information */
    const char *getbuf() const { return buf; }
    size_t getlen() const { return used; }

    /* yield ownership of our buffer */
    char *yield_buffer()
    {
        char *ret;
        
        /* if the buffer was not allocated, make an allocated copy */
        if (buf == init_buf)
        {
            /* make a new copy */
            char *new_buf = (char *)t3malloc(used + 1);
            if (new_buf == 0)
                err_throw(TCERR_CODEGEN_NO_MEM);

            /* copy the data */
            memcpy(new_buf, buf, used + 1);

            /* return the new buffer */
            ret = new_buf;
        }
        else
        {
            /* our buffer is already allocated, so just return it */
            ret = buf;
        }

        /* revert to our initial built-in buffer */
        buf = init_buf;

        /* clear the buffer */
        buf[0] = '\0';
        used = 0;
        alo = init_buf_size;
    }

protected:

    /* 
     *   initial buffer - this is allocated as part of the initial structure
     *   to save malloc overhead 
     */
    static const size_t init_buf_size = 512;
    char init_buf[init_buf_size];

    /* buffer */
    char *buf;

    /* buffer space allocated/used */
    size_t alo;
    size_t used;
};


/* ------------------------------------------------------------------------ */
/*
 *   T3-specific code generator helper class.  This class provides a set
 *   of static functions that are useful for T3 code generation.  
 */
class CTcGenTarg
{
public:
    /* initialize the code generator */
    CTcGenTarg();

    /* destroy the code generator object */
    ~CTcGenTarg();

    /* 
     *   Push a javascript expression.  The string can be a printf-style
     *   format string, with varargs following.  First, the printf formatting
     *   is applied to generate the control string.  Then, we expand '$'
     *   expressions:
     *   
     *   $1 = the top-of-stack code expression
     *.  $2 = the 2nd item on the stack
     *.  $n = the nth item on the stack
     *   
     *   ${disc n} = substitute an empty string, but mark the nth stack item
     *   for discarding at the end of the process (see below) as though it
     *   had been mentioned in a regular $n expression
     *   
     *   ${args n m} = the 'm' stack elements starting at $n, concatenated
     *   together into an argument list (with commas as separators)
     *   
     *   ${,args n m} = same as ${args}, but start the list with a comma if
     *   it's non empty, otherwise generate nothing
     *   
     *   ${args[] n m varargs} = same as ${args}, but enclose the list in
     *   square brackets to make it an array at run-time.  If varargs is
     *   true, we already have a varargs list on the stack.
     *   
     *   $$ = a single literal '$'
     *   
     *   After expanding the '$' expressions, we pop the stack expressions
     *   used.  We pop every stack item up to and including the highest
     *   numbered item used in a '$' expression - even if an item is never
     *   used, it will be discarded if it's on the stack after an item that
     *   was mentioned.
     *   
     *   Finally, the expanded expression string that results from the above
     *   substitutions is pushed onto the expression stack.  
     */
    void js_expr(const char *str, ...);

    /* allocate a new global property ID */
    tctarg_prop_id_t new_prop_id() { return next_prop_++; }

    /* allocate a new global object ID */
    tctarg_obj_id_t new_obj_id() { return next_obj_++; }

    /* 
     *   Note constant data sizes.  This doesn't affect us, since we don't
     *   have to worry about dividing anything up into pages the way the T3
     *   code generator does.  
     */
    void note_str(size_t) { }
    void note_list(size_t) { }

    /* get the maximum string/list/bytecode lengths */
    size_t get_max_str_len() const { return 0; }
    size_t get_max_list_cnt() const { return 0; }
    size_t get_max_bytecode_len() const { return 0; }

    /* 
     *   Add a debug line record.  We don't keep debugging information, so we
     *   can just ignore this. 
     */
    void add_line_rec(class CTcTokFileDesc *, long) { }

    /*
     *   Add a function set to the dependency table - returns the index of
     *   the function set in the table 
     */
    int add_fnset(const char *fnset_extern_name, size_t len);
    int add_fnset(const char *fnset_extern_name)
        { return add_fnset(fnset_extern_name, strlen(fnset_extern_name)); }

    /* get the name of a function set given its index */
    const char *get_fnset_name(int idx) const;

    /* get the number of defined function sets */
    int get_fnset_cnt() const { return fnset_cnt_; }

    /* 
     *   Find a metaclass entry, adding it if it's not already there.  If the
     *   metaclass is already defined, and it has an associated symbol, we
     *   will not change the associated symbol - this will let the caller
     *   detect that the metaclass has been previously defined for a
     *   different symbol, which is usually an error.  
     */
    int find_or_add_meta(const char *nm, size_t len,
                         class CTcSymMetaclass *sym);

    /* get/set the symbol for a given metaclass */
    class CTcSymMetaclass *get_meta_sym(int meta_idx);
    void set_meta_sym(int meta_idx, class CTcSymMetaclass *sym);

    /* get the number of metaclasses */
    int get_meta_cnt() const { return meta_cnt_; }

    /* get the external (universally unique) name for the given metaclass */
    const char *get_meta_name(int idx) const;

    /*
     *   Notify the code generator that we're replacing an object (via the
     *   "replace" statement) at the given stream offset.  We'll mark the
     *   data in the stream as deleted so that we don't write it to the image
     *   file.  
     */
    void notify_replace_object(ulong stream_ofs);

    /*
     *   Notify the code generator that parsing is finished.  This should be
     *   called after parsing and before code generation begins.  
     */
    void parsing_done();

    /* generate synthesized code during linking */
    void build_synthesized_code();

    /* generate code for a dictionary object */
    void gen_code_for_dict(class CTcDictEntry *dict);

    /* generate code for a grammar production object */
    void gen_code_for_gramprod(class CTcGramProdEntry *prod);

    /*
     *   Write to an object file.  The compiler calls this after all parsing
     *   and code generation are completed to write an object file, which can
     *   then be linked with other object files to create an image file.  
     */
    void write_to_object_file(class CVmFile *object_fp,
                              class CTcMake *make_obj);

    /*
     *   Load an object file.  Returns zero on success, non-zero on error.  
     */
    int load_object_file(CVmFile *fp, const textchar_t *fname);

    /*
     *   Write the image file.  The compiler calls this after all parsing and
     *   code generation are completed to write an image file.  We must apply
     *   all fixups, assign the code and constant pool layouts, and write the
     *   data to the image file.  
     */
    void write_to_image(class CVmFile *image_fp, uchar data_xor_mask,
                        const char tool_data[4]);

    /*
     *   $$$ deprecated - temporary build scaffolding only 
     */
    void write_op(int op) { assert(FALSE); }
    void write_callprop(int argc, int varargs, int propno) { assert(FALSE); }
    void note_push(int n = 1) { assert(FALSE); }
    void note_pop(int n = 1) { assert(FALSE); }

    /* 
     *   Debugger mode/Speculative debugger evaluation?  We don't (currently)
     *   have any debugger integration possible in the js version, so this
     *   isn't relevant.  
     */
    int is_eval_for_debug() const { return FALSE; }
    int is_speculative() const { return FALSE; }
    int get_debug_stack_level() const { return 0; }

    /* push/pop expressions to/from the js stack */
    void push_js_expr(js_expr_buf &buf);
    void pop_js_expr() { pop_js_exprs(1); }
    void pop_js_exprs(int n);
    int get_sp_depth() const { return expr_stack_depth; }

    /* pop the js expression stack, appending the top of stack to a buffer */
    void pop_js_expr(js_expr_buf &buf)
    {
        copy_js_expr(buf, 1);
        pop_js_expr();
    }

    /* copy the contents of stack[n] to a buffer */
    void copy_js_expr(js_expr_buf &dst, int n);

protected:
    /* expression stack head */
    js_expr_ele *expr_stack;
    int expr_stack_depth;

    /* property/object ID allocators */
    tctarg_prop_id_t next_prop_;
    tctarg_obj_id_t next_obj_;

    /* metaclass tracking */
    int meta_cnt_;

    /* function set tracking */
    int fnset_cnt_;
};


#endif /* TCJS_H */
