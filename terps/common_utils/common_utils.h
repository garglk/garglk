/*
 *  common_utils.h
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Glk helpers of the ScottFree family.
 */

#ifndef common_utils_h
#define common_utils_h

#include <stddef.h>

#include "fatal.h"
#include "glk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A terp that links common_utils.c supplies what Fatal() needs: the global
   winid_t Bottom (the main text window, which Fatal() opens if there is none
   yet), Display(winid_t, const char *fmt, ...), and CleanupAndExit(). */

winid_t FindGlkWindowWithRock(glui32 rock);

/* Makes win, which was split off above its sibling, size tall (rows for a text
   grid, pixels for a graphics window). */
static inline void SetFixedWindowHeight(winid_t win, glui32 size)
{
    winid_t parent = glk_window_get_parent(win);
    if (parent)
        glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed, size, NULL);
}

#ifdef __cplusplus
}
#endif

#endif /* common_utils_h */
