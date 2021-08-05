#include <tads.h>

myObj: object
    lst = [ ({: "This is <<name>>. "}), ({: "I am <<name>>. "}) ]
    lst2 = [ (self.name), (self.name) ]
    lst3 = (showName)
    name = 'myObj'
    showName()
    {
        tadsSay(name);
    }
    showDisp(which)
    {
        (lst[which])();
    }
;

main(args)
{
    myObj.showDisp(1); "\n";
    myObj.showDisp(2); "\b";

    "<<myObj.lst2[1]>>\n<<myObj.lst2[2]>>\b";

    myObj.lst3; "\b";
}
