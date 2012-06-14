/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#ifndef _WIN32
#include <unistd.h> /* for getcwd() */
#endif

#include "glk.h"
#include "glkstart.h"
#include "garglk.h"

int gli_utf8input = FALSE ;
int gli_utf8output = FALSE ;

char *gli_conf_propr = "CharterBT-Roman";
char *gli_conf_propb = "CharterBT-Bold";
char *gli_conf_propi = "CharterBT-Italic";
char *gli_conf_propz = "CharterBT-BoldItalic";

char *gli_conf_monor = "LuxiMonoRegular";
char *gli_conf_monob = "LuxiMonoBold";
char *gli_conf_monoi = "LuxiMonoOblique";
char *gli_conf_monoz = "LuxiMonoBoldOblique";

#ifdef BUNDLED_FONTS
char *gli_conf_monofont = "";
char *gli_conf_propfont = "";
float gli_conf_monosize = 12.6;	/* good size for LuxiMono */
float gli_conf_propsize = 14.7;	/* good size for CharterBT */
#else
char *gli_conf_monofont = "Liberation Mono";
char *gli_conf_propfont = "Linux Libertine O";
float gli_conf_monosize = 12.5;	/* good size for LiberationMono */
float gli_conf_propsize = 15.5;	/* good size for Libertine */
#endif

style_t gli_tstyles[style_NUMSTYLES] =
{
    {PROPR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Normal */
    {PROPI, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Emphasized */
    {MONOR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Preformatted */
    {PROPB, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Header */
    {PROPB, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Subheader */
    {PROPZ, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Alert */
    {PROPR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Note */
    {PROPR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* BlockQuote */
    {PROPB, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Input */
    {MONOR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* User1 */
    {MONOR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* User2 */
};

style_t gli_gstyles[style_NUMSTYLES] =
{
    {MONOR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Normal */
    {MONOI, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Emphasized */
    {MONOR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Preformatted */
    {MONOB, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Header */
    {MONOB, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Subheader */
    {MONOR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Alert */
    {MONOR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Note */
    {MONOR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* BlockQuote */
    {MONOR, {0xff,0xff,0xff}, {0x00,0x00,0x00}, 0}, /* Input */
    {MONOR, {0x60,0x60,0x60}, {0xff,0xff,0xff}, 0}, /* User1 */
    {MONOR, {0x60,0x60,0x60}, {0xff,0xff,0xff}, 0}, /* User2 */
};

style_t gli_tstyles_def[style_NUMSTYLES];
style_t gli_gstyles_def[style_NUMSTYLES];

static int font2idx(char *font)
{
    if (!strcmp(font, "monor")) return MONOR;
    if (!strcmp(font, "monob")) return MONOB;
    if (!strcmp(font, "monoi")) return MONOI;
    if (!strcmp(font, "monoz")) return MONOZ;
    if (!strcmp(font, "propr")) return PROPR;
    if (!strcmp(font, "propb")) return PROPB;
    if (!strcmp(font, "propi")) return PROPI;
    if (!strcmp(font, "propz")) return PROPZ;
    return MONOR;
}

float gli_conf_gamma = 1.0;

unsigned char gli_window_color[3] = { 0xff, 0xff, 0xff };
unsigned char gli_caret_color[3] = { 0x00, 0x00, 0x00 };
unsigned char gli_border_color[3] = { 0x00, 0x00, 0x00 };
unsigned char gli_more_color[3] = { 0x00, 0x00, 0x00 };
unsigned char gli_link_color[3] = { 0x00, 0x00, 0x60 };

unsigned char gli_window_save[3] = { 0xff, 0xff, 0xff };
unsigned char gli_caret_save[3] = { 0x00, 0x00, 0x00 };
unsigned char gli_border_save[3] = { 0x00, 0x00, 0x00 };
unsigned char gli_more_save[3] = { 0x00, 0x00, 0x00 };
unsigned char gli_link_save[3] = { 0x00, 0x00, 0x60 };

int gli_override_fg_set = 0;
int gli_override_bg_set = 0;
int gli_override_fg_val = 0;
int gli_override_bg_val = 0;
int gli_override_reverse = 0;

char *gli_more_prompt = "\207 more \207";
int gli_more_align = 0;
int gli_more_font = PROPB;

unsigned char gli_scroll_bg[3] = { 0xb0, 0xb0, 0xb0 };
unsigned char gli_scroll_fg[3] = { 0x80, 0x80, 0x80 };
int gli_scroll_width = 0;

int gli_caret_shape = 2;
int gli_link_style = 1;

int gli_conf_lcd = 1;

int gli_wmarginx = 15;
int gli_wmarginy = 15;
int gli_wpaddingx = 0;
int gli_wpaddingy = 0;
int gli_wborderx = 1;
int gli_wbordery = 1;
int gli_tmarginx = 7;
int gli_tmarginy = 7;

int gli_wmarginx_save = 15;
int gli_wmarginy_save = 15;

int gli_cols = 60;
int gli_rows = 25;

int gli_conf_lockcols = FALSE;
int gli_conf_lockrows = FALSE;

float gli_conf_propaspect = 1.0;
float gli_conf_monoaspect = 1.0;

int gli_baseline = 15;
int gli_leading = 20;

int gli_conf_justify = 0;
int gli_conf_quotes = 1;
int gli_conf_dashes = 1;
int gli_conf_spaces = 0;
int gli_conf_caps = 0;

int gli_conf_graphics = 1;
int gli_conf_sound = 1;
int gli_conf_speak = 0;

int gli_conf_stylehint = 0;
int gli_conf_safeclicks = 0;

static void parsecolor(char *str, unsigned char *rgb)
{
    char r[3];
    char g[3];
    char b[3];

    if (strlen(str) != 6)
        return;

    r[0] = str[0]; r[1] = str[1]; r[2] = 0;
    g[0] = str[2]; g[1] = str[3]; g[2] = 0;
    b[0] = str[4]; b[1] = str[5]; b[2] = 0;

    rgb[0] = strtol(r, NULL, 16);
    rgb[1] = strtol(g, NULL, 16);
    rgb[2] = strtol(b, NULL, 16);
}

char* trim(char* src)
{
    while(src[strlen(src)-1] == ' ' || src[strlen(src)-1] == '\t')
        src[strlen(src)-1] = 0;

    while(src[0] == ' ' || src[0] == '\t')
        src++;

    return src;
}

static void readoneconfig(char *fname, char *argv0, char *gamefile)
{
    FILE *f;
    char buf[1024];
    char *s;
    char *cmd, *arg;
    int accept = 1;
    int i;

    f = fopen(fname, "r");
    if (!f)
        return;

    while (1)
    {
        s = fgets(buf, sizeof buf, f);
        if (!s)
            break;

        /* kill newline */
        if (strlen(buf) && buf[strlen(buf)-1] == '\n')
            buf[strlen(buf)-1] = 0;

        if (!strlen(buf) || buf[0] == '#')
            continue;

        if (strchr(buf,'['))
        {
            for (i = 0; i < strlen(buf); i++)
                buf[i] = tolower(buf[i]);

            if (strstr(buf, argv0) || strstr(buf, gamefile))
                accept = 1;
            else
                accept = 0;
        }

        if (!accept)
            continue;

        cmd = strtok(buf, "\r\n\t ");
        if (!cmd)
            continue;

        if (!strcmp(cmd, "tcolor") || !strcmp(cmd, "gcolor"))
            arg = strtok(NULL, "\r\n#");
        else if (!strcmp(cmd, "tfont") || !strcmp(cmd, "gfont"))
            arg = strtok(NULL, "\r\n#");
        else if ((!strcmp(cmd, "monofont") || !strcmp(cmd, "propfont")))
            arg = strtok(NULL, "\r\n#");
        else if ((!strncmp(cmd, "mono", 4) || !strncmp(cmd, "prop", 4)) && strlen(cmd) == 5)
            arg = strtok(NULL, "\r\n#");
        else if (!strcmp(cmd, "moreprompt"))
            arg = strtok(NULL, "\r\n");
        else
            arg = strtok(NULL, "\r\n\t #");
        if (!arg)
            continue;

        if (!strcmp(cmd, "moreprompt"))
            gli_more_prompt = strdup(arg);

        if (!strcmp(cmd, "morecolor"))
        {
            parsecolor(arg, gli_more_color);
            parsecolor(arg, gli_more_save);
        }

        if (!strcmp(cmd, "morefont"))
            gli_more_font = font2idx(arg);
        if (!strcmp(cmd, "morealign"))
            gli_more_align = atoi(arg);

        if (!strcmp(cmd, "monoaspect"))
            gli_conf_monoaspect = atof(arg);
        if (!strcmp(cmd, "propaspect"))
            gli_conf_propaspect = atof(arg);

        if (!strcmp(cmd, "monosize"))
            gli_conf_monosize = atof(arg);
        if (!strcmp(cmd, "monor"))
            gli_conf_monor = trim(strdup(arg));
        if (!strcmp(cmd, "monob"))
            gli_conf_monob = trim(strdup(arg));
        if (!strcmp(cmd, "monoi"))
            gli_conf_monoi = trim(strdup(arg));
        if (!strcmp(cmd, "monoz"))
            gli_conf_monoz = trim(strdup(arg));
        if (!strcmp(cmd, "monofont"))
            gli_conf_monofont = trim(strdup(arg));

        if (!strcmp(cmd, "propsize"))
            gli_conf_propsize = atof(arg);
        if (!strcmp(cmd, "propr"))
            gli_conf_propr = trim(strdup(arg));
        if (!strcmp(cmd, "propb"))
            gli_conf_propb = trim(strdup(arg));
        if (!strcmp(cmd, "propi"))
            gli_conf_propi = trim(strdup(arg));
        if (!strcmp(cmd, "propz"))
            gli_conf_propz = trim(strdup(arg));
        if (!strcmp(cmd, "propfont"))
            gli_conf_propfont = trim(strdup(arg));

        if (!strcmp(cmd, "leading"))
            gli_leading = atof(arg) + 0.5;
        if (!strcmp(cmd, "baseline"))
            gli_baseline = atof(arg) + 0.5;

        if (!strcmp(cmd, "rows"))
            gli_rows = atoi(arg);
        if (!strcmp(cmd, "cols"))
            gli_cols = atoi(arg);

        if (!strcmp(cmd, "minrows"))
        {
            int r = atoi(arg);
            if (gli_rows < r)
                gli_rows = r;
        }

        if (!strcmp(cmd, "maxrows"))
        {
            int r = atoi(arg);
            if (gli_rows > r)
                gli_rows = r;
        }

        if (!strcmp(cmd, "mincols"))
        {
            int c = atoi(arg);
            if (gli_cols < c)
                gli_cols = c;
        }

        if (!strcmp(cmd, "maxcols"))
        {
            int c = atoi(arg);
            if (gli_cols > c)
                gli_cols = c;
        }

        if (!strcmp(cmd, "lockrows"))
            gli_conf_lockrows = atoi(arg);
        if (!strcmp(cmd, "lockcols"))
            gli_conf_lockcols = atoi(arg);

        if (!strcmp(cmd, "wmarginx"))
        {
            gli_wmarginx = atoi(arg);
            gli_wmarginx_save = gli_wmarginx;
        }

        if (!strcmp(cmd, "wmarginy"))
        {
            gli_wmarginy = atoi(arg);
            gli_wmarginy_save = gli_wmarginy;
        }

        if (!strcmp(cmd, "wpaddingx")) gli_wpaddingx = atoi(arg);
        if (!strcmp(cmd, "wpaddingy")) gli_wpaddingy = atoi(arg);
        if (!strcmp(cmd, "wborderx")) gli_wborderx = atoi(arg);
        if (!strcmp(cmd, "wbordery")) gli_wbordery = atoi(arg);
        if (!strcmp(cmd, "tmarginx")) gli_tmarginx = atoi(arg);
        if (!strcmp(cmd, "tmarginy")) gli_tmarginy = atoi(arg);

        if (!strcmp(cmd, "gamma"))
            gli_conf_gamma = atof(arg);

        if (!strcmp(cmd, "caretcolor"))
        {
            parsecolor(arg, gli_caret_color);
            parsecolor(arg, gli_caret_save);
        }

        if (!strcmp(cmd, "linkcolor"))
        {
            parsecolor(arg, gli_link_color);
            parsecolor(arg, gli_link_save);
        }

        if (!strcmp(cmd, "bordercolor"))
        {
            parsecolor(arg, gli_border_color);
            parsecolor(arg, gli_border_save);
        }

        if (!strcmp(cmd, "windowcolor"))
        {
            parsecolor(arg, gli_window_color);
            parsecolor(arg, gli_window_save);
        }

        if (!strcmp(cmd, "lcd"))
            gli_conf_lcd = atoi(arg);

        if (!strcmp(cmd, "caretshape"))
            gli_caret_shape = atoi(arg);

        if (!strcmp(cmd, "linkstyle"))
            gli_link_style = atoi(arg) ? 1 : 0;

        if (!strcmp(cmd, "scrollwidth"))
            gli_scroll_width = atoi(arg);
        if (!strcmp(cmd, "scrollbg"))
            parsecolor(arg, gli_scroll_bg);
        if (!strcmp(cmd, "scrollfg"))
            parsecolor(arg, gli_scroll_fg);

        if (!strcmp(cmd, "justify"))
            gli_conf_justify = atoi(arg);
        if (!strcmp(cmd, "quotes"))
            gli_conf_quotes = atoi(arg);
        if (!strcmp(cmd, "dashes"))
            gli_conf_dashes = atoi(arg);
        if (!strcmp(cmd, "spaces"))
            gli_conf_spaces = atoi(arg);
        if (!strcmp(cmd, "caps"))
            gli_conf_caps = atoi(arg);

        if (!strcmp(cmd, "graphics"))
            gli_conf_graphics = atoi(arg);
        if (!strcmp(cmd, "sound"))
            gli_conf_sound = atoi(arg);
        if (!strcmp(cmd, "speak"))
            gli_conf_speak = atoi(arg);

        if (!strcmp(cmd, "stylehint"))
            gli_conf_stylehint = atoi(arg);

        if (!strcmp(cmd, "safeclicks"))
            gli_conf_safeclicks = atoi(arg);

        if (!strcmp(cmd, "tcolor") || !strcmp(cmd, "gcolor"))
        {
            char *style = strtok(arg, "\r\n\t ");
            char *fg = strtok(NULL, "\r\n\t ");
            char *bg = strtok(NULL, "\r\n\t ");
            int i = atoi(style);
            if (i < 0 || i >= style_NUMSTYLES)
                continue;
            if (!bg)
                bg = gli_window_color;
            if (cmd[0] == 't')
            {
                parsecolor(fg, gli_tstyles[i].fg);
                parsecolor(bg, gli_tstyles[i].bg);
            }
            else
            {
                parsecolor(fg, gli_gstyles[i].fg);
                parsecolor(bg, gli_gstyles[i].bg);
            }
        }

        if (!strcmp(cmd, "tfont") || !strcmp(cmd, "gfont"))
        {
            char *style = strtok(arg, "\r\n\t ");
            char *font = strtok(NULL, "\r\n\t ");
            int i = atoi(style);
            if (i < 0 || i >= style_NUMSTYLES)
                continue;
            if (cmd[0] == 't')
                gli_tstyles[i].font = font2idx(font);
            else
                gli_gstyles[i].font = font2idx(font);
        }
    }

    fclose(f);
}

void gli_read_config(int argc, char **argv)
{
    char gamefile[1024] = "default";
    char argv0[1024] = "default";
    char buf[1024];
    int i;
    char *tmp;

    /* load argv0 with name of executable without suffix */
    if (strrchr(argv[0], '\\'))
        strcpy(argv0, strrchr(argv[0], '\\') + 1);
    else if (strrchr(argv[0], '/'))
        strcpy(argv0, strrchr(argv[0], '/') + 1);
    else
        strcpy(argv0, argv[0]);
    if (strrchr(argv0, '.'))
        strrchr(argv0, '.')[0] = 0;

    for (i = 0; i < strlen(argv0); i++)
        argv0[i] = tolower(argv0[i]);

    /* load gamefile with basename of last argument */
    if (strrchr(argv[argc-1], '\\'))
        strcpy(gamefile, strrchr(argv[argc-1], '\\') + 1);
    else if (strrchr(argv[argc-1], '/'))
        strcpy(gamefile, strrchr(argv[argc-1], '/') + 1);
    else
        strcpy(gamefile, argv[argc-1]);

    for (i = 0; i < strlen(gamefile); i++)
        gamefile[i] = tolower(gamefile[i]);

    /* try all the usual config places */

#ifdef WIN32
    {
        char *s;
        strcpy(buf, argv[0]);
        s = strrchr(buf, '\\');
        if (s) *s = 0;
        strcat(buf, "/garglk.ini");
        readoneconfig(buf, argv0, gamefile);
    }
#else
    strcpy(buf, GARGLKINI);
    readoneconfig(buf, argv0, gamefile);
#endif

    if (getenv("GARGLK_INI"))
    {
        strcpy(buf, getenv("GARGLK_INI"));
        strcat(buf, "/garglk.ini");
        readoneconfig(buf, argv0, gamefile);
    }

    if (getenv("HOME"))
    {
        strcpy(buf, getenv("HOME"));
        strcat(buf, "/.garglkrc");
        readoneconfig(buf, argv0, gamefile);
        strcpy(buf, getenv("HOME"));
        strcat(buf, "/garglk.ini");
        readoneconfig(buf, argv0, gamefile);
    }

    if (getenv("XDG_CONFIG_HOME"))
    {
        strcpy(buf, getenv("XDG_CONFIG_HOME"));
        strcat(buf, "/.garglkrc");
        readoneconfig(buf, argv0, gamefile);
        strcpy(buf, getenv("XDG_CONFIG_HOME"));
        strcat(buf, "/garglk.ini");
        readoneconfig(buf, argv0, gamefile);
    }

    if (argc > 1)
    {
        strcpy(buf, argv[argc-1]);
        if (strrchr(buf, '\\'))
        {
            strcpy(strrchr(buf, '\\'), "\\garglk.ini");
            readoneconfig(buf, argv0, gamefile);
        }
        else if (strrchr(buf, '/'))
        {
            strcpy(strrchr(buf, '/'), "/garglk.ini");
            readoneconfig(buf, argv0, gamefile);
        }

        strcpy(buf, argv[argc-1]);
        if (strrchr(buf, '.'))
            strcpy(strrchr(buf, '.'), ".ini");
        else
            strcat(buf, ".ini");
        readoneconfig(buf, argv0, gamefile);
    }
}

strid_t glkunix_stream_open_pathname(char *pathname, glui32 textmode, glui32 rock)
{
    return gli_stream_open_pathname(pathname, (textmode != 0), rock);
}

void gli_startup(int argc, char *argv[])
{
    gli_baseline = 0;

    wininit(&argc, argv);

    if (argc > 1)
        glkunix_set_base_file(argv[argc-1]);

    gli_read_config(argc, argv);

    memcpy(gli_tstyles_def, gli_tstyles, sizeof(gli_tstyles_def));
    memcpy(gli_gstyles_def, gli_gstyles, sizeof(gli_gstyles_def));

    if (!gli_baseline)
        gli_baseline = gli_conf_propsize + 0.5;

#ifdef USETTS
    gli_initialize_tts();
    if (gli_conf_speak)
        gli_conf_quotes = 0;
#endif

    gli_initialize_misc();
    gli_initialize_fonts();
    gli_initialize_windows();
    gli_initialize_sound();

    winopen();
    gli_initialize_babel();
}
