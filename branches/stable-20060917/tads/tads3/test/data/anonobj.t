/*
 *   Anonymous object test 
 */

#include "tads.h"
#include "t3.h"

/* set the 'location' property */
+ property location;

PreinitObject
    execute()
    {
        /* initialize the location for every object */
        forEachInstance(CObject, { obj: obj.initLocation() });
    }
;

main(args)
{
    local cnt;
    
    "This is the anonymous object test!\b";

    "List of 'Item' objects:\n";
    cnt = 0;
    forEachInstance(Item, { obj: "[<<++cnt>>]: <<obj.sdesc>>\n" });
    "\b";

    "Containment graph:\n";
    forEachInstance(Room,
        { obj: "Room: <<obj.sdesc>>\n<<obj.listCont(1)>>\b" });
    "\b";
}

/*
 *   root object 
 */
class CObject: object
    /* list the contents */
    listCont(level)
    {
        for (local i = 1, local cnt = contents.length() ; i <= cnt ; ++i)
        {
            /* indent */
            for (local j = 1 ; j <= level ; ++j)
                "\ \ ";

            /* show this object and end the line */
            contents[i].sdesc;
            "\n";

            /* show the contents of this object */
            contents[i].listCont(level + 1);
        }
    }

    /* initialize location - add myself to my location's contents list */
    initLocation()
    {
        if (self.propType(&location) == TypeObject)
            location.contents += self;
    }

    /* contents list */
    contents = []
;
    
class Item: CObject
    sdesc = "<Item>"
;

class Container: Item
;

class Room: CObject
    sdesc = "<Room>"
;

Item sdesc = "red ball";
Item sdesc = "blue ball";
Item sdesc = "green box";

yellowBox: Item sdesc = "yellow box";

Cave: Room
    sdesc = "Cave"
;

+ Item sdesc = "nasty knife" ;
+ rustyKnife: Item sdesc = "rusty knife" ;
+ Container sdesc = "box" ;

++ Container sdesc = "envelope" ;
+++ Item sdesc = "letter" ;
++ Item sdesc = "fuel cell" ;

+ Item sdesc = "axe" ;

NSPassage: Room
    sdesc = "North-South Passage"
;

+ Container sdesc = "tank" ;
++ Item sdesc = "quantity of water" ;

