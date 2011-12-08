#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCPRSSTM.CPP,v 1.1 1999/07/11 00:46:54 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2010 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcprs.cpp - TADS 3 Compiler Parser - statement node classes
Function
  This parser module includes statement node classes that are needed for
  parsing executable code bodies - the insides of functions and methods.
  This doesn't include the outer program-level structures, such as object
  definitions, or the inner expression structures.

  This code is needed for interpreters that include "eval()" functionality,
  for dynamic construction of new executable code.
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

    /* we haven't yet referenced 'self' or any other method context yet */
    self_referenced_ = FALSE;
    full_method_ctx_referenced_ = FALSE;

    /* we don't yet need the method context in the local context */
    local_ctx_needs_self_ = FALSE;
    local_ctx_needs_full_method_ctx_ = FALSE;
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

/* ------------------------------------------------------------------------ */
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
    CTcSymLocal *lcl = 0;
    CTcToken varname;
    int named_param = FALSE, opt_param = FALSE;
    int is_typed = FALSE;
    int defval_cnt = 0;

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
        tc_toktyp_t suffix_tok = TOKT_INVALID;
        CTcPrsNode *defval_expr = 0;

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
            is_typed = FALSE;
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

                    /* flag it as typed */
                    is_typed = TRUE;
                    
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

                    /*
                     *   We need to add an untyped element to the type list,
                     *   but don't do so quite yet - we need to make sure
                     *   that this isn't a named argument parameter.  Only
                     *   positional elements go in the type list.  For now,
                     *   simply flag it as non-typed.  
                     */
                    is_typed = FALSE;
                }
            }

            /* remember and skip the symbol name */
            varname = *G_tok->getcur();
            G_tok->next();

            /* check for a colon, which might indicate a named parameter */
            named_param = FALSE;
            if (G_tok->cur() == TOKT_COLON)
            {
                /* it looks like a named param so far */
                named_param = TRUE;

                /*
                 *   There's one case where a colon *doesn't* indicate a
                 *   named parameter, which is when it's the closing colon of
                 *   a short-form anonymous function parameter list.  For
                 *   those, the colon is a name marker only if it's followed
                 *   by a continuation of the formal list: specifically,
                 *   another parameter (so, a comma), or ':' to indicate the
                 *   actual end of the list.  If anything else follows, the
                 *   colon is the terminator rather than part of the formal
                 *   list.  
                 */
                if (for_short_anon_func)
                {
                    /* check the next token */
                    switch (G_tok->next())
                    {
                    case TOKT_COMMA:
                    case TOKT_COLON:
                        /* 
                         *   More list follows, so this is indeed a named
                         *   parameter marker. 
                         */
                        break;
                        
                    default:
                        /* 
                         *   We don't have more list, so this colon is the
                         *   end marker for the formal list.  Put it back and
                         *   continue.  
                         */
                        named_param = FALSE;
                        G_tok->unget();
                        break;
                    }
                }
                else
                {
                    /* 
                     *   for regular argument lists, this can only be a named
                     *   parameter indicator; skip it
                     */
                    G_tok->next();
                }

                /* named parameters can't be typed */
                if (named_param && is_typed)
                {
                    G_tok->log_error(TCERR_NAMED_ARG_NO_TYPE,
                                     (int)varname.get_text_len(),
                                     varname.get_text());
                }
            }
            
            /* 
             *   check for an optionality flag (a '?' suffix) or a default
             *   value ('= <expression>') 
             */
            opt_param = FALSE;
            suffix_tok = G_tok->cur();
            defval_expr = 0;
            if (opt_allowed
                && (suffix_tok == TOKT_QUESTION || suffix_tok == TOKT_EQ))
            {
                /* it's optional */
                opt_param = TRUE;
                
                /* count the optional argument */
                if (opt_argc != 0 && !named_param)
                    ++(*opt_argc);

                /* skip the suffix token */
                G_tok->next();
                
                /* if it was an '=', parse the default value expression */
                if (suffix_tok == TOKT_EQ)
                {
                    /* 
                     *   parse the expression - comma has precedence in this
                     *   context as an argument separator, so we need to
                     *   parse this as an assignment expression (this is for
                     *   precedence reasons, NOT because it's syntactically
                     *   like an assignment - the '=' postfix to the argument
                     *   name isn't part of the expression we're parsing) 
                     */
                    defval_expr = parse_asi_expr();
                }
            }

            /* 
             *   if any previous arguments are optional, all subsequent
             *   arguments must be optional as well 
             */
            if (opt_allowed
                && opt_argc != 0 && *opt_argc != 0
                && !opt_param && !named_param)
            {
                /* flag it as an error */
                G_tok->log_error(TCERR_ARG_MUST_BE_OPT,
                                 (int)varname.get_text_len(),
                                 varname.get_text());
            }
            
            /* if we're creating symbol table entries, add this symbol */
            lcl = 0;
            if (!count_only)
            {
                /* 
                 *   create a new local symbol table if we don't already have
                 *   one 
                 */
                create_scope_local_symtab();

                /*
                 *   Insert the new variable for the formal.  If it's a named
                 *   parameter or an optional parameter, it's not a real
                 *   formal, since it doesn't point directly to a parameter
                 *   slot in the stack frame - it points instead to a regular
                 *   local variable slot that we have to set up with
                 *   generated code in the function prolog.  
                 */
                if (named_param || opt_param)
                {
                    /* it's a pseudo-formal - create a local for it */
                    lcl = local_symtab_->add_local(
                        varname.get_text(), varname.get_text_len(),
                        alloc_local(), FALSE, FALSE, FALSE);

                    /* set the special flags */
                    lcl->set_named_param(named_param);
                    lcl->set_opt_param(opt_param);

                    /* remember the default value expression, if any */
                    if (defval_expr != 0)
                        lcl->set_defval_expr(defval_expr, defval_cnt++);
                }
                else
                {
                    /* insert the new local variable for the formal */
                    lcl = local_symtab_->add_formal(
                        varname.get_text(), varname.get_text_len(),
                        base_formal_num + *argc, FALSE);
                }

                /* 
                 *   If it's a position parameter, set its parameter index.
                 *   Since optional parameters are actually stored as local
                 *   variables, we need to separately track the parameter
                 *   list index so that we can load the value into the local
                 *   on function entry.  
                 */
                if (!named_param && lcl != 0)
                    lcl->set_param_index(*argc);
            }

            /* 
             *   If it's not a named parameter, count it.  Named parameters
             *   don't count as formals since they're not part of the
             *   positional list and aren't in the parameter area of the
             *   stack frame. 
             */
            if (!named_param)
            {
                /* count it in the positional list */
                ++(*argc);

                /* if it was untyped, add it to the type list */
                if (type_list != 0 && *type_list != 0 && !is_typed)
                    (*type_list)->add_untyped_param();
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

    /* deduct the optional arguments from the fixed arguments */
    if (opt_argc != 0)
        *argc -= *opt_argc;
}


/* ------------------------------------------------------------------------ */
/*
 *   Parse a nested code body (such as an anonymous function code body) 
 */
CTPNCodeBody *CTcParser::
   parse_nested_code_body(int eq_before_brace,
                          int self_valid,
                          int *p_argc, int *p_opt_argc, int *p_varargs,
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
                                p_argc, p_opt_argc, p_varargs,
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


/* ------------------------------------------------------------------------ */
/*
 *   Parse a function or method body
 *   
 *   op_args is non-zero if this is an operator overload property.  This
 *   encodes the possible usages of the operator: bit 0x01 is set if this can
 *   be a unary operator, bit 0x02 if it can be binary, and bit 0x4 is set if
 *   it can be trinary.  Most operators have only one bit set, but some have
 *   multiple uses, such as "-" which can be unary (-3) or binary (5-2).  The
 *   number of arguments is always one less than the number of operands,
 *   because 'self' is always the primary operand (the sole operand of a
 *   unary, the left operand of a binary, or the leftmost operand of a
 *   trinary).  
 */
CTPNCodeBody *CTcParser::parse_code_body(int eq_before_brace, int is_obj_prop,
                                         int self_valid,
                                         int *p_argc, int *p_opt_argc,
                                         int *p_varargs, int *p_varargs_list,
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
    int opt_formal_num;
    int varargs;
    int varargs_list;
    CTcSymLocal *varargs_list_local;
    CTPNStmComp *stm;
    unsigned long flow_flags;
    CTPNCodeBody *body_stm;
    int parsing_anon_fn;
    long start_line = 0;
    CTcTokFileDesc *start_desc = 0;

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
    opt_formal_num = 0;
    varargs = FALSE;
    varargs_list = FALSE;
    varargs_list_local = 0;

    /* check for a short anonymous function, which uses unusual syntax */
    if (cb_type == TCPRS_CB_SHORT_ANON_FN)
    {
        CTcPrsNode *expr;
        CTPNStm *ret_stm;

        /* we're at the opening brace now */
        G_tok->get_last_pos(&start_desc, &start_line);
        
        /* 
         *   a short-form anonymous function always has an argument list,
         *   but it uses special notation: the argument list is simply the
         *   first thing after the function's open brace, and ends with a
         *   colon 
         */
        parse_formal_list(FALSE, TRUE, &formal_num, &opt_formal_num,
                          &varargs, &varargs_list, &varargs_list_local,
                          err, 0, TRUE, 0);
        if (*err)
            return 0;

        /*
         *   The contents of a short-form anonymous function are simply an
         *   expression, whose value is implicitly returned by the function.
         *   Alternatively, it can start with a list of "local" clauses, to
         *   define local variables.  
         */
        expr = 0;
        while (G_tok->cur() == TOKT_LOCAL)
        {
            /* we need a variable name symbol */
            if (G_tok->next() == TOKT_SYM)
            {
                /* add the symbol */
                CTcSymLocal *lcl = local_symtab_->add_local(alloc_local());

                /* check for an initializer */
                CTcPrsNode *subexpr = 0;
                if (G_tok->next() == TOKT_ASI || G_tok->cur() == TOKT_EQ)
                {
                    /* parse the initializer */
                    subexpr = parse_local_initializer(lcl, err);

                    /* combine it with the expression we had so far */
                    expr = (expr == 0 ? subexpr :
                            new CTPNComma(expr, subexpr));
                }

                /* if there's a comma, skip it */
                if (G_tok->cur() == TOKT_COMMA)
                    G_tok->next();
            }
            else
            {
                /* invalid 'local' syntax */
                G_tok->log_error_curtok(TCERR_LOCAL_REQ_SYM);
            }
        }

        /* if there's anything left, parse the simple expression */
        if (expr == 0 || G_tok->cur() != TOKT_RBRACE)
        {
            /* parse the rest of the expression */
            CTcPrsNode *subexpr = parse_expr_or_dstr(TRUE);

            /* combine it with the local initializers, if any */
            expr = (expr == 0 ? subexpr : new CTPNComma(expr, subexpr));
        }

        /*
         *   The next token must be the closing brace ('}') of the function.
         *   If the next token is a semicolon, it's an error, but it's
         *   probably just a superfluous semicolon that we can ignore.  
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
        /*
         *   If we have a propertyset stack, set up an inserted token stream
         *   with the expanded token list for the formals, combining the
         *   formals from the enclosing propertyset definitions with the
         *   formals defined here.  
         */
        if (propset_depth != 0)
            insert_propset_expansion(propset_stack, propset_depth);

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
            parse_formal_list(FALSE, TRUE, &formal_num, &opt_formal_num,
                              &varargs, &varargs_list, &varargs_list_local,
                              err, 0, FALSE, type_list);
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
            /* note the location of the opening brace */
            G_tok->get_last_pos(&start_desc, &start_line);

            /* parse the compound statement */
            stm = parse_compound(err, TRUE, TRUE, 0, TRUE);
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
    if (p_opt_argc != 0)
        *p_opt_argc = opt_formal_num;
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
                                formal_num, opt_formal_num, varargs,
                                varargs_list, varargs_list_local,
                                max_local_cnt_, self_valid,
                                enclosing_code_body);

    /* store this new statement in the current code body reference object */
    cur_code_body_->ptr = body_stm;

    /* save the starting location */
    body_stm->set_start_location(start_desc, start_line);

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
CTPNStmComp *CTcParser::parse_compound(int *err,
                                       int skip_lbrace, int need_rbrace,
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

        case TOKT_EOF:
            /* 
             *   if we're at end of file, and we don't need a right brace to
             *   end the block, consider this the end of the block 
             */
            if (!need_rbrace)
            {
                done = TRUE;
                cur_stm = 0;
                skip_rbrace = FALSE;
                break;
            }
            else
            {
                /* it's an error */
                G_tok->log_error(TCERR_EOF_IN_CODE);
                cur_stm = 0;
                done = TRUE;
                break;
            }
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
        return parse_compound(err, TRUE, TRUE,
                              0, compound_use_enclosing_scope);
        
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
    /* save the current line information for later */
    CTcTokFileDesc *file;
    long linenum;
    G_tok->get_last_pos(&file, &linenum);

    /* 
     *   enter a scope, in case we create a local symbol table for local
     *   variables defined within the 'for' statement 
     */
    tcprs_scope_t scope_data;
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
    CTcPrsNode *init_expr = 0;
    CTcPrsNode *cond_expr = 0;
    CTcPrsNode *reinit_expr = 0;
    int found_in = FALSE;

    /* "in" expression list */
    CTPNForIn *in_head = 0, *in_tail = 0;

    /* parse the initializer list */
    for (int done = FALSE ; !done ; )
    {
        /* presume we won't find an expression on this round */
        CTcPrsNode *expr = 0;
        
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
                /* add the local symbol */
                CTcSymLocal *lcl = local_symtab_->add_local(alloc_local());

                /* check for the required initializer */
                switch(G_tok->next())
                {
                case TOKT_ASI:
                case TOKT_EQ:
                    /* parse the initializer */
                    expr = parse_local_initializer(lcl, err);
                    break;

                case TOKT_SYM:
                    /* check for an 'in' expression */
                    if (G_tok->getcur()->text_matches("in", 2))
                    {
                        /* parse the 'var in expr' or 'var in from..to' */
                        G_tok->next();
                        expr = parse_for_in_clause(
                            new CTPNSymResolved(lcl), in_head, in_tail);

                        /* note the "in" */
                        found_in = TRUE;

                        /* if that failed, stop scanning the initializer */
                        if (expr == 0)
                            done = TRUE;
                    }
                    else
                    {
                        /* anything else is an error */
                        goto req_asi;
                    }
                    break;
                    
                default:
                req_asi:
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
            /* if we found an "in", we can end early */
            if (found_in)
            {
                done = TRUE;
                break;
            }

            /* otherwise, fall through to the missing part error... */
            /* FALL THROUGH */

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

            /* check for an 'in' expression */
            if (G_tok->cur() == TOKT_SYM
                && G_tok->getcur()->text_matches("in", 2))
            {
                /* skip the "in" and parse the collection/range */
                G_tok->next();
                expr = parse_for_in_clause(expr, in_head, in_tail);

                /* note the "in" */
                found_in = TRUE;
                
                /* if that failed, stop parsing the initializer */
                if (expr == 0)
                    done = TRUE;
            }

            /* done with this clause */
            break;
        }

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

        /* if we're done, we can stop now */
        if (done)
            break;

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
            /* if we found an "in" clause, we only need an initializer */
            if (found_in)
            {
                done = TRUE;
                break;
            }

            /* otherwise, fall through to the error case... */
            /* FALL THROUGH */
            
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
         *   We already found the right paren - early, so we logged an error
         *   if necessary (it's an error in most cases, but *not* if there's
         *   an 'in' clause in the initializer list).  Simply skip it now so
         *   that we can proceed to the body of the 'for'.  
         */
        G_tok->next();
    }

    /* create the "for" node */
    CTPNStmFor *for_stm = new CTPNStmFor(
        init_expr, cond_expr, reinit_expr, in_head,
        local_symtab_, enclosing_stm_);

    /* set the 'for' to enclose its body */
    CTPNStmEnclosing *old_enclosing = set_enclosing_stm(for_stm);

    /* parse the body of the "for" loop */
    CTPNStm *body_stm = parse_stm(err, 0, FALSE);

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
 *   Parse a for..in clause.  This parses the part after the "in"; we accept
 *   either a simple expression, or a range (expr .. expr).  We return the
 *   expression node representing the clause.  It's up to the caller to look
 *   up the local variable before the "in" and pass it to us.
 */
CTcPrsNode *CTcParser::parse_for_in_clause(
    CTcPrsNode *lval, CTPNForIn *&head, CTPNForIn *&tail)
{
    /* 
     *   Parse an assignment expression.  We parse from this point in the
     *   grammar because the entire 'in' clause can be part of a comma
     *   expression, so we want to stop if we reach a comma rather than
     *   treating the comma as part of our own expression.  
     */
    CTcPrsNode *expr = parse_asi_expr();

    /* if that failed, return failure */
    if (expr == 0)
        return 0;

    /* if the next token is "..", we have a "for var in from..to" range */
    CTPNForIn *in_expr;
    if (G_tok->cur() == TOKT_DOTDOT)
    {
        /* skip the ".." and parse the "to" expression */
        G_tok->next();
        CTcPrsNode *toExpr = parse_asi_expr();

        /* if that failed, return failure */
        if (toExpr == 0)
            return 0;

        /* check for the optional "step" expression */
        CTcPrsNode *stepExpr = 0;
        if (G_tok->cur() == TOKT_SYM
            && G_tok->getcur()->text_matches("step", 4))
        {
            /* skip the "step" and parse the expression */
            G_tok->next();
            stepExpr = parse_asi_expr();
        }

        /* 
         *   allocate variables for the "to" and "step" expressions, but only
         *   if they're non-constant 
         */
        int to_local = (toExpr->is_const() ? -1 : alloc_local());
        int step_local = (stepExpr == 0 || stepExpr->is_const() ? -1 :
                          alloc_local());

        /* build the "for x in range" node */
        in_expr = new CTPNVarInRange(lval, expr, toExpr, stepExpr,
                                     to_local, step_local);
    }
    else
    {
        /* it's a plain old "for x in collection" */
        in_expr = new CTPNVarIn(lval, expr, alloc_local());
    }

    /* link it into the list */
    if (tail != 0)
        tail->setnxt(in_expr);
    else
        head = in_expr;
    tail = in_expr;

    /* return the new expression */
    return in_expr;
}

/*
 *   'for var in expr' - fold constants 
 */
CTcPrsNode *CTPNVarInBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* fold each element */
    lval_ = lval_->fold_constants(symtab);
    expr_ = expr_->fold_constants(symtab);

    /* we can't fold the overall 'in' expression itself */
    return this;
}

/*
 *   'for var in from .. to' - fold constants 
 */
CTcPrsNode *CTPNVarInRangeBase::fold_constants(CTcPrsSymtab *symtab)
{
    /* fold each element */
    lval_ = lval_->fold_constants(symtab);
    from_expr_ = from_expr_->fold_constants(symtab);
    to_expr_ = to_expr_->fold_constants(symtab);
    if (step_expr_ != 0)
        step_expr_ = step_expr_->fold_constants(symtab);

    /* we can't fold the overall 'in' expression itself */
    return this;
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
    body_stm = parse_compound(err, FALSE, TRUE, switch_stm, FALSE);

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
void CTcPrsOpUnary::enum_for_anon2(void *ctx0, CTcSymbol *sym)
{
    enum_for_anon_ctx *ctx = (enum_for_anon_ctx *)ctx0;

    /*
     *   If this symbol isn't in the anonymous function's local symbol table,
     *   the anonymous function didn't end up using it.  The fact that it's
     *   in the enclosing table in this case simply means that it's a context
     *   variable in the enclosing context.  
     */
    if (ctx->symtab->find_direct(sym->get_sym(), sym->get_sym_len()) == 0)
        return;
    
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
    int is_method;

    /* 
     *   our code body type can be an anonymous function, a short anonymous
     *   function, or an anonymous method 
     */
    cb_type = (short_form ? TCPRS_CB_SHORT_ANON_FN :
               G_tok->cur() == TOKT_METHOD ? TCPRS_CB_ANON_METHOD :
               TCPRS_CB_ANON_FN);

    /* note if it's a method */
    is_method = (cb_type == TCPRS_CB_ANON_METHOD);

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
     *   Parse the code body.
     *   
     *   If it's a function (not a method), it shares the method context from
     *   the lexically enclosing scope, so it has access to 'self' and other
     *   method context variables if and only if the enclosing scope does.
     *   If it's a method, it has access to the live method context, so the
     *   enclosing scope's status is irrelevant.  
     */
    err = 0;
    code_body = G_prs->parse_nested_code_body(
        FALSE, is_method ||  G_prs->is_self_valid(),
        0, 0, 0, 0, 0, &has_retval, &err, inner_lcl_symtab, cb_type);

    /* if that failed, return failure */
    if (code_body == 0 || err != 0)
        return 0;

    /* 
     *   If this is an anonymous function (not an anonymous method), and the
     *   nested code body references 'self' or the full method context, then
     *   so does the enclosing code body, and we need these variables in the
     *   local context for the topmost enclosing code body.
     *   
     *   If this is an anonymous method, references to the method context are
     *   to the live frame at the time of invocation, not to the saved
     *   lexical frame at the time of creation.  So references from within
     *   the method's code body don't create references on the enclosing
     *   scope, and they don't require any shared context information.  
     */
    if (!is_method)
    {
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
    }
    else
    {
        /* mark it as a method */
        code_body->set_anon_method(TRUE);
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
     *   if this is a method, and there's a 'self' object, and we referenced
     *   'self' or the full method context in the nested code body, we'll
     *   definitely need a local context, so make sure we have one
     *   initialized even if we don't have any local variables shared 
     */
    if (!is_method
        && G_prs->is_self_valid()
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
    return new CTPNAnonFunc(code_body, has_retval, is_method);
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
 *   adjust for dynamic (run-time) compilation 
 */
CTcPrsNode *CTPNStmCompBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
{
    /* adjust each statement in our list */
    for (CTPNStm *cur = first_stm_ ; cur != 0 ; cur = cur->get_next_stm())
    {
        /* 
         *   adjust this statement - we assume that statements are never
         *   replaced by this operation, so we just keep our existing list 
         */
        cur->adjust_for_dyn(info);
    }

    /* return myself */
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
     *   either a reinitialization expression or a loop body, warn about the
     *   unreachable code.  If the body is directly reachable via a code
     *   label, we don't need this warning.
     */
    if (cond_expr_ != 0
        && cond_expr_->is_const()
        && !cond_expr_->get_const_val()->get_val_bool()
        && (reinit_expr_ != 0 || body_stm_ != 0)
        && (body_stm_ != 0 && !body_stm_->has_code_label())
        && !G_prs->get_syntax_only())
    {
        /* log a warning if desired */
        if (warn && !cond_expr_->get_const_val()->is_ctc())
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
     *   If we have a condition, and either the condition is a constant
     *   false value or has a non-constant value, assume that the
     *   condition can possibly become false and hence that the loop can
     *   exit, and hence that the next statement is reachable.
     */
    if (cond_expr_ != 0
        && (!cond_expr_->is_const()
            || (cond_expr_->is_const()
                && !cond_expr_->get_const_val()->get_val_bool())))
        flags |= TCPRS_FLOW_NEXT;

    /* 
     *   If we have any "in" clauses, we implicitly have a condition that
     *   allows the loop to terminate. 
     */
    if (in_exprs_ != 0)
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
        if (warn && !cond_expr_->get_const_val()->is_ctc())
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
 *   adjust for dynamic (run-time) compilation 
 */
CTcPrsNode *CTPNStmTryBase::adjust_for_dyn(const tcpn_dyncomp_info *info)
{
    /* adjust our body statement */
    if (body_stm_ != 0)
        body_stm_ = (CTPNStm *)body_stm_->adjust_for_dyn(info);

    /* adjust each item in our catch list */
    for (CTPNStmCatch *cur_catch = first_catch_stm_ ; cur_catch != 0 ;
         cur_catch = cur_catch->get_next_catch())
    {
        /* adjust this item; assume it won't replace the node */
        cur_catch->adjust_for_dyn(info);
    }

    /* adjust our 'finally' clause */
    if (finally_stm_ != 0)
        finally_stm_ = (CTPNStmFinally *)finally_stm_->adjust_for_dyn(info);

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
 *   Grammar tree node subclasses 
 */

/*
 *   Grammar tree node - base class for nodes with children (OR, cat)
 */
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
        /* create a new 'cat' node */
        CTcPrsGramNodeCat *new_cat = new (G_prsmem) CTcPrsGramNodeCat();

        /* add a clone of each of my children and add it to my replica */
        for (CTcPrsGramNode *cur = sub_head_ ; cur != 0 ; cur = cur->next_)
            new_cat->add_sub_item(cur->clone_expansion());

        /* return my replica */
        return new_cat;
    }

    /* clone this node (without any children) */
    virtual CTcPrsGramNodeWithChildren *clone_this_node()
    {
        return new (G_prsmem) CTcPrsGramNodeCat();
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


/*
 *   Parse a 'grammar' statement's alternative list. 
 */
void CTcParser::parse_gram_alts(int *err, CTcSymObj *gram_obj,
                                CTcGramProdEntry *prod,
                                CTcGramPropArrows *arrows,
                                CTcGramAltFuncs *funcs)
{
    /* set up the first alternative and add it to the production */
    CTcGramProdAlt *alt = new (G_prsmem) CTcGramProdAlt(gram_obj, dict_cur_);
    if (prod != 0)
        prod->add_alt(alt);

    /* flatten the grammar rules */
    CTcPrsGramNode *tree = flatten_gram_rule(err);
    if (tree == 0 || *err != 0)
        return;

    /* 
     *   install the tree as the nested token source, so that we read tokens
     *   from our expanded set of grammar rules rather than from the original
     *   input stream 
     */
    G_tok->set_external_source(tree);

    /* parse the token specification list */
    for (int done = FALSE ; !done ; )
    {
        CTcGramProdTok *tok;
        
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
                            return;

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
                        return;
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
        case TOKT_DSTR:
            /* 
             *   quoted string (single or double) - this gives a literal
             *   string to match 
             */
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
                    return;

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
                    /* it's undefined - presume it's a production */
                    CTcGramProdEntry *sub_prod = funcs->declare_gramprod(
                        G_tok->getcur()->get_text(),
                        G_tok->getcur()->get_text_len());

                    /* if the symbol if still undefined it's an error */
                    if (sub_prod == 0)
                        return;

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
                    /* get the enum symbol properly cast */
                    CTcSymEnum *enumsym = (CTcSymEnum *)sym;

                    /* enforce the 'enum token' rule if applicable */
                    funcs->check_enum_tok(enumsym);

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
                    sym = funcs->look_up_prop(G_tok->getcur(), FALSE);
                    
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
                        for (i = 0 ; i < arrows->cnt ; ++i)
                        {
                            /* if we found it, stop looking */
                            if (arrows->prop[i] == propsym)
                                break;
                        }

                        /* 
                         *   if we didn't find it, and we have room for
                         *   another, add it 
                         */
                        if (i == arrows->cnt
                            && arrows->cnt < arrows->max_arrows)
                        {
                            /* it's not there - add it */
                            arrows->prop[arrows->cnt++] = propsym;
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
            /* let the compiler interface decide whether it's an error */
            funcs->on_eof(err);
            if (*err)
                return;

            /* in any case, it's the end of the object */
            done = TRUE;
            break;

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
}

/*
 *   Flatten a set of grammar rules.  Each time we find a parenthesized
 *   alternator, we'll expand the alternatives, until we have no
 *   parenthesized alternators. 
 */
CTcPrsGramNode *CTcParser::flatten_gram_rule(int *err)
{
    /* first, build the top-level 'or' tree */
    CTcPrsGramNode *tree = parse_gram_or(err, 0);
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

/* ------------------------------------------------------------------------ */
/*
 *   Grammar production token object 
 */

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

