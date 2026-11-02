/*
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* scarier-autosave.h

   Spatterlight autosave/autorestore for the Scarier terp -- both the
   ADRIFT <=4 engine (scare, gsc_main) and the ADRIFT 5 engine (a5run,
   gsc_a5_main) -- modeled on the Question implementation
   (questionglk-autosave.*), which is in turn modeled on Bocfel's
   (bocfel-spatterlight/).

   Only the Spatterlight app build compiles scarier-autosave.mm; the Glk
   front end's side (os_glk_autosave.cpp and the call sites in the other
   os_glk*.cpp files) sits behind #ifdef SPATTERLIGHT so headless builds are
   unaffected.

   Both engines share the same file layout under
   ~/Library/Application Support/Spatterlight/SCARE Files/Autosaves/(HASH)/:
   autosave.glksave holds a small container around the engine's own state
   serialization plus the undo history (see the SCARAUTO4/SCARAUTO5
   containers in os_glk_autosave.cpp), autosave.plist the Glk library state
   (TempLibrary) with the frontend's window/stream/channel tags appended by
   an archive hook.  A given game file only ever runs on one of the two
   engines, so the shared names never collide.

   The split of labour: glkimp's autosavefiles.m owns the files (writing
   the pair, renaming it into place, reading it back), shared with other
   terps; scarier-autosave.mm adds the plist hooks for the frontend state
   (Objective-C, app target only); os_glk_autosave.cpp owns everything that
   touches the engines and the Glk globals (containers, stash/recover), and
   the prompts in os_glk.cpp / os_glk_a5.cpp call it.
*/

#ifndef SCARIER_AUTOSAVE_H
#define SCARIER_AUTOSAVE_H

#include <cstdint>
#include <string>

/* ---- implemented in scarier-autosave.mm ---------------------------------- */

/* True if a complete autosave (game state + Glk library plist) exists for
 * the current game and autosaving is enabled. */
bool scarier_autosave_exists(void);

/* The shared per-prompt guard: autosaving enabled, and the last Glk event
 * was one worth saving after (not init/arrange/redraw, and timer only when
 * the autosave-on-timer preference is on). */
bool scarier_autosave_wanted(void);

/* Autosaving enabled at all, whatever the last Glk event was: the guard for
 * a save taken at a point the game reaches once (a <waitkey> pause), where
 * there is no prompt loop for a resize or a timer to send round again. */
bool scarier_autosave_enabled(void);

/* Delete the autosave files (called after a failed restore, so the next
 * launch boots fresh instead of hitting the same failure again). */
void scarier_autosave_discard(void);

/* Write the engine-state container and the Glk library plist (with the
 * frontend state stashed via gsc_stash_frontend_state), then ask the window
 * server to snapshot the GUI under the same tag.  The per-prompt guards
 * (scarier_autosave_wanted and the engine-side checks) are the caller's
 * job. */
void scarier_autosave_write(const std::string &engine_state);

/* Read autosave.glksave into `out` (the caller parses the container). */
bool scarier_autosave_read_game(std::string *out);

/* Unarchive autosave.plist, replace the live Glk object lists, re-point the
 * frontend globals (gsc_recover_frontend_state) and run the library's
 * "late" pass (file stream reopening, timer re-arm).  Returns false, with
 * nothing replaced, when the plist is missing or unusable. */
bool scarier_autosave_restore_library(void);

/* ---- implemented in os_glk_autosave.cpp ---------------------------------- */

/* The Glk-facing front-end globals that an autorestore must re-point at
 * the restored Glk objects, carried across the archive as the objects'
 * serialization tags. */
struct ScarierGlkFrontendState {
    int mainwintag = 0;
    int statuswintag = 0;
    int sidewintag = 0;         /* a5 <window NAME> side buffer, 0 if closed  */
    int mapwintag = 0;          /* map pane, 0 when closed                    */
    int gfxwintag = 0;          /* title/cover graphics window, 0 when closed */
    int transcripttag = 0;      /* open transcript file stream, if any        */
    int inputlogtag = 0;
    int readlogtag = 0;
    int soundchanneltag = 0;    /* the ADRIFT <=4 single sound channel        */
    /* 9 = GSC_A5_MAX_CHANNELS (static_assert'd in os_glk_autosave.cpp). */
    int a5_channeltags[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint32_t a5_chan_sound[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int seen_input = 0;         /* first input given (title window dismissed) */
    uint32_t title_image = 0;   /* cover image resource id, 0 when none       */
    int map_shown = 0;
    int map_at_top = 0;
    int map_zoom = 0;
    /* Pan/zoom chrome: follow-the-player, and the last camera centre so a
     * mid-pan autosave comes back looking the same.  An autosave written
     * before these fields existed restores follow as on (the unarchiver's
     * default) and the centre as 0, which the next redraw re-clamps. */
    int map_follow = 1;
    int map_cx = 0;
    int map_cy = 0;
    int map_page = 0;
    /* "glk colour": the archived windows come back painted in the game's
     * palette, so the flag has to come back with them.  Without it the
     * interpreter reads as off in front of a coloured screen, and the next
     * status redraw paints the bar with the reverse video that colour mode
     * drops -- the game's own text colour as a solid bar. */
    int colour_on = 0;
    /* Exact RNG state (erkyrath_random detstate): which generator is active
     * plus the xoshiro words, so deterministic randomness continues across
     * an autorestore.  -1 = not recorded (an older autosave). */
    int rng_usenative = -1;
    uint32_t rng_state[4] = { 0, 0, 0, 0 };
    /* The engine's Runner-compatible generator (SCR_RNG=xoshiro, scutils.cpp
     * scr_runner_rand).  It draws from erkyrath_random too, so these words
     * repeat rng_state when it is active; they are kept so that autosaves
     * from before the two were merged still restore:
     * 1 = it was active, with its words and draw count; 0 = it was not;
     * -1 = not recorded. */
    int rng_runner = -1;
    uint32_t rng_runner_state[4] = { 0, 0, 0, 0 };
    uint32_t rng_runner_draws = 0;
};

void gsc_stash_frontend_state(ScarierGlkFrontendState *st);
void gsc_recover_frontend_state(const ScarierGlkFrontendState *st);

/* The on-disk path of the running game, for getautosavedir's file hash. */
const char *gsc_autosave_game_path(void);

#endif
