#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.  
 */

#include <tads.h>
#include <strbuf.h>
#include "reflect.h"

/* ------------------------------------------------------------------------ */
/*
 *   Main reflection services object.
 *   
 *   During pre-initialization, we'll plug this into the _main module's
 *   globals so that the _main module will know it can use reflection
 *   services.  
 */
reflectionServices: PreinitObject
    /* execute preinitialization */
    execute()
    {
        /* plug ourselves into the main globals */
        mainGlobal.reflectionObj = self;

        /* store the main symbol table */
        symtab_ = t3GetGlobalSymbols();

        /* create a reverse lookup table from the main symbol table */
        if (symtab_ != nil)
        {
            /* 
             *   create a lookup table for the reverse table - it'll be
             *   the same size as the original table, so create it using
             *   the same statistics 
             */
            reverseSymtab_ = new LookupTable(symtab_.getBucketCount(),
                                             symtab_.getEntryCount());

            /* 
             *   for each entry in the main table, create an entry in the
             *   reverse table with the role of key and value reversed -
             *   this will allow us to look up any value and find its
             *   global symbol, if it has one 
             */
            symtab_.forEachAssoc({key, val: reverseSymtab_[val] = key});
        }
    }

    /*
     *   Convert a value to a symbol, or to a string representation if
     *   it's not of a symbolic type.  
     */
    valToSymbol(val)
    {
        local sym;

        /* the representation depends on the type */
        switch(dataType(val))
        {
        case TypeNil:
            return 'nil';

        case TypeTrue:
            return 'true';

        case TypeInt:
            return toString(val);

        case TypeSString:
        case TypeList:
        case TypeObject:
            /* 
             *   If we're asking about 'self', inherit the handling.  Note
             *   that, for any object type, x.ofKind(x) is always true, so
             *   there's no need for a separate test to see if val equals
             *   self.  
             */
            if (val.ofKind(self))
                return inherited();

            /* check for intrinsic classes */
            if (IntrinsicClass.isIntrinsicClass(val))
            {
                /* intrinsic classes should always be in the symbol table */
                sym = reverseSymtab_[val];
                return (sym != nil ? sym : '{intrinsicClass}');
            }

            /* use our special value-to-symbol method on the object itself */
            return val.valToSymbol();
            
        case TypeProp:
            /* 
             *   this should usually convert to a symbol, but might have
             *   been allocated dynamically 
             */
            sym = reverseSymtab_[val];
            return (sym != nil ? sym : '{prop}');

        case TypeFuncPtr:
            /* 
             *   look for a name; if it doesn't have one, it must be an
             *   anonymous function 
             */
            sym = reverseSymtab_[val];
            return (sym != nil ? sym : '{anonFunc}');
            
        case TypeEnum:
            /* these should always convert directly to symbols */
            sym = reverseSymtab_[val];
            return (sym != nil ? sym : '{enum}');

        case TypeBifPtr:
            /* these should always convert directly to symbols */
            sym = reverseSymtab_[val];
            return (sym != nil ? sym : '{intrinsicFunc}');

        case TypeNativeCode:
            return '{native code}';

        default:
            return '???';
        }
    }

    /*
     *   Format a stack frame object (of class T3StackInfo). 
     */
    formatStackFrame(fr, includeSourcePos)
    {
        local ret = new StringBuffer();
        
        /* see what kind of frame we have */
        if (fr.func_ != nil)
        {
            /* it's a function */
            ret.append(valToSymbol(fr.func_));
        }
        else if (fr.obj_ != nil)
        {
            /* 
             *   It's an object.property.  Check for one special case we
             *   want to show specially: if the object is an AnonFuncPtr
             *   object, ignore the property and just show it as an
             *   anonymous function. 
             */
            if (fr.obj_.ofKind(AnonFuncPtr))
                ret.append('{anonFunc}');
            else
            {
                ret.append(valToSymbol(fr.self_));
                ret.append('.');
                ret.append(valToSymbol(fr.prop_));
            }
        }
        else
        {
            /* no function or object - must be a system routine */
            ret.append('(System)');
        }

        /* if it's not a system routine, add the argument list */
        if (fr.argList_ != nil)
        {
            /* add the open paren */
            ret.append('(');
            
            /* add the arguments */
            local i, len = fr.argList_.length();
            for (i = 1 ; i <= len ; ++i)
            {
                /* if it's not the first one, add a comma */
                if (i != 1)
                    ret.append(', ');

                /* add this value */
                ret.append(valToSymbol(fr.argList_[i]).htmlify());
            }

            /* add any named arguments */
            if (fr.namedArgs_ != nil)
            {
                /* add each key from the named argument table */
                fr.namedArgs_.forEachAssoc(function(key, val)
                {
                    /* add a separator if this isn't the first item */
                    if (i++ != 1)
                        ret.append(', ');

                    /* add this "name: value" */
                    ret.append(key);
                    ret.append(':');
                    ret.append(valToSymbol(val));
                });
            }

            /* add the close paren */
            ret.append(')');

            /* if desired, add the source location */
            if (includeSourcePos && fr.srcInfo_ != nil)
            {
                ret.append(' ');
                ret.append(fr.srcInfo_[1]);
                ret.append(', line ');
                ret.append(fr.srcInfo_[2]);
            }
        }

        /* return the result */
        return toString(ret);
    }

    /* the global symbol table */
    symtab_ = nil

    /* the global reverse-lookup symbol table */
    reverseSymtab_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Export the reflection services interfaces used by the VM
 */
export reflectionServices 'reflection.reflectionServices';
export valToSymbol 'reflection.valToSymbol';

/* ------------------------------------------------------------------------ */
/*
 *   Modify the basic Object class to provide a to-symbol mapping
 */
modify Object
    valToSymbol()
    {
        /* get my symbol from the global reflection table */
        local sym = reflectionServices.reverseSymtab_[self];

        /* if we got a symbol, return it */
        if (sym != nil)
            return sym;

        /* 
         *   We didn't get a symbol, so there's no source file name.  See
         *   if we can find source-file names for the superclasses, though.
         */
        sym = '{obj:';
        local found = nil;
        foreach (local sc in getSuperclassList())
        {
            local scSym;

            /* add a comma to the list if this isn't the first element */
            if (sym != '{obj:')
                sym += ',';
                
            /* if we have a name here, add it to the list */
            if ((scSym = reflectionServices.reverseSymtab_[sc]) != nil)
            {
                /* note that we found a named superclass */
                found = true;

                /* add the superclass name to the list */
                sym += scSym;
            }
            else
            {
                /* we don't have a name for this superclass; say so */
                sym += '{anonymous}';
            }
        }

        /* 
         *   if we found any named superclasses, return the list of names;
         *   otherwise, just say (obj) 
         */
        return (found ? sym + '}' : '{obj}');
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Modify the String intrinsic class to provide a to-symbol mapping 
 */
modify String
    valToSymbol()
    {
        local ret;
        local i;
        local start;
        
        /* start with an open quote */
        ret = '\'';

        /* loop through the string to find each special character */
        for (i = 1, local len = length(), start = 1 ;
             i <= len ; ++i)
        {
            local qu;

            /* presume we won't add a quoted character on this round */
            qu = nil;
            
            /* see what we have here */
            switch(substr(i, 1))
            {
            case '\\':
                qu = '\\\\';
                break;
                
            case '\'':
                qu = '\\\'';
                break;

            case '\n':
                qu = '\\n';
                break;

            case '\t':
                qu = '\\t';
                break;

            case '\b':
                qu = '\\b';
                break;

            case '\ ':
                qu = '\\ ';
                break;

            case '\^':
                qu = '\\^';
                break;

            case '\v':
                qu = '\\v';
                break;
            }

            /* 
             *   if we have a quoted character, add the part up to the
             *   quoted character plus the quoted character 
             */
            if (qu != nil)
            {
                /* add the part up to here but not including this char */
                if (i != start)
                    ret += substr(start, i - start);

                /* add the quoted form of the character */
                ret += qu;

                /* start again after this character */
                start = i + 1;
            }
        }

        /* add the trailing unquoted part if we haven't already */
        if (i != start)
            ret += substr(start, i - start);

        /* add a close quote and return the result */
        return ret + '\'';
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Modify the List intrinsic class to provide a to-symbol mapping 
 */
modify List
    valToSymbol()
    {
        local ret;
        
        /* start off with an open bracket */
        ret = '[';

        /* convert each element to symbolic form */
        for (local i = 1, local len = length() ; i <= len ; ++i)
        {
            /* add a comma if this isn't the first element */
            if (i != 1)
                ret += ', ';

            /* add this element converted to symbolic form */
            ret += reflectionServices.valToSymbol(self[i]);
        }

        /* add the close bracket and return the result */
        return ret + ']';
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   If desired, modify the BigNumber intrinsic class to provide a
 *   to-symbol mapping.  We only include this modification if the program
 *   is compiled with REFLECT_BIGNUM defined.  
 */
#ifdef REFLECT_BIGNUM
#include "bignum.h"

modify BigNumber
    valToSymbol()
    {
        /* use the default formatting */
        return formatString(12);
    }
;

#endif /* REFLECT_BIGNUM */
