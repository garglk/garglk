/* $Header: d:/cvsroot/tads/tads3/tcglob.h,v 1.4 1999/07/11 00:46:58 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcglob.h - TADS 3 Compiler globals
Function
  The TADS 3 Compiler uses a number of static variables that are
  shared by several subsystems.  We define these variables as
  global variables for quick access, and to minimize the number
  of parameters that are passed around among subsystems.

Notes
  
Modified
  05/01/99 MJRoberts  - creation
*/

#ifndef TCGLOB_H
#define TCGLOB_H

/*
 *   If we're not explicitly defining the storage for the globals, define
 *   them as external - this lets everyone pick up external declarations
 *   for all of the globals simply by including this file.  
 */
#ifndef TC_GLOB_DECLARE
#define TC_GLOB_DECLARE extern
#endif

/* host system interface */
TC_GLOB_DECLARE class CTcHostIfc *G_hostifc;

/* main compiler driver */
TC_GLOB_DECLARE class CTcMain *G_tcmain;

/* the parser */
TC_GLOB_DECLARE class CTcParser *G_prs;

/* parse tree node list memory manager */
TC_GLOB_DECLARE class CTcPrsMem *G_prsmem;

/* the tokenizer */
TC_GLOB_DECLARE class CTcTokenizer *G_tok;

/* 
 *   Current code stream - this points to the currently active code stream
 *   object.  The active code stream can vary according to what kind of
 *   code we're generating. 
 */
TC_GLOB_DECLARE class CTcCodeStream *G_cs;

/* primary generated code stream - for all normal code */
TC_GLOB_DECLARE class CTcCodeStream *G_cs_main;

/* static initializer code stream */
TC_GLOB_DECLARE class CTcCodeStream *G_cs_static;

/* local variable name stream */
TC_GLOB_DECLARE class CTcDataStream *G_lcl_stream;

/* generated data (constant) stream */
TC_GLOB_DECLARE class CTcDataStream *G_ds;

/* TADS-Object metaclass data stream */
TC_GLOB_DECLARE class CTcDataStream *G_os;

/* Dictionary metaclass data stream */
TC_GLOB_DECLARE class CTcDataStream *G_dict_stream;

/* GrammarProduction metaclass data stream */
TC_GLOB_DECLARE class CTcDataStream *G_gramprod_stream;

/* BigNumber metaclass data stream */
TC_GLOB_DECLARE class CTcDataStream *G_bignum_stream;

/* RexPattern metaclass data stream */
TC_GLOB_DECLARE class CTcDataStream *G_rexpat_stream;

/* IntrinsicClass metaclass data stream */
TC_GLOB_DECLARE class CTcDataStream *G_int_class_stream;

/* intrinsic class modifier metaclass data stream */
TC_GLOB_DECLARE class CTcDataStream *G_icmod_stream;

/* static initializer obj.prop ID stream */
TC_GLOB_DECLARE class CTcDataStream *G_static_init_id_stream;

/* target-specific code generator class */
TC_GLOB_DECLARE class CTcGenTarg *G_cg;

/* 
 *   Run-time metaclass table.  When we're doing dynamic compilation, the
 *   interpreter will set this to its live metaclass table for the loaded
 *   program. 
 */
TC_GLOB_DECLARE class CVmMetaTable *G_metaclass_tab;

/* 
 *   object ID fixup list head, and flag indicating whether to keep object
 *   fixups 
 */
TC_GLOB_DECLARE struct CTcIdFixup *G_objfixup;
TC_GLOB_DECLARE int G_keep_objfixups;

/* 
 *   property ID fixup list head, and flag indicating whether to keep
 *   property ID fixups 
 */
TC_GLOB_DECLARE struct CTcIdFixup *G_propfixup;
TC_GLOB_DECLARE int G_keep_propfixups;

/*
 *   enumerator ID fixup list head, and flag indicating whether to keep
 *   enumerator fixups 
 */
TC_GLOB_DECLARE struct CTcIdFixup *G_enumfixup;
TC_GLOB_DECLARE int G_keep_enumfixups;


/* 
 *   Debug mode - if this is true, we're compiling for debugging, so we
 *   must generate additional symbolic information for the debugger. 
 */
TC_GLOB_DECLARE int G_debug;

/* disassembly output stream, if disassembly display is desired */
TC_GLOB_DECLARE class CTcUnasOut *G_disasm_out;

/*
 *   Image file sizes.  For the normal compiler, these are fixed quantities
 *   based on the current VM version we're targeting.  For the dynamic
 *   compiler, these are adjusted to match the data from the loaded image
 *   file; this is necessary to ensure that code we generate matches static
 *   code loaded from the image.  
 */
struct tc_image_info
{
    /* size of the method header */
    int mhdr;

    /* size of the exception table entry */
    int exc_entry;

    /* debugger table header size */
    int dbg_hdr;

    /* debugger line table entry size */
    int dbg_line;

    /* debugger frame record size */
    int dbg_frame;

    /* local symbol header size */
    int lcl_hdr;

    /* debug record format version ID */
    int dbg_fmt_vsn;
};
TC_GLOB_DECLARE tc_image_info G_sizes;

/*
 *   Compiler interface to the live VM.  In dynamic compilation mode, the VM
 *   supplies the compiler with this object to provide the compiler with
 *   access to VM resources.  
 */
TC_GLOB_DECLARE class CTcVMIfc *G_vmifc;

#endif /* VMGLOB_H */

