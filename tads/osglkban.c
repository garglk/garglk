/* osansi4.c -- glk banner interface */

#include "os.h"

#include "glk.h"
#ifdef GARGLK
#include "garglk.h"	/* for size-to-contents hack */
#endif

extern winid_t mainwin;
extern winid_t statuswin;

void *os_banner_create(void *parent, int where, void *other, int wintype,
                       int align, int siz, int siz_units,
                       unsigned long style)
{
	winid_t gparent = parent;
	glui32 gwintype = 0;
	glui32 gwinmeth = 0;

//printf("create banner wh=%d wt=%d a=%d s=%d su=%d\n",
//	where, wintype, align, siz, siz_units);

	switch (wintype)
	{
	case OS_BANNER_TYPE_TEXT: gwintype = wintype_TextBuffer; break;
	case OS_BANNER_TYPE_TEXTGRID: gwintype = wintype_TextGrid; break;
	default: gwintype = wintype_TextGrid; break;
	}

	switch (align)
	{
	case OS_BANNER_ALIGN_TOP: gwinmeth = winmethod_Above; break;
	case OS_BANNER_ALIGN_BOTTOM: gwinmeth = winmethod_Below; break;
	case OS_BANNER_ALIGN_LEFT: gwinmeth = winmethod_Left; break;
	case OS_BANNER_ALIGN_RIGHT: gwinmeth = winmethod_Right; break;
	default: gwinmeth = winmethod_Above; break;
	}

	switch (siz)
	{
	case OS_BANNER_SIZE_PCT: gwinmeth |= winmethod_Proportional; break;
	case OS_BANNER_SIZE_ABS: gwinmeth |= winmethod_Fixed; break;
	default: gwinmeth |= winmethod_Fixed; break;
	}

	if (!gparent)
		gparent = mainwin;

	/* TODO -- fiddle parent and other to split the right window */
	switch (where)
	{
	case OS_BANNER_FIRST: break;
	case OS_BANNER_LAST: break;
	case OS_BANNER_BEFORE: break;
	case OS_BANNER_AFTER: break;
	}

	return glk_window_open(gparent, gwinmeth, siz_units, gwintype, (glui32)parent);
}

void os_banner_set_size(void *banner_handle, int siz, int siz_units, int is_advisory)
{
	winid_t win = banner_handle;
	winid_t pair = glk_window_get_parent(win);
	glui32 gwinmeth;

	glk_window_get_arrangement(pair, &gwinmeth, 0, 0);
	gwinmeth &= 
		winmethod_Above | winmethod_Below |
		winmethod_Left | winmethod_Right;

	switch (siz)
	{
	case OS_BANNER_SIZE_PCT: gwinmeth |= winmethod_Proportional; break;
	case OS_BANNER_SIZE_ABS: gwinmeth |= winmethod_Fixed; break;
	default: gwinmeth |= winmethod_Fixed; break;
	}

	glk_window_set_arrangement(pair, gwinmeth, siz_units, win);
}

void os_banner_size_to_contents(void *banner_handle)
{
#ifdef GARGLK
	window_textbuffer_t *dwin;
	winid_t win = banner_handle;
	glui32 gwintype = glk_window_get_type(win);
	int size;

	if (gwintype == wintype_TextBuffer)
	{
		dwin = win->data;
		size = dwin->scrollmax;
		if (dwin->numchars)
			size ++;
		os_banner_set_size(win, OS_BANNER_SIZE_ABS, size, 0);
	}
#endif
}

void os_banner_delete(void *banner_handle)
{
	winid_t win;

	glk_window_close(banner_handle, 0);

	/* no no no, doing this here invalidates the pointers... */
	while ((win = glk_window_iterate(0, banner_handle)))
	{
		/* I hope that the game closes all windows explicitly... */
		/* glk_window_close(win, 0); */
	}
}

void os_banner_orphan(void *banner_handle)
{
	/* ignore, this should only happen when tads terminates */
}

int os_banner_getinfo(void *banner_handle, os_banner_info_t *info)
{
	winid_t win = banner_handle;
	glui32 gwinmeth;
	glui32 gw, gh;

	glk_window_get_arrangement(glk_window_get_parent(win), &gwinmeth, 0, 0);
	if (gwinmeth & winmethod_Above)
		info->align = OS_BANNER_ALIGN_TOP;
	if (gwinmeth & winmethod_Below)
		info->align = OS_BANNER_ALIGN_BOTTOM;
	if (gwinmeth & winmethod_Left)
		info->align = OS_BANNER_ALIGN_LEFT;
	if (gwinmeth & winmethod_Right)
		info->align = OS_BANNER_ALIGN_RIGHT;

	info->style = 0;

	glk_window_get_size(win, &gw, &gh);
	info->rows = gh;
	info->columns = gw;

	info->pix_width = 0;
	info->pix_height = 0;

	info->os_line_wrap = 1;

	return 1;
}

int os_banner_get_charwidth(void *banner_handle)
{
	winid_t win = banner_handle;
	glui32 gw, gh;
	glk_window_get_size(win, &gw, &gh);
	return gw;
}

int os_banner_get_charheight(void *banner_handle)
{
	winid_t win = banner_handle;
	glui32 gw, gh;
	glk_window_get_size(win, &gw, &gh);
	return gh;
}

void os_banner_clear(void *banner_handle)
{
	winid_t win = banner_handle;
	glk_window_clear(win);
}

void os_banner_disp(void *banner_handle, const char *txt, size_t len)
{
	winid_t win = banner_handle;
	strid_t str = glk_window_get_stream(win);
	glk_put_buffer_stream(str, (char*)txt, len);
}

void os_banner_set_attr(void *banner_handle, int attr)
{
	winid_t win = banner_handle;
	strid_t str = glk_window_get_stream(win);
	if (attr & OS_ATTR_BOLD && attr & OS_ATTR_ITALIC)
		glk_set_style_stream(str, style_Alert);
	else if (attr & OS_ATTR_BOLD)
		glk_set_style_stream(str, style_Subheader);
	else if (attr & OS_ATTR_ITALIC)
		glk_set_style_stream(str, style_Emphasized);
	else
		glk_set_style_stream(str, style_Normal);
}

void os_banner_goto(void *banner_handle, int row, int col)
{
	winid_t win = banner_handle;
	glk_window_move_cursor(win, col, row);
}

void os_banner_set_color(void *banner_handle, os_color_t fg, os_color_t bg) {}
void os_banner_set_screen_color(void *banner_handle, os_color_t color) {}
void os_banner_flush(void *banner_handle) {}
void os_banner_start_html(void *banner_handle) {}
void os_banner_end_html(void *banner_handle) {}

