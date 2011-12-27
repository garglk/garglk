/* Copyright (c) 2009 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tcjsint.h - tads 3 compiler - javascript code generator - intermediate
              parse node classes
Function
  
Notes
  
Modified
  02/17/09 MJRoberts  - Creation
*/

#ifndef TCJSINT_H
#define TCJSINT_H

#include "tcjsbase.h"
#include "tcpnint.h"

/* ------------------------------------------------------------------------ */
/*
 *   Unary Operator Expression node 
 */
class CTPNUnary: public CTPNUnaryBase
{
public:
    CTPNUnary(CTcPrsNode *subexpr)
        : CTPNUnaryBase(subexpr) { }

    /*
     *   Generate code for a unary operator with no side effects.  We'll
     *   generate code for the subexpression, then apply the given javascript
     *   operator to the result.
     *   
     *   If 'discard' is true, we won't generate the opcode.  We know the
     *   opcode has no side effects, so if the result of the calculation
     *   isn't going to be used, there's no point in calculating it in the
     *   first place.  
     */
    void gen_unary(const char *op, int discard, int for_condition);
};


/* ------------------------------------------------------------------------ */
/*
 *   Binary Operator Expression node 
 */
class CTPNBin: public CTPNBinBase
{
public:
    CTPNBin(CTcPrsNode *lhs, CTcPrsNode *rhs)
        : CTPNBinBase(lhs, rhs) { }

    /*
     *   Generate code for a binary operator with no side effects.  We'll
     *   generate code for the left operand, then for the right operand, then
     *   apply the given javascript operator to the pair.
     *   
     *   If 'discard' is true, we won't generate the opcode.  We know that
     *   the opcode has no side effects, so if the result of the calculation
     *   isn't needed, we need not apply the opcode; we simply want to
     *   evaluate the subexpressions for any side effects they might have.  
     */
    void gen_binary(const char *op, int discard, int for_condition);
};

/* ------------------------------------------------------------------------ */
/*
 *   generic statement 
 */
class CTPNStm: public CTPNStmBase
{
public:
    /* initialize at the tokenizer's current source file position */
    CTPNStm() : CTPNStmBase() { }

    /* initialize at the given source position */
    CTPNStm(class CTcTokFileDesc *file, long linenum)
        : CTPNStmBase(file, linenum) { }

    /*
     *   Generate code for a labeled 'continue'.  These are called when this
     *   statement is labeled, and the same label is used in a 'continue'
     *   statement.
     *   
     *   Returns true if successful, false if not.  This should return false
     *   if the statement is not suitable for a 'continue'.  By default, we
     *   simply return false, since most statements cannot be used as the
     *   target of a 'continue'.  
     */
    virtual int gen_code_labeled_continue() { return FALSE; }
};

/* ------------------------------------------------------------------------ */
/*
 *   Enclosing Statement class: this is the base class for statements that
 *   have special needs for exiting due to break, continue, goto, and return.
 *   Refer to tcpnint.h for details.  
 */
class CTPNStmEnclosing: public CTPNStm
{
public:
    CTPNStmEnclosing(CTPNStmEnclosing *enclosing)
    {
        /* remember the statement that encloses me */
        enclosing_ = enclosing;
    }

    /* get the enclosing statement */
    CTPNStmEnclosing *get_enclosing() const { return enclosing_; }

    /* 
     *   Generate code for a 'break' to a given label, or an unlabeled
     *   'break' if lbl is null.  Returns true if the break was fully
     *   processed, false if not.  If any work is necessary to unwind the
     *   exception stack to break out of this block, that should be done
     *   here.  If this node doesn't actually accomplish the break, it should
     *   recursively invoke the next enclosing statement's routine to do the
     *   work.
     *   
     *   By default, we'll simply invoke the enclosing handler, if there is
     *   one, or return failure (i.e., false) if not.  
     */
    virtual int gen_code_break(const textchar_t *lbl, size_t lbl_len)
    {
        if (enclosing_)
            return enclosing_->gen_code_break(lbl, lbl_len);
        else
            return 0;
    }

    /*
     *   Generate code for a 'continue' to a given label, or an unlabeled
     *   'continue' if lbl is null.  Same rules as gen_code_break().  
     *   
     *   By default, we'll simply invoke the enclosing handler, if there is
     *   one, or return failure (i.e., false) if not.  
     */
    virtual int gen_code_continue(const textchar_t *lbl, size_t lbl_len)
    {
        if (enclosing_)
            return enclosing_->gen_code_continue(lbl, lbl_len);
        else
            return 0;
    }

    /*
     *   Generate the code necessary to unwind the stack for returning
     *   through this enclosing statement.  This should generate any code
     *   necessary (such as a call to a 'finally' block, for a 'try'
     *   statement), then call the next enclosing statement to do the same
     *   thing.  By default, this does nothing except invoke the enclosing
     *   statement, since most enclosing statements don't need to generate
     *   any code to handle a 'return' through them.  
     */
    virtual void gen_code_unwind_for_return()
    {
        /* if there's an enclosing statement, ask it to unwind itself */
        if (enclosing_ != 0)
            enclosing_->gen_code_unwind_for_return();
    }

    /*
     *   Determine if we'll generate any code to unwind the stack for
     *   returning through this enclosing statement.  This should return true
     *   if any code will be generated by a call to
     *   gen_code_unwind_for_return().  By default, we'll return whatever our
     *   enclosing statement does.  
     */
    virtual int will_gen_code_unwind_for_return() const
    {
        /* 
         *   return what the enclosing statement does, or false if we're the
         *   outermost statement 
         */
        return (enclosing_ != 0
                ? enclosing_->will_gen_code_unwind_for_return()
                : FALSE);
    }

    /*
     *   Generate the code necessary to unwind the stack for executing a
     *   'goto' to the given label statement.  We first determine if the
     *   target label is contained within this statement; if it is, there's
     *   nothing we need to do, because we're transferring control within the
     *   same enclosing statement.
     *   
     *   If control is being transferred outside of this statement (i.e., the
     *   label is not contained within this statement), we must generate the
     *   necessary code to leave the block; for example, a 'try' block must
     *   generate a call to its 'finally' block.  We must then invoke this
     *   same function on our enclosing statement to let it do the same work.
     */
    void gen_code_unwind_for_goto(class CTPNStmGoto *goto_stm,
                                  class CTPNStmLabel *target);

    /* 
     *   Generate code for transferring control out of this statement.  We
     *   invoke this from gen_code_unwind_for_goto() when we determine that
     *   the 'goto' target is not enclosed within this statement.  By default
     *   we do nothing; statements such as 'try' that must do work on
     *   transferring control must override this to generate the appropriate
     *   code.  
     */
    virtual void gen_code_for_transfer_out() { }

    /*
     *   Check to see if we are allowed to transfer in to this block via
     *   'goto' statements.  If so, simply return TRUE.  If we don't allow
     *   this type of transfer, we should log an appropriate error, then
     *   return FALSE.  Note that the function should both log an error and
     *   return FALSE if the transfer isn't allowed; the return value allows
     *   the caller to ensure that only one error is displayed for this type
     *   of problem even if the target label is nested within several levels
     *   of blocks that don't allow transfers in.
     *   
     *   By default, we'll simply return TRUE, since most block types allow
     *   this type of transfer.  
     */
    virtual int check_enter_by_goto(class CTPNStmGoto * /*goto_stm*/,
                                    class CTPNStmLabel * /*target*/)
        { return TRUE; }

protected:
    /* 
     *   generate code for a break - this can be used as a service routine
     *   by loop/switch statements 
     */
    int gen_code_break_loop(CTcCodeLabel *code_label,
                            const textchar_t *lbl, size_t lbl_len);
    
    /* 
     *   generate code for a continue - this can be used as a service
     *   routine by loop/switch statements 
     */
    int gen_code_continue_loop(CTcCodeLabel *code_label,
                               const textchar_t *lbl, size_t lbl_len);
    
  
    /* the block enclosing this block */
    CTPNStmEnclosing *enclosing_;
};

#endif /* TCJSINT_H */
