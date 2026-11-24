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
 * The library command tables, and nothing else: one row per pattern, in the
 * order the passes in scrun_priority.cpp and scrun_rows.cpp walk them.  Row
 * order is behaviour, so a diff of this file is a diff of what answers a line.
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


/* Movement commands for the four point compass. */
scr_commands_t MOVE_COMMANDS_4[] = {
  {"{go {to {the}}} [north/n]", lib_cmd_go_north},
  {"{go {to {the}}} [east/e]", lib_cmd_go_east},
  {"{go {to {the}}} [south/s]", lib_cmd_go_south},
  {"{go {to {the}}} [west/w]", lib_cmd_go_west},
  {"{go {to {the}}} [up/u]", lib_cmd_go_up},
  {"{go {to {the}}} [down/d]", lib_cmd_go_down},
  {"{go {to {the}}} [in/inside/enter]", lib_cmd_go_in},
  {"{go {to {the}}} [out/o/outside/exit]", lib_cmd_go_out},
  {nullptr, nullptr}
};

/* Movement commands for the eight point compass. */
scr_commands_t MOVE_COMMANDS_8[] = {
  {"{go {to {the}}} [north/n]", lib_cmd_go_north},
  {"{go {to {the}}} [east/e]", lib_cmd_go_east},
  {"{go {to {the}}} [south/s]", lib_cmd_go_south},
  {"{go {to {the}}} [west/w]", lib_cmd_go_west},
  {"{go {to {the}}} [up/u]", lib_cmd_go_up},
  {"{go {to {the}}} [down/d]", lib_cmd_go_down},
  {"{go {to {the}}} [in/inside/enter]", lib_cmd_go_in},
  {"{go {to {the}}} [out/o/outside/exit]", lib_cmd_go_out},
  {"{go {to {the}}} [northeast/north-east/ne]", lib_cmd_go_northeast},
  {"{go {to {the}}} [southeast/south-east/se]", lib_cmd_go_southeast},
  {"{go {to {the}}} [northwest/north-west/nw]", lib_cmd_go_northwest},
  {"{go {to {the}}} [southwest/south-west/sw]", lib_cmd_go_southwest},
  {nullptr, nullptr}
};

/*
 * The six put-in / put-on rows for one verb spelling ("put", or the
 * "[drop/put down]" alias).  Each table below uses it once per spelling,
 * and the two tables list the spellings in opposite order.  Order matters
 * because "put %text% in %object%" also matches "put down X in Y".
 * PRIORITY_COMMANDS explains its order where the rows appear.
 * STANDARD_COMMANDS' order has never been probed on its own, so keep it.
 */
#define PUT_IN_ON_ROWS(verb) \
  {verb " [all/everything] [in/into/inside {of}] %object%", \
   lib_cmd_put_all_in}, \
  {verb " [all/everything] [[except/but] {for}/apart from] %text%" \
   " [in/into/inside {of}] %object%", lib_cmd_put_in_except_multiple}, \
  {verb " %text% [in/into/inside {of}] %object%", \
   lib_cmd_put_in_multiple, RUN_ROW_PUT}, \
  {verb " [all/everything] [on/onto/on top of] %object%", \
   lib_cmd_put_all_on}, \
  {verb " [all/everything] [[except/but] {for}/apart from] %text%" \
   " [on/onto/on top of] %object%", lib_cmd_put_on_except_multiple}, \
  {verb " %text% [on/onto/on top of] %object%", \
   lib_cmd_put_on_multiple, RUN_ROW_PUT},

/* "Priority" library commands, may take precedence over the game. */
scr_commands_t PRIORITY_COMMANDS[] = {

  /* Acquisition of and disposal of inventory.
   *
   * Bare `pick %object%` is a genuine take synonym in both real Runners:
   * run400 answers `pick pretty flowers` (Professor Von Witt) with "You take
   * the pretty flowers from the window box.", and run390 answers `pick boat`
   * (Marooned v1) with "You can't take the wrecked boat."  It reaches the
   * whole *object* take family -- `pick all`, `pick X from Y`, `pick all
   * from Y` -- but NOT the NPC handlers: run400 gives `pick burton` "Take
   * what?" where `take burton` is answered "...would appreciate being
   * handled.", and `pick all from burton` "I don't understand where you want
   * to get things from." where `take all from burton` again gets the
   * "handled" reply.  `pick up X from Y` is not a take-from either ("Take
   * what?").  All measured live 2026-08-18; Professor Von Witt's own bundled
   * walkthrough depends on the bare-`pick` form.  `pick` is also the only
   * one of these that survives the check described above `close %object%`
   * below: it appears as a literal in all four Runner listings, where
   * `grab` -- once listed here -- appears in none. */
  {"[[get/take/remove/extract/pick] [all/everything] from/empty] %object%",
   lib_cmd_take_all_from},
  {"[[get/take/remove/extract/pick] [all/everything] from/empty] %object%"
   " [[except/but] {for}/apart from] %text%",
   lib_cmd_take_from_except_multiple},
  {"[get/take/remove/extract/pick] [all/everything]"
   " [[except/but] {for}/apart from] %text% from %object%",
   lib_cmd_take_from_except_multiple},
  {"[get/take/remove/extract/pick] %text% from %object%",
   lib_cmd_take_from_multiple},
  {"[get/take] [all/everything] from %character%", lib_cmd_take_all_from_npc},
  {"[get/take] [all/everything] from %character%"
   " [[except/but] {for}/apart from] %text%",
   lib_cmd_take_from_npc_except_multiple},
  {"[get/take] [all/everything]"
   " [[except/but] {for}/apart from] %text% from %character%",
   lib_cmd_take_from_npc_except_multiple},
  {"[get/take] %text% from %character%", lib_cmd_take_from_npc_multiple},
  {"[[get/take/pick up/pick] [all/everything]/pick [all/everything] up]",
   lib_cmd_take_all},
  {"[get/take/pick up/pick] [all/everything]"
   " [[except/but] {for}/apart from] %text%",
   lib_cmd_take_except_multiple},
  /* `pick %text% up` before the bare-`pick` catch-all: a failed object parse
   * in lib_cmd_take_multiple falls through, but keep `pick flowers up` from
   * ever being read as `pick "flowers up"` in the first place. */
  {"pick %text% up", lib_cmd_take_multiple},
  {"[get/take/pick up/pick] %text%", lib_cmd_take_multiple},
  /*
   * "drop X in Y" and "drop X on Y" are ADRIFT's put handlers wearing a
   * different verb: `drop wallet in bin` answers "You put your wallet inside
   * the rubbish bin.", and `drop wallet on bin` gives the put-on refusal
   * "You can't put anything onto the rubbish bin!".  "put down" and "all"
   * behave the same way.  Verified live against run400.exe with Ticket to No
   * Where, whose walkthrough disposes of five bits of litter with
   * "drop <litter> in bin".  These have to precede the plain drop patterns
   * below, whose %text% would otherwise swallow the "in <container>" tail and
   * leave the player with "Drop what?".
   */
  /*
   * 4.0 names the CONTAINER of a put/drop line before its object, and
   * three of name_object's exits speak for a container that is not there
   * or not a container -- "I don't understand what you want to put things
   * inside.", "Where do you want to put the desk?", "You can't put anything
   * onto the box!" -- before any of the rows below could name the object;
   * see lib_cmd_put_container_400().  The refusal defers from here like
   * the rows below and prints from the STANDARD_COMMANDS twin.
   */
  {"put *", lib_cmd_put_container_400},
  {"[drop/put down] *", lib_cmd_put_container_400},
  PUT_IN_ON_ROWS ("[drop/put down]")
  /*
   * The plain "put" spellings of the same handlers.  Priority placement is
   * live-verified (run400, 2026-08-02, probe FM7 / TheADRIFTProject): a
   * completable put beats a matched-but-failing task, so these must run
   * before the loud-fail task pass; the refusal cases defer from here (see
   * run_priority_commands) and print from the STANDARD_COMMANDS duplicates.
   */
  PUT_IN_ON_ROWS ("put")
  /* 3.9's "can't put anything inside/onto that!"; see lib_put_that_390(). */
  {"put %text% [in/into/inside {of}] *", lib_cmd_put_in_that_390},
  {"put %text% [on/onto/on top of] *", lib_cmd_put_on_that_390},
  {"[[drop/put down] [all/everything]/put [all/everything] down]",
   lib_cmd_drop_all},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%",
   lib_cmd_drop_except_multiple},
  {"[drop/put down] %text%", lib_cmd_drop_multiple, RUN_ROW_PUT},
  {"put %text% down", lib_cmd_drop_multiple, RUN_ROW_PUT},
  /* Below 4.0 `leave` is a third spelling of `drop`; see
   * lib_cmd_leave_all_pre400().  The all-row has to come first, as "leave
   * all" matches the named row with %text% = "all" as well. */
  {"leave [all/everything]", lib_cmd_leave_all_pre400},
  {"leave [all/everything] [[except/but] {for}/apart from] %text%",
   lib_cmd_leave_except_multiple_pre400},
  {"leave %text%", lib_cmd_leave_multiple_pre400},

  /*
   * Inventory display.  Treated as a priority system command so that it is
   * not pre-empted by a game task whose command matches "i"/"inventory" but
   * whose restrictions fail (those tasks print their fail message in a later
   * pass).  This matches the ADRIFT Runner -- e.g. "i" in Lair of the
   * CyberCow lists the items you hold rather than showing the failing task's
   * "...lost it all in the well" message.  A game task with *passing*
   * restrictions still overrides this, as those run before priority commands.
   */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[inventory/inv]", lib_cmd_inventory},
#else
  {"[inventory/inv/i]", lib_cmd_inventory},
#endif
  {nullptr, nullptr}
};

/* Standard library commands, other than movement and priority above. */
scr_commands_t STANDARD_COMMANDS[] = {

  /* Inventory, and general investigation of surroundings. */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[inventory/inv]", lib_cmd_inventory},
  {"[ex/exam/examine/look {at}] {{the} [room/location]}",
   lib_cmd_look_typed},
  {"look around", lib_cmd_look_typed},
  {"[ex/exam/examine/look {at/in}] %object%",
   lib_cmd_examine_object},
  {"[ex/exam/examine/look {at}] %character%",
   lib_cmd_examine_npc},
  {"[ex/exam/examine/look {at}] [me/self/myself]",
   lib_cmd_examine_self},
  {"[ex/exam/examine/look {at}] all", nullptr, lib_print_message,
   "Please examine one object at a time.\n"},
#else
  {"[inventory/inv/i]", lib_cmd_inventory},
  {"[x/ex/exam/examine/l/look {at}] {{the} [room/location]}",
   lib_cmd_look_typed},
  {"[l/look] around", lib_cmd_look_typed},
  {"[x/ex/exam/examine/look {at/in}] %object%",
   lib_cmd_examine_object},
  {"[x/ex/exam/examine/look {at}] %character%",
   lib_cmd_examine_npc},
  {"[x/ex/exam/examine/look {at}] [me/self/myself]",
   lib_cmd_examine_self},
  {"[x/ex/exam/examine/look {at}] all", nullptr, lib_print_message,
   "Please examine one object at a time.\n"},
#endif

  /* Attempted acquisition of and disposal of NPCs. */
  {"[get/take/pick up] %character%", lib_cmd_take_npc},
  {"pick %character% up", lib_cmd_take_npc},

  /*
   * Manipulating selected objects.  The put-family rows repeat their
   * PRIORITY_COMMANDS twins (drop spellings included): the priority pass
   * defers the refusal cases so a matched task's fail message can claim the
   * input first, and this second appearance prints the refusal when no task
   * did.
   */
  {"put *", lib_cmd_put_container_400},
  {"[drop/put down] *", lib_cmd_put_container_400},
  PUT_IN_ON_ROWS ("put")
  PUT_IN_ON_ROWS ("[drop/put down]")
  {"open %object%", lib_cmd_open_object},
  /*
   * DO NOT ADD VERB SYNONYMS BY DIFFING RESPONSES AGAINST A LIVE RUNNER.
   * Twelve were added that way on 2026-08-18 -- grab, inspect, check, shut,
   * hand, consume, slay, ignite, shatter, crack, swallow, yank -- each
   * "confirmed" because it drew the canonical verb's unmatched-object
   * response rather than the catch-all.  Every one was a false positive:
   * the probe game, easter.taf, ships its own 100-entry SYNONYM table
   * (`SCR_DUMP_TASKS=1` prints it) that rewrites all of them before the
   * parser ever sees them.  None of the twelve occurs as a literal --
   * anywhere, in any casing -- in run370.bas, run380.bas, run390's
   * Form1.frm or run400.bas, while every verb that survives here does.
   *
   * The listings are the authority: grep the four of them in
   * ~/Adrift_decompile, or use `index/verbs.py -w <word>`, which lists the
   * matcher literals per Runner.  A live probe can only confirm what the
   * listing already shows, and only on a game with no SYNONYM table of its
   * own (The Town of Azra has none, easter.taf and The Cellar do).
   */
  {"close %object%", lib_cmd_close_object},
  {"unlock %object% with %text%", lib_cmd_unlock_object_with},
  {"lock %object% with %text%", lib_cmd_lock_object_with},
  {"unlock %object%", lib_cmd_unlock_object},
  {"lock %object%", lib_cmd_lock_object},
  {"read %object%", lib_cmd_read_object},
  {"read *", lib_cmd_read_other},
  {"give %object% to %character%", lib_cmd_give_object_npc},
  /*
   * The Runner's give is word-order free: the character handler tests
   * c("give"), a present NPC, and then every object whose name appears
   * anywhere in the line (run400 48022F), so `give cathy diary` and `give
   * diary cathy` both offer the diary.  House (4.0)
   * runner_probes/house.run400.b.txt: `give cathy diary` gets the same reply
   * as `give diary to cathy`, not "Give the diary to who?".
   */
  {"give %character% %object%", lib_cmd_give_object_npc},
  {"give %object% %character%", lib_cmd_give_object_npc},
  {"sit {down/up} [on/in] %object%", lib_cmd_sit_on_object},
  {"stand {up/down} [on/in] %object%", lib_cmd_stand_on_object},
  {"[lie/lay] on %object%", lib_cmd_lie_on_object},
  /*
   * `get up` is a `stand` synonym in every Runner; `get off` and `get down`
   * are `stand`-off-a-thing synonyms that arrived in 3.9.  The sitstand
   * proc tests `c("get off") Or c("get down")`; neither string appears
   * anywhere in run370/run380.
   * Measured live on the one file microwaveman.taf -- a 3.8 game -- run
   * under three Runners, which is the way to see a *library* change rather
   * than a file-format one:
   *
   *              run380              run390                  run400
   *   get up    "already standing!"  "already standing!"   "already standing!"
   *   get down  "Take what?"         "not standing on ..."  "not standing on ..."
   *   get off   "Take what?"         "not standing on ..."  "not standing on ..."
   *
   * Deliberate deviation: Scarier takes all three at every version, with
   * the 3.9 behaviour, rather than the pre-3.9 "Take what?" (see
   * lib_cmd_get_off()).
   *
   * `get on %object%` arrived at the same time; it is a `stand on` synonym
   * that refuses differently.  See lib_cmd_get_on_object().
   */
  {"get [on/onto] %object%", lib_cmd_get_on_object},
  /*
   * Deliberate deviation: no Runner has `get in X` or `get out of X`; the
   * take rows above read every one of them as a take of X (see
   * lib_take_declines_get_motion()).  Here `get in X` stands on a standable
   * X, or lies on a lieable one, and `get out of X` is `get off X` while the
   * player is on X.  Anything else declines to the verb catch-all.
   */
  {"get [in/into/inside] %object%", lib_cmd_get_in_object},
  {"get out of %object%", lib_cmd_get_out_of_object},
  {"get {down/up} off %object%", lib_cmd_get_off_object},
  {"get off", lib_cmd_get_off},
  {"get down", lib_cmd_get_down},
  {"get up", lib_cmd_stand_on_floor},
  {"sit {down/up} {[on/in] {the} [ground/floor]}", lib_cmd_sit_on_floor},
  {"stand {up/down} {[on/in] {the} [ground/floor]}", lib_cmd_stand_on_floor},
  {"[lie/lay] {down/up} {[on/in] {the} [ground/floor]}", lib_cmd_lie_on_floor},
  /* 3.7's scope-free sitstand loops; see lib_cmd_sit_scan_370(). */
  {"sit {down/up} [on/in] *", lib_cmd_sit_scan_370},
  {"stand {up/down} [on/in] *", lib_cmd_stand_scan_370},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_lie_scan_370},
  {"eat %object%", lib_cmd_eat_object},

  /* Dressing up, and dressing down. */
  {"[[wear/put on/don] [all/everything]/put [all/everything] on]",
   lib_cmd_wear_all},
  {"[wear/put on/don] [all/everything] [[except/but] {for}/apart from] %text%",
   lib_cmd_wear_except_multiple},
  {"[wear/put on/don] %text%", lib_cmd_wear_multiple},
  {"put %text% on", lib_cmd_wear_multiple},
  {"[[remove/take off/doff] [all/everything]/take [all/everything] off/strip]",
   lib_cmd_remove_all},
  {"[remove/take off/doff] [all/everything]"
   " [[except/but] {for}/apart from] %text%",
   lib_cmd_remove_except_multiple},
  {"[remove/take off/doff] %text%", lib_cmd_remove_multiple},
  {"take %text% off", lib_cmd_remove_multiple},

  /* Selected NPC interactions and conversation. */
  {"ask %character% about %text%", lib_cmd_ask_npc_about},

  /*
   * `talk to %character% about %text%` is the same conversation branch as
   * `ask`: every Runner guards it with `c("ask") Or c("talk to")` (run400
   * loc_47F8F7).  `speak to` is not in that list at any version, so it only
   * ever reaches the ask-format hint below, even where a topic would have
   * matched.  A `talk to` with no matching topic lands on the hint too -- see
   * lib_ask_npc_about().
   *
   * The hint itself is the per-character pass's `talk`/`speak` branch, and
   * its guard is where the versions part: 3.7 and 3.8 match the bare words
   * (run380 loc_4405D7), 3.9 and 4.0 require the "to" (run400 loc_47F84A).
   * Hence the second row's gate.
   * Both rows sit ahead of `talk *` further down, which is the generaltasks
   * rabblings line -- the fall-through 3.9/4.0 leave for a bare `talk bob`.
   * See lib_cmd_talk_to_npc().
   */
  {"talk to %character% about %text%", lib_cmd_talk_to_npc_about},
  {"[talk/speak] to %character% *", lib_cmd_talk_to_npc},
  {"[talk/speak] %character% *", lib_cmd_talk_to_npc_pre_390},
  {"[attack/kick] %character% with %object%",
   lib_cmd_attack_npc_with, RUN_ROW_WITH_DEFERRED},
  {"chop %character% with %object%",
   lib_cmd_chop_npc_with, RUN_ROW_WITH_DEFERRED},
  {"cut %character% with %object%",
   lib_cmd_cut_npc_with, RUN_ROW_WITH_DEFERRED},
  {"hit %character% with %object%",
   lib_cmd_hit_npc_with, RUN_ROW_WITH_DEFERRED},
  {"shoot %character% with %object%",
   lib_cmd_shoot_npc_with, RUN_ROW_WITH_DEFERRED},
  {"stab %character% with %object%",
   lib_cmd_stab_npc_with, RUN_ROW_WITH_DEFERRED},
  {"throw %object% at %character%",
   lib_cmd_throw_npc_with, RUN_ROW_WITH_DEFERRED},
  {"kill %character% with %object%",
   lib_cmd_kill_npc_with, RUN_ROW_WITH_DEFERRED},
  {"fight %character% with %object%",
   lib_cmd_fight_npc_with, RUN_ROW_WITH_DEFERRED},
  /*
   * `slap` is a pre-parse rewrite to `hit`, not a grammar verb, and it is
   * space-bounded in 4.0 so a command-initial `slap` never fires there.
   * `smack` is in no Runner at all.  See lib_cmd_slap_*() for the sites.
   */
  {"[attack/kick] %character%", lib_cmd_attack_npc, RUN_ROW_WITH_DEFERRED},
  {"slap %character% with %object%",
   lib_cmd_slap_npc_with, RUN_ROW_WITH_DEFERRED},
  {"slap %character%", lib_cmd_slap_npc, RUN_ROW_WITH_DEFERRED},
  {"chop %character%", lib_cmd_chop_npc, RUN_ROW_WITH_DEFERRED},
  {"cut %character%", lib_cmd_cut_npc, RUN_ROW_WITH_DEFERRED},
  {"shoot %character%", lib_cmd_shoot_npc, RUN_ROW_WITH_DEFERRED},
  {"stab %character%", lib_cmd_stab_npc, RUN_ROW_WITH_DEFERRED},
  {"kill %character%", lib_cmd_kill_npc, RUN_ROW_WITH_DEFERRED},
  {"fight %character%", lib_cmd_fight_npc, RUN_ROW_WITH_DEFERRED},
  /*
   * One line naming several NPCs: dobattle strikes every one of them, where
   * Scarier asks which was meant and strikes that one alone (a deliberate
   * deviation, see lib_battle_attack_many()); a battle verb naming no NPC
   * at all asks "Who do you want to attack?".  `throw` is one of the battle
   * verbs too (47E9EF): les_feux T115 `throw grappin on rocher` asks Who.
   */
  {"[attack/kick/fight/kill/chop/cut/hit/shoot/stab/throw] %text%"
   " with %object%", lib_cmd_attack_npcs_with, RUN_ROW_WITH_DEFERRED},
  {"[attack/kick/fight/kill/chop/cut/hit/shoot/stab/throw] %text%",
   lib_cmd_attack_npcs, RUN_ROW_WITH_DEFERRED},
  /* A bare battle verb asks too, ahead of `kick`'s and `hit`'s "what?". */
  {"[attack/kick/fight/kill/chop/cut/hit/shoot/stab/throw]",
   lib_cmd_attack_npcs, RUN_ROW_WITH_DEFERRED},

  /* More movement, waiting, and miscellaneous administrative commands. */
  /*
   * `go` and `enter` are the two verbs generaltasks answers with a nudge back
   * to the compass (run400 loc_48936C / loc_489383).  The movement table
   * above has already taken every bare direction word, `enter` and `exit`
   * among them, so what is left here is a bare `go` and an `enter` with
   * something attached.  See lib_cmd_just_a_direction().
   *
   * `goto`/`go to` keep their own rows below: they are the Runner's
   * gotoplace(), which exits at once on a bare `go`, `goto` or `go to` and so
   * leaves those to the nudge as well.
   */
  {"go", lib_cmd_just_a_direction},
  {"go to", lib_cmd_just_a_direction},
  {"enter *", lib_cmd_just_a_direction},

  /*
   * gotoplace() itself, which tests the whole line: "goto" anywhere, or a
   * line starting "go " (3.9+, and a deviation below; see
   * lib_goto_line_enters()) or "go to" (3.7, 3.8).  See lib_cmd_go_place().
   * What it leaves is a bare `goto`, which the Runner answers with
   * DontUnderstand (runner_probes/goto.run3[89]0.b.*,
   * runner_probes/goto.run400.txt).  Deliberate deviation: Scarier lists
   * the exits, as SCARE did.
   */
  {"*", lib_cmd_go_place},
  {"goto", lib_cmd_print_room_exits},
  {"[exits/directions/where]", lib_cmd_print_room_exits},
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[wait] %number%", lib_cmd_wait_number},
  {"[wait]", lib_cmd_wait},
#else
  /*
   * `z` only entered the Runner vocabulary at 3.9 (index/verbs.py; cave.taf
   * run380 live 2026-08-31 answers it "Say again?").  Deliberate deviation:
   * Scarier waits on it at every version.  A game's own `z` task still wins,
   * tasks being matched first, and no 3.7/3.8 corpus game has one.
   */
  {"[wait/z] %number%", lib_cmd_wait_number},
  {"[wait/z]", lib_cmd_wait},
#endif
  {"save", lib_cmd_save},
  {"[restore/load]", lib_cmd_restore},
  {"restart", lib_cmd_restart},
  /*
   * The Runner takes six words for "do that again", tested as a set on the
   * whole input line before anything else looks at it:
   *
   *   00089FE2  If s = "!!" Or s = "again" Or s = "last" Or s = "previous"
   *                Or s = "!" Or s = "g" Then
   *
   * (mdlSpreadTheLoad.Sub_20_62 in run400).  All six measured live in run400
   * on easter.taf -- "previous" repeats an examine, "!!" repeats it again --
   * with one wrinkle worth knowing: the history scan that follows skips only
   * entries *identical* to the word just typed, so "last" straight after
   * "previous" repeats the literal word "previous" and is refused.  "!" and
   * "!!" are Scarier's own history shorthand as well, and keep the richer
   * SCARE forms ("!5", "!take") the Runner has no equivalent of.
   */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[again/last/previous]", lib_cmd_again},
#else
  {"[again/g/last/previous]", lib_cmd_again},
#endif
  /*
   * `redo` is ours; `!` is the Runner's, so only the word goes away under
   * SCARIER_NO_ABBREVIATIONS.  The `!5` / `!take` arguments stay either way --
   * the Runner has no equivalent, but it has no bare `!`-plus-argument form to
   * clash with either, so they cost nothing.
   */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"!%number%", lib_cmd_redo_number, RUN_ROW_SCARE_ONLY},
  {"!%text%", lib_cmd_redo_text, RUN_ROW_SCARE_ONLY},
  {"!", lib_cmd_redo_last, RUN_ROW_SCARE_ONLY},
#else
  {"[redo /!]%number%", lib_cmd_redo_number, RUN_ROW_SCARE_ONLY},
  {"[redo /!]%text%", lib_cmd_redo_text, RUN_ROW_SCARE_ONLY},
  {"[redo/!]", lib_cmd_redo_last, RUN_ROW_SCARE_ONLY},
#endif
  /*
   * `bye` and `end` are exact synonyms of `quit` in every Runner -- one
   * three-way whole-line test that unloads the form and answers "I'm so glad
   * you said no..." if the confirmation is declined (run400 loc_48AA85).
   * `q` is ours, not theirs; see the audit note above.
   */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[quit/bye/end]", lib_cmd_quit},
#else
  {"[quit/q/bye/end]", lib_cmd_quit},
#endif
  {"endgame", lib_cmd_endgame},
  /*
   * Audited against the four Runner constant pools and the 426-game v4
   * corpus ("Meta commands" in test/adrift4/notes/WINE-TRANSCRIPTS.md):
   * besides the `stats` row below, `hints`, `q`, `brief`,
   * `verbose`, `notify`, `notification`, `redo`, `hist`, `gpl`, `license` and
   * `statusline` are all SCARE inventions no Runner accepts (the Runner words
   * are `hint`, `quit`, `history`).  A game task is matched before
   * run_standard_commands(), so any task carrying COMPLETE text wins and the
   * invention never runs; a task that does not claim its line left the gap
   * that `stats` fell into, and run_game_owns_scare_word() closes it for the
   * ten that are words: a word some task of the game names is
   * answered as the Runner would.  `q` and the shorthands are not covered.
   *
   * All eleven are compiled out by SCARIER_NO_ABBREVIATIONS, alongside the
   * `g`/`i`/`z` shorthands, so that build offers a game exactly the meta
   * vocabulary the Runner does and can never steal a line from a task.  The
   * default build keeps them: they are useful, and the corpus says they are
   * harmless.
   *
   * `turns`/`undo` are 3.8+ and `version` 3.9+ in the Runner; left ungated
   * here because no 3.7/3.8 corpus game names them.
   */
  {"turns", lib_cmd_turns},
  {"score", lib_cmd_score},
  {"undo", lib_cmd_undo},
  /* `past` is the Runner's own synonym of `history` -- the two are one
   * whole-line test in all four (run400 loc_48A51A).  The `%number%` form
   * is a SCARE extension on top of both, and `hist` is ours alone; see the
   * audit note below. */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[history/past] %number%", lib_cmd_history_number, RUN_ROW_SCARE_ONLY},
  {"[history/past]", lib_cmd_history, RUN_ROW_SCARE_ONLY},
  {"hint", lib_cmd_hints, RUN_ROW_SCARE_ONLY},
#else
  {"[hist/history/past] %number%", lib_cmd_history_number, RUN_ROW_SCARE_ONLY},
  {"[hist/history/past]", lib_cmd_history, RUN_ROW_SCARE_ONLY},
  {"[hint/hints]", lib_cmd_hints, RUN_ROW_SCARE_ONLY},
  {"verbose", lib_cmd_verbose, RUN_ROW_SCARE_ONLY},
  {"brief", lib_cmd_brief, RUN_ROW_SCARE_ONLY},
  {"[notify/notification] %text%", lib_cmd_notify_on_off, RUN_ROW_SCARE_ONLY},
  {"[notify/notification]", lib_cmd_notify, RUN_ROW_SCARE_ONLY},
#endif
  {"time", lib_cmd_time},
  /* The ADRIFT Runner answers with the system date; a smart-alec reply. */
  {"date", nullptr, lib_print_message,
   "Maybe we should just be good friends.\n"},
  {"[help/commands]", lib_cmd_help},
#ifndef SCARIER_NO_ABBREVIATIONS
  {"[gpl/license]", lib_cmd_license, RUN_ROW_SCARE_ONLY},
#endif
  {"[about/info/information/author]", lib_cmd_information},
  {"[clear/cls/clr]", lib_cmd_clear},
#ifndef SCARIER_NO_ABBREVIATIONS
  {"[statusline/stats]", lib_cmd_statusline, RUN_ROW_SCARE_ONLY},
#endif
  {"[control panel/control-panel/control/panel]", lib_cmd_control_panel},
  /*
   * `stats` is a SCARE invention: the string does not occur in ANY of the
   * four Runner binaries, and in suburbanprodigy3 it would steal T31 from the
   * game's own task.  It is a name for `statusline` above because
   * run_game_owns_scare_word() hands the word to a game with a task for it.
   * `status` itself is 3.9/4.0 only and lives solely inside the
   * Battle System handler (run400 47DCA1, run390 44C510), which is why both
   * handlers below are gated on battle_is_enabled().
   */
  {"status %character%", lib_cmd_status_npc},
  {"[status]", lib_cmd_status_player},
  {"wield %object%", lib_cmd_wield},
  {"wield %text%", lib_cmd_wield_other},
  {"wield", lib_cmd_wield_other},
  {"version", lib_cmd_version},

  {"[locate/where {is/are}/find] %object%", lib_cmd_locate_object},
  {"[locate/where {is}/find] %character%", lib_cmd_locate_npc},

  {"[count/num]", lib_cmd_count},

  {nullptr, nullptr}
};

/*
 * Standard response commands; no real action, just output.  A separate
 * table because run_standard_commands() only reaches it after the table
 * above has had both its positional and its containment pass: the Runner's
 * per-verb co() finds an object named anywhere in the line before any
 * "You see no such thing." can fire, so `x silver key` must examine "a key"
 * (man_overboard, 4.0, run400 transcript line 170) rather than fall to
 * lib_cmd_examine_other.
 */
/*
 * Two catch-alls that are not part of the bucket below, even though they look
 * like catch-alls of the same kind.  Both belong to named handlers that
 * run390's generaltasks() calls ABOVE the out-of-room task refusal at
 * loc_45FFE8, so both outrank "You can't do that here!" where the therest()
 * catch-alls lose to it.
 *
 * Take is the first.  In run390 the take code is a dedicated handler with its
 * own "Take what?" and its own "You can't get anything from that.", not
 * therest()'s.  ALEXIS.TAF under run390 measures the difference: `take pot`,
 * `take jacket`, `take coins`, `take ornate key` and four more all answer
 * "Take what?" on turns where a task matching the line exists in another
 * room and `give stones to larnt` on the same row answers "You can't do that
 * here!" instead of therest()'s "Give what?"
 * (runner_probes/alexis_worn_cube.run390.txt).  Hence a table of its own,
 * run at the end of run_standard_verb_commands().
 */
/*
 * run390's insides() -- the whole "take X from Y" handler -- answers before
 * the undress verb and before the generic catch-alls, so its two "nothing
 * answers to the noun after from" replies need a table above STANDARD_
 * COMMANDS rather than a row inside it: `remove coin from zzzz` is run390's
 * "Get the coin from what?" and run400's "I don't understand where you want
 * to get things from.", never scarier's "You are not wearing the coin!"
 * (p39DARK/p4TFROM, runner_probes/tfrom.run400.feed2.txt,
 * runner_probes/dark.run390.feed5.txt).  Both rows end in a
 * wildcard because the container slot resolved nothing; the %object% rows for
 * the same shapes are up in PRIORITY_COMMANDS and have already declined by
 * here.  See lib_cmd_take_from_nowhere().
 */
scr_commands_t STANDARD_TAKE_FROM_COMMANDS[] = {
  {"[[get/take/remove/extract/pick] [all/everything] from/empty] *",
   lib_cmd_take_from_nowhere_all},
  {"[get/take/remove/extract/pick] %text% from *", lib_cmd_take_from_nowhere},
  {nullptr, nullptr}
};

/*
 * The pre-4.0 put's answers for a container slot that named nothing.  Unlike
 * the take-from pair above, these belong BELOW STANDARD_COMMANDS: a line
 * whose container is a real object is the ordinary put rows' to answer, and
 * only what they decline -- "put lamp in box" with the box a room away, "put
 * coin in me" -- reaches here.  See lib_cmd_put_in_nowhere() and its
 * surface twin lib_cmd_put_on_nowhere().
 */
scr_commands_t STANDARD_PUT_COMMANDS[] = {
  /* The clauseless spellings, which none of the rows below can match; see
   * lib_cmd_put_no_clause_pre400(). */
  {"put [in/into/inside {of}] *", lib_cmd_put_no_clause_pre400},
  {"put [on/onto/on top of] *", lib_cmd_put_no_clause_pre400},
  {"put %text% [in/into/inside {of}] *", lib_cmd_put_in_nowhere},
  {"[drop/put down] %text% [in/into/inside {of}] *", lib_cmd_put_in_nowhere},
  {"put %text% [on/onto/on top of] *", lib_cmd_put_on_nowhere},
  {"[drop/put down] %text% [on/onto/on top of] *", lib_cmd_put_on_nowhere},
  {nullptr, nullptr}
};

scr_commands_t STANDARD_ABOVE_REFUSAL_COMMANDS[] = {
  /*
   * The 4.0 named take for a noun that names only objects the player has
   * seen elsewhere; it declines to anything else, leaving "Take what?" to
   * the catch-all below it.  See lib_cmd_take_absent().
   */
  {"[get/take/pick up/pick] %object%", lib_cmd_take_absent},
  {"[get/take/pick up/pick] *", lib_cmd_get_what},
  /*
   * run380's drops() writes "You don't have <Prefix> <Short>!" for a
   * named object it cannot drop before it calls tasks(), and the room refusal
   * only fills an empty message -- so below 3.9 it outranks "You can't do
   * that here.".  See lib_cmd_drop_absent_pre390().
   */
  {"[drop/put down] *", lib_cmd_drop_absent_pre390},
  {"leave *", lib_cmd_leave_absent_pre400},
  {nullptr, nullptr}
};

/*
 * run390's examines() is called from generaltasks at 45F684, far above the
 * out-of-room refusal's `If msg = ""` at 45FFE8, and its tail writes "Nothing
 * special." for a noun that names nothing -- so an examine line that
 * an out-of-room task also matches is "Nothing special.", not "You can't do
 * that here!".  Gamma Gals (3.9) `look under dresser` and `look under bed`
 * typed outside the tasks' rooms (run390x, Delron walkthrough replay).  3.9
 * only: 3.7/3.8 have no bare look in examines and are unmeasured; 4.0's
 * refusal is not here.
 */
scr_commands_t EXAMINE_ABOVE_REFUSAL_390_COMMANDS[] = {
  {"[x/ex/exam/examine/look {at}] %object%", lib_cmd_examine_absent},
  {"[x/ex/exam/examine/look {at}] *", lib_cmd_examine_other},
  {nullptr, nullptr}
};

scr_commands_t STANDARD_FALLBACK_COMMANDS[] = {
  /*
   * The two 4.0-only absent-object rows sit directly above the catch-alls
   * they pre-empt, because the Runner's clause fires only when nothing
   * else in the turn has spoken; see lib_absent_seen_object().
   */
  {"open %object%", lib_cmd_open_absent},
  {"open *", lib_cmd_open_other},
  {"close %object%", lib_cmd_close_absent},
  {"close *", lib_cmd_close_other},
  {"give *", lib_cmd_verb_absent_400},
  {"give %object% *", lib_cmd_give_object},
  {"give *", lib_cmd_give_what},
  {"lock %object% *", lib_cmd_lock_object_pre_400},
  {"lock %text%", lib_cmd_lock_other},
  {"lock", lib_cmd_lock_what},
  {"unlock %object% *", lib_cmd_unlock_object_pre_400},
  {"unlock %text%", lib_cmd_unlock_other},
  {"unlock", lib_cmd_unlock_what},
  {"sit {down/up} [on/in] *", lib_cmd_verb_absent_400},
  {"sit {down/up} [on/in] *", lib_cmd_sit_other},
  {"stand {up/down} [on/in] *", lib_cmd_verb_absent_400},
  {"stand {up/down} [on/in] *", lib_cmd_stand_other},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_verb_absent_400},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_lie_other},
  {"[remove/take off/doff] *", lib_cmd_remove_what},
  {"[drop/put down] *", lib_cmd_drop_what},
  {"leave *", lib_cmd_leave_what_pre400},
  {"[wear/put on/don] *", lib_cmd_wear_what},
  /* 4.0 only, below the wear row so `put on X` keeps its own refusal; see
   * lib_cmd_put_unclear(). */
  {"put *", lib_cmd_put_unclear},
  /*
   * The swearing list is version-split at both ends.  `piss` has been in it
   * since 3.7 and was simply missed here; `bugger` arrived in 3.9 and is not
   * in the 3.7/3.8 Runners; `bloody` went the other way and was dropped in
   * 4.0.  Read out of generaltasks in all four decompiled Runners with
   * `index/verbs.py -w <word>` (~/Adrift_decompile), which lists every
   * literal handed to the parser's whole-word matchers.
   */
  {"[shit/fuck/bastard/cunt/crap/hell/shag/bollocks/bollox/piss] *",
   lib_cmd_profanity},
  {"bugger *", lib_cmd_profanity_390},
  {"bloody *", lib_cmd_profanity_pre_400},
  {"[x/ex/exam/examine/look {at}] %object%", lib_cmd_examine_absent},
  {"[x/ex/exam/examine/look {at}] *", lib_cmd_examine_other},
  {"[locate/where {is/are}/find] *", lib_cmd_locate_other},
  {"[cp/mv/ln/ls] *", nullptr, lib_print_message,
   "This isn't Unix you know!\n"},
  {"dir *", nullptr, lib_print_message, "This isn't Dos you know!\n"},
  {"ask %character% *", lib_cmd_ask_npc},
  {"ask %object% *", lib_cmd_ask_object},
  {"ask * about *", lib_cmd_ask_about_nothing},
  {"ask *", lib_cmd_ask_other},
  {"block %object% *", nullptr, lib_cant_do_object, "block"},
  {"block %text%", nullptr, lib_cant_do_other, "block"},
  {"block", nullptr, lib_what_or_cant_do, "block"},
  {"[break/destroy/smash] %object% *", lib_cmd_break_object},
  {"[break/destroy/smash] %object% *", lib_cmd_break_absent},
  {"[break/destroy/smash] %text%", lib_cmd_break_other},
  {"break", lib_cmd_break_what},
  {"destroy", lib_cmd_destroy_what},
  {"smash", lib_cmd_smash_what},
  {"buy %object% *", lib_cmd_buy_object},
  {"buy %object% *", lib_cmd_buy_absent},
  {"buy %text%", lib_cmd_buy_other},
  {"buy", lib_cmd_buy_what},
  {"clean %object% *", nullptr, lib_cant_do_object, "clean"},
  /* clean, wash, cut, shake, climb and sell are therest arms below the
   * 4887A0 clause too. */
  {"clean %text%", lib_cmd_verb_absent_400},
  {"clean %text%", nullptr, lib_cant_do_other, "clean"},
  {"clean", nullptr, lib_what_or_cant_do, "clean"},
  {"climb %object% *", nullptr, lib_cant_do_object, "climb"},
  {"climb %text%", lib_cmd_verb_absent_400},
  {"climb %text%", nullptr, lib_cant_do_other, "climb"},
  {"climb", nullptr, lib_what_or_cant_do, "climb"},
  {"cry *", nullptr, lib_print_message, "There's no need for that!\n"},
  {"cut %object% *", nullptr, lib_cant_do_object, "cut"},
  {"cut %text%", lib_cmd_verb_absent_400},
  {"cut %text%", nullptr, lib_cant_do_other, "cut"},
  {"cut", nullptr, lib_what_or_cant_do, "cut"},
  {"dance *", lib_cmd_dance},
  {"drink %object% *", nullptr, lib_cant_do_object, "drink"},
  {"drink %text%", nullptr, lib_cant_do_other, "drink"},
  {"drink", lib_cmd_drink_what},
  /* therest()'s seen-but-absent clause comes before the eat arm, so a
   * task that hides the noun first (Main Course `eat human`) is "can't see
   * the dead human." and not eat_other's line; see
   * lib_cmd_verb_absent_400(). */
  {"eat *", lib_cmd_verb_absent_400},
  {"eat *", lib_cmd_eat_other},
  {"feed *", nullptr, lib_print_message,
   "There is nothing worth feeding here.\n"},
  {"feel *", lib_cmd_feel},
  {"fight *", nullptr, lib_print_message,
   "There is nothing worth fighting here.\n"},
  /*
   * therest's clear arm (run370 43DFB7 ... run400 4896AC) answers any line
   * holding the word "clear" that the exact-line clear/cls/clr command did
   * not take: "You can't clear the rope." / "... the rope with the knife."
   * (p4WITHQ2, runner_probes/withq2.run400{,.clear}.txt), and "You can't
   * clear that." for a word naming nothing, a turn at every version
   * (runner_probes/admin.run3[78]0.rtf, runner_probes/admin.run390.clear.txt,
   * runner_probes/withq2.run400.clear.txt).
   */
  {"clear %object% *", nullptr, lib_cant_do_object, "clear"},
  {"clear %text%", nullptr, lib_cant_do_other, "clear"},
  {"fix %object% *", lib_cmd_fix_object},
  /* Every therest() verb meets the seen-but-absent clause (4887A0) before
   * its own arm: crashland T8 `fix ship`, the ship seen and left behind, is
   * "You can't see the ship." (run400 fix arm 489BE7 is below the clause). */
  {"fix %text%", lib_cmd_verb_absent_400},
  {"fix %text%", lib_cmd_fix_other},
  {"fix", lib_cmd_fix_what},
  {"fly *", lib_cmd_fly},
  {"hint *", lib_cmd_hint},
  {"hit %character%", lib_cmd_hit_npc},
  {"hit %object% *", lib_cmd_hit_object},
  {"hit %text%", lib_cmd_verb_absent_400},
  {"hit %text%", lib_cmd_hit_other},
  {"hit", lib_cmd_hit_what},
  {"slap %object% *", lib_cmd_slap_object},
  {"slap %text%", lib_cmd_verb_absent_400},
  {"slap %text%", lib_cmd_slap_other},
  {"slap", lib_cmd_slap_what},
  {"hum *", lib_cmd_hum},
  {"jump *", nullptr, lib_print_message, "Wheee-boinng.\n"},
  {"kick %character%", lib_cmd_attack_npc, RUN_ROW_WITH_DEFERRED},
  {"kick %object% *", lib_cmd_kick_object},
  {"kick %text%", lib_cmd_verb_absent_400},
  {"kick %text%", lib_cmd_kick_other},
  {"kick", lib_cmd_kick_what},
  {"kiss %character% *", lib_cmd_kiss_npc},
  {"kiss %object% *", lib_cmd_kiss_object},
  {"kiss *", lib_cmd_kiss_other},
  /* Uninteresting kill message when no weaponry is involved. */
  {"kill *", nullptr, lib_print_message, "Now that isn't very nice.\n"},
  {"lift %object% *", nullptr, lib_cant_do_object, "lift"},
  {"lift %text%", lib_cmd_verb_absent_400},
  {"lift %text%", nullptr, lib_cant_do_other, "lift"},
  {"lift", nullptr, lib_what_or_cant_do, "lift"},
  {"light %object% *", nullptr, lib_cant_do_object, "light"},
  {"light %text%", lib_cmd_verb_absent_400},
  {"light %text%", nullptr, lib_cant_do_other, "light"},
  {"light", nullptr, lib_what_or_cant_do, "light"},
  {"listen *", lib_cmd_listen},
  {"mend %object% *", lib_cmd_mend_object},
  {"mend %text%", lib_cmd_verb_absent_400},
  {"mend %text%", lib_cmd_mend_other},
  {"mend", lib_cmd_mend_what},
  {"move %object% *", nullptr, lib_cant_do_object, "move"},
  {"move %text%", lib_cmd_verb_absent_400},
  {"move %text%", nullptr, lib_cant_do_other, "move"},
  {"move", nullptr, lib_what_or_cant_do, "move"},
  {"please *", lib_cmd_please},
  {"press %object% *", lib_cmd_press_object},
  {"press %text%", lib_cmd_verb_absent_400},
  {"press %text%", lib_cmd_press_other},
  {"press", lib_cmd_press_what},
  {"pull %object% *", lib_cmd_pull_object},
  {"pull %text%", lib_cmd_verb_absent_400},
  {"pull %text%", lib_cmd_pull_other},
  {"pull", lib_cmd_pull_what},
  {"punch *", nullptr, lib_print_message,
   "Who do you think you are, Mike Tyson?\n"},
  {"push %object% *", lib_cmd_push_object},
  {"push %text%", lib_cmd_verb_absent_400},
  {"push %text%", lib_cmd_push_other},
  {"push", lib_cmd_push_what},
  {"repair %object% *", lib_cmd_repair_object},
  {"repair %text%", lib_cmd_verb_absent_400},
  {"repair %text%", lib_cmd_repair_other},
  {"repair", lib_cmd_repair_what},
  /*
   * The rub arm is run400's alone: no "rub" sits in run370, run380 or run390
   * (string census, decompiles), so below 4.0 `rub coin` that no task takes
   * is the object catch-all's "I don't understand what you want me to do with
   * the coin." and `rub,coin` (no whole word "coin") "I don't understand."
   * (p37TASK/p38TASK, runner_probes/task.run3[78]0.comma.rtf; p39TASK
   * runner_probes/task.run390.comma.txt).  Deliberate deviation: Scarier has
   * the rub arm at every version.
   */
  {"rub %object% *", nullptr, lib_cant_do_object, "rub"},
  {"rub %text%", lib_cmd_verb_absent_400},
  {"rub %text%", nullptr, lib_cant_do_other, "rub"},
  {"rub", nullptr, lib_what_or_cant_do, "rub"},
  {"run *", lib_cmd_run},
  {"say *", lib_cmd_say},
  {"sell %object% *", lib_cmd_sell_object},
  {"sell %text%", lib_cmd_verb_absent_400},
  {"sell %text%", lib_cmd_sell_other},
  {"sell", lib_cmd_sell_what},
  {"shake %object% *", lib_cmd_shake_object},
  {"shake %text%", lib_cmd_verb_absent_400},
  {"shake %text%", lib_cmd_shake_other},
  {"shake", lib_cmd_shake_what},
  {"shout *", nullptr, lib_print_message, "Aaarrrrgggghhhhhh!\n"},
  {"sing *", lib_cmd_sing},
  {"sleep *", nullptr, lib_print_message, "Zzzzz.  Bored are you?\n"},
  {"smell %object% *", lib_cmd_smell_object},
  {"smell *", lib_cmd_smell_other},
  {"stop %object% *", nullptr, lib_cant_do_object, "stop"},
  /* stop is a therest arm below the 4887A0 clause too: British
   * Fox T376 `stop engine`, the Britmobile's engine seen elsewhere, is
   * "You can't see Britmobile engine." */
  {"stop %text%", lib_cmd_verb_absent_400},
  {"stop %text%", nullptr, lib_cant_do_other, "stop"},
  {"stop", nullptr, lib_what_or_cant_do, "stop"},
  {"suck %object% *", nullptr, lib_cant_do_object, "suck"},
  {"suck %text%", nullptr, lib_cant_do_other, "suck"},
  {"suck", nullptr, lib_what_or_cant_do, "suck"},
  {"talk to * about *", lib_cmd_ask_about_nothing},
  {"talk *", lib_cmd_talk},
  {"speak *", lib_cmd_speak_pre390},
  {"thank *", nullptr, lib_print_message, "You're welcome.\n"},
  {"turn %object% *", lib_cmd_turn_object},
  {"turn %object% *", lib_cmd_turn_absent},
  /* The clause reads every object the line names, not the one a pattern
   * bound: British Fox T306 `eugene turn on computer`, Sharon's computer
   * seen elsewhere, is "You can't see Sharon's computer.". */
  {"turn %text%", lib_cmd_verb_absent_400},
  {"turn %text%", lib_cmd_turn_other},
  {"turn", lib_cmd_turn_what},
  {"touch %object% *", nullptr, lib_cant_do_object, "touch"},
  {"touch %text%", nullptr, lib_cant_do_other, "touch"},
  {"touch", nullptr, lib_what_or_cant_do, "touch"},
  {"unblock %object% *", nullptr, lib_cant_do_object, "unblock"},
  {"unblock %text%", nullptr, lib_cant_do_other, "unblock"},
  {"unblock", nullptr, lib_what_or_cant_do, "unblock"},
  {"wash %object% *", nullptr, lib_cant_do_object, "wash"},
  {"wash %text%", lib_cmd_verb_absent_400},
  {"wash %text%", nullptr, lib_cant_do_other, "wash"},
  {"wash", nullptr, lib_what_or_cant_do, "wash"},
  {"whistle *", lib_cmd_whistle},
  {"[why/when/what/can/how] *", lib_cmd_interrogation},
  {"xyzzy *", lib_cmd_xyzzy},
  {"campbell", lib_cmd_egotistic},
  {"[yes/no] *", lib_cmd_yes_or_no},
  {"*", lib_cmd_look_anywhere_pre_400},
  {"* %object% *", lib_cmd_verb_object},
  {"put *", lib_cmd_put_where_400},
  {"* %character% *", lib_cmd_verb_npc},

  /* Scarier debugger hook command, placed last just in case... */
  {"{#}debug{ger}", debug_cmd_debugger},

  {nullptr, nullptr}
};


/*
 * STANDARD_ENDED_FALLBACK_COMMANDS
 *
 * What is left of STANDARD_FALLBACK_COMMANDS at 4.0 once a task has ended the
 * game on the line.  run400 calls therest (Proc_19_85_489F4C) at 48AFE4, BELOW
 * generaltasks' gameover jump at 48AC62 (`If MemVar_4941AD <> 0 Then GoTo
 * loc_48B4E3`), so every therest arm -- give, lock, ask, the "You can't <verb>
 * <X>." refusals, the question words -- is lost with the unhandled-verb tail.
 * iachini (runner_transcripts/iachini.txt T185): `turn on tv` runs
 * silent TASK 30, whose only action is End Game, and run400 answers the game's
 * DontUnderstand, not "You can't turn the 32-inch television on.".  Rows whose
 * Runner handler sits above 48AC62 (open/close 48A515, wear 48A48C, remove
 * 48A491) or has not been placed keep their answer; so does the character
 * tail (48B56E, with its own 4805CD gate in lib_cmd_verb_npc()).
 */
scr_commands_t STANDARD_ENDED_FALLBACK_COMMANDS[] = {
  {"open %object%", lib_cmd_open_absent},
  {"open *", lib_cmd_open_ended_400},
  {"close %object%", lib_cmd_close_absent},
  {"close *", lib_cmd_close_ended_400},
  {"sit {down/up} [on/in] *", lib_cmd_verb_absent_400},
  {"sit {down/up} [on/in] *", lib_cmd_sit_other},
  {"stand {up/down} [on/in] *", lib_cmd_verb_absent_400},
  {"stand {up/down} [on/in] *", lib_cmd_stand_other},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_verb_absent_400},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_lie_other},
  {"[remove/take off/doff] *", lib_cmd_remove_what},
  {"[drop/put down] *", lib_cmd_drop_what},
  {"[wear/put on/don] *", lib_cmd_wear_what},
  {"put *", lib_cmd_put_unclear},
  {"[shit/fuck/bastard/cunt/crap/hell/shag/bollocks/bollox/piss] *",
   lib_cmd_profanity},
  {"bugger *", lib_cmd_profanity_390},
  {"[x/ex/exam/examine/look {at}] %object%", lib_cmd_examine_absent},
  {"[x/ex/exam/examine/look {at}] *", lib_cmd_examine_other},
  {"[locate/where {is/are}/find] *", lib_cmd_locate_other},
  {"hint *", lib_cmd_hint},
  {"kiss *", lib_cmd_kiss_ended_400},
  {"* %character% *", lib_cmd_verb_npc},
  {nullptr, nullptr}
};
