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
  vmconmor.cpp - TADS 3 console input/output - MORE-enabled/text-only version
Function
  There are three possible configurations for the output formatter.
  For each configuration, there is one extra file that must be linked
  into the system when building an application; the choice of this
  file determines the configuration of the system.  The three choices
  are:

    Text only, MORE mode enabled - vmconmor.cpp
    Text only, OS-level MORE handling - vmconnom.cpp
    HTML mode - vmconhtm.cpp
Notes
  
Modified
  09/06/99 MJRoberts  - Creation
*/

#include "os.h"
#include "t3std.h"
#include "vmconsol.h"


/* ------------------------------------------------------------------------ */
/*
 *   This is a MORE-enabled configuration, so indicate that we should handle
 *   MORE mode in the formatter layer.  
 */
int CVmFormatter::formatter_more_mode() const
{
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   This is a non-HTML version, so we turn off the HTML-target flag 
 */
int CVmFormatter::get_init_html_target() const
{
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   This is a text-only (non-HTML) version, so the HTML start/end functions
 *   do nothing 
 */
void CVmFormatterMain::start_html_in_os()
{
}

void CVmFormatterMain::end_html_in_os()
{
}

/*
 *   This is a MORE-enabled configuration, so indicate that we handle line
 *   wrapping ourselves (not in the OS layer).
 */
int CVmFormatterMain::get_os_line_wrap()
{
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   This is a MORE-mode version, so show a MORE prompt ourselves 
 */
void CVmConsole::show_con_more_prompt(VMG0_)
{
    int done;
    int next_page;

    /* display the "MORE" prompt */
    disp_str_->print_to_os("[More]");
    disp_str_->flush_to_os();

    /* wait for an acceptable keystroke */
    for (done = FALSE ; !done ; )
    {
        os_event_info_t evt;

        /* get an event */
        switch(os_get_event(0, FALSE, &evt))
        {
        case OS_EVT_KEY:
            switch(evt.key[0])
            {
            case ' ':
                /* stop waiting, show one page */
                done = TRUE;
                next_page = TRUE;
                break;

            case '\r':
            case '\n':
            case 0x2028:                /* unicode line separator character */
                /* stop waiting, show one line */
                done = TRUE;
                next_page = FALSE;
                break;

            default:
                /* ignore any other keystrokes */
                break;
            }
            break;

        case OS_EVT_EOF:
            /* end of file - there's nothing to wait for now */
            done = TRUE;
            next_page = TRUE;

            /* stop showing [more] prompts, as the user can't respond */
            G_os_moremode = FALSE;
            break;

        default:
            /* ignore other events */
            break;
        }
    }

    /* 
     *   Remove the prompt from the screen by backing up and overwriting
     *   it with spaces.  (Note that this assumes that we're running in
     *   some kind of terminal or character mode with a fixed-pitch font;
     *   if that's not the case, the OS layer should be taking
     *   responsibility for pagination anyway, so this code shouldn't be
     *   in use in the first place.)  
     */
    disp_str_->print_to_os( "\r      \r" );

    /* 
     *   if they pressed the space key, it means that we should show an
     *   entire new page, so reset the line count to zero; otherwise,
     *   we'll want to display another MORE prompt at the very next line,
     *   so leave the line count alone 
     */
    if (next_page)
        disp_str_->reset_line_count(FALSE);
}

