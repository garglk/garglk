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
#include <stdarg.h>

#include "os.h"
#include "t3std.h"
#include "tcprs.h"
#include "tctarg.h"
#include "tcgen.h"
#include "vmhash.h"
#include "tcmain.h"
#include "vmfile.h"
#include "tctok.h"
#include "vmbignum.h"


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
static const CTcPrsOpAShr S_op_ashr;
static const CTcPrsOpLShr S_op_lshr;
static const CTcPrsOpBin *const S_grp_shift[] =
    { &S_op_shl, &S_op_ashr, &S_op_lshr, 0 };
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

/* if-nil operator ?? */
static const CTcPrsOpIfnil S_op_ifnil;

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
 *   Embedded expression token list.  We capture the tokens of an embedded
 *   expression in a private list for a first scan, to compare against the
 *   list of special embedding templates.  If we determine that the embedding
 *   is a simple expression, or that a portion of it is an expression, we'll
 *   push the captured tokens back into the token stream via 'unget'.  
 */
class CTcEmbedTokenList
{
public:
    CTcEmbedTokenList()
    {
        /* create the initial list entry */
        wrt = head = new CTcTokenEle();
        cnt = 0;
    }

    ~CTcEmbedTokenList()
    {
        /* delete the token list */
        while (head != 0)
        {
            CTcTokenEle *nxt = head->getnxt();
            delete head;
            head = nxt;
        }
    }

    /* reset the list */
    void reset()
    {
        /* set the read and write pointers to the allocated list head */
        wrt = head;
        cnt = 0;
    }

    /* get the number of tokens remaining */
    int getcnt() const { return cnt; }

    /* get the head elemenet */
    const CTcToken *get_head() const { return head; }

    /* remove the head element */
    void unlink_head(CTcToken *tok)
    {
        if (head != 0)
        {
            /* copy the token to the caller's buffer */
            G_tok->copytok(tok, head);

            /* unlink it */
            CTcTokenEle *h = head;
            head = head->getnxt();
            --cnt;

            /* delete it */
            delete h;
        }
        else
        {
            /* no token available - return EOF */
            tok->settyp(TOKT_EOF);
        }
    }

    /* add a token to the list */
    void add_tok(const CTcToken *tok)
    {
        /* fill in the write token */
        G_tok->copytok(wrt, tok);

        /* if this is the last item in the list, add another */
        if (wrt->getnxt() == 0)
        {
            CTcTokenEle *ele = new CTcTokenEle();
            ele->setprv(wrt);
            wrt->setnxt(ele);
        }

        /* advance the write pointer */
        wrt = wrt->getnxt();

        /* count it */
        ++cnt;
    }

    /*
     *   Unget the captured tokens, skipping the first n tokens and the last
     *   m tokens.   
     */
    void unget(int n = 0, int m = 0)
    {
        /* skip the last 'm' tokens */
        CTcTokenEle *ele;
        int i;
        for (ele = wrt->getprv(), i = cnt ; m != 0 && ele != 0 ;
             ele = ele->getprv(), --m, --i) ;

        /* unget tokens until we're down to the first 'n' */
        for ( ; ele != 0 && i > n ; ele = ele->getprv(), --i)
            G_tok->unget(ele);
    }

    /* match a string of space-delimited tokens */
    int match(const char *txt)
    {
        /* count the tokens */
        int argn = 0;
        const char *p;
        for (p = txt ; *p != '\0' ; ++argn)
        {
            /* skip leading spaces */
            for ( ; is_space(*p) ; ++p) ;

            /* scan to the next space */
            for ( ; *p != '\0' && !is_space(*p) ; ++p) ;
        }

        /* if we don't have enough tokens to match the input, we can't match */
        if (cnt < argn)
            return FALSE;

        /* check the arguments against the list */
        CTcTokenEle *ele;
        int tokn;
        for (tokn = cnt, ele = head, p = txt ; tokn != 0 && argn != 0 ;
             ele = ele->getnxt(), --tokn, --argn)
        {
            /* get this argument */
            for ( ; is_space(*p) ; ++p) ;
            const char *arg = p;

            /* find the end of the argument */
            for ( ; *p != '\0' && !is_space(*p) ; ++p) ;
            size_t len = p - arg;
            
            /* check for special arguments */
            if (arg[0] == '*')
            {
                /* 
                 *   '*' - this matches one or more tokens in the token list
                 *   up to the last remaining arguments after the '*'.  Skip
                 *   arguments until the token list and remaining argument
                 *   list are the same length.  
                 */
                for ( ; tokn > argn ; ele = ele->getnxt(), --tokn) ;
            }
            else
            {
                /* it's a literal - check for a match */
                if (!ele->text_matches(arg, len))
                    return FALSE;
            }
        }

        /* if the lists ran out at the same time, it's a match */
        return (tokn == 0 && argn == 0);
    }

    /* match a string template definition */
    int match(CTcStrTemplate *tpl)
    {
        /* if we don't have enough tokens to match, don't bother looking */
        if (cnt < tpl->cnt)
            return FALSE;

        /* check the arguments against the list */
        CTcTokenEle *ele, *tele;
        int tokn, tpln;
        for (tokn = cnt, tpln = tpl->cnt, ele = head, tele = tpl->head ;
             tokn != 0 && tpln != 0 ;
             ele = ele->getnxt(), tele = tele->getnxt(), --tokn, --tpln)
        {
            /* check for special arguments */
            if (tele->text_matches("*", 1))
            {
                /* 
                 *   '*' - this matches one or more tokens in the token list
                 *   up to the last remaining arguments after the '*'.  Skip
                 *   arguments until the token list and remaining argument
                 *   list are the same length.  
                 */
                for ( ; tokn > tpln ; ele = ele->getnxt(), --tokn) ;
            }
            else
            {
                /* it's a literal - check for a match */
                if (!ele->text_matches(tele->get_text(), tele->get_text_len()))
                    return FALSE;
            }
        }
        
        /* if the lists ran out at the same time, it's a match */
        return (tokn == 0 && tpln == 0);
    }

    /*
     *   Reduce our token list to include only the tokens matching the '*' in
     *   a template.  This presumes that the template actually does match.  
     */
    void reduce(CTcStrTemplate *tpl)
    {
        /* find the first token in our list matching '*' in the template */
        CTcTokenEle *src, *tele;
        int rem, trem;
        for (src = head, tele = tpl->head, rem = cnt, trem = tpl->cnt ;
             src != 0 && tele != 0 ;
             src = src->getnxt(), tele = tele->getnxt(), --rem, --trem)
        {
            /* stop when we reach '*' in the template */
            if (tele->text_matches("*", 1))
                break;
        }

        /* skip the '*' in the template */
        trem -= 1;

        /* 
         *   The number of tokens matching '*' is the number left in our
         *   list, minus the number left in the token list after the '*'.
         */
        rem -= trem;
        cnt = rem;

        /* if we had any leading fixed tokens to remove, remove them */
        if (src != head)
        {
            CTcTokenEle *dst;
            for (dst = head ; rem != 0 && src != 0 ;
                 src = src->getnxt(), dst = dst->getnxt(), --rem)
            {
                /* copy this token */
                G_tok->copytok(dst, src);
            }
        }

        /* move the write pointer to the proper position */
        for (wrt = head, rem = cnt ; rem != 0 ; wrt = wrt->getnxt(), --rem) ;
    }

protected:
    /* head of the allocated list */
    CTcTokenEle *head;

    /* current write pointer */
    CTcTokenEle *wrt;

    /* number of tokens currently in the list */
    int cnt;
};


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

    /* no string templates yet */
    str_template_head_ = str_template_tail_ = 0;

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

    /* create the embedded expression list object */
    embed_toks_ = new CTcEmbedTokenList();
}

/*
 *   Add a special built-in property symbol 
 */
CTcSymProp *CTcParser::def_special_prop(int def, const char *name,
                                        tc_prop_id *idp)
{
    /* we haven't created or found the property yet */
    CTcSymProp *propsym = 0;
    
    /* define or look up the property, as required */
    if (def)
    {
        /* allocate the ID */
        tctarg_prop_id_t id = G_cg->new_prop_id();

        /* create the symbol */
        propsym = new CTcSymProp(name, strlen(name), FALSE, id);
        
        /* mark it as referenced, since the compiler itself uses it */
        propsym->mark_referenced();

        /* add it to the global symbol table */
        global_symtab_->add_entry(propsym);
    }
    else
    {
        /* find the entry */
        CTcSymbol *sym = global_symtab_->find(name, strlen(name));

        /* check to see if we found a property symbol */
        if (sym != 0 && sym->get_type() == TC_SYM_PROP)
        {
            /* got it - use the definition we found */
            propsym = (CTcSymProp *)sym;
        }
        else
        {
            /* not found - create a dummy symbol for it */
            propsym = new CTcSymProp(name, strlen(name), FALSE,
                                     TCTARG_INVALID_PROP);
        }
    }

    /* hand the property ID back to the caller if they want it */
    if (idp != 0)
        *idp = (propsym != 0 ? propsym->get_prop() : TCTARG_INVALID_PROP);

    /* return the symbol */
    return propsym;
}

/*
 *   Initialize.  This must be called after the code generator is set up.  
 */
void CTcParser::init()
{
    /* define the special properties */
    cache_special_props(TRUE);
}

/*
 *   Define or look up the special properties.  If 'def' is true, we'll
 *   create definitions; otherwise we'll look up the definitions in the
 *   existing symbol table.  The former case is for normal initialization of
 *   a new compiler; the latter is for use in dynamic compilation, where the
 *   global symbol table is provided by the running program.  
 */
void CTcParser::cache_special_props(int def)
{
    tc_prop_id propid;

    /* add a "construct" property for constructors */
    constructor_sym_ = def_special_prop(def, "construct", &constructor_prop_);

    /* add a "finalize" property for finalizers */
    def_special_prop(def, "finalize", &finalize_prop_);

    /* add some properties for grammar production match objects */
    graminfo_prop_ = def_special_prop(def, "grammarInfo");
    gramtag_prop_ = def_special_prop(def, "grammarTag");

    /* add a "miscVocab" property for miscellaneous vocabulary words */
    def_special_prop(def, "miscVocab", &propid);
    miscvocab_prop_ = (tc_prop_id)propid;

    /* add a "lexicalParent" property for a nested object's parent */
    lexical_parent_sym_ = def_special_prop(def, "lexicalParent");

    /* add a "sourceTextOrder" property */
    src_order_sym_ = def_special_prop(def, "sourceTextOrder");

    /* start the sourceTextOrder index at 1 */
    src_order_idx_ = 1;

    /* add a "sourceTextGroup" property */
    src_group_sym_ = def_special_prop(def, "sourceTextGroup");

    /* we haven't created the sourceTextGroup referral object yet */
    src_group_id_ = TCTARG_INVALID_OBJ;

    /* add a "sourceTextGroupName" property */
    src_group_mod_sym_ = def_special_prop(def, "sourceTextGroupName");

    /* add a "sourceTextGroupOrder" property */
    src_group_seq_sym_ = def_special_prop(def, "sourceTextGroupOrder");

    /* define the operator overload properties */
    ov_op_add_ = def_special_prop(def, "operator +");
    ov_op_sub_ = def_special_prop(def, "operator -");
    ov_op_mul_ = def_special_prop(def, "operator *");
    ov_op_div_ = def_special_prop(def, "operator /");
    ov_op_mod_ = def_special_prop(def, "operator %");
    ov_op_xor_ = def_special_prop(def, "operator ^");
    ov_op_shl_ = def_special_prop(def, "operator <<");
    ov_op_ashr_ = def_special_prop(def, "operator >>");
    ov_op_lshr_ = def_special_prop(def, "operator >>>");
    ov_op_bnot_ = def_special_prop(def, "operator ~");
    ov_op_bor_ = def_special_prop(def, "operator |");
    ov_op_band_ = def_special_prop(def, "operator &");
    ov_op_neg_ = def_special_prop(def, "operator negate");
    ov_op_idx_ = def_special_prop(def, "operator []");
    ov_op_setidx_ = def_special_prop(def, "operator []=");
}

/* get the grammarTag property ID */
tc_prop_id CTcParser::get_grammarTag_prop() const
{
    return gramtag_prop_ != 0
        ? gramtag_prop_->get_prop()
        : TCTARG_INVALID_PROP;
}

/* get the grammarInfo property ID */
tc_prop_id CTcParser::get_grammarInfo_prop() const
{
    return graminfo_prop_ != 0
        ? graminfo_prop_->get_prop()
        : TCTARG_INVALID_PROP;
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

    /* delete the embedding look-ahead list */
    delete embed_toks_;
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
 *   Parse an operator name.  Call this when the current token is 'operator'
 *   and an operator name is expected.  We'll fill in 'tok' with the pseudo
 *   property name of the operator ("operator +", etc).  
 */
int CTcParser::parse_op_name(CTcToken *tok, int *op_argp)
{
    const char *propname;
    int ok = TRUE;
    int op_args = 0;

    /* get the actual operator */
    switch (G_tok->next())
    {
    case TOKT_SYM:
        /* check for named operators */
        if (G_tok->getcur()->text_matches("negate"))
        {
            propname = "operator negate";
            op_args = 1;
        }
        else
        {
            /* unknown symbolic operator name */
            G_tok->log_error_curtok(TCERR_BAD_OP_OVERLOAD);
            ok = FALSE;
            propname = "unknown_operator";
        }
        break;
        
    case TOKT_PLUS:
        propname = "operator +";
        op_args = 2;
        break;
        
    case TOKT_MINUS:
        propname = "operator -";
        op_args = 2;
        break;
        
    case TOKT_TIMES:
        propname = "operator *";
        op_args = 2;
        break;
        
    case TOKT_DIV:
        propname = "operator /";
        op_args = 2;
        break;
        
    case TOKT_MOD:
        propname = "operator %";
        op_args = 2;
        break;
        
    case TOKT_XOR:
        propname = "operator ^";
        op_args = 2;
        break;
        
    case TOKT_SHL:
        propname = "operator <<";
        op_args = 2;
        break;
        
    case TOKT_ASHR:
        propname = "operator >>";
        op_args = 2;
        break;

    case TOKT_LSHR:
        propname = "operator >>>";
        op_args = 2;
        break;
        
    case TOKT_BNOT:
        propname = "operator ~";
        op_args = 1;
        break;
        
    case TOKT_OR:
        propname = "operator |";
        op_args = 2;
        break;

    case TOKT_AND:
        propname = "operator &";
        op_args = 2;
        break;
        
    case TOKT_LBRACK:
        /* we need at least a ']', and a '=' can follow */
        if (G_tok->next() != TOKT_RBRACK)
            G_tok->log_error_curtok(TCERR_EXPECTED_RBRACK_IN_OP);
        
        /* check what follows that */
        if (G_tok->next() == TOKT_EQ)
        {
            /* it's the assign-to-index operator []= */
            propname = "operator []=";
            op_args = 3;
        }
        else
        {
            /* it's just the regular index operator [] */
            propname = "operator []";
            op_args = 2;
            
            /* put back our peek-ahead token */
            G_tok->unget();
        }
        break;
        
    default:
        /* it's not an operator we can override */
        G_tok->log_error_curtok(TCERR_BAD_OP_OVERLOAD);
        propname = "unknown_operator";
        ok = FALSE;
        break;
    }
    
    /* copy the property name to the token */
    tok->set_text(propname, strlen(propname));

    /* set the caller's argument counter if desired */
    if (op_argp != 0)
        *op_argp = op_args;

    /* return the success/error indication */
    return ok;
}

/*
 *   Create a symbol node 
 */
CTcPrsNode *CTcParser::create_sym_node(const textchar_t *sym, size_t sym_len)
{
    /* 
     *   First, look up the symbol in local scope.  Local scope symbols
     *   can always be resolved during parsing, because the language
     *   requires that local scope items be declared before their first
     *   use. 
     */
    CTcPrsSymtab *symtab;
    CTcSymbol *entry = local_symtab_->find(sym, sym_len, &symtab);

    /* if we found it in local scope, return a resolved symbol node */
    if (entry != 0 && symtab != global_symtab_)
        return new CTPNSymResolved(entry);

    /* if there's a debugger local scope, look it up there */
    if (debug_symtab_ != 0)
    {
        /* look it up in the debug symbol table */
        tcprsdbg_sym_info info;
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
 *   Add a generated object 
 */
CTcSymObj *CTcParser::add_gen_obj(const char *clsname)
{
    /* look up the class symbol */
    CTcSymObj *cls = (CTcSymObj *)global_symtab_->find(
        clsname, strlen(clsname));

    /* make sure we found an object symbol */
    if (cls != 0 && cls->get_type() == TC_SYM_OBJ)
    {
        /* got it - go create the object */
        return add_gen_obj(cls);
    }
    else
    {
        /* not an object - return failure */
        return 0;
    }
}

/*
 *   Add a generated object.  
 */
CTcSymObj *CTcParser::add_gen_obj(CTcSymObj *cls)
{
    /* check for dynamic compilation */
    if (G_vmifc != 0)
    {
        /* create a live object in the VM, and wrap it in an anon symbol */
        tctarg_obj_id_t clsid = (cls != 0 ? cls->get_obj_id() :
                                 TCTARG_INVALID_OBJ);
        return new CTcSymObj(".anon", 5, FALSE, G_vmifc->new_obj(clsid),
                             FALSE, TC_META_TADSOBJ, 0);
    }
    else
    {
        /* static mode */
        return add_gen_obj_stat(cls);
    }
}

/*
 *   Add a constant property value to a generated object 
 */
void CTcParser::add_gen_obj_prop(
    CTcSymObj *obj, const char *propn, const CTcConstVal *val)
{
    /* look up the property - this counts as an explicit definition */
    CTcSymbol *sym = G_prs->get_global_symtab()
                     ->find_or_def_prop_explicit(propn, strlen(propn), FALSE);

    /* make sure we found it, and that it's a property symbol */
    if (sym != 0 && sym->get_type() == TC_SYM_PROP)
    {
        /* it's a property symbol - cast it */
        CTcSymProp *prop = (CTcSymProp *)sym;

        /* check for dynamic compilation */
        if (G_vmifc != 0)
        {
            /* dynamic mode - add it to the live object in the VM */
            G_vmifc->set_prop(obj->get_obj_id(), prop->get_prop(), val);
        }
        else
        {            
            /* static compilation - add the value to the object statement */
            add_gen_obj_prop_stat(obj, prop, val);
        }
    }
}

/*
 *   add an integer property value to a generated object 
 */
void CTcParser::add_gen_obj_prop(CTcSymObj *obj, const char *prop, int val)
{
    /* set up a constant structure for the value */
    CTcConstVal c;
    c.set_int(val);

    /* set the property */
    add_gen_obj_prop(obj, prop, &c);
}

/*
 *   add a string property value to a generated object 
 */
void CTcParser::add_gen_obj_prop(CTcSymObj *obj, const char *prop,
                                 const char *val)
{
    /* set up a constant structure for the value */
    CTcConstVal c;
    c.set_sstr(val, strlen(val));

    /* set the property */
    add_gen_obj_prop(obj, prop, &c);
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

void CTcConstVal::set_sstr(const CTcToken *tok)
{
    /* get the string from the token */
    if (tok != 0)
        set_sstr(tok->get_text(), tok->get_text_len());
    else
        set_sstr("", 0);
}

void CTcConstVal::set_sstr(uint32_t ofs)
{
    typ_ = TC_CVT_SSTR;
    val_.strval_.strval_ = 0;
    val_.strval_.strval_len_ = 0;
    val_.strval_.pool_ofs_ = ofs;
}

/*
 *   Set a regex string value 
 */
void CTcConstVal::set_restr(const CTcToken *tok)
{
    typ_ = TC_CVT_RESTR;
    val_.strval_.strval_ = tok->get_text();
    val_.strval_.strval_len_ = tok->get_text_len();
    val_.strval_.pool_ofs_ = 0;
}

/* 
 *   BigNumber string formatter buffer allocator.  Allocates a buffer of the
 *   required size from the parser memory pool. 
 */
class CBigNumStringBufPrsAlo: public IBigNumStringBuf
{
public:
    virtual char *get_buf(size_t need)
        { return new (G_prsmem) char[need]; }
};

/* set a floating point value from a token string */
void CTcConstVal::set_float(const char *str, size_t len, int promoted)
{
    /* set the float */
    typ_ = TC_CVT_FLOAT;
    val_.floatval_.txt_ = str;
    val_.floatval_.len_ = len;
    promoted_ = promoted;
}

/* set a floating point value from a vbignum_t */
void CTcConstVal::set_float(const vbignum_t *val, int promoted)
{
    /* format the value */
    CBigNumStringBufPrsAlo alo;
    const char *buf = val->format(&alo);

    /* 
     *   read the length from the buffer - vbignum_t::format() fills in the
     *   buffer using the TADS String format, with a two-byte little-endian
     *   (VMB_LEN) length prefix 
     */
    size_t len = vmb_get_len(buf);
    buf += VMB_LEN;

    /* store it */
    set_float(buf, len, promoted);
}

/* set a floating point value from a promoted integer value */
void CTcConstVal::set_float(ulong i)
{
    /* set up the bignum value */
    vbignum_t b(i);

    /* store it as a promoted integer value */
    set_float(&b, TRUE);
}

/* 
 *   Try demoting a float back to an int.  This can be used after a
 *   constant-folding operation to turn a previously promoted float back to
 *   an int if the result of the calculation now fits the int type. 
 */
void CTcConstVal::demote_float()
{
    /* if this is a promoted integer, see if we can demote it back to int */
    if (typ_ == TC_CVT_FLOAT && promoted_)
    {
        /* get the BigNumber value */
        vbignum_t b(val_.floatval_.txt_, val_.floatval_.len_, 0);

        /* convert it to an integer */
        int ov;
        int32_t i = b.to_int(ov);

        /* if it didn't overflow, demote the constant back to an int */
        if (!ov)
            set_int(i);
    }
}


/*
 *   set a list value 
 */
void CTcConstVal::set_list(CTPNList *lst)
{
    /* set the type */
    typ_ = TC_CVT_LIST;

    /* remember the list */
    val_.listval_.l_ = lst;

    /* for image file layout purposes, record the length of this list */
    G_cg->note_list(lst->get_count());
}

void CTcConstVal::set_list(uint32_t ofs)
{
    typ_ = TC_CVT_LIST;
    val_.listval_.l_ = 0;
    val_.listval_.pool_ofs_ = ofs;
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

/* is this a numeric value equal to zero? */
int CTcConstVal::equals_zero() const
{
    /* check for integer zero */
    if (typ_ == TC_CVT_INT && get_val_int() == 0)
        return TRUE;

    /* check for a float zero */
    if (typ_ == TC_CVT_FLOAT)
    {
        vbignum_t v(get_val_float(), get_val_float_len(), 0);
        return v.is_zero();
    }

    /* not zero */
    return FALSE;
}

/*
 *   Compare for equality to another constant value 
 */
int CTcConstVal::is_equal_to(const CTcConstVal *val) const
{
    CTPNListEle *ele1;
    CTPNListEle *ele2;

    /* check for float-int comparisons */
    if (typ_ == TC_CVT_INT && val->get_type() == TC_CVT_FLOAT)
    {
        vbignum_t a(get_val_int());
        vbignum_t b(val->get_val_float(), val->get_val_float_len(), 0);
        return a.compare(b) == 0;
    }
    if (typ_ == TC_CVT_FLOAT && val->get_type() == TC_CVT_INT)
    {
        vbignum_t a(get_val_float(), get_val_float_len(), 0);
        vbignum_t b(val->get_val_int());
        return a.compare(b) == 0;
    }
    
    /* 
     *   if the types aren't equal, the values are not equal; otherwise,
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

    case TC_CVT_FLOAT:
        {
            vbignum_t a(get_val_float(), get_val_float_len(), 0);
            vbignum_t b(val->get_val_float(), val->get_val_float_len(), 0);
            return a.compare(b) == 0;
        }

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
    /* parse our left side - if that fails, return failure */
    CTcPrsNode *lhs = left_->parse();
    if (lhs == 0)
        return 0;

    /* keep going as long as we find our operator */
    for (;;)
    {
        /* check my operator */
        if (G_tok->cur() == get_op_tok())
        {
            /* skip the matching token */
            G_tok->next();
            
            /* parse the right-hand side */
            CTcPrsNode *rhs = right_->parse();
            if (rhs == 0)
                return 0;

            /* try folding our subnodes into a constant value, if possible */
            CTcPrsNode *const_tree = eval_constant(lhs, rhs);

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
    /* parse our left side - if that fails, return failure */
    CTcPrsNode *lhs = left_->parse();
    if (lhs == 0)
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
    /* check each operator at this precedence level */
    for (const CTcPrsOpBin *const *op = ops_ ; *op != 0 ; ++op)
    {
        /* check this operator's token */
        if (G_tok->cur() == (*op)->get_op_tok())
        {
            /* skip the operator token */
            G_tok->next();

            /* parse the right-hand side */
            CTcPrsNode *rhs = right_->parse();
            if (rhs == 0)
            {
                /* error - cancel the entire expression */
                *lhs = 0;
                return FALSE;
            }

            /* try folding our subnodes into a constant value */
            CTcPrsNode *const_tree = (*op)->eval_constant(*lhs, rhs);

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
    /* parse our left side - if that fails, return failure */
    CTcPrsNode *lhs = left_->parse();
    if (lhs == 0)
        return 0;

    /* keep going as long as we find one of our operators */
    for (;;)
    {
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
                CTPNArglist *rhs = parse_inlist();
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
                CTPNArglist *rhs = parse_inlist();
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
         *   The left value is a constant, so we can eliminate the &&.  If
         *   the left value is nil, the result is nil; otherwise, it's the
         *   right half.  
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
        /* get the types */
        CTcConstVal *c1 = left->get_const_val();
        CTcConstVal *c2 = right->get_const_val();
        tc_constval_type_t typ1 = c1->get_type();
        tc_constval_type_t typ2 = c2->get_type();

        /* determine what we're comparing */
        int sense;
        if (typ1 == TC_CVT_INT && typ2 == TC_CVT_INT)
        {
            /* get the values */
            long val1 = c1->get_val_int();
            long val2 = c2->get_val_int();

            /* calculate the sense of the integer comparison */
            sense = (val1 < val2 ? -1 : val1 == val2 ? 0 : 1);
        }
        else if (typ1 == TC_CVT_SSTR && typ2 == TC_CVT_SSTR)
        {
            /* compare the string values */
            sense = strcmp(c1->get_val_str(), c2->get_val_str());
        }
        else if (typ1 == TC_CVT_FLOAT && typ2 == TC_CVT_FLOAT)
        {
            /* both are floats - get as BigNumber values */
            vbignum_t a(c1->get_val_float(), c1->get_val_float_len(), 0);
            vbignum_t b(c2->get_val_float(), c2->get_val_float_len(), 0);
            sense = a.compare(b);
        }
        else if (typ1 == TC_CVT_FLOAT && typ2 == TC_CVT_INT)
        {
            /* float vs int */
            vbignum_t a(c1->get_val_float(), c1->get_val_float_len(), 0);
            vbignum_t b(c2->get_val_int());
            sense = a.compare(b);
        }
        else if (typ1 == TC_CVT_INT && typ2 == TC_CVT_FLOAT)
        {
            vbignum_t a(c1->get_val_int());
            vbignum_t b(c2->get_val_float(), c2->get_val_float_len(), 0);
            sense = a.compare(b);
        }
        else
        {
            /* these types are incomparable */
            G_tok->log_error(TCERR_CONST_BAD_COMPARE,
                             G_tok->get_op_text(get_op_tok()));
            return 0;
        }

        /* set the result in the left value */
        c1->set_bool(get_bool_val(sense));

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
        CTcConstVal *c1 = left->get_const_val();
        CTcConstVal *c2 = right->get_const_val();
        tc_constval_type_t typ1 = c1->get_type();
        tc_constval_type_t typ2 = c2->get_type();

        /* check for ints, floats, or a combination */
        if (typ1 == TC_CVT_INT && typ2 == TC_CVT_INT)
        {
            /* calculate the int result */
            int ov;
            long r = calc_result(c1->get_val_int(), c2->get_val_int(), ov);

            /* on overflow, recalculate using BigNumbers */
            if (ov)
            {
                /* calculate the BigNumber result */
                vbignum_t a(c1->get_val_int());
                vbignum_t b(c2->get_val_int());
                vbignum_t *c = calc_result(a, b);

                /* if successful, assign it back to the left operand */
                if (c != 0)
                {
                    c1->set_float(c, TRUE);
                    delete c;
                }
            }
            else
            {
                /* success - assign the folded int to the left operand */
                c1->set_int(r);
            }
        }
        else if (typ1 == TC_CVT_FLOAT && typ2 == TC_CVT_FLOAT)
        {
            /* calculate the BigNumber result */
            vbignum_t a(c1->get_val_float(), c1->get_val_float_len(), 0);
            vbignum_t b(c2->get_val_float(), c2->get_val_float_len(), 0);
            vbignum_t *c = calc_result(a, b);

            /* assign it back to the left operand */
            if (c != 0)
            {
                /* save the value and delete the calculation result */
                c1->set_float(c, c1->is_promoted() && c2->is_promoted());
                delete c;

                /* check for possible demotion back to int */
                c1->demote_float();
            }
        }
        else if (typ1 == TC_CVT_FLOAT && typ2 == TC_CVT_INT)
        {
            /* calculate the BigNumber result */
            vbignum_t a(c1->get_val_float(), c1->get_val_float_len(), 0);
            vbignum_t b(c2->get_val_int());
            vbignum_t *c = calc_result(a, b);

            /* assign it back to the left operand */
            if (c != 0)
            {
                /* save the value and delete the calculation result */
                c1->set_float(c, c1->is_promoted());
                delete c;

                /* check for possible demotion back to int */
                c1->demote_float();
            }
        }
        else if (typ1 == TC_CVT_INT && typ2 == TC_CVT_FLOAT)
        {
            /* calculate the BigNumber result */
            vbignum_t a(c1->get_val_int());
            vbignum_t b(c2->get_val_float(), c2->get_val_float_len(), 0);
            vbignum_t *c = calc_result(a, b);

            /* assign it back to the left operand */
            if (c != 0)
            {
                /* save the value and delete the calculation result */
                c1->set_float(c, c2->is_promoted());
                delete c;

                /* check for possible demotion back to int */
                c1->demote_float();
            }
        }
        else
        {
            /* incompatible types - log an error */
            G_tok->log_error(TCERR_CONST_BINARY_REQ_NUM,
                             G_tok->get_op_text(get_op_tok()));
            return 0;
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
 *   arithmetic shift right operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpAShr::build_tree(CTcPrsNode *left,
                                     CTcPrsNode *right) const
{
    return new CTPNAShr(left, right);
}

/* ------------------------------------------------------------------------ */
/*
 *   logical shift right operator 
 */

/*
 *   build the subtree 
 */
CTcPrsNode *CTcPrsOpLShr::build_tree(CTcPrsNode *left,
                                     CTcPrsNode *right) const
{
    return new CTPNLShr(left, right);
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

/*
 *   calculate a constant float result 
 */
vbignum_t *CTcPrsOpMul::calc_result(
    const vbignum_t &a, const vbignum_t &b) const
{
    return a * b;
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
 *   evaluate constant integer result 
 */
long CTcPrsOpDiv::calc_result(long a, long b, int &ov) const
{
    /* check for divide-by-zero */
    if (b == 0)
    {
        /* log a divide-by-zero error */
        G_tok->log_error(TCERR_CONST_DIV_ZERO);

        /* the result isn't really meaningful, but return something anyway */
        return 1;
    }

    /* 
     *   check for overflow - there's only one way that integer division can
     *   overflow, which is to divide INT32MINVAL by -1 
     */
    ov = (a == INT32MINVAL && b == 1);

    /* return the result */
    return a / b;
}

/*
 *   calculate a constant float result 
 */
vbignum_t *CTcPrsOpDiv::calc_result(
    const vbignum_t &a, const vbignum_t &b) const
{
    /* check for division by zero */
    if (b.is_zero())
    {
        /* log an error, and just return the left operand */
        G_tok->log_error(TCERR_CONST_DIV_ZERO);
        return new vbignum_t(1L);
    }
    else
    {
        /* calculate the result */
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
long CTcPrsOpMod::calc_result(long a, long b, int &ov) const
{
    /* there's no way for integer modulo to overflow */
    ov = FALSE;
    
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

/*
 *   calculate a constant float result 
 */
vbignum_t *CTcPrsOpMod::calc_result(
    const vbignum_t &a, const vbignum_t &b) const
{
    /* check for division by zero */
    if (b.is_zero())
    {
        /* log an error, and just return the left operand */
        G_tok->log_error(TCERR_CONST_DIV_ZERO);
        return new vbignum_t(1L);
    }
    else
    {
        /* calculate the result */
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
        if ((typ1 == TC_CVT_INT || typ1 == TC_CVT_FLOAT)
            && (typ2 == TC_CVT_INT || typ2 == TC_CVT_FLOAT))
        {
            /* int/float minus int/float - use the inherited handling */
            return CTcPrsOpArith::eval_constant(left, right);
        }
        else if (typ1 == TC_CVT_LIST)
        {
            /* get the original list */
            CTPNList *lst = left->get_const_val()->get_val_list();

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
        else if (typ1 == TC_CVT_OBJ)
        {
            /* 
             *   assume it's an overloaded operator, in which case constant
             *   evaluation isn't possible, but it could still be a legal
             *   expression 
             */
            return 0;
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

/*
 *   calculate a constant float result 
 */
vbignum_t *CTcPrsOpSub::calc_result(
    const vbignum_t &a, const vbignum_t &b) const
{
    return a - b;
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
        if ((typ1 == TC_CVT_INT || typ1 == TC_CVT_FLOAT)
            && (typ2 == TC_CVT_INT || typ2 == TC_CVT_FLOAT))
        {
            /* int/float plus int/float - use the inherited handling */
            return CTcPrsOpArith::eval_constant(left, right);
        }
        else if (typ1 == TC_CVT_LIST)
        {
            /* get the original list */
            CTPNList *lst = left->get_const_val()->get_val_list();

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

            /* treat nil as an empty string */
            if (typ2 == TC_CVT_NIL)
                str1 = "", len2 = 0;

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
        else if (typ1 == TC_CVT_OBJ)
        {
            /* 
             *   assume it's an overloaded operator, in which case constant
             *   evaluation isn't possible, but it could still be a legal
             *   expression 
             */
            return 0;
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
 *   calculate a constant float result 
 */
vbignum_t *CTcPrsOpAdd::calc_result(
    const vbignum_t &a, const vbignum_t &b) const
{
    return a + b;
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
    /* start by parsing a conditional subexpression */
    CTcPrsNode *lhs = S_op_if.parse();
    if (lhs == 0)
        return 0;

    /* get the next operator */
    tc_toktyp_t curtyp = G_tok->cur();

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
    case TOKT_ASHREQ:
    case TOKT_LSHREQ:
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
    CTcPrsNode *rhs = parse();
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

    case TOKT_ASHREQ:
        lhs = new CTPNAShrAsi(lhs, rhs);
        break;

    case TOKT_LSHREQ:
        lhs = new CTPNLShrAsi(lhs, rhs);
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
 *   Ifnil operator ??
 */

CTcPrsNode *CTcPrsOpIfnil::parse() const
{
    /* parse the conditional part */
    CTcPrsNode *first = S_op_or.parse();
    if (first == 0)
        return 0;

    /* if we're not looking at the '??' operator, we're done */
    if (G_tok->cur() != TOKT_QQ)
        return first;

    /* skip the '??' operator */
    G_tok->next();

    /* 
     *   parse the second part, which can be any expression except ',', since
     *   we have higher precedence than ',' 
     */
    CTcPrsNode *second = G_prs->parse_expr_or_dstr(FALSE);
    if (second == 0)
        return 0;

    /* if the first expression is constant, we can fold immediately */
    if (first->is_const())
    {
        /* if it's nil, yield the second value, otherwise yield the first */
        return (first->get_const_val()->get_type() == TC_CVT_NIL
                ? second : first);
    }
    else
    {
        /* it's not a constant value - return a new if-nil node */
        return new CTPNIfnil(first, second);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Tertiary Conditional Operator 
 */

CTcPrsNode *CTcPrsOpIf::parse() const
{
    /* parse the conditional part */
    CTcPrsNode *first = S_op_ifnil.parse();
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
    CTcPrsNode *second = G_prs->parse_expr_or_dstr(TRUE);
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
    CTcPrsNode *third = G_prs->parse_expr_or_dstr(FALSE);
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
 *   Bmbedded string expressions.
 */

/*
 *   Embedded string tree generator.  This abstracts the tree construction
 *   for an embedded string, so that the same parser can be used for single
 *   and double quoted strings.  
 */
struct CTcEmbedBuilder
{
    /* is this a double-quoted string builder? */
    virtual int is_double() const = 0;

    /* create the initial node for the leading fixed part of the string */
    virtual CTcPrsNode *lead_node() = 0;

    /* 
     *   Finish the tree.  If we skipped creating a tree because we had an
     *   empty string in the lead position, and we haven't added anything to
     *   it, this should create a non-null node to represent the final tree,
     *   so we know it's not an error.  
     */
    virtual CTcPrsNode *finish_tree(CTcPrsNode *cur) = 0;

    /* parse an embedded expression */
    virtual CTcPrsNode *parse_expr() = 0;

    /* 
     *   Add an embedding: create a node combining the tree before an
     *   expression, an embedded expression, and the current token
     *   representing the continuation of the string after the expression.  
     */
    virtual CTcPrsNode *add_embedding(CTcPrsNode *cur, CTcPrsNode *sub) = 0;

    /* add a string segment (a "mid" or "end" segment) */
    virtual CTcPrsNode *add_segment(CTcPrsNode *cur) = 0;

    /* string mid/end token types */
    virtual tc_toktyp_t mid_tok() = 0;
    virtual tc_toktyp_t end_tok() = 0;
};

/* 
 *   Embedded string tree generator for double-quoted strings.  Double-quoted
 *   strings with embeddings are converted to a series of comma expressions:
 *   
 *.      "one <<two>> three <<four>> five"
 *.   -> "one ", say(two), " three ", say(four), " five";
 *   
 *   Note that the embedded expressions are wrapped in "say()" calls, since
 *   we have to make sure they generate output.  Some embeddings will
 *   generate output as a side effect rather than as a value result, but
 *   that's fine; in that case they'll perform their side effect (i.e.,
 *   printing text) and then return nil, so the say() will add nothing to the
 *   output stream.  
 */
struct CTcEmbedBuilderDbl: CTcEmbedBuilder
{
    /* we're the builder for double-quoted strings */
    virtual int is_double() const { return TRUE; }

    /* string mid/end token types */
    virtual tc_toktyp_t mid_tok() { return TOKT_DSTR_MID; }
    virtual tc_toktyp_t end_tok() { return TOKT_DSTR_END; }

    /* build the leading node */
    virtual CTcPrsNode *lead_node()
    {
        /* 
         *   Create a node for the initial part of the string.  This is just
         *   an ordinary double-quoted string node. If the initial part of
         *   the string is zero-length, don't create an initial node at all,
         *   since this would just generate do-nothing code.  
         */
        if (G_tok->getcur()->get_text_len() != 0)
        {
            /* create the node for the initial part of the string */
            return new CTPNDstr(G_tok->getcur()->get_text(),
                                G_tok->getcur()->get_text_len());
        }
        else
        {
            /* 
             *   the initial part of the string is empty, so we don't need a
             *   node for this portion 
             */
            return 0;
        }
    }

    /* finish the tree */
    virtual CTcPrsNode *finish_tree(CTcPrsNode *cur)
    {
        return (cur != 0 ? cur : new CTPNDstr("", 0));
    }

    /* parse an embedded expression */
    virtual CTcPrsNode *parse_expr()
    {
        /* 
         *   an expression embedded in a dstring can be any value expression,
         *   or another dstring expression 
         */
        return G_prs->parse_expr_or_dstr(TRUE);
    }

    /* add another embedding */
    virtual CTcPrsNode *add_embedding(CTcPrsNode *cur, CTcPrsNode *sub)
    {
        /* wrap the expresion in a "say()" embedding node */
        sub = new CTPNDstrEmbed(sub);
        
        /*
         *   Build a node representing everything so far: do this by
         *   combining the sub-expression with everything preceding, using a
         *   comma operator.  This isn't necessary if there's nothing
         *   preceding the sub-expression, since this means the
         *   sub-expression itself is everything so far.  
         */
        if (cur != 0)
            return new CTPNComma(cur, sub);
        else
            return sub;
    }

    /* add a string segment */
    virtual CTcPrsNode *add_segment(CTcPrsNode *cur)
    {
        /*
         *   Combine the part so far with the next string segment, using a
         *   comma operator.  If the next string segment is empty, there's no
         *   need to add anything for it.  
         */
        size_t len = G_tok->getcur()->get_text_len();
        if (len != 0)
        {
            /* create a node for the new string segment */
            CTcPrsNode *newstr = new CTPNDstr(
                G_tok->getcur()->get_text(), len);
            
            /* combine it into the part so far with a comma operator */
            cur = new CTPNComma(cur, newstr);
        }

        /* return the new combined node */
        return cur;
    }
};

/* 
 *   Embedded string tree generator for single-quoted strings.  
 *   
 *   Single-quoted strings with embeddings are translated as follows:
 *   
 *.       local a = 'one <<two>> three <<four>> five';
 *.   ->  local a = ('one ' + (two) + ' three ' + (four) + ' five');
 */
struct CTcEmbedBuilderSgl: CTcEmbedBuilder
{
    /* we're the builder for single-quoted strings */
    virtual int is_double() const { return FALSE; }

    /* string mid/end token types */
    virtual tc_toktyp_t mid_tok() { return TOKT_SSTR_MID; }
    virtual tc_toktyp_t end_tok() { return TOKT_SSTR_END; }

    /* build the leading node */
    virtual CTcPrsNode *lead_node()
    {
        /* 
         *   Create a string constant node for the first part of the string.
         *   Note that we need this even if the string is empty, to guarantee
         *   that the overall expression is treated as a string.  
         */
        return string_node(G_tok->getcur());
    }

    virtual CTcPrsNode *finish_tree(CTcPrsNode *cur)
    {
        return (cur != 0 ? cur : string_node(0));
    }

    /* create a constant string node from the current token */
    CTcPrsNode *string_node(const CTcToken *tok)
    {
        CTcConstVal cval;
        cval.set_sstr(tok);
        return new CTPNConst(&cval);
    }

    /* parse an embedded expression */
    virtual CTcPrsNode *parse_expr()
    {
        /* in a single-quoted string, we need a value expression */
        return G_prs->parse_expr();
    }

    /* add another embedding */
    virtual CTcPrsNode *add_embedding(CTcPrsNode *cur, CTcPrsNode *sub)
    {
        /*
         *   Combine the sub-expression with the preceding portion using an
         *   addition operator.  This will be interpreted as concatenation at
         *   run-time since the left side is definitively a string.  
         */
        return new CTPNAdd(cur, sub);
    }

    /* add a string segment */
    virtual CTcPrsNode *add_segment(CTcPrsNode *cur)
    {        
        /* 
         *   Add the next string segment.  We can omit it if it's zero
         *   length, as this will add nothing to the result.  
         */
        if (G_tok->getcur()->get_text_len() != 0)
        {
            CTcConstVal cval;
            cval.set_sstr(G_tok->getcur());
            cur = new CTPNAdd(cur, new CTPNConst(&cval));
        }
        
        /* return the new combined node */
        return cur;
    }
};

/*
 *   Embedding stack entry.  Each nesting level keeps one of these structures
 *   to track the enclosing constructs.  
 */
struct CTcEmbedLevel
{
    /* set up the level, linking to our parent */
    CTcEmbedLevel(int typ, CTcEmbedLevel *parent)
        : typ(typ), parent(parent) { }

    /* level type */
    int typ;

    /* level types */
    static const int Outer = 0;
    static const int If = 1;
    static const int OneOf = 2;
    static const int FirstTime = 3;

    /* parent level */
    CTcEmbedLevel *parent;

    /* find a type among my parents */
    int is_in(int typ)
    {
        /* if this is my type or a parent's type, we're in this type */
        return (typ == this->typ
                || (parent != 0 && parent->is_in(typ)));
    }
};

/*
 *   Parse an embedded expression in a string.  This handles embedding for
 *   both single-quoted and double-quoted strings, using the tree builder
 *   object to handle the differences.  The two have the same parsing syntax,
 *   but they do generate different parse trees.  
 */
CTcPrsNode *CTcPrsOpUnary::parse_embedding(CTcEmbedBuilder *b)
{
    /* keep going until we reach the end of the string */
    for (;;)
    {
        /* parse a series of embeddings */
        int eos;
        CTcEmbedLevel level(CTcEmbedLevel::Outer, 0);
        CTcPrsNode *n = parse_embedding_list(b, eos, &level);

        /* if we encountered an error or the end of the string, we're done */
        if (n == 0 || eos)
            return n;

        /* 
         *   We stopped parsing without reaching the end of the string, so we
         *   must have encountered a token that terminates a nested structure
         *   - <<end>>, <<else>>, <<case>>, etc.  Capture the current
         *   embedding so we can check which type of end token it is.  
         */
        const char *open_kw;
        parse_embedded_end_tok(b, &level, &open_kw);
        G_tok->log_error(TCERR_BAD_EMBED_END_TOK,
                         (int)G_tok->getcur()->get_text_len(),
                         G_tok->getcur()->get_text(), open_kw);
    }
}

/*
 *   Parse a series of embeddings 
 */
CTcPrsNode *CTcPrsOpUnary::parse_embedding_list(
    CTcEmbedBuilder *b, int &eos, CTcEmbedLevel *parent)
{
    /* we haven't reached the end of the string yet */
    eos = FALSE;

    /* create the node for the leading string */
    tc_toktyp_t curtyp = G_tok->cur();
    CTcPrsNode *cur = b->lead_node();
    G_tok->next();

    /* 
     *   If we're starting with the final segment of the string, there's no
     *   need to parse any further.  We can enter with the final segment when
     *   parsing sub-constructs such as <<if>>.  
     */
    if (curtyp == b->end_tok())
    {
        /* tell the caller we're at the end of the string */
        eos = TRUE;

        /* return the lead node */
        return b->finish_tree(cur);
    }

    /* keep going until we find the end of the string */
    for (;;)
    {
        /* note the current error count */
        int nerr = G_tcmain->get_error_count();

        /* 
         *   Capture tokens up to the next string segment, so that we can
         *   compare the tokens to the embedding templates.
         */
        CTcEmbedTokenList *tl = G_prs->embed_toks_;
        capture_embedded(b, tl);

        /*
         *   First, check the special built-in syntax:
         *   
         *.    <<if expr>>
         *.    <<unless expr>>
         *.    <<one of>>
         */
        CTcPrsNode *sub = 0;
        const char *open_kw = 0;
        int check_end = TRUE;
        if (tl->match("if *"))
        {
            /* parse the "if" list */
            tl->unget(1, 0);
            sub = parse_embedded_if(b, FALSE, eos, parent);
        }
        else if (tl->match("unless *"))
        {
            /* parse the "unless" (which is just an inverted "if") */
            tl->unget(1);
            sub = parse_embedded_if(b, TRUE, eos, parent);
        }
        else if (tl->match("one of"))
        {
            /* parse the "one of" list */
            sub = parse_embedded_oneof(b, eos, parent);
        }
        else if (tl->match("first time"))
        {
            /* parse the "first time" */
            sub = parse_embedded_firsttime(b, eos, parent);
        }
        else if (parse_embedded_end_tok(tl, parent, &open_kw))
        {
            /* 
             *   We're at a terminator for a multi-part embedding construct.
             *   Stop here and return what we have, with the token left at
             *   the terminator.  The caller is presumably parsing this
             *   construct recursively, so it'll take it from here.  
             */
            tl->unget();
            return b->finish_tree(cur);
        }
        else if (tl->getcnt() == 0)
        {
            /* 
             *   empty embedding, or we got here via error recovery; proceed
             *   with the next segment without any embedding
             */
            sub = 0;
        }
        else
        {
            /* 
             *   No special syntax, so treat it as an expression.  First,
             *   check for a sprintf format spec - e.g., <<%2d x>>
             */
            CTcToken fmttok(TOKT_INVALID);
            if (tl->get_head()->gettyp() == TOKT_FMTSPEC)
                tl->unlink_head(&fmttok);

            /* parse the embedded expression */
            sub = parse_embedded_expr(b, tl);
            if (sub == 0)
                return 0;
            
            /* we can't have an end token immediately after an expression */
            check_end = FALSE;

            /* 
             *   if we have a format spec, wrap the expression in a
             *   sprintf node as sprintf(fmtspec, sub) 
             */
            if (fmttok.gettyp() == TOKT_FMTSPEC)
            {
                /* 
                 *   set up the argument list - fmtspec string, sub (note
                 *   that arg lists are built in reverse order) 
                 */
                CTcConstVal fmtcv;
                fmtcv.set_sstr(&fmttok);
                CTPNArg *args = new CTPNArg(sub);
                args->set_next_arg(new CTPNArg(new CTPNConst(&fmtcv)));

                /* set up the sprintf call */
                sub = new CTPNCall(new CTPNSym("sprintf", 7),
                                   new CTPNArglist(2, args));
            }
        }

        /* if we have an embedding, add it */
        if (sub != 0)
            cur = b->add_embedding(cur, sub);

        /* 
         *   after the expression, we must find either another string segment
         *   with another embedded expression following, or the final string
         *   segment; anything else is an error 
         */
        if (eos)
        {
            /* 
             *   We reached the end of the string in a sub-construct; there's
             *   nothing more to add to the main string.  Just return what we
             *   have to the caller.  
             */
            return b->finish_tree(cur);
        }
        else if ((curtyp = G_tok->cur()) == b->mid_tok())
        {
            /* 
             *   It's a string with yet another embedded expression.  Add
             *   this segment to the expression and proceed to the next
             *   embedding.  
             */
            cur = b->add_segment(cur);
            G_tok->next();
        }
        else if (curtyp == b->end_tok())
        {
            /* 
             *   It's the last segment of the string.  Add the final segment
             *   to the string.
             */
            cur = b->add_segment(cur);
            G_tok->next();

            /* tell the caller we've reached the end, and return our tree */
            eos = TRUE;
            return b->finish_tree(cur);
        }
        else if (check_end && parse_embedded_end_tok(b, parent, &open_kw))
        {
            /* 
             *   We parsed a recursive list for <<if>>, <<one of>>, etc.  End
             *   tokens are implied when missing, so an end token at one
             *   level can implicitly end multiple levels.  We stopped at
             *   such a token without consuming it, so it's for our caller to
             *   digest.  Simply return what we have and let the caller take
             *   it from here.  
             */
            return b->finish_tree(cur);
        }
        else if (G_tcmain->get_error_count() != nerr)
        {
            /*
             *   We logged an error within a sub-expression, so it's not
             *   surprising that we're not at a valid continuation token at
             *   this point.  Try syncing up with the source: look for a
             *   continuation token or something that probably ends the
             *   statement, such as a semicolon or right brace.  
             */
            for (;;)
            {
                /* if we're at a statement ender, stop here */
                if (curtyp == TOKT_SEM || curtyp == TOKT_RBRACE)
                    return b->finish_tree(cur);

                /* if at eof, return failure */
                if (curtyp == TOKT_EOF)
                    return 0;

                /* if we're at a continuation token, resume parsing */
                if (curtyp == b->mid_tok() || curtyp == b->end_tok())
                    break;

                /* skip this token and keep looking for a sync point */
                curtyp = G_tok->next();
            }
        }
        else
        {
            /* anything else is invalid - log an error */
            G_tok->log_error_curtok(TCERR_EXPECTED_STR_CONT);

            /*
             *   Skip ahead until we find the next string segment or
             *   something that looks like the end of the statement. 
             */
            tl->reset();
            capture_embedded(b, tl);

            /* 
             *   If we stopped at the next segment, carry on.  Otherwise,
             *   assume they simply left off the end of the string and return
             *   what we have. 
             */
            if (G_tok->cur() != b->mid_tok() && G_tok->cur() != b->end_tok())
                return (cur != 0 ? cur : b->finish_tree(sub));
        }
    }
}

/*
 *   Parse a single embedded expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_embedded_expr(
    CTcEmbedBuilder *b, CTcEmbedTokenList *tl)
{
    /* search for a template for the expression */
    for (CTcStrTemplate *st = G_prs->get_str_template_head() ; st != 0 ;
         st = st->nxt)
    {
        /* check for a match */
        if (tl->match(st))
        {
            /*
             *   This template matches.  Generate the code as a call to the
             *   template's processor function.
             */
            CTPNSymResolved *func = new CTPNSymResolved(st->func);

            /* 
             *   If the template has a '*', the tokens matching the star are
             *   a sub-expression that we evaluate as the argument to the
             *   function.  Otherwise we call it with no arguments. 
             */
            CTPNArglist *arglist;
            if (st->star)
            {
                /* 
                 *   If the template has any fixed tokens, reparse the
                 *   remaining tokens to see if they refer to a new template.
                 *   If the template was just '*', skip this, since we'd just
                 *   find the same match again.  
                 */
                CTcPrsNode *sub;
                if (st->cnt > 1)
                {
                    /* remove the tokens that matched the fixed part */
                    tl->reduce(st);

                    /* reparse as a new embedded expression */
                    sub = parse_embedded_expr(b, tl);
                }
                else
                {
                    /* the template is just '*', so parse a raw expression */
                    tl->unget();
                    sub = b->parse_expr();
                }

                /* if we failed to parse a sub-expression, return failure */
                if (sub == 0)
                    return 0;
                
                /* create the argument list */
                arglist = new CTPNArglist(1, new CTPNArg(sub));
            }
            else
            {
                /* there's no argument list */
                arglist = new CTPNArglist(0, 0);
            }

            /* create and return the call to the template function */
            return new CTPNCall(func, arglist);
        }
    }

    /* 
     *   There's no template, so process it as an ordinary expression.  Put
     *   the captured token list back into the token stream, and parse an
     *   expression node.  
     */
    tl->unget();
    return b->parse_expr();
}

/*
 *   Capture an embedded expression to an embedded token list
 */
void CTcPrsOpUnary::capture_embedded(CTcEmbedBuilder *b, CTcEmbedTokenList *tl)
{
    /* reset the list */
    tl->reset();

    /* we don't have any nested braces or parens yet */
    int braces = 0, parens = 0, bracks = 0;

    /* run through the list */
    for (;;)
    {
        /* check what we have */
        switch (G_tok->cur())
        {
        case TOKT_LBRACE:
            ++braces;
            break;
            
        case TOKT_RBRACE:
            /* 
             *   if we find an unbalanced close brace, assume that we've
             *   reached the end of the statement without properly
             *   terminating the string 
             */
            if (--braces < 0)
                return;
            break;
            
        case TOKT_LPAR:
            ++parens;
            break;
            
        case TOKT_RPAR:
            --parens;
            break;
            
        case TOKT_LBRACK:
            ++bracks;
            break;
            
        case TOKT_RBRACK:
            --bracks;
            break;
            
        case TOKT_SEM:
            /* 
             *   if we find a semicolon outside of braces, assume that we've
             *   reached the end of the statement without properly
             *   terminating the string 
             */
            if (braces == 0)
                return;
            break;
            
        case TOKT_EOF:
            return;
            
        default:
            /* stop if we've reached the next string segment */
            if (G_tok->cur() == b->end_tok()
                || G_tok->cur() == b->mid_tok())
                return;
            break;
        }
        
        /* add the token to the capture list and move on to the next */
        tl->add_tok(G_tok->getcur());
        G_tok->next();
    }
}

/*
 *   Parse an embedded "if" or "unless" construct.  The two are the same,
 *   except the "unless" inverts the condition.  
 */
CTcPrsNode *CTcPrsOpUnary::parse_embedded_if(
    CTcEmbedBuilder *b, int unless, int &eos, CTcEmbedLevel *parent)
{
    /* set up our embedding stack level */
    CTcEmbedLevel level(CTcEmbedLevel::If, parent);

    /* parse the condition expression */
    CTcPrsNode *cond = G_prs->parse_cond_expr();
    if (cond == 0)
        return 0;

    /* if this is an "unless", invert the condition */
    if (unless)
        cond = new CTPNNot(cond);
    
    /* parse the "then" list */
    CTcPrsNode *then = parse_embedding_list(b, eos, &level);
    if (then == 0)
        return 0;

    /* 
     *   create our top-level 'if' node - leave the 'else' branch empty for
     *   now; we'll build this out if we find an 'else' 
     */
    CTPNIf *top = new CTPNIf(cond, then, 0);

    /* 
     *   as we build out the 'else if' branches, we'll add elses to the tail
     *   of the tree; this is currently the top node 
     */
    CTPNIf *tail = top;

    /* parse zero or more "else if" branches */
    int found_else = FALSE;
    while (!eos
           && (G_tok->cur() == TOKT_ELSE
               || G_tok->cur_tok_matches("otherwise")))
    {
        /* skip the "else"/"otherwise", and check for another "if" */
        if (G_tok->next() == TOKT_IF || G_tok->cur_tok_matches("unless"))
        {
            /* 
             *   <<else if cond>> or <<else unless cond>>.  Note which sense
             *   of the test we're using.
             */
            unless = (G_tok->cur() != TOKT_IF);

            /* skip the "else"/"otherwise" and parse the condition */
            G_tok->next();
            cond = G_prs->parse_cond_expr();
            if (cond == 0)
                return 0;

            /* if it's 'unless', invert the condition */
            if (unless)
                cond = new CTPNNot(cond);

            /* parse the "then" list for this new branch */
            then = parse_embedding_list(b, eos, &level);
            if (then == 0)
                return 0;

            /* add the new 'if' node to the 'else' of the tail of the tree */
            CTPNIf *sub = new CTPNIf(cond, then, 0);
            tail->set_else(sub);

            /* this is the new tail, for adding the next 'else' */
            tail = sub;
        }
        else
        {
            /* it's the final else/otherwise - parse the branch */
            CTcPrsNode *sub = parse_embedding_list(b, eos, &level);
            if (sub == 0)
                return 0;

            /* add it as the else branch of the tail node of the tree */
            tail->set_else(sub);

            /* no more clauses of the "if" can follow an "else" */
            found_else = TRUE;
            break;
        }
    }

    /* 
     *   if we ended without an "else", add an implicit "nil" as the final
     *   "else" branch 
     */
    if (!found_else)
        tail->set_else(b->finish_tree(0));

    /* 
     *   If we're at an "end" token, this is the explicit close of the "if".
     *   Otherwise, the "if" ended implicitly at the end of the string or at
     *   a closing token for a containing structure. 
     */
    if (!eos && G_tok->cur_tok_matches("end"))
        G_tok->next();

    /* return the condition tree */
    return top;
}

/*
 *   "one of" enders 
 */
struct one_of_option
{
    /* ending phrase */
    const char *endph;

    /* list attribute string */
    const char *attr;
};
static one_of_option one_of_list[] =
{
    { "purely at random", "rand" },
    { "then purely at random", "seq,rand" },
    { "at random" , "rand-norpt" },
    { "then at random", "seq,rand-norpt" },
    { "sticky random", "rand,stop" },
    { "as decreasingly likely outcomes", "rand-wt" },
    { "shuffled", "shuffle" },
    { "then shuffled", "seq,shuffle" },
    { "half shuffled", "shuffle2" },
    { "then half shuffled", "seq,shuffle2" },
    { "cycling", "seq" },
    { "stopping", "seq,stop" }
};

/*
 *   Parse an embedded "one of" construct 
 */
CTcPrsNode *CTcPrsOpUnary::parse_embedded_oneof(
    CTcEmbedBuilder *b, int &eos, CTcEmbedLevel *parent)
{
    /* get the embedding token list capturer */
    CTcEmbedTokenList *tl = G_prs->embed_toks_;

    /* set up our stack level */
    CTcEmbedLevel level(CTcEmbedLevel::OneOf, parent);

    /* create a list node to hold the items */
    CTPNList *lst = new CTPNList();

    /* keep going until we get to the end of the list */
    for (;;)
    {
        /* parse the next string or embedded expression */
        CTcPrsNode *ele = parse_embedding_list(b, eos, &level);
        if (ele == 0)
            return 0;

        /* add this element to the list */
        lst->add_element(ele);

        /* capture the ending token list */
        tl->reset();
        capture_embedded(b, tl);

        /* if we're not at an OR, this is the end of the list */
        if (!tl->match("or") && !tl->match("||"))
            break;
    }

    /* 
     *   Search for a match to one of our endings.  If we don't find one, use
     *   "purely at random" as the default. 
     */
    const char *attrs = 0;
    for (size_t i = 0 ; i < countof(one_of_list) ; ++i)
    {
        /* check for a match */
        if (tl->match(one_of_list[i].endph))
        {
            /* got it - set this attribute and stop searching */
            attrs = one_of_list[i].attr;
            break;
        }
    }

    /* 
     *   if we didn't find a match for the ending tokens, we must have the
     *   ending for an enclosing structure; put the tokens back for the
     *   enclosing structure to parse, and use "purely at random" as the
     *   default 
     */
    if (attrs == 0)
    {
        tl->unget();
        attrs = "rand";
    }

    /* create the one-of list */
    return create_oneof_node(b, lst, attrs);
}

/*
 *   Create a one-of list node
 */
CTcPrsNode *CTcPrsOpUnary::create_oneof_node(
    CTcEmbedBuilder *b, CTPNList *lst, const char *attrs)
{
    /*
     *   If we're actually compiling (not just doing a symbol extraction
     *   pass), create our list state object.  This is an anonymous
     *   TadsObject of class OneOfIndexGen, with the following properties:
     *   
     *.    numItems = number of items in the list (integer)
     *.    listAttrs = our 'attrs' list attributes value (string)
     */
    CTcSymObj *obj = 0;
    if (!G_prs->get_syntax_only())
    {
        /* geneate our anonymous OneOfIndexGen instance */
        obj = G_prs->add_gen_obj("OneOfIndexGen");
        if (obj != 0)
        {
            G_prs->add_gen_obj_prop(obj, "numItems", lst->get_count());
            G_prs->add_gen_obj_prop(obj, "listAttrs", attrs);
        }
        else
        {
            /* check to see if OneOfIndexGen is undefined */
            CTcSymbol *cls = G_prs->get_global_symtab()->find(
                "OneOfIndexGen", 13);
            if (cls == 0 || cls->get_type() != TC_SYM_OBJ)
                G_tok->log_error(TCERR_ONEOF_REQ_GENCLS);
        }
    }

    /* return the <<one of>> node */
    return new CTPNStrOneOf(b->is_double(), lst, obj);
}

/*
 *   Parse an embedded "first time" structure
 */
CTcPrsNode *CTcPrsOpUnary::parse_embedded_firsttime(
    CTcEmbedBuilder *b, int &eos, CTcEmbedLevel *parent)
{
    /* get the embedding token list capturer */
    CTcEmbedTokenList *tl = G_prs->embed_toks_;

    /* set up our stack level */
    CTcEmbedLevel level(CTcEmbedLevel::FirstTime, parent);

    /* parse the embedding */
    CTcPrsNode *ele = parse_embedding_list(b, eos, &level);
    if (ele == 0)
        return 0;

    /* capture the ending token list */
    tl->reset();
    capture_embedded(b, tl);

    /* 
     *   check for our "only" list - if we don't match it, put the tokens
     *   back, since they must be the ending list for an enclosing structure 
     */
    if (!tl->match("only"))
        tl->unget();

    /* create this as equivalent to <<one of>>our item<<or>><<stopping>> */
    CTPNList *lst = new CTPNList();
    lst->add_element(ele);

    CTcConstVal cv;
    cv.set_sstr("", 0);
    lst->add_element(new CTPNConst(&cv));

    /* create the <<one of>> node */
    return create_oneof_node(b, lst, "seq,stop");
}


/*
 *   Check for an end token in an embedded expression construct.  Reads from
 *   the token stream, but restores the tokens when done.  
 */
int CTcPrsOpUnary::parse_embedded_end_tok(CTcEmbedBuilder *b,
                                          CTcEmbedLevel *parent,
                                          const char **open_kw)
{
    /* capture the current expression's token list */
    CTcEmbedTokenList *tl = G_prs->embed_toks_;
    tl->reset();
    capture_embedded(b, tl);

    /* parse the end token */
    int ret = parse_embedded_end_tok(tl, parent, open_kw);

    /* un-get the captured tokens */
    tl->unget();

    /* return the result */
    return ret;
}

/*
 *   Check for an end token in an embedded expression construct.  Compares
 *   tokens in the given capture list.  
 */
int CTcPrsOpUnary::parse_embedded_end_tok(CTcEmbedTokenList *tl,
                                          CTcEmbedLevel *parent,
                                          const char **open_kw)
{
    /* presume we won't find a closing keyword */
    *open_kw = "unknown";

    /* 
     *   consider ending keywords to always be significant, regardless of
     *   context, since these are unambiguous with valid expressions 
     */
    if (tl->match("else")
        || tl->match("else if *")
        || tl->match("else unless *"))
    {
        *open_kw = "<<if>> or <<unless>>";
        return TRUE;
    }

    /* check for "if" ending tokens - else, end */
    if (tl->match("end")
        || tl->match("otherwise")
        || tl->match("otherwise if *")
        || tl->match("otherwise unless *"))
    {
        *open_kw = "<<if>> or <<unless>>";
        if (parent->is_in(CTcEmbedLevel::If))
            return TRUE;
    }

    /* check for an "or", which delimits branches of a "one of" */
    if (tl->match("or") || tl->match("||"))
    {
        *open_kw = "<<one of>>";
        if (parent->is_in(CTcEmbedLevel::OneOf))
            return TRUE;
    }

    /* check for "one of" ending tokens */
    for (size_t i = 0 ; i < countof(one_of_list) ; ++i)
    {
        if (tl->match(one_of_list[i].endph))
        {
            *open_kw = "<<one of>>";
            if (parent->is_in(CTcEmbedLevel::OneOf))
                return TRUE;
            break;
        }
    }

    /* check for an "only", which ends a "first time" */
    if (tl->match("only"))
    {
        *open_kw = "<<first time>>";
        if (parent->is_in(CTcEmbedLevel::FirstTime))
            return TRUE;
    }

    /* didn't find a close token */
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Embedded <<one of>> list in a string
 */

CTcPrsNode *CTPNStrOneOfBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
{
    lst_ = (CTPNList *)lst_->adjust_for_dyn(info);
    return this;
}

CTcPrsNode *CTPNStrOneOfBase::fold_constants(CTcPrsSymtab *symtab)
{
    lst_ = (CTPNList *)lst_->fold_constants(symtab);
    return this;
}

/* ------------------------------------------------------------------------ */
/*
 *   Unary Operator Parser
 */

CTcPrsNode *CTcPrsOpUnary::parse() const
{
    /* get the current token, which may be a prefix operator */
    tc_toktyp_t op = G_tok->cur();

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
        {
            /* skip the operator */
            G_tok->next();
            
            /* 
             *   recursively parse the unary expression to which to apply the
             *   operator 
             */
            CTcPrsNode *sub = parse();
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
                /* 
                 *   we shouldn't ever get here, since this switch should
                 *   contain every case that brought us into it in the first
                 *   place
                 */
                return 0;
            }
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
    /* try folding a constant value */
    CTcPrsNode *ret = eval_const_not(subexpr);

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
    /* 
     *   if it's a simple symbol, create an unresolved symbol node for it;
     *   otherwise parse the entire expression 
     */
    CTcPrsNode *subexpr;
    if (G_tok->cur() == TOKT_SYM)
    {
        /* 
         *   create an unresolved symbol node - we'll resolve this during
         *   code generation 
         */
        const CTcToken *tok = G_tok->getcur();
        subexpr = new CTPNSym(tok->get_text(), tok->get_text_len());

        /*
         *   If the symbol is undefined, assume it's a property, but make the
         *   definition "weak".  Starting in 3.1, &x can be a property ID, a
         *   function pointer, or a built-in function pointer, so it's no
         *   longer safe to just assume it's a property.  If we don't see the
         *   symbol used in any other context, we'll assume it's a property,
         *   but mark it as "weak" if it's not already defined.  This will
         *   allow any conflicting definition to override the property
         *   assumption without complaint.  
         */
        G_prs->get_global_symtab()->find_or_def_prop_weak(
            tok->get_text(), tok->get_text_len(), FALSE);

        /* skip the symbol */
        G_tok->next();
    }
    else if (G_tok->cur() == TOKT_OPERATOR)
    {
        /* operator property - parse the operator name */
        CTcToken proptok;
        if (G_prs->parse_op_name(&proptok))
        {
            /* skip the last token of the operator name */
            G_tok->next();
        }

        /* create a symbol for it */
        subexpr = new CTPNSym(proptok.get_text(), proptok.get_text_len());
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
        CTcConstVal *cval = subexpr->get_const_val();
        if (cval->get_type() == TC_CVT_INT)
        {
            /* 
             *   Negating an integer.  There's a special overflow case to
             *   check for: if we're negating INT32MINVAL, the result doesn't
             *   fit in a signed integer, so we need to promote it to float. 
             */
            long l = cval->get_val_int();
            if (l <= INT32MINVAL)
            {
                /* overflow - promote to BigNumber */
                vbignum_t b((ulong)2147483648U);
                cval->set_float(&b, TRUE);
            }
            else
            {
                /* set the value negative in the subexpression */
                cval->set_int(-l);
            }
        }
        else if (subexpr->get_const_val()->get_type() == TC_CVT_FLOAT)
        {
            /* get the original value */
            const char *ctxt = cval->get_val_float();
            size_t clen = cval->get_val_float_len();

            /* if the old value is negative, simply remove the sign */
            if (clen > 0 && ctxt[0] == '-')
            {
                /* keep the old value, removing the sign */
                cval->set_float(ctxt + 1, clen - 1, cval->is_promoted());
            }
            else
            {
                /* allocate new buffer for the float text plus a '-' */
                char *new_txt = (char *)G_prsmem->alloc(clen + 1);

                /* insert the minus sign */
                new_txt[0] = '-';

                /* add the original string */
                memcpy(new_txt + 1, ctxt, clen);

                /* update the subexpression's constant value to the new text */
                cval->set_float(new_txt, clen + 1, cval->is_promoted());
            }

            /* check for possible demotion back to int */
            cval->demote_float();
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
                         G_tok->get_op_text(TOKT_DEC));
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
    /* parse a primary expression */
    CTcPrsNode *sub = parse_primary();
    if (sub == 0)
        return 0;

    /* keep going as long as we find postfix operators */
    for (;;)
    {
        /* check for a postfix operator */
        tc_toktyp_t op = G_tok->cur();
        switch(op)
        {
        case TOKT_LPAR:
            /* left paren - function or method call */
            if (allow_call_expr)
            {
                /* parse the function call expression */
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
    /* skip the open paren */
    G_tok->next();

    /* keep going until we find the close paren */
    int argc;
    CTPNArg *arg_head;
    for (argc = 0, arg_head = 0 ;; )
    {
        /* presume it's not a named argument */
        CTcToken param_name;
        param_name.settyp(TOKT_INVALID);

        /* if this is the close paren, we're done */
        if (G_tok->cur() == TOKT_RPAR)
            break;

        /* check for a named argument:  "symbol: expression" */
        if (G_tok->cur() == TOKT_SYM)
        {
            /* remember the symbol, in case it is in fact a named argument */
            param_name = *G_tok->getcur();
            
            /* peek ahead - if a colon follows, it's a named argument */
            if (G_tok->next() == TOKT_COLON)
            {
                /* 
                 *   it's a named argument - we already have the name stashed
                 *   away for when we create the argument node, so simply
                 *   skip the colon so we can parse the argument value
                 *   expression 
                 */
                G_tok->next();
            }
            else
            {
                /* 
                 *   No colon - it's a regular parameter expression.  Put
                 *   back the token we peeked at, clear out the name token,
                 *   and keep parsing as normal.  
                 */
                G_tok->unget();
                param_name.settyp(TOKT_INVALID);
            }
        }

        /* 
         *   parse this argument expression - this is an 'assignment'
         *   expression, since a comma is an argument separator in this
         *   context rather than an operator 
         */
        CTcPrsNode *expr = S_op_asi.parse();
        if (expr == 0)
            return 0;

        /* if this is a positional argument (not named), count it */
        if (param_name.gettyp() != TOKT_SYM)
            ++argc;

        /* create a new argument node */
        CTPNArg *arg_cur = new CTPNArg(expr);

        /* if it's a named argument, store the name in the argument node */
        arg_cur->set_name(&param_name);

        /* check to see if the argument is followed by an ellipsis */
        if (G_tok->cur() == TOKT_ELLIPSIS)
        {
            /* this isn't allowed for a named argument */
            if (param_name.gettyp() == TOKT_SYM)
                G_tok->log_error(TCERR_NAMED_ARG_NO_ELLIPSIS);

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
    /* parse the argument list */
    CTPNArglist *arglist = parse_arg_list();
    if (arglist == 0)
        return 0;

    /* check for the special "defined()" syntax */
    if (lhs != 0 && lhs->sym_text_matches("defined", 7))
    {
        /* make sure we have one argument that's a symbol */
        CTcPrsNode *arg;
        if (arglist->get_argc() == 1
            && (arg = arglist->get_arg_list_head()->get_arg_expr())
                ->get_sym_text() != 0)
        {
            /* look up the symbol */
            CTcSymbol *sym = G_prs->get_global_symtab()->find(
                arg->get_sym_text(), arg->get_sym_text_len());

            /* 
             *   The result is a constant 'true' or 'nil' node, depending on
             *   whether the symbol is defined.  Note that this is a "compile
             *   time constant" expression, not a true constant - flag it as
             *   such so that we don't generate a warning if this value is
             *   used as the conditional expression of an if, while, or for.
             */
            CTcConstVal cval;
            cval.set_bool(sym != 0 && sym->get_type() != TC_SYM_UNKNOWN);
            cval.set_ctc(TRUE);
            return new CTPNConst(&cval);
        }
        else
        {
            /* invalid syntax */
            G_tok->log_error(TCERR_DEFINED_SYNTAX);
        }
    }

    /* check for the special "__objref" syntax */
    if (lhs != 0 && lhs->sym_text_matches("__objref", 8))
    {
        /* assume we won't generate an error or warning if it's undefined */
        int errhandling = 0;

        /* get the rightmost argument */
        CTPNArg *curarg = arglist->get_arg_list_head();

        /* if we have other than two arguments, it's an error */
        if (arglist->get_argc() > 2)
        {
            G_tok->log_error(TCERR___OBJREF_SYNTAX);
            return lhs;
        }

        /* if we have two arguments, get the warning/error mode */
        if (arglist->get_argc() == 2)
        {
            /* get the argument, and skip to the next to the left */
            CTcPrsNode *arg = curarg->get_arg_expr();
            curarg = curarg->get_next_arg();

            /* it has to be "warn" or "error" */
            if (arg->sym_text_matches("warn", 4))
            {
                errhandling = 1;
            }
            else if (arg->sym_text_matches("error", 5))
            {
                errhandling = 2;
            }
            else
            {
                G_tok->log_error(TCERR___OBJREF_SYNTAX);
                return lhs;
            }
        }

        /* the first argument must be a symbol */
        CTcPrsNode *arg = curarg->get_arg_expr();
        if (arg->get_sym_text() != 0)
        {
            /* look up the symbol */
            CTcSymbol *sym = G_prs->get_global_symtab()->find(
                arg->get_sym_text(), arg->get_sym_text_len());
            
            /* 
             *   The result is the object reference value if the symbol is
             *   defined as an object, or nil if it's undefined or something
             *   other than an object.
             */
            CTcConstVal cval;
            if (sym != 0 && sym->get_type() == TC_SYM_OBJ)
            {
                /* it's an object - the result is the object symbol */
                return arg;
            }
            else
            {
                /* log a warning or error, if applicable */
                if (errhandling != 0 && !G_prs->get_syntax_only())
                {
                    /* note whether it's undefined or otherwise defined */
                    int errcode =
                        (sym == 0 ? TCERR_UNDEF_SYM : TCERR_SYM_NOT_OBJ);

                    /* log an error or warning, as desired */
                    if (errhandling == 1)
                        G_tok->log_warning(
                            errcode, (int)arg->get_sym_text_len(),
                            arg->get_sym_text());
                    else
                        G_tok->log_error(
                            errcode, (int)arg->get_sym_text_len(),
                            arg->get_sym_text());
                }
                
                /* not defined or non-object - the result is nil */
                cval.set_bool(FALSE);
                cval.set_ctc(TRUE);
                return new CTPNConst(&cval);
            }
        }
        else
        {
            /* invalid syntax */
            G_tok->log_error(TCERR___OBJREF_SYNTAX);
        }
    }

    /* build and return the function call node */
    return new CTPNCall(lhs, arglist);
}

/*
 *   Parse a subscript expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_subscript(CTcPrsNode *lhs)
{
    /* skip the '[' */
    G_tok->next();

    /* parse the expression within the brackets */
    CTcPrsNode *subscript = S_op_comma.parse();
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
    CTcPrsNode *cval = eval_const_subscript(lhs, subscript);

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
CTcPrsNode *CTcPrsOpUnary::eval_const_subscript(
    CTcPrsNode *lhs, CTcPrsNode *subscript)
{
    /* assume we won't be able to evaluate this as a constant */
    CTcPrsNode *c = 0;

    /* 
     *   if we're subscripting a constant list by a constant index value,
     *   we can evaluate a constant result 
     */
    if (lhs->is_const() && subscript->is_const())
    {
        /* check the type of value we're indexing */
        switch (lhs->get_const_val()->get_type())
        {
        case TC_CVT_LIST:
            /* 
             *   It's a constant list.  Lists can only be indexed by integer
             *   values. 
             */
            if (subscript->get_const_val()->get_type() == TC_CVT_INT)
            {
                /* 
                 *   it's an integer - index the constant list by the
                 *   constant subscript to get the element value, which
                 *   replaces the entire list-and-index expression
                 */
                c = lhs->get_const_val()->get_val_list()->get_const_ele(
                    subscript->get_const_val()->get_val_int());
            }
            else
            {
                /* a list index must be an integer */
                G_tok->log_error(TCERR_CONST_IDX_NOT_INT);
            }
            break;

        case TC_CVT_SSTR:
        case TC_CVT_OBJ:
        case TC_CVT_FUNCPTR:
        case TC_CVT_ANONFUNCPTR:
        case TC_CVT_FLOAT:
            /* 
             *   these types don't define indexing as a native operator, but
             *   it's possible for this to be meaningful at run-time via
             *   operator overloading; simply leave the constant expression
             *   unevaluated so that we generate code to perform the index
             *   operation at run-time 
             */
            break;
            
        default:
            /* other types definitely cannot be indexed */
            G_tok->log_error(TCERR_CONST_IDX_INV_TYPE);
            break;
        }
    }
    
    /* return the constant result, if any */
    return c;
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
 *   Parse a primary expression 
 */
CTcPrsNode *CTcPrsOpUnary::parse_primary()
{
    CTcPrsNode *sub;
    CTcConstVal cval;
    tc_toktyp_t t;
    CTcToken tok, tok2;
    
    /* keep going until we find something interesting */
    for (;;)
    {
        /* determine what we have */
        switch(G_tok->cur())
        {
        case TOKT_LBRACE:
            /* short form of anonymous function */
            return parse_anon_func(TRUE, FALSE);

        case TOKT_METHOD:
            /* parse an anonymous method */
            return parse_anon_func(FALSE, TRUE);
            
        case TOKT_FUNCTION:
            /* parse an anonymous function */
            return parse_anon_func(FALSE, FALSE);

        case TOKT_OBJECT:
            /* in-line object definition */
            return parse_inline_object(FALSE);

        case TOKT_NEW:
            /* skip the operator and check for 'function' or 'method' */
            if ((t = G_tok->next()) == TOKT_FUNCTION)
            {
                /* it's an anonymous function definition - go parse it */
                sub = parse_anon_func(FALSE, FALSE);
            }
            else if (t == TOKT_METHOD)
            {
                /* anonymous method */
                sub = parse_anon_func(FALSE, TRUE);
            }
            else
            {
                /* check for the 'transient' keyword */
                int trans = (G_tok->cur() == TOKT_TRANSIENT);
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
            /* integer constant value */
            cval.set_int(G_tok->getcur()->get_int_val());
            goto return_constant;

        case TOKT_FLOAT:
            /* floating point constant value */
            cval.set_float(G_tok->getcur()->get_text(),
                           G_tok->getcur()->get_text_len(),
                           FALSE);
            goto return_constant;

        case TOKT_BIGINT:
            /* an integer constant promoted to float due to overflow */
            cval.set_float(G_tok->getcur()->get_text(),
                           G_tok->getcur()->get_text_len(),
                           TRUE);
            goto return_constant;
            
        case TOKT_SSTR:
        handle_sstring:
            /* single-quoted string - the result is a constant string value */
            cval.set_sstr(G_tok->getcur());
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
            
        case TOKT_RESTR:
            /* regular expression string */
            cval.set_restr(G_tok->getcur());
            goto return_constant;

        case TOKT_DSTR_START:
            /* a string implicitly references 'self' */
            G_prs->set_self_referenced(TRUE);

            /* parse the embedding expression */
            {
                CTcEmbedBuilderDbl builder;
                return parse_embedding(&builder);
            }
            
        case TOKT_SSTR_START:
            /* parse the embedded expression */
            {
                CTcEmbedBuilderSgl builder;
                return parse_embedding(&builder);
            }

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

        case TOKT_INVOKEE:
            /* generate the "invokee" node */
            G_tok->next();
            return new CTPNInvokee();

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

        case TOKT_POUND:
            /*
             *   When evaluating a debugger expression, we accept a special
             *   notation for certain types that can't always be represented
             *   in ordinary source code:
             *   
             *.     #__obj n - object number n (n is in decimal)
             *.     #__prop n - property number n
             *.     #__sstr ofs - s-string at offset ofs (in decimal)
             *.     #__list ofs - list at offset ofs
             *.     #__enum n - enum n
             *.     #__bifptr s i - built-in function pointer, set s, index i
             *.     #__invalid - invalid type
             */
            tok = *G_tok->copycur();
            if (G_prs->is_debug_expr()
                && G_tok->next() == TOKT_SYM
                && ((tok2 = *G_tok->copycur()).text_matches("__obj", 5)
                    || tok2.text_matches("__obj", 5)
                    || tok2.text_matches("__prop", 6)
                    || tok2.text_matches("__sstr", 6)
                    || tok2.text_matches("__list", 6)
                    || tok2.text_matches("__func", 6)
                    || tok2.text_matches("__enum", 6)
                    || tok2.text_matches("__bifptr", 8)))
            {
                /* get the integer value */
                if (G_tok->next() == TOKT_INT)
                {
                    /* get and skip the integer value */
                    long i = G_tok->getcur()->get_int_val();
                    G_tok->next();

                    /* check which symbol we have */
                    if (tok2.text_matches("__obj", 5))
                    {
                        /* generate an object expression */
                        cval.set_obj(i, TC_META_UNKNOWN);
                        return new CTPNDebugConst(&cval);
                    }
                    else if (tok2.text_matches("__prop", 6))
                    {
                        /* generate a property expression */
                        cval.set_prop(i);
                        return new CTPNDebugConst(&cval);
                    }
                    else if (tok2.text_matches("__sstr", 6))
                    {
                        /* generate a constant-pool string expression */
                        cval.set_sstr(i);
                        return new CTPNDebugConst(&cval);
                    }
                    else if (tok2.text_matches("__list", 6))
                    {
                        /* generate a constant-pool list expression */
                        cval.set_list(i);
                        return new CTPNDebugConst(&cval);
                    }
                    else if (tok2.text_matches("__func", 6))
                    {
                        /* generate a fake symbol for the function */
                        CTcSymFunc *sym = new CTcSymFunc(
                            ".anon", 5, FALSE, 0, 0, FALSE, FALSE,
                            FALSE, FALSE, FALSE, TRUE);

                        /* set the absolute address */
                        sym->set_abs_addr(i);

                        /* generate a code-pool function pointer expression */
                        cval.set_funcptr(sym);
                        return new CTPNDebugConst(&cval);
                    }
                    else if (tok2.text_matches("__enum", 6))
                    {
                        /* generate an enum expression */
                        cval.set_enum(i);
                        return new CTPNDebugConst(&cval);
                    }
                    else if (tok2.text_matches("__bifptr", 8))
                    {
                        /* get and skip the function index token */
                        long j = G_tok->getcur()->get_int_val();
                        G_tok->next();

                        /* set up a fake symbol for the function */
                        CTcSymBif *sym = new CTcSymBif(
                            ".anon", 5, FALSE, (ushort)i, (ushort)j,
                            FALSE, 0, 0, FALSE);

                        /* generate a built-in function pointer expression */
                        cval.set_bifptr(sym);
                        return new CTPNDebugConst(&cval);
                    }
                }
                else
                {
                    /* it's not a special sequence - put back the tokens */
                    G_tok->unget(&tok2);
                    G_tok->unget(&tok);
                }
            }
            else
            {
                /* it's not one of our symbols - put back the '#' */
                G_tok->unget(&tok);
            }

            /* 
             *   if we got this far, we didn't get a valid special debugger
             *   value - complain about the '#' 
             */
            goto bad_primary;

        default:
        bad_primary:
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

        /* this passes method context information to the inheritor */
        G_prs->set_full_method_ctx_referenced(TRUE);

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
    CTcPrsNode *target = parse_postfix(FALSE, FALSE);

    /* set up the "delegated" node */
    CTcPrsNode *lhs = new CTPNDelegated(target);

    /* return the rest as a normal member expression */
    return parse_member(lhs);
}

/*
 *   Parse a list 
 */
CTcPrsNode *CTcPrsOpUnary::parse_list()
{
    /* assume this isn't a lookup table (with "key->val" pairs) */
    int is_lookup_table = FALSE;

    /* we're not at the default value for a lookup table ("*->val") */
    int at_def_val = FALSE;

    /* set up a nil value for adding placeholders (for error recovery) */
    CTcConstVal nilval;
    nilval.set_nil();
    
    /* skip the opening '[' */
    G_tok->next();

    /* 
     *   create the list expression -- we'll add elements to the list as
     *   we parse the elements
     */
    CTPNList *lst = new CTPNList();

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

        case TOKT_TIMES:
            /*
             *   If a -> follows, this is the default value for a lookup
             *   table list. 
             */
            if (G_tok->next() == TOKT_ARROW)
            {
                /* skip the arrow and get to the value */
                G_tok->next();

                /* 
                 *   if this is a non-lookup table list with other entries,
                 *   this is an error 
                 */
                if (!is_lookup_table && lst->get_count() != 0)
                {
                    /* log the error, but keep parsing the list */
                    G_tok->log_error(TCERR_ARROW_IN_LIST, lst->get_count());
                }
                else
                {
                    /* it's definitely a lookup list now */
                    is_lookup_table = TRUE;

                    /* we're now on the default value */
                    at_def_val = TRUE;
                }
            }
            else
            {
                /* it's not *-> - put back the next token */
                G_tok->unget();
            }

        default:
            /* it must be the next element expression */
            break;
        }

        /* 
         *   Attempt to parse another list element expression.  Parse just
         *   below a comma expression, because commas can be used to
         *   separate list elements.  
         */
        CTcPrsNode *ele = S_op_asi.parse();
        if (ele == 0)
            return 0;
        
        /* add the element to the list */
        lst->add_element(ele);

        /* if this was the default value, the list should end here */
        if (at_def_val && G_tok->cur() != TOKT_RBRACK)
        {
            /* log an error */
            G_tok->log_error_curtok(TCERR_LOOKUP_LIST_EXPECTED_END_AT_DEF);

            /* we've left the default value */
            at_def_val = FALSE;

            /* 
             *   if we're at a comma, add a nil value to the list to
             *   resynchronize - they must want to add more Key->Value pairs 
             */
            if (G_tok->cur() == TOKT_COMMA)
                lst->add_element(new CTPNConst(&nilval));
        }

        /* check what follows the element */
        switch(G_tok->cur())
        {
        case TOKT_COMMA:
            /* skip the comma introducing the next element */
            G_tok->next();

            /*
             *   If this is a lookup table list, commas are only allowed at
             *   even elements: [ODD -> EVEN, ODD -> EVEN...].
             */
            if (is_lookup_table && (lst->get_count() & 1) != 0)
            {
                /* log an error */
                G_tok->log_error(TCERR_LOOKUP_LIST_EXPECTED_ARROW,
                                 (lst->get_count()+1)/2);

                /* 
                 *   Add an implied nil element as the value.  This will help
                 *   resynchronize with the source code in the most likely
                 *   case that they simply left out a ->value item, so that
                 *   we don't log alternating "expected comma" and "expected
                 *   arrow" messages on every subsequent delimiter.  
                 */
                lst->add_element(new CTPNConst(&nilval));
            }

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

        case TOKT_ARROW:
            /* skip the arrow */
            G_tok->next();

            /* 
             *   '->' indicates a lookup table key->value list.  This is an
             *   all-or-nothing proposition: it has to be on every pair of
             *   elements, or on none of them.
             */
            if (lst->get_count() == 1)
            {
                /* first element: this is now a lookup table list */
                is_lookup_table = TRUE;
            }
            else if (!is_lookup_table)
            {
                /* 
                 *   it's not the first element, and we haven't seen arrows
                 *   before, so this list shouldn't have arrows at all 
                 */
                G_tok->log_error(TCERR_ARROW_IN_LIST, lst->get_count());
            }
            else if (is_lookup_table && (lst->get_count() & 1) == 0)
            {
                /*
                 *   We have an even number of elements, so an arrow is not
                 *   allowed here under any circumstances. 
                 */
                G_tok->log_error(TCERR_MISPLACED_ARROW_IN_LIST,
                                 lst->get_count()/2);

                /* 
                 *   They probably just left out the key value accidentally,
                 *   so add a nil value as the key.  This will avoid a
                 *   cascade of errors for subsequent delimiters if they did
                 *   just leave out one item. 
                 */
                lst->add_element(new CTPNConst(&nilval));
            }
            break;

        case TOKT_RBRACK:
            /* 
             *   we're done with the list - skip the bracket and return
             *   the finished list 
             */
            G_tok->next();

            /* 
             *   If this is a lookup table list, make sure we ended on an
             *   even number - if not, we're missing a ->Value entry.  The
             *   exception is that if the last value was a default, we end on
             *   an odd item.  
             */
            if (is_lookup_table
                && (lst->get_count() & 1) != 0
                && !at_def_val)
            {
                /* log the error */
                G_tok->log_error(TCERR_LOOKUP_LIST_EXPECTED_ARROW,
                                 (lst->get_count()+1)/2);
            }
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

    /* if it's a lookup table list, mark it as such */
    if (is_lookup_table)
        lst->set_lookup_table();

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

    /* fixups are allocated in parser memory, so they're gone now */
    G_objfixup = 0;
    G_propfixup = 0;
    G_enumfixup = 0;
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
 *   Make adjustments for dynamic evaluation
 */
CTcPrsNode *CTcPrsNodeBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
{
    /* by default, a node can't be used in dynamic evaluation */
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
CTcPrsNode *CTPNConstBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
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
 *   Adjust for dynamic compilation
 */
CTcPrsNode *CTPNListBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
{
    CTPNListEle *cur;

    /* run through my list and adjust each element */
    for (cur = head_ ; cur != 0 ; cur = cur->get_next())
    {
        /* adjust this element */
        cur->adjust_for_dyn(info);
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
 *   If-nil operator ?? node base class 
 */

/*
 *   fold constants 
 */
CTcPrsNode *CTPNIfnilBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* fold constants in the subnodes */
    first_ = first_->fold_constants(symtab);
    second_ = second_->fold_constants(symtab);

    /* 
     *   if the first is now a constant, we can fold this entire expression
     *   node by choosing the first or second node.  Otherwise return myself
     *   unchanged.  
     */
    if (first_->is_const())
    {
        /* 
         *   if the first expression is nil, return the second, otherwise
         *   return the first 
         */
        return (first_->get_const_val()->get_type() == TC_CVT_NIL
                ? second_ : first_);
    }
    else
    {
        /* we can't fold this node any further - return it unchanged */
        return this;
    }
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
         *   The condition is a constant - the result is the 'then' or 'else'
         *   part, based on the condition's value.  
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
 *   adjust for dynamic (run-time) compilation
 */
CTcPrsNode *CTPNDstrBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
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
 *   adjust for dynamic (run-time) compilation 
 */
CTcPrsNode *CTPNSymBase::adjust_for_dyn(const tcpn_dyncomp_info *)
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
    /* fold each list element */
    CTPNArg *cur;
    for (cur = get_arg_list_head() ; cur != 0 ; cur = cur->get_next_arg())
        cur->fold_constants(symtab);

    /* return myself with no further folding */
    return this;
}

/*
 *   adjust for dynamic (run-time) compilation 
 */
CTcPrsNode *CTPNArglistBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
{
    /* adjust each argument list member */
    CTPNArg *arg;
    for (arg = list_ ; arg != 0 ; arg = arg->get_next_arg())
    {
        /* 
         *   adjust this argument; assume the argument node itself isn't
         *   affected 
         */
        arg->adjust_for_dyn(info);
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

/*
 *   Set the parameter name 
 */
void CTPNArgBase::set_name(const CTcToken *tok)
{
    /* remember the name token */
    name_ = *tok;
}

/* ------------------------------------------------------------------------ */
/*
 *   Member with no arguments 
 */

/*
 *   adjust for dynamic (run-time) compilation 
 */
CTcPrsNode *CTPNMemberBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
{
    /* adjust the object and property expressions */
    obj_expr_ = obj_expr_->adjust_for_dyn(info);
    prop_expr_ = prop_expr_->adjust_for_dyn(info);

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
 *   adjust for dynamic (run-time) compilation 
 */
CTcPrsNode *CTPNMemArgBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
{
    /* don't allow in speculative mode due to possible side effects */
    if (info->speculative)
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* 
     *   adjust my object expression, property expression, and argument
     *   list 
     */
    obj_expr_ = obj_expr_->adjust_for_dyn(info);
    prop_expr_ = prop_expr_->adjust_for_dyn(info);
    arglist_->adjust_for_dyn(info);

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
 *   adjust for dynamic (run-time) compilation 
 */
CTcPrsNode *CTPNCallBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
{
    /* don't allow in speculative mode because of side effects */
    if (info->speculative)
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* adjust the function expression */
    func_ = func_->adjust_for_dyn(info);
    
    /* adjust the argument list (assume it doesn't change) */
    arglist_->adjust_for_dyn(info);
    
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

    /* we don't have a generated byte code range yet */
    start_ofs_ = end_ofs_ = 0;
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
    /*
     *   Look for the symbol.  Start in the current symbol table, and work
     *   outwards to the outermost enclosing table.  
     */
    for (CTcPrsSymtab *curtab = this ; curtab != 0 ;
         curtab = curtab->get_parent())
    {
        /* look for the symbol in this table */
        CTcSymbol *entry = curtab->find_direct(sym, len);
        if (entry != 0)
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
 *   Add a formal parameter symbol 
 */
CTcSymLocal *CTcPrsSymtab::add_formal(const textchar_t *sym, size_t len,
                                      int formal_num, int copy_str)
{
    CTcSymLocal *lcl;
    
    /* 
     *   Make sure it's not already defined in our own symbol table - if
     *   it is, log an error and ignore the redundant definition.  (We
     *   only care about our own scope, not enclosing scopes, since it's
     *   perfectly fine to hide variables in enclosing scopes.)  
     */
    if (find_direct(sym, len) != 0)
    {
        /* log an error */
        G_tok->log_error(TCERR_FORMAL_REDEF, (int)len, sym);

        /* don't add the symbol again */
        return 0;
    }

    /* create the symbol entry */
    lcl = new CTcSymLocal(sym, len, copy_str, TRUE, formal_num);

    /* add it to the table */
    add_entry(lcl);

    /* return the new symbol */
    return lcl;
}

/*
 *   Add the current token as a local variable symbol, initially unreferenced
 *   and uninitialized 
 */
CTcSymLocal *CTcPrsSymtab::add_local(int local_num)
{
    return add_local(G_tok->getcur()->get_text(),
                     G_tok->getcur()->get_text_len(),
                     local_num, FALSE, FALSE, FALSE);
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
    if (find_direct(sym, len) != 0)
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
    add_entry(lcl);

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
    if (find_direct(sym, len) != 0)
    {
        /* log the error */
        G_tok->log_error(TCERR_CODE_LABEL_REDEF, (int)len, sym);

        /* don't create the symbol again - return the original definition */
        return 0;
    }

    /* create the symbol entry */
    lbl = new CTcSymLabel(sym, len, copy_str);

    /* add it to the table */
    add_entry(lbl);

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
    /*
     *   Look for the symbol.  Start in the current symbol table, and work
     *   outwards to the outermost enclosing table. 
     */
    for (CTcPrsSymtab *curtab = this ; ; curtab = curtab->get_parent())
    {
        /* look for the symbol in this table */
        CTcSymbol *entry = (CTcSymbol *)curtab->find_direct(sym, len);
        if (entry != 0)
        {
            /* mark the entry as referenced */
            entry->mark_referenced();

            /* 
             *   if this is a non-weak property definition, and this is a
             *   property symbol, remove any existing weak flag from the
             *   property symbol 
             */
            if (entry->get_type() == TC_SYM_PROP
                && (action == TCPRS_UNDEF_ADD_PROP
                    || action == TCPRS_UNDEF_ADD_PROP_NO_WARNING))
                ((CTcSymProp *)entry)->set_weak(FALSE);

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
            case TCPRS_UNDEF_ADD_PROP_NO_WARNING:
            case TCPRS_UNDEF_ADD_PROP_WEAK:
                {
                    /* create a new symbol of type property */
                    CTcSymProp *prop = new CTcSymProp(
                        sym, len, copy_str, G_cg->new_prop_id());

                    /* mark it as "weak" if desired */
                    if (action == TCPRS_UNDEF_ADD_PROP_WEAK)
                        prop->set_weak(TRUE);

                    /* use it as the new table entry */
                    entry = prop;
                }
                
                /* show a warning if desired */
                if (action == TCPRS_UNDEF_ADD_PROP)
                    G_tok->log_warning(TCERR_ASSUME_SYM_PROP, (int)len, sym);

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
 *   Find a symbol.  If the symbol is already defined as a "weak" property,
 *   delete the existing symbol to make way for the overriding strong
 *   definition. 
 */
CTcSymbol *CTcPrsSymtab::find_delete_weak(const char *name, size_t len)
{
    /* look up the symbol */
    CTcSymbol *sym = find(name, len);

    /* if it's a weak property definition, delete it */
    if (sym != 0
        && sym->get_type() == TC_SYM_PROP
        && ((CTcSymProp *)sym)->is_weak())
    {
        /* delete it and forget it */
        remove_entry(sym);
        sym = 0;
    }

    /* return what we found */
    return sym;
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
    param_index_ = 0;
    is_param_ = is_param;
    is_named_param_ = FALSE;
    is_opt_param_ = FALSE;
    is_list_param_ = FALSE;

    /* presume it's a regular stack variable (not a context local) */
    is_ctx_local_ = FALSE;
    ctx_orig_ = 0;
    ctx_var_num_ = 0;
    ctx_level_ = 0;
    ctx_var_num_set_ = FALSE;

    /* presume there's no default value expression */
    defval_expr_ = 0;

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
     *   Note if this is some kind of parameter.  Some parameters are
     *   represented as locals: varargs-list parameters, optional parameters,
     *   and named parameters.  We need to check for those kinds of
     *   parameters specifically because they otherwise look like regular
     *   local variables.  
     */
    int param = (is_param() || is_list_param()
                 || is_opt_param() || is_named_param());

    /* 
     *   If it's unreferenced or unassigned (or both), log an error; note
     *   that a formal parameter is always assigned, since the value is
     *   assigned by the caller.  
     */
    if (!get_val_used() && !get_val_assigned() && !param)
    {
        /* this is a regular local that was never assigned or referenced */
        err = TCERR_UNREFERENCED_LOCAL;
    }
    else if (!get_val_used())
    {
        if (param)
        {
            /* it's a parameter - generate only a pendantic error */
            sev = TC_SEV_PEDANTIC;
            err = TCERR_UNREFERENCED_PARAM;
        }
        else
        {
            /* 
             *   this is a regular local with a value that was assigned and
             *   never used 
             */
            err = TCERR_UNUSED_LOCAL_ASSIGNMENT;
        }
    }
    else if (!get_val_assigned() && !param)
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
    /* create a new local with the same name */
    CTcSymLocal *lcl = new CTcSymLocal(
        get_sym(), get_sym_len(), FALSE, FALSE, 0);

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
int CTcSymLocalBase::apply_ctx_var_conv(
    CTcPrsSymtab *symtab, CTPNCodeBody *code_body)
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
    /* if it's not a TadsObject symbol, we can't fold it into a constant */
    if (get_metaclass() != TC_META_TADSOBJ)
        return 0;

    /* set up the object constant */
    CTcConstVal cval;
    cval.set_obj(get_obj_id(), get_metaclass());

    /* return a constant node */
    return new CTPNConst(&cval);
}

/*
 *   Add a superclass name entry.  
 */
void CTcSymObjBase::add_sc_name_entry(const char *txt, size_t len)
{
    /* create the entry object */
    CTPNSuperclass *entry = new CTPNSuperclass(txt, len);

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
    /* 
     *   Scan my direct superclasses.  For each one, check to see if my
     *   superclass matches the given superclass, or if my superclass
     *   inherits from the given superclass.  
     */
    for (CTPNSuperclass *entry = sc_name_head_ ; entry != 0 ;
         entry = entry->nxt_)
    {
        /* look up this symbol */
        CTcSymObj *entry_sym = (CTcSymObj *)G_prs->get_global_symtab()->find(
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
 *   Add a deleted property entry 
 */
void CTcSymObjBase::add_del_prop_to_list(CTcObjPropDel **list_head,
                                         CTcSymProp *prop_sym)
{
    /* create the new entry */
    CTcObjPropDel *entry = new CTcObjPropDel(prop_sym);

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
 *   Add a word to my vocabulary 
 */
void CTcSymObjBase::add_vocab_word(const char *txt, size_t len,
                                   tctarg_prop_id_t prop)
{
    /* create a new vocabulary entry */
    CTcVocabEntry *entry = new (G_prsmem) CTcVocabEntry(txt, len, prop);

    /* link it into my list */
    entry->nxt_ = vocab_;
    vocab_ = entry;
}

/*
 *   Delete a vocabulary property from my list (for 'replace') 
 */
void CTcSymObjBase::delete_vocab_prop(tctarg_prop_id_t prop)
{
    /* scan my list and delete each word defined for the given property */
    for (CTcVocabEntry *prv = 0, *entry = vocab_, *nxt = 0 ;
         entry != 0 ; entry = nxt)
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
 *   Add my words to the dictionary, associating the words with the given
 *   object.  This can be used to add my own words to the dictionary or to
 *   add my words to a subclass's dictionary.  
 */
void CTcSymObjBase::inherit_vocab()
{
    /* 
     *   if I've already inherited my superclass vocabulary, there's
     *   nothing more we need to do 
     */
    if (vocab_inherited_)
        return;

    /* make a note that I've inherited my superclass vocabulary */
    vocab_inherited_ = TRUE;

    /* inherit words from each superclass */
    for (CTcObjScEntry *sc = sc_ ; sc != 0 ; sc = sc->nxt_)
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
    /* add each of my words to the subclass */
    for (CTcVocabEntry *entry = vocab_ ; entry != 0 ; entry = entry->nxt_)
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

/* ------------------------------------------------------------------------ */
/*
 *   metaclass symbol   
 */

/*
 *   add a property 
 */
void CTcSymMetaclassBase::add_prop(
    const char *txt, size_t len, const char *obj_fname, int is_static)
{
    /* see if this property is already defined */
    CTcSymProp *prop_sym =
        (CTcSymProp *)G_prs->get_global_symtab()->find(txt, len);
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
    /* create a new list entry for the property */
    CTcSymMetaProp *entry = new (G_prsmem) CTcSymMetaProp(prop, is_static);
    
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
 *   get the nth property in our table
 */
CTcSymMetaProp *CTcSymMetaclassBase::get_nth_prop(int n) const
{
    /* traverse the list to the desired index */
    CTcSymMetaProp *prop;
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
    /* set up the property pointer constant */
    CTcConstVal cval;
    cval.set_prop(get_prop());

    /* return a constant node */
    return new CTPNConst(&cval);
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
    /* set up the enumerator constant */
    CTcConstVal cval;
    cval.set_enum(get_enum_id());

    /* return a constant node */
    return new CTPNConst(&cval);
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
    /* search my list for an existing association to the same obj/prop */
    CTcPrsDictItem *item;
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
 *   Code Body Parse Node 
 */

/*
 *   instantiate 
 */
CTPNCodeBodyBase::CTPNCodeBodyBase(
    CTcPrsSymtab *lcltab, CTcPrsSymtab *gototab, CTPNStm *stm,
    int argc, int opt_argc, int varargs,
    int varargs_list, CTcSymLocal *varargs_list_local, int local_cnt,
    int self_valid, CTcCodeBodyRef *enclosing_code_body)
{
    /* remember the data in the code body */
    lcltab_ = lcltab;
    gototab_ = gototab;
    stm_ = stm;
    argc_ = argc;
    opt_argc_ = opt_argc;
    varargs_ = varargs;
    varargs_list_ = varargs_list;
    varargs_list_local_ = varargs_list_local;
    local_cnt_ = local_cnt;
    self_valid_ = self_valid;
    self_referenced_ = FALSE;
    full_method_ctx_referenced_ = FALSE;
    op_overload_ = FALSE;
    is_anon_method_ = FALSE;
    is_dyn_func_ = FALSE;
    is_dyn_method_ = FALSE;

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

    /* leave it up to the caller to set the starting location */
    start_desc_ = 0;
    start_linenum_ = 0;

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
    /* scan our list to see if the level is already assigned */
    CTcCodeBodyCtx *ctx;
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
    /* if they want level zero, it's our local context */
    if (level == 0)
    {
        /* set the variable ID to our local context variable */
        *varnum = local_ctx_var_;

        /* return true only if we actually have a local context */
        return has_local_ctx_;
    }

    /* scan our list to see if the level is already assigned */
    for (CTcCodeBodyCtx *ctx = ctx_head_ ; ctx != 0 ; ctx = ctx->nxt_)
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
 *   Anonymous function 
 */

/*
 *   mark as replaced/obsolete 
 */
void CTPNAnonFuncBase::set_replaced(int flag)
{
    if (code_body_ != 0)
        code_body_->set_replaced(flag);
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
    CTcSymObj *sym = (CTcSymObj *)get_sym();
    if (sym == 0
        || sym->get_type() != TC_SYM_OBJ
        || sym->get_metaclass() != TC_META_TADSOBJ)
        return FALSE;

    /* scan our symbol's superclass list for a match */
    for (CTPNSuperclass *sc = sym->get_sc_name_head() ;
         sc != 0 ; sc = sc->nxt_)
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
