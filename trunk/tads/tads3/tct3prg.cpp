#ifdef RCSID
static char RCSid[] =
    "$Header: d:/cvsroot/tads/tads3/TCT3STM.CPP,v 1.1 1999/07/11 00:46:57 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tct3stm.cpp - TADS 3 Compiler - T3 VM Code Generator - program-level classes
Function
  Generate code for the T3 VM.  This file contains program-level classes,
  in order to segregate the code generation classes required for the full
  compiler from those required for subsets that require only expression
  parsing (such as debuggers).  The program-level classes are for statements
  at the outer level, outside of executable code: object definitions,
  dictionary declarations, etc.  This also includes code to read and
  write object files.
Notes

Modified
  05/08/99 MJRoberts  - Creation
*/

#include <stdio.h>

#include "t3std.h"
#include "os.h"
#include "tcprs.h"
#include "tct3.h"
#include "tcgen.h"
#include "vmtype.h"
#include "vmwrtimg.h"
#include "vmfile.h"
#include "tcmain.h"
#include "tcerr.h"



/* ------------------------------------------------------------------------ */
/*
 *   Object Definition Statement
 */

/*
 *   given the offset of the start of an object in the compiled object
 *   stream, get the offset of the first property 
 */
ulong CTPNStmObject::get_stream_first_prop_ofs(CTcDataStream *stream,
                                               ulong obj_ofs)
{
    /* 
     *   the property table follows the superclass table, which follows
     *   the tads-object header; the superclass table contains 4 bytes per
     *   superclass (we can obtain the superclass count from the stream
     *   data) 
     */
    return obj_ofs + TCT3_TADSOBJ_SC_OFS
        + (get_stream_sc_cnt(stream, obj_ofs) * 4);
}

/*
 *   given the offset of the start of an object in the compiled object
 *   stream, get the offset of the property at the given index
 */
ulong CTPNStmObject::get_stream_prop_ofs(CTcDataStream *stream,
                                         ulong obj_ofs, uint idx)
{
    /* 
     *   calculate the offset to the selected property from the start of
     *   the property table 
     */
    return get_stream_first_prop_ofs(stream, obj_ofs)
        + (TCT3_TADSOBJ_PROP_SIZE * idx);
}

/*
 *   given the offset of the start of an object in the compiled object
 *   stream, get the number of properties in the stream data 
 */
uint CTPNStmObject::get_stream_prop_cnt(CTcDataStream *stream,
                                        ulong obj_ofs)
{
    /* the property count is at offset 2 in the tads-object header */
    return stream->readu2_at(obj_ofs + TCT3_TADSOBJ_HEADER_OFS + 2);
}

/*
 *   given the offset of the start of an object in the compiled object
 *   stream, set the number of properties in the stream data, adjusting
 *   the data size in the metaclass header to match 
 */
size_t CTPNStmObject::set_stream_prop_cnt(CTcDataStream *stream,
                                          ulong obj_ofs, uint prop_cnt)
{
    size_t data_size;
    
    /* the property count is at offset 2 in the tads-object header */
    stream->write2_at(obj_ofs + TCT3_TADSOBJ_HEADER_OFS + 2, prop_cnt);

    /* 
     *   calculate the new data size to store in the metaclass header --
     *   this is the size of the tads-object header, plus the size of the
     *   superclass table (4 bytes per superclass), plus the size of the
     *   property table 
     */
    data_size = TCT3_TADSOBJ_HEADER_SIZE
                + (get_stream_sc_cnt(stream, obj_ofs) * 4)
                + (prop_cnt * TCT3_TADSOBJ_PROP_SIZE);

    /* write the data size to the metaclass header (it's at offset 4) */
    stream->write2_at(obj_ofs + TCT3_META_HEADER_OFS + 4, data_size);

    /* return the new data size */
    return data_size;
}

/*
 *   Get the object flags from an object in a compiled stream 
 */
uint CTPNStmObject::get_stream_obj_flags(CTcDataStream *stream,
                                         ulong obj_ofs)
{
    /* the flags are at offset 4 in the tads-object header */
    return stream->read2_at(obj_ofs + TCT3_TADSOBJ_HEADER_OFS + 4);
}

/*
 *   Set the object flags in an object in a compiled stream 
 */
void CTPNStmObject::set_stream_obj_flags(CTcDataStream *stream,
                                         ulong obj_ofs, uint flags)
{
    /* 
     *   write the new flags - they're at offset 4 in the tads-object
     *   header 
     */
    stream->write2_at(obj_ofs + TCT3_TADSOBJ_HEADER_OFS + 4, flags);
}

/*
 *   given the offset of the start of an object in the compiled object
 *   stream, get the number of superclasses in the stream data 
 */
uint CTPNStmObject::get_stream_sc_cnt(CTcDataStream *stream,
                                      ulong obj_ofs)
{
    /* the superclass count is at offset 0 in the tads-object header */
    return stream->readu2_at(obj_ofs + TCT3_TADSOBJ_HEADER_OFS + 0);
}

/*
 *   given the stream offset of the start of an object in the compiled
 *   object stream, change a superclass object ID 
 */
void CTPNStmObject::set_stream_sc(CTcDataStream *stream, ulong obj_ofs,
                                  uint sc_idx, vm_obj_id_t new_sc)
{
    /* 
     *   set the superclass - it's at offset 6 in the object data, plus
     *   four bytes (UINT4) per index slot 
     */
    stream->write2_at(obj_ofs + TCT3_TADSOBJ_HEADER_OFS + 6 + (sc_idx * 4),
                      new_sc);
}

/*
 *   given the offset of the start of an object in the compiled object
 *   stream, get the property ID of the property at a given index in the
 *   property table 
 */
vm_prop_id_t CTPNStmObject::get_stream_prop_id(CTcDataStream *stream,
                                               ulong obj_ofs, uint prop_idx)
{
    ulong prop_ofs;
    
    /* get the property's data offset */
    prop_ofs = get_stream_prop_ofs(stream, obj_ofs, prop_idx);

    /* read the property ID - it's at offset 0 in the property data */
    return (vm_prop_id_t)stream->readu2_at(prop_ofs);
}

/*
 *   given the offset of the start of an object in the compiled object
 *   stream, get the datatype of the property at the given index in the
 *   property table 
 */
vm_datatype_t CTPNStmObject::
   get_stream_prop_type(CTcDataStream *stream, ulong obj_ofs, uint prop_idx)
{
    ulong dh_ofs;

    /* get the property's data holder offset */
    dh_ofs = get_stream_prop_val_ofs(stream, obj_ofs, prop_idx);
    
    /* the type is the first byte of the serialized data holder */
    return (vm_datatype_t)stream->get_byte_at(dh_ofs);
}

/*
 *   given the offset of the start of an object in the compiled object
 *   stream, get the stream offset of the serialized DATAHOLDER structure
 *   for the property at the given index in the property table 
 */
ulong CTPNStmObject::get_stream_prop_val_ofs(CTcDataStream *stream,
                                             ulong obj_ofs, uint prop_idx)
{
    ulong prop_ofs;

    /* get the property's data offset */
    prop_ofs = get_stream_prop_ofs(stream, obj_ofs, prop_idx);

    /* the data holder immediately follows the (UINT2) property ID */
    return prop_ofs + 2;
}


/*
 *   given the offset of the start of an object in the compiled object
 *   stream, get the property ID of the property at a given index in the
 *   property table 
 */
void CTPNStmObject::set_stream_prop_id(CTcDataStream *stream,
                                       ulong obj_ofs, uint prop_idx,
                                       vm_prop_id_t new_id)
{
    ulong prop_ofs;

    /* get the property's data offset */
    prop_ofs = get_stream_prop_ofs(stream, obj_ofs, prop_idx);

    /* set the data */
    stream->write2_at(prop_ofs, (uint)new_id);
}

/* ------------------------------------------------------------------------ */
/*
 *   Object Property list entry - value node
 */
void CTPNObjProp::gen_code(int, int)
{
    vm_val_t val;
    char buf[VMB_DATAHOLDER];
    CTcDataStream *str;

    /* get the correct data stream */
    str = obj_stm_->get_obj_sym()->get_stream();

    /* set the current source location for error reporting */
    G_tok->set_line_info(file_, linenum_);

    /* generate code for our expression or our code body, as appropriate */
    if (expr_ != 0)
    {
        /* 
         *   if my value is constant, write out a dataholder for the
         *   constant value to the stream; otherwise, write out our code
         *   and store a pointer to the code 
         */
        if (expr_->is_const())
        {
            /* write the constant value to the object stream */
            G_cg->write_const_as_dh(str, str->get_ofs(),
                                    expr_->get_const_val());
        }
        else if (expr_->is_dstring())
        {
            CTPNDstr *dstr;
            
            /* it's a double-quoted string node */
            dstr = (CTPNDstr *)expr_;
            
            /* 
             *   Add the string to the constant pool.  Note that the fixup
             *   will be one byte from the current object stream offset,
             *   since we need to write the type byte first.  
             */
            G_cg->add_const_str(dstr->get_str(), dstr->get_str_len(),
                                str, str->get_ofs() + 1);

            /* 
             *   Set up the dstring value.  Use a zero placeholder for
             *   now; add_const_str() already added a fixup for us that
             *   will supply the correct value at link time.  
             */
            val.set_dstring(0);
            vmb_put_dh(buf, &val);
            str->write(buf, VMB_DATAHOLDER);
        }
        else
        {
            /* we should never get here */
            G_tok->throw_internal_error(TCERR_INVAL_PROP_CODE_GEN);
        }
    }
    else if (code_body_ != 0)
    {
        char buf[VMB_DATAHOLDER];
        vm_val_t val;

        /* if this is a constructor, mark the code body accordingly */
        if (prop_sym_->get_prop() == G_prs->get_constructor_prop())
            code_body_->set_constructor(TRUE);

        /* if it's static, do some extra work */
        if (is_static_)
        {
            /* mark the code body as static */
            code_body_->set_static();

            /* 
             *   add the obj.prop to the static ID stream, so the VM knows
             *   to invoke this initializer at start-up 
             */
            G_static_init_id_stream
                ->write_obj_id(obj_stm_->get_obj_sym()->get_obj_id());
            G_static_init_id_stream
                ->write_prop_id(prop_sym_->get_prop());
        }
        
        /* tell our code body to generate the code */
        code_body_->gen_code(FALSE, FALSE);
        
        /* 
         *   Set up our code offset value.  Write a code offset of zero
         *   for now, since we won't know the correct offset until link
         *   time.  
         */
        val.set_codeofs(0);
        
        /* 
         *   Add a fixup to the code body's fixup list for our dataholder,
         *   so that we fix up the property value when we link.  Note that
         *   the fixup is one byte into our object stream from the current
         *   offset, because the first byte is the type.  
         */
        CTcAbsFixup::add_abs_fixup(code_body_->get_fixup_list_head(),
                                   str, str->get_ofs() + 1);
        
        /* write out our value in DATAHOLDER format */
        vmb_put_dh(buf, &val);
        str->write(buf, VMB_DATAHOLDER);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Implicit constructor 
 */
void CTPNStmImplicitCtor::gen_code(int /*discard*/, int /*for_condition*/)
{
    CTPNSuperclass *sc;

    /* 
     *   Generate a call to inherit each superclass constructor.  Pass the
     *   same argument list we received by expanding the varargs list
     *   parameter in local 0.  
     */
    for (sc = obj_stm_->get_first_sc() ; sc != 0 ; sc = sc->nxt_)
    {
        CTcSymObj *sc_sym;

        /* 
         *   if this one is valid, generate code to call its constructor -
         *   it's valid if it has an object symbol 
         */
        sc_sym = (CTcSymObj *)sc->get_sym();
        if (sc_sym != 0 && sc_sym->get_type() == TC_SYM_OBJ)
        {
            /* push the argument counter so far (no other arguments) */
            G_cg->write_op(OPC_PUSH_0);
            G_cg->note_push();

            /* get the varargs list local */
            CTcSymLocal::s_gen_code_getlcl(0, FALSE);

            /* convert it to varargs */
            G_cg->write_op(OPC_MAKELSTPAR);

            /* note the extra push and pop for the argument count */
            G_cg->note_push();
            G_cg->note_pop();

            /* it's a varargs call */
            G_cg->write_op(OPC_VARARGC);

            /* generate an EXPINHERIT to this superclass */
            G_cg->write_op(OPC_EXPINHERIT);
            G_cs->write(0);                      /* varargs -> argc ignored */
            G_cs->write_prop_id(G_prs->get_constructor_prop());
            G_cs->write_obj_id(sc_sym->get_obj_id());

            /* 
             *   this removes arguments (the varargs list variable and
             *   argument count) 
             */
            G_cg->note_pop(2);
        }
    }
}



