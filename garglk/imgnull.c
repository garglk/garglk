#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

#ifdef GLK_MODULE_IMAGE

glui32 glk_image_draw(winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
    return FALSE;
}

glui32 glk_image_draw_scaled(winid_t win, glui32 image,
    glsi32 val1, glsi32 val2, glui32 width, glui32 height)
{
    return FALSE;
}

glui32 glk_image_get_info(glui32 image, glui32 *width, glui32 *height)
{
    return FALSE;
}

void glk_window_flow_break(winid_t win)
{
}

void glk_window_erase_rect(winid_t win,
	glsi32 left, glsi32 top, glui32 width, glui32 height)
{
}

void glk_window_fill_rect(winid_t win, glui32 color,
	glsi32 left, glsi32 top, glui32 width, glui32 height)
{
}

void glk_window_set_background_color(winid_t win, glui32 color)
{
}

#endif

