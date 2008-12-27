#include "nitfol.h"

/* A bunch of trivial wrappers so we can get away with ignoring whether
 * during compilation whether or not the glk library supports graphics.
 *  
 * Link this in only if you glk supports graphics */


#ifdef GLK_MODULE_IMAGE


glui32 wrap_glk_image_draw(winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
  if(!glk_gestalt(gestalt_Graphics, 0))
    return FALSE;
  return glk_image_draw(win, image, val1, val2);
}


glui32 wrap_glk_image_draw_scaled(winid_t win, glui32 image, glsi32 val1, glsi32 val2, glui32 width, glui32 height)
{
  if(!glk_gestalt(gestalt_Graphics, 0))
    return FALSE;
  return glk_image_draw_scaled(win, image, val1, val2, width, height);
}


glui32 wrap_glk_image_get_info(glui32 image, glui32 *width, glui32 *height)
{
  if(!glk_gestalt(gestalt_Graphics, 0)) {
    *width = 0; *height = 0;
    return FALSE;
  }
  return glk_image_get_info(image, width, height);
}


#endif
