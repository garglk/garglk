/*
** glkstart.c - OS-specific startup code. This is where all those weird
** nifty different Glk startup code bits go.
**
** Notes:
**   14 Jan 99: Initial creation 
*/

#include "glk.h"
#include "glkstart.h"
#include "os.h"
#include "oss_glk.h"

#ifdef GLKUNIX

glkunix_argumentlist_t glkunix_arguments[] = {
    { "-ctab", glkunix_arg_ValueFollows, "-ctab FILE: use the character \
mapping table FILE." },
    { "-i", glkunix_arg_ValueFollows, "-i FILE: read commands from FILE." },
    { "-o", glkunix_arg_ValueFollows, "-o FILE: write commands to FILE." },
    { "-l", glkunix_arg_ValueFollows, "-l FILE: log all output to FILE." },
    { "-m", glkunix_arg_NumberValue, "-m SIZE: maximum cache size (in \
bytes). " },
    { "-mh", glkunix_arg_NumberValue, "-mh SIZE: heap size (default 65535 \
bytes). " },
    { "-ms", glkunix_arg_NumberValue, "-ms SIZE: stack size (default 512 \
elements). " },
    { "-p", glkunix_arg_NoValue, "-p: [toggle] pause after play. " },
    { "-r", glkunix_arg_ValueFollows, "-r FILE: restore saved game position \
from FILE. " },
    { "-s", glkunix_arg_NumberValue, "-s LEVEL: set I/O safety level. " },
    { "-s?", glkunix_arg_NoValue, "-s?: help on I/O safety level. " },
    { "-tf", glkunix_arg_ValueFollows, "-tf FILE: use FILE for swapping. " },
    { "-ts", glkunix_arg_NumberValue, "-ts SIZE: maximum swapfile size \
(default: unlimited). " },
    { "-tp", glkunix_arg_NoValue, "-tp: preload all objects. " },
    { "-t+", glkunix_arg_NoValue, "-t+: enable swapping. " },
    { "-u", glkunix_arg_NumberValue, "-u SIZE: set undo size (0 to disable; \
default 60000). " },
    { "", glkunix_arg_ValueFollows, "filename: The .gam file to load." },
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    tads_argc = data->argc;
    tads_argv = data->argv;
    return TRUE;
}

#endif /* GLKUNIX */
