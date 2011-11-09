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
#include <stdio.h>

#include "glk.h"
#include "garglk.h"
#include "garversion.h"
#include "launcher.h"

#include <Elementary.h>
#include <unistd.h>

static const char * AppName = "Gargoyle " VERSION;
static const char * LaunchingTemplate = "%s/%s";
static const char * DirSeparator = "/";

char dir[MaxBuffer];
char buf[MaxBuffer];
char tmp[MaxBuffer];
char brmsg[MaxBuffer];

char *filterlist[] =
{
"All Games|*.taf;*.agx;*.d[0-9][0-9];*.acd;*.a3c;*.asl;*.cas;*.ulx;*.hex;*.jacl;*.j2;*.gam;*.t3;*.z?;*.l9;*.sna;*.mag;*.dat;*.saga;*.blb;*.glb;*.zlb;*.blorb;*.gblorb;*.zblorb",
"Adrift Games (*.taf)|*.taf",
"AdvSys Games (*.dat)|*.dat",
"AGT Games (*.agx)|*.agx;*.d[0-9][0-9]",
"Alan Games (*.acd,*.a3c)|*.acd;*.a3c",
"Glulx Games (*.ulx)|*.ulx;*.blb;*.blorb;*.glb;*.gblorb",
"Hugo Games (*.hex)|*.hex",
"JACL Games (*.jacl,*.j2)|*.jacl;*.j2",
"Level 9 (*.l9)|*.l9;*.sna",
"Magnetic Scrolls (*.mag)|*.mag",
"Quest Games (*.asl,*.cas)|*.asl;*.cas",
"Scott Adams Grand Adventures (*.saga)|*saga",
"TADS 2 Games (*.gam)|*.gam;*.t3",
"TADS 3 Games (*.t3)|*.gam;*.t3",
"Z-code Games (*.z?)|*.z[0-9];*.zlb;*.zblorb",
"All Files|*",
};

const int filtercount = 15;

/*converts "hello\nworld" into "hello<br>world"*/
static void
newline_to_br(const char *str)
{
    brmsg[0] = '\0';
    
    char *p = strchr(str, '\n');
    while (p != NULL){
        strncat(brmsg, str, p - str);
        strcat(brmsg, "<br>");
        str = p + 1;
        p = strchr(str, '\n');
    }
    strcat(brmsg, str);
}

static void
my_win_del(void *data, Evas_Object *obj, void *event_info)
{
   elm_exit();
}

/*TODO: show a decent dialog, possibly with edje*/
void winmsg(const char * msg)
{
   Evas_Object *win, *bg, *bx, *lb;

   win = elm_win_add(NULL, AppName, ELM_WIN_BASIC);
   elm_win_title_set(win, AppName);
   evas_object_smart_callback_add(win, "delete,request", my_win_del, NULL);

   bg = elm_bg_add(win);
   elm_win_resize_object_add(win, bg);
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(bg);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, EVAS_HINT_FILL);

   lb = elm_label_add(win);
   newline_to_br(msg);
   elm_label_label_set(lb, brmsg);
   evas_object_size_hint_weight_set(lb, 0.0, 0.0);
   evas_object_size_hint_align_set(lb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(bx, lb);
   evas_object_show(lb);

   elm_win_resize_object_add(win, bx);
   evas_object_show(bx);
   
   evas_object_resize(win, 320, 100);

   evas_object_show(win);
   elm_run();
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
    elm_init( argc, argv );
    
    /* get dir of executable */
    winpath(dir);

    /* get story file.*/
    /* TODO: implement file filtering as in GTK version with Elementary supports it*/
    if (!winargs(argc, argv, buf))
        winopenfile( "an interactive fiction file", buf, sizeof(buf), 0 );

    if (!strlen(buf))
        return EXIT_FAILURE;

    /* run story file */
    return rungame(dir, buf);
}
