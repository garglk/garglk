/*
 *   Object Modification test 
 */

#include "tads.h"
#include "t3.h"


/*
 *   define our base objects 
 */
obj_base: object
    p1 = "This is obj_base.p1"
    p2 = "This is obj_base.p2"
    p3 = "This is obj_base.p3"
    p4 = "This is obj_base.p4"
;

obj1: obj_base
    p1 = "This is obj1.p1 - inheriting: << inherited.p1 >>"
    p2 = "This is obj1.p2 - inheriting: << inherited.p2 >>"
    p3 = "This is obj1.p3 - inheriting: << inherited.p3 >>"
;

obj2: obj_base
    p2 = "This is obj2.p2 - inheriting: << inherited.p2 >>"
    p1 = "This is obj2.p1 - inheriting: << inherited.p1 >>"
;

modify obj2
    p1 = "This is obj1mod1-modified obj2.p1 - inheriting: <<inherited.p1>>"
    p2 = "This is obj1mod1-modified obj2.p2 - inheriting: <<inherited.p2>>"
;

obj3: obj2
    p1 = "obj3.p1 - inheriting from obj2: <<inherited.p1>>"
    p2 = "obj3.p2 - inheriting from obj2: <<inherited.p2>>"
    p3 = "obj3.p3 - inheriting from obj2: <<inherited.p3>>"
;

/*
 *   modify obj1
 */
modify obj1
    p1
    {
        "This is modified obj1.p1 - inheriting: ";
        inherited.p1();
    }
    replace p2()
    {
        "This is modified+replaced obj1.p2 - inheriting: ";
        inherited.p2();
    }
;

modify obj1
    p1 = "Doubly-modified obj1.p1 - inheriting: <<inherited.p1>>";

/*
 *   display information on an object 
 */
function showObj(obj, nm)
{
    "----\nShowing information for <<nm>>:\n";

    "\tp1 -> << obj.p1() >>\n";
    "\tp2 -> << obj.p2() >>\n";
    "\tp3 -> << obj.p3() >>\n";
    "\tp4 -> << obj.p4() >>\n";

    "\b";
}

function _say_embed(str)
{
    tadsSay(str);
}

function _main(args)
{
    t3SetSay(_say_embed);

    showObj(obj1, 'obj1');
    showObj(obj2, 'obj2');
    showObj(obj3, 'obj3');
}

