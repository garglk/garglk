#include "tads.h"
#include "t3.h"
#include "t3test.h"

function _main(args)
{
    local x;
    
    t3SetSay(_my_say_embed);

    /*
     *   Create a bunch of objects, but only keep a reference to the last
     *   of them. 
     */
    for (local i = 1 ; i <= 10 ; ++i)
        x = new obj1(i);

    "\nrunning garbage collection: should delete objects 1 - 9...\n";
    t3RunGC();
    "done running garbage collection!\n\n";

    "running gc again: should delete object 10...\n";
    x = nil;
    t3RunGC();
    "done running gc again!\n\n";

    "Make sure obj2.objptr is still valid - obj2.objptr.id_ =
        << obj2.objptr.id_ >>\n\n";
    x = t3test_get_obj_id(obj2.objptr);
    "- id = <<x>>, gc state = <<t3test_get_obj_gc_state(x)>>\n";

    "Creating obj3\n";
    new obj3;

    "\nrunning gc: should delete obj3...\n";
    t3RunGC();
    "done running gc\n\n";

    "\nrunning gc: should delete obj4...\n";
    t3RunGC();
    "done running gc\n\n";

    "\nclearing obj2.objptr; running gc: should delete object id = 5...\n";
    x = t3test_get_obj_id(obj2.objptr);
    "- id = <<x>>, gc state = <<t3test_get_obj_gc_state(x)>>\n";
    obj2.objptr = nil;
    t3RunGC();
    "done running gc\n";
    "- id = <<x>>, gc state = <<t3test_get_obj_gc_state(x)>>\n\n";
}

class obj3: object
    finalize()
    {
        "This is obj3.finalize - setting self.objptr = new obj4\n";
        objptr = new obj4;
    }
    objptr = nil
;

_my_say_embed(str) { tadsSay(str); }

class obj4: object
    finalize()
    {
        "This is obj4.finalize\n";
    }
;    

obj2: object
    objptr = nil
;

class obj1: object
    finalize()
    {
        "This is obj1.finalize - id = <<id_>>\n";
        if (id_ == 5)
        {
            "-- Storing a reference to 'self' in obj2.objptr\n";
            obj2.objptr = self;
        }
    }
    construct(id)
    {
        id_ = id;
        "This is obj1.construct - id == <<id_>>\n";
    }
    id_ = nil
;

