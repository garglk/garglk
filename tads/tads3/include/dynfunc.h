#charset "us-ascii"
#pragma once

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   The header defines the DynamicFunc intrinsic class.  This class is used
 *   for creating new functions from source code strings dynamically at
 *   run-time.
 *   
 *   Programs that use the DynamicFunc class should add the system library
 *   source file dynfunc.t to the build file list, since it contains some
 *   additional definitions that are useful with this class.  
 */


/* include our base class definitions */
#include "systype.h"

/*
 *   The DynamicFunc class is used to create new functions from source code
 *   strings dynamically at run-time.  To create a new executable function,
 *   create a new DynamicFunc object:
 *   
 *.    local f = new DynamicFunc('function(x) { return x*2; }',
 *.                              globals, locals, macros);
 *   
 *   The first constructor argument is a string giving the source code to
 *   compile.
 *   
 *   The optional second argument is a LookupTable containing the global
 *   symbols for the compilation.  This can be the table returned from
 *   t3GetGlobalSymbols(), or it can be a custom table.  The compiler looks
 *   up string keys in the table to resolve global symbol names; the
 *   corresponding values can be objects, properties, enum values, or
 *   function pointers.
 *   
 *   The optional third argument is a StackFrameDesc object, or a list of
 *   StackFrameDesc objects, giving the stack frames to use for local
 *   variable access within the dynamic code.  The source code can refer to
 *   local variables in these frames, and can assign new values to them.
 *   This is analogous to the way anonymous functions can refer to local
 *   variables in their lexically enclosing scopes.  If you supply a list,
 *   the compiler searches the frames in order of their appearance in the
 *   list.  If a variable appears in more than one frame in the list, the
 *   compiler will use the first occurrence it finds. 
 *   
 *   The optional fourth argument is a LookupTable giving the preprocessor
 *   macros to use for the compilation.  This is a table in the format
 *   returned by t3GetGlobalsSymbols(T3PreprocMacros).
 *   
 *   If the 'symtab' or 'macros' arguments are omitted or nil, the
 *   compilation is done without global symbols or macros, respectively.
 *   Note that built-in functions and intrinsic classes are identified by
 *   global symbols, so you won't be able to call any built-ins if you don't
 *   provide a symbol table.
 *   
 *   The source code string can start with the keyword 'function', followed
 *   by a list of parameter names, followed by the executable statements in
 *   braces.  Or, it can simply be a list of statements, in which case it's a
 *   function with no arguments, as though it started with 'function()'.
 *   
 *   Creating a DynamicFunc compiles the source code.  If a compilation error
 *   occurs (such as a syntax error, or an unknown symbol reference), the
 *   'new' call will throw a CompilerException error.  The exception object
 *   contains information on the specific compiler error.
 *   
 *   To invoke the new function, simply use the standard function call syntax
 *   on the object value, as though it were a regular function pointer:
 *   
 *.     local y = f(7);
 */
intrinsic class DynamicFunc 'dynamic-func/030000'
{
    /*
     *   Get the original source code of the function.  This returns the
     *   string that was used as the source string to create the function.  
     */
    getSource();
}

