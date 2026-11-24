/* vi: set ts=2 shiftwidth=2 expandtab:
 *
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
 * The lifecycle table: what each module keeps beyond a turn, and when it
 * has to let go of it.
 *
 * See scrunner.h for what the `run_` files share.
 */

#include "core/scarier.h"
#include "core/scprotos.h"
#include "core/scgamest.h"
#include "core/scmap.h"
#include "scrunner.h"


/*
 * A cache keyed by a game or a bundle's address must not outlive it, or a
 * later one allocated at the same address inherits it; a question or a
 * prefix carried from one line to the next must not outlive the game it
 * was asked in.  Each module that keeps such state has one row here naming
 * what clears it, and the three walks below are the only callers:
 *
 *   FORGET_GAME    when a game state is freed: gs_destroy(), through
 *                  run_forget_game().  The undo and temporary copies of a
 *                  game are game states too.
 *   FORGET_BUNDLE  when a property bundle is freed: run_destroy_bundle().
 *                  These caches are keyed by bundle rather than by game
 *                  because the undo and temporary copies share their
 *                  game's bundle.
 *   RESET_INPUT    when line input starts over for a game that is not
 *                  running: run_input_reset().
 *
 * A module that gains state of one of these kinds adds a row, or fills a
 * column of its row, and nothing else needs to know.  Each walk takes the
 * rows in order.
 */
typedef struct
{
  const scr_char *const name;
  void (*const forget_game) (const void *game);
  void (*const forget_bundle) (scr_prop_setref_t bundle);
  void (*const reset_input) (void);
} run_lifecycle_t;

static const run_lifecycle_t RUN_LIFECYCLE[] = {
  /* The %object%/%character% candidates of the parser. */
  {"parser", uip_forget_game, nullptr, nullptr},
  {"tasks", task_forget_game, nullptr, nullptr},
  {"serializer", ser_forget_game, nullptr, nullptr},
  {"events", evt_forget_game, nullptr, nullptr},
  /* The command patterns of the task matcher. */
  {"task matcher", run_match_forget_game, nullptr, nullptr},
  {"map", scmap_forget_game, nullptr, nullptr},

  {"objects", nullptr, obj_forget_bundle, nullptr},
  {"variables", nullptr, var_forget_bundle, nullptr},
  /* The long names lib_mask_long_names() masks. */
  {"library", nullptr, lib_forget_bundle, nullptr},
  {"restrictions", nullptr, restr_forget_bundle, nullptr},

  /* The 4.0 ambiguity question and the list it offered. */
  {"which", nullptr, nullptr, lib_co_400_reset},
  /* "Who do you want to attack?" and its kin. */
  {"battle who", nullptr, nullptr, lib_battle_who_reset},
  /* The 3.9 "...with?" prefix. */
  {"with prefix", nullptr, nullptr, lib_with_prefix_390_reset},
  {"put", nullptr, nullptr, lib_put_reset}
};

enum { RUN_LIFECYCLE_ROWS = sizeof (RUN_LIFECYCLE) / sizeof (RUN_LIFECYCLE[0]) };


/*
 * run_forget_game()
 * run_lifecycle_forget_bundle()
 * run_lifecycle_reset_input()
 *
 * The three walks.  run_forget_game() is one of the hooks the engine calls
 * (core/schooks.h).
 */
void
run_forget_game (const void *game)
{
  for (scr_int row = 0; row < RUN_LIFECYCLE_ROWS; row++)
    {
      if (RUN_LIFECYCLE[row].forget_game)
        RUN_LIFECYCLE[row].forget_game (game);
    }
}

void
run_lifecycle_forget_bundle (scr_prop_setref_t bundle)
{
  for (scr_int row = 0; row < RUN_LIFECYCLE_ROWS; row++)
    {
      if (RUN_LIFECYCLE[row].forget_bundle)
        RUN_LIFECYCLE[row].forget_bundle (bundle);
    }
}

void
run_lifecycle_reset_input (void)
{
  for (scr_int row = 0; row < RUN_LIFECYCLE_ROWS; row++)
    {
      if (RUN_LIFECYCLE[row].reset_input)
        RUN_LIFECYCLE[row].reset_input ();
    }
}
