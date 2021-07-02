/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2009 by Baltasar García Perez-Schofield.                     *
 * Copyright (C) 2010 by Ben Cressey.                                         *
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <strings.h>

#include "glk.h"
#include "glkstart.h"
#include "gi_blorb.h"
#include "launcher.h"

#define T_ADRIFT    "scare"
#define T_ADVSYS    "advsys"
#define T_AGT       "agility"
#define T_ALAN2     "alan2"
#define T_ALAN3     "alan3"
#define T_GLULX     "git"
#define T_HUGO      "hugo"
#define T_JACL      "jacl"
#define T_LEV9      "level9"
#define T_MGSR      "magnetic"
#define T_QUEST     "geas"
#define T_SCOTT     "scott"
#define T_TADS2     "tadsr"
#define T_TADS3     "tadsr"
#define T_ZCODE     "bocfel"
#define T_ZSIX      "bocfel"

#define ID_ZCOD (giblorb_make_id('Z','C','O','D'))
#define ID_GLUL (giblorb_make_id('G','L','U','L'))

struct launch {
    char terp[MaxBuffer];
    char flags[MaxBuffer];
};

#define MaxBuffer 1024

static int call_winterp(const char *path, const char *interpreter, const char *flags, const char *game)
{
    char exe[MaxBuffer];

    snprintf(exe, sizeof exe, "%s%s", GARGLKPRE, interpreter);

    return winterp(path, exe, flags, game);
}

static int runblorb(const char *path, const char *game, const struct launch *launch)
{
    char magic[4];
    strid_t file = NULL;
    giblorb_result_t res;
    giblorb_map_t *map = NULL;
    const char *found_interpreter;

#define goto_err(reason) do { winmsg("Could not load Blorb file %s:\n%s", game, (reason)); goto err; } while(0);

    file = glkunix_stream_open_pathname((char *)game, 0, 0);
    if (!file)
        goto_err("Unable to open file");

    if (giblorb_create_map(file, &map) != giblorb_err_None)
        goto_err("Does not appear to be a Blorb file");

    if (giblorb_load_resource(map, giblorb_method_FilePos, &res, giblorb_ID_Exec, 0) != giblorb_err_None)
        goto_err("Does not contain a story file (look for a corresponding game file to load instead)");

    glk_stream_set_position(file, res.data.startpos, 0);
    if (glk_get_buffer_stream(file, magic, 4) != 4)
        goto_err("Unable to read story file (possibly corrupted Blorb file)");

    switch (res.chunktype)
    {
        case ID_ZCOD:
            if (strlen(launch->terp) > 0)
                found_interpreter = launch->terp;
            else if (magic[0] == 6)
                found_interpreter = T_ZSIX;
            else
                found_interpreter = T_ZCODE;
            break;

        case ID_GLUL:
            if (strlen(launch->terp) > 0)
                found_interpreter = launch->terp;
            else
                found_interpreter = T_GLULX;
            break;

        default: {
            char msg[MaxBuffer];
            snprintf(msg, sizeof msg, "Unknown game type: 0x%08lx", (unsigned long)res.chunktype);
            goto_err(msg);
        }
    }

#undef goto_err

    giblorb_destroy_map(map);
    glk_stream_close(file, NULL);

    return call_winterp(path, found_interpreter, launch->flags, game);

err:
    if (map != NULL)
        giblorb_destroy_map(map);
    if (file != NULL)
        glk_stream_close(file, NULL);

    return FALSE;
}

static int findterp_impl(const char *file, const char *target, struct launch *launch)
{
    FILE *f;
    char buf[MaxBuffer];
    char *s;
    char *cmd, *arg, *opt;
    int accept = 0;
    int i;

    f = fopen(file, "r");
    if (!f)
        return FALSE;

    while (1)
    {
        s = fgets(buf, sizeof buf, f);
        if (!s)
            break;

        buf[strlen(buf)-1] = 0; /* kill newline */

        if (buf[0] == '#')
            continue;

        if (buf[0] == '[')
        {
            for (i = 0; i < strlen(buf); i++)
                buf[i] = tolower(buf[i]);

            if (strstr(buf, target))
                accept = 1;
            else
                accept = 0;
        }

        if (!accept)
            continue;

        cmd = strtok(buf, "\r\n\t ");
        if (!cmd)
            continue;

        arg = strtok(NULL, "\r\n\t #");
        if (!arg)
            continue;

        if (strcmp(cmd, "terp"))
            continue;

        snprintf(launch->terp, sizeof launch->terp, "%s", arg);

        opt = strtok(NULL, "\r\n\t #");
        if (opt && opt[0] == '-')
            snprintf(launch->flags, sizeof launch->flags, "%s", opt);
    }

    fclose(f);
    return strlen(launch->terp) > 0;
}

static int findterp(const char *config, const char *story, struct launch *launch)
{
    const char *s;
    char ext[MaxBuffer];

    s = strrchr(story, '.');
    if (s != NULL)
        snprintf(ext, sizeof ext, "*%s", s);
    else
        snprintf(ext, sizeof ext, "*.*");

    return findterp_impl(config, story, launch) || findterp_impl(config, ext, launch);
}

static void configterp(const char *path, const char *game, struct launch *launch)
{
    char config[MaxBuffer];
    char story[MaxBuffer];
    char ext[MaxBuffer];
    char *s;
    int i;

    /* set up story */
    s = strrchr(game,'\\');
    if (!s) s = strrchr(game, '/');
    if (s) snprintf(story, sizeof story, "%s", s+1);
    else snprintf(story, sizeof story, "%s", game);

    if (!strlen(story))
        return;

    for (i=0; i < strlen(story); i++)
        story[i] = tolower((unsigned char)story[i]);

    /* set up extension */
    s = strrchr(story, '.');
    if (s) snprintf(ext, sizeof ext, "*%s", s);
    else snprintf(ext, sizeof ext, "*.*");

    /* game file .ini */
    strcpy(config, game);
    s = strrchr(config, '.');
    if (s) strcpy(s, ".ini");
    else strcat(config, ".ini");

    if (findterp(config, story, launch))
        return;

    /* game directory .ini */
    strcpy(config, game);
    s = strrchr(config, '\\');
    if (!s) s = strrchr(config, '/');
    if (s) strcpy(s+1, "garglk.ini");
    else strcpy(config, "garglk.ini");

    if (findterp(config, story, launch))
        return;

    /* current directory .ini */
    s = getcwd(config, sizeof(config));
    if (s)
    {
        strcat(config, "/garglk.ini");
        if (findterp(config, story, launch))
            return;
    }

    /* various environment directories */
    const char *env_vars[] = {"XDG_CONFIG_HOME", "HOME", "GARGLK_INI"};
    for(size_t i = 0; i < (sizeof env_vars) / (sizeof *env_vars); i++)
    {
        const char *dir = getenv(env_vars[i]);
        if (dir != NULL)
        {
            snprintf(config, sizeof config, "%s/.garglkrc", dir);
            if (findterp(config, story, launch))
                return;

            snprintf(config, sizeof config, "%s/garglk.ini", dir);
            if (findterp(config, story, launch))
                return;
        }
    }

    /* system directory */
    if (findterp(GARGLKINI, story, launch))
        return;

    /* install directory */
    snprintf(config, sizeof config, "%s/garglk.ini", path);
    if (findterp(config, story, launch))
        return;
}

struct interpreter {
    const char *ext;
    const char *interpreter;
    const char *flags;
};

#define ASIZE(array) ((sizeof (array)) / (sizeof *(array)))

int rungame(const char *path, const char *game)
{
    const char *blorbs[] = {"blb", "blorb", "glb", "gbl", "gblorb", "zlb", "zbl", "zblorb"};
    const struct interpreter interpreters[] = {
        { .ext = "dat", .interpreter = T_ADVSYS },
        { .ext = "d$$", .interpreter = T_AGT, .flags = "-gl" },
        { .ext = "agx", .interpreter = T_AGT, .flags = "-gl" },
        { .ext = "acd", .interpreter = T_ALAN2 },
        { .ext = "a3c", .interpreter = T_ALAN3 },
        { .ext = "taf", .interpreter = T_ADRIFT },
        { .ext = "ulx", .interpreter = T_GLULX },
        { .ext = "hex", .interpreter = T_HUGO },
        { .ext = "jacl", .interpreter = T_JACL },
        { .ext = "j2", .interpreter = T_JACL },
        { .ext = "gam", .interpreter = T_TADS2 },
        { .ext = "t3", .interpreter = T_TADS3 },
        { .ext = "z1", .interpreter = T_ZCODE },
        { .ext = "z2", .interpreter = T_ZCODE },
        { .ext = "z3", .interpreter = T_ZCODE },
        { .ext = "z4", .interpreter = T_ZCODE },
        { .ext = "z5", .interpreter = T_ZCODE },
        { .ext = "z7", .interpreter = T_ZCODE },
        { .ext = "z8", .interpreter = T_ZCODE },
        { .ext = "z6", .interpreter = T_ZSIX },
        { .ext = "l9", .interpreter = T_LEV9 },
        { .ext = "sna", .interpreter = T_LEV9 },
        { .ext = "mag", .interpreter = T_MGSR },
        { .ext = "asl", .interpreter = T_QUEST },
        { .ext = "cas", .interpreter = T_QUEST },
        { .ext = "saga", .interpreter = T_SCOTT },
    };
    struct launch launch = { 0 };

    configterp(path, game, &launch);

    char *ext = strrchr(game, '.');
    if (ext)
        ext++;
    else
        ext = "";

    for (int i = 0; i < ASIZE(blorbs); i++)
    {
        if (strcasecmp(ext, blorbs[i]) == 0)
        {
            return runblorb(path, game, &launch);
        }
    }

    if (strlen(launch.terp) > 0)
        return call_winterp(path, launch.terp, launch.flags, game);

    for (int i = 0; i < ASIZE(interpreters); i++)
    {
        const struct interpreter *interpreter = &interpreters[i];

        if (strcasecmp(ext, interpreter->ext) == 0)
        {
            const char *flags = interpreter->flags == NULL ? "" : interpreter->flags;
            return call_winterp(path, interpreter->interpreter, flags, game);
        }
    }

    winmsg("Unknown file type: \"%s\"\nSorry.", ext);

    return FALSE;
}
