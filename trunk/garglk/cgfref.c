/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Andrew Plotkin.                  *
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

/* cgfref.c: Fileref functions for Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html

    Portions of this file are copyright 1998-2004 by Andrew Plotkin.
    You may copy, distribute, and incorporate it into your own programs,
    by any means and under any conditions, as long as you do not modify it.
    You may also modify this file, incorporate it into your own programs,
    and distribute the modified version, as long as you retain a notice
    in your program or documentation which mentions my name and the URL
    shown above.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h> /* for unlink() */
#endif
#include <sys/stat.h> /* for stat() */
#include "glk.h"
#include "garglk.h"

char gli_workdir[1024] = ".";
char gli_workfile[1024] = "";

char *garglk_fileref_get_name(fileref_t *fref)
{
    return fref->filename;
}

/* This file implements filerefs as they work in a stdio system: a
    fileref contains a pathname, a text/binary flag, and a file
    type.
*/

/* Linked list of all filerefs */
static fileref_t *gli_filereflist = NULL; 

fileref_t *gli_new_fileref(char *filename, glui32 usage, glui32 rock)
{
    fileref_t *fref = (fileref_t *)malloc(sizeof(fileref_t));
    if (!fref)
        return NULL;

    fref->magicnum = MAGIC_FILEREF_NUM;
    fref->rock = rock;

    fref->filename = malloc(1 + strlen(filename));
    strcpy(fref->filename, filename);

    fref->textmode = ((usage & fileusage_TextMode) != 0);
    fref->filetype = (usage & fileusage_TypeMask);

    fref->prev = NULL;
    fref->next = gli_filereflist;
    gli_filereflist = fref;
    if (fref->next)
        fref->next->prev = fref;

    if (gli_register_obj)
        fref->disprock = (*gli_register_obj)(fref, gidisp_Class_Fileref);
    else
        fref->disprock.ptr = NULL;

    return fref;
}

void gli_delete_fileref(fileref_t *fref)
{
    fileref_t *prev, *next;

    if (gli_unregister_obj)
    {
        (*gli_unregister_obj)(fref, gidisp_Class_Fileref, fref->disprock);
        fref->disprock.ptr = NULL;
    }

    fref->magicnum = 0;

    if (fref->filename)
    {
        free(fref->filename);
        fref->filename = NULL;
    }

    prev = fref->prev;
    next = fref->next;
    fref->prev = NULL;
    fref->next = NULL;

    if (prev)
        prev->next = next;
    else
        gli_filereflist = next;
    if (next)
        next->prev = prev;

    free(fref);
}

void glk_fileref_destroy(fileref_t *fref)
{
    if (!fref)
    {
        gli_strict_warning("fileref_destroy: invalid ref");
        return;
    }
    gli_delete_fileref(fref);
}

frefid_t glk_fileref_create_temp(glui32 usage, glui32 rock)
{
    char *filename;
    fileref_t *fref;
    char *tempdir = getenv("TEMP");
    if (tempdir == NULL) tempdir = ".";
    filename = tempnam(tempdir, "gargtmp");

    fref = gli_new_fileref(filename, usage, rock);
    if (!fref)
    {
        gli_strict_warning("fileref_create_temp: unable to create fileref.");
        return NULL;
    }

    return fref;
}

frefid_t glk_fileref_create_from_fileref(glui32 usage, frefid_t oldfref,
    glui32 rock)
{
    fileref_t *fref; 

    if (!oldfref)
    {
        gli_strict_warning("fileref_create_from_fileref: invalid ref");
        return NULL;
    }

    fref = gli_new_fileref(oldfref->filename, usage, rock);
    if (!fref)
    {
        gli_strict_warning("fileref_create_from_fileref: unable to create fileref.");
        return NULL;
    }

    return fref;
}

frefid_t glk_fileref_create_by_name(glui32 usage, char *name,
    glui32 rock)
{
    fileref_t *fref;
    char buf[256];
    char buf2[256];
    int len;
    char *cx;

    len = strlen(name);
    if (len > 255)
        len = 255;

    /* Take out all dangerous characters, and make sure the length is greater 
        than zero.  The overall goal is to make a legal 
        platform-native filename, without any extra directory 
        components.
       Suffixes are another sore point. Really, the game program 
        shouldn't have a suffix on the name passed to this function. So
        in DOS/Windows, this function should chop off dot-and-suffix,
        if there is one, and then add a dot and a three-letter suffix
        appropriate to the file type (as gleaned from the usage 
        argument.)
    */

    memcpy(buf, name, len);
    if (len == 0)
    {
        buf[0] = 'X';
        len++;
    }
    buf[len] = '\0';

    for (cx=buf; *cx; cx++)
    {
        if (*cx == '/' || *cx == '\\' || *cx == ':')
            *cx = '-';
    }

    sprintf(buf2, "%s/%s", gli_workdir, buf);

    fref = gli_new_fileref(buf2, usage, rock);
    if (!fref)
    {
        gli_strict_warning("fileref_create_by_name: unable to create fileref.");
        return NULL;
    }

    return fref;
}

frefid_t glk_fileref_create_by_prompt(glui32 usage, glui32 fmode, glui32 rock)
{
    fileref_t *fref;
    char buf[256];
    int val, filter;
    char *prompt;

    strcpy(buf, "");

    switch (usage & fileusage_TypeMask)
    {
        case fileusage_SavedGame:
            prompt = "Saved game";
            filter = FILTER_SAVE;
            break;
        case fileusage_Transcript:
            prompt = "Transcript file";
            filter = FILTER_TEXT;
            break;
        case fileusage_InputRecord:
            prompt = "Command record file";
            filter = FILTER_TEXT;
            break;
        case fileusage_Data:
        default:
            prompt = "Data file";
            filter = FILTER_ALL;
            break;
    }

    if (fmode == filemode_Read)
        winopenfile(prompt, buf, sizeof buf, filter);
    else
        winsavefile(prompt, buf, sizeof buf, filter);

    val = strlen(buf);
    if (!val)
    {
        /* The player just hit return. It would be nice to provide a
            default value, but this implementation is too cheap. */
        return NULL;
    }

    fref = gli_new_fileref(buf, usage, rock);
    if (!fref)
    {
        gli_strict_warning("fileref_create_by_prompt: unable to create fileref.");
        return NULL;
    }

    return fref;
}

frefid_t glk_fileref_iterate(fileref_t *fref, glui32 *rock)
{
    if (!fref)
        fref = gli_filereflist;
    else
        fref = fref->next;

    if (fref)
    {
        if (rock)
            *rock = fref->rock;
        return fref;
    }

    if (rock)
        *rock = 0;
    return NULL;
}

glui32 glk_fileref_get_rock(fileref_t *fref)
{
    if (!fref)
    {
        gli_strict_warning("fileref_get_rock: invalid ref.");
        return 0;
    }

    return fref->rock;
}

glui32 glk_fileref_does_file_exist(fileref_t *fref)
{
    struct stat buf;

    if (!fref)
    {
        gli_strict_warning("fileref_does_file_exist: invalid ref");
        return FALSE;
    }

    /* This is sort of Unix-specific, but probably any stdio library
        will implement at least this much of stat(). */

    if (stat(fref->filename, &buf))
        return 0;

#ifdef S_ISREG
    if (S_ISREG(buf.st_mode))
#else
    if (buf.st_mode & _S_IFREG)
#endif
        return 1;
    else
        return 0;
}

void glk_fileref_delete_file(fileref_t *fref)
{
    if (!fref)
    {
        gli_strict_warning("fileref_delete_file: invalid ref");
        return;
    }

    /* If you don't have the unlink() function, obviously, change it
        to whatever file-deletion function you do have. */

    unlink(fref->filename);
}

/* This should only be called from startup code. */
void glkunix_set_base_file(char *filename)
{
    strcpy(gli_workdir, filename);
    if (strrchr(gli_workdir, '/'))
        strrchr(gli_workdir, '/')[0] = 0;
    else if (strrchr(gli_workdir, '\\'))
        strrchr(gli_workdir, '\\')[0] = 0;
    else
        strcpy(gli_workdir, ".");
    strcpy(gli_workfile, filename);
    chdir(gli_workdir);
}
