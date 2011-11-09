/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2009 by Baltasar GarcÌa Perez-Schofield.                     *
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
#include "garversion.h"
#include "launcher.h"

#include <gtk/gtk.h>
#include <unistd.h>

static const char * AppName = "Gargoyle " VERSION;
static const char * LaunchingTemplate = "%s/%s";
static const char * DirSeparator = "/";

char dir[MaxBuffer];
char buf[MaxBuffer];
char tmp[MaxBuffer];

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

void winstart(void)
{
    gtk_init(NULL, NULL);
}

void winmsg(const char * msg)
{
    GtkWidget * msgDlg = gtk_message_dialog_new(NULL,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", msg
    );

    gtk_window_set_title(GTK_WINDOW(msgDlg), AppName);
    gtk_dialog_run(GTK_DIALOG(msgDlg ));
    gtk_widget_destroy(msgDlg);
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

void winfilteradd(GtkFileChooser *dialog, const char *name, const char *pattern)
{
    gchar **patterns;
    gint i;

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, name);

    patterns = g_strsplit(pattern, ";", -1);
    for(i = 0; patterns[i] != NULL; i++)
        gtk_file_filter_add_pattern(filter, patterns[i]);
    g_strfreev(patterns);

    gtk_file_chooser_add_filter(dialog, filter);
}

void winfilterfiles(GtkFileChooser *dialog)
{
    gchar **format;
    gint i;

    for (i = 0; i < filtercount; i++)
    {
        format = g_strsplit(filterlist[i], "|", -1);
        if (format[0] != NULL && format[1] != NULL)
            winfilteradd(dialog, format[0], format[1]);
        g_strfreev(format);
    }
}

void winbrowsefile(char *buffer)
{
    *buffer = 0;

    GtkWidget * openDlg = gtk_file_chooser_dialog_new(AppName, NULL,
                                                      GTK_FILE_CHOOSER_ACTION_OPEN,
                                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                      NULL);

    if (getenv("GAMES"))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(openDlg), getenv("GAMES"));
    else if (getenv("HOME"))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(openDlg), getenv("HOME"));

    winfilterfiles(GTK_FILE_CHOOSER(openDlg));

    gint result = gtk_dialog_run(GTK_DIALOG(openDlg));

    if (result == GTK_RESPONSE_ACCEPT)
        strcpy(buffer, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(openDlg)));

    gtk_widget_destroy(openDlg);
    gdk_flush();
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

    setenv("GARGLK_INI", path, FALSE);

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
        return FALSE;
    }

    return TRUE;
}

int main(int argc, char **argv)
{
    winstart();

    /* get dir of executable */
    winpath(dir);

    /* get story file */
    if (!winargs(argc, argv, buf))
        winbrowsefile(buf);

    if (!strlen(buf))
        return TRUE;

    /* run story file */
    return rungame(dir, buf);
}
