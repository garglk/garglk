#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2010 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcprsprg.cpp - Program-level parsing
Function
  This parser module includes statement node classes that are needed only
  for a full compiler that parses at the outer program-level structure -
  object definitions, 'extern' statements, etc.  This file also contains code
  to read and write symbol and object files.  This module should not be
  needed for tools that need only to compile expressions or code fragments,
  such as debuggers or interpreters equipped with "eval()" functionality.
Notes
  
Modified
  01/09/10 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "os.h"
#include "t3std.h"
#include "utf8.h"
#include "tcprs.h"
#include "tctarg.h"
#include "tcgen.h"
#include "vmhash.h"
#include "tcmain.h"
#include "vmfile.h"
#include "tcmake.h"


/* ------------------------------------------------------------------------ */
/*
 *   Add an entry to a global symbol table, and mark the symbol as
 *   referenced.  
 */
void CTcPrsSymtab::add_to_global_symtab(CTcPrsSymtab *tab, CTcSymbol *entry)
{
    /* add the entry to the symbol table */
    tab->get_hashtab()->add(entry);

    /* mark the entry as referenced */
    entry->mark_referenced();
}


/* ------------------------------------------------------------------------ */
/*
 *   Callback context for enumerating the global symbol table entries for
 *   writing a symbol export file 
 */
struct prs_wsf_ctx
{
    /* number of symbols so far */
    ulong cnt;
    
    /* the file we're writing */
    CVmFile *fp;

    /* the parser object */
    CTcParser *prs;
};

/*
 *   Signature string for symbol export file.  The numerical suffix at the
 *   end of the initial string is a format version number - we simply
 *   increment this each time we make a change to the format.  On loading a
 *   symbol file, we check to make sure the format matches; if it doesn't,
 *   we'll abort with an error, ensuring that we don't try to read a file
 *   that's not formatted with the current structure.  Since symbol files are
 *   derived directly from source files via compilation, the worst case is
 *   that the user has to do a full recompile when upgrading to a new
 *   compiler version, so we don't bother trying to maintain upward
 *   compatibility among the symbol file formats.  
 */
static const char symbol_export_sig[] = "TADS3.SymbolExport.0007\n\r\032";

/*
 *   Write a symbol export file.  
 */
void CTcParser::write_symbol_file(CVmFile *fp, CTcMake *make_obj)
{
    prs_wsf_ctx ctx;
    long size_ofs;
    long cnt_ofs;
    long end_ofs;
    int i;
    int cnt;

    /* write the file signature */
    fp->write_bytes(symbol_export_sig, sizeof(symbol_export_sig) - 1);

    /* write a placeholder for the make-config block size */
    size_ofs = fp->get_pos();
    fp->write_uint4(0);

    /* if there's a make object, tell it to write out its config data */
    if (make_obj != 0)
    {
        long end_ofs;

        /* write out the data */
        make_obj->write_build_config_to_sym_file(fp);

        /* go back and fix up the size */
        end_ofs = fp->get_pos();
        fp->set_pos(size_ofs);
        fp->write_uint4(end_ofs - size_ofs - 4);

        /* seek back to the end for continued writing */
        fp->set_pos(end_ofs);
    }

    /* 
     *   write the number of intrinsic function sets, followed by the
     *   function set names 
     */
    fp->write_uint4(cnt = G_cg->get_fnset_cnt());
    for (i = 0 ; i < cnt ; ++i)
    {
        const char *nm;
        size_t len;

        /* get the name */
        nm = G_cg->get_fnset_name(i);

        /* write the length followed by the name */
        fp->write_uint2(len = strlen(nm));
        fp->write_bytes(nm, len);
    }

    /* 
     *   write the number of intrinsic classes, followed by the intrinsic
     *   class names 
     */
    fp->write_uint4(cnt = G_cg->get_meta_cnt());
    for (i = 0 ; i < cnt ; ++i)
    {
        const char *nm;
        size_t len;

        /* get the name */
        nm = G_cg->get_meta_name(i);

        /* write the length followed by the name */
        fp->write_uint2(len = strlen(nm));
        fp->write_bytes(nm, len);
    }

    /* 
     *   write a placeholder for the symbol count, first remembering where
     *   we put it so that we can come back and fix it up after we know
     *   how many symbols there actually are 
     */
    cnt_ofs = fp->get_pos();
    fp->write_uint4(0);

    /* write each entry from the global symbol table */
    ctx.cnt = 0;
    ctx.fp = fp;
    ctx.prs = this;
    global_symtab_->enum_entries(&write_sym_cb, &ctx);

    /* go back and fix up the counter */
    end_ofs = fp->get_pos();
    fp->set_pos(cnt_ofs);
    fp->write_uint4(ctx.cnt);

    /* 
     *   seek back to the ending offset, in case the caller is embedding
     *   the symbol data stream in a larger file structure of some kind
     *   and will be writing additional data after the end of our portion 
     */
    fp->set_pos(end_ofs);
}

/*
 *   Callback for symbol table enumeration for writing a symbol export
 *   file.  Writes one symbol to the file.  
 */
void CTcParser::write_sym_cb(void *ctx0, CTcSymbol *sym)
{
    prs_wsf_ctx *ctx = (prs_wsf_ctx *)ctx0;

    /* ask the symbol to write itself to the file */
    if (sym->write_to_sym_file(ctx->fp))
    {
        /* we wrote this symbol - count it */
        ++(ctx->cnt);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Seek to the start of the build configuration information in an object
 *   file 
 */
ulong CTcParser::seek_sym_file_build_config_info(class CVmFile *fp)
{
    char buf[128];
    ulong siz;
    int err = FALSE;

    /* read the signature, and make sure it matches */
    err_try
    {
        fp->read_bytes(buf, sizeof(symbol_export_sig) - 1);
        if (memcmp(buf, symbol_export_sig,
                   sizeof(symbol_export_sig) - 1) != 0)
        {
            /* invalid signature - no config data available */
            return 0;
        }

        /* read the size of the block */
        siz = fp->read_uint4();
    }
    err_catch_disc
    {
        /* note the error */
        err = TRUE;
    }
    err_end;

    /* 
     *   if an error occurred, indicate that no configuration block is
     *   available 
     */
    if (err)
        return 0;

    /* 
     *   tell the caller the size of the block; the block immediately
     *   follows the size prefix, so the file pointer is set to the right
     *   position now 
     */
    return siz;
}


/* ------------------------------------------------------------------------ */
/*
 *   Read a symbol file 
 */
int CTcParser::read_symbol_file(CVmFile *fp)
{
    char buf[128];
    ulong cnt;
    ulong i;
    ulong skip_len;
    
    /* read the signature and ensure it's valid */
    fp->read_bytes(buf, sizeof(symbol_export_sig) - 1);
    if (memcmp(buf, symbol_export_sig, sizeof(symbol_export_sig) - 1) != 0)
    {
        /* it's invalid - log an error */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_SYMEXP_INV_SIG);

        /* return failure */
        return 1;
    }

    /* 
     *   read the make-config data size and skip the data block - it's here
     *   for use by the make utility, and we don't have any use for it here 
     */
    skip_len = fp->read_uint4();
    fp->set_pos(fp->get_pos() + skip_len);

    /* read the intrinsic function set count, then read the function sets */
    cnt = fp->read_uint4();
    for (i = 0 ; i < cnt ; ++i)
    {
        char buf[256];

        /* read the function set name */
        if (CTcParser::read_len_prefix_str(fp, buf, sizeof(buf),
                                           TCERR_SYMEXP_SYM_TOO_LONG))
            return 101;

        /* add it to the function set list */
        G_cg->add_fnset(buf);
    }

    /* read the metaclass count, then read the metaclasses */
    cnt = fp->read_uint4();
    for (i = 0 ; i < cnt ; ++i)
    {
        char buf[256];
        
        /* read the metaclass external name */
        if (CTcParser::read_len_prefix_str(fp, buf, sizeof(buf),
                                           TCERR_SYMEXP_SYM_TOO_LONG))
            return 102;

        /* add it to the metaclass list */
        G_cg->find_or_add_meta(buf, strlen(buf), 0);
    }

    /* read the symbol count */
    cnt = fp->read_uint4();

    /* read the symbols */
    for ( ; cnt != 0 ; --cnt)
    {
        /* read a symbol */
        CTcSymbol *sym = CTcSymbol::read_from_sym_file(fp);

        /* if that failed, abort */
        if (sym == 0)
            return 2;

        /* check for an existing definition */
        CTcSymbol *old_sym = global_symtab_->find(
            sym->get_sym(), sym->get_sym_len());

        /* 
         *   if we're redefining a weak property symbol, delete the old
         *   property symbol 
         */
        if (old_sym != 0
            && old_sym != sym
            && old_sym->get_type() == TC_SYM_PROP
            && ((CTcSymProp *)old_sym)->is_weak())
        {
            global_symtab_->remove_entry(old_sym);
            old_sym = 0;
        }

        /* 
         *   If the symbol is already in the symbol table, log a warning
         *   and ignore the symbol.  If the new symbol we loaded is the
         *   same object as the original symbol table entry, it means that
         *   this is a valid redefinition - the symbol subclass loader
         *   indicated this by returning the same entry.  
         */
        if (old_sym == sym)
        {
            /* 
             *   it's a harmless redefinition - there's no need to warn
             *   and no need to add the symbol to the table again 
             */
        }
        else if (old_sym != 0)
        {
            /* 
             *   it's already defined - log it as a pedantic warning (it's
             *   not a standard-level warning because, if it is actually a
             *   problem, it'll cause an actual error at link time, so
             *   there's little value in complaining about it now)
             */
            G_tcmain->log_error(0, 0, TC_SEV_PEDANTIC, TCERR_SYMEXP_REDEF,
                                (int)sym->get_sym_len(), sym->get_sym());
        }
        else
        {
            /* add it to the global symbol table */
            global_symtab_->add_entry(sym);
        }
    }

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Write to an object file 
 */

/*
 *   Write to an object file
 */
void CTcParser::write_to_object_file(CVmFile *fp)
{
    prs_wsf_ctx ctx;
    long cnt_ofs;
    long end_ofs;
    ulong anon_cnt;
    ulong nonsym_cnt;
    tcprs_nonsym_obj *nonsym;
    CTcSymObj *anon_obj;
    CTcGramProdEntry *prod;
    ulong prod_cnt;
    CTcPrsExport *exp;
    ulong exp_cnt;

    /* 
     *   reset the object file symbol and dictionary index values - start
     *   with 1, since 0 is a "null pointer" index value 
     */
    obj_file_sym_idx_ = 1;
    obj_file_dict_idx_ = 1;

    /* 
     *   write a placeholder for the symbol index table count, the
     *   dictionary table count, and the actual symbol count 
     */
    cnt_ofs = fp->get_pos();
    fp->write_uint4(0);
    fp->write_uint4(0);
    fp->write_uint4(0);

    /* write each entry from the global symbol table */
    ctx.cnt = 0;
    ctx.fp = fp;
    ctx.prs = this;
    global_symtab_->enum_entries(&write_obj_cb, &ctx);

    /* count the anonymous objects */
    for (anon_cnt = 0, anon_obj = anon_obj_head_ ; anon_obj != 0 ;
         ++anon_cnt, anon_obj = (CTcSymObj *)anon_obj->nxt_) ;

    /* write the number of anonymous objects */
    fp->write_uint4(anon_cnt);

    /* write the anonymous objects */
    for (anon_obj = anon_obj_head_ ; anon_obj != 0 ;
         anon_obj = (CTcSymObj *)anon_obj->nxt_)
    {
        /* write this anonymous object */
        anon_obj->write_to_obj_file(fp);
    }

    /* count the non-symbol objects */
    for (nonsym_cnt = 0, nonsym = nonsym_obj_head_ ; nonsym != 0 ;
         ++nonsym_cnt, nonsym = nonsym->nxt_) ;

    /* write the non-symbol object ID's */
    fp->write_uint4(nonsym_cnt);
    for (nonsym = nonsym_obj_head_ ; nonsym != 0 ; nonsym = nonsym->nxt_)
        fp->write_uint4((ulong)nonsym->id_);

    /* remember where we are, and seek back to the counters for fixup */
    end_ofs = fp->get_pos();
    fp->set_pos(cnt_ofs);

    /* write the actual symbol index count */
    fp->write_uint4(obj_file_sym_idx_);

    /* write the actual dictionary index count */
    fp->write_uint4(obj_file_dict_idx_);

    /* write the total named symbol count */
    fp->write_uint4(ctx.cnt);

    /* 
     *   seek back to the ending offset so we can continue on to write the
     *   remainder of the file 
     */
    fp->set_pos(end_ofs);

    /* write a placeholder for the symbol cross-reference entries */
    cnt_ofs = fp->get_pos();
    fp->write_uint4(0);

    /* 
     *   enumerate entries again, this time writing cross-object
     *   references (this must be done after we've written out the main
     *   part of each object, so that we have a full table of all of the
     *   objects loaded before we write any references) 
     */
    ctx.cnt = 0;
    global_symtab_->enum_entries(&write_obj_ref_cb, &ctx);

    /* go back and fix up the cross-reference count */
    end_ofs = fp->get_pos();
    fp->set_pos(cnt_ofs);
    fp->write_uint4(ctx.cnt);
    fp->set_pos(end_ofs);

    /* write the anonymous object cross-references */
    fp->write_uint4(anon_cnt);
    for (anon_obj = anon_obj_head_ ; anon_obj != 0 ;
         anon_obj = (CTcSymObj *)anon_obj->nxt_)
    {
        /* write this anonymous object's cross-references */
        anon_obj->write_refs_to_obj_file(fp);
    }

    /* count the grammar productions */
    for (prod_cnt = 0, prod = gramprod_head_ ; prod != 0 ;
         ++prod_cnt, prod = prod->get_next()) ;

    /* write the number of productions */
    fp->write_uint4(prod_cnt);

    /* 
     *   write the main list of grammar rules - this is the list of
     *   productions that are associated with anonymous match objects and
     *   hence cannot be replaced or modified 
     */
    for (prod = gramprod_head_ ; prod != 0 ; prod = prod->get_next())
    {
        /* write this entry */
        prod->write_to_obj_file(fp);
    }

    /* write a placeholder for the named grammar rule count */
    cnt_ofs = fp->get_pos();
    fp->write_uint4(0);

    /* 
     *   write the private grammar rules - this is the set of rules
     *   associated with named match objects, so we write these by
     *   enumerating the symbol table again through the grammar rule writer
     *   callback 
     */
    ctx.cnt = 0;
    global_symtab_->enum_entries(&write_obj_gram_cb, &ctx);

    /* go back and fix up the named grammar rule count */
    end_ofs = fp->get_pos();
    fp->set_pos(cnt_ofs);
    fp->write_uint4(ctx.cnt);
    fp->set_pos(end_ofs);

    /* count the exports */
    for (exp_cnt = 0, exp = exp_head_ ; exp != 0 ;
         ++exp_cnt, exp = exp->get_next());

    /* write the number of exports */
    fp->write_uint4(exp_cnt);

    /* write the exports */
    for (exp = exp_head_ ; exp != 0 ; exp = exp->get_next())
    {
        /* write this entry */
        exp->write_to_obj_file(fp);
    }
}

/*
 *   Callback for symbol table enumeration for writing an object file.
 *   Writes one symbol to the file.  
 */
void CTcParser::write_obj_cb(void *ctx0, CTcSymbol *sym)
{
    prs_wsf_ctx *ctx = (prs_wsf_ctx *)ctx0;

    /* ask the symbol to write itself to the file */
    if (sym->write_to_obj_file(ctx->fp))
    {
        /* we wrote this symbol - count it */
        ++(ctx->cnt);
    }
}

/*
 *   Callback for symbol table numeration - write object references to the
 *   object file. 
 */
void CTcParser::write_obj_ref_cb(void *ctx0, CTcSymbol *sym)
{
    prs_wsf_ctx *ctx = (prs_wsf_ctx *)ctx0;

    /* ask the symbol to write its references to the file */
    if (sym->write_refs_to_obj_file(ctx->fp))
        ++(ctx->cnt);
}

/* 
 *   Callback for symbol table enumeration - write named grammar rules 
 */
void CTcParser::write_obj_gram_cb(void *ctx0, CTcSymbol *sym)
{
    prs_wsf_ctx *ctx = (prs_wsf_ctx *)ctx0;
    CTcSymObj *obj_sym;

    /* if it's not an object symbol, we can ignore it */
    if (sym->get_type() != TC_SYM_OBJ)
        return;

    /* cast it to the proper type */
    obj_sym = (CTcSymObj *)sym;

    /* if it doesn't have a private grammar rule list, ignore it */
    if (obj_sym->get_grammar_entry() == 0)
        return;

    /* count it */
    ++(ctx->cnt);

    /* write the defining symbol's object file index */
    ctx->fp->write_uint4(obj_sym->get_obj_file_idx());

    /* write the grammar rule */
    obj_sym->get_grammar_entry()->write_to_obj_file(ctx->fp);
}

/* ------------------------------------------------------------------------ */
/*
 *   Turn sourceTextGroup mode on or off 
 */
void CTcParser::set_source_text_group_mode(int f)
{
    /* set the new mode */
    src_group_mode_ = (f != 0);

    /* 
     *   If we're turning this mode on for the first time, add the object
     *   definition for the sourceTextGroup object - this is an anonymous
     *   object that we automatically create, once per source module; all of
     *   the sourceTextGroup properties in the module point to this same
     *   object, which is how we can tell at run-time that they were defined
     *   in the same module.  
     */
    if (f && src_group_id_ == TCTARG_INVALID_OBJ)
    {
        CTcSymObj *obj;
        CTPNStmObject *obj_stm;
        CTcConstVal cval;

        /* create the anonymous object */
        src_group_id_ = G_cg->new_obj_id();

        /* create an anonymous symbol table entry for the object */
        obj = new CTcSymObj(".sourceTextGroup", 16, FALSE,
                            src_group_id_, FALSE, TC_META_TADSOBJ, 0);
        G_prs->add_anon_obj(obj);

        /* create an object statement for it */
        obj_stm = new CTPNStmObject(obj, FALSE);
        obj->set_obj_stm(obj_stm);

        /* 
         *   if we know the module name and sequence number, set properties
         *   for them in the referent object 
         */
        if (module_name_ != 0)
        {
            cval.set_sstr(module_name_, strlen(module_name_));
            obj_stm->add_prop(src_group_mod_sym_, new CTPNConst(&cval),
                              FALSE, FALSE);
        }
        if (module_seqno_ != 0)
        {
            cval.set_int(module_seqno_);
            obj_stm->add_prop(src_group_seq_sym_, new CTPNConst(&cval),
                              FALSE, FALSE);
        }

        /* add it to the nested statement list */
        add_nested_stm(obj_stm);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse top-level definitions.  Returns the head of a list of statement
 *   nodes; the statements are the top-level statements in the program.  
 */
class CTPNStmProg *CTcParser::parse_top()
{
    /* nothing in our list yet */
    CTPNStmTop *first_stm = 0;
    CTPNStmTop *last_stm = 0;

    /* if there are any pending nested statements, add them to the list now */
    if (nested_stm_head_ != 0)
    {
        /* move the nested statement list into the top-level list */
        first_stm = nested_stm_head_;
        last_stm = nested_stm_tail_;

        /* clear out the old list */
        nested_stm_head_ = nested_stm_tail_ = 0;
    }

    /* do not suppress errors */
    int suppress_error = FALSE;
    
    /* no end-of-file error yet */
    int err = FALSE;

    /* keep going until we reach the end of the file */
    for (int done = FALSE ; !done && !err ; )
    {
        /* we don't have a statement yet */
        CTPNStmTop *cur_stm = 0;

        /* 
         *   presume we'll find a valid statement and hence won't need to
         *   suppress subsequent errors 
         */
        int suppress_next_error = FALSE;
        
        /* see what we have */
        switch(G_tok->cur())
        {
        case TOKT_EOF:
            /* we're done */
            done = TRUE;
            break;
            
        case TOKT_FUNCTION:
        case TOKT_METHOD:
            /* parse the function definition */
            cur_stm = parse_function(&err, FALSE, FALSE, FALSE, TRUE);
            break;

        case TOKT_EXTERN:
            /* parse the extern definition */
            parse_extern(&err);
            break;

        case TOKT_INTRINSIC:
            /* parse an intrinsic function set definition */
            cur_stm = parse_intrinsic(&err);
            break;

        case TOKT_TRANSIENT:
        case TOKT_SYM:
            /* it's an object or function definition */
            cur_stm = parse_object_or_func(&err, FALSE, suppress_error,
                                           &suppress_next_error);
            break;

        case TOKT_OBJECT:
            /* 
             *   it's an anonymous object with 'object' superclass, or an
             *   'object template' definition 
             */
            cur_stm = parse_object_stm(&err, FALSE);
            break;

        case TOKT_PLUS:
        case TOKT_INC:
            cur_stm = parse_plus_object(&err);
            break;

        case TOKT_CLASS:
            /* it's a class definition */
            cur_stm = parse_class(&err);
            break;

        case TOKT_REPLACE:
            /* it's a 'replace' statement */
            cur_stm = parse_replace(&err);
            break;

        case TOKT_MODIFY:
            /* it's a 'modify' statement */
            cur_stm = parse_modify(&err);
            break;

        case TOKT_PROPERTY:
            /* it's a 'property' statement */
            parse_property(&err);
            break;

        case TOKT_EXPORT:
            /* it's an 'export' statement */
            parse_export(&err);
            break;

        case TOKT_DICTIONARY:
            /* it's a 'dictionary' statement */
            cur_stm = parse_dict(&err);
            break;

        case TOKT_GRAMMAR:
            /* it's a 'grammar' statement */
            cur_stm = parse_grammar(&err, FALSE, FALSE);
            break;

        case TOKT_ENUM:
            /* it's an 'enum' statement */
            parse_enum(&err);
            break;

        case TOKT_SEM:
            /* empty statement - skip it */
            G_tok->next();
            break;

        default:
            /* 
             *   note the error, unless we just generated the same error
             *   for the previous token - if we did, there's no point in
             *   showing it again, since we're probably skipping lots of
             *   tokens trying to get resynchronized 
             */
            if (!suppress_error)
                G_tok->log_error_curtok(TCERR_REQ_FUNC_OR_OBJ);

            /* suppress the next error of the same kind */
            suppress_next_error = TRUE;
            
            /* skip the errant token */
            G_tok->next();
            break;
        }

        /* 
         *   set the error suppression status according to whether we just
         *   found an error - if we did, don't report another of the same
         *   kind twice in a row, since we're probably just scanning
         *   through tons of stuff trying to get re-synchronized 
         */
        suppress_error = suppress_next_error;

        /* if we parsed a statement, add it to our list */
        if (cur_stm != 0)
        {
            /* link the statement at the end of our list */
            if (last_stm != 0)
                last_stm->set_next_stm_top(cur_stm);
            else
                first_stm = cur_stm;
            last_stm = cur_stm;
        }
    }

    /* add the list of nested top-level statements to the statement list */
    if (nested_stm_head_ != 0)
    {
        if (last_stm != 0)
            last_stm->set_next_stm_top(nested_stm_head_);
        else
            first_stm = nested_stm_head_;
    }

    /* 
     *   we've now moved the nested top-level statement list into the normal
     *   program statement list, so we can forget this as a separate list 
     */
    nested_stm_head_ = nested_stm_tail_ = 0;

    /* construct a compound statement to hold the top-level statement list */
    return new CTPNStmProg(first_stm);
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a 'property' top-level statement 
 */
void CTcParser::parse_property(int *err)
{
    int done;
    
    /* skip the 'property' token */
    G_tok->next();

    /* parse property names until we run out */
    for (done = FALSE ; !done ; )
    {
        /* we should be looking at a property name */
        if (G_tok->cur() == TOKT_SYM)
        {
            /* 
             *   look up the property; this will add it to the symbol table
             *   if it isn't already in there, and will generate an error if
             *   it's already defined as something other than a property 
             */
            look_up_prop(G_tok->getcur(), TRUE);
            
            /* skip the symbol and check what follows */
            if (G_tok->next() == TOKT_COMMA)
            {
                /* skip the comma and continue */
                G_tok->next();
            }
            else if (G_tok->cur() == TOKT_SEM)
            {
                /* 
                 *   end of the statement - skip the semicolon and we're
                 *   done 
                 */
                G_tok->next();
                break;
            }
            else
            {
                /* 
                 *   if it's a brace, they probably left out the semicolon;
                 *   if it's a symbol, they probably left out the comma;
                 *   otherwise, it's probably a stray token 
                 */
                switch(G_tok->cur())
                {
                case TOKT_SYM:
                    /* 
                     *   probably left out a comma - flag an error, but
                     *   proceed with parsing this new symbol 
                     */
                    G_tok->log_error_curtok(TCERR_PROPDECL_REQ_COMMA);
                    break;
                    
                case TOKT_LBRACE:
                case TOKT_RBRACE:
                case TOKT_EOF:
                    /* they probably left out the semicolon */
                    G_tok->log_error_curtok(TCERR_EXPECTED_SEMI);
                    return;
                    
                default:
                    /* log an error */
                    G_tok->log_error_curtok(TCERR_PROPDECL_REQ_COMMA);
                    
                    /* skip the errant symbol */
                    G_tok->next();
                    
                    /* 
                     *   if a comma follows, something was probably just
                     *   inserted - skip the comma; if a semicolon follows,
                     *   assume we're at the end of the statement 
                     */
                    if (G_tok->cur() == TOKT_COMMA)
                    {
                        /* 
                         *   we're probably okay again - skip the comma and
                         *   continue 
                         */
                        G_tok->next();
                    }
                    else if (G_tok->cur() == TOKT_SEM)
                    {
                        /* skip the semicolon, and we're done */
                        G_tok->next();
                        done = TRUE;
                    }
                    
                    /* proceed with what follows */
                    break;
                }
            }
        }
        else
        {
            /* log an error */
            G_tok->log_error_curtok(TCERR_PROPDECL_REQ_SYM);

            switch(G_tok->cur())
            {
            case TOKT_SEM:
            case TOKT_LBRACE:
            case TOKT_RBRACE:
            case TOKT_EOF:
                /* they're probably done with the statement */
                return;

            case TOKT_COMMA:
                /* 
                 *   they probably just doubled a comma - skip it and
                 *   continue 
                 */
                G_tok->next();
                break;
                
            default:
                /* skip this token */
                G_tok->next();
                
                /* 
                 *   if a comma follows, they probably put in a keyword or
                 *   something like that - skip the comma and continue 
                 */
                if (G_tok->cur() == TOKT_COMMA)
                    G_tok->next();
                break;
            }
            
            /* if a semicolon follows, we're done */
            if (G_tok->cur() == TOKT_SEM)
            {
                /* skip the semicolon and we're done */
                G_tok->next();
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse an 'export' top-level statement 
 */
void CTcParser::parse_export(int *err)
{
    CTcPrsExport *exp;
    
    /* skip the 'export' token and see what follows */
    switch(G_tok->next())
    {
    case TOKT_SYM:
        /* create a new export record for this symbol token */
        exp = add_export(G_tok->getcur()->get_text(),
                         G_tok->getcur()->get_text_len());

        /* 
         *   Check to see what follows.  We can either have a single-quoted
         *   string giving the external name by which this symbol should be
         *   known, or the end of the statement.  If it's the latter, the
         *   external name is the same as the symbol name. 
         */
        switch(G_tok->next())
        {
        case TOKT_SEM:
            /* end of the statement - skip the semicolon, and we're done */
            G_tok->next();
            break;

        case TOKT_SSTR:
            /* 
             *   it's an explicit external name specification - save the
             *   name with the export record 
             */
            exp->set_extern_name(G_tok->getcur()->get_text(),
                                 G_tok->getcur()->get_text_len());

            /* if it's too long, flag an error */
            if (G_tok->getcur()->get_text_len() > TOK_SYM_MAX_LEN)
                G_tok->log_error_curtok(TCERR_EXPORT_EXT_TOO_LONG);

            /* skip the string and check for a semicolon */
            if (G_tok->next() != TOKT_SEM)
            {
                /* assume they just left out the semicolon, but log it */
                G_tok->log_error_curtok(TCERR_EXPECTED_SEMI);
            }
            break;

        default:
            /* 
             *   anything else is an error; assume they simply omitted the
             *   semicolon 
             */
            G_tok->log_error_curtok(TCERR_EXPECTED_SEMI);
        }

        /* done */
        break;

    case TOKT_SEM:
    case TOKT_LBRACE:
    case TOKT_RBRACE:
    case TOKT_EOF:
        /* 
         *   they probably just left something out; consider this the end of
         *   the statement, but log an error about it 
         */
        G_tok->log_error_curtok(TCERR_EXPORT_REQ_SYM);
        break;
        
    default:
        /* 
         *   something weird happened; skip the errant token, and consider
         *   it the end of the statement 
         */
        G_tok->log_error_curtok(TCERR_EXPORT_REQ_SYM);
        G_tok->next();
        break;
    }
}

/*
 *   Add an export 
 */
CTcPrsExport *CTcParser::add_export(const char *sym, size_t len)
{
    CTcPrsExport *exp;
    
    /* create a new record */
    exp = new CTcPrsExport(sym, len);

    /* add it to our list */
    add_export_to_list(exp);

    /* return it */
    return exp;
}

/*
 *   Add an export to our list 
 */
void CTcParser::add_export_to_list(CTcPrsExport *exp)
{
    /* link it at the end of our list */
    if (exp_tail_ == 0)
        exp_head_ = exp;
    else
        exp_tail_->set_next(exp);
    exp_tail_ = exp;
}

/*
 *   Parse a 'dictionary' top-level statement 
 */
CTPNStmTop *CTcParser::parse_dict(int *err)
{
    int done;
        
    /* skip the 'dictionary' token and check what follows */
    switch(G_tok->next())
    {
    case TOKT_PROPERTY:
        /* 
         *   'dictionary property' definition - what follows is a list of
         *   properties that are going to be considered dictionary
         *   properties. 
         */

        /* skip the 'property' token */
        G_tok->next();

        /* parse the list of property names */
        for (done = FALSE ; !done ; )
        {
            /* check what we have */
            if (G_tok->cur() == TOKT_SYM)
            {
                CTcSymProp *sym;

                /* look up the property */
                sym = look_up_prop(G_tok->getcur(), TRUE);

                /* 
                 *   if it's already marked as a dictionary property,
                 *   there's nothing we need to do now; otherwise, mark it 
                 */
                if (sym != 0 && !sym->is_vocab())
                {
                    CTcDictPropEntry *entry;

                    /* mark it as a vocabulary property */
                    sym->set_vocab(TRUE);

                    /* 
                     *   include the symbol in our master dictionary
                     *   property list 
                     */
                    
                    /* create a new list entry */
                    entry = new (G_prsmem) CTcDictPropEntry(sym);
                    
                    /* link it into our list */
                    entry->nxt_ = dict_prop_head_;
                    dict_prop_head_ = entry;
                }

                /* skip the symbol and check what follows */
                if (G_tok->next() == TOKT_COMMA)
                {
                    /* skip the comma and continue */
                    G_tok->next();
                }
                else if (G_tok->cur() == TOKT_SEM)
                {
                    /* 
                     *   end of the statement - skip the semicolon and
                     *   we're done 
                     */
                    G_tok->next();
                    break;
                }
                else
                {
                    /* 
                     *   if it's a brace, they probably left out the
                     *   semicolon; if it's a symbol, they probably left
                     *   out the comma; otherwise, it's probably a stray
                     *   token 
                     */
                    switch(G_tok->cur())
                    {
                    case TOKT_SYM:
                        /* 
                         *   probably left out a comma - flag an error,
                         *   but proceed with parsing this new symbol 
                         */
                        G_tok->log_error_curtok(TCERR_DICT_PROP_REQ_COMMA);
                        break;

                    case TOKT_LBRACE:
                    case TOKT_RBRACE:
                    case TOKT_EOF:
                        /* they probably left out the semicolon */
                        G_tok->log_error_curtok(TCERR_EXPECTED_SEMI);
                        return 0;

                    default:
                        /* log an error */
                        G_tok->log_error_curtok(TCERR_DICT_PROP_REQ_COMMA);

                        /* skip the errant symbol */
                        G_tok->next();

                        /* 
                         *   if a comma follows, something was probably
                         *   just inserted - skip the comma; if a
                         *   semicolon follows, assume we're at the end of
                         *   the statement 
                         */
                        if (G_tok->cur() == TOKT_COMMA)
                        {
                            /* 
                             *   we're probably okay again - skip the
                             *   comma and continue 
                             */
                            G_tok->next();
                        }
                        else if (G_tok->cur() == TOKT_SEM)
                        {
                            /* skip the semicolon, and we're done */
                            G_tok->next();
                            done = TRUE;
                        }

                        /* proceed with what follows */
                        break;
                    }
                }
            }
            else
            {
                /* log an error */
                G_tok->log_error_curtok(TCERR_DICT_PROP_REQ_SYM);

                switch(G_tok->cur())
                {
                case TOKT_SEM:
                case TOKT_LBRACE:
                case TOKT_RBRACE:
                case TOKT_EOF:
                    /* they're probably done with the statement */
                    return 0;

                case TOKT_COMMA:
                    /* 
                     *   they probably just doubled a comma - skip it and
                     *   continue 
                     */
                    G_tok->next();
                    break;

                default:
                    /* skip this token */
                    G_tok->next();

                    /* 
                     *   if a comma follows, they probably put in a
                     *   keyword or something like that - skip the comma
                     *   and continue 
                     */
                    if (G_tok->cur() == TOKT_COMMA)
                        G_tok->next();
                    break;
                }

                /* if a semicolon follows, we're done */
                if (G_tok->cur() == TOKT_SEM)
                {
                    /* skip the semicolon and we're done */
                    G_tok->next();
                    break;
                }
            }
        }

        /* 
         *   this statement doesn't result in adding anything to the parse
         *   tree 
         */
        return 0;

    case TOKT_SYM:
        /* 
         *   Dictionary object definition - what follows is the name of an
         *   object to be created of metaclass 'dictionary'.  This object
         *   is to become the active dictionary for subsequent parsing. 
         */
        {
            CTcDictEntry *dict;
            CTPNStmDict *dict_stm = 0;

            /* find the dictionary entry */
            dict = declare_dict(G_tok->getcur()->get_text(),
                                G_tok->getcur()->get_text_len());
            
            /* if we have a symbol, create the statement node */
            if (dict != 0)
            {
                /* create the 'dictionary' statement node */
                dict_stm = new CTPNStmDict(dict);

                /* 
                 *   remember the active dictionary - this is the
                 *   dictionary into which subsequent vocabulary will be
                 *   inserted 
                 */
                dict_cur_ = dict;

                /* mark the dictionary as non-external */
                dict->get_sym()->set_extern(FALSE);
            }

            /* skip the symbol and parse the required closing semicolon */
            G_tok->next();
            if (parse_req_sem())
            {
                *err = TRUE;
                return 0;
            }

            /* return the 'dictionary' statement */
            return dict_stm;
        }

    default:
        /* anything else is invalid */
        G_tok->log_error_curtok(TCERR_DICT_SYNTAX);
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Grammar tree parser compiler interface functions for the regular static
 *   compiler 
 */
class CTcGramAltFuncsStatic: public CTcGramAltFuncs
{
public:
    /* initialize with the parser object */
    CTcGramAltFuncsStatic(CTcParser *parser) { this->prs = parser; }

    /* look up a property - look up or add to the global symbols */
    CTcSymProp *look_up_prop(const CTcToken *tok, int show_err)
        { return prs->look_up_prop(tok, show_err); }

    /* declare a grammar object - look up in or add to the global symbols */
    CTcGramProdEntry *declare_gramprod(const char *txt, size_t len)
        { return prs->declare_gramprod(txt, len); }

    /* check the given enum for use as a production token */
    virtual void check_enum_tok(class CTcSymEnum *enumsym)
    {
        /* 
         *   Only allow 'enum token' values in static compilation.  I'm not
         *   actually sure at this point what the rationale ever was for this
         *   bit of strictness; there's no separate meaning for regular enums
         *   in this context, and as far as I can tell the compiler doesn't
         *   use the is_token() attribute for any other purpose.  The
         *   somewhat similar 'dictionary property' attribute at least does
         *   trigger special compiler behavior, which is to add strings to
         *   the dictionary when 'dictionary property' properties are defined
         *   on objects.  My guess is that I originally had something along
         *   the same lines in mind for 'enum token', but whatever it was
         *   turned out to be superfluous or just wrong.  So 'enum token'
         *   amounts to documentation at this point.
         *   
         *   The reason for abstracting this test into the AltFuncs structure
         *   is that the dynamic compiler doesn't have any information on
         *   'enum token' or 'dictionary property' attributes - it just knows
         *   these symbols as 'enum' or 'property' values, since there's no
         *   VM-level representation of the extra attributes.  So for dynamic
         *   compilation, the grammar parser doesn't have any way to accept
         *   enum tokens in rules.  One possible solution was to contrive to
         *   store the extra attributes in the run-time symbol table.  But
         *   given the apparent pointlessness of the strict test, I'm opting
         *   for the easier route, which is to simply relax enforcement for
         *   dynamic compilation, and allow any enum value to be used.  We
         *   can probably drop the test for static compilation as well, but
         *   I'm keeping it for now on the general principle of avoiding
         *   gratuitous changes.  
         */
        if (!enumsym->is_token())
            G_tok->log_error_curtok(TCERR_GRAMMAR_BAD_ENUM);
    }

    /* EOF in the middle of an alternative - this is an error */
    void on_eof(int *err)
    {
        /* log the error */
        G_tok->log_error_curtok(TCERR_GRAMMAR_INVAL_TOK);

        /* note the error for our caller */
        *err = TRUE;
    }

    /* the parser object */
    CTcParser *prs;
};

/* ------------------------------------------------------------------------ */
/*
 *   Parse a 'grammar' top-level statement
 */
CTPNStmTop *CTcParser::parse_grammar(int *err, int replace, int modify)
{
    /* no GrammarProd object yet */
    CTcSymObj *gram_obj = 0;

    /* presume we're not modifying anything */
    CTcSymObj *mod_orig_sym = 0;

    /* presume we won't need a private production object */
    int need_private_prod = FALSE;

    /* presume we won't find a valid production name */
    CTcToken prod_name;
    prod_name.set_text("?", 1);
    CTcGramProdEntry *prod = 0;

    /* presume it will be anonymous (i.e., no name tag) */
    int is_anon = TRUE;

    /* skip the 'grammar' token and check for the production name */
    if (G_tok->next() != TOKT_SYM)
    {
        /* log an error, then proceed without a symbol name */
        G_tok->log_error_curtok(TCERR_GRAMMAR_REQ_SYM);
    }
    else
    {
        /* remember the production name */
        prod_name = *G_tok->copycur();

        /* find or create the 'grammar production' entry */
        prod = declare_gramprod(prod_name.get_text(),
                                prod_name.get_text_len());
    }

    /* use the production name as the name tag */
    char tag_buf[TOK_SYM_MAX_LEN*2 + 32];
    const char *name_tag = prod_name.get_text();
    size_t name_tag_len = prod_name.get_text_len();

    /* presume there'll be no sub-tag */
    const char *sub_tag = 0;
    size_t sub_tag_len = 0;

    /* check for the optional name-tag token */
    if (G_tok->next() == TOKT_LPAR)
    {
        /* skip the paren - the name token follows */
        G_tok->next();

        /* 
         *   limit the added copying so we don't overflow our buffer - note
         *   that we need two bytes for parens 
         */
        size_t copy_len = G_tok->getcur()->get_text_len();
        if (copy_len > sizeof(tag_buf) - name_tag_len - 2)
            copy_len = sizeof(tag_buf) - name_tag_len - 2;

        /* remember the sub-tag */
        sub_tag = G_tok->getcur()->get_text();
        sub_tag_len = G_tok->getcur()->get_text_len();

        /* build the name */
        memcpy(tag_buf, name_tag, name_tag_len);
        tag_buf[name_tag_len] = '(';
        memcpy(tag_buf + name_tag_len + 1,
               G_tok->getcur()->get_text(), copy_len);
        tag_buf[name_tag_len + 1 + copy_len] = ')';

        /* store the tag in tokenizer memory */
        name_tag_len += 2 + copy_len;
        name_tag = G_tok->store_source(tag_buf, name_tag_len);

        /* require the closing paren */
        if (G_tok->next() == TOKT_RPAR)
        {
            /* found it - skip it */
            G_tok->next();
        }
        else
        {
            /* flag the error, but proceed as though it was there */
            G_tok->log_error_curtok(TCERR_GRAMMAR_REQ_NAME_RPAR);
        }

        /*
         *   There's a tag, so the grammar match object has a symbol name
         *   given by the full "prod(tag)" string.  Note that grammar match
         *   definitions are inherently classes.  
         */
        int is_class = TRUE;
        int trans = FALSE;
        gram_obj = find_or_def_obj(name_tag, name_tag_len,
                                   replace, modify, &is_class,
                                   &mod_orig_sym, 0, &trans);

        /* note that the object is named */
        is_anon = FALSE;

        /*
         *   Since this is a named grammar match object, we must keep the
         *   list of grammar rules defined here in a private list.  At link
         *   time, we'll merge this private grammar rule list with the
         *   master list for the production, but we must keep a private list
         *   until link time in case this match object is modified or
         *   replaced.  
         */
        need_private_prod = TRUE;
    }
    else if (!replace && !modify && G_tok->cur() == TOKT_SEM)
    {
        /* 
         *   It's an empty 'grammar x;' statement, with no rule definition
         *   and no properties.  This is a simple declaration of the
         *   production name and defines no rules for it; the only thing we
         *   have to do is mark the production object as explicitly declared.
         */
        if (prod != 0)
            prod->set_declared(TRUE);

        /* skip the semicolon */
        G_tok->next();

        /* 
         *   we're done - a simple grammar production declaration generates
         *   no parse tree object 
         */
        return 0;
    }
    else
    {
        /* 
         *   There's no tag, so the grammar match object is anonymous.
         *   Create a new anonymous object symbol for it.  
         */
        gram_obj = new CTcSymObj(".anon", 5, FALSE, G_cg->new_obj_id(),
                                 FALSE, TC_META_TADSOBJ, 0);

        /* 
         *   'replace' and 'modify' can only be used with tagged grammar
         *   rules, since that's the only way we can refer to a specific rule
         *   previously defined 
         */
        if (replace || modify)
            G_tok->log_error(TCERR_GRAMMAR_MOD_REQ_TAG);
    }

    /* check for and skip the required colon */
    if (G_tok->cur() == TOKT_COLON)
    {
        /* it's there - skip it and proceed */
        G_tok->next();
    }
    else
    {
        /* 
         *   log an error, then proceed on the assumption that the colon
         *   was simply omitted 
         */
        G_tok->log_error_curtok(TCERR_GRAMMAR_REQ_COLON);
    }

    /* 
     *   mark it as a class - this object exists as a factory for match tree
     *   instances 
     */
    if (gram_obj != 0)
        gram_obj->set_is_class(TRUE);

    /*
     *   If we're modifying the rule, check for an empty rule list.  With
     *   'modify', an empty rule list has the special meaning that we retain
     *   the original rule list of the base object being modified.  
     */
    CTcGramPropArrows arrows;
    if (modify && G_tok->cur() == TOKT_COLON)
    {
        /* 
         *   This is a 'modify', and the rule list is empty.  This means that
         *   we're to leave the original rule list of the base object intact,
         *   so we don't need to flatten the input syntax (since there's no
         *   input syntax to flatten), we don't need to create a new private
         *   list (since we want to leave the existing private list intact),
         *   and we don't need to set up the first alternative for the rule
         *   (since there will be no alternatives at all).  
         */
        G_tok->next();
    }
    else
    {
        /* 
         *   We don't have an empty 'modify' rule list, so we're defining a
         *   rule list for this production.  If we decided that we need to
         *   store the list in a private list, create the private list.  
         */
        if (need_private_prod && gram_obj != 0)
            prod = gram_obj->create_grammar_entry(
                prod_name.get_text(), prod_name.get_text_len());

        /* parse the token specification list */
        CTcGramAltFuncsStatic funcs(this);
        parse_gram_alts(err, gram_obj, prod, &arrows, &funcs);
        if (*err != 0)
            return 0;
    }

    /* parse the object body */
    CTPNStmObject *stm = parse_object_body(
        err, gram_obj, TRUE, is_anon, TRUE, FALSE, modify, mod_orig_sym,
        0, 0, 0, FALSE);

    /*
     *   Add the grammarInfo property.  This property is a method that
     *   returns a list whose first element is the match's name tag, and
     *   whose subsequent elements are the arrow-assigned properties.
     *   
     *   This is only necessary if we managed to create a statement object.  
     */
    if (stm != 0)
    {
        CTPNList *lst;
        CTcConstVal cval;
        size_t i;

        /* create the list expression */
        lst = new CTPNList();

        /* add the name-tag string element */
        cval.set_sstr(name_tag, name_tag_len);
        lst->add_element(new CTPNConst(&cval));

        /* add each property to the list */
        for (i = 0 ; i < arrows.cnt ; ++i)
        {
            /* add a node to evaluate this property symbol */
            lst->add_element(create_sym_node(arrows.prop[i]->get_sym(),
                                             arrows.prop[i]->get_sym_len()));
        }

        /* add a property for this */
        stm->add_prop(graminfo_prop_, lst, FALSE, FALSE);

        /* add a grammarTag property with the tag name */
        cval.set_sstr(sub_tag, sub_tag_len);
        stm->add_prop(gramtag_prop_, new CTPNConst(&cval), FALSE, FALSE);
    }

    /* return the object definition statement */
    return stm;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a grammar rule list object 
 */
CTcGramProdEntry *CTcSymObjBase::create_grammar_entry(
    const char *prod_sym, size_t prod_sym_len)
{
    /* look up the grammar production symbol */
    CTcSymObj *sym = G_prs->find_or_def_gramprod(prod_sym, prod_sym_len, 0);

    /* create a new grammar list associated with the production */
    grammar_entry_ = new (G_prsmem) CTcGramProdEntry(sym);

    /* return the new grammar list */
    return grammar_entry_;
}


/* ------------------------------------------------------------------------ */
/*
 *   Find or add a grammar production symbol.  Returns the master
 *   CTcGramProdEntry object for the grammar production.  
 */
CTcGramProdEntry *CTcParser::declare_gramprod(const char *txt, size_t len)
{
    /* find or define the grammar production object symbol */
    CTcGramProdEntry *entry;
    (void)find_or_def_gramprod(txt, len, &entry);

    /* return the entry */
    return entry;
}

/* ------------------------------------------------------------------------ */
/*
 *   Find or add a grammar production symbol.  
 */
CTcSymObj *CTcParser::find_or_def_gramprod(const char *txt, size_t len,
                                           CTcGramProdEntry **entryp)
{
    CTcSymObj *sym;
    CTcGramProdEntry *entry;

    /* look up the symbol */
    sym = (CTcSymObj *)global_symtab_->find(txt, len);

    /* if it's not defined, add it; otherwise, make sure it's correct */
    if (sym == 0)
    {
        /* it's not yet defined - define it as a grammar production */
        sym = new CTcSymObj(G_tok->getcur()->get_text(),
                            G_tok->getcur()->get_text_len(),
                            FALSE, G_cg->new_obj_id(), FALSE,
                            TC_META_GRAMPROD, 0);

        /* add it to the global symbol table */
        global_symtab_->add_entry(sym);

        /* create a new production list entry */
        entry = create_gramprod_entry(sym);
    }
    else
    {
        /* make sure it's an object of metaclass 'grammar production' */
        if (sym->get_type() != TC_SYM_OBJ)
        {
            /* log an error */
            G_tok->log_error(TCERR_REDEF_AS_OBJ, (int)len, txt);

            /* forget the symbol - it's not even an object */
            sym = 0;
        }
        else if (sym->get_metaclass() != TC_META_GRAMPROD)
        {
            /* it's an object, but not a production - log an error */
            G_tok->log_error(TCERR_REDEF_AS_GRAMPROD, (int)len, txt);

            /* forget the symbol */
            sym = 0;
        }

        /* 
         *   If we found the symbol, make sure it's not marked as external,
         *   since we're actually defining a rule for this production.  If
         *   the production is exported from any other modules, we'll share
         *   the production object with the other modules.  
         */
        if (sym != 0)
            sym->set_extern(FALSE);

        /* get the existing production object, if any */
        entry = get_gramprod_entry(sym);

        /* 
         *   if we didn't find the entry, we must have loaded the symbol from
         *   a symbol file - add the entry now 
         */
        if (entry == 0)
            entry = create_gramprod_entry(sym);
    }

    /* 
     *   if the caller is interested in knowing the associated grammar rule
     *   list entry, return it 
     */
    if (entryp != 0)
        *entryp = entry;

    /* return the new symbol */
    return sym;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a grammar production list entry.  This creates a CTcGramProdEntry
 *   object associated with the given grammar production symbol, and links
 *   the new entry into the master list of grammar production entries.  
 */
CTcGramProdEntry *CTcParser::create_gramprod_entry(CTcSymObj *sym)
{
    CTcGramProdEntry *entry;

    /* allocate the new entry */
    entry = new (G_prsmem) CTcGramProdEntry(sym);

    /* link the new entry into our list of anonymous match entries */
    if (gramprod_tail_ != 0)
        gramprod_tail_->set_next(entry);
    else
        gramprod_head_ = entry;
    gramprod_tail_ = entry;

    /* 
     *   set the metaclass-specific data in the object symbol to point to the
     *   grammar production list entry 
     */
    if (sym != 0)
        sym->set_meta_extra(entry);

    /* return the new entry */
    return entry;
}

/* ------------------------------------------------------------------------ */
/* 
 *   find a grammar entry for a given (GrammarProd) object symbol 
 */
CTcGramProdEntry *CTcParser::get_gramprod_entry(CTcSymObj *obj)
{
    /* the grammar entry is the metaclass-specific data pointer */
    return (obj == 0 ? 0 : (CTcGramProdEntry *)obj->get_meta_extra());
}

/* ------------------------------------------------------------------------ */
/*
 *   Find or add a dictionary symbol 
 */
CTcDictEntry *CTcParser::declare_dict(const char *txt, size_t len)
{
    CTcSymObj *sym;
    CTcDictEntry *entry = 0;

    /* look up the symbol */
    sym = (CTcSymObj *)global_symtab_->find(txt, len);

    /* if it's not defined, add it; otherwise, make sure it's correct */
    if (sym == 0)
    {
        /* it's not yet defined - define it as a dictionary */
        sym = new CTcSymObj(G_tok->getcur()->get_text(),
                            G_tok->getcur()->get_text_len(),
                            FALSE, G_cg->new_obj_id(), FALSE,
                            TC_META_DICT, 0);

        /* add it to the global symbol table */
        global_symtab_->add_entry(sym);

        /* create a new dictionary entry */
        entry = create_dict_entry(sym);
    }
    else
    {
        /* make sure it's an object of metaclass 'dictionary' */
        if (sym->get_type() != TC_SYM_OBJ)
        {
            /* log an error */
            G_tok->log_error_curtok(TCERR_REDEF_AS_OBJ);

            /* forget the symbol - it's not even an object */
            sym = 0;
        }
        else if (sym->get_metaclass() != TC_META_DICT)
        {
            /* it's an object, but not a dictionary - log an error */
            G_tok->log_error_curtok(TCERR_REDEF_AS_DICT);

            /* forget the symbol */
            sym = 0;
        }

        /* find it in our dictionary list */
        entry = get_dict_entry(sym);

        /* 
         *   if we didn't find the entry, we must have loaded the symbol from
         *   a symbol file - add the dictionary list entry now 
         */
        if (entry == 0)
            entry = create_dict_entry(sym);
    }

    /* return the new dictionary object */
    return entry;
}

/* ------------------------------------------------------------------------ */
/*
 *   Object symbols 
 */

/*
 *   Build my dictionary 
 */
void CTcSymObjBase::build_dictionary()
{
    CTcVocabEntry *entry;

    /* if I don't have a dictionary, there's nothing to do */
    if (dict_ == 0)
        return;

    /* 
     *   if I'm a class, there's nothing to do, since vocabulary defined in a
     *   class is only entered in the dictionary for the instances of the
     *   class, not for the class itself 
     */
    if (is_class_)
        return;

    /* add inherited words from my superclasses to my list */
    inherit_vocab();

    /* add each of my words to the dictionary */
    for (entry = vocab_ ; entry != 0 ; entry = entry->nxt_)
    {
        /* add this word to my dictionary */
        dict_->add_word(entry->txt_, entry->len_, FALSE,
                        obj_id_, entry->prop_);
    }
}

/*
 *   Synthesize a placeholder symbol for a modified object.
 *   
 *   The new symbol is not for use by the source code; we add it merely as a
 *   placeholder.  Build its name starting with a space so that it can never
 *   be reached from source code, and use the object number to ensure it's
 *   unique within the file.  
 */
CTcSymObj *CTcSymObjBase::synthesize_modified_obj_sym(int anon)
{
    char nm[TOK_SYM_MAX_LEN + 1];
    const char *stored_nm;
    tc_obj_id mod_id;
    CTcSymObj *mod_sym;
    size_t len;

    /* create a new ID for the modified object */
    mod_id = G_cg->new_obj_id();

    /* build the name */
    if (anon)
    {
        /* it's anonymous - we don't need a real name */
        stored_nm = ".anon";
        len = strlen(nm);
    }
    else
    {
        /* synthesize a name */
        synthesize_modified_obj_name(nm, mod_id);
        len = strlen(nm);

        /* store it in tokenizer memory */
        stored_nm = G_tok->store_source(nm, len);
    }

    /* create the object */
    mod_sym = new CTcSymObj(stored_nm, len, FALSE, mod_id, FALSE,
                            TC_META_TADSOBJ, 0);

    /* mark it as modified */
    mod_sym->set_modified(TRUE);

    /* add it to the symbol table, if it has a name */
    if (!anon)
        G_prs->get_global_symtab()->add_entry(mod_sym);
    else
        G_prs->add_anon_obj(mod_sym);

    /* return the new symbol */
    return mod_sym;
}

/*
 *   Build the name of a synthesized placeholder symbol for a given object
 *   number.  The buffer should be TOK_SYM_MAX_LEN + 1 bytes long.  
 */
void CTcSymObjBase::
    synthesize_modified_obj_name(char *buf, tctarg_obj_id_t obj_id)
{
    /* 
     *   Build the fake name, based on the object ID to ensure uniqueness and
     *   so that we can look it up based on the object ID.  Start it with a
     *   space so that no source token can ever refer to it.  
     */
    sprintf(buf, " %lx", (ulong)obj_id);
}


/* ------------------------------------------------------------------------ */
/*
 *   create a new dictionary list entry 
 */
CTcDictEntry *CTcParser::create_dict_entry(CTcSymObj *sym)
{
    CTcDictEntry *entry;

    /* allocate the new entry */
    entry = new (G_prsmem) CTcDictEntry(sym);

    /* link the new entry into our list */
    if (dict_tail_ != 0)
        dict_tail_->set_next(entry);
    else
        dict_head_ = entry;
    dict_tail_ = entry;

    /* 
     *   set the metaclass-specific extra data in the object symbol to point
     *   to the dictionary list entry 
     */
    sym->set_meta_extra(entry);

    /* return the new entry */
    return entry;
}

/* ------------------------------------------------------------------------ */
/* 
 *   find a dictionary list entry for a given object symbol 
 */
CTcDictEntry *CTcParser::get_dict_entry(CTcSymObj *obj)
{
    /* the dictionary list entry is the metaclass-specific data pointer */
    return (obj == 0 ? 0 : (CTcDictEntry *)obj->get_meta_extra());
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse an 'enum' top-level statement 
 */
void CTcParser::parse_enum(int *err)
{
    int done;
    int is_token;

    /* presume it's not a 'token' enum */
    is_token = FALSE;
    
    /* skip the 'enum' token */
    G_tok->next();

    /* 
     *   if this is the 'token' context-sensitive keyword, note it and
     *   skip it 
     */
    if (G_tok->cur() == TOKT_SYM && G_tok->cur_tok_matches("token", 5))
    {
        /* remember that it's a 'token' enum */
        is_token = TRUE;

        /* skip the keyword */
        G_tok->next();
    }

    /* parse enum constants */
    for (done = FALSE ; !done ; )
    {
        CTcSymEnum *sym;
        
        switch (G_tok->cur())
        {
        case TOKT_SYM:
            /* make sure the symbol isn't already defined */
            sym = (CTcSymEnum *)
                  global_symtab_->find(G_tok->getcur()->get_text(),
                                       G_tok->getcur()->get_text_len());

            /* if it's already defined, make sure it's an enum */
            if (sym != 0)
            {
                /* if it's not an enum, it's an error */
                if (sym->get_type() != TC_SYM_ENUM)
                {
                    /* log the error */
                    G_tok->log_error_curtok(TCERR_REDEF_AS_ENUM);
                }
                else if (is_token)
                {
                    /* 
                     *   we're defining a token - mark the old symbol as a
                     *   token if it wasn't already 
                     */
                    sym->set_is_token(TRUE);
                }
            }
            else
            {
                /* create the new symbol */
                sym = new CTcSymEnum(G_tok->getcur()->get_text(),
                                     G_tok->getcur()->get_text_len(),
                                     FALSE, new_enum_id(), is_token);

                /* add it to the symbol table */
                global_symtab_->add_entry(sym);

                /* mark it as referenced, since it's defined here */
                sym->mark_referenced();
            }

            /* skip the symbol name and see what follows */
            switch(G_tok->next())
            {
            case TOKT_COMMA:
                /* skip the comma and keep going */
                G_tok->next();
                break;
                
            case TOKT_SEM:
                /* end of the statement */
                done = TRUE;
                G_tok->next();
                break;

            case TOKT_LBRACE:
            case TOKT_RBRACE:
            case TOKT_EOF:
                /* they probably omitted the closing semicolon */
                G_tok->log_error_curtok(TCERR_EXPECTED_SEMI);
                done = TRUE;
                break;

            default:
                /* a comma was required */
                G_tok->log_error_curtok(TCERR_ENUM_REQ_COMMA);

                /* skip the offending symbol and proceed */
                G_tok->next();
                break;
            }

            /* done with this token */
            break;

        case TOKT_LBRACE:
        case TOKT_RBRACE:
        case TOKT_EOF:
            /* 
             *   they probably omitted the closing semicolon - log an
             *   error, then stop scanning the statement, since we're
             *   probably in the next statement already 
             */
            G_tok->log_error_curtok(TCERR_EXPECTED_SEMI);
            done = TRUE;
            break;

        case TOKT_SEM:
            /* they must have left out a symbol */
            G_tok->log_error_curtok(TCERR_ENUM_REQ_SYM);

            /* accept the semicolon as the end of the statement */
            G_tok->next();
            done = TRUE;
            break;
            
        default:
            /* anything else is an error */
            G_tok->log_error_curtok(TCERR_ENUM_REQ_SYM);

            /* skip the offending token and keep parsing */
            G_tok->next();
            break;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse an 'extern' top-level statement 
 */
void CTcParser::parse_extern(int *err)
{
    int is_class;

    /* skip the 'extern' token and check what follows */
    switch(G_tok->next())
    {
    case TOKT_FUNCTION:
    case TOKT_METHOD:
        /* parse the function definition for the prototype only */
        parse_function(err, TRUE, FALSE, FALSE, TRUE);
        break;

    case TOKT_CLASS:
        /* note that it's a class, and process it otherwise like 'object' */
        is_class = TRUE;
        goto do_object;

    case TOKT_OBJECT:
        /* note that it's not a class */
        is_class = FALSE;

    do_object:
        /* external object/class - skip the keyword and get the symbol */
        if (G_tok->next() == TOKT_SYM)
        {
            CTcSymObj *sym;

            /* check for a prior definition */
            sym = (CTcSymObj *)
                  global_symtab_->find(G_tok->getcur()->get_text(),
                                       G_tok->getcur()->get_text_len());
            if (sym == 0)
            {
                /* not yet defined - create the external object symbol */
                sym = new CTcSymObj(G_tok->getcur()->get_text(),
                                    G_tok->getcur()->get_text_len(),
                                    FALSE, G_cg->new_obj_id(), TRUE,
                                    TC_META_TADSOBJ, 0);

                /* add it to the symbol table */
                global_symtab_->add_entry(sym);
            }
            else if (sym->get_type() == TC_SYM_OBJ
                     && sym->get_metaclass() == TC_META_TADSOBJ
                     && ((sym->is_class() != 0) == (is_class != 0)))
            {
                /* 
                 *   It's already defined as this same type, so this
                 *   definition is merely redundant - ignore it.  
                 */
            }
            else
            {
                /* 
                 *   it's already defined, but not as an object - log an
                 *   error and ignore the redefinition 
                 */
                G_tok->log_error_curtok(TCERR_OBJ_REDEF);
            }

            /* parse the required semicolon */
            G_tok->next();
            if (parse_req_sem())
                *err = TRUE;
        }
        else
        {
            /* invalid syntax - log an error */
            G_tok->log_error_curtok(TCERR_EXTERN_OBJ_REQ_SYM);

            /* skip to the next semicolon */
            if (skip_to_sem())
                *err = TRUE;
        }
        break;

    default:
        /* log an error */
        G_tok->log_error_curtok(TCERR_INVAL_EXTERN);

        /* skip to the next semicolon */
        if (skip_to_sem())
            *err = TRUE;
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a 'function' top-level statement 
 */
CTPNStmTop *CTcParser::parse_function(int *err, int is_extern,
                                      int replace, int modify,
                                      int func_kw_present)
{
    CTcToken tok;
    CTcSymFunc *func_sym;
    CTPNCodeBody *code_body;
    int has_proto = TRUE;
    int argc;
    int opt_argc;
    int varargs;
    int varargs_list;
    CTcSymLocal *varargs_list_local;
    int has_retval;
    int redef;
    CTcTokFileDesc *sym_file;
    long sym_linenum;
    CTcFormalTypeList *type_list = 0;
    int is_method = FALSE;

    /* skip the 'function' or 'method' keyword, if present */
    if (func_kw_present)
    {
        /* note if it's a method definition */
        is_method = (G_tok->cur() == TOKT_METHOD);

        /* skip the token */
        G_tok->next();
    }

    /* we need a symbol */
    if (G_tok->cur() != TOKT_SYM)
    {
        /* log an error */
        G_tok->log_error_curtok(TCERR_FUNC_REQ_SYM);
        
        /* proceed from this token */
        return 0;
    }

    /* remember the symbol token */
    tok = *G_tok->copycur();

    /* remember the location of the symbol (for later error reporting) */
    G_tok->get_last_pos(&sym_file, &sym_linenum);

    /* 
     *   Skip past the function name.  If the next token is a semicolon,
     *   this is a forward declaration; otherwise, we need either an open
     *   brace or an argument list 
     */
    switch(G_tok->next())
    {
    case TOKT_SEM:
        /* it's just a forward declaration - skip the semicolon */
        G_tok->next();

        /* 
         *   if this is an 'extern' declaration, then declare the function
         *   as extern with zero arguments 
         */
        if (is_extern)
        {
            /* no prototype and no arguments */
            has_proto = FALSE;
            argc = 0;
            opt_argc = 0;
            varargs = FALSE;
            varargs_list = FALSE;

            /* assume it has a return value */
            has_retval = TRUE;

            /* no code body */
            code_body = 0;

            /* handle the declaration */
            goto decl_func;
        }

        /* 
         *   A non-extern forward declaration has no effect.  Ignore the
         *   definition and keep going. 
         */
        return 0;
        
    case TOKT_LPAR:
    case TOKT_LBRACE:
        /* 
         *   check whether this is an 'extern' declaration or the actual
         *   definition, and parse accordingly
         */
        if (is_extern)
        {
            /* 
             *   It's external - parse simply the formal parameter list.
             *   A brace is not allowed here. 
             */
            if (G_tok->cur() == TOKT_LPAR)
            {
                /* skip the paren */
                G_tok->next();

                /* parse the formals */
                parse_formal_list(TRUE, TRUE, &argc, &opt_argc, &varargs,
                                  &varargs_list, &varargs_list_local,
                                  err, 0, FALSE, &type_list);

                /* 
                 *   if it looks as though there's a code body, log an
                 *   error and skip and ignore the code body 
                 */
                if (G_tok->cur() == TOKT_LBRACE
                    || G_tok->cur() == TOKT_EQ)
                {
                    /* log the error */
                    G_tok->log_error(TCERR_EXTERN_NO_CODE_BODY);

                    /* parse and ignore the code body */
                    parse_code_body(FALSE, FALSE, FALSE,
                                    &argc, &opt_argc, &varargs,
                                    &varargs_list, &varargs_list_local,
                                    &has_retval, err, 0, TCPRS_CB_NORMAL,
                                    0, 0, 0, 0);

                    /* return failure */
                    return 0;
                }

                /* parse the required terminating semicolon */
                parse_req_sem();
            }
            else
            {
                /* code body is not allowed - log an error */
                G_tok->log_error(TCERR_EXTERN_NO_CODE_BODY);

                /* parse and ignore the code body */
                parse_code_body(FALSE, FALSE, is_method,
                                &argc, &opt_argc, &varargs,
                                &varargs_list, &varargs_list_local,
                                &has_retval, err, 0, TCPRS_CB_NORMAL,
                                0, 0, 0, 0);

                /* done */
                return 0;
            }

            /* there's no code body for an 'extern' definintion */
            code_body = 0;

            /* assume all externals return a value */
            has_retval = TRUE;
        }
        else
        {
            /*
             *   We're parsing the actual definition.  Parse the formal
             *   parameter list and the code body.  
             */
            code_body = parse_code_body(FALSE, FALSE, is_method,
                                        &argc, &opt_argc, &varargs,
                                        &varargs_list, &varargs_list_local,
                                        &has_retval, err, 0, TCPRS_CB_NORMAL,
                                        0, 0, 0, &type_list);
        }

        /* if that failed, do not go on */
        if (*err || (code_body == 0 && !is_extern))
            return 0;

    decl_func:
        /* if we have a type list, generate the decorated name */
        if (type_list != 0)
        {
            /*
             *   The function we're defining here will go by a decorated
             *   name, not the base name, since it's a particular version of
             *   this multi-method.  So, we need to replace the function name
             *   token with the decorated form, which incorporates the typed
             *   parameter list to distinguish this version from other
             *   multi-methods with the same base name and different
             *   parameter type lists.
             *   
             *   Before we generate the decorated name, though, define the
             *   base name as an external multi-method function, so that
             *   we'll know what to do when we see calls to the base
             *   function.  We define the base name as external because the
             *   current source code isn't actually a definition of the base
             *   name; the base name refers to an implied function that's not
             *   actually defined anywhere in the source code, but rather is
             *   generated automatically by the compiler.  As such, the
             *   source we're compiling effectively declares the existence of
             *   a function with the base name, but says that it will be
             *   defined elsewhere - this is, of course, the exact purpose of
             *   an external declaration.
             *   
             *   So, if the base name isn't yet defined, define it as an
             *   external function, and mark it as a multi-method.  Start by
             *   looking up the base function name.  
             */
            func_sym = (CTcSymFunc *)global_symtab_->find_delete_weak(
                tok.get_text(), tok.get_text_len());

            /* 
             *   if it's defined, make sure the existing definition is
             *   compatible; if it's not defined, define it now 
             */
            if (func_sym != 0)
            {
                /* 
                 *   it's previously defined: to be compatible, it has to be
                 *   an external, multi-method function 
                 */
                if (func_sym->get_type() != TC_SYM_FUNC)
                {
                    /* it's already defined as a non-function */
                    G_tcmain->log_error(
                        sym_file, sym_linenum,
                        TC_SEV_ERROR, TCERR_REDEF_AS_FUNC,
                        (int)tok.get_text_len(), tok.get_text());
                }
                else if (!(func_sym->is_extern()
                           && func_sym->is_multimethod()))
                {
                    /* it's already defined incompatibly - flag the error */
                    G_tcmain->log_error(
                        sym_file, sym_linenum,
                        TC_SEV_ERROR, TCERR_FUNC_REDEF_AS_MULTIMETHOD,
                        (int)tok.get_text_len(), tok.get_text());

                    /* ignore the redefinition */
                    return 0;
                }
            }
            else
            {
                /* 
                 *   It's not defined yet, so add it as an external
                 *   multi-method function.  This base form is always a
                 *   varargs function with no fixed arguments, since the
                 *   parameter lists of the different definitions can vary.
                 *   We'll likewise assume that there's a return value, since
                 *   some instances might have returns.  
                 */
                func_sym = new CTcSymFunc(tok.get_text(), tok.get_text_len(),
                                          FALSE, 0, 0, TRUE, TRUE,
                                          TRUE, TRUE, TRUE, TRUE);

                /* 
                 *   Mark the function symbol as defined in this file.  This
                 *   will ensure that we export it to our symbol file, even
                 *   though it's defined here as an extern symbol.  We don't
                 *   normally export externs, since we'd simply count on the
                 *   defining file to export them; but the base symbol for a
                 *   multi-method has no definer at all until link time, so
                 *   *someone* needs to export it.  So export it from any
                 *   file that defines a multi-method with the base name.  
                 */
                func_sym->set_mm_def(TRUE);

                /* add it to the symbol table */
                global_symtab_->add_entry(func_sym);
            }

            /* 
             *   Now we can generate the decorated name.  This name
             *   distinguishes this type-list version of the function from
             *   other versions with the same base name, and is the name by
             *   which the actual function definition will be known.  We'll
             *   also operate on the decorated name for 'replace' or
             *   'modify', since what we're replacing or modifying is this
             *   specific type-list version of the function.  
             */
            type_list->decorate_name(&tok, &tok);
        }

        /* find any existing function symbol */
        func_sym = (CTcSymFunc *)global_symtab_->find_delete_weak(
            tok.get_text(), tok.get_text_len());

        /* 
         *   If the symbol was already defined, the previous definition must
         *   be external, or this new definition must be external, or the new
         *   definition must be a 'replace' definition.
         */
        redef = (func_sym != 0
                 && (func_sym->get_type() != TC_SYM_FUNC
                     || (!func_sym->is_extern()
                         && !is_extern && !replace && !modify)));

        /* 
         *   In addition, if both function definitions have prototypes, the
         *   prototypes must match. 
         */
        if (has_proto && func_sym != 0 && func_sym->has_proto())
        {
            redef |= (func_sym->get_argc() != argc
                      || func_sym->is_varargs() != varargs
                      || func_sym->is_multimethod() != (type_list != 0));
        }

        /* 
         *   If the symbol was already defined, log an error, then ignore
         *   the redefinition and return an empty statement.  Note that we
         *   waited until now because we still wanted to parse the syntax
         *   of the function, but we can't really do anything with it once
         *   we're finished with the parsing.  
         */
        if (redef)
        {
            int err;
            
            /* 
             *   Note the problem.  Select the error message to display
             *   depending on whether the symbol was previously defined as
             *   a function (in which case the problem is the conflicting
             *   prototype) or as another type.  
             */
            if (func_sym->get_type() == TC_SYM_FUNC)
            {
                /* 
                 *   if one or the other definition is external, or we're
                 *   replacing the function, the problem is a mismatch in
                 *   the parameter lists; otherwise, the problem is that
                 *   the function is simply defined twice 
                 */
                if (func_sym->is_extern() || is_extern || replace || modify)
                    err = TCERR_INCOMPAT_FUNC_REDEF;
                else
                    err = TCERR_FUNC_REDEF;
            }
            else
            {
                /* it's being redefined as a new type */
                err = TCERR_REDEF_AS_FUNC;
            }

            /* log the error (at the symbol's position) */
            G_tcmain->log_error(sym_file, sym_linenum, TC_SEV_ERROR, err,
                                (int)tok.get_text_len(), tok.get_text());

            /* ignore the re-definition */
            return 0;
        }

        /* 
         *   create a symbol table entry for the function if we didn't
         *   have on already 
         */
        if (func_sym == 0)
        {
            /* we didn't have a symbol previously - create one */
            func_sym = new CTcSymFunc(tok.get_text(), tok.get_text_len(),
                                      FALSE, argc, opt_argc, varargs,
                                      has_retval, type_list != 0,
                                      FALSE, is_extern, has_proto);

            /* add the entry to the global symbol table */
            global_symtab_->add_entry(func_sym);

            /* 
             *   We can't replace/modify a function that wasn't previously
             *   defined - if we're replacing it, note the error.  Don't
             *   bother with this check if we're parsing only for syntax.  
             */
            if ((replace || modify) && !syntax_only_)
                G_tok->log_error(TCERR_REPFUNC_UNDEF);
        }
        else
        {
            /* replace or modify the previous definition if appropriate */
            if (replace)
            {
                /* 
                 *   check to see whether it's external or previously
                 *   defined in the same translation unit 
                 */
                if (func_sym->is_extern())
                {
                    /* 
                     *   The function was previously external, and we're
                     *   replacing it -- mark it as such so that we'll
                     *   know to perform the replacement at link time.  We
                     *   only need to do this when replacing an external
                     *   function, because if the function is defined in
                     *   the same translation unit then the replacement
                     *   will be completed right now, and we don't need to
                     *   do any more work at link time.  
                     */
                    func_sym->set_ext_replace(TRUE);
                }
                else
                {
                    CTcSymFunc *mod_base;

                    /*
                     *   The function was previously defined in the same
                     *   translation unit.  Mark the previous code body as
                     *   replaced so that we don't bother generating any
                     *   code for it. 
                     */
                    if (func_sym->get_code_body() != 0)
                        func_sym->get_code_body()->set_replaced(TRUE);

                    /*
                     *   If the function we're replacing modifies any base
                     *   functions, we're effectively replacing all of the
                     *   base functions as well.  None of these functions
                     *   will be reachable, since a modified base function is
                     *   only reachable through its modifying function; since
                     *   the function we're replacing is becoming
                     *   unreachable, therefore, all of its base functions
                     *   are as well.  
                     */
                    for (mod_base = func_sym->get_mod_base() ; mod_base != 0 ;
                         mod_base = mod_base->get_mod_base())
                    {
                        /* mark this base function as replaced */
                        if (mod_base->is_extern())
                        {
                            /* mark it as a replaced extern */
                            mod_base->set_ext_replace(TRUE);

                            /* note that our function replaces an external */
                            func_sym->set_ext_replace(TRUE);
                        }
                        else if (mod_base->get_code_body() != 0)
                        {
                            /* mark its code body as replaced */
                            mod_base->get_code_body()->set_replaced(TRUE);
                        }
                    }

                    /* 
                     *   since we're replacing the function, it no longer
                     *   has any modified base functions 
                     */
                    func_sym->set_mod_base(0);
                }
            }
            else if (modify)
            {
                CTcSymFunc *mod_sym;

                /* 
                 *   Create a new dummy symbol for the original version.
                 *   This symbol doesn't go in the symbol table; instead, we
                 *   link it into our modified base function list behind the
                 *   symbol table entry.
                 *   
                 *   The name is important, though: we have to give this an
                 *   empty name so that the object file reader will know that
                 *   this isn't an actual global symbol.  This will let the
                 *   object file loader's code stream reader know that it
                 *   can't fix up references to the code stream anchor by
                 *   looking at the global symbol table.  (We have to write
                 *   some extra code in the function symbol loader to deal
                 *   with this, since the code stream reader can't.)
                 *   
                 *   Note that we want the new base function symbol to keep
                 *   the same 'extern' attribute as the original symbol.  If
                 *   we're modifying an external function, this will allow us
                 *   to apply the modification at link time between this
                 *   module and the module where the imported original
                 *   function was defined.  
                 */
                mod_sym = new CTcSymFunc("", 0, FALSE,
                                         argc, opt_argc, varargs, has_retval,
                                         func_sym->is_multimethod(),
                                         func_sym->is_multimethod_base(),
                                         func_sym->is_extern(),
                                         func_sym->has_proto());

                /* 
                 *   remember the original global function in the modified
                 *   placeholder symbol 
                 */
                mod_sym->set_mod_global(func_sym);

                /* 
                 *   If the original has a code body, switch the code body to
                 *   use our new symbol's fixup list.
                 *   
                 *   The only way to reference the original code body is
                 *   through a 'replaced()' call in the modifying function,
                 *   so any fixups currently associated with the function
                 *   symbol should remain with the function symbol - in other
                 *   words, we don't have to move any existing fixups
                 *   anywhere.  
                 */
                if (func_sym->get_code_body() != 0)
                    func_sym->get_code_body()->set_symbol_fixup_list(
                        mod_sym, mod_sym->get_fixup_list_anchor());

                /* transfer the old code body to the modified base symbol */
                mod_sym->set_code_body(func_sym->get_code_body());

                /* 
                 *   put the new modified base function at the head of the
                 *   modified base list 
                 */
                mod_sym->set_mod_base(func_sym->get_mod_base());
                func_sym->set_mod_base(mod_sym);
            }
            
            /* 
             *   The function was already defined.  If this is an actual
             *   definition (not an 'extern' declaration), then clear the
             *   'extern' flag on the existing symbol.  
             */
            if (!is_extern)
                func_sym->set_extern(FALSE);
        }

        /* 
         *   If there's a code body, tell the code body about its symbol
         *   table entry.  (If the function is 'extern' it has no code
         *   body at this point.)  
         */
        if (code_body != 0)
            code_body->set_symbol_fixup_list(
                func_sym, func_sym->get_fixup_list_anchor());

        /* tell the function about its code body */
        func_sym->set_code_body(code_body);

        /* return the code body as the result */
        return code_body;

    default:
        /* log an error and continue from here */
        G_tok->log_error_curtok(TCERR_REQ_CODE_BODY);
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse an 'intrinsic' top-level statement 
 */
CTPNStmTop *CTcParser::parse_intrinsic(int *err)
{
    int list_done;
    ushort fn_set_id;
    ushort fn_set_idx;
    tc_toktyp_t tok;
    
    /* skip the 'intrinsic' keyword, and check for the function set name */
    if ((tok = G_tok->next()) == TOKT_SSTR)
    {
        /* tell the code generator to add this function set */
        fn_set_id = G_cg->add_fnset(G_tok->getcur()->get_text(),
                                    G_tok->getcur()->get_text_len());

        /* skip the name */
        G_tok->next();
    }
    else if (tok == TOKT_CLASS)
    {
        /* it's an intrinsic class (metaclass) definition - go parse it */
        return parse_intrinsic_class(err);
    }
    else
    {
        /* 
         *   note the error, then keep going, assuming that the name was
         *   missing but that the rest is correctly formed 
         */
        G_tok->log_error_curtok(TCERR_REQ_INTRINS_NAME);

        /* use an arbitrary function set ID, since we're failing */
        fn_set_id = 0;
    }

    /* we need an open brace after that */
    if (G_tok->cur() == TOKT_LBRACE)
    {
        /* skip the brace */
        G_tok->next();
    }
    else
    {
        /* note the error, but keep going */
        G_tok->log_error_curtok(TCERR_REQ_INTRINS_LBRACE);
    }

    /* keep going until we find the closing brace */
    for (list_done = FALSE, fn_set_idx = 0 ; !list_done ; )
    {
        CTcToken fn_tok;
        int argc;
        int optargc;
        int varargs;
        int varargs_list;
        CTcSymLocal *varargs_list_local;
        CTcSymBif *bif_sym;

        /* check what we have */
        switch(G_tok->cur())
        {
        case TOKT_RBRACE:
            /* closing brace - skip it, and we're done */
            G_tok->next();
            list_done = TRUE;
            break;

        case TOKT_EOF:
            /* end of file - log an error */
            G_tok->log_error(TCERR_EOF_IN_INTRINS);
            
            /* return failure */
            *err = TRUE;
            return 0;

        case TOKT_SEM:
            /* empty list element; skip it */
            G_tok->next();
            break;

        case TOKT_SYM:
            /* remember the function name */
            fn_tok = *G_tok->copycur();

            /* the next token must be an open paren */
            if (G_tok->next() == TOKT_LPAR)
            {
                /* skip the paren */
                G_tok->next();
            }
            else
            {
                /* log an error, and assume the parent was simply missing */
                G_tok->log_error_curtok(TCERR_REQ_INTRINS_LPAR);
            }

            /* 
             *   parse the formals - igore the names (hence 'count_only' =
             *   true), and allow optional arguments 
             */
            parse_formal_list(TRUE, TRUE, &argc, &optargc, &varargs,
                              &varargs_list, &varargs_list_local, err, 0,
                              FALSE, 0);
            if (*err)
                return 0;

            /* require the closing semicolon */
            if (parse_req_sem())
            {
                *err = TRUE;
                return 0;
            }

            /* add a global symbol table entry for the function */
            bif_sym = new CTcSymBif(fn_tok.get_text(),
                                    fn_tok.get_text_len(), FALSE,
                                    fn_set_id, fn_set_idx, TRUE,
                                    argc, argc + optargc, varargs);
            global_symtab_->add_entry(bif_sym);

            /* count the function set entry */
            ++fn_set_idx;

            /* done with this function */
            break;

        default:
            /* anything else is an error */
            G_tok->log_error_curtok(TCERR_REQ_INTRINS_SYM);

            /* skip the errant token and proceed */
            G_tok->next();
            break;
        }
    }

    /* 
     *   there's no node to return - a function set doesn't generate any
     *   code, but simply adds entries to the symbol table, which we've
     *   already done 
     */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse an 'intrinsic class' definition
 */
CTPNStmTop *CTcParser::parse_intrinsic_class(int *err)
{
    int meta_id;
    int list_done;
    int got_name_tok = FALSE;
    CTcToken meta_tok;
    CTcSymMetaclass *meta_sym = 0;
    
    /* skip the 'class' keyword and check the class name symbol */
    if (G_tok->next() == TOKT_SYM)
    {
        /* get the token */
        meta_tok = *G_tok->copycur();

        /* 
         *   See if it's defined yet.  If it was previously defined as an
         *   external metaclass, 
         */
        meta_sym = (CTcSymMetaclass *)global_symtab_->find(
            meta_tok.get_text(), meta_tok.get_text_len());
        if (meta_sym != 0
            && (meta_sym->get_type() != TC_SYM_METACLASS
                || !meta_sym->is_ext()))
        {
            /* duplicate definition - log an error and forget the symbol */
            G_tok->log_error_curtok(TCERR_REDEF_INTRINS_NAME);
            meta_sym = 0;
        }
        else
        {
            /* note that we successfully parsed the class name token */
            got_name_tok = TRUE;
        }

        /* move on to the next token */
        G_tok->next();
    }
    else
    {
        /* 
         *   note the error, then keep going, assuming the name was
         *   missing but that the rest is well-formed 
         */
        G_tok->log_error_curtok(TCERR_REQ_INTRINS_CLASS_NAME_SYM);
    }

    /* skip the name symbol, and check for the global name string */
    if (G_tok->cur() == TOKT_SSTR)
    {
        /* set up the definitions if we got a valid name token */
        if (got_name_tok)
        {
            /* if we don't already have a symbol table entry, create one */
            if (meta_sym == 0)
            {
                /* no prior symbol table entry - create it and add it */
                meta_sym = new CTcSymMetaclass(
                    meta_tok.get_text(), meta_tok.get_text_len(),
                    FALSE, 0, G_cg->new_obj_id());
                global_symtab_->add_entry(meta_sym);
            }
            else
            {
                /* 
                 *   we have a prior definition, which must have been an
                 *   external import from a symbol file; remove the external
                 *   attribute since we're actually defining it now
                 */
                meta_sym->set_ext(FALSE);
            }

            /* tell the code generator to add this metaclass */
            meta_id = G_cg->find_or_add_meta(
                G_tok->getcur()->get_text(), G_tok->getcur()->get_text_len(),
                meta_sym);

            /* 
             *   if the metaclass was already defined for another symbol,
             *   it's an error
             */
            CTcSymMetaclass *table_sym = G_cg->get_meta_sym(meta_id);
            if (table_sym != meta_sym)
                G_tok->log_error(TCERR_META_ALREADY_DEF,
                                 (int)table_sym->get_sym_len(),
                                 table_sym->get_sym());

            /* set the metaclass ID in the symbol */
            meta_sym->set_meta_idx(meta_id);
        }

        /* skip the name */
        G_tok->next();
    }
    else
    {
        /* 
         *   note the error, then keep going, assuming that the name was
         *   missing but that the rest is correctly formed 
         */
        G_tok->log_error_curtok(TCERR_REQ_INTRINS_CLASS_NAME);
    }

    /* check for the optional intrinsic superclass */
    if (G_tok->cur() == TOKT_COLON)
    {
        /* skip the colon and get the symbol */
        if (G_tok->next() != TOKT_SYM)
        {
            /* note the error, and just continue from here */
            G_tok->log_error_curtok(TCERR_REQ_INTRINS_SUPERCLASS_NAME);
        }
        else
        {
            /* look up the superclass in the global symbols */
            CTcSymMetaclass *sc_meta_sym =
                (CTcSymMetaclass *)global_symtab_->find(
                    G_tok->getcur()->get_text(),
                    G_tok->getcur()->get_text_len());

            /* make sure we found the symbol, and that it's a metaclass */
            if (sc_meta_sym == 0)
            {
                /* the intrinsic superclass is not defined */
                G_tok->log_error_curtok(TCERR_INTRINS_SUPERCLASS_UNDEF);
            }
            else if (sc_meta_sym->get_type() != TC_SYM_METACLASS)
            {
                /* this is not an intrinsic class */
                G_tok->log_error_curtok(TCERR_INTRINS_SUPERCLASS_NOT_INTRINS);
            }
            else
            {
                /* remember the supermetaclass */
                if (meta_sym != 0)
                    meta_sym->set_super_meta(sc_meta_sym);
            }

            /* we're done with the name token */
            G_tok->next();
        }
    }

    /* we need an open brace after that */
    if (G_tok->cur() == TOKT_LBRACE)
    {
        /* skip the brace */
        G_tok->next();
    }
    else
    {
        /* note the error, but keep going */
        G_tok->log_error_curtok(TCERR_REQ_INTRINS_CLASS_LBRACE);
    }

    /* keep going until we find the closing brace */
    for (list_done = FALSE ; !list_done ; )
    {
        CTcToken prop_tok;

        /* presume it won't be a 'static' property */
        int is_static = FALSE;

        /* check what we have */
        switch(G_tok->cur())
        {
        case TOKT_RBRACE:
            /* closing brace - skip it, and we're done */
            G_tok->next();
            list_done = TRUE;
            break;

        case TOKT_EOF:
            /* end of file - log an error */
            G_tok->log_error(TCERR_EOF_IN_INTRINS_CLASS);

            /* return failure */
            *err = TRUE;
            return 0;

        case TOKT_SEM:
            /* empty list element; skip it */
            G_tok->next();
            break;

        case TOKT_STATIC:
            /* note that we have a static property */
            is_static = TRUE;

            /* a symbol must follow */
            if (G_tok->next() != TOKT_SYM)
            {
                G_tok->log_error(TCERR_REQ_INTRINS_CLASS_PROP);
                break;
            }

            /* fall through to parse the rest of the property definition */

        case TOKT_SYM:
            /* remember the property name */
            prop_tok = *G_tok->copycur();

            /* add the property */
            if (meta_sym != 0)
                meta_sym->add_prop(prop_tok.get_text(),
                                   prop_tok.get_text_len(), 0, is_static);

            /* skip the property name symbol */
            G_tok->next();

            /* if there's a formal parameter list, parse it */
            if (G_tok->cur() == TOKT_LPAR)
            {
                int argc;
                int opt_argc;
                int varargs;
                int varargs_list;
                CTcSymLocal *varargs_list_local;

                /* skip the open paren */
                G_tok->next();
                
                /* parse the list, ignoring the symbols defined */
                parse_formal_list(TRUE, TRUE, &argc, &opt_argc, &varargs,
                                  &varargs_list, &varargs_list_local,
                                  err, 0, FALSE, 0);

                /* if a fatal error occurred, return failure */
                if (*err != 0)
                    return 0;
            }

            /* require a semicolon after the property definition */
            if (parse_req_sem())
            {
                *err = TRUE;
                return 0;
            }

            /* done with this function */
            break;

        default:
            /* anything else is an error */
            G_tok->log_error_curtok(TCERR_REQ_INTRINS_CLASS_PROP);

            /* skip the errant token and proceed */
            G_tok->next();
            break;
        }
    }

    /* 
     *   there's no node to return - a metaclass definition doesn't
     *   generate any code, but simply adds entries to the symbol table,
     *   which we've already done 
     */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse an 'object' statement - this can be either an 'object template'
 *   statement or an anonymous object definition for a base object.  
 */
CTPNStmTop *CTcParser::parse_object_stm(int *err, int trans)
{
    /* it's either a template definition or an anonymous object */
    if (G_tok->next() == TOKT_TEMPLATE)
    {
        /* 
         *   it's an 'object template' statement - go parse the template,
         *   with no superclass token 
         */
        return parse_template_def(err, 0);
    }
    else
    {
        /* 
         *   no 'template' token, so this is an anonymous object instance
         *   definition - put back the 'object' token 
         */
        G_tok->unget();

        /* parse the anonymous object statement */
        return parse_anon_object(err, 0, FALSE, 0, trans);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse an object template definition.  If the class token is null, this
 *   is a root object template; otherwise, the class token gives the class
 *   symbol with which to associate the template.  
 */
CTPNStmTop *CTcParser::parse_template_def(int *err, const CTcToken *class_tok)
{
    CTcToken prop_tok;
    CTcObjTemplateItem *alt_group_head;
    CTcObjTemplateItem *item_head;
    CTcObjTemplateItem *item_tail;
    CTcSymObj *class_sym;

    /* no items in our list yet */
    item_head = item_tail = alt_group_head = 0;
    size_t item_cnt = 0;

    /* presume we won't find an 'inherited' token */
    int found_inh = FALSE;

    /* 
     *   If there's a class token, it must refer to an object class.  If
     *   there's no such symbol defined yet, define it as an external
     *   object; otherwise, make sure the existing symbol is an object of
     *   metaclass tads-object.  
     */
    if (class_tok != 0)
    {
        /* 
         *   if the class token is 'string', it's a string template rather
         *   than an object template - go parse that separately if so 
         */
        if (class_tok->text_matches("string"))
            return parse_string_template_def(err);

        /* 
         *   Look up the symbol.  Don't mark the symbol as referenced; merely
         *   defining a template for an object doesn't require an external
         *   reference on the object.  
         */
        class_sym = (CTcSymObj *)get_global_symtab()->find_noref(
            class_tok->get_text(), class_tok->get_text_len(), 0);

        /* 
         *   if we didn't find it, define it; otherwise, ensure it's defined
         *   as an object 
         */
        if (class_sym == 0)
        {
            /* 
             *   it's undefined, so add a forward definition by creating the
             *   symbol as an external object 
             */
            class_sym = new CTcSymObj(class_tok->get_text(),
                                      class_tok->get_text_len(), FALSE,
                                      G_cg->new_obj_id(), TRUE,
                                      TC_META_TADSOBJ, 0);

            /* add it to the master symbol table */
            get_global_symtab()->add_entry(class_sym);
        }
        else if (class_sym->get_type() != TC_SYM_OBJ
                 || class_sym->get_metaclass() != TC_META_TADSOBJ)
        {
            /* it's defined incorrectly - flag the error */
            G_tok->log_error_curtok(TCERR_REDEF_AS_OBJ);

            /* forget the conflicting symbol and proceed with parsing */
            class_sym = 0;
        }
    }
    else
    {
        /* this is a root object template - there's no class symbol */
        class_sym = 0;
    }

    /* move on to the next token */
    G_tok->next();

    /* keep going until we run out of template tokens */
    int all_ok = TRUE;
    for (int done = FALSE ; !done ; )
    {
        tc_toktyp_t def_tok;
        int ok;
        int is_inh;
        int is_alt, is_opt;

        /* presume we will find a valid token */
        ok = TRUE;

        /* presume the defining token type is the current token */
        def_tok = G_tok->cur();

        /* presume this won't be an 'inherited' token */
        is_inh = FALSE;

        /* see what we have next */
        switch(G_tok->cur())
        {
        case TOKT_SEM:
            /* 
             *   that's the end of the statement - skip the semicolon, and
             *   we're done 
             */
            G_tok->next();
            done = TRUE;
            break;

        case TOKT_INHERITED:
            /* flag that we are to inherit superclass templates */
            is_inh = TRUE;
            found_inh = TRUE;
            break;

        case TOKT_SSTR:
        case TOKT_DSTR:
            /* string property */
            {
                utf8_ptr p;
                size_t rem;
                int valid;
                tc_toktyp_t kwtok;

                /* this is our property name token */
                prop_tok = *G_tok->copycur();

                /* 
                 *   this is also our defining token - the actual value in
                 *   object instances will use this string type 
                 */
                def_tok = G_tok->cur();
                
                /* 
                 *   make sure that the contents of the string forms a
                 *   valid symbol token 
                 */

                /* set up at the start of the token */
                p.set((char *)G_tok->getcur()->get_text());
                rem = G_tok->getcur()->get_text_len();

                /* make sure the first character is valid */
                valid = (rem != 0 && is_syminit(p.getch()));
                
                /* skip the first character */
                if (rem != 0)
                    p.inc(&rem);

                /* scan the rest of the string */
                for ( ; rem != 0 && valid ; p.inc(&rem))
                {
                    /* 
                     *   if this isn't a valid symbol character, we don't
                     *   have a valid symbol 
                     */
                    if (!is_sym(p.getch()))
                        valid = FALSE;
                }

                /* 
                 *   if the string is lexically valid as a symbol, make
                 *   sure it's not a keyword - if it is, it can't be a
                 *   property name 
                 */
                if (valid && G_tok->look_up_keyword(G_tok->getcur(), &kwtok))
                    valid = FALSE;

                /* if the symbol isn't valid, it's an error */
                if (!valid)
                {
                    /* log the error */
                    G_tok->log_error_curtok(TCERR_OBJ_TPL_STR_REQ_PROP);

                    /* note the problem */
                    ok = FALSE;
                }
            }
            break;

        case TOKT_LBRACK:
            /* list property - defining instances will use lists here */
            def_tok = G_tok->cur();

            /* the next token must be the property name */
            if (G_tok->next() != TOKT_SYM)
            {
                /* log an error */
                G_tok->log_error(TCERR_OBJ_TPL_OP_REQ_PROP,
                                 G_tok->get_op_text(def_tok),
                                 (int)G_tok->getcur()->get_text_len(),
                                 G_tok->getcur()->get_text());

                /* note that we don't have a valid token */
                ok = FALSE;

                /* 
                 *   if this token isn't ']' or something that looks like
                 *   an end-of-statement token, skip it 
                 */
                switch(G_tok->cur())
                {
                case TOKT_SEM:
                case TOKT_EOF:
                case TOKT_LBRACE:
                case TOKT_RBRACE:
                    /* 
                     *   don't skip any of these - they might be statement
                     *   enders 
                     */
                    break;

                case TOKT_RBRACK:
                    /* 
                     *   they must simply have left out the property - don't
                     *   skip this, since we'll skip a token shortly anyway 
                     */
                    break;

                default:
                    /* skip the errant token */
                    G_tok->next();
                    break;
                }
            }
            else
            {
                /* remember the property token */
                prop_tok = *G_tok->copycur();

                /* require the ']' token */
                if (G_tok->next() != TOKT_RBRACK)
                {
                    /* log the error */
                    G_tok->log_error_curtok(TCERR_OBJ_TPL_REQ_RBRACK);

                    /* 
                     *   proceed on the assumption that they simply left
                     *   out the ']' 
                     */
                }
            }
            break;

        case TOKT_AT:
        case TOKT_PLUS:
        case TOKT_MINUS:
        case TOKT_TIMES:
        case TOKT_DIV:
        case TOKT_MOD:
        case TOKT_ARROW:
        case TOKT_AND:
        case TOKT_NOT:
        case TOKT_BNOT:
        case TOKT_COMMA:
            /* the operator is the defining token */
            def_tok = G_tok->cur();

            /* the next token must be the property name */
            if (G_tok->next() != TOKT_SYM)
            {
                /* log an error */
                G_tok->log_error(TCERR_OBJ_TPL_OP_REQ_PROP,
                                 G_tok->get_op_text(def_tok),
                                 (int)G_tok->getcur()->get_text_len(),
                                 G_tok->getcur()->get_text());
                
                /* note that we don't have a valid token */
                ok = FALSE;
            }

            /* remember the property token */
            prop_tok = *G_tok->copycur();
            break;

        case TOKT_LBRACE:
        case TOKT_RBRACE:
        case TOKT_EOF:
        case TOKT_OBJECT:
            /* they must have left off the ';' */
            G_tok->log_error_curtok(TCERR_OBJ_TPL_BAD_TOK);

            /* 
             *   stop parsing here, assuming the statement should have
             *   ended by now 
             */
            done = TRUE;
            ok = FALSE;
            break;

        default:
            /* log an error and skip the invalid token */
            G_tok->log_error_curtok(TCERR_OBJ_TPL_BAD_TOK);
            G_tok->next();
            ok = FALSE;
            break;
        }

        /* presume we won't find an alternative or optionality suffix */
        is_opt = FALSE;
        is_alt = FALSE;

        /* 
         *   if we haven't reached the end of the statement, check to see if
         *   the item is optional or an alternative 
         */
        if (!done)
        {
            /* get the next token */
            G_tok->next();
            
            /* check to see if the item is optional */
            if (G_tok->cur() == TOKT_QUESTION)
            {
                /* it's optional */
                is_opt = TRUE;

                /* skip the '?' */
                G_tok->next();
            }

            /* check for an alternative */
            if (G_tok->cur() == TOKT_OR)
            {
                /* this item is an alternative */
                is_alt = TRUE;

                /* skip the '|' */
                G_tok->next();
            }
        }

        /* 
         *   If the previous item was followed by '|', we're in the same
         *   alternative group with that item.  This means we're optional if
         *   it's marked as optional, and all of the preceding items in our
         *   group are optional if we're marked as optional.  
         */
        if (item_tail != 0 && item_tail->is_alt_)
        {
            /* 
             *   if we're optional, mark all prior items in the group as
             *   optional 
             */
            if (is_opt)
            {
                CTcObjTemplateItem *item;

                /* mark everything in our group as optional */
                for (item = alt_group_head ; item != 0 ; item = item->nxt_)
                    item->is_opt_ = TRUE;
            }
            
            /* if the prior item was optional, we're optional */
            if (item_tail->is_opt_)
                is_opt = TRUE;
        }

        /* 
         *   if we encountered any problems, note that we have an error in
         *   the overall statement 
         */
        if (!ok)
            all_ok = FALSE;

        /* if we found a valid property token, add it to the template */
        if (!done && ok)
        {
            CTcSymProp *prop_sym;
            CTcObjTemplateItem *item;

            /* presume we won't create a new item */
            item = 0;

            /* check to see if it's a property or an 'inherited' token */
            if (is_inh)
            {
                /* create an 'inherited' template item */
                item = new (G_prsmem)
                       CTcObjTemplateItem(0, TOKT_INHERITED, FALSE, FALSE);
            }
            else
            {
                /* make sure we have a valid property name */
                prop_sym = look_up_prop(&prop_tok, FALSE);
                if (prop_sym == 0)
                {
                    /* couldn't find the property - log an error */
                    G_tok->log_error(TCERR_OBJ_TPL_SYM_NOT_PROP,
                                     (int)prop_tok.get_text_len(),
                                     prop_tok.get_text());
                }
                else if (prop_sym->is_vocab())
                {
                    /* dictionary properties are not valid in templates */
                    G_tok->log_error(TCERR_OBJ_TPL_NO_VOCAB,
                                     (int)prop_sym->get_sym_len(),
                                     prop_sym->get_sym());
                }
                else
                {
                    /* 
                     *   Scan the list so far to ensure that this same
                     *   property isn't already part of the list.  However,
                     *   do allow duplicates within the run of alternatives
                     *   of which the new item will be a part, since we'll
                     *   only end up using one of the alternatives and hence
                     *   the property will only be entered once in the
                     *   property even if it appears multiple times in the
                     *   run.  The run of alternatives of which we're a part
                     *   is the final run in the list, since we're being
                     *   added at the end of the list.  
                     */
                    for (item = item_head ; item != 0 ; item = item->nxt_)
                    {
                        /* check for a duplicate of this item's property */
                        if (item->prop_ == prop_sym)
                        {
                            CTcObjTemplateItem *sub;
                            
                            /* 
                             *   if everything from here to the end of the
                             *   list is marked as an alternative, then we're
                             *   just adding a duplicate property to a run of
                             *   alternatives, which is fine 
                             */
                            for (sub = item ; sub != 0 && sub->is_alt_ ;
                                 sub = sub->nxt_) ;

                            /* 
                             *   if we found a non-alternative following this
                             *   item, then this is indeed a duplicate; if we
                             *   didn't find any non-alternatives, we're just
                             *   adding this to a run of alternatives, so
                             *   we're okay 
                             */
                            if (sub != 0)
                            {
                                /* it's a duplicate - log the error */
                                G_tok->log_error(TCERR_OBJ_TPL_PROP_DUP,
                                    (int)prop_sym->get_sym_len(),
                                    prop_sym->get_sym());
                            
                                /* no need to look any further */
                                break;
                            }
                        }
                    }
                    
                    /* create the template item */
                    item = new (G_prsmem)
                           CTcObjTemplateItem(prop_sym, def_tok,
                                              is_alt, is_opt);
                }
            }
            
            /* if we have a valid new item, add it to our list */
            if (item != 0)
            {
                /* 
                 *   if we're an alternative and the prior item was not,
                 *   we're the head of a new alternative group; if we're not
                 *   an alternative, then we're not in an alternative group
                 *   after this item 
                 */
                if (is_alt && (item_tail == 0 || !item_tail->is_alt_))
                {
                    /* we're the head of a new group */
                    alt_group_head = item;
                }
                else if (!is_alt)
                {
                    /* we're no longer in an alternative group */
                    alt_group_head = 0;
                }

                /* link it into our list */
                if (item_tail != 0)
                    item_tail->nxt_ = item;
                else
                    item_head = item;
                item_tail = item;
            
                /* count it */
                ++item_cnt;
            }
        }
    }

    /* if the template is empty, warn about it */
    if (item_cnt == 0)
    {
        /* flag it as an error */
        G_tok->log_error(TCERR_TEMPLATE_EMPTY);

        /* there's no point in saving an empty template */
        all_ok = FALSE;
    }

    /* if we were successful, add the new template to our master list */
    if (all_ok)
    {
        /*
         *   If we found an 'inherited' keyword, we must expand the template
         *   with all inherited templates.  Otherwise, simply add the
         *   template exactly as given. 
         */
        if (found_inh)
        {
            /*
             *   Traverse all superclass templates and add each inherited
             *   form. 
             */
            add_inherited_templates(class_sym, item_head, item_cnt);
        }
        else
        {
            /* add our single template */
            add_template_def(class_sym, item_head, item_cnt);
        }
    }

    /* an 'object template' statement generates no tree data */
    return 0;
}

/*
 *   Linked list structure for inherited template list 
 */
struct inh_tpl_entry
{
    inh_tpl_entry() { }
    
    /* this template */
    CTcObjTemplate *tpl;
    
    /* next in list */
    inh_tpl_entry *nxt;
};

/*
 *   Expand a template definition for inherited superclass templates 
 */
void CTcParser::add_inherited_templates(CTcSymObj *sc_sym,
                                        CTcObjTemplateItem *item_head,
                                        size_t item_cnt)
{
    inh_tpl_entry *head;
    inh_tpl_entry *tail;
    inh_tpl_entry *cur;
    CTcObjTemplate *tpl;
    
    /* start with an empty list */
    head = tail = 0;

    /* 
     *   traverse the superclass tree, building a list of all unique
     *   templates inherited from the tree 
     */
    build_super_template_list(&head, &tail, sc_sym);

    /* expand our template for each item in the superclass template list */
    for (cur = head ; cur != 0 ; cur = cur->nxt)
        expand_and_add_inherited_template(sc_sym, item_head, cur->tpl);

    /* expand our template for each item in the root template list */
    for (tpl = template_head_ ; tpl != 0 ; tpl = tpl->nxt_)
        expand_and_add_inherited_template(sc_sym, item_head, tpl);

    /* 
     *   finally, add the trivial expansion (in other words, with the
     *   'inherited' keyword replaced by nothing) 
     */
    expand_and_add_inherited_template(sc_sym, item_head, 0);
}

/*
 *   Expand a template that contains an 'inherited' keyword, substituting
 *   the given inherited template list for the 'inherited' keyword, and add
 *   the resulting template to the template list for the given class. 
 */
void CTcParser::expand_and_add_inherited_template(
    CTcSymObj *sc_sym, CTcObjTemplateItem *item_head,
    CTcObjTemplate *sc_tpl)
{
    CTcObjTemplateItem *exp_head;
    CTcObjTemplateItem *exp_tail;
    CTcObjTemplateItem *cur;
    CTcObjTemplateItem *end_nxt;
    size_t cnt;

    /* our new list is empty thus far */
    exp_head = exp_tail = 0;

    /*
     *   Build a new list.  Copy items from the original list until we find
     *   the 'inherited' item, then copy items from the inherited template,
     *   and finally copy the remaining items from the original list. 
     */
    for (end_nxt = 0, cur = item_head, cnt = 0 ; cur != 0 ; cur = cur->nxt_)
    {
        CTcObjTemplateItem *new_item;
        
        /* if this is the 'inherited' entry, switch to the superclass list */
        if (cur->tok_type_ == TOKT_INHERITED)
        {
            /* 
             *   if we're doing a trivial expansion, indicated by a null
             *   superclass template, simply skip the 'inherited' keyword
             *   altogether and proceed with copying the main list
             */
            if (sc_tpl == 0)
                continue;
            
            /* remember where to pick up in the containing list */
            end_nxt = cur;

            /* switch to the inherited list */
            cur = sc_tpl->items_;
        }

        /* make a copy of the current item */
        new_item = new (G_prsmem)
                   CTcObjTemplateItem(cur->prop_, cur->tok_type_,
                                      cur->is_alt_, cur->is_opt_);

        /* link it into our list */
        if (exp_tail != 0)
            exp_tail->nxt_ = new_item;
        else
            exp_head = new_item;
        exp_tail = new_item;

        /* count the item */
        ++cnt;

        /* 
         *   if we're at the end of the current list, and we have an outer
         *   list to resume, resume the outer list 
         */
        if (cur->nxt_ == 0 && end_nxt != 0)
        {
            /* resume at the outer list */
            cur = end_nxt;

            /* forget the outer list now that we're resuming it */
            end_nxt = 0;
        }
    }

    /* add the expanded template definition */
    add_template_def(sc_sym, exp_head, cnt);
}

/*
 *   Build a list of inherited templates given the base class.  We add only
 *   templates from superclasses of the given class.
 *   
 *   We add only templates that aren't already in the list, because the
 *   class could conceivably inherit from the same base class more than
 *   once, if it (or a superclass) inherits from multiple base classes that
 *   share a common base class.  
 */
void CTcParser::build_super_template_list(inh_tpl_entry **list_head,
                                          inh_tpl_entry **list_tail,
                                          CTcSymObj *sc_sym)
{
    CTPNSuperclass *sc;

    /* if this object was defined as a subclass of 'object', we're done */
    if (sc_sym->sc_is_root())
        return;
    
    /* scan all superclasses of the given superclass */
    for (sc = sc_sym->get_sc_name_head() ; sc != 0 ; sc = sc->nxt_)
    {
        CTcSymObj *sc_sym_cur;
        CTcObjTemplate *tpl;
        
        /* if this item is not an object, skip it */
        sc_sym_cur = (CTcSymObj *)sc->get_sym();
        if (sc_sym_cur == 0 || sc_sym_cur->get_type() != TC_SYM_OBJ)
            continue;

        /* 
         *   scan this superclass's templates and add each one that's not
         *   already in the list 
         */
        for (tpl = sc_sym_cur->get_first_template() ; tpl != 0 ;
             tpl = tpl->nxt_)
        {
            inh_tpl_entry *entry;

            /* find this template in the inherited list */
            for (entry = *list_head ; entry != 0 ; entry = entry->nxt)
            {
                /* if it's in the list, stop looking for it */
                if (entry->tpl == tpl)
                    break;
            }

            /* if we didn't find this template, add it */
            if (entry == 0)
            {
                /* create a new list item */
                entry = new (G_prsmem) inh_tpl_entry();
                entry->tpl = tpl;

                /* 
                 *   Link it in at the end of the list.  Note that we want to
                 *   link it at the end because this puts the template
                 *   inheritance list in the same order as the superclass
                 *   definitions.  Since we search the template list in
                 *   order, this ensures that we find the first matching
                 *   template in superclass inheritance order in cases where
                 *   there are multiple superclasses.  
                 */
                if (*list_tail != 0)
                    (*list_tail)->nxt = entry;
                else
                    *list_head = entry;
                *list_tail = entry;
                entry->nxt = 0;
            }
        }

        /* inherit from this superclass's superclasses */
        build_super_template_list(list_head, list_tail, sc_sym_cur);
    }
}

/*
 *   Add a template definition 
 */
void CTcParser::add_template_def(
    CTcSymObj *class_sym, CTcObjTemplateItem *item_head, size_t item_cnt)
{
    /* create the new template list entry */
    CTcObjTemplate *tpl = new (G_prsmem) CTcObjTemplate(item_head, item_cnt);
    
    /* 
     *   link it into the appropriate list - if it's associated with a
     *   class, link it into the class symbol's list; otherwise link it into
     *   the master list for the root object class 
     */
    if (class_sym != 0)
    {
        /* link it into the class's list */
        class_sym->add_template(tpl);
    }
    else
    {
        /* it's for the root class - link it into the master list */
        if (template_tail_ != 0)
            template_tail_->nxt_ = tpl;
        else
            template_head_ = tpl;
        template_tail_ = tpl;
    }
    
    /* 
     *   if this is the longest template so far, extend the template
     *   instance expression array to make room for parsing this template's
     *   instances 
     */
    if (item_cnt > template_expr_max_)
    {
        /* 
         *   note the new length, rounding up to ensure that we don't have
         *   to repeatedly reallocate if we have several more of roughly
         *   this same length 
         */
        template_expr_max_ = (item_cnt + 15) & ~15;
        
        /* reallocate the list */
        template_expr_ = (CTcObjTemplateInst *)G_prsmem->
                         alloc(sizeof(template_expr_[0])
                               * template_expr_max_);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   String template definition 
 */

void CTcStrTemplate::append_tok(const CTcToken *tok)
{
    /* create a new list element for the token */
    CTcTokenEle *ele = new (G_prsmem)CTcTokenEle();
    G_tok->copytok(ele, tok);

    /* add it to the tail of the list */
    if (tail != 0)
        tail->setnxt(ele);
    else
        head = ele;
    tail = ele;

    /* count the token */
    cnt++;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a string template definition:
 *   
 *.    string template <<tok...>> funcName;
 *   
 *   The 'tok' entries are the literal tokens to match for the embedding
 *   contents.  We can have zero or more of these; a completely empty
 *   template sets up a function that processes every expression not
 *   processed by another template.  Optionally, one token may be '*', which
 *   represents an actual expression.  If there's no '*', the template simply
 *   matches the literal token list exactly as given, and doesn't accept an
 *   expression.  'funcName' is a symbol giving the name of the function
 *   that's to be invoked at run-time when this template is used.  If there's
 *   a '*', this function takes one argument giving the expression value for
 *   the '*', otherwise it takes no arguments.  
 */
CTPNStmTop *CTcParser::parse_string_template_def(int *err)
{
    /* set up an empty string template object */
    CTcStrTemplate *tpl = new (G_prsmem) CTcStrTemplate();

    /* check that the template starts with << */
    if (G_tok->next() == TOKT_SHL)
        G_tok->next();
    else
        G_tok->log_error_curtok(TCERR_STRTPL_MISSING_LANGLE);

    /* keep going until we find the >> or end of statement */
    for (int done = FALSE ; !done ; )
    {
        switch (G_tok->cur())
        {
        case TOKT_ASHR:
            /* >> - end of the token list */
            G_tok->next();
            done = TRUE;
            break;

        case TOKT_SEM:
        case TOKT_RBRACE:
            /* 
             *   we must be missing the >> at the end of the list - log an
             *   error and return, since we're assuming we've run out of
             *   tokens in the template definition 
             */
            G_tok->log_error_curtok(TCERR_STRTPL_MISSING_RANGLE);
            return 0;

        default:
            /* add the token to the template's list */
            tpl->append_tok(G_tok->getcur());

            /* if this is a '*' token, note it */
            if (G_tok->cur() == TOKT_TIMES)
            {
                /* if we already have a '*', it's an error */
                if (tpl->star)
                    G_tok->log_error(TCERR_STRTPL_MULTI_STAR);

                /* note it in the template */
                tpl->star = TRUE;
            }

            /* on to the next token */
            G_tok->next();
            break;
        }
    }

    /* we need a symbol for the function */
    if (G_tok->cur() != TOKT_SYM)
    {
        /* missing symbol - skip to the next semicolon or brace */
        if (skip_to_sem())
            *err = TRUE;
        
        /* give up */
        return 0;
    }

    /* 
     *   Our requirement for the processor function is that it takes one
     *   argument if we have a '*' symbol in the token list, otherwise no
     *   arguments.  
     */
    int argc = tpl->star ? 1 : 0;

    /* look up the symbol */
    const CTcToken *functok = G_tok->getcur();
    CTcSymFunc *funcsym = (CTcSymFunc *)global_symtab_->find(
        functok->get_text(), functok->get_text_len());

    /* 
     *   if it's defined, check that it matches; otherwise add an implicit
     *   'extern' declaration for it 
     */
    if (funcsym != 0)
    {
        /*
         *   It's already defined.  Make sure it accepts the argument count
         *   we want to pass it.  If it's not external, make sure that it
         *   returns a value.  (We'll have to give it the benefit of the
         *   doubt if it's external, as the 'extern' declaration has no way
         *   to say one way or the other.)  
         */
        if (!funcsym->argc_ok(argc)
            || (!funcsym->is_extern() && !funcsym->has_retval()))
            G_tok->log_error(TCERR_STRTPL_FUNC_MISMATCH,
                             (int)functok->get_text_len(),
                             functok->get_text(), argc);
    }
    else
    {
        /* 
         *   It's not defined yet, so add an extern definition for it.  The
         *   function takes one argument if there's a '*' in the template,
         *   otherwise no arguments. 
         */
        funcsym = new CTcSymFunc(
            functok->get_text(), functok->get_text_len(), FALSE,
            tpl->star ? 1 : 0, 0, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE);

        /* add it to the symbol table */
        global_symtab_->add_entry(funcsym);
    }

    /* save the function symbol in the template */
    tpl->func = funcsym;

    /* link the template into the parser's global list */
    if (str_template_tail_ != 0)
        str_template_tail_->nxt = tpl;
    else
        str_template_head_ = tpl;
    str_template_tail_ = tpl;

    /* skip the function symbol and require a semicolon */
    G_tok->next();
    if (parse_req_sem())
        *err = TRUE;

    /* the 'string template' statement generates no parse tree entry */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Parse an object or function definition 
 */
CTPNStmTop *CTcParser::parse_object_or_func(int *err, int replace,
                                            int suppress_error,
                                            int *suppress_next_error)
{
    CTcToken init_tok;
    int trans;

    /* if the token is 'transient', it's an object definition */
    if ((trans = G_tok->cur() == TOKT_TRANSIENT) != 0)
    {
        /* skip the 'transient' keyword */
        G_tok->next();

        /* 
         *   if 'object' follows, treat this as an object statement;
         *   otherwise, we'll process using our normal handling, nothing our
         *   'transient' status 
         */
        if (G_tok->cur() == TOKT_OBJECT)
            return parse_object_stm(err, TRUE);
    }

    /* remember the initial token */
    init_tok = *G_tok->copycur();
    
    /* check the next token */
    switch(G_tok->next())
    {
    case TOKT_COLON:
        /* 
         *   it's an object definition - back up to the object name symbol
         *   and go parse the object definition 
         */
        G_tok->unget();
        return parse_object(err, replace, FALSE, FALSE, 0, trans);

    case TOKT_LPAR:
        /* 'transient' isn't allowed here */
        if (trans)
            G_tok->log_error(TCERR_INVAL_TRANSIENT);

        /* 
         *   it's a function definition - back up to the function name
         *   symbol and go parse the function 
         */
        G_tok->unget();
        return parse_function(err, FALSE, replace, FALSE, FALSE);

    case TOKT_TEMPLATE:
        /* 'transient template' is an error - flag it, but proceed anyway */
        if (trans)
            G_tok->log_error(TCERR_INVAL_TRANSIENT);

        /* it's a template for a specific object - parse it */
        return parse_template_def(err, &init_tok);

    default:
        /* 
         *   if the first token was a symbol that has already been defined
         *   as a class or object, this must be an anonymous object
         *   definition 
         */
        if (init_tok.gettyp() == TOKT_SYM)
        {
            CTcSymObj *sym;
            
            /* look up the symbol in the global symbol table */
            sym = (CTcSymObj *)
                  global_symtab_->find(init_tok.get_text(),
                                       init_tok.get_text_len());

            /* 
             *   If it's a TADS Object symbol, this must be an anonymous
             *   instance definition.  If the symbol is not yet defined
             *   and we're doing a syntax-only pass, assume the same
             *   thing.  
             */
            if ((sym !=0
                 && sym->get_type() == TC_SYM_OBJ
                 && sym->get_metaclass() == TC_META_TADSOBJ)
                || (sym == 0 && syntax_only_))
            {
                /*
                 *   If this is a 'replace' statement, we must have a class
                 *   list with the replacement object, so this is invalid. 
                 */
                if (replace)
                {
                    /* log the error, but continue parsing despite it */
                    G_tok->log_error(TCERR_REPLACE_OBJ_REQ_SC);
                }

                /* put the initial symbol back */
                G_tok->unget();

                /* parse the anonymous object definition */
                return parse_anon_object(err, 0, FALSE, 0, trans);
            }
        }
        else if (init_tok.gettyp() == TOKT_OBJECT)
        {
            /* 
             *   this is a base anonymous object - put the 'object' token
             *   back and go parse the object definition
             */
            G_tok->unget();
            return parse_anon_object(err, 0, FALSE, 0, trans);
        }
        
        /* 
         *   anything else is an error - log an error, unless we just
         *   logged this same error on the previous token 
         */
        if (!suppress_error)
            G_tok->log_error_curtok(TCERR_REQ_FUNC_OR_OBJ);

        /* 
         *   suppress this same error if it occurs again on the next
         *   token, since if it does we're probably just scanning past
         *   tons of garbage trying to resynchronize 
         */
        if (suppress_next_error != 0)
            *suppress_next_error = TRUE;
        return 0;
    }
}

/*
 *   Parse a '+' object definition, or a '+ property' statement 
 */
CTPNStmTop *CTcParser::parse_plus_object(int *err)
{
    int cnt;
    int is_class;
    int anon;
    int trans;
    
    /* count the '+' or '++' tokens */
    for (cnt = 0 ; G_tok->cur() == TOKT_PLUS || G_tok->cur() == TOKT_INC ;
          G_tok->next())
    {
        /* count it */
        ++cnt;

        /* '++' counts twice for obvious reasons */
        if (G_tok->cur() == TOKT_INC)
            ++cnt;
    }

    /* 
     *   if the count is one, and the next token is 'property', it's the
     *   '+' property definition 
     */
    if (cnt == 1 && G_tok->cur() == TOKT_PROPERTY)
    {
        /* skip the 'property' token and make sure we have a symbol */
        if (G_tok->next() != TOKT_SYM)
        {
            /* log the error */
            G_tok->log_error_curtok(TCERR_PLUSPROP_REQ_SYM);

            /* if it's anything other than a ';', skip it */
            if (G_tok->cur() != TOKT_SEM)
                G_tok->next();
        }
        else
        {
            /* set the property */
            plus_prop_ = look_up_prop(G_tok->getcur(), TRUE);

            /* skip the token */
            G_tok->next();
        }

        /* require the closing semicolon */
        if (parse_req_sem())
        {
            *err = TRUE;
            return 0;
        }

        /* done - there's no statement object for this directive */
        return 0;
    }

    /* presume it's an ordinary object */
    is_class = FALSE;
    trans = FALSE;
    anon = FALSE;

    /* check for the 'class' keyword or the 'transient' keyword */
    if (G_tok->cur() == TOKT_CLASS)
    {
        /* skip the 'class' keyword */
        G_tok->next();

        /* note it */
        is_class = TRUE;
    }
    else if (G_tok->cur() == TOKT_TRANSIENT)
    {
        /* skip the 'transient' keyword */
        G_tok->next();

        /* note it */
        trans = TRUE;
    }

    /* 
     *   check to see if the object name is followed by a colon - if not,
     *   it's an anonymous object definition 
     */
    if (G_tok->cur() == TOKT_SYM)
    {
        /* check the next symbol to see if it's a colon */
        anon = (G_tok->next() != TOKT_COLON);

        /* put back the token */
        G_tok->unget();
    }
    else if (G_tok->cur() == TOKT_OBJECT)
    {
        /* it's an anonymous base object */
        anon = TRUE;
    }

    /* parse appropriately for anonymous or named object */
    if (anon)
    {
        /* parse the anonymous object definition */
        return parse_anon_object(err, cnt, FALSE, 0, trans);
    }
    else
    {
        /* parse the object definition */
        return parse_object(err, FALSE, FALSE, is_class, cnt, trans);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Find or define an object symbol. 
 */
CTcSymObj *CTcParser::find_or_def_obj(
    const char *tok_txt, size_t tok_len, int replace, int modify,
    int *is_class, CTcSymObj **mod_orig_sym, CTcSymMetaclass **meta_sym,
    int *trans)
{
    /* we don't have a modify base symbol yet */
    *mod_orig_sym = 0;

    /* presume we won't find a modified metaclass definition */
    if (meta_sym != 0)
        *meta_sym = 0;

    /* look up the symbol to see if it's already defined */
    CTcSymbol *sym = global_symtab_->find(tok_txt, tok_len);

    /* check for 'modify' used with an intrinsic class */
    CTcSymObj *obj_sym;
    if (meta_sym != 0
        && modify
        && sym != 0
        && sym->get_type() == TC_SYM_METACLASS)
    {
        /*
         *   We're modifying an intrinsic class.  In this case, the base
         *   object is the previous modification object for the class, or no
         *   object at all.  In either case, we must create a new object and
         *   assign it to the metaclass.  
         */

        /* get the metaclass symbol */
        *meta_sym = (CTcSymMetaclass *)sym;

        /* get the original modification object */
        CTcSymObj *old_mod_obj = (*meta_sym)->get_mod_obj();

        /* 
         *   If there is no original modification object, create one - this
         *   is necessary so that there's always a dummy object at the root
         *   of the modification chain in each object file.  During linking,
         *   when one object file modifies one of these chains loaded from
         *   another object file, we'll use the pointer to the dummy root
         *   object in the second object file to point instead to the top of
         *   the chain in the first object file.  
         */
        if (old_mod_obj == 0)
        {
            /* create an anonymous base object */
            old_mod_obj = CTcSymObj::synthesize_modified_obj_sym(FALSE);

            /* the object is of metaclass IntrinsicClassModifier */
            old_mod_obj->set_metaclass(TC_META_ICMOD);

            /* give the dummy base object an empty statement */
            CTPNStmObject *base_stm = new CTPNStmObject(old_mod_obj, FALSE);
            old_mod_obj->set_obj_stm(base_stm);

            /* 
             *   add the base statement to our nested top-level statement
             *   list, so that we generate this object's code 
             */
            add_nested_stm(base_stm);
        }

        /* 
         *   note the superclass of the new modification object - this is
         *   simply the previous modification object, which we're further
         *   modifying, thus making this previous modification object the
         *   base of the new modification 
         */
        *mod_orig_sym = old_mod_obj;

        /* create a symbol for the new modification object */
        obj_sym = CTcSymObj::synthesize_modified_obj_sym(FALSE);

        /* the object is of metaclass IntrinsicClassModifier */
        obj_sym->set_metaclass(TC_META_ICMOD);

        /* set the new object's base modified object */
        obj_sym->set_mod_base_sym(old_mod_obj);

        /* make this the new metaclass modifier object */
        (*meta_sym)->set_mod_obj(obj_sym);

        /* return the symbol */
        return obj_sym;
    }

    /* assume for now that the symbol is an object (we'll check shortly) */
    obj_sym = (CTcSymObj *)sym;
    if (obj_sym != 0)
    {
        /* 
         *   This symbol is already defined.  The previous definition must
         *   be an object, or this definition is incompatible.  If it's an
         *   object, then it must have been external, *or* we must be
         *   replacing or modifying the previous definition.  
         */
        if (obj_sym->get_type() != TC_SYM_OBJ)
        {
            /* 
             *   it's not an object - log an error; continue parsing the
             *   definition for syntax, but the definition is ultimately
             *   invalid because the object name is invalid 
             */
            G_tok->log_error_curtok(TCERR_REDEF_AS_OBJ);

            /* ignore the definition */
            obj_sym = 0;
        }
        else if ((modify || replace)
                 && obj_sym->get_metaclass() != TC_META_TADSOBJ)
        {
            /* 
             *   we can't modify or replace BigNumber instances, Dictionary
             *   instances, or anything other than ordinary TADS objects 
             */
            G_tok->log_error(TCERR_CANNOT_MOD_OR_REP_TYPE);

            /* ignore the definition */
            obj_sym = 0;
        }
        else if (modify)
        {
            CTPNStmObject *mod_orig_stm;
            CTcSymObj *mod_sym;

            /* 
             *   remember the original pre-modified object parse tree - we
             *   might need to delete properties in the tree (for any
             *   properties defined with the "replace" keyword) 
             */
            mod_orig_stm = obj_sym->get_obj_stm();

            /* 
             *   We're modifying the previous definition.  The object was
             *   previously defined within this translation unit, so the
             *   modification can be conducted entirely within the
             *   translation unit (i.e., there's no link-time work we'll
             *   need to do).
             *   
             *   Create a new fake symbol for the original object, and
             *   assign it a new ID.  We need a symbol table entry because
             *   the object statement parse tree depends upon having a
             *   symbol table entry, and because the symbol table entry is
             *   the mechanism by which the linker can fix up the object ID
             *   from object file local numbering to image file global
             *   numbering.  
             */
            mod_sym = CTcSymObj::synthesize_modified_obj_sym(FALSE);

            if (mod_orig_stm != 0)
            {
                /* 
                 *   transfer the old object definition tree to the new
                 *   symbol, since the new symbol now holds the original
                 *   object - the real symbol is going to have the new
                 *   (modified) object definition instead 
                 */
                mod_sym->set_obj_stm(mod_orig_stm);
                mod_orig_stm->set_obj_sym(mod_sym);

                /* mark the original as modified */
                mod_orig_stm->set_modified(TRUE);

                /* keep the same class attributes as the original */
                *is_class = mod_orig_stm->is_class();

                /* keep the same transient status as the original */
                *trans = mod_orig_stm->is_transient();
            }
            else
            {
                /* 
                 *   it's external - use the class and transient status of
                 *   the imported symbol 
                 */
                *is_class = obj_sym->is_class();
                *trans = obj_sym->is_transient();
            }

            /* 
             *   transfer the property deletion list from the original
             *   symbol to the new symbol 
             */
            mod_sym->set_del_prop_head(obj_sym->get_first_del_prop());
            obj_sym->set_del_prop_head(0);

            /* transfer the vocabulary list from the original */
            mod_sym->set_vocab_head(obj_sym->get_vocab_head());
            obj_sym->set_vocab_head(0);

            /* copy the 'transient' status to the new symbol */
            if (obj_sym->is_transient())
                mod_sym->set_transient();

            /*
             *   Build a token containing the synthesized object name for
             *   the pre-modified object, so that we can refer to it in the
             *   superclass list of the new object.  
             */
            *mod_orig_sym = mod_sym;

            /* 
             *   transfer the base 'modify' symbol to the new symbol for the
             *   original object 
             */
            mod_sym->set_mod_base_sym(obj_sym->get_mod_base_sym());

            /* 
             *   If we're modifying an external object, we must mark the
             *   symbol as such - this will put the 'modify' flag with the
             *   symbol in the object file, so that we'll apply the 'modify'
             *   at link time.  
             */
            if (obj_sym->is_extern())
            {
                /* mark this symbol as modifying an external symbol */
                obj_sym->set_ext_modify(TRUE);

                /* we're no longer external */
                obj_sym->set_extern(FALSE);

                /* mark the synthesized original symbol as external */
                mod_sym->set_extern(TRUE);
            }

            /* remember the base symbol */
            obj_sym->set_mod_base_sym(mod_sym);

            /* 
             *   keep the original version's dictionary with the base
             *   symbol, and change the modified symbol to use the
             *   dictionary now active 
             */
            mod_sym->set_dict(obj_sym->get_dict());
            obj_sym->set_dict(dict_cur_);
        }
        else if (obj_sym->is_extern())
        {
            /* 
             *   it was previously defined as external, so this definition
             *   is acceptable - simply remove the 'extern' flag from the
             *   object symbol now that we have the real definition 
             */
            obj_sym->set_extern(FALSE);

            /* set the object's dictionary to the active dictionary */
            obj_sym->set_dict(dict_cur_);

            /*
             *   If we're replacing an external object, we must mark the
             *   symbol as a replacement - this will put the 'replace' flag
             *   with the symbol in the object file, so that we'll apply the
             *   replacement at link time.  
             */
            if (replace)
                obj_sym->set_ext_replace(TRUE);
        }
        else if (replace)
        {
            /* 
             *   We're replacing the previous definition.  We can simply
             *   discard the previous definition and replace its parse tree
             *   with our own.  Mark the original object parse tree as
             *   replaced, so that we do not generate any code for it.  
             */
            obj_sym->get_obj_stm()->set_replaced(TRUE);

            /*
             *   Note that we do NOT mark the object as a 'replace' object
             *   in the symbol table.  Because the object was previously
             *   defined within this translation unit, the replacement is
             *   now completed - there is no need to apply any replacement
             *   from this object at link time, and in fact it would be
             *   incorrect to do so.  
             */
        }
        else
        {
            /* 
             *   the symbol is already defined in this module, and we're not
             *   replacing or modifying it -- this is an error, because we
             *   can only define an object once 
             */
            G_tok->log_error(TCERR_OBJ_REDEF, (int)tok_len, tok_txt);

            /* ignore the definition */
            obj_sym = 0;
        }
    }
    else
    {
        /* 
         *   the object is not already defined in the global symbol table --
         *   create a new object symbol 
         */
        obj_sym = new CTcSymObj(tok_txt, tok_len, FALSE,
                                G_cg->new_obj_id(), FALSE,
                                TC_META_TADSOBJ, dict_cur_);

        /* add the object to the global symbol table */
        global_symtab_->add_entry(obj_sym);

        /* 
         *   We can't replace or modify an object that hasn't been defined
         *   yet (it has to have been defined at least as an external).
         *   Do not make this check in syntax-only mode, because it's not
         *   important to the syntax.  
         */
        if (replace || modify)
        {
            /* only log an error in full compile mode */
            if (!syntax_only_)
            {
                /* full compilation - log the error */
                G_tok->log_error(TCERR_REPMODOBJ_UNDEF);
            }
            else
            {
                /*
                 *   We're only parsing - mark this as an external replace
                 *   or external modify on the assumption that the symbol
                 *   will be defined externally in the actual compilation.
                 *   In particular, if we're building a symbol file, we
                 *   don't want to write undefined 'modify' or 'replace'
                 *   symbols, because such symbols are not really defined
                 *   by this module.  
                 */
                if (replace)
                    obj_sym->set_ext_replace(TRUE);
                else
                    obj_sym->set_ext_modify(TRUE);
            }
        }
    }

    /* mark the object as a class if appropriate */
    if (*is_class && obj_sym != 0)
        obj_sym->set_is_class(TRUE);

    /* mark the object as transient */
    if (*trans && obj_sym != 0)
        obj_sym->set_transient();

    /* return the object symbol */
    return obj_sym;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a top-level object definition statement
 */
CTPNStmTop *CTcParser::parse_object(
    int *err, int replace, int modify, int is_class, int plus_cnt, int trans)
{
    /* find or define the symbol */
    CTcSymObj *mod_orig_sym;
    CTcSymMetaclass *meta_sym;
    CTcSymObj *obj_sym = find_or_def_obj(
        G_tok->getcur()->get_text(), G_tok->getcur()->get_text_len(),
        replace, modify, &is_class, &mod_orig_sym, &meta_sym, &trans);

    /* skip the object name */
    G_tok->next();

    /* parse the body */
    return parse_object_body(
        err, obj_sym, is_class, FALSE, FALSE, FALSE,
        modify, mod_orig_sym, plus_cnt, meta_sym, 0, trans);
}

/*
 *   Parse an anonymous object.  This can be either a top-level object
 *   definition with no name, or a nested object defined as the value side of
 *   a property setting within an object definition's property list.
 */
CTPNStmObject *CTcParser::parse_anon_object(
    int *err, int plus_cnt, int is_nested,
    tcprs_term_info *term_info, int trans)
{
    /* create an anonymous object symbol */
    CTcSymObj *obj_sym = new CTcSymObj(
        ".anon", 5, FALSE, G_cg->new_obj_id(), FALSE, TC_META_TADSOBJ, 0);

    /* set the dictionary for the new object */
    obj_sym->set_dict(dict_cur_);

    /* parse the object body */
    return parse_object_body(
        err, obj_sym, FALSE, TRUE, FALSE, is_nested,
        FALSE, 0, plus_cnt, 0, term_info, trans);
}

/* ------------------------------------------------------------------------ */
/*
 *   Object statement 
 */

/*
 *   Parse a property definition using the nested object syntax.  This is for
 *   property definitions of the form "prop: Thing { ... }" that define a new
 *   static object instance and use a reference as the value of the property.
 */
int CTPNStmObjectBase::parse_nested_obj_prop(
    CTPNObjProp* &new_prop, int *err, tcprs_term_info *term_info,
    const CTcToken *prop_tok, int replace)
{
    /* parse the body of the nested object */
    CTPNStmObject *nested_obj = G_prs->parse_anon_object(
        err, 0, TRUE, term_info, FALSE);
    
    /* 
     *   If we encountered a termination error, assume the error is actually
     *   in the enclosing object, and that this wasn't meant to be a nested
     *   object after all.  Do not define the symbol we took as a property to
     *   be a property, since it might well have been meant as an object name
     *   instead.  If a termination error did occur, simply return, so that
     *   the caller can handle the assumed termination.  
     */
    if (term_info->unterm_)
        return FALSE;
    
    /* make sure we didn't encounter an error */
    if (*err != 0 )
        return FALSE;
    
    /* make sure we created a valid parse tree for the object */
    if (nested_obj == 0)
    {
        *err = TRUE;
        return FALSE;
    }

    /* the value we wish to assign is a reference to this object */
    CTcConstVal cval;
    cval.set_obj(nested_obj->get_obj_sym()->get_obj_id(),
                 nested_obj->get_obj_sym()->get_metaclass());
    CTcPrsNode *expr = new CTPNConst(&cval);
    
    /* 
     *   look up the property symbol - we can finally look it up with
     *   reasonable confidence, since it appears we do indeed have a valid
     *   nested object definition, hence we can stop waiting to see if
     *   something goes wrong 
     */
    CTcSymProp *prop_sym = G_prs->look_up_prop(prop_tok, TRUE);
    
    /* if it's a vocabulary property, this type isn't allowed */
    if (prop_sym != 0 && prop_sym->is_vocab())
        G_tok->log_error(TCERR_VOCAB_REQ_SSTR, 1, ":");
    
    /* if we have a valid property and expression, add it */
    if (prop_sym != 0)
        new_prop = add_prop(prop_sym, expr, replace, FALSE);
    
    /*
     *   Define the lexicalParent property in the nested object with a
     *   reference back to the parent object. 
     */
    cval.set_obj(get_obj_sym() != 0
                 ? get_obj_sym()->get_obj_id() : TCTARG_INVALID_OBJ,
                 get_obj_sym() != 0
                 ? get_obj_sym()->get_metaclass() : TC_META_UNKNOWN);
    expr = new CTPNConst(&cval);
    nested_obj->add_prop(G_prs->get_lexical_parent_sym(), expr, FALSE, FALSE);
    
    /* 
     *   add the nested object's parse tree to the queue of nested top-level
     *   statements - since an object is a top-level statement, but we're not
     *   parsing it at the top level, we need to add it to the pending queue
     *   explicitly 
     */
    G_prs->add_nested_stm(nested_obj);

    /* success */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse an object body 
 */
CTPNStmObject *CTcParser::parse_object_body(
    int *err, CTcSymObj *obj_sym,
    int is_class, int is_anon, int is_grammar, int is_nested,
    int modify, CTcSymObj *mod_orig_sym, int plus_cnt,
    CTcSymMetaclass *meta_sym, tcprs_term_info *term_info, int trans)
{
    /* 
     *   If we don't have an enclosing term_info, use my own; if a valid
     *   term_info was passed in, continue using the enclosing one.  We
     *   always want to use the outermost term_info, because our heuristic
     *   is to assume that any lack of termination applies to the first
     *   object it possibly could apply to. 
     */
    tcprs_term_info my_term_info;
    if (term_info == 0)
        term_info = &my_term_info;

    /* presume the object won't use braces around its property list */
    int braces = FALSE;

    /* create the object statement */
    CTPNStmObject *obj_stm = new CTPNStmObject(obj_sym, is_class);
    if (obj_sym != 0)
        obj_sym->set_obj_stm(obj_stm);

    /* set the 'transient' status if appropriate */
    if (trans)
        obj_stm->set_transient();

    /* remember whether 'self' was valid in the enclosing context */
    int old_self_valid = self_valid_;

    /* 'self' is valid within object definitions */
    self_valid_ = TRUE;

    /* if it's anonymous, add it to the anonymous object list */
    if (is_anon && obj_sym != 0)
        add_anon_obj(obj_sym);

    /* 
     *   set the object's line location (we must do this explicitly
     *   because we're in a top-level statement and hence do not have a
     *   current source position saved at the moment) 
     */
    obj_stm->set_source_pos(G_tok->get_last_desc(),
                            G_tok->get_last_linenum());

    /* 
     *   If we're not modifying, parse the class list.  If this is a
     *   'modify' statement, there's no class list.  
     */
    if (!modify)
    {
        /* 
         *   Parse and skip the colon, if required.  All objects except
         *   anonymous objects and grammar rule definitions have the colon;
         *   for anonymous objects and grammar definitions, the caller is
         *   required to advance us to the class list before parsing the
         *   object body.  
         */
        if (is_anon || is_grammar)
        {
            /* 
             *   it's anonymous, or it's a grammar definition - the caller
             *   will have already advanced us to the class list, so there's
             *   no need to look for a colon 
             */
        }
        else if (G_tok->cur() == TOKT_COLON)
        {
            /* skip the colon */
            G_tok->next();
        }
        else
        {
            /* it's an error, but assume they simply left off the colon */
            G_tok->log_error_curtok(TCERR_OBJDEF_REQ_COLON);
        }

        /* 
         *   Drop any existing list of superclass names from the symbol.  We
         *   might have a list of names from loading the symbol file; if so,
         *   forget that, and use the list defined here instead.  
         */
        if (obj_sym != 0)
            obj_sym->clear_sc_names();
        
        /* parse the superclass list */
        parse_superclass_list(obj_sym, obj_stm->get_superclass_list());
    }
    else
    {
        /*
         *   This is a 'modify' statement.  The original pre-modified
         *   object is our single superclass. 
         *   
         *   Note that we do not do anything with the superclass name list,
         *   because we're reassigning the symbol representing the global
         *   name of the object (obj_sym) to refer to the new modifier
         *   object.  As far as the name structure of the program is
         *   concerned, this object's superclass list is the same as the
         *   original object's superclass list.  
         */
        if (mod_orig_sym != 0)
            obj_stm->add_superclass(mod_orig_sym);
    }

    /* 
     *   clear the list of dictionary properties, so we can keep track of
     *   which dictionary properties this object explicitly defines 
     */
    for (CTcDictPropEntry *dict_prop = dict_prop_head_ ; dict_prop != 0 ;
         dict_prop = dict_prop->nxt_)
    {
        /* mark this dictionary property as not defined for this object */
        dict_prop->defined_ = FALSE;
    }

    /*
     *   If we have a '+' list, figure the object's location by finding
     *   the item in the location stack at the desired depth. 
     */
    if (plus_cnt != 0)
    {
        /* 
         *   if no '+' property has been defined, or they're asking for a
         *   nesting level that doesn't exist, note the error and ignore the
         *   '+' setting 
         */
        if (plus_prop_ == 0
            || (size_t)plus_cnt > plus_stack_alloc_
            || plus_stack_[plus_cnt - 1] == 0)
        {
            /* log the error */
            G_tok->log_error(TCERR_PLUSOBJ_TOO_MANY);
        }
        else
        {
            /* 
             *   find the object - the location of an object with N '+'
             *   signs is the object most recently defined with N-1 '+'
             *   signs 
             */
            CTPNStmObject *loc = plus_stack_[plus_cnt - 1];

            /* add the '+' property value, if it's a valid object symbol */
            if (loc->get_obj_sym() != 0)
            {
                /* set up a constant expression for the object reference */
                CTcConstVal cval;
                cval.set_obj(loc->get_obj_sym()->get_obj_id(),
                             loc->get_obj_sym()->get_metaclass());
                CTcPrsNode *expr = new CTPNConst(&cval);
                
                /* add the location property */
                CTPNObjProp *prop = obj_stm->add_prop(
                    plus_prop_, expr, FALSE, FALSE);

                /* 
                 *   mark it as overwritable, since it was added via the '+'
                 *   notation 
                 */
                prop->set_overwritable();
            }
        }
    }

    /*
     *   Remember this object as the most recent object defined with its
     *   number of '+' signs.  Only pay attention to '+' signs for top-level
     *   objects - if this is a nested object, it doesn't participate in the
     *   '+' mechanism at all.  Also, ignore classes defined with no '+'
     *   signs - the '+' mechanism is generally only useful for instances,
     *   so ignore it for classes that don't use the notation explicitly.  
     */
    if (!is_nested && (!is_class || plus_cnt != 0))
    {
        /* first, allocate more space in the '+' stack if necessary */
        if ((size_t)plus_cnt >= plus_stack_alloc_)
        {
            size_t new_alloc;
            size_t i;
            
            /* 
             *   allocate more space - go a bit above what we need to avoid
             *   having to reallocate again immediately if they a few levels
             *   deeper 
             */
            new_alloc = plus_cnt + 16;
            plus_stack_ = (CTPNStmObject **)
                          t3realloc(plus_stack_,
                                    new_alloc * sizeof(*plus_stack_));
            
            /* clear out the newly-allocated entries */
            for (i = plus_stack_alloc_ ; i < new_alloc ; ++i)
                plus_stack_[i] = 0;
            
            /* remember the new allocation size */
            plus_stack_alloc_ = new_alloc;
        }
        
        /* remember this object as the last object at its depth */
        plus_stack_[plus_cnt] = obj_stm;
    }

    /*
     *   Check for an open brace.  If the object uses braces around its
     *   property list, template properties can be either before the open
     *   brace or just inside it. 
     */
    if (G_tok->cur() == TOKT_LBRACE)
    {
        /* skip the open brace */
        G_tok->next();

        /* note that we're using braces */
        braces = TRUE;
    }

    /* check for template syntax */
    parse_obj_template(err, obj_stm, FALSE);
    if (*err)
    {
        self_valid_ = old_self_valid;
        return 0;
    }

    /*
     *   If we didn't already find an open brace, try again now that we've
     *   scanned the template properties, since template properties can
     *   either immediately precede or immediately follow the open brace, if
     *   present. 
     */
    if (!braces && G_tok->cur() == TOKT_LBRACE)
    {
        /* note that we're using braces */
        braces = TRUE;

        /* skip the open brace */
        G_tok->next();
    }

    /*
     *   If this is a nested object, the property list must be enclosed in
     *   braces.  If we haven't found a brace by this point, it's an error.  
     */
    if (is_nested && !braces)
    {
        /* 
         *   No braces - this is illegal, because a nested object requires
         *   braces.  The most likely error is that the previous object was
         *   not properly terminated, and this is simply a new object
         *   definition.  Flag the error as a missing terminator on the
         *   enclosing object.  
         */
        G_tcmain->log_error(term_info->desc_, term_info->linenum_,
                            TC_SEV_ERROR, TCERR_UNTERM_OBJ_DEF);

        /* set the termination error flag for the caller */
        term_info->unterm_ = TRUE;
    }

    /*
     *   If we're not a class, and we're not a 'modify', add the automatic
     *   'sourceTextOrder' property, add the 'sourceTextGroup' if desired.
     *   sourceTextOrder provides a source file sequence number that can be
     *   used to order a set of objects into the same sequence in which they
     *   appeared in the original source file, which is often useful for
     *   assembling pre-initialized structures.  sourceTextGroup identifies
     *   the file itself.  
     */
    if (!is_class && !modify)
    {
        /* set up the constant expression for the source sequence number */
        CTcConstVal cval;
        cval.set_int(src_order_idx_++);
        CTcPrsNode *expr = new CTPNConst(&cval);

        /* create the new sourceTextOrder property */
        CTPNObjProp *prop = obj_stm->add_prop(
            src_order_sym_, expr, FALSE, FALSE);

        /* mark it as overwritable with an explicit property definition */
        prop->set_overwritable();

        /* if we're generating sourceTextGroup properties, add it */
        if (src_group_mode_)
        {
            /* set up the expression for the source group object */
            cval.set_obj(src_group_id_, TC_META_TADSOBJ);
            expr = new CTPNConst(&cval);

            /* create the new sourceTextGroup property */
            prop = obj_stm->add_prop(src_group_sym_, expr, FALSE, FALSE);

            /* mark it as overwritable */
            prop->set_overwritable();
        }
    }

    /* parse the property list */
    if (!parse_obj_prop_list(
        err, obj_stm, meta_sym, modify, is_nested, braces, FALSE,
        &my_term_info, term_info))
    {
        self_valid_ = old_self_valid;
        return 0;
    }

    /* 
     *   Finish the object by adding dictionary property placeholders.  Don't
     *   bother if this is an intrinsic class modifier, since intrinsic class
     *   objects can't have dictionary associations.  
     */
    if (obj_stm != 0 && meta_sym == 0)
    {
        /* 
         *   Add a placeholder for each dictionary property we didn't
         *   explicitly define for the object.  This will ensure that we have
         *   enough property slots pre-allocated at link time if the object
         *   inherits vocabulary from its superclasses.  The linker will
         *   remove any slots that end up being unused, so this won't affect
         *   run-time efficiency or the ultimate size of the image file.
         */
        for (CTcDictPropEntry *dict_prop = dict_prop_head_ ; dict_prop != 0 ;
             dict_prop = dict_prop->nxt_)
        {
            /* if this one isn't defined, add a placeholder */
            if (!dict_prop->defined_)
            {
                /* set up a VOCAB_LIST value */
                CTcConstVal cval;
                cval.set_vocab_list();
                CTcPrsNode *expr = new CTPNConst(&cval);

                /* add it */
                obj_stm->add_prop(dict_prop->prop_, expr, FALSE, FALSE);
            }
        }
    }

    /* restore the enclosing 'self' validity */
    self_valid_ = old_self_valid;

    /* return the object statement node */
    return obj_stm;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a class definition 
 */
CTPNStmTop *CTcParser::parse_class(int *err)
{
    /* skip the 'class' keyword */
    G_tok->next();

    /* parse the object definition */
    return parse_object(err, FALSE, FALSE, TRUE, 0, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a 'modify' statement
 */
CTPNStmTop *CTcParser::parse_modify(int *err)
{
    /* skip the 'modify' keyword */
    G_tok->next();

    /* if 'grammar' follows, parse the grammar definition */
    if (G_tok->cur() == TOKT_GRAMMAR)
        return parse_grammar(err, FALSE, TRUE);

    /* if 'function' follows, parse the function definition */
    if (G_tok->cur() == TOKT_FUNCTION || G_tok->cur() == TOKT_METHOD)
        return parse_function(err, FALSE, FALSE, TRUE, TRUE);

    /* if a symbol and an open paren follow, parse the function definition */
    if (G_tok->cur() == TOKT_SYM)
    {
        /* we have a symbol - check the next token, but put it right back */
        int is_lpar = (G_tok->next() == TOKT_LPAR);
        G_tok->unget();

        /* if a paren follows the symbol, it's a function */
        if (is_lpar)
            return parse_function(err, FALSE, FALSE, TRUE, FALSE);
    }        

    /* parse the object definition */
    return parse_object(err, FALSE, TRUE, FALSE, 0, FALSE);
}

/*
 *   Parse a 'replace' statement 
 */
CTPNStmTop *CTcParser::parse_replace(int *err)
{
    /* skip the 'replace' keyword and see what follows */
    switch (G_tok->next())
    {
    case TOKT_FUNCTION:
    case TOKT_METHOD:
        /* replace the function */
        return parse_function(err, FALSE, TRUE, FALSE, TRUE);

    case TOKT_CLASS:
        /* skip the 'class' keyword */
        G_tok->next();

        /* parse the object definition */
        return parse_object(err, TRUE, FALSE, TRUE, 0, FALSE);

    case TOKT_GRAMMAR:
        /* parse the 'grammar' definition */
        return parse_grammar(err, TRUE, FALSE);

    case TOKT_SYM:
    case TOKT_TRANSIENT:
        /* replace the object or function definition */
        return parse_object_or_func(err, TRUE, 0, 0);

    default:
        /* it's an error */
        G_tok->log_error_curtok(TCERR_REPLACE_REQ_OBJ_OR_FUNC);

        /* skip the invalid token */
        G_tok->next();
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Generic statement node 
 */

/*
 *   Add a debugging line record for the current statement.  Call this just
 *   before generating instructions for the statement; we'll create a line
 *   record associating the current byte-code location with our source file
 *   and line number.  
 */
void CTPNStmBase::add_debug_line_rec()
{
    /* add a line record to the code generator's list */
    if (file_ != 0)
        G_cg->add_line_rec(file_, linenum_);
}

/*
 *   Add a debugging line record for a given position 
 */
void CTPNStmBase::add_debug_line_rec(CTcTokFileDesc *desc, long linenum)
{
    /* add a line record to the code generator's list */
    G_cg->add_line_rec(desc, linenum);
}

/* ------------------------------------------------------------------------ */
/*
 *   Export symbol 
 */

/*
 *   write to object file 
 */
void CTcPrsExport::write_to_obj_file(CVmFile *fp)
{
    /* write our internal symbol name, with length prefix */
    fp->write_uint2((int)get_sym_len());
    fp->write_bytes(get_sym(), get_sym_len());

    /* write our external name, with length prefix */
    fp->write_uint2((int)get_ext_len());
    fp->write_bytes(get_ext_name(), get_ext_len());
}

/*
 *   read from object file 
 */
CTcPrsExport *CTcPrsExport::read_from_obj_file(CVmFile *fp)
{
    char buf[TOK_SYM_MAX_LEN + 1];
    size_t sym_len;
    size_t ext_len;
    const char *sym;
    const char *ext;
    CTcPrsExport *exp;

    /* read the symbol */
    sym = CTcParser::read_len_prefix_str(fp, buf, sizeof(buf), &sym_len,
                                         TCERR_EXPORT_SYM_TOO_LONG);
    if (sym == 0)
        return 0;

    /* read the external name */
    ext = CTcParser::read_len_prefix_str(fp, buf, sizeof(buf), &ext_len,
                                         TCERR_EXPORT_SYM_TOO_LONG);
    if (ext == 0)
        return 0;

    /* create a new export record for the symbol */
    exp = new CTcPrsExport(sym, sym_len);

    /* set the external name */
    exp->set_extern_name(ext, ext_len);

    /* return the new record */
    return exp;
}


/* ------------------------------------------------------------------------ */
/*
 *   Add a generated object - static compilation mode only. 
 */
CTcSymObj *CTcParser::add_gen_obj_stat(CTcSymObj *cls)
{
    /* allocate an object ID */
    tctarg_obj_id_t id = G_cg->new_obj_id();

    /* set up an anonymous symbol for it */
    CTcSymObj *sym = new CTcSymObj(
        ".anon", 5, FALSE, id, FALSE, TC_META_TADSOBJ, 0);

    /* create its object statement, and plug it into the symbol */
    CTPNStmObject *stm = new CTPNStmObject(sym, FALSE);
    sym->set_obj_stm(stm);

    /* add the superclass, if we have one */
    if (cls != 0)
        stm->add_superclass(cls);

    /* add the object to the top-level statement list */
    add_anon_obj(sym);
    add_nested_stm(stm);

    /* return the object symbol */
    return sym;
}

/*
 *   Add a property to a generated object - static compilation mode only. 
 */
void CTcParser::add_gen_obj_prop_stat(
    CTcSymObj *obj, CTcSymProp *prop, const CTcConstVal *val)
{
    /* add the property to the object statement */
    obj->get_obj_stm()->add_prop(prop, new CTPNConst(val), FALSE, FALSE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Program Statement node 
 */

/*
 *   fold constants - we simply fold constants in each of our statements 
 */
CTcPrsNode *CTPNStmProg::fold_constants(class CTcPrsSymtab *symtab)
{
    CTPNStmTop *cur;

    /* iterate through our statements and fold constants in each one */
    for (cur = first_stm_ ; cur != 0 ; cur = cur->get_next_stm_top())
        cur->fold_constants(symtab);

    /* 
     *   although nodes within our subtree might have been changed, this
     *   compound statement itself is unchanged by constant folding 
     */
    return this;
}

/*
 *   Generate code 
 */
void CTPNStmProg::gen_code(int, int)
{
    CTPNStmTop *cur;

    /* 
     *   iterate through our statements and generate code for each in
     *   sequence 
     */
    for (cur = first_stm_ ; cur != 0 ; cur = cur->get_next_stm_top())
        cur->gen_code(TRUE, TRUE);

    /* 
     *   iterate again, this time checking local symbol tables - we must wait
     *   to do this until after we've generated all of the code, because
     *   local symbol tables can be cross-referenced in some cases (in
     *   particular, when locals are shared through contexts, such as with
     *   anonymous functions) 
     */
    for (cur = first_stm_ ; cur != 0 ; cur = cur->get_next_stm_top())
        cur->check_locals();
}

/* ------------------------------------------------------------------------ */
/*
 *   Dictionary entry - each dictionary object gets one of these objects to
 *   track it 
 */

/*
 *   construction 
 */
   CTcDictEntry::CTcDictEntry(CTcSymObj *sym)
{
    const size_t hash_table_size = 128;

    /* remember my object symbol and word truncation length */
    sym_ = sym;

    /* no object file index yet */
    obj_idx_ = 0;

    /* not in a list yet */
    nxt_ = 0;

    /* create my hash table */
    hashtab_ = new (G_prsmem)
               CVmHashTable(hash_table_size,
                            new (G_prsmem) CVmHashFuncCI(), TRUE,
                            new (G_prsmem) CVmHashEntry *[hash_table_size]);
}

/*
 *   Add a word to the table 
 */
void CTcDictEntry::add_word(const char *txt, size_t len, int copy,
                            tc_obj_id obj, tc_prop_id prop)
{
    CVmHashEntryPrsDict *entry;

    /* search for an existing entry */
    entry = (CVmHashEntryPrsDict *)hashtab_->find(txt, len);

    /* if there's no entry, create a new one */
    if (entry == 0)
    {
        /* create a new item */
        entry = new (G_prsmem) CVmHashEntryPrsDict(txt, len, copy);

        /* add it to the table */
        hashtab_->add(entry);
    }

    /* add this object/property association to the word's hash table entry */
    entry->add_item(obj, prop);
}

/*
 *   Write my symbol to an object file 
 */
void CTcDictEntry::write_sym_to_obj_file(CVmFile *fp)
{
    /* if I already have a non-zero index value, I've already been written */
    if (obj_idx_ != 0)
        return;

    /* assign myself an object file dictionary index */
    obj_idx_ = G_prs->get_next_obj_file_dict_idx();

    /* write my symbol to the object file */
    sym_->write_to_obj_file(fp);
}

/* ------------------------------------------------------------------------ */
/*
 *   Grammar production entry
 */

/*
 *   Write to an object file 
 */
void CTcGramProdEntry::write_to_obj_file(CVmFile *fp)
{
    ulong cnt;
    CTcGramProdAlt *alt;
    ulong flags;

    /* write the object file index of my production object symbol */
    fp->write_uint4(prod_sym_ == 0 ? 0 : prod_sym_->get_obj_file_idx());

    /* set up the flags */
    flags = 0;
    if (is_declared_)
        flags |= 1;

    /* write the flags */
    fp->write_uint4(flags);

    /* count my alternatives */
    for (cnt = 0, alt = alt_head_ ; alt != 0 ;
         ++cnt, alt = alt->get_next()) ;

    /* write the count */
    fp->write_uint4(cnt);

    /* write each alternative */
    for (alt = alt_head_ ; alt != 0 ; alt = alt->get_next())
        alt->write_to_obj_file(fp);
}


/* ------------------------------------------------------------------------ */
/*
 *   Grammar production rule alternative object
 */

/*
 *   Write to an object file 
 */
void CTcGramProdAlt::write_to_obj_file(CVmFile *fp)
{
    ulong cnt;
    CTcGramProdTok *tok;

    /* write my score and badness */
    fp->write_int2(score_);
    fp->write_int2(badness_);

    /* write the index of my processor object symbol */
    fp->write_uint4(obj_sym_ == 0 ? 0 : obj_sym_->get_obj_file_idx());

    /* write the dictionary index */
    fp->write_uint4(dict_ == 0 ? 0 : dict_->get_obj_idx());

    /* count my tokens */
    for (cnt = 0, tok = tok_head_ ; tok != 0 ;
         ++cnt, tok = tok->get_next()) ;

    /* write my token count */
    fp->write_uint4(cnt);

    /* write the tokens */
    for (tok = tok_head_ ; tok != 0 ; tok = tok->get_next())
        tok->write_to_obj_file(fp);
}

/* ------------------------------------------------------------------------ */
/*
 *   Grammar production token object 
 */

/*
 *   write to an object file 
 */
void CTcGramProdTok::write_to_obj_file(CVmFile *fp)
{
    size_t i;

    /* write my type */
    fp->write_uint2((int)typ_);

    /* write my data */
    switch(typ_)
    {
    case TCGRAM_PROD:
        /* write my object's object file index */
        fp->write_uint4(val_.obj_ != 0
                        ? val_.obj_->get_obj_file_idx() : 0);
        break;

    case TCGRAM_TOKEN_TYPE:
        /* write my enum token ID */
        fp->write_uint4(val_.enum_id_);
        break;

    case TCGRAM_PART_OF_SPEECH:
        /* write my property ID */
        fp->write_uint2(val_.prop_);
        break;

    case TCGRAM_LITERAL:
        /* write my string */
        fp->write_uint2(val_.str_.len_);
        fp->write_bytes(val_.str_.txt_, val_.str_.len_);
        break;

    case TCGRAM_STAR:
        /* no additional value data */
        break;

    case TCGRAM_PART_OF_SPEECH_LIST:
        /* write the length */
        fp->write_uint2(val_.prop_list_.len_);

        /* write each element */
        for (i = 0 ; i < val_.prop_list_.len_ ; ++i)
            fp->write_uint2(val_.prop_list_.arr_[i]);

        /* done */
        break;

    case TCGRAM_UNKNOWN:
        /* no value - there's nothing extra to write */
        break;
    }

    /* write my property association */
    fp->write_uint2(prop_assoc_);
}

