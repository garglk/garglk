/* $Header: d:/cvsroot/tads/tads3/TCERRNUM.H,v 1.5 1999/07/11 00:46:53 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcerrnum.h - T3 Compiler Error Numbers
Function

Notes
  All T3 Compiler error numbers are in the range 10000-19999.
Modified
  04/15/99 MJRoberts  - Creation
*/

#ifndef TCERRNUM_H
#define TCERRNUM_H


/* ------------------------------------------------------------------------ */
/*
 *   Tokenizer and Preprocessor Errors 
 */

/* out of memory for source line - source line is too long */
#define TCERR_LINE_MEM            10001

/* invalid preprocessor directive "%.*s" */
#define TCERR_INV_PP_DIR          10002

/* unable to load character set specified in #charset directive */
#define TCERR_CANT_LOAD_CHARSET   10003

/* 
 *   unexpected or invalid #charset - directive must be at the very
 *   beginning of the file, and must specify a character set name enclosed
 *   in double quotes 
 */
#define TCERR_UNEXPECTED_CHARSET  10004

/* invalid #include syntax - required '"' or '< >' around filename */
#define TCERR_BAD_INC_SYNTAX      10005

/* #include file "%.*s" not found */
#define TCERR_INC_NOT_FOUND       10006

/* #include file "%.*s" previously included; redundant inclusion ignored */
#define TCERR_REDUNDANT_INCLUDE   10007

/* invalid symbol "%.*s" for #define */
#define TCERR_BAD_DEFINE_SYM      10008

/* out of memory for token text */
#define TCERR_NO_MEM_TOKEN        10009

/* missing ')' in macro parameter list */
#define TCERR_MACRO_NO_RPAR       10010

/* invalid macro parameter name "%.*s" */
#define TCERR_BAD_MACRO_ARG_NAME  10011

/* expected comma or ')' in macro parameter list */
#define TCERR_MACRO_EXP_COMMA     10012

/* redefinition of macro "%.*s" */
#define TCERR_MACRO_REDEF         10013

/* unrecognized pragma "%.*s" */
#define TCERR_UNKNOWN_PRAGMA      10014

/* pragma syntax error */
#define TCERR_BAD_PRAGMA_SYNTAX   10015

/* extra characters after end of preprocessor directive */
#define TCERR_PP_EXTRA            10016

/* #if nesting too deep */
#define TCERR_IF_NESTING_OVERFLOW 10017

/* #else without #if */
#define TCERR_PP_ELSE_WITHOUT_IF  10018

/* #endif without #if */
#define TCERR_PP_ENDIF_WITHOUT_IF 10019

/* #elif without #if */
#define TCERR_PP_ELIF_WITHOUT_IF  10020

/* numeric value required in preprocessor constant expression */
#define TCERR_PP_INT_REQUIRED     10021

/* incompatible types for comparison in preprocessor constant expression */
#define TCERR_PP_INCOMP_TYPES     10022

/* extra characters after end of preprocessor constant expression */
#define TCERR_PP_EXPR_EXTRA       10023

/* division by zero in preprocessor constant expression */
#define TCERR_PP_DIV_ZERO         10024

/* 
 *   expected number, symbol, or single-quoted string in preprocessor
 *   constant expression, got "%.*s"
 */
#define TCERR_PP_INVALID_VALUE    10025

/* unterminated string */
#define TCERR_PP_UNTERM_STRING    10026

/* unmatched left parenthesis in preprocessor constant expression */
#define TCERR_PP_UNMATCHED_LPAR   10027

/* number or true/nil required for "!" operator in preprocessor expression */
#define TCERR_PP_BAD_NOT_VAL      10028

/* missing right paren in macro expansion (must be on same line for #) */
#define TCERR_PP_MACRO_ARG_RPAR_1LINE 10029

/* argument list must be provided in macro invocation */
#define TCERR_PP_NO_MACRO_ARGS    10030

/* not enough arguments in macro invocation */
#define TCERR_PP_FEW_MACRO_ARGS   10031

/* no close paren in macro argument list */
#define TCERR_PP_MACRO_ARG_RPAR   10032

/* too many arguments in macro invocation */
#define TCERR_PP_MANY_MACRO_ARGS  10033

/* symbol required for defined() preprocessor operator */
#define TCERR_PP_DEFINED_NO_SYM   10034

/* missing ')' in defined() preprocess operator */
#define TCERR_PP_DEFINED_RPAR     10035

/* symbol "%.*s" too long; truncated to "%.*s" */
#define TCERR_SYMBOL_TRUNCATED    10036

/* too many formal parameters defined for macro (maximum %d) */
#define TCERR_TOO_MANY_MAC_PARMS  10037

/* out of memory for string buffer */
#define TCERR_NO_STRBUF_MEM       10038

/* unable to open source file "%.*s" */
#define TCERR_CANT_OPEN_SRC       10039

/* #error - %.*s */
#define TCERR_ERROR_DIRECTIVE     10040

/* out of memory for macro expansion */
#define TCERR_OUT_OF_MEM_MAC_EXP  10041

/* integer value required for line number in #line */
#define TCERR_LINE_REQ_INT        10042

/* string value required for filename in #line */
#define TCERR_LINE_FILE_REQ_STR   10043

/* #if without #endif at line %ld in file %s */
#define TCERR_IF_WITHOUT_ENDIF    10044

/* internal error: unsplicing not from current line */
#define TCERR_UNSPLICE_NOT_CUR    10045

/* internal error: unsplicing more than once in same line */
#define TCERR_MULTI_UNSPLICE      10046

/* #elif not in same file as #if */
#define TCERR_PP_ELIF_NOT_IN_SAME_FILE 10047

/* #else not in same file as #if */
#define TCERR_PP_ELSE_NOT_IN_SAME_FILE 10048

/* #endif not in same file as #if */
#define TCERR_PP_ENDIF_NOT_IN_SAME_FILE 10049

/* cannot #define "defined" - symbol reserved as preprocessor operator */
#define TCERR_REDEF_OP_DEFINED    10050

/* string appears unterminated (';' or '}' appears alone on a line) */
#define TCERR_POSSIBLE_UNTERM_STR 10051

/* source line too long after macro expansion */
#define TCERR_SRCLINE_TOO_LONG    10052

/* invalid character */
#define TCERR_INVALID_CHAR        10053

/* ":" missing in "?" expression */
#define TCERR_PP_QC_MISSING_COLON 10054

/* preprocessor expression is not a constant value */
#define TCERR_PP_EXPR_NOT_CONST   10055

/* unable to load default character set "%s" */
#define TCERR_CANT_LOAD_DEFAULT_CHARSET 10056

/* '...' in macro formal parameter list must be followed by ')' */
#define TCERR_MACRO_ELLIPSIS_REQ_RPAR 10057

/* #foreach nesting too deep */
#define TCERR_PP_FOREACH_TOO_DEEP 10058

/* trailing whitespace after backslash */
#define TCERR_TRAILING_SP_AFTER_BS 10059

/* extraneous characters after #include filename */
#define TCERR_EXTRA_INC_SYNTAX    10060

/* nested comment? */
#define TCERR_NESTED_COMMENT      10061

/* unmappable character */
#define TCERR_UNMAPPABLE_CHAR     10062

/* decimal digit found in octal constant */
#define TCERR_DECIMAL_IN_OCTAL    10063

/* '<>' operator is obsolete */
#define TCERR_LTGT_OBSOLETE       10064

/* integer constant exceeds maximum value (promoting to BigNumber) */
#define TCERR_INT_CONST_OV        10065

/* unrecognized escape sequence \%c in string */
#define TCERR_BACKSLASH_SEQ       10066

/* non-ASCII character in symbol */
#define TCERR_NON_ASCII_SYMBOL    10067

/* embedded expressions in string nested too deeply */
#define TCERR_EMBEDDING_TOO_DEEP  10068


/* ------------------------------------------------------------------------ */
/*
 *   General Messages 
 */

/* internal error explanation */
#define TCERR_INTERNAL_EXPLAN     10500

/* 
 *   internal error - after we log an internal error, we'll throw this
 *   generic error code; since internal errors are unrecoverable, any
 *   frames that catch this error should simply clean up and terminate as
 *   gracefully as possible 
 */
#define TCERR_INTERNAL_ERROR      10501

/*
 *   fatal error - after we log a fatal error, we'll throw this generic
 *   error code; any frames that catch this error should simply clean up
 *   and terminate 
 */
#define TCERR_FATAL_ERROR         10502


/* ------------------------------------------------------------------------ */
/*
 *   "Make" errors 
 */

/* cannot create symbol file "%s" */
#define TCERR_MAKE_CANNOT_CREATE_SYM 10600

/* cannot create object file "%s" */
#define TCERR_MAKE_CANNOT_CREATE_OBJ 10601

/* cannot create image file "%s" */
#define TCERR_MAKE_CANNOT_CREATE_IMG 10602

/* cannot open symbol file "%s" for reading */
#define TCERR_MAKE_CANNOT_OPEN_SYM 10603

/* cannot open object file "%s" for reading */
#define TCERR_MAKE_CANNOT_OPEN_OBJ 10604

/* cannot open image file "%s" */
#define TCERR_MAKE_CANNOT_OPEN_IMG 10605

/* maximum error count exceeded */
#define TCERR_TOO_MANY_ERRORS      10606

/* same name used for more than one source module */
#define TCERR_MODULE_NAME_COLLISION   10607

/* same name used for module as for module from a given library */
#define TCERR_MODULE_NAME_COLLISION_WITH_LIB  10608

/* "source-file-name (from library library-name)" */
#define TCERR_SOURCE_FROM_LIB      10609

/* cannot create directory "%s" */
#define TCERR_CANNOT_CREATE_DIR    10610


/* ------------------------------------------------------------------------ */
/*
 *   Parsing Errors 
 */

/* divide by zero in constant expression */
#define TCERR_CONST_DIV_ZERO      11001

/* out of memory for parse tree block */
#define TCERR_NO_MEM_PRS_TREE     11002

/* parse tree node too large (size = %ld) (internal error) */
#define TCERR_PRS_BLK_TOO_BIG     11003

/* invalid lvalue (operator "%s") */
#define TCERR_INVALID_LVALUE      11004

/* expected ':' in '?' operator expression */
#define TCERR_QUEST_WITHOUT_COLON 11005

/* invalid lvalue for unary operator (++ or --) */
#define TCERR_INVALID_UNARY_LVALUE 11006

/* operator delete is obsolete */
#define TCERR_DELETE_OBSOLETE     11007

/* can't take address of expression */
#define TCERR_NO_ADDRESS          11008

/* invalid constant type for unary '%s': number required */
#define TCERR_CONST_UNARY_REQ_NUM 11009

/* invalid constant type for binary '%s': number required */
#define TCERR_CONST_BINARY_REQ_NUM 11010

/* incompatible constant types for '+' operator */
#define TCERR_CONST_BINPLUS_INCOMPAT 11011

/* missing ')' in expression */
#define TCERR_EXPR_MISSING_RPAR   11012

/* invalid primary expression */
#define TCERR_BAD_PRIMARY_EXPR    11013

/* constant types are not comparable */
#define TCERR_CONST_BAD_COMPARE   11014

/* expected ';', found "%.*s" */
#define TCERR_EXPECTED_SEMI       11015

/* expected ">>" and the continuation of the string, found "%.*s" */
#define TCERR_EXPECTED_STR_CONT   11016

/* expected ',' in argument list, found "%.*s" */
#define TCERR_EXPECTED_ARG_COMMA  11017

/* expected ']' in subscript, found "%.*s" */
#define TCERR_EXPECTED_SUB_RBRACK 11018

/* invalid expression after '.' - property name or expression required */
#define TCERR_INVALID_PROP_EXPR   11019

/* found "%.*s" in list, assuming that list is missing ']' */
#define TCERR_LIST_MISSING_RBRACK 11020

/* extraneous ')' in list */
#define TCERR_LIST_EXTRA_RPAR     11021

/* expected ',' separating list elements, but found "%.*s" */
#define TCERR_LIST_EXPECT_COMMA   11022

/* constant list index value must be an integer value */
#define TCERR_CONST_IDX_NOT_INT   11023

/* constant list index out of range */
#define TCERR_CONST_IDX_RANGE     11024

/* unterminated string (string started with %c%*.s%c) */
#define TCERR_UNTERM_STRING       11025

/* missing ')' in argument list */
#define TCERR_EXPECTED_ARG_RPAR   11026

/* extra ')' - ignoring */
#define TCERR_EXTRA_RPAR          11027

/* extra ']' - ignoring */
#define TCERR_EXTRA_RBRACK        11028

/* expected operand, found '%s' */
#define TCERR_EXPECTED_OPERAND    11029

/* expected property name pattern string after 'propertyset', found "%.*s" */
#define TCERR_PROPSET_REQ_STR     11030

/* invalid "inherited class" syntax - expected '.', found "%.*s" */
#define TCERR_INH_CLASS_SYNTAX    11031

/* undefined symbol "%.*s" */
#define TCERR_UNDEF_SYM           11032

/* undefined symbol "%.*s" - assuming that it's a property */
#define TCERR_ASSUME_SYM_PROP     11033

/* incompatible constant types for '-' operator */
#define TCERR_CONST_BINMINUS_INCOMPAT 11034

/* expected a symbol in formal parameter list, found "%.*s" */
#define TCERR_REQ_SYM_FORMAL      11035

/* expected a comma in formal parameter list, found "%.*s" */
#define TCERR_REQ_COMMA_FORMAL    11036

/* missing parameter at end of formal parameter list */
#define TCERR_MISSING_LAST_FORMAL 11037

/* missing right paren at end of formal parameter list (found "%.*s") */
#define TCERR_MISSING_RPAR_FORMAL 11038

/* formal parameter "%.*s" defined more than once */
#define TCERR_FORMAL_REDEF        11039

/* '=' in method definition is obsolete */
#define TCERR_EQ_WITH_METHOD_OBSOLETE 11040

/* '{' required at the start of code body - found "%.*s" */
#define TCERR_REQ_LBRACE_CODE     11041

/* unexpected end of file - '}' required */
#define TCERR_EOF_IN_CODE         11042

/* expected '(' after 'if', but found "%.*s" */
#define TCERR_REQ_LPAR_IF         11043

/* misplaced 'else' */
#define TCERR_MISPLACED_ELSE      11044

/* misplaced 'case' */
#define TCERR_MISPLACED_CASE      11045

/* misplaced 'default' */
#define TCERR_MISPLACED_DEFAULT   11046

/* ellipsis must be the last formal parameter */
#define TCERR_ELLIPSIS_NOT_LAST   11047

/* expected ',' or ';' after symbol, but found "%.*s" */
#define TCERR_LOCAL_REQ_COMMA     11048

/* expected symbol name in local variable definition, but found "%.*s" */
#define TCERR_LOCAL_REQ_SYM       11049

/* expected symbol name in function defintion */
#define TCERR_FUNC_REQ_SYM        11050

/* expected code body */
#define TCERR_REQ_CODE_BODY       11051

/* expected function or object definition */
#define TCERR_REQ_FUNC_OR_OBJ     11052

/* expected ';' or expression after 'return', but found "%.*s" */
#define TCERR_RET_REQ_EXPR        11053

/* unreachable code */
#define TCERR_UNREACHABLE_CODE    11054

/* code has 'return' statements both with and without values */
#define TCERR_RET_VAL_AND_VOID    11055

/* code has value returns but also falls off end */
#define TCERR_RET_VAL_AND_IMP_VOID 11056

/* function set name expected after 'intrinsic' keyword, but found "%.*s" */
#define TCERR_REQ_INTRINS_NAME    11057

/* open brace required ater intrinsic function set name */
#define TCERR_REQ_INTRINS_LBRACE  11058

/* end of file in 'intrinsic' function set list */
#define TCERR_EOF_IN_INTRINS      11059

/* missing open paren after function name in 'instrinsic' - found "%.*s" */
#define TCERR_REQ_INTRINS_LPAR    11060

/* expected function name in 'intrinsic' list, but found "%.*s" */
#define TCERR_REQ_INTRINS_SYM     11061

/* expected '(' after 'for', but found "%.*s" */
#define TCERR_REQ_FOR_LPAR        11062

/* local variable "%.*s" is already defined in this scope */
#define TCERR_LOCAL_REDEF         11063

/* initializer is required for "local" in "for" - found "%.*s" */
#define TCERR_REQ_FOR_LOCAL_INIT  11064

/* missing expression after ',' in "for" initializer */
#define TCERR_MISSING_FOR_INIT_EXPR 11065

/* missing expression in "for" statement */
#define TCERR_MISSING_FOR_PART    11066

/* ',' or ';' expected in "for" initializer, but found "%.*s" */
#define TCERR_REQ_FOR_INIT_COMMA  11067

/* ';' expected in "for" after condition expression, but found "%.*s" */
#define TCERR_REQ_FOR_COND_SEM    11068

/* missing ')' at end of 'for' statement expression list */
#define TCERR_REQ_FOR_RPAR        11069

/* "for" condition is always false - reinitializer and body are unreachable */
#define TCERR_FOR_COND_FALSE      11070

/* '(' missing after 'while' - found "%.*s" instead */
#define TCERR_REQ_WHILE_LPAR      11071

/* ')' missing at end of 'while' expression - found "%.*s" instead */
#define TCERR_REQ_WHILE_RPAR      11072

/* "while" condition is always false - body is unreachable */
#define TCERR_WHILE_COND_FALSE    11073

/* "while" missing in "do" statement (found "%.*s" instead) */
#define TCERR_REQ_DO_WHILE        11074

/* misplaced "catch" - not part of "try" */
#define TCERR_MISPLACED_CATCH     11075

/* misplaced "finally" - not part of "try" */
#define TCERR_MISPLACED_FINALLY   11076

/* '(' missing after 'switch' - found "%.*s" instead */
#define TCERR_REQ_SWITCH_LPAR     11077

/* ')' missing at end of 'switch' expression - found "%.*s" instead */
#define TCERR_REQ_SWITCH_RPAR     11078

/* '{' missing after 'switch' expression - found "%.*s" instead */
#define TCERR_REQ_SWITCH_LBRACE   11079

/* code preceding first 'case' or 'default' label in switch is not allowed */
#define TCERR_UNREACHABLE_CODE_IN_SWITCH 11080

/* end of file in switch body */
#define TCERR_EOF_IN_SWITCH       11081

/* duplicate code label "%.*s" */
#define TCERR_CODE_LABEL_REDEF    11082

/* ':' missing after 'case' expression - found "%.*s" instead */
#define TCERR_REQ_CASE_COLON      11083

/* 'case' expression has a non-constant value */
#define TCERR_CASE_NOT_CONSTANT   11084

/* ':' missing after 'default' - found "%.*s" instead */
#define TCERR_REQ_DEFAULT_COLON   11085

/* switch already has 'default' label */
#define TCERR_DEFAULT_REDEF       11086

/* 'try' has no 'catch' or 'finally' clauses */
#define TCERR_TRY_WITHOUT_CATCH   11087

/* '(' required after 'catch' - found "%.*s" instead */
#define TCERR_REQ_CATCH_LPAR      11088

/* ')' required after 'catch' variable name - found "%.*s" instead */
#define TCERR_REQ_CATCH_RPAR      11089

/* exception class name symbol required in 'catch' - found "%.*s" instead */
#define TCERR_REQ_CATCH_CLASS     11090

/* variable name symbol required in 'catch' - found "%.*s" instead */
#define TCERR_REQ_CATCH_VAR       11091

/* 'catch' target variable "%.*s" is not a local variable */
#define TCERR_CATCH_VAR_NOT_LOCAL 11092

/* label expected after 'break' - found "%.*s" */
#define TCERR_BREAK_REQ_LABEL     11093

/* label expected after 'continue' - found "%.*s" */
#define TCERR_CONT_REQ_LABEL      11094

/* label expected after 'goto' - found "%.*s" */
#define TCERR_GOTO_REQ_LABEL      11095

/* symbol "%.*s" is already defined - can't use as a new function name */
#define TCERR_REDEF_AS_FUNC       11096

/* expected 'function' or 'object' after 'extern', but found '%.*s' */
#define TCERR_INVAL_EXTERN        11097

/* code body is not allowed in an 'extern function' definition */
#define TCERR_EXTERN_NO_CODE_BODY 11098

/* function "%.*s" is already defined */
#define TCERR_FUNC_REDEF          11099

/* function "%.*s" has a previous incompatible definition */
#define TCERR_INCOMPAT_FUNC_REDEF 11100

/* expected ':' in object definition after object name, but found "%.*s" */
#define TCERR_OBJDEF_REQ_COLON    11101

/* expected superclass name in object definition, but found "%.*s" */
#define TCERR_OBJDEF_REQ_SC       11102

/* superclasses cannot be specified with 'object' as the base class */
#define TCERR_OBJDEF_OBJ_NO_SC    11103

/* symbol "%.*s" is already defined - can't redefine as an object */
#define TCERR_REDEF_AS_OBJ        11104

/* object "%.*s" is already defined */
#define TCERR_OBJ_REDEF           11105

/* expected property name in definition of object "%.*s", but found "%.*s" */
#define TCERR_OBJDEF_REQ_PROP     11106

/* redefining symbol "%.*s" as property */
#define TCERR_REDEF_AS_PROP       11107

/* expected prop value or method after property name %.*s, but found %.*s */
#define TCERR_OBJDEF_REQ_PROPVAL  11108

/* expected 'function' or object name after 'replace', but found '%.*s' */
#define TCERR_REPLACE_REQ_OBJ_OR_FUNC 11109

/* replace/modify cannot be used with an undefined object */
#define TCERR_REPMODOBJ_UNDEF     11110

/* double-quoted strings cannot be used within expressions */
#define TCERR_DQUOTE_IN_EXPR      11111

/* assignment in condition (possible use of '=' where '==' was intended) */
#define TCERR_ASI_IN_COND         11112

/* replace/modify cannot be used with an undefined function */
#define TCERR_REPFUNC_UNDEF       11113

/* 'replace' can only be applied to a property in a 'modify' object */
#define TCERR_REPLACE_PROP_REQ_MOD_OBJ 11114

/* expected object name symbol in 'extern' but found "%.*s" */
#define TCERR_EXTERN_OBJ_REQ_SYM  11115

/* property "%.*s" is already defined in object "%.*s" */
#define TCERR_PROP_REDEF_IN_OBJ   11116

/* '=' required between property name and value - found "%.*s" */
#define TCERR_PROP_REQ_EQ         11117

/* extra list element expected after comma, but found end of list */
#define TCERR_LIST_EXPECT_ELEMENT  11118

/* expected class name string after name string but found "%.*s" */
#define TCERR_REQ_INTRINS_CLASS_NAME  11119

/* open brace required ater intrinsic class name */
#define TCERR_REQ_INTRINS_CLASS_LBRACE  11120

/* end of file in 'intrinsic class' property list */
#define TCERR_EOF_IN_INTRINS_CLASS  11121

/* expected property name in 'intrinsic class' list, but found "%.*s" */
#define TCERR_REQ_INTRINS_CLASS_PROP  11122

/* expected class name symbol after 'intrinsic class' but found "%.*s" */
#define TCERR_REQ_INTRINS_CLASS_NAME_SYM 11123

/* redefining symbol "%.*s" as a metaclass name */
#define TCERR_REDEF_INTRINS_NAME  11124

/* cannot evaluate metaclass symbol */
#define TCERR_CANNOT_EVAL_METACLASS 11125

/* expected 'property' or object name after 'dictionary', but found "%.*s" */
#define TCERR_DICT_SYNTAX         11126

/* expected property name in 'dictionary property' list, but found "%.*s" */
#define TCERR_DICT_PROP_REQ_SYM   11127

/* expected comma in 'dictionary property' list, but found "%.*s" */
#define TCERR_DICT_PROP_REQ_COMMA 11128

/* redefining object "%.*s" as dictionary */
#define TCERR_REDEF_AS_DICT       11129

/* undefined symbol "%.*s" (used as superclass of "%.*s") */
#define TCERR_UNDEF_SYM_SC        11130

/* vocabulary property requires one or more single-quoted string values */
#define TCERR_VOCAB_REQ_SSTR      11131

/* vocabulary property cannot be used without a dictionary */
#define TCERR_VOCAB_NO_DICT       11132

/* list varargs parameter not last */
#define TCERR_LISTPAR_NOT_LAST    11133

/* expected ']' after list varargs parameter name, found "%.*s" */
#define TCERR_LISTPAR_REQ_RBRACK  11134

/* expected list varargs parameter name, but found "%.*s" */
#define TCERR_LISTPAR_REQ_SYM     11135

/* anonymous functions are not allowed in the debugger */
#define TCERR_DBG_NO_ANON_FUNC    11136

/* anonymous function requires 'new' */
#define TCERR_ANON_FUNC_REQ_NEW   11137

/* expected production name after 'grammar' */
#define TCERR_GRAMMAR_REQ_SYM     11138

/* expected colon after production name in 'grammar' statement */
#define TCERR_GRAMMAR_REQ_COLON   11139

/* expected object or property name in 'grammar' token list, found "%.*s" */
#define TCERR_GRAMMAR_REQ_OBJ_OR_PROP 11140

/* property name must follow '->' in 'grammar' statement, found "%.*s" */
#define TCERR_GRAMMAR_ARROW_REQ_PROP 11141

/* invalid token list entry "%.*s" */
#define TCERR_GRAMMAR_INVAL_TOK   11142

/* redefining object "%.*s" as grammar production */
#define TCERR_REDEF_AS_GRAMPROD   11143

/* object "%.*s" is not valid in a grammar list (only productions allowed) */
#define TCERR_GRAMMAR_REQ_PROD    11144

/* symbol expected in enum list, but found "%.*s" */
#define TCERR_ENUM_REQ_SYM        11145

/* symbol "%.*s" is already defined - can't use as an enum name */
#define TCERR_REDEF_AS_ENUM       11146

/* comma expected in 'enum' symbol list, but found "%.*s" */
#define TCERR_ENUM_REQ_COMMA      11147

/* enumerator "%.*s" in 'grammar' list must be declare with 'enum token' */
#define TCERR_GRAMMAR_BAD_ENUM    11148

/* '*' must be the last token in a 'grammar' alt list - found "%.*s" */
#define TCERR_GRAMMAR_STAR_NOT_END 11149

/* grammar qualifiers must precede all tokens in the alternative */
#define TCERR_GRAMMAR_QUAL_NOT_FIRST 11150

/* symbol required in grammar qualifier - found "%.*s" */
#define TCERR_GRAMMAR_QUAL_REQ_SYM 11151

/* invalid grammar qualifier "%.*s" */
#define TCERR_BAD_GRAMMAR_QUAL    11152

/* integer constant required for grammar qualifier "%s" */
#define TCERR_GRAMMAR_QUAL_REQ_INT 11153

/* ']' required at end of grammar qualifier - found "%.*s" */
#define TCERR_GRAMMAR_QUAL_REQ_RBRACK 11154

/* '+ property' statement requires symbol, found "%.*s" instead */
#define TCERR_PLUSPROP_REQ_SYM     11155

/* too many '+' signs - location is not defined */
#define TCERR_PLUSOBJ_TOO_MANY     11156

/* property name required after "%s" in object template, but found "%.*s" */
#define TCERR_OBJ_TPL_OP_REQ_PROP  11157

/* property name required in string in object template, but found "%.*s" */
#define TCERR_OBJ_TPL_STR_REQ_PROP 11158

/* ']' required after list property in object template, found "%.*s" */
#define TCERR_OBJ_TPL_REQ_RBRACK   11159

/* invalid 'object template' syntax - "%.*s" */
#define TCERR_OBJ_TPL_BAD_TOK      11160

/* symbol \"%.*s\" in object template is not a property */
#define TCERR_OBJ_TPL_SYM_NOT_PROP 11161

/* object definition does not match any object template */
#define TCERR_OBJ_DEF_NO_TEMPLATE  11162

/* dictionary properties are not valid in object templates */
#define TCERR_OBJ_TPL_NO_VOCAB     11163

/* property duplicated in template list */
#define TCERR_OBJ_TPL_PROP_DUP     11164

/* metaclass is already defined as intrinsic class "%.*s" */
#define TCERR_META_ALREADY_DEF     11165

/* missing ';' at end of object definition - found "%.*s" */
#define TCERR_OBJ_DEF_REQ_SEM      11166

/* expected a statement, but found "%.*s" */
#define TCERR_EXPECTED_STMT_START  11167

/* missing colon at end of short anonymous function formals - found "%.*s" */
#define TCERR_MISSING_COLON_FORMAL 11168

/* semicolon is not allowed in a short anonymous function */
#define TCERR_SEM_IN_SHORT_ANON_FN 11169

/* close brace missing at end of anonymous function */
#define TCERR_SHORT_ANON_FN_REQ_RBRACE 11170

/* '(' required after 'in' operator */
#define TCERR_IN_REQ_LPAR          11171

/* expected comma in 'in' list */
#define TCERR_EXPECTED_IN_COMMA    11172

/* missing ')' at end of 'in' list */
#define TCERR_EXPECTED_IN_RPAR     11173

/* cannot modify or replace objects of this type */
#define TCERR_CANNOT_MOD_OR_REP_TYPE 11174

/* expected '(' after 'foreach', but found "%.*s" */
#define TCERR_REQ_FOREACH_LPAR     11175

/* missing expression in 'foreach' - found "%.*s" */
#define TCERR_MISSING_FOREACH_EXPR 11176

/* 'in' required in 'foreach' - found "%.*s" */
#define TCERR_FOREACH_REQ_IN       11177

/* missing ')' at end of 'for' statement expression list */
#define TCERR_REQ_FOREACH_RPAR     11178

/* property declaration requires symbol, but found "%.*s" */
#define TCERR_PROPDECL_REQ_SYM     11179

/* property declaration requires comma, but found "%.*s" */
#define TCERR_PROPDECL_REQ_COMMA   11180

/* expected symbol in 'export' statement, but found "%.*s" */
#define TCERR_EXPORT_REQ_SYM       11181

/* external name "%.*s" too long in 'export' statement */
#define TCERR_EXPORT_EXT_TOO_LONG  11182

/* unterminated object definition */
#define TCERR_UNTERM_OBJ_DEF       11183

/* missing right brace at end of object definition - found "%.*s" */
#define TCERR_OBJ_DEF_REQ_RBRACE   11184

/* right paren required after name-tag in 'grammar' - found "%.*s" */
#define TCERR_GRAMMAR_REQ_NAME_RPAR 11185

/* open brace required for propertyset - found "%.*s" */
#define TCERR_PROPSET_REQ_LBRACE   11186

/* right paren expected at end of group - found "%.*s" */
#define TCERR_GRAMMAR_REQ_RPAR_AFTER_GROUP 11187

/* '->' not allowed after parenthesized group in 'grammar' statement */
#define TCERR_GRAMMAR_GROUP_ARROW_NOT_ALLOWED 11188

/* cannot use template with unimported extern class as superclass */
#define TCERR_OBJ_DEF_CANNOT_USE_TEMPLATE 11189

/* superclass list missing for 'replace' object definition */
#define TCERR_REPLACE_OBJ_REQ_SC  11190

/* propertyset nesting too deep */
#define TCERR_PROPSET_TOO_DEEP    11191

/* propertyset token expansion too large */
#define TCERR_PROPSET_TOK_TOO_LONG 11192

/* invalid pattern string for propertyset - "%.*s" */
#define TCERR_PROPSET_INVAL_PAT   11193

/* invalid propertyset formal parameter list */
#define TCERR_PROPSET_INVAL_FORMALS 11194

/* circular class definition - "%.*s" is a subclass of "%.*s" */
#define TCERR_CIRCULAR_CLASS_DEF  11195

/* property required in part-of-speech list in 'grammar', but found "%.*s" */
#define TCERR_GRAMMAR_LIST_REQ_PROP 11196

/* missing '>' in 'grammar' property list - found "%.*s" */
#define TCERR_GRAMMAR_LIST_UNCLOSED 11197

/* invalid indexed type in constant expression (expected list) */
#define TCERR_CONST_IDX_INV_TYPE    11198

/* 'transient' is not allowed here (expected object definition) */
#define TCERR_INVAL_TRANSIENT       11199

/* 'modify/replace grammar' can only be applied to tagged rule name */
#define TCERR_GRAMMAR_MOD_REQ_TAG   11200

/* intrinsic superclass name expected after colon, but found "%.*s" */
#define TCERR_REQ_INTRINS_SUPERCLASS_NAME  11201

/* intrinsic superclass "%.*s" undefined */
#define TCERR_INTRINS_SUPERCLASS_UNDEF  11202

/* grammar rule ends with '|' */
#define TCERR_GRAMMAR_ENDS_WITH_OR  11203

/* grammar rule is completely empty */
#define TCERR_GRAMMAR_EMPTY         11204

/* empty template definition */
#define TCERR_TEMPLATE_EMPTY        11205

/* expected ')' after 'if' condition, but found "%.*s" */
#define TCERR_REQ_RPAR_IF         11206

/* intrinsic class superclass "%.*s" is not an intrinsic class */
#define TCERR_INTRINS_SUPERCLASS_NOT_INTRINS  11207

/* function redefined as multimethod */
#define TCERR_FUNC_REDEF_AS_MULTIMETHOD   11208

/* "multimethod" declaration not allowed in this context */
#define TCERR_MULTIMETHOD_NOT_ALLOWED   11209

/* multi-method parameter type %.*s is not an object */
#define TCERR_MMPARAM_NOT_OBJECT   11210

/* type name missing in inherited<> type list */
#define TCERR_MMINH_MISSING_ARG_TYPE   11211

/* comma is missing in inherited<> type list */
#define TCERR_MMINH_MISSING_COMMA      11212

/* '>' is missing at end of inherited<> type list */
#define TCERR_MMINH_MISSING_GT         11213

/* argument list is missing after inherited<> */
#define TCERR_MMINH_MISSING_ARG_LIST   11214

/* "..." cannot be used with a named argument value */
#define TCERR_NAMED_ARG_NO_ELLIPSIS    11215

/* argument %.*s must be optional */
#define TCERR_ARG_MUST_BE_OPT          11216

/* named argument '%.*s' has a type - named arguments can't be typed */
#define TCERR_NAMED_ARG_NO_TYPE        11217

/* expected -> instead of ',' after %dth element in LookupTable list */
#define TCERR_LOOKUP_LIST_EXPECTED_ARROW   11218

/* found -> in a list that appeared to be an ordinary list */
#define TCERR_ARROW_IN_LIST            11219

/* misplaced -> in a LookupTable list after %dth element */
#define TCERR_MISPLACED_ARROW_IN_LIST  11220

/* expected end of list after default value */
#define TCERR_LOOKUP_LIST_EXPECTED_END_AT_DEF 11221

/* 'operator' not valid within a 'propertyset' */
#define TCERR_OPER_IN_PROPSET          11222

/* expected ']' in 'operator []' */
#define TCERR_EXPECTED_RBRACK_IN_OP    11223

/* invalid operator overloading */
#define TCERR_BAD_OP_OVERLOAD          11224

/* wrong number of arguments for overloaded operator */
#define TCERR_OP_OVERLOAD_WRONG_FORMALS  11225

/* can't define new global symbols at run-time */
#define TCERR_RT_CANNOT_DEFINE_GLOBALS 11226

/* 'for..in' variable %.*s is not defined as a local variable */
#define TCERR_FOR_IN_NOT_LOCAL         11227

/* 
 *   unexpected '%.*s' in << >> expression in string (mismatched 'end',
 *   'else', 'default', etc) 
 */
#define TCERR_BAD_EMBED_END_TOK        11228

/* expected << at start of string template found '%.*s' */
#define TCERR_STRTPL_MISSING_LANGLE    11229

/* expected >> at end of string template, found '%.*s' */
#define TCERR_STRTPL_MISSING_RANGLE    11230

/* only one '*' is allowed in a string template */
#define TCERR_STRTPL_MULTI_STAR        11231

/* 
 *   wrong prototype for string template processor function %.*s - expected
 *   %d parameters and a return value 
 */
#define TCERR_STRTPL_FUNC_MISMATCH     11232

/* invalid defined() syntax */
#define TCERR_DEFINED_SYNTAX           11233

/* invalid __objref() syntax */
#define TCERR___OBJREF_SYNTAX          11234

/* invalid constant value type for operator */
#define TCERR_BAD_OP_FOR_FLOAT         11235

/* expected inline object property list starting with '{', found '%.*s' */
#define TCERR_INLINE_OBJ_REQ_LBRACE    11236


/* ------------------------------------------------------------------------ */
/*
 *   General Code Generator Errors 
 */

/* out of memory for code generation */
#define TCERR_CODEGEN_NO_MEM      11500

/* internal error: %d unresolved temporary label fixups */
#define TCERR_UNRES_TMP_FIXUP     11501

/* internal error: attempting to write past end of code stream */
#define TCERR_WRITEAT_PAST_END    11502

/* "self" is not valid in this context */
#define TCERR_SELF_NOT_AVAIL      11503

/* "inherited" is not valid in this context */
#define TCERR_INH_NOT_AVAIL       11504

/* no address for symbol "%.*s" */
#define TCERR_NO_ADDR_SYM         11505

/* cannot assign to symbol "%.*s" */
#define TCERR_CANNOT_ASSIGN_SYM   11506

/* cannot evaluate a code label ("%.*s") */
#define TCERR_CANNOT_EVAL_LABEL   11507

/* cannot call symbol "%.*s" */
#define TCERR_CANNOT_CALL_SYM     11508

/* cannot call property "%.*s" without explicit object (no "self") */
#define TCERR_PROP_NEEDS_OBJ      11509

/* "%.*s" has no return value to be used in an expression */
#define TCERR_VOID_RETURN_IN_EXPR 11510

/* '&' operator cannot be used with function name */
#define TCERR_INVAL_FUNC_ADDR     11511

/* cannot apply operator 'new' to this expression */
#define TCERR_INVAL_NEW_EXPR      11512

/* not enough arguments to function "%.*s" */
#define TCERR_TOO_FEW_FUNC_ARGS   11513

/* too many arguments to function "%.*s" */
#define TCERR_TOO_MANY_FUNC_ARGS  11514

/* assigning to "%.*s" needs object */
#define TCERR_SETPROP_NEEDS_OBJ   11515

/* symbol "%.*s" is not a property - illegal on right side of '.' */
#define TCERR_SYM_NOT_PROP        11516

/* invalid property expression - right side of '.' is not a property */
#define TCERR_INVAL_PROP_EXPR     11517

/* illegal inherited syntax - "%.*s" is not an object */
#define TCERR_INH_NOT_OBJ         11518

/* invalid object expression - left side of '.' is not an object */
#define TCERR_INVAL_OBJ_EXPR      11519

/* symbol "%.*s" is not an object - illegal on left of '.' */
#define TCERR_SYM_NOT_OBJ         11520

/* "if" condition is always true */
#define TCERR_IF_ALWAYS_TRUE      11521

/* "if" condition is always false */
#define TCERR_IF_ALWAYS_FALSE     11522

/* invalid "break" - not in a loop or switch */
#define TCERR_INVALID_BREAK       11523

/* invalid "continue" - not in a loop */
#define TCERR_INVALID_CONTINUE    11524

/* entrypoint function "_main" is not defined */
#define TCERR_MAIN_NOT_DEFINED    11525

/* "_main" is not a function */
#define TCERR_MAIN_NOT_FUNC       11526

/* exception class symbol "%.*s" in "catch" clause is not an object */
#define TCERR_CATCH_EXC_NOT_OBJ   11527

/* invalid 'break' - no label \"%.*s\" on any enclosing statement */
#define TCERR_INVALID_BREAK_LBL   11528

/* invalid 'continue' - no label \"%.*s\" on any enclosing statement */
#define TCERR_INVALID_CONT_LBL    11529

/* 'goto' label "%.*s" is not defined in this function */
#define TCERR_INVALID_GOTO_LBL    11530

/* unresolved external reference "%.*s" */
#define TCERR_UNRESOLVED_EXTERN   11531

/* label "%.*s" is never referenced */
#define TCERR_UNREFERENCED_LABEL  11532

/* local variable "%.*s" is never referenced */
#define TCERR_UNREFERENCED_LOCAL  11533

/* local variable "%.*s" never has a value assigned */
#define TCERR_UNASSIGNED_LOCAL    11534

/* local variable "%.*s" is assigned a value that is never used */
#define TCERR_UNUSED_LOCAL_ASSIGNMENT 11535

/* superclass "%.*s" is not an object */
#define TCERR_SC_NOT_OBJECT       11536

/* argument expression %d has no value */
#define TCERR_ARG_EXPR_HAS_NO_VAL 11537

/* expression on right of assignment operator has no value */
#define TCERR_ASI_EXPR_HAS_NO_VAL 11538

/* wrong number of arguments to function %.*s: %s required, %d actual */
#define TCERR_WRONG_ARGC_FOR_FUNC 11539

/* transfer into 'finally' block via 'goto' is not allowed */
#define TCERR_GOTO_INTO_FINALLY   11540

/* parameter "%.*s" is never referenced */
#define TCERR_UNREFERENCED_PARAM  11541

/* grammar production "%.*s" has no alternatives defined */
#define TCERR_GRAMPROD_HAS_NO_ALTS 11542

/* intrinsic class property "%.*s" cannot be modified */
#define TCERR_CANNOT_MOD_META_PROP 11543

/* Collection.createIterator() is not defined - foreach() cannot be used */
#define TCERR_FOREACH_NO_CREATEITER 11544

/* Iterator.getNext() is not defined - foreach() cannot be used */
#define TCERR_FOREACH_NO_GETNEXT   11545

/* Iterator.isNextAvailable() is not defined - foreach() cannot be used */
#define TCERR_FOREACH_NO_ISNEXTAVAIL 11546

/* invalid type for export */
#define TCERR_INVALID_TYPE_FOR_EXPORT 11547

/* duplicate export - extern name "%.*s", internal names "%.*s" and "%.*s" */
#define TCERR_DUP_EXPORT           11548

/* another duplicate export for extern name "%.*s", internal name "%.*s" */
#define TCERR_DUP_EXPORT_AGAIN     11549

/* "targetprop" is not valid in this context */
#define TCERR_TARGETPROP_NOT_AVAIL 11550

/* "targetobj" is not valid in this context */
#define TCERR_TARGETOBJ_NOT_AVAIL 11551

/* "definingobj" is not valid in this context */
#define TCERR_DEFININGOBJ_NOT_AVAIL 11552

/* cannot call constant expression */
#define TCERR_CANNOT_CALL_CONST   11553

/* "replaced" is not valid in this context */
#define TCERR_REPLACED_NOT_AVAIL  11554

/* grammar production "%.*s" has too many alternatives defined */
#define TCERR_GRAMPROD_TOO_MANY_ALTS 11555

/* cannot apply 'new' to this symbol (not a TadsObject object) */
#define TCERR_BAD_META_FOR_NEW    11556

/* inherited<> type token is not an object name */
#define TCERR_MMINH_TYPE_NOT_OBJ  11557

/* inherited<> invalid in this context */
#define TCERR_MMINH_BAD_CONTEXT   11558

/* multi-method 'inherited' missing support function %.*s */
#define TCERR_MMINH_MISSING_SUPPORT_FUNC    11559

/* inherited<> function not found */
#define TCERR_MMINH_UNDEF_FUNC    11560

/* named parameter support function %.*s missing */
#define TCERR_NAMED_PARAM_MISSING_FUNC  11561

/* optional parameter support function %.*s missing */
#define TCERR_OPT_PARAM_MISSING_FUNC  11562

/* <<one of>> requires class OneOfIndexGen to be defined */
#define TCERR_ONEOF_REQ_GENCLS     11563

/* <<one of>> requires getNextIndex to be a property name */
#define TCERR_ONEOF_REQ_GETNXT     11564

/* metaclass %.*s is not defined in this module */
#define TCERR_EXT_METACLASS        11565

/* cannot call extern function without an argument list prototype */
#define TCERR_FUNC_CALL_NO_PROTO   11566

/* %.*s is not defined or is not a metaclass */
#define TCERR_UNDEF_METACLASS      11567


/* ------------------------------------------------------------------------ */
/*
 *   Errors related to symbol export files 
 */

/* 
 *   invalid symbol type in symbol file - file is corrupted or was created
 *   by an incompatible version of the compiler 
 */
#define TCERR_SYMEXP_INV_TYPE     11700

/* invalid symbol file signature */
#define TCERR_SYMEXP_INV_SIG      11701

/* symbol name too long in symbol file */
#define TCERR_SYMEXP_SYM_TOO_LONG 11702

/* 
 *   symbol "%.*s" found in symbol file is already defined; ignoring the
 *   redefinition 
 */
#define TCERR_SYMEXP_REDEF        11703


/* ------------------------------------------------------------------------ */
/*
 *   Errors related to object files 
 */

/* invalid object file signature */
#define TCERR_OBJFILE_INV_SIG     11800

/* too many object/property ID's for this platform */
#define TCERR_OBJFILE_TOO_MANY_IDS 11801

/* out of memory loading object file */
#define TCERR_OBJFILE_OUT_OF_MEM  11802

/* invalid symbol type in object file */
#define TCERR_OBJFILE_INV_TYPE    11803

/* symbol %.*s (type %s) redefined (type %s) in object file "%s" */
#define TCERR_OBJFILE_REDEF_SYM_TYPE 11804

/* symbol %.*s (type %s) redefined in object file "%s" */
#define TCERR_OBJFILE_REDEF_SYM   11805

/* built-in function %.*s has different definition in object file "%s" */
#define TCERR_OBJFILE_BIF_INCOMPAT  11806

/* function %.*s has different definition in object file "%s" */
#define TCERR_OBJFILE_FUNC_INCOMPAT 11807

/* internal symbol reference \"%.*s\" missing in object file %s */
#define TCERR_OBJFILE_INT_SYM_MISSING 11808

/* invalid stream ID in object file %s */
#define TCERR_OBJFILE_INVAL_STREAM_ID 11809

/* invalid object ID in object file %s */
#define TCERR_OBJFILE_INVAL_OBJ_ID 11810

/* invalid property ID in object file %s */
#define TCERR_OBJFILE_INVAL_PROP_ID 11811

/* function set/metaclass entry is invalid in object file "%s" */
#define TCERR_OBJFILE_INV_FN_OR_META 11812

/* conflicting function set "%s" in object file "%s" */
#define TCERR_OBJFILE_FNSET_CONFLICT 11813

/* conflicting metaclass "%s" in object file "%s" */
#define TCERR_OBJFILE_META_CONFLICT 11814

/* modified or replaced object %.*s found before original in object file %s */
#define TCERR_OBJFILE_MODREPOBJ_BEFORE_ORIG 11815

/* replaced function %.*s found before original in object file %s */
#define TCERR_OBJFILE_REPFUNC_BEFORE_ORIG 11816

/* "construct" cannot return a value */
#define TCERR_CONSTRUCT_CANNOT_RET_VAL 11817

/* 
 *   object file doesn't contain debugging information (required because
 *   we're linking for debugging) 
 */
#define TCERR_OBJFILE_NO_DBG      11818

/* metaclass prop "%.*s" in does not match original "%.*s" in obj file %s */
#define TCERR_OBJFILE_METACLASS_PROP_CONFLICT  11819

/* metaclass index conflict in object file %s */
#define TCERR_OBJFILE_METACLASS_IDX_CONFLICT  11820

/* cannot modify or replace object of this type - %.*s in object file %s */
#define TCERR_OBJFILE_CANNOT_MOD_OR_REP_TYPE  11821

/* exported symbol too long in object file */
#define TCERR_EXPORT_SYM_TOO_LONG 11822

/* macro name too long in object file debug records */
#define TCERR_OBJFILE_MACRO_SYM_TOO_LONG  11823

/* conflicting definitions of %.*s as ordinary function and as multi-method */
#define TCERR_OBJFILE_MMFUNC_INCOMPAT  11824

/* multi-method parameter type %.*s in function %.*s is not an object */
#define TCERR_OBJFILE_MMPARAM_NOT_OBJECT  11825

/* multi-method registration function %s is missing or invalid */
#define TCERR_MISSING_MMREG  11826


/* ------------------------------------------------------------------------ */
/*
 *   T3 Code Generator Errors 
 */

/* constant string or list exceeds 32k - won't run on 16-bit machines */
#define TCERR_CONST_POOL_OVER_32K 15001

/* internal error: constant too large for page */
#define TCERR_CONST_TOO_LARGE_FOR_PG 15002

/* internal error: corrupted list */
#define TCERR_CORRUPT_LIST        15003

/* internal error: 'inherited' codegen */
#define TCERR_GEN_CODE_INH        15004

/* internal error: unknown constant type in code generation */
#define TCERR_GEN_UNK_CONST_TYPE  15005

/* internal error: attempting to generate code for non-lvalue */
#define TCERR_GEN_BAD_LVALUE      15006

/* internal error: attempting to generate address for non-addressable */
#define TCERR_GEN_BAD_ADDR        15007

/* too many arguments to function (maximum of 127 allowed) */
#define TCERR_TOO_MANY_CALL_ARGS  15008

/* too many arguments to constructor (maximum of 126 allowed) */
#define TCERR_TOO_MANY_CTOR_ARGS  15009

/* internal error: 'delegated' codegen */
#define TCERR_GEN_CODE_DELEGATED  15010

/* code block exceeds 32k - won't run on 16-bit machines */
#define TCERR_CODE_POOL_OVER_32K  15011

/* internal error: 'case' or 'default' out of place or non-const expression */
#define TCERR_GEN_BAD_CASE        15012

/* internal error: 'catch' or 'finally' gen_code called directly */
#define TCERR_CATCH_FINALLY_GEN_CODE 15013

/* internal error: property value has invalid type for code generation */
#define TCERR_INVAL_PROP_CODE_GEN 15014

/* symbol "%.*s" cannot be exported - reserved for compiler use */
#define TCERR_RESERVED_EXPORT     15015

/* expression too complex */
#define TCERR_EXPR_TOO_COMPLEX    15016

/* property 'construct' is not defined */
#define TCERR_CONSTRUCT_NOT_DEFINED 15017

/* 'construct' is not a property */
#define TCERR_CONSTRUCT_NOT_PROP 15018

/* property 'finalize' is not defined */
#define TCERR_FINALIZE_NOT_DEFINED 15019

/* 'finalize' is not a property */
#define TCERR_FINALIZE_NOT_PROP 15020


/* ------------------------------------------------------------------------ */
/*
 *   Javascript code generator errors 
 */

/* invalid expression template */
#define TCERR_BAD_JS_TPL   16000

/* expression template expansion overflow */
#define TCERR_JS_EXPR_EXPAN_OVF  16001


#endif /* TCERRNUM_H */

