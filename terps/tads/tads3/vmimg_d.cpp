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
  vmimg_d.cpp - T3 VM image file implementation - DEBUG version
Function
  This implements certain functions for the debugger version of the
  T3 VM.  These functions can be omitted (but must be stubbed out)
  for stand-alone non-debug versions.
Notes
  
Modified
  12/03/99 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "t3std.h"
#include "vmtype.h"
#include "vminit.h"
#include "vmimage.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmfile.h"
#include "tctok.h"
#include "tcprs.h"
#include "tctarg.h"
#include "tcglob.h"
#include "vmdbg.h"
#include "vmmeta.h"
#include "vmmcreg.h"


/* ------------------------------------------------------------------------ */
/*
 *   load a Global Symbols block
 */
void CVmImageLoader::load_gsym(VMG_ ulong siz)
{
    char buf[TOK_SYM_MAX_LEN + 128];

    /* 
     *   if there's no global symbol table, merely load into the runtime
     *   symbol table, since we don't seem to need the information for
     *   debugging 
     */
    if (G_prs->get_global_symtab() == 0)
    {
        /* load it into the runtime reflection symbol table */
        load_runtime_symtab_from_gsym(vmg_ siz);

        /* we're done */
        return;
    }

    /* 
     *   remember the starting seek position and size, so we can re-read the
     *   block into the runtime symbol table 
     */
    long init_seek_pos = fp_->get_seek();
    ulong init_siz = siz;

    /* 
     *   note that we have a GSYM block - this implies that the image was
     *   linked for debugging 
     */
    has_gsym_ = TRUE;

    /* read the symbol count */
    read_data(buf, 4, &siz);
    ulong cnt = t3rp4u(buf);

    /* read the symbols and populate the global symbol table */
    for ( ; cnt != 0 ; --cnt)
    {
        char *sym_name;
        size_t sym_len;
        size_t dat_len;
        CTcSymbol *sym;
        tc_symtype_t sym_type;
        char *dat;
        
        /* read the symbol's length, extra data length, and type code */
        read_data(buf, 6, &siz);
        sym_len = osrp2(buf);
        dat_len = osrp2(buf + 2);
        sym_type = (tc_symtype_t)osrp2(buf + 4);

        /* check the lengths to make sure they don't overflow our buffer */
        if (sym_len > TOK_SYM_MAX_LEN)
        {
            /* 
             *   this symbol name is too long - skip the symbol and its
             *   extra data entirely
             */
            skip_data(sym_len + dat_len, &siz);

            /* go on to the next symbol */
            continue;
        }
        else if (dat_len + sym_len > sizeof(buf))
        {
            /* 
             *   the extra data block is too long - truncate the extra
             *   data so that we don't overflow our buffer, but proceed
             *   anyway with the truncated extra data 
             */
            read_data(buf, sizeof(buf), &siz);

            /* skip the remainder of the extra data */
            skip_data(sym_len + dat_len - sizeof(buf), &siz);
        }
        else
        {
            /* read the symbol's name and its type-specific data */
            read_data(buf, sym_len + dat_len, &siz);
        }

        /* get the pointer to the extra data (after the name) */
        dat = buf + sym_len;

        /* 
         *   allocate a copy of the symbol name in parser memory - this is
         *   necessary because the symbol objects themselves always come
         *   from parser memory, and hence never get individually deleted 
         */
        sym_name = (char *)G_prsmem->alloc(sym_len);
        memcpy(sym_name, buf, sym_len);

        /* create the new symbol table entry, depending on its type */
        switch(sym_type)
        {
        case TC_SYM_FUNC:
            /* create the function symbol */
            sym = new CTcSymFunc(
                sym_name, sym_len, FALSE,
                (int)osrp2(dat + 4),                                /* argc */

                /* 
                 *   the optional argument count is present in 3.1+ records,
                 *   which are at least 10 bytes; otherwise there aren't any
                 *   optional arguments 
                 */
                (dat_len >= 10 ? osrp2(dat+8) : 0),             /* opt_argc */
                
                dat[6] != 0,                                     /* varargs */
                dat[7] != 0,                                  /* has_retval */
                FALSE,                                    /* is_multimethod */
                FALSE,                               /* is_multimethod_base */
                FALSE,                                         /* is_extern */
                TRUE);                                         /* has_proto */

            /* add the reverse mapping entry */
            G_debugger->add_rev_sym(sym_name, sym_len, sym_type, t3rp4u(dat));

            /* set the function's absolute address */
            ((CTcSymFunc *)sym)->set_abs_addr(t3rp4u(dat));
            break;

        case TC_SYM_OBJ:
            /* create the object symbol */
            sym = new CTcSymObj(sym_name, sym_len, FALSE, t3rp4u(dat), FALSE,
                                TC_META_TADSOBJ, 0);

            /* if there's a modifying object, store it */
            if (dat_len >= 8)
                ((CTcSymObj *)sym)->set_modifying_obj_id(t3rp4u(dat + 4));
            else
                ((CTcSymObj *)sym)->set_modifying_obj_id(VM_INVALID_OBJ);

            /* add the reverse mapping entry */
            G_debugger->add_rev_sym(sym_name, sym_len, sym_type, t3rp4u(dat));
            break;

        case TC_SYM_PROP:
            /* create the property symbol */
            sym = new CTcSymProp(sym_name, sym_len, FALSE, osrp2(dat));

            /* set the 'dictionary property' flag if present */
            if (dat_len >= 3 && (buf[2] & 1) != 0)
                ((CTcSymProp *)sym)->set_vocab(TRUE);

            /* add the reverse mapping entry */
            G_debugger->add_rev_sym(sym_name, sym_len, sym_type, osrp2(dat));
            break;

        case TC_SYM_ENUM:
            /* create the enumerator symbol */
            sym = new CTcSymEnum(sym_name, sym_len, FALSE,
                                 t3rp4u(dat),
                                 (dat_len >= 5 && (buf[4] & 1) != 0));

            /* add the reverse mapping entry */
            G_debugger->add_rev_sym(sym_name, sym_len, sym_type, t3rp4u(dat));
            break;

        case TC_SYM_BIF:
            /* create the built-in function symbol */
            sym = new CTcSymBif(sym_name, sym_len, FALSE,
                                osrp2(dat + 2),          /* function set ID */
                                osrp2(dat),               /* function index */
                                dat[4] != 0,                  /* has_retval */
                                osrp2(dat + 5),                 /* min_argc */
                                osrp2(dat + 7),                 /* max_argc */
                                dat[9] != 0);                    /* varargs */

            /* add the reverse mapping entry */
            G_debugger->add_rev_sym(sym_name, sym_len, sym_type,
                                    (osrp2(dat+2) << 16) | osrp2(dat));
            break;

        case TC_SYM_EXTFN:
            /* not currently supported */
            sym = 0;
            break;

        case TC_SYM_METACLASS:
            /* create the metaclass symbol */
            sym = new CTcSymMetaclass(sym_name, sym_len, FALSE,
                                      osrp2(dat), t3rp4u(dat + 2));

            /* 
             *   add a reverse mapping symbol for the object instance
             *   (note that we add this as an object, not a metaclass,
             *   since it refers to the IntrinsicClass instance) 
             */
            if ((vm_obj_id_t)t3rp4u(dat + 2) != VM_INVALID_OBJ)
                G_debugger->add_rev_sym(sym_name, sym_len,
                                        TC_SYM_OBJ, t3rp4u(dat + 2));
            break;

        default:
            /* ignore other types of symbols */
            sym = 0;
            break;
        }

        /* if the symbol was valid, add it to the global symbol table */
        if (sym != 0)
            G_prs->get_global_symtab()->add_entry(sym);
    }

    /*
     *   Seek back to the starting position, and then load the GSYM data
     *   into the runtime reflection symbol table 
     */
    fp_->seek(init_seek_pos);
    load_runtime_symtab_from_gsym(vmg_ init_siz);
}

/* ------------------------------------------------------------------------ */
/*
 *   Callback context structure for fix_obj_meta_cb() 
 */
struct fix_obj_meta_cb_ctx
{
    /* holder for VM globals structure pointer */
    vm_globals *vmg;

    /* translation table - metaclass index to compiler TC_META_xxx ID */
    const tc_metaclass_t *xlat;
};

/*
 *   Global symbol enumeration callback - fix up this symbol's compiler
 *   metaclass identifier, if this is an object symbol.  
 */
static void fix_obj_meta_cb(void *ctx0, CTcSymbol *sym)
{
    /* cast our context and set up to access VM globals */
    fix_obj_meta_cb_ctx *ctx = (fix_obj_meta_cb_ctx *)ctx0;
    VMGLOB_PTR(ctx->vmg);

    /* if this is an object symbol, fix up its metaclass identifier */
    if (sym->get_type() == TC_SYM_OBJ)
    {
        CVmObject *obj;
        CVmMetaclass *meta;
        uint idx;
        tc_metaclass_t id;

        /* cast the symbol to an object symbol */
        CTcSymObj *obj_sym = (CTcSymObj *)sym;
        
        /* get the object */
        obj = vm_objp(vmg_ (vm_obj_id_t)obj_sym->get_obj_id());

        /* look up its metaclass */
        meta = obj->get_metaclass_reg();

        /* get the registration table index */
        idx = meta->get_reg_idx();

        /* that's our index into the translation table - look it up */
        id = ctx->xlat[idx];

        /* store the compiler metaclass ID back in the symbol */
        obj_sym->set_metaclass(id);
    }
}

/*
 *   Fix up the global symbol table to set the correct metaclasses for all
 *   object symbols.  This has to wait until we've finished reading the whole
 *   image file, because we have need to finish reading all of the actual
 *   objects (specifically the OBJS records) before we can be sure we'll have
 *   access to all of the metaclass information.  
 */
void CVmImageLoader::fix_gsym_meta(VMG0_)
{
    tc_metaclass_t *xlat;
    size_t i, cnt;
    fix_obj_meta_cb_ctx ctx;

    /* if there's no global symbol table, there's nothing to do */
    if (G_prs->get_global_symtab() == 0)
        return;

    /*
     *   Tell the debugger code generator about the actual loaded metaclass
     *   dependency table.  This is necessary so that the code generator can
     *   generate the correct metaclass index references for this object
     *   file.  First, start the table.
     */
    G_cg->start_image_file_meta_table();

    /* now add each entry from the table */
    for (cnt = G_meta_table->get_count(), i = 0 ; i < cnt ; ++i)
    {
        /* get this item's name */
        const char *nm = G_meta_table->get_entry(i)->image_meta_name_;

        /* find the version suffix, if any */
        const char *p;
        for (p = nm ; *p != '\0' && *p != '/' ; ++p) ;

        /* tell the code generator about the entry */
        G_cg->load_image_file_meta_table(nm, p - nm, i);
    }

    /* done with the table */
    G_cg->end_image_file_meta_table();

    /* build a translation from metaclass index to compiler TC_META_xxx ID */
    xlat = CVmMetaTable::build_runtime_to_compiler_id_table(vmg0_);

    /* now run through the symbol table and fix up each object symbol */
    ctx.vmg = VMGLOB_ADDR;
    ctx.xlat = xlat;
    G_prs->get_global_symtab()->enum_entries(&fix_obj_meta_cb, &ctx);

    /* free our temporary translation table */
    t3free(xlat);
}

/* ------------------------------------------------------------------------ */
/*
 *   Load a method header list block 
 */
void CVmImageLoader::load_mhls(VMG_ ulong siz)
{
    char buf[512];
    ulong cnt;
    ulong i;

    /* read the count */
    read_data(buf, 4, &siz);
    cnt = t3rp4u(buf);

    /* allocate space for the list */
    G_debugger->alloc_method_header_list(cnt);

    /* read the entries */
    for (i = 0 ; cnt != 0 ; )
    {
        size_t cur;
        char *p;
        
        /* read as many as we can fit, up to the number remaining */
        cur = sizeof(buf) / 4;
        if (cur > cnt)
            cur = (size_t)cnt;

        /* read the chunk */
        read_data(buf, cur * 4, &siz);

        /* deduct the number we just read from the number remaining */
        cnt -= cur;

        /* process these entries */
        for (p = buf ; cur != 0 ; --cur, ++i, p += 4)
            G_debugger->set_method_header(i, t3rp4u(p));
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Error handler for macro loader 
 */
class MyLoadMacErr: public CTcTokLoadMacErr
{
public:
    /* log an error */
    virtual void log_error(int err)
    {
        /* throw an error */
        err_throw_a(VMERR_INVAL_IMAGE_MACRO, 1, ERR_TYPE_INT, err);
    }
};

/*
 *   Load a Macro Symbols (MACR) block 
 */
void CVmImageLoader::load_macros(VMG_ ulong siz)
{
    /* 
     *   remember the starting position, so we can load the same data for
     *   reflection after we load it for the debugger 
     */
    long init_seek_pos = fp_->get_seek();
    ulong init_siz = siz;

    /* set up a stream object for reading from the MACR block */
    CVmImageFileStream str(fp_, siz);

    /* read the macros */
    MyLoadMacErr err_handler;
    G_tok->load_macros_from_file(&str, &err_handler);

    /*
     *   Seek back to the starting position, and then load the MACR data into
     *   the runtime reflection symbol table as well
     */
    fp_->seek(init_seek_pos);
    load_runtime_symtab_from_macr(vmg_ init_siz);
}
