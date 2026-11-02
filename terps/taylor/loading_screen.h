//
//  loading_screen.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Show a ZX Spectrum loading screen (SCREEN$) as a title image,
//  extracted from a .z80 snapshot or a .tap / .tzx tape image.
//

#ifndef loading_screen_h
#define loading_screen_h

#include <stdint.h>
#include <stddef.h>

#include "decompressz80.h"  /* ZX_SCREEN_SIZE and the SCREEN$ decode helpers */

/* Owns a malloc'd 6912-byte SCREEN$, or NULL. Captured by ProcessFile()
   during loading, consumed (and freed) by DrawZXTitleImage(). */
extern uint8_t *ZXLoadingScreen;

#ifndef SPATTERLIGHT
/* Reveal the loading screen slowly. Set by -g; glkimp.h declares
   Spatterlight's setting of the same name. */
extern int gli_slowdraw;
#endif

/* How an Alkatraz loader draws a game's loading screen. Each game's values
   are in its *_loadscreen_data.h header. */
typedef struct {
    const uint8_t *descriptors; /* window descriptors, 4 bytes each        */
    int windows;                /* number of window descriptors            */
    int block;                  /* TZX data block holding the pixel stream */
    int offset;                 /* byte offset of the stream in the block  */
    uint8_t key_seed;           /* initial rolling XOR key (LOACON)        */
    uint8_t key_step;           /* key increment per byte (ADD2)           */
    uint8_t attr_fill;          /* uniform attribute fill                  */
} AlkatrazScreen;

/* Reconstruct an Alkatraz-drawn loading screen (e.g. Kayleth, Terraquake)
   from the raw TZX image. Returns a malloc'd 6912-byte SCREEN$ (caller
   frees) or NULL. */
uint8_t *DecodeAlkatrazLoadingScreen(uint8_t *image, size_t length,
    const AlkatrazScreen *spec);

/* Record the order the loader drew the screen in, so the title can be
   revealed slowly in that same sequence (see DrawZXTitleImage). Callers that
   decode the screen themselves (e.g. Temple of Terror) use this to pass the
   order along. */
void ZXSetDrawOrder(const uint16_t *order, int count);

/* Show ZXLoadingScreen as a title image, wait for a keypress, then
   restore the normal game windows. Frees ZXLoadingScreen, sets it NULL. */
void DrawZXTitleImage(void);

#endif /* loading_screen_h */
