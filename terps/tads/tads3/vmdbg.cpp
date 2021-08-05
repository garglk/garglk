#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmdbg.cpp - T3 VM debugger API
Function
  
Notes
  
Modified
  11/23/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>

#include "t3std.h"
#include "vmuni.h"
#include "os.h"
#include "charmap.h"
#include "vmglob.h"
#include "vmtype.h"
#include "vmdbg.h"
#include "vmrun.h"
#include "vmsrcf.h"
#include "vmhash.h"
#include "vmpool.h"
#include "vmop.h"
#include "vmlst.h"
#include "tctarg.h"
#include "tcglob.h"
#include "tctok.h"
#include "tcprs.h"
#include "tcmain.h"
#include "resload.h"
#include "tchostsi.h"
#include "tchost.h"
#include "tcgen.h"
#include "vmimage.h"
#include "vmhost.h"
#include "vmvec.h"
#include "vmbignum.h"
#include "vmdate.h"
#include "vmtzobj.h"
#include "vmtz.h"
#include "vmfilnam.h"
#include "vmstr.h"
#include "vmstrbuf.h"
#include "vmpat.h"
#include "vmanonfn.h"
#include "vmlookup.h"
#include "vmdynfunc.h"
#include "vmmeta.h"
#include "osifcnet.h"


/* ------------------------------------------------------------------------ */
/*
 *   Special override host interface for compiling expressions in the
 *   debugger.  This host interface allows us to capture error messages
 *   generated during compilation for display in the debugger user
 *   interface. 
 */
class CTcHostIfcDebug: public CTcHostIfc
{
public:
    CTcHostIfcDebug()
    {
        /* initially clear the message buffer */
        reset_messages();
    }

    /* clear the message buffer */
    void reset_messages()
    {
        errmsg_[0] = '\0';
        errmsg_free_ = errmsg_;
        have_errmsg_ = FALSE;
    }

    /* get the text of the first error message since we cleared the buffer */
    const char *get_error_msg() const { return errmsg_; }
    
    /*
     *   CTcHostIfc interface 
     */
    
    /* display an informational message */
    virtual void v_print_msg(const char *, va_list)
    {
        /* ignore informational messages */
    }

    /* display a process step message */
    virtual void v_print_step(const char *, va_list)
    {
        /* ignore process step messages */
    }

    /* display an error message */
    virtual void v_print_err(const char *msg, va_list args)
    {
        /* if we haven't already captured an error message, capture it */
        if (!have_errmsg_)
        {
            char buf[1024];

            /* format the message */
            t3vsprintf(buf, sizeof(buf), msg, args);

            /* append as much of it as possible to our buffer */
            lib_strcpy(errmsg_free_,
                       sizeof(errmsg_) - (errmsg_free_ - errmsg_) - 1,
                       buf);

            /* advance the free pointer past what we just added */
            errmsg_free_ += strlen(errmsg_free_);

            /* 
             *   if there's a newline in the appended message, we're done
             *   gathering this message line 
             */
            if (strchr(buf, '\n') != 0)
            {
                char *p;
                
                /* note that we have the complete first error line now */
                have_errmsg_ = TRUE;

                /* terminate the message at the newline */
                p = strchr(errmsg_, '\n');
                if (p != 0)
                    *p = '\0';
            }
        }
    }

private:
    /* our error message buffer */
    char errmsg_[256];

    /* pointer to next available byte of message buffer */
    char *errmsg_free_;

    /* flag: we've captured an error message */
    uint have_errmsg_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   Compiler parser symbol table interface.  This provides an
 *   implementation of the compiler parser's debugger symbol table
 *   interface, which allows the compiler to look up symbols in the image
 *   file's debug records.  
 */
class CVmDbgSymtab: public CTcPrsDbgSymtab
{
public:
    CVmDbgSymtab()
    {
        globals_ = 0;
        frame_id_ = 0;
    }
    
    /* initialize in a given scope */
    CVmDbgSymtab(VMG_ const CVmDbgTablePtr *debug_table, uint frame_id,
                 uint stack_level)
    {
        /* initialize with the given parameters */
        init(vmg_ debug_table, frame_id, stack_level);
    }

    /* find a symbol - implementation of CTcPrsDbgSymtab interface */
    virtual int find_symbol(const textchar_t *sym, size_t len,
                            tcprsdbg_sym_info *info);

    void init(VMG_ const CVmDbgTablePtr *debug_table, uint frame_id,
              uint stack_level)
    {
        /* save the globals if necessary */
        globals_ = VMGLOB_ADDR;

        /* save my debug table pointer */
        debug_ptr_.copy_from(debug_table);

        /* save my frame ID */
        frame_id_ = frame_id;

        /* save my stack level */
        stack_level_ = stack_level;
    }

private:
    /* VM globals */
    vm_globals *globals_;
    
    /* my debug records table */
    CVmDbgTablePtr debug_ptr_;

    /* active frame ID for this scope */
    uint frame_id_;

    /* stack frame level */
    uint stack_level_;
};

/*
 *   Find a symbol 
 */
int CVmDbgSymtab::find_symbol(const textchar_t *sym, size_t len,
                              tcprsdbg_sym_info *info)
{
    CVmDbgFramePtr frame;
    VMGLOB_PTR(globals_);

    /* if I have no local frame, there's nothing to do */
    if (frame_id_ == 0)
        return FALSE;
    
    /* 
     *   search the enclosing frames, starting with my innermost frame and
     *   working outwards 
     */
    debug_ptr_.set_frame_ptr(vmg_ &frame, frame_id_);
    for (;;)
    {
        CVmDbgFrameSymPtr sym_ptr;
        uint i;
        
        /* iterate over the symbols in this frame */
        for (i = frame.get_sym_count(),
             frame.set_first_sym_ptr(vmg_ &sym_ptr) ;
             i != 0 ; --i, sym_ptr.inc(vmg0_))
        {
            /* if this one matches, we found it */
            if (sym_ptr.get_sym_len(vmg0_) == len
                && memcmp(sym_ptr.get_sym(vmg0_), sym, len) == 0)
            {
                /* fill in the type information for the caller */
                if (sym_ptr.is_ctx_local())
                {
                    /* it's a context local */
                    info->sym_type = TC_SYM_LOCAL;
                    info->ctx_arr_idx = sym_ptr.get_ctx_arr_idx();

                    /* fill in the local variable ID and stack level */
                    info->var_id = sym_ptr.get_var_num();
                    info->frame_idx = stack_level_;
                }
                else if (sym_ptr.is_local())
                {
                    /* it's a local */
                    info->sym_type = TC_SYM_LOCAL;
                    info->ctx_arr_idx = 0;

                    /* fill in the local variable ID and stack level */
                    info->var_id = sym_ptr.get_var_num();
                    info->frame_idx = stack_level_;
                }
                else if (sym_ptr.is_param())
                {
                    /* it's a parameter variable */
                    info->sym_type = TC_SYM_PARAM;
                    info->ctx_arr_idx = 0;

                    /* fill in the parameter ID and stack level */
                    info->var_id = sym_ptr.get_var_num();
                    info->frame_idx = stack_level_;
                }
                else
                {
                    /* unknown symbol type */
                    info->sym_type = TC_SYM_UNKNOWN;
                }

                /* tell the caller we found it */
                return TRUE;
            }
        }
        
        /* if there's no enclosing frame, we're done */
        if (frame.get_enclosing_frame() == 0)
            break;

        /* move on to the enclosing frame */
        debug_ptr_.set_frame_ptr(vmg_ &frame, frame.get_enclosing_frame());
    }

    /* failure */
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Creation
 */
CVmDebug::CVmDebug(VMG0_)
{
    /* create our debugger break event object */
    break_event_ = new OS_Event(TRUE);

    /* we have not yet been initialized during program load */
    program_inited_ = FALSE;

    /* we have no UI context yet */
    ui_ctx_ = 0;

    /* not currently in the debugger */
    in_debugger_ = FALSE;

    /* there's no valid debug pointer yet */
    dbg_ptr_valid_ = FALSE;

    /* 
     *   start in step-in mode, so that we break as soon as we start
     *   executing the program
     */
    set_step_in();

    /* note in step-over-breakpoint mode yet */
    step_over_bp_ = FALSE;
    step_frame_depth_ = 0;

    /* no global breakpoints yet */
    global_bp_cnt_ = 0;

    /* 
     *   we have no valid source line bounds yet, so set both ends to zero
     *   - zero is never a valid PC address (even if a function is at
     *   address zero, its method header will precede any executable code
     *   in the function, hence the program counter could never be at
     *   zero) 
     */
    cur_stm_start_ = cur_stm_end_ = 0;
    entry_ = 0;

    /* create our reverse-lookup hash tables */
    obj_rev_table_ = new CVmHashTable(256, new CVmHashFuncDbgRev, TRUE);
    prop_rev_table_ = new CVmHashTable(256, new CVmHashFuncDbgRev, TRUE);
    func_rev_table_ = new CVmHashTable(256, new CVmHashFuncDbgRev, TRUE);
    enum_rev_table_ = new CVmHashTable(256, new CVmHashFuncDbgRev, TRUE);
    bif_rev_table_ = new CVmHashTable(256, new CVmHashFuncDbgRev, TRUE);

    /* create a host interface for the compiler */
    hostifc_ = new CTcHostIfcDebug();

    /* no halt requested yet */
    G_interpreter->set_halt_vm(FALSE);

    /* we haven't allocated a method header list yet */
    method_hdr_ = 0;
    method_hdr_cnt_ = 0;
}

/*
 *   Deletion
 */
CVmDebug::~CVmDebug()
{
    /* delete the host interface object */
    delete hostifc_;

    /* delete our reverse-lookup hash tables */
    delete obj_rev_table_;
    delete prop_rev_table_;
    delete func_rev_table_;
    delete enum_rev_table_;
    delete bif_rev_table_;

    /* delete the method header list */
    if (method_hdr_ != 0)
        t3free(method_hdr_);

    /* release the debugger break event */
    break_event_->release_ref();
}

/* ------------------------------------------------------------------------ */
/*
 *   VM initialization 
 */
void CVmDebug::init(VMG_ const char *image_fname)
{
    /* note that the program has been initialized */
    program_inited_ = TRUE;

    /* tell the UI to initialize */
    CVmDebugUI::init(vmg_ image_fname);

    /* initialize the dynamic compiler */
    CVmDynamicCompiler::get(vmg0_);
}

/*
 *   Initialization phase 2 - after loading the image file 
 */
void CVmDebug::init_after_load(VMG0_)
{
    /* tell the UI to initialize */
    CVmDebugUI::init_after_load(vmg0_);
}

/*
 *   VM termination 
 */
void CVmDebug::terminate(VMG0_)
{
    CVmDebugBp *bp;
    size_t i;

    /* if we were never initialized, there's nothing to terminate */
    if (!program_inited_)
        return;

    /* tell the UI to terminate */
    CVmDebugUI::terminate(vmg0_);

    /* tell the breakpoint objects we're about to terminate */
    for (i = 0, bp = bp_ ; i < VMDBG_BP_MAX ; ++i, ++bp)
        bp->do_terminate(vmg0_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Determine if the image file has debugger information 
 */
int CVmDebug::image_has_debug_info(VMG0_) const
{
    /* ask the image loader if there's a GSYM block */
    return G_image_loader->has_gsym();
}


/* ------------------------------------------------------------------------ */
/*
 *   Add an entry to the reverse-lookup hash tables 
 */
void CVmDebug::add_rev_sym(const char *sym, size_t sym_len,
                           tc_symtype_t sym_type, ulong sym_val)
{
    CVmHashTable *table;
    CVmHashEntryDbgRev *entry;
    
    /* figure out which table to use, based on the symbol type */
    switch(sym_type)
    {
    case TC_SYM_FUNC:
        /* function */
        table = func_rev_table_;
        break;
        
    case TC_SYM_OBJ:
        /* object */
        table = obj_rev_table_;
        break;

    case TC_SYM_PROP:
        /* property */
        table = prop_rev_table_;
        break;

    case TC_SYM_ENUM:
        /* enumerator */
        table = enum_rev_table_;
        break;

    case TC_SYM_BIF:
        /* built-in function */
        table = bif_rev_table_;
        break;

    default:
        /* 
         *   ignore other symbol types - we don't maintain reverse-lookup
         *   tables for anything else 
         */
        return;
    }

    /* create the symbol */
    entry = new CVmHashEntryDbgRev(sym_val, sym, sym_len);

    /* add it to the table */
    table->add(entry);
}

/*
 *   Look up a symbol in one of our reverse-mapping tables 
 */
const char *CVmDebug::find_rev_sym(const CVmHashTable *tab,
                                   ulong val) const
{
    /* look up the symbol */
    CVmHashEntryDbgRev *entry =
        (CVmHashEntryDbgRev *)tab->find((char *)&val, sizeof(val));

    /* if we found it, return the name pointer, otherwise null */
    return (entry != 0 ? entry->get_sym() : 0);
}

/*
 *   Find the actual symbol for a 'modify' chain. 
 */
const char *CVmDebug::get_modifying_sym(const char *sym_name) const
{
    /* if there's no original symbol, there's obviously no modifier */
    if (sym_name == 0)
        return 0;

    /* keep going until we find the base object */
    for (;;)
    {
        CTcSymObj *sym;
        
        /* look up the symbol in the global symbol table */
        sym = (CTcSymObj *)G_prs->get_global_symtab()
              ->find(sym_name, strlen(sym_name));

        /* 
         *   if we found it, and it has a modifier object, proceed up to the
         *   modifier object 
         */
        if (sym != 0
            && sym->get_type() == TC_SYM_OBJ
            && sym->get_modifying_obj_id() != VM_INVALID_OBJ)
        {
            const char *new_sym_name;
            
            /* 
             *   Forget the current object, and use the modification object
             *   instead.  Look up the symbol name for the modification
             *   object, and replace the previous symbol - which was just a
             *   fake symbol that the compiler synthesized anyway - with the
             *   modifying object.  At the top of the chain of modifying
             *   objects is the actual symbol name used in the source code,
             *   which is what we're after.  
             */
            new_sym_name = objid_to_sym(sym->get_modifying_obj_id());

            /* 
             *   if we can't find the modifying object name, stop at the
             *   previous symbol 
             */
            if (new_sym_name == 0)
                return sym_name;

            /* we got a symbol - use it */
            sym_name = new_sym_name;
        }
        else
        {
            /* 
             *   There's either no symbol or no modifier, so we can stop
             *   looking.  Simply return what we have.  
             */
            return sym_name;
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Given a function header address, get the associated symbol 
 */
const char *CVmDebug::funchdr_to_sym(
    VMG_ const uchar *func_addr, char *buf) const
{
    /* convert the function address to a value */
    CVmFuncPtr fp((const char *)func_addr);
    vm_val_t v;
    if (fp.get_fnptr(vmg_ &v))
    {
        /* check the type */
        const char *ret;
        switch (v.typ)
        {
        case VM_OBJ:
            /* code object - return the symbol for the object, if any */
            if ((ret = objid_to_sym(v.val.obj)) == 0)
            {
                ret = buf;
                sprintf(buf, "DynamicFunc#%lx", (unsigned long)v.val.obj);
            }
            return ret;

        case VM_FUNCPTR:
        case VM_CODEOFS:
            /* function pointer or code offset - return the function */
            if ((ret = funcaddr_to_sym(v.val.ofs)) == 0)
            {
                ret = buf;
                sprintf(buf, "func#%lx", (unsigned long)v.val.ofs);
            }
            return ret;

        default:
            break;
        }
    }

    /* there's no symbolic form of the address */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Get information on the source location at a given stack level 
 */
int CVmDebug::get_source_info(VMG_ const char **fname,
                              unsigned long *linenum, int level) const
{
    CVmFuncPtr func_ptr;
    CVmDbgLinePtr line_ptr;
    CVmSrcfEntry *srcf_entry;
    const uchar *stm_start, *stm_end;

    /* if there's no source file table, we can't get any information */
    if (G_srcf_table == 0)
        return 1;

    /* get information on the execution location at the requested level */
    if (get_stack_level_info(vmg_ level, &func_ptr, &line_ptr,
                             &stm_start, &stm_end))
        return 1;

    /* get the source file entry for this line's source file index */
    srcf_entry = G_srcf_table->get_entry(line_ptr.get_source_id());

    /* if we didn't find an entry, we can't return any information */
    if (srcf_entry == 0)
        return 1;

    /* fill in the caller's filename pointer */
    *fname = srcf_entry->get_name();

    /* fill in the caller's line number record */
    *linenum = line_ptr.get_source_line();

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Enumerate local variables at the given stack level 
 */
void CVmDebug::enum_locals(VMG_ void (*cbfunc)(void *, const char *, size_t),
                           void *cbctx, int level)
{
    CVmFuncPtr func_ptr;
    CVmDbgLinePtr line_ptr;
    CVmDbgTablePtr dbg_ptr;
    CVmDbgFramePtr frame_ptr;
    CVmDbgFrameSymPtr sym;
    const uchar *stm_start, *stm_end;
    uint i;
    uint frame_id;

    /* get information on the execution location at the requested level */
    if (get_stack_level_info(vmg_ level, &func_ptr, &line_ptr,
                             &stm_start, &stm_end))
        return;

    /* set up a debug table pointer for the function */
    if (!func_ptr.set_dbg_ptr(&dbg_ptr))
        return;

    /* if there's a 'self' in this context, add it to the list */
    if (G_interpreter->get_self_at_level(vmg_ level) != VM_INVALID_OBJ)
        (*cbfunc)(cbctx, "self", 4);

    /* iterate over the enclosing frames, starting with the innermost */
    for (frame_id = line_ptr.get_frame_id() ; frame_id != 0 ;
         frame_id = frame_ptr.get_enclosing_frame())
    {
        /* set up a frame table pointer for the current local frame */
        dbg_ptr.set_frame_ptr(vmg_ &frame_ptr, frame_id);

        /* set up a pointer to the first symbol */
        frame_ptr.set_first_sym_ptr(vmg_ &sym);

        /* iterate through the frame and call the callback for each symbol */
        for (i = frame_ptr.get_sym_count() ; i != 0 ; --i, sym.inc(vmg0_))
            (*cbfunc)(cbctx, sym.get_sym(vmg0_), sym.get_sym_len(vmg0_));
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Callback for enumerating the named argument table, to add the named
 *   arguments to the stack trace. 
 */
struct named_arg_stack_ctx
{
    named_arg_stack_ctx(CVmDebug *dbg, char *p, size_t rem, int argc)
    {
        this->dbg = dbg;
        this->p = p;
        this->rem = rem;
        this->argc = argc;
    }

    /* debugger object */
    CVmDebug *dbg;

    /* current write buffer position */
    char *p;
    size_t rem;

    /* number of arguments so far, including previous position arguments */
    int argc;

    void append(const char *txt, size_t len)
    {
        if (len < rem)
        {
            memcpy(p, txt, len);
            p += len;
            rem -= len;
            *p = '\0';
        }
        else if (rem > 3)
        {
            strcpy(p, "...");
            rem = 0;
        }
        else
        {
            rem = 0;
        }
    }
    void append(const char *txt) { append(txt, strlen(txt)); }

    void add(VMG_ const char *name, size_t len, const vm_val_t *val)
    {
        /* add "<comma> name: val" */
        if (argc++ > 0)
            append(", ");

        /* add the name */
        append(name, len);

        /* add the colon */
        append(":");

        /* add the value */
        dbg->format_val(vmg_ p, rem, val);

        /* advance past it */
        size_t l = strlen(p);
        p += l;
        rem -= l;
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Build a stack traceback listing 
 */
void CVmDebug::build_stack_listing(VMG_
                                   void (*cbfunc)(void *, const char *, int),
                                   void *cbctx, int source_info)
{
    const uchar *methodp;
    vm_val_t *fp;
    vm_obj_id_t self_obj;
    const uchar *entry;
    const vm_rcdesc *rc;
    int level;
    
    /* start at the current function */
    fp = G_interpreter->get_frame_ptr();
    self_obj = G_interpreter->get_self(vmg0_);
    entry = entry_;
    methodp = pc_;

    /* iterate through the frames */
    for (level = 0, rc = 0 ; fp != 0 ; ++level)
    {
        vm_obj_id_t def_obj;
        int argc;
        const vm_val_t *argp = 0;
        char buf[256];
        int i;
        char *p;
        char *open_paren_ptr = 0;
        char *close_paren_ptr = 0;
        size_t rem;

        /* get the current 'self' object */
        self_obj = G_interpreter->get_self_from_frame(vmg_ fp);

        /* get the current number of arguments */
        argc = G_interpreter->get_argc_from_frame(vmg_ fp);
        
        /* get the current function's defining object */
        def_obj = G_interpreter->get_defining_obj_from_frame(vmg_ fp);

        /* determine whether we have an object.method or a function call */
        if (methodp == 0)
        {
            /* 
             *   A zero method offset indicates a native caller making a
             *   recursive entry into the byte-code interpreter.  If we have
             *   a recursive caller context, we can get the information from
             *   it; otherwise show a generic "System" caller.  
             */
            sprintf(buf, "<System>");
            argc = 0;
            if (rc != 0)
            {
                /* check what kind of native caller we have */
                if (rc->bifptr.typ != VM_NIL)
                {
                    /* we have a built-in function at this level */
                    const char *func_sym = bif_to_sym(
                        rc->bifptr.val.bifptr.set_idx,
                        rc->bifptr.val.bifptr.func_idx);
                    if (func_sym != 0)
                        sprintf(buf, "%.255s", func_sym);
                    else
                        sprintf(buf, "intrinsicFunction#%d.%d",
                                (rc->bifptr.val.intval >> 16) & 0xffff,
                                rc->bifptr.val.intval & 0xffff);

                    /* use the arguments from the native descriptor */
                    argc = rc->argc;
                    argp = rc->argp;
                }
                else if (rc->self.typ != VM_NIL)
                {
                    const char *ic_sym = 0;
                    const char *prop_sym = 0;
                    
                    /* intrinsic class method - get the metaclass */
                    CVmMetaclass *mc;
                    switch (rc->self.typ)
                    {
                    case VM_OBJ:
                        /* get the metaclass from the object */
                        mc = vm_objp(vmg_ rc->self.val.obj)
                             ->get_metaclass_reg();
                        break;

                    case VM_LIST:
                        /* list constant - use the List metaclass */
                        mc = CVmObjList::metaclass_reg_;
                        break;

                    case VM_SSTRING:
                        /* string constant - use the String metaclass */
                        mc = CVmObjString::metaclass_reg_;
                        break;

                    default:
                        /* other types don't have metaclasses */
                        mc = 0;
                        break;
                    }

                    /* get the registration table entry */
                    vm_meta_entry_t *me =
                        mc == 0 ? 0 :
                        G_meta_table->get_entry_from_reg(mc->get_reg_idx());

                    /* get the metaclass object and property from the entry */
                    if (me != 0)
                    {
                        /* get the IntrinsicClass object */
                        ic_sym = objid_to_sym(me->class_obj_);

                        /* get the property ID */
                        prop_sym = propid_to_sym(me->xlat_func(rc->method_idx));
                    }

                    /* build the object.property name */
                    if (ic_sym != 0)
                        sprintf(buf, "%.255s [", ic_sym);
                    else if (me != 0)
                        sprintf(buf, "IntrinsicClass#%lx [",
                                (unsigned long)me->class_obj_);
                    else
                        sprintf(buf, "IntrinsicClass [");

                    /* add the 'self' object */
                    size_t cur_len = strlen(buf);
                    p = buf + cur_len;
                    rem = sizeof(buf) - cur_len;
                    format_val(vmg_ p, rem - 10, &rc->self);

                    /* add the property separator */
                    cur_len = strlen(p);
                    rem -= cur_len;
                    p += cur_len;
                    if (rem > 10)
                    {
                        *p++ = ']';
                        *p++ = '.';
                        *p = '\0';
                        rem -= 2;
                    }

                    /* add the property name */
                    if (rem > 15)
                    {
                        if (prop_sym != 0)
                            sprintf(p, "%.*s", rem - 1, prop_sym);
                        else if (me != 0)
                            sprintf(p, "prop#%x",
                                    (int)me->xlat_func(rc->method_idx));
                        else
                            sprintf(p, "prop#?");
                    }

                    /* use the arguments from the native descriptor */
                    argc = rc->argc;
                    argp = rc->argp;
                }
            }
        }
        else if (def_obj != VM_INVALID_OBJ)
        {
            const char *def_obj_sym;
            const char *self_obj_sym;
            const char *prop_sym;
            vm_prop_id_t prop_id;
            int is_anon_fn = FALSE;
            
            /* it's an object.method call */
            def_obj_sym = objid_to_sym(def_obj);
            self_obj_sym = objid_to_sym(self_obj);

            /* translate the object to the original 'modify' object name */
            def_obj_sym = get_modifying_sym(def_obj_sym);

            /* 
             *   Add the object name.  If we have a name for 'self', use that
             *   name; otherwise, if we have a name for the defining object,
             *   use that, and show the self object number after it;
             *   otherwise, if this is an anonymous function object, note
             *   that and show it specially; otherwise just show the object
             *   number.  
             */
            if (def_obj != VM_INVALID_OBJ
                && CVmObjAnonFn::is_anonfn_obj(vmg_ def_obj))
            {
                /* 
                 *   it's an anonymous function - note this, and use a
                 *   special format for the display 
                 */
                is_anon_fn = TRUE;
                sprintf(buf, "{anonfn:%lx}", (unsigned long)def_obj);
            }
            else if (self_obj_sym != 0)
            {
                /* 'self' has a name, so show that */
                strcpy(buf, self_obj_sym);
            }
            else if (def_obj_sym != 0)
            {
                /* 
                 *   'self' has no name, but the defining object does, so
                 *   show the defining object name and add the 'self' object
                 *   number 
                 */
                sprintf(buf, "%.240s [%lx]",
                        def_obj_sym, (unsigned long)self_obj);
            }
            else
            {
                /* there's no name to be had, so use the object number */
                sprintf(buf, "[%lx]", (unsigned long)self_obj);
            }

            /* prepare to add to the buffer */
            p = buf + strlen(buf);

            /* get the property ID */
            prop_id = G_interpreter->get_target_prop_from_frame(vmg_ fp);
            if (is_anon_fn)
            {
                /* it's an anonymous function - there's no property part */
                *p = '\0';
            }
            else if (prop_id != VM_INVALID_PROP
                     && (prop_sym = propid_to_sym(prop_id)) != 0)
            {
                /* we got the property symbol - add it */
                sprintf(p, ".%.255s", prop_sym);
            }
            else
            {
                /* unknown property name */
                sprintf(p, ".prop#%x", (uint)prop_id);
            }
        }
        else if (entry != 0)
        {
            const char *func_sym;
            char fbuf[256];
            
            /* no object, so it's a function - find the symbol */
            if ((func_sym = funchdr_to_sym(vmg_ entry, fbuf)) != 0)
                sprintf(buf, "%.255s", func_sym);
            else
                sprintf(buf, "bytecode#%08lx", (unsigned long)entry);
        }

        /* get the remainder of the buffer */
        p = buf + strlen(buf);
        rem = sizeof(buf) - (p - buf);

        /* add the open paren */
        open_paren_ptr = p;
        *p++ = '(';
        --rem;

        /* add the arguments */
        for (i = 0 ; i < argc ; ++i)
        {
            size_t cur_len;

            /* 
             *   if we're running out of space, just add an indication
             *   that more arguments follow, and stop here 
             */
            if (rem <= 10)
            {
                strcpy(p, " ...");
                p += 4;
                rem -= 4;
                break;
            }

            /* 
             *   add this argument, making sure we leave room over for the
             *   closing paren, newline, null terminator, and if necessary
             *   an ellipsis indicating we ran out of room 
             */
            format_val(vmg_ p, rem - 10,
                       argp != 0 ? argp - i :
                       G_interpreter->get_param_from_frame(vmg_ fp, i));

            /* move past the latest addition */
            cur_len = strlen(p);
            rem -= cur_len;
            p += cur_len;

            /* add a comma if this isn't the last argument */
            if (i + 1 < argc)
            {
                *p++ = ',';
                *p++ = ' ';
                *p = '\0';
                rem -= 2;
            }
        }

        /* check for named arguments */
        vm_val_t *nargp;
        const uchar *t = CVmRun::get_named_args_from_frame(vmg_ fp, &nargp);
        if (t != 0)
        {
            /* get the number of named arguments in the table */
            int cnt = osrp2(t);
            t += 2;

            /* 
             *   Display the named arguments.  The compiler builds the table
             *   in right-to-left order, reflecting the order of pushing the
             *   arguments onto the stack.  For readability, reverse this for
             *   display, so that the display order matches the original
             *   source code order.  
             */
            named_arg_stack_ctx ctx(this, p, rem, argc);
            nargp += (cnt - 1);
            for (int i = (cnt-1)*2 ; i >= 0 ; i -= 2, --nargp)
            {
                /* get this item's name and figure its length */
                uint ofs = osrp2(t + i);
                uint len = osrp2(t + i + 2) - ofs;

                /* add the name */
                ctx.add(vmg_ (const char *)t + ofs, len, nargp);
            }

            /* advance past the data we wrote */
            p = ctx.p;
            rem = ctx.rem;
        }

        /* add a close paren */
        close_paren_ptr = p;
        *p++ = ')';

        /* if we have room, add the method offset */
        if (methodp != 0 && entry != 0 && rem > 12)
        {
            sprintf(p, " + 0x%lX", (unsigned long)(methodp - entry));
            p += strlen(p);
        }

        /* add a newline and a null terminator */
        *p++ = '\n';
        *p = '\0';

        /* if they want source line information, add it */
        if (methodp != 0 && source_info)
        {
            const char *fname;
            unsigned long linenum;
            
            /* get the source information */
            if (!get_source_info(vmg_ &fname, &linenum, level))
            {
                char numbuf[32];
                size_t fname_len;
                size_t num_len;
                
                /* convert the line number to a string */
                sprintf(numbuf, " line %ld]\n", linenum);

                /* get the lengths of what we're adding */
                fname_len = strlen(fname);
                num_len = strlen(numbuf);

                /* 
                 *   if we don't have room, try getting the root of the
                 *   filename, in case it has a long path prefix 
                 */
                if (fname_len + num_len + 2 >= rem)
                {
                    /* skip to the root of the filename */
                    fname = os_get_root_name((char *)fname);

                    /* get the new length */
                    fname_len = strlen(fname);
                }

                /* 
                 *   If it's still too long, we must have a really long
                 *   argument list - try removing some of the argument list,
                 *   adding "..." in place of the removed bits, to make room
                 *   for the line number.  Don't bother if we can't free up
                 *   enough space even doing this.  
                 */
                if (fname_len + num_len + 2 >= rem
                    && close_paren_ptr != 0
                    && (fname_len + num_len + 2
                        < rem + (close_paren_ptr - open_paren_ptr - 4)))
                {
                    char *dst;
                    size_t adjust;

                    /* 
                     *   get the adjustment size - back up by as much as we
                     *   need to make the filename/number fit, plus space
                     *   for the terminating null and the ellipsis 
                     */
                    adjust = (fname_len + num_len + 2 - rem + 1 + 3);
                    
                    /* back up by the adjustment */
                    dst = close_paren_ptr - adjust;

                    /* insert the ellipsis */
                    memcpy(dst, "...", 3);

                    /* move the part after the close paren */
                    memmove(dst + 3, close_paren_ptr,
                            strlen(close_paren_ptr));

                    /* 
                     *   adjust by the amount we moved, not counting the
                     *   ellipsis we added 
                     */
                    rem += adjust - 3;
                    p -= adjust - 3;
                }
                
                /* add it if there's room */
                if (fname_len + num_len + 2 < rem)
                {
                    /* change the newline to a space */
                    *(p-1) = ' ';

                    /* add the filename and line number in brackets */
                    *p = '[';
                    memcpy(p + 1, fname, fname_len);
                    memcpy(p + 1 + fname_len, numbuf, num_len + 1);
                }
            }
        }

        /* invoke the callback for this level */
        (*cbfunc)(cbctx, buf, strlen(buf));

        /* move on to the enclosing frame */
        if (rc != 0 && rc->has_return_addr())
        {
            /* 
             *   recursive context - stay in the same frame, but move to the
             *   bytecode caller
             */
            methodp = rc->get_return_addr();
            rc = 0;
        }
        else
        {
            /* no recursive context - move to the next stack frame */
            entry = G_interpreter
                    ->get_enclosing_entry_ptr_from_frame(vmg_ fp);
            methodp = G_interpreter->get_return_addr_from_frame(vmg_ fp);
            rc = G_interpreter->get_rcdesc_from_frame(vmg_ fp);
            fp = G_interpreter->get_enclosing_frame_ptr(vmg_ fp);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Toggle a breakpoint 
 */
int CVmDebug::toggle_breakpoint(VMG_ const uchar *code_addr,
                                const char *cond, int change,
                                int *bpnum, int *did_set,
                                char *errbuf, size_t errbuflen)
{
    char *code_ptr;
    CVmDebugBp *bp;

    /* if there's a code address, get a pointer to it */
    if (code_addr != 0)
    {
        /* get a writable pointer to the code location */
        code_ptr = (char *)code_addr;
    }
    else
    {
        /* global breakpoint - no code address is required */
        code_ptr = 0;
    }

    /* search for a breakpoint at this same address */
    bp = find_bp(code_addr);

    /* 
     *   if we found it, we're deleting the old one; otherwise, we're
     *   creating a new one 
     */
    if (bp == 0)
    {
        int err;
        
        /* setting a new breakpoint - allocate a new record */
        bp = alloc_bp();
        if (bp == 0)
        {
            /* generate a message */
            if (errbuf != 0 && errbuflen != 0)
                lib_strcpy(errbuf, errbuflen,
                           "out of internal breakpoint records");

            /* return failure */
            return 2;
        }

        /* initialize the breakpoint */
        if ((err = bp->set_info(vmg_ code_addr, cond, change, FALSE,
                                errbuf, errbuflen)) != 0)
        {
            /* un-allocate the breakpoint */
            bp->set_in_use(FALSE);
            
            /* return the error */
            return err;
        }

        /* indicate to the caller that we're creating a breakpoint */
        *did_set = TRUE;

        /* 
         *   fill in the breakpoint ID for the caller - this is just the
         *   1-based index into our breakpoint array of the breakpoint
         *   record 
         */
        *bpnum = (int)(bp - bp_) + 1;

        /* if it's global, count it */
        if (bp->is_global())
            ++global_bp_cnt_;
    }
    else
    {
        /* if it's global, count the deletion */
        if (bp->is_global() && !bp->is_disabled())
            --global_bp_cnt_;
        
        /* free the breakpoint record */
        bp->do_delete(vmg0_);

        /* indicate to the caller that we're deleting the breakpoint */
        *did_set = FALSE;

        /* let the caller know which breakpoint we deleted */
        *bpnum = (int)(bp - bp_) + 1;
    }

    /* success */
    return 0;
}

/*
 *   Toggle a breakpoint's enabled/disabled status 
 */
void CVmDebug::toggle_breakpoint_disable(VMG_ int bpnum)
{
    CVmDebugBp *bp;

    /* if the breakpoint ID is invalid, ignore the request */
    if (bpnum < 1 || bpnum > VMDBG_BP_MAX)
        return;

    /* get the breakpoint record (the breakpoint ID is a 1-based index) */
    bp = bp_ + (bpnum - 1);

    /* toggle the disabled flag */
    set_breakpoint_disable(vmg_ bpnum, !bp->is_disabled());
}

/*
 *   Set a breakpoint's disabled status 
 */
void CVmDebug::set_breakpoint_disable(VMG_ int bpnum, int disable)
{
    CVmDebugBp *bp;

    /* if the breakpoint ID is invalid, ignore the request */
    if (bpnum < 1 || bpnum > VMDBG_BP_MAX)
        return;

    /* get the breakpoint record (the breakpoint ID is a 1-based index) */
    bp = bp_ + (bpnum - 1);

    /* set the new state */
    bp->set_disabled(disable);

    /* add or remove the breakpoint from the code if necessary */
    bp->set_bp_instr(vmg_ !bp->is_disabled(), FALSE);

    /* if it's global, count the change */
    if (bp->is_global())
    {
        if (bp->is_disabled())
            --global_bp_cnt_;
        else
            ++global_bp_cnt_;
    }
}

/*
 *   Determine if a breakpoint is disabled 
 */
int CVmDebug::is_breakpoint_disabled(VMG_ int bpnum)
{
    CVmDebugBp *bp;

    /* if the breakpoint ID is invalid, ignore the request */
    if (bpnum < 1 || bpnum > VMDBG_BP_MAX)
        return FALSE;

    /* get the breakpoint record (the breakpoint ID is a 1-based index) */
    bp = bp_ + (bpnum - 1);

    /* return the status */
    return (bp->is_disabled() != 0);
}

/*
 *   Set a breakpoint's condition expression 
 */
int CVmDebug::set_breakpoint_condition(VMG_ int bpnum,
                                       const char *cond, int change,
                                       char *errbuf, size_t errbuflen)
{
    CVmDebugBp *bp;
    int err;

    /* if the breakpoint ID is invalid, ignore the request */
    if (bpnum < 1 || bpnum > VMDBG_BP_MAX)
    {
        /* set the error message if needed */
        if (errbuf != 0 && errbuflen != 0)
            lib_strcpy(errbuf, errbuflen, "invalid breakpoint");

        /* return failure */
        return 1;
    }

    /* get the breakpoint record (the breakpoint ID is a 1-based index) */
    bp = bp_ + (bpnum - 1);

    /* set the condition */
    if ((err = bp->set_condition(vmg_ cond, change, errbuf, errbuflen)) != 0)
        return err;

    /* success */
    return 0;
}

/*
 *   Delete a breakpoint 
 */
void CVmDebug::delete_breakpoint(VMG_ int bpnum)
{
    CVmDebugBp *bp;
    
    /* if the breakpoint ID is invalid, ignore the request */
    if (bpnum < 1 || bpnum > VMDBG_BP_MAX)
        return;

    /* get the breakpoint record (the breakpoint ID is a 1-based index) */
    bp = bp_ + (bpnum - 1);

    /* 
     *   If the breakpoint is global, update the global bp count.  (Don't
     *   bother if the breakpoint is disabled, as we don't include disabled
     *   breakpoints in the count in the first place.) 
     */
    if (bp->is_global() && !bp->is_disabled())
        --global_bp_cnt_;

    /* clear the breakpoint */
    bp->do_delete(vmg0_);
}

/*
 *   Allocate a new breakpoint record 
 */
CVmDebugBp *CVmDebug::alloc_bp()
{
    size_t i;
    CVmDebugBp *bp;

    /* scan our list for an entry not currently in use */
    for (i = 0, bp = bp_ ; i < VMDBG_BP_MAX ; ++i, ++bp)
    {
        /* if this one isn't in use, allocate it */
        if (!bp->is_in_use())
        {
            /* mark it in use */
            bp->set_in_use(TRUE);

            /* return the new record */
            return bp;
        }
    }

    /* there are no free records - return failure */
    return 0;
}

/*
 *   Find a breakpoint at a given address 
 */
CVmDebugBp *CVmDebug::find_bp(const uchar *code_addr)
{
    size_t i;
    CVmDebugBp *bp;
    
    /* 
     *   if the code address is zero, we know there can't be a breakpoint
     *   there 
     */
    if (code_addr == 0)
        return 0;

    /* scan our list */
    for (i = 0, bp = bp_ ; i < VMDBG_BP_MAX ; ++i, ++bp)
    {
        /* if this one is in use, and it matches the address, it's the one */
        if (bp->is_in_use() && bp->get_code_addr() == code_addr)
            return bp;
    }

    /* we didn't find it */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Format a value, allocating space 
 */
char *CVmDebug::format_val(VMG_ const vm_val_t *val)
{
    /* first, do the formatting to figure the space we need */
    size_t len = format_val(vmg_ 0, 0, val);

    /* allocate space */
    char *buf = (char *)t3malloc(len + 1);

    /* do the formatting for real this time */
    format_val(vmg_ buf, len + 1, val);

    /* return the allocated buffer */
    return buf;
}

/* ------------------------------------------------------------------------ */
/*
 *   Generic string traverser - for utf-8 or ucs-2 strings
 */
class StrSource
{
public:
    virtual wchar_t nextch() = 0;
    size_t rem;
};

class StrSourceUTF: public StrSource
{
public:
    StrSourceUTF *init(const char *p)
    {
        rem = vmb_get_len(p);
        this->p.set((char *)p + VMB_LEN);
        return this;
    }

    virtual wchar_t nextch()
    {
        if (rem > 0)
        {
            wchar_t ch = p.getch();
            p.inc(&rem);
            return ch;
        }
        else return 0;
    }

    utf8_ptr p;
};

class StrSourceWide: public StrSource
{
public:
    StrSourceWide *init(const wchar_t *p, size_t len)
    {
        this->p = p;
        this->rem = len;
        return this;
    }

    virtual wchar_t nextch()
    {
        if (rem > 0)
        {
            --rem;
            return *p++;
        }
        else
            return 0;
    }

    const wchar_t *p;
};


/* ------------------------------------------------------------------------ */
/*
 *   Lookup table enumerator for building an inline value display 
 */

/* callback context */
struct lookup_lister_ctx
{
    /* debugger object */
    CVmDebug *dbg;

    /* the output buffer */
    char *dst;
    size_t dstlen;

    /* the total space we need so far */
    size_t retlen;

    /* number of items so far */
    int cnt;

    /* enumerator callback, static version */
    static void scb(
        VMG_ const vm_val_t *key, const vm_val_t *val, void *ctx0)
    {
        /* get our context, properly cast, and call its handler */
        lookup_lister_ctx *ctx = (lookup_lister_ctx *)ctx0;
        ctx->cb(vmg_ key, val);
    }

    /* enumerator callback, method version */
    void cb(VMG_ const vm_val_t *key, const vm_val_t *val)
    {
        /* if this isn't the first element, add a comma */
        if (cnt++ != 0)
            append(",");
        
        /* format this key (or just "*" if this is the default value) */
        if (key != 0)
            consume(dbg->format_val(vmg_ dst, dstlen, key));
        else
            append("*");

        /* add the -> */
        append("->");

        /* format the value */
        consume(dbg->format_val(vmg_ dst, dstlen, val));
    }

    /* append text */
    void append(const char *txt) { append(txt, strlen(txt)); }
    void append(const char *txt, size_t len)
    {
        /* copy up to the available space */
        size_t copylen = (len < dstlen ? len : dstlen > 0 ? dstlen - 1 : 0);

        /* copy as much as we can */
        if (copylen != 0)
            memcpy(dst, txt, copylen);

        /* consume the space */
        consume(len);
    }

    /* consume space */
    void consume(size_t len)
    {
        /* count the full text length in the target size */
        retlen += len;

        /* skip the space, but always leave room for a null terminator */
        if (dst != 0 && dstlen > 0)
        {
            if (len < dstlen)
            {
                dst += len;
                dstlen -= len;
            }
            else
            {
                dst += dstlen - 1;
                dstlen = 1;
            }
        }
    }
};



/* ------------------------------------------------------------------------ */
/*
 *   Format a value into the given buffer 
 */
size_t CVmDebug::format_val(VMG_ char *dst, size_t dstlen, const vm_val_t *val)
{
    const char *p;
    char buf[128];
    size_t retlen = 0;
    StrSource *ps = 0;
    StrSourceUTF psUTF;
    StrSourceWide psWide;

    /* format the value based on the type */
    switch(val->typ)
    {
    case VM_NIL:
        /* constant 'nil' value */
        p = "nil";
        break;
        
    case VM_TRUE:
        /* constant 'true' value */
        p = "true";
        break;

    case VM_OBJ:
        /* object reference - get the object's name */
        p = objid_to_sym(val->val.obj);

        /* if there's no symbol name, try alternative display formats */
        if (p == 0)
        {
            /* try it as a string */
            if ((p = vm_objp(vmg_ val->val.obj)->get_as_string(vmg0_)) != 0)
            {
                ps = psUTF.init(p);
                goto format_string;
            }

            /* try it as a list */
            if (vm_objp(vmg_ val->val.obj)->get_as_list() != 0)
                goto format_list;

            /* try it as a BigNumber */
            if (CVmObjBigNum::is_bignum_obj(vmg_ val->val.obj))
            {
                /* cast it to a BigNumber object */
                CVmObjBigNum *bn = (CVmObjBigNum *)vm_objp(vmg_ val->val.obj);

                /* try formatting it as a numeric value */
                if (bn->cvt_to_string_buf(
                    buf, sizeof(buf), -1, -1, -1, -1,
                    VMBN_FORMAT_POINT | VMBN_FORMAT_COMPACT) != 0)
                {
                    /*
                     *   We got a string, but it's in length-prefix notation.
                     *   Change it to C notation by moving the string
                     *   contents to the bottom of the buffer and
                     *   null-terminating.
                     */
                    size_t len = vmb_get_len(buf);
                    memmove(buf, buf + VMB_LEN, len);
                    buf[len] = '\0';
                }
                else
                {
                    /* couldn't format it - show it as an obj# value */
                    sprintf(buf, "obj#%lx (BigNumber)",
                            (unsigned long)val->val.obj);
                    p = buf;
                }
                    
                /* use the buffer */
                p = buf;

                /* handled */
                break;
            }

            /* try it as a Date object */
            if (CVmObjDate::is_date_obj(vmg_ val->val.obj))
            {
                /* cast it to a Date */
                CVmObjDate *date = (CVmObjDate *)vm_objp(vmg_ val->val.obj);

                /* get the formatted date */
                char buf2[64];
                date->format_string_buf(vmg_ buf2, sizeof(buf2));

                /* store it as "Date(m/d/y...)" */
                t3sprintf(buf, sizeof(buf), "Date(%s)", buf2);

                /* use the formatted text */
                p = buf;
                break;
            }

            /* try it as a TimeZone object */
            if (CVmObjTimeZone::is_CVmObjTimeZone_obj(vmg_ val->val.obj))
            {
                /* cast it to a TimeZone */
                CVmObjTimeZone *tz =
                    (CVmObjTimeZone *)vm_objp(vmg_ val->val.obj);

                /* store it as "TimeZone(name)" */
                size_t len;
                const char *name = tz->get_tz()->get_name(len);
                t3sprintf(buf, sizeof(buf), "TimeZone(%.*s)", (int)len, name);

                /* use the formatted text */
                p = buf;
                break;
            }

            /* try it as a FileName object */
            if (CVmObjFileName::is_vmfilnam_obj(vmg_ val->val.obj))
            {
                /* cast it */
                CVmObjFileName *fn = vm_val_cast(CVmObjFileName, val);

                /* store it as "FileName(name)" */
                const char *name = fn->get_path_string() + VMB_LEN;
                t3sprintf(buf, sizeof(buf), "FileName(%s)", name);

                /* use the formatted text */
                p = buf;
                break;
            }

            /* 
             *   It's a dynamically-created object with no name.  Show it
             *   by object it. 
             */
            sprintf(buf, "obj#%lx", (unsigned long)val->val.obj);

            /* if possible, add the name of the of the superclass */
            if (vm_objp(vmg_ val->val.obj)
                ->get_superclass_count(vmg_ val->val.obj) > 0)
            {
                vm_obj_id_t sc_obj;

                /* get the superclass */
                sc_obj = vm_objp(vmg_ val->val.obj)
                         ->get_superclass(vmg_ val->val.obj, 0);

                /* if it's valid, look up the superclass name */
                if (sc_obj != 0 && (p = objid_to_sym(sc_obj)) != 0)
                {
                    /* add "(superclass name)" after the numeric ID */
                    sprintf(buf + strlen(buf), " (%.100s)", p);
                }
            }

            /* 
             *   if it's a StringBuffer, Vector, or LookupTable, start the
             *   result buffer with the object ID string, but also add an
             *   inline expansion of the contents 
             */
            if (CVmObjStringBuffer::is_string_buffer_obj(vmg_ val->val.obj)
                || (CVmObjVector::is_vector_obj(vmg_ val->val.obj)
                    && !CVmObjAnonFn::is_anonfn_obj(vmg_ val->val.obj))
                || CVmObjLookupTable::is_lookup_table_obj(vmg_ val->val.obj)
                || CVmObjPattern::is_pattern_obj(vmg_ val->val.obj))
            {
                /* figure the length we need to add for the type name */
                size_t len = strlen(buf);
                retlen += len + 1;
                
                /* if there's room, add the type name */
                if (dstlen > len + 1)
                {
                    memcpy(dst, buf, len);
                    dst[len] = ' ';
                    dst += len + 1;
                    dstlen -= len + 1;
                }

                /* if it's a StringBuffer, add the string contents */
                if (CVmObjStringBuffer::is_string_buffer_obj(vmg_ val->val.obj))
                {
                    CVmObjStringBuffer *strbuf =
                        ((CVmObjStringBuffer *)vm_objp(vmg_ val->val.obj));
                    ps = psWide.init(strbuf->get_buf(), strbuf->get_len());
                    goto format_string;
                }

                /* if it's a RexPattern object, add the pattern string */
                if (CVmObjPattern::is_pattern_obj(vmg_ val->val.obj))
                {
                    /* get the pattern */
                    CVmObjPattern *pat =
                        (CVmObjPattern *)vm_objp(vmg_ val->val.obj);

                    /* get the source string, if it has one */
                    const vm_val_t *patval;
                    if ((patval = pat->get_orig_str()) != 0
                        && (p = patval->get_as_string(vmg0_)) != 0)
                    {
                        /* got it - go add the string to the display */
                        ps = psUTF.init(p);
                        goto format_string;
                    }
                }

                /*  if it's a Vector, add the contents in list format */
                if (CVmObjVector::is_vector_obj(vmg_ val->val.obj))
                    goto format_list;

                /* if it's a lookup table, add the table contents */
                if (CVmObjLookupTable::is_lookup_table_obj(vmg_ val->val.obj))
                {
                    /* get the lookup table object, properly cast */
                    CVmObjLookupTable *lt =
                        (CVmObjLookupTable *)vm_objp(vmg_ val->val.obj);

                    /* set up our enumerator context */
                    lookup_lister_ctx lctx;
                    lctx.dbg = this;
                    lctx.dst = dst;
                    lctx.dstlen = dstlen;
                    lctx.retlen = retlen;
                    lctx.cnt = 0;

                    /* start the list */
                    lctx.append("[");

                    /* enumerate the table's contents */
                    lt->for_each(vmg_ &lctx.scb, &lctx);;

                    /* if there's a default value, add it */
                    vm_val_t defval;
                    lt->get_default_val(&defval);
                    if (defval.typ != VM_NIL)
                        lctx.cb(vmg_ 0, &defval);

                    /* end the list */
                    lctx.append("]");

                    /* null-terminate the table */
                    if (lctx.dst != 0 && lctx.dstlen > 0)
                        *lctx.dst = '\0';
                    
                    /* we've handled this value fully */
                    return lctx.retlen;
                }
            }

            /* use the buffer we formatted */
            p = buf;
        }
        break;

    case VM_PROP:
        /* property ID - get the property name */
        p = propid_to_sym(val->val.prop);
        if (p != 0)
        {
            /* we have a symbol - use it with an ampersand operator */
            sprintf(buf, "&%.127s", p);
        }
        else
        {
            /* there's no symbol, so use the numeric property ID */
            sprintf(buf, "prop#%x", (uint)val->val.prop);
        }

        /* use the text we built in the buffer */
        p = buf;
        break;

    case VM_ENUM:
        /* enum ID - get the enum name */
        p = enum_to_sym(val->val.enumval);

        /* if there's no symbol, use the numeric enum ID */
        if (p == 0)
        {
            sprintf(buf, "enum#%x", (uint)val->val.enumval);
            p = buf;
        }
        break;

    case VM_BIFPTR:
        /* built-in function pointer - get the function name */
        p = bif_to_sym(val->val.bifptr.set_idx, val->val.bifptr.func_idx);
        if (p != 0)
        {
            /* we have a symbol - use it with an ampersand operator */
            sprintf(buf, "&%.127s", p);
            p = buf;
        }
        else
        {
            /* there's no symbol, so use the numeric ID */
            sprintf(buf, "&intrinsic_function#%u:%u",
                    val->val.bifptr.set_idx, val->val.bifptr.func_idx);
            p = buf;
        }
        break;

    case VM_INT:
        /* integer */
        sprintf(buf, "%ld", (long)val->val.intval);
        p = buf;
        break;

    case VM_SSTRING:
        /* length-prefixed string - get a pointer to the data */
        ps = psUTF.init(G_const_pool->get_ptr(val->val.ofs));

    format_string:
        /* add an open quote, if we have room for it */
        ++retlen;
        if (dstlen > 1)
        {
            *dst++ = '\'';
            --dstlen;
        }
        
        /* 
         *   keep going until we run out of source text or space in our
         *   output buffer - leave ourselves 5 extra bytes (close quote,
         *   "..." if we run out of space, and null terminator) 
         */
        while (ps->rem != 0)
        {
            /* get the current unicode character code */
            wchar_t uni = ps->nextch();

            /* 
             *   determine if the character is mappable to the local
             *   character set 
             */
            int mappable = G_cmap_to_ui->is_mappable(uni);
            
            /* 
             *   If there's no mapping, insert a '\u' sequence for the
             *   character.  If the character is in the non-printable control
             *   character range, use a backslash representation as well.  If
             *   the character is a backslash or single quote, also represent
             *   it as a backslash sequence, since it would need to be quoted
             *   in a string submitted to the compiler.  
             */
            if (uni < 32 || uni == '\\' || uni == '\'')
            {
                char esc;
                unsigned int dig;
                
                switch(uni)
                {
                case 9:
                    /* tab - '\t' */
                    esc = 't';
                    
                do_escape:
                    /* add the escape sequence */
                    retlen += 2;
                    if (dstlen > 2)
                    {
                        *dst++ = '\\';
                        *dst++ = esc;
                        dstlen -= 2;
                    }
                    break;
                    
                case 10:
                    /* newline - '\n' */
                    esc = 'n';
                    goto do_escape;
                    
                case 13:
                    /* return - '\r' */
                    esc = 'r';
                    goto do_escape;
                    
                case 0x000F:
                    /* caps - '\^' */
                    esc = '^';
                    goto do_escape;
                    
                case 0x000E:
                    /* uncaps - '\v' */
                    esc = 'v';
                    goto do_escape;
                    
                case 0x000B:
                    /* blank - '\b' */
                    esc = 'b';
                    goto do_escape;
                    
                case 0x0015:
                    /* quoted space - '\ ' */
                    esc = ' ';
                    goto do_escape;
                    
                case '\\':
                case '\'':
                    esc = (char)uni;
                    goto do_escape;
                    
                default:
                    /* represent anything else as a hex sequence */
                    retlen += 4;
                    if (dstlen > 4)
                    {
                        /* add the backslash and 'x' */
                        *dst++ = '\\';
                        *dst++ = 'x';
                        
                        /* add the the first hex digit */
                        dig = (uni >> 4) & 0xF;
                        *dst++ = dig + (dig < 10 ? '0' : 'A' - 10);
                        
                        /* add the second hex digit */
                        dig = (uni & 0xF);
                        *dst++ = dig + (dig < 10 ? '0' : 'A' - 10);
                        
                        /* consume the space */
                        dstlen -= 4;
                    }
                    break;
                }
            }
            else if (!mappable)
            {
                int i;
                unsigned int c;
                
                /* 
                 *   the '\u' sequence requires six bytes (backslash, 'u',
                 *   and the four hex digits of the character code) 
                 */
                retlen += 6;
                if (dstlen > 6)
                {
                    /* add the '\u' */
                    *dst++ = '\\';
                    *dst++ = 'u';
                    
                    /* add the four hex digits */
                    for (c = (unsigned int)uni, i = 0 ; i < 4 ; ++i)
                    {
                        unsigned int dig;
                        
                        /* get the current most significant digit's value */
                        dig = (c >> 12) & 0xf;
                        
                        /* generate the next hex digit */
                        *dst++ = dig + (dig < 10 ? '0' : 'A' - 10);
                        
                        /* shift up so the next digit is in current */
                        c <<= 4;
                    }
                    
                    /* consume the space */
                    dstlen -= 6;
                }
            }
            else
            {
                char buf[20];
                size_t xlat_len;
                size_t copy_len;
                
                /* get the translation of this character */
                xlat_len = G_cmap_to_ui->map_char(uni, buf, sizeof(buf));
                copy_len = (dstlen <= 1 ? 0 :
                            xlat_len < dstlen ? xlat_len :
                            dstlen - 1);
                
                /* copy the translation */
                memcpy(dst, buf, copy_len);
                
                /* advance past the space we've used */
                retlen += xlat_len;
                dst += copy_len;
                dstlen -= copy_len;
            }
        }
        
        /* add the close quote */
        retlen += 1;
        if (dstlen > 1)
            *dst++ = '\'';
        
        /* add the null terminator */
        if (dstlen > 0)
            *dst++ = '\0';

        /* we're done - don't bother with the normal copying */
        return retlen;

    case VM_LIST:
    format_list:
        {
            /* get the element count */
            size_t cnt = val->ll_length(vmg0_);

            /* add the open bracket */
            retlen += 1;
            if (dstlen > 1)
            {
                *dst++ = '[';
                --dstlen;
            }

            /* scan through the elements (using a 1-based counter) */
            for (size_t i = 1 ; i <= cnt ; ++i)
            {
                /* get this element */
                vm_val_t ele;
                val->ll_index(vmg_ &ele, i);

                /* format the element */
                size_t cur_len = format_val(vmg_ dst, dstlen, &ele);
                retlen += cur_len;

                /* consume the space */
                if (dst != 0 && dstlen > 0)
                {
                    if (cur_len < dstlen)
                    {
                        dstlen -= cur_len;
                        dst += cur_len;
                    }
                    else
                    {
                        dst += dstlen - 1;
                        dstlen = 1;
                    }
                }

                /* add a comma between elements if possible */
                if (i != cnt)
                {
                    retlen += 1;
                    if (dstlen > 1)
                    {
                        *dst++ = ',';
                        --dstlen;
                    }
                }
            }

            /* add the closing bracket if possible */
            retlen += 1;
            if (dstlen > 1)
            {
                *dst++ = ']';
                --dstlen;
            }

            /* add the null terminator */
            if (dstlen > 0)
                *dst = '\0';
        }

        /* we're done - don't bother with normal copying */
        return retlen;

    case VM_FUNCPTR:
        /* function pointer - get the function name */
        p = funcaddr_to_sym(val->val.ofs);

        /* if there's no symbol, use the numeric function address */
        if (p == 0)
        {
            sprintf(buf, "function#%08lx", (unsigned long)val->val.ofs);
            p = buf;
        }
        break;

    default:
        p = "?";
        break;
    }

    /* copy as much of the value as possible into the buffer */
    retlen = strlen(p);
    lib_strcpy(dst, dstlen, p);

    /* make sure the buffer is null-terminated */
    if (dstlen > 0)
        dst[dstlen - 1] = '\0';

    /* return the total length required */
    return retlen;
}

/* ------------------------------------------------------------------------ */
/*
 *   Format a value as a special 
 */
void CVmDebug::format_special(VMG_ char *buf, size_t buflen,
                              const vm_val_t *val)
{
    /* we need a fairly fixed size for the buffer: "#__type xxxxxxxx" */
    if (buflen < 25)
    {
        buf[0] = '\0';
        return;
    }
    
    switch (val->typ)
    {
    case VM_NIL:
        strcpy(buf, "nil");
        return;

    case VM_TRUE:
        strcpy(buf, "true");
        return;

    case VM_OBJ:
        sprintf(buf, "#__obj %lu", (unsigned long)val->val.obj);
        return;

    case VM_PROP:
        sprintf(buf, "#__prop %u", (unsigned int)val->val.prop);
        break;

    case VM_INT:
        sprintf(buf, "%ld", (long)val->val.intval);
        return;

    case VM_SSTRING:
        sprintf(buf, "#__sstr %lu", (unsigned long)val->val.ofs);
        return;

    case VM_LIST:
        sprintf(buf, "#__list %lu", (unsigned long)val->val.ofs);
        return;

    case VM_FUNCPTR:
        sprintf(buf, "#__func %lu", (unsigned long)val->val.ofs);
        return;

    case VM_ENUM:
        sprintf(buf, "#__enum %lu", (unsigned long)val->val.enumval);
        return;

    case VM_BIFPTR:
        sprintf(buf, "#__bifptr %u %u",
                val->val.bifptr.set_idx, val->val.bifptr.func_idx);
        return;

    default:
        strcpy(buf, "#__invalid");
        return;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Callback context structure for aggregation enumerations
 */
struct eval_enum_cb_ctx
{
    /* the UI callback function to invoke and its own context */
    void (*ui_cb)(void *, const char *, int, const char *);
    void *ui_cb_ctx;

    /* the original object whose properties we're enumerating */
    vm_obj_id_t obj;

    /* debugger object */
    CVmDebug *dbg;
};

/*
 *   Lookup table enumeration callback for expression evaluation
 */
static void enum_lookup_keys_cb(
    VMG_ const vm_val_t *key, const vm_val_t *val, void *ctx0)
{
    /* get our context, properly cast */
    eval_enum_cb_ctx *ctx = (eval_enum_cb_ctx *)ctx0;

    /* format the key value */
    char keybuf[256];
    keybuf[0] = '[';
    ctx->dbg->format_val(vmg_ keybuf+1, sizeof(keybuf)-3, key);
    strcat(keybuf, "]");

    /* 
     *   For the relationship, generate the special __type# notation for the
     *   key.  This is a special debugger-only compact format with full
     *   round-trip fidelity for all value types.  Some values have no
     *   ordinary source code representation, such as objects without symbol
     *   names; the __type# notation ensures that every value is
     *   representable in source.  Note that we also use the "!" notation to
     *   let the debugger know that this is the whole child expression rather
     *   than an operator that we join with the key expression.  
     */
    char relbuf[128], valbuf[120];
    ctx->dbg->format_special(vmg_ valbuf, sizeof(valbuf), key);
    sprintf(relbuf, "![%s]", valbuf);
    
    /* send the formatted values to the debugger UI */
    (*ctx->ui_cb)(ctx->ui_cb_ctx, keybuf, strlen(keybuf), relbuf);
}

/*
 *   Object property enumeration callback for evaluating an object-valued
 *   expression.  
 */
static void enum_props_eval_cb(VMG_ void *ctx0,
                               vm_obj_id_t self, vm_prop_id_t prop,
                               const vm_val_t *val)
{
    /* get our context, properly cast */
    eval_enum_cb_ctx *ctx = (eval_enum_cb_ctx *)ctx0;

    /* 
     *   The property enumerator tells us about all of the properties
     *   throughout the entire inheritance tree.  Make sure this one isn't
     *   overridden - if it is, we'll already have shown (or suppressed
     *   showing) the overriding copy of the property.  Since overridden
     *   properties can't be seen in the actual object, we don't want to show
     *   them here.  
     */
    vm_val_t ov_val;
    vm_obj_id_t src_obj;
    if (vm_objp(vmg_ ctx->obj)
           ->get_prop(vmg_ prop, &ov_val, ctx->obj, &src_obj, 0)
        && src_obj != self)
    {
        /* 
         *   we found the property, but it was defined in a different object
         *   than the object whose properties we're enumerating - this must
         *   be an inherited version of the property which the object
         *   overrides, so we don't want to show it among the original
         *   object's properties 
         */
        return;
    }

    /* get the name of this property */
    const char *prop_name = ctx->dbg->propid_to_sym(prop);

    /* 
     *   if we couldn't get a name for the property, don't bother invoking
     *   the UI callback, since it won't be able to use the property
     *   anyway 
     */
    if (prop_name == 0)
        return;

    /* ignore methods and properties containing self-printing strings */
    switch(val->typ)
    {
    case VM_NIL:
    case VM_TRUE:
    case VM_OBJ:
    case VM_PROP:
    case VM_INT:
    case VM_ENUM:
    case VM_SSTRING:
    case VM_LIST:
    case VM_FUNCPTR:
    case VM_BIFPTR:
        /* 
         *   these types are all valid for enumeration - invoke the UI
         *   callback
         */
        (*ctx->ui_cb)(ctx->ui_cb_ctx, prop_name, strlen(prop_name), ".");
        break;

    default:
        /* do not enumerate any other types */
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Evaluate an expression 
 */
int CVmDebug::eval_expr(VMG_ char *result, size_t resultlen,
                        const char *expr,
                        int level, int *is_lval, int *is_openable,
                        void (*aggcb)(void *, const char *,
                                      int, const char *),
                        void *aggctx, int speculative)
{
    vm_obj_id_t code_obj;
    vm_val_t expr_val;
    CVmDbgSymtab local_symtab;
    vmdbg_step_save_t old_step;
    vm_obj_id_t self_obj;
    vm_obj_id_t orig_target_obj;
    vm_obj_id_t defining_obj;
    vm_prop_id_t target_prop;
    vmrun_save_ctx run_ctx;
    
    /* presume it won't be an lvalue or openable */
    if (is_lval != 0)
        *is_lval = FALSE;
    if (is_openable != 0)
        *is_openable = FALSE;

    /* we don't have a result string yet */
    if (result != 0 && resultlen > 0)
        *result = '\0';

    /* get information on the indicated stack level */
    {
        CVmFuncPtr func_ptr;
        CVmDbgLinePtr line_ptr;
        CVmDbgTablePtr dbg_ptr;
        const uchar *stm_start, *stm_end;

        if (get_stack_level_info(vmg_ level, &func_ptr, &line_ptr,
                                 &stm_start, &stm_end)
            || !func_ptr.set_dbg_ptr(&dbg_ptr))
            return 1;
    
        /* set up our local symbol table interface for this context */
        local_symtab.init(vmg_ &dbg_ptr, line_ptr.get_frame_id(), level);
    }

    /* get the 'self' object for the selected level */
    self_obj = G_interpreter->get_self_at_level(vmg_ level);

    /* get the target property for the level */
    target_prop = G_interpreter->get_target_prop_at_level(vmg_ level);

    /* get the original target object and defining object at the level */
    orig_target_obj = G_interpreter->get_orig_target_obj_at_level(vmg_ level);
    defining_obj = G_interpreter->get_defining_obj_at_level(vmg_ level);

    /* compile the expression */
    CVmDynCompResults comp_results;
    compile_expr(vmg_ expr, level, &local_symtab,
                 self_obj != VM_INVALID_OBJ
                 && target_prop != VM_INVALID_PROP,
                 speculative, is_lval, &code_obj, &comp_results);

    /* if we got a compilation error, return failure */
    if (comp_results.err != 0)
    {
        /* copy the result message, if desired */
        if (comp_results.msgbuf != 0)
        {
            /* copy the message to the caller's buffer (if there is one) */
            if (result != 0)
                lib_strcpy(result, resultlen, comp_results.msgbuf);
        }
        
        /* return the error */
        return comp_results.err;
    }

    /* note the pre-call stack depth */
    G_interpreter->save_context(vmg_ &run_ctx);

    /* set up for the recursive execution */
    prepare_for_eval(&old_step);

    /* push the code object onto the stack for gc protection */
    G_stk->push()->set_obj(code_obj);

    /* execute the code in a protected block */
    int err = 0;
    err_try
    {
        /* set up the invocation frame */
        vm_val_t *fp = G_stk->push(5);
        (fp++)->set_propid(target_prop);
        (fp++)->set_obj_or_nil(orig_target_obj);
        (fp++)->set_obj_or_nil(defining_obj);
        (fp++)->set_obj_or_nil(self_obj);
        (fp++)->set_obj(code_obj);
        
        /* execute the code in a recursive call to the VM */
        vm_rcdesc rc("dbg eval");
        G_interpreter->call_func_ptr_fr(vmg_ G_stk->get(0), 0, &rc, 0);
    }
    err_catch(exc)
    {
        /* note the error */
        err = exc->get_error_code();

        /* 
         *   if it's "unhandled exception," get the message from the
         *   exception object itself; otherwise, format the message for
         *   the error 
         */
        if (err == VMERR_UNHANDLED_EXC)
        {
            /* get the message from the exception */
            size_t msg_len;
            const char *msg = CVmRun::get_exc_message(vmg_ exc, &msg_len);
            if (msg == 0)
                msg = "Unhandled program exception";

            /* limit the size to the caller's buffer size */
            t3sprintf(result, resultlen, "error: %.*s", (int)msg_len, msg);
        }
        else
        {
            /* format the VM error message into the caller's result buffer */
            const char *msg = err_get_msg(
                vm_messages, vm_message_count, err, FALSE);
            char *msgf = err_format_msg(msg, exc);

            /* format the full copy for the caller */
            t3sprintf(result, resultlen, "error: %s", msgf);

            /* free the base message */
            t3free(msgf);
        }
    }
    err_end;

    /* discard the code object */
    G_stk->discard();

    /* restore the original execution mode */
    restore_from_eval(&old_step);

    /* restore the interpreter context */
    G_interpreter->restore_context(vmg_ &run_ctx);

    /* get the return value, if any */
    if (err == 0)
    {
        /* get the value from R0 */
        expr_val = *G_interpreter->get_r0();

        /* leave the value on the stack for gc protection */
        G_stk->push(&expr_val);

        /* format the value into an allocated buffer */
        format_val(vmg_ result, resultlen, &expr_val);
    }
    else
    {
        /* return failure */
        return 1;
    }

    /* if an aggregation callback is provided, use it */
    if (aggcb != 0)
    {
        size_t cnt;
        size_t i;
        
        /* determine if the object has contents to be iterated */
        switch(expr_val.typ)
        {
        case VM_LIST:
        agg_list:
            /* get the element count */
            cnt = expr_val.ll_length(vmg0_);

            /* iterate through the elements */
            for (i = 1 ; i <= cnt ; ++i)
            {
                /* format an index operator for this index */
                char idxbuf[30];
                sprintf(idxbuf, "[%u]", (unsigned int)i);

                /* invoke the callback with this subitem */
                (*aggcb)(aggctx, idxbuf, strlen(idxbuf), "");
            }
            break;

        case VM_OBJ:
            /* if it's a list object, use the list contents */
            if (vm_objp(vmg_ expr_val.val.obj)->get_as_list() != 0
                || (CVmObjVector::is_vector_obj(vmg_ expr_val.val.obj)
                    && !CVmObjAnonFn::is_anonfn_obj(vmg_ expr_val.val.obj)))
                goto agg_list;

            /* set up our callback context in case we need it */
            eval_enum_cb_ctx cb_ctx;
            cb_ctx.ui_cb = aggcb;
            cb_ctx.ui_cb_ctx = aggctx;
            cb_ctx.dbg = this;
            cb_ctx.obj = expr_val.val.obj;

            /* handle lookup tables specially */
            if (CVmObjLookupTable::is_lookup_table_obj(vmg_ expr_val.val.obj))
            {
                /* get the lookup table object, properly cast */
                CVmObjLookupTable *lt =
                    (CVmObjLookupTable *)vm_objp(vmg_ expr_val.val.obj);
                
                /* enumerate the table's keys */
                lt->for_each(vmg_ enum_lookup_keys_cb, &cb_ctx);

                /* if it has a default value, include it in the listing */
                vm_val_t defval;
                lt->get_default_val(&defval);
                if (defval.typ != VM_NIL)
                    (*aggcb)(aggctx, "*", 3, "!.getDefaultValue()");

                /* done */
                break;
            }

            /* determine if the object provides a property list */
            if (vm_objp(vmg_ expr_val.val.obj)->provides_props(vmg0_))
            {
                /* enumerate the properties */
                vm_objp(vmg_ expr_val.val.obj)
                    ->enum_props(vmg_ expr_val.val.obj, &enum_props_eval_cb,
                                 &cb_ctx);
            }
            break;

        default:
            /* other types don't have sub-parts to iterate */
            break;
        }
    }

    /* check the openable status if the caller wants to know */
    if (is_openable != 0)
    {
        /* 
         *   check to see if it's openable - it is if it's a TADS object
         *   or a list 
         */
        switch(expr_val.typ)
        {
        case VM_LIST:
            /* lists are always openable */
            *is_openable = TRUE;
            break;
            
        case VM_OBJ:
            /* 
             *   object - if it's a list or TADS object, it's openable;
             *   otherwise it's not 
             */
            if (vm_objp(vmg_ expr_val.val.obj)->get_as_list() != 0
                || (CVmObjVector::is_vector_obj(vmg_ expr_val.val.obj)
                    && !CVmObjAnonFn::is_anonfn_obj(vmg_ expr_val.val.obj))
                || CVmObjLookupTable::is_lookup_table_obj(vmg_ expr_val.val.obj)
                || vm_objp(vmg_ expr_val.val.obj)->provides_props(vmg0_))
                *is_openable = TRUE;
            else
                *is_openable = FALSE;
            break;
            
        default:
            /* other types aren't openable */
            *is_openable = FALSE;
            break;
        }
    }

    /* discard the gc protection */
    G_stk->discard();

    /* success */
    return 0;
}


/*
 *   Compile an expression 
 */
void CVmDebug::compile_expr(VMG_ const char *expr,
                            int level, CVmDbgSymtab *local_symtab,
                            int self_valid, int speculative,
                            int *is_lval, vm_obj_id_t *code_obj,
                            CVmDynCompResults *results)
{
    /* set up our compilation mode descriptors */
    tcpn_dyncomp_info adjust_info(TRUE, speculative, level);
    CVmDynCompDebug dbg_info(local_symtab, adjust_info, self_valid);

    /* compile the code via the dynamic compiler */
    *code_obj = CVmDynamicCompiler::get(vmg0_)->compile(
        vmg_ FALSE, VM_INVALID_OBJ, VM_INVALID_OBJ, VM_INVALID_OBJ, 0,
        expr, strlen(expr), DCModeExpression, &dbg_info, results);

    /* exclude debugger objects from save/restore/undo */
    if (*code_obj != VM_INVALID_OBJ)
        G_obj_table->set_obj_transient(*code_obj);

    /* pass the lvalue information back to the caller */
    if (is_lval != 0)
        *is_lval = dbg_info.is_lval;
}

/* ------------------------------------------------------------------------ */
/*
 *   Set step-over mode 
 */
void CVmDebug::set_step_over(VMG0_)
{
    /* set the mode flags */
    single_step_ = TRUE;
    step_in_ = FALSE;
    step_out_ = FALSE;

    /* we're in run mode again */
    break_event_->reset();

    /* 
     *   note the current stack frame depth - we won't stop until the
     *   stack frame is back at this same depth, or at an enclosing level 
     */
    step_frame_depth_ = G_interpreter->get_frame_depth(vmg0_);
}

/*
 *   Set step-out mode 
 */
void CVmDebug::set_step_out(VMG0_)
{
    /* set the mode flags for stepping over source lines */
    single_step_ = TRUE;
    step_in_ = FALSE;
    step_out_ = TRUE;

    /* we're in run mode again */
    break_event_->reset();

    /*
     *   Keep stepping until we're at an enclosing stack frame level - to
     *   do this, note the current frame depth, but decrement it, since we
     *   don't want to stop again until the frame depth is lower than it
     *   is now.
     */
    step_frame_depth_ = G_interpreter->get_frame_depth(vmg0_) - 1;
}


/* ------------------------------------------------------------------------ */
/*
 *   Note a RETURN for single-step purposes 
 */
void CVmDebug::step_return(VMG0_)
{
    /*
     *   Note the frame depth for single-stepping purposes.  If we're in STEP
     *   OVER mode, and we're now in a frame where we can once again stop for
     *   stepping, clear the step-over flag.  Don't actually stop now; that
     *   can wait until the next steppable instruction.  But do note that we
     *   *should* stop at the next steppable instruction.
     *   
     *   This check usually isn't required, since in most cases we'd match
     *   the frame-depth criterion anyway at the next instruction in the
     *   caller.  However, if the caller isn't steppable (e.g., it's native
     *   code), and the caller turns around and calls another routine, we
     *   could incorrectly think that the new routine was being called from
     *   the routine that's returning.  This can happen if the new routine
     *   has more arguments than the old one, or the caller pushes some
     *   temporary state onto the stack.  In either case, the frame depth in
     *   the new routine will look deeper than it did in the old routine, so
     *   we'll think it's a callee of the old routine rather than a peer.  By
     *   explicitly tracing through the return, we ensure that we realize
     *   that we should resume stepping in the new routine.  
     */
    if (single_step_ && !step_in_
        && G_interpreter->get_frame_depth(vmg0_) < step_frame_depth_)
    {
        /* we're in a caller - stop at the next steppable instruction */
        if (step_over_bp_)
            orig_step_in_ = TRUE;
        else
            step_in_ = TRUE;

        /* we're in run mode again */
        break_event_->reset();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Synchronize our internal memory of the last execution point 
 */
void CVmDebug::sync_exec_pos(VMG_ const uchar *pc_ptr, const uchar *entry)
{
    CVmFuncPtr func_ptr;
    CVmDbgLinePtr line_ptr;
    const uchar *stm_start, *stm_end;

    /* 
     *   if we're already in the debugger, don't change anything - this must
     *   be a recursive invocation due to expression evaluation 
     */
    if (in_debugger_)
        return;

    /* set up a pointer to the current function's header */
    G_interpreter->set_current_func_ptr(vmg_ &func_ptr);

    /* get the byte-code offset of this instruction */
    pc_ = pc_ptr;
    entry_ = entry;

    /* get the boundaries of the current source code statement */
    if (CVmRun::get_stm_bounds(vmg_ &func_ptr, pc_ - entry_, &line_ptr,
                               &stm_start, &stm_end))
    {
        /* remember the current function pointer */
        func_ptr_.copy_from(&func_ptr);

        /* remember the new line pointer */
        cur_stm_line_.copy_from(&line_ptr);

        /* remember the method start offset */
        entry_ = entry;

        /* remember the statement bounds */
        cur_stm_start_ = stm_start;
        cur_stm_end_ = stm_end;

        /* ask the function pointer to set up the debug table pointer */
        dbg_ptr_valid_ = func_ptr_.set_dbg_ptr(&dbg_ptr_);
    }
    else
    {
        /* the statement has no valid bounds; clear the debug info */
        entry_ = 0;
        cur_stm_start_ = 0;
        cur_stm_end_ = 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Single-step interrupt 
 */
void CVmDebug::step(VMG_ const uchar **pc_ptr, const uchar *entry,
                    int hit_bp, int error_code)
{
    int line_ptr_valid;
    CVmFuncPtr func_ptr;
    CVmDbgLinePtr line_ptr;
    int bpnum;
    int hit_global_bp;
    int stop_for_step;
    const uchar *stm_start, *stm_end;
    int trace_over_bp;
    vm_val_t orig_r0;
    int released_stack_reserve = FALSE;

    /*
     *   If we're stepping over a breakpoint, put the breakpoint back into
     *   effect and restore the actual execution mode 
     */
    if (step_over_bp_)
    {
        /* no longer stepping over a breakpoint */
        step_over_bp_ = FALSE;

        /* restore the original execution mode */
        single_step_ = orig_single_step_;
        step_in_ = orig_step_in_;
        step_out_ = orig_step_out_;

        /* restore the breakpoint */
        step_over_bp_bp_->set_bp_instr(vmg_ TRUE, TRUE);
    }

    /* if we're already in the debugger, don't re-enter */
    if (in_debugger_)
        return;

    /* note that we're in the debugger so that we don't recurse into here */
    in_debugger_ = TRUE;

    /* 
     *   if this is a stack overflow error, release the debugger reserve, so
     *   that we can look at expressions and otherwise go about our business
     *   without encountering additional stack overflows 
     */
    if (error_code == VMERR_STACK_OVERFLOW)
        released_stack_reserve = G_stk->release_reserve();

    /* 
     *   Remember the value of R0, so we can restore it when we leave; also
     *   push it onto the stack, so that it's protected against any garbage
     *   collection that occurs while the debugger has control.  
     */
    orig_r0 = *G_interpreter->get_r0();
    G_stk->push(&orig_r0);

    /* presume we will not trace over a breakpoint at exit */
    trace_over_bp = FALSE;

    /* get the byte-code offset of this instruction */
    pc_ = (pc_ptr != 0 ? *pc_ptr : 0);
    entry_ = entry;

    /* 
     *   If we're in single-step mode, assume we'll stop for stepping.  If
     *   we're stopping for an error, also set single-step mode for now. 
     */
    stop_for_step = single_step_ || (error_code != 0);

    /* if we hit a breakpoint, find it */
    if (hit_bp)
    {
        CVmDebugBp *bp;

        /* find the breakpoint */
        bp = find_bp(pc_);

        /* get the breakpoint number */
        bpnum = (bp == 0 ? 0 : (int)(bp - bp_) + 1);

        /* 
         *   If the breakpoint has a condition attached to it, stop only
         *   if the condition is true.  
         */
        if (bp != 0 && bp->has_condition() && !bp->eval_cond(vmg0_))
        {
            /* 
             *   The condition is false - this means we must ignore the
             *   breakpoint; forget that we hit the breakpoint.  (We can't
             *   just decide to keep running now because there are
             *   numerous other reasons we might want to stop now, and we
             *   must consider the other possibilities before we jump back
             *   into the program.)  
             */
            hit_bp = FALSE;
            
            /* we don't have a valid breakpoint number after all */
            bpnum = 0;

            /*
             *   Since we're skipping this breakpoint, we need to trace
             *   over it to continue running. 
             */
            trace_over_bp = TRUE;
        }
    }
    else
    {
        /* we don't have a valid breakpoint */
        bpnum = 0;
    }

    /* presume we won't hit a global breakpoint */
    hit_global_bp = FALSE;

    /*
     *   If there are any global breakpoints, and we didn't hit a code
     *   breakpoint, check for a hit.  Don't bother with this if we've hit
     *   a real breakpoint, since we'll certainly stop in that case.
     *   Also, don't bother checking global breakpoints if we're in native
     *   code (i.e., pc_ptr == 0), since we can't stop right now if we
     *   are.  
     */
    if (!hit_bp && pc_ptr != 0 && global_bp_cnt_ != 0)
    {
        CVmDebugBp *bp;
        size_t i;
        
        /* scan all of the breakpoints and look for a global breakpoint */
        for (i = 0, bp = bp_ ; i < VMDBG_BP_MAX ; ++i, ++bp)
        {
            /* 
             *   if this breakpoint is enabled and global, evaluate its
             *   condition 
             */
            if (bp->is_in_use() && !bp->is_disabled() && bp->is_global())
            {
                /* evaluate the condition */
                if (bp->eval_cond(vmg0_))
                {
                    /* 
                     *   we've hit a global breakpoint - note it, and
                     *   compute the breakpoint number (it's just the
                     *   breakpoint index adjusted to a 1-based index) 
                     */
                    hit_global_bp = TRUE;
                    bpnum = (int)(i + 1);

                    /* 
                     *   If this is a stop-when-true condition, automatically
                     *   disable the breakpoint.  Once a global
                     *   stop-when-true breakpoint hits, its condition will
                     *   probably remain true for some time, and it would be
                     *   pointless to have it keep hitting over and over
                     *   again now.  Note that this isn't necessary for
                     *   stop-on-change breakpoints, as the fact that the
                     *   value just changed doesn't tell us anything about
                     *   the likelihood of future changes.  
                     */
                    if (!bp->stop_on_change())
                        bp->set_disabled(TRUE);

                    /* 
                     *   there's no need to look further - we only need to
                     *   have one global breakpoint hit in order to stop 
                     */
                    break;
                }
            }
        }
    }

    /* 
     *   Check to see if we're within the same source code statement that
     *   we were in the last time we were in this routine.  If so, we
     *   already have most of the information we need about the execution
     *   location, so we can avoid a little set-up work.  If we're not in
     *   the same statement any more, we have to figure out where we are.
     *   Note that we will also break if we're at a different stack level
     *   than we were last time, because in this case we're stepping out
     *   of a recursive invocation.  
     */
    if (stop_for_step
        && pc_ >= cur_stm_start_ && pc_ < cur_stm_end_
        && G_interpreter->get_frame_depth(vmg0_) == step_frame_depth_)
    {
        /* 
         *   No matter what source-stepping mode we're in, we never stop
         *   twice consecutively at the same source statement.  Make a
         *   note that we don't need to stop here.  However, don't just
         *   return now, since we might need to stop for a breakpoint or a
         *   global breakpoint.  
         */
        stop_for_step = FALSE;
    }

    /*
     *   If we're in step-over mode, there's no need to stop if we're
     *   executing at any stack level nested within the original stack frame.
     *   If this is the case, we can simply return now.  Of course, if we're
     *   stopping because we hit an error, don't worry about our step mode.  
     */
    if (stop_for_step
        && error_code == 0
        && single_step_ && !step_in_
        && G_interpreter->get_frame_depth(vmg0_) > step_frame_depth_)
    {
        /* we don't need to stop for stepping */
        stop_for_step = FALSE;
    }

    /* figure out where we're tracing, if we're in byte code */
    if (pc_ptr != 0)
    {
        /* set up a pointer to the current function's header */
        G_interpreter->set_current_func_ptr(vmg_ &func_ptr);
        
        /* get the boundaries of the current source code statement */
        line_ptr_valid =
            CVmRun::get_stm_bounds(vmg_ &func_ptr, pc_ - entry_, &line_ptr,
                                   &stm_start, &stm_end);
    }
    else
    {
        /* we're in native code, so there is no valid byte code location */
        line_ptr_valid = FALSE;
        stm_start = stm_end = 0;
    }

    /* 
     *   If we're within the same source line and at the same stack depth
     *   that we were at last time - even if the apparent byte-code location
     *   has us in a different line - don't stop for a single-step
     *   operation.  The same source line can generate byte code at
     *   different places in certain types of complex statements, and we
     *   don't want to confuse the user by stopping twice in a row at the
     *   same line (thus showing no apparent change in the execution
     *   location) in such cases.
     *   
     *   However, if we're stopping due to a run-time error, do stop even if
     *   we're in the same location - it just means we got another error at
     *   the same place.  
     */
    if (stop_for_step
        && error_code == 0
        && G_interpreter->get_frame_depth(vmg0_) == step_frame_depth_
        && line_ptr_valid
        && cur_stm_start_ != 0
        && line_ptr.get_source_id() == cur_stm_line_.get_source_id()
        && line_ptr.get_source_line() == cur_stm_line_.get_source_line())
    {
        /* note that we don't want to stop for single-step mode */
        stop_for_step = FALSE;
    }

    /*
     *   If we're in native code and we decided that we should stop,
     *   remember the current code location as the last stop location.
     *   Even though we can't actually stop here, we want to act as though
     *   we did, so that next time we step through actual byte code we'll
     *   notice that we are at a different location than we were for the
     *   last stop. 
     */
    if (pc_ptr == 0 && stop_for_step)
    {
        /* 
         *   set the current statement bounds to invalid, to ensure that
         *   we will notice we are in a different location for the next
         *   valid byte code location 
         */
        cur_stm_start_ = cur_stm_end_ = 0;
    }

    /*
     *   If we're stopping for any reason (single-stepping, breakpoint, or
     *   global breakpoint), enter the interactive debugger user
     *   interface.  In any case, only stop if we have a valid line
     *   pointer, since we can't do anything at a location without a valid
     *   source line.  
     *   
     *   Note that we can never take control when we're stepping through
     *   native code (i.e., pc_ptr == 0).  We're called for native code
     *   execution only to advise us of the change in stack levels so that
     *   we can properly track step in/over/out modes properly through
     *   native code traversal.  
     */
    if (line_ptr_valid && (stop_for_step || hit_bp || hit_global_bp))
    {
        /* remember the current function pointer */
        func_ptr_.copy_from(&func_ptr);

        /* remember the current frame depth */
        step_frame_depth_ = G_interpreter->get_frame_depth(vmg0_);

        /* remember the new line pointer */
        cur_stm_line_.copy_from(&line_ptr);

        /* remember the method start offset */
        entry_ = entry;

        /* remember the statement bounds */
        cur_stm_start_ = stm_start;
        cur_stm_end_ = stm_end;

        /* ask the function pointer to set up the debug table pointer */
        dbg_ptr_valid_ = func_ptr_.set_dbg_ptr(&dbg_ptr_);

        /* 
         *   remove all breakpoints from the code while we have control,
         *   so that the code is all the original instructions while the
         *   user is looking at it 
         */
        suspend_all_bps(vmg0_);

        /* 
         *   call the UI interactive command loop - this won't return
         *   until the user tells us to resume execution 
         */
        const uchar *pc = pc_;
        CVmDebugUI::cmd_loop(vmg_ bpnum, error_code, &pc);

        /* restore breakpoints now that we're resuming execution */
        restore_all_bps(vmg0_);

        /* update our execution pointer in case the UI moved it */
        pc_ = pc;

        /* update the caller's execution pointer as well */
        *pc_ptr = pc;

        /* 
         *   get the boundaries of the current statement, in case it
         *   changed - we need this for the next time we enter the
         *   debugger, so we can tell (if we're stepping) whether we've
         *   left the most recent statement or not 
         */
        if (!CVmRun::get_stm_bounds(vmg_ &func_ptr_, pc - entry,
                                   &cur_stm_line_,
                                   &cur_stm_start_, &cur_stm_end_))
        {
            /* we have no information for this statement */
            cur_stm_start_ = cur_stm_end_ = 0;
        }

        /* 
         *   note that we must trace over any breakpoint at this new
         *   location, since we want to execute the original instruction
         *   rather than just stopping again 
         */
        trace_over_bp = TRUE;
    }

    /* 
     *   if we released the stack reserve, put it back in reserve now that
     *   we're leaving the debugger - the reserve is for the debugger's use
     *   only, so we don't need it once we resume execution 
     */
    if (released_stack_reserve)
        G_stk->recover_reserve();

    /* note that we're leaving the debugger */
    in_debugger_ = FALSE;

    /*
     *   If appropriate, check the current code location for a breakpoint;
     *   if there's a breakpoint, we need to go through a couple of extra
     *   steps to resume execution so that we don't jump right back into
     *   the debugger.
     *   
     *   The problem is that if we go or step after hitting a breakpoint,
     *   and we were to simply leave the breakpoint in effect, we'd just
     *   hit the same breakpoint again.  What we actually want to do is
     *   execute the original instruction.  To do this, we must first
     *   remove the breakpoint, restoring the original instruction, then
     *   single-step the instruction, then, when the debugger regains
     *   control after the single-step, put the breakpoint back.  Finally,
     *   we can resume with whatever kind of stepping we were going to do
     *   in the first place.  
     */
    if (trace_over_bp && **pc_ptr == (uchar)OPC_BP)
    {
        CVmDebugBp *bp;
        
        /* find the breakpoint */
        bp = find_bp(pc_);
        
        /* if we found the breakpoint, remove it temporarily */
        if (bp != 0)
        {
            /* restore the original instruction */
            bp->set_bp_instr(vmg_ FALSE, TRUE);
            
            /* note that we're in step-over-bp mode */
            step_over_bp_ = TRUE;
            
            /* remember the breakpoint we're stepping over */
            step_over_bp_bp_ = bp;
            
            /* remember the original execution mode */
            orig_single_step_ = single_step_;
            orig_step_in_ = step_in_;
            orig_step_out_ = step_out_;
            
            /* 
             *   temporary set single-step mode, so that we get control
             *   immediately after executing this single instruction 
             */
            single_step_ = TRUE;
            step_in_ = TRUE;
            step_out_ = FALSE;
        }
    }

    /* 
     *   restore the original value of R0, and discard the copy we pushed
     *   onto the stack for protection against garbage collection 
     */
    *G_interpreter->get_r0() = orig_r0;
    G_stk->discard();
}

/* ------------------------------------------------------------------------ */
/*
 *   Suspend all breakpoints - removes all BP instructions from the code
 *   and restores the original instructions 
 */
void CVmDebug::suspend_all_bps(VMG0_)
{
    CVmDebugBp *bp;
    size_t i;

    /* loop over all breakpoints */
    for (i = 0, bp = bp_ ; i < VMDBG_BP_MAX ; ++i, ++bp)
    {
        /* 
         *   if this breakpoint is active, restore its original
         *   instruction (since this routine sets up for switching in and
         *   out of the debugger, force the instruction update even if
         *   we're in debugger mode)
         */
        if (bp->is_in_use() && !bp->is_disabled())
            bp->set_bp_instr(vmg_ FALSE, TRUE);
    }
}

/*
 *   Restore all breakpoints - restores all BP instructions removed by
 *   suspend_all_bps() 
 */
void CVmDebug::restore_all_bps(VMG0_)
{
    CVmDebugBp *bp;
    size_t i;

    /* loop over all breakpoints */
    for (i = 0, bp = bp_ ; i < VMDBG_BP_MAX ; ++i, ++bp)
    {
        /* if this breakpoint is active, put it back */
        if (bp->is_in_use() && !bp->is_disabled())
            bp->set_bp_instr(vmg_ TRUE, TRUE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Determine if a code location is within the current active method 
 */
int CVmDebug::is_in_current_method(VMG_ const uchar *code_addr)
{
    /* 
     *   if the current debug pointer isn't valid, we can't make this
     *   determination, so indicate that we're not in the active method 
     */
    if (!dbg_ptr_valid_ || code_addr == 0)
        return FALSE;

    /* 
     *   Determine the endpoint of the method's code.  If the method has
     *   an exception table, it's the endpoint; otherwise, the debug table
     *   is the endpoint. 
     */
    const uchar *end_addr = entry_
                            + (func_ptr_.get_exc_ofs() != 0
                               ? func_ptr_.get_exc_ofs()
                               : func_ptr_.get_debug_ofs());

    /* 
     *   if the code address is between the entrypoint address for the
     *   active method and the ending address we just calculated, it's in
     *   the active method; otherwise, it's somewhere else 
     */
    return (code_addr >= entry_ && code_addr < end_addr);
}



/* ------------------------------------------------------------------------ */
/*
 *   Set up a function pointer and line pointer, and get the statement
 *   bounds, for the function at the given stack level.  Level 0 is the
 *   currently executing function, 1 is the first enclosing stack level
 *   (which called the current function), and so on.  Returns true if
 *   successful, false if the information isn't available.  
 */
int CVmDebug::get_stack_level_info(VMG_ int level, CVmFuncPtr *func_ptr,
                                   CVmDbgLinePtr *line_ptr,
                                   const uchar **stm_start,
                                   const uchar **stm_end) const
{
    /* 
     *   if we're at level 0, it's the current method; otherwise, get the
     *   given enclosing frame from the stack 
     */
    if (level == 0)
    {
        /* 
         *   if we don't have a valid debug information pointer, we can't
         *   get any information on the current source location 
         */
        if (!dbg_ptr_valid_)
            return 1;
        
        /* use the current statement */
        func_ptr->copy_from(&func_ptr_);
        line_ptr->copy_from(&cur_stm_line_);
        *stm_start = cur_stm_start_;
        *stm_end = cur_stm_end_;

        /* success */
        return 0;
    }
    else
    {
        vm_val_t *fp = G_interpreter->get_frame_ptr();
        const vm_rcdesc *rc = G_interpreter->get_rcdesc_from_frame(vmg_ fp);
        const uchar *retp = G_interpreter->get_return_addr_from_frame(vmg_ fp);

        /* 
         *   Enclosing level - walk up the stack to the desired level.
         *   Note that we are actually looking for the return address, so
         *   we want to find the frame just within the desired level - for
         *   the first enclosing level, we actually want the current
         *   frame.  
         */
        for ( ; level > 1 && fp != 0 ; --level)
        {
            /* 
             *   if there's a native caller context at this level, the level
             *   counts twice, since the native caller counts as its own
             *   stack level 
             */
            if (rc != 0)
            {
                /* move to the recursive caller's return address */
                retp = rc->get_return_addr();

                /* move up to the bytecode caller by clearing 'rc' */
                rc = 0;

                /* count the level */
                if (--level <= 1)
                    break;
            }

            /* move up to the enclosing level */
            fp = G_interpreter->get_enclosing_frame_ptr(vmg_ fp);
            rc = G_interpreter->get_rcdesc_from_frame(vmg_ fp);
            retp = G_interpreter->get_return_addr_from_frame(vmg_ fp);
        }

        /* 
         *   If we didn't reach the desired level, or the frame pointer is
         *   null, we didn't find the requested frame - return failure.  
         */
        if (level > 1 || fp == 0)
            return 1;

        /* if we stopped at a native frame, there's no source location */
        if (rc != 0)
            return 1;

        /* get the entry address from the frame */
        const uchar *entry =
            G_interpreter->get_enclosing_entry_ptr_from_frame(vmg_ fp);

        /* set up a function pointer for the method */
        G_interpreter->set_return_funcptr_from_frame(vmg_ func_ptr, fp);

        /* 
         *   Find the source line information for the return address in
         *   the frame.  Return failure if there's no debug information
         *   for the method.  
         */
        if (!CVmRun::get_stm_bounds(vmg_ func_ptr, retp - entry,
                                    line_ptr, stm_start, stm_end))
            return 1;

        /* success */
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Allocate the method header list 
 */
void CVmDebug::alloc_method_header_list(ulong cnt)
{
    /* 
     *   if we already have a method header list, expand it; otherwise,
     *   allocate a new one 
     */
    if (method_hdr_ != 0)
    {
        /* 
         *   if we're growing the list, expand it; otherwise ignore the new
         *   allocation 
         */
        if (cnt > method_hdr_cnt_)
        {
            /* expand the list */
            method_hdr_ = (ulong *)t3realloc(method_hdr_,
                cnt * sizeof(method_hdr_[0]));

            /* note the new size */
            method_hdr_cnt_ = cnt;
        }
    }
    else
    {
        /* allocate the new list */
        method_hdr_ = (ulong *)t3malloc(cnt * sizeof(method_hdr_[0]));

        /* note the size */
        method_hdr_cnt_ = cnt;
    }
}

/*
 *   Given a code pool address, find the method header containing the
 *   address.  This searches the method header list for the nearest method
 *   header whose address is less than the given address.  
 */
const uchar *CVmDebug::find_method_header(VMG_ const uchar *addr)
{
    ulong lo;
    ulong hi;
    pool_ofs_t ofs;

    /* if there are no method headers, there's nothing to find */
    if (method_hdr_cnt_ == 0)
        return 0;

    /* if this isn't a valid pool address, we won't have an entry for it */
    if (!G_code_pool->get_ofs((const char *)addr, &ofs))
        return 0;

    /* perform a binary search of the method header list */
    lo = 0;
    hi = method_hdr_cnt_ - 1;
    while (lo <= hi)
    {
        ulong cur;
        pool_ofs_t addr;
        pool_ofs_t next_addr;

        /* split the difference to get the current entry */
        cur = lo + (hi - lo)/2;

        /* get this entry's address */
        addr = (pool_ofs_t)get_method_header(cur);

        /* 
         *   get the next entry's address - if this is the last entry, the
         *   next entry's address is the highest possible address 
         */
        next_addr = (cur + 1 >= method_hdr_cnt_
                     ? (pool_ofs_t)0xffffffff
                     : (pool_ofs_t)get_method_header(cur + 1));

        /* check how this value compares */
        if (ofs >= next_addr)
        {
            /* we need to go higher */
            lo = (cur == lo ? cur + 1 : cur);
        }
        else if (ofs < addr)
        {
            /* we need to go lower */
            hi = (cur == hi ? hi - 1 : cur);
        }
        else
        {
            /* found it - return this start address */
            return (const uchar *)G_code_pool->get_ptr(addr);
        }
    }

    /* 
     *   didn't find anything - we don't have any way to indicate this, so
     *   just return the lowest code pool address 
     */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Reverse-mapping hash entry 
 */

/*
 *   Construct.  Note that we don't bother to convert the ulong key value
 *   to a portable representation, since this entry is never written to a
 *   file - it's purely an in-memory construct.  Note also that we don't
 *   care whether there are any embedded null bytes in the value, since
 *   the hash entry base class works entirely with counted-length strings.
 *   
 *   Treating a ulong value as a character buffer might seem suspicious
 *   from a portability standpoint, but the C/C++ standards make this
 *   perfectly legitimate as long as we don't make assumptions about the
 *   size or byte ordering of the underlying representation, which we're
 *   not - the byte ordering is irrelevant, since we're simply treating
 *   this value as an identifying string of bytes (i.e., a hash key), and
 *   we're using sizeof() to ensure we use the correct local size for the
 *   type.  What we're doing is exactly the same thing that memcpy() would
 *   do if asked to copy a ulong value, and equally safe.
 */
CVmHashEntryDbgRev::CVmHashEntryDbgRev(ulong sym_val, const char *sym,
                                       size_t len)
    : CVmHashEntry((char *)&sym_val, sizeof(sym_val), TRUE)
{
    /* make a copy of the symbol name */
    sym_ = lib_copy_str(sym, len);
    sym_len_ = len;
}

/*
 *   Destruct 
 */
CVmHashEntryDbgRev::~CVmHashEntryDbgRev()
{
    /* delete our copy of the symbol name */
    lib_free_str(sym_);
}

/*
 *   check for a match 
 */
int CVmHashEntryDbgRev::matches(const char *str, size_t len) const
{
    /*
     *   it's a match if the strings are the same length and all
     *   characters match, treating case as significant 
     */
    return (len == len_
            && memcmp(str, str_, len * sizeof(*str)) == 0);
}

/*
 *   Reverse-lookup hash function 
 */
unsigned int CVmHashFuncDbgRev::compute_hash(const char *s, size_t l) const
{
    uint acc;

    /* add up all the byte values in the string */
    for (acc = 0 ; l != 0 ; ++s, --l)
    {
        uchar c;

        c = (uchar)*s;
        acc += c;
    }

    /* return the accumulated value */
    return acc;
}

/* ------------------------------------------------------------------------ */
/*
 *   Breakpoint object 
 */

/*
 *   instantiate 
 */
CVmDebugBp::CVmDebugBp()
{
    /* not yet in use */
    in_use_ = FALSE;
    
    /* no code address yet */
    code_addr_ = 0;

    /* not disabled */
    disabled_ = FALSE;
    
    /* no original instruction yet */
    orig_instr_ = OPC_NOP;
    
    /* no condition expression or byte code object yet */
    cond_ = 0;
    cond_buf_len_ = 0;
    compiled_cond_ = 0;
    has_cond_ = FALSE;
    stop_on_change_ = FALSE;
    prv_val_ = 0;
}
   

/*
 *   delete 
 */
CVmDebugBp::~CVmDebugBp()
{
    /* delete any condition text buffer */
    if (cond_ != 0)
        t3free(cond_);
}

/*
 *   terminate - this is called during VM termination so that we can
 *   delete our compiled condition code objects (we can't do this in the
 *   destructor because we need access to the VM globals) 
 */
void CVmDebugBp::do_terminate(VMG0_)
{
    /* release our object table global variables */
    if (prv_val_ != 0)
    {
        G_obj_table->delete_global_var(prv_val_);
        prv_val_ = 0;
    }
    if (compiled_cond_ != 0)
    {
        G_obj_table->delete_global_var(compiled_cond_);
        compiled_cond_ = 0;
    }
}

/*
 *   Set the breakpoint's information 
 */
int CVmDebugBp::set_info(VMG_ const uchar *code_addr,
                         const char *cond, int change,
                         int disabled, char *errbuf, size_t errbuflen)
{
    int err;
    
    /* remember the code address */
    code_addr_ = code_addr;

    /* remember the disabled status */
    disabled_ = disabled;

    /* remember the expression */
    if ((err = set_condition(vmg_ cond, change, errbuf, errbuflen)) != 0)
        return err;

    /* remember the original instruction at the code address */
    if (code_addr != 0)
    {
        /* get a writable pointer to the code location */
        uchar *code_ptr = (uchar *)code_addr;

        /* 
         *   remember the original instruction at this address so that we can
         *   restore it when we remove the BP instruction 
         */
        orig_instr_ = *code_ptr;

        /* set the BP instruction if appropriate */
        set_bp_instr(vmg_ TRUE, FALSE);
    }

    /* success */
    return 0;
}

/*
 *   Set the condition text 
 */
int CVmDebugBp::set_condition(VMG_ const char *new_cond, int change,
                              char *errbuf, size_t errbuflen)
{
    /* delete any existing condition variable */
    if (compiled_cond_ != 0)
    {
        G_obj_table->delete_global_var(compiled_cond_);
        compiled_cond_ = 0;
    }

    /* 
     *   check for an empty condition string - treat it as no condition if
     *   it's all blanks 
     */
    if (new_cond != 0)
    {
        const char *p;

        /* scan for a non-blank character */
        for (p = new_cond ; t3_is_whitespace(*p) ; ++p) ;

        /* 
         *   if we scanned the entire string without finding anything but
         *   spaces, treat this is no condition 
         */
        if (*p == '\0')
            new_cond = 0;
    }

    /* if there's a new condition, set it up */
    if (new_cond != 0)
    {
        size_t new_cond_len;
        CVmDbgSymtab *symtab;
        CVmDbgSymtab local_symtab;

        /* presume we won't find a local symbol table */
        symtab = 0;

        /* 
         *   if there's a code address, set up a local symbol table to
         *   compile the expression in the scope of the code location;
         *   otherwise, compile with no local scope 
         */
        if (code_addr_ != 0)
        {
            CVmFuncPtr func_ptr;
            CVmDbgLinePtr line_ptr;
            CVmDbgTablePtr dbg_ptr;
            const uchar *stm_start, *stm_end;

            /* get the method header for the breakpoint location */
            func_ptr.set(G_debugger->find_method_header(vmg_ code_addr_));

            /* set up the line pointer */
            if (func_ptr.get() != 0
                && CVmRun::get_stm_bounds(vmg_ &func_ptr,
                                          code_addr_ - func_ptr.get(),
                                          &line_ptr, &stm_start, &stm_end)
                && func_ptr.set_dbg_ptr(&dbg_ptr))
            {
                /* 
                 *   We have a valid debug table for the code location -
                 *   set up a local symbol table object for the code.
                 *   Compile the expression for stack level zero, since we
                 *   will always evaluate the expression when this code is
                 *   active.  
                 */
                local_symtab.init(vmg_ &dbg_ptr, line_ptr.get_frame_id(), 0);

                /* we now have a local symbol table for the code - use it */
                symtab = &local_symtab;
            }
        }

        /* 
         *   Compile the expression.  Compile at stack level zero, since
         *   we'll always execute this code immediately upon hitting the
         *   breakpoint, which means that the breakpoint location will be
         *   the active stack level whenever this code is executed.  
         *   
         *   Presume that 'self' will be valid in this context.  If it
         *   isn't, 'self' will be nil at run-time, so no harm will be done.
         */
        CVmDynCompResults results;
        vm_obj_id_t co;
        G_debugger->compile_expr(vmg_ new_cond, 0, symtab, TRUE,
                                 FALSE, 0, &co, &results);

        /* if an error occurred, return the error code */
        if (results.err != 0)
        {
            if (results.msgbuf != 0)
                lib_strcpy(errbuf, errbuflen, results.msgbuf);

            return results.err;
        }

        /* create a global variable to hold the condition object reference */
        compiled_cond_ = G_obj_table->create_global_var();
        compiled_cond_->val.set_obj(co);

        /* note that we have a condition */
        has_cond_ = TRUE;

        /* note whether or not it's a stop-on-change condition */
        stop_on_change_ = change;

        /* set the size of the new condition, including the trailing null */
        new_cond_len = strlen(new_cond) + 1;

        /* allocate or expand the condition buffer if required */
        if (cond_buf_len_ < new_cond_len)
        {
            /* allocate or reallocate the buffer */
            if (cond_ == 0)
                cond_ = (char *)t3malloc(new_cond_len);
            else
                cond_ = (char *)t3realloc(cond_, new_cond_len);

            /* note the new size of our condition buffer */
            cond_buf_len_ = new_cond_len;
        }

        /* copy the new condition text into our buffer */
        memcpy(cond_, new_cond, new_cond_len);

        /* if it's a stop-on-change condition, initialize the value */
        if (change)
        {
            /* 
             *   Allocate an object table global variable to hold the
             *   previous value, if we haven't already.  We allocate a global
             *   for this so that our value is tracked in the garbage
             *   collector.  
             */
            if (prv_val_ == 0)
                prv_val_ = G_obj_table->create_global_var();
            
            /* 
             *   set the value to 'empty' to indicate that we haven't
             *   evaluated our expression for the first time yet 
             */
            prv_val_->val.set_empty();

            /* 
             *   evaluate the condition to initialize our memory of the
             *   current value; we'll break the next time we reach this
             *   breakpoint and the new value of the expression differs from
             *   this saved value 
             */
            eval_cond(vmg0_);
        }
    }
    else
    {
        /* note that there's no condition */
        has_cond_ = FALSE;

        /* if we had a global variable for the condition, delete it */
        if (prv_val_ != 0)
        {
            G_obj_table->delete_global_var(prv_val_);
            prv_val_ = 0;
        }
    }

    /* success */
    return 0;
}

/*
 *   Evaluate my condition 
 */
int CVmDebugBp::eval_cond(VMG0_)
{
    int ret;
    vmrun_save_ctx run_ctx;
    vmdbg_step_save_t old_step;
    
    /* save the interpreter context */
    G_interpreter->save_context(vmg_ &run_ctx);

    /* set the debugger to non-stepping mode */
    G_debugger->prepare_for_eval(&old_step);

    /* 
     *   if there's no condition, just return false, since there's nothing
     *   to evaluate 
     */
    if (compiled_cond_ == 0)
        return FALSE;

    /* execute the code in a protected block */
    err_try
    {
        vm_obj_id_t self_obj;
        vm_obj_id_t orig_target_obj;
        vm_obj_id_t defining_obj;
        vm_prop_id_t target_prop;
        vm_val_t *valp;
        vm_val_t *fp;
        
        /* get the 'self' object at the current stack level */
        self_obj = G_interpreter->get_self_at_level(vmg_ 0);

        /* get the target property for the level */
        target_prop = G_interpreter->get_target_prop_at_level(vmg_ 0);

        /* get the original target object and defining object at the level */
        orig_target_obj =
            G_interpreter->get_orig_target_obj_at_level(vmg_ 0);
        defining_obj = G_interpreter->get_defining_obj_at_level(vmg_ 0);

        /* set up the frame context */
        fp = G_stk->push(5);
        (fp++)->set_propid(target_prop);
        (fp++)->set_obj_or_nil(orig_target_obj);
        (fp++)->set_obj_or_nil(defining_obj);
        (fp++)->set_obj_or_nil(self_obj);
        *(fp++) = compiled_cond_->val;

        /* execute the code */
        vm_rcdesc rc("dbg cond");
        G_interpreter->call_func_ptr(vmg_ &compiled_cond_->val, 0, &rc, 0);

        /* get the return value */
        valp = G_interpreter->get_r0();

        /* check what kind of condition we have */
        if (stop_on_change_)
        {
            /* 
             *   We're a stop-on-change condition - this means that we stop
             *   if the value of the expression differs from the previous
             *   value.  Check to see if the value differs, and return true
             *   (to indicate that the breakpoint condition has been met) if
             *   so.  If the previous value is 'empty', this means we've
             *   never evaluated the value before, so the value can't have
             *   changed; simply remember the new value in this case.  
             */
            ret = (prv_val_->val.typ != VM_EMPTY
                   && !prv_val_->val.equals(vmg_ valp));

            /* remember the new value for next time */
            prv_val_->val = *valp;
        }
        else
        {
            /* see what we have */
            switch(valp->typ)
            {
            case VM_INT:
                /* if the number is zero, it's false, otherwise it's true */
                ret = (valp->val.intval != 0);
                break;
                
            case VM_NIL:
                /* condition is false */
                ret = FALSE;
                break;
                
            default:
                /* anything else is a true condition value */
                ret = TRUE;
                break;
            }
        }
    }
    err_catch(exc)
    {
        /* 
         *   an error occurred - treat the condition as true so that we
         *   stop here 
         */
        ret = TRUE;
    }
    err_end;

    /* restore the original execution mode */
    G_debugger->restore_from_eval(&old_step);

    /* rerstore the interpreter context */
    G_interpreter->restore_context(vmg_ &run_ctx);

    /* return the result */
    return ret;
}


/*
 *   Delete the breakpoint 
 */
void CVmDebugBp::do_delete(VMG0_)
{
    /* remove the BP instruction from the code if necessary */
    set_bp_instr(vmg_ FALSE, FALSE);

    /* mark myself as no longer active */
    in_use_ = FALSE;
}

/*
 *   Set or remove the BP instruction in the code 
 */
void CVmDebugBp::set_bp_instr(VMG_ int set, int always)
{
    /* if I have a code address, set or remove the BP instruction */
    if (code_addr_ != 0)
    {
        /* get a writable pointer to the code location */
        uchar *code_ptr = (uchar *)code_addr_;

        /* check to see if we're setting or removing the BP instruction */
        if (set)
        {
            /* 
             *   if we're not in the debugger, write the BP instruction to
             *   the address - don't do this if we're in the debugger, since
             *   we remove all BP instructions from the code while the
             *   debugger has control to prevent any recursive debugger
             *   invocations from taking place 
             */
            if (always || !G_debugger->is_in_debugger())
                *code_ptr = OPC_BP;
        }
        else
        {
            /* 
             *   Restore the original instruction to that location.  This
             *   isn't necessary if we're in the debugger, since we remove
             *   all BP's when the debugger takes control.  
             */
            if (always || !G_debugger->is_in_debugger())
                *code_ptr = orig_instr_;
        }
    }
}
