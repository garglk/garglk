#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCPRS.CPP,v 1.5 1999/07/11 00:46:53 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcprs.cpp - TADS 3 Compiler Parser
Function
  This parser module contains code required for any parser usage, both
  for a full compiler and for a debugger.
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
 *   Expression Operator parser objects.  These objects are linked
 *   together in a network that defines the order of precedence for
 *   expression operators.
 *   
 *   These objects use static storage.  This is acceptable, even though
 *   these objects aren't all "const" qualified, because the compiler uses
 *   a single global parser object; since there's only the one parser,
 *   there only needs to be a single network of these objects.  
 */

/* unary operator parser */
static CTcPrsOpUnary S_op_unary;

/* factor group */
static const CTcPrsOpMul S_op_mul;
static const CTcPrsOpDiv S_op_div;
static const CTcPrsOpMod S_op_mod;
static const CTcPrsOpBin *const
   S_grp_factor[] = { &S_op_mul, &S_op_div, &S_op_mod, 0 };
static const CTcPrsOpBinGroup S_op_factor(&S_op_unary, &S_op_unary,
                                          S_grp_factor);

/* term group */
static const CTcPrsOpAdd S_op_add;
static const CTcPrsOpSub S_op_sub;
static const CTcPrsOpBin *const S_grp_term[] = { &S_op_add, &S_op_sub, 0 };
static const CTcPrsOpBinGroup S_op_term(&S_op_factor, &S_op_factor,
                                        S_grp_term);

/* shift group */
static const CTcPrsOpShl S_op_shl;
static const CTcPrsOpShr S_op_shr;
static const CTcPrsOpBin *const S_grp_shift[] = { &S_op_shl, &S_op_shr, 0 };
static const CTcPrsOpBinGroup S_op_shift(&S_op_term, &S_op_term,
                                         S_grp_shift);

/* magnitude comparisons group */
static const CTcPrsOpGt S_op_gt;
static const CTcPrsOpGe S_op_ge;
static const CTcPrsOpLt S_op_lt;
static const CTcPrsOpLe S_op_le;
static const CTcPrsOpBin *const
   S_grp_relcomp[] = { &S_op_gt, &S_op_ge, &S_op_lt, &S_op_le, 0 };
static const CTcPrsOpBinGroup S_op_relcomp(&S_op_shift, &S_op_shift,
                                           S_grp_relcomp);

/* 
 *   Equality/inequality comparison group.  Note that the equality operator
 *   is non-const because we want the option to change the operator on the
 *   fly based on syntax mode - '==' in C-mode, '=' in TADS traditional mode.
 *   (This was a feature of tads 2, but we have since deprecated it in tads
 *   3, so this ability is actually just vestigial at this point.  No harm in
 *   keeping around the code internally for it, though, since it's pretty
 *   simple.)
 *   
 *   Note also that this is a special binary group - this one recognizes the
 *   non-keyword operators 'is in' and 'not in'.  
 */
static CTcPrsOpEq S_op_eq;
static const CTcPrsOpNe S_op_ne;
static const CTcPrsOpBin *const
   S_grp_eqcomp[] = { &S_op_eq, &S_op_ne, 0 };
static const CTcPrsOpBinGroupCompare
   S_op_eqcomp(&S_op_relcomp, &S_op_relcomp, S_grp_eqcomp);

/* bitwise AND operator */
static const CTcPrsOpBAnd S_op_band(&S_op_eqcomp, &S_op_eqcomp);

/* bitwise XOR operator */
static const CTcPrsOpBXor S_op_bxor(&S_op_band, &S_op_band);

/* bitwise OR operator */
static const CTcPrsOpBOr S_op_bor(&S_op_bxor, &S_op_bxor);

/* logical AND operator */
static const CTcPrsOpAnd S_op_and(&S_op_bor, &S_op_bor);

/* logical OR operator */
static const CTcPrsOpOr S_op_or(&S_op_and, &S_op_and);

/* conditional operator */
static const CTcPrsOpIf S_op_if;

/* 
 *   assignment operator - note that this is non-const, because we must be
 *   able to change the operator - '=' in C-mode, and ':=' in TADS
 *   traditional mode 
 */
static CTcPrsOpAsi S_op_asi;

/* comma operator */
static const CTcPrsOpComma S_op_comma(&S_op_asi, &S_op_asi);


/* ------------------------------------------------------------------------ */
/*
 *   Main Parser 
 */

/*
 *   initialize the parser 
 */
CTcParser::CTcParser()
{
    size_t i;
    
    /* we don't have any module information yet */
    module_name_ = 0;
    module_seqno_ = 0;

    /* start out in normal mode */
    pp_expr_mode_ = FALSE;
    src_group_mode_ = FALSE;

    /* create the global symbol table */
    global_symtab_ = new CTcPrsSymtab(0);

    /* no enclosing statement yet */
    enclosing_stm_ = 0;

    /* no source location yet */
    cur_desc_ = 0;
    cur_linenum_ = 0;

    /* no dictionaries yet */
    dict_cur_ = 0;
    dict_head_ = dict_tail_ = 0;

    /* no object file dictionary list yet */
    obj_dict_list_ = 0;
    obj_file_dict_idx_ = 0;

    /* no dictionary properties yet */
    dict_prop_head_ = 0;

    /* no grammar productions yet */
    gramprod_head_ = gramprod_tail_ = 0;

    /* no object file symbol list yet */
    obj_sym_list_ = 0;
    obj_file_sym_idx_ = 0;

    /* no object templates yet */
    template_head_ = template_tail_ = 0;

    /* allocate some initial template parsing space */
    template_expr_max_ = 16;
    template_expr_ = (CTcObjTemplateInst *)G_prsmem->
                     alloc(sizeof(template_expr_[0]) * template_expr_max_);

    /* no locals yet */
    local_cnt_ = max_local_cnt_ = 0;

    /* no local or goto symbol table yet */
    local_symtab_ = 0;
    enclosing_local_symtab_ = 0;
    goto_symtab_ = 0;

    /* no debugger local symbol table yet */
    debug_symtab_ = 0;

    /* not in a preprocessor constant expression */
    pp_expr_mode_ = FALSE;

    /* assume we're doing full compilation */
    syntax_only_ = FALSE;

    /* no nested top-level statements yet */
    nested_stm_head_ = 0;
    nested_stm_tail_ = 0;

    /* no anonymous objects yet */
    anon_obj_head_ = 0;
    anon_obj_tail_ = 0;

    /* no non-symbol objects yet */
    nonsym_obj_head_ = 0;
    nonsym_obj_tail_ = 0;

    /* allocate an initial context variable property array */
    ctx_var_props_size_ = 50;
    ctx_var_props_ = (tctarg_prop_id_t *)
                     t3malloc(ctx_var_props_size_ * sizeof(tctarg_prop_id_t));

    /* no context variable properties assigned yet */
    ctx_var_props_cnt_ = 0;
    ctx_var_props_used_ = 0;

    /* 
     *   no context variable indices assigned yet - start at one higher
     *   than the index at which we always store 'self' 
     */
    next_ctx_arr_idx_ = TCPRS_LOCAL_CTX_METHODCTX + 1;

    /* 'self' isn't valid yet */
    self_valid_ = FALSE;

    /* start at enum ID 1 (let 0 serve as an invalid value) */
    next_enum_id_ = 1;

    /* the '+' property is not yet defined */
    plus_prop_ = 0;

    /* no exported symbols yet */
    exp_head_ = exp_tail_ = 0;

    /* allocate an initial '+' stack */
    plus_stack_alloc_ = 32;
    plus_stack_ = (CTPNStmObject **)
                  t3malloc(plus_stack_alloc_ * sizeof(*plus_stack_));

    /* clear out the stack */
    for (i = 0 ; i < plus_stack_alloc_ ; ++i)
        plus_stack_[i] = 0;

    /* there's no current code body (function/method body) yet */
    cur_code_body_ = 0;

    /* nothing in the local context has been referenced yet */
    self_referenced_ = FALSE;
    local_ctx_needs_self_ = FALSE;
    full_method_ctx_referenced_ = FALSE;
    local_ctx_needs_full_method_ctx_ = FALSE;
}

/*
 *   Initialize.  This must be called after the code generator is set up.  
 */
void CTcParser::init()
{
    static const char construct_name[] = "construct";
    static const char finalize_name[] = "finalize";
    static const char objcall_name[] = ".objcall";
    static const char graminfo_name[] = "grammarInfo";
    static const char miscvocab_name[] = "miscVocab";
    static const char lexical_parent_name[] = "lexicalParent";
    static const char src_order_name[] = "sourceTextOrder";
    static const char src_group_name[] = "sourceTextGroup";
    static const char src_group_mod_name[] = "sourceTextGroupName";
    static const char src_group_seq_name[] = "sourceTextGroupOrder";
    tctarg_prop_id_t graminfo_prop_id;
    tctarg_prop_id_t lexpar_prop_id;
    tctarg_prop_id_t src_order_prop_id;
    tctarg_prop_id_t src_group_prop_id;
    tctarg_prop_id_t src_group_mod_prop_id;
    tctarg_prop_id_t src_group_seq_prop_id;
    CTcSymProp *sym;

    /* allocate and note our special property ID's */
    constructor_prop_ = G_cg->new_prop_id();
    finalize_prop_ = G_cg->new_prop_id();
    objcall_prop_ = G_cg->new_prop_id();
    graminfo_prop_id = G_cg->new_prop_id();
    miscvocab_prop_ = G_cg->new_prop_id();
    lexpar_prop_id = G_cg->new_prop_id();
    src_order_prop_id = G_cg->new_prop_id();
    src_group_prop_id = G_cg->new_prop_id();
    src_group_mod_prop_id = G_cg->new_prop_id();
    src_group_seq_prop_id = G_cg->new_prop_id();

    /* add a "construct" property for constructors */
    sym = new CTcSymProp(construct_name, sizeof(construct_name) - 1,
                         FALSE, (tctarg_prop_id_t)constructor_prop_);
    sym->mark_referenced();
    global_symtab_->add_entry(sym);
    constructor_sym_ = sym;

    /* add a "finalize" property for finalizers */
    sym = new CTcSymProp(finalize_name, sizeof(finalize_name) - 1,
                         FALSE, (tctarg_prop_id_t)finalize_prop_);
    sym->mark_referenced();
    global_symtab_->add_entry(sym);

    /* add an "object call" property for anonymous functions */
    sym = new CTcSymProp(objcall_name, sizeof(objcall_name) - 1,
                         FALSE, (tctarg_prop_id_t)objcall_prop_);
    sym->mark_referenced();
    global_symtab_->add_entry(sym);

    /* add a "grammarInfo" property for grammar production match objects */
    graminfo_prop_ = new CTcSymProp(graminfo_name, sizeof(graminfo_name) - 1,
                                    FALSE,
                                    (tctarg_prop_id_t)graminfo_prop_id);
    graminfo_prop_->mark_referenced();
    global_symtab_->add_entry(graminfo_prop_);

    /* add a "miscVocab" property for miscellaneous vocabulary words */
    sym = new CTcSymProp(miscvocab_name, sizeof(miscvocab_name) - 1,
                         FALSE, miscvocab_prop_);
    sym->mark_referenced();
    global_symtab_->add_entry(sym);

    /* add a "lexicalParent" property for a nested object's parent */
    lexical_parent_sym_ = new CTcSymProp(lexical_parent_name,
                                         sizeof(lexical_parent_name) - 1,
                                         FALSE,
                                         (tctarg_prop_id_t)lexpar_prop_id);
    lexical_parent_sym_->mark_referenced();
    global_symtab_->add_entry(lexical_parent_sym_);

    /* add a "sourceTextOrder" property */
    src_order_sym_ = new CTcSymProp(src_order_name,
                                    sizeof(src_order_name) - 1,
                                    FALSE,
                                    (tctarg_prop_id_t)src_order_prop_id);
    src_order_sym_->mark_referenced();
    global_symtab_->add_entry(src_order_sym_);

    /* start the sourceTextOrder index at 1 */
    src_order_idx_ = 1;

    /* add a "sourceTextGroup" property */
    src_group_sym_ = new CTcSymProp(src_group_name,
                                    sizeof(src_group_name) - 1,
                                    FALSE,
                                    (tctarg_prop_id_t)src_group_prop_id);
    src_group_sym_->mark_referenced();
    global_symtab_->add_entry(src_group_sym_);

    /* we haven't created the sourceTextGroup referral object yet */
    src_group_id_ = TCTARG_INVALID_OBJ;

    /* add a "sourceTextGroupName" property */
    src_group_mod_sym_ = new CTcSymProp(
        src_group_mod_name, sizeof(src_group_mod_name) - 1, FALSE,
        (tctarg_prop_id_t)src_group_mod_prop_id);
    src_group_mod_sym_->mark_referenced();
    global_symtab_->add_entry(src_group_mod_sym_);

    /* add a "sourceTextGroupOrder" property */
    src_group_seq_sym_ = new CTcSymProp(
        src_group_seq_name, sizeof(src_group_seq_name) - 1, FALSE,
        (tctarg_prop_id_t)src_group_seq_prop_id);
    src_group_seq_sym_->mark_referenced();
    global_symtab_->add_entry(src_group_seq_sym_);
}


/*
 *   destroy the parser
 */
CTcParser::~CTcParser()
{
    /*
     *   Note that we don't have to delete certain objects, because we
     *   allocated them out of the parser memory pool and will be
     *   automatically deleted when the pool is deleted.  For example, we
     *   don't have to delete any symbol tables, including the global
     *   symbol table.  
     */

    /* delete the module name, if it's known */
    lib_free_str(module_name_);

    /* delete the object file symbol fixup list, if present */
    if (obj_sym_list_ != 0)
        t3free(obj_sym_list_);

    /* delete the object file dictionary fixup list, if present */
    if (obj_dict_list_ != 0)
        t3free(obj_dict_list_);

    /* delete the context variable property list */
    if (ctx_var_props_ != 0)
        t3free(ctx_var_props_);

    /* delete the export list */
    while (exp_head_ != 0)
    {
        CTcPrsExport *nxt;
        
        /* remember the next entry, since we're deleting our pointer to it */
        nxt = exp_head_->get_next();
        
        /* delete this entry */
        delete exp_head_;

        /* move on to the next */
        exp_head_ = nxt;
    }

    /* delete the '+' stack */
    t3free(plus_stack_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the module information 
 */
void CTcParser::set_module_info(const char *name, int seqno)
{
    /* if we have a name stored already, delete the old one */
    lib_free_str(module_name_);

    /* store the new name and sequence number */
    module_name_ = lib_copy_str(name);
    module_seqno_ = seqno;
}

/*
 *   Change the #pragma C mode.  On changing this mode, we'll change the
 *   assignment operator and equality operator tokens.  If 'mode' is true,
 *   we're in C mode; otherwise, we're in traditional TADS mode.
 *   
 *   #pragma C+: assignment is '=', equality is '=='
 *.  #pragma C-: assignment is ':=', equality is '='.  
 */
void CTcParser::set_pragma_c(int mode)
{
    /* set the assignment operator */
    S_op_asi.set_asi_op(mode ? TOKT_EQ : TOKT_ASI);

    /* set the equality comparison operator */
    S_op_eq.set_eq_op(mode ? TOKT_EQEQ : TOKT_EQ);
}

/*
 *   Parse an expression.  This parses a top-level comma expression.
 */
CTcPrsNode *CTcParser::parse_expr()
{
    /* parse a comma expression */
    return S_op_comma.parse();
}

/*
 *   Parse a condition expression.  Warns if the outermost operator is a
 *   simple assignment.  
 */
CTcPrsNode *CTcParser::parse_cond_expr()
{
    CTcPrsNode *cond;

    /* parse the expression */
    cond = parse_expr();

    /* 
     *   if the outermost operator is a simple assignment, display an
     *   error 
     */
    if (cond != 0 && cond->is_simple_asi() && !G_prs->get_syntax_only())
        G_tok->log_warning(TCERR_ASI_IN_COND);

    /* return the result */
    return cond;
}

/*
 *   Parse an assignment expression.  
 */
CTcPrsNode *CTcParser::parse_asi_expr()
{
    /* parse an assignment expression */
    return S_op_asi.parse();
}

/*
 *   Parse an expression or a double-quoted string expression 
 */
CTcPrsNode *CTcParser::parse_expr_or_dstr(int allow_comma_expr)
{
    /* 
     *   parse the appropriate kind of expression - if a comma expression is
     *   allowed, parse that, otherwise parse an assignment expression (as
     *   that's the next thing down the hierarchy from the comma operator) 
     */
    return (allow_comma_expr ? S_op_comma.parse() : S_op_asi.parse());
}

/*
 *   Parse a required semicolon 
 */
int CTcParser::parse_req_sem()
{
    const char eof_str[] = "<end of file>";
    
    /* check to see if we found the semicolon */
    if (G_tok->cur() == TOKT_SEM)
    {
        /* success - skip the semicolon and tell the caller to proceed */
        G_tok->next();
        return 0;
    }

    /* 
     *   check what we have; the type of error we want to log depends on
     *   what we find next 
     */
    switch(G_tok->cur())
    {
    case TOKT_RPAR:
        /* log the extra ')' error */
        G_tok->log_error(TCERR_EXTRA_RPAR);

        /* 
         *   we're probably in an expression that ended before the user
         *   thought it should; skip the extraneous material up to the
         *   next semicolon 
         */
        return skip_to_sem();
        
    case TOKT_RBRACK:
        /* log the error */
        G_tok->log_error(TCERR_EXTRA_RBRACK);

        /* skip up to the next semicolon */
        return skip_to_sem();

    case TOKT_EOF:
        /* 
         *   missing semicolon at end of file - log the missing-semicolon
         *   error and tell the caller not to proceed, since there's
         *   nothing left to parse 
         */
        G_tok->log_error(TCERR_EXPECTED_SEMI,
                         (int)sizeof(eof_str)-1, eof_str);
        return 1;

    default:
        /* 
         *   the source is probably just missing a semicolon; log the
         *   error, and tell the caller to proceed from the current
         *   position 
         */
        G_tok->log_error_curtok(TCERR_EXPECTED_SEMI);
        return 0;
    }
}

/*
 *   Skip to the next semicolon 
 */
int CTcParser::skip_to_sem()
{
    /* keep going until we find a semicolon or some other reason to stop */
    for (;;)
    {
        /* see what we have next */
        switch(G_tok->cur())
        {
        case TOKT_EOF:
            /* end of file - tell the caller not to proceed */
            return 1;

        case TOKT_SEM:
            /* 
             *   it's the semicolon at last - skip it and tell the caller
             *   to proceed 
             */
            G_tok->next();
            return 0;

        case TOKT_LBRACE:
        case TOKT_RBRACE:
            /* 
             *   Don't skip past braces - the caller probably simply left
             *   out a semicolon at the end of a statement, and we've now
             *   reached the next block start or end.  Stop here and tell
             *   the caller to proceed.  
             */
            return 0;

        default:
            /* skip anything else */
            G_tok->next();
            break;
        }
    }
}

/*
 *   Create a symbol node 
 */
CTcPrsNode *CTcParser::create_sym_node(const textchar_t *sym, size_t sym_len)
{
    CTcSymbol *entry;
    CTcPrsSymtab *symtab;
    
    /* 
     *   First, look up the symbol in local scope.  Local scope symbols
     *   can always be resolved during parsing, because the language
     *   requires that local scope items be declared before their first
     *   use. 
     */
    entry = local_symtab_->find(sym, sym_len, &symtab);

    /* if we found it in local scope, return a resolved symbol node */
    if (entry != 0 && symtab != global_symtab_)
        return new CTPNSymResolved(entry);

    /* if there's a debugger local scope, look it up there */
    if (debug_symtab_ != 0)
    {
        tcprsdbg_sym_info info;

        /* look it up in the debug symbol table */
        if (debug_symtab_->find_symbol(sym, sym_len, &info))
        {
            /* found it - return a debugger local variable */
            return new CTPNSymDebugLocal(&info);
        }
    }

    /* 
     *   We didn't find it in local scope, so the symbol cannot be resolved
     *   until code generation - return an unresolved symbol node.  Note a
     *   possible implicit self-reference, since this could be a property of
     *   'self'.  
     */
    set_self_referenced(TRUE);
    return new CTPNSym(sym, sym_len);
}

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
         *   if we didn't find the entry, we must have loaded the symbol
         *   from a symbol file - add the dictionary list entry now
         */
        if (entry == 0)
            entry = create_dict_entry(sym);
    }

    /* return the new dictionary object */
    return entry;
}

/*
 *   Find or add a grammar production symbol.  Returns the master
 *   CTcGramProdEntry object for the grammar production.  
 */
CTcGramProdEntry *CTcParser::declare_gramprod(const char *txt, size_t len)
{
    CTcSymObj *sym;
    CTcGramProdEntry *entry;

    /* find or define the grammar production object symbol */
    sym = find_or_def_gramprod(txt, len, &entry);

    /* return the entry */
    return entry;
}

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
            G_tok->log_error_curtok(TCERR_REDEF_AS_OBJ);

            /* forget the symbol - it's not even an object */
            sym = 0;
        }
        else if (sym->get_metaclass() != TC_META_GRAMPROD)
        {
            /* it's an object, but not a production - log an error */
            G_tok->log_error_curtok(TCERR_REDEF_AS_GRAMPROD);

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
         *   if we didn't find the entry, we must have loaded the symbol
         *   from a symbol file - add the entry now 
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
     *   set the metaclass-specific extra data in the object symbol to
     *   point to the dictionary list entry 
     */
    sym->set_meta_extra(entry);

    /* return the new entry */
    return entry;
}

/* 
 *   find a dictionary list entry for a given object symbol 
 */
CTcDictEntry *CTcParser::get_dict_entry(CTcSymObj *obj)
{
    /* the dictionary list entry is the metaclass-specific data pointer */
    return (obj == 0 ? 0 : (CTcDictEntry *)obj->get_meta_extra());
}

/*
 *   Create a grammar production list entry.  This creates a
 *   CTcGramProdEntry object associated with the given grammar production
 *   symbol, and links the new entry into the master list of grammar
 *   production entries.   
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
     *   set the metaclass-specific data in the object symbol to point to
     *   the grammar production list entry 
     */
    if (sym != 0)
        sym->set_meta_extra(entry);

    /* return the new entry */
    return entry;
}

/* 
 *   find a grammar entry for a given (GrammarProd) object symbol 
 */
CTcGramProdEntry *CTcParser::get_gramprod_entry(CTcSymObj *obj)
{
    /* the grammar entry is the metaclass-specific data pointer */
    return (obj == 0 ? 0 : (CTcGramProdEntry *)obj->get_meta_extra());
}

/*
 *   Add a nested top-level statement to our list 
 */
void CTcParser::add_nested_stm(CTPNStmTop *stm)
{
    /* link it into our list */
    if (nested_stm_tail_ != 0)
        nested_stm_tail_->set_next_stm_top(stm);
    else
        nested_stm_head_ = stm;
    nested_stm_tail_ = stm;
}

/*
 *   Add an anonymous object to our list 
 */
void CTcParser::add_anon_obj(CTcSymObj *sym)
{
    /* link it into our list */
    if (anon_obj_tail_ != 0)
        anon_obj_tail_->nxt_ = sym;
    else
        anon_obj_head_ = sym;
    anon_obj_tail_ = sym;

    /* it's the last one */
    sym->nxt_ = 0;

    /* mark the symbol as anonymous */
    sym->set_anon(TRUE);
}

/*
 *   Add a non-symbolic object to our list 
 */
void CTcParser::add_nonsym_obj(tctarg_obj_id_t id)
{
    tcprs_nonsym_obj *obj;
    
    /* allocate a link structure */
    obj = new (G_prsmem) tcprs_nonsym_obj(id);
    
    /* link it into our list */
    if (nonsym_obj_tail_ != 0)
        nonsym_obj_tail_->nxt_ = obj;
    else
        nonsym_obj_head_ = obj;
    nonsym_obj_tail_ = obj;
}

/*
 *   Basic routine to read a length-prefixed symbol.  Uses the given
 *   temporary buffer, then stores the text in tokenizer memory (which
 *   remains valid and available throughout compilation).  If the length
 *   exceeds the temporary buffer length, we'll flag the given error and
 *   return null.  The length return pointer can be null if the caller wants
 *   the results null-terminated rather than returned with a counted length.
 *   If the length pointer is given, the result will not be null-terminated.
 *   
 */
const char *CTcParser::read_len_prefix_str
   (CVmFile *fp, char *tmp_buf, size_t tmp_buf_len, size_t *ret_len,
    int err_if_too_long)
{
    size_t read_len;
    size_t alloc_len;
    
    /* read the length to read from the file */
    read_len = (size_t)fp->read_uint2();

    /* if we need null termination, add a byte to the allocation length */
    alloc_len = read_len + (ret_len == 0 ? 1 : 0);

    /* if it won't fit in the temporary buffer, it's an error */
    if (alloc_len > tmp_buf_len)
    {
        /* log the error and return failure */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, err_if_too_long);
        return 0;
    }

    /* read the bytes into the temporary buffer */
    fp->read_bytes(tmp_buf, read_len);

    /* add null termination if required, or set the return length if not */
    if (ret_len == 0)
        tmp_buf[read_len] = '\0';
    else
        *ret_len = read_len;

    /* store the result in the tokenizer's text list and return the result */
    return G_tok->store_source(tmp_buf, alloc_len);
}

/*
 *   Read a length prefixed string into a given buffer.  Returns zero on
 *   success, non-zero on failure. 
 */
int CTcParser::read_len_prefix_str(CVmFile *fp, char *buf, size_t buf_len,
                                   int err_if_too_long)
{
    size_t read_len;
    size_t alloc_len;

    /* read the length to read from the file */
    read_len = (size_t)fp->read_uint2();

    /* add a byte for null termination */
    alloc_len = read_len + 1;

    /* if it won't fit in the temporary buffer, it's an error */
    if (alloc_len > buf_len)
    {
        /* log the error and return failure */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, err_if_too_long);
        return 1;
    }

    /* read the bytes into the caller's buffer */
    fp->read_bytes(buf, read_len);

    /* add null termination */
    buf[read_len] = '\0';

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Constant Value 
 */

/*
 *   set a string value 
 */
void CTcConstVal::set_sstr(const char *val, size_t len)
{
    /* store the type */
    typ_ = TC_CVT_SSTR;

    /* store a pointer to the string */
    val_.strval_.strval_ = val;
    val_.strval_.strval_len_ = len;

    /* for image file layout purposes, record the length of this string */
    G_cg->note_str(len);
}

/*
 *   set a list value 
 */
void CTcConstVal::set_list(CTPNList *lst)
{
    /* set the type */
    typ_ = TC_CVT_LIST;

    /* remember the list */
    val_.listval_ = lst;

    /* for image file layout purposes, record the length of this list */
    G_cg->note_list(lst->get_count());
}

/*
 *   Convert a value to a string 
 */
const char *CTcConstVal::cvt_to_str(char *buf, size_t bufl,
                                    size_t *result_len)
{
    /* check my type */
    switch(typ_)
    {
    case TC_CVT_NIL:
        /* the result is "nil" */
        if (bufl < 4)
            return 0;

        strcpy(buf, "nil");
        *result_len = 3;
        return buf;

    case TC_CVT_TRUE:
        /* the result is "true" */
        if (bufl < 5)
            return 0;

        strcpy(buf, "true");
        *result_len = 4;
        return buf;

    case TC_CVT_SSTR:
        /* it's already a string */
        *result_len = get_val_str_len();
        return get_val_str();

    case TC_CVT_INT:
        /* convert our signed integer value */
        if (bufl < 12)
            return 0;

        sprintf(buf, "%ld", get_val_int());
        *result_len = strlen(buf);
        return buf;

    case TC_CVT_FLOAT:
        /* we store these as strings */
        *result_len = get_val_float_len();
        return get_val_float();

    default:
        /* can't convert other types */
        return 0;
    }
}

/*
 *   Compare for equality to another constant value 
 */
int CTcConstVal::is_equal_to(const CTcConstVal *val) const
{
    CTPNListEle *ele1;
    CTPNListEle *ele2;
    
    /* 
     *   if the types are not equal, the values are not equal; otherwise,
     *   check the various types 
     */
    if (typ_ != val->get_type())
    {
        /* the types aren't equal, so the values are not equal */
        return FALSE;
    }

    /* the types are the same; do the comparison based on the type */
    switch(typ_)
    {
    case TC_CVT_UNK:
        /* unknown type; unknown values can never be equal */
        return FALSE;

    case TC_CVT_TRUE:
    case TC_CVT_NIL:
        /* 
         *   nil==nil and true==true; since we know the types are the
         *   same, the values are the same
         */
        return TRUE;

    case TC_CVT_INT:
        /* compare the integers */
        return (get_val_int() == val->get_val_int());

    case TC_CVT_SSTR:
        /* compare the strings */
        return (get_val_str_len() == val->get_val_str_len()
                && memcmp(get_val_str(), val->get_val_str(),
                          get_val_str_len()) == 0);
            
    case TC_CVT_LIST:
        /* 
         *   if the lists don't have the same number of elements, they're
         *   not equal 
         */
        if (get_val_list()->get_count() != val->get_val_list()->get_count())
            return FALSE;

        /* 
         *   compare each element of each list; if they're all the same,
         *   the values are the same 
         */
        ele1 = get_val_list()->get_head();
        ele2 = val->get_val_list()->get_head();
        for ( ; ele1 != 0 && ele2 != 0 ;
              ele1 = ele1->get_next(), ele2 = ele2->get_next())
        {
            /* if these elements aren't equal, the lists aren't equal */
            if (!ele1->get_expr()->get_const_val()
                ->is_equal_to(ele2->get_expr()->get_const_val()))
                return FALSE;
        }

        /* we didn't find any differences, so the lists are equal */
        return TRUE;

    case TC_CVT_OBJ:
        /* if the object values are the same, the values match */
        return (get_val_obj() == val->get_val_obj());

    case TC_CVT_PROP:
        /* if the property values are the same, the values match */
        return (get_val_prop() == val->get_val_prop());

    case TC_CVT_FUNCPTR:
        /* 
         *   if both symbols are the same, the values match; otherwise,
         *   they refer to different functions 
         */
        return (get_val_funcptr_sym() == val->get_val_funcptr_sym());

    default:
        /* unknown type; return unequal */
        return FALSE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Operator Parsers 
 */

/* ------------------------------------------------------------------------ */
/*
 *   Parse a left-associative binary operator 
 */
CTcPrsNode *CTcPrsOpBin::parse() const
{
    CTcPrsNode *lhs;
    CTcPrsNode *rhs;
    
    /* parse our left side - if that fails, return failure */
    if ((lhs = left_->parse()) == 0)
        return 0;

    /* keep going as long as we find our operator */
    for (;;)
    {
        /* check my operator */
        if (G_tok->cur() == get_op_tok())
        {
            CTcPrsNode *const_tree;

            /* skip the matching token */
            G_tok->next();
            
            /* parse the right-hand side */
            if ((rhs = right_->parse()) == 0)
                return 0;

            /* try folding our subnodes into a constant value, if possible */
            const_tree = eval_constant(lhs, rhs);

            /* 
             *   if we couldn't calculate a constant value, build the tree
             *   normally 
             */
            if (const_tree == 0)
            {
                /* 
                 *   Build my tree, then proceed to parse any additional
                 *   occurrences of our operator, with the result of
                 *   applying this occurrence of the operator as the
                 *   left-hand side of the new operator.  
                 */
                lhs = build_tree(lhs, rhs);
            }
            else
            {
                /* we got a constant value - use it as the result directly */
                lhs = const_tree;
            }
        }
        else
        {
            /* 
             *   it's not my operator - return what we thought might have
             *   been our left-hand side 
             */
            return lhs;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a group of left-associative binary operators at the same
 *   precedence level 
 */
CTcPrsNode *CTcPrsOpBinGroup::parse() const
{
    CTcPrsNode *lhs;

    /* parse our left side - if that fails, return failure */
    if ((lhs = left_->parse()) == 0)
        return 0;

    /* keep going as long as we find one of our operators */
    while (find_and_apply_op(&lhs)) ;

    /* return the expression tree */
    return lhs;
}

/*
 *   Find an apply one of our operators to the already-parsed left-hand
 *   side.  Returns true if we found an operator, false if not.  
 */
int CTcPrsOpBinGroup::find_and_apply_op(CTcPrsNode **lhs) const
{
    const CTcPrsOpBin *const *op;
    CTcPrsNode *rhs;

    /* check each operator at this precedence level */
    for (op = ops_ ; *op != 0 ; ++op)
    {
        /* check this operator's token */
        if (G_tok->cur() == (*op)->get_op_tok())
        {
            CTcPrsNode *const_tree;

            /* skip the operator token */
            G_tok->next();

            /* parse the right-hand side */
            if ((rhs = right_->parse()) == 0)
            {
                /* error - cancel the entire expression */
                *lhs = 0;
                return FALSE;
            }

            /* try folding our subnodes into a constant value */
            const_tree = (*op)->eval_constant(*lhs, rhs);

            /* 
             *   if we couldn't calculate a constant value, build the tree
             *   normally 
             */
            if (const_tree == 0)
            {
                /* 
                 *   build my tree, replacing the original left-hand side
                 *   with the new expression 
                 */
                *lhs = (*op)->build_tree(*lhs, rhs);
            }
            else
            {
                /* we got a constant value - use it as the result */
                *lhs = const_tree;
            }

            /*
             *   Tell the caller to proceed to parse any additional
             *   occurrences of our operator - this will apply the next
             *   occurrence of the operator as the left-hand side of the
             *   new operator.  
             */
            return TRUE;
        }
    }

    /* 
     *   if we got here, we didn't find an operator - tell the caller that
     *   we've reached the end of this operator's possible span
     */
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Comparison operator group 
 */
CTcPrsNode *CTcPrsOpBinGroupCompare::parse() const
{
    CTcPrsNode *lhs;

    /* parse our left side - if that fails, return failure */
    if ((lhs = left_->parse()) == 0)
        return 0;

    /* keep going as long as we find one of our operators */
    for (;;)
    {
        CTPNArglist *rhs;
        
        /* 
         *   try one of our regular operators - if we find it, go back for
         *   another round to see if there's another operator following
         *   the next expression
         */
        if (find_and_apply_op(&lhs))
            continue;

        /* 
         *   check for the 'is in' operator - 'is' and 'in' aren't
         *   keywords, so we must check for symbol tokens with the text of
         *   these context-sensitive keywords 
         */
        if (G_tok->cur() == TOKT_SYM
            && G_tok->getcur()->text_matches("is", 2))
        {
            /* we have 'is' - get the next token and check if it's 'in' */
            if (G_tok->next() == TOKT_SYM
                && G_tok->getcur()->text_matches("in", 2))
            {
                /* scan the expression list */
                rhs = parse_inlist();
                if (rhs == 0)
                    return 0;

                /* build the node */
                lhs = new CTPNIsIn(lhs, rhs);

                /* 
                 *   we've applied the 'is in' operator - go back for
                 *   another operator from the comparison group 
                 */
                continue;
            }
            else
            {
                /* it's not 'is in' - throw back the token and keep looking */
                G_tok->unget();
            }
        }

        /*
         *   Check for the 'not in' operator 
         */
        if (G_tok->cur() == TOKT_SYM
            && G_tok->getcur()->text_matches("not", 3))
        {
            /* we have 'is' - get the next token and check if it's 'in' */
            if (G_tok->next() == TOKT_SYM
                && G_tok->getcur()->text_matches("in", 2))
            {
                /* scan the expression list */
                rhs = parse_inlist();
                if (rhs == 0)
                    return 0;

                /* build the node */
                lhs = new CTPNNotIn(lhs, rhs);

                /* 
                 *   we've applied the 'is in' operator - go back for
                 *   another operator from the comparison group 
                 */
                continue;
            }
            else
            {
                /* it's not 'is in' - throw back the token and keep looking */
                G_tok->unget();
            }
        }

        /* we didn't find any of our operators - we're done */
        break;
    }

    /* return the expression */
    return lhs;
}

/*
 *   parse the list for the right-hand side of an 'is in' or 'not in'
 *   expression 
 */
CTPNArglist *CTcPrsOpBinGroupCompare::parse_inlist() const
{
    int argc;
    CTPNArg *arg_head;
    CTPNArg *arg_tail;

    /* skip the second keyword token, and check for an open paren */
    if (G_tok->next() == TOKT_LPAR)
    {
        /* skip the paren */
        G_tok->next();
    }
    else
    {
        /* 
         *   log an error, and keep going on the assumption that it was
         *   merely omitted and the rest of the list is well-formed 
         */
        G_tok->log_error_curtok(TCERR_IN_REQ_LPAR);
    }

    /* keep going until we find the close paren */
    for (argc = 0, arg_head = arg_tail = 0 ;; )
    {
        CTcPrsNode *expr;
        CTPNArg *arg_cur;

        /* if this is the close paren, we're done */
        if (G_tok->cur() == TOKT_RPAR)
            break;

        /* parse this expression */
        expr = S_op_asi.parse();
        if (expr == 0)
            return 0;

        /* count the argument */
        ++argc;

        /* create a new argument node */
        arg_cur = new CTPNArg(expr);

        /* 
         *   link the new node at the end of our list (this preserves the
         *   order of the original list) 
         */
        if (arg_tail != 0)
            arg_tail->set_next_arg(arg_cur);
        else
            arg_head = arg_cur;
        arg_tail = arg_cur;

        /* we need to be looking at a comma or right paren */
        if (G_tok->cur() == TOKT_RPAR)
        {
            /* that's the end of the list */
            break;
        }
        else if (G_tok->cur() == TOKT_COMMA)
        {
            /* skip the comma and parse the next argument */
            G_tok->next();
        }
        else
        {
            /* 
             *   If we're at the end of the file, there's no point
             *   proceding, so return failure.  If we've reached something
             *   that looks like a statement separator (semicolon, curly
             *   brace), also return failure, since the problem is clearly
             *   a missing right paren.  Otherwise, assume that a comma
             *   was missing and continue as though we have another
             *   argument.  
             */
            switch(G_tok->cur())
            {
            default:
                /* log an error */
                G_tok->log_error_curtok(TCERR_EXPECTED_IN_COMMA);

                /* 
                 *   if we're at the end of file, return what we have so
                 *   far; otherwise continue, assuming that they merely
                 *   left out a comma between two argument expressions 
                 */
                if (G_tok->cur() == TOKT_EOF)
                    return new CTPNArglist(argc, arg_head);
                break;

            case TOKT_SEM:
            case TOKT_LBRACE:
            case TOKT_RBRACE:
            case TOKT_DSTR_MID:
            case TOKT_DSTR_END:
                /* 
                 *   we're apparently at the end of the statement; flag
                 *   the error as a missing right paren, and return what
                 *   we have so far 
                 */
                G_tok->log_error_curtok(TCERR_EXPECTED_IN_RPAR);
                return new CTPNArglist(argc, arg_head);
            }
        }
    }

    /* skip the closing paren */
    G_tok->next();

    /* create and return the argument list descriptor */
    return new CTPNArglist(argc, arg_head);
}

/* ------------------------------------------------------------------------ */
/*
 *   Comma Operator 
 */

/*
 *   try to evaluate a constant expression 
 */
CTcPrsNode *CTcPrsOpComma::eval_constant(CTcPrsNode *left,
                                         CTcPrsNode *right) const
{
    /* 
     *   if both sides are constants, the result is the constant on the
     *   right side; we can't simply fold down to a right-side constant if
     *   the left side is not constant, though, because we must still
     *   evaluate the left side at run-time for any possible side effects 
     */
    if (left->is_const() && right->is_const())
    {
        /* both are constants - simply return the right constant value */
        return right;
    }
    else
    {
        /* 
         *   one or the other is non-constant, so we can't fold the
         *   expression - return null to so indicate 
         */
        return 0;
    }
}

/*
 *   build a subtree for the comma operator 
 */
CTcPrsNode *CTcPrsOpComma::build_tree(CTcPrsNode *left,
                                      CTcPrsNode *right) const
{
    return new CTPNComma(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   logical OR operator 
 */

/*
 *   try to evaluate a constant expression 
 */
CTcPrsNode *CTcPrsOpOr::eval_constant(CTcPrsNode *left,
                                      CTcPrsNode *right) const
{
    /* check for constants */
    if (left->is_const())
    {
        CTcPrsNode *ret;
        
        /* 
         *   Check for constants.  If the first expression is constant,
         *   the result will always be either 'true' (if the first
         *   expression's constant value is true), or the value of the
         *   second expression (if the first expression's constant value
         *   is 'nil').
         *   
         *   Note that it doesn't matter whether or not the right side is
         *   a constant.  If the left is true, the right will never be
         *   executed because of the short-circuit logic; if the left is
         *   nil, the result will always be the result of the right value.
         */
        if (left->get_const_val()->get_val_bool())
        {
            /* 
             *   the left is true, so the result is always true, and the
             *   right never gets executed 
             */
            ret = left;
        }
        else
        {
            /* the left is nil, so the result is the right value */
            ret = right;
        }

        /* ensure the result is a boolean value */
        if (ret->is_const())
        {
            /* make it a true/nil constant value */
            ret->get_const_val()
                ->set_bool(ret->get_const_val()->get_val_bool());
        }
        else
        {
            /* boolean-ize the value at run-time as needed */
            ret = new CTPNBoolize(ret);
        }

        /* return the result */
        return ret;
    }
    else
    {
        /* 
         *   one or the other is non-constant, so we can't fold the
         *   expression - return null to so indicate 
         */
        return 0;
    }
}

/*
 *   build the subtree
 */
CTcPrsNode *CTcPrsOpOr::build_tree(CTcPrsNode *left,
                                   CTcPrsNode *right) const
{
    return new CTPNOr(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   logical AND operator 
 */

/*
 *   try to evaluate a constant expression 
 */
CTcPrsNode *CTcPrsOpAnd::eval_constant(CTcPrsNode *left,
                                       CTcPrsNode *right) const
{
    /* 
     *   Check for constants.  If the first expression is constant, the
     *   result will always be either 'nil' (if the first expression's
     *   constant value is nil), or the value of the second expression (if
     *   the first expression's constant value is 'true').
     *   
     *   Note that it doesn't matter whether or not the right side is a
     *   constant.  If the left is nil, the right will never be executed
     *   because of the short-circuit logic; if the left is true, the
     *   result will always be the result of the right value.  
     */
    if (left->is_const())
    {
        CTcPrsNode *ret;
        
        /*
         *   The left value is a constant, so the result is always know.
         *   If the left value is nil, the result is nil; otherwise, it's
         *   the right half.  
         */
        if (left->get_const_val()->get_val_bool())
        {
            /* the left side is true - the result is the right side */
            ret = right;
        }
        else
        {
            /* 
             *   The left side is nil - the result is nil, and the right
             *   side never gets executed.
             */
            ret = left;
        }

        /* ensure the result is a boolean value */
        if (ret->is_const())
        {
            /* make it a true/nil constant value */
            ret->get_const_val()
                ->set_bool(ret->get_const_val()->get_val_bool());
        }
        else
        {
            /* boolean-ize the value at run-time as needed */
            ret = new CTPNBoolize(ret);
        }

        /* return the result */
        return ret;
    }
    else
    {
        /* 
         *   one or the other is non-constant, so we can't fold the
         *   expression - return null to so indicate 
         */
        return 0;
    }
}

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpAnd::build_tree(CTcPrsNode *left,
                                    CTcPrsNode *right) const
{
    return new CTPNAnd(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   Generic Comparison Operator parser base class 
 */

/*
 *   evaluate a constant expression 
 */
CTcPrsNode *CTcPrsOpRel::eval_constant(CTcPrsNode *left,
                                       CTcPrsNode *right) const
{
    /* check for constants */
    if (left->is_const() && right->is_const())
    {
        tc_constval_type_t typ1, typ2;
        int sense;

        /* get the types */
        typ1 = left->get_const_val()->get_type();
        typ2 = right->get_const_val()->get_type();

        /* determine what we're comparing */
        if (typ1 == TC_CVT_INT && typ2 == TC_CVT_INT)
        {
            long val1, val2;

            /* get the values */
            val1 = left->get_const_val()->get_val_int();
            val2 = right->get_const_val()->get_val_int();

            /* calculate the sense of the integer comparison */
            sense = (val1 < val2 ? -1 : val1 == val2 ? 0 : 1);
        }
        else if (typ1 == TC_CVT_SSTR && typ2 == TC_CVT_SSTR)
        {
            /* compare the string values */
            sense = strcmp(left->get_const_val()->get_val_str(),
                           right->get_const_val()->get_val_str());
        }
        else if (typ1 == TC_CVT_FLOAT || typ2 == TC_CVT_FLOAT)
        {
            /* we can't compare floats at compile time, but it's legal */
            return 0;
        }
        else
        {
            /* these types are incomparable */
            G_tok->log_error(TCERR_CONST_BAD_COMPARE,
                             G_tok->get_op_text(get_op_tok()));
            return 0;
        }

        /* set the result in the left value */
        left->get_const_val()->set_bool(get_bool_val(sense));

        /* return the updated left value */
        return left;
    }
    else
    {
        /* 
         *   one or the other is non-constant, so we can't fold the
         *   expression - return null to so indicate 
         */
        return 0;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   greater-than operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpGt::build_tree(CTcPrsNode *left,
                                   CTcPrsNode *right) const
{
    return new CTPNGt(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   less-than operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpLt::build_tree(CTcPrsNode *left,
                                   CTcPrsNode *right) const
{
    return new CTPNLt(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   greater-or-equal operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpGe::build_tree(CTcPrsNode *left,
                                   CTcPrsNode *right) const
{
    return new CTPNGe(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   less-or-equal operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpLe::build_tree(CTcPrsNode *left,
                                   CTcPrsNode *right) const
{
    return new CTPNLe(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   General equality/inequality operators base class 
 */

/*
 *   evaluate a constant expression 
 */
CTcPrsNode *CTcPrsOpEqComp::eval_constant(CTcPrsNode *left,
                                          CTcPrsNode *right) const
{
    int ops_equal;

    /* check for constants */
    if (left->is_const() && right->is_const())
    {
        /* both sides are constants - determine if they're equal */
        ops_equal = left->get_const_val()
                    ->is_equal_to(right->get_const_val());

        /* set the result in the left value */
        left->get_const_val()->set_bool(get_bool_val(ops_equal));

        /* return the updated left value */
        return left;
    }
    else if (left->is_addr() && right->is_addr())
    {
        CTcConstVal cval;
        int comparable;
        
        /* 
         *   both sides are addresses - if they're both addresses of the
         *   same subexpression, then the values are comparable as
         *   compile-time constants 
         */
        ops_equal = ((CTPNAddr *)left)
                    ->is_addr_eq((CTPNAddr *)right, &comparable);

        /* if they're not comparable, we can't fold this as a constant */
        if (!comparable)
            return 0;

        /* generate the appropriate boolean result for the comparison */
        cval.set_bool(get_bool_val(ops_equal));

        /* return a new constant node with the result */
        return new CTPNConst(&cval);
    }
    else
    {
        /* 
         *   one or the other is non-constant, so we can't fold the
         *   expression - return null to so indicate 
         */
        return 0;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   equality operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpEq::build_tree(CTcPrsNode *left,
                                   CTcPrsNode *right) const
{
    return new CTPNEq(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   inequality operator 
 */

/*
 *   build the subtree
 */
CTcPrsNode *CTcPrsOpNe::build_tree(CTcPrsNode *left,
                                   CTcPrsNode *right) const
{
    return new CTPNNe(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   'is in' operator 
 */

/*
 *   construct 
 */
CTPNIsInBase::CTPNIsInBase(CTcPrsNode *lhs, class CTPNArglist *rhs)
    : CTPNBin(lhs, rhs)
{
    /* presume we don't have a constant value */
    const_true_ = FALSE;
}

/*
 *   fold constants 
 */
CTcPrsNode *CTPNIsInBase::fold_binop()
{
    CTPNArglist *lst;
    CTPNArg *arg;
    CTPNArg *prv;
    CTPNArg *nxt;
    
    /* if the left-hand side isn't constant, there's nothing to do */
    if (!left_->is_const())
        return this;

    /* the right side is always an argument list */
    lst = (CTPNArglist *)right_;

    /* look for the value in the arguments */
    for (prv = 0, arg = lst->get_arg_list_head() ; arg != 0 ; arg = nxt)
    {
        /* remember the next argument, in case we eliminate this one */
        nxt = arg->get_next_arg();
        
        /* check to see if this argument is a constant */
        if (arg->is_const())
        {
            /*
             *   This one's a constant, so check to see if we found the
             *   left side value.  If the left side equals this value,
             *   note that we found the value.
             */
            if (left_->get_const_val()->is_equal_to(arg->get_const_val()))
            {
                /*
                 *   The values are equal, so the result of the expression
                 *   is definitely 'true'.  
                 */
                const_true_ = TRUE;

                /*
                 *   Because the 'is in' operator only evaluates operands
                 *   from the 'in' list until it finds one that matches,
                 *   any remaining operands will simply never be
                 *   evaluated.  We can thus discard the rest of the
                 *   argument list.  
                 */
                nxt = 0;
            }

            /*
             *   We now know whether the left side equals this constant
             *   list element.  This is never going to change because both
             *   values are constant, so there's no point in making this
             *   same comparison over and over again at run-time.  We can
             *   thus eliminate this argument from the list.  
             */
            lst->set_argc(lst->get_argc() - 1);
            if (prv == 0)
                lst->set_arg_list_head(nxt);
            else
                prv->set_next_arg(nxt);
        }
    }

    /*
     *   If the argument list is now completely empty, the result of the
     *   expression is a constant.  
     */
    if (lst->get_arg_list_head() == 0)
    {
        /* set the left operand's value to our result */
        left_->get_const_val()->set_bool(const_true_);
        
        /* return the constant value in place of the entire expression */
        return left_;
    }

    /* we're not a constant, to return myself unchanged */
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   'not in' operator 
 */

/*
 *   construct 
 */
CTPNNotInBase::CTPNNotInBase(CTcPrsNode *lhs, class CTPNArglist *rhs)
    : CTPNBin(lhs, rhs)
{
    /* presume we don't have a constant value */
    const_false_ = FALSE;
}

/*
 *   fold constants for binary operator 
 */
CTcPrsNode *CTPNNotInBase::fold_binop()
{
    CTPNArglist *lst;
    CTPNArg *arg;
    CTPNArg *prv;
    CTPNArg *nxt;

    /* if the left-hand side isn't constant, there's nothing to do */
    if (!left_->is_const())
        return this;

    /* the right side is always an argument list */
    lst = (CTPNArglist *)right_;

    /* look for the value in the arguments */
    for (prv = 0, arg = lst->get_arg_list_head() ; arg != 0 ; arg = nxt)
    {
        /* remember the next argument, in case we eliminate this one */
        nxt = arg->get_next_arg();

        /* check to see if this argument is a constant */
        if (arg->is_const())
        {
            /*
             *   This one's a constant, so check to see if we found the
             *   left side value.  If the left side equals this value,
             *   note that we found the value.
             */
            if (left_->get_const_val()->is_equal_to(arg->get_const_val()))
            {
                /*
                 *   The values are equal, so the result of the expression
                 *   is definitely 'nil'.  
                 */
                const_false_ = TRUE;

                /*
                 *   Because the 'not in' operator only evaluates operands
                 *   from the 'in' list until it finds one that matches,
                 *   any remaining operands will simply never be
                 *   evaluated.  We can thus discard the rest of the
                 *   argument list.  
                 */
                nxt = 0;
            }

            /*
             *   We now know whether the left side equals this constant
             *   list element.  This is never going to change because both
             *   values are constant, so there's no point in making this
             *   same comparison over and over again at run-time.  We can
             *   thus eliminate this argument from the list.  
             */
            lst->set_argc(lst->get_argc() - 1);
            if (prv == 0)
                lst->set_arg_list_head(nxt);
            else
                prv->set_next_arg(nxt);
        }
    }

    /*
     *   If the argument list is now completely empty, the result of the
     *   expression is a constant.  
     */
    if (lst->get_arg_list_head() == 0)
    {
        /* set the left operand's value to our result */
        left_->get_const_val()->set_bool(!const_false_);
        
        /* return the constant value in place of the entire expression */
        return left_;
    }

    /* we're not a constant, to return myself unchanged */
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   General arithmetic operator base class
 */

/*
 *   evaluate constant value 
 */
CTcPrsNode *CTcPrsOpArith::eval_constant(CTcPrsNode *left,
                                         CTcPrsNode *right) const
{
    /* check for constants */
    if (left->is_const() && right->is_const())
    {
        /* require that both values are integers or floats */
        if (left->get_const_val()->get_type() == TC_CVT_FLOAT
            || right->get_const_val()->get_type() == TC_CVT_FLOAT)
        {
            /* can't do it at compile time, but it's legal */
            return 0;
        }
        else if (left->get_const_val()->get_type() != TC_CVT_INT
            || right->get_const_val()->get_type() != TC_CVT_INT)
        {
            /* incompatible types - log an error */
            G_tok->log_error(TCERR_CONST_BINARY_REQ_NUM,
                             G_tok->get_op_text(get_op_tok()));
            return 0;
        }
        else
        {
            long result;
            
            /* calculate the result */
            result = calc_result(left->get_const_val()->get_val_int(),
                                 right->get_const_val()->get_val_int());

            /* assign the result back to the left operand */
            left->get_const_val()->set_int(result);
        }

        /* return the updated left value */
        return left;
    }
    else
    {
        /* 
         *   one or the other is non-constant, so we can't fold the
         *   expression - return null to so indicate 
         */
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise OR operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpBOr::build_tree(CTcPrsNode *left,
                                    CTcPrsNode *right) const
{
    return new CTPNBOr(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise AND operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpBAnd::build_tree(CTcPrsNode *left,
                                     CTcPrsNode *right) const
{
    return new CTPNBAnd(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise XOR operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpBXor::build_tree(CTcPrsNode *left,
                                     CTcPrsNode *right) const
{
    return new CTPNBXor(left, right);
}


/* ------------------------------------------------------------------------ */
/*
 *   shift left operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpShl::build_tree(CTcPrsNode *left,
                                    CTcPrsNode *right) const
{
    return new CTPNShl(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   shift right operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpShr::build_tree(CTcPrsNode *left,
                                    CTcPrsNode *right) const
{
    return new CTPNShr(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   multiplication operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpMul::build_tree(CTcPrsNode *left,
                                    CTcPrsNode *right) const
{
    return new CTPNMul(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   division operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpDiv::build_tree(CTcPrsNode *left,
                                    CTcPrsNode *right) const
{
    return new CTPNDiv(left, right);
}

/*
 *   evaluate constant result 
 */
long CTcPrsOpDiv::calc_result(long a, long b) const
{
    /* check for divide-by-zero */
    if (b == 0)
    {
        /* log a divide-by-zero error */
        G_tok->log_error(TCERR_CONST_DIV_ZERO);

        /* the result isn't really meaningful, but return something anyway */
        return 1;
    }
    else
    {
        /* return the result */
        return a / b;
    }
}
/* ------------------------------------------------------------------------ */
/*
 *   modulo operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpMod::build_tree(CTcPrsNode *left,
                                    CTcPrsNode *right) const
{
    return new CTPNMod(left, right);
}

/*
 *   evaluate constant result 
 */
long CTcPrsOpMod::calc_result(long a, long b) const
{
    /* check for divide-by-zero */
    if (b == 0)
    {
        /* log a divide-by-zero error */
        G_tok->log_error(TCERR_CONST_DIV_ZERO);

        /* the result isn't really meaningful, but return something anyway */
        return 1;
    }
    else
    {
        /* return the result */
        return a % b;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   subtraction operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpSub::build_tree(CTcPrsNode *left,
                                    CTcPrsNode *right) const
{
    return new CTPNSub(left, right);
}

/*
 *   evaluate a constant value 
 */
CTcPrsNode *CTcPrsOpSub::eval_constant(CTcPrsNode *left,
                                       CTcPrsNode *right) const
{
    if (left->is_const() && right->is_const())
    {
        tc_constval_type_t typ1, typ2;

        /* get the types */
        typ1 = left->get_const_val()->get_type();
        typ2 = right->get_const_val()->get_type();

        /* check our types */
        if (typ1 == TC_CVT_INT && typ2 == TC_CVT_INT)
        {
            /* calculate the integer sum */
            left->get_const_val()
                ->set_int(left->get_const_val()->get_val_int()
                          - right->get_const_val()->get_val_int());
        }
        else if (typ1 == TC_CVT_FLOAT || typ2 == TC_CVT_FLOAT)
        {
            /* can't fold float constants at compile time */
            return 0;
        }
        else if (typ1 == TC_CVT_LIST)
        {
            CTPNList *lst;

            /* get the original list */
            lst = left->get_const_val()->get_val_list();

            /* 
             *   if the right side is a list, remove each element of that
             *   list from the list on the left; otherwise, remove the
             *   value on the right from the list on the left 
             */
            if (typ2 == TC_CVT_LIST)
            {
                /* remove each element of the rhs list from the lhs list */
                CTPNListEle *ele;

                /* scan the list, adding each element */
                for (ele = right->get_const_val()
                           ->get_val_list()->get_head() ;
                     ele != 0 ; ele = ele->get_next())
                {
                    /* add this element's underlying expression value */
                    lst->remove_element(ele->get_expr()->get_const_val());
                }
            }
            else
            {
                /* remove the rhs value from the lhs list */
                lst->remove_element(right->get_const_val());
            }
        }
        else
        {
            /* these types are incompatible - log an error */
            G_tok->log_error(TCERR_CONST_BINMINUS_INCOMPAT);
            return 0;
        }

        /* return the updated left side */
        return left;
    }
    else
    {
        /* they're not constant - we can't generate a constant result */
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   addition operator 
 */

/*
 *   evaluate constant value 
 */
CTcPrsNode *CTcPrsOpAdd::eval_constant(CTcPrsNode *left,
                                       CTcPrsNode *right) const
{
    /* check for constants */
    if (left->is_const() && right->is_const())
    {
        tc_constval_type_t typ1, typ2;

        /* get the types */
        typ1 = left->get_const_val()->get_type();
        typ2 = right->get_const_val()->get_type();
        
        /* check our types */
        if (typ1 == TC_CVT_INT && typ2 == TC_CVT_INT)
        {
            /* calculate the integer sum */
            left->get_const_val()
                ->set_int(left->get_const_val()->get_val_int()
                          + right->get_const_val()->get_val_int());
        }
        else if (typ1 == TC_CVT_FLOAT || typ2 == TC_CVT_FLOAT)
        {
            /* can't fold float constants at compile time */
            return 0;
        }
        else if (typ1 == TC_CVT_LIST)
        {
            CTPNList *lst;

            /* get the original list */
            lst = left->get_const_val()->get_val_list();

            /* 
             *   if the right side is also a list, concatenate it onto the
             *   left list; otherwise, just add the right side as a new
             *   element to the existing list 
             */
            if (typ2 == TC_CVT_LIST)
            {
                CTPNListEle *ele;
                
                /* scan the list, adding each element */
                for (ele = right->get_const_val()
                           ->get_val_list()->get_head() ;
                     ele != 0 ; ele = ele->get_next())
                {
                    /* add this element's underlying expression value */
                    lst->add_element(ele->get_expr());
                }
            }
            else
            {
                /* add a new list element for the right side */
                lst->add_element(right);
            }

            /* 
             *   this list is longer than the original(s); tell the parser
             *   about it in case it's the longest list yet 
             */
            G_cg->note_list(lst->get_count());
        }
        else if (typ1 == TC_CVT_SSTR || typ2 == TC_CVT_SSTR)
        {
            char buf1[128];
            char buf2[128];
            const char *str1, *str2;
            size_t len1, len2;
            char *new_str;

            /* if the second value is a list, we can't make a constant */
            if (typ2 == TC_CVT_LIST)
                return 0;
            
            /* convert both values to strings if they're not already */
            str1 = left->get_const_val()
                   ->cvt_to_str(buf1, sizeof(buf1), &len1);
            str2 = right->get_const_val()
                   ->cvt_to_str(buf2, sizeof(buf2), &len2);

            /* 
             *   if we couldn't convert one or the other, leave the result
             *   non-constant 
             */
            if (str1 == 0 || str2 == 0)
                return 0;
            
            /* 
             *   allocate space in the node pool for the concatenation of
             *   the two strings - if that fails, don't bother with the
             *   concatenation 
             */
            new_str = (char *)G_prsmem->alloc(len1 + len2 + 1);
            if (new_str == 0)
                return 0;

            /* copy the two string values into the new space */
            memcpy(new_str, str1, len1);
            memcpy(new_str + len1, str2, len2);
            new_str[len1 + len2] = '\0';

            /* set the new value in the left node */
            left->get_const_val()->set_sstr(new_str, len1 + len2);
        }
        else
        {
            /* these types are incompatible - log an error */
            G_tok->log_error(TCERR_CONST_BINPLUS_INCOMPAT);
            return 0;
        }

        /* return the updated left value */
        return left;
    }
    else
    {
        /* the values aren't constant, so the result isn't constant */
        return 0;
    }
}


/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpAdd::build_tree(CTcPrsNode *left,
                                    CTcPrsNode *right) const
{
    return new CTPNAdd(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   Assignment Operator Group 
 */

/*
 *   parse an assignment expression 
 */
CTcPrsNode *CTcPrsOpAsi::parse() const
{
    CTcPrsNode *lhs;
    CTcPrsNode *rhs;
    tc_toktyp_t curtyp;
    
    /* start by parsing a conditional subexpression */
    lhs = S_op_if.parse();
    if (lhs == 0)
        return 0;

    /* get the next operator */
    curtyp = G_tok->cur();

    /* check to see if it's an assignment operator of some kind */
    switch(curtyp)
    {
    case TOKT_PLUSEQ:
    case TOKT_MINEQ:
    case TOKT_TIMESEQ:
    case TOKT_DIVEQ:
    case TOKT_MODEQ:
    case TOKT_ANDEQ:
    case TOKT_OREQ:
    case TOKT_XOREQ:
    case TOKT_SHLEQ:
    case TOKT_SHREQ:
        /* it's an assignment operator - process it */
        break;
        
    default:
        /* check against the current simple-assignment operator */
        if (curtyp == asi_op_)
        {
            /* it's an assignment operator - process it */
            break;
        }
        else
        {
            /* 
             *   it's not an assignment - return the original
             *   subexpression with no further elaboration 
             */
            return lhs;
        }
    }

    /* check for a valid lvalue */
    if (!lhs->check_lvalue())
    {
        /* log an error but continue parsing */
        G_tok->log_error(TCERR_INVALID_LVALUE,
                         G_tok->get_op_text(G_tok->cur()));
    }

    /* skip the assignment operator */
    G_tok->next();
    
    /* 
     *   Recursively parse an assignment subexpression.  Do this
     *   recursively rather than iteratively, because assignment operators
     *   group right-to-left.  By recursively parsing an assignment, our
     *   right-hand side will contain all remaining assignment expressions
     *   incorporated into it.  
     */
    rhs = parse();
    if (rhs == 0)
        return 0;

    /* build and return the result based on the operator type */
    switch(curtyp)
    {
    case TOKT_PLUSEQ:
        lhs = new CTPNAddAsi(lhs, rhs);
        break;
        
    case TOKT_MINEQ:
        lhs = new CTPNSubAsi(lhs, rhs);
        break;
        
    case TOKT_TIMESEQ:
        lhs = new CTPNMulAsi(lhs, rhs);
        break;

    case TOKT_DIVEQ:
        lhs = new CTPNDivAsi(lhs, rhs);
        break;

    case TOKT_MODEQ:
        lhs = new CTPNModAsi(lhs, rhs);
        break;

    case TOKT_ANDEQ:
        lhs = new CTPNBAndAsi(lhs, rhs);
        break;

    case TOKT_OREQ:
        lhs = new CTPNBOrAsi(lhs, rhs);
        break;

    case TOKT_XOREQ:
        lhs = new CTPNBXorAsi(lhs, rhs);
        break;

    case TOKT_SHLEQ:
        lhs = new CTPNShlAsi(lhs, rhs);
        break;

    case TOKT_SHREQ:
        lhs = new CTPNShrAsi(lhs, rhs);
        break;

    default:
        /* plain assignment operator */
        lhs = new CTPNAsi(lhs, rhs);
        break;
    }

    /* return the result */
    return lhs;
}

/* ------------------------------------------------------------------------ */
/*
 *   Tertiary Conditional Operator 
 */

CTcPrsNode *CTcPrsOpIf::parse() const
{
    CTcPrsNode *first;
    CTcPrsNode *second;
    CTcPrsNode *third;

    /* parse the conditional part */
    first = S_op_or.parse();
    if (first == 0)
        return 0;

    /* if we're not looking at the '?' operator, we're done */
    if (G_tok->cur() != TOKT_QUESTION)
        return first;

    /* skip the '?' operator */
    G_tok->next();

    /* 
     *   parse the second part, which can be any expression, including a
     *   double-quoted string expression or a comma expression (even though
     *   the '?:' operator overall has higher precedence than ',', we can't
     *   steal away operands from a ',' before our ':' because that would
     *   leave the ':' with nothing to go with) 
     */
    second = G_prs->parse_expr_or_dstr(TRUE);
    if (second == 0)
        return 0;
    
    /* make sure we have the ':' after the second part */
    if (G_tok->cur() != TOKT_COLON)
    {
        /* 
         *   log the error, but continue parsing as though we found the
         *   ':' - if the ':' is simply missing, this will allow us to
         *   recover and continue parsing the rest of the expression 
         */
        G_tok->log_error(TCERR_QUEST_WITHOUT_COLON);

        /* if we're at the end of file, there's no point in continuing */
        if (G_tok->cur() == TOKT_EOF)
            return 0;
    }
    
    /* skip the ':' */
    G_tok->next();
    
    /* 
     *   parse the third part, which can be any other expression, including a
     *   double-quoted string expression - but not a comma expression, since
     *   we have higher precedence than ',' 
     */
    third = G_prs->parse_expr_or_dstr(FALSE);
    if (third == 0)
        return 0;
        
    /* 
     *   If the condition is constant, we can choose the second or third
     *   expression directly.  It doesn't matter whether or not the second
     *   and/or third parts are themselves constant, because a constant
     *   condition means that we'll always execute only one of the
     *   alternatives.  
     */
    if (first->is_const())
    {
        /* 
         *   evaluate the conditional value as a true/false value, and
         *   return the second part's constant if the condition is true,
         *   or the third part's constant if the condition is false 
         */
        return (first->get_const_val()->get_val_bool()
                ? second : third);
    }
    else
    {
        /* it's not a constant value - return a new conditional node */
        return new CTPNIf(first, second, third);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Unary Operator Parser
 */

CTcPrsNode *CTcPrsOpUnary::parse() const
{
    CTcPrsNode *sub;
    tc_toktyp_t op;
    
    /* get the current token, which may be a prefix operator */
    op = G_tok->cur();

    /* check for prefix operators */
    switch(op)
    {
    case TOKT_AND:
        /* skip the '&' */
        G_tok->next();
        
        /* parse the address expression */
        return parse_addr();

    case TOKT_NOT:
    case TOKT_BNOT:
    case TOKT_PLUS:
    case TOKT_MINUS:
    case TOKT_INC:
    case TOKT_DEC:
    case TOKT_DELETE:
        /* skip the operator */
        G_tok->next();

        /* 
         *   recursively parse the unary expression to which to apply the
         *   operator 
         */
        sub = parse();
        if (sub == 0)
            return 0;

        /* apply the operator */
        switch(op)
        {
        case TOKT_NOT:
            /* apply the NOT operator */
            return parse_not(sub);

        case TOKT_BNOT:
            /* apply the bitwise NOT operator */
            return parse_bnot(sub);

        case TOKT_PLUS:
            /* apply the unary positive operator */
            return parse_pos(sub);

        case TOKT_MINUS:
            /* apply the unary negation operator */
            return parse_neg(sub);

        case TOKT_INC:
            /* apply the pre-increment operator */
            return parse_inc(TRUE, sub);

        case TOKT_DEC:
            /* apply the pre-decrement operator */
            return parse_dec(TRUE, sub);

        case TOKT_DELETE:
            /* apply the deletion operator */
            return parse_delete(sub);

        default:
            break;
        }

    default:
        /* it's not a unary prefix operator - parse a postfix expression */
        return parse_postfix(TRUE, TRUE);
    }
}

/*
 *   parse a unary NOT expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_not(CTcPrsNode *subexpr)
{
    CTcPrsNode *ret;

    /* try folding a constant value */
    ret = eval_const_not(subexpr);

    /* 
     *   if we got a constant result, return it; otherwise, create a NOT
     *   node for code generation 
     */
    if (ret != 0)
        return ret;
    else
        return new CTPNNot(subexpr);
}

/*
 *   evaluate a constant NOT expression 
 */
CTcPrsNode *CTcPrsOpUnary::eval_const_not(CTcPrsNode *subexpr)
{
    /* 
     *   if the underlying expression is a constant value, apply the
     *   operator 
     */
    if (subexpr->is_const())
    {
        /* set the new value */
        subexpr->get_const_val()
            ->set_bool(!subexpr->get_const_val()->get_val_bool());

        /* return the modified constant value */
        return subexpr;
    }

    /* the result is not constant */
    return 0;
}

/*
 *   parse a unary bitwise NOT expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_bnot(CTcPrsNode *subexpr)
{
    /* 
     *   if the underlying expression is a constant value, apply the
     *   operator 
     */
    if (subexpr->is_const())
    {
        /* we need an integer - log an error if it's not */
        if (subexpr->get_const_val()->get_type() != TC_CVT_INT)
            G_tok->log_error(TCERR_CONST_UNARY_REQ_NUM,
                             G_tok->get_op_text(TOKT_BNOT));
        else
            subexpr->get_const_val()
                ->set_int(~subexpr->get_const_val()->get_val_int());

        /* return the updated value */
        return subexpr;
    }
    
    /* create the bitwise NOT node */
    return new CTPNBNot(subexpr);
}

/*
 *   parse a unary address expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_addr() const
{
    CTcPrsNode *subexpr;

    /* 
     *   if it's a simple symbol, create an unresolved symbol node for it;
     *   otherwise parse the entire expression 
     */
    if (G_tok->cur() == TOKT_SYM)
    {
        const CTcToken *tok;
        
        /* 
         *   create an unresolved symbol node - we'll resolve this during
         *   code generation 
         */
        tok = G_tok->getcur();
        subexpr = new CTPNSym(tok->get_text(), tok->get_text_len());

        /*
         *   The address operator implies that the symbol is a property, so
         *   define the property symbol and mark it as referenced if we
         *   haven't already.  
         */
        G_prs->get_global_symtab()->find_or_def_prop_explicit(
            tok->get_text(), tok->get_text_len(), FALSE);

        /* skip the symbol */
        G_tok->next();
    }
    else
    {
        /* parse an expression */
        subexpr = parse();
        if (subexpr == 0)
            return 0;
    }

    /*
     *   The underlying expression must be something that has an address;
     *   if it's not, it's an error.  
     */
    if (!subexpr->has_addr())
    {
        /* 
         *   can't take the address of the subexpression - log an error,
         *   but continue parsing the expression anyway 
         */
        G_tok->log_error(TCERR_NO_ADDRESS);
    }
    
    /* create the address node */
    return new CTPNAddr(subexpr);
}

/*
 *   parse a unary arithmetic positive expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_pos(CTcPrsNode *subexpr)
{
    /* 
     *   if the underlying expression is a constant value, apply the
     *   operator 
     */
    if (subexpr->is_const())
    {
        /* if it's a float, a unary '+' has no effect at all */
        if (subexpr->get_const_val()->get_type() == TC_CVT_FLOAT)
            return subexpr;

        /* we need an integer - log an error if it's not */
        if (subexpr->get_const_val()->get_type() != TC_CVT_INT)
            G_tok->log_error(TCERR_CONST_UNARY_REQ_NUM,
                             G_tok->get_op_text(TOKT_PLUS));

        /* 
         *   positive-ing a value doesn't change the value, so return the
         *   original constant 
         */
        return subexpr;
    }
    
    /* create the unary positive node */
    return new CTPNPos(subexpr);
}

/*
 *   parse a unary arithmetic negation expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_neg(CTcPrsNode *subexpr)
{
    /* 
     *   if the underlying expression is a constant value, apply the
     *   operator 
     */
    if (subexpr->is_const())
    {
        /* we need an integer or float */
        if (subexpr->get_const_val()->get_type() == TC_CVT_INT)
        {
            /* set the value negative in the subexpression */
            subexpr->get_const_val()
                ->set_int(-(subexpr->get_const_val()->get_val_int()));
        }
        else if (subexpr->get_const_val()->get_type() == TC_CVT_FLOAT)
        {
            CTcConstVal *cval = subexpr->get_const_val();
            char *new_txt;
            
            /* allocate a buffer for a copy of the float text plus a '-' */
            new_txt = (char *)G_prsmem->alloc(cval->get_val_float_len() + 1);

            /* insert the minus sign */
            new_txt[0] = '-';

            /* add the original string */
            memcpy(new_txt + 1, cval->get_val_float(),
                   cval->get_val_float_len());

            /* update the subexpression's constant value to the new text */
            cval->set_float(new_txt, cval->get_val_float_len() + 1);
        }
        else
        {
            /* log the error */
            G_tok->log_error(TCERR_CONST_UNARY_REQ_NUM,
                             G_tok->get_op_text(TOKT_MINUS));
        }

        /* return the modified constant value */
        return subexpr;
    }

    /* create the unary negation node */
    return new CTPNNeg(subexpr);
}


/*
 *   parse a pre-increment expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_inc(int pre, CTcPrsNode *subexpr)
{
    /* require an lvalue */
    if (!subexpr->check_lvalue())
    {
        /* log an error, but continue parsing */
        G_tok->log_error(TCERR_INVALID_UNARY_LVALUE,
                         G_tok->get_op_text(TOKT_INC));
    }

    /* apply the increment operator */
    if (pre)
        return new CTPNPreInc(subexpr);
    else
        return new CTPNPostInc(subexpr);
}

/*
 *   parse a pre-decrement expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_dec(int pre, CTcPrsNode *subexpr)
{
    /* require an lvalue */
    if (!subexpr->check_lvalue())
    {
        /* log an error, but continue parsing */
        G_tok->log_error(TCERR_INVALID_UNARY_LVALUE,
                         G_tok->get_op_text(TOKT_INC));
    }

    /* apply the pre-increment operator */
    if (pre)
        return new CTPNPreDec(subexpr);
    else
        return new CTPNPostDec(subexpr);
}

/*
 *   parse a unary allocation expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_new(CTcPrsNode *subexpr, int is_transient)
{
    /* create the allocation node */
    return new CTPNNew(subexpr, is_transient);
}

/*
 *   parse a unary deletion expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_delete(CTcPrsNode *subexpr)
{
    /* the delete operator is obsolete in TADS 3 - warn about it */
    if (!G_prs->get_syntax_only())
        G_tok->log_warning(TCERR_DELETE_OBSOLETE);

    /* create the deletion node */
    return new CTPNDelete(subexpr);
}

/*
 *   parse a postfix expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_postfix(int allow_member_expr,
                                         int allow_call_expr)
{
    CTcPrsNode *sub;
    
    /* parse a primary expression */
    sub = parse_primary();
    if (sub == 0)
        return 0;

    /* keep going as long as we find postfix operators */
    for (;;)
    {
        tc_toktyp_t op;
        
        /* check for a postfix operator */
        op = G_tok->cur();
        switch(op)
        {
        case TOKT_LPAR:
            /* left paren - function or method call */
            if (allow_call_expr)
            {
                /* parse the call expression */
                sub = parse_call(sub);
            }
            else
            {
                /* call expressions aren't allowed - stop here */
                return sub;
            }
            break;

        case TOKT_LBRACK:
            /* left square bracket - subscript */
            sub = parse_subscript(sub);
            break;

        case TOKT_DOT:
            /* 
             *   Dot - member selection.  If a member expression is allowed
             *   by the caller, parse it; otherwise, just return the
             *   expression up to this point.  
             */
            if (allow_member_expr)
            {
                /* 
                 *   it's allowed - parse it and continue to look for other
                 *   postfix expressions following the member expression 
                 */
                sub = parse_member(sub);
            }
            else
            {
                /* 
                 *   member expressions aren't allowed - stop here,
                 *   returning the expression up to this point 
                 */
                return sub;
            }
            break;

        case TOKT_INC:
            /* post-increment */
            G_tok->next();
            sub = parse_inc(FALSE, sub);
            break;

        case TOKT_DEC:
            /* post-decrement */
            G_tok->next();
            sub = parse_dec(FALSE, sub);
            break;
            
        default:
            /* it's not a postfix operator - return the result */
            return sub;
        }

        /* if the last parse failed, return failure */
        if (sub == 0)
            return 0;
    }
}

/*
 *   Parse an argument list 
 */
CTPNArglist *CTcPrsOpUnary::parse_arg_list()
{
    int argc;
    CTPNArg *arg_head;

    /* skip the open paren */
    G_tok->next();

    /* keep going until we find the close paren */
    for (argc = 0, arg_head = 0 ;; )
    {
        CTcPrsNode *expr;
        CTPNArg *arg_cur;

        /* if this is the close paren, we're done */
        if (G_tok->cur() == TOKT_RPAR)
            break;

        /* parse this actual parameter expression */
        expr = S_op_asi.parse();
        if (expr == 0)
            return 0;

        /* count the argument */
        ++argc;

        /* create a new argument node */
        arg_cur = new CTPNArg(expr);

        /* check to see if the argument is followed by an ellipsis */
        if (G_tok->cur() == TOKT_ELLIPSIS)
        {
            /* skip the ellipsis */
            G_tok->next();

            /* mark the argument as a list-to-varargs parameter */
            arg_cur->set_varargs(TRUE);
        }

        /* 
         *   Link the new node in at the beginning of our list - this will
         *   ensure that the list is built in reverse order, which is the
         *   order in which we push the arguments onto the stack.
         */
        arg_cur->set_next_arg(arg_head);
        arg_head = arg_cur;

        /* we need to be looking at a comma, right paren, or ellipsis */
        if (G_tok->cur() == TOKT_RPAR)
        {
            /* that's the end of the list */
            break;
        }
        else if (G_tok->cur() == TOKT_COMMA)
        {
            /* skip the comma and parse the next argument */
            G_tok->next();
        }
        else
        {
            /* 
             *   If we're at the end of the file, there's no point
             *   proceding, so return failure.  If we've reached something
             *   that looks like a statement separator (semicolon, curly
             *   brace), also return failure, since the problem is clearly
             *   a missing right paren.  Otherwise, assume that a comma
             *   was missing and continue as though we have another
             *   argument.
             */
            switch(G_tok->cur())
            {
            default:
                /* log an error */
                G_tok->log_error_curtok(TCERR_EXPECTED_ARG_COMMA);

                /* 
                 *   if we're at the end of file, return what we have so
                 *   far; otherwise continue, assuming that they merely
                 *   left out a comma between two argument expressions 
                 */
                if (G_tok->cur() == TOKT_EOF)
                    return new CTPNArglist(argc, arg_head);
                break;

            case TOKT_SEM:
            case TOKT_LBRACE:
            case TOKT_RBRACE:
            case TOKT_DSTR_MID:
            case TOKT_DSTR_END:
                /* 
                 *   we're apparently at the end of the statement; flag
                 *   the error as a missing right paren, and return what
                 *   we have so far 
                 */
                G_tok->log_error_curtok(TCERR_EXPECTED_ARG_RPAR);
                return new CTPNArglist(argc, arg_head);
            }
        }
    }

    /* skip the closing paren */
    G_tok->next();

    /* create and return the argument list descriptor */
    return new CTPNArglist(argc, arg_head);
}

/*
 *   Parse a function call expression
 */
CTcPrsNode *CTcPrsOpUnary::parse_call(CTcPrsNode *lhs)
{
    CTPNArglist *arglist;
    
    /* parse the argument list */
    arglist = parse_arg_list();
    if (arglist == 0)
        return 0;

    /* build and return the function call node */
    return new CTPNCall(lhs, arglist);
}

/*
 *   Parse a subscript expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_subscript(CTcPrsNode *lhs)
{
    CTcPrsNode *subscript;
    CTcPrsNode *cval;
    
    /* skip the '[' */
    G_tok->next();

    /* parse the expression within the brackets */
    subscript = S_op_comma.parse();
    if (subscript == 0)
        return 0;

    /* check for the matching ']' */
    if (G_tok->cur() == TOKT_RBRACK)
    {
        /* got it - skip it */
        G_tok->next();
    }
    else
    {
        /* log an error, and forgive the missing ']' */
        G_tok->log_error_curtok(TCERR_EXPECTED_SUB_RBRACK);
    }

    /* try folding constants */
    cval = eval_const_subscript(lhs, subscript);

    /* 
     *   if that worked, use the result; otherwise, build an expression
     *   node to generate code for the subscript operator
     */
    if (cval != 0)
        return cval;
    else
        return new CTPNSubscript(lhs, subscript);
}

/*
 *   Evaluate a constant subscript value 
 */
CTcPrsNode *CTcPrsOpUnary::eval_const_subscript(CTcPrsNode *lhs,
                                                CTcPrsNode *subscript)
{
    /* 
     *   if we're subscripting a constant list by a constant index value,
     *   we can evaluate a constant result 
     */
    if (lhs->is_const() && subscript->is_const())
    {
        long idx;
        CTcPrsNode *ele;

        /* 
         *   make sure the index value is an integer and the value being
         *   indexed is a list; if either type is wrong, the indexing
         *   expression is invalid 
         */
        if (subscript->get_const_val()->get_type() != TC_CVT_INT)
        {
            /* we can't use a non-integer expression as a list index */
            G_tok->log_error(TCERR_CONST_IDX_NOT_INT);
        }
        else if (lhs->get_const_val()->get_type() != TC_CVT_LIST)
        {
            /* we can't index any constant type other than list */
            G_tok->log_error(TCERR_CONST_IDX_INV_TYPE);
        }
        else
        {
            /* get the index value */
            idx = subscript->get_const_val()->get_val_int();

            /* ask the list to look up the item by index */
            ele = lhs->get_const_val()->get_val_list()->get_const_ele(idx);

            /* if we got a valid result, return it */
            if (ele != 0)
                return ele;
        }
    }

    /* we couldn't fold it to a constant expression */
    return 0;
}

/*
 *   Parse a member selection ('.') expression.  If no '.' is actually
 *   present, then '.targetprop' is implied.  
 */
CTcPrsNode *CTcPrsOpUnary::parse_member(CTcPrsNode *lhs)
{
    CTcPrsNode *rhs;
    int rhs_is_expr;

    /*
     *   If a '.' is present, skip it; otherwise, '.targetprop' is implied. 
     */
    if (G_tok->cur() == TOKT_DOT)
    {
        /* we have an explicit property expression - skip the '.' */
        G_tok->next();

        /* assume the property will be a simple symbol, not an expression */
        rhs_is_expr = FALSE;

        /* we could have a symbol or a parenthesized expression */
        switch(G_tok->cur())
        {
        case TOKT_SYM:
            /* 
             *   It's a simple property name - create a symbol node.  Note
             *   that we must explicitly create an unresolved symbol node,
             *   since we want to ignore any local variable with the same
             *   name and look only in the global symbol table for a
             *   property; we must hence defer resolving the symbol until
             *   code generation.  
             */
            rhs = new CTPNSym(G_tok->getcur()->get_text(),
                              G_tok->getcur()->get_text_len());
            
            /* skip the symbol token */
            G_tok->next();
            
            /* proceed to check for an argument list */
            break;
            
        case TOKT_LPAR:
            /* 
             *   It's a parenthesized expression - parse it.  First, skip
             *   the open paren - we don't want the sub-expression to go
             *   beyond the close paren (if we didn't skip the open paren,
             *   the open paren would be part of the sub-expression, hence
             *   any postfix expression after the close paren would be
             *   considered part of the sub-expression; this would be
             *   invalid, since we might want to find a postfix actual
             *   parameter list).  
             */
            G_tok->next();
            
            /* remember that it's an expression */
            rhs_is_expr = TRUE;
            
            /* parse the sub-expression */
            rhs = S_op_comma.parse();
            if (rhs == 0)
                return 0;
            
            /* require the close paren */
            if (G_tok->cur() == TOKT_RPAR)
            {
                /* skip the closing paren */
                G_tok->next();
            }
            else
            {
                /* log the error */
                G_tok->log_error_curtok(TCERR_EXPR_MISSING_RPAR);
                
                /* 
                 *   if we're at a semicolon or end of file, we must be on
                 *   to the next statement - stop trying to parse this one
                 *   if so; otherwise, continue on the assumption that they
                 *   merely left out the close paren and what follows is
                 *   more expression for us to process 
                 */
                if (G_tok->cur() == TOKT_SEM || G_tok->cur() == TOKT_EOF)
                    return lhs;
            }
            break;
            
        case TOKT_TARGETPROP:
            /* 
             *   it's an unparenthesized "targetprop" expression - skip the
             *   keyword 
             */
            G_tok->next();
            
            /* 
             *   the property value is the result of evaluating
             *   "targetprop", which is an expression 
             */
            rhs = new CTPNTargetprop();
            rhs_is_expr = TRUE;

            /* note the reference to the extended method context */
            G_prs->set_full_method_ctx_referenced(TRUE);

            /* go parse the rest */
            break;
            
        default:
            /* anything else is invalid - log an error */
            G_tok->log_error_curtok(TCERR_INVALID_PROP_EXPR);
            
            /* skip the errant token so we don't loop on it */
            G_tok->next();
            
            /* return what we have so far */
            return lhs;
        }
    }
    else
    {
        /* there's no property specified, so '.targetprop' is implied */
        rhs = new CTPNTargetprop();
        rhs_is_expr = TRUE;

        /* 
         *   note the reference to the full method context (since
         *   'targetprop' is part of the extended method context beyond
         *   'self') 
         */
        G_prs->set_full_method_ctx_referenced(TRUE);
    }
        
    /* check for an argument list */
    if (G_tok->cur() == TOKT_LPAR)
    {
        CTPNArglist *arglist;
        
        /* parse the argument list */
        arglist = parse_arg_list();
        if (arglist == 0)
            return 0;

        /* create and return a member-with-arguments node */
        return new CTPNMemArg(lhs, rhs, rhs_is_expr, arglist);
    }
    else
    {
        /* 
         *   there's no argument list - create and return a simple member
         *   node 
         */
        return new CTPNMember(lhs, rhs, rhs_is_expr);
    }
}

/*
 *   Parse a double-quoted string with an embedded expression.  We treat
 *   this type of expression as though it were a comma expression. 
 */
CTcPrsNode *CTcPrsOpUnary::parse_dstr_embed()
{
    CTcPrsNode *cur;
    
    /* 
     *   First, create a node for the initial part of the string.  This is
     *   just an ordinary double-quoted string node. If the initial part of
     *   the string is zero-length, don't create an initial node at all,
     *   since this would just generate do-nothing code.  
     */
    if (G_tok->getcur()->get_text_len() != 0)
    {
        /* create the node for the initial part of the string */
        cur = new CTPNDstr(G_tok->getcur()->get_text(),
                           G_tok->getcur()->get_text_len());
    }
    else
    {
        /* 
         *   the initial part of the string is empty, so we don't need a node
         *   for this portion 
         */
        cur = 0;
    }

    /* skip the dstring */
    G_tok->next();

    /* keep going until we find the end of the string */
    for (;;)
    {
        CTcPrsNode *sub;
        int done;

        /* 
         *   parse the embedded expression, which can be any ordinary
         *   expression type, including a double-quoted string expression 
         */
        sub = G_prs->parse_expr_or_dstr(TRUE);
        if (sub == 0)
            return 0;

        /* build an embedding node for the expression */
        sub = new CTPNDstrEmbed(sub);
        

        /* 
         *   after the expression, we must find either another string
         *   segment with another embedded expression following, or the
         *   final string segment; anything else is an error 
         */
    do_next_segment:
        switch(G_tok->cur())
        {
        case TOKT_DSTR_MID:
            /* 
             *   It's a string with yet another embedded expression.
             *   Simply continue to the next segment. 
             */
            done = FALSE;
            break;

        case TOKT_DSTR_END:
            /* 
             *   It's the last segment of the string.  We can stop after
             *   processing this segment. 
             */
            done = TRUE;
            break;

        default:
            /* 
             *   anything else is invalid - we must find the end of the
             *   string.  Log an error. 
             */
            G_tok->log_error_curtok(TCERR_EXPECTED_DSTR_CONT);

            /* 
             *   if this is the end of the file, there's no point in
             *   continuing; return what we have so far 
             */
            if (G_tok->cur() == TOKT_EOF)
                return (cur != 0 ? cur : sub);

            /* tell the tokenizer to assume the missing '>>' */
            G_tok->assume_missing_dstr_cont();

            /* go back and try it again */
            goto do_next_segment;
        }

        /*
         *   Build a node representing everything so far: do this by
         *   combining the sub-expression with everything preceding, using a
         *   comma operator.  This isn't necessary if there's nothing
         *   preceding the sub-expression, since this means the
         *   sub-expression itself is everything so far.  
         */
        if (cur != 0)
            cur = new CTPNComma(cur, sub);
        else
            cur = sub;

        /*
         *   Combine the part so far with the next string segment, using a
         *   comma operator.  If the next string segment is empty, there's no
         *   need to add anything for it.  
         */
        if (G_tok->getcur()->get_text_len() != 0)
        {
            CTcPrsNode *newstr;

            /* create a node for the new string segment */
            newstr = new CTPNDstr(G_tok->getcur()->get_text(),
                                  G_tok->getcur()->get_text_len());

            /* combine it into the part so far with a comma operator */
            cur = new CTPNComma(cur, newstr);
        }

        /* skip this string part */
        G_tok->next();

        /* if that was the last segment, this is the final result */
        if (done)
            return cur;
    }
}

/*
 *   Parse a primary expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_primary()
{
    CTcPrsNode *sub;
    CTcConstVal cval;
    
    /* keep going until we find something interesting */
    for (;;)
    {
        /* determine what we have */
        switch(G_tok->cur())
        {
        case TOKT_LBRACE:
            /* short form of anonymous function */
            return parse_anon_func(TRUE);

        case TOKT_FUNCTION:
            /* anonymous function requires 'new' */
            G_tok->log_error(TCERR_ANON_FUNC_REQ_NEW);
            
            /* 
             *   parse it as an anonymous function anyway, even though the
             *   syntax isn't quite correct - the rest of it might still
             *   be okay, so we can at least continue parsing from here to
             *   find out 
             */
            return parse_anon_func(FALSE);

        case TOKT_NEW:
            /* skip the operator and check for 'function' */
            if (G_tok->next() == TOKT_FUNCTION)
            {
                /* it's an anonymous function definition - go parse it */
                sub = parse_anon_func(FALSE);
            }
            else
            {
                int trans;
                
                /* check for the 'transient' keyword */
                trans = (G_tok->cur() == TOKT_TRANSIENT);
                if (trans)
                    G_tok->next();
                
                /* 
                 *   it's an ordinary 'new' expression - parse the primary
                 *   making up the name 
                 */
                sub = parse_primary();

                /* if there's an argument list, parse the argument list */
                if (G_tok->cur() == TOKT_LPAR)
                    sub = parse_call(sub);

                /* create the 'new' node */
                sub = parse_new(sub, trans);
            }
            return sub;

        case TOKT_LPAR:
            /* left parenthesis - skip it */
            G_tok->next();
            
            /* parse the expression */
            sub = S_op_comma.parse();
            if (sub == 0)
                return 0;
            
            /* require the matching right parenthesis */
            if (G_tok->cur() == TOKT_RPAR)
            {
                /* skip the right paren */
                G_tok->next();
            }
            else
            {
                /* 
                 *   log an error; assume that the paren is simply
                 *   missing, so continue with our work 
                 */
                G_tok->log_error_curtok(TCERR_EXPR_MISSING_RPAR);
            }

            /* return the parenthesized expression */
            return sub;

        case TOKT_NIL:
            /* nil - the result is the constant value nil */
            cval.set_nil();
            
        return_constant:
            /* skip the token */
            G_tok->next();
            
            /* return a constant node */
            return new CTPNConst(&cval);
            
        case TOKT_TRUE:
            /* true - the result is the constant value true */
            cval.set_true();
            goto return_constant;
            
        case TOKT_INT:
            /* integer - the result is a constant integer value */
            cval.set_int(G_tok->getcur()->get_int_val());
            goto return_constant;

        case TOKT_FLOAT:
            /* floating point number */
            cval.set_float(G_tok->getcur()->get_text(),
                           G_tok->getcur()->get_text_len());
            goto return_constant;
            
        case TOKT_SSTR:
        handle_sstring:
            /* single-quoted string - the result is a constant string value */
            cval.set_sstr(G_tok->getcur()->get_text(),
                          G_tok->getcur()->get_text_len());
            goto return_constant;
            
        case TOKT_DSTR:
            /* 
             *   if we're in preprocessor expression mode, treat this the
             *   same as a single-quoted string 
             */
            if (G_prs->get_pp_expr_mode())
                goto handle_sstring;

            /* 
             *   a string implicitly references 'self', because we could run
             *   through the default output method on the current object 
             */
            G_prs->set_self_referenced(TRUE);
            
            /* build a double-quoted string node */
            sub = new CTPNDstr(G_tok->getcur()->get_text(),
                               G_tok->getcur()->get_text_len());
            
            /* skip the string */
            G_tok->next();
            
            /* return the new node */
            return sub;
            
        case TOKT_DSTR_START:
            /* a string implicitly references 'self' */
            G_prs->set_self_referenced(TRUE);

            /* parse the embedding expression */
            return parse_dstr_embed();
            
        case TOKT_LBRACK:
            /* parse the list */
            return parse_list();
            
        case TOKT_SYM:
            /* 
             *   symbol - the meaning of the symbol is not yet known, so
             *   create an unresolved symbol node 
             */
            sub = G_prs->create_sym_node(G_tok->getcur()->get_text(),
                                         G_tok->getcur()->get_text_len());
            
            /* skip the symbol token */
            G_tok->next();
            
            /* return the new node */
            return sub;

        case TOKT_SELF:
            /* note the explicit self-reference */
            G_prs->set_self_referenced(TRUE);

            /* generate the "self" node */
            G_tok->next();
            return new CTPNSelf();

        case TOKT_REPLACED:
            /* generate the "replaced" node */
            G_tok->next();
            return new CTPNReplaced();

        case TOKT_TARGETPROP:
            /* note the explicit extended method context reference */
            G_prs->set_full_method_ctx_referenced(TRUE);

            /* generate the "targetprop" node */
            G_tok->next();
            return new CTPNTargetprop();

        case TOKT_TARGETOBJ:
            /* note the explicit extended method context reference */
            G_prs->set_full_method_ctx_referenced(TRUE);

            /* generate the "targetobj" node */
            G_tok->next();
            return new CTPNTargetobj();

        case TOKT_DEFININGOBJ:
            /* note the explicit extended method context reference */
            G_prs->set_full_method_ctx_referenced(TRUE);

            /* generate the "definingobj" node */
            G_tok->next();
            return new CTPNDefiningobj();

        case TOKT_ARGCOUNT:
            /* generate the "argcount" node */
            G_tok->next();
            return new CTPNArgc();

        case TOKT_INHERITED:
            /* parse the "inherited" operation */
            return parse_inherited();

        case TOKT_DELEGATED:
            /* parse the "delegated" operation */
            return parse_delegated();
            
        case TOKT_RPAR:
            /* extra right paren - log an error */
            G_tok->log_error(TCERR_EXTRA_RPAR);

            /* skip it and go back for more */
            G_tok->next();
            break;

        case TOKT_RBRACK:
            /* extra right square bracket - log an error */
            G_tok->log_error(TCERR_EXTRA_RBRACK);

            /* skip it and go back for more */
            G_tok->next();
            break;

        case TOKT_DSTR_MID:
        case TOKT_DSTR_END:
        case TOKT_SEM:
        case TOKT_RBRACE:
            /* 
             *   this looks like the end of the statement, but we expected
             *   an operand - note the error and end the statement 
             */
            G_tok->log_error_curtok(TCERR_EXPECTED_OPERAND);

            /* 
             *   Synthesize a constant zero as the operand value.  Do not
             *   skip the current token, because it's almost certainly not
             *   meant to be part of the expression; we want to stay put
             *   so that the caller can resynchronize on this token. 
             */
            cval.set_int(G_tok->getcur()->get_int_val());
            return new CTPNConst(&cval);

        default:
            /* invalid primary expression - log the error */
            G_tok->log_error_curtok(TCERR_BAD_PRIMARY_EXPR);
            
            /* 
             *   Skip the token that's causing the problem; this will
             *   ensure that we don't loop indefinitely trying to figure
             *   out what this token is about, and return a constant zero
             *   value as the primary.  
             */
            G_tok->next();

            /* synthesize a constant zero as the operand value */
            cval.set_int(G_tok->getcur()->get_int_val());
            goto return_constant;
        }
    }
}

/*
 *   Parse an "inherited" expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_inherited()
{
    CTcPrsNode *lhs;
    
    /* skip the "inherited" keyword and check what follows */
    switch(G_tok->next())
    {
    case TOKT_SYM:
        /* 
         *   it's an "inherited superclass..." expression - set up the
         *   "inherited superclass" node 
         */
        lhs = new CTPNInhClass(G_tok->getcur()->get_text(),
                               G_tok->getcur()->get_text_len());

        /* skip the superclass token */
        G_tok->next();

        /* parse the member expression portion normally */
        break;

    case TOKT_LT:
        /* 
         *   '<' - this is the start of a multi-method type list.  Parse the
         *   list: type1 ',' type2 ',' ... '>', then the argument list to the
         *   'inherited' call.  
         */
        {
            /* create the formal type list */
            CTcFormalTypeList *tl = new (G_prsmem)CTcFormalTypeList();

            /* skip the '<' */
            G_tok->next();

            /* parse each list element */
            for (int done = FALSE ; !done ; )
            {
                switch (G_tok->cur())
                {
                case TOKT_GT:
                    /* end of the list - skip the '>', and we're done */
                    G_tok->next();
                    done = TRUE;
                    break;

                case TOKT_ELLIPSIS:
                    /* '...' */
                    tl->add_ellipsis();

                    /* this has to be the end of the list */
                    if (G_tok->next() == TOKT_GT)
                        G_tok->next();
                    else
                        G_tok->log_error_curtok(TCERR_MMINH_MISSING_GT);

                    /* assume the list ends here in any case */
                    done = TRUE;
                    break;

                case TOKT_SYM:
                    /* a type token - add it to the list */
                    tl->add_typed_param(G_tok->getcur());

                finish_type:
                    /* skip the type token */
                    switch (G_tok->next())
                    {
                    case TOKT_COMMA:
                        /* another type follows */
                        G_tok->next();
                        break;

                    case TOKT_GT:
                        G_tok->next();
                        done = TRUE;
                        break;

                    case TOKT_SYM:
                    case TOKT_ELLIPSIS:
                    case TOKT_TIMES:
                        /* probably just a missing comma */
                        G_tok->log_error_curtok(TCERR_MMINH_MISSING_COMMA);
                        break;

                    default:
                        /* anything else is an error */
                        G_tok->log_error_curtok(TCERR_MMINH_MISSING_COMMA);
                        G_tok->next();
                        break;
                    }
                    break;

                case TOKT_TIMES:
                    /* '*' indicates an untyped parameter */
                    tl->add_untyped_param();
                    goto finish_type;

                case TOKT_LPAR:
                    /* probably a missing '>' */
                    G_tok->log_error_curtok(TCERR_MMINH_MISSING_GT);
                    done = TRUE;
                    break;

                case TOKT_COMMA:
                    /* probably a missing type */
                    G_tok->log_error_curtok(TCERR_MMINH_MISSING_ARG_TYPE);
                    G_tok->next();
                    break;

                case TOKT_SEM:
                case TOKT_RPAR:
                case TOKT_EOF:
                    /* all of these indicate a premature end of the list */
                    G_tok->log_error_curtok(TCERR_MMINH_MISSING_ARG_TYPE);
                    return 0;

                default:
                    /* anything else is an error */
                    G_tok->log_error_curtok(TCERR_MMINH_MISSING_ARG_TYPE);
                    G_prs->skip_to_sem();
                    return 0;
                }
            }

            /* the left-hand side is an "inherited" node, with the arg list */
            lhs = new CTPNInh();
            ((CTPNInh *)lhs)->set_typelist(tl);

            /* an inherited<> expression must have an argument list */
            if (G_tok->cur() != TOKT_LPAR)
            {
                G_tok->log_error_curtok(TCERR_MMINH_MISSING_ARG_LIST);
                G_prs->skip_to_sem();
                return 0;
            }
        }
        break;
        
    default:
        /*
         *   There's no explicit superclass name listed, so the left-hand
         *   side of the '.' expression is the simple "inherited" node. 
         */
        lhs = new CTPNInh();

        /*
         *   Since we don't have an explicit superclass, we'll need the
         *   method context at run-time to establish the next class in
         *   inheritance order.  Flag the need for the full method context.  
         */
        G_prs->set_full_method_ctx_referenced(TRUE);

        /* parse the member expression portion normally */
        break;
    }

    /* parse and return the member expression */
    return parse_member(lhs);
}

/*
 *   Parse a "delegated" expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_delegated()
{
    CTcPrsNode *lhs;
    CTcPrsNode *target;

    /* 'delegated' always references 'self' */
    G_prs->set_self_referenced(TRUE);

    /* skip the "delegated" keyword */
    G_tok->next();

    /* 
     *   Parse a postfix expression giving the delegatee.  Don't allow
     *   nested member subexpressions (unless they're enclosed in
     *   parentheses, of course) - our implicit '.' postfix takes
     *   precedence.  Also, don't allow call subexpressions (unless enclosed
     *   in parens), since a postfix argument list binds to the 'delegated'
     *   expression, not to a subexpression involving a function/method
     *   call.  
     */
    target = parse_postfix(FALSE, FALSE);

    /* set up the "delegated" node */
    lhs = new CTPNDelegated(target);

    /* return the rest as a normal member expression */
    return parse_member(lhs);
}

/*
 *   Parse a list 
 */
CTcPrsNode *CTcPrsOpUnary::parse_list()
{
    CTPNList *lst;
    CTcPrsNode *ele;
    
    /* skip the opening '[' */
    G_tok->next();

    /* 
     *   create the list expression -- we'll add elements to the list as
     *   we parse the elements
     */
    lst = new CTPNList();

    /* scan all list elements */
    for (;;)
    {
        /* check what we have */
        switch(G_tok->cur())
        {
        case TOKT_RBRACK:
            /* 
             *   that's the end of the list - skip the closing bracket and
             *   return the finished list 
             */
            G_tok->next();
            goto done;
            
        case TOKT_EOF:
        case TOKT_RBRACE:
        case TOKT_SEM:
        case TOKT_DSTR_MID:
        case TOKT_DSTR_END:
            /* 
             *   these would all seem to imply that the closing ']' was
             *   missing from the list; flag the error and end the list
             *   now 
             */
            G_tok->log_error_curtok(TCERR_LIST_MISSING_RBRACK);
            goto done;

        case TOKT_RPAR:
            /* 
             *   extra right paren - log an error, but then skip the paren
             *   and try to keep parsing
             */
            G_tok->log_error(TCERR_LIST_EXTRA_RPAR);
            G_tok->next();
            break;

        default:
            /* it must be the next element expression */
            break;
        }

        /* 
         *   Attempt to parse another list element expression.  Parse just
         *   below a comma expression, because commas can be used to
         *   separate list elements.  
         */
        ele = S_op_asi.parse();
        if (ele == 0)
            return 0;
        
        /* add the element to the list */
        lst->add_element(ele);

        /* check what follows the element */
        switch(G_tok->cur())
        {
        case TOKT_COMMA:
            /* skip the comma introducing the next element */
            G_tok->next();

            /* if a close bracket follows, we seem to have an extra comma */
            if (G_tok->cur() == TOKT_RBRACK)
            {
                /* 
                 *   log an error about the missing element, then end the
                 *   list here 
                 */
                G_tok->log_error_curtok(TCERR_LIST_EXPECT_ELEMENT);
                goto done;
            }
            break;

        case TOKT_RBRACK:
            /* 
             *   we're done with the list - skip the bracket and return
             *   the finished list 
             */
            G_tok->next();
            goto done;

        case TOKT_EOF:
        case TOKT_LBRACE:
        case TOKT_RBRACE:
        case TOKT_SEM:
        case TOKT_DSTR_MID:
        case TOKT_DSTR_END:
            /* 
             *   these would all seem to imply that the closing ']' was
             *   missing from the list; flag the error and end the list
             *   now 
             */
            G_tok->log_error_curtok(TCERR_LIST_MISSING_RBRACK);
            goto done;

        default:
            /* 
             *   Anything else is an error - note that we expected a
             *   comma, then proceed with parsing from the current token
             *   as though we had found the comma (in all likelihood, the
             *   comma was accidentally omitted).  If we've reached the
             *   end of the file, return what we have so far, since it's
             *   pointless to keep looping.  
             */
            G_tok->log_error_curtok(TCERR_LIST_EXPECT_COMMA);

            /* give up on end of file, otherwise keep going */
            if (G_tok->cur() == TOKT_EOF)
                goto done;
            break;
        }
    }

done:
    /* tell the parser to note this list, in case it's the longest yet */
    G_cg->note_list(lst->get_count());

    /* return the list */
    return lst;
}


/* ------------------------------------------------------------------------ */
/*
 *   Parse Allocation Object 
 */

/*
 *   memory allocator for parse nodes
 */
void *CTcPrsAllocObj::operator new(size_t siz)
{
    /* allocate the space out of the node pool */
    return G_prsmem->alloc(siz);
}


/* ------------------------------------------------------------------------ */
/*
 *   Parse Tree space manager 
 */

/*
 *   create 
 */
CTcPrsMem::CTcPrsMem()
{
    /* we have no blocks yet */
    head_ = tail_ = 0;

    /* allocate our first block */
    alloc_block();
}

CTcPrsMem::~CTcPrsMem()
{
    /* delete all objects in our pool */
    delete_all();
}

/*
 *   Save state, for later resetting 
 */
void CTcPrsMem::save_state(tcprsmem_state_t *state)
{
    /* save the pool information in the state structure */
    state->tail = tail_;
    state->free_ptr = free_ptr_;
    state->rem = rem_;
}

/*
 *   Reset to initial state 
 */
void CTcPrsMem::reset()
{
    /* delete all blocks */
    delete_all();

    /* re-allocate the initial block */
    alloc_block();
}

/*
 *   Reset.  This deletes all objects allocated out of the parser pool
 *   since the state was saved.  
 */
void CTcPrsMem::reset(const tcprsmem_state_t *state)
{
    tcprsmem_blk_t *cur;
    
    /* 
     *   delete all of the blocks that were allocated after the last block
     *   that existed when the state was saved 
     */
    for (cur = state->tail->next_ ; cur != 0 ; )
    {
        tcprsmem_blk_t *nxt;

        /* remember the next block */
        nxt = cur->next_;

        /* delete this block */
        t3free(cur);

        /* move on to the next one */
        cur = nxt;
    }

    /* re-establish the saved last block */
    tail_ = state->tail;

    /* make sure the list is terminated at the last block */
    tail_->next_ = 0;

    /* re-establish the saved allocation point in the last block */
    free_ptr_ = state->free_ptr;
    rem_ = state->rem;
}

/*
 *   Delete all parser memory.  This deletes all objects allocated out of
 *   parser memory, so the caller must be sure that all of these objects
 *   are unreferenced.  
 */
void CTcPrsMem::delete_all()
{
    /* free all blocks */
    while (head_ != 0)
    {
        tcprsmem_blk_t *nxt;

        /* remember the next block after this one */
        nxt = head_->next_;

        /* free this block */
        t3free(head_);

        /* move on to the next one */
        head_ = nxt;
    }

    /* there's no tail now */
    tail_ = 0;
}

/*
 *   allocate a block 
 */
void CTcPrsMem::alloc_block()
{
    tcprsmem_blk_t *blk;

    /* 
     *   block size - pick a size that's large enough that we won't be
     *   unduly inefficient (in terms of having tons of blocks), but still
     *   friendly to 16-bit platforms (i.e., under 64k) 
     */
    const size_t BLOCK_SIZE = 65000;

    /* allocate space for the block */
    blk = (tcprsmem_blk_t *)t3malloc(sizeof(tcprsmem_blk_t) + BLOCK_SIZE - 1);

    /* if that failed, throw an error */
    if (blk == 0)
        err_throw(TCERR_NO_MEM_PRS_TREE);

    /* link in the block at the end of our list */
    blk->next_ = 0;
    if (tail_ != 0)
        tail_->next_ = blk;
    else
        head_ = blk;

    /* the block is now the last block in the list */
    tail_ = blk;

    /* 
     *   Set up to allocate out of our block.  Make sure the free pointer
     *   starts out on a worst-case alignment boundary; normally, the C++
     *   compiler will properly align our "buf_" structure member on a
     *   worst-case boundary, so this calculation won't actually change
     *   anything, but this will help ensure portability even to weird
     *   compilers.  
     */
    free_ptr_ = (char *)osrndpt((unsigned char *)blk->buf_);

    /* 
     *   get the amount of space remaining in the block (in the unlikely
     *   event that worst-case alignment actually moved the free pointer
     *   above the start of the buffer, we'll have lost a little space in
     *   the buffer for the alignment offset) 
     */
    rem_ = BLOCK_SIZE - (free_ptr_ - blk->buf_);
}

/*
 *   Allocate space 
 */
void *CTcPrsMem::alloc(size_t siz)
{
    char *ret;
    size_t space_used;

    /* if there's not enough space available, allocate a new block */
    if (siz > rem_)
    {
        /* allocate a new block */
        alloc_block();

        /* 
         *   if there's still not enough room, the request must exceed the
         *   largest block we can allocate 
         */
        if (siz > rem_)
            G_tok->throw_internal_error(TCERR_PRS_BLK_TOO_BIG, (ulong)siz);
    }

    /* return the free pointer */
    ret = free_ptr_;

    /* advance the free pointer past the space, rounding for alignment */
    free_ptr_ = (char *)osrndpt((unsigned char *)free_ptr_ + siz);

    /* deduct the amount of space we consumed from the available space */
    space_used = free_ptr_ - ret;
    if (space_used > rem_)
        rem_ = 0;
    else
        rem_ -= space_used;

    /* return the allocated space */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   parse node base class 
 */

/*
 *   By default, an expression cannot be used as a debugger expression 
 */
CTcPrsNode *CTcPrsNodeBase::adjust_for_debug(const tcpn_debug_info *info)
{
    err_throw(VMERR_INVAL_DBG_EXPR);
    AFTER_ERR_THROW(return 0;)
}


/* ------------------------------------------------------------------------ */
/*
 *   constant node 
 */

/*
 *   adjust for debugger use 
 */
CTcPrsNode *CTPNConstBase::adjust_for_debug(const tcpn_debug_info *info)
{
    /* convert to a debugger-constant */
    return new CTPNDebugConst(&val_);
}


/* ------------------------------------------------------------------------ */
/*
 *   List parse node 
 */

/*
 *   add an element to a list 
 */
void CTPNListBase::add_element(CTcPrsNode *expr)
{
    CTPNListEle *ele;
    
    /* create a list element object for the new element */
    ele = new CTPNListEle(expr);

    /* count the new entry */
    ++cnt_;

    /* add the element to our linked list */
    ele->set_prev(tail_);
    if (tail_ != 0)
        tail_->set_next(ele);
    else
        head_ = ele;
    tail_ = ele;

    /* 
     *   if the new element does not have a constant value, the list no
     *   longer has a constant value (if it did before) 
     */
    if (!expr->is_const())
        is_const_ = FALSE;
}

/*
 *   remove each occurrence of a given constant value from the list
 */
void CTPNListBase::remove_element(const CTcConstVal *val)
{
    CTPNListEle *cur;
    
    /* scan the list */
    for (cur = head_ ; cur != 0 ; cur = cur->get_next())
    {
        /* 
         *   if this element is constant, compare it to the value to be
         *   removed; if it matches, remove it 
         */
        if (cur->get_expr()->is_const()
            && cur->get_expr()->get_const_val()->is_equal_to(val))
        {
            /* set the previous element's forward pointer */
            if (cur->get_prev() == 0)
                head_ = cur->get_next();
            else
                cur->get_prev()->set_next(cur->get_next());

            /* set the next element's back pointer */
            if (cur->get_next() == 0)
                tail_ = cur->get_prev();
            else
                cur->get_next()->set_prev(cur->get_prev());

            /* decrement our element counter */
            --cnt_;
        }
    }
}

/*
 *   Get the constant value of the element at the given index.  Logs an
 *   error and returns null if there's no such element. 
 */
CTcPrsNode *CTPNListBase::get_const_ele(int index)
{
    CTPNListEle *ele;
    
    /* if the index is negative, it's out of range */
    if (index < 1)
    {
        /* log the error and return failure */
        G_tok->log_error(TCERR_CONST_IDX_RANGE);
        return 0;
    }

    /* scan the list for the given element */
    for (ele = head_ ; ele != 0 && index > 1 ;
         ele = ele->get_next(), --index) ;

    /* if we ran out of elements, the index is out of range */
    if (ele == 0 || index != 1)
    {
        G_tok->log_error(TCERR_CONST_IDX_RANGE);
        return 0;
    }

    /* return the element's constant value */
    return ele->get_expr();
}

/*
 *   Fold constants 
 */
CTcPrsNode *CTPNListBase::fold_constants(CTcPrsSymtab *symtab)
{
    CTPNListEle *cur;
    int all_const;

    /* 
     *   if the list is already constant, there's nothing extra we need to
     *   do 
     */
    if (is_const_)
        return this;

    /* presume the result will be all constants */
    all_const = TRUE;
    
    /* run through my list and fold each element */
    for (cur = head_ ; cur != 0 ; cur = cur->get_next())
    {
        /* fold this element */
        cur->fold_constants(symtab);

        /* 
         *   if this element is not a constant, the whole list cannot be
         *   constant 
         */
        if (!cur->get_expr()->is_const())
            all_const = FALSE;
    }

    /* if every element was a constant, the overall list is constant */
    if (all_const)
        is_const_ = TRUE;

    /* return myself */
    return this;
}

/*
 *   Adjust for debugging 
 */
CTcPrsNode *CTPNListBase::adjust_for_debug(const tcpn_debug_info *info)
{
    CTPNListEle *cur;

    /* run through my list and adjust each element */
    for (cur = head_ ; cur != 0 ; cur = cur->get_next())
    {
        /* adjust this element */
        cur->adjust_for_debug(info);
    }

    /* 
     *   force the list to be non-constant - in debugger mode, we have to
     *   build the value we push as a dynamic object, never as an actual
     *   constant, to ensure that the generated code can be deleted
     *   immediately after being executed 
     */
    is_const_ = FALSE;

    /* return myself */
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   conditional operator node base class 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNIfBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* fold constants in the subnodes */
    first_ = first_->fold_constants(symtab);
    second_ = second_->fold_constants(symtab);
    third_ = third_->fold_constants(symtab);

    /* 
     *   if the first is now a constant, we can fold this entire
     *   expression node by choosing the second or third based on its
     *   value; otherwise, return myself unchanged 
     */
    if (first_->is_const())
    {
        /* 
         *   the condition is a constant - the result is the 'then' or
         *   'else' part, based on the condition's value 
         */
        return (first_->get_const_val()->get_val_bool()
                ? second_ : third_);
    }
    else
    {
        /* we can't fold this node any further - return it unchanged */
        return this;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Double-quoted string node - base class 
 */

/* 
 *   create a double-quoted string node 
 */
CTPNDstrBase::CTPNDstrBase(const char *str, size_t len)
{
    /* remember the string */
    str_ = str;
    len_ = len;

    /* 
     *   note the length in the parser, in case it's the longest string
     *   we've seen so far 
     */
    G_cg->note_str(len);
}

/*
 *   adjust for debugger use 
 */
CTcPrsNode *CTPNDstrBase::adjust_for_debug(const tcpn_debug_info *info)
{
    /* 
     *   don't allow dstring evaluation in speculative mode, since we
     *   can't execute anything with side effects in this mode 
     */
    if (info->speculative)
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* return a debugger dstring node */
    return new CTPNDebugDstr(str_, len_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Address-of parse node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNAddrBase::fold_constants(CTcPrsSymtab *symtab)
{
    CTcPrsNode *ret;

    /* ask the symbol to generate a constant expression for its address */
    ret = get_sub_expr()->fold_addr_const(symtab);

    /* 
     *   if we got a constant value, return it; otherwise, return myself
     *   unchanged 
     */
    return (ret != 0 ? ret : this);
}

/*
 *   determine if my address equals that of another node 
 */
int CTPNAddrBase::is_addr_eq(const CTPNAddr *node, int *comparable) const
{
    /* 
     *   If both sides are symbols, the addresses are equal if and only if
     *   the symbols are identical.  One symbol has exactly one meaning in
     *   a given context, and no two symbols can have the same meaning.
     *   (It's important that we be able to state this for all symbols,
     *   because we can't necessarily know during parsing the meaning of a
     *   given symbol, since the symbol could be a forward reference.)  
     */
    if (get_sub_expr()->get_sym_text() != 0
        && node->get_sub_expr()->get_sym_text() != 0)
    {
        CTcPrsNode *sym1;
        CTcPrsNode *sym2;

        /* they're both symbols, so they're comparable */
        *comparable = TRUE;

        /* they're the same if both symbols have the same text */
        sym1 = get_sub_expr();
        sym2 = node->get_sub_expr();
        return (sym1->get_sym_text_len() == sym2->get_sym_text_len()
                && memcmp(sym1->get_sym_text(), sym2->get_sym_text(),
                          sym1->get_sym_text_len()) == 0);
    }

    /* they're not comparable */
    *comparable = FALSE;
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Symbol parse node base class 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNSymBase::fold_constants(CTcPrsSymtab *symtab)
{
    CTcSymbol *sym;
    CTcPrsNode *ret;

    /*
     *   Look up my symbol.  At this stage, don't assume a definition;
     *   merely look to see if it's already known.  We don't have enough
     *   information to determine how we should define the symbol, so
     *   leave it undefined until code generation if it's not already
     *   known.  
     */
    sym = symtab->find(get_sym_text(), get_sym_text_len());
    if (sym != 0)
    {
        /* ask the symbol to do the folding */
        ret = sym->fold_constant();

        /* if that succeeded, return it; otherwise, return unchanged */
        return (ret != 0 ? ret : this);
    }
    else
    {
        /* not defined - return myself unchanged */
        return this;
    }
}

/*
 *   Fold my address to a constant node.  If I have no address value, I'll
 *   simply return myself unchanged.  Note that it's an error if I have no
 *   constant value, but we'll count on the code generator to report the
 *   error, and simply ignore it for now.  
 */
CTcPrsNode *CTPNSymBase::fold_addr_const(CTcPrsSymtab *symtab)
{
    CTcSymbol *sym;

    /* look up my symbol; if we don't find it, don't define it */
    sym = symtab->find(get_sym_text(), get_sym_text_len());
    if (sym != 0)
    {
        /* we got a symbol - ask it to do the folding */
        return sym->fold_addr_const();
    }
    else
    {
        /* undefined symbol - there's no constant address value */
        return 0;
    }
}

/*
 *   Determine if I have a return value when called 
 */
int CTPNSymBase::has_return_value_on_call() const
{
    CTcSymbol *sym;
    
    /* try resolving my symbol */
    sym = G_prs->get_global_symtab()->find(sym_, len_);

    /* 
     *   if we found a symbol, let it resolve the call; otherwise, assume
     *   that we do have a return value 
     */
    if (sym != 0)
        return sym->has_return_value_on_call();
    else
        return TRUE;
}

/*
 *   Determine if I am a valid lvalue 
 */
int CTPNSymBase::check_lvalue_resolved(class CTcPrsSymtab *symtab) const
{
    CTcSymbol *sym;

    /* look up the symbol in the given scope */
    sym = symtab->find(get_sym_text(), get_sym_text_len());
    if (sym != 0)
    {
        /* ask the symbol what it thinks */
        return sym->check_lvalue();
    }
    else
    {
        /* it's undefined - can't be an lvalue */
        return FALSE;
    }
}

/*
 *   Adjust for debugger use 
 */
CTcPrsNode *CTPNSymBase::adjust_for_debug(const tcpn_debug_info *)
{
    /* 
     *   If this symbol isn't defined in the global symbol table, we can't
     *   evaluate this expression in the debugger - new symbols can never
     *   be defined in the debugger, so there's no point in trying to hold
     *   a forward reference as we normally would for an undefined symbol.
     *   We need look only in the global symbol table because local
     *   symbols will already have been resolved.  
     */
    if (G_prs->get_global_symtab()->find(sym_, len_) == 0)
    {
        /* log the error, to generate an appropriate message */
        G_tok->log_error(TCERR_UNDEF_SYM, (int)len_, sym_);
        
        /* throw the error as well */
        err_throw_a(TCERR_UNDEF_SYM, 2,
                    ERR_TYPE_INT, (int)len_, ERR_TYPE_TEXTCHAR, sym_);
    }
    
    /* return myself unchanged */
    return this;
}


/* ------------------------------------------------------------------------ */
/*
 *   Resolved Symbol parse node base class 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNSymResolvedBase::fold_constants(CTcPrsSymtab *symtab)
{
    CTcPrsNode *ret;

    /* ask the symbol to generate the folded constant value */
    ret = sym_->fold_constant();

    /* if that succeeded, return it; otherwise, return myself unchanged */
    return (ret != 0 ? ret : this);
}

/*
 *   Fold my address to a constant node.  If I have no address value, I'll
 *   simply return myself unchanged.  Note that it's an error if I have no
 *   constant value, but we'll count on the code generator to report the
 *   error, and simply ignore it for now.  
 */
CTcPrsNode *CTPNSymResolvedBase::fold_addr_const(CTcPrsSymtab *symtab)
{
    /* ask my symbol to generate the folded constant value */
    return sym_->fold_addr_const();
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugger local variable resolved symbol 
 */

/*
 *   construction 
 */
CTPNSymDebugLocalBase::CTPNSymDebugLocalBase(const tcprsdbg_sym_info *info)
{
    /* save the type information */
    switch(info->sym_type)
    {
    case TC_SYM_LOCAL:
        var_id_ = info->var_id;
        ctx_arr_idx_ = info->ctx_arr_idx;
        frame_idx_ = info->frame_idx;
        is_param_ = FALSE;
        break;

    case TC_SYM_PARAM:
        var_id_ = info->var_id;
        ctx_arr_idx_ = 0;
        frame_idx_ = info->frame_idx;
        is_param_ = TRUE;
        break;

    default:
        /* other types are invalid */
        assert(FALSE);
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Argument List parse node base class 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNArglistBase::fold_constants(CTcPrsSymtab *symtab)
{
    CTPNArg *cur;
    
    /* fold each list element */
    for (cur = get_arg_list_head() ; cur != 0 ; cur = cur->get_next_arg())
        cur->fold_constants(symtab);

    /* return myself with no further folding */
    return this;
}

/*
 *   adjust for debugger use 
 */
CTcPrsNode *CTPNArglistBase::adjust_for_debug(const tcpn_debug_info *info)
{
    CTPNArg *arg;
    
    /* adjust each argument list member */
    for (arg = list_ ; arg != 0 ; arg = arg->get_next_arg())
    {
        /* 
         *   adjust this argument; assume the argument node itself isn't
         *   affected 
         */
        arg->adjust_for_debug(info);
    }

    /* return myself otherwise unchanged */
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   Argument List Entry parse node base class 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNArgBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* fold my argument expression */
    arg_expr_ = arg_expr_->fold_constants(symtab);

    /* return myself unchanged */
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   Member with no arguments 
 */

/*
 *   adjust for debugger use
 */
CTcPrsNode *CTPNMemberBase::adjust_for_debug(const tcpn_debug_info *info)
{
    /* adjust the object and property expressions */
    obj_expr_ = obj_expr_->adjust_for_debug(info);
    prop_expr_ = prop_expr_->adjust_for_debug(info);

    /* return myself otherwise unchanged */
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   Member with Arguments parse node base class 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNMemArgBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* fold constants in the object and property expressions */
    obj_expr_ = obj_expr_->fold_constants(symtab);
    prop_expr_ = prop_expr_->fold_constants(symtab);

    /* 
     *   fold constants in the argument list; an argument list node never
     *   changes to a new node type during constant folding, so we don't
     *   need to update the member 
     */
    arglist_->fold_constants(symtab);

    /* return myself with no further evaluation */
    return this;
}

/* 
 *   adjust for debugger use 
 */
CTcPrsNode *CTPNMemArgBase::adjust_for_debug(const tcpn_debug_info *info)
{
    /* don't allow in speculative mode due to possible side effects */
    if (info->speculative)
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* 
     *   adjust my object expression, property expression, and argument
     *   list 
     */
    obj_expr_ = obj_expr_->adjust_for_debug(info);
    prop_expr_ = prop_expr_->adjust_for_debug(info);
    arglist_->adjust_for_debug(info);

    /* return myself otherwise unchanged */
    return this;
}


/* ------------------------------------------------------------------------ */
/*
 *   Function/Method Call parse node base class 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNCallBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* fold my function expression */
    func_ = func_->fold_constants(symtab);

    /* fold my argument list */
    arglist_->fold_constants(symtab);

    /* return myself unchanged */
    return this;
}

/*
 *   adjust for debugger use 
 */
CTcPrsNode *CTPNCallBase::adjust_for_debug(const tcpn_debug_info *info)
{
    /* don't allow in speculative mode because of side effects */
    if (info->speculative)
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* adjust the function expression */
    func_ = func_->adjust_for_debug(info);
    
    /* adjust the argument list (assume it doesn't change) */
    arglist_->adjust_for_debug(info);
    
    /* return myself otherwise unchanged */
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parser Symbol Table implementation 
 */

/* static hash function */
CVmHashFunc *CTcPrsSymtab::hash_func_ = 0;

/*
 *   allocate parser symbol tables out of the parser memory pool 
 */
void *CTcPrsSymtab::operator new(size_t siz)
{
    return G_prsmem->alloc(siz);
}

/*
 *   static initialization 
 */
void CTcPrsSymtab::s_init()
{
    /* create our static hash function */
    if (hash_func_ == 0)
        hash_func_ = new CVmHashFuncCS();
}

/*
 *   static termination 
 */
void CTcPrsSymtab::s_terminate()
{
    /* delete our static hash function */
    if (hash_func_ != 0)
    {
        delete hash_func_;
        hash_func_ = 0;
    }
}

/*
 *   initialize 
 */
CTcPrsSymtab::CTcPrsSymtab(CTcPrsSymtab *parent_scope)
{
    size_t hash_table_size;
    
    /* 
     *   Create the hash table.  If we're at global scope (parent_scope ==
     *   0), create a large hash table, since we'll probably add lots of
     *   symbols; otherwise, it's just a local table, which probably won't
     *   have many entries, so create a small table.
     *   
     *   Note that we always use the static hash function object, hence
     *   the table doesn't own the object.  
     */
    hash_table_size = (parent_scope == 0 ? 512 : 32);
    hashtab_ = new (G_prsmem)
               CVmHashTable(hash_table_size, hash_func_, FALSE,
                            new (G_prsmem) CVmHashEntry *[hash_table_size]);

    /* remember the parent scope */
    parent_ = parent_scope;

    /* we're not in a debugger frame list yet */
    list_index_ = 0;
    list_next_ = 0;
}

/*
 *   delete 
 */
CTcPrsSymtab::~CTcPrsSymtab()
{
    /* delete our underlying hash table */
    delete hashtab_;
}

/*
 *   Find a symbol, marking it as referenced if we find it.   
 */
CTcSymbol *CTcPrsSymtab::find(const textchar_t *sym, size_t len,
                              CTcPrsSymtab **symtab)
{
    CTcSymbol *entry;

    /* find the symbol */
    entry = find_noref(sym, len, symtab);

    /* if we found an entry, mark it as referenced */
    if (entry != 0)
        entry->mark_referenced();

    /* return the result */
    return entry;
}

/*
 *   Find a symbol.  This base version does not affect the 'referenced'
 *   status of the symbol we look up.  
 */
CTcSymbol *CTcPrsSymtab::find_noref(const textchar_t *sym, size_t len,
                                    CTcPrsSymtab **symtab)
{
    CTcPrsSymtab *curtab;

    /*
     *   Look for the symbol.  Start in the current symbol table, and work
     *   outwards to the outermost enclosing table.  
     */
    for (curtab = this ; curtab != 0 ; curtab = curtab->get_parent())
    {
        CTcSymbol *entry;

        /* look for the symbol in this table */
        if ((entry = curtab->find_direct(sym, len)) != 0)
        {
            /* 
             *   found it - if the caller wants to know about the symbol
             *   table in which we actually found the symbol, return that
             *   information 
             */
            if (symtab != 0)
                *symtab = curtab;

            /* return the symbol table entry we found */
            return entry;
        }
    }

    /* we didn't find the symbol - return failure */
    return 0;
}

/*
 *   Find a symbol directly in this table 
 */
CTcSymbol *CTcPrsSymtab::find_direct(const textchar_t *sym, size_t len)
{
    /* return the entry from our hash table */
    return (CTcSymbol *)get_hashtab()->find(sym, len);
}

/*
 *   Add a formal parameter symbol 
 */
void CTcPrsSymtab::add_formal(const textchar_t *sym, size_t len,
                              int formal_num, int copy_str)
{
    CTcSymLocal *lcl;
    
    /* 
     *   Make sure it's not already defined in our own symbol table - if
     *   it is, log an error and ignore the redundant definition.  (We
     *   only care about our own scope, not enclosing scopes, since it's
     *   perfectly fine to hide variables in enclosing scopes.)  
     */
    if (get_hashtab()->find(sym, len) != 0)
    {
        /* log an error */
        G_tok->log_error(TCERR_FORMAL_REDEF, (int)len, sym);

        /* don't add the symbol again */
        return;
    }

    /* create the symbol entry */
    lcl = new CTcSymLocal(sym, len, copy_str, TRUE, formal_num);

    /* add it to the table */
    get_hashtab()->add(lcl);
}

/*
 *   Add a symbol to the table 
 */
void CTcPrsSymtab::add_entry(CTcSymbol *sym)
{
    /* add it to the table */
    get_hashtab()->add(sym);
}

/*
 *   remove a symbol from the table 
 */
void CTcPrsSymtab::remove_entry(CTcSymbol *sym)
{
    /* remove it from the underyling hash table */
    get_hashtab()->remove(sym);
}

/*
 *   Add a local variable symbol 
 */
CTcSymLocal *CTcPrsSymtab::add_local(const textchar_t *sym, size_t len,
                                     int local_num, int copy_str,
                                     int init_assigned, int init_referenced)
{
    CTcSymLocal *lcl;

    /* 
     *   make sure the symbol isn't already defined in this scope; if it
     *   is, log an error 
     */
    if (get_hashtab()->find(sym, len) != 0)
    {
        /* log the error */
        G_tok->log_error(TCERR_LOCAL_REDEF, (int)len, sym);

        /* don't create the symbol again - return the original definition */
        return 0;
    }

    /* create the symbol entry */
    lcl = new CTcSymLocal(sym, len, copy_str, FALSE, local_num);

    /* 
     *   if the symbol is initially to be marked as referenced or
     *   assigned, mark it now 
     */
    if (init_assigned)
        lcl->set_val_assigned(TRUE);
    if (init_referenced)
        lcl->set_val_used(TRUE);
    
    /* add it to the table */
    get_hashtab()->add(lcl);

    /* return the new local */
    return lcl;
}

/*
 *   Add a code label ('goto') symbol
 */
CTcSymLabel *CTcPrsSymtab::add_code_label(const textchar_t *sym, size_t len,
                                          int copy_str)
{
    CTcSymLabel *lbl;

    /* 
     *   make sure the symbol isn't already defined in this scope; if it
     *   is, log an error 
     */
    if (get_hashtab()->find(sym, len) != 0)
    {
        /* log the error */
        G_tok->log_error(TCERR_CODE_LABEL_REDEF, (int)len, sym);

        /* don't create the symbol again - return the original definition */
        return 0;
    }

    /* create the symbol entry */
    lbl = new CTcSymLabel(sym, len, copy_str);

    /* add it to the table */
    get_hashtab()->add(lbl);

    /* return the new label */
    return lbl;
}


/* 
 *   Find a symbol; if the symbol isn't defined, add a new entry according
 *   to the action flag.  Because we add a symbol entry if the symbol
 *   isn't defined, this *always* returns a valid symbol object.  
 */
CTcSymbol *CTcPrsSymtab::find_or_def(const textchar_t *sym, size_t len,
                                     int copy_str, tcprs_undef_action action)
{
    CTcSymbol *entry;
    CTcPrsSymtab *curtab;

    /*
     *   Look for the symbol.  Start in the current symbol table, and work
     *   outwards to the outermost enclosing table. 
     */
    for (curtab = this ; ; curtab = curtab->get_parent())
    {
        /* look for the symbol in this table */
        entry = (CTcSymbol *)curtab->get_hashtab()->find(sym, len);
        if (entry != 0)
        {
            /* mark the entry as referenced */
            entry->mark_referenced();

            /* found it - return the symbol */
            return entry;
        }

        /* 
         *   If there's no parent symbol table, the symbol is undefined.
         *   Add a new symbol according to the action parameter.  Note
         *   that we always add the new symbol at global scope, hence we
         *   add it to 'curtab', not 'this'.  
         */
        if (curtab->get_parent() == 0)
        {
            /* check which action we're being asked to perform */
            switch(action)
            {
            case TCPRS_UNDEF_ADD_UNDEF:
                /* add an "undefined" entry - log an error */
                G_tok->log_error(TCERR_UNDEF_SYM, (int)len, sym);

                /* create a new symbol of type undefined */
                entry = new CTcSymUndef(sym, len, copy_str);

                /* finish up */
                goto add_entry;

            case TCPRS_UNDEF_ADD_PROP:
                /* add a new "property" entry - log a warning */
                G_tok->log_warning(TCERR_ASSUME_SYM_PROP, (int)len, sym);

                /* create a new symbol of type property */
                entry = new CTcSymProp(sym, len, copy_str,
                                       G_cg->new_prop_id());

                /* finish up */
                goto add_entry;

            case TCPRS_UNDEF_ADD_PROP_NO_WARNING:
                /* add a new property entry with no warning */
                entry = new CTcSymProp(sym, len, copy_str,
                                       G_cg->new_prop_id());

                /* finish up */
                goto add_entry;

            add_entry:
                /* add the new entry to the global table */
                add_to_global_symtab(curtab, entry);

                /* return the new entry */
                return entry;
            }
        }
    }
}

/*
 *   Enumerate the entries in a symbol table 
 */
void CTcPrsSymtab::enum_entries(void (*func)(void *, CTcSymbol *),
                                void *ctx)
{
    /* 
     *   Ask the hash table to perform the enumeration.  We know that all
     *   of our entries in the symbol table are CTcSymbol objects, so we
     *   can coerce the callback function to the appropriate type without
     *   danger. 
     */
    get_hashtab()->enum_entries((void (*)(void *, CVmHashEntry *))func, ctx);
}

/*
 *   Scan the symbol table for unreferenced local variables 
 */
void CTcPrsSymtab::check_unreferenced_locals()
{
    /* skip the check if we're only parsing for syntax */
    if (!G_prs->get_syntax_only())
    {
        /* run the symbols through our unreferenced local check callback */
        enum_entries(&unref_local_cb, this);
    }
}

/*
 *   Enumeration callback to check for unreferenced locals 
 */
void CTcPrsSymtab::unref_local_cb(void *, CTcSymbol *sym)
{
    /* check the symbol */
    sym->check_local_references();
}

/* ------------------------------------------------------------------------ */
/*
 *   Comma node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNCommaBase::fold_binop()
{
    /* use the normal addition folder */
    return S_op_comma.eval_constant(left_, right_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Addition parse node 
 */

/* 
 *   Fold constants.  We override the default fold_constants() for
 *   addition nodes because addition constancy can be affected by symbol
 *   resolution.  In particular, if we resolve symbols in a list, the list
 *   could turn constant, which could in turn make the result of an
 *   addition operator with the list as an operand turn constant.  
 */
CTcPrsNode *CTPNAddBase::fold_binop()
{
    /* use the normal addition folder */
    return S_op_add.eval_constant(left_, right_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Subtraction parse node 
 */

/* 
 *   Fold constants.  We override the default fold_constants() for the
 *   subtraction node because subtraction constancy can be affected by
 *   symbol resolution.  In particular, if we resolve symbols in a list,
 *   the list could turn constant, which could in turn make the result of
 *   a subtraction operator with the list as an operand turn constant.  
 */
CTcPrsNode *CTPNSubBase::fold_binop()
{
    /* use the normal addition folder */
    return S_op_sub.eval_constant(left_, right_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Equality Comparison parse node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNEqBase::fold_binop()
{
    /* use the normal addition folder */
    return S_op_eq.eval_constant(left_, right_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Inequality Comparison parse node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNNeBase::fold_binop()
{
    /* use the normal addition folder */
    return S_op_ne.eval_constant(left_, right_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Logical AND parse node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNAndBase::fold_binop()
{
    /* use the normal addition folder */
    return S_op_and.eval_constant(left_, right_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Logical OR parse node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNOrBase::fold_binop()
{
    /* use the normal addition folder */
    return S_op_or.eval_constant(left_, right_);
}

/* ------------------------------------------------------------------------ */
/*
 *   NOT parse node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNNotBase::fold_unop()
{
    /* use the normal addition folder */
    return S_op_unary.eval_const_not(sub_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Subscript parse node
 */

/* 
 *   Fold constants.  We override the default fold_constants() for
 *   subscript nodes because subscript constancy can be affected by symbol
 *   resolution.  In particular, if we resolve symbols in a list, the list
 *   could turn constant, which could in turn make the result of a
 *   subscript operator with the list as an operand turn constant.  
 */
/* ------------------------------------------------------------------------ */
/*
 *   Equality Comparison parse node 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNSubscriptBase::fold_binop()
{
    /* use the normal addition folder */
    return S_op_unary.eval_const_subscript(left_, right_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Parser Symbol Table Entry base class 
 */

/*
 *   Allocate symbol objects from the parse pool, since these objects have
 *   all of the lifespan characteristics of pool objects.  
 */
void *CTcSymbolBase::operator new(size_t siz)
{
    return G_prsmem->alloc(siz);
}

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
    fp->write_int2((int)get_type());

    /* write my name */
    return write_name_to_file(fp);
}

/*
 *   Write the symbol name to a file 
 */
int CTcSymbolBase::write_name_to_file(CVmFile *fp)
{
    /* write the length of my symbol name, followed by the symbol name */
    fp->write_int2((int)get_sym_len());

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
        return CTcSymFunc::read_from_sym_file(fp);

    case TC_SYM_OBJ:
        return CTcSymObj::read_from_sym_file(fp);

    case TC_SYM_PROP:
        return CTcSymProp::read_from_sym_file(fp);

    case TC_SYM_ENUM:
        return CTcSymEnum::read_from_sym_file(fp);

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
 *   function symbol entry base 
 */

/*
 *   fold function name into a function address
 */
CTcPrsNode *CTcSymFuncBase::fold_constant()
{
    CTcConstVal cval;

    /* set up the function pointer constant */
    cval.set_funcptr((CTcSymFunc *)this);

    /* return a constant node with the function pointer */
    return new CTPNConst(&cval);
}

/*
 *   Write to a symbol file 
 */
int CTcSymFuncBase::write_to_sym_file(CVmFile *fp)
{
    char buf[5];
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
    buf[2] = (varargs_ != 0);
    buf[3] = (has_retval_ != 0);
    buf[4] = (is_multimethod_ ? 1 : 0)
             | (is_multimethod_base_ ? 2 : 0);
    fp->write_bytes(buf, 5);

    /* we wrote the symbol */
    return TRUE;
}

/*
 *   add an absolute fixup to my list 
 */
void CTcSymFuncBase::add_abs_fixup(CTcDataStream *ds, ulong ofs)
{
    /* ask the code body to add the fixup */
    CTcAbsFixup::add_abs_fixup(&fixups_, ds, ofs);
}

/*
 *   add an absolute fixup at the current stream offset 
 */
void CTcSymFuncBase::add_abs_fixup(CTcDataStream *ds)
{
    /* ask the code body to add the fixup */
    CTcAbsFixup::add_abs_fixup(&fixups_, ds, ds->get_ofs());
}

/*
 *   Read from a symbol file
 */
CTcSymbol *CTcSymFuncBase::read_from_sym_file(CVmFile *fp)
{
    char symbuf[4096];
    const char *sym;
    char info[5];
    int argc;
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

    /* read the argument count, varargs flag, and return value flag */
    fp->read_bytes(info, 5);
    argc = osrp2(info);
    varargs = (info[2] != 0);
    has_retval = (info[3] != 0);
    is_multimethod = ((info[4] & 1) != 0);
    is_multimethod_base = ((info[4] & 2) != 0);

    /* create and return the new symbol */
    return new CTcSymFunc(sym, strlen(sym), FALSE, argc,
                          varargs, has_retval,
                          is_multimethod, is_multimethod_base, TRUE);
}

/*
 *   Write to an object file 
 */
int CTcSymFuncBase::write_to_obj_file(CVmFile *fp)
{
    char buf[10];
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
    buf[2] = (varargs_ != 0);
    buf[3] = (has_retval_ != 0);
    buf[4] = (is_extern_ != 0);
    buf[5] = (ext_replace_ != 0);
    buf[6] = (ext_modify != 0);
    buf[7] = (is_multimethod_ ? 1 : 0)
             | (is_multimethod_base_ ? 2 : 0);
    oswp2(buf + 8, mod_body_cnt);
    fp->write_bytes(buf, 10);

    /* if we modify an external, save its fixup list */
    if (ext_modify)
        CTcAbsFixup::write_fixup_list_to_object_file(fp, last_mod->fixups_);

    /* write the code stream offsets of the modified base function bodies */
    for (cur = get_mod_base() ; cur != 0 ; cur = cur->get_mod_base())
    {
        /* if this one has a code body, write its code stream offset */
        if (cur->get_code_body() != 0)
            fp->write_int4(cur->get_anchor()->get_ofs());
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
 *   local variable symbol entry base 
 */

/*
 *   initialize 
 */
CTcSymLocalBase::CTcSymLocalBase(const char *str, size_t len, int copy,
                                 int is_param, int var_num)
    : CTcSymbol(str, len, copy, (is_param ? TC_SYM_PARAM : TC_SYM_LOCAL))
{
    /* remember the information */
    var_num_ = var_num;
    is_param_ = is_param;

    /* presume it's a regular stack variable (not a context local) */
    is_ctx_local_ = FALSE;
    ctx_orig_ = 0;
    ctx_var_num_ = 0;
    ctx_level_ = 0;
    ctx_var_num_set_ = FALSE;

    /* so far, the value isn't used anywhere */
    val_used_ = FALSE;
    val_assigned_ = FALSE;

    /* the symbol has not been referenced so far */
    referenced_ = FALSE;

    /* remember where the symbol is defined in the source file */
    G_tok->get_last_pos(&src_desc_, &src_linenum_);
}

/*
 *   Mark the value of the variable as used 
 */
void CTcSymLocalBase::set_val_used(int f)
{
    /* note the new status */
    val_used_ = f;

    /* if we have now assigned the value, propagate to the original */
    if (f && ctx_orig_ != 0)
        ctx_orig_->set_val_used(TRUE);
}

/*
 *   Mark the value of the variable as assigned
 */
void CTcSymLocalBase::set_val_assigned(int f)
{
    /* note the new status */
    val_assigned_ = f;

    /* if we have now assigned the value, propagate to the original */
    if (f && ctx_orig_ != 0)
        ctx_orig_->set_val_assigned(TRUE);
}

/*
 *   Check for references to this local 
 */
void CTcSymLocalBase::check_local_references()
{
    int err;
    tc_severity_t sev = TC_SEV_WARNING;
    
    /* 
     *   if this isn't an original, but is simply a copy of a variable
     *   inherited from an enclosing scope (such as into an anonymous
     *   function), don't bother even checking for errors - we'll let the
     *   original symbol take care of reporting its own errors 
     */
    if (ctx_orig_ != 0)
        return;

    /* 
     *   if it's unreferenced or unassigned (or both), log an error; note
     *   that a formal parameter is always assigned, since the value is
     *   assigned by the caller 
     */
    if (!get_val_used() && (!get_val_assigned() && !is_param()))
    {
        /* the variable is never used at all */
        err = TCERR_UNREFERENCED_LOCAL;
    }
    else if (!get_val_used())
    {
        if (is_param() || is_list_param())
        {
            /* 
             *   it's a parameter, or a local that actually contains a
             *   varargs parameter list - generate only a pedantic error 
             */
            sev = TC_SEV_PEDANTIC;
            err = TCERR_UNREFERENCED_PARAM;
        }
        else
        {
            /* this local is assigned a value that's never used */
            err = TCERR_UNUSED_LOCAL_ASSIGNMENT;
        }
    }
    else if (!get_val_assigned() && !is_param())
    {
        /* it's used but never assigned */
        err = TCERR_UNASSIGNED_LOCAL;
    }
    else
    {
        /* no error */
        return;
    }

    /* 
     *   display the warning message, showing the error location as the
     *   source line where the local was defined 
     */
    G_tcmain->log_error(get_src_desc(), get_src_linenum(),
                        sev, err, (int)get_sym_len(), get_sym());
}

/*
 *   create a new context variable copy of this symbol 
 */
CTcSymbol *CTcSymLocalBase::new_ctx_var() const
{
    CTcSymLocal *lcl;
    
    /* create a new local with the same name */
    lcl = new CTcSymLocal(get_sym(), get_sym_len(), FALSE, FALSE, 0);

    /* refer the copy back to the original (i.e., me) */
    lcl->set_ctx_orig((CTcSymLocal *)this);

    /* set up the context variable information */
    if (!is_ctx_local_)
    {
        /* 
         *   The original is a true local - we're at the first context
         *   level, and we don't yet have a property assigned, since we
         *   don't know if this variable is actually going to be
         *   referenced. 
         */
        lcl->set_ctx_level(1);
    }
    else
    {
        /* 
         *   The original was already a context variable - we're at one
         *   higher context level in this function, and we use the same
         *   context property as the original did.  
         */
        lcl->set_ctx_level(ctx_level_ + 1);
    }

    /* return the new symbol */
    return lcl;
}

/*
 *   Apply context variable conversion 
 */
int CTcSymLocalBase::apply_ctx_var_conv(CTcPrsSymtab *symtab,
                                        CTPNCodeBody *code_body)
{
    /* 
     *   if this symbol isn't referenced, simply delete it from the table,
     *   so that it doesn't get entered in the debug records; and there's
     *   no need to propagate it back to the enclosing scope as a context
     *   variable, since it's not referenced from this enclosed scope 
     */
    if (!referenced_)
    {
        /* remove the symbol from the table */
        symtab->remove_entry(this);

        /* this variable doesn't need to be converted */
        return FALSE;
    }

    /* 
     *   convert the symbol in the enclosing scope to a context local, if
     *   it's not already 
     */
    if (ctx_orig_ != 0)
    {
        /* convert the original to a context symbol */
        ctx_orig_->convert_to_ctx_var(get_val_used(), get_val_assigned());

        /* 
         *   ask the code body for the context object's local variable for
         *   our recursion level 
         */
        ctx_var_num_ = code_body->get_or_add_ctx_var_for_level(ctx_level_);

        /* note that we've set our context variable ID */
        ctx_var_num_set_ = TRUE;

        /* this variable was converted */
        return TRUE;
    }

    /* this variable wasn't converted */
    return FALSE;
}

/*
 *   convert this variable to a context variable 
 */
void CTcSymLocalBase::convert_to_ctx_var(int val_used, int val_assigned)
{
    /* if I'm not already a context local, mark me as a context local */
    if (!is_ctx_local_)
    {
        /* mark myself as a context local */
        is_ctx_local_ = TRUE;

        /* 
         *   we haven't yet assigned our local context variable, since
         *   we're still processing the inner scope at this point; just
         *   store placeholders for now so we know to come back and fix
         *   this up 
         */
        ctx_var_num_ = 0;
        ctx_arr_idx_ = 0;
    }

    /* note that I've been referenced */
    mark_referenced();

    /* propagate the value-used and value-assigned flags */
    if (val_used)
        set_val_used(TRUE);
    if (val_assigned)
        set_val_assigned(TRUE);
        
    /* propagate the conversion to the original symbol */
    if (ctx_orig_ != 0)
        ctx_orig_->convert_to_ctx_var(val_used, val_assigned);
}

/*
 *   finish the context variable conversion 
 */
void CTcSymLocalBase::finish_ctx_var_conv()
{
    /* 
     *   If this isn't already marked as a context variable, there's
     *   nothing to do - this variable must not have been referenced from
     *   an anonymous function yet, and hence can be kept in the stack.
     *   
     *   Similarly, if my context local variable number has been assigned
     *   already, there's nothing to do - we must have been set to refer
     *   back to a context variable in an enclosing scope (this can happen
     *   in a nested anonymous function).
     */
    if (!is_ctx_local_ || ctx_var_num_set_)
        return;

    /* 
     *   tell the parser to create a local context for this scope, if it
     *   hasn't already 
     */
    G_prs->init_local_ctx();

    /* use the local context variable specified by the parser */
    ctx_var_num_ = G_prs->get_local_ctx_var();
    ctx_var_num_set_ = TRUE;

    /* assign our array index */
    if (ctx_arr_idx_ == 0)
        ctx_arr_idx_ = G_prs->alloc_ctx_arr_idx();
}

/*
 *   Get my context variable array index 
 */
int CTcSymLocalBase::get_ctx_arr_idx() const
{
    /* 
     *   if I'm based on an original symbol from another scope, use the
     *   same property ID as the original symbol 
     */
    if (ctx_orig_ != 0)
        return ctx_orig_->get_ctx_arr_idx();

    /* return my context property */
    return ctx_arr_idx_;
}

/* ------------------------------------------------------------------------ */
/*
 *   object symbol entry base 
 */

/*
 *   fold the symbol as a constant 
 */
CTcPrsNode *CTcSymObjBase::fold_constant()
{
    CTcConstVal cval;

    /* set up the object constant */
    cval.set_obj(get_obj_id(), get_metaclass());

    /* return a constant node */
    return new CTPNConst(&cval);
}

/*
 *   Write to a symbol file 
 */
int CTcSymObjBase::write_to_sym_file(CVmFile *fp)
{
    int result;
    
    /* 
     *   If we're external, don't bother writing to the file - if we're
     *   importing an object, we don't want to export it as well.  If it's
     *   modified, don't write it either, because modified symbols cannot
     *   be referenced directly by name (the symbol for a modified object
     *   is a fake symbol anyway).  In addition, don't write the symbol if
     *   it's a 'modify' or 'replace' definition that applies to an
     *   external base object - instead, we'll pick up the symbol from the
     *   other symbol file with the original definition.  
     */
    if (is_extern_ || modified_ || ext_modify_ || ext_replace_)
        return FALSE;

    /* inherit default */
    result =  CTcSymbol::write_to_sym_file(fp);

    /* if that was successful, write additional object-type-specific data */
    if (result)
    {
        /* write the metaclass ID */
        fp->write_int2((int)metaclass_);

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
            fp->write_int2(cnt);
            for (sc = sc_name_head_ ; sc != 0 ; sc = sc->nxt_)
            {
                /* write the counted-length identifier */
                fp->write_int2(sc->get_sym_len());
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
        CTcSymbol *old_sym;

        /* look for a previous instance of the symbol */
        old_sym = G_prs->get_global_symtab()->find(txt, strlen(txt));
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
 *   Add a superclass name entry.  
 */
void CTcSymObjBase::add_sc_name_entry(const char *txt, size_t len)
{
    CTPNSuperclass *entry;

    /* create the entry object */
    entry = new CTPNSuperclass(txt, len);

    /* link it into our list */
    if (sc_name_tail_ != 0)
        sc_name_tail_->nxt_ = entry;
    else
        sc_name_head_ = entry;
    sc_name_tail_ = entry;
}

/*
 *   Check to see if I have a given superclass.  
 */
int CTcSymObjBase::has_superclass(class CTcSymObj *sc_sym) const
{
    CTPNSuperclass *entry;

    /* 
     *   Scan my direct superclasses.  For each one, check to see if my
     *   superclass matches the given superclass, or if my superclass
     *   inherits from the given superclass.  
     */
    for (entry = sc_name_head_ ; entry != 0 ; entry = entry->nxt_)
    {
        CTcSymObj *entry_sym;

        /* look up this symbol */
        entry_sym = (CTcSymObj *)G_prs->get_global_symtab()->find(
            entry->get_sym_txt(), entry->get_sym_len());

        /* 
         *   if the entry's symbol doesn't exist or isn't an object symbol,
         *   skip it 
         */
        if (entry_sym == 0 || entry_sym->get_type() != TC_SYM_OBJ)
            continue;

        /* 
         *   if it matches the given superclass, we've found the given
         *   superclass among our direct superclasses, so we definitely have
         *   the given superclass 
         */
        if (entry_sym == sc_sym)
            return TRUE;

        /* 
         *   ask the symbol if the given class is among its direct or
         *   indirect superclasses - if it's a superclass of my superclass,
         *   it's also my superclass 
         */
        if (entry_sym->has_superclass(sc_sym))
            return TRUE;
    }

    /* 
     *   we didn't find the given class anywhere among our superclasses or
     *   their superclasses, so it must not be a superclass of ours 
     */
    return FALSE;
}

/*
 *   Synthesize a placeholder symbol for a modified object.
 *   
 *   The new symbol is not for use by the source code; we add it merely as
 *   a placeholder.  Build its name starting with a space so that it can
 *   never be reached from source code, and use the object number to
 *   ensure it's unique within the file.  
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
     *   Build the fake name, based on the object ID to ensure uniqueness
     *   and so that we can look it up based on the object ID.  Start it
     *   with a space so that no source token can ever refer to it.  
     */
    sprintf(buf, " %lx", (ulong)obj_id);
}

/*
 *   Add a deleted property entry 
 */
void CTcSymObjBase::add_del_prop_to_list(CTcObjPropDel **list_head,
                                         CTcSymProp *prop_sym)
{
    CTcObjPropDel *entry;

    /* create the new entry */
    entry = new CTcObjPropDel(prop_sym);

    /* link it into my list */
    entry->nxt_ = *list_head;
    *list_head = entry;
}

/*
 *   Add a self-reference fixup 
 */
void CTcSymObjBase::add_self_ref_fixup(CTcDataStream *stream, ulong ofs)
{
    /* add a fixup to our list */
    CTcIdFixup::add_fixup(&fixups_, stream, ofs, obj_id_);
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
         *   if we've never been counted in the object file before, we
         *   must have been written indirectly in the course of writing
         *   another symbol - in this case, return true to indicate that
         *   we are in the file, even though we're not actually writing
         *   anything now 
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
 *   Write the object symbol to an object file.  This main routine does
 *   most of the actual work, once we've decided that we're actually going
 *   to write the symbol.  
 */
int CTcSymObjBase::write_to_obj_file_main(CVmFile *fp)
{
    char buf[32];
    uint cnt;
    CTcObjPropDel *delprop;

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
     *   symbol (don't do this for anonymous objects, since they don't
     *   have a name to write) 
     */
    if (!anon_)
        write_to_file_gen(fp);

    /* 
     *   write my object ID, so that we can translate from the local
     *   numbering system in the object file to the new numbering system
     *   in the image file 
     */
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
     *   add my object file index (we store this to eliminate any
     *   dependency on the load order - this allows us to write other
     *   symbols recursively without worrying about exactly where the
     *   recursion occurs relative to assigning the file index) 
     */
    oswp2(buf + 15, get_obj_file_idx());

    /* write the data to the file */
    fp->write_bytes(buf, 17);

    /* if we're not external, write our stream address */
    if (!is_extern_)
        fp->write_int4(stream_ofs_);

    /* if we're modifying another object, store some extra information */
    if (mod_base_sym_ != 0)
    {
        /* 
         *   Write our list of properties to be deleted from base objects
         *   at link time.  First, count the properties in the list.  
         */
        for (cnt = 0, delprop = first_del_prop_ ; delprop != 0 ;
             ++cnt, delprop = delprop->nxt_) ;

        /* write the count */
        fp->write_int2(cnt);

        /* write the deleted property list */
        for (delprop = first_del_prop_ ; delprop != 0 ;
             delprop = delprop->nxt_)
        {
            /* 
             *   write out this property symbol (we write the symbol
             *   rather than the ID, because when we load the object file,
             *   we'll need to adjust the ID to new global numbering
             *   system in the image file; the easiest way to do this is
             *   to write the symbol and look it up at load time) 
             */
            fp->write_int2(delprop->prop_sym_->get_sym_len());
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
     *   modified base object recursively as we read its modifiers, which
     *   is necessary so that we can build up the same modification chain
     *   on loading the object file.  
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
     *   if this symbol wasn't written to the object file in the first
     *   place, we obviously don't want to include any extra data for it 
     */
    if (!written_to_obj_)
        return FALSE;
    
    /* write my symbol index */
    fp->write_int4(get_obj_file_idx());

    /* write a placeholder superclass count */
    cnt_pos = fp->get_pos();
    fp->write_int2(0);

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
            fp->write_int4(sym->get_obj_file_idx());

            /* count it */
            ++cnt;
        }
    }

    /* go back and write the superclass count */
    end_pos = fp->get_pos();
    fp->set_pos(cnt_pos);
    fp->write_int2(cnt);
    fp->set_pos(end_pos);

    /* count my vocabulary words */
    for (cnt = 0, voc = vocab_ ; voc != 0 ; ++cnt, voc = voc->nxt_) ;

    /* write my vocabulary words */
    fp->write_int2(cnt);
    for (voc = vocab_ ; voc != 0 ; voc = voc->nxt_)
    {
        /* write the text of the word */
        fp->write_int2(voc->len_);
        fp->write_bytes(voc->txt_, voc->len_);

        /* write the property ID */
        fp->write_int2(voc->prop_);
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

/*
 *   Add a word to my vocabulary 
 */
void CTcSymObjBase::add_vocab_word(const char *txt, size_t len,
                                   tctarg_prop_id_t prop)
{
    CTcVocabEntry *entry;
    
    /* create a new vocabulary entry */
    entry = new (G_prsmem) CTcVocabEntry(txt, len, prop);

    /* link it into my list */
    entry->nxt_ = vocab_;
    vocab_ = entry;
}

/*
 *   Delete a vocabulary property from my list (for 'replace') 
 */
void CTcSymObjBase::delete_vocab_prop(tctarg_prop_id_t prop)
{
    CTcVocabEntry *entry;
    CTcVocabEntry *prv;
    CTcVocabEntry *nxt;
    
    /* scan my list and delete each word defined for the given property */
    for (prv = 0, entry = vocab_ ; entry != 0 ; entry = nxt)
    {
        /* remember the next entry */
        nxt = entry->nxt_;
        
        /* if this entry is for the given property, unlink it */
        if (entry->prop_ == prop)
        {
            /* 
             *   it matches - unlink it from the list (note that we don't
             *   have to delete the entry, because it's allocated in
             *   parser memory and thus will be deleted when the parser is
             *   deleted) 
             */
            if (prv != 0)
                prv->nxt_ = nxt;
            else
                vocab_ = nxt;

            /* 
             *   this entry is no longer in any list (we don't really have
             *   to clear the 'next' pointer here, since nothing points to
             *   'entry' any more, but doing so will make it obvious that
             *   the entry was removed from the list, which could be handy
             *   during debugging from time to time) 
             */
            entry->nxt_ = 0;
        }
        else
        {
            /* 
             *   this entry is still in the list, so it's now the previous
             *   entry for our scan 
             */
            prv = entry;
        }
    }
}

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
     *   if I'm a class, there's nothing to do, since vocabulary defined
     *   in a class is only entered in the dictionary for the instances of
     *   the class, not for the class itself 
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
 *   Add my words to the dictionary, associating the words with the given
 *   object.  This can be used to add my own words to the dictionary or to
 *   add my words to a subclass's dictionary.  
 */
void CTcSymObjBase::inherit_vocab()
{
    CTcObjScEntry *sc;

    /* 
     *   if I've already inherited my superclass vocabulary, there's
     *   nothing more we need to do 
     */
    if (vocab_inherited_)
        return;

    /* make a note that I've inherited my superclass vocabulary */
    vocab_inherited_ = TRUE;

    /* inherit words from each superclass */
    for (sc = sc_ ; sc != 0 ; sc = sc->nxt_)
    {
        /* make sure this superclass has built its inherited list */
        sc->sym_->inherit_vocab();
        
        /* add this superclass's words to my list */
        sc->sym_->add_vocab_to_subclass((CTcSymObj *)this);
    }
}

/*
 *   Add my vocabulary words to the given subclass's vocabulary list 
 */
void CTcSymObjBase::add_vocab_to_subclass(CTcSymObj *sub)
{
    CTcVocabEntry *entry;

    /* add each of my words to the subclass */
    for (entry = vocab_ ; entry != 0 ; entry = entry->nxt_)
    {
        /* add this word to my dictionary */
        sub->add_vocab_word(entry->txt_, entry->len_, entry->prop_);
    }
}

/*
 *   Set my base 'modify' object.  This tells us the object that we're
 *   modifying. 
 */
void CTcSymObjBase::set_mod_base_sym(CTcSymObj *sym)
{
    /* remember the object I'm modifying */
    mod_base_sym_ = sym;

    /* 
     *   set the other object's link back to me, so it knows that I'm the
     *   object that's modifying it 
     */
    if (sym != 0)
        sym->set_modifying_sym((CTcSymObj *)this);
}

/*
 *   Get the appropriate stream for a given metaclass 
 */
CTcDataStream *CTcSymObjBase::get_stream_from_meta(tc_metaclass_t meta)
{
    switch(meta)
    {
    case TC_META_TADSOBJ:
        /* it's the regular object stream */
        return G_os;

    case TC_META_ICMOD:
        /* intrinsic class modifier stream */
        return G_icmod_stream;

    default:
        /* other metaclasses have no stream */
        return 0;
    }
}

/*
 *   Add a class-specific template 
 */
void CTcSymObjBase::add_template(CTcObjTemplate *tpl)
{
    /* link it in at the tail of our list */
    if (template_tail_ != 0)
        template_tail_->nxt_ = tpl;
    else
        template_head_ = tpl;
    template_tail_ = tpl;
}

/*
 *   Create a grammar rule list object 
 */
CTcGramProdEntry *CTcSymObjBase::create_grammar_entry(
    const char *prod_sym, size_t prod_sym_len)
{
    CTcSymObj *sym;

    /* look up the grammar production symbol */
    sym = G_prs->find_or_def_gramprod(prod_sym, prod_sym_len, 0);

    /* create a new grammar list associated with the production */
    grammar_entry_ = new (G_prsmem) CTcGramProdEntry(sym);

    /* return the new grammar list */
    return grammar_entry_;
}


/* ------------------------------------------------------------------------ */
/*
 *   metaclass symbol   
 */

/*
 *   add a property 
 */
void CTcSymMetaclassBase::add_prop(const char *txt, size_t len,
                                   const char *obj_fname, int is_static)
{
    CTcSymProp *prop_sym;

    /* see if this property is already defined */
    prop_sym = (CTcSymProp *)G_prs->get_global_symtab()->find(txt, len);
    if (prop_sym != 0)
    {
        /* it's already defined - make sure it's a property */
        if (prop_sym->get_type() != TC_SYM_PROP)
        {
            /* 
             *   it's something other than a property - log the
             *   appropriate type of error, depending on whether we're
             *   loading this from an object file or from source code 
             */
            if (obj_fname == 0)
            {
                /* creating from source - note the code location */
                G_tok->log_error_curtok(TCERR_REDEF_AS_PROP);
            }
            else
            {
                /* loading from an object file */
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_OBJFILE_REDEF_SYM_TYPE,
                                    (int)len, txt, "property", obj_fname);
            }

            /* forget the symbol - it's not a property */
            prop_sym = 0;
        }
    }
    else
    {
        /* add the property definition */
        prop_sym = new CTcSymProp(txt, len, FALSE, G_cg->new_prop_id());
        G_prs->get_global_symtab()->add_entry(prop_sym);
    }

    /* 
     *   if we found a valid property symbol, add it to the metaclass
     *   property list 
     */
    if (prop_sym != 0)
    {
        /* 
         *   mark the symbol as referenced - even if we don't directly
         *   make use of it, the metaclass table references this symbol 
         */
        prop_sym->mark_referenced();
        
        /* add the property to the metaclass list */
        add_prop(prop_sym, is_static);
    }
}

/*
 *   add a property 
 */
void CTcSymMetaclassBase::add_prop(class CTcSymProp *prop, int is_static)
{
    CTcSymMetaProp *entry;

    /* create a new list entry for the property */
    entry = new (G_prsmem) CTcSymMetaProp(prop, is_static);
    
    /* link it at the end of our list */
    if (prop_tail_ != 0)
        prop_tail_->nxt_ = entry;
    else
        prop_head_ = entry;
    prop_tail_ = entry;

    /* count the addition */
    ++prop_cnt_;
}

/* 
 *   write some additional data to the object file 
 */
int CTcSymMetaclassBase::write_to_obj_file(class CVmFile *fp)
{
    CTcSymMetaProp *cur;
    char buf[16];
    
    /* inherit default */
    CTcSymbol::write_to_obj_file(fp);

    /* write my metaclass index, class object ID, and property count */
    fp->write_int2(meta_idx_);
    fp->write_int4(class_obj_);
    fp->write_int2(prop_cnt_);

    /* write my property symbol list */
    for (cur = prop_head_ ; cur != 0 ; cur = cur->nxt_)
    {
        /* write this symbol name */
        fp->write_int2(cur->prop_->get_sym_len());
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
 *   get the nth property in our table
 */
CTcSymMetaProp *CTcSymMetaclassBase::get_nth_prop(int n) const
{
    CTcSymMetaProp *prop;
    
    /* traverse the list to the desired index */
    for (prop = prop_head_ ; prop != 0 && n != 0 ; prop = prop->nxt_, --n) ;

    /* return the property */
    return prop;
}


/* ------------------------------------------------------------------------ */
/*
 *   property symbol entry base 
 */

/*
 *   fold an address constant 
 */
CTcPrsNode *CTcSymPropBase::fold_addr_const()
{
    CTcConstVal cval;

    /* set up the property pointer constant */
    cval.set_prop(get_prop());

    /* return a constant node */
    return new CTPNConst(&cval);
}

/*
 *   Read from a symbol file 
 */
CTcSymbol *CTcSymPropBase::read_from_sym_file(CVmFile *fp)
{
    const char *sym;
    CTcSymbol *old_entry;

    /* read the symbol name */
    if ((sym = base_read_from_sym_file(fp)) == 0)
        return 0;

    /* 
     *   If this property is already defined, this is a harmless
     *   redefinition - every symbol file can define the same property
     *   without any problem.  Indicate the harmless redefinition by
     *   returning the original symbol.  
     */
    old_entry = G_prs->get_global_symtab()->find(sym, strlen(sym));
    if (old_entry != 0 && old_entry->get_type() == TC_SYM_PROP)
        return old_entry;

    /* create and return the new symbol */
    return new CTcSymProp(sym, strlen(sym), FALSE, G_cg->new_prop_id());
}

/*
 *   Write to an object file 
 */
int CTcSymPropBase::write_to_obj_file(CVmFile *fp)
{
    /* 
     *   If the property has never been referenced, don't bother writing
     *   it.  We must have picked up the definition from an external
     *   symbol set we loaded but have no references of our own to the
     *   property.  
     */
    if (!ref_)
        return FALSE;

    /* inherit default */
    CTcSymbol::write_to_obj_file(fp);

    /* 
     *   write my local property ID value - when we load the object file,
     *   we'll need to figure out the translation from our original
     *   numbering system to the new numbering system used in the image
     *   file 
     */
    fp->write_int4((ulong)prop_);

    /* written */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Enumerator symbol base
 */

/*
 *   fold the symbol as a constant 
 */
CTcPrsNode *CTcSymEnumBase::fold_constant()
{
    CTcConstVal cval;

    /* set up the enumerator constant */
    cval.set_enum(get_enum_id());

    /* return a constant node */
    return new CTPNConst(&cval);
}


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
     *   it.  We must have picked up the definition from an external
     *   symbol set we loaded but have no references of our own to the
     *   enumerator.  
     */
    if (!ref_)
        return FALSE;

    /* inherit default */
    CTcSymbol::write_to_obj_file(fp);

    /* 
     *   write my local enumerator ID value - when we load the object file,
     *   we'll need to figure out the translation from our original
     *   numbering system to the new numbering system used in the image
     *   file 
     */
    fp->write_int4((ulong)enum_id_);

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
 *   Parser dictionary hash table entry 
 */

/*
 *   add an item to my list of object associations
 */
void CVmHashEntryPrsDict::add_item(tc_obj_id obj, tc_prop_id prop)
{
    CTcPrsDictItem *item;

    /* search my list for an existing association to the same obj/prop */
    for (item = list_ ; item != 0 ; item = item->nxt_)
    {
        /* if it matches, we don't need to add this one again */
        if (item->obj_ == obj && item->prop_ == prop)
            return;
    }

    /* not found - create a new item */
    item = new (G_prsmem) CTcPrsDictItem(obj, prop);
    
    /* link it into my list */
    item->nxt_ = list_;
    list_ = item;
}

/* ------------------------------------------------------------------------ */
/*
 *   Dictionary entry - each dictionary object gets one of these objects
 *   to track it 
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
 *   Grammar production list entry 
 */
CTcGramProdEntry::CTcGramProdEntry(CTcSymObj *prod_sym)
{
    /* remember my object symbol */
    prod_sym_ = prod_sym;

    /* not in a list yet */
    nxt_ = 0;

    /* no alternatives yet */
    alt_head_ = alt_tail_ = 0;

    /* not explicitly declared yet */
    is_declared_ = FALSE;
}

/*
 *   Add an alternative 
 */
void CTcGramProdEntry::add_alt(CTcGramProdAlt *alt)
{
    /* link it at the end of my list */
    if (alt_tail_ != 0)
        alt_tail_->set_next(alt);
    else
        alt_head_ = alt;
    alt_tail_ = alt;

    /* this is now the last element in our list */
    alt->set_next(0);
}

/*
 *   Move my alternatives to a new owner 
 */
void CTcGramProdEntry::move_alts_to(CTcGramProdEntry *new_entry)
{
    CTcGramProdAlt *alt;
    CTcGramProdAlt *nxt;

    /* move each of my alternatives */
    for (alt = alt_head_ ; alt != 0 ; alt = nxt)
    {
        /* remember the next alternative, since we're unlinking this one */
        nxt = alt->get_next();

        /* unlink this one from the list */
        alt->set_next(0);

        /* link this one into the new owner's list */
        new_entry->add_alt(alt);
    }

    /* there's nothing left in our list */
    alt_head_ = alt_tail_ = 0;
}

/*
 *   Write to an object file 
 */
void CTcGramProdEntry::write_to_obj_file(CVmFile *fp)
{
    ulong cnt;
    CTcGramProdAlt *alt;
    ulong flags;

    /* write the object file index of my production object symbol */
    fp->write_int4(prod_sym_ == 0 ? 0 : prod_sym_->get_obj_file_idx());

    /* set up the flags */
    flags = 0;
    if (is_declared_)
        flags |= 1;

    /* write the flags */
    fp->write_int4(flags);

    /* count my alternatives */
    for (cnt = 0, alt = alt_head_ ; alt != 0 ;
         ++cnt, alt = alt->get_next()) ;

    /* write the count */
    fp->write_int4(cnt);

    /* write each alternative */
    for (alt = alt_head_ ; alt != 0 ; alt = alt->get_next())
        alt->write_to_obj_file(fp);
}

/* ------------------------------------------------------------------------ */
/*
 *   Grammar production alternative 
 */
CTcGramProdAlt::CTcGramProdAlt(CTcSymObj *obj_sym, CTcDictEntry *dict)
{
    /* remember the associated processor object */
    obj_sym_ = obj_sym;

    /* remember the default dictionary currently in effect */
    dict_ = dict;
    
    /* nothing in our token list yet */
    tok_head_ = tok_tail_ = 0;

    /* we don't have a score or badness yet */
    score_ = 0;
    badness_ = 0;

    /* we're not in a list yet */
    nxt_ = 0;
}

void CTcGramProdAlt::add_tok(CTcGramProdTok *tok)
{
    /* link the token at the end of my list */
    if (tok_tail_ != 0)
        tok_tail_->set_next(tok);
    else
        tok_head_ = tok;
    tok_tail_ = tok;

    /* there's nothing after this token */
    tok->set_next(0);
}

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
    fp->write_int4(obj_sym_ == 0 ? 0 : obj_sym_->get_obj_file_idx());

    /* write the dictionary index */
    fp->write_int4(dict_ == 0 ? 0 : dict_->get_obj_idx());

    /* count my tokens */
    for (cnt = 0, tok = tok_head_ ; tok != 0 ;
         ++cnt, tok = tok->get_next()) ;

    /* write my token count */
    fp->write_int4(cnt);

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
    fp->write_int2((int)typ_);

    /* write my data */
    switch(typ_)
    {
    case TCGRAM_PROD:
        /* write my object's object file index */
        fp->write_int4(val_.obj_ != 0
                       ? val_.obj_->get_obj_file_idx() : 0);
        break;

    case TCGRAM_TOKEN_TYPE:
        /* write my enum token ID */
        fp->write_int4(val_.enum_id_);
        break;

    case TCGRAM_PART_OF_SPEECH:
        /* write my property ID */
        fp->write_int2(val_.prop_);
        break;

    case TCGRAM_LITERAL:
        /* write my string */
        fp->write_int2(val_.str_.len_);
        fp->write_bytes(val_.str_.txt_, val_.str_.len_);
        break;

    case TCGRAM_STAR:
        /* no additional value data */
        break;

    case TCGRAM_PART_OF_SPEECH_LIST:
        /* write the length */
        fp->write_int2(val_.prop_list_.len_);

        /* write each element */
        for (i = 0 ; i < val_.prop_list_.len_ ; ++i)
            fp->write_int2(val_.prop_list_.arr_[i]);

        /* done */
        break;

    case TCGRAM_UNKNOWN:
        /* no value - there's nothing extra to write */
        break;
    }

    /* write my property association */
    fp->write_int2(prop_assoc_);
}

/*
 *   Initialize with a part-of-speech list 
 */
void CTcGramProdTok::set_match_part_list()
{
    const size_t init_alo = 10;

    /* remember the type */
    typ_ = TCGRAM_PART_OF_SPEECH_LIST;

    /* we have nothing in the list yet */
    val_.prop_list_.len_ = 0;

    /* set the initial allocation size */
    val_.prop_list_.alo_ = init_alo;

    /* allocate the initial list */
    val_.prop_list_.arr_ = (tctarg_prop_id_t *)G_prsmem->alloc(
        init_alo * sizeof(val_.prop_list_.arr_[0]));
}

/*
 *   Add a property to our part-of-speech match list 
 */
void CTcGramProdTok::add_match_part_ele(tctarg_prop_id_t prop)
{
    /* if necessary, re-allocate the array at a larger size */
    if (val_.prop_list_.len_ == val_.prop_list_.alo_)
    {
        tctarg_prop_id_t *oldp;

        /* bump up the size a bit */
        val_.prop_list_.alo_ += 10;

        /* remember the current list long enough to copy it */
        oldp = val_.prop_list_.arr_;

        /* reallocate it */
        val_.prop_list_.arr_ = (tctarg_prop_id_t *)G_prsmem->alloc(
            val_.prop_list_.alo_ * sizeof(val_.prop_list_.arr_[0]));

        /* copy the old list into the new one */
        memcpy(val_.prop_list_.arr_, oldp,
               val_.prop_list_.len_ * sizeof(val_.prop_list_.arr_[0]));
    }

    /* 
     *   we now know we have space for the new element, so add it, bumping up
     *   the length counter to account for the addition 
     */
    val_.prop_list_.arr_[val_.prop_list_.len_++] = prop;
}

/* ------------------------------------------------------------------------ */
/*
 *   Code Body Parse Node 
 */

/*
 *   instantiate 
 */
CTPNCodeBodyBase::CTPNCodeBodyBase(CTcPrsSymtab *lcltab,
                                   CTcPrsSymtab *gototab, CTPNStm *stm,
                                   int argc, int varargs,
                                   int varargs_list,
                                   CTcSymLocal *varargs_list_local,
                                   int local_cnt, int self_valid,
                                   CTcCodeBodyRef *enclosing_code_body)
{
    /* remember the data in the code body */
    lcltab_ = lcltab;
    gototab_ = gototab;
    stm_ = stm;
    argc_ = argc;
    varargs_ = varargs;
    varargs_list_ = varargs_list;
    varargs_list_local_ = varargs_list_local;
    local_cnt_ = local_cnt;
    self_valid_ = self_valid;
    self_referenced_ = FALSE;
    full_method_ctx_referenced_ = FALSE;

    /* remember the enclosing code body */
    enclosing_code_body_ = enclosing_code_body;

    /* presume we won't need a local context object */
    has_local_ctx_ = FALSE;
    local_ctx_arr_size_ = 0;
    ctx_head_ = ctx_tail_ = 0;
    local_ctx_needs_self_ = FALSE;
    local_ctx_needs_full_method_ctx_ = FALSE;

    /* presume we have an internal fixup list */
    fixup_owner_sym_ = 0;
    fixup_list_anchor_ = &internal_fixups_;

    /* no internal fixups yet */
    internal_fixups_ = 0;

    /* we haven't been replaced yet */
    replaced_ = FALSE;

    /* 
     *   remember the source location of the closing brace, which should
     *   be the current location when we're instantiated 
     */
    end_desc_ = G_tok->get_last_desc();
    end_linenum_ = G_tok->get_last_linenum();
}


/*
 *   fold constants 
 */
CTcPrsNode *CTPNCodeBodyBase::fold_constants(class CTcPrsSymtab *)
{
    /* 
     *   fold constants in our compound statement, in the scope of our
     *   local symbol table 
     */
    if (stm_ != 0)
        stm_->fold_constants(lcltab_);

    /* we are not directly changed by this operation */
    return this;
}

/*
 *   Check for unreferenced labels 
 */
void CTPNCodeBodyBase::check_unreferenced_labels()
{
    /* 
     *   enumerate our labels - skip this check if we're only parsing the
     *   program for syntax 
     */
    if (gototab_ != 0 && !G_prs->get_syntax_only())
        gototab_->enum_entries(&unref_label_cb, this);
}

/*
 *   Callback for enumerating labels for checking for unreferenced labels 
 */
void CTPNCodeBodyBase::unref_label_cb(void *, CTcSymbol *sym)
{
    /* if it's a label, check it out */
    if (sym->get_type() == TC_SYM_LABEL)
    {
        CTcSymLabel *lbl = (CTcSymLabel *)sym;

        /* 
         *   get its underlying statement, and make sure it has a
         *   control-flow flag for goto, continue, or break 
         */
        if (lbl->get_stm() != 0)
        {
            ulong flags;

            /* 
             *   get the explicit control flow flags for this statement --
             *   these flags indicate the use of the label in a goto,
             *   break, or continue statement 
             */
            flags = lbl->get_stm()->get_explicit_control_flow_flags();

            /* 
             *   if the flags aren't set for at least one of the explicit
             *   label uses, the label is unreferenced 
             */
            if ((flags & (TCPRS_FLOW_GOTO | TCPRS_FLOW_BREAK
                          | TCPRS_FLOW_CONT)) == 0)
                lbl->get_stm()->log_warning(TCERR_UNREFERENCED_LABEL,
                                            (int)lbl->get_sym_len(),
                                            lbl->get_sym());
        }
    }
}

/*
 *   add an absolute fixup to my list 
 */
void CTPNCodeBodyBase::add_abs_fixup(CTcDataStream *ds, ulong ofs)
{
    /* ask the code body to add the fixup */
    CTcAbsFixup::add_abs_fixup(fixup_list_anchor_, ds, ofs);
}

/*
 *   add an absolute fixup at the current stream offset 
 */
void CTPNCodeBodyBase::add_abs_fixup(CTcDataStream *ds)
{
    /* ask the code body to add the fixup */
    CTcAbsFixup::add_abs_fixup(fixup_list_anchor_, ds, ds->get_ofs());
}

/*
 *   Get the context variable for a given level 
 */
int CTPNCodeBodyBase::get_or_add_ctx_var_for_level(int level)
{
    CTcCodeBodyCtx *ctx;
    
    /* scan our list to see if the level is already assigned */
    for (ctx = ctx_head_ ; ctx != 0 ; ctx = ctx->nxt_)
    {
        /* if we've already set up this level, return its variable */
        if (ctx->level_ == level)
            return ctx->var_num_;
    }

    /* we didn't find it - allocate a new level structure */
    ctx = new (G_prsmem) CTcCodeBodyCtx();

    /* set up its level and allocate a new variable and property for it */
    ctx->level_ = level;
    ctx->var_num_ = G_prs->alloc_ctx_holder_var();

    /* 
     *   allocating a new variable probably increased the maximum local
     *   variable count - update our information from the parser 
     */
    local_cnt_ = G_prs->get_max_local_cnt();

    /* link it into our list */
    ctx->prv_ = ctx_tail_;
    ctx->nxt_ = 0;
    if (ctx_tail_ != 0)
        ctx_tail_->nxt_ = ctx;
    else
        ctx_head_ = ctx;
    ctx_tail_ = ctx;

    /* return the variable for the new level */
    return ctx->var_num_;
}

/*
 *   Find a local context for a given level 
 */
int CTPNCodeBodyBase::get_ctx_var_for_level(int level, int *varnum)
{
    CTcCodeBodyCtx *ctx;

    /* if they want level zero, it's our local context */
    if (level == 0)
    {
        /* set the variable ID to our local context variable */
        *varnum = local_ctx_var_;

        /* return true only if we actually have a local context */
        return has_local_ctx_;
    }

    /* scan our list to see if the level is already assigned */
    for (ctx = ctx_head_ ; ctx != 0 ; ctx = ctx->nxt_)
    {
        /* if we've already set up this level, return its variable */
        if (ctx->level_ == level)
        {
            /* set the caller's variable number */
            *varnum = ctx->var_num_;

            /* indicate that we found it */
            return TRUE;
        }
    }

    /* didn't find it */
    return FALSE;
}

/*
 *   Get the immediately enclosing code body 
 */
CTPNCodeBody *CTPNCodeBodyBase::get_enclosing() const
{
    /* 
     *   if we have no enclosing code body reference, we have no enclosing
     *   code body 
     */
    if (enclosing_code_body_ == 0)
        return 0;

    /* get the code body from my enclosing code body reference object */
    return enclosing_code_body_->ptr;
}

/*
 *   Get the outermost enclosing code body 
 */
CTPNCodeBody *CTPNCodeBodyBase::get_outermost_enclosing() const
{
    CTPNCodeBody *cur;
    CTPNCodeBody *nxt;

    /* 
     *   scan each enclosing code body until we find one without an enclosing
     *   code body 
     */
    for (cur = 0, nxt = get_enclosing() ; nxt != 0 ;
         cur = nxt, nxt = nxt->get_enclosing()) ;

    /* return what we found */
    return cur;
}

/*
 *   Get the base function symbol for a code body defining a modified
 *   function (i.e., 'modify <funcname>...').  This is the function to which
 *   'replaced' refers within this code body and within nested code bodies.  
 */
class CTcSymFunc *CTPNCodeBodyBase::get_replaced_func() const
{
    CTcSymFunc *b;
    CTPNCodeBody *enc;

    /* if we have an associated function symbol, it's the base function */
    if ((b = get_func_sym()) != 0)
        return b->get_mod_base();

    /* 
     *   if we have an enclosing code body, then 'replaced' here means the
     *   same thing it does there, since we don't explicitly replace anything
     *   here 
     */
    if ((enc = get_enclosing()) != 0)
        return enc->get_replaced_func();

    /* if we haven't found anything yet, we don't have a base function */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Generic statement node 
 */

/* 
 *   initialize at the tokenizer's current source file position 
 */
CTPNStmBase::CTPNStmBase()
{
    /* get the current source location from the parser */
    init(G_prs->get_cur_desc(), G_prs->get_cur_linenum());
}

/* 
 *   log an error at this statement's source file position 
 */
void CTPNStmBase::log_error(int errnum, ...) const
{
    va_list marker;

    /* display the message */
    va_start(marker, errnum);
    G_tcmain->v_log_error(file_, linenum_, TC_SEV_ERROR, errnum, marker);
    va_end(marker);
}

/* 
 *   log a warning at this statement's source file position 
 */
void CTPNStmBase::log_warning(int errnum, ...) const
{
    va_list marker;

    /* display the message */
    va_start(marker, errnum);
    G_tcmain->v_log_error(file_, linenum_, TC_SEV_WARNING, errnum, marker);
    va_end(marker);
}

/*
 *   Generate code for a sub-statement 
 */
void CTPNStmBase::gen_code_substm(CTPNStm *substm)
{
    /* set the error reporting location to refer to the sub-statement */
    G_tok->set_line_info(substm->get_source_desc(),
                         substm->get_source_linenum());

    /* generate code for the sub-statement */
    substm->gen_code(TRUE, TRUE);

    /* restore the error reporting location to the main statement */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());
}

/* ------------------------------------------------------------------------ */
/*
 *   Object Definition Statement 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmObjectBase::fold_constants(CTcPrsSymtab *symtab)
{
    CTPNObjProp *prop;

    /* fold constants in each of our property list entries */
    for (prop = first_prop_ ; prop != 0 ; prop = prop->nxt_)
        prop->fold_constants(symtab);

    /* we're not changed directly by this */
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   superclass record
 */

/* 
 *   get my symbol 
 */
CTcSymbol *CTPNSuperclass::get_sym() const
{
    /* if we know the symbol already, return it directly */
    if (sym_ != 0)
        return sym_;

    /* look up my symbol by name in the global symbol table */
    return G_prs->get_global_symtab()->find(sym_txt_, sym_len_);
}

/*
 *   am I a subclass of the given class?  
 */
int CTPNSuperclass::is_subclass_of(const CTPNSuperclass *other) const
{
    CTcSymObj *sym;
    CTPNSuperclass *sc;

    /* 
     *   if my name matches, we're a subclass (we are a subclass of
     *   ourselves) 
     */
    if (other->sym_len_ == sym_len_
        && memcmp(other->sym_txt_, sym_txt_, sym_len_) == 0)
        return TRUE;

    /* 
     *   We're a subclass if any of our superclasses are subclasses of the
     *   given object.  Get my object symbol, and make sure it's really a
     *   tads-object - if it's not, we're definitely not a subclass of
     *   anything.  
     */
    sym = (CTcSymObj *)get_sym();
    if (sym == 0
        || sym->get_type() != TC_SYM_OBJ
        || sym->get_metaclass() != TC_META_TADSOBJ)
        return FALSE;

    /* scan our symbol's superclass list for a match */
    for (sc = sym->get_sc_name_head() ; sc != 0 ; sc = sc->nxt_)
    {
        /* 
         *   if this one's a subclass of the given class, we're a subclass
         *   as well, since we're a subclass of this superclass 
         */
        if (sc->is_subclass_of(other))
            return TRUE;
    }

    /* 
     *   we didn't find any superclass that's a subclass of the given
     *   class, so we're not a subclass of the given class 
     */
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   'return' statement 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNStmReturnBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* set our location for any errors that occur */
    G_tok->set_line_info(get_source_desc(), get_source_linenum());

    /* fold constants in the expression, if we have one */
    if (expr_ != 0)
        expr_ = expr_->fold_constants(symtab);

    /* we are not directly changed by this operation */
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   Formal type list 
 */

/* 
 *   add a typed parameter to the list - 'tok' is the symbol giving the type
 *   name 
 */
void CTcFormalTypeList::add_typed_param(const CTcToken *tok)
{
    add(new (G_prsmem) CTcFormalTypeEle(tok->get_text(), tok->get_text_len()));
}

/* add an untyped parameter to the list */
void CTcFormalTypeList::add_untyped_param()
{
    add(new (G_prsmem) CTcFormalTypeEle());
}

/* add a list element */
void CTcFormalTypeList::add(CTcFormalTypeEle *ele)
{
    /* link it into our list */
    if (tail_ != 0)
        tail_->nxt_ = ele;
    else
        head_ = ele;
    tail_ = ele;
    ele->nxt_ = 0;
}

/* 
 *   create a decorated name token for the multi-method defined by the given
 *   function name and our type list 
 */
void CTcFormalTypeList::decorate_name(CTcToken *decorated_name,
                                      const CTcToken *func_base_name)
{
    CTcFormalTypeEle *ele;
    size_t len;
    const char *p;
    
    /* figure out how much space we need for the decorated name */
    for (len = func_base_name->get_text_len() + 1, ele = head_ ;
         ele != 0 ; ele = ele->nxt_)
    {
        /* add this type name's length, if there's a name */
        if (ele->name_ != 0)
            len += ele->name_len_;

        /* add a semicolon after the type */
        len += 1;
    }

    /* add "...;" if it's varargs */
    if (varargs_)
        len += 4;

    /* allocate space for the name */
    G_tok->reserve_source(len);

    /* start with the function name */
    p = G_tok->store_source_partial(func_base_name->get_text(),
                                    func_base_name->get_text_len());

    /* add a "*" separator for the multi-method indicator */
    G_tok->store_source_partial("*", 1);

    /* add each type name */
    for (ele = head_ ; ele != 0 ; ele = ele->nxt_)
    {
        /* add the type, if it has one (if not, leave the type empty) */
        if (ele->name_ != 0)
            G_tok->store_source_partial(ele->name_, ele->name_len_);

        /* add a semicolon to terminate the parameter name */
        G_tok->store_source_partial(";", 1);
    }

    /* add the varargs indicator ("...;"), if applicable */
    if (varargs_)
        G_tok->store_source_partial("...;", 4);

    /* null-terminate it */
    G_tok->store_source_partial("\0", 1);

    /* set the decorated token name */
    decorated_name->settyp(TOKT_SYM);
    decorated_name->set_text(p, len);
}

/* formal list element - construction */
CTcFormalTypeEle::CTcFormalTypeEle(const char *name, size_t len)
{
    name_ = new (G_prsmem) char[len + 1];
    memcpy(name_, name, len);
    name_len_ = len;
}
