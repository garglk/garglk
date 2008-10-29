/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osgen.h - definitions for the osgen subsystem
Function
  
Notes
  
Modified
  05/19/02 MJRoberts  - Creation
*/

#ifndef OSGEN_H
#define OSGEN_H

/* ------------------------------------------------------------------------ */
/*
 *   osgen support routines.  These routines are implemented by the
 *   semi-portable osgen.c layer for use by the underlying osxxx code.  
 */

/*
 *   Initialize the osgen scrollback buffer.  Allocate the given amount of
 *   space in bytes for scrollback.
 */
void osssbini(unsigned int size);

/*
 *   Delete the scrollback buffer.  This should be called during program
 *   termination to release memory allocated by osssbini().
 */
void osssbdel(void);


/*
 *   The size, in character cells, of the ENTIRE SCREEN (or, at least, the
 *   entire part of the screen available for our use).  Note that these
 *   variables reflect the size of the entire screen - these are NOT
 *   adjusted for status lines, banners, or anything else.
 *   
 *   The system-specific "oss" code MUST initialize these variables during
 *   startup, and MUST then keep them up to date whenever the screen size
 *   changes (for example, if we're running in a terminal window, and the
 *   user resizes the window, these must be updated to reflect the new
 *   window size).  IN ADDITION, whenever the display size changes after
 *   start-up, the "oss" code MUST call osssb_on_resize_screen() to notify
 *   the osgen layer of the change.
 *   
 *   IMPORTANT: In the past, the "oss" code was responsible for initializing
 *   and maintaining the global variables G_os_linewidth and
 *   G_os_pagelength.  The "oss" code need not do this any more - in fact,
 *   the OS code MUST STOP setting G_os_pagelength and G_os_linewidth.
 *   Change these variables instead.  
 */
extern int G_oss_screen_width;
extern int G_oss_screen_height;

/* 
 *   the "oss" code MUST call this routine whenever G_os_screen_width or
 *   G_os_screen_height change (except during initialization) 
 */
void osssb_on_resize_screen(void);

#ifdef RUNTIME

/*
 *   Redraw the screen if necessary.  The oss code MUST call this routine
 *   whenever it's going to pause for a timed wait, or stop to wait for user
 *   input.  Specifically, this should be called from the OS implementation
 *   of the following routines:
 *   
 *   os_getc()
 *.  os_getc_raw()
 *.  os_waitc()
 *.  os_get_event()
 *.  os_askfile();
 *.  os_input_dialog()
 *.  os_sleep_ms()
 *.  os_yield();
 */
void osssb_redraw_if_needed(void);

/*
 *   Put the cursor at the default position.  The oss code CAN optionally
 *   call this at the start of routines that pause for input to put the
 *   cursor at the default position in the main window.  Depending on how the
 *   oss code implements text drawing, the osgen code can sometimes leave the
 *   cursor in a banner window rather than in the main window after drawing.
 *   This routine will ensure that the cursor is in the main window at the
 *   end of the main window's text.  
 */
void osssb_cursor_to_default_pos(void);

/*
 *   Determine if stdin is at end-of-file.  If possible, this should return
 *   the stdin status even if we've never attempted to read stdin.
 *   
 *   For most Unix systems, this can return feof(stdin).  With some C
 *   libraries, however, this doesn't work until after an attempt has already
 *   been made to read from stdin, so we provide this cover to allow flexible
 *   implementation on different systems.  On Windows, for example, it's
 *   necessary to use _feof(_fileno(stdin)), because the higher-level feof()
 *   doesn't recognize EOF until the handle has been read from.  
 */
int oss_eof_on_stdin(void);

#else /* RUNTIME */

/* in non-RUNTIME mode, we don't use osssb_redraw_if_needed at all */
#define osssb_redraw_if_needed()

/* in non-RUNTIME mode, we don't need to reposition the cursor */
#define osssb_cursor_to_default_pos()

#endif /* RUNTIME */



/* ------------------------------------------------------------------------ */
/*
 *   ossxxx implementation for osgen.  These routines must be defined by the
 *   OS-specific subsystem. 
 */

/*
 *   Get extended system information.  This checks to see if the ossxxx
 *   implementation handles the system information code.  If the osxxx layer
 *   doesn't want to handle the code, it can return false so that the osgen
 *   layer will provide a default interpretation.
 *   
 *   Currently, the following codes should be handled by the oss layer:
 *   
 *   SYSINFO_TEXT_COLORS - return the level of color support on this
 *   platform.  Because the ossxxx color interface that osgen uses is limited
 *   to the ANSI colors, this should normally return one of the ANSI support
 *   levels if color is supported, or the unsupported level if color isn't
 *   supported.  Note that if the platform uses a fixed color scheme that
 *   cannot be controlled via the ossxxx routines, this should usually return
 *   the "highlighting only" or "parameterized colors" level.
 *   
 *   SYSINFO_TEXT_HILITE - indicate whether or not highlighted text is
 *   rendered in a distinctive appearance.  
 */
int oss_get_sysinfo(int code, void *param, long *result);

/*
 *   Translate a portable color specification to an oss-style color code.
 *   This returns a color code suitable for use in ossclr(), ossdsp(), and
 *   the other ossxxx() routines defined here that take color codes as
 *   parameters; this color code is opaque to the caller.
 *   
 *   The purpose of this routine is to translate from the portable interface
 *   to color to the hardware-specific or OS-specific color coding used in
 *   the ossxxx layer, so that a caller given a color specified via the
 *   portable interface can send the appropriate ossxxx color code to the
 *   lower-level routines.
 *   
 *   'fg', 'bg', and 'screen_color' are OSGEN_COLOR_xxx values.  'attrs' is a
 *   combination of OS_ATTR_xxx values (defined in osifc.h).  This routine
 *   should combine the colors specified by the combination of these values
 *   to yield an appropriate color code for use in the ossxxx routines.
 *   
 *   Note that the 'bg' color can be OSGEN_COLOR_TRANSPARENT.  If this is the
 *   case, the 'screen_color' value should be used to determine the
 *   background color, if possible.  'fg' should never be transparent.  If
 *   'bg' is not transparent, then screen color is usually ignored.
 *   
 *   COLOR SUPPORT IS OPTIONAL, but this routine must be implemented if
 *   osgen.c is to be used.  If the platform does NOT support explicit color
 *   settings, this routine should still return an appropriate code that can
 *   be used with ossclr, etc.  For example, the Unix oss implementation, at
 *   the time of this writing, used an internal color scheme to select from a
 *   fixed set of parameterized colors (normal, highlighted, reverse), so the
 *   Unix implementation could simply return its appropriate internal code,
 *   largely ignoring the requested colors.  Note, however, that a platform
 *   with only parameterized colors might still want to inspect the 'fg' and
 *   'bg' colors for the OSGEN_COLOR_xxx parameterized colors
 *   (OSGEN_COLOR_TEXT, OSGEN_COLOR_STATUSBG, etc), and return its internal
 *   code corresponding to the selected parameter color when possible.  
 */
int ossgetcolor(int fg, int bg, int attrs, int screen_color);

/*
 *   Translate a key event from "raw" mode to processed mode.  This takes an
 *   event of type OS_EVT_KEY, and converts the keystroke encoded in the
 *   event from the raw keystroke to a CMD_xxx code, if appropriate.
 *   
 *   In effect, this routine converts the event returned from os_get_event()
 *   to the corresponding command code that would be returned from os_getc().
 *   
 *   This routine is defined at the "oss" level because the osgen layer is
 *   semi-portable, and thus needs to be aloof from the details of key
 *   mappings, which vary by system.  
 */
void oss_raw_key_to_cmd(os_event_info_t *evt);

/*
 *   Clear an area of the screen.  The color code is opaque to callers; it is
 *   meaningful only to the ossxxx implementation.  
 */
void ossclr(int top, int left, int bottom, int right, int color);

/*
 *   Display text at a particular location in a particular color.
 */
void ossdsp(int line, int column, int color, const char *msg);

/*
 *   Move the cursor 
 */
void ossloc(int line, int column);

/*
 *   Scroll a region of the screen UP one line. 
 */
void ossscu(int top_line, int left_column, int bottom_line,
            int right_column, int blank_color);

/*
 *   Scroll a region of the screen DOWN one line. 
 */
void ossscr(int top_line, int left_column, int bottom_line,
            int right_column, int blank_color);


/* ------------------------------------------------------------------------ */
/*
 *   osgen attribute names - these are simply substitutes for the regular
 *   osifc attribute names; they map directly to the corresponding osifc
 *   attributes.  
 */
#define OSGEN_ATTR_BOLD OS_ATTR_BOLD


/* ------------------------------------------------------------------------ */
/*
 *   osgen colors.  We don't use full RGB values in the osgen
 *   implementation, but use a simpler color scheme involving the
 *   parameterized colors (from the osifc layer), plus the eight ANSI colors
 *   (black, white, red, green, blue, yellow, cyan, magenta), each in a
 *   "low-intensity" and "high-intensity" version.
 *   
 *   If an underlying platform supports some colors but not all of the ANSI
 *   colors defined here, it should simply map each OSGEN_COLOR_xxx color to
 *   the closest available system color.
 *   
 *   Some systems use color intensity to render the "hilite" ("bold")
 *   attribute.  On these systems, it's obviously not possible for the
 *   caller to select both boldness and the full range of colors
 *   independently, because that would leave no way to render bold text for
 *   the brighter half of the range of colors.  Such systems should simply
 *   treat the high-intensity and low-intensity version of a given color as
 *   equivalent, and select the high- or low-intensity version according to
 *   the boldness attribute.  For reference, here's the correspondence
 *   between the low- and high-intensity versions of the colors:
 *   
 *   low <-> high
 *.  black <-> gray
 *.  maroon <-> red
 *.  green <-> lime
 *.  navy <-> blue
 *.  teal <-> cyan
 *.  purple <-> magenta
 *.  olive <-> yellow
 *.  silver <-> white
 *   
 *   The comment for each absolute color gives the approximate RGB value we
 *   expect for the color (given as hex triplets on 00-FF intensity scale).  
 */

/* the parameterized colors, mapped to single-byte values for osgen's use */
#define OSGEN_COLOR_TRANSPARENT ((char)((OS_COLOR_P_TRANSPARENT) >> 24))
#define OSGEN_COLOR_TEXT        ((char)((OS_COLOR_P_TEXT) >> 24))
#define OSGEN_COLOR_TEXTBG      ((char)((OS_COLOR_P_TEXTBG) >> 24))
#define OSGEN_COLOR_STATUSLINE  ((char)((OS_COLOR_P_STATUSLINE) >> 24))
#define OSGEN_COLOR_STATUSBG    ((char)((OS_COLOR_P_STATUSBG) >> 24))
#define OSGEN_COLOR_INPUT       ((char)((OS_COLOR_P_INPUT) >> 24))

/* "low-intensity" versions of the ANSI colors */
#define OSGEN_COLOR_BLACK       0x70                            /* 00 00 00 */
#define OSGEN_COLOR_MAROON      0x71                            /* 80 00 00 */
#define OSGEN_COLOR_GREEN       0x72                            /* 00 80 00 */
#define OSGEN_COLOR_NAVY        0x73                            /* 00 00 80 */
#define OSGEN_COLOR_TEAL        0x74                            /* 00 80 80 */
#define OSGEN_COLOR_PURPLE      0x75                            /* 80 00 80 */
#define OSGEN_COLOR_OLIVE       0x76                            /* 80 80 00 */
#define OSGEN_COLOR_SILVER      0x77                            /* C0 C0 C0 */

/* "high-intensity" versions of the ANSI colors */
#define OSGEN_COLOR_GRAY        0x78                            /* 80 80 80 */
#define OSGEN_COLOR_RED         0x79                            /* FF 00 00 */
#define OSGEN_COLOR_LIME        0x7A                            /* 00 FF 00 */
#define OSGEN_COLOR_BLUE        0x7B                            /* 00 00 FF */
#define OSGEN_COLOR_CYAN        0x7C                            /* 00 FF FF */
#define OSGEN_COLOR_MAGENTA     0x7D                            /* FF 00 FF */
#define OSGEN_COLOR_YELLOW      0x7E                            /* FF FF 00 */
#define OSGEN_COLOR_WHITE       0x7F                            /* FF FF FF */

#endif /* OSGEN_H */
