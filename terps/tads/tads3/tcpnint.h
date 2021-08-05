/* $Header: d:/cvsroot/tads/tads3/TCPNINT.H,v 1.3 1999/07/11 00:46:53 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcpnint.h - intermediate derived parse node classes
Function
  
Notes
  
Modified
  05/12/99 MJRoberts  - Creation
*/

#ifndef TCPNINT_H
#define TCPNINT_H



/* ------------------------------------------------------------------------ */
/*
 *   Generic unary parse node.  Most unary node types don't differ at the
 *   parser level, so target node types can be subclassed directly off of
 *   this generic unary node class.  
 */
class CTPNUnaryBase: public CTcPrsNode
{
public:
    CTPNUnaryBase(class CTcPrsNode *sub)
    {
        sub_ = sub;
    }

    /* get the subexpression */
    class CTcPrsNode *get_sub_expr() const { return sub_; }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        class CTcPrsNode *ret;
        
        /* fold constants on my subexpression */
        sub_ = sub_->fold_constants(symtab);

        /* try folding the unary operator */
        ret = fold_unop();

        /* 
         *   if we folded the operator, return the result; otherwise,
         *   return the original operator tree for full code generation 
         */
        return (ret != 0 ? ret : this);
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        /* 
         *   if this expression has side effects, don't allow it to be
         *   evaluated speculatively 
         */
        if (info->speculative && has_side_effects())
            err_throw(VMERR_BAD_SPEC_EVAL);
        
        /* adjust my subexpression */
        sub_ = sub_->adjust_for_dyn(info);

        /* return myself otherwise unchanged */
        return this;
    }

protected:
    /* fold constants for the operator */
    virtual class CTcPrsNode *fold_unop() { return 0; }

    /* determine if I have side effects */
    virtual int has_side_effects() const { return FALSE; }

    /* our subexpression */
    class CTcPrsNode *sub_;
};

/*
 *   Define a constructor for a CTPNUnary target subclass.  Target
 *   subclasses can use this macro to define the standard CTPNUnary
 *   subclass constructor - this is simply for conciseness in those
 *   subclasses.  
 */
#define CTPNUnary_ctor(scname) \
    scname(class CTcPrsNode *sub) \
        : CTPNUnary(sub) { }


/*
 *   define a CTPNUnary target subclass 
 */
#define CTPNUnary_def(scname) \
class scname: public CTPNUnary \
{ \
public: \
    CTPNUnary_ctor(scname); \
    void gen_code(int discard, int for_condition); \
}

/* 
 *   define a CTPNUnary target subclass for a unary operator with side
 *   effects
 */
#define CTPNUnary_side_def(scname) \
class scname: public CTPNUnary \
{ \
public: \
    CTPNUnary_ctor(scname); \
    void gen_code(int discard, int for_condition); \
    virtual int has_side_effects() const { return TRUE; } \
}



/* ------------------------------------------------------------------------ */
/*
 *   Generic binary parse node.  Most binary node types don't differ at
 *   the parser level, so target node types can be subclassed directly off
 *   of this generic binary node class.  
 */
class CTPNBinBase: public CTcPrsNode
{
public:
    CTPNBinBase(class CTcPrsNode *left, class CTcPrsNode *right)
    {
        left_ = left;
        right_ = right;
    }

    /* fold constants */
    class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab)
    {
        CTcPrsNode *ret;
        
        /* fold constants on my subexpressions */
        left_ = left_->fold_constants(symtab);
        right_ = right_->fold_constants(symtab);

        /* try folding this operator, if we can */
        ret = fold_binop();

        /* 
         *   if we got a folded value, return it; otherwise, return
         *   myself, unchanged except any subnode folding we did 
         */
        return (ret != 0 ? ret : this);
    }

    /* adjust for dynamic (run-time) compilation */
    class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info)
    {
        /* if I have side effects, don't allow speculative evaluation */
        if (info->speculative && has_side_effects())
            err_throw(VMERR_BAD_SPEC_EVAL);
        
        /* adjust the left and right subexpressions */
        left_ = left_->adjust_for_dyn(info);
        right_ = right_->adjust_for_dyn(info);

        /* return myself otherwise unchanged */
        return this;
    }

protected:
    /* perform folding on this binary operator, if possible */
    virtual class CTcPrsNode *fold_binop() { return 0; }

    /* determine if I have side effects */
    virtual int has_side_effects() const { return FALSE; }

    /* left and right operands */
    class CTcPrsNode *left_;
    class CTcPrsNode *right_;
};

/*
 *   Define a constructor for a CTPNBin target subclass.  Target
 *   subclasses can use this macro to define the standard CTPNBin subclass
 *   constructor - this is simply for conciseness in those subclasses.  
 */
#define CTPNBin_ctor(scname) \
    scname(class CTcPrsNode *left, class CTcPrsNode *right) \
    : CTPNBin(left, right) { }

/*
 *   Define a full general class interface for a CTPNBin target subclass.
 *   Most target subclasses will not need to add anything of their own, so
 *   they can use this generic class definition macro.  
 */
#define CTPNBin_def(scname) \
class scname: public CTPNBin \
{ \
public: \
    CTPNBin_ctor(scname); \
    void gen_code(int discard, int for_condition); \
}

/*
 *   define a CTPNBin target subclass for a binary comparison operator 
 */
#define CTPNBin_cmp_def(scname) \
class scname: public CTPNBin \
{ \
public: \
    CTPNBin_ctor(scname); \
    void gen_code(int discard, int for_condition); \
    virtual int is_bool() const { return TRUE; } \
}

/* 
 *   define a CTPNBin target subclass for a binary operator with side
 *   effects 
 */
#define CTPNBin_side_def(scname) \
class scname: public CTPNBin \
{ \
public: \
    CTPNBin_ctor(scname); \
    void gen_code(int discard, int for_condition); \
    virtual int has_side_effects() const { return TRUE; } \
}

/* ------------------------------------------------------------------------ */
/*
 *   Generic statement node base. 
 */
class CTPNStmBase: public CTcPrsNode
{
public:
    /* initialize at the tokenizer's current source file position */
    CTPNStmBase();

    /* initialize at the given source position */
    CTPNStmBase(class CTcTokFileDesc *file, long linenum)
    {
        init(file, linenum);
    }

    /* set the file and line number */
    void set_source_pos(class CTcTokFileDesc *file, long linenum)
    {
        /* remember the new line information */
        file_ = file;
        linenum_ = linenum;
    }

    /* get the next statement after this one */
    class CTPNStm *get_next_stm() const { return next_stm_; }

    /* set the next statement after this one */
    void set_next_stm(class CTPNStm *nxt) { next_stm_ = nxt; }

    /* 
     *   log an error, using the source file location of this statement as
     *   the location of the error 
     */
    void log_error(int errnum, ...) const;

    /* 
     *   log a warning, using the source file location of this statement
     *   as the location of the warning 
     */
    void log_warning(int errnum, ...) const;

    /* get my source location information */
    class CTcTokFileDesc *get_source_desc() const { return file_; }
    long get_source_linenum() const { return linenum_; }

    /* 
     *   Get the ending source location - for most statements, this is
     *   identical to the normal source location information.  For certain
     *   complex statements, such as compound statements, this will
     *   provide the source location where the statement's structure ends. 
     */
    virtual class CTcTokFileDesc *get_end_desc() const { return file_; }
    virtual long get_end_linenum() const { return linenum_; }

    /*
     *   Determine the possible control flow routes through this
     *   statement.  Set or clear each flag as appropriate.
     *   
     *.  TCPRS_FLOW_NEXT - flow continues to the next statement
     *.  TCPRS_FLOW_THROW - statement throws an exception
     *.  TCPRS_FLOW_RET_VOID - statement returns no value
     *.  TCPRS_FLOW_RET_VAL - statement returns a value
     *.  TCPRS_FLOW_GOTO - control transfers to a code label
     *.  TCPRS_FLOW_BREAK - control breaks out of the current loop/case
     *.  TCPRS_FLOW_CONT - control goes to the top of the current loop
     *   
     *   Most statements simply proceed to the next statement in all
     *   cases, so the default implementation simply sets the 'cont' flag
     *   and clears the others.
     *   
     *   This routine is not meant to evaluate dependencies on called
     *   functions or methods.  Functions and methods that are called can
     *   be assumed to return, with or without a value as needed for the
     *   usage here.  The possibility of VM exceptions due to run-time
     *   error conditions can be ignored; we're only interested in
     *   statements that explicitly throw exceptions.  
     */

#define TCPRS_FLOW_NEXT       0x00000001
#define TCPRS_FLOW_THROW      0x00000002
#define TCPRS_FLOW_RET_VOID   0x00000004
#define TCPRS_FLOW_RET_VAL    0x00000008
#define TCPRS_FLOW_GOTO       0x00000010
#define TCPRS_FLOW_BREAK      0x00000020
#define TCPRS_FLOW_CONT       0x00000040

    virtual unsigned long get_control_flow(int /*warn*/) const
    {
        /* by default, a statement continues to the next statement */
        return TCPRS_FLOW_NEXT;
    }

    /*
     *   Determine if this statement has a code label.  By default, we
     *   return false.  
     */
    virtual int has_code_label() const { return FALSE; }

    /* a statement has no return value */
    virtual int has_return_value() const { return FALSE; }

    /* add a debugging line record for this statement */
    void add_debug_line_rec();

protected:
    /* 
     *   Generate code for a sub-statement (such as the 'then' part of an
     *   'if', or a statement in a compount statement).  This sets up the
     *   error reporting location to refer to the sub-statement, then
     *   generates code for the sub-statement normally.  
     */
    void gen_code_substm(class CTPNStm *substm);

    /* add a debugging line record at the given position */
    void add_debug_line_rec(class CTcTokFileDesc *desc, long linenum);

    /* initialize */
    void init(class CTcTokFileDesc *file, long linenum)
    {
        /* remember the file and line number containing the code */
        file_ = file;
        linenum_ = linenum;

        /* there's not another statement after this one yet */
        next_stm_ = 0;
    }
    
    /* next statement in execution order */
    class CTPNStm *next_stm_;

    /* file and line number where this statement's source code appears */
    class CTcTokFileDesc *file_;
    long linenum_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Enclosing Statement Block object.  This is a base class for
 *   statements such as "try" blocks and code labels for which we must
 *   keep a nesting list.  A statement of this class has special needs for
 *   exiting, via break, continue, goto, or return, the block of code that
 *   the statement encloses
 *   
 *   NOTE: To avoid having to introduce yet another level of intermediate
 *   #include file in our target-generic/target-specific layering, we
 *   require that the target-specific intermediate include file define the
 *   base class CTPNStmEnclosing for us.  It must be defined as below,
 *   with the addition of the target-specific definitions.  Sorry about
 *   any confusion this causes, but inserting a fourth include layer is
 *   just going too far.  
 */
// class CTPNStmEnclosing: public CTPNStm
// {
// public:
//     CTPNStmEnclosing(CTPNStmEnclosing *enclosing)
//     {
//         /* remember the statement that encloses me */
//         enclosing_ = enclosing;
//     }
//
// protected:
//     /* the block enclosing this block */
//     CTPNStmEnclosing *enclosing_;
// };


/* ------------------------------------------------------------------------ */
/*
 *   Base class for 'for' statement 'var in collection' and 'var in from..to'
 *   expressions.  We keep a separate list of these nodes for a 'for'
 *   statement, for code generation purposes.  These expression nodes reside
 *   in the initializer clause of the 'for', but they implicitly generate
 *   code in the condition and reinit phases as well.  
 */
class CTPNForInBase: public CTcPrsNode
{
public:
    CTPNForInBase()
    {
        nxt_ = 0;
    }

    /* get/set the next list entry */
    class CTPNForIn *getnxt() const { return nxt_; }
    void setnxt(class CTPNForIn *nxt) { nxt_ = nxt; }

protected:
    /* next in list of 'in' clauses for this 'for' */
    class CTPNForIn *nxt_;
};

#endif /* TCPNINT_H */

