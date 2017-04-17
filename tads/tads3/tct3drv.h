/* $Header: d:/cvsroot/tads/tads3/TCT3DRV.H,v 1.4 1999/07/11 00:46:57 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tct3drv.h - derived final T3-specific parse node classes
Function
  
Notes
  
Modified
  05/10/99 MJRoberts  - Creation
*/

#ifndef TCT3DRV_H
#define TCT3DRV_H

#include <assert.h>

/* include our T3-specific intermediate classes */
#include "tct3int.h"

/* include the target-independent derived classes */
#include "tcpndrv.h"


/* ------------------------------------------------------------------------ */
/*
 *   "self" 
 */
class CTPNSelf: public CTPNSelfBase
{
public:
    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);

    /* get the object value for a '.' expression */
    vm_obj_id_t gen_code_obj_predot(int *is_self);
};

/* ------------------------------------------------------------------------ */
/*
 *   "replaced" 
 */
class CTPNReplaced: public CTPNReplacedBase
{
public:
    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate a function call expression */
    void gen_code_call(int discard, int argc, int varargs,
                       struct CTcNamedArgs *named_args);
};

/* ------------------------------------------------------------------------ */
/*
 *   "targetprop" 
 */
class CTPNTargetprop: public CTPNTargetpropBase
{
public:
    /* generate code */
    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   "targetobj" 
 */
class CTPNTargetobj: public CTPNTargetobjBase
{
public:
    /* generate code */
    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   "definingobj" 
 */
class CTPNDefiningobj: public CTPNDefiningobjBase
{
public:
    /* generate code */
    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   "invokee" 
 */
class CTPNInvokee: public CTPNInvokeeBase
{
public:
    /* generate code */
    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   "inherited" 
 */
class CTPNInh: public CTPNInhBase
{
public:
    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);

protected:
    /* generate a multi-method inherited() call */
    void gen_code_mminh(CTcSymFunc *func, int discard,
                        class CTcPrsNode *prop_expr, int prop_is_expr,
                        int argc, int varargs, struct CTcNamedArgs *named_args);
};

/*
 *   "inherited" with explicit superclass 
 */
class CTPNInhClass: public CTPNInhClassBase
{
public:
    CTPNInhClass(const char *sym, size_t len)
        : CTPNInhClassBase(sym, len) { }

    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);
};

/*
 *   "delegated" 
 */
class CTPNDelegated: public CTPNDelegatedBase
{
public:
    CTPNDelegated(CTcPrsNode *delegatee)
        : CTPNDelegatedBase(delegatee) { }

    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);
};

/* ------------------------------------------------------------------------ */
/*
 *   "argcount" node 
 */
class CTPNArgc: public CTPNArgcBase
{
public:
    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/* 
 *   constant node 
 */
class CTPNConst: public CTPNConstBase
{
public:
    CTPNConst(const CTcConstVal *val) : CTPNConstBase(val) { }

    /* generate code for the constant */
    void gen_code(int discard, int for_condition);

    /* generate code for operator 'new' applied to this expression */
    void gen_code_new(int discard, int argc, int varargs,
                      struct CTcNamedArgs *named_args,
                      int from_call, int is_transient);

    /* generate code for assigning to this expression */
    int gen_code_asi(int discard, int phase, tc_asitype_t typ, const char *op,
                     CTcPrsNode *rhs, 
                     int ignore_errors, int xplicit, void **ctx);

    /* evaluate a property ID */
    vm_prop_id_t gen_code_propid(int check_only, int is_expr);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);

    /* generate a function call expression */
    void gen_code_call(int discard, int argc, int varargs,
                       struct CTcNamedArgs *named_args);

    /* get the object value for a '.' expression */
    vm_obj_id_t gen_code_obj_predot(int *is_self);

    /* generate code to push an integer constant */
    static void s_gen_code_int(long intval);

    /* generate code to push a string constant */
    static void s_gen_code_str(const CTcConstVal *val);
    static void s_gen_code_str(const char *str, size_t len);

    /* generate code to push a symbol's name as a string constant */
    static void s_gen_code_str(const class CTcSymbol *sym);

    /* 
     *   generate code to push a string, using the correct code for the
     *   current compilation mode (static vs dynamic) 
     */
    static void s_gen_code_str_by_mode(const class CTcSymbol *sym);
};

/*
 *   Debugger constant node 
 */
class CTPNDebugConst: public CTPNConst
{
public:
    CTPNDebugConst(CTcConstVal *val) : CTPNConst(val) { }

    /* debugger constants aren't actual constants - they require code gen */
    CTcConstVal *get_const_val() { return 0; }

    /* generate code for the constant */
    void gen_code(int discard, int for_condition);

    /* generate a constant string */
    static void s_gen_code_str(const char *str, size_t len);
};

/* ------------------------------------------------------------------------ */
/*
 *   Unary Operators 
 */

/* bitwise NOT */
CTPNUnary_def(CTPNBNot);

/* arithmetic positive */
CTPNUnary_def(CTPNPos);

/* arithmetic negative */
CTPNUnary_def(CTPNNeg);

/* pre-increment */
CTPNUnary_side_def(CTPNPreInc);
void CTPNPreInc_gen_code(class CTcPrsNode *sub, int discard);

/* pre-decrement */
CTPNUnary_side_def(CTPNPreDec);

/* post-increment */
CTPNUnary_side_def(CTPNPostInc);

/* post-decrement */
CTPNUnary_side_def(CTPNPostDec);

/* delete */
CTPNUnary_side_def(CTPNDelete);

/* boolean-ize */
CTPNUnary_def(CTPNBoolize);

/* ------------------------------------------------------------------------ */
/*
 *   NEW operator 
 */
class CTPNNew: public CTPNUnary
{ 
public:
    CTPNNew(class CTcPrsNode *sub, int is_transient)
        : CTPNUnary(sub)
    {
        /* remember 'transient' status */
        transient_ = is_transient;
    }

    /* generate code */
    void gen_code(int discard, int for_condition);

    /* note that we have side effects */
    virtual int has_side_effects() const { return TRUE; }

protected:
    /* flag: it's a 'new transient' operator */
    int transient_;
};


/* ------------------------------------------------------------------------ */
/*
 *   NOT operator 
 */
class CTPNNot: public CTPNNotBase
{
public:
    CTPNNot(CTcPrsNode *sub) : CTPNNotBase(sub) { }

    /* generate code */
    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/* 
 *   Binary Operators 
 */

class CTPNComma: public CTPNCommaBase
{
public:
    CTPNComma(class CTcPrsNode *lhs, class CTcPrsNode *rhs)
        : CTPNCommaBase(lhs, rhs) { }

    /* generate code */
    void gen_code(int discard, int for_condition);
};

/* bitwise OR */
CTPNBin_def(CTPNBOr);

/* bitwise AND */
CTPNBin_def(CTPNBAnd);

/* bitwise XOR */
CTPNBin_def(CTPNBXor);

/* greater than */
CTPNBin_cmp_def(CTPNGt);

/* greater or equal */
CTPNBin_cmp_def(CTPNGe);

/* less than */
CTPNBin_cmp_def(CTPNLt);

/* less or equal */
CTPNBin_cmp_def(CTPNLe);

/* bit shift left */
CTPNBin_def(CTPNShl);

/* arithmetic shift right */
CTPNBin_def(CTPNAShr);

/* logical shift right */
CTPNBin_def(CTPNLShr);

/* multiply */
CTPNBin_def(CTPNMul);

/* divide */
CTPNBin_def(CTPNDiv);

/* modulo */
CTPNBin_def(CTPNMod);

/* ------------------------------------------------------------------------ */
/*
 *   Addition 
 */

class CTPNAdd: public CTPNAddBase
{
public:
    CTPNAdd(class CTcPrsNode *left, class CTcPrsNode *right)
        : CTPNAddBase(left, right) { }

    /* generate code */
    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   Subtraction
 */

class CTPNSub: public CTPNSubBase
{
public:
    CTPNSub(class CTcPrsNode *left, class CTcPrsNode *right)
        : CTPNSubBase(left, right) { }

    /* generate code */
    void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   Equality Comparison
 */

class CTPNEq: public CTPNEqBase
{
public:
    CTPNEq(class CTcPrsNode *left, class CTcPrsNode *right)
        : CTPNEqBase(left, right) { }

    /* generate code */
    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   Inequality Comparison
 */

class CTPNNe: public CTPNNeBase
{
public:
    CTPNNe(class CTcPrsNode *left, class CTcPrsNode *right)
        : CTPNNeBase(left, right) { }

    /* generate code */
    void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   'is in' 
 */

class CTPNIsIn: public CTPNIsInBase
{
public:
    CTPNIsIn(class CTcPrsNode *left, class CTPNArglist *right)
        : CTPNIsInBase(left, right) { }

    /* generate code */
    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   'not in' 
 */

class CTPNNotIn: public CTPNNotInBase
{
public:
    CTPNNotIn(class CTcPrsNode *left, class CTPNArglist *right)
        : CTPNNotInBase(left, right) { }

    /* generate code */
    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   Logical AND (short-circuit logic)
 */

class CTPNAnd: public CTPNAndBase
{
public:
    CTPNAnd(class CTcPrsNode *left, class CTcPrsNode *right)
        : CTPNAndBase(left, right) { }

    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate a condition */
    virtual void gen_code_cond(struct CTcCodeLabel *then_label,
                               struct CTcCodeLabel *else_label);
};


/* ------------------------------------------------------------------------ */
/*
 *   Logical OR (short-circuit logic)
 */

class CTPNOr: public CTPNOrBase
{
public:
    CTPNOr(class CTcPrsNode *left, class CTcPrsNode *right)
        : CTPNOrBase(left, right) { }

    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate a condition */
    virtual void gen_code_cond(struct CTcCodeLabel *then_label,
                               struct CTcCodeLabel *else_label);
};


/* ------------------------------------------------------------------------ */
/*
 *   Assignment Operators 
 */

/* simple assignment */
class CTPNAsi: public CTPNAsiBase
{
public:
    CTPNAsi(class CTcPrsNode *left, class CTcPrsNode *right)
        : CTPNAsiBase(left, right) { }

    /* generate code */
    void gen_code(int discard, int for_condition);
};

/* add and assign */
CTPNBin_side_def(CTPNAddAsi);
void CTPNAddAsi_gen_code(class CTcPrsNode *left, class CTcPrsNode *right,
                         int discard);

/* subtract and assign */
CTPNBin_side_def(CTPNSubAsi);

/* multiple and assign */
CTPNBin_side_def(CTPNMulAsi);

/* divide and assign */
CTPNBin_side_def(CTPNDivAsi);

/* modulo and assign */
CTPNBin_side_def(CTPNModAsi);

/* bitwise AND and assign */
CTPNBin_side_def(CTPNBAndAsi);

/* bitwise OR and assign */
CTPNBin_side_def(CTPNBOrAsi);

/* bitwise XOR and assign */
CTPNBin_side_def(CTPNBXorAsi);

/* bit shift left and assign */
CTPNBin_side_def(CTPNShlAsi);

/* arithmetic shift right and assign */
CTPNBin_side_def(CTPNAShrAsi);

/* logical shift right and assign */
CTPNBin_side_def(CTPNLShrAsi);

/* ------------------------------------------------------------------------ */
/*
 *   Array/list Subscript 
 */
class CTPNSubscript: public CTPNSubscriptBase
{
public:
    CTPNSubscript(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNSubscriptBase(lhs, rhs) { }

    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate code to assign a value to the symbol */
    int gen_code_asi(int discard, int phase, tc_asitype_t typ, const char *op,
                     CTcPrsNode *rhs,
                     int ignore_errors, int xplicit, void **ctx);
};


/* ------------------------------------------------------------------------ */
/*
 *   Address-of Operator 
 */

class CTPNAddr: public CTPNAddrBase
{
public:
    CTPNAddr(CTcPrsNode *sub)
        : CTPNAddrBase(sub) { }

    /* generate code */
    void gen_code(int discard, int for_condition)
    {
        /* 
         *   if we're not discarding the value, tell the subnode to
         *   generate its address; if the value is being discarded, do
         *   nothing, since taking an address has no side effects 
         */
        if (!discard)
            sub_->gen_code_addr();
    }

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args)
    {
        /* 
         *   let the subnode do the work, since "&obj" is the same as
         *   "obj" in any context 
         */
        sub_->gen_code_member(discard, prop_expr, prop_is_expr,
                              argc, varargs, named_args);
    }

    /* evaluate a property ID */
    vm_prop_id_t gen_code_propid(int check_only, int is_expr)
    {
        /* 
         *   let the subexpression handle it, so that we treat "x.&prop"
         *   the same as "x.prop" 
         */
        return sub_->gen_code_propid(check_only, is_expr);
    }

    /* get the object value for a '.' expression */
    vm_obj_id_t gen_code_obj_predot(int *is_self)
    {
        /* 
         *   let the subexpression handle it, since "x.&prop" is the same
         *   as "x.prop" 
         */
        return sub_->gen_code_obj_predot(is_self);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   If-nil operator ??
 */
class CTPNIfnil: public CTPNIfnilBase
{
public:
    CTPNIfnil(class CTcPrsNode *first, class CTcPrsNode *second)
        : CTPNIfnilBase(first, second) { }

    /* generate code for the conditional */
    void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   Conditional Operator 
 */
class CTPNIf: public CTPNIfBase
{
public:
    CTPNIf(class CTcPrsNode *first, class CTcPrsNode *second,
           class CTcPrsNode *third)
        : CTPNIfBase(first, second, third) { }

    /* generate code for the conditional */
    void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   Symbol node 
 */
class CTPNSym: public CTPNSymBase
{
public:
    CTPNSym(const char *sym, size_t len)
        : CTPNSymBase(sym, len) { }

    /* generate code for the symbol evaluated as an rvalue */
    void gen_code(int discard, int for_condition);

    /* generate code to assign a value to the symbol */
    int gen_code_asi(int discard, int phase, tc_asitype_t typ, const char *op,
                     CTcPrsNode *rhs,
                     int ignore_errors, int xplicit, void **ctx);

    /* generate code to take the address of the symbol */
    void gen_code_addr();

    /* generate code to call the symbol */
    void gen_code_call(int discard, int argc, int varargs,
                       struct CTcNamedArgs *named_args);

    /* generate code for operator 'new' applied to this expression */
    void gen_code_new(int discard, int argc, int varargs,
                      struct CTcNamedArgs *named_args,
                      int from_call, int is_transient);

    /* evaluate a property ID */
    vm_prop_id_t gen_code_propid(int check_only, int is_expr);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);

    /* get the object value for a '.' expression */
    vm_obj_id_t gen_code_obj_predot(int *is_self);
};

/* ------------------------------------------------------------------------ */
/*
 *   Resolved symbol 
 */
class CTPNSymResolved: public CTPNSymResolvedBase
{
public:
    CTPNSymResolved(CTcSymbol *sym)
        : CTPNSymResolvedBase(sym) { }

    /* generate code for the symbol evaluated as an rvalue */
    void gen_code(int discard, int for_condition);

    /* generate code to assign a value to the symbol */
    int gen_code_asi(int discard, int phase, tc_asitype_t typ, const char *op,
                     CTcPrsNode *rhs,
                     int ignore_errors, int xplicit, void **ctx);

    /* generate code to take the address of the symbol */
    void gen_code_addr();

    /* generate code to call the symbol */
    void gen_code_call(int discard, int argc, int varargs,
                       struct CTcNamedArgs *named_args);

    /* generate code for operator 'new' applied to this expression */
    void gen_code_new(int discard, int argc, int varargs,
                      struct CTcNamedArgs *named_args,
                      int from_call, int is_transient);

    /* evaluate a property ID */
    vm_prop_id_t gen_code_propid(int check_only, int is_expr);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);

    /* get the object value for a '.' expression */
    vm_obj_id_t gen_code_obj_predot(int *is_self);
};

/* ------------------------------------------------------------------------ */
/*
 *   Resolved debugger local variable symbol 
 */
class CTPNSymDebugLocal: public CTPNSymDebugLocalBase
{
public:
    CTPNSymDebugLocal(const struct tcprsdbg_sym_info *info)
        : CTPNSymDebugLocalBase(info)
    {
    }

    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate code for assigning into this node */
    int gen_code_asi(int discard, int phase, tc_asitype_t typ, const char *op,
                     class CTcPrsNode *rhs,
                     int ignore_errors, int xplicit, void **ctx);
};

/* ------------------------------------------------------------------------ */
/*
 *   double-quoted string expression 
 */
class CTPNDstr: public CTPNDstrBase
{
public:
    CTPNDstr(const char *str, size_t len)
        : CTPNDstrBase(str, len) { }

    void gen_code(int discard, int for_condition);
};

/*
 *   debug version of string 
 */
class CTPNDebugDstr: public CTPNDstr
{
public:
    CTPNDebugDstr(const char *str, size_t len)
        : CTPNDstr(str, len) { }

    void gen_code(int discard, int for_condition);
};

/*
 *   double-quoted string embedding 
 */
class CTPNDstrEmbed: public CTPNDstrEmbedBase
{
public:
    CTPNDstrEmbed(CTcPrsNode *sub);

    void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   Embedded <<one of>> list in a string 
 */
class CTPNStrOneOf: public CTPNStrOneOfBase
{
public:
    CTPNStrOneOf(int dstr, class CTPNList *lst, class CTcSymObj *state_obj)
        : CTPNStrOneOfBase(dstr, lst, state_obj) { }

    void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   Argument List 
 */
class CTPNArglist: public CTPNArglistBase
{
public:
    CTPNArglist(int argc, class CTPNArg *list_head)
        : CTPNArglistBase(argc, list_head) { }

    void gen_code(int, int)
    {
        /* 
         *   this isn't used - we always generate explicitly via
         *   gen_code_arglist() 
         */
        assert(FALSE);
    }

    /* generate code for an argument list */
    void gen_code_arglist(int *varargs, struct CTcNamedArgs &named_args);
};

/*
 *   Argument List Entry 
 */
class CTPNArg: public CTPNArgBase
{
public:
    CTPNArg(CTcPrsNode *expr)
        : CTPNArgBase(expr) { }

    void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   Function/method call 
 */
class CTPNCall: public CTPNCallBase
{
public:
    CTPNCall(CTcPrsNode *func, class CTPNArglist *arglist);

    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate code for operator 'new' applied to this expression */
    void gen_code_new(int discard, int argc, int varargs,
                      struct CTcNamedArgs *named_args,
                      int from_call, int is_transient);

    /* generate code for a call to 'rand' */
    int gen_code_rand(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   Member expression with no arguments 
 */
class CTPNMember: public CTPNMemberBase
{
public:
    CTPNMember(CTcPrsNode *lhs, CTcPrsNode *rhs, int rhs_is_expr)
        : CTPNMemberBase(lhs, rhs, rhs_is_expr) { }

    /* generate code */
    void gen_code(int discard, int for_condition);

    /* generate code to assign a value to the symbol */
    int gen_code_asi(int discard, int phase, tc_asitype_t typ, const char *op,
                     CTcPrsNode *rhs,
                     int ignore_errors, int xplicit, void **ctx);
};

/*
 *   Member evaluation with argument list 
 */
class CTPNMemArg: public CTPNMemArgBase
{
public:
    CTPNMemArg(CTcPrsNode *obj_expr, CTcPrsNode *prop_expr, int prop_is_expr,
               class CTPNArglist *arglist)
        : CTPNMemArgBase(obj_expr, prop_expr, prop_is_expr, arglist) { }

    void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   List 
 */
class CTPNList: public CTPNListBase
{
public:
    void gen_code(int discard, int for_condition);
};

/*
 *   List Element 
 */
class CTPNListEle: public CTPNListEleBase
{
public:
    CTPNListEle(CTcPrsNode *expr)
        : CTPNListEleBase(expr) { }

    void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   Derived T3-specific symbol classes 
 */

/*
 *   undefined symbol 
 */
class CTcSymUndef: public CTcSymUndefBase
{
public:
    CTcSymUndef(const char *str, size_t len, int copy)
        : CTcSymUndefBase(str, len, copy) { }

    /* 
     *   generate code to evaluate the symbol; since it's undefined, this
     *   doesn't generate any code at all 
     */
    virtual void gen_code(int) { }

    /* 
     *   Generate code to assign to the symbol.  The symbol is undefined,
     *   so we can't generate any code; but we'll indicate to the caller
     *   that the assignment is fully processed anyway, since there's no
     *   need for the caller to try generating any code either. 
     */
    virtual int gen_code_asi(int, int phase, tc_asitype_t, const char *,
                             class CTcPrsNode *, int, int, void **)
    {
        return TRUE;
    }

    /* 
     *   Generate code for taking the address of the symbol.  The symbol
     *   has no address, so this generates no code. 
     */
    virtual void gen_code_addr() { }

    /* 
     *   generate code to call the symbol - we can't generate any code for
     *   this, but don't bother issuing an error since we will have shown
     *   an error for the undefined symbol in the first place 
     */
    virtual void gen_code_call(int, int, int, struct CTcNamedArgs *) { }

    /* 
     *   generate code for operator 'new' applied to this expression - we
     *   can't generate any code, but suppress errors as usual 
     */
    void gen_code_new(int, int, int, struct CTcNamedArgs *, int) { }

    /* generate a member expression */
    void gen_code_member(int, class CTcPrsNode *, int, int, int,
                         struct CTcNamedArgs *) { }

    /* get the object value for a '.' expression */
    vm_obj_id_t gen_code_obj_predot(int *is_self)
    {
        *is_self = FALSE;
        return VM_INVALID_OBJ;
    }
};


/*
 *   function 
 */
class CTcSymFunc: public CTcSymFuncBase
{
public:
    CTcSymFunc(const char *str, size_t len, int copy,
               int argc, int opt_argc, int varargs, int has_retval,
               int is_multimethod, int is_mm_base,
               int is_extern, int has_proto)
        : CTcSymFuncBase(str, len, copy, argc, opt_argc, varargs, has_retval,
                         is_multimethod, is_mm_base, is_extern, has_proto)
    {
        /* 
         *   we don't have a valid absolute address - by default, we use
         *   the anchor 
         */
        abs_addr_valid_ = FALSE;
        abs_addr_ = 0;
    }

    /* generate code to evaluate the symbol */
    virtual void gen_code(int discard);

    /* we can take the address of this symbol type */
    virtual void gen_code_addr();

    /* generate a call */
    virtual void gen_code_call(int discard, int argc, int varargs,
                               struct CTcNamedArgs *named_args);

    /* 
     *   Get my code pool address.  This is valid only after the linking
     *   phase has been completed and all fixups have been applied.  (The
     *   normal use for this is to generate image file elements not
     *   subject to fixup, such as the entrypoint record.  Normally,
     *   references to the symbol that require the code pool address would
     *   simply generate a fixup rather than asking for the true address,
     *   since a fixup can be generated at any time during code
     *   generation.)
     */
    ulong get_code_pool_addr() const;

    /* write the symbol to an image file's global symbol table */
    int write_to_image_file_global(class CVmImageWriter *image_writer);

    /*
     *   Set the absolute code pool address.  This can only be used when
     *   the symbol is loaded from a fully-linked image file; in most
     *   cases, this routine is not needed, because the function's address
     *   will be kept with its associated anchor to allow for relocation
     *   of the function during the linking process.  
     */
    void set_abs_addr(uint32_t addr)
    {
        /* remember the address */
        abs_addr_ = addr;

        /* note that the absolute address is valid */
        abs_addr_valid_ = TRUE;
    }

    /* add a runtime symbol table entry */
    void add_runtime_symbol(class CVmRuntimeSymbols *symtab);
    
protected:
    /*
     *   Absolute code pool address, and a flag indicating whether the
     *   address is valid.  When we load a function symbol from a
     *   fully-linked image file (specifically, when we load the symbol
     *   from the debug records of an image file), the address of the
     *   function is fully resolved and doesn't require fixups via the
     *   anchor mechanism.  In these cases, we'll simply remember the
     *   resolved address here.  
     */
    ulong abs_addr_;
    uint abs_addr_valid_ : 1;
};

/*
 *   metaclass 
 */
class CTcSymMetaclass: public CTcSymMetaclassBase
{
public:
    CTcSymMetaclass(const char *str, size_t len, int copy, int meta_idx,
                    tctarg_obj_id_t class_obj)
        : CTcSymMetaclassBase(str, len, copy, meta_idx, class_obj)
    {
    }

    /* generate code to evaluate the symbol */
    virtual void gen_code(int discard);

    /* generate code for operator 'new' */
    void gen_code_new(int discard, int argc, int varargs,
                      struct CTcNamedArgs *named_args,
                      int is_transient);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);

    /* get the object value for a '.' expression */
    vm_obj_id_t gen_code_obj_predot(int *is_self)
    {
        /* simply generate our code as normal */
        gen_code(FALSE);

        /* indicate that the caller must use generated code */
        *is_self = FALSE;
        return VM_INVALID_OBJ;
    }

    /* write the symbol to an image file's global symbol table */
    int write_to_image_file_global(class CVmImageWriter *image_writer);

    /* 
     *   fix up the inheritance chain in the modifier objects - each
     *   object file has its own independent inheritance list, so we can
     *   only build the complete and connected list after loading them all 
     */
    void fix_mod_obj_sc_list();

    /* add a runtime symbol table entry */
    void add_runtime_symbol(class CVmRuntimeSymbols *symtab);
};

/*
 *   object 
 */
class CTcSymObj: public CTcSymObjBase
{
public:
    CTcSymObj(const char *str, size_t len, int copy, vm_obj_id_t obj_id,
              int is_extern, tc_metaclass_t meta, class CTcDictEntry *dict)
        : CTcSymObjBase(str, len, copy, obj_id, is_extern, meta, dict)
    {
    }

    /* we have a valid object value */
    virtual vm_obj_id_t get_val_obj() const { return get_obj_id(); }

    /* generate code to evaluate the symbol */
    virtual void gen_code(int discard);

    /* we can take the address of this symbol type */
    virtual void gen_code_addr();

    /* generate code for operator 'new' */
    void gen_code_new(int discard, int argc, int varargs,
                      struct CTcNamedArgs *named_args,
                      int is_transient);

    /* static code to generate code for 'new' */
    static void s_gen_code_new(int discard, vm_obj_id_t obj_id,
                               tc_metaclass_t meta,
                               int argc, int varargs,
                               struct CTcNamedArgs *named_args,
                               int is_transient);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);

    /* static code to generate a member expression */
    static void s_gen_code_member(int discard,
                                  class CTcPrsNode *prop_expr,
                                  int prop_is_expr, int argc,
                                  vm_obj_id_t obj_id,
                                  int varargs, struct CTcNamedArgs *named_args);

    /* get the object value for a '.' expression */
    vm_obj_id_t gen_code_obj_predot(int *is_self);

    /* delete a property from our modified base classes */
    virtual void delete_prop_from_mod_base(tctarg_prop_id_t prop_id);

    /* mark the compiled data for the object as a 'class' object */
    virtual void mark_compiled_as_class();

    /* write the symbol to an image file's global symbol table */
    int write_to_image_file_global(class CVmImageWriter *image_writer);

    /* build my dictionary */
    virtual void build_dictionary();

    /* add a runtime symbol table entry */
    void add_runtime_symbol(class CVmRuntimeSymbols *symtab);
};

/*
 *   Function-like object symbol.  This is an object that's invokable as a
 *   function, such as an anonymous function object or a DynamicFunc
 *   instance.  These occur in the dynamic compiler only.  
 */
class CTcSymFuncObj: public CTcSymObj
{
public:
    CTcSymFuncObj(const char *str, size_t len, int copy, vm_obj_id_t obj_id,
                  int is_extern, tc_metaclass_t meta, class CTcDictEntry *dict)
        : CTcSymObj(str, len, copy, obj_id, is_extern, meta, dict)
    {
    }

    /* generate a call */
    virtual void gen_code_call(int discard, int argc, int varargs,
                               struct CTcNamedArgs *named_args);
};

/*
 *   property 
 */
class CTcSymProp: public CTcSymPropBase
{
public:
    CTcSymProp(const char *str, size_t len, int copy, tctarg_prop_id_t prop)
        : CTcSymPropBase(str, len, copy, prop) { }

    /* generate code to evaluate the symbol */
    virtual void gen_code(int discard);

    /* we can take the address of this symbol type */
    virtual void gen_code_addr();

    /* assign to the property */
    virtual int gen_code_asi(int discard, int phase,
                             tc_asitype_t typ, const char *op,
                             class CTcPrsNode *rhs,
                             int ignore_errors, int xplicit, void **ctx);

    /* generate a call */
    virtual void gen_code_call(int discard, int argc, int varargs,
                               struct CTcNamedArgs *named_args);

    /* evaluate a property ID */
    vm_prop_id_t gen_code_propid(int check_only, int is_expr);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);

    /* get the object value for a '.' expression */
    vm_obj_id_t gen_code_obj_predot(int *is_self)
    {
        /* simply generate our code as normal */
        gen_code(FALSE);

        /* indicate that the caller must use generated code */
        *is_self = FALSE;
        return VM_INVALID_OBJ;
    }

    /* write the symbol to an image file's global symbol table */
    int write_to_image_file_global(class CVmImageWriter *image_writer);

    /* add a runtime symbol table entry */
    void add_runtime_symbol(class CVmRuntimeSymbols *symtab);
};

/*
 *   Enumerator 
 */
class CTcSymEnum: public CTcSymEnumBase
{
public:
    CTcSymEnum(const char *str, size_t len, int copy, ulong id, int is_token)
        : CTcSymEnumBase(str, len, copy, id, is_token)
    {
    }

    /* generate code */
    virtual void gen_code(int discard);

    /* write the symbol to an image file's global symbol table */
    int write_to_image_file_global(class CVmImageWriter *image_writer);

    /* add a runtime symbol table entry */
    void add_runtime_symbol(class CVmRuntimeSymbols *symtab);
};

/*
 *   local variable/parameter 
 */
class CTcSymLocal: public CTcSymLocalBase
{
public:
    CTcSymLocal(const char *str, size_t len, int copy,
                int is_param, int var_num)
        : CTcSymLocalBase(str, len, copy, is_param, var_num) { }

    /* generate code to evaluate the symbol */
    virtual void gen_code(int discard);

    /* generate code to store the value at top of stack in the variable */
    void gen_code_setlcl_stk() { s_gen_code_setlcl_stk(var_num_, is_param_); }

    /* generate code to get a local with a given ID */
    static void s_gen_code_getlcl(int local_id, int is_param);

    /* generate code to store a local with a given ID */
    static void s_gen_code_setlcl_stk(int local_id, int is_param);

    /* generate code to store the value at top of stack in the local */
    void gen_code_setlcl();

    /* assign to the variable */
    virtual int gen_code_asi(int discard, int phase,
                             tc_asitype_t typ, const char *op,
                             class CTcPrsNode *rhs,
                             int ignore_errors, int xplicit,
                             void **ctx);

    /* 
     *   generate a call - invoking a local as a function assumes that the
     *   local contains a method or function pointer 
     */
    virtual void gen_code_call(int discard, int argc, int varargs,
                               struct CTcNamedArgs *named_args);

    /* evaluate a property ID */
    vm_prop_id_t gen_code_propid(int check_only, int is_expr);

    /* generate a member expression */
    void gen_code_member(int discard,
                         class CTcPrsNode *prop_expr, int prop_is_expr,
                         int argc, int varargs,
                         struct CTcNamedArgs *named_args);

    /* get the object value for a '.' expression */
    vm_obj_id_t gen_code_obj_predot(int *is_self)
    {
        /* simply generate our code as normal */
        gen_code(FALSE);

        /* indicate that the caller must use generated code */
        *is_self = FALSE;
        return VM_INVALID_OBJ;
    }

    /* write the symbol to a debug frame */
    virtual int write_to_debug_frame(int test_only);
};

/*
 *   Run-time dynamic code local variable symbol.  This is the symbol type
 *   for a local accessed via a StackFrameRef object, for use in DynamicFunc
 *   compilations.  
 */
class CTcSymDynLocal: public CTcSymDynLocalBase
{
public:
    CTcSymDynLocal(const char *str, size_t len, int copy,
                   tctarg_obj_id_t fref, int varnum, int ctxidx)
        : CTcSymDynLocalBase(str, len, copy, fref, varnum, ctxidx)
    {
    }

    /* generate code */
    void gen_code(int discard);

    /* generate code for assigning into this node */
    int gen_code_asi(int discard, int phase, tc_asitype_t typ, const char *op,
                     class CTcPrsNode *rhs,
                     int ignore_errors, int xplicit, void **ctx);
};

/*
 *   built-in function 
 */
class CTcSymBif: public CTcSymBifBase
{
public:
    CTcSymBif(const char *str, size_t len, int copy,
              ushort func_set_id, ushort func_idx, int has_retval,
              int min_argc, int max_argc, int varargs)
        : CTcSymBifBase(str, len, copy, func_set_id, func_idx,
                        has_retval, min_argc, max_argc, varargs) { }

    /* generate code to evaluate the symbol */
    virtual void gen_code(int discard);

    /* generate a call */
    virtual void gen_code_call(int discard, int argc, int varargs,
                               struct CTcNamedArgs *named_args);

    /* generate code for an address '&' operator */
    virtual void gen_code_addr();

    /* write the symbol to an image file's global symbol table */
    int write_to_image_file_global(class CVmImageWriter *image_writer);

    /* add a runtime symbol table entry */
    void add_runtime_symbol(class CVmRuntimeSymbols *symtab);

};

/*
 *   external function 
 */
class CTcSymExtfn: public CTcSymExtfnBase
{
public:
    CTcSymExtfn(const char *str, size_t len, int copy,
                int argc, int varargs, int has_retval)
        : CTcSymExtfnBase(str, len, copy, argc, varargs, has_retval) { }

    /* generate code to evaluate the symbol */
    virtual void gen_code(int discard);

    /* generate a call */
    virtual void gen_code_call(int discard, int argc, int varargs,
                               struct CTcNamedArgs *named_args);

    /* write the symbol to an image file's global symbol table */
    int write_to_image_file_global(class CVmImageWriter *image_writer);
};

/*
 *   code label 
 */
class CTcSymLabel: public CTcSymLabelBase
{
public:
    CTcSymLabel(const char *str, size_t len, int copy)
        : CTcSymLabelBase(str, len, copy) { }

    /* generate code to evaluate the symbol */
    virtual void gen_code(int discard);
};

/* ------------------------------------------------------------------------ */
/*
 *   Anonymous function 
 */
class CTPNAnonFunc: public CTPNAnonFuncBase
{
public:
    CTPNAnonFunc(class CTPNCodeBody *code_body, int has_retval, int is_method)
        : CTPNAnonFuncBase(code_body, has_retval, is_method)
    {
    }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   Code Body 
 */
class CTPNCodeBody: public CTPNCodeBodyBase
{
public:
    CTPNCodeBody(class CTcPrsSymtab *lcltab, class CTcPrsSymtab *gototab,
                 class CTPNStm *stm, int argc, int opt_argc, int varargs,
                 int varargs_list, class CTcSymLocal *varargs_list_local,
                 int local_cnt, int self_valid,
                 struct CTcCodeBodyRef *enclosing_code_body)
        : CTPNCodeBodyBase(lcltab, gototab, stm, argc, opt_argc, varargs,
                           varargs_list, varargs_list_local,
                           local_cnt, self_valid, enclosing_code_body)
    {
        /* presume I'm not a constructor */
        is_constructor_ = FALSE;

        /* presume I won't need a 'finally' return value holder local */
        allocated_fin_ret_lcl_ = FALSE;
        fin_ret_lcl_ = 0;

        /* presume it won't be static */
        is_static_ = FALSE;

        /* no nested symbol table list yet */
        first_nested_symtab_ = 0;

        /* no named argument tables yet */
        named_arg_tables_ = 0;
    }

    /* 
     *   Mark the code body as belonging to a constructor.  A constructor
     *   must return its 'self' object as the return value.  
     */
    void set_constructor(int f) { is_constructor_ = f; }

    /* 
     *   mark the code body as static - this indicates that it is a static
     *   initializer
     */
    void set_static() { is_static_ = TRUE; }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* check locals */
    virtual void check_locals();

    /*
     *   Allocate a local variable for temporarily saving the expression
     *   value of a 'return' statement while calling the enclosing
     *   'finally' block(s).  Returns the variable ID.  Only one such
     *   local is needed per code body, so if we've already allocated one
     *   for another 'return' statement, we'll just return the same one we
     *   allocated previously.  
     */
    uint alloc_fin_ret_lcl()
    {
        /* if we haven't allocated it yet, do so now */
        if (!allocated_fin_ret_lcl_)
        {
            /* use the next available local */
            fin_ret_lcl_ = local_cnt_++;

            /* note that we've allocated it */
            allocated_fin_ret_lcl_ = TRUE;
        }

        /* return the local ID */
        return fin_ret_lcl_;
    }

    /* 
     *   Add a named argument table to the code body.  Each call that has
     *   named arguments requires one of these tables.  We keep a list of
     *   tables, and generate them all at the end of the method.  Returns a
     *   label that can be used to write a forward reference offset to the
     *   table's offset in the code stream.  
     */
    struct CTcCodeLabel *add_named_arg_tab(
        const struct CTcNamedArgs *named_args);

protected:
    /* 
     *   callback for enumerating local frame symbol table entries - write
     *   each entry to the code stream as a debug record 
     */
    static void write_local_to_debug_frame(void *ctx, class CTcSymbol *sym);

    /* 
     *   enumerator for checking for parameter symbols that belong in the
     *   local context 
     */
    static void enum_for_param_ctx(void *ctx, class CTcSymbol *sym);

    /* enumerator for generating named parameter bindings */
    static void enum_for_named_params(void *ctx, class CTcSymbol *sym);

    /* write the debug information table to the code stream */
    void build_debug_table(ulong start_ofs, int include_lines);

    /* show disassembly */
    void show_disassembly(unsigned long start_ofs,
                          unsigned long code_start_ofs,
                          unsigned long code_end_ofs);

    /* head of list of nested symbol tables */
    class CTcPrsSymtab *first_nested_symtab_;

    /* head of list of named argument tables */
    struct CTcNamedArgTab *named_arg_tables_;

    /* 
     *   ID of local variable for temporarily storing the expression value
     *   of a 'return' statement while calling the enclosing 'finally'
     *   block(s); valid only when allocated_fin_ret_lcl_ is true 
     */
    uint fin_ret_lcl_;

    /* flag: I'm a constructor */
    uint is_constructor_ : 1;

    /* 
     *   flag: I've allocated a local for saving the expression value of a
     *   'return' statement while calling the enclosing 'finally' block(s) 
     */
    uint allocated_fin_ret_lcl_ : 1;

    /* flag: I'm a static property initializer */
    uint is_static_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Compound Statement
 */
class CTPNStmComp: public CTPNStmCompBase
{
public:
    CTPNStmComp(class CTPNStm *first_stm, class CTcPrsSymtab *symtab)
        : CTPNStmCompBase(first_stm, symtab) { }
};

/* ------------------------------------------------------------------------ */
/*
 *   Null statement 
 */
class CTPNStmNull: public CTPNStmNullBase
{
public:
    CTPNStmNull() { }
};

/* ------------------------------------------------------------------------ */
/*
 *   Expression statement 
 */
class CTPNStmExpr: public CTPNStmExprBase
{
public:
    CTPNStmExpr(class CTcPrsNode *expr)
        : CTPNStmExprBase(expr) { }
};

/* ------------------------------------------------------------------------ */
/*
 *   Static property initializer statement
 */
class CTPNStmStaticPropInit: public CTPNStmStaticPropInitBase
{
public:
    CTPNStmStaticPropInit(class CTcPrsNode *expr,
                          tctarg_prop_id_t prop)
        : CTPNStmStaticPropInitBase(expr, prop) { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   'if' statement 
 */
class CTPNStmIf: public CTPNStmIfBase
{
public:
    CTPNStmIf(class CTcPrsNode *cond_expr,
              class CTPNStm *then_part, class CTPNStm *else_part)
        : CTPNStmIfBase(cond_expr, then_part, else_part) { }
    
    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   'for' statement 
 */
class CTPNStmFor: public CTPNStmForBase
{
public:
    CTPNStmFor(class CTcPrsNode *init_expr,
               class CTcPrsNode *cond_expr,
               class CTcPrsNode *reinit_expr,
               class CTPNForIn *in_exprs,
               class CTcPrsSymtab *symtab,
               class CTPNStmEnclosing *enclosing_stm)
        : CTPNStmForBase(init_expr, cond_expr, reinit_expr, in_exprs,
                         symtab, enclosing_stm)
    {
    }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate code for 'break' */
    virtual int gen_code_break(const textchar_t *lbl, size_t lbl_len)
        { return gen_code_break_loop(break_lbl_, lbl, lbl_len); }

    /* generate code for 'continue' */
    virtual int gen_code_labeled_continue()
        { return gen_code_continue_loop(cont_lbl_, 0, 0); }

    /* generate code for a labeled 'continue' */
    virtual int gen_code_continue(const textchar_t *lbl, size_t lbl_len)
        { return gen_code_continue_loop(cont_lbl_, lbl, lbl_len); }

protected:
    /* our 'break' label */
    struct CTcCodeLabel *break_lbl_;

    /* our 'continue' label */
    struct CTcCodeLabel *cont_lbl_;
};

/* ------------------------------------------------------------------------ */
/*
 *   '<variable> in <expression>' node, for 'for' statements 
 */
class CTPNVarIn: public CTPNVarInBase
{
public:
    CTPNVarIn(class CTcPrsNode *lval, class CTcPrsNode *expr,
              int iter_local_id)
        : CTPNVarInBase(lval, expr, iter_local_id) { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate the condition part of the 'for' */
    virtual void gen_forstm_cond(struct CTcCodeLabel *endlbl);

    /* generate the reinit part of the 'for' */
    virtual void gen_forstm_reinit();

    /* 
     *   Generate the iterator initializer.  This is a static method so that
     *   it can be shared by for..in and foreach..in.  'kw' is the keyword
     *   for the statement type that we're generating ("for" or "foreach"),
     *   so that we can show the appropriate statement type in any error
     *   messages.  
     */
    static void gen_iter_init(class CTcPrsNode *coll_expr, int iter_local_id,
                              const char *kw);

    /* 
     *   Generate the iterator condition.  This is a static method so that it
     *   can be shared by for..in and foreach..in. 
     */
    static void gen_iter_cond(class CTcPrsNode *lval, int iter_local_id,
                              struct CTcCodeLabel *&endlbl, const char *kw);
};

/*
 *   '<variable> in <from> .. <to>' node, for 'for' statements 
 */
class CTPNVarInRange: public CTPNVarInRangeBase
{
public:
    CTPNVarInRange(class CTcPrsNode *lval,
                   class CTcPrsNode *from_expr,
                   class CTcPrsNode *to_expr,
                   class CTcPrsNode *step_expr,
                   int to_local_id, int step_local_id)
        : CTPNVarInRangeBase(lval, from_expr, to_expr, step_expr,
                             to_local_id, step_local_id)
    { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate the condition part of the 'for' */
    virtual void gen_forstm_cond(struct CTcCodeLabel *endlbl);

    /* generate the reinit part of the 'for' */
    virtual void gen_forstm_reinit();
};


/* ------------------------------------------------------------------------ */
/*
 *   'for' statement 
 */
class CTPNStmForeach: public CTPNStmForeachBase
{
public:
    CTPNStmForeach(class CTcPrsNode *iter_expr,
                   class CTcPrsNode *coll_expr,
                   class CTcPrsSymtab *symtab,
                   class CTPNStmEnclosing *enclosing_stm,
                   int iter_local_id)
        : CTPNStmForeachBase(iter_expr, coll_expr, symtab, enclosing_stm,
                             iter_local_id)
    {
    }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate code for 'break' */
    virtual int gen_code_break(const textchar_t *lbl, size_t lbl_len)
        { return gen_code_break_loop(break_lbl_, lbl, lbl_len); }

    /* generate code for 'continue' */
    virtual int gen_code_labeled_continue()
        { return gen_code_continue_loop(cont_lbl_, 0, 0); }

    /* generate code for a labeled 'continue' */
    virtual int gen_code_continue(const textchar_t *lbl, size_t lbl_len)
        { return gen_code_continue_loop(cont_lbl_, lbl, lbl_len); }

protected:
    /* our 'break' label */
    struct CTcCodeLabel *break_lbl_;

    /* our 'continue' label */
    struct CTcCodeLabel *cont_lbl_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'break' statement 
 */
class CTPNStmBreak: public CTPNStmBreakBase
{
public:
    CTPNStmBreak() { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   'continue' statement 
 */
class CTPNStmContinue: public CTPNStmContinueBase
{
public:
    CTPNStmContinue() { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   'while' statement 
 */
class CTPNStmWhile: public CTPNStmWhileBase
{
public:
    CTPNStmWhile(class CTcPrsNode *cond_expr,
                 class CTPNStmEnclosing *enclosing_stm)
        : CTPNStmWhileBase(cond_expr, enclosing_stm) { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate code for 'break' */
    virtual int gen_code_break(const textchar_t *lbl, size_t lbl_len)
        { return gen_code_break_loop(break_lbl_, lbl, lbl_len); }

    /* generate code for 'continue' */
    virtual int gen_code_labeled_continue()
        { return gen_code_continue_loop(cont_lbl_, 0, 0); }

    /* generate code for a labeled 'continue' */
    virtual int gen_code_continue(const textchar_t *lbl, size_t lbl_len)
        { return gen_code_continue_loop(cont_lbl_, lbl, lbl_len); }

protected:
    /* our 'break' label */
    struct CTcCodeLabel *break_lbl_;

    /* our 'continue' label */
    struct CTcCodeLabel *cont_lbl_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'do-while' statement 
 */
class CTPNStmDoWhile: public CTPNStmDoWhileBase
{
public:
    CTPNStmDoWhile(class CTPNStmEnclosing *enclosing_stm)
        : CTPNStmDoWhileBase(enclosing_stm) { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate code for 'break' */
    virtual int gen_code_break(const textchar_t *lbl, size_t lbl_len)
        { return gen_code_break_loop(break_lbl_, lbl, lbl_len); }

    /* generate code for 'continue' */
    virtual int gen_code_labeled_continue()
        { return gen_code_continue_loop(cont_lbl_, 0, 0); }

    /* generate code for a labeled 'continue' */
    virtual int gen_code_continue(const textchar_t *lbl, size_t lbl_len)
        { return gen_code_continue_loop(cont_lbl_, lbl, lbl_len); }

protected:
    /* our 'break' label */
    struct CTcCodeLabel *break_lbl_;

    /* our 'continue' label */
    struct CTcCodeLabel *cont_lbl_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'return' statement 
 */
class CTPNStmReturn: public CTPNStmReturnBase
{
public:
    CTPNStmReturn(class CTcPrsNode *expr)
        : CTPNStmReturnBase(expr) { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   'switch' statement 
 */
class CTPNStmSwitch: public CTPNStmSwitchBase
{
public:
    CTPNStmSwitch(class CTPNStmEnclosing *enclosing_stm)
        : CTPNStmSwitchBase(enclosing_stm)
    {
        /* the 'case' and 'default' slot pointers aren't valid yet */
        case_slot_ofs_ = 0;
        default_slot_ofs_ = 0;
    }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* 
     *   Get the next case slot offset, and consume the slot.  Each call
     *   consumes one slot.  This is valid only during code generation in
     *   the switch body; 'case' labels use this to figure out where to
     *   write their data.  
     */
    ulong alloc_case_slot()
    {
        ulong ret;

        /* note the value to return */
        ret = case_slot_ofs_;

        /* move our internal counter to the next slot */
        case_slot_ofs_ += VMB_DATAHOLDER + VMB_UINT2;

        /* return the allocated slot */
        return ret;
    }

    /* get the code offset of the 'default' case slot */
    ulong get_default_slot() const { return default_slot_ofs_; }

    /* generate code for 'break' */
    virtual int gen_code_break(const textchar_t *lbl, size_t lbl_len)
    {
        /* 
         *   if there's no label, and we don't have a 'break' label
         *   ourselves, it must mean that we're processing an unreachable
         *   'break' - we can simply do nothing in this case and pretend we
         *   succeeded 
         */
        if (lbl == 0 && break_lbl_ == 0)
            return TRUE;

        /* break to our break label */
        return gen_code_break_loop(break_lbl_, lbl, lbl_len);
    }

protected:
    /* 
     *   code offset of the next 'case' slot to be allocated from the
     *   switch's case table - this is only valid during generation of the
     *   body, because we set this up during generation of the switch
     *   itself 
     */
    ulong case_slot_ofs_;

    /* code offset of the 'default' slot */
    ulong default_slot_ofs_;

    /* our 'break' label */
    struct CTcCodeLabel *break_lbl_;
};


/* ------------------------------------------------------------------------ */
/*
 *   code label statement 
 */
class CTPNStmLabel: public CTPNStmLabelBase
{
public:
    CTPNStmLabel(class CTcSymLabel *lbl, class CTPNStmEnclosing *enclosing);

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate code for 'break' */
    virtual int gen_code_break(const textchar_t *lbl, size_t lbl_len);

    /* generate code for 'continue' */
    virtual int gen_code_continue(const textchar_t *lbl, size_t lbl_len);

    /* 
     *   Generate code for a labeled 'continue'.  We'll simply generate
     *   code for a labeled continue in our enclosed statement.
     *   
     *   (This is normally a routine that *we* call on our contained
     *   statement in response to a 'continue' being targeted at this
     *   label.  This can be invoked on us, however, when a statement has
     *   multiple labels, and the outer label is mentioned in the 'break'
     *   -- the outer label will call this routine on the inner label,
     *   which is us, so we simply need to pass it along to the enclosed
     *   statement.)  
     */
    virtual int gen_code_labeled_continue()
        { return (stm_ != 0 ? stm_->gen_code_labeled_continue() : FALSE); }

    /* get my 'goto' label */
    struct CTcCodeLabel *get_goto_label();

protected:
    /* the byte label to jump to this statement */
    struct CTcCodeLabel *goto_label_;

    /* the byte label to break from this statement */
    struct CTcCodeLabel *break_label_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'case' label statement 
 */
class CTPNStmCase: public CTPNStmCaseBase
{
public:
    CTPNStmCase() { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   'default' label statement 
 */
class CTPNStmDefault: public CTPNStmDefaultBase
{
public:
    CTPNStmDefault() { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   'try' block 
 */
class CTPNStmTry: public CTPNStmTryBase
{
public:
    CTPNStmTry(class CTPNStmEnclosing *enclosing)
        : CTPNStmTryBase(enclosing)
    {
    }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate code for 'break' */
    virtual int gen_code_break(const textchar_t *lbl, size_t lbl_len);

    /* generate code for 'continue' */
    virtual int gen_code_continue(const textchar_t *lbl, size_t lbl_len);

    /* 
     *   Generate the code necessary to unwind for returning.  We must
     *   generate a call to our 'finally' code, then unwind any enclosing
     *   statements. 
     */
    virtual void gen_code_unwind_for_return()
    {
        /* call 'finally' on the way out */
        gen_jsr_finally();

        /* inherit default to unwind enclosing statements */
        CTPNStmTryBase::gen_code_unwind_for_return();
    }

    /* determine if we'll generate code for unwinding from a return */
    virtual int will_gen_code_unwind_for_return() const
    {
        /* we'll generate a call to our 'finally' block if we have one */
        if (finally_stm_ != 0)
            return TRUE;

        /* we'll also generate code if any enclosing statement does */
        return CTPNStmTryBase::will_gen_code_unwind_for_return();
    }

    /* generate the code necessary to transfer out of the block */
    virtual void gen_code_for_transfer_out()
    {
        /* call 'finally' on the way out */
        gen_jsr_finally();
    }

    /* 
     *   generate a local subroutine call to our 'finally' block, if we
     *   have a 'finally' block - this should be invoked whenever code
     *   within the protected block or a 'catch' block executes a break,
     *   continue, return, or goto out of the protected region, or when
     *   merely falling off the end of the protected block or a 'catch'
     *   block.  
     */
    void gen_jsr_finally();

    /* get my 'finally' label */
    struct CTcCodeLabel *get_finally_lbl() const { return finally_lbl_; }
    
protected:
    /* our 'finally' label */
    struct CTcCodeLabel *finally_lbl_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'catch'
 */
class CTPNStmCatch: public CTPNStmCatchBase
{
public:
    CTPNStmCatch() { }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate code for the 'catch' clause */
    virtual void gen_code_catch(ulong protected_start_ofs,
                                ulong protected_end_ofs);
};

/* ------------------------------------------------------------------------ */
/*
 *   'finally' 
 */
class CTPNStmFinally: public CTPNStmFinallyBase
{
public:
    CTPNStmFinally(CTPNStmEnclosing *enclosing,
                   int exc_local_id, int jsr_local_id)
        : CTPNStmFinallyBase(enclosing, exc_local_id, jsr_local_id)
    {
    }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate code for the 'finally' clause */
    virtual void gen_code_finally(ulong protected_start_ofs,
                                  ulong protected_end_ofs,
                                  class CTPNStmTry *try_stm);

    /* check for transfer in via 'goto' statements */
    virtual int check_enter_by_goto(class CTPNStmGoto *goto_stm,
                                    class CTPNStmLabel *target);
};


/* ------------------------------------------------------------------------ */
/*
 *   'throw' statement 
 */
class CTPNStmThrow: public CTPNStmThrowBase
{
public:
    CTPNStmThrow(class CTcPrsNode *expr)
        : CTPNStmThrowBase(expr)
    {
    }
    
    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   'goto' statement 
 */
class CTPNStmGoto: public CTPNStmGotoBase
{
public:
    CTPNStmGoto(const textchar_t *lbl, size_t lbl_len)
        : CTPNStmGotoBase(lbl, lbl_len)
    {
    }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   'dictionary' statement 
 */
class CTPNStmDict: public CTPNStmDictBase
{
public:
    CTPNStmDict(class CTcDictEntry *dict)
        : CTPNStmDictBase(dict)
    {
    }

    /* generate code */
    virtual void gen_code(int, int) { }
};

/* ------------------------------------------------------------------------ */
/*
 *   Object Definition 
 */
class CTPNStmObject: public CTPNStmObjectBase
{
public:
    CTPNStmObject(class CTcSymObj *obj_sym, int is_class)
        : CTPNStmObjectBase(obj_sym, is_class)
    {
    }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* check locals */
    virtual void check_locals();

    /* 
     *   given the stream offset for the start of the object data in the
     *   stream, get information on the object from the stream data:
     *   
     *.  - get the number of properties in the stream data 
     *.  - set the number of properties in the stream data, updating the
     *   stored data size in the metaclass header in the stream; returns
     *   the new data size
     *.  - get the number of superclasses in the stream data
     *.  - get a superclass object ID
     *.  - get the offset of the first property in the stream data
     *.  - get the offset of the property at the given index
     *.  - get the ID of the object's property at the given index
     *.  - set the ID of the object's property at the given index
     *.  - get the object flags from the tads-object header
     *.  - set the object flags in the tads-object heade 
     */
    static uint get_stream_prop_cnt(class CTcDataStream *stream,
                                    ulong obj_ofs);
    static size_t set_stream_prop_cnt(class CTcDataStream *stream,
                                      ulong obj_ofs, uint prop_cnt);
    static uint get_stream_sc_cnt(class CTcDataStream *stream,
                                  ulong obj_ofs);
    static vm_obj_id_t get_stream_sc(class CTcDataStream *stream,
                                     ulong obj_ofs, uint sc_idx);
    static void set_stream_sc(class CTcDataStream *stream,
                              ulong obj_ofs, uint sc_idx, vm_obj_id_t new_sc);
    static ulong get_stream_first_prop_ofs(class CTcDataStream *stream,
                                           ulong obj_ofs);
    static ulong get_stream_prop_ofs(class CTcDataStream *stream,
                                     ulong obj_ofs, uint prop_idx);
    static vm_prop_id_t get_stream_prop_id(class CTcDataStream *stream,
                                           ulong obj_ofs, uint prop_idx);
    static enum vm_datatype_t
        get_stream_prop_type(class CTcDataStream *stream,
                             ulong obj_ofs, uint prop_idx);
    static ulong get_stream_prop_val_ofs(class CTcDataStream *stream,
                                         ulong obj_ofs, uint prop_idx);
    static void set_stream_prop_id(class CTcDataStream *stream,
                                   ulong obj_ofs, uint prop_idx,
                                   vm_prop_id_t new_id);
    static uint get_stream_obj_flags(class CTcDataStream *stream,
                                     ulong obj_ofs);
    static void set_stream_obj_flags(class CTcDataStream *stream,
                                     ulong obj_ofs, uint flags);
};

/*
 *   Inline object definition (defined within an expression)
 */
class CTPNInlineObject: public CTPNInlineObjectBase
{
public:
    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};


/*
 *   Property Value list entry 
 */
class CTPNObjProp: public CTPNObjPropBase
{
public:
    CTPNObjProp(class CTPNObjDef *objdef, class CTcSymProp *prop_sym,
                class CTcPrsNode *expr, class CTPNCodeBody *code_body,
                class CTPNAnonFunc *inline_method, int is_static)
        : CTPNObjPropBase(
            objdef, prop_sym, expr, code_body, inline_method, is_static)
    {
    }
    
    /* generate code */
    virtual void gen_code(int discard, int for_condition);

    /* generate code for an inline object instantiation */
    void gen_code_inline_obj();

    /* check locals */
    void check_locals();

protected:
    /* generate a setMethod() call for inline object creation */
    void gen_setMethod();
};

/*
 *   Implicit constructor 
 */
class CTPNStmImplicitCtor: public CTPNStmImplicitCtorBase
{
public:
    CTPNStmImplicitCtor(class CTPNStmObject *obj_stm)
        : CTPNStmImplicitCtorBase(obj_stm)
    {
    }

    /* generate code */
    virtual void gen_code(int discard, int for_condition);
};


#endif /* TCT3DRV_H */
