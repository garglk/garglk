/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey, Chris Spiegel.                          *
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

/* screen.c - Generic screen manipulation
 *
 *  Portions copyright (c) 1995-1997 Stefan Jokisch.
 */

#include "glkfrotz.h"

static zchar statusline[256];
static int oldstyle = 0;
static int curstyle = 0;
static int cury = 1;
static int curx = 1;
static int fixforced = 0;

static int curr_fg = -2;
static int curr_bg = -2;
static int curr_font = 1;
static int prev_font = 1;
static int temp_font = 0;

/* To make the common code happy */

int os_char_width (zchar z)
{
	return 1;
}

int os_string_width (const zchar *s)
{
	int width = 0;
	zchar c;
	while ((c = *s++) != 0)
		if (c == ZC_NEW_STYLE || c == ZC_NEW_FONT)
			s++;
		else
			width += os_char_width(c);
	return width;
}

int os_string_length (zchar *s)
{
	int length = 0;
	while (*s++) length++;
	return length;
}

void os_prepare_sample (int a)
{
	glk_sound_load_hint(a, 1);
}

void os_finish_with_sample (int a)
{
	glk_sound_load_hint(a, 0);
}

/*
 * os_start_sample
 *
 * Play the given sample at the given volume (ranging from 1 to 8 and
 * 255 meaning a default volume). The sound is played once or several
 * times in the background (255 meaning forever). In Z-code 3 the
 * repeats value is always 0 and the number of repeats is taken from
 * the sound file itself. The end_of_sound function is called as soon
 * as the sound finishes.
 *
 */

void os_start_sample (int number, int volume, int repeats, zword eos)
{
	int vol;

	if (!gos_channel)
	{
		gos_channel = glk_schannel_create(0);
		if (!gos_channel)
			return;
	}

	switch (volume)
	{
	case   1: vol = 0x02000; break;
	case   2: vol = 0x04000; break;
	case   3: vol = 0x06000; break;
	case   4: vol = 0x08000; break;
	case   5: vol = 0x0a000; break;
	case   6: vol = 0x0c000; break;
	case   7: vol = 0x0e000; break;
	case   8: vol = 0x10000; break;
	default:  vol = 0x20000; break;
	}

	/* we dont do repeating or eos-callback for now... */
	glk_schannel_play_ext(gos_channel, number, 1, 0);
	glk_schannel_set_volume(gos_channel, vol);
}

void os_stop_sample (int a)
{
	if (!gos_channel)
		return;
	glk_schannel_stop(gos_channel);
}

void os_beep (int volume)
{
}

void gos_update_width(void)
{
	glui32 width;
	if (gos_upper)
	{
		glk_window_get_size(gos_upper, &width, NULL);
		h_screen_cols = width;
		SET_BYTE(H_SCREEN_COLS, width);
		if (curx > width)
		{
			glk_window_move_cursor(gos_upper, 0, cury-1);
			curx = 1;
		}
	}
}

void gos_update_height(void)
{
	glui32 height_upper;
	glui32 height_lower;
	if (gos_curwin)
	{
		glk_window_get_size(gos_upper, NULL, &height_upper);
		glk_window_get_size(gos_lower, NULL, &height_lower);
		h_screen_rows = height_upper + height_lower + 1;
		SET_BYTE(H_SCREEN_ROWS, h_screen_rows);
	}
}

void reset_status_ht(void)
{
	glui32 height;
	if (gos_upper)
	{
		glk_window_get_size(gos_upper, NULL, &height);
		if (mach_status_ht != height)
		{
			glk_window_set_arrangement(
				glk_window_get_parent(gos_upper),
				winmethod_Above | winmethod_Fixed,
				mach_status_ht, NULL);
		}
	}
}

void erase_window (zword w)
{
	if (w == 0)
		glk_window_clear(gos_lower);
	else if (gos_upper)
	{
#ifdef GARGLK
			garglk_set_reversevideo_stream(
				glk_window_get_stream(gos_upper),
				TRUE);
#endif /* GARGLK */
		memset(statusline, ' ', sizeof statusline);
		glk_window_clear(gos_upper);
		reset_status_ht();
		curr_status_ht = 0;
	}
}

void split_window (zword lines)
{
	if (!gos_upper)
		return;
	/* The top line is always set for V1 to V3 games */
	if (h_version < V4)
		lines++;

	if (!lines || lines > curr_status_ht)
	{
		glui32 height;

		glk_window_get_size(gos_upper, NULL, &height);
		if (lines != height)
			glk_window_set_arrangement(
				glk_window_get_parent(gos_upper),
				winmethod_Above | winmethod_Fixed,
				lines, NULL);
		curr_status_ht = lines;
	}
	mach_status_ht = lines;
	if (cury > lines)
	{
		glk_window_move_cursor(gos_upper, 0, 0);
		curx = cury = 1;
	}
	gos_update_width();

	if (h_version == V3)
		glk_window_clear(gos_upper);
}

void restart_screen (void)
{
	erase_window(0);
	erase_window(1);
	split_window(0);
}

/*
 * statusline overflowed the window size ... bad game!
 * so ... split status text into regions, reformat and print anew.
 */

void packspaces(zchar *src, zchar *dst)
{
	int killing = 0;
	while (*src)
	{
		if (*src == 0x20202020)
			*src = ' ';
		if (*src == ' ')
			killing++;
		else
			killing = 0;
		if (killing > 2)
			src++;
		else
			*dst++ = *src++;
	}
	*dst = 0;
}

void smartstatusline (void)
{
	zchar packed[256];
	zchar buf[256];
	zchar *a, *b, *c, *d;
	int roomlen, scorelen, scoreofs;
	int len, tmp;

	packspaces(statusline, packed);
	//strcpy(packed, statusline);
	len = os_string_length(packed);

	a = packed;
	while (a[0] == ' ')
		a ++;

	b = a;
	while (b[0] != 0 && !(b[0] == ' ' && b[1] == ' '))
		b ++;

	c = b;
	while (c[0] == ' ')
		c ++;

	d = packed + len - 1;
	while (d[0] == ' ' && d > c)
		d --;
	if (d[0] != ' ' && d[0] != 0)
		d ++;
	if (d < c)
		d = c;

	//printf("smart '%s'\n", packed);
	//printf("smart %d %d %d %d\n",a-packed,b-packed,c-packed,d-packed);

	roomlen = b - a;
	scorelen = d - c;
	scoreofs = h_screen_cols - scorelen - 2;
	if (scoreofs <= roomlen)
		scoreofs = roomlen + 2;

	for (tmp = 0; tmp < h_screen_cols; tmp++)
		buf[tmp] = ' ';

	memcpy(buf + 1 + scoreofs, c, scorelen * sizeof(zchar));
	memcpy(buf + 1, a, roomlen * sizeof(zchar));
	//if (roomlen >= scoreofs)
	//	buf[roomlen + 1] = '|';

	glk_window_move_cursor(gos_upper, 0, 0);
	glk_put_buffer_uni(buf, h_screen_cols);
	glk_window_move_cursor(gos_upper, cury - 1, curx - 1);
}

void screen_char (zchar c)
{
	if (gos_linepending && (gos_curwin == gos_linewin))
	{
		gos_cancel_pending_line();
		if (gos_curwin == gos_upper)
		{
			curx = 1;
			cury ++;
		}
		if (c == '\n')
			return;
	}

	/* check fixed flag in header, game can change it at whim */
	int forcefix = ((h_flags & FIXED_FONT_FLAG) != 0);
	int curfix = ((curstyle & FIXED_WIDTH_STYLE) != 0);
	if (forcefix && !curfix)
	{
		zargs[0] = 0xf000;	/* tickle tickle! */
		z_set_text_style();
		fixforced = TRUE;
	}
	else if (!forcefix && fixforced)
	{
		zargs[0] = 0xf000;	/* tickle tickle! */
		z_set_text_style();
		fixforced = FALSE;
	}

	if (gos_upper && gos_curwin == gos_upper)
	{
		if (c == '\n' || c == ZC_RETURN) {
			glk_put_char('\n');
			curx = 1;
			cury ++;
		}
		else {
			if (cury == 1)
			{
				if (curx <= ((sizeof statusline / sizeof(zchar)) - 1))
				{
					statusline[curx - 1] = c;
					statusline[curx] = 0;
				}
				if (curx < h_screen_cols)
				{
					glk_put_char_uni(c);
				}
				else if (curx == h_screen_cols)
				{
					glk_put_char_uni(c);
					glk_window_move_cursor(gos_curwin, curx-1, cury-1);
				}
				else
				{
					smartstatusline();
				}
				curx ++;
			}
			else
			{
				if (curx < h_screen_cols)
				{
					glk_put_char_uni(c);
				}
				else if (curx == (h_screen_cols))
				{
					glk_put_char_uni(c);
					glk_window_move_cursor(gos_curwin, curx-1, cury-1);
				}
				curx++;
			}
		}
	}
	else if (gos_curwin == gos_lower)
	{
		if (c == ZC_RETURN)
			glk_put_char('\n');
		else glk_put_char_uni(c);
	}
}

void screen_new_line (void)
{
	screen_char('\n');
}

void screen_word (const zchar *s)
{
	zchar c;
	while ((c = *s++) != 0)
		if (c == ZC_NEW_FONT)
			s++;
		else if (c == ZC_NEW_STYLE)
			s++;
		else
			screen_char (c); 
}

void screen_mssg_on (void)
{
	if (gos_curwin == gos_lower)
	{
		oldstyle = curstyle;
		glk_set_style(style_Preformatted);
		glk_put_string("\n    ");
	}
}

void screen_mssg_off (void)
{
	if (gos_curwin == gos_lower)
	{
		glk_put_char('\n');
		zargs[0] = 0;
		z_set_text_style();
		zargs[0] = oldstyle;
		z_set_text_style();
	}
}

/*
 * z_buffer_mode, turn text buffering on/off.
 *
 *		zargs[0] = new text buffering flag (0 or 1)
 *
 */

void z_buffer_mode (void)
{
}

/*
 * z_buffer_screen, set the screen buffering mode.
 *
 *	zargs[0] = mode
 *
 */

void z_buffer_screen (void)
{
	store (0);
}

/*
 * z_erase_line, erase the line starting at the cursor position.
 *
 *		zargs[0] = 1 + #units to erase (1 clears to the end of the line)
 *
 */

void z_erase_line (void)
{
	int i;

	if (gos_upper && gos_curwin == gos_upper)
	{
		for (i = 0; i < h_screen_cols + 1 - curx; i++)
			glk_put_char(' ');
		glk_window_move_cursor(gos_curwin, curx - 1, cury - 1);
	}
}

/*
 * z_erase_window, erase a window or the screen to background colour.
 *
 *		zargs[0] = window (-3 current, -2 screen, -1 screen & unsplit)
 *
 */

void z_erase_window (void)
{
	short w = zargs[0];
	if (w == -2)
	{
		if (gos_upper) {
			glk_set_window(gos_upper);
#ifdef GARGLK
			garglk_set_zcolors(curr_fg, curr_bg);
#endif /* GARGLK */
			glk_window_clear(gos_upper);
			glk_set_window(gos_curwin);
		}
		glk_window_clear(gos_lower);
	}
	if (w == -1)
	{
		if (gos_upper) {
			glk_set_window(gos_upper);
#ifdef GARGLK
			garglk_set_zcolors(curr_fg, curr_bg);
#endif /* GARGLK */
			glk_window_clear(gos_upper);
		}
		glk_window_clear(gos_lower);
		split_window(0);
		glk_set_window(gos_lower);
		gos_curwin = gos_lower;
	}
	if (w == 0)
		glk_window_clear(gos_lower);
	if (w == 1 && gos_upper)
		glk_window_clear(gos_upper);
}

/*
 * z_get_cursor, write the cursor coordinates into a table.
 *
 *		zargs[0] = address to write information to
 *
 */

void z_get_cursor (void)
{
	storew ((zword) (zargs[0] + 0), cury);
	storew ((zword) (zargs[0] + 2), curx);
}

/*
 * z_print_table, print ASCII text in a rectangular area.
 *
 *		zargs[0] = address of text to be printed
 *		zargs[1] = width of rectangular area
 *		zargs[2] = height of rectangular area (optional)
 *		zargs[3] = number of char's to skip between lines (optional)
 *
 */

void z_print_table (void)
{
	zword addr = zargs[0];
	zword x;
	int i, j;

	/* Supply default arguments */

	if (zargc < 3)
		zargs[2] = 1;
	if (zargc < 4)
		zargs[3] = 0;

	/* Write text in width x height rectangle */

	x = curx;

	for (i = 0; i < zargs[2]; i++) {

		if (i != 0) {
			cury += 1;
			curx = x;
		}

		for (j = 0; j < zargs[1]; j++) {

			zbyte c;

			LOW_BYTE (addr, c)
			addr++;

			print_char (c);
		}

		addr += zargs[3];
	}
}

#define zB(i) ((((i >> 10) & 0x1F) << 3) | (((i >> 10) & 0x1F) >> 2))
#define zG(i) ((((i >>  5) & 0x1F) << 3) | (((i >>  5) & 0x1F) >> 2))
#define zR(i) ((((i      ) & 0x1F) << 3) | (((i      ) & 0x1F) >> 2))

#define zRGB(i) (zR(i) << 16 | zG(i) << 8 | zB(i))

/*
 * z_set_true_colour, set the foreground and background colours
 * to specific RGB colour values.
 *
 *	zargs[0] = foreground colour
 *	zargs[1] = background colour
 *	zargs[2] = window (-3 is the current one, optional)
 *
 */

void z_set_true_colour (void)
{
	int zfore = zargs[0];
	int zback = zargs[1];

	if (!(zfore < 0))
		zfore = zRGB(zargs[0]);

	if (!(zback < 0))
		zback = zRGB(zargs[1]);

#ifdef GARGLK
	garglk_set_zcolors(zfore, zback);
#endif /* GARGLK */

	curr_fg = zfore;
	curr_bg = zback;
}

static int zcolor_map[] = {
	-2,						/*  0 = current */
	-1,						/*  1 = default */
	0x0000,					/*  2 = black */
	0x001D,					/*  3 = red */
	0x0340,					/*  4 = green */
	0x03BD,					/*  5 = yellow */
	0x59A0,					/*  6 = blue */
	0x7C1F,					/*  7 = magenta */
	0x77A0,					/*  8 = cyan */
	0x7FFF,					/*  9 = white */
	0x5AD6,					/* 10 = light grey */
	0x4631,					/* 11 = medium grey */
	0x2D6B,					/* 12 = dark grey */
};

#define zcolor_NUMCOLORS    (13)

/*
 * z_set_colour, set the foreground and background colours.
 *
 *		zargs[0] = foreground colour
 *		zargs[1] = background colour
 *		zargs[2] = window (-3 is the current one, optional)
 *
 */

void z_set_colour (void)
{
	int zfore = zargs[0];
	int zback = zargs[1];

	switch (zfore)
	{
	case -1:
		zfore = -3;

	case 0:
	case 1:
		zfore = zcolor_map[zfore];
		break;

	default:
		if (zfore < zcolor_NUMCOLORS)
			zfore = zRGB(zcolor_map[zfore]);
		break;
	}

	switch (zback)
	{
	case -1:
		zback = -3;

	case 0:
	case 1:
		zback = zcolor_map[zback];
		break;

	default:
		if (zback < zcolor_NUMCOLORS)
			zback = zRGB(zcolor_map[zback]);
		break;
	}

#ifdef GARGLK
	garglk_set_zcolors(zfore, zback);
#endif /* GARGLK */

	curr_fg = zfore;
	curr_bg = zback;
}

/*
 * z_set_font, set the font for text output and store the previous font.
 *
 *		 zargs[0] = number of font or 0 to keep current font
 *
 */

void z_set_font (void)
{
	zword font = zargs[0];

	switch (font)
	{
		case 0: /* previous font */
			temp_font = curr_font;
			curr_font = prev_font;
			prev_font = temp_font;
			zargs[0] = 0xf000;	/* tickle tickle! */
			z_set_text_style();
			store (curr_font);
			break;

		case 1: /* normal font */
			prev_font = curr_font;
			curr_font = 1;
			zargs[0] = 0xf000;	/* tickle tickle! */
			z_set_text_style();
			store (prev_font);
			break; 

		case 4: /* fixed-pitch font*/
			prev_font = curr_font;
			curr_font = 4;
			zargs[0] = 0xf000;	/* tickle tickle! */
			z_set_text_style();
			store (prev_font);
			break;

		case 2: /* picture font, undefined per 1.1 */
		case 3: /* character graphics font */
		default: /* unavailable */
			store (0);
			break;
	}
}

/*
 * z_set_cursor, set the cursor position or turn the cursor on/off.
 *
 *		zargs[0] = y-coordinate or -2/-1 for cursor on/off
 *		zargs[1] = x-coordinate
 *		zargs[2] = window (-3 is the current one, optional)
 *
 */

void z_set_cursor (void)
{
	cury = zargs[0];
	curx = zargs[1];

	if (gos_upper) {
		if (cury > mach_status_ht) {
			mach_status_ht = cury;
			reset_status_ht();
		}

		glk_window_move_cursor(gos_upper, curx - 1, cury - 1);
	}
}

/*
 * z_set_text_style, set the style for text output.
 *
 *		 zargs[0] = style flags to set or 0 to reset text style
 *
 */

void z_set_text_style (void)
{
	int style;

	if (zargs[0] == 0)
		curstyle = 0;
	else if (zargs[0] != 0xf000) /* not tickle time */
		curstyle |= zargs[0];

	if (h_flags & FIXED_FONT_FLAG || curr_font == 4)
		style = curstyle | FIXED_WIDTH_STYLE;
	else
		style = curstyle;

	if (gos_linepending && gos_curwin == gos_linewin)
		return;

	if (style & REVERSE_STYLE)
	{
#ifdef GARGLK
		garglk_set_reversevideo(TRUE);
#endif /* GARGLK */
	}

	if (style & FIXED_WIDTH_STYLE)
	{
		if (style & BOLDFACE_STYLE && style & EMPHASIS_STYLE)
			glk_set_style(style_BlockQuote);	/* monoz */
		else if (style & EMPHASIS_STYLE)
			glk_set_style(style_Alert);			/* monoi */
		else if (style & BOLDFACE_STYLE)
			glk_set_style(style_Subheader);		/* monob */
		else
			glk_set_style(style_Preformatted);	/* monor */
	}
	else
	{
		if (style & BOLDFACE_STYLE && style & EMPHASIS_STYLE)
			glk_set_style(style_Note);			/* propz */
		else if (style & EMPHASIS_STYLE)
			glk_set_style(style_Emphasized);	/* propi */
		else if (style & BOLDFACE_STYLE)
			glk_set_style(style_Header);		/* propb */
		else
			glk_set_style(style_Normal);		/* propr */
	}

	if (curstyle == 0)
	{
#ifdef GARGLK
		garglk_set_reversevideo(FALSE);
#endif /* GARGLK */
	}

}

/*
 * z_set_window, select the current window.
 *
 *		zargs[0] = window to be selected (-3 is the current one)
 *
 */

void z_set_window (void)
{
	int win = zargs[0];

	if (win == 0)
	{
		glk_set_window(gos_lower);
		gos_curwin = gos_lower;
	}
	else
	{
		if (gos_upper)
			glk_set_window(gos_upper);
		gos_curwin = gos_upper;
	}

	if (win == 0)
		enable_scripting = TRUE;
	else
		enable_scripting = FALSE;

	zargs[0] = 0xf000;	/* tickle tickle! */
	z_set_text_style();
}

/*
 * z_show_status, display the status line for V1 to V3 games.
 *
 *		no zargs used
 *
 */

static void pad_status_line (int column)
{
	int spaces;
	spaces = (h_screen_cols + 1 - curx) - column;
	while (spaces-- > 0)
		print_char(' ');
}

void z_show_status (void)
{
	zword global0;
	zword global1;
	zword global2;
	zword addr;

	bool brief = FALSE;

	if (!gos_upper)
		return;

	/* One V5 game (Wishbringer Solid Gold) contains this opcode by
	   accident, so just return if the version number does not fit */

	if (h_version >= V4)
		return;

	/* Read all relevant global variables from the memory of the
	   Z-machine into local variables */

	addr = h_globals;
	LOW_WORD (addr, global0)
	addr += 2;
	LOW_WORD (addr, global1)
	addr += 2;
	LOW_WORD (addr, global2)

	/* Move to top of the status window, and print in reverse style. */

	glk_set_window(gos_upper);
	gos_curwin = gos_upper;

#ifdef GARGLK
	garglk_set_reversevideo(TRUE);
#endif /* GARGLK */

	curx = cury = 1;
	glk_window_move_cursor(gos_upper, 0, 0);

	/* If the screen width is below 55 characters then we have to use
	   the brief status line format */

	if (h_screen_cols < 55)
		brief = TRUE;

	/* Print the object description for the global variable 0 */

	print_char (' ');
	print_object (global0);

	/* A header flag tells us whether we have to display the current
	   time or the score/moves information */

	if (h_config & CONFIG_TIME) {		/* print hours and minutes */

		zword hours = (global1 + 11) % 12 + 1;

		pad_status_line (brief ? 15 : 20);

		print_string ("Time: ");

		if (hours < 10)
			print_char (' ');
		print_num (hours);

		print_char (':');

		if (global2 < 10)
			print_char ('0');
		print_num (global2);

		print_char (' ');

		print_char ((global1 >= 12) ? 'p' : 'a');
		print_char ('m');

	} else {								/* print score and moves */

		pad_status_line (brief ? 15 : 30);

		print_string (brief ? "S: " : "Score: ");
		print_num (global1);

		pad_status_line (brief ? 8 : 14);

		print_string (brief ? "M: " : "Moves: ");
		print_num (global2);

	}

	/* Pad the end of the status line with spaces */

	pad_status_line (0);

	/* Return to the lower window */

	glk_set_window(gos_lower);
	gos_curwin = gos_lower;
}

/*
 * z_split_window, split the screen into an upper (1) and lower (0) window.
 *
 *		zargs[0] = height of upper window in screen units (V6) or #lines
 *
 */

void z_split_window (void)
{
	split_window(zargs[0]);
}

