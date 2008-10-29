#include "tads.h"

obj: object
    x = 100
    y = 'hello'
    newPropId = nil
;

main(args)
{
    local newProp;
    local newProp2;

    /* allocate a property */
    newProp = t3AllocProp();
    obj.(newProp) = 'new prop!';
    obj.newPropId = newProp;

    /* save the game */
    global.newProp_ = newProp;
    saveGame('test.t3v');
    "Saved!\n";

    "obj.x = <<obj.x>>\n
    obj.y = <<obj.y>>\n
    obj.(newProp) = <<obj.(newProp)>>\n
    obj.(obj.newPropId) = <<obj.(obj.newPropId)>>\n";
    
    "Restoring...\n";
    restoreGame('test.t3v');
    newProp = global.newProp_;
    "Restored!\n";

    "Post-restore:\n
    obj.x = <<obj.x>>\n
    obj.y = <<obj.y>>\n
    obj.(newProp) = <<obj.(newProp)>>\n
    obj.(obj.newPropId) = <<obj.(obj.newPropId)>>\n";
    
    /* allocate another new property */
    newProp2 = t3AllocProp();
    obj.(newProp2) = 'new prop 2!!!';
    
    "after second new prop:\n
    obj.x = <<obj.x>>\n
    obj.y = <<obj.y>>\n
    obj.(newProp) = <<obj.(newProp)>>\n
    obj.(obj.newPropId) = <<obj.(obj.newPropId)>>\n
    obj.(newProp2) = <<obj.(newProp2)>>\n";

    "Done!\n";
}

global: object
    newProp_ = nil
;
