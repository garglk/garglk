//
//  templea_loadscreen_data.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Temple of Terror Side A (the graphics version) draws the same Alkatraz
//  loading screen as Side B, but its load path doesn't decrypt the loader, so
//  its window descriptors are bundled here. See DecodeAlkatrazLoadingScreen().
//

#ifndef templea_loadscreen_data_h
#define templea_loadscreen_data_h

#include <stdint.h>

#include "loading_screen.h"

/* Decode parameters for Temple of Terror Side A (TZX, file size 0xccca). */
#define TEMPLEA_SCREEN_WINDOWS  13

static const uint8_t templea_screen_descriptors[TEMPLEA_SCREEN_WINDOWS * 4] = {
    0x61, 0x40, 0xc8, 0x02, 0xc1, 0x40, 0xc8, 0x03, 0x41, 0x48, 0xc8, 0x05,
    0xe0, 0x48, 0xc8, 0x04, 0xff, 0x57, 0x9a, 0x45, 0x28, 0x4f, 0x88, 0x21,
    0xa8, 0x47, 0x88, 0x01, 0x5f, 0x57, 0x98, 0x44, 0xc9, 0x4f, 0x81, 0x2c,
    0xc0, 0x4f, 0x81, 0x0c, 0xdf, 0x4f, 0x96, 0x4f, 0x49, 0x47, 0x8a, 0x23,
    0xe5, 0x57, 0x86, 0x05,
};

static const AlkatrazScreen templea_screen = {
    .descriptors = templea_screen_descriptors,
    .windows = TEMPLEA_SCREEN_WINDOWS,
    .block = 3,
    .offset = 2,
    .key_seed = 0x98,
    .key_step = 0xcd,
    .attr_fill = 0x00,
};

#endif /* templea_loadscreen_data_h */
