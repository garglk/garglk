/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osscolor.c - ossxxx color routines for DOS
Function
  
Notes
  
Modified
  05/19/02 MJRoberts  - Creation
*/

#include "os.h"
#include "osgen.h"

extern int os_f_plain;

/*
 *   Given an OSGEN_COLOR_xxx value, translate to the 4-bit system color:
 *   
 *.  0 = black
 *.  1 = navy
 *.  2 = green
 *.  3 = teal
 *.  4 = maroon
 *.  5 = purple
 *.  6 = olive
 *.  7 = silver
 *
 *.  8 = gray ("bright black")
 *.  9 = blue
 *.  10 = lime
 *.  11 = cyan
 *.  12 = red
 *.  13 = magenta
 *.  14 = yellow
 *.  15 = white
 *   
 *   If the color value is one of the "parameterized" colors, we'll use the
 *   current value for the color as set in the global variables
 *   (text_normal_color, etc).  
 */
static int xlat_color(int color)
{
    switch(color)
    {
    case OSGEN_COLOR_TEXT:
        /* return the foreground portion of the normal text color */
        return (text_normal_color & 0x07);

    case OSGEN_COLOR_TEXTBG:
        /* return the background portion of the normal text color */
        return ((text_normal_color >> 4) & 0x07);

    case OSGEN_COLOR_STATUSLINE:
        /* return the foreground portion of the statusline color */
        return (sdesc_color & 0x07);

    case OSGEN_COLOR_STATUSBG:
        /* return the background portion of the statusline color */
        return ((sdesc_color >> 4) & 0x07);

    case OSGEN_COLOR_INPUT:
        /* return the foreground portion of the normal text color */
        return (text_normal_color & 0x07);

    case OSGEN_COLOR_BLACK:
        return 0;

    case OSGEN_COLOR_NAVY:
        return 1;

    case OSGEN_COLOR_GREEN:
        return 2;

    case OSGEN_COLOR_TEAL:
        return 3;

    case OSGEN_COLOR_MAROON:
        return 4;

    case OSGEN_COLOR_PURPLE:
        return 5;

    case OSGEN_COLOR_OLIVE:
        return 6;

    case OSGEN_COLOR_SILVER:
        return 7;

    case OSGEN_COLOR_GRAY:
        return 8;

    case OSGEN_COLOR_BLUE:
        return 9;

    case OSGEN_COLOR_LIME:
        return 10;

    case OSGEN_COLOR_CYAN:
        return 11;

    case OSGEN_COLOR_RED:
        return 12;

    case OSGEN_COLOR_MAGENTA:
        return 13;

    case OSGEN_COLOR_YELLOW:
        return 14;

    case OSGEN_COLOR_WHITE:
        return 15;

    default:
        return 0;
    }
}

/*
 *   Translate a portable color specifiation to an ossxxx value.
 *   
 *   For DOS character mode, the OS-level color consists of a combination of
 *   bit fields:
 *   
 *.  7 6 5 4 3 2 1 0
 *   b b b b h f f f
 *   
 *.  'b' = background color - a 4-bit value in bits 4-7
 *.  'h' = highlight - a 1-bit value at bit 3
 *.  'f' = foreground color - a 3-bit value in bits 0-2
 *   
 *   The foreground and background values are taken from the set listed in
 *   the comments on xlat_color() above.
 *   
 *   We render the "hilite" attribute using foreground color intensity: for
 *   normal text, we only use the dim colors, values 0-7; for highlighted
 *   text, we only use the bright colors, values 8-15.  This means that we
 *   can map the full complement of 16 OSGEN_COLOR_xxx values to system
 *   colors for the background, but we must collapse the low- and
 *   high-intensity version of the OSGEN_COLOR_xxx values, so that we can
 *   independently set the dim/bright color according to the highlighting
 *   attribute.  
 */
int ossgetcolor(int fg, int bg, int attrs, int screen_color)
{
    int color;

    /*
     *   Check for a special case: if they're just setting the BOLD attribute
     *   for text in the normal text color (the PARAMETERIZED normal text
     *   color, not the color that happens to match the current parameter
     *   value), then use the text_bold_color global variable, which comes
     *   from the user preferences. 
     */
    if (fg == OSGEN_COLOR_TEXT && bg == OSGEN_COLOR_TRANSPARENT
        && (attrs & OS_ATTR_BOLD) != 0)
    {
        /* 
         *   they specifically want bold normal text, so use the
         *   parameterized setting for this style 
         */
        return text_bold_color;
    }
    
    /*
     *   First, figure the background color.  If 'bg' is transparent, then
     *   use the current screen color.  Otherwise, use the given background
     *   color.  
     */
    if (bg == OSGEN_COLOR_TRANSPARENT)
    {
        /* use the screen color */
        color = xlat_color(screen_color);
    }
    else
    {
        /* use the explicit color */
        color = xlat_color(bg);
    }

    /* shift the background color into the appropriate bit field */
    color <<= 4;

    /* 
     *   Map the foreground color and combine it with the background color.
     *   Note that we must collapse the bright and dim foreground colors
     *   into a single set, because we need to control brightness
     *   independently with the "highlight" attribute; to do this, simply
     *   mask out the brightness bit from the translated foreground color.  
     */
    if (fg == OSGEN_COLOR_TRANSPARENT)
    {
        /* it's transparent, so use the screen color */
        color |= (xlat_color(screen_color) & 0x07);
    }
    else
    {
        /* use the explicit color */
        color |= (xlat_color(fg) & 0x07);
    }

    /* if the BOLD attribute is set, set the highlight bit in the color */
    if ((attrs & OS_ATTR_BOLD) != 0)
        color |= 0x08;

    /* return the color */
    return color;
}

/*
 *   Get extended system information 
 */
int oss_get_sysinfo(int code, void *param, long *result)
{
    switch(code)
    {
    case SYSINFO_TEXT_COLORS:
        /* we support the ANSI colors, foreground and background */
        *result = SYSINFO_TXC_ANSI_FGBG;
        return TRUE;

    case SYSINFO_TEXT_HILITE:
        /* 
         *   we can render highlighted text with a unique appearance, as long
         *   as we're not in 'plain' mode 
         */
        *result = (os_f_plain ? 0 : 1);
        return TRUE;

    default:
        /* not recognized */
        return FALSE;
    }
}

