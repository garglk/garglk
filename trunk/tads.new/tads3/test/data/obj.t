/*
 *   object definition tests 
 */

Item: object
    sdesc = "thing"
    adesc = "a <<sdesc>>"
    ldesc = "It's an ordinary <<sdesc>>."
    idesc = "<<sdesc>>"
;

LightItem: Item
    idesc
    {
        sdesc;
        if (isLit)
            " (providing light)";
    }
;

redBook: Item
    sdesc = "red book"
;

class Item
    sdesc "thing"
    adesc "a <<sdesc>>"
    ldesc "It's an ordinary <<sdesc>>."
    idesc "<<sdesc>>"
;

class LightItem: Item
    idesc
    {
        sdesc;
        if (isLit)
            " (providing light)";
    }
;

Item 

