/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2009 by Baltasar GarcÌa Perez-Schofield.                     *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 * Copyright (C) 2011 by Vaagn Khachatryan.                                   *
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

#include "glk.h"
#include "garversion.h"
#include "launcher.h"

#include <unistd.h>

static const char * AppName = "Gargoyle " VERSION;
static const char * LaunchingTemplate = "%s/%s";
static const char * DirSeparator = "/";

char dir[MaxBuffer];
char buf[MaxBuffer];
char tmp[MaxBuffer];

/*TODO: show a decent dialog, possibly with edje*/
void winmsg(const char * msg)
{
    fprintf(stderr, "%s", msg);
}

int winargs(int argc, char **argv, char *buffer)
{
    if (argc == 2)
    {
        if (!(strlen(argv[1]) < MaxBuffer - 1))
            return 0;

        strcpy(buffer, argv[1]);
    }

    return (argc == 2);
}

void winpath(char *buffer)
{
    char exepath[MaxBuffer] = {0};
    unsigned int exelen;

    exelen = readlink("/proc/self/exe", exepath, sizeof(exepath));

    if (exelen <= 0 || exelen >= MaxBuffer)
    {
        winmsg("Unable to locate executable path");
        exit(EXIT_FAILURE);
    }

    strcpy(buffer, exepath);

    char *dirpos = strrchr(buffer, *DirSeparator);
    if ( dirpos != NULL )
        *dirpos = '\0';

    return;
}

int winexec(const char *cmd, char **args)
{
    return (execv(cmd, args) == 0);
}

int winterp(char *path, char *exe, char *flags, char *game)
{
    sprintf(tmp, LaunchingTemplate, path, exe);

    setenv("GARGLK_INI", path, 0);

    char *args[4] = {NULL, NULL, NULL, NULL};

    if (strstr(flags, "-"))
    {
        args[0] = exe;
        args[1] = flags;
        args[2] = game;
    }
    else
    {
        args[0] = exe;
        args[1] = buf;
    }

    if (!winexec(tmp, args))
    {
        winmsg("Could not start 'terp.\nSorry.");
        return 0;
    }

    return 1;
}

int main(int argc, char **argv)
{
    /* get dir of executable */
    winpath(dir);

    /* get story file */
    if (!winargs(argc, argv, buf))
        return EXIT_FAILURE;

    if (!strlen(buf))
        return EXIT_FAILURE;

    /* run story file */
    return rungame(dir, buf);
}
