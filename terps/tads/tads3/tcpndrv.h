/* $Header: d:/cvsroot/tads/tads3/TCPNDRV.H,v 1.4 1999/07/11 00:46:53 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcpndrv.h - parse node base classes derived from CTcPrsNode
Function
  This file contains the intermediate classes derived from CTcPrsNode.

  CTcPrsNode is defined separately for each target code generator.
  These classes are separated into this header file so that they can
  be defined after the target-specific CTcPrsNode header has been
  included, which in turn must be included after the base parse node
  file, tcpnbase.h.  Finally, after this file, the target-specific
  derived classes header can be included.

  The sequence is thus:

  tcpnbase.h  - target-independent CTcPrsNodeBase
  ???.h       - target-specific CTcPrsNode definition (file varies by target)
  tcpnint.h   - target-independent intermediate classes
  ???.h       - target-specific intermediate classes
  tcpndrv.h   - target-independent derived CTPNXxxBase classes
  ???.h       - target-specific derived CTPNXxx final classes
Notes
  
Modified
  05/10/99 MJRoberts  - Creation
*/

#ifndef TCPNDRV_H
#define TCPNDRV_H

/* ------------------------------------------------------------------------ */
/*
 *   "argcount" node 
 */
class CTPNArgcBase: public CTcPrsNode
{
public:
    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* simply return myself unchanged */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }
};

/* ------------------------------------------------------------------------ */
/*
 *   "self" node 
 */
class CTPNSelfBase: public CTcPrsNode
{
public:
    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* simply return myself unchanged */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }
};

/* ------------------------------------------------------------------------ */
/*
 *   "targetprop" node 
 */
class CTPNTargetpropBase: public CTcPrsNode
{
public:
    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* simply return myself unchanged */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }
};


/* ------------------------------------------------------------------------ */
/*
 *   "targetobj" node 
 */
class CTPNTargetobjBase: public CTcPrsNode
{
public:
    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* simply return myself unchanged */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }
};


/* ------------------------------------------------------------------------ */
/*
 *   "definingobj" node 
 */
class CTPNDefiningobjBase: public CTcPrsNode
{
public:
    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *)
    {
        /* simply return myself unchanged */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }
};

/* ------------------------------------------------------------------------ */
/*
 *   "invokee" node 
 */
class CTPNInvokeeBase: public CTcPrsNode
{
public:
    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *) { return this; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }
};


/* ------------------------------------------------------------------------ */
/*
 *   "inherited" node 
 */
class CTPNInhBase: public CTcPrsNode
{
public:
    CTPNInhBase()
    {
        typelist_ = 0;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* simply return myself unchanged */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }

    /* get/set the formal type list */
    class CTcFormalTypeList *get_typelist() const { return typelist_; }
    void set_typelist(class CTcFormalTypeList *l) { typelist_ = l; }

protected:
    class CTcFormalTypeList *typelist_;
};

/*
 *   "inherited class" node - for (inherited superclass.method) construct 
 */
class CTPNInhClassBase: public CTcPrsNode
{
public:
    CTPNInhClassBase(const char *sym, size_t len)
    {
        sym_ = sym;
        len_ = len;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* simply return myself unchanged */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }

protected:
    /* the superclass name symbol and its length */
    const char *sym_;
    size_t len_;
};


/*
 *   "delegated" node - for (delegated expr.method) construct 
 */
class CTPNDelegatedBase: public CTcPrsNode
{
public:
    CTPNDelegatedBase(CTcPrsNode *delegatee)
    {
        delegatee_ = delegatee;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* fold constants in the delegatee */
        delegatee_ = delegatee_->fold_constants(symtab);

        /* return myself otherwise unchanged */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        /* adjust the delegatee subexpression */
        delegatee_ = delegatee_->adjust_for_dyn(info);

        /* return myself otherwise unchanged */
        return this;
    }

protected:
    /* the delegatee (the object to which we delegate the method) */
    CTcPrsNode *delegatee_;
};

/*
 *   "replaced" node 
 */
class CTPNReplacedBase: public CTcPrsNode
{
public:
    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* simply return myself unchanged */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }
};


/* ------------------------------------------------------------------------ */
/*
 *   Parse node for constant values (numbers, strings) 
 */
class CTPNConstBase: public CTcPrsNode
{
public:
    CTPNConstBase(const CTcConstVal *val) { val_ = *val; }

    /* get the constant value of the node */
    virtual class CTcConstVal *get_const_val() { return &val_; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* simply return myself unchanged */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);

    /* check to see if this is a boolean value */
    virtual int is_bool() const { return val_.is_bool(); }

protected:
    CTcConstVal val_;
};

/*
 *   The target must define a subclass, CTPNDebugConst, for use when
 *   evaluating debugger expressions.
 */
/* class CTPNDebugConst: public CTPNConst ... */

/* ------------------------------------------------------------------------ */
/*
 *   Address-of Operator 
 */
class CTPNAddrBase: public CTPNUnary
{
public:
    CTPNAddrBase(CTcPrsNode *sub)
        : CTPNUnary(sub) { }

    /* this is an address expression */
    virtual int is_addr() const { return TRUE; }

    /*
     *   Determine if this address expression is the same as a given
     *   address expression.  Returns true if so, false if not.  Sets
     *   (*comparable) to true if the comparison is meaningful, false if
     *   not; if (*comparable) is set to false, the caller should not
     *   attempt to perform a compile-time comparison of the addresses,
     *   and must defer the comparison to run-time.  
     */
    int is_addr_eq(const class CTPNAddr *, int *comparable) const;

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);
};

/* ------------------------------------------------------------------------ */
/*
 *   Symbol node.  This node handles symbols that are not resolved at
 *   parse time, and whose resolution must be deferred to code generation.
 *   Any symbol that doesn't resolve at parse time to something in local
 *   scope uses this type of node.  
 */
class CTPNSymBase: public CTcPrsNode
{
public:
    CTPNSymBase(const char *sym, size_t len)
    {
        /* remember my symbol name */
        sym_ = sym;
        len_ = len;
    }

    /* 
     *   Assume that a simple symbol primary can be used as an lvalue.
     *   Some symbols are not suitable as lvalues, but we can't know this
     *   at parse time, so assume that an lvalue is okay for now.  
     */
    virtual int check_lvalue() const { return TRUE; }

    /* check to see if this is an lvalue, resolving in the given scope */
    virtual int check_lvalue_resolved(class CTcPrsSymtab *symtab) const;

    /*
     *   At parse time, assume that we can take the address of a simple
     *   symbol primary. 
     */
    virtual int has_addr() const { return TRUE; }

    /* determine if I have a return value when called */
    virtual int has_return_value_on_call() const;

    /* get my symbol information */
    const char *get_sym_text() const { return sym_; }
    size_t get_sym_text_len() const { return len_; }
    int sym_text_matches(const char *sym, size_t len) const
        { return (len_ == len && memcmp(sym, sym_, len) == 0); }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* generate a constant node for my address value */
    class CTcPrsNode *fold_addr_const(class CTcPrsSymtab *symtab);

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *);

protected:
    /* my symbol */
    const char *sym_;
    size_t len_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Resolved symbol node.  This node handles symbols that are resolve
 *   during parsing.  All local scope symbols, by the nature of the
 *   language, are resolvable during parsing, because local scope symbols
 *   must be declared before their first use.  
 */
class CTPNSymResolvedBase: public CTcPrsNode
{
public:
    CTPNSymResolvedBase(CTcSymbol *sym)
    {
        /* remember my symbol table entry */
        sym_ = sym;
    }

    /* get my symbol information */
    const char *get_sym_text() const { return sym_->get_sym(); }
    size_t get_sym_text_len() const { return sym_->get_sym_len(); }
    int sym_text_matches(const char *sym, size_t len) const
    {
        return (sym_->get_sym_len() == len
                && memcmp(sym, sym_->get_sym(), len) == 0);
    }

    /* determine if I can be an lvalue */
    virtual int check_lvalue() const { return sym_->check_lvalue(); }

    /* 
     *   check to see if this is an lvalue, resolving in the given scope -
     *   this type of symbol is pre-resolved, so we can ignore the symbol
     *   table and just ask our resoloved symbol directly 
     */
    virtual int check_lvalue_resolved(class CTcPrsSymtab *) const
        { return sym_->check_lvalue(); }

    /* determine if I have an address */
    virtual int has_addr() const { return sym_->has_addr(); }

    /* determine if I have a return value when called as a function */
    virtual int has_return_value_on_call() const
        { return sym_->has_return_value_on_call(); }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* generate a constant node for my address value */
    class CTcPrsNode *fold_addr_const(class CTcPrsSymtab *symtab);

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }

protected:
    /* the symbol table entry for this symbol */
    class CTcSymbol *sym_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Debugger local variable resolved symbol 
 */
class CTPNSymDebugLocalBase: public CTcPrsNode
{
public:
    CTPNSymDebugLocalBase(const struct tcprsdbg_sym_info *info);

    /* locals can always be lvalues */
    virtual int check_lvalue() const { return TRUE; }
    virtual int check_lvalue_resolved(class CTcPrsSymtab *) const
        { return TRUE; }

    /* determine if I have an address */
    virtual int has_addr() const { return FALSE; }

    /* 
     *   Determine if I have a return value when called as a function.  We
     *   have to assume that we do, since we can't know in advance what the
     *   variable will hold at run-time.  If it holds a function with a
     *   return value, we'll need to return the value; if it doesn't, the
     *   function will effectively return nil, so returning the non-result
     *   will do no harm.  
     */
    virtual int has_return_value_on_call() const { return TRUE; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *) { return this; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }

protected:
    /* local variable ID */
    uint var_id_;

    /* context array index for this variable, if I'm in a context */
    int ctx_arr_idx_;

    /* stack frame index */
    uint frame_idx_;

    /* is this a parameter? (if not, it's a local) */
    uint is_param_ : 1;

};

/* ------------------------------------------------------------------------ */
/*
 *   Argument List 
 */
class CTPNArglistBase: public CTcPrsNode
{
public:
    CTPNArglistBase(int argc, class CTPNArg *list_head)
    {
        /* remember the argument count and list head */
        argc_ = argc;
        list_ = list_head;
    }

    /* get/set the argument count */
    int get_argc() const { return argc_; }
    void set_argc(int argc) { argc_ = argc; }

    /* 
     *   Get the head argument of the list.  The other arguments can be
     *   obtained by following the linked list from this one.  Note that
     *   we build some argument lists in reverse order, with the last
     *   argument at the head of the list.  
     */
    class CTPNArg *get_arg_list_head() const { return list_; }

    /* set the argument list head */
    void set_arg_list_head(class CTPNArg *head) { list_ = head; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);

protected:
    /* 
     *   argument count (this is simply the number of entries in the list,
     *   but we keep it separately for easy access) 
     */
    int argc_;

    /* head of the argument list */
    class CTPNArg *list_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Argument list entry.  Each list entry has the expression for the
 *   argument and a pointer to the next element of the argument list.  
 */
class CTPNArgBase: public CTcPrsNode
{
public:
    CTPNArgBase(CTcPrsNode *arg_expr)
    {
        /* remember the expression for this argument */
        arg_expr_ = arg_expr;

        /* there's nothing else in our list yet */
        next_arg_ = 0;

        /* assume it's not a list-to-varargs conversion */
        is_varargs_ = FALSE;

        /* presume no argument name */
        name_.settyp(TOKT_INVALID);
    }

    /* set the parameter name, for a "name: value" argument */
    void set_name(const class CTcToken *tok);

    /* get the name and its length */
    const char *get_name() const { return name_.get_text(); }
    const size_t get_name_len() const { return name_.get_text_len(); }

    /* is this a named parameter? */
    int is_named_param() { return name_.gettyp() != TOKT_INVALID; }

    /* get the argument expression */
    CTcPrsNode *get_arg_expr() const { return arg_expr_; }

    /* get the next argument in my list */
    class CTPNArg *get_next_arg() const { return next_arg_; }

    /* set the next element in my list */
    void set_next_arg(class CTPNArg *prv) { next_arg_ = prv; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        /* adjust the expression */
        arg_expr_ = arg_expr_->adjust_for_dyn(info);

        /* return myself otherwise unchanged */
        return this;
    }

    /* get/set list-to-varargs conversion flag */
    int is_varargs() const { return is_varargs_; }
    void set_varargs(int f) { is_varargs_ = (f != 0); }

protected:
    /* argument expression */
    CTcPrsNode *arg_expr_;

    /* next argument in the list */
    class CTPNArg *next_arg_;

    /* the parameter name, for a "name: expression" argument */
    CTcToken name_;

    /* flag: this is a list-to-varargs parameter */
    unsigned int is_varargs_ : 1;
};



/* ------------------------------------------------------------------------ */
/*
 *   Function/Method Call node 
 */
class CTPNCallBase: public CTcPrsNode
{
public:
    CTPNCallBase(CTcPrsNode *func, class CTPNArglist *arglist)
    {
        /* remember the expression giving the function or method to call */
        func_ = func;

        /* remember the argument list */
        arglist_ = arglist;
    }

    /* get the function to call */
    CTcPrsNode *get_func() const { return func_; }

    /* get the argument list */
    class CTPNArglist *get_arg_list() const { return arglist_; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* we have a return value if our function has a return value */
    virtual int has_return_value() const
        { return func_->has_return_value_on_call(); }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);

protected:
    /* expression giving function or method to call */
    CTcPrsNode *func_;

    /* argument list */
    class CTPNArglist *arglist_;
};


/* ------------------------------------------------------------------------ */
/*
 *   double-quoted string node 
 */
class CTPNDstrBase: public CTcPrsNode
{
public:
    CTPNDstrBase(const char *str, size_t len);

    /* get my string information */
    const char *get_str() const { return str_; }
    size_t get_str_len() const { return len_; }

    /* I'm a double-quoted string node */
    virtual int is_dstring() const { return TRUE; }
    virtual int is_dstring_expr() const { return TRUE; }

    /* fold constants - there's nothing extra to do here */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
        { return this; }

    /* double-quoted strings have no value */
    virtual int has_return_value() const { return FALSE; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);

protected:
    /* my string */
    const char *str_;
    size_t len_;
};

/*
 *   The target must also define CTPNDebugDstr, a subclass of CTPNDstr.
 *   This for use in debugger expressions.  
 */
/* class CTPNDebugDstr: public CTPNDstr ... */

/*
 *   Double-quoted string embedding.  This node contains an expression
 *   embedded with the "<< >>" notation inside a double-quoted string.
 *   This type of expression requires an implicit say() call.  
 */
class CTPNDstrEmbedBase: public CTPNUnary
{
public:
    CTPNDstrEmbedBase(CTcPrsNode *sub)
        : CTPNUnary(sub) { }

    /* this is part of a dstring expression */
    virtual int is_dstring_expr() const { return TRUE; }

    /* we have no return value */
    virtual int has_return_value() const { return FALSE; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        /* don't allow in speculative mode due to side effects */
        if (info->speculative)
            err_throw(VMERR_BAD_SPEC_EVAL);

        /* inherit default unary operator handling */
        return CTPNUnary::adjust_for_dyn(info);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   An embedded <<one of>> list in a string.
 */
class CTPNStrOneOfBase: public CTcPrsNode
{
public:
    /*
     *   Create from a list of values, and the attribute string for selecting
     *   a value on each invocation.  The attributes string must be static
     *   data, since we don't make a copy.  Currently we only allow built-in
     *   attribute selections, which we define in a static structure.  
     */
    CTPNStrOneOfBase(int dstr, class CTPNList *lst, class CTcSymObj *state_obj)
    {
        dstr_ = dstr;
        lst_ = lst;
        state_obj_ = state_obj;
    }

    /* this is part of a dstring expression */
    virtual int is_dstring_expr() const { return TRUE; }

    /* adjust for dynamic compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);
    
    /* folder constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* 
     *   The overall "one of" list has a return value if it's in a
     *   single-quoted string, since the individual elements will themselves
     *   be represented as single-quoted strings.  If we're in a
     *   double-quoted string, the individual elements are double-quoted
     *   strings. 
     */
    int has_return_value() const { return !dstr_; }

protected:
    /* type of string - double quoted or single quoted */
    int dstr_;

    /* the underlying list of substrings to choose from */
    class CTPNList *lst_;

    /* the state object (an anonymous OneOfIndexGen instance) */
    class CTcSymObj *state_obj_;
};


/* ------------------------------------------------------------------------ */
/*
 *   List node.  A list has an underlying linked list of elements.
 */
class CTPNListBase: public CTcPrsNode
{
public:
    CTPNListBase()
    {
        /* there are initially no elements */
        cnt_ = 0;
        head_ = tail_ = 0;

        /* 
         *   initially, the list is constant, because it's just an empty
         *   list; as we add elements, we'll check for constants, and
         *   we'll clear the constant flag the first time we add a
         *   non-constant element 
         */
        is_const_ = TRUE;

        /* presume it's not a lookup table list */
        is_lookup_table_ = FALSE;

        /* set my constant value to point to myself */
        const_val_.set_list((class CTPNList *)this);
    }

    /* add an element to the list with a given expression */
    void add_element(CTcPrsNode *expr);

    /* remove each occurrence of a constant element from the list */
    void remove_element(const class CTcConstVal *val);

    /* get the constant value of the list */
    CTcConstVal *get_const_val()
    {
        /* 
         *   return our constant value only if we have one AND we're not a
         *   lookup table - a lookup table requires a NEW operation at
         *   run-time, so even if the underlying list of source values is
         *   constant, the overall list value isn't 
         */
        return (is_const_ && !is_lookup_table_ ? &const_val_ : 0);
    }

    /* 
     *   get the first/last element - other elements can be reached by
     *   traversing the linked list of elements starting with the head or
     *   tail 
     */
    class CTPNListEle *get_head() const { return head_; }
    class CTPNListEle *get_tail() const { return tail_; }

    /* get the constant element at the given index */
    CTcPrsNode *get_const_ele(int index);

    /* get the number of elements in the list */
    int get_count() const { return cnt_; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);

    /* mark it as a lookup table list */
    void set_lookup_table() { is_lookup_table_ = TRUE; }

protected:
    /* number of elements in the list */
    int cnt_;

    /* head and tail of list of elements */
    class CTPNListEle *head_;
    class CTPNListEle *tail_;

    /* 
     *   our constant value - this is valid only when all elements of the
     *   list are constant, as indicated by the is_const_ paremeter 
     */
    CTcConstVal const_val_;

    /* flag: all elements of the list are constant */
    unsigned int is_const_ : 1;

    /* flag: this is a LookupTable list */
    unsigned int is_lookup_table_ : 1;
};

/*
 *   List Element.  Each list element is represented by one of these
 *   objects; these are in turn linked together into a list.  
 */
class CTPNListEleBase: public CTcPrsNode
{
public:
    CTPNListEleBase(CTcPrsNode *expr)
    {
        /* remember the underlying expression */
        expr_ = expr;

        /* this element isn't in a list yet */
        next_ = 0;
        prev_ = 0;
    }

    /* get/set the next element */
    class CTPNListEle *get_next() const { return next_; }
    void set_next(class CTPNListEle *nxt) { next_ = nxt; }

    /* get/set the previous element */
    class CTPNListEle *get_prev() const { return prev_; }
    void set_prev(class CTPNListEle *prv) { prev_ = prv; }

    /* get the underlying expression */
    CTcPrsNode *get_expr() const { return expr_; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* fold constants in my expression */
        expr_ = expr_->fold_constants(symtab);

        /* return myself unchanged (although my contents may be changed) */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        /* adjust my expression */
        expr_ = expr_->adjust_for_dyn(info);

        /* return myself otherwise unchanged */
        return this;
    }

protected:
    /* the underlying expression */
    CTcPrsNode *expr_;

    /* the next element of the list */
    class CTPNListEle *next_;

    /* previous element of the list */
    class CTPNListEle *prev_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Member with no arguments 
 */
class CTPNMemberBase: public CTcPrsNode
{
public:
    CTPNMemberBase(CTcPrsNode *obj_expr, CTcPrsNode *prop_expr,
                   int prop_is_expr)
    {
        /* remember the subexpressions */
        obj_expr_ = obj_expr;
        prop_expr_ = prop_expr;

        /* remember whether or not the property is an expression */
        prop_is_expr_ = prop_is_expr;
    }
        
    /* a member with no arguments can always be used as an lvalue */
    virtual int check_lvalue() const { return TRUE; }
    virtual int check_lvalue_resolved(class CTcPrsSymtab *) const
        { return TRUE; }

    /* get the subexpressions */
    CTcPrsNode *get_obj_expr() const { return obj_expr_; }
    CTcPrsNode *get_prop_expr() const { return prop_expr_; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        /* fold constants in the object and property expressions */
        obj_expr_ = obj_expr_->fold_constants(symtab);
        prop_expr_ = prop_expr_->fold_constants(symtab);

        /* return myself with no further evaluation */
        return this;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);

protected:
    /* object expression (left of '.') */
    CTcPrsNode *obj_expr_;

    /* property expression (right of '.') */
    CTcPrsNode *prop_expr_;

    /* flag: the property is an expression, not a simple property name */
    unsigned int prop_is_expr_ : 1;
};

/*
 *   The target must also define another class, CTPNSpecDebugMember, a
 *   subclass of CTPNMember.  This new class must ensure at run-time that
 *   the property evaluation has no side effects. 
 */
/* class CTPNSpecDebugMember: public CTPNMember ... */

/*
 *   Member with arguments - this is for a property evaluation with an
 *   argument list.  (Simple property evaluation without arguments is a
 *   normal binary node subclass.)  
 */
class CTPNMemArgBase: public CTcPrsNode
{
public:
    CTPNMemArgBase(CTcPrsNode *obj_expr, CTcPrsNode *prop_expr,
                   int prop_is_expr, class CTPNArglist *arglist)
    {
        /* save the subexpressions */
        obj_expr_ = obj_expr;
        prop_expr_ = prop_expr;
        arglist_ = arglist;

        /* remember whether or not the property is an expression */
        prop_is_expr_ = prop_is_expr;
    }

    /* get the subexpressions */
    CTcPrsNode *get_obj_expr() const { return obj_expr_; }
    CTcPrsNode *get_prop_expr() const { return prop_expr_; }
    class CTPNArglist *get_arg_list() const { return arglist_; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);

protected:
    /* object expression (left of '.') */
    CTcPrsNode *obj_expr_;

    /* property expression (right of '.') */
    CTcPrsNode *prop_expr_;

    /* argument list */
    class CTPNArglist *arglist_;

    /* flag: the property is an expression, not a simple property name */
    unsigned int prop_is_expr_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Subscript 
 */
class CTPNSubscriptBase: public CTPNBin
{
public:
    CTPNSubscriptBase(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNBin(lhs, rhs) { }

    /* a subscripted array element can be assigned into */
    virtual int check_lvalue() const { return TRUE; }
    virtual int check_lvalue_resolved(class CTcPrsSymtab *) const
        { return TRUE; }

    /* fold constants */
    class CTcPrsNode *fold_binop();
};


/* ------------------------------------------------------------------------ */
/*
 *   Comma operator
 */
class CTPNCommaBase: public CTPNBin
{
public:
    CTPNCommaBase(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNBin(lhs, rhs) { }

    /* fold constants */
    class CTcPrsNode *fold_binop();

    /* 
     *   a comma expression's value is the value of the right operand, so the
     *   type is boolean if the right operand's type is boolean 
     */
    virtual int is_bool() const { return right_->is_bool(); }

    /* 
     *   determine if I have a value - I have a value if my right
     *   subexpression has a value 
     */
    virtual int has_return_value() const
        { return right_->has_return_value(); }

    /* this is part of a dstring expression if either side is */
    virtual int is_dstring_expr() const
        { return left_->is_dstring_expr() || right_->is_dstring_expr(); }
};

/* ------------------------------------------------------------------------ */
/*
 *   Add
 */
class CTPNAddBase: public CTPNBin
{
public:
    CTPNAddBase(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNBin(lhs, rhs) { }

    /* fold constants */
    class CTcPrsNode *fold_binop();
};

/* ------------------------------------------------------------------------ */
/*
 *   Subtract
 */
class CTPNSubBase: public CTPNBin
{
public:
    CTPNSubBase(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNBin(lhs, rhs) { }

    /* fold constants */
    class CTcPrsNode *fold_binop();
};

/* ------------------------------------------------------------------------ */
/*
 *   Equality Comparison
 */
class CTPNEqBase: public CTPNBin
{
public:
    CTPNEqBase(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNBin(lhs, rhs) { }

    /* fold constants */
    class CTcPrsNode *fold_binop();

    /* comparison operators yield boolean results */
    virtual int is_bool() const { return TRUE; }
};

/* ------------------------------------------------------------------------ */
/*
 *   Inequality Comparison
 */
class CTPNNeBase: public CTPNBin
{
public:
    CTPNNeBase(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNBin(lhs, rhs) { }

    /* fold constants */
    class CTcPrsNode *fold_binop();

    /* comparison operators yield boolean results */
    virtual int is_bool() const { return TRUE; }
};

/* ------------------------------------------------------------------------ */
/*
 *   'is in' 
 */
class CTPNIsInBase: public CTPNBin
{
public:
    CTPNIsInBase(CTcPrsNode *lhs, class CTPNArglist *rhs);

    /* fold constants */
    class CTcPrsNode *fold_binop();

    /* comparison operators yield boolean results */
    virtual int is_bool() const { return TRUE; }

protected:
    /* 
     *   flag: if true, we have a constant true value (because the left
     *   side is a constant, and the right side list contains the same
     *   constant) 
     */
    unsigned int const_true_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   'not in' 
 */
class CTPNNotInBase: public CTPNBin
{
public:
    CTPNNotInBase(CTcPrsNode *lhs, class CTPNArglist *rhs);

    /* fold constants */
    class CTcPrsNode *fold_binop();

    /* comparison operators yield boolean results */
    virtual int is_bool() const { return TRUE; }

    /* 
     *   flag: if true, we have a constant true value (because the left
     *   side is a constant, and the right side list is entirely constants
     *   and doesn't contain the left side value) 
     */
    unsigned int const_false_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Logical AND operator node 
 */
class CTPNAndBase: public CTPNBin
{
public:
    CTPNAndBase(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNBin(lhs, rhs) { }

    /* a logical AND always yields a boolean */
    virtual int is_bool() const { return TRUE; }

    /* fold constants */
    class CTcPrsNode *fold_binop();
};

/* ------------------------------------------------------------------------ */
/*
 *   Logical OR operator node 
 */
class CTPNOrBase: public CTPNBin
{
public:
    CTPNOrBase(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNBin(lhs, rhs) { }

    /* a logical OR always yields a boolean value */
    virtual int is_bool() const { return TRUE; }

    /* fold constants */
    class CTcPrsNode *fold_binop();
};

/* ------------------------------------------------------------------------ */
/*
 *   Simple assignment operator 
 */
class CTPNAsiBase: public CTPNBin
{
public:
    CTPNAsiBase(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNBin(lhs, rhs) { }

    /* we're a simple assignment operator */
    virtual int is_simple_asi() const { return TRUE; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        /* if it's speculative, don't allow it, because of side effects */
        if (info->speculative)
            err_throw(VMERR_BAD_SPEC_EVAL);

        /* inherit default processing */
        return CTPNBin::adjust_for_dyn(info);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   NOT operator node 
 */
class CTPNNotBase: public CTPNUnary
{
public:
    CTPNNotBase(CTcPrsNode *sub) : CTPNUnary(sub) { }

    /* NOT always yields a boolean value */
    virtual int is_bool() const { return TRUE; }

    /* fold constants */
    class CTcPrsNode *fold_unop();
};

/* ------------------------------------------------------------------------ */
/*
 *   If-nil node - operator ??
 */
class CTPNIfnilBase: public CTcPrsNode
{
public:
    CTPNIfnilBase(class CTcPrsNode *first, class CTcPrsNode *second)
    {
        first_ = first;
        second_ = second;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* I have a return value if one or the other of my subnodes has a value */
    virtual int has_return_value() const
        { return first_->has_return_value() || second_->has_return_value(); }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        /* adjust my subexpressions */
        first_ = first_->adjust_for_dyn(info);
        second_ = second_->adjust_for_dyn(info);
        return this;
    }

protected:
    class CTcPrsNode *first_;
    class CTcPrsNode *second_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Conditional operator parse node 
 */
class CTPNIfBase: public CTcPrsNode
{
public:
    CTPNIfBase(class CTcPrsNode *first, class CTcPrsNode *second,
               class CTcPrsNode *third)
    {
        first_ = first;
        second_ = second;
        third_ = third;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* add the branches */
    void set_cond(CTcPrsNode *e) { first_ = e; }
    void set_then(CTcPrsNode *e) { second_ = e; }
    void set_else(CTcPrsNode *e) { third_ = e; }

    /* I have a return value if one or the other of my subnodes has a value */
    virtual int has_return_value() const
        { return second_->has_return_value() || third_->has_return_value(); }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        /* adjust my subexpressions */
        first_ = first_->adjust_for_dyn(info);
        second_ = second_->adjust_for_dyn(info);
        third_ = third_->adjust_for_dyn(info);

        /* return myself otherwise unchanged */
        return this;
    }
    
protected:
    class CTcPrsNode *first_;
    class CTcPrsNode *second_;
    class CTcPrsNode *third_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Derived target-independent symbol classes 
 */

/*
 *   Undefined symbol.  During symbol resolution (after all parsing is
 *   completed), when we encounter a symbol that isn't defined, we can use
 *   this symbol type to provide a placeholder.  Creating an unknown
 *   symbol is an error, so the program cannot be successfully compiled
 *   once an undefined symbol is encountered, but this at least allows
 *   compilation to proceed through the rest of the program to check for
 *   other errors. 
 */
class CTcSymUndefBase: public CTcSymbol
{
public:
    CTcSymUndefBase(const char *str, size_t len, int copy)
        : CTcSymbol(str, len, copy, TC_SYM_UNKNOWN) { }

    /* do not write this to a symbol file */
    virtual int write_to_sym_file(class CVmFile *) { return FALSE; }

    /* do not write this to an object file */
    virtual int write_to_obj_file(class CVmFile *) { return FALSE; }

    /* this can't be an lvalue */
    virtual int check_lvalue() const { return FALSE; }
};

/*
 *   Function.  At construction time, a function's actual code offset is
 *   never known; we cannot know a function's code offset until after
 *   we've finished code generation.  
 */
class CTcSymFuncBase: public CTcSymbol
{
public:
    CTcSymFuncBase(const char *str, size_t len, int copy,
                   int argc, int opt_argc, int varargs, int has_retval,
                   int is_multimethod, int is_multimethod_base,
                   int is_extern, int has_proto)
        : CTcSymbol(str, len, copy, TC_SYM_FUNC)
    {
        /* remember the interface information */
        argc_ = argc;
        opt_argc_ = opt_argc;
        varargs_ = varargs;
        has_retval_ = has_retval;
        is_multimethod_ = is_multimethod;
        is_multimethod_base_ = is_multimethod_base;

        /* note whether it's external */
        is_extern_ = is_extern;

        /* note whether it has a prototype */
        has_proto_ = has_proto;

        /* no code stream anchor yet */
        anchor_ = 0;

        /* no fixups yet */
        fixups_ = 0;

        /* this isn't replacing an external function */
        ext_replace_ = FALSE;

        /* we don't know our code body yet */
        code_body_ = 0;

        /* there's no base modified symbol */
        mod_base_ = 0;

        /* we haven't been modified yet */
        mod_global_ = 0;

        /* assume it's not defined in this file */
        mm_def_ = FALSE;
    }

    /* determine if I'm an unresolved external */
    virtual int is_unresolved_extern() const { return is_extern_; }

    /* this can't be an lvalue */
    virtual int check_lvalue() const { return FALSE; }

    /* 
     *   Get the argument count.  If is_varargs() returns true, this is a
     *   minimum argument count; otherwise, this is the exact number of
     *   arguments required. 
     */
    int get_argc() const { return argc_; }
    int get_opt_argc() const { return opt_argc_; }
    int is_varargs() const { return varargs_; }

    /* is the argument list within the correct range? */
    int argc_ok(int n) const
    {
        /* 
         *   the argument list is correct if it has at least the fixed
         *   arguments, AND either we have varargs (in which case there's no
         *   upper limit) or the argument count is less than the maximum
         *   (which is the fixed plus optionals) 
         */
        return (n >= argc_
                && (varargs_ || n <= argc_ + opt_argc_));
    }

    /* build a descriptive message about the argument list, for errors */
    const char *get_argc_desc(char *buf)
    {
        if (varargs_)
            sprintf(buf, "%d+", argc_);
        else if (opt_argc_ != 0)
            sprintf(buf, "%d-%d", argc_, argc_ + opt_argc_);
        else
            sprintf(buf, "%d", argc_);

        return buf;
    }

    /* get/set the 'extern' flag */
    int is_extern() const { return is_extern_; }
    void set_extern(int f) { is_extern_ = f; }

    /* get/set the 'has prototype' flag */
    int has_proto() const { return has_proto_; }
    void set_has_proto(int f) { has_proto_ = f; }

    /* get/set the multi-method flag */
    int is_multimethod() const { return is_multimethod_; }
    void set_multimethod(int f) { is_multimethod_ = f; }

    /* get/set the multi-method base function flag */
    int is_multimethod_base() const { return is_multimethod_base_; }
    void set_multimethod_base(int f) { is_multimethod_base_ = f; }

    /* is this multi-method defined in this file? */
    int is_mm_def() const { return mm_def_; }
    void set_mm_def(int f) { mm_def_ = f; }

    /* get/set the external 'replace' flag */
    int get_ext_replace() const { return ext_replace_; }
    void set_ext_replace(int f) { ext_replace_ = f; }

    /* get/set the modified base symbol */
    class CTcSymFunc *get_mod_base() const { return mod_base_; }
    void set_mod_base(class CTcSymFunc *f) { mod_base_ = f; }

    /* 
     *   Get/set the global symbol.  For a function replaced with 'modify',
     *   this is the original symbol for the function, which is still in the
     *   global symbol table.  Otherwise, this is simply null.  
     */
    class CTcSymFunc *get_mod_global() const { return mod_global_; }
    void set_mod_global(class CTcSymFunc *f) { mod_global_ = f; }

    /* get/set my code body */
    class CTPNCodeBody *get_code_body() const { return code_body_; }
    void set_code_body(class CTPNCodeBody *cb) { code_body_ = cb; }

    /* determine if the function has a return value */
    int has_retval() const { return has_retval_; }

    /* the name of a function used alone yields its address */
    class CTcPrsNode *fold_constant();

    /* write some additional data to the symbol file */
    virtual int write_to_sym_file(class CVmFile *fp);

    /* write some additional data to the object file */
    virtual int write_to_obj_file(class CVmFile *fp);

    /* read from a symbol file */
    static class CTcSymbol *read_from_sym_file(class CVmFile *fp);

    /* load from an object file */
    static int load_from_obj_file(class CVmFile *fp, const textchar_t *fname);
    
    /* get my anchor */
    struct CTcStreamAnchor *get_anchor() const { return anchor_; }

    /*
     *   Set my anchor.  This must be invoked during code generation when
     *   our code body is generated.  
     */
    virtual void set_anchor(struct CTcStreamAnchor *anchor)
        { anchor_ = anchor; }

    /* get a pointer to my fixup list anchor */
    virtual struct CTcAbsFixup **get_fixup_list_anchor() { return &fixups_; }

    /* add an absolute fixup for the symbol at the given stream offset */
    void add_abs_fixup(class CTcDataStream *ds, ulong ofs);

    /* add an absolute fixup for the symbol at the current stream offset */
    void add_abs_fixup(class CTcDataStream *ds);

    /* determine if I have a return value when called as a function */
    virtual int has_return_value_on_call() const
        { return has_retval_; }

    /* get the number of allocated modified base function offsets */
    int get_mod_base_offset_count() const
        { return mod_base_ofs_list_.get_count(); }

    /* get a modified base function offset by index */
    ulong get_mod_base_offset(int idx)
        { return (ulong)mod_base_ofs_list_.get_ele_long(idx); }

    /* add a modified base function offset */
    void add_mod_base_offset(ulong ofs)
        { mod_base_ofs_list_.add_ele((long)ofs); }

    /* clear the modified base offset list */
    void clear_mod_base_offsets() { mod_base_ofs_list_.clear(); }

    /* 
     *   Set the absolute address.  For debugger expressions, this sets the
     *   code pool address specified in the debugger source expression. 
     */
    virtual void set_abs_addr(uint32_t addr) = 0;

protected:
    /* head of the fixup list for our function's code */
    struct CTcAbsFixup *fixups_;

    /* 
     *   my stream anchor - this records my byte stream data size, code
     *   stream offset, and (after linking) final pool address 
     */
    struct CTcStreamAnchor *anchor_;

    /* the code body of the function */
    class CTPNCodeBody *code_body_;

    /* argument count */
    int argc_;

    /* number of additional optional arguments */
    int opt_argc_;

    /* the original symbol we modified, if any */
    class CTcSymFunc *mod_base_;

    /* global symbol, for a modified function */
    class CTcSymFunc *mod_global_;

    /* array of modified base function code stream offsets */
    CPrsArrayList mod_base_ofs_list_;

    /* flag: variable arguments (in which case argc_ is only a minimum) */
    unsigned int varargs_ : 1;

    /* flag: the function has a return value (false -> void function) */
    unsigned int has_retval_ : 1;

    /* flag: this function is external to us */
    unsigned int is_extern_ : 1;

    /* flag: this function replaces an external function */
    unsigned int ext_replace_ : 1;

    /* flag: this is a multi-method */
    unsigned int is_multimethod_ : 1;

    /* flag: this is a multi-method base function */
    unsigned int is_multimethod_base_ : 1;

    /* flag: this multi-method is defined in this file */
    unsigned int mm_def_ : 1;

    /* 
     *   Flag: this function has a known prototype.  An 'extern' function
     *   declaration is allowed without a prototype, in which case the
     *   function can be defined elsewhere with any prototype. 
     */
    unsigned int has_proto_ : 1;
};

/*
 *   metaclass symbol - property list entry 
 */
class CTcSymMetaProp
{
public:
    CTcSymMetaProp(class CTcSymProp *prop, int is_static)
    {
        /* remember the property symbol */
        prop_ = prop;

        /* remember whether it's static */
        is_static_ = is_static;

        /* not in a list yet */
        nxt_ = 0;
    }
    
    /* property symbol for this entry */
    class CTcSymProp *prop_;

    /* next entry in the list */
    CTcSymMetaProp *nxt_;

    /* flag: the property is declared as 'static' */
    unsigned int is_static_ : 1;
};

/*
 *   Metaclass.
 *   
 *   Metaclasses must be declared per module.  However, we store "soft"
 *   references in symbol files, purely so that the defined() operator knows
 *   whether a metaclass is included in the build.  The soft references don't
 *   contain any property table information, so we still need a full
 *   'intrinsic class' definition in each module.
 */
class CTcSymMetaclassBase: public CTcSymbol
{
public:
    CTcSymMetaclassBase(const char *str, size_t len, int copy,
                        int meta_idx, tctarg_obj_id_t class_obj)
        : CTcSymbol(str, len, copy, TC_SYM_METACLASS)
    {
        /* remember the metaclass dependency list index */
        meta_idx_ = meta_idx;

        /* we have no properties in our list yet */
        prop_head_ = 0;
        prop_tail_ = 0;
        prop_cnt_ = 0;

        /* remember my class object */
        class_obj_ = class_obj;

        /* we have no modification object yet */
        mod_obj_ = 0;

        /* we don't have a superclass yet */
        super_meta_ = 0;

        /* assume it's not an external symbol */
        ext_ = FALSE;

        /* not yet referenced */
        ref_ = FALSE;
    }

    /* this can't be an lvalue */
    virtual int check_lvalue() const { return FALSE; }

    /* add a property to my list */
    void add_prop(const char *txt, size_t len, const char *obj_fname,
                  int is_static);
    void add_prop(class CTcSymProp *prop, int is_static);

    /* get the head of the property symbol list */
    CTcSymMetaProp *get_prop_head() const { return prop_head_; }

    /* get the nth property ID from the metaclass method table */
    CTcSymMetaProp *get_nth_prop(int n) const;

    /* get/set the metaclass dependency table index */
    int get_meta_idx() const { return meta_idx_; }
    void set_meta_idx(int idx) { meta_idx_ = idx; }

    /* write to a symbol file */
    virtual int write_to_sym_file(class CVmFile *fp);

    /* read from a symbol file */
    static class CTcSymbol *read_from_sym_file(class CVmFile *fp);

    /* write some additional data to the object file */
    virtual int write_to_obj_file(class CVmFile *fp);

    /* load from an object file */
    static int load_from_obj_file(class CVmFile *fp,
                                  const textchar_t *fname,
                                  tctarg_obj_id_t *obj_xlat);

    /* get my class object */
    tctarg_obj_id_t get_class_obj() const { return class_obj_; }

    /* get/set my modification object */
    class CTcSymObj *get_mod_obj() const { return mod_obj_; }
    void set_mod_obj(CTcSymObj *obj) { mod_obj_ = obj; }

    /* get/set my intrinsic superclass */
    class CTcSymMetaclass *get_super_meta() const { return super_meta_; }
    void set_super_meta(class CTcSymMetaclass *sc) { super_meta_ = sc; }

    /*
     *   Get/set the external reference status.  An external metaclass is one
     *   that's defined in another module; we have visibility to it via a
     *   reference in a symbol file.  This definition isn't sufficient to
     *   actually use the metaclass; the metaclass must be declared per
     *   module via an explicitly included 'intrinsic class' statment.  The
     *   external reference is sufficient for the defined() operator, though,
     *   so we can test whether a metaclass is part of the build within a
     *   module that doesn't declare the metaclass.
     */
    int is_ext() const { return ext_; }
    void set_ext(int f) { ext_ = f; }

protected:
    /* our intrinsic superclass */
    class CTcSymMetaclass *super_meta_;

    /* metaclass dependency table index */
    int meta_idx_;
    
    /* list of properties */
    CTcSymMetaProp *prop_head_;
    CTcSymMetaProp *prop_tail_;

    /* 
     *   modification object - this is the object that contains the user
     *   methods added to the intrinsic class via 'modify' 
     */
    class CTcSymObj *mod_obj_;

    /* number of properties in our list */
    int prop_cnt_;

    /* the object that represents the class */
    tctarg_obj_id_t class_obj_;

    /* is this an external metaclass symbol? */
    uint ext_ : 1;

    /* has our object been referenced in code generation? */
    uint ref_ : 1;
};

/*
 *   Object.  An object's ID number can be assigned as soon as the object
 *   is defined during parsing.  
 */
class CTcSymObjBase: public CTcSymbol
{
public:
    CTcSymObjBase(const char *str, size_t len, int copy, ulong obj_id,
                  int is_extern, tc_metaclass_t meta,
                  class CTcDictEntry *dict)
        : CTcSymbol(str, len, copy, TC_SYM_OBJ)
    {
        /* store the object ID */
        obj_id_ = obj_id;

        /* note whether it's external */
        is_extern_ = is_extern;

        /* we're not yet referenced */
        ref_ = FALSE;

        /* we don't have a stream offset yet */
        stream_ofs_ = 0xffffffff;

        /* we're don't 'modify' or 'replace' an external object */
        ext_modify_ = ext_replace_ = FALSE;

        /* I haven't been modified yet */
        modified_ = FALSE;

        /* presume we won't modify another object */
        mod_base_sym_ = 0;

        /* presume we won't be modified by another object */
        modifying_obj_.sym_ = 0;

        /* no parse tree yet */
        obj_stm_ = 0;

        /* no self-reference fixups yet */
        fixups_ = 0;

        /* no deleted properties yet */
        first_del_prop_ = 0;

        /* set the metaclass */
        metaclass_ = meta;

        /* set the dictionary */
        dict_ = dict;

        /* not yet written to object file */
        written_to_obj_ = FALSE;
        counted_in_obj_ = FALSE;

        /* no object file index yet */
        obj_file_idx_ = 0;

        /* no vocabulary yet */
        vocab_ = 0;

        /* no superclasses in our list yet */
        sc_ = 0;

        /* no superclass names in our list yet */
        sc_name_head_ = 0;
        sc_name_tail_ = 0;

        /* presume we're not a class */
        is_class_ = FALSE;

        /* we haven't built our inherited vocabulary list yet */
        vocab_inherited_ = FALSE;

        /* presume we're fully nonymous */
        anon_ = FALSE;

        /* assume we will have no metaclass-specific extra data object */
        meta_extra_ = 0;

        /* we're not yet explicitly based on the root object class */
        sc_is_root_ = FALSE;

        /* no templates for this class yet */
        template_head_ = template_tail_ = 0;

        /* we don't have a private grammar rule list yet */
        grammar_entry_ = 0;

        /* presume it's not transient */
        transient_ = FALSE;
    }

    /* this can't be an lvalue */
    virtual int check_lvalue() const { return FALSE; }

    /* get/set my metaclass */
    tc_metaclass_t get_metaclass() const { return metaclass_; }
    void set_metaclass(tc_metaclass_t meta) { metaclass_ = meta; }

    /* determine if I'm an unresolved external */
    virtual int is_unresolved_extern() const { return is_extern_; }

    /* get/set the object ID */
    ulong get_obj_id() const { return obj_id_; }
    void set_obj_id(ulong obj_id) { obj_id_ = obj_id; }

    /* 
     *   Get/set the 'extern' flag.  If an object symbol is defined as
     *   extern, it means that the object is referenced but not defined in
     *   the current file.  This is used for separate compilation; extern
     *   objects must be resolved during linking. 
     */
    int is_extern() const { return is_extern_; }
    void set_extern(int f) { is_extern_ = f; }

    /* get the appropriate object data stream for this object */
    class CTcDataStream *get_stream()
        { return get_stream_from_meta(metaclass_); }

    /* get/set my object stream offset */
    ulong get_stream_ofs() const { return stream_ofs_; }
    void set_stream_ofs(ulong ofs) { stream_ofs_ = ofs; }

    /* get/set my object statement's parse tree */
    class CTPNStmObject *get_obj_stm() const { return obj_stm_; }
    void set_obj_stm(class CTPNStmObject *obj_stm) { obj_stm_ = obj_stm; }

    /* get/set the external 'modify' flag */
    int get_ext_modify() const { return ext_modify_; }
    void set_ext_modify(int f) { ext_modify_ = f; }

    /* get/set the external 'replace' flag */
    int get_ext_replace() const { return ext_replace_; }
    void set_ext_replace(int f) { ext_replace_ = f; }

    /*
     *   Get/Set the 'modify' base object.  For a 'modify' object, this sets
     *   the symbol (which is always a synthesized symbol) of the object
     *   being modified. 
     */
    class CTcSymObj *get_mod_base_sym() const { return mod_base_sym_; }
    void set_mod_base_sym(class CTcSymObj *sym);

    /* 
     *   Get/set the modifiying object - this is the 'modify' object
     *   that's modifying us.  When we set this value, we'll also tell the
     *   modified object that we are its modifier.  
     */
    class CTcSymObj *get_modifying_sym() const
        { return modifying_obj_.sym_; }
    void set_modifying_sym(CTcSymObj *sym)
        { modifying_obj_.sym_ = sym; }

    /* 
     *   Get/set the modifying object ID - the ID can be used instead of
     *   the symbol, if desired.  Note that only one or the other may be
     *   stored, since they share the same data space. 
     */
    tctarg_obj_id_t get_modifying_obj_id() const
        { return modifying_obj_.id_; }
    void set_modifying_obj_id(tctarg_obj_id_t id)
        { modifying_obj_.id_ = id; }

    /* 
     *   get/set the 'modified' flag - this flag indicates that this is a
     *   placeholder symbol for a modified object 
     */
    int get_modified() const { return modified_; }
    void set_modified(int f) { modified_ = f; }

    /*
     *   Synthesize a placeholder symbol to use for modified objects.
     *   This symbol keeps track of the original object after
     *   modification. 
     */
    static class CTcSymObj *synthesize_modified_obj_sym(int anon);

    /* 
     *   synthesize the name of a modified object base class symbol given
     *   the base class's object ID 
     */
    static void synthesize_modified_obj_name(char *buf,
                                             tctarg_obj_id_t obj_id);

    /* add a self-reference fixup */
    void add_self_ref_fixup(class CTcDataStream *stream, ulong ofs);

    /* an object's value is a compile-time constant */
    class CTcPrsNode *fold_constant();

    /* 
     *   we can't take the address of an object - an object symbol yields a
     *   reference to begin with, so taking its address is meaningless 
     */
    virtual int has_addr() const { return FALSE; }

    /* write some additional data to the symbol file */
    virtual int write_to_sym_file(class CVmFile *fp);

    /* write some additional data to the object file */
    virtual int write_to_obj_file(class CVmFile *fp);

    /* write myself as a modified base object to an object file */
    int write_to_obj_file_as_modified(class CVmFile *fp);

    /* write references to the object file */
    virtual int write_refs_to_obj_file(class CVmFile *fp);

    /* load references from the object file */
    virtual void load_refs_from_obj_file(class CVmFile *fp,
                                         const textchar_t *fname,
                                         tctarg_obj_id_t *obj_xlat,
                                         tctarg_prop_id_t *prop_xlat);

    /* mark the object as referenced */
    void mark_referenced() { ref_ = TRUE; }

    /* read from a symbol file */
    static class CTcSymbol *read_from_sym_file(class CVmFile *fp);

    /* load from an object file */
    static int load_from_obj_file(class CVmFile *fp, const textchar_t *fname,
                                  tctarg_obj_id_t *obj_xlat, int anon);

    /* load a modified base object from an object file */
    static class CTcSymObj *
        load_from_obj_file_modbase(class CVmFile *fp, const textchar_t *fname,
                                   tctarg_obj_id_t *obj_xlat,
                                   const textchar_t *mod_name,
                                   size_t mod_name_len, int anon);

    /* get/set the head of my fixup list */
    struct CTcIdFixup *get_fixups() const { return fixups_; }
    void set_fixups(struct CTcIdFixup *fixups) { fixups_ = fixups; }

    /* apply our internal fixups */
    virtual void apply_internal_fixups();

    /* add a deleted property entry */
    void add_del_prop_entry(class CTcSymProp *prop)
        { add_del_prop_to_list(&first_del_prop_, prop); }

    /* get the head of my deleted property list */
    class CTcObjPropDel *get_first_del_prop() const
        { return first_del_prop_; }

    /* set the head of my deleted property list, replacing any old list */
    void set_del_prop_head(class CTcObjPropDel *list_head)
        { first_del_prop_ = list_head; }

    /*
     *   Delete a property from 'modify' base objects.  This must be
     *   defined by the target-specific code subclass, because the action
     *   of this routine is specific to the code generator.
     *   
     *   This carries out a link-time 'replace' on a property in a
     *   'modify' object.  This object is the 'modify' object; we must
     *   delete the property in all of the base classes of this object
     *   that were modified by this 'modify' or by a 'modify' to which
     *   this 'modify' indirectly applies.  Do not delete the property
     *   from this object - only from its modified base classes.  Stop
     *   upon reaching a non-modified base class, because this is the base
     *   class of the original.  
     */
    virtual void delete_prop_from_mod_base(tctarg_prop_id_t prop_id) = 0;

    /*
     *   Mark the compiled data for this object as a class object.  This
     *   must be defined by the target-specific code subclass, because we
     *   need to modify the generated code for the object.
     *   
     *   This routine is called when an object loaded from an object file
     *   is modified by an object in another object file, and hence
     *   becomes a base class at link-time.  We call this on the original,
     *   which doesn't know it's being modified until link time.  
     */
    virtual void mark_compiled_as_class() = 0;

    /* get/set the symbol's containing dictionary */
    class CTcDictEntry *get_dict() const { return dict_; }
    void set_dict(class CTcDictEntry *dict) { dict_ = dict; }

    /* get/set my object file index */
    uint get_obj_file_idx() const { return obj_file_idx_; }
    void set_obj_file_idx(uint idx) { obj_file_idx_ = idx; }

    /* add a word to my vocabulary list */
    void add_vocab_word(const char *txt, size_t len, tctarg_prop_id_t prop);

    /* delete a vocabulary property from my list (for 'replace') */
    void delete_vocab_prop(tctarg_prop_id_t prop);

    /* get/set the vocabulary word list head */
    class CTcVocabEntry *get_vocab_head() const { return vocab_; }
    void set_vocab_head(class CTcVocabEntry *entry) { vocab_ = entry; }

    /* build my dictionary */
    virtual void build_dictionary();

    /* inherit my vocabulary from superclasses (used during linking) */
    void inherit_vocab();

    /* add my vocabulary words to a subclass's vocabulary (for linking) */
    void add_vocab_to_subclass(class CTcSymObj *sub);

    /* get/set my class flag (used and valid only during linking) */
    int is_class() const { return is_class_ != 0; }
    void set_is_class(int is_class) { is_class_ = (is_class != 0); }

    /* get/set the resolved superclass list head (link-time only) */
    class CTcObjScEntry *get_sc_head() const { return sc_; }
    void set_sc_head(class CTcObjScEntry *sc) { sc_ = sc; }

    /* get the named superclass list head (compile-time only) */
    class CTPNSuperclass *get_sc_name_head() const { return sc_name_head_; }

    /* clear the named superclass list */
    void clear_sc_names() { sc_name_head_ = sc_name_tail_ = 0; }

    /* copy the name list from another object symbol */
    void copy_sc_names(CTcSymObjBase *other_obj)
    {
        /* point our entries directly to the other symbol's */
        sc_name_head_ = other_obj->sc_name_head_;
        sc_name_tail_ = other_obj->sc_name_tail_;
    }

    /* 
     *   Add a named superclass.
     *   
     *   We keep a superclass list here, separately from the superclass list
     *   that the associated object statement itself maintains, because we
     *   want hierarchy information for external objects as well as objects
     *   defined in the current module.  An external object has only a
     *   symbol table entry, so there's no object statement in which to
     *   stash the object list for such objects.
     *   
     *   In addition, this symbolic superclass list isn't exactly the same
     *   as the true superclass list, because the symbolic list doesn't
     *   include the special unnamed objects created when 'modify' is used.
     *   The list we keep here mirrors the apparent structure as represented
     *   in the source code.  
     */
    void add_sc_name_entry(const char *txt, size_t len);

    /* 
     *   determine if I have the given superclass - returns true if the
     *   given superclass is among my direct superclasses, or if any of my
     *   direct superclasses has the given superclass 
     */
    int has_superclass(class CTcSymObj *sc_sym) const;

    /* get/set the anonymous flag */
    int is_anon() const { return anon_ != 0; }
    void set_anon(int f) { anon_ = (f != 0); }

    /* get/set the metaclass extra data for this object */
    void *get_meta_extra() const { return meta_extra_; }
    void set_meta_extra(void *ptr) { meta_extra_ = ptr; }

    /* 
     *   get/set the root-object-superclass flag - if this is set, it means
     *   that the object was explicitly defined with 'object' (the root
     *   object type) as its superclass 
     */
    int sc_is_root() const { return sc_is_root_; }
    void set_sc_is_root(int f) { sc_is_root_ = (f != 0); }

    /* add a class-specific template */
    void add_template(class CTcObjTemplate *tpl);

    /* get the head/tail of my template list */
    class CTcObjTemplate *get_first_template() const
        { return template_head_; }
    class CTcObjTemplate *get_last_template() const
        { return template_tail_; }

    /*
     *   Create our grammar rule entry object.  If we already have one,
     *   we'll discard it in favor of the new one; this allows a 'replace'
     *   or 'modify' to override any previously-defined grammar rules.  
     */
    class CTcGramProdEntry *create_grammar_entry(
        const char *prod_sym, size_t prod_sym_len);

    /* get my grammar rule list */
    class CTcGramProdEntry *get_grammar_entry() const
        { return grammar_entry_; }

    /* 
     *   merge my grammar rule list (if any) into the global rule list for
     *   the associated production object 
     */
    void merge_grammar_entry();

    /* get/set transient status */
    int is_transient() const { return transient_; }
    void set_transient() { transient_ = TRUE; }

protected:
    /* add a deleted property entry to a list */
    static void add_del_prop_to_list(class CTcObjPropDel **list_head,
                                     class CTcSymProp *prop);

    /* main object file writer routine */
    int write_to_obj_file_main(CVmFile *fp);

    /* main object file loader routine */
    static class CTcSymObj *
        load_from_obj_file_main(class CVmFile *fp, const textchar_t *fname,
                                tctarg_obj_id_t *obj_xlat,
                                const textchar_t *mod_name,
                                size_t mod_name_len, int anon);

    /* get the stream for a given metaclass */
    static class CTcDataStream *get_stream_from_meta(tc_metaclass_t meta);

    /* the object's ID */
    ulong obj_id_;

    /* 
     *   the object stream (G_os, or other stream as appropriate) offset of
     *   our data - this is valid after code generation if we're not
     *   external 
     */
    ulong stream_ofs_;

    /* the parse tree for our object definition */
    class CTPNStmObject *obj_stm_;

    /* head of our self-reference object ID fixup list */
    struct CTcIdFixup *fixups_;

    /* 
     *   Head of list of deleted properties.  This list is used only in
     *   the top object in a stack of modified objects (i.e., this list is
     *   attached to the latest 'modify' object), and only when the bottom
     *   object in the stack is external to the current translation unit.
     *   This is the list of objects that must be deleted at link time,
     *   because they're external to the current translation unit.  
     */
    class CTcObjPropDel *first_del_prop_;

    /*
     *   Base symbol for a 'modify' object.  This is the synthesized
     *   symbol for the object that this object modifies. 
     */
    class CTcSymObj *mod_base_sym_;

    /* 
     *   Modifying symbol for an object modified by a 'modify' object.
     *   This points to the object that is modifying us - it's simply the
     *   reverse link of mod_base_sym_.  
     */
    union
    {
        class CTcSymObj *sym_;
        tctarg_obj_id_t id_;
    } modifying_obj_;

    /* the dictionary holding my vocabulary words */
    class CTcDictEntry *dict_;

    /* head of vocabulary list for this object */
    class CTcVocabEntry *vocab_;

    /* 
     *   Head of superclass list - this list is used and valid ONLY during
     *   linking; it is null during compilation.  Because a class or object
     *   can be defined before its superclasses, and can be defined in
     *   separate a separate module from its superclass, we can't resolve
     *   superclasses until link time.  
     */
    class CTcObjScEntry *sc_;

    /*
     *   Superclass name list.  This list is used and valid ONLY during
     *   normal compilation, and keeps track of the symbolic name structure
     *   of the class hiearchy.  We don't resolve these names; we simply
     *   keep track of the names as they appear in the source code.
     *   
     *   Note that the entries in this list must all have valid txt_ entries
     *   - no entries with sym_ values are allowed in this list.  
     */
    class CTPNSuperclass *sc_name_head_;
    class CTPNSuperclass *sc_name_tail_;

    /* pointer to metaclass-specific extra data */
    void *meta_extra_;

    /* the object's metaclass */
    tc_metaclass_t metaclass_;

    /* list of templates associated with this class */
    class CTcObjTemplate *template_head_;
    class CTcObjTemplate *template_tail_;

    /* object file index - used for serializing to the object file */
    uint obj_file_idx_;

    /* grammar rule list, if we have one */
    class CTcGramProdEntry *grammar_entry_;

    /* flag: this object is external to us */
    uint is_extern_ : 1;

    /* flag: the object has been referenced */
    uint ref_ : 1;

    /* 
     *   'modify' and 'replace' flags - these indicate that this object
     *   modifies/replaces an *external* object, hence the modify/replace
     *   action must be performed at link time 
     */
    uint ext_modify_ : 1;
    uint ext_replace_ : 1;

    /* flag: I've been modified by another object */
    uint modified_ : 1;

    /* flag: I'm a 'class' object (used and valid only during linking) */
    uint is_class_ : 1;

    /* flag: this object has been written to the object file */
    uint written_to_obj_ : 1;

    /* flag: this object has been counted as written to the object file */
    uint counted_in_obj_ : 1;

    /* 
     *   flag: we've built our full vocabulary by explicitly inheriting
     *   our superclass's words into our own list 
     */
    uint vocab_inherited_ : 1;

    /* flag: this object is anonymous */
    uint anon_ : 1;

    /* 
     *   flag: we are explicitly defined with the root object ('object') as
     *   our superclass 
     */
    uint sc_is_root_ : 1;

    /* flag: the object is transient */
    uint transient_ : 1;
};

/*
 *   Object vocabulary word entry 
 */
class CTcVocabEntry
{
public:
    CTcVocabEntry(const char *txt, size_t len, tctarg_prop_id_t prop)
    {
        /* 
         *   keep a reference to the text - assume it's in parser memory,
         *   so it'll stay around for the duration of the compilation 
         */
        txt_ = txt;
        len_ = len;

        /* remember; the property */
        prop_ = prop;

        /* not in a list yet */
        nxt_ = 0;
    }

    /* text of word */
    const char *txt_;
    size_t len_;

    /* property of the word association */
    tctarg_prop_id_t prop_;

    /* next entry in list */
    CTcVocabEntry *nxt_;
};

/*
 *   Object superclass list entry 
 */
class CTcObjScEntry
{
public:
    CTcObjScEntry(class CTcSymObj *sym)
    {
        /* remember the object symbol */
        sym_ = sym;

        /* not in a list yet */
        nxt_ = 0;
    }

    /* object symbol */
    class CTcSymObj *sym_;

    /* next superclass list entry */
    CTcObjScEntry *nxt_;
};


/*
 *   Property.  A property's ID can be assigned as soon as the property is
 *   first used in an object definition. 
 */
class CTcSymPropBase: public CTcSymbol
{
public:
    CTcSymPropBase(const char *str, size_t len, int copy,
                   tctarg_prop_id_t prop)
        : CTcSymbol(str, len, copy, TC_SYM_PROP)
    {
        /* remember our property ID */
        prop_ = prop;

        /* we're not referenced yet */
        ref_ = FALSE;

        /* presume it's not a vocabulary property */
        vocab_ = FALSE;

        /* presume it's not a weak property assumption */
        weak_ = FALSE;
    }

    /* get the property ID */
    tctarg_prop_id_t get_prop() const { return prop_; }
    
    /* we can get the address of a property */
    class CTcPrsNode *fold_addr_const();

    /* we can assign to a property */
    virtual int check_lvalue() const { return TRUE; }

    /* we can take the address of a property */
    virtual int has_addr() const { return TRUE; }

    /* mark the property as referenced */
    void mark_referenced() { ref_ = TRUE; }

    /* get/set the vocabulary-property flag */
    int is_vocab() const { return vocab_; }
    void set_vocab(int flag) { vocab_ = (flag != 0); }

    /* get/set the "weak" flag */
    int is_weak() const { return weak_; }
    void set_weak(int flag) { weak_ = (flag != 0); }

    /* read/write symbol file */
    static class CTcSymbol *read_from_sym_file(class CVmFile *fp);
    virtual int write_to_sym_file(class CVmFile *fp);

    /* write to an object file */
    int write_to_obj_file(class CVmFile *fp);

    /* load from an object file */
    static int load_from_obj_file(class CVmFile *fp, const textchar_t *fname,
                                  tctarg_prop_id_t *prop_xlat);

protected:
    /* property ID */
    tctarg_prop_id_t prop_;

    /* flag: the property is referenced */
    unsigned int ref_ : 1;

    /* flag: this is a vocabulary property */
    unsigned int vocab_ : 1;

    /* 
     *   Flag: this is a "weak" property definition.  A weak definition is
     *   one that can be overridden by a conflicting definition for the same
     *   symbol using another type.  Symbols used with "&" are assumed to be
     *   properties, but only if we don't later find a conflicting
     *   definition; the weak flag tells us that we can drop the property
     *   assumption without complaint if we do find a different definition
     *   for the symbol later.  
     */
    unsigned int weak_: 1;
};

/*
 *   Enumerator
 */
class CTcSymEnumBase: public CTcSymbol
{
public:
    CTcSymEnumBase(const char *str, size_t len, int copy, ulong id,
                   int is_token)
        : CTcSymbol(str, len, copy, TC_SYM_ENUM)
    {
        /* remember my ID */
        enum_id_ = id;

        /* remember if it's a 'token' enum */
        is_token_ = is_token;

        /* not yet referenced */
        ref_ = FALSE;
    }

    /* this can't be an lvalue */
    virtual int check_lvalue() const { return FALSE; }

    /* mark the symbol as referenced */
    void mark_referenced() { ref_ = TRUE; }

    /* an object's value is a compile-time constant */
    class CTcPrsNode *fold_constant();

    /* get my enumerator ID */
    ulong get_enum_id() const { return enum_id_; }

    /* get/set the 'token' flag */
    int is_token() const { return is_token_ != 0; }
    void set_is_token(int flag) { is_token_ = (flag != 0); }

    /* write to a symbol file */
    virtual int write_to_sym_file(class CVmFile *fp);

    /* read from a symbol file */
    static class CTcSymbol *read_from_sym_file(class CVmFile *fp);

    /* write to an object file */
    int write_to_obj_file(class CVmFile *fp);

    /* load from an object file */
    static int load_from_obj_file(class CVmFile *fp, const textchar_t *fname,
                                  ulong *enum_xlat);

protected:
    /* my enumerator ID */
    ulong enum_id_;

    /* flag: this is a 'token' enum */
    unsigned int is_token_ : 1;

    /* referenced? */
    unsigned int ref_ : 1;
};


/*
 *   local variable/parameter
 */
class CTcSymLocalBase: public CTcSymbol
{
public:
    CTcSymLocalBase(const char *str, size_t len, int copy,
                    int is_param, int var_num);

    /* get the variable number */
    int get_var_num() const { return var_num_; }

    /* true -> parameter, false -> local variable */
    int is_param() const { return is_param_; }

    /* get/set the parameter position (0 is the leftmost argument) */
    int get_param_index() const { return param_index_; }
    void set_param_index(int n) { param_index_ = n; }

    /* true -> named parameter */
    int is_named_param() const { return is_named_param_; }
    void set_named_param(int f) { is_named_param_ = f; }

    /* true -> optional parameter */
    int is_opt_param() const { return is_opt_param_; }
    void set_opt_param(int f) { is_opt_param_ = f; }

    /* get/set the default value expression */
    CTcPrsNode *get_defval_expr() const { return defval_expr_; }
    int get_defval_seqno() const { return defval_seqno_; }
    void set_defval_expr(CTcPrsNode *expr, int seqno)
    {
        defval_expr_ = expr;
        defval_seqno_ = seqno;
    }

    /* 
     *   get/set "list parameter" flag - a list parameter is still a local,
     *   but from the programmer's perspective acts like a parameter because
     *   its role is to contain a list of varargs parameters beyond a certain
     *   point in the argument list 
     */
    int is_list_param() const { return is_list_param_; }
    void set_list_param(int f) { is_list_param_ = f; }

    /* 
     *   true -> "context local" - A context local is a local variable
     *   contained in a scope enclosing an anonymous function.  Because
     *   the lifetime of an anonymous function can exceed that of the
     *   activation record that was active when the anonymous function
     *   object was created, every local variable in a scope that is
     *   referenced from within an anonymous function defined within that
     *   scope must be stored in a separate context object, rather than in
     *   the activation record.  The context object is created along with
     *   the activation record, but it is not destroyed with the
     *   activation record; instead, a reference to the context object is
     *   stored in the anonymous function, which ensures that the context
     *   object stays reachable as long as the anonymous function object
     *   itself is reachable.  
     */
    int is_ctx_local() const { return (is_ctx_local_ != 0); }

    /*
     *   Establish the variable as a context local, assigning the property
     *   ID of the context object that stores the variable's value, and
     *   the local variable that contains the context object itself.  
     */
    void set_ctx_level(int ctx_level)
    {
        /* it's a context local now */
        is_ctx_local_ = TRUE;

        /* remember the context level */
        ctx_level_ = ctx_level;

        /* we don't know our context array index yet */
        ctx_arr_idx_ = 0;
    }

    /* 
     *   get/set the original local in the enclosing scope for an
     *   anonymous function's inherited local 
     */
    class CTcSymLocal *get_ctx_orig() const { return ctx_orig_; }
    void set_ctx_orig(class CTcSymLocal *orig) { ctx_orig_ = orig; }

    /* get my context array index */
    int get_ctx_arr_idx() const;

    /* we can always assign to a local */
    virtual int check_lvalue() const { return TRUE; }

    /* 
     *   get/set the 'used' flag - this tracks whether or not the
     *   variable's value was referenced anywhere in the scope in which it
     *   was defined 
     */
    int get_val_used() const { return val_used_; }
    void set_val_used(int f);

    /* 
     *   get/set the 'assigned' flag - this tracks whether or not the
     *   variable has been assigned a value 
     */
    int get_val_assigned() const { return val_assigned_; }
    void set_val_assigned(int f);

    /* get the source location where the symbol was defined */
    class CTcTokFileDesc *get_src_desc() const { return src_desc_; }
    long get_src_linenum() const { return src_linenum_; }

    /* get/set reference flag */
    int is_referenced() const { return referenced_ != 0; }
    void mark_referenced() { referenced_ = TRUE; }

    /* check for references in local scope */
    virtual void check_local_references();

    /* create a new context copy */
    virtual class CTcSymbol *new_ctx_var() const;

    /* apply context variable conversion */
    virtual int apply_ctx_var_conv(class CTcPrsSymtab *symtab,
                                   class CTPNCodeBody *code_body);

    /* finish context variable conversion */
    virtual void finish_ctx_var_conv();

    /* convert this variable to a context variable */
    void convert_to_ctx_var(int val_used, int val_assigned);

protected:
    /* source location where the symbol was defined */
    class CTcTokFileDesc *src_desc_;
    long src_linenum_;

    /* variable number - stack frame offset for local or parameter */
    int var_num_;

    /* 
     *   Parameter index.  This is the index within the formal parameter list
     *   of a positional parameter variable.  For regular positional
     *   parameters, this is the same as var_num_.  However, it's different
     *   for *optional* parameters, because optional parameters are actually
     *   stored in local variables, meaning that var_num_ is a *local* number
     *   rather than an argument number.  In order to generate the code that
     *   loads the actual argument value into the local variable, we have to
     *   keep track of both stack locations.  This is where we keep track of
     *   the parameter index.  
     */
    int param_index_;

    /* 
     *   context variable number - if this is a context variable, this is
     *   the stack frame index of the variable containing the object
     *   containing our value 
     */
    int ctx_var_num_;

    /* 
     *   Context level - this is the nesting level of the anonymous
     *   function's context.  A level of 0 indicates a local variable in
     *   the current function; a level of 1 indicates a variable in the
     *   enclosing scope; 2 is the next enclosing scope; and so on. 
     */
    int ctx_level_;

    /* 
     *   for a context variable, the index of this variable in the local
     *   context array (zero means that it's not assigned) 
     */
    int ctx_arr_idx_;

    /* 
     *   for a variable within an anonymous function, the original local
     *   variable symbol from the enclosing scope to which this variable
     *   refers 
     */
    class CTcSymLocal *ctx_orig_;

    /* 
     *   For a formal parameter with a default value, the default value
     *   expression, and the sequence number among the default expressions.
     *   We process these expressions at function entry in left-to-right
     *   order for the argument list, to ensure a predicatable order for
     *   dependency resolution and side effects.
     */
    class CTcPrsNode *defval_expr_;
    int defval_seqno_;

    /* true -> parameter, false -> local variable */
    unsigned int is_param_ : 1;

    /* true -> named parameter */
    unsigned int is_named_param_ : 1;

    /* true -> optional parametre */
    unsigned int is_opt_param_ : 1;

    /* 
     *   true -> not a real parameter, but a "parameter list" local - this is
     *   a local variable into which we deposit a list of the parameters
     *   beyond a certain point, defined via the "[varname]" syntax in a
     *   formal parameter list 
     */
    unsigned int is_list_param_ : 1;

    /* true -> this is a context local variable */
    unsigned int is_ctx_local_ : 1;

    /* flag: the context variable number has been assigned */
    unsigned int ctx_var_num_set_ : 1;

    /* true -> the value was referenced somewhere in its scope */
    unsigned int val_used_ : 1;

    /* true -> the value was assigned */
    unsigned int val_assigned_ : 1;

    /* true -> symbol has been referenced */
    unsigned int referenced_ : 1;
};

/*
 *   Dynamic local.  This is a symbol entry for a local variable accessed
 *   through a StackFrameRef object, for dynamic compilation. 
 */
class CTcSymDynLocalBase: public CTcSymbol
{
public:
    CTcSymDynLocalBase(const char *str, size_t len, int copy,
                       tctarg_obj_id_t fref, int varnum, int ctxidx)
        : CTcSymbol(str, len, copy, TC_SYM_DYNLOCAL)
    {
        fref_ = fref;
        varnum_ = varnum;
        ctxidx_ = ctxidx;
    }

protected:
    /* the StackFrameRef object giving the frame containing the variable */
    tctarg_obj_id_t fref_;

    /* the variable number in the frame */
    int varnum_;

    /* the context index (0 if it's an ordinary frame variable) */
    int ctxidx_;
};

/*
 *   Built-in function 
 */
class CTcSymBifBase: public CTcSymbol
{
public:
    CTcSymBifBase(const char *str, size_t len, int copy,
                  ushort func_set_id, ushort func_idx, int has_retval,
                  int min_argc, int max_argc, int varargs)
        : CTcSymbol(str, len, copy, TC_SYM_BIF)
    {
        func_set_id_ = func_set_id;
        func_idx_ = func_idx;
        has_retval_ = has_retval;
        min_argc_ = min_argc;
        max_argc_ = max_argc;
        varargs_ = varargs;
    }

    /* this can't be an lvalue */
    virtual int check_lvalue() const { return FALSE; }

    /* get the function set ID and index in the function set */
    ushort get_func_set_id() const { return func_set_id_; }
    ushort get_func_idx() const { return func_idx_; }

    /* 
     *   Get the minimum and maximum argument counts.  If is_varargs()
     *   returns true, there is no maximum, in which case get_max_argc()
     *   is not meaningful.  
     */
    int get_min_argc() const { return min_argc_; }
    int get_max_argc() const { return max_argc_; }
    int is_varargs() const { return varargs_; }

    /* determine if the function has a return value */
    int has_retval() const { return has_retval_; }

    /* 
     *   Do not write this to a symbol export file - this symbol is
     *   external to us, hence we have no need to define it for other
     *   modules to import.  
     */
    virtual int write_to_sym_file(class CVmFile *) { return FALSE; }

    /* write to an object file */
    virtual int write_to_obj_file(class CVmFile *fp);

    /* load from an object file */
    static int load_from_obj_file(class CVmFile *fp, const textchar_t *fname);

protected:
    /* function set ID */
    ushort func_set_id_;

    /* index within the function set */
    ushort func_idx_;

    /* minimum and maximum argument count, and varargs flag */
    int min_argc_;
    int max_argc_;
    unsigned int varargs_ : 1;

    /* true -> function has a return value, false -> void return */
    unsigned int has_retval_ : 1;
};

/*
 *   External function 
 */
class CTcSymExtfnBase: public CTcSymbol
{
public:
    CTcSymExtfnBase(const char *str, size_t len, int copy,
                    int argc, int varargs, int has_retval)
        : CTcSymbol(str, len, copy, TC_SYM_EXTFN)
    {
        /* offset is not yet known */
        argc_ = argc;
        varargs_ = varargs;
        has_retval_ = has_retval;
    }

    /* this can't be an lvalue */
    virtual int check_lvalue() const { return FALSE; }

    /* 
     *   Get the argument count.  If is_varargs() returns true, this is a
     *   minimum argument count; otherwise, this is the exact number of
     *   arguments required.  
     */
    int get_argc() const { return argc_; }
    int is_varargs() const { return varargs_; }

    /* determine if the function has a return value */
    int has_retval() const { return has_retval_; }

protected:
    /* argument count */
    int argc_;

    /* flag: variable arguments (in which case argc_ is only a minimum) */
    unsigned int varargs_ : 1;

    /* flag: function has a return value (false -> void function) */
    unsigned int has_retval_ : 1;
};

/*
 *   Code ("goto") label.
 */
class CTcSymLabelBase: public CTcSymbol
{
public:
    CTcSymLabelBase(const char *str, size_t len, int copy)
        : CTcSymbol(str, len, copy, TC_SYM_LABEL)
    {
        /* the symbol is not yet referenced */
        referenced_ = FALSE;

        /* we don't know the defining statement node yet */
        stm_ = 0;
    }

    /* this can't be an lvalue */
    virtual int check_lvalue() const { return FALSE; }

    /* get/set the defining statement node */
    class CTPNStmLabel *get_stm() const { return stm_; }
    void set_stm(class CTPNStmLabel *stm) { stm_ = stm; }

    /* add control flow flags to our statement */
    void add_control_flow_flags(ulong flags);

protected:
    /* the defining statement's parse node */
    class CTPNStmLabel *stm_;

    /* 
     *   flag: the symbol is referenced by at least one statement (we keep
     *   track of this so that we warn when a label is defined but never
     *   used as a target) 
     */
    unsigned int referenced_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   Program Node.  This is the top-level node in the tree; this node
 *   contains the entire program as a sublist. 
 */
class CTPNStmProg: public CTPNStm
{
public:
    CTPNStmProg(class CTPNStmTop *first_stm)
    {
        /* remember the head of our statement list */
        first_stm_ = first_stm;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /*
     *   Generate the program code and write to the image file.  After
     *   parsing, if parsing is successful, the compiler can invoke this
     *   routine to construct the image file; there is no need to invoke
     *   gen_code() directly.  
     */
    void build_image(class CVmFile *image_fp, uchar xor_mask,
                     const char tool_data[4]);

    /*
     *   Generate the program code and write to the object file.  After
     *   parsing, if parsing is successful, the compiler can invoke this
     *   routine to construct the object file.
     */
    void build_object_file(class CVmFile *object_fp, class CTcMake *make_obj);

    /* generate code for the program */
    virtual void gen_code(int discard, int for_condition);

protected:
    /* 
     *   Generate code.  Returns zero on success, non-zero on error.  This
     *   is a common routine used by build_image() and build_object_file()
     *   to complete code generation after parsing. 
     */
    int gen_code_for_build();
    
    /* head of our list */
    class CTPNStmTop *first_stm_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Top-level object - this is a base class for statements which can
 *   appear as top-level definitions, such as functions and objects.
 *   
 *   This class should not need to be derived per target, so further
 *   target-generic base classes are derived from this directly.  
 */
class CTPNStmTop: public CTPNStm
{
public:
    CTPNStmTop()
    {
        /* we're not in a list yet */
        next_stm_top_ = 0;
    }
    
    /* get/set the next top-level statement in the top-level list */
    CTPNStmTop *get_next_stm_top() const { return next_stm_top_; }
    void set_next_stm_top(CTPNStmTop *nxt) { next_stm_top_ = nxt; }

    /* 
     *   Check the local symbol table.  This is called after all of the
     *   code has been generated for the entire program, which ensures
     *   that all of the code that could affect any local symbols (even
     *   those shared through context variables) has been finished.  By
     *   default we do nothing.  
     */
    virtual void check_locals() { }

    /* get/set the dynamic compilation run-time object ID */
    tctarg_obj_id_t get_dyn_obj_id() const { return dyn_obj_id_; }
    void set_dyn_obj_id(tctarg_obj_id_t id) { dyn_obj_id_ = id; }

protected:
    /*
     *   Dynamic code compilation object ID.  When we compile new source code
     *   at run-time, the bytecode is stored in a DynamicFunc instance rather
     *   than in the constant code pool, because the code pool is limited to
     *   code loaded from the image file.  Before generating code, the
     *   dynamic compiler assigns a new DynamicFunc ID to each top-level
     *   statement in the nested statement list.  The DynamicFunc instances
     *   aren't actually populated until the code is generated, but the
     *   object numbers are assigned in advance to allow cross-references in
     *   generated code.  
     */
    tctarg_obj_id_t dyn_obj_id_;

    /* next top-level statement in the top-level list */
    CTPNStmTop *next_stm_top_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Anonymous function 
 */
class CTPNAnonFuncBase: public CTcPrsNode
{
public:
    CTPNAnonFuncBase(class CTPNCodeBody *code_body, int has_retval,
                     int is_method)
    {
        /* remember my code body and return value status */
        code_body_ = code_body;
        has_retval_ = has_retval;
        is_method_ = is_method;
    }

    /* no constant value */
    virtual class CTcConstVal *get_const_val() { return 0; }

    /* I have a return value when called if the underlying code body does */
    virtual int has_return_value_on_call() const { return has_retval_; }

    /* 
     *   fold constants - we don't have to fold constants in the
     *   underlying code body, because its constants will be folded along
     *   with other top-level statements
     */
    virtual class CTcPrsNode *fold_constants(class CTcPrsSymtab *)
        { return this; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }

    /* mark as replaced/obsolete */
    void set_replaced(int flag);

protected:
    /* code body of the function */
    class CTPNCodeBody *code_body_;

    /* true -> the underlying code body has a return value */
    unsigned int has_retval_ : 1;

    /* true -> this is a method; false -> it's a function  */
    unsigned int is_method_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Code Body.  A code body has local variables, goto labels, and a
 *   compound statement.  
 */
class CTPNCodeBodyBase: public CTPNStmTop
{
public:
    CTPNCodeBodyBase(class CTcPrsSymtab *lcltab, class CTcPrsSymtab *gototab,
                     class CTPNStm *stm, int argc, int opt_argc, int varargs,
                     int varargs_list, class CTcSymLocal *varargs_list_local,
                     int local_cnt, int self_valid,
                     struct CTcCodeBodyRef *enclosing_code_body);

    /* 
     *   Mark the code as having been replaced - this is used when this is
     *   the code body for a function, and the function is replaced by
     *   another implementation in the same translation unit.  When we've
     *   been replaced, we can't be referenced, hence we don't need to
     *   generate any code. 
     */
    void set_replaced(int f) { replaced_ = f; }
    int is_replaced() const { return replaced_; }

    /* 
     *   Set our fixup list anchor - our creator must invoke this if we
     *   are to use a fixup list associated with a symbol table entry.  A
     *   code body that can be reached from a symbol table entry (such as
     *   a function name) must have its fixup list anchor associated with
     *   the symbol table entry so that we can create the necessary
     *   associations in an object file. 
     */
    void set_symbol_fixup_list(class CTcSymFunc *sym,
                               struct CTcAbsFixup **fixup_list_anchor)
    {
        /* remember the information */
        fixup_owner_sym_ = sym;
        fixup_list_anchor_ = fixup_list_anchor;
    }

    /* use our internal fixup list */
    void use_internal_fixup_list()
    {
        /* we have no function symbol */
        fixup_owner_sym_ = 0;

        /* use our internal list anchor */
        fixup_list_anchor_ = &internal_fixups_;
    }

    /* get my associated function symbol, if there is one */
    class CTcSymFunc *get_func_sym() const { return fixup_owner_sym_; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /*
     *   Check for unreferenced labels.  Scans the 'goto' symbol table and
     *   ensures that each labeled statement has been referenced; logs
     *   a warning for each label that isn't referenced.
     */
    void check_unreferenced_labels();

    /* get a pointer to my fixup list head */
    struct CTcAbsFixup **get_fixup_list_head() const
        { return fixup_list_anchor_; }

    /* set the location of the opening brace */
    void set_start_location(class CTcTokFileDesc *desc, long linenum)
    {
        start_desc_ = desc;
        start_linenum_ = linenum;
    }

    /* set the location of the closing brace */
    void set_end_location(class CTcTokFileDesc *desc, long linenum)
    {
        end_desc_ = desc;
        end_linenum_ = linenum;
    }

    /* add an absolute fixup for the symbol at the given stream offset */
    void add_abs_fixup(class CTcDataStream *ds, ulong ofs);

    /* add an absolute fixup for the symbol at the current stream offset */
    void add_abs_fixup(class CTcDataStream *ds);

    /* mark the code body as requiring a local variable context object */
    void set_local_ctx(int var_num, int max_local_arr_idx)
    {
        /* note that we have a local context */
        has_local_ctx_ = TRUE;

        /* 
         *   remember our the local variable number that contains our
         *   context object 
         */
        local_ctx_var_ = var_num;

        /* remember our maximum context local array index */
        local_ctx_arr_size_ = max_local_arr_idx;
    }

    /* does this function have a local context? */
    int has_local_ctx() const { return has_local_ctx_; }

    /* 
     *   Get the context variable for a given recursion level, creating
     *   the new variable if necessary.  Level 1 is the first enclosing
     *   scope, 2 is the next enclosing scope, and so on.  
     */
    int get_or_add_ctx_var_for_level(int level);

    /* 
     *   find a context variable given a requested level; returns true if
     *   successful, false if not 
     */
    int get_ctx_var_for_level(int level, int *varnum);

    /* get the head of the local context list */
    struct CTcCodeBodyCtx *get_ctx_head() const { return ctx_head_; }
    struct CTcCodeBodyCtx *get_ctx_tail() const { return ctx_tail_; }

    /* mark this as an anonymous method */
    int is_anon_method() const { return is_anon_method_; }
    void set_anon_method(int f) { is_anon_method_ = f; }

    /* mark this as a dynamic function or method */
    int is_dyn_func() const { return is_dyn_func_; }
    int is_dyn_method() const { return is_dyn_method_; }
    void set_dyn_func(int f) { is_dyn_func_ = f; }
    void set_dyn_method(int f) { is_dyn_method_ = f; }

    /* get/set 'self' reference status */
    int self_referenced() const { return self_referenced_; }
    void set_self_referenced(int f) { self_referenced_ = f; }

    /* get/set full method context reference status */
    int full_method_ctx_referenced() const
        { return full_method_ctx_referenced_; }
    void set_full_method_ctx_referenced(int f)
        { full_method_ctx_referenced_ = f; }

    /* get/set the local context need for 'self' */
    int local_ctx_needs_self() const { return local_ctx_needs_self_; }
    void set_local_ctx_needs_self(int f) { local_ctx_needs_self_ = f; }

    /* get/set the local context need for the full method context */
    int local_ctx_needs_full_method_ctx() const
        { return local_ctx_needs_full_method_ctx_; }
    void set_local_ctx_needs_full_method_ctx(int f)
        { local_ctx_needs_full_method_ctx_ = f; }

    /* get/set the operator overload flag */
    int is_operator_overload() const { return op_overload_; }
    void set_operator_overload(int f) { op_overload_ = f; }

    /*
     *   Get the base function symbol for a code body defining a modified
     *   function (i.e., 'modify <funcname>...').  This is the function to
     *   which 'replaced' refers within this code body and within nested code
     *   bodies.  
     */
    class CTcSymFunc *get_replaced_func() const;

    /* get the immediately enclosing code body */
    class CTPNCodeBody *get_enclosing() const;

    /* get my argument count */
    int get_argc() const { return argc_; }
    int get_opt_argc() const { return opt_argc_; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        stm_ = (CTPNStm *)stm_->adjust_for_dyn(info);
        return this;
    }

protected:
    /* enumerator callback for unreferenced label check */
    static void unref_label_cb(void *ctx, class CTcSymbol *sym);

    /* get the outermost enclosing code cody */
    class CTPNCodeBody *get_outermost_enclosing() const;

    /* source location of opening brace */
    class CTcTokFileDesc *start_desc_;
    long start_linenum_;

    /* source location of the end of the code body (the closing '}') */
    class CTcTokFileDesc *end_desc_;
    long end_linenum_;

    /* local symbol table for the code body */
    class CTcPrsSymtab *lcltab_;

    /* goto symbol table for the code body */
    class CTcPrsSymtab *gototab_;

    /* statement making up the code body */
    class CTPNStm *stm_;

    /* 
     *   fixup list head pointer - this will be set to &internal_fixups_
     *   if we don't have a symbol table fixup list, or will point to our
     *   symbol table fixup list if we have an external symbol providing
     *   the fixup list 
     */
    struct CTcAbsFixup **fixup_list_anchor_;

    /* our symbol table entry, if our fixup list is defined there */
    class CTcSymFunc *fixup_owner_sym_;

    /* 
     *   Our internal list head for our fixup list for references to this
     *   code block.  This is used only if we don't have an external fixup
     *   list defined in a symbol table entry. 
     */
    struct CTcAbsFixup *internal_fixups_;

    /* enclosing code body pointer */
    struct CTcCodeBodyRef *enclosing_code_body_;

    /* head/tail of anonymous function context list */
    struct CTcCodeBodyCtx *ctx_head_;
    struct CTcCodeBodyCtx *ctx_tail_;
    
    /* number of local variable slots required */
    int local_cnt_;

    /* number of arguments */
    int argc_;

    /* number of additional optional arguments */
    int opt_argc_;

    /* if function has a varargs-list local, this is the local ID */
    class CTcSymLocal *varargs_list_local_;

    /* if function has a local context, this is its local ID */
    int local_ctx_var_;

    /* 
     *   number of slots in the local context array, if we have a local
     *   context at all 
     */
    int local_ctx_arr_size_;

    /* 
     *   flag: we have a varargs-list local (varargs_list_local_ has the
     *   local variable symbol object) 
     */
    unsigned int varargs_list_ : 1;

    /* flag: function takes variable number of arguments */
    unsigned int varargs_ : 1;

    /* flag: I've been replaced (and hence don't need codegen) */
    unsigned int replaced_ : 1;

    /* 
     *   flag: the code body requires a local context - this is set if we
     *   have any local variables that are stored in context objects for
     *   use in anonymous functions 
     */
    unsigned int has_local_ctx_ : 1;

    /* flag: the local context requires 'self' */
    unsigned int local_ctx_needs_self_ : 1;

    /* 
     *   flag: the local context requires the full method context (self,
     *   targetprop, targetobj, definingobj) 
     */
    unsigned int local_ctx_needs_full_method_ctx_ : 1;

    /* flag: 'self' is valid in this code body */
    unsigned int self_valid_ : 1;

    /* flags: 'self' and full method context referenced in this code body */
    unsigned int self_referenced_ : 1;
    unsigned int full_method_ctx_referenced_ : 1;

    /* flag: this is an operator overload method */
    unsigned int op_overload_ : 1;

    /* flag: this is an anonymous method */
    unsigned int is_anon_method_ : 1;

    /* flags: this is a dynamic function/method */
    unsigned int is_dyn_func_ : 1;
    unsigned int is_dyn_method_ : 1;
};

/*
 *   Context level information 
 */
struct CTcCodeBodyCtx
{
    CTcCodeBodyCtx() { }
    
    /* level number */
    int level_;

    /* local variable containing the context object for this level */
    int var_num_;

    /* next/previous level structure in list */
    CTcCodeBodyCtx *nxt_;
    CTcCodeBodyCtx *prv_;
};

/*
 *   Code body reference.
 *   
 *   We don't create a CTPNCodeBody object for a code body we're parsing
 *   until after we finish parsing it, so we can't establish a pointer to an
 *   enclosing code body while we're parsing a nested code body within the
 *   enclosing one.  However, we want nested code body objects to be able to
 *   refer to their enclosing code bodies.  To deal with this, we create a
 *   "code body reference" object for each code body at the start of parsing
 *   the code body, and then fill it in with the code body object once we
 *   finish parsing; nested code bodies can refer to the reference object
 *   during parsing, so that after parsing they can get to the code body
 *   itself.  
 */
struct CTcCodeBodyRef
{
    CTcCodeBodyRef() { ptr = 0; }

    /* pointer to enclosing code body */
    class CTPNCodeBody *ptr;
};

/* ------------------------------------------------------------------------ */
/*
 *   Compound statement.  A compound statement simply contains a list of
 *   statements to be executed in sequence; this corresponds to a set of
 *   statements enclosed in braces.  A compound statement can have its own
 *   set of local variables, since the enclosing braces constitute a local
 *   scope.
 */
class CTPNStmCompBase: public CTPNStm
{
public:
    CTPNStmCompBase(class CTPNStm *first_stm, class CTcPrsSymtab *symtab);

    /* get my first statement */
    class CTPNStm *get_first_stm() const { return first_stm_; }

    /* get the local scope's symbol table */
    class CTcPrsSymtab *get_symtab() const { return symtab_; }

    /* set our own-scope flag */
    void set_has_own_scope(int f) { has_own_scope_ = f; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* generate code for the compound statement */
    virtual void gen_code(int discard, int for_condition);

    /* evaluate control flow for the statement list */
    virtual unsigned long get_control_flow(int warn) const;

    /*
     *   Determine if this statement has a code label.  We have a code
     *   label if our first statement has a code label.  
     */
    virtual int has_code_label() const
    {
        return (first_stm_ == 0 ? FALSE : first_stm_->has_code_label());
    }

    /* get the location of the closing brace */
    class CTcTokFileDesc *get_end_desc() const { return end_desc_; }
    long get_end_linenum() const { return end_linenum_; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *);

protected:
    /* source location of closing brace */
    class CTcTokFileDesc *end_desc_;
    long end_linenum_;
    
    /* first statement in our list of statements */
    class CTPNStm *first_stm_;
    
    /* 
     *   our local symbol table; this is a private symbol table with the
     *   symbols for our local scope if we have any, or is simply the
     *   enclosing scope's symbol table if there are no variables in the
     *   local scope 
     */
    class CTcPrsSymtab *symtab_;

    /* 
     *   flag: we have our own private symbol table (it's not the
     *   enclosing scope's symbol table) 
     */
    unsigned int has_own_scope_ : 1;
};

/* ------------------------------------------------------------------------ */
/* 
 *   Null statement - this is the statement that results from an empty
 *   pair of braces, or simply a semicolon. 
 */
class CTPNStmNullBase: public CTPNStm
{
public:
    CTPNStmNullBase() { }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *) { return this; }

    /* generate code for the compound statement */
    virtual void gen_code(int, int) { }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *)
        { return this; }
};

/* ------------------------------------------------------------------------ */
/*
 *   Expression statement. 
 */
class CTPNStmExprBase: public CTPNStm
{
public:
    CTPNStmExprBase(class CTcPrsNode *expr)
    {
        /* remember my expression */
        expr_ = expr;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* generate code for the compound statement */
    virtual void gen_code(int discard, int for_condition);

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        expr_ = expr_->adjust_for_dyn(info);
        return this;
    }

protected:
    /* our expression */
    class CTcPrsNode *expr_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Static property initializer statement.  This is used to hold the
 *   expression of a static initializer, just under the CTPNObjProp
 *   element in the tree.  
 */
class CTPNStmStaticPropInitBase: public CTPNStm
{
public:
    CTPNStmStaticPropInitBase(class CTcPrsNode *expr,
                              tctarg_prop_id_t prop)
    {
        /* remember my expression and the property to be initialized */
        expr_ = expr;
        prop_ = prop;
    }

    /* 
     *   fold constants - this is never used, because we always fold the
     *   constants in the underlying expression before we create one of
     *   these nodes, as this type of node is only created during constant
     *   folding of the containing property initializer node 
     */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *) { return this; }

protected:
    /* our expression */
    class CTcPrsNode *expr_;

    /* property we initialize */
    tctarg_prop_id_t prop_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'if' statement 
 */
class CTPNStmIfBase: public CTPNStm
{
public:
    CTPNStmIfBase(class CTcPrsNode *cond_expr,
                  class CTPNStm *then_part, class CTPNStm *else_part)
    {
        /* remember the condition and the two statements */
        cond_expr_ = cond_expr;
        then_part_ = then_part;
        else_part_ = else_part;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the conditional */
    virtual unsigned long get_control_flow(int warn) const;

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        if (then_part_ != 0)
            then_part_ = (CTPNStm *)then_part_->adjust_for_dyn(info);
        if (else_part_ != 0)
            else_part_ = (CTPNStm *)else_part_->adjust_for_dyn(info);
        return this;
    }

protected:
    /* condition expression */
    class CTcPrsNode *cond_expr_;

    /* 'then' and 'else' substatements */
    class CTPNStm *then_part_;
    class CTPNStm *else_part_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'for' statement 
 */
class CTPNStmForBase: public CTPNStmEnclosing
{
public:
    CTPNStmForBase(class CTcPrsNode *init_expr,
                   class CTcPrsNode *cond_expr,
                   class CTcPrsNode *reinit_expr,
                   class CTPNForIn *in_exprs,
                   class CTcPrsSymtab *symtab,
                   class CTPNStmEnclosing *enclosing_stm)
        : CTPNStmEnclosing(enclosing_stm)
    {
        /* remember the statement parts */
        init_expr_ = init_expr;
        cond_expr_ = cond_expr;
        reinit_expr_ = reinit_expr;
        in_exprs_ = in_exprs;
        symtab_ = symtab;

        /* no body yet */
        body_stm_ = 0;

        /* 
         *   presume we use our parent's symbol table, not our own private
         *   one 
         */
        has_own_scope_ = FALSE;
    }

    /* set the body */
    void set_body(class CTPNStm *body) { body_stm_ = body; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the loop */
    virtual unsigned long get_control_flow(int warn) const;

    /* set our own-scope flag */
    void set_has_own_scope(int f) { has_own_scope_ = f; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        if (init_expr_ != 0)
            init_expr_ = init_expr_->adjust_for_dyn(info);
        if (cond_expr_ != 0)
            cond_expr_ = cond_expr_->adjust_for_dyn(info);
        if (reinit_expr_ != 0)
            reinit_expr_ = reinit_expr_->adjust_for_dyn(info);
        if (body_stm_ != 0)
            body_stm_ = (CTPNStm *)body_stm_->adjust_for_dyn(info);

        return this;
    }

protected:
    /* initialization expression */
    class CTcPrsNode *init_expr_;

    /* loop condition expression */
    class CTcPrsNode *cond_expr_;

    /* reinitialization expression */
    class CTcPrsNode *reinit_expr_;

    /* 
     *   Head of list of "in" expressions (lval in collection, lval in
     *   from..to).  These appear as ordinary expressions within the
     *   init_expr_ comma list, but we track them separately here, since they
     *   implicitly generate code in the condition and reinit phases as well.
     */
    class CTPNForIn *in_exprs_;
    
    /* body of the loop */
    class CTPNStm *body_stm_;

    /* our local scope symbol table */
    class CTcPrsSymtab *symtab_;

    /* flag: we have our own private symbol table (not our parent's) */
    unsigned int has_own_scope_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   '<variable> in <expression>' node, for 'for' statements.  Since 3.1,
 *   regular 'for' statements can include 'in' clauses mixed in with other
 *   initializer expressions.  
 */
class CTPNVarInBase: public CTPNForIn
{
public:
    CTPNVarInBase(class CTcPrsNode *lval, class CTcPrsNode *expr,
                  int iter_local_id)
    {
        lval_ = lval;
        expr_ = expr;
        iter_local_id_ = iter_local_id;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        lval_ = lval_->adjust_for_dyn(info);
        expr_ = expr_->adjust_for_dyn(info);

        return this;
    }

protected:
    /* the lvalue for the control variable */
    class CTcPrsNode *lval_;

    /* the collection expression */
    class CTcPrsNode *expr_;

    /* private local variable for the iterator */
    int iter_local_id_;
};

/*
 *   '<variable> in <from> .. <to>' node, for 'for' statements.  Since 3.1,
 *   'for' statements can include 'in' clauses with range expressions using
 *   '..'.  
 */
class CTPNVarInRangeBase: public CTPNForIn
{
public:
    CTPNVarInRangeBase(class CTcPrsNode *lval,
                       class CTcPrsNode *from_expr,
                       class CTcPrsNode *to_expr,
                       class CTcPrsNode *step_expr,
                       int to_local_id, int step_local_id)
    {
        lval_ = lval;
        from_expr_ = from_expr;
        to_expr_ = to_expr;
        step_expr_ = step_expr;
        to_local_id_ = to_local_id;
        step_local_id_ = step_local_id;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        lval_ = lval_->adjust_for_dyn(info);
        from_expr_ = from_expr_->adjust_for_dyn(info);
        to_expr_ = to_expr_->adjust_for_dyn(info);
        if (step_expr_ != 0)
            step_expr_ = step_expr_->adjust_for_dyn(info);

        return this;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

protected:
    /* the lvalue for the control variable */
    class CTcPrsNode *lval_;

    /* the 'from' and 'to' expressions */
    class CTcPrsNode *from_expr_;
    class CTcPrsNode *to_expr_;

    /* the optional 'step' expression */
    class CTcPrsNode *step_expr_;

    /* local variable number for the evaluated "to" expression */
    int to_local_id_;

    /* 
     *   local variable number for the evaluated "step" expression - this is
     *   used only if we have a non-constant step expression 
     */
    int step_local_id_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'foreach' statement 
 */
class CTPNStmForeachBase: public CTPNStmEnclosing
{
public:
    CTPNStmForeachBase(class CTcPrsNode *iter_expr,
                       class CTcPrsNode *coll_expr,
                       class CTcPrsSymtab *symtab,
                       class CTPNStmEnclosing *enclosing_stm,
                       int iter_local_id)
        : CTPNStmEnclosing(enclosing_stm)
    {
        /* remember the statement parts */
        iter_expr_ = iter_expr;
        coll_expr_ = coll_expr;
        symtab_ = symtab;

        /* no body yet */
        body_stm_ = 0;

        /* remember my private iterator local */
        iter_local_id_ = iter_local_id;

        /* 
         *   presume we use our parent's symbol table, not our own private
         *   one 
         */
        has_own_scope_ = FALSE;
    }
    
    /* set the body */
    void set_body(class CTPNStm *body) { body_stm_ = body; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);
    
    /* evaluate control flow for the loop */
    virtual unsigned long get_control_flow(int warn) const;
    
    /* set our own-scope flag */
    void set_has_own_scope(int f) { has_own_scope_ = f; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        iter_expr_ = iter_expr_->adjust_for_dyn(info);
        coll_expr_ = coll_expr_->adjust_for_dyn(info);
        body_stm_ = (CTPNStm *)body_stm_->adjust_for_dyn(info);
        return this;
    }
    
protected:
    /* iteration lvalue expression */
    class CTcPrsNode *iter_expr_;
    
    /* collection expression */
    class CTcPrsNode *coll_expr_;
    
    /* body of the loop */
    class CTPNStm *body_stm_;

    /* our local scope symbol table */
    class CTcPrsSymtab *symtab_;

    /* ID of secret local variable containing the iterator object */
    int iter_local_id_;
    
    /* flag: we have our own private symbol table (not our parent's) */
    unsigned int has_own_scope_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   'while' statement - this is an enclosing statement, because it can
 *   interact with 'try' blocks with 'break' and 'continue' statements
 *   used within the body of the loop 
 */
class CTPNStmWhileBase: public CTPNStmEnclosing
{
public:
    CTPNStmWhileBase(class CTcPrsNode *cond_expr,
                     class CTPNStmEnclosing *enclosing_stm)
        : CTPNStmEnclosing(enclosing_stm)
    {
        /* remember the statement parts */
        cond_expr_ = cond_expr;

        /* no body yet */
        body_stm_ = 0;
    }

    /* set the body */
    void set_body(class CTPNStm *body) { body_stm_ = body; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the loop */
    virtual unsigned long get_control_flow(int warn) const;

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        cond_expr_ = cond_expr_->adjust_for_dyn(info);
        body_stm_ = (CTPNStm *)body_stm_->adjust_for_dyn(info);
        return this;
    }
    
protected:
    /* loop condition expression */
    class CTcPrsNode *cond_expr_;

    /* body of the loop */
    class CTPNStm *body_stm_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'do-while' statement 
 */
class CTPNStmDoWhileBase: public CTPNStmEnclosing
{
public:
    CTPNStmDoWhileBase(class CTPNStmEnclosing *enclosing_stm)
        : CTPNStmEnclosing(enclosing_stm)
    {
        /* no condition or body yet */
        cond_expr_ = 0;
        body_stm_ = 0;

        /* 
         *   we don't know the location of the 'while' part yet, so assume
         *   it's at the same location as the 'do' for now 
         */
        while_desc_ = file_;
        while_linenum_ = linenum_;
    }

    /* set the condition */
    void set_cond(class CTcPrsNode *cond) { cond_expr_ = cond; }

    /* set the body */
    void set_body(class CTPNStm *body) { body_stm_ = body; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the loop */
    virtual unsigned long get_control_flow(int warn) const;

    /* set the source location of the 'while' statement */
    void set_while_pos(class CTcTokFileDesc *desc, long linenum)
    {
        while_desc_ = desc;
        while_linenum_ = linenum;
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        cond_expr_ = cond_expr_->adjust_for_dyn(info);
        body_stm_ = (CTPNStm *)body_stm_->adjust_for_dyn(info);
        return this;
    }
    
protected:
    /* source location of 'while' */
    class CTcTokFileDesc *while_desc_;
    long while_linenum_;

    /* loop condition expression */
    class CTcPrsNode *cond_expr_;

    /* body of the loop */
    class CTPNStm *body_stm_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'break' statement 
 */
class CTPNStmBreakBase: public CTPNStm
{
public:
    CTPNStmBreakBase()
    {
        /* no label yet */
        lbl_ = 0;
        lbl_len_ = 0;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *)
    {
        /* we have nothing to fold - return unchangdd */
        return this;
    }

    /* evaluate control flow for the statement */
    virtual unsigned long get_control_flow(int warn) const;

    /* set my label */
    void set_label(const class CTcToken *tok);

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
        { return this; }

protected:
    /* my break-to label, if specified */
    const textchar_t *lbl_;
    size_t lbl_len_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'continue' statement 
 */
class CTPNStmContinueBase: public CTPNStm
{
public:
    CTPNStmContinueBase()
    {
        /* no label yet */
        lbl_ = 0;
        lbl_len_ = 0;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *)
    {
        /* we have nothing to fold - return unchangdd */
        return this;
    }

    /* evaluate control flow for the statement */
    virtual unsigned long get_control_flow(int warn) const;

    /* set my label */
    void set_label(const class CTcToken *tok);

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
        { return this; }

protected:
    /* my break-to label, if specified */
    const textchar_t *lbl_;
    size_t lbl_len_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'return' statement; if the expression node is null, this is a simple
 *   void return 
 */
class CTPNStmReturnBase: public CTPNStm
{
public:
    CTPNStmReturnBase(class CTcPrsNode *expr)
    {
        /* remember the expression */
        expr_ = expr;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the statement */
    virtual unsigned long get_control_flow(int /*warn*/) const
    {
        /* 
         *   we return a value if we have an expression, otherwise we're
         *   just a void return 
         */
        return (expr_ == 0 ? TCPRS_FLOW_RET_VOID : TCPRS_FLOW_RET_VAL);
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        expr_ = expr_->adjust_for_dyn(info);
        return this;
    }

protected:
    /* the expression to return, or null for a void return */
    class CTcPrsNode *expr_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'switch' statement.  This is an 'enclosing' statement type because
 *   'break' can refer to the statement from within the code body.  
 */
class CTPNStmSwitchBase: public CTPNStmEnclosing
{
public:
    CTPNStmSwitchBase(class CTPNStmEnclosing *enclosing_stm)
        : CTPNStmEnclosing(enclosing_stm)
    {
        /* we have no body or expression yet */
        expr_ = 0;
        body_ = 0;

        /* we have no case or default labels yet */
        case_cnt_ = 0;
        has_default_ = FALSE;
    }

    /* set my expression */
    void set_expr(class CTcPrsNode *expr) { expr_ = expr; }

    /* set my body */
    void set_body(class CTPNStm *body) { body_ = body; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the statement */
    virtual unsigned long get_control_flow(int warn) const;

    /* get the number of 'case' labels */
    int get_case_cnt() const { return case_cnt_; }

    /* determine if there's a 'default' label */
    int get_has_default() const { return has_default_ != 0; }

    /* increment the number of case labels */
    void inc_case_cnt() { ++case_cnt_; }

    /* mark that the body has a 'default' label */
    void set_has_default() { has_default_ = TRUE; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        expr_ = expr_->adjust_for_dyn(info);
        body_ = (CTPNStm *)body_->adjust_for_dyn(info);
        return this;
    }

protected:
    /* the controlling expression */
    class CTcPrsNode *expr_;

    /* body of the switch */
    class CTPNStm *body_;

    /* number of case labels */
    int case_cnt_;

    /* flag: we have a 'default' label */
    unsigned int has_default_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   code label statement - a code label contains the statement that it
 *   labels.  This is an enclosing statement type because targeting a
 *   label with a 'goto', 'break', or 'continue' requires that we be able
 *   to find intermediate 'try' blocks.  
 */
class CTPNStmLabelBase: public CTPNStmEnclosing
{
public:
    CTPNStmLabelBase(class CTcSymLabel *lbl, CTPNStmEnclosing *enclosing)
        : CTPNStmEnclosing(enclosing)
    {
        /* remember my label */
        lbl_ = lbl;

        /* we have no enclosed statement yet */
        stm_ = 0;

        /* we're not targeted with any 'break' or 'continue' statements yet */
        control_flow_flags_ = 0;
    }

    /* set my enclosed statement */
    void set_stm(class CTPNStm *stm) { stm_ = stm; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the statement */
    virtual unsigned long get_control_flow(int warn) const;

    /* I am a code label, thus I have a code label */
    virtual int has_code_label() const { return TRUE; }

    /* 
     *   add explicit targeting flags (break, continue, goto) to my
     *   statement 
     */
    void add_control_flow_flags(ulong flags);

    /* get my explicit control flow flag */
    ulong get_explicit_control_flow_flags() const
        { return control_flow_flags_; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        stm_ = (CTPNStm *)adjust_for_dyn(info);
        return this;
    }

protected:
    /* my label */
    class CTcSymLabel *lbl_;
    
    /* my enclosed statement */
    class CTPNStm *stm_;

    /*
     *   My explicit control flow flags.  Any time this label appears in a
     *   'break' or 'continue' statement enclosed within the labeled
     *   statement, we'll set the appropriate flags here.  When charting
     *   the control flow through the statement, we will take these into
     *   account after charting the flow through our body.  
     */
    ulong control_flow_flags_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'goto' statement
 */
class CTPNStmGotoBase: public CTPNStm
{
public:
    CTPNStmGotoBase(const textchar_t *lbl, size_t lbl_len)
    {
        /* 
         *   remember the label - we can store a reference to it without
         *   copying, because the tokenizer keeps symbol token text around
         *   throughout compilation 
         */
        lbl_ = lbl;
        lbl_len_ = lbl_len;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *)
    {
        /* 'goto' statements are unaffected by constant folding */
        return this;
    }

    /* evaluate control flow for the statement */
    virtual unsigned long get_control_flow(int warn) const;

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
        { return this; }

protected:
    /* our code label text */
    const textchar_t *lbl_;
    size_t lbl_len_;
};


/* ------------------------------------------------------------------------ */
/*
 *   'case' label statement
 */
class CTPNStmCaseBase: public CTPNStm
{
public:
    CTPNStmCaseBase()
    {
        /* we have no statement or expression yet */
        expr_ = 0;
        stm_ = 0;
    }

    /* set my expression */
    void set_expr(class CTcPrsNode *expr) { expr_ = expr; }

    /* set my enclosed statement */
    void set_stm(class CTPNStm *stm) { stm_ = stm; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the statement */
    virtual unsigned long get_control_flow(int warn) const;

    /* I am a code label, thus I have a code label */
    virtual int has_code_label() const { return TRUE; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        expr_ = expr_->adjust_for_dyn(info);
        stm_ = (CTPNStm *)stm_->adjust_for_dyn(info);
        return this;
    }
    
protected:
    /* my expression */
    class CTcPrsNode *expr_;

    /* my enclosed statement */
    class CTPNStm *stm_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'default' label statement 
 */
class CTPNStmDefaultBase: public CTPNStm
{
public:
    CTPNStmDefaultBase()
    {
        /* we have no statement yet */
        stm_ = 0;
    }

    /* set my enclosed statement */
    void set_stm(class CTPNStm *stm) { stm_ = stm; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the statement */
    virtual unsigned long get_control_flow(int warn) const;

    /* I am a code label, thus I have a code label */
    virtual int has_code_label() const { return TRUE; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
        { return this; }

protected:
    /* my enclosed statement */
    class CTPNStm *stm_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'try' statement 
 */
class CTPNStmTryBase: public CTPNStmEnclosing
{
public:
    CTPNStmTryBase(CTPNStmEnclosing *enclosing)
        : CTPNStmEnclosing(enclosing)
    {
        /* we have no protected body yet */
        body_stm_ = 0;

        /* we have no 'catch' or 'finally' clause yet */
        first_catch_stm_ = last_catch_stm_ = 0;
        finally_stm_ = 0;
    }
    
    /* set my protected statement */
    void set_body_stm(class CTPNStm *body_stm) { body_stm_ = body_stm; }

    /* add a catch to my list */
    void add_catch(class CTPNStmCatch *catch_stm);

    /* set the 'finally' clause */
    void set_finally(class CTPNStmFinally *finally_stm)
        { finally_stm_ = finally_stm; }

    /* determine if I have any 'catch' or 'finally' clauses */
    int has_catch_or_finally() const
    {
        return (first_catch_stm_ != 0 || finally_stm_ != 0);
    }
    
    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);
    
    /* evaluate control flow for the statement list */
    virtual unsigned long get_control_flow(int warn) const;
    
    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);

protected:
    /* my enclosed statement */
    class CTPNStm *body_stm_;

    /* head and tail of my 'catch' clause list */
    class CTPNStmCatch *first_catch_stm_;
    class CTPNStmCatch *last_catch_stm_;

    /* my 'finally' clause, if any */
    class CTPNStmFinally *finally_stm_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'catch' clause
 */
class CTPNStmCatchBase: public CTPNStm
{
public:
    CTPNStmCatchBase()
    {
        /* we have no statement yet */
        body_ = 0;

        /* we don't know our exception class yet */
        exc_class_ = 0;
        exc_class_len_ = 0;

        /* we don't know our exception variable yet */
        exc_var_ = 0;

        /* we're not in a list yet */
        next_catch_ = 0;

        /* we don't know our local symbol table yet */
        symtab_ = 0;
    }

    /* set my enclosed statement */
    void set_body(class CTPNStm *body) { body_ = body; }

    /* set my exception class name */
    void set_exc_class(const class CTcToken *tok);

    /* set my exception variable */
    void set_exc_var(class CTcSymLocal *var) { exc_var_ = var; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the clause */
    virtual unsigned long get_control_flow(int warn) const;

    /* get/set the next 'catch' clause in our list */
    CTPNStmCatch *get_next_catch() const { return next_catch_; }
    void set_next_catch(CTPNStmCatch *nxt) { next_catch_ = nxt; }

    /* set my local-scope symbol table */
    void set_symtab(class CTcPrsSymtab *symtab) { symtab_ = symtab; }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        body_ = (CTPNStm *)body_->adjust_for_dyn(info);
        return this;
    }

protected:
    /* the body of the 'catch' block */
    class CTPNStm *body_;

    /* exception name */
    const textchar_t *exc_class_;
    size_t exc_class_len_;

    /* exception variable (formal parameter to 'catch' block) */
    class CTcSymLocal *exc_var_;

    /* next 'catch' clause in our list */
    CTPNStmCatch *next_catch_;

    /* my local-scope symbol table (for my formal parameter) */
    class CTcPrsSymtab *symtab_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'finally' clause
 */
class CTPNStmFinallyBase: public CTPNStmEnclosing
{
public:
    CTPNStmFinallyBase(CTPNStmEnclosing *enclosing,
                       int exc_local_id, int jsr_local_id)
        : CTPNStmEnclosing(enclosing)
    {
        /* we have no body yet */
        body_ = 0;

        /* remember our local variable ID's */
        exc_local_id_ = exc_local_id;
        jsr_local_id_ = jsr_local_id;

        /* use my starting position as the ending position for now */
        end_desc_ = file_;
        end_linenum_ = linenum_;
    }

    /* set my body */
    void set_body(class CTPNStm *stm) { body_ = stm; }

    /* 
     *   set my ending source position (i.e., the position of the closing
     *   brace of the block of code contained within the 'finally' block 
     */
    void set_end_pos(class CTcTokFileDesc *desc, long linenum)
    {
        end_desc_ = desc;
        end_linenum_ = linenum;
    }

    /* get the ending source position information */
    class CTcTokFileDesc *get_end_desc() const { return end_desc_; }
    long get_end_linenum() const { return end_linenum_; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* evaluate control flow for the clause */
    virtual unsigned long get_control_flow(int warn) const;

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        body_ = (CTPNStm *)body_->adjust_for_dyn(info);
        return this;
    }

protected:
    /* my body */
    class CTPNStm *body_;

    /* 
     *   our exception local ID - this is an unnamed local variable that
     *   we use to store our exception parameter temporarily while we run
     *   the 'finally' block 
     */
    int exc_local_id_;

    /* 
     *   our subroutine return address local ID - this is another unnamed
     *   local that we use to store our subroutine return address 
     */
    int jsr_local_id_;

    /* 
     *   source position of end of the 'finally' clause - we keep track of
     *   this location because code generated after the return from
     *   calling the 'finally' clause is most sensibly thought of as being
     *   at this location 
     */
    class CTcTokFileDesc *end_desc_;
    long end_linenum_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'throw' statement
 */
class CTPNStmThrowBase: public CTPNStm
{
public:
    CTPNStmThrowBase(class CTcPrsNode *expr)
    {
        /* remember the expression to be thrown */
        expr_ = expr;
    }
    
    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);
    
    /* evaluate control flow for the statement */
    virtual unsigned long get_control_flow(int /*warn*/) const
    {
        /* this statement always exits by 'throw' (obviously) */
        return TCPRS_FLOW_THROW;
    }
    
    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        expr_ = expr_->adjust_for_dyn(info);
        return this;
    }

protected:
    /* expression to throw */
    class CTcPrsNode *expr_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'dictionary' statement 
 */
class CTPNStmDictBase: public CTPNStmTop
{
public:
    CTPNStmDictBase(class CTcDictEntry *dict)
    {
        /* remember the dictionary entry */
        dict_ = dict;
    }

    /* fold constants - this has no effect on us */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *) { return this; }

protected:
    /* the underlying dictionary entry */
    class CTcDictEntry *dict_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Parse node for object superclass list entries 
 */
class CTPNSuperclass: public CTcPrsNode
{
public:
    CTPNSuperclass(const textchar_t *sym, size_t len)
    {
        /* remember my name */
        sym_txt_ = sym;
        sym_len_ = len;

        /* we have a name instead of a symbol object */
        sym_ = 0;

        /* I'm not in a list yet */
        nxt_ = prv_ = 0;
    }

    CTPNSuperclass(class CTcSymbol *sym)
    {
        /* remember my symbol */
        sym_ = sym;

        /* we have the symbol, so we don't need to store the name */
        sym_txt_ = 0;
        sym_len_ = 0;

        /* I'm not in a list yet */
        nxt_ = 0;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
        { return this; }

    /* generate code for the compound statement */
    virtual void gen_code(int discard, int for_condition) { }

    /* get my symbol */
    class CTcSymbol *get_sym() const;

    /* get my text - valid only if we are storing text, not a symbol */
    const char *get_sym_txt() const { return sym_txt_; }
    size_t get_sym_len() const { return sym_len_; }

    /* am I a subclass of the given class? */
    int is_subclass_of(const CTPNSuperclass *sc) const;

    /* next/previous entry in my list */
    CTPNSuperclass *nxt_, *prv_;

protected:
    /* my symbol - if this isn't set, we'll use sym_txt_ instead */
    class CTcSymbol *sym_;

    /* my name - we use this if sym_ is not set  */
    const textchar_t *sym_txt_;
    size_t sym_len_;
};

/*
 *   Superclass list 
 */
class CTPNSuperclassList
{
public:
    CTPNSuperclassList() { head_ = tail_ = 0; dst_ = &head_; }

    void append(class CTPNSuperclass *sc)
    {
        /* set the 'next' pointer for the current tail */
        *dst_ = sc;
        *(dst_ = &sc->nxt_) = 0;

        /* set the 'previous' pointer for the new entry */
        sc->prv_ = tail_;
        tail_ = sc;
    }

    /* head and tail of our list */
    class CTPNSuperclass *head_;
    class CTPNSuperclass *tail_;

private:
    class CTPNSuperclass **dst_;
};


/*
 *   Property list 
 */
class CTPNPropList
{
public:
    CTPNPropList() { first_ = last_ = 0; dst_ = &first_; cnt_ = 0; }

    /* append a property to our list */
    void append(class CTPNObjProp *prop);

    /* delete a property from our list; returns true if found, false if not */
    int del(class CTcSymProp *sym);

    /* head/tail of our property list */
    class CTPNObjProp *first_;
    class CTPNObjProp *last_;

    /* number of properties in the list */
    int cnt_;

private:
    /* next destination for append() */
    class CTPNObjProp **dst_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Mix-in base for an object definer node.  This is instantiated in the
 *   top-level object statement class (CTPNStmObject) and the in-line object
 *   definition expression node (CTPNInlineObject). 
 */
class CTPNObjDef
{
public:
    CTPNObjDef()
    { 
        /* presume there's no object symbol */
        obj_sym_ = 0;

        /* presume we won't have an undescribed superclass */
        undesc_sc_ = FALSE;

        /* presume we won't have a template usage error */
        bad_template_ = FALSE;
    }

    /* is this a regular object or an inline object? */
    virtual int is_inline_object() const = 0;

    /* set the 'undescribed class' flag */
    int has_undesc_sc() const { return undesc_sc_; }
    void set_undesc_sc(int f) { undesc_sc_ = f; }

    /* get/set the 'bad template' flag */
    int has_bad_template() const { return bad_template_; }
    void note_bad_template(int f) { bad_template_ = f; }

    /* add a property value */
    class CTPNObjProp *add_prop(
        class CTcSymProp *prop_sym, class CTcPrsNode *expr,
        int replace, int is_static);

    /* 
     *   Add a method.  'expr' is the original expression for cases where the
     *   code body is derived from a simple expression in the source code
     *   rather than an explicit code block.  Keeping the original expression
     *   allows us to use a simple constant value if the expression ends up
     *   folding to a constant. 
     */
    class CTPNObjProp *add_method(
        class CTcSymProp *prop_sym, class CTPNCodeBody *code_body,
        class CTcPrsNode *expr, int replace);

    /* 
     *   Add a method to an inline object.  The method in this case is
     *   represented as an anonymous method object.  'expr' is the original
     *   expression for cases where the anonymous function is derived from a
     *   simple expression in the source code rather than an explicit code
     *   block. 
     */
    class CTPNObjProp *add_inline_method(
        class CTcSymProp *prop_sym, class CTPNAnonFunc *inline_method,
        class CTcPrsNode *expr, int replace);

    /* add a property as a nested object value */
    virtual int parse_nested_obj_prop(
        class CTPNObjProp* &new_prop, int *err,
        struct tcprs_term_info *term_info,
        const class CTcToken *prop_tok, int replace) = 0;

    /* delete a property */
    virtual void delete_property(class CTcSymProp *prop_sym)
        { proplist_.del(prop_sym); }

    /* get my superclass list */
    CTPNSuperclassList &get_superclass_list() { return sclist_; }

    /* get my first superclass */
    class CTPNSuperclass *get_first_sc() const { return sclist_.head_; }

    /* get my first property */
    class CTPNObjProp *get_first_prop() const { return proplist_.first_; }

    /* get the object symbol */
    class CTcSymObj *get_obj_sym() const { return obj_sym_; }

    /* 
     *   set my object symbol - when an object is modified (via the 'modify'
     *   statement), the object tree can be moved to a new symbol 
     */
    void set_obj_sym(class CTcSymObj *obj_sym) { obj_sym_ = obj_sym; }

    /* fold constants in the property list */
    void fold_proplist(class CTcPrsSymtab *symtab);

protected:
    /* add an entry to my property list */
    virtual void add_prop_entry(class CTPNObjProp *prop, int /*replace*/)
        { proplist_.append(prop); }

    /* object name symbol */
    class CTcSymObj *obj_sym_;

    /* property list */
    CTPNPropList proplist_;

    /* superclass list */
    CTPNSuperclassList sclist_;

    /* 
     *   Flag: this object definition includes an undescribed superclass.
     *   This indicates that we're based on a class that was explicitly
     *   defined as 'extern', in which case it can't be used as the source of
     *   a template, since we know nothing about the class other than that it
     *   is indeed a class.  
     */
    unsigned int undesc_sc_ : 1;

    /* flag: this object definition used a template that wasn't matched */
    unsigned int bad_template_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   object definition statement base class
 */
class CTPNStmObjectBase: public CTPNStmTop, public CTPNObjDef
{
public:
    CTPNStmObjectBase(class CTcSymObj *obj_sym, int is_class)
    {
        /* remember our defining global symbol */
        obj_sym_ = obj_sym;

        /* we're not yet replaced by another object */
        replaced_ = FALSE;

        /* we're not yet modified */
        modified_ = FALSE;

        /* note whether I'm a class or an ordinary object instance */
        is_class_ = is_class;

        /* presume it's not transient */
        transient_ = FALSE;
    }

    /* this is a regular top-level object definition */
    virtual int is_inline_object() const { return FALSE; }

    /* 
     *   mark the object as replaced - this indicates that another object
     *   in the same translation unit has replaced this object, hence we
     *   should not generate any code for this object 
     */
    void set_replaced(int f) { replaced_ = f; }

    /*
     *   Mark the object as modified - this indicates that another object
     *   in the same translation unit has modified this object.  We'll
     *   store this information in the object stream header data for use
     *   at link time.  
     */
    void set_modified(int f) { modified_ = f; }

    /* add a superclass with the given name or symbol */
    void add_superclass(const class CTcToken *tok);
    void add_superclass(class CTcSymbol *sym);

    /* determine if I'm a class object */
    int is_class() const { return (is_class_ != 0); }

    /*
     *   Delete a property value.  This is used when a 'modify' object
     *   defines a property with 'replace', so that the property defined in
     *   the modified original object is removed entirely rather than left in
     *   as an inherited property.  
     */
    virtual void delete_property(class CTcSymProp *prop_sym);

    /*
     *   Add an implicit constructor.  This should be called just before
     *   code generation; we'll check to see if the object requires an
     *   implicit constructor, and add one to the object if so.  An object
     *   provides an implicit constructor if it has multiple superclasses
     *   and no explicit constructor.  
     */
    void add_implicit_constructor();

    /* get/set the 'transient' status */
    int is_transient() const { return transient_; }
    void set_transient() { transient_ = TRUE; }

    /* add a property as a nested object value */
    virtual int parse_nested_obj_prop(
        class CTPNObjProp* &new_prop, int *err,
        struct tcprs_term_info *term_info,
        const class CTcToken *prop_tok, int replace);

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        CTPNObjDef::fold_proplist(symtab);
        return this;
    }
    
protected:
    /* add an entry to my property list */
    virtual void add_prop_entry(class CTPNObjProp *prop, int replace);

    /* flag: I'm a class */
    unsigned int is_class_ : 1;

    /* flag: I've been replaced by another object */
    unsigned int replaced_ : 1;

    /* flag: I've been modified by another object */
    unsigned int modified_ : 1;

    /* flag: the object is transient */
    unsigned int transient_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   In-line object definition.  This represents an object defined in-line as
 *   part of an expression.  This type of object definition creates a new
 *   instance of the specified object when the expression is evaluated, using
 *   anonymous methods for code properties.  (Anonymous methods are the same
 *   as anonymous functions, aka closures, except that they bind only to the
 *   local variables in the lexically enclosing frame but not to the 'self'
 *   context.)  Using anonymous methods allows methods defined in the object
 *   to read and write the lexically enclosing frame's local variables, so
 *   that the entire object acts like a closure.
 */
class CTPNInlineObjectBase: public CTcPrsNode, public CTPNObjDef
{
public:
    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        CTPNObjDef::fold_proplist(symtab);
        return this;
    }

    /* this is an inline object definition */
    virtual int is_inline_object() const { return TRUE; }

    /* parse a nested object property */
    virtual int parse_nested_obj_prop(
        class CTPNObjProp* &new_prop, int *err,
        struct tcprs_term_info *term_info,
        const class CTcToken *prop_tok, int replace);

    /* adjust for dynamic execution */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);
};

/* ------------------------------------------------------------------------ */
/*
 *   Parse node entry for a property value or method entry in an object
 *   definition statement's property list.  We can have either an
 *   expression entry or a code body entry; we keep open the possibility
 *   of either one in this single class (rather than subclassing for the
 *   two possibilities) because we might have to transform from an
 *   expression to a code body if we find a non-constant expression, which
 *   we can't determine until constant-folding time.  
 */
class CTPNObjPropBase: public CTPNStm
{
    friend class CTPNObjDef;
    friend class CTPNStmObjectBase;
    friend class CTPNStmObject;
    friend class CTPNPropList;
    
public:
    CTPNObjPropBase(class CTPNObjDef *objdef, class CTcSymProp *prop_sym,
                    class CTcPrsNode *expr, class CTPNCodeBody *code_body,
                    class CTPNAnonFunc *inline_method, int is_static)
    {
        /* remember the object and property information */
        objdef_ = objdef;
        prop_sym_ = prop_sym;

        /* not in a property list yet */
        nxt_ = 0;

        /* remember our expression and code body values */
        expr_ = expr;
        code_body_ = code_body;
        inline_method_ = inline_method;

        /* remember if it's static */
        is_static_ = is_static;

        /* by default, properties cannot be overwritten with redefinitions */
        is_overwritable_ = FALSE;
    }

    /* get my property symbol */
    class CTcSymProp *get_prop_sym() const { return prop_sym_; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab);

    /* adjust for dynamic execution */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);

    /* get the next property of this object */
    class CTPNObjProp *get_next_prop() const { return nxt_; }

    /* am I a constructor? */
    int is_constructor() const;

    /* 
     *   Set/get flag indicating that we're overwritable.  An overwritable
     *   property can be superseded with a duplicate definition of the same
     *   property in the same object.  By default, it's an error to define
     *   the same property more than once in the same object, but properties
     *   defined with certain implicit notation can be overwritten with an
     *   explicit redefinition.  Specifically, a 'location' property added
     *   with the '+' notation can be overwritten, and the automatic
     *   'sourceTextOrder' and 'sourceTextGroup' properties can be
     *   overwritten.  
     */
    int is_overwritable() const { return is_overwritable_; }
    void set_overwritable() { is_overwritable_ = TRUE; }

protected:
    /* my object statement */
    class CTPNObjDef *objdef_;

    /* my property symbol */
    class CTcSymProp *prop_sym_;

    /* next property in my list */
    class CTPNObjProp *nxt_;

    /* my value expression */
    class CTcPrsNode *expr_;

    /* symbol table in effect for this expression */
    class CTcPrsSymtab *symtab_;

    /* my code body node */
    class CTPNCodeBody *code_body_;

    /* for in-line objects, code is represented as an anonymous method */
    class CTPNAnonFunc *inline_method_;

    /* am I static? */
    unsigned int is_static_ : 1;

    /* am I overwritable? */
    unsigned int is_overwritable_ : 1;
};

/*
 *   Deleted property object.  This is used for an entry in a deleted
 *   property list for a 'modify' object.  
 */
class CTcObjPropDel: public CTcPrsAllocObj
{
public:
    CTcObjPropDel(class CTcSymProp *prop_sym)
    {
        /* save the property information */
        prop_sym_ = prop_sym;

        /* not in a list yet */
        nxt_ = 0;
    }

    /* the property to delete */
    class CTcSymProp *prop_sym_;

    /* next in list */
    CTcObjPropDel *nxt_;
};

/*
 *   Implicit constructor statement.
 */
class CTPNStmImplicitCtorBase: public CTPNStm
{
public:
    CTPNStmImplicitCtorBase(class CTPNStmObject *obj_stm)
    {
        /* remember my object statement */
        obj_stm_ = obj_stm;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *) { return this; }

protected:
    /* my object statement */
    class CTPNStmObject *obj_stm_;
};

#endif /* TCPNDRV_H */

