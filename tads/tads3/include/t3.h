#charset "us-ascii"
#pragma once

/* 
 *   Copyright (c) 1999, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3
 *   
 *   This header defines the t3vm intrinsic function set.  These functions
 *   provide access to basic features of the Virtual Machine.  
 */

/* 
 *   include the LookupTable intrinsic class, since t3GetGlobalSymbols()
 *   returns an instance of this class 
 */
#include "lookup.h"


/*
 *   T3 intrinsic function set definition
 */
intrinsic 't3vm/010006'
{
    /* 
     *   Explicitly run garbage collection.
     */
    t3RunGC();

    /* 
     *   Set the default output function or method.  The return value is the
     *   old function pointer or method, depending on which one is being set
     *   with this call.  (If 'val' is a function pointer, the return value
     *   will be the old function; if 'val' is a property ID, the return
     *   value is the old method.)
     *   
     *   The special values T3SetSayNoFunc and T3SetSayNoMethod can be passed
     *   to the function to remove any existing function or method,
     *   respectively, and are returned when appropriate to indicate that
     *   there was no previous setting.  
     */
    t3SetSay(val);

    /* 
     *   Get the VM version number.  Returns the version number as an integer
     *   value, with the major version in the high-order 16 bits, the minor
     *   version number in the next 8 bits, and the patch number ("point
     *   release" number) in the low-order 8 bits.  For example, version
     *   3.0.10 is encoded as 0x0003000A.  
     */
    t3GetVMVsn();

    /* 
     *   Get the VM identifier string.  This returns the version number as a
     *   string, as in '3.0.10'.
     */
    t3GetVMID();

    /* 
     *   Get the VM banner string.  This returns a string with the name of
     *   the VM, the version number, and a copyright string, in a format
     *   suitable for displaying to the user to identify the VM executable.  
     */
    t3GetVMBanner();

    /* 
     *   Get the preinitialization mode flag.  This returns true if the VM is
     *   running as part of the compiler's pre-initialization phase, nil if
     *   it's running as a normal interpreter.  
     */
    t3GetVMPreinitMode();

    /* 
     *   Debugger trace operations.  This provides access to the interactive
     *   debugger subsystem, if the VM is running under a debugger.  The
     *   'mode' argument determines what the function does and what the
     *   additional arguments, if any, are for:
     *   
     *   T3DebugCheck - checks to see if an interactive debugger is present.
     *   No additional arguments; returns true if a debugger is present, nil
     *   if not.
     *   
     *   T3DebugBreak - breaks into the interactive debugger, pausing
     *   execution at the current code location so that the user can inspect
     *   the current machine state and determine how to proceed.  No
     *   additional arguments; after the user proceeds with execution, the
     *   function returns true to indicate that a debugger is present.  If no
     *   debugger is present, the function simply returns nil, and has no
     *   other effect.
     *   
     *   T3DebugLog - writes a message to the debug log.  The second argument
     *   is a string with the text of the message to write.  When running
     *   under an interactive debugger, the log is usually displayed as a
     *   window in the UI, or something similar.  When running in a regular
     *   interpreter, the log is stored as a text file called tadslog.txt, in
     *   a directory location that varies by system.  When a log file is
     *   used, the system automatically adds a timestamp to each message.
     */
    t3DebugTrace(mode, ...);

    /*
     *   Get the global symbol table or the global macro table.
     *   
     *   'which' specifies which table to retrieve:
     *   
     *.     T3GlobalSymbols - return the global symbol table
     *.     T3PreprocMacros - return the preprocessor macro table
     *   
     *   If 'which' is omitted, the global symbol table is returned by
     *   default.
     *   
     *   If the requested symbol table is available, this returns a
     *   LookupTable object; otherwise, it returns nil.
     *   
     *   The symbol tables are available under two conditions.  First, while
     *   pre-initialization is running during the program build (compiling)
     *   process, regardless of the debug/release mode being used for
     *   compilation.  Second, during normal "t3run" execution, but only when
     *   the program has been compiled for debugging.  When you compile in
     *   release mode, the compiler omits the debugging symbols from the .t3
     *   image file to save space, so the symbol tables won't be available
     *   when running a release build under the interpreter. 
     *   
     *   If you want to access the symbol tables under normal execution
     *   (i.e., after preinit) in a release build, you can do it, but it
     *   requires an extra manual step.  The trick is to call this function
     *   during preinit, when the symbol tables are definitely available
     *   regardless of the debug/release mode, and then save a reference to
     *   each desired table in an object property.  This will ensure that the
     *   final image file saved after preinit completes includes the tables,
     *   because the object property reference ensures that the garbage
     *   collector won't delete them.  Now, you *still* can't access the
     *   tables again at run-time by calling t3GetGlobalSymbols(), but you
     *   can instead get the same information from your saved object
     *   property.  
     */
    t3GetGlobalSymbols(which?);

    /*
     *   Allocate a new property.  Returns a new property not yet used
     *   anywhere in the program.  Note that property ID's are a somewhat
     *   limited resource - only approximately 65,000 total are available,
     *   including all of the properties that the program defines
     *   statically.  
     */
    t3AllocProp();

    /*
     *   Get a stack trace.  This returns a list of T3StackInfo objects.
     *   Each object represents a nesting level in the call stack.  The first
     *   element in the list represents the currently active level (i.e., the
     *   level that called this function), the second element represents the
     *   caller of the first element, and so on.
     *   
     *   If 'level' is an integer, we'll return a single T3StackInfo object
     *   giving the context at the given stack level - 1 is the active level,
     *   2 is its caller, and so on, so 'level' would simply be the index in
     *   the returned list when this argument is omitted.  If 'level' is
     *   omitted or nil, we return a list of T3StackInfo objects giving the
     *   entire stack trace.
     *   
     *   If 'flags' is specified, it's a combination of T3GetStackXxx flags
     *   specifying additional options.  If this isn't included, the default
     *   is 0 (i.e., all flags turned off).  
     */
    t3GetStackTrace(level?, flags?);

    /*
     *   Get a named argument.  This searches for the specified named
     *   argument, and returns the value of the argument if it's defined.
     *   
     *   'name' is a string giving the name of the argument to search for.
     *   This must exactly match the name of an argument passed by a caller
     *   with the "name: value" syntax.  The match is case-sensitive.
     *   
     *   'defval' is an optional default value to return if the argument
     *   doesn't exist.  If 'deval' is specified, and the argument doesn't
     *   exist, the function returns 'defval'.  If 'defval' is omitted, and
     *   the argument doesn't exist, the function throws an error.
     */
    t3GetNamedArg(name, defval?);

    /*
     *   Get a list of all named arguments currently in effect.  This returns
     *   a list of strings, where each string is the name of a named argument
     *   that's currently active.  
     */
    t3GetNamedArgList();
}


/*
 *   t3DebugTrace() mode flags 
 */

/* check to see if the debugger is present */
#define T3DebugCheck     1

/* break into the debugger */
#define T3DebugBreak     2

/* log a message to the system/debug log */
#define T3DebugLog       3

/*
 *   t3SetSay() special values.  These can be passed in lieu of a function
 *   pointer or property ID when the caller wants to remove any existing
 *   function or method rather than install a new one.  
 */
#define T3SetSayNoFunc    1
#define T3SetSayNoMethod  2

/*
 *   t3GetGlobalSymbols 'which' flag.  One of these values can be specified
 *   as the function argument to specify which type of table is to be
 *   retrieved.  
 */
#define T3GlobalSymbols   1
#define T3PreprocMacros   2

/*
 *   t3GetStackTrace flags.  These can be combined with the bitwise '|'
 *   operator.
 */
#define T3GetStackLocals  0x0001
#define T3GetStackDesc    0x0002

/*
 *   Macro information flags. 
 */
#define T3MacroHasArgs     0x0001
#define T3MacroHasVarargs  0x0002

