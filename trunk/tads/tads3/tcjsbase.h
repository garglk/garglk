/* Copyright (c) 2009 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tcjsbase.h - tads 3 compiler - javascript code generator - parse node
               base class definitions
Function
  
Notes
  
Modified
  02/17/09 MJRoberts  - Creation
*/

#ifndef TCJSBASE_H
#define TCJSBASE_H

#include "tcprs.h"
#include "tcpnbase.h"

/* ------------------------------------------------------------------------ */
/*
 *   Base parse node class 
 */
class CTcPrsNode: public CTcPrsNodeBase
{
public:
    /*
     *   Generate code to assign into the expression as an lvalue.  By
     *   default, we'll throw an error; a default node should never be
     *   called upon to generate code as an assignment target, because the
     *   impossibility of this condition should be detected during
     *   parsing.  Each class that overrides check_lvalue() to return true
     *   must override this to generate code for an assignment.
     *   
     *   Returns true if the assignment type was handled, false if not.
     *   Simple assignment must always be handled, but composite
     *   assignments (such as "+=" or "*=") can refused.  If the
     *   assignment isn't handled, the caller must perform a generic
     *   calculation to compute the result of the assignment (for example,
     *   for "+=", the caller must generate code compute the sum of the
     *   right-hand side and the original value of the left-hand side),
     *   then call gen_code_asi() again to generate a simple assignment.
     *   
     *   In general, gen_code_asi() must generate code for complex
     *   assignments itself only when it can take advantage of special
     *   opcodes that would result in more efficient generated code.  When
     *   no special opcodes are available for the assignment, there's no
     *   need for special coding here.
     *   
     *   If 'ignore_error' is true, this should not generate an error if
     *   this node is not a suitable lvalue, but should simply return
     *   false.  
     */
    virtual int gen_code_asi(int discard, tc_asitype_t typ,
                             class CTcPrsNode *rhs,
                             int ignore_error);

    /*
     *   Generate code to take the address of the expression.  By default,
     *   we'll throw an internal error; a default node should never be
     *   called upon to generate code to take its address, because we
     *   shouldn't be able to parse such a thing.  Each class that
     *   overrides has_addr() to return true must override this to
     *   generate address code.  
     */
    virtual void gen_code_addr();

    /*
     *   Generate code to call the expression as a function or method.
     *   The caller will already have generated code to push the argument
     *   list; this routine only needs to generate code to make the call.
     *   
     *   By default, we'll assume that the result of evaluating the
     *   expression is a method or function pointer, so we'll generate a
     *   call-indirect instruction to call the result of evaluating the
     *   expression.  
     */
    virtual void gen_code_call(int discard, int argc, int varargs);

    /*
     *   Generate code to apply operator 'new' to the expression.  By
     *   default, 'new' cannot be applied to an expression; nodes that
     *   allow operator 'new' must override this.  
     */
    virtual void gen_code_new(int discard, int argc, int varargs,
                              int from_call, int is_transient);

    /*
     *   Generate code to evaluate a member expression on this object
     *   expression, calling the property expression given.  By default,
     *   we'll evaluate our own expression to yield the object value, then
     *   get the property expression (either as a constant or by
     *   generating code to yield a property pointer), then we'll invoke
     *   that code.  Nodes should override this where more specific
     *   instructions can be generated.
     *   
     *   If 'prop_is_expr' is true, the property expression (prop_expr) is
     *   a parenthesized expression; otherwise, it's a simple property
     *   symbol.  (We need this information because a symbol enclosed in
     *   parentheses would be otherwise indistinguishable from a plain
     *   symbol, but the two syntax cases differ in their behavior: a
     *   plain symbol is always a property name, whereas a symbol in
     *   parentheses can be a local variable.)  
     */
    virtual void gen_code_member(int discard,
                                 class CTcPrsNode *prop_expr,
                                 int prop_is_expr,
                                 int argc, int varargs);

    /*
     *   Generate code for an object on the left side of a '.' expression.
     *   If possible, return the constant object rather than generating
     *   code.  If the expression refers to "self", set (*is_self) to true
     *   and return VM_INVALID_OBJ; otherwise, if the expression refers to
     *   a constant object reference, set (*is_self) to false and return
     *   the constant object value; otherwise, generate code for the
     *   expression, set (*is_self) to false, and return VM_INVALID_OBJ to
     *   indicate that the value must be obtained from the generated code
     *   at run-time.
     *   
     *   By default, we'll use generated code to get the value.  
     */
    virtual vm_obj_id_t gen_code_obj_predot(int *is_self)
    {
        /* generate the expression */
        gen_code(FALSE, FALSE);

        /* tell the caller that the value is not a compile-time constant */
        *is_self = FALSE;
        return VM_INVALID_OBJ;
    }
    

    /* 
     *   generic generation for a member expression after the left side
     *   has been generated 
     */
    static void s_gen_member_rhs(int discard,
                                 class CTcPrsNode *prop_expr,
                                 int prop_is_expr, int argc, int varargs);

    /*
     *   Get the property ID of this expression.  If the property ID is
     *   available as a constant value, this doesn't generate any code and
     *   simply returns the constant property ID value.  If this
     *   expression requires run-time evaluation, we'll generate code for
     *   the expression and return VM_INVALID_PROP to indicate that a
     *   constant property ID is not available.
     *   
     *   If 'check_only' is true, this routine should only check to see
     *   whether a constant property value is available and return the
     *   appropriate indication, but should not generate any code in any
     *   case.  Errors should also be suppressed in this case, because the
     *   routine will always be called again to perform the actual
     *   generation, at which point any errors can be logged.
     *   
     *   If 'is_expr' is true, it means that this property was given as an
     *   expression by being explicitly enclosed in parentheses.  If
     *   'is_expr' is false, it means that the property was given as a
     *   simple symbol name.  For cases where the property expression is a
     *   symbol, this distinction is important because we must resolve an
     *   unparenthesized symbol as a property name, even if it's hidden by
     *   a local variable.  
     */
    virtual vm_prop_id_t gen_code_propid(int check_only, int is_expr);

    /* 
     *   generate a jump-ahead instruction, returning a new label which
     *   serves as the jump destination 
     */
    static struct CTcCodeLabel *gen_jump_ahead(uchar opc);

    /* define the position of a code label */
    static void def_label_pos(struct CTcCodeLabel *lbl);

    /* allocate a new label at the current write position */
    static struct CTcCodeLabel *new_label_here();
};

/* ------------------------------------------------------------------------ */
/*
 *   Basic T3-specific symbol class 
 */
class CTcSymbol: public CTcSymbolBase
{
public:
    CTcSymbol(const char *str, size_t len, int copy, tc_symtype_t typ)
        : CTcSymbolBase(str, len, copy, typ) { }

    /* 
     *   get the object value for the symbol, if the symbol has an object
     *   value; returns VM_INVALID_OBJ if the symbol is not an object 
     */
    virtual vm_obj_id_t get_val_obj() const { return VM_INVALID_OBJ; }

    /* generate code to evaluate the symbol */
    virtual void gen_code(int discard) = 0;

    /* 
     *   Generate code to assign to the symbol.  By default, we'll
     *   generate an error indicating that the symbol cannot be assigned
     *   into.  As with CTcPrsNode::gen_code_asi(), this returns true if
     *   the assignment was generated, false if the caller must generate a
     *   generic assignment; simple assignment must always return true.
     *   
     *   If 'rhs' is null, the caller has already generated the value to
     *   be assigned, so this code doesn't need to do that.  Otherwise,
     *   this code must generate the rvalue code.  The reason that 'rhs'
     *   is passed down at all is that we can sometimes optimize the type
     *   of opcode we generate according to the type of value being
     *   assigned, especially when a constant value is being assigned.
     *   
     *   If 'ignore_error' is true, this should not log an error if the
     *   value cannot be assigned.  
     */
    virtual int gen_code_asi(int discard, tc_asitype_t typ,
                             class CTcPrsNode *rhs, int ignore_error);

    /* 
     *   Generate code for taking the address of the symbol.  By default,
     *   we'll generate an error indicating that the symbol's address
     *   cannot be taken. 
     */
    virtual void gen_code_addr();

    /*
     *   Generate code to call the symbol.  By default, we can't call a
     *   symbol.  
     */
    virtual void gen_code_call(int discard, int argc, int varargs);

    /* 
     *   generate code for operator 'new' applied to the symbol, with the
     *   given number of arguments; by default, we can't generate any such
     *   code 
     */
    virtual void gen_code_new(int discard, int argc, int varargs,
                              int is_transient);

    /* evaluate a property ID */
    virtual vm_prop_id_t gen_code_propid(int check_only, int is_expr);

    /* generate code for a member expression */
    virtual void gen_code_member(int discard,
                                 class CTcPrsNode *prop_expr,
                                 int prop_is_expr,
                                 int argc, int varargs);

    /* get the object value for a '.' expression */
    virtual vm_obj_id_t gen_code_obj_predot(int *is_self);

    /* 
     *   Write the symbol to a local frame debugging record in the code
     *   stream.  Returns true if we wrote the symbol, false if not.  By
     *   default, we'll write nothing and return false, since most symbols
     *   aren't used in local symbol tables.  
     */
    virtual int write_to_debug_frame() { return FALSE; }

    /*
     *   Write the symbol to the global symbol table in the debugging
     *   records in an image file.  Returns true if we wrote the symbol,
     *   false if not.  By default, we'll write nothing and return false,
     *   since by default symbols don't go in the image file's global
     *   symbol table.
     */
    virtual int write_to_image_file_global(class CVmImageWriter *)
        { return FALSE; }
};



#endif /* TCJSBASE_H */
