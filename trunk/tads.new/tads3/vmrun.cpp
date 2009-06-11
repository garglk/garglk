#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMRUN.CPP,v 1.4 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmrun.cpp - VM Execution
Function
  
Notes
  
Modified
  11/12/98 MJRoberts  - Creation
*/

#include <stdio.h>

#include "t3std.h"
#include "os.h"
#include "vmrun.h"
#include "vmdbg.h"
#include "vmop.h"
#include "vmstack.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmobj.h"
#include "vmlst.h"
#include "vmstr.h"
#include "vmtobj.h"
#include "vmfunc.h"
#include "vmmeta.h"
#include "vmbif.h"
#include "vmpredef.h"
#include "vmfile.h"
#include "vmsave.h"
#include "vmprof.h"
#include "vmhash.h"


/* ------------------------------------------------------------------------ */
/*
 *   Initialize 
 */
CVmRun::CVmRun()
{
    init();
}

void CVmRun::init()
{
    /* start out with 'nil' in R0 */
    r0_.set_nil();

    /* there's no frame yet */
    frame_ptr_ = 0;

    /* there's no entry pointer yet */
    entry_ptr_ = 0;
    entry_ptr_native_ = 0;

    /* function header size is not yet known */
    funchdr_size_ = 0;

    /* we have no 'say' function yet */
    say_func_ = 0;

    /* no default 'say' method */
    say_method_ = VM_INVALID_PROP;

    /* no debugger halt requested yet */
    halt_vm_ = FALSE;

    /* we have no program counter yet */
    pc_ptr_ = 0;

    /*
     *   If we're including the profiler in the build, allocate and
     *   initialize its memory structures. 
     */
#ifdef VM_PROFILER

    /* 
     *   Allocate the profiler stack.  This stack will contain one record per
     *   activation frame in the regular VM stack.  
     */
    prof_stack_max_ = 250;
    (prof_stack_ = (vm_profiler_rec *)
                   t3malloc(prof_stack_max_ * sizeof(prof_stack_[0])));

    /* we don't have anything on the profiler stack yet */
    prof_stack_idx_ = 0;

    /* create the profiler master hash table */
    prof_master_table_ = new CVmHashTable(512, new CVmHashFuncCI(), TRUE);

    /* we're not running the profiler yet */
    profiling_ = FALSE;

#endif /* VM_PROFILER */
}

/* ------------------------------------------------------------------------ */
/*
 *   Terminate 
 */
CVmRun::~CVmRun()
{
    terminate();
}

void CVmRun::terminate()
{
    /*
     *   If we're including the profiler in the build, delete its memory
     *   structures.  
     */
#ifdef VM_PROFILER

    /* delete the profiler stack */
    t3free(prof_stack_);
    prof_stack_ = 0;

    /* delete the profiler master hash table */
    delete prof_master_table_;
    prof_master_table_ = 0;

#endif /* VM_PROFILER */
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the function header size 
 */
void CVmRun::set_funchdr_size(size_t siz)
{
    /* remember the new size */
    funchdr_size_ = siz;

    /* 
     *   ensure that the size is at least as large as our required
     *   function header block - if it's not, this version of the VM can't
     *   run this image file 
     */
    if (siz < VMFUNC_HDR_MIN_SIZE)
        err_throw(VMERR_IMAGE_INCOMPAT_HDR_FMT);
}

/* ------------------------------------------------------------------------ */
/*
 *   Add two values, leaving the result in *val1 
 */
void CVmRun::compute_sum(VMG_ vm_val_t *val1, vm_val_t *val2)
{    
    /* the meaning of "add" depends on the type of the first operand */
    switch(val1->typ)
    {
    case VM_SSTRING:
        /* 
         *   string constant - add the second value to the string, using
         *   the static string add method 
         */
        CVmObjString::add_to_str(vmg_ val1, VM_INVALID_OBJ,
                                 get_const_ptr(vmg_ val1->val.ofs), val2);
        break;

    case VM_LIST:
        /* 
         *   list constant - add the second value to the list, using the
         *   static list add method 
         */
        CVmObjList::add_to_list(vmg_ val1, VM_INVALID_OBJ,
                                get_const_ptr(vmg_ val1->val.ofs), val2);
        break;

    case VM_OBJ:
        /*
         *   object - add the second value to the object, using the
         *   object's virtual metaclass add method 
         */
        vm_objp(vmg_ val1->val.obj)->add_val(vmg_ val1, val1->val.obj, val2);
        break;

    case VM_INT:
        /* make sure the other value is a number as well */
        if (!val2->is_numeric())
            err_throw(VMERR_NUM_VAL_REQD);

        /* compute the sum */
        val1->val.intval += val2->num_to_int();
        break;

    default:
        /* other types cannot be added */
        err_throw(VMERR_BAD_TYPE_ADD);
        break;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Compute the difference of two values, leaving the result in *val1 
 */
void CVmRun::compute_diff(VMG_ vm_val_t *val1, vm_val_t *val2)
{
    /* the meaning of "subtract" depends on the type of the first operand */
    switch(val1->typ)
    {
    case VM_LIST:
        /* 
         *   list constant - remove the second value from the list, using
         *   the static list subtraction method 
         */
        CVmObjList::sub_from_list(vmg_ val1, val1,
                                  get_const_ptr(vmg_ val1->val.ofs), val2);
        break;

    case VM_OBJ:
        /* object - use the object's virtual subtraction method */
        vm_objp(vmg_ val1->val.obj)->sub_val(vmg_ val1, val1->val.obj, val2);
        break;

    case VM_INT:
        /* make sure the other value is a number as well */
        if (!val2->is_numeric())
            err_throw(VMERR_NUM_VAL_REQD);

        /* compute the difference */
        val1->val.intval -= val2->num_to_int();
        break;

    default:
        /* other types cannot be subtracted */
        err_throw(VMERR_BAD_TYPE_SUB);
    }

}

/* ------------------------------------------------------------------------ */
/*
 *   Compute the product val1 * val2, leaving the result in val1 
 */
void CVmRun::compute_product(VMG_ vm_val_t *val1, vm_val_t *val2)
{
    switch(val1->typ)
    {
    case VM_OBJ:
        /* use the object's virtual multiplication method */
        vm_objp(vmg_ val1->val.obj)->mul_val(vmg_ val1, val1->val.obj, val2);
        break;

    case VM_INT:
        /* make sure the other value is a number as well */
        if (!val2->is_numeric())
            err_throw(VMERR_NUM_VAL_REQD);

        /* compute the product */
        val1->val.intval *= val2->num_to_int();
        break;

    default:
        /* other types are invalid */
        err_throw(VMERR_BAD_TYPE_MUL);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute the quotient val1/val2, leaving the result in val1.  
 */
void CVmRun::compute_quotient(VMG_ vm_val_t *val1, vm_val_t *val2)
{
    switch(val1->typ)
    {
    case VM_OBJ:
        /* use the object's virtual division method */
        vm_objp(vmg_ val1->val.obj)->div_val(vmg_ val1, val1->val.obj, val2);
        break;

    case VM_INT:
        /* make sure the other value is a number as well */
        if (!val2->is_numeric())
            err_throw(VMERR_NUM_VAL_REQD);

        /* check for divide by zero */
        if (val2->num_to_int() == 0)
            err_throw(VMERR_DIVIDE_BY_ZERO);

        /* compute the product */
        val1->val.intval = os_divide_long(val1->val.intval,
                                          val2->num_to_int());
        break;

    default:
        /* other types are invalid */
        err_throw(VMERR_BAD_TYPE_DIV);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   XOR two values and push the result.  The values can be numeric or
 *   logical.  If either value is logical, the result will be logical;
 *   otherwise, the result will be a bitwise XOR of the integers.  
 */
void CVmRun::xor_and_push(VMG_ vm_val_t *val1, vm_val_t *val2)
{
    /* if either value is logical, compute the logical XOR */
    if (val1->is_logical() && val2->is_logical())
    {
        /* both values are logical - compute the logical XOR */
        val1->set_logical(val1->get_logical() ^ val2->get_logical());
    }
    else if (val1->is_logical() || val2->is_logical())
    {
        /* 
         *   one value is logical, but not both - convert the other value
         *   from a number to a logical and compute the result as a
         *   logical value 
         */
        if (!val1->is_logical())
            val1->num_to_logical();
        else if (!val2->is_logical())
            val2->num_to_logical();

        /* compute the logical xor */
        val1->set_logical(val1->get_logical() ^ val2->get_logical());
    }
    else if (val1->typ == VM_INT && val2->typ == VM_INT)
    {
        /* compute and store the bitwise XOR */
        val1->val.intval = val1->val.intval ^ val2->val.intval;
    }
    else
    {
        /* no logical conversion */
        err_throw(VMERR_NO_LOG_CONV);
    }

    /* push the result */
    pushval(vmg_ val1);
}


/* ------------------------------------------------------------------------ */
/*
 *   Index a value and push the result.
 */
void CVmRun::apply_index(VMG_ vm_val_t *result,
                         const vm_val_t *container_val,
                         const vm_val_t *index_val)
{
    /* check the type of the value we're indexing */
    switch(container_val->typ)
    {
    case VM_LIST:
        /* list constant - use the static list indexing method */
        CVmObjList::index_list(vmg_ result,
                               get_const_ptr(vmg_ container_val->val.ofs),
                               index_val);
        break;

    case VM_OBJ:
        /* object - use the object's virtual indexing method */
        vm_objp(vmg_ container_val->val.obj)
            ->index_val(vmg_ result, container_val->val.obj, index_val);
        break;

    default:
        /* other values cannot be indexed */
        err_throw(VMERR_CANNOT_INDEX_TYPE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Set an indexed value.  Updates *container_val with the modified
 *   container, if the operation requires this.  (For example, setting an
 *   indexed element of a list will create a new list, and return the new
 *   list in *container_val.  Setting an element of a vector simply modifies
 *   the vector in place, hence the container reference is unchanged.)  
 */
void CVmRun::set_index(VMG_ vm_val_t *container_val,
                       const vm_val_t *index_val,
                       const vm_val_t *new_val)
{
    switch(container_val->typ)
    {
    case VM_LIST:
        /* list constant - use the static list set-index method */
        CVmObjList::set_index_list(vmg_ container_val,
                                   get_const_ptr(vmg_ container_val->val.ofs),
                                   index_val, new_val);
        break;

    case VM_OBJ:
        /* object - use the object's virtual set-index method */
        vm_objp(vmg_ container_val->val.obj)
            ->set_index_val(vmg_ container_val,
                            container_val->val.obj, index_val, new_val);
        break;

    default:
        /* other values cannot be indexed */
        err_throw(VMERR_CANNOT_INDEX_TYPE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a new object and store it in R0
 */
const uchar *CVmRun::new_and_store_r0(VMG_ const uchar *pc,
                                      uint metaclass_idx, uint argc,
                                      int is_transient)
{
    vm_obj_id_t obj;
    
    /* create the object */
    obj = G_meta_table->create_from_stack(vmg_ &pc, metaclass_idx, argc);

    /* if we got a valid object, store a reference to it in R0 */
    if (obj != VM_INVALID_OBJ)
    {
        /* set the object return value */
        r0_.set_obj(obj);

        /* make the object transient if desired */
        if (is_transient)
            G_obj_table->set_obj_transient(obj);
    }
    else
    {
        /* failed - return nil */
        r0_.set_nil();
    }

    /* return the new instruction pointer */
    return pc;
}

/* ------------------------------------------------------------------------ */
/*
 *   Execute byte code 
 */
void CVmRun::run(VMG_ const uchar *start_pc)
{
    /* 
     *   If you're concerned about a compiler warning on the following
     *   'register' declaration, refer to the footnote at the bottom of this
     *   file (search for [REGISTER_P_FOOTNOTE]).  Executive summary: you can
     *   safely ignore the warning, and I'm keeping the code as it is.  
     */
    register const uchar *p = start_pc;
    const uchar *last_pc;
    const uchar **old_pc_ptr;
    vm_val_t *valp;
    vm_val_t *valp2;
    vm_val_t val;
    vm_val_t val2;
    vm_val_t val3;
    int done;
    vm_obj_id_t obj;
    vm_prop_id_t prop;
    uint argc;
    uint idx;
    uint set_idx;
    pool_ofs_t ofs;
    uint cnt;
    vm_obj_id_t unhandled_exc;
    int level;
    int trans;

    /* save the enclosing program counter pointer, and remember the new one */
    old_pc_ptr = pc_ptr_;
    pc_ptr_ = &last_pc;

    /* we're not done yet */
    done = FALSE;

    /* no unhandled exception yet */
    unhandled_exc = VM_INVALID_OBJ;

    /*
     *   Come back here whenever we catch a run-time exception and find a
     *   byte-code error handler to process it in the stack.  We'll
     *   re-enter our exception handler and resume byte-code execution at
     *   the handler.  
     */
resume_execution:

    /*
     *   Execute all code within an exception frame.  If any routine we
     *   call throws an exception, we'll catch the exception and process
     *   it as a run-time error.  
     */
    err_try
    {
        /* execute code until something makes us stop */
        for (;;)
        {
            VM_IF_DEBUGGER(static int brkchk = 0);

            /* 
             *   check for user-requested break, and step into the debugger
             *   if we find it 
             */
            VM_IF_DEBUGGER(
                /* check for break every so often */
                if (++brkchk > 10000)
                {
                    /* reset the break counter */
                    brkchk = 0;

                    /* check for break, and step into debugger if found */
                    if (os_break())
                        G_debugger->set_break_stop();
                }
            );

            /* if we're single-stepping, break into the debugger */
            VM_IF_DEBUGGER(if (G_debugger->is_single_step())
                G_debugger->step(vmg_ &p, entry_ptr_, FALSE, 0));

            /* check for a halt request from the debugger */
            VM_IF_DEBUGGER(if (halt_vm_) { done = TRUE; goto exit_loop; });

        exec_instruction:
            /* 
             *   Remember the location of this instruction in a non-register
             *   variable, in case there's an exception.  (We know that
             *   last_pc is guaranteed to be a non-register variable because
             *   we take its address and store it in our pc_ptr_ member.)
             *   
             *   We need to know the location of the last instruction when
             *   an exception occurs so that we can find the exception
             *   handler.  We want to encourage the compiler to enregister
             *   'p', since we access it so frequently in this routine; but
             *   if it's in a register, there's a risk we'd get the
             *   setjmp-time value in our exception handler.  To handle both
             *   needs, simply copy the value to our non-register variable
             *   last_pc; this will still let the vast majority of our
             *   access to 'p' use fast register operations if the compiler
             *   allows this, while ensuring we have a safe copy around in
             *   case of exceptions.  
             */
            last_pc = p;

            /* execute the current instruction */
            switch(*p++)
            {
            case OPC_PUSH_0:
                /* push the constant value 0 */
                push_int(vmg_ 0);
                break;

            case OPC_PUSH_1:
                /* push the constant value 1 */
                push_int(vmg_ 1);
                break;
                
            case OPC_PUSHINT8:
                /* push an SBYTE operand value */
                push_int(vmg_ get_op_int8(&p));
                break;

            case OPC_PUSHINT:
                /* push a UINT4 operand value */
                push_int(vmg_ get_op_int32(&p));
                break;

            case OPC_PUSHENUM:
                /* push a UINT4 operand value */
                push_enum(vmg_ get_op_uint32(&p));
                break;

            case OPC_PUSHSTR:
                /* push UINT4 offset operand as a string */
                G_stk->push()->set_sstring(get_op_uint32(&p));
                break;

            case OPC_PUSHSTRI:
                /* inline string - get the length prefix */
                cnt = get_op_uint16(&p);

                /* create the new string from the inline data */
                obj = CVmObjString::create(vmg_ FALSE, (const char *)p, cnt);

                /* skip past the string's bytes */
                p += cnt;

                /* push the new string */
                push_obj(vmg_ obj);
                break;

            case OPC_PUSHLST:
                /* push UINT4 offset operand as a list */
                G_stk->push()->set_list(get_op_uint32(&p));
                break;

            case OPC_PUSHOBJ:
                /* push UINT4 object ID operand */
                G_stk->push()->set_obj(get_op_uint32(&p));
                break;

            case OPC_PUSHNIL:
                /* push nil */
                push_nil(vmg0_);
                break;

            case OPC_PUSHTRUE:
                /* push true */
                G_stk->push()->set_true();
                break;

            case OPC_PUSHPROPID:
                /* push UINT2 property ID operand */
                G_stk->push()->set_propid(get_op_uint16(&p));
                break;

            case OPC_PUSHFNPTR:
                /* push a function pointer operand */
                G_stk->push()->set_fnptr(get_op_uint32(&p));
                break;

            case OPC_PUSHPARLST:
                /* get the number of fixed parameters */
                cnt = *p++;

                /* allocate the list from the parameters */
                obj = CVmObjList::create_from_params(
                    vmg_ cnt, get_cur_argc(vmg0_) - cnt);

                /* push the new list */
                push_obj(vmg_ obj);
                break;

            case OPC_MAKELSTPAR:
                {
                    const char *lstp;
                    uint i;
                    uint hdr_depth;
                    CVmFuncPtr hdr_ptr;

                    /* pop the value */
                    popval(vmg_ &val);

                    /* pop the argument counter so far */
                    pop_int(vmg_ &val2);

                    /* if it's not a list, just push it again unchanged */
                    if ((lstp = val.get_as_list(vmg0_)) == 0)
                    {
                        /* put it back on the stack */
                        pushval(vmg_ &val);

                        /* increment the argument count and push it */
                        ++val2.val.intval;
                        pushval(vmg_ &val2);

                        /* our work here is done */
                        break;
                    }

                    /* set up a pointer to the current function header */
                    hdr_ptr.set(entry_ptr_native_);

                    /* get the depth required for the header */
                    hdr_depth = hdr_ptr.get_stack_depth();

                    /* 
                     *   deduct the amount stack space we've already used
                     *   from the amount noted in the header, because
                     *   that's the amount more that we could need for the
                     *   fixed stuff
                     */
                    hdr_depth -= (G_stk->get_depth_rel(frame_ptr_) - 1);

                    /* get the number of elements in the list */
                    cnt = vmb_get_len(lstp);
                      
                    /* make sure we have enough stack space available */
                    if (!G_stk->check_space(cnt + hdr_depth))
                        err_throw(VMERR_STACK_OVERFLOW);

                    /* push the elements of the list from last to first */
                    for (i = cnt ; i != 0 ; --i)
                    {
                        /* get this element's value */
                        CVmObjList::index_and_push(vmg_ lstp, i);
                    }

                    /* increment and push the argument count */
                    val2.val.intval += cnt;
                    pushval(vmg_ &val2);
                }
                break;

            case OPC_NEG:
                /* check for an object */
                if ((valp = G_stk->get(0))->typ == VM_OBJ)
                {
                    /* call the object's negate method */
                    vm_objp(vmg_ valp->val.obj)
                        ->neg_val(vmg_ &val2, valp->val.obj);

                    /* replace TOS with the result */
                    *valp = val2;
                }
                else
                {
                    /* make sure it's a number */
                    if (!valp->is_numeric())
                        err_throw(VMERR_NUM_VAL_REQD);

                    /* negate number in place */
                    valp->val.intval = -valp->val.intval;
                }
                break;

            case OPC_BNOT:
                /* ensure we have an integer */
                if ((valp = G_stk->get(0))->typ != VM_INT)
                    err_throw(VMERR_INT_VAL_REQD);

                /* bitwise NOT the integer on top of stack */
                valp->val.intval = ~valp->val.intval;
                break;

            case OPC_ADD:
                /* if they're both integers, add them the quick way */
                valp = G_stk->get(0);
                valp2 = G_stk->get(1);
                if (valp->typ == VM_INT && valp2->typ == VM_INT)
                {
                    /* add the two values */
                    valp2->val.intval += valp->val.intval;

                    /* discard the second value */
                    G_stk->discard();
                }
                else
                {
                    /* 
                     *   compute the sum of (TOS-1) + (TOS), leaving the
                     *   result in (TOS-1) 
                     */
                    compute_sum(vmg_ valp2, valp);

                    /* discard TOS */
                    G_stk->discard();
                }
                break;

            case OPC_INC:
                /* 
                 *   Increment the value at top of stack.  We must perform
                 *   the same type conversions as the ADD instruction
                 *   does.  As an optimization, check to see if we have an
                 *   integer on top of the stack, and if so simply
                 *   increment its value without popping and repushing.  
                 */
                if ((valp = G_stk->get(0))->typ == VM_INT)
                {
                    /* it's an integer - increment it, and we're done */
                    ++(valp->val.intval);
                }
                else
                {
                    /* add 1 to the value at TOS, leaving it on the stack */
                    val2.set_int(1);
                    compute_sum(vmg_ valp, &val2);
                }
                break;

            case OPC_DEC:
                /* 
                 *   Decrement the value at top of stack.  We must perform
                 *   the same type conversions as the SUB instruction
                 *   does.  As an optimization, check to see if we have an
                 *   integer on top of the stack, and if so simply
                 *   decrement its value without popping and repushing.  
                 */
                if ((valp = G_stk->get(0))->typ == VM_INT)
                {
                    /* it's an integer - decrement it, and we're done */
                    --(valp->val.intval);
                }
                else
                {
                    /* compute TOS - 1, leaving the result in TOS */
                    val2.set_int(1);
                    compute_diff(vmg_ valp, &val2);
                }
                break;

            case OPC_SUB:
                /* if they're both integers, subtract them the quick way */
                valp = G_stk->get(0);
                valp2 = G_stk->get(1);
                if (valp->typ == VM_INT && valp2->typ == VM_INT)
                {
                    /* compute the difference */
                    valp2->val.intval -= valp->val.intval;

                    /* discard the second value */
                    G_stk->discard();
                }
                else
                {
                    /* 
                     *   compute the difference (TOS-1) - (TOS), leaving the
                     *   result in (TOS-1) 
                     */
                    compute_diff(vmg_ valp2, valp);

                    /* discard TOS */
                    G_stk->discard();
                }
                break;

            case OPC_MUL:
                /* if they're both integers, this is easy */
                valp = G_stk->get(0);
                valp2 = G_stk->get(1);
                if (valp->typ == VM_INT && valp2->typ == VM_INT)
                {
                    /* compute the difference */
                    valp2->val.intval *= valp->val.intval;

                    /* discard the second value */
                    G_stk->discard();
                }
                else
                {
                    /* 
                     *   compute the product (TOS-1) * (TOS), leaving the
                     *   result in (TOS-1) 
                     */
                    compute_product(vmg_ valp2, valp);

                    /* discard TOS */
                    G_stk->discard();
                }
                break;

            case OPC_DIV:
                /* if they're both integers, divide them the quick way */
                valp = G_stk->get(0);
                valp2 = G_stk->get(1);
                if (valp->typ == VM_INT && valp2->typ == VM_INT)
                {
                    /* check for division by zero */
                    if (valp->val.intval == 0)
                        err_throw(VMERR_DIVIDE_BY_ZERO);

                    /* compute the result of the division */
                    valp2->val.intval = os_divide_long(
                        valp2->val.intval, valp->val.intval);

                    /* discard the second value */
                    G_stk->discard();
                }
                else
                {
                    /* 
                     *   compute (TOS-1) / (TOS), leaving the result in
                     *   (TOS-1) 
                     */
                    compute_quotient(vmg_ valp2, valp);

                    /* discard TOS */
                    G_stk->discard();
                }
                break;

            case OPC_MOD:
                /* remainder number at (TOS-1) by number at top of stack */
                valp = G_stk->get(0);
                valp2 = G_stk->get(1);

                /* make sure the values are integers */
                if (valp->typ != VM_INT || valp2->typ != VM_INT)
                    err_throw(VMERR_INT_VAL_REQD);

                /* 
                 *   compute the remainger (TOS-1) % (TOS), leaving the
                 *   result at (TOS-1), and discard the second operand 
                 */
                valp2->val.intval = os_remainder_long(
                    valp2->val.intval, valp->val.intval);
                G_stk->discard();
                break;

            case OPC_BAND:
                /* bitwise AND two integers on top of stack */
                valp = G_stk->get(0);
                valp2 = G_stk->get(1);

                /* ensure we have two integers */
                if (valp->typ != VM_INT || valp2->typ != VM_INT)
                    err_throw(VMERR_INT_VAL_REQD);

                /* compute the result and discard the second operand */
                valp2->val.intval &= valp->val.intval;
                G_stk->discard();
                break;

            case OPC_BOR:
                /* bitwise OR two integers on top of stack */
                valp = G_stk->get(0);
                valp2 = G_stk->get(1);

                /* ensure we have two integers */
                if (valp->typ != VM_INT || valp2->typ != VM_INT)
                    err_throw(VMERR_INT_VAL_REQD);

                /* compute the result and discard the second operand */
                valp2->val.intval |= valp->val.intval;
                G_stk->discard();
                break;

            case OPC_SHL:
                /* 
                 *   bit-shift left integer at (TOS-1) by integer at top
                 *   of stack 
                 */
                valp = G_stk->get(0);
                valp2 = G_stk->get(1);

                /* ensure we have two integers */
                if (valp->typ != VM_INT || valp2->typ != VM_INT)
                    err_throw(VMERR_INT_VAL_REQD);

                /* compute the result and discard the second operand */
                valp2->val.intval <<= valp->val.intval;
                G_stk->discard();
                break;

            case OPC_SHR:
                /* 
                 *   bit-shift right integer at (TOS-1) by integer at top
                 *   of stack 
                 */
                valp = G_stk->get(0);
                valp2 = G_stk->get(1);

                /* ensure we have two integers */
                if (valp->typ != VM_INT || valp2->typ != VM_INT)
                    err_throw(VMERR_INT_VAL_REQD);

                /* compute the result and discard the second operand */
                valp2->val.intval >>= valp->val.intval;
                G_stk->discard();
                break;

            case OPC_XOR:
                /* XOR two values at top of stack */
                popval_2(vmg_ &val, &val2);
                xor_and_push(vmg_ &val, &val2);
                break;

            case OPC_NOT:
                /* 
                 *   invert the logic value; if the value is a number,
                 *   treat 0 as nil and non-zero as true 
                 */
                valp = G_stk->get(0);
                switch(valp->typ)
                {
                case VM_NIL:
                    /* !nil -> true */
                    valp->set_true();
                    break;

                case VM_OBJ:
                    /* !obj -> true if obj is nil, nil otherwise */
                    valp->set_logical(valp->val.obj == VM_INVALID_OBJ);
                    break;

                case VM_TRUE:
                case VM_PROP:
                case VM_SSTRING:
                case VM_LIST:
                case VM_CODEOFS:
                case VM_FUNCPTR:
                case VM_ENUM:
                    /* these are all considered true, so !them -> nil */
                    valp->set_nil();
                    break;

                case VM_INT:
                    /* !int -> true if int is 0, nil otherwise */
                    valp->set_logical(valp->val.intval == 0);
                    break;

                default:
                    err_throw(VMERR_NO_LOG_CONV);
                }
                break;

            case OPC_BOOLIZE:
                /* set to a boolean value */
                valp = G_stk->get(0);
                switch(valp->typ)
                {
                case VM_NIL:
                case VM_TRUE:
                    /* it's already a logical value - leave it alone */
                    break;

                case VM_INT:
                    /* integer: 0 -> nil, non-zero -> true */
                    valp->set_logical(valp->val.intval);
                    break;

                case VM_ENUM:
                    /* an enum is always non-nil */
                    valp->set_true();
                    break;

                default:
                    err_throw(VMERR_NO_LOG_CONV);
                }
                break;

            case OPC_EQ:
                /* compare two values at top of stack for equality */
                push_bool(vmg_ pop2_equal(vmg0_));
                break;

            case OPC_NE:
                /* compare two values at top of stack for inequality */
                push_bool(vmg_ !pop2_equal(vmg0_));
                break;

            case OPC_LT:
                /* compare values at top of stack - true if (TOS-1) < TOS */
                push_bool(vmg_ pop2_compare_lt(vmg0_));
                break;

            case OPC_LE:
                /* compare values at top of stack - true if (TOS-1) <= TOS */
                push_bool(vmg_ pop2_compare_le(vmg0_));
                break;

            case OPC_GT:
                /* compare values at top of stack - true if (TOS-1) > TOS */
                push_bool(vmg_ pop2_compare_gt(vmg0_));
                break;

            case OPC_GE:
                /* compare values at top of stack - true if (TOS-1) >= TOS */
                push_bool(vmg_ pop2_compare_ge(vmg0_));
                break;

            case OPC_VARARGC:
                {
                    uchar opc;

                    /* get the modified opcode */
                    opc = *p++;

                    /* 
                     *   skip the immediate data argument count - this is
                     *   superseded by our dynamic argument counter 
                     */
                    ++p;
                    
                    /* pop the argument counter */
                    pop_int(vmg_ &val);
                    argc = val.val.intval;

                    /* execute the appropriate next opcode */
                    switch(opc)
                    {
                    case OPC_CALL:
                        goto do_opc_call;

                    case OPC_PTRCALL:
                        goto do_opc_ptrcall;

                    case OPC_CALLPROP:
                        goto do_opc_callprop;

                    case OPC_PTRCALLPROP:
                        goto do_opc_ptrcallprop;

                    case OPC_CALLPROPSELF:
                        goto do_opc_callpropself;

                    case OPC_PTRCALLPROPSELF:
                        goto do_opc_ptrcallpropself;

                    case OPC_OBJCALLPROP:
                        goto do_opc_objcallprop;

                    case OPC_CALLPROPLCL1:
                        goto do_opc_callproplcl1;

                    case OPC_CALLPROPR0:
                        goto do_opc_callpropr0;

                    case OPC_INHERIT:
                        goto do_opc_inherit;

                    case OPC_PTRINHERIT:
                        goto do_opc_ptrinherit;

                    case OPC_EXPINHERIT:
                        goto do_opc_expinherit;

                    case OPC_PTREXPINHERIT:
                        goto do_opc_ptrexpinherit;

                    case OPC_DELEGATE:
                        goto do_opc_delegate;

                    case OPC_PTRDELEGATE:
                        goto do_opc_ptrdelegate;

                    case OPC_BUILTIN_A:
                        goto do_opc_builtin_a;

                    case OPC_BUILTIN_B:
                        goto do_opc_builtin_b;

                    case OPC_BUILTIN_C:
                        goto do_opc_builtin_c;

                    case OPC_BUILTIN_D:
                        goto do_opc_builtin_d;

                    case OPC_BUILTIN1:
                        goto do_opc_builtin1;

                    case OPC_BUILTIN2:
                        goto do_opc_builtin2;

                    case OPC_NEW1:
                        trans = FALSE;
                        goto do_opc_new1_argc;

                    case OPC_TRNEW1:
                        trans = TRUE;
                        goto do_opc_new1_argc;

                    case OPC_NEW2:
                        trans = FALSE;
                        goto do_opc_new2_argc;

                    case OPC_TRNEW2:
                        trans = TRUE;
                        goto do_opc_new2_argc;

                    default:
                        err_throw(VMERR_INVALID_OPCODE_MOD);
                        break;
                    }
                }
                break;

            case OPC_CALL:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_call:
                /* get the code offset to invoke */
                ofs = get_op_int32(&p);

                /* call it */
                p = do_call_func_nr(vmg_ p - entry_ptr_native_, ofs, argc);
                break;

            case OPC_PTRCALL:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_ptrcall:
                /* retrieve the target of the call */
                popval(vmg_ &val);

                /* 
                 *   if it's a prop ID, and there's a valid "self" object,
                 *   treat it as a PTRCALLPROPSELF 
                 */
                if (val.typ == VM_PROP && get_self(vmg0_) != VM_INVALID_OBJ)
                    goto do_opc_ptrcallpropself_val;
                
                /* call the function */
                p = call_func_ptr(vmg_ &val, argc, 0, p - entry_ptr_native_);
                break;

            case OPC_RETVAL:
                /* pop the return value into R0 */
                popval(vmg_ &r0_);

                /* return */
                if ((p = do_return(vmg0_)) == 0)
                    goto exit_loop;
                break;

            case OPC_RET:
                /* return, leaving R0 unchanged */
                if ((p = do_return(vmg0_)) == 0)
                    goto exit_loop;
                break;

            case OPC_RETNIL:
                /* store nil in R0 */
                r0_.set_nil();

                /* return */
                if ((p = do_return(vmg0_)) == 0)
                    goto exit_loop;
                break;

            case OPC_RETTRUE:
                /* store true in R0 */
                r0_.set_true();

                /* return */
                if ((p = do_return(vmg0_)) == 0)
                    goto exit_loop;
                break;

            case OPC_GETPROP:
                /* get the object whose property we're fetching */
                G_stk->pop(&val);

                /* evaluate the property given by the immediate data */
                prop = get_op_uint16(&p);
                p = get_prop(vmg_ p - entry_ptr_native_, &val, prop, &val, 0);
                break;

            case OPC_GETPROPLCL1:
                /* get the local whose property we're evaluating */
                valp = get_local(vmg_ get_op_uint8(&p));

                /* evaluate the property of the local variable */
                prop = get_op_uint16(&p);
                p = get_prop(vmg_ p - entry_ptr_native_, valp, prop, valp, 0);
                break;

            case OPC_GETPROPR0:
                /* evaluate the property of R0 */
                val = r0_;
                valp = &val;
                prop = get_op_uint16(&p);
                p = get_prop(vmg_ p - entry_ptr_native_, valp, prop, valp, 0);
                break;

            case OPC_CALLPROP:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_callprop:
                /* pop the object whose property we're fetching */
                G_stk->pop(&val);

                /* evaluate the property given by the immediate data */
                prop = get_op_uint16(&p);
                p = get_prop(vmg_ p - entry_ptr_native_, &val,
                             prop, &val, argc);
                break;

            case OPC_CALLPROPLCL1:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_callproplcl1:
                /* get the local whose property we're calling */
                valp = get_local(vmg_ get_op_uint8(&p));

                /* call the property of the local */
                prop = get_op_uint16(&p);
                p = get_prop(vmg_ p - entry_ptr_native_, valp,
                             prop, valp, argc);
                break;

            case OPC_CALLPROPR0:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_callpropr0:
                /* call the property of R0 */
                val = r0_;
                prop = get_op_uint16(&p);
                p = get_prop(vmg_ p - entry_ptr_native_, &val,
                             prop, &val, argc);
                break;

            case OPC_PTRCALLPROP:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_ptrcallprop:
                /* 
                 *   pop the property to be evaluated, and the object
                 *   whose property we're evaluating 
                 */
                pop_prop(vmg_ &val);
                G_stk->pop(&val2);
                
                /* evaluate the property */
                p = get_prop(vmg_ p - entry_ptr_native_, &val2,
                             val.val.prop, &val2, argc);
                break;

            case OPC_GETPROPSELF:
                /* evaluate the property of 'self' */
                val.set_obj(get_self(vmg0_));
                prop = get_op_uint16(&p);
                p = get_prop(vmg_ p - entry_ptr_native_, &val, prop, &val, 0);
                break;

            case OPC_CALLPROPSELF:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_callpropself:
                /* evaluate the property of 'self' */
                val.set_obj(get_self(vmg0_));
                prop = get_op_uint16(&p);
                p = get_prop(vmg_ p - entry_ptr_native_, &val,
                             prop, &val, argc);
                break;

            case OPC_PTRCALLPROPSELF:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_ptrcallpropself:
                /* get the property to be evaluated */
                pop_prop(vmg_ &val);

            do_opc_ptrcallpropself_val:
                /* evaluate the property of 'self' */
                val2.set_obj(get_self(vmg0_));
                p = get_prop(vmg_ p - entry_ptr_native_,
                             &val2, val.val.prop, &val2, argc);
                break;

            case OPC_OBJGETPROP:
                /* get the object */
                val.set_obj((vm_obj_id_t)get_op_uint32(&p));
                
                /* evaluate the property */
                prop = get_op_uint16(&p);
                p = get_prop(vmg_ p - entry_ptr_native_, &val, prop, &val, 0);
                break;

            case OPC_OBJCALLPROP:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_objcallprop:
                /* get the object */
                val.set_obj((vm_obj_id_t)get_op_uint32(&p));
                
                /* evaluate the property */
                prop = get_op_uint16(&p);
                p = get_prop(vmg_ p - entry_ptr_native_,
                             &val, prop, &val, argc);
                break;

            case OPC_GETPROPDATA:
                /* get the object whose property we're fetching */
                G_stk->pop(&val);

                /* 
                 *   if the object is not an object, it's one of the
                 *   native types, in which case we'll definitely run
                 *   native code to evaluate the property, in which case
                 *   it's not valid for speculative evaluation 
                 */
                if (val.typ != VM_OBJ)
                    err_throw(VMERR_BAD_SPEC_EVAL);

                /* get the property */
                prop = (vm_prop_id_t)get_op_uint16(&p);

                /* check validity for speculative evaluation */
                check_prop_spec_eval(vmg_ val.val.obj, prop);

                /* evaluate the property given by the immediate data */
                p = get_prop(vmg_ p - entry_ptr_native_, &val, prop, &val, 0);
                break;

            case OPC_PTRGETPROPDATA:
                /* get the property and object to evaluate */
                pop_prop(vmg_ &val);
                G_stk->pop(&val2);

                /* 
                 *   if the object is not an object, it's one of the
                 *   native types, in which case we'll definitely run
                 *   native code to evaluate the property, in which case
                 *   it's not valid for speculative evaluation 
                 */
                if (val2.typ != VM_OBJ)
                    err_throw(VMERR_BAD_SPEC_EVAL);

                /* check validity for speculative evaluation */
                check_prop_spec_eval(vmg_ val2.val.obj, val.val.prop);

                /* evaluate it */
                p = get_prop(vmg_ p - entry_ptr_native_,
                             &val2, val.val.prop, &val2, 0);
                break;

            case OPC_GETLCL1:
                /* push the local */
                pushval(vmg_ get_local(vmg_ get_op_uint8(&p)));
                break;

            case OPC_GETLCL2:
                /* push the local */
                pushval(vmg_ get_local(vmg_ get_op_uint16(&p)));
                break;

            case OPC_GETARG1:
                /* push the argument */
                pushval(vmg_ get_param(vmg_ get_op_uint8(&p)));
                break;

            case OPC_GETARG2:
                /* push the argument */
                pushval(vmg_ get_param(vmg_ get_op_uint16(&p)));
                break;

            case OPC_PUSHSELF:
                /* push 'self' */
                pushval(vmg_ get_self_val(vmg0_));
                break;

            case OPC_SETSELF:
                /* retrieve the 'self' object */
                G_stk->pop(&val);

                /* set 'self' */
                set_self(vmg_ &val);
                break;

            case OPC_STORECTX:
                {
                    char buf[VMB_LEN + 4*VMB_DATAHOLDER];

                    /* our list has four elements */
                    vmb_put_len(buf, 4);

                    /* 
                     *   put the list elements: 'self', targetprop, original
                     *   target object, and defining object 
                     */
                    vmb_put_dh_obj(buf + VMB_LEN, get_self(vmg0_));
                    vmb_put_dh_prop(buf + VMB_LEN + VMB_DATAHOLDER,
                                    get_target_prop(vmg0_));
                    vmb_put_dh_obj(buf + VMB_LEN + 2*VMB_DATAHOLDER,
                                   get_orig_target_obj(vmg0_));
                    vmb_put_dh_obj(buf + VMB_LEN + 3*VMB_DATAHOLDER,
                                   get_defining_obj(vmg0_));

                    /* push a new list copied from our prepared buffer */
                    push_obj(vmg_ CVmObjList::create(vmg_ FALSE, buf));
                }
                break;

            case OPC_LOADCTX:
                {
                    const char *lstp;

                    /* 
                     *   convert the context object (at top of stack) to a
                     *   list pointer 
                     */
                    lstp = G_stk->get(0)->get_as_list(vmg0_);

                    /* throw an error if it's not what we're expecting */
                    if (lstp == 0 || vmb_get_len(lstp) < 4)
                        err_throw(VMERR_LIST_VAL_REQD);

                    /* retrieve and store the context elements */
                    set_method_ctx(
                        vmg_ vmb_get_dh_obj(lstp + VMB_LEN),
                        vmb_get_dh_prop(lstp + VMB_LEN
                                        + VMB_DATAHOLDER),
                        vmb_get_dh_obj(lstp + VMB_LEN
                                       + 2*VMB_DATAHOLDER),
                        vmb_get_dh_obj(lstp + VMB_LEN
                                       + 3*VMB_DATAHOLDER));

                    /* discard the context object at top of stack */
                    G_stk->discard();
                }
                break;

            case OPC_PUSHCTXELE:
                /* check our context element type */
                switch(*p++)
                {
                case PUSHCTXELE_TARGPROP:
                    /* push the target property ID */
                    push_prop(vmg_ get_target_prop(vmg0_));
                    break;

                case PUSHCTXELE_TARGOBJ:
                    /* push the original target object ID */
                    push_obj(vmg_ get_orig_target_obj(vmg0_));
                    break;

                case PUSHCTXELE_DEFOBJ:
                    /* push the defining object */
                    push_obj(vmg_ get_defining_obj(vmg0_));
                    break;

                default:
                    /* the opcode is not valid in this VM version */
                    err_throw(VMERR_INVALID_OPCODE);
                }
                break;

            case OPC_GETARGC:
                /* push the argument counter */
                push_int(vmg_ get_cur_argc(vmg0_));
                break;

            case OPC_DUP:
                /* re-push the item at top of stack */
                pushval(vmg_ G_stk->get(0));
                break;

            case OPC_SWAP:
                /* swap the top two elements on the stack */
                valp = G_stk->get(0);
                valp2 = G_stk->get(1);

                /* make a working copy of TOS */
                val = *valp;

                /* copy TOS-1 over TOS */
                *valp = *valp2;

                /* copy the working copy of TOS over TOS-1 */
                *valp2 = val;
                break;

            case OPC_DISC:
                /* discard the item at the top of the stack */
                G_stk->discard();
                break;

            case OPC_DISC1:
                /* discard n items */
                G_stk->discard(get_op_uint8(&p));
                break;

            case OPC_GETR0:
                /* push the contents of R0 */
                pushval(vmg_ &r0_);
                break;

            case OPC_GETDBARGC:
                /* push the argument count from the selected frame */
                push_int(vmg_ get_argc_at_level(vmg_ get_op_uint16(&p) + 1));
                break;

            case OPC_GETDBLCL:
                /* get the local variable number and stack level */
                idx = get_op_uint16(&p);
                level = get_op_uint16(&p);

                /* push the value */
                pushval(vmg_ get_local_at_level(vmg_ idx, level + 1));
                break;
                
            case OPC_GETDBARG:
                /* get the parameter variable number and stack level */
                idx = get_op_uint16(&p);
                level = get_op_uint16(&p);

                /* push the value */
                pushval(vmg_ get_param_at_level(vmg_ idx, level + 1));
                break;

            case OPC_SETDBLCL:
                /* get the local variable number and stack level */
                idx = get_op_uint16(&p);
                level = get_op_uint16(&p);

                /* get the local pointer */
                valp = get_local_at_level(vmg_ idx, level + 1);

                /* pop the value into the local */
                popval(vmg_ valp);
                break;

            case OPC_SETDBARG:
                /* get the parameter variable number and stack level */
                idx = get_op_uint16(&p);
                level = get_op_uint16(&p);

                /* get the parameter pointer */
                valp = get_param_at_level(vmg_ idx, level + 1);

                /* pop the value into the local */
                popval(vmg_ valp);
                break;

            case OPC_SWITCH:
                /* get the control value */
                valp = G_stk->get(0);

                /* get the case count */
                cnt = get_op_uint16(&p);

                /* iterate through the case table */
                for ( ; cnt != 0 ; p += 7, --cnt)
                {
                    /* get this value */
                    vmb_get_dh((const char *)p, &val2);

                    /* check if the values match */
                    if (valp->equals(vmg_ &val2))
                    {
                        /* it matches - jump to this offset */
                        p += VMB_DATAHOLDER;
                        p += osrp2s(p);

                        /* no need to look any further */
                        break;
                    }
                }

                /* discard the control value */
                G_stk->discard();

                /* if we didn't find it, jump to the default case */
                if (cnt == 0)
                    p += osrp2s(p);
                break;

            case OPC_JMP:
                /* unconditionally jump to the given offset */
                p += osrp2s(p);
                break;

            case OPC_JT:
                /* get the value */
                valp = G_stk->get(0);

                /* 
                 *   if it's true, or a non-zero numeric value, or any
                 *   non-numeric and non-boolean value, jump 
                 */
                if (valp->typ == VM_NIL
                    || (valp->typ == VM_INT && valp->val.intval == 0))
                {
                    /* it's zero or nil - do not jump */
                    p += 2;
                }
                else
                {
                    /* it's non-zero and non-nil - jump */
                    p += osrp2s(p);
                }

                /* discard the value */
                G_stk->discard();
                break;

            case OPC_JR0T:
                /* 
                 *   if R0 is true, or it's a non-zero numeric value, or any
                 *   non-numeric and non-boolean value, jump 
                 */
                if (r0_.typ == VM_NIL
                    || (r0_.typ == VM_INT && r0_.val.intval == 0))
                {
                    /* it's zero or nil - do not jump */
                    p += 2;
                }
                else
                {
                    /* it's non-zero and non-nil - jump */
                    p += osrp2s(p);
                }
                break;

            case OPC_JF:
                /* get the value */
                valp = G_stk->get(0);

                /* 
                 *   if it's true, or a non-zero numeric value, or any
                 *   non-numeric and non-boolean value, do not jump;
                 *   otherwise, jump 
                 */
                if (valp->typ == VM_NIL
                    || (valp->typ == VM_INT && valp->val.intval == 0))
                {
                    /* it's zero or nil - jump */
                    p += osrp2s(p);
                }
                else
                {
                    /* it's non-zero and non-nil - do not jump */
                    p += 2;
                }

                /* discard the value */
                G_stk->discard();
                break;

            case OPC_JR0F:
                /* 
                 *   if R0 is true, or it's a non-zero numeric value, or any
                 *   non-numeric and non-boolean value, stay put; otherwise,
                 *   jump 
                 */
                if (r0_.typ == VM_NIL
                    || (r0_.typ == VM_INT && r0_.val.intval == 0))
                {
                    /* it's zero or nil - jump */
                    p += osrp2s(p);
                }
                else
                {
                    /* it's non-zero and non-nil - do not jump */
                    p += 2;
                }
                break;

            case OPC_JE:
                /* jump if the two values at top of stack are equal */
                p += (pop2_equal(vmg0_) ? osrp2s(p) : 2);
                break;

            case OPC_JNE:
                /* jump if the two values at top of stack are not equal */
                p += (!pop2_equal(vmg0_) ? osrp2s(p) : 2);
                break;

            case OPC_JGT:
                /* jump if greater */
                p += (pop2_compare_gt(vmg0_) ? osrp2s(p) : 2);
                break;

            case OPC_JGE:
                /* jump if greater or equal */
                p += (pop2_compare_ge(vmg0_) ? osrp2s(p) : 2);
                break;

            case OPC_JLT:
                /* jump if less */
                p += (pop2_compare_lt(vmg0_) ? osrp2s(p) : 2);
                break;

            case OPC_JLE:
                /* jump if less or equal */
                p += (pop2_compare_le(vmg0_) ? osrp2s(p) : 2);
                break;

            case OPC_JST:
                /* get (do not remove) the element at top of stack */
                valp = G_stk->get(0);

                /* 
                 *   if it's true or a non-zero number, jump, saving the
                 *   value; otherwise, require that it be a logical value,
                 *   pop it, and proceed 
                 */
                if (valp->typ == VM_TRUE
                    || valp->typ == VM_ENUM
                    || valp->typ == VM_INT && !valp->num_is_zero())
                {
                    /* it's true - save it and jump */
                    p += osrp2s(p);
                }
                else
                {
                    /* 
                     *   it's not true - discard the value, but require
                     *   that it be a valid logical value 
                     */
                    if (valp->typ != VM_NIL && valp->typ != VM_INT)
                        err_throw(VMERR_LOG_VAL_REQD);
                    G_stk->discard();

                    /* skip to the next instruction */
                    p += 2;
                }
                break;

            case OPC_JSF:
                /* get (do not remove) the element at top of stack */
                valp = G_stk->get(0);

                /* 
                 *   if it's nil or zero, jump, saving the value;
                 *   otherwise, discard the value and proceed 
                 */
                if (valp->typ == VM_NIL
                    || valp->typ == VM_INT && valp->num_is_zero())
                {
                    /* it's nil or zero - save it and jump */
                    p += osrp2s(p);
                }
                else
                {
                    /* it's something non-false - discard it */
                    G_stk->discard();

                    /* skip to the next instruction */
                    p += 2;
                }
                break;

            case OPC_LJSR:
                /* 
                 *   compute and push the offset of the next instruction
                 *   (at +2 because of the branch offset operand) from our
                 *   method header - this will be the return address,
                 *   which in this offset format will survive any code
                 *   swapping that might occur in subsequent execution 
                 */
                push_int(vmg_ pc_to_method_ofs(p + 2));

                /* jump to the target address */
                p += osrp2s(p);
                break;

            case OPC_LRET:
                /* get the indicated local variable */
                valp = get_local(vmg_ get_op_uint16(&p));
                
                /* the value must be an integer */
                if (valp->typ != VM_INT)
                    err_throw(VMERR_INT_VAL_REQD);
                
                /* 
                 *   jump to the code address obtained from adding the
                 *   integer value in the given local variable to the
                 *   current method header pointer 
                 */
                p = entry_ptr_native_ + valp->val.intval;
                break;

            case OPC_JNIL:
                /* jump if top of stack is nil */
                valp = G_stk->get(0);
                p += (valp->typ == VM_NIL ? osrp2s(p) : 2);

                /* discard the top value, regardless of what happened */
                G_stk->discard();
                break;

            case OPC_JNOTNIL:
                /* jump if top of stack is not nil */
                valp = G_stk->get(0);
                p += (valp->typ != VM_NIL ? osrp2s(p) : 2);

                /* discard the top value, regardless of what happened */
                G_stk->discard();
                break;

            case OPC_SAY:
                /* get the string offset */
                ofs = get_op_int32(&p);

                /* display it */
                p = disp_dstring(vmg_ ofs, p - entry_ptr_native_,
                                 get_self_check(vmg0_));
                break;

            case OPC_SAYVAL:
                /* invoke the default string display function */
                p = disp_string_val(vmg_ p - entry_ptr_native_,
                                    get_self_check(vmg0_));
                break;

            case OPC_THROW:
                /* pop the exception object */
                pop_obj(vmg_ &val);

                /* 
                 *   Throw it.  Note that we pass the start of the current
                 *   instruction as the program counter, since we want to
                 *   find the exception handler (if any) for the current
                 *   instruction, not for the next instruction. 
                 */
                if ((p = do_throw(vmg_ p - 1, val.val.obj)) == 0)
                {
                    /* remember the unhandled exception for re-throwing */
                    unhandled_exc = val.val.obj;

                    /* terminate execution */
                    goto exit_loop;
                }
                break;

            case OPC_INHERIT:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_inherit:
                /* inherit the property */
                prop = (vm_prop_id_t)get_op_uint16(&p);
                p = inh_prop(vmg_ p - entry_ptr_native_, prop, argc);
                break;

            case OPC_PTRINHERIT:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_ptrinherit:
                /* pop the property to be inherited */
                pop_prop(vmg_ &val);

                /* inherit it */
                p = inh_prop(vmg_ p - entry_ptr_native_, val.val.prop, argc);
                break;

            case OPC_EXPINHERIT:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_expinherit:
                /* get the property to inherit */
                prop = (vm_prop_id_t)get_op_uint16(&p);

                /* get the superclass to inherit it from */
                val.set_obj((vm_obj_id_t)get_op_uint32(&p));

                /* 
                 *   inherit it -- process this essentially the same way
                 *   as a normal CALLPROP, since we're going to evaluate
                 *   the given property of the given object, but retain
                 *   the current 'self' object 
                 */
                val2.set_obj(get_self(vmg0_));
                p = get_prop(vmg_ p - entry_ptr_native_,
                             &val, prop, &val2, argc);
                break;

            case OPC_PTREXPINHERIT:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_ptrexpinherit:
                /* pop the property to inherit */
                pop_prop(vmg_ &val);

                /* get the superclass to inherit it from */
                val3.set_obj((vm_obj_id_t)get_op_uint32(&p));

                /* inherit it */
                val2.set_obj(get_self(vmg0_));
                p = get_prop(vmg_ p - entry_ptr_native_,
                             &val3, val.val.prop, &val2, argc);
                break;

            case OPC_DELEGATE:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_delegate:
                /* get the property to inherit */
                prop = (vm_prop_id_t)get_op_uint16(&p);

                /* get the object to delegate to */
                G_stk->pop(&val);

                /* delegate it */
                val2.set_obj(get_self(vmg0_));
                p = get_prop(vmg_ p - entry_ptr_native_,
                             &val, prop, &val2, argc);
                break;

            case OPC_PTRDELEGATE:
                /* get the argument count */
                argc = get_op_uint8(&p);

            do_opc_ptrdelegate:
                /* pop the property to delegate to */
                pop_prop(vmg_ &val);

                /* pop the object to delegate to */
                G_stk->pop(&val2);

                /* delegate it */
                val3.set_obj(get_self(vmg0_));
                p = get_prop(vmg_ p - entry_ptr_native_,
                             &val2, val.val.prop, &val3, argc);
                break;

            case OPC_BUILTIN_A:
                /* get the function index and argument count */
                argc = get_op_uint8(&p);

            do_opc_builtin_a:
                idx = get_op_uint8(&p);

                /* call the function in set #0 */
                call_bif(vmg_ 0, idx, argc);
                break;

            case OPC_BUILTIN_B:
                /* get the function index and argument count */
                argc = get_op_uint8(&p);

            do_opc_builtin_b:
                idx = get_op_uint8(&p);

                /* call the function in set #1 */
                call_bif(vmg_ 1, idx, argc);
                break;

            case OPC_BUILTIN_C:
                /* get the function index and argument count */
                argc = get_op_uint8(&p);

            do_opc_builtin_c:
                idx = get_op_uint8(&p);

                /* call the function in set #2 */
                call_bif(vmg_ 2, idx, argc);
                break;

            case OPC_BUILTIN_D:
                /* get the function index and argument count */
                argc = get_op_uint8(&p);

            do_opc_builtin_d:
                idx = get_op_uint8(&p);

                /* call the function in set #3 */
                call_bif(vmg_ 3, idx, argc);
                break;

            case OPC_BUILTIN1:
                /* get the function index and argument count */
                argc = get_op_uint8(&p);

            do_opc_builtin1:
                idx = get_op_uint8(&p);

                /* get the function set ID */
                set_idx = get_op_uint8(&p);

                /* call the function in set #0 */
                call_bif(vmg_ set_idx, idx, argc);
                break;

            case OPC_BUILTIN2:
                /* get the function index and argument count */
                argc = get_op_uint8(&p);

            do_opc_builtin2:
                idx = get_op_uint16(&p);

                /* get the function set ID */
                set_idx = get_op_uint8(&p);

                /* call the function in set #0 */
                call_bif(vmg_ set_idx, idx, argc);
                break;

            case OPC_CALLEXT:
                //$$$
                err_throw(VMERR_CALLEXT_NOT_IMPL);
                break;

            case OPC_INDEX:
                /* 
                 *   make a safe copy of the object to index, as we're going
                 *   to store the result directly over that stack slot 
                 */
                val = *(valp = G_stk->get(1));

                /* index val by TOS, storing the result at TOS-1 */
                apply_index(vmg_ valp, &val, G_stk->get(0));

                /* discard the index value */
                G_stk->discard();
                break;

            case OPC_IDXLCL1INT8:
                /* get the local */
                valp = get_local(vmg_ get_op_uint8(&p));
                
                /* get the index value */
                val2.set_int(get_op_uint8(&p));

                /* 
                 *   look up the indexed value of the local, storing the
                 *   result in a newly-pushed stack element 
                 */
                apply_index(vmg_ G_stk->push(), valp, &val2);
                break;

            case OPC_IDXINT8:
                /* 
                 *   make a copy of the value to index, so we can overwrite
                 *   the stack slot with the result 
                 */
                val = *(valp = G_stk->get(0));

                /* set up the index value */
                val2.set_int(get_op_uint8(&p));

                /* apply the index, storing the result at TOS */
                apply_index(vmg_ valp, &val, &val2);
                break;

            case OPC_BP:
                /* step back to the breakpoint location itself */
                VM_IF_DEBUGGER(--p);

                /* let the debugger take control */
                VM_IF_DEBUGGER(G_debugger
                               ->step(vmg_ &p, entry_ptr_, TRUE, 0));

                /* if there's no debugger, it's an error */
                VM_IF_NOT_DEBUGGER(err_throw(VMERR_BREAKPOINT));

                /* 
                 *   go back and execute the current instruction - bypass
                 *   single-step tracing into the debugger in this case,
                 *   since the debugger expects when it returns that one
                 *   instruction will always be traced before the debugger
                 *   is re-entered 
                 */
                goto exec_instruction;

            case OPC_NOP:
                /* NO OP - no effect */
                break;

            case OPC_TRNEW1:
                trans = TRUE;
                goto do_opc_new1;

            case OPC_NEW1:
                trans = FALSE;
                /* fall through to do_opc_new1 */

            do_opc_new1:
                /* get the argument count */
                argc = get_op_uint8(&p);

                /* fall through to do_opc_new1_argc */

            do_opc_new1_argc:
                /* get the metaclass ID */
                idx = get_op_uint8(&p);
                
                /* create the new object */
                p = new_and_store_r0(vmg_ p, idx, argc, trans);
                break;

            case OPC_TRNEW2:
                trans = TRUE;
                goto do_opc_new2;

            case OPC_NEW2:
                trans = FALSE;
                /* fall through to do_opc_new2 */

            do_opc_new2:
                /* get the argument count */
                argc = get_op_uint16(&p);

                /* fall through to do_opc_new2_argc */

            do_opc_new2_argc:
                /* get the metaclass ID */
                idx = get_op_uint16(&p);

                /* create the new object */
                p = new_and_store_r0(vmg_ p, idx, argc, trans);
                break;

            case OPC_INCLCL:
                /* get the local */
                valp = get_local(vmg_ get_op_uint16(&p));
                
                /* check if it's a number */
                if (valp->is_numeric())
                {
                    /* it's a number - just increment the value */
                    ++(valp->val.intval);
                }
                else
                {
                    /* it's a non-numeric value - do the full addition */
                    val2.set_int(1);
                    compute_sum(vmg_ valp, &val2);
                }
                break;

            case OPC_DECLCL:
                /* get the local */
                valp = get_local(vmg_ get_op_uint16(&p));

                /* check for a number */
                if (valp->is_numeric())
                {
                    /* it's a number - just decrement the value */
                    --(valp->val.intval);
                }
                else
                {
                    /* non-numeric - we must do the full subtraction work */
                    val2.set_int(1);
                    compute_diff(vmg_ valp, &val2);
                }
                break;

            case OPC_ADDILCL1:
                /* get the local */
                valp = get_local(vmg_ get_op_uint8(&p));

                /* if it's numeric, handle it in-line */
                if (valp->is_numeric())
                {
                    /* it's a number - just add the value */
                    valp->val.intval += get_op_int8(&p);
                }
                else
                {
                    /* get the number to add */
                    val2.set_int(get_op_int8(&p));

                    /* compute the sum, leaving the result in the local */
                    compute_sum(vmg_ valp, &val2);
                }
                break;

            case OPC_ADDILCL4:
                /* get the local */
                valp = get_local(vmg_ get_op_uint16(&p));

                /* if it's a number, handle it in-line */
                if (valp->is_numeric())
                {
                    /* it's a number - just add the value */
                    valp->val.intval += get_op_int32(&p);
                }
                else
                {
                    /* get the number to add */
                    val2.set_int(get_op_int32(&p));

                    /* compute the sum, leaving the result in the local */
                    compute_sum(vmg_ valp, &val2);
                }
                break;

            case OPC_ADDTOLCL:
                /* get the local */
                valp = get_local(vmg_ get_op_uint16(&p));

                /* get the value to add */
                valp2 = G_stk->get(0);

                /* if they're both numeric, handle in-line */
                if (valp->is_numeric() && valp2->is_numeric())
                {
                    /* add the value to the local */
                    valp->val.intval += valp2->val.intval;
                }
                else
                {
                    /* compute the sum, leaving the result in the local */
                    compute_sum(vmg_ valp, valp2);
                }

                /* discard the addend */
                G_stk->discard();
                break;

            case OPC_SUBFROMLCL:
                /* get the local */
                valp = get_local(vmg_ get_op_uint16(&p));

                /* get the value to add */
                valp2 = G_stk->get(0);

                /* if they're both numeric, handle in-line */
                if (valp->is_numeric() && valp2->is_numeric())
                {
                    /* subtract the value from the local */
                    valp->val.intval -= valp2->val.intval;
                }
                else
                {
                    /* subtract the values, leaving the result in the local */
                    compute_diff(vmg_ valp, valp2);
                }

                /* discard the value subtracted */
                G_stk->discard();
                break;

            case OPC_ZEROLCL1:
                /* get the local and set it to zero */
                get_local(vmg_ get_op_uint8(&p))->set_int(0);
                break;

            case OPC_ZEROLCL2:
                /* get the local and set it to zero */
                get_local(vmg_ get_op_uint16(&p))->set_int(0);
                break;

            case OPC_NILLCL1:
                /* get the local and set it to zero */
                get_local(vmg_ get_op_uint8(&p))->set_nil();
                break;

            case OPC_NILLCL2:
                /* get the local and set it to zero */
                get_local(vmg_ get_op_uint16(&p))->set_nil();
                break;

            case OPC_ONELCL1:
                /* get the local and set it to zero */
                get_local(vmg_ get_op_uint8(&p))->set_int(1);
                break;

            case OPC_ONELCL2:
                /* get the local and set it to zero */
                get_local(vmg_ get_op_uint16(&p))->set_int(1);
                break;

            case OPC_SETLCL1:
                /* get a pointer to the local */
                valp = get_local(vmg_ get_op_uint8(&p));
                
                /* pop the value into the local */
                popval(vmg_ valp);
                break;

            case OPC_SETLCL2:
                /* get a pointer to the local */
                valp = get_local(vmg_ get_op_uint16(&p));

                /* pop the value into the local */
                popval(vmg_ valp);
                break;

            case OPC_SETLCL1R0:
                /* store R0 in the specific local */
                *get_local(vmg_ get_op_uint8(&p)) = r0_;
                break;

            case OPC_SETARG1:
                /* get a pointer to the parameter */
                valp = get_param(vmg_ get_op_uint8(&p));

                /* pop the value into the parameter */
                popval(vmg_ valp);
                break;

            case OPC_SETARG2:
                /* get a pointer to the parameter */
                valp = get_param(vmg_ get_op_uint16(&p));

                /* pop the value into the parameter */
                popval(vmg_ valp);
                break;

            case OPC_SETIND:
                /* pop the index */
                popval(vmg_ &val2);

                /* pop the value to be indexed */
                popval(vmg_ &val);

                /* pop the value to assign */
                popval(vmg_ &val3);

                /* assign into the index */
                set_index(vmg_ &val, &val2, &val3);

                /* push the new container value */
                pushval(vmg_ &val);
                break;

            case OPC_SETINDLCL1I8:
                /* get the local */
                valp = get_local(vmg_ get_op_uint8(&p));

                /* get the index value */
                val2.set_int(get_op_uint8(&p));

                /* pop the value to assign */
                popval(vmg_ &val3);

                /* 
                 *   set the index value - this will update the local
                 *   variable directly if the container value changes 
                 */
                set_index(vmg_ valp, &val2, &val3);
                break;

            case OPC_SETPROP:
                /* get the object whose property we're setting */
                pop_obj(vmg_ &val);

                /* pop the value we're setting */
                popval(vmg_ &val2);

                /* set the value */
                set_prop(vmg_ val.val.obj, get_op_uint16(&p), &val2);
                break;

            case OPC_PTRSETPROP:
                /* get the property and object to set */
                pop_prop(vmg_ &val);
                pop_obj(vmg_ &val2);

                /* get the value to set */
                popval(vmg_ &val3);

                /* set it */
                set_prop(vmg_ val2.val.obj, val.val.prop, &val3);
                break;

            case OPC_SETPROPSELF:
                /* get the value to set */
                popval(vmg_ &val);

                /* set it */
                set_prop(vmg_ get_self(vmg0_), get_op_uint16(&p), &val);
                break;

            case OPC_OBJSETPROP:
                /* get the objet */
                obj = (vm_obj_id_t)get_op_uint32(&p);

                /* get the new value */
                popval(vmg_ &val);

                /* set the property */
                set_prop(vmg_ obj, get_op_uint16(&p), &val);
                break;

#ifdef OS_FILL_OUT_CASE_TABLES
            /*
             *   Since we this switch is the innermost inner loop of the VM,
             *   we go to some extra lengths to optimize it where possible.
             *   See tads2/osifc.h for information on how to use
             *   OS_FILL_OUT_CASE_TABLES and OS_IMPOSSIBLE_DEFAULT_CASE.
             *   
             *   Our controlling expression is an unsigned character value,
             *   so we know the range of possible values will be limited to
             *   0-255.  Therefore, we simply need to provide a "case"
             *   alternative for every invalid opcode.  To further encourage
             *   the compiler to favor speed here, we specifically put
             *   different code in every one of these case alternatives, to
             *   force the compiler to generate a separate jump location for
             *   each one; some compilers will generate a two-level jump
             *   table if many cases point to shared code, to reduce the size
             *   of the table, but we don't want that here because this
             *   switch is critical to VM performance so we want it as fast
             *   as possible.  
             */
            case 0x00: val.val.intval = 0x00;
            case 0x10: val.val.intval = 0x10;
            case 0x11: val.val.intval = 0x11;
            case 0x12: val.val.intval = 0x12;
            case 0x13: val.val.intval = 0x13;
            case 0x14: val.val.intval = 0x14;
            case 0x15: val.val.intval = 0x15;
            case 0x16: val.val.intval = 0x16;
            case 0x17: val.val.intval = 0x17;
            case 0x18: val.val.intval = 0x18;
            case 0x19: val.val.intval = 0x19;
            case 0x1A: val.val.intval = 0x1A;
            case 0x1B: val.val.intval = 0x1B;
            case 0x1C: val.val.intval = 0x1C;
            case 0x1D: val.val.intval = 0x1D;
            case 0x1E: val.val.intval = 0x1E;
            case 0x1F: val.val.intval = 0x1F;
            case 0x30: val.val.intval = 0x30;
            case 0x31: val.val.intval = 0x31;
            case 0x32: val.val.intval = 0x32;
            case 0x33: val.val.intval = 0x33;
            case 0x34: val.val.intval = 0x34;
            case 0x35: val.val.intval = 0x35;
            case 0x36: val.val.intval = 0x36;
            case 0x37: val.val.intval = 0x37;
            case 0x38: val.val.intval = 0x38;
            case 0x39: val.val.intval = 0x39;
            case 0x3A: val.val.intval = 0x3A;
            case 0x3B: val.val.intval = 0x3B;
            case 0x3C: val.val.intval = 0x3C;
            case 0x3D: val.val.intval = 0x3D;
            case 0x3E: val.val.intval = 0x3E;
            case 0x3F: val.val.intval = 0x3F;
            case 0x46: val.val.intval = 0x46;
            case 0x47: val.val.intval = 0x47;
            case 0x48: val.val.intval = 0x48;
            case 0x49: val.val.intval = 0x49;
            case 0x4A: val.val.intval = 0x4A;
            case 0x4B: val.val.intval = 0x4B;
            case 0x4C: val.val.intval = 0x4C;
            case 0x4D: val.val.intval = 0x4D;
            case 0x4E: val.val.intval = 0x4E;
            case 0x4F: val.val.intval = 0x4F;
            case 0x53: val.val.intval = 0x53;
            case 0x55: val.val.intval = 0x55;
            case 0x56: val.val.intval = 0x56;
            case 0x57: val.val.intval = 0x57;
            case 0x5A: val.val.intval = 0x5A;
            case 0x5B: val.val.intval = 0x5B;
            case 0x5C: val.val.intval = 0x5C;
            case 0x5D: val.val.intval = 0x5D;
            case 0x5E: val.val.intval = 0x5E;
            case 0x5F: val.val.intval = 0x5F;
            case 0x6E: val.val.intval = 0x6E;
            case 0x6F: val.val.intval = 0x6F;
            case 0x70: val.val.intval = 0x70;
            case 0x71: val.val.intval = 0x71;
            case 0x79: val.val.intval = 0x79;
            case 0x7A: val.val.intval = 0x7A;
            case 0x7B: val.val.intval = 0x7B;
            case 0x7C: val.val.intval = 0x7C;
            case 0x7D: val.val.intval = 0x7D;
            case 0x7E: val.val.intval = 0x7E;
            case 0x7F: val.val.intval = 0x7F;
            case 0x8F: val.val.intval = 0x8F;
            case 0xA2: val.val.intval = 0xA2;
            case 0xA3: val.val.intval = 0xA3;
            case 0xA4: val.val.intval = 0xA4;
            case 0xA5: val.val.intval = 0xA5;
            case 0xA6: val.val.intval = 0xA6;
            case 0xA7: val.val.intval = 0xA7;
            case 0xA8: val.val.intval = 0xA8;
            case 0xA9: val.val.intval = 0xA9;
            case 0xAA: val.val.intval = 0xAA;
            case 0xAB: val.val.intval = 0xAB;
            case 0xAC: val.val.intval = 0xAC;
            case 0xAD: val.val.intval = 0xAD;
            case 0xAE: val.val.intval = 0xAE;
            case 0xAF: val.val.intval = 0xAF;
            case 0xBD: val.val.intval = 0xBD;
            case 0xBE: val.val.intval = 0xBE;
            case 0xBF: val.val.intval = 0xBF;
            case 0xC4: val.val.intval = 0xC4;
            case 0xC5: val.val.intval = 0xC5;
            case 0xC6: val.val.intval = 0xC6;
            case 0xC7: val.val.intval = 0xC7;
            case 0xC8: val.val.intval = 0xC8;
            case 0xC9: val.val.intval = 0xC9;
            case 0xCA: val.val.intval = 0xCA;
            case 0xCB: val.val.intval = 0xCB;
            case 0xCC: val.val.intval = 0xCC;
            case 0xCD: val.val.intval = 0xCD;
            case 0xCE: val.val.intval = 0xCE;
            case 0xCF: val.val.intval = 0xCF;
            case 0xDC: val.val.intval = 0xDC;
            case 0xDD: val.val.intval = 0xDD;
            case 0xDE: val.val.intval = 0xDE;
            case 0xDF: val.val.intval = 0xDF;
            case 0xF0: val.val.intval = 0xF0;
            case 0xF3: val.val.intval = 0xF3;
            case 0xF4: val.val.intval = 0xF4;
            case 0xF5: val.val.intval = 0xF5;
            case 0xF6: val.val.intval = 0xF6;
            case 0xF7: val.val.intval = 0xF7;
            case 0xF8: val.val.intval = 0xF8;
            case 0xF9: val.val.intval = 0xF9;
            case 0xFA: val.val.intval = 0xFA;
            case 0xFB: val.val.intval = 0xFB;
            case 0xFC: val.val.intval = 0xFC;
            case 0xFD: val.val.intval = 0xFD;
            case 0xFE: val.val.intval = 0xFE;
            case 0xFF: val.val.intval = 0xFF;
                err_throw(VMERR_INVALID_OPCODE);

            OS_IMPOSSIBLE_DEFAULT_CASE

#else /* OS_FILL_OUT_CASE_TABLES */
            case 0:
                /* 
                 *   Explicitly call out this invalid instruction case so
                 *   that we can avoid extra work in computing the switch.
                 *   Some compilers will be smart enough to observe that we
                 *   populate the full range of possible values (0-255) for
                 *   the datatype of the switch control expression, and thus
                 *   will build jump tables that can be jumped through
                 *   without range-checking the value.  (No range checking
                 *   is necessary, because a uchar simply cannot hold any
                 *   values outside of the 0-255 range.)  This doesn't
                 *   guarantee that the compiler will be smart, but it does
                 *   help with some compilers and shouldn't hurt performance
                 *   with those that don't make any use of the situation.  
                 */
                err_throw(VMERR_INVALID_OPCODE);
                
            case 0xFF:
                /* 
                 *   explicitly call out this invalid instruction for the
                 *   same reasons we call out case 0 above 
                 */
                err_throw(VMERR_INVALID_OPCODE);

            default:
                /* unrecognized opcode */
                err_throw(VMERR_INVALID_OPCODE);
                break;

#endif /* OS_FILL_OUT_CASE_TABLES */
            }
        }

        /*
         *   We jump to this label when it's time to terminate execution
         *   and return to the host environment which called us. 
         */
    exit_loop:
        /* note that we're ready to return */
        done = TRUE;
    }
    err_catch(err)
    {
        int i;
        volatile int released_reserve = FALSE;

        err_try
        {
            /* 
             *   Return to the start of the most recent instruction - we've
             *   already at least partially decoded the instruction, so we
             *   won't be pointing to its first byte.  Note that last_pc is
             *   a non-register variable (because we take its address to
             *   store in pc_ptr_), so it will correctly indicate the
             *   current instruction even though we've jumped here via
             *   longjmp.  
             */
            p = last_pc;
                
            /* 
             *   Create a new exception object to describe the error.  The
             *   arguments to the constructor are the error number and the
             *   error parameters.
             *   
             *   If the error code is "unhandled exception," it means that
             *   an exception occurred in a recursive interpreter
             *   invocation, and the exception wasn't handled within the
             *   code called recursively; in this case, we can simply
             *   re-throw the original error, and perhaps handle it in the
             *   context of the current code.  
             */
            if (err->get_error_code() == VMERR_UNHANDLED_EXC)
            {
                /* get the original exception object from the error stack */
                obj = (vm_obj_id_t)err->get_param_ulong(0);
            }
            else
            {
                /* step into the debugger, if it's present */
                VM_IF_DEBUGGER(
                {
                    const uchar *dbgp;
                    
                    /* 
                     *   If we're in the process of halting the VM, don't
                     *   bother stepping into the debugger.  We'll check the
                     *   same thing in a moment, after we get back from
                     *   stepping into the debugger, but this check isn't
                     *   redundant: we could already be halting even before
                     *   we enter the debugger here, because we could be
                     *   unwinding the native (C++) error stack on our way
                     *   out from such a halt. 
                     */
                    if (halt_vm_)
                    {
                        done = TRUE;
                        goto skip_throw;
                    }

                    /* make a copy of the PC for the debugger's use */
                    dbgp = p;
                    
                    /* step into the debugger */
                    G_debugger->step(vmg_ &dbgp, entry_ptr_, FALSE,
                                     err->get_error_code());
                    
                    /* 
                     *   if the VM was halted while in the debugger, stop
                     *   running immediately - do not process the exception
                     *   any further 
                     */
                    if (halt_vm_)
                    {
                        done = TRUE;
                        goto skip_throw;
                    }
                    
                    /* 
                     *   if they moved the execution pointer, resume
                     *   execution at the new point, discarding the
                     *   exception 
                     */
                    if (dbgp != p)
                    {
                        /* resume execution at the new location */
                        p = dbgp;
                        
                        /* discard the exception and resume execution */
                        goto skip_throw;
                    }
                }
                );

                /* 
                 *   If this is a stack overflow exception, there's probably
                 *   not enough stack left to create the exception object.
                 *   Fortunately, we have an emergency stack reserve just for
                 *   such conditions, so release it now, hopefully giving us
                 *   enough room to work with to construct the exception.  
                 */
                if (err->get_error_code() == VMERR_STACK_OVERFLOW)
                    released_reserve = G_stk->release_reserve();
            
                /* push the error parameters (in reverse order) */
                for (i = err->get_param_count() ; i > 0 ; )
                {
                    /* go to the next parameter */
                    --i;
                    
                    /* see what we have and push an appropriate value */
                    switch(err->get_param_type(i-1))
                    {
                    case ERR_TYPE_INT:
                        /* push the integer value */
                        push_int(vmg_ err->get_param_int(i));
                        break;

                    case ERR_TYPE_ULONG:
                        /* push the value */
                        push_int(vmg_ (int32)err->get_param_ulong(i));
                        break;

                    case ERR_TYPE_TEXTCHAR:
                        /* push a new string with the text */
                        push_obj(vmg_ CVmObjString::create(vmg_ FALSE,
                            err->get_param_text(i),
                            get_strlen(err->get_param_text(i))));
                        break;

                    case ERR_TYPE_CHAR:
                        /* push a new string with the text */
                        push_obj(vmg_ CVmObjString::create(vmg_ FALSE,
                            err->get_param_char(i),
                            strlen(err->get_param_char(i))));
                        break;

                    default:
                        /* unrecognized type - push nil for now */
                        push_nil(vmg0_);
                        break;
                    }
                }

                /* 
                 *   if there's a RuntimeError base class defined, create an
                 *   instance; otherwise, create a simple instance of the
                 *   basic object type to throw as a placeholder, since the
                 *   program hasn't made any provision to catch run-time
                 *   errors 
                 */
                if (G_predef->rterr != VM_INVALID_OBJ)
                {
                    /* push the error number */
                    push_int(vmg_ err->get_error_code());
                    
                    /* 
                     *   If we're not in the debugger, set up a recursive
                     *   call frame for the constructor invocation.  We'll
                     *   do this on any recursive call into byte code if
                     *   we're running in the debugger, so we only need to
                     *   do this in the non-debug version.
                     *   
                     *   This extra recursive frame is needed in this one
                     *   case when in non-debug mode because the constructor
                     *   to the exception object might want to look at the
                     *   stack trace.  In order for the location where the
                     *   error actually occurred to be included in the stack
                     *   trace, we need to push a recursive call frame that
                     *   points back to that location.  
                     */
                    VM_IF_NOT_DEBUGGER(enter_recursive_frame(
                        vmg_ err->get_param_count() + 1, &last_pc));

                    /* 
                     *   Create the new RuntimeException instance.  Run the
                     *   constructor in a recursive invocation of the
                     *   interpreter (by passing a null PC pointer).  
                     */
                    vm_objp(vmg_ G_predef->rterr)
                        ->create_instance(vmg_ G_predef->rterr, 0,
                                          err->get_param_count() + 1);

                    /* get the object from R0 */
                    if (r0_.typ != VM_OBJ)
                        err_throw(VMERR_OBJ_VAL_REQD);
                    obj = r0_.val.obj;
                }
                else
                {
                    /* 
                     *   There's no RuntimeError object defined by the image
                     *   file, so create a basic object to throw.  This
                     *   won't convey any information to the program except
                     *   that it's not one of the errors they're expecting;
                     *   this is fine, since they have made no provisions to
                     *   catch VM errors, as demonstrated by their lack of a
                     *   RuntimeError definition.  
                     */
                    obj = CVmObjTads::create(vmg_ FALSE, 0, 1);
                }

                /* 
                 *   if possible, set the exceptionMessage property in the
                 *   new exception object to the default error message for
                 *   the run-time error we're processing 
                 */
                if (G_predef->rterrmsg_prop != VM_INVALID_PROP)
                {
                    const char *msg;
                    char buf[256];
                    vm_obj_id_t str_obj;

                    /* format the message text */
                    msg = err_get_msg(vm_messages, vm_message_count,
                                      err->get_error_code(), FALSE);
                    err_format_msg(buf, sizeof(buf), msg, err);

                    /* 
                     *   momentarily push the new exception object, so we
                     *   don't lose track of it if we run garbage collection
                     *   here 
                     */
                    push_obj(vmg_ obj);

                    /* create a string object with the message text */
                    str_obj =
                        CVmObjString::create(vmg_ FALSE, buf, strlen(buf));

                    /* 
                     *   before we can build a stack trace, let the debugger
                     *   synchronize its current position information 
                     */
                    VM_IF_DEBUGGER(
                        G_debugger->sync_exec_pos(vmg_ p, entry_ptr_));
                    
                    /* set the property in the new object */
                    val.set_obj(str_obj);
                    vm_objp(vmg_ obj)
                        ->set_prop(vmg_ G_undo, obj,
                                   G_predef->rterrmsg_prop, &val);
                    
                    /* we don't need gc protection any more */
                    G_stk->discard();
                }
            }
        
            /* 
             *   If we released the stack reserve, take it back.  We've
             *   finished creating the exception object, so we don't need the
             *   emergency stack space any more.  We want to put it back now
             *   that we're done with it so that it'll be there for us if we
             *   should run into another stack overflow in the future.  
             */
            if (released_reserve)
                G_stk->recover_reserve();

            /* throw the exception */
            if ((p = do_throw(vmg_ p, obj)) == 0)
            {
                /* remember the unhandled exception for a moment */
                unhandled_exc = obj;
            }
            
            /* come here to skip throwing the exception */
            VM_IF_DEBUGGER(skip_throw: );
        }
        err_catch(exc2)
        {
            /* 
             *   we got another exception trying to handle the first
             *   exception - just throw the error again, but at least clean
             *   up statics on the way out 
             */
            pc_ptr_ = old_pc_ptr;

            /* if we released the stack reserve, take it back */
            if (released_reserve)
                G_stk->recover_reserve();

            /* re-throw the error */
            err_rethrow();
        }
        err_end;
    }
    err_end;

    /* 
     *   If an unhandled exception occurred, re-throw it.  This will wrap our
     *   exception object in a C++ object and throw it through our C++
     *   err_try/err_catch exception mechanism, so that the exception is
     *   thrown out of the recursive native-code invoker.  
     */
    if (unhandled_exc != VM_INVALID_OBJ)
    {
        /* restore the enclosing PC pointer */
        pc_ptr_ = old_pc_ptr;
        
        /* re-throw the unhandled exception */
        err_throw_a(VMERR_UNHANDLED_EXC, 1,
                    ERR_TYPE_ULONG, (unsigned long)unhandled_exc);
    }

    /* if we're not done, go back and resume execution */
    if (!done)
        goto resume_execution;

    /* restore the enclosing PC pointer */
    pc_ptr_ = old_pc_ptr;
}


/* ------------------------------------------------------------------------ */
/*
 *   Throw an exception of the given class, with the constructor arguments
 *   on the stack.  
 */
void CVmRun::throw_new_class(VMG_ vm_obj_id_t cls, uint argc,
                             const char *fallback_msg)
{
    /* if the class isn't defined, use the basic run-time exception */
    if (cls != VM_INVALID_OBJ)
    {
        /* create the object */
        vm_objp(vmg_ cls)->create_instance(vmg_ cls, 0, argc);
        
        /* make sure we created an object */
        if (r0_.typ == VM_OBJ)
        {
            vm_obj_id_t exc_obj;
            
            /* get the object from R0 */
            exc_obj = r0_.val.obj;
            
            /* 
             *   throw an 'unhandled exception' with this object as the
             *   parameter; the execution loop will catch it and dispatch it
             *   properly 
             */
            err_throw_a(VMERR_UNHANDLED_EXC, 1,
                        ERR_TYPE_ULONG, (unsigned long)exc_obj);
        }
    }
    
    /* 
     *   the imported exception class isn't defined, or we failed to create
     *   it; throw a generic intrinsic class exception with the fallback
     *   message string 
     */
    err_throw_a(VMERR_INTCLS_GENERAL_ERROR, 1, ERR_TYPE_CHAR, fallback_msg);
}


/* ------------------------------------------------------------------------ */
/*
 *   Throw an exception.  Returns true if an exception handler was found,
 *   which means that execution can proceed; returns false if no handler
 *   was found, in which case the execution loop must throw the exception
 *   to its caller.  
 */
const uchar *CVmRun::do_throw(VMG_ const uchar *pc, vm_obj_id_t exception_obj)
{
    /*
     *   Search the stack for a handler for this exception class.  Start
     *   at the current stack frame; if we find a handler here, use it;
     *   otherwise, unwind the stack to the enclosing frame and search for
     *   a handler there; repeat until we exhaust the stack.  
     */
    for (;;)
    {
        CVmExcTablePtr tab;
        const uchar *func_start;
        uint ofs;

        /* get a pointer to the start of the current function */
        func_start = entry_ptr_native_;

        /* set up a pointer to the current exception table */
        if (tab.set(func_start))
        {
            size_t cnt;
            size_t i;
            CVmExcEntryPtr entry;

            /* calculate our offset in the current function */
            ofs = pc - func_start;

            /* set up a pointer to the first table entry */
            tab.set_entry_ptr(vmg_ &entry, 0);
            
            /* loop through the entries */
            for (i = 0, cnt = tab.get_count() ; i < cnt ;
                 ++i, entry.inc(vmg0_))
            {
                /* 
                 *   Check to see if we're in the range for this entry.
                 *   If this entry covers the appropriate range, and the
                 *   exception we're handling is of the class handled by
                 *   this exception (or derives from that class), this
                 *   handler handles this exception. 
                 */
                if (ofs >= entry.get_start_ofs()
                    && ofs <= entry.get_end_ofs()
                    && (entry.get_exception() == VM_INVALID_OBJ
                        || exception_obj == entry.get_exception()
                        || (vm_objp(vmg_ exception_obj)
                            ->is_instance_of(vmg_ entry.get_exception()))))
                {
                    /* 
                     *   this is it - move the program counter to the
                     *   first byte of the handler's code 
                     */
                    pc = func_start + entry.get_handler_ofs();

                    /* push the exception so that the handler can get at it */
                    push_obj(vmg_ exception_obj);

                    /* return the new program counter at which to resume */
                    return pc;
                }
            }
        }

        /* 
         *   We didn't find a handler in the current function - unwind the
         *   stack one level, using an ordinary RETURN operation (we're not
         *   really returning, though, so we don't need to provide a return
         *   value).  First, though, check to make sure there is an enclosing
         *   frame at all - if there's not, we can simply return immediately.
         */
        if (frame_ptr_ == 0)
        {
            /* there's no enclosing frame, so there's nowhere to go */
            return 0;
        }

        /* try unwinding the stack a level */
        if ((pc = do_return(vmg0_)) == 0)
        {
            /* 
             *   The enclosing frame is a recursive invocation, so we cannot
             *   unwind any further at this point.  Return null to indicate
             *   that the exception was not handled and should be thrown out
             *   of the current recursive VM invocation.  
             */
            return 0;
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Call a built-in function 
 */
void CVmRun::call_bif(VMG_ uint set_index, uint func_index, uint argc)
{
    /* 
     *   Call the function -- presume the compiler has ensured that the
     *   function set index is valid for the load image, and that the
     *   function index is valid for the function set; all of this can be
     *   determined at compile time, since function sets are statically
     *   defined. 
     */
    G_bif_table->call_func(vmg_ set_index, func_index, argc);
}


/* ------------------------------------------------------------------------ */
/*
 *   Call a function pointer 
 */
const uchar *CVmRun::call_func_ptr(VMG_ const vm_val_t *funcptr, uint argc,
                                   const char *recurse_name, uint caller_ofs)
{
    vm_val_t prop_val;

    /* 
     *   if it's an object, and the predefined property ObjectCallProp is
     *   defined, and the object defines this property, call this property
     *   in the object 
     */
    if (funcptr->typ == VM_OBJ
        && G_predef->obj_call_prop != VM_INVALID_PROP)
    {
        vm_obj_id_t srcobj;
        int found;
        uint objcall_argc = 0;
        
        /* make sure the object defines ObjectCallProp */
        found = vm_objp(vmg_ funcptr->val.obj)
                ->get_prop(vmg_ G_predef->obj_call_prop, &prop_val,
                           funcptr->val.obj, &srcobj, &objcall_argc);
        
        /* 
         *   if we didn't find it, this object can't be used in this
         *   fashion - throw an error 
         */
        if (!found)
            err_throw(VMERR_FUNCPTR_VAL_REQD);
        
        /* 
         *   if this is a function pointer, call the function pointer with
         *   the function object as 'self' 
         */
        if (prop_val.typ == VM_FUNCPTR)
        {
            /* call the function and return the new program counter */
            return do_call(vmg_ caller_ofs, prop_val.val.ofs, argc,
                           funcptr->val.obj, VM_INVALID_PROP,
                           funcptr->val.obj, srcobj, recurse_name);
        }
        
        /* proceed with the new value */
        funcptr = &prop_val;
    }
    
    /* if it's not a function pointer, it's an error */
    if (funcptr->typ != VM_FUNCPTR)
        err_throw(VMERR_FUNCPTR_VAL_REQD);
    
    /* call the function */
    return do_call(vmg_ caller_ofs, funcptr->val.ofs, argc,
                   VM_INVALID_OBJ, VM_INVALID_PROP,
                   VM_INVALID_OBJ, VM_INVALID_OBJ, recurse_name);
}

/* ------------------------------------------------------------------------ */
/*
 *   Call a function, non-recursively. 
 *   
 *   This is a separate form of do_call(), but simplified for cases where we
 *   know in advance that we won't need to check for recursion and when we
 *   know in advance that we're calling a function and thus have no 'self'
 *   or other method context objects.  These simplifications reduce the
 *   amount of work we have to do, so that ordinary function calls run a
 *   little faster than they would if we used the full do_call() routine.  
 */
const uchar *CVmRun::do_call_func_nr(VMG_ uint caller_ofs,
                                     pool_ofs_t target_ofs, uint argc)
{
    const uchar *target_ofs_ptr;
    CVmFuncPtr hdr_ptr;
    uint i;
    vm_val_t *fp;
    int lcl_cnt;

    /* store nil in R0 */
    r0_.set_nil();

    /* translate the target address */
    target_ofs_ptr = (const uchar *)G_code_pool->get_ptr(target_ofs);

    /* set up a pointer to the new function header */
    hdr_ptr.set(target_ofs_ptr);

    /* get the number of locals from the header */
    lcl_cnt = hdr_ptr.get_local_cnt();

    /* get the target's stack space needs and check for stack overflow */
    if (!G_stk->check_space(hdr_ptr.get_stack_depth() + 8))
        err_throw(VMERR_STACK_OVERFLOW);

    /* allocate the stack frame */
    fp = G_stk->push(8 + lcl_cnt);

    /* there's no target property, target object, defining object, or self */
    (fp++)->set_propid(VM_INVALID_PROP);
    (fp++)->set_nil();
    (fp++)->set_nil();
    (fp++)->set_nil();

    /* push the caller's code offset */
    (fp++)->set_codeofs(caller_ofs);

    /* push the current entrypoint code offset */
    (fp++)->set_codeofs(entry_ptr_);

    /* push the actual parameter count */
    (fp++)->set_int((int32)argc);

    /* push the current frame pointer */
    (fp++)->set_stack(frame_ptr_);

    /* verify the argument count */
    if (!hdr_ptr.argc_ok(argc))
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* set up the new stack frame */
    frame_ptr_ = fp;

    /* load EP with the new code offset */
    entry_ptr_ = target_ofs;
    entry_ptr_native_ = target_ofs_ptr;

    /* push nil for each local */
    for (i = lcl_cnt ; i != 0 ; --i)
        (fp++)->set_nil();

    /* create and activate the new function's profiler frame */
    VM_IF_PROFILER(if (profiling_)
        prof_enter(target_ofs, VM_INVALID_OBJ, VM_INVALID_PROP));

    /* return the new program counter */
    return target_ofs_ptr + get_funchdr_size();
}

/* ------------------------------------------------------------------------ */
/*
 *   Call a function or method
 */
const uchar *CVmRun::do_call(VMG_ uint caller_ofs,
                             pool_ofs_t target_ofs, uint argc,
                             vm_obj_id_t self, vm_prop_id_t target_prop,
                             vm_obj_id_t orig_target_obj,
                             vm_obj_id_t defining_obj,
                             const char *recurse_name)
{
    const uchar *target_ofs_ptr;
    CVmFuncPtr hdr_ptr;
    uint i;
    vm_val_t *fp;
    int lcl_cnt;

    /* store nil in R0 */
    r0_.set_nil();

    /* 
     *   If we have a debugger, and this is a recursive call, set up a
     *   frame for the recursive call, so that the debugger can look up
     *   the stack to the byte-code caller of the native code that's
     *   recursing into the VM.
     *   
     *   This is unnecessary if there's no debugger; the only reason we
     *   need a special frame on native recursion is to allow the debugger
     *   to traverse the stack correctly through the native call.  
     */
    VM_IF_DEBUGGER(if (caller_ofs == 0)
        enter_recursive_frame(vmg_ argc, pc_ptr_));

    /* 
     *   We're done with the old code segment now, so we can safely
     *   translate a new address.  Get the physical address we're calling
     *   -- this will swap in the new code segment if necessary.  
     */
    target_ofs_ptr = (const uchar *)G_code_pool->get_ptr(target_ofs);

    /* set up a pointer to the new function header */
    hdr_ptr.set(target_ofs_ptr);

    /* get the number of locals from the header */
    lcl_cnt = hdr_ptr.get_local_cnt();

    /* 
     *   Get the space needs of the new function, and ensure we have enough
     *   stack space available.  Include the size of the frame that we store
     *   (the original target object, the target property, the defining
     *   object, the 'self' object, the caller's code offset, the caller's
     *   entrypoint offset, the actual parameter count, and the enclosing
     *   frame pointer) in our space needs.  
     */
    if (!G_stk->check_space(hdr_ptr.get_stack_depth() + 8))
    {
        /* 
         *   If we just entered a recursive frame, remove it.  This will
         *   allow us to stop in the debugger in the byte code that triggered
         *   the recursive call. 
         */
        VM_IF_DEBUGGER(if (caller_ofs == 0)
            leave_recursive_frame(vmg0_));

        /* throw the error */
        err_throw(VMERR_STACK_OVERFLOW);
    }

    /* allocate the stack frame */
    fp = G_stk->push(8 + lcl_cnt);

    /* push the target property */
    (fp++)->set_propid(target_prop);

    /* 
     *   if there's no 'self' object, push nil's for the object context;
     *   otherwise, push the object context 
     */
    if (self == VM_INVALID_OBJ)
    {
        /* push nil for target, defining, and self */
        (fp++)->set_nil();
        (fp++)->set_nil();
        (fp++)->set_nil();
    }
    else
    {
        /* push the original target object */
        (fp++)->set_obj(orig_target_obj);

        /* push the defining object */
        (fp++)->set_obj(defining_obj);

        /* push 'self' */
        (fp++)->set_obj(self);
    }

    /* 
     *   Push the caller's code offset.  Note that if the caller's offset is
     *   zero, it indicates that the caller is not the byte-code interpreter
     *   and that this is a recursive invocation; we represent recursive
     *   frames using a zero caller offset, to we can just use the zero
     *   value as given in this case. 
     */
    (fp++)->set_codeofs(caller_ofs);

    /* push the current entrypoint code offset */
    (fp++)->set_codeofs(entry_ptr_);

    /* push the actual parameter count */
    (fp++)->set_int((int32)argc);

    /* push the current frame pointer */
    (fp++)->set_stack(frame_ptr_);

    /* 
     *   check the argument count - do this before establishing the new
     *   frame and entry pointers, so that if we report a stack traceback in
     *   the debugger, we'll report the error in the calling frame, which is
     *   where it really belongs 
     */
    if (!hdr_ptr.argc_ok(argc))
    {
        /* leave the recursive frame, if we entered one */
        VM_IF_DEBUGGER(if (caller_ofs == 0)
            leave_recursive_frame(vmg0_));

        /* 
         *   if we're making a recursive call, throw an error indicating
         *   what kind of recursive call we're making 
         */
        if (recurse_name != 0)
        {
            /* throw the named generic argument mismatch error */
            err_throw_a(VMERR_WRONG_NUM_OF_ARGS_CALLING, 1,
                        ERR_TYPE_CHAR, recurse_name);
        }
        else
        {
            /* throw the generic argument mismatch error */
            err_throw(VMERR_WRONG_NUM_OF_ARGS);
        }
    }

    /* 
     *   set up the new frame so that the frame pointer points to the old
     *   frame pointer stored in the stack 
     */
    frame_ptr_ = fp;

    /* load EP with the new code offset */
    entry_ptr_ = target_ofs;
    entry_ptr_native_ = target_ofs_ptr;

    /* push nil for each local */
    for (i = lcl_cnt ; i != 0 ; --i)
        (fp++)->set_nil();

    /* create and activate the new function's profiler frame */
    VM_IF_PROFILER(if (profiling_)
        prof_enter(target_ofs, defining_obj, target_prop));

    /* if desired, make a recursive call into the byte code interpreter */
    if (caller_ofs != 0)
    {
        /* 
         *   return the new program counter at the first byte of code in the
         *   new function, which immediately follows the header 
         */
        return target_ofs_ptr + get_funchdr_size();
    }
    else
    {
        VM_IF_DEBUGGER(err_try {)

        /* recursively call the interpreter loop */
        run(vmg_ target_ofs_ptr + get_funchdr_size());

        /* 
         *   if the debugger is present, always remove our recursive frame on
         *   the way out 
         */
        VM_IF_DEBUGGER(
        }
        err_finally
        {
            leave_recursive_frame(vmg0_);
        }
        err_end;)

        /* 
         *   this was a recursive call, so there's no program counter to
         *   return - just return null 
         */
        return 0;
    }
}

/*
 *   Determine if we're in a recursive VM invocation.  If this frame or
 *   any enclosing frame other than the outermost has a code offset of
 *   zero in the return address slot, we are in a recursive VM invocation.
 */
int CVmRun::is_recursive_invocation(VMG0_) const
{
    vm_val_t *p;
    
    /* start with the current frame */
    p = frame_ptr_;

    /* if there's no frame pointer, it's obviously not recursive */
    if (p == 0)
        return FALSE;

    /* scan frames until we get to the outermost frame */
    for (;;)
    {
        /* 
         *   If this is the outermost frame, we can stop now.  The
         *   outermost frame has an enclosing frame pointer value of null.
         *   (A given frame pointer always points directly to the
         *   enclosing frame pointer stored in the stack frame, so the
         *   offset from this frame pointer is zero.)  
         */
        if (get_enclosing_frame_ptr(vmg_ p) == 0)
            break;

        /* 
         *   Check the return address in this frame - if it's at offset
         *   zero, it means that this method was called directly as a
         *   recursive VM invocation.
         */
        if (get_return_addr_from_frame(vmg_ p) == 0)
            return TRUE;

        /* move to the enclosing frame */
        p = get_enclosing_frame_ptr(vmg_ p);
    }

    /* 
     *   we didn't find any direct invocations after the outermost frame,
     *   so this is the top-level VM invocation 
     */
    return FALSE;
}


/*
 *   Return from the current function.  Returns true if execution can
 *   proceed, false if this returns us out of the outermost function, in
 *   which case the execution loop must terminate and return control to
 *   the host environment.  
 */
const uchar *CVmRun::do_return(VMG0_)
{
    int argc;
    pool_ofs_t caller_ofs;

    /*
     *   The frame pointer always points to the location on the stack
     *   where we pushed the enclosing frame pointer.  Reset the stack
     *   pointer to the current frame pointer, then pop the enclosing
     *   frame pointer.  
     */
    G_stk->set_sp(frame_ptr_);
    frame_ptr_ = (vm_val_t *)G_stk->get(0)->val.ptr;

    /* restore the enclosing argument count */
    argc = G_stk->get(1)->val.intval;

    /* restore the enclosing entry pointer */
    entry_ptr_ = G_stk->get(2)->val.ofs;

    /* translate the method entry pointer to a physical address */
    entry_ptr_native_ = (const uchar *)G_code_pool->get_ptr(entry_ptr_);

    /* restore the enclosing code offset */
    caller_ofs = G_stk->get(3)->val.ofs;

    /* 
     *   Discard the actual parameters, plus the 'self', defining object,
     *   original target object, and target property values.  While we're at
     *   it, also discard the enclosing frame pointer, enclosing argument
     *   count, enclosing entry pointer, and enclosing code offset, which
     *   we've already restored.  
     */
    G_stk->discard(argc + 8);

    /* leave the profiler stack level */
    VM_IF_PROFILER(if (profiling_)
        prof_leave());

    /* 
     *   If the enclosing code offset is invalid, we've returned from the
     *   outermost function invoked by the host environment.  0 is an
     *   invalid offset, since offset 0 in a method never contains valid
     *   code.  
     */
    if (caller_ofs == 0)
        return 0;

    /* 
     *   return the new program counter - calculate the PC offset by adding
     *   the offset within the method to the entry pointer 
     */
    return entry_ptr_native_ + caller_ofs;
}


/* ------------------------------------------------------------------------ */
/*
 *   Recursive frame routines.
 */

/*
 *   Enter a recursive call frame from a native routine 
 */
void CVmRun::enter_recursive_frame(VMG_ int argc,
                                   const uchar **pc_ptr)
{
    pool_ofs_t old_ofs;
    int i;

    /* 
     *   don't bother setting up a recursive frame for a recursive call
     *   from the debugger itself - the only purpose of these frames is to
     *   aid the debugger in tracing the stack, which it obviously won't
     *   need to do when it's the native caller 
     */
    VM_IF_DEBUGGER(if (G_debugger->is_in_debugger())
        return);

    /* 
     *   if there's no global PC register, we're being called from the
     *   outermost native caller, so there's no need for a native frame 
     */
    if (pc_ptr == 0)
        return;
    
    /* get the return address from the global PC register */
    old_ofs = pc_to_method_ofs(*pc_ptr);

    /* make sure we have space for the native frame */
    if (!G_stk->check_space(6))
        err_throw(VMERR_STACK_OVERFLOW);

    /* there's no target property for a recursive caller */
    push_nil(vmg0_);

    /* there's no original target object */
    push_nil(vmg0_);

    /* there's no defining object */
    push_nil(vmg0_);

    /* there's no 'self' for a recursive caller */
    push_nil(vmg0_);

    /* push the caller's code offset */
    push_codeofs(vmg_ old_ofs);

    /* push the old entrypoint code offset */
    push_codeofs(vmg_ entry_ptr_);

    /* 
     *   push the argument count to the routine being invoked from the
     *   native code - this isn't actually the argument count to the
     *   native routine, which we don't know, but we must push it anyway
     *   because the arguments are arranged as though they're to this fake
     *   native frame 
     */
    push_int(vmg_ (int32)argc);

    /* push the current frame pointer */
    push_stackptr(vmg_ frame_ptr_);

    /* set up the new frame pointer */
    frame_ptr_ = G_stk->get_sp();

    /* there's no entrypoint address for the native code */
    entry_ptr_ = 0;
    entry_ptr_native_ = 0;

    /* 
     *   call the debugger to do a step trace - the debugger obviously
     *   can't really stop here, but what it can do is note that we've
     *   stepped through this native stack level for the purposes of
     *   determining when it should stop next for step-in, step-over, and
     *   step-out modes 
     */
    VM_IF_DEBUGGER(if (G_debugger->is_single_step())
        G_debugger->step(vmg_ 0, 0, FALSE, 0));

    /* 
     *   Copy the arguments from this frame - this is necessary so that
     *   the recursive frame we'll set up next (after we return) receives
     *   a copy of its arguments, which we hijacked by establishing this
     *   intermediate frame to represent the native caller.  Note that we
     *   must follow the normal convention of pushing arguments in reverse
     *   order.  
     */
    for (i = argc ; i > 0 ; --i)
        G_stk->push(get_param(vmg_ i - 1));
}

#ifdef VM_DEBUGGER

/*
 *   Leave a recursive call frame on our way back out to a native routine 
 */
void CVmRun::leave_recursive_frame(VMG0_)
{
    vm_val_t val;
    int argc;
    
    /* 
     *   if we're in the debugger, we will not have set up a recursive
     *   call frame, so we will not need to remove one 
     */
    if (G_debugger->is_in_debugger())
        return;

    /* 
     *   if there's no global PC pointer, it means that we're at the
     *   outermost native frame, which we suppress 
     */
    if (pc_ptr_ == 0)
        return;

    /* re-activate the enclosing frame */
    G_stk->set_sp(frame_ptr_);
    G_stk->pop(&val);
    frame_ptr_ = (vm_val_t *)val.val.ptr;

    /* pop the argument count */
    G_stk->pop(&val);
    argc = val.val.intval;

    /* pop the enclosing entry pointer */
    G_stk->pop(&val);
    entry_ptr_ = val.val.ofs;
    entry_ptr_native_ = (const uchar *)G_code_pool->get_ptr(entry_ptr_);

    /* 
     *   discard the enclosing code offset - since we know this is
     *   actually a native caller, we pushed the enclosing code offset
     *   only to enable the debugger to find the native caller 
     */
    G_stk->discard();

    /* 
     *   discard the actual parameters, plus the target property, original
     *   target object, defining object, and the 'self' object 
     */
    G_stk->discard(argc + 4);
}

/*
 *   save the execution context 
 */
void CVmRun::save_context(VMG_ vmrun_save_ctx *ctx)
{
    /* save our registers */
    ctx->entry_ptr_ = entry_ptr_;
    ctx->frame_ptr_ = frame_ptr_;
    ctx->pc_ptr_ = pc_ptr_;

    /* save the stack depth */
    ctx->old_stack_depth_ = G_stk->get_depth();
}

/*
 *   restore the execution context 
 */
void CVmRun::restore_context(VMG_ vmrun_save_ctx *ctx)
{
    /* restore our registers */
    entry_ptr_ = ctx->entry_ptr_;
    entry_ptr_native_ = (const uchar *)G_code_pool->get_ptr(entry_ptr_);
    frame_ptr_ = ctx->frame_ptr_;
    pc_ptr_ = ctx->pc_ptr_;

    /* if there's anything extra left on the stack, discard it */
    if (G_stk->get_depth() > ctx->old_stack_depth_)
        G_stk->discard(G_stk->get_depth() - ctx->old_stack_depth_);
}

#endif /* VM_DEBUGGER */

/* ------------------------------------------------------------------------ */
/*
 *   Append a stack trace to a string.  This is only meaningful in a
 *   debugger-equipped version. 
 */
#if VM_DEBUGGER

/*
 *   callback context for stack trace appender 
 */
struct append_stack_ctx
{
    /* the string so far */
    vm_obj_id_t str_obj;

    /* globals */
    vm_globals *vmg;

    /* frame pointer where we pushed our string for gc protection */
    vm_val_t *gc_fp;
};

/*
 *   stack trace callback 
 */
static void append_stack_cb(void *ctx0, const char *str, int strl)
{
    append_stack_ctx *ctx = (append_stack_ctx *)ctx0;
    size_t new_len;
    size_t old_len;
    const char *old_str;
    char *new_str;

    /* set up access to globals */
    VMGLOB_PTR(ctx->vmg);

    /* get the original string text */
    old_str = vm_objp(vmg_ ctx->str_obj)->get_as_string(vmg0_);
    old_len = vmb_get_len(old_str);
    old_str += VMB_LEN;

    /* 
     *   allocate a new string, big enough for the old string plus the new
     *   text, plus a newline 
     */
    new_len = old_len + strl + 1;
    ctx->str_obj = CVmObjString::create(vmg_ FALSE, new_len);

    /* get the new string buffer */
    new_str = ((CVmObjString *)vm_objp(vmg_ ctx->str_obj))->cons_get_buf();

    /* build the new string */
    memcpy(new_str, old_str, old_len);
    new_str[old_len] = '\n';
    memcpy(new_str + old_len + 1, str, strl);

    /* 
     *   replace our gc-protective stack reference to the old string with
     *   the new string - we're done with the old string now, so it's okay
     *   if it gets collected, but we obviously want to keep the new one
     *   around 
     */
    G_stk->get_from_frame(ctx->gc_fp, 0)->set_obj(ctx->str_obj);
}

/*
 *   append a stack trace to the given string 
 */
vm_obj_id_t CVmRun::append_stack_trace(VMG_ vm_obj_id_t str_obj)
{
    append_stack_ctx ctx;

    /* push the string for protection from gc */
    push_obj(vmg_ str_obj);
    
    /* call the debugger to set up the stack traceback */
    ctx.str_obj = str_obj;
    ctx.vmg = VMGLOB_ADDR;
    ctx.gc_fp = G_stk->get_sp();
    G_debugger->build_stack_listing(vmg_ &append_stack_cb, &ctx, TRUE);

    /* discard the gc protection */
    G_stk->discard();

    /* return the result string */
    return ctx.str_obj;
}

#endif /* VM_DEBUGGER */

/* ------------------------------------------------------------------------ */
/*
 *   Set a property of an object 
 */
void CVmRun::set_prop(VMG_ vm_obj_id_t obj, vm_prop_id_t prop,
                      const vm_val_t *new_val)
{
    /* set the property */
    vm_objp(vmg_ obj)->set_prop(vmg_ G_undo, obj, prop, new_val);
}

/* ------------------------------------------------------------------------ */
/*
 *   Evaluate a property of an object 
 */
const uchar *CVmRun::get_prop(VMG_ uint caller_ofs,
                              const vm_val_t *target_obj,
                              vm_prop_id_t target_prop,
                              const vm_val_t *self, uint argc)
{
    vm_val_t val;
    vm_obj_id_t srcobj;
    int found;
    vm_val_t new_self;

    /* find the property without evaluating it */
    found = get_prop_no_eval(vmg_ &target_obj, target_prop,
                             &argc, &srcobj, &val, &self, &new_self);

    /* if we didn't find it, try propNotDefined */
    if (!found && G_predef->prop_not_defined_prop != VM_INVALID_PROP)
    {
        /* 
         *   We didn't find it, so call propNotDefined on the object, with
         *   the property originally called as an additional first argument.
         *   If propNotDefined is not exported by the program, we'll fall
         *   back on the default of evaluating to nil.  
         */
        found = get_prop_no_eval(vmg_ &target_obj,
                                 G_predef->prop_not_defined_prop,
                                 &argc, &srcobj, &val, &self, &new_self);

        /* 
         *   if we found it, and it's code, push the original property ID as
         *   the new first argument 
         */
        if (found && val.typ == VM_CODEOFS)
        {
            /* 
             *   add the property argument (we push backwards, so this will
             *   conveniently become the new first argument, since we're
             *   pushing it last) 
             */
            push_prop(vmg_ target_prop);
            
            /* count the additional argument */
            ++argc;

            /* the target property changes to propNotDefined */
            target_prop = G_predef->prop_not_defined_prop;
        }
    }
    
    /* evaluate whatever we found or didn't find */
    return eval_prop_val(vmg_ found, caller_ofs, &val, self->val.obj,
                         target_prop, target_obj, srcobj, argc);
}

/*
 *   Look up a property without evaluating it. 
 */
inline int CVmRun::get_prop_no_eval(VMG_ const vm_val_t **target_obj,
                                    vm_prop_id_t target_prop,
                                    uint *argc, vm_obj_id_t *srcobj,
                                    vm_val_t *val,
                                    const vm_val_t **self,
                                    vm_val_t *new_self)
{
    int found;
    const char *target_ptr;
    
    /* 
     *   we can evaluate properties of regular objects, as well as string
     *   and list constants - see what we have 
     */
    switch((*target_obj)->typ)
    {
    case VM_LIST:
        /* 'self' must be the same as the target for a constant list */
        if ((*self)->typ != (*target_obj)->typ
            || (*self)->val.ofs != (*target_obj)->val.ofs)
            err_throw(VMERR_OBJ_VAL_REQD);

        /* translate the list offset to a physical pointer */
        target_ptr = G_const_pool->get_ptr((*target_obj)->val.ofs);

        /* evaluate the constant list property */
        found = CVmObjList::const_get_prop(vmg_ val, *target_obj,
                                           target_ptr, target_prop,
                                           srcobj, argc);

        /* 
         *   If the result is a method to run, we need an actual object for
         *   'self'.  In this case, create a dynamic list object with the
         *   same contents as the constant list value.  
         */
        if (found && val->typ == VM_CODEOFS)
        {
            /* create the list */
            new_self->set_obj(CVmObjListConst::create(vmg_ target_ptr));

            /* use it as the new 'self' and the new effective target */
            *self = new_self;
            *target_obj = new_self;
        }

        /* go evaluate the result as normal */
        break;

    case VM_SSTRING:
        /* 'self' must be the same as the target for a constant string */
        if ((*self)->typ != (*target_obj)->typ
            || (*self)->val.ofs != (*target_obj)->val.ofs)
            err_throw(VMERR_OBJ_VAL_REQD);

        /* translate the string offset to a physical pointer */
        target_ptr = G_const_pool->get_ptr((*target_obj)->val.ofs);

        /* evaluate the constant string property */
        found = CVmObjString::const_get_prop(vmg_ val, *target_obj,
                                             target_ptr, target_prop,
                                             srcobj, argc);

        /* 
         *   If the result is a method to run, we need an actual object for
         *   'self'.  In this case, create a dynamic string object with the
         *   same contents as the constant string value.  
         */
        if (found && val->typ == VM_CODEOFS)
        {
            /* create the string */
            new_self->set_obj(CVmObjStringConst::create(vmg_ target_ptr));

            /* it's the new 'self' and the new effective target object */
            *self = new_self;
            *target_obj = new_self;
        }

        /* go evaluate the result as normal */
        break;

    case VM_OBJ:
        /* get the property value from the target object */
        found = vm_objp(vmg_ (*target_obj)->val.obj)
                ->get_prop(vmg_ target_prop, val, (*target_obj)->val.obj,
                           srcobj, argc);

        /* 'self' must be an object as well */
        if ((*self)->typ != VM_OBJ)
            err_throw(VMERR_OBJ_VAL_REQD);
        break;

    case VM_NIL:
        /* nil pointer dereferenced */
        err_throw(VMERR_NIL_DEREF);

    default:
        /* we can't evaluate properties of anything else */
        err_throw(VMERR_OBJ_VAL_REQD);
    }

    /* return the 'found' indication */
    return found;
}

/* ------------------------------------------------------------------------ */
/*
 *   Given a value that has been retrieved from an object property,
 *   evaluate the value.  If the value contains code, we'll execute the
 *   code; if it contains a self-printing string, we'll display the
 *   string; otherwise, we'll just store the value in R0.
 *   
 *   'found' indicates whether or not the property value is defined.
 *   False indicates that the property value is not defined by the object;
 *   true indicates that it is.  
 */
inline const uchar *CVmRun::eval_prop_val(VMG_ int found, uint caller_ofs,
                                          const vm_val_t *val,
                                          vm_obj_id_t self,
                                          vm_prop_id_t target_prop,
                                          const vm_val_t *orig_target_obj,
                                          vm_obj_id_t defining_obj,
                                          uint argc)
{
    /* check whether or not the property is defined */
    if (found)
    {
        /* take appropriate action based on the datatype of the result */
        switch(val->typ)
        {
        case VM_CODEOFS:
            /* 
             *   It's a method - invoke the method.  This will set us up
             *   to start executing this new code, so there's nothing more
             *   we need to do here.  
             */
            return do_call(vmg_ caller_ofs, val->val.ofs, argc,
                           self, target_prop, orig_target_obj->val.obj,
                           defining_obj, 0);
            
        case VM_DSTRING:
            /* no arguments are allowed */
            if (argc != 0)
                err_throw(VMERR_WRONG_NUM_OF_ARGS);
            
            /* 
             *   it's a self-printing string - invoke the default string
             *   output function (this is effectively a do_call()) 
             */
            return disp_dstring(vmg_ val->val.ofs, caller_ofs, self);
            
        default:
            /* for any other value, no arguments are allowed */
            if (argc != 0)
                err_throw(VMERR_WRONG_NUM_OF_ARGS);
            
            /* store the result in R0 */
            r0_ = *val;

            /* resume execution where we left off */
            return entry_ptr_native_ + caller_ofs;
        }
    }
    else
    {
        /* 
         *   the property or method is not defined - discard arguments and
         *   set R0 to nil
         */
        G_stk->discard(argc);
        r0_.set_nil();

        /* resume execution where we left off */
        return entry_ptr_native_ + caller_ofs;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Inherit a property or method from the appropriate superclass of the
 *   object that defines currently executing code.  
 */
const uchar *CVmRun::inh_prop(VMG_ uint caller_ofs,
                              vm_prop_id_t prop, uint argc)
{
    vm_val_t orig_target_obj;
    vm_obj_id_t defining_obj;
    vm_val_t val;
    vm_obj_id_t srcobj;
    int found;
    vm_obj_id_t self;

    /* get the defining object from the stack frame */
    defining_obj = get_defining_obj(vmg0_);

    /* get the original target object from the stack frame */
    orig_target_obj.set_obj(get_orig_target_obj(vmg0_));

    /* get the 'self' object */
    self = get_self(vmg0_);

    /* get the inherited property value */
    found = vm_objp(vmg_ self)->inh_prop(vmg_ prop, &val, self,
                                         orig_target_obj.val.obj,
                                         defining_obj, &srcobj, &argc);

    /* if we didn't find it, try inheriting propNotDefined */
    if (!found && G_predef->prop_not_defined_prop != VM_INVALID_PROP)
    {
        /* 
         *   Look up propNotDefined using the same search conditions we used
         *   to find the original inherited property.  This lets us look up
         *   the "inherited" propNotDefined.  
         */
        found = vm_objp(vmg_ self)->inh_prop(vmg_
                                             G_predef->prop_not_defined_prop,
                                             &val, self,
                                             orig_target_obj.val.obj,
                                             defining_obj, &srcobj, &argc);

        /* 
         *   if we found it, and it's code, push the original property ID we
         *   were attempting to inherit - this becomes the new first
         *   parameter to the propNotDefined method 
         */
        if (found && val.typ == VM_CODEOFS)
        {
            /* add the original property pointer argument */
            push_prop(vmg_ prop);

            /* count the additional argument */
            ++argc;

            /* the target property changes to propNotDefined */
            prop = G_predef->prop_not_defined_prop;
        }
    }

    /* 
     *   evaluate and store the result - note that "self" remains the
     *   current "self" object, since we're inheriting within the context
     *   of the original method call 
     */
    return eval_prop_val(vmg_ found, caller_ofs, &val, self, prop,
                         &orig_target_obj, srcobj, argc);
}


/* ------------------------------------------------------------------------ */
/*
 *   Display a dstring via the default string display mechanism 
 */
const uchar *CVmRun::disp_dstring(VMG_ pool_ofs_t ofs, uint caller_ofs,
                                  vm_obj_id_t self)
{
    /* push the string */
    G_stk->push()->set_sstring(ofs);

    /* invoke the default "say" function */
    return disp_string_val(vmg_ caller_ofs, self);
}

/*
 *   Display the value at top of stack via the default string display
 *   mechanism 
 */
const uchar *CVmRun::disp_string_val(VMG_ uint caller_ofs, vm_obj_id_t self)
{
    /* 
     *   if there's a valid 'self' object, and there's a default display
     *   method defined, and 'self' defines or inherits that method,
     *   invoke the method 
     */
    if (say_method_ != VM_INVALID_PROP && self != VM_INVALID_OBJ)
    {
        vm_obj_id_t src_obj;
        vm_val_t val;

        /* 
         *   look up the property - if we find it, and it's a regular
         *   method, invoke it 
         */
        if (vm_objp(vmg_ self)->get_prop(vmg_ say_method_, &val, self,
                                         &src_obj, 0)
            && val.typ == VM_CODEOFS)
        {
            vm_val_t self_val;

            /* set up a 'self' value - this is the target object */
            self_val.set_obj(self);

            /* there's a default display method - invoke it */
            return eval_prop_val(vmg_ TRUE, caller_ofs, &val, self,
                                 say_method_, &self_val, src_obj, 1);
        }
    }
    
    /* if the "say" function isn't initialized, it's an error */
    if (say_func_ == 0 || say_func_->val.typ == VM_NIL)
        err_throw(VMERR_SAY_IS_NOT_DEFINED);

    /* call the "say" function with the argument at top of stack */
    return call_func_ptr(vmg_ &say_func_->val, 1, 0, caller_ofs);
}

/*
 *   Set the "say" function.
 */
void CVmRun::set_say_func(VMG_ const vm_val_t *val)
{
    /* 
     *   if we haven't yet allocated a global to hold the 'say' function,
     *   allocate one now 
     */
    if (say_func_ == 0)
        say_func_ = G_obj_table->create_global_var();

    /* remember the new function */
    say_func_->val = *val;
}

/*
 *   Get the current "say" function 
 */
void CVmRun::get_say_func(vm_val_t *val) const
{
    /* 
     *   if we ever allocated a global to hold the 'say' function, return its
     *   value; otherwise, there's no 'say' function, so the result is nil 
     */
    if (say_func_ != 0)
        *val = say_func_->val;
    else
        val->set_nil();
}

/* ------------------------------------------------------------------------ */
/*
 *   Check a property for speculative evaluation 
 */
void CVmRun::check_prop_spec_eval(VMG_ vm_obj_id_t obj, vm_prop_id_t prop)
{
    vm_val_t val;
    vm_obj_id_t srcobj;

    /* get the property value */
    if (vm_objp(vmg_ obj)->get_prop(vmg_ prop, &val, obj, &srcobj, 0))
    {
        /* check the type of the value */
        switch(val.typ)
        {
        case VM_CODEOFS:
        case VM_DSTRING:
        case VM_NATIVE_CODE:
            /* 
             *   evaulating these types could result in side effects, so
             *   this property cannot be evaulated during a speculative
             *   evaluation 
             */
            err_throw(VMERR_BAD_SPEC_EVAL);
            break;

        default:
            /* evaluating other types causes no side effects, so proceed */
            break;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Set up a function header pointer for the current function 
 */
void CVmRun::set_current_func_ptr(VMG_ CVmFuncPtr *func_ptr)
{
    /* set up the pointer based on the current Entry Pointer register */
    func_ptr->set(entry_ptr_native_);
}

/*
 *   Set up a function header pointer for the return address of the given
 *   stack frame 
 */
void CVmRun::set_return_funcptr_from_frame(VMG_ CVmFuncPtr *func_ptr,
                                           vm_val_t *frame_ptr)
{
    pool_ofs_t ep;
    
    /* get the enclosing entry pointer for the frame */
    ep = get_enclosing_entry_ptr_from_frame(vmg_ frame_ptr);

    /* set up the function pointer for the entry pointer */
    func_ptr->set((const uchar *)G_code_pool->get_ptr(ep));
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the frame pointer at a given stack level 
 */
vm_val_t *CVmRun::get_fp_at_level(VMG_ int level) const
{
    vm_val_t *fp;
    
    /* walk up the stack to the desired level */
    for (fp = frame_ptr_ ; fp != 0 && level != 0 ;
         --level, fp = get_enclosing_frame_ptr(vmg_ fp));

    /* 
     *   if we ran out of frames before we reached the desired level,
     *   throw an error 
     */
    if (fp == 0)
        err_throw(VMERR_BAD_FRAME);

    /* return the frame */
    return fp;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the message from an exception object 
 */
void CVmRun::get_exc_message(VMG_ const CVmException *exc,
                             char *buf, size_t buflen, int add_unh_prefix)
{
    CVmException tmpexc;
    const char *tmpmsg;
    const char *msg;

    /* set up our temporary exception object with no parameters by default */
    tmpexc.param_count_ = 0;
    
    /* check for unhandled program exceptions */
    if (exc->get_error_code() == VMERR_UNHANDLED_EXC)
    {
        size_t msg_len;
        
        /* 
         *   This is not a VM error, but is simply an exception that the
         *   program itself threw but did not handle.  We might be able to
         *   find an informational message in the exception object itself.
         */

        /* get the exception's message, if available */
        msg = get_exc_message(vmg_ exc, &msg_len);
        if (msg != 0)
        {
            /* 
             *   we got a message from the exception object - use it 
             */

            /* set up our parameters for the formatting */
            tmpexc.param_count_ = 1;
            tmpexc.set_param_str(0, msg, msg_len);

            /*
             *   If they want an "unhandled exception" prefix, get the
             *   message for the prefix; otherwise, just use the message
             *   from the exception without further adornment. 
             */
            if (add_unh_prefix)
            {
                /* they want a prefix - get the prefix message */
                tmpmsg = err_get_msg(vm_messages, vm_message_count,
                                     VMERR_UNHANDLED_EXC_PARAM, FALSE);
            }
            else
            {
                /* no prefix desired - just use the message as we got it */
                tmpmsg = "%s";
            }

            /* format the message */
            err_format_msg(buf, buflen, tmpmsg, &tmpexc);
        }
        else
        {
            /* no message - use a generic exception message */
            tmpmsg = err_get_msg(vm_messages, vm_message_count,
                                 VMERR_UNHANDLED_EXC, FALSE);
            err_format_msg(buf, buflen, tmpmsg, &tmpexc);
        }
    }
    else
    {
        /* 
         *   It's a VM exception, so we can determine the error's meaning
         *   from the error code.  Look up the message for the error code
         *   in our error message list.  
         */
        msg = err_get_msg(vm_messages, vm_message_count,
                          exc->get_error_code(), FALSE);
        
        /* if that failed, just show the error number */
        if (msg == 0)
        {
            /* no message - just show the error code */
            tmpmsg = err_get_msg(vm_messages, vm_message_count,
                                 VMERR_VM_EXC_CODE, FALSE);

            /* set up our parameters for formatting */
            tmpexc.param_count_ = 1;
            tmpexc.set_param_int(0, exc->get_error_code());

            /* format the message */
            err_format_msg(buf, buflen, tmpmsg, &tmpexc);
        }
        else
        {
            char tmpbuf[256];

            /* format the message from the exception parameters */
            err_format_msg(tmpbuf, sizeof(tmpbuf), msg, exc);
            
            /* get the prefix message */
            tmpmsg = err_get_msg(vm_messages, vm_message_count,
                                 VMERR_VM_EXC_PARAM, FALSE);

            /* set up our parameters for the formatting */
            tmpexc.param_count_ = 1;
            tmpexc.set_param_str(0, tmpbuf);

            /* format the message */
            err_format_msg(buf, buflen, tmpmsg, &tmpexc);
        }
    }
}

/*
 *   Get the message from an "unhandled exception" error object
 */
const char *CVmRun::get_exc_message(VMG_ const CVmException *exc,
                                    size_t *msg_len)
{
    vm_obj_id_t exc_obj;

    /* 
     *   if the error isn't "unhandled exception," there's not a stored
     *   exception object; likewise, if there's no object parameter in the
     *   exception, there's nothing to use to obtain the message 
     */
    if (exc->get_error_code() != VMERR_UNHANDLED_EXC
        || exc->get_param_count() < 1)
        return 0;
    
    /* get the exception object */
    exc_obj = (vm_obj_id_t)exc->get_param_ulong(0);

    /* get the message from the object */
    return get_exc_message(vmg_ exc_obj, msg_len);
}

/*
 *   Get the message from an exception object 
 */
const char *CVmRun::get_exc_message(VMG_ vm_obj_id_t exc_obj, size_t *msg_len)
{
    vm_val_t val;
    vm_obj_id_t src_obj;
    const char *str;
    uint argc;
    
    /* if there's no object, there's no message */
    if (exc_obj == VM_INVALID_OBJ)
        return 0;
    
    /* 
     *   get the exceptionMessage property value from the object; if
     *   there's not a valid exceptionMessage property defined, or the
     *   object doesn't have a value for the property, there's no message 
     */
    argc = 0;
    if (G_predef->rterrmsg_prop == VM_INVALID_PROP
        || (!vm_objp(vmg_ exc_obj)->get_prop(vmg_ G_predef->rterrmsg_prop,
                                             &val, exc_obj, &src_obj,
                                             &argc)))
        return 0;

    /* 
     *   We got the property.  If it's a string or an object containing a
     *   string, retrieve the string.
     */
    switch(val.typ)
    {
    case VM_SSTRING:
        /* get the constant string */
        str = G_const_pool->get_ptr(val.val.ofs);
        break;

    case VM_OBJ:
        /* get the string value of the object, if possible */
        str = vm_objp(vmg_ val.val.obj)->get_as_string(vmg0_);
        break;

    default:
        /* it's not a string - we can't use it */
        str = 0;
        break;
    }

    /* check to see if we got a string */
    if (str != 0)
    {
        /* 
         *   The string is in the standard VM internal format, which means
         *   it has a 2-byte length prefix followed by the bytes of the
         *   string (with no null termination).  Read the length prefix,
         *   then skip past it so the caller doesn't have to.  
         */
        *msg_len = osrp2(str);
        str += VMB_LEN;
    }

    /* return the string pointer */
    return str;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the boundaries of the current statement, based on debugging
 *   information.  Returns true if valid debugging information was found for
 *   the given code location, false if not.  
 */
int CVmRun::get_stm_bounds(VMG_ const CVmFuncPtr *func_ptr,
                           ulong method_ofs,
                           CVmDbgLinePtr *caller_line_ptr,
                           ulong *stm_start, ulong *stm_end)
{
    CVmDbgTablePtr dbg_ptr;
    int lo;
    int hi;
    int cur;

    /* presume we won't find anything */
    *stm_start = *stm_end = 0;

    /* 
     *   if the current method has no line records, we can't find the
     *   boundaries 
     */
    if (!func_ptr->set_dbg_ptr(&dbg_ptr)
        || dbg_ptr.get_line_count(vmg0_) == 0)
    {
        /* indicate that we didn't find debug information */
        return FALSE;
    }

    /*
     *   We must perform a binary search of the line records for the line
     *   that contains this program counter offset.  
     */
    lo = 0;
    hi = dbg_ptr.get_line_count(vmg0_) - 1;
    while (lo <= hi)
    {
        ulong start_ofs;
        ulong end_ofs;
        CVmDbgLinePtr line_ptr;

        /* split the difference and get the current entry */
        cur = lo + (hi - lo)/2;
        dbg_ptr.set_line_ptr(vmg_ &line_ptr, cur);

        /* get the current statement's start relative to the method header */
        start_ofs = line_ptr.get_start_ofs();

        /* 
         *   Get the next statement's start offset, which gives us the end
         *   of this statement.  If this is the last statement in the table,
         *   it runs to the end of the function; use the debug records table
         *   offset as the upper bound in this case.  
         */
        if (cur == (int)dbg_ptr.get_line_count(vmg0_) - 1)
        {
            /* 
             *   it's the last record - use the debug table offset as an
             *   upper bound, since we know the function can't have any
             *   executable code past this point 
             */
            end_ofs = func_ptr->get_debug_ofs();
        }
        else
        {
            CVmDbgLinePtr next_line_ptr;

            /* another record follows this one - use it */
            next_line_ptr.copy_from(&line_ptr);
            next_line_ptr.inc(vmg0_);
            end_ofs = next_line_ptr.get_start_ofs();
        }

        /* see where we are relative to this line record */
        if (method_ofs >= end_ofs)
        {
            /* we need to go higher */
            lo = (cur == lo ? cur + 1 : cur);
        }
        else if (method_ofs < start_ofs)
        {
            /* we need to go lower */
            hi = (cur == hi ? hi - 1 : cur);
        }
        else
        {
            /* found it - set the bounds to this record's limits */
            *stm_start = start_ofs;
            *stm_end = end_ofs;

            /* fill in the caller's line pointer if desired */
            if (caller_line_ptr != 0)
                caller_line_ptr->copy_from(&line_ptr);

            /* indicate that we found the line boundaries successfully */
            return TRUE;
        }
    }

    /* return failure */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Profiler functions
 */
#ifdef VM_PROFILER

/*
 *   Profiler master hash table entry 
 */
class CVmHashEntryProfiler: public CVmHashEntryCI
{
public:
    CVmHashEntryProfiler(const char *str, size_t len,
                         const vm_profiler_rec *rec)
        : CVmHashEntryCI(str, len, TRUE)
    {
        /* copy the profiler record's identifying portion */
        rec_.func = rec->func;
        rec_.obj = rec->obj;
        rec_.prop = rec->prop;

        /* initialize the timers and counters to zero */
        rec_.sum_direct.hi = rec_.sum_direct.lo = 0;
        rec_.sum_chi.hi = rec_.sum_chi.lo = 0;
        rec_.call_cnt = 0;
    }

    /* our profiler record */
    vm_profiler_rec rec_;
};

/*
 *   Begin profiling 
 */
void CVmRun::start_profiling()
{
    /* clear any old profiler data from the master hash table */
    prof_master_table_->delete_all_entries();

    /* reset the profiler stack */
    prof_stack_idx_ = 0;

    /* turn on profiling */
    profiling_ = TRUE;
}

/*
 *   End profiling 
 */
void CVmRun::end_profiling()
{
    /* turn off profiling */
    profiling_ = FALSE;

    /* leave all active profiler stack levels */
    while (prof_stack_idx_ != 0)
        prof_leave();
}

/* context for our profiling callback */
struct vmrun_prof_enum
{
    /* interpreter object */
    CVmRun *terp;

    /* debugger object */
    CVmDebug *dbg;

    /* client callback and its context */
    void (*cb)(void *, const char *, unsigned long, unsigned long,
               unsigned long);
    void *cb_ctx;
};

/*
 *   Get the profiling data 
 */
void CVmRun::get_profiling_data(VMG_
                                void (*cb)(void *,
                                           const char *,
                                           unsigned long,
                                           unsigned long,
                                           unsigned long),
                                void *cb_ctx)
{
    vmrun_prof_enum our_ctx;

    /* if there's no debugger, we can't get symbols, so we can't proceed */
    if (G_debugger == 0)
        return;

    /* set up our callback context */
    our_ctx.terp = this;
    our_ctx.dbg = G_debugger;
    our_ctx.cb = cb;
    our_ctx.cb_ctx = cb_ctx;

    /* enumerate the master table entries through our callback */
    prof_master_table_->enum_entries(&prof_enum_cb, &our_ctx);
}

/*
 *   Callback for enumerating the profiling data 
 */
void CVmRun::prof_enum_cb(void *ctx0, CVmHashEntry *entry0)
{
    vmrun_prof_enum *ctx = (vmrun_prof_enum *)ctx0;
    CVmHashEntryProfiler *entry = (CVmHashEntryProfiler *)entry0;
    char namebuf[128];
    const char *p;

    /* generate the name of the function or method */
    if (entry->rec_.obj != VM_INVALID_OBJ)
    {
        char *dst;
        
        /* look up the object name */
        p = ctx->dbg->objid_to_sym(entry->rec_.obj);

        /* get the original name, if this is a synthetic 'modify' object */
        p = ctx->dbg->get_modifying_sym(p);

        /* 
         *   if we got an object name, use it; otherwise, synthesize a name
         *   using the object number 
         */
        if (p != 0)
            strcpy(namebuf, p);
        else
            sprintf(namebuf, "obj#%lx", (long)entry->rec_.obj);

        /* add a period */
        dst = namebuf + strlen(namebuf);
        *dst++ = '.';

        /* look up the property name */
        p = ctx->dbg->propid_to_sym(entry->rec_.prop);
        if (p != 0)
            strcpy(dst, p);
        else
            sprintf(dst, "prop#%x", (int)entry->rec_.prop);
    }
    else if (entry->rec_.func != 0)
    {
        /* look up the function at the code offset */
        p = ctx->dbg->funcaddr_to_sym(entry->rec_.func);
        if (p != 0)
            strcpy(namebuf, p);
        else
            sprintf(namebuf, "func#%lx", (long)entry->rec_.func);
    }
    else
    {
        /* it must be system code */
        strcpy(namebuf, "<System>");
    }

    /* invoke the callback with the data */
    (*ctx->cb)(ctx->cb_ctx, namebuf,
               os_prof_time_to_ms(&entry->rec_.sum_direct),
               os_prof_time_to_ms(&entry->rec_.sum_chi),
               entry->rec_.call_cnt);
}


/*
 *   Profile entry into a new function or method
 */
void CVmRun::prof_enter(pool_ofs_t call_ofs,
                        vm_obj_id_t obj, vm_prop_id_t prop)
{
    vm_prof_time cur;

    /* get the current time */
    os_prof_curtime(&cur);

    /* if we have a valid previous entry, suspend it */
    if (prof_stack_idx_ > 0 && prof_stack_idx_ - 1 < prof_stack_max_)
    {
        vm_profiler_rec *p;
        vm_prof_time delta;

        /* get a pointer to the outgoing entry */
        p = &prof_stack_[prof_stack_idx_ - 1];

        /* 
         *   add the time since the last start to the cumulative time spent
         *   in this function 
         */
        prof_calc_elapsed(&delta, &cur, &prof_start_);
        prof_add_elapsed(&p->sum_direct, &delta);
    }

    /* if we have room on the profiler stack, add a new level */
    if (prof_stack_idx_ < prof_stack_max_)
    {
        vm_profiler_rec *p;

        /* get a pointer to the new entry */
        p = &prof_stack_[prof_stack_idx_];

        /* remember the identifying data for the method or function */
        p->func = call_ofs;
        p->obj = obj;
        p->prop = prop;

        /* we have no cumulative time yet */
        p->sum_direct.hi = p->sum_direct.lo = 0;
        p->sum_chi.hi = p->sum_chi.lo = 0;
    }

    /* count the level */
    ++prof_stack_idx_;

    /* remember the start time in the new current function */
    os_prof_curtime(&prof_start_);
}

/*
 *   Profile returning from a function or method
 */
void CVmRun::prof_leave()
{
    vm_prof_time delta;
    vm_prof_time cur;
    vm_prof_time chi;

    /* get the current time */
    os_prof_curtime(&cur);

    /* move to the last level */
    --prof_stack_idx_;

    /* presume we won't know the child time */
    chi.hi = chi.lo = 0;

    /* if we're on a valid level, finish the call */
    if (prof_stack_idx_ < prof_stack_max_)
    {
        vm_profiler_rec *p;
        CVmHashEntryProfiler *entry;

        /* get a pointer to the outgoing entry */
        p = &prof_stack_[prof_stack_idx_];

        /* 
         *   add the time since the last start to the cumulative time spent
         *   in this function 
         */
        prof_calc_elapsed(&delta, &cur, &prof_start_);
        prof_add_elapsed(&p->sum_direct, &delta);

        /*
         *   Find or create the master record for the terminating function or
         *   method, and add the cumulative times from this call to the
         *   master record's cumulative times.  Also count the invocation in
         *   the master record.  
         */
        entry = prof_find_master_rec(p);
        prof_add_elapsed(&entry->rec_.sum_direct, &p->sum_direct);
        prof_add_elapsed(&entry->rec_.sum_chi, &p->sum_chi);
        ++(entry->rec_.call_cnt);

        /*
         *   Calculate the cumulative time in the outgoing function - this is
         *   the total time directly in the function plus the cumulative time
         *   in all of its children.  We must add this to the caller's
         *   cumulative child time, since this function and all of its
         *   children are children of the caller and thus must count in the
         *   caller's total child time.  
         */
        chi = p->sum_direct;
        prof_add_elapsed(&chi, &p->sum_chi);
    }

    /* if we're leaving to a valid level, re-activate it */
    if (prof_stack_idx_ > 0 && prof_stack_idx_ < prof_stack_max_)
    {
        vm_profiler_rec *p;

        /* get a pointer to the resuming entry */
        p = &prof_stack_[prof_stack_idx_ - 1];

        /* 
         *   add the time spent in the child and its children to our
         *   cumulative child time 
         */
        prof_add_elapsed(&p->sum_chi, &chi);
    }

    /* 
     *   remember the new start time for the function we're resuming - we
     *   must reset this to the current time, since we measure deltas from
     *   the last call or return on each call or return 
     */
    os_prof_curtime(&prof_start_);
}

/*
 *   Calculate an elapsed 64-bit time value 
 */
void CVmRun::prof_calc_elapsed(vm_prof_time *diff, const vm_prof_time *a,
                               const vm_prof_time *b)
{
    /* calculate the differences of the low and high parts */
    diff->lo = a->lo - b->lo;
    diff->hi = a->hi - b->hi;

    /* 
     *   if the low part ended up higher than it started, then we
     *   underflowed, and hence must borrow from the high part 
     */
    if (diff->lo > a->lo)
        --(diff->hi);
}

/*
 *   Add one elapsed time value to another
 */
void CVmRun::prof_add_elapsed(vm_prof_time *sum, const vm_prof_time *val)
{
    unsigned long orig_lo;

    /* remember the original low part */
    orig_lo = sum->lo;
    
    /* add the low parts and high parts */
    sum->lo += val->lo;
    sum->hi += val->hi;

    /* 
     *   if the low part of the sum is less than where it started, then it
     *   overflowed, and we must hence carry to the high part 
     */
    if (sum->lo < orig_lo)
        ++(sum->hi);
}

/*
 *   Find or create a hash table entry for a profiler record 
 */
CVmHashEntryProfiler *CVmRun::prof_find_master_rec(const vm_profiler_rec *p)
{
    const size_t id_siz = sizeof(p->func) + sizeof(p->obj) + sizeof(p->prop);
    char id[id_siz];
    CVmHashEntryProfiler *entry;
    
    /* 
     *   Build the ID string, which we'll use as our hash key.  We never have
     *   to serialize this, so it doesn't matter that it's dependent on byte
     *   order and word size. 
     */
    memcpy(id, &p->func, sizeof(p->func));
    memcpy(id + sizeof(p->func), &p->obj, sizeof(p->obj));
    memcpy(id + sizeof(p->func) + sizeof(p->obj), &p->prop, sizeof(p->prop));

    /* try to find an existing entry */
    entry = (CVmHashEntryProfiler *)prof_master_table_->find(id, id_siz);

    /* if we didn't find an entry, create one */
    if (entry == 0)
    {
        /* create a new entry */
        entry = new CVmHashEntryProfiler(id, id_siz, p);

        /* add it to the table */
        prof_master_table_->add(entry);
    }

    /* return the entry */
    return entry;
}

#endif /* VM_PROFILER */

/* ------------------------------------------------------------------------ */
/*
 *   Footnote - for the referring code, search the code above for
 *   [REGISTER_P_FOOTNOTE].
 *   
 *   This footnote pertains to a 'register' declaration that causes gcc (and
 *   probably some other compilers) to generate a warning message.  The
 *   'register' declaration is useful on some compilers and will be retained.
 *   Here's a note I sent to Nikos Chantziaras (who asked about the warning)
 *   explaining why I'm choosing to leave the 'register' declaration in, and
 *   why I think this 'register' declaration is actually correct and useful
 *   despite the warning it generates on some compilers.
 *   
 *   The basic issue is that the code takes the address of the variable in
 *   question in expressions passed as parameters to certain function calls.
 *   These function calls all happen to be in-linable functions, and it
 *   happens that in each function, the address operator is always canceled
 *   out by a '*' dereference operator - in other words, we have '*&p', which
 *   the compiler can turn into just plain 'p' when the calls are in-lined,
 *   eliminating the need to actually take the address of 'p'.
 *   
 *   Nikos:
 *.  >I'm no expert, but I think GCC barks at this because it isn't possible
 *.  >at all to store the variable in a register if the code wants its
 *.  >address, therefore the 'register' in the declaration does nothing.
 *   
 *   That's correct, but a compiler is always free to ignore 'register'
 *   declarations *anyway*, even if enregistration is possible.  Therefore a
 *   warning that it's not possible to obey 'register' is unnecessary,
 *   because it's explicit in the language definition that 'register' is not
 *   binding.  It simply is not possible for an ignored 'register' attribute
 *   to cause unexpected behavior.  Warnings really should only be generated
 *   for situations where it is likely that the programmer expects different
 *   behavior than the compiler will deliver; in the case of an ignored
 *   'register' attribute, the programmer is *required* to expect that the
 *   attribute might be ignored, so a warning to this effect is superfluous.
 *   
 *   Now, I understand why they generate the warning - it's because the
 *   compiler believes that the program code itself makes enregistration
 *   impossible, not because the compiler has chosen for optimization
 *   purposes to ignore the 'register' request.  However, as we'll see
 *   shortly, the program code doesn't truly make enregistration impossible;
 *   it is merely impossible in some interpretations of the code.  Therefore
 *   we really are back to the compiler choosing to ignore the 'register'
 *   request due to its own optimization decisions; the 'register' request is
 *   made impossible far downstream of the actual decisions that the compiler
 *   makes (which have to do with in-line vs out-of-line calls), but it
 *   really is compiler decisions that make it impossible, not the inherent
 *   structure of the code.
 *   
 *.  >Furthermore, I'm not sure I understand the relationship
 *.  >between 'register' and inlining; why should "*(&p)" do something
 *.  >else "in calls to inlines" than its obvious meaning?
 *   
 *   When a function is in-lined, the compiler is not required to generate
 *   the same code it would generate for the most general case of the same
 *   function call, as long as the meaning is the same.
 *   
 *   For example, suppose we have some code that contains a call to a
 *   function like so:
 *   
 *   a = myFunc(a + 7, 3);
 *   
 *   In the general out-of-line case, the compiler must generate some
 *   machine-code instructions like this:
 *   
 *.  push #3
 *.  mov [a], d0
 *.  add #7, d0
 *.  push d0
 *.  call #myFunc
 *.  mov d0, [a]
 *   
 *   The compiler doesn't have access to the inner workings of myFunc, so it
 *   must generate the appropriate code for the generic interface to an
 *   external function.
 *   
 *   Now, suppose the function is defined like so:
 *   
 *   int myFunc(int a, int b) { return a - 6; }
 *   
 *   and further suppose that the compiler decides to in-line this function.
 *   In-lining means the compiler will generate the code that implements the
 *   function directly in the caller; there will be no call to an external
 *   linkage point.  This means the compiler can implement the linkage to the
 *   function with a custom one-off interface for this particular invocation
 *   - every in-line invocation can be customized to the exact context where
 *   it appears.  So, for example, if we call myFunc right now and registers
 *   d1 and d2 happens to be available, we can put the parameters in d1 and
 *   d2, and the generated function will refer to those registers for the
 *   parameters rather than having to look in the stack.  Later on, if we
 *   generate a separate call to the same function, but registers d3 and d7
 *   are the ones available, we can use those instead.  Each generated copy
 *   of the function can fit its exact context.
 *   
 *   Furthermore, looking at this function and at the arguments passed, we
 *   can see that the formal parameter 'b' has no effect on the function's
 *   results, and the actual parameter '3' passed for 'b' has no side
 *   effects.  Therefore, the compiler is free to completely ignore this
 *   parameter - there's no need to generate any code for it at all, since we
 *   have sufficient knowledge to see that it has no effect on the meaning of
 *   the code.
 *   
 *   Further still, we can globally optimize the entire function.  So, we can
 *   see that myFunc(a+7, 3) is going to turn into the expression (a+7-6).
 *   We can fold constants to arrive at (a+1) as the result of the function.
 *   We can therefore generate the entire code for the function's invocation
 *   like so:
 *   
 *   inc [a]
 *   
 *   Okay, now let's look at the &p case.  In the specific examples in
 *   vmrun.cpp, we have a bunch of function invocations like this:
 *   
 *   register const char *p;
 *.  int x = myfunc(&p);
 *   
 *   In the most general case, we have to generate code like this:
 *   
 *.  lea [p], d0        ; load effective address
 *.  push d0
 *.  call #myfunc
 *.  mov d0, [x]
 *   
 *   So, in the most general case of a call with external linkage, we need
 *   'p' to have a main memory address so that we can push it on the stack as
 *   the parameter to this call.  Registers don't have main memory addresses,
 *   so 'p' can't go in a register.
 *   
 *   However, we know what myfunc() looks like:
 *   
 *.  char myfunc(const char **p)
 *.  {
 *.      char c = **p;
 *.      *p += 1;
 *.      return c;
 *.  }
 *   
 *   If the compiler chooses to in-line this function, it can globally
 *   optimize its linkage and implementation as we saw earlier.  So, the
 *   compiler can rewrite the code like so:
 *   
 *   register const char *p;
 *.  int x = **(&p);
 *.  *(&p) += 1;
 *   
 *   which can be further rewritten to:
 *   
 *.  register const char *p;
 *.  int x = *p;
 *.  p += 1;
 *   
 *   Now we can generate the machine code for the final optimized form:
 *   
 *.  mov [p], a0         ; get the *value* of p into index register 0
 *.  mov.byte [a0+0], d0 ; get the value index register 0 points to
 *.  mov.byte d0, [x]    ; store it in x
 *.  inc [p]             ; inc the value of p
 *   
 *   Nowhere do we need a main memory address for p.  This means the compiler
 *   can keep p in a register, say d5:
 *   
 *.  mov d5, a0
 *.  mov.byte [a0+0], d0
 *.  mov.byte d0, [x]
 *.  inc d5
 *   
 *   And this is indeed exactly what the code that comes out of vc++ looks
 *   like (changed from my abstract machine to 32-bit x86, of course).  
 *   
 *   So: if the compiler chooses to in-line the functions that are called
 *   with '&p' as a parameter, and the compiler performs the available
 *   optimizations on those calls once they're in-lined, then a memory
 *   address for 'p' is never needed.  Thus there is a valid interpretation
 *   of the code where 'register p' can be obeyed.  If the compiler doesn't
 *   choose to in-line the functions or make those optimizations, then the
 *   compiler will be unable to satisfy the 'register p' request and will be
 *   forced to put 'p' in addressable main memory.  But it really is entirely
 *   up to the compiler whether to obey the 'register p' request; the
 *   program's structure does not make the request impossible to satisfy.
 *   Therefore there is no reason for the compiler to warn about this, any
 *   more than there would be if the compiler chose not to obey the 'register
 *   p' simply because it thought it could make more optimal use of the
 *   available registers.  That gcc warns is understandable, in that a
 *   superficial reading of the code would not reveal the optimization
 *   opportunity; but the warning is nonetheless unnecessary, and the
 *   'register' does provide useful optimization hinting to at least vc++, so
 *   I think it's best to leave it in and ignore the warning.  
 */
