//
//  loading_screen.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Decode a ZX Spectrum loading screen (the 6912-byte display-file dump:
//  6144 bytes of bitmap in the Spectrum's non-linear layout, followed by
//  768 bytes of 8x8 colour attributes) and show it as a title image.
//

#include <stdlib.h>
#include <string.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#else
/* Only Spatterlight has a setting for this: the loading screen is drawn in
   one go unless the -g option is given. */
int gli_slowdraw = 0;
#endif
#include "decompressz80.h"      /* ZX_RAM_BASE, SCREEN$ geometry, the Alkatraz loader */
#include "zx_title.h"           /* shared ZXTitle renderer */
#include "aiukpalette.h"            /* pal[], palchosen, ZXOPT, DefinePalette */
#include "taylor.h"
#include "graphics.h"           /* GraphicsWindowChanged */
#include "ui.h"                 /* Bottom, Top, Graphics, Open*Window */
#include "utility.h"            /* MemAlloc, FindGlkWindowWithRock, rocks */

#include "alkatraz_screen.h"
#include "loading_screen.h"

uint8_t *ZXLoadingScreen = NULL;

/* For the slow "draw as the loader did" reveal: the screen addresses in the
   exact order the Alkatraz loader wrote them. Empty (length 0) for plain
   loaders/snapshots, which fall back to a linear top-to-bottom reveal. */
static uint16_t AlkatrazDrawOrder[ZX_SCREEN_SIZE];
static int AlkatrazDrawOrderLen = 0;

/* The SCREEN$ geometry and decode, and the scaled/centred drawing itself,
   all come from the shared ZXTitle renderer (zx_title.h) and
   decompressz80.h. */

void ZXSetDrawOrder(const uint16_t *order, int count)
{
    if (count > ZX_SCREEN_SIZE)
        count = ZX_SCREEN_SIZE;
    if (count < 0 || order == NULL)
        count = 0;
    if (count > 0)
        memcpy(AlkatrazDrawOrder, order, count * sizeof AlkatrazDrawOrder[0]);
    AlkatrazDrawOrderLen = count;
}

/* Some Alkatraz-protected games (Kayleth, Terraquake) hide the loading screen
   behind the turbo loader, which draws the picture pixel-by-pixel in a
   scrambled order rather than storing a plain SCREEN$ block. We expand the
   loader's window descriptors into that draw order (AlkatrazScreenOrder), pull
   the pixel stream from the given tape block, and decrypt each byte with the
   same rolling-key XOR the loader uses, writing it to the screen address it
   draws next. Background bitmap is black and the attributes are a uniform
   fill. The per-game descriptors and parameters live in the game's
   *_loadscreen_data.h header.

   Returns a malloc'd 6912-byte SCREEN$ (caller frees) or NULL. */
uint8_t *DecodeAlkatrazLoadingScreen(uint8_t *image, size_t length,
    const AlkatrazScreen *spec)
{
    uint16_t order[ZX_SCREEN_SIZE];
    int order_len = AlkatrazScreenOrder(spec->descriptors, spec->windows, order, ZX_SCREEN_SIZE);
    if (order_len <= 0)
        return NULL;

    /* Remember the draw order so the title can be revealed slowly, in the
       same scrambled sequence the loader used. */
    ZXSetDrawOrder(order, order_len);

    size_t block_len = 0;
    uint8_t *block = GetTZXBlock(spec->block, image, length, &block_len);
    if (block == NULL)
        return NULL;
    int offset = spec->offset;
    if (block_len < (size_t)(offset + order_len)) {
        free(block);
        return NULL;
    }

    uint8_t *screen = MemAlloc(ZX_SCREEN_SIZE);
    memset(screen, 0x00, ZX_BITMAP_SIZE);                 /* black bitmap   */
    memset(screen + ZX_BITMAP_SIZE, spec->attr_fill,
        ZX_SCREEN_SIZE - ZX_BITMAP_SIZE);                 /* attribute fill */

    /* key is the loader's rolling XOR key (its LOACON), advancing by
       key_step (the loader's ADD2) after each decoded byte. */
    uint8_t key = spec->key_seed;
    for (int i = 0; i < order_len; i++) {
        uint16_t addr = order[i];
        uint8_t encrypted = block[offset + i];
        /* addr is a Spectrum screen address (0x4000-0x5AFF); ZX_RAM_BASE
           maps to the start of our SCREEN$ buffer. */
        screen[addr - ZX_RAM_BASE] =
            encrypted ^ key ^ 1 ^ ((addr >> 8) & 0xff) ^ (addr & 0xff);
        key += spec->key_step;
    }

    free(block);
    return screen;
}

void DrawZXTitleImage(void)
{
    if (!TAYLOR_GRAPHICS_ENABLED)
        return;
    if (!ZXLoadingScreen)
        return;

    /* Force the ZX palette regardless of the game's own (a forced-C64
       setting would otherwise mangle the colours), restored afterward. */
    palette_type storedpal = palchosen;
    palchosen = ZXOPT;
    DefinePalette();

    /* Replace the whole window tree with a single full-screen graphics
       window, like ScottFree's title routine. */
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top) {
        glk_window_close(Top, NULL);
        Top = NULL;
    }
    Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics) {
        glk_window_close(Graphics, NULL);
        Graphics = NULL;
    }
    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom) {
        glk_window_close(Bottom, NULL);
        Bottom = NULL;
    }

    Graphics = glk_window_open(0, 0, 0, wintype_Graphics, GLK_GRAPHICS_ROCK);
    if (Graphics == NULL) {
        palchosen = storedpal;
        DefinePalette();
        OpenBottomWindow();
        glk_set_window(Bottom);
        OpenTopWindow();
        return;
    }

    /* Black border in the centring margins around the 256x192 screen. */
    glk_window_set_background_color(Graphics, 0);
    glk_window_clear(Graphics);

    /* We need a window that can take a char event. Use the graphics
       window if the implementation supports it, else a thin text buffer. */
    winid_t keywin = Graphics;
    if (glk_gestalt_ext(gestalt_GraphicsCharInput, 0, NULL, 0) == 0) {
        Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Fixed,
            2, wintype_TextBuffer, GLK_BUFFER_ROCK);
        keywin = Bottom;
    }

    /* Reveal the picture slowly (in the loader's draw order, when one was
       captured) when the slow-draw setting (or -g) is on and delays are
       not off. Otherwise paint it at once.
       taylor uses the brighter ZXOPT palette (pal[]) and centres vertically. */
    int slow = gli_slowdraw && !(Options & NO_DELAYS);
    ZXTitle title = { Graphics, pal, 2, 0, 0, 0 };
    ZXTitleShow(&title, ZXLoadingScreen, keywin, slow,
        AlkatrazDrawOrderLen > 0 ? AlkatrazDrawOrder : NULL, AlkatrazDrawOrderLen);

    /* Tear the title down and rebuild the normal window stack. */
    glk_window_close(Graphics, NULL);
    Graphics = NULL;
    GraphicsWindowChanged();
    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom)
        glk_window_close(Bottom, NULL);
    OpenBottomWindow();
    glk_set_window(Bottom);
    /* DisplayInit() had pointed CurrentWindow at the now-closed Bottom;
       repoint it at the fresh one so output isn't misrouted. */
    CurrentWindow = Bottom;
    OpenTopWindow();

    palchosen = storedpal;
    DefinePalette();

    free(ZXLoadingScreen);
    ZXLoadingScreen = NULL;
    AlkatrazDrawOrderLen = 0;
}
