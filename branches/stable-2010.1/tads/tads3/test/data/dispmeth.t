#include "tads.h"
#include "t3.h"

main(args)
{
    t3SetSay(&sayValue);

    "Hello from main!\n";
    obj1.sayHello;
    obj2.sayHello;
    obj3.sayHello;
    obj4.sayHello;

    redItem.sdesc; "\n";
    redItem.ldesc; "\n";
    blueItem.sdesc; "\n";
    blueItem.ldesc; "\n";
    "Done!\n";
}

class ColorItem: object
  sayValue(val)
  {
    /* substitute color placeholder strings */
    if (dataType(val) == TypeSString)
      val = rexReplace('COLOR', val, colorName, ReplaceAll);

    /* display the value */
    tadsSay(val);
  }
  sdesc = "COLOR item"
  ldesc = "It's a COLOR item. "
;

redItem: ColorItem colorName='red';
blueItem: ColorItem colorName='blue';

obj1: object
    sayValue(val)
    {
        if (dataType(val) == TypeSString)
            tadsSay(val.toUpper());
        else
            tadsSay(val);
    }

    sayHello { "This is obj1.sayHello!\n"; }
;

obj2: obj1
    sayHello { "This is obj2.sayHello (obj2 is an obj1)\n"; }
;

obj3: object
    sayHello { "This is obj3.sayHello (obj3 has no sayValue method)\n"; }
;

obj4: object
    sayHello { "This is obj4.sayHello\n"; }
    
    sayValue(val)
    {
        if (dataType(val) == TypeSString)
            tadsSay(val.toLower());
        else
            tadsSay(val);
    }
;

preinit()
{
}



