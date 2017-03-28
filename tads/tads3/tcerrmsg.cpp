#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcerrmsg.cpp - TADS 3 Compiler error message text
Function
  Defines the error message strings for the TADS 3 compiler.  The message
  strings are all isolated in this module to allow for easy replacement
  for translated versions.
Notes
  
Modified
  05/13/00 MJRoberts  - Creation
*/

#include "tcerr.h"
#include "tcerrnum.h"

/* ------------------------------------------------------------------------ */
/*
 *   Error Messages - fixed English version; these can be replaced at
 *   run-time by messages loaded from an external file, but we compile in an
 *   English set as a fallback in case there's no message file.
 *   
 *   The messages must be sorted by message number, so that we can perform a
 *   binary search to look up a message by number.  
 */
const err_msg_t tc_messages_english[] =
{
    { TCERR_LINE_MEM,
    "out of memory for line source",
    "Out of memory for line source.  The source line may be too long, "
    "or you may need to make more memory available to the compiler "
    "by closing other applications." },

    { TCERR_INV_PP_DIR,
    "invalid preprocessor directive",
    "\"%~.*s\" is not a valid preprocessor '#' directive." },

    { TCERR_CANT_LOAD_CHARSET,
    "unable to load #charset",
    "Unable to load the character set specified in the #charset directive. "
    "Check the spelling of the character set name, and make sure that "
    "a character mapping file (with a name ending in \".TCM\") is "
    "available.  Refer to the compiler's installation notes for your "
    "type of computer for details." },

    { TCERR_UNEXPECTED_CHARSET,
    "unexpected or invalid #charset",
    "Unexpected or invalid #charset directive.  This directive must "
    "be at the very beginning of the file, and must specify a character "
    "set name enclosed in double quotes." },

    { TCERR_BAD_INC_SYNTAX,
    "syntax error in #include",
    "Invalid #include syntax - the filename must be enclosed with "
    "'\"' or '< >' characters." },

    { TCERR_INC_NOT_FOUND,
    "cannot open include file \"%.*s\"",
    "The compiler cannot access the #include file \"%.*s\".  "
    "Check the filename to ensure that it's spelled correctly, and check "
    "the compiler's include path setting (command line \"-I\" options) "
    "to ensure that the compiler is searching in the directory or folder "
    "containing the file.  If the file exists, make sure that it's not "
    "being used by another application and that you have permission "
    "to read the file." },

    { TCERR_REDUNDANT_INCLUDE,
    "file \"%.*s\" previously included; ignored",
    "The #include file \"%.*s\" has already been included.  "
    "This redundant inclusion will be ignored." },

    { TCERR_BAD_DEFINE_SYM,
    "invalid symbol \"%~.*s\" for #define",
    "Invalid symbol \"%~.*s\" for #define.  A #define symbol must start "
    "with an ASCII letter or underscore symbol '_', and must contain "
    "only ASCII letters, digits, and underscores." },

    { TCERR_MACRO_NO_RPAR,
    "missing ')' in macro parameter list",
    "The macro parameter list is missing a right parenthesis ')' at "
    "the end of the list." },

    { TCERR_BAD_MACRO_ARG_NAME,
    "invalid macro parameter name \"%~.*s\"",
    "Invalid macro parameter name \"%~.*s\".  A macro parameter name must "
    "start with an ASCII letter or underscore symbol '_', and must contain "
    "only ASCII letters, digits, and underscores." },

    { TCERR_MACRO_EXP_COMMA,
    "expected ',' or ')' in macro parameter list (found \"%~.*s\")",
    "Expected a comma ',' or right parenthesis ')' in the macro "
    "parameter list, but found \"%~.*s\"." },

    { TCERR_MACRO_REDEF,
    "redefinition of macro \"%~.*s\"",
    "Macro \"%~.*s\" has been previously defined; this new definition "
    "replaces the previous definition." },

    { TCERR_UNKNOWN_PRAGMA,
    "unrecognized #pragma \"%~.*s\"; ignored",
    "Unrecognized #pragma \"%~.*s\"; ignored.  (The compiler ignores "
    "#pragma directives that it doesn't understand, in case you're "
    "using a #pragma meant for another compiler, but if you meant "
    "the #pragma for this compiler, you need to correct a problem "
    "with it.)" },

    { TCERR_BAD_PRAGMA_SYNTAX,
    "invalid syntax for #pragma",
    "Invalid syntax for #pragma." },

    { TCERR_PP_EXTRA,
    "extra characters on line",
    "Extra characters found after the end of the preprocessor directive.  "
    "Check the syntax and remove the extraneous characters at the "
    "end of the line." },

    { TCERR_IF_NESTING_OVERFLOW,
    "#if nesting too deep",
    "#if/#ifdef nesting is too deep - you have specified more "
    "#if's within other #if's than the compiler allows.  (The compiler "
    "has an internal limit on this nesting; you must simplify the "
    "nested #if structure of your source code.)" },

    { TCERR_PP_ELSE_WITHOUT_IF,
    "#else without #if",
    "#else without a matching #if or #ifdef." },

    { TCERR_PP_ENDIF_WITHOUT_IF,
    "#endif without #if",
    "#endif without a matching #if or #ifdef." },

    { TCERR_PP_ELIF_WITHOUT_IF,
    "#elif without #if",
    "#elif without a matching #if." },

    { TCERR_PP_INT_REQUIRED,
    "integer value required in preprocessor constant expression",
    "Incorrect value in preprocessor constant expression - an integer "
    "value is required." },

    { TCERR_PP_INCOMP_TYPES,
    "incompatible types for comparison operator",
    "Incompatible types for comparison in preprocessor constant "
    "expression." },

    { TCERR_PP_EXPR_EXTRA,
    "extra characters at end of line",
    "Extra characters found after end of preprocessor constant "
    "expression.  Check the syntax of the expression and remove any "
    "extraneous characters at the end of the line." },

    { TCERR_PP_DIV_ZERO,
    "divide by zero in constant expression",
    "Division by zero in preprocessor constant expression." },

    { TCERR_PP_INVALID_VALUE,
    "expected number, symbol, or string (found \"%~.*s\")",
    "Expected a number, symbol, or single-quoted string in preprocessor "
    "constant expression, but found \"%~.*s\"." },

    { TCERR_PP_UNTERM_STRING,
    "unterminated string in preprocessor constant expression",
    "Unterminated string found in a preprocessor constant expression.  "
    "(A string in a preprocessor expression must be contained entirely "
    "on a single line.)" },

    { TCERR_PP_UNMATCHED_LPAR,
    "unmatched '('",
    "Unmatched left parenthesis '(' in preprocessor constant expression." },

    { TCERR_PP_BAD_NOT_VAL,
    "integer value required for '!' operator",
    "Integer value is required for '!' operator in preprocessor "
    "expression." },

    { TCERR_PP_MACRO_ARG_RPAR_1LINE,
    "missing ')' in argument list for macro \"%~.*s\"",
    "Missing right parenthesis ')' in argument list in invocation of "
    "macro \"%~.*s\".  The entire argument list must be on a single "
    "logical line for a line with a preprocessor directive (but you can "
    "extend the logical line over several physical lines by ending each "
    "line but the last with a backslash '\\')." },

    { TCERR_PP_NO_MACRO_ARGS,
    "missing argument list for macro \"%~.*s\"",
    "An argument list must be specified in invocation of macro \"%~.*s\"." },

    { TCERR_PP_FEW_MACRO_ARGS,
    "not enough arguments for macro \"%~.*s\"",
    "Not enough arguments are specified in invocation of macro \"%~.*s\"." },

    { TCERR_PP_MACRO_ARG_RPAR,
    "missing ')' in argument list for macro \"%~.*s\"",
    "Missing right parenthesis ')' in argument list in "
    "invocation of macro \"%~.*s\"." },

    { TCERR_PP_MANY_MACRO_ARGS,
    "too many arguments for macro \"%~.*s\"",
    "Too many arguments found in invocation of macro \"%~.*s\".  (The "
    "macro was defined to take fewer arguments.  Check the definition "
    "of the macro for proper usage, and make sure that all of the "
    "parentheses in the macro invocation are properly matched.)" },

    { TCERR_PP_DEFINED_NO_SYM,
    "symbol required for defined() (found \"%~.*s\")",
    "Symbol required for defined() preprocessor operator "
    "(found \"%~.*s\" instead.)" },

    { TCERR_PP_DEFINED_RPAR,
    "missing ')' in defined()",
    "Missing right parenthesis ')' in defined() preprocess operator." },

    { TCERR_SYMBOL_TRUNCATED,
    "symbol \"%~.*s\" truncated to \"%~.*s\"",
    "The symbol \"%~.*s\" is too long; it has been truncated to \"%~.*s\".  "
    "(The compiler limits the length of each symbol name; you must make "
    "this symbol name shorter.)" },

    { TCERR_TOO_MANY_MAC_PARMS,
    "too many parameters for macro \"%~.*s\" (maximum %d)",
    "Too many formal parameters are defined for macro "
    "\"%~.*s\".  (The compiler imposes a limit of %d parameters per macro.)" },

    { TCERR_NO_STRBUF_MEM,
    "out of memory for string buffer",
    "Out of memory for string buffer.  You may need to "
    "close other applications to make more memory available for "
    "the compiler." },

    { TCERR_CANT_OPEN_SRC,
    "unable to open source file \"%.*s\"",
    "Unable to open source file \"%.*s\" - check that the filename "
    "is spelled correctly, and check that the file exists and that you "
    "have permission to open the file for reading." },

    { TCERR_ERROR_DIRECTIVE,
    "#error : %.*s",
    "#error : %.*s" },

    { TCERR_OUT_OF_MEM_MAC_EXP,
    "out of memory for macro expansion",
    "Out of memory for macro expansion.  You may need to "
    "close other applications to make more memory available for "
    "the compiler." },

    { TCERR_LINE_REQ_INT,
    "integer value required for #line",
    "An integer value is required for the line number in the #line "
    "directive." },

    { TCERR_LINE_FILE_REQ_STR,
    "string value required for #line",
    "A string value is required for the filename in the #line directive." },

    { TCERR_IF_WITHOUT_ENDIF,
    "#if without #endif (line %ld, file %.*s)",
    "The #if directive at line %ld of file %.*s has no matching #endif "
    "(a matching #endif is required in the same file as the #if)." },

    { TCERR_UNSPLICE_NOT_CUR,
    "unsplicing invalid line",
    "Unsplicing invalid line." },

    { TCERR_MULTI_UNSPLICE,
    "too much unsplicing",
    "Too much unsplicing." },

    { TCERR_PP_ELIF_NOT_IN_SAME_FILE,
    "#elif without #if",
    "#elif without #if - an entire #if-#elif-#else-#endif sequence must "
    "all be contained within a single file; this #elif appears to "
    "correspond to a #if in the including file." },

    { TCERR_PP_ELSE_NOT_IN_SAME_FILE,
    "#else without #if",
    "#else without #if - an entire #if-#elif-#else-#endif sequence must "
    "all be contained within a single file; this #else appears to "
    "correspond to a #if in the including file." },

    { TCERR_PP_ENDIF_NOT_IN_SAME_FILE,
    "#endif without #if",
    "#endif without #if - an entire #if-#elif-#else-#endif sequence must "
    "all be contained within a single file; this #endif appears to "
    "correspond to a #if in the including file." },

    { TCERR_REDEF_OP_DEFINED,
    "cannot #define reserved preprocessor symbol \"defined\"",
    "You cannot #define \"defined\" as a macro - this symbol is reserved "
    "as a preprocessor keyword and cannot be defined as a macro name." },

    { TCERR_POSSIBLE_UNTERM_STR,
    "string appears unterminated (';' or '}' appears alone on line %ld)",
    "This string appears unterminated, because ';' or '}' appears on a line "
    "with no other non-blank characters (at line %ld) within the string.  "
    "Check the string for proper termination; if the string is "
    "properly terminated and the ';' or '}' is meant to be part of the "
    "string, move the ';' or '}' onto the next or previous line to group "
    "it with at least one other non-whitespace character, so that it "
    "doesn't confuse the compiler" },

    { TCERR_SRCLINE_TOO_LONG,
    "source line too long (exceeds maximum line length %ld bytes)",
    "This source line is too long - it exceeds the internal compiler "
    "limit of %ld bytes per source line.  (The compiler's idea of the "
    "length of the logical source line may be longer than the line appears "
    "to be in the source file, because the compiler limit applies to the "
    "line after expansion of macros, assembly of all parts of any string "
    "that runs across several lines into a single line, and splicing of "
    "any lines that end in backslash characters '\\'.  You must reduce "
    "the length of the logical source line; check in particular for any "
        "quoted strings that run across several lines. " },

    { TCERR_INVALID_CHAR,
    "invalid character in input \"%~.*s\"",
    "Invalid character in input: \"%~.*s\".  This character is not valid "
    "in any symbol name or as punctuation in source code; the character "
    "will be ignored.  Check for missing quotes around a string, a missing "
    "ending quote for a string just before this point, or for "
    "a quote mark embedded within an earlier string (if you want to use "
    "a quote mark within a string, you must precede the quote mark with "
    "a backslash '\\')." },

    { TCERR_PP_QC_MISSING_COLON,
    "missing colon ':' after conditional operator '?' in preprocessor "
        "expression",
    "The preprocessor constant conditional expression on this line is "
    "missing the colon ':' part.  A question-mark operator '?' must be "
    "followed by the true-part, then a colon ':', then the false-part.  "
    "Check the expression to ensure proper placement of parentheses, "
    "and check for other errors in the expression syntax. " },

    { TCERR_PP_EXPR_NOT_CONST,
    "preprocessor expression is not a constant value",
    "The preprocessor expression given does not have a constant value.  "
    "A preprocessor expression (in a #if, #elif, or #line directive) "
    "must use only numbers and strings, or #define symbols defined as "
    "numbers or strings.  Preprocessor expressions cannot include "
    "variables, function calls, property references, or other values "
    "that do not have a constant value during compilation." },

    { TCERR_CANT_LOAD_DEFAULT_CHARSET,
    "can't load mapping file for default character set \"%s\"",
    "The compiler cannot open the mapping file for the default "
    "character set, \"%s\".  Make sure that a mapping file for this "
    "character set (with the same name as the character set plus the "
    "suffix \".TCM\") is properly installed on your system.  Refer "
    "to the compiler's installation notes for your type of "
    "computer for details.  You might need to re-install the compiler." },

    { TCERR_MACRO_ELLIPSIS_REQ_RPAR,
    "'...' in macro formal parameter list must be followed by ')' - "
        "found \"%~.*s\"",
    "An ellipsis '...' in a macro formal parameter list can only be "
    "used with the last parameter to the macro, so it must be immediately "
    "followed by a close parenthesis ')' - the compiler found \"%~.*s\" "
    "instead.  Check the macro definition syntax and insert the missing "
    "parenthesis." },

    { TCERR_PP_FOREACH_TOO_DEEP,
    "#foreach/#ifempty/#ifnempty nesting too deep",
    "This line uses a macro whose expansion has too many nested uses "
    "of #foreach, #ifempty, and #ifnempty for its variable arguments.  "
    "You must simplify the expansion text to reduce the nesting (in "
    "other words, you must reduce the number of these constructs that "
    "are contained within the expansion areas of others of these "
    "same constructs)." },

    { TCERR_TRAILING_SP_AFTER_BS,
    "line ends with whitespace following backslash",
    "This line ends with one or more whitespace characters (space, tab, "
    "etc.) following a backslash '\\'.  A backslash at the end of a line "
    "is most frequently used to indicate a continuation line, where "
    "a long preprocessor directive is broken up over several lines "
    "in the source file.  To indicate line continuation, though, the "
    "backslash must be the very last character on the line; because of "
    "the extra whitespace after the backslash on this line, the "
    "preprocessor must treat the backslash as quoting the whitespace "
    "character that follows.  If you meant to indicate line continuation, "
    "remove the trailing whitspace.  If you actually intended to escape "
    "the whitespace character, you can suppress this warning by adding "
    "a comment (a simple '//' is adequate) at the end of the line." },

    { TCERR_EXTRA_INC_SYNTAX,
    "extraneous characters following #include filename",
    "This #include line has extra characters after the name of the file. "
    "The only thing allowed on a #include line is the filename, enclosed "
    "in quotes (\"filename\") or angle brackets (<filename>).  This line "
    "has extra characters after the filename specification.  Check the "
    "syntax and remove the extraneous text.  If the extra text is "
    "supposed to be a comment, make sure the comment syntax is correct." },

    { TCERR_NESTED_COMMENT,
    "\"/*\" found within \"/* ... */\" - nested comments are not allowed",
    "The compiler found \"/*\" within a block comment (a \"/* ... */\" "
    "sequence).  This type of comment cannot be nested.  If you didn't "
    "intend to create a nested comment, the problem could be that the "
    "previous block comment is missing its \"*/\" end marker - you might "
    "want to check the previous comment to make sure it ended properly. "
    "If you did want to create a nested comment, consider using \"//\" "
    "comments instead, or you can use #if 0 ... #endif to comment out "
    "a large block." },

    { TCERR_UNMAPPABLE_CHAR,
    "unmappable character in input",
    "The compiler encountered an \"unmappable\" character in the input. "
    "This is a character that's not defined as part of the source file "
    "character set you're using, so the compiler doesn't know how to "
    "interpret it. This can happen if you've declared the file to be "
    "in plain ASCII, via a #charset \"us-ascii\" directive, but the file "
    "actually contains characters outside of the plain ASCII range. "
    "The same can happen with the ISO-8859 (Latin-1, etc) character sets, "
    "since these do not define all possible character codes. Check for "
    "any accented letters or special symbols. If you don't see any of "
    "these, there might be invisible control characters or spaces causing "
    "the problem. If you are intentionally using accented characters, "
    "add a #charset directive to the start of the file to indicate the "
    "correct character set that your text editor is using when saving "
    "the source file." },

    { TCERR_DECIMAL_IN_OCTAL,
    "decimal digit found in octal constant \"%.*s\"",
    "The compiler found a decimal digit (an 8 or a 9) within the octal "
    "constant value \"%.*s\". When you start a numeric constant with the "
    "digit zero (0), it signifies an octal constant - that is, a number "
    "written in base-8 notation. Octal numbers can only contain the "
    "digits 0 through 7, so you can't use an 8 or a 9 in this type of "
    "constant. If you didn't mean this to be interpreted as an octal "
    "value, simply remove the leading zero. Otherwise, remove the "
    "invalid digits from the octal number." },

    { TCERR_LTGT_OBSOLETE,
    "the '<>' operator is obsolete - use '!=' instead",
    "The '<>' operator is obsolete - use '!=' instead. TADS 2 treated "
    "'<>' as equivalent to '!=', but TADS 3 doesn't allow '<>', because "
    "the varying operator syntax was sometimes confusing to people "
    "reading source code." },

    { TCERR_INT_CONST_OV,
    "constant value exceeds integer range; promoted to BigNumber",
    "The numeric value specified is outside of the range that can be "
    "stored in the TADS integer type (-2147483648 to +2147483647), so it "
    "has been automatically promoted to a BigNumber (floating point) "
    "value. BigNumber values can represent much larger numbers than "
    "TADS integers, but some functions that require numeric arguments "
    "only accept integer values. If you're using this value in such a "
    "context, it might cause an error at run-time." },

    { TCERR_BACKSLASH_SEQ,
    "invalid backslash escape sequence \\%c in string",
    "The backslash sequence \\%c is not valid." },

    { TCERR_NON_ASCII_SYMBOL,
    "symbol \"%~.*s\" contains non-ASCII character (U+%04x)",
    "The symbol \"%~.*s\" contains one or more non-ASCII characters (the "
    "first is [Unicode] character U+%04x). Only plain ASCII characters "
    "can be used in symbols; accented letters and other non-ASCII "
    "characters aren't allowed." },

    { TCERR_EMBEDDING_TOO_DEEP,
    "embedded expressions in strings are nested too deeply",
    "The embedded \"<< >>\" expressions in this string are nested too "
    "deeply.  A nested embedding is a \"<< >>\" expression within "
    "a string that iself contains another string that has its own "
    "\"<< >>\" embedding, which in turn has another string with its "
    "own embedding, and so on.  This is allowed, but only to a limited "
    "nesting depth.  You will need to simplify the strings to reduce "
    "the depth." },

    { TCERR_INTERNAL_EXPLAN,
    "Please report this error to the compiler's maintainer.",
    "This indicates a problem within the compiler itself.  Please report "
    "this problem to the compiler's maintainer -- refer to the README file "
    "or release notes for contact information." },

    { TCERR_INTERNAL_ERROR,
    "general internal error",
    "general internal error" },

    { TCERR_MAKE_CANNOT_CREATE_SYM,
    "error creating symbol file \"%s\"",
    "Error creating symbol file \"%s\".  Check to make sure the filename "
    "contains only valid characters, that the path is valid, and that "
    "you have the required permissions to create the file." },

    { TCERR_MAKE_CANNOT_CREATE_OBJ,
    "error creating object file \"%s\"",
    "Error creating object file \"%s\".  Check to make sure the filename "
    "contains only valid characters, that the path is valid, and that "
    "you have the required permissions to create the file." },

    { TCERR_MAKE_CANNOT_CREATE_IMG,
    "error creating image file \"%s\"",
    "Error creating image file \"%s\".  Check to make sure the filename "
    "contains only valid characters, that the path is valid, and that "
    "you have the required permissions to create the file." },

    { TCERR_MAKE_CANNOT_OPEN_SYM,
    "error opening symbol file \"%s\" for reading",
    "Error opening symbol file \"%s\" for reading.  Check that the "
    "file exists and that its name and path are valid." },

    { TCERR_MAKE_CANNOT_OPEN_OBJ,
    "error opening object file \"%s\" for reading",
    "Error opening object file \"%s\" for reading.  Check that the "
    "file exists and that its name and path are valid." },

    { TCERR_MAKE_CANNOT_OPEN_IMG,
    "error opening image file \"%s\" for reading",
    "Error opening image file \"%s\" for reading.  Since this is an "
    "intermediate file created during the build process, this probably "
    "indicates a problem with the filename path, directory permissions, "
    "or disk space." },

    { TCERR_TOO_MANY_ERRORS,
    "too many errors (try fixing the first couple of errors and recompiling)",
    "Too many errors - aborting compilation.  Don't panic!  Your source "
    "code probably doesn't have nearly as many errors as the compiler "
    "is reporting.  In all likelihood, the compiler got tripped up after "
    "the first couple of errors and hasn't been able to get itself "
    "re-synchronized with your code - it thinks there are errors only "
    "because it's trying to interpret the code in the wrong context.  You "
    "should simply fix the first few errors, then try compiling your "
    "program again - chances are the compiler will get a lot further "
    "if you fix just the first few errors.  If you want, you can save a "
    "copy of the current source code and send it to TADS's author, who "
    "might be able to make the compiler a little smarter about dealing "
    "with whatever is confusing it so badly." },

    { TCERR_MODULE_NAME_COLLISION,
    "module %s the has same name as an existing module",
    "The module \"%s\" has the same name as an existing module elsewhere "
    "in the project.  The root filename of each module must be unique, "
    "because the two modules' object files might otherwise overwrite one "
    "another.  You must change the name of one of the modules so that "
    "each module has a unique name." },

    { TCERR_MODULE_NAME_COLLISION_WITH_LIB,
    "module %s has the same name as an existing module from library \"%s\"",
    "The module %s has the same name as an existing module included "
    "from the library \"%s\".  The root filename of each module must be "
    "unique, even for files included from libraries.  If more than one "
    "module has the same root name, the modules' object files would "
    "overwrite one another.  You must change the name of this module "
    "to a name not used anywhere else in the project." },

    { TCERR_SOURCE_FROM_LIB,
    "\"%s\" (from library \"%s\")",
    "\"%s\" (from library \"%s\")" },

    { TCERR_CANNOT_CREATE_DIR,
    "unable to create directory \"%s\"",
    "An error occurred creating the directory/folder \"%s\". This might "
    "mean that the name contains invalid characters, the disk is "
    "full, or you don't have the necessary permissions or privileges "
    "to create a new file in the parent folder." },

    { TCERR_CONST_DIV_ZERO,
    "divide by zero in constant expression",
    "Division by zero in constant expression.  (The compiler was trying "
    "to calculate the value of this expression because it appears to be "
    "constant.  Check for any macros that might expand to zero, and check "
    "for proper placement of parentheses.)" },

    { TCERR_NO_MEM_PRS_TREE,
    "out of memory - cannot allocate parse tree block",
    "Out of memory.  (The compiler was trying to allocate space for a "
    "\"parse tree block,\" which stores an intermediate form of your "
    "program source code during compilation.  If possible, make more "
    "memory available to the compiler by closing other applications or "
    "reconfiguring any background tasks, services, or device drivers "
    "that can be changed to use less memory.)" },

    { TCERR_PRS_BLK_TOO_BIG,
    "parse block too big (size=%ld)",
    "Parse block too big (size=%ld)." },

    { TCERR_INVALID_LVALUE,
    "invalid lvalue - cannot assign to expression on left of \"%s\"",
    "Invalid \"lvalue\".  The expression on the left-hand side of the "
    "operator \"%s\" cannot be used as the destination of an assignment.  "
    "You can only assign values to expressions such as local variables, "
    "object properties, and list elements. "
    "Check for missing or extra parentheses and operators. " },

    { TCERR_QUEST_WITHOUT_COLON,
    "missing ':' in '?' conditional expression",
    "The ':' part of a '?' conditional expression is missing.  A '?' "
    "expression uses the syntax (condition ? true-part : false-part).  "
    "Check for missing parentheses or other errors in the "
    "condition or true-part expression." },

    { TCERR_INVALID_UNARY_LVALUE,
    "invalid lvalue - cannot apply \"%s\" operator to expression",
    "The \"%s\" operator cannot be applied to this expression.  This "
    "operator can only be applied to an expression that can be used in "
    "an assignment, such as local variables, object properties, and "
    "list elements.  Check for missing or extra parentheses." },

    { TCERR_DELETE_OBSOLETE,
    "operator 'delete' is obsolete; expression has no effect",
    "The 'delete' operator is obsolete.  The compiler still accepts "
    "'delete' expressions, but they have no effect at run-time.  (The "
    "run-time system now provides automatic deletion of objects as "
    "they become unreachable, so explicit deletion is no longer "
    "necessary.  You can simply remove all 'delete' expressions from "
    "your program.)" },

    { TCERR_NO_ADDRESS,
    "invalid address expression - can't apply '&' operator",
    "The unary '&' operator cannot be applied to this expression.  You "
    "can only apply the '&' prefix operator to function names and property "
    "names.  Check the expression after the '&' to make sure it is a valid "
    "function or property name, and check for other expression errors, "
    "such as unbalanced parentheses." },

    { TCERR_CONST_UNARY_REQ_NUM,
    "unary '%s' operator requires numeric value in constant expression",
    "The '%s' operator must be followed by a numeric value in a constant "
    "expression.  The value after the operator is a constant value, but "
    "is not a number." },

    { TCERR_CONST_BINARY_REQ_NUM,
    "binary '%s' operator requires numeric value in constant expression",
    "The '%s' operator must be applied only to numeric values in a constant "
    "expression.  Both of the values are constants, but both are not "
    "numbers." },

    { TCERR_CONST_BINPLUS_INCOMPAT,
    "incompatible constant types for two-operand '+' operator",
    "The constant types in this expression are not compatible for use "
    "with the two-operand '+' operator.  You can add two numbers, or add a "
    "non-list value to a string, or add a value to a list; other "
    "combinations are not allowed." },

    { TCERR_EXPR_MISSING_RPAR,
    "expected ')' but found \"%~.*s\"",
    "This expression is missing a right parenthesis ')' - \"%~.*s\" is "
    "where the ')' should go.  The parenthesis is required to "
    "match an earlier left parenthesis '('.  Check to make sure the "
    "parentheses are properly balanced, and check for unterminated "
    "strings and missing operators." },

    { TCERR_BAD_PRIMARY_EXPR,
    "expected integer, string, symbol, '[', or '(', but found \"%~.*s\"",
    "Invalid expression; expected an integer value, a string value (in "
    "single or double quotes), a symbolic name (such as a function, "
    "object, or property name), a list constant enclosed in square "
    "brackets '[ ]', or an expression in parentheses '( )', but "
    "found \"%~.*s\"." },

    { TCERR_CONST_BAD_COMPARE,
    "incompatible types for comparison in constant expression",
    "This constant expression contains a comparison operator ('<', '<=', "
    "'>', or '>=') that attempts to compare values of incompatible types "
    "(you can compare an integer to an integer, a string to a string, "
    "or a floating-point value to another floating point value).  Check "
    "the expression and correct the invalid comparison." },

    { TCERR_EXPECTED_SEMI,
    "expected ';', but found \"%~.*s\"",
    "Expected a semicolon ';' but found \"%~.*s\".  Please add the "
    "required semicolon.  If a semicolon is already present, check "
    "for unbalanced parentheses or other expression errors." },

    { TCERR_EXPECTED_STR_CONT,
    "expected '>>' and the string continuation, but found \"%~.*s\"",
    "Expected '>>' after the embedded expression, followed by the "
    "continuation of the string, but found \"%~.*s\" instead.  Check the "
    "embedded expression (between '<<' and '>>') for errors, such as "
    "unbalanced parentheses, and check that the string is properly "
    "continued after a '>>' sequence." },

    { TCERR_EXPECTED_ARG_COMMA,
    "expected ',' or ')' in argument list, but found \"%~.*s\"",
    "Expected a comma ',' or right parenthesis ')' in an argument list, "
    "but found \"%~.*s\".  Arguments must be separated by commas, and "
    "the entire list must be enclosed in parentheses '( )'.  Check for "
    "errors and unbalanced parentheses in the argument expression." },

    { TCERR_EXPECTED_SUB_RBRACK,
    "expected ']' at end of subscript, but found \"%~.*s\"",
    "Expected a right square bracket ']' at the end of the subscript "
    "(index) expression, but found \"%~.*s\" instead.  A ']' is required "
    "to match the '[' at the start of the subscript.  Check for errors "
    "and unbalanced parentheses in the subscript expression." },

    { TCERR_INVALID_PROP_EXPR,
    "expected property name or parenthesized expression after '.', "
    "found \"%~.*s\"",
    "Expected a property name or a parenthesized expression (which must "
    "evaluate at run-time to a property address value) after '.', but "
    "found \"%~.*s\", which is not a valid property-valued expression." },

    { TCERR_LIST_MISSING_RBRACK,
    "expected ']' or a list element, but found \"%~.*s\"",
    "Expected to find a list element expression or a right square "
    "bracket ']' ending the list, but found \"%~.*s\" instead.  "
    "The compiler will assume that this is the end of the list.  "
    "Please insert the missing ']', or check for errors in the list "
    "expressions." },

    { TCERR_LIST_EXTRA_RPAR,
    "found extraneous ')' in list; ignored",
    "Found an extra right parenthesis ')' where a list element "
    "expression or a right square bracket ']' ending the list should be.  "
    "The compiler will assume that the ')' is extraneous and will "
    "ignore it.  Please remove the extra ')' or check the list "
    "for unbalanced parentheses or other expression errors." },

    { TCERR_LIST_EXPECT_COMMA,
    "expected ',' separating list elements, but found \"%~.*s\"",
    "A comma ',' must be used to separate each element of a list.  "
    "The compiler found \"%~.*s\" where a comma should be.  Please insert "
    "the missing comma between the list elements, or check for an "
    "error in the preceding list element expression." },

    { TCERR_CONST_IDX_NOT_INT,
    "index value must be an integer in constant list index expression",
    "The list index expression has a constant value, but the list index "
    "is not a number.  A list index must have a numeric value.  Check "
    "the expression in the square brackets '[ ]' and correct any errors." },

    { TCERR_CONST_IDX_RANGE,
    "index value out of range in constant list index expression",
    "The list index expression is out of range for the list.  The index "
    "value must be a number from 1 to the number of elements in the "
    "list.  Check the expression in the square brackets '[ ]' and "
    "correct the value." },

    { TCERR_UNTERM_STRING,
    "unterminated string literal: string started with %c%~.*s%c",
    "Unterminated string.  The string starting with %c%~.*s%c does not have a "
    "matching close quote before the end of the file.  Please insert the "
    "missing quote mark.  If the string looks properly terminated, check "
    "the previous string (or the previous few strings), since an unbalanced "
    "quote mark in an earlier string can sometimes propagate to later "
    "strings." },

    { TCERR_EXPECTED_ARG_RPAR,
    "expected ')' at end of argument list, but found \"%~.*s\"",
    "Expected a right parenthesis ')' at the end of an argument list, "
    "but found \"%~.*s\".  The compiler is assuming that this is the end "
    "of the statement.  Please insert the missing parenthesis, or check "
    "the argument list for unbalanced parentheses or other errors." },

    { TCERR_EXTRA_RPAR,
    "unexpected ')' found - ignored",
    "The expression contains an unbalanced right parenthesis ')'.  "
    "The compiler will ignore the extra ')'.  Remove the extra ')', "
    "or check the expression for other errors." },

    { TCERR_EXTRA_RBRACK,
    "unexpected ']' found - ignored",
    "The expression contains an unbalanced right square bracket ']'.  "
    "The compiler will ignore the extra ']'.  Remove the extra ']', "
    "or check the expression for other errors." },

    { TCERR_EXPECTED_OPERAND,
    "expected an operand, but found \"%~.*s\"",
    "The expression is missing an operand value - the compiler found "
    "\"%~.*s\" instead of a valid operand.  Please check the expression and "
    "correct the error." },

    { TCERR_PROPSET_REQ_STR,
    "expected property name pattern string after 'propertyset' - "
        "found \"%~.*s\"",
    "A property name pattern string, enclosed in single quotes, is "
    "required after the 'propertyset' keyword, but the compiler found "
    "\"%~.*s\" instead.  Add the missing pattern string." },

    { TCERR_INH_CLASS_SYNTAX,
    "\"inherited superclass\" syntax - expected '.', found \"%~.*s\"",
    "Invalid syntax in \"inherited class\" expression.  The class name "
    "must be followed by '.', but the compiler found \"%~.*s\" instead.  "
    "Check and correct the syntax." },

    { TCERR_UNDEF_SYM,
    "undefined symbol \"%~.*s\"",
    "The symbol \"%~.*s\" is not defined.  Check the spelling of the "
    "symbol name, and make sure that the corresponding local variable, "
    "object, or function definition is entered correctly.  This error "
    "could be the result of a syntax error in the original declaration "
    "of the symbol; if the declaration has an error, correct that error "
    "first, then try recompiling." },

    { TCERR_ASSUME_SYM_PROP,
    "undefined symbol \"%~.*s\" - assuming this is a property name",
    "The symbol \"%~.*s\" is undefined, but appears from context to be "
    "a property name.  The compiler is assuming that this is a property.  "
    "Check the spelling of the symbol.  If this assumption is correct, "
    "you can avoid this warning by explicitly declaring a value to the "
    "property in an object definition rather than in method code." },

    { TCERR_CONST_BINMINUS_INCOMPAT,
    "incompatible constant types for two-operand '-' operator",
    "The constant types in this expression are not compatible for use "
    "with the two-operand '-' operator.  You can subtract one number "
    "from another, or subtract a value from a list; other combinations "
    "are not allowed." },

    { TCERR_REQ_SYM_FORMAL,
    "expected a symbol in formal parameter list, but found \"%~.*s\"",
    "A symbol name is required for each formal parameter.  The compiler "
    "found \"%~.*s\" instead of a symbol for the parameter name." },

    { TCERR_REQ_COMMA_FORMAL,
    "expected a comma in formal parameter list, but found \"%~.*s\"",
    "The formal parameter list is missing a comma between two parameter "
    "names - a comma should come before \"%~.*s\" in the parameter list." },

    { TCERR_MISSING_LAST_FORMAL,
    "missing parameter name at end of formal parameter list",
    "The last parameter name in the formal parameter list is missing.  "
    "Insert a parameter name before the ')', or remove the extra comma "
    "at the end of the list." },

    { TCERR_MISSING_RPAR_FORMAL,
    "missing right parenthesis ')' at end of formal parameter list - "
        "found \"%~.*s\"",
    "The right parenthesis ')' at the end of the formal parameter list "
    "is missing.  The parenthesis should come before \"%~.*s\". " },

    { TCERR_FORMAL_REDEF,
    "formal parameter \"%~.*s\" defined more than once",
    "The formal parameter name \"%~.*s\" is defined more than once in the "
    "parameter list.  Each parameter name can be used only once in the "
    "same list.  Remove the redundant variable, or change its name." },

    { TCERR_EQ_WITH_METHOD_OBSOLETE,
    "'=' is not allowed with a method definition",
    "An equals sign '=' is not allowed in a method definition.  You can "
    "only use '=' when defining a simple value for a property, not to "
    "define method code.  (TADS 2 used '=' in methods, but this syntax is "
    "now obsolete.)  Remove the '='." },

    { TCERR_REQ_LBRACE_CODE,
    "expected '{' at start of method code body, but found \"%~.*s\"",
    "An open brace '{' was expected before a method's program code body, "
    "but the compiler found \"%~.*s\" instead." },

    { TCERR_EOF_IN_CODE,
    "unexpected end of file in code block - '}' missing",
    "The compiler reached the end of the file before the current function "
    "or method was finished.  A close brace '}' is probably missing - "
    "insert the close brace at the end of the function or method." },

    { TCERR_REQ_LPAR_IF,
    "expected '(' after \"if\", but found \"%~.*s\"",
    "An open parenthesis '(' is required after the keyword \"if\" - "
    "the compiler found \"%~.*s\" instead.  The compiler will assume "
    "that a parenthesis was intended.  Please correct the syntax "
    "by inserting a parenthesis." },

    { TCERR_MISPLACED_ELSE,
    "misplaced \"else\" - no corresponding \"if\" statement",
    "This \"else\" clause is invalid because it is not properly associated "
    "with an \"if\" statement.  Most likely, this is because the group of "
    "statements following the \"if\" is not properly enclosed in braces "
    "'{ }', or because the braces just before the \"else\" aren't properly "
    "balanced, or because there are too many or too few semicolons ';' after "
    "the statement following the \"if\" and before the \"else\".  Check "
    "braces to make sure they're properly balanced, and check the statement "
    "or statements before the \"else\" for proper syntax, especially "
    "for the correct number of terminating semicolons." },

    { TCERR_MISPLACED_CASE,
    "misplaced \"case\" keyword - not in a \"switch\" statement",
    "This \"case\" clause is invalid because it is not part of a \"switch\" "
    "statement.  The most likely cause is that braces before this \"case\" "
    "keyword aren't properly balanced.  \"case\" labels must be enclosed "
    "directly by the \"switch\" - they cannot be enclosed within statements "
    "or braces inside the \"switch\".  Check code preceding the \"case\" "
    "clause for proper syntax and balanced braces." },

    { TCERR_MISPLACED_DEFAULT,
    "misplaced \"default\" keyword - not in a \"switch\" statement",
    "This \"default\" clause is invalid because it is not part of a "
    "\"switch\" statement.  The most likely cause is that braces before "
    "this \"default\" keyword aren't properly balanced.  A \"default\" "
    "label must be enclosed directly by the \"switch\" - it cannot be "
    "enclosed within a statement or braces within the \"switch\" body.  "
    "Check code preceding the \"default\" clause for proper syntax "
    "and balanced braces." },

    { TCERR_ELLIPSIS_NOT_LAST,
    "'...' cannot be followed by additional formal parameters",
    "An ellipsis '...' cannot be followed by additional parameters "
    "in an argument list.  Move the '...' to the end of the parameter "
    "list, or remove the extraneous parameters after the ellipsis." },

    { TCERR_LOCAL_REQ_COMMA,
    "expected ',' or ';' after local variable, but found \"%~.*s\"",
    "The compiler expected a comma ',' or semicolon ';' after a local "
    "variable declaration, but found \"%~.*s\" instead.  If you're "
    "defining an additional variable, add a comma before the additional "
    "variable; if not, check for a missing semicolon." },

    { TCERR_LOCAL_REQ_SYM,
    "expected symbol name in local variable declaration, but found \"%~.*s\"",
    "A symbol name is required in the local variable declaration, "
    "but the compiler found \"%~.*s\" instead.  Check the syntax "
    "and correct the error." },

    { TCERR_FUNC_REQ_SYM,
    "expected symbol after 'function', but found \"%~.*s\"",
    "A symbol name is required after the 'function' keyword, but the "
    "compiler found \"%~.*s\" instead.  Check the function definition "
    "syntax." },

    { TCERR_REQ_CODE_BODY,
    "expected ';', '(', or '{', but found \"%~.*s\"",
    "The compiler expected a left parenthesis '(' starting a formal "
    "parameter list, a left brace '{' starting a code body, or a semicolon "
    "';' terminating the statement, but found \"%~.*s\".  Check the function "
    "definition syntax." },

    { TCERR_REQ_FUNC_OR_OBJ,
    "expected function or object definition, but found \"%~.*s\"",
    "The compiler expected a function or object definition, but "
    "found \"%~.*s\".  Check the syntax, and check for unbalanced "
    "braces '{ }' and other syntax errors preceding this line." },

    { TCERR_RET_REQ_EXPR,
    "expected ';' or expression after \"return\", but found \"%~.*s\"",
    "The \"return\" keyword must be followed by an expression giving the "
    "value to return, or by a semicolon ';' if there is no value to "
    "return; the compiler found \"%~.*s\" instead.  Check the syntax, and "
    "insert the missing semicolon or expression as appropriate." },

    { TCERR_UNREACHABLE_CODE,
    "unreachable statement",
    "This statement cannot be reached, because the previous statement "
    "returns or throws an exception.  This code will never be executed.  "
    "Check the logic to determine if the code is necessary; if not, "
    "remove the code.  If the code is necessary, you must determine "
    "why the code is unreachable and correct the program logic." },

    { TCERR_RET_VAL_AND_VOID,
    "code has \"return\" statements both with and without values",
    "This code has \"return\" statements both with and without values.  "
    "A function or method's \"return\" statements should consistently "
    "return values or not; these should not be mixed in a single "
    "function or method, because callers will not have predictable "
    "results when using the function's return value." },

    { TCERR_RET_VAL_AND_IMP_VOID,
    "code has \"return\" with value but also falls off end",
    "This code has one or more \"return\" statements that explicitly "
    "return a value from the function, but also \"falls off\" the end "
    "of the function without a \"return\" statement, which will result "
    "in a return without a value.  The last statement in the function "
    "or method should be a \"return\" with a value, for consistency "
    "with the other \"return\" statements." },

    { TCERR_REQ_INTRINS_NAME,
    "expected function set name string after \"intrinsic\", "
        "but found \"%~.*s\"",
    "The \"intrinsic\" keyword must be followed by the global name of the "
    "function set, enclosed in single-quotes, but the compiler found "
    "\"%~.*s\" instead.  Check the \"intrinsic\" statement syntax." },

    { TCERR_REQ_INTRINS_LBRACE,
    "expected '{' after intrinsic name, but found \"%~.*s\"",
    "The function set listing for an \"intrinsic\" statement must be "
    "closed in braces '{ }'.  The compiler found \"%~.*s\" after the "
    "function set name, where the open brace '{' should be.  Check the "
    "syntax." },

    { TCERR_EOF_IN_INTRINS,
    "end of file in \"intrinsic\" list - '}' is probably missing",
    "The compiler reached the end of the file while still scanning "
    "an \"intrinsic\" statement's function set listing.  The closing "
    "brace '}' of the function set list is probably missing; check "
    "for the missing brace." },

    { TCERR_REQ_INTRINS_LPAR,
    "expected '(' after function name in intrinsic list, but found \"%~.*s\"",
    "An open parenthesis '(' is required after the name of a function "
    "in an intrinsic function list, but the compiler found \"%~.*s\" "
    "instead.  Check the syntax and insert the missing parenthesis." },

    { TCERR_REQ_INTRINS_SYM,
    "expected function name in intrinsic list, but found \"%~.*s\"",
    "The compiler expected the name of a function in the intrinsic "
    "function list, but found \"%~.*s\" instead.  Check the syntax of "
    "the statement, and check for unbalanced parentheses and braces." },

    { TCERR_REQ_FOR_LPAR,
    "expected '(' after \"for\", but found \"%~.*s\"",
    "An open parenthesis '(' is required after the \"for\" keyword, "
    "but the compiler found \"%~.*s\" instead.  Check the syntax "
    "and insert the missing parenthesis." },

    { TCERR_LOCAL_REDEF,
    "local variable \"%~.*s\" defined more than once",
    "The local variable name \"%~.*s\" is defined more than once in this "
    "scope.  Each local variable name can be used only once at the same "
    "level of braces '{ }'.  Remove the redundant variable, or change its "
    "name." },

    { TCERR_REQ_FOR_LOCAL_INIT,
    "initializer expected after local variable name in \"for\", but found "
        "\"%~.*s\"",
    "A local variable defined within a \"for\" statement's initialization "
    "clause requires an initializer expression.  The compiler expected to "
    "find an assignment operator after the local variable name, but found "
    "\"%~.*s\" instead.  Check the syntax, and add the missing initializer "
    "to the local variable definition." },

    { TCERR_MISSING_FOR_INIT_EXPR,
    "missing expression after comma in \"for\" initializer",
    "An expression must follow a comma in a \"for\" initializer list.  "
    "Check the expression, and remove the extra comma before the "
    "semicolon, or supply the missing expression." },

    { TCERR_MISSING_FOR_PART,
    "missing expression in \"for\" statement - expected ';', found '%~.*s'",
    "A \"for\" statement requires three expressions, separated by "
    "semicolons ';'.  This statement doesn't seem to have all of the required "
    "parts - it ends unexpectedly at \"%~.*s\".  Add the missing "
    "parts or correct the syntax." },

    { TCERR_REQ_FOR_INIT_COMMA,
    "expected ',' or ';' in \"for\" initializer, but found \"%~.*s\"",
    "A comma ',' or semicolon ';' was expected in the \"for\" statement's "
    "initializer list, but the compiler found \"%~.*s\" instead.  Check "
    "the expression syntax." },

    { TCERR_REQ_FOR_COND_SEM,
    "expected ';' after \"for\" condition, but found \"%~.*s\"",
    "A semicolon ';' was expected after the \"for\" statement's "
    "condition expression, but the compiler found \"%~.*s\" instead.  "
    "Check the expression syntax." },

    { TCERR_REQ_FOR_RPAR,
    "missing ')' at end of \"for\" expression list - found \"%~.*s\"",
    "A closing parenthesis ')' is required at the end of the \"for\" "
    "statement's expression list, but the compiler found \"%~.*s\" instead.  "
    "Check the syntax, and insert the missing parenthesis, or correct "
    "unbalanced parentheses or other syntax errors earlier in the "
    "expression list." },

    { TCERR_FOR_COND_FALSE,
    "\"for\" condition is always false - body and reinitializer are "
        "unreachable",
    "The condition of this \"for\" statement is always false, so the "
    "body and reinitialization expression will never be executed." },

    { TCERR_REQ_WHILE_LPAR,
    "missing '(' after \"while\" - found \"%~.*s\"",
    "An open parenthesis '(' is required after the \"while\" keyword, but "
    "the compiler found \"%~.*s\" instead.  Check the syntax and insert "
    "the missing parenthesis." },

    { TCERR_REQ_WHILE_RPAR,
    "missing ')' after \"while\" expression - found \"%~.*s\"",
    "A close parenthesis ')' is required after the expression condition "
    "in a \"while\" statement, but the compiler found \"%~.*s\" instead.  "
    "Check the syntax and insert the missing parenthesis." },

    { TCERR_WHILE_COND_FALSE,
    "\"while\" condition is always false - loop body is unreachable",
    "The condition of this \"while\" statement is always false, so the "
    "body of the loop will never be executed." },

    { TCERR_REQ_DO_WHILE,
    "expected \"while\" in \"do\" statement, but found \"%~.*s\"",
    "This \"do\" statement is missing the required \"while\" keyword "
    "immediately after the loop body.  The compiler found \"%~.*s\" where "
    "the \"while\" keyword shuld be.  Check the syntax and insert the "
    "missing \"while\" keyword." },

    { TCERR_MISPLACED_CATCH,
    "misplaced \"catch\" clause - must be associated with \"try\"",
    "This \"catch\" clause is invalid because it is not part of a \"try\""
    " statement.  The most likely cause is that braces before this \"catch\""
    " keyword aren't properly balanced.  Check braces preceding the "
    "\"catch\" clause for proper syntax." },
    
    { TCERR_MISPLACED_FINALLY,
    "misplaced \"finally\" clause - must be associated with \"try\"",
    "This \"finally\" clause is invalid because it is not part of a \"try\""
    " statement.  The most likely cause is that braces before this "
    "\"finally\" keyword aren't properly balanced.  Check braces "
    "preceding the \"finally\" clause for proper syntax." },

    { TCERR_REQ_SWITCH_LPAR,
    "missing '(' after \"switch\" - found \"%~.*s\"",
    "An open parenthesis '(' is required after the \"switch\" keyword, but "
    "the compiler found \"%~.*s\" instead.  Check the syntax and insert "
    "the missing parenthesis." },

    { TCERR_REQ_SWITCH_RPAR,
    "missing ')' after \"switch\" expression - found \"%~.*s\"",
    "A close parenthesis ')' is required after the controlling expression "
    "of a \"switch\" statement, but the compiler found \"%~.*s\" instead.  "
    "Check the syntax and insert the missing parenthesis." },

    { TCERR_REQ_SWITCH_LBRACE,
    "missing '{' after \"switch\" expression - found \"%~.*s\"",
    "An open brace '{' is required after the controlling expression "
    "of a \"switch\" statement, but the compiler found \"%~.*s\" instead.  "
    "The body of the \"switch\" must be enclosed in braces '{ }'.  "
    "Check the syntax and insert the missing parenthesis." },

    { TCERR_UNREACHABLE_CODE_IN_SWITCH,
    "code before first \"case\" or \"default\" label in \"switch\" is "
        "not allowed",
    "This statement precedes the first \"case\" or \"default\" label "
    "in the \"switch\" body - all code within a \"switch\" body must "
    "be reachable from a \"case\" or \"default\" label.  Even \"local\" "
    "declarations must be within a labelled section of the \"switch\" "
    "body.  Check for a missing \"case\" label, or move the code so that "
    "it is outside the \"switch\" body or after a \"case\" label." },

    { TCERR_EOF_IN_SWITCH,
    "end of file in \"switch\" body",
    "End of file found in \"switch\" body.  The \"switch\" body's "
    "braces '{ }' are probably not properly balanced.  Check the code "
    "within the \"switch\" body for unbalanced braces and other errors." },

    { TCERR_CODE_LABEL_REDEF,
    "code label \"%~.*s\" already defined in this function or method",
    "The code label \"%~.*s\" is already defined in this function or "
    "method.  A code label can be used only once within each function "
    "or method; code labels always have function- or method-level scope, "
    "even when they're nested within braces.  Change the name of one of "
    "the conflicting labels so that the two labels have different names." },

    { TCERR_REQ_CASE_COLON,
    "missing ':' after \"case\" expression - found \"%~.*s\"",
    "A colon ':' is required after the \"case\" expression, but the "
    "compiler found \"%~.*s\" instead.  Check the expression for errors, "
    "and insert the missing colon after the expression." },

    { TCERR_CASE_NOT_CONSTANT,
    "\"case\" expression has a non-constant value",
    "The expression in this \"case\" label does not have a constant "
    "value.  \"case\" expressions must always be constants or expressions "
    "involving only constants.  Check the expression and remove "
    "references to local variables, object properties, or any other "
    "non-constant values." },

    { TCERR_REQ_DEFAULT_COLON,
    "missing ':' after \"default\" - found \"%~.*s\"",
    "A colon ':' is required after the \"default\" keyword, but the "
    "compiler found \"%~.*s\" instead.  Insert the missing colon "
    "after the keyword." },

    { TCERR_DEFAULT_REDEF,
    "this \"switch\" already has a \"default:\" label",
    "This \"switch\" statement already has a \"default:\" label.  A "
    "\"switch\" statement can have at most one \"default:\" label.  Remove "
    "the redundant label." },

    { TCERR_TRY_WITHOUT_CATCH,
    "\"try\" statement has no \"catch\" or \"finally\" clauses",
    "This \"try\" statement has no \"catch\" or \"finally\" clauses.  A "
    "\"try\" statement must have at least one such clause, since it is "
    "otherwise superfluous.  Check for unbalanced braces in the body of "
    "the \"try\" block." },

    { TCERR_REQ_CATCH_LPAR,
    "missing '(' after \"catch\" - found \"%~.*s\"",
    "An open parenthesis '(' is required after the \"catch\" keyword, but "
    "the compiler found \"%~.*s\" instead.  Check the syntax and insert "
    "the missing parenthesis." },

    { TCERR_REQ_CATCH_RPAR,
    "missing ')' after \"catch\" variable name - found \"%~.*s\"",
    "A close parenthesis ')' is required after the variable name "
    "of a \"switch\" clause, but the compiler found \"%~.*s\" instead.  "
    "Check the syntax and insert the missing parenthesis." },

    { TCERR_REQ_CATCH_CLASS,
    "expected class name in \"catch\" clause - found \"%~.*s\"",
    "The class name of the exception to catch is required in the "
    "\"catch\" clause, but the compiler found \"%~.*s\" instead.  Check "
    "and correct the syntax." },

    { TCERR_REQ_CATCH_VAR,
    "expected variable name in \"catch\" clause - found \"%~.*s\"",
    "The name of a local variable (which can either be an existing "
    "variable or can be a new variable implicitly defined by this use) "
    "is required in the \"catch\" clause, but the compiler found \"%~.*s\" "
    "instead.  Check and correct the syntax." },

    { TCERR_CATCH_VAR_NOT_LOCAL,
    "\"%~.*s\" is not a local variable, so is not a valid \"catch\" target "
        "variable",
    "The symbol \"%~.*s\" is defined as something other than a local "
    "variable, so it cannot be used as the target of this \"catch\" clause.  "
    "Check for a conflicting symbol, and either remove the conflicting "
    "symbol or rename the \"catch\" variable." },

    { TCERR_BREAK_REQ_LABEL,
    "label name expected after \"break\", but found \"%~.*s\"",
    "A label name was expected after the \"break\" keyword, but the "
    "compiler found \"%~.*s\" instead.  This might indicate that a "
    "semicolon after \"break\" is missing.  Check the syntax." },

    { TCERR_CONT_REQ_LABEL,
    "label name expected after \"continue\", but found \"%~.*s\"",
    "A label name was expected after the \"continue\" keyword, but the "
    "compiler found \"%~.*s\" instead.  This might indicate that a "
    "semicolon after \"continue\" is missing.  Check the syntax." },

    { TCERR_GOTO_REQ_LABEL,
    "label name expected after \"goto\", but found \"%~.*s\"",
    "A label name was expected after the \"goto\" keyword, but the "
    "compiler found \"%~.*s\" instead.  Check the syntax and insert "
    "the missing label name." },

    { TCERR_REDEF_AS_FUNC,
    "symbol \"%~.*s\" is already defined - can't redefine as function",
    "The symbol \"%~.*s\" is already defined, so you cannot use it "
    "as the name of a function here.  This symbol is already being "
    "used as the name of an object or property elsewhere "
    "in your program.  Change the name to a unique symbol." },

    { TCERR_INVAL_EXTERN,
    "invalid \"extern\" type specifier \"%~.*s\"",
    "Invalid keyword \"%~.*s\" following \"extern\".  The \"extern\" "
    "keyword must be followed by \"function\" or \"object\" to indicate "
    "the type of the external symbol to declare." },

    { TCERR_EXTERN_NO_CODE_BODY,
    "code body is not allowed in an \"extern function\" declaration",
    "This \"extern function\" declaration is not valid because it has "
    "a left brace introducing a code body after the function prototype.  "
    "An extern function declaration can have only the prototype, because "
    "it specifies that the actual code body of the function is defined "
    "in another module.  Either remove the \"extern\" keyword or remove "
    "the code body." },

    { TCERR_FUNC_REDEF,
    "function \"%~.*s\" is already defined",
    "The function \"%~.*s\" is already defined earlier in the program.  "
    "Each function must have a unique name.  Remove the redundant "
    "definition or change the name of one of the functions." },

    { TCERR_INCOMPAT_FUNC_REDEF,
    "function \"%~.*s\" has an incompatible previous declaration",
    "The function \"%~.*s\" has an incompatible declaration previously "
    "in the program.  The earlier definition could be from an \"extern\" "
    "declaration in this source file, or it could come from a symbol "
    "file for another module in your program.  Each definition of "
    "a function must declare the same parameter list for the function.  "
    "Check for other declarations of this function and correct the "
    "inconsistency." },

    { TCERR_OBJDEF_REQ_COLON,
    "expected ':' after object name in object definition, but found \"%~.*s\"",
    "A colon ':' is required after the object name in an object "
    "definition, but the compiler found \"%~.*s\" instead.  Check the "
    "object syntax and insert the missing colon." },

    { TCERR_OBJDEF_REQ_SC,
    "expected superclass name in object definition, but found \"%~.*s\"",
    "The name of a superclass was expected in the object definition, "
    "but the compiler found \"%~.*s\".  Check the object syntax; add or "
    "correct the superclass name, or correct other syntax errors." },

    { TCERR_OBJDEF_OBJ_NO_SC,
    "superclasses cannot be specified with \"object\" as the base class",
    "This object specifies \"object\" as the base class, but also lists "
    "named superclasses.  This is not valid -- a basic \"object\" "
    "definition cannot also specify named superclasses." },

    { TCERR_REDEF_AS_OBJ,
    "symbol \"%~.*s\" is already defined - can't redefine as object",
    "The symbol \"%~.*s\" is already defined, so you cannot use it "
    "as the name of an object here.  This symbol is already being "
    "used as the name of a function or property elsewhere "
    "in your program.  Change the name to a unique symbol." },

    { TCERR_OBJ_REDEF,
    "object \"%~.*s\" is already defined - can't redefine as object",
    "The object \"%~.*s\" is already defined earlier in the program.  "
    "Each object must have a unique name.  Remove the redundant "
    "definition or change the name of one of the objects." },

    { TCERR_OBJDEF_REQ_PROP,
    "expected property name in object definition, but found \"%~.*s\"",
    "A property name was expected in the object definition, but the "
    "compiler found \"%~.*s\" instead.  Check for a missing semicolon at the "
    "end of the object definition, and check for unbalanced "
    "braces prior to this line." },

    { TCERR_REDEF_AS_PROP,
    "symbol \"%~.*s\" is already defined - can't redefine as property",
    "The symbol \"%~.*s\" is already defined, so you cannot use it as "
    "the name of a property here.  The symbol is already being "
    "used as the name of an object or function elsewhere in the program.  "
    "Change the name to a unique symbol." },

    { TCERR_OBJDEF_REQ_PROPVAL,
    "expected property value or method after property name \"%~.*s\", "
        "but found \"%~.*s\"",
    "A property value or method was expected after the property "
    "name \"%~.*s\", but the compiler found \"%~.*s\".  Check the syntax "
    "and supply a property value expression or method code in braces '{ }'."},

    { TCERR_REPLACE_REQ_OBJ_OR_FUNC,
    "expected \"function\" or object name after \"replace\", but found "
        "\"%~.*s\"",
    "The keyword \"replace\" must be followed by \"function\" or by an "
    "object name, but the compiler found \"%~.*s\" instead." },

    { TCERR_REPMODOBJ_UNDEF,
    "replace/modify cannot be used with an object not previously defined",
    "The \"replace\" and \"modify\" keywords cannot be used with an object "
    "that is not previously defined.  The object must at least be defined "
    "as an \"extern\" object before it can be replaced or modified." },

    { TCERR_DQUOTE_IN_EXPR,
    "a double-quoted string (\"%~.*s\") is not valid within an expression",
    "The double-quoted string value (\"%~.*s\") cannot be used within an "
    "expression, because a double-quoted string has no value.  A "
    "double-quoted string indicates that you simply want to display "
    "the text of the string.  Change "
    "the string's quotes to single quotes (') if you meant to use the "
    "string as a value in an expression." },

    { TCERR_ASI_IN_COND,
    "assignment in condition (possible use of '=' where '==' was intended)",
    "The condition expression contains an assignment.  This frequently "
    "indicates that the '=' (assignment) operator was used where the "
    "'==' (equality comparison) operator was intended.  Check the condition "
    "to ensure that an assignment was actually intended.  If the assignment "
    "is intentional, you can remove this warning by modifying the "
    "condition expression from the current form (x = y) to the form "
    "((x = y) != nil) or ((x = y) != 0) as appropriate, which will not "
    "change the meaning but will make it clear that the assignment is "
    "intentional." },

    { TCERR_REPFUNC_UNDEF,
    "\"replace\"/\"modify\" cannot be used with a function not "
        "previously defined",
    "The \"replace\" and \"modify\" keywords cannot be used with a function "
    "that is not previously defined.  The function must at least be defined "
    "as an \"extern\" function before it can be replaced." },

    { TCERR_REPLACE_PROP_REQ_MOD_OBJ,
    "\"replace\" can be used with a property only in a \"modify\" object",
    "The \"replace\" keyword can be used with a property only in an "
    "object defined with the \"modify\" keyword.  This object is not "
    "defined with \"modify\", so \"replace\" is not allowed with its "
    "property definitions." },

    { TCERR_EXTERN_OBJ_REQ_SYM,
    "expected object name symbol in \"extern\" statement but found \"%~.*s\"",
    "An object name symbol is required after \"extern object\" or "
    "\"extern class\", but the compiler found \"%~.*s\".  Check the syntax "
    "and supply the missing object name." },

    { TCERR_PROP_REDEF_IN_OBJ,
    "property \"%~.*s\" already defined in object",
    "The property \"%~.*s\" is already defined in this object.  An "
    "object can have at most one definition for a given property.  Remove "
    "the redundant property definition." },

    { TCERR_PROP_REQ_EQ,
    "'=' required between property name and value - found \"%~.*s\"",
    "An equals sign '=' is required to separate the property name and "
    "its value; the parser found \"%~.*s\" where the '=' should go.  "
    "Check the syntax and supply the missing '='." },

    { TCERR_LIST_EXPECT_ELEMENT,
    "extra list element expected after comma, but found end of list",
    "An additional list element was expected after the last comma in the "
    "list, but the compiler found the end of the list (a closing bracket "
    "']') instead.  Check the list and either remove the unnecessary "
    "extra comma or add the missing list element." },

    { TCERR_REQ_INTRINS_CLASS_NAME,
    "expected class name string after class name symbol, but found \"%~.*s\"",
    "The \"intrinsic class\" name must be followed by the global name "
    "of the metaclass, enclosed in single-quotes, but the compiler found "
    "\"%~.*s\" instead.  Check the statement syntax." },

    { TCERR_REQ_INTRINS_CLASS_LBRACE,
    "expected '{' after intrinsic class name, but found \"%~.*s\"",
    "The property listing for an \"intrinsic class\" statement must be "
    "closed in braces '{ }'.  The compiler found \"%~.*s\" after the "
    "metaclass name, where the open brace '{' should be.  Check the "
    "syntax." },

    { TCERR_EOF_IN_INTRINS_CLASS,
    "end of file in \"intrinsic class\" list - '}' is probably missing",
    "The compiler reached the end of the file while still scanning "
    "an \"intrinsic class\" statement's property listing.  The closing "
    "brace '}' of the list is probably missing; check "
    "for the missing brace." },

    { TCERR_REQ_INTRINS_CLASS_PROP,
    "expected property name in intrinsic class list, but found \"%~.*s\"",
    "The compiler expected the name of a property in the intrinsic "
    "class property list, but found \"%~.*s\" instead.  Check the syntax of "
    "the statement, and check for unbalanced braces." },

    { TCERR_REQ_INTRINS_CLASS_NAME_SYM,
    "expected class name symbol after \"intrinsic class\", "
        "but found \"%~.*s\"",
    "The \"intrinsic class\" keywords must be followed by the class "
    "name symbol, but the compiler found \"%~.*s\" instead.  Check "
    "the statement syntax." },

    { TCERR_REDEF_INTRINS_NAME,
    "symbol \"%~.*s\" is already defined - can't redefine as intrinsic class",
    "The symbol \"%~.*s\" is already defined, so you cannot use it as "
    "the name of this intrinsic class.  The symbol is already being "
    "used as the name of an object, function, or property elsewhere "
    "in the program.  Change the name to a unique symbol." },

    { TCERR_CANNOT_EVAL_METACLASS,
    "\"%~.*s\" is an intrinsic class name and cannot be evaluated "
        "in an expression",
    "The symbol \"%~.*s\" is an intrinsic class name.  This symbol cannot "
    "be evaluated in an expression, because it has no value.  You can "
    "only use this symbol in specific contexts where class names "
    "are permitted, such as with 'new'." },

    { TCERR_DICT_SYNTAX,
    "expected 'property' or object name after 'dictionary', "
        "but found \"%~.*s\"",
    "The keyword 'property' or an object name symbol must follow "
    "the 'dictionary' keyword, but the compiler found \"%~.*s\" instead.  "
    "Check the 'dictionary' statement syntax." },

    { TCERR_DICT_PROP_REQ_SYM,
    "expected property name in 'dictionary property' list, but "
        "found \"%~.*s\"",
    "The name of a property was expected in the 'dictionary property' "
    "list, but the compiler found \"%~.*s\" instead.  Check the "
    "statement syntax." },

    { TCERR_DICT_PROP_REQ_COMMA,
    "expected comma in 'dictionary property' list, but found \"%~.*s\"",
    "A comma was expected after a property name symbol in a "
    "'dictionary property' list, but the compiler found \"%~.*s\" "
    "instead.  Check the syntax of the property list and ensure that "
    "each item is separated from the next by a comma, and that the "
    "list ends with a semicolon." },

    { TCERR_REDEF_AS_DICT,
    "redefining symbol \"%~.*s\" as dictionary object",
    "The symbol \"%~.*s\" cannot be used as a dictionary object, because "
    "it is already defined as a different type of object.  You must change "
    "the name of this dictionary object, or change the name of the "
    "conflicting object definition." },

    { TCERR_UNDEF_SYM_SC,
    "undefined symbol \"%~.*s\" (used as superclass of \"%~.*s\")",
    "The symbol \"%~.*s\" is undefined.  This symbol is used as a superclass "
    "in the definition of the object \"%~.*s\".  Check the object definition "
    "to ensure that the superclass name is spelled correctly, and check "
    "that the superclass's object definition is correct." },

    { TCERR_VOCAB_REQ_SSTR,
    "vocabulary property requires string value, but found \"%~.*s\"",
    "A vocabulary property value must be one or more single-quoted "
    "string values, but the compiler found \"%~.*s\" instead.  Check the "
    "property definition and use a single-quoted string value." },

    { TCERR_VOCAB_NO_DICT,
    "vocabulary property cannot be defined - no dictionary is active",
    "A vocabulary property cannot be defined for this object because "
    "no dictionary is active.  Insert a 'dictionary' statement prior "
    "to this object definition to establish the dictionary object that "
    "will be used to store this object's vocabulary." },

    { TCERR_LISTPAR_NOT_LAST,
    "variable-argument list parameter must be the last parameter",
    "A variable-argument list parameter (a parameter enclosed in square "
    "brackets '[ ]') is required to be the last parameter in the "
    "parameter list.  Remove the parameters following the list parameter." },

    { TCERR_LISTPAR_REQ_RBRACK,
    "expected ']' after variable-argument list parameter, but found \"%~.*s\"",
    "A closing square bracket ']' was expected following the "
    "variable-argument list parameter name, but the compiler found "
    "\"%~.*s\" instead.  Insert the missing bracket." },

    { TCERR_LISTPAR_REQ_SYM,
    "variable-argument list parameter name expected, but found \"%~.*s\"",
    "A parameter name symbol was expected after the left square bracket '[', "
    "but the compiler found \"%~.*s\" instead.  Check the syntax." },

    { TCERR_DBG_NO_ANON_FUNC,
    "anonymous functions are not allowed in the debugger",
    "Anonymous functions are not allowed in the debugger." },

    { TCERR_ANON_FUNC_REQ_NEW,
    "anonymous function requires 'new' before 'function' keyword",
    "An anonymous function definition requires the keyword 'new' "
    "before the keyword 'function'.  This definition does not contain "
    "the 'new' keyword.  Insert 'new' before 'function' in the "
    "definition." },

    { TCERR_GRAMMAR_REQ_SYM,
    "expected symbol name after 'grammar', but found \"%~.*s\"",
    "A symbol giving the name of a production is required after "
    "the 'grammar' keyword, but the compiler found \"%~.*s\" instead.  "
    "Check the statement syntax." },

    { TCERR_GRAMMAR_REQ_COLON,
    "expected ':' after production name, but found \"%~.*s\"",
    "A colon ':' is required after the name of the production in "
    "a 'grammar' statement, but the compiler found \"%~.*s\" instead.  "
    "Check the statement syntax and insert the missing colon." },

    { TCERR_GRAMMAR_REQ_OBJ_OR_PROP,
    "object or property symbol required in 'grammar' token list "
        "(found \"%~.*s\")",
    "Symbols used in a token list in a 'grammar' statement must "
    "be property or object names.  The symbol \"%~.*s\" is not a "
    "property or object name, so it cannot be used in the token list.  "
    "Remove this symbol from the list or replace it with a symbol "
    "of the appropriate type." },

    { TCERR_GRAMMAR_ARROW_REQ_PROP,
    "'->' in 'grammar' token list must be followed by a property name "
        "(found \"%~.*s\")",
    "An arrow '->' in a 'grammar' statement's token list must be "
    "followed by a property name, but the compiler found \"%~.*s\" "
    "instead.  Check the statement syntax." },

    { TCERR_GRAMMAR_INVAL_TOK,
    "invalid token \"%~.*s\" in 'grammar' token list",
    "The token \"%~.*s\" is not valid in a 'grammar' statement's token "
    "list.  The token list must consist of property names, object names, "
    "and literal strings (in single quotes).  Check the statement syntax." },

    { TCERR_REDEF_AS_GRAMPROD,
    "redefining symbol \"%~.*s\" as grammar production object",
    "The symbol \"%~.*s\" cannot be used as a grammar production, because "
    "it is already defined as a different type of object.  You must change "
    "the name of this production object, or change the name of the "
    "conflicting object definition." },

    { TCERR_GRAMMAR_REQ_PROD,
    "object \"%~.*s\" is not valid in a grammar rule - only production "
        "names are allowed",
    "The object \"%~.*s\" cannot be used in a grammar rule.  Only "
    "production names are allowed.  A production name is a symbol "
    "that is used immediately after the 'grammar' keyword in a "
    "grammar rule definition." },

    { TCERR_ENUM_REQ_SYM,
    "symbol expected in 'enum' list - found \"%~.*s\"",
    "A symbol name was expected in the 'enum' statement's list of "
    "enumerator symbols to define, but the compiler found \"%~.*s\" "
    "instead.  Check the statement syntax." },

    { TCERR_REDEF_AS_ENUM,
    "symbol \"%~.*s\" is already defined - can't be used as an enum name",
    "The symbol \"%~.*s\" is already defined, so it can't be used as an "
    "'enum' name.  An 'enum' name cannot be used for any other global "
    "symbol, such as an object, function, or property name.  Change the "
    "enum name, or change the conflicting symbol definition." },

    { TCERR_ENUM_REQ_COMMA,
    "comma expected in 'enum' symbol list, but found \"%~.*s\"",
    "A comma was expected after a symbol name in the 'enum' statement's "
    "list of enumerator symbols, but the compiler found \"%~.*s\" "
    "instead.  Check the statement syntax and insert the missing "
    "comma." },

    { TCERR_GRAMMAR_BAD_ENUM,
    "enumerator \"%~.*s\" in 'grammar' list is not declared with "
        "'enum token'",
    "The enumerator \"%~.*s\" in the 'grammar' list was not originally "
    "declared with 'enum token'.  Only 'enum token' enumerators can be "
    "used in 'grammar' token lists.  Check the original definition of "
    "the enumerator and change it to 'enum token', or remove the "
    "enumerator from this 'grammar' list." },

    { TCERR_GRAMMAR_STAR_NOT_END,
    "'*' must be the last token in a 'grammar' alternative list "
        "(found \"%~.*s\")",
    "A '*' must be the last token in a 'grammar' alternative list, "
    "because this specifies a match for any remaining input tokens.  "
    "The compiler found \"%~.*s\" after the '*'.  Check the grammar "
    "definition, and end the alternative immediately after the '*'." },

    { TCERR_GRAMMAR_QUAL_NOT_FIRST,
    "grammar qualifiers must precede all tokens",
    "Grammar qualifiers (sequences enclosed in square brackets '[ ]' "
    "within a 'grammar' statement's item list) must precede all "
    "token items in an alternative.  This statement contains a qualifier "
    "that appears after one or more token items.  Move the qualifier "
    "to the start of the alternative's token list." },

    { TCERR_GRAMMAR_QUAL_REQ_SYM,
    "keyword required after '[' in grammar qualifier - found \"%~.*s\"",
    "A keyword is required after the open bracket '[' in a grammar "
    "qualifier, but the compiler found \"%~.*s\" instead.  The keyword "
    "must specify a valid grammar qualifier." },

    { TCERR_BAD_GRAMMAR_QUAL,
    "invalid grammar qualifier \"%~.*s\"",
    "The grammar qualifier \"%~.*s\" is not valid.  Check the statement "
    "syntax." },

    { TCERR_GRAMMAR_QUAL_REQ_INT,
    "grammar qualifier \"[%s]\" requires integer constant value",
    "The grammar qualifier \"[%s]\" requires an integer constant value.  "
    "The expression is missing, invalid, or does not evaluate to a "
    "constant integer value.  Check the qualifier syntax." },

    { TCERR_GRAMMAR_QUAL_REQ_RBRACK,
    "']' required after grammar qualifier - found \"%~.*s\"",
    "A closing bracket ']' is required at the end of a grammar "
    "qualifier, but the compiler found \"%~.*s\" instead.  Check the "
    "syntax, and add the missing ']' or remove extraneous extra text." },
    
    { TCERR_PLUSPROP_REQ_SYM,
    "symbol expected after '+ property', but found \"%~.*s\" instead",
    "A property symbol is required for the '+ property' statement, but the "
    "compiler found \"%~.*s\" instead.  Check the syntax." },

    { TCERR_PLUSOBJ_TOO_MANY,
    "too many '+' signs in object definition - location not defined",
    "This object definition has too many '+' signs.  The immediately "
    "preceding object is not at a deep enough containment level for "
    "this many '+' signs.  Check the previous object's location "
    "definition.  You might need to move the immediately preceding "
    "object definition so that it doesn't come between this object "
    "and the preceding container you wish to refer to." },

    { TCERR_OBJ_TPL_OP_REQ_PROP,
    "property name required after \"%s\" in object template - found \"%~.*s\"",
    "A property name is required after the \"%s\" token in the 'object "
    "template' statement, but the compiler found \"%~.*s\" instead.  Check "
    "the statement syntax." },

    { TCERR_OBJ_TPL_STR_REQ_PROP,
    "property name required in object template string - found %~.*s",
    "The contents of each string in an 'object "
    "template' statement must be a property name symbol, but the compiler "
    "found %~.*s in a string instead.  Check the statement syntax." },

    { TCERR_OBJ_TPL_REQ_RBRACK,
    "expected ']' after object template property name - found \"%~.*s\"",
    "A matching right square bracket ']' is required after a list property "
    "name in an 'object template' statement, but the compiler found "
    "\"%~.*s\" instead.  Check the statement syntax." },

    { TCERR_OBJ_TPL_BAD_TOK,
    "unexpected token \"%~.*s\" in object template",
    "The token \"%~.*s\" is invalid in an 'object template' statement.  "
    "Each entry in the statement must be a property name in single or "
    "double quotes or square brackets, or one of the allowed operators "
    "('@', '+', '-', etc) followed by a property name." },

    { TCERR_OBJ_TPL_SYM_NOT_PROP,
    "symbol \"%~.*s\" in object template is not a property",
    "The symbol \"%~.*s\" is used in an 'object template' statement where "
    "a property is required, but the symbol is not a property.  Check "
    "for conflicting usage of the symbol (as an object or function name, "
    "for example)." },

    { TCERR_OBJ_DEF_NO_TEMPLATE,
    "object definition does not match any template",
    "This object definition appears to use template notation, but it doesn't "
    "match any defined object template.  Check your 'object template' "
    "definitions to make sure this object syntax is defined, or correct "
    "this object definition to match one of the defined templates.  If "
    "this object is not intended to use a template at all, the object's "
    "first property definition probably has a syntax error, so check "
    "the object's property list syntax." },

    { TCERR_OBJ_TPL_NO_VOCAB,
    "property \"%~.*s\" is a dictionary property - not valid in templates",
    "Property \"%~.*s\" is a dictionary property, which cannot be used "
    "in an object template." },

    { TCERR_OBJ_TPL_PROP_DUP,
    "property \"%~.*s\" duplicated in object template",
    "The property \"%~.*s\" appears more than once in this 'object template' "
    "list.  A given property can be used only once in a template." },

    { TCERR_META_ALREADY_DEF,
    "intrinsic class has been previously defined as \"%~.*s\"",
    "This same intrinsic class has been previously defined with "
    "the name \"%~.*s\".  An intrinsic class may only be defined with "
    "one class symbol.  Remove one of the conflicting definitions." },

    { TCERR_OBJ_DEF_REQ_SEM,
    "missing semicolon at end of object definition - found \"%~.*s\"",
    "This object definition is missing its closing semicolon ';' - the "
    "compiler found \"%~.*s\", which the compiler must assume is the start "
    "of a new statement or object definition.  Insert the missing "
    "semicolon.  If this is actually meant to be part of the object "
    "definition, there is a syntax error here - check and correct the "
    "syntax." },

    { TCERR_EXPECTED_STMT_START,
    "expected start of statement, but found \"%~.*s\"",
    "The compiler expected to find the beginning of a statement, "
    "but found \"%~.*s\" instead.  Check the statement syntax; check "
    "for preceding strings that weren't properly terminated, or extra "
    "or missing braces." },

    { TCERR_MISSING_COLON_FORMAL,
    "missing colon at end of anonymous function formal list - found \"%~.*s\"",
    "The short-form of the anonymous function definition requires a colon "
    "':' at the end of the formal parameter list, but the compiler "
    "found \"%~.*s\" instead.  Even if the function has no arguments, it "
    "still requires a colon immediately following the open brace '{'. " },

    { TCERR_SEM_IN_SHORT_ANON_FN,
    "semicolon is not allowed in a short anonymous function",
    "This short-form anonymous function contains a semicolon, which is "
    "not allowed.  A short-form anonymous function must consist of "
    "a single expression, and is terminated with the closing brace '}' "
    "with no semicolon between the expression and the brace." },

    { TCERR_SHORT_ANON_FN_REQ_RBRACE,
    "semicolon is not allowed in a short-form anonymous function",
    "This short-form anonymous function's expression is followed by a "
    "semicolon, which is not allowed.  A short-form anonymous function "
    "can contain only an expression, and ends with a right brace '}'." },

    { TCERR_SHORT_ANON_FN_REQ_RBRACE,
    "missing '}' at end of anonymous function expression - found \"%~.*s\"",
    "There is no right brace '}' at the end of this short-form "
    "anonymous function.  A short-form anonymous function must contain "
    "only an expression and end with '}'.  Check the syntax." },

    { TCERR_IN_REQ_LPAR,
    "missing '(' after 'in' operator - found \"%~.*s\"",
    "An open parenthesis '(' is required after the 'in' operator, but "
    "the compiler found \"%~.*s\".  Check the syntax and insert the "
    "missing parenthesis." },

    { TCERR_EXPECTED_IN_COMMA,
    "expected ',' or ')' in 'in' list, but found \"%~.*s\"",
    "Expected a comma ',' or right parenthesis ')' in the 'in' list, "
    "but found \"%~.*s\".  The expressions in an 'in' list must be "
    "separated by commas, and the entire list must be enclosed in "
    "parentheses '( )'.  Check for errors and unbalanced parentheses "
    "in the argument expression." },

    { TCERR_EXPECTED_IN_RPAR,
    "expected ')' at end of 'in' list, but found \"%~.*s\"",
    "Expected a right parenthesis ')' at the end of the 'in' list, "
    "but found \"%~.*s\".  The compiler is assuming that this is the end "
    "of the statement.  Please insert the missing parenthesis, or check "
    "the argument list for unbalanced parentheses or other errors." },

    { TCERR_CANNOT_MOD_OR_REP_TYPE,
    "objects of this type cannot be modified or replaced",
    "You cannot use 'modify' or 'replace' with an object of this type.  "
    "Only objects and classes originally defined as ordinary objects "
    "can be modified or replaced." },

    { TCERR_REQ_FOREACH_LPAR,
    "expected '(' after \"foreach\", but found \"%~.*s\"",
    "An open parenthesis '(' is required after the \"foreach\" keyword, "
    "but the compiler found \"%~.*s\" instead.  Check the syntax "
    "and insert the missing parenthesis." },

    { TCERR_MISSING_FOREACH_EXPR,
    "missing expression in \"foreach\" - found \"%~.*s\"",
    "This \"foreach\" statement is missing its iteration expression; "
    "the compiler found \"%~.*s\" when it was expecting the iteration "
    "expression of the form 'x in <collectionExpression>'.  Check the "
    "syntax." },

    { TCERR_FOREACH_REQ_IN,
    "\"in\" required in \"foreach\" - found \"%~.*s\"",
    "The keyword \"in\" was expected after the iteration variable "
    "expression in the this \"foreach\" statement, but the compiler "
    "found \"%~.*s\" instead.  Check the syntax." },

    { TCERR_REQ_FOREACH_RPAR,
    "missing ')' at end of \"foreach\" expression - found \"%~.*s\"",
    "A closing parenthesis ')' is required at the end of the \"foreach\" "
    "statement's expression, but the compiler found \"%~.*s\" instead.  "
    "Check the syntax, and insert the missing parenthesis, or correct "
    "unbalanced parentheses or other syntax errors earlier in the "
    "expression." },

    { TCERR_PROPDECL_REQ_SYM,
    "expected property name in 'property' list, but found \"%~.*s\"",
    "The name of a property was expected in the 'property' "
    "list, but the compiler found \"%~.*s\" instead.  Check the "
    "statement syntax." },

    { TCERR_PROPDECL_REQ_COMMA,
    "expected comma in 'property' list, but found \"%~.*s\"",
    "A comma was expected after a property name symbol in a "
    "'property' list, but the compiler found \"%~.*s\" "
    "instead.  Check the syntax of the list and ensure that "
    "each item is separated from the next by a comma, and that the "
    "list ends with a semicolon." },

    { TCERR_EXPORT_REQ_SYM,
    "expected symbol in 'export' statement, but found \"%~.*s\"",
    "A symbol name must follow the 'export' keyword, but the compiler "
    "found \"%~.*s\" instead.  Check the syntax of the statement." },

    { TCERR_EXPORT_EXT_TOO_LONG,
    "external name \"%~.*s\" in 'export' is too long",
    "The external name \"%~.*s\" in this 'export' statement is too long.  "
    "External names are limited to the same maximum length as regular "
    "symbol names." },

    { TCERR_UNTERM_OBJ_DEF,
    "unterminated object definition",
    "This object definition is not properly terminated - a semicolon ';' or "
    "closing brace '}' should appear at the end of the definition.  The "
    "compiler found a new object definition or a statement that cannot "
    "be part of an object definition, but did not find the end of the "
    "current object definition.  Insert the appropriate terminator, or "
    "check the syntax of the property definition.  Note that this error "
    "is normally reported at the END of the unterminated object, so if "
    "the line number of the error refers to the start of a new object, "
    "it's probably the preceding object that is not property terminated." },

    { TCERR_OBJ_DEF_REQ_RBRACE,
    "missing right brace at end of object definition - found \"%~.*s\"",
    "This object definition is missing its closing brace '}' - the "
    "compiler found \"%~.*s\", which the compiler must assume is the start "
    "of a new statement or object definition.  Insert the missing "
    "brace.  If this is actually meant to be part of the object "
    "definition, there is a syntax error here - check and correct the "
    "syntax." },

    { TCERR_GRAMMAR_REQ_NAME_RPAR,
    "missing right parenthesis after name in 'grammar' - found \"%~.*s\"",
    "A right parenthsis ')' is required after the name tag in a 'grammar' "
    "statement, but the compiler found \"%~.*s\".  Check the syntax and "
    "insert the missing parenthesis." },

    { TCERR_PROPSET_REQ_LBRACE,
    "missing open brace '{' in propertyset definition - found \"%~.*s\"",
    "An open brace '{' is required to group the properties in the "
    "propertyset definition.  The compiler found \"%~.*s\" where the "
    "open brace should be.  Check the syntax and remove any extraneous "
    "characters or insert the missing open brace, as appropriate." },

    { TCERR_GRAMMAR_REQ_RPAR_AFTER_GROUP,
    "missing right parentheses after group in 'grammar' - found \"%~.*s\"",
    "A right parenthesis ')' is required at the end of a parenthesized "
    "token group in a 'grammar' statement, but the compiler found "
    "\"%~.*s\".  Check the syntax and insert the missing parenthesis." },

    { TCERR_GRAMMAR_GROUP_ARROW_NOT_ALLOWED,
    "'->' is not allowed after parenthesized group in 'grammar' statement",
    "The arrow operator '->' is not allowed after a parenthesized group "
    "in a 'grammar' statement.  A group is not a true sub-production, so "
    "it has no match object to assign to a property.  If you want to "
    "create a run-time match object for the group, make the group into "
    "a rule for a separate, named production, and use the name of the "
    "production instead of the group in this rule." },

    { TCERR_OBJ_DEF_CANNOT_USE_TEMPLATE,
    "cannot use template with an unimported extern class as superclass",
    "This object cannot be defined with template notation (property value "
    "constants immediately after the superclass name or names) because "
    "one or more of its superclasses are unimported 'extern' classes, or "
    "inherit from unimported extern classes.  The compiler does not have "
    "any class relationship information about classes that are explicitly "
    "defined as external ('extern') and not imported from a symbol file "
    "that is part of the current build, so it cannot determine which "
    "class's template definition to use.  You must define this object's "
    "properties using the normal 'name = value' notation rather than "
    "using a template." },

    { TCERR_REPLACE_OBJ_REQ_SC,
    "base class list required for object 'replace' definition",
    "This 'replace' statement replaces an object with a new definition, "
    "but it does not have a superclass list.  A superclass list is "
    "required for the replacement object definition." },

    { TCERR_PROPSET_TOO_DEEP,
    "'propertyset' nesting is too deep",
    "This 'propertyset' definition is too deeply nested - it's inside "
    "too many other 'propertyset' definitions.  You must reduce "
    "the nesting depth." },

    { TCERR_PROPSET_TOK_TOO_LONG,
    "expanded property name in propertyset is too long",
    "This property name, expanded into its full name using the "
    "propertyset pattern string, is too long.  You must shorten the "
    "fully expanded name by shortening the propertyset pattern, "
    "shortening this property's name, or reducing the propertyset "
    "nesting depth." },

    { TCERR_PROPSET_INVAL_PAT,
    "propertyset pattern string \"%~.*s\" is invalid",
    "The propertyset pattern string \"%~.*s\" is not valid.  A propertyset "
    "pattern string must consist of valid symbol characters plus "
    "exactly one asterisk '*' (which specifies where the property "
    "names within the propertyset are inserted into the pattern)." },

    { TCERR_PROPSET_INVAL_FORMALS,
    "invalid formal parameters in propertyset definition",
    "The propertyset formal parameter list is invalid.  The parameters "
    "must contain exactly one asterisk '*', specifying the argument "
    "position where additional per-property parameters are inserted "
    "into the common parameter list." },

    { TCERR_CIRCULAR_CLASS_DEF,
    "circular class definition: %~.*s is a subclass of %~.*s",
    "This class is defined circularly: %~.*s is a subclass of %~.*s, "
    "so the latter cannot also be a subclass of the former.  Check the "
    "definition of the base class. " },

    { TCERR_GRAMMAR_LIST_REQ_PROP,
    "property name required in list in 'grammar', but found \"%~.*s\"",
    "A part-of-speech list within a 'grammar' rule definition is only "
    "allowed to contain property names, but the compiler found \"%~.*s\" "
    "instead.  Check the syntax. " },

    { TCERR_GRAMMAR_LIST_UNCLOSED,
    "missing '>' in 'grammar' property list - found \"%~.*s\"",
    "The grammar property list is missing the closing angle bracket '>'; "
    "the compiler found \"%~.*s\" in the list and will assume that the '>' "
    "was accidentally omitted.  Insert the missing '>' or remove the "
    "extraneous text from the list. " },

    { TCERR_CONST_IDX_INV_TYPE,
    "invalid indexed type in constant expression - list required",
    "Only a list value can be indexed in a constant expression.  The "
    "constant value being indexed in this expression is not a list. " },

    { TCERR_INVAL_TRANSIENT,
    "'transient' is not allowed here",
    "The 'transient' keyword is not allowed in this context.  'transient' "
    "can only be used to modify an object definition; it cannot be used "
    "with classes, functions, templates, or any other definitions. " },

    { TCERR_GRAMMAR_MOD_REQ_TAG,
    "'modify/replace grammar' requires name-tag",
    "'modify grammar' and 'replace grammar' can only be used with a grammar "
    "rule that includes a name-tag (in parentheses after the production "
    "name).  The name-tag must match a grammar rule defined previously. " },

    { TCERR_REQ_INTRINS_SUPERCLASS_NAME,
    "expected intrinsic superclass name after colon, but found \"%~.*s\"",
    "The name of the intrinsic class's superclass was expected after "
    "the colon, but the compiler found \"%~.*s\" instead.  Add the "
    "superclass name (or remove the colon). " },

    { TCERR_INTRINS_SUPERCLASS_UNDEF,
    "intrinsic class superclass \"%~.*s\" is not defined",
    "The intrinsic class superclass \"%~.*s\" is not defined.  You must "
    "define the superclass before you define any subclasses. " },

    { TCERR_GRAMMAR_ENDS_WITH_OR,
    "grammar rule ends with '|' (add '()' if empty last rule is "
        "intentional)",
    "The grammar rule ends with '|'.  This indicates that the rule matches "
    "a completely empty input.  This is legal, but it's uncommon.  "
    "If you really intend for this rule to match empty input, "
    "you can eliminate this warning by making the empty rule "
    "explicit, by using '()' for the empty match rule. " },

    { TCERR_GRAMMAR_EMPTY,
    "grammar rule is empty (add '()' if this is intentional)",
    "The grammar rule is empty, which indicates that the rule matches "
    "a completely empty input.  This is legal, but it's uncommon.  "
    "If you really intend for this rule to match empty input, "
    "you can eliminate this warning by making the empty rule "
    "explicit, by using '()' for the empty match rule. " },

    { TCERR_TEMPLATE_EMPTY,
    "empty template definition",
    "The template is empty.  A template definition must include at "
    "least one property name." },

    { TCERR_REQ_RPAR_IF,
    "expected ')' after the \"if\" condition, but found \"%~.*s\"",
    "A close parenthesis ')' is required after the condition expression "
    "in the \"if\" statement, to balance the required open parenthesis, "
    "but the compiler found \"%~.*s\" instead.  The compiler will assume "
    "that a parenthesis was intended.  Please correct the syntax "
    "by inserting a parenthesis." },

    { TCERR_INTRINS_SUPERCLASS_NOT_INTRINS,
    "intrinsic class superclass \"%~.*s\" is not an intrinsic class",
    "The intrinsic class superclass \"%~.*s\" is not itself an intrinsic "
    "class.  An intrinsic class can only be derived from another intrinsic "
    "class. " },

    { TCERR_FUNC_REDEF_AS_MULTIMETHOD,
    "function \"%.*s\" is already defined - can't redefine as a multi-method",
    "The function \"%.*s\" is already defined as an ordinary function, "
    "so it can't be redefined here with typed parameters. If you intended "
    "for the original version to be a multi-method, add the 'multimethod' "
    "modifier to its definition, immediately after the closing parenthesis "
    "of the parameter list in the function definition, as in "
    "\"foo(x) multimethod { ... }\"." },

    { TCERR_MULTIMETHOD_NOT_ALLOWED,
    "'multimethod' is not allowed here",
    "The 'multimethod' modifier is not allowed in this context. "
    "This modifier can only be used for top-level function definitions." },

    { TCERR_MMPARAM_NOT_OBJECT,
    "parameter type \"%~.*s\" is not an object",
    "The declared parameter type \"%~.*s\" is not an object. Only object "
    "or class names can be used to declare parameter types. Check the "
    "definition of the function and make sure the type name is correct, "
    "and that the same name isn't used elsewhere as a non-object type." },

    { TCERR_MMINH_MISSING_ARG_TYPE,
    "expected a type name in inherited<> type list, but found \"%~.*s\"",
    "An inherited<> type list requires a type name (a class or object "
    "name, or '*' or '...') in each position. The compiler found "
    "\"%~.*s\" where it expected to find a type. The type must be "
    "written as a simple name, with no parentheses or other punctuation." },

    { TCERR_MMINH_MISSING_COMMA,
    "expected a comma in inherited<> type list, but found \"%~.*s\"",
    "The compiler found \"%~.*s\" where it expected to find a comma "
    "in an inherited<> type list. Each element in the list must be "
    "separated by a comma." },

    { TCERR_MMINH_MISSING_GT,
    "missing '>' in inherited<> type list (found \"%~.*s\")",
    "An inherited<> type list must end with a '>'.  The compiler "
    "found \"%~.*s\", and is assuming that the list ends here.  If "
    "this isn't the end of the type list, check the syntax; otherwise, "
    "just add '>' at the end of the list." },

    { TCERR_MMINH_MISSING_ARG_LIST,
    "argument list missing in inherited<> (expected '(', found \"%~.*s\")",
    "An inherited<> expression requires an argument list.  The compiler "
    "found \"%~.*s\" it expected the opening '(' of the argument list.  "
    "If the function doesn't take any arguments, use empty parentheses, "
    "'()', to indicate the empty argument list." },

    { TCERR_NAMED_ARG_NO_ELLIPSIS,
    "a pass-by-name argument value cannot use an ellipsis '...'",
    "A named argument value (\"name: expression\") cannot be combined "
    "with an ellipsis '...'.  The ellipsis expands a list value into "
    "a series of positional arguments.  This isn't meaningful with a "
    "named argument, because a named argument is passed by name rather "
    "than by position." },

    { TCERR_ARG_MUST_BE_OPT,
    "parameter '%.*s' must be optional because it follows an "
    "optional parameter",
    "The parameter variable '%.*s' in this function or method definition "
    "must be declared as optional, because it follows one or more other "
    "optional arguments in the list. Argument values are assigned to parameter "
    "names in left-to-right order at run-time, so once you have one optional "
    "parameter to the list, all of the parameters that follow it must be "
    "optional as well. You can make this parameter optional by adding "
    "a '?' suffix after the variable name, or by adding an '= expression' "
    "default value expression after the name." },

    { TCERR_NAMED_ARG_NO_TYPE,
    "formal parameter '%.*s' is named - a type declaration isn't allowed",
    "The formal parameter '%.*s' is named (it uses a ':' suffix), so it "
    "can't have a type declaration.  Multimethod selection is based "
    "strictly on the positional arguments; named parameters aren't "
    "used in the selection process, so they can't be declared with types." },

    { TCERR_LOOKUP_LIST_EXPECTED_ARROW,
    "missing -> in %dth element in LookupTable list",
    "An arrow '->' was expected in the %dth element of the LookupTable "
    "list.  This list appears to be a LookupTable list, because earlier "
    "elements use the Key->Value notation, so every element must given "
    "as a Key->Value pair." },

    { TCERR_ARROW_IN_LIST,
    "unexpected -> found in list (after %dth element)",
    "An arrow '->' was found in a list expression, after the %dth element. "
    "The arrow isn't allowed here because the expression appears to be an "
    "ordinary list, not a LookupTable list.  If you intended this to be "
    "a LookupTable list, every element must be given as a Key->Value pair." },

    { TCERR_MISPLACED_ARROW_IN_LIST,
    "misplaced -> in LookupTable list after %dth element - expected comma",
    "A comma ',' should appear after the %dth element of the LookupTable "
    "list, but an arrow '->' was found instead.  The arrow appears to be "
    "misplaced. Each Key->Value pair in a LookupTable list should be "
    "separated from the next by a comma." },

    { TCERR_LOOKUP_LIST_EXPECTED_END_AT_DEF,
    "LookupTable list must end after default *-> value, but found \"%~.*s\"",
    "A LookupTable list must end after the default value (specified with "
    "\"*->Value\"), but the compiler found \"%~.*s\" where the closing "
    "bracket ']' should be. Rearrange the list so that the default "
    "value is in the final position in the list." },

    { TCERR_OPER_IN_PROPSET,
    "an 'operator' property cannot be defined within a 'propertyset'",
    "An 'operator' property cannot be defined within a 'propertyset' "
    "group. You must move the 'operator' property definition outside "
    "of the group." },

    { TCERR_EXPECTED_RBRACK_IN_OP,
    "expected ']' in 'operator []', found '%~.*s'",
    "The ']' in an 'operator []' property declaration was missing (the "
    "compiler found '%~.*s' where the ']' should go)." },

    { TCERR_BAD_OP_OVERLOAD,
    "invalid overloaded operator '%~.*s'",
    "The 'operator' property type requires one of the overloadable "
    "operators, but the compiler found '%~.*s' where the operator "
    "should appear.  You must specify one of the overloadable operator "
    "symbols:  + - * / % ^ << >> ~ | & [] []=" },

    { TCERR_OP_OVERLOAD_WRONG_FORMALS,
    "wrong number of parameters defined for overloaded operator (%d required)",
    "This 'operator' property has the wrong number of parameters. "
    "This overloaded operator requires %d parameter(s)." },

    { TCERR_RT_CANNOT_DEFINE_GLOBALS,
    "new global symbols (\"%~.*s\") cannot be defined in "
    "run-time code compilation",
    "Code compiled during program execution cannot define new global "
    "symbols.  The code being compiled attempted to define \"%~.*s\"." },

    { TCERR_FOR_IN_NOT_LOCAL,
    "'for..in' variable %.*s is not defined as a local variable",
    "The variable \"%.*s\" is being used as the control variable "
    "of a 'for..in' statement, but this variable isn't defined as a "
    "local variable. Check that the variable name is correct. If so, "
    "you must add a 'local' statement defining this variable (or simply "
    "add the word 'local' before the variable name in the 'in' clause)." },

    { TCERR_BAD_EMBED_END_TOK,
    "<<%.*s>> found in string without a matching %s",
    "The compiler found the embedded keyword <<%.*s>> in this string, "
    "but this wasn't preceded by a matching %s. Check that all "
    "keywords are present and spelled correctly. If you're using nested "
    "<<if>>'s or the like, the problem might be that the begin/end "
    "aren't balanced properly for one of the nested levels." },

    { TCERR_STRTPL_MISSING_LANGLE,
    "expected '<<' at start of string template, but found '%.*s'",
    "The compiler expected to find '<<' at the start of the string "
    "template definition (after the word 'template'), but found '%.*s'. "
    "Check the syntax, and insert the missing '<<'." },

    { TCERR_STRTPL_MISSING_RANGLE,
    "expected '>>' at end of string template, but found '%.*s'",
    "The compiler expected to find '>>' at the end of the string "
    "template definition, but found '%.*s'. This symbol isn't allowed "
    "within a string template, so the compiler is guessing that this "
    "is really the end of the statement and that the template is missing "
    "its closing '>>'. Check the syntax. If you really meant to include "
    "this symbol in the template, you must change it to something else, "
    "since it's not an allowed template element. If you simply left off "
    "the closing '>>', add it at the end of the definition." },

    { TCERR_STRTPL_MULTI_STAR,
    "only one '*' is allowed in a string template",
    "This string template definition has more than one '*' symbol. "
    "Only one '*' is allowed in a string template." },

    { TCERR_STRTPL_FUNC_MISMATCH,
    "string template processor function %.*s() does not match template - "
    "must take %d parameter(s) and return a value",
    "The processor function %.*s() for this string template does not "
    "match the template usage. It must be defined as a function taking "
    "%d argument(s) and returning a value." },

    { TCERR_DEFINED_SYNTAX,
    "invalid syntax for defined() operator - argument must be a symbol name",
    "The syntax for the defined() operator is invalid. This operator "
    "requires a single symbol name as the argument, in parentheses." },

    { TCERR___OBJREF_SYNTAX,
    "invalid syntax for __objref() operator",
    "The syntax for the __objref() operator is invalid. This operator "
    "requires syntax of the form __objref(symbol) or __objref(symbol, mode), "
    "where the mode is 'warn' or 'error'." },

    { TCERR_BAD_OP_FOR_FLOAT,
    "floating point values can't be used with this operator",
    "Floating point values can't be used with this operator. Only ordinary "
    "integer values can be used." },

    { TCERR_INLINE_OBJ_REQ_LBRACE,
    "expected inline object property list starting with '{' (found '%.*s')",
    "An inline property definition must contain a property list enclosed "
    "in braces, '{ ... }'. The compiler found '%.*s' where it expected "
    "to see the left brace '{' at the start of the property list. Check "
    "the object definition syntax." },

    { TCERR_CODEGEN_NO_MEM,
    "out of memory for code generation",
    "Out of memory.  The compiler cannot allocate memory to generate "
    "the compiled code for the program.  Make more memory available, "
    "if possible (by closing other applications, for example), then "
    "try running the compiler again." },

    { TCERR_UNRES_TMP_FIXUP,
    "%d unresolved branches",
    "%d unresolved branches." },

    { TCERR_WRITEAT_PAST_END,
    "attempting to write past end of code stream",
    "The compiler's code generator is attempting to "
    "write past the end of the code stream." },

    { TCERR_SELF_NOT_AVAIL,
    "'self' is not valid in this context",
    "The 'self' object is not available in this context.  You can only "
    "refer to 'self' within a method associated with an object; you cannot "
    "use 'self' in a function or in other code that is not part of an "
    "object's method." },

    { TCERR_INH_NOT_AVAIL,
    "'inherited' is not valid in this context",
    "The 'inherited' construct is not available in this context.  You "
    "can only use 'inherited' within a method associated with an object "
    "with a superclass." },

    { TCERR_NO_ADDR_SYM,
    "cannot take address of \"%~.*s\"",
    "You cannot take the address of \"%~.*s\".  You can only take the "
    "address of a function or a property." },

    { TCERR_CANNOT_ASSIGN_SYM,
    "cannot assign to \"%~.*s\"",
    "You cannot assign a value to \"%~.*s\".  You can only assign a value "
    "to an object property, to a local variable or parameter, or to a "
    "subscripted element of a list." },

    { TCERR_CANNOT_EVAL_LABEL,
    "\"%~.*s\" is a label and cannot be evaluated in an expression",
    "The symbol \"%~.*s\" is a code label; this symbol cannot be "
    "evaluated in an expression.  You can only use this symbol "
    "in a 'goto' statement." },

    { TCERR_CANNOT_CALL_SYM,
    "cannot call \"%~.*s\" as a method or function",
    "The symbol \"%~.*s\" is not a valid method, function, or expression "
    "value.  You cannot invoke this symbol as a method or function call.  "
    "Check spelling, and check for missing operators or mismatched "
    "parentheses." },

    { TCERR_PROP_NEEDS_OBJ,
    "cannot call property \"%~.*s\" without \"object.\" prefix - no \"self\"",
    "You cannot call the property \"%~.*s\" in this context without an "
    "explicit \"object.\" prefix, because no implied \"self\" object "
    "exists in this context.  A \"self\" object exists only inside object "
    "method code.  Check spelling and syntax to ensure you meant to call "
    "a property, and add the object qualifier if so." },

    { TCERR_VOID_RETURN_IN_EXPR,
    "\"%~.*s\" has no return value - cannot use in an expression",
    "\"%~.*s\" has no return value, so it cannot be used in an expression "
    "(there's no value to use in further calculations).  Check the "
    "definition of the function or method." },

    { TCERR_INVAL_FUNC_ADDR,
    "'&' cannot be used with a function name (\"%~.*s\")",
    "The '&' operator cannot be used with function name \"%~.*s\".  If you "
    "want a reference to the function, simply use the name by itself with "
    "no argument list.  If you want to call the function, put the argument "
    "list in parentheses '( )' after the function name; if the function "
    "takes no arguments, use empty parentheses '()' after the function's "
    "name." },

    { TCERR_INVAL_NEW_EXPR,
    "cannot apply operator 'new' to this expression",
    "You cannot apply operator 'new' to this expression.  'new' can "
    "only be applied to an object or metaclass name, followed by an "
    "optional argument list.  Check for syntax errors." },

    { TCERR_TOO_FEW_FUNC_ARGS,
    "too few arguments to function \"%~.*s\"",
    "Not enough arguments are specified in the call to the "
    "function \"%~.*s\".  Check the definition of the function, "
    "and check for unbalanced parentheses or other syntax errors." },

    { TCERR_TOO_MANY_FUNC_ARGS,
    "too many arguments to function \"%~.*s\"",
    "Too many arguments are specified in the call to the "
    "function \"%~.*s\".  Check the definition of the function, "
    "and check for unbalanced parentheses or other syntax errors." },

    { TCERR_SETPROP_NEEDS_OBJ,
    "cannot assign to property \"%~.*s\" without \"object.\" prefix - "
        "no \"self\"",
    "You cannot assign to the property \"%~.*s\" in this context without an "
    "explicit \"object.\" prefix, because no implied \"self\" object "
    "exists in this context.  A \"self\" object exists only inside object "
    "method code.  Check spelling and syntax to ensure you meant to assign "
    "to a property, and add the object qualifier if so." },

    { TCERR_SYM_NOT_PROP,
    "invalid '.' expression - symbol \"%~.*s\" is not a property",
    "The symbol \"%~.*s\" cannot be used after a period '.' because this "
    "symbol is not a property.  The period in a member evaluation "
    "expression must be followed by a property name, or by an expression "
    "that yields a property pointer value.  Check spelling and check"
    "for syntax errors." },

    { TCERR_INVAL_PROP_EXPR,
    "invalid '.' expression - right side is not a property",
    "This property evaluation expression is invalid.  The period '.' "
    "in a property expression must be followed by a property name, "
    "or by an expression that yields a property pointer value.  Check "
    "spelling and check for syntax errors." },

    { TCERR_INH_NOT_OBJ,
    "illegal 'inherited' - \"%~.*s\" is not a class",
    "This 'inherited' expression is not valid, because \"%~.*s\" is not "
    "a class.  'inherited' must be followed by a period '.' or by the "
    "name of a superclass of this method's object.  Check spelling and "
    "expression syntax." },

    { TCERR_INVAL_OBJ_EXPR,
    "illegal expression - left of '.' is not an object",
    "The expression on the left of the period '.' is not an object value.  "
    "The left of a '.' expression must be an object name, or an expression "
    "that evaluates to an object value.  Check spelling and syntax." },
    
    { TCERR_SYM_NOT_OBJ,
    "symbol \"%~.*s\" is not an object - illegal on left of '.'",
    "The symbol \"%~.*s\" is not an object, so you cannot use this "
    "symbol on the left side of a period '.' expression.  The left side of a "
    "'.' expression must be an object name or an expression that evaluates "
    "to an object value.  Check the spelling and syntax." },

    { TCERR_IF_ALWAYS_TRUE,
    "\"if\" condition is always true - \"else\" part is unreachable",
    "The condition in this \"if\" statement is always true, which means "
    "that the code in the \"else\" clause will never be executed.  If this "
    "is unintentional, check the condition to ensure it's correct." },

    { TCERR_IF_ALWAYS_FALSE,
    "\"if\" condition is always false",
    "The condition in this \"if\" statement is always false, which means "
    "that the statement or statements following the \"if\" will never "
    "be executed.  If this is unintentional, check the condition to "
    "ensure it's correct." },

    { TCERR_INVALID_BREAK,
    "invalid \"break\" - not in a for/while/do/switch",
    "This \"break\" statement is invalid.  A \"break\" can only appear "
    "in the body of a loop (\"for\", \"while\", or \"do\") or within a "
    "case in a \"switch\" statement." },

    { TCERR_INVALID_CONTINUE,
    "invalid \"continue\" - not in a for/while/do",
    "This \"continue\" statement is invalid.  A \"continue\" can only appear "
    "in the body of a loop (\"for\", \"while\", or \"do\")." },

    { TCERR_MAIN_NOT_DEFINED,
    "function _main() is not defined",
    "No function named _main() is defined.  This function must be defined, "
    "because _main() is the entrypoint to the program at run-time." },

    { TCERR_MAIN_NOT_FUNC,
    "_main is not a function",
    "The global symbol \"_main\" does not refer to a function.  This symbol "
    "must be defined as a function, because _main() is the entrypoint "
    "to the program at run-time." },

    { TCERR_CATCH_EXC_NOT_OBJ,
    "exception class symbol \"%~.*s\" in \"catch\" clause is not an object",
    "The exception class symbol \"%~.*s\" used in this \"catch\" clause "
    "is not an object class.  Only an object class can be used to "
    "specify the type of exception to catch." },

    { TCERR_INVALID_BREAK_LBL,
    "invalid \"break\" - no label \"%~.*s\" on any enclosing statement",
    "This \"break\" is invalid because the label \"%~.*s\" does not "
    "refer to an enclosing statement.  When a label is used with "
    "\"break\", the label must refer to an enclosing statement.  "
    "Check for unbalanced braces, and check that the label is correct." },

    { TCERR_INVALID_CONT_LBL,
    "invalid \"continue\" - no label \"%~.*s\" on any enclosing loop",
    "This \"continue\" is invalid because the label \"%~.*s\" does not "
    "refer to an enclosing statement.  When a label is used with "
    "\"continue\", the label must refer directly to an enclosing loop "
    "(\"while\", \"for\", or \"do\") statement.  Check for unbalanced "
    "braces, and check that the label is correct." },

    { TCERR_INVALID_GOTO_LBL,
    "invalid \"goto\" - label \"%~.*s\" is not defined in this function "
        "or method",
    "This \"goto\" is invalid because the label \"%~.*s\" is not defined "
    "in this function or method.  Check the label name and correct "
    "any misspelling, or insert the missing label definition." },

    { TCERR_UNRESOLVED_EXTERN,
    "unresolved external reference \"%~.*s\"",
    "The symbol \"%~.*s\" was defined as an external reference but "
    "is not defined anywhere in the program.  Determine where this "
    "symbol should be defined, and make sure that the source file "
    "that contains the definition is included in the compilation." },

    { TCERR_UNREFERENCED_LABEL,
    "label \"%~.*s\" is not referenced",
    "The statement label \"%~.*s\" is not referenced in any \"goto\", "
    "\"break\", or \"continue\" statement.  The label is unnecessary "
    "and can be removed." },

    { TCERR_UNREFERENCED_LOCAL,
    "local variable \"%~.*s\" is never used",
    "The local variable \"%~.*s\" is never used.  This local variable "
    "can be removed.  Check to make sure that the local variable isn't "
    "hidden by another variable of the same name inside nested braces '{ }' "
    "within the code." },
        
    { TCERR_UNASSIGNED_LOCAL,
    "no value is assigned to local variable \"%~.*s\"",
    "No value is assigned to the local variable \"%~.*s\".  "
    "You should initialize the variable with a value before its first "
    "use to make the meaning of the code clear." },

    { TCERR_UNUSED_LOCAL_ASSIGNMENT,
    "value is assigned to local variable \"%~.*s\" but the value is "
        "never used",
    "A value is assigned to the local variable \"%~.*s\", but the variable's "
    "value is never used.  It is possible that the variable can be "
    "removed." },

    { TCERR_SC_NOT_OBJECT,
    "invalid superclass \"%~.*s\" - not an object",
    "The superclass \"%~.*s\" is not valid, because the symbol does not "
    "refer to an object.  Each superclass of an object must be a class "
    "or object." },

    { TCERR_ARG_EXPR_HAS_NO_VAL,
    "argument expression %d yields no value",
    "The argument expression in position %d in the argument list yields no "
    "value.  This usually indicates that the argument expression uses "
    "a double-quoted (self-printing) string where a single-quoted string "
    "was intended." },

    { TCERR_ASI_EXPR_HAS_NO_VAL,
    "right side of assignment has no value",
    "The expression on the right side of the assignment operator yields "
    "no value.  This usually indicates that the expression after the "
    "assignment operator uses a double-quoted (self-printing) string "
    "where a single-quoted string was intended." },

    { TCERR_WRONG_ARGC_FOR_FUNC,
    "wrong number of arguments to function \"%~.*s\": %s required, %d actual",
    "The call to function \"%~.*s\" has the wrong number of arguments.  "
    "The function requires %s argument(s), but this call has %d.  Check "
    "the argument list." },

    { TCERR_GOTO_INTO_FINALLY,
    "'goto' cannot transfer control into a 'finally' block",
    "This 'goto' statement transfers control to a label defined in a "
    "'finally' block, but the 'goto' statement is not within the same "
    "'finally' block.  This type of transfer is illegal because it would "
    "leave the exit path from the 'finally' undefined." },

    { TCERR_UNREFERENCED_PARAM,
    "formal parameter \"%~.*s\" is never used",
    "The formal parameter \"%~.*s\" is never used.  In many cases this "
    "doesn't indicate a problem, since a method might receive parameters "
    "it doesn't actually need - the method interface might be inherited "
    "from a base class, or the extra parameters might be included "
    "for possible future use or for architectural reasons.  You might "
    "want to check that the parameter really wasn't meant to be used; in "
    "particular, check to make sure that the parameter isn't hidden "
    "by another variable of the same name inside nested braces '{ }' "
    "within the code." },

    { TCERR_GRAMPROD_HAS_NO_ALTS,
    "grammar production \"%~.*s\" has no alternatives defined",
    "The grammar production symbol \"%~.*s\" has no alternatives defined.  "
    "This probably indicates that this symbol was accidentally "
    "misspelled when used as a sub-production in a grammar rule.  Check "
    "for occurrences of this symbol in grammar rules, and correct the "
    "spelling if necessary.  If the symbol is spelled correctly, add "
    "at least one grammar rule for this production." },

    { TCERR_CANNOT_MOD_META_PROP,
    "intrinsic class property \"%~.*s\" cannot be modified",
    "The property \"%~.*s\" is defined in this intrinsic class, "
    "so it cannot be modified.  You can only add new properties to "
    "an intrinsic class; you can never override, modify, or replace "
    "properties defined in the intrinsic class itself." },

    { TCERR_FOREACH_NO_CREATEITER,
    "Container.createIterator is not defined - %s..in is not valid",
    "The intrinsic class Container's method createIterator is not defined, "
    "so the %s..in syntax cannot be used.  This should be corrected if you "
    "#include <tads.h> or <systype.h> in this source file." },

    { TCERR_FOREACH_NO_GETNEXT,
    "Iterator.getNext is not defined - %s..in cannot be used",
    "The intrinsic class Iterator's method getNext is not defined, "
    "so the '%s..in' statement cannot be used.  This should be corrected if "
    "you #include <tads.h> or <systype.h> in this source file." },

    { TCERR_FOREACH_NO_ISNEXTAVAIL,
    "Iterator.isNextAvailable is not defined - %s..in cannot be used",
    "The intrinsic class Iterator's method isNextAvailable is not defined, "
    "so the '%s..in' statement cannot be used.  This should be corrected if "
    "you #include <tads.h> or <systype.h> in this source file." },

    { TCERR_INVALID_TYPE_FOR_EXPORT,
    "symbol \"%~.*s\" is not a valid type for export",
    "The symbol \"%~.*s\" appears in an 'export' statement, but this "
    "symbol is not of a valid type for export.  Only object and property "
    "names may be exported." },

    { TCERR_DUP_EXPORT,
    "duplicate export for external name \"%~.*s\": \"%~.*s\" and \"%~.*s\"",
    "Invalid duplicate 'export' statements.  The external name \"%~.*s\" "
    "has been given to the exported symbols \"%~.*s\" and \"%~.*s\".  An "
    "external name can be given to only one symbol.  You must remove "
    "one of the duplicate exports." },

    { TCERR_DUP_EXPORT_AGAIN,
    "additional duplicate export for external name \"%~.*s\": \"%~.*s\"",
    "Additional duplicate export: the external name \"%~.*s\" has also "
    "been exported with symbol \"%~.*s\".  Only one export per external "
    "name is allowed." },

    { TCERR_TARGETPROP_NOT_AVAIL,
    "'targetprop' is not available in this context",
    "The 'targetprop' value is not available in this context.  You can only "
    "refer to 'targetprop' within a method associated with an object; "
    "you cannot use 'targetprop' in a function or in other code that is "
    "not part of an object's method." },

    { TCERR_TARGETOBJ_NOT_AVAIL,
    "'targetobj' is not available in this context",
    "The 'targetobj' value is not available in this context.  You can only "
    "refer to 'targetobj' within a method associated with an object; "
    "you cannot use 'targetobj' in a function or in other code that is "
    "not part of an object's method." },

    { TCERR_DEFININGOBJ_NOT_AVAIL,
    "'definingobj' is not available in this context",
    "The 'definingobj' value is not available in this context.  You can only "
    "refer to 'definingobj' within a method associated with an object; "
    "you cannot use 'definingobj' in a function or in other code that is "
    "not part of an object's method." },

    { TCERR_CANNOT_CALL_CONST,
    "this constant expression cannot be called as a function",
    "This constant expression cannot be called as a function.  Check for "
    "a missing operator before an open parenthesis, since this can make "
    "an expression look like a function call. " },

    { TCERR_REPLACED_NOT_AVAIL,
    "'replaced' is not available in this context",
    "The 'replaced' keyword is not available in this context.  You can only "
    "use 'replaced' within a function you are defining with 'modify'. " },

    { TCERR_GRAMPROD_TOO_MANY_ALTS,
    "grammar production \"%~.*s\" has too many (>65,535) rules",
    "The grammar production symbol \"%~.*s\" has too many rules - a "
    "single production is limited to 65,535 rules, taking into "
    "account all 'grammar' statements for the production.  This probably "
    "indicate that the production has one or more grammar rules with "
    "deeply nested '|' alternatives.  Check for very complex 'grammar' "
    "statements, and replace complex '|' structures with separate "
    "sub-productions." },

    { TCERR_BAD_META_FOR_NEW,
    "invalid 'new' - cannot create an instance of this type",
    "The 'new' operator cannot be used to create an instance of this "
    "type of object. " },

    { TCERR_MMINH_TYPE_NOT_OBJ,
    "'inherited<>' type list element \"%~.*s\" is not an object name",
    "Type names in an 'inherited<>' type list must be object or "
    "class names. \"%~.*s\" is not an object name, so it can't be used "
    "as a type name here." },

    { TCERR_MMINH_BAD_CONTEXT,
    "'inherited<>' is not valid here; it can only be used in a multi-method",
    "'inherited<>' is not valid here. This syntax can only be used "
    "within a function defined as a multi-method." },

    { TCERR_MMINH_MISSING_SUPPORT_FUNC,
    "'inherited<>' requires \"%~.*s\", which is not defined "
    "or not a function",
    "'inherited<>' requires the library function \"%~.*s\" to be included "
    "included in the build. This is not defined or is not a function. "
    "Check that the library file 'multmeth.t' is included in the build." },
    
    { TCERR_MMINH_UNDEF_FUNC,
    "'inherited<>' function \"%~.*s\" undefined",
    "The function \"%~.*s\" with the specified inherited<> type list is "
    "not defined.  The inherited<> type list must exactly match the "
    "definition of the function you wish to call." },

    { TCERR_NAMED_PARAM_MISSING_FUNC,
    "named parameters require support function \"%~.*s\", which is undefined",
    "This program uses named parameters, so it depends on the built-in "
    "support function \"%~.*s\". This is not defined or is not a built-in "
    "function. Check that you're including the current version of the "
    "system header file 't3.h' in the build." },

    { TCERR_OPT_PARAM_MISSING_FUNC,
    "optional parameters require support function \"%~.*s\", which is undefined",
    "This program uses optional parameters, so it depends on the built-in "
    "support function \"%~.*s\". This is not defined or is not a built-in "
    "function. Check that you're including the current version of the "
    "system header file 't3.h' in the build.  (An optional parameter is "
    "a function or method argument declared with a '?' after the variable "
    "name, or with '=' and a default expression after the name." },

    { TCERR_ONEOF_REQ_GENCLS,
    "<<one of>> requires class OneOfIndexGen to be defined in the program",
    "<<one of>> requires your program to define the class OneOfIndexGen. "
    "This class isn't defined. Make sure that you're using the current "
    "version of the system library file _main.t." },

    { TCERR_ONEOF_REQ_GETNXT,
    "<<one of>> requires getNextIndex to be defined as a property",
    "<<one of>> requires your program to define the property getNextIndex, "
    "as part of class OneOfIndexGen. This symbol is used as a different "
    "type in the program. Make sure that you're using the current version "
    "of the sy stem library file _main.t" },

    { TCERR_EXT_METACLASS,
    "intrinsic class '%.*s' is not defined in this module",
    "The intrinsic class '%.*s' is not defined in this source module. "
    "To refer to this class within this module, you must declare it. "
    "The usual way to declare an intrinsic class is simply to #include "
    "the system header file that defines the class, near the beginning "
    "of your source file." },

    { TCERR_FUNC_CALL_NO_PROTO,
    "cannot call extern function %.*s without an argument list definition",
    "The function %.*s was declared 'extern' without an argument list "
    "definition. This function can't be called without declaring the "
    "argument list. You can declare the argument list in the 'extern' "
    "statement, but note that in most cases the problem is that the "
    "function itself is never defined within the program." },

    { TCERR_UNDEF_METACLASS,
    "\"%.*s\" is not defined or is not an intrinsic class",
    "The compiler requires \"%.*s\" to be defined as an intrinsic class; "
    "the symbol is either undefined or is defined as a different type. "
    "Check that the system header that defines this class is #included "
    "in this source file, and that there are no conflicting definitions "
    "of the class name." },

    { TCERR_SYMEXP_INV_TYPE,
    "invalid symbol type in symbol file",
    "Invalid symbol type in symbol file.  The symbol file is either "
    "corrupted or was created by an incompatible version of the "
    "compiler.  Regenerate the symbol file." },

    { TCERR_SYMEXP_INV_SIG,
    "invalid symbol file signature",
    "Invalid symbol file signature.  The symbol file is corrupted, "
    "was generated by an incompatible verison of the compiler, or is "
    "not a symbol file.  Delete this symbol file and re-generate it "
    "from its source file.  (If you do not have access to the source"
    "file, you must obtain an updated symbol file from the person or "
    "vendor who provided you with the original symbol file.)" },

    { TCERR_SYMEXP_SYM_TOO_LONG,
    "symbol name too long in symbol file",
    "A symbol name stored in the symbol file is too long.  The symbol "
    "file is either "
    "corrupted or was created by an incompatible version of the "
    "compiler.  Regenerate the symbol file." },

    { TCERR_SYMEXP_REDEF,
    "symbol \"%~.*s\" in imported symbol file is already defined; ignoring "
        "redefinition",
    "The symbol \"%~.*s\" is defined in the symbol file, but is already "
    "defined.  This usually indicates that two symbol files that you're "
    "importing both define this symbol as a function, object, or property "
    "name (the two files might define the symbol as the same type, or as "
    "different types - it doesn't matter, because the symbol can only be "
    "used once for any of these types).  You must resolve the conflict "
    "by renaming the symbol in one or more of the source files to make each "
    "name unique." },

    { TCERR_OBJFILE_INV_SIG,
    "invalid object file signature",
    "Invalid object file signature.  The object file is corrupted, "
    "was generated by an incompatible version of the compiler, or "
    "is not an object file.  Delete this object file and re-compile "
    "it from its source file.  (If you do not have access to the source "
    "file, you must obtain an updated object file from the person or "
    "vendor who provided you with the original object file." },

    { TCERR_OBJFILE_TOO_MANY_IDS,
    "too many object/property ID's in object file",
    "This object file contains too many object or property ID's.  The "
    "compiler cannot load this object file on this operating system.  "
    "This usually happens only on 16-bit computers, and indicates that "
    "the object file exceeds the architectural limit of the compiler "
    "on this machine.  You might be able to work around this by reducing "
    "the number of objects in this object file, which you can probably do "
    "by splitting the source file into multiple files and compiling each "
    "one into its own object file.  If you're running on MS-DOS on a "
    "386 or higher processor, you can use a 32-bit version of the "
    "compiler, which does not have this limit." },

    { TCERR_OBJFILE_OUT_OF_MEM,
    "out of memory loading object file",
    "Out of memory.  The compiler cannot "
    "allocate memory necessary to load the object file.  Make more "
    "memory available, if possible (by closing other applications, "
    "for example), then try running the compiler again." },

    { TCERR_OBJFILE_INV_TYPE,
    "invalid symbol type in object file",
    "Invalid symbol type in object file.  The object file is either "
    "corrupted or was created by an incompatible version of the "
    "compiler.  Re-compile the object file." },

    { TCERR_OBJFILE_REDEF_SYM_TYPE,
    "symbol \"%~.*s\" (type: %s) redefined (type: %s) in object file \"%s\"",
    "The symbol \"%~.*s\", which was originally defined of type %s, is "
    "redefined with type %s in object file \"%s\".  A global symbol can "
    "be defined only once in the entire program.  You must change one of "
    "the symbol's names in one of your source files to remove the "
    "conflict.\n\n"
    "If you recently changed the meaning of this symbol, you might "
    "simply need to do a full recompile - try building again with the -a "
    "option. " },

    { TCERR_OBJFILE_REDEF_SYM,
    "symbol \"%~.*s\" (type: %s) redefined in object file \"%s\"",
    "The symbol \"%~.*s\" (of type %s) is redefined in object file \"%s\".  "
    "A global symbol can be defined only once in the entire program.  "
    "You must change one of names in one of your source files to "
    "remove the conflict." },

    { TCERR_OBJFILE_BIF_INCOMPAT,
    "intrinsic function \"%~.*s\" has different definition in object file "
        "\"%s\"",
    "The intrinsic function \"%~.*s\" has a different definition in object "
    "file \"%s\" than in its previous definition.  The intrinsic function "
    "sets used in your program must be identical in each object file.  "
    "Check the \"intrinsic\" definition statements in each source file "
    "and ensure that they all match." },

    { TCERR_OBJFILE_FUNC_INCOMPAT,
    "function \"%~.*s\" has different definition in object file "
        "\"%s\"",
    "The function \"%~.*s\" has a different definition in object "
    "file \"%s\" than in its previous definition.  This probably indicates "
    "that one or more of your object or symbol files are out of date "
    "and must be recompiled." },

    { TCERR_OBJFILE_INT_SYM_MISSING,
    "internal symbol reference \"%~.*s\" missing in object file \"%s\"",
    "The internal symbol reference \"%~.*s\" is missing in the object "
    "file \"%s\".  This is an error in the object file and probably indicates"
    " that the file is corrupted.  Delete the object file and recompile its "
    "source file." },

    { TCERR_OBJFILE_INVAL_STREAM_ID,
    "invalid stream ID in object file \"%s\"",
    "The object file \"%s\" contains an invalid internal stream ID code.  "
    "This is an error in the object file and probably indicates that "
    "the file is corrupted.  Delete the object file and recompile its "
    "source file." },

    { TCERR_OBJFILE_INVAL_OBJ_ID,
    "invalid object ID in object file \"%s\"",
    "The object file \"%s\" contains an invalid object ID code.  "
    "This is an error in the object file and probably indicates that "
    "the file is corrupted.  Delete the object file and recompile its "
    "source file." },

    { TCERR_OBJFILE_INVAL_PROP_ID,
    "invalid property ID in object file \"%s\"",
    "The object file \"%s\" contains an invalid property ID code.  "
    "This is an error in the object file and probably indicates that "
    "the file is corrupted.  Delete the object file and recompile its "
    "source file." },

    { TCERR_OBJFILE_INV_FN_OR_META,
    "invalid function set or metaclass data in object file \"%s\"",
    "The object file \"%s\" contains invalid function set or metaclass "
    "data.  This is an error in the object file and probably indicates that "
    "the file is corrupted.  Delete the object file and recompile its "
    "source file." },

    { TCERR_OBJFILE_FNSET_CONFLICT,
    "conflicting intrinsic function set \"%s\" found in object file \"%s\"",
    "The intrinsic function set \"%s\" in object file \"%s\" conflicts with "
    "a previous intrinsic definition from other object files.  Each "
    "object file must define an identical list of intrinsics, in "
    "the same order.  Examine your source files and use the same "
    "intrinsic definitions in all files." },

    { TCERR_OBJFILE_META_CONFLICT,
    "conflicting metaclass \"%s\" found in object file \"%s\"",
    "The metaclass \"%s\" in object file \"%s\" conflicts with "
    "a previous metaclass definition from other object files.  Each "
    "object file must define an identical list of metaclasses, in "
    "the same order.  Examine your source files and use the same "
    "metaclass definitions in all files." },

    { TCERR_OBJFILE_MODREPOBJ_BEFORE_ORIG,
    "modified or replaced object \"%~.*s\" found in object file \"%s\" "
        "before original object definition was loaded",
    "The object \"%~.*s\" in object file \"%s\" is defined with \"replace\" "
    "or \"modify\", but this object file was loaded before the object file "
    "containing the original definition of the object.  You must change "
    "the order in which you're loading the object files so that the "
    "object file containing the original definition of each object is "
    "loaded before the object's first modification or replacement." },

    { TCERR_OBJFILE_REPFUNC_BEFORE_ORIG,
    "modified or replaced function \"%~.*s\" found in object file \"%s\" "
        "before original function definition was loaded",
    "The function \"%~.*s\" in object file \"%s\" is defined with "
    "\"modify\" or \"replace\", but this object file was loaded before the "
    "object file containing the original definition of the function.  You "
    "must change the order in which you're loading the object files so "
    "that the object file containing the original definition of each "
    "function is loaded before the function's first replacement." },

    { TCERR_CONSTRUCT_CANNOT_RET_VAL,
    "\"construct\" cannot return a value",
    "The \"construct\" method cannot return a value.  Remove the "
    "return value expression." },

    { TCERR_OBJFILE_NO_DBG,
    "object file \"%s\" has no debugger information "
    "(required to link for debug)",
    "The object file \"%s\" has no debugger information.  In order to "
    "include debug information in the image file, all object files must be "
    "compiled with debugging information.  Recompile the object file with "
    "debug information and then re-link the program." },

    { TCERR_OBJFILE_METACLASS_PROP_CONFLICT,
    "intrinsic class property \"%~.*s\" does not match "
        "previous name (\"%~.*s\") in object file %s",
    "The intrinsic class property \"%~.*s\" does not match its previous "
    "name (\"%~.*s\") in object file %s.  Each property defined in an "
    "intrinsic class must match in all object files where the "
    "intrinsic class is declared.  You must recompile your source code "
    "using a single definition for the intrinsic class that is the same "
    "in each file." },

    { TCERR_OBJFILE_METACLASS_IDX_CONFLICT,
    "intrinsic class \"%~.*s\" in object file %s out of order with "
       "previous definition",
    "The intrinsic class \"%~.*s\" is defined in object file %s in a "
    "different order than it appeared in previous object files.  Each "
    "object file must define all of its intrinsic classes in the same "
    "order as all other object files.  You must recompile your source "
    "code using a single set of intrinsic class definitions, all in the "
    "same order." },

    { TCERR_OBJFILE_CANNOT_MOD_OR_REP_TYPE,
    "cannot modify or replace object of this type - \"%~.*s\" in "
        "object file %s",
    "You cannot use 'modify' or 'replace' to modify an object of this "
    "type.  The symbol \"%~.*s\" is being modified or replaced in object "
    "file %s, but the symbol was previously defined using an incompatible "
    "type.  You can only modify or replace ordinary objects." },

    { TCERR_EXPORT_SYM_TOO_LONG,
    "exported symbol in object file is too long - object file may be "
        "corrupted",
    "An exported symbol in this object file is too long.  This probably "
    "indicates a corrupted object file.  Recompile the source code to "
    "create a new object file.  If the object file was just compiled, "
    "this probably indicates an internal error in the compiler." },

    { TCERR_OBJFILE_MACRO_SYM_TOO_LONG,
    "macro symbol too long in object file debug records - object file may "
        "be corrupted - object file %s",
    "A macro symbol name in the object file %s is too long.  This probably "
    "indicates a corrupted object file.  Recompile the source code to "
    "create a new object file.  If the object file was just compiled, "
    "this probably indicates an internal error in the compiler. " },

    { TCERR_OBJFILE_MMFUNC_INCOMPAT,
    "function \"%~.*s\" has conflicting definitions as multi-method and "
       "as regular function - object file %s",
    "The function \"%~*.s\" has a definition in object file \"%s\" that "
    "conflicts with one or more definitions in other object files. The "
    "function is defined both as an ordinary function and as a multi-method. "
    "The function must be defined exclusively as one or the other. If the "
    "ordinary function definition was meant to be a multi-method definition, "
    "add the \"multimethod\" modifier keyword immediately after the "
    "closing parenthesis of the parameter list in that definition." },

    { TCERR_OBJFILE_MMPARAM_NOT_OBJECT,
    "parameter type \"%~.*s\" in multi-method \"%~.*s\" is not an object",
    "The declared parameter type \"%~.*s\" in the definition of the "
    "multi-method function \"%~.*s\" is not an object. Only object or "
    "class names can be used to declare parameter types. Check the "
    "definition of the function and make sure the type name is correct, "
    "and that the same name isn't used elsewhere as a non-object type." },

    { TCERR_MISSING_MMREG,
    "library support function \"%s\" is missing or invalid - "
        "check that the system library is included in the build",
    "The system library function \"%s\" is missing or "
    "is not defined as a function. This function is part of the system "
    "run-time library, which is usually included by default in the build. "
    "Check that system.tl is included in the build and that the "
    "multi-method support module is not excluded." },

    { TCERR_CONST_POOL_OVER_32K,
    "constant value exceeds 32k - program will not run on 16-bit machines",
    "This constant data item (string or list constant) exceeds 32k in "
    "length (as stored in the compiled program file).  This program will "
    "not work on 16-bit computers.  If you want the program to be able "
    "to run on 16-bit machines, break up the string or list into multiple "
    "values, and manipulate them separately." },

    { TCERR_CONST_TOO_LARGE_FOR_PG,
    "string or list constant exceeds constant page size",
    "A string or list constant is too large to fit on "
    "a single constant page." },

    { TCERR_CORRUPT_LIST,
    "corrupted internal list data",
    "Corrupted internal list data." },

    { TCERR_GEN_CODE_INH,
    "attempting to generate 'inherited' node directly",
    "Attempting to generate 'inherited' node directly." },

    { TCERR_GEN_UNK_CONST_TYPE,
    "unknown constant type in code generation",
    "Unknown constant type in code generation." },

    { TCERR_GEN_BAD_LVALUE,
    "attempting to generate assignment for non-lvalue",
    "Attempting to generate assignment for non-lvalue." },

    { TCERR_GEN_BAD_ADDR,
    "attempting to generate address for non-addressable",
    "Attempting to generate address for non-addressable." },

    { TCERR_TOO_MANY_CALL_ARGS,
    "function call argument list exceeds machine limit (maximum 127)",
    "This function call has too many arguments; the T3 VM limits function "
    "and method calls to a maximum of 127 arguments.  Check for missing "
    "parentheses if you didn't intend to pass this many arguments.  If the "
    "function actually does take more than 127 arguments, you will have "
    "to modify your program to reduce the argument count; you could do "
    "this by grouping arguments into an object or list value, or by "
    "breaking the function into several simpler functions." },

    { TCERR_TOO_MANY_CTOR_ARGS,
    "'new' argument list exceeds machine limit (maximum 126)",
    "This 'new' operator has too many arguments; the T3 VM limits "
    "constructor calls to a maximum of 126 arguments.  Check for missing "
    "parentheses if you didn't intend to pass this many arguments.  If the "
    "constructor actually does take more than 126 arguments, you will have "
    "to modify your program to reduce the argument count; you could do "
    "this by grouping arguments into an object or list value." },

    { TCERR_GEN_CODE_DELEGATED,
    "attempting to generate 'delegated' node directly",
    "Attempting to generate 'delegated' node directly." },

    { TCERR_CODE_POOL_OVER_32K,
    "code block size exceeds 32k - program will not run on 16-bit machines",
    "This code block (a function or method body) exceeds 32k in length "
    "(as stored in the compiled program file).  This program will not work "
    "on 16-bit computers.  If you want the program to be able to run on "
    "16-bit machines, break up this function or method into multiple "
    "pieces." },

    { TCERR_GEN_BAD_CASE,
    "case/default without switch or non-constant case expression",
    "case/default without switch or non-constant case expression" },

    { TCERR_CATCH_FINALLY_GEN_CODE,
    "catch/finally gen_code called directly",
    "catch/finally gen_code called directly" },
    
    { TCERR_INVAL_PROP_CODE_GEN,
    "invalid property value type for code generation",
    "invalid property value type for code generation" },

    { TCERR_RESERVED_EXPORT,
    "external name \"%~.*s\" illegal in 'export' - reserved for compiler use",
    "The external name \"%~.*s\" illegally appears in an 'export' statement.  "
    "The compiler automatically provides an export for this symbol, "
    "so the program cannot explicitly export this name itself." },

    { TCERR_EXPR_TOO_COMPLEX,
    "expression too complex",
    "The expression is too complex.  If possible, simplify the "
    "expression by breaking it up into sub-expressions, storing "
    "intermediate values in local variables." },

    { TCERR_CONSTRUCT_NOT_DEFINED,
    "property \"construct\" is not defined",
    "No property named \"construct\" is defined.  Creating an object "
    "with operator 'new' invokes this property if defined." },

    { TCERR_CONSTRUCT_NOT_PROP,
    "\"construct\" is not a property",
    "The symbol \"construct\" does not refer to a property.  Creating "
    "an object with operator 'new' invokes this property if defined." },

    { TCERR_FINALIZE_NOT_DEFINED,
    "property \"finalize\" is not defined",
    "No property named \"finalize\" is defined.  This property is "
    "invoked when an object is about to be deleted during garbage "
    "collection." },

    { TCERR_FINALIZE_NOT_PROP,
    "\"finalize\" is not a property",
    "The symbol \"finalize\" does not refer to a property.  This property "
    "is invoked when an object is about to be deleted during garbage "
    "collection." },

    { TCERR_BAD_JS_TPL,
    "invalid js template", "Invalid js template." },

    { TCERR_JS_EXPR_EXPAN_OVF,
    "js template expansion overflow", "Overflow expanding js template." }
};

/* english message count */
size_t tc_message_count_english =
    sizeof(tc_messages_english)/sizeof(tc_messages_english[0]);

/* 
 *   the actual message array - we'll initialize this to the english list
 *   that's linked in, but if we find an external message file, we'll use
 *   the external file instead 
 */
const err_msg_t *tc_messages = tc_messages_english;

/* message count - initialize to the english message array count */
size_t tc_message_count =
    sizeof(tc_messages_english)/sizeof(tc_messages_english[0]);

