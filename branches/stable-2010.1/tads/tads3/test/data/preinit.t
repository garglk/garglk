#include "tads.h"
#include "t3.h"

/* 
 *   global variables 
 */
global: object
    /* flag: we've run the 'preinit' function */
    preinited_ = nil
;

export RuntimeError;
export exceptionMessage;

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    exceptionMessage = nil
    errno_ = 0
;

room1: object
    contents = []
;

book: object
    sdesc = "book"
    location = room1
;

ball: object
    sdesc = "ball"
    location = room1
;

_say_embed(str) { tadsSay(str); }

/*
 *   main entrypoint 
 */
function _main(args)
{
    t3SetSay(_say_embed);
    try
    {
        /* if haven't yet run preinit, do so now */
        if (!global.preinited_)
        {
            /* run preinit */
            preinit();
            
            /* make a note that we've completed pre-initialization */
            global.preinited_ = true;
            
            tadsSay('global.preinited_ = '
                    + (global.preinited_ ? 'true' : 'false')
                    + '\n');
        }
        
        /* if we're in preinit-only mode, we're done */
        if (t3GetVMPreinitMode())
            return;
        
        /* invoke the user's main program */
        main();
    }
    catch(RuntimeError rt)
    {
        tadsSay('\n!!! RuntimeError: ' + rt.errno_ + '\n');
    }
}

/*
 *   user pre-initialization routine 
 */
function preinit()
{
    local str;
    
    tadsSay('this is preinit!!!\n');

    obj1.prop1 = 'Hello';
    obj1.prop1 += '!!!';
    obj1.prop2 = [obj1.prop1, obj1.prop1, obj1.prop1];

    str = 'Test';
    str += ' ';
    str += 'Instance';
    str += '!';
    obj1.prop3 = new MyClass(str);

    /* build contents lists */
    for (local obj = firstObj() ; obj != nil ; obj = nextObj(obj))
    {
        if (obj.location != nil)
            obj.location.contents += obj;
    }
}

/*
 *   user main program 
 */
function main()
{
    tadsSay('this is the main entrypoint!!!\n');

    tadsSay('obj1.prop1 = ' + obj1.prop1 + '\n');

    tadsSay('obj1.prop2 = ');
    sayList(obj1.prop2);
    tadsSay('\n');

    tadsSay('obj1.prop3 = nil? ' + (obj1.prop3 == nil ? 'yes' : 'no') + '\n');
    tadsSay('obj1.prop3.instName = ' + obj1.prop3.instName + '\n');

    tadsSay('contents of room1:\n');
    for (local lst = room1.contents ; lst.length() != 0 ; lst = lst.cdr())
        "\t<<lst.car().sdesc>>\n";
}

obj1: object
    prop1 = '<init>'
    prop2 = '<init>'
    prop3 = '<init>'
;

class MyClass: object
    sdesc { tadsSay('This is MyClass: instName = ' + instName); }
    instName = '(none)'

    construct(nm) { instName = nm; }
;

function sayList(lst)
{
    tadsSay('[');
    for (local i = 1, local cnt = lst.length() ; i <= cnt ; ++i)
    {
        tadsSay(lst[i]);
        if (i < cnt)
            tadsSay(', ');
    }
    tadsSay(']');
}
