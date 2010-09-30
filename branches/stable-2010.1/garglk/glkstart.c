/* glkstart.c: Unix-specific startup code -- sample file.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html

    This file is copyright 1998-2004 by Andrew Plotkin. You may copy,
    distribute, and incorporate it into your own programs, by any means
    and under any conditions, as long as you do not modify it. You may
    also modify this file, incorporate it into your own programs,
    and distribute the modified version, as long as you retain a notice
    in your program or documentation which mentions my name and the URL
    shown above.
*/

/*
    This is Unix startup code for the simplest possible kind of Glk
    program -- no command-line arguments; no startup files; no nothing.

    Remember, this is a sample file. You should copy it into the Glk
    program you are compiling, and modify it to your needs. This should
    *not* be compiled into the Glk library itself.
*/

#include "glk.h"
#include "glkstart.h"

glkunix_argumentlist_t glkunix_arguments[] = {
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    return TRUE;
}

