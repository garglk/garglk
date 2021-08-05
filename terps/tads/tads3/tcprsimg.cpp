#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCPRSIMG.CPP,v 1.1 1999/07/11 00:46:53 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcprsimg.cpp - TADS 3 Compiler Parser - image writing functions
Function
  
Notes
  
Modified
  04/30/99 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "os.h"
#include "t3std.h"
#include "tcprs.h"
#include "tctarg.h"
#include "tcgen.h"
#include "vmhash.h"
#include "tcmain.h"
#include "vmfile.h"
#include "tctok.h"


/* ------------------------------------------------------------------------ */
/*
 *   Read an object file and load the global symbol table 
 */
int CTcParser::load_object_file(class CVmFile *fp, const textchar_t *fname,
                                tctarg_obj_id_t *obj_xlat,
                                tctarg_prop_id_t *prop_xlat,
                                ulong *enum_xlat)
{
    ulong sym_cnt;
    ulong dict_cnt;
    ulong i;
    ulong anon_cnt;
    ulong nonsym_cnt;
    ulong prod_cnt;
    ulong exp_cnt;

    /* read the number of symbol index entries */
    sym_cnt = fp->read_uint4();
    if (sym_cnt != 0)
    {
        /* allocate space for the symbol index list */
        obj_sym_list_ = (CTcSymbol **)
                        t3malloc(sym_cnt * sizeof(obj_sym_list_[0]));

        /* the list is empty so far */
        obj_file_sym_idx_ = 0;
    }

    /* read the number of dictionary symbols */
    dict_cnt = fp->read_uint4();

    /* if there are any symbols, read them */
    if (dict_cnt != 0)
    {
        /* allocate space for the dictionary index list */
        obj_dict_list_ = (CTcDictEntry **)
                         t3malloc(dict_cnt * sizeof(obj_dict_list_[0]));

        /* nothing in the list yet */
        obj_file_dict_idx_ = 0;
    }

    /* read the number of symbols in the file */
    sym_cnt = fp->read_uint4();

    /* read the symbols */
    for (i = 0 ; i < sym_cnt ; ++i)
    {
        /* load a symbol */
        if (CTcSymbol::load_from_obj_file(fp, fname,
                                          obj_xlat, prop_xlat, enum_xlat))
            return 1;
    }

    /* read the number of anonymous object symbols */
    anon_cnt = fp->read_uint4();

    /* read the anonymous object symbols */
    for (i = 0 ; i < anon_cnt ; ++i)
    {
        /* load the next anonymous object symbol */
        if (CTcSymObj::load_from_obj_file(fp, fname, obj_xlat, TRUE))
            return 1;
    }

    /* read the non-symbol object ID's */
    nonsym_cnt = fp->read_uint4();
    for (i = 0 ; i < nonsym_cnt ; ++i)
    {
        tctarg_obj_id_t id;

        /* read the next non-symbol object ID */
        id = (tctarg_obj_id_t)fp->read_uint4();

        /* 
         *   allocate a new ID for the object, and set the translation
         *   table for the new ID - this will ensure that references to
         *   this non-symbol object are properly fixed up 
         */
        obj_xlat[id] = G_cg->new_obj_id();
    }

    /* read the number of symbol cross-reference sections in the file */
    sym_cnt = fp->read_uint4();

    /* read the symbol cross-references */
    for (i = 0 ; i < sym_cnt ; ++i)
    {
        ulong idx;
        CTcSymbol *sym;

        /* read the symbol index */
        idx = fp->read_uint4();

        /* get the symbol from the index list */
        sym = get_objfile_sym(idx);

        /* load the symbol's reference information */
        sym->load_refs_from_obj_file(fp, fname, obj_xlat, prop_xlat);
    }

    /* read the number of anonymous object cross-references */
    anon_cnt = fp->read_uint4();

    /* read the anonymous object cross-references */
    for (i = 0 ; i < anon_cnt ; ++i)
    {
        ulong idx;
        CTcSymbol *sym;

        /* read the symbol index */
        idx = fp->read_uint4();

        /* get the symbol from the index list */
        sym = get_objfile_sym(idx);

        /* load the symbol's reference information */
        sym->load_refs_from_obj_file(fp, fname, obj_xlat, prop_xlat);
    }

    /* read the master grammar rule count */
    prod_cnt = fp->read_uint4();

    /* read the master grammar rule list */
    for (i = 0 ; i < prod_cnt ; ++i)
    {
        /* read the next grammar production */
        CTcGramProdEntry::load_from_obj_file(fp, prop_xlat, enum_xlat, 0);
    }

    /* read the number of named grammar rules */
    prod_cnt = fp->read_uint4();

    /* read the private grammar rules */
    for (i = 0 ; i < prod_cnt ; ++i)
    {
        CTcSymObj *match_sym;

        /* read the match object defining the rule */
        match_sym = get_objfile_objsym(fp->read_uint4());

        /* read the private rule list */
        CTcGramProdEntry::load_from_obj_file(
            fp, prop_xlat, enum_xlat, match_sym);
    }

    /* read the export symbol list */
    exp_cnt = fp->read_uint4();
    for (i = 0 ; i < exp_cnt ; ++i)
    {
        CTcPrsExport *exp;
        
        /* read the next entry */
        exp = CTcPrsExport::read_from_obj_file(fp);

        /* if that failed, the whole load fails */
        if (exp == 0)
            return 1;

        /* add it to our list */
        add_export_to_list(exp);
    }

    /* done with the symbol index list - free it */
    if (obj_sym_list_ != 0)
    {
        /* free it and forget it */
        t3free(obj_sym_list_);
        obj_sym_list_ = 0;
    }

    /* done with the dictionary index list - free it */
    if (obj_dict_list_ != 0)
    {
        /* free the memory and forget it */
        t3free(obj_dict_list_);
        obj_dict_list_ = 0;
    }

    /* success */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Generate code and write the image file 
 */
void CTPNStmProg::build_image(class CVmFile *image_fp, uchar xor_mask,
                              const char tool_data[4])
{
    /* generate code */
    if (gen_code_for_build())
        return;

    /* scan the symbol table for unresolved external references */
    if (G_prs->check_unresolved_externs())
        return;

    /*
     *   Finally, our task of constructing the program is complete.  All
     *   that remains is to write the image file.  Tell the code generator
     *   to begin the process.  
     */
    G_cg->write_to_image(image_fp, xor_mask, tool_data);
}

/* ------------------------------------------------------------------------ */
/*
 *   Generate code and write the object file 
 */
void CTPNStmProg::build_object_file(class CVmFile *object_fp,
                                    class CTcMake *make_obj)
{
    /* generate code */
    if (gen_code_for_build())
        return;

    /*
     *   Finally, our task of constructing the program is complete.  All
     *   that remains is to write the image file.  Tell the code generator
     *   to begin the process.  
     */
    G_cg->write_to_object_file(object_fp, make_obj);
}

/* ------------------------------------------------------------------------ */
/*
 *   Generate code for a build, in preparation for writing an image file
 *   or an object file.
 */
int CTPNStmProg::gen_code_for_build()
{
    /* notify the tokenizer that parsing is done */
    G_tok->parsing_done();

    /* notify the code generator that we're finished parsing */
    G_cg->parsing_done();

    /* set the global symbol table in the code streams */
    G_cs_main->set_symtab(G_prs->get_global_symtab());
    G_cs_static->set_symtab(G_prs->get_global_symtab());

    /* generate code for the entire program */
    gen_code(TRUE, TRUE);

    /* 
     *   if we encountered any errors generating code, don't bother
     *   writing the image 
     */
    if (G_tcmain->get_error_count() != 0)
        return 1;

    /* return success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check for unresolved external symbols.  Logs an error for each
 *   unresolved external.  
 */
int CTcParser::check_unresolved_externs()
{
    int errcnt;

    /* generate any synthesized code */
    G_cg->build_synthesized_code();
    
    /* note the previous error count */
    errcnt = G_tcmain->get_error_count();
    
    /* enumerate the entries with our unresolved check callback */
    get_global_symtab()->enum_entries(&enum_sym_extref, this);

    /* 
     *   if the error count increased, we logged errors for unresolved
     *   symbols 
     */
    return (G_tcmain->get_error_count() > errcnt);
}

/*
 *   Enumeration callback - check for unresolved external references.  For
 *   each object or function still marked "external," we'll log an error.  
 */
void CTcParser::enum_sym_extref(void *, CTcSymbol *sym)
{
    /* if it's an external symbol, log an error */
    if (sym->is_unresolved_extern())
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_UNRESOLVED_EXTERN,
                            (int)sym->get_sym_len(), sym->get_sym());
}


/* ------------------------------------------------------------------------ */
/*
 *   Build dictionaries.  We go through all objects and insert their
 *   vocabulary words into their dictionaries.  
 */
void CTcParser::build_dictionaries()
{
    CTcDictEntry *dict;
    CTcSymObj *sym;
    
    /* 
     *   enumerate our symbols to insert dictionary words - this will
     *   populate each dictionary's hash table with a complete list of the
     *   words and object associations for the dictionary 
     */
    get_global_symtab()->enum_entries(&enum_sym_dict, this);

    /* do the same for the anonymous objects */
    for (sym = anon_obj_head_ ; sym != 0 ; sym = (CTcSymObj *)sym->nxt_)
        sym->build_dictionary();

    /* generate the object stream for each dictionary */
    for (dict = dict_head_ ; dict != 0 ; dict = dict->get_next())
    {
        /* generate the code (static data, actually) for this dictionary */
        G_cg->gen_code_for_dict(dict);
    }
}

/*
 *   enumeration callback - build dictionaries 
 */
void CTcParser::enum_sym_dict(void *, CTcSymbol *sym)
{
    /* tell this symbol to build its dictionary entries */
    sym->build_dictionary();
}

/* ------------------------------------------------------------------------ */
/*
 *   Build grammar productions 
 */
void CTcParser::build_grammar_productions()
{
    CTcGramProdEntry *entry;

    /* 
     *   First, run through the symbol table and merge all of the private
     *   grammar rules into the master grammar rule list.  Since we've
     *   finished linking, we've already applied all modify/replace
     *   overrides, hence each symbol table entry referring to an object
     *   will contain its final private grammar rule list.  So, we can
     *   safely merge the private lists into the master lists at this point,
     *   since no more modifications to private lists are possible.  
     */
    get_global_symtab()->enum_entries(&build_grammar_cb, this);

    /* 
     *   iterate over the master list of productions and generate the image
     *   data for each one 
     */
    for (entry = gramprod_head_ ; entry != 0 ; entry = entry->get_next())
    {
        /* build this entry */
        G_cg->gen_code_for_gramprod(entry);
    }
}

/*
 *   Symbol table enumeration callback - merge match object private grammar
 *   rules into the master grammar rule list.  
 */
void CTcParser::build_grammar_cb(void *, CTcSymbol *sym)
{
    /* if this is an object, merge its private grammar list */
    if (sym->get_type() == TC_SYM_OBJ)
        ((CTcSymObj *)sym)->merge_grammar_entry();
}

/* ------------------------------------------------------------------------ */
/*
 *   Apply self-reference object ID fixups.  This traverses the symbol
 *   table and applies each object's list of fixups.  This can be called
 *   once after loading all object files.  
 */
void CTcParser::apply_internal_fixups()
{
    CTcSymObj *anon_obj;
    
    /* enumerate the entries with our callback */
    get_global_symtab()->enum_entries(&enum_sym_internal_fixup, this);

    /* apply internal fixups to our anonymous objects */
    for (anon_obj = anon_obj_head_ ; anon_obj != 0 ;
         anon_obj = (CTcSymObj *)anon_obj->nxt_)
    {
        /* apply internal fixups to this symbol */
        anon_obj->apply_internal_fixups();
    }
}

/*
 *   Enumeration callback - apply internal ID fixups
 */
void CTcParser::enum_sym_internal_fixup(void *, CTcSymbol *sym)
{
    /* apply its self-reference fixups */
    sym->apply_internal_fixups();
}

/* ------------------------------------------------------------------------ */
/*
 *   Basic symbol class - image/object file functions 
 */

/*
 *   Read a symbol from an object file 
 */
int CTcSymbolBase::load_from_obj_file(CVmFile *fp, const textchar_t *fname,
                                      tctarg_obj_id_t *obj_xlat,
                                      tctarg_prop_id_t *prop_xlat,
                                      ulong *enum_xlat)
{
    tc_symtype_t typ;

    /* 
     *   read the type - this is the one thing we know is always present
     *   for every symbol (the rest of the data might vary per subclass) 
     */
    typ = (tc_symtype_t)fp->read_uint2();

    /* create the object based on the type */
    switch(typ)
    {
    case TC_SYM_FUNC:
        return CTcSymFunc::load_from_obj_file(fp, fname);

    case TC_SYM_OBJ:
        return CTcSymObj::load_from_obj_file(fp, fname, obj_xlat, FALSE);

    case TC_SYM_PROP:
        return CTcSymProp::load_from_obj_file(fp, fname, prop_xlat);

    case TC_SYM_ENUM:
        return CTcSymEnum::load_from_obj_file(fp, fname, enum_xlat);

    case TC_SYM_BIF:
        return CTcSymBif::load_from_obj_file(fp, fname);

    case TC_SYM_METACLASS:
        return CTcSymMetaclass::load_from_obj_file(fp, fname, obj_xlat);

    default:
        /* other types should not be in an object file */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_OBJFILE_INV_TYPE);

        /* return an error indication */
        return 1;
    }
}

/*
 *   Log a conflict with another symbol from an object file 
 */
void CTcSymbolBase::log_objfile_conflict(const textchar_t *fname,
                                         tc_symtype_t new_type) const
{
    static const textchar_t *type_name[] =
    {
        "unknown", "function", "object", "property", "local",
        "parameter", "intrinsic function", "native function", "code label",
        "intrinsic class", "enum"
    };

    /* 
     *   if the types differ, log an error indicating the different types;
     *   otherwise, simply log an error indicating the redefinition 
     */
    if (new_type != get_type())
    {
        /* the types differ - show the two types */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_OBJFILE_REDEF_SYM_TYPE,
                            (int)get_sym_len(), get_sym(),
                            type_name[get_type()], type_name[new_type],
                            fname);
    }
    else
    {
        /* the types are the same */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_OBJFILE_REDEF_SYM,
                            (int)get_sym_len(), get_sym(),
                            type_name[new_type], fname);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Function Symbol subclass - image/object file functions 
 */

/*
 *   Load from an object file 
 */
int CTcSymFuncBase::load_from_obj_file(CVmFile *fp,
                                       const textchar_t *fname)
{
    char txtbuf[4096];
    const char *txt;
    size_t len;
    char buf[12];
    int is_extern;
    int ext_replace;
    int ext_modify;
    int has_retval;
    int varargs;
    int argc;
    int opt_argc;
    int is_multimethod, is_multimethod_base;
    int mod_base_cnt;
    CTcSymFunc *sym;

    /* 
     *   Read the symbol name.  Use a custom reader instead of the base
     *   reader, because function symbols can be quite long, due to
     *   multimethod name decoration.  
     */
    if ((txt = CTcParser::read_len_prefix_str(
        fp, txtbuf, sizeof(txtbuf), 0, TCERR_SYMEXP_SYM_TOO_LONG)) == 0)
        return 1;
    len = strlen(txt);

    /* read our extra data */
    fp->read_bytes(buf, 12);
    argc = osrp2(buf);
    opt_argc = osrp2(buf + 2);
    varargs = buf[4];
    has_retval = buf[5];
    is_extern = buf[6];
    ext_replace = buf[7];
    ext_modify = buf[8];
    is_multimethod = (buf[9] & 1) != 0;
    is_multimethod_base = (buf[9] & 2) != 0;
    mod_base_cnt = osrp2(buf + 10);

    /* look up any existing symbol */
    sym = (CTcSymFunc *)G_prs->get_global_symtab()->find(txt, len);

    /* 
     *   If this symbol is already defined, make sure the original
     *   definition is a function, and make sure that it's only defined
     *   (not referenced as external) once.  If it's not defined, define
     *   it anew.  
     */
    if (sym == 0)
    {
        /* 
         *   It's not defined yet - create the new definition and add it
         *   to the symbol table.  
         */
        sym = new CTcSymFunc(txt, len, FALSE, argc, opt_argc, varargs,
                             has_retval, is_multimethod, is_multimethod_base,
                             is_extern, TRUE);
        G_prs->get_global_symtab()->add_entry(sym);

        /* it's an error if we're replacing a previously undefined function */
        if (ext_replace || ext_modify)
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_OBJFILE_REPFUNC_BEFORE_ORIG,
                                (int)len, txt, fname);
    }
    else if (sym->get_type() != TC_SYM_FUNC
             || (!sym->is_extern()
                 && !is_extern && !ext_replace && !ext_modify))
    {
        /* 
         *   It's already defined, but it's not a function, or this is a
         *   non-extern/replaced definition and the symbol is already
         *   defined non-extern - log a symbol type conflict error.  
         */
        sym->log_objfile_conflict(fname, TC_SYM_FUNC);

        /* 
         *   proceed despite the error, since this is merely a symbol
         *   conflict and not a file corruption - create a fake symbol to
         *   hold the information, so that we can read the data and thus
         *   keep in sync with the file, but don't bother adding the fake
         *   symbol object to the symbol table 
         */
        sym = new CTcSymFunc(txt, len, FALSE, argc, opt_argc, varargs,
                             has_retval, is_multimethod, is_multimethod_base,
                             is_extern, TRUE);
    }
    else if (sym->get_argc() != argc
             || sym->is_varargs() != varargs
             || sym->has_retval() != has_retval)
    {
        /* the symbol has an incompatible definition - log the error */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_OBJFILE_FUNC_INCOMPAT,
                            (int)len, txt, fname);
    }
    else if (sym->is_multimethod() != is_multimethod
             || sym->is_multimethod_base() != is_multimethod_base)
    {
        /* the multi-method status conflicts */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_OBJFILE_MMFUNC_INCOMPAT,
                            (int)len, txt, fname);
    }

    /* 
     *   if this is a non-extern definition, we now have the object
     *   defined -- remove the 'extern' flag from the symbol table entry
     *   in this case 
     */
    if (!is_extern)
    {
        /* mark the symbol as defined */
        sym->set_extern(FALSE);

        /* 
         *   if we're replacing it, delete the original; if we're modifying
         *   it, chain the original into our modify list 
         */
        if (ext_replace)
        {
            int i;

            /* 
             *   mark the previous code anchor as obsolete so that we
             *   don't write its code to the image file 
             */
            if (sym->get_anchor() != 0)
                sym->get_anchor()->set_replaced(TRUE);

            /*
             *   Mark all of the modified base function code offsets as
             *   replaced as well.  
             */
            for (i = 0 ; i < sym->get_mod_base_offset_count() ; ++i)
            {
                CTcStreamAnchor *anchor;

                /* get the anchor for this offset */
                anchor = G_cs->find_anchor(sym->get_mod_base_offset(i));

                /* mark it as replaced */
                if (anchor != 0)
                    anchor->set_replaced(TRUE);
            }

            /*
             *   We can now forget everything in the modify base list, as
             *   everything in the list is being replaced and is thus no
             *   longer relevant.  
             */
            sym->clear_mod_base_offsets();
        }
        else if (ext_modify)
        {
            /*
             *   We're modifying an external symbol.  The anchor to the code
             *   stream object that we previously loaded is actually the
             *   anchor to the modified base object, not to the new meaning
             *   of the symbol, so detach the anchor from our symbol.  
             */
            sym->get_anchor()->detach_from_symbol();

            /*
             *   The object file has a fixup list for references to the
             *   external base object that we're modifying.  In other words,
             *   these are external references from the object file we're
             *   loading to the now-nameless code stream object that we're
             *   replacing, which is the code stream object at our anchor.
             *   So, load those fixups into the anchor's new internal fixup
             *   list.  It's important to note that these aren't references
             *   to this symbol - they're specifically references to the
             *   modified base code stream object.  
             */
            CTcAbsFixup::load_fixup_list_from_object_file(
                fp, fname, sym->get_anchor()->fixup_list_head_);

            /*
             *   Add the old code stream anchor to the list of modified base
             *   offsets for the function.  The function we're reading from
             *   the object file modifies this as a base function, so we need
             *   to add this to the list of modified base functions.  
             */
            sym->add_mod_base_offset(sym->get_anchor()->get_ofs());

            /* 
             *   Complete the dissociation from the anchor by forgetting the
             *   anchor in the symbol.  This will allow the code stream
             *   object that's associated with this symbol in the current
             *   file to take over the anchor duty for this symbol, which
             *   will ensure that all fixups that reference this symbol will
             *   be resolved to the new code stream object.  
             */
            sym->set_anchor(0);
        }
    }

    /* 
     *   Read the list of modified base function offsets.  Each entry is a
     *   code stream offset, so adjust each using the base code stream offset
     *   for this object file.  
     */
    for ( ; mod_base_cnt != 0 ; --mod_base_cnt)
    {
        int i;

        /* read them */
        for (i = 0 ; i < mod_base_cnt ; ++i)
        {
            /* read the offset, adjusting to the object file start position */
            ulong ofs = fp->read_uint4() + G_cs->get_object_file_start_ofs();

            /* append this item */
            sym->add_mod_base_offset(ofs);
        }
    }

    /* if it's extern, load the fixup list */
    if (is_extern)
    {
        /*
         *   This is an external reference, so we must load our fixup
         *   list, adding it to any fixup list that already exists with
         *   the symbol.  
         */
        CTcAbsFixup::
            load_fixup_list_from_object_file(fp, fname, &sym->fixups_);
    }

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   object symbol entry base - image/object file functions
 */

/*
 *   Load from an object file 
 */
int CTcSymObjBase::load_from_obj_file(CVmFile *fp,
                                      const textchar_t *fname,
                                      tctarg_obj_id_t *obj_xlat,
                                      int anon)
{
    /* 
     *   do the main loading - if it fails to return a symbol, return
     *   failure (i.e., non-zero) 
     */
    return (load_from_obj_file_main(fp, fname, obj_xlat, 0, 0, anon) == 0);
}

/*
 *   Load a modified base object from an object file 
 */
CTcSymObj *CTcSymObjBase::
   load_from_obj_file_modbase(class CVmFile *fp, const textchar_t *fname,
                              tctarg_obj_id_t *obj_xlat,
                              const textchar_t *mod_name,
                              size_t mod_name_len, int anon)
{
    /* skip the type prefix - we know it's an object */
    fp->read_uint2();

    /* load the object and return the symbol */
    return load_from_obj_file_main(fp, fname, obj_xlat,
                                   mod_name, mod_name_len, anon);
}

/*
 *   Load from an object file.  This main routine does most of the work,
 *   and returns the loaded symbol.
 *   
 *   'mod_name' is the primary symbol name for a stack of 'modify'
 *   objects.  Each of the objects in a 'modify' stack, except for the
 *   topmost (i.e., last defined) object, has a fake symbol name, since
 *   the program can't refer directly to the base object once modified.
 *   However, while loading, we must know the actual name for the entire
 *   stack, so that we can link the bottom of the stack in this object
 *   file to the top of the stack in another object file if the bottom of
 *   our stack is declared external (i.e., this object file's source code
 *   used 'modify' with an external object).  If we're loading a top-level
 *   object, not a modified object, 'mod_name' should be null.  
 */
CTcSymObj *CTcSymObjBase::
   load_from_obj_file_main(CVmFile *fp, const textchar_t *fname,
                           tctarg_obj_id_t *obj_xlat,
                           const textchar_t *mod_name, size_t mod_name_len,
                           int anon)
{
    /* presume we won't have to use a fake symbol */
    int use_fake_sym = FALSE;

    /* presume we won't be able to read a stream offset */
    int stream_ofs_valid = FALSE;
    ulong stream_ofs = 0;
    
    /* read the symbol name information if it's not anonymous */
    const char *txt;
    if (!anon)
    {
        /* read the symbol name */
        if ((txt = base_read_from_sym_file(fp)) == 0)
            return 0;
    }
    else
    {
        /* use ".anon" as our symbol name placeholder */
        txt = ".anon";
    }

    /* get the symbol len */
    size_t len = strlen(txt);

    /* read our extra data */
    char buf[32];
    fp->read_bytes(buf, 17);
    ulong id = t3rp4u(buf);
    int is_extern = buf[4];
    int ext_replace_flag = buf[5];
    int modified_flag = buf[6];
    int modify_flag = buf[7];
    int ext_modify_flag = buf[8];
    int class_flag = buf[9];
    int trans_flag = buf[10];
    tc_metaclass_t meta = (tc_metaclass_t)osrp2(buf + 11);
    uint dict_idx = osrp2(buf + 13);
    uint obj_file_idx = osrp2(buf + 15);

    /* 
     *   if we're not external, read our stream offset, and adjust for the
     *   object stream base in the object file 
     */
    if (!is_extern)
    {
        /* get the appropriate stream */
        CTcDataStream *stream = get_stream_from_meta(meta);

        /* read the relative stream offset */
        stream_ofs = fp->read_uint4();

        /* 
         *   Ensure the stream offset was actually valid.  It must be valid
         *   unless the object has no stream (for example, dictionary and
         *   grammar production objects are not generated until link time,
         *   hence they don't have to have - indeed, can't have - valid
         *   stream offsets when we're loading an object file).  
         */
        assert(stream_ofs != 0xffffffff || stream == 0);

        /* determine if it's valid */
        if (stream_ofs != 0xffffffff)
        {
            /* adjust it relative to this object file's stream base */
            stream_ofs += stream->get_object_file_start_ofs();

            /* note that it's valid */
            stream_ofs_valid = TRUE;
        }
        else
        {
            /* the stream offset is not valid */
            stream_ofs_valid = FALSE;
        }
    }

    /* we have no deleted properties yet */
    CTcObjPropDel *del_prop_head = 0;

    /* if this is a 'modify' object, read some additional data */
    if (modify_flag)
    {
        uint cnt;

        /* read the deleted property list */
        for (cnt = fp->read_uint2() ; cnt != 0 ; --cnt)
        {
            const char *prop_name;
            CTcSymProp *prop_sym;
            
            /* read the symbol name from the file */
            prop_name = base_read_from_sym_file(fp);
            if (prop_name == 0)
                return 0;
            
            /* 
             *   find the property symbol, or define it if it's not
             *   already defined as a property 
             */
            prop_sym = (CTcSymProp *)G_prs->get_global_symtab()
                       ->find_or_def_prop(prop_name, strlen(prop_name),
                                          FALSE);
            
            /* make sure it's a property */
            if (prop_sym->get_type() != TC_SYM_PROP)
            {
                /* it's not a property - log the conflict */
                prop_sym->log_objfile_conflict(fname, TC_SYM_PROP);
            }
            else
            {
                /* add the entry to my list */
                add_del_prop_to_list(&del_prop_head, prop_sym);
            }
        }
    }

    /* read the self-reference fixup list */
    CTcIdFixup *fixups = 0;
    CTcIdFixup::load_object_file(fp, 0, 0, TCGEN_XLAT_OBJ,
                                 4, fname, &fixups);

    /* 
     *   if this is a 'modify' object, load the base object - this is the
     *   original version of the object, which this object modifies 
     */
    CTcSymObj *mod_base_sym = 0;
    if (modify_flag)
    {
        /* 
         *   Load the base object - pass the top-level object's name
         *   (which is our own name if the caller didn't pass an enclosing
         *   top-level object to us).  Note that we must read, and can
         *   immediately discard, the type data in the object file - we
         *   know that the base symbol is going to be an object, since we
         *   always write it out at this specific place in the file, but
         *   we will have written the type information anyway; thus, we
         *   don't need the type information, but we must at least skip it
         *   in the file.  
         */
        mod_base_sym =
            load_from_obj_file_modbase(fp, fname, obj_xlat,
                                       mod_name != 0 ? mod_name : txt,
                                       mod_name != 0 ? mod_name_len : len,
                                       FALSE);

        /* if that failed, return failure */
        if (mod_base_sym == 0)
            return 0;
    }

    /* 
     *   If this is a 'modifed extern' symbol, it's just a placeholder to
     *   connect the bottom object in the stack of modified objects in
     *   this file with the top object in another object file. 
     */
    CTcSymObj *sym = 0;
    if (is_extern && modified_flag)
    {
        /*
         *   We're modifying an external object.  This must be the bottom
         *   object in the stack for this object file, and serves as a
         *   placeholder for the top object in a stack in another object
         *   file.  We must find the object with the name of our top-level
         *   object (not the fake name for this modified base object, but
         *   the real name for the top-level object, because the symbol
         *   we're modifying in the other file is the top object in its
         *   stack, if any).  So, look up the symbol in the other file,
         *   which must already be loaded.  
         */
        sym = (CTcSymObj *)
              G_prs->get_global_symtab()->find(mod_name, mod_name_len);

        /* 
         *   If the original base symbol wasn't an object of metaclass
         *   "TADS Object", we can't modify it. 
         */
        if (sym != 0
            && (sym->get_type() != TC_SYM_OBJ
                || sym->get_metaclass() != TC_META_TADSOBJ))
        {
            /* log an error */
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_OBJFILE_CANNOT_MOD_OR_REP_TYPE,
                                (int)sym->get_sym_len(), sym->get_sym(),
                                fname);

            /* forget the symbol */
            sym = 0;
        }
        
        /* create a synthesized object to hold the original definition */
        CTcSymObj *mod_sym = synthesize_modified_obj_sym(FALSE);

        /* transfer data to the new fake symbol */
        if (sym != 0)
        {
            /* 
             *   'sym' has the original version of the object from the
             *   other object file - the original object file must be
             *   loaded before an object file that modifies a symbol it
             *   exports, so 'sym' will definitely be present in this case
             *   (it's an error - undefined external symbol - that we will
             *   have already caught if it's not defined).  We want to
             *   hijack 'sym' for our own use, since 'modify' replaces the
             *   symbol's meaning with the new object data.
             *   
             *   Transfer the self-reference fixup list from the original
             *   version of the object to the new synthesized object --
             *   all of the self-references must now refer to the
             *   renumbered object.
             *   
             *   This is really all we need to do to renumber the object.
             *   By moving the self-fixup list to the new fake object, we
             *   ensure that the original object will use its new number,
             *   which leaves the original number for our use in the new,
             *   modifying object (i.e., the one we're loading now).  Note
             *   that we'll replace the self-fixup list for this symbol
             *   with the fixup list of the modifying symbol, below.  
             */
            mod_sym->set_fixups(sym->get_fixups());

            /* 
             *   Give the modified fake symbol the original pre-modified
             *   object data stream.  The fake symbol owns the
             *   pre-modified data stream because it's the pre-modified
             *   object.  
             */
            mod_sym->set_stream_ofs(sym->get_stream_ofs());

            /* 
             *   transfer the 'modify' base symbol from the original
             *   version of this symbol to the new fake version 
             */
            mod_sym->set_mod_base_sym(sym->get_mod_base_sym());

            /* transfer the property deletion list */
            mod_sym->set_del_prop_head(sym->get_first_del_prop());
            sym->set_del_prop_head(0);

            /* 
             *   mark the original object as a 'class' object - it might
             *   have been compiled as a normal instance in its own
             *   translation unit, but it's now a class because it's the
             *   base class for this link-time 'modify' 
             */
            mod_sym->mark_compiled_as_class();

            /* transfer the dictionary to the base symbol */
            mod_sym->set_dict(sym->get_dict());

            /* copy the class flag to the base symbol */
            mod_sym->set_is_class(sym->is_class());

            /* set our class flag to the one from the original symbol */
            class_flag = sym->is_class();

            /* 
             *   transfer the superclass list from the original symbol to
             *   the modified base symbol 
             */
            mod_sym->set_sc_head(sym->get_sc_head());
            sym->set_sc_head(0);

            /* transfer the vocabulary list to the modified base symbol */
            mod_sym->set_vocab_head(sym->get_vocab_head());
            sym->set_vocab_head(0);
        }

        /* do the remaining loading into the synthesized placeholder */
        sym = mod_sym;
    }
    else if (modified_flag)
    {
        /*
         *   The symbol was modified, so the name is fake.  Because the
         *   name is tied to the object ID, which can change between the
         *   the time of writing the object file and now, when we're
         *   loading the object file, we must synthesize a new fake name
         *   in the context of the loaded object file.  The name is based
         *   on the object number, which is why it must be re-synthesized
         *   - the object number in this scheme can be different than the
         *   original object number in the object file.  
         */
        sym = synthesize_modified_obj_sym(FALSE);

        /* set the appropriate metaclass */
        sym->set_metaclass(meta);
    }
    else if (anon)
    {
        /* 
         *   we will definitely not find a previous entry for an anonymous
         *   symbol, because there's no name to look up 
         */
        sym = 0;
    }
    else
    {
        /* 
         *   normal object - look up a previous definition of the symbol
         *   in the global symbol table 
         */
        sym = (CTcSymObj *)G_prs->get_global_symtab()->find(txt, len);
    }

    /* 
     *   If this symbol is already defined, make sure the original
     *   definition is an object, and make sure that it's only defined
     *   (not referenced as external) once.  If it's not defined, define
     *   it anew.  
     */
    if (sym != 0 && sym->get_type() != TC_SYM_OBJ)
    {
        /* 
         *   It's already defined, but it's not an object - log a symbol
         *   type conflict error
         */
        sym->log_objfile_conflict(fname, TC_SYM_OBJ);

        /* 
         *   proceed despite the error, since this is merely a symbol
         *   conflict and not a file corruption - create a fake symbol to
         *   hold the data of the original symbol so we can continue
         *   loading 
         */
        sym = 0;
        use_fake_sym = TRUE;
    }
    else if ((ext_replace_flag || ext_modify_flag)
             && sym != 0 && sym->get_metaclass() != TC_META_TADSOBJ)
    {
        /* cannot modify or replace anything but an ordinary object */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                            TCERR_OBJFILE_CANNOT_MOD_OR_REP_TYPE,
                            (int)sym->get_sym_len(), sym->get_sym(),
                            fname);

        /* forget that we're doing a replacement */
        ext_replace_flag = ext_modify_flag = FALSE;
    }
    else if (sym != 0
             && (sym->get_metaclass() == TC_META_DICT
                 || sym->get_metaclass() == TC_META_GRAMPROD))
    {
        /* 
         *   If this is a dictionary or grammar production object, and the
         *   original definition was of the same metaclass, allow the
         *   multiple definitions without conflict - just treat the new
         *   definition as external.  These object types don't require a
         *   primary definition - every time such an object is defined,
         *   it's a definition, but the same definition can appear in
         *   multiple object files without conflict.  Simply act as though
         *   this new declaration is extern after all in this case.
         */
        if (meta == sym->get_metaclass())
        {
            /* 
             *   it's another one of the same type - allow it without
             *   conflict; act as though this new definition is external 
             */
            if (!sym->is_extern())
                is_extern = FALSE;
        }
        else
        {
            /* the other one's of a different type - log a conflict */
            sym->log_objfile_conflict(fname, TC_SYM_OBJ);

            /* proceed with a fake symbol */
            sym = 0;
            use_fake_sym = TRUE;
        }
    }
    else if ((ext_replace_flag || ext_modify_flag)
             && (sym == 0 || sym->is_extern()))
    {
        /*
         *   This symbol isn't defined yet, or is only defined as an
         *   external, but the new symbol is marked as 'replace' or
         *   'modify' - it's an error, because the original version of an
         *   object must always be loaded before the replaced or modified
         *   version 
         */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                            TCERR_OBJFILE_MODREPOBJ_BEFORE_ORIG,
                            (int)len, txt, fname);

        /* forget the symbol */
        sym = 0;

        /* not replacing anything after all */
        ext_replace_flag = ext_modify_flag = FALSE;
    }
    else if (sym != 0
             && !sym->is_extern()
             && !(is_extern || ext_modify_flag || ext_replace_flag
                  || modified_flag))
    {
        /* 
         *   the symbol was already defined, and this is a new actual
         *   definition (not external, and not replace or modify) -- this
         *   is an error because it means the same object is defined more
         *   than once 
         */
        sym->log_objfile_conflict(fname, TC_SYM_OBJ);

        /* 
         *   proceed despite the error, since this is merely a symbol
         *   conflict and not a file corruption - create a fake symbol to
         *   hold the data of the original symbol so we can continue
         *   loading 
         */
        sym = 0;
        use_fake_sym = TRUE;
    }
    else if (sym != 0 && meta != sym->get_metaclass())
    {
        /* 
         *   the new symbol and the old symbol have different metaclasses
         *   - it's a conflict 
         */
        sym->log_objfile_conflict(fname, TC_SYM_OBJ);

        /* proceed with a fake symbol */
        sym = 0;
        use_fake_sym = TRUE;
    }

    /* create the object if necessary */
    if (sym == 0)
    {
        /* 
         *   The symbol isn't defined yet - create the new definition and
         *   add it to the symbol table.  Allocate a new object ID for the
         *   symbol in the normal fashion.  
         */
        sym = new CTcSymObj(txt, len, FALSE, G_cg->new_obj_id(),
                            is_extern, meta, 0);

        /* 
         *   if we're using a fake symbol, don't bother adding the symbol
         *   to the symbol table, since its only function is to allow us
         *   to finish reading the object file data (we won't actually try
         *   to link when using a fake symbol, since this always means
         *   that an error has made linking impossible; we'll proceed
         *   anyway so that we catch any other errors that remain to be
         *   found)
         *   
         *   similarly, don't add the symbol if it's anonymous 
         */
        if (!use_fake_sym && !anon)
            G_prs->get_global_symtab()->add_entry(sym);

        /* if it's anonymous, add it to the anonymous symbol list */
        if (anon)
            G_prs->add_anon_obj(sym);
    }

    /*
     *   If we're replacing the object, tell the code generator to get rid
     *   of the old object definition in the object stream -- delete the
     *   definition at the symbol's old stream offset.  
     */
    if (ext_replace_flag)
        G_cg->notify_replace_object(sym->get_stream_ofs());

    /* 
     *   If this is a non-extern definition, we now have the object
     *   defined -- remove the 'extern' flag from the symbol table entry,
     *   and set the symbol's data to the data we just read.  Do not
     *   transfer data to the symbol if this is an extern, since we want
     *   to use the existing data from the originally loaded object.  
     */
    if (!is_extern)
    {
        /* clear the external flag */
        sym->set_extern(FALSE);

        /* set the object's stream offset, if we read one */
        if (stream_ofs_valid)
            sym->set_stream_ofs(stream_ofs);

        /* set the base 'modify' symbol if this symbol modifies another */
        if (mod_base_sym != 0)
            sym->set_mod_base_sym(mod_base_sym);

        /* set the new symbol's fixup list */
        sym->set_fixups(fixups);

        /* set the new symbol's deleted property list */
        sym->set_del_prop_head(del_prop_head);

        /* 
         *   set the symbol's class flag - only add the class flag,
         *   because we might have already set the class flag for this
         *   symbol based on the external definition 
         */
        if (class_flag)
            sym->set_is_class(class_flag);
    }

    /* add this symbol to the load file object index list */
    G_prs->add_sym_from_obj_file(obj_file_idx, sym);

    /* set the dictionary, if one was specified */
    if (dict_idx != 0)
        sym->set_dict(G_prs->get_obj_dict(dict_idx));

    /* 
     *   if this is a dictionary symbol, add it to the dictionary fixup
     *   list 
     */
    if (meta == TC_META_DICT)
        G_prs->add_dict_from_obj_file(sym);

    /* set the transient flag */
    if (trans_flag)
        sym->set_transient();

    /*
     *   Set the translation table entry for the symbol.  We know the
     *   original ID local to the object file, and we know the new global
     *   object ID.  
     */
    obj_xlat[id] = sym->get_obj_id();

    /* success */
    return sym;
}

/*
 *   Apply our self-reference fixups 
 */
void CTcSymObjBase::apply_internal_fixups()
{
    /* run through our list and apply each fixup */
    for (CTcIdFixup *fixup = fixups_ ; fixup != 0 ; fixup = fixup->nxt_)
        fixup->apply_fixup(obj_id_, 4);
    
    /*
     *   If we're a 'modify' object, and we were based at compile-time on
     *   an object external to the translation unit in which this modified
     *   version of the object was defined, we'll have a property deletion
     *   list to be applied at link time.  Now is the time - go through
     *   our list and delete each property in each of our 'modify' base
     *   classes.  Don't delete the properties in our own object,
     *   obviously - just in our modified base classes.  
     */
    for (CTcSymObj *mod_base = mod_base_sym_ ; mod_base != 0 ;
         mod_base = mod_base->get_mod_base_sym())
    {
        /* delete each property in our deletion list in this base class */
        for (CTcObjPropDel *entry = first_del_prop_ ; entry != 0 ;
             entry = entry->nxt_)
        {
            /* delete this property from the base object */
            mod_base->delete_prop_from_mod_base(entry->prop_sym_->get_prop());

            /* remove it from the base object's vocabulary list */
            mod_base->delete_vocab_prop(entry->prop_sym_->get_prop());
        }
    }
}

/*
 *   Merge my private grammar rules into the master rule list for the
 *   associated grammar production object.  
 */
void CTcSymObjBase::merge_grammar_entry()
{
    /* if I don't have a grammar list, there's nothing to do */
    if (grammar_entry_ == 0)
        return;

    /* get the grammar production object my rules are associated with */
    CTcSymObj *prod_sym = grammar_entry_->get_prod_sym();

    /* get the master list for the production */
    CTcGramProdEntry *master_entry = G_prs->get_gramprod_entry(prod_sym);

    /* move the alternatives from my private list to the master list */
    grammar_entry_->move_alts_to(master_entry);
}


/* ------------------------------------------------------------------------ */
/*
 *   metaclass symbol base - image/object file functions 
 */

/* 
 *   load from an object file 
 */
int CTcSymMetaclassBase::
   load_from_obj_file(CVmFile *fp, const textchar_t *fname,
                      tctarg_obj_id_t *obj_xlat)
{
    const char *txt;
    size_t len;
    int meta_idx;
    int prop_cnt;
    CTcSymMetaclass *sym;
    char buf[TOK_SYM_MAX_LEN + 1];
    CTcSymMetaProp *prop;
    int was_defined;
    tctarg_obj_id_t class_obj;

    /* read the symbol name */
    if ((txt = base_read_from_sym_file(fp)) == 0)
        return 1;
    len = strlen(txt);

    /* read the metaclass index, class object ID, and property count */
    fp->read_bytes(buf, 8);
    meta_idx = osrp2(buf);
    class_obj = t3rp4u(buf + 2);
    prop_cnt = osrp2(buf + 6);

    /* check for a previous definition */
    sym = (CTcSymMetaclass *)G_prs->get_global_symtab()->find(txt, len);
    if (sym == 0)
    {
        /* it's not defined yet - create the new definition */
        sym = new CTcSymMetaclass(txt, len, FALSE, meta_idx,
                                  G_cg->new_obj_id());
        G_prs->get_global_symtab()->add_entry(sym);

        /* note that it wasn't yet defined */
        was_defined = FALSE;

        /* set the metaclass symbol pointer in the dependency table */
        G_cg->set_meta_sym(meta_idx, sym);
    }
    else if (sym->get_type() != TC_SYM_METACLASS)
    {
        /* log a conflict */
        sym->log_objfile_conflict(fname, TC_SYM_METACLASS);

        /* forget the symbol */
        sym = 0;
        was_defined = FALSE;
    }
    else
    {
        /* if the metaclass index doesn't match, it's an error */
        if (sym->get_meta_idx() != meta_idx)
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_OBJFILE_METACLASS_IDX_CONFLICT,
                                (int)len, txt, fname);

        /* note that it was previously defined */
        was_defined = TRUE;

        /* start with the first property */
        prop = sym->get_prop_head();
    }

    /* set the ID translation for the class object */
    if (sym != 0)
        obj_xlat[class_obj] = sym->get_class_obj();

    /* read the property names */
    for ( ; prop_cnt != 0 ; --prop_cnt)
    {
        int is_static;
        
        /* read the property symbol name */
        if ((txt = base_read_from_sym_file(fp)) == 0)
            return 1;
        len = strlen(txt);

        /* read the flags */
        fp->read_bytes(buf, 1);
        is_static = ((buf[0] & 1) != 0);

        /* check what we're doing */
        if (sym == 0)
        {
            /* 
             *   we have a conflict, so we're just scanning the names to
             *   keep in sync with the file - ignore it 
             */
        }
        else if (was_defined)
        {
            /* 
             *   the metaclass was previously defined - simply check to
             *   ensure that this property matches the corresponding
             *   property (by list position) in the original definition 
             */
            if (prop == 0)
            {
                /* 
                 *   we're past the end of the original definition's
                 *   property list - this is okay, as we can simply add
                 *   the properties in the new list (which must be a more
                 *   recent definition than the original one) 
                 */
                sym->add_prop(txt, len, fname, is_static);
            }
            else if (prop->prop_->get_sym_len() != len
                     || memcmp(prop->prop_->get_sym(), txt, len) != 0)
            {
                /* this one doesn't match - it's an error */
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_OBJFILE_METACLASS_PROP_CONFLICT,
                                    (int)len, txt,
                                    (int)prop->prop_->get_sym_len(),
                                    prop->prop_->get_sym(), fname);
            }

            /* move on to the next property in the list */
            if (prop != 0)
                prop = prop->nxt_;
        }
        else
        {
            /* 
             *   we're defining the metaclass anew - add this property to
             *   the metaclass's property list 
             */
            sym->add_prop(txt, len, fname, is_static);
        }
    }

    /* read our modifier object flag */
    fp->read_bytes(buf, 1);
    if (buf[0] != 0)
    {
        /* laod the new object */
        CTcSymObj *mod_obj;
        
        /* we have a modification object - load it */
        mod_obj = CTcSymObj::load_from_obj_file_modbase(
            fp, fname, obj_xlat, 0, 0, FALSE);

        /* 
         *   if the metaclass already has a modification object, then the
         *   bottom of the chain we just loaded modifies the top of the
         *   existing chain 
         */
        if (sym->get_mod_obj() != 0)
        {
            CTcSymObj *obj;
            CTcSymObj *prv;

            /* 
             *   Set the bottom of the new chain to point to the top of
             *   the existing chain.  The bottom object in each object
             *   file's modification chain is always a dummy root object;
             *   we'll thus find the second to last object in the new
             *   chain, and replace the pointer to its dummy root
             *   superclass with a pointer to the top of the
             *   previously-loaded chain that we're modifying.  
             */

            /* find the second-to-last object in the new chain */
            for (prv = 0, obj = mod_obj ;
                 obj != 0 && obj->get_mod_base_sym() != 0 ;
                 prv = obj, obj = obj->get_mod_base_sym()) ;

            /* 
             *   if we found the second-to-last object, set up the link
             *   back into the old chain 
             */
            if (prv != 0)
                prv->set_mod_base_sym(sym->get_mod_obj());
        }

        /* point the metaclass to the modification object */
        sym->set_mod_obj(mod_obj);
    }

    /* return success - the file appears well-formed */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   property symbol entry base - image/object file functions
 */

/*
 *   Load from an object file 
 */
int CTcSymPropBase::load_from_obj_file(class CVmFile *fp,
                                       const textchar_t *fname,
                                       tctarg_prop_id_t *prop_xlat)
{
    /* read the symbol name information */
    const char *txt;
    if ((txt = base_read_from_sym_file(fp)) == 0)
        return 1;
    size_t len = strlen(txt);

    /* read our property ID */
    ulong id = fp->read_uint4();

    /* 
     *   If this symbol is already defined, make sure the original
     *   definition is a property.  If it's not defined, define it anew.  
     */
    CTcSymProp *sym = (CTcSymProp *)G_prs->get_global_symtab()->find(txt, len);
    if (sym == 0)
    {
        /* 
         *   It's not defined yet - create the new definition and add it
         *   to the symbol table.  Allocate a new property ID for the
         *   symbol in the normal fashion.  
         */
        sym = new CTcSymProp(txt, len, FALSE, G_cg->new_prop_id());
        G_prs->get_global_symtab()->add_entry(sym);
    }
    else if (sym->get_type() != TC_SYM_PROP)
    {
        /* 
         *   It's not already defined as a property - log a symbol type
         *   conflict error 
         */
        sym->log_objfile_conflict(fname, TC_SYM_PROP);

        /* 
         *   proceed despite the error, since this is merely a symbol
         *   conflict and not a file corruption 
         */
        return 0;
    }

    /*
     *   Set the translation table entry for the symbol.  We know the
     *   original ID local to the object file, and we know the new global
     *   property ID.
     */
    prop_xlat[id] = sym->get_prop();

    /* success */
    return 0;
}
                                       
/* ------------------------------------------------------------------------ */
/*
 *   enumerator symbol entry base - image/object file functions 
 */

/*
 *   Load from an object file 
 */
int CTcSymEnumBase::load_from_obj_file(class CVmFile *fp,
                                       const textchar_t *fname,
                                       ulong *enum_xlat)
{
    const char *txt;
    size_t len;
    ulong id;
    CTcSymEnum *sym;
    char buf[32];
    int is_token;

    /* read the symbol name information */
    if ((txt = base_read_from_sym_file(fp)) == 0)
        return 1;
    len = strlen(txt);

    /* read our enumerator ID */
    id = fp->read_uint4();

    /* read our flags */
    fp->read_bytes(buf, 1);

    /* get the 'token' flag */
    is_token = ((buf[0] & 1) != 0);

    /* 
     *   If this symbol is already defined, make sure the original
     *   definition is an enum.  If it's not defined, define it anew.  
     */
    sym = (CTcSymEnum *)G_prs->get_global_symtab()->find(txt, len);
    if (sym == 0)
    {
        /* 
         *   It's not defined yet - create the new definition and add it
         *   to the symbol table.  Allocate a new enumerator ID for the
         *   symbol in the normal fashion.  
         */
        sym = new CTcSymEnum(txt, len, FALSE, G_prs->new_enum_id(), is_token);
        G_prs->get_global_symtab()->add_entry(sym);
    }
    else if (sym->get_type() != TC_SYM_ENUM)
    {
        /* 
         *   It's not already defined as an enumerator - log a symbol type
         *   conflict error 
         */
        sym->log_objfile_conflict(fname, TC_SYM_ENUM);

        /* 
         *   proceed despite the error, since this is merely a symbol
         *   conflict and not a file corruption 
         */
        return 0;
    }

    /*
     *   Set the translation table entry for the symbol.  We know the
     *   original ID local to the object file, and we know the new global
     *   enum ID.  
     */
    enum_xlat[id] = sym->get_enum_id();

    /* success */
    return 0;
}
                                       
/* ------------------------------------------------------------------------ */
/*
 *   Built-in function symbol base - image/object file functions
 */

/* 
 *   load from an object file 
 */
int CTcSymBifBase::load_from_obj_file(class CVmFile *fp,
                                      const textchar_t *fname)
{
    const char *txt;
    size_t len;
    CTcSymBif *sym;
    char buf[10];
    ushort func_set_id;
    ushort func_idx;
    int has_retval;
    int min_argc;
    int max_argc;
    int varargs;

    /* read the symbol name information */
    if ((txt = base_read_from_sym_file(fp)) == 0)
        return 1;
    len = strlen(txt);

    /* read our additional information */
    fp->read_bytes(buf, 10);
    varargs = buf[0];
    has_retval = buf[1];
    min_argc = osrp2(buf+2);
    max_argc = osrp2(buf+4);
    func_set_id = osrp2(buf+6);
    func_idx = osrp2(buf+8);

    /* 
     *   If this symbol is already defined, make sure the new definition
     *   matches the original definition - built-in function sets must be
     *   identical in all object files loaded.  If it's not already
     *   defined, add it now.  
     */
    sym = (CTcSymBif *)G_prs->get_global_symtab()->find(txt, len);
    if (sym == 0)
    {
        /* 
         *   it's not defined yet - create the new definition and add it
         *   to the symbol table 
         */
        sym = new CTcSymBif(txt, len, FALSE, func_set_id, func_idx,
                            has_retval, min_argc, max_argc, varargs);
        G_prs->get_global_symtab()->add_entry(sym);
    }
    else if (sym->get_type() != TC_SYM_BIF)
    {
        /* log the error */
        sym->log_objfile_conflict(fname, TC_SYM_BIF);
    }
    else if (sym->get_func_set_id() != func_set_id
             || sym->get_func_idx() != func_idx
             || sym->get_min_argc() != min_argc
             || sym->get_max_argc() != max_argc
             || sym->is_varargs() != varargs
             || sym->has_retval() != has_retval)
    {
        /* 
         *   this function is already defined but has different settings
         *   -- we cannot reconcile the different usages of the function,
         *   so this is an error 
         */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_OBJFILE_BIF_INCOMPAT,
                            (int)len, txt, fname);
    }
    else
    {
        /* 
         *   everything about the symbol matches - there's no need to
         *   redefine the symbol, since it's already set up exactly as we
         *   need it to be 
         */
    }

    /* continue reading the file */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Grammar production list entry
 */

/* 
 *   load from an object file 
 */
void CTcGramProdEntry::load_from_obj_file(
    CVmFile *fp, const tctarg_prop_id_t *prop_xlat, const ulong *enum_xlat,
    CTcSymObj *private_owner)
{
    uint idx;
    ulong cnt;
    CTcSymObj *obj;
    CTcGramProdEntry *prod;
    ulong flags;

    /* 
     *   read the object file index of the production object, and get the
     *   production object 
     */
    idx = fp->read_uint4();
    obj = G_prs->get_objfile_objsym(idx);

    /* declare the production object */
    prod = G_prs->declare_gramprod(obj->get_sym(), obj->get_sym_len());

    /* if we have a private owner, create a private rule list */
    if (private_owner != 0)
        prod = private_owner->create_grammar_entry(
            obj->get_sym(), obj->get_sym_len());

    /* read the flags */
    flags = fp->read_uint4();

    /* set the explicitly-declared flag if appropriate */
    if (flags & 1)
        prod->set_declared(TRUE);

    /* read the alternative count */
    cnt = fp->read_uint4();

    /* read the alternatives */
    for ( ; cnt != 0 ; --cnt)
    {
        CTcGramProdAlt *alt;

        /* read an alternative */
        alt = CTcGramProdAlt::load_from_obj_file(fp, prop_xlat, enum_xlat);

        /* add it to the production's list */
        if (prod != 0)
            prod->add_alt(alt);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Grammar production alternative
 */

/* 
 *   load from an object file 
 */
CTcGramProdAlt *CTcGramProdAlt::
   load_from_obj_file(CVmFile *fp, const tctarg_prop_id_t *prop_xlat,
                      const ulong *enum_xlat)
{
    uint idx;
    ulong cnt;
    CTcSymObj *obj;
    CTcGramProdAlt *alt;
    CTcDictEntry *dict;
    int score;
    int badness;

    /* read my score and badness */
    score = fp->read_int2();
    badness = fp->read_int2();

    /* read my processor object index, and get the associated object */
    idx = fp->read_uint4();
    obj = G_prs->get_objfile_objsym(idx);

    /* read my dictionary object index, and get the associated entry */
    idx = fp->read_uint4();
    dict = G_prs->get_obj_dict(idx);

    /* create the alternative object */
    alt = new (G_prsmem) CTcGramProdAlt(obj, dict);

    /* set the score badness */
    alt->set_score(score);
    alt->set_badness(badness);

    /* read the number of tokens */
    cnt = fp->read_uint4();

    /* read the tokens */
    for ( ; cnt != 0 ; --cnt)
    {
        CTcGramProdTok *tok;

        /* read a token */
        tok = CTcGramProdTok::load_from_obj_file(fp, prop_xlat, enum_xlat);

        /* add it to the alternative's list */
        alt->add_tok(tok);
    }

    /* return the alternative */
    return alt;
}


/* ------------------------------------------------------------------------ */
/*
 *   Grammar production token
 */

/* 
 *   load from an object file 
 */
CTcGramProdTok *CTcGramProdTok::
   load_from_obj_file(CVmFile *fp, const tctarg_prop_id_t *prop_xlat,
                      const ulong *enum_xlat)
{
    CTcGramProdTok *tok;
    CTcSymObj *obj;
    tcgram_tok_type typ;
    tctarg_prop_id_t prop;
    size_t len;
    char *txt;
    uint idx;
    ulong enum_id;
    size_t i;

    /* create a new token */
    tok = new (G_prsmem) CTcGramProdTok();

    /* read the type */
    typ = (tcgram_tok_type)fp->read_int2();

    /* read the data, which depends on the type */
    switch(typ)
    {
    case TCGRAM_PROD:
        /* read the production object's object file index */
        idx = fp->read_uint4();

        /* translate it to an object */
        obj = G_prs->get_objfile_objsym(idx);

        /* set the production object in the token */
        tok->set_match_prod(obj);
        break;

    case TCGRAM_TOKEN_TYPE:
        /* read the token ID, translating to the new enum numbering */
        enum_id = enum_xlat[fp->read_uint4()];

        /* set the token-type match */
        tok->set_match_token_type(enum_id);
        break;

    case TCGRAM_PART_OF_SPEECH:
        /* read the property ID, translating to the new numbering system */
        prop = prop_xlat[fp->read_int2()];

        /* set the part of speech in the token */
        tok->set_match_part_of_speech(prop);
        break;

    case TCGRAM_PART_OF_SPEECH_LIST:
        /* read the list length */
        len = (size_t)fp->read_int2();

        /* set the type */
        tok->set_match_part_list();

        /* read each element and add it to the list */
        for (i = 0 ; i < len ; ++i)
            tok->add_match_part_ele(prop_xlat[fp->read_int2()]);

        /* done */
        break;

    case TCGRAM_LITERAL:
        /* read the length of the string */
        len = (size_t)fp->read_int2();

        /* allocate parser memory to hold the text */
        txt = (char *)G_prsmem->alloc(len);

        /* read the text of the literal */
        fp->read_bytes(txt, len);

        /* set the literal in the token */
        tok->set_match_literal(txt, len);
        break;

    case TCGRAM_STAR:
        /* there's no additional data */
        tok->set_match_star();
        break;

    case TCGRAM_UNKNOWN:
        /* no extra data to read */
        break;
    }

    /* read and set the property association */
    tok->set_prop_assoc(prop_xlat[fp->read_int2()]);

    /* return the token */
    return tok;
}
