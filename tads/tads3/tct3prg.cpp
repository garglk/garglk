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

