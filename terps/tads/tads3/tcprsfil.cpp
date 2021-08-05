#ifdef RCSID
static char RCSid[] =
"";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcprssym.cpp - TADS 3 Compiler Parser, symbol file operations
Function
  This parser module contains code that manages symbol and object files.
  These routines aren't needed in interpreter builds with dynamic compilation
  ("eval()" support), so we isolate them out for separate linkage.
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
 *   main parser symbol and object file operations 
 */

/*
 *   Add a symbol loaded from an object file 
 */
void CTcParser::add_sym_from_obj_file(uint idx, class CTcSymbol *sym)
{
    /* 
     *   add the entry to the object file index list - adjust from the
     *   1-based index used in the file to an array index 
     */
    obj_sym_list_[idx - 1] = sym;
}

/*
 *   Get an object file symbol, ensuring that it's an object symbol 
 */
CTcSymObj *CTcParser::get_objfile_objsym(uint idx)
{
    CTcSymObj *sym;

    /* get the object based on the index */
    sym = (CTcSymObj *)get_objfile_sym(idx);

    /* make sure it's an object - if it isn't, return null */
    if (sym == 0 || sym->get_type() != TC_SYM_OBJ)
        return 0;

    /* it checks out - return it */
    return sym;
}


/*
 *   Add a dictionary symbol loaded from an object file 
 */
void CTcParser::add_dict_from_obj_file(CTcSymObj *sym)
{
    CTcDictEntry *entry;

    /* get the current entry, if any */
    entry = get_dict_entry(sym);

    /* if there's no current entry, create a new one */
    if (entry == 0)
    {
        /* create the entry */
        entry = create_dict_entry(sym);
    }

    /* add the entry to the object file index list */
    obj_dict_list_[obj_file_dict_idx_++] = entry;
}



/* ------------------------------------------------------------------------ */
/*
 *   Base symbol class
 */

/*
 *   Write to a symbol file.  
 */
int CTcSymbolBase::write_to_sym_file(CVmFile *fp)
{
    /* do the basic writing */
    return write_to_file_gen(fp);
}

/*
 *   Write to a file.  This is a generic base routine that can be used for
 *   writing to a symbol or object file.  
 */
int CTcSymbolBase::write_to_file_gen(CVmFile *fp)
{
    /* write my type */
    fp->write_uint2((int)get_type());

    /* write my name */
    return write_name_to_file(fp);
}

/*
 *   Write the symbol name to a file 
 */
int CTcSymbolBase::write_name_to_file(CVmFile *fp)
{
    /* write the length of my symbol name, followed by the symbol name */
    fp->write_uint2((int)get_sym_len());

    /* write the symbol string */
    fp->write_bytes(get_sym(), get_sym_len());

    /* we wrote the symbol */
    return TRUE;
}

/*
 *   Read a symbol from a symbol file 
 */
CTcSymbol *CTcSymbolBase::read_from_sym_file(CVmFile *fp)
{
    /* 
     *   read the type - this is the one thing we know is always present for
     *   every symbol (the rest of the data might vary per subclass) 
     */
    tc_symtype_t typ = (tc_symtype_t)fp->read_uint2();

    /* create the object based on the type */
    switch(typ)
    {
    case TC_SYM_FUNC:
        return CTcSymFunc::read_from_sym_file(fp);

    case TC_SYM_OBJ:
        return CTcSymObj::read_from_sym_file(fp);

    case TC_SYM_PROP:
        return CTcSymProp::read_from_sym_file(fp);

    case TC_SYM_ENUM:
        return CTcSymEnum::read_from_sym_file(fp);

    case TC_SYM_METACLASS:
        return CTcSymMetaclass::read_from_sym_file(fp);

    default:
        /* other types should not be in a symbol file */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_SYMEXP_INV_TYPE);
        return 0;
    }
}

/*
 *   Read the basic information from the symbol file 
 */
const char *CTcSymbolBase::base_read_from_sym_file(CVmFile *fp)
{
    char buf[TOK_SYM_MAX_LEN + 1];

    /* read, null-terminate, and return the string */
    return CTcParser::read_len_prefix_str(fp, buf, sizeof(buf), 0,
                                          TCERR_SYMEXP_SYM_TOO_LONG);
}

/*
 *   Write to an object file.  
 */
int CTcSymbolBase::write_to_obj_file(CVmFile *fp)
{
    /* do the basic writing */
    return write_to_file_gen(fp);
}


/* ------------------------------------------------------------------------ */
/*
 *   Function symbols 
 */


/*
 *   Write to a symbol file 
 */
int CTcSymFuncBase::write_to_sym_file(CVmFile *fp)
{
    char buf[7];
    CTcSymFunc *cur;
    int ext_modify;

    /* scan for the bottom of our modify stack */
    for (cur = get_mod_base() ; cur != 0 && cur->get_mod_base() != 0 ;
         cur = cur->get_mod_base()) ;

    /* we modify an external if the bottom of our modify stack is external */
    ext_modify = (cur != 0 && cur->is_extern());

    /* 
     *   If we're external, don't bother writing to the file - if we're
     *   importing a function, we don't want to export it as well.  Note that
     *   a function that is replacing or modifying an external function is
     *   fundamentally external itself, because the function must be defined
     *   in another file to be replaceable/modifiable.
     *   
     *   As an exception, if this is a multi-method base symbol, and a
     *   multi-method with this name is defined in this file, export it even
     *   though it's technically an extern symbol.  We don't export most
     *   extern symbols because we count on the definer to export them, but
     *   in the case of multi-method base symbols, there is no definer - the
     *   base symbol is basically a placeholder to be filled in by the
     *   linker.  So *someone* has to export these.  The logical place to
     *   export them is from any file that defines a multi-method based on
     *   the base symbol.  
     */
    if ((is_extern_ || ext_replace_ || ext_modify) && !mm_def_)
        return FALSE;

    /* inherit default */
    CTcSymbol::write_to_sym_file(fp);

    /* write our argument count, varargs flag, and return value flag */
    oswp2(buf, argc_);
    oswp2(buf + 2, opt_argc_);
    buf[4] = (varargs_ != 0);
    buf[5] = (has_retval_ != 0);
    buf[6] = (is_multimethod_ ? 1 : 0)
             | (is_multimethod_base_ ? 2 : 0);
    fp->write_bytes(buf, 7);

    /* we wrote the symbol */
    return TRUE;
}

/*
 *   Read from a symbol file 
 */
CTcSymbol *CTcSymFuncBase::read_from_sym_file(CVmFile *fp)
{
    char symbuf[4096];
    const char *sym;
    char info[7];
    int argc;
    int opt_argc;
    int varargs;
    int has_retval;
    int is_multimethod, is_multimethod_base;

    /* 
     *   Read the symbol name.  Use a custom reader instead of the base
     *   reader, because function symbols can be quite long, due to
     *   multimethod name decoration.  
     */
    if ((sym = CTcParser::read_len_prefix_str(
        fp, symbuf, sizeof(symbuf), 0, TCERR_SYMEXP_SYM_TOO_LONG)) == 0)
        return 0;

    /* 
     *   read the argument & optional argument count, varargs flag, and
     *   return value flag 
     */
    fp->read_bytes(info, 7);
    argc = osrp2(info);
    opt_argc = osrp2(info + 2);
    varargs = (info[4] != 0);
    has_retval = (info[5] != 0);
    is_multimethod = ((info[6] & 1) != 0);
    is_multimethod_base = ((info[6] & 2) != 0);

    /* create and return the new symbol */
    return new CTcSymFunc(sym, strlen(sym), FALSE, argc, opt_argc,
                          varargs, has_retval,
                          is_multimethod, is_multimethod_base, TRUE, TRUE);
}

/*
 *   Write to an object file 
 */
int CTcSymFuncBase::write_to_obj_file(CVmFile *fp)
{
    char buf[12];
    CTcSymFunc *cur;
    CTcSymFunc *last_mod;
    int mod_body_cnt;
    int ext_modify;

    /* 
     *   If it's external, and we have no fixups, don't bother writing it to
     *   the object file.  If there are no fixups, we don't have any
     *   references to the function, hence there's no need to include it in
     *   the object file.  
     */
    if (is_extern_ && fixups_ == 0)
        return FALSE;

    /*
     *   If we have a modified base function, scan down the chain of modified
     *   bases until we reach the last one.  If it's external, we need to
     *   note this, and we need to store the fixup list for the external
     *   symbol so that we can explicitly link it to the imported symbol at
     *   link time.  
     */
    for (mod_body_cnt = 0, last_mod = 0, cur = get_mod_base() ; cur != 0 ;
         last_mod = cur, cur = cur->get_mod_base())
    {
        /* if this one has an associated code body, count it */
        if (cur->get_code_body() != 0 && !cur->get_code_body()->is_replaced())
            ++mod_body_cnt;
    }

    /* we modify an external if the last in the modify chain is external */
    ext_modify = (last_mod != 0 && last_mod->is_extern());

    /* inherit default */
    CTcSymbol::write_to_obj_file(fp);

    /* 
     *   write our argument count, varargs flag, return value, extern flags,
     *   and the number of our modified base functions with code bodies 
     */
    oswp2(buf, argc_);
    oswp2(buf + 2, opt_argc_);
    buf[4] = (varargs_ != 0);
    buf[5] = (has_retval_ != 0);
    buf[6] = (is_extern_ != 0);
    buf[7] = (ext_replace_ != 0);
    buf[8] = (ext_modify != 0);
    buf[9] = (is_multimethod_ ? 1 : 0)
             | (is_multimethod_base_ ? 2 : 0);
    oswp2(buf + 10, mod_body_cnt);
    fp->write_bytes(buf, 12);

    /* if we modify an external, save its fixup list */
    if (ext_modify)
        CTcAbsFixup::write_fixup_list_to_object_file(fp, last_mod->fixups_);

    /* write the code stream offsets of the modified base function bodies */
    for (cur = get_mod_base() ; cur != 0 ; cur = cur->get_mod_base())
    {
        /* if this one has a code body, write its code stream offset */
        if (cur->get_code_body() != 0)
            fp->write_uint4(cur->get_anchor()->get_ofs());
    }

    /* 
     *   If we're defined as external, write our fixup list.  Since this
     *   is an external symbol, it will have no anchor in the code stream,
     *   hence we need to write our fixup list with the symbol and not
     *   with the anchor.  
     */
    if (is_extern_)
        CTcAbsFixup::write_fixup_list_to_object_file(fp, fixups_);

    /* we wrote the symbol */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Object symbols 
 */

/*
 *   Write to a symbol file 
 */
int CTcSymObjBase::write_to_sym_file(CVmFile *fp)
{
    int result;

    /* 
     *   If we're external, don't bother writing to the file - if we're
     *   importing an object, we don't want to export it as well.  If it's
     *   modified, don't write it either, because modified symbols cannot be
     *   referenced directly by name (the symbol for a modified object is a
     *   fake symbol anyway).  In addition, don't write the symbol if it's a
     *   'modify' or 'replace' definition that applies to an external base
     *   object - instead, we'll pick up the symbol from the other symbol
     *   file with the original definition.  
     */
    if (is_extern_ || modified_ || ext_modify_ || ext_replace_)
        return FALSE;

    /* inherit default */
    result =  CTcSymbol::write_to_sym_file(fp);

    /* if that was successful, write additional object-type-specific data */
    if (result)
    {
        /* write the metaclass ID */
        fp->write_uint2((int)metaclass_);

        /* if it's of metaclass tads-object, write superclass information */
        if (metaclass_ == TC_META_TADSOBJ)
        {
            char c;
            size_t cnt;
            CTPNSuperclass *sc;

            /* 
             *   set up our flags: indicate whether or not we're explicitly
             *   based on the root object class, and if we're a 'class'
             *   object 
             */
            c = ((sc_is_root() ? 1 : 0)
                 | (is_class() ? 2 : 0)
                 | (is_transient() ? 4 : 0));
            fp->write_bytes(&c, 1);

            /* count the declared superclasses */
            for (cnt = 0, sc = sc_name_head_ ; sc != 0 ;
                 sc = sc->nxt_, ++cnt) ;

            /* 
             *   write the number of declared superclasses followed by the
             *   names of the superclasses 
             */
            fp->write_uint2(cnt);
            for (sc = sc_name_head_ ; sc != 0 ; sc = sc->nxt_)
            {
                /* write the counted-length identifier */
                fp->write_uint2(sc->get_sym_len());
                fp->write_bytes(sc->get_sym_txt(), sc->get_sym_len());
            }
        }
    }

    /* return the result */
    return result;
}

/*
 *   Read from a symbol file 
 */
CTcSymbol *CTcSymObjBase::read_from_sym_file(CVmFile *fp)
{
    const char *txt;
    tc_metaclass_t meta;
    CTcSymObj *sym;
    char c;
    size_t cnt;
    size_t i;

    /* read the symbol name */
    if ((txt = base_read_from_sym_file(fp)) == 0)
        return 0;

    /* read the metaclass ID */
    meta = (tc_metaclass_t)fp->read_uint2();

    /* 
     *   If it's a dictionary object, check to see if it's already defined -
     *   a dictionary object can be exported from multiple modules without
     *   error, since dictionaries are shared across modules.
     *   
     *   The same applies to grammar productions, since a grammar production
     *   can be implicitly created in multiple files.  
     */
    if (meta == TC_META_DICT || meta == TC_META_GRAMPROD)
    {
        /* look for a previous instance of the symbol */
        CTcSymbol *old_sym =
            G_prs->get_global_symtab()->find(txt, strlen(txt));
        if (old_sym != 0
            && old_sym->get_type() == TC_SYM_OBJ
            && ((CTcSymObj *)old_sym)->get_metaclass() == meta)
        {
            /* 
             *   the dictionary is already in the symbol table - return the
             *   existing one, since there's no conflict with importing the
             *   dictionary from multiple places 
             */
            return old_sym;
        }
    }

    /* create the new symbol */
    sym = new CTcSymObj(txt, strlen(txt), FALSE, G_cg->new_obj_id(),
                        TRUE, meta, 0);

    /* if the metaclass is tads-object, read additional information */
    if (meta == TC_META_TADSOBJ)
    {
        /* read the root-object-superclass flag and the class-object flag */
        fp->read_bytes(&c, 1);
        sym->set_sc_is_root((c & 1) != 0);
        sym->set_is_class((c & 2) != 0);
        if ((c & 4) != 0)
            sym->set_transient();

        /* read the number of superclasses, and read the superclass names */
        cnt = fp->read_uint2();
        for (i = 0 ; i < cnt ; ++i)
        {
            char buf[TOK_SYM_MAX_LEN + 1];
            const char *sc_txt;
            size_t sc_len;

            /* read the symbol */
            sc_txt = CTcParser::read_len_prefix_str(
                fp, buf, sizeof(buf), &sc_len, TCERR_SYMEXP_SYM_TOO_LONG);

            /* add the superclass list entry to the symbol */
            sym->add_sc_name_entry(sc_txt, sc_len);
        }
    }

    /* return the symbol */
    return sym;
}

/*
 *   Write to a object file 
 */
int CTcSymObjBase::write_to_obj_file(CVmFile *fp)
{
    /* 
     *   If the object is external and has never been referenced, don't
     *   bother writing it.
     *   
     *   In addition, if the object is marked as modified, don't write it.
     *   We write modified base objects specially, because we must control
     *   the order in which a modified base object is written relative its
     *   modifying object.  
     */
    if ((is_extern_ && !ref_) || modified_)
        return FALSE;

    /* if the object has already been written, don't write it again */
    if (written_to_obj_)
    {
        /* 
         *   if we've never been counted in the object file before, we must
         *   have been written indirectly in the course of writing another
         *   symbol - in this case, return true to indicate that we are in
         *   the file, even though we're not actually writing anything now 
         */
        if (!counted_in_obj_)
        {
            /* we've now been counted in the object file */
            counted_in_obj_ = TRUE;

            /* indicate that we have been written */
            return TRUE;
        }
        else
        {
            /* we've already been written and counted - don't write again */
            return FALSE;
        }
    }

    /* do the main part of the writing */
    return write_to_obj_file_main(fp);
}

/*
 *   Write the object symbol to an object file.  This main routine does most
 *   of the actual work, once we've decided that we're actually going to
 *   write the symbol.  
 */
int CTcSymObjBase::write_to_obj_file_main(CVmFile *fp)
{
    /* take the next object file index */
    set_obj_file_idx(G_prs->get_next_obj_file_sym_idx());

    /* 
     *   if I have a dictionary object, make sure it's in the object file
     *   before I am - we need to be able to reference the object during
     *   load, so it has to be written before me 
     */
    if (dict_ != 0)
        dict_->write_sym_to_obj_file(fp);

    /* 
     *   if I'm not anonymous, write the basic header information for the
     *   symbol (don't do this for anonymous objects, since they don't have a
     *   name to write) 
     */
    if (!anon_)
        write_to_file_gen(fp);

    /* 
     *   write my object ID, so that we can translate from the local
     *   numbering system in the object file to the new numbering system in
     *   the image file 
     */
    char buf[32];
    oswp4(buf, obj_id_);

    /* write the flags */
    buf[4] = (is_extern_ != 0);
    buf[5] = (ext_replace_ != 0);
    buf[6] = (modified_ != 0);
    buf[7] = (mod_base_sym_ != 0);
    buf[8] = (ext_modify_ != 0);
    buf[9] = (obj_stm_ != 0 && obj_stm_->is_class());
    buf[10] = (transient_ != 0);

    /* add the metaclass type */
    oswp2(buf + 11, (int)metaclass_);

    /* add the dictionary's object file index, if we have one */
    if (dict_ != 0)
        oswp2(buf + 13, dict_->get_obj_idx());
    else
        oswp2(buf + 13, 0);

    /* 
     *   add my object file index (we store this to eliminate any dependency
     *   on the load order - this allows us to write other symbols
     *   recursively without worrying about exactly where the recursion
     *   occurs relative to assigning the file index) 
     */
    oswp2(buf + 15, get_obj_file_idx());

    /* write the data to the file */
    fp->write_bytes(buf, 17);

    /* if we're not external, write our stream address */
    if (!is_extern_)
        fp->write_uint4(stream_ofs_);

    /* if we're modifying another object, store some extra information */
    if (mod_base_sym_ != 0)
    {
        /* 
         *   Write our list of properties to be deleted from base objects at
         *   link time.  First, count the properties in the list.  
         */
        CTcObjPropDel *delprop;
        int cnt;
        for (cnt = 0, delprop = first_del_prop_ ; delprop != 0 ;
             ++cnt, delprop = delprop->nxt_) ;

        /* write the count */
        fp->write_uint2(cnt);

        /* write the deleted property list */
        for (delprop = first_del_prop_ ; delprop != 0 ;
             delprop = delprop->nxt_)
        {
            /* 
             *   write out this property symbol (we write the symbol rather
             *   than the ID, because when we load the object file, we'll
             *   need to adjust the ID to new global numbering system in the
             *   image file; the easiest way to do this is to write the
             *   symbol and look it up at load time) 
             */
            fp->write_uint2(delprop->prop_sym_->get_sym_len());
            fp->write_bytes(delprop->prop_sym_->get_sym(),
                            delprop->prop_sym_->get_sym_len());
        }
    }

    /* write our self-reference fixup list */
    CTcIdFixup::write_to_object_file(fp, fixups_);

    /*
     *   If this is a modifying object, we must write the entire chain of
     *   modified base objects immediately after this object.  When we're
     *   reading the symbol table, this ensures that we can read each
     *   modified base object recursively as we read its modifiers, which is
     *   necessary so that we can build up the same modification chain on
     *   loading the object file.  
     */
    if (mod_base_sym_ != 0)
    {
        /* write the main part of the definition */
        mod_base_sym_->write_to_obj_file_main(fp);
    }

    /* mark the object as written to the file */
    written_to_obj_ = TRUE;

    /* written */
    return TRUE;
}

/*
 *   Write a modified object to an object file 
 */
int CTcSymObjBase::write_to_obj_file_as_modified(class CVmFile *fp)
{
    return write_to_obj_file_main(fp);
}


/*
 *   Write cross-references to the object file 
 */
int CTcSymObjBase::write_refs_to_obj_file(CVmFile *fp)
{
    CTPNSuperclass *sc;
    uint cnt;
    long cnt_pos;
    long end_pos;
    CTcVocabEntry *voc;

    /* 
     *   if this symbol wasn't written to the object file in the first place,
     *   we obviously don't want to include any extra data for it 
     */
    if (!written_to_obj_)
        return FALSE;

    /* write my symbol index */
    fp->write_uint4(get_obj_file_idx());

    /* write a placeholder superclass count */
    cnt_pos = fp->get_pos();
    fp->write_uint2(0);

    /* write my superclass list */
    for (sc = (obj_stm_ != 0 ? obj_stm_->get_first_sc() : 0), cnt = 0 ;
         sc != 0 ; sc = sc->nxt_)
    {
        CTcSymObj *sym;

        /* look up this superclass symbol */
        sym = (CTcSymObj *)sc->get_sym();
        if (sym != 0 && sym->get_type() == TC_SYM_OBJ)
        {
            /* write the superclass symbol index */
            fp->write_uint4(sym->get_obj_file_idx());

            /* count it */
            ++cnt;
        }
    }

    /* go back and write the superclass count */
    end_pos = fp->get_pos();
    fp->set_pos(cnt_pos);
    fp->write_uint2(cnt);
    fp->set_pos(end_pos);

    /* count my vocabulary words */
    for (cnt = 0, voc = vocab_ ; voc != 0 ; ++cnt, voc = voc->nxt_) ;

    /* write my vocabulary words */
    fp->write_uint2(cnt);
    for (voc = vocab_ ; voc != 0 ; voc = voc->nxt_)
    {
        /* write the text of the word */
        fp->write_uint2(voc->len_);
        fp->write_bytes(voc->txt_, voc->len_);

        /* write the property ID */
        fp->write_uint2(voc->prop_);
    }

    /* indicate that we wrote the symbol */
    return TRUE;
}

/*
 *   Load references from the object file 
 */
void CTcSymObjBase::load_refs_from_obj_file(CVmFile *fp, const char *,
                                            tctarg_obj_id_t *,
                                            tctarg_prop_id_t *prop_xlat)
{
    uint i;
    uint cnt;
    CTcObjScEntry *sc_tail;

    /* read the superclass count */
    cnt = fp->read_uint2();

    /* read the superclass list */
    for (sc_tail = 0, i = 0 ; i < cnt ; ++i)
    {
        ulong idx;
        CTcSymObj *sym;
        CTcObjScEntry *sc;

        /* read the next index */
        idx = fp->read_uint4();

        /* get the symbol */
        sym = (CTcSymObj *)G_prs->get_objfile_sym(idx);
        if (sym->get_type() != TC_SYM_OBJ)
            sym = 0;

        /* create a new list entry */
        sc = new (G_prsmem) CTcObjScEntry(sym);

        /* link it in at the end of the my superclass list */
        if (sc_tail != 0)
            sc_tail->nxt_ = sc;
        else
            sc_ = sc;

        /* this is now the last entry in my superclass list */
        sc_tail = sc;
    }

    /* load the vocabulary words */
    cnt = fp->read_uint2();
    for (i = 0 ; i < cnt ; ++i)
    {
        size_t len;
        char *txt;
        tctarg_prop_id_t prop;

        /* read the length of this word's text */
        len = fp->read_uint2();

        /* allocate parser memory for the word's text */
        txt = (char *)G_prsmem->alloc(len);

        /* read the word into the allocated text buffer */
        fp->read_bytes(txt, len);

        /* read the property */
        prop = (tctarg_prop_id_t)fp->read_uint2();

        /* translate the property to the new numbering system */
        prop = prop_xlat[prop];

        /* add the word to our vocabulary */
        add_vocab_word(txt, len, prop);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Property symbols 
 */

/*
 *   Write to a symbol file 
 */
int CTcSymPropBase::write_to_sym_file(CVmFile *fp)
{
    /* inherit the default handling */
    int ret = CTcSymbol::write_to_sym_file(fp);
    if (ret)
    {
        /* encode and write our flags */
        char flags = 0;
        if (ref_)
            flags |= 1;
        if (vocab_)
            flags |= 2;
        if (weak_)
            flags |= 4;
        
        fp->write_byte(flags);
    }

    return ret;
}

/*
 *   Read from a symbol file 
 */
CTcSymbol *CTcSymPropBase::read_from_sym_file(CVmFile *fp)
{
    /* read the symbol name */
    const char *sym;
    if ((sym = base_read_from_sym_file(fp)) == 0)
        return 0;

    /* read and decode the flags of interest */
    char flags = fp->read_byte();
    int weak = flags & 4;

    /* 
     *   If this property is already defined, this is a harmless redefinition
     *   - every symbol file can define the same property without any
     *   problem.  Indicate the harmless redefinition by returning the
     *   original symbol.  
     */
    CTcSymbol *old_entry = G_prs->get_global_symtab()->find(sym, strlen(sym));
    if (old_entry != 0 && old_entry->get_type() == TC_SYM_PROP)
        return old_entry;

    /* 
     *   if there's an old entry of a different type, and our entry is weak,
     *   the other symbol overrides our property definition 
     */
    if (old_entry != 0 && weak)
        return old_entry;

    /* create and return the new symbol */
    CTcSymProp *prop_entry =
        new CTcSymProp(sym, strlen(sym), FALSE, G_cg->new_prop_id());

    /* if our definition is weak, record that */
    if (weak)
        prop_entry->set_weak(TRUE);

    /* return the new entry */
    return prop_entry;
}

/*
 *   Write to an object file 
 */
int CTcSymPropBase::write_to_obj_file(CVmFile *fp)
{
    /* 
     *   If the property has never been referenced, don't bother writing it.
     *   We must have picked up the definition from an external symbol set we
     *   loaded but have no references of our own to the property.  
     */
    if (!ref_)
        return FALSE;

    /* inherit default */
    CTcSymbol::write_to_obj_file(fp);

    /* 
     *   write my local property ID value - when we load the object file,
     *   we'll need to figure out the translation from our original numbering
     *   system to the new numbering system used in the final image file 
     */
    fp->write_uint4((ulong)prop_);

    /* written */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Enumerator symbol 
 */


/*
 *   Write to a symbol file 
 */
int CTcSymEnumBase::write_to_sym_file(CVmFile *fp)
{
    int result;
    char buf[32];

    /* inherit default */
    result =  CTcSymbol::write_to_sym_file(fp);

    /* write the 'token' flag */
    if (result)
    {
        /* clear the flags */
        buf[0] = 0;

        /* set the 'token' flag if appropriate */
        if (is_token_)
            buf[0] |= 1;

        /* write the flags */
        fp->write_bytes(buf, 1);
    }

    /* return the result */
    return result;
}

/*
 *   Read from a symbol file 
 */
CTcSymbol *CTcSymEnumBase::read_from_sym_file(CVmFile *fp)
{
    const char *sym;
    CTcSymEnum *old_entry;
    char buf[32];
    int is_token;

    /* read the symbol name */
    if ((sym = base_read_from_sym_file(fp)) == 0)
        return 0;

    /* read the 'token' flag */
    fp->read_bytes(buf, 1);
    is_token = ((buf[0] & 1) != 0);

    /* 
     *   If this enumerator is already defined, this is a harmless
     *   redefinition - every symbol file can define the same enumerator
     *   without any problem.  Indicate the harmless redefinition by
     *   returning the original symbol.  
     */
    old_entry = (CTcSymEnum *)
                G_prs->get_global_symtab()->find(sym, strlen(sym));
    if (old_entry != 0 && old_entry->get_type() == TC_SYM_ENUM)
    {
        /* if this is a 'token' enum, mark the old entry as such */
        if (is_token)
            old_entry->set_is_token(TRUE);

        /* return the original entry */
        return old_entry;
    }

    /* create and return the new symbol */
    return new CTcSymEnum(sym, strlen(sym), FALSE,
                          G_prs->new_enum_id(), is_token);
}

/*
 *   Write to an object file 
 */
int CTcSymEnumBase::write_to_obj_file(CVmFile *fp)
{
    char buf[32];

    /* 
     *   If the enumerator has never been referenced, don't bother writing
     *   it.  We must have picked up the definition from an external symbol
     *   set we loaded but have no references of our own to the enumerator.  
     */
    if (!ref_)
        return FALSE;

    /* inherit default */
    CTcSymbol::write_to_obj_file(fp);

    /* 
     *   write my local enumerator ID value - when we load the object file,
     *   we'll need to figure out the translation from our original numbering
     *   system to the new numbering system used in the image file 
     */
    fp->write_uint4((ulong)enum_id_);

    /* clear the flags */
    buf[0] = 0;

    /* set the 'token' flag if appropriate */
    if (is_token_)
        buf[0] |= 1;

    /* write the flags */
    fp->write_bytes(buf, 1);

    /* written */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Built-in function symbol base 
 */

/*
 *   Write to a object file 
 */
int CTcSymBifBase::write_to_obj_file(CVmFile *fp)
{
    char buf[10];

    /* inherit default */
    CTcSymbol::write_to_obj_file(fp);

    /* write the varargs and return value flags */
    buf[0] = (varargs_ != 0);
    buf[1] = (has_retval_ != 0);

    /* write the argument count information */
    oswp2(buf+2, min_argc_);
    oswp2(buf+4, max_argc_);

    /* 
     *   write the function set ID and index - these are required to match
     *   those used in all other object files that make up a single image
     *   file 
     */
    oswp2(buf+6, func_set_id_);
    oswp2(buf+8, func_idx_);
    fp->write_bytes(buf, 10);

    /* written */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Metaclass
 */

/* 
 *   write some additional data to the object file 
 */
int CTcSymMetaclassBase::write_to_obj_file(class CVmFile *fp)
{
    CTcSymMetaProp *cur;
    char buf[16];

    /* if this is an unreferenced external symbol, don't write it */
    if (ext_ && !ref_)
        return FALSE;

    /* inherit default */
    CTcSymbol::write_to_obj_file(fp);

    /* write my metaclass index, class object ID, and property count */
    fp->write_uint2(meta_idx_);
    fp->write_uint4(class_obj_);
    fp->write_uint2(prop_cnt_);

    /* write my property symbol list */
    for (cur = prop_head_ ; cur != 0 ; cur = cur->nxt_)
    {
        /* write this symbol name */
        fp->write_uint2(cur->prop_->get_sym_len());
        fp->write_bytes(cur->prop_->get_sym(), cur->prop_->get_sym_len());

        /* set up the flags */
        buf[0] = 0;
        if (cur->is_static_)
            buf[0] |= 1;

        /* write the flags */
        fp->write_bytes(buf, 1);
    }

    /* write our modifying object flag */
    buf[0] = (mod_obj_ != 0);
    fp->write_bytes(buf, 1);

    /* if we have a modifier object chain, write it out */
    if (mod_obj_ != 0)
        mod_obj_->write_to_obj_file_as_modified(fp);

    /* written */
    return TRUE;
}

/*
 *   Write to a symbol file 
 */
int CTcSymMetaclassBase::write_to_sym_file(class CVmFile *fp)
{
    /* inherit the base class handling to write the symbol */
    CTcSymbol::write_to_sym_file(fp);

    /* add the metaclass ID string */
    const char *id = G_cg->get_meta_name(meta_idx_);
    size_t idlen = strlen(id);
    fp->write_byte((char)idlen);
    fp->write_bytes(id, idlen);

    /* success */
    return TRUE; 
}

/*
 *   Read from a symbol file 
 */
CTcSymbol *CTcSymMetaclassBase::read_from_sym_file(class CVmFile *fp)
{
    /* read the symbol name */
    const char *txt;
    if ((txt = base_read_from_sym_file(fp)) == 0)
        return 0;

    /* read the metaclass ID string */
    char id[256];
    size_t idlen = (uchar)fp->read_byte();
    fp->read_bytes(id, idlen);

    /* if the symbol is already defined, return the existing copy */
    CTcSymbol *old_sym = G_prs->get_global_symtab()->find(txt, strlen(txt));
    if (old_sym != 0 && old_sym->get_type() == TC_SYM_METACLASS)
        return old_sym;

    /* create a new external metaclass symbol */
    CTcSymMetaclass *sym = new CTcSymMetaclass(
        txt, strlen(txt), FALSE, 0, G_cg->new_obj_id());

    /* mark it as external */
    sym->set_ext(TRUE);

    /* add it to the metaclass table */
    int idx = G_cg->find_or_add_meta(id, idlen, sym);

    /* set the index in the symbol */
    sym->set_meta_idx(idx);

    /* return the new symbol */
    return sym;
}
