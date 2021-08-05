/* Copyright (c) 2009 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tcjs.cpp - tads 3 compiler - javascript code generator
Function
  
Notes
  
Modified
  02/17/09 MJRoberts  - Creation
*/

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

#include "t3std.h"
#include "os.h"
#include "tcprs.h"
#include "tcjs.h"
#include "tcgen.h"
#include "tcmain.h"
#include "tcerr.h"


/* ------------------------------------------------------------------------ */
/*
 *   JS stack element 
 */

js_expr_ele::js_expr_ele(js_expr_buf &expr, js_expr_ele *tos)
{
    /* assume ownership of the expression's buffer */
    assume_buffer(expr.yield_buffer(), expr.getlen());

    /* remember the old top-of-stack as our next element */
    nxt = tos;
}

/* ------------------------------------------------------------------------ */
/*
 *   Javascript code generator 
 */
   
CTcGenTarg::CTcGenTarg()
{
    /* start at the first valid property and object ID */
    next_prop_ = 1;
    next_obj_ = 1;

    /* no expression stack yet */
    expr_stack = 0;
    expr_stack_depth = 0;
}

CTcGenTarg::~CTcGenTarg()
{
    /* clear the expression stack */
    while (expr_stack != 0)
        pop_js_expr();
}

/*
 *   push a computed expression onto the value stack, replacing operands
 *   taken from the stack 
 */
void CTcGenTarg::js_expr(const char *str, ...)
{
    char tpl[128];
    js_expr_buf expr;
    char *src;
    size_t rem;
    int stk_cnt = 0;
    int n, m;

    /* first, expand the printf-style formatting to form the template */
    va_list args;
    va_start(args, str);
    vsnprintf(tpl, sizeof(tpl), str, args);
    va_end(args);

    /* now replace $n stack elements in the template to make the expression */
    for (src = tpl ; *src != '\0' ; )
    {
        /* check what we have */
        switch (*src)
        {
        case '$':
            /* it's a substitution string - check the next character */
            switch (*++src)
            {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                /* number - read and skip it */
                for (n = atoi(src++) ; isdigit(*src) ; ++src) ;

                /* note the new high-water mark, if applicable */
                if (n > stk_cnt)
                    stk_cnt = n;

                /* add stack element 'n' */
                copy_js_expr(expr, n);
                
                /* done */
                break;

            case '{':
                /* check what follows */
                ++src;
                if ((memcmp(src, "args ", 5) == 0 && isdigit(*(src+5))
                     || (memcmp(src, ",args ", 6) == 0 && isdigit(*src+6))
                     || (memcmp(src, "args[] ", 7) == 0 && isdigit(*src+6))))
                {
                    /* note if it's the comma form */
                    int comma = (*src == ',');
                    if (comma)
                        ++src;

                    /* note if it's the array form */
                    int array = *src == '[' && *(src+1) == ']';
                    if (array)
                        src += 2;

                    /* get and skip the stack index value */
                    for (src += 5, n = atoi(src++) ; isdigit(*src) ; ++src) ;

                    /* skip the space */
                    if (*src++ != ' ')
                        G_tok->throw_internal_error(TCERR_BAD_JS_TPL);

                    /* get and skip the arg count value */
                    for (m = atoi(src) ; isdigit(*src) ; ++src) ;

                    /* for the array form, get and skip the varargs flag */
                    int varargs = FALSE;
                    if (array)
                    {
                        /* skip the space */
                        if (*src++ != ' ')
                            G_tok->throw_internal_error(TCERR_BAD_JS_TPL);

                        /* get and skip the varargs flag */
                        for (varargs = atoi(src) ; isdigit(*src) ; ++src) ;
                    }

                    /* add a leading comma if desired and applicable */
                    if (comma && m != 0)
                        expr += ',';

                    /* start the varargs list if applicable */
                    if (array)
                    {
                        /* check for varargs */
                        if (varargs)
                        {
                            /* 
                             *   it's varargs, so the argument list was set
                             *   up as an array to start with - all we need
                             *   to do is add the single argument list
                             *   expression from the stack 
                             */
                            m = 1;
                        }
                        else
                        {
                            /* 
                             *   it's not varargs, so the argument list
                             *   consists of the 'm' expression on the stack
                             *   - add an open bracket to make the list we
                             *   generate into an array 
                             */
                            expr += '[';
                        }
                    }

                    /* copy each expression */
                    for ( ; m != 0 ; ++n, --m)
                    {
                        /* copy this element */
                        copy_js_expr(expr, n);

                        /* if this isn't the last element, add a separator */
                        if (m > 1 && rem != 0)
                            expr += ',';
                    }

                    /* add the ']' at the end of the array, if applicable */
                    if (array && !varargs)
                        expr += ']';

                    /* mark the last element as used */
                    --n;
                    if (n > stk_cnt)
                        stk_cnt = n;
                }
                else if (memcmp(src, "disc ", 5) == 0 && isdigit(*(src+5)))
                {
                    /* get and skip the 'n' value */
                    src += 5;
                    n = (*src++) - '0';

                    /* mark this argument for discarding */
                    if (n > stk_cnt)
                        stk_cnt = n;
                }
                else if (isdigit(*src))
                {
                    /* it's a number in braces - read it and skip it */
                    for (n = atoi(src) ; isdigit(*src) ; ++src) ;

                    /* add stack element 'n' */
                    copy_js_expr(expr, n);

                    /* note the new high-water mark, if applicable */
                    if (n > stk_cnt)
                        stk_cnt = n;
                }
                else
                {
                    /* looks like a syntax error in the template */
                    G_tok->throw_internal_error(TCERR_BAD_JS_TPL);
                }

                /* ensure that we have a '}' following */
                if (*src++ != '}')
                    G_tok->throw_internal_error(TCERR_BAD_JS_TPL);

                /* done with the ${...} expression */
                break;

            case '$':
            default:
                /* copy '$', or anything not otherwise special, literally */
                expr += *src++;
                break;
            }
            break;

        default:
            /* copy anything else literally */
            expr += *src++;
            break;
        }
    }

    /* pop all of the stack elements we used */
    pop_js_exprs(stk_cnt);

    /* push the new expression */
    push_js_expr(expr);
}

/*
 *   Push an expression onto the js stack
 */
void CTcGenTarg::push_js_expr(js_expr_buf &buf)
{
    /* allocate a new expression element and link it at the top of the stack */
    expr_stack = new js_expr_ele(buf, expr_stack);
    ++expr_stack_depth;
}

/*
 *   Pop a number of expressions from the js stack 
 */
void CTcGenTarg::pop_js_exprs(int n)
{
    while (n-- != 0 && expr_stack != 0)
    {
        /* unlink this item from the stack */
        js_expr_ele *ele = expr_stack;
        expr_stack = expr_stack->getnxt();
        --expr_stack_depth;

        /* delete this element */
        delete ele;
    }
}

/*
 *   Copy a js expression into the given buffer 
 */
void CTcGenTarg::copy_js_expr(js_expr_buf &dst, int n)
{
    js_expr_ele *ele;

    /* find the nth expression stack element (1 = top of stack) */
    for (ele = expr_stack ; ele != 0 && n > 1 ; --n, ele = ele->getnxt()) ;

    /* if we found it, copy it */
    if (n == 1)
        dst += ele;
}


/* ------------------------------------------------------------------------ */
/*
 *   Generic parse node 
 */


/*
 *   generate code for assignment to this node 
 */
int CTcPrsNode::gen_code_asi(int, tc_asitype_t, CTcPrsNode *,
                             int ignore_error)
{
    /* 
     *   if ignoring errors, the caller is trying to assign if possible
     *   but doesn't require it to be possible; simply return false to
     *   indicate that nothing happened if this is the case 
     */
    if (ignore_error)
        return FALSE;
    
    /* we should never get here - throw an internal error */
    G_tok->throw_internal_error(TCERR_GEN_BAD_LVALUE);
    AFTER_ERR_THROW(return FALSE;)
}

/*
 *   generate code for taking the address of this node 
 */
void CTcPrsNode::gen_code_addr()
{
    /* we should never get here - throw an internal error */
    G_tok->throw_internal_error(TCERR_GEN_BAD_ADDR);
}

/*
 *   Generate code to call the expression as a function or method.  
 */
void CTcPrsNode::gen_code_call(int discard, int argc, int varargs)
{
    /* function/method calls are never valid in speculative mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);
    
    /*
     *   For default nodes, assume that the result of evaluating the
     *   expression contained in the node is a method or function pointer.
     *   First, generate code to evaluate the expression, which should
     *   yield an appropriate pointer value. 
     */
    gen_code(FALSE, FALSE);

    /* if we have a varargs list, we have to use special syntax */
    if (varargs)
    {
        /* varargs - use the 'apply' syntax to pass an argument array  */
        G_cg->js_expr("(($1).apply(null, $2))");
    }
    else
    {
        /* fixed arguments - generate a regular call */
        G_cg->js_expr("(($1).call(null ${,args 2 %d})))", argc);
    }
}

/*
 *   Generate code for operator 'new' applied to this node
 */
void CTcPrsNode::gen_code_new(int, int, int, int, int)
{
    /* operator 'new' cannot be applied to a default node */
    G_tok->log_error(TCERR_INVAL_NEW_EXPR);
}

/*
 *   Generate code for a member evaluation 
 */
void CTcPrsNode::gen_code_member(int discard,
                                 CTcPrsNode *prop_expr, int prop_is_expr,
                                 int argc, int varargs)
{
    /* evaluate my own expression to yield the object value */
    gen_code(FALSE, FALSE);

    /* use the generic code to generate the rest */
    s_gen_member_rhs(discard, prop_expr, prop_is_expr, argc, varargs);
}

/*
 *   Generic code to generate the rest of a member expression after the
 *   left side of the '.' has been generated.  This can be used for cases
 *   where the left of the '.' is an arbitrary expression, and hence must
 *   be evaluated at run-time.
 */
void CTcPrsNode::s_gen_member_rhs(int discard,
                                  CTcPrsNode *prop_expr, int prop_is_expr,
                                  int argc, int varargs)
{
    /* we can't call methods with arguments in speculative mode */
    if (argc != 0 && G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* get or generate the property ID value */
    prop_expr->gen_code_propid(FALSE, prop_is_expr);

    /* generate the getprop */
    G_cg->js_expr("T3_getprop($1, $2, ${args[] 3 %d %d})", argc, varargs);
}

/*
 *   Generate code to get the property ID of the expression.
 */
vm_prop_id_t CTcPrsNode::gen_code_propid(int check_only, int is_expr)
{
    /* 
     *   simply evaluate the expression normally, anticipating that this
     *   will yield a property ID value at run-time 
     */
    if (!check_only)
        gen_code(FALSE, FALSE);

    /* tell the caller that there's no constant ID available */
    return VM_INVALID_PROP;
}

/* ------------------------------------------------------------------------ */
/*
 *   "self" 
 */

/*
 *   generate code 
 */
void CTPNSelf::gen_code(int discard, int)
{
    /* it's an error if we're not in a method context */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);
    
    /* "self" is the js "this" */
    if (!discard)
        G_cg->js_expr("this");
}

/*
 *   evaluate a property 
 */
void CTPNSelf::gen_code_member(int discard,
                               CTcPrsNode *prop_expr, int prop_is_expr,
                               int argc, int varargs)
{
    /* make sure "self" is available */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);

    /* don't allow arguments in speculative eval mode */
    if (argc != 0 && G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* we currently don't implement speculative evaluation at all */
    if (G_cg->is_speculative())
        assert(FALSE);

    /* generate the property value */
    prop_expr->gen_code_propid(FALSE, prop_is_expr);

    /* generate the getprop */
    G_cg->js_expr("T3_getprop(this, $1, ${args[] 2 %d %d})", argc, varargs);
}


/*
 *   generate code for an object before a '.' 
 */
vm_obj_id_t CTPNSelf::gen_code_obj_predot(int *is_self)
{
    /* make sure "self" is available */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);

    /* tell the caller that this is "self" */
    *is_self = TRUE;
    return VM_INVALID_OBJ;
}


/* ------------------------------------------------------------------------ */
/*
 *   "replaced" 
 */

/*
 *   evaluate 'replaced' on its own - this simply yields a function pointer
 *   to the modified base code 
 */
void CTPNReplaced::gen_code(int discard, int for_condition)
{
    /* get the modified base function symbol */
    CTcSymFunc *mod_base = G_cs->get_code_body()->get_replaced_func();

    /* make sure we're in a 'modify func()' context */
    if (mod_base == 0)
        G_tok->log_error(TCERR_REPLACED_NOT_AVAIL);

    /* this expression yields a pointer to the modified base function */
    G_cg->js_expr("T3F_%.*s", mod_base->getlen(), mod_base->getstr());
}

/*
 *   'replaced()' call - this invokes the modified base code 
 */
void CTPNReplaced::gen_code_call(int discard, int argc, int varargs)
{
    /* get the modified base function symbol */
    CTcSymFunc *mod_base = G_cs->get_code_body()->get_replaced_func();

    /* make sure we're in a 'modify func()' context */
    if (mod_base == 0)
        G_tok->log_error(TCERR_REPLACED_NOT_AVAIL);

    /* call it */
    if (varargs)
        G_cg->js_expr("T3F_%.*s.apply(null, $1)");
    else
        G_cg->js_expr("T3F_%.*s(${args 1 %d})", argc);

    /* make sure the argument count is correct */
    if (mod_base != 0
        && (mod_base->is_varargs() ? argc < mod_base->get_argc()
                                   : argc != mod_base->get_argc()))
        G_tok->log_error(TCERR_WRONG_ARGC_FOR_FUNC,
                         (int)mod_base->get_sym_len(), mod_base->get_sym(),
                         mod_base->get_argc(), argc);
}

/* ------------------------------------------------------------------------ */
/*
 *   "targetprop" 
 */

/*
 *   generate code 
 */
void CTPNTargetprop::gen_code(int discard, int)
{
    /* it's an error if we're not in a method context */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_TARGETPROP_NOT_AVAIL);

    /* if we're not discarding the result, push the target property ID */
    if (!discard)
        G_cg->js_expr("T3V_targetprop");
}

/* ------------------------------------------------------------------------ */
/*
 *   "targetobj" 
 */

/*
 *   generate code 
 */
void CTPNTargetobj::gen_code(int discard, int)
{
    /* it's an error if we're not in a method context */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_TARGETOBJ_NOT_AVAIL);

    /* if we're not discarding the result, push the target object ID */
    if (!discard)
        G_cg->js_expr("T3V_targetobj");
}

/* ------------------------------------------------------------------------ */
/*
 *   "definingobj" 
 */

/*
 *   generate code 
 */
void CTPNDefiningobj::gen_code(int discard, int)
{
    /* it's an error if we're not in a method context */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_DEFININGOBJ_NOT_AVAIL);

    /* if we're not discarding the result, push the defining object ID */
    if (!discard)
        G_cg->js_expr("T3V_defobj");
}


/* ------------------------------------------------------------------------ */
/*
 *   "inherited" 
 */
void CTPNInh::gen_code(int, int)
{
    /* 
     *   we should never be asked to generate an "inherited" node
     *   directly; these nodes should always be generated as part of
     *   member evaluation 
     */
    G_tok->throw_internal_error(TCERR_GEN_CODE_INH);
}

/*
 *   evaluate a property 
 */
void CTPNInh::gen_code_member(int discard,
                              CTcPrsNode *prop_expr, int prop_is_expr,
                              int argc, int varargs)
{
    /* 
     *   make sure "self" is available - we obviously can't inherit
     *   anything if we're not in an object's method 
     */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);

    /* don't allow 'inherited' in speculative evaluation mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* generate the property value */
    prop_expr->gen_code_propid(FALSE, prop_is_expr);

    /* generate the inhprop call */
    G_cg->js_expr("T3ctx.T3_inhprop($1, ${args[] 2 %d %d})",
                  argc, varargs);
}

/* ------------------------------------------------------------------------ */
/*
 *   "inherited class"
 */
void CTPNInhClass::gen_code(int discard, int for_condition)
{
    /* 
     *   we should never be asked to generate an "inherited" node
     *   directly; these nodes should always be generated as part of
     *   member evaluation 
     */
    G_tok->throw_internal_error(TCERR_GEN_CODE_INH);
}

/*
 *   evaluate a property 
 */
void CTPNInhClass::gen_code_member(int discard,
                                   CTcPrsNode *prop_expr, int prop_is_expr,
                                   int argc, int varargs)
{
    CTcSymbol *objsym;
    vm_obj_id_t obj;

    /* 
     *   make sure "self" is available - we obviously can't inherit
     *   anything if we're not in an object's method 
     */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);

    /* don't allow 'inherited' in speculative evaluation mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* get the superclass name symbol */
    objsym = G_cs->get_symtab()->find_or_def_undef(sym_, len_, FALSE);
    
    /* if it's not an object, we can't inherit from it */
    obj = objsym->get_val_obj();
    if (obj == VM_INVALID_OBJ)
    {
        G_tok->log_error(TCERR_INH_NOT_OBJ, (int)len_, sym_);
        return;
    }

    /* generate the property value */
    prop_expr->gen_code_propid(FALSE, prop_is_expr);

    /* generate the inhprop */
    G_cg->js_expr("T3ctx.T3_inhprop_from(T3O_%.*s, $1, ${args[] 2 %d %d})",
                  objsym->getlen(), objsym->getstr(),
                  argc, varargs);
}

/* ------------------------------------------------------------------------ */
/*
 *   "delegated" 
 */
void CTPNDelegated::gen_code(int discard, int for_condition)
{
    /* 
     *   we should never be asked to generate a "delegated" node directly;
     *   these nodes should always be generated as part of member evaluation 
     */
    G_tok->throw_internal_error(TCERR_GEN_CODE_DELEGATED);
}

/*
 *   evaluate a property 
 */
void CTPNDelegated::gen_code_member(int discard,
                                    CTcPrsNode *prop_expr, int prop_is_expr,
                                    int argc, int varargs)
{
    /* 
     *   make sure "self" is available - we obviously can't delegate
     *   anything if we're not in an object's method 
     */
    if (!G_cs->is_self_available())
        G_tok->log_error(TCERR_SELF_NOT_AVAIL);

    /* don't allow 'delegated' in speculative evaluation mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* generate the property value */
    prop_expr->gen_code_propid(FALSE, prop_is_expr);

    /* generate the delegatee expression */
    delegatee_->gen_code(FALSE, FALSE);

    /* generate the delegate call */
    G_cg->js_expr("T3_delegate($1, $2, ${args[] 3 %d %d})", argc, varargs);
}


/* ------------------------------------------------------------------------ */
/*
 *   "argcount" 
 */
void CTPNArgc::gen_code(int discard, int)
{
    /* generate the argument count, if we're not discarding */
    if (!discard)
        G_cg->js_expr("arguments.length");
}


/* ------------------------------------------------------------------------ */
/*
 *   constant
 */
void CTPNConst::gen_code(int discard, int)
{
    /* if we're discarding the value, do nothing */
    if (discard)
        return;

    /* generate the value into an expression buffer */
    js_expr_buf expr;
    gen_code(expr);

    /* push the expression onto the value stack */
    G_cg->push_js_expr(expr);
}

/*
 *   append a constant value to an expression buffer
 */
void CTPNConst::gen_code(js_expr_buf &expr)
{
    char buf[64];
    
    /* generate the appropriate type of push for the value */
    switch(val_.get_type())
    {
    case TC_CVT_NIL:
        expr += "null";
        break;

    case TC_CVT_TRUE:
        expr += "true";
        break;

    case TC_CVT_INT:
        /* write the push-integer instruction */
        s_gen_code_int(expr, val_.get_val_int());
        break;

    case TC_CVT_FLOAT:
        /* represent it as a regular js floating point value */
        expr.append(val_.get_val_float(), val_.get_val_float_len());
        break;

    case TC_CVT_SSTR:
        {
            /* start with an open quote */
            expr += '"';

            /* add characters from the string, quoting specials */
            const char *start, *str = val_.get_val_str();
            size_t len = val_.get_val_str_len();
            for (start = str ; ; ++str, --len)
            {
                /* we need to escape double quotes and control characters */
                int esc = (*str == '"' || *str < 32);
                
                /* 
                 *   if we're escaping this character, or we've reached the
                 *   end of the string, copy out the chunk up to this point 
                 */
                if ((len == 0 || esc) && str != start)
                    expr.append(start, str - start);
                
                /* if this is the end of the string, we're done */
                if (len == 0)
                    break;
                
                /* escape the current character if necessary */
                if (esc)
                {
                    /* escape it */
                    if (*str == '"')
                    {
                        /* double quote - generate the \" sequence */
                        expr += "\\\"";
                    }
                    else
                    {
                        /* control character - generate an octal \nnn sequence */
                        char escbuf[30];
                        sprintf(escbuf, "\\%03o", *str);
                        expr += escbuf;
                    }
                    
                    /* the next segment starts at the next character */
                    start = str + 1;
                }
            }
            
            /* add the close quote */
            expr += '"';
        }
        break;

    case TC_CVT_LIST:
        {
            /* open the array expression */
            expr += '[';
            
            /* add the list elements */
            int i;
            CTPNListEle *cur;
            for (cur = val_.get_val_list()->get_head(), i = 0 ; cur != 0 ;
                 cur = cur->get_next(), ++i)
            {
                /* add a comma before each element after the first */
                if (i != 0)
                    expr += ',';
                
                /* generate this expression */
                cur->get_expr()->gen_code(FALSE, FALSE);

                /* pop it into the expression buffer */
                G_cg->pop_js_expr(expr);
            }
            
            /* close the array expression */
            expr += ']';
        }
        break;

    case TC_CVT_OBJ:
        /* generate the object ID */
        // $$$ need the symbolic name instead
        sprintf(buf, "T3O_%d", val_.get_val_obj());
        expr += buf;
        break;

    case TC_CVT_PROP:
        /* generate the property ID */
        // $$$ need the symbolic name instead
        sprintf(buf, "T3P_%d", val_.get_val_prop());
        expr += buf;
        break;

    case TC_CVT_ENUM:
        /* generate the enum value */
        // $$$ need the symbolic name instead
        sprintf(buf, "T3E_%d", val_.get_val_enum());
        expr += buf;
        break;

    case TC_CVT_FUNCPTR:
        {
            /* generate the function pointer */
            CTcSymFunc *func = val_.get_val_funcptr_sym();
            expr.append(func->getstr(), func->getlen());
        }
        break;

    case TC_CVT_ANONFUNCPTR:
        /* generate a js anonymous function from the code body */
        val_.get_val_anon_func_ptr()->js_anon_func();

        /* that's our value - pop it into the expression buffer */
        G_cg->pop_js_expr(expr);
        break;

    default:
        /* anything else is an internal error */
        G_tok->throw_internal_error(TCERR_GEN_UNK_CONST_TYPE);
    }
}

/*
 *   generate code to push an integer value 
 */
void CTPNConst::s_gen_code_int(js_expr_buf &expr, long intval)
{
    char buf[32];
    
    sprintf(buf, "%d", intval);
    expr += buf;
}

/*
 *   Generate code to apply operator 'new' to the constant.  We can apply
 *   'new' only to constant object values. 
 */
void CTPNConst::gen_code_new(int discard, int argc, int varargs,
                             int /*from_call*/, int is_transient)
{
    /* check the type */
    switch(val_.get_type())
    {
    case TC_CVT_OBJ:
        /* 
         *   Treat this the same as any other 'new' call.  An object symbol
         *   folded into a constant is guaranteed to be of metaclass
         *   TadsObject - that's the only kind of symbol we'll ever fold this
         *   way. 
         */
        CTcSymObj::s_gen_code_new(discard,
                                  val_.get_val_obj(), val_.get_val_obj_meta(),
                                  argc, varargs, is_transient);
        break;

    default:
        /* can't apply 'new' to other constant values */
        G_tok->log_error(TCERR_INVAL_NEW_EXPR);
        break;
    }
}

/*
 *   Generate code to make a function call to this expression.  If we're
 *   calling a function, we can generate this directly.  
 */
void CTPNConst::gen_code_call(int discard, int argc, int varargs)
{
    /* check our type */
    switch(val_.get_type())
    {
    case TC_CVT_FUNCPTR:
        /* generate a call to our function symbol */
        val_.get_val_funcptr_sym()->gen_code_call(discard, argc, varargs);
        break;

    default:
        /* other types cannot be called */
        G_tok->log_error(TCERR_CANNOT_CALL_CONST);
        break;
    }
}

/*
 *   generate a property ID expression 
 */
vm_prop_id_t CTPNConst::gen_code_propid(int check_only, int is_expr)
{
    /* check the type */
    switch(val_.get_type())
    {
    case TC_CVT_PROP:
        /* generate the code */
        // $$$ make it symbolic?
        G_cg->js_expr("T3P_%d", val_.get_val_prop());

        /* return the constant property ID */
        return (vm_prop_id_t)val_.get_val_prop();

    default:
        /* other values cannot be used as properties */
        if (!check_only)
            G_tok->log_error(TCERR_INVAL_PROP_EXPR);

        /* indicate it's not a property value */
        return VM_INVALID_PROP;
    }
}


/*
 *   Generate code for a member evaluation 
 */
void CTPNConst::gen_code_member(int discard,
                                CTcPrsNode *prop_expr, int prop_is_expr,
                                int argc, int varargs)
{
    /* check our constant type */
    switch(val_.get_type())
    {
    case TC_CVT_OBJ:
        /* call the object symbol code to do the work */
        CTcSymObj::s_gen_code_member(discard, prop_expr, prop_is_expr,
                                     argc, val_.get_val_obj(), varargs);
        break;

    case TC_CVT_LIST:
    case TC_CVT_SSTR:
    case TC_CVT_FLOAT:
        /* 
         *   list/string/BigNumber constant - generate our value as
         *   normal, then use the standard member generation 
         */
        gen_code(FALSE, FALSE);

        /* use standard member generation */
        CTcPrsNode::s_gen_member_rhs(discard, prop_expr, prop_is_expr,
                                     argc, varargs);
        break;

    default:
        G_tok->log_error(TCERR_INVAL_OBJ_EXPR);
        break;
    }
}


/*
 *   generate code for an object before a '.'  
 */
vm_obj_id_t CTPNConst::gen_code_obj_predot(int *is_self)
{
    /* we're certainly not "self" */
    *is_self = FALSE;

    /* if I don't have an object value, this is illegal */
    if (val_.get_type() != TC_CVT_OBJ)
    {
        G_tok->log_error(TCERR_INVAL_OBJ_EXPR);
        return VM_INVALID_OBJ;
    }

    /* report our constant object value */
    return val_.get_val_obj();
}

/* ------------------------------------------------------------------------ */
/*
 *   debugger constant 
 */
void CTPNDebugConst::gen_code(int discard, int for_condition)
{
    /* not currently implemented */
    assert(FALSE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Generic Unary Operator 
 */

/* 
 *   Generate a unary-operator opcode.  We assume that the opcode has no
 *   side effects other than to compute the result, so we do not generate
 *   the opcode at all if 'discard' is true; we do, however, always
 *   generate code for the subexpression to ensure that its side effects
 *   are performed.
 *   
 *   In most cases, the caller simply should pass through its 'discard'
 *   status, since the result of the subexpression is generally needed
 *   only when the result of the enclosing expression is needed.
 *   
 *   In most cases, the caller should pass FALSE for 'for_condition',
 *   because applying an operator to the result generally requires that
 *   the result be properly converted for use as a temporary value.
 *   However, when the caller knows that its own opcode will perform the
 *   same conversions that a conditional opcode would, 'for_condition'
 *   should be TRUE.  In most cases, the caller's own 'for_condition'
 *   status is not relevant and should thus not be passed through.  
 */
void CTPNUnary::gen_unary(const char *op, int discard, int for_condition)
{
    /* 
     *   Generate the operand.  Pass through the 'discard' status to the
     *   operand - if the result of the parent operator is being
     *   discarded, then so is the result of this subexpression.  In
     *   addition, pass through the caller's 'for_condition' disposition.  
     */
    sub_->gen_code(discard, for_condition);

    /* apply the operator if we're not discarding the result */
    if (!discard)
        G_cg->js_expr("%s($1)", op);
}

/* ------------------------------------------------------------------------ */
/*
 *   Generic Binary Operator
 */

/*
 *   Generate a binary-operator opcode.
 *   
 *   In most cases, the caller's 'discard' status should be passed
 *   through, since the results of the operands are usually needed if and
 *   only if the results of the enclosing expression are needed.
 *   
 *   In most cases, the caller should pass FALSE for 'for_condition'.
 *   Only when the caller knows that the opcode will perform the same
 *   conversions as a BOOLIZE instruction should it pass TRUE for
 *   'for_condition'.  
 */
void CTPNBin::gen_binary(const char *op, int discard, int for_condition)
{
    /* 
     *   generate the operands, passing through the discard and
     *   conditional status 
     */
    left_->gen_code(discard, for_condition);
    right_->gen_code(discard, for_condition);

    /* generate our operand if we're not discarding the result */
    if (!discard)
        G_cg->js_expr("($1)%s($2)", op);
}



/* ------------------------------------------------------------------------ */
/*
 *   logical NOT
 */
void CTPNNot::gen_code(int discard, int)
{
    /*
     *   Generate the subexpression and apply the NOT opcode.  Note that we
     *   have to generate the subexpression as a regular value, not a
     *   conditional, since javascript treats empty strings as false values
     *   with this operator.  
     */
    gen_unary("!", discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Boolean-ize operator
 */
void CTPNBoolize::gen_code(int discard, int for_condition)
{
    /*
     *   If the result will be used for a conditional, there's no need to
     *   generate an instruction to convert the value to boolean.  The opcode
     *   that will be used for the condition will perform exactly the same
     *   conversions that this opcode would apply; avoid the redundant work
     *   in this case, and simply generate the underlying expression
     *   directly.  
     */
    if (for_condition)
    {
        /* generate the underlying expression without modification */
        sub_->gen_code(discard, for_condition);

        /* done */
        return;
    }
    
    /* turn it into a boolean value */
    gen_unary("T3_boolize", discard, for_condition);
}


/* ------------------------------------------------------------------------ */
/*
 *   bitwise NOT 
 */
void CTPNBNot::gen_code(int discard, int)
{
    gen_unary("~", discard, FALSE);
}


/* ------------------------------------------------------------------------ */
/*
 *   arithmetic positive
 */
void CTPNPos::gen_code(int discard, int)
{
    /* 
     *   simply generate our operand, since the operator itself has no
     *   effect 
     */
    sub_->gen_code(discard, FALSE); 
}

/* ------------------------------------------------------------------------ */
/*
 *   unary arithmetic negative
 */
void CTPNNeg::gen_code(int discard, int)
{
    gen_unary("-", discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   pre-increment
 */
void CTPNPreInc::gen_code(int discard, int)
{
    /* ask the subnode to generate it */
    if (!sub_->gen_code_asi(discard, TC_ASI_PREINC, 0, FALSE))
    {
        /* 
         *   the subnode didn't handle it - generate code to evaluate the
         *   subnode, increment that value, then assign the result back to
         *   the subnode with a simple assignment 
         */
        sub_->gen_code(FALSE, FALSE);

        /* increment the value at top of stack */
        G_cg->js_expr("++($1)");
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   pre-decrement
 */
void CTPNPreDec::gen_code(int discard, int)
{
    /* ask the subnode to generate it */
    if (!sub_->gen_code_asi(discard, TC_ASI_PREDEC, 0, FALSE))
    {
        /* 
         *   the subnode didn't handle it - generate code to evaluate the
         *   subnode, decrement that value, then assign the result back to
         *   the subnode with a simple assignment 
         */
        sub_->gen_code(FALSE, FALSE);

        /* decrement the value at top of stack */
        G_cg->js_expr("--($1)");
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   post-increment
 */
void CTPNPostInc::gen_code(int discard, int)
{
    /* ask the subnode to generate it */
    if (!sub_->gen_code_asi(discard, TC_ASI_POSTINC, 0, FALSE))
    {
        /* 
         *   the subnode didn't handle it - generate code to evaluate the
         *   subnode, increment that value, then assign the result back to
         *   the subnode with a simple assignment 
         */
        sub_->gen_code(FALSE, FALSE);

        /* increment the value at top of stack */
        G_cg->js_expr("($1)++");
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   post-decrement
 */
void CTPNPostDec::gen_code(int discard, int)
{
    /* ask the subnode to generate it */
    if (!sub_->gen_code_asi(discard, TC_ASI_POSTDEC, 0, FALSE))
    {
        /* 
         *   the subnode didn't handle it - generate code to evaluate the
         *   subnode, decrement that value, then assign the result back to
         *   the subnode with a simple assignment 
         */
        sub_->gen_code(FALSE, FALSE);

        /* decrement the value at top of stack */
        G_cg->js_expr("($1)--");
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   operator 'new'
 */
void CTPNNew::gen_code(int discard, int /*for condition*/)
{
    /* 
     *   ask my subexpression to generate the code - at this point we
     *   don't know the number of arguments, so pass in zero for now 
     */
    sub_->gen_code_new(discard, 0, FALSE, FALSE, transient_);
}

/* ------------------------------------------------------------------------ */
/*
 *   operator 'delete'
 */
void CTPNDelete::gen_code(int, int)
{
    /* 'delete' generates no code for T3 VM */
}

/* ------------------------------------------------------------------------ */
/*
 *   comma operator
 */
void CTPNComma::gen_code(int discard, int for_condition)
{
    /* 
     *   Generate each side's code.  Note that the left side is *always*
     *   discarded, regardless of whether the result of the comma operator
     *   will be discarded.  After we generate our subexpressions, there's
     *   nothing left to do, since the comma operator itself doesn't
     *   change anything - we simply use the right operand result as our
     *   result.
     *   
     *   Pass through the 'for_condition' status to the right operand,
     *   since we pass through its result to the caller.  For the left
     *   operand, treat it as a condition - we don't care about the result
     *   value, so don't bother performing any extra conversions on it.  
     */
    left_->gen_code(TRUE, TRUE);
    right_->gen_code(discard, for_condition);

    /* write the js comma expression */
    G_cg->js_expr("($1),($2)");
}

/* ------------------------------------------------------------------------ */
/*
 *   logical OR (short-circuit logic)
 */
void CTPNOr::gen_code(int discard, int for_condition)
{
    /* generate the left and right subexpressions */
    left_->gen_code(FALSE, TRUE);
    right_->gen_code(discard, TRUE);

    /* generate the OR */
    G_cg->js_expr("($1)||($2)");

    /* 
     *   if the result is not going to be used directly for a condition,
     *   we must boolean-ize the value 
     */
    if (!for_condition)
        G_cg->js_expr("T3_boolize($1)");
}

/* ------------------------------------------------------------------------ */
/*
 *   logical AND (short-circuit logic)
 */
void CTPNAnd::gen_code(int discard, int for_condition)
{
    /* generate the left and right subexpressions */
    left_->gen_code(FALSE, TRUE);
    right_->gen_code(discard, TRUE);

    /* generate the AND */
    G_cg->js_expr("($1)&&($2)");

    /* 
     *   if the result is not going to be used directly for a condition,
     *   we must boolean-ize the value 
     */
    if (!for_condition)
        G_cg->js_expr("T3_boolize($1)");
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise OR
 */
void CTPNBOr::gen_code(int discard, int)
{
    gen_binary(OPC_BOR, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise AND
 */
void CTPNBAnd::gen_code(int discard, int)
{
    gen_binary(OPC_BAND, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise XOR
 */
void CTPNBXor::gen_code(int discard, int)
{
    gen_binary(OPC_XOR, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   greater-than
 */
void CTPNGt::gen_code(int discard, int)
{
    gen_binary(OPC_GT, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   greater-or-equal
 */
void CTPNGe::gen_code(int discard, int)
{
    gen_binary(OPC_GE, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   less-than
 */
void CTPNLt::gen_code(int discard, int)
{
    gen_binary(OPC_LT, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   less-or-equal
 */
void CTPNLe::gen_code(int discard, int)
{
    gen_binary(OPC_LE, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   compare for equality
 */
void CTPNEq::gen_code(int discard, int)
{
    gen_binary(OPC_EQ, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   compare for inequality
 */
void CTPNNe::gen_code(int discard, int)
{
    gen_binary(OPC_NE, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   'is in' 
 */
void CTPNIsIn::gen_code(int discard, int)
{
    CTPNArglist *lst;
    CTPNArg *arg;
    CTcCodeLabel *lbl_found;
    CTcCodeLabel *lbl_done;

    /* allocate our 'found' label */
    lbl_found = G_cs->new_label_fwd();

    /* 
     *   allocate our 'done' label - we only need to do this if we don't
     *   have a constant true value and we're not discarding the result
     */
    if (!const_true_ && !discard)
        lbl_done = G_cs->new_label_fwd();

    /* generate my left-side expression */
    left_->gen_code(FALSE, FALSE);

    /* the right side is always an argument list */
    lst = (CTPNArglist *)right_;

    /* compare to each element in the list on the right */
    for (arg = lst->get_arg_list_head() ; arg != 0 ;
         arg = arg->get_next_arg())
    {
        /* 
         *   duplicate the left-side value, so we don't have to generate
         *   it again for this comparison 
         */
        G_cg->write_op(OPC_DUP);

        /* generate this list element */
        arg->gen_code(FALSE, FALSE);

        /* if they're equal, jump to the 'found' label */
        G_cg->write_op(OPC_JE);
        G_cs->write_ofs2(lbl_found, 0);

        /* we pushed one more (DUP) and popped two (JE) */
        G_cg->note_push(1);
        G_cg->note_pop(2);
    }

    /* 
     *   Generate the code that comes at the end of all of tests when we
     *   fail to find any matches - we simply discard the left-side value
     *   from the stack, push our 'nil' value, and jump to the end label.
     *   
     *   If we have a constant 'true' value, there's no need to do any of
     *   this, because we know that, even after testing all of our
     *   non-constant values, there's a constant value that makes the
     *   entire expression true, and we can thus just fall through to the
     *   'found' code.
     *   
     *   If we're discarding the result, there's no need to push a
     *   separate value for the result, so we can just fall through to the
     *   common ending code in this case.  
     */
    if (!const_true_ && !discard)
    {
        G_cg->write_op(OPC_DISC);
        G_cg->write_op(OPC_PUSHNIL);
        G_cg->write_op(OPC_JMP);
        G_cs->write_ofs2(lbl_done, 0);
    }

    /* 
     *   Generate the 'found' code - this discards the left-side value and
     *   pushes our 'true' result.  Note that there's no reason to push
     *   our result if we're discarding it.  
     */
    def_label_pos(lbl_found);
    G_cg->write_op(OPC_DISC);

    /* 
     *   if we're discarding the result, just note the pop of the left
     *   value; otherwise, push our result 
     */
    if (discard)
        G_cg->note_pop();
    else
        G_cg->write_op(OPC_PUSHTRUE);

    /* our 'done' label is here, if we needed one */
    if (!const_true_ && !discard)
        def_label_pos(lbl_done);
}

/* ------------------------------------------------------------------------ */
/*
 *   'not in' 
 */
void CTPNNotIn::gen_code(int discard, int)
{
    CTPNArglist *lst;
    CTPNArg *arg;
    CTcCodeLabel *lbl_found;
    CTcCodeLabel *lbl_done;

    /* allocate our 'found' label */
    lbl_found = G_cs->new_label_fwd();

    /* 
     *   allocate our 'done' label - we only need to do this if we don't
     *   have a constant false value 
     */
    if (!const_false_ && !discard)
        lbl_done = G_cs->new_label_fwd();

    /* generate my left-side expression */
    left_->gen_code(FALSE, FALSE);

    /* the right side is always an argument list */
    lst = (CTPNArglist *)right_;

    /* compare to each element in the list on the right */
    for (arg = lst->get_arg_list_head() ; arg != 0 ;
         arg = arg->get_next_arg())
    {
        /* 
         *   duplicate the left-side value, so we don't have to generate
         *   it again for this comparison 
         */
        G_cg->write_op(OPC_DUP);

        /* generate this list element */
        arg->gen_code(FALSE, FALSE);

        /* if they're equal, jump to the 'found' label */
        G_cg->write_op(OPC_JE);
        G_cs->write_ofs2(lbl_found, 0);

        /* we pushed one more (DUP) and popped two (JE) */
        G_cg->note_push(1);
        G_cg->note_pop(2);
    }

    /* 
     *   Generate the code that comes at the end of all of tests when we
     *   fail to find any matches - we simply discard the left-side value
     *   from the stack, push our 'true' value, and jump to the end label.
     *   
     *   If we have a constant 'nil' value, however, there's no need to do
     *   any of this, because we know that, even after testing all of our
     *   non-constant values, there's a matching constant value that makes
     *   the entire expression false (because 'not in' is false if we find
     *   a match), and we can thus just fall through to the 'found' code.  
     */
    if (!const_false_ && !discard)
    {
        G_cg->write_op(OPC_DISC);
        G_cg->write_op(OPC_PUSHTRUE);
        G_cg->write_op(OPC_JMP);
        G_cs->write_ofs2(lbl_done, 0);
    }

    /* 
     *   generate the 'found' code - this discards the left-side value and
     *   pushes our 'nil' result (because the result of 'not in' is false
     *   if we found the value) 
     */
    def_label_pos(lbl_found);
    G_cg->write_op(OPC_DISC);

    /* push the result, or note the pop if we're just discarding it */
    if (discard)
        G_cg->note_pop();
    else
        G_cg->write_op(OPC_PUSHNIL);

    /* our 'done' label is here, if we needed one */
    if (!const_false_ && !discard)
        def_label_pos(lbl_done);
}

/* ------------------------------------------------------------------------ */
/*
 *   bit-shift left
 */
void CTPNShl::gen_code(int discard, int)
{
    gen_binary(OPC_SHL, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   bit-shift right
 */
void CTPNShr::gen_code(int discard, int)
{
    gen_binary(OPC_SHR, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   multiply
 */
void CTPNMul::gen_code(int discard, int)
{
    /* if either side is zero or one, we can apply special handling */
    if (left_->is_const_int(0))
    {
        /* evaluate the right for side effects and discard the result */
        right_->gen_code(TRUE, TRUE);

        /* the result is zero */
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();

        /* done */
        return;
    }
    else if (right_->is_const_int(0))
    {
        /* evaluate the left for side effects and discard the result */
        left_->gen_code(TRUE, TRUE);

        /* the result is zero */
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();

        /* done */
        return;
    }
    else if (left_->is_const_int(1))
    {
        /* 
         *   evaluate the right side - it's the result; note that, because
         *   of the explicit multiplication, we must compute logical
         *   results using assignment (not 'for condition') rules 
         */
        right_->gen_code(discard, FALSE);

        /* done */
        return;
    }
    else if (right_->is_const_int(1))
    {
        /* evaluate the right side - it's the result */
        left_->gen_code(discard, FALSE);

        /* done */
        return;
    }

    /* apply generic handling */
    gen_binary(OPC_MUL, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   divide
 */
void CTPNDiv::gen_code(int discard, int for_cond)
{
    /* if dividing by 1, we can skip the whole thing (except side effects) */
    if (right_->is_const_int(1))
    {
        /* 
         *   simply generate the left side for side effects; actually
         *   doing the arithmetic has no effect 
         */
        left_->gen_code(discard, for_cond);
        return;
    }

    /* if the left side is zero, the result is always zero */
    if (left_->is_const_int(0))
    {
        /* evaluate the right for side effects, but discard the result */
        right_->gen_code(TRUE, TRUE);

        /* the result is zero */
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();
        return;
    }

    /* use generic code generation */
    gen_binary(OPC_DIV, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   modulo
 */
void CTPNMod::gen_code(int discard, int for_condition)
{
    /* if dividing by 1, we can skip the whole thing (except side effects) */
    if (right_->is_const_int(1))
    {
        /* 
         *   simply generate the left side for side effects; actually
         *   doing the arithmetic has no effect 
         */
        left_->gen_code(discard, for_condition);

        /* the result is zero */
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();
        return;
    }

    /* if the left side is zero, the result is always zero */
    if (left_->is_const_int(0))
    {
        /* evaluate the right for side effects, but discard the result */
        right_->gen_code(TRUE, TRUE);

        /* the result is zero */
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();
        return;
    }

    /* use generic processing */
    gen_binary(OPC_MOD, discard, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   subtract
 */
void CTPNSub::gen_code(int discard, int for_cond)
{
    /* check for subtracting 1, which we can accomplish more efficiently */
    if (right_->is_const_int(1))
    {
        /* 
         *   We're subtracting one - use decrement.  The decrement
         *   operator itself has no side effects, so we can pass through
         *   the 'discard' status to the subnode.  
         */
        left_->gen_code(discard, FALSE);

        /* apply decrement if we're not discarding the result */
        if (!discard)
            G_cg->write_op(OPC_DEC);
    }
    else
    {
        /* we can't do anything special - use the general-purpose code */
        gen_binary(OPC_SUB, discard, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   add
 */
void CTPNAdd::gen_code(int discard, int)
{
    /* check for adding 1, which we can accomplish more efficiently */
    if (right_->is_const_int(1))
    {
        /* 
         *   We're adding one - use increment.  The increment operator
         *   itself has no side effects, so we can pass through the
         *   'discard' status to the subnode.  
         */
        left_->gen_code(discard, FALSE);
        
        /* apply increment if we're not discarding the result */
        if (!discard)
            G_cg->write_op(OPC_INC);
    }
    else
    {
        /* we can't do anything special - use the general-purpose code */
        gen_binary(OPC_ADD, discard, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   simple assignment
 */
void CTPNAsi::gen_code(int discard, int)
{
    /* 
     *   Ask the left subnode to generate a simple assignment to the value
     *   on the right.  Simple assignments cannot be refused, so we don't
     *   need to try to do any assignment work ourselves.  
     */
    left_->gen_code_asi(discard, TC_ASI_SIMPLE, right_, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   add and assign
 */
void CTPNAddAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate an add-and-assign; if it can't,
     *   handle it generically 
     */
    if (!left_->gen_code_asi(discard, TC_ASI_ADD, right_, FALSE))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused
         */
        gen_binary(OPC_ADD, FALSE, FALSE);
        left_->gen_code_asi(discard, TC_ASI_SIMPLE, 0, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   subtract and assign
 */
void CTPNSubAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a subtract-and-assign; if it
     *   can't, handle it generically 
     */
    if (!left_->gen_code_asi(discard, TC_ASI_SUB, right_, FALSE))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary(OPC_SUB, FALSE, FALSE);
        left_->gen_code_asi(discard, TC_ASI_SIMPLE, 0, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   multiply and assign
 */
void CTPNMulAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a multiply-and-assign; if it
     *   can't, handle it generically 
     */
    if (!left_->gen_code_asi(discard, TC_ASI_MUL, right_, FALSE))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary(OPC_MUL, FALSE, FALSE);
        left_->gen_code_asi(discard, TC_ASI_SIMPLE, 0, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   divide and assign
 */
void CTPNDivAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a divide-and-assign; if it
     *   can't, handle it generically 
     */
    if (!left_->gen_code_asi(discard, TC_ASI_DIV, right_, FALSE))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary(OPC_DIV, FALSE, FALSE);
        left_->gen_code_asi(discard, TC_ASI_SIMPLE, 0, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   modulo and assign
 */
void CTPNModAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a mod-and-assign; if it can't,
     *   handle it generically 
     */
    if (!left_->gen_code_asi(discard, TC_ASI_MOD, right_, FALSE))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary(OPC_MOD, FALSE, FALSE);
        left_->gen_code_asi(discard, TC_ASI_SIMPLE, 0, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise-AND and assign
 */
void CTPNBAndAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate an AND-and-assign; if it can't,
     *   handle it generically 
     */
    if (!left_->gen_code_asi(discard, TC_ASI_BAND, right_, FALSE))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary(OPC_BAND, FALSE, FALSE);
        left_->gen_code_asi(discard, TC_ASI_SIMPLE, 0, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise-OR and assign
 */
void CTPNBOrAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate an OR-and-assign; if it can't,
     *   handle it generically 
     */
    if (!left_->gen_code_asi(discard, TC_ASI_BOR, right_, FALSE))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary(OPC_BOR, FALSE, FALSE);
        left_->gen_code_asi(discard, TC_ASI_SIMPLE, 0, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   bitwise-XOR and assign
 */
void CTPNBXorAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate an XOR-and-assign; if it can't,
     *   handle it generically 
     */
    if (!left_->gen_code_asi(discard, TC_ASI_BXOR, right_, FALSE))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary(OPC_XOR, FALSE, FALSE);
        left_->gen_code_asi(discard, TC_ASI_SIMPLE, 0, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   bit-shift left and assign
 */
void CTPNShlAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate an shift-left-and-assign; if it
     *   can't, handle it generically 
     */
    if (!left_->gen_code_asi(discard, TC_ASI_SHL, right_, FALSE))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary(OPC_SHL, FALSE, FALSE);
        left_->gen_code_asi(discard, TC_ASI_SIMPLE, 0, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   bit-shift right and assign
 */
void CTPNShrAsi::gen_code(int discard, int)
{
    /* 
     *   ask the left subnode to generate a shift-right-and-assign; if it
     *   can't, handle it generically 
     */
    if (!left_->gen_code_asi(discard, TC_ASI_SHR, right_, FALSE))
    {
        /* 
         *   there's no special coding for this assignment type -- compute
         *   the result generically, then assign the result as a simple
         *   assignment, which cannot be refused 
         */
        gen_binary(OPC_SHR, FALSE, FALSE);
        left_->gen_code_asi(discard, TC_ASI_SIMPLE, 0, FALSE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   subscript a list/array value
 */
void CTPNSubscript::gen_code(int discard, int)
{
    gen_binary(OPC_INDEX, discard, FALSE);
}

/*
 *   assign to a subscripted value
 */
int CTPNSubscript::gen_code_asi(int discard, tc_asitype_t typ,
                                CTcPrsNode *rhs, int)
{
    /* 
     *   If this isn't a simple assignment, tell the caller to emit the
     *   generic code to compute the composite result, then call us again
     *   for a simple assignment.  We can't add any value with specialized
     *   instructions for composite assignments, so there's no point in
     *   dealing with those here. 
     */
    if (typ != TC_ASI_SIMPLE)
        return FALSE;
    
    /* 
     *   Generate the value to assign to the element - that's the right
     *   side of the assignment operator.  If rhs is null, it means the
     *   caller has already done this.  
     */
    if (rhs != 0)
        rhs->gen_code(FALSE, FALSE);

    /* 
     *   if we're not discarding the result, duplicate the value to be
     *   assigned, so that it's left on the stack after we're finished
     *   (this is necessary because we'll consume one copy with the SETIND
     *   instruction) 
     */
    if (!discard)
    {
        G_cg->write_op(OPC_DUP);
        G_cg->note_push();
    }
    
    /* generate the value to be subscripted - that's my left-hand side */
    left_->gen_code(FALSE, FALSE);

    /* generate the index value - that's my right-hand side */
    right_->gen_code(FALSE, FALSE);

    /* generate the assign-to-indexed-value opcode */
    G_cg->write_op(OPC_SETIND);

    /* setind pops three and pushes one - net of pop 2 */
    G_cg->note_pop(2);

    /*
     *   The top value now on the stack is the new container value.  The new
     *   container will be different from the old container in some cases
     *   (with lists, for example, because we must create a new list object
     *   to contain the modified list value).  Therefore, if my left-hand
     *   side is an lvalue, we must assign the new container to the left-hand
     *   side - this makes something like "x[1] = 5" actually change the
     *   value in "x" if "x" is a local variable.  If my left-hand side isn't
     *   an lvalue, don't bother with this step, and simply discard the new
     *   container value.
     *   
     *   Regardless of whether we're keeping the result of the overall
     *   expression, we're definitely not keeping the result of assigning the
     *   new container - the result of the assignment is the value assigned,
     *   not the container.  Thus, discard = true in this call.  
     *   
     *   There's a special case that's handled through the peep-hole
     *   optimizer: if we are assigning to a local variable and indexing with
     *   a constant integer value, we will have converted the whole operation
     *   to a SETINDLCL1I8.  That instruction takes care of assigning the
     *   value back to the rvalue, so we don't need to generate a separate
     *   rvalue assignment.  
     */
    if (G_cg->get_last_op() == OPC_SETINDLCL1I8)
    {
        /* 
         *   no assignment is necessary - we just need to account for the
         *   difference in the stack arrangement with this form of the
         *   assignment, which is that we don't leave the value on the stack 
         */
        G_cg->note_pop();
    }
    else if (!left_->gen_code_asi(TRUE, TC_ASI_SIMPLE, 0, TRUE))
    {
        /* no assignment is possible; discard the new container value */
        G_cg->write_op(OPC_DISC);
        G_cg->note_pop();
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   conditional operator
 */
void CTPNIf::gen_code(int discard, int for_condition)
{
    CTcCodeLabel *lbl_else;
    CTcCodeLabel *lbl_end;

    /* 
     *   Generate the condition value - we need the value regardless of
     *   whether the overall result is going to be used, because we need
     *   it to determine which branch to take.  Generate the subexpression
     *   for a condition, so that we don't perform any extra unnecessary
     *   conversions on it.  
     */
    first_->gen_code(FALSE, TRUE);
 
    /* if the condition is false, jump to the 'else' expression part */
    lbl_else = gen_jump_ahead(OPC_JF);

    /* JF pops a value */
    G_cg->note_pop();

    /* 
     *   Generate the 'then' expression part.  Only request a return value if
     *   it has one AND we're not discarding it.  If it doesn't return a
     *   value, and we actually need one, we'll supply a default 'nil' value
     *   next.  This value will be our yielded value (in this branch,
     *   anyway), so pass through the for-condition flag.  
     */
    second_->gen_code(discard || !second_->has_return_value(), for_condition);

    /* 
     *   If this expression has no return value, and we need the return
     *   value, supply nil as the result.
     */
    if (!discard && !second_->has_return_value())
    {
        G_cg->write_op(OPC_PUSHNIL);
        G_cg->note_push();
    }
 
    /* unconditionally jump over the 'else' part */
    lbl_end = gen_jump_ahead(OPC_JMP);

    /* set the label for the 'else' part */
    def_label_pos(lbl_else);

    /* 
     *   Generate the 'else' part.  Only request a return value if it has one
     *   AND we're not discarding it.  Pass through 'discard' and
     *   'for_condition', since this result is our result.  
     */
    third_->gen_code(discard || !third_->has_return_value(), for_condition);

    /* 
     *   If this expression has no return value, and we need the return
     *   value, supply nil as the result.  
     */
    if (!discard && !third_->has_return_value())
    {
        G_cg->write_op(OPC_PUSHNIL);
        G_cg->note_push();
    }
 
    /* 
     *   Because of the jump, we only evaluate one of the two expressions
     *   we generated, so note an extra pop for the branch we didn't take.
     *   Note that if either one pushes a value, both will, since we'll
     *   explicitly have pushed nil for the one that doesn't generate a
     *   value to keep the stack balanced on both branches.
     *   
     *   If neither of our expressions yields a value, don't pop anything
     *   extra, since we won't think we've pushed two values in the course
     *   of generating the two expressions.  
     */
    if (second_->has_return_value() || third_->has_return_value())
        G_cg->note_pop();
 
    /* set the label for the end of the expression */
    def_label_pos(lbl_end);
}

/* ------------------------------------------------------------------------ */
/*
 *   symbol
 */
void CTPNSym::gen_code(int discard, int)
{
    /* 
     *   Look up the symbol; if it's undefined, add a default property
     *   symbol entry if possible.  Then ask the symbol to generate the
     *   code.  
     */
    G_cs->get_symtab()
        ->find_or_def_prop_implied(get_sym_text(), get_sym_text_len(),
                                   FALSE, G_cs->is_self_available())
        ->gen_code(discard);
}

/*
 *   assign to a symbol
 */
int CTPNSym::gen_code_asi(int discard, tc_asitype_t typ, CTcPrsNode *rhs,
                          int ignore_errors)
{
    /* 
     *   Look up the symbol; if it's undefined and there's a "self" object
     *   available, define it as a property by default, since a property
     *   is the only kind of symbol that we could possibly assign to
     *   without having defined anywhere in the program.  Once we have the
     *   symbol, tell it to generate the code for assigning to it.  
     */
    return G_cs->get_symtab()
        ->find_or_def_prop_implied(get_sym_text(), get_sym_text_len(),
                                   FALSE, G_cs->is_self_available())
        ->gen_code_asi(discard, typ, rhs, ignore_errors);
}

/*
 *   take the address of the symbol 
 */
void CTPNSym::gen_code_addr()
{
    /* 
     *   Look up our symbol in the symbol table, then ask the resulting
     *   symbol to generate the appropriate code.  If the symbol isn't
     *   defined, and we have a "self" object available (i.e., we're in
     *   method code), define the symbol by default as a property.
     *   
     *   Note that we look only in the global symbol table, because local
     *   symbols have no address value.  So, even if the symbol is defined
     *   in the local table, ignore the local definition and look at the
     *   global definition.  
     */
    G_prs->get_global_symtab()
        ->find_or_def_prop_explicit(get_sym_text(), get_sym_text_len(),
                                    FALSE)
        ->gen_code_addr();
}

/*
 *   call the symbol 
 */
void CTPNSym::gen_code_call(int discard, int argc, int varargs)
{
    /*
     *   Look up our symbol in the symbol table, then ask the resulting
     *   symbol to generate the appropriate call.  The symbol is
     *   implicitly a property (if in a method context), since that's the
     *   only kind of undefined symbol that we could be calling.  
     */
    G_cs->get_symtab()
        ->find_or_def_prop_implied(get_sym_text(), get_sym_text_len(),
                                   FALSE, G_cs->is_self_available())
        ->gen_code_call(discard, argc, varargs);
}

/*
 *   generate code for 'new' 
 */
void CTPNSym::gen_code_new(int discard, int argc, int varargs,
                           int /*from_call*/, int is_transient)
{
    /*
     *   Look up our symbol, then ask the resulting symbol to generate the
     *   'new' code.  If the symbol is undefined, add an 'undefined' entry
     *   to the table; we can't implicitly create an object symbol. 
     */
    G_cs->get_symtab()
        ->find_or_def_undef(get_sym_text(), get_sym_text_len(), FALSE)
        ->gen_code_new(discard, argc, varargs, is_transient);
}

/*
 *   generate a property ID expression 
 */
vm_prop_id_t CTPNSym::gen_code_propid(int check_only, int is_expr)
{
    CTcSymbol *sym;
    CTcPrsSymtab *symtab;

    /*
     *   Figure out where to look for the symbol.  If the symbol was given
     *   as an expression (in other words, it was explicitly enclosed in
     *   parentheses), look it up in the local symbol table, since it
     *   could refer to a local.  Otherwise, it must refer to a property,
     *   so look only in the global table.
     *   
     *   If the symbol isn't defined already, define it as a property now.
     *   Because the symbol is explicitly on the right side of a member
     *   evaluation, we can define it as a property whether or not there's
     *   a valid "self" in this context.  
     */
    if (is_expr)
    {
        /* it's an expression - look it up in the local symbol table */
        symtab = G_cs->get_symtab();
    }
    else
    {
        /* it's a simple symbol - look only in the global symbol table */
        symtab = G_prs->get_global_symtab();
    }

    /* 
     *   look it up (note that this will always return a valid symbol,
     *   since it will create one if we can't find an existing entry) 
     */
    sym = symtab->find_or_def_prop(get_sym_text(), get_sym_text_len(), FALSE);

    /* ask the symbol to generate the property reference */
    return sym->gen_code_propid(check_only, is_expr);
}

/*
 *   generate code for a member expression 
 */
void CTPNSym::gen_code_member(int discard, CTcPrsNode *prop_expr,
                              int prop_is_expr, int argc, int varargs)
{
    /* 
     *   Look up the symbol, and let it do the work.  There's no
     *   appropriate default for the symbol, so leave it undefined if we
     *   can't find it. 
     */
    G_cs->get_symtab()
        ->find_or_def_undef(get_sym_text(), get_sym_text_len(), FALSE)
        ->gen_code_member(discard, prop_expr, prop_is_expr, argc, varargs);
}

/*
 *   generate code for an object before a '.'  
 */
vm_obj_id_t CTPNSym::gen_code_obj_predot(int *is_self)
{
    /* 
     *   Look up the symbol, and let it do the work.  There's no default
     *   type for the symbol, so leave it undefined if we don't find it. 
     */
    return G_cs->get_symtab()
        ->find_or_def_undef(get_sym_text(), get_sym_text_len(), FALSE)
        ->gen_code_obj_predot(is_self);
}

/* ------------------------------------------------------------------------ */
/*
 *   resolved symbol 
 */
void CTPNSymResolved::gen_code(int discard, int)
{
    /* let the symbol handle it */
    sym_->gen_code(discard);
}

/*
 *   assign to a symbol 
 */
int CTPNSymResolved::gen_code_asi(int discard, tc_asitype_t typ,
                                  CTcPrsNode *rhs,
                                  int ignore_errors)
{
    /* let the symbol handle it */
    return sym_->gen_code_asi(discard, typ, rhs, ignore_errors);
}

/*
 *   take the address of the symbol 
 */
void CTPNSymResolved::gen_code_addr()
{
    /* let the symbol handle it */
    sym_->gen_code_addr();
}

/*
 *   call the symbol 
 */
void CTPNSymResolved::gen_code_call(int discard, int argc, int varargs)
{
    /* let the symbol handle it */
    sym_->gen_code_call(discard, argc, varargs);
}

/*
 *   generate code for 'new' 
 */
void CTPNSymResolved::gen_code_new(int discard, int argc, int varargs,
                                   int /*from_call*/, int is_transient)
{
    /* let the symbol handle it */
    sym_->gen_code_new(discard, argc, varargs, is_transient);
}

/*
 *   generate a property ID expression 
 */
vm_prop_id_t CTPNSymResolved::gen_code_propid(int check_only, int is_expr)
{
    /* let the symbol handle it */
    return sym_->gen_code_propid(check_only, is_expr);
}

/*
 *   generate code for a member expression 
 */
void CTPNSymResolved::gen_code_member(int discard, 
                                      CTcPrsNode *prop_expr, int prop_is_expr,
                                      int argc, int varargs)
{
    /* let the symbol handle it */
    sym_->gen_code_member(discard, prop_expr, prop_is_expr, argc, varargs);
}

/*
 *   generate code for an object before a '.'  
 */
vm_obj_id_t CTPNSymResolved::gen_code_obj_predot(int *is_self)
{
    /* let the symbol handle it */
    return sym_->gen_code_obj_predot(is_self);
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger local variable symbol 
 */

/*
 *   generate code to evaluate the variable
 */
void CTPNSymDebugLocal::gen_code(int discard, int for_condition)
{
    /* if we're not discarding the value, push the local */
    if (!discard)
    {
        /* generate the debugger local/parameter variable instruction */
        G_cg->write_op(is_param_ ? OPC_GETDBARG : OPC_GETDBLCL);
        G_cs->write2(var_id_);
        G_cs->write2(frame_idx_);

        /* note that we pushed the value */
        G_cg->note_push();

        /* if it's a context local, get the value from the context array */
        if (ctx_arr_idx_ != 0)
        {
            CTPNConst::s_gen_code_int(ctx_arr_idx_);
            G_cg->write_op(OPC_INDEX);

            /* 
             *   the 'index' operation pops two values and pushes one, for a
             *   net of one pop 
             */
            G_cg->note_pop();
        }
    }
}

/*
 *   generate code for assigning to this variable 
 */
int CTPNSymDebugLocal::gen_code_asi(int discard, tc_asitype_t typ,
                                    CTcPrsNode *rhs, int ignore_error)    
{
    /* 
     *   if this isn't a simple assignment, use the generic combination
     *   assignment computation 
     */
    if (typ != TC_ASI_SIMPLE)
        return FALSE;

    /* generate the value to be assigned */
    if (rhs != 0)
        rhs->gen_code(FALSE, FALSE);

    /* 
     *   if we're not discarding the result, duplicate the value so we'll
     *   have a copy after the assignment 
     */
    if (!discard)
    {
        G_cg->write_op(OPC_DUP);
        G_cg->note_push();
    }

    /* check for a context property */
    if (ctx_arr_idx_ == 0)
    {
        /* 
         *   generate the debug-local-set instruction - the operands are
         *   the variable number and the stack frame index 
         */
        G_cg->write_op(is_param_ ? OPC_SETDBARG : OPC_SETDBLCL);
        G_cs->write2(var_id_);
        G_cs->write2(frame_idx_);
    }
    else
    {
        /* get the local containing our context object */
        G_cg->write_op(OPC_GETDBLCL);
        G_cs->write2(var_id_);
        G_cs->write2(frame_idx_);

        /* set the actual variable value in the context object */
        CTPNConst::s_gen_code_int(ctx_arr_idx_);
        G_cg->write_op(OPC_SETIND);
        G_cg->write_op(OPC_DISC);

        /* 
         *   we did three pops (SETIND), then a push (SETIND), then a pop
         *   (DISC) - this is a net of three extra pops
         */
        G_cg->note_pop(3);
    }

    /* the debug-local-set removes the rvalue from the stack */
    G_cg->note_pop();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Double-quoted string.  The 'discard' status is irrelevant, because we
 *   evaluate double-quoted strings for their side effects.  
 */
void CTPNDstr::gen_code(int discard, int)
{
    /* if we're not discarding the value, it's an error */
    if (!discard)
        G_tok->log_error(TCERR_DQUOTE_IN_EXPR, (int)len_, str_);

    /* generate the instruction to display it */
    G_cg->write_op(OPC_SAY);

    /* add the string to the constant pool, creating a fixup here */
    G_cg->add_const_str(str_, len_, G_cs, G_cs->get_ofs());

    /* write a placeholder value, which will be corrected by the fixup */
    G_cs->write4(0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Double-quoted debug string 
 */
void CTPNDebugDstr::gen_code(int, int)
{
    /* generate code to push the in-line string */
    G_cg->write_op(OPC_PUSHSTRI);
    G_cs->write2(len_);
    G_cs->write(str_, len_);

    /* write code to display the value */
    G_cg->write_op(OPC_SAYVAL);

    /* note that we pushed the string and then popped it */
    G_cg->note_push();
    G_cg->note_pop();
}

/* ------------------------------------------------------------------------ */
/*
 *   Double-quoted string embedding 
 */

/*
 *   create an embedding 
 */
CTPNDstrEmbed::CTPNDstrEmbed(CTcPrsNode *sub)
    : CTPNDstrEmbedBase(sub)
{
}

/*
 *   Generate code for a double-quoted string embedding 
 */
void CTPNDstrEmbed::gen_code(int, int)
{
    int orig_depth;

    /* note the stack depth before generating the expression */
    orig_depth = G_cg->get_sp_depth();
    
    /* 
     *   Generate code for the embedded expression.  If the expression has a
     *   return value, generate the value so that it can be displayed in the
     *   string; but don't request a value if it doesn't have one, as a
     *   return value is optional in this context.  This is a normal value
     *   invocation, not a conditional, so we need any applicable normal
     *   value conversions.  
     */
    sub_->gen_code(!sub_->has_return_value(), FALSE);

    /* 
     *   If the code generation left anything on the stack, generate code
     *   to display the value via the default display function.  
     */
    if (G_cg->get_sp_depth() > orig_depth)
    {
        /* add a SAYVAL instruction */
        G_cg->write_op(OPC_SAYVAL);

        /* SAYVAL pops the argument value */
        G_cg->note_pop();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Argument list
 *   
 *   For fixed arguments, generate as 'argc' stack elements.
 *   
 *   For varargs, generate as a single stack entry containing an array with
 *   the arguments.  
 */
void CTPNArglist::gen_code_arglist(int *varargs)
{
    CTPNArg *arg;
    int i;
    int *va = new int[argc_ + 1];

    /* 
     *   scan the argument list for varargs - if we have any, we must
     *   treat all of them as varargs 
     */
    for (i = argc_, *varargs = FALSE, arg = get_arg_list_head() ;
         arg != 0 ; arg = arg->get_next_arg(), --i)
    {
        /* note the individual varargs value */
        va[i] = arg->is_varargs();

        /* if this is a varargs argument, it's a varargs list overall */
        *varargs |= va[i];
    }

    /* 
     *   Push each argument in the list - start with the last element and
     *   work backwards through the list to the first element.  The parser
     *   builds the list in reverse order, so we must merely follow the
     *   list from head to tail.
     *   
     *   We need each argument value to be pushed (hence discard = false),
     *   and we need the assignable value of each argument expression
     *   (hence for_condition = false).  
     */
    for (i = argc_, arg = get_arg_list_head() ; arg != 0 ;
         arg = arg->get_next_arg(), --i)
    {
        int depth;

        /* note the stack depth before generating the value */
        depth = G_cg->get_sp_depth();

        /* generate the argument's code */
        arg->gen_code(FALSE, FALSE);

        /* ensure that it generated something */
        if (G_cg->get_sp_depth() <= depth)
            G_tok->log_error(TCERR_ARG_EXPR_HAS_NO_VAL, i);
    }

    /*
     *   If this is a varargs list, build an array containing the expanded
     *   argument values. 
     */
    if (*varargs)
    {
        /* set up an expression buffer for the result */
        js_expr_buf expr;

        /* start with an empty array */
        expr += "[]";

        /* append each element in turn */
        for (i = 1 ; i <= argc_ ; --i)
        {
            char buf[30];
            
            /* 
             *   if it's a varargs element, it's an array, so use concat() to
             *   append its members to the array under construction;
             *   otherwise use push() to add this element individually 
             */
            sprintf(buf, va[i] ? ".concat($%d)" : ".push($%d)", i);

            /* add it to the expression under construction */
            expr += buf;
        }

        /* replace the argument list with the array expression */
        G_cg->js_expr(buf.getbuf());
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   argument list entry
 */
void CTPNArg::gen_code(int, int)
{
    /* 
     *   Generate the argument expression.  We need the value (hence
     *   discard = false), and we need the assignable value (hence
     *   for_condition = false). 
     */
    get_arg_expr()->gen_code(FALSE, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   function/method call
 */

/*
 *   create 
 */
CTPNCall::CTPNCall(CTcPrsNode *func, class CTPNArglist *arglist)
    : CTPNCallBase(func, arglist)
{
}


/*
 *   generate code 
 */
void CTPNCall::gen_code(int discard, int)
{
    int varargs;
    
    /* push the argument list */
    get_arg_list()->gen_code_arglist(&varargs);

    /* generate an appropriate call instruction */
    get_func()->gen_code_call(discard, get_arg_list()->get_argc(),
                              varargs);
}

/*
 *   Generate code for operator 'new'.  A 'new' with an argument list
 *   looks like a function call: NEW(CALL(object-contents, ARGLIST(...))).
 */
void CTPNCall::gen_code_new(int discard, int argc, int varargs,
                            int from_call, int is_transient)
{
    /* 
     *   if this is a recursive call from another 'call' node, it's not
     *   allowed - we'd be trying to use the result of a call as the base
     *   class of the 'new', which is illegal 
     */
    if (from_call)
    {
        G_tok->log_error(TCERR_INVAL_NEW_EXPR);
        return;
    }
    
    /* generate the argument list */
    get_arg_list()->gen_code_arglist(&varargs);

    /* generate the code for the 'new' call */
    get_func()->gen_code_new(discard, get_arg_list()->get_argc(), varargs,
                             TRUE, is_transient);
}


/* ------------------------------------------------------------------------ */
/*
 *   member property evaluation
 */
void CTPNMember::gen_code(int discard, int)
{
    /* ask the object expression to generate the code */
    get_obj_expr()->gen_code_member(discard, get_prop_expr(), prop_is_expr_,
                                    0, FALSE);
}

/*
 *   assign to member expression
 */
int CTPNMember::gen_code_asi(int discard, tc_asitype_t typ, CTcPrsNode *rhs,
                             int ignore_errors)
{
    int is_self;
    vm_obj_id_t obj;
    vm_prop_id_t prop;

    /* 
     *   if it's not a simple assignment, tell the caller to generate the
     *   generic code to compute the composite value, and then call us
     *   again for a simple assignment 
     */
    if (typ != TC_ASI_SIMPLE)
        return FALSE;

    /* generate the right-hand side, unless the caller has already done so */
    if (rhs != 0)
        rhs->gen_code(FALSE, FALSE);
    
    /* 
     *   if the caller wants to use the assigned value, push a copy --
     *   we'll consume one copy in the SETPROP or related instruction, so
     *   we'll need another copy for the caller 
     */
    if (!discard)
    {
        G_cg->write_op(OPC_DUP);
        G_cg->note_push();
    }

    /* 
     *   Determine what we have on the left: we could have self, a
     *   constant object value, or any other expression.  
     */
    obj = get_obj_expr()->gen_code_obj_predot(&is_self);

    /* 
     *   determine what kind of property expression we have - don't
     *   generate any code for now, since we may need to generate some
     *   more code ahead of the property generation 
     */
    prop = get_prop_expr()->gen_code_propid(TRUE, prop_is_expr_);

    /* determine what we need to do based on the operands */
    if (prop == VM_INVALID_PROP)
    {
        /* 
         *   We're assigning through a property pointer -- we must
         *   generate a PTRSETPROP instruction.
         *   
         *   Before we generate the property expression, we must generate
         *   the object expression.  If we got a constant object, we must
         *   generate code to push that object value; otherwise, the code
         *   to generate the object value is already generated. 
         */
        if (is_self)
        {
            /* self - generate code to push the "self" value */
            G_cg->write_op(OPC_PUSHSELF);
            G_cg->note_push();
        }
        else if (obj != VM_INVALID_OBJ)
        {
            /* constant object - generate code to push the value */
            G_cg->write_op(OPC_PUSHOBJ);
            G_cs->write_obj_id(obj);
            G_cg->note_push();
        }

        /* generate the property value expression */
        get_prop_expr()->gen_code_propid(FALSE, prop_is_expr_);

        /* generate the PTRSETPROP instruction */
        G_cg->write_op(OPC_PTRSETPROP);

        /* ptrsetprop removes three elements */
        G_cg->note_pop(3);
    }
    else
    {
        /* 
         *   We have a constant property value, so we have several
         *   instructions to choose from.  If we're assigning to a
         *   property of "self", use SETPROPSELF.  If we're assigning to a
         *   constant object, use OBJSETPROP.  Otherwise, use the plain
         *   SETPROP. 
         */
        if (is_self)
        {
            /* write the SETPROPSELF */
            G_cg->write_op(OPC_SETPROPSELF);
            G_cs->write_prop_id(prop);

            /* setpropself removes the value */
            G_cg->note_pop();
        }
        else if (obj != VM_INVALID_OBJ)
        {
            /* write the OBJSETPROP */
            G_cg->write_op(OPC_OBJSETPROP);
            G_cs->write_obj_id(obj);
            G_cs->write_prop_id(prop);

            /* objsetprop removes the value */
            G_cg->note_pop();
        }
        else
        {
            /* 
             *   write the normal SETPROP; we already generated the code
             *   to push the object value, so it's where it should be 
             */
            G_cg->write_op(OPC_SETPROP);
            G_cs->write_prop_id(prop);

            /* setprop removes the value and the object */
            G_cg->note_pop(2);
        }
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   member with argument list
 */
void CTPNMemArg::gen_code(int discard, int)
{
    int varargs;
    
    /* push the argument list */
    get_arg_list()->gen_code_arglist(&varargs);

    /* ask the object expression to generate the code */
    get_obj_expr()->gen_code_member(discard, get_prop_expr(), prop_is_expr_,
                                    get_arg_list()->get_argc(),
                                    varargs);
}

/* ------------------------------------------------------------------------ */
/*
 *   construct a list
 */
void CTPNList::gen_code(int discard, int for_condition)
{
    CTPNListEle *ele;
    
    /*
     *   Before we construct the list dynamically, check to see if the
     *   list is constant.  If it is, we need only built the list in the
     *   constant pool, and push its offset.  
     */
    if (is_const())
    {
        /* push the value only if we're not discarding it */
        if (!discard)
        {
            /* write the instruction */
            G_cg->write_op(OPC_PUSHLST);

            /* add the list to the constant pool */
            G_cg->add_const_list(this, G_cs, G_cs->get_ofs());

            /* 
             *   write a placeholder address, which will be corrected by
             *   the fixup that add_const_list() created 
             */
            G_cs->write4(0);

            /* note the push */
            G_cg->note_push();
        }

        /* done */
        return;
    }

    /*
     *   It's not a constant list, so we must generate code to construct a
     *   list dynamically.  Push each element of the list.  We need each
     *   value (hence discard = false), and we require the assignable
     *   value of each expression (hence for_condition = false).  Push the
     *   argument list in reverse order, since the run-time metaclass
     *   requires this ordering.  
     */
    for (ele = get_tail() ; ele != 0 ; ele = ele->get_prev())
        ele->gen_code(FALSE, FALSE);

    /* generate a NEW instruction for an object of metaclass LIST */
    if (get_count() <= 255)
    {
        /* the count will fit in one byte - use the short form */
        G_cg->write_op(OPC_NEW1);
        G_cs->write((char)get_count());
        G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_LIST));
    }
    else
    {
        /* count doesn't fit in one byte - use the long form */
        G_cg->write_op(OPC_NEW2);
        G_cs->write2(get_count());
        G_cs->write2(G_cg->get_predef_meta_idx(TCT3_METAID_LIST));
    }
    
    /* new1/new2 remove arguments */
    G_cg->note_pop(get_count());

    /* if we're not discarding the value, push it */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   list element
 */
void CTPNListEle::gen_code(int discard, int for_condition)
{
    /* generate the subexpression */
    expr_->gen_code(discard, for_condition);
}

/* ------------------------------------------------------------------------ */
/*
 *   Basic T3-specific symbol class 
 */

/*
 *   generate code to take the address of a symbol - in general, we cannot
 *   take the address of a symbol, so we'll just log an error
 */
void CTcSymbol::gen_code_addr()
{
    G_tok->log_error(TCERR_NO_ADDR_SYM, (int)get_sym_len(), get_sym());
}

/*
 *   generate code to assign to the symbol - in general, we cannot assign
 *   to a symbol, so we'll just log an error 
 */
int CTcSymbol::gen_code_asi(int, tc_asitype_t, class CTcPrsNode *,
                            int ignore_error)
{
    /* 
     *   if we're ignoring errors, simply return false to indicate that
     *   nothing happened 
     */
    if (ignore_error)
        return FALSE;

    /* log the error */
    G_tok->log_error(TCERR_CANNOT_ASSIGN_SYM, (int)get_sym_len(), get_sym());

    /* 
     *   even though we didn't generate anything, this has been fully
     *   handled - the caller shouldn't attempt to generate any additional
     *   code for this 
     */
    return TRUE;
}

/*
 *   Generate code for calling the symbol.  By default, we can't call a
 *   symbol. 
 */
void CTcSymbol::gen_code_call(int, int, int)
{
    /* log an error */
    G_tok->log_error(TCERR_CANNOT_CALL_SYM, (int)get_sym_len(), get_sym());
}

/*
 *   Generate code for operator 'new' 
 */
void CTcSymbol::gen_code_new(int, int, int, int)
{
    G_tok->log_error(TCERR_INVAL_NEW_EXPR);
}

/* 
 *   evaluate a property ID 
 */
vm_prop_id_t CTcSymbol::gen_code_propid(int check_only, int is_expr)
{
    /* by default, a symbol cannot be used as a property ID */
    if (!check_only)
        G_tok->log_error(TCERR_SYM_NOT_PROP, (int)get_sym_len(), get_sym());

    /* we can't return a valid property ID */
    return VM_INVALID_PROP;
}

/*
 *   evaluate a member expression 
 */
void CTcSymbol::gen_code_member(int discard,
                                CTcPrsNode *prop_expr, int prop_is_expr,
                                int argc, int varargs)
{
    /* by default, a symbol cannot be used as an object expression */
    G_tok->log_error(TCERR_SYM_NOT_OBJ, (int)get_sym_len(), get_sym());
}

/*
 *   generate code for an object expression before a '.' 
 */
vm_obj_id_t CTcSymbol::gen_code_obj_predot(int *is_self)
{
    /* by default, a symbol cannot be used as an object expression */
    G_tok->log_error(TCERR_SYM_NOT_OBJ, (int)get_sym_len(), get_sym());

    /* indicate that we don't have a constant object */
    *is_self = FALSE;
    return VM_INVALID_OBJ;
}



/* ------------------------------------------------------------------------ */
/*
 *   T3-specific function symbol class 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymFunc::gen_code(int discard)
{
    /* 
     *   function address are always unknown during code generation;
     *   generate a placeholder instruction and add a fixup record for it 
     */
    G_cg->write_op(OPC_PUSHFNPTR);

    /* add a fixup for the current code location */
    add_abs_fixup(G_cs);

    /* write a placeholder offset - arbitrarily use zero */
    G_cs->write4(0);

    /* note the push */
    G_cg->note_push();
}

/*
 *   take the address of the function 
 */
void CTcSymFunc::gen_code_addr()
{
    /* 
     *   the address of a function cannot be taken - using the name alone
     *   yields the address 
     */
    G_tok->log_error(TCERR_INVAL_FUNC_ADDR, (int)get_sym_len(), get_sym());
}


/*
 *   call the symbol 
 */
void CTcSymFunc::gen_code_call(int discard, int argc, int varargs)
{
    /*
     *   If this is a multi-method, a call to the function is actually a call
     *   to _multiMethodCall('name', args).  
     */
    if (is_multimethod_)
    {
        /* make a list out of the arguments */
        if (varargs)
        {
            G_cg->write_op(OPC_VARARGC);
            G_cs->write((char)argc);
        }
        else if (argc <= 255)
        {
            G_cg->write_op(OPC_NEW1);
            G_cs->write((char)argc);
        }
        else
        {
            G_cg->write_op(OPC_NEW2);
            G_cs->write2(argc);
        }
        G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_LIST));
        G_cg->note_pop(argc);

        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();

        /* add the base function pointer argument */
        CTcConstVal funcval;
        funcval.set_funcptr(this);
        CTPNConst func(&funcval);
        func.gen_code(FALSE, FALSE);

        /* look up _multiMethodCall */
        static const char mmc_name[] = "_multiMethodCall";
        static const size_t mmc_len = sizeof(mmc_name) - 1;
        CTcSymFunc *mmc = (CTcSymFunc *)G_prs->get_global_symtab()->find(
            mmc_name, mmc_len);
        if (mmc == 0)
        {
            /* it's not defined - add an implied declaration for it */
            mmc = new CTcSymFunc(mmc_name, mmc_len, FALSE, 1, TRUE, TRUE,
                                 FALSE, FALSE, TRUE);
            G_prs->get_global_symtab()->add_entry(mmc);
        }
        else if(mmc->get_type() != TC_SYM_FUNC)
        {
            /* it's defined, but not as a function - this is an error */
            G_tok->log_error(TCERR_REDEF_AS_FUNC, (int)mmc_len, mmc_name);
            return;
        }

        /* 
         *   Generate the call.  Note that there are always two arguments at
         *   this point: the base function pointer, and the argument list.
         *   The argument list is just one argument because we've already
         *   constructed a list out of it.  
         */
        mmc->gen_code_call(discard, 2, FALSE);
    }
    else
    {
        /* write the varargs modifier if appropriate */
        if (varargs)
            G_cg->write_op(OPC_VARARGC);

        /* generate the call instruction and argument count */
        G_cg->write_op(OPC_CALL);
        G_cs->write((char)argc);

        /* check the mode */
        if (G_cg->is_eval_for_debug())
        {
            /* 
             *   debugger expression compilation - we know the absolute
             *   address already, since all symbols are pre-resolved in the
             *   debugger 
             */
            G_cs->write4(get_code_pool_addr());
        }
        else
        {
            /* 
             *   Normal compilation - we won't know the function's address
             *   until after generation is completed, so add a fixup for the
             *   current location, then write a placeholder for the offset
             *   field.  
             */
            add_abs_fixup(G_cs);
            G_cs->write4(0);
        }

        /* call removes arguments */
        G_cg->note_pop(argc);

        /* make sure the argument count is correct */
        if (varargs_ ? argc < argc_ : argc != argc_)
            G_tok->log_error(TCERR_WRONG_ARGC_FOR_FUNC,
                             (int)get_sym_len(), get_sym(), argc_, argc);

        /* if we're not discarding, push the return value from R0 */
        if (!discard)
        {
            G_cg->write_op(OPC_GETR0);
            G_cg->note_push();
        }
    }
}

/*
 *   Get my code pool address.  Valid only after linking. 
 */
ulong CTcSymFunc::get_code_pool_addr() const
{
    /* check for an absolute address */
    if (abs_addr_valid_)
    {
        /* 
         *   we have an absolute address - this means the symbol was
         *   loaded from a fully-linked image file (specifically, from the
         *   debug records) 
         */
        return abs_addr_;
    }
    else
    {
        /* 
         *   we don't have an absolute address, so our address must have
         *   been determined through a linking step - get the final
         *   address from the anchor 
         */
        return anchor_->get_addr();
    }
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymFunc::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    vm_val_t val;
    
    /* add an entry for our absolute address */
    val.set_fnptr(get_code_pool_addr());
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}


/* ------------------------------------------------------------------------ */
/*
 *   T3-specific object symbol class 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymObj::gen_code(int discard)
{
    /* write code to push the object ID */
    if (!discard)
    {
        /* push the object */
        G_cg->write_op(OPC_PUSHOBJ);
        G_cs->write_obj_id(obj_id_);

        /* note the push */
        G_cg->note_push();
    }
}

/*
 *   take the address of the object
 */
void CTcSymObj::gen_code_addr()
{
    /* act as though we were pushing the object ID directly */
    gen_code(FALSE);
}

/*
 *   Generate a 'new' expression 
 */
void CTcSymObj::gen_code_new(int discard, int argc, int varargs,
                             int is_transient)
{
    /* use our static generator */
    s_gen_code_new(discard, obj_id_, metaclass_, argc, varargs, is_transient);
}

/*
 *   Generate a 'new' expression.  (This is a static method so that this
 *   code can be used by all of the possible expression types to which
 *   'new' can be applied.)
 *   
 *   This type of generation applies only to objects of metaclass TADS
 *   Object.  
 */
void CTcSymObj::s_gen_code_new(int discard, vm_obj_id_t obj_id,
                               tc_metaclass_t meta,
                               int argc, int varargs, int is_transient)
{
    /* 
     *   push the base class object - this is always the first argument
     *   (hence last pushed) to the metaclass constructor 
     */
    G_cg->write_op(OPC_PUSHOBJ);
    G_cs->write_obj_id(obj_id);

    /* note the push */
    G_cg->note_push();

    /* 
     *   note that we can only allow 126 arguments to a constructor,
     *   because we must add the implicit superclass argument 
     */
    if (argc > 126)
        G_tok->log_error(TCERR_TOO_MANY_CTOR_ARGS);

    /* 
     *   if we have varargs, swap the top stack elements to get the
     *   argument count back on top, and then generate the varargs
     *   modifier opcode 
     */
    if (varargs)
    {
        /* swap the top stack elements to get argc back to the top */
        G_cg->write_op(OPC_SWAP);

        /* 
         *   increment the argument count to account for the superclass
         *   object argument 
         */
        G_cg->write_op(OPC_INC);

        /* write the varargs modifier opcode */
        G_cg->write_op(OPC_VARARGC);
    }

    /* figure the metaclass index - the compiler can only generate known

    /* 
     *   write the NEW instruction - since we always add TADS Object to
     *   our metaclass table before we start compiling any code, we know
     *   it always has a small metaclass number and will always fit in the
     *   short form of the instruction
     *   
     *   Note that the actual argument count we generate is one higher
     *   than the source code argument list, because we add the implicit
     *   first argument to the metaclass constructor 
     */
    G_cg->write_op(is_transient ? OPC_TRNEW1 : OPC_NEW1);
    G_cs->write((char)(argc + 1));

    /* write out the dependency table index for the metaclass */
    switch (meta)
    {
    case TC_META_TADSOBJ:
        G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_TADSOBJ));
        break;

    default:
        /* we can't use 'new' on symbols of other metaclasses */
        G_tok->log_error(TCERR_BAD_META_FOR_NEW);
        G_cs->write(0);
        break;
    }

    /* new1 removes the arguments */
    G_cg->note_pop(argc + 1);

    /* 
     *   if they're not discarding the value, push the new object
     *   reference, which will be in R0 when the constructor returns 
     */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   Generate code for a member expression 
 */
void CTcSymObj::gen_code_member(int discard,
                                CTcPrsNode *prop_expr, int prop_is_expr,
                                int argc, int varargs)
{
    s_gen_code_member(discard, prop_expr, prop_is_expr,
                      argc, obj_id_, varargs);
}

/*
 *   Static method to generate code for a member expression.  This is
 *   static so that constant object nodes can share it.  
 */
void CTcSymObj::s_gen_code_member(int discard,
                                  CTcPrsNode *prop_expr, int prop_is_expr,
                                  int argc, vm_obj_id_t obj_id, int varargs)
{
    vm_prop_id_t prop;

    /* 
     *   generate the property expression - don't generate the code right
     *   now even if code generation is necessary, because this isn't the
     *   right place for it; for now, simply check to determine if we're
     *   going to need to generate any code for the property expression 
     */
    prop = prop_expr->gen_code_propid(TRUE, prop_is_expr);

    /* don't allow method calls with arguments in speculative mode */
    if (argc != 0 && G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);
    
    /* check for a constant property value */
    if (prop != VM_INVALID_PROP)
    {
        /* generate an OBJGETPROP or OBJCALLPROP as appropriate */
        if (G_cg->is_speculative())
        {
            /* speculative evaluation - use GETPROPDATA */
            G_cg->write_op(OPC_PUSHOBJ);
            G_cs->write_obj_id(obj_id);
            G_cg->write_op(OPC_GETPROPDATA);
            G_cs->write_prop_id(prop);

            /* we pushed the object, then popped it */
            G_cg->note_push();
            G_cg->note_pop();
        }
        else if (argc == 0)
        {
            /* no arguments - use OBJGETPROP */
            G_cg->write_op(OPC_OBJGETPROP);
            G_cs->write_obj_id(obj_id);
            G_cs->write_prop_id(prop);
        }
        else
        {
            /* generate a varargs modifier if needed */
            if (varargs)
                G_cg->write_op(OPC_VARARGC);
            
            /* arguments - use OBJCALLPROP */
            G_cg->write_op(OPC_OBJCALLPROP);
            G_cs->write((char)argc);
            G_cs->write_obj_id(obj_id);
            G_cs->write_prop_id(prop);

            /* objcallprop removes arguments */
            G_cg->note_pop(argc);
        }
    }
    else
    {
        /* 
         *   non-constant property value - we must first push the object
         *   value, then push the property value, then write a PTRCALLPROP
         *   instruction 
         */

        /* generate the object push */
        G_cg->write_op(OPC_PUSHOBJ);
        G_cs->write_obj_id(obj_id);

        /* note the pushes */
        G_cg->note_push();

        /* keep the argument counter on top if necessary */
        if (varargs)
            G_cg->write_op(OPC_SWAP);

        /* generate the property push */
        prop_expr->gen_code_propid(FALSE, prop_is_expr);

        /* generate the PTRCALLPROP or PTRGETPROPDATA */
        if (G_cg->is_speculative())
        {
            /* speculative - use the data-only property evaluation */
            G_cg->write_op(OPC_PTRGETPROPDATA);
        }
        else
        {
            /* 
             *   if we have a varargs list, modify the call instruction
             *   that follows to make it a varargs call 
             */
            if (varargs)
            {
                /* swap to get the arg counter back on top */
                G_cg->write_op(OPC_SWAP);
                
                /* write the varargs modifier */
                G_cg->write_op(OPC_VARARGC);
            }
            
            /* normal - call the property */
            G_cg->write_op(OPC_PTRCALLPROP);
            G_cs->write((int)argc);
        }

        /* ptrcallprop removes the arguments, the object, and the property */
        G_cg->note_pop(argc + 2);
    }

    /* if they want the result, push it onto the stack */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   generate code for an object before a '.'  
 */
vm_obj_id_t CTcSymObj::gen_code_obj_predot(int *is_self)
{
    /* return our constant object reference */
    *is_self = FALSE;
    return obj_id_;
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymObj::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    vm_val_t val;

    /* add our entry */
    val.set_obj(obj_id_);
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}

/* ------------------------------------------------------------------------ */
/*
 *   T3-specific property symbol class 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymProp::gen_code(int discard)
{
    /* 
     *   Evaluating a property is equivalent to calling the property on
     *   the "self" object with no arguments.  If there's no "self"
     *   object, an unqualified property evaluation is not possible, so
     *   log an error if this is the case.  
     */
    if (!G_cs->is_self_available())
    {
        G_tok->log_error(TCERR_PROP_NEEDS_OBJ, (int)get_sym_len(), get_sym());
        return;
    }

    if (G_cg->is_speculative())
    {
        /* push 'self', then evaluate the property in data-only mode */
        G_cg->write_op(OPC_PUSHSELF);
        G_cg->write_op(OPC_GETPROPDATA);
        G_cs->write_prop_id(prop_);

        /* we pushed the 'self' value then popped it again */
        G_cg->note_push();
        G_cg->note_pop();
    }
    else
    {
        /* generate the call to 'self' */
        G_cg->write_op(OPC_GETPROPSELF);
        G_cs->write_prop_id(prop_);
    }

    /* if they're not discarding the value, push the result */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   evaluate a member expression 
 */
void CTcSymProp::gen_code_member(int discard,
                                 CTcPrsNode *prop_expr, int prop_is_expr,
                                 int argc, int varargs)
{
    /* generate code to evaluate the property */
    gen_code(FALSE);

    /* if we have an argument counter, put it back on top */
    if (varargs)
        G_cg->write_op(OPC_SWAP);

    /* use the standard member generation */
    CTcPrsNode::s_gen_member_rhs(discard, prop_expr, prop_is_expr,
                                 argc, varargs);
}

/*
 *   take the address of the property
 */
void CTcSymProp::gen_code_addr()
{
    /* write code to push the property ID */
    G_cg->write_op(OPC_PUSHPROPID);
    G_cs->write_prop_id(prop_);

    /* note the push */
    G_cg->note_push();
}

/*
 *   assign to a property, implicitly of the "self" object 
 */
int CTcSymProp::gen_code_asi(int discard, tc_asitype_t typ,
                             class CTcPrsNode *rhs, int /*ignore_errors*/)
{
    /* if there's no "self" object, we can't make this assignment */
    if (!G_cs->is_self_available())
    {
        /* log an error */
        G_tok->log_error(TCERR_SETPROP_NEEDS_OBJ,
                         (int)get_sym_len(), get_sym());

        /* 
         *   indicate that we're finished, since there's nothing more we
         *   can do here 
         */
        return TRUE;
    }

    /* 
     *   if it's not a simple assignment, tell the caller to do the
     *   composite work and get back to us with the value to store 
     */
    if (typ != TC_ASI_SIMPLE)
        return FALSE;

    /* 
     *   generate the right-hand side's expression for assignment, unless
     *   the caller has already done so 
     */
    if (rhs != 0)
        rhs->gen_code(FALSE, FALSE);

    /* 
     *   if we're not discarding the value, make a copy - we'll consume a
     *   copy in the SETPROP instruction, so we need one more copy to
     *   return to the enclosing expression 
     */
    if (!discard)
    {
        G_cg->write_op(OPC_DUP);
        G_cg->note_push();
    }

    /* 
     *   write the SETPROP instruction - use the special form to assign to
     *   "self" 
     */
    G_cg->write_op(OPC_SETPROPSELF);
    G_cs->write_prop_id(prop_);

    /* setpropself removes the value */
    G_cg->note_pop();

    /* handled */
    return TRUE;
}

/*
 *   call the symbol 
 */
void CTcSymProp::gen_code_call(int discard, int argc, int varargs)
{
    /* 
     *   if there's no "self", we can't invoke a property without an
     *   explicit object reference 
     */
    if (!G_cs->is_self_available())
    {
        G_tok->log_error(TCERR_PROP_NEEDS_OBJ, (int)get_sym_len(), get_sym());
        return;
    }

    /* don't allow calling with arguments in speculative mode */
    if (argc != 0 && G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* generate code to invoke the property of "self" */
    if (G_cg->is_speculative())
    {
        /* push 'self', then get the property in data-only mode */
        G_cg->write_op(OPC_PUSHSELF);
        G_cg->write_op(OPC_GETPROPDATA);
        G_cs->write_prop_id(get_prop());

        /* we pushed 'self' then popped it again */
        G_cg->note_push();
        G_cg->note_pop();
    }
    else if (argc == 0)
    {
        /* use the instruction with no arguments */
        G_cg->write_op(OPC_GETPROPSELF);
        G_cs->write_prop_id(get_prop());
    }
    else
    {
        /* write the varargs modifier if appropriate */
        if (varargs)
            G_cg->write_op(OPC_VARARGC);
        
        /* use the instruction with arguments */
        G_cg->write_op(OPC_CALLPROPSELF);
        G_cs->write((char)argc);
        G_cs->write_prop_id(get_prop());

        /* callpropself removes arguments */
        G_cg->note_pop(argc);
    }

    /* if we're not discarding, push the return value from R0 */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   generate a property ID expression 
 */
vm_prop_id_t CTcSymProp::gen_code_propid(int check_only, int is_expr)
{
    /*
     *   If I'm to be treated as an expression (which indicates that the
     *   property symbol is explicitly enclosed in parentheses in the
     *   original source code expression), then I must evaluate this
     *   property of self.  Otherwise, I yield literally the property ID. 
     */
    if (is_expr)
    {
        /* generate code unless we're only checking */
        if (!check_only)
        {
            /* evaluate this property of self */
            G_cg->write_op(OPC_GETPROPSELF);
            G_cs->write_prop_id(get_prop());

            /* leave the result on the stack */
            G_cg->write_op(OPC_GETR0);
            G_cg->note_push();
        }

        /* tell the caller to use the stack value */
        return VM_INVALID_PROP;
    }
    else
    {
        /* simple '.prop' - return my property ID as a constant value */
        return get_prop();
    }
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymProp::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    vm_val_t val;

    /* add our entry */
    val.set_propid(get_prop());
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}

/* ------------------------------------------------------------------------ */
/*
 *   Enumerator symbol 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymEnum::gen_code(int discard)
{
    if (!discard)
    {
        /* generate code to push the enum value */
        G_cg->write_op(OPC_PUSHENUM);
        G_cs->write_enum_id(get_enum_id());

        /* note the push */
        G_cg->note_push();
    }
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymEnum::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    vm_val_t val;

    /* add our entry */
    val.set_enum(get_enum_id());
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}


/* ------------------------------------------------------------------------ */
/*
 *   T3-specific local variable/parameter symbol class 
 */

/*
 *   generate code to evaluate the symbol 
 */
void CTcSymLocal::gen_code(int discard)
{
    /* generate code to push the local, if we're not discarding it */
    if (!discard)
    {
        /* 
         *   generate as a context local if required, otherwise as an
         *   ordinary local variable 
         */
        if (is_ctx_local_)
        {
            /* generate the context array lookup */
            if (ctx_var_num_ <= 255 && get_ctx_arr_idx() <= 255)
            {
                /* we can do this whole operation with one instruction */
                G_cg->write_op(OPC_IDXLCL1INT8);
                G_cs->write((uchar)ctx_var_num_);
                G_cs->write((uchar)get_ctx_arr_idx());

                /* this pushes one value */
                G_cg->note_push();
            }
            else
            {
                /* get our context array */
                s_gen_code_getlcl(ctx_var_num_, FALSE);
                
                /* get our value from the context array */
                CTPNConst::s_gen_code_int(get_ctx_arr_idx());
                G_cg->write_op(OPC_INDEX);
                
                /* the INDEX operation removes two values and pushes one */
                G_cg->note_pop();
            }
        }
        else
        {
            /* generate as an ordinary local */
            s_gen_code_getlcl(get_var_num(), is_param());
        }
    }

    /* 
     *   Mark the value as referenced, whether or not we're generating the
     *   code - the value has been logically referenced in the program
     *   even if the result of evaluating it isn't needed.  
     */
    set_val_used(TRUE);
}

/*
 *   generate code to push a local onto the stack 
 */
void CTcSymLocal::s_gen_code_getlcl(int var_num, int is_param)
{
    /* use the shortest form of the instruction that we can */
    if (var_num <= 255)
    {
        /* 8-bit local number - use the one-byte form */
        G_cg->write_op(is_param ? OPC_GETARG1 : OPC_GETLCL1);
        G_cs->write((char)var_num);
    }
    else
    {
        /* local number won't fit in 8 bits - use the two-byte form */
        G_cg->write_op(is_param ? OPC_GETARG2 : OPC_GETLCL2);
        G_cs->write2(var_num);
    }

    /* note the push */
    G_cg->note_push();
}

/*
 *   assign a value 
 */
int CTcSymLocal::gen_code_asi(int discard, tc_asitype_t typ,
                              class CTcPrsNode *rhs, int ignore_errors)
{
    int adding;
    
    /* mark the variable as having had a value assigned to it */
    set_val_assigned(TRUE);

    /* 
     *   if the assignment is anything but simple, this references the
     *   value as well 
     */
    if (typ != TC_ASI_SIMPLE)
        set_val_used(TRUE);

    /* 
     *   If this is a context variable, use standard assignment (i.e.,
     *   generate the result first, then generate a simple assignment to the
     *   variable).  Otherwise, we might be able to generate a fancy
     *   combined calculate-and-assign sequence, depending on the type of
     *   assignment calculation we're performing.
     */
    if (is_ctx_local_ && typ != TC_ASI_SIMPLE)
    {
        /* 
         *   it's a context local and it's not a simple assignment, so we
         *   can't perform any special calculate-and-assign sequence - tell
         *   the caller to calculate the full result first and then try
         *   again using simple assignment 
         */
        return FALSE;
    }

    /* 
     *   check the type of assignment - we can optimize the code
     *   generation to use more compact instruction sequences for certain
     *   types of assignments 
     */
    switch(typ)
    {
    case TC_ASI_SIMPLE:
        /* 
         *   Simple assignment to local/parameter.  Check for some special
         *   cases: when assigning a constant value of 0, 1, or nil to a
         *   local, we can generate a short instruction 
         */
        if (!is_param() && !is_ctx_local_ && rhs != 0 && rhs->is_const())
        {
            CTcConstVal *cval;

            /* get the constant value */
            cval = rhs->get_const_val();
            
            /* check for nil and 0 or 1 values */
            if (cval->get_type() == TC_CVT_NIL)
            {
                /* it's nil - generate NILLCL1 or NILLCL2 */
                if (get_var_num() <= 255)
                {
                    G_cg->write_op(OPC_NILLCL1);
                    G_cs->write((char)get_var_num());
                }
                else
                {
                    G_cg->write_op(OPC_NILLCL2);
                    G_cs->write2(get_var_num());
                }

                /* if not discarding, leave nil on the stack */
                if (!discard)
                {
                    G_cg->write_op(OPC_PUSHNIL);
                    G_cg->note_push();
                }

                /* handled */
                return TRUE;
            }
            else if (cval->get_type() == TC_CVT_INT
                     && (cval->get_val_int() == 0 
                         || cval->get_val_int() == 1))
            {
                int ival;

                /* get the integer value */
                ival = cval->get_val_int();
                
                /* 0 or 1 - generate ZEROLCLn or ONELCLn */
                if (get_var_num() <= 255)
                {
                    G_cg->write_op(ival == 0 ? OPC_ZEROLCL1 : OPC_ONELCL1);
                    G_cs->write((char)get_var_num());
                }
                else
                {
                    G_cg->write_op(ival == 0 ? OPC_ZEROLCL2 : OPC_ONELCL2);
                    G_cs->write2(get_var_num());
                }

                /* if not discarding, leave the value on the stack */
                if (!discard)
                {
                    G_cg->write_op(ival == 0 ? OPC_PUSH_0 : OPC_PUSH_1);
                    G_cg->note_push();
                }

                /* handled */
                return TRUE;
            }
        }

        /* 
         *   If we got here, we can't generate a specialized constant
         *   assignment - so, first, generate the right-hand side's value
         *   normally.  (If no 'rhs' is specified, the value is already on
         *   the stack.)  
         */
        if (rhs != 0)
            rhs->gen_code(FALSE, FALSE);

        /* leave an extra copy of the value on the stack if not discarding */
        if (!discard)
        {
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();
        }

        /* now assign the value at top of stack to the variable */
        gen_code_setlcl();

        /* handled */
        return TRUE;

    case TC_ASI_ADD:
        adding = TRUE;
        goto add_or_sub;
        
    case TC_ASI_SUB:
        adding = FALSE;

    add_or_sub:
        /* if this is a parameter, there's nothing special we can do */
        if (is_param())
            return FALSE;
        
        /* 
         *   Add/subtract to a local/parameter.  If the right-hand side is a
         *   constant integer value, we might be able to generate a special
         *   instruction to add/subtract it.  
         */
        if (rhs != 0
            && adding
            && rhs->is_const()
            && rhs->get_const_val()->get_type() == TC_CVT_INT)
        {
            long ival;

            /* get the integer value to assign */
            ival = rhs->get_const_val()->get_val_int();

            /* 
             *   if the right-hand side's integer value fits in one byte,
             *   generate the short (8-bit) instruction; otherwise,
             *   generate the long (32-bit) format 
             */
            if (ival == 1)
            {
                /* adding one - increment the local */
                G_cg->write_op(OPC_INCLCL);
                G_cs->write2(get_var_num());
            }
            else if (ival == -1)
            {
                /* subtracting one - decrement the local */
                G_cg->write_op(OPC_DECLCL);
                G_cs->write2(get_var_num());
            }
            else if (ival <= 127 && ival >= -128
                     && get_var_num() <= 255)
            {
                /* fits in 8 bits - use the 8-bit format */
                G_cg->write_op(OPC_ADDILCL1);
                G_cs->write((char)get_var_num());
                G_cs->write((char)ival);
            }
            else
            {
                /* 
                 *   either the value or the variable number doesn't fit
                 *   in 8 bits - use the 32-bit format 
                 */
                G_cg->write_op(OPC_ADDILCL4);
                G_cs->write2(get_var_num());
                G_cs->write4(ival);
            }
        }
        else
        {
            /* 
             *   We don't have a special instruction for the right side,
             *   so generate it normally and add/subtract the value.  (If
             *   there's no 'rhs' value specified, it means that the value
             *   is already on the stack, so there's nothing extra for us
             *   to generate.)  
             */
            if (rhs != 0)
                rhs->gen_code(FALSE, FALSE);
            
            /* write the ADDTOLCL instruction */
            G_cg->write_op(adding ? OPC_ADDTOLCL : OPC_SUBFROMLCL);
            G_cs->write2(get_var_num());

            /* addtolcl/subfromlcl remove the rvalue */
            G_cg->note_pop();
        }

        /* 
         *   if not discarding, push the result onto the stack; do this by
         *   simply evaluating the local, which is the simplest and most
         *   efficient way to obtain the result of the computation 
         */
        if (!discard)
            gen_code(FALSE);

        /* handled */
        return TRUE;

    case TC_ASI_PREINC:
        /* if this is a parameter, there's nothing special we can do */
        if (is_param())
            return FALSE;

        /* generate code to increment the local */
        G_cg->write_op(OPC_INCLCL);
        G_cs->write2(get_var_num());

        /* if we're not discarding, push the local's new value */
        if (!discard)
            gen_code(FALSE);

        /* handled */
        return TRUE;

    case TC_ASI_POSTINC:
        /* if this is a parameter, there's nothing special we can do */
        if (is_param())
            return FALSE;

        /* 
         *   if we're not discarding, push the local's value prior to
         *   incrementing it - this will be the result we'll leave on the
         *   stack 
         */
        if (!discard)
            gen_code(FALSE);

        /* generate code to increment the local */
        G_cg->write_op(OPC_INCLCL);
        G_cs->write2(get_var_num());

        /* handled */
        return TRUE;

    case TC_ASI_PREDEC:
        /* if this is a parameter, there's nothing special we can do */
        if (is_param())
            return FALSE;

        /* generate code to decrement the local */
        G_cg->write_op(OPC_DECLCL);
        G_cs->write2(get_var_num());

        /* if we're not discarding, push the local's new value */
        if (!discard)
            gen_code(FALSE);

        /* handled */
        return TRUE;

    case TC_ASI_POSTDEC:
        /* if this is a parameter, there's nothing special we can do */
        if (is_param())
            return FALSE;

        /* 
         *   if we're not discarding, push the local's value prior to
         *   decrementing it - this will be the result we'll leave on the
         *   stack 
         */
        if (!discard)
            gen_code(FALSE);

        /* generate code to decrement the local */
        G_cg->write_op(OPC_DECLCL);
        G_cs->write2(get_var_num());

        /* handled */
        return TRUE;

    default:
        /* we can't do anything special with other assignment types */
        return FALSE;
    }
}

/*
 *   generate code to assigin the value at top of stack to the local
 *   variable 
 */
void CTcSymLocal::gen_code_setlcl()
{
    /* check to see if we're a context local (as opposed to a stack local) */
    if (is_ctx_local_)
    {
        /* generate the assignment using the appropriate sequence */
        if (ctx_var_num_ <= 255 && get_ctx_arr_idx() <= 255)
        {
            /* we can fit this in a single instruction */
            G_cg->write_op(OPC_SETINDLCL1I8);
            G_cs->write((uchar)ctx_var_num_);
            G_cs->write((uchar)get_ctx_arr_idx());

            /* this pops the value being assigned */
            G_cg->note_pop();
        }
        else
        {
            /* get our context array */
            s_gen_code_getlcl(ctx_var_num_, FALSE);
            
            /* set our value in the context array */
            CTPNConst::s_gen_code_int(get_ctx_arr_idx());
            G_cg->write_op(OPC_SETIND);
            G_cg->write_op(OPC_DISC);
            
            /* 
             *   the SETIND pops three values and pushes one (for a net two
             *   pops), and the DISC pops one more value, so our total is
             *   three pops 
             */
            G_cg->note_pop(3);
        }
    }
    else
    {
        /* we're just a plain stack variable */
        gen_code_setlcl_stk();
    }
}

/*
 *   Generate code to store the value at the top of the stack into the given
 *   local stack slot.  Note that this routine will not work with a context
 *   local - it only works if the variable is known to be a stack variable.  
 */
void CTcSymLocal::s_gen_code_setlcl_stk(int var_num, int is_param)
{
    /* use the shortest form that will fit our variable index */
    if (var_num <= 255)
    {
        /* use the one-byte instruction */
        G_cg->write_op(is_param ? OPC_SETARG1 : OPC_SETLCL1);
        G_cs->write((char)var_num);
    }
    else
    {
        /* big number - use the two-byte instruction */
        G_cg->write_op(is_param ? OPC_SETARG2 : OPC_SETLCL2);
        G_cs->write2(var_num);
    }

    /* the setarg/setlcl ops remove the rvalue */
    G_cg->note_pop();
}

/*
 *   call the symbol 
 */
void CTcSymLocal::gen_code_call(int discard, int argc, int varargs)
{
    /* 
     *   to call a local, we'll simply evaluate the local normally, then
     *   call through the resulting (presumed) property or function
     *   pointer value 
     */
    gen_code(FALSE);

    /* 
     *   if we have a varargs list, modify the call instruction that
     *   follows to make it a varargs call 
     */
    if (varargs)
    {
        /* swap the top of the stack to get the arg counter back on top */
        G_cg->write_op(OPC_SWAP);

        /* write the varargs modifier */
        G_cg->write_op(OPC_VARARGC);
    }

    /* don't allow this at all in speculative mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);

    /* call the result as a function or method pointer */
    G_cg->write_op(OPC_PTRCALL);
    G_cs->write((char)argc);

    /* ptrcall removes the arguments and the function pointer */
    G_cg->note_pop(argc + 1);

    /* if we're not discarding the value, push the result */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/*
 *   generate a property ID expression 
 */
vm_prop_id_t CTcSymLocal::gen_code_propid(int check_only, int /*is_expr*/)
{
    /*
     *   treat the local as a property-valued expression; generate the
     *   code for the local, then tell the caller that no constant value
     *   is available, since the local's property ID value should be on
     *   the stack 
     */
    if (!check_only)
        gen_code(FALSE);

    /* tell the caller to use the stack value */
    return VM_INVALID_PROP;
}

/*
 *   evaluate a member expression 
 */
void CTcSymLocal::gen_code_member(int discard,
                                  CTcPrsNode *prop_expr, int prop_is_expr,
                                  int argc, int varargs)
{
    /* generate code to evaluate the local */
    gen_code(FALSE);

    /* if we have an argument counter, put it back on top */
    if (varargs)
        G_cg->write_op(OPC_SWAP);

    /* use the standard member generation */
    CTcPrsNode::s_gen_member_rhs(discard, prop_expr, prop_is_expr,
                                 argc, varargs);
}

/*
 *   write to a debug record 
 */
int CTcSymLocal::write_to_debug_frame()
{
    int flags;
    
    /* 
     *   write my ID - if we're a context variable, we want to write the
     *   context variable ID; otherwise write our stack location as normal 
     */
    if (is_ctx_local_)
        G_cs->write2(ctx_var_num_);
    else
        G_cs->write2(var_num_);

    /* compute my flags */
    flags = 0;
    if (is_param_)
        flags |= 1;
    if (is_ctx_local_)
        flags |= 2;

    /* write my flags */
    G_cs->write2(flags);

    /* write my local context array index */
    G_cs->write2(get_ctx_arr_idx());

    /* write the length of my symbol name */
    G_cs->write2(len_);
    G_cs->write(str_, len_);

    /* we did write this symbol */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Built-in function symbol
 */

/*
 *   Evaluate the symbol.  Invoking a built-in function without an
 *   argument list is simply a call to the built-in function with no
 *   arguments.  
 */
void CTcSymBif::gen_code(int discard)
{
    /* generate a call */
    gen_code_call(discard, 0, FALSE);
}

/*
 *   Generate code to call the built-in function 
 */
void CTcSymBif::gen_code_call(int discard, int argc, int varargs)
{
    /* don't allow calling built-in functions in speculative mode */
    if (G_cg->is_speculative())
        err_throw(VMERR_BAD_SPEC_EVAL);
    
    /* check for minimum and maximum arguments */
    if (argc < min_argc_)
    {
        G_tok->log_error(TCERR_TOO_FEW_FUNC_ARGS,
                         (int)get_sym_len(), get_sym());
    }
    else if (!varargs_ && argc > max_argc_)
    {
        G_tok->log_error(TCERR_TOO_MANY_FUNC_ARGS,
                         (int)get_sym_len(), get_sym());
    }

    /* write the varargs modifier if appropriate */
    if (varargs)
        G_cg->write_op(OPC_VARARGC);

    /* generate the call */
    if (get_func_set_id() < 4 && get_func_idx() < 256)
    {
        uchar short_ops[] =
            { OPC_BUILTIN_A, OPC_BUILTIN_B, OPC_BUILTIN_C, OPC_BUILTIN_D };
        
        /* 
         *   it's one of the first 256 functions in one of the first four
         *   function sets - we can generate a short instruction 
         */
        G_cg->write_op(short_ops[get_func_set_id()]);
        G_cs->write((char)argc);
        G_cs->write((char)get_func_idx());
    }
    else
    {
        /* it's not in the default set - use the longer instruction */
        if (get_func_idx() < 256)
        {
            /* low function index - write the short form */
            G_cg->write_op(OPC_BUILTIN1);
            G_cs->write((char)argc);
            G_cs->write((char)get_func_idx());
        }
        else
        {
            /* big function index - write the long form */
            G_cg->write_op(OPC_BUILTIN2);
            G_cs->write((char)argc);
            G_cs->write2(get_func_idx());
        }

        /* write the function set ID */
        G_cs->write((char)get_func_set_id());
    }

    /* the built-in functions always remove arguments */
    G_cg->note_pop(argc);

    /* 
     *   if they're not discarding the value, push it - the value is
     *   sitting in R0 after the call returns
     */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   External function symbol 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymExtfn::gen_code(int /*discard*/)
{
    //$$$ to be implemented
    assert(FALSE);
}

/*
 *   generate a call to the symbol
 */
void CTcSymExtfn::gen_code_call(int /*discard*/, int /*argc*/, int /*varargs*/)
{
    //$$$ to be implemented
    assert(FALSE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Code Label symbol 
 */

/*
 *   evaluate the symbol 
 */
void CTcSymLabel::gen_code(int discard)
{
    /* it's not legal to evaluate a code label; log an error */
    G_tok->log_error(TCERR_CANNOT_EVAL_LABEL,
                     (int)get_sym_len(), get_sym());
}

/* ------------------------------------------------------------------------ */
/*
 *   Metaclass symbol 
 */

/*
 *   generate code for evaluating the symbol 
 */
void CTcSymMetaclass::gen_code(int discard)
{
    /* 
     *   the metaclass name refers to the IntrinsicClass instance
     *   associated with the metaclass 
     */
    G_cg->write_op(OPC_PUSHOBJ);
    G_cs->write_obj_id(class_obj_);

    /* note the push */
    G_cg->note_push();
}

/*
 *   generate code for operator 'new' applied to the metaclass 
 */
void CTcSymMetaclass::gen_code_new(int discard, int argc, int varargs,
                                   int is_transient)
{
    /* if we have varargs, write the modifier */
    if (varargs)
        G_cg->write_op(OPC_VARARGC);
    
    if (meta_idx_ <= 255 && argc <= 255)
    {
        G_cg->write_op(is_transient ? OPC_TRNEW1 : OPC_NEW1);
        G_cs->write((char)argc);
        G_cs->write((char)meta_idx_);
    }
    else
    {
        G_cg->write_op(is_transient ? OPC_TRNEW1 : OPC_NEW2);
        G_cs->write2(argc);
        G_cs->write2(meta_idx_);
    }

    /* new1/new2 remove arguments */
    G_cg->note_pop(argc);

    /* if we're not discarding the value, push it */
    if (!discard)
    {
        G_cg->write_op(OPC_GETR0);
        G_cg->note_push();
    }
}

/* 
 *   generate a member expression 
 */
void CTcSymMetaclass::gen_code_member(int discard, CTcPrsNode *prop_expr,
                                      int prop_is_expr,
                                      int argc, int varargs)
{
    /* generate code to push our class object onto the stack */
    gen_code(FALSE);

    /* if we have an argument counter, put it back on top */
    if (varargs)
        G_cg->write_op(OPC_SWAP);

    /* use the standard member generation */
    CTcPrsNode::s_gen_member_rhs(discard, prop_expr, prop_is_expr,
                                 argc, varargs);
}

/*
 *   add a runtime symbol table entry 
 */
void CTcSymMetaclass::add_runtime_symbol(CVmRuntimeSymbols *symtab)
{
    vm_val_t val;

    /* add our entry */
    val.set_obj(get_class_obj());
    symtab->add_sym(get_sym(), get_sym_len(), &val);
}


/* ------------------------------------------------------------------------ */
/*
 *   Exception Table 
 */

/*
 *   create 
 */
CTcT3ExcTable::CTcT3ExcTable()
{
    /* allocate an initial table */
    exc_alloced_ = 1024;
    table_ = (CTcT3ExcEntry *)t3malloc(exc_alloced_ * sizeof(table_[0]));

    /* no entries are in use yet */
    exc_used_ = 0;

    /* method offset is not yet known */
    method_ofs_ = 0;
}


/*
 *   add an entry to our table 
 */
void CTcT3ExcTable::add_catch(ulong protected_start_ofs,
                              ulong protected_end_ofs,
                              ulong exc_obj_id, ulong catch_block_ofs)
{
    CTcT3ExcEntry *entry;

    /* if necessary, expand our table */
    if (exc_used_ == exc_alloced_)
    {
        /* expand the table a bit */
        exc_alloced_ += 1024;

        /* reallocate the table at the larger size */
        table_ = (CTcT3ExcEntry *)
                 t3realloc(table_, exc_alloced_ * sizeof(table_[0]));
    }

    /* 
     *   set up the new entry - store the offsets relative to the method
     *   header start address 
     */
    entry = table_ + exc_used_;
    entry->start_ofs = protected_start_ofs - method_ofs_;
    entry->end_ofs = protected_end_ofs - method_ofs_;
    entry->exc_obj_id = exc_obj_id;
    entry->catch_ofs = catch_block_ofs - method_ofs_;

    /* consume the new entry */
    ++exc_used_;
}

/*
 *   write our exception table to the code stream 
 */
void CTcT3ExcTable::write_to_code_stream()
{
    CTcT3ExcEntry *entry;
    size_t i;

    /* write the number of entries as a UINT2 */
    G_cs->write2(exc_used_);

    /* write the entries */
    for (i = 0, entry = table_ ; i < exc_used_ ; ++i, ++entry)
    {
        /* write this entry */
        G_cs->write2(entry->start_ofs);
        G_cs->write2(entry->end_ofs);
        G_cs->write_obj_id(entry->exc_obj_id);
        G_cs->write2(entry->catch_ofs);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Code body 
 */

/*
 *   generate code 
 */
void CTPNCodeBody::gen_code(int, int)
{
    CTcCodeBodyCtx *cur_ctx;
    int ctx_idx;
    tct3_method_gen_ctx gen_ctx;

    /* if I've been replaced, don't bother generating any code */
    if (replaced_)
        return;

    /* 
     *   Open the method header.
     *   
     *   Generate to the static stream if this is a static initializer
     *   method, otherwise to the main stream. 
     *   
     *   Anchor the fixups in the associated symbol table entry, if any.  We
     *   maintain our own fixup list if we don't have a symbol, otherwise we
     *   use the one from our symbol table entry - in either case, we have to
     *   keep track of it ourselves, because a code body might be reachable
     *   through multiple references (a function, for example, has a global
     *   symbol table entry - fixups referencing us might already have been
     *   created by the time we generate our code).  
     */
    G_cg->open_method(is_static_ ? G_cs_static : G_cs_main,
                      fixup_owner_sym_, fixup_list_anchor_,
                      this, gototab_,
                      argc_, varargs_, is_constructor_, self_valid_,
                      &gen_ctx);

    /* 
     *   Add each local symbol table enclosing the code body's primary
     *   local symbol table to the frame list.  The outermost code body
     *   table can be outside the primary code body table for situations
     *   such as anonymous functions.  Since these tables are outside of
     *   any statements, we must explicitly add them to ensure that they
     *   are assigned debugging frame ID's and are written to the debug
     *   data.
     */
    if (lcltab_ != 0)
    {
        CTcPrsSymtab *tab;

        /* add each frame outside the primary frame to the code gen list */
        for (tab = lcltab_->get_parent() ; tab != 0 ; tab = tab->get_parent())
            G_cs->set_local_frame(tab);
    }

    /* the method's local symbol table is now the active symbol table */
    G_cs->set_local_frame(lcltab_);

    /* if we have a local context, initialize it */
    if (has_local_ctx_)
    {
        /* write code to create the new Vector to store the context locals */
        CTPNConst::s_gen_code_int(local_ctx_arr_size_);
        G_cg->write_op(OPC_DUP);
        G_cg->write_op(OPC_NEW1);
        G_cs->write(2);
        G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_VECTOR));

        /* retrieve the object value */
        G_cg->write_op(OPC_GETR0);

        /*
         *   we duplicated the vector size argument, then we popped it and
         *   pushed the object; so we have a maximum of one extra push and a
         *   net of zero 
         */
        G_cg->note_push();
        G_cg->note_pop();

        /* store the new object in the context local variable */
        CTcSymLocal::s_gen_code_setlcl_stk(local_ctx_var_, FALSE);

        /* 
         *   go through our symbol table, and copy each parameter that's
         *   also a context local into its context local slot 
         */
        if (lcltab_ != 0)
            lcltab_->enum_entries(&enum_for_param_ctx, this);
    }

    /* 
     *   If we have a varargs-list parameter, generate the code to set up
     *   the list value from the actual parameters.  Note that we must do
     *   this after we set up the local context, in case the varargs list
     *   parameter variable is a context local, in which case it will need
     *   to be stored in the context, in which case we need the context to
     *   be initialized first.  
     */
    if (varargs_list_)
    {
        /* generate the PUSHPARLST instruction to create the list */
        G_cg->write_op(OPC_PUSHPARLST);
        G_cs->write((uchar)argc_);

        /* 
         *   we pushed at least one value (the list); we don't know how many
         *   others we might have pushed, but it doesn't matter because the
         *   interpreter is responsible for checking for stack space 
         */
        G_cg->note_push();

        /* store the list in our varargs parameter list local */
        varargs_list_local_->gen_code_setlcl();
    }

    /* 
     *   Generate code to initialize each enclosing-context-pointer local -
     *   these variables allow us to find the context objects while we're
     *   running inside this function.
     *   
     *   We *have to* generate context level 1 last.  Context level 1 does a
     *   set-self to re-establish the method context (if there is one), and
     *   once we've changed to the method context 'self', we can no longer
     *   access the anonymous function pointer context, which is in 'self'
     *   until we change it.  So, we have to wait and do level 1 last, so
     *   that we're completely done with the anonymous function context
     *   before we lose it.
     *   
     *   To ensure we generate level 1 last, make two passes: in the first
     *   pass, generate everything except level 1; on the second pass,
     *   generate only level 1.  This two-pass approach guarantees that level
     *   1 will be the last one generated, regardless of where it appears in
     *   the list.  (We can't just rearrange the list - not easily, at least
     *   - because the list is in order of function object ('self') context
     *   slot index.)  
     */
    for (int ctx_pass = 1 ; ctx_pass <= 2 ; ++ctx_pass)
    {
        /* loop over each context entry */
        for (ctx_idx = 0, cur_ctx = ctx_head_ ; cur_ctx != 0 ;
             cur_ctx = cur_ctx->nxt_, ++ctx_idx)
        {
            /* 
             *   Context level 1 *must* be generated last.  If we're on pass
             *   1 and this is level 1, skip it for now; if we're on pass 2,
             *   skip everything *except* level 1. 
             */
            if ((ctx_pass == 1 && cur_ctx->level_ == 1)
                || (ctx_pass == 2 && cur_ctx->level_ != 1))
                continue;
            
            /* 
             *   Get this context value, stored in the function object
             *   ('self') at index value 2+n (n=0,1,...).  Note that the
             *   context object indices start at 2 because the code pointer
             *   for the function is at index 1.  
             */
            G_cg->write_op(OPC_PUSHSELF);
            CTPNConst::s_gen_code_int(ctx_idx + 2);
            G_cg->write_op(OPC_INDEX);

            /* 
             *   we pushed the object, then popped the object and index and
             *   pushed the indexed value - this is a net of no change with
             *   one maximum push 
             */
            G_cg->note_push();
            G_cg->note_pop();

            /*
             *   If this is context level 1, and this context has a 'self',
             *   and we need either 'self' or the full method context from
             *   the lexically enclosing scope, generate code to load the
             *   self or the full method context (as appropriate) from our
             *   local context.
             *   
             *   The enclosing method context is always stored in the context
             *   at level 1, because this is inherently shared context for
             *   all enclosed lexical scopes.  We thus only have to worry
             *   about this for context level 1.  
             */
            if (cur_ctx->level_ == 1
                && self_valid_
                && (self_referenced_ || full_method_ctx_referenced_))
            {
                CTPNCodeBody *outer;
                
                /* 
                 *   we just put our context object on the stack in
                 *   preparation for storing it - make a duplicate copy of it
                 *   for our own purposes 
                 */
                G_cg->write_op(OPC_DUP);
                G_cg->note_push();
                
                /* get the saved method context from the context object */
                CTPNConst::s_gen_code_int(TCPRS_LOCAL_CTX_METHODCTX);
                G_cg->write_op(OPC_INDEX);
                
                /* 
                 *   Load the context.  We must check the outermost context
                 *   to determine what it stored, because we must load
                 *   whatever it stored.  
                 */
                if ((outer = get_outermost_enclosing()) != 0
                    && outer->local_ctx_needs_full_method_ctx())
                {
                    /* load the full method context */
                    G_cg->write_op(OPC_LOADCTX);
                }
                else
                {
                    /* load the 'self' object */
                    G_cg->write_op(OPC_SETSELF);
                }
                
                /* 
                 *   we popped two values and pushed one in the INDEX, then
                 *   popped a value in the LOADCTX or SETSELF: the net is
                 *   removal of two elements and no additional maximum depth 
                 */
                G_cg->note_pop(2);
            }

            /* store the context value in the appropriate local variable */
            CTcSymLocal::s_gen_code_setlcl_stk(cur_ctx->var_num_, FALSE);

            /* 
             *   if we just did context level 1, and this is pass 2, we're
             *   done - pass 2's only function is to do level 1, so once we
             *   reach it, there's nothing left to do 
             */
            if (ctx_pass == 2 && cur_ctx->level_ == 1)
                break;
        }
    }

    /* 
     *   if we created our own local context, and we have a 'self' object,
     *   and we need access to the 'self' object or the full method context
     *   from anonymous functions that refer to the local context, generate
     *   code to store the appropriate data in the local context 
     */
    if (has_local_ctx_ && self_valid_
        && (local_ctx_needs_self_ || local_ctx_needs_full_method_ctx_))
    {
        /* check to see what we need */
        if (local_ctx_needs_full_method_ctx_)
        {
            /* 
             *   we need the full method context - generate code to store it
             *   and push a reference to it onto the stack 
             */
            G_cg->write_op(OPC_STORECTX);
        }
        else
        {
            /* we only need 'self' - push it */
            G_cg->write_op(OPC_PUSHSELF);
        }

        /* we just pushed one value */
        G_cg->note_push();

        /* assign the value to the context variable */
        if (local_ctx_var_ <= 255 && TCPRS_LOCAL_CTX_METHODCTX <= 255)
        {
            /* we can make the assignment with a single instruction */
            G_cg->write_op(OPC_SETINDLCL1I8);
            G_cs->write((uchar)local_ctx_var_);
            G_cs->write(TCPRS_LOCAL_CTX_METHODCTX);

            /* that pops one value */
            G_cg->note_pop();
        }
        else
        {
            /* get the context object */
            CTcSymLocal::s_gen_code_getlcl(local_ctx_var_, FALSE);
            
            /* store the data in the local context object */
            CTPNConst::s_gen_code_int(TCPRS_LOCAL_CTX_METHODCTX);
            G_cg->write_op(OPC_SETIND);

            /* discard the indexed result */
            G_cg->write_op(OPC_DISC);
        
            /* 
             *   the SETIND pops three values and pushes one, then we pop one
             *   more with the DISC - this is a net three pops with no extra
             *   maximum depth 
             */
            G_cg->note_pop(3);
        }
    }

    /* generate the compound statement, if we have one */
    if (stm_ != 0)
        stm_->gen_code(TRUE, TRUE);

#ifdef T3_DEBUG
    if (G_cg->get_sp_depth() != 0)
    {
        printf("---> stack depth is %d after block codegen!\n",
               G_cg->get_sp_depth());
        if (fixup_owner_sym_ != 0)
            printf("---> code block for %.*s\n",
                   (int)fixup_owner_sym_->get_sym_len(),
                   fixup_owner_sym_->get_sym());
    }
#endif

    /* close the method */
    G_cg->close_method(local_cnt_, end_desc_, end_linenum_, &gen_ctx);

    /* remember the head of the nested symbol table list */
    first_nested_symtab_ = G_cs->get_first_frame();

    /* generate debug records if appropriate */
    if (G_debug)
        build_debug_table(gen_ctx.method_ofs);

    /* check for unreferenced labels and issue warnings */
    check_unreferenced_labels();

    /* clean up globals for the end of the method */
    G_cg->close_method_cleanup(&gen_ctx);
}

/*
 *   Check for unreferenced local variables 
 */
void CTPNCodeBody::check_locals()
{
    CTcPrsSymtab *tab;
    
    /* check for unreferenced locals in each nested scope */
    for (tab = first_nested_symtab_ ; tab != 0 ; tab = tab->get_list_next())
    {
        /* check this table */
        tab->check_unreferenced_locals();
    }
}

/* 
 *   local symbol table enumerator for checking for parameter symbols that
 *   belong in the local context 
 */
void CTPNCodeBody::enum_for_param_ctx(void *, class CTcSymbol *sym)
{
    /* if this is a local, check it further */
    if (sym->get_type() == TC_SYM_LOCAL || sym->get_type() == TC_SYM_PARAM)
    {
        CTcSymLocal *lcl = (CTcSymLocal *)sym;

        /* 
         *   if it's a parameter, and it's also a context variable, its
         *   value needs to be moved into the context 
         */
        if (lcl->is_param() && lcl->is_ctx_local())
        {
            /* get the actual parameter value from the stack */
            CTcSymLocal::s_gen_code_getlcl(lcl->get_var_num(), TRUE);

            /* store the value in the context variable */
            lcl->gen_code_asi(TRUE, TC_ASI_SIMPLE, 0, TRUE);
        }
    }
}

/*
 *   generate as an anonymous function 
 */
void CTPNCodeBody::js_anon_func()
{
    /* generate the body onto the js expression stack */
    gen_code(FALSE, FALSE);

    /* start with the 'function' keyword and open the arg list */
    js_expr_buf expr;
    expr += "function(";

    /* add the argument names */
    for (int i = 0 ; i < argc_ ; ++i)
    {
        char buf[32];
        sprintf(buf, "%sa%d", i == 0 ? "" : ",", i);
        expr += buf;
    }

    /* close the arg list and add the placeholder for the body */
    expr += ")$1";

    /* push this as our new expression */
    G_cg->js_expr((const char *)expr);
}


/* ------------------------------------------------------------------------ */
/*
 *   'return' statement 
 */

/*
 *   generate code 
 */
void CTPNStmReturn::gen_code(int, int)
{
    int val_on_stack;
    int need_gen;

    /* add a line record */
    add_debug_line_rec();

    /* presume we'll generate a value */
    need_gen = TRUE;
    val_on_stack = FALSE;

    /* generate the return value expression, if appropriate */
    if (expr_ != 0)
    {
        /* 
         *   it's an error if we're in a constructor, because a
         *   constructor implicitly always returns 'self' 
         */
        if (G_cg->is_in_constructor())
            log_error(TCERR_CONSTRUCT_CANNOT_RET_VAL);

        /* check for a constant expression */
        if (expr_->is_const())
        {
            switch(expr_->get_const_val()->get_type())
            {
            case TC_CVT_NIL:
            case TC_CVT_TRUE:
                /* 
                 *   we can use special constant return instructions for
                 *   these, so there's no need to generate the value 
                 */
                need_gen = FALSE;
                break;

            default:
                /* 
                 *   other types don't have constant-return opcodes, so we
                 *   must generate the expression code 
                 */
                need_gen = TRUE;
                break;
            }
        }

        /* if necessary, generate the value */
        if (need_gen)
        {
            int depth;

            /* note the initial stack depth */
            depth = G_cg->get_sp_depth();

            /*  
             *   Generate the value.  We are obviously not discarding the
             *   value, and since returning a value is equivalent to
             *   assigning the value, we must use the stricter assignment
             *   (not 'for condition') rules for logical expressions 
             */
            expr_->gen_code(FALSE, FALSE);

            /* note whether we actually left a value on the stack */
            val_on_stack = (G_cg->get_sp_depth() > depth);
        }
        else
        {
            /* 
             *   we obviously aren't leaving a value on the stack if we
             *   don't generate anything 
             */
            val_on_stack = FALSE;
        }
    }

    /* 
     *   Before we return, let any enclosing statements generate any code
     *   necessary to leave their scope (in particular, we must invoke
     *   'finally' handlers in any enclosing 'try' blocks).
     *   
     *   Note that we generated the expression BEFORE we call any
     *   'finally' handlers.  This is necessary because something we call
     *   in the course of evaluating the return value could have thrown an
     *   exception; if we were to call the 'finally' clauses before
     *   generating the return value, we could invoke the 'finally' clause
     *   twice (once explicitly, once in the handling of the thrown
     *   exception), which would be incorrect.  By generating the
     *   'finally' calls after the return expression, we're sure that the
     *   'finally' blocks are invoked only once - either through the
     *   throw, or else now, after there's no more possibility of a
     *   'throw' before the return.  
     */
    if (G_cs->get_enclosing() != 0)
    {
        int did_save_retval;
        uint fin_ret_lcl;

        /* 
         *   if we're going to generate any subroutine calls, and we have
         *   a return value on the stack, we need to save the return value
         *   in a local to make sure the calculated value isn't affected
         *   by the subroutine call 
         */
        if (val_on_stack
            && G_cs->get_enclosing()->will_gen_code_unwind_for_return()
            && G_cs->get_code_body() != 0)
        {
            /* allocate a local variable to save the return value */
            fin_ret_lcl = G_cs->get_code_body()->alloc_fin_ret_lcl();

            /* save the return value in a stack temporary for a moment */
            CTcSymLocal::s_gen_code_setlcl_stk(fin_ret_lcl, FALSE);

            /* 
             *   note that we saved the return value, so we can retrieve
             *   it later 
             */
            did_save_retval = TRUE;
        }
        else
        {
            /* note that we didn't save the return value */
            did_save_retval = FALSE;
        }

        /* generate the unwind */
        G_cs->get_enclosing()->gen_code_unwind_for_return();

        /* if we saved the return value, retrieve it */
        if (did_save_retval)
            CTcSymLocal::s_gen_code_getlcl(fin_ret_lcl, FALSE);
    }

    /* check for an expression to return */
    if (G_cg->is_in_constructor())
    {
        /* we're in a constructor - return 'self' */
        G_cg->write_op(OPC_PUSHSELF);
        G_cg->write_op(OPC_RETVAL);
    }
    else if (expr_ == 0)
    {
        /* 
         *   there's no expression - generate a simple void return (but
         *   explicitly return nil, so we don't return something left in
         *   R0 from a previous function call we made) 
         */
        G_cg->write_op(OPC_RETNIL);
    }
    else
    {
        /* check for a constant expression */
        if (expr_->is_const())
        {
            switch(expr_->get_const_val()->get_type())
            {
            case TC_CVT_NIL:
                /* generate a RETNIL instruction */
                G_cg->write_op(OPC_RETNIL);
                break;

            case TC_CVT_TRUE:
                /* generate a RETTRUE instruction */
                G_cg->write_op(OPC_RETTRUE);
                break;

            default:
                break;
            }
        }

        /* 
         *   if we needed code generation to evaluate the return value, we
         *   now need to return the value 
         */
        if (need_gen)
        {
            /* 
             *   Other types don't have constant-return opcodes.  We
             *   already generated the expression value (before invoking
             *   the enclosing 'finally' handlers, if any), so the value
             *   is on the stack, and all we need to do is return it.
             *   
             *   If we didn't actually leave a value on the stack, we'll
             *   just return nil.  
             */
            if (val_on_stack)
            {
                /* generate the return-value opcode */
                G_cg->write_op(OPC_RETVAL);
                
                /* RETVAL removes an element from the stack */
                G_cg->note_pop();
            }
            else
            {
                /* 
                 *   The depth didn't change - they must have evaluated an
                 *   expression involving a dstring or void function.
                 *   Return nil instead of the non-existent value.  
                 */
                G_cg->write_op(OPC_RETNIL);
            }
        }
    }
}
    
/* ------------------------------------------------------------------------ */
/*
 *   Static property initializer statement 
 */
void CTPNStmStaticPropInit::gen_code(int, int)
{
    int depth;
    
    /* add a line record */
    add_debug_line_rec();

    /* note the initial stack depth */
    depth = G_cg->get_sp_depth();

    /* generate the expression, keeping the generated value */
    expr_->gen_code(FALSE, FALSE);

    /* ensure that we generated a value; if we didn't, push nil by default */
    if (G_cg->get_sp_depth() <= depth)
    {
        /* push a default nil value */
        G_cg->write_op(OPC_PUSHNIL);
        G_cg->note_push();
    }

    /* 
     *   duplicate the value on the stack, so we can assign it to
     *   initialize the property and also return it 
     */
    G_cg->write_op(OPC_DUP);
    G_cg->note_push();

    /* write the SETPROPSELF to initialize the property */
    G_cg->write_op(OPC_SETPROPSELF);
    G_cs->write_prop_id(prop_);

    /* SETPROPSELF removes the value */
    G_cg->note_pop();

    /* return the value (which we duplicated on the stack) */
    G_cg->write_op(OPC_RETVAL);

    /* RETVAL removes the value */
    G_cg->note_pop();
}


/* ------------------------------------------------------------------------ */
/*
 *   Object Definition Statement 
 */

/*
 *   generate code 
 */
void CTPNStmObject::gen_code(int, int)
{
    CTPNSuperclass *sc;
    CTPNObjProp *prop;
    int sc_cnt;
    ulong start_ofs;
    uint internal_flags;
    uint obj_flags;
    CTcDataStream *str;
    int bad_sc;

    /* if this object has been replaced, don't generate any code for it */
    if (replaced_)
        return;

    /* add an implicit constructor if necessary */
    add_implicit_constructor();

    /* get the appropriate stream for generating the data */
    str = obj_sym_->get_stream();

    /* clear the internal flags */
    internal_flags = 0;

    /* 
     *   if we're a modified object, set the 'modified' flag in the object
     *   header 
     */
    if (modified_)
        internal_flags |= TCT3_OBJ_MODIFIED;

    /* set the 'transient' flag if appropriate */
    if (transient_)
        internal_flags |= TCT3_OBJ_TRANSIENT;

    /* clear the object flags */
    obj_flags = 0;

    /* 
     *   If we're specifically marked as a 'class' object, or we're a
     *   modified object, set the 'class' flag in the object flags.  
     */
    if (is_class_ || modified_)
        obj_flags |= TCT3_OBJFLG_CLASS;

    /* remember our starting offset in the object stream */
    start_ofs = str->get_ofs();

    /* 
     *   store our stream offset in our defining symbol, for storage in
     *   the object file 
     */
    obj_sym_->set_stream_ofs(start_ofs);

    /* write our internal flags */
    str->write2(internal_flags);

    /* 
     *   First, write the per-object image file "OBJS" header - each
     *   object starts with its object ID and the number of bytes in the
     *   object's metaclass-specific data.  For now, write zero as a
     *   placeholder for our data size.  Note that this is a
     *   self-reference: it must be modified if the object is renumbered.  
     */
    str->write_obj_id_selfref(obj_sym_);
    str->write2(0);

    /* write a placeholder for the superclass count */
    str->write2(0);

    /* write the fixed property count */
    str->write2(prop_cnt_);

    /* write the object flags */
    str->write2(obj_flags);

    /*
     *   First, go through the superclass list and verify that each
     *   superclass is actually an object.  
     */
    for (bad_sc = FALSE, sc_cnt = 0, sc = first_sc_ ; sc != 0 ; sc = sc->nxt_)
    {
        CTcSymObj *sc_sym;

        /* look up the superclass in the global symbol table */
        sc_sym = (CTcSymObj *)sc->get_sym();

        /* make sure it's defined, and that it's really an object */
        if (sc_sym == 0)
        {
            /* not defined */
            log_error(TCERR_UNDEF_SYM_SC,
                      (int)sc->get_sym_len(), sc->get_sym_txt(),
                      (int)obj_sym_->get_sym_len(), obj_sym_->get_sym());

            /* note that we have an invalid superclass */
            bad_sc = TRUE;
        }
        else if (sc_sym->get_type() != TC_SYM_OBJ)
        {
            /* log an error */
            log_error(TCERR_SC_NOT_OBJECT,
                      (int)sc_sym->get_sym_len(), sc_sym->get_sym());

            /* note that we have an invalid superclass */
            bad_sc = TRUE;
        }
        else
        {
            /* count the superclass */
            ++sc_cnt;

            /* write the superclass to the object header */
            str->write_obj_id(sc_sym->get_obj_id());
        }
    }

    /* 
     *   If we detected a 'bad template' error when we were parsing the
     *   object definition, and all of our superclasses are valid, report the
     *   template error.
     *   
     *   Do not report this error if we have any undefined or invalid
     *   superclasses, because (1) we've already reported one error for this
     *   object definition (the bad superclass error), and (2) the missing
     *   template is likely just a consequence of the bad superclass, since
     *   we can't have scanned the proper superclass's list of templates if
     *   they didn't tell us the correct superclass to start with.  When they
     *   fix the superclass list and re-compile the code, it's likely that
     *   this will fix the template problem as well, since we'll probably be
     *   able to find the template give the corrected superclass list.
     *   
     *   If we found an undescribed class anywhere in our hierarchy, a
     *   template simply cannot be used with this object; otherwise, the
     *   error is that we failed to find a suitable template 
     */
    if (has_bad_template() && !bad_sc)
        log_error(has_undesc_sc()
                  ? TCERR_OBJ_DEF_CANNOT_USE_TEMPLATE
                  : TCERR_OBJ_DEF_NO_TEMPLATE);

    /* go back and write the superclass count to the header */
    str->write2_at(start_ofs + TCT3_TADSOBJ_HEADER_OFS, sc_cnt);

    /*
     *   Write the properties.  We're required to write the properties in
     *   sorted order of property ID, but we can't do that yet, because
     *   the property ID's aren't finalized until after linking.  For now,
     *   just write them out in the order in which they were defined.  
     */
    for (prop = first_prop_ ; prop != 0 ; prop = prop->nxt_)
    {
        /* make sure we have a valid property symbol */
        if (prop->get_prop_sym() != 0)
        {
            /* write the property ID */
            str->write_prop_id(prop->get_prop_sym()->get_prop());

            /* generate code for the property */
            prop->gen_code(FALSE, FALSE);
        }
    }

    /* 
     *   go back and write the size of our metaclass-specific data - this
     *   goes at offset 4 in the T3 generic metaclass header
     */
    str->write2_at(start_ofs + TCT3_META_HEADER_OFS + 4,
                   str->get_ofs() - (start_ofs + TCT3_META_HEADER_OFS + 6));
}

/*
 *   Check for unreferenced local variables 
 */
void CTPNStmObject::check_locals()
{
    CTPNObjProp *prop;

    /* check for unreferenced locals for each property */
    for (prop = first_prop_ ; prop != 0 ; prop = prop->nxt_)
        prop->check_locals();
}
