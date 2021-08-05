#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  askf_os.cpp - OS-dialog-based implementation of askfile
Function
  Implements file dialog using the OS-provided file dialog
Notes
  Only one of askf_tx.c or askf_os.c should be included in a given
  executable.  For a text-only version, include askf_tx.  For a version
  where os_askfile() provides a file dialog, use askf_os instead.

  We provide a choice of tio_askfile() implementations in the portable
  code (rather than only through the OS code) so that we can call
  the formatted text output routines in this version.  An OS-layer
  implementation could not call the formatted output routines (it would
  have to call os_printf directly), which would result in poor prompt
  formatting any time a prompt exceeded a single line of text.
Modified
  09/27/99 MJRoberts  - Creation
*/


#include "os.h"
#include "t3std.h"
#include "vmglob.h"
#include "vmconsol.h"
#include "charmap.h"

/*
 *   formatted text-only file prompt 
 */
int CVmConsole::askfile(VMG_ const char *prompt, size_t prompt_len,
                        char *reply, size_t replen,
                        int dialog_type, os_filetype_t file_type,
                        int /*bypass_script*/)
{
    int result;
    size_t prompt_ui_len;
    char prompt_ui[256];
    char fname[OSFNMAX];

    /* flush any pending output */
    flush(vmg_ VM_NL_INPUT);
    
    /* translate the prompt to the local UI character set */
    prompt_ui_len = G_cmap_to_ui->map_utf8(prompt_ui, sizeof(prompt_ui) - 1,
                                           prompt, prompt_len, 0);
    
    /* null-terminate the translated prompt */
    prompt_ui[prompt_ui_len] = '\0';
    
    /* let the system handle it */
    result = os_askfile(prompt, fname, sizeof(fname),
                        dialog_type, file_type);

    /* if we were successful, translate the filename to UTF-8 */
    if (result == OS_AFE_SUCCESS)
    {
        char *dst;
        size_t dstlen;
        
        /* 
         *   The result string is in the local UI character set, so we
         *   must translate it to UTF-8 for the caller.  Note that we
         *   reserve one byte of the result buffer for the null
         *   terminator.  
         */
        dst = reply;
        dstlen = replen - 1;
        G_cmap_from_ui->map(&dst, &dstlen, fname, strlen(fname));

        /* null-terminate the result */
        *dst = '\0';
    }

    /* return the result */
    return result;
}

