/* Copyright (c) 2005 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osifcext.h - tads OS interface extensions
Function
  This header defines some extensions to the portable interfaces to the OS,
  beyond the interfaces provided in osifc.h.  These functions are optional
  extensions, for use in tads 3: platforms generally only need to implement
  these functions if the tads 3 tads-io-ext function set is used.
Notes
  
Modified
  03/02/05 MJRoberts  - Creation
*/

#ifndef OSIFCEXT_H
#define OSIFCEXT_H

/* if we're in C++ mode, explicitly specify C-style linkage */
#ifdef __cplusplus
extern "C" {
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Open a popup menu window.  The new window should be opened at the given
 *   x,y coordinates (given in pixels, relative to the top left of the main
 *   game window), and should be drawn in a style consistent with local
 *   system conventions for context menus.  If 'default_pos' is true, ignore
 *   the x,y coordinates and place the menu at a suitable default position,
 *   according to local conventions; for example, on Windows, the default
 *   position for a context menu is at the current mouse position.  The given
 *   HTML text is to be used as the contents of the new window.  The window
 *   should be sized to fit the contents.
 *   
 *   The window should have essentially the same lifetime as a context menu
 *   would have.  That is, if the user clicks the mouse outside of the popup,
 *   the popup should be automatically closed.  If the user clicks on a
 *   hyperlink in the popup, the hyperlink should fire its event as normal,
 *   and the popup window should be closed.
 *   
 *   If we successfully show the menu, and the user clicks on a hyperlink
 *   item from the menu, we'll return OSPOP_HREF, and we'll fill in *evt with
 *   the event information for the hyperlink selection.  If we successfully
 *   show the menu, but the user clicks outside the menu or otherwise cancels
 *   the menu without making a selection, we'll return OSPOP_CANCEL.  If we
 *   can't show the menu, we'll return OSPOP_FAIL.  If the application is
 *   shut down while we're showing the menu (because the user closes the app,
 *   for example, or because the system is shutting down) we'll return
 *   OSPOP_EOF.  
 */
int os_show_popup_menu(int default_pos, int x, int y,
                       const char *txt, size_t txtlen,
                       union os_event_info_t *evt);

#define OSPOP_FAIL    0                            /* can't show popup menu */
#define OSPOP_HREF    1          /* user clicked on a hyperlink in the menu */
#define OSPOP_CANCEL  2         /* user canceled without making a selection */
#define OSPOP_EOF     3                           /* application is closing */


/* close the extern "C" if we're in C++ mode */
#ifdef __cplusplus
}
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Enable/disable a System Menu Command event in os_get_event().
 *   
 *   Some interpreters provide menu commands for the common "system" commands
 *   that most games support, such as SAVE, RESTORE, UNDO, and QUIT.  The
 *   older interpreters didn't have any support for a menu interface, so the
 *   standard handling of these menu commands is a bit of a hack: we simply
 *   write the literal text of the command ("save", "restore", etc) into the
 *   command-line buffer, as though the player had actually typed the
 *   command, and then return the command line from os_gets().  This works
 *   fine as far as it goes, but it has one big restriction: there's no way
 *   to get these system command events from os_get_event().
 *   
 *   This function gives os_get_event() access to the system menu commands.
 *   The system commands can be enabled individually; this is important
 *   because it allows the GUI to show the commands as appropriately enabled
 *   or disabled in menus and toolbars and whatever other widgets are used.
 *   Once a command is enabled, then whenever we're blocked in a UI read in
 *   os_get_event(), the command will be shown as enabled in the visual UI;
 *   if the user then selects the command, it will be returned from
 *   os_get_event() via an OS_EVT_COMMAND event.
 *   
 *   'id' is an OS_CMD_xxx value identifying which event is to be enabled or
 *   disabled.  Once set, an event's status will remain as given until the
 *   next call to this routine with the same ID.
 *   
 *   'status' is a combination of OS_CMDSTAT_xxx flags specifying the event's
 *   status.  By default, all events are *disabled* for os_get_event(), and
 *   all events are *enabled* for os_gets().  You can control the
 *   os_get_event() and os_gets() status of the event separately.  If you
 *   don't include an OS_CMDSTAT_EVT_xxx bit in the flags, the current
 *   os_get_event() status is left unchanged; likewise, if you don't include
 *   an OS_CMDSTAT_GETS_xxx bit, the os_gets() status is left unchanged.  
 */
void os_enable_cmd_event(int id, unsigned int status);

/* ------------------------------------------------------------------------ */
/* 
 *   Command status codes for os_enable_cmd_event.
 *   
 *   Note that if the ENA and DIS bit for the same type are both set, the ENA
 *   takes precedence.
 *   
 *   (OS implementors: note that each ENA/DIS pair consists of a "select" bit
 *   and an "enable" bit.  If the "select" bit for a pair is missing, then
 *   you don't need to update the corresponding status bit.  If the "select"
 *   bit is present, then simply set the corresponding status bit to the
 *   "enable" bit value.)  
 */

/* enable/disable the event for os_get_event() */
#define OS_CMDSTAT_EVT_ENA   (OSCS_EVT_SEL | OSCS_EVT_ON)
#define OS_CMDSTAT_EVT_DIS   (OSCS_EVT_SEL | 0x0000)

/* enable/disable the event for os_gets() */
#define OS_CMDSTAT_GETS_ENA  (OSCS_GETS_SEL | OSCS_GETS_ON)
#define OS_CMDSTAT_GETS_DIS  (OSCS_GETS_SEL | 0x0000)

/* internal use: SELECT and ON bits for OS_CMDSTAT_xxx_yyy */
#define OSCS_EVT_SEL   0x0001
#define OSCS_EVT_ON    0x0002
#define OSCS_GTS_SEL   0x0004
#define OSCS_GTS_ON    0x0008


#endif /* OSIFCEXT_H */

