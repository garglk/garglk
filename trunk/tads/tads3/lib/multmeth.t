#charset "us-ascii"

/*
 *   Copyright (c) 2008 Michael J. Roberts.  All Rights Reserved.
 *   
 *   This module provides the run-time component of "multi-methods" in TADS
 *   3.  This works with the compiler to implement a multiple-dispatch
 *   system.
 *   
 *   Multi-methods are essentially a combination of regular object methods
 *   and "overloaded functions" in languages like C++.  Like a regular
 *   object method, multi-methods are polymorphic: you can define several
 *   incarnations of the same function name, with different parameter
 *   types, the system picks the right binding for each invocation
 *   dynamically, based on the actual argument values at run-time.  Unlike
 *   regular methods, though, the selection is made on ALL of the argument
 *   types, not just a special "self" argument.  In that respect,
 *   multi-methods are like overloaded functions in C++; but multi-methods
 *   differ from C++ overloading in that the selection of which method to
 *   call is made dynamically at run-time, not at compile time.
 *   
 *   There are two main uses for multi-methods.
 *   
 *   First, most obviously, multi-methods provide what's known as "multiple
 *   dispatch" semantics.  There are some situations (actually, quite a
 *   few) where the ordinary Object Oriented notion of polymorphism -
 *   selecting a method based on a single target object - doesn't quite do
 *   the trick, because what you really want to do is select a particular
 *   method based on the *combination* of objects involved in an operation.
 *   Some canonical examples are calculating intersections of shapes in a
 *   graphics program, where you want to select a specialized "Rectangle +
 *   Circle" routine in one case and a "Line + Polygon" routine in another;
 *   or performing file format conversions, where you want to select, say,
 *   a specialized "JPEG to PNG" routine.  In an IF context, the obvious
 *   use is for carrying out multi-object verbs, where you might want a
 *   special routine for PUT (liquid) IN (vessel), and another for PUT
 *   (object) IN (container).
 *   
 *   Second, multi-methods offer a way of extending a class without having
 *   to change the class's source code.  Since a multi-method is defined
 *   externally to any classes it refers to, you can create a method that's
 *   polymorphic on class type - just like a regular method - but as a
 *   syntactically stand-alone function.  This feature isn't as important
 *   in TADS as in some other languages, since TADS lets you do essentially
 *   the same thing with the "modify" syntax; but for some purposes the
 *   multi-method approach might be preferable aesthetically, since it's
 *   wholly external to the class rather than a sort of lexically separate
 *   continuation of the class's code.  (However, as a practical matter,
 *   it's not all that different; our implementation of multi-methods does
 *   in fact modify the original class object, since we store the binding
 *   information in the class objects.)  
 */

#include <tads.h>


/* ------------------------------------------------------------------------ */
/* 
 *   Invoke a multi-method function.  For an expression of the form
 *   
 *.     f(a, b, ...)
 *   
 *   where 'f' has been declared as a multi-method, the compiler will
 *   actually generate code that invokes this function, like so:
 *   
 *.     _multiMethodCall(baseFunc, params);
 *   
 *   'baseFunc' is a function pointer giving the base function; this is a
 *   pointer to the common stub function that the compiler generates to
 *   identify all of the multi-methods with a given name.  'params' is a
 *   list giving the actual parameter values for invoking the function.
 *   
 *   Our job is to find the actual run-time binding for the function given
 *   the actual parameters, and invoke it.  
 */
_multiMethodCall(baseFunc, args)
{
    /* get the function binding lookup information */
    local info = _multiMethodRegistry.boundFuncTab_[baseFunc];

    /* it's an error if there's no binding */
    if (info == nil)
        throw new UnboundMultiMethod(baseFunc, args);

    /* 
     *   Look up the function binding based on the arguments.  To ensure
     *   that we match a function with the correct number of argument, we
     *   have to explicitly add the last-argument placeholder to the list. 
     */
    local func = _multiMethodSelect(info, args + _multiMethodEndOfList);

    /* if we found a binding, invoke it; otherwise throw an error */
    if (func == nil)
        throw new UnboundMultiMethod(baseFunc, args);
    else
        return (func)(args...);
}

/*
 *   Invoke the base multi-method inherited from the given multi-method.
 *   'fromFunc' is a pointer to a multi-method, presumably the one
 *   currently running; we look up the next in line in inheritance order
 *   and invoke it with the given argument list.  
 */
_multiMethodCallInherited(fromFunc, [args])
{
#ifdef MULTMETH_STATIC_INHERITED

    /* static mode - get the cached inheritance information */
    local inh = _multiMethodRegistry.inhTab_[fromFunc];

#else

    /* dynamic mode - get the base function binding */
    local info = _multiMethodRegistry.boundFuncTab_[
        _multiMethodRegistry.baseFuncTab_[fromFunc]];

    /* it's an error if it doesn't exist */
    if (info == nil)
        throw new UnboundInheritedMultiMethod(fromFunc, args);

    /* look up the inherited function based on the actual parameters */
    local inh = _multiMethodInherit(
        fromFunc, info, args + _multiMethodEndOfList);

#endif
    
    /* it's an error if there's no inherited binding */
    if (inh == nil)
        throw new UnboundInheritedMultiMethod(fromFunc, args);

    /* call it */
    return (inh)(args...);
}


/* ------------------------------------------------------------------------ */
/*
 *   Get a pointer to a resolved multi-method function.  This takes a
 *   pointer to the base function for the multi-method and a list of actual
 *   argument values, and returns a function pointer to the specific
 *   version of the multi-method that would be invoked if you called the
 *   multi-method with that argument list.
 *   
 *   For example, if you want to get a pointer to the function that would
 *   be called if you were to call foo(x, y, z), you'd use:
 *   
 *.     local func = getMultiMethodPointer(foo, x, y, z);
 *   
 *   We return a pointer to the individual multi-method function that
 *   matches the argument list, or nil if there's no matching multi-method.
 */
getMultiMethodPointer(baseFunc, [args])
{
    /* get the function binding lookup information */
    local info = _multiMethodRegistry.boundFuncTab_[baseFunc];
    
    /* if there's no binding information, return failure */
    if (info == nil)
        return nil;

    /* look up and return the function binding based on the arguments */
    return _multiMethodSelect(info, args + _multiMethodEndOfList);
}


/* ------------------------------------------------------------------------ */
/*
 *   Resolve a multi-method binding.  This function takes a binding
 *   property ID (the property we assign during the registration process to
 *   generate the binding tables) and a "remaining" argument list.  This
 *   function invokes itself recursively to traverse the arguments from
 *   left to right, so at each recursive invocation, we lop off the
 *   leftmost argument (the one we're working on currently) and pass in the
 *   remaining arguments in the list.
 *   
 *   We look up the binding property on the first argument in the remaining
 *   argument list.  This can yield one of three things:
 *   
 *   - The trivial result is nil, which means that this binding property
 *   has no definition on the first argument.  This doesn't necessarily
 *   mean that the whole function is undefined on the arguments; it only
 *   means that the current inheritance level we're looking at for the
 *   previous argument(s) has no binding.  If we get this result we simply
 *   return nil to tell the caller that it must look at an inherited
 *   binding for the previous argument.
 *   
 *   - If the result is a function pointer, it's the bound function.  This
 *   is the final result for the recursion, so we simply return it.
 *   
 *   - Otherwise, the result will be a new property ID, giving the property
 *   that resolves the binding for the *next* argument.  In this case, we
 *   use this property to resolve the next argument in the list by a
 *   recursive invocation.  If that recursive call succeeds (i.e., returns
 *   a non-nil value), we're done - we simply return the recursive result
 *   as though it were our own.  If it fails, it means that there's no
 *   binding for the particular subclass we're currently working on for the
 *   first argument - however, there could still be a binding for a parent
 *   class of the first argument.  So, we iterate up to any inherited
 *   binding for the first argument, and if we find one, we try again with
 *   the same recursive call.  We continue up our first argument's class
 *   tree until we either find a binding (in which case we return it) or
 *   exhaust the class tree (in which case we return nil).  
 */
_multiMethodSelect(prop, args)
{
    local obj, binding;

    /* 
     *   Get the first argument from the remaining arguments.  If it's not
     *   an object, use the placeholder object for non-object parameter
     *   bindings.  
     */
    local orig = args[1];
    if (dataType(orig) not in (TypeObject, TypeList, TypeSString))
        orig = _multiMethodNonObjectBindings;

    /* get the remaining arguments */
    args = args.sublist(2);

    /* 
     *   Look up the initial binding - this is simply the value of the
     *   binding property for the first argument.  In order to process the
     *   inheritance tree later, we'll need to know where we got this
     *   definition from, so look up the specific defining object.
     *   
     *   If the initial binding property isn't defined, or its value is
     *   explicitly nil, the function isn't bound (or, in the case of nil,
     *   is explicitly unbound).  Inheritance won't help in these cases, so
     *   we can immediately return nil to indicate that we don't have a
     *   binding.  
     */
    if ((obj = orig.propDefined(prop, PropDefGetClass)) == nil
        || (binding = obj.(prop)) == nil)
        return nil;

    /* 
     *   If there are no more arguments, but we didn't just find a final
     *   function binding, we don't have enough arguments to match the
     *   current multi-method path.  Return failure.  
     */
    if (args.length() == 0 && dataType(binding) != TypeFuncPtr)
        return nil;

    /* 
     *   starting at our current defining object for the first argument,
     *   scan up its superclass tree until we find a binding 
     */
    for (;;)
    {
        local ret;

        /* if we have a function pointer, we've found our binding */
        if (dataType(binding) == TypeFuncPtr)
            return binding;

        /* 
         *   Recursively bind the binding for the remaining arguments.  If
         *   we find a binding, we're done - simply return it. 
         */
        if ((ret = _multiMethodSelect(binding, args)) != nil)
            return ret;

        /* 
         *   We didn't find a binding for the remaining arguments, so we
         *   must have chosen too specific a binding for the first
         *   argument.  Look for an inherited value of the binding property
         *   in the next superclass of the object where we found the last
         *   binding value.  
         */
        obj = orig.propInherited(prop, orig, obj, PropDefGetClass);
        if (obj == nil)
            return nil;

        /* we found an inherited value, so retrieve it from the superclass */
        binding = obj.(prop);
    }
}

/*
 *   Select the INHERITED version of a multi-method.  This takes a
 *   particular version of the multi-method, and finds the next version in
 *   inheritance order.
 *   
 *   This is basically a copy of _multiMethodSelect(), with a small amount
 *   of extra logic.  This code repetition isn't good maintenance-wise, and
 *   the two functions could in principle be merged into one.  However,
 *   doing so would have an efficiency cost to _multiMethodSelect(), which
 *   we want to keep as lean as possible.  
 */
_multiMethodInherit(fromFunc, prop, args)
{
    return _multiMethodInheritMain(
        new _MultiMethodInheritCtx(), fromFunc, prop, args);
}

class _MultiMethodInheritCtx: object
    foundFromFunc = nil
;
    
_multiMethodInheritMain(ctx, fromFunc, prop, args)
{
    local obj, binding;

    /* 
     *   Get the first argument from the remaining arguments.  If it's not
     *   an object, use the placeholder object for non-object parameter
     *   bindings.  
     */
    local orig = args[1];
    if (dataType(orig) not in (TypeObject, TypeList, TypeSString))
        orig = _multiMethodNonObjectBindings;

    /* get the remaining arguments */
    args = args.sublist(2);

    /* 
     *   Look up the initial binding - this is simply the value of the
     *   binding property for the first argument.  In order to process the
     *   inheritance tree later, we'll need to know where we got this
     *   definition from, so look up the specific defining object.
     *   
     *   If the initial binding property isn't defined, or its value is
     *   explicitly nil, the function isn't bound (or, in the case of nil,
     *   is explicitly unbound).  Inheritance won't help in these cases, so
     *   we can immediately return nil to indicate that we don't have a
     *   binding.  
     */
    if ((obj = orig.propDefined(prop, PropDefGetClass)) == nil
        || (binding = obj.(prop)) == nil)
        return nil;

    /* 
     *   If there are no more arguments, but we didn't just find a final
     *   function binding, we don't have enough arguments to match the
     *   current multi-method path.  Return failure.  
     */
    if (args.length() == 0 && dataType(binding) != TypeFuncPtr)
        return nil;

    /* 
     *   starting at our current defining object for the first argument,
     *   scan up its superclass tree until we find a binding 
     */
    for (;;)
    {
        /* we haven't found a function binding yet */
        local ret = nil;

        /* 
         *   we either have a function pointer, in which case it's the
         *   actual binding, or a property, in which case it's the next
         *   binding level 
         */
        if (dataType(binding) == TypeFuncPtr)
        {
            /* this is the binding */
            ret = binding;
        }
        else
        {
            /* if there are no more arguments, return failure */
            if (args.length() == 0)
                return nil;

            /* 
             *   Recursively bind the binding for the remaining arguments.
             *   If we find a binding, we're done - simply return it.  
             */
            ret = _multiMethodInheritMain(ctx, fromFunc, binding, args);
        }

        /* check to see if we found a binding */
        if (ret != nil)
        {
            /* 
             *   We found a binding.  If we've already found the inheriting
             *   version, return the first thing we find, since that's the
             *   next inheriting level.  Otherwise, if this is the
             *   inheriting version, note that we've found it, but keep
             *   looking, since we want to find the next one after that.
             *   Otherwise, just keep looking, since we haven't even
             *   reached the overriding version yet. 
             */
            if (ctx.foundFromFunc)
                return ret;
            else if (ret == fromFunc)
                ctx.foundFromFunc = true;
        }
        
        /* 
         *   We didn't find a binding for the remaining arguments, so we
         *   must have chosen too specific a binding for the first
         *   argument.  Look for an inherited value of the binding property
         *   in the next superclass of the object where we found the last
         *   binding value.  
         */
        obj = orig.propInherited(prop, orig, obj, PropDefGetClass);
        if (obj == nil)
            return nil;

        /* we found an inherited value, so retrieve it from the superclass */
        binding = obj.(prop);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Unbound multi-method exception.  This is thrown when a call to resolve
 *   a multi-method fails to find a binding, meaning that there's no
 *   definition of the method that matches the types of the arguments.  
 */
class UnboundMultiMethod: Exception
    construct(func, args)
    {
        /* note the function name and argument list */
        func_ = func;
        args_ = args;

        /* look up the function's name */
        name_ = _multiMethodRegistry.funcNameTab_[func];
    }

    /* display an error message describing the exception */
    displayException()
    {
        "Unbound multi-method \"<<name_>>\" (<<args_.length()>> argument(s))";
    }

    /* the base function pointer */
    func_ = nil

    /* the symbol name of the base function */
    name_ = ''

    /* the number of arguments */
    args_ = 0
;

class UnboundInheritedMultiMethod: UnboundMultiMethod
    displayException()
    {
        "No inherited multi-method for \"<<name_>>\" (<<args_.length()>>
        arguments(s))";
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Base class for our internal placeholder objects for argument list
 *   matching.
 */
class _MultiMethodPlaceholder: object
;

/*
 *   A placeholder object for bindings for non-object arguments.  Whenever
 *   we have an actual argument value that's not an object, we'll look here
 *   for bindings for that parameter.  When registering a function, we'll
 *   register a binding here for any parameter that doesn't have a type
 *   specification.  
 */
_multiMethodNonObjectBindings: _MultiMethodPlaceholder
;

/*
 *   A placeholder object for end-of-list bindings.  When we're matching an
 *   argument list, we'll use this to represent the end of the list so that
 *   we can match the "..." in any varargs functions in the multi-method
 *   set that we're matching against.  
 */
_multiMethodEndOfList: _MultiMethodPlaceholder
;


/* ------------------------------------------------------------------------ */
/*
 *   Register a multi-method.
 *   
 *   The compiler automatically generates a call to this function during
 *   pre-initialization for each defined multi-method.  'baseFunc' is a
 *   pointer to the "base" function - this is a stub function that the
 *   compiler generates to refer to the whole collection of multi-methods
 *   with a given name.  'func' is the pointer to the specific multi-method
 *   we're registering; this is the actual function defined in the code
 *   with a given set of parameter types.  'params' is a list of the
 *   parameter type values; each parameter type in the list is given as a
 *   class object (meaning that the parameter matches that class), nil
 *   (meaning that the parameter matches ANY type of value), or the string
 *   '...' (meaning that this is a "varargs" function, and any number of
 *   additional parameters can be supplied at this point in the parameters;
 *   this is always the last parameter in the list if it's present).  
 */
_multiMethodRegister(baseFunc, func, params)
{
    /* if there's no hash entry for the function yet, add one */
    local tab = _multiMethodRegistry.funcTab_;
    if (tab[baseFunc] == nil)
        tab[baseFunc] = new Vector(10);

    /* add the entry to the list of variants for this function */
    tab[baseFunc].append([func, params]);

    /* add the mapping from the function to the base function */
    _multiMethodRegistry.baseFuncTab_[func] = baseFunc;

    /* also add the function to the direct parameter table */
    _multiMethodRegistry.funcParamTab_[func] = params;
}

/*
 *   Build the method bindings.  The compiler generates a call to this
 *   after all methods have been registered; we run through the list of
 *   registered methods and generate the binding properties in the
 *   referenced objects.  
 */
_multiMethodBuildBindings()
{
    /* no errors yet */
    local errs = [];
    
    /* 
     *   build a lookup table that maps function pointers to symbol names,
     *   so we can look up our function names for diagnostic purposes 
     */
    local nameTab = new LookupTable(128, 256);
    t3GetGlobalSymbols().forEachAssoc(function(key, val)
    {
        /* if it's a function, store a value-to-name association */
        if (dataType(val) == TypeFuncPtr)
            nameTab[val] = key;
    });

    /* run through each entry in the method table */
    _multiMethodRegistry.funcTab_.forEachAssoc(function(baseFunc, val)
    {
        /* look up the base function's name */
        local name = nameTab[baseFunc];

        /* add this to the saved name table */
        _multiMethodRegistry.funcNameTab_[baseFunc] = name;

        /* note the number of registered instances of this function */
        local funcCnt = val.length();

        /* 
         *   Assign the initial binding property for this function.  This
         *   is the property that gives us the binding for the first
         *   variant argument.  Each unique multi-method (which is defined
         *   as a multi-method with a given name and a given number of
         *   parameters) has a single initial binding property.  
         */
        local initProp = t3AllocProp();

        /* 
         *   store the binding starter information for the function - to
         *   find the binding on invocation, we'll need the initial binding
         *   property so that we can trace the argument list 
         */
        _multiMethodRegistry.boundFuncTab_[baseFunc] = initProp;

        /* build the argument binding tables */
        for (local i = 1 ; i <= funcCnt ; i++)
        {
            /* get the function binding */
            local func = val[i][1];

            /* get the formal parameter type list for this function */
            local params = val[i][2];
            local paramCnt = params.length();

            /*
             *   If the last formal isn't a varargs placeholder, then we
             *   must explicitly find the end of the list in the actual
             *   parameters in order to match a call.  To match the end of
             *   the list, add the special End-Of-List placeholder to the
             *   formals list.
             *   
             *   This isn't necessary when there's a varargs placeholder
             *   because the placeholder can match zero or more - so it
             *   doesn't matter where the list ends as long as we get to
             *   the varargs slot.  
             */
            if (paramCnt == 0 || params[paramCnt] != '...')
            {
                params += _multiMethodEndOfList;
                ++paramCnt;
            }

            /* start at the initial binding property */
            local prop = initProp;

            /* run through the parameters and build the bindings */
            for (local j = 1 ; j <= paramCnt ; j++)
            {
                /* get this parameter type */
                local origTyp = params[j], typ = origTyp;

                /* 
                 *   If the type is nil, it means that this parameter slot
                 *   can accept any type.  So, map the slot to the generic
                 *   Object type - this will catch everything, since we
                 *   handle non-objects by mapping them to the
                 *   _multiMethodNonObjectBindings placeholder object,
                 *   which like all objects inherits from Object.  This
                 *   means we'll match argument values that are objects or
                 *   non-objects, thus fulfilling our requirement to match
                 *   all values.
                 *   
                 *   If the type is the string '...', it means that this is
                 *   a varargs placeholder argument.  In this case, we need
                 *   to set up a match for the generic Object, in case we
                 *   have one or more actual arguments for the varargs
                 *   portion.  This will also automatically match the case
                 *   where we have no extra arguments, because in this case
                 *   the matcher will try to match the End-Of-List
                 *   placeholder object _multiMethodEndOfList, which (as
                 *   above) inherits from Object and thus picks up the
                 *   any-type binding.
                 *   
                 *   The one tricky bit is that when we have a parameter
                 *   explicitly bound to Object, or an explicit End-Of-List
                 *   flag object, we'll get an undesired side effect of
                 *   this otherwise convenient arrangement: we'll
                 *   effectively bind non-object types to the Object by
                 *   virtue of the inheritance.  To deal with this, we'll
                 *   explicitly set the placeholders' binding to nil in
                 *   this situation - this makes non-object types
                 *   explicitly *not* bound to the function, overriding any
                 *   binding we'd otherwise inherit from Object.  
                 */
                if (typ == nil || typ == '...')
                    typ = Object;

                /* 
                 *   Figure the binding.
                 *   
                 *   - If this is the last parameter, it's the end of the
                 *   line, so bind directly to the function pointer.
                 *   
                 *   - If this isn't the variant parameter, the binding is
                 *   the next binding property.  At invocation, we'll
                 *   continue on to the next argument value, evaluating
                 *   this next property to get its binding in the context
                 *   established by the current argument and property.  The
                 *   next binding property is specific to the current class
                 *   in the current position, so we might already have
                 *   assigned a property for it from another version of
                 *   this function.  Look it up, or create a new one if we
                 *   haven't assigned it already.  
                 */
                local binding;
                if (j == paramCnt)
                {
                    /* end of the line - the binding is the actual function */
                    binding = func;

                    /* 
                     *   if this type is already bound to a different
                     *   definition for this function, we have a
                     *   conflicting definition 
                     */
                    if (typ.propDefined(prop, PropDefDirectly)
                        && typ.(prop) != binding)
                        errs += [baseFunc, func, params, name];
                }
                else
                {
                    /* check for an existing binding property here */
                    if (typ.propDefined(prop, PropDefDirectly))
                    {
                        /* we already have a forward binding here - use it */
                        binding = typ.(prop);
                    }
                    else
                    {
                        /* it's not defined here, so create a new property */
                        binding = t3AllocProp();
                    }
                }

                /* set the binding */
                typ.(prop) = binding;
                
                /*
                 *   As we mentioned above, if the original type is
                 *   explicitly Object, we *don't* want to allow non-object
                 *   types (int, true, nil, property pointers, etc) and
                 *   End-Of-List placeholders to match - without some kind
                 *   of explicit intervention here, the placeholders would
                 *   match by inheritance because the placeholders are just
                 *   objects themselves.  To handle this properly, set the
                 *   non-object placeholder bindings explicitly to nil.  
                 */
                if (origTyp == Object)
                    _MultiMethodPlaceholder.(prop) = nil;

                /* 
                 *   if there's another argument, this binding is the
                 *   binding property for the next argument 
                 */
                prop = binding;
            }
        }
    });

#ifdef MULTMETH_STATIC_INHERITED
    /*
     *   If we're operating in static inheritance mode, cache the
     *   next-override information for inherited() calls.  Since we're
     *   using static inheritance, we can figure this at startup and just
     *   look up the cached information whenever we need to perform an
     *   inherited() call.  
     */
    _multiMethodRegistry.funcTab_.forEachAssoc(function(baseFunc, val)
    {
        /* get the binding property for the base function */
        local prop = _multiMethodRegistry.boundFuncTab_[baseFunc];

        /* run through the functions registered under this function name */
        for (local i = 1 ; i <= val.length() ; i++)
        {
            /* get this function binding and the type list */
            local func = val[i][1];
            local params = val[i][2];
            local paramCnt = params.length();

            /* 
             *   Add the end-of-list marker if applicable.  For a varargs
             *   function, add one generic Object parameter in place of the
             *   variable list - but we'll check later to make sure that
             *   any match we find is really varargs, since a varargs
             *   function can only inherit from another varargs function.
             *   (This is because, in order to actually invoke the
             *   inherited function from an overrider, the callee must be
             *   varargs to be able to handle varargs from the caller.)  
             */
            local varargs = (paramCnt != 0 && params[paramCnt] == '...');
            if (varargs)
                params[paramCnt] = Object;
            else
                params += _multiMethodEndOfList;

            /* look up the inherited version of the method */
            local inh = _multiMethodInherit(func, prop, params);

            /* varargs can only inherit from varargs */
            if (inh != nil && varargs)
            {
                /* look up the inherited parameters */
                local inhParams = _multiMethodRegistry.funcParamTab_[inh];
                local inhParamCnt = inhParams.length();

                /* make sure it's varargs, too */
                if (inhParamCnt == 0 || inhParams[inhParamCnt] != '...')
                    inh = nil;
            }

            /* remember the inherited function */
            _multiMethodRegistry.inhTab_[func] = inh;
        }
    });
#endif /* MULTMETH_STATIC_INHERITED */
    
    /* we're done with the source bindings - discard them to save memory */
    _multiMethodRegistry.funcTab_ = nil;
    _multiMethodRegistry.funcParamTab_ = nil;
}

/*
 *   Multi-method registry.  This is where we keep the registry information
 *   that we build during initialization.  
 */
_multiMethodRegistry: object
    /* table of registered functions, indexed by base function */
    funcTab_ = static new LookupTable(128, 256)

    /* table of function parameter lists, indexed by function */
    funcParamTab_ = static new LookupTable(128, 256)

    /* function name table */
    funcNameTab_ = static new LookupTable(64, 128)

    /* base function -> initial binding property */
    boundFuncTab_ = static new LookupTable(64, 128)

    /* function -> base function */
    baseFuncTab_ = static new LookupTable(64, 128)

#ifdef MULTMETH_STATIC_INHERITED
    /* table of cached inherited() information, indexed by function */
    inhTab_ = static new LookupTable(64, 128)
#endif
;
