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
  vmconnom.cpp - TADS 3 console input/output - OS-level MORE/text-only version
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
 *   This version uses OS-level MORE prompting, so indicate that we do not
 *   handle the MORE prompt in the output formatter 
 */
int CVmFormatter::formatter_more_mode() const
{
    return FALSE;
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
 *   The OS layer handles line wrapping in this version
 */
int CVmFormatterMain::get_os_line_wrap()
{
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   This version handles MORE prompting in the OS code, so call the OS
 *   code when we need to display a MORE prompt 
 */
void CVmConsole::show_con_more_prompt(VMG0_)
{
    os_more_prompt();
}

