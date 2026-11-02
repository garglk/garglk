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
 * The library command tables and the passes that walk them: the priority
 * pass, the standard verb and fallback buckets, pre-4.0 therest() and the
 * anywhere handlers.  Which library row answers a line, and when.
 *
 * Split out of scrun_dispatch.cpp; see scrunner.h for what the runner files
 * share.
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
static scr_commands_t MOVE_COMMANDS_4[] = {
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
static scr_commands_t MOVE_COMMANDS_8[] = {
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

/* "Priority" library commands, may take precedence over the game. */
static scr_commands_t PRIORITY_COMMANDS[] = {

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
   * "drop X in Y" and "drop X on Y" are Adrift's put handlers wearing a
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
  {"[drop/put down] [all/everything] [in/into/inside {of}] %object%",
   lib_cmd_put_all_in},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%"
   " [in/into/inside {of}] %object%", lib_cmd_put_in_except_multiple},
  {"[drop/put down] %text% [in/into/inside {of}] %object%",
   lib_cmd_put_in_multiple},
  {"[drop/put down] [all/everything] [on/onto/on top of] %object%",
   lib_cmd_put_all_on},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%"
   " [on/onto/on top of] %object%", lib_cmd_put_on_except_multiple},
  {"[drop/put down] %text% [on/onto/on top of] %object%",
   lib_cmd_put_on_multiple},
  /*
   * The plain "put" spellings of the same handlers.  Priority placement is
   * live-verified (run400, 2026-08-02, probe FM7 / TheADRIFTProject): a
   * completable put beats a matched-but-failing task, so these must run
   * before the loud-fail task pass; the refusal cases defer from here (see
   * run_priority_commands) and print from the STANDARD_COMMANDS duplicates.
   */
  {"put [all/everything] [in/into/inside {of}] %object%", lib_cmd_put_all_in},
  {"put [all/everything] [[except/but] {for}/apart from] %text%"
   " [in/into/inside {of}] %object%", lib_cmd_put_in_except_multiple},
  {"put %text% [in/into/inside {of}] %object%", lib_cmd_put_in_multiple},
  {"put [all/everything] [on/onto/on top of] %object%", lib_cmd_put_all_on},
  {"put [all/everything] [[except/but] {for}/apart from] %text%"
   " [on/onto/on top of] %object%", lib_cmd_put_on_except_multiple},
  {"put %text% [on/onto/on top of] %object%", lib_cmd_put_on_multiple},
  /* 3.9's "can't put anything inside/onto that!"; see lib_put_that_390(). */
  {"put %text% [in/into/inside {of}] *", lib_cmd_put_in_that_390},
  {"put %text% [on/onto/on top of] *", lib_cmd_put_on_that_390},
  {"[[drop/put down] [all/everything]/put [all/everything] down]",
   lib_cmd_drop_all},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%",
   lib_cmd_drop_except_multiple},
  {"[drop/put down] %text%", lib_cmd_drop_multiple},
  {"put %text% down", lib_cmd_drop_multiple},
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
   * pass).  This matches the Adrift runner -- e.g. "i" in Lair of the
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
static scr_commands_t STANDARD_COMMANDS[] = {

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
  {"[ex/exam/examine/look {at}] all", lib_cmd_examine_all},
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
  {"[x/ex/exam/examine/look {at}] all", lib_cmd_examine_all},
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
  {"put [all/everything] [in/into/inside {of}] %object%", lib_cmd_put_all_in},
  {"put [all/everything] [[except/but] {for}/apart from] %text%"
   " [in/into/inside {of}] %object%", lib_cmd_put_in_except_multiple},
  {"put %text% [in/into/inside {of}] %object%", lib_cmd_put_in_multiple},
  {"put [all/everything] [on/onto/on top of] %object%", lib_cmd_put_all_on},
  {"put [all/everything] [[except/but] {for}/apart from] %text%"
   " [on/onto/on top of] %object%", lib_cmd_put_on_except_multiple},
  {"put %text% [on/onto/on top of] %object%", lib_cmd_put_on_multiple},
  {"[drop/put down] [all/everything] [in/into/inside {of}] %object%",
   lib_cmd_put_all_in},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%"
   " [in/into/inside {of}] %object%", lib_cmd_put_in_except_multiple},
  {"[drop/put down] %text% [in/into/inside {of}] %object%",
   lib_cmd_put_in_multiple},
  {"[drop/put down] [all/everything] [on/onto/on top of] %object%",
   lib_cmd_put_all_on},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%"
   " [on/onto/on top of] %object%", lib_cmd_put_on_except_multiple},
  {"[drop/put down] %text% [on/onto/on top of] %object%",
   lib_cmd_put_on_multiple},
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
   * anywhere in the line (run400 48022F-480384, run390 45A0BA, run380
   * 440E8C, run370 438F79), so `give cathy diary` and `give diary cathy`
   * both offer the diary.  House (4.00) runner_probes/house.run400.b.txt
   * 2026-09-06: `give cathy diary` gets the same reply as `give diary to
   * cathy`.  Scarier used to fall to "Give the diary to who?".
   */
  {"give %character% %object%", lib_cmd_give_object_npc},
  {"give %object% %character%", lib_cmd_give_object_npc},
  {"sit {down/up} [on/in] %object%", lib_cmd_sit_on_object},
  {"stand {up/down} [on/in] %object%", lib_cmd_stand_on_object},
  {"[lie/lay] on %object%", lib_cmd_lie_on_object},
  /*
   * `get up` is a `stand` synonym in every Runner; `get off` and `get down`
   * are `stand`-off-a-thing synonyms that arrived in 3.9.  The sitstand
   * proc tests `c("get off") Or c("get down")` (run400 loc_46B6E4, run390
   * loc_44432x); neither string appears anywhere in run370/run380.
   * Measured live on the one file microwaveman.taf -- a 3.80 game -- run
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
  {"get on %object%", lib_cmd_get_on_object},
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
   * `ask`: every Runner guards it with `c("ask") Or c("talk to")` (run370
   * loc_4387F4, run380 loc_440683, run390 loc_4597F2, run400 loc_47F8F7).
   * `speak to` is not in that list at any version, so it only ever reaches
   * the ask-format hint below, even where a topic would have matched.  A
   * `talk to` with no matching topic lands on the hint too -- see
   * lib_ask_npc_about().
   *
   * The hint itself is the per-character pass's `talk`/`speak` branch, and
   * its guard is where the versions part: 3.7 and 3.8 match the bare words
   * (run370 loc_438748, run380 loc_4405D7), 3.9 and 4.0 require the "to"
   * (run390 loc_45973D, run400 loc_47F84A).  Hence the second row's gate.
   * Both rows sit ahead of `talk *` further down, which is the generaltasks
   * rabblings line -- the fall-through 3.9/4.0 leave for a bare `talk bob`.
   * See lib_cmd_talk_to_npc().
   */
  {"talk to %character% about %text%", lib_cmd_talk_to_npc_about},
  {"[talk/speak] to %character% *", lib_cmd_talk_to_npc},
  {"[talk/speak] %character% *", lib_cmd_talk_to_npc_pre_390},
  {"[attack/kick] %character% with %object%", lib_cmd_attack_npc_with},
  {"chop %character% with %object%", lib_cmd_chop_npc_with},
  {"cut %character% with %object%", lib_cmd_cut_npc_with},
  {"hit %character% with %object%", lib_cmd_hit_npc_with},
  {"shoot %character% with %object%", lib_cmd_shoot_npc_with},
  {"stab %character% with %object%", lib_cmd_stab_npc_with},
  {"throw %object% at %character%", lib_cmd_throw_npc_with},
  {"kill %character% with %object%", lib_cmd_kill_npc_with},
  {"fight %character% with %object%", lib_cmd_fight_npc_with},
  /*
   * `slap` is a pre-parse rewrite to `hit`, not a grammar verb, and it is
   * space-bounded in 4.0 so a command-initial `slap` never fires there.
   * `smack` is in no Runner at all.  See lib_cmd_slap_*() for the sites.
   */
  {"[attack/kick] %character%", lib_cmd_attack_npc},
  {"slap %character% with %object%", lib_cmd_slap_npc_with},
  {"slap %character%", lib_cmd_slap_npc},
  {"chop %character%", lib_cmd_chop_npc},
  {"cut %character%", lib_cmd_cut_npc},
  {"shoot %character%", lib_cmd_shoot_npc},
  {"stab %character%", lib_cmd_stab_npc},
  {"kill %character%", lib_cmd_kill_npc},
  {"fight %character%", lib_cmd_fight_npc},
  /*
   * One line naming several NPCs: dobattle strikes every one of them, where
   * Scarier asks which was meant and strikes that one alone (a deliberate
   * deviation, see lib_battle_attack_many()); a battle verb naming no NPC
   * at all asks "Who do you want to attack?".  `throw` is one of var_90's
   * verbs too (47E9EF-47EADB): les_feux T115 `throw grappin on rocher`
   * asks Who.
   */
  {"[attack/kick/fight/kill/chop/cut/hit/shoot/stab/throw] %text%"
   " with %object%", lib_cmd_attack_npcs_with},
  {"[attack/kick/fight/kill/chop/cut/hit/shoot/stab/throw] %text%",
   lib_cmd_attack_npcs},
  /* A bare battle verb asks too, ahead of `kick`'s and `hit`'s "what?". */
  {"[attack/kick/fight/kill/chop/cut/hit/shoot/stab/throw]",
   lib_cmd_attack_npcs},

  /* More movement, waiting, and miscellaneous administrative commands. */
  /*
   * `go` and `enter` are the two verbs generaltasks answers with a nudge back
   * to the compass -- run370 loc_43DD8B / loc_43DDB4, run380 loc_44481C /
   * loc_444845, run390 loc_45DF66 / loc_45DF83, run400 loc_48936C /
   * loc_489383.  The movement table above has already taken every bare
   * direction word, `enter` and `exit` among them, so what is left here is a
   * bare `go` and an `enter` with something attached.  See
   * lib_cmd_just_a_direction().
   *
   * `goto`/`go to` keep their own rows below: they are the Runner's
   * gotoplace(), which exits at once on a bare `go`, `goto` or `go to`
   * (run390 loc_43C7B0, run400 loc_464998) and so leaves those to the nudge
   * as well.
   */
  {"go", lib_cmd_just_a_direction},
  {"go to", lib_cmd_just_a_direction},
  {"enter *", lib_cmd_just_a_direction},

  /*
   * gotoplace() itself, which tests the whole line: "goto" anywhere, or a
   * line starting "go " (3.9+, and a deviation below; see
   * lib_goto_line_enters()) or "go to" (3.7, 3.8).  See lib_cmd_go_place().
   * What it leaves is a bare `goto`, which the Runner answers with
   * DontUnderstand (run380x runner_probes/goto.run380.b.rtf, run390x
   * runner_probes/goto.run390.b.txt, run400x
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
   * `z` only entered the Runner vocabulary at 3.90 (index/verbs.py; cave.taf
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
  {"!%number%", lib_cmd_redo_number},
  {"!%text%", lib_cmd_redo_text},
  {"!", lib_cmd_redo_last},
#else
  {"[redo /!]%number%", lib_cmd_redo_number},
  {"[redo /!]%text%", lib_cmd_redo_text},
  {"[redo/!]", lib_cmd_redo_last},
#endif
  /*
   * `bye` and `end` are exact synonyms of `quit` in every Runner -- one
   * three-way whole-line test that unloads the form and answers "I'm so glad
   * you said no..." if the confirmation is declined (run400 loc_48AA85-48AAC9,
   * run370 loc_43C06B).  `q` is ours, not theirs; see the audit note above.
   */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[quit/bye/end]", lib_cmd_quit},
#else
  {"[quit/q/bye/end]", lib_cmd_quit},
#endif
  {"endgame", lib_cmd_endgame},
  /*
   * Audited 2026-09-07 against the four Runner constant pools and the 426-game
   * v4 corpus (test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md, "Audited
   * 2026-09-07"): besides the `stats` row below, `hints`, `q`, `brief`,
   * `verbose`, `notify`, `notification`, `redo`, `hist`, `gpl`, `license` and
   * `statusline` are all SCARE inventions no Runner accepts (the Runner words
   * are `hint`, `quit`, `history`).  A game task is matched before
   * run_standard_commands(), so any task carrying COMPLETE text wins and the
   * invention never runs; a task that does not claim its line left the gap
   * that `stats` fell into, and run_game_owns_scare_word() closes it for the
   * ten that are words (2026-10-11): a word some task of the game names is
   * answered as the Runner would.  `q` and the shorthands are not covered.
   *
   * All eleven are compiled out by SCARIER_NO_ABBREVIATIONS, alongside the
   * `g`/`i`/`z` shorthands, so that build offers a game exactly the meta
   * vocabulary the Runner does and can never steal a line from a task.  The
   * default build keeps them: they are useful, and the corpus says they are
   * harmless.
   *
   * `turns`/`undo` are 3.80+ and `version` 3.90+ in the Runner; left ungated
   * here because no 3.70/3.80 corpus game names them.
   */
  {"turns", lib_cmd_turns},
  {"score", lib_cmd_score},
  {"undo", lib_cmd_undo},
  /* `past` is the Runner's own synonym of `history` -- the two are one
   * whole-line test in all four (run370 loc_43BA2A, run380 loc_44228D,
   * run400 loc_48A51A).  The `%number%` form is a SCARE extension on top of
   * both, and `hist` is ours alone; see the audit note below. */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[history/past] %number%", lib_cmd_history_number},
  {"[history/past]", lib_cmd_history},
  {"hint", lib_cmd_hints},
#else
  {"[hist/history/past] %number%", lib_cmd_history_number},
  {"[hist/history/past]", lib_cmd_history},
  {"[hint/hints]", lib_cmd_hints},
  {"verbose", lib_cmd_verbose},
  {"brief", lib_cmd_brief},
  {"[notify/notification] %text%", lib_cmd_notify_on_off},
  {"[notify/notification]", lib_cmd_notify},
#endif
  {"time", lib_cmd_time},
  {"date", lib_cmd_date},
  {"[help/commands]", lib_cmd_help},
#ifndef SCARIER_NO_ABBREVIATIONS
  {"[gpl/license]", lib_cmd_license},
#endif
  {"[about/info/information/author]", lib_cmd_information},
  {"[clear/cls/clr]", lib_cmd_clear},
#ifndef SCARIER_NO_ABBREVIATIONS
  {"[statusline/stats]", lib_cmd_statusline},
#endif
  {"[control panel/control-panel/control/panel]", lib_cmd_control_panel},
  /*
   * `stats` is a SCARE invention: the string does not occur in ANY of the
   * four Runner binaries, and it stole `suburbanprodigy3` T31 from the game's
   * own task.  Dropped 2026-09-07 for that (test/adrift4/notes/
   * WINE-TRANSCRIPTS-TODO.md "Ported 2026-09-07: `stats` is not a Runner
   * command") and back 2026-10-11 as a name for `statusline` above, now that
   * run_game_owns_scare_word() hands the word to a game with a task for it.
   * `status` itself is 3.90/4.00 only and lives solely inside the
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
 * (man_overboard, 4.00, run400 transcript line 170) rather than fall to
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
 * runner_probes/dark.run390.feed5.txt, 2026-09-10).  Both rows end in a
 * wildcard because the container slot resolved nothing; the %object% rows for
 * the same shapes are up in PRIORITY_COMMANDS and have already declined by
 * here.  See lib_cmd_take_from_nowhere().
 */
static scr_commands_t STANDARD_TAKE_FROM_COMMANDS[] = {
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
static scr_commands_t STANDARD_PUT_COMMANDS[] = {
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

static scr_commands_t STANDARD_ABOVE_REFUSAL_COMMANDS[] = {
  /*
   * The 4.0 named take for a noun that names only objects the player has
   * seen elsewhere; it declines to anything else, leaving "Take what?" to
   * the catch-all below it.  See lib_cmd_take_absent().
   */
  {"[get/take/pick up/pick] %object%", lib_cmd_take_absent},
  {"[get/take/pick up/pick] *", lib_cmd_get_what},
  /*
   * run380's drops() (438FF0) writes "You don't have <Prefix> <Short>!" for a
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
 * special." for a noun that names nothing (44BF94) -- so an examine line that
 * an out-of-room task also matches is "Nothing special.", not "You can't do
 * that here!".  Gamma Gals (3.90) `look under dresser` and `look under bed`
 * typed outside the tasks' rooms (run390x, Delron walkthrough replay
 * 2026-10-09).  3.9 only: 3.7/3.8 have no bare look in examines and are
 * unmeasured; 4.0's refusal is not here.
 */
scr_commands_t EXAMINE_ABOVE_REFUSAL_390_COMMANDS[] = {
  {"[x/ex/exam/examine/look {at}] %object%", lib_cmd_examine_absent},
  {"[x/ex/exam/examine/look {at}] *", lib_cmd_examine_other},
  {nullptr, nullptr}
};

static scr_commands_t STANDARD_FALLBACK_COMMANDS[] = {
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
  {"[cp/mv/ln/ls] *", lib_cmd_unix_like},
  {"dir *", lib_cmd_dos_like},
  {"ask %character% *", lib_cmd_ask_npc},
  {"ask %object% *", lib_cmd_ask_object},
  {"ask * about *", lib_cmd_ask_about_nothing},
  {"ask *", lib_cmd_ask_other},
  {"block %object% *", lib_cmd_block_object},
  {"block %text%", lib_cmd_block_other},
  {"block", lib_cmd_block_what},
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
  {"clean %object% *", lib_cmd_clean_object},
  /* clean (488F02), wash (488FFC), cut (489041), shake (48950E), climb
   * (4898F5) and sell (489A3D) are therest arms below the 4887A0 clause too. */
  {"clean %text%", lib_cmd_verb_absent_400},
  {"clean %text%", lib_cmd_clean_other},
  {"clean", lib_cmd_clean_what},
  {"climb %object% *", lib_cmd_climb_object},
  {"climb %text%", lib_cmd_verb_absent_400},
  {"climb %text%", lib_cmd_climb_other},
  {"climb", lib_cmd_climb_what},
  {"cry *", lib_cmd_cry},
  {"cut %object% *", lib_cmd_cut_object},
  {"cut %text%", lib_cmd_verb_absent_400},
  {"cut %text%", lib_cmd_cut_other},
  {"cut", lib_cmd_cut_what},
  {"dance *", lib_cmd_dance},
  {"drink %object% *", lib_cmd_drink_object},
  {"drink %text%", lib_cmd_drink_other},
  {"drink", lib_cmd_drink_what},
  /* therest()'s seen-but-absent clause comes before the eat arm, so a
   * task that hides the noun first (Main Course `eat human`) is "can't see
   * the dead human." and not eat_other's line; see
   * lib_cmd_verb_absent_400(). */
  {"eat *", lib_cmd_verb_absent_400},
  {"eat *", lib_cmd_eat_other},
  {"feed *", lib_cmd_feed},
  {"feel *", lib_cmd_feel},
  {"fight *", lib_cmd_fight},
  {"clear %object% *", lib_cmd_clear_object},
  {"clear %text%", lib_cmd_clear_other},
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
  {"jump *", lib_cmd_jump},
  {"kick %character%", lib_cmd_attack_npc},
  {"kick %object% *", lib_cmd_kick_object},
  {"kick %text%", lib_cmd_verb_absent_400},
  {"kick %text%", lib_cmd_kick_other},
  {"kick", lib_cmd_kick_what},
  {"kiss %character% *", lib_cmd_kiss_npc},
  {"kiss %object% *", lib_cmd_kiss_object},
  {"kiss *", lib_cmd_kiss_other},
  {"kill *", lib_cmd_kill_other},
  {"lift %object% *", lib_cmd_lift_object},
  {"lift %text%", lib_cmd_verb_absent_400},
  {"lift %text%", lib_cmd_lift_other},
  {"lift", lib_cmd_lift_what},
  {"light %object% *", lib_cmd_light_object},
  {"light %text%", lib_cmd_verb_absent_400},
  {"light %text%", lib_cmd_light_other},
  {"light", lib_cmd_light_what},
  {"listen *", lib_cmd_listen},
  {"mend %object% *", lib_cmd_mend_object},
  {"mend %text%", lib_cmd_verb_absent_400},
  {"mend %text%", lib_cmd_mend_other},
  {"mend", lib_cmd_mend_what},
  {"move %object% *", lib_cmd_move_object},
  {"move %text%", lib_cmd_verb_absent_400},
  {"move %text%", lib_cmd_move_other},
  {"move", lib_cmd_move_what},
  {"please *", lib_cmd_please},
  {"press %object% *", lib_cmd_press_object},
  {"press %text%", lib_cmd_verb_absent_400},
  {"press %text%", lib_cmd_press_other},
  {"press", lib_cmd_press_what},
  {"pull %object% *", lib_cmd_pull_object},
  {"pull %text%", lib_cmd_verb_absent_400},
  {"pull %text%", lib_cmd_pull_other},
  {"pull", lib_cmd_pull_what},
  {"punch *", lib_cmd_punch},
  {"push %object% *", lib_cmd_push_object},
  {"push %text%", lib_cmd_verb_absent_400},
  {"push %text%", lib_cmd_push_other},
  {"push", lib_cmd_push_what},
  {"repair %object% *", lib_cmd_repair_object},
  {"repair %text%", lib_cmd_verb_absent_400},
  {"repair %text%", lib_cmd_repair_other},
  {"repair", lib_cmd_repair_what},
  {"rub %object% *", lib_cmd_rub_object},
  {"rub %text%", lib_cmd_verb_absent_400},
  {"rub %text%", lib_cmd_rub_other},
  {"rub", lib_cmd_rub_what},
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
  {"shout *", lib_cmd_shout},
  {"sing *", lib_cmd_sing},
  {"sleep *", lib_cmd_sleep},
  {"smell %object% *", lib_cmd_smell_object},
  {"smell *", lib_cmd_smell_other},
  {"stop %object% *", lib_cmd_stop_object},
  /* stop (488F56) is a therest arm below the 4887A0 clause too: British
   * Fox T376 `stop engine`, the Britmobile's engine seen elsewhere, is
   * "You can't see Britmobile engine." */
  {"stop %text%", lib_cmd_verb_absent_400},
  {"stop %text%", lib_cmd_stop_other},
  {"stop", lib_cmd_stop_what},
  {"suck %object% *", lib_cmd_suck_object},
  {"suck %text%", lib_cmd_suck_other},
  {"suck", lib_cmd_suck_what},
  {"talk to * about *", lib_cmd_ask_about_nothing},
  {"talk *", lib_cmd_talk},
  {"speak *", lib_cmd_speak_pre390},
  {"thank *", lib_cmd_thank},
  {"turn %object% *", lib_cmd_turn_object},
  {"turn %object% *", lib_cmd_turn_absent},
  /* The clause reads every object the line names, not the one a pattern
   * bound: British Fox T306 `eugene turn on computer`, Sharon's computer
   * seen elsewhere, is "You can't see Sharon's computer.". */
  {"turn %text%", lib_cmd_verb_absent_400},
  {"turn %text%", lib_cmd_turn_other},
  {"turn", lib_cmd_turn_what},
  {"touch %object% *", lib_cmd_touch_object},
  {"touch %text%", lib_cmd_touch_other},
  {"touch", lib_cmd_touch_what},
  {"unblock %object% *", lib_cmd_unblock_object},
  {"unblock %text%", lib_cmd_unblock_other},
  {"unblock", lib_cmd_unblock_what},
  {"wash %object% *", lib_cmd_wash_object},
  {"wash %text%", lib_cmd_verb_absent_400},
  {"wash %text%", lib_cmd_wash_other},
  {"wash", lib_cmd_wash_what},
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
 * run_priority_commands()
 * run_standard_commands()
 *
 * Compare a user input string against commands recognized by the library,
 * and action any command.  Returns TRUE if the string matched a command
 * that then ran successfully, FALSE otherwise.
 *
 * "Priority" commands are ones that Adrift seems to action no matter what
 * the game tries to override.  For example, a simple game with one "ball"
 * object and a task "* ball *" should, if the task is restricted, override
 * "take ball" such that the ball can never be acquired.  Adrift lets the
 * "take" succeed, though (and more curiously, may respond "I don't
 * understand..." to "drop ball").  This could be an Adrift bug.  Shrug.
 *
 * For now, I can't find any better way to try to handle it than to make
 * object acquisition take precedence over game commands.
 *
 * The put-in/put-on family runs here TENTATIVELY: probed live against
 * run400 (2026-08-02, probe FM7 + a TheADRIFTProject .tas transplant), the
 * real Runner lets a put that can actually complete beat a matched task
 * whose restrictions fail ("put pill in cup" answers the library's "You put
 * the small pill inside the coffee cup." while the task's fail message is
 * suppressed), but when the put would be REFUSED (target not a container/
 * surface) the failing task's message wins ("drop pill in slime" prints the
 * fail message, not the refusal).  So in this pass the validity check
 * defers instead of refusing -- run_priority_defer() -- and the scan stops
 * so the plain-drop %text% rows cannot swallow the input; the loud-fail
 * task pass then gets its chance, and the duplicate rows in
 * STANDARD_COMMANDS print the refusal when no task claims it.
 */
#ifdef SCARIER_DUMP_TOOLS
/* SCR_TRACE_ADMIN: the last command dispatched, for the ADMIN trace line. */
std::string run_trace_last_input;
#endif

static scr_bool run_priority_pass_active = FALSE;

scr_bool run_priority_deferred = FALSE;

scr_bool run_priority_refused = FALSE;

static scr_bool run_priority_unnamed_put = FALSE;

scr_bool
run_in_priority_pass (void)
{
  return run_priority_pass_active;
}

/*
 * run_in_put_clause_loop()
 *
 * TRUE while run_game_commands_common() is running the clauses of a 4.0 put
 * list one at a time; see lib_put_clauses_400() and lib_put_implicit_take().
 */
scr_bool run_put_clause_loop_active = FALSE;

scr_bool
run_in_put_clause_loop (void)
{
  return run_put_clause_loop_active;
}

static void
run_priority_defer (void)
{
  assert (run_priority_pass_active);
  run_priority_deferred = TRUE;
}

/*
 * run_priority_defer_if_active()
 *
 * run_priority_defer(), but only inside the tentative priority pass: a
 * handler about to print a refusal leaves the line for a matched task's fail
 * message to claim first (see lib_put_in_is_valid()).  Returns TRUE if it
 * deferred, in which case the caller returns FALSE without printing.
 */
scr_bool
run_priority_defer_if_active (void)
{
  if (!run_priority_pass_active)
    return FALSE;

  run_priority_defer ();
  return TRUE;
}

/*
 * run_priority_refuse()
 *
 * A priority command has printed a refusal that does NOT claim the command:
 * the 4.0 put whose every object was turned away on size or capacity.  The
 * handler returns FALSE after calling this, the table walk stops, and
 * run_line_t::put_pass() lets the task passes answer the same line, joined
 * onto the refusal.  See the 4.0 put notes on run_line_t::put_prepass().
 */
void
run_priority_refuse (void)
{
  assert (run_priority_pass_active);
  run_priority_refused = TRUE;
}

/*
 * run_priority_unnamed_put_object()
 *
 * 4.0's "put X in Y" where X names nothing leaves the command line
 * CLOBBERED, and the task passes never see what was typed.  run400 hands
 * every line holding the whole word "put" or "drop" to put_drop_list
 * (Proc_19_40_459DB4) ahead of the task dispatch; that normalises the line
 * ("drop " -> "put ", "inside"/"into" -> "in", "onto" -> "on"), splits it
 * at " in ", and calls name_object (Proc_19_41_46E5D8) to name the direct
 * object.  name_object sets the global command line MemVar_494174 to the
 * fragment Left(line, split) -- "put ice cream " -- and resolves that
 * (Proc_21_58_463640 mode 2).  Every other outcome leaves by 46E5CD, which
 * puts the line back; the one that does not is 46E142, reached when the
 * fragment names nothing at all.  There the Runner prints "It is not clear
 * which object you are referring to." ("Drop what?" for a "drop" line), but
 * only when no put/drop-class task pre-matches the typed line
 * (Proc_19_35_453C50 mode 2), and then returns at 46E23B with the fragment
 * still installed.  generaltasks dispatches the tasks against THAT, so a
 * task that would have claimed the typed line never matches, and the
 * catch-all -- whose noun was resolved up front from the original line --
 * answers instead: "I don't understand what you want to do with the cone."
 *
 * Measured on IceCream.taf in run400 (runner_probes/icecream.run400.put.txt,
 * 2026-09-07), whose three `put/place/set [the] ice cream in/on [the] cone`
 * tasks share one pattern: `put ice cream in cone` and `put the ice cream in
 * the cone` both come out as the catch-all, while `place ice cream in cone`
 * runs the task (place never enters put_drop_list) and `put ice cream on
 * cone` runs it too -- the "on" branch has an escape of its own at 459C39
 * that zeroes the split when the fragment resolves to nothing, so the line
 * reaches the tasks intact.  The clobber is the "in" form only.
 *
 * The put-in handler signals the fragment with this, and the put refusals
 * read it back through run_priority_put_was_unnamed().  The task passes
 * get the line as typed instead; see run_line_t::get_outer_400().
 */
void
run_priority_unnamed_put_object (void)
{
  assert (run_priority_pass_active);
  run_priority_unnamed_put = TRUE;
}

/* TRUE when this line's put-in went silent at 46E15A; see
 * lib_cmd_put_unclear(). */
scr_bool
run_priority_put_was_unnamed (void)
{
  return run_priority_unnamed_put;
}

scr_bool
run_priority_commands (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  scr_commandsref_t command;

  run_priority_pass_active = TRUE;
  run_priority_deferred = FALSE;
  run_priority_refused = FALSE;
  run_priority_unnamed_put = FALSE;
  for (command = PRIORITY_COMMANDS; command->command; command++)
    {
      if (uip_match (command->command, string, game))
        {
          if (command->handler (game))
            {
              run_priority_pass_active = FALSE;
              return TRUE;
            }
          if (run_priority_deferred || run_priority_refused)
            break;
        }
    }
  run_priority_pass_active = FALSE;

  /* Nothing matched the string.  Or if it did, its handler failed. */
  return FALSE;
}

/*
 * run_is_put_command()
 *
 * TRUE if the string is one of the priority table's NAMED put/drop rows --
 * "put X in Y", "drop X on Y", "drop X", "put X down".  These are the rows
 * run400's put_drop_list answers itself, above the task dispatcher; the
 * all/everything rows are deliberately NOT among them.  The probe binds
 * object references as a side effect, so they are saved and put back, as
 * run_task_reachable_by_library_callback() does for its own speculative
 * matches.
 *
 * Which side of the line a row falls on was measured on p4REPEAT2.taf and
 * p4REPEAT3.taf (run400 runner_probes/repeat2.run400.txt,
 * runner_probes/repeat3.run400.txt, 2026-09-08), where every one of the
 * probe's cells is a task whose command is the typed line, none of them
 * done, all with a RepeatText:
 *
 *   `drop coin`, `drop hat`   library, and the task NEVER ran (no "C2"/"C3")
 *   `put coin on desk`, `put hat on desk`               likewise (no C3/C4)
 *   `drop all`, `put all on desk`     the TASK ran -- "C14"/"C15", and the
 *                                     second time round its RepeatText
 *
 * The named rows are the whole of run400's drop routine Proc_19_7_46FB8C:
 * its var_88 = 0 arm resolves the noun itself and composes "<player> drop
 * <object>." at loc_46F732-46F76C without ever asking the dispatcher.  The
 * `all` arm does the opposite -- loc_46F1D8 tests the word "all" and, still
 * before anything is printed, hands the line to a task
 * (`Proc_19_35_453C50("drop all")` at 46F1EA) and returns from the routine
 * outright when one claims it (`Result: End Sub`, 46F1F8).  So an "all"
 * line reaches the tasks first and a named one does not, and the empty-hands
 * "You're not carrying anything." (loc_46F457, and 46FB33 for the put half)
 * is only ever reached once the tasks have declined.
 */
scr_bool
run_is_put_command (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_ref_entity_guard ref_entity (game);
  std::vector<scr_bool> references (game->object_references);
  scr_commandsref_t command;
  scr_bool is_put = FALSE;

  /*
   * Take the table in order and stop at the first row that matches, exactly
   * as run_priority_commands() does: "drop all" matches the named row
   * "[drop/put down] %text%" too, with %text% = "all", and it is only
   * because the all/everything row sits above it that the Runner's `all`
   * arm is the one that runs.  Scanning for the named handlers alone made
   * every "all" line look named.
   */
  for (command = PRIORITY_COMMANDS; command->command; command++)
    {
      if (!uip_match (command->command, string, game))
        continue;
      /* The container-first rows answer only their three exits and pass
       * everything else down to these; look through them. */
      if (command->handler == lib_cmd_put_container_400)
        continue;

      is_put = command->handler == lib_cmd_put_in_multiple
               || command->handler == lib_cmd_put_on_multiple
               || command->handler == lib_cmd_drop_multiple;
      break;
    }

  game->object_references = references;
  return is_put;
}

/*
 * run_is_put_command_400()
 *
 * run_is_put_command(), and at 4.0 a second look by whole-word containment
 * when the container phrase names a fitting container or surface to run400's
 * scorer but the put rows' positional %object% does not -- `put my homework
 * on my desk`, the desk's Prefix being "your school"; see
 * lib_put_container_fits_400().  *CONTAINED says the put rows need that
 * containment again when they run.
 */
scr_bool
run_is_put_command_400 (scr_gameref_t game, const scr_char *string,
                        scr_bool *contained)
{
  scr_bool is_put;

  *contained = FALSE;
  if (run_is_put_command (game, string))
    return TRUE;
  if (!lib_put_container_fits_400 (game, string))
    return FALSE;

  {
    const scr_containment_guard containment (TRUE);

    is_put = run_is_put_command (game, string);
  }
  *contained = is_put;
  return is_put;
}

/*
 * run_is_inventory_command()
 *
 * TRUE for the lines run400's inventory handler answers.  That handler,
 * Proc_19_70_45C304, is called at loc_48A457 -- above the task dispatcher at
 * 48A481 -- so its listing is in the message buffer before any task runs,
 * and a task that then matches the same line has its CompleteText APPENDED
 * to the listing rather than replacing it.  Measured on p4REPEAT2/3
 * (runner_probes/repeat2.run400.txt, runner_probes/repeat3.run400.txt,
 * 2026-09-08): the first `i` answers "You are carrying a coin and a hat.  C1
 * i.", and `inv` / `inventory` the same way.  (The listing is also why a
 * spent task's RepeatText never survives here -- the 44CC7D store is gated
 * on the buffer still being empty; that is the survivor table's first row.)
 */
scr_bool
run_is_inventory_command (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_ref_entity_guard ref_entity (game);
  scr_commandsref_t command;
  scr_bool is_inventory = FALSE;

  for (command = PRIORITY_COMMANDS; command->command && !is_inventory;
       command++)
    {
      if (command->handler != lib_cmd_inventory)
        continue;

      is_inventory = uip_match (command->command, string, game);
    }

  return is_inventory;
}

/*
 * run_repeat_survivor_400()
 *
 * TRUE if a 4.0 line is one that a spent task's RepeatText does NOT take
 * away from the standard library.  Measured 2026-09-08 with p4REPEAT.taf,
 * p4REPEAT2.taf and p4REPEAT3.taf (make_400_repeatprobe.py and
 * make_400_repeatprobe2.py, 30-odd one-cell tasks all done, non-repeatable
 * and carrying a RepeatText, each typed twice) under run400, transcripts
 * runner_probes/repeat.run400.txt, runner_probes/repeat2.run400.txt and
 * runner_probes/repeat3.run400.txt.
 *
 * run400 prints a done non-repeatable task's RepeatText inside the task
 * dispatcher at 48A481 (Proc_19_24_44CCE0) and then jumps to loc_48B4E3,
 * past every general library verb.  Only two groups of handlers escape it:
 *
 *   - the ones the input routine runs BEFORE the dispatcher: inventory
 *     (48A457) and the put/drop list (48A462).  They print first, and the
 *     RepeatText is emitted only while the output is still empty, so
 *     whatever they say silences it -- `i` lists the inventory, `drop coin`
 *     and `put coin on desk` keep "You are not holding the coin.".  The
 *     all/everything forms are in that same list but say nothing with empty
 *     hands, and `drop all` / `put all on desk` DO draw the RepeatText, so
 *     they are matched here first and excluded.
 *   - the per-character pass at 48B56E (Proc_19_0_480674), which sits BELOW
 *     48B4E3 and so still runs, its answer replacing the message: NPC
 *     examine (47FE4F) and the ask-format hint (47F84A) that `talk to X`,
 *     `speak to X` and `ask X <anything but "about">` land on.
 *
 * Everything else loses the line to the RepeatText, movement and take
 * included: `north` and `east` print it and the player does not move,
 * `take coin` never takes the coin, `look`, `wait`, `score`, `turns`,
 * `open`, `read`, `wear`, `x <object>`, `x me`, `take all` and `drop all`
 * all print it, and a wildcard task's RepeatText wins as readily as a
 * literal one's (`* dance *`) -- so neither of the pre-4.0 pass's literal
 * and movement exemptions carries over.  `kiss X`, `give X to Y` and `ask X
 * about Y` are general verbs rather than parts of the character pass, and
 * lose the line too; `talk X` without the "to" is the 3.7/3.8 spelling that
 * 4.0's hint does not answer, so it loses it as well.
 */
typedef struct
{
  const scr_char *command;
  scr_bool survives;
} scr_repeat_survivor_t;

static const scr_repeat_survivor_t REPEAT_SURVIVOR_COMMANDS[] = {
  /* Pre-dispatcher, 48A457. */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[inventory/inv]", TRUE},
#else
  {"[inventory/inv/i]", TRUE},
#endif

  /* Pre-dispatcher, 48A462 -- but only where it has something to say. */
  {"put [all/everything] *", FALSE},
  {"[drop/put down] [all/everything] *", FALSE},
  {"put %text% [in/into/inside {of}] %object%", TRUE},
  {"put %text% [on/onto/on top of] %object%", TRUE},
  {"[drop/put down] %text% [in/into/inside {of}] %object%", TRUE},
  {"[drop/put down] %text% [on/onto/on top of] %object%", TRUE},
  {"[drop/put down] %text%", TRUE},
  {"put %text% down", TRUE},

  /* The character pass at 48B56E, and the two branches that are not it. */
  {"ask %character% about %text%", FALSE},
  {"[talk/speak] to %character% about %text%", FALSE},
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[ex/exam/examine/look {at}] %character%", TRUE},
#else
  {"[x/ex/exam/examine/look {at}] %character%", TRUE},
#endif
  {"[talk/speak] to %character% *", TRUE},
  {"ask %character% *", TRUE},

  {nullptr, FALSE}
};

scr_bool
run_repeat_survivor_400 (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_ref_entity_guard ref_entity (game);
  std::vector<scr_bool> objects (game->object_references);
  std::vector<scr_bool> npcs (game->npc_references);
  const scr_repeat_survivor_t *row;
  scr_bool survives = FALSE;

  for (row = REPEAT_SURVIVOR_COMMANDS; row->command; row++)
    {
      if (uip_match (row->command, string, game))
        {
          survives = row->survives;
          break;
        }
    }

  game->object_references = objects;
  game->npc_references = npcs;
  return survives;
}

/*
 * run_normalise_put_line()
 * run_unnamed_put_fragment()
 *
 * Rebuild the command line run400's put_drop_list leaves behind when the
 * direct object of a "put X in Y" names nothing -- see the 46E142 note on
 * run_priority_unnamed_put_object() above.  put_drop_list normalises the
 * line with four plain VB Replace() calls (459B3D-459BAC, substring not
 * word, and "inside" before "into" so that "inside" cannot be reached by
 * the shorter pattern), splits it at the first " in " (459BCD), and
 * name_object installs Left(line, split) -- everything up to and including
 * the space before "in" -- as the command line at 46DE99.
 *
 * The list branches leave before the clobber, so none of them is rebuilt
 * here: a whole-word "all" or "and" in the fragment is answered by the
 * loops at 46E04E and 46E0B2 (unported), and an " and " at or beyond the
 * split sends put_drop_list round its own loop at 459C75, whose clauses are
 * carved by lib_put_clauses_400().  "put a in b and put c in d" is not an
 * example of either: the word after its " and " is "put", not an object, so
 * the top-level splitter cuts it into two commands (run_find_split_400()).
 */
std::string
run_normalise_put_line (const scr_char *string)
{
  /*
   * Deliberate deviation (2026-09-30): the four rewrites are whole-word, and
   * "drop" only as the line's verb.  run400's substring Replace()s reach
   * into names, so `put coin in drop box` became "put coin in put box" and
   * was "I don't understand what you want to put things inside.", and a
   * "pointo" or "gloveinside" would lose letters the same way.
   */
  static const scr_char *const WORDS[][2] = {
    {"inside", "in"}, {"into", "in"}, {"onto", "on"}
  };
  std::string line (string);
  size_t index_;

  if (line.compare (0, 5, "drop ") == 0)
    line.replace (0, 4, "put");
  for (index_ = 0; index_ < sizeof WORDS / sizeof WORDS[0]; index_++)
    {
      const std::string from (WORDS[index_][0]);
      size_t at = 0;

      while ((at = line.find (from, at)) != std::string::npos)
        {
          const size_t end = at + from.size ();

          if ((at == 0 || line[at - 1] == ' ')
              && (end == line.size () || line[end] == ' '))
            {
              line.replace (at, from.size (), WORDS[index_][1]);
              at += strlen (WORDS[index_][1]);
            }
          else
            at = end;
        }
    }
  if (SCR_TRACING_DEVIATIONS ())
    {
      static const scr_char *const RUNNER[][2] = {
        {"drop", "put"}, {"inside", "in"}, {"into", "in"}, {"onto", "on"}
      };
      std::string runner (string);

      for (index_ = 0; index_ < sizeof RUNNER / sizeof RUNNER[0]; index_++)
        for (size_t at = 0;
             (at = runner.find (RUNNER[index_][0], at)) != std::string::npos;
             at += strlen (RUNNER[index_][1]))
          runner.replace (at, strlen (RUNNER[index_][0]), RUNNER[index_][1]);
      if (runner != line)
        SCR_DEVIATION ("put_whole_word", "runner=\"%s\" ours=\"%s\"",
                       runner.c_str (), line.c_str ());
    }
  return line;
}

scr_bool
run_unnamed_put_fragment (const scr_char *string, std::string &fragment)
{
  std::string line = run_normalise_put_line (string);
  std::string::size_type split;

  split = line.find (" in ");
  if (split == std::string::npos)
    return FALSE;

  fragment = line.substr (0, split + 1);
  if ((" " + fragment).find (" all ") != std::string::npos
      || (" " + fragment).find (" and ") != std::string::npos
      || line.find (" and ", split) != std::string::npos)
    return FALSE;

  return TRUE;
}

/*
 * run_move_commands()
 *
 * Return the movement command table matching the game's compass setting.
 */
static scr_commandsref_t
run_move_commands (const scr_prop_setref_t bundle)
{
  return prop_get_global_boolean (bundle, "EightPointCompass")
         ? MOVE_COMMANDS_8 : MOVE_COMMANDS_4;
}

/*
 * run_npc_library_blocked()
 *
 * Whether the character-handler rows are skipped for this line: from 3.9
 * on, once a task has run for it.
 */
static scr_bool
run_npc_library_blocked (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;
  return run_any_task_ran_this_command ();
}

/*
 * run_npc_row_blocked()
 *
 * Whether a library row is one of the character-handler branches that the
 * Runner skips once a task has run for this line.  See the comment at the
 * call site in run_try_command_table() for the run400 addresses.
 */
static scr_bool
run_npc_row_blocked (const scr_commands_t *command)
{
  if (command->handler == lib_cmd_attack_npcs
      || command->handler == lib_cmd_attack_npcs_with)
    return TRUE;
  if (strstr (command->command, "%character%") == nullptr)
    return FALSE;
  return command->handler != lib_cmd_give_object_npc
         && command->handler != lib_cmd_ask_npc_about
         && command->handler != lib_cmd_talk_to_npc_about
         && command->handler != lib_cmd_kiss_npc
         && command->handler != lib_cmd_status_npc
         && command->handler != lib_cmd_verb_npc;
}

/*
 * Set while the pre-4.0 verb pass runs, so that the give-to-character rows
 * wait for run_standard_give_npc_commands() below the room refusal.
 */
static scr_bool run_defer_give_npc = FALSE;

/*
 * Set while the 3.9 verb pass runs on a line run390's therest with-arm will
 * answer (lib_with_arm_390()): the characters() attack rows, which run390
 * calls below therest, wait for it -- and lose to it.  With the Battle
 * System on, those rows are dobattle's, which run390 calls above therest
 * (45F4AF): secret_of_lost_world's `hit ghost with excalibur` is a blow.
 */
static scr_bool run_defer_with_390 = FALSE;

static scr_bool
run_is_with_deferred_handler (scr_bool (*handler) (scr_gameref_t))
{
  static scr_bool (*const DEFERRED[]) (scr_gameref_t) = {
    lib_cmd_attack_npc, lib_cmd_attack_npc_with, lib_cmd_attack_npcs,
    lib_cmd_attack_npcs_with, lib_cmd_chop_npc, lib_cmd_chop_npc_with,
    lib_cmd_cut_npc, lib_cmd_cut_npc_with, lib_cmd_fight_npc,
    lib_cmd_fight_npc_with, lib_cmd_hit_npc_with, lib_cmd_kill_npc,
    lib_cmd_kill_npc_with, lib_cmd_shoot_npc, lib_cmd_shoot_npc_with,
    lib_cmd_slap_npc, lib_cmd_slap_npc_with, lib_cmd_stab_npc,
    lib_cmd_stab_npc_with, lib_cmd_throw_npc_with, nullptr
  };

  for (scr_int index_ = 0; DEFERRED[index_]; index_++)
    {
      if (DEFERRED[index_] == handler)
        return TRUE;
    }
  return FALSE;
}

/*
 * run_is_scare_only_handler()
 * run_line_opens_with_scare_word()
 * run_game_owns_scare_word()
 *
 * Deliberate deviation's limit (2026-10-11).  `verbose`, `brief`, `notify`,
 * `notification`, `hints`, `hist`, `redo`, `gpl`, `license`, `statusline` and
 * `stats` are SCARE's words; no Runner answers them (see the audit note in
 * STANDARD_COMMANDS).  A task is matched before the library, so one that
 * speaks keeps its line -- but one that does not claim it (restrictions
 * that fail without a message, nothing to say, already done, another room)
 * used to lose it to our command, where the Runner says it does not
 * understand: `stats` in suburbanprodigy3 was that, and the census
 * (SCR_CENSUS_META, tools/scrun_census.cpp) finds 275 tasks in 28 games
 * that take `hints`, 12 games whose `statusline` we answered at the first
 * prompt.
 *
 * So a word a game's tasks name is the game's: when any task has a command
 * that matches the line the Runner's way, in whatever room and state, the
 * SCARE rows stand aside and the line goes on as it would in the Runner.  A
 * task that would take the line with any other word in the place of ours
 * (`* on`, `*`) does not name it and does not count.  Nothing is put out of
 * reach: `glk verbose`, `glk notify`, `glk hints`, `glk license` and `glk
 * statusline` cannot be shadowed, and `history` and `!` are the Runner's
 * own.  The one-letter `g`, `i`, `q` and `z` are left alone -- conveniences
 * kept on purpose.
 */
static scr_bool
run_is_scare_only_handler (scr_bool (*handler) (scr_gameref_t))
{
  static scr_bool (*const SCARE_ONLY[]) (scr_gameref_t) = {
    lib_cmd_verbose, lib_cmd_brief, lib_cmd_notify_on_off, lib_cmd_notify,
    lib_cmd_hints, lib_cmd_history, lib_cmd_history_number,
    lib_cmd_redo_number, lib_cmd_redo_text, lib_cmd_redo_last,
    lib_cmd_license, lib_cmd_statusline, nullptr
  };

  for (scr_int index_ = 0; SCARE_ONLY[index_]; index_++)
    {
      if (SCARE_ONLY[index_] == handler)
        return TRUE;
    }
  return FALSE;
}

/* The length of the SCARE word the line opens with, or 0. */
static size_t
run_line_opens_with_scare_word (const scr_char *string)
{
  static const scr_char *const WORDS[] = {
    "verbose", "brief", "notify", "notification", "hints", "hist", "redo",
    "gpl", "license", "statusline", "stats", nullptr
  };
  const size_t length = strcspn (string, WHITESPACE);

  for (scr_int index_ = 0; WORDS[index_]; index_++)
    {
      if (strlen (WORDS[index_]) == length
          && scr_strncasecmp (string, WORDS[index_], length) == 0)
        return length;
    }
  return 0;
}

static scr_bool
run_game_owns_scare_word (scr_gameref_t game, const scr_char *string)
{
  string += strspn (string, WHITESPACE);
  const size_t length = run_line_opens_with_scare_word (string);

  if (length == 0)
    return FALSE;

  /* The same line with a word no task can have been written for. */
  const std::string other = std::string ("qzxqzx") + (string + length);
  const scr_lenient_tasks_guard strict_match (FALSE);

  return run_line_matches_task_strictly (game, string, other.c_str (), TRUE);
}

/*
 * run_try_command_table()
 *
 * Search a command table for a match to the string, returning TRUE on the
 * first matching command whose handler succeeds.
 */
scr_bool
run_try_command_table (scr_commandsref_t command,
                       scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_bool npc_blocked = run_npc_library_blocked (game);
  scr_int scare_word_owned = -1;

  for (; command->command; command++)
    {
      if (run_defer_give_npc && command->handler == lib_cmd_give_object_npc)
        continue;
      if (run_defer_with_390 && run_is_with_deferred_handler (command->handler))
        continue;

      /*
       * Once a game task has run for this line, most of the Runner's
       * character handler answers nothing: the who (47F32C), hit/kill/kick/
       * punch/attack (47F452), get/take/pick up (47F734), talk to/speak to
       * (47F863), ask-without-about hint (47FB93), where/find/locate
       * (47FCB1), examine/look (47FE4F) and take-from (4803DD) branches of
       * run400's Proc_19_0_480674 all test MemVar_4941F8 = 0, the flag
       * execute_task sets at 45A176 and the input routine clears at 489FF6.
       * run390 guards the same branches with MemVar_468198 (45939D,
       * 459658); run370 has no such flag (4386BC), and run380's rendering
       * of the test (44054B) is too ambiguous to lean on, so pre-3.9 keeps
       * every row.  Measured live on House (4.00), 2026-09-06: the silent
       * task "# attention on cathy grave vision" (`*cathy*`, once only) runs
       * on the first `get cathy` and the library then says "Take what?"
       * (runner_probes/house.run400.t93.txt,
       * runner_probes/house.run400.girl.txt); `x cathy` on that first
       * mention gets "You see no such thing."
       * (runner_probes/house.run400.t98.txt); with the task spent, both fall
       * to the NPC handlers -- "I don't think girl would appreciate being
       * handled." and her description.
       *
       * The rows that survive a task are the ones the Runner reaches by
       * another route: give is handled in the input routine at 48A98A with
       * no flag test; `ask X about Y` is re-opened at 47F922-47F935 by the
       * "<player> can't talk to that." buffer that generaltasks_verbs seeds
       * at 488C65 whenever no object took the ask (run390 tests no flag at
       * all at 4597FE) -- Humbug's silent scoring task `ask * hacker about
       * * humbug` still gets the hacker's reply
       * (runner_probes/humbug.run400.b.txt); kiss (47F7E7) is gated on the
       * buffer, not the flag; and the end-of-handler "I don't understand
       * what you want to do with" fallback at 4805DA asks for an empty
       * buffer rather than the flag.  (It does have one other guard, added
       * here 2026-09-07: `loc_4805CD: If MemVar_4941AD > 0 Then Exit Sub`
       * above it takes it off a line that a task has just ended the game on
       * -- see lib_cmd_verb_npc().)
       */
      if (npc_blocked && run_npc_row_blocked (command))
        continue;

      if (uip_match (command->command, string, game))
        {
          /* A SCARE word the game has a task for is the game's; see
             run_game_owns_scare_word().  Asked once, and only of a line
             that reaches one of those rows. */
          if (run_is_scare_only_handler (command->handler))
            {
              if (scare_word_owned < 0)
                scare_word_owned = run_game_owns_scare_word (game, string);
              if (scare_word_owned)
                continue;
            }
          if (command->handler (game))
            return TRUE;
        }
    }
  return FALSE;
}

/*
 * run_movement_succeeds()
 *
 * Return TRUE if the input is a movement command that would really move the
 * player out of the room.  Prints nothing and changes nothing -- the movement
 * handlers run under lib_set_movement_probe().  Used only by the version 3.8
 * ordering in run_all_commands().
 */
scr_bool
run_movement_succeeds (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool is_movement;

  lib_set_movement_probe (TRUE);
  is_movement = run_try_command_table (run_move_commands (bundle),
                                       game, string);
  lib_set_movement_probe (FALSE);

  return is_movement;
}


/*
 * run_standard_verb_commands_inner()
 *
 * The dedicated verb handlers: everything run390's generaltasks() calls by
 * name, above the out-of-room task refusal at loc_45FFE8.  Split from the
 * fallback bucket below because the refusal goes between the two; see the
 * ordering note on run_task_refusal().
 */
static scr_bool
run_standard_verb_commands_inner (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * Search movement commands first, returning TRUE if any matching command
   * handler succeeded.  Then repeat for standard library commands.
   */
  if (run_try_command_table (run_move_commands (bundle), game, string))
    return TRUE;

  /* The take-from catch-alls; see STANDARD_TAKE_FROM_COMMANDS. */
  if (run_try_command_table (STANDARD_TAKE_FROM_COMMANDS, game, string))
    return TRUE;

  if (run_try_command_table (STANDARD_COMMANDS, game, string))
    return TRUE;

  /*
   * The same commands again with whole-word containment for a trailing
   * %object% (uip_match_entity()): the Runner's co() finds an object named
   * anywhere in the line, so `x silver key` examines "a key" (man_overboard,
   * 4.00).  A separate pass, so that every pattern's positional match has
   * had its turn first, and before the catch-alls below so that they never
   * pre-empt an object the line does name.
   */
  {
    const scr_containment_guard containment (TRUE);

    if (run_try_command_table (STANDARD_COMMANDS, game, string))
      return TRUE;
  }

  /* The pre-4.0 put catch-alls; see STANDARD_PUT_COMMANDS. */
  if (run_try_command_table (STANDARD_PUT_COMMANDS, game, string))
    return TRUE;

  /* run390's take handler, still above the refusal; see
     STANDARD_ABOVE_REFUSAL_COMMANDS. */
  const scr_containment_guard containment (TRUE);

  return run_try_command_table (STANDARD_ABOVE_REFUSAL_COMMANDS, game, string);
}

/*
 * run_standard_verb_commands()
 *
 * The verb pass with its pre-4.0 deferrals set for the while.
 */
scr_bool
run_standard_verb_commands (scr_gameref_t game, const scr_char *string)
{
  /*
   * Pre-4.0 the give to a present character is characters()'s (run390
   * 45A0BA) or therest()'s (run380 440E8C), both below the room refusal,
   * and run390's give writes only into an empty message (or one holding
   * " might need " / "I don't understand", 45A11D-45A167).  the_hangover
   * (3.90) T42 `give the doctor some french fries`, with Where=0 task 10
   * matching it: "You can't do that here!" (runner_transcripts/
   * the_hangover.txt), not "Doctor doesn't seem interested in the french
   * fries.".  4.0 gives in the input routine (48A98A), above the refusal.
   */
  run_defer_give_npc =
      run_get_version (gs_get_bundle (game)) < TAF_VERSION_400;
  run_defer_with_390 = !battle_is_enabled (game)
                       && lib_with_arm_390_applies (game);
  const scr_bool status = run_standard_verb_commands_inner (game, string);
  run_defer_give_npc = FALSE;
  run_defer_with_390 = FALSE;
  return status;
}

/*
 * The give-to-character rows of STANDARD_COMMANDS, run pre-4.0 between the
 * room refusal and the fallback bucket; see run_standard_verb_commands().
 */
scr_bool
run_standard_give_npc_commands (scr_gameref_t game, const scr_char *string)
{
  static scr_commands_t GIVE_NPC_COMMANDS[] = {
    {"give %object% to %character%", lib_cmd_give_object_npc},
    {"give %character% %object%", lib_cmd_give_object_npc},
    {"give %object% %character%", lib_cmd_give_object_npc},
    {nullptr, nullptr}
  };

  if (run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    return FALSE;
  if (run_try_command_table (GIVE_NPC_COMMANDS, game, string))
    return TRUE;
  const scr_containment_guard containment (TRUE);

  return run_try_command_table (GIVE_NPC_COMMANDS, game, string);
}


/*
 * STANDARD_ENDED_FALLBACK_COMMANDS
 *
 * What is left of STANDARD_FALLBACK_COMMANDS at 4.0 once a task has ended the
 * game on the line.  run400 calls therest (Proc_19_85_489F4C) at 48AFE4, BELOW
 * generaltasks' gameover jump at 48AC62 (`If MemVar_4941AD <> 0 Then GoTo
 * loc_48B4E3`), so every therest arm -- give, lock, ask, the "You can't <verb>
 * <X>." refusals, the question words -- is lost with the unhandled-verb tail.
 * iachini (run400x runner_transcripts/iachini.txt T185): `turn on tv` runs
 * silent TASK 30, whose only action is End Game, and run400 answers the game's
 * DontUnderstand, not "You can't turn the 32-inch television on.".  Rows whose
 * Runner handler sits above 48AC62 (open/close 48A515, wear 48A48C, remove
 * 48A491) or has not been placed keep their answer; so does the character
 * tail (48B56E, with its own 4805CD gate in lib_cmd_verb_npc()).
 */
static scr_commands_t STANDARD_ENDED_FALLBACK_COMMANDS[] = {
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


/* run390's therest(): the generic catch-alls, below the refusal. */
scr_bool
run_standard_fallback_commands (scr_gameref_t game, const scr_char *string)
{
  const scr_bool is_ended = game->pending_endgame != 0
      && run_get_version (gs_get_bundle (game)) == TAF_VERSION_400;

  /*
   * The fallback verbs resolve their noun the Runner's way too: generaltasks
   * (Proc_19_85_489F4C) runs co() once, up front, and every generic verb
   * after it sees the object it found -- `pull de la palanca` (Vardock
   * Bates, 4.00, a `tirar`->`pull` synonym leaving the "de" in place) is
   * "You pull la palanca, but nothing happens." in run400 (transcript
   * 2026-08-29), not the object-less "You pull, but nothing happens.".
   * Containment stays a per-row fallback (positional first), so a row's
   * "%object% *" form still wins over the "%text%" catch-all beneath it.
   */
  const scr_containment_guard containment (TRUE);

  return run_try_command_table (is_ended ? STANDARD_ENDED_FALLBACK_COMMANDS
                                         : STANDARD_FALLBACK_COMMANDS,
                                game, string);
}


/*
 * run_c_word_pre400()
 *
 * The pre-4.0 Runners' c(word) test (run370 423C80, run380 429048, run390
 * 4334B0): the FIRST case-insensitive InStr hit that starts the line or
 * follows a space decides, and it is true only if the word ends there -- at
 * the end of the line, a space or a comma, and at 3.9 a period too.  A hit
 * inside a word is skipped, so `unblock` never holds `block`; a word-start
 * hit that runs on stops the search, so `lookout look` holds no `look`.
 * Returns the hit's offset, or -1.
 */
scr_int
run_c_word_pre400 (scr_int version, const scr_char *line, const scr_char *word)
{
  const scr_int length = strlen (word);
  const scr_char *hit;
  scr_char end;

  for (hit = line; *hit != NUL; hit++)
    {
      if (scr_strncasecmp (hit, word, length) != 0)
        continue;
      if (hit == line || hit[-1] == ' ')
        break;
    }
  if (*hit == NUL)
    return -1;

  end = hit[length];
  if (end == NUL || end == ' ' || end == ','
      || (end == '.' && version == TAF_VERSION_390))
    return hit - line;
  return -1;
}

/*
 * run_c_word()
 *
 * c(word) at any version: 4.0's whole-word test (lib_input_contains_word())
 * or the pre-4.0 first-hit rule above.
 */
scr_bool
run_c_word (scr_int version, const scr_char *line, const scr_char *word)
{
  return version >= TAF_VERSION_400
         ? lib_input_contains_word (line, word)
         : run_c_word_pre400 (version, line, word) >= 0;
}


/* In therest() order; '?' marks an arm that needs an empty message. */
static const scr_char *const THEREST_ARMS_380[] = {
  "open", "close", "eat", "drink", "give", "ask", "talk to", "?talk",
  "say", "look", "clean", "run", "stop", "read", "wash", "cut", "hit",
  "kill", "move", "lift", "light", "suck", "feel", "turn", "go", "enter",
  "smell", "push", "pull", "press", "shake", "clear", "clean", "kick",
  "punch", "fight", "jump", "feed", "unblock", "?block", "unlock",
  "?lock", "climb", "listen", "shout", "sing", "hum", "dance", "whistle",
  "cry", "wait", "buy", "sell", "break", "destroy", "smash", "kiss", "fly",
  "feed", "feel", "please", "fix", "repair", "mend", "date", "time",
  "sleep", "sit on", "sit in", "stand on", "stand in", "lie on", "lie in",
  nullptr
};

static const scr_char *const THEREST_ARMS_390[] = {
  "open", "close", "eat", "drink", "give", "ask", "talk to", "?talk",
  "say", "look", "clean", "run", "stop", "read", "wash", "cut", "kill",
  "move", "lift", "light", "suck", "feel", "touch", "turn", "go", "enter",
  "smell", "push", "pull", "press", "shake", "kick", "hit", "clear",
  "punch", "fight", "jump", "feed", "unblock", "block", "unlock", "lock",
  "climb", "listen", "shout", "sing", "hum", "dance", "whistle", "cry",
  "wait", "examine", "ex", "x", "buy", "sell", "break", "destroy", "smash",
  "kiss", "fly", "feed", "feel", "please", "fix", "repair", "mend", "date",
  "time", "sleep", "sit on", "sit in", "stand on", "stand in", "lie on",
  "lie in", "xyzzy", nullptr
};

/* The arm table for a version; run370 has no shake arm. */
static const scr_char *const *
run_therest_arms (scr_int version)
{
  return version == TAF_VERSION_390 ? THEREST_ARMS_390 : THEREST_ARMS_380;
}

static scr_bool
run_therest_arm_skipped (scr_int version, const scr_char *word)
{
  return version < TAF_VERSION_380 && strcmp (word, "shake") == 0;
}

/*
 * run_therest_arm_at()
 *
 * TRUE when one of therest()'s own verb arms begins at WORD.  Used to tell
 * a line that reaches therest from one an earlier handler owns; the arms
 * that need an empty message count too, being arms all the same.
 */
scr_bool
run_therest_arm_at (scr_int version, const scr_char *word)
{
  const scr_char *const *arm;

  for (arm = run_therest_arms (version); *arm; arm++)
    {
      const scr_char *const entry = *arm + ((*arm)[0] == '?' ? 1 : 0);
      const scr_int length = strlen (entry);

      if (run_therest_arm_skipped (version, entry))
        continue;
      if (scr_strncasecmp (word, entry, length) == 0
          && (word[length] == NUL || word[length] == ' '))
        return TRUE;
    }
  return FALSE;
}


/*
 * run_therest_winner_pre400()
 *
 * Pre-4.0 therest() is not a verb-first table.  It is one long run of
 * keyword arms, each `If c("<verb>") Then msg = ...`, over the whole line,
 * and each arm overwrites the message the one before it wrote, so the LAST
 * arm whose keyword the line holds answers (run380 443CBB-445521; run390
 * 45D465-45EB86, where most arms go through checkverb 42A504 with the same
 * c() test).  Measured on p38ASK/p39ASK with harness/make_38_askprobe.py and
 * make_39_askprobe.py (run380x runner_probes/ask.run380.kw.rtf, run390x
 * runner_probes/ask.run390.kw.txt): `zzz push stone`, `stone jump` and `zzz
 * cut stone` answer the verb wherever it stands; `push and pull stone` and
 * `push stone pull` pull, `push stone and kick` kicks, `buy stone kiss`
 * kisses, `drink push stone` pushes and `climb stone and sit on stone` sits.
 * talk, block and lock only write an empty message, so they lose to any
 * earlier arm.  The arms that live above therest() (open, read, ...) have
 * answered before any of this runs.
 *
 * Returns the winning arm's keyword and sets *offset to where c() found it,
 * or returns NULL.
 */
static const scr_char *
run_therest_winner_pre400 (scr_int version, const scr_char *line,
                           scr_int *offset)
{
  const scr_char *const *arm;
  const scr_char *winner = nullptr;

  for (arm = run_therest_arms (version); *arm; arm++)
    {
      const scr_bool needs_empty = (*arm)[0] == '?';
      const scr_char *const word = *arm + (needs_empty ? 1 : 0);
      scr_int found;

      if (run_therest_arm_skipped (version, word))
        continue;
      if (needs_empty && winner)
        continue;

      found = run_c_word_pre400 (version, line, word);
      if (found >= 0)
        {
          winner = word;
          *offset = found;
        }
    }

  return winner;
}


/*
 * run_therest_pre400()
 *
 * When the winning arm's keyword does not open the line, answer the line as
 * if it did: the keyword is moved to the front and the library runs again,
 * so its "<verb> %object% *" row answers with the object therest() found
 * (the fallback rows resolve by containment).  The look arm, and 3.9's
 * examine arms, answer the flat "Nothing special." whatever object the line
 * names.  Returns TRUE if the line was answered.
 */
scr_bool
run_therest_pre400 (scr_gameref_t game, const scr_char *string)
{
  static scr_bool is_active = FALSE;
  const scr_int version = run_get_version (gs_get_bundle (game));
  const scr_char *winner;
  scr_int offset = -1;
  std::string moved;
  scr_bool answered;

  if (is_active || version >= TAF_VERSION_400
      || game->pending_endgame != 0)
    return FALSE;

  /*
   * A winner opening the line has had its own row.  That includes a 3.7/3.8
   * `look X`, which the Runner leaves to therest's look arm ("Nothing
   * special." for anything at all, run370 434E2A, run380 44439D/43C69D;
   * p37EXAM/p38EXAM, run370x plookobj37, run380x plookobj38).  Deliberate
   * deviation: Scarier examines it, as from 3.9.
   */
  winner = run_therest_winner_pre400 (version, string, &offset);
  if (!winner || offset == 0)
    return FALSE;

  /*
   * generaltasks calls takes, drops, wears, removes and examines ahead of
   * therest, so a line naming any of them never reaches these arms at all;
   * see lib_earlier_handler_claims_pre400().  The 3.7 refusal above this
   * one has always known it (run_therest_absent_370's EARLIER list).
   */
  if (lib_earlier_handler_claims_pre400 (game, string))
    return FALSE;

  if (strcmp (winner, "look") == 0 || strcmp (winner, "examine") == 0
      || strcmp (winner, "ex") == 0 || strcmp (winner, "x") == 0)
    {
      pf_buffer_string (gs_get_filter (game), "Nothing special.\n");
      return TRUE;
    }

  moved = winner;
  moved += ' ';
  moved.append (string, offset);
  moved += ' ';
  moved.append (string + offset + strlen (winner));

  /* Tidy the spaces the cut left behind. */
  {
    std::string tidy;
    for (const scr_char c : moved)
      if (c != ' ' || (!tidy.empty () && tidy.back () != ' '))
        tidy += c;
    while (!tidy.empty () && tidy.back () == ' ')
      tidy.pop_back ();
    moved = tidy;
  }

  is_active = TRUE;
  answered = run_standard_verb_commands (game, moved.c_str ())
             || run_standard_give_npc_commands (game, moved.c_str ())
             || run_standard_fallback_commands (game, moved.c_str ());
  is_active = FALSE;
  return answered;
}


/*
 * run_therest_absent_370()
 *
 * 3.7's therest() refuses a line naming an absent object before any verb
 * arm -- see lib_therest_absent_370().  Only a line that reaches therest()
 * gets it: one holding a therest arm keyword that no earlier generaltasks
 * handler takes (run370 43B942-43C644).  Those handlers enter on c() words
 * anywhere in the line -- takes, drops, wears and removes (435E28, 430475,
 * 42C533, 4295FF), openclose (4264E7), examines (434E2A), give (43BED8),
 * wait (43C1B3), whereis (42F9FF), gotoplace, thank and the question words
 * (43C499) -- and keep their own wording, so a line holding any of them is
 * left to them.  ask, talk and say are left alone too: not measured.
 */
scr_bool
run_therest_absent_370 (scr_gameref_t game, const scr_char *string)
{
  static const scr_char *const EARLIER[] = {
    "get", "take", "pick", "from", "drop", "put", "leave", "wear", "remove",
    "strip", "open", "close", "x", "examine", "look at", "ex", "exam", "read",
    "give", "wait", "where", "find", "locate", "goto", "go to", "thank",
    "when", "who", "what", "how", "can", "why", "score", "ask", "talk", "say",
    nullptr
  };
  const scr_int version = run_get_version (gs_get_bundle (game));
  const scr_char *const *word;
  scr_int offset;

  if (version >= TAF_VERSION_380 || game->pending_endgame != 0)
    return FALSE;
  if (!run_therest_winner_pre400 (version, string, &offset))
    return FALSE;
  /* The " with " split is made above this test; see
     lib_with_clause_claims(). */
  if (lib_with_clause_claims (game))
    return FALSE;
  for (word = EARLIER; *word; word++)
    if (run_c_word_pre400 (version, string, *word) >= 0)
      return FALSE;
  if (lib_sitstand_claims_370 (game))
    return FALSE;

  return lib_therest_absent_370 (game);
}


/*
 * run_line_for_anywhere()
 *
 * The standard rows run on a line of a pass's own making: the sit words cut
 * out for lib_sitstand_anywhere()'s pre-run of wears, removes and the
 * 3.7/4.0 put, the open/close word brought to the head for
 * lib_openclose_anywhere(), the where-is half of the line for
 * lib_whereis_anywhere().
 */
scr_bool
run_line_for_anywhere (scr_gameref_t game, const scr_char *line)
{
  const run_dispatch_input_guard input (line);

  return run_standard_verb_commands (game, line);
}


/*
 * run_score_anywhere()
 *
 * Every Runner's score arm is `If c("score") Then` -- the whole word
 * anywhere in the line -- and it assigns the message, overwriting whatever
 * sitstand, inventory, the help hint or examines had written, and marks the
 * line administrative (run370 43BB98, run380 4423EC, run390 45F6B5 with
 * MemVar_468219 = 1, run400 48A6AE with MemVar_494281).  It sits above the
 * wait gate, so `score wait` is the score and no time passes; below takes,
 * drops, wears and removes, which claim or act first, and above the
 * swearing arm, which overwrites it.  Measured 2026-09-20 on p37SITN..p4SITN
 * (harness/make_3738_sitnpcprobe.py: `score sit`, `score wait`; the sit case
 * is lib_sitstand_anywhere()'s).  A line holding one of those earlier or
 * later handlers' words is left to its rows: not measured.
 */
scr_bool
run_score_anywhere (scr_gameref_t game, const scr_char *string)
{
  static const scr_char *const OTHERS[] = {
    "get", "take", "pick", "from", "drop", "put", "leave", "wear", "remove",
    "strip", "sit", "stand", "lie", "x", "examine", "look at", "ex", "exam",
    "read", "shit", "fuck", "bastard", "cunt", "crap", "hell", "shag",
    "bollocks", "bollox", "piss", "bugger", "bloody", nullptr
  };
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = prop_get_taf_version (bundle);
  const scr_char *const *word;
  const auto has_word = [&] (const scr_char *what) -> scr_bool
    {
      return run_c_word (version, string, what);
    };

  if (game->pending_endgame != 0 || !has_word ("score")
      || scr_strcasecmp (string, "score") == 0)
    return FALSE;
  for (word = OTHERS; *word; word++)
    if (has_word (*word))
      return FALSE;
  if (version >= TAF_VERSION_400 && has_word ("lay"))
    return FALSE;
  if ((version >= TAF_VERSION_380 && has_word ("look in"))
      || (version >= TAF_VERSION_390 && (has_word ("look") || has_word ("l"))))
    return FALSE;

  return lib_cmd_score (game);
}


/*
 * run_wait_anywhere()
 *
 * Every Runner answers a line holding the whole word `wait` ANYWHERE with
 * "Time passes..." and the wait turns, if no handler above has written a
 * message by then: `If c("wait") [Or line = "z"] And msg = ""` (run370
 * 43C1B3, run380 442A07, run390 45FCA2, run400 48ABB8 -- 4.0's c() too).
 * That gate sits after the tasks and the handlers that enter on words
 * anywhere in the line (takes, drops, wears, removes, sitstand, openclose,
 * examines, score, profanity), and before whereis, gotoplace and therest.
 * So `wait stone` with the stone elsewhere, `please wait`, `wait here`,
 * `push stone wait` and `turn wait` all pass time.  A line holding an
 * earlier handler's word is left to the rows that already answer it.
 * Give, say, inventory and a direction are NOT such words: inventory()
 * writes a message sitstand-fashion but nothing below wears() claims
 * without moving something, and give, say and the directions are all
 * `If msg = ""` arms below the gate -- so `give coin to bob wait`, `say
 * hello wait`, `i wait`, `inventory wait` and `n wait` are "Time passes..."
 * with nothing given, listed or walked (p37SITN..p4SITN,
 * harness/make_3738_sitnpcprobe.py: run370x runner_probes/sitn.run370.rtf,
 * run380x runner_probes/sitn.run380.rtf, run390x
 * runner_probes/sitn.run390.txt, run400x runner_probes/sitn.run400.txt,
 * 2026-09-20).  ask/talk stay with their rows: characters() overwrites the
 * wait text with the reply (`ask bob about hat wait` is "BOB HAT.").  `score
 * wait` is the score's (run_score_anywhere() runs first).  up/down/in/out with
 * wait: not measured.  run370's openclose writes nothing unless the line names
 * an openable object (4264E7), so there `open stone wait` passes time; 3.8 on
 * refuse the open.  `look wait` is examines' from 3.9 (bare `look`), time
 * passing at 3.7/3.8.
 *
 * gotoplace still runs after it and adds its "Unknown place." -- `goto hall
 * wait` answers both.  A goto that walks prints "Moving to..." straight to
 * the screen and jumps past the message print and the turn's tick (run370
 * 42BDEE "&&&"), so the "Time passes..." is lost and only the wait loop's
 * WaitTurns - 1 ticks run: the walk is administrative and the wait counter
 * stays set.  run370x reaches that with `wait goto hall`, the game's goto
 * word and "goto" each cut from the front.
 *
 * Measured 2026-09-19 on p37GOTO, p38GOTO, p39GOTO and p4EXAM
 * (harness/make_37_gotoprobe.py, make_38_gotoprobe.py,
 * make_39_gotoprobe.py, make_400_examprobe.py): run370x
 * runner_probes/goto.run370.wait.rtf, run380x
 * runner_probes/goto.run380.wait.rtf, run390x
 * runner_probes/goto.run390.wait.txt, run400x
 * runner_probes/exam.run400.wait.txt.
 */
scr_bool
run_wait_anywhere (scr_gameref_t game, const scr_char *string)
{
  static const scr_char *const EARLIER[] = {
    "get", "take", "pick", "from", "drop", "put", "leave", "wear", "remove",
    "strip", "sit", "stand", "lie", "x", "examine", "look at", "ex", "exam",
    "read", "score", "ask", "talk", "up", "down", "in", "out",
    "shit", "fuck", "bastard", "cunt", "crap", "hell",
    "shag", "bollocks", "bollox", "piss", nullptr
  };
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = prop_get_taf_version (bundle);
  const scr_char *const *word;
  const auto has_word = [&] (const scr_char *what) -> scr_bool
    {
      return run_c_word (version, string, what);
    };

  if (game->pending_endgame != 0 || !has_word ("wait"))
    return FALSE;

  /* The plain forms keep their own rows. */
  if (scr_strncasecmp (string, "wait", 4) == 0
      && strspn (string + 4, " 0123456789") == strlen (string + 4))
    return FALSE;

  for (word = EARLIER; *word; word++)
    if (has_word (*word))
      return FALSE;
  if ((version >= TAF_VERSION_390 && has_word ("bugger"))
      || (version < TAF_VERSION_400 && has_word ("bloody"))
      || (version >= TAF_VERSION_380 && has_word ("look in"))
      || (version >= TAF_VERSION_390 && (has_word ("look") || has_word ("l"))))
    return FALSE;

  if (has_word ("open") || has_word ("close"))
    {
      scr_int object;

      if (version >= TAF_VERSION_380)
        return FALSE;
      GS_FOR_EACH_OBJECT (game, object)
        {
          const scr_char *name =
              prop_get_indexed_string (bundle, "Objects", object, "Short");
          const scr_int openness = gs_object_openness (game, object);

          if ((openness == OBJ_OPEN || openness == OBJ_CLOSED)
              && name && name[0] != NUL && has_word (name))
            return FALSE;
        }
    }

  const size_t mark = pf_buffer_length (filter);
  const scr_bool was_admin = game->is_admin;

  lib_cmd_wait (game);
  const size_t after = pf_buffer_length (filter);
  game->is_admin = FALSE;
  if (lib_cmd_go_place (game) && game->is_admin)
    {
      const std::string moving = pf_cut_tail (filter, after);

      pf_truncate (filter, mark);
      pf_buffer_string (filter, moving.c_str ());
      return TRUE;
    }

  game->is_admin = was_admin;
  return TRUE;
}


