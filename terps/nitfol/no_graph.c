#include "nitfol.h"

/* Link this in only if your glk doesn't support graphics */

glui32 wrap_glk_image_draw(winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
  return FALSE;
}


glui32 wrap_glk_image_draw_scaled(winid_t win, glui32 image, glsi32 val1, glsi32 val2, glui32 width, glui32 height)
{
  return FALSE;
}


glui32 wrap_glk_image_get_info(glui32 image, glui32 *width, glui32 *height)
{
  *width = 0; *height = 0;
  return FALSE;
}


