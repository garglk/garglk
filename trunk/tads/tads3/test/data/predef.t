/*
 *   test of pre-defined compiler macros 
 */

#include "t3.h"
#include "tads.h"

_say_embed(str) { tadsSay(str); }

_main(args)
{
    t3SetSay(&_say_embed);

#if defined(__DEBUG)
    "This program was compiled for debugging.\n";
#else
    "This program was not compiled for debugging.\n";
#endif

    "The compiler version number is:
    <<__TADS_VERSION_MAJOR>>.<<__TADS_VERSION_MINOR>>.\n";

    "The compiler system name is <<__TADS_SYSTEM_NAME>>.\n";

#if defined(__TADS_SYS_MSDOS)
    "Compiled for MSDOS\n";
#elif defined(__TADS_SYS_WIN32)
    "Compiled for Win32\n";
#elif defined(__TADS_SYS_MAC)
    "Compiled for Macintosh\n";
#else
    "Compiled for unknown system\n";
#endif

#ifndef LINEDEF
#define LINEDEF 'not defined using -D'
#endif
    
    "The value of LINEDEF is '<<LINEDEF>>'.\n";
}

