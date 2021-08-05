#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved.
 *   
 *   This module defines a number of low-level functions and classes that
 *   most TADS 3 programs will need, whether based on the adv3 library or
 *   not.  This module includes the main program entrypoint, the basic
 *   Exception classes, and the modular initialization framework.
 *   
 *   The compiler automatically links this module into every program by
 *   default, but you can override this by specifying the "-nodef" option
 *   to t3make.  If you remove this module, you'll have to provide your own
 *   implementations for many of the functions and classes defined here.  
 */

#include "tads.h"
#include "reflect.h"
#include "strbuf.h"


/* ------------------------------------------------------------------------ */
/*
 *   Main program entrypoint.  The VM invokes this function at program
 *   startup.  
 */
_main(args)
{
    /* call the common main entrypoint, with no startup file specified */
    _mainCommon(args, nil);
}

/* declare 'main' as a function, in case it's not otherwise defined */
extern function main;

/*
 *   Main program entrypoint for restoring a saved state.  The VM invokes
 *   this function at startup instead of _main() when the user explicitly
 *   specifies a saved state file to restore when starting the program.
 *   (On a command-line interpreter, this would involve using a special
 *   option on the T3 interpreter command line; for a GUI shell, this
 *   might simply involve double-clicking on the desktop icon for a saved
 *   state file.)
 *   
 *   Note that we must export this as the 'mainRestore' symbol so that the
 *   interpreter knows how to find it.  
 */
export _mainRestore 'mainRestore';
_mainRestore(args, restoreFile)
{
    /* call the common main entrypoint */
    _mainCommon(args, restoreFile);
}

/*
 *   Common main entrypoint.  This function can be called with or without
 *   a saved state file to restore.  
 */
_mainCommon(args, restoreFile)
{    
    try
    {
        /* 
         *   Keep going as long as we keep restarting.  Note that the
         *   restoreFile only counts on the first iteration, so we clear it
         *   out after each iteration; if we RESTART after that, we'll want
         *   to just start the game from the beginning.  
         */
        for ( ; ; restoreFile = nil)
        {
            /* perform load-time initialization */
            initAfterLoad();

            /* if we're in preinit-only mode, we're done */
            if (t3GetVMPreinitMode())
                return;

            /* catch any RESTART signals thrown out of the main entrypoint */
            try
            {
                /* 
                 *   If there's a saved state file to restore, call our
                 *   mainRestore() function instead of main().  If
                 *   mainRestore() isn't defined, show a message to this
                 *   effect but keep going.  
                 */
                if (restoreFile != nil
                    && dataType(mainGlobal.mainRestoreFunc) == TypeFuncPtr)
                {
                    /* 
                     *   Call the user's main startup-and-restore
                     *   entrypoint function.  Note that we call indirectly
                     *   through our function pointer so that we don't
                     *   force a function called mainRestore() to be linked
                     *   with the program.  
                     */
                    (mainGlobal.mainRestoreFunc)(args, restoreFile);
                }
                else
                {
                    /* 
                     *   if we have a restore file but no mainRestore
                     *   routine, explain to the user that they'll have to
                     *   restore manually 
                     */
                    if (restoreFile != nil)
                    {
                        "\n[This program cannot restore the saved position
                        file automatically.  Please try restoring the
                        saved position file again using a command within
                        the program.]\b";
                    }

                    /* call the selected main entrypoint */
                    flexcall(&main, args);
                }

                /* 
                 *   we made it through the main entrypoint without a
                 *   restart, so we're done 
                 */
                break;
            }
            catch (RestartSignal rsig)
            {
                /* 
                 *   call the intrinsic restartGame function to reset all
                 *   of the static objects to their initial state 
                 */
                restartGame();

                /* 
                 *   Now that we've reset the VM, update the restart ID in
                 *   the main globals.  Note that we waited until now to do
                 *   this, because this change would have been lost in the
                 *   reset if we'd made the change before the reset.  Note
                 *   also that the 'rsig' object itself will survive the
                 *   reset because the thrower presumably allocated it
                 *   dynamically, hence it's not a static object subject to
                 *   reset.  
                 */
                mainGlobal.restartID = rsig.restartID;

                /*
                 *   Now we can just continue on to the next iteration of
                 *   the restart loop.  This will take us back to the
                 *   initialization and enter the game as though we'd just
                 *   started the program again.  
                 */
            }
        }
    }
    catch (ProgramException exc)
    {
        /* 
         *   just re-throw these out to the VM, so that the VM exits to
         *   the operating system with an error indication 
         */
        throw exc;
    }
    catch (Exception exc)
    {
        /* write output to the main console */
        t3SetSay(_default_display_fn);
        
        /* display the unhandled exception */
        "\n<<exc.displayException()>>\n";
    }
    finally
    {
        /* before exiting, call registered exit handlers */
        mainAtExit.callHandlers();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Flexible function call.  This calls the given function, passing as
 *   many arguments from the given argument list as the function actually
 *   wants.  If the list doesn't have enough arguments to satisfy the
 *   function's minimum requirements, we add 'nil' arguments to pad out the
 *   minimum.  If the list exceeds the function's maximum, we drop
 *   arguments past the maximum.
 */
flexcall(func, [args])
{
    /* get the function's desired argument list */
    local paramDesc = getFuncParams(func);

    /* add or remove arguments as needed to fit the limits */
    local n = args.length();
    if (n < paramDesc[1])
    {
        /* we need more to satisfy the minimum - add nil values */
        args += makeList(nil, paramDesc[1] - n);
    }
    else if (n > paramDesc[1] + paramDesc[2] && !paramDesc[3])
    {
        /* 
         *   the function doesn't take infinite arguments, and we have more
         *   than it allows in the fixed plus optional parts, so drop the
         *   extra elements 
         */
        args = args.sublist(1, paramDesc[1] + paramDesc[2]);
    }

    /* call the function */
    func(args...);
}

/* ------------------------------------------------------------------------ */
/*
 *   Restart signal.  This can be used to restart from the main
 *   entrypoint.  The caller should create one of these objects, then use
 *   restartGame() (or an equivalent from a different function set, if
 *   appropriate) to reset static object state to the initial program load
 *   conditions, then throw the signal object.  
 */
class RestartSignal: Exception
    construct()
    {
        /* 
         *   use the next restart ID, so we can tell that we're on a fresh
         *   run on this session 
         */
        restartID = mainGlobal.restartID + 1;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   General post-load initialization.  The main program entrypoint
 *   _main() calls this routine to set up the default display function,
 *   run pre-initialization if necessary, and run initialization.  This
 *   routine is also useful for the target of a restartGame() routine, to
 *   perform all of the basic load-time initialization again after a
 *   restart.  
 */
initAfterLoad()
{
    /* establish the default display function */
    t3SetSay(_default_display_fn);

    /* if we haven't run preinit, do so now */
    if (!mainGlobal.preinited_)
    {
        /* 
         *   Explicitly run garbage collection prior to preinit.
         *   
         *   In most cases, this is unnecessary, but in some cases it's
         *   important.  In particular, object loops (over all objects, or
         *   over all instances of a given class) can still see otherwise
         *   unreachable objects.  It's common to do object loops in
         *   preinit to set up static data caches and tables and so on.  If
         *   there *were* any garbage objects lying around, preinit could
         *   find them via object loops, and might register them into
         *   tables or what not.
         *   
         *   Even considering the preinit object loop, doing a garbage
         *   collection sweep would *still* be redundant in most cases,
         *   since preinit is normally done right after compilation, when
         *   the program wouldn't yet have had a chance to create any
         *   garbage objects to be worried about in object loops.  However,
         *   there's still one more case to consider, and that's RESTART:
         *   in a debug build, or even in some release builds, we'd have to
         *   re-run preinit after a RESTART, and there certainly could be
         *   garbage objects left around from before the RESTART.
         *   
         *   To ensure that we deal gracefully with this combination of
         *   conditions - garbage objects, RESTART, and object loops in
         *   preinit - simply do an explicit garbage collection run before
         *   invoking preinit.  
         */
        t3RunGC();

        /* run our internal preinit */
        _preinit();
        
        /* remember that we've run preinit */
        mainGlobal.preinited_ = true;
    }

    /* if we're not in preinit-only mode, run internal initialization */
    if (!t3GetVMPreinitMode())
        _init();
}



/* ------------------------------------------------------------------------ */
/*
 *   Module Execution Object.  This is an abstract base class for various
 *   classes that provide modular execution hooks.  This class and its
 *   subclasses are mix-in classes - they can be multiply inherited by any
 *   object (as long as it's not already some other kind of module
 *   execution object).
 *   
 *   The point of the Module Execution Object and its subclasses is to
 *   allow libraries and user code to define execution hooks, without
 *   having to worry about what other libraries and user code bits are
 *   defining the same hook.  When we need to execute a hook defined via
 *   this object, we iterate over all of the instances of the appropriate
 *   subclass and invoke its execute() method.
 *   
 *   By default, the order of execution is arbitrary.  In some cases,
 *   though, dependencies will exist, so that one object cannot be invoked
 *   until another object has already been invoked.  In these cases, you
 *   must set the execBeforeMe property to contain a list of the objects
 *   whose execute() methods must be invoked before this object's
 *   execute() method is invoked.  The library will check this list before
 *   calling execute() on this object, and ensure that each object in the
 *   list has been invoked before calling this object's execute().  
 */
class ModuleExecObject: object
    /* 
     *   List of objects that must be executed before me - by default, the
     *   order doesn't matter, so we'll set this to an empty list.
     *   Instances can override this if it is necessary to execute other
     *   objects before this object can be executed.  
     */
    execBeforeMe = []

    /*
     *   List of objects that must be executed after me - this is
     *   analogous to execBeforeMe, but we make sure we run before these. 
     */
    execAfterMe = []

    /* 
     *   Subclass-specific execution method.  Each subclass should
     *   override this method to provide its execution code.  
     */
    execute() { }


    /* 
     *   PRIVATE METHODS AND PROPERTIES.  Subclasses and instances should
     *   not need to override or invoke these.  
     */

    /* flag - true if we've been executed on this round */
    isExecuted_ = nil

    /* flag - true if we're in the process of executing */
    isDoingExec_ = nil

    /* execute - internal method: checks dependency order */
    _execute()
    {
        /*
         *   If I've already been executed, there's nothing more that I
         *   need to do.  We might be called by the arbitrarily-ordered
         *   iteration over all objects after we've already been executed,
         *   because we might be executed explicitly by an object that
         *   depends upon us if it's reached before we are.  
         */
        if (isExecuted_)
            return;

        /* 
         *   If we're in the process of executing any of the objects we
         *   depend upon, and a dependent calls us, we have a circular
         *   dependency.  
         */
        if (isDoingExec_)
            throw new CircularExecException(self);

        /*
         *   Mark ourselves as being in the process of executing.  If
         *   there are any circular dependencies (i.e., if we depend on an
         *   object, which in turn depends on us), it's clearly an error,
         *   in that both objects can't be executed before the other.
         *   This flag allows us to detect circular dependencies by
         *   noticing if we're called by a dependent while we're in the
         *   process of calling the things we depend upon.  
         */
        isDoingExec_ = true;
        
        /*
         *   Check each entry in my 'before' list to ensure that they've
         *   all been executed already.  Invoke execute() now for any that
         *   haven't.  
         */
        for (local i = 1, local cnt = execBeforeMe.length() ;
             i <= cnt ; ++i)
        {
            local cur;

            /* get this object */
            cur = execBeforeMe[i];
            
            /* if this one hasn't been executed yet, do so now */
            if (!cur.isExecuted_)
            {
                /* 
                 *   This one hasn't been executed yet - explicitly
                 *   execute it now.  Note that we do this recursively
                 *   through the internal execution method, so that 'cur'
                 *   has a chance to execute any objects that it depends
                 *   upon.  
                 */
                cur._execute();
            }
        }

        /* 
         *   we've resolved all of our dependencies, so we're good to go -
         *   run the user's execution code 
         */
        execute();

        /* 
         *   mark ourselves as having been executed, so we don't run the
         *   user's code again should we be called again by a dependent or
         *   by the global iteration loop later in the scan 
         */
        isExecuted_ = true;
        isDoingExec_ = nil;
    }

    /* flag to indicate that this is the first time running classExec */
    hasInitialized_ = nil

    /*
     *   Class execution.  Call this method on the particular class of
     *   modules to execute.  We'll iterate over all instances of that
     *   class and invoke each instance's _execute() method. 
     */
    classExec()
    {
        /*
         *   If this is the first time running this classExec, turn
         *   execAfterMe dependencies into appropriate execBeforeMe
         *   dependencies.  
         */
        if (!hasInitialized_)
        {
            /* 
             *   Go through all instances of this type of initializer, and
             *   re-cast the execAfterMe lists as execBeforeMe lists.  
             */
            forEachInstance(self,
                function(obj)
                {
                    foreach(local dependent in obj.execAfterMe)
                        dependent.execBeforeMe += obj;
                });

            /* remember that we're now initialized */
            hasInitialized_ = true;
        }
        
        /* 
         *   since we're starting a new round, clear all of the 'executed'
         *   flags in all of the objects, to ensure that we execute all
         *   objects on this round (this cleans up the flag settings from
         *   any previous rounds) 
         */
        forEachInstance(self,
            { obj: obj.isExecuted_ = obj.isDoingExec_ = nil });

        /* execute all objects */
        forEachInstance(self, { obj: obj._execute() });
    }
;

/*
 *   Pre-Initialization object.  During pre-initialization, we'll invoke
 *   the execute() method on each instance of this class.  
 */
class PreinitObject: ModuleExecObject
    /*
     *   Each instance of this object MUST override execute() with the
     *   specific pre-initialization code that the instance wants to
     *   perform.
     *   
     *   In addition, each instance can optionally set the property
     *   execBeforeMe to a list of the other PreinitObject's that must be
     *   invoked before this object is.  If this property is not set, this
     *   object's place in the preinit execution order will be arbitrary.  
     */
;

/*
 *   Initialization object.  During initialization, just before calling
 *   the user's main(args) function, we'll invoke the execute() method on
 *   each instance of this class. 
 */
class InitObject: ModuleExecObject
    /*
     *   Each instance of this object MUST override execute() with the
     *   specific initialization code that the instance wants to perform.
     *   
     *   In addition, each instance can optionally set the property
     *   execBeforeMe to a list of the other InitObject's that must be
     *   invoked before this object is.  If this property is not set, this
     *   object's place in the initialization execution order will be
     *   arbitrary.  
     */
;


/*
 *   Exception: circular execution dependency in ModuleExecObject
 */
class CircularExecException: Exception
    construct(obj) { obj_ = obj; }
    displayException()
    {
        "circular module dependency detected (refer to
        ModuleExecObject._execute() in _main.t)";
    }

    /* 
     *   The object that detected the circular dependency.  We can't use
     *   this for much ourselves, but it might be useful to store this
     *   information so that it's available to the programmer from within
     *   the debugger.  
     */
    obj_ = nil
;

/*
 *   Library pre-initialization.  This is called immediately after
 *   compilation to pre-initialize the program.  Any changes made here to
 *   object states become part of the initial state stored in the image
 *   file, so this establishes the static initial state of the program.
 *   
 *   The advantage of doing work during pre-initialization is that this
 *   work is done once, during compilation, and is thus not repeated each
 *   time a user starts the program.  Time-consuming initialization work
 *   can thus be made invisible to the user.
 *   
 *   Note that the pre-initialization code should never do anything that
 *   involves the user interface, since this code runs during compilation
 *   and does not run again when users start the program.  So, anything
 *   that you want a user to see must be done during normal initialization
 *   (such as in the main() routine), not here.  
 */
_preinit()
{
    local symtab;

    /* try getting the mainRestore() function from the global symbol table */
    if ((symtab = t3GetGlobalSymbols()) != nil)
        mainGlobal.mainRestoreFunc = symtab['mainRestore'];

    /* execute all preinit objects */
    PreinitObject.classExec();
}

/*
 *   Library initialization.  This is called during each program start-up
 *   to initialize the program.  Since this is run each time the user
 *   starts the program, this can display any introductory messages, set
 *   up the user interface, and so on.  
 */
_init()
{
    /* execute all init objects */
    InitObject.classExec();
}

/* ------------------------------------------------------------------------ */
/*
 *   For convenience, a simple object iterator function.  This function
 *   invokes a callback function for each instance of the given class, in
 *   arbitrary order.
 *   
 *   The callback is invoked with one argument, which gives the current
 *   instance.  The callback can "break" out of the loop by throwing a
 *   BreakLoopSignal, which can be done conveniently using the breakLoop
 *   macro.  
 */
forEachInstance(cls, func)
{
    try
    {
        /* loop over all objects of the given class */
        for (local obj = firstObj(cls) ; obj != nil ; obj = nextObj(obj, cls))
            func(obj);
    }
    catch (BreakLoopSignal sig)
    {
        /* 
         *   ignore the signal - it simply means we want to terminate the
         *   loop and return to the caller 
         */
    }
}

/*
 *   Find an instance of the given class for which the given function
 *   returns true.  We iterate over objects of the given class in
 *   arbitrary order, and return the first instance for which the function
 *   returns true.  Retursn nil if there is no such instance.  
 */
instanceWhich(cls, func)
{
    /* loop over all objects of the given class */
    for (local obj = firstObj(cls) ; obj != nil ; obj = nextObj(obj, cls))
    {
        /* if the callback returns true for this object, return the object */
        if (func(obj))
            return obj;
    }

    /* 
     *   we didn't find any instances for which the callback returns true;
     *   indicate this by returning nil 
     */
    return nil;
}

/*
 *   An exception object for breaking out of a callback loop, such as
 *   forEachInstance. 
 */
class BreakLoopSignal: Exception
    displayException() { "loop break signal"; }
;


/* ------------------------------------------------------------------------ */
/*
 *   Get the "translated" datatype of a value.  This is essentially the
 *   same as dataType(), except that anonymous function objects and dynamic
 *   function objects are treated as being "function pointer" types
 *   (TypeFuncPtr).  
 */
dataTypeXlat(val)
{
    local t;

    /* get the base type */
    t = dataType(val);
    
    /* if it's an anonymous function, return TypeFuncPtr */
    if (t == TypeObject
        && (val.ofKind(AnonFuncPtr)
            || (defined(DynamicFunc) && val.ofKind(DynamicFunc))))
        return TypeFuncPtr;

    /* otherwise, just return the base type */
    return t;
}

/* ------------------------------------------------------------------------ */
/*
 *   Base class for all exception objects.  We derive all exceptions from
 *   this base class so that we can write 'catch' blocks that catch all
 *   exceptions by catching 'Exception'.
 *   
 *   The displayException() method displays a message describing the
 *   exception.  Subclasses should override this method.  
 */
class Exception: object
    /* construct, with an optional message describing the error */
    construct(msg?, ...)
    {
        /* if there's a message, save it, otherwise keep the default */
        if (msg != nil)
            errmsg_ = msg;
    }

    /* display the exception - should always be overridden */
    displayException()
    {
        "<<errmsg_>>";
    }

    /* 
     *   Get the exception message as a string.  This captures the output
     *   of displayException() and returns it a string.  Use this instead
     *   of accessing errmsg_, since that member is private and might not
     *   reflect the actual displayed message. 
     */
    getExceptionMessage()
    {
        /* capture and return the displayed exception message as a string */
        return _outputCapture({: displayException() });
    }

    /* 
     *   Private member: The error message passed to the constructor, if
     *   any.  Note that this doesn't necessarily contain the actual
     *   displayed exception message, since displayException() can be
     *   overridden in subclasses to display additional parameters or other
     *   text entirely.  The definitive message is the one that
     *   displayException() generates.  If you want the displayed message
     *   as a string, use getExceptionMessage().  
     */
    errmsg_ = 'Unknown exception'

    /* 
     *   Display a stack trace, given a list of T3StackInfo objects.  Note
     *   that, for efficiency, we do not by default cache a stack trace
     *   when an exception occurs; individual subclasses can obtain a
     *   stack trace if desired at construction and use the information to
     *   show a stack trace for the exception. 
     */
    showStackTrace(stackList)
    {
        local haveSrc;

        /* check to see if there's any source info in the stack trace */
        haveSrc = nil;
        foreach (local cur in stackList)
        {
            /* note if we have source info here */
            if (cur.srcInfo_ != nil)
            {
                /* 
                 *   we have source information - note it and stop
                 *   searching, since even one bit of source info is
                 *   enough to show the stack 
                 */
                haveSrc = true;
                break;
            }
        }

        /* 
         *   if we have any source information at all, or we have
         *   reflection services available to decode the stack trace
         *   symbolically, show the stack 
         */
        if (haveSrc || mainGlobal.reflectionObj != nil)
        {
            for (local i = 1, local cnt = stackList.length() ; i <= cnt ; ++i)
            {
                local cur = stackList[i];
                
                /* show a mark next to level 1, spaces elsewhere */
                if (i == 1)
                    "--&gt;";
                else
                    "\ \ \ ";

                /* 
                 *   if there's a system reflection object, show symbolic
                 *   information on the current function call; otherwise,
                 *   simply show the source location 
                 */
                if (mainGlobal.reflectionObj != nil)
                {
                    /* reflection is available - show full symbolic info */
                    "<<mainGlobal.reflectionObj.
                      formatStackFrame(cur, true)>>";
                }
                else
                {
                    /* no reflection information - show source only */
                    if (cur.srcInfo_ != nil)
                        "<<cur.srcInfo_[1]>>, line <<cur.srcInfo_[2]>>";
                    else if (cur.isSystem())
                        "&lt;System&gt;";
                    else
                        "???";
                }

                /* end the line */
                "\n";
            }
        }
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   RuntimeError exception class.  The VM creates and throws an instance
 *   of this class when any run-time error occurs.  The VM explicitly sets
 *   the exceptionMessage property to a string giving the VM error message
 *   for the run-time error that occurred.  
 */
class RuntimeError: Exception
    construct(errno, ...)
    {
        /* remember the VM error number */
        errno_ = errno;

        /* 
         *   Store a stack trace for the current location.  Always discard
         *   the first element of the result, since this will reflect
         *   RuntimeError.construct, which is obviously not interesting.  
         */
        stack_ = t3GetStackTrace().sublist(2);

        /* 
         *   The next element of the stack trace is usually a native code
         *   frame, because the VM itself invokes our constructor in
         *   response to a runtime exception; this is not an interesting
         *   frame, so if it's present, remove it.  
         */
        if (stack_.length() > 0 && stack_[1].isSystem())
            stack_ = stack_.sublist(2);
    }

    /* create a runtime error with a given error message */
    newRuntimeError(errno, msg)
    {
        local e = new RuntimeError(errno);
        e.exceptionMessage = msg;
        return e;
    }

    /* display the exception */
    displayException()
    {
        /* show the exception message */
        "Runtime error: <<exceptionMessage>>\n";

        /* show a stack trace if possible */
        showStackTrace(stack_);
    }

    /* check to see if it's a debugger signal of some kind */
    isDebuggerSignal()
    {
        return errno_ is in (
            2391,                        /* debugger 'abort command' signal */
            2392                               /* debugger 'restart' signal */
        );
    }

    /* the VM error number of the exception */
    errno_ = 0

    /* the exception message, provided to us by the VM after creation */
    exceptionMessage = ''

    /* the stack trace, which we store at the time we're created */
    stack_ = nil
;

/*
 *   Export our RuntimeError class so that the VM knows about it and can
 *   create instances of it.  Also export our exceptionMessage property,
 *   so the VM can store its explanatory text there.
 */
export RuntimeError;
export exceptionMessage;

/*
 *   Unknown character set exception - this is thrown from any routine that
 *   needs a local character set mapping when no mapping exists on the local
 *   platform. 
 */
class UnknownCharSetException: Exception
    displayException = "Unknown character set"
;

/* 
 *   this exception object must be exported for use by the CharacterSet
 *   intrinsic class 
 */
export UnknownCharSetException 'CharacterSet.UnknownCharSetException';


/*
 *   A Program Exception terminates the entire program, passing an error
 *   indication to the operating system.  The VM doesn't provide a way to
 *   specify the *particular* error code to return to the OS, as there's no
 *   portable set of error codes; rather, the VM simply returns a code to
 *   the OS that means generically that an error occurred, if there's any
 *   such concept on the local operating system.  The VM will normally
 *   display this message just before it terminates the program, possibly
 *   with some additional text mentioning that a program error occurred
 *   (such as "unhandled exception: <your message>").
 */
class ProgramException: Exception
    construct(msg) { exceptionMessage = msg.htmlify(); }
    displayException() { "<<exceptionMessage>> "; }
;

/*
 *   A StorageServerError is thrown when a file operation on a remote
 *   storage server fails.  The storage server is used when the game runs
 *   on a Web game server in client/server mode.  In Web mode, files are
 *   stored on a separate storage server rather than on the Web server
 *   itself, so that the files can be transparently accessed if the game is
 *   continued from another Web server.  This exception is used when a
 *   request to the storage server fails, which could be due to an error on
 *   the storage server, a network error communicating between the game
 *   server and the storage server, or an invalid request (e.g., incorrect
 *   user credentials).  
 */
class StorageServerError: RuntimeError
    construct(errno, msg)
    {
        /* 
         *   Do the base class construction.  Note that errno is the
         *   VM-level error number, which is usually just the generic
         *   "storage server error" code.  The storage server provides a
         *   separate, more specific error code of its own as the first
         *   token of the message string.  
         */
        inherited(errno);
        
        /* 
         *   storage server error messages are formatted with an error code
         *   as the first space-delimited token, and a human-readable
         *   message following 
         */
        local sp = msg.find(' ');
        errCode = msg.substr(1, sp - 1);
        msg = msg.substr(sp + 1);

        /* 
         *   If the error code is a negative integer, it's an error on the
         *   client side sending the HTTP request to the storage server.
         *   If it's a positive integer, it's an HTTP error code from the
         *   storage server.  Otherwise it's an error abbreviation token
         *   from the server. 
         */
        local errNum = toInteger(errCode);
        if (errNum < 0)
        {
            local reqErrs = [
                -1 -> 'out of memory',
                -2 -> 'unable to connect',
                -3 -> 'network error',
                -4 -> 'invalid parameters',
                -5 -> 'error reading temporary file',
                -6 -> 'error writing temporary file',
                * -> 'error code <<errNum>>'
            ];
            errMsg = reqErrs[errNum];
        }
        else if (errNum > 0)
            errMsg = 'HTTP error (status code <<errCode>>)';
        else
            errMsg = msg;
    }

    /* the storage server error code */
    errCode = nil

    /* 
     *   error message - this is the message text we get back from the
     *   storage server for a request that's successful at the HTTP level
     *   but fails on the storage server, OR a message describing the HTTP
     *   error or network error that caused the request to fail 
     */
    errMsg = 'no details available'

    /* display the exception */
    displayException()
    {
        /* show the exception message */
        "Storage server error: <<errMsg>>\n";

        /* show a stack trace if possible */
        showStackTrace(stack_);
    }
;

/* export this for use by the interpreter networking package */
export StorageServerError 'StorageServerError';


/* ------------------------------------------------------------------------ */
/*
 *   Default string display function.  Our main entrypoint code
 *   establishes this function as the default output function.  
 */
_default_display_fn(str) { _tads_io_say(str); }

/*
 *   Raw output capture.  This bypasses any filtering and directly captures
 *   any output generated by the callback. 
 */
_outputCapture(func)
{
    /* temporarily set the low-level string output to capture output */
    local buf = new StringBuffer();
    local oldSay = t3SetSay({str: buf.append(str)});

    /* make sure we restore the "say" function on the way out */
    try
    {
        /* call the callback in the new capture context */
        func();

        /* return the captured data */
        return toString(buf);
    }
    finally
    {
        /* restore the old "say" function */
        t3SetSay(oldSay);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   The stack information object.  The intrinsic function
 *   t3GetStackTrace() in the 't3vm' function set returns a list of these
 *   objects; each object represents a level in the stack trace.  
 */
class T3StackInfo: object
    /*
     *   Construct a stack level object.  The system invokes this
     *   constructor with information on the stack level.
     */
    construct(func, obj, prop, selfObj, argList, srcInfo,
              locals, namedArgs, frameDesc)
    {
        /* remember the values */
        func_ = func;
        obj_ = obj;
        prop_ = prop;
        self_ = selfObj;
        argList_ = argList;
        srcInfo_ = srcInfo;
        locals_ = locals;
        namedArgs_ = namedArgs;
        frameDesc_ = frameDesc;
    }

    /*
     *   Is this a system routine?  This returns true if an intrinsic
     *   function or an intrinsic class method is running at this level. 
     */
    isSystem()
    {
        /*
         *   It's a system function if:
         *   
         *   - we have NEITHER a function nor a method
         *.  - the function is a built-in function pointer
         *.  - the defining object is an intrinsic class
         *   
         *   The first case applies to pre-3.0.19 VMs, where no information
         *   was available for native callers.  Starting in 3.0.19, full
         *   information is available.  
         */
        return ((func_ == nil && obj_ == nil)
                || dataType(func_) == TypeBifPtr
                || (obj_ != nil && IntrinsicClass.isIntrinsicClass(obj_)));
    }

    /* 
     *   the function running at this stack level - this is nil if an
     *   object property is running instead of a function 
     */
    func_ = nil

    /* 
     *   The object and property running at this stack level - these are
     *   nil if a function is running instead of an object method.  The
     *   object is the object where the method is actually defined - this
     *   might not be the same as self, because the object might have
     *   inherited the method from a base class.  
     */
    obj_ = nil
    prop_ = nil

    /* 
     *   the 'self' object at this level - this is nil if a function is
     *   running at this level instead of an object method 
     */
    self_ = nil

    /* 
     *   The list of positional arguments to the function or method.  Each
     *   element is the value of an argument; the list is arranged in the
     *   same order as the arguments. 
     */
    argList_ = []

    /*
     *   Local variables.  This is a LookupTable containing the local
     *   variables currently in scope at this stack level.  Each element in
     *   the table has a string key (index) giving the name of the local
     *   variable, and each corresponding value is the local's current
     *   value.  The table is only included when the stack listing was
     *   produced by a call to t3GetStackTrace() with the T3GetStackLocals
     *   flag set; otherwise it's nil.  If the locals were requested, and
     *   the stack level has no local variables, this will be an empty
     *   lookup table.  
     */
    locals_ = nil

    /*
     *   Named arguments.  This is a LookupTable containing the named
     *   arguments passed in from this stack level.  Each element in the
     *   table has a string key (index) giving the name of the argument,
     *   and each corresponding value is the value of that argument.  If
     *   there are no named arguments, this value is nil.  
     */
    namedArgs_ = nil

    /*
     *   The source location of the next code to be executed in the
     *   function or method in this frame.  If source-level debugging
     *   information is available for the current execution point in this
     *   frame, this will contain a list of two values:
     *   
     *   srcInfo_[1] = string giving the name of the source file
     *.  srcInfo_[2] = integer giving the line number in the source file
     *   
     *   If the program wasn't compiled with source-level debugging
     *   information, or the current code location in the frame doesn't
     *   have any source information, this will be set to nil.
     *   
     *   Note that the location reflected here is the *return address* in
     *   this frame - that is, the code location that will be executed when
     *   control returns to the frame.  This means that the source location
     *   will frequently appear as the next executable line after the one
     *   that called the next inner frame, because this is where execution
     *   will resume when control returns to the frame.  
     */
    srcInfo_ = nil

    /*
     *   A StackFrameDesc object that can be used to get information from
     *   the frame and change local variables in the frame.  
     */
    frameDesc_ = nil
;

/* export T3StackInfo for use by the system */
export T3StackInfo;


/* ------------------------------------------------------------------------ */
/*
 *   Stream state object for String.specialsToHtml().
 */
class SpecialsToHtmlState: object
    /* 
     *   Reset the state.  This should be used when the output stream
     *   context is reset, such as when clearing the window.  
     */
    resetState()
    {
        flags_ = 0;
        tag_ = '';
    }

    /* 
     *   Explicitly reset to the start of a line.  This can be called after
     *   a non-output operation that resets the line position, such as
     *   reading an input line.  
     */
    resetLine()
    {
        /* reset the in-line flag, space flag, qspace flag, and tab column */
        flags_ &= ~(0x0001 | 0x0040 | 0x0080 | 0x0300);
    }

    /* 
     *   Internal output state flags at end of last string parsed.  This is
     *   a combination of bit flags:
     *   
     *   0x0001 - last string ended within a line of text
     *.  0x0002 - caps flag '\^' pending
     *.  0x0004 - lowercase flag '\v' pending
     *.  0x0008 - last string ended within an HTML tag
     *.  0x0010 - last string ended in double-quoted HTML tag attribute text
     *.  0x0020 - last string ended in single-quoted HTML tag attribute text
     *.  0x0040 - last string ended with an ordinary space
     *.  0x0080 - last string ended with a quoted space '\ '
     *.  0x0100 - <Q> parity level: 0=double quotes, 1=single quotes
     *.  0x0300 - distance from last '\t' tab column (0..3)
     */
    flags_ = 0

    /* tag in progress at end of last string parsed */
    tag_ = ''
;
    


/* ------------------------------------------------------------------------ */
/*
 *   global data object for this module
 */
mainGlobal: object
    /* flag: we've run pre-initialization */
    preinited_ = nil

    /* 
     *   The global reflection object - if the program is compiled with
     *   the standard reflection module, that module will set this
     *   property to point to the reflection object.
     *   
     *   We use this so that we don't require the reflection module to be
     *   included.  If the module isn't included, this will be nil, so
     *   we'll know not to use reflection.  If this is not nil, we'll know
     *   we can use reflection services.  
     */
    reflectionObj = nil

    /*
     *   Restart ID.  This is an integer that indicates how the main
     *   entrypoint was last reached.  This is initially zero; each time
     *   we restart the game, this is incremented.
     *   
     *   The restart ID is the only information that survives across a
     *   restart boundary.  Other than this, entering via a restart is
     *   exactly like loading the program from scratch; all other
     *   information about the program state before the restart is lost in
     *   the restart operation.  
     */
    restartID = 0

    /* pointer to mainRestore function, if defined */
    mainRestoreFunc = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   At-exit handlers.  This is a registry for custom handlers that are to
 *   be invoked just before the program terminates.  
 */
transient mainAtExit: object
    /*
     *   Add an at-exit handler.  User code can call this to register a
     *   handler that will be invoked just before the program exits. 
     */
    addHandler(func)
    {
        handlers.append(func);
    }

    /* call our exit handlers */
    callHandlers()
    {
        /* call each handler in the list */
        foreach (local f in handlers)
        {
            try
            {
                /* call this handler */
                f();
            }
            catch (Exception exc)
            {
                /* display and ignore any exceptions it throws */
                "\nError in exit handler: <<exc.displayException()>>\n";
            }
        }
    }

    /* list of exit handlers */
    handlers = static new Vector()
;

/* ------------------------------------------------------------------------ */
/*
 *   <<one of>> index generator.  The compiler generates an anonymous
 *   instance of this class for each <<one of>> list in string, setting the
 *   property 'numItems' to the number of items in the list, and
 *   'listAttrs' property to a string giving the sequence type.  The
 *   compiler generates a call to getNextIndex() within the string to get
 *   the next index value, which is an integer from 1 to numItems.  
 */
class OneOfIndexGen: object
    /* number of list items */
    numItems = 1

    /* 
     *   List attributes.  This is a string with a comma-delimited list of
     *   tokens describing the treatment on the list for each fetch.  The
     *   first call to getNextIndex() takes the first token off the list
     *   and generates an appropriate return value, possibly queuing up a
     *   list of future values.  The next call to getNextIndex() reads from
     *   the queue.  Once the queue is exhausted, the next call takes the
     *   second token off the attribute list and repeats the process.  Once
     *   the attribute list is down to one token, we don't remove the
     *   token, but simply repeat it forever.
     *   
     *   For example, 'seq,rand' runs through the entire list in sequence
     *   once, then generates independently random values from then on.
     *   'shuffle,stop' runs through the list once in shuffled order, then
     *   repeats the last pick from the shuffled list forever.
     *   
     *   The individual attribute values are:
     *   
     *   rand - pick an item at random, independently of past selections.
     *   
     *   rand-norpt - pick an item at random, but don't pick the single
     *   most recent item chosen, to avoid repeating the same thing twice
     *   in a row.
     *   
     *   rand-wt - pick an item by random, weighting the items with
     *   decreasing probabilities.  The last item is given relative weight
     *   1, the second to last weight 2, the third to last weight 3, etc.
     *   In other words, the nth item from the end of the list is n times
     *   as likely to be picked as the last item.  The picks are
     *   independent.
     *   
     *   seq - run through the items in sequence (1, 2, ... numItems).
     *   
     *   shuffle - run through the list in a shuffled order.
     *   
     *   shuffle2 - shuffle the list into a random order, but only run
     *   through half before reshuffling
     *   
     *   stop - repeat the previous selection forever.  (This should only
     *   be used as the second or later attribute in the list, since it
     *   depends on a prior selection being made.)  
     */
    listAttrs = ''

    /* 
     *   Get the next index value.  Returns an integer from 1 to numItems.
     *   The algorithm for choosing the index depends on the list type, as
     *   defined by listAttrs.  
     */
    getNextIndex()
    {
        /* if we're out of items, generate the next iteration of the list */
        if (idx_ > lst_.length())
        {
            /* pull out the first item from the attributes */
            local alst = listAttrs.split(',', 2);

            /* 
             *   If we have more than one attribute, keep only the
             *   remaining attributes, since the first attribute is for the
             *   first list iteration only. 
             */
            if (alst.length() > 1)
                listAttrs = alst[2];

            /* some of the attributes are interested in the last item */
            local last = lst_.length() >= 1 ? lst_[lst_.length()] : nil;

            /* build the list for the head attribute */
            local pick;
            switch (alst[1])
            {
            case 'stop':
                /* reuse the last item every time from now on */
                lst_ = [last ?? 1];
                break;

            case 'seq':
                /* run through the list in sequence */
                lst_ = List.generate({i: i}, numItems);
                break;
                
            case 'rand':
                /* 
                 *   pick an item at random, independent of other picks -
                 *   just generate a single item list, since we'll want to
                 *   pick at random again next time 
                 */
                lst_ = [rand(numItems) + 1];
                break;

            case 'rand-norpt':
                /* 
                 *   Pick an item at random, but don't repeat the last item
                 *   chosen.  Pick repeatedly until we select an item that
                 *   doesn't match the last selection, if there is one.
                 *   Exception: if the list has only one item, it's
                 *   impossible to choose a different item, so don't try.  
                 */
                do
                {
                    pick = rand(numItems) + 1;
                }
                while (numItems > 1 && pick == last);
                lst_ = [pick];
                break;

            case 'rand-wt':
                /* 
                 *   Pick an item at random, independently of other trials,
                 *   weighting the list in decreasing order of probability.
                 *   The relative weights are 1 for the last item, 2 for
                 *   the second to last item, 3 for the third to last, etc.
                 *   This gives us a total weight of n(n+1)/2, so start by
                 *   picking a number in that range.  1 represents the last
                 *   item, 2..3 the second to last, 3..5 the third, etc.  
                 */
                pick = rand(numItems*(numItems+1)/2) + 1;
                for (local i in 1..numItems ; ; pick -= i)
                {
                    /* if pick is <= i, it's this item */
                    if (pick <= i)
                    {
                        lst_ = [numItems - i + 1];
                        break;
                    }
                }
                break;

            case 'shuffle':
            case 'shuffle2':
                /* 
                 *   Shuffle the list.  Populate the list with the integer
                 *   values from 1 to numItems, then shuffle the list into
                 *   a random order.
                 *   
                 *   The shuffling algorithm, as a physical analogy: Start
                 *   with a deck of cards labeled 1 to numItems.  Put all
                 *   of the cards in a hat.  Pick a card at random from the
                 *   hat and set it aside - call this the "pile".  Pick
                 *   another random card from the hat and put it on the top
                 *   of the pile.  Repeat until the hat is empty.  We now
                 *   have a pile of the cards arranged in random order.
                 *   
                 *   To implement this, we create a vector and fill it with
                 *   integers from 1 to numItems.  Partition the vector
                 *   with an index 'i': items from 1..i are the hat, items
                 *   from i+1..numItems are the deck.  To draw a card at
                 *   random, we pick a random number from 1..i.  To remove
                 *   it from the hat, we swap it into position 'i', because
                 *   that will effectively shrink the hat by one slot and
                 *   grow the pile by one slot.  Decrement i and repeat.  
                 */
                lst_ = Vector.generate({i: i}, numItems);
                for (local i in numItems..2 step -1)
                {
                    /* pick a random element from the remaining set */
                    pick = rand(i) + 1;
                    
                    /* swap the chosen item and the last element */
                    local tmp = lst_[i];
                    lst_[i] = lst_[pick];
                    lst_[pick] = tmp;
                }

                /* 
                 *   The whole point of shuffling is to avoid obvious
                 *   repetition, so make sure we don't repeat any previous
                 *   item from a previous run through the list as the first
                 *   item of the new list.  If the first item matches the
                 *   previous item, reshuffle it into a random spot in the
                 *   list.  (If the list only has one item, of course, this
                 *   is pointless, so don't bother in that case.)  
                 */
                if (lst_[1] == last && numItems > 1)
                {
                    pick = rand(numItems - 1) + 2;
                    lst_[1] = lst_[pick];
                    lst_[pick] = last;
                }

                /* 
                 *   in half-shuffle mode, only keep the first half of the
                 *   list, so that we re-shuffle halfway through 
                 */
                if (alst[1] == 'shuffle2' && numItems > 2)
                    lst_ = lst_.toList(1, numItems/2);
                break;
            }

            /* start from the first item in the list */
            idx_ = 1;
        }

        /* return and consume the next item */
        return lst_[idx_++];
    }

    /* generated list */
    lst_ = []

    /* current position in the list */
    idx_ = 1
;
