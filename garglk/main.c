/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef WIN32
#include <limits.h>
#endif

#include "glk.h"
#include "glkstart.h"
#include "garglk.h"

int main(int argc, char *argv[])
{
#ifdef WIN32
    char full[_MAX_PATH];
    if (argc > 1 && _fullpath(full, argv[argc - 1], _MAX_PATH) != NULL)
        argv[argc - 1] = full;
#else
    char full[PATH_MAX];
    if (argc > 1 && realpath(argv[argc - 1], full) != NULL)
        argv[argc - 1] = full;
#endif
    glkunix_startup_t startdata;
    startdata.argc = argc;
    startdata.argv = malloc(argc * sizeof(char*));
    memcpy(startdata.argv, argv, argc * sizeof(char*));

    gli_startup(argc, argv);

    if (!glkunix_startup_code(&startdata))
        glk_exit();

    glk_main();
    glk_exit();

    return 0;
}
