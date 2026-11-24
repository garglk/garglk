/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * The random draws the Runner makes while it loads a game, replayed so the
 * RNG stream lines up with the Runner's from the first turn.
 *
 * See scrunner.h for what the `run_` files share.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "core/scarier.h"
#include "core/scprotos.h"
#include "core/scgamest.h"
#include "scrunner.h"


/*
 * run_runner_resource_draws()
 * run_runner_load_draws()
 *
 * Runner-compatible RNG mode (SCR_RNG=xoshiro) only: consume the words run400
 * draws while it loads a game, in its order, so that a Scarier stream seeded
 * like the Runner's (vbrng.dll, VBRNG=xoshiro) stays aligned draw for draw
 * from the first turn.  Every site is in openadv (mdlSpreadTheLoad 49347C):
 *
 *   48ED52  the decoded .taf's temp file, `\_<Int(Rnd * 10000)>.tmp`;
 *   48F48E  the player's starting stamina, Battle System games only,
 *           read with the header: `Int(Rnd * (hi - lo)) + lo`;
 *   491628  each StarterType 1 event's length, `Int(Rnd*(T2-T1))+T1+1`,
 *   491678  each StarterType 2 event's delay, `Int(Rnd*(End-Start))+Start`,
 *           in event order -- StarterType 3 rolls nothing;
 *   4920B1  each NPC's starting stamina, Battle System games only;
 *   454874  one `Int(Rnd * 100000)` per resource the .taf carries data for
 *           (name set, length > 0 -- a back-reference or an external file
 *           rolls nothing), naming its temp file.  These are throwaways, so
 *           only their count matters, but the Runner's order is kept:
 *           IntroRes, WinRes, then rooms (Res, then per alternate Res1 and
 *           Res2), objects (Res1, Res2), tasks (Res), events (Res 0-4),
 *           NPCs (Res 0-3), sound before graphic for each.
 *
 * Scarier makes the same rolls in its own places -- gs_create(),
 * evt_start_load_events(), battle_start() -- and in its own order; in this
 * mode those consume what is rolled here instead (see gs_event_loadtime(),
 * battle_preroll_*()).  Called at the end of run_create() and again on
 * restart, which in the Runner is a fresh load.
 */
static void
run_runner_resource_draw (scr_prop_setref_t bundle,
                          const scr_char *partial_format,
                          const scr_vartype_t vt_partial[],
                          const scr_char *embedded_key)
{
  scr_vartype_t vt_key[8], vt_rvalue;
  scr_char format[16];
  size_t length;

  length = strlen (partial_format);
  assert (length + 1 < sizeof (vt_key) / sizeof (vt_key[0]));
  memcpy (vt_key, vt_partial, length * sizeof (vt_key[0]));
  vt_key[length].string = embedded_key;
  snprintf (format, sizeof (format), "I<-%ss", partial_format);

  /* The key is absent when the game has no sound or no graphics at all. */
  if (prop_get (bundle, format, &vt_rvalue, vt_key) && vt_rvalue.integer)
    scr_randomint (0, 99999);
}

static void
run_runner_resource_draws (scr_prop_setref_t bundle,
                           const scr_char *partial_format,
                           const scr_vartype_t vt_partial[])
{
  run_runner_resource_draw (bundle, partial_format, vt_partial,
                            "SoundEmbedded");
  run_runner_resource_draw (bundle, partial_format, vt_partial,
                            "GraphicEmbedded");
}

/*
 * run_runner_event_draws()
 *
 * The event starts every Runner draws while loading (run390 46616F/4661BF,
 * run400 491628/491678): for each event with a random starter, one DRAW
 * between Time1 and Time2 (StarterType 1, the load time) or between
 * StartTime and EndTime (StarterType 2, the start time).
 */
static void
run_runner_event_draws (scr_gameref_t game,
                        scr_int (*draw) (scr_int lo, scr_int hi))
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int index_;

  GS_FOR_EACH_EVENT (game, index_)
    {
      scr_int startertype, lo, hi;

      vt_key[0].string = "Events";
      vt_key[1].integer = index_;
      vt_key[2].string = "StarterType";
      startertype = prop_get_integer (bundle, "I<-sis", vt_key);

      switch (startertype)
        {
        case 1:
          vt_key[2].string = "Time1";
          lo = prop_get_integer (bundle, "I<-sis", vt_key);
          vt_key[2].string = "Time2";
          hi = prop_get_integer (bundle, "I<-sis", vt_key);
          gs_set_event_loadtime (game, index_, draw (lo, hi));
          break;

        case 2:
          vt_key[2].string = "StartTime";
          lo = prop_get_integer (bundle, "I<-sis", vt_key);
          vt_key[2].string = "EndTime";
          hi = prop_get_integer (bundle, "I<-sis", vt_key);
          gs_set_event_time (game, index_, draw (lo, hi));
          break;

        default:
          break;
        }
    }
}

/* Int(Rnd * (hi - lo)) + lo, from the codec's LCG; see below. */
static scr_int
run_legacy_event_draw (scr_int lo, scr_int hi)
{
  return lo + (scr_int) floor (scr_vb_rnd () * (hi - lo));
}

static void
run_runner_legacy_load_draws (scr_gameref_t game)
{
  /*
   * A 3.9 game draws NOTHING from the game stream while loading: run390
   * seeds with Timer only at the end of openadv (467013), after the codec's
   * `Randomize 1976`, so its event starts (46616F/4661BF, the same formulas
   * as run400's) and Speed 1 attack counters (466A43) come from the codec's
   * own LCG, continued from the last file byte -- deterministic, and replayed
   * here from the decoder's state (taf_runtime_rnd).  Events are read before
   * NPCs.  No temp-file draw, no stamina draws, no embedded resources.
   */
  taf_runtime_rnd_reset ();
  run_runner_event_draws (game, run_legacy_event_draw);
  battle_preroll_legacy (game);
}

void
run_runner_load_draws (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[6], vt_rvalue;
  scr_int index_, count, sub;

  if (!scr_is_runner_random ())
    return;

  vt_key[0].string = "Version";
  if (prop_get (bundle, "I<-s", &vt_rvalue, vt_key)
      && vt_rvalue.integer < TAF_VERSION_400)
    {
      run_runner_legacy_load_draws (game);
      return;
    }

  /* 48ED52: the temp file name. */
  scr_randomint (0, 9999);

  /* 48F48E: the player's stamina. */
  battle_preroll_player_stamina (game);

  /* 491628 / 491678: event starts. */
  run_runner_event_draws (game, scr_randomint_exclusive);

  /* 4920B1: NPC stamina. */
  battle_preroll_npc_stamina (game);

  /* 454874: temp files for the embedded resources, 4.0 games only. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "Embedded";
  if (!prop_get_boolean (bundle, "B<-ss", vt_key))
    return;

  vt_key[1].string = "IntroRes";
  run_runner_resource_draws (bundle, "ss", vt_key);
  vt_key[1].string = "WinRes";
  run_runner_resource_draws (bundle, "ss", vt_key);

  vt_key[0].string = "Rooms";
  count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < count; index_++)
    {
      scr_int alts;

      vt_key[1].integer = index_;
      vt_key[2].string = "Res";
      run_runner_resource_draws (bundle, "sis", vt_key);

      vt_key[2].string = "Alts";
      alts = prop_get_child_count (bundle, "I<-sis", vt_key);
      for (sub = 0; sub < alts; sub++)
        {
          /* Both sounds, then both graphics (492B4B..492CA0). */
          vt_key[3].integer = sub;
          vt_key[4].string = "Res1";
          run_runner_resource_draw (bundle, "sisis", vt_key, "SoundEmbedded");
          vt_key[4].string = "Res2";
          run_runner_resource_draw (bundle, "sisis", vt_key, "SoundEmbedded");
          vt_key[4].string = "Res1";
          run_runner_resource_draw (bundle, "sisis", vt_key,
                                    "GraphicEmbedded");
          vt_key[4].string = "Res2";
          run_runner_resource_draw (bundle, "sisis", vt_key,
                                    "GraphicEmbedded");
        }
    }

  vt_key[0].string = "Objects";
  count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < count; index_++)
    {
      vt_key[1].integer = index_;
      vt_key[2].string = "Res1";
      run_runner_resource_draws (bundle, "sis", vt_key);
      vt_key[2].string = "Res2";
      run_runner_resource_draws (bundle, "sis", vt_key);
    }

  vt_key[0].string = "Tasks";
  count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < count; index_++)
    {
      vt_key[1].integer = index_;
      vt_key[2].string = "Res";
      run_runner_resource_draws (bundle, "sis", vt_key);
    }

  vt_key[0].string = "Events";
  count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < count; index_++)
    {
      vt_key[1].integer = index_;
      vt_key[2].string = "Res";
      for (sub = 0; sub < 5; sub++)
        {
          vt_key[3].integer = sub;
          run_runner_resource_draws (bundle, "sisi", vt_key);
        }
    }

  vt_key[0].string = "NPCs";
  count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < count; index_++)
    {
      vt_key[1].integer = index_;
      vt_key[2].string = "Res";
      for (sub = 0; sub < 4; sub++)
        {
          vt_key[3].integer = sub;
          run_runner_resource_draws (bundle, "sisi", vt_key);
        }
    }
}
