/* $Header: d:/cvsroot/tads/tads3/tcprs.h,v 1.5 1999/07/11 00:46:58 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcprs.h - TADS 3 Compiler - parser
Function
  
Notes
  
Modified
  04/29/99 MJRoberts  - Creation
*/

#ifndef TCPRS_H
#define TCPRS_H

#include <assert.h>

#include "vmtype.h"
#include "t3std.h"
#include "tcglob.h"
#include "tctok.h"
#include "tctargty.h"
#include "tcprstyp.h"


/* ------------------------------------------------------------------------ */
/*
 *   Object ID type 
 */
typedef ulong tc_obj_id;

/*
 *   Property ID type 
 */
typedef uint tc_prop_id;


/* ------------------------------------------------------------------------ */
/*
 *   scope data structure 
 */
struct tcprs_scope_t
{
    /* local symbol table */
    class CTcPrsSymtab *local_symtab;

    /* enclosing scope's local symbol table */
    class CTcPrsSymtab *enclosing_symtab;

    /* number of locals allocated in scope */
    int local_cnt;
};

/* ------------------------------------------------------------------------ */
/*
 *   Code body parsing types.  Each type of code body is essentially the
 *   same with minor variations, so we use a common code body parser that
 *   checks the parsing type to apply the variations.  
 */
enum tcprs_codebodytype
{
    /* a standard function or method code body */
    TCPRS_CB_NORMAL,

    /* anonymous function */
    TCPRS_CB_ANON_FN,

    /* anonymous method */
    TCPRS_CB_ANON_METHOD,

    /* short-form anonymous function */
    TCPRS_CB_SHORT_ANON_FN
};


/* ------------------------------------------------------------------------ */
/*
 *   the saved method context is always at index 1 in local variable context
 *   arrays, when we're using local variable context arrays 
 */
#define TCPRS_LOCAL_CTX_METHODCTX  1


/* ------------------------------------------------------------------------ */
/*
 *   Parser 
 */
class CTcParser
{
public:
    CTcParser();
    ~CTcParser();

    /* initialize - call this after the code generator is set up */
    void init();

    /* 
     *   reset - for dynamic code compilation in the interpreter, this resets
     *   internal state for the start of a new compilation 
     */
    void reset()
    {
        /* forget any old local symbol table */
        local_symtab_ = 0;

        /* clear out all of our lists */
        nested_stm_head_ = nested_stm_tail_ = 0;
        anon_obj_head_ = anon_obj_tail_ = 0;
        nonsym_obj_head_ = nonsym_obj_tail_ = 0;
        exp_head_ = exp_tail_ = 0;
        enclosing_stm_ = 0;
        dict_cur_ = dict_head_ = dict_tail_ = 0;
        dict_prop_head_ = 0;
        gramprod_head_ = gramprod_tail_ = 0;
        template_head_ = template_tail_ = 0;
        cur_code_body_ = 0;

        /* clear out any symbol table references */
        local_symtab_ = 0;
        enclosing_local_symtab_ = 0;
        goto_symtab_ = 0;
    }

    /* 
     *   define (def=TRUE) or look up (def=FALSE) a special compiler-defined
     *   property 
     */
    class CTcSymProp *def_special_prop(int def, const char *name,
                                       tc_prop_id *idp = 0);

    /*
     *   Set the module information.  This tells us the module's name (as
     *   it's given in the makefile (.t3m) or library (.tl) file) and its
     *   sequence number (an ordinal giving its position in the list of
     *   modules making up the overall program build).  We use this
     *   information in generating the sourceTextGroup object.  
     */
    void set_module_info(const char *name, int seqno);

    /*
     *   Write an exported symbol file.  An exported symbol file
     *   facilitates separate compilation by providing a listing of the
     *   symbols defined in another module.  If module A depends on the
     *   symbols from module B, the user can first create an exported
     *   symbol file for module B, then can compile module A in the
     *   presence of B's symbol file, without actually loading B, and
     *   without manually entering a set of external definitions in module
     *   A's source code. 
     */
    void write_symbol_file(class CVmFile *fp, class CTcMake *make_obj);

    /*
     *   Seek to the start of the build configuration information in a symbol
     *   file.  The return value is the number of bytes stored in the build
     *   configuration block; on return, the file object will have its seek
     *   offset set to the first byte of the build configuration data.
     *   Returns zero if the symbol file is invalid or does not contain any
     *   configuration data.  
     */
    static ulong seek_sym_file_build_config_info(class CVmFile *fp);

    /* 
     *   Write the global table to an object file. 
     */
    void write_to_object_file(class CVmFile *fp);

    /*
     *   Read an object file and load it into the global symbol table.  We
     *   will fill in the object and property ID translation tables
     *   provided with the translated values for the object and property
     *   symbols that we find in the object file.
     *   
     *   Returns zero on success; logs error messages and returns non-zero
     *   on error.  Note that a non-zero value should be returned only
     *   when the file appears to be corrupted or an I/O error occurs;
     *   errors involving conflicting symbols, or other problems that do
     *   not prevent us from continuing to read the file in an orderly
     *   fashion, should not return failure but should simply log the
     *   error and continue; this way, we can detect any additional symbol
     *   conflicts or other errors.  This routine should return failure
     *   only when it is not possible to continue reading the file.  
     */
    int load_object_file(class CVmFile *fp,
                         const textchar_t *fname,
                         tctarg_obj_id_t *obj_xlat,
                         tctarg_prop_id_t *prop_xlat,
                         ulong *enum_xlat);

    /*
     *   Apply internal object/property ID fixups.  This traverses the
     *   symbol table and calls each symbol's apply_internal_fixups()
     *   method.  This can be called once after loading all object files.  
     */
    void apply_internal_fixups();
    
    /*
     *   Read an exported symbol file.  Reads the file and loads the
     *   global symbol table with the symbols in the file, with each
     *   symbol marked as external.
     *   
     *   This can be used for separate compilation.  If module A depends
     *   on symbols in module B, first create a symbol file for module B,
     *   then module A can be compiled simply be pre-loading B's symbol
     *   file.  Any symbol files that a module depends upon must be loaded
     *   before the module is compiled - symbol file loading must precede
     *   parsing.
     *   
     *   If any errors occur, we'll log the errors and return non-zero.
     *   We'll return zero on success.  
     */
    int read_symbol_file(class CVmFile *fp);

    /* get the global symbol table */
    class CTcPrsSymtab *get_global_symtab() const { return global_symtab_; }

    /* set the global symbol table */
    class CTcPrsSymtab *set_global_symtab(class CTcPrsSymtab *t)
    {
        /* remember the old table */
        class CTcPrsSymtab *old = global_symtab_;

        /* set the new table */
        global_symtab_ = t;

        /* refresh the cache of special internal symbols */
        cache_special_props(FALSE);

        /* return the old symbol table */
        return old;
    }

    /* get the current local symbol table */
    class CTcPrsSymtab *get_local_symtab() const { return local_symtab_; }

    /* get the 'goto' symbol table */
    class CTcPrsSymtab *get_goto_symtab() const { return goto_symtab_; }

    /* set the current pragma C mode */
    void set_pragma_c(int mode);

    /* turn preprocess expression mode on or off */
    void set_pp_expr_mode(int f) { pp_expr_mode_ = f; }

    /* get the current preprocess expression mode flag */
    int get_pp_expr_mode() const { return pp_expr_mode_; }

    /* set/get the sourceTextGroup mode */
    void set_source_text_group_mode(int f);
    int get_source_text_group_mode() const { return src_group_mode_; }

    /* get/set the syntax-only mode flag */
    int get_syntax_only() const { return syntax_only_; }
    void set_syntax_only(int f) { syntax_only_ = f; }

    /* get/set the debugger expression flag */
    int is_debug_expr() const { return debug_expr_; }
    void set_debug_expr(int f) { debug_expr_ = f; }

    /* 
     *   Get the constructor and finalize property ID's - all constructors
     *   and finalizers have these property ID's respectively 
     */
    tc_prop_id get_constructor_prop() const { return constructor_prop_; }
    tc_prop_id get_finalize_prop() const { return finalize_prop_; }

    /* get the constructor property symbol */
    class CTcSymProp *get_constructor_sym() const { return constructor_sym_; }

    /* get the exported GrammarProd exported */
    tc_prop_id get_grammarTag_prop() const;
    tc_prop_id get_grammarInfo_prop() const;

    /*
     *   Check for unresolved external symbols.  Scans the global symbol
     *   table and logs an error for each unresolved external.  Returns
     *   true if any unresolved externals exist, false if not.  
     */
    int check_unresolved_externs();

    /* 
     *   build the dictionaries - scans the global symbol table, and
     *   inserts each object symbol's dictionary words into its
     *   corresponding dictionary 
     */
    void build_dictionaries();

    /* build the grammar productions */
    void build_grammar_productions();

    /*
     *   Top-level parser.  Parse functions, objects, and other top-level
     *   definitions and declarations. 
     */
    class CTPNStmProg *parse_top();

    /* 
     *   Parse a required semicolon.  If the semicolon is present, we'll
     *   simply skip it.  If it's missing, we'll log an error and try to
     *   resynchronize.  If we find something that looks like it should go
     *   at the end of an expression, we'll try to skip up to the next
     *   semicolon; otherwise, we'll simply stay put.
     *   
     *   Returns zero if the caller should proceed, non-zero if we're at
     *   end of file, in which case there's nothing more for the caller to
     *   parse.  
     */
    static int parse_req_sem();

    /* 
     *   Skip to the next semicolon, ignoring any tokens up to that point.
     *   This can be used when the caller encounters an error that makes
     *   it impossible to process the current statement further, and wants
     *   to find the next semicolon in the hope that it will be a good
     *   place to start again with the next statement.
     *   
     *   Returns zero if the caller should proceed, non-zero if we reach
     *   the end of the file.  
     */
    static int skip_to_sem();

    /* 
     *   Parse an expression.  This parses a top-level "comma" expression. 
     */
    class CTcPrsNode *parse_expr();

    /*
     *   Parse a condition expression.  This parses a top-level "comma"
     *   expression, but displays a warning if the outermost operator in
     *   the expression is an assignment, because such expressions are
     *   very frequently meant as comparisons, but the '=' operator was
     *   inadvertantly used instead of '=='. 
     */
    class CTcPrsNode *parse_cond_expr();

    /* 
     *   Parse a value expression or a double-quoted string expression
     *   (including a double-quoted string with embedded expressions).  If
     *   allow_comma_expr is true, we'll parse a comma expression;
     *   otherwise, we'll parse an assignment expression.  (A comma
     *   expression is broader than an assignment expression, since the
     *   comma separates assignment expressions.)  
     */
    class CTcPrsNode *parse_expr_or_dstr(int allow_comma_expr);

    /* 
     *   Parse an assignment expression - this is the next precedence
     *   level down from comma expressions.  In certain contexts, a
     *   top-level comma expression is not allowed because a comma has a
     *   separate meaning (in the initializer clause of a 'for' statement,
     *   for example, or in a list element). 
     */
    class CTcPrsNode *parse_asi_expr();

    /* parse an 'enum' top-level statement */
    void parse_enum(int *err);

    /* parse a 'dictionary' top-level statement */
    class CTPNStmTop *parse_dict(int *err);

    /* parse a 'grammar' top-level statement */
    class CTPNStmTop *parse_grammar(int *err, int replace, int modify);

    /* parse a grammar token list (an alternative list) */
    void parse_gram_alts(int *err, class CTcSymObj *gram_obj,
                         class CTcGramProdEntry *prod,
                         struct CTcGramPropArrows *arrows,
                         class CTcGramAltFuncs *funcs);

    /* parse and flatten a set of grammar rules */
    class CTcPrsGramNode *flatten_gram_rule(int *err);

    /* parse a 'grammar' OR node */
    class CTcPrsGramNode *parse_gram_or(int *err, int level);

    /* parse a 'grammar' CAT node */
    class CTcPrsGramNode *parse_gram_cat(int *err, int level);

    /* parse a 'grammar' qualifier int value */
    int parse_gram_qual_int(int *err, const char *qual_name, int *stm_end);

    /* skip to the end of a mal-formed grammar qualifier */
    void parse_gram_qual_skip(int *err, int *stm_end);

    /* 
     *   Parse a 'function' top-level statement.  If 'is_extern' is true,
     *   the function is being defined externally, so it should have no
     *   code body defined here (just the prototype).  If 'replace' is
     *   true, we're replacing an existing function.
     *   
     *   If 'func_kw_present' is true, the 'function' keyword is present
     *   and must be skipped; otherwise, the function definition elides
     *   the 'function' keyword and starts directly with the function name
     *   symbol.  
     */
    class CTPNStmTop *parse_function(int *err, int is_extern,
                                     int replace, int modify,
                                     int func_kw_present);

    /* parse an 'intrinsic' top-level statement */
    class CTPNStmTop *parse_intrinsic(int *err);

    /* parse an 'intrinsic class' top-level statement */
    class CTPNStmTop *parse_intrinsic_class(int *err);

    /* parse an 'extern' top-level statement */
    void parse_extern(int *err);

    /* 
     *   parse an object or function defintion (this is called when the
     *   first thing in a statement is a symbol; we must check what
     *   follows to determine what type of definition it is) 
     */
    class CTPNStmTop *parse_object_or_func(int *err, int replace,
                                           int suppress_error,
                                           int *suppress_next_error);

    /* parse a template definition statement */
    class CTPNStmTop *parse_template_def(int *err,
                                         const class CTcToken *class_tok);

    /* parse a string template definition statement */
    class CTPNStmTop *parse_string_template_def(int *err);

    /* add a template definition */
    void add_template_def(class CTcSymObj *class_sym,
                          class CTcObjTemplateItem *item_head,
                          size_t item_cnt);

    /* add inherited template definitions */
    void add_inherited_templates(class CTcSymObj *sc_sym,
                                 class CTcObjTemplateItem *item_head,
                                 size_t item_cnt);

    /* 
     *   expand the 'inherited' keyword in a template for the given
     *   superclass template and add the result to the template list for the
     *   class 
     */
    void expand_and_add_inherited_template(class CTcSymObj *sc_sym,
                                           class CTcObjTemplateItem *items,
                                           class CTcObjTemplate *sc_tpl);

    /* 
     *   build a list of superclass templates, for expanding an 'inherited'
     *   token in a template definition 
     */
    void build_super_template_list(struct inh_tpl_entry **list_head,
                                   struct inh_tpl_entry **list_tail,
                                   class CTcSymObj *sc_sym);

    /* parse an 'object' statement */
    class CTPNStmTop *parse_object_stm(int *err, int is_transient);

    /* 
     *   parse an object definition that starts with a '+' string; this
     *   also parses '+ property' statements 
     */
    class CTPNStmTop *parse_plus_object(int *err);

    /* 
     *   Parse an object definition.  If 'replace' is true, this
     *   definition is to replace a previous definition of the same
     *   object; if 'modify' is true, this definition is to modify a
     *   previous definition.  If 'is_class' is true, the definition is
     *   for a class, otherwise it's for a static instance.
     *   
     *   If the definition uses the '+' notation to set the location,
     *   plus_cnt gives the number of '+' signs preceding the object
     *   definition.  
     */
    class CTPNStmTop *parse_object(int *err, int replace, int modify,
                                   int is_class, int plus_cnt,
                                   int is_transient);

    /* find or define an object symbol */
    CTcSymObj *find_or_def_obj(const char *tok_txt, size_t tok_len,
                               int replace, int modify, int *is_class,
                               class CTcSymObj **mod_orig_sym,
                               class CTcSymMetaclass **meta_sym,
                               int *is_transient);

    /* parse an anonymous object */
    class CTPNStmObject *parse_anon_object(int *err, int plus_cnt,
                                           int is_nested,
                                           struct tcprs_term_info *term_info,
                                           int is_transient);

    /* 
     *   Parse an object body.  We start parsing from the colon that
     *   introduces the class list, and parse the class list and the
     *   property list for the object.
     *   
     *   If 'is_anon' is true, this is an anonymous object.  'obj_sym'
     *   should be null in this case.
     *   
     *   If 'is_nested' is true, this is a nested object defined in-line in
     *   an object's property list.  Note that is_nested implies is_anon,
     *   since nested objects are always anonymous.
     *   
     *   If this is a 'modify' definition, 'mod_orig_tok' should be set up
     *   with the synthesized symbol for the modified base object;
     *   otherwise, 'mod_orig_tok' should be null.
     *   
     *   If 'meta_sym' is non-null, we're modifying an intrinsic class.
     *   This imposes certain restrictions; in particular, we cannot modify
     *   a method defined in the native interface to the class.  
     */
    class CTPNStmObject *parse_object_body(
        int *err, class CTcSymObj *obj_sym,
        int is_class, int is_anon, int is_grammar, int is_nested,
        int modify, class CTcSymObj *mod_orig_sym,
        int plus_cnt, class CTcSymMetaclass *meta_sym,
        struct tcprs_term_info *term_info, int is_transient);

    /* parse an object definition's superclass list */
    void parse_superclass_list(
        class CTcSymObj *obj_sym, class CTPNSuperclassList &sclist);

    /*
     *   Add a generated object.  This is used for objects created implicitly
     *   rather than defined in the source code.  For the static compiler,
     *   we'll create an anonymous object and set up its definition
     *   statements.  For the dynamic compiler, we'll actually create the VM
     *   object directly.  
     */
    class CTcSymObj *add_gen_obj(const char *clsname);
    class CTcSymObj *add_gen_obj(class CTcSymObj *cls);
    class CTcSymObj *add_gen_obj()
        { return add_gen_obj((class CTcSymObj *)0); }

    /* add constant property values to a generated object */
    void add_gen_obj_prop(class CTcSymObj *obj, const char *prop, int val);
    void add_gen_obj_prop(class CTcSymObj *obj, const char *prop,
                          const char *val);
    void add_gen_obj_prop(class CTcSymObj *obj, const char *prop,
                          const CTcConstVal *val);

    /* parse an object template instance in an object body */
    void parse_obj_template(int *err, class CTPNObjDef *objdef, int is_inline);

    /* search a superclass list for a template match */
    const class CTcObjTemplate
        *find_class_template(const class CTPNSuperclass *first_sc,
                             class CTcObjTemplateInst *src,
                             size_t src_cnt, const CTPNSuperclass **def_sc,
                             int *undescribed_class);

    /* find a match for a given template in the given list */
    const class CTcObjTemplate
        *find_template_match(const class CTcObjTemplate *first_tpl,
                             class CTcObjTemplateInst *src,
                             size_t src_cnt);

    /*
     *   Match a template to a given actual template parameter list.  Returns
     *   true if we match, false if not.  We'll fill in the actual list with
     *   the property symbols that we matched; these values are only
     *   meaningful if we return true to indicate a match.  
     */
    int match_template(const class CTcObjTemplateItem *tpl_head,
                       class CTcObjTemplateInst *src, size_t src_cnt);

    /* get the first string template */
    class CTcStrTemplate *get_str_template_head() const
        { return str_template_head_; }

    /* parse an object's property list */
    int parse_obj_prop_list(
        int *err, class CTPNObjDef *objdef, class CTcSymMetaclass *meta_sym,
        int modify, int is_nested, int braces, int is_inline,
        struct tcprs_term_info *outer_term_info,
        struct tcprs_term_info *term_info);

    /* parse property definition within an object */
    void parse_obj_prop(
        int *err, class CTPNObjDef *objdef, int replace,
        class CTcSymMetaclass *meta_sym, struct tcprs_term_info *term_info,
        struct propset_def *propset_stack, int propset_depth,
        int enclosing_obj_is_nested, int is_inline);

    /* parse a class definition */
    class CTPNStmTop *parse_class(int *err);

    /* parse a 'modify' definition */
    class CTPNStmTop *parse_modify(int *err);

    /* parse a 'replace' definition */
    class CTPNStmTop *parse_replace(int *err);

    /* parse a 'property' statement */
    void parse_property(int *err);

    /* parse an 'export' statement */
    void parse_export(int *err);

    /* add an export for the given symbol; returns the new export record */
    class CTcPrsExport *add_export(const char *sym, size_t sym_len);

    /* add an export record to our list */
    void add_export_to_list(class CTcPrsExport *exp);

    /* get the head of the export list */
    class CTcPrsExport *get_exp_head() const { return exp_head_; }

    /*
     *   Parse a function or method body, starting with the formal parameter
     *   list.  If 'eq_before_brace' is set, we expect an '=' before the
     *   opening brace of the code body, and we allow the expression syntax,
     *   where an expression enclosed in parentheses can be used.
     *   'self_valid' indicates whether or not 'self' is valid in the context
     *   of the code being compiled; for an object method, 'self' is usually
     *   valid, while for a stand-alone function it isn't.  
     */
    class CTPNCodeBody *parse_code_body(int eq_before_brace, int is_obj_prop,
                                        int self_valid, 
                                        int *p_argc, int *p_opt_argc,
                                        int *p_varargs, int *p_varargs_list,
                                        class CTcSymLocal **
                                            p_varargs_list_local,
                                        int *has_retval, int *err,
                                        class CTcPrsSymtab *local_symtab,
                                        tcprs_codebodytype cb_type,
                                        struct propset_def *propset_stack,
                                        int propset_depth,
                                        struct CTcCodeBodyRef *enclosing,
                                        class CTcFormalTypeList **type_list);

    /* parse a nested code body (such as an anonymous function) */
    class CTPNCodeBody *parse_nested_code_body(
        int eq_before_brace,
        int self_valid,
        int *p_argc, int *p_opt_argc, int *p_varargs,
        int *p_varargs_list, class CTcSymLocal **p_varargs_list_local,
        int *has_retval, int *err,
        class CTcPrsSymtab *local_symtab,
        tcprs_codebodytype cb_type);

    /* insert a propertyset expansion */
    void insert_propset_expansion(struct propset_def *propset_stack,
                                  int propset_depth);

    /* parse a formal parameter list */
    void parse_formal_list(int count_only, int opt_allowed,
                           int *argc, int *opt_argc, int *varargs,
                           int *varargs_list,
                           class CTcSymLocal **varargs_list_local,
                           int *err, int base_formal_num,
                           int for_short_anon_func,
                           class CTcFormalTypeList **type_list);

    /*
     *   Parse a compound statement.  If 'skip_lbrace' is true, we'll skip
     *   the opening '{', otherwise the caller must already have skipped it.
     *   If 'need_rbrace' is true, we require the block to be closed by an
     *   '}', which we'll skip before returning; otherwise, the block can end
     *   at end-of-file on the token stream.
     *   
     *   'enclosing_symtab' is the enclosing scope's symbol table, and
     *   'local_symtab' is the symbol table for the new scope within the
     *   compound statement; if the caller has not already allocated a new
     *   symbol table for the inner scope, it should simply pass the same
     *   value for both symbol tables.
     *   
     *   'enclosing_switch' is the immediately enclosing switch statement, if
     *   any.  This is only set when we're parsing the immediate body of a
     *   switch statement.  
     */
    class CTPNStmComp *parse_compound(int *err,
                                      int skip_lbrace, int need_rbrace,
                                      class CTPNStmSwitch *enclosing_switch,
                                      int use_enclosing_scope);

    /* parse a local variable definition */
    class CTPNStm *parse_local(int *err);

    /* parse a local initializer */
    class CTcPrsNode *parse_local_initializer(class CTcSymLocal *lcl,
                                              int *err);

    /*
     *   Parse an individual statement.
     *   
     *   If 'compound_use_enclosing_scope' is true, then if the statement
     *   is a compound statement (i.e., the current token is a left
     *   brace), the compound statement will use the current scope rather
     *   than creating its own scope.  Normally, a compound statement
     *   establishes its own scope, so that local variables can hide
     *   locals and parameters defined outside the braces.  In certain
     *   cases, however, locals defined within the braces should share the
     *   enclosing scope: at the top level of a function or method, for
     *   example, the formal parameters and the locals within the function
     *   body go in the same scope, so the function body's compound
     *   statement doesn't create its own scope.  
     */
    class CTPNStm *parse_stm(int *err, class CTPNStmSwitch *enclosing_switch,
                             int compound_use_enclosing_scope);

    /* parse a 'case' label */
    class CTPNStm *parse_case(int *err,
                              class CTPNStmSwitch *enclosing_switch);

    /* parse a 'default' label */
    class CTPNStm *parse_default(int *err,
                                 class CTPNStmSwitch *enclosing_switch);

    /* parse an 'if' statement */
    class CTPNStm *parse_if(int *err);

    /* parse a 'return' statement */
    class CTPNStm *parse_return(int *err);

    /* parse a 'for' statement */
    class CTPNStm *parse_for(int *err);

    /* parse an 'in' clause in a 'for' statement */
    class CTcPrsNode *parse_for_in_clause(
        class CTcPrsNode *lval,
        class CTPNForIn *&head, class CTPNForIn *&tail);

    /* parse a 'foreach' statement */
    class CTPNStm *parse_foreach(int *err);

    /* parse a 'break' statement */
    class CTPNStm *parse_break(int *err);

    /* parse a 'continue' statement */
    class CTPNStm *parse_continue(int *err);

    /* parse a 'while' */
    class CTPNStm *parse_while(int *err);
    
    /* parse a 'do-while' */
    class CTPNStm *parse_do_while(int *err);

    /* parse a 'switch' */
    class CTPNStm *parse_switch(int *err);

    /* parse a 'goto' */
    class CTPNStm *parse_goto(int *err);

    /* parse a 'try' */
    class CTPNStm *parse_try(int *err);

    /* parse a 'throw' */
    class CTPNStm *parse_throw(int *err);

    /* parse an 'operator' property name */
    int parse_op_name(class CTcToken *tok, int *op_argp = 0);

    /*
     *   Create a symbol node.  We'll look up the symbol in local scope.
     *   If we find the symbol in local scope, we'll return a resolved
     *   symbol node for the local scope item.  If the symbol isn't
     *   defined in local scope, we'll return an unresolved symbol node,
     *   so that the symbol's resolution can be deferred until code
     *   generation. 
     */
    class CTcPrsNode *create_sym_node(const textchar_t *sym, size_t sym_len);

    /*
     *   Get the source file descriptor and line number for the current
     *   source line.  We note this at the start of each statement, so
     *   that a statement node constructed when we finish parsing the
     *   statement can record the location of the start of the statement. 
     */
    class CTcTokFileDesc *get_cur_desc() const { return cur_desc_; }
    long get_cur_linenum() const { return cur_linenum_; }

    /*
     *   Get/set the current enclosing statement.  An enclosing statement
     *   is a 'try' or 'label:' container.  At certain times, we need to
     *   know the current enclosing statement, or one of its enclosing
     *   statements; for example, a 'break' with a label must find the
     *   label in the enclosing statement list to know where to jump to
     *   after the 'break', and must also know about all of the enclosing
     *   'try' blocks our to that point so that it can invoke their
     *   'finally' blocks.  
     */
    class CTPNStmEnclosing *get_enclosing_stm() const
        { return enclosing_stm_; }
    class CTPNStmEnclosing *set_enclosing_stm(class CTPNStmEnclosing *stm)
    {
        class CTPNStmEnclosing *old_enclosing;

        /* remember the current enclosing statement for a moment */
        old_enclosing = enclosing_stm_;

        /* set the new enclosing statement */
        enclosing_stm_ = stm;

        /* 
         *   return the previous enclosing statement - this allows the
         *   caller to restore the previous enclosing statement upon
         *   leaving a nested block, if that's why the caller is setting a
         *   new enclosing statement 
         */
        return old_enclosing;
    }

    /* get the current code body reference object */
    struct CTcCodeBodyRef *get_cur_code_body() const
        { return cur_code_body_; }

    /* determine if 'self' is valid in the current context */
    int is_self_valid() const { return self_valid_; }

    /* 
     *   get/set the 'self' reference status - this indicates whether or not
     *   'self' has been referenced, explicitly via the 'self'
     *   pseudo-variable or implicitly (such as via a property reference or
     *   method call), in the code body currently being parsed 
     */
    int self_referenced() const { return self_referenced_; }
    void set_self_referenced(int f) { self_referenced_ = f; }

    /* 
     *   get/set the full method context reference status - this indicates
     *   whether or not any of the method context variables (self,
     *   targetprop, targetobj, definingobj) have been referenced, explicitly
     *   or implicitly, in the code body currently being parsed 
     */
    int full_method_ctx_referenced() const
        { return full_method_ctx_referenced_; }
    void set_full_method_ctx_referenced(int f)
        { full_method_ctx_referenced_ = f; }

    /* 
     *   Get/set the flag indicating whether or not the local context of the
     *   outermost code body needs 'self'.  The outer code body needs 'self'
     *   in the local context if any lexically nested code body requires
     *   access to 'self'.  
     */
    int local_ctx_needs_self() const { return local_ctx_needs_self_; }
    void set_local_ctx_needs_self(int f) { local_ctx_needs_self_ = f; }

    /* 
     *   Get/set the flag indicating whether or not the local context of the
     *   outermost code body needs the full method context stored in its
     *   local context.  The outer code body needs the full context stored if
     *   any lexically nested code body requires access to any of the method
     *   context variables besides 'self' (targetprop, targetobj,
     *   definingobj).  
     */
    int local_ctx_needs_full_method_ctx() const
        { return local_ctx_needs_full_method_ctx_; }
    void set_local_ctx_needs_full_method_ctx(int f)
        { local_ctx_needs_full_method_ctx_ = f; }

    /*
     *   Add a code label.  This creates a 'goto' symbol table for the
     *   current code body if one doesn't already exist 
     */
    class CTcSymLabel *add_code_label(const class CTcToken *tok);

    /*
     *   Set the debugger local symbol table.  Returns the previous symbol
     *   table so that it can be restored if desired.  
     */
    class CTcPrsDbgSymtab *set_debug_symtab(class CTcPrsDbgSymtab *tab)
    {
        class CTcPrsDbgSymtab *old_tab;

        /* remember the original for later use */
        old_tab = debug_symtab_;

        /* set the new table */
        debug_symtab_ = tab;

        /* return the original */
        return old_tab;
    }

    /*
     *   given a (1-based) object file symbol index, get the symbol 
     */
    class CTcSymbol *get_objfile_sym(uint idx)
        { return (idx == 0 ? 0 : obj_sym_list_[idx - 1]); }

    /* 
     *   given a 1-based object file symbol index, get an object symbol;
     *   if the symbol does not refer to an object, we'll return null 
     */
    class CTcSymObj *get_objfile_objsym(uint idx);

    /* 
     *   given an object file (1-based) object file dictionary index, get
     *   the dictionary entry 
     */
    class CTcDictEntry *get_obj_dict(uint idx)
        { return (idx == 0 ? 0 : obj_dict_list_[idx - 1]); }

    /* add a dictionary object loaded from the object file */
    void add_dict_from_obj_file(class CTcSymObj *sym);

    /* add a symbol object loaded from the object file */
    void add_sym_from_obj_file(uint idx, class CTcSymbol *sym);

    /*
     *   Get the next object file symbol index.  Object file symbol
     *   indices are used to relate symbols stored in the object file to
     *   the corresponding symbol object in memory when the object file is
     *   reloaded.  
     */
    uint get_next_obj_file_sym_idx()
    {
        /* return the next index, consuming the index value */
        return obj_file_sym_idx_++;
    }

    /*
     *   Get the next object file dictionary index.
     */
    uint get_next_obj_file_dict_idx()
    {
        /* return the next index, consuming the index value */
        return obj_file_dict_idx_++;
    }

    /* 
     *   add an anonymous function or other anonymous top-level statement
     *   to our list of nested top-level statements
     */
    void add_nested_stm(class CTPNStmTop *stm);

    /* get the head of the nested statement list */
    class CTPNStmTop *get_first_nested_stm() const
        { return nested_stm_head_; }

    /* add an anonymous object to our list */
    void add_anon_obj(class CTcSymObj *obj);

    /* add a non-symbolic object ID */
    void add_nonsym_obj(tctarg_obj_id_t id);

    /* determine if the current code body has a local context */
    int has_local_ctx() const { return has_local_ctx_ != 0; }

    /* get the local context variable number */
    int get_local_ctx_var() const { return local_ctx_var_num_; }

    /* set up a local context, in preparation for a nested code body */
    void init_local_ctx();

    /* finish the local context, after parsing a nested code body */
    void finish_local_ctx(CTPNCodeBody *cb, class CTcPrsSymtab *local_symtab);

    /* 
     *   allocate a context variable index - this assigns an array index
     *   for a context variable within the context object that contains
     *   the shared locals for its scope 
     */
    int alloc_ctx_arr_idx();

    /* allocate a local for use as a local context holder */
    int alloc_ctx_holder_var() { return alloc_local(); }

    /* get the maximum number of locals required in the function */
    int get_max_local_cnt() const { return max_local_cnt_; }

    /* get the lexicalParent property symbol */
    class CTcSymProp *get_lexical_parent_sym() const
        { return lexical_parent_sym_; }

    /* 
     *   find a grammar production symbol, adding a new one if needed,
     *   returning the grammar production list entry for the object 
     */
    class CTcGramProdEntry *declare_gramprod(const char *sym, size_t len);

    /* find a grammar production list entry for a given object */
    class CTcGramProdEntry *get_gramprod_entry(class CTcSymObj *sym);

    /* find a grammar production symbol, adding a new one if needed */
    class CTcSymObj *find_or_def_gramprod(const char *txt, size_t len,
                                          class CTcGramProdEntry **entryp);

    /* allocate a new enumerator ID */
    ulong new_enum_id() { return next_enum_id_++; }

    /* get the number of enumerator ID's allocated */
    ulong get_enum_count() const { return next_enum_id_; }

    /* 
     *   Look up a property symbol, adding it if not yet defined.  If the
     *   symbol is defined as another type, we'll show an error if
     *   show_err is true, and return null.  
     */
    CTcSymProp *look_up_prop(const class CTcToken *tok, int show_err);

    /* get the '+' property for tracking the location graph */
    CTcSymProp *get_plus_prop() const { return plus_prop_; }

    /*
     *   Read a length-prefixed string from a file.  Copies the string into
     *   tokenizer space (which is guaranteed valid throughout compilation),
     *   and returns a pointer to the tokenizer copy.  If ret_len is null,
     *   we'll return a null-terminated string; otherwise, we'll return a
     *   non-null-terminated string and set *ret_len to the length of the
     *   string.
     *   
     *   The string must fit in the temporary buffer to be read, but the
     *   permanent tokenizer copy is returned rather than the temp buffer.
     *   If the string doesn't fit in the temp buffer (with null
     *   termination, if null termination is requested), we'll log the given
     *   error.  
     */
    static const char *read_len_prefix_str
        (CVmFile *fp, char *tmp_buf, size_t tmp_buf_len, size_t *ret_len,
         int err_if_too_long);

    /*
     *   Read a length-prefixed string into the given buffer, null
     *   terminating the result.  If the string is too long for the buffer,
     *   we'll flag the given error code and return non-zero.  If
     *   successful, we'll return zero. 
     */
    static int read_len_prefix_str(CVmFile *fp, char *buf, size_t buf_len,
                                   int err_if_too_long);


    /* get the miscVocab property symbol */
    tctarg_prop_id_t get_miscvocab_prop() const { return miscvocab_prop_; }

    /* property symbols for the operator overload properties */
    class CTcSymProp *ov_op_add_;
    class CTcSymProp *ov_op_sub_;
    class CTcSymProp *ov_op_mul_;
    class CTcSymProp *ov_op_div_;
    class CTcSymProp *ov_op_mod_;
    class CTcSymProp *ov_op_xor_;
    class CTcSymProp *ov_op_shl_;
    class CTcSymProp *ov_op_ashr_;
    class CTcSymProp *ov_op_lshr_;
    class CTcSymProp *ov_op_bnot_;
    class CTcSymProp *ov_op_bor_;
    class CTcSymProp *ov_op_band_;
    class CTcSymProp *ov_op_neg_;
    class CTcSymProp *ov_op_idx_;
    class CTcSymProp *ov_op_setidx_;

    /* embedded expression token capture list */
    class CTcEmbedTokenList *embed_toks_;

private:
    /* cache the special properties, defining them if desired */
    void cache_special_props(int def);

    /* 
     *   Static compiler handling for object generation.  These are
     *   implemented only in the static compiler; they're stubbed out for the
     *   dynamic compiler, because the dynamic compiler generates objects in
     *   the live VM through the G_vmifc interface instead.  
     */
    CTcSymObj *add_gen_obj_stat(class CTcSymObj *cls);
    void add_gen_obj_prop_stat(class CTcSymObj *obj, class CTcSymProp *prop,
                               const CTcConstVal *val);

    /* clear the anonymous function local context information */
    void clear_local_ctx();

    /* 
     *   begin a property expression, saving parser state for later
     *   restoration with finish_prop_expr 
     */
    void begin_prop_expr(
        class CTcPrsPropExprSave *save_info, int is_static, int is_inline);

    /* 
     *   Finish a property expression, wrapping it in a code body if
     *   necessary to allow for an embedded anonymous function.  Returns
     *   null if no wrapping is required, in which case the original
     *   expression should continue to be used, or the non-null code body
     *   wrapper if needed, in which case the original expression should be
     *   discarded in favor of the fully wrapped code body.  
     */
    void finish_prop_expr(
        class CTcPrsPropExprSave *save_info, class CTcPrsNode* &expr,
        class CTPNCodeBody* &code_body, class CTPNAnonFunc* &inline_method,
        int is_static, int is_inline, class CTcSymProp *prop_sym);
    
    /* 
     *   callback for symbol table enumeration for writing a symbol export
     *   file 
     */
    static void write_sym_cb(void *ctx, class CTcSymbol *sym);
    
    /* callback for symbol table enumeration for writing an object file */
    static void write_obj_cb(void *ctx, class CTcSymbol *sym);
    
    /* callback for symbol table enumeration for writing cross references */
    static void write_obj_ref_cb(void *ctx, class CTcSymbol *sym);

    /* callback for symbol table enumeration for named grammar rules */
    static void write_obj_gram_cb(void *ctx, class CTcSymbol *sym);

    /* callback for symbol table enumeration for merging grammar rules */
    static void build_grammar_cb(void *ctx, class CTcSymbol *sym);


    /*
     *   Enter a scope.  Upon entering, we'll remember the current local
     *   variable data; on leaving, we'll restore the enclosing scope.  
     */
    void enter_scope(struct tcprs_scope_t *info)
    {
        /* remember the current scope information */
        info->local_symtab = local_symtab_;
        info->enclosing_symtab = enclosing_local_symtab_;
        info->local_cnt = local_cnt_;

        /* 
         *   We haven't yet allocated a symbol table local to the new
         *   scope -- we defer this until we actually need to insert a
         *   symbol into the new scope.  In order to detect when we need
         *   to create our own local symbol table, we keep track of the
         *   enclosing symbol table; when the local table is the same as
         *   the enclosing table, and we need to insert a symbol, it means
         *   that we must create a new table for the current scope.  
         */
        enclosing_local_symtab_ = local_symtab_;
    }

    /* leave a scope */
    void leave_scope(struct tcprs_scope_t *info)
    {
        /* restore enclosing scope information */
        local_symtab_ = info->local_symtab;
        enclosing_local_symtab_ = info->enclosing_symtab;

        /* return to the local count in the enclosing scope */
        // $$$ we can't actually do this because variables could
        //     be allocated after this scope ends, but need lifetimes
        //     that overlap with the enclosed scope; what we actually
        //     need to do, if we wanted to optimize things, would be
        //     to allow this block of variables to be used in *disjoint*
        //     scopes, but not again in enclosing scopes.  We can easily,
        //     though suboptimally, handle this by simply not allowing
        //     the variables in the enclosed scope to be re-used at all
        //     in the current code block.
        // local_cnt_ = info->local_cnt;
    }

    /* 
     *   Create a local symbol table in the current scope, if necessary.
     *   If we've already created a local symbol table for the current
     *   scope, this has no effect. 
     */
    void create_scope_local_symtab();

    /* allocate a new local variable ID */
    int alloc_local()
    {
        /* 
         *   if this exceeds the maximum depth in the block so far, note
         *   the new maximum depth 
         */
        if (local_cnt_ + 1 > max_local_cnt_)
            max_local_cnt_ = local_cnt_ + 1;

        /* return the local number, and increment the counter */
        return local_cnt_++;
    }

    /* find a dictionary symbol, adding a new one if needed */
    class CTcDictEntry *declare_dict(const char *sym, size_t len);

    /* create a new dictionary list entry */
    class CTcDictEntry *create_dict_entry(class CTcSymObj *sym);

    /* find a dictionary list entry for a given object */
    class CTcDictEntry *get_dict_entry(class CTcSymObj *sym);

    /* create a new grammar production list entry */
    class CTcGramProdEntry *create_gramprod_entry(class CTcSymObj *sym);

    /* symbol enumerator - look for unresolved external references */
    static void enum_sym_extref(void *ctx, class CTcSymbol *sym);

    /* symbol enumerator - apply internal fixups */
    static void enum_sym_internal_fixup(void *ctx, class CTcSymbol *sym);

    /* symbol enumerator - build dictionary */
    static void enum_sym_dict(void *ctx, class CTcSymbol *sym);

    /* enumeration callback - context local conversion */
    static void enum_for_ctx_locals(void *ctx, class CTcSymbol *sym);

    /* global symbol table */
    class CTcPrsSymtab *global_symtab_;

    /* the constructor property ID and symbol */
    tc_prop_id constructor_prop_;
    class CTcSymProp *constructor_sym_;

    /* the finalizer property ID */
    tc_prop_id finalize_prop_;

    /* grammarInfo property symbol */
    class CTcSymProp *graminfo_prop_;

    /* grammarTag property symbol */
    class CTcSymProp *gramtag_prop_;

    /* miscVocab property ID */
    tctarg_prop_id_t miscvocab_prop_;

    /* lexicalParent property symbol */
    class CTcSymProp *lexical_parent_sym_;

    /* sourceTextOrder property symbol */
    class CTcSymProp *src_order_sym_;

    /* sourceTextGroup property symbol */
    class CTcSymProp *src_group_sym_;

    /* sourceTextGroupName, sourceTextGroupOrder */
    class CTcSymProp *src_group_mod_sym_;
    class CTcSymProp *src_group_seq_sym_;

    /* 
     *   Source text order index.  Each time we encounter an object
     *   definition in the source code, we assign the current index value to
     *   the object's 'sourceTextOrder' property, then we increment the
     *   index.  This provides the game program with information on the order
     *   in which static objects appear in the source code, so that the
     *   program can sort a collection of objects into their source file
     *   order if desired. 
     */
    long src_order_idx_;

    /*
     *   Source group object.  If we're assigning source text group values,
     *   we create an object for each source module to identify the module. 
     */
    tctarg_obj_id_t src_group_id_;

    /* 
     *   flag: in preprocessor constant expression mode; double-quoted
     *   strings should be treated the same as single-quoted strings for
     *   concatenation and comparisons 
     */
    uint pp_expr_mode_ : 1;

    /* 
     *   Is source text mode turned on?  If this is true, we'll generate
     *   sourceTextGroup properties for objects, otherwise we won't. 
     */
    uint src_group_mode_ : 1;

    /*
     *   Flag: syntax-only mode.  We use this mode to analyze the syntax
     *   of the file without building the image; this is used, for
     *   example, to build the exported symbol file for a source file.  In
     *   this mode, we'll suppress certain warnings and avoid doing work
     *   that's not necessary for syntactic analysis; for example, we
     *   won't show "unreachable code" errors.  
     */
    uint syntax_only_ : 1;

    /*
     *   Flag: debugger expression mode.  We accept some special internal
     *   syntax in this mode that's not allowed in ordinary source code.  
     */
    uint debug_expr_ : 1;

    /*
     *   Code block parsing state
     */
    
    /* 
     *   'goto' symbol table for the current code block - there's only one
     *   'goto' scope for an entire code block, so this never changes over
     *   the course of a code block 
     */
    class CTcPrsSymtab *goto_symtab_;

    /* 
     *   Current local symbol table.  Each inner scope that defines its
     *   own local variables has its own local symbol table, nested within
     *   the enclosing scope's.  When leaving an inner scope, this should
     *   always be restored to the local symbol table of the enclosing
     *   scope.  
     */
    class CTcPrsSymtab *local_symtab_;

    /*
     *   Enclosing local symbol table.  If this is the same as
     *   local_symtab_, it means that the current scope has not yet
     *   created its own local symbol table.  We defer this creation until
     *   we find we actually need a local symbol table in a scope, since
     *   most scopes don't define any of their own local variables. 
     */
    class CTcPrsSymtab *enclosing_local_symtab_;

    /*
     *   Current debugger local symbol table.  When we're compiling a
     *   debugger expression, this will provide access to the current
     *   local scope in the debug records. 
     */
    class CTcPrsDbgSymtab *debug_symtab_;

    /* 
     *   Number of local variables allocated so far in current code block
     *   -- this reflects nesting to the current innermost scope, because
     *   variables in inner scope are allocated in the same stack frame as
     *   the enclosing scopes.  When leaving an inner scope, this should
     *   be restored 
     */
    int local_cnt_;

    /* 
     *   maximum local variable depth for the current code block -- this
     *   reflects the maximum depth, including all inner scopes so far 
     */
    int max_local_cnt_;

    /*
     *   Enclosing statement - this is the innermost 'try' or 'label:'
     *   enclosing the current code.
     */
    class CTPNStmEnclosing *enclosing_stm_;

    /* file descriptor and line number at start of current statement */
    class CTcTokFileDesc *cur_desc_;
    long cur_linenum_;

    /* currently active dictionary */
    class CTcDictEntry *dict_cur_;

    /* head and tail of dictionary list */
    class CTcDictEntry *dict_head_;
    class CTcDictEntry *dict_tail_;

    /* head and tail of grammar production entry list */
    class CTcGramProdEntry *gramprod_head_;
    class CTcGramProdEntry *gramprod_tail_;

    /* 
     *   array of symbols loaded from the object file - these are indexed
     *   by the object file symbol index stored in symbol references in
     *   the object file, allowing us to fix up references from one symbol
     *   to another during loading 
     */
    class CTcSymbol **obj_sym_list_;

    /* 
     *   array of dictionary objects for the object file being loaded -
     *   these are indexed by the dictionary index stored in symbol
     *   references in the object file, allowing us to fix up references
     *   from an object to its dictionary 
     */
    class CTcDictEntry **obj_dict_list_;

    /* next available object file dictionary index */
    uint obj_file_dict_idx_;

    /* next available object file symbol index */
    uint obj_file_sym_idx_;

    /* dictionary property list head */
    class CTcDictPropEntry *dict_prop_head_;

    /*
     *   Head and tail of list of nested top-level statements parsed for the
     *   current top-level statement.  This list includes anonymous
     *   functions and nested objects, since these statements must
     *   ultimately be linked into the top-level statement queue, but can't
     *   be linked in while they're being parsed because of their nested
     *   location in the recursive descent.  We'll throw each new nested
     *   top-level statement into this list as we parse them, then add this
     *   list to the top-level statement list when we're done with the
     *   entire program.  
     */
    class CTPNStmTop *nested_stm_head_;
    class CTPNStmTop *nested_stm_tail_;

    /*
     *   Anonymous object list.  This is a list of objects which are
     *   defined without symbol names.  
     */
    class CTcSymObj *anon_obj_head_;
    class CTcSymObj *anon_obj_tail_;

    /* 
     *   Non-symbolic object list.  This is a list of objects that are
     *   defined without symbols at all.
     */
    struct tcprs_nonsym_obj *nonsym_obj_head_;
    struct tcprs_nonsym_obj *nonsym_obj_tail_;

    /*
     *   Object template list - this is the master list of templates for the
     *   root object class.  
     */
    class CTcObjTemplate *template_head_;
    class CTcObjTemplate *template_tail_;

    /*
     *   Object template instance parsing expression array.  Each time we
     *   define a new template, we'll make sure this array is long enough
     *   for the longest defined template.  We use this list when we're
     *   parsing a template instance to keep track of the expressions in
     *   the template instance - we can't know until we have the entire
     *   list which template we're using, so we must keep track of the
     *   entire list until we reach the end of the list. 
     */
    class CTcObjTemplateInst *template_expr_;
    size_t template_expr_max_;

    /* 
     *   String template list.  String templates are unrelated to object
     *   templates; these are for custom syntax in << >> expressions in
     *   strings.  
     */
    class CTcStrTemplate *str_template_head_;
    class CTcStrTemplate *str_template_tail_;

    /* head and tail of exported symbol list */
    class CTcPrsExport *exp_head_;
    class CTcPrsExport *exp_tail_;

    /* 
     *   Flag: current code body has a local variable context object.  If
     *   this is set, we must generate code that sets up the context
     *   object on entry to the code body. 
     */
    unsigned int has_local_ctx_ : 1;

    /* local variable number of the code body's local variable context */
    int local_ctx_var_num_;

    /* array of context variable property values */
    tctarg_prop_id_t *ctx_var_props_;

    /* size of array */
    size_t ctx_var_props_size_;

    /* number of context variable property values in the list */
    size_t ctx_var_props_cnt_;

    /* 
     *   number of context variable property values assigned to the
     *   current code body 
     */
    size_t ctx_var_props_used_;

    /* next available local variable context index */
    int next_ctx_arr_idx_;

    /* reference to the current code body being parsed */
    CTcCodeBodyRef *cur_code_body_;

    /* flag: 'self' is valid in current code body */
    int self_valid_;

    /* 
     *   flag: 'self' is used (explicitly or implicitly, such as via a
     *   property reference or method call) in the current code body 
     */
    int self_referenced_;

    /* 
     *   Flag: method context beyond 'self' (targetprop, targetobj,
     *   definingobj) is referenced (explicitly or implicitly, such as via
     *   'inherited' or 'delegated') in the current code body.  
     */
    int full_method_ctx_referenced_;

    /*
     *   Flags: the local context of the outermost code body requires
     *   'self'/the full method context to be stored.  
     */
    int local_ctx_needs_self_;
    int local_ctx_needs_full_method_ctx_;

    /* next available enumerator ID */
    ulong next_enum_id_;

    /* 
     *   The '+' property - this is the property that defines the
     *   containment graph for the purposes of the '+' syntax. 
     */
    class CTcSymProp *plus_prop_;

    /*
     *   '+' property location stack.  Each time the program defines an
     *   object using the '+' notation to set the location, we'll update our
     *   record here of the last object at that depth.  Any time an object
     *   is defined at depth N (i.e., using N '+' signs), its location is
     *   set to the last object at depth N-1.  An object with no '+' signs
     *   is at depth zero.  
     */
    class CTPNStmObject **plus_stack_;
    size_t plus_stack_alloc_;

    /*
     *   The module name and sequence number, if known.  The module name is
     *   the name as it appears on the command line, makefile (.t3m), or
     *   library (.tl) file.  The sequence number is an ordinal giving its
     *   position in the list of modules making up the overall program build.
     *   We use this information in generating the sourceTextGroup object.  
     */
    char *module_name_;
    int module_seqno_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Grammar tree parser - property arrow list
 */
struct CTcGramPropArrows
{
    CTcGramPropArrows() { cnt = 0; }
    
    /* maximum number of arrows */
    static const size_t max_arrows = 100;

    /* array of property arrows */
    class CTcSymProp *prop[max_arrows];

    /* number of arrows */
    size_t cnt;
};

/* ------------------------------------------------------------------------ */
/*
 *   Grammar tree parser - compiler interface functions.  We abstract a
 *   number of functions from the compiler interface to allow for differences
 *   in behavior between regular static compilation and run-time construction
 *   of grammar rules.  
 */
class CTcGramAltFuncs
{
public:
    /* look up a property symbol, defining it if it's undefined */
    virtual class CTcSymProp *look_up_prop(
        const class CTcToken *tok, int show_err) = 0;

    /* declare a grammar production symbol */
    virtual class CTcGramProdEntry *declare_gramprod(
        const char *txt, size_t len) = 0;

    /* check the given enum for use as a production token */
    virtual void check_enum_tok(class CTcSymEnum *enumsym) = 0;

    /* handle EOF in an alternative list */
    virtual void on_eof(int *err) = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   Grammar tree node - base class 
 */
class CTcPrsGramNode: public CTcTokenSource
{
public:
    CTcPrsGramNode()
    {
        /* we have no siblings yet */
        next_ = 0;
    }

    /* get the next token - does nothing by default */
    virtual const CTcToken *get_next_token() { return 0; }

    /* consolidate OR nodes at the top of the subtree */
    virtual CTcPrsGramNode *consolidate_or() = 0;

    /* flatten CAT nodes together in the tree */
    virtual void flatten_cat() { /* by default, do nothing */ }

    /* am I an "or" node? */
    virtual int is_or() { return FALSE; }

    /* am I a "cat" node? */
    virtual int is_cat() { return FALSE; }

    /* get my token - if I'm not a token node, returns null */
    virtual const CTcToken *get_tok() const { return 0; }

    /* initialize expansion - by default, we do nothing */
    virtual void init_expansion() { }

    /* 
     *   Advance to the next expansion state.  Returns true if we 'carry' out
     *   of the current item, which means that we were already at our last
     *   state and hence are wrapping back to our first state.  Returns false
     *   if we advanced to a new state without wrapping back.
     *   
     *   By default, since normal items have only one alternative, we don't
     *   do anything but return a 'carry, since each advance takes us back to
     *   our single and initial state.  
     */
    virtual int advance_expansion() { return TRUE; }

    /* clone the current expansion subtree */
    virtual CTcPrsGramNode *clone_expansion() const = 0;

    /* next sibling node */
    CTcPrsGramNode *next_;
};



/* ------------------------------------------------------------------------ */
/*
 *   Statement termination information.  This is used for certain nested
 *   definition parsers, where a lack of termination in the nested
 *   definition is to be interpreted as being actually caused by a lack of
 *   termination of the enclosing definition. 
 */
struct tcprs_term_info
{
    /* initialize */
    void init(class CTcTokFileDesc *desc, long linenum)
    {
        /* remember the current location */
        desc_ = desc;
        linenum_ = linenum;

        /* no termination error yet */
        unterm_ = FALSE;
    }

    /* 
     *   source location where original terminator might have been - this is
     *   where we decided to go into a nested definition, so if it turns out
     *   that the definintion shouldn't have been nested after all, there
     *   was missing termination here 
     */
    class CTcTokFileDesc *desc_;
    long linenum_;

    /* 
     *   flag: termination was in fact missing in the nested definition; the
     *   nested parser sets this to relay the problem to the caller 
     */
    int unterm_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Object template list entry 
 */
class CTcObjTemplate
{
public:
    CTcObjTemplate(class CTcObjTemplateItem *item_head, size_t item_cnt)
    {
        /* remember my item list */
        items_ = item_head;
        item_cnt_ = item_cnt;

        /* not in a list yet */
        nxt_ = 0;
    }
    
    /* head of list of template items */
    class CTcObjTemplateItem *items_;

    /* number of items in the list */
    size_t item_cnt_;

    /* next template in master list of templates */
    CTcObjTemplate *nxt_;
};

/*
 *   Object template list item 
 */
class CTcObjTemplateItem
{
public:
    CTcObjTemplateItem(class CTcSymProp *prop, tc_toktyp_t tok_type,
                       int is_alt, int is_opt)
    {
        /* remember my defining information */
        prop_ = prop;
        tok_type_ = tok_type;
        is_alt_ = is_alt;
        is_opt_ = is_opt;

        /* not in a list yet */
        nxt_ = 0;
    }
    
    /* property that the item in this position defines */
    class CTcSymProp *prop_;

    /* token type of item in this position */
    tc_toktyp_t tok_type_;

    /* next item in this template's item list */
    CTcObjTemplateItem *nxt_;

    /* flag: this item is an alternative to the previous item */
    unsigned int is_alt_ : 1;

    /* flag: this item is optional */
    unsigned int is_opt_ : 1;
};

/*
 *   Template item instance - we keep track of the actual parameters to a
 *   template with these items. 
 */
class CTcObjTemplateInst
{
public:
    /* 
     *   expression value for the actual parameter, as either a naked
     *   expression (expr_) or as a code body (code_body_) - only one of
     *   expr_ or code_body_ will be valid 
     */
    class CTcPrsNode *expr_;
    class CTPNCodeBody *code_body_;
    class CTPNAnonFunc *inline_method_;

    /* 
     *   the introductory token of the parameter - if the parameter is
     *   introduced by an operator token, this will not be part of the
     *   expression 
     */
    tc_toktyp_t def_tok_;

    /* the first token of the value */
    CTcToken expr_tok_;

    /* 
     *   The property to which to assign this actual parameter value.  This
     *   isn't filled in until we match the full list to an actual template,
     *   since we don't know the meanings of the parameters until we match
     *   the actuals to an existing template in memory.  
     */
    class CTcSymProp *prop_;
};

/* ------------------------------------------------------------------------ */
/*
 *   'propertyset' definition structure.  Each property set defines a
 *   property pattern and an optional argument list for the properties within
 *   the propertyset group.  
 */
struct propset_def
{
    /* the property name pattern */
    const char *prop_pattern;
    size_t prop_pattern_len;

    /* head of list of tokens in the parameter list */
    struct propset_tok *param_tok_head;
};

/*
 *   propertyset token list entry 
 */
struct propset_tok
{
    propset_tok(const CTcToken *t)
    {
        /* copy the token */
        this->tok = *t;

        /* we're not in a list yet */
        nxt = 0;
    }

    /* the token */
    CTcToken tok;

    /* next token in the list */
    propset_tok *nxt;
};

/*
 *   Token source for parsing formal parameters using property set formal
 *   lists.  This retrieves tokens from a propertyset stack.  
 */
class propset_token_source: public CTcTokenSource
{
public:
    propset_token_source()
    {
        /* nothing in our list yet */
        nxt_tok = last_tok = 0;
    }

    /* get the next token */
    virtual const CTcToken *get_next_token()
    {
        /* if we have another entry in our list, retrieve it */
        if (nxt_tok != 0)
        {
            /* remember the token to return */
            CTcToken *ret = &nxt_tok->tok;

            /* advance our internal position to the next token */
            nxt_tok = nxt_tok->nxt;

            /* return the token */
            return ret;
        }
        else
        {
            /* we have nothing more to return */
            return 0;
        }
    }

    /* insert a token */
    void insert_token(const CTcToken *tok);

    /* insert a token based on type */
    void insert_token(tc_toktyp_t typ, const char *txt, size_t len);

    /* the next token we're to retrieve */
    propset_tok *nxt_tok;

    /* tail of our list */
    propset_tok *last_tok;
};

/* maximum propertyset nesting depth */
const size_t MAX_PROPSET_DEPTH = 10;



/* ------------------------------------------------------------------------ */
/*
 *   String template entry.  A string template defines custom syntax for
 *   embedded << >> expressions in strings.  
 */
class CTcStrTemplate
{
public:
    CTcStrTemplate()
    {
        /* initially clear the token list */
        head = tail = 0;
        cnt = 0;
        star = FALSE;

        /* no function symbol yet */
        func = 0;

        /* not in the global list yet */
        nxt = 0;
    }

    /* append a token to the list */
    void append_tok(const CTcToken *tok);

    /* the token list */
    CTcTokenEle *head, *tail;

    /* number of tokens in the list */
    int cnt;

    /* do we have a '*' token? */
    int star;

    /* the processor function symbol */
    class CTcSymFunc *func;

    /* next list element */
    CTcStrTemplate *nxt;
};

/* ------------------------------------------------------------------------ */
/*
 *   Non-symbolic object list entry 
 */
struct tcprs_nonsym_obj
{
    tcprs_nonsym_obj(tctarg_obj_id_t id)
    {
        /* remember the ID */
        id_ = id;

        /* not in a list yet */
        nxt_ = 0;
    }
    
    /* ID of this object */
    tctarg_obj_id_t id_;
    
    /* next entry in the list */
    tcprs_nonsym_obj *nxt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Dictionary property list entry.  Each time the source code defines a
 *   dictionary property, we'll make an entry in this list. 
 */
class CTcDictPropEntry
{
public:
    CTcDictPropEntry(class CTcSymProp *prop)
    {
        /* remember the property */
        prop_ = prop;

        /* not in a list yet */
        nxt_ = 0;

        /* not defined for current object yet */
        defined_ = FALSE;
    }

    /* my property */
    class CTcSymProp *prop_;

    /* next entry in list */
    CTcDictPropEntry *nxt_;

    /* flag: the current object definition includes this property */
    unsigned int defined_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Dictionary list entry.  Each dictionary object gets an entry in this
 *   list.
 */
class CTcDictEntry
{
public:
    CTcDictEntry(class CTcSymObj *sym);

    /* get/set my object file index */
    uint get_obj_idx() const { return obj_idx_; }
    void set_obj_idx(uint idx) { obj_idx_ = idx; }

    /* get my object symbol */
    class CTcSymObj *get_sym() const { return sym_; }

    /* get/set the next item in the list */
    CTcDictEntry *get_next() const { return nxt_; }
    void set_next(CTcDictEntry *nxt) { nxt_ = nxt; }

    /* add a word to the table */
    void add_word(const char *txt, size_t len, int copy,
                  tc_obj_id obj, tc_prop_id prop);

    /* write my symbol to the object file if I haven't already done so */
    void write_sym_to_obj_file(CVmFile *fp);

    /* get the hash table */
    class CVmHashTable *get_hash_table() const { return hashtab_; }

protected:
    /* enumeration callback - write to object file */
    static void enum_cb_writeobj(void *ctx, class CVmHashEntry *entry);
    
    /* associated object symbol */
    class CTcSymObj *sym_;

    /* 
     *   object file index (we use this to match up the dictionary objects
     *   when we re-load the object file) 
     */
    uint obj_idx_;

    /* next item in the dictionary list */
    CTcDictEntry *nxt_;

    /* hash table containing the word entries */
    class CVmHashTable *hashtab_;
};


/*
 *   entry in a dictionary list 
 */
struct CTcPrsDictItem
{
    CTcPrsDictItem(tc_obj_id obj, tc_prop_id prop)
    {
        obj_ = obj;
        prop_ = prop;
        nxt_ = 0;
    }

    /* object */
    tc_obj_id obj_;

    /* property */
    tc_prop_id prop_;

    /* next entry in list */
    CTcPrsDictItem *nxt_;
};

/*
 *   Parser dictionary hash table entry 
 */
class CVmHashEntryPrsDict: public CVmHashEntryCS
{
public:
    CVmHashEntryPrsDict(const char *txt, size_t len, int copy)
        : CVmHashEntryCS(txt, len, copy)
    {
        /* nothing in my list yet */
        list_ = 0;
    }

    /* add an item to my list */
    void add_item(tc_obj_id obj, tc_prop_id prop);

    /* get the list head */
    struct CTcPrsDictItem *get_list() const { return list_; }

protected:
    /* list of object/property associations with this word */
    struct CTcPrsDictItem *list_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Grammar production list entry 
 */
class CTcGramProdEntry
{
public:
    CTcGramProdEntry(class CTcSymObj *prod_obj);

    /* get my production object symbol */
    class CTcSymObj *get_prod_sym() const { return prod_sym_; }

    /* get/set the next item in the list */
    CTcGramProdEntry *get_next() const { return nxt_; }
    void set_next(CTcGramProdEntry *nxt) { nxt_ = nxt; }

    /* add an alternative */
    void add_alt(class CTcGramProdAlt *alt);

    /* get the alternative list head */
    class CTcGramProdAlt *get_alt_head() const { return alt_head_; }

    /* write to an object file */
    void write_to_obj_file(class CVmFile *fp);

    /* load from an object file */
    static void load_from_obj_file(class CVmFile *fp,
                                   const tctarg_prop_id_t *prop_xlat,
                                   const ulong *enum_xlat,
                                   class CTcSymObj *private_owner);

    /* move alternatives from my list to the given target list */
    void move_alts_to(CTcGramProdEntry *new_entry);

    /* get/set explicitly-declared flag */
    int is_declared() const { return is_declared_; }
    void set_declared(int f) { is_declared_ = f; }

protected:
    /* associated production object symbol */
    class CTcSymObj *prod_sym_;

    /* next item in the list */
    CTcGramProdEntry *nxt_;

    /* head and tail of alternative list */
    class CTcGramProdAlt *alt_head_;
    class CTcGramProdAlt *alt_tail_;

    /* 
     *   flag: this production was explicitly declared (this means that we
     *   will consider it valid at link time even if it has no alternatives
     *   defined) 
     */
    unsigned int is_declared_ : 1;
};

/*
 *   Grammar production alternative.  Each grammar production has one or
 *   more alternatives that, when matched, generate the production. 
 */
class CTcGramProdAlt
{
public:
    CTcGramProdAlt(class CTcSymObj *obj_sym, class CTcDictEntry *dict);

    /* get/set my score */
    int get_score() const { return score_; }
    void set_score(int score) { score_ = score; }

    /* get/set my badness */
    int get_badness() const { return badness_; }
    void set_badness(int badness) { badness_ = badness; }

    /* get my processor object symbol */
    class CTcSymObj *get_processor_obj() const { return obj_sym_; }

    /* get/set the next list element */
    CTcGramProdAlt *get_next() const { return nxt_; }
    void set_next(CTcGramProdAlt *nxt) { nxt_ = nxt; }

    /* add a token to my list */
    void add_tok(class CTcGramProdTok *tok);

    /* get the head of my token list */
    class CTcGramProdTok *get_tok_head() const { return tok_head_; }

    /* write to an object file */
    void write_to_obj_file(class CVmFile *fp);

    /* load from an object file */
    static CTcGramProdAlt *
        load_from_obj_file(class CVmFile *fp,
                           const tctarg_prop_id_t *prop_xlat,
                           const ulong *enum_xlat);

    /* get the dictionary in effect when the alternative was defined */
    class CTcDictEntry *get_dict() const { return dict_; }

protected:
    /* head and tail of our token list */
    class CTcGramProdTok *tok_head_;
    class CTcGramProdTok *tok_tail_;

    /* dictionary in effect when alternative was defined */
    class CTcDictEntry *dict_;

    /* the processor object associated with this alternative */
    class CTcSymObj *obj_sym_;

    /* next alternative in our production */
    CTcGramProdAlt *nxt_;

    /* score */
    int score_;

    /* badness */
    int badness_;
};

/* grammar production token types */
enum tcgram_tok_type
{
    /* unknown */
    TCGRAM_UNKNOWN,
    
    /* match a production (given by the production object) */
    TCGRAM_PROD,

    /* match a part of speech (given by the dictionary property) */
    TCGRAM_PART_OF_SPEECH,

    /* match a literal string */
    TCGRAM_LITERAL,

    /* token-type match */
    TCGRAM_TOKEN_TYPE,

    /* free-floating end-of-string */
    TCGRAM_STAR,

    /* match one of several parts of speech */
    TCGRAM_PART_OF_SPEECH_LIST
};

/*
 *   Grammar production alternative token 
 */
class CTcGramProdTok
{
public:
    CTcGramProdTok()
    {
        /* not in a list yet */
        nxt_ = 0;

        /* no type yet */
        typ_ = TCGRAM_UNKNOWN;

        /* no property association yte */
        prop_assoc_ = TCTARG_INVALID_PROP;
    }

    /* get/set my next element */
    CTcGramProdTok *get_next() const { return nxt_; }
    void set_next(CTcGramProdTok *nxt) { nxt_ = nxt; }

    /* set me to match a production object */
    void set_match_prod(class CTcSymObj *obj)
    {
        /* remember the production object */
        typ_ = TCGRAM_PROD;
        val_.obj_ = obj;
    }

    /* set me to match a token type */
    void set_match_token_type(ulong enum_id)
    {
        /* remember the token enum ID */
        typ_ = TCGRAM_TOKEN_TYPE;
        val_.enum_id_ = enum_id;
    }

    /* set me to match a dictionary property */
    void set_match_part_of_speech(tctarg_prop_id_t prop)
    {
        /* remember the part of speech */
        typ_ = TCGRAM_PART_OF_SPEECH;
        val_.prop_ = prop;
    }

    /* 
     *   set me to match a list of parts of speech; each part of speech must
     *   be separately added via add_match_part_ele() 
     */
    void set_match_part_list();

    /* add an element to the part-of-speech match list */
    void add_match_part_ele(tctarg_prop_id_t prop);

    /* set me to match a literal string */
    void set_match_literal(const char *txt, size_t len)
    {
        /* remember the string */
        typ_ = TCGRAM_LITERAL;
        val_.str_.txt_ = txt;
        val_.str_.len_ = len;
    }

    /* set me to match a free-floating end-of-string */
    void set_match_star()
    {
        /* set the type */
        typ_ = TCGRAM_STAR;
    }

    /* get my type */
    tcgram_tok_type get_type() const { return typ_; }

    /* get my value */
    class CTcSymObj *getval_prod() const { return val_.obj_; }
    tctarg_prop_id_t getval_part_of_speech() const { return val_.prop_; }
    const char *getval_literal_txt() const { return val_.str_.txt_; }
    const size_t getval_literal_len() const { return val_.str_.len_; }
    ulong getval_token_type() const { return val_.enum_id_; }
    size_t getval_part_list_len() const { return val_.prop_list_.len_; }
    tctarg_prop_id_t getval_part_list_ele(size_t idx) const
        { return val_.prop_list_.arr_[idx]; }

    /* 
     *   get/set my property association - this is the property to which
     *   the actual match to the rule is assigned when we match the rule 
     */
    tctarg_prop_id_t get_prop_assoc() const { return prop_assoc_; }
    void set_prop_assoc(tctarg_prop_id_t prop) { prop_assoc_ = prop; }

    /* write to an object file */
    void write_to_obj_file(class CVmFile *fp);

    /* load from an object file */
    static CTcGramProdTok *
        load_from_obj_file(class CVmFile *fp,
                           const tctarg_prop_id_t *prop_xlat,
                           const ulong *enum_xlat);

protected:
    /* next token in my list */
    CTcGramProdTok *nxt_;

    /* my type - this specifies how this token matches */
    tcgram_tok_type typ_;

    /* match specification - varies according to my type */
    union
    {
        /* object - for matching a production */
        class CTcSymObj *obj_;

        /* property - for matching a part of speech */
        tctarg_prop_id_t prop_;

        /* token enum id - for matching a token type */
        ulong enum_id_;

        /* literal string */
        struct
        {
            const char *txt_;
            size_t len_;
        } str_;

        /* list of vocabulary elements */
        struct
        {
            /* number of array entries allocated */
            size_t alo_;

            /* number of array entries actually used */
            size_t len_;

            /* array of entries */
            tctarg_prop_id_t *arr_;
        } prop_list_;
    } val_;

    /* property association */
    tctarg_prop_id_t prop_assoc_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Exported symbol record 
 */
class CTcPrsExport
{
public:
    /* create with the given compiler symbol */
    CTcPrsExport(const char *sym, size_t sym_len)
    {
        /* remember my name */
        sym_ = sym;
        sym_len_ = sym_len;

        /* 
         *   we don't yet have an explicit external name, so export using
         *   the internal name 
         */
        ext_name_ = sym;
        ext_len_ = sym_len;

        /* we're not in a list yet */
        nxt_ = 0;
    }

    /* set the external name */
    void set_extern_name(const char *txt, size_t len)
    {
        ext_name_ = txt;
        ext_len_ = len;
    }

    /* get the symbol name and length */
    const char *get_sym() const { return sym_; }
    size_t get_sym_len() const { return sym_len_; }

    /* get the external name and length */
    const char *get_ext_name() const { return ext_name_; }
    size_t get_ext_len() const { return ext_len_; }

    /* get/set the next entry in the list */
    CTcPrsExport *get_next() const { return nxt_; }
    void set_next(CTcPrsExport *nxt) { nxt_ = nxt; }

    /* write to an object file */
    void write_to_obj_file(class CVmFile *fp);

    /* read from an object file */
    static CTcPrsExport *read_from_obj_file(class CVmFile *fp);

    /* determine if my external name matches the given export's */
    int ext_name_matches(const CTcPrsExport *exp) const
    {
        return (exp->get_ext_len() == get_ext_len()
                && memcmp(exp->get_ext_name(), get_ext_name(),
                          get_ext_len()) == 0);
    }

    /* determine if my name matches the given string */
    int ext_name_matches(const char *txt) const
    {
        return (get_ext_len() == get_strlen(txt)
                && memcmp(get_ext_name(), txt, get_ext_len()) == 0);
    }

    /* determine if my name matches the leading substring */
    int ext_name_starts_with(const char *txt) const
    {
        return (get_ext_len() >= get_strlen(txt)
                && memcmp(get_ext_name(), txt, get_strlen(txt)) == 0);
    }

    /* determine if my symbol name matches the given export's */
    int sym_matches(const CTcPrsExport *exp) const
    {
        return (exp->get_sym_len() == get_sym_len()
                && memcmp(exp->get_sym(), get_sym(), get_sym_len()) == 0);
    }

protected:
    /* symbol name - this is the internal compiler symbol being exported */
    const char *sym_;
    size_t sym_len_;

    /* external name - this is the name visible to the VM loader */
    const char *ext_name_;
    size_t ext_len_;
    
    /* next in list */
    CTcPrsExport *nxt_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Parser Symbol Table.  The parser maintains a hierarchy of symbol
 *   tables; a local symbol table can be nested inside an enclosing
 *   scope's symbol table, and so on up to the top-level block scope,
 *   which is enclosed by the global scope.  In addition, at function
 *   scope there's a separate table for "goto" labels. 
 */

/* find_or_def actions for undefined symbols */
enum tcprs_undef_action
{
    /* if undefined, add an "undefined" entry unconditionally */
    TCPRS_UNDEF_ADD_UNDEF,

    /* add a "property" entry unconditionally, but warn about it */
    TCPRS_UNDEF_ADD_PROP,

    /* add a "property" entry unconditionally, with no warning */
    TCPRS_UNDEF_ADD_PROP_NO_WARNING,

    /* add a "weak" property entry without warning */
    TCPRS_UNDEF_ADD_PROP_WEAK
};

/* parser symbol table */
class CTcPrsSymtab
{
public:
    CTcPrsSymtab(CTcPrsSymtab *parent_scope);
    virtual ~CTcPrsSymtab();

    /* allocate parser symbol tables out of the parser memory pool */
    void *operator new(size_t siz);

    /* 
     *   perform static initialization/termination - call once at program
     *   startup and shutdown (respectively) 
     */
    static void s_init();
    static void s_terminate();

    /* get the enclosing scope's symbol table */
    CTcPrsSymtab *get_parent() const { return parent_; }

    /* find a symbol; returns null if the symbol isn't defined */
    class CTcSymbol *find(const textchar_t *sym, size_t len)
        { return find(sym, len, 0); }

    class CTcSymbol *find(const textchar_t *sym)
        { return find(sym, strlen(sym), 0); }

    /* 
     *   Find a symbol; returns null if the symbol isn't defined.  If
     *   symtab is not null, we'll fill it in with the actual symbol table
     *   in which we found the symbol; this might be an enclosing symbol
     *   table, since we search up the enclosing scope list. 
     */
    class CTcSymbol *find(const textchar_t *sym, size_t len,
                          CTcPrsSymtab **symtab);

    /* find a symbol without changing its referenced status */
    class CTcSymbol *find_noref(const textchar_t *sym, size_t len,
                                CTcPrsSymtab **symtab);

    /* 
     *   Find a symbol; if the symbol isn't defined, log an error and add
     *   the symbol as type "undefined".  Because we add a symbol entry if
     *   the symbol isn't defined, this *always* returns a valid symbol
     *   object.  
     */
    class CTcSymbol *find_or_def_undef(const char *sym, size_t len,
                                       int copy_str)
    {
        return find_or_def(sym, len, copy_str, TCPRS_UNDEF_ADD_UNDEF);
    }

    /*
     *   Find a symbol; if the symbol isn't defined, log a warning and
     *   define the symbol as type property.  Because we add an entry if
     *   the symbol isn't defined, this *always* returns a valid symbol
     *   object.  
     */
    class CTcSymbol *find_or_def_prop(const char *sym, size_t len,
                                      int copy_str)
    {
        return find_or_def(sym, len, copy_str, TCPRS_UNDEF_ADD_PROP);
    }
        
    /*
     *   Find a symbol; if the symbol isn't defined, define the symbol as
     *   type property with no warning.  This should be used when it is
     *   unambiguous that a symbol is meant as a property name.  Because we
     *   add an entry if the symbol isn't defined, this *always* returns a
     *   valid symbol object.  
     */
    class CTcSymbol *find_or_def_prop_explicit(
        const char *sym, size_t len, int copy_str)
    {
        return find_or_def(sym, len, copy_str,
                           TCPRS_UNDEF_ADD_PROP_NO_WARNING);
    }

    /*
     *   Find a symbol; if the symbol isn't defined, define it as a property
     *   without warning, but flag it as a "weak" definition. 
     */
    class CTcSymbol *find_or_def_prop_weak(
        const char *sym, size_t len, int copy_str)
    {
        return find_or_def(sym, len, copy_str, TCPRS_UNDEF_ADD_PROP_WEAK);
    }
        
    /*
     *   Find a symbol.  If the symbol isn't defined, and a "self" object
     *   is available, define the symbol as a property.  If the symbol
     *   isn't defined an no "self" object is available, add an
     *   "undefined" entry for the symbol. 
     */
    class CTcSymbol *find_or_def_prop_implied(const char *sym, size_t len,
                                              int copy_str, int is_self_avail)
    {
        return find_or_def(sym, len, copy_str,
                           is_self_avail
                           ? TCPRS_UNDEF_ADD_PROP : TCPRS_UNDEF_ADD_UNDEF);
    }

    /*
     *   Find a symbol.  If the symbol is already defined as a "weak"
     *   property, delete it to make way for the new definition. 
     */
    class CTcSymbol *find_delete_weak(const char *sym, size_t len);

    /* add a formal parameter symbol */
    class CTcSymLocal *add_formal(const textchar_t *sym, size_t len,
                                  int formal_num, int copy_str);

    /* add a local variable symbol */
    class CTcSymLocal *add_local(const textchar_t *sym, size_t len,
                                 int local_num, int copy_str,
                                 int init_assigned, int init_referenced);

    /* 
     *   add the current token as a local symbol, initially unassigned and
     *   unreferenced 
     */
    class CTcSymLocal *add_local(int local_num);

    /* add a 'goto' symbol */
    class CTcSymLabel *add_code_label(const textchar_t *sym, size_t len,
                                      int copy_str);

    /* enumerate entries in the table through a callback */
    void enum_entries(void (*func)(void *, class CTcSymbol *), void *ctx);

    /*
     *   Scan the symbol table and check for unreferenced locals.  Logs an
     *   error for each unreferenced or unassigned local.
     */
    void check_unreferenced_locals();

    /* 
     *   Get/set my debugging list index - this is the index of this table
     *   in the list for this function or method.  The index values start
     *   at 1 - a value of zero indicates that the symbol table isn't part
     *   of any list.  
     */
    int get_list_index() const { return list_index_; }
    void set_list_index(int n) { list_index_ = n; }

    /* get/set the next entry in the linked list */
    CTcPrsSymtab *get_list_next() const { return list_next_; }
    void set_list_next(CTcPrsSymtab *nxt) { list_next_ = nxt; }

    /*
     *   The low-level virtual interface.  All searching and manipulation of
     *   the underlying hash table goes through these routines.  This allows
     *   subclasses to implement the symbol table as a view of another data
     *   structure.
     *   
     *   The compiler itself just uses the underlying hash table directly.
     *   The dynamic compiler for interpreter "eval()" functionality uses a
     *   customized version that creates a view of an underlying user-code
     *   LookupTable object.  
     */

    /* find a symbol directly in this table, without searching parents */
    virtual class CTcSymbol *find_direct(const textchar_t *sym, size_t len);

    /* add an entry to the table */
    virtual void add_entry(class CTcSymbol *sym);

    /* remove an entry */
    virtual void remove_entry(class CTcSymbol *sym);

    /* expand my byte code range to include the given location */
    void add_to_range(int ofs)
    {
        /* 
         *   if we don't have a starting offset yet, or this is before it,
         *   this is the new start offset 
         */
        if (start_ofs_ == 0 || ofs < start_ofs_)
            start_ofs_ = ofs;

        /* 
         *   if we don't have an ending offset yet, or this is after it, this
         *   is the ending offset 
         */
        if (ofs > end_ofs_)
            end_ofs_ = ofs;
    }

    /* get my bytecode offset range */
    int get_start_ofs() const { return start_ofs_; }
    int get_end_ofs() const { return end_ofs_; }

protected:
    /* add an entry to a global symbol table */
    static void add_to_global_symtab(CTcPrsSymtab *tab, CTcSymbol *entry);

    /* get the underlying hash table */
    class CVmHashTable *get_hashtab() const { return hashtab_; }

    /* enumeration callback - check for unreferenced locals */
    static void unref_local_cb(void *ctx, class CTcSymbol *sym);

    /* 
     *   find a symbol, or define a new symbol, according to the given
     *   action mode, if the symbol is undefined 
     */
    class CTcSymbol *find_or_def(const textchar_t *sym, size_t len,
                                 int copy_str, tcprs_undef_action action);
    
    /* enclosing scope (parent) symbol table */
    CTcPrsSymtab *parent_;
    
    /* hash table */
    class CVmHashTable *hashtab_;

    /* hash function */
    static class CVmHashFunc *hash_func_;

    /*
     *   Byte code range covered by this frame.  This is the range of offsets
     *   within the method where this frame is in effect.  The range is
     *   inclusive of the start offset and exclusive of the end offset.  
     */
    int start_ofs_;
    int end_ofs_;

    /*
     *   Next symbol table in local scope chain.  For each function or
     *   method, we keep a simple linear list of the local scopes so that
     *   they can be written to the debugging records.  We also keep an
     *   index value giving its position in the list, so that we can store
     *   references to the table using the list index.  
     */
    CTcPrsSymtab *list_next_;
    int list_index_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Debugger symbol table interface.  This is an abstract interface that
 *   debuggers can implement to allow us to search for symbols that are
 *   obtained from a compiled program's debugger records.  To keep the
 *   compiler independent of the target architecture and the debugger's
 *   own internal structures, we define this abstract interface that the
 *   debugger must implement.
 *   
 *   Since this type of symbol table is provided by a debugger as a view
 *   on the symbol information in a previously compiled program, the
 *   parser naturally has no need to add symbols to the table; hence the
 *   only required operations are symbol lookups.  
 */
class CTcPrsDbgSymtab
{
public:
    /* 
     *   Get information on a symbol.  Returns true if the symbol is
     *   found, false if not.  If we find the symbol, fills in the
     *   information structure with the appropriate data.  
     */
    virtual int find_symbol(const textchar_t *sym, size_t len,
                            struct tcprsdbg_sym_info *info) = 0;
};

/*
 *   Debugger local symbol information structure 
 */
struct tcprsdbg_sym_info
{
    /* symbol type */
    enum tc_symtype_t sym_type;
    
    /* local/parameter number */
    uint var_id;

    /* context variable index - 0 if it's not a context local */
    int ctx_arr_idx;

    /* stack frame index */
    uint frame_idx;
};



/* ------------------------------------------------------------------------ */
/*
 *   Parse Tree storage manager.
 *   
 *   The parse tree has some special characteristics that make it
 *   desirable to use a special memory manager for it.  First, the parse
 *   tree consists of many small objects, so we would like to have as
 *   little overhead per object for memory tracking as possible.  Second,
 *   parse tree objects all have a similar lifetime: we create the entire
 *   parse tree as we scan the source, then use it to generate target
 *   code, then discard the whole thing.
 *   
 *   To manage memory efficiently for the parse tree, we define our own
 *   memory manager for parse tree objects.  The memory manager is very
 *   simple, fast, and has minimal per-object overhead.  We simply
 *   maintain a list of large blocks, then suballocate requests out of the
 *   large blocks.  Each time we run out of space in a block, we allocate
 *   a new block.  We do not keep track of any extra tracking information
 *   per node, so a node cannot be individually freed; however, the entire
 *   block list can be freed at once, which is exactly the behavior we
 *   want. 
 */
class CTcPrsMem
{
public:
    CTcPrsMem();
    ~CTcPrsMem();
    
    /* allocate storage */
    void *alloc(size_t siz);

    /* save the current pool state, for later resetting */
    void save_state(struct tcprsmem_state_t *state);

    /* 
     *   reset the pool to the given state - delete all objects allocated
     *   in the pool since the state was saved 
     */
    void reset(const struct tcprsmem_state_t *state);

    /* reset to initial state */
    void reset();

private:
    /* delete all parser memory */
    void delete_all();

    /* allocate a new block */
    void alloc_block();

    /* head of list of memory blocks */
    struct tcprsmem_blk_t *head_;

    /* tail of list and current memory block */
    struct tcprsmem_blk_t *tail_;

    /* current allocation offset in last block */
    char *free_ptr_;

    /* remaining space available in last block */
    size_t rem_;
};

/* 
 *   state-saving structure
 */
struct tcprsmem_state_t
{
    /* current tail of memory block list */
    struct tcprsmem_blk_t *tail;

    /* current allocation offset in last block */
    char *free_ptr;

    /* current remaining space in last block */
    size_t rem;
};


/*
 *   Provide an overridden operator new for allocating objects explicitly
 *   from the pool 
 */
inline void *operator new(size_t siz, CTcPrsMem *pool)
{
    return pool->alloc(siz);
}

/* 
 *   provide an array operator new as well 
 */
inline void *operator new[](size_t siz, CTcPrsMem *pool)
{
    return pool->alloc(siz);
}


/*
 *   parse tree memory block 
 */
struct tcprsmem_blk_t
{
    /* next block in the list */
    tcprsmem_blk_t *next_;

    /* 
     *   This block's byte array (the array extends off the end of the
     *   structure).
     */
    char buf_[1];
};

/* ------------------------------------------------------------------------ */
/*
 *   Special array list subclass that uses parser memory 
 */
class CPrsArrayList: public CArrayList
{
protected:
    /* 
     *   override the memory management functions to use parser memory 
     */

    virtual void *alloc_mem(size_t siz)
    {
        /* allocate from the parser pool */
        return G_prsmem->alloc(siz);
    }

    virtual void *realloc_mem(void *p, size_t oldsiz, size_t newsiz)
    {
        void *pnew;

        /* allocate a new block from the parser pool */
        pnew = G_prsmem->alloc(newsiz);

        /* copy from the old block to the new block */
        memcpy(pnew, p, oldsiz);

        /* return the new block */
        return pnew;
    }

    virtual void free_mem(void *p)
    {
        /* 
         *   do nothing - the parser pool automatically frees everything as a
         *   block when terminating the parser 
         */
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Expression Constant Value object.  This object is used to express the
 *   value of a constant expression.  
 */
class CTcConstVal
{
public:
    CTcConstVal()
    {
        /* the type is unknown */
        typ_ = TC_CVT_UNK;

        /* assume it's a true constant, not just a compile-time constant */
        ctc_ = FALSE;
    }

    /* 
     *   determine if this is a constant value - it is a constant if it
     *   has any known value 
     */
    int is_const() const { return (typ_ != TC_CVT_UNK); }

    /* 
     *   set the type to unknown - this indicates that there is no valid
     *   value, which generally means that the associated expression does
     *   not have a constant value 
     */
    void set_unknown() { typ_ = TC_CVT_UNK; }

    /* set from another value */
    void set(const CTcConstVal *val)
    {
        /* copy the type */
        typ_ = val->typ_;

        /* copy the value */
        val_ = val->val_;
    }

    /* set an integer value */
    void set_int(long val) { typ_ = TC_CVT_INT; val_.intval_ = val; }

    /* set a floating-point value */
    void set_float(const char *val, size_t len, int promoted);

    /* set a floating-point value from a vbignum_t */
    void set_float(const class vbignum_t *val, int promoted);

    /* set a floating-point value promoted from an integer */
    void set_float(ulong i);

    /* 
     *   Check to see if a promoted float value can be demoted back to int.
     *   If the value is a float that was flagged as promoted from an int
     *   constant, and the value fits in an int32, we'll demote the value
     *   back to an int.  This has no effect if the value isn't a float,
     *   wasn't promoted, or doesn't fit in an int.  This can be used after a
     *   constant-folding operation is applied to a value that was at some
     *   point promoted from int to see if the promotion is no longer
     *   required.
     */
    void demote_float();

    /* set an enumerator value */
    void set_enum(ulong val) { typ_ = TC_CVT_ENUM; val_.enumval_ = val; }

    /* set a single-quoted string value */
    void set_sstr(const char *val, size_t len);
    void set_sstr(const CTcToken *tok);

    /* set a regex string value (R'...' or R"...") */
    void set_restr(const CTcToken *tok);

    /* set a list value */
    void set_list(class CTPNList *lst);

    /* for the debugger only: set a pre-resolved constant pool value */
    void set_sstr(uint32_t ofs);
    void set_list(uint32_t ofs);

    /* set an object reference value */
    void set_obj(ulong obj, enum tc_metaclass_t meta)
    {
        typ_ = TC_CVT_OBJ;
        val_.objval_.id_ = obj;
        val_.objval_.meta_ = meta;
    }

    /* set a property pointer value */
    void set_prop(uint prop)
    {
        typ_ = TC_CVT_PROP;
        val_.propval_ = prop;
    }

    /* set a function pointer value */
    void set_funcptr(class CTcSymFunc *sym)
    {
        typ_ = TC_CVT_FUNCPTR;
        val_.funcptrval_ = sym;
    }

    /* set an anonymous function pointer value */
    void set_anon_funcptr(class CTPNCodeBody *code_body)
    {
        typ_ = TC_CVT_ANONFUNCPTR;
        val_.codebodyval_ = code_body;
    }

    /* set a built-in function pointer value */
    void set_bifptr(class CTcSymBif *sym)
    {
        typ_ = TC_CVT_BIFPTR;
        val_.bifptrval_ = sym;
    }

    /* set a nil/true value */
    void set_nil() { typ_ = TC_CVT_NIL; }
    void set_true() { typ_ = TC_CVT_TRUE; }

    /*
     *   Set a vocabulary list placeholder.  This has no actual value
     *   during compilation; instead, this is just a placeholder.  During
     *   linking, we'll replace each of these with a list of strings
     *   giving the actual vocabulary for the property. 
     */
    void set_vocab_list() { typ_ = TC_CVT_VOCAB_LIST; }

    /* set a nil/true value based on a boolean value */
    void set_bool(int val)
    {
        typ_ = (val ? TC_CVT_TRUE : TC_CVT_NIL);
    }

    /* is this a boolean value? */
    int is_bool() const { return typ_ == TC_CVT_NIL || typ_ == TC_CVT_TRUE; }

    /* get my type */
    tc_constval_type_t get_type() const { return typ_; }

    /* get my int value (no type checking) */
    long get_val_int() const { return val_.intval_; }

    /* get my floating point value (no type checking) */
    const char *get_val_float() const { return val_.floatval_.txt_; }
    size_t get_val_float_len() const { return val_.floatval_.len_; }

    /* was the value promoted to float from int due to overflow? */
    int is_promoted() const { return promoted_; }

    /* get my enumerator value (no type checking) */
    ulong get_val_enum() const { return val_.enumval_; }

    /* get my string value (no type checking) */
    const char *get_val_str() const { return val_.strval_.strval_; }
    size_t get_val_str_len() const { return val_.strval_.strval_len_; }

    /* get my list value (no type checking) */
    class CTPNList *get_val_list() const { return val_.listval_.l_; }

    /* 
     *   for debugger expressions only: the string/list as a pre-resolved
     *   constant pool address 
     */
    uint32_t get_val_str_ofs() const { return val_.strval_.pool_ofs_; }
    uint32_t get_val_list_ofs() const { return val_.listval_.pool_ofs_; }

    /* get my object reference value (no type checking) */
    ulong get_val_obj() const
        { return val_.objval_.id_; }
    enum tc_metaclass_t get_val_obj_meta() const
        { return val_.objval_.meta_; }

    /* get my property pointer value (no type checking) */
    uint get_val_prop() const { return val_.propval_; }

    /* get my function pointer symbol value (no type checking) */
    class CTcSymFunc *get_val_funcptr_sym() const
        { return val_.funcptrval_; }

    /* get my anonymous function pointer value (no type checking) */
    class CTPNCodeBody *get_val_anon_func_ptr() const
        { return val_.codebodyval_; }

    /* get my built-in function pointer symbol value (noi type checking) */
    class CTcSymBif *get_val_bifptr_sym() const
        { return val_.bifptrval_; }

    /*
     *   Determine if this value equals a given constant value.  Returns
     *   true if so, false if not.  We'll set (*can_compare) to true if
     *   the values are comparable, false if the comparison is not
     *   meaningful.  
     */
    int is_equal_to(const CTcConstVal *val) const;

    /*
     *   Convert an integer, nil, or true value to a string.  Fills in the
     *   buffer with the result of the conversion if the value wasn't
     *   already a string.  If the value is already a string, we'll simply
     *   return a pointer to the original string without making a copy.
     *   Returns null if the value is not convertible to a string.  
     */
    const char *cvt_to_str(char *buf, size_t bufl, size_t *result_len);

    /* 
     *   Get my true/nil value.  Returns false if the value is nil or zero,
     *   true if it's anything else.  
     */
    int get_val_bool() const
    {
        return !(typ_ == TC_CVT_NIL || equals_zero());
    }

    /* is this is a numeric value equal to zero? */
    int equals_zero() const;

    /*
     *   Set/get the compile-time constant flag.  A compile-time constant is
     *   a value that's constant at compile-time, but which can vary from one
     *   compilation to the next.  The defined() and __objref() operators
     *   have this property.
     */
    void set_ctc(int f) { ctc_ = f; }
    int is_ctc() const { return ctc_; }
    
private:
    /* my type */
    tc_constval_type_t typ_;

    union
    {
        /* integer value (valid when typ_ == TC_CVT_INT) */
        long intval_;

        /* floating-point value (valid when typ_ == TC_CVT_FLOAT) */
        struct
        {
            const char *txt_;
            size_t len_;
        }
        floatval_;

        /* enumerator value (valid when typ_ == TC_CVT_ENUM) */
        ulong enumval_;

        /* 
         *   String value (valid when typ_ == TC_CVT_TYPE_SSTR).  We need
         *   to know the length separately, because the underyling string
         *   may not be null-terminated.  
         */
        struct
        {
            const char *strval_;
            size_t strval_len_;

            /* 
             *   For debugger expressions only: the pre-resolved constant
             *   pool address in the live running program of a string or list
             *   expression.  This type is indicated by setting the value
             *   data type to string or list, and setting the token value
             *   pointer for that type to null.  
             */
            uint32_t pool_ofs_;
        }
        strval_;

        /* my list value */
        struct
        {
            class CTPNList *l_;

            /* for debugger expressions only: the pre-resolved pool address */
            uint32_t pool_ofs_;
        }
        listval_;

        /* property ID value */
        uint propval_;

        /* object reference value */
        struct
        {
            ulong id_;
            enum tc_metaclass_t meta_;
        }
        objval_;

        /* 
         *   function pointer value - we store the underlying symbol,
         *   since function pointers are generally not resolved until late
         *   in the compilation 
         */
        class CTcSymFunc *funcptrval_;

        /* built-in function pointer value */
        class CTcSymBif *bifptrval_;

        /* 
         *   code body pointer value - we store the underlying code body
         *   for anonymous functions 
         */
        class CTPNCodeBody *codebodyval_;
    } val_;

    /*
     *   Is this a compile-time constant value?  A compile-time constant is a
     *   value that has a fixed constant value as of compile time, but could
     *   vary from one compilation to another.  The defined() operator
     *   produces this type of constant, for example.
     *   
     *   The main distinction between a true constant and a compile-time
     *   constant is that true constants generate warnings when they produce
     *   invariant code, such as when a true constant is used as the
     *   condition of an 'if'.  Compile-time constants are specifically for
     *   producing code that's invariant once compiled, but which can vary
     *   across compilations, allowing for more sophisticated configuration
     *   management.  For example, defined() makes it possible to produce
     *   code that only gets compiled when a particular symbol is included in
     *   the build, so that code in module A can refer to symbols defined in
     *   module B when module B is included in the build, but will
     *   automatically omit the referring code when module B is omitted.
     */
    uint ctc_ : 1;

    /* 
     *   Is this a promoted value?  This is set to true for promotions from
     *   int to BigNumber that are due to overflows.  This isn't set for
     *   values explicitly entered as floating point values or that were
     *   constant-folded from expressions containing explicit floats.
     */
    uint promoted_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   Assignment Types.
 */

enum tc_asitype_t
{
    /* simple assignment: x = 1 */
    TC_ASI_SIMPLE,

    /* add to: x += 1 */
    TC_ASI_ADD,

    /* subtract from: x -= 1 */
    TC_ASI_SUB,

    /* multiply by:  x *= 1 */
    TC_ASI_MUL,

    /* divide by: x /= 1 */
    TC_ASI_DIV,

    /* modulo: x %= 1 */
    TC_ASI_MOD,

    /* bitwise-and with: x &= 1 */
    TC_ASI_BAND,

    /* bitwise-or with: x |= 1 */
    TC_ASI_BOR,

    /* bitwise-xor with: x ^= 1 */
    TC_ASI_BXOR,

    /* shift left: x <<= 1 */
    TC_ASI_SHL,

    /* arithmetic shift right: x >>= 1 */
    TC_ASI_ASHR,

    /* logical shift right: x >>>= 1 */
    TC_ASI_LSHR,

    /* pre-increment */
    TC_ASI_PREINC,

    /* pre-decrement */
    TC_ASI_PREDEC,

    /* post-increment */
    TC_ASI_POSTINC,

    /* post-decrement */
    TC_ASI_POSTDEC,

    /* 
     *   Subscript assignment: []=
     *   
     *   This isn't actually an operator in the language, but subscripted
     *   assignments are handled specially because of the way subscript
     *   assignment generates a new container value.  So a[b] = c is actually
     *   treated as a = a.operator[]=(b, c).  
     */
    TC_ASI_IDX
};


/* ------------------------------------------------------------------------ */
/*
 *   Formal parameter type list.  For functions with declared formal
 *   parameter types (such as multi-methods), we use this class to keep the
 *   list of type names in the parameter list.  
 */
class CTcFormalTypeList
{
public:
    CTcFormalTypeList()
    {
        /* no entries in our type list yet */
        head_ = tail_ = 0;

        /* assume this isn't a varargs list */
        varargs_ = FALSE;
    }

    ~CTcFormalTypeList() { }

    /* create the decorated name */
    void decorate_name(CTcToken *decorated_name,
                       const CTcToken *func_base_name);

    /* get the first parameter in the list */
    class CTcFormalTypeEle *get_first() const { return head_; }

    /* add a typed variable to the list */
    void add_typed_param(const CTcToken *tok);

    /* add an untyped variable to the list */
    void add_untyped_param();

    /* add 'n' untyped variables to the list */
    void add_untyped_params(int n)
    {
        while (n-- > 0)
            add_untyped_param();
    }

    /* add a trailing ellispsis (varargs indicator) */
    void add_ellipsis() { varargs_ = TRUE; }

protected:
    /* add a new list element */
    void add(class CTcFormalTypeEle *ele);

    /* add/tail of parameter list */
    class CTcFormalTypeEle *head_, *tail_;

    /* is this a varargs list? */
    int varargs_;
};

/* formal parameter type list entry */
class CTcFormalTypeEle
{
public:
    CTcFormalTypeEle() { name_ = 0; }
    CTcFormalTypeEle(const char *name, size_t len);
    ~CTcFormalTypeEle()
    {
    }

    /* next element in list */
    CTcFormalTypeEle *nxt_;

    /* type name */
    char *name_;
    size_t name_len_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Expression Operator Parsers.  We construct a tree of these operator
 *   parsers so that we can express the expression grammar in a relatively
 *   compact and declarative notation.  
 */

/*
 *   basic operator parser 
 */
class CTcPrsOp
{
public:
    /* 
     *   Parse an expression with this operator.  Logs an error and
     *   returns non-zero if the expression is not valid; on success,
     *   returns zero.
     *   
     *   Fills in *val with the constant value, if any, of the expression.
     *   If the expression does not have a constant value, *val's type
     *   will be set to TC_CVT_UNKNOWN to indicate this.
     *   
     *   Returns a parse node if successful, or null if an error occurs
     *   and the operator parser is unable to make a guess about what was
     *   intended.  
     */
    virtual class CTcPrsNode *parse() const = 0;
};

/*
 *   generic left-associative binary operator
 */
class CTcPrsOpBin: public CTcPrsOp
{
public:
    CTcPrsOpBin()
    {
        /* no left or right subexpression specified */
        left_ = right_ = 0;

        /* as-yet unknown operator token */
        op_tok_ = TOKT_INVALID;
    }

    CTcPrsOpBin(tc_toktyp_t typ)
    {
        /* remember my operator token */
        op_tok_ = typ;
    }

    CTcPrsOpBin(const CTcPrsOp *left, const CTcPrsOp *right, tc_toktyp_t typ)
    {
        /* remember my left and right sub-operators */
        left_ = left;
        right_ = right;

        /* remember my operator token */
        op_tok_ = typ;
    }

    /* parse the binary expression */
    class CTcPrsNode *parse() const;

    /* build a new tree out of our left-hand and right-hand subtrees */
    virtual class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const = 0;

    /* 
     *   Try evaluating a constant result.  If the two values can be
     *   combined with the operator to yield a constant value result,
     *   create a new parse node for the constant value (or update one of
     *   the given subnodes) and return it.  If we can't provide a
     *   constant value, return null.
     *   
     *   By default, we'll indicate that the expression does not have a
     *   valid constant value.  
     */
    virtual class CTcPrsNode
        *eval_constant(class CTcPrsNode *left,
                       class CTcPrsNode *right) const
    {
        /* indicate that we cannot synthesize a constant value */
        return 0;
    }

    /* get/set my token */
    tc_toktyp_t get_op_tok() const { return op_tok_; }
    void set_op_tok(tc_toktyp_t tok) { op_tok_ = tok; }
    
protected:
    /* operator that can be parsed for my left-hand side */
    const CTcPrsOp *left_;

    /* operator that can be parsed for my right-hand side */
    const CTcPrsOp *right_;

    /* my operator token */
    tc_toktyp_t op_tok_;
};

/*
 *   Binary Operator Group.  This is a group of operators at a common
 *   precedence level.  The group has an array of binary operators that
 *   are all at the same level of precedence; we'll evaluate the left
 *   suboperator, then check the token in the input stream against each of
 *   our group's operators, applying the one that matches, if one matches.
 */
class CTcPrsOpBinGroup: public CTcPrsOp
{
public:
    CTcPrsOpBinGroup(const CTcPrsOp *left, const CTcPrsOp *right,
                     const class CTcPrsOpBin *const *ops)
    {
        /* remember my left and right suboperators */
        left_ = left;
        right_ = right;
        
        /* remember the operators in my group */
        ops_ = ops;
    }

    /* parse the expression */
    class CTcPrsNode *parse() const;

protected:
    /* find and apply an operator to the parsed left-hand side */
    int find_and_apply_op(CTcPrsNode **lhs) const;
    
    /* my left and right suboperators */
    const CTcPrsOp *left_;
    const CTcPrsOp *right_;
    
    /* group of binary operators at this precedence level */
    const class CTcPrsOpBin *const *ops_;
};

/*
 *   Binary operator group for comparison operators.  This is a similar to
 *   other binary groups, but also includes the special "is in" and "not
 *   in" operators. 
 */
class CTcPrsOpBinGroupCompare: public CTcPrsOpBinGroup
{
public:
    CTcPrsOpBinGroupCompare(const class CTcPrsOp *left,
                            const class CTcPrsOp *right,
                            const class CTcPrsOpBin *const *ops)
        : CTcPrsOpBinGroup(left, right, ops)
    {
    }

    class CTcPrsNode *parse() const;

protected:
    /* parse the 'in' list portion of the expression */
    class CTPNArglist *parse_inlist() const;
};

/* comma operator */
class CTcPrsOpComma: public CTcPrsOpBin
{
public:
    CTcPrsOpComma(const CTcPrsOp *left, const CTcPrsOp *right)
        : CTcPrsOpBin(left, right, TOKT_COMMA) { }

    /* evaluate constant result */
    class CTcPrsNode
        *eval_constant(class CTcPrsNode *left,
                       class CTcPrsNode *right) const;

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;
};

/* logical OR */
class CTcPrsOpOr: public CTcPrsOpBin
{
public:
    CTcPrsOpOr(const CTcPrsOp *left, const CTcPrsOp *right)
        : CTcPrsOpBin(left, right, TOKT_OROR) { }

    /* evaluate constant result */
    class CTcPrsNode
        *eval_constant(class CTcPrsNode *left,
                       class CTcPrsNode *right) const;

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;
};

/* logical AND */
class CTcPrsOpAnd: public CTcPrsOpBin
{
public:
    CTcPrsOpAnd(const CTcPrsOp *left, const CTcPrsOp *right)
        : CTcPrsOpBin(left, right, TOKT_ANDAND) { }

    /* evaluate constant result */
    class CTcPrsNode
        *eval_constant(class CTcPrsNode *left,
                       class CTcPrsNode *right) const;

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;
};

/* general magnitude comparison operators */
class CTcPrsOpRel: public CTcPrsOpBin
{
public:
    CTcPrsOpRel(tc_toktyp_t typ) : CTcPrsOpBin(typ) { }
    
    /* evaluate constant result */
    class CTcPrsNode
        *eval_constant(class CTcPrsNode *left,
                       class CTcPrsNode *right) const;

protected:
    /*
     *   Get the result true/false value, given the result of the
     *   comparison.  For example, if this is a greater-than operator,
     *   this should return TRUE if comp > 0, FALSE otherwise.  
     */
    virtual int get_bool_val(int comparison_value) const = 0;
};

/* comparison - greater than */
class CTcPrsOpGt: public CTcPrsOpRel
{
public:
    CTcPrsOpGt() : CTcPrsOpRel(TOKT_GT) { }
    
    /* get the boolean value for a comparison sense */
    int get_bool_val(int comp) const { return comp > 0; }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;
};

/* comparison - greater than or equal to */
class CTcPrsOpGe: public CTcPrsOpRel
{
public:
    CTcPrsOpGe() : CTcPrsOpRel(TOKT_GE) { }

    /* get the boolean value for a comparison sense */
    int get_bool_val(int comp) const { return comp >= 0; }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;
};

/* comparison - less than */
class CTcPrsOpLt: public CTcPrsOpRel
{
public:
    CTcPrsOpLt() : CTcPrsOpRel(TOKT_LT) { }

    /* get the boolean value for a comparison sense */
    int get_bool_val(int comp) const { return comp < 0; }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;
};

/* comparison - less than or equal to */
class CTcPrsOpLe: public CTcPrsOpRel
{
public:
    CTcPrsOpLe() : CTcPrsOpRel(TOKT_LE) { }

    /* get the boolean value for a comparison sense */
    int get_bool_val(int comp) const { return comp <= 0; }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;
};

/*
 *   Equality/inequality comparison 
 */
class CTcPrsOpEqComp: public CTcPrsOpBin
{
public:
    CTcPrsOpEqComp(tc_toktyp_t typ) : CTcPrsOpBin(typ) { }

    /* evaluate constant result */
    class CTcPrsNode
        *eval_constant(class CTcPrsNode *left,
                       class CTcPrsNode *right) const;

protected:
    /* get the boolean value to use if the operands are equal */
    virtual int get_bool_val(int ops_equal) const = 0;
};


/*
 *   Equality comparison 
 */
class CTcPrsOpEq: public CTcPrsOpEqComp
{
public:
    /* start out in C mode - use '==' operator by default */
    CTcPrsOpEq()
        : CTcPrsOpEqComp(TOKT_EQEQ) { }

    /* set the current equality operator */
    void set_eq_op(tc_toktyp_t op) { op_tok_ = op; }
    
    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

    /* get the boolean value to use if the operands are equal */
    virtual int get_bool_val(int ops_equal) const { return ops_equal; }
};

/*
 *   Inequality comparison 
 */
class CTcPrsOpNe: public CTcPrsOpEqComp
{
public:
    CTcPrsOpNe() : CTcPrsOpEqComp(TOKT_NE) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

    /* get the boolean value to use if the operands are equal */
    virtual int get_bool_val(int ops_equal) const { return !ops_equal; }
};

/*
 *   binary arithmetic operators
 */
class CTcPrsOpArith: public CTcPrsOpBin
{
public:
    CTcPrsOpArith(tc_toktyp_t typ)
        : CTcPrsOpBin(typ) { }

    CTcPrsOpArith(const CTcPrsOp *left, const CTcPrsOp *right,
                  tc_toktyp_t typ)
        : CTcPrsOpBin(left, right, typ) { }

    /* evaluate constant result */
    class CTcPrsNode
        *eval_constant(class CTcPrsNode *left,
                       class CTcPrsNode *right) const;

protected:
    /* calculate the result of the operand applied to constant int values */
    virtual long calc_result(long val1, long val2, int &ov) const = 0;

    /* calculate the result for constant float values */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const = 0;
};

/* bitwise OR */
class CTcPrsOpBOr: public CTcPrsOpArith
{
public:
    CTcPrsOpBOr(const CTcPrsOp *left, const CTcPrsOp *right)
        : CTcPrsOpArith(left, right, TOKT_OR) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

protected:
    /* calculate a constant integer result */
    virtual long calc_result(long val1, long val2, int &ov) const
    {
        ov = FALSE;
        return val1 | val2;
    }

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const
    {
        G_tok->log_error(TCERR_BAD_OP_FOR_FLOAT);
        return 0;
    }
};

/* bitwise XOR */
class CTcPrsOpBXor: public CTcPrsOpArith
{
public:
    CTcPrsOpBXor(const CTcPrsOp *left, const CTcPrsOp *right)
        : CTcPrsOpArith(left, right, TOKT_XOR) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

protected:
    /* calculate the result */
    virtual long calc_result(long val1, long val2, int &ov) const
    {
        ov = FALSE;
        return val1 ^ val2;
    }

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const
    {
        G_tok->log_error(TCERR_BAD_OP_FOR_FLOAT);
        return 0;
    }
};

/* bitwise AND */
class CTcPrsOpBAnd: public CTcPrsOpArith
{
public:
    CTcPrsOpBAnd(const CTcPrsOp *left, const CTcPrsOp *right)
        : CTcPrsOpArith(left, right, TOKT_AND) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

protected:
    /* calculate the result */
    virtual long calc_result(long val1, long val2, int &ov) const
    {
        ov = FALSE;
        return val1 & val2;
    }

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const
    {
        G_tok->log_error(TCERR_BAD_OP_FOR_FLOAT);
        return 0;
    }
};

/*
 *   shift left 
 */
class CTcPrsOpShl: public CTcPrsOpArith
{
public:
    CTcPrsOpShl() : CTcPrsOpArith(TOKT_SHL) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

protected:
    long calc_result(long a, long b, int &ov) const
    {
        ov = FALSE;
        return a << b;
    }

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const
    {
        G_tok->log_error(TCERR_BAD_OP_FOR_FLOAT);
        return 0;
    }
};

/*
 *   arithmetic shift right
 */
class CTcPrsOpAShr: public CTcPrsOpArith
{
public:
    CTcPrsOpAShr() : CTcPrsOpArith(TOKT_ASHR) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

protected:
    long calc_result(long a, long b, int &ov) const
    {
        ov = FALSE;
        return t3_ashr(a, b);
    }

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const
    {
        G_tok->log_error(TCERR_BAD_OP_FOR_FLOAT);
        return 0;
    }
};

/*
 *   logical shift right 
 */
class CTcPrsOpLShr: public CTcPrsOpArith
{
public:
    CTcPrsOpLShr() : CTcPrsOpArith(TOKT_LSHR) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

protected:
    /* calculate a constant integer result */
    long calc_result(long a, long b, int &ov) const
    {
        ov = FALSE;
        return t3_lshr(a, b);
    }

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const
    {
        G_tok->log_error(TCERR_BAD_OP_FOR_FLOAT);
        return 0;
    }
};

/*
 *   multiply
 */
class CTcPrsOpMul: public CTcPrsOpArith
{
public:
    CTcPrsOpMul() : CTcPrsOpArith(TOKT_TIMES) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

protected:
    /* calculate a constant integer result */
    long calc_result(long a, long b, int &ov) const
    {
        int64_t prod = a * b;
        ov = (prod > (int64_t)INT32MAXVAL || prod < (int64_t)INT32MINVAL);
        return (long)prod;
    }

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const;
};

/*
 *   divide
 */
class CTcPrsOpDiv: public CTcPrsOpArith
{
public:
    CTcPrsOpDiv()
        : CTcPrsOpArith(TOKT_DIV) { }

    CTcPrsOpDiv(tc_toktyp_t tok)
        : CTcPrsOpArith(tok) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;
    
protected:
    /* calculate a constant integer result */
    long calc_result(long a, long b, int &ov) const;

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const;
};


/*
 *   mod - inherit from divide operator to pick up divide-by-zero checking 
 */
class CTcPrsOpMod: public CTcPrsOpDiv
{
public:
    CTcPrsOpMod() : CTcPrsOpDiv(TOKT_MOD) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

protected:
    /* calculate a constant integer result */
    long calc_result(long a, long b, int &ov) const;

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const;
};

/*
 *   add 
 */
class CTcPrsOpAdd: public CTcPrsOpArith
{
public:
    CTcPrsOpAdd() : CTcPrsOpArith(TOKT_PLUS) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

    /* evaluate constant result */
    class CTcPrsNode
        *eval_constant(class CTcPrsNode *left,
                       class CTcPrsNode *right) const;

protected:
    /* calculate a constant integer result */
    long calc_result(long a, long b, int &ov) const
    {
        int32_t sum = (int32_t)(a + b);
        ov = (a >= 0 ? b > 0 && sum < a : b < 0 && sum > a);
        return sum;
    }

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const;
};

/*
 *   subtract
 */
class CTcPrsOpSub: public CTcPrsOpArith
{
public:
    CTcPrsOpSub() : CTcPrsOpArith(TOKT_MINUS) { }

    /* build a new tree out of our left-hand and right-hand subtrees */
    class CTcPrsNode
        *build_tree(class CTcPrsNode *left,
                    class CTcPrsNode *right) const;

    /* evaluate constant result */
    class CTcPrsNode
        *eval_constant(class CTcPrsNode *left,
                       class CTcPrsNode *right) const;

protected:
    /* calculate a constant integer result */
    long calc_result(long a, long b, int &ov) const
    {
        int32_t diff = (int32_t)(a - b);
        ov = (a >= 0 ? b < 0 && diff < a : b > 0 && diff > a);
        return diff;
    }

    /* calculate a constant float result */
    virtual class vbignum_t *calc_result(
        const class vbignum_t &a, const class vbignum_t &b) const;
};

/*
 *   Unary Operators 
 */
class CTcPrsOpUnary: public CTcPrsOp
{
public:
    class CTcPrsNode *parse() const;

    /* 
     *   evaluate a constant subscript expression; returns a constant
     *   parse node expression if the subscript can be evaluated to a
     *   compile-time constant, or null if not 
     */
    static class CTcPrsNode
        *eval_const_subscript(class CTcPrsNode *lhs,
                              class CTcPrsNode *subscript);

    /* 
     *   evaluate a constant NOT expression; returns a constant parse node
     *   expression if the logical negation can be evaluated to a
     *   compile-time constant, or null if not 
     */
    static class CTcPrsNode *eval_const_not(class CTcPrsNode *lhs);

    /* parse a string with embedded expressions */
    static class CTcPrsNode *parse_embedding(struct CTcEmbedBuilder *builder);

    /* parse a list */
    static class CTcPrsNode *parse_list();

    /* parse a primary expression */
    static class CTcPrsNode *parse_primary();

    /* parse an anonymous function */
    static class CTPNAnonFunc *parse_anon_func(int short_form, int is_method);

    /* parse an in-line object definition */
    static class CTPNInlineObject *parse_inline_object(int has_colon);

protected:
    /* embedded expression: parse an expression list */
    static CTcPrsNode *parse_embedding_list(
        struct CTcEmbedBuilder *b, int &eos, struct CTcEmbedLevel *parent);

    /* embedded expression: parse an <<if>> or <<unless>> embedding */
    static CTcPrsNode *parse_embedded_if(
        struct CTcEmbedBuilder *b, int unless,
        int &eos, struct CTcEmbedLevel *parent);

    /* parse a single embedded expression */
    static CTcPrsNode *parse_embedded_expr(
        struct CTcEmbedBuilder *b, class CTcEmbedTokenList *tl);

    /* embedded expression: parse an <<one of>> embedding */
    static CTcPrsNode *parse_embedded_oneof(
        struct CTcEmbedBuilder *b, int &eos, struct CTcEmbedLevel *parent);

    /* embedded expression: parse an <<first time>>...<<only>> embedding */
    static CTcPrsNode *parse_embedded_firsttime(
        struct CTcEmbedBuilder *b, int &eos, struct CTcEmbedLevel *parent);

    /* create the parse node for a <<one of>> structure */
    static CTcPrsNode *create_oneof_node(
        struct CTcEmbedBuilder *b, class CTPNList *lst, const char *attrs);

    /* capture an embedded expression to a saved token list */
    static void capture_embedded(struct CTcEmbedBuilder *b,
                                 class CTcEmbedTokenList *tl);

    /* 
     *   Match an end token for an embedded expression construct - <<end>>,
     *   <<else>>, etc.  This version parses captured tokens.
     */
    static int parse_embedded_end_tok(class CTcEmbedTokenList *tl,
                                      struct CTcEmbedLevel *parent,
                                      const char **open_kw);

    /* parse an end token directly from the token stream */
    static int parse_embedded_end_tok(struct CTcEmbedBuilder *b,
                                      struct CTcEmbedLevel *parent,
                                      const char **open_kw);

    /* parse a logical NOT operator */
    static class CTcPrsNode *parse_not(CTcPrsNode *sub);
    
    /* parse a bitwise NOT operator */
    static class CTcPrsNode *parse_bnot(CTcPrsNode *sub);

    /* parse an address-of operator */
    class CTcPrsNode *parse_addr() const;

    /* parse an arithmetic positive operator */
    static class CTcPrsNode *parse_pos(CTcPrsNode *sub);

    /* parse an arithmetic negative operator */
    static class CTcPrsNode *parse_neg(CTcPrsNode *sub);

    /* parse a pre- or post-increment operator */
    static class CTcPrsNode *parse_inc(int pre, CTcPrsNode *sub);

    /* parse a pre- or post-decrement operator */
    static class CTcPrsNode *parse_dec(int pre, CTcPrsNode *sub);

    /* parse a 'new' operator */
    static class CTcPrsNode *parse_new(CTcPrsNode *sub, int is_transient);

    /* parse a 'delete' operator */
    static class CTcPrsNode *parse_delete(CTcPrsNode *sub);

    /* parse a postfix expression */
    static class CTcPrsNode *parse_postfix(int allow_member_expr,
                                           int allow_call_expr);

    /* parse a function or method call */
    static class CTcPrsNode *parse_call(CTcPrsNode *lhs);

    /* parse an argument list */
    static class CTPNArglist *parse_arg_list();

    /* parse a subscript */
    static class CTcPrsNode *parse_subscript(CTcPrsNode *lhs);

    /* parse a member selection ('.' operator) */
    static class CTcPrsNode *parse_member(CTcPrsNode *lhs);

    /* parse an "inherited" expression */
    static class CTcPrsNode *parse_inherited();

    /* parse a "delegated" expression */
    static class CTcPrsNode *parse_delegated();
};

/*
 *   if-nil operator ?? 
 */
class CTcPrsOpIfnil: public CTcPrsOp
{
public:
    CTcPrsOpIfnil() { }
    class CTcPrsNode *parse() const;
};

/*
 *   tertiary conditional operator 
 */
class CTcPrsOpIf: public CTcPrsOp
{
public:
    CTcPrsOpIf() { }
    class CTcPrsNode *parse() const;
};

/*
 *   Assignment operators (including the regular assignment, "="/":=",
 *   plus all calculate-and-assign operators: "+=", "-=", etc) 
 */
class CTcPrsOpAsi: public CTcPrsOp
{
public:
    CTcPrsOpAsi()
    {
        /* start out with the C-mode simple assignment operator */
        asi_op_ = TOKT_EQ;
    }

    /* parse an assignment */
    class CTcPrsNode *parse() const;

    /* set the current simple assignment operator */
    void set_asi_op(tc_toktyp_t tok) { asi_op_ = tok; }

private:
    /* current simple assignment operator */
    tc_toktyp_t asi_op_;
};

#endif /* TCPRS_H */

