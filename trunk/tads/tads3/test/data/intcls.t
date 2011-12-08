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
      <<y.getSuperclassList().length()>>\n";
    "first superclass of BigNumber = IntrinsicClass:
      <<sayTF(y.getSuperclassList()[1] == IntrinsicClass)>>\n";

    "\b";

    "BigNumber is an IntrinsicClass:
      <<sayTF(BigNumber.ofKind(IntrinsicClass))>>\n";
    "BigNumber is actually an intrinsic class:
      <<sayTF(IntrinsicClass.isIntrinsicClass(BigNumber))>>\n";
    "y is actually an intrinsic class:
      <<sayTF(IntrinsicClass.isIntrinsicClass(y))>>\n";
    "List is an IntrinsicClass: <<sayTF(List.ofKind(IntrinsicClass))>>\n";
    "List is actually an intrinsic class:
      <<sayTF(IntrinsicClass.isIntrinsicClass(List))>>\n";
    "[1,2,3] is actually an intrinsic class:
      <<sayTF(IntrinsicClass.isIntrinsicClass([1,2,3]))>>\n";
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

