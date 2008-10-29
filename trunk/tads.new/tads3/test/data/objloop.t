/*
 *   object loop tests 
 */

#include "tads.h"
#include "t3.h"


class Room: object
    sdesc = "&lt;class Room>"
;

startroom: Room
    sdesc = "Starting Room"
;

eastroom: Room
    sdesc = "East Room"
;

westroom: Room
    sdesc = "West Room"
;

class Item: object
    sdesc = "&lt;class Item>"
    ldesc = "It looks like an ordinary <<sdesc>>. "
;

book: Item
    location = startroom
    sdesc = "book"
    ldesc = "It's a book. "
;

ball: Item
    location = eastroom
    sdesc = "ball"
;

class FixedItem: object
    sdesc = "&lt;class FixedItem>"
;

box: Item, FixedItem
    location = westroom
    sdesc = "box"
;

preinit()
{
}

main(args)
{
    local obj;
    local lst;

    "Scanning all objects:\n";
    for (obj = firstObj() ; obj != nil ; obj = nextObj(obj))
        "<<obj.sdesc>>\n";

    "\bScanning all class objects:\n";
    for (obj = firstObj(ObjClasses) ; obj != nil ;
         obj = nextObj(obj, ObjClasses))
        "<<obj.sdesc>>\n";

    "\bScanning all Rooms:\n";
    for (obj = firstObj(Room) ; obj != nil ; obj = nextObj(obj, Room))
        "<<obj.sdesc>>\n";

    "\bScanning all Items:\n";
    for (obj = firstObj(Item) ; obj != nil ; obj = nextObj(obj, Item))
        "<<obj.sdesc>>\n";

    "\bTesting ofKind:\n";
    "startroom.ofKind(Room) = <<bool2str(startroom.ofKind(Room))>>\n";
    "startroom.ofKind(Item) = <<bool2str(startroom.ofKind(Item))>>\n";
    "box.ofKind(Room) = <<bool2str(box.ofKind(Room))>>\n";
    "box.ofKind(Item) = <<bool2str(box.ofKind(Item))>>\n";

    "\bTesting getSuperclassList:\n";
    lst = startroom.getSuperclassList();
    for (local i = 1, local len = lst.length() ; i <= len ; ++i)
        "sc(startroom, <<i>>) = <<lst[i].sdesc>>\n";

    lst = box.getSuperclassList();
    for (local i = 1, local len = lst.length() ; i <= len ; ++i)
        "sc(box, <<i>>) = <<lst[i].sdesc>>\n";

    "\bTesting propDefined:\n";
    "startroom.propDefined(sdesc) =
        <<bool2str(startroom.propDefined(&sdesc))>>\n";
    "startroom.propDefined(location) =
        <<bool2str(startroom.propDefined(&location))>>\n";
    "book.propDefined(sdesc) =
        <<bool2str(book.propDefined(&sdesc))>>\n";
    "book.propDefined(location) =
        <<bool2str(book.propDefined(&location))>>\n";

    "\b";
    "book.propDefined(&sdesc, PropDefDirectly) =
        <<bool2str(book.propDefined(&sdesc, PropDefDirectly))>>\n";
    "book.propDefined(&sdesc, PropDefInherits) =
        <<bool2str(book.propDefined(&sdesc, PropDefInherits))>>\n";
    "book.propDefined(&ldesc, PropDefDirectly) =
        <<bool2str(book.propDefined(&ldesc, PropDefDirectly))>>\n";
    "book.propDefined(&ldesc, PropDefInherits) =
        <<bool2str(book.propDefined(&ldesc, PropDefInherits))>>\n";
    "book.propDefined(&sdesc, PropDefGetClass) =
        <<obj2str(book.propDefined(&sdesc, PropDefGetClass))>>\n";
    "book.propDefined(&ldesc, PropDefGetClass) =
        <<obj2str(book.propDefined(&ldesc, PropDefGetClass))>>\n";

    "\b";
    "ball.propDefined(&sdesc, PropDefDirectly) =
        <<bool2str(ball.propDefined(&sdesc, PropDefDirectly))>>\n";
    "ball.propDefined(&sdesc, PropDefInherits) =
        <<bool2str(ball.propDefined(&sdesc, PropDefInherits))>>\n";
    "ball.propDefined(&ldesc, PropDefDirectly) =
        <<bool2str(ball.propDefined(&ldesc, PropDefDirectly))>>\n";
    "ball.propDefined(&ldesc, PropDefInherits) =
        <<bool2str(ball.propDefined(&ldesc, PropDefInherits))>>\n";
    "ball.propDefined(&sdesc, PropDefGetClass) =
        <<obj2str(ball.propDefined(&sdesc, PropDefGetClass))>>\n";
    "ball.propDefined(&ldesc, PropDefGetClass) =
        <<obj2str(ball.propDefined(&ldesc, PropDefGetClass))>>\n";

    "\bTesting propType:\n";
    "startroom.propType(&sdesc) = <<startroom.propType(&sdesc)>>\n";
    "startroom.propType(&location) = <<startroom.propType(&location)>>\n";

    "\bDone!\n";
}

bool2str(x) { return x ? 'true' : 'nil'; }

obj2str(obj) { obj = nil ? "nil" : obj.sdesc; }
