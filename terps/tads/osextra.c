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

#ifdef _WIN32
#include <windows.h>
#endif

#include "os.h"
#include "osextra.h"

/*
 * This file contains extra OS-specific functionality which is required
 * but is not part of osportable.cc.
 */

#ifdef _WIN32
/*
 *   Copied and adapted from tads2/osnoui.c.
 *   Get the DOS attributes for a file
 *
 *   We don't use either of the versions provided in tads2/osnoui.c directly,
 *   even though the version provided for the MICROSOFT compiler is pretty
 *   much what we need, because defining the MICROSOFT precompiler symbol
 *   would include a bunch of other stuff that we don't necessarily want.
 */
unsigned long oss_get_file_attrs(const char *fname)
{
    unsigned long attrs = 0;
    
    /* get the DOS attribute flags */
    DWORD dos_attrib = GetFileAttributes(fname);

    /* translate the HIDDEN and SYSTEM bits */
    if ((dos_attrib & _A_HIDDEN) != 0)
        attrs |= OSFATTR_HIDDEN;
    if ((dos_attrib & _A_SYSTEM) != 0)
        attrs |= OSFATTR_SYSTEM;

    /* if the RDONLY bit is set, it's readable, otherwise read/write */
    attrs |= OSFATTR_READ;
    if ((dos_attrib & _A_RDONLY) == 0)
        attrs |= OSFATTR_WRITE;

    /*
     *   On Windows, attempt to do a full security manager check to determine
     *   our actual access rights to the file.  If we fail to get the
     *   security info, we'll give up and return the simple RDONLY
     *   determination we just made above.  In most cases, failure to check
     *   the security information will simply be because the file has no ACL,
     *   which means that anyone can access it, hence our tentative result
     *   from the RDONLY attribute is correct after all.
     */
    {
        /* 
         *   get the file's DACL and owner/group security info; first, ask
         *   how much space we need to allocate for the returned information 
         */
        DWORD len = 0;
        SECURITY_INFORMATION info = (SECURITY_INFORMATION)(
            OWNER_SECURITY_INFORMATION
            | GROUP_SECURITY_INFORMATION
            | DACL_SECURITY_INFORMATION);
        GetFileSecurity(fname, info, 0, 0, &len);
        if (len != 0)
        {
            /* allocate a buffer for the security info */
            SECURITY_DESCRIPTOR *sd = (SECURITY_DESCRIPTOR *)malloc(len);
            if (sd != 0)
            {
                /* now actually retrieve the security info into our buffer */
                if (GetFileSecurity(fname, info, sd, len, &len))
                {
                    HANDLE ttok;

                    /* impersonate myself for security purposes */
                    ImpersonateSelf(SecurityImpersonation);

                    /* 
                     *   get the security token for the current thread, which
                     *   is the context in which the caller will presumably
                     *   eventually attempt to access the file 
                     */
                    if (OpenThreadToken(
                        GetCurrentThread(), TOKEN_ALL_ACCESS, TRUE, &ttok))
                    {
                        GENERIC_MAPPING genmap = {
                            FILE_GENERIC_READ,
                            FILE_GENERIC_WRITE,
                            FILE_GENERIC_EXECUTE,
                            FILE_ALL_ACCESS
                        };
                        PRIVILEGE_SET privs;
                        DWORD privlen = sizeof(privs);
                        BOOL granted = FALSE;
                        DWORD desired;
                        DWORD allowed;

                        /* test read access */
                        desired = GENERIC_READ;
                        MapGenericMask(&desired, &genmap);
                        if (AccessCheck(sd, ttok, desired, &genmap,
                                        &privs, &privlen, &allowed, &granted))
                        {
                            /* clear the read bit if reading isn't allowed */
                            if (allowed == 0)
                                attrs &= ~OSFATTR_READ;
                        }

                        /* test write access */
                        desired = GENERIC_WRITE;
                        MapGenericMask(&desired, &genmap);
                        if (AccessCheck(sd, ttok, desired, &genmap,
                                        &privs, &privlen, &allowed, &granted))
                        {
                            /* clear the read bit if reading isn't allowed */
                            if (allowed == 0)
                                attrs &= ~OSFATTR_WRITE;
                        }

                        /* done with the thread token */
                        CloseHandle(ttok);
                    }

                    /* terminate our security self-impersonation */
                    RevertToSelf();
                }

                /* free the allocated security info buffer */
                free(sd);
            }
        }
    }

    /* return the attributes */
    return attrs;
}
#endif /* _WIN32 */
