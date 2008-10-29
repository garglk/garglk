/* osansi4.c -- dummy banner interface */

#include "os.h"

void *os_banner_create(void *parent, int where, void *other, int wintype,
                       int align, int siz, int siz_units,
                       unsigned long style)
{
	return 0;
}

int os_banner_get_charwidth(void *banner_handle) { return 80; }
int os_banner_get_charheight(void *banner_handle) { return 1; }

int os_banner_getinfo(void *banner_handle, os_banner_info_t *info)
{
	info->align = OS_BANNER_ALIGN_TOP;
	info->style = 0;
	info->rows = 80;
	info->columns = 1;
	info->pix_width = 0;
	info->pix_height = 0;
	info->os_line_wrap = 1;
	return 1;
}

void os_banner_delete(void *banner_handle) {}
void os_banner_orphan(void *banner_handle) {}
void os_banner_clear(void *banner_handle) {}
void os_banner_disp(void *banner_handle, const char *txt, size_t len) {}
void os_banner_set_attr(void *banner_handle, int attr) {}
void os_banner_set_color(void *banner_handle, os_color_t fg, os_color_t bg) {}
void os_banner_set_screen_color(void *banner_handle, os_color_t color) {}
void os_banner_flush(void *banner_handle) {}
void os_banner_set_size(void *banner_handle, int siz, int siz_units, int is_advisory) {}
void os_banner_size_to_contents(void *banner_handle) {}
void os_banner_start_html(void *banner_handle) {}
void os_banner_end_html(void *banner_handle) {}
void os_banner_goto(void *banner_handle, int row, int col) {}

