/*
 *   intrinsic classes 
 */

#include "tads.h"
#include "t3.h"
#include "bignum.h"
#include "dict.h"

dictionary gDict;

main(args)
{
    local x, y;
    
    x = BigNumber;
    y = x.getPi(10);
    "y = <<y>>\n";

    "y is a BigNumber: <<sayTF(y.ofKind(BigNumber))>>\n";
    "y is a Dictionary: <<sayTF(y.ofKind(Dictionary))>>\n";
    "y is an IntrinsicClass: <<sayTF(y.ofKind(IntrinsicClass))>>\n";
    "gDict is a Dictionary: <<sayTF(gDict.ofKind(Dictionary))>>\n";
    "BigNumber is an IntrinsicClass:
        <<sayTF(BigNumber.ofKind(IntrinsicClass))>>\n";
    "IntrinsicClass is an IntrinsicClass:
        <<sayTF(IntrinsicClass.ofKind(IntrinsicClass))>>\n";
    "y is an Object: <<sayTF(y.ofKind(Object))>>\n";
    "BigNumber is an Object: <<sayTF(BigNumber.ofKind(Object))>>\n";

    "\b";
    
    "first superclass of y = BigNumber:
      <<sayTF(y.getSuperclassList()[1] == BigNumber)>>\n";
    "first superclass of y = Dictionary:
      <<sayTF(y.getSuperclassList()[1] == Dictionary)>>\n";

    "number of superclasses of BigNumber:
      <<BigNumber.getSuperclassList().length()>>\n";
    "first superclass of BigNumber = IntrinsicClass:
      <<sayTF(BigNumber.getSuperclassList()[1] == IntrinsicClass)>>\n";

    "\b";
    
    y = 'abc';
    "y = '<<y>>' (constant)\n";
    "y is a String: <<sayTF(y.ofKind(String))>>\n";
    "y is a List: <<sayTF(y.ofKind(List))>>\n";
    "y is a Object: <<sayTF(y.ofKind(Object))>>\n";
    "first superclass of y = String:
        <<sayTF(y.getSuperclassList()[1] == String)>>\n";
    "first superclass of y = List:
        <<sayTF(y.getSuperclassList()[1] == List)>>\n";

    "\b";
    y += 'def';
    "y = '<<y>>' (object)\n";
    "y is a String: <<sayTF(y.ofKind(String))>>\n";
    "y is a List: <<sayTF(y.ofKind(List))>>\n";
    "y is a Object: <<sayTF(y.ofKind(Object))>>\n";
    "first superclass of y = String:
        <<sayTF(y.getSuperclassList()[1] == String)>>\n";
    "first superclass of y = List:
        <<sayTF(y.getSuperclassList()[1] == List)>>\n";

    "\b";
    y = [1, 2, 3];
    "y = [1,2,3] (constant)\n";
    "y is a String: <<sayTF(y.ofKind(String))>>\n";
    "y is a List: <<sayTF(y.ofKind(List))>>\n";
    "y is a Object: <<sayTF(y.ofKind(Object))>>\n";
    "first superclass of y = String:
        <<sayTF(y.getSuperclassList()[1] == String)>>\n";
    "first superclass of y = List:
        <<sayTF(y.getSuperclassList()[1] == List)>>\n";

    "\b";
    y += [4,5];
    "y = [1,2,3,4,5] (object)\n";
    "y is a String: <<sayTF(y.ofKind(String))>>\n";
    "y is a List: <<sayTF(y.ofKind(List))>>\n";
    "y is a Object: <<sayTF(y.ofKind(Object))>>\n";
    "first superclass of y = String:
        <<sayTF(y.getSuperclassList()[1] == String)>>\n";
    "first superclass of y = List:
        <<sayTF(y.getSuperclassList()[1] == List)>>\n";
}

sayTF(val)
{
    if (val)
        "yes";
    else
        "no";
}

preinit()
{
}

