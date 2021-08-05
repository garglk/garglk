#ifdef RCSID
static char RCSid[] =
    "$Header$";
#endif

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osnet-fgwin.cpp - AllowSetForegroundWindow interface glue
Function
  Defines a cover function for AllowSetForegroundWindow that links
  dynamically to USER32.DLL, so that we can call the function where
  available but still work on older Windows versions that don't
  export the function.
Notes

Modified
  05/12/10 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>
#include "t3std.h"
#include "os.h"
#include "osifcnet.h"


/* ------------------------------------------------------------------------ */
/*
 *   Run-time imports, for functions that aren't available in all Windows
 *   version that we wish to support.  
 */
static BOOL (WINAPI *pAllowSetForegroundWindow)(DWORD) = 0;
static int checked_imports = FALSE;
static HINSTANCE hUser32 = 0;
static void check_imports()
{
    if (!checked_imports)
    {
        checked_imports = TRUE;
        if ((hUser32 = LoadLibrary("USER32.DLL")) != 0)
        {
            pAllowSetForegroundWindow =
                (BOOL (WINAPI *)(DWORD))GetProcAddress(
                    hUser32, "AllowSetForegroundWindow");
        }
    }
}

/* AllowSetForegroundWindow cover (it's only in Win2k and up) */
BOOL tads_AllowSetForegroundWindow(DWORD pid)
{
    check_imports();
    if (pAllowSetForegroundWindow != 0)
        return (*pAllowSetForegroundWindow)(pid);
    else
        return FALSE;
}

/* unlink USER32.DLL, if we loaded it */
void tads_unlink_user32()
{
    /* unload dynamically loaded libraries */
    if (hUser32 != 0)
        FreeLibrary(hUser32);
}
