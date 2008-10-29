#if defined( FINCH_DEBUG_COMPILE )
#error entering fi_util.t...
#endif

#if !defined( FINCH_UTILITY_LIBRARY )

#include "tads.h"
#include "t3.h"
#include "lookup.h"

modify TadsObject
    _name = nil

    _init_TadsObject() {}

    construct() {
        _init_();
    }

    _init_( [args] ) {
        if (self == TadsObject) return;

        local obj = (args.length > 0 ? args[1] : self);

        local lst = getSuperclassList();
        foreach (local c in lst)
            c._init_( obj );

        callDefinedMethod( obj, '_init_' + self._name );
    }
;

/**
 *  Permits the Finch utilities by saving the global symbol hashtable
 *  before it gets garbage-collected after preinit.
 */
FinchUtility: PreinitObject
    globalSymbols = nil

    storeGlobalSymbols = true

    execute() {
        globalSymbols = t3GetGlobalSymbols();

        t3DebugTrace( T3DebugBreak );

        globalSymbols.forEach(
            { tok, obj :
                dataType( obj ) == TypeObject &&
                obj.ofKind( TadsObject )
                    ? obj._name = tok
                    : nil
            }
        );

        for (local o = firstObj(); o; o = nextObj( o )) {
            if (o.ofKind( TadsObject)) {
                o._init_();
            }
        }
    }
;

/**
 *  Retrieves the global symbol hashtable, saved by the FinchUtility
 *  PreinitObject.
 */
getGlobalSymbols() {
    return FinchUtility.globalSymbols;
}

/**
 *  Calls the given method-name on the given object using the given
 *  argument list.
 *
 *  @param  obj     an object reference
 *  @param  meth    a string containing a valid property name
 *  @param  params  all following parameters will be passed sequentially
 *                  to the method
 *  @return         whatever the method returned
 *  @exception  NoSuchSymbolException if the method name can't be found in
 *                  the global symbol table
 *  @exception  NoSuchMethodException if the associated entry in the
 *                  global symbol table isn't a property
 *  @exception  InvocationTargetException if the method itself threw an
 *                  exception.
 */
callMethod( obj, meth, [params] ) {

    //  get the actual method reference from the symbol table
    local prop = (getGlobalSymbols())[ meth ];

    //  if the symbol doesn't exist, throw an exception
    if (prop == nil)
        throw new NoSuchSymbolException( meth );

    //  if the symbol isn't a property, throw an exception
    else if (dataType( prop ) != TypeProp)
        throw new NoSuchMethodException( meth );

    else {
        try {
            return obj.(prop)( params... );
        }
        catch (Exception e) {
            throw new InvocationTargetException( obj, meth, e );
        }
    }
}

/**
 *  Calls the given method-name on the given object using the given
 *  argument list.  If the method does not exist, or exists but isn't
 *  defined on this object, ignore it (instead of, say, trying to call
 *  a method that we know won't ever be valid, since code can never be
 *  assigned to a method in the current implementation of the VM).
 *
 *  @param  obj     an object reference
 *  @param  meth    a string containing a valid property name
 *  @param  params  all following parameters will be passed sequentially
 *                  to the method
 *  @return         whatever the method returned
 *  @exception  NoSuchMethodException if the associated entry in the
 *                  global symbol table isn't a property
 *  @exception  InvocationTargetException if the method itself threw an
 *                  exception.
 */
callDefinedMethod( obj, meth, [params] ) {

    // get the actual method reference from the symbol table
    local prop = (getGlobalSymbols())[ meth ];

    // if the symbol doesn't exist, abort.
    if (prop == nil) return nil;

    // if the property isn't defined by this object, abort.
    if (!obj.propDefined( prop )) return nil;

    // if the symbol isn't a property, throw an exception
    else if (dataType( prop ) != TypeProp)
        throw new NoSuchMethodException( meth );

    else {
        try {
            return obj.(prop)( params... );
        }
        catch (Exception e) {
            throw new InvocationTargetException( obj, meth, e );
        }
    }
}

/**
 *  Creates a new property, updating the global symbol table.
 *
 *  @param  str     a string to correspond to the new property
 *  @return         the new property
 */
createGlobalProperty( str ) {

    // allocate a new property id
    local prop = t3AllocProp();

    // assign it to the given string
    (getGlobalSymbols())[ str ] = prop;

    // return the new property
    return prop;
}

/**
 *  A nearly meta-exception thrown when reflectively invoked code
 *  throws an exception.
 */
class InvocationTargetException: Exception
    location = nil
    position = nil
    datum = nil

    construct( obj, meth, e ) {
        location = obj;
        position = meth;
        datum = e;
    }

    getMessage() {
        "InvocationTargetException: the method << position >>
        on the object << location._name >> threw the following
        exception: << datum.getMessage() >>. ";
    }
;

/**
 *  An exception thrown when a reflectively invoked method
 *  does't actually exist.
 */
class NoSuchMethodException: Exception
    position = nil

    construct( meth ) {
        position = meth;
    }

    getMessage() {
        "NoSuchMethodException: the symbol << position >>
        does not correspond to a property address.";
    }
;

/**
 *  An exception thrown when a reflectively requested symbol
 *  does't actually exist.
 */
class NoSuchSymbolException: Exception
    position = nil

    construct( sym ) {
        position = sym;
    }

    getMessage() {
        "NoSuchSymbolException: the symbol << position >>
        does not exist in the global symbols table.";
    }
;

#endif

