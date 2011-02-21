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

#include <windows.h>
#include <commdlg.h>

static const char * AppName = "Gargoyle " VERSION;
static const char * LaunchingTemplate = "\"%s\\%s.exe\" %s \"%s\"";
static const char * DirSeparator = "\\";

char dir[MaxBuffer];
char buf[MaxBuffer];
char tmp[MaxBuffer];

char filterlist[] =
"All Games\0*.taf;*.agx;*.d$$;*.acd;*.a3c;*.asl;*.cas;*.ulx;*.hex;*.jacl;*.j2;*.gam;*.t3;*.z?;*.l9;*.sna;*.mag;*.dat;*.saga;*.blb;*.glb;*.zlb;*.blorb;*.gblorb;*.zblorb\0"
"Adrift Games (*.taf)\0*.taf\0"
"AdvSys Games (*.dat)\0*.dat\0"
"AGT Games (*.agx)\0*.agx;*.d$$\0"
"Alan Games (*.acd,*.a3c)\0*.acd;*.a3c\0"
"Glulx Games (*.ulx)\0*.ulx;*.blb;*.blorb;*.glb;*.gblorb\0"
"Hugo Games (*.hex)\0*.hex\0"
"JACL Games (*.jacl,*.j2)\0*.jacl;*.j2\0"
"Level 9 (*.l9)\0*.l9;*.sna\0"
"Magnetic Scrolls (*.mag)\0*.mag\0"
"Quest Games (*.asl,*.cas)\0*.asl;*.cas\0"
"Scott Adams Grand Adventures (*.saga)\0*.saga\0"
"TADS 2 Games (*.gam)\0*.gam;*.t3\0"
"TADS 3 Games (*.t3)\0*.gam;*.t3\0"
"Z-code Games (*.z?)\0*.z?;*.zlb;*.zblorb\0"
"All Files\0*\0"
"\0\0";

void winstart(void)
{
}

void winmsg(const char *msg)
{
    MessageBox(NULL, msg, AppName, MB_ICONERROR);
}

int urldecode(char *decoded, unsigned int maxlen, const char *encoded)
{
    unsigned int i;
    int convert, ascii;
    char buffer[3] = {0};
    FILE * file;

    memset(decoded, 0, maxlen);
    convert = FALSE;

    for(i = 0; i < strlen(encoded); ++i)
    {
        if (!convert)
        {
            if (!(strlen(decoded) < maxlen - 1))
                break;

            if(encoded[i] != '%')
                strncat(decoded, &encoded[i], 1);
            else
                convert = TRUE;
         }

        else
        {
            if (strlen(&encoded[i]) < 2)
                break;

            strncpy(buffer, &encoded[i], 2);
            buffer[2] = '\0';

            if (!isxdigit(buffer[0]) || !isxdigit(buffer[1]))
                break;

            if (sscanf(buffer, "%x", &ascii) != 1)
                break;

            if (!(strlen(decoded) < maxlen - 1))
                break;

            strncat(decoded, (char*)&ascii, 1);

            convert = FALSE;
            i++;
        }
    }

    while (strlen(decoded) && decoded[strlen(decoded)-1] == '/')
        decoded[strlen(decoded)-1] = '\0';

    if (!(file = fopen(decoded, "r")))
    {
        snprintf(decoded, maxlen, "Could not open URL path:\n%s\n", encoded);
        winmsg(decoded);
        exit(EXIT_FAILURE);
    }

    fclose(file);
    return 1;
}

int winargs(int argc, char **argv, char *buffer)
{
    if (argc == 2)
    {
        if (!(strlen(argv[1]) < MaxBuffer - 1))
            return 0;

        if (strstr(argv[1], "garglk:///"))
            return urldecode(buffer, MaxBuffer, argv[1]+10);

        strcpy(buffer, argv[1]);
    }

    return (argc == 2);
}

void winbrowsefile(char *buffer)
{
    *buffer = 0;

    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MaxBuffer;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = AppName;
    ofn.lpstrFilter = filterlist;
    ofn.Flags = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;

    GetOpenFileName(&ofn);
}

void winpath(char *buffer)
{
    char exepath[MaxBuffer] = {0};
    unsigned int exelen;

    exelen = GetModuleFileName(NULL, exepath, sizeof(exepath));

    if (exelen <= 0 || exelen >= MaxBuffer) {
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
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    int res;

    memset(&si, 0, sizeof si);
    memset(&pi, 0, sizeof pi);

    si.cb = sizeof si;

    sprintf(tmp,"%s",cmd);

    res = CreateProcess(
        NULL,	/* no module name (use cmd line)    */
        tmp,	/* command line                     */
        NULL,	/* security attrs,                  */
        NULL,	/* thread attrs,                    */
        FALSE,	/* inherithandles                   */
        0,		/* creation flags                   */
        NULL,	/* environ                          */
        NULL,	/* cwd                              */
        &si,	/* startupinfo                      */
        &pi		/* procinfo                         */
    );

    return (res != 0);
}

int winterp(char *path, char *exe, char *flags, char *game)
{
    sprintf(tmp, LaunchingTemplate, path, exe, flags, game);

    if (!winexec(tmp, NULL)) {
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
