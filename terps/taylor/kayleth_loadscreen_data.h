//
//  kayleth_loadscreen_data.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Kayleth's loading screen is drawn by its Alkatraz turbo loader in a
//  scrambled pixel order rather than stored as a plain SCREEN$ block. The
//  loader's screen-draw is driven by a small list of "window" descriptors
//  (recovered from the running loader); the generic Alkatraz address-order
//  generator in alkatraz_screen.c expands them into the draw order, and the
//  pixel stream in tape block 3 fills it. See DecodeAlkatrazLoadingScreen().
//

#ifndef kayleth_loadscreen_data_h
#define kayleth_loadscreen_data_h

#include <stdint.h>

#include "loading_screen.h"

/* Decode parameters for the standard Kayleth TZX (file size 0xcadc). */
#define KAYLETH_SCREEN_WINDOWS  22

/* Window descriptors: 4 bytes each (start_lo, start_hi, dir/width, sim/height).
   These are the loader's own SCADDS table, drawing the picture as a set of
   horizontal/vertical strips (some in simultaneous interleaved groups). */
static const uint8_t kayleth_screen_descriptors[KAYLETH_SCREEN_WINDOWS * 4] = {
    0x0f, 0x47, 0x50, 0x21, 0x10, 0x40, 0x10, 0x01, 0x20, 0x40, 0xc1, 0x25,
    0x3f, 0x40, 0xc1, 0x08, 0x3f, 0x48, 0xc1, 0x2d, 0x60, 0x48, 0xc1, 0x0b,
    0xc0, 0x50, 0x10, 0x22, 0xff, 0x57, 0x50, 0x02, 0xbe, 0x57, 0x9e, 0x02,
    0x31, 0x40, 0xcc, 0x11, 0x55, 0x50, 0xc2, 0x62, 0x7c, 0x57, 0x82, 0x42,
    0x57, 0x50, 0x02, 0x22, 0x7a, 0x57, 0x42, 0x02, 0x3d, 0x40, 0xc1, 0x53,
    0x61, 0x57, 0x81, 0x29, 0x6c, 0x57, 0x84, 0x02, 0x4f, 0x57, 0x8e, 0x05,
    0x6b, 0x4f, 0x87, 0x01, 0x50, 0x4f, 0x91, 0x05, 0xaf, 0x47, 0x8f, 0x03,
    0x41, 0x47, 0x81, 0x02,
};

static const AlkatrazScreen kayleth_screen = {
    .descriptors = kayleth_screen_descriptors,
    .windows = KAYLETH_SCREEN_WINDOWS,
    .block = 3,
    .offset = 2,
    .key_seed = 0x3a,
    .key_step = 0x8f,
    .attr_fill = 0x46, /* bright yellow on black */
};

#endif /* kayleth_loadscreen_data_h */
