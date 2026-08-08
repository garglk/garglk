/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjolund
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

/*
 * The ADRIFT 4 map (run400.exe: Form29).
 *
 * A .taf holds no coordinates -- ADRIFT 4 rooms know only which room lies in
 * each of their twelve directions -- so the runner works the layout out for
 * itself every time it draws the map, and so must we.  scmap_build() is a port
 * of its algorithm (Form29.calc_grid and friends), which is a good deal less
 * principled than "graph layout" suggests:
 *
 *   - Start the player's room at the middle of a notional 1000x1000 grid and
 *     recurse outwards along the eight compass exits, one grid cell per exit.
 *     Up/Down/In/Out do not move you on the grid at all.
 *   - Recursion stops at rooms the player has not seen: they are placed (so
 *     they take up a cell) but never expanded through.
 *   - When the cell a room wants is already taken, nothing is re-placed.
 *     Instead the room doing the placing shoves *itself* one cell away and
 *     hands the cell it just vacated to its neighbour -- and shoving a room
 *     drags every room rigidly connected to it along too (shuffle()).
 *   - If a room's neighbour is already on the grid but in the wrong place, it
 *     shoves itself around (trytoinc_x and friends) until the two line up.
 *   - Give up after 1000 shoves, or if anything leaves the grid, and say so:
 *     "Cannot draw map - too complex."  Real ADRIFT 4 games do hit this.
 *
 * It follows that the map is only as good as the exit graph is planar-ish, and
 * that it changes shape as you explore.  Both were true in the runner as well;
 * this is a faithful port, bugs included (see the comments in scmap.cpp), on
 * the grounds that a map that matches the one ADRIFT 4 authors designed
 * against is worth more than a prettier one that does not.
 *
 * The result is handed to mapdraw.h, which draws it exactly as it draws an
 * ADRIFT 5 map.  (The ADRIFT 4 runner's own map, built out of Visual Basic
 * Label and Line controls, looked quite different; we do not reproduce that.)
 */

#ifndef SCMAP_H
#define SCMAP_H

#include "scprotos.h"
#include "mapdraw.h"

/* Fill in the view callbacks over SCARE's game state. */
extern void scmap_view (scr_gameref_t game, map_view_t *view);

/* Can this game ever show a map -- has it rooms, and did the author leave the
   map switched on?  This is a property of the game, unlike scmap_build()
   returning NULL, which also happens when there is merely nothing to draw from
   where the player is standing at the moment. */
extern int scmap_available (scr_gameref_t game);

/* Lay the game's rooms out and build a map of them.  Room keys are the decimal
   SCARE room number.  Returns NULL if there is nothing to draw: the game has no
   map (see scmap_available), or the layout gave up (see scmap_failed), or the
   player is standing in a room the author hid from the map -- in which case the
   ADRIFT 4 runner drew nothing at all either, and the map comes back by itself
   as soon as you step out of that room.

   `view` supplies which rooms count as seen -- the layout needs to know, since
   it only expands outwards through rooms the player has been to, and it must
   agree with the renderer about that or the two will disagree about which
   rooms exist.  Pass the same view you render with. */
extern map_t *scmap_build (scr_gameref_t game, const map_view_t *view);

/* Did the layout give up on this game ("too complex")?  Valid after a build
   that returned NULL; lets the host explain itself rather than say nothing. */
extern int scmap_failed (void);

/* Does the game define a task command of its own that a bare "map" would
   match?  As in ADRIFT 5, a game's own MAP verb must win over the host's. */
extern int scmap_command_taken (scr_gameref_t game);

#endif /* SCMAP_H */
