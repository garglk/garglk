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
    /* keep going as long as we keep restarting */
    for (;;)
    {
        try
        {
            /* perform load-time initialization */
            initAfterLoad();
            
            /* if we're not in preinit-only mode, run the program */
            if (!t3GetVMPreinitMode())
            {
                /* 
                 *   If there's a saved state file to restore, call our
                 *   mainRestore() function instead of main().  If
                 *   mainRestore() isn't defined, show a message to this
                 *   effect but keep going. 
                 */
                if (restoreFile != nil)
                {
                    /* check for a mainRestore function */
                    if (dataType(mainGlobal.mainRestoreFunc) == TypeFuncPtr)
                    {
                        try
                        {
                            /* 
                             *   Call the user's main startup-and-restore
                             *   entrypoint function.  Note that we call
                             *   indirectly through our function pointer
                             *   so that we don't force a function called
                             *   mainRestore() to be linked with the
                             *   program.  
                             */
                            (mainGlobal.mainRestoreFunc)(args, restoreFile);
                        }
                        finally
                        {
                            /* 
                             *   whatever happens, forget the saved state
                             *   file, since we do not want to restore it
                             *   again after restarting or anything else
                             *   that takes us back through the main
                             *   entrypoint again 
                             */
                            restoreFile = nil;
                        }
                    }
                    else
                    {
                        /* 
                         *   there's no mainRestore, so we can't restore
                         *   the saved state - note the problem 
                         */
                        "\n[This program cannot restore the saved position
                        file automatically.  Please try restoring the
                        saved position file again using a command within
                        the program.]\b";

                        /* call the ordinary main */
                        main(args);
                    }
                }
                else
                {
                    /* call the user's main program entrypoint */
                    main(args);
                }
            }

            /* we're done - break out of the restart loop */
            break;
        }
        catch (RestartSignal rsig)
        {
            /* 
             *   call the intrinsic restartGame function to reset all of
             *   the static objects to their initial state 
             */
            restartGame();

            /* 
             *   Now that we've reset the VM, update the restart ID in the
             *   main globals.  Note that we waited until now to do this,
             *   because this change would have been lost in the reset if
             *   we'd made the change before the reset.  Note also that the
             *   'rsig' object itself will survive the reset because the
             *   thrower presumably allocated it dynamically, hence it's
             *   not a static object subject to reset.  
             */
            mainGlobal.restartID = rsig.restartID;

            /*
             *   Now we can just continue on to the next iteration of the
             *   restart loop.  This will take us back to the
             *   initialization and enter the game as though we'd just
             *   started the program again. 
             */
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
            /* display the unhandled exception */
            "\n<<exc.displayException()>>\n";

            /* we can't go on - break out of the restart loop */
            break;
        }
    }
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
         *   Explicitly run garbage collection prior to preinit.  This is
         *   usually harmless but unnecessary, since it usually doesn't
         *   much matter whether unreachable objects are still sitting in
         *   memory or not; but under certain conditions it's important.
         *   
         *   In particular, object loops (over all objects, or over all
         *   instances of a given class) can still see otherwise
         *   unreachable objects.  It's common to do these kinds of loops
         *   in preinit to set up static data caches and tables and so on.
         *   So, if there were any garbage objects lying around, preinit
         *   might find them and register them into tables or what not.
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
         *   preinit - simply perform an explicit garbage collection cycle
         *   before invoking preinit.  
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
                new function(obj)
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
 *   same as dataType(), except that anonymous function objects are
 *   indicated as ordinary function pointer (TypeFuncPtr).  
 */
dataTypeXlat(val)
{
    local t;

    /* get the base type */
    t = dataType(val);
    
    /* if it's an anonymous function, return TypeFuncPtr */
    if (t == TypeObject && val.ofKind(AnonFuncPtr))
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
    /* display the exception - should always be overridden */
    displayException()
    {
        "Unknown exception";
    }

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
            "\nStack trace:\n";
            for (local i = 1, local cnt = stackList.length() ; i <= cnt ; ++i)
            {
                local cur = stackList[i];
                
                /* show a mark next to level 1, spaces elsewhere */
                if (i == 1)
                    "--&gt;";
                else
                    "\ \ ";

                /* 
                 *   if there's a system reflection object, show symbolic
                 *   information on the current function call; otherwise,
                 *   simply show the source location 
                 */
                if (mainGlobal.reflectionObj != nil)
                {
                    /* reflection is available - show full symbolic info */
                    _tads_io_say(mainGlobal.reflectionObj.
                                 formatStackFrame(cur, true));
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

    /* display the exception */
    displayException()
    {
        /* show the exception message */
        "Runtime error: <<exceptionMessage>>";

        /* show a stack trace if possible */
        showStackTrace(stack_);
    }

    /* check to see if it's a debugger signal of some kind */
    isDebuggerSignal()
    {
        switch(errno_)
        {
        case 2391:                       /* debugger 'abort command' signal */
        case 2392:                             /* debugger 'restart' signal */
            /* it's a debugger signal */
            return true;

        default:
            /* not a debugger signal */
            return nil;
        }
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

/* ------------------------------------------------------------------------ */
/*
 *   Default string display function.  Our main entrypoint code
 *   establishes this function as the default output function.  
 */
_default_display_fn(str) { _tads_io_say(str); }


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
    construct(func, obj, prop, selfObj, argList, srcInfo)
    {
        /* remember the values */
        func_ = func;
        obj_ = obj;
        prop_ = prop;
        self_ = selfObj;
        argList_ = argList;
        srcInfo_ = srcInfo;
    }

    /*
     *   Is this a system routine?  This returns true if an intrinsic
     *   function or an intrinsic class method is running at this level. 
     */
    isSystem()
    {
        /* 
         *   if it's a system routine, we won't have a function OR an
         *   object method 
         */
        return func_ == nil && obj_ == nil;
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

    /* the list of arguments to the function or method */
    argList_ = []

    /*
     *   The source location of the next code to be executed in the
     *   function or method in this frame.  If debugging records are
     *   available for the current execution point in this frame, this
     *   will contain a list of two values:
     *   
     *   srcInfo_[1] = string giving the name of the source file
     *.  srcInfo_[2] = integer giving the line number in the source file
     *   
     *   If the program wasn't compiled with debugging records, or the
     *   current code location in the frame doesn't have any source
     *   information, this will be set to nil.
     *   
     *   Note that this gives the location of the *next* statement to be
     *   executed in this frame, when control returns to the frame.  This
     *   means that the location is frequently the next statement after
     *   the one that called the next inner frame, because this is where
     *   execution will resume when control returns to the frame.  
     */
    srcInfo_ = nil
;

/* export T3StackInfo for use by the system */
export T3StackInfo;


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

