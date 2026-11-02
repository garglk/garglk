//
//  sagadraw_host.h
//
//  What the common_sagadraw renderers need from the interpreter that links
//  them (ScottFree, Plus, and the test harnesses). Each host defines these;
//  including this header next to the definitions lets the compiler check
//  that both sides agree on the signatures.
//

#ifndef sagadraw_host_h
#define sagadraw_host_h

#include <stdint.h>

#include "glk.h"

#ifdef __cplusplus
extern "C" {
#endif

// The graphics window and the placement of the picture inside it
extern winid_t Graphics;
extern int pixel_size;
extern int x_offset;
extern int y_offset;
// Picture height in host pixels (Apple II: twice the hi-res rows drawn)
extern int ImageHeight;

// Set palette slot `index` to the RGB value `color`
void SetColor(int32_t index, glui32 color);
// Fill a pixel_width x 1 block (in picture pixels) with palette slot `color`
void PutPixelWithWidth(glsi32 xpos, glsi32 ypos, int32_t color, int pixel_width);
// PutPixelWithWidth with a width of 2
void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color);

#ifdef __cplusplus
}
#endif

#endif /* sagadraw_host_h */
