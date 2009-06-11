/*
 *   object replacement test
 */

#include "tads.h"
#include "t3.h"


/*
 *   define our base objects 
 */
obj1: object
    p1 = "This is obj1.p1"
    p2 = "This is obj1.p2"
    p3 = "This is obj1.p3"
;

obj2: object
    p2 = "This is obj2.p2"
    p1 = "This is obj2.p1"
;

/*
 *   replace object obj1 within this translation unit 
 */
replace obj1: object
    p1 = "This is replaced obj1.p1"
    p2 = "This is replaced obj1.p2"
    p4 = "This is replaced obj1.p4"
;

/*
 *   display information on an object 
 */
function showObj(obj, nm)
{
    "----\nShowing information for <<nm>>:\n";

    "\tp1 -> << obj.p1 >>\n";
    "\tp2 -> << obj.p2 >>\n";
    "\tp3 -> << obj.p3 >>\n";
    "\tp4 -> << obj.p4 >>\n";

    "\n";
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
}

