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
  indlg_os.cpp - OS-dialog-based implementation of input_dialog
Function
  Implements the input dialog using the OS-provided dialog
Notes
  Only one of indlg_tx.c or indlg_os.c should be included in a given
  executable.  For a text-only version, include indlg_tx.  For a version
  where os_input_dialog() provides a system dialog, use indlg_os instead.

  We provide a choice of input_dialog() implementations in the portable
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
int CVmConsole::input_dialog(VMG_ int icon_id, const char *prompt,
                             int standard_button_set,
                             const char **buttons, int button_count,
                             int default_index, int cancel_index,
                             int bypass_script)
{
    char ui_prompt[256];
    const char *ui_buttons[10];
    char ui_button_buf[256];
    size_t ui_prompt_len;
    char *dst;
    size_t dstrem;
    int i;
    
    /* flush any pending output */
    flush(vmg_ VM_NL_INPUT);

    /* convert the prompt to the local character set */
    ui_prompt_len = G_cmap_to_ui->map_utf8(ui_prompt, sizeof(ui_prompt) - 1,    
                                           prompt, strlen(prompt), 0);

    /* null-terminate the prompt text */
    ui_prompt[ui_prompt_len] = '\0';

    /* convert the button labels to the local character set */
    if (button_count > (int)sizeof(ui_buttons)/sizeof(ui_buttons[0]))
        button_count = (int)sizeof(ui_buttons)/sizeof(ui_buttons[0]);
    for (i = 0, dst = ui_button_buf, dstrem = sizeof(ui_button_buf) ;
         i < button_count ; ++i)
    {
        size_t len;
        
        /* set up this converted button pointer */
        ui_buttons[i] = dst;

        /* map this button's text, leaving room for a null terminator */
        len = G_cmap_to_ui->map_utf8(dst, dstrem - 1,
                                     buttons[i], strlen(buttons[i]), 0);

        /* null-terminate it */
        dst[len++] = '\0';

        /* advance past this label */
        dst += len;
        dstrem -= len;
    }

    /* ask the OS to do the work, and return the result */
    return os_input_dialog(icon_id, ui_prompt, standard_button_set,
                           ui_buttons, button_count,
                           default_index, cancel_index);
}

