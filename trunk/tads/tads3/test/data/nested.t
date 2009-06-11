#include "tads.h"

class Connector: object
    travelTo()
    {
        "Connector: Traveling to <<dest.name>>\n";
    }
    dest = nil
;

class Room: object
    travelTo()
    {
        "Room: Traveling to <<dest.name>>\n";
    }
;

northSouthCrawl: Room
    east = iceCave
    name = "North-South Crawl"
;

iceCave: Room
    west = northSouthCrawl
    east: Connector { dest = topOfGlacier }
    name = "Ice Cave"
;

topOfGlacier: Room
    west: Connector { dest = iceCave }
    name = "Top of Glacier"
;

main(args)
{
    iceCave.east.travelTo();
    topOfGlacier.west.travelTo();
}

