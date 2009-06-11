#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCPRSSTM.CPP,v 1.1 1999/07/11 00:46:54 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcprs.cpp - TADS 3 Compiler Parser - statement node classes
Function
  This parser module includes statement node classes that are needed
  only for a full compiler.  This module should not be needed for
  tools that need only to compile expressions, such as debuggers.
Notes
  
Modified
  04/30/99 MJRoberts  - Creation
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
static const char symbol_export_sig[] = "TADS3.SymbolExport.0006\n\r\032";

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
    fp->write_int4(0);

    /* if there's a make object, tell it to write out its config data */
    if (make_obj != 0)
    {
        long end_ofs;

        /* write out the data */
        make_obj->write_build_config_to_sym_file(fp);

        /* go back and fix up the size */
        end_ofs = fp->get_pos();
        fp->set_pos(size_ofs);
        fp->write_int4(end_ofs - size_ofs - 4);

        /* seek back to the end for continued writing */
        fp->set_pos(end_ofs);
    }

    /* 
     *   write the number of intrinsic function sets, followed by the
     *   function set names 
     */
    fp->write_int4(cnt = G_cg->get_fnset_cnt());
    for (i = 0 ; i < cnt ; ++i)
    {
        const char *nm;
        size_t len;

        /* get the name */
        nm = G_cg->get_fnset_name(i);

        /* write the length followed by the name */
        fp->write_int2(len = strlen(nm));
        fp->write_bytes(nm, len);
    }

    /* 
     *   write the number of intrinsic classes, followed by the intrinsic
     *   class names 
     */
    fp->write_int4(cnt = G_cg->get_meta_cnt());
    for (i = 0 ; i < cnt ; ++i)
    {
        const char *nm;
        size_t len;

        /* get the name */
        nm = G_cg->get_meta_name(i);

        /* write the length followed by the name */
        fp->write_int2(len = strlen(nm));
        fp->write_bytes(nm, len);
    }

    /* 
     *   write a placeholder for the symbol count, first remembering where
     *   we put it so that we can come back and fix it up after we know
     *   how many symbols there actually are 
     */
    cnt_ofs = fp->get_pos();
    fp->write_int4(0);

    /* write each entry from the global symbol table */
    ctx.cnt = 0;
    ctx.fp = fp;
    ctx.prs = this;
    global_symtab_->enum_entries(&write_sym_cb, &ctx);

    /* go back and fix up the counter */
    end_ofs = fp->get_pos();
    fp->set_pos(cnt_ofs);
    fp->write_int4(ctx.cnt);

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
    err_catch(exc)
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
        CTcSymbol *sym;
        CTcSymbol *old_sym;
        
        /* read a symbol */
        sym = CTcSymbol::read_from_sym_file(fp);

        /* if that failed, abort */
        if (sym == 0)
            return 2;

        /* check for an existing definition */
        old_sym = global_symtab_->find(sym->get_sym(), sym->get_sym_len());

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
    fp->write_int4(0);
    fp->write_int4(0);
    fp->write_int4(0);

    /* write each entry from the global symbol table */
    ctx.cnt = 0;
    ctx.fp = fp;
    ctx.prs = this;
    global_symtab_->enum_entries(&write_obj_cb, &ctx);

    /* count the anonymous objects */
    for (anon_cnt = 0, anon_obj = anon_obj_head_ ; anon_obj != 0 ;
         ++anon_cnt, anon_obj = (CTcSymObj *)anon_obj->nxt_) ;

    /* write the number of anonymous objects */
    fp->write_int4(anon_cnt);

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
    fp->write_int4(nonsym_cnt);
    for (nonsym = nonsym_obj_head_ ; nonsym != 0 ; nonsym = nonsym->nxt_)
        fp->write_int4((ulong)nonsym->id_);

    /* remember where we are, and seek back to the counters for fixup */
    end_ofs = fp->get_pos();
    fp->set_pos(cnt_ofs);

    /* write the actual symbol index count */
    fp->write_int4(obj_file_sym_idx_);

    /* write the actual dictionary index count */
    fp->write_int4(obj_file_dict_idx_);

    /* write the total named symbol count */
    fp->write_int4(ctx.cnt);

    /* 
     *   seek back to the ending offset so we can continue on to write the
     *   remainder of the file 
     */
    fp->set_pos(end_ofs);

    /* write a placeholder for the symbol cross-reference entries */
    cnt_ofs = fp->get_pos();
    fp->write_int4(0);

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
    fp->write_int4(ctx.cnt);
    fp->set_pos(end_ofs);

    /* write the anonymous object cross-references */
    fp->write_int4(anon_cnt);
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
    fp->write_int4(prod_cnt);

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
    fp->write_int4(0);

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
    fp->write_int4(ctx.cnt);
    fp->set_pos(end_ofs);

    /* count the exports */
    for (exp_cnt = 0, exp = exp_head_ ; exp != 0 ;
         ++exp_cnt, exp = exp->get_next());

    /* write the number of exports */
    fp->write_int4(exp_cnt);

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
    ctx->fp->write_int4(obj_sym->get_obj_file_idx());

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
    int err;
    int done;
    CTPNStmTop *first_stm;
    CTPNStmTop *last_stm;
    int suppress_error;

    /* nothing in our list yet */
    first_stm = last_stm = 0;

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
    suppress_error = FALSE;
    
    /* no end-of-file error yet */
    err = FALSE;

    /* keep going until we reach the end of the file */
    for (done = FALSE ; !done && !err ; )
    {
        CTPNStmTop *cur_stm;
        int suppress_next_error;

        /* we don't have a statement yet */
        cur_stm = 0;

        /* 
         *   presume we'll find a valid statement and hence won't need to
         *   suppress subsequent errors 
         */
        suppress_next_error = FALSE;
        
        /* see what we have */
        switch(G_tok->cur())
        {
        case TOKT_EOF:
            /* we're done */
            done = TRUE;
            break;
            
        case TOKT_FUNCTION:
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
 *   Grammar tree node - base class 
 */
class CTcPrsGramNode: public CTcTokenSource
{
public:
    CTcPrsGramNode()
    {
        /* we have no siblings yet */
        next_ = 0;
    }
    
    /* get the next token - does nothing by default */
    virtual const CTcToken *get_next_token() { return 0; }

    /* consolidate OR nodes at the top of the subtree */
    virtual CTcPrsGramNode *consolidate_or() = 0;

    /* flatten CAT nodes together in the tree */
    virtual void flatten_cat() { /* by default, do nothing */ }

    /* am I an "or" node? */
    virtual int is_or() { return FALSE; }

    /* am I a "cat" node? */
    virtual int is_cat() { return FALSE; }

    /* get my token - if I'm not a token node, returns null */
    virtual const CTcToken *get_tok() const { return 0; }

    /* initialize expansion - by default, we do nothing */
    virtual void init_expansion() { }

    /* 
     *   Advance to the next expansion state.  Returns true if we 'carry'
     *   out of the current item, which means that we were already at our
     *   last state and hence are wrapping back to our first state.  Returns
     *   false if we advanced to a new state without wrapping back.
     *   
     *   By default, since normal items have only one alternative, we don't
     *   do anything but return a 'carry, since each advance takes us back
     *   to our single and initial state. 
     */
    virtual int advance_expansion() { return TRUE; }

    /* clone the current expansion subtree */
    virtual CTcPrsGramNode *clone_expansion() const = 0;

    /* next sibling node */
    CTcPrsGramNode *next_;
};


class CTcPrsGramNodeWithChildren: public CTcPrsGramNode
{
public:
    CTcPrsGramNodeWithChildren()
    {
        /* no subitems yet */
        sub_head_ = sub_tail_ = 0;
    }

    /* add a sub-item */
    void add_sub_item(CTcPrsGramNode *sub)
    {
        /* add it to the end of the list */
        if (sub_tail_ != 0)
            sub_tail_->next_ = sub;
        else
            sub_head_ = sub;

        /* it's now the last item */
        sub_tail_ = sub;
        sub->next_ = 0;
    }

    virtual void flatten_cat()
    {
        CTcPrsGramNode *cur;
        
        /* flatten CAT nodes in our subnodes */
        for (cur = sub_head_ ; cur != 0 ; cur = cur->next_)
            cur->flatten_cat();
    }
    
    /* initialize expansion */
    virtual void init_expansion()
    {
        CTcPrsGramNode *cur;

        /* initialize subnodes */
        for (cur = sub_head_ ; cur != 0 ; cur = cur->next_)
            cur->init_expansion();
    }

    /* head and tail of list of subnodes */
    CTcPrsGramNode *sub_head_;
    CTcPrsGramNode *sub_tail_;
};

/*
 *   Grammar tree node - CAT node.  This represents a string of
 *   concatenated tokens. 
 */
class CTcPrsGramNodeCat: public CTcPrsGramNodeWithChildren
{
public:
    /* I'm a CAT node */
    virtual int is_cat() { return TRUE; }

    /* 
     *   Consolidate all of the OR's at the top of the tree.  If we have any
     *   OR subnodes, we'll rewrite ourselves as an OR of the concatenations
     *   of all of the sub-OR's in all combinations.  
     */
    virtual CTcPrsGramNode *consolidate_or();

    /* 
     *   Flatten CAT nodes together.  If we have any CAT nodes below us,
     *   move their contents into our own list directly. 
     */
    virtual void flatten_cat()
    {
        CTcPrsGramNode *prv;
        CTcPrsGramNode *cur;
        CTcPrsGramNode *nxt;
        
        /*
         *   Check each child.  For each child that's a CAT node, move its
         *   contents directly into our list, removing the redundant
         *   intervening CAT node. 
         */
        for (prv = 0, cur = sub_head_ ; cur != 0 ; cur = nxt)
        {
            /* note the next item, in case we fiddle with the list */
            nxt = cur->next_;

            /* flatten the CAT items below this one */
            cur->flatten_cat();

            /* if this item is a CAT, move its contents directly into us */
            if (cur->is_cat())
            {
                CTcPrsGramNodeCat *cur_cat;
                
                /* cast it to a CAT node */
                cur_cat = (CTcPrsGramNodeCat *)cur;

                /* if it's empty, just remove it; otherwise, link it in */
                if (cur_cat->sub_head_ != 0)
                {
                    /* link from the previous item to the sublist head */
                    if (prv == 0)
                    {
                        /* 
                         *   this is the first item - put it at the start of
                         *   our list 
                         */
                        sub_head_ = cur_cat->sub_head_;
                    }
                    else
                    {
                        /* link it after the previous item */
                        prv->next_ = cur_cat->sub_head_;
                    }
                    
                    /* link from the tail of the sublist to the next item */
                    cur_cat->sub_tail_->next_ = nxt;
                    
                    /* if it's the last item, set our tail pointer */
                    if (sub_tail_ == cur)
                        sub_tail_ = cur_cat->sub_tail_;
                    
                    /* 
                     *   the tail of the sublist is now the previous item in
                     *   the list for the purposes of the next iteration 
                     */
                    prv = cur_cat->sub_tail_;
                }
                else
                {
                    /* unlink it from our list */
                    if (prv != 0)
                        prv->next_ = nxt;
                    else
                        sub_head_ = nxt;

                    /* update the tail pointer if removing the last item */
                    if (sub_tail_ == cur_cat)
                        sub_tail_ = prv;

                    /* note that the previous item is unchanged */
                }
            }
            else
            {
                /* 
                 *   it's not a CAT, so we're not changing it; this is the
                 *   previous item for the next iteration 
                 */
                prv = cur;
            }
        }
    }

    /* clone the current expansion subtree */
    virtual CTcPrsGramNode *clone_expansion() const
    {
        CTcPrsGramNodeCat *new_cat;
        CTcPrsGramNode *cur;

        /* create a new 'cat' node */
        new_cat = new (G_prsmem) CTcPrsGramNodeCat();

        /* add a clone of each of my children and add it to my replica */
        for (cur = sub_head_ ; cur != 0 ; cur = cur->next_)
            new_cat->add_sub_item(cur->clone_expansion());

        /* return my replica */
        return new_cat;
    }

    /* clone this node (without any children) */
    virtual CTcPrsGramNodeWithChildren *clone_this_node()
    {
        return new CTcPrsGramNodeCat();
    }

    /* initialize expansion */
    virtual void init_expansion()
    {
        /* set to expand from our first child */
        cur_sub_ = sub_head_;

        /* inherit default to initialize subnodes */
        CTcPrsGramNodeWithChildren::init_expansion();
    }

    /* get the next token in a token expansion */
    const CTcToken *get_next_token()
    {
        /* advance to the next subitem */
        cur_sub_ = cur_sub_->next_;
        
        /* 
         *   If there's anything left, return it; otherwise, return null.
         *   During final token expansion, a 'cat' node never has anything
         *   but tokens beneath it, since we move all the 'or' nodes to a
         *   single 'or' at the top of the tree and flatten out the 'cat'
         *   nodes to a single level under that; so, we can simply return
         *   the token directly from the next underlying node.  
         */
        return (cur_sub_ != 0 ? cur_sub_->get_tok() : 0);
    }

    /* 
     *   get my current token - we'll just return the token from the current
     *   subitem 
     */
    const CTcToken *get_tok() const
    {
        return (cur_sub_ != 0 ? cur_sub_->get_tok() : 0);
    }

    /* current subitem in expansion */
    CTcPrsGramNode *cur_sub_;
};

/*
 *   Grammar tree node - OR node.  This represents a set of alternatives. 
 */
class CTcPrsGramNodeOr: public CTcPrsGramNodeWithChildren
{
public:
    /* I'm an "or" node */
    virtual int is_or() { return TRUE; }

    /* 
     *   Consolidate all of the OR's at the top of the tree.
     */
    virtual CTcPrsGramNode *consolidate_or()
    {
        CTcPrsGramNode *cur;
        CTcPrsGramNode *nxt;
        CTcPrsGramNode *old_head;

        /*
         *   Consolidate OR's in all subtrees.  If any of our immediate
         *   children turn into OR's, simply pull their children into our OR
         *   list directly - OR(a, OR(b, c)) is equivalent to OR(a, b, c).
         *   
         *   Before we build the new list, stash away our entire subtree,
         *   since we'll rebuild the tree from the updated versions.  
         */
        old_head = sub_head_;
        sub_head_ = sub_tail_ = 0;

        /* run through our list and build the new, consolidated list */
        for (cur = old_head ; cur != 0 ; cur = nxt)
        {
            CTcPrsGramNode *new_sub;

            /* remember the next item in the old list */
            nxt = cur->next_;
            
            /* consolidate OR's in the subtree */
            new_sub = cur->consolidate_or();

            /* 
             *   if this is an OR node, add its children directly to us;
             *   otherwise, add the node itself
             */
            if (new_sub->is_or())
            {
                CTcPrsGramNodeOr *new_or;
                CTcPrsGramNode *chi;
                CTcPrsGramNode *next_chi;

                /* cast it */
                new_or = (CTcPrsGramNodeOr *)new_sub;

                /* add each of the sub-OR's children as our direct children */
                for (chi = new_or->sub_head_ ; chi != 0 ; chi = next_chi)
                {
                    /* remember the next child in the old sub-list */
                    next_chi = chi->next_;

                    /* move it directly to our list */
                    add_sub_item(chi);
                }

                /* the old OR item is now empty of children */
                new_or->sub_head_ = new_or->sub_tail_ = 0;
            }
            else
            {
                /* it's not an OR - add it directly to our list */
                add_sub_item(new_sub);
            }
        }

        /* return myself with no further changes */
        return this;
    }

    /* initialize expansion - set up at the first alternative */
    virtual void init_expansion()
    {
        /* start at the first alternative */
        cur_alt_ = sub_head_;

        /* initialize the first alternative for expansion */
        cur_alt_->init_expansion();

        /* we didn't just do an 'or' */
        just_did_or_ = FALSE;

        /* we haven't yet returned the first token */
        before_first_ = TRUE;
    }

    /* advance to the next alternative for expansion */
    virtual int advance_expansion()
    {
        /* advance to the next state */
        cur_alt_ = cur_alt_->next_;

        /* 
         *   if that was the last state, wrap back to the first state and
         *   indicate a 'carry'; otherwise, indicate no carry 
         */
        if (cur_alt_ == 0)
        {
            /* we ran out of states - wrap back to the first one */
            cur_alt_ = sub_head_;

            /* initialize expansion in the new subitem */
            cur_alt_->init_expansion();

            /* indicate that we've wrapped and should carry forward */
            return TRUE;
        }
        else
        {
            /* initialize expansion in the new subitem */
            cur_alt_->init_expansion();

            /* this is another valid state - no carry */
            return FALSE;
        }
    }
    
    /* clone the current expansion subtree */
    virtual CTcPrsGramNode *clone_expansion() const
    {
        /* return a replica of the current alternative being expanded */
        return cur_alt_->clone_expansion();
    }

    /* get the next token in a token expansion */
    const CTcToken *get_next_token()
    {
        const CTcToken *tok;

        /* 
         *   If we returned the synthesized '|' token between alternatives
         *   last time, advance to the next alternative and return its first
         *   token.  
         */
        if (just_did_or_)
        {
            /* we no longer just did an '|' */
            just_did_or_ = FALSE;
            
            /* advance to the next alternative */
            cur_alt_ = cur_alt_->next_;

            /* if there is no next alternative, there's nothing to return */
            if (cur_alt_ == 0)
                return 0;

            /* initialize the new alternative */
            cur_alt_->init_expansion();

            /* if this alternative has no subitems, return another '|' */
            if (cur_alt_->get_tok() == 0)
            {
                /* flag the '|' */
                just_did_or_ = TRUE;

                /* if this was the last alternative, we're done */
                if (cur_alt_->next_ == 0)
                    return 0;

                /* 
                 *   return the '|' again (we know it's already set up,
                 *   because we only get here if we did an '|' previously) 
                 */
                return &or_tok_;
            }

            /* return its current token */
            return cur_alt_->get_tok();
        }

        /*
         *   Get the first or next token in the current alternative,
         *   depending on where we are. 
         */
        if (before_first_)
        {
            /* we're before the first token - get the current token */
            tok = cur_alt_->get_tok();

            /* we're now past the first token */
            before_first_ = FALSE;
        }
        else
        {
            /* we're past the first token, so get the next one */
            tok = cur_alt_->get_next_token();
        }

        /* if we got a token from the current alternative, return it */
        if (tok != 0)
            return tok;

        /* 
         *   We've run out of tokens in this alternative - synthesize an OR
         *   token ('|') and return it, so the caller will know a new
         *   top-level alternative follows.
         *   
         *   Do NOT synthesize an OR token after our last alternative.  
         */
        if (cur_alt_->next_ == 0)
        {
            /* this was our last alternative - we're done */
            return 0;
        }

        /* we have another alternative - synthesize an OR */
        or_tok_.settyp(TOKT_OR);
        or_tok_.set_text("|", 1);

        /* flag that we just did an '|' */
        just_did_or_ = TRUE;

        /* return the synthesized '|' token */
        return &or_tok_;
    }

    /* current alternative being expanded */
    CTcPrsGramNode *cur_alt_;

    /* 
     *   my synthesized token for returning the '|' token between
     *   alternatives during expansion 
     */
    CTcToken or_tok_;

    /* flag: we just returned the 'or' between two alternatives */
    unsigned int just_did_or_ : 1;

    /* flag: we haven't yet returned the first token */
    unsigned int before_first_ : 1;
};

/* 
 *   Consolidate all of the OR's at the top of the tree.  If we have any OR
 *   subnodes, we'll rewrite ourselves as an OR of the concatenations of all
 *   of the sub-OR's in all combinations.  
 */
CTcPrsGramNode *CTcPrsGramNodeCat::consolidate_or()
{
    CTcPrsGramNode *old_head;
    CTcPrsGramNodeOr *new_or;
    CTcPrsGramNode *cur;
    CTcPrsGramNode *nxt;
    int or_cnt;

    /* 
     *   Consolidate OR's in all subtrees.  Before we do, stash away our
     *   entire subtree, since we'll rebuild the tree from the updated
     *   versions.  
     */
    old_head = sub_head_;
    sub_head_ = sub_tail_ = 0;

    /* run through our old list and build the new, consolidated list */
    for (cur = old_head ; cur != 0 ; cur = nxt)
    {
        /* remember the next item in the old list */
        nxt = cur->next_;

        /* consolidate the subtree and add it to our new list */
        add_sub_item(cur->consolidate_or());
    }

    /* 
     *   Count of OR nodes - if we don't have any, we don't have to make any
     *   changes.  Since we've already consolidated all OR's out of
     *   sub-nodes into a single node at the top of each subtree, we don't
     *   have to worry about OR's below our immediate children.  
     */
    for (or_cnt = 0, cur = sub_head_ ; cur != 0 ; cur = cur->next_)
    {
        /* if it's an OR node, count it */
        if (cur->is_or())
            ++or_cnt;
    }

    /* if we have no OR nodes, we need no changes */
    if (or_cnt == 0)
        return this;

    /* create a new OR node to contain the list of expansions */
    new_or = new (G_prsmem) CTcPrsGramNodeOr();

    /* set up each OR node with the next alternative */
    for (cur = sub_head_ ; cur != 0 ; cur = cur->next_)
        cur->init_expansion();

    /*
     *   Iterate through all of the possibilities.  For each iteration,
     *   we'll build the currently selected alternative, then we'll advance
     *   by one alternative.  We'll keep doing this until we've advanced
     *   through all of the alternatives.  
     */
    for (;;)
    {
        CTcPrsGramNodeCat *new_cat;

        /* create a new CAT node for the current selected alternative */
        new_cat = new (G_prsmem) CTcPrsGramNodeCat();

        /* clone and add each current alternative to the new CAT node */
        for (cur = sub_head_ ; cur != 0 ; cur = cur->next_)
            new_cat->add_sub_item(cur->clone_expansion());

        /* 
         *   we've finished this entire expansion, so add it to the new main
         *   OR we're building 
         */
        new_or->add_sub_item(new_cat);

        /*
         *   Go through the list of subitems and advance to the next OR
         *   state.  Stop when we reach the first item that advances without
         *   a 'carry'.  If we're still carrying when we reach the last
         *   item, we know we've wrapped around back to the first
         *   alternative and hence are done.  
         */
        for (cur = sub_head_ ; cur != 0 ; cur = cur->next_)
        {
            /* advance this one; if it doesn't carry, we're done */
            if (!cur->advance_expansion())
                break;
        }

        /* 
         *   if we carried past the last item, we've wrapped around back to
         *   the first alternative, so we're done 
         */
        if (cur == 0)
            break;
    }
    
    /* return the new main OR we built */
    return new_or;
}

/*
 *   Grammar tree node - token node.  This is a terminal node representing a
 *   single token.  
 */
class CTcPrsGramNodeTok: public CTcPrsGramNode
{
public:
    CTcPrsGramNodeTok(const CTcToken *tok)
    {
        /* remember the token */
        G_tok->copytok(&tok_, tok);
    }

    /* consolidate OR's */
    virtual CTcPrsGramNode *consolidate_or()
    {
        /* since I'm a terminal node, there's nothing to do here */
        return this;
    }

    /* clone the current expansion subtree */
    virtual CTcPrsGramNode *clone_expansion() const
    {
        /* return a new token node with my same token value */
        return new (G_prsmem) CTcPrsGramNodeTok(&tok_);
    }

    /* get my token */
    virtual const CTcToken *get_tok() const { return &tok_; }

    /* my token */
    CTcToken tok_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Parse a 'grammar' top-level statement
 */
CTPNStmTop *CTcParser::parse_grammar(int *err, int replace, int modify)
{
    CTcSymObj *gram_obj;
    CTcToken prod_name;
    CTcGramProdEntry *prod;
    int prod_name_valid;
    int done;
    CTPNStmObject *stm;
    CTcGramProdAlt *alt;
    CTcGramProdTok *tok;
    const char *name_tag;
    size_t name_tag_len;
    const size_t prop_arrow_max = 100;
    CTcSymProp *prop_arrows[prop_arrow_max];
    size_t prop_arrow_cnt;
    CTcPrsGramNode *tree;
    CTcSymObj *mod_orig_sym;
    int is_anon;
    int need_private_prod;

    /* presume we're not modifying anything */
    mod_orig_sym = 0;

    /* presume we won't need a private production object */
    need_private_prod = FALSE;

    /* no property arrow assignments yet */
    prop_arrow_cnt = 0;

    /* presume we won't find a valid production name */
    prod_name_valid = FALSE;
    prod = 0;

    /* presume it will be anonymous (i.e., no name tag) */
    is_anon = TRUE;

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
        prod_name_valid = TRUE;

        /* find or create the 'grammar production' entry */
        prod = declare_gramprod(prod_name.get_text(),
                                prod_name.get_text_len());
    }

    /* use the production name as the name tag */
    name_tag = prod_name.get_text();
    name_tag_len = prod_name.get_text_len();

    /* check for the optional name-tag token */
    if (G_tok->next() == TOKT_LPAR)
    {
        char tag_buf[TOK_SYM_MAX_LEN*2 + 32];
        size_t copy_len;
        int is_class;
        int trans;
        
        /* skip the paren - the name token follows */
        G_tok->next();

        /* 
         *   limit the added copying so we don't overflow our buffer - note
         *   that we need two bytes for parens 
         */
        copy_len = G_tok->getcur()->get_text_len();
        if (copy_len > sizeof(tag_buf) - name_tag_len - 2)
            copy_len = sizeof(tag_buf) - name_tag_len - 2;

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
        is_class = TRUE;
        trans = FALSE;
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
    }
    else
    {
        /* 
         *   We don't have an empty 'modify' rule list, so we are defining a
         *   rule list for this production.  If we decided that we need to
         *   store the list in a private list, create the private list.  
         */
        if (need_private_prod && gram_obj != 0)
            prod = gram_obj->create_grammar_entry(
                prod_name.get_text(), prod_name.get_text_len());

        /* set up the first alternative and add it to the production */
        alt = new (G_prsmem) CTcGramProdAlt(gram_obj, dict_cur_);
        if (prod != 0)
            prod->add_alt(alt);

        /* flatten the grammar rules */
        tree = flatten_gram_rule(err);
        if (tree == 0 || *err != 0)
            return 0;

        /* 
         *   install the tree as the nested token source, so that we read
         *   tokens from our expanded set of grammar rules rather than from
         *   the original input stream 
         */
        G_tok->set_external_source(tree);
    }

    /* parse the token specification list */
    for (done = FALSE ; !done ; )
    {
        /* see what we have */
        switch(G_tok->cur())
        {
        case TOKT_LBRACK:
            {
                int skip_to_close;

                /* presume we won't need to skip anything */
                skip_to_close = FALSE;
                
                /* make sure we're at the start of the alternative */
                if (alt->get_tok_head() != 0)
                    G_tok->log_warning(TCERR_GRAMMAR_QUAL_NOT_FIRST);

                /* skip the open bracket and make sure a symbol follows */
                if (G_tok->next() == TOKT_SYM)
                {
                    /* check what qualifier we have */
                    if (G_tok->cur_tok_matches("badness", 7))
                    {
                        int val;

                        /* parse the integer-valued qualifier */
                        val = parse_gram_qual_int(err, "badness", &done);
                        if (*err != 0)
                            return 0;

                        /* set the badness for the alternative */
                        alt->set_badness(val);
                    }
                    else
                    {
                        /* not a recognized qualifier */
                        G_tok->log_error_curtok(TCERR_BAD_GRAMMAR_QUAL);

                        /* skip remaining tokens to the ']' */
                        skip_to_close = TRUE;
                    }

                    /* check for proper close if we're is okay so far */
                    if (!skip_to_close)
                    {
                        /* make sure we're at the close bracket */
                        if (G_tok->cur() != TOKT_RBRACK)
                        {
                            /* log an error */
                            G_tok->log_error_curtok(
                                TCERR_GRAMMAR_QUAL_REQ_RBRACK);
                            
                            /* skip to the bracket */
                            skip_to_close = TRUE;
                        }
                        else
                        {
                            /* skip the bracket */
                            G_tok->next();
                        }
                    }
                }
                else
                {
                    /* log the error */
                    G_tok->log_error_curtok(TCERR_GRAMMAR_QUAL_REQ_SYM);
                    
                    /* skip to the end */
                    skip_to_close = TRUE;
                }

                /* 
                 *   If we encountered an error, skip ahead until we find
                 *   a ']' or something that looks like the end of the
                 *   statement.
                 */
                if (skip_to_close)
                {
                    /* skip to the end of the qualifier */
                    parse_gram_qual_skip(err, &done);
                    if (*err)
                        return 0;
                }
            }

            /* done */
            break;
            
        case TOKT_COLON:
            /* this is the end of the list - we're done */
            G_tok->next();
            done = TRUE;
            break;

        case TOKT_OR:
            /* 
             *   alternator - create a new alternative and add it to the
             *   production's list 
             */
            alt = new (G_prsmem) CTcGramProdAlt(gram_obj, dict_cur_);
            if (prod != 0)
                prod->add_alt(alt);

            /* skip the '|' token and proceed to the next token list */
            G_tok->next();
            break;

        case TOKT_SSTR:
            /* quoted string - this gives a literal string to match */
            tok = new (G_prsmem) CTcGramProdTok();
            tok->set_match_literal(G_tok->getcur()->get_text(),
                                   G_tok->getcur()->get_text_len());

            /* add it to the current alternative's list */
            alt->add_tok(tok);

            /* 
             *   go check for an arrow (to assign the matching token to a
             *   property) 
             */
            goto check_for_arrow;

        case TOKT_LT:
            /* 
             *   part-of-speech list - this gives a list, enclosed in angle
             *   brackets, of part-of-speech properties that can be matched
             *   by the token 
             */

            /* skip the open angle bracket */
            G_tok->next();

            /* set up a new token with a part-of-speech list */
            tok = new (G_prsmem) CTcGramProdTok();
            tok->set_match_part_list();

            /* add it to the current alternative's list */
            alt->add_tok(tok);

            /* keep going until we reach the closing bracket */
            for (;;)
            {
                CTcSymbol *sym;

                /* see what we have */
                switch(G_tok->cur())
                {
                case TOKT_GT:
                    /* 
                     *   closing angle bracket - we're done, so go check for
                     *   an arrow to assign the match to a property 
                     */
                    goto check_for_arrow;

                case TOKT_EOF:
                    /* log the error */
                    G_tok->log_error_curtok(TCERR_GRAMMAR_INVAL_TOK);

                    /* give up */
                    *err = TRUE;
                    return 0;

                case TOKT_SYM:
                    /* 
                     *   symbol - look it up, defining it as a property if
                     *   it's not already defined as a property 
                     */
                    sym = global_symtab_->find_or_def_prop_explicit(
                        G_tok->getcur()->get_text(),
                        G_tok->getcur()->get_text_len(), FALSE);

                    /* make sure it's a property */
                    if (sym != 0 && sym->get_type() == TC_SYM_PROP)
                    {
                        CTcSymProp *propsym = (CTcSymProp *)sym;

                        /* add the property to the token's list */
                        tok->add_match_part_ele(propsym->get_prop());
                    }
                    else
                    {
                        /* flag an error */
                        G_tok->log_error_curtok(TCERR_GRAMMAR_LIST_REQ_PROP);
                    }

                    /* we're done with the symbol, so skip it */
                    G_tok->next();

                    /* back for more */
                    break;

                default:
                    /* 
                     *   anything else is an error; assume the '>' was
                     *   missing, so just flag the error and then act as
                     *   though we reached the end of the list 
                     */
                    G_tok->log_error_curtok(TCERR_GRAMMAR_LIST_UNCLOSED);
                    goto check_for_arrow;
                }
            }
            /* NOT REACHED */

        case TOKT_SYM:
            /*
             *   symbol token - this gives a part-of-speech or
             *   sub-production match, depending on whether the symbol
             *   refers to a property (in which case we have a
             *   part-of-speech match) or an object (in which case we have a
             *   sub-production match).  
             */
            {
                CTcSymbol *sym;

                /* look it up in the global symbol table */
                sym = global_symtab_->find(G_tok->getcur()->get_text(),
                                           G_tok->getcur()->get_text_len());

                /* 
                 *   if it's not defined, and we're not in syntax-only mode,
                 *   provide a default definition of the symbol as a
                 *   production object 
                 */
                if (sym == 0 && !syntax_only_)
                {
                    CTcGramProdEntry *sub_prod;
                    
                    /* it's undefined - presume it's a production */
                    sub_prod = declare_gramprod(
                        G_tok->getcur()->get_text(),
                        G_tok->getcur()->get_text_len());

                    /* get the production's symbol */
                    sym = sub_prod->get_prod_sym();
                }

                /* check what kind of symbol we have */
                if (sym == 0)
                {
                    /* 
                     *   we're in parse-only mode and we have no symbol -
                     *   we can just ignore this until we're really
                     *   compiling 
                     */
                    tok = 0;
                }
                else if (sym->get_type() == TC_SYM_OBJ)
                {
                    CTcSymObj *objsym = (CTcSymObj *)sym;
                    
                    /* make sure it's a production */
                    if (objsym->get_metaclass() != TC_META_GRAMPROD)
                        G_tok->log_error_curtok(TCERR_GRAMMAR_REQ_PROD);

                    /* create the production token */
                    tok = new (G_prsmem) CTcGramProdTok();
                    tok->set_match_prod(objsym);
                }
                else if (sym->get_type() == TC_SYM_PROP)
                {
                    CTcSymProp *propsym = (CTcSymProp *)sym;
                    
                    /* create the part-of-speech token */
                    tok = new (G_prsmem) CTcGramProdTok();
                    tok->set_match_part_of_speech(propsym->get_prop());
                }
                else if (sym->get_type() == TC_SYM_ENUM)
                {
                    CTcSymEnum *enumsym = (CTcSymEnum *)sym;

                    /* make sure it's a token enum */
                    if (!enumsym->is_token())
                        G_tok->log_error_curtok(TCERR_GRAMMAR_BAD_ENUM);

                    /* create the token-type token */
                    tok = new (G_prsmem) CTcGramProdTok();
                    tok->set_match_token_type(enumsym->get_enum_id());
                }
                else
                {
                    /* it's an error */
                    G_tok->log_error_curtok(TCERR_GRAMMAR_REQ_OBJ_OR_PROP);

                    /* no token */
                    tok = 0;
                }
            }

            /* if we have a token, add it to the alternative's list */
            if (tok != 0)
                alt->add_tok(tok);

        check_for_arrow:
            /* skip the symbol and check for a '->' specification */
            if (G_tok->next() == TOKT_ARROW)
            {
                /* skip the arrow and make sure a symbol follows */
                if (G_tok->next() == TOKT_SYM)
                {
                    CTcSymbol *sym;
                    
                    /* look it up */
                    sym = look_up_prop(G_tok->getcur(), FALSE);
                    
                    /* set the association if we got a symbol */
                    if (sym != 0)
                    {
                        CTcSymProp *propsym = (CTcSymProp *)sym;
                        size_t i;
                        
                        /* set the property association for the token */
                        if (tok != 0)
                            tok->set_prop_assoc(propsym->get_prop());

                        /* 
                         *   Add the property to our list of assigned
                         *   properties - we'll use this to build the
                         *   grammar info list for the match object.  Only
                         *   add the property if it's not already in the
                         *   list.
                         */
                        for (i = 0 ; i < prop_arrow_cnt ; ++i)
                        {
                            /* if we found it, stop looking */
                            if (prop_arrows[i] == propsym)
                                break;
                        }

                        /* 
                         *   if we didn't find it, and we have room for
                         *   another, add it 
                         */
                        if (i == prop_arrow_cnt
                            && prop_arrow_cnt < prop_arrow_max)
                        {
                            /* it's not there - add it */
                            prop_arrows[prop_arrow_cnt++] = propsym;
                        }
                    }
                    else
                    {
                        /* log an error */
                        G_tok->log_error_curtok(TCERR_GRAMMAR_ARROW_REQ_PROP);
                    }
                    
                    /* skip the symbol */
                    G_tok->next();
                }
                else
                {
                    /* it's an error */
                    G_tok->log_error_curtok(TCERR_GRAMMAR_ARROW_REQ_PROP);
                }
            }
            break;

        case TOKT_TIMES:
            /* free-floating end token */
            tok = new (G_prsmem) CTcGramProdTok();
            tok->set_match_star();

            /* add it to the current alternative's list */
            alt->add_tok(tok);

            /* skip the star */
            G_tok->next();

            /* 
             *   this must be the last token in the alternative, so we
             *   must have a ':' or '|' following 
             */
            if (G_tok->cur() != TOKT_OR && G_tok->cur() != TOKT_COLON)
                G_tok->log_error_curtok(TCERR_GRAMMAR_STAR_NOT_END);
            break;

        case TOKT_EOF:
            /* log the error */
            G_tok->log_error_curtok(TCERR_GRAMMAR_INVAL_TOK);
            
            /* give up */
            *err = TRUE;
            return 0;

        case TOKT_LBRACE:
        case TOKT_RBRACE:
        case TOKT_SEM:
            /* 
             *   they probably meant to end the statement, even though
             *   this isn't the time or place to do so - log an error and
             *   stop scanning 
             */
            G_tok->log_error_curtok(TCERR_GRAMMAR_INVAL_TOK);
            done = TRUE;
            break;

        default:
            /* log an error */
            G_tok->log_error_curtok(TCERR_GRAMMAR_INVAL_TOK);

            /* 
             *   skip the offending token, and hope that they merely
             *   inserted something invalid 
             */
            G_tok->next();
            break;
        }
    }

    /* parse the object body */
    stm = parse_object_body(err, gram_obj, TRUE, is_anon, TRUE,
                            FALSE, modify, mod_orig_sym, 0, 0, 0, FALSE);

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
        for (i = 0 ; i < prop_arrow_cnt ; ++i)
        {
            /* add a node to evaluate this property symbol */
            lst->add_element(create_sym_node(prop_arrows[i]->get_sym(),
                                             prop_arrows[i]->get_sym_len()));
        }

        /* add a property for this */
        stm->add_prop(graminfo_prop_, lst, FALSE, FALSE);
    }

    /* return the object definition statement */
    return stm;
}

/*
 *   Flatten a set of grammar rules.  Each time we find a parenthesized
 *   alternator, we'll expand the alternatives, until we have no
 *   parenthesized alternators. 
 */
CTcPrsGramNode *CTcParser::flatten_gram_rule(int *err)
{
    CTcPrsGramNode *tree;
    
    /* first, build the top-level 'or' tree */
    tree = parse_gram_or(err, 0);
    if (tree == 0 || *err != 0)
        return 0;

    /* move all of the OR's to a single OR at the top of the tree */
    tree = tree->consolidate_or();

    /* flatten CAT nodes in the resulting tree */
    tree->flatten_cat();

    /* 
     *   if the top-level node isn't an OR, insert an OR at the top - this
     *   makes the tree always follow the same shape, which makes it easier
     *   to step through the tokens in the expansion
     */
    if (!tree->is_or())
    {
        CTcPrsGramNodeOr *new_or;

        /* create an OR node for the root of the tree */
        new_or = new (G_prsmem) CTcPrsGramNodeOr();

        /* insert our old root as the single alternative under the OR */
        new_or->add_sub_item(tree);

        /* the OR is now the root of the tree */
        tree = new_or;
    }

    /* start reading from the first token in the tree */
    tree->init_expansion();

    /* return the tree */
    return tree;
}

/*
 *   Parse an OR expression in a grammar rule.  Returns an OR node
 *   representing the expression.  
 */
CTcPrsGramNode *CTcParser::parse_gram_or(int *err, int level)
{
    CTcPrsGramNodeOr *or_node;
    CTcPrsGramNode *sub;

    /* build our 'or' node */
    or_node = new (G_prsmem) CTcPrsGramNodeOr();

    /* if the rule is completely empty, warn about it */
    if (level == 0 && G_tok->cur() == TOKT_COLON && !syntax_only_)
        G_tok->log_warning(TCERR_GRAMMAR_EMPTY);

    /* parse our initial 'cat' subnode */
    sub = parse_gram_cat(err, level + 1);

    /* abort on error */
    if (sub == 0 || *err != 0)
        return 0;

    /* add the subnode to our 'or' list */
    or_node->add_sub_item(sub);

    /* keep going as long as we have more rules appended with '|' */
    while (G_tok->cur() == TOKT_OR)
    {
        /* skip the '|' */
        G_tok->next();

        /* 
         *   if we're at the top level, and the next token is the closing
         *   ':', warn about the empty last rule 
         */
        if (level == 0 && G_tok->cur() == TOKT_COLON && !syntax_only_)
            G_tok->log_warning(TCERR_GRAMMAR_ENDS_WITH_OR);

        /* parse the 'cat' subnode */
        sub = parse_gram_cat(err, level + 1);

        /* abort on error */
        if (sub == 0 || *err != 0)
            return 0;

        /* add the subnode to our 'or' list */
        or_node->add_sub_item(sub);
    }

    /* return the 'or' list */
    return or_node;
}

/*
 *   Parse a concatenation expression in a grammar rule.  Returns a CAT node
 *   representing the expression.  
 */
CTcPrsGramNode *CTcParser::parse_gram_cat(int *err, int level)
{
    CTcPrsGramNodeCat *cat_node;
    CTcPrsGramNode *sub;

    /* build our concatenation node */
    cat_node = new (G_prsmem) CTcPrsGramNodeCat();

    /* add tokens to the cat list until we find the end of the list */
    for (;;)
    {
        /* see what we have */
        switch(G_tok->cur())
        {
        case TOKT_LPAR:
            /* skip the paren */
            G_tok->next();

            /* parse the OR expression */
            sub = parse_gram_or(err, level + 1);

            /* if that failed, abort */
            if (sub == 0 || *err != 0)
                return 0;

            /* add the 'OR' to our concatenation expression */
            cat_node->add_sub_item(sub);

            /* make sure we're at the close paren */
            if (G_tok->cur() != TOKT_RPAR)
                G_tok->log_error_curtok(TCERR_GRAMMAR_REQ_RPAR_AFTER_GROUP);
            else
                G_tok->next();

            /* 
             *   make sure we don't have an arrow immediately following; an
             *   arrow isn't allowed after a close paren, because a group
             *   isn't a true subproduction 
             */
            if (G_tok->cur() == TOKT_ARROW)
                G_tok->log_error(TCERR_GRAMMAR_GROUP_ARROW_NOT_ALLOWED);

            /* done with the 'or' expression */
            break;

        case TOKT_OR:
            /* 
             *   the 'or' has lower precedence than concatenation, so we've
             *   reached the end of the concatenation expression - simply
             *   return what we have so far and let the caller figure out
             *   where to go next 
             */
            return cat_node;

        case TOKT_COLON:
        case TOKT_SEM:
        case TOKT_LBRACE:
        case TOKT_RBRACE:
        case TOKT_EOF:
        case TOKT_RPAR:
            /* we've reached the end of the rule */
            return cat_node;

        default:
            /* anything else is simply concatenated to the list so far */
            cat_node->add_sub_item(
                new (G_prsmem) CTcPrsGramNodeTok(G_tok->getcur()));

            /* skip the token and keep scanning */
            G_tok->next();
            break;
        }
    }
}
            
/*
 *   Parse a grammar qualifier integer value 
 */
int CTcParser::parse_gram_qual_int(int *err, const char *qual_name,
                                   int *stm_end)
{
    CTcPrsNode *expr;
    CTcConstVal *cval;

    /* skip the qualifier name */
    G_tok->next();

    /* check for a missing expression */
    if (G_tok->cur() == TOKT_RBRACK)
    {
        /* don't bother looking for an expression - it's not there */
        expr = 0;
    }
    else
    {
        /* parse an expression */
        expr = parse_expr();
    }

    /* 
     *   if we didn't get an expression, or it doesn't have a constant
     *   value, or the constant value is something other than an integer,
     *   it's an error 
     */
    if (expr == 0
        || (cval = expr->get_const_val()) == 0
        || cval->get_type() != TC_CVT_INT)
    {
        /* log an error */
        G_tok->log_error(TCERR_GRAMMAR_QUAL_REQ_INT, qual_name);

        /* skip to the closing bracket */
        parse_gram_qual_skip(err, stm_end);

        /* we don't have a value to return; use zero by default */
        return 0;
    }
    else
    {
        /* return the constant expression value */
        return cval->get_val_int();
    }
}

/*
 *   Skip to the end of a grammar qualifier.  This is used when a syntax
 *   error occurs and we abandon parsing the rest of the qualifier. 
 */
void CTcParser::parse_gram_qual_skip(int *err, int *stm_end)
{
    /* scan until we find the end */
    for (;;)
    {
        switch(G_tok->next())
        {
        case TOKT_RBRACK:
            /* 
             *   that's the end of the mal-formed qualifier; skip the
             *   bracket and keep going from here 
             */
            G_tok->next();
            break;

        case TOKT_EOF:
        case TOKT_RBRACE:
        case TOKT_LBRACE:
        case TOKT_SEM:
            /* 
             *   probably the end of the statement - stop scanning, and
             *   set the 'stm_end' flag to tell the caller that we're done
             *   parsing the entire statement 
             */
            *stm_end = TRUE;
            return;
            
        default:
            /* skip everything else and just keep going */
            break;
        }
    }
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
    int argc;
    int varargs;
    int varargs_list;
    CTcSymLocal *varargs_list_local;
    int has_retval;
    int redef;
    CTcTokFileDesc *sym_file;
    long sym_linenum;
    CTcFormalTypeList *type_list = 0;

    /* skip the 'function' keyword if present */
    if (func_kw_present)
        G_tok->next();

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
            /* no arguments */
            argc = 0;
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
                parse_formal_list(TRUE, FALSE, &argc, 0, &varargs,
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
                                    &argc, &varargs,
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
                parse_code_body(FALSE, FALSE, FALSE, &argc, &varargs,
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
            code_body = parse_code_body(FALSE, FALSE, FALSE, &argc, &varargs,
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
            func_sym = (CTcSymFunc *)global_symtab_->find(
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
                                          FALSE, 0, TRUE, TRUE,
                                          TRUE, TRUE, TRUE);

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
        func_sym = (CTcSymFunc *)
                   global_symtab_->find(tok.get_text(), tok.get_text_len());

        /* 
         *   If the symbol was already defined, the previous definition must
         *   be external, or this new definition must be external, or the new
         *   definition must be a 'replace' definition; and the two
         *   definitions must exactly match the new parameters and the
         *   multi-method status.  Ignore differences in the return value,
         *   since we have no way to specify in 'extern' definitions whether
         *   a function has a return value or not.  
         */
        redef = (func_sym != 0
                 && (func_sym->get_type() != TC_SYM_FUNC
                     || (!func_sym->is_extern()
                         && !is_extern && !replace && !modify)
                     || func_sym->get_argc() != argc
                     || func_sym->is_varargs() != varargs
                     || func_sym->is_multimethod() != (type_list != 0)));

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
                                      FALSE, argc, varargs, has_retval,
                                      type_list != 0, FALSE, is_extern);

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
                                         argc, varargs, has_retval,
                                         func_sym->is_multimethod(),
                                         func_sym->is_multimethod_base(),
                                         func_sym->is_extern());

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

/*
 *   Parse an 'intrinsic' top-level statement 
 */
CTPNStmTop *CTcParser::parse_intrinsic(int *err)
{
    int list_done;
    int fn_set_id;
    int fn_set_idx;
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
                                    argc - optargc, argc, varargs);
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
    CTcSymMetaclass *old_meta_sym = 0;
    
    /* skip the 'class' keyword and check the class name symbol */
    if (G_tok->next() == TOKT_SYM)
    {
        /* get the token */
        meta_tok = *G_tok->copycur();

        /* see if it's defined yet */
        old_meta_sym = (CTcSymMetaclass *)
                       global_symtab_->find(meta_tok.get_text(),
                                            meta_tok.get_text_len());
        if (old_meta_sym != 0)
        {
            /* log an error */
            G_tok->log_error_curtok(TCERR_REDEF_INTRINS_NAME);
        }
        else
        {
            /* note that we got the name token successfully */
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
            CTcSymMetaclass *table_sym;
            
            /* define the new symbol */
            meta_sym = new CTcSymMetaclass(meta_tok.get_text(),
                                           meta_tok.get_text_len(),
                                           FALSE, 0, G_cg->new_obj_id());
            global_symtab_->add_entry(meta_sym);

            /* tell the code generator to add this metaclass */
            meta_id = G_cg->find_or_add_meta(G_tok->getcur()->get_text(),
                                             G_tok->getcur()->get_text_len(),
                                             meta_sym);

            /* 
             *   if the metaclass was already defined for another symbol,
             *   it's an error
             */
            table_sym = G_cg->get_meta_sym(meta_id);
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
            CTcSymMetaclass *sc_meta_sym;
            
            /* look up the superclass in the global symbols */
            sc_meta_sym = (CTcSymMetaclass *)global_symtab_->find(
                G_tok->getcur()->get_text(), G_tok->getcur()->get_text_len());

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
        int is_static;

        /* presume it won't be a 'static' property */
        is_static = FALSE;

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

/*
 *   Parse an object template definition.  If the class token is null, this
 *   is a root object template; otherwise, the class token gives the class
 *   symbol with which to associate the template.  
 */
CTPNStmTop *CTcParser::parse_template_def(int *err, const CTcToken *class_tok)
{
    int done;
    int all_ok;
    CTcToken prop_tok;
    CTcObjTemplateItem *alt_group_head;
    CTcObjTemplateItem *item_head;
    CTcObjTemplateItem *item_tail;
    size_t item_cnt;
    CTcSymObj *class_sym;
    int found_inh;

    /* no items in our list yet */
    item_head = item_tail = alt_group_head = 0;
    item_cnt = 0;

    /* presume we won't find an 'inherited' token */
    found_inh = FALSE;

    /* 
     *   If there's a class token, it must refer to an object class.  If
     *   there's no such symbol defined yet, define it as an external
     *   object; otherwise, make sure the existing symbol is an object of
     *   metaclass tads-object.  
     */
    if (class_tok != 0)
    {
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
    for (done = FALSE, all_ok = TRUE ; !done ; )
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
void CTcParser::add_template_def(CTcSymObj *class_sym,
                                 CTcObjTemplateItem *item_head,
                                 size_t item_cnt)
{
    CTcObjTemplate *tpl;
    
    /* create the new template list entry */
    tpl = new (G_prsmem) CTcObjTemplate(item_head, item_cnt);
    
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
 *   Look up a property symbol.  If the symbol is not defined, add it.  If
 *   it's defined as some other kind of symbol, we'll log an error and
 *   return null. 
 */
CTcSymProp *CTcParser::look_up_prop(const CTcToken *tok, int show_err)
{
    CTcSymProp *prop;
    
    /* look up the symbol */
    prop = (CTcSymProp *)global_symtab_->find(tok->get_text(),
                                              tok->get_text_len());

    /* if it's already defined, make sure it's a property */
    if (prop != 0)
    {
        /* make sure it's a property */
        if (prop->get_type() != TC_SYM_PROP)
        {
            /* log an error if appropriate */
            if (show_err)
                G_tok->log_error(TCERR_REDEF_AS_PROP,
                                 (int)tok->get_text_len(), tok->get_text());
            
            /* we can't use the symbol, so forget it */
            prop = 0;
        }
    }
    else
    {
        /* create the new property symbol */
        prop = new CTcSymProp(tok->get_text(), tok->get_text_len(),
                              FALSE, G_cg->new_prop_id());

        /* add it to the global symbol table */
        global_symtab_->add_entry(prop);

        /* mark it as referenced */
        prop->mark_referenced();
    }

    /* return the property symbol */
    return prop;
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

/*
 *   Find or define an object symbol. 
 */
CTcSymObj *CTcParser::find_or_def_obj(const char *tok_txt, size_t tok_len,
                                      int replace, int modify,
                                      int *is_class,
                                      CTcSymObj **mod_orig_sym,
                                      CTcSymMetaclass **meta_sym,
                                      int *trans)
{
    CTcSymbol *sym;
    CTcSymObj *obj_sym;

    /* we don't have a modify base symbol yet */
    *mod_orig_sym = 0;

    /* presume we won't find a modified metaclass definition */
    if (meta_sym != 0)
        *meta_sym = 0;

    /* look up the symbol to see if it's already defined */
    sym = global_symtab_->find(tok_txt, tok_len);

    /* check for 'modify' used with an intrinsic class */
    if (meta_sym != 0
        && modify
        && sym != 0
        && sym->get_type() == TC_SYM_METACLASS)
    {
        CTcSymObj *old_mod_obj;

        /*
         *   We're modifying an intrinsic class.  In this case, the base
         *   object is the previous modification object for the class, or no
         *   object at all.  In either case, we must create a new object and
         *   assign it to the metaclass.  
         */

        /* get the metaclass symbol */
        *meta_sym = (CTcSymMetaclass *)sym;

        /* get the original modification object */
        old_mod_obj = (*meta_sym)->get_mod_obj();

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
            CTPNStmObject *base_stm;

            /* create an anonymous base object */
            old_mod_obj = CTcSymObj::synthesize_modified_obj_sym(FALSE);

            /* the object is of metaclass IntrinsicClassModifier */
            old_mod_obj->set_metaclass(TC_META_ICMOD);

            /* give the dummy base object an empty statement */
            base_stm = new CTPNStmObject(old_mod_obj, FALSE);
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

/*
 *   Parse an object definition 
 */
CTPNStmTop *CTcParser::parse_object(int *err, int replace, int modify,
                                    int is_class, int plus_cnt, int trans)
{
    CTcSymObj *obj_sym;
    CTcSymObj *mod_orig_sym;
    CTcSymMetaclass *meta_sym;

    /* find or define the symbol */
    obj_sym = find_or_def_obj(G_tok->getcur()->get_text(),
                              G_tok->getcur()->get_text_len(),
                              replace, modify, &is_class,
                              &mod_orig_sym, &meta_sym, &trans);

    /* skip the object name */
    G_tok->next();

    /* parse the body */
    return parse_object_body(err, obj_sym, is_class, FALSE, FALSE, FALSE,
                             modify, mod_orig_sym, plus_cnt, meta_sym, 0,
                             trans);
}

/*
 *   Parse an anonymous object 
 */
CTPNStmObject *CTcParser::parse_anon_object(int *err, int plus_cnt,
                                            int is_nested,
                                            tcprs_term_info *term_info,
                                            int trans)
{
    CTcSymObj *obj_sym;
    
    /* create an anonymous object symbol */
    obj_sym = new CTcSymObj(".anon", 5, FALSE, G_cg->new_obj_id(),
                            FALSE, TC_META_TADSOBJ, 0);

    /* set the dictionary for the new object */
    obj_sym->set_dict(dict_cur_);

    /* parse the object body */
    return parse_object_body(err, obj_sym, FALSE, TRUE, FALSE, is_nested,
                             FALSE, 0, plus_cnt, 0, term_info, trans);
}

/* ------------------------------------------------------------------------ */
/*
 *   'propertyset' definition structure.  Each property set defines a
 *   property pattern and an optional argument list for the properties
 *   within the propertyset group.  
 */
struct propset_def
{
    /* the property name pattern */
    const char *prop_pattern;
    size_t prop_pattern_len;

    /* head of list of tokens in the parameter list */
    struct propset_tok *param_tok_head;
};

/*
 *   propertyset token list entry 
 */
struct propset_tok
{
    propset_tok(const CTcToken *t)
    {
        /* copy the token */
        this->tok = *t;

        /* we're not in a list yet */
        nxt = 0;
    }
    
    /* the token */
    CTcToken tok;
    
    /* next token in the list */
    propset_tok *nxt;
};

/*
 *   Token source for parsing formal parameters using property set formal
 *   lists.  This retrieves tokens from a propertyset stack.  
 */
class propset_token_source: public CTcTokenSource
{
public:
    propset_token_source()
    {
        /* nothing in our list yet */
        nxt_tok = last_tok = 0;
    }
    
    /* get the next token */
    virtual const CTcToken *get_next_token()
    {
        /* if we have another entry in our list, retrieve it */
        if (nxt_tok != 0)
        {
            CTcToken *ret;

            /* remember the token to return */
            ret = &nxt_tok->tok;

            /* advance our internal position to the next token */
            nxt_tok = nxt_tok->nxt;

            /* return the token */
            return ret;
        }
        else
        {
            /* we have nothing more to return */
            return 0;
        }
    }

    /* insert a token */
    void insert_token(const CTcToken *tok)
    {
        propset_tok *cur;
        
        /* create a new link entry and initialize it */
        cur = new (G_prsmem) propset_tok(tok);

        /* link it into our list */
        if (last_tok != 0)
            last_tok->nxt = cur;
        else
            nxt_tok = cur;
        last_tok = cur;
    }

    /* insert a token based on type */
    void insert_token(tc_toktyp_t typ, const char *txt, size_t len)
    {
        CTcToken tok;

        /* set up the token object */
        tok.settyp(typ);
        tok.set_text(txt, len);

        /* insert it */
        insert_token(&tok);
    }

    /* the next token we're to retrieve */
    propset_tok *nxt_tok;

    /* tail of our list */
    propset_tok *last_tok;
};

/* maximum propertyset nesting depth */
const size_t MAX_PROPSET_DEPTH = 10;

/* ------------------------------------------------------------------------ */
/*
 *   Parse an object body 
 */
CTPNStmObject *CTcParser::parse_object_body(int *err, CTcSymObj *obj_sym,
                                            int is_class, int is_anon,
                                            int is_grammar,
                                            int is_nested, int modify,
                                            CTcSymObj *mod_orig_sym,
                                            int plus_cnt,
                                            CTcSymMetaclass *meta_sym,
                                            tcprs_term_info *term_info,
                                            int trans)
{
    CTPNStmObject *obj_stm;
    CTcDictPropEntry *dict_prop;
    int done;
    int braces;
    tcprs_term_info my_term_info;
    int propset_depth;
    propset_def propset_stack[MAX_PROPSET_DEPTH];
    CTcSymObj *sc_sym;
    int old_self_valid;

    /* we are not in a propertyset definition yet */
    propset_depth = 0;

    /* 
     *   If we don't have an enclosing term_info, use my own; if a valid
     *   term_info was passed in, continue using the enclosing one.  We
     *   always want to use the outermost term_info, because our heuristic
     *   is to assume that any lack of termination applies to the first
     *   object it possibly could apply to. 
     */
    if (term_info == 0)
        term_info = &my_term_info;

    /* presume the object won't use braces around its property list */
    braces = FALSE;

    /* create the object statement */
    obj_stm = new CTPNStmObject(obj_sym, is_class);
    if (obj_sym != 0)
        obj_sym->set_obj_stm(obj_stm);

    /* set the 'transient' status if appropriate */
    if (trans)
        obj_stm->set_transient();

    /* remember whether 'self' was valid in the enclosing context */
    old_self_valid = self_valid_;

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
        for (done = FALSE ; !done ; )
        {
            /* we need a symbol */
            switch(G_tok->cur())
            {
            case TOKT_SYM:
                /* 
                 *   It's another superclass.  Look up the superclass
                 *   symbol.  
                 */
                sc_sym = (CTcSymObj *)get_global_symtab()->find(
                    G_tok->getcur()->get_text(),
                    G_tok->getcur()->get_text_len());

                /* 
                 *   If this symbol is defined, and it's an object, check to
                 *   make sure this won't set up a circular class definition
                 *   - so, make sure the base class isn't the same as the
                 *   object being defined, and that it doesn't inherit from
                 *   the object being defined.  
                 */
                if (sc_sym != 0
                    && sc_sym->get_type() == TC_SYM_OBJ
                    && (sc_sym == obj_sym
                        || sc_sym->has_superclass(obj_sym)))
                {
                    /* 
                     *   this is a circular class definition - complain
                     *   about it and don't add it to my superclass list 
                     */
                    G_tok->log_error(TCERR_CIRCULAR_CLASS_DEF,
                                     (int)sc_sym->get_sym_len(),
                                     sc_sym->get_sym(),
                                     (int)obj_sym->get_sym_len(),
                                     obj_sym->get_sym());
                }
                else
                {
                    /* it's good - add the new superclass to our list */
                    obj_stm->add_superclass(G_tok->getcur());

                    /* 
                     *   add it to the symbol's superclass name list as well
                     *   - we use this for keeping track of the hierarchy in
                     *   the symbol file for compile-time access 
                     */
                    if (obj_sym != 0)
                        obj_sym->add_sc_name_entry(
                            G_tok->getcur()->get_text(),
                            G_tok->getcur()->get_text_len());
                }
                                
                /* skip the symbol and see what follows */
                switch (G_tok->next())
                {
                case TOKT_COMMA:
                    /* we have another superclass following */
                    G_tok->next();
                    break;
                    
                default:
                    /* no more superclasses */
                    done = TRUE;
                    break;
                }
                break;
                
            case TOKT_OBJECT:
                /* 
                 *   it's a basic object definition - make sure other
                 *   superclasses weren't specified 
                 */
                if (obj_stm->get_first_sc() != 0)
                    G_tok->log_error(TCERR_OBJDEF_OBJ_NO_SC);
                
                /* 
                 *   mark the object as having an explicit superclass of the
                 *   root object class 
                 */
                if (obj_sym != 0)
                    obj_sym->set_sc_is_root(TRUE);
                
                /* 
                 *   skip the 'object' keyword and we're done - there's no
                 *   superclass list 
                 */
                G_tok->next();
                done = TRUE;
                break;
                
            default:
                /* premature end of the object list */
                G_tok->log_error_curtok(TCERR_OBJDEF_REQ_SC);
                
                /* stop here */
                done = TRUE;
                break;
            }
        }
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
    for (dict_prop = dict_prop_head_ ; dict_prop != 0 ;
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
            CTPNStmObject *loc;
            CTcConstVal cval;
            CTcPrsNode *expr;
            
            /* 
             *   find the object - the location of an object with N '+'
             *   signs is the object most recently defined with N-1 '+'
             *   signs 
             */
            loc = plus_stack_[plus_cnt - 1];

            /* add the '+' property value, if it's a valid object symbol */
            if (loc->get_obj_sym() != 0)
            {
                CTPNObjProp *prop;

                /* set up a constant expression for the object reference */
                cval.set_obj(loc->get_obj_sym()->get_obj_id(),
                             loc->get_obj_sym()->get_metaclass());
                expr = new CTPNConst(&cval);
                
                /* add the location property */
                prop = obj_stm->add_prop(plus_prop_, expr, FALSE, FALSE);

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

    /*
     *   check for template syntax 
     */
    switch(G_tok->cur())
    {
    case TOKT_SSTR:
    case TOKT_DSTR:
    case TOKT_DSTR_START:
    case TOKT_LBRACK:
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
        /* we have an object template */
        parse_obj_template(err, obj_stm);

        /* if that failed, return failure */
        if (*err)
        {
            /* restore the original 'self' validity, and return failure */
            self_valid_ = old_self_valid;
            return 0;
        }

        /* proceed to parse the rest of the object body */
        break;

    default:
        /* it's not a template - proceed to the rest of the object body */
        break;
    }

    /*
     *   If we didn't already find an open brace, try again now that we've
     *   scanned the template properties, since template properties can
     *   either immediately precede or immediately follow the open brace, if
     *   present. 
     */
    if (!braces && G_tok->cur() == TOKT_LBRACE)
    {
        /* skip the open brace */
        G_tok->next();

        /* note that we're using braces */
        braces = TRUE;
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
     *   'sourceTextOrder' property, and the 'sourceTextGroup' if desired.
     *   sourceTextOrder provides a source file sequence number that can be
     *   used to order a set of objects into the same sequence in which they
     *   appeared in the original source file, which is often useful for
     *   assembling pre-initialized structures.  sourceTextGroup identifies
     *   the file itself.  
     */
    if (!is_class && !modify)
    {
        CTPNObjProp *prop;
        CTcConstVal cval;
        CTcPrsNode *expr;
        
        /* set up the constant expression for the source sequence number */
        cval.set_int(src_order_idx_++);
        expr = new CTPNConst(&cval);

        /* create the new sourceTextOrder property */
        prop = obj_stm->add_prop(src_order_sym_, expr, FALSE, FALSE);

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

    /*
     *   Parse the property list.  Keep going until we get to the closing
     *   semicolon.  
     */
    for (done = FALSE ; !done ; )
    {
        /* 
         *   Remember the statement start location.  Some types of
         *   property definitions result in implicit statements being
         *   created, so we must record the statement start position for
         *   those cases.  
         */
        cur_desc_ = G_tok->get_last_desc();
        cur_linenum_ = G_tok->get_last_linenum();

        /* parse the property definition */
        switch(G_tok->cur())
        {
        case TOKT_PROPERTYSET:
            {
                propset_def *cur_def;
                propset_def dummy_def;
                
                /* 
                 *   It's a property set definition.  These definitions
                 *   nest; make sure we have space left in our stack.  
                 */
                if (propset_depth >= MAX_PROPSET_DEPTH)
                {
                    /* 
                     *   nested too deeply - flag the error if this is the
                     *   one that pushes us over the edge 
                     */
                    if (propset_depth == MAX_PROPSET_DEPTH)
                        G_tok->log_error(TCERR_PROPSET_TOO_DEEP);
                    
                    /* 
                     *   keep going with a syntactic parse despite the
                     *   problems - increment the current depth, but use a
                     *   dummy definition object, since we have no more
                     *   stack entries to store the real data 
                     */
                    ++propset_depth;
                    cur_def = &dummy_def;
                }
                else
                {
                    /* set up to use the next available stack entry */
                    cur_def = &propset_stack[propset_depth];

                    /* consume this stack entry */
                    ++propset_depth;
                }
                
                /* skip the keyword and get the property name pattern */
                if (G_tok->next() == TOKT_SSTR)
                {
                    int star_cnt;
                    int inval_cnt;
                    utf8_ptr p;
                    size_t rem;
                    
                    /* note the pattern string */
                    cur_def->prop_pattern = G_tok->getcur()->get_text();
                    cur_def->prop_pattern_len =
                        G_tok->getcur()->get_text_len();

                    /* 
                     *   Validate the pattern.  It must consist of valid
                     *   token characters, except for a single asterisk,
                     *   which it must have.
                     */
                    for (star_cnt = inval_cnt = 0,
                         p.set((char *)cur_def->prop_pattern),
                         rem = cur_def->prop_pattern_len ;
                         rem != 0 ; p.inc(&rem))
                    {
                        /* check this character */
                        if (p.getch() == '*')
                            ++star_cnt;
                        else if (!is_sym(p.getch()))
                            ++inval_cnt;
                    }

                    /* 
                     *   if the first character isn't an asterisk, it must
                     *   be a valid initial symbol character 
                     */
                    if (cur_def->prop_pattern_len != 0)
                    {
                        wchar_t firstch;

                        /* get the first character */
                        firstch = utf8_ptr::s_getch(cur_def->prop_pattern);

                        /* 
                         *   if it's not a '*', it must be a valid initial
                         *   token character 
                         */
                        if (firstch != '*' && !is_syminit(firstch))
                            ++inval_cnt;
                    }

                    /* 
                     *   if it has no '*' or more than one '*', or it has
                     *   invalid characters, flag it as invalid 
                     */
                    if (star_cnt != 1 || inval_cnt != 0)
                        G_tok->log_error_curtok(TCERR_PROPSET_INVAL_PAT);
                }
                else
                {
                    /* not a string; flag an error, but try to keep going */
                    G_tok->log_error_curtok(TCERR_PROPSET_REQ_STR);
                    
                    /* we have no pattern string */
                    cur_def->prop_pattern = "";
                    cur_def->prop_pattern_len = 0;
                }

                /* we have no parameter token list yet */
                cur_def->param_tok_head = 0;

                /* 
                 *   skip the pattern string, and see if we have an argument
                 *   list 
                 */
                if (G_tok->next() == TOKT_LPAR)
                {
                    propset_tok *last;
                    propset_tok *cur;
                    int star_cnt;

                    /*
                     *   Current parsing state: 0=start, 1=after symbol or
                     *   '*', 2=after comma, 3=done 
                     */
                    int state;

                    /* start with an empty token list */
                    last = 0;
                    
                    /*
                     *   We have a formal parameter list that is to be
                     *   applied to each property in the set.  Parse tokens
                     *   up to the closing paren, stashing them in our list.
                     */
                    for (G_tok->next(), state = 0, star_cnt = 0 ;
                         state != 3 ; )
                    {
                        /* check the token */
                        switch (G_tok->cur())
                        {
                        case TOKT_LBRACE:
                        case TOKT_RBRACE:
                        case TOKT_SEM:
                            /* 
                             *   A brace or semicolon - assume that the right
                             *   paren was accidentally omitted.  Flag the
                             *   error and consider the list to be finished. 
                             */
                            G_tok->log_error_curtok(
                                TCERR_MISSING_RPAR_FORMAL);

                            /* switch to 'done' state */
                            state = 3;
                            break;

                        case TOKT_RPAR:
                            /* 
                             *   Right paren.  This ends the list.  If we're
                             *   in state 2 ('after comma'), the paren is out
                             *   of place, so flag it as an error - but still
                             *   count it as ending the list.  
                             */
                            if (state == 2)
                                G_tok->log_error(TCERR_MISSING_LAST_FORMAL);

                            /* skip the paren */
                            G_tok->next();

                            /* switch to state 'done' */
                            state = 3;
                            break;

                        case TOKT_SYM:
                        case TOKT_TIMES:
                            /*
                             *   Formal parameter name or '*'.  We can accept
                             *   these in states 'start' and 'after comma' -
                             *   in those states, just go add the token to
                             *   the list we're gathering.  In state 'after
                             *   formal', this is an error - a comma should
                             *   have come between the two parameters.  
                             */
                            if (state == 1)
                            {
                                /* 
                                 *   'after formal' - a comma must be
                                 *   missing.  Flag the error, but then
                                 *   proceed as though the comma had been
                                 *   there. 
                                 */
                                G_tok->log_error_curtok(
                                    TCERR_REQ_COMMA_FORMAL);
                            }

                            /* switch to state 'after formal' */
                            state = 1;

                        add_to_list:
                            /* add the current token to the token list */
                            cur = new (G_prsmem) propset_tok(G_tok->getcur());
                            if (last != 0)
                                last->nxt = cur;
                            else
                                cur_def->param_tok_head = cur;
                            last = cur;

                            /* if it's a '*', count it */
                            if (G_tok->cur() == TOKT_TIMES)
                                ++star_cnt;

                            /* skip it */
                            G_tok->next();
                            break;

                        case TOKT_COMMA:
                            /*
                             *   In state 'after formal', a comma just goes
                             *   into our token list.  A comma is invalid in
                             *   any other state.  
                             */
                            if (state == 1)
                            {
                                /* switch to state 'after comma' */
                                state = 2;

                                /* go add the token to our list */
                                goto add_to_list;
                            }
                            else
                            {
                                /* otherwise, go handle as a bad token */
                                goto bad_token;
                            }

                        default:
                        bad_token:
                            /* generate an error according to our state */
                            switch (state)
                            {
                            case 0:
                            case 2:
                                /* 
                                 *   'start' or 'after comma' - we expected a
                                 *   formal name 
                                 */
                                G_tok->log_error_curtok(TCERR_REQ_SYM_FORMAL);
                                break;

                            case 1:
                                /* 'after formal' - expected a comma */
                                G_tok->log_error_curtok(
                                    TCERR_REQ_COMMA_FORMAL);
                                break;
                            }

                            /* skip the token and keep going */
                            G_tok->next();
                            break;
                        }
                    }

                    /* make sure we have exactly one star */
                    if (star_cnt != 1)
                        G_tok->log_error(TCERR_PROPSET_INVAL_FORMALS);
                }

                /* require the open brace for the set */
                if (G_tok->cur() == TOKT_LBRACE)
                    G_tok->next();
                else
                    G_tok->log_error_curtok(TCERR_PROPSET_REQ_LBRACE);
            }
                
            /* proceed to parse the properties within */
            break;
            
        case TOKT_SEM:
            /*
             *   If we're using brace notation (i.e., the property list is
             *   surrounded by braces), a semicolon inside the property list
             *   is ignored.  If we're using regular notation, a semicolon
             *   ends the property list.  
             */
            if (braces || propset_depth != 0)
            {
                /* 
                 *   we're using brace notation, or we're inside the braces
                 *   of a propertyset, so semicolons are ignored 
                 */
            }
            else
            {
                /* 
                 *   we're using regular notation, so this semicolon
                 *   terminates the property list and the object body - note
                 *   that we're done parsing the object body 
                 */
                done = TRUE;
            }

            /* in any case, skip the semicolon and carry on */
            G_tok->next();
            break;

        case TOKT_CLASS:
        case TOKT_EXTERN:
        case TOKT_MODIFY:
        case TOKT_DICTIONARY:
        case TOKT_PROPERTY:
        case TOKT_PLUS:
        case TOKT_INC:
        case TOKT_INTRINSIC:
        case TOKT_OBJECT:
        case TOKT_GRAMMAR:
        case TOKT_ENUM:
            /* 
             *   All of these probably indicate the start of a new
             *   statement - they probably just left off the closing
             *   semicolon of the current object.  Flag the error and
             *   return, on the assumption that we'll find a new statement
             *   starting here. 
             */
            G_tok->log_error_curtok(braces || propset_depth != 0
                                    ? TCERR_OBJ_DEF_REQ_RBRACE
                                    : TCERR_OBJ_DEF_REQ_SEM);
            done = TRUE;
            break;

        case TOKT_SYM:
            /* 
             *   Property definition.  For better recovery from certain
             *   common errors, check a couple of scenarios.
             *   
             *   Look ahead one token.  If the next token is a colon, then
             *   we have a nested object definition (the property is
             *   assigned to a reference to an anonymous object about to be
             *   defined in-line).  This could also indicate a missing
             *   semicolon (or close brace) immediately before the symbol
             *   we're taking as a property name - the symbol we just parsed
             *   could actually be intended as the next object's name, and
             *   the colon introduces its superclass list.  If the new
             *   object's property list isn't enclosed in braces, this is
             *   probably what happened.
             *   
             *   If the next token is '(', '{', or '=', it's almost
             *   certainly a property definition, so proceed on that
             *   assumption.
             *   
             *   If the next token is a symbol, we have one of two probable
             *   errors.  Either we have a missing '=' in a property
             *   definition, or this is a new anonymous object definition,
             *   and the current object definition is missing its
             *   terminating semicolon.  If the current symbol is a class
             *   name, it's almost certainly a new object definition;
             *   otherwise, assume it's really a property definition, and
             *   we're just missing the '='.  
             */

            /* look ahead at the next token */
            switch(G_tok->next())
            {
            case TOKT_LPAR:
            case TOKT_LBRACE:
            case TOKT_EQ:
                /* 
                 *   It's almost certainly a property definition, no
                 *   matter what the current symbol looks like.  Put back
                 *   the token and proceed.  
                 */
                G_tok->unget();
                break;

            case TOKT_COLON:
                /* 
                 *   "symbol: " - a nested object definition, or what's
                 *   meant to be a new object definition, with the
                 *   terminating semicolon or brace of the current object
                 *   missing.  Assume for now it's correct, in which case
                 *   it's a nested object definition; put back the token and
                 *   proceed.  
                 */
                G_tok->unget();
                break;

            case TOKT_SYM:
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
                {
                    CTcSymbol *sym;
                    
                    /* 
                     *   Two symbols in a row, or a symbol followed by
                     *   what might be a template operator - either an
                     *   equals sign was left out, or this is a new object
                     *   definition.  Put back the token and check what
                     *   kind of symbol we had in the first place.  
                     */
                    G_tok->unget();
                    sym = global_symtab_
                          ->find(G_tok->getcur()->get_text(),
                                 G_tok->getcur()->get_text_len());

                    /* 
                     *   if it's an object symbol, we almost certainly
                     *   have a new object definition 
                     */
                    if (sym != 0 && sym->get_type() == TC_SYM_OBJ)
                    {
                        /* log the error */
                        G_tok->log_error_curtok(braces || propset_depth != 0
                                                ? TCERR_OBJ_DEF_REQ_RBRACE
                                                : TCERR_OBJ_DEF_REQ_SEM);

                        /* assume the object is finished */
                        done = TRUE;
                    }
                }
                break;

            default:
                /* 
                 *   anything else is probably an error, but it's hard to
                 *   guess what the error is; proceed on the assumption
                 *   that it's going to be a property definition, and let
                 *   the property definition parser take care of analyzing
                 *   the problem 
                 */
                G_tok->unget();
                break;
            }

            /* 
             *   if we decided that the object is finished, cancel the
             *   property definition parsing 
             */
            if (done)
                break;

            /* 
             *   initialize my termination information object with the
             *   current location (note that we don't necessarily initialize
             *   the current termination info, since only the outermost one
             *   matters; if ours happens to be active, it's because it's
             *   the outermost) 
             */
            my_term_info.init(G_tok->get_last_desc(),
                              G_tok->get_last_linenum());

            /* go parse the property definition */
            parse_obj_prop(err, obj_stm, FALSE, meta_sym, term_info,
                           propset_stack, propset_depth, is_nested);

            /* if we ran into an unrecoverable error, give up */
            if (*err)
            {
                /* restore the 'self' validity, and return failure */
                self_valid_ = old_self_valid;
                return 0;
            }

            /* 
             *   if we encountered a termination error, assume the current
             *   object was intended to be terminated, so stop here 
             */
            if (term_info->unterm_)
            {
                /* 
                 *   we found what looks like the end of the object within
                 *   the property parser - assume we're done with this
                 *   object 
                 */
                done = TRUE;
            }

            /* done */
            break;

        case TOKT_REPLACE:
            /* 'replace' is only allowed with 'modify' objects */
            if (!modify)
                G_tok->log_error(TCERR_REPLACE_PROP_REQ_MOD_OBJ);
            
            /* skip the 'replace' keyword */
            G_tok->next();

            /* 
             *   initialize my termination information object, in case it's
             *   the active (i.e., outermost) one 
             */
            my_term_info.init(G_tok->get_last_desc(),
                              G_tok->get_last_linenum());

            /* parse the property */
            parse_obj_prop(err, obj_stm, TRUE, meta_sym, term_info,
                           propset_stack, propset_depth, is_nested);

            /* if we ran into an unrecoverable error, give up */
            if (*err)
            {
                /* restore the old 'self' validity, and return failure */
                self_valid_ = old_self_valid;
                return 0;
            }

            /* 
             *   if we encountered a termination error, assume the current
             *   object was intended to be terminated, so stop here 
             */
            if (term_info->unterm_)
            {
                /* 
                 *   we found what looks like the end of the object within
                 *   the property parser - assume we're done with this
                 *   object 
                 */
                done = TRUE;
            }

            /* done */
            break;

        case TOKT_RBRACE:
            /*
             *   If we're in a 'propertyset' definition, this ends the
             *   current propertyset.  If the property list is enclosed in
             *   braces, this brace terminates the property list and the
             *   object body.  Otherwise, it's an error.  
             */
            if (propset_depth != 0)
            {
                /* pop a propertyset level */
                --propset_depth;

                /* 
                 *   skip the brace, and proceed with parsing the rest of
                 *   the object
                 */
                G_tok->next();
            }
            else if (braces)
            {
                /* we're done with the object body */
                done = TRUE;

                /* skip the brace */
                G_tok->next();
            }
            else
            {
                /* 
                 *   this property list isn't enclosed in braces, so this is
                 *   completely unexpected - treat it the same as any other
                 *   invalid token 
                 */
                goto inval_token;
            }
            break;

        case TOKT_EOF:
        default:
        inval_token:
            /* anything else is invalid */
            G_tok->log_error(TCERR_OBJDEF_REQ_PROP,
                             (int)G_tok->getcur()->get_text_len(),
                             G_tok->getcur()->get_text());

            /* skip the errant token and continue */
            G_tok->next();

            /* if we're at EOF, abort */
            if (G_tok->cur() == TOKT_EOF)
            {
                *err = TRUE;
                self_valid_ = old_self_valid;
                return 0;
            }
            break;
        }
    }

    /* 
     *   Finish the object by adding dictionary property placeholders.  Do
     *   not add the placeholders if this is an intrinsic class modifier,
     *   since intrinsic class objects cannot have dictionary associations.  
     */
    if (obj_stm != 0 && meta_sym == 0)
    {
        /* 
         *   Add a placeholder for each dictionary property we didn't
         *   explicitly define for the object.  This will ensure that we
         *   have sufficient property slots at link time if the object
         *   inherits vocabulary from its superclasses.  (The added slots
         *   will be removed at link time if not used at that point, so this
         *   will not affect run-time efficiency or the ultimate size of the
         *   image file.)  
         */
        for (dict_prop = dict_prop_head_ ; dict_prop != 0 ;
             dict_prop = dict_prop->nxt_)
        {
            /* if this one isn't defined, add a placeholder */
            if (!dict_prop->defined_)
            {
                CTcConstVal cval;
                CTcPrsNode *expr;
                
                /* set up a VOCAB_LIST value */
                cval.set_vocab_list();
                expr = new CTPNConst(&cval);

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

/*
 *   Parse an object template instance at the beginning of an object body 
 */
void CTcParser::parse_obj_template(int *err, CTPNStmObject *obj_stm)
{
    size_t cnt;
    CTcObjTemplateInst *p;
    const CTcObjTemplate *tpl;
    int done;
    int undesc_class;
    CTcPrsPropExprSave save_info;
    const CTPNSuperclass *def_sc;

    /* parse the expressions until we reach the end of the template */
    for (cnt = 0, p = template_expr_, done = FALSE ; !done ; ++cnt)
    {
        /* 
         *   remember the statment start location, in case we have a
         *   template element that generates code (such as a double-quoted
         *   string with an embedded expression) 
         */
        cur_desc_ = G_tok->get_last_desc();
        cur_linenum_ = G_tok->get_last_linenum();

        /* 
         *   note the token, so that we can figure out which template we
         *   are using 
         */
        p->def_tok_ = G_tok->cur();

        /* assume this will also be the first token of the value expression */
        p->expr_tok_ = *G_tok->copycur();

        /* we don't have any expression yet */
        p->expr_ = 0;
        p->code_body_ = 0;

        /* prepare to parse a property value expression */
        begin_prop_expr(&save_info);

        /* check to see if this is another template item */
        switch(G_tok->cur())
        {
        case TOKT_SSTR:
            /* single-quoted string - parse just the string */
            p->expr_ = CTcPrsOpUnary::parse_primary();
            break;

        case TOKT_DSTR:
            /* string - parse it */
            p->expr_ = parse_expr_or_dstr(TRUE);
            break;

        case TOKT_DSTR_START:
            /* it's a single-quoted string - parse it */
            p->expr_ = parse_expr_or_dstr(TRUE);

            /* 
             *   treat it as a regular double-quoted string for the
             *   purposes of matching the template 
             */
            p->def_tok_ = TOKT_DSTR;
            break;
            
        case TOKT_LBRACK:
            /* it's a list */
            p->expr_ = CTcPrsOpUnary::parse_list();
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
            /* skip the operator token */
            G_tok->next();

            /* the value expression starts with this token */
            p->expr_tok_ = *G_tok->copycur();

            /* a primary expression must follow */
            p->expr_ = CTcPrsOpUnary::parse_primary();
            break;

        case TOKT_EOF:
            /* end of file - return and let the caller deal with it */
            return;

        default:
            /* anything else ends the template list */
            done = TRUE;

            /* don't count this item after all */
            --cnt;
            break;
        }

        /* 
         *   check for embedded anonymous functions, and wrap the expression
         *   in a code body if necessary 
         */
        p->code_body_ = finish_prop_expr(&save_info, p->expr_, FALSE, 0);

        /* if we wrapped it in a code body, discard the naked expression */
        if (p->code_body_ != 0)
            p->expr_ = 0;

        /* 
         *   move on to the next expression slot if we have room (if we
         *   don't, we won't match anything anyway; just keep writing over
         *   the last slot so that we can at least keep parsing entries) 
         */
        if (cnt + 1 < template_expr_max_)
            ++p;
    }

    /* we have no matching template yet */
    tpl = 0;
    def_sc = 0;

    /* presume we don't have any undescribed classes in our hierarchy */
    undesc_class = FALSE;

    /*
     *   Search for the template, using the normal inheritance rules that we
     *   use at run-time: start with the first superclass and look for a
     *   match; if we find a match, look at subsequent superclasses to look
     *   for one that overrides the match.  
     */
    if (obj_stm != 0)
    {
        /* search our superclasses for a match */
        tpl = find_class_template(obj_stm->get_first_sc(),
                                  template_expr_, cnt, &def_sc,
                                  &undesc_class);

        /* remember the 'undescribed class' status */
        obj_stm->set_undesc_sc(undesc_class);
    }

    /* if we didn't find a match, look for a root object match */
    if (tpl == 0 && !undesc_class)
        tpl = find_template_match(template_head_, template_expr_, cnt);

    /* if we didn't find a match, it's an error */
    if (tpl == 0)
    {
        /*
         *   Note the error, but don't report it yet.  It might be that we
         *   failed to find a template match simply because one of our
         *   superclass names was misspelled.  If that's the case, then the
         *   missing template is the least of our problems, and it's not
         *   worth reporting since it's probably just a side effect of the
         *   missing superclass (that is, once the superclass misspelling is
         *   corrected and the code is re-compiled, we might find that the
         *   template is correct after all, since we'll know which class to
         *   scan for the needed template.)  At code generation time, we'll
         *   be able to resolve the superclasses and find out what's really
         *   going on, so that we can flag the appropriate error.  
         */
        if (obj_stm != 0)
            obj_stm->note_bad_template(TRUE);
        
        /* ignore the template instance */
        return;
    }

    /* if there's no object statement, there's nothing left to do */
    if (obj_stm == 0)
        return;

    /* 
     *   we know we have a matching template, so populate our actual
     *   parameter list with the property identifiers for the matching
     *   template 
     */
    match_template(tpl->items_, template_expr_, cnt);

    /* define the property values according to the template */
    for (p = template_expr_ ; cnt != 0 ; ++p, --cnt)
    {
        /* add this property */
        if (p->expr_ != 0)
            obj_stm->add_prop(p->prop_, p->expr_, FALSE, FALSE);
        else
            obj_stm->add_method(p->prop_, p->code_body_, FALSE);
    }
}

/*
 *   search a class for a template match 
 */
const CTcObjTemplate *CTcParser::
   find_class_template(const CTPNSuperclass *first_sc,
                       CTcObjTemplateInst *src, size_t src_cnt,
                       const CTPNSuperclass **def_sc,
                       int *undesc_class)
{
    const CTPNSuperclass *sc;
    const CTcObjTemplate *tpl;

    /* scan each superclass in the list for a match */
    for (tpl = 0, sc = first_sc ; sc != 0 ; sc = sc->nxt_)
    {
        const CTPNSuperclass *cur_def_sc;
        const CTcObjTemplate *cur_tpl;
        CTcSymObj *sc_sym;

        /* find the symbol for this superclass */
        sc_sym = (CTcSymObj *)get_global_symtab()->find(
            sc->get_sym_txt(), sc->get_sym_len());

        /* if there's no symbol, or it's not a tads-object, give up */
        if (sc_sym == 0
            || sc_sym->get_type() != TC_SYM_OBJ
            || sc_sym->get_metaclass() != TC_META_TADSOBJ)
        {
            /* 
             *   this class has an invalid superclass - just give up without
             *   issuing any errors now, since we'll have plenty to say
             *   about this when building the object file data 
             */
            return 0;
        }

        /* find a match in this superclass hierarchy */
        cur_tpl = find_template_match(sc_sym->get_first_template(),
                                      src, src_cnt);

        /* see what we found */
        if (cur_tpl != 0)
        {
            /* we found it - note the current defining superclass */
            cur_def_sc = sc;
        }
        else
        {
            /* 
             *   If this one has no superclass list, and it's not explicitly
             *   a subclass of the root class, then this is an undescribed
             *   class and cannot be used with templates at all.  A class is
             *   undescribed when it is explicitly declared as 'extern', and
             *   does not have a definition in any imported symbol file in
             *   the current compilation.  If this is the case, flag it so
             *   the caller will know we have an undescribed class.
             *   
             *   Note that we only set this flag if we failed to find a
             *   template.  A template can still be used if a matching
             *   template is explicitly defined on the class in this
             *   compilation unit, since in that case we don't need to look
             *   up the inheritance hierarchy for the class.  That's why we
             *   set the flag here, only after we have failed to find a
             *   template for the object.  
             */
            if (sc_sym->get_sc_name_head() == 0 && !sc_sym->sc_is_root())
            {
                /* tell the caller we have an undescribed class */
                *undesc_class = TRUE;

                /* 
                 *   there's no need to look any further, since any matches
                 *   we might find among our other superclasses would be in
                 *   doubt because of the lack of information about this
                 *   earlier class, which might override later superclasses
                 *   if we knew more about it 
                 */
                return 0;
            }

            /* we didn't find it - search superclasses of this class */
            cur_tpl = find_class_template(sc_sym->get_sc_name_head(),
                                          src, src_cnt, &cur_def_sc,
                                          undesc_class);

            /* 
             *   if we have an undescribed class among our superclasses,
             *   we're implicitly undescribed as well - if that's the case,
             *   there's no need to look any further, so return failure 
             */
            if (*undesc_class)
                return 0;
        }

        /* if we found a match, see if we want to keep it */
        if (cur_tpl != 0)
        {
            /* 
             *   if this is our first match, note it; if it's not, see if it
             *   overrides the previous match 
             */
            if (tpl == 0)
            {
                /* this is the first match - definitely keep it */
                tpl = cur_tpl;
                *def_sc = cur_def_sc;
            }
            else
            {
                /* 
                 *   if the current source object descends from the previous
                 *   source object, this definition overrides the previous
                 *   definition, so keep it rather than the last one 
                 */
                if (cur_def_sc->is_subclass_of(*def_sc))
                {
                    /* it overrides the previous one - keep the new one */
                    tpl = cur_tpl;
                    *def_sc = cur_def_sc;
                }
            }
        }
    }

    /* return the best match we found */
    return tpl;
}

/*
 *   Find a matching template in the given template list 
 */
const CTcObjTemplate *CTcParser::
   find_template_match(const CTcObjTemplate *first_tpl,
                       CTcObjTemplateInst *src, size_t src_cnt)
{
    const CTcObjTemplate *tpl;

    /* find the matching template */
    for (tpl = first_tpl ; tpl != 0 ; tpl = tpl->nxt_)
    {
        /* check for a match */
        if (match_template(tpl->items_, src, src_cnt))
        {
            /* it's a match - return this template */
            return tpl;
        }
    }

    /* we didn't find a match */
    return 0;
}

/*
 *   Match a template to an actual template parameter list.  
 */
int CTcParser::match_template(const CTcObjTemplateItem *tpl_head,
                              CTcObjTemplateInst *src, size_t src_cnt)
{
    CTcObjTemplateInst *p;
    const CTcObjTemplateItem *item;
    size_t rem;

    /* check each element of the list */
    for (p = src, rem = src_cnt, item = tpl_head ; item != 0 && rem != 0 ;
         item = item->nxt_)
    {
        int match;
        int is_opt;

        /* 
         *   Note whether or not this item is optional.  Every element of an
         *   alternative group must have the same optional status, so we need
         *   only note the status of the first item if this is a group. 
         */
        is_opt = item->is_opt_;

        /* 
         *   Scan each alternative in the current group.  Note that if we're
         *   not in an alternative group, the logic is the same: we won't
         *   have any 'alt' flags, so we'll just scan a single item. 
         */
        for (match = FALSE ; ; item = item->nxt_)
        {
            /* if this one matches, note the match */
            if (item->tok_type_ == p->def_tok_)
            {
                /* note the match */
                match = TRUE;
                
                /* this is the property to assign for the actual */
                p->prop_ = item->prop_;
            }

            /* 
             *   If this one is not marked as an alternative, we're done.
             *   The last item of an alternative group is identified by
             *   having its 'alt' flag cleared.  Also, if we somehow have an
             *   ill-formed list, where we don't have a terminating
             *   non-flagged item, we can stop now as well.  
             */
            if (!item->is_alt_ || item->nxt_ == 0)
                break;
        }

        /* check to see if the current item is optional */
        if (is_opt)
        {
            /* 
             *   The item is optional.  If it matches, try it both ways:
             *   first try matching the item, then try skipping it.  If we
             *   can match this item and still match the rest of the string,
             *   take that interpretation; otherwise, if we can skip this
             *   item and match the rest of the string, take *that*
             *   interpretation.  If we can't match it either way, we don't
             *   have a match.  
             *   
             *   First, check to see if we can match the item and still match
             *   the rest of the string.  
             */
            if (match && match_template(item->nxt_, p + 1, rem - 1))
            {
                /* we have a match */
                return TRUE;
            }

            /* 
             *   Matching this optional item doesn't let us match the rest of
             *   the string, so try it with this optional item omitted - in
             *   other words, just match the rest of the string, including
             *   the current source item, to the rest of the template,
             *   *excluding* the current optional item.
             *   
             *   There's no need to recurse to do this; simply continue
             *   iterating, but do NOT skip the current source item.  
             */
        }
        else
        {
            /* 
             *   It's not optional, so if it doesn't match, the whole
             *   template fails to match; if it does match, simply proceed
             *   through the rest of the template.  
             */
            if (!match)
                return FALSE;

            /* we matched, so consume this source item */
            ++p;
            --rem;
        }
    }

    /* skip any trailing optional items in the template list */
    while (item != 0 && item->is_opt_)
        item = item->nxt_;
    
    /* 
     *   it's a match if and only if we reached the end of both lists at the
     *   same time 
     */
    return (item == 0 && rem == 0);
}


/*
 *   Parse a property definition within an object definition.
 *   
 *   'obj_is_nested' indicates that the enclosing object is a nested object.
 *   This tells us that we can't allow the lexicalParent property to be
 *   manually defined, since we will automatically define it explicitly for
 *   the enclosing object by virtue of its being nested.  
 */
void CTcParser::parse_obj_prop(int *err, CTPNStmObject *obj_stm,
                               int replace, CTcSymMetaclass *meta_sym,
                               tcprs_term_info *term_info,
                               propset_def *propset_stack, int propset_depth,
                               int obj_is_nested)
{
    CTcToken prop_tok;
    CTPNCodeBody *code_body;
    CTcPrsNode *expr;
    int argc;
    int varargs;
    int varargs_list;
    CTcSymLocal *varargs_list_local;
    int has_retval;
    CTcSymProp *prop_sym;
    CTPNObjProp *obj_prop;
    int is_static;
    CTcSymbol *sym;

    /* presume it's not a static property definition */
    is_static = FALSE;
    
    /* remember the property token */
    prop_tok = *G_tok->copycur();

    /* 
     *   if we're in a property set, apply the property set pattern to the
     *   property name 
     */
    if (propset_depth != 0)
    {
        char expbuf[TOK_SYM_MAX_LEN];
        size_t explen;
        propset_def *cur;
        int i;
        const char *newstr;
        
        /*
         *   Build the real property name based on all enclosing propertyset
         *   pattern strings.  Start with the current (innermost) pattern,
         *   then apply the next pattern out, and so on.
         *   
         *   Note that if the current nesting depth is greater than the
         *   maximum nesting depth, ignore the illegally deep levels and
         *   just start with the deepest legal level.  
         */
        i = propset_depth;
        if (i > MAX_PROPSET_DEPTH)
            i = MAX_PROPSET_DEPTH;
        
        /* start with the current token */
        explen = G_tok->getcur()->get_text_len();
        memcpy(expbuf, G_tok->getcur()->get_text(), explen);
        
        /* iterate through the propertyset stack */
        for (cur = &propset_stack[i-1] ; i > 0 ; --i, --cur)
        {
            char tmpbuf[TOK_SYM_MAX_LEN];
            utf8_ptr src;
            utf8_ptr dst;
            size_t rem;

            /* if we'd exceed the maximum token length, stop here */
            if (explen + cur->prop_pattern_len > TOK_SYM_MAX_LEN)
            {
                /* complain about it */
                G_tok->log_error(TCERR_PROPSET_TOK_TOO_LONG);
                
                /* stop the expansion */
                break;
            }
            
            /* copy the pattern up to the '*' */
            for (src.set((char *)cur->prop_pattern),
                 dst.set(tmpbuf), rem = cur->prop_pattern_len ;
                 rem != 0 && src.getch() != '*' ; src.inc(&rem))
            {
                /* copy this character */
                dst.setch(src.getch());
            }
            
            /* if we found a '*', skip it */
            if (rem != 0 && src.getch() == '*')
                src.inc(&rem);
            
            /* insert the expansion from the last round here */
            memcpy(dst.getptr(), expbuf, explen);
            
            /* advance our output pointer */
            dst.set(dst.getptr() + explen);
            
            /* copy the remainder of the pattern string */
            if (rem != 0)
            {
                /* copy the remaining bytes */
                memcpy(dst.getptr(), src.getptr(), rem);

                /* advance the output pointer past the copied bytes */
                dst.set(dst.getptr() + rem);
            }
            
            /* copy the result back to the expansion buffer */
            explen = dst.getptr() - tmpbuf;
            memcpy(expbuf, tmpbuf, explen);
        }

        /* store the new token in tokenizer memory */
        newstr = G_tok->store_source(expbuf, explen);

        /* set the new text in the property token */
        prop_tok.set_text(newstr, explen);
    }

    /* presume we won't find a valid symbol for the property token */
    prop_sym = 0;

    /* 
     *   look up the property token as a symbol, to see if it's already
     *   defined - we don't want to presume it's a property quite yet, but
     *   we can at least look to see if it's defined as something else 
     */
    sym = global_symtab_->find(prop_tok.get_text(),
                               prop_tok.get_text_len());

    /* make sure that the object doesn't already define this propery */
    if (sym != 0)
    {
        int is_dup;

        /* presume it's not a duplicate */
        is_dup = FALSE;
        
        /* 
         *   if the enclosing object is nested, and this is the
         *   lexicalParent property, then it's defined automatically by the
         *   compiler and thus can't be redefined explicitly
         */
        if (obj_is_nested && sym == lexical_parent_sym_)
            is_dup = TRUE;

        /* if we didn't already find a duplicate, scan the existing list */
        if (!is_dup)
        {
            /* scan the property list for this object */
            for (obj_prop = obj_stm->get_first_prop() ; obj_prop != 0 ;
                 obj_prop = obj_prop->get_next_prop())
            {
                /* if it matches, log an error */
                if (obj_prop->get_prop_sym() == sym)
                {
                    /* 
                     *   if this property is overwritable, we're allowed to
                     *   replace it with a new value; otherwise, it's an
                     *   illegal duplicate 
                     */
                    if (obj_prop->is_overwritable())
                    {
                        /* 
                         *   we're allowed to overwrite it with an explicit
                         *   redefinition; simply remove the old property
                         *   from the list so we can add the new one 
                         */
                        obj_stm->delete_property(obj_prop->get_prop_sym());
                    }
                    else
                    {
                        /* note that we found a duplicate property */
                        is_dup = TRUE;
                    }
                        
                    /* no need to continue looking */
                    break;
                }
            }
        }

        /* if we found a duplicate, log an error */
        if (is_dup)
            G_tok->log_error(TCERR_PROP_REDEF_IN_OBJ,
                             (int)sym->get_sym_len(), sym->get_sym());
    }

    /* skip it and see what comes next */
    switch (G_tok->next())
    {
    case TOKT_LPAR:
    case TOKT_LBRACE:
    parse_method:
        /* look up the symbol */
        prop_sym = look_up_prop(&prop_tok, TRUE);

        /* vocabulary properties can't be code */
        if (prop_sym != 0 && prop_sym->is_vocab())
            G_tok->log_error_curtok(TCERR_VOCAB_REQ_SSTR);

        /* 
         *   a left paren starts a formal parameter list, a left brace
         *   starts a code body - in either case, we have a method, so
         *   parse the code body 
         */
        code_body = parse_code_body(FALSE, TRUE, TRUE,
                                    &argc, &varargs,
                                    &varargs_list, &varargs_list_local,
                                    &has_retval, err, 0, TCPRS_CB_NORMAL,
                                    propset_stack, propset_depth, 0, 0);
        if (*err)
            return;

        /* add the method definition */
        if (prop_sym != 0)
            obj_stm->add_method(prop_sym, code_body, replace);
        break;

    case TOKT_SEM:
    case TOKT_RBRACE:
    case TOKT_RPAR:
    case TOKT_RBRACK:
    case TOKT_COMMA:
        /* log the error */
        G_tok->log_error(TCERR_OBJDEF_REQ_PROPVAL,
                         (int)prop_tok.get_text_len(), prop_tok.get_text(),
                         (int)G_tok->getcur()->get_text_len(),
                         G_tok->getcur()->get_text());

        /* if it's anything other than a semicolon, skip it */
        if (G_tok->cur() != TOKT_SEM)
            G_tok->next();
        break;

    case TOKT_COLON:
        /*
         *   It's a nested object definition.  The value of the property is
         *   a reference to an anonymous object that we now define in-line.
         *   The in-line object definition is pretty much normal: following
         *   the colon is the superclass list, then the property list.  The
         *   property list must be enclosed in braces, though.
         *   
         *   Note that we hold off defining the property symbol for as long
         *   as possible, since we want to make sure that this isn't
         *   actually meant to be a new object definition (following an
         *   object not properly terminated) rather than a nested object.  
         */
        {
            CTcConstVal cval;
            CTPNStmObject *nested_obj_stm;
            
            /* 
             *   If the symbol we're taking to be a property name is already
             *   known as an object name, we must have mistaken a new object
             *   definition for a nested object definition - if this is the
             *   case, flag the termination error and proceed to parse the
             *   new object definition.  
             */
            if (sym != 0 && sym->get_type() == TC_SYM_OBJ)
            {
                /* 
                 *   flag the unterminated object error - note that the
                 *   location where termination should have occurred is
                 *   where the caller tells us 
                 */
                G_tcmain->log_error(term_info->desc_, term_info->linenum_,
                                    TC_SEV_ERROR, TCERR_UNTERM_OBJ_DEF);

                /* unget the colon so we can parse the new object def */
                G_tok->unget();

                /* flag the termination error for the caller */
                term_info->unterm_ = TRUE;

                /* we're done */
                return;
            }

            /* 
             *   skip the colon, since the object body parser requires us to
             *   advance to the class list before parsing any anonymous
             *   object body 
             */
            G_tok->next();
            
            /* parse the body of the nested object */
            nested_obj_stm = parse_anon_object(err, 0, TRUE,
                                               term_info, FALSE);

            /* 
             *   If we encountered a termination error, assume the error is
             *   actually in the enclosing object, and that this wasn't
             *   meant to be a nested object after all.  Do not define the
             *   symbol we took as a property to be a property, since it
             *   might well have been meant as an object name instead.  If a
             *   termination error did occur, simply return, so that the
             *   caller can handle the assumed termination.  
             */
            if (term_info->unterm_)
                return;

            /* make sure we didn't encounter an error */
            if (*err != 0 )
                return;
            
            /* make sure we created a valid parse tree for the object */
            if (nested_obj_stm == 0)
            {
                *err = TRUE;
                return;
            }
            
            /* the value we wish to assign is a reference to this object */
            cval.set_obj(nested_obj_stm->get_obj_sym()->get_obj_id(),
                         nested_obj_stm->get_obj_sym()->get_metaclass());
            expr = new CTPNConst(&cval);

            /* 
             *   look up the property symbol - we can finally look it up
             *   with reasonable confidence, since it appears we do indeed
             *   have a valid nested object definition, hence we can stop
             *   waiting to see if something goes wrong 
             */
            prop_sym = look_up_prop(&prop_tok, TRUE);

            /* if it's a vocabulary property, this type isn't allowed */
            if (prop_sym != 0 && prop_sym->is_vocab())
                G_tok->log_error(TCERR_VOCAB_REQ_SSTR, 1, ":");

            /* if we have a valid property and expression, add it */
            if (prop_sym != 0)
                obj_stm->add_prop(prop_sym, expr, replace, FALSE);

            /*
             *   Define the lexicalParent property in the nested object with
             *   a reference back to the parent object. 
             */
            cval.set_obj(obj_stm->get_obj_sym() != 0
                         ? obj_stm->get_obj_sym()->get_obj_id()
                         : TCTARG_INVALID_OBJ,
                         obj_stm->get_obj_sym() != 0
                         ? obj_stm->get_obj_sym()->get_metaclass()
                         : TC_META_UNKNOWN);
            expr = new CTPNConst(&cval);
            nested_obj_stm->add_prop(lexical_parent_sym_, expr, FALSE, FALSE);

            /* 
             *   add the nested object's parse tree to the queue of nested
             *   top-level statements - since an object is a top-level
             *   statement, but we're not parsing it at the top level, we
             *   need to add it to the pending queue explicitly 
             */
            add_nested_stm(nested_obj_stm);
        }
        
        /* done */
        break;

    case TOKT_EQ:
        /* skip the '=' */
        G_tok->next();

        /* 
         *   if a brace follows, this must be using the obsolete TADS 2
         *   notation, in which an equals sign followed a property name even
         *   if a method was being defined; if this is the case, generate an
         *   error, but then proceed to treat what follows as a method 
         */
        if (G_tok->cur() == TOKT_LBRACE)
        {
            /* log the error... */
            G_tok->log_error(TCERR_EQ_WITH_METHOD_OBSOLETE);

            /* ...but then go ahead and parse it as a method anyway */
            goto parse_method;
        }

        /* go parse the value */
        goto parse_value;

    default:
        /* 
         *   an '=' is required after a value property name; log the error
         *   and proceed, assuming that the '=' was left out but that the
         *   property value is otherwise well-formed 
         */
        G_tok->log_error_curtok(TCERR_PROP_REQ_EQ);

    parse_value:
        /* look up the property symbol */
        prop_sym = look_up_prop(&prop_tok, TRUE);

        /* 
         *   presume the value will be a simple expression that will not
         *   require a code body wrapper 
         */
        code_body = 0;

        /* 
         *   if this is a vocabulary property, perform special vocabulary
         *   list parsing - a vocabulary property can have one or more
         *   single-quoted strings that we store in a list, but the list
         *   does not have to be enclosed in brackets 
         */
        if (prop_sym != 0 && prop_sym->is_vocab()
            && G_tok->cur() == TOKT_SSTR)
        {
            CTcSymObj *obj_sym;
            CTcConstVal cval;

            /* 
             *   set the property to a vocab list placeholder for now in
             *   the object - we'll replace it during linking with the
             *   actual vocabulary list
             */
            cval.set_vocab_list();
            expr = new CTPNConst(&cval);

            /* if I have no dictionary, it's an error */
            if (dict_cur_ == 0)
                G_tok->log_error(TCERR_VOCAB_NO_DICT);

            /* get the object symbol */
            obj_sym = (obj_stm != 0 ? obj_stm->get_obj_sym() : 0);
            
            /* parse the list entries */
            for (;;)
            {
                /* add the word to our vocabulary list */
                if (obj_sym != 0)
                    obj_sym->add_vocab_word(G_tok->getcur()->get_text(),
                                            G_tok->getcur()->get_text_len(),
                                            prop_sym->get_prop());
                                            
                /* 
                 *   skip the string; if another string doesn't
                 *   immediately follow, we're done with the implied list 
                 */
                if (G_tok->next() != TOKT_SSTR)
                    break;
            }
        }
        else
        {
            CTcPrsPropExprSave save_info;

            /* if it's a vocabulary property, other types aren't allowed */
            if (prop_sym != 0 && prop_sym->is_vocab())
                G_tok->log_error_curtok(TCERR_VOCAB_REQ_SSTR);

            /* check for the 'static' keyword */
            if (G_tok->cur() == TOKT_STATIC)
            {
                /* note that it's static */
                is_static = TRUE;

                /* skip the 'static' keyword */
                G_tok->next();
            }

            /* prepare to parse the property value expression */
            begin_prop_expr(&save_info);

            /* 
             *   parse the expression (which can be a double-quoted string
             *   instead of a value expression) 
             */
            expr = parse_expr_or_dstr(TRUE);
            if (expr == 0)
            {
                *err = TRUE;
                return;
            }

            /* 
             *   check for embedded anonymous functions and wrap the
             *   expression in a code body if necessary; if we do wrap it,
             *   discard the expression, since the code body contains the
             *   value now 
             */
            code_body = finish_prop_expr(&save_info, expr,
                                         is_static, prop_sym);
            if (code_body != 0)
                expr = 0;
        }

        /* if we have a valid property and expression, add it */
        if (prop_sym != 0)
        {
            /* add the expression or code body, as appropriate */
            if (expr != 0)
                obj_stm->add_prop(prop_sym, expr, replace, is_static);
            else if (code_body != 0)
                obj_stm->add_method(prop_sym, code_body, replace);
        }

        /* done with the property */
        break;
    }

    /* 
     *   if we're modifying an intrinsic class, make sure the property isn't
     *   defined in the class's native interface 
     */
    if (meta_sym != 0 && prop_sym != 0)
    {
        CTcSymMetaclass *cur_meta;
        
        /* check this intrinsic class and all of its superclasses */
        for (cur_meta = meta_sym ; cur_meta != 0 ;
             cur_meta = cur_meta->get_super_meta())
        {
            CTcSymMetaProp *mprop;

            /* scan the list of native methods */
            for (mprop = cur_meta->get_prop_head() ; mprop != 0 ;
                 mprop = mprop->nxt_)
            {
                /* if it matches, flag an error */
                if (mprop->prop_ == prop_sym)
                {
                    /* log the error */
                    G_tok->log_error(TCERR_CANNOT_MOD_META_PROP,
                                     prop_sym->get_sym_len(),
                                     prop_sym->get_sym());
                    
                    /* no need to look any further */
                    break;
                }
            }
        }
    }

    /* 
     *   if this is a dictionary property, note that it's been defined for
     *   this object 
     */
    if (prop_sym != 0 && prop_sym->is_vocab())
    {
        CTcDictPropEntry *entry;

        /* scan the list of dictionary properties for this one */
        for (entry = dict_prop_head_ ; entry != 0 ; entry = entry->nxt_)
        {
            /* check this one for a match */
            if (entry->prop_ == prop_sym)
            {
                /* mark this entry as defined for this object */
                entry->defined_ = TRUE;

                /* no need to look any further */
                break;
            }
        }
    }
}

/*
 *   Begin a property expression 
 */
void CTcParser::begin_prop_expr(CTcPrsPropExprSave *save_info)
{
    /* save the current parser state */
    save_info->has_local_ctx_ = has_local_ctx_;
    save_info->local_ctx_var_num_ = local_ctx_var_num_;
    save_info->ctx_var_props_used_ = ctx_var_props_used_;
    save_info->next_ctx_arr_idx_ = next_ctx_arr_idx_;
    save_info->self_referenced_ = self_referenced_;
    save_info->cur_code_body_ = cur_code_body_;
    save_info->full_method_ctx_referenced_ = full_method_ctx_referenced_;
    save_info->local_ctx_needs_self_ = local_ctx_needs_self_;
    save_info->local_ctx_needs_full_method_ctx_ =
        local_ctx_needs_full_method_ctx_;

    /* 
     *   we've saved the local context information, so clear it out for the
     *   next parse job 
     */
    clear_local_ctx();

    /* there's no current code body */
    cur_code_body_ = 0;

    /* this is parsed in a global symbol table context */
    local_symtab_ = global_symtab_;
}

/*
 *   Finish a property expression, checking for anonymous functions and
 *   wrapping the expression in a code body if necesssary.  If the
 *   expression contains an anonymous function which needs to share context
 *   with its enclosing scope, we need to build the code body wrapper
 *   immediately so that we can capture the context information, which is
 *   stored in the parser object itself (i.e,.  'this').  
 */
CTPNCodeBody *CTcParser::finish_prop_expr(CTcPrsPropExprSave *save_info,
                                          CTcPrsNode *expr,
                                          int is_static,
                                          CTcSymProp *prop_sym)
{
    CTPNStm *stm;
    CTPNCodeBody *cb;

    /* presume we won't need to create a code body */
    cb = 0;

    /* 
     *   make sure that we have an expression and a local context - if
     *   there's no expression at all, or we don't have a shared context,
     *   there's nothing extra we need to do - we can return the expression
     *   as it is 
     */
    if (expr != 0 && has_local_ctx_)
    {
        /* create a return statement returning the value of the expression */
        if (is_static)
        {
            /* 
             *   it's a static initializer - wrap it in a static initializer
             *   statement node 
             */
            stm = new CTPNStmStaticPropInit(expr, prop_sym->get_prop());
        }
        else if (expr->has_return_value())
        {
            /* normal property value - wrap it in a 'return' */
            stm = new CTPNStmReturn(expr);
        }
        else
        {
            /* 
             *   it's an expression that yields no value, such as a
             *   double-quoted string expression or a call to a void
             *   function; just use the expression itself 
             */
            stm = new CTPNStmExpr(expr);
        }
        
        /* 
         *   Create a code body to wrap the expression.  Note that we have no
         *   lexically enclosing code body for a property expression.  
         */
        cb = new CTPNCodeBody(G_prs->get_global_symtab(), 0, stm,
                              0, FALSE, FALSE, 0, local_cnt_,
                              self_valid_, 0);
        
        /* set the local context information in the code body */
        cb->set_local_ctx(local_ctx_var_num_, next_ctx_arr_idx_ - 1);

        /* mark the code body for references to the method context */
        cb->set_self_referenced(self_referenced_);
        cb->set_full_method_ctx_referenced(full_method_ctx_referenced_);

        /* mark the code body for inclusion in any local context */
        cb->set_local_ctx_needs_self(local_ctx_needs_self_);
        cb->set_local_ctx_needs_full_method_ctx(
            local_ctx_needs_full_method_ctx_);
    }
        
    /* restore the saved parser state */
    has_local_ctx_ = save_info->has_local_ctx_;
    local_ctx_var_num_ = save_info->local_ctx_var_num_;
    ctx_var_props_used_ = save_info->ctx_var_props_used_;
    next_ctx_arr_idx_ = save_info->next_ctx_arr_idx_;
    cur_code_body_ = save_info->cur_code_body_;
    self_referenced_ = save_info->self_referenced_;
    full_method_ctx_referenced_ = save_info->full_method_ctx_referenced_;
    local_ctx_needs_self_ = save_info->local_ctx_needs_self_;
    local_ctx_needs_full_method_ctx_ =
        save_info->local_ctx_needs_full_method_ctx_;

    /* return the code body, if we created one */
    return cb;
}

/*
 *   Clear the local anonymous function shared context information 
 */
void CTcParser::clear_local_ctx()
{
    /* we don't have a local context */
    has_local_ctx_ = FALSE;

    /* there is no local context local yet */
    local_ctx_var_num_ = 0;

    /* no variable properties are used */
    ctx_var_props_used_ = 0;

    /* 
     *   start the context array index at the next entry after the slot we
     *   always use to store the method context of the enclosing lexical
     *   scope 
     */
    next_ctx_arr_idx_ = TCPRS_LOCAL_CTX_METHODCTX + 1;

    /* 
     *   We haven't yet referenced any method context variables.
     *   
     *   However, even if we don't detect a reference to 'self', mark 'self'
     *   as referenced anyway.  We err on the side of safety here: there are
     *   so many ways that 'self' can be implicitly referenced that it seems
     *   safer to assume it's needed in all cases.  Ideally, we wouldn't be
     *   so cautious, but the cost of assuming that 'self' is referenced in
     *   all code bodies is (for the moment, at least) relatively small, in
     *   that there are relatively few optimizations we can perform based on
     *   this information.  
     */
    self_referenced_ = TRUE;
    full_method_ctx_referenced_ = FALSE;

    /* we don't yet need the method context in the local context */
    local_ctx_needs_self_ = FALSE;
    local_ctx_needs_full_method_ctx_ = FALSE;
}

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
    if (G_tok->cur() == TOKT_FUNCTION)
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


/*
 *   Parse a formal parameter list.  If 'count_only' is true, we're only
 *   interested in counting the formals, and we don't bother adding them
 *   to any symbol table.  If 'opt_allowed' is true, a parameter name can
 *   be followed by a question mark token to mark the parameter as
 *   optional.
 *   
 *   '*argc' returns with the number of parameters.  'opt_argc' can be
 *   null; if it's not null, '*opt_argc' returns with the number of
 *   parameters marked as optional.
 *   
 *   'base_formal_num' is the starting formal parameter number to use in
 *   creating symbol table entries.  This is meaningful only if
 *   'count_only' is false.
 *   
 *   The caller must already have checked for an open paren and skipped
 *   it.  
 */
void CTcParser::parse_formal_list(int count_only, int opt_allowed,
                                  int *argc, int *opt_argc, int *varargs,
                                  int *varargs_list,
                                  CTcSymLocal **varargs_list_local,
                                  int *err, int base_formal_num,
                                  int for_short_anon_func,
                                  CTcFormalTypeList **type_list)
{
    int done;
    int missing_end_tok_err;
    tc_toktyp_t end_tok;

    /* 
     *   choose the end token - if this is a normal argument list, the
     *   ending token is a right parenthesis; for a short-form anonymous
     *   function, it's a colon 
     */
    end_tok = (for_short_anon_func ? TOKT_COLON : TOKT_RPAR);
    missing_end_tok_err = (for_short_anon_func
                           ? TCERR_MISSING_COLON_FORMAL
                           : TCERR_MISSING_RPAR_FORMAL);
    
    /* no arguments yet */
    *argc = 0;
    if (opt_argc != 0)
        *opt_argc = 0;
    *varargs = FALSE;
    *varargs_list = FALSE;

    /* no error yet */
    *err = FALSE;

    /* we've only just begun */
    done = FALSE;

    /* check for an empty list */
    if (G_tok->cur() == end_tok)
    {
        /* the list is empty - we're already done */
        done = TRUE;

        /* skip the closing token */
        G_tok->next();
    }

    /* keep going until done */
    while (!done)
    {
        /* see what comes next */
        switch(G_tok->cur())
        {
        case TOKT_ELLIPSIS:
            /* it's an ellipsis - note that we have varargs */
            *varargs = TRUE;

        parse_ellipsis:
            /* add the varargs indicator to the formal type list */
            if (type_list != 0 && *type_list != 0)
                (*type_list)->add_ellipsis();

            /* the next token must be the close paren */
            if (G_tok->next() == end_tok)
            {
                /* we've reached the end of the list */
                goto handle_end_tok;
            }
            else
            {
                /* this is an error - guess about the problem */
                switch(G_tok->cur())
                {
                case TOKT_COMMA:
                    /* 
                     *   we seem to have more in the list - log an error,
                     *   but continue parsing the list 
                     */
                    G_tok->next();
                    G_tok->log_error(TCERR_ELLIPSIS_NOT_LAST);
                    break;
                    
                default:
                    /* 
                     *   anything else is probably a missing right paren -
                     *   provide it and exit the formal list 
                     */
                    done = TRUE;
                    G_tok->log_error_curtok(missing_end_tok_err);
                    break;
                }
            }
            break;
            
        case TOKT_LBRACK:
            /* 
             *   varargs with named list variable for last parameter -
             *   this generates setup code that stores the arguments from
             *   this one forward in a list to be stored in this variable 
             */

            /* note that we have varargs */
            *varargs = TRUE;

            /* note it in the type list as well */
            if (type_list != 0 && *type_list != 0)
                (*type_list)->add_ellipsis();

            /* skip the bracket and check that a symbol follows */
            switch(G_tok->next())
            {
            case TOKT_RBRACK:
                /* 
                 *   empty brackets - treat this as identical to an
                 *   ellipsis; since they didn't name the varargs list
                 *   parameter, it's just a varargs indication 
                 */
                goto parse_ellipsis;
                
            case TOKT_SYM:
                /* if we're creating a symbol table, add the symbol */
                if (!count_only)
                {
                    int local_id;
                    
                    /* create a local scope if we haven't already */
                    create_scope_local_symtab();

                    /* note that we have a varargs list parameter */
                    *varargs_list = TRUE;

                    /* 
                     *   insert the new variable as a local - it's not a
                     *   formal, since we're going to take the actuals,
                     *   make a list, and store them in this local; the
                     *   formal in this position will not be named, since
                     *   this is a varargs function 
                     */
                    local_id = alloc_local();
                    *varargs_list_local = local_symtab_->add_local(
                        G_tok->getcur()->get_text(),
                        G_tok->getcur()->get_text_len(),
                        local_id, FALSE, TRUE, FALSE);

                    /* mark it as a list parameter */
                    (*varargs_list_local)->set_list_param(TRUE);
                }

                /* a close bracket must follow */
                switch (G_tok->next())
                {
                case TOKT_RBRACK:
                    /* skip the close bracket, and check for the end token */
                    if (G_tok->next() == end_tok)
                    {
                        /* that's it - the list is done */
                        goto handle_end_tok;
                    }

                    /* we didn't find the end token - guess about the error */
                    switch (G_tok->cur())
                    {
                    case TOKT_COMMA:
                        /* 
                         *   they seem to want another parameter - this is
                         *   an error, but keep parsing anyway 
                         */
                        G_tok->log_error(TCERR_LISTPAR_NOT_LAST);
                        break;

                    case TOKT_LBRACE:
                    case TOKT_RBRACE:
                    case TOKT_LPAR:
                    case TOKT_EOF:
                        /* the ending token was probably just forgotten */
                        G_tok->log_error_curtok(missing_end_tok_err);

                        /* presume that the argument list is done */
                        done = TRUE;
                        break;

                    default:
                        /* end token is missing - log an error */
                        G_tok->log_error_curtok(missing_end_tok_err);

                        /* 
                         *   skip the errant token; if the ending token
                         *   follows this one, skip it as well, since
                         *   there's probably just a stray extra token
                         *   before the ending token 
                         */
                        if (G_tok->next() == end_tok)
                            G_tok->next();

                        /* presume they simply left out the paren */
                        done = TRUE;
                        break;
                    }
                    break;

                case TOKT_LBRACE:
                case TOKT_RBRACE:
                case TOKT_EOF:
                    /* they must have left out the bracket and paren */
                    G_tok->log_error_curtok(TCERR_LISTPAR_REQ_RBRACK);
                    done = TRUE;
                    break;
                    
                default:
                    /* note the missing bracket */
                    G_tok->log_error_curtok(TCERR_LISTPAR_REQ_RBRACK);

                    /* skip the errant token and assume we're done */
                    G_tok->next();
                    done = TRUE;
                    break;
                }
                break;

            default:
                /* log an error */
                G_tok->log_error_curtok(TCERR_LISTPAR_REQ_SYM);

                /* if this is the ending token, we're done */
                if (G_tok->cur() == end_tok)
                    goto handle_end_tok;

                /* 
                 *   if we're not on something that looks like the end of
                 *   the parameter list, skip the errant token 
                 */
                switch(G_tok->cur())
                {
                case TOKT_RBRACE:
                case TOKT_LBRACE:
                case TOKT_EOF:
                    /* looks like they left out the closing markers */
                    done = TRUE;
                    break;

                default:
                    /* 
                     *   skip the errant token and continue - they
                     *   probably have more argument list following 
                     */
                    G_tok->next();
                    break;
                }
                break;
            }
            break;

        case TOKT_SYM:
            /*
             *   If a formal type list is allowed, check to see if the next
             *   token is a symbol - if it is, the current token is the type,
             *   and the next token is the variable name. 
             */
            if (type_list != 0)
            {
                /* save the initial token */
                CTcToken init_tok = *G_tok->copycur();

                /* check the next one */
                if (G_tok->next() == TOKT_SYM)
                {
                    /* 
                     *   We have another symbol, so the first token is the
                     *   type name, and the current token is the param name.
                     *   Add the type to the formal type list.
                     */

                    /* first, make sure we've created the type list */
                    if (*type_list == 0)
                    {
                        /* 
                         *   We haven't created one yet - create it now.
                         *   Since we're just getting around to creating the
                         *   list, add untyped elements for the parameters
                         *   we've seen so far - they all must have been
                         *   untyped, since we create the list on
                         *   encountering the first typed parameter.  
                         */
                        *type_list = new (G_prsmem) CTcFormalTypeList();
                        (*type_list)->add_untyped_params(*argc);
                    }

                    /* add this typed parameter */
                    (*type_list)->add_typed_param(&init_tok);
                    
                    /* 
                     *   make sure the type symbol is defined as a class or
                     *   object 
                     */
                    CTcSymbol *clsym = global_symtab_->find(
                        init_tok.get_text(), init_tok.get_text_len());
                    if (clsym == 0)
                    {
                        /* it's not defined - add it as an external object */
                        clsym = new CTcSymObj(
                            init_tok.get_text(), init_tok.get_text_len(),
                            FALSE, G_cg->new_obj_id(), TRUE,
                            TC_META_TADSOBJ, 0);

                        /* mark it as referenced */
                        clsym->mark_referenced();

                        /* add it to the table */
                        global_symtab_->add_entry(clsym);
                    }
                    else if (clsym->get_type() != TC_SYM_OBJ
                             && clsym->get_type() != TC_SYM_METACLASS)
                    {
                        /* it's something other than an object */
                        G_tok->log_error(TCERR_MMPARAM_NOT_OBJECT,
                                         (int)init_tok.get_text_len(),
                                         init_tok.get_text());
                    }
                }
                else
                {
                    /* there's no type name - back up to the param name */
                    G_tok->unget();

                    /* add an untyped variable to the type list */
                    if (*type_list)
                        (*type_list)->add_untyped_param();
                }
            }

            /* if we're creating symbol table entries, add this symbol */
            if (!count_only)
            {
                /* 
                 *   create a new local symbol table if we don't already
                 *   have one 
                 */
                create_scope_local_symtab();

                /* insert the new formal */
                local_symtab_->add_formal(G_tok->getcur()->get_text(),
                                          G_tok->getcur()->get_text_len(),
                                          base_formal_num + *argc, FALSE);
            }
            
            /* count the parameter */
            ++(*argc);

            /* skip the symbol */
            G_tok->next();
            
            /* check for an optional question mark */
            if (opt_allowed && G_tok->cur() == TOKT_QUESTION)
            {
                /* count the optional argument */
                if (opt_argc != 0)
                    ++(*opt_argc);
                
                /* skip the question mark */
                G_tok->next();
            }

            /* check for the closing token */
            if (G_tok->cur() == end_tok)
            {
                /* it's the closing token - skip it and stop scanning */
                goto handle_end_tok;
            }

            /* check what follows */
            switch (G_tok->cur())
            {
            case TOKT_COMMA:
                /* skip the comma and continue */
                G_tok->next();
                break;

            case TOKT_SEM:
            case TOKT_RBRACE:
            case TOKT_LBRACE:
            case TOKT_EQ:
            case TOKT_EOF:
                /* 
                 *   We've obviously left the list - the problem is
                 *   probably that we're missing the right paren.  Catch
                 *   it in the main loop.  
                 */
                break;

            case TOKT_SYM:
                /* 
                 *   they seem to have left out a comma - keep parsing
                 *   from the symbol token 
                 */
                G_tok->log_error_curtok(TCERR_REQ_COMMA_FORMAL);
                break;

            default:
                /* anything else is an error */
                G_tok->log_error_curtok(TCERR_REQ_COMMA_FORMAL);
                
                /* skip the errant token and continue */
                G_tok->next();
                break;
            }

            /* done with the formal */
            break;

        case TOKT_SEM:
        case TOKT_RBRACE:
        case TOKT_LBRACE:
        case TOKT_EOF:
            /* 
             *   We've obviously left the list - the problem is probably
             *   that we're missing the right paren.  Log an error and
             *   stop scanning.  
             */
            G_tok->log_error_curtok(missing_end_tok_err);
            done = TRUE;
            break;

        default:
            /* check to see if it's the ending token */
            if (G_tok->cur() == end_tok)
            {
                /* 
                 *   they seem to have put in a comma followed by the
                 *   ending token - it's probably just a stray extra comma 
                 */
                G_tok->log_error(TCERR_MISSING_LAST_FORMAL);
                
                /* skip the paren and stop scanning */
                G_tok->next();
                done = TRUE;
                break;
            }

            /*
             *   If this is a short-form anonymous function's parameter
             *   list, they probably forgot the colon - generate a more
             *   specific error for this case, and assume the list ends
             *   here.  
             */
            if (for_short_anon_func)
            {
                /* tell them they left out the colon */
                G_tok->log_error_curtok(TCERR_MISSING_COLON_FORMAL);
                
                /* presume the argument list was meant to end here */
                done = TRUE;
                break;
            }
            
            /* 
             *   anything else is probably just an extraneous token; skip
             *   it and go on 
             */
            G_tok->log_error_curtok(TCERR_REQ_SYM_FORMAL);
            G_tok->next();
            break;

        handle_end_tok:
            /* we've reached the end token - skip it, and we're done */
            G_tok->next();
            done = TRUE;
            break;
        }
    }

    /*
     *   Check for the "multimethod" modifier, if allowed.  It's allowed if a
     *   formal type list is allowed and this is a normal full parameter list
     *   (i.e., specified in parentheses rather than as an anonymous function
     *   formal list).  
     */
    if (G_tok->cur() == TOKT_SYM
        && G_tok->cur_tok_matches("multimethod", 11))
    {
        /* make sure it's allowed */
        if (type_list != 0 && end_tok == TOKT_RPAR)
        {
            /* it's allowed - create the type list if we haven't already */
            if (*type_list == 0)
            {
                /* create the type list */
                *type_list = new (G_prsmem) CTcFormalTypeList();

                /* add untyped parameters for the ones we've defined */
                (*type_list)->add_untyped_params(*argc);

                /* mark it as varargs if applicable */
                if (*varargs)
                    (*type_list)->add_ellipsis();
            }
        }
        else
        {
            /* it's not allowed - flag it as an error */
            G_tok->log_error(TCERR_MULTIMETHOD_NOT_ALLOWED);
        }

        /* skip the token */
        G_tok->next();
    }
}

/*
 *   Parse a nested code body (such as an anonymous function code body) 
 */
CTPNCodeBody *CTcParser::
   parse_nested_code_body(int eq_before_brace,
                          int self_valid,
                          int *p_argc, int *p_varargs,
                          int *p_varargs_list,
                          CTcSymLocal **p_varargs_list_local,
                          int *p_has_retval,
                          int *err, CTcPrsSymtab *local_symtab,
                          tcprs_codebodytype cb_type)
{
    CTcPrsSymtab *old_local_symtab;
    CTcPrsSymtab *old_enclosing_local_symtab;
    CTPNStmEnclosing *old_enclosing_stm;
    CTcPrsSymtab *old_goto_symtab;
    int old_local_cnt;
    int old_max_local_cnt;
    CTPNCodeBody *code_body;
    int old_has_local_ctx;
    int old_local_ctx_var_num;
    size_t old_ctx_var_props_used;
    int old_next_ctx_arr_idx;
    int old_self_valid;
    int old_self_referenced;
    int old_full_method_ctx_referenced;
    int old_local_ctx_needs_self;
    int old_local_ctx_needs_full_method_ctx;
    CTcCodeBodyRef *old_cur_code_body;

    /* remember the original parser state */
    old_local_symtab = local_symtab_;
    old_enclosing_local_symtab = enclosing_local_symtab_;
    old_enclosing_stm = enclosing_stm_;
    old_goto_symtab = goto_symtab_;
    old_local_cnt = local_cnt_;
    old_max_local_cnt = max_local_cnt_;
    old_has_local_ctx = has_local_ctx_;
    old_local_ctx_var_num = local_ctx_var_num_;
    old_ctx_var_props_used = ctx_var_props_used_;
    old_next_ctx_arr_idx = next_ctx_arr_idx_;
    old_self_valid = self_valid_;
    old_self_referenced = self_referenced_;
    old_full_method_ctx_referenced = full_method_ctx_referenced_;
    old_local_ctx_needs_self = local_ctx_needs_self_;
    old_local_ctx_needs_full_method_ctx = local_ctx_needs_full_method_ctx_;
    old_cur_code_body = cur_code_body_;

    /* parse the code body */
    code_body = parse_code_body(eq_before_brace, FALSE, self_valid,
                                p_argc, p_varargs,
                                p_varargs_list, p_varargs_list_local,
                                p_has_retval, err, local_symtab, cb_type,
                                0, 0, cur_code_body_, 0);

    /* restore the parser state */
    cur_code_body_ = old_cur_code_body;
    local_symtab_ = old_local_symtab;
    enclosing_local_symtab_ = old_enclosing_local_symtab;
    enclosing_stm_ = old_enclosing_stm;
    goto_symtab_ = old_goto_symtab;
    local_cnt_ = old_local_cnt;
    max_local_cnt_ = old_max_local_cnt;
    has_local_ctx_ = old_has_local_ctx;
    local_ctx_var_num_ = old_local_ctx_var_num;
    ctx_var_props_used_ = old_ctx_var_props_used;
    next_ctx_arr_idx_ = old_next_ctx_arr_idx;
    self_valid_ = old_self_valid;
    self_referenced_ = old_self_referenced;
    full_method_ctx_referenced_= old_full_method_ctx_referenced;
    local_ctx_needs_self_ = old_local_ctx_needs_self;
    local_ctx_needs_full_method_ctx_ = old_local_ctx_needs_full_method_ctx;

    /* return the code body we parsed */
    return code_body;
}

/*
 *   Set up the current code body with a local variable context 
 */
void CTcParser::init_local_ctx()
{
    /* if I already have a local variable context, there's nothing to do */
    if (has_local_ctx_)
        return;

    /* note that we now require a local variable context object */
    has_local_ctx_ = TRUE;

    /* create the local context variable */
    local_ctx_var_num_ = alloc_local();
}

/*
 *   Allocate a context variable index 
 */
int CTcParser::alloc_ctx_arr_idx()
{
    return next_ctx_arr_idx_++;
}

/*
 *   Allocate a context variable property ID 
 */
tctarg_prop_id_t CTcParser::alloc_ctx_var_prop()
{
    CTcSymProp *sym;
    char prop_name[50];
    char *prop_txt;
    tctarg_prop_id_t prop;
    
    /* if possible, return an existing property ID */
    if (ctx_var_props_used_ < ctx_var_props_cnt_)
        return ctx_var_props_[ctx_var_props_used_++];

    /* if we need more room in the array, reallocate the array */
    if (ctx_var_props_cnt_ == ctx_var_props_size_)
    {
        /* reallocate a larger array */
        ctx_var_props_size_ += 50;
        ctx_var_props_ = (tctarg_prop_id_t *)
                         t3realloc(ctx_var_props_,
                                   ctx_var_props_size_
                                   * sizeof(tctarg_prop_id_t));
    }

    /* allocate the new property ID */
    prop = G_cg->new_prop_id();

    /* synthesize a name for the private property */
    sprintf(prop_name, ".anon_var[%d]", (int)ctx_var_props_cnt_);

    /* copy the name to parser memory */
    prop_txt = (char *)G_prsmem->alloc(strlen(prop_name));
    strcpy(prop_txt, prop_name);

    /* allocate a new property symbol and add it to the table */
    sym = new CTcSymProp(prop_txt, strlen(prop_txt), FALSE, prop);
    global_symtab_->add_entry(sym);

    /* mark the property as referenced */
    sym->mark_referenced();

    /* add it to our table */
    ctx_var_props_[ctx_var_props_cnt_++] = prop;

    /* return the property */
    return ctx_var_props_[ctx_var_props_used_++];
}

/* 
 *   enumeration callback context 
 */
struct enum_locals_ctx
{
    /* symbol table containing context locals */
    CTcPrsSymtab *symtab;

    /* code body */
    CTPNCodeBody *code_body;
};

/*
 *   Enumeration callback - find local variables inherited from enclosing
 *   scopes (for anonymous functions) 
 */
void CTcParser::enum_for_ctx_locals(void *ctx0, CTcSymbol *sym)
{
    enum_locals_ctx *ctx = (enum_locals_ctx *)ctx0;

    /* tell this symbol to apply its local variable conversion */
    sym->apply_ctx_var_conv(ctx->symtab, ctx->code_body);
}

/*
 *   Parse a function or method body 
 */
CTPNCodeBody *CTcParser::parse_code_body(int eq_before_brace, int is_obj_prop,
                                         int self_valid,
                                         int *p_argc, int *p_varargs,
                                         int *p_varargs_list,
                                         CTcSymLocal **p_varargs_list_local,
                                         int *p_has_retval,
                                         int *err, CTcPrsSymtab *local_symtab,
                                         tcprs_codebodytype cb_type,
                                         struct propset_def *propset_stack,
                                         int propset_depth,
                                         CTcCodeBodyRef *enclosing_code_body,
                                         CTcFormalTypeList **type_list)
{
    int formal_num;
    int varargs;
    int varargs_list;
    CTcSymLocal *varargs_list_local;
    CTPNStmComp *stm;
    unsigned long flow_flags;
    CTPNCodeBody *body_stm;
    int parsing_anon_fn;

    /* 
     *   create a new code body reference - this will let nested code bodies
     *   refer back to the code body object we're about to parse, even though
     *   we won't create the actual code body object until we're done parsing
     *   the entire code body 
     */
    cur_code_body_ = new (G_prsmem) CTcCodeBodyRef();

    /* note if we're parsing some kind of anonymous function */
    parsing_anon_fn = (cb_type == TCPRS_CB_ANON_FN
                       || cb_type == TCPRS_CB_SHORT_ANON_FN);

    /* remember the 'self' validity */
    self_valid_ = self_valid;

    /* presume we will not need a local variable context object */
    clear_local_ctx();

    /* 
     *   Set the outer local symbol table.  If the caller has provided us
     *   with an explicit pre-constructed local symbol table, use that;
     *   otherwise, use the global symbol table, since we have no locals
     *   of our own yet.  
     */
    local_symtab_ = (local_symtab == 0 ? global_symtab_ : local_symtab);
    enclosing_local_symtab_ = (local_symtab_->get_parent() == 0
                               ? global_symtab_
                               : local_symtab_->get_parent());

    /* there's no enclosing statement yet */
    enclosing_stm_ = 0;

    /* 
     *   defer creating a 'goto' symbol table until we encounter a label
     *   or a 'goto' 
     */
    goto_symtab_ = 0;

    /* no locals yet */
    local_cnt_ = 0;
    max_local_cnt_ = 0;

    /* no formals yet */
    formal_num = 0;
    varargs = FALSE;
    varargs_list = FALSE;
    varargs_list_local = 0;

    /* check for a short anonymous function, which uses unusual syntax */
    if (cb_type == TCPRS_CB_SHORT_ANON_FN)
    {
        CTcPrsNode *expr;
        CTPNStm *ret_stm;
        
        /* 
         *   a short-form anonymous function always has an argument list,
         *   but it uses special notation: the argument list is simply the
         *   first thing after the function's open brace, and ends with a
         *   colon 
         */
        parse_formal_list(FALSE, FALSE, &formal_num, 0, &varargs,
                          &varargs_list, &varargs_list_local, err,
                          0, TRUE, 0);
        if (*err)
            return 0;

        /*
         *   The contents of a short-form anonymous function are simply an
         *   expression, whose value is implicitly returned by the
         *   function.  Parse the expression.
         */
        expr = parse_expr_or_dstr(TRUE);

        /*
         *   The next token must be the closing brace ('}') of the
         *   function.  If the next token is a semicolon, it's an error,
         *   but it's probably harmless to parsing synchronization, since
         *   the semicolon is almost certainly superfluous, and we can
         *   simply skip it to find our close brace.  
         */
        if (G_tok->cur() == TOKT_SEM)
        {
            /* log an error explaining the problem */
            G_tok->log_error(TCERR_SEM_IN_SHORT_ANON_FN);

            /* skip the semicolon */
            G_tok->next();
        }

        /* check for the brace */
        switch (G_tok->cur())
        {
        case TOKT_RBRACE:
            /* this is what we want - skip it and continue */
            G_tok->next();
            break;

        case TOKT_EOF:
            /* log an error and give up */
            G_tok->log_error_curtok(TCERR_SHORT_ANON_FN_REQ_RBRACE);
            *err = 1;
            return 0;

        default:
            /* log an error, assuming they simply forgot the '}' */
            G_tok->log_error_curtok(TCERR_SHORT_ANON_FN_REQ_RBRACE);
            break;
        }

        /* 
         *   This anonymous function syntax implicitly returns the value of
         *   the expression, so generate a 'return' statement node that
         *   returns the expression.  If the expression has no return value,
         *   we're simply evaluating it for side-effects, so wrap it in a
         *   simple 'expression' statement.  
         */
        if (expr->has_return_value())
            ret_stm = new CTPNStmReturn(expr);
        else
            ret_stm = new CTPNStmExpr(expr);

        /* put the 'return' statement inside a compound statement */
        stm = new CTPNStmComp(ret_stm, local_symtab_);
    }
    else
    {
        propset_token_source tok_src;

        /*
         *   If we have a propertyset stack, set up an inserted token stream
         *   with the expanded token list for the formals, combining the
         *   formals from the enclosing propertyset definitions with the
         *   formals defined here.  
         */
        if (propset_depth != 0)
        {
            int i;
            int formals_found;
            
            /* 
             *   First, determine if we have any added formals from
             *   propertyset definitions.  
             */
            for (formals_found = FALSE, i = 0 ; i < propset_depth ; ++i)
            {
                /* if this one has formals, so note */
                if (propset_stack[i].param_tok_head != 0)
                {
                    /* note it, and we need not look further */
                    formals_found = TRUE;
                    break;
                }
            }

            /*
             *   If we found formals from property sets, we must expand them
             *   into the token stream. 
             */
            if (formals_found)
            {
                int need_comma;

                /* insert an open paren at the start of the expansion list */
                tok_src.insert_token(TOKT_LPAR, "(", 1);

                /* we don't yet need a leading comma */
                need_comma = FALSE;

                /*
                 *   Add the tokens from each propertyset in the stack, from
                 *   the outside in, until we reach an asterisk in each
                 *   stack. 
                 */
                for (i = 0 ; i < propset_depth ; ++i)
                {
                    propset_tok *cur;

                    /* add the tokens from the stack element */
                    for (cur = propset_stack[i].param_tok_head ; cur != 0 ;
                         cur = cur->nxt)
                    {
                        /*
                         *   If we need a comma before the next real
                         *   element, add it now.  
                         */
                        if (need_comma)
                        {
                            tok_src.insert_token(TOKT_COMMA, ",", 1);
                            need_comma = FALSE;
                        }
                        
                        /*
                         *   If this is a comma and the next item is the
                         *   '*', omit the comma - if we have nothing more
                         *   following, we want to suppress the comma. 
                         */
                        if (cur->tok.gettyp() == TOKT_COMMA
                            && cur->nxt != 0
                            && cur->nxt->tok.gettyp() == TOKT_TIMES)
                        {
                            /* 
                             *   it's the comma before the star - simply
                             *   stop here for this list, but note that we
                             *   need a comma before any additional formals
                             *   that we add in the future 
                             */
                            need_comma = TRUE;
                            break;
                        }
                        
                        /* 
                         *   if it's the '*' for this list, stop here, since
                         *   we want to insert the next level in before we
                         *   add these tokens 
                         */
                        if (cur->tok.gettyp() == TOKT_TIMES)
                            break;
                        
                        /* insert it into our expansion list */
                        tok_src.insert_token(&cur->tok);
                    }
                }

                /*
                 *   If we have explicit formals in the true input stream,
                 *   add them, up to but not including the close paren. 
                 */
                if (G_tok->cur() == TOKT_LPAR)
                {
                    /* skip the open paren */
                    G_tok->next();

                    /* check for a non-empty list */
                    if (G_tok->cur() != TOKT_RPAR)
                    {
                        /* 
                         *   the list is non-empty - if we need a comma, add
                         *   it now 
                         */
                        if (need_comma)
                            tok_src.insert_token(TOKT_COMMA, ",", 1);

                        /* we will need a comma at the end of this list */
                        need_comma = TRUE;
                    }

                    /* 
                     *   copy everything up to but not including the close
                     *   paren to the expansion list
                     */
                    while (G_tok->cur() != TOKT_RPAR
                           && G_tok->cur() != TOKT_EOF)
                    {
                        /* insert this token into our expansion list */
                        tok_src.insert_token(G_tok->getcur());

                        /* skip it */
                        G_tok->next();
                    }

                    /* skip the closing paren */
                    if (G_tok->cur() == TOKT_RPAR)
                        G_tok->next();
                }

                /* 
                 *   Finish the expansion by adding the parts of each
                 *   propertyset list after the '*' to the expansion list.
                 *   Copy from the inside out, since we want to unwind the
                 *   nesting from outside in that we did to start with.  
                 */
                for (i = propset_depth ; i != 0 ; )
                {
                    propset_tok *cur;

                    /* move down to the next level */
                    --i;

                    /* find the '*' in this list */
                    for (cur = propset_stack[i].param_tok_head ;
                         cur != 0 && cur->tok.gettyp() != TOKT_TIMES ;
                         cur = cur->nxt) ;

                    /* if we found the '*', skip it */
                    if (cur != 0)
                        cur = cur->nxt;

                    /* 
                     *   also skip the comma following the '*', if present -
                     *   we'll explicitly insert the needed extra comma if
                     *   we actually find we need one 
                     */
                    if (cur != 0 && cur->tok.gettyp() == TOKT_COMMA)
                        cur = cur->nxt;

                    /* 
                     *   insert the remainder of the list into the expansion
                     *   list 
                     */
                    for ( ; cur != 0 ; cur = cur->nxt)
                    {
                        /* if we need a comma, add it now */
                        if (need_comma)
                        {
                            tok_src.insert_token(TOKT_COMMA, ",", 1);
                            need_comma = FALSE;
                        }

                        /* insert this token */
                        tok_src.insert_token(&cur->tok);
                    }
                }

                /* add the closing paren at the end of the expansion list */
                tok_src.insert_token(TOKT_RPAR, ")", 1);

                /*
                 *   We've fully expanded the formal list.  Now all that
                 *   remains is to insert the expanded token list into the
                 *   token input stream, so that we read from the expanded
                 *   list instead of from the original token stream.
                 */
                G_tok->set_external_source(&tok_src);
            }
        }

        /* 
         *   if we have an explicit left parenthesis, or an implied formal
         *   list from an enclosing propertyset, parse the list 
         */
        if (G_tok->cur() == TOKT_LPAR)
        {
            /* skip the open paren */
            G_tok->next();
            
            /* 
             *   Parse the formal list.  Add the symbols to the local
             *   table (hence 'count_only' = false), and don't allow
             *   optional arguments.  
             */
            parse_formal_list(FALSE, FALSE, &formal_num, 0, &varargs,
                              &varargs_list, &varargs_list_local, err,
                              0, FALSE, type_list);
            if (*err)
                return 0;
        }

        /* parse an equals sign, if present */
        if (G_tok->cur() == TOKT_EQ)
        {
            /*
             *   An equals sign after a formal parameter list can be used if
             *   the 'eq_before_brace' flag is set.  Otherwise, if we're
             *   defining an object property, this is an error, since it's
             *   obsolete TADS 2 syntax that we no longer allow - because
             *   this is a change in syntax, we want to catch it
             *   specifically so we can provide good diagnostic information
             *   for it.  
             */
            if (eq_before_brace && G_tok->cur() == TOKT_EQ)
            {
                /* it's allowed - skip the '=' */
                G_tok->next();
            }
            else if (is_obj_prop)
            {
                /* obsolete tads 2 syntax - flag the error */
                G_tok->log_error(TCERR_EQ_WITH_METHOD_OBSOLETE);

                /* 
                 *   skip the '=' so we can continue parsing the rest of the
                 *   code body without cascading errors 
                 */
                G_tok->next();
            }
            else
            {
                /* 
                 *   it's not a situation where we allow '=' specifically,
                 *   or where we know why it might be present erroneously -
                 *   let it go for now, as we'll flag the error in the
                 *   normal compound statement parsing 
                 */
            }
        }

        /* check for '(' syntax */
        //$$$

        /* require the '{' */
        switch (G_tok->cur())
        {
        case TOKT_LBRACE:
        parse_body:
            /* parse the compound statement */
            stm = parse_compound(err, TRUE, 0, TRUE);
            break;

        case TOKT_SEM:
        case TOKT_RBRACE:
            /* 
             *   we seem to have found the end of the object definition, or
             *   the end of a code body - treat it as an empty code body 
             */
            G_tok->log_error_curtok(TCERR_REQ_LBRACE_CODE);
            stm = new CTPNStmComp(0, 0);
            break;

        default:
            /* 
             *   the '{' was missing - log an error, but proceed from the
             *   current token on the assumption that they merely left out
             *   the open brace 
             */
            G_tok->log_error_curtok(TCERR_REQ_LBRACE_CODE);
            goto parse_body;
        }
    }

    /* if that failed, return the error */
    if (*err || stm == 0)
        return 0;

    /* 
     *   determine how the statement exits, and generate any internal flow
     *   warnings within the body code
     */
    flow_flags = stm->get_control_flow(TRUE);

    /*
     *   Warn if the function has both explicit void and value returns.
     *   If not, check to see if it continues; if so, it implicitly
     *   returns a void value by falling off the end, so warn if it both
     *   falls off the end and returns a value somewhere else.  Suppress
     *   this warning if this is a syntax check only.  
     */
    if (!G_prs->get_syntax_only())
    {
        if ((flow_flags & TCPRS_FLOW_RET_VAL) != 0
            && (flow_flags & TCPRS_FLOW_RET_VOID) != 0)
        {
            /* it has explicit void and value returns */
            stm->log_warning(TCERR_RET_VAL_AND_VOID);
        }
        else if ((flow_flags & TCPRS_FLOW_RET_VAL) != 0
                 && (flow_flags & TCPRS_FLOW_NEXT) != 0)
        {
            /* it has explicit value returns, and implicit void return */
            stm->log_warning(TCERR_RET_VAL_AND_IMP_VOID);
        }
    }
        
    /* if the caller is interested, return the interface details */
    if (p_argc != 0)
        *p_argc = formal_num;
    if (p_varargs != 0)
        *p_varargs = varargs;
    if (p_varargs_list != 0)
        *p_varargs_list = varargs_list;
    if (p_varargs_list_local != 0)
        *p_varargs_list_local = varargs_list_local;
    if (p_has_retval)
        *p_has_retval = ((flow_flags & TCPRS_FLOW_RET_VAL) != 0);

    /* create a code body node for the result */
    body_stm = new CTPNCodeBody(local_symtab_, goto_symtab_, stm,
                                formal_num, varargs,
                                varargs_list, varargs_list_local,
                                max_local_cnt_, self_valid,
                                enclosing_code_body);

    /* store this new statement in the current code body reference object */
    cur_code_body_->ptr = body_stm;

    /* 
     *   set the end location in the new code body to the end location in
     *   the underlying compound statement 
     */
    body_stm->set_end_location(stm->get_end_desc(), stm->get_end_linenum());

    /* if we have a local context, mark the code body accordingly */
    if (has_local_ctx_)
        body_stm->set_local_ctx(local_ctx_var_num_, next_ctx_arr_idx_ - 1);

    /* 
     *   If the caller passed in a local symbol table, check the table for
     *   context variables from enclosing scopes, and assign the local
     *   holder for each such variable. 
     */
    if (local_symtab != 0)
    {
        enum_locals_ctx ctx;
        CTcPrsSymtab *tab, *par;

        /* 
         *   consider only the outermost local table, since that's where
         *   the shared locals reside 
         */
        for (tab = local_symtab ;
             par = tab->get_parent(),
             par != 0 && par != G_prs->get_global_symtab() ;
             tab = par) ;

        /* enumerate the variables */
        ctx.symtab = tab;
        ctx.code_body = body_stm;
        tab->enum_entries(&enum_for_ctx_locals, &ctx);
    }

    /* 
     *   if 'self' is valid, and we're parsing an anonymous function, and we
     *   have any references in this code body to any method context
     *   variables (self, targetprop, targetobj, definingobj), make certain
     *   that the code body has a context at level 1, so that it can pick up
     *   our method context 
     */
    if (self_valid && parsing_anon_fn
        && (self_referenced_ || full_method_ctx_referenced_))
        body_stm->get_or_add_ctx_var_for_level(1);

    /* mark the code body for references to the method context */
    body_stm->set_self_referenced(self_referenced_);
    body_stm->set_full_method_ctx_referenced(full_method_ctx_referenced_);

    /* 
     *   mark the code body for inclusion in any local context of the method
     *   context 
     */
    body_stm->set_local_ctx_needs_self(local_ctx_needs_self_);
    body_stm->set_local_ctx_needs_full_method_ctx(
        local_ctx_needs_full_method_ctx_);

    /* return the new body statement */
    return body_stm;
}

/*
 *   Parse a compound statement 
 */
CTPNStmComp *CTcParser::parse_compound(int *err, int skip_lbrace,
                                       CTPNStmSwitch *enclosing_switch,
                                       int use_enclosing_scope)
{
    CTPNStm *first_stm;
    CTPNStm *last_stm;
    CTPNStm *cur_stm;
    CTPNStmComp *comp_stm;
    int done;
    tcprs_scope_t scope_data;
    CTcTokFileDesc *file;
    long linenum;
    int skip_rbrace;

    /* save the current line information for later */
    G_tok->get_last_pos(&file, &linenum);

    /* skip the '{' if we're on one and the caller wants us to */
    if (skip_lbrace && G_tok->cur() == TOKT_LBRACE)
        G_tok->next();

    /* enter a scope */
    if (!use_enclosing_scope)
        enter_scope(&scope_data);

    /* we don't have any statements in our sublist yet */
    first_stm = last_stm = 0;

    /* presume we won't find the closing brace */
    skip_rbrace = FALSE;

    /* keep going until we reach the closing '}' */
    for (done = FALSE ; !done ; )
    {
        /* check what we've found */
        switch (G_tok->cur())
        {
        case TOKT_RBRACE:
            /* it's our closing brace - we're done */
            done = TRUE;
            cur_stm = 0;

            /* note that we must still skip the closing brace */
            skip_rbrace = TRUE;

            /* stop scanning statements */
            break;

        default:
            /* parse a statement */
            cur_stm = parse_stm(err, enclosing_switch, FALSE);

            /* if an error occurred, stop parsing */
            if (*err)
                done = TRUE;
            break;
        }

        /* if we parsed a statement, add it to our list */
        if (cur_stm != 0)
        {
            /* link the statement at the end of our list */
            if (last_stm != 0)
                last_stm->set_next_stm(cur_stm);
            else
                first_stm = cur_stm;
            last_stm = cur_stm;
        }
    }

    /* if there's no statement, make the body a null statement */
    if (first_stm == 0)
        first_stm = new CTPNStmNull();

    /* build the compound statement node */
    comp_stm = new CTPNStmComp(first_stm, local_symtab_);

    /* set some additional information if we created a statement */
    if (comp_stm != 0)
    {
        /* set the statement's line to the start of the compound */
        comp_stm->set_source_pos(file, linenum);

        /* note whether or not we have our own private scope */
        comp_stm->set_has_own_scope(!use_enclosing_scope
                                    && (local_symtab_
                                        != scope_data.local_symtab));
    }

    /* if necessary, skip the closing brace */
    if (skip_rbrace)
        G_tok->next();

    /* leave the local scope */
    if (!use_enclosing_scope)
        leave_scope(&scope_data);

    /* return the compound statement object */
    return comp_stm;
}

/*
 *   Create a local symbol table for the current scope, if necessary 
 */
void CTcParser::create_scope_local_symtab()
{
    /* 
     *   if our symbol table is the same as the enclosing symbol table, we
     *   must create our own table 
     */
    if (local_symtab_ == enclosing_local_symtab_)        
    {
        /* 
         *   Create our own local symbol table, replacing the current one
         *   - we saved the enclosing one already when we entered the
         *   scope, so we'll restore it on our way out.  The new local
         *   symbol table has the enclosing symbol table as its parent
         *   scope.  
         */
        local_symtab_ = new CTcPrsSymtab(local_symtab_);
    }
}

/*
 *   Parse a local variable definition 
 */
CTPNStm *CTcParser::parse_local(int *err)
{
    int done;
    CTPNStm *first_stm;
    CTPNStm *last_stm;

    /* we have no initializer statements yet */
    first_stm = last_stm = 0;

    /* skip the 'local' keyword */
    G_tok->next();

    /* keep going until we reach the closing semicolon */
    for (done = FALSE ; !done ; )
    {
        /* we need a symbol name */
        if (G_tok->cur() == TOKT_SYM)
        {
            const char *sym;
            size_t symlen;
            CTcSymLocal *lcl;
            CTPNStm *stm;
            CTcPrsNode *expr;
            
            /* get the symbol string from the token */
            sym = G_tok->getcur()->get_text();
            symlen = G_tok->getcur()->get_text_len();
            
            /* add the new local variable to our symbol table */
            lcl = local_symtab_->add_local(sym, symlen, alloc_local(),
                                           FALSE, FALSE, FALSE);

            /* skip the symbol and check for an initial value assignment */
            switch (G_tok->next())
            {
            case TOKT_EQ:
            case TOKT_ASI:
                /* parse the initializer */
                expr = parse_local_initializer(lcl, err);

                /* if we didn't get a statement, we can't proceed */
                if (expr == 0)
                {
                    done = TRUE;
                    break;
                }

                /* create a statement for the assignment */
                stm = new CTPNStmExpr(expr);

                /* 
                 *   set the statement's source location according to the
                 *   current source location - if we have multiple
                 *   initializers over several lines, this will allow the
                 *   debugger to step through the individual
                 *   initializations 
                 */
                stm->set_source_pos(G_tok->get_last_desc(),
                                    G_tok->get_last_linenum());

                /* add the statement to our list */
                if (last_stm != 0)
                    last_stm->set_next_stm(stm);
                else
                    first_stm = stm;
                last_stm = stm;

                /* done */
                break;

            default:
                /* there's nothing more to do with this variable */
                break;
            }

            /* 
             *   check what follows - we can have a comma to introduce
             *   another local variable, or a semicolon to end the
             *   statement 
             */
            switch(G_tok->cur())
            {
            case TOKT_COMMA:
                /* skip the comma and go on to the next variable */
                G_tok->next();
                break;

            case TOKT_SEM:
                /* skip the semicolon, and stop scanning */
                G_tok->next();
                done = TRUE;
                break;

            case TOKT_SYM:
                /* 
                 *   they probably just left out a comma - assume the
                 *   comma is there and keep going 
                 */
                G_tok->log_error_curtok(TCERR_LOCAL_REQ_COMMA);
                break;

            default:
                /* 
                 *   these almost certainly indicate that they left out a
                 *   semicolon - report the error and continue from here 
                 */
                G_tok->log_error_curtok(TCERR_LOCAL_REQ_COMMA);
                done = TRUE;
                break;
            }
        }
        else
        {
            /* symbol required - log the error */
            G_tok->log_error_curtok(TCERR_LOCAL_REQ_SYM);

            /* determine how to proceed based on what we have */
            switch(G_tok->cur())
            {
            case TOKT_COMMA:
                /* 
                 *   they probably just put in an extra comma - skip it
                 *   and keep trying to parse the local list 
                 */
                G_tok->next();
                break;

            case TOKT_SEM:
                /* that's the end of the statement */
                G_tok->next();
                done = TRUE;
                break;

            case TOKT_EOF:
                /* set the error flag and stop scanning */
                *err = TRUE;
                done = TRUE;
                break;

            default:
                /* try skipping this token and trying again */
                G_tok->next();
                break;
            }
        }
    }

    /* 
     *   if we have one statement, return it; if we have more than one,
     *   return a compound statement to contain the list; if we have
     *   nothing, return nothing 
     */
    if (first_stm == 0)
        return 0;
    else if (first_stm == last_stm)
        return first_stm;
    else
        return new CTPNStmComp(first_stm, local_symtab_);
}

/*
 *   Parse a local variable initializer 
 */
CTcPrsNode *CTcParser::parse_local_initializer(CTcSymLocal *lcl, int *err)
{
    CTcPrsNode *expr;
    
    /* 
     *   skip the assignment operator and parse the expression (which
     *   cannot use the comma operator) 
     */
    G_tok->next();
    expr = parse_asi_expr();

    /* if that failed, return failure */
    if (expr == 0)
        return 0;

    /* 
     *   if we have a valid local, return a new expression node for the
     *   assignment; otherwise just return the expression, since we have
     *   nothing to assign to 
     */
    return (lcl != 0 ? new CTPNAsi(new CTPNSymResolved(lcl), expr) : expr);
}

/*
 *   Parse a statement 
 */
CTPNStm *CTcParser::parse_stm(int *err, CTPNStmSwitch *enclosing_switch,
                              int compound_use_enclosing_scope)
{
    CTcToken tok;

    /* 
     *   remember where the statement starts - when we create the
     *   statement object, it will refer to these values to set its
     *   internal memory of the statement's source file location 
     */
    cur_desc_ = G_tok->get_last_desc();
    cur_linenum_ = G_tok->get_last_linenum();

    /* see what we have */
try_again:
    switch(G_tok->cur())
    {
    case TOKT_EOF:
        /* unexpected end of file - log an error */
        G_tok->log_error(TCERR_EOF_IN_CODE);
        
        /* set the caller's error flag */
        *err = TRUE;

        /* there's no statement to return, obviously */
        return 0;

    case TOKT_DSTR_MID:
    case TOKT_DSTR_END:
    case TOKT_RBRACE:
        /* 
         *   we shouldn't be looking at any of these at the start of a
         *   statement 
         */
        G_tok->log_error_curtok(TCERR_EXPECTED_STMT_START);
        G_tok->next();
        return 0;

    case TOKT_SEM:
        /* 
         *   null statement - this doesn't generate any code; simply skip
         *   the semicolon and keep going 
         */
        G_tok->next();

        /* this doesn't generate any code */
        return 0;

    case TOKT_LOCAL:
        /* if we don't have our own local symbol table, create one */
        create_scope_local_symtab();

        /* parse the local variable definition and return the result */
        return parse_local(err);

    case TOKT_LBRACE:
        /* it's a compound statement */
        return parse_compound(err, TRUE, 0, compound_use_enclosing_scope);
        
    case TOKT_IF:
        /* parse an if statement */
        return parse_if(err);

    case TOKT_RETURN:
        /* parse a return statement */
        return parse_return(err);

    case TOKT_FOR:
        /* parse a for statement */
        return parse_for(err);

    case TOKT_FOREACH:
        /* parse a foreach statement */
        return parse_foreach(err);

    case TOKT_WHILE:
        /* parse a while statement */
        return parse_while(err);

    case TOKT_DO:
        /* parse a do-while */
        return parse_do_while(err);

    case TOKT_SWITCH:
        /* parse a switch */
        return parse_switch(err);

    case TOKT_GOTO:
        /* parse a 'goto' */
        return parse_goto(err);

    case TOKT_BREAK:
        return parse_break(err);

    case TOKT_CONTINUE:
        return parse_continue(err);

    case TOKT_TRY:
        return parse_try(err);

    case TOKT_THROW:
        return parse_throw(err);

    case TOKT_CATCH:
        /* misplaced 'catch' clause - log an error */
        G_tok->log_error(TCERR_MISPLACED_CATCH);

        /* 
         *   skip the following open paren, class name, variable name, and
         *   closing paren, as long as we find all of these 
         */
        if (G_tok->next() == TOKT_LPAR
            && G_tok->next() == TOKT_SYM
            && G_tok->next() == TOKT_SYM
            && G_tok->next() == TOKT_RPAR)
            G_tok->next();

        /* there's no valid statement to return */
        return 0;

    case TOKT_FINALLY:
        /* misplaced 'finally' clause - log an error */
        G_tok->log_error(TCERR_MISPLACED_FINALLY);

        /* skip the 'finally' keyword, and return failure */
        G_tok->next();
        return 0;

    case TOKT_ELSE:
        /* 
         *   misplaced 'else' clause - log an error, skip the 'else'
         *   keyword, and proceed with what follows 
         */
        G_tok->log_error(TCERR_MISPLACED_ELSE);
        G_tok->next();
        return 0;

    case TOKT_CASE:
        /* 
         *   if we're in a 'switch', it's a valid 'case' label; otherwise,
         *   it's misplaced 
         */
        if (enclosing_switch != 0)
        {
            /* parse the 'case' label */
            return parse_case(err, enclosing_switch);
        }
        else
        {
            /* 
             *   not directly within a 'switch', so this is a misplaced
             *   'case' keyword - log an error 
             */
            G_tok->log_error(TCERR_MISPLACED_CASE);
            
            /* skip the 'case' keyword */
            G_tok->next();
            
            /* assume there's an expression here, and skip that as well */
            parse_expr();
            
            /* if there's a colon, skip it, too */
            if (G_tok->cur() == TOKT_COLON)
                G_tok->next();
            
            /* proceed from here */
            return 0;
        }

    case TOKT_DEFAULT:
        /* allow this only if we're directly in a 'switch' body */
        if (enclosing_switch != 0)
        {
            /* parse the 'default' label */
            return parse_default(err, enclosing_switch);
        }
        else
        {
            /* misplaced 'default' keyword - log an error */
            G_tok->log_error(TCERR_MISPLACED_DEFAULT);
            
            /* skip the 'default' keyword; if there's a colon, skip it, too */
            if (G_tok->next() == TOKT_COLON)
                G_tok->next();

            /* proceed from here */
            return 0;
        }

    case TOKT_SYM:
        /*
         *   It's a symbol.  First, check for a label.  This requires that
         *   we look ahead one token, because we have to look at the next
         *   token to see if it's a colon; if it's not, we have to back up
         *   and parse the symbol as the start of an expression.  So,
         *   remember the current symbol token, then look at what follows.
         */
        tok = *G_tok->copycur();
        if (G_tok->next() == TOKT_COLON)
        {
            CTPNStmEnclosing *old_enclosing;
            CTPNStmLabel *label_stm;
            CTcSymLabel *lbl;
            CTPNStm *stm;

            /* it's a label - create a symbol table entry for it */
            lbl = add_code_label(&tok);

            /* create the labeled statement node */
            label_stm = new CTPNStmLabel(lbl, enclosing_stm_);

            /* skip the colon */
            G_tok->next();

            /* 
             *   set our new label to be the enclosing label for
             *   everything contained within its statement 
             */
            old_enclosing = set_enclosing_stm(label_stm);

            /* parse the labeled statement */
            stm = parse_stm(err, enclosing_switch, FALSE);

            /* restore our enclosing statement */
            set_enclosing_stm(old_enclosing);

            /* if parsing the labeled statement failed, give up */
            if (*err)
                return 0;

            /* connect to the label to the statement it labels */
            label_stm->set_stm(stm);

            /* point the label symbol to its statement node */
            if (lbl != 0)
                lbl->set_stm(label_stm);

            /* return the labeled statement node */
            return label_stm;
        }

        /* 
         *   it's not a label - push the colon back into the input stream
         *   so that we read it again, then parse this as an ordinary
         *   expression 
         */
        G_tok->unget();
        goto do_parse_expr;

    case TOKT_RPAR:
        /* 
         *   they probably had too many close parens in something like a
         *   'for' or 'if' statement - flag the error 
         */
        G_tok->log_error(TCERR_EXTRA_RPAR);

        /* skip the extra paren and go back for another try */
        G_tok->next();
        goto try_again;
        
    default:
    do_parse_expr:
        /* anything else must be the start of an expression */
        {
            CTcPrsNode *expr;
            
            /* parse the expression */
            expr = parse_expr_or_dstr(TRUE);

            /* the statement must be terminated with a semicolon */
            if (parse_req_sem())
            {
                /* set the error flag */
                *err = TRUE;

                /* there's no statement to return */
                return 0;
            }
            
            /* 
             *   if we successfully parsed an expression, create a
             *   statement node for the expression; if expr is null, the
             *   expression parser will already have issued an error, so
             *   we can simply ignore the failed expression and continue
             *   to the next statement 
             */
            if (expr != 0)
                return new CTPNStmExpr(expr);
            else
                return 0;
        }
    }
}


/*
 *   Add a 'goto' label symbol to the current code body
 */
CTcSymLabel *CTcParser::add_code_label(const CTcToken *tok)
{
    /* if there's no 'goto' symbol table, create one */
    if (goto_symtab_ == 0)
        goto_symtab_ = new CTcPrsSymtab(0);

    /* create the label and return it */
    return goto_symtab_->add_code_label(tok->get_text(),
                                        tok->get_text_len(), FALSE);
}


/* 
 *   Parse an 'if' statement 
 */
CTPNStm *CTcParser::parse_if(int *err)
{
    CTcPrsNode *cond_expr;
    CTPNStm *if_stm;
    CTPNStm *then_stm;
    CTPNStm *else_stm;
    CTcTokFileDesc *file;
    long linenum;
    
    /* save the starting line information for later */
    G_tok->get_last_pos(&file, &linenum);

    /* skip the 'if' keyword, and require the open paren */
    if (G_tok->next() == TOKT_LPAR)
    {
        /* skip the left paren */
        G_tok->next();
    }
    else
    {
        /* 
         *   log an error, but proceed on the assumption that they simply
         *   left out the paren 
         */
        G_tok->log_error_curtok(TCERR_REQ_LPAR_IF);
    }

    /* parse the expression */
    cond_expr = parse_cond_expr();

    /* if that failed, return failure */
    if (cond_expr == 0)
    {
        *err = TRUE;
        return 0;
    }

    /* require the close paren */
    if (G_tok->cur() == TOKT_RPAR)
    {
        /* skip it */
        G_tok->next();
    }
    else
    {
        /* 
         *   log an error, then proceed assuming that the paren was merely
         *   omitted 
         */
        G_tok->log_error_curtok(TCERR_REQ_RPAR_IF);
    }

    /* parse the true-part statement */
    then_stm = parse_stm(err, 0, FALSE);

    /* if an error occurred, return failure */
    if (*err)
        return 0;

    /* check for 'else' */
    if (G_tok->cur() == TOKT_ELSE)
    {
        /* skip the 'else' keyword */
        G_tok->next();

        /* parse the false-part statement */
        else_stm = parse_stm(err, 0, FALSE);

        /* if an error occurred, return failure */
        if (*err)
            return 0;
    }
    else
    {
        /* there's no 'else' part */
        else_stm = 0;
    }

    /* create and return the 'if' statement node */
    if_stm = new CTPNStmIf(cond_expr, then_stm, else_stm);

    /* set the original statement position in the node */
    if_stm->set_source_pos(file, linenum);

    /* return the 'if' statement node */
    return if_stm;
}

/*
 *   Parse a 'return' statement 
 */
CTPNStm *CTcParser::parse_return(int *err)
{
    CTPNStm *stm;
    
    /* skip the 'return' keyword and see what we have */
    switch(G_tok->next())
    {
    case TOKT_SEM:
        /* 
         *   end of the statement - this is a void return; skip the
         *   semicolon, and return a void return statement node 
         */
        G_tok->next();
        stm = new CTPNStmReturn(0);
        break;

    case TOKT_LBRACE:
    case TOKT_RBRACE:
        /* 
         *   they probably just left out a semicolon - flag the error, and
         *   continue parsing from this token 
         */
        G_tok->log_error_curtok(TCERR_RET_REQ_EXPR);
        return 0;

    default:
        /* it's a return with an expression - parse the expression */
        stm = new CTPNStmReturn(parse_expr());

        /* make sure we're on a semicolon */
        if (parse_req_sem())
        {
            *err = TRUE;
            return 0;
        }

        /* done */
        break;
    }

    /* return the statement node we created */
    return stm;
}

/* 
 *   Parse a 'for' statement 
 */
CTPNStm *CTcParser::parse_for(int *err)
{
    tcprs_scope_t scope_data;
    int done;
    CTcPrsNode *init_expr;
    CTcPrsNode *cond_expr;
    CTcPrsNode *reinit_expr;
    CTPNStm *body_stm;
    CTPNStmFor *for_stm;
    CTcTokFileDesc *file;
    long linenum;
    CTPNStmEnclosing *old_enclosing;
    
    /* save the current line information for later */
    G_tok->get_last_pos(&file, &linenum);

    /* 
     *   enter a scope, in case we create a local symbol table for local
     *   variables defined within the 'for' statement 
     */
    enter_scope(&scope_data);

    /* parse the open paren */
    if (G_tok->next() == TOKT_LPAR)
    {
        /* skip it */
        G_tok->next();
    }
    else
    {
        /* log an error, and proceed, assuming it was simply left out */
        G_tok->log_error_curtok(TCERR_REQ_FOR_LPAR);
    }

    /* we don't have any of the expressions yet */
    init_expr = 0;
    cond_expr = 0;
    reinit_expr = 0;

    /* parse the initializer list */
    for (done = FALSE ; !done ; )
    {
        CTcPrsNode *expr;

        /* presume we won't find an expression on this round */
        expr = 0;
        
        /* check what we have */
        switch(G_tok->cur())
        {
        case TOKT_LOCAL:
            /* 
             *   if we haven't created our own symbol table local to the
             *   'for' loop, do so now 
             */
            create_scope_local_symtab();
            
            /* skip the 'local' keyword and get the local name */
            if (G_tok->next() == TOKT_SYM)
            {
                CTcSymLocal *lcl;
                
                /* add the local symbol */
                lcl = local_symtab_
                      ->add_local(G_tok->getcur()->get_text(),
                                  G_tok->getcur()->get_text_len(),
                                  alloc_local(), FALSE, FALSE, FALSE);

                /* check for the required initializer */
                switch(G_tok->next())
                {
                case TOKT_ASI:
                case TOKT_EQ:
                    /* parse the initializer */
                    expr = parse_local_initializer(lcl, err);
                    break;
                    
                default:
                    /* log an error - an initializer is required */
                    G_tok->log_error_curtok(TCERR_REQ_FOR_LOCAL_INIT);
                    break;
                }
            }
            else
            {
                /* 
                 *   the 'local' statement isn't constructed properly -
                 *   this is difficult to recover from intelligently, so
                 *   just log an error and keep going from here 
                 */
                G_tok->log_error_curtok(TCERR_LOCAL_REQ_SYM);
                break;
            }
            break;

        case TOKT_SEM:
            /* it's a semicolon - we're done with the initializer list */
            done = TRUE;

            /* 
             *   if we have an expression already, it means that the
             *   previous token was a comma - this is an error, since we
             *   have a missing expression; log the error but continue
             *   anyway 
             */
            if (init_expr != 0)
                G_tok->log_error(TCERR_MISSING_FOR_INIT_EXPR);
            break;

        case TOKT_RPAR:
        case TOKT_LBRACE:
        case TOKT_RBRACE:
            /* premature end of the list - log an error and stop */
            G_tok->log_error_curtok(TCERR_MISSING_FOR_PART);
            done = TRUE;
            break;

        default:
            /* 
             *   This must be an expression - parse it.  Parse an
             *   assignment expression, not a comma expression, because we
             *   must check for a "local" clause after each comma. 
             */
            expr = parse_asi_expr();

            /* if that failed, stop scanning the "for" */
            if (expr == 0)
                done = TRUE;
            break;
        }

        /* if we're done, we can stop now */
        if (done)
            break;

        /* 
         *   if we got an expression, add it into the initializer
         *   expression under construction by adding it under a "comma"
         *   node 
         */
        if (expr != 0)
        {
            /* 
             *   if there's an expression, build a comma expression for
             *   the expression so far plus the new expression; otherwise,
             *   the new expression becomes the entire expression so far 
             */
            if (init_expr != 0)
                init_expr = new CTPNComma(init_expr, expr);
            else
                init_expr = expr;
        }

        /* 
         *   we must have a semicolon or comma after each initializer
         *   expression 
         */
        switch(G_tok->cur())
        {
        case TOKT_SEM:
            /* that's the end of the statement - stop now */
            done = TRUE;
            break;

        case TOKT_COMMA:
            /* skip the comma and parse the next initializer */
            G_tok->next();
            break;

        case TOKT_RPAR:
        case TOKT_LBRACE:
        case TOKT_RBRACE:
            /* log an error, and stop parsing the expression list */
            G_tok->log_error_curtok(TCERR_MISSING_FOR_PART);
            done = TRUE;
            break;

        default:
            /* log an error */
            G_tok->log_error_curtok(TCERR_REQ_FOR_INIT_COMMA);

            /* skip the errant token and keep going */
            G_tok->next();
            break;
        }
    }

    /* 
     *   if we successfully found the ';' at the end of the initializer
     *   list, parse the condition expression 
     */
    if (G_tok->cur() == TOKT_SEM)
    {
        int cont_to_reinit;

        /* presume we'll want to continue to the reinit expression */
        cont_to_reinit = TRUE;

        /* skip the ';' */
        G_tok->next();
        
        /* if the condition isn't empty, parse it */
        if (G_tok->cur() != TOKT_SEM)
            cond_expr = parse_cond_expr();

        /* require the ';' after the expression */
        switch(G_tok->cur())
        {
        case TOKT_SEM:
            /* it's fine - keep going from here */
            G_tok->next();
            break;

        case TOKT_RPAR:
        case TOKT_LBRACE:
        case TOKT_RBRACE:
            /* missing part */
            G_tok->log_error_curtok(TCERR_MISSING_FOR_PART);

            /* don't bother trying to find a reinitialization expression */
            cont_to_reinit = FALSE;
            break;

        default:
            /* 
             *   we seem to be missing the semicolon; keep going from
             *   here, assuming that they simply left out the semicolon
             *   between the condition and reinitializer expressions 
             */
            G_tok->log_error_curtok(TCERR_REQ_FOR_COND_SEM);
            break;
        }

        /* 
         *   if we're to continue to the reinitializer, parse it; there is
         *   no reinitialization expression if the next token is a right
         *   paren 
         */
        if (cont_to_reinit && G_tok->cur() != TOKT_RPAR)
        {
            /* parse the expression */
            reinit_expr = parse_expr();
        }

        /* make sure we have the right paren */
        if (G_tok->cur() == TOKT_RPAR)
        {
            /* skip the paren */
            G_tok->next();
        }
        else
        {
            /* 
             *   log an error, and try parsing the body from here, on the
             *   assumption that they simply forgot about the right paren
             *   and jumped right into the body 
             */
            G_tok->log_error_curtok(TCERR_REQ_FOR_RPAR);
        }
    }
    else if (G_tok->cur() == TOKT_RPAR)
    {
        /* 
         *   we already found the right paren - early, so we logged an
         *   error - simply skip it now so that we can proceed to the body
         *   of the 'for' 
         */
        G_tok->next();
    }

    /* create the "for" node */
    for_stm = new CTPNStmFor(init_expr, cond_expr, reinit_expr,
                             local_symtab_, enclosing_stm_);

    /* set the 'for' to enclose its body */
    old_enclosing = set_enclosing_stm(for_stm);

    /* parse the body of the "for" loop */
    body_stm = parse_stm(err, 0, FALSE);

    /* restore the old enclosing statement */
    set_enclosing_stm(old_enclosing);

    /* if that failed, return failure */
    if (*err)
        return 0;

    /* set the body of the 'for' */
    for_stm->set_body(body_stm);

    /* set the original statement position in the node */
    for_stm->set_source_pos(file, linenum);

    /* set the own-scope flag */
    for_stm->set_has_own_scope(local_symtab_ != scope_data.local_symtab);

    /* exit any local scope we created */
    leave_scope(&scope_data);

    /* return the "for" node */
    return for_stm;
}

/* 
 *   Parse a 'foreach' statement 
 */
CTPNStm *CTcParser::parse_foreach(int *err)
{
    tcprs_scope_t scope_data;
    CTcPrsNode *iter_expr;
    CTcPrsNode *coll_expr;
    CTPNStm *body_stm;
    CTPNStmForeach *foreach_stm;
    CTcTokFileDesc *file;
    long linenum;
    CTPNStmEnclosing *old_enclosing;

    /* save the current line information for later */
    G_tok->get_last_pos(&file, &linenum);

    /* 
     *   enter a scope, in case we create a local symbol table for local
     *   variables defined within the 'for' statement 
     */
    enter_scope(&scope_data);

    /* parse the open paren */
    if (G_tok->next() == TOKT_LPAR)
    {
        /* skip it */
        G_tok->next();
    }
    else
    {
        /* log an error, and proceed, assuming it was simply left out */
        G_tok->log_error_curtok(TCERR_REQ_FOREACH_LPAR);
    }

    /* we don't have the iterator lvalue or collection expression yet */
    iter_expr = 0;
    coll_expr = 0;

    /* check for 'local' before the iteration variable */
    switch (G_tok->cur())
    {
    case TOKT_LOCAL:
        /* 
         *   if we haven't created our own symbol table local to the 'for'
         *   loop, do so now 
         */
        create_scope_local_symtab();

        /* skip the 'local' keyword and get the local name */
        if (G_tok->next() == TOKT_SYM)
        {
            /* add the local symbol */
            local_symtab_->add_local(G_tok->getcur()->get_text(),
                                     G_tok->getcur()->get_text_len(),
                                     alloc_local(), FALSE, FALSE, FALSE);
        }
        else
        {
            /* log the error */
            G_tok->log_error_curtok(TCERR_LOCAL_REQ_SYM);
        }

        /* go handle the local as the iteration expression */
        goto do_expr;

    case TOKT_LPAR:
    case TOKT_SYM:
    do_expr:
        /* parse the iterator lvalue expression */
        iter_expr = parse_expr();
        if (iter_expr == 0)
        {
            *err = TRUE;
            return 0;
        }
        break;
        
    default:
        /* premature end of the list - log an error and stop */
        G_tok->log_error_curtok(TCERR_MISSING_FOREACH_EXPR);
        return 0;
    }

    /* require the 'in' keyword */
    if (G_tok->cur() != TOKT_SYM || !G_tok->getcur()->text_matches("in", 2))
    {
        /* log an error */
        G_tok->log_error_curtok(TCERR_FOREACH_REQ_IN);

        /* see what we have */
        switch(G_tok->cur())
        {
        case TOKT_LBRACE:
        case TOKT_RBRACE:
        case TOKT_SEM:
        case TOKT_EOF:
            /* probably end of statement */
            return 0;
            
        case TOKT_RPAR:
            /* 
             *   probably an extra paren in the variable expression - skip
             *   the paren and continue 
             */
            G_tok->next();
            break;

        default:
            /* probably just left out 'in' - continue from here */
            break;
        }
    }
    else
    {
        /* skip the 'in' */
        G_tok->next();
    }

    /* parse the collection expression */
    coll_expr = parse_expr();
    if (coll_expr == 0)
    {
        *err = TRUE;
        return 0;
    }

    /* make sure we have the close paren */
    if (G_tok->cur() != TOKT_RPAR)
    {
        /* 
         *   log the error, but continue from here on the assumption that
         *   they simply left out the paren 
         */
        G_tok->log_error_curtok(TCERR_REQ_FOREACH_RPAR);
    }
    else
    {
        /* skip the paren */
        G_tok->next();
    }

    /* 
     *   create the "foreach" node, allocating a private local variable
     *   for holding the iterator object 
     */
    foreach_stm = new CTPNStmForeach(iter_expr, coll_expr, 
                                     local_symtab_, enclosing_stm_,
                                     alloc_local());

    /* set the "foreach" node to enclose its body */
    old_enclosing = set_enclosing_stm(foreach_stm);

    /* parse the body of the loop */
    body_stm = parse_stm(err, 0, FALSE);

    /* restore the old enclosing statement */
    set_enclosing_stm(old_enclosing);

    /* if that failed, return failure */
    if (*err)
        return 0;

    /* set the body of the 'for' */
    foreach_stm->set_body(body_stm);

    /* set the original statement position in the node */
    foreach_stm->set_source_pos(file, linenum);

    /* set the own-scope flag */
    foreach_stm->set_has_own_scope(local_symtab_ != scope_data.local_symtab);

    /* exit any local scope we created */
    leave_scope(&scope_data);

    /* return the new statement node */
    return foreach_stm;
}

/*
 *   Parse a 'break' statement 
 */
CTPNStm *CTcParser::parse_break(int *err)
{
    CTPNStmBreak *brk_stm;

    /* create the 'break' statement */
    brk_stm = new CTPNStmBreak();

    /* skip the 'break' keyword and check what follows */
    switch(G_tok->next())
    {
    case TOKT_SYM:
        /* set the label in the statement */
        brk_stm->set_label(G_tok->getcur());

        /* skip the label token */
        G_tok->next();
        break;

    case TOKT_SEM:
        /* keep going - we'll skip it in a moment */
        break;

    case TOKT_LBRACE:
    case TOKT_RBRACE:
        /* 
         *   they almost certainly simply left off the semicolon - don't
         *   bother with a "label expected" error, since the real error is
         *   most likely just "missing semicolon" 
         */
        break;

    default:
        /* log the error */
        G_tok->log_error_curtok(TCERR_BREAK_REQ_LABEL);
        break;
    }

    /* parse the required terminating semicolon */
    if (parse_req_sem())
    {
        *err = TRUE;
        return 0;
    }

    /* return the 'break' node */
    return brk_stm;
}

/*
 *   Parse a 'continue' statement 
 */
CTPNStm *CTcParser::parse_continue(int *err)
{
    CTPNStmContinue *cont_stm;

    /* create the 'break' statement */
    cont_stm = new CTPNStmContinue();

    /* skip the 'continue' keyword and check what follows */
    switch(G_tok->next())
    {
    case TOKT_SYM:
        /* set the label in the statement */
        cont_stm->set_label(G_tok->getcur());

        /* skip the label token */
        G_tok->next();
        break;

    case TOKT_SEM:
        /* keep going - we'll skip it in a moment */
        break;

    case TOKT_LBRACE:
    case TOKT_RBRACE:
        /* 
         *   they almost certainly simply left off the semicolon - don't
         *   bother with a "label expected" error, since the real error is
         *   most likely just "missing semicolon" 
         */
        break;

    default:
        /* log the error */
        G_tok->log_error_curtok(TCERR_CONT_REQ_LABEL);
        break;
    }

    /* parse the required terminating semicolon */
    if (parse_req_sem())
    {
        *err = TRUE;
        return 0;
    }

    /* return the 'continue' node */
    return cont_stm;
}

/* 
 *   Parse a 'while' statement 
 */
CTPNStm *CTcParser::parse_while(int *err)
{
    CTcPrsNode *expr;
    CTPNStm *body_stm;
    CTPNStmWhile *while_stm;
    CTPNStmEnclosing *old_enclosing;
    
    /* skip the 'while' and check for the open paren */
    if (G_tok->next() == TOKT_LPAR)
    {
        /* skip the paren */
        G_tok->next();
    }
    else
    {
        /* 
         *   log an error, and proceed on the assumption that the paren
         *   was simply left out and the statement is otherwise
         *   well-formed 
         */
        G_tok->log_error_curtok(TCERR_REQ_WHILE_LPAR);
    }

    /* parse the condition expression */
    expr = parse_cond_expr();
    if (expr == 0)
    {
        *err = TRUE;
        return 0;
    }

    /* create the 'while' statement node */
    while_stm = new CTPNStmWhile(expr, enclosing_stm_);

    /* check for the close paren */
    if (G_tok->cur() == TOKT_RPAR)
    {
        /* skip the paren */
        G_tok->next();
    }
    else
    {
        /* log an error, and continue from here */
        G_tok->log_error_curtok(TCERR_REQ_WHILE_RPAR);
    }

    /* set the 'while' to enclose its body */
    old_enclosing = set_enclosing_stm(while_stm);

    /* parse the loop body */
    body_stm = parse_stm(err, 0, FALSE);

    /* restore the old enclosing statement */
    set_enclosing_stm(old_enclosing);

    /* give up on error */
    if (*err)
        return 0;

    /* set the body */
    while_stm->set_body(body_stm);

    /* that's it - build and return the 'while' node */
    return while_stm;
}

/* 
 *   Parse a 'do-while' statement 
 */
CTPNStm *CTcParser::parse_do_while(int *err)
{
    CTPNStm *body_stm;
    CTcPrsNode *expr;
    CTPNStmDoWhile *do_stm;
    CTPNStmEnclosing *old_enclosing;
    
    /* create the statement object */
    do_stm = new CTPNStmDoWhile(enclosing_stm_);

    /* skip the 'do' keyword */
    G_tok->next();

    /* set the 'do' to be the enclosing statement */
    old_enclosing = set_enclosing_stm(do_stm);

    /* parse the loop body */
    body_stm = parse_stm(err, 0, FALSE);

    /* restore the enclosing statement */
    set_enclosing_stm(old_enclosing);

    /* return on failure */
    if (*err)
        return 0;

    /* require the 'while' keyword */
    if (G_tok->cur() == TOKT_WHILE)
    {
        /* skip the 'while' */
        G_tok->next();
    }
    else
    {
        /* 
         *   no 'while' keyword - there's no obvious way to correct this,
         *   so simply ignore the 'do' statement and keep going from here,
         *   on the assumption that they inadvertantly started a new
         *   statement without finishing the 'do' 
         */
        G_tok->log_error_curtok(TCERR_REQ_DO_WHILE);
    }

    /* require the open paren */
    if (G_tok->cur() == TOKT_LPAR)
        G_tok->next();
    else
        G_tok->log_error_curtok(TCERR_REQ_WHILE_LPAR);

    /* parse the expression */
    expr = parse_cond_expr();
    if (expr == 0)
    {
        *err = TRUE;
        return 0;
    }

    /* require the close paren */
    if (G_tok->cur() == TOKT_RPAR)
        G_tok->next();
    else
        G_tok->log_error_curtok(TCERR_REQ_WHILE_RPAR);

    /* set the condition expression and body in the 'do' node */
    do_stm->set_cond(expr);
    do_stm->set_body(body_stm);

    /* 
     *   remember the location of the 'while' part, since this part
     *   generates code 
     */
    do_stm->set_while_pos(G_tok->get_last_desc(), G_tok->get_last_linenum());

    /* parse the required closing semicolon */
    if (parse_req_sem())
    {
        *err = TRUE;
        return 0;
    }

    /* return the new 'do-while' node */
    return do_stm;
}

/* 
 *   Parse a 'switch' statement 
 */
CTPNStm *CTcParser::parse_switch(int *err)
{
    CTcPrsNode *expr;
    CTPNStmSwitch *switch_stm;
    CTPNStm *body_stm;
    int skip;
    int unreachable_error_shown;
    CTPNStmEnclosing *old_enclosing;
    
    /* create the switch statement object */
    switch_stm = new CTPNStmSwitch(enclosing_stm_);

    /* skip the 'switch' and check for the left paren */
    if (G_tok->next() == TOKT_LPAR)
    {
        /* skip the left paren */
        G_tok->next();
    }
    else
    {
        /* log an error, and assume the paren is simply missing */
        G_tok->log_error_curtok(TCERR_REQ_SWITCH_LPAR);
    }

    /* parse the controlling expression */
    expr = parse_expr();
    if (expr == 0)
    {
        *err = TRUE;
        return 0;
    }

    /* set expression in the switch statement node */
    switch_stm->set_expr(expr);

    /* check for and skip the close paren */
    if (G_tok->cur() == TOKT_RPAR)
    {
        /* the right paren is present - skip it */
        G_tok->next();
    }
    else
    {
        /* log an error, and keep going from here */
        G_tok->log_error_curtok(TCERR_REQ_SWITCH_RPAR);
    }

    /* check for and skip the brace */
    if (G_tok->cur() == TOKT_LBRACE)
    {
        /* it's there - skip it */
        G_tok->next();
    }
    else
    {
        /* 
         *   log an error, and keep going on the assumption that the brace
         *   is simply missing but the switch body is otherwise correct 
         */
        G_tok->log_error_curtok(TCERR_REQ_SWITCH_LBRACE);
    }

    /* 
     *   The first thing in the switch body must be a 'case', 'default',
     *   or closing brace.  Other statements preceding the first 'case' or
     *   'default' label within the switch body are not allowed, because
     *   they would be unreachable.  Keep skipping statements until we get
     *   to one of these.  
     */
    for (skip = TRUE, unreachable_error_shown = FALSE ; skip ; )
    {
        /* see what we have */
        switch(G_tok->cur())
        {
        case TOKT_CASE:
        case TOKT_DEFAULT:
        case TOKT_RBRACE:
            /* this is what we're looking for */
            skip = FALSE;
            break;

        case TOKT_EOF:
            /* end of file within the switch - log an error */
            G_tok->log_error(TCERR_EOF_IN_SWITCH);

            /* return failure */
            *err = TRUE;
            return 0;

        default:
            /* 
             *   for anything else, log an error explaining that the code
             *   is unreachable - do this only once, no matter how many
             *   unreachable statements precede the first case label 
             */
            if (!unreachable_error_shown && !G_prs->get_syntax_only())
            {
                /* show the error */
                G_tok->log_error(TCERR_UNREACHABLE_CODE_IN_SWITCH);
                
                /* 
                 *   note that we've shown the error, so we don't show it
                 *   again if more unreachable statements follow
                 */
                unreachable_error_shown = TRUE;
            }

            /* parse (and ignore) this statement */
            parse_stm(err, switch_stm, FALSE);
            if (*err != 0)
                return 0;

            /* keep looking for the first label */
            break;
        }
    }

    /* the 'switch' is the enclosing statement for children */
    old_enclosing = set_enclosing_stm(switch_stm);

    /* parse the switch body */
    body_stm = parse_compound(err, FALSE, switch_stm, FALSE);

    /* restore the enclosing statement */
    set_enclosing_stm(old_enclosing);

    /* if we failed to parse the compound statement, give up */
    if (*err)
        return 0;

    /* connect the switch to its body */
    switch_stm->set_body(body_stm);

    /* return the switch statement node */
    return switch_stm;
}

/*
 *   Parse a 'case' label 
 */
CTPNStm *CTcParser::parse_case(int *err, CTPNStmSwitch *enclosing_switch)
{
    CTcPrsNode *expr;
    CTPNStm *stm;
    CTPNStmCase *case_stm;
    
    /* skip the 'case' keyword */
    G_tok->next();

    /* create the 'case' statement node */
    case_stm = new CTPNStmCase();

    /* parse the expression */
    expr = parse_expr();
    if (expr == 0)
    {
        *err = TRUE;
        return 0;
    }

    /* store the expression in the case statement node */
    case_stm->set_expr(expr);

    /* require the colon */
    if (G_tok->cur() == TOKT_COLON)
    {
        /* skip the colon */
        G_tok->next();
    }
    else
    {
        /* log the error */
        G_tok->log_error_curtok(TCERR_REQ_CASE_COLON);
    }

    /* 
     *   parse the labeled statement - it's directly within this same
     *   enclosing switch, because a case label doesn't create a new
     *   expression nesting level (hence another 'case' label immediately
     *   following without an intervening statement is perfectly valid) 
     */
    stm = parse_stm(err, enclosing_switch, FALSE);

    /* set the statement in the case node */
    case_stm->set_stm(stm);

    /* count the 'case' label in the 'switch' node */
    enclosing_switch->inc_case_cnt();

    /* return the case node */
    return case_stm;
}

/*
 *   Parse a 'default' label 
 */
CTPNStm *CTcParser::parse_default(int *err, CTPNStmSwitch *enclosing_switch)
{
    CTPNStm *stm;
    CTPNStmDefault *default_stm;

    /* create the 'default' statement node */
    default_stm = new CTPNStmDefault();

    /* 
     *   if the enclosing 'switch' already has a 'default' label, it's an
     *   error; continue anyway, since we still want to finish parsing the
     *   syntax 
     */
    if (enclosing_switch->get_has_default())
        G_tok->log_error(TCERR_DEFAULT_REDEF);

    /* mark the switch as having a 'default' case */
    enclosing_switch->set_has_default();

    /* skip the 'default' node, and require the colon */
    if (G_tok->next() == TOKT_COLON)
    {
        /* skip the colon */
        G_tok->next();
    }
    else
    {
        /* 
         *   log an error, and keep going, assuming that the token was
         *   accidentally omitted 
         */
        G_tok->log_error_curtok(TCERR_REQ_DEFAULT_COLON);
    }

    /* 
     *   parse the labeled statement - it's directly within this same
     *   enclosing switch, because a 'default' label doesn't create a new
     *   expression nesting level (hence another 'case' label immediately
     *   following without an intervening statement is perfectly valid) 
     */
    stm = parse_stm(err, enclosing_switch, FALSE);

    /* set the statement in the 'default' node */
    default_stm->set_stm(stm);

    /* return the 'default' node */
    return default_stm;
}

/* 
 *   Parse a 'goto' statement 
 */
CTPNStm *CTcParser::parse_goto(int *err)
{
    CTPNStmGoto *goto_stm;
    
    /* skip the 'goto' keyword, and demand that a symbol follows */
    if (G_tok->next() == TOKT_SYM)
    {
        /* create the parse node for the 'goto' statement */
        goto_stm = new CTPNStmGoto(G_tok->getcur()->get_text(),
                                   G_tok->getcur()->get_text_len());
        
        /* skip the symbol */
        G_tok->next();
    }
    else
    {
        /* log the error */
        G_tok->log_error(TCERR_GOTO_REQ_LABEL);

        /* no statement */
        goto_stm = 0;
    }

    /* parse the required semicolon */
    if (parse_req_sem())
    {
        *err = TRUE;
        return 0;
    }

    /* return the statement node */
    return goto_stm;
}

/* 
 *   Parse a 'try-catch-finally' statement 
 */
CTPNStm *CTcParser::parse_try(int *err)
{
    CTPNStmTry *try_stm;
    CTPNStmEnclosing *old_enclosing;
    CTPNStm *body_stm;

    /* create the 'try' statement node */
    try_stm = new CTPNStmTry(enclosing_stm_);

    /* 
     *   the 'try' is the enclosing statement for the duration of its
     *   protected code block 
     */
    old_enclosing = set_enclosing_stm(try_stm);
    
    /* skip the 'try' */
    G_tok->next();

    /* parse the body of the 'try' block */
    body_stm = parse_stm(err, 0, FALSE);

    /* restore the previous enclosing statement */
    set_enclosing_stm(old_enclosing);

    /* if parsing the body failed, stop now */
    if (*err)
        return 0;

    /* add the body to the 'try' */
    try_stm->set_body_stm(body_stm);

    /* 
     *   check for 'catch' clauses - there could be several, so keep going
     *   until we stop seeing 'catch' keywords 
     */
    while (G_tok->cur() == TOKT_CATCH)
    {
        int catch_has_err;
        CTPNStm *catch_body;
        CTPNStmCatch *catch_stm;
        tcprs_scope_t scope_data;

        /* create a local scope for the 'catch' clause */
        enter_scope(&scope_data);
        create_scope_local_symtab();
        
        /* create the 'catch' statement node */
        catch_stm = new CTPNStmCatch();

        /* 
         *   set the 'catch' clause's source position independently of the
         *   overall 'try' statement, so that the debugger can track entry
         *   into this clause 
         */
        catch_stm->set_source_pos(G_tok->get_last_desc(),
                                  G_tok->get_last_linenum());

        /* presume we'll parse this successfully */
        catch_has_err = FALSE;
            
        /* skip the 'catch' keyword and check for the left paren */
        if (G_tok->next() == TOKT_LPAR)
        {
            /* skip the paren */
            G_tok->next();
        }
        else
        {
            /* log the error */
            G_tok->log_error_curtok(TCERR_REQ_CATCH_LPAR);
        }

        /* get the exception class token */
        if (G_tok->cur() == TOKT_SYM)
        {
            /* set the class name in the 'catch' clause */
            catch_stm->set_exc_class(G_tok->getcur());

            /* move on */
            G_tok->next();
        }
        else
        {
            /* flag the problem */
            G_tok->log_error_curtok(TCERR_REQ_CATCH_CLASS);

            /* unless this is a close paren, skip the errant token */
            if (G_tok->cur() != TOKT_RPAR)
                G_tok->next();

            /* note the error */
            catch_has_err = TRUE;
        }

        /* get the variable name token */
        if (G_tok->cur() == TOKT_SYM)
        {
            CTcSymLocal *var;
            
            /* 
             *   create the local variable - note that this variable is
             *   implicitly assigned when the 'catch' clause is entered, so
             *   mark it as initially assigned; we don't care if the
             *   variable is ever used, so also mark it as used 
             */
            var = local_symtab_->add_local(G_tok->getcur()->get_text(),
                                           G_tok->getcur()->get_text_len(),
                                           alloc_local(),
                                           FALSE, TRUE, TRUE);
            
            /* set the variable in the 'catch' clause */
            if (!catch_has_err)
                catch_stm->set_exc_var(var);

            /* move on */
            G_tok->next();
        }
        else
        {
            /* flag the problem */
            G_tok->log_error_curtok(TCERR_REQ_CATCH_VAR);

            /* unless this is a close paren, skip the errant token */
            if (G_tok->cur() != TOKT_RPAR)
                G_tok->next();

            /* note the error */
            catch_has_err = TRUE;
        }

        /* check for the close paren */
        if (G_tok->cur() == TOKT_RPAR)
        {
            /* skip the paren */
            G_tok->next();
        }
        else
        {
            /* 
             *   log the error and continue, assuming that the paren is
             *   simply missing and things are otherwise okay 
             */
            G_tok->log_error_curtok(TCERR_REQ_CATCH_RPAR);
        }

        /* 
         *   parse the 'catch' statement block - we've already established
         *   a special scope for the 'catch' block, so don't start a new
         *   scope if the block contains a compound statement 
         */
        catch_body = parse_stm(err, 0, TRUE);

        /* leave the special 'catch' scope */
        leave_scope(&scope_data);

        /* if the statement block failed, give up now */
        if (*err)
            return 0;

        /* add the 'catch' clause to the 'try' if we were successful */
        if (!catch_has_err)
        {
            /* set the 'catch' node's body */
            catch_stm->set_body(catch_body);

            /* set the local scope in the 'catch' */
            catch_stm->set_symtab(local_symtab_);
            
            /* add the 'catch' to the 'try' */
            try_stm->add_catch(catch_stm);
        }
    }

    /* check for a 'finally' clause */
    if (G_tok->cur() == TOKT_FINALLY)
    {
        CTPNStmFinally *fin_stm;
        CTPNStm *fin_body;
        tcprs_scope_t scope_data;
        CTPNStmEnclosing *old_enclosing;

        /* 
         *   the locals we allocate for the 'finally' are in the finally's
         *   own scope - the slots can be reused later 
         */
        enter_scope(&scope_data);

        /* create the 'finally' node */
        fin_stm = new CTPNStmFinally(enclosing_stm_,
                                     alloc_local(), alloc_local());

        /* 
         *   set the 'finally' clause's source position - we want this
         *   clause to have its own source position independent of the
         *   'try' statement of which it is a part, so that the debugger
         *   can keep track of entry into this clause's generated code 
         */
        fin_stm->set_source_pos(G_tok->get_last_desc(),
                                G_tok->get_last_linenum());

        /* skip the 'finally' keyword */
        G_tok->next();

        /* set the 'finally' to enclose its body */
        old_enclosing = set_enclosing_stm(fin_stm);

        /* parse the 'finally' statement block */
        fin_body = parse_stm(err, 0, FALSE);

        /* set the 'finally' block's closing position */
        fin_stm->set_end_pos(fin_body->get_end_desc(),
                             fin_body->get_end_linenum());

        /* restore the enclosing statement */
        set_enclosing_stm(old_enclosing);

        /* we're done with the special scope */
        leave_scope(&scope_data);

        /* if that failed, give up now */
        if (*err)
            return 0;

        /* set the 'finally' node's body */
        fin_stm->set_body(fin_body);

        /* add the 'finally' to the 'try' */
        try_stm->set_finally(fin_stm);
    }

    /* make sure we have at least one 'catch' or 'finally' clause */
    if (!try_stm->has_catch_or_finally())
        try_stm->log_error(TCERR_TRY_WITHOUT_CATCH);

    /* return the 'try' statement node */
    return try_stm;
}

/* 
 *   Parse a 'throw' statement 
 */
CTPNStm *CTcParser::parse_throw(int *err)
{
    CTcPrsNode *expr;
    CTPNStmThrow *throw_stm;
    
    /* skip the 'throw' keyword */
    G_tok->next();

    /* parse the expression to be thrown */
    expr = parse_expr();
    if (expr == 0)
    {
        *err = TRUE;
        return 0;
    }

    /* create the statement node */
    throw_stm = new CTPNStmThrow(expr);

    /* require a terminating semicolon */
    if (parse_req_sem())
    {
        *err = TRUE;
        return 0;
    }

    /* return the statement node */
    return throw_stm;
}

/* ------------------------------------------------------------------------ */
/*
 *   Unary operator parsing 
 */

/* callback context for local symbol enumeration */
struct enum_for_anon_ctx
{
    /* new local symbol table for anonymous function scope */
    CTcPrsSymtab *symtab;

    /* number of context locals */
    int cnt;
};

/*
 *   local symbol table enumeration callback for anonymous function setup 
 */
void CTcPrsOpUnary::enum_for_anon(void *ctx0, CTcSymbol *sym)
{
    enum_for_anon_ctx *ctx = (enum_for_anon_ctx *)ctx0;
    CTcSymbol *new_sym;

    /* 
     *   If this symbol is already in our table, another symbol from an
     *   enclosed scope hides it, so ignore this one.  Note that we're only
     *   interested in the symbols defined directly in our table - we hide
     *   symbols defined in the enclosing global scope, so we don't care if
     *   they're already defined there.  
     */
    if (ctx->symtab->find_direct(sym->get_sym(), sym->get_sym_len()) != 0)
        return;

    /* create a context-variable copy */
    new_sym = sym->new_ctx_var();

    /* if we got a new symbol, add it to the new symbol table */
    if (new_sym != 0)
        ctx->symtab->add_entry(new_sym);
}

/*
 *   local symbol table enumeration callback for anonymous function -
 *   follow-up - mark the code body as having a context if needed 
 */
void CTcPrsOpUnary::enum_for_anon2(void *, CTcSymbol *sym)
{
    /* ask the symbol to apply the necessary conversion */
    sym->finish_ctx_var_conv();
}

/*
 *   Parse an anonymous function 
 */
CTcPrsNode *CTcPrsOpUnary::parse_anon_func(int short_form)
{
    CTPNCodeBody *code_body;
    int err;
    int has_retval;
    CTcPrsSymtab *new_lcl_symtab;
    CTcPrsSymtab *inner_lcl_symtab;
    CTcPrsSymtab *tab;
    enum_for_anon_ctx ctx;
    tcprs_codebodytype cb_type;

    /* 
     *   our code body type is either an anonymous function or a short
     *   anonymous function 
     */
    cb_type = (short_form ? TCPRS_CB_SHORT_ANON_FN : TCPRS_CB_ANON_FN);
    
    /* skip the initial token */
    G_tok->next();

    /* 
     *   Create a new local symbol table to represent the enclosing scope of
     *   the new nested scope.  Unlike most nested scopes, we can't simply
     *   plug in the current scope's symbol table - instead, we have to build
     *   a special representation of the enclosing scope to handle the
     *   "closure" behavior.  The enclosing scope's local variable set
     *   effectively becomes an object rather than a simple stack frame.  The
     *   enclosing lexical scope and the anonymous function then both
     *   reference the shared locals object.
     *   
     *   This synthesized enclosing scope will directly represent all of the
     *   nested scopes up but not including to the root global scope.  The
     *   global scope doesn't need the special closure representation, since
     *   it's already shared among all lexical scopes anyway.  Since we're
     *   not linking in to the enclosing scope list the way we normally
     *   would, we need to explicitly link in the global scope.  To do this,
     *   we can simply make the global scope the enclosing scope of our
     *   synthesized outer scope table.  
     */
    new_lcl_symtab = new CTcPrsSymtab(G_prs->get_global_symtab());

    /* set up the enumeration callback context */
    ctx.symtab = new_lcl_symtab;
    ctx.cnt = 0;

    /* 
     *   fill the new local symbol table with the inherited local symbols
     *   from the current local scope and any enclosing scopes - but stop
     *   when we reach the global scope, since this is already shared by all
     *   scopes and doesn't need any special closure representation 
     */
    for (tab = G_prs->get_local_symtab() ;
         tab != 0 && tab != G_prs->get_global_symtab() ;
         tab = tab->get_parent())
    {
        /* enumerate entries in this table */
        tab->enum_entries(&enum_for_anon, &ctx);
    }

    /* 
     *   create another local symbol table, this time nested within the
     *   table containing the locals shared from the enclosing scope -
     *   this one will contain any formals defined for the anonymous
     *   function, which hide inherited locals from the enclosing scope 
     */
    inner_lcl_symtab = new CTcPrsSymtab(new_lcl_symtab);

    /* 
     *   Parse the code body.  Give the code body the same defining object
     *   and property that we have in our own code body, so that it can
     *   reference 'self' if we can, and use 'inherited' if we can.  
     */
    err = 0;
    code_body = G_prs->parse_nested_code_body(FALSE,
                                              G_prs->is_self_valid(),
                                              0, 0, 0, 0,
                                              &has_retval, &err,
                                              inner_lcl_symtab, cb_type);

    /* if that failed, return failure */
    if (code_body == 0 || err != 0)
        return 0;

    /* 
     *   If the nested code body references 'self' or the full method
     *   context, then so does the enclosing code body, and we need these
     *   variables in the local context for the topmost enclosing code body.
     */
    if (code_body->self_referenced())
    {
        G_prs->set_self_referenced(TRUE);
        G_prs->set_local_ctx_needs_self(TRUE);
    }
    if (code_body->full_method_ctx_referenced())
    {
        G_prs->set_full_method_ctx_referenced(TRUE);
        G_prs->set_local_ctx_needs_full_method_ctx(TRUE);
    }

    /*
     *   Enumerate all of the entries in our scope once again - this time,
     *   we want to determine if there are any variables that were not
     *   previously referenced from anonymous functions but have been now;
     *   we need to convert all such variables to context locals.  
     */
    for (tab = G_prs->get_local_symtab() ;
         tab != 0 && tab != G_prs->get_global_symtab() ;
         tab = tab->get_parent())
    {
        /* enumerate entries in this table */
        tab->enum_entries(&enum_for_anon2, &ctx);
    }

    /* 
     *   if there's a 'self' object, and we referenced 'self' or the full
     *   method context in the nested code body, we'll definitely need a
     *   local context, so make sure we have one initialized even if we don't
     *   have any local variables shared 
     */
    if (G_prs->is_self_valid()
        && (code_body->self_referenced()
            || code_body->full_method_ctx_referenced()))
    {
        /* initialize a local context here, in the enclosing level */
        G_prs->init_local_ctx();
    }

    /* 
     *   add the code body to the parser's master list of nested top-level
     *   statements for the current program 
     */
    G_prs->add_nested_stm(code_body);

    /* return a new anonymous function node */
    return new CTPNAnonFunc(code_body, has_retval);
}


/* ------------------------------------------------------------------------ */
/*
 *   Generic statement node 
 */

/*
 *   Add a debugging line record for the current statement.  Call this
 *   just before generating instructions for the statement; we'll create a
 *   line record associating the current byte-code location with our
 *   source file and line number.  
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
 *   compound statement base class 
 */

/*
 *   instantiate 
 */
CTPNStmCompBase::CTPNStmCompBase(CTPNStm *first_stm, CTcPrsSymtab *symtab)
{
    /* remember the first statement in our statement list */
    first_stm_ = first_stm;
    
    /* remember our local symbol table */
    symtab_ = symtab;
    
    /* 
     *   presume we don't have our own private symbol table, but will
     *   simply use our parent's 
     */
    has_own_scope_ = FALSE;

    /* 
     *   remember the source location of the closing brace, which should
     *   be the current location when we're instantiated 
     */
    end_desc_ = G_tok->get_last_desc();
    end_linenum_ = G_tok->get_last_linenum();
}

/*
 *   fold constants - we simply fold constants in each of our statements 
 */
CTcPrsNode *CTPNStmCompBase::fold_constants(class CTcPrsSymtab *symtab)
{
    CTPNStm *cur;
    
    /* iterate through our statements and fold constants in each one */
    for (cur = first_stm_ ; cur != 0 ; cur = cur->get_next_stm())
    {
        /* 
         *   set this statement's source line location to be the current
         *   location for error reporting 
         */
        G_tok->set_line_info(cur->get_source_desc(),
                             cur->get_source_linenum());

        /* 
         *   Fold constants, using the enclosing symbol table (not the
         *   local symbol table - during code generation we can resolve
         *   only to the global scope, since local scope symbols must
         *   always be resolved during parsing).  We assume that statement
         *   nodes are never replaced during constant folding, so we
         *   ignore the result of this call and keep our original
         *   statement list entry.  
         */
        cur->fold_constants(symtab);
    }

    /* 
     *   although nodes within our subtree might have been changed, this
     *   compound statement itself is unchanged by constant folding 
     */
    return this;
}

/*
 *   Generate code 
 */
void CTPNStmCompBase::gen_code(int, int)
{
    CTPNStm *cur;
    CTcPrsSymtab *old_frame;

    /* set my local scope symbol frame, if necessary */
    old_frame = G_cs->set_local_frame(symtab_);

    /* set the code location for the start of the group */
    add_debug_line_rec();

    /* 
     *   iterate through our statements and generate code for each in
     *   sequence 
     */
    for (cur = first_stm_ ; cur != 0 ; cur = cur->get_next_stm())
    {
        /* 
         *   set this statement's source line location to be the current
         *   location for error reporting 
         */
        G_tok->set_line_info(cur->get_source_desc(),
                             cur->get_source_linenum());

        /* 
         *   Generate code for the statement.  Note that we generate in
         *   the scope of the enclosing symbol table, because we never
         *   resolve symbols to local scope during code generation - local
         *   scope symbols must be resolved during parsing, because these
         *   symbols must always be declared before first being used 
         *   
         *   We have no use for the results of any expressions, so discard
         *   = true; we don't care about the form of any logical operator
         *   results, so use the looser "for condition" rules 
         */
        cur->gen_code(TRUE, TRUE);
    }

    /* set the source location for the end of the group */
    add_debug_line_rec(end_desc_, end_linenum_);

    /* check for unreferenced local variables */
    if (symtab_ != 0 && has_own_scope_)
    {
        /* 
         *   set our line information to be current again, so that any
         *   unreferenced local errors are reported at the start of the
         *   compound statement 
         */
        G_tok->set_line_info(get_source_desc(),
                             get_source_linenum());
    }

    /* restore the enclosing local frame */
    old_frame = G_cs->set_local_frame(old_frame);
}

/*
 *   Chart the control flow through the statement list
 */
unsigned long CTPNStmCompBase::get_control_flow(int warn) const
{
    CTPNStm *cur;
    unsigned long flags;

    /* 
     *   presume we will not find a 'throw' or 'return', and that we'll be
     *   able to reach the next statement after the list 
     */
    flags = TCPRS_FLOW_NEXT;

    /* 
     *   iterate through the statements in our list, since they'll be
     *   executed in the same sequence at run-time 
     */
    for (cur = first_stm_ ; cur != 0 ; cur = cur->get_next_stm())
    {
        unsigned long flags1;
        
        /* get this statement's control flow characteristics */
        flags1 = cur->get_control_flow(warn);

        /* 
         *   OR each of 'throws', 'returns void', and 'returns value',
         *   since anything a reachable statement does to return or throw,
         *   the overall list can do to leave.  Likewise, the 'break',
         *   'goto', and 'continue' flags push up into the enclosing
         *   statement.  
         */
        flags |= (flags1 & (TCPRS_FLOW_THROW
                            | TCPRS_FLOW_RET_VOID
                            | TCPRS_FLOW_RET_VAL
                            | TCPRS_FLOW_GOTO
                            | TCPRS_FLOW_BREAK
                            | TCPRS_FLOW_CONT));

        /* 
         *   if this statement can't continue to the next statement, the
         *   remainder of this compound statement is unreachable, unless
         *   it has a code label 
         */
        if (!(flags1 & TCPRS_FLOW_NEXT))
        {
            CTPNStm *nxt;

            /*
             *   we can't reach the next statement; unless we find a label
             *   at some point after this, we won't be able to reach any
             *   of the remaining statements, hence we won't be able to
             *   reach the statement after our list 
             */
            flags &= ~TCPRS_FLOW_NEXT;
            
            /* 
             *   If another statement follows, check to see if it's
             *   reachable by label, because it's not reachable by any
             *   other means
             */
            nxt = cur->get_next_stm();
            if (nxt != 0)
            {
                /* 
                 *   check to see if this statement has a label; if not,
                 *   warn that it is unreachable 
                 */
                if (warn
                    && !nxt->has_code_label()
                    && !G_prs->get_syntax_only())
                    nxt->log_warning(TCERR_UNREACHABLE_CODE);

                /* skip ahead until we find something with a label */
                while (nxt != 0 && !nxt->has_code_label())
                {
                    /* remember the previous statement */
                    cur = nxt;

                    /* move on to the next statement */
                    nxt = nxt->get_next_stm();
                }

                /* 
                 *   if we found a reachable statement again, we might be
                 *   able to continue from the end of the list after all 
                 */
                if (nxt != 0)
                    flags |= TCPRS_FLOW_NEXT;
            }
        }
    }

    /* return the result */
    return flags;
}

/* ------------------------------------------------------------------------ */
/*
 *   expression statement base class 
 */

/*
 *   fold constants
 */
CTcPrsNode *CTPNStmExprBase::fold_constants(class CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* 
     *   fold constants in our expression, replacing our expression with
     *   the folded version 
     */
    expr_ = expr_->fold_constants(symtab);

    /* 
     *   although our expression subtree might have been changed, this
     *   compound statement itself is unchanged by constant folding 
     */
    return this;
}

/*
 *   Generate code 
 */
void CTPNStmExprBase::gen_code(int, int)
{
    /* add a debug line record for the statement */
    add_debug_line_rec();

    /* 
     *   generate code in our expression - the result will not be used in any
     *   further calculation, so discard it; and since we don't care about
     *   the return value, use the looser "for condition" rules for any
     *   top-level logical operators in the expression so that we don't
     *   bother applying any extra conversions to a value we're just
     *   discarding anyway 
     */
    expr_->gen_code(TRUE, TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   'if' statement 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmIfBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* 
     *   fold constants in the condition, replacing the expression with
     *   the folded expression 
     */
    cond_expr_ = cond_expr_->fold_constants(symtab);

    /* fold constants in the substatements */
    if (then_part_ != 0)
        then_part_ = (CTPNStm *)then_part_->fold_constants(symtab);
    if (else_part_ != 0)
        else_part_ = (CTPNStm *)else_part_->fold_constants(symtab);

    /* we are not directly changed by this operation */
    return this;
}

/*
 *   Chart the control flow through the conditional 
 */
unsigned long CTPNStmIfBase::get_control_flow(int warn) const
{
    unsigned long flags1;
    unsigned long flags2;
    
    /* 
     *   if my condition is constant, our control flow is determined
     *   entirely by the branch we'll take 
     */
    if (cond_expr_ != 0 && cond_expr_->is_const())
    {
        CTPNStm *part;
        
        /* 
         *   We have a constant expression - if it's true, our control
         *   flow is determined by the 'then' part, otherwise it's
         *   determines by the 'else' part. 
         */
        part = (cond_expr_->get_const_val()->get_val_bool()
                ? then_part_ : else_part_);

        /* 
         *   if the part determining the control flow is a null statement,
         *   we'll simply continue to the next statement; otherwise, the
         *   result is the control flow of the determining statement 
         */
        if (part == 0)
        {
            /* it's a null statement, which merely continues */
            return TCPRS_FLOW_NEXT;
        }
        else
        {
            /* our control flow is this statement's control flow */
            return part->get_control_flow(warn);
        }
    }
    
    /*
     *   The control flow through the 'if' is a combination of the control
     *   flow through the two parts.  If one of the parts is a void
     *   statement, it continues to the next statement after the 'if'.  
     */

    /* check the 'then' part if present */
    flags1 = (then_part_ != 0
              ? then_part_->get_control_flow(warn) : TCPRS_FLOW_NEXT);

    /* check the 'else' part if present */
    flags2 = (else_part_ != 0
              ? else_part_->get_control_flow(warn) : TCPRS_FLOW_NEXT);

    /* combine the parts to get the possible control flows */
    return flags1 | flags2;
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
     *   iterate again, this time checking local symbol tables - we must
     *   wait to do this until after we've generated all of the code,
     *   because local symbol tables can be cross-referenced in some cases
     *   (in particular, when locals are shared through contexts, such as
     *   with anonymous functions) 
     */
    for (cur = first_stm_ ; cur != 0 ; cur = cur->get_next_stm_top())
        cur->check_locals();
}

/* ------------------------------------------------------------------------ */
/*
 *   "for" statement node
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmForBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* 
     *   fold constants in each of our expressions, replacing the
     *   expressions with the folded versions
     */
    if (init_expr_ != 0)
        init_expr_ = init_expr_->fold_constants(symtab);
    if (cond_expr_ != 0)
        cond_expr_ = cond_expr_->fold_constants(symtab);
    if (reinit_expr_ != 0)
        reinit_expr_ = reinit_expr_->fold_constants(symtab);

    /* fold constants in the body statement */
    if (body_stm_ != 0)
        body_stm_ = (CTPNStm *)body_stm_->fold_constants(symtab);

    /* we are not directly changed by this operation */
    return this;
}

/*
 *   Chart the control flow through the loop
 */
unsigned long CTPNStmForBase::get_control_flow(int warn) const
{
    unsigned long flags;

    /*
     *   If we have a condition with a constant false value, and there's
     *   either a reinitialization expression or a loop body, warn about
     *   the unreachable code.  If the body is directly reachable via a
     *   code label, we don't need this warning.  
     */
    if (cond_expr_ != 0
        && cond_expr_->is_const()
        && !cond_expr_->get_const_val()->get_val_bool()
        && (reinit_expr_ != 0 || body_stm_ != 0)
        && (body_stm_ != 0 && !body_stm_->has_code_label())
        && !G_prs->get_syntax_only())
    {
        /* log a warning if desired */
        if (warn)
            log_warning(TCERR_FOR_COND_FALSE);

        /* this will simply continue to the next statement */
        return TCPRS_FLOW_NEXT;
    }

    /* check for a body */
    if (body_stm_ != 0)
    {
        /* determine how our body can exit */
        flags = body_stm_->get_control_flow(warn);
    }
    else
    {
        /* 
         *   We have no body, so it's entirely up to the loop - we'll
         *   evaluate those parameters below, but for now clear all flags 
         */
        flags = 0;
    }

    /* 
     *   ignore any 'next' flag that comes out of the body, because 'next'
     *   from the last statement in the body actually takes us back to the
     *   top of the loop 
     */
    flags &= ~TCPRS_FLOW_NEXT;

    /* 
     *   if there's a 'break' in the body, we can continue to the next
     *   statement, since a break in the body takes us to the statement
     *   after the 'for' 
     */
    if ((flags & TCPRS_FLOW_BREAK) != 0)
        flags |= TCPRS_FLOW_NEXT;

    /* 
     *   if we have a condition, and either the condition is a constant
     *   false value or has a non-constant value, assume that the
     *   condition can possibly become false and hence that the loop can
     *   exit, and hence that the next statement is reachable 
     */
    if (cond_expr_ != 0
        && (!cond_expr_->is_const()
            || (cond_expr_->is_const()
                && !cond_expr_->get_const_val()->get_val_bool())))
        flags |= TCPRS_FLOW_NEXT;

    /* 
     *   clear the 'break' and 'continue' flags, since we capture them in
     *   our own scope - if our child breaks or continues, it won't affect
     *   the "for" loop's container 
     */
    flags &= ~(TCPRS_FLOW_BREAK | TCPRS_FLOW_CONT);

    /* return the flags */
    return flags;
}


/* ------------------------------------------------------------------------ */
/*
 *   "foreach" statement node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmForeachBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* 
     *   fold constants in each of our expressions, replacing the
     *   expressions with the folded versions 
     */
    if (iter_expr_ != 0)
        iter_expr_ = iter_expr_->fold_constants(symtab);
    if (coll_expr_ != 0)
        coll_expr_ = coll_expr_->fold_constants(symtab);

    /* fold constants in the body statement */
    if (body_stm_ != 0)
        body_stm_ = (CTPNStm *)body_stm_->fold_constants(symtab);
    
    /* we are not directly changed by this operation */
    return this;
}

/*
 *   Chart the control flow through the loop 
 */
unsigned long CTPNStmForeachBase::get_control_flow(int warn) const
{
    unsigned long flags;
    
    /* check for a body */
    if (body_stm_ != 0)
    {
        /* determine how our body can exit */
        flags = body_stm_->get_control_flow(warn);

        /* 
         *   add in the 'next' flag, because collection iterations always
         *   terminate 
         */
        flags |= TCPRS_FLOW_NEXT;

        /* 
         *   clear the 'break' and 'continue' flags, since we capture them
         *   in our own scope - if our child breaks or continues, it won't
         *   affect the container of the 'foreach' itself
         */
        flags &= ~(TCPRS_FLOW_BREAK | TCPRS_FLOW_CONT);
    }
    else
    {
        /* 
         *   we have no body, so it's entirely up to the loop, which
         *   simply iterates over the list and goes to the next statement 
         */
        flags = TCPRS_FLOW_NEXT;
    }

    /* return the flags */
    return flags;
}

/* ------------------------------------------------------------------------ */
/*
 *   "while" statement node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmWhileBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* 
     *   fold constants in our condition expression, replacing the
     *   expression with the folded version
     */
    if (cond_expr_ != 0)
        cond_expr_ = cond_expr_->fold_constants(symtab);

    /* fold constants in the body statement */
    if (body_stm_ != 0)
        body_stm_ = (CTPNStm *)body_stm_->fold_constants(symtab);

    /* we are not directly changed by this operation */
    return this;
}

/*
 *   Chart the control flow through the statement
 */
unsigned long CTPNStmWhileBase::get_control_flow(int warn) const
{
    unsigned long flags;

    /*
     *   If we have a condition with a constant false value, and we have a
     *   non-empty body, and the body isn't reachable via a label, warn
     *   about the unreachable code 
     */
    if (cond_expr_->is_const()
        && !cond_expr_->get_const_val()->get_val_bool()
        && body_stm_ != 0
        && !body_stm_->has_code_label()
        && G_prs->get_syntax_only())
    {
        /* log a warning if desired */
        if (warn)
            log_warning(TCERR_WHILE_COND_FALSE);

        /* this will simply continue to the next statement */
        return TCPRS_FLOW_NEXT;
    }

    /* check for a body */
    if (body_stm_ != 0)
    {
        /* determine how our body can exit */
        flags = body_stm_->get_control_flow(warn);
    }
    else
    {
        /* 
         *   We have no body, so it's entirely up to the loop - we'll
         *   evaluate those parameters below, but for now clear all flags 
         */
        flags = 0;
    }

    /* 
     *   ignore any 'next' flag that comes out of the body, because 'next'
     *   from the last statement in the body actually takes us back to the
     *   top of the loop 
     */
    flags &= ~TCPRS_FLOW_NEXT;

    /* 
     *   if there's a 'break' in the body, we can continue to the next
     *   statement, since a break in the body takes us to the statement
     *   after the 'for' 
     */
    if ((flags & TCPRS_FLOW_BREAK) != 0)
        flags |= TCPRS_FLOW_NEXT;

    /* 
     *   if the condition is a constant false value, or has a non-constant
     *   value, assume that the condition can possibly become false and
     *   hence that the loop can exit, and hence that the next statement
     *   is reachable 
     */
    if (!cond_expr_->is_const()
        || (cond_expr_->is_const()
            && !cond_expr_->get_const_val()->get_val_bool()))
        flags |= TCPRS_FLOW_NEXT;

    /* 
     *   clear the 'break' and 'continue' flags, since we capture them in
     *   our own scope - if our child breaks or continues, it won't affect
     *   the loop's container 
     */
    flags &= ~(TCPRS_FLOW_BREAK | TCPRS_FLOW_CONT);

    /* return the flags */
    return flags;
}

/* ------------------------------------------------------------------------ */
/*
 *   "do-while" statement node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmDoWhileBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* 
     *   fold constants in our condition expression, replacing the
     *   expression with the folded version 
     */
    if (cond_expr_ != 0)
        cond_expr_ = cond_expr_->fold_constants(symtab);

    /* fold constants in the body statement */
    if (body_stm_ != 0)
        body_stm_ = (CTPNStm *)body_stm_->fold_constants(symtab);

    /* we are not directly changed by this operation */
    return this;
}

/*
 *   Chart the control flow through the statement 
 */
unsigned long CTPNStmDoWhileBase::get_control_flow(int warn) const
{
    unsigned long flags;

    /* check for a body */
    if (body_stm_ != 0)
    {
        /* determine how our body can exit */
        flags = body_stm_->get_control_flow(warn);
    }
    else
    {
        /* 
         *   We have no body, so it's entirely up to the loop - we'll
         *   evaluate those parameters below, but for now clear all flags 
         */
        flags = 0;
    }

    /*
     *   If we have a 'next' flag, it means that control can flow out from
     *   the end of the last statement in the loop.  That in turn means that
     *   we can reach the code where we evaluate the condition.  So, in order
     *   for flow to proceed out of the loop, we have to be able to reach the
     *   condition, and then the condition has to be capable of being false
     *   (because if it's always true, we'll simply go back to the start of
     *   the loop every time we hit the condition).  
     */
    if ((flags & TCPRS_FLOW_NEXT) != 0)
    {
        /*
         *   The 'next' flag up to this point only means that control can
         *   reach the condition at the bottom of the loop.  Since we want to
         *   tell our caller whether or not control can flow *out* of the
         *   loop, this is now irrelevant, so clear the 'next' flag for now. 
         */
        flags &= ~TCPRS_FLOW_NEXT;

        /*
         *   Okay, we now know we can reach the condition at the bottom of
         *   the loop.  If the condition is a constant false value, we know
         *   that we can flow out of the loop, since we know we'll hit the
         *   condition, find that it's false, and exit the loop.  Likewise,
         *   if the condition hasa non-constant value, assume that it can
         *   possibly become false and hence that the loop can exit, and
         *   hence that the next statement is reachable.
         */
        if (!cond_expr_->is_const()
            || (cond_expr_->is_const()
                && !cond_expr_->get_const_val()->get_val_bool()))
            flags |= TCPRS_FLOW_NEXT;
    }

    /* 
     *   if there's a 'break' in the body, we can continue to the next
     *   statement, since a break in the body takes us to the statement
     *   after the 'while' 
     */
    if ((flags & TCPRS_FLOW_BREAK) != 0)
        flags |= TCPRS_FLOW_NEXT;

    /* 
     *   clear the 'break' and 'continue' flags, since we capture them in
     *   our own scope - if our child breaks or continues, it won't affect
     *   the loop's container 
     */
    flags &= ~(TCPRS_FLOW_BREAK | TCPRS_FLOW_CONT);

    /* return the flags */
    return flags;
}


/* ------------------------------------------------------------------------ */
/*
 *   "switch" statement node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmSwitchBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* fold constants in our expression and body */
    expr_ = expr_->fold_constants(symtab);
    if (body_ != 0)
        body_ = (CTPNStm *)body_->fold_constants(symtab);

    /* we're unchanged by constant folding */
    return this;
}

/*
 *   Chart the control flow through the statement 
 */
unsigned long CTPNStmSwitchBase::get_control_flow(int warn) const
{
    unsigned long flags;
    
    /*
     *   Control flow through a switch is controlled by control flow
     *   through the compound statement.  If the compound statement can
     *   break, then we flow to the next statement.  If the compound
     *   statement can reach the next statement, then so can we.  
     */
    flags = (body_ != 0 ? body_->get_control_flow(warn) : TCPRS_FLOW_NEXT);

    /* 
     *   check for a reachable break in the body - a break means the next
     *   statement is reachable 
     */
    if ((flags & TCPRS_FLOW_BREAK) != 0)
    {
        /* 
         *   break takes us to the next statement, and doesn't propagate
         *   up to a break in any enclosing statement 
         */
        flags |= TCPRS_FLOW_NEXT;
        flags &= ~TCPRS_FLOW_BREAK;
    }

    /*
     *   If the switch has no 'default' case, it is almost certain that we
     *   can reach the next statement after the switch, because we can
     *   probably assume that not every case is accounted for and hence we
     *   have the equivalent of a null 'default' case.
     *   
     *   Note that we can't know for sure that every case hasn't been
     *   accounted for, because we do not have datatype information for the
     *   controlling expression; even if we did know the controlling
     *   expression's datatype, it is impossible to enumerate every possible
     *   value for expressions of certain datatypes (such as lists and
     *   strings, which have effectively infinite ranges).  However, it is a
     *   reasonable assumption that not every possible case has been
     *   accounted for, 
     */
    if (!get_has_default())
        flags |= TCPRS_FLOW_NEXT;

    /* return the flags */
    return flags;
}

/* ------------------------------------------------------------------------ */
/*
 *   code label statement node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmLabelBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* fold constants in our labeled statement */
    if (stm_ != 0)
        stm_ = (CTPNStm *)stm_->fold_constants(symtab);

    /* we're unchanged by constant folding */
    return this;
}

/*
 *   Chart the control flow through the statement 
 */
unsigned long CTPNStmLabelBase::get_control_flow(int warn) const
{
    ulong flags;
    
    /* 
     *   if we have an underlying statement, control flow is the same as
     *   the statement's control flow; if we don't have a statement (i.e.,
     *   our labeled statement is an empty statement), we flow to the next
     *   statement 
     */
    flags = (stm_ != 0 ? stm_->get_control_flow(warn) : TCPRS_FLOW_NEXT);

    /* 
     *   if we have an explicit 'break' flag in our control flow flags, it
     *   means that we were targeted specifically with a 'break' statement
     *   contained within our body - this transfers control to the next
     *   statement after me, therefore my next statement is reachable in
     *   this case 
     */
    if ((control_flow_flags_ & TCPRS_FLOW_BREAK) != 0)
        flags |= TCPRS_FLOW_NEXT;

    /* return the result */
    return flags;
}

/*
 *   add control flow flags 
 */
void CTPNStmLabelBase::add_control_flow_flags(ulong flags)
{
    /* add the flags to any we already have */
    control_flow_flags_ |= flags;
}

/* ------------------------------------------------------------------------ */
/*
 *   'goto' statement node 
 */

/*
 *   Chart the control flow through the statement 
 */
unsigned long CTPNStmGotoBase::get_control_flow(int warn) const
{
    CTcSymbol *sym;
    
    /* find our target statement */
    if (G_prs->get_goto_symtab() != 0
        && (sym = G_prs->get_goto_symtab()->find(lbl_, lbl_len_)) != 0
        && sym->get_type() == TC_SYM_LABEL)
    {
        /* 
         *   we found our label - mark its statement as having a 'goto'
         *   targeted at it 
         */
        ((CTcSymLabel *)sym)->add_control_flow_flags(TCPRS_FLOW_GOTO);
    }
    
    /* indicate that we jump somewhere */
    return TCPRS_FLOW_GOTO;
}

/* ------------------------------------------------------------------------ */
/*
 *   'case' label statement node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmCaseBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* fold constants in our expression */
    expr_ = expr_->fold_constants(symtab);

    /* if the expression isn't a constant, it's an error */
    if (!expr_->is_const())
        log_error(TCERR_CASE_NOT_CONSTANT);
    
    /* fold constants in our labeled statement */
    if (stm_ != 0)
        stm_ = (CTPNStm *)stm_->fold_constants(symtab);

    /* we're unchanged by constant folding */
    return this;
}

/*
 *   Chart the control flow through the statement 
 */
unsigned long CTPNStmCaseBase::get_control_flow(int warn) const
{
    /* 
     *   if we have an underlying statement, control flow is the same as
     *   the statement's control flow; if we don't have a statement (i.e.,
     *   our labeled statement is an empty statement), we flow to the next
     *   statement 
     */
    return (stm_ != 0 ? stm_->get_control_flow(warn) : TCPRS_FLOW_NEXT);
}

/* ------------------------------------------------------------------------ */
/*
 *   'default' label statement node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmDefaultBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* fold constants in our labeled statement */
    if (stm_ != 0)
        stm_ = (CTPNStm *)stm_->fold_constants(symtab);

    /* we're unchanged by constant folding */
    return this;
}

/*
 *   Chart the control flow through the statement 
 */
unsigned long CTPNStmDefaultBase::get_control_flow(int warn) const
{
    /* 
     *   if we have an underlying statement, control flow is the same as
     *   the statement's control flow; if we don't have a statement (i.e.,
     *   our labeled statement is an empty statement), we flow to the next
     *   statement 
     */
    return (stm_ != 0 ? stm_->get_control_flow(warn) : TCPRS_FLOW_NEXT);
}

/* ------------------------------------------------------------------------ */
/*
 *   'try' statement 
 */

/*
 *   add a 'catch' clause 
 */
void CTPNStmTryBase::add_catch(CTPNStmCatch *catch_stm)
{
    /* link it in at the end of our list */
    if (last_catch_stm_ != 0)
        last_catch_stm_->set_next_catch(catch_stm);
    else
        first_catch_stm_ = catch_stm;

    /* it's now the last catch statement */
    last_catch_stm_ = catch_stm;
    catch_stm->set_next_catch(0);

    /* it's the last statement in its group */
    catch_stm->set_next_stm(0);
}

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmTryBase::fold_constants(CTcPrsSymtab *symtab)
{
    CTPNStmCatch *cur_catch;
    
    /* fold constants in our protected code */
    if (body_stm_ != 0)
        body_stm_->fold_constants(symtab);

    /* fold constants in our catch list */
    for (cur_catch = first_catch_stm_ ; cur_catch != 0 ;
         cur_catch = cur_catch->get_next_catch())
    {
        /* fold constants; assume this will not change the statement */
        cur_catch->fold_constants(symtab);
    }

    /* fold constants in our 'finally' clause */
    if (finally_stm_ != 0)
        finally_stm_->fold_constants(symtab);

    /* we're unchanged by constant folding */
    return this;
}

/*
 *   Chart the control flow through the statement 
 */
unsigned long CTPNStmTryBase::get_control_flow(int warn) const
{
    unsigned long flags;
    unsigned long fin_flags;
    CTPNStmCatch *cur_catch;
    
    /*
     *   First, determine the control flow through the protected code.  If
     *   it exits, we either exit or proceed to the 'finally' clause.  
     */
    flags = (body_stm_ != 0
             ? body_stm_->get_control_flow(warn) : TCPRS_FLOW_NEXT);

    /* 
     *   Next, figure out what the various 'catch' clauses can do.  We
     *   assume that each 'catch' clause is enterable, since any exception
     *   could occur within the protected code (whether it explicitly
     *   throws or not - it could call a function or method that throws).
     *   So, we can simply OR together with the main block's flags all of
     *   the ways the different 'catch' clauses can exit.  
     */
    for (cur_catch = first_catch_stm_ ; cur_catch != 0 ;
         cur_catch = cur_catch->get_next_catch())
    {
        /* OR in the flags for this 'catch' block */
        flags |= cur_catch->get_control_flow(warn);
    }

    /*
     *   If we have a 'finally' clause, it is definitely enterable, since
     *   every other way of leaving the protected code goes through it.
     *   Find how it can exit.  
     */
    fin_flags = (finally_stm_ != 0
                 ? finally_stm_->get_control_flow(warn)
                 : TCPRS_FLOW_NEXT);

    /* 
     *   If the 'finally' block can't flow to next, then neither can the
     *   main/catch blocks.  In addition, the main/catch blocks can't
     *   'break', 'continue', 'goto', or 'return' either, because the
     *   'finally' block will not allow the other blocks to proceed with
     *   whatever transfers they attempt.  So, if 'next' isn't set in the
     *   'finally' flags, clear out all of these flags from the main/catch
     *   flags.  
     */
    if (!(fin_flags & TCPRS_FLOW_NEXT))
        flags &= ~(TCPRS_FLOW_NEXT
                   | TCPRS_FLOW_BREAK
                   | TCPRS_FLOW_CONT
                   | TCPRS_FLOW_RET_VAL
                   | TCPRS_FLOW_RET_VOID
                   | TCPRS_FLOW_GOTO);

    /* 
     *   Add in the remaining flags from the 'finally' block, since
     *   whatever it does, the overall statement does.  However, ignore
     *   'next' flags in the 'finally' block, because if the 'finally'
     *   code goes to next, it just means that we keep doing whatever we
     *   were doing in the main block.  
     */
    fin_flags &= ~TCPRS_FLOW_NEXT;
    flags |= fin_flags;

    /* return the result */
    return flags;
}


/* ------------------------------------------------------------------------ */
/*
 *   'catch' clause
 */

/*
 *   set our exception class name 
 */
void CTPNStmCatchBase::set_exc_class(const CTcToken *tok)
{
    /* 
     *   remember the token's contents - the tokenizer keeps token symbol
     *   strings in memory throughout the compilation, so we can simply
     *   store a direct reference to the string 
     */
    exc_class_ = tok->get_text();
    exc_class_len_ = tok->get_text_len();
}

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmCatchBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* fold constants in our body */
    if (body_ != 0)
        body_ = (CTPNStm *)body_->fold_constants(symtab);

    /* we're unchanged by constant folding */
    return this;
}

/*
 *   Chart the control flow through the statement 
 */
unsigned long CTPNStmCatchBase::get_control_flow(int warn) const
{
    /*
     *   our control flow is that of the body 
     */
    return (body_ != 0 ? body_->get_control_flow(warn) : TCPRS_FLOW_NEXT);
}



/* ------------------------------------------------------------------------ */
/*
 *   'finally' clause 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmFinallyBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* fold constants in our body */
    if (body_ != 0)
        body_ = (CTPNStm *)body_->fold_constants(symtab);

    /* we're unchanged by constant folding */
    return this;
}

/*
 *   Chart the control flow through the statement 
 */
unsigned long CTPNStmFinallyBase::get_control_flow(int warn) const
{
    /* our control flow is that of the body */
    return (body_ != 0 ? body_->get_control_flow(warn) : TCPRS_FLOW_NEXT);
}



/* ------------------------------------------------------------------------ */
/*
 *   'throw' statement
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmThrowBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* fold constants in our expression */
    expr_ = expr_->fold_constants(symtab);
    
    /* we're unchanged by constant folding */
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   'break' statement
 */

/*
 *   set the label 
 */
void CTPNStmBreakBase::set_label(const CTcToken *tok)
{
    /* 
     *   token text is kept in memory throughout compilation, so we can
     *   simply store a reference to the text without copying it 
     */
    lbl_ = tok->get_text();
    lbl_len_ = tok->get_text_len();
}

/*
 *   chart control flow 
 */
ulong CTPNStmBreakBase::get_control_flow(int warn) const
{
    /*
     *   If we don't have a label, we break out of our enclosing control
     *   structure.  If we have a label, we act like a 'goto' statement. 
     */
    if (lbl_ == 0)
    {
        /* no label - we simply break out of the enclosing loop or switch */
        return TCPRS_FLOW_BREAK;
    }
    else
    {
        CTcSymbol *sym;
        
        /* 
         *   We have a label - we act like a 'goto' statement.  Find our
         *   target statement and mark it as having a targeted 'break'.  
         */
        if (G_prs->get_goto_symtab() != 0
            && (sym = G_prs->get_goto_symtab()->find(lbl_, lbl_len_)) != 0
            && sym->get_type() == TC_SYM_LABEL)
        {
            /* 
             *   we found our label - mark its statement as having a
             *   'break' targeted at it 
             */
            ((CTcSymLabel *)sym)->add_control_flow_flags(TCPRS_FLOW_BREAK);
        }

        /* indicate that we jump somewhere else */
        return TCPRS_FLOW_GOTO;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   'continue' statement 
 */

/*
 *   set the label 
 */
void CTPNStmContinueBase::set_label(const CTcToken *tok)
{
    /* 
     *   token text is kept in memory throughout compilation, so we can
     *   simply store a reference to the text without copying it 
     */
    lbl_ = tok->get_text();
    lbl_len_ = tok->get_text_len();
}

/*
 *   chart control flow 
 */
ulong CTPNStmContinueBase::get_control_flow(int warn) const
{
    /*
     *   If we don't have a label, we continue in our enclosing control
     *   structure.  If we have a label, we act like a 'goto' statement.  
     */
    if (lbl_ == 0)
    {
        /* no label - we simply continue with the enclosing loop */
        return TCPRS_FLOW_CONT;
    }
    else
    {
        CTcSymbol *sym;

        /* 
         *   We have a label - we act like a 'goto' statement.  Find our
         *   target statement and mark it as having a targeted 'continue'.
         */
        if (G_prs->get_goto_symtab() != 0
            && (sym = G_prs->get_goto_symtab()->find(lbl_, lbl_len_)) != 0
            && sym->get_type() == TC_SYM_LABEL)
        {
            /* 
             *   we found our label - mark its statement as having a
             *   'continue' targeted at it 
             */
            ((CTcSymLabel *)sym)->add_control_flow_flags(TCPRS_FLOW_CONT);
        }

        /* indicate that we jump somewhere else */
        return TCPRS_FLOW_GOTO;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Object Definition Statement 
 */

/*
 *   add a superclass given the name
 */
void CTPNStmObjectBase::add_superclass(const CTcToken *tok)
{
    CTPNSuperclass *sc;
    
    /* create a new superclass list entry */
    sc = new CTPNSuperclass(tok->get_text(), tok->get_text_len());

    /* add it to the end of my list */
    if (last_sc_ != 0)
        last_sc_->nxt_ = sc;
    else
        first_sc_ = sc;
    last_sc_ = sc;
}

/*
 *   add a superclass given the object symbol
 */
void CTPNStmObjectBase::add_superclass(CTcSymbol *sym)
{
    CTPNSuperclass *sc;

    /* create a new superclass list entry */
    sc = new CTPNSuperclass(sym);

    /* add it to the end of my list */
    if (last_sc_ != 0)
        last_sc_->nxt_ = sc;
    else
        first_sc_ = sc;
    last_sc_ = sc;
}

/*
 *   Add a property value 
 */
CTPNObjProp *CTPNStmObjectBase::add_prop(CTcSymProp *prop_sym,
                                         CTcPrsNode *expr,
                                         int replace, int is_static)
{
    CTPNObjProp *prop;
        
    /* create the new property item */
    prop = new CTPNObjProp((CTPNStmObject *)this, prop_sym, expr, 0,
                           is_static);

    /* link it into my list */
    add_prop_entry(prop, replace);

    /* return the new property to our caller */
    return prop;
}

/*
 *   Add a method 
 */
void CTPNStmObjectBase::add_method(CTcSymProp *prop_sym,
                                   CTPNCodeBody *code_body, int replace)
{
    /* create a new property and link it into my list */
    add_prop_entry(new CTPNObjProp((CTPNStmObject *)this, prop_sym,
                                   0, code_body, FALSE), replace);
}

/*
 *   Add an entry to my property list
 */
void CTPNStmObjectBase::add_prop_entry(CTPNObjProp *prop, int replace)
{
    /* link it in at the end of my list */
    if (last_prop_ != 0)
        last_prop_->nxt_ = prop;
    else
        first_prop_ = prop;
    last_prop_ = prop;

    /* this is the last element */
    prop->nxt_ = 0;

    /* count the new property */
    ++prop_cnt_;

    /* check for replacement */
    if (replace && !G_prs->get_syntax_only())
    {
        CTPNStmObject *stm;

        /* start at the current object */
        stm = (CTPNStmObject *)this;

        /* iterate through all previous versions of the object */
        for (;;)
        {
            CTcSymObj *sym;
            CTPNSuperclass *sc;
            
            /* 
             *   Get this object's superclass - since this is a 'modify'
             *   object, its only superclass is object it directly
             *   modifies.  If there's no superclass, the modified base
             *   object must have been undefined, so we have errors to
             *   begin with - ignore the replacement in this case.  
             */
            sc = stm->get_first_sc();
            if (sc == 0)
                break;

            /* get the symbol for the superclass */
            sym = (CTcSymObj *)sc->get_sym();

            /* 
             *   if this object is external, we must mark the property for
             *   replacement so that we replace it at link time, after
             *   we've loaded and resolved the external original object 
             */
            if (sym->is_extern())
            {
                /*
                 *   Add an entry to my list of properties to delete in my
                 *   base objects at link time.
                 */
                obj_sym_->add_del_prop_entry(prop->get_prop_sym());
                
                /* 
                 *   no need to look any further for modified originals --
                 *   we won't find any more of them in this translation
                 *   unit, since they must all be external from here on up
                 *   the chain 
                 */
                break;
            }

            /* 
             *   Get this symbol's object parse tree - since the symbol
             *   isn't external, and because we can only modify objects
             *   that we've already seen, the parse tree must be present.
             *   If the parse tree isn't here, stop now; we might be
             *   parsing for syntax only in this case.  
             */
            stm = sym->get_obj_stm();
            if (stm == 0)
                break;

            /* if this object isn't modified, we're done */
            if (!sym->get_modified())
                break;
        
            /* delete the property in the original object */
            stm->delete_property(prop->get_prop_sym());
        }
    }
}

/*
 *   Delete a property - this is used when we 'replace' a property in a
 *   'modify' object 
 */
void CTPNStmObjectBase::delete_property(CTcSymProp *prop_sym)
{
    CTPNObjProp *cur;
    CTPNObjProp *prv;

    /* scan our and look for a property matching the given symbol */
    for (prv = 0, cur = first_prop_ ; cur != 0 ; prv = cur, cur = cur->nxt_)
    {
        /* check for a match */
        if (cur->get_prop_sym() == prop_sym)
        {
            /* this is the one to delete - unlink it from the list */
            if (prv != 0)
                prv->nxt_ = cur->nxt_;
            else
                first_prop_ = cur->nxt_;

            /* if that left the list empty, clear the tail pointer */
            if (first_prop_ == 0)
                last_prop_ = 0;

            /* decrement the property count */
            --prop_cnt_;

            /* delete the property from the vocabulary list as well */
            if (obj_sym_ != 0)
                obj_sym_->delete_vocab_prop(prop_sym->get_prop());

            /* we need look no further */
            break;
        }
    }
}

/*
 *   Add an implicit constructor, if one is required.  
 */
void CTPNStmObjectBase::add_implicit_constructor()
{
    int sc_cnt;
    CTPNSuperclass *sc;

    /* count the superclasses */
    for (sc_cnt = 0, sc = first_sc_ ; sc != 0 ; sc = sc->nxt_, ++sc_cnt) ;

    /*
     *   If the object has multiple superclasses and doesn't have an
     *   explicit constructor, add an implicit constructor that simply
     *   invokes each superclass constructor using the same argument list
     *   sent to our constructor.  
     */
    if (sc_cnt > 1)
    {
        CTPNObjProp *prop;
        int has_constructor;

        /* scan the property list for a constructor property */
        for (prop = first_prop_, has_constructor = FALSE ; prop != 0 ;
             prop = prop->nxt_)
        {
            /* 
             *   if this is a constructor, note that the object has an
             *   explicit constructor 
             */
            if (prop->is_constructor())
            {
                /* we have a consructor */
                has_constructor = TRUE;

                /* that's all we needed to know - no need to keep looking */
                break;
            }
        }

        /* if we don't have a constructor, add one */
        if (!has_constructor)
        {
            CTPNStmImplicitCtor *stm;
            CTPNCodeBody *cb;
            CTcSymLocal *varargs_local;

            /* 
             *   Create a placeholder symbol for the varargs list variable
             *   - we don't really need a symbol for this, but we do need
             *   the symbol object so we can generate code for it
             *   properly.  Note that a varargs list variable is a local,
             *   not a formal.  
             */
            varargs_local = new CTcSymLocal("*", 1, FALSE, FALSE, 0);

            /* create the pseudo-statement for the implicit constructor */
            stm = new CTPNStmImplicitCtor((CTPNStmObject *)this);

            /* 
             *   Create a code body to hold the statement.  The code body
             *   has one local symbol (the vararg list formal parameter),
             *   and is defined by the 'constructor' property.  
             */
            cb = new CTPNCodeBody(G_prs->get_global_symtab(),
                                  G_prs->get_goto_symtab(),
                                  stm, 0, TRUE, TRUE, varargs_local, 1,
                                  TRUE, 0);

            /* 
             *   set the implicit constructor's source file location to the
             *   source file location of the start of the object definition 
             */
            cb->set_source_pos(file_, linenum_);
            
            /* add it to the object's property list */
            add_method(G_prs->get_constructor_sym(), cb, FALSE);
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Object Property node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNObjPropBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* fold constants in our expression or code body, as appropriate */
    if (expr_ != 0)
    {
        /* 
         *   set this statement's source line location to be the current
         *   location for error reporting 
         */
        G_tok->set_line_info(get_source_desc(),
                             get_source_linenum());
        
        /* fold constants in our expression */
        expr_ = expr_->fold_constants(symtab);
        
        /*
         *   If the resulting expression isn't a constant value, we must
         *   convert our expression to a method, since we need to evaluate
         *   the value at run-time with code.  Create an expression
         *   statement to hold the expression, then create a code body to
         *   hold the expression statement.  
         */
        if (!expr_->is_const() && !expr_->is_dstring())
        {
            CTPNStm *stm;
            
            /*
             *   Generate an implicit 'return' for a regular expression,
             *   or an implicit static initializer for a static property.  
             */
            if (is_static_)
            {
                /* 
                 *   it's a static initializer - generate a static
                 *   initializer expression 
                 */
                stm = new CTPNStmStaticPropInit(expr_, prop_sym_->get_prop());
            }
            else if (expr_->has_return_value())
            {
                /* 
                 *   create a 'return' statement to return the value of
                 *   the expression 
                 */
                stm = new CTPNStmReturn(expr_);
            }
            else
            {
                /* 
                 *   It has no return value, so it must be something like a
                 *   double-quoted string or a call to a void function.  Wrap
                 *   this in an 'expression statement' so that we evaluate
                 *   the expression for its side effects but discard any
                 *   return value.  
                 */
                stm = new CTPNStmExpr(expr_);
            }

            /* create the code body */
            code_body_ = new CTPNCodeBody(G_prs->get_global_symtab(), 0,
                                          stm, 0, FALSE, FALSE, 0, 0,
                                          TRUE, 0);

            /* 
             *   forget about our expression, since it's now incorporated
             *   into the code body 
             */
            expr_ = 0;

            /* 
             *   set the source file location in both the code body and
             *   the expression statement nodes to use our own source file
             *   location 
             */
            stm->set_source_pos(file_, linenum_);
            code_body_->set_source_pos(file_, linenum_);
        }
    }
    else if (code_body_ != 0)
    {
        /* fold constants in our code body */
        code_body_->fold_constants(symtab);
    }

    /* we're not changed directly by this */
    return this;
}

/*
 *   Am I a constructor?  
 */
int CTPNObjPropBase::is_constructor() const
{
    /*
     *   I'm a constructor if I have a code body, and my property is the
     *   constructor property.  
     */
    return (code_body_ != 0
            && prop_sym_ != 0
            && prop_sym_->get_prop() == G_prs->get_constructor_prop());
}

/* ------------------------------------------------------------------------ */
/*
 *   code ('goto') label base 
 */

/*
 *   Add control flow flags to my statement 
 */
void CTcSymLabelBase::add_control_flow_flags(ulong flags)
{
    if (stm_ != 0)
        stm_->add_control_flow_flags(flags);
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
    fp->write_int2((int)get_sym_len());
    fp->write_bytes(get_sym(), get_sym_len());

    /* write our external name, with length prefix */
    fp->write_int2((int)get_ext_len());
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
