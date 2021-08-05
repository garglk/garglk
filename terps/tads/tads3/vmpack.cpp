#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmpack.cpp - binary data stream pack/unpack
Function
  
Notes
  
Modified
  10/01/10 MJRoberts  - Creation
*/

#include <float.h>
#include <math.h>
#include <limits.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmpack.h"
#include "vmdatasrc.h"
#include "vmstack.h"
#include "vmrun.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmbignum.h"
#include "vmbytarr.h"
#include "utf8.h"
#include "charmap.h"


/* ------------------------------------------------------------------------ */
/*
 *   Type code classifiers 
 */

/*
 *   Is the type code valid? 
 */
static inline int is_valid_type(wchar_t c)
{
    return c <= 127 && strchr("aAbuUwWhHcCsSlLqQkfdxX@\"{", (char)c) != 0;
}


/* 
 *   Is the type repeatable?  A repeatable type is a fixed-length type like L
 *   or C.  String types are not repeatable; they instead treat a count as a
 *   length for the string. 
 */
static inline int is_repeatable(wchar_t c)
{
    /* it's repeatable if it's not one of the string types */
    return strchr("aAbuUwWhHxX@", (char)c) == 0;
}

/*
 *   Is this a varying-length item? 
 */
static inline int is_varying_length(wchar_t c)
{
    return strchr("aAbuUwWhH", (char)c) != 0;
}

/*
 *   Get the character padding for a type.  A, U, and W use space padding;
 *   everything else uses Nul padding.
 */
static inline char get_padding_char(wchar_t c)
{
    return strchr("AUW", (char)c) != 0 ? ' ' : 0;
}

/*
 *   Is this a type that consumes an argument?
 */
static inline int type_consumes_arg(wchar_t c)
{
    return strchr("xX@\"{", (char)c) == 0;
}



/* ------------------------------------------------------------------------ */
/* 
 *   string parser position 
 */
struct CVmPackPos
{
    CVmPackPos(const char *p, size_t len)
        : p((char *)p), len(len), idx(1)
    { }

    CVmPackPos(const CVmPackPos *pos)
        : p(pos->p.getptr()), len(pos->len), idx(pos->idx)
    { }

    CVmPackPos()
        : p(), len(0), idx(0)
    { }

    /* set to another position */
    void set(const CVmPackPos *pos)
    {
        p.set(&pos->p);
        len = pos->len;
        idx = pos->idx;
    }

    /* is another character available? */
    int more()
    {
        /* get the current character (skipping spaces) */
        getch();

        /* indicate whether or not we have more in the buffer */
        return len != 0;
    }

    /* get the current charater, without skipping spaces */
    wchar_t getch_raw()
    {
        /* if there's a character left, return it */
        return len != 0 ? p.getch() : 0;
    }

    /* get the current character, skipping spaces */
    wchar_t getch()
    {
        /* skip spaces */
        while (len != 0 && is_space(p.getch()))
        {
            ++idx;
            p.inc(&len);
        }

        /* return the current character */
        return getch_raw();
    }

    /* skip the current character */
    void inc()
    {
        if (len != 0)
        {
            p.inc(&len);
            ++idx;
        }
    }

    /* get and skip the current character, skipping whitespace */
    wchar_t nextch()
    {
        /* skip spaces */
        while (len != 0 && is_space(p.getch()))
        {
            p.inc(&len);
            ++idx;
        }

        /* if there's anything left, get and skip a character */
        if (len != 0)
        {
            /* we have a character available - get the current character */
            wchar_t ch = p.getch();

            /* skip it */
            p.inc(&len);
            ++idx;

            /* return the last character */
            return ch;
        }
        else
        {
            /* no more characters available - return nul */
            return 0;
        }
    }

    /* parse an integer value */
    int parse_int()
    {
        /* remember the starting location, for error reporting */
        int start_idx = idx;

        /* parse digits until we find something else */
        int acc;
        wchar_t c;
        for (acc = 0 ; is_digit(c = getch()) ; inc())
        {
            /* add the digit into the accumulator */
            acc *= 10;
            acc += value_of_digit(c);

            /* if this overflows an int, it's an error */
            if (acc < 0)
                err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, start_idx);
        }

        /* return the result */
        return acc;
    }

    /* get the current character index (1-based, for reporting to the user) */
    int index() const { return idx; }

    /* current pointer and remaining length */
    utf8_ptr p;
    size_t len;

    /* current character index from the beginning of the string */
    int idx;
};


/* ------------------------------------------------------------------------ */
/*
 *   Special repeat counts
 */
const int ITER_NONE = -1;                      /* no repeat count specified */
const int ITER_STAR = -2;                   /* star '*' as the repeat count */


/* ------------------------------------------------------------------------ */
/* 
 *   type code descriptor - this captures the type information for a single
 *   type code entry, including prefix and postfix flags 
 */
struct CVmPackType
{
    CVmPackType()
    {
        type_code = 0;
        big_endian = FALSE;
        count = ITER_NONE;
        count_as_type = 0;
        count_in_bytes = FALSE;
        up_to_count = FALSE;
        bang = FALSE;
        tilde = FALSE;
        pct = FALSE;
        fmtidx = 0;
        null_term = FALSE;
        qu = FALSE;
    }

    CVmPackType(const CVmPackGroup *g);

    /* type code character */
    wchar_t type_code;

    /* 
     *   for type codes '"' and '{', the literal text segment, as a pointer
     *   into the original format string buffer 
     */
    CVmPackPos lit;

    /* is it big-endian? */
    int big_endian;

    /* 
     *   repeat count - this is the number of repetitions specified in the
     *   repeat count suffix, or a special ITER_xxx value 
     */
    int count;

    /* is the repeat count in bytes (:b suffix)? */
    int count_in_bytes;

    /* 
     *   Repeat count as a type code.  If the :X modifier if used, the X is
     *   another type code, and the repeat count is taken as the size of that
     *   type.  This stores the type code in this case.  We also set 'count'
     *   to the size of the type, so this is just preserving the extra
     *   information about the source of the size.  
     */
    wchar_t count_as_type;

    /* 
     *   Should we treat the count as an upper bound?  This is set if we
     *   combine '*' with a number, as in "H30*" - this means pack/unpack up
     *   to 30 items, but stop if we run out of source material first. 
     */
    int up_to_count;

    /* is there a '!' qualifier? */
    int bang;

    /* is there a '~' qualifier? */
    int tilde;

    /* is there a '%' qualifier? */
    int pct;

    /* is there a '?' qualifier? */
    int qu;

    /* is it null-terminated? */
    int null_term;

    /* format string index */
    int fmtidx;

    /* get the count - returns 1 as the implied count if none was specified */
    int get_count() const { return has_count() ? count : 1; }

    /* is there an explicit count specified? */
    int has_count() const { return count >= 0; }
};


/* ------------------------------------------------------------------------ */
/*
 *   Pack string group.  This represents a ( ) group within the string.  The
 *   overall string is the root group.  Each parenthesized group has its own
 *   set of modifiers, a repeat count, and establishes a starting byte offset
 *   for the "@" operator.  
 */
struct CVmPackGroup
{
    /* set up the defaults for the root group */
    CVmPackGroup(CVmPackPos *pos, CVmDataSource *ds)
        : start(pos)
    {
        this->ds = ds;
        parent = 0;
        big_endian = FALSE;
        tilde = FALSE;
        pct = FALSE;
        stream_ofs = ds->get_pos();
        cur_iter = 1;
        num_iters = 1;
        up_to_iters = FALSE;
        close_paren = 0;
    }

    /* set up a new nested group */
    CVmPackGroup(CVmPackGroup *parent, wchar_t open_paren,
                 CVmPackPos *pos, const CVmPackType *t)
        : parent(parent), start(pos)
    {
        ds = parent->ds;
        stream_ofs = ds->get_pos();
        cur_iter = 1;
        big_endian = t->big_endian;
        tilde = t->tilde;
        pct = t->pct;
        num_iters = t->count;
        up_to_iters = t->up_to_count;
        close_paren = (open_paren == '(' ? ')' :
                       open_paren == '[' ? ']' :
                       open_paren == '{' ? '}' : 0);
    }

    /* parent group */
    CVmPackGroup *parent;

    /* the paren type that closes the group */
    wchar_t close_paren;

    /* 
     *   Default endian-ness within this group.  The root group is
     *   little-endian by default, and each nested group inherits the
     *   endian-ness of its containing group.  
     */
    int big_endian;

    /*
     *   Default 'tilde' (~) modifier mode within this group.  The root group
     *   has this turned off by default, and each nested group inherits its
     *   containing group's status.  
     */
    int tilde;

    /*
     *   Default 'percent' (%) modifier mode within this group.  This is
     *   inherited like the endian-ness and tilde modifiers.  
     */
    int pct;

    /* 
     *   Group start position in template string.  This is the position of
     *   the first character after the open paren of the group.  
     */
    CVmPackPos start;

    /* data stream */
    CVmDataSource *ds;

    /* 
     *   Group starting offset in the data stream.  Each iteration of a group
     *   resets the zero point for '@' codes within the group. 
     */
    long stream_ofs;

    /* get the stream offset of the outermost parent */
    long root_stream_ofs() const
    {
        return (parent != 0 ? parent->root_stream_ofs() : stream_ofs);
    }

    /* current iteration for this group */
    int cur_iter;
    
    /* total number of iterations for this group */
    int num_iters;

    /* is this an "up to" iteration count (e.g., 30*)? */
    int up_to_iters;
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmPackType implementation 
 */

CVmPackType::CVmPackType(const CVmPackGroup *g)
{
    /* clear the type code and count */
    type_code = 0;
    count = ITER_NONE;
    count_as_type = 0;
    count_in_bytes = FALSE;
    bang = FALSE;
    qu = FALSE;
    null_term = FALSE;

    /* inherit group attributes */
    big_endian = g->big_endian;
    tilde = g->tilde;
    pct = g->pct;
}


/* ------------------------------------------------------------------------ */
/*
 *   Packing argument list.  There are two kinds of argument list: function
 *   arguments on the stack, or a list-like object.  
 */
struct CVmPackArgs
{
    /* are there more arguments? */
    virtual int more() = 0;

    /* number of remaining arguments */
    virtual int nrem() = 0;

    /* get the current argument */
    virtual vm_val_t *get(vm_val_t *val) = 0;
    
    /* advance to the next argument */
    virtual void next() = 0;
};

/* 
 *   StackArgs represents arguments directly from a function/method call
 *   frame on the stack 
 */
struct StackArgs: CVmPackArgs
{
    StackArgs(VMG_ int first_arg, int argc)
    {
        this->stk = G_stk;
        this->curarg = first_arg;
        this->lastarg = first_arg + argc - 1;
    }

    virtual int more() { return curarg <= lastarg; }
    virtual int nrem() { return curarg <= lastarg ? lastarg - curarg + 1 : 0; }
    virtual vm_val_t *get(vm_val_t *val)
    {
        if (curarg <= lastarg)
            *val = *stk->get(curarg);
        else
            val->set_nil();
        return val;
    }
    virtual void next() { ++curarg; }

    CVmStack *stk;
    int curarg;
    int lastarg;
};

/*
 *   ListArgs represents arguments contained in a list.  For a repeated item,
 *   the corresponding argument can be a list, in which case the repetitions
 *   are taken from the list:
 *   
 *   packBytes('a10 c5', 'hello', [1, 2, 3, 4, 5]);
 */
struct ListArgs: CVmPackArgs
{
    ListArgs(VMG_ vm_val_t *lst) { set(vmg_ lst); }

    void set(VMG_ vm_val_t *lst)
    {
        this->vmg = VMGLOB_ADDR;
        this->lst = *lst;
        this->idx = 1;
        this->len = lst->ll_length(vmg0_);
    }

    virtual int more() { return idx <= (len >= 0 ? len : 1); }
    virtual int nrem()
    {
        int last = (len >= 0 ? len : 1);
        return idx <= last ? last - idx + 1 : 0;
    }
    virtual vm_val_t *get(vm_val_t *val)
    {
        if (len < 0 && idx == 1)
        {
            /* 
             *   we don't have a list, so pretend the value was wrapped in a
             *   single-element list by returning it at index 1
             */
            *val = lst;
        }
        else if (idx <= len)
        {
            /* we have a valid list and a valid index - get the element */
            VMGLOB_PTR(vmg);
            lst.ll_index(vmg_ val, idx);
        }
        else
        {
            /* we're past the end of the list - return nil */
            val->set_nil();
        }
        return val;
    }
    virtual void next() { ++idx; }

    /* get the effective count - the number of items we'll return */
    int effective_count() { return len < 0 ? 1 : len; }

    vm_globals *vmg;
    vm_val_t lst;
    int idx;
    int len;
};

/*
 *   A single argument.  This encapsulates an argument value that we've
 *   already resolved and represents it as an argument list with one element.
 */
struct SingleArg: CVmPackArgs
{
    SingleArg(const vm_val_t *val)
    {
        this->val = *val;
        idx = 0;
    }
    SingleArg(long l)
    {
        val.set_int(l);
        idx = 0;
    }
    SingleArg()
    {
        val.set_nil();
        idx = 0;
    }

    virtual int more() { return idx == 0; }
    virtual int nrem() { return idx == 0 ? 1 : 0; }
    virtual vm_val_t *get(vm_val_t *val)
    {
        if (idx == 0)
            *val = this->val;
        else
            val->set_nil();
        return val;
    }
    virtual void next() { ++idx; }

    vm_val_t val;
    int idx;
};

/* ------------------------------------------------------------------------ */
/*
 *   Pack data from stack arguments into the given data stream 
 */
void CVmPack::pack(VMG_ int arg_index, int argc, CVmDataSource *dst)
{
    /* the first argument is the format string - retrieve it */
    const char *fmt = G_stk->get(arg_index)->get_as_string(vmg0_);
    if (fmt == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get the format string length and buffer pointer */
    size_t fmtlen = vmb_get_len(fmt);
    fmt += VMB_LEN;

    /* pack the root group */
    CVmPackPos p(fmt, fmtlen);
    CVmPackGroup root(&p, dst);
    StackArgs args(vmg_ arg_index + 1, argc - 1);
    pack_group(vmg_ &p, &args, &root, FALSE);

    /* make sure we exhausted the string */
    if (p.more())
        err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p.index());

    /* 
     *   if we didn't consume all of the arguments, it means that the format
     *   string didn't have enough entries for the supplied arguments - flag
     *   an error 
     */
    if (args.more())
        err_throw_a(VMERR_PACK_ARGC_MISMATCH, 1, ERR_TYPE_INT, (int)fmtlen+1);
}

/*
 *   Pack a group 
 */
void CVmPack::pack_group(VMG_ CVmPackPos *p, CVmPackArgs *args_main,
                         CVmPackGroup *group, int list_per_iter)
{
    /* assume we're unpacking from the main argument list */
    CVmPackArgs *args = args_main;
    
    /* 
     *   if there's a list per group iteration, the next argument should be a
     *   list from which we'll take arguments for this single iteration 
     */
    vm_val_t listval;
    ListArgs listargs(vmg_ args->get(&listval));
    int more = args->more();
    if (list_per_iter)
    {
        /* pull out and switch to the sublist for the first iteration */
        more = args_main->more();
        args = &listargs;
        args_main->next();
    }

    /* 
     *   if this is an up-to count, and the source list is empty, packing
     *   zero iterations is valid 
     */
    if ((group->num_iters == ITER_STAR || group->up_to_iters) && !more)
    {
        /* skip the group in the source */
        p->set(&group->start);
        skip_group(p, group->close_paren);

        /* we're done */
        return;
    }

    /* run through the format list */
    while (p->more())
    {
        /* check for group entry/exit */
        wchar_t c = p->getch();
        if (c == '(' || c == '[')
        {
            /* start of a group - pack the sub-group */
            pack_subgroup(vmg_ p, args, group, 0);
        }
        else if (c == group->close_paren)
        {
            /* 
             *   if we're reading arguments from a separate sublist per
             *   iteration of the group, move on to the next sublist 
             */
            int more = args->more();
            if (list_per_iter)
            {
                /* switch to the next sublist from the main arguments */
                more = args_main->more();
                listargs.set(vmg_ args_main->get(&listval));
                args_main->next();
            }

            /* 
             *   Determine if we're done.  If the group has a '*' repeat
             *   count, we're done when we're out of arguments.  If it has an
             *   up-to count, we're done if we're either out of arguments or
             *   at the iteration count limit.  Otherwise, we're done when
             *   we've reached the iteration count limit.
             */
            if ((group->num_iters == ITER_STAR
                 ? !more
                 : group->cur_iter >= group->num_iters)
                || (group->up_to_iters && !more))
            {
                /* 
                 *   if this is an up-to count, make sure we consumed all
                 *   arguments 
                 */
                if (group->up_to_iters)
                {
                    if (list_per_iter)
                        for ( ; args_main->more() ; args_main->next()) ;
                    else
                        for ( ; args->more() ; args->next()) ;
                }
                
                /* done */
                return;
            }

            /* 
             *   We're going back for another iteration of the group.
             *   Increment the iteration count, and go back to the start of
             *   the group in the template string.  
             */
            group->cur_iter++;
            group->stream_ofs = group->ds->get_pos();
            p->set(&group->start);
        }
        else
        {
            /* 
             *   It's not a group, so it must be a single item.  Parse the
             *   item definition. 
             */
            CVmPackType t;
            parse_type(p, &t, group);

            /* check to see if this is a length prefix to the next item */
            if (p->getch() == '/')
            {
                /* skip the '/' */
                p->inc();

                /* check for a group */
                if ((c = p->getch()) == '(' || c == '[')
                {
                    /* go pack the subgroup with the prefix count */
                    pack_subgroup(vmg_ p, args, group, &t);
                }
                else
                {
                    /* 
                     *   It's not a group, so it must be a single item.
                     *   Parse the item definition. 
                     */
                    CVmPackType t2;
                    parse_type(p, &t2, group);

                    /* pack the item with the count prefix */
                    pack_item(vmg_ group, args, &t2, &t);
                }
            }
            else
            {
                /* there's no count prefix - go pack the single item */
                pack_item(vmg_ group, args, &t, 0);
            }
        }
    }
}
            
/*
 *   Pack a subgroup 
 */
void CVmPack::pack_subgroup(VMG_ CVmPackPos *p, CVmPackArgs *args,
                            CVmPackGroup *group, CVmPackType *prefix_count)
{
    /* get the output stream */
    CVmDataSource *dst = group->ds;
    
    /* parse the group modifiers */
    CVmPackType gt(group);
    parse_group_mods(p, &gt);

    /* note the open paren */
    wchar_t paren = p->getch();

    /* set up a child group definition reflecting the modifiers */
    p->inc();
    CVmPackGroup child(group, paren, p, &gt);

    /* set up a list argument reader, in case we have a list for the group */
    vm_val_t arg;
    ListArgs la(vmg_ args->get(&arg));

    /* 
     *   Set up the child argument list.  If the open paren for the list in
     *   the template is '[', the arguments come from a list; otherwise they
     *   come from the main argument list. 
     */
    CVmPackArgs *child_args = args;
    int list_per_iter = FALSE;
    if (paren == '[')
    {
        /* [] group - pack from a list or a set of lists */
        if (gt.bang)
        {
            /* ! modifier - each iteration is a separate sublist */
            list_per_iter = TRUE;
        }
        else
        {
            /* "[ ]" group - the whole group comes from a single sublist */
            child_args = &la;
            args->next();
        }
    }

    /* check for a prefix count */
    long prefix_pos = 0;
    if (prefix_count != 0)
    {
        /* remember the location of the prefix */
        prefix_pos = dst->get_pos();

        /* pack a zero count as a placeholder */
        SingleArg count((long)0);
        pack_one_item(vmg_ group, &count, prefix_count, 0);

        /* the prefix count overrides any suffix count with '*' */
        child.num_iters = ITER_STAR;
    }

    /* recursively pack the child group */
    pack_group(vmg_ p, child_args, &child, list_per_iter);

    /* make sure we stopped at the close paren */
    if (p->nextch() != child.close_paren)
        err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p->index());

    /* 
     *   if there was a prefix count, go back and patch it with the actual
     *   iteration count 
     */
    if (prefix_count != 0)
    {
        /* save the end-of-group position, and seek back to the count */
        long endpos = dst->get_pos();
        dst->seek(prefix_pos, OSFSK_SET);

        /* it's an error if the prefix count is a variable-length item */
        if (prefix_count->type_code == 'k'
            || (is_varying_length(prefix_count->type_code)
                && prefix_count->count == ITER_STAR))
            err_throw_a(VMERR_PACK_ARG_MISMATCH, 1, ERR_TYPE_INT,
                        prefix_count->fmtidx);

        /* write the count */
        SingleArg count(child.cur_iter);
        pack_one_item(vmg_ group, &count, prefix_count, 0);

        /* seek back to the end of the group */
        dst->seek(endpos, OSFSK_SET);
    }

    /* skip any modifiers after the group */
    parse_mods(p, &gt);
}


/* ------------------------------------------------------------------------ */
/*
 *   Write padding bytes.  We write alternating ch1 and ch2 bytes to fill out
 *   the byte count.  
 */
void CVmPack::write_padding(CVmDataSource *dst, char ch1, char ch2, long cnt)
{
    /* set up a buffer full of the padding bytes */
    char buf[256];
    for (size_t i = 0 ; i < sizeof(buf) ; buf[i++] = ch1, buf[i++] = ch2) ;

    /* keep going until we satisfy the request */
    while (cnt > 0)
    {
        /* write out one buffer-full, or the remaining amount if less */
        size_t cur = (cnt > (long)sizeof(buf) ? sizeof(buf) : (size_t)cnt);
        if (dst->write(buf, cur))
            err_throw(VMERR_WRITE_FILE);

        /* deduct this from the remaining request */
        cnt -= cur;
    }
}
void CVmPack::write_padding(CVmDataSource *dst, char ch, long cnt)
{
    write_padding(dst, ch, ch, cnt);
}

/* ------------------------------------------------------------------------ */
/*
 *   get an integer value, enforcing the given limits 
 */
static long intval(VMG_ const vm_val_t *val, long minval, long maxval,
                   int pct, unsigned long mask)
{
    /* get the value */
    long l;
    switch (val->typ)
    {
    case VM_NIL:
        /* use 0 for nil */
        l = 0;
        break;

    case VM_INT:
        /* regular integer */
        l = val->val.intval;
        break;

    case VM_OBJ:
        /* 
         *   convert BigNumber to integer explicitly; use the type cast for
         *   other types 
         */
        if (CVmObjBigNum::is_bignum_obj(vmg_ val->val.obj))
        {
            /* explicitly cast the BigNumber so that we can catch overflows */
            int ov;
            if (minval < 0)
            {
                l = ((CVmObjBigNum *)vm_objp(vmg_ val->val.obj))
                    ->convert_to_int(ov);
            }
            else
            {
                l = (long)((CVmObjBigNum *)vm_objp(vmg_ val->val.obj))
                    ->convert_to_uint(ov);
            }

            /* check for overflow */
            if (ov && !pct)
                err_throw(VMERR_NUM_OVERFLOW);;

            /* done */
            break;
        }
        /* FALL THROUGH to default */

    default:
        /* use the generic type cast */
        l = val->cast_to_int(vmg0_);
        break;
    }

    /* if the '%' flag is set, truncate the value, otherwise check the range */
    if (pct)
    {
        /* % modifier - truncate the value to fit the type */
        l &= mask;
    }
    else
    {
        /* no % modifier - enforce the range */
        if (l < minval || l > maxval)
            err_throw(VMERR_NUM_OVERFLOW);
    }

    /* return the value */
    return l;
}

/* 
 *   integer decomposition - get the low-order byte and shift 
 */
static inline char lsb(long &l, int is_signed)
{
    /* note the sign */
    int neg = l < 0;

    /* the return value is the low-order byte */
    char ret = (char)(l & 0xff);

    /* shift the value to remove the low-order byte */
    l >>= 8;

    /* mask off the high byte */
    l &= 0x00ffffff;

    /* if it's a signed negative value, put 1 bits in the high byte */
    if (is_signed && neg)
        l |= 0xff000000;

    /* return the low byte */
    return ret;
}

/* 
 *   reverse the byte order of a buffer 
 */
static void reverse_bytes(char *buf, size_t len)
{
    /* 
     *   Run through the buffer from opposite ends, swapping each byte pair.
     *   Stop when our left index crosses our right index.  If the buffer
     *   length is even, the two indices will pass each other; if the length
     *   is even, they'll both land on the middle byte of the buffer at the
     *   same time.  Simply keep going as long as the left index is less than
     *   the right index.  
     */
    int i, j;
    for (i = 0, j = len - 1 ; i < j ; ++i, --j)
    {
        /* swap the current high/low pair */
        char tmp = buf[i];
        buf[i] = buf[j];
        buf[j] = tmp;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   String source for packing.  We can pack strings and ByteArrays into
 *   character fields; this class is an abstraction for the different types
 *   of sources.  
 */
class CharFieldSource
{
public:
    virtual ~CharFieldSource() { }

    /* create a concrete CharFieldSource for a value */
    static CharFieldSource *create(VMG_ const vm_val_t *val, vm_val_t *newval);

    /* get the array length to use for the given field type */
    virtual int arraylen(CVmPackType *t) = 0;

    /* is another character available? */
    virtual int more() = 0;

    /* get the next character */
    virtual wchar_t getch() = 0;
};

/* string source */
class CharFieldSourceString: public CharFieldSource
{
public:
    CharFieldSourceString(const char *src, size_t len)
        : p((char *)src), len(len)
    { }

    virtual int more() { return len != 0; }
    virtual wchar_t getch()
    {
        if (len != 0)
        {
            wchar_t ch = p.getch();
            p.inc(&len);
            return ch;
        }
        else
            return 0;
    }

    virtual int arraylen(CVmPackType *t)
    {
        switch (t->type_code)
        {
        case 'a':
        case 'A':
        case 'b':
            /* the length is in bytes, and we write one byte per character */
            return p.len(len);

        case 'w':
        case 'W':
            /* 
             *   the default length is in characters; if counting in bytes,
             *   we need two bytes per character 
             */
            return (t->count_in_bytes ? 2 : 1) * p.len(len);
            
        case 'h':
        case 'H':
            /* 
             *   the length is in source nibbles, one nibble per character;
             *   in bytes, we need half as many bytes as characters, rounded
             *   up 
             */
            return t->count_in_bytes ? (p.len(len)+1)/2 : p.len(len);

        case 'u':
        case 'U':
            /* 
             *   the length is in bytes, in UTF-8 format; this is the same
             *   format as our underlying string, so we can simply return its
             *   byte length 
             */
            return len;

        default:
            return 0;
        }
    }

protected:
    /* utf8 pointer and remaining byte length */
    utf8_ptr p;
    size_t len;
};

/* ByteArray source */
class CharFieldSourceByteArray: public CharFieldSource
{
public:
    CharFieldSourceByteArray(VMG_ vm_obj_id_t id)
    {
        arr = (CVmObjByteArray *)vm_objp(vmg_ id);
        idx = 0;
        len = arr->get_element_count();
    }

    virtual int more() { return idx < len; }
    virtual wchar_t getch() { return idx < len ? arr->get_element(++idx) : 0; }

    virtual int arraylen(CVmPackType *t)
    {
        switch (t->type_code)
        {
        case 'a':
        case 'A':
        case 'b':
            /* 
             *   the length is in bytes, and we write one byte per character;
             *   we treat each byte in the array as a Latin-1 character, so
             *   this is the same as our array length 
             */
            return (int)len;

        case 'w':
        case 'W':
            /* 
             *   The length is in characters, or two bytes per character.  We
             *   treat our bytes as latin-1 characters, so the character
             *   count is the saqme as the array length. 
             */
            return (int)(t->count_in_bytes ? len*2 : len);
            
        case 'h':
        case 'H':
            /* 
             *   one character per array element, or one byte per pair of
             *   array elements 
             */
            return (int)(t->count_in_bytes ? (len+1)/2 : len);

        case 'u':
        case 'U':
            /* 
             *   The length is in bytes, in UTF-8 format.  We treat our array
             *   elements as Latin-1 characters.  In UTF-8, characters in the
             *   range 0-127 take one byte, and 128-255 take two bytes.  Run
             *   through our array and add up the required lengths.  
             */
            {
                int ret;
                unsigned long i;
                for (ret = 0, i = 1 ; i <= len ; ++i)
                    ret += arr->get_element(i) < 128 ? 1 : 2;

                /* return the length */
                return ret;
            }

        default:
            return 0;
        }
    }

protected:
    /* ByteArray object */
    CVmObjByteArray *arr;

    /* current byte index */
    unsigned long idx;

    /* length of the array */
    unsigned long len;
};

/* create an appropriate CharFieldSource subclass for a given value */
CharFieldSource *CharFieldSource::create(
    VMG_ const vm_val_t *val, vm_val_t *newval)
{
    /* presume we won't create a new value */
    newval->set_nil();

    /* check to see if it's a ByteArray */
    if (val->typ == VM_OBJ
        && CVmObjByteArray::is_byte_array(vmg_ val->val.obj))
        return new CharFieldSourceByteArray(vmg_ val->val.obj);

    /* it's not another acceptable type, so it has to be a string */
    const char *str = val->cast_to_string(vmg_ newval);
    return new CharFieldSourceString(str + VMB_LEN, vmb_get_len(str));
}


/* ------------------------------------------------------------------------ */
/*
 *   String character-to-byte converters.  Each one of these has a singleton
 *   instance that we use in the item packer.  These objects have no state
 *   (they're really just a convenient way to write function vectors, via
 *   their virtual methods), so the static singletons are inherently
 *   thread-safe.  
 */
class CharConv
{
public:
    /* convert one character from source to packed format */
    virtual void putch(char *&buf, wchar_t ch, int idx, int pct) = 0;

    /* 
     *   Close out a packed string.  This is required for formats that write
     *   multiple source characters per byte, such as 'hH', to make sure we
     *   write out any partially built last byte.  Formats that write one or
     *   more complete bytes per source character need do nothing here.  
     */
    virtual void putch_end(char *&buf, int idx) { }

    /* write a null terminator */
    virtual void null_term(CVmDataSource *dst)
    {
        if (dst->write("\0", 1))
            err_throw(VMERR_WRITE_FILE);
    }

    /* 
     *   Get the field width in bytes for the given descriptor.  If the
     *   descriptor's repeat count is explicitly stated in bytes, this simply
     *   returns the repeat count.  Otherwise, we translate the repeat count
     *   to a byte count according to the underlying character encoding.
     */
    int get_byte_count(const CVmPackType *t)
        { return t->count_in_bytes ? t->get_count() : count_to_bytes(t); }

    /* translate a repeat count to a byte length; assume 1-to-1 by default */
    virtual int count_to_bytes(const CVmPackType *t)
        { return t->get_count(); }

    /* translate a byte length to a repeat count; assume 1-to-1 by default */
    virtual long bytes_to_count(long bytes)
        { return bytes; }

    /* write space or nul padding - by default, write single-byte padding */
    virtual void pad(CVmDataSource *dst, char ch, long str_bytes,
                     const CVmPackType *t)
    {
        CVmPack::write_padding(dst, ch, get_byte_count(t) - str_bytes);
    }

    /* unpack file data */
    virtual void unpack(VMG_ vm_val_t *val, CVmDataSource *src, int cnt,
                        char pad)
    {
        /* if the requested length is zero, return an empty string */
        if (cnt == 0)
        {
            val->set_obj(CVmObjString::create(vmg_ FALSE, 0));
            return;
        }
            
        /*
         *   By default, we return a string.  Set up a wide character buffer
         *   to hold the input. 
         */
        wchar_t *wbuf = new wchar_t[cnt];
        if (wbuf == 0)
            err_throw(VMERR_OUT_OF_MEMORY);

        err_try
        {
            /* read the data */
            read(wbuf, src, cnt);

            /* remove padding characters from the end of the string */
            for ( ; cnt > 0 && wbuf[cnt-1] == pad ; --cnt) ;

            /* create the string */
            val->set_obj(CVmObjString::create(vmg_ FALSE, wbuf, cnt));
        }
        err_finally
        {
            delete [] wbuf;
        }
        err_end;
    }

    /* unpack file data for a varying-length field with null termination */
    void unpackz(VMG_ vm_val_t *val, CVmDataSource *src)
    {
        /* set up a string with an initial guess at the size */
        val->set_obj(CVmObjString::create(vmg_ FALSE, 256));
        CVmObjString *str = (CVmObjString *)vm_objp(vmg_ val->val.obj);
        G_stk->push(val);

        /* read characters until we reach the null terminator */
        char *dst = str->cons_get_buf();
        for (;;)
        {
            /* read a character */
            wchar_t ch = readch(src);

            /* if it's the null character, we're done */
            if (ch == 0)
                break;

            /* append it to the string */
            dst = str->cons_append(vmg_ dst, ch, 256);
        }

        /* set the final size of the string */
        str->cons_shrink_buffer(vmg_ dst);
    }

    /* read a single character */
    virtual wchar_t readch(CVmDataSource *src)
    {
        /* read a byte into our buffer */
        char buf[1];
        if (src->read(buf, 1))
            err_throw(VMERR_READ_FILE);

        /* return the character */
        return buf[0];
    }

    /* get the character converter for a given type code */
    static CharConv *conv_for_type(const CVmPackType *t);

protected:
    /* read from the data source, converting to wide characters */
    virtual void read(wchar_t *buf, CVmDataSource *src, int cnt) = 0;
};

/*
 *   Latin-1 converter, for 'a' and 'A' formats 
 */
class CharConvLatin1: public CharConv
{
public:
    virtual void putch(char *&buf, wchar_t ch, int idx, int pct)
    {
        /* latin-1 - store the character if it's in range, '?' otherwise */
        *buf++ = (ch <= 255 ? (char)ch : pct ? (char)ch & 0xff : '?');
    }

    virtual void read(wchar_t *buf, CVmDataSource *src, int cnt)
    {
        /* read the latin-1 character bytes into the buffer */
        if (src->read(buf, cnt))
            err_throw(VMERR_READ_FILE);

        /* 
         *   The file characters are bytes, but we want wide characters.  The
         *   bytewise read packed the Latin-1 byte representations into the
         *   lower half of the buffer.  Run through the buffer from the end,
         *   expanding each byte into a wide character.  This is trivial for
         *   Latin-1 since a Latin-1 code point is identical to the
         *   corresponding Unicode code point.  
         */
        for (int i = cnt - 1 ; i >= 0 ; --i)
            buf[i] = (wchar_t)(((unsigned char *)buf)[i]);
    }
};
static CharConvLatin1 s_CharConvLatin1;

/*
 *   Byte converter, for 'b' format 
 */
class CharConvByte: public CharConv
{
    virtual void putch(char *&buf, wchar_t ch, int idx, int pct)
    {
        /* raw bytes - make sure the byte is in range */
        if (ch > 255)
        {
            /* % modifier -> truncate to byte, otherwise it's an error */
            if (pct)
                ch &= 0xff;
            else
                err_throw(VMERR_NUM_OVERFLOW);
        }

        /* store it */
        *buf++ = (char)ch;
    }

    /* unpack file data */
    virtual void unpack(VMG_ vm_val_t *val, CVmDataSource *src, int cnt,
                        char pad)
    {
        /*
         *   For the byte type, we unpack into a ByteArray object rather than
         *   a string, and we leave any padding intact.  Create a byte array
         *   with the required size (and push it for gc protection).  
         */
        val->set_obj(CVmObjByteArray::create(vmg_ FALSE, cnt));
        CVmObjByteArray *arr = (CVmObjByteArray *)vm_objp(vmg_ val->val.obj);
        G_stk->push(val);

        /* read the data (don't save undo, since it's a new object) */
        if ((int)arr->read_from_file(
            vmg_ val->val.obj, src, 1, cnt, FALSE) != cnt)
            err_throw(VMERR_READ_FILE);

        /* done with the gc protection */
        G_stk->discard();
    }

    /* we don't need to read characters, since we unpack into a byte array */
    virtual void read(wchar_t *, CVmDataSource *, int) { }
};
static CharConvByte s_CharConvByte;

/*
 *   UTF8 converter, for 'u' and 'U' formats 
 */
class CharConvUTF8: public CharConv
{
public:
    virtual void putch(char *&buf, wchar_t ch, int idx, int pct)
    {
        /* utf8 - store the utf8 sequence for the character */
        buf += utf8_ptr::s_putch(buf, ch);
    }

    /* read a single character */
    virtual wchar_t readch(CVmDataSource *src)
    {
        /* read the first byte */
        char buf[10];
        if (src->read(buf, 1))
            err_throw(VMERR_READ_FILE);

        /* figure how many more bytes we have to read */
        size_t len = utf8_ptr::s_charsize(buf[0]);
        if (len > sizeof(buf))
            len = sizeof(buf);

        /* read the rest of the bytes */
        if (len > 1 && src->read(buf+1, len-1))
            err_throw(VMERR_READ_FILE);

        /* return the translated character */
        return utf8_ptr::s_getch(buf);
    }

    /* unpack file data */
    virtual void unpack(VMG_ vm_val_t *val, CVmDataSource *src, int cnt,
                        char pad)
    {
        /*
         *   For UTF-8, we can simply read the bytes directly into a string
         *   buffer.  Allocate the string at the required byte size, and push
         *   for gc protection.  
         */
        val->set_obj(CVmObjString::create(vmg_ FALSE, cnt));
        CVmObjString *str = (CVmObjString *)vm_objp(vmg_ val->val.obj);
        G_stk->push(val);

        /* if the read count is non-zero, read the data */
        if (cnt != 0)
        {
            /* read the data directly into the string buffer */
            if (src->read(str->cons_get_buf(), cnt))
                err_throw(VMERR_READ_FILE);

            /* validate the UTF-8 */
            CCharmapToUni::validate(str->cons_get_buf(), cnt);

            /* remove trailing padding */
            utf8_ptr p(str->cons_get_buf() + cnt);
            for (size_t rem = 0 ; (int)rem < cnt ; )
            {
                /* 
                 *   move to the previous character; if it's not the padding
                 *   character, we're done 
                 */
                p.dec(&rem);
                if (p.getch() != pad)
                {
                    p.inc(&rem);
                    break;
                }
            }

            /* set the string length to the length without the padding */
            str->cons_shrink_buffer(vmg_ p.getptr());
        }

        /* done with the gc protection */
        G_stk->discard();
    }

    /* we don't need a low-level wchar_t reader */
    virtual void read(wchar_t *, CVmDataSource *, int) { }
};
static CharConvUTF8 s_CharConvUTF8;

/*
 *   Base class for UCS2 converters, for 'w' and 'W' formats
 */
class CharConvUCS2: public CharConv
{
public:
    virtual void read(wchar_t *buf, CVmDataSource *src, int cnt)
    {
        /* 
         *   Read the wide characters (two bytes each).  The local wchar_t
         *   type could be bigger than two bytes, but can't be less, so we
         *   can be sure we have enough space for the file read.  
         */
        if (src->read(buf, cnt*2))
            err_throw(VMERR_READ_FILE);

        /* 
         *   Convert each character from little-endian to local byte order.
         *   The local wchar_t type could be bigger than two bytes, so work
         *   from the end of the buffer downwards - this will ensure we won't
         *   overwrite bytes before we translate them. 
         */
        for (int i = cnt - 1 ; i > 0 ; --i)
            buf[i] = file_to_machine((unsigned char *)buf + i*2);
    }

    /* UCS-2 nulls are two bytes, like every other wide character */
    virtual void null_term(CVmDataSource *dst)
    {
        if (dst->write("\0\0", 2))
            err_throw(VMERR_WRITE_FILE);
    }

    /* read a single character */
    virtual wchar_t readch(CVmDataSource *src)
    {
        /* read a two-byte character into our buffer */
        unsigned char buf[2];
        if (src->read(buf, 2))
            err_throw(VMERR_READ_FILE);

        /* return the character */
        return file_to_machine(buf);
    }

    /* 
     *   the wide-char formats treat the repeat count as a character count,
     *   so the byte count is double the repeat count 
     */
    virtual int count_to_bytes(const CVmPackType *t)
        { return t->get_count() * 2; }

    virtual long bytes_to_count(long bytes)
        { return (bytes + 1) / 2; }

protected:
    /* convert a wchar_t from file byte order to local machine byte order */
    virtual wchar_t file_to_machine(const unsigned char *ch) = 0;
};

/*
 *   Little-endian UCS2 converter, for 'w' and 'W' formats
 */
class CharConvUCS2L: public CharConvUCS2
{
public:
    virtual void putch(char *&buf, wchar_t ch, int idx, int pct)
    {
        /* UCS-2 little-endian - store the low byte first */
        *buf++ = (char)(ch & 0xff);
        *buf++ = (char)((ch >> 8) & 0xff);
    }

    virtual void pad(CVmDataSource *dst, char ch, long str_bytes,
                     const CVmPackType *t)
        { CVmPack::write_padding(dst, ch, 0, get_byte_count(t) - str_bytes); }

    virtual wchar_t file_to_machine(const unsigned char *p)
    {
        return (wchar_t)p[0] + (((wchar_t)p[1]) << 8);
    }
};
static CharConvUCS2L s_CharConvUCS2L;

/*
 *   Big-endian UCS2 converter, for 'w' and 'W' formats 
 */
class CharConvUCS2B: public CharConvUCS2
{
public:
    virtual void putch(char *&buf, wchar_t ch, int idx, int pct)
    {
        /* UCS2 big-endian - store the high byte first */
        *buf++ = (char)((ch >> 8) & 0xff);
        *buf++ = (char)(ch & 0xff);
    }

    virtual void pad(CVmDataSource *dst, char ch, long str_bytes,
                     const CVmPackType *t)
        { CVmPack::write_padding(dst, 0, ch, get_byte_count(t) - str_bytes); }

    virtual wchar_t file_to_machine(const unsigned char *p)
    {
        return (wchar_t)p[1] + (((wchar_t)p[0]) << 8);
    }
};
static CharConvUCS2B s_CharConvUCS2B;

/*
 *   Hex string converter, for 'h' and 'H' foramts 
 */
class CharConvHex: public CharConv
{
public:
    virtual void read(wchar_t *buf, CVmDataSource *src, int cnt)
    {
        /* 
         *   Read the packed bytes.  We need half a byte per hex nibble,
         *   rounding up if we want an odd number of nibbles. 
         */
        if (src->read(buf, (cnt+1)/2))
            err_throw(VMERR_READ_FILE);

        /*
         *   Convert each byte into a pair of hex nibbles.  Since the
         *   unpacked representation is larger than the bytes that we read,
         *   work from the top end of the buffer backwards. 
         */
        for (int i = cnt - 1 ; i >= 0 ; --i)
        {
            unsigned char *p = ((unsigned char *)buf) + i/2;
            buf[i] = int_to_xdigit(byte_to_nibble(*p, i & 1));
        }
    }

    virtual void putch(char *&buf, wchar_t ch, int idx, int pct)
    {
        /* get the nibble value from the character */
        int d = is_xdigit(ch) ? value_of_xdigit(ch) : 0;
        int n = nibble_to_byte(d, idx & 1);

        /* 
         *   if this is an even index, it's the first nibble of the byte, so
         *   set the whole byte to the nibble value; if it's an odd index,
         *   it's the second nibble for the byte, so OR it into the byte and
         *   increment the output pointer 
         */
        if (idx & 1)
            *buf++ |= (char)n;
        else
            *buf = (char)n;
    }

    virtual void putch_end(char *&buf, int idx)
    {
        /* 
         *   If we wrote an odd number of source characters, we've only
         *   written a partial last byte. 
         */
        if (idx & 1)
            ++buf;
    }

    /* 
     *   The repeat count for this format is in nibbles of the unpacked hex
     *   string, which corresponds to half-bytes in the packed stream.  Round
     *   up if it's odd.  
     */
    virtual int count_to_bytes(const CVmPackType *t)
        { return (t->get_count() + 1)/2; }

    virtual long bytes_to_count(long bytes)
        { return bytes * 2; }

protected:
    /* 
     *   Extract a hex nibble from a byte.  'idx' is zero for the first
     *   nibble of a pair, 1 for the second nibble of a pair, in unpacked
     *   string order.  
     */
    virtual int byte_to_nibble(int b, int idx) = 0;

    /* convert a nibble value to a byte value */
    virtual int nibble_to_byte(int n, int idx) = 0;
};

/*
 *   Hex string converter, low nibble first, for 'h' format 
 */
class CharConvHexL: public CharConvHex
{
public:
    virtual int byte_to_nibble(int b, int idx)
        { return (idx ? (b >> 4) : b) & 0x0F; }

    virtual int nibble_to_byte(int b, int idx)
        { return (idx ? (b << 4) & 0xF0 : b & 0x0F); }
};
static CharConvHexL s_CharConvHexL;

/*
 *   Hex string converter, high nibble first, for 'H' format 
 */
class CharConvHexB: public CharConvHex
{
public:
    virtual int byte_to_nibble(int b, int idx)
        { return (idx ? b : (b >> 4)) & 0x0F; }

    virtual int nibble_to_byte(int b, int idx)
        { return (idx ? b & 0x0F : (b << 4) & 0xF0); }
};
static CharConvHexB s_CharConvHexB;

/*
 *   Get the converter for a given type code 
 */
CharConv *CharConv::conv_for_type(const CVmPackType *t)
{
    switch (t->type_code)
    {
    case 'a':
    case 'A':
        return &s_CharConvLatin1;

    case 'b':
        return &s_CharConvByte;

    case 'u':
    case 'U':
        return &s_CharConvUTF8;

    case 'w':
    case 'W':
        if (t->big_endian)
            return &s_CharConvUCS2B;
        else
            return &s_CharConvUCS2L;

    case 'h':
        return &s_CharConvHexL;

    case 'H':
        return &s_CharConvHexB;

    default:
        /* it's not a string type */
        return 0;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Write the bytes for a numeric value 
 */
static void write_num_bytes(class CVmDataSource *dst,
                            char *buf, const CVmPackType *t, size_t len)
{
    /* 
     *   we always prepare our buffers in our standard little-endian byte
     *   order, so if the type has a big-endian flag, we need to reverse the
     *   byte order
     */
    if (len > 1 && t->big_endian)
        reverse_bytes(buf, len);

    /* write the bytes */
    if (dst->write(buf, len))
        err_throw(VMERR_WRITE_FILE);
}

/*
 *   Encode a BER compressed unsigned integer value.  This format stores an
 *   unsigned integer as a series of bytes representing base-128 digits.  The
 *   most significant digit is always stored first (there's no little-endian
 *   variant of this format).  Each byte except the last has its high bit
 *   (0x80) set, to indicate that more bytes follow.  We can store integers
 *   or BigNumber values in this format.  
 */
static void encode_ber(char *buf, size_t &blen, uint32_t l)
{
    /* nothing in the buffer yet */
    blen = 0;

    /* if it's zero, the result is trivial */
    if (l == 0)
    {
        buf[blen++] = 0;
        return;
    }

    /* generate the bytes of the absolute value */
    int shift, mask;
    for (shift = 28, mask = 0x08 ; shift >= 0 ; shift -= 7, mask = 0x7f)
    {
        /* get the next most significant 128-based "digit" */
        char b = (char)((l >> shift) & mask);
        
        /* if it's significant (non-zero or not the first digit), write it */
        if (b != 0 || blen != 0)
            buf[blen++] = b;
    }

    /* set the high bit in every byte except the last */
    for (size_t i = 0 ; i < blen - 1 ; ++i)
        buf[i] |= 0x80;
}

/*
 *   Decode a BER-encoded integer. 
 */
static int32_t decode_ber(const char *buf, size_t len)
{
    /* decode the bytes, starting from the least significant */
    size_t i = len;
    int shift = 0;
    int32_t val = 0;
    do
    {
        val += buf[--i] << shift;
        shift += 7;
    }
    while (i != 0);

    /* return the result */
    return val;
}

/* ------------------------------------------------------------------------ */
/*
 *   Pack an item or iterated list of items
 */
void CVmPack::pack_item(VMG_ CVmPackGroup *group, CVmPackArgs *args,
                        CVmPackType *t, CVmPackType *prefix_count)
{
    /* 
     *   Retrieve the current argument, to test if it's a list.  Note that if
     *   we're out of arguments, it's no problem - get() will fill in val
     *   with nil, and the is_listlike test on nil will return false. 
     */
    vm_val_t val;
    args->get(&val);

    /* 
     *   Note if we have an iterated repeat count.  The repeat count is the
     *   number of times to repeat the field type with subsequent arguments,
     *   except for the string types and xX@. 
     */
    int repeatable = is_repeatable(t->type_code);

    /* if we have a count prefix, the item count is implicitly '*' */
    int count = t->count;
    if (prefix_count != 0)
        count = ITER_STAR;

    /* check to see if we have a list-like argument */
    if (val.is_listlike(vmg0_))
    {
        /* it's a list - set up a list argument reader */
        ListArgs lst(vmg_ &val);

        /* if we have a type that consumes arguments, consume the list */
        if (type_consumes_arg((char)t->type_code))
            args->next();

        /* if there's a prefix count, pack it based on the list length */
        if (prefix_count != 0)
        {
            /* pack the length of the list as the count */
            SingleArg cnt(lst.len);
            pack_one_item(vmg_ group, &cnt, prefix_count, 0);
        }

        /* 
         *   Figure the number of list items to pack.  If we have an explicit
         *   numeric repeat count, loop exactly that many times.  If we no
         *   repeat count, loop once.  If we have '*' as the repeat count, or
         *   a prefix count, loop over the list length.
         */
        int iters = (count == ITER_STAR ? lst.len :
                     count < 1 ? 1 :
                     count);

        /* if this is an up-to count, limit it to the list length */
        if (t->up_to_count && iters > lst.len)
            iters = lst.len;

        /* iterate over the list */
        for (int i = 0 ; i < iters ; ++i)
            pack_one_item(vmg_ group, &lst, t, 0);
    }
    else if (repeatable && (count == ITER_STAR || count > 1))
    {
        /* 
         *   We have '*' as the repeat count, which means that we repeat this
         *   item for the remaining arguments at this level.
         */
        if (prefix_count != 0)
        {
            /* write the number of remaining arguments */
            SingleArg cntarg(args->nrem());
            pack_one_item(vmg_ group, &cntarg, prefix_count, 0);
        }

        /* pack items until we're out of arguments or reach the repeat count */
        int n;
        for (n = 0 ; count == ITER_STAR ? args->more() : n < count ; ++n)
        {
            /* if this is an up-to count and we're out of arguments, stop */
            if (t->up_to_count && !args->more())
                break;

            /* pack the item */
            pack_one_item(vmg_ group, args, t, 0);
        }

        /* if it's an up-to count, consume the rest of the arguments */
        if (t->up_to_count)
            for ( ; args->more() ; args->next()) ;
    }
    else
    {
        /* 
         *   Either there's no prefix count at all, in which case we're
         *   writing a singleton; or there's a prefix count for a string
         *   value, in which case the prefix is the string length.  In either
         *   case, we have a single item to write rather than an iterated
         *   list.  
         */
        pack_one_item(vmg_ group, args, t, prefix_count);
    }
}

/*
 *   Figure the padding for alignment to a given size boundary 
 */
static int alignment_padding(const CVmPackType *t, const CVmPackGroup *group,
                             int dir)
{
    /* get the alignment size; if one or less, add no padding */
    int c = t->count;
    if (c <= 1)
        return 0;

    /* get the current offset from the root starting position */
    long ofs = group->ds->get_pos() - group->root_stream_ofs();

    /* if we're already at a multiple of the size, no padding is needed */
    if (ofs % c == 0)
        return 0;

    /* figure padding in the desired direction */
    if (dir > 0)
    {
        /* forward padding - pad to the next multiple of 'c' */
        return c - (int)(ofs % c);
    }
    else
    {
        /* reverse padding - pad to the previous multiple of 'c' */
        return -(int)(ofs % c);
    }
}

/*
 *   Pack a single item
 */
void CVmPack::pack_one_item(VMG_ CVmPackGroup *group, CVmPackArgs *args,
                            CVmPackType *t, CVmPackType *prefix_count)
{
    vm_val_t val, cval;
    int len;
    char buf[256];
    CVmDataSource *dst = group->ds;

    /* check for non-argument codes */
    switch (t->type_code)
    {
    case 'x':
        /* Nul byte(s) */
        if (t->bang)
            write_padding(dst, '\0', alignment_padding(t, group, 1));
        else
            write_padding(dst, '\0', t->get_count());
        return;

    case 'X':
        /* back up by 'count' bytes */
        if (t->bang)
            dst->seek(alignment_padding(t, group, -1), OSFSK_CUR);
        else
            dst->seek(-t->get_count(), OSFSK_CUR);
        return;

    case '"':
        /* literal bytes */
        len = 0;
        for (CVmPackPos p(&t->lit) ; p.len != 0 ; p.inc())
        {
            /* get the next character */
            wchar_t ch = p.getch_raw();

            /* if it's a quote, it must be stuttered, so skip one more */
            if (ch == '"')
                p.inc();

            /* if it's outside the 0-255 range, it's an error */
            if (ch > 255)
                err_throw(VMERR_NUM_OVERFLOW);

            /* write the charater */
            buf[len++] = (char)ch;

            /* flush the buffer if it's full */
            if (len == sizeof(buf))
            {
                dst->write(buf, len);
                len = 0;
            }
        }

        /* write the final buffer */
        if (len != 0)
            dst->write(buf, len);

        /* done */
        return;

    case '{':
        /* in-line hex bytes */
        len = 0;
        for (CVmPackPos p(&t->lit) ; p.more() ; )
        {
            /* get the next two hex digits */
            int b = (value_of_xdigit(p.nextch()) << 4);
            b += value_of_xdigit(p.nextch());

            /* add it to the buffer */
            buf[len++] = (char)b;

            /* flush the buffer if it's full */
            if (len == sizeof(buf))
            {
                dst->write(buf, len);
                len = 0;
            }
        }

        /* write the final buffer */
        if (len != 0)
            dst->write(buf, len);

        /* done */
        return;
        
    case '@':
        /* Nul-fill to the absolute position; @? does nothing on packing */
        if (!t->qu)
        {
            /* 
             *   Figure the seek position as an offset from the current file
             *   position.  If there's a !, it's relative to the start of the
             *   whole string, otherwise relative to the start of the current
             *   group. 
             */
            long l = (t->bang ? group->root_stream_ofs() : group->stream_ofs)
                     + t->get_count() - dst->get_pos();

            /* if positive, write Nul bytes; if negative, back up */
            if (l > 0)
                write_padding(dst, '\0', l);
            else if (l < 0)
                dst->seek(l, OSFSK_CUR);
        }
        return;
    }

    /* note if it's upper or lower case */
    int lc = is_lower(t->type_code);

    /* retrieve and consume the next argument */
    args->get(&val);
    args->next();

    /* check for a character string conversion */
    CharConv *cvtchar = CharConv::conv_for_type(t);
    if (cvtchar != 0)
    {
        /* create the field data source based on the object type */
        CharFieldSource *src = CharFieldSource::create(vmg_ &val, &cval);
        
        err_try
        {
            /* push any new value created by the cast, for gc protection */
            G_stk->push(&cval);
            
            /* if there's an prefix count, write the string length */
            if (prefix_count != 0)
            {
                /* write the string length */
                SingleArg alen(src->arraylen(t));
                pack_one_item(vmg_ group, &alen, prefix_count, 0);
            }

            /*
             *   Figure the byte length of the write.  The mapping between
             *   the repeat count and the byte length varies by format, so
             *   ask the format handler.  If we have a prefix count or a '*'
             *   repeat count, though, we simply write the exact length of
             *   the string.
             */
            int nbytes;
            if (prefix_count != 0
                || t->count == ITER_STAR
                || (t->count == ITER_NONE && t->null_term))
            {
                /* prefix count, *, or null-term - use the actual length */
                nbytes = ITER_STAR;
            }
            else
            {
                /* specific count - translate characters to bytes */
                nbytes = cvtchar->get_byte_count(t);
            }

            /* convert the string to the specified representation */
            long tot;
            char *p;
            int srcidx;
            for (tot = 0, srcidx = 0, p = buf ; src->more() ; )
            {
                /* 
                 *   If there's not room for another character in the buffer,
                 *   flush the buffer.  To make things easy, we assume the
                 *   conversion will require the most space of any of our
                 *   conversions, which is three bytes for some utf8
                 *   characters.  
                 */
                if (p - buf > sizeof(buf) - 3)
                {
                    /* flush the buffer */
                    if (dst->write(buf, p - buf))
                        err_throw(VMERR_WRITE_FILE);

                    /* start over at the beginning of the buffer */
                    p = buf;
                }
                
                /* add the next character */
                char *prvp = p;
                cvtchar->putch(p, src->getch(), srcidx++, t->pct);
                
                /* add this to the running total */
                tot += p - prvp;
                
                /* 
                 *   If this would push us over the repeat count, back out
                 *   this character.  
                 */
                if (nbytes > 0 && tot > nbytes)
                {
                    /* back it out and stop adding */
                    tot -= p - prvp;
                    p = prvp;
                    break;
                }
            }

            /* close out the buffer */
            cvtchar->putch_end(p, srcidx);
            
            /* if there's anything in the buffer, flush it */
            if (p > buf && dst->write(buf, p - buf))
                err_throw(VMERR_WRITE_FILE);
            
            /* 
             *   If we have a repeat count, and we didn't reach the maximum
             *   length, add the padding.  Skip this if it's an up-to count,
             *   since an up-to count only packs up to the actual length if
             *   that's less than the count limit.
             */
            if (tot < nbytes && !t->up_to_count)
            {
                /* 
                 *   figure the padding - use space padding for A, U, and W,
                 *   and nul padding for everything else 
                 */
                char pad = get_padding_char(t->type_code);
                
                /* write it out */
                cvtchar->pad(dst, pad, tot, t);
            }

            /* if it's null-terminated, write the null character */
            if (t->null_term)
                cvtchar->null_term(dst);
            
            /* discard the gc protection */
            G_stk->discard();
        }
        err_finally
        {
            /* delete the field data source object */
            delete src;
        }
        err_end;

        /* done */
        return;
    }

    /* it's not a character string type; check other types */
    switch (t->type_code)
    {
    case 'c':
        /* signed byte */
        buf[0] = (char)intval(vmg_ &val, -128, 127, t->pct, 0xff);
        write_num_bytes(dst, buf, t, 1);
        break;
        
    case 'C':
        /* unsigned byte */
        buf[0] = (char)intval(vmg_ &val, 0, 255, t->pct, 0xff);
        write_num_bytes(dst, buf, t, 1);
        break;

    case 's':
        /* signed short (16 bits) */
        oswp2s(buf, intval(vmg_ &val, -32768, 32767, t->pct, 0xffff));
        write_num_bytes(dst, buf, t, 2);
        break;
        
    case 'S':
        /* unsigned short (16 bits) */
        oswp2(buf, intval(vmg_ &val, 0, 65535, t->pct, 0xffff));
        write_num_bytes(dst, buf, t, 2);
        break;        

    case 'l':
        /* signed long (32 bits) */
        oswp4s(buf, intval(vmg_ &val, -2147483647-1, 2147483647,
                           t->pct, 0xffffffff));
        write_num_bytes(dst, buf, t, 4);
        break;
        
    case 'L':
        /* 
         *   Unsigned long (32 bits).  TADS doesn't have unsigned longs as a
         *   VM datatype, but a BigNumber can hold a value above LONG_MAX.
         *   So if we have a BigNumber or a string, get the BigNumber value
         *   and see if it fits in a 32-bit unsigned long.  Otherwise, it has
         *   to be a positive integer.  
         */
        if ((val.typ == VM_OBJ
             && CVmObjBigNum::is_bignum_obj(vmg_ val.val.obj))
            || val.get_as_string(vmg0_) != 0)
        {
            /* cast it to a BigNumber */
            CVmObjBigNum::cast_to_bignum(vmg_ &cval, &val);

            /* convert to ulong */
            int ov;
            unsigned long l = ((CVmObjBigNum *)vm_objp(vmg_ cval.val.obj))
                              ->convert_to_uint(ov);

            /* make sure it fits in a 32-bit long (unless '%' is set) */
            if (ov || l > 4294967295U)
            {
                if (t->pct)
                    l &= 0xffffffff;
                else
                    err_throw(VMERR_NUM_OVERFLOW);
            }

            /* save it */
            oswp4(buf, l);
            write_num_bytes(dst, buf, t, 4);
        }
        else
        {
            /* 
             *   It's not a BigNumber, so just cast to int.  Since TADS
             *   doesn't have an unsigned 32-bit type, we have to limit the
             *   range to positive integers.  
             */
            oswp4(buf, intval(vmg_ &val, 0, 2147483647, t->pct, 0xffffffff));
            write_num_bytes(dst, buf, t, 4);
        }
        break;

    case 'q':
    case 'Q':
        /* 
         *   Long-long (64-bit) types.  We can source these values from
         *   integers or BigNumber values.  
         */
        if (val.typ == VM_INT)
        {
            /* 
             *   Integer value.  Write the low-order 32 bits, and sign-extend
             *   into the high-order 32 bits.  The sign extension is all zero
             *   bits for a positive or zero value, and all 1 bits (0xff
             *   bytes) for a negative value.  An unsigned value is
             *   inherently non-negative, so the 1-bit extension applies only
             *   to negative signed values.  
             */
            unsigned char sign_ext = 0;
            if (lc)
            {
                /* signed - write the low-order 32 bits, little-endian */
                oswp4s(buf, val.val.intval);

                /* if it's negative, sign-extend with 0xff bytes */
                if (val.val.intval < 0)
                    sign_ext = 0xff;
            }
            else
            {
                /* unsigned - make sure it's not negative */
                if (val.val.intval < 0 && !t->pct)
                    err_throw(VMERR_NUM_OVERFLOW);

                /* write the low-order 32-bits, little-endian, unsigned */
                oswp4(buf, val.val.intval);
            }

            /* sign-extend or zero-extend into the high-order 32 bits */
            memset(buf + 4, sign_ext, 4);
        }
        else if (val.typ == VM_NIL)
        {
            /* nil - write 0 */
            memset(buf, 0, 8);
        }
        else
        {
            /* it's not an int, so cast it to BigNumber */
            CVmObjBigNum::cast_to_bignum(vmg_ &cval, &val);

            /* write the 8-byte portable little-endian value */
            int ov;
            if (lc)
                ((CVmObjBigNum * )vm_objp(vmg_ cval.val.obj))->wp8s(buf, ov);
            else
                ((CVmObjBigNum * )vm_objp(vmg_ cval.val.obj))->wp8(buf, ov);

            /* check for overflow */
            if (ov && !t->pct)
                err_throw(VMERR_NUM_OVERFLOW);
        }

        /* write the buffer */
        write_num_bytes(dst, buf, t, 8);
        break;

    case 'k':
        {
            /* BER compressed integer */
            size_t blen = 0;
            if (val.typ == VM_INT)
            {
                /* make sure it's positive, or that 'pct' is set */
                long l = val.val.intval;
                if (l < 0)
                {
                    /* 
                     *   if % is set, use the absolute value; otherwise it's
                     *   an error 
                     */
                    if (t->pct)
                        l = -l;
                    else
                        err_throw(VMERR_NUM_OVERFLOW);
                }

                /* encode the value */
                encode_ber(buf, blen, l);
            }
            else if (val.typ == VM_NIL)
            {
                /* encode 0 */
                buf[blen++] = 0;
            }
            else
            {
                /* it's not an int, so cast it to BigNumber */
                CVmObjBigNum::cast_to_bignum(vmg_ &cval, &val);

                /* encode the BigNumber in BER format */
                int ov;
                ((CVmObjBigNum *)vm_objp(vmg_ cval.val.obj))->encode_ber(
                    buf, sizeof(buf), blen, ov);

                /* check for overflow */
                if (ov && !t->pct)
                    err_throw(VMERR_NUM_OVERFLOW);
            }

            /* write the buffer */
            if (dst->write(buf, blen))
                err_throw(VMERR_WRITE_FILE);
        }
        break;
        
    case 'f':
        len = 4;
        goto float_fmt;
        
    case 'd':
        len = 8;

    float_fmt:
        /* float/double - cast to BigNumber */
        if (val.typ == VM_NIL)
        {
            /* nil - return as zero, which is all 0 bits in all formats */
            memset(buf, 0, len);
        }
        else
        {
            /* cast the value to BigNumber */
            CVmObjBigNum::cast_to_bignum(vmg_ &cval, &val);
            G_stk->push(&cval);

            /* convert to the 32-bit or 64-bit IEEE 754-2008 format */
            int ov;
            ((CVmObjBigNum *)vm_objp(vmg_ cval.val.obj))
                ->convert_to_ieee754(vmg_ buf, len*8, ov);

            /* check for overflow */
            if (ov && !t->pct)
                err_throw(VMERR_NUM_OVERFLOW);

            /* discard the gc protection */
            G_stk->discard();
        }

        /* write out the result */
        write_num_bytes(dst, buf, t, len);
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Unpack data from the given stream into a list 
 */
void CVmPack::unpack(VMG_ vm_val_t *retval, const char *fmt, size_t fmtlen,
                     CVmDataSource *src)
{
    /* 
     *   Set up a list for the return value.  We don't know how many elements
     *   we'll need, so allocate an initial chunk now and expand later as
     *   needed.  (We can't necessarily know the return size even by
     *   pre-parsing the format string, since it could have variable-size
     *   items with array counts.)  
     */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, 10));
    CVmObjList *retlst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* 
     *   since we're going to build the list iteratively, clear it out, so
     *   that it doesn't contain any uninitialized elements that the garbage
     *   collector might encounter 
     */
    retlst->cons_clear();

    /* save it on the stack for gc protection */
    G_stk->push(retval);

    /* we don't have any elements in our list yet */
    int retcnt = 0;

    /* unpack the root group */
    CVmPackPos p(fmt, fmtlen);
    CVmPackGroup root(&p, src);
    unpack_group(vmg_ &p, retlst, &retcnt, &root, FALSE);

    /* make sure we made it all the way through the format string */
    if (p.more())
        err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p.index());

    /* set the return list's final length */
    retlst->cons_set_len(retcnt);

    /* discard the gc protection */
    G_stk->discard();
}

/*
 *   Unpack a group
 */
void CVmPack::unpack_group(VMG_ CVmPackPos *p,
                           CVmObjList *retlst_par, int *retcnt_par,
                           CVmPackGroup *group, int list_per_iter)
{
    /* get the data source */
    CVmDataSource *src = group->ds;

    /* 
     *   If the group has an "up to" count (e.g., "30*"), check to see if
     *   we're already at EOF, and if so don't unpack anything - an iteration
     *   count of zero is valid for an up-to count. 
     */
    if ((group->num_iters == ITER_STAR || group->up_to_iters)
        && src->get_pos() >= src->get_size())
    {
        /* skip the group in the source */
        p->set(&group->start);
        skip_group(p, group->close_paren);

        /* we're done */
        return;
    }

    /* assume we'll unpack directly into the parent list */
    CVmObjList *retlst = retlst_par;
    int *retcnt = retcnt_par;

    /* 
     *   if we're unpacking into a per-instance sublist, create the sublist
     *   for the first iteration, and set this as the output list instead of
     *   unpacking directly into the parent list
     */
    int grpcnt = 0;
    if (list_per_iter)
    {
        retlst = create_group_sublist(vmg_ retlst_par, retcnt_par);
        retcnt = &grpcnt;
    }

    /* run through the format list */
    while (p->more())
    {
        /* check for group entry/exit and repeat counts */
        wchar_t c = p->getch();
        if (c == '(' || c == '[')
        {
            /* start of a group - unpack the sub-group */
            unpack_subgroup(vmg_ p, retlst, *retcnt, group, 0);
        }
        else if (c == group->close_paren)
        {
            /* end of group - count the completed iteration */
            group->cur_iter++;

            /* 
             *   if we're unpacking into a per-iteration sublist, note the
             *   final group count in the sublist 
             */
            if (list_per_iter)
                retlst->cons_set_len(*retcnt);

            /* 
             *   We're done with the group if (a) we have a '*' iteration
             *   count, and we're at end of file, or (b) we've reached the
             *   desired iteration count.  If done, simply return to the
             *   enclosing group. 
             */
            if (group->num_iters == ITER_STAR
                ? src->get_pos() >= src->get_size()
                : group->cur_iter > group->num_iters)
                return;

            /* 
             *   if we have an 'up to' iteration count, stop if we're at EOF
             *   even if we haven't exhausted the numeric iteration count 
             */
            if (group->up_to_iters && src->get_pos() >= src->get_size())
                return;

            /* return to the start of the group template for the next round */
            p->set(&group->start);
            group->stream_ofs = group->ds->get_pos();

            /* 
             *   If we're unpacking into a sublist per iteration, create the
             *   new sublist for the new iteration. 
             */
            if (list_per_iter)
            {
                retlst = create_group_sublist(vmg_ retlst_par, retcnt_par);
                grpcnt = 0;
            }
        }
        else if (c == '/')
        {
            /*
             *   Repeat count prefix.  Remove the preceding return list
             *   element and interpret it as an integer.  Make sure there
             *   actually is an item in the list - it's an error if not.  
             */
            if (retcnt == 0)
                err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p->index());
            
            /* remove the item and cast it to integer */
            vm_val_t cntval;
            retlst->get_element(--(*retcnt), &cntval);
            int cnt = (int)cntval.cast_to_int(vmg0_);

            /* skip the '/' and check what follows */
            p->inc();
            if ((c = p->getch()) == '(' || c == '[')
            {
                /* it's a group - unpack the group with the count */
                unpack_subgroup(vmg_ p, retlst, *retcnt, group, &cnt);
            }
            else
            {
                /* it's a single item - unpack the repeated items */
                unpack_iter_item(vmg_ p, retlst, *retcnt, group, &cnt);
            }
        }
        else
        {
            /* it's a regular item, possibly iterated - unpack it */
            unpack_iter_item(vmg_ p, retlst, *retcnt, group, 0);
        }
    }
}

/*
 *   Unpack a subgroup 
 */
void CVmPack::unpack_subgroup(VMG_ CVmPackPos *p,
                              CVmObjList *retlst, int &retcnt,
                              CVmPackGroup *group, const int *prefix_count)
{
    /* group - parse the group modifiers */
    CVmPackType gt(group);
    parse_group_mods(p, &gt);

    /* note the open paren type */
    wchar_t paren = p->getch();

    /* skip the paren and set up the child group */
    p->inc();
    CVmPackGroup child(group, paren, p, &gt);

    /* if there's a prefix count, it overrides the suffix count */
    if (prefix_count != 0)
        child.num_iters = *prefix_count;

    /*
     *   If the group started with '[', it means that it's to be unpacked
     *   into a list.  Otherwise, it's unpacked directly into the enclosing
     *   list.  Create a new list if necessary.  
     */
    if (paren == '[')
    {
        /* 
         *   if there's a '!' modifier, we unpack each iteration into a
         *   separate list; otherwise we unpack the whole group into a single
         *   list 
         */
        if (gt.bang)
        {
            /* 
             *   unpack into individual per-iteration sublists, each of which
             *   goes into the main list 
             */
            unpack_group(vmg_ p, retlst, &retcnt, &child, TRUE);
        }
        else
        {
            /* create the result list for the whole group */
            CVmObjList *grplst = create_group_sublist(vmg_ retlst, &retcnt);
            int grpcnt = 0;
            
            /* unpack into the sublist */
            unpack_group(vmg_ p, grplst, &grpcnt, &child, FALSE);
            
            /* set the final number of elements in the list */
            grplst->cons_set_len(grpcnt);
        }
    }
    else
    {
        /* no sublist - unpack into the main list */
        unpack_group(vmg_ p, retlst, &retcnt, &child, FALSE);
    }
    
    /* make sure we stopped at the close paren */
    if (p->nextch() != child.close_paren)
        err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p->index());
    
    /* skip any modifiers for the group */
    parse_mods(p, &gt);
}

/*
 *   Create a sublist for unpacking a "[ ]" group
 */
CVmObjList *CVmPack::create_group_sublist(
    VMG_ CVmObjList *parent_list, int *parent_cnt)
{
    /* create the new sublist */
    vm_val_t ele;
    ele.set_obj(CVmObjList::create(vmg_ FALSE, 10));
    CVmObjList *sublst = (CVmObjList *)vm_objp(vmg_ ele.val.obj);

    /* clear it, since we'll be filling it in iteratively */
    sublst->cons_clear();

    /* 
     *   Add it to the parent list.  The parent list is on the stack (or in a
     *   list that's on the stack, or in a list that's in a list on the
     *   stack, etc) - that will protect the new list from premature garbage
     *   collection.
     */
    parent_list->cons_ensure_space(vmg_ *parent_cnt, 10);
    parent_list->cons_set_element((*parent_cnt)++, &ele);

    /* return the sublist */
    return sublst;
}


/* ------------------------------------------------------------------------ */
/*
 *   Unpack an iterated item 
 */
void CVmPack::unpack_iter_item(VMG_ CVmPackPos *p,
                               CVmObjList *retlst, int &retcnt,
                               CVmPackGroup *group, const int *prefix_count)
{
    int len;
    
    /* parse the template item */
    CVmPackType t;
    parse_type(p, &t, group);

    /* get the data source */
    CVmDataSource *src = group->ds;

    /* check for non-value types and string types */
    int is_string_type = FALSE;
    switch (t.type_code)
    {
    case 'x':
        /* Nul bytes: on input, just skip bytes */
        if (t.bang)
            src->seek(alignment_padding(&t, group, 1), OSFSK_CUR);
        else
            src->seek(t.get_count(), OSFSK_CUR);
        return;

    case 'X':
        /* back up by 'count' bytes */
        if (t.bang)
            src->seek(alignment_padding(&t, group, -1), OSFSK_CUR);
        else
            src->seek(-t.get_count(), OSFSK_CUR);
        return;

    case '"':
        /* count the characters in the string */
        len = 0;
        for (CVmPackPos p(&t.lit) ; p.len != 0 ; p.inc(), ++len)
        {
            /* if this is a quote, it must be stuttered, so skip it */
            if (p.getch_raw() == '"')
                p.inc();
        }

        /* skip the bytes, and we're done */
        src->seek(len * t.get_count(), OSFSK_CUR);
        return;

    case '{':
        /* count the hex digit pairs */
        len = 0;
        for (CVmPackPos p(&t.lit) ; p.more() ; ++len, p.inc(), p.inc()) ;

        /* skip the bytes, and we're done */
        src->seek(len * t.get_count(), OSFSK_CUR);
        return;

    case '@':
        /* seek to position or retrieve current position */
        if (t.qu)
        {
            /* @? retrieves the current position as an output value */
            unpack_into_list(vmg_ retlst, retcnt, src, &t, group);
        }
        else
        {
            /* 
             *   Absolute positioning: seek to the starting position plus the
             *   specified offset.  The starting position is the current
             *   group's starting position, or the whole string's start if
             *   the !  qualifier is present.  
             */
            src->seek((t.bang ? group->root_stream_ofs() : group->stream_ofs)
                      + t.get_count() - src->get_pos(),
                      OSFSK_CUR);
        }
        return;

    case 'a':
    case 'A':
    case 'b':
    case 'u':
    case 'U':
    case 'w':
    case 'W':
    case 'h':
    case 'H':
        /* make a note that this is a string type */
        is_string_type = TRUE;
        break;
    }

    /*
     *   We have a regular unpacking type (not one of the special positioning
     *   codes).  If there's a prefix count, it overrides any count specified
     *   in the type.  
     */
    if (prefix_count != 0)
        t.count = *prefix_count;

    /* 
     *   If it's a string type, the repeat count specifies the length of the
     *   string in the file (in bytes, characters, nibbles, or other units
     *   that vary by template type).  Otherwise, the repeat count is the
     *   number of copies of the item to unpack.  
     */
    if (is_string_type)
    {
        /* 
         *   It's a string type.  The count doesn't specify the number of
         *   strings to write, but rather specifies the number of bytes to
         *   read into one string.  
         */
        unpack_into_list(vmg_ retlst, retcnt, src, &t, group);
    }
    else if (t.count == ITER_STAR)
    {
        /* unpack this item repeatedly until we run out of source */
        long src_size = src->get_size();
        while (src->get_pos() < src_size)
            unpack_into_list(vmg_ retlst, retcnt, src, &t, group);
    }
    else if (t.up_to_count)
    {
        /* 
         *   unpack this item the number of times given by the repeat count,
         *   but stop if we reach EOF first
         */
        long src_size = src->get_size();
        for (int i = 0 ; i < t.get_count() && src->get_pos() < src_size ; ++i)
            unpack_into_list(vmg_ retlst, retcnt, src, &t, group);
    }
    else
    {
        /* unpack this item the number of times given by the repeat count */
        for (int i = 0 ; i < t.get_count() ; ++i)
            unpack_into_list(vmg_ retlst, retcnt, src, &t, group);
    }
}

/*
 *   Unpack an item and add it to the list 
 */
void CVmPack::unpack_into_list(VMG_ CVmObjList *retlst, int &retcnt,
                               CVmDataSource *src, const CVmPackType *t,
                               const CVmPackGroup *group)
{
    /* unpack the item */
    vm_val_t ele;
    unpack_item(vmg_ &ele, src, t, group);

    /* add it to the list */
    retlst->cons_ensure_space(vmg_ retcnt, 10);
    retlst->cons_set_element(retcnt++, &ele);
}

/* ------------------------------------------------------------------------ */
/*
 *   Read a numeric (int/float/double) buffer, normalizing into little-endian
 *   order.
 */
static void read_num_bytes(char *buf, CVmDataSource *src, size_t len,
                           const CVmPackType *t)
{
    /* read the bytes */
    if (src->read(buf, len))
        err_throw(VMERR_READ_FILE);

    /* 
     *   if the source is in big-endian order, swap the bytes to get our
     *   standard little-endian order 
     */
    if (len > 1 && t->big_endian)
        reverse_bytes(buf, len);
}


/* ------------------------------------------------------------------------ */
/*
 *   Unpack one item 
 */
void CVmPack::unpack_item(VMG_ vm_val_t *val,
                          CVmDataSource *src, const CVmPackType *t,
                          const CVmPackGroup *group)
{
    /* check for a character conversion */
    CharConv *cvtchar = CharConv::conv_for_type(t);
    if (cvtchar != 0 && t->null_term && t->count <= 0)
    {
        /* 
         *   It's a null-terminated string type without a fixed field width.
         *   Use the special unpacking method for this type.  
         */
        cvtchar->unpackz(vmg_ val, src);

        /* done */
        return;
    }
    if (cvtchar != 0)
    {
        /* character string - the repeat count is the length */
        int count = t->get_count();

        /* if the count is in bytes, convert it */
        if (t->count_in_bytes)
            count = cvtchar->bytes_to_count(count);

        /* if the character count is zero, return an empty string */
        if (count == 0)
        {
            val->set_obj(CVmObjString::create(vmg_ FALSE, 0));
            return;
        }

        /* if it's '*', we read the rest of the data source */
        if (t->count == ITER_STAR || t->up_to_count)
        {
            /* 
             *   get the remaining length, and convert it to a repeat count
             *   appropriate to the string conversion type 
             */
            long l = cvtchar->bytes_to_count(src->get_size() - src->get_pos());

            /* use 'l' as a maximum for 'n*', or the actual count for '*' */
            if (t->count == ITER_STAR)
            {
                /* 
                 *   '*' means read the whole rest of the file - that's a
                 *   long value, so cast it to int, making sure we don't
                 *   overflow 
                 */
                count = (int)l;
                if (l > INT_MAX)
                    err_throw(VMERR_NUM_OVERFLOW);
            }
            else
            {
                /* 
                 *   'n*' means read up to n, stopping at EOF, so stop at 'l'
                 *   if it's less than the specified count 
                 */
                if (l < count)
                    count = (int)l;
            }
        }

        /* figure the padding type */
        char pad = get_padding_char(t->type_code);

        /* unpack the string */
        cvtchar->unpack(vmg_ val, src, count, pad);

        /* check for null termination */
        if (t->null_term)
        {
            /* skip the null, which is tacked on after the fixed length */
            cvtchar->readch(src);

            /* terminate the string at the first null, if present */
            if (val->typ == VM_OBJ
                && CVmObjString::is_string_obj(vmg_ val->val.obj))
            {
                /* get the string */
                CVmObjString *str = (CVmObjString *)vm_objp(vmg_ val->val.obj);

                /* scan for a null byte */
                char *nul = (char *)memchr(
                    str->cons_get_buf(), 0, str->cons_get_len());

                /* if we found a null byte, terminate the string there */
                if (nul != 0)
                    str->cons_shrink_buffer(vmg_ nul);
            }
        }

        /* done */
        return;
    }

    /* it's not a string; check the numeric types */
    char buf[256];
    switch (t->type_code)
    {
    case '@':
        /* 
         *   @? retrieves the current position, relative to the current group
         *   or, if ! is specified, relative to the start of the string
         */
        val->set_int(src->get_pos() -
                     (t->bang ? group->root_stream_ofs() : group->stream_ofs));
        break;
        
    case 'c':
        /* signed byte */
        read_num_bytes(buf, src, 1, t);
        val->set_int((char)buf[0]);
        break;

    case 'C':
        /* unsigned byte */
        read_num_bytes(buf, src, 1, t);
        val->set_int((unsigned char)buf[0]);
        break;

    case 's':
        /* signed 16-bit short */
        read_num_bytes(buf, src, 2, t);
        val->set_int(osrp2s(buf));
        break;
        
    case 'S':
        /* unsigned 16-bit short */
        read_num_bytes(buf, src, 2, t);
        val->set_int((unsigned short)osrp2(buf));
        break;
        
    case 'l':
        /* signed 32-bit long */
        read_num_bytes(buf, src, 4, t);
        val->set_int(osrp4s(buf));
        break;
        
    case 'L':
        /*
         *   Unsigned 32-bit long.  The VM_INT type is a signed long, so we
         *   can't use this to represent the full range of unsigned longs.
         *   Instead, use BigNumber.  Note that we could check the file value
         *   and use VM_INT if the value actually fits (i.e., it's less than
         *   0x7fffffff); this might be desirable in some cases because
         *   VM_INT is much more efficient than BigNumber.  However, since
         *   the caller is asking for type 'L', they're clearly expecting the
         *   full unsigned 32-bit range, which might mean they plan to do
         *   further arithmetic on the value we read, in which case they'd
         *   expect this arithmetic to be safe on unsigned 32-bit values.
         *   The only way we can guarantee this is to use a BigNumber for the
         *   value regardless of whether or not it fits in a VM_INT.
         *   
         *   If the ~ qualifier is set, unpack into an int if the value fits
         *   in a signed 32-bit int.  
         */
        {
            read_num_bytes(buf, src, 4, t);
            unsigned long lu = osrp4(buf);
            if (t->tilde && lu <= 0x7fffffff)
                val->set_int((long)lu);
            else
                val->set_obj(CVmObjBigNum::createu(vmg_ FALSE, lu, 10));
        }
        break;

    case 'q':
        /* 
         *   "Long long" 64-bit integer, signed.  There's no VM_xxx primitive
         *   type that can handle a value this large, so use BigNumber.  Read
         *   the 8-byte file value.
         *   
         *   If the ~ qualifier is set, and the high part of the value is all
         *   zeros or all '1' bits, return it as an integer.  Note that the
         *   buffer is in our standard osrp8 format, which is little-endian
         *   2's complement, so we can inspect the byte values portably.  
         */
        read_num_bytes(buf, src, 8, t);
        if (t->tilde
            && ((memcmp(buf+4, "\0\0\0\0", 4) == 0
                 && (buf[3] & 0x80) == 0)
                || (memcmp(buf+4, "\377\377\377\377", 4) == 0
                    && (buf[3] & 0x80) != 0)))
            val->set_int(osrp4s(buf));
        else
            val->set_obj(CVmObjBigNum::create_rp8s(vmg_ FALSE, buf));
        break;

    case 'Q':
        /* 
         *   "Long long" 64-bit integer, unsigned.  Unpack as a BigNumber,
         *   unless the ~ qualifier is set and the value will fit in an int.
         */
        read_num_bytes(buf, src, 8, t);
        if (t->tilde
            && memcmp(buf+4, "\0\0\0\0", 4) == 0
            && (buf[3] & 0x80) == 0)
            val->set_int(osrp4s(buf));
        else
            val->set_obj(CVmObjBigNum::create_rp8(vmg_ FALSE, buf));
        break;

    case 'k':
        /* 
         *   BER compressed unsigned integer.  This is a sequence of 7-bit,
         *   base-128 "digits", most significant digit first.  The high bit
         *   (0x80) is set on each digit except the last, which tells us
         *   where the value ends.  Read bytes one at a time until we reach
         *   the last byte or run out of buffer space.  
         */
        {
            size_t i;
            for (i = 0 ; ; )
            {
                /* if we're out of buffer space, throw an error */
                if (i >= sizeof(buf))
                    err_throw(VMERR_NUM_OVERFLOW);

                /* read the next byte */
                if (src->read(buf + i, 1))
                    err_throw(VMERR_READ_FILE);

                /* if the high bit isn't set, this is the last byte */
                if ((buf[i++] & 0x80) == 0)
                    break;

                /* keep just the low-order 7 bits of the value */
                buf[i-1] &= 0x7F;
            }

            /* 
             *   If the value is 31 bits or fewer, return it as an integer.
             *   Otherwise, return it as a BigNumber.  31 bits fits in 4
             *   bytes (7*4=28) plus 3 bits of the MSB, so if we have more
             *   than 5 bytes, or any of bits 4-8 of the MSB are set, we need
             *   a BigNumber.  (Why 31 bits and not 32?  Because BER values
             *   are unsigned, and our int32 type is signed, meaning it only
             *   holds 31 bits for a positive value.)  
             */
            if (i < 5 || (i == 5 && (buf[0] & 0xF8) == 0))
            {
                /* it fits in a 32-bit integer - decode it */
                val->set_int(decode_ber(buf, i));
            }
            else
            {
                /* more than 31 bits - decode into a BigNumber */
                val->set_obj(CVmObjBigNum::create_from_ber(vmg_ FALSE, buf, i));
            }
        }
        break;
        
    case 'f':
        /* 
         *   Float - read the 32-bit float value, and return it as a
         *   BigNumber value.  Our standard IEEE 754-2008 32-bit format
         *   represents 7.22 decimal digits of precision, so keep 8 digits.  
         */
        read_num_bytes(buf, src, 4, t);
        val->set_obj(CVmObjBigNum::create_from_ieee754(vmg_ FALSE, buf, 32));
        break;
        
    case 'd':
        /* 
         *   Double - read the 64-bit double value, and return it as a
         *   BigNumber value.  Our standard IEEE 754-2008 64-bit format
         *   represents 15.95 decimal digits of precision.  
         */
        read_num_bytes(buf, src, 8, t);
        val->set_obj(CVmObjBigNum::create_from_ieee754(vmg_ FALSE, buf, 64));
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a type code 
 */
void CVmPack::parse_type(CVmPackPos *p, CVmPackType *info,
                         const CVmPackGroup *group)
{
    /* get the type code */
    wchar_t c = p->nextch();
    if (!is_valid_type(c))
        err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p->index() - 1);

    /* set the format string index */
    info->fmtidx = p->index() - 1;

    /* this is the type code - save it */
    info->type_code = c;

    /* 
     *   if it's a quote, parse the characters to the next quote; if it's an
     *   open brace, parse the hex digits to the close brace 
     */
    if (c == '"')
    {
        /* note the start of the literal section */
        info->lit.set(p);
        
        /* scan for the closing quote */
        for ( ; ; p->inc())
        {
            /* if at end of string, it's an error */
            if (!p->more())
                err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p->index());

            /* check for the close quote */
            if (p->getch() == '"')
            {
                /* this might be it - note the length if so */
                size_t len = info->lit.len - p->len;

                /* skip it */
                p->inc();

                /* if it's not a stuttered quote, it's really the end */
                if (p->getch() != '"')
                {
                    info->lit.len = len;
                    break;
                }
            }
        }
    }
    else if (c == '{')
    {
        /* note the start of the literal section */
        info->lit.set(p);
        
        /* scan for the closing brace - only hex digits are allowed */
        for ( ; ; p->inc())
        {
            /* if at end of string, it's an error */
            if (!p->more())
                err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p->index());

            /* make sure it's a hex digit or close brace */
            if (p->getch() == '}')
            {
                /* it's the close brace - note it, skip it, and we're done */
                info->lit.len -= p->len;
                p->inc();
                break;
            }
            else if (!is_xdigit(p->getch()))
                err_throw_a(VMERR_PACK_PARSE, ERR_TYPE_INT, p->index());
        }
    }

    /* set the default modifiers */
    info->big_endian = group->big_endian;
    info->tilde = group->tilde;
    info->pct = group->pct;
    info->count = ITER_NONE;
    info->count_as_type = 0;
    info->count_in_bytes = FALSE;
    info->bang = info->qu = FALSE;
    info->null_term = FALSE;

    /* check for modifiers */
    parse_mods(p, info);
}

/*
 *   Parse group modifiers.  Call this at the open parenthesis of a group to
 *   find the matching close paren and parse the suffix qualifiers.  
 */
void CVmPack::parse_group_mods(const CVmPackPos *p, CVmPackType *gt)
{
    /* set up a copy of the pointer, so we don't alter the original */
    CVmPackPos p2(p);

    /* parse and skip the group and modifiers */
    skip_group_mods(&p2, gt);
}

/*
 *   Parse and skip a group and its modifiers.  Call this at the open paren
 *   of a group to find the matching close paren and parse past the suffix
 *   qualifiers.  This leaves 'p' positioned after the group's qualifiers.
 */
void CVmPack::skip_group_mods(CVmPackPos *p, CVmPackType *gt)
{
    /* note the close paren type we're looking for */
    wchar_t c = p->getch();
    wchar_t close_paren = (c == '(' ? ')' :
                           c == '[' ? ']' :
                           c == '{' ? '}' :
                           0);

    /* skip to the close paren */
    p->inc();
    skip_group(p, close_paren);

    /* skip the close paren */
    p->inc();

    /* parse the group modifiers */
    parse_mods(p, gt);
}

/*
 *   Skip to the close paren of a group.  Call this at the character after
 *   the open paren; we'll read ahead until we find the close paren, and
 *   return positioned at the paren. 
 */
void CVmPack::skip_group(CVmPackPos *p, wchar_t close_paren)
{
    /* scan ahead to the matching close paren */
    int depth = 1;
    for (p->inc() ; p->more() ; p->inc())
    {
        /* note any further nesting */
        wchar_t c = p->getch();
        switch (c)
        {
        case '(':
        case '[':
        case '{':
            depth++;
            break;

        case ')':
        case ']':
        case '}':
            depth--;
            break;
        }

        /* 
         *   if this is our close paren, and we're not in nested parens or
         *   other brackets, we're done 
         */
        if (c == close_paren && depth == 0)
            break;
    }

    /* make sure we found the matching close paren */
    if (!p->more())
        err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p->index());
}

/*
 *   Figure the '*' repeat count value for a given type code 
 */
static int star_to_count(wchar_t type_code)
{
    switch (type_code)
    {
    case 'x':
    case 'X':
    case '@':
        return 0;

    default:
        return ITER_STAR;
    }
}

/*
 *   Parse modifiers 
 */
void CVmPack::parse_mods(CVmPackPos *p, CVmPackType *t)
{
    /* keep going until we stop finding modifiers */
    while (p->more())
    {
        /* get the next character */
        wchar_t c = p->getch();

        /* check what we have */
        if (c == '<')
        {
            /* little-endian flag - note it and skip it */
            t->big_endian = FALSE;
            p->inc();
        }
        else if (c == '>')
        {
            /* big-endian flag - note it and skip it */
            t->big_endian = TRUE;
            p->inc();
        }
        else if (c == '0')
        {
            /* this can only be used with certain string types */
            if (strchr("auwhH", (char)t->type_code) == 0)
                err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p->index());

            /* null-termination */
            t->null_term = TRUE;
            p->inc();
        }
        else if (is_digit(c))
        {
            /* 
             *   if we already have a '*', the combination of a numeric count
             *   and a '*' makes it an up-to count 
             */
            if (t->count == ITER_STAR)
                t->up_to_count = TRUE;

            /* it's a digit, so this is a simple numeric repeat count */
            int count_idx = p->index();
            t->count = p->parse_int();
            t->count_as_type = 0;

            /* enforce a minimum value of 1 for most types, 0 for @ */
            int minval = (t->type_code == '@' ? 0 : 1);
            if (t->count < minval)
                err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, count_idx);
        }
        else if (c == ':')
        {
            /* repeat count as type - get and remember the type code */
            p->inc();
            c = p->nextch();

            /* validate the type code and get its size */
            switch (c)
            {
            case 'a':
            case 'A':
            case 'b':
            case 'u':
            case 'U':
            case 'h':
            case 'H':
            case 'k':
                t->count_as_type = c;
                t->count = 1;

            case 'w':
            case 'W':
                t->count_as_type = c;
                t->count = 2;
                break;
                
            case 'c':
            case 'C':
                t->count_as_type = c;
                t->count = 1;
                break;
                
            case 's':
            case 'S':
                t->count_as_type = c;
                t->count = 2;
                break;
                
            case 'l':
            case 'L':
            case 'f':
                t->count_as_type = c;
                t->count = 4;
                break;
                
            case 'q':
            case 'Q':
            case 'd':
                t->count_as_type = c;
                t->count = 8;
                break;

            default:
                /* invalid type */
                err_throw_a(VMERR_PACK_PARSE, 1, ERR_TYPE_INT, p->index());
            }
        }
        else if (c == '*')
        {
            /* 
             *   if we already have a numeric count, this makes it an up-to
             *   count 
             */
            if (t->count >= 0)
            {
                /* mark it as an up-to count, but leave the count intact */
                t->up_to_count = TRUE;
            }
            else
            {
                /* there's no count yet, so it's an indefinite counter type */
                t->count = star_to_count(t->type_code);
                t->count_as_type = 0;
            }

            /* skip the '*' */
            p->inc();
        }
        else if (c == '!')
        {
            /* 
             *   ! means "count in bytes" for string types wWhH; for other
             *   types just record the !
             */
            if (t->type_code != 0 && strchr("wWhH", (char)t->type_code) != 0)
                t->count_in_bytes = TRUE;
            else
                t->bang = TRUE;
            p->inc();
        }
        else if (c == '~')
        {
            /* note the ~ qualifier */
            t->tilde = TRUE;
            p->inc();
        }
        else if (c == '?')
        {
            /* note the ? qualifier */
            t->qu = TRUE;
            p->inc();
        }
        else if (c == '%')
        {
            /* note the % qualifier */
            t->pct = TRUE;
            p->inc();
        }
        else
        {
            /* it's not a modifier character, so we're done */
            break;
        }
    }
}
