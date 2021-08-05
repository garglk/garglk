#include "tads.h"

class Connector: object
    construct(name)
    {
        tadsSay('Connector.construct(' + name + ')\n');
        name_ = name;
    }
    name_ = nil
;

obj1: object
    prop1 = 5
    prop2 = static(new Connector('hello'))
    prop3 = static(self.prop4 + 10)
    prop4 = static(getTime(GetTimeTicks))
    prop5 { return new Connector('goodbye'); }
;

main(args)
{
    "start: ticks = <<getTime(GetTimeTicks)>>\b";
    
    "obj1.prop1 = <<obj1.prop1>>\n
    obj1.prop2.name_ = <<obj1.prop2.name_>>\n
    obj1.prop3 = <<obj1.prop3>>\n
    obj1.prop4 = <<obj1.prop4>>\n
    obj1.prop5.name_ = <<obj1.prop5.name_>>\n";

    "\bend: ticks = <<getTime(GetTimeTicks)>>\n";
}
