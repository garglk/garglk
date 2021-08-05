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
  askf_tx.cpp - formatted text implementation of askfile
Function
  Implements file dialog using text prompts.
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

/*
 *   formatted text-only file prompt 
 */
int CVmConsole::askfile(VMG_ const char *prompt, size_t prompt_len,
                        char *reply, size_t replen,
                        int /*dialog_type*/, os_filetype_t /*file_type*/,
                        int bypass_script)
{
    /* show the prompt */
    format_text(vmg_ prompt, prompt_len);
    format_text(vmg_ " >");

    /* ask for the filename */
    if (read_line(vmg_ reply, replen, bypass_script))
        return OS_AFE_FAILURE;

    /* 
     *   if they entered an empty line, return "cancel"; otherwise, return
     *   success 
     */
    return (reply[0] == '\0' ? OS_AFE_CANCEL : OS_AFE_SUCCESS);
}

