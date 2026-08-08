/* vi: set ts=2 shiftwidth=2 expandtab:
 *
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

/*
 * The ADRIFT 5 map loader (Adrift 5 runner: clsMap.vb, Map.vb).
 *
 * The room layout is authored data, not something the interpreter derives:
 * the Developer writes an <Adventure><Map> block of <Page>s, each holding
 * <Node>s with the location key and its X/Y/Z position in map units.  All we
 * have to do is read it; mapdraw.h then draws it.  (ADRIFT 4 stores no such
 * block, and no coordinates at all -- see scmap.h, which has to derive the
 * layout for itself.)
 */

#ifndef A5MAP_H
#define A5MAP_H

#include "a5model.h"
#include "../mapdraw.h"

/* Parse <Map> out of a loaded adventure.  Returns NULL if the game has no map
   nodes at all.  The result aliases the adventure's XML document, so it must
   not outlive `adv`. */
extern map_t *a5map_load (const a5_adventure_t *adv);

/* Does the game define a task command of its own that a bare "map" would
   match?  The runner's map is GUI chrome, not a game command, so ADRIFT
   authors are free to use MAP as a verb (Lost Coastlines does, for its sea
   chart).  A host that offers a "map" command must let the game's win. */
extern int a5map_command_taken (const a5_adventure_t *adv);

#endif /* A5MAP_H */
