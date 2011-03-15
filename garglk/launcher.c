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
#define T_ZOLD      "frotz"
#define T_ZCODE     "bocfel"
#define T_ZSIX      "bocfel"

#define ID_ZCOD (giblorb_make_id('Z','C','O','D'))
#define ID_GLUL (giblorb_make_id('G','L','U','L'))

#define MaxBuffer 1024
char tmp[MaxBuffer];
char terp[MaxBuffer];
char exe[MaxBuffer];
char flags[MaxBuffer];

int runblorb(char *path, char *game)
{
    char magic[4];
    strid_t file;
    giblorb_result_t res;
    giblorb_err_t err;
    giblorb_map_t *map;

    sprintf(tmp, "Could not load Blorb file:\n%s\n", game);

    file = glkunix_stream_open_pathname(game, 0, 0);
    if (!file)
    {
        winmsg(tmp);
        return FALSE;
    }

    err = giblorb_create_map(file, &map);
    if (err)
    {
        winmsg(tmp);
        return FALSE;
    }

    err = giblorb_load_resource(map, giblorb_method_FilePos,
            &res, giblorb_ID_Exec, 0);
    if (err)
    {
        winmsg(tmp);
        return FALSE;
    }

    glk_stream_set_position(file, res.data.startpos, 0);
    glk_get_buffer_stream(file, magic, 4);

    switch (res.chunktype)
    {
        case ID_ZCOD:
            if (strlen(terp))
                return winterp(path, strcat(exe,terp), flags, game);
            else if (magic[0] == 6)
                return winterp(path, strcat(exe,T_ZSIX), flags, game);
            else
                return winterp(path, strcat(exe,T_ZCODE), flags, game);
            break;

        case ID_GLUL:
            if (strlen(terp))
                return winterp(path, strcat(exe,terp), flags, game);
            else
                return winterp(path, strcat(exe,T_GLULX), flags, game);
            break;

        default:
            sprintf(tmp, "Unknown game type in Blorb file:\n%s\n", game);
            winmsg(tmp);
    }
    
    return FALSE;
}

int findterp(char *file, char *target)
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

        strcpy(terp,arg);

        opt = strtok(NULL, "\r\n\t #");
        if (opt && opt[0] == '-')
            strcpy(flags, opt);
    }

    fclose(f);
    return strlen(terp);

}

int configterp(char *path, char *game)
{
    char config[MaxBuffer];
    char story[MaxBuffer];
    char ext[MaxBuffer];
    char *s;
    int i;

    /* set up story */
    s = strrchr(game,'\\');
    if (!s) s = strrchr(game, '/');
    if (s) strcpy(story, s+1);
    else strcpy(story, game);

    if (!strlen(story))
        return FALSE;

    for (i=0; i < strlen(story); i++)
        story[i] = tolower(story[i]);

    /* set up extension */
    strcpy(ext, "*");
    s = strrchr(story, '.');
    if (s) strcat(ext, s);
    else strcat(ext, ".*");

    /* game file .ini */
    strcpy(config, game);
    s = strrchr(config, '.');
    if (s) strcpy(s, ".ini");
    else strcat(config, ".ini");

    if (findterp(config, story) || findterp(config, ext))
        return TRUE;

    /* game directory .ini */
    strcpy(config, game);
    s = strrchr(config, '\\');
    if (!s) s = strrchr(config, '/');
    if (s) strcpy(s+1, "garglk.ini");
    else strcpy(config, "garglk.ini");

    if (findterp(config, story) || findterp(config, ext))
        return TRUE;

    /* current directory .ini */
    s = getcwd(config, sizeof(config));
    if (s)
    {
        strcat(config, "/garglk.ini");
        if (findterp(config, story) || findterp(config, ext))
            return TRUE;
    }

    /* various environment directories */
    if (getenv("XDG_CONFIG_HOME"))
    {
        strcpy(config, getenv("XDG_CONFIG_HOME"));
        strcat(config, "/.garglkrc");
        if (findterp(config, story) || findterp(config, ext))
            return TRUE;

        strcpy(config, getenv("XDG_CONFIG_HOME"));
        strcat(config, "/garglk.ini");
        if (findterp(config, story) || findterp(config, ext))
            return TRUE;
    }

    if (getenv("HOME"))
    {
        strcpy(config, getenv("HOME"));
        strcat(config, "/.garglkrc");
        if (findterp(config, story) || findterp(config, ext))
            return TRUE;

        strcpy(config, getenv("HOME"));
        strcat(config, "/garglk.ini");
        if (findterp(config, story) || findterp(config, ext))
            return TRUE;
    }

    if (getenv("GARGLK_INI"))
    {
        strcpy(config, getenv("GARGLK_INI"));
        strcat(config, "/.garglkrc");
        if (findterp(config, story) || findterp(config, ext))
            return TRUE;

        strcpy(config, getenv("GARGLK_INI"));
        strcat(config, "/garglk.ini");
        if (findterp(config, story) || findterp(config, ext))
            return TRUE;
    }

    /* system directory */
    strcpy(config, GARGLKINI);
    if (findterp(config, story) || findterp(config, ext))
        return TRUE;

    /* install directory */
    strcpy(config, path);
    strcat(config, "/garglk.ini");
    if (findterp(config, story) || findterp(config, ext))
        return TRUE;

    /* give up */
    return FALSE;
}

int rungame(char *path, char *game)
{
    /* initialize buffers */
    strcpy(exe, GARGLKPRE);
    strcpy(terp, "");
    strcpy(flags, "");

    configterp(path, game);

    char *ext = strrchr(game, '.');

    if (ext)
        ext++;
    else
        ext = "";

    if (!strcasecmp(ext, "blb"))
        return runblorb(path, game);
    if (!strcasecmp(ext, "blorb"))
        return runblorb(path, game);
    if (!strcasecmp(ext, "glb"))
        return runblorb(path, game);
    if (!strcasecmp(ext, "gbl"))
        return runblorb(path, game);
    if (!strcasecmp(ext, "gblorb"))
        return runblorb(path, game);
    if (!strcasecmp(ext, "zlb"))
        return runblorb(path, game);
    if (!strcasecmp(ext, "zbl"))
        return runblorb(path, game);
    if (!strcasecmp(ext, "zblorb"))
        return runblorb(path, game);

    if (strlen(terp))
        return winterp(path, strcat(exe,terp), flags, game);

    if (!strcasecmp(ext, "dat"))
        return winterp(path, strcat(exe,T_ADVSYS), "", game);

    if (!strcasecmp(ext, "d$$"))
        return winterp(path, strcat(exe,T_AGT), "-gl", game);
    if (!strcasecmp(ext, "agx"))
        return winterp(path, strcat(exe,T_AGT), "-gl", game);

    if (!strcasecmp(ext, "acd"))
        return winterp(path, strcat(exe,T_ALAN2), "", game);

    if (!strcasecmp(ext, "a3c"))
        return winterp(path, strcat(exe,T_ALAN3), "", game);

    if (!strcasecmp(ext, "taf"))
        return winterp(path, strcat(exe,T_ADRIFT), "", game);

    if (!strcasecmp(ext, "ulx"))
        return winterp(path, strcat(exe,T_GLULX), "", game);

    if (!strcasecmp(ext, "hex"))
        return winterp(path, strcat(exe,T_HUGO), "", game);

    if (!strcasecmp(ext, "jacl"))
        return winterp(path, strcat(exe,T_JACL), "", game);
    if (!strcasecmp(ext, "j2"))
        return winterp(path, strcat(exe,T_JACL), "", game);

    if (!strcasecmp(ext, "gam"))
        return winterp(path, strcat(exe,T_TADS2), "", game);

    if (!strcasecmp(ext, "t3"))
        return winterp(path, strcat(exe,T_TADS3), "", game);

    if (!strcasecmp(ext, "z1"))
        return winterp(path, strcat(exe,T_ZOLD), "", game);
    if (!strcasecmp(ext, "z2"))
        return winterp(path, strcat(exe,T_ZOLD), "", game);

    if (!strcasecmp(ext, "z3"))
        return winterp(path, strcat(exe,T_ZCODE), "", game);
    if (!strcasecmp(ext, "z4"))
        return winterp(path, strcat(exe,T_ZCODE), "", game);
    if (!strcasecmp(ext, "z5"))
        return winterp(path, strcat(exe,T_ZCODE), "", game);
    if (!strcasecmp(ext, "z7"))
        return winterp(path, strcat(exe,T_ZCODE), "", game);
    if (!strcasecmp(ext, "z8"))
        return winterp(path, strcat(exe,T_ZCODE), "", game);

    if (!strcasecmp(ext, "z6"))
        return winterp(path, strcat(exe,T_ZSIX), "", game);

    if (!strcasecmp(ext, "l9"))
        return winterp(path, strcat(exe,T_LEV9), "", game);
    if (!strcasecmp(ext, "sna"))
        return winterp(path, strcat(exe,T_LEV9), "", game);
    if (!strcasecmp(ext, "mag"))
        return winterp(path, strcat(exe,T_MGSR), "", game);

    if (!strcasecmp(ext, "asl"))
        return winterp(path, strcat(exe,T_QUEST), "", game);
    if (!strcasecmp(ext, "cas"))
        return winterp(path, strcat(exe,T_QUEST), "", game);

    if (!strcasecmp(ext, "saga"))
        return winterp(path, strcat(exe,T_SCOTT), "", game);

    sprintf(tmp, "Unknown file type: \"%s\"\nSorry.", ext);
    winmsg(tmp);
    
    return FALSE;
}
