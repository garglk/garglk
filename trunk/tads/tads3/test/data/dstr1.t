#include "tads.h"
#include "t3.h"

main(args)
{
    local f;

    f = func();
    "f = <<f>>; f == nil ? <<f == nil ? "yes" : "no">>\n";

    f = callback({ : "anon!\n" });
    "f = <<f>>; f == nil ? <<f == nil ? "yes" : "no">>\n";
}

preinit() { }

func()
{
    local x = true;
    
    return x ? "Hello!\n" : "";
}

callback(f) { return f(); }
