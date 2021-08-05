#include "tads.h"

/* define the T3 system interface */
intrinsic 't3vm/010000'
{
    t3RunGC();
    t3SetSay(funcptr);
    t3GetVMVsn();
    t3GetVMID();
    t3GetVMBanner();
    t3GetVMPreinitMode();
}

function _say_embed(str)
{
    tadsSay(str);
}

function _main(args)
{
    local lst = ['one', 'two', 'three', 'four', 'five',
                 'six', 'seven', 'eight', 'nine', 'ten'];
    local x;

    t3SetSay(_say_embed);
    
    "This is a test of double-quoted strings.\b";
    
    for (local i = 1 ; i <= 10 ; ++i)
        "This is embedded string <<lst[i]>>\n";

    x = 5;
    (x > 3 ? "x is greater than 3 (correct)" : "huh? x is less than 3??");
    "\n";
    (x < 3 ? "x is less than 3 (wrong)" : "x is still greater than 3!");
    "\n";

    "\bDone!!!\n";
}
