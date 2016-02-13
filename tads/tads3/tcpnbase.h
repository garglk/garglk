/* $Header: d:/cvsroot/tads/tads3/TCPNBASE.H,v 1.3 1999/07/11 00:46:53 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcpn.h - Parse Node - base class
Function
  Defines the target-independent base class for parse nodes
Notes
  All expression parse nodes are derived from the target-specific
  subclass of this class.  The target-independent base class is
  CTcPrsNodeBase; the target-specific class is CTcPrsNode.
Modified
  05/10/99 MJRoberts  - Creation
*/

#ifndef TCPN_H
#define TCPN_H

#include "vmhash.h"

/* ------------------------------------------------------------------------ */
/*
 *   Parse Tree Allocation Object.  This is a base class that can be used
 *   for tree objects that are to be allocated from the parser node pool. 
 */
class CTcPrsAllocObj
{
public:
    /*
     *   Override operator new() - allocate all parse node objects out of
     *   the parse node pool.  
     */
    void *operator new(size_t siz);
};


/* ------------------------------------------------------------------------ */
/*
 *   Parse Tree Expression Node - base class.  As we parse an expression,
 *   we build a tree of these objects to describe the source code.
 *   
 *   This class is subclassed for each type of parsing node: each type of
 *   statement has a node type, some statements have helper node types for
 *   parts of statements, and each expression operator has a node type.
 *   These subclasses contain the information specific to the type of
 *   parsing construct represented.
 *   
 *   Each parsing subclass is then further subclassed for each target
 *   architecture.  This final subclass contains the code generator for
 *   the node in the target architecture.
 *   
 *   The target-independent base version of each subclass is called
 *   CTPNXxxBase.  The target-specific subclass derived from this base
 *   class is CTPNXxx.  For example, the final subclass for constant
 *   nodes, which is derived from the target-independent base class
 *   CTPNConstBase, is CTPNConst.  (Note that each target uses the same
 *   name for the final subclass, so we can only link one target
 *   architecture into a given build of the compiler.  Each additional
 *   target requires a separate compiler executable with the appropriate
 *   CTPNConst classes linked in.)  
 */
class CTcPrsNodeBase: public CTcPrsAllocObj
{
public:
    /* 
     *   Generate code for the expression for the target architecture.
     *   This method is defined only by the final target-specific
     *   subclasses.
     *   
     *   This method is used to generate code to evaluate the expression
     *   as an rvalue.
     *   
     *   If 'discard' is true, it indicates that any value yielded by the
     *   expression will not be used, in which case the generated code
     *   need not leave the result of the expression on the stack.  We can
     *   generate code more efficiently for certain types of expressions
     *   when we know that we're evaluating them only for side effects.
     *   For example, an assignment expression has a result value, but
     *   this value need not be pushed onto the stack if it will simply be
     *   discarded.  Also, an operator like "+" that has no side effects
     *   of its own can merely evaluate its operands for their side
     *   effects, but need not compute its own result if that result would
     *   simply be discarded.
     *   
     *   If 'for_condition' is true, it indicates that the result of the
     *   expression will be used directly for a conditional of some kind
     *   (for a "?:" operator, an "if" statement, a "while" statement, or
     *   the like).  In some cases, we can avoid extra conversions to some
     *   values when they're going to be used directly for a comparison;
     *   for example, the "&&" operator must return a true/nil value, but
     *   the code generator may be able to avoid the extra conversion when
     *   the value will be used for an "if" statement's conditional value.
     */
    virtual void gen_code(int discard, int for_condition) = 0;

    /*
     *   Get the constant value of the parse node, if available.  Most
     *   parse nodes have no constant value, so by default this returns
     *   null.  Only constant parse nodes can provide a constant value, so
     *   they should override this.  
     */
    virtual class CTcConstVal *get_const_val() { return 0; }

    /* determine if the node has a constant value */
    int is_const() { return get_const_val() != 0; }

    /* determine if I have a given constant integer value */
    int is_const_int(int val)
    {
        return (is_const()
                && get_const_val()->get_type() == TC_CVT_INT
                && get_const_val()->get_val_int() == val);
    }

    /*
     *   Set the constant value of the parse node from that of another
     *   node.  The caller must already have checked that this node and
     *   the value being assigned are both valid constant values.  
     */
    void set_const_val(class CTcPrsNode *src)
    {
        /* set my constant value from the source's constant value */
        get_const_val()->set(((CTcPrsNodeBase *)src)->get_const_val());
    }

    /* 
     *   Is this value guaranteed to be a boolean value?  Certain operators
     *   produce boolean results regardless of the operands.  In particular,
     *   the comparison operators (==, !=, >, <, >=, <=) and the logical NOT
     *   operator (!) always produce boolean results. 
     */
    virtual int is_bool() const { return FALSE; }

    /*
     *   Check to see if this expression can possibly be a valid lvalue.
     *   Return true if so, false if not.  This check is made before
     *   symbol resolution; when it is not certain whether or not a symbol
     *   expression can be an lvalue, assume it can be at this point.  By
     *   default, we'll return false; operator nodes whose result can be
     *   used as an lvalue should override this to return true.  
     */
    virtual int check_lvalue() const { return FALSE; }

    /*
     *   Check to see if this expression is an valid lvalue, after
     *   resolving symbols in the given scope.  Returns true if so, false
     *   if not. 
     */
    virtual int check_lvalue_resolved(class CTcPrsSymtab *symtab) const
        { return FALSE; }

    /*
     *   Check to see if this expression can possibly be a valid address
     *   value, so that the address-of ("&") operator can be applied.
     *   Returns true if it is possible, false if not.  The only type of
     *   expression whose address can be taken is a simple symbol.  The
     *   address of a symbol can be taken only if the symbol is a function
     *   or property name, but we won't know this at parse time, so we'll
     *   indicate that any symbol is acceptable.  By default, this returns
     *   false, since the address of most expressions cannot be taken.  
     */
    virtual int has_addr() const { return FALSE; }

    /*
     *   Check to see if this expression is an address expression of some
     *   kind (i.e., of class CTPNAddrBase, or of a class derived from
     *   CTPNAddrBase).  Returns true if so, false if not.  
     */
    virtual int is_addr() const { return FALSE; }

    /*
     *   Determine if this node is of type double-quoted string (dstring).
     *   Returns true if so, false if not.  By default, we return false.
     */
    virtual int is_dstring() const { return FALSE; }

    /*
     *   Determine if this node contains a dstring expression.  This returns
     *   true if the node is a dstring, a dstring embedding, or a comma node
     *   containing a dstring and/or a dstring embedding.
     */
    virtual int is_dstring_expr() const { return FALSE; }

    /*
     *   Determine if this is a simple assignment operator node.  Returns
     *   true if so, false if not.  By default, we return false. 
     */
    virtual int is_simple_asi() const { return FALSE; }

    /*
     *   Determine if this node yields a value when evaluated.  Returns
     *   true if so, false if not.  When it cannot be determined at
     *   compile-time whether or not the node has a value (for example,
     *   for a call to a pointer to a function whose return type is not
     *   declared), this should indicate that a value is returned.
     *   
     *   Most nodes yield a value when executed, so we'll return true by
     *   default.  
     */
    virtual int has_return_value() const { return TRUE; }

    /*
     *   Determine if this node yields a return value when called as a
     *   function.  We assume by default that it does. 
     */
    virtual int has_return_value_on_call() const { return TRUE; }

    /*
     *   Get the text of the symbol for this node, if any.  If the node is
     *   not some kind of symbol node, this returns null.  
     */
    virtual const textchar_t *get_sym_text() const { return 0; }
    virtual size_t get_sym_text_len() const { return 0; }

    /* check for a match to the given symbol text */
    virtual int sym_text_matches(const char *str, size_t len) const
        { return FALSE; }

    /*
     *   Fold constant expressions, given a finished symbol table.  We do
     *   most of our constant folding during the initial parsing, but some
     *   constant folding must wait until the symbol table is finished; in
     *   particular, we can't figure out what to do with symbols until we
     *   know what the symbols mean.
     *   
     *   For most nodes, this function should merely recurse into subnodes
     *   and fold constants.  Nodes that are affected by symbol
     *   resolution, directly or indirectly, should override this.
     *   
     *   For example, a list can change from unknown to constant during
     *   this operation.  If the list contains a symbol, the list will
     *   initially be set to unknown, since the symbol could turn out to
     *   be a property evaluation, which would be non-constant, or an
     *   object name, which would be constant.
     *   
     *   Returns the folded version of the node, or simply 'this' if no
     *   folding takes place.  
     */
    virtual class CTcPrsNode *fold_constants(class CTcPrsSymtab *symtab) = 0;

    /* 
     *   generate a constant value node for the address of this node;
     *   returns null if the symbol has no address 
     */
    virtual class CTcPrsNode *fold_addr_const(class CTcPrsSymtab *)
    {
        /* by default, we have no address */
        return 0;
    }

    /*
     *   Adjust the expression for use as a dynamic (run-time) compilation
     *   expression expression.  Dynamic expressions include debugger
     *   expression evaluations and the run-time "eval()" facility.
     *   
     *   Code generation for dynamic expressions is somewhat different than
     *   for normal expressions.  This routine should allocate and return a
     *   new node, if necessary, for dynamic use.  If no changes are
     *   necessary, simply return 'this'.
     *   
     *   If 'speculative' is true, the expression is being evaluated
     *   speculatively by the debugger.  This means that the user hasn't
     *   explicitly asked for the expression to be evaluated, but rather the
     *   debugger is making a guess that the expression might be of interest
     *   to the user and is making an unsolicited attempt to offer it to the
     *   user.  Because the debugger is only guessing that the expression is
     *   interesting, the expression must not be evaluated if it has any side
     *   effects at all.  
     */
    virtual class CTcPrsNode *adjust_for_dyn(const tcpn_dyncomp_info *info);
};

/* ------------------------------------------------------------------------ */
/*
 *   Symbol Table Entry.  Each symbol has an entry in one of the symbol
 *   tables:
 *   
 *   - The global symbol table contains object, property, and built-in
 *   functions from the default function set.
 *   
 *   - Local symbol tables contain local variables and parameters.  Local
 *   tables have block-level scope.
 *   
 *   - Label symbol tables contain code labels (for "goto" statements).
 *   Label tables have function-level or method-level scope.  
 */

/*
 *   Basic symbol table entry.  The target 
 */
class CTcSymbolBase: public CVmHashEntryCS
{
public:
    CTcSymbolBase(const char *str, size_t len, int copy, tc_symtype_t typ)
        : CVmHashEntryCS(str, len, copy)
    {
        typ_ = typ;
    }

    /* allocate symbol entries from the parser memory pool */
    void *operator new(size_t siz);

    /* get the symbol type */
    tc_symtype_t get_type() const { return typ_; }

    /* get the symbol text and length */
    const char *get_sym() const { return getstr(); }
    size_t get_sym_len() const { return getlen(); }

    /*
     *   Generate a constant value node for this symbol, if possible;
     *   returns null if the symbol does not evaluate to a compile-time
     *   constant value.  An object name, for example, evaluates to a
     *   compile-time constant equal to the object reference; a property
     *   name, in contrast, is (when not qualified by another operator) an
     *   invocation of the property, hence must be executed at run time,
     *   hence is not a compile-time constant.  
     */
    virtual class CTcPrsNode *fold_constant()
    {
        /* by default, a symbol's value is not a constant */
        return 0;
    }

    /* 
     *   generate a constant value node for the address of this symbol;
     *   returns null if the symbol has no address 
     */
    virtual class CTcPrsNode *fold_addr_const()
    {
        /* by default, a symbol has no address */
        return 0;
    }

    /* determine if this symbol can be used as an lvalue */
    virtual int check_lvalue() const { return FALSE; }

    /* determine if this symbol can have its address taken */
    virtual int has_addr() const { return FALSE; }

    /* determine if I have a return value when evaluated */
    virtual int has_return_value_on_call() const { return TRUE; }

    /* 
     *   Write the symbol to a symbol export file.  By default, we'll
     *   write the type and symbol name to the file.  Some subclasses
     *   might wish to override this to write additional data, or to write
     *   something different or nothing at all (for example, built-in
     *   function symbols are not written to a symbol export file).
     *   
     *   When a subclass does override this, it must write the type as a
     *   UINT2 value as the first thing written to the file.  The generic
     *   file reader switches on this type code to determine what to call
     *   to load the entry, then calls the subclass-specific loader to do
     *   the actual work.
     *   
     *   Returns true if we wrote the symbol to the file, false if not.
     *   (False doesn't indicate an error - it indicates that we chose not
     *   to store the symbol because the symbol is not of a type that we
     *   want to put in the export file.)  
     */
    virtual int write_to_sym_file(class CVmFile *fp);

    /* write the symbol name (with a UINT2 length prefix) to a file */
    int write_name_to_file(class CVmFile *fp);

    /*
     *   Write the symbol to an object file.  By default, we'll write the
     *   type and symbol name to the file.  Some subclasses might wish to
     *   override this to write additional data, or to write something
     *   different or nothing at all (for example, built-in function
     *   symbols are not written to an object file).
     *   
     *   When a subclass does override this, it must write the type as a
     *   UINT2 value as the first thing written to the file.  The generic
     *   file reader switches on this type code to determine what to call
     *   to load the entry, then calls the subclass-specific loader to do
     *   the actual work.
     *   
     *   Returns true if we wrote the symbol to the file, false if not.
     *   (False doesn't indicate an error - it indicates that we chose not
     *   to store the symbol because the symbol is not of a type that we
     *   want to put in the export file.)  
     */
    virtual int write_to_obj_file(class CVmFile *fp);

    /*
     *   Write the symbol's cross references to the object file.  This can
     *   write references to other symbols by storing the other symbol's
     *   index in the object file.  Most symbols don't have any cross
     *   references, so this does nothing by default.
     *   
     *   If this writes anything, the first thing written must be a UINT4
     *   giving the object file index of this symbol.  On loading, we'll
     *   read this and look up the loaded symbol.  
     */
    virtual int write_refs_to_obj_file(class CVmFile *) { return FALSE; }

    /* 
     *   perform basic writing to a file - this performs common work that
     *   can be used for object or symbol files 
     */
    int write_to_file_gen(CVmFile *fp);

    /*
     *   Read a symbol from a symbol file, returning the new symbol 
     */
    static class CTcSymbol *read_from_sym_file(class CVmFile *fp);

    /*
     *   Load a symbol from an object file.  Stores the symbol in the
     *   global symbol table, and fills in the appropriate translation
     *   mapping table when necessary.  Returns zero on success; logs
     *   error messages and return non-zero on failure.  
     */
    static int load_from_obj_file(class CVmFile *fp,
                                  const textchar_t *fname,
                                  tctarg_obj_id_t *obj_xlat,
                                  tctarg_prop_id_t *prop_xlat,
                                  ulong *enum_xlat);

    /*
     *   Load references from the object file - reads the information that
     *   write_refs_to_obj_file() wrote, except that the caller will have
     *   read the first UINT4 giving the symbol's object file index before
     *   calling this routine. 
     */
    virtual void load_refs_from_obj_file(class CVmFile *,
                                         const textchar_t * /*obj_fname*/,
                                         tctarg_obj_id_t * /*obj_xlat*/,
                                         tctarg_prop_id_t * /*prop_xlat*/)
    {
        /* by default, do nothing */
    }

    /*
     *   Log an object file loading conflict with this symbol.  The given
     *   type is the new type found in the object file of the given name. 
     */
    void log_objfile_conflict(const textchar_t *fname, tc_symtype_t new_type)
        const;

    /*
     *   Get a pointer to the head of the fixup list for this symbol.
     *   Symbols such as functions that keep a list of fixups for
     *   references to the symbol must override this to provide a fixup
     *   list head; by default, symbols keep no fixup list, so we'll just
     *   return null. 
     */
    virtual struct CTcAbsFixup **get_fixup_list_anchor() { return 0; }

    /*
     *   Set my code stream anchor object.  By default, symbols don't keep
     *   track of any stream anchors.  Symbols that refer to code or data
     *   stream locations directly must keep an anchor, since they must
     *   keep track of their fixup list in order to fix up generated
     *   references to the symbol.  This must be overridden by any
     *   subclasses that keep anchors.  
     */
    virtual void set_anchor(struct CTcStreamAnchor *) { }

    /*
     *   Determine if this symbol is external and unresolved.  By default,
     *   a symbol cannot be external at all, so this will return false.
     *   Subclasses for symbol types that can be external should override
     *   this to return true if the symbol is an unresolved external
     *   reference. 
     */
    virtual int is_unresolved_extern() const { return FALSE; }

    /*
     *   Mark the symbol as referenced.  Some symbol types keep track of
     *   whether they've been referenced or not; those types can override
     *   this to keep track.  This method is called each time the symbol
     *   is found in the symbol table via the find() or find_or_def()
     *   methods.  By default, we do nothing.
     */
    virtual void mark_referenced() { }

    /*
     *   Apply internal fixups.  If the symbol keeps its own internal
     *   fixup information, it can translate the fixups here.  By default,
     *   this does nothing.  
     */
    virtual void apply_internal_fixups() { }

    /*
     *   Build dictionary entries for this symbol.  Most symbols do
     *   nothing here; objects which can have associated vocabulary words
     *   should insert their vocabulary into the dictionary.  
     */
    virtual void build_dictionary() { }

    /*
     *   Create a new "context variable" version of this symbol for use in
     *   an anonymous function.  This is only needed for symbols that can
     *   exist in a local scope.  
     */
    virtual class CTcSymbol *new_ctx_var() const { return 0; }

    /*
     *   Apply context variable conversion.  If this symbol has not been
     *   referenced, this should simply remove the symbol from the symbol
     *   table.  Otherwise, this should apply the necessary conversions to
     *   the original symbol from which this symbol was created to ensure
     *   that the original and this symbol share a context variable slot.
     *   
     *   Returns true if a conversion was performed (i.e., the symbol was
     *   referenced), false if not.  
     */
    virtual int apply_ctx_var_conv(class CTcPrsSymtab *,
                                   class CTPNCodeBody *)
        { return FALSE; }

    /*
     *   Finalize context variable conversion.  This should do nothing if
     *   the variable hasn't already been notified that it's a context
     *   variable (how this happens varies by symbol type - see locals in
     *   particular).  This is called with the variable's own scope active
     *   in the parser, so the final variable assignments for the symbol
     *   can be made.  
     */
    virtual void finish_ctx_var_conv() { }

    /*
     *   Check for local references.  For variables that can exist in
     *   local scope, such as locals, this will be called when all of the
     *   code for the scope has been parsed; this should check to see if
     *   the symbol has been referenced in the scope, and display an
     *   appropriate warning message if not.  
     */
    virtual void check_local_references() { }

    /*
     *   Add an entry for this symbol to a "runtime symbol table," which is
     *   a symbol table that we can pass to the interpreter.  This must be
     *   overridden by each symbol type for each target architecture,
     *   because the nature of the runtime symbol table varies by target
     *   architecture.
     *   
     *   By default, this does nothing.  Symbol types that don't need to
     *   generate runtime symbol table entries don't need to override this.  
     */
    virtual void add_runtime_symbol(class CVmRuntimeSymbols *) { }
    
protected:
    /* 
     *   Base routine to read from a symbol file - reads the symbol name.
     *   Returns a pointer to the symbol name (stored in tokenizer memory
     *   that will remain valid throughout the compilation) on success; on
     *   failure, logs an error and returns null.  
     */
    static const char *base_read_from_sym_file(class CVmFile *fp);
    
    /* symbol type */
    tc_symtype_t typ_;
};

#endif /* TCPN_H */
