#charset "us-ascii"

/* 
 *   Copyright (c) 1999, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3
 *   
 *   This header defines the t3vm intrinsic function set.  These functions
 *   provide access to basic features of the Virtual Machine.  
 */

/*
 *   T3 intrinsic function set definition
 */

#ifndef T3_H
#define T3_H

/* 
 *   include the LookupTable intrinsic class, since t3GetGlobalSymbols()
 *   returns an instance of this class 
 */
#include "lookup.h"


/* 
 *   define the T3 system interface 
 */
intrinsic 't3vm/010005'
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
     */
    t3DebugTrace(mode, ...);

    /*
     *   Get the global symbol table.  If a symbol table is available, this
     *   returns a LookupTable object; otherwise, it returns nil.
     *   
     *   This call will return a valid object value when pre-initialization
     *   is running during program building, or when the program has been
     *   compiled for debugging.  When a program compiled for release (i.e.,
     *   no debug information) is run under the interpreter, this will
     *   return nil, because no symbol information is available.
     *   
     *   Note that programs can, if they wish, get a reference to this
     *   object during pre-initialization, then keep the reference (by
     *   storing it in an object property, for example) so that it is
     *   available during normal execution under the interpreter.  If the
     *   program is compiled for release, and it does not keep a reference
     *   in this manner, the garbage collector will automatically delete the
     *   object when pre-initialization is completed.  This allows programs
     *   that wish to keep the symbol information around at run-time to do
     *   so, while not burdening programs that don't need the information
     *   with the extra memory the symbols consume.  
     */
    t3GetGlobalSymbols();

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
     *   If 'level' is specified, we'll return a single T3StackInfo object
     *   giving the context at the given stack level - 1 is the active level,
     *   2 is its caller, and so on, so 'level' would simply be the index in
     *   the returned list when this argument is omitted.  
     */
    t3GetStackTrace(level?);
}

/*
 *   t3DebugTrace() mode flags 
 */

/* check to see if the debugger is present */
#define T3DebugCheck     1

/* break into the debugger */
#define T3DebugBreak     2

/*
 *   t3SetSay() special values.  These can be passed in lieu of a function
 *   pointer or property ID when the caller wants to remove any existing
 *   function or method rather than install a new one.  
 */
#define T3SetSayNoFunc    1
#define T3SetSayNoMethod  2


#endif /* T3_H */
