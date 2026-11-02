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

/* scarier-autosave.mm

   Spatterlight autosave/autorestore for the Scarier terp -- the ADRIFT <=4
   engine (gsc_main) and the ADRIFT 5 engine (gsc_a5_main) -- adapted from
   the Question implementation (terps/question/questionglk-autosave.mm), which is in
   turn adapted from Bocfel's, which follows Andrew Plotkin's IosGlk
   autosave design.

   At every top-level command prompt the engine-state container built by
   os_glk_autosave.cpp goes into

     ~/Library/Application Support/Spatterlight/SCARE Files/Autosaves/(HASH)/autosave.glksave

   and the Glk library state into autosave.plist in the same directory.
   Both are written to temp names first and only then renamed into place,
   so a failed or interrupted write leaves the previous good pair intact;
   if the second rename fails, the first is rolled back.  (Only a crash
   in the instant between the two renames can still split the pair.)
   win_autosave() then tells the window server to snapshot the GUI under
   the same tag.

   The files themselves -- writing the pair, renaming it into place,
   reading it back -- are glkimp's (autosavefiles.m), shared with the other
   terps.  This file adds only the plist hooks that carry the frontend
   state; everything engine-side (containers, window tags) lives in
   os_glk_autosave.cpp behind #ifdef SPATTERLIGHT.
*/

#include <cstdlib>
#include <string>

#include "autosavefiles.h"

#include "scarier-autosave.h"

#import "TempLibrary.h"

/* ---- file plumbing: glkimp/autosavefiles.m ------------------------------- */

bool scarier_autosave_exists(void)
{
    return gli_autosave_exists(gsc_autosave_game_path());
}

bool scarier_autosave_wanted(void)
{
    return gli_autosave_wanted();
}

bool scarier_autosave_enabled(void)
{
    return gli_enable_autosave != 0;
}

void scarier_autosave_discard(void)
{
    gli_autosave_discard(gsc_autosave_game_path());
}

/* ---- the frontend state, as plist archive extras ------------------------- */

static ScarierGlkFrontendState frontend_state;

static void scarier_library_archive(TempLibrary * /* library */, NSCoder *encoder)
{
    const ScarierGlkFrontendState *st = &frontend_state;
    [encoder encodeInt32:st->mainwintag forKey:@"scarier_mainwintag"];
    [encoder encodeInt32:st->statuswintag forKey:@"scarier_statuswintag"];
    [encoder encodeInt32:st->sidewintag forKey:@"scarier_sidewintag"];
    [encoder encodeInt32:st->mapwintag forKey:@"scarier_mapwintag"];
    [encoder encodeInt32:st->gfxwintag forKey:@"scarier_gfxwintag"];
    [encoder encodeInt32:st->transcripttag forKey:@"scarier_transcripttag"];
    [encoder encodeInt32:st->inputlogtag forKey:@"scarier_inputlogtag"];
    [encoder encodeInt32:st->readlogtag forKey:@"scarier_readlogtag"];
    [encoder encodeInt32:st->soundchanneltag forKey:@"scarier_soundchanneltag"];
    for (int i = 0; i < 9; i++) {
        [encoder encodeInt32:st->a5_channeltags[i]
                      forKey:[NSString stringWithFormat:@"scarier_a5_channeltag%d", i]];
        [encoder encodeInt32:(int32_t)st->a5_chan_sound[i]
                      forKey:[NSString stringWithFormat:@"scarier_a5_chan_sound%d", i]];
    }
    [encoder encodeInt32:st->seen_input forKey:@"scarier_seen_input"];
    [encoder encodeInt32:(int32_t)st->title_image forKey:@"scarier_title_image"];
    [encoder encodeInt32:st->map_shown forKey:@"scarier_map_shown"];
    [encoder encodeInt32:st->map_at_top forKey:@"scarier_map_at_top"];
    [encoder encodeInt32:st->map_zoom forKey:@"scarier_map_zoom"];
    [encoder encodeInt32:st->map_follow forKey:@"scarier_map_follow"];
    [encoder encodeInt32:st->map_cx forKey:@"scarier_map_cx"];
    [encoder encodeInt32:st->map_cy forKey:@"scarier_map_cy"];
    [encoder encodeInt32:st->map_page forKey:@"scarier_map_page"];
    [encoder encodeInt32:st->colour_on forKey:@"scarier_colour_on"];
    [encoder encodeInt32:st->rng_usenative forKey:@"scarier_rng_usenative"];
    for (int i = 0; i < 4; i++)
        [encoder encodeInt32:(int32_t)st->rng_state[i]
                      forKey:[NSString stringWithFormat:@"scarier_rng_state%d", i]];
    [encoder encodeInt32:st->rng_runner forKey:@"scarier_rng_runner"];
    for (int i = 0; i < 4; i++)
        [encoder encodeInt32:(int32_t)st->rng_runner_state[i]
                      forKey:[NSString stringWithFormat:@"scarier_rng_runner_state%d", i]];
    [encoder encodeInt32:(int32_t)st->rng_runner_draws
                  forKey:@"scarier_rng_runner_draws"];
}

static void scarier_library_unarchive(TempLibrary * /* library */, NSCoder *decoder)
{
    ScarierGlkFrontendState *st = &frontend_state;
    st->mainwintag = [decoder decodeInt32ForKey:@"scarier_mainwintag"];
    st->statuswintag = [decoder decodeInt32ForKey:@"scarier_statuswintag"];
    st->sidewintag = [decoder decodeInt32ForKey:@"scarier_sidewintag"];
    st->mapwintag = [decoder decodeInt32ForKey:@"scarier_mapwintag"];
    st->gfxwintag = [decoder decodeInt32ForKey:@"scarier_gfxwintag"];
    st->transcripttag = [decoder decodeInt32ForKey:@"scarier_transcripttag"];
    st->inputlogtag = [decoder decodeInt32ForKey:@"scarier_inputlogtag"];
    st->readlogtag = [decoder decodeInt32ForKey:@"scarier_readlogtag"];
    st->soundchanneltag = [decoder decodeInt32ForKey:@"scarier_soundchanneltag"];
    for (int i = 0; i < 9; i++) {
        st->a5_channeltags[i] = [decoder decodeInt32ForKey:
            [NSString stringWithFormat:@"scarier_a5_channeltag%d", i]];
        st->a5_chan_sound[i] = (uint32_t)[decoder decodeInt32ForKey:
            [NSString stringWithFormat:@"scarier_a5_chan_sound%d", i]];
    }
    st->seen_input = [decoder decodeInt32ForKey:@"scarier_seen_input"];
    st->title_image = (uint32_t)[decoder decodeInt32ForKey:@"scarier_title_image"];
    st->map_shown = [decoder decodeInt32ForKey:@"scarier_map_shown"];
    st->map_at_top = [decoder decodeInt32ForKey:@"scarier_map_at_top"];
    st->map_zoom = [decoder decodeInt32ForKey:@"scarier_map_zoom"];
    st->map_follow =
        [decoder containsValueForKey:@"scarier_map_follow"]
            ? [decoder decodeInt32ForKey:@"scarier_map_follow"] : 1;
    st->map_cx = [decoder decodeInt32ForKey:@"scarier_map_cx"];
    st->map_cy = [decoder decodeInt32ForKey:@"scarier_map_cy"];
    st->map_page = [decoder decodeInt32ForKey:@"scarier_map_page"];
    st->colour_on = [decoder decodeInt32ForKey:@"scarier_colour_on"];
    st->rng_usenative =
        [decoder containsValueForKey:@"scarier_rng_usenative"]
            ? [decoder decodeInt32ForKey:@"scarier_rng_usenative"] : -1;
    for (int i = 0; i < 4; i++)
        st->rng_state[i] = (uint32_t)[decoder
            decodeInt32ForKey:[NSString stringWithFormat:@"scarier_rng_state%d", i]];
    st->rng_runner =
        [decoder containsValueForKey:@"scarier_rng_runner"]
            ? [decoder decodeInt32ForKey:@"scarier_rng_runner"] : -1;
    for (int i = 0; i < 4; i++)
        st->rng_runner_state[i] = (uint32_t)[decoder decodeInt32ForKey:
            [NSString stringWithFormat:@"scarier_rng_runner_state%d", i]];
    st->rng_runner_draws =
        (uint32_t)[decoder decodeInt32ForKey:@"scarier_rng_runner_draws"];
}

/* ---- save ---------------------------------------------------------------- */

void scarier_autosave_write(const std::string &engine_state)
{
    gsc_stash_frontend_state(&frontend_state);
    gli_autosave_write(gsc_autosave_game_path(), engine_state.data(),
                       engine_state.size(), scarier_library_archive);
}

/* ---- restore ------------------------------------------------------------- */

bool scarier_autosave_read_game(std::string *out)
{
    void *data = nullptr;
    size_t length = 0;
    if (!gli_autosave_read_game(gsc_autosave_game_path(), &data, &length))
        return false;
    out->assign((const char *)data, length);
    free(data);
    return true;
}

bool scarier_autosave_restore_library(void)
{
    @autoreleasepool {
        TempLibrary *newlib = gli_autosave_load_library(gsc_autosave_game_path(),
                                                        scarier_library_unarchive);
        if (!newlib)
            return false;
        [newlib updateFromLibrary];
        gsc_recover_frontend_state(&frontend_state);
        [newlib updateFromLibraryLate];
    }
    return true;
}
