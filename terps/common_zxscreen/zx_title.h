//
//  zx_title.h
//  Shared by the TaylorMade (taylor) and unquill interpreters.
//
//  A ZX Spectrum loading-screen (SCREEN$) title renderer over Glk. The caller
//  owns the graphics window and the surrounding window-tree teardown; this
//  scales and centres the 256x192 picture, paints it, and runs the
//  keypress/reveal event loop. The SCREEN$ format decode itself lives in
//  decompressz80.h.
//

#ifndef zx_title_h
#define zx_title_h

#include <stdint.h>

#include "glk.h"
#include "decompressz80.h"

/* Drawing context. Set 'window', 'palette' and 'vcentre_div' before use; the
 * scale/origin fields are filled in by ZXTitleRelayout(). */
typedef struct ZXTitle {
    winid_t       window;       /* graphics window to draw into             */
    const glui32 *palette;      /* 16 colours, indexed by ZXDecodeAttr()    */
    int           vcentre_div;  /* vertical centring divisor (2 = centred)  */

    int scale, origin_x, origin_y;   /* computed by ZXTitleRelayout()       */
} ZXTitle;

/* (Re)compute the scale and centring offsets for the window's current size.
 * Call once before drawing, and again on every Arrange (resize) event. */
void ZXTitleRelayout(ZXTitle *title);

/* Paint the whole 6912-byte SCREEN$ at the current geometry. */
void ZXTitleDraw(ZXTitle *title, const uint8_t *screen);

/* Display 'screen' and block until a key arrives on 'keywin', relaying out and
 * redrawing on window resize. When 'slow' is set, reveal the picture
 * progressively on a timer: in the order of order[0..order_len-1] (Spectrum
 * screen addresses, as the tape loader drew them) or, when 'order' is NULL,
 * linearly top-to-bottom. A keypress dismisses it, skipping any remaining
 * reveal. The caller must have set window/palette/vcentre_div on 'title'. */
void ZXTitleShow(ZXTitle *title, const uint8_t *screen, winid_t keywin,
    int slow, const uint16_t *order, int order_len);

#endif /* zx_title_h */
