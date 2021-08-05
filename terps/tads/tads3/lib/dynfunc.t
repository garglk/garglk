#charset "us-ascii"

/*
 *   Copyright (c) 2001, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This module defines classes related to the DynamicFunc intrinsic
 *   class.  You should include this source file in your build if you're
 *   using the DynamicFunc class.  
 */

#include <tads.h>
#include <dynfunc.h>


/* ------------------------------------------------------------------------ */
/*
 *   Compiler: This object provides a simplified interface to the dynamic
 *   compiler.  The methods here can be used instead of manually creating
 *   DynamicFunc instances.
 *   
 *   The main advantage of using this object to compile code is that it
 *   automatically provides access to the global symbol table that was used
 *   to compile the current program, for use in dynamic code.  Without the
 *   global symbol table, dynamic code won't have access to object names,
 *   property names, function names, and so on.  That doesn't stop you from
 *   compiling code that only depends upon its own function parameters and
 *   local variables, but for most purposes the global symbols are useful
 *   to have around.
 *   
 *   Note that including this object in a project will automatically save
 *   the global symbol table in the compiled .t3 file.  This increases the
 *   size of the .t3 file, as well as memory usage during execution.  If
 *   you're concerned about minimizing the .t3 file size or the run-time
 *   memory footprint, *and* you don't need global symbols for dynamic code
 *   (or you don't use the dynamic compiler at all), you can save some
 *   space by omitting this whole module from the build.  
 */
Compiler: PreinitObject
    /*
     *   Compile an expression or function.  'str' is a string giving the
     *   code to compile.  This can be a simple value expression, such as
     *   'Me.location' or 'new BigNumber(12345).sqrt()'.  Or, it can be a
     *   complete unnamed function definition, using this syntax:
     *   
     *.    'function(x, y, z) { ...body of function... }'
     *   
     *   The body of the function can contain any executable code that you
     *   could write in a regular function in static code: if, while,
     *   switch, return, etc.
     *   
     *   The return value is a DynamicFunc containing the compiled
     *   expression or function.  You call it by using the return value as
     *   though it were a function:
     *   
     *.    local f = Compiler.compile('Me.location');
     *.    local loc = f();  
     *   
     *   If the source string was just an expression, it acts like a
     *   function that takes zero arguments, and returns the computed value
     *   of the expression.  The expression is evaluated anew each time you
     *   invoke it, so you'll get the "live" value of an expression that
     *   refers to object properties or other external data.  In the
     *   example above, we'd get the current value of Me.location every
     *   time we call f().
     *   
     *   The source string is actually compiled immediately when you call
     *   this function.  This means it's checked for errors, such as syntax
     *   errors and unknown symbol names.  If the code contains any errors,
     *   this method throws a CompilerException describing the problem.
     */
    compile(str, locals?)
    {
        /* compile the string, using our saved global symbol table */
        return new DynamicFunc(str, symtab_, locals, macros_);
    }

    /*
     *   Compile a dynamic function string, and add it to the global symbol
     *   table as a function with the given name.  This effectively creates
     *   a new named function that you can call from other dynamic code
     *   objects. 
     */
    defineFunc(name, str, locals?)
    {
        /* 
         *   compile the string, and add it to our symbol table under the
         *   given function name 
         */
        symtab_[name] = new DynamicFunc(str, symtab_, locals, macros_);
    }

    /*
     *   Evaluate an expression.  'str' is a string giving code to compile.
     *   In most cases, this is simply a simple value expression, although
     *   it's also acceptable to use the 'function()' syntax to create a
     *   function that takes no arguments.
     *   
     *   This method compiles the source string and immediately calls the
     *   resulting compiled code.  The return value is the value returned
     *   from the compiled code itself.  This method thus provides a quick
     *   way to evaluate an expression.
     *   
     *   If the string contains any syntax errors or other compilation
     *   errors, the method throws a CompilerException.  In addition, it's
     *   possible for the compiled code to throw exceptions of its own;
     *   this method doesn't catch those, leaving it up to the caller to
     *   handle them.
     *   
     *   If you expect to evaluate the same expression repeatedly, you're
     *   better off using compile() to get the compiled representation of
     *   the expression, and then call that compiled code each time the
     *   value is needed.  That's more efficient than using eval() each
     *   time, since eval() to recompile the expression on every call,
     *   which is a fairly complex process.  
     */
    eval(str, locals?)
    {
        /* 
         *   compile the string, call the resulting function, and return
         *   the result from the function 
         */
        return (new DynamicFunc(str, symtab_, locals, macros_))();
    }

    /*
     *   During preinit, save a reference to the program's global symbol
     *   table in a property of self.  The VM always makes the global
     *   symbols available during preinit, but by default it discards the
     *   table after that because most programs don't need it.  That means
     *   that the symbols aren't available by default during normal
     *   execution.  However, saving a reference here prevents the garbage
     *   collector from discarding the table when preinit finishes, which
     *   forces it to be saved in the final .t3 file and thus makes it
     *   available permanently.  
     */
    execute()
    {
        /* save the global symbol table */
        symtab_ = t3GetGlobalSymbols(T3GlobalSymbols);
        macros_ = t3GetGlobalSymbols(T3PreprocMacros);
    }

    /* a saved reference to the global symbol table */
    symtab_ = nil

    /* a saved referenced to the preprocessor macro table */
    macros_ = nil
;
    

/* ------------------------------------------------------------------------ */
/*
 *   Compiler Exception.  'new DynamicFunc()' throws an exception of this
 *   class if an error occurs compiling the source code of the new object.
 */
class CompilerException: Exception
    construct(msg) { errmsg_ = msg; }
    displayException()
    {
        if (errmsg_ != nil)
            "<<errmsg_>>";
        else
            "Source code compilation error";
    }

    /* the error message from the compiler */
    errmsg_ = nil
;

/* export the compiler exception for use by the intrinsic class */
export CompilerException 'DynamicFunc.CompilerException';

