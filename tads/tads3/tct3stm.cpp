#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCT3STM.CPP,v 1.1 1999/07/11 00:46:57 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tct3stm.cpp - TADS 3 Compiler - T3 VM Code Generator - statement classes
Function
  Generate code for the T3 VM.  This file contains statement classes,
  in order to segregate the code generation classes required for the
  full compiler from those required for subsets that require only
  expression parsing (such as debuggers).
Notes
  
Modified
  05/08/99 MJRoberts  - Creation
*/

#include <stdio.h>

#include "t3std.h"
#include "os.h"
#include "tcprs.h"
#include "tct3.h"
#include "tcgen.h"
#include "vmtype.h"
#include "vmwrtimg.h"
#include "vmfile.h"
#include "tcmain.h"
#include "tcerr.h"


/* ------------------------------------------------------------------------ */
/*
 *   'if' statement
 */

/*
 *   generate code 
 */
void CTPNStmIf::gen_code(int, int)
{
    /* add a line record */
    add_debug_line_rec();

    /* 
     *   if the condition has a constant value, don't bother generating
     *   code for both branches 
     */
    if (cond_expr_->is_const())
    {
        int val;

        /* determine whether it's true or false */
        val = cond_expr_->get_const_val()->get_val_bool();
        
        /* 
         *   Warn about it if it's always false (in which case the 'then'
         *   code is unreachable); or it's always true and we have an
         *   'else' part (since the 'else' part is unreachable).  Don't
         *   warn if it's true and there's no 'else' part, since this
         *   merely means that there's some redundant source code, but
         *   will have no effect on the generated code.  
         */
        if (!val)
        {
            /* 
             *   It's false - the 'then' part cannot be executed.  If this
             *   isn't a compile-time constant expression, warn about it.
             */
            if (!cond_expr_->get_const_val()->is_ctc())
                log_warning(TCERR_IF_ALWAYS_FALSE);

            /* generate the 'else' part if there is one */
            if (else_part_ != 0)
                gen_code_substm(else_part_);
        }
        else
        {
            /* it's true - the 'else' part cannot be executed */
            if (else_part_ != 0)
                log_warning(TCERR_IF_ALWAYS_TRUE);

            /* generate the 'then' part */
            if (then_part_ != 0)
                gen_code_substm(then_part_);
        }

        /* we're done */
        return;
    }

    /*
     *   If both the 'then' and 'else' parts are null statements, we're
     *   evaluating the condition purely for side effects.  Simply
     *   evaluate the condition in this case, since there's no need to so
     *   much as test the condition once evaluated. 
     */
    if (then_part_ == 0 && else_part_ == 0)
    {
        /* generate the condition, discarding the result */
        cond_expr_->gen_code(TRUE, TRUE);

        /* we're done */
        return;
    }

    /* 
     *   The condition is non-constant, and we have at least one subclause,
     *   so we must evaluate the condition expression.  To minimize the
     *   amount of jumping, check whether we have a true part, else part, or
     *   both, and generate the branching accordingly.  
     */
    if (then_part_ != 0)
    {
        CTcCodeLabel *lbl_else;
        CTcCodeLabel *lbl_end;

        /*
         *   We have a true part, so we will want to evaluate the expression
         *   and jump past the true part if the expression is false.  Create
         *   a label for the false branch.  
         */
        lbl_else = G_cs->new_label_fwd();

        /* generate the condition expression */
        cond_expr_->gen_code_cond(0, lbl_else);

        /* generate the 'then' part */
        gen_code_substm(then_part_);

        /* if there's an 'else' part, generate it */
        if (else_part_ != 0)
        {
            /* at the end of the 'then' part, jump past the 'else' part */
            lbl_end = gen_jump_ahead(OPC_JMP);

            /* this is the start of the 'else' part */
            def_label_pos(lbl_else);

            /* generate the 'else' part */
            gen_code_substm(else_part_);

            /* set the label for the jump over the 'else' part */
            def_label_pos(lbl_end);
        }
        else
        {
            /* 
             *   there's no 'else' part - set the label for the jump past the
             *   'then' part 
             */
            def_label_pos(lbl_else);
        }
    }
    else
    {
        CTcCodeLabel *lbl_end;

        /* 
         *   There's no 'then' part, so there must be an 'else' part (we
         *   wouldn't have gotten this far if both 'then' and 'else' are
         *   empty).  To minimize branching, evaluate the condition and jump
         *   past the 'else' part if the condition is true, falling through
         *   to the 'else' part otherwise.  Create a label for the end of the
         *   statement, which is also the empty 'then' part.  
         */
        lbl_end = G_cs->new_label_fwd();

        /* evaluate the condition and jump to the end if it's true */
        cond_expr_->gen_code_cond(lbl_end, 0);

        /* generate the 'else' part */
        gen_code_substm(else_part_);

        /* set the label for the jump over the 'else' part */
        def_label_pos(lbl_end);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   'for' statement 
 */

/*
 *   generate code 
 */
void CTPNStmFor::gen_code(int, int)
{
    CTcCodeLabel *top_lbl;
    CTcCodeLabel *end_lbl;
    CTcCodeLabel *cont_lbl;
    CTPNStmEnclosing *old_enclosing;
    CTcPrsSymtab *old_frame;

    /* set my local frame if necessary */
    old_frame = G_cs->set_local_frame(symtab_);

    /* 
     *   add a line record - note that we add the line record after
     *   setting up the local frame, so that the 'for' statement itself
     *   appears within its own inner scope 
     */
    add_debug_line_rec();

    /* push the enclosing statement */
    old_enclosing = G_cs->set_enclosing(this);
    
    /* if there's an initializer expression, generate it */
    if (init_expr_ != 0)
        init_expr_->gen_code(TRUE, TRUE);

    /* set the label for the top of the loop */
    top_lbl = new_label_here();

    /* allocate a forward label for 'continue' jumps */
    cont_lbl = G_cs->new_label_fwd();

    /* allocate a forward label for the end of the loop */
    end_lbl = G_cs->new_label_fwd();

    /* generate the implicit conditions for any 'in' clauses */
    for (CTPNForIn *i = in_exprs_ ; i != 0 ; i = i->getnxt())
        i->gen_forstm_cond(end_lbl);

    /* 
     *   If there's an explicit condition clause, generate its code, jumping
     *   to the end of the loop if the condition is false.  
     */
    if (cond_expr_ != 0)
        cond_expr_->gen_code_cond(0, end_lbl);

    /* 
     *   set our labels, so that 'break' and 'continue' statements in our
     *   body will know where to go 
     */
    break_lbl_ = end_lbl;
    cont_lbl_ = cont_lbl;

    /* if we have a body, generate it */
    if (body_stm_ != 0)
        gen_code_substm(body_stm_);

    /* 
     *   add another line record - we're now generating code again for the
     *   original 'for' line, even though it's after the body 
     */
//$$$    add_debug_line_rec();

    /* this is where we come for 'continue' statements */
    def_label_pos(cont_lbl);

    /* generate the reinitializer expression, if we have one */
    if (reinit_expr_ != 0)
        reinit_expr_->gen_code(TRUE, TRUE);

    /* generate the implicit reinitializers for any 'in' clauses */
    for (CTPNForIn *i = in_exprs_ ; i != 0 ; i = i->getnxt())
        i->gen_forstm_reinit();

    /* jump back to the top of the loop */
    G_cg->write_op(OPC_JMP);
    G_cs->write_ofs2(top_lbl, 0);

    /* 
     *   we're at the end of the loop - this is where we jump for 'break'
     *   and when the condition becomes false 
     */
    def_label_pos(end_lbl);

    /* restore the enclosing statement */
    G_cs->set_enclosing(old_enclosing);

    /* restore the enclosing local scope */
    G_cs->set_local_frame(old_frame);
}

/* ------------------------------------------------------------------------ */
/*
 *   'for var in expr' node 
 */

/*
 *   Generate code.  This is called during the initializer phase of the 'for'
 *   that we're part of.  We create the collection enumerator and assign the
 *   first value from the enumerator to the lvalue.  
 */
void CTPNVarIn::gen_code(int, int)
{
    /* generate the iterator initializer */
    gen_iter_init(expr_, iter_local_id_, "for");
}

/*
 *   Static method: generate the iterator initializer 
 */
void CTPNVarIn::gen_iter_init(CTcPrsNode *coll_expr, int iter_local_id,
                              const char *kw)
{
    /* 
     *   Look up the createIterator property of the Collection metaclass.
     *   This property is defined by the Collection specification as the
     *   property in the first (zeroeth) slot in the method table for
     *   Collection.  If Collection isn't defined, or this slot isn't
     *   defined, it's an error.  
     */
    CTcPrsNode *create_iter = G_cg->get_metaclass_prop("collection", 0);

    /* if we didn't find the property, it's an error */
    if (create_iter != VM_INVALID_PROP)
    {
        /* 
         *   generate a call to the createIterator() property on the
         *   collection expression 
         */
        if (coll_expr != 0)
            coll_expr->gen_code_member(FALSE, create_iter, FALSE, 0, FALSE, 0);

        /* assign the result to the internal iterator stack local */
        CTcSymLocal::s_gen_code_setlcl_stk(iter_local_id, FALSE);
    }
    else
    {
        /* tell them about the problem */
        G_tok->log_error(TCERR_FOREACH_NO_CREATEITER, kw);
    }
}

/* 
 *   Generate code for the condition portion of the 'for'.  We check to see
 *   if there's a next value: if so we fetch it into our lvalue, it not we
 *   break the loop.  
 */
void CTPNVarIn::gen_forstm_cond(CTcCodeLabel *endlbl)
{
    /* generate the code */
    gen_iter_cond(lval_, iter_local_id_, endlbl, "for");
}

/*
 *   Generate code for the condition portion of a for..in or a foreach..in.
 */
void CTPNVarIn::gen_iter_cond(CTcPrsNode *lval, int iter_local_id,
                              CTcCodeLabel *&end_lbl, const char *kw)
{
#if 1
    /* if the caller didn't provide an end-of-loop label, create one */
    if (end_lbl == 0)
        end_lbl = G_cs->new_label_fwd();

    /* 
     *   Generate the ITERNEXT <iterator local>, <end offset>.  This pushes
     *   the next iterator value if available, otherwise jumps to the end
     *   label (without pushing anything onto the stack).  
     */
    G_cg->write_op(OPC_ITERNEXT);
    G_cs->write2(iter_local_id);
    G_cs->write_ofs2(end_lbl, 0);

    /* we're on the looping branch - ITERNEXT pushed the next value */
    G_cg->note_push();

    /* assign the next value to the loop control lvalue */
    if (lval != 0)
        lval->gen_code_asi(TRUE, 1, TC_ASI_SIMPLE, "=", 0, FALSE, TRUE, 0);
    else
    {
        G_cg->write_op(OPC_DISC);
        G_cg->note_pop();
    }
    
#else
    /* 
     *   Look up Iterator.getNext() (iterator metaclass method index 0) and
     *   Iterator.isNextAvailable() (index 1).
     */
    CTcPrsNode *get_next = G_cg->get_metaclass_prop("iterator", 0);
    CTcPrsNode *next_avail = G_cg->get_metaclass_prop("iterator", 1);

    /* generate the isNextAvailable test */
    if (next_avail != 0)
    {
        /* get the internal iterator local */
        CTcSymLocal::s_gen_code_getlcl(iter_local_id, FALSE);

        /* generate a call to the property */
        CTcPrsNode::s_gen_member_rhs(
            FALSE, next_avail, FALSE, 0, FALSE, FALSE);

        /* 
         *   generate a jump to the end of the loop, creating the label if
         *   the caller didn't already
         */
        if (end_lbl != 0)
        {
            /* jump to the caller's end label */
            G_cg->write_op(OPC_JF);
            G_cs->write_ofs2(end_lbl, 0);
        }
        else
        {
            /* we need a new label */
            end_lbl = gen_jump_ahead(OPC_JF);
        }

        /* the JF pops an element off the stack */
        G_cg->note_pop();
    }
    else
    {
        /* this property is required to be defined - this is an error */
        G_tok->log_error(TCERR_FOREACH_NO_ISNEXTAVAIL, kw);

        /* 
         *   generate an arbitrary 'end' label if the caller didn't already
         *   create one - we're not going to end up generating valid code
         *   anyway, but since we're not going to abort code generation,
         *   it'll avoid problems elsewhere if we have a valid label assigned
         */
        if (end_lbl == 0)
            end_lbl = new_label_here();
    }

    /* generate the code to get the next element of the iteration */
    if (get_next != 0)
    {
        /* get the internal iterator local */
        CTcSymLocal::s_gen_code_getlcl(iter_local_id, FALSE);

        /* generate a call to the property */
        CTcPrsNode::s_gen_member_rhs(FALSE, get_next, FALSE, 0, FALSE, FALSE);

        /* assign the result to the iterator lvalue */
        if (lval != 0)
            lval->gen_code_asi(TRUE, 1, TC_ASI_SIMPLE, "=", 0, FALSE, TRUE, 0);
    }
    else
    {
        /* this property is required to be defined - this is an error */
        G_tok->log_error(TCERR_FOREACH_NO_GETNEXT, kw);
    }
#endif
}

/*
 *   Generate code for the reinit portion of the 'for'.  We do nothing on
 *   this step; we do the test and fetch as one operation in the condition
 *   instead, since the iterator test has to be done ahead of the fetch.  
 */
void CTPNVarIn::gen_forstm_reinit()
{
    /* do nothing on this phase */
}


/* ------------------------------------------------------------------------ */
/*
 *   'for var in from..to' node 
 */

/*
 *   Generate code.  This is called during the initializer pharse of the
 *   'for' that we're part of.  We evaluate the 'from' and 'to' expressions,
 *   assign the 'from' to the lvalue, and save the 'to' in a private local
 *   variable.  
 */
void CTPNVarInRange::gen_code(int, int)
{
    /* assign the 'from' expression to the lvalue */
    lval_->gen_code_asi(TRUE, 1, TC_ASI_SIMPLE, "=", from_expr_,
                        FALSE, TRUE, 0);

    /* 
     *   if the 'to' expression is non-constant, evaluate it and save it in a
     *   local, so that we don't have to re-evaluate it on each iteration 
     */
    if (!to_expr_->is_const())
    {
        /* evaluate the 'to' expression */
        to_expr_->gen_code(FALSE, FALSE);

        /* assign it to our stack local */
        CTcSymLocal::s_gen_code_setlcl_stk(to_local_id_, FALSE);
    }

    /* 
     *   likewise, if there's a non-constant 'step' expression, evaluate it
     *   and store it in our step stack local 
     */
    if (step_expr_ != 0 && !step_expr_->is_const())
    {
        /* generate the step expression value */
        step_expr_->gen_code(FALSE, FALSE);

        /* generate the assignment to our local */
        CTcSymLocal::s_gen_code_setlcl_stk(step_local_id_, FALSE);

        /* replace the step expression with the local */
        step_expr_ = new CTPNSymResolved(new CTcSymLocal(
            ".step", 5, FALSE, FALSE, step_local_id_));
    }
}

/*
 *   Generate code for the condition portion of the 'for'.  We test our
 *   lvalue to see if we've passed the 'to' limit, in which case we break out
 *   of the loop.  
 */
void CTPNVarInRange::gen_forstm_cond(CTcCodeLabel *endlbl)
{
    /* push the lvalue */
    lval_->gen_code(FALSE, FALSE);

    /* 
     *   push the 'to' expression - if it's constant, generate the constant
     *   expression value, otherwise get the local 
     */
    if (to_expr_->is_const())
        to_expr_->gen_code(FALSE, FALSE);
    else
        CTcSymLocal::s_gen_code_getlcl(to_local_id_, FALSE);

    /* 
     *   If the step expression is a constant, compare based on the sign of
     *   the constant expression: if positive, we stop when the lval is
     *   greater than the 'to' limit; if negative, we stop when the lval is
     *   less than to 'to' limit.  If the expression isn't a constant, we
     *   have to test the sign at run-time.  
     */
    int s = 0;
    if (step_expr_ == 0)
    {
        /* no step expression -> default step is 1, sign is positive */
        s = 1;
    }
    else if (step_expr_->is_const())
    {
        /* explicit step expression with a constant value - check the type */
        switch (step_expr_->get_const_val()->get_type())
        {
        case TC_CVT_INT:
            /* integer - get its sign */
            s = step_expr_->get_const_val()->get_val_int() < 0 ? -1 : 1;
            break;

        case TC_CVT_FLOAT:
            /* BigNumber value - check for a '-' */
            s = step_expr_->get_const_val()->get_val_float()[0] == '-'
                ? -1 : 1;
            break;

        default:
            /* others will have to be interpreted at run-time */
            s = 0;
            break;
        }
    }

    /* 
     *   if we have a known sign, generate a fixed test, otherwise generate a
     *   run-time test 
     */
    if (s > 0)
    {
        /* positive constant - we're looping up to the 'to' limit */
        G_cg->write_op(OPC_JGT);
        G_cs->write_ofs2(endlbl, 0);

        /* JGT pops 2 */
        G_cg->note_pop(2);
    }
    else if (s < 0)
    {
        /* negative constant - we're looping down to the 'to' limit */
        G_cg->write_op(OPC_JLT);
        G_cs->write_ofs2(endlbl, 0);

        /* JLT pops 2 */
        G_cg->note_pop(2);
    }
    else
    {
        /* 
         *   Variable step - generate a run-time test:
         *   
         *.   ; test the sign of the step expression: if step < 0 goto $1
         *.      push <step>
         *.      push 0
         *.      jlt $1
         *.   ; positive branch: if lval > to goto endlbl else goto $2
         *.      jgt endlbl
         *.      jmp $2
         *.   ; negative branch: if lval < to goto endlbl
         *.    $1:
         *.      jlt endlbl
         *.   ; end of test
         *.    $2:
         */
        
        /* push <step>; push 0; jlt $1 */
        step_expr_->gen_code(FALSE, FALSE);
        G_cg->write_op(OPC_PUSH_0);
        G_cg->note_push();
        CTcCodeLabel *l1 = gen_jump_ahead(OPC_JLT);
        G_cg->note_pop(2);

        /* positive branch: jgt endlbl; jmp $2 */
        G_cg->write_op(OPC_JGT);
        G_cs->write_ofs2(endlbl, 0);
        CTcCodeLabel *l2 = gen_jump_ahead(OPC_JMP);

        /* negative branch: $1: jlt endlbl */
        def_label_pos(l1);
        G_cg->write_op(OPC_JLT);
        G_cs->write_ofs2(endlbl, 0);

        /* $2: */
        def_label_pos(l2);

        /* 
         *   we took one or the other of the positive or negative branch, and
         *   each one did a JxT that popped two elements 
         */
        G_cg->note_pop(2);
    }
}

/*
 *   Generate code for the reinit portion of the 'for'.  We add 'step' to our
 *   lvalue.  
 */
void CTPNVarInRange::gen_forstm_reinit()
{
    /* 
     *   If we have a step expression, generate "lval += step".  Otherwise
     *   just generate "++lval".  
     */
    if (step_expr_ != 0)
    {
        /* generate "lval += step" */
        CTPNAddAsi_gen_code(lval_, step_expr_, TRUE);
    }
    else
    {
        /* increment the control variable */
        CTPNPreInc_gen_code(lval_, TRUE);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   'foreach' statement 
 */

/*
 *   generate code 
 */
void CTPNStmForeach::gen_code(int, int)
{
    /* set my local frame if necessary */
    CTcPrsSymtab *old_frame = G_cs->set_local_frame(symtab_);

    /* 
     *   add a line record - note that we add the line record after
     *   setting up the local frame, so that the 'for' statement itself
     *   appears within its own inner scope 
     */
    add_debug_line_rec();

    /* push the enclosing statement */
    CTPNStmEnclosing *old_enclosing = G_cs->set_enclosing(this);

    /* if there's a collection expression, generate it */
    if (coll_expr_ != 0)
        CTPNVarIn::gen_iter_init(coll_expr_, iter_local_id_, "foreach");

    /* set the label for the top of the loop */
    CTcCodeLabel *top_lbl = new_label_here();

    /* check for and fetch the next value */
    CTcCodeLabel *end_lbl = 0;
    CTPNVarIn::gen_iter_cond(iter_expr_, iter_local_id_, end_lbl, "foreach");

    /* 
     *   set our labels, so that 'break' and 'continue' statements in our
     *   body will know where to go 
     */
    break_lbl_ = end_lbl;
    cont_lbl_ = top_lbl;

    /* if we have a body, generate it */
    if (body_stm_ != 0)
        gen_code_substm(body_stm_);

    /* 
     *   add another line record - we're now generating code again for the
     *   original 'foreach' line, even though it's after the body 
     */
//$$$    add_debug_line_rec();

    /* jump back to the top of the loop */
    G_cg->write_op(OPC_JMP);
    G_cs->write_ofs2(top_lbl, 0);

    /* 
     *   we're at the end of the loop - this is where we jump for 'break'
     *   and when the condition becomes false 
     */
    if (end_lbl != 0)
        def_label_pos(end_lbl);

    /* restore the enclosing statement */
    G_cs->set_enclosing(old_enclosing);

    /* restore the enclosing local scope */
    G_cs->set_local_frame(old_frame);
}

/* ------------------------------------------------------------------------ */
/*
 *   'while' statement 
 */

/*
 *   generate code 
 */
void CTPNStmWhile::gen_code(int, int)
{
    CTcCodeLabel *top_lbl;
    CTcCodeLabel *end_lbl;
    CTPNStmEnclosing *old_enclosing;

    /* add a line record */
    add_debug_line_rec();

    /* push the enclosing statement */
    old_enclosing = G_cs->set_enclosing(this);

    /* set the label for the top of the loop */
    top_lbl = new_label_here();

    /* generate a label for the end of the loop */
    end_lbl = G_cs->new_label_fwd();

    /* generate the condition, jumping to the end of the loop if false */
    cond_expr_->gen_code_cond(0, end_lbl);

    /* 
     *   set the 'break' and 'continue' label in our node, so that 'break'
     *   and 'continue' statements in subnodes can find the labels during
     *   code generation 
     */
    break_lbl_ = end_lbl;
    cont_lbl_ = top_lbl;

    /* if we have a body, generate it */
    if (body_stm_ != 0)
        gen_code_substm(body_stm_);

    /* 
     *   add another line record - the jump back to the top of the loop is
     *   part of the 'while' itself 
     */
//$$$    add_debug_line_rec();

    /* jump back to the top of the loop */
    G_cg->write_op(OPC_JMP);
    G_cs->write_ofs2(top_lbl, 0);

    /* 
     *   we're at the end of the loop - this is where we jump for 'break'
     *   and when the condition becomes false 
     */
    def_label_pos(end_lbl);

    /* restore the enclosing statement */
    G_cs->set_enclosing(old_enclosing);
}


/* ------------------------------------------------------------------------ */
/*
 *   'do-while' statement 
 */

/*
 *   generate code 
 */
void CTPNStmDoWhile::gen_code(int, int)
{
    CTcCodeLabel *top_lbl;
    CTcCodeLabel *end_lbl;
    CTcCodeLabel *cont_lbl;
    CTPNStmEnclosing *old_enclosing;

    /* add a line record */
    add_debug_line_rec();

    /* push the enclosing statement */
    old_enclosing = G_cs->set_enclosing(this);

    /* set the label for the top of the loop */
    top_lbl = new_label_here();

    /* create a label for after the loop, for any enclosed 'break's */
    end_lbl = G_cs->new_label_fwd();

    /* 
     *   create a label for just before the expression, for any enclosed
     *   'continue' statements 
     */
    cont_lbl = G_cs->new_label_fwd();

    /* set our 'break' and 'continue' labels in our node */
    break_lbl_ = end_lbl;
    cont_lbl_ = cont_lbl;

    /* if we have a body, generate it */
    if (body_stm_ != 0)
        gen_code_substm(body_stm_);

    /* set the debug source position to the 'while' clause's location */
    add_debug_line_rec(while_desc_, while_linenum_);

    /* put the 'continue' label here, just before the condition */
    def_label_pos(cont_lbl);

    /* 
     *   Generate the condition.  If the condition is true, jump back to the
     *   top label; otherwise fall through out of the loop structure.  
     */
    cond_expr_->gen_code_cond(top_lbl, 0);

    /* we're past the end of the loop - this is where we jump for 'break' */
    def_label_pos(end_lbl);

    /* restore the enclosing statement */
    G_cs->set_enclosing(old_enclosing);
}


/* ------------------------------------------------------------------------ */
/*
 *   'break' statement 
 */

/*
 *   generate code 
 */
void CTPNStmBreak::gen_code(int, int)
{
    /* add a line record */
    add_debug_line_rec();

    /* 
     *   ask the enclosing statement to do the work - if there's no
     *   enclosing statement, or none of the enclosing statements can
     *   perform the break, it's an error 
     */
    if (G_cs->get_enclosing() == 0
        || !G_cs->get_enclosing()->gen_code_break(lbl_, lbl_len_))
    {
        /* 
         *   log the error - if there's a label, the problem is that we
         *   couldn't find the label, otherwise it's that we can't perform
         *   a 'break' here at all 
         */
        if (lbl_ == 0)
            G_tok->log_error(TCERR_INVALID_BREAK);
        else
            G_tok->log_error(TCERR_INVALID_BREAK_LBL, (int)lbl_len_, lbl_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   'continue' statement 
 */

/*
 *   generate code 
 */
void CTPNStmContinue::gen_code(int, int)
{
    /* add a line record */
    add_debug_line_rec();

    /* 
     *   ask the enclosing statement to do the work - if there's no
     *   enclosing statement, or none of the enclosing statements can
     *   perform the break, it's an error 
     */
    if (G_cs->get_enclosing() == 0
        || !G_cs->get_enclosing()->gen_code_continue(lbl_, lbl_len_))
    {
        /* 
         *   log the error - if there's a label, the problem is that we
         *   couldn't find the label, otherwise it's that we can't perform
         *   a 'break' here at all 
         */
        if (lbl_ == 0)
            G_tok->log_error(TCERR_INVALID_CONTINUE);
        else
            G_tok->log_error(TCERR_INVALID_CONT_LBL, (int)lbl_len_, lbl_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   'switch' statement 
 */

/*
 *   generate code 
 */
void CTPNStmSwitch::gen_code(int, int)
{
    CTPNStmSwitch *enclosing_switch;
    int i;
    char buf[VMB_DATAHOLDER + VMB_UINT2];
    CTcCodeLabel *end_lbl;
    CTPNStmEnclosing *old_enclosing;

    /* add a line record */
    add_debug_line_rec();

    /* push the enclosing statement */
    old_enclosing = G_cs->set_enclosing(this);

    /* 
     *   Generate the controlling expression.  We want to keep the value,
     *   hence 'discard' is false, and we need assignment (not 'for
     *   condition') conversion rules, because we're going to use the
     *   value in direct comparisons 
     */
    expr_->gen_code(FALSE, FALSE);

    /* make myself the current innermost switch */
    enclosing_switch = G_cs->set_switch(this);

    /*
     *   if we can flow out of the switch, allocate a label for the end of
     *   the switch body 
     */
    if ((get_control_flow(FALSE) & TCPRS_FLOW_NEXT) != 0)
        end_lbl = G_cs->new_label_fwd();
    else
        end_lbl = 0;

    /* the end label is the 'break' location for subnodes */
    break_lbl_ = end_lbl;

    /*
     *   Write my SWITCH opcode, and the placeholder case table.  We'll
     *   fill in the case table with its real values as we encounter the
     *   cases in the course of generating the code.  For now, all we know
     *   is the number of cases we need to put into the table.  
     */
    G_cg->write_op(OPC_SWITCH);

    /* the SWITCH opcode pops the controlling expression value */
    G_cg->note_pop();

    /* write the number of cases */
    G_cs->write2(case_cnt_);

    /* 
     *   remember where the first case slot is - the 'case' parse nodes
     *   will use this to figure out where to write their slot data 
     */
    case_slot_ofs_ = G_cs->get_ofs();

    /* 
     *   Write the placeholder case slots - each case slot gets a
     *   DATA_HOLDER for the case value, plus an INT2 for the branch
     *   offset.  For now, completely zero each case slot.  
     */
    memset(buf, 0, VMB_DATAHOLDER + VMB_UINT2);
    for (i = 0 ; i < case_cnt_ ; ++i)
        G_cs->write(buf, VMB_DATAHOLDER + VMB_UINT2);

    /* write a placeholder for the default jump */
    if (has_default_)
    {
        /* 
         *   remember where the 'default' slot is, so that the 'default'
         *   parse node can figure out where to write its branch offset
         */
        default_slot_ofs_ = G_cs->get_ofs();
        
        /* 
         *   Write the placeholder for the 'default' slot - this just gets
         *   an INT2 for the 'default' jump offset.  As with the case
         *   labels, just zero it for now; we'll fill it in later when we
         *   encounter the 'default' case.  
         */
        G_cs->write2(0);
    }
    else
    {
        /* 
         *   there's no default slot, so the 'default' slot is simply a
         *   jump to the end of the switch body - generate a jump ahead to
         *   our end label 
         */
        G_cs->write_ofs2(end_lbl, 0);
    }

    /* 
     *   generate the switch body - this will fill in the case table as we
     *   encounter the 'case' nodes in the parse tree
     */
    if (body_ != 0)
        gen_code_substm(body_);

    /* 
     *   We're past the body - if we have an end label, set it here.  (We
     *   won't have created an end label if control can't flow out of the
     *   switch; this allows us to avoid generating unreachable instructions
     *   after the switch, which would only increase the code size for no
     *   reason.)  
     */
    if (end_lbl != 0)
        def_label_pos(end_lbl);

    /* restore the enclosing switch */
    G_cs->set_switch(enclosing_switch);

    /* restore the enclosing statement */
    G_cs->set_enclosing(old_enclosing);
}

/* ------------------------------------------------------------------------ */
/*
 *   'case' label statement 
 */

/*
 *   generate code 
 */
void CTPNStmCase::gen_code(int, int)
{
    ulong slot_ofs;
    ulong jump_ofs;

    /* 
     *   we must have an active 'switch' statement, and our expression
     *   value must be a constant -- if either of these is not true, we
     *   have an internal error of some kind, because we should never get
     *   this far if these conditions weren't true 
     */
    if (G_cs->get_switch() == 0 || !expr_->is_const())
        G_tok->throw_internal_error(TCERR_GEN_BAD_CASE);

    /* allocate our case slot from the enclosing 'switch' statement */
    slot_ofs = G_cs->get_switch()->alloc_case_slot();

    /* write the case table entry as a DATAHOLDER value */
    G_cg->write_const_as_dh(G_cs, slot_ofs, expr_->get_const_val());

    /*
     *   Add the jump offset.  This is the offset from this INT2 entry in
     *   our case slot to the current output offset.  The INT2 is offset
     *   from the start of our slot by the DATAHOLDER value.  
     */
    jump_ofs = G_cs->get_ofs() - (slot_ofs + VMB_DATAHOLDER);
    G_cs->write2_at(slot_ofs + VMB_DATAHOLDER, (int)jump_ofs);

    /* 
     *   because we can jump here (via the case table), we cannot allow
     *   peephole optimizations from past instructions - clear the
     *   peephole 
     */
    G_cg->clear_peephole();

    /* generate our substatement, if we have one */
    if (stm_ != 0)
        gen_code_substm(stm_);
}

/* ------------------------------------------------------------------------ */
/*
 *   'default' label statement 
 */

/*
 *   generate code 
 */
void CTPNStmDefault::gen_code(int, int)
{
    ulong slot_ofs;
    char buf[VMB_UINT2];
    ulong jump_ofs;

    /* 
     *   we must have an active 'switch' statement -- if we don't, we have
     *   an internal error of some kind, because we should never have
     *   gotten this far 
     */
    if (G_cs->get_switch() == 0)
        G_tok->throw_internal_error(TCERR_GEN_BAD_CASE);

    /* ask the switch where our slot goes */
    slot_ofs = G_cs->get_switch()->get_default_slot();

    /*
     *   Set the jump offset.  This is the offset from our slot entry in
     *   the case table to the current output offset.  
     */
    jump_ofs = G_cs->get_ofs() - slot_ofs;
    oswp2(buf, (int)jump_ofs);

    /* write our slot entry to the case table */
    G_cs->write_at(slot_ofs, buf, VMB_UINT2);

    /* 
     *   because we can jump here (via the case table), we cannot allow
     *   peephole optimizations from past instructions - clear the
     *   peephole 
     */
    G_cg->clear_peephole();

    /* generate our substatement, if we have one */
    if (stm_ != 0)
        gen_code_substm(stm_);
}

/* ------------------------------------------------------------------------ */
/*
 *   code label statement 
 */

/*
 *   ininitialize 
 */
CTPNStmLabel::CTPNStmLabel(CTcSymLabel *lbl, CTPNStmEnclosing *enclosing)
    : CTPNStmLabelBase(lbl, enclosing)
{
    /* 
     *   we don't have a 'goto' label yet - we'll allocate it on demand
     *   during code generation (labels are local in scope to a code body
     *   so we can't allocate this until code generation begins for our
     *   containing code body) 
     */
    goto_label_ = 0;

    /* 
     *   we don't yet have a 'break' label - we'll allocate this when
     *   someone first refers to it 
     */
    break_label_ = 0;
}

/*
 *   get our code label 
 */
CTcCodeLabel *CTPNStmLabel::get_goto_label()
{
    /* if we don't have a label already, allocate it */
    if (goto_label_ == 0)
        goto_label_ = G_cs->new_label_fwd();

    /* return the label */
    return goto_label_;
}

/*
 *   generate code 
 */
void CTPNStmLabel::gen_code(int, int)
{
    CTPNStmEnclosing *old_enclosing;

    /* push the enclosing statement */
    old_enclosing = G_cs->set_enclosing(this);
    
    /* 
     *   Define our label position - this is where we come if someone does
     *   a 'goto' to this label.  (Note that we might not have a 'goto'
     *   label defined yet - if we weren't forward-referenced by a 'goto'
     *   statement, we won't have a label defined.  Call get_goto_label()
     *   to ensure we create a label if it doesn't already exist.)
     */
    def_label_pos(get_goto_label());

    /* 
     *   add the source location of the label - this probably will have no
     *   effect, since we don't generate any code for the label itself,
     *   but it's harmless so do it anyway to guard against weird cases 
     */
    add_debug_line_rec();

    /* 
     *   generate code for the labeled statement, discarding any
     *   calculated value
     */
    if (stm_ != 0)
        gen_code_substm(stm_);

    /* 
     *   If we have a 'break' label, it means that code within our labeled
     *   statement (i.e., nested within the label) did a 'break' to leave
     *   the labeled statement.  The target of the break is the next
     *   statement after the labeled statement, which comes next, so
     *   define the label here.  
     */
    if (break_label_ != 0)
        def_label_pos(break_label_);

    /* restore the enclosing statement */
    G_cs->set_enclosing(old_enclosing);
}


/*
 *   generate code for a 'break' 
 */
int CTPNStmLabel::gen_code_break(const textchar_t *lbl, size_t lbl_len)
{
    /* 
     *   If the 'break' doesn't specify a label, inherit the default
     *   handling, since we're not a default 'break' target.  If there's a
     *   label, and the label isn't our label, also inherit the default,
     *   since the target lies somewhere else.  
     */
    if (lbl == 0 || G_cs->get_goto_symtab() == 0
        || G_cs->get_goto_symtab()->find(lbl, lbl_len) != lbl_)
        return CTPNStmLabelBase::gen_code_break(lbl, lbl_len);

    /* 
     *   if we don't yet have a 'break' label defined, define one now
     *   (it's a forward declaration, because we won't know where it goes
     *   until we finish generating the entire body of the statement
     *   contained in the label)
     */
    if (break_label_ == 0)
        break_label_ = G_cs->new_label_fwd();

    /* jump to the label */
    G_cg->write_op(OPC_JMP);
    G_cs->write_ofs2(break_label_, 0);

    /* handled */
    return TRUE;
}


/*
 *   generate code for a 'continue' 
 */
int CTPNStmLabel::gen_code_continue(const textchar_t *lbl, size_t lbl_len)
{
    /* 
     *   If there's no label, inherit the default handling, since we're
     *   not a default 'continue' target.  If there's a label, and the
     *   label isn't our label, also inherit the default, since the target
     *   lies somewhere else.  
     */
    if (lbl == 0 || G_cs->get_goto_symtab() == 0
        || G_cs->get_goto_symtab()->find(lbl, lbl_len) != lbl_)
        return CTPNStmLabelBase::gen_code_continue(lbl, lbl_len);

    /* 
     *   It's a 'continue' with my label - ask my enclosed statement to do
     *   the work; return failure if I have no enclosed statement.  Note
     *   that we use a special call - generate a *labeled* continue - to
     *   let the statement know that it must perform the 'continue'
     *   itself and cannot defer to enclosing statements.  
     */
    if (stm_ != 0)
        return stm_->gen_code_labeled_continue();
    else
        return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   'try' 
 */

/*
 *   generate code 
 */
void CTPNStmTry::gen_code(int, int)
{
    ulong start_ofs;
    ulong end_ofs;
    CTPNStmCatch *cur_catch;
    CTcCodeLabel *end_lbl;
    CTPNStmEnclosing *old_enclosing;
    int finally_never_returns;

    /* we have no end label yet */
    end_lbl = 0;

    /* 
     *   add the source location of the 'try' - it probably won't be
     *   needed, because we don't generate any code before the protected
     *   body, but it's harmless and makes sure we have a good source
     *   location in weird cases 
     */
    add_debug_line_rec();

    /* push the enclosing statement */
    old_enclosing = G_cs->set_enclosing(this);

    /* 
     *   If we have a 'finally' clause, we must allocate a
     *   forward-reference code label for it.  We need to be able to reach
     *   the 'finally' clause throughout generation of the protected code
     *   and the 'catch' blocks. 
     */
    if (finally_stm_ != 0)
        finally_lbl_ = G_cs->new_label_fwd();
    else
        finally_lbl_ = 0;

    /* note where the protected code begins */
    start_ofs = G_cs->get_ofs();

    /* generate the protected code */
    if (body_stm_ != 0)
        gen_code_substm(body_stm_);

    /*
     *   Check to see if we have a 'finally' block that never returns.  If we
     *   have a 'finally' block, and it doesn't flow to its next statement,
     *   then our LJSR's to the 'finally' block will never return.  
     */
    finally_never_returns =
        (finally_stm_ != 0
         && (finally_stm_->get_control_flow(FALSE) & TCPRS_FLOW_NEXT) == 0);

    /* 
     *   if there's a "finally" clause, we must generate a local subroutine
     *   call to the "finally" block
     */
    gen_jsr_finally();

    /* 
     *   We must now jump past the "catch" and "finally" code blocks.  If the
     *   "finally" block itself doesn't flow to the next statement, then
     *   there's no need to do this, since we'll never be reached here.  If
     *   there's no "finally" block, then we won't have LJSR'd anywhere, so
     *   this code is definitely reachable.  
     */
    if (!finally_never_returns)
        end_lbl = gen_jump_ahead(OPC_JMP);

    /* 
     *   Note where the protected code ends - it ends at one byte below
     *   the current write offset, because the current write offset is the
     *   next byte we'll write.  The code range we store in the exception
     *   table is inclusive of the endpoints. 
     */
    end_ofs = G_cs->get_ofs() - 1;

    /* generate the 'catch' blocks */
    for (cur_catch = first_catch_stm_ ; cur_catch != 0 ;
         cur_catch = cur_catch->get_next_catch())
    {
        /* generate the 'catch' block */
        cur_catch->gen_code_catch(start_ofs, end_ofs);

        /* call the 'finally' block after the 'catch' finishes */
        gen_jsr_finally();

        /* 
         *   If there's a finally block, or there's another 'catch' after me,
         *   generate a jump past the remaining catch/finally blocks.
         *   
         *   If we do have a finally that doesn't flow to the next statement
         *   (i.e., we throw or return out of the finally), then there's no
         *   need to generate a jump, since we'll never come back here from
         *   the finally block.  
         */
        if (!finally_never_returns
            && (finally_stm_ != 0 || cur_catch->get_next_catch() != 0))
        {
            /* 
             *   if we have no end label yet, generate one now - we might
             *   not have one because we might not have been able to reach
             *   any previous jump to the end of the catch (because we threw
             *   or returned out of the end of all blocks to this point, for
             *   example) 
             */
            if (end_lbl == 0)
            {
                /* we have no label - generate a new one now */
                end_lbl = gen_jump_ahead(OPC_JMP);
            }
            else
            {
                /* we already have an end label - generate a jump to it */
                G_cg->write_op(OPC_JMP);
                G_cs->write_ofs2(end_lbl, 0);
            }
        }
    }

    /* 
     *   Restore the enclosing statement.  We enclose the protected code
     *   and all of the 'catch' blocks, because all of these must leave
     *   through the 'finally' handler.  We do not, however, enclose the
     *   'finally' handler itself - once it's entered, we do not invoke it
     *   again as it leaves.  
     */
    G_cs->set_enclosing(old_enclosing);

    /* generate the 'finally' block, if we have one */
    if (finally_stm_ != 0)
    {
        /* 
         *   Generate the 'finally' code.  The 'finally' block is executed
         *   for the 'try' block plus all of the 'catch' blocks, so the
         *   ending offset is the current position (less one byte, since
         *   the range is inclusive), which encompasses all of the 'catch'
         *   blocks 
         */
        finally_stm_->gen_code_finally(start_ofs, G_cs->get_ofs() - 1, this);
    }

    /* 
     *   we're now past all of the "catch" and "finally" blocks, so we can
     *   define the jump label for jumping past those blocks (we make this
     *   jump from the end of the protected code) - note that we might not
     *   have actually generated the label, since we might never have
     *   reached any code which jumped to it 
     */
    if (end_lbl != 0)
        def_label_pos(end_lbl);
}

/*
 *   Generate code for a 'break' within our protected code or a 'catch'.
 *   We'll first generate a call to our 'finally' block, if we have one,
 *   then let the enclosing statement handle the break.  
 */
int CTPNStmTry::gen_code_break(const textchar_t *lbl, size_t lbl_len)
{
    /* if we have a 'finally' block, invoke it as a local subroutine call */
    gen_jsr_finally();

    /* 
     *   If there's an enclosing statement, let it generate the break; if
     *   there's not, return failure, because we're not a meaningful
     *   target for break.
     */
    if (enclosing_)
        return enclosing_->gen_code_break(lbl, lbl_len);
    else
        return FALSE;
}

/*
 *   Generate code for a 'break' within our protected code or a 'catch'.
 *   We'll first generate a call to our 'finally' block, if we have one,
 *   then let the enclosing statement handle the break.  
 */
int CTPNStmTry::gen_code_continue(const textchar_t *lbl, size_t lbl_len)
{
    /* if we have a 'finally' block, invoke it as a local subroutine call */
    gen_jsr_finally();

    /* 
     *   if there's an enclosing statement, let it generate the continue;
     *   if there's not, return failure, because we're not a meaningful
     *   target for continue
     */
    if (enclosing_)
        return enclosing_->gen_code_continue(lbl, lbl_len);
    else
        return FALSE;
}

/*
 *   Generate a local subroutine call to our 'finally' block, if we have
 *   one.  This should be used when executing a break, continue, goto, or
 *   return out of the protected code or a 'catch' block, or when merely
 *   falling off the end of the protected code or 'catch' block. 
 */
void CTPNStmTry::gen_jsr_finally()
{
    /* if we have a 'finally', call it */
    if (finally_lbl_ != 0)
    {
        /* generate the local subroutine call */
        G_cg->write_op(OPC_LJSR);
        G_cs->write_ofs2(finally_lbl_, 0);

        /* 
         *   the LJSR pushes a value, which is then immediately popped (we
         *   must note the push and pop because it affects our maximum
         *   stack depth requirement) 
         */
        G_cg->note_push();
        G_cg->note_pop();

        /* 
         *   whatever follows the LJSR is logically at the end of the
         *   'finally' block 
         */
        add_debug_line_rec(finally_stm_->get_end_desc(),
                           finally_stm_->get_end_linenum());
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   'catch'
 */

/*
 *   generate code 
 */
void CTPNStmCatch::gen_code(int, int)
{
    /* this can't be called directly - use gen_code_catch() instead */
    G_tok->throw_internal_error(TCERR_CATCH_FINALLY_GEN_CODE);
}

/*
 *   generate code for the 'catch' 
 */
void CTPNStmCatch::gen_code_catch(ulong start_prot_ofs, ulong end_prot_ofs)
{
    CTcSymbol *sym;
    ulong exc_obj_id;
    CTcPrsSymtab *old_frame;

    /* add the source location of the 'catch' clause */
    add_debug_line_rec();

    /* set the local scope */
    old_frame = G_cs->set_local_frame(symtab_);

    /* look up the object defining the class of exceptions to catch */
    sym = G_cs->get_symtab()->find_or_def_undef(exc_class_,
                                                exc_class_len_, FALSE);

    /* assume we won't find a valid object ID */
    exc_obj_id = VM_INVALID_OBJ;

    /* if it's an object, get its ID */
    if (sym->get_type() == TC_SYM_OBJ)
    {
        /* get its object ID */
        exc_obj_id = ((CTcSymObj *)sym)->get_obj_id();
    }
    else if (sym->get_type() != TC_SYM_UNKNOWN)
    {
        /* 
         *   it's defined, but it's not an object - log an error (note
         *   that we don't log an error if the symbol is undefined,
         *   because find_or_def_undef() will already have logged an error
         *   for us) 
         */
        log_error(TCERR_CATCH_EXC_NOT_OBJ, (int)exc_class_len_, exc_class_);
    }

    /* add our exception table entry */
    G_cg->get_exc_table()->add_catch(start_prot_ofs, end_prot_ofs,
                                     exc_obj_id, G_cs->get_ofs());

    /* don't allow any peephole optimizations to affect this offset */
    G_cg->clear_peephole();

    /* 
     *   the VM automatically pushes a value onto the stack to perform the
     *   'catch' 
     */
    G_cg->note_push();

    /* 
     *   generate a SETLCL for our formal parameter, so that the exception
     *   object is stored in our local variable
     */
    exc_var_->gen_code_setlcl();
    
    /* generate code for our statement, if we have one */
    if (body_ != 0)
        gen_code_substm(body_);

    /* restore the enclosing local scope */
    G_cs->set_local_frame(old_frame);
}

/* ------------------------------------------------------------------------ */
/*
 *   'finally' 
 */

/*
 *   generate code 
 */
void CTPNStmFinally::gen_code(int, int)
{
    /* this can't be called directly - use gen_code_finally() instead */
    G_tok->throw_internal_error(TCERR_CATCH_FINALLY_GEN_CODE);
}


/*
 *   generate code for the 'finally' 
 */
void CTPNStmFinally::gen_code_finally(ulong start_prot_ofs,
                                      ulong end_prot_ofs,
                                      CTPNStmTry *try_stm)
{
    CTPNStmEnclosing *old_enclosing;

    /* 
     *   set the source location for our prolog code to the 'finally'
     *   clause's start 
     */
    add_debug_line_rec();

    /* push the enclosing statement */
    old_enclosing = G_cs->set_enclosing(this);

    /* 
     *   add our exception table entry - use the invalid object ID as a
     *   special flag to indicate that we catch all exceptions 
     */
    G_cg->get_exc_table()->add_catch(start_prot_ofs, end_prot_ofs,
                                     VM_INVALID_OBJ, G_cs->get_ofs());

    /* don't allow any peephole optimizations to affect this offset */
    G_cg->clear_peephole();

    /* the VM pushes the exception onto the stack before calling us */
    G_cg->note_push();

    /*
     *   When we get called due to an exception, we want to run the
     *   'finally' code block and then re-throw the exception.  First, store
     *   the exception parameter in our special local stack slot that we
     *   allocated specifically for the purpose of being a temporary holder
     *   for this value.  
     */
    CTcSymLocal::s_gen_code_setlcl_stk(exc_local_id_, FALSE);

    /* call the 'finally' block */
    try_stm->gen_jsr_finally();

    /*
     *   After the 'finally' block returns, we must re-throw the
     *   exception.  Retrieve the contents of our local where we stashed
     *   the exception object and re-throw the exception.  
     */
    CTcSymLocal::s_gen_code_getlcl(exc_local_id_, FALSE);

    /* re-throw the exception - this pops the exception object */
    G_cg->write_op(OPC_THROW);
    G_cg->note_pop();

    /* 
     *   set the source location to the 'finally' clause once again, since
     *   we changed the source location in the course of generating the
     *   catch-all handler 
     */
    add_debug_line_rec();

    /* this is where the 'finally' code block begins - define our label */
    def_label_pos(try_stm->get_finally_lbl());

    /*
     *   The 'finally' block is the target of LJSR instructions, since we
     *   must run the 'finally' block's code from numerous code paths.
     *   The first thing we must do is pop the return address and stash it
     *   in a local variable.  (We note an extra push, since the LJSR will
     *   have pushed the value before transferring control here.)
     */
    G_cg->note_push();
    CTcSymLocal::s_gen_code_setlcl_stk(jsr_local_id_, FALSE);
    
    /* generate the code block, if there is one */
    if (body_ != 0)
        gen_code_substm(body_);

    /* return from the 'finally' subroutine */
    G_cg->write_op(OPC_LRET);
    G_cs->write2(jsr_local_id_);

    /* restore the enclosing statement */
    G_cs->set_enclosing(old_enclosing);
}

/*
 *   It is not legal to enter a 'finally' block via a 'goto' statement,
 *   because there is no valid way to exit the 'finally' block in this
 *   case.  
 */
int CTPNStmFinally::check_enter_by_goto(CTPNStmGoto *goto_stm,
                                        CTPNStmLabel *)
{
    /* this is illegal - log an error */
    goto_stm->log_error(TCERR_GOTO_INTO_FINALLY);

    /* indicate that it's not allowed */
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   'throw' statement 
 */

/*
 *   generate code 
 */
void CTPNStmThrow::gen_code(int, int)
{
    /* add a line record */
    add_debug_line_rec();

    /* 
     *   generate our expression - we use the result (discard = false),
     *   and we are effectively assigning the result, so we can't use the
     *   'for condition' rules 
     */
    expr_->gen_code(FALSE, FALSE);

    /* generate the 'throw' */
    G_cg->write_op(OPC_THROW);

    /* 'throw' pops the expression from the stack */
    G_cg->note_pop();
}

/* ------------------------------------------------------------------------ */
/*
 *   'goto' statement 
 */

/*
 *   generate code 
 */
void CTPNStmGoto::gen_code(int, int)
{
    CTcSymbol *sym;
    CTPNStmLabel *label_stm;
    
    /* add a line record */
    add_debug_line_rec();

    /* 
     *   look up our label symbol in the 'goto' table for the function,
     *   and get the label statement node from the label 
     */
    if (G_cs->get_goto_symtab() == 0
        || (sym = G_cs->get_goto_symtab()->find(lbl_, lbl_len_)) == 0
        || sym->get_type() != TC_SYM_LABEL
        || (label_stm = ((CTcSymLabel *)sym)->get_stm()) == 0)
    {
        /* log an error */
        log_error(TCERR_INVALID_GOTO_LBL, (int)lbl_len_, lbl_);

        /* give up */
        return;
    }

    /*
     *   Tell any enclosing statements to unwind their 'try' blocks for a
     *   transfer to the given label.  We only need to go as far as the
     *   most deeply nested enclosing statement we have in common with the
     *   label, because we'll be transferring control entirely within the
     *   confines of that enclosing statement.  
     */
    if (G_cs->get_enclosing() != 0)
    {
        /* generate the unwinding code */
        G_cs->get_enclosing()->gen_code_unwind_for_goto(this, label_stm);
    }
    else
    {
        CTPNStmEnclosing *enc;
        
        /*
         *   The 'goto' isn't enclosed in any statements.  This means that
         *   we are entering every block that contains the target label.
         *   Some blocks don't allow entering via 'goto', so we must check
         *   at this point to see if any of the enclosing blocks are
         *   problematic. 
         */
        for (enc = label_stm ; enc != 0 ; enc = enc->get_enclosing())
        {
            /* 
             *   make sure we're allowed to enter this statement - if not,
             *   stop scanning, so that we display only one such error 
             */
            if (!enc->check_enter_by_goto(this, label_stm))
                break;
        }
    }

    /* generate a jump to the label */
    G_cg->write_op(OPC_JMP);
    G_cs->write_ofs2(label_stm->get_goto_label(), 0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Generic enclosing statement node 
 */

/*
 *   Generate code for a break, given the target code label object and the
 *   target label symbol, if any.  This can be used for any of the looping
 *   statement types.  
 */
int CTPNStmEnclosing::gen_code_break_loop(CTcCodeLabel *code_label,
                                          const textchar_t *lbl,
                                          size_t lbl_len)
{
    /* 
     *   If the statement is labeled, let the enclosing statement handle
     *   it -- since it's labeled, we can't assume the statement refers to
     *   us without searching for the enclosing label.  
     */
    if (lbl != 0)
    {
        /* if there's an enclosing statement, let it handle it */
        if (enclosing_ != 0)
            return enclosing_->gen_code_break(lbl, lbl_len);

        /* 
         *   there's no enclosing statement, and we can't handle this
         *   because it has an explicit label attached - indicate that no
         *   break has been generated and give up 
         */
        return FALSE;
    }

    /* 
     *   It's unlabeled, so we can take it by default as the nearest
     *   enclosing statement for which 'break' makes sense -- generate the
     *   jump to the given code label.  
     */
    G_cg->write_op(OPC_JMP);
    G_cs->write_ofs2(code_label, 0);

    /* we have generated the break */
    return TRUE;
}

/*
 *   Generate code for a continue, given the target code label object and
 *   the target label symbol, if any.  This can be used for any of the
 *   looping statement types.  
 */
int CTPNStmEnclosing::gen_code_continue_loop(CTcCodeLabel *code_label,
                                             const textchar_t *lbl,
                                             size_t lbl_len)
{
    /* 
     *   If the statement is labeled, let the enclosing statement handle
     *   it -- since it's labeled, we can't assume the statement refers to
     *   us without searching for the enclosing label.  
     */
    if (lbl != 0)
    {
        /* if there's an enclosing statement, let it handle it */
        if (enclosing_ != 0)
            return enclosing_->gen_code_continue(lbl, lbl_len);

        /* 
         *   there's no enclosing statement, and we can't handle this
         *   because it has an explicit label attached - indicate that no
         *   continue has been generated and give up 
         */
        return FALSE;
    }

    /* 
     *   it's unlabeled, so we can take it by default as the nearest
     *   enclosing statement for which 'continue' makes sense -- generate
     *   the jump to the given code label 
     */
    G_cg->write_op(OPC_JMP);
    G_cs->write_ofs2(code_label, 0);

    /* we have generated the continue */
    return TRUE;
}

/*
 *   Generate the code necessary to unwind the stack for executing a
 *   'goto' to the given labeled statement> 
 */
void CTPNStmEnclosing::gen_code_unwind_for_goto(CTPNStmGoto *goto_stm,
                                                CTPNStmLabel *target)
{
    CTPNStmEnclosing *enc;
    
    /* 
     *   Detmerine if the target statement is enclosed within this
     *   statement.  If it is, we do not need to unwind from this
     *   statement or any of its enclosing statements, because control
     *   will remain within this statement.
     *   
     *   To make this determination, start at the target label and search
     *   up its list of enclosing statements.  If we reach 'this', we know
     *   that we enclose the target.  If we reach the outermost enclosing
     *   statement, we know that we do not enclose the taret.  
     */
    for (enc = target ; enc != 0 ; enc = enc->get_enclosing())
    {
        /* 
         *   if we found ourself in the list of enclosing statements
         *   around the target label, the label is contained within us,
         *   hence we do not need to generate any code to leave, because
         *   we're not leaving - simply return immediately without looking
         *   any further 
         */
        if (enc == this)
        {
            /*
             *   'this' is the common ancestor of both the 'goto' and the
             *   target label, so we're not transferring control in or out
             *   of 'this' or any enclosing statement.  However, we are
             *   transfering control IN through all of the statements that
             *   enclose the label up to but not including 'this'.
             *   
             *   Some types of statements do not allow control transfers
             *   in to enclosed labels - in particular, we can't use
             *   'goto' to transfer control into a 'finally' clause.
             *   Check all statements that enclose the label up to but not
             *   including 'this', and make sure they will allow a
             *   transfer in.
             *   
             *   Note that we make this check now, only after we've found
             *   the common ancestor, because we can't tell if we're
             *   actually entering any blocks until we find the common
             *   ancestor.  
             */
            for (enc = target ; enc != 0 && enc != this ;
                 enc = enc->get_enclosing())
            {
                /* make sure we're allowed to enter this statement */
                if (!enc->check_enter_by_goto(goto_stm, target))
                    break;
            }

            /* 
             *   we're not transferring out of this statement or any
             *   enclosing statement, since the source 'goto' and the
             *   target label are both contained within 'this' - we're
             *   done unwinding for the transfer 
             */
            return;
        }
    }

    /* generate code to transfer out of this statement */
    gen_code_for_transfer_out();

    /* check for an enclosing statement */
    if (enclosing_ != 0)
    {
        /* 
         *   We are enclosed by another statement or statements.  This
         *   means that we haven't found a common ancestor yet, so we
         *   might be leaving the enclosing block as well - continue on to
         *   our enclosing statement.  
         */
        enclosing_->gen_code_unwind_for_goto(goto_stm, target);
    }
    else
    {
        /*
         *   This is the outermost significant enclosing statement, which
         *   means that we are transferring control into a completely
         *   unrelated block.  As a result, we will enter every statement
         *   that encloses the target label.
         *   
         *   We must check each block we're entering to see if it allows
         *   entry by 'goto' statements.  Since we now know there is no
         *   common ancestor, and thus that we're entering every block
         *   enclosing the target label, we must check every block
         *   enclosing the target label to see if they allow transfers in
         *   via 'goto' statements.  
         */
        for (enc = target ; enc != 0 ; enc = enc->get_enclosing())
        {
            /* make sure we're allowed to enter this statement */
            if (!enc->check_enter_by_goto(goto_stm, target))
                break;
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Anonymous function 
 */

/*
 *   generate code 
 */
void CTPNAnonFunc::gen_code(int discard, int)
{
    /* if we're discarding the value, don't bother generating the code */
    if (discard)
        return;

    /* 
     *   Push each context object - these are the additional arguments to
     *   the anonymous function pointer object's constructor beyond the
     *   function pointer itself.  Note that we must push the arguments in
     *   reverse order of our list, since arguments are always pushed from
     *   last to first.  
     */
    int argc = 0;
    for (CTcCodeBodyCtx *cur_ctx = code_body_->get_ctx_tail() ; cur_ctx != 0 ;
         cur_ctx = cur_ctx->prv_, ++argc)
    {
        /* 
         *   find our context matching this context - the caller's
         *   contexts are all one level lower than the callee's contexts,
         *   because the caller is at the next recursion level out 
         */
        int our_varnum;
        if (!G_cs->get_code_body()
            ->get_ctx_var_for_level(cur_ctx->level_ - 1, &our_varnum))
        {
            /* this should never happen */
            assert(FALSE);
        }

        /* 
         *   push this context object - to do this, simply retrieve the
         *   value of the local variable in our frame that contains this
         *   context level 
         */
        CTcSymLocal::s_gen_code_getlcl(our_varnum, FALSE);
    }

    /* 
     *   The first argument (and thus last pushed) to the constructor is the
     *   constant function pointer that refers to the code of the anonymous
     *   function.
     *   
     *   For regular static compilation, the function pointer is a simple
     *   code offset constant.  For run-time compilation (in the debugger or
     *   interpreter "eval()" facility), it's a DynamicFunc representing the
     *   dynamically compiled code.  
     */
    if (G_cg->is_eval_for_dyn())
    {
        /* 
         *   Dynamic (run-time) compilation - the bytecode is in a
         *   dynamically created DynamicFunc instance.  The dynamic compiler
         *   assigns object IDs for the anonymous function code body before
         *   generating any code for any referencing object, so we simply
         *   generate a PUSHOBJ for the code body's object ID. 
         */
        G_cg->write_op(OPC_PUSHOBJ);
        G_cs->write_obj_id(code_body_->get_dyn_obj_id());
    }
    else
    {
        /* conventional static compiler - the bytecode is in the code pool */
        G_cg->write_op(OPC_PUSHFNPTR);
        code_body_->add_abs_fixup(G_cs);
        G_cs->write4(0);
    }

    /* count the argument */
    ++argc;

    /* note the push of the function pointer argument */
    G_cg->note_push();

    /*
     *   If we have a local context, we need to create an anonymous function
     *   metaclass object that combines the function pointer with the local
     *   context.  If we don't have a local context, this is a simple static
     *   function that we can call directly, in which case we're already done
     *   since we've pushed the function pointer.  
     */
    if (argc > 1 || code_body_->has_local_ctx())
    {
        /* 
         *   We have a local context, so we need create to create the
         *   anonymous function metaclass object.  Generate the NEW.  
         */
        if (argc <= 255)
        {
            G_cg->write_op(OPC_NEW1);
            G_cs->write((char)argc);
            G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_ANONFN));
        }
        else
        {
            G_cg->write_op(OPC_NEW2);
            G_cs->write2(argc);
            G_cs->write2(G_cg->get_predef_meta_idx(TCT3_METAID_ANONFN));
        }
        
        /* push the object value */
        G_cg->write_op(OPC_GETR0);

        /* the 'new' popped the arguments, then we pushed the result */
        G_cg->note_pop(argc - 1);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Implicit constructor 
 */
void CTPNStmImplicitCtor::gen_code(int /*discard*/, int /*for_condition*/)
{
    /* 
     *   Generate a call to inherit each superclass constructor.  Pass the
     *   same argument list we received by expanding the varargs list
     *   parameter in local 0.  
     */
    for (CTPNSuperclass *sc = obj_stm_->get_first_sc() ; sc != 0 ;
         sc = sc->nxt_)
    {
        /* 
         *   if this one is valid, generate code to call its constructor -
         *   it's valid if it has an object symbol 
         */
        CTcSymObj *sc_sym = (CTcSymObj *)sc->get_sym();
        if (sc_sym != 0 && sc_sym->get_type() == TC_SYM_OBJ)
        {
            /* push the argument counter so far (no other arguments) */
            G_cg->write_op(OPC_PUSH_0);
            G_cg->note_push();

            /* get the varargs list local */
            CTcSymLocal::s_gen_code_getlcl(0, FALSE);

            /* convert it to varargs */
            G_cg->write_op(OPC_MAKELSTPAR);

            /* note the extra push and pop for the argument count */
            G_cg->note_push();
            G_cg->note_pop();

            /* it's a varargs call */
            G_cg->write_op(OPC_VARARGC);

            /* generate an EXPINHERIT to this superclass */
            G_cg->write_op(OPC_EXPINHERIT);
            G_cs->write(0);                      /* varargs -> argc ignored */
            G_cs->write_prop_id(G_prs->get_constructor_prop());
            G_cs->write_obj_id(sc_sym->get_obj_id());

            /* 
             *   this removes arguments (the varargs list variable and
             *   argument count) 
             */
            G_cg->note_pop(2);
        }
    }
}



/* ------------------------------------------------------------------------ */
/*
 *   Object Property list entry - value node
 */
void CTPNObjProp::gen_code(int, int)
{
    vm_val_t val;
    char buf[VMB_DATAHOLDER];

    /* get the correct data stream */
    CTcDataStream *str = objdef_->get_obj_sym()->get_stream();

    /* set the current source location for error reporting */
    G_tok->set_line_info(file_, linenum_);

    /* generate code for our expression or our code body, as appropriate */
    if (code_body_ != 0)
    {
        /* if this is a constructor, mark the code body accordingly */
        if (prop_sym_->get_prop() == G_prs->get_constructor_prop())
            code_body_->set_constructor(TRUE);

        /* if it's static, do some extra work */
        if (is_static_)
        {
            /* mark the code body as static */
            code_body_->set_static();

            /* 
             *   add the obj.prop to the static ID stream, so the VM knows to
             *   invoke this initializer at start-up 
             */
            G_static_init_id_stream
                ->write_obj_id(objdef_->get_obj_sym()->get_obj_id());
            G_static_init_id_stream
                ->write_prop_id(prop_sym_->get_prop());
        }

        /* tell our code body to generate the code */
        code_body_->gen_code(FALSE, FALSE);

        /* 
         *   Set up our code offset value.  Write a code offset of zero for
         *   now, since we won't know the correct offset until link time.  
         */
        val.set_codeofs(0);

        /* 
         *   Add a fixup to the code body's fixup list for our dataholder, so
         *   that we fix up the property value when we link.  Note that the
         *   fixup is one byte into our object stream from the current
         *   offset, because the first byte is the type.  
         */
        CTcAbsFixup::add_abs_fixup(code_body_->get_fixup_list_head(),
                                   str, str->get_ofs() + 1);
        
        /* write out our value in DATAHOLDER format */
        vmb_put_dh(buf, &val);
        str->write(buf, VMB_DATAHOLDER);
    }
    else if (expr_ != 0)
    {
        /* 
         *   if my value is constant, write out a dataholder for the constant
         *   value to the stream; otherwise, write out our code and store a
         *   pointer to the code 
         */
        if (expr_->is_const())
        {
            /* write the constant value to the object stream */
            G_cg->write_const_as_dh(str, str->get_ofs(),
                                    expr_->get_const_val());
        }
        else if (expr_->is_dstring())
        {
            /* it's a double-quoted string node */
            CTPNDstr *dstr = (CTPNDstr *)expr_;

            /* 
             *   Add the string to the constant pool.  Note that the fixup
             *   will be one byte from the current object stream offset,
             *   since we need to write the type byte first.  
             */
            G_cg->add_const_str(dstr->get_str(), dstr->get_str_len(),
                                str, str->get_ofs() + 1);

            /* 
             *   Set up the dstring value.  Use a zero placeholder for now;
             *   add_const_str() already added a fixup for us that will
             *   supply the correct value at link time.  
             */
            val.set_dstring(0);
            vmb_put_dh(buf, &val);
            str->write(buf, VMB_DATAHOLDER);
        }
        else
        {
            /* we should never get here */
            G_tok->throw_internal_error(TCERR_INVAL_PROP_CODE_GEN);
        }
    }
}

/*
 *   Generate code for an inline object instantiation 
 */
void CTPNObjProp::gen_code_inline_obj()
{
    /* set the current source location for error reporting */
    G_tok->set_line_info(file_, linenum_);

    /* generate code for our expression or our code body, as appropriate */
    if (inline_method_ != 0)
    {
        /* 
         *   Code as an in-line method.  This means that the code was
         *   specified as an explicit method in braces; we compile it as an
         *   anonymous method so that it can access locals in enclosing
         *   scopes.  Generate the code to construct the closure object, then
         *   call obj.setMethod(prop, anonMethod) to bind it as a method of
         *   the object.  This will make it act like a regular method, in
         *   that evaluating the property will call the method.
         */
        inline_method_->gen_code(FALSE, FALSE);

        /* generate the setMethod */
        gen_setMethod();
    }
    else if (expr_ != 0)
    {
        /* check the type of expression */
        if (expr_->is_dstring())
        {
            /* 
             *   Double-quoted string - generate obj.setMethod(prop, string)
             */
            
            /* push the string contents as a regular sstring constant */
            CTPNDstr *dstr = (CTPNDstr *)expr_;
            CTPNConst::s_gen_code_str(dstr->get_str(), dstr->get_str_len());
            
            /* get the setMethod property */
            gen_setMethod();
        }
        else
        {
            /*
             *   Expression, constant or live.  Generate the code to evaluate
             *   the expression; this runs at the moment of evaluating the
             *   overall inline object expression, so we caputre the value of
             *   the expression at the time we create the inline object
             *   instance (and fix the value at that point - this isn't a
             *   method that's invoked when the property is evaluated).
             */
            
            /* push the new object referene */
            G_cg->write_op(OPC_DUP);
            G_cg->note_push();

            /* it's a constant value - do a setprop; start with my value */
            expr_->gen_code(FALSE, FALSE);

            /* if it doesn't have a return value, set the property to nil */
            if (!expr_->has_return_value())
            {
                G_cg->write_op(OPC_PUSHNIL);
                G_cg->note_push();
            }

            /* get the object and value into the proper order */
            G_cg->write_op(OPC_SWAP);

            /* generate the setprop for <obj>.<prop> = <value> */
            G_cg->write_op(OPC_SETPROP);
            G_cs->write_prop_id(prop_sym_->get_prop());
            G_cg->note_pop(2);
        }
    }
}

/*
 *   Generate a setMethod call.  The caller must push the value for the
 *   setMethod, and the object ID must be on the stack just above that.
 */
void CTPNObjProp::gen_setMethod()
{
    /* look up the setMethod property */
    CTcSymProp *setm = (CTcSymProp *)G_prs->get_global_symtab()->find(
        "setMethod", 9);
    if (setm == 0 || setm->get_type() != TC_SYM_PROP)
    {
        G_tok->log_error(TCERR_SYM_NOT_PROP, 9, "setMethod");
        return;
    }
    
    /* push the property ID */
    G_cg->write_op(OPC_PUSHPROPID);
    G_cs->write_prop_id(prop_sym_->get_prop());
    G_cg->note_push();

    /* re-push the object reference (it's on the stack above the value) */
    G_cg->write_op(OPC_GETSPN);
    G_cs->write(2);
    G_cg->note_push();

    /* generate <obj>.setMethod(<prop>, string) */
    G_cg->write_op(OPC_CALLPROP);
    G_cs->write(2);
    G_cs->write_prop_id(setm->get_prop());
    G_cg->note_pop(3);
}

/* ------------------------------------------------------------------------ */
/*
 *   Inline object definition 
 */

/*
 *   Generate code.  An inline object definition creates a new instance of
 *   the object on each evaluation, and creates a new instance of each
 *   anonymous method contained in the property list.  The result of the
 *   expression is the new object reference.
 */
void CTPNInlineObject::gen_code(int discard, int for_condition)
{
    /*
     *   First, create the object via TadsObject.createInstanceOf(), with the
     *   list of superclasses as the arguments.  Use the no-constructor form
     *   of the call, since we'll invoke the new object's construct() method
     *   directly instead.
     */

    /* 
     *   push the superclass references, from last to first (standard
     *   right-to-left order for the method arguments)
     */
    int sccnt = 0;
    for (CTPNSuperclass *sc = sclist_.tail_ ; sc != 0 ; sc = sc->prv_, ++sccnt)
        sc->get_sym()->gen_code(FALSE);

    /* look up TadsObject */
    CTcPrsSymtab *tab = G_prs->get_global_symtab();
    CTcSymMetaclass *tadsobj = (CTcSymMetaclass *)tab->find("TadsObject", 10);
    if (tadsobj == 0 || tadsobj->get_type() != TC_SYM_METACLASS)
    {
        G_tok->log_error(TCERR_UNDEF_METACLASS, 10, "TadsObject");
        return;
    }

    /* look up createInstanceOf */
    const char *cioname = "createInstanceOf";
    size_t ciolen = 16;
    CTcSymProp *cio = (CTcSymProp *)tab->find(cioname, ciolen);
    if (cio == 0 || cio->get_type() != TC_SYM_PROP)
    {
        G_tok->log_error(TCERR_SYM_NOT_PROP, (int)ciolen, cioname);
        return;
    }

    /* call TadsObject.createInstanceOf() */
    G_cg->write_op(OPC_OBJCALLPROP);
    G_cs->write((char)sccnt);
    G_cs->write_obj_id(tadsobj->get_class_obj());
    G_cs->write_prop_id(cio->get_prop());
    G_cg->note_pop(sccnt);

    /* push the new object reference as the result of the overall expression */
    G_cg->write_op(OPC_GETR0);
    G_cg->note_push();

    /* assume we won't find a constructor among the properties */
    int has_ctor = FALSE;

    /* install each property or method */
    for (CTPNObjProp *prop = proplist_.first_ ; prop != 0 ;
         prop = prop->get_next_prop())
    {
        /* generate the property/method setup */
        prop->gen_code_inline_obj();

        /* note if it's a constructor */
        if (prop->get_prop_sym()->get_prop() == G_prs->get_constructor_prop())
            has_ctor = TRUE;
    }

    /* if there's an explicit constructor defined, call it */
    if (has_ctor)
    {
        G_cg->write_op(OPC_DUP);
        G_cg->write_op(OPC_CALLPROP);
        G_cs->write(0);
        G_cs->write_prop_id(G_prs->get_constructor_prop());
    }
}
