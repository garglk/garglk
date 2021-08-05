#include "tads.h"
#include "t3.h"

function _main(args)
{
    local vsn = t3GetVMVsn();
    
    tadsSay('getting VM information...\n');
    tadsSay('VM version number = ' + (vsn >> 16) + '.' + ((vsn >> 8) & 0xff)
        + ',' + (vsn & 0xff) + '\n');
    tadsSay('VM ID = ' + t3GetVMID() + '\n');
    tadsSay('VM banner = "' + t3GetVMBanner() + '"\n');

    tadsSay('Setting the SAY function...\n');
    t3SetSay(&mySay);

    tadsSay('Trying a dstring...\n');
    "Hello from the dstring!!!\n";
    "Here's another one!\n";

    tadsSay('Done!!!\n');
}

function mySay(tr)
{
    tadsSay('<' + str + '>\n');
}
