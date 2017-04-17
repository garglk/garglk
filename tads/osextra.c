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

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "os.h"

/*
 * This file contains extra OS-specific functionality which is required
 * but is not part of osportable.cc.
 */

/*
 *   Determine if a path is absolute or relative.  If the path starts with
 *   a path separator character, we consider it absolute, otherwise we
 *   consider it relative.
 *   
 *   Note that, on Windows, an absolute path can also follow a drive letter.
 *   So, if the path contains a letter followed by a colon, we'll consider
 *   the path to be absolute. 
 *
 *   This is provided because the noui version only knows about DOS, not
 *   Windows, so will not handle drive letters properly, assuming
 *   something like C:/foo is not absoolute. The macro
 *   OSNOUI_OMIT_IS_FILE_ABSOLUTE must be defined so that the noui version
 *   is not built.
 */
int os_is_file_absolute(const char *fname)
{
    /* If the name starts with a path separator, it's absolute. */
    if (fname[0] == OSPATHCHAR || strchr(OSPATHALT, fname[0]) != NULL)
        return TRUE;

#ifdef _WIN32
    /* On Windows, a file is absolute if it starts with a drive letter. */
    if (isalpha((unsigned char)fname[0]) && fname[1] == ':')
        return TRUE;
#endif /* _WIN32 */

    /* The path is relative. */
    return FALSE;
}
