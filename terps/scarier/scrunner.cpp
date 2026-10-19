/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
enum { LINE_BUFFER_SIZE = 256 };
static const scr_char NUL = '\0';
static const scr_char SPECIAL_PATTERN = '#';
static const scr_char WILDCARD_PATTERN = '*';
static const scr_char *const WHITESPACE = "\t\n\v\f\r ";

/*
 * run_loop_halt
 *
 * Control-flow exception used to unwind out of run_main_loop() back to
 * run_interpret() when a *running* game is quit / restarted / restored / has a
 * turn undone.  This replaces a longjmp(game->quitter) that skipped the
 * destructors of any non-trivial C++ local live in the command/task/print/expr
 * call tree -- undefined behaviour once that tree holds std::string/std::vector,
 * which is what blocked RAII across the runner.  Throwing unwinds the same
 * frames but runs their destructors.  All throw sites and the sole catch are in
 * this file; the meaning (quit vs restart vs restore) is still carried by the
 * game's do_restart/do_restore flags exactly as before, so the type is empty.
 *
 * It is caught specifically (never `catch (...)`) so that a P2 scr_fatal_error
 * -- or any other genuine exception -- still propagates to the scinterf boundary
 * instead of being mistaken for a normal halt.
 */
namespace { struct run_loop_halt {}; }

/*
 * run_is_separator()
 *
 * Return TRUE if the character at the given position in the line buffer acts
 * as a player-input command separator.  A comma always separates.  A period
 * separates only when followed by whitespace or the end of the line.
 *
 * This matches the ADRIFT Runner, whose input splitter (verified by reverse-
 * engineering run390/run400) normalises on ", ", ". " and "then" -- i.e. it
 * splits on a period only when that period is followed by a space.  A period
 * embedded in a word (e.g. "login to think.com", or a decimal like "3.5") is
 * therefore part of the command, not a separator.  Splitting on a bare period
 * made such commands untypeable and rendered games that rely on them
 * unwinnable (e.g. "The Annihilation of think.com").  Splitting on a trailing
 * period is harmless (and preserves long-standing behaviour for input like
 * "n.").
 */
static scr_bool
run_is_separator (const scr_char *line, scr_int posn)
{
  if (line[posn] == ',')
    return TRUE;
  if (line[posn] == '.')
    return line[posn + 1] == NUL || scr_isspace (line[posn + 1]);
  return FALSE;
}


/*
 * run_get_version()
 *
 * Return the game's TAF version from the bundle's top-level "Version"
 * property -- a TAF_VERSION_* value, set once at parse time.
 */
static scr_int
run_get_version (const scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key;

  vt_key.string = "Version";
  return prop_get_integer (bundle, "I<-s", &vt_key);
}


/*
 * run_squeeze_spaces()
 *
 * Copy 'string' into 'buffer' with every space dropped.  The 4.0 Runner
 * squeezes the whole command this way before it looks for a task command
 * function, which is what makes the spacing in the function free-form.
 */
static void
run_squeeze_spaces (const scr_char *string, scr_char *buffer)
{
  const scr_char *cursor;
  scr_char *out;

  for (cursor = string, out = buffer; *cursor != NUL; cursor++)
    {
      if (*cursor != ' ')
        *out++ = *cursor;
    }
  *out = NUL;
}


/*
 * run_is_task_function()
 *
 * Check for the presence of a command function in a task command, and action
 * it if found.  This is a 4.0 feature -- at present, only getdynfromroom()
 * exists.  Returns TRUE if function found and handled.
 *
 * The syntax and the selection are measured against the live 4.0 Runner (see
 * RUNNER_TESTS_TODO.md section 9, probes GDA..GDR of
 * test/adrift4/harness/make_arena_probe.py).  run400 squeezes every space out
 * of the command, requires what is left to open "#%object%=getdynfromroom("
 * and the *raw* command to end in ")" -- so "getdynfromroom(larder)x" is not
 * a function at all -- then compares the squeezed argument to room names
 * case-insensitively and takes the first non-static object standing directly
 * in the room it finds.  Object order decides between candidates (a room
 * holding "ring" then "gem" yields the ring), a room holding only statics
 * yields nothing, and the reference set here survives the rest of the turn.
 *
 * Two run400 bugs are deliberately not reproduced, both of which can only
 * lose a match the author meant to make:
 *
 *   deliberate: run400 scans rooms with "For r = 0 To roomCount - 1" over a
 *     1-based array, so the game's *last* room can never be found.  Proved
 *     live: "larder" as room 10 of 10 matched nothing, and started returning
 *     its pie the moment a spare 11th room was appended.  (Its object loop
 *     has no such fencepost -- an object added last is still found.)
 *   deliberate: run400 squeezes the spaces out of the argument but compares
 *     it against the unsqueezed room name, so no room whose name contains a
 *     space is reachable -- not even by the manual's own worked example,
 *     "getdynfromroom(The Park)".  We squeeze both sides, which keeps every
 *     match run400 can make and adds the ones it drops.
 *
 * The 3.9 Runner has no getdynfromroom at all (not one occurrence in its
 * P-code), hence the version gate.
 */
static scr_bool
run_is_task_function (const scr_char *pattern, scr_gameref_t game)
{
  static const scr_char *const FUNCTION = "#%object%=getdynfromroom(";

  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int length, argument_length, room, object;
  scr_char *argument;

  if (run_get_version (bundle) < TAF_VERSION_400)
    return FALSE;

  /* The Runner tests the raw command's final character for the ")". */
  length = strlen (pattern);
  if (length == 0 || pattern[length - 1] != ')')
    return FALSE;

  std::vector<scr_char> squeezed (length + 1);
  run_squeeze_spaces (pattern, squeezed.data ());
  if (scr_strncasecmp (squeezed.data (), FUNCTION, strlen (FUNCTION)) != 0)
    return FALSE;

  /* Take the argument, dropping the ")" that the raw test guarantees. */
  argument = squeezed.data () + strlen (FUNCTION);
  argument_length = strlen (argument);
  assert (argument_length > 0 && argument[argument_length - 1] == ')');
  argument[argument_length - 1] = NUL;

  /* Compare the argument read in against known room names. */
  for (room = 0; room < gs_room_count (game); room++)
    {
      const scr_char *name;

      name = prop_get_indexed_string (bundle, "Rooms", room, "Short");
      std::vector<scr_char> compressed (strlen (name) + 1);
      run_squeeze_spaces (name, compressed.data ());
      if (scr_strcasecmp (compressed.data (), argument) == 0)
        break;
    }
  if (room == gs_room_count (game))
    return FALSE;

  /* Select the first dynamic object standing on the room's floor. */
  for (object = 0; object < gs_object_count (game); object++)
    {
      scr_bool bstatic;

      bstatic = prop_get_indexed_boolean (bundle, "Objects", object, "Static");
      if (!bstatic && obj_directly_in_room (game, object, room))
        break;
    }
  if (object == gs_object_count (game))
    return FALSE;

  /* Set this object reference, unambiguously, as if %object% match. */
  gs_clear_object_references (game);
  game->object_references[object] = TRUE;
  var_set_ref_object (vars, object);

  return TRUE;
}


/* Structure used to associate a pattern with a handler function. */
typedef struct scr_commands_s
{
  const scr_char *const command;
  scr_bool (*const handler) (scr_gameref_t game);
} scr_commands_t;
typedef scr_commands_t *scr_commandsref_t;

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
  {NULL, NULL}
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
  {NULL, NULL}
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
  {"[[drop/put down] [all/everything]/put [all/everything] down]",
   lib_cmd_drop_all},
  {"[drop/put down] [all/everything] [[except/but] {for}/apart from] %text%",
   lib_cmd_drop_except_multiple},
  {"[drop/put down] %text%", lib_cmd_drop_multiple},
  {"put %text% down", lib_cmd_drop_multiple},

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
  {NULL, NULL}
};

/* Standard library commands, other than movement and priority above. */
static scr_commands_t STANDARD_COMMANDS[] = {

  /* Inventory, and general investigation of surroundings. */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[inventory/inv]", lib_cmd_inventory},
  {"[ex/exam/examine/look {at}] {{the} [room/location]}",
   lib_cmd_look},
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
   lib_cmd_look},
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
   * both offer the diary.  House (4.00) Adrift_102 2026-09-06: `give cathy
   * diary` gets the same reply as `give diary to cathy`.  Scarier used to
   * fall to "Give the diary to who?".
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
   * SCARE has only the file, so it gates on the .taf version as a proxy for
   * the Runner the game was written for -- the same proxy every other
   * version-gated row here uses.  The pre-3.9 "Take what?" then falls out
   * on its own: lib_cmd_get_off()/lib_cmd_get_down() decline below 3.9 and
   * the command drops through to the bare-verb row
   * `[get/take/pick up/pick] *` -> lib_cmd_get_what further down.
   *
   * `get on %object%` arrived at the same time and is gated with them; it
   * is a `stand on` synonym that refuses differently.  See
   * lib_cmd_get_on_object().
   */
  {"get on %object%", lib_cmd_get_on_object},
  {"get {down/up} off %object%", lib_cmd_get_off_object},
  {"get off", lib_cmd_get_off},
  {"get down", lib_cmd_get_down},
  {"get up", lib_cmd_stand_on_floor},
  {"sit {down/up} {[on/in] {the} [ground/floor]}", lib_cmd_sit_on_floor},
  {"stand {up/down} {[on/in] {the} [ground/floor]}", lib_cmd_stand_on_floor},
  {"[lie/lay] {down/up} {[on/in] {the} [ground/floor]}", lib_cmd_lie_on_floor},
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
   * The room-request rows.  `goto X` and `go to X` are gotoplace() at every
   * version; a bare `go X` only became one in 3.9, so under 3.7 and 3.8 it
   * falls to the nudge instead -- see lib_cmd_just_a_direction_pre_390(),
   * which returns FALSE from 3.9 on and lets the two `go` rows after it have
   * the command.
   */
  {"goto %text%", lib_cmd_go_room},
  {"goto *", lib_cmd_print_room_exits},
  {"go to %text%", lib_cmd_go_room},
  {"go to *", lib_cmd_print_room_exits},
  {"go *", lib_cmd_just_a_direction_pre_390},
  {"go %text%", lib_cmd_go_room},
  {"go *", lib_cmd_print_room_exits},
  {"[exits/directions/where]", lib_cmd_print_room_exits},
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[wait] %number%", lib_cmd_wait_number},
  {"[wait]", lib_cmd_wait},
#else
  /* `z` only entered the Runner vocabulary at 3.90 (index/verbs.py; cave.taf
   * run380 live 2026-08-31 answers it "Say again?"), so its rows decline
   * below that version and the word falls through to the unknown reply. */
  {"[wait] %number%", lib_cmd_wait_number},
  {"[wait]", lib_cmd_wait},
  {"z %number%", lib_cmd_wait_number_390},
  {"z", lib_cmd_wait_390},
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
   * v4 corpus (notes/WINE-TRANSCRIPTS-TODO.md, "Audited 2026-09-07"): besides
   * the `stats` row below, `hints`, `q`, `brief`, `verbose`, `notify`,
   * `notification`, `redo`, `hist`, `gpl`, `license` and `statusline` are all
   * SCARE inventions no Runner accepts (the Runner words are `hint`, `quit`,
   * `history`).  None of them diverges today: a game task is matched before
   * run_standard_commands(), so any task carrying COMPLETE text wins and the
   * invention never runs -- only a *silent* task leaves the gap that `stats`
   * fell into.  `hints` is the widest exposure (224 tasks in 11 games).
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
  {"statusline", lib_cmd_statusline},
#endif
  {"[control panel/control-panel/control/panel]", lib_cmd_control_panel},
  /*
   * `stats` was a SCARE invention: the string does not occur in ANY of the
   * four Runner binaries, so it stole `suburbanprodigy3` T31 from the game's
   * own task.  Dropped 2026-09-07; see test/adrift4/notes/
   * WINE-TRANSCRIPTS-TODO.md "Ported 2026-09-07: `stats` is not a Runner
   * command".  `status` itself is 3.90/4.00 only and lives solely inside the
   * Battle System handler (run400 47DCA1, run390 44C510), which is why both
   * handlers below are gated on battle_is_enabled().
   */
  {"status %character%", lib_cmd_status_npc},
  {"[status]", lib_cmd_status_player},
  {"wield %object%", lib_cmd_wield},
  {"version", lib_cmd_version},

  {"[locate/where {is/are}/find] %object%", lib_cmd_locate_object},
  {"[locate/where {is}/find] %character%", lib_cmd_locate_npc},

  {"[count/num]", lib_cmd_count},

  {NULL, NULL}
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
static scr_commands_t STANDARD_FALLBACK_COMMANDS[] = {
  /*
   * The 4.0 named take for a noun that names only objects the player has
   * seen elsewhere; it declines to anything else, leaving "Take what?" to
   * the catch-all below.  See lib_cmd_take_absent().
   */
  {"[get/take/pick up/pick] %object%", lib_cmd_take_absent},
  {"[get/take/pick up/pick] *", lib_cmd_get_what},
  /*
   * The two 4.0-only absent-object rows sit directly above the catch-alls
   * they pre-empt, because the Runner's clause fires only when nothing
   * else in the turn has spoken; see lib_absent_seen_object().
   */
  {"open %object%", lib_cmd_open_absent},
  {"open *", lib_cmd_open_other},
  {"close %object%", lib_cmd_close_absent},
  {"close *", lib_cmd_close_other},
  {"give %object% *", lib_cmd_give_object},
  {"give *", lib_cmd_give_what},
  {"lock %text%", lib_cmd_lock_other},
  {"lock", lib_cmd_lock_what},
  {"unlock %text%", lib_cmd_unlock_other},
  {"unlock", lib_cmd_unlock_what},
  {"sit {down/up} [on/in] *", lib_cmd_sit_other},
  {"stand {up/down} [on/in] *", lib_cmd_stand_other},
  {"[lie/lay] {down/up} [on/in] *", lib_cmd_lie_other},
  {"[remove/take off/doff] *", lib_cmd_remove_what},
  {"[drop/put down] *", lib_cmd_drop_what},
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
  {"[x/examine/look {at}] %object%", lib_cmd_examine_absent},
  {"[x/examine/look {at}] *", lib_cmd_examine_other},
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
  {"[break/destroy/smash] %text%", lib_cmd_break_other},
  {"break", lib_cmd_break_what},
  {"destroy", lib_cmd_destroy_what},
  {"smash", lib_cmd_smash_what},
  {"buy %object% *", lib_cmd_buy_object},
  {"buy %object% *", lib_cmd_buy_absent},
  {"buy %text%", lib_cmd_buy_other},
  {"buy", lib_cmd_buy_what},
  {"clean %object% *", lib_cmd_clean_object},
  {"clean %text%", lib_cmd_clean_other},
  {"clean", lib_cmd_clean_what},
  {"climb %object% *", lib_cmd_climb_object},
  {"climb %text%", lib_cmd_climb_other},
  {"climb", lib_cmd_climb_what},
  {"cry *", lib_cmd_cry},
  {"cut %object% *", lib_cmd_cut_object},
  {"cut %text%", lib_cmd_cut_other},
  {"cut", lib_cmd_cut_what},
  {"dance *", lib_cmd_dance},
  {"drink %object% *", lib_cmd_drink_object},
  {"drink %text%", lib_cmd_drink_other},
  {"drink", lib_cmd_drink_what},
  {"eat *", lib_cmd_eat_other},
  {"feed *", lib_cmd_feed},
  {"feel *", lib_cmd_feel},
  {"fight *", lib_cmd_fight},
  {"fix %object% *", lib_cmd_fix_object},
  {"fix %text%", lib_cmd_fix_other},
  {"fix", lib_cmd_fix_what},
  {"fly *", lib_cmd_fly},
  {"hint *", lib_cmd_hint},
  {"hit %character%", lib_cmd_hit_npc},
  {"hit %object% *", lib_cmd_hit_object},
  {"hit %text%", lib_cmd_hit_other},
  {"hit", lib_cmd_hit_what},
  {"slap %object% *", lib_cmd_slap_object},
  {"slap %text%", lib_cmd_slap_other},
  {"slap", lib_cmd_slap_what},
  {"hum *", lib_cmd_hum},
  {"jump *", lib_cmd_jump},
  {"kick %character%", lib_cmd_attack_npc},
  {"kick %object% *", lib_cmd_kick_object},
  {"kick %text%", lib_cmd_kick_other},
  {"kick", lib_cmd_kick_what},
  {"kiss %character% *", lib_cmd_kiss_npc},
  {"kiss %object% *", lib_cmd_kiss_object},
  {"kiss *", lib_cmd_kiss_other},
  {"kill *", lib_cmd_kill_other},
  {"lift %object% *", lib_cmd_lift_object},
  {"lift %text%", lib_cmd_lift_other},
  {"lift", lib_cmd_lift_what},
  {"light %object% *", lib_cmd_light_object},
  {"light %text%", lib_cmd_light_other},
  {"light", lib_cmd_light_what},
  {"listen *", lib_cmd_listen},
  {"mend %object% *", lib_cmd_mend_object},
  {"mend %text%", lib_cmd_mend_other},
  {"mend", lib_cmd_mend_what},
  {"move %object% *", lib_cmd_move_object},
  {"move %text%", lib_cmd_move_other},
  {"move", lib_cmd_move_what},
  {"please *", lib_cmd_please},
  {"press %object% *", lib_cmd_press_object},
  {"press %text%", lib_cmd_press_other},
  {"press", lib_cmd_press_what},
  {"pull %object% *", lib_cmd_pull_object},
  {"pull %text%", lib_cmd_pull_other},
  {"pull", lib_cmd_pull_what},
  {"punch *", lib_cmd_punch},
  {"push %object% *", lib_cmd_push_object},
  {"push %text%", lib_cmd_push_other},
  {"push", lib_cmd_push_what},
  {"repair %object% *", lib_cmd_repair_object},
  {"repair %text%", lib_cmd_repair_other},
  {"repair", lib_cmd_repair_what},
  {"rub %object% *", lib_cmd_rub_object},
  {"rub %text%", lib_cmd_rub_other},
  {"rub", lib_cmd_rub_what},
  {"run *", lib_cmd_run},
  {"say *", lib_cmd_say},
  {"sell %object% *", lib_cmd_sell_object},
  {"sell %text%", lib_cmd_sell_other},
  {"sell", lib_cmd_sell_what},
  {"shake %object% *", lib_cmd_shake_object},
  {"shake %text%", lib_cmd_shake_other},
  {"shake", lib_cmd_shake_what},
  {"shout *", lib_cmd_shout},
  {"sing *", lib_cmd_sing},
  {"sleep *", lib_cmd_sleep},
  {"smell %object% *", lib_cmd_smell_object},
  {"smell *", lib_cmd_smell_other},
  {"stop %object% *", lib_cmd_stop_object},
  {"stop %text%", lib_cmd_stop_other},
  {"stop", lib_cmd_stop_what},
  {"suck %object% *", lib_cmd_suck_object},
  {"suck %text%", lib_cmd_suck_other},
  {"suck", lib_cmd_suck_what},
  {"talk to * about *", lib_cmd_ask_about_nothing},
  {"talk *", lib_cmd_talk},
  {"thank *", lib_cmd_thank},
  {"turn %object% *", lib_cmd_turn_object},
  {"turn %text%", lib_cmd_turn_other},
  {"turn", lib_cmd_turn_what},
  {"touch %object% *", lib_cmd_touch_object},
  {"touch %text%", lib_cmd_touch_other},
  {"touch", lib_cmd_touch_what},
  {"unblock %object% *", lib_cmd_unblock_object},
  {"unblock %text%", lib_cmd_unblock_other},
  {"unblock", lib_cmd_unblock_what},
  {"wash %object% *", lib_cmd_wash_object},
  {"wash %text%", lib_cmd_wash_other},
  {"wash", lib_cmd_wash_what},
  {"whistle *", lib_cmd_whistle},
  {"[why/when/what/can/how] *", lib_cmd_interrogation},
  {"xyzzy *", lib_cmd_xyzzy},
  {"campbell", lib_cmd_egotistic},
  {"[yes/no] *", lib_cmd_yes_or_no},
  {"* %object% *", lib_cmd_verb_object},
  {"put *", lib_cmd_put_where_400},
  {"* %character% *", lib_cmd_verb_npc},

  /* SCARIER debugger hook command, placed last just in case... */
  {"{#}debug{ger}", debug_cmd_debugger},

  {NULL, NULL}
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
static std::string run_trace_last_input;
#endif

static scr_bool run_priority_pass_active = FALSE;
static scr_bool run_priority_deferred = FALSE;
static scr_bool run_priority_refused = FALSE;
static scr_bool run_priority_unnamed_put = FALSE;

scr_bool
run_in_priority_pass (void)
{
  return run_priority_pass_active;
}

void
run_priority_defer (void)
{
  assert (run_priority_pass_active);
  run_priority_deferred = TRUE;
}

/*
 * run_priority_refuse()
 *
 * A priority command has printed a refusal that does NOT claim the command:
 * the 4.0 put whose every object was turned away on size or capacity.  The
 * handler returns FALSE after calling this, the table walk stops, and
 * run_all_commands() lets the task passes answer the same line, joined onto
 * the refusal.  See the 4.0 put notes in run_all_commands().
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
 * Measured on IceCream.taf in run400 (Adrift_900_icecream2.txt, 2026-09-07),
 * whose three `put/place/set [the] ice cream in/on [the] cone` tasks share
 * one pattern: `put ice cream in cone` and `put the ice cream in the cone`
 * both come out as the catch-all, while `place ice cream in cone` runs the
 * task (place never enters put_drop_list) and `put ice cream on cone` runs
 * it too -- the "on" branch has an escape of its own at 459C39 that zeroes
 * the split when the fragment resolves to nothing, so the line reaches the
 * tasks intact.  The clobber is the "in" form only.
 *
 * The put-in handler signals the fragment with this, and run_all_commands()
 * runs the task passes against the fragment in its place.
 */
void
run_priority_unnamed_put_object (void)
{
  assert (run_priority_pass_active);
  run_priority_unnamed_put = TRUE;
}

/*
 * scr_ref_number_guard
 *
 * Save and put back the game's referenced-number state across a library
 * command table walk.
 *
 * Three library rows carry a %number% wildcard -- "wait 5", "redo 3",
 * "hist 3" (the wait row is written twice, abbreviation-gated) -- and
 * uip_match_number() sets the referenced number as a side effect of matching
 * one, exactly as it does for a game task's own %number%.  But those commands are Scarier's, not the Runner's: run400 has no
 * "wait N" at all, and its referenced number (MemVar_49420C) has just two
 * writers, numintext/numintext2, reachable only from the wildcard expansion
 * helper mdlSpreadTheLoad.Proc_19_36_45F268 and only when the pattern being
 * expanded really contains %number%.  So in the Runner a game with no number
 * wildcard anywhere can never see a referenced number other than the initial
 * zero, and a "$number = N" restriction (Type 4, Var1 0 -- run400
 * loc_4817BF..loc_4817F8) can never pass.
 *
 * Leaving the leak in makes those meta commands part of the game: Sandy.taf's
 * win task tests the referenced number against 2 where the author meant the
 * variable "mom", so `wait 2` followed by `look in toilet` won a game that is
 * unwinnable in the Runner.  Restoring around the whole walk -- not inside the
 * handlers -- also covers a row that matches and then declines, such as
 * "redo 99" falling through to the %text% row.
 *
 * Only the number is guarded.  The referenced object, character and text are
 * bound by library rows the Runner does have, and the text they later expand
 * into is meant to see them.
 */
class scr_ref_number_guard
{
public:
  explicit scr_ref_number_guard (scr_gameref_t game)
    : vars_ (gs_get_vars (game)),
      number_ (var_get_ref_number (vars_)),
      is_referenced_ (var_is_number_referenced (vars_))
  {
  }

  ~scr_ref_number_guard ()
  {
    var_restore_ref_number (vars_, number_, is_referenced_);
  }

  scr_ref_number_guard (const scr_ref_number_guard &) = delete;
  scr_ref_number_guard &operator= (const scr_ref_number_guard &) = delete;

private:
  const scr_var_setref_t vars_;
  const scr_int number_;
  const scr_bool is_referenced_;
};


static scr_bool
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
 * p4REPEAT3.taf (run400 Adrift_951/952, 2026-09-08), where every one of the
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
static scr_bool
run_is_put_command (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
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

      is_put = command->handler == lib_cmd_put_in_multiple
               || command->handler == lib_cmd_put_on_multiple
               || command->handler == lib_cmd_drop_multiple;
      break;
    }

  game->object_references = references;
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
 * (Adrift_951/952, 2026-09-08): the first `i` answers "You are carrying a
 * coin and a hat.  C1 i.", and `inv` / `inventory` the same way.  (The
 * listing is also why a spent task's RepeatText never survives here -- the
 * 44CC7D store is gated on the buffer still being empty; that is the
 * survivor table's first row.)
 */
static scr_bool
run_is_inventory_command (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
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
 * and carrying a RepeatText, each typed twice) under run400, feeds
 * cmdfile_rep1-3, transcripts Adrift_950-952.
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

  {NULL, FALSE}
};

static scr_bool
run_repeat_survivor_400 (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
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
 * run_replace_all()
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
 * loops at 46E04E and 46E0B2, and an " and " at or beyond the split --
 * "put a in b and put c in d" -- sends put_drop_list round its own loop at
 * 459C75 (the InStr there starts at the split, 459C60, so it only ever sees
 * the second clause).
 */
static std::string
run_replace_all (const std::string &string,
                 const scr_char *from, const scr_char *to)
{
  const std::string pattern (from), replacement (to);
  std::string result = string;
  std::string::size_type at = 0;

  while ((at = result.find (pattern, at)) != std::string::npos)
    {
      result.replace (at, pattern.length (), replacement);
      at += replacement.length ();
    }
  return result;
}

scr_bool
run_unnamed_put_fragment (const scr_char *string, std::string &fragment)
{
  std::string line (string);
  std::string::size_type split;

  line = run_replace_all (line, "drop ", "put ");
  line = run_replace_all (line, "inside", "in");
  line = run_replace_all (line, "into", "in");
  line = run_replace_all (line, "onto", "on");

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
 * run_try_command_table()
 *
 * Search a command table for a match to the string, returning TRUE on the
 * first matching command whose handler succeeds.
 */
static scr_bool run_npc_library_blocked (scr_gameref_t game);
static scr_bool run_npc_row_blocked (const scr_commands_t *command);

static scr_bool
run_try_command_table (scr_commandsref_t command,
                       scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_bool npc_blocked = run_npc_library_blocked (game);

  for (; command->command; command++)
    {
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
       * (Adrift_93/95); `x cathy` on that first mention gets "You see no
       * such thing." (Adrift_98); with the task spent, both fall to the NPC
       * handlers -- "I don't think girl would appreciate being handled."
       * and her description.
       *
       * The rows that survive a task are the ones the Runner reaches by
       * another route: give is handled in the input routine at 48A98A with
       * no flag test; `ask X about Y` is re-opened at 47F922-47F935 by the
       * "<player> can't talk to that." buffer that generaltasks_verbs seeds
       * at 488C65 whenever no object took the ask (run390 tests no flag at
       * all at 4597FE) -- Humbug's silent scoring task `ask * hacker about
       * * humbug` still gets the hacker's reply (Adrift_4_humbug.txt);
       * kiss (47F7E7) is gated on the buffer, not the flag; and the
       * end-of-handler "I don't understand what you want to do with"
       * fallback at 4805DA asks for an empty buffer rather than the flag.
       * (It does have one other guard, added here 2026-09-07:
       * `loc_4805CD: If MemVar_4941AD > 0 Then Exit Sub` above it takes it
       * off a line that a task has just ended the game on -- see
       * lib_cmd_verb_npc().)
       */
      if (npc_blocked && run_npc_row_blocked (command))
        continue;

      if (uip_match (command->command, string, game))
        {
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
static scr_bool
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


static scr_bool
run_standard_commands (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * Search movement commands first, returning TRUE if any matching command
   * handler succeeded.  Then repeat for standard library commands.
   */
  if (run_try_command_table (run_move_commands (bundle), game, string))
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
  uip_set_containment (TRUE);
  const scr_bool contained =
      run_try_command_table (STANDARD_COMMANDS, game, string);
  uip_set_containment (FALSE);
  if (contained)
    return TRUE;

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
  uip_set_containment (TRUE);
  const scr_bool fallback =
      run_try_command_table (STANDARD_FALLBACK_COMMANDS, game, string);
  uip_set_containment (FALSE);
  if (fallback)
    return TRUE;

  /* Nothing matched the string.  Or if it did, its handler failed. */
  return FALSE;
}


/*
 * run_update_status()
 *
 * Update the game's current room and status line strings.
 */
static void
run_update_status (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *name, *status;
  scr_char *filtered;
  scr_bool statusbox;

  /* Get the current room name, and filter and untag it. */
  name = lib_get_room_name (game, gs_playerroom (game));
  filtered = pf_filter (name, vars, bundle);
  pf_strip_tags (filtered);

  /* Save this room name; the owning pointer frees any existing name. */
  game->current_room_name.reset (filtered);

  /* See if the game does a status box. */
  statusbox = prop_get_global_boolean (bundle, "StatusBox");
  if (statusbox)
    {
      /* Get the status line, and filter and untag it. */
      status = prop_get_global_string (bundle, "StatusBoxText");
      filtered = pf_filter (status, vars, bundle);
      pf_strip_tags (filtered);
    }
  else
    /* No status line, so use NULL. */
    filtered = NULL;

  /* Save this status text; the owning pointer frees any existing line. */
  game->status_line.reset (filtered);
}


/*
 * run_notify_score_change()
 *
 * Print an indication of any score change, if appropriate.  The change is
 * detected by comparing against the undo game.  Uses if_print_string()
 * directly for printing, rather than the filter, so that it can place its
 * output ahead of buffered printfilter text.
 */
static void
run_notify_score_change (scr_gameref_t game)
{
  const scr_gameref_t undo = game->undo;
  scr_char buffer[32];
  assert (gs_is_game_valid (undo));

  /*
   * Do nothing if no undo available, or if notification is off, or if we've
   * already done this once this turn.
   */
  if (!game->undo_available
      || !game->notify_score_change || game->has_notified)
    return;

  /* Note any change in the score. */
  if (game->score > undo->score)
    {
      if_print_string ("(Your score has increased by ");
      snprintf (buffer, sizeof(buffer), "%ld", game->score - undo->score);
      if_print_string (buffer);
      if_print_string (")\n");
    }
  else if (game->score < undo->score)
    {
      if_print_string ("(Your score has decreased by ");
      snprintf (buffer, sizeof(buffer), "%ld", undo->score - game->score);
      if_print_string (buffer);
      if_print_string (")\n");
    }
  game->has_notified = TRUE;
}


/*
 * Cached per-task command patterns.
 *
 * run_match_task_commands() is called for every task on every player command,
 * and before caching it re-read the task's (Reverse)Command pattern strings
 * from the bundle on each attempt.  The patterns are immutable once a game
 * is loaded, and prop_get_string() returns stable pointers into the bundle,
 * so remember each task's pattern list (per direction) the first time it is
 * walked; the walk itself goes through the same fatal-checking prop_get_*
 * wrappers the uncached code used.  The cache tracks a single game;
 * gs_destroy() calls run_forget_game().
 */
typedef struct
{
  scr_bool known[2];                          /* indexed [forwards] */
  std::vector<const scr_char *> patterns[2];
} scr_task_commands_t;

static scr_bool run_task_passes_class_filter (scr_gameref_t game,
                                              scr_int task);
static const void *run_cache_game = NULL;
static std::vector<scr_task_commands_t> run_cache;

static const std::vector<const scr_char *> &
run_task_command_patterns (scr_gameref_t game, scr_int task,
                           scr_bool forwards)
{
  const int direction = forwards ? 1 : 0;
  scr_task_commands_t *cached;

  if (run_cache_game != game)
    {
      run_cache.assign (gs_task_count (game), scr_task_commands_t ());
      run_cache_game = game;
    }

  cached = &run_cache[task];
  if (!cached->known[direction])
    {
      const scr_prop_setref_t bundle = gs_get_bundle (game);
      scr_vartype_t vt_key[4];
      scr_int command_count, command;

      vt_key[0].string = "Tasks";
      vt_key[1].integer = task;
      vt_key[2].string = forwards ? "Command" : "ReverseCommand";
      command_count = prop_get_child_count (bundle, "I<-sis", vt_key);
      cached->patterns[direction].reserve (command_count);
      for (command = 0; command < command_count; command++)
        {
          vt_key[3].integer = command;
          cached->patterns[direction]
              .push_back (prop_get_string (bundle, "S<-sisi", vt_key));
        }
      cached->known[direction] = TRUE;
    }
  return cached->patterns[direction];
}

/*
 * run_forget_game()
 *
 * Drop any command pattern cache built for the given game.  Called from
 * gs_destroy() so a stale cache can never outlive its game.
 */
void
run_forget_game (const void *game)
{
  if (run_cache_game == game)
    {
      run_cache_game = NULL;
      run_cache.clear ();
    }
}

/*
 * run_pattern_names_verb()
 *
 * Helper for run_match_task_commands().  Return TRUE if the pattern
 * contains, as a standalone whitespace-delimited token, the first word of
 * the string passed in (case insensitive).
 */
static scr_bool
run_pattern_names_verb (const scr_char *pattern, const scr_char *string)
{
  static const scr_char *const PATTERN_WORD_BREAK = "\t\n\v\f\r *";
  const scr_char *verb;
  scr_int verb_length;

  /* Isolate the first word of the string; no word, no possible match. */
  verb = string + strspn (string, WHITESPACE);
  verb_length = strcspn (verb, WHITESPACE);
  if (verb_length == 0)
    return FALSE;

  /*
   * Scan pattern tokens for a case-insensitive whole-word match.  A '*' ends
   * a token as surely as a space does: authors write wildcards glued to their
   * words, and frustrated.taf's `*drop*tree*` names the verb "drop" just as
   * plainly as "* drop * tree *" would.  Treating the glued form as one
   * eleven-character token hid the verb, and the pattern -- being
   * wildcard-leading -- was then skipped by every library call, so the
   * 4.0 drop handler's look-up could not find it (run400
   * Adrift_274_frustrated.txt gives it the line).
   */
  for (pattern += strspn (pattern, PATTERN_WORD_BREAK); *pattern != NUL;)
    {
      const scr_int token_length = strcspn (pattern, PATTERN_WORD_BREAK);

      if (token_length == verb_length
          && scr_strncasecmp (pattern, verb, verb_length) == 0)
        return TRUE;

      pattern += token_length;
      pattern += strspn (pattern, PATTERN_WORD_BREAK);
    }

  return FALSE;
}

/*
 * The player's current command element (pronoun-substituted), stashed by
 * run_all_commands() so that library-initiated match attempts can consult
 * the verb the player actually typed alongside the library's canonical
 * constructed command ("get <object>", and so on).
 */
static const scr_char *run_dispatch_input = NULL;

/*
 * run_get_dispatch_input()
 *
 * Expose the stashed command element to other modules.  sclibrar's
 * SCR_TRACE_CO diagnostic needs the player's own words to reproduce the
 * Runner's whole-command containment test.
 */
const scr_char *
run_get_dispatch_input (void)
{
  return run_dispatch_input;
}

/*
 * Tasks that have already been run by the current command element, reset by
 * run_all_commands() alongside run_dispatch_input.
 *
 * A command is dispatched to the tasks twice, once with restrictions ignored
 * and once with them honoured, and the spent-task refusal pass below has to
 * know the difference between a task that was already done when the player
 * typed, and one this very command has just completed.  Only the first is a
 * refusal.  "Shadow of the Past" is the case: `examine good book` runs a
 * silent task (it drops a key and scores, and has no completion text), which
 * turns its own "book not yet crumbled" restriction false; the library
 * examine then prints the book's -- now crumbled -- description.  run400
 * answers the first `examine good book` with that description and only a
 * second one with the restriction's "The book is nothing but dust now."
 * (measured live 2026-08-23, Adrift_15.txt).
 */
static std::vector<scr_bool> run_tasks_ran_this_command;

/*
 * The last typed command as the dispatcher saw it, and whether a game task
 * ran for it: the pre-4.0 end-of-turn ambiguity prompt (see
 * lib_co_ambiguity_prompt()) fires only on a line no task claimed, and the
 * Runner's flag for that (MemVar_44F12C) is set at the same place any task
 * executes, library-callback tasks included.
 */
static std::string run_co_pending_input;
static scr_bool run_co_task_claimed = FALSE;

static void
run_note_task_ran (scr_gameref_t game, scr_int task)
{
  run_co_task_claimed = TRUE;
  if (run_tasks_ran_this_command.size () != (size_t) gs_task_count (game))
    run_tasks_ran_this_command.assign (gs_task_count (game), FALSE);
  run_tasks_ran_this_command[task] = TRUE;
}

static scr_bool
run_task_ran_this_command (scr_int task)
{
  return (size_t) task < run_tasks_ran_this_command.size ()
         && run_tasks_ran_this_command[task];
}

/*
 * run_npc_library_blocked()
 *
 * TRUE while the library's NPC rows are to be skipped: a 3.9+ game in which
 * some task has already run for the current line.  See the note in
 * run_try_command_table().
 */
static scr_bool
run_any_task_ran_this_command (void)
{
  for (const scr_bool ran : run_tasks_ran_this_command)
    {
      if (ran)
        return TRUE;
    }
  return FALSE;
}

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
  if (strstr (command->command, "%character%") == NULL)
    return FALSE;
  return command->handler != lib_cmd_give_object_npc
         && command->handler != lib_cmd_ask_npc_about
         && command->handler != lib_cmd_talk_to_npc_about
         && command->handler != lib_cmd_kiss_npc
         && command->handler != lib_cmd_status_npc
         && command->handler != lib_cmd_verb_npc;
}

/*
 * UNPORTED, measured 2026-08-23 (make_39_doneprobe.py, run390 Adrift_18.txt
 * and Adrift_19.txt): below 4.0 a game task that matches the command element
 * claims it even when it says nothing, so the standard library verb that would
 * otherwise answer never gets a turn.  `x book` on a spent `* x * book *` task
 * answers "You have already done that." instead of the book's description,
 * where `look at book` -- matching no task -- prints the description; and a
 * silent task that runs and prints nothing leaves "I don't understand." rather
 * than the library answer.  4.0 dropped this: run400 falls through to the
 * library examine in both cells (Adrift_14.txt, Adrift_15.txt).
 *
 * Implementing it as written -- record the silent match, then skip
 * run_standard_commands() below 4.0 -- costs 15 v4-corpus goldens, several of
 * them whole walkthroughs that stop winning, so the rule as stated is too
 * broad and the narrowing is not yet measured.  Left out until it is; see
 * test/adrift4/notes/RUNNER_TESTS_TODO.md.
 */

/*
 * scr_strict_reference_guard
 *
 * Turns strict %object% matching on for the lifetime of the guard, and off
 * again however the scope is left.  match_case separates the two Runners that
 * bind strictly: 4.0 substitutes the name as authored, 3.90 lower-cases it.
 */
class scr_strict_reference_guard
{
public:
  scr_strict_reference_guard (scr_bool strict, scr_bool match_case)
    : strict_ (strict)
  {
    if (strict_)
      uip_set_strict_reference (TRUE, match_case);
  }

  ~scr_strict_reference_guard ()
  {
    if (strict_)
      uip_set_strict_reference (FALSE, FALSE);
  }

  scr_strict_reference_guard (const scr_strict_reference_guard &) = delete;
  scr_strict_reference_guard &
  operator= (const scr_strict_reference_guard &) = delete;

private:
  const scr_bool strict_;
};


/*
 * run_match_task_commands()
 *
 * Helper for run_game_commands_common().
 *
 * Search task command for a match to the string passed in, returning TRUE
 * if a task command matches, FALSE otherwise.  Ordinary or reverse commands
 * are selected by 'forwards'.
 */
static scr_bool
run_match_task_commands (scr_gameref_t game,
                         scr_int task, const scr_char *string,
                         scr_bool forwards, scr_bool is_library)
{
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, forwards);
  const scr_int command_count = (scr_int) patterns.size ();
  scr_int command;
  scr_bool is_matched;

  /* 3.90 and up bind %object% strictly, and only 4.0 binds it case-
   * sensitively -- see the note above uip_compare_reference_strict().  Task
   * commands only; the library's own patterns keep the tolerant matcher. */
  const scr_int version = run_get_version (gs_get_bundle (game));
  const scr_strict_reference_guard strict_reference
      (version >= TAF_VERSION_390, version >= TAF_VERSION_400);

  /* Iterate over commands, looking for patterns that match string. */
  is_matched = FALSE;
  for (command = 0; command < command_count; command++)
    {
      const scr_char *pattern;
      scr_int first;

      /* Retrieve the pattern for this command, find its first character. */
      pattern = patterns[command];
      first = strspn (pattern, WHITESPACE);

      /*
       * Make a special case of library calls and commands that begin with a
       * wildcard.  Probed live in run400 (2026-08-22, probes pPREC and
       * pPREC2, transcripts Adrift_10/11.txt): a wildcard-leading pattern
       * with failing messaged restrictions blocks the system take only when
       * the pattern explicitly names a verb -- either the library's
       * canonical verb ("* get * tent *" blocks both "get tent" and "take
       * tent") or the verb the player actually typed ("* take * tent *"
       * blocks "take tent" but NOT "get tent").  A verb-less "* ball *"
       * pattern never blocks: the system take wins even though the same
       * pattern with passing restrictions would run as a game command.
       * Failing restrictions with an empty message fall through to the
       * system command silently in every case (that drops out of the
       * loudly-restricted machinery here without special handling).
       *
       * The library constructs its match string with the canonical verb
       * ("get <object>"), so a pattern naming only the typed verb cannot
       * match it; for those, retry the match against the player's actual
       * input, stashed by run_all_commands().
       */
      if (pattern[first] == SPECIAL_PATTERN)
        ;
      else if (is_library && pattern[first] == WILDCARD_PATTERN)
        {
          if (run_pattern_names_verb (pattern, string))
            is_matched = uip_match (pattern, string, game);
          if (!is_matched && run_dispatch_input != NULL
              && run_pattern_names_verb (pattern, run_dispatch_input))
            is_matched = uip_match (pattern, run_dispatch_input, game);
        }
      else
        is_matched = uip_match (pattern, string, game);

      /* Stop searching if we find a match. */
      if (is_matched)
        {
#ifdef SCARIER_DUMP_TOOLS
          {
            static const scr_bool trace_match =
                getenv ("SCR_TRACE_MATCH") != NULL;
            if (trace_match)
              fprintf (stderr, "MATCH task=%ld pattern=[%s] input=[%s]\n",
                       task, pattern, string);
          }
          {
            /* SCR_TRACE_SCOPE: report matches the real Runner would refuse
             * or bind differently.  Its parser only matches a %object%
             * against objects present to the player (probed live in Topaz
             * and pBP2), while uip_match_entity() has no scope filter and
             * binds the LAST name match.  SCOPE-MISS = no matched object is
             * present (the Runner would fail the whole command); SCOPE-BIND
             * = the bound object is absent while a present one also
             * matched (the Runner would bind the present one). */
            static const scr_bool trace_scope =
                getenv ("SCR_TRACE_SCOPE") != NULL;
            if (trace_scope && strstr (pattern, "%object%") != NULL)
              {
                const scr_var_setref_t vars = gs_get_vars (game);
                scr_int object, present, matched, room, bound_present;

                room = gs_playerroom (game);
                present = matched = 0;
                bound_present = FALSE;
                for (object = 0; object < gs_object_count (game); object++)
                  {
                    if (!game->object_references[object])
                      continue;
                    matched++;
                    if (obj_indirectly_in_room (game, object, room))
                      {
                        present++;
                        if (object == var_get_ref_object (vars))
                          bound_present = TRUE;
                      }
                  }
                if (matched > 0 && present == 0)
                  fprintf (stderr, "SCOPE-MISS task=%ld pattern=[%s]"
                           " input=[%s]\n", task, pattern, string);
                else if (present > 0 && !bound_present)
                  fprintf (stderr, "SCOPE-BIND task=%ld pattern=[%s]"
                           " input=[%s]\n", task, pattern, string);
              }
          }
#endif
          break;
        }
    }

  /* Return TRUE if we found a pattern match. */
  return is_matched;
}


/*
 * run_task_is_unrestricted()
 * run_task_is_loudly_restricted()
 *
 * Helpers for run_game_commands_common().
 *
 * Adapters for uncovering task restriction state.  The first returns TRUE
 * if the task is unrestricted, and can therefore run unimpeded.  The second
 * returns TRUE iff the task is restricted and has a fail message that
 * indicates why it fails; such tasks, if run, produce their failure message
 * and don't change state.
 */
static scr_bool
run_task_is_unrestricted (scr_gameref_t game, scr_int task)
{
  scr_bool restrictions_passed;
  const scr_char *fail_message;

  /*
   * Evaluate task restrictions, and if they fail to parse for some reason,
   * return as if restrictions did not pass.
   */
  if (!restr_eval_task_restrictions (game, task,
                                     &restrictions_passed, &fail_message))
    {
      scr_error ("run_task_is_unrestricted: restrictions error, %ld\n", task);
      return FALSE;
    }

  /* Return TRUE if the task is unrestricted. */
  return restrictions_passed;
}

static scr_bool
run_task_is_loudly_restricted (scr_gameref_t game, scr_int task)
{
  scr_bool restrictions_passed;
  const scr_char *fail_message;

  /*
   * Evaluate task restrictions, and if they fail to parse for some reason,
   * return as if restrictions did not pass.
   */
  if (!restr_eval_task_restrictions (game, task,
                                     &restrictions_passed, &fail_message))
    {
      scr_error ("run_task_is_loudly_restricted:"
                " restrictions error, %ld\n", task);
      return TRUE;
    }

  /* Return TRUE if the task is restricted and indicates why. */
  return !restrictions_passed && (fail_message != NULL);
}


/*
 * run_task_is_silent_and_literal()
 *
 * First half of run_game_commands_common()'s exclude_silent_literal peek (see
 * run_all_commands(); the other half is run_task_reachable_by_library_
 * callback()).  Return TRUE for a task that (a) has no CompleteText of its
 * own to print and (b) has no wildcard-leading command pattern in either
 * direction -- i.e. a task that, if matched here, would run its actions with
 * no visible sign that it ran at all, and whose every pattern is specific
 * enough that the priority-command callback (run_game_task_commands(),
 * is_library=TRUE) could in principle get a look at it first.
 *
 * Silence alone is necessary but nowhere near sufficient, and must never be
 * used on its own: "Sommeril" task 35 ("take silver orb", +5, no message) is
 * silent and literal in exactly this sense, yet the Runner runs it and then
 * lets the library's own take answer "You are already carrying the SILVER
 * ORB." -- which is why the caller also requires that no library short form
 * can reach the pattern.  Both halves are load-bearing: dropping this one
 * costs 27 corpus walkthroughs, dropping the other costs 4.
 *
 * The case that needs excluding is "Space Boy's First Adventure" task 72,
 * "drop cape to the floor" (+250, no message) -- run400 lets the library's
 * ordinary drop win outright and the task never runs at all (Adrift_8_pET2.txt/
 * Adrift_10_pET4.txt, 2026-08-23).  The case that must NOT be excluded is that
 * same game's task 27, "{take/get}{them/boots}" (CompleteText "Taken."),
 * which must still win over the library's own take (run_v4_walkthroughs.sh
 * space_boy golden, "Taken." not "You take the pair of Flight Boots.").
 * Task 27 is kept safe here rather than by reachability: the library only
 * ever builds its callback string from the object's display Short name
 * ("get pair of Flight Boots"), never from an alias like "boots", so 27
 * looks unreachable too -- but it has real CompleteText and so is never
 * silent, and priority's generic take never gets to steal it.
 *
 * "No visible sign that it ran" has to cover every way task_run_task_
 * unrestricted() (sctasks.cpp) can print, not just CompleteText: a
 * ShowRoomDesc auto room-description or an AdditionalMessage are both
 * static per-task properties read the same cheap way, and both print
 * unconditionally when set (see the comment on that code, topaz.taf task 22
 * and marooned.taf task 29).  Found live on "Princess in the Tower"
 * (princess1.taf): task 15, the win task for walking into the tower, has
 * empty CompleteText but a non-zero ShowRoomDesc, so the original
 * CompleteText-only version of this check misclassified it as silent,
 * excluded it from the peek, and let task 18's refusal (identical literal
 * pattern "in") win the peek instead -- breaking the golden.  Also exclude
 * any task with an Execute-Task (action type 5) or End-Game (action type 6)
 * action: task_run_task_action() (sctasks.cpp) is the only place besides
 * CompleteText/ShowRoomDesc/AdditionalMessage that can make a task's status
 * come back TRUE, and only those two action types ever set it -- an
 * End-Game action prints a score summary/ending text of its own, and an
 * Execute-Task action cascades into another task whose own output can't be
 * predicted from here.  The other action types (move object, move NPC,
 * change object status, change variable, change score, change battle
 * attribute) never do.  A task tripping either check just falls through to
 * a full, unexcluded try in the next pass, same as any wildcard-leading
 * task -- this is a conservative "don't know, so don't peek" exclusion, not
 * a correctness requirement in itself.
 */
/*
 * run_task_reachable_by_library_callback()
 *
 * Second half of the exclude_silent_literal test (see run_all_commands()).
 * Return TRUE if the library's own priority-command callback could ever put
 * this task's pattern in front of run_game_task_commands() -- that is, if
 * some object's canonical "verb OBJECT" short form (the only shape
 * lib_try_game_command_common() ever constructs: "<verb> <Prefix> <Short>"
 * and, prefix dropped, "<verb> <Short>") matches one of the task's patterns.
 *
 * This is what actually separates the two run400-probed cases the peek has
 * to tell apart, and it is a property of the *pattern*, not of the task's
 * silence.  "Space Boy's First Adventure" task 72 is "drop cape to the
 * floor": the library's short forms are "drop the cape" and "drop cape", and
 * neither matches a pattern that insists on the trailing "to the floor", so
 * the Runner's library claims the command outright and the task never runs
 * (Adrift_8_pET2.txt/Adrift_10_pET4.txt, 2026-08-23).  "Sommeril" task 35 is the bare
 * "take silver orb", which IS the library's own short form for that object,
 * so the Runner runs it (silently, +5) and the library's take then answers
 * "You are already carrying the SILVER ORB." -- the shape the golden records.
 * Excluding the latter along with the former loses the task and five points;
 * see the corpus note in run_all_commands().
 *
 * uip_match() binds game object references as it goes, and this runs at the
 * moment a real match has already bound them for the task about to run, so
 * save and restore them around the probe -- same contract
 * lib_try_game_command_common() keeps for its own speculative matches.
 */
static scr_bool
run_task_reachable_by_library_callback (scr_gameref_t game, scr_int task,
                                        const scr_char *string,
                                        scr_bool is_forwards)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, is_forwards);
  std::vector<scr_bool> references (game->object_references);
  const scr_char *verb;
  scr_int verb_length, object;
  scr_bool is_reachable = FALSE;

  /* Isolate the verb the player typed; no verb, nothing to construct. */
  verb = string + strspn (string, WHITESPACE);
  verb_length = strcspn (verb, WHITESPACE);
  if (verb_length == 0)
    return FALSE;

  for (object = 0; object < gs_object_count (game) && !is_reachable; object++)
    {
      scr_vartype_t vt_key[3];
      const scr_char *prefix, *name;
      std::string candidates[2];
      scr_int form;

      vt_key[0].string = "Objects";
      vt_key[1].integer = object;
      vt_key[2].string = "Prefix";
      prefix = prop_get_string (bundle, "S<-sis", vt_key);
      vt_key[2].string = "Short";
      name = prop_get_string (bundle, "S<-sis", vt_key);
      if (scr_strempty (name))
        continue;

      candidates[0] = std::string (verb, verb_length) + " " + name;
      candidates[1] = std::string (verb, verb_length) + " "
                      + (prefix ? prefix : "") + " " + name;

      for (form = 0; form < 2 && !is_reachable; form++)
        {
          scr_int command;

          for (command = 0; command < (scr_int) patterns.size (); command++)
            {
              if (uip_match (patterns[command], candidates[form].c_str (),
                             game))
                {
                  is_reachable = TRUE;
                  break;
                }
            }
        }
    }

  game->object_references = references;
  return is_reachable;
}


static scr_bool
run_task_is_silent_and_literal (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  const scr_char *completetext, *additionalmessage;
  scr_int direction, showroomdesc, action_count, action;

  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "CompleteText";
  completetext = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (completetext))
    return FALSE;

  vt_key[2].string = "ShowRoomDesc";
  showroomdesc = prop_get_integer (bundle, "I<-sis", vt_key);
  if (showroomdesc != 0)
    return FALSE;

  vt_key[2].string = "AdditionalMessage";
  additionalmessage = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (additionalmessage))
    return FALSE;

  vt_key[2].string = "Actions";
  action_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  for (action = 0; action < action_count; action++)
    {
      scr_int type;

      vt_key[3].integer = action;
      vt_key[4].string = "Type";
      type = prop_get_integer (bundle, "I<-sisis", vt_key);
      if (type == 5 || type == 6)
        return FALSE;
    }

  for (direction = 0; direction < 2; direction++)
    {
      const scr_bool is_forwards = !direction;
      const std::vector<const scr_char *> &patterns =
          run_task_command_patterns (game, task, is_forwards);
      scr_int command;

      for (command = 0; command < (scr_int) patterns.size (); command++)
        {
          const scr_char *pattern = patterns[command];
          const scr_int first = strspn (pattern, WHITESPACE);

          if (pattern[first] == WILDCARD_PATTERN)
            return FALSE;
        }
    }
  return TRUE;
}


/*
 * run_game_commands_common()
 * run_game_commands_in_parser_context()
 *
 * The central handler for running, or at least trying to run, game-defined
 * tasks that have commands that match the input string.  Here's the algorithm
 * as currently understood (and it may not be right, so be warned):
 *
 *  for each task executable in the current room
 *    for direction in forwards, backwards
 *      for each command string defined by the task for this direction
 *        match against player input
 *      if any command string matched player input
 *        if task restrictions pass
 *          run the task actions in the current direction
 *          if the task actions produced output
 *            return
 *          is_matched := true
 *          break out of all loops
 *
 *  if not is_matched and we're allowing restrictions to fail tasks
 *    for each task executable in the current room
 *      for direction in forwards, backwards
 *        for each command string defined by the task for this direction
 *          match against player input
 *        if any command string matched player input
 *          if task restrictions fail with an error message
 *            run the task, to persuade it to print this error message
 *            return
 *
 * Part of the fun and games is that run_game_task_commands() is called by the
 * library to try to run "get " and "drop " game commands for standard get/drop
 * handlers and get_all/drop_all handlers.  No pressure, then.
 *
 * exclude_silent_literal makes the first (unrestricted) loop below a "peek":
 * a task that matches, would win, and is both silent (run_task_is_silent_and_
 * literal()) and unreachable from the library's own callback string
 * (run_task_reachable_by_library_callback()) does not win -- and, crucially,
 * neither does any task after it.  The whole pass is abandoned and returns
 * FALSE, because ADRIFT task precedence is "lowest matching index wins": a
 * task that matched cannot be stepped over in favour of a later one without
 * inventing a match the Runner never makes.  See run_all_commands().  This
 * has no effect on the second (loudly-restricted) loop, which is about
 * failing-restriction messages, not CompleteText.
 */
static scr_bool
run_game_commands_common (scr_gameref_t game, const scr_char *string,
                          scr_bool include_restrictions, scr_bool is_library,
                          scr_bool exclude_silent_literal)
{
  scr_bool is_matched = FALSE, is_handled = FALSE, is_abandoned = FALSE;
  scr_int task_count, task, direction;

  /*
   * Matching is expensive, so it helps to use a cache of results from the
   * first loop in the second.  If we're using the second, that is.  The cache
   * stays empty when restrictions are off (the second loop is then skipped).
   */
  /*
   * The Runner dispatches ONE task per typed line.  run400's dispatcher
   * Proc_19_24_44CCE0 asks the pre-matcher (Proc_19_66_454EF0) for a single
   * task, runs it, and returns "handled" only if the message buffer is
   * non-empty (44CCC0); the restriction-failure pass (Proc_19_68_45404C)
   * runs only when no task was found at all (44CCA5).  A silent task
   * therefore lets the LIBRARY run, but never a second task.  run390's
   * tasks() (42BDC4) is the same shape: checktask picks one task, execute_task
   * runs it.  Scarier reaches the same line in several passes (peek, no
   * restrictions, restrictions), and the later passes used to re-scan from
   * task 0 with the first task now spent: House (4.00, 2026-09-06,
   * Adrift_100) `kiss cathy` as the first line naming Cathy runs the silent
   * once-only task 200 `*cathy*`, and run400 then prints the library's
   * "I'm not sure she would appreciate that!" -- the game's own kiss task
   * 882 `[hug/kiss/touch/shake] [her/cathy]` only runs on the SECOND kiss.
   * Scarier ran 200 and then 882 on the first line.  Library callbacks
   * (is_library) are left alone: those model the Runner's own pre-matcher
   * look-ups from inside the library handlers, which happen after the
   * dispatcher regardless.
   */
  if (!is_library && run_any_task_ran_this_command ())
    return FALSE;

  task_count = gs_task_count (game);
  std::vector<scr_bool> is_matching;
  if (include_restrictions)
    is_matching.assign (task_count, FALSE);

  /*
   * Iterate over every task, ignoring those not runnable.  For each runnable
   * task, try matching task commands, and on matches, check restrictions and
   * if they pass, try running the task.
   *
   * Spent tasks (done, non-repeatable, but still in their rooms) also get
   * their forwards commands matched -- not to run them, but to seed the
   * cache for the loud restriction-failure pass below.  The Runner checks a
   * matched task's restrictions before its done state, so a spent task whose
   * restrictions fail with a message still prints that message (run400,
   * Provenance's squeeze-through-the-hole task: a second "s" at the hole
   * re-prints "The only way you are going to make it through that hole is if
   * you drop everything you are carrying.", 2026-08-22).  A spent task whose
   * restrictions PASS instead falls through to run_task_refusal(), which
   * answers with RepeatText or "You have already done that.".
   *
   * This holds on the library-callback path too, not just for plain game
   * commands: probe DONE, task `* get * gem *` with a "holding the stone"
   * restriction, run400 2026-08-23 -- once the task is spent, `get gem`
   * without the stone answers "BLOCK-GEM." instead of taking the gem, and
   * with the stone falls through to the library take ("Player take the
   * gem.").  So `is_library` does not gate the spent-task match here.
   *
   * All of that is 4.0 only.  The 3.9 twin of the probe (p39done.taf, built
   * by test/adrift4/harness/make_39_doneprobe.py) says run390 orders the two
   * tests the other way about: a spent task answers "You have already done
   * that." whether its restrictions pass or fail, and never prints a fail
   * message (`alpha` and `x book` after dropping the stone, Adrift_18.txt
   * 2026-08-23).  Pre-4.0 therefore leaves spent tasks out of the loud
   * restriction pass entirely, and run_task_refusal() answers them.
   */
  const scr_bool is_restriction_first =
      run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400;

  for (task = 0; task < task_count; task++)
    {
      const scr_bool is_refusing = include_restrictions
                                   && is_restriction_first
                                   && !run_task_ran_this_command (task)
                                   && task_is_done_refused (game, task);

      if (!is_refusing && !task_can_run_task (game, task))
        continue;
      if (!run_task_passes_class_filter (game, task))
        continue;

      /*
       * Try matching forwards and reverse commands.  If there's a match for
       * unrestricted tasks, run the task, and if it runs (defined as printing
       * some game output), we're done; otherwise, note the command match but
       * keep searching for other possible matches.
       */
      for (direction = 0; direction < 2; direction++)
        {
          const scr_bool is_forwards = !direction;
          const scr_bool is_runnable_directional =
              task_can_run_task_directional (game, task, is_forwards);

          if (!is_runnable_directional && !(is_refusing && is_forwards))
            continue;

          if (run_match_task_commands (game, task, string,
                                       is_forwards, is_library))
            {
              if (is_runnable_directional
                  && run_task_is_unrestricted (game, task))
                {
                  /*
                   * In the peek pass, a silent, literal task does not win --
                   * but neither may any task after it.  Abandon the whole
                   * pass so that task precedence (lowest matching index wins)
                   * is preserved: priority commands get their look next, and
                   * if they don't claim the command the immediately following
                   * unexcluded pass re-matches from task 0.  Skipping just
                   * this task and reading on would hand the command to a
                   * later, lower-precedence task the Runner never reaches --
                   * "The Forum" task 1 (literal "x ... hand", silent, falls
                   * through to the library's own examine) losing to task 2
                   * ("[examine/...]{at}[%object%]", which has CompleteText of
                   * its own), and seven more corpus walkthroughs like it.
                   */
                  if (exclude_silent_literal
                      && run_task_is_silent_and_literal (game, task)
                      && !run_task_reachable_by_library_callback (
                             game, task, string, is_forwards))
                    {
                      is_abandoned = TRUE;
                      break;
                    }

                  run_note_task_ran (game, task);
                  if (task_run_task (game, task, is_forwards))
                    is_handled = TRUE;
                  is_matched = TRUE;
                  break;
                }

              if (!is_matching.empty ())
                is_matching[task] = TRUE;
            }
        }
      if (is_matched || is_abandoned)
        break;
    }

  if (is_abandoned)
    return FALSE;

  /*
   * If no match, and we've been asked to consider failing restrictions, look
   * through all of the runnable tasks again, this time searching for
   * restricted ones with a fail message.  Use the cache built above to weed
   * out matches that are certain to fail.
   */
  if (!is_handled && !is_matched && include_restrictions)
    {
      for (task = 0; task < task_count; task++)
        {
          scr_bool is_refusing;

          if (!is_matching[task])
            continue;

          /* Spent tasks are eligible here too (4.0); see the first loop. */
          is_refusing = is_restriction_first
                        && !run_task_ran_this_command (task)
                        && task_is_done_refused (game, task);

          /*
           * Check matches of forwards and reverse commands.  If there's a
           * match for restricted tasks (ones that have and will print a fail
           * message if we try to run them), run the task to get the print of
           * the fail message, and we're done.
           */
          for (direction = 0; direction < 2; direction++)
            {
              const scr_bool is_forwards = !direction;

              if ((task_can_run_task_directional (game, task, is_forwards)
                   || (is_refusing && is_forwards))
                  && run_match_task_commands (game, task, string,
                                              is_forwards, is_library))
                {
                  if (run_task_is_loudly_restricted (game, task))
                    {
                      run_note_task_ran (game, task);
                      if (task_run_task (game, task, is_forwards))
                        {
                          is_handled = TRUE;
                          break;
                        }
                    }
                }
            }
          if (is_handled)
            break;
        }
    }

  /* Return TRUE if any game task handled the command in some way. */
  return is_handled;
}

static scr_bool
run_game_commands_in_parser_context (scr_gameref_t game, const scr_char *string,
                                     scr_bool include_restrictions,
                                     scr_bool exclude_silent_literal)
{
  /*
   * Try game commands, either with or without restrictions, and all full and
   * complete parse matching (no special case for game commands that begin
   * with a '*' wildcard).
   */
  return run_game_commands_common (game, string, include_restrictions, FALSE,
                                   exclude_silent_literal);
}

/*
 * run_task_class_filter, run_set_task_class_filter()
 * run_task_passes_class_filter()
 *
 * The 4.0 Runner's task pre-matcher (Proc_19_35_453C50) takes a mode byte and
 * skips every task that Proc_21_57_4494FC rejects for it: mode 1 keeps only
 * tasks whose record byte 104 is set, mode 2 only those with byte 105 set,
 * mode 3 either.  Both bytes are computed once at LOAD (mdlSpreadTheLoad
 * @4931B5 and @493225): byte 104 when any of the task's command patterns
 * contains "get", "take" or "pick" as a substring, byte 105 when one contains
 * "drop", "leave" or "put", and a pattern that is exactly "*" sets both
 * (@493281-49328D).  The mode each caller passes is fixed: the put handler's
 * implicit-take gate and the get handler's refusal exits pre-match with 1,
 * the put and drop handlers' own look-ups (name_object's "Drop what?" and
 * "It is not clear" gates, the insides handler's canonical rebuild, "drop
 * all") with 2, and the insides handler's typed-line fallback with 0, no
 * filter at all.
 *
 * The consequence is visible in House (House.taf): task 459
 * "[put/place/drop] {some} [wood] {in/into/in to} {the} [fireplace/fire
 * place]" carries the put flag only, so "put wood in fireplace" with the
 * wood on the floor sails past the mode-1 take gate into "(Taking the wood
 * first)", where the old unfiltered pre-match would have hit the task and
 * suppressed the take.  The Runner's substring test is a binary-compare
 * InStr on the stored pattern; ours is case-insensitive, which only differs
 * for an author who capitalized a verb inside a pattern.
 */
static scr_int run_task_class_filter = 0;

void
run_set_task_class_filter (scr_int mode)
{
  assert (mode >= 0 && mode <= 3);
  run_task_class_filter = mode;
}

static scr_bool
run_pattern_contains (const scr_char *pattern, const scr_char *word)
{
  const size_t length = strlen (word);
  const scr_char *cursor;

  for (cursor = pattern; *cursor; cursor++)
    {
      if (scr_strncasecmp (cursor, word, length) == 0)
        return TRUE;
    }
  return FALSE;
}

static scr_bool
run_task_passes_class_filter (scr_gameref_t game, scr_int task)
{
  static const scr_char *const TAKE_WORDS[] = { "get", "take", "pick" };
  static const scr_char *const PUT_WORDS[] = { "drop", "leave", "put" };
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, TRUE);
  scr_bool is_take = FALSE, is_put = FALSE;

  if (run_task_class_filter == 0)
    return TRUE;

  for (const scr_char *pattern : patterns)
    {
      size_t index_;

      if (strcmp (pattern, "*") == 0)
        is_take = is_put = TRUE;
      for (index_ = 0; index_ < 3; index_++)
        {
          if (run_pattern_contains (pattern, TAKE_WORDS[index_]))
            is_take = TRUE;
          if (run_pattern_contains (pattern, PUT_WORDS[index_]))
            is_put = TRUE;
        }
    }

  switch (run_task_class_filter)
    {
    case 1:
      return is_take;
    case 2:
      return is_put;
    default:
      return is_take || is_put;
    }
}


/*
 * run_does_command_match()
 *
 * Non-destructive probe: return TRUE if the input string matches a command of
 * any currently runnable game task (forwards or reverse), without running it.
 *
 * This lets the front end give author-defined commands precedence over its own
 * conveniences.  In particular, the Glk port expands single letters such as
 * "c", "k" and "p" into "close", "attack" and "open"; that silently corrupts
 * games which use single letters as menu choices (battle/conversation menus,
 * e.g. attack choices in hyper_b_s.taf, or "C" in The PK Girl).  The port asks
 * here first, and skips its expansion when the game already recognises the raw
 * input.
 *
 * The string is run through the game's input synonyms before matching, exactly
 * as run_game_task_commands() does with real input.  Without that, a game that
 * routes a letter to its tasks indirectly looks unclaimed: "The Warlord, The
 * Princess & The Bulldog" maps "i" to the synonym "iii" and keys its inventory
 * task on "iii", so probing the raw "i" found nothing and the port expanded it
 * to "inventory" -- announcing "[i -> inventory]" for what the author had
 * already handled.
 *
 * Matching has the same incidental side effects as ordinary command matching
 * (it may set referenced-object/NPC/variable state via uip_match()), but this
 * is harmless: the probe runs before input is submitted, and the real command
 * pass that follows re-matches and overwrites that state.
 *
 * With check_restrictions set, the probe is run400's task pre-matcher
 * Proc_19_35_453C50 (mdlSpreadTheLoad.bas:26153) called with its second
 * argument 1, as the put handler's implicit-take gate calls it @46E2C7.  That
 * routine makes two passes over the tasks, and neither ignores restrictions:
 *
 *   1. @453B00-453C0B: a task in scope for the room whose state allows a run
 *      ((not done Or repeatable) Or (done And reversible)) is a hit only if
 *      restriction_walk Proc_19_64_455C60 PASSES and a pattern matches.
 *   2. Proc_19_68_45404C(1, mode) @453C34: a pattern-matching task in scope
 *      whose lowest failing restriction has a NON-EMPTY FailMessage
 *      (Proc_19_2_481DA0(task, i, 1) records it @481D99; the hit is the
 *      message buffer having changed, @453FA1 and @45403A), or, with no
 *      failing restriction, whose RepeatText (text index 2) is non-empty
 *      @453FE2-454028.  A failing restriction with an empty message drops
 *      the task and the walk moves on to the next one @453FC6->454034.
 *
 * Measured live 2026-09-06 on House.taf (Adrift_91.txt-Adrift_93.txt): at
 * the fireplace, "put wood in fireplace" with the wood on the floor matches
 * task 60 "* %object%" (a take-flagged task restricted to the house
 * spinning, FailMessage empty), and run400 still prints "(Taking the wood
 * first)" -- the restriction-blind probe used to count that task as a hit
 * and skip the take.
 */
scr_bool
run_does_command_match (scr_gameref_t game, const scr_char *string,
                        scr_bool check_restrictions)
{
  scr_int task_count, task, direction;

  /* Only meaningful while a game is actually running. */
  if (!run_is_running (game))
    return FALSE;

  /* Apply input synonyms, so indirection through a synonym still counts. */
  scr_owned_string filtered (pf_filter_input (string, gs_get_bundle (game)));
  if (filtered)
    string = scr_normalize_string (filtered.get ());

  /* Iterate over every task, ignoring those not runnable. */
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      if (!run_task_passes_class_filter (game, task))
        continue;

      if (check_restrictions)
        {
          const scr_char *fail_message;
          scr_bool matched_forwards, matched_reverse, pass;

          /*
           * The Runner's pre-matcher, both passes.  Only the room scope is
           * fixed; the task's state and its restrictions decide together.
           */
          if (!task_where_allows_run (game, task))
            continue;

          matched_forwards = run_match_task_commands (game, task, string,
                                                      TRUE, FALSE);
          matched_reverse = !matched_forwards
                            && task_can_run_task_directional (game, task,
                                                              FALSE)
                            && run_match_task_commands (game, task, string,
                                                        FALSE, FALSE);
          if (!matched_forwards && !matched_reverse)
            continue;

          if (!restr_eval_task_restrictions (game, task,
                                             &pass, &fail_message))
            pass = TRUE, fail_message = NULL;

          if (pass)
            {
              /*
               * First pass: a runnable task whose restrictions pass.  Our
               * state test also admits a spent task with a RepeatText,
               * which is the fallback pass's other hit.
               */
              if (matched_reverse
                  || task_can_run_task_directional (game, task, TRUE))
                return TRUE;
            }
          else if (fail_message)
            {
              /* Fallback pass: the failing restriction has a message. */
              return TRUE;
            }
          continue;
        }

      if (!task_can_run_task (game, task))
        continue;

      /* A match in either direction means the game claims this command. */
      for (direction = 0; direction < 2; direction++)
        {
          const scr_bool is_forwards = !direction;

          if (task_can_run_task_directional (game, task, is_forwards)
              && run_match_task_commands (game, task, string,
                                          is_forwards, FALSE))
            return TRUE;
        }
    }

  return FALSE;
}



/*
 * run_task_run_by_index()
 *
 * Run a task selected by its index rather than by matching input -- the 4.0
 * Runner's Sub_20_22.  Every one of that routine's callers goes through here:
 * an "execute task" action, an event running its TaskAffected, a walk's
 * CharTask or ObjectTask, and the battle system.
 *
 * The reason it is not simply task_run_task() is the preamble: before running
 * the task, run400 walks the task's *alternate* commands looking for a task
 * command function, and a getdynfromroom() found there sets the Referenced
 * Object for the run.  The task's primary command is not in the array it
 * scans, so a task whose only command is the function never evaluates it.
 * Verified live in run400 (RUNNER_TESTS_TODO.md section 9): wrapping each
 * probe in an "execute task" action is the only way to make its function
 * fire at all, and a probe carrying the function as its sole command stays
 * silent.
 */
scr_bool
run_task_run_by_index (scr_gameref_t game, scr_int task)
{
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, TRUE);
  scr_int command;

  for (command = 1; command < (scr_int) patterns.size (); command++)
    {
      if (run_is_task_function (patterns[command], game))
        break;
    }

  return task_run_task (game, task, TRUE);
}


/*
 * run_npc_walk_task()
 *
 * Run the task triggered by an NPC walk meeting a character or an object (a
 * walk's CharTask or ObjectTask).  The 3.9 Runner does not run this one task
 * by its stored index: it copies the task's command text into the input
 * global and calls the task matcher, instruction-for-instruction the same
 * dispatch its events use (Form1.characters at 0005AAD5/0005AB88 vs
 * Form1.checkevent at 00048D83, all three "copy tasks[n-1].command[0], call
 * Form1.tasks(1)"), so everything verified for event dispatch holds here
 * too: the first task in list order that matches and passes where +
 * restrictions fires, an earlier runnable `*` wildcard steals the execution
 * outright, and a restricted match is skipped silently.  Verified live in
 * run390 (test/adrift4/harness/make_39_walkprobe.py variants E/F/G): with a wildcard first
 * the arrival turn prints the wildcard's text twice and the walk task's
 * never, with the walk task first it fires itself, and a restricted walk
 * task prints nothing.  The matcher dispatch is also what fans one walk
 * trigger out across same-command tasks -- e.g. "Lair of the CyberCow" has
 * two "#lured" tasks (steeple and chapel yard) and the fairy snatches the
 * milk bowl in whichever of those rooms the player is standing.
 *
 * The 4.0 Runner instead runs the task directly by index -- its walk handler
 * calls the same direct task runner (Sub_20_22) as its events -- so there is
 * no interception, and a failing restriction prints its FailMessage (which
 * task_run_task does).  Verified live in run400 (test/adrift4/harness/make_400_walkprobe.py
 * variants E/G): the walk task fires with a wildcard listed before it, and
 * a restricted walk task prints its FailMessage on every arrival turn.
 */
static void run_task_command_dispatch (scr_gameref_t game, scr_int eventtask);


void
run_npc_walk_task (scr_gameref_t game, scr_int walktask)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (run_get_version (bundle) < TAF_VERSION_400)
    run_task_command_dispatch (game, walktask);
  else if (task_can_run_task_directional (game, walktask, TRUE))
    run_task_run_by_index (game, walktask);
}


/*
 * run_event_task()
 *
 * Run the task executed by a finishing event (its TaskAffected, with
 * "task finished" unset) in a pre-4.0 game.  The reference Runner does not
 * run this task by its stored index: it submits the task's command
 * text through the task matcher, and the first task in list order that
 * matches the text -- a `*` wildcard matches it like any other input -- and
 * whose "where" and restrictions pass is the one that fires.  So a runnable
 * wildcard task earlier in the list steals the event's execution outright
 * (its text prints instead, the affected task does not run), and the theft
 * happens even when the affected task itself could not run where the player
 * is standing.  Restricted matches are passed over silently.
 *
 * All of this is verified against the live 3.9 Runner (see
 * RUNNER_TESTS_TODO.md section 2): "thetest" depends on the stealing -- its
 * "Nice try fish face!" `*` task fires on the same turn as the library drop
 * because an always-restarting one-turn event executes a task every turn,
 * and the author's ALRs splice the two messages -- while a probe with the
 * wildcard placed after the affected task shows list order deciding the
 * winner, and a probe without any event shows no same-turn firing at all.
 *
 * The literal-text comparison below backstops the pattern matcher for the
 * customary un-typeable "#name" commands, which never survive the player
 * input path; the matcher is still consulted so wildcard patterns match.
 *
 * run_task_command_dispatch() is the dispatch itself, shared with the 3.9
 * walk CharTask/ObjectTask path above (in the 3.9 Runner both are the same
 * P-code sequence); run_event_task() is the event-facing name.
 */
static void
run_task_command_dispatch (scr_gameref_t game, scr_int eventtask)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  const scr_char *command;
  scr_int task_count, task;

  /* Get the task's first command pattern; nothing to match if absent. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = eventtask;
  vt_key[2].string = "Command";
  vt_key[3].integer = 0;
  command = prop_get_string (bundle, "S<-sisi", vt_key);
  if (scr_strempty (command))
    {
      /* No command text to dispatch; run the task directly. */
      if (task_can_run_task_directional (game, eventtask, TRUE)
          && run_task_is_unrestricted (game, eventtask))
        task_run_task (game, eventtask, TRUE);
      return;
    }

  /* Run the first task in list order the command text matches. */
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      scr_bool is_matched;

      if (!task_can_run_task (game, task)
          || !task_can_run_task_directional (game, task, TRUE))
        continue;

      if (task == eventtask)
        is_matched = TRUE;
      else
        {
          const scr_char *other;

          is_matched = run_match_task_commands (game, task, command,
                                                TRUE, FALSE);
          if (!is_matched)
            {
              vt_key[1].integer = task;
              other = prop_get_string (bundle, "S<-sisi", vt_key);
              vt_key[1].integer = eventtask;
              is_matched = scr_strcasecmp (command, other) == 0;
            }
        }
      if (!is_matched)
        continue;

      if (run_task_is_unrestricted (game, task))
        {
#ifdef SCARIER_DUMP_TOOLS
          {
            static const scr_bool trace_evtask =
                getenv ("SCR_TRACE_EVENT_TASK") != NULL;
            if (trace_evtask && task != eventtask)
              fprintf (stderr, "EVTASK steal: task=%ld stole [%s] from"
                       " task=%ld\n", task, command, eventtask);
          }
#endif
          task_run_task (game, task, TRUE);
          return;
        }
    }

#ifdef SCARIER_DUMP_TOOLS
  {
    static const scr_bool trace_evtask =
        getenv ("SCR_TRACE_EVENT_TASK") != NULL;
    if (trace_evtask)
      fprintf (stderr, "EVTASK no-run: [%s] task=%ld candone=%d dir=%d\n",
               command, eventtask,
               (int) task_can_run_task (game, eventtask),
               (int) task_can_run_task_directional (game, eventtask, TRUE));
  }
#endif
}

void
run_event_task (scr_gameref_t game, scr_int eventtask)
{
  run_task_command_dispatch (game, eventtask);
}


/*
 * run_defer_loud_tasks_to_movement()
 *
 * In version 3.8 (and so also 3.7) a task whose command matches but whose
 * restrictions fail does NOT get to swallow a direction the player can
 * actually walk in: the movement happens, and the task's fail message is
 * never printed.  Verified live in run380 with "The Twilight" (2026-08-04):
 * its task 6 is "w" at the Cliff Top, restricted to Gale being present, and
 * before she joins you the Runner answers a bare "w" with "You move west."
 * and no message at all.  Under the version 4.0 ordering the message wins
 * instead, which strands the player on the very first move of that game --
 * and again later, since it also blocks the attic with "d"/"u" tasks
 * restricted to the Sentinel and the apparition being present.
 *
 * This is specific to movement.  Other standard commands still lose to the
 * message: in the same session "ask gale about mansion" printed task 4's
 * "You can't do that in your present company." even though the Runner has a
 * perfectly good answer of its own for asking an absent character (it says
 * "You can't talk to that." when no task matches at all).  So all this does
 * is let a *successful* move jump the queue.  A move that would be refused
 * changes nothing: the loud task still gets its say, and the refusal is
 * printed afterwards by run_standard_commands() if no task claims the input.
 */
static scr_bool
run_defer_loud_tasks_to_movement (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (run_get_version (bundle) > TAF_VERSION_380)
    return FALSE;

  return run_movement_succeeds (game, string);
}


/*
 * run_task_has_catchall_command()
 *
 * TRUE if any of the task's forward command patterns is a bare "*".
 *
 * Such a task matches every line the player types, and the Runner treats it as
 * a deliberate catch-all rather than as something the player got into the wrong
 * room for: run390's checktask sets the room flag for an out-of-room match and
 * then walks the task's own 25 command slots, clearing it again the moment one
 * of them is exactly "*" (loc_44B684 sets it, loc_44B6BC clears it).  Only the
 * forward Command list is walked, not ReverseCommand.
 */
static scr_bool
run_task_has_catchall_command (scr_gameref_t game, scr_int task)
{
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, TRUE);

  for (const scr_char *pattern : patterns)
    {
      if (strcmp (pattern, "*") == 0)
        return TRUE;
    }
  return FALSE;
}

/*
 * run_task_command_is_literal()
 *
 * TRUE if every forward command pattern of the task is a plain literal -- no
 * `*` wildcard anywhere in it.
 *
 * This is what separates the already-done refusal that beats the standard
 * library from the one that does not.  chicago.taf's `listen` (task 18,
 * cmd=[listen]) is a bare literal and run390 answers the second typing with
 * "You have already done that." rather than the library's "You hear nothing
 * out of the ordinary."  Every corpus row that a blanket pre-library pass
 * broke is the other shape: circus task 77 is `ask* barb* *tape`, and in
 * inverness, journ2, vampire, merry_murders and mr_smith the hijacked
 * commands are movement and ordinary library verbs swallowed by wildcard
 * patterns -- eight walkthroughs, several of them stopping winning.
 *
 * That is the same line RUNNER_TESTS_TODO already draws twice: a done
 * wildcard task claiming every later command is the inverness soft-lock we
 * deliberately do not import, while the narrow exact-command case is real and
 * ported.  Ordering is simply the other half of that distinction -- the narrow
 * case outranks the library, the broad one does not.
 */
static scr_bool
run_task_command_is_literal (scr_gameref_t game, scr_int task)
{
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, TRUE);

  for (const scr_char *pattern : patterns)
    {
      if (strchr (pattern, '*'))
        return FALSE;
    }
  return !patterns.empty ();
}

/*
 * run_input_is_movement()
 *
 * TRUE if the input is one of the compass words, whether or not the move would
 * succeed.  Runs no handler and prints nothing.
 */
static scr_bool
run_input_is_movement (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  scr_commandsref_t command = run_move_commands (gs_get_bundle (game));

  for (; command->command; command++)
    {
      if (uip_match (command->command, string, game))
        return TRUE;
    }
  return FALSE;
}

/*
 * run_task_refusal()
 *
 * The Runners have two answers of their own for a command that matches a task
 * the main dispatcher would not run, both of which Scarier used to leave to the
 * standard library or to the game's DontUnderstand text:
 *
 *   - the player is in the wrong room for the task.  Pre-4.0 only: 3.7 and 3.8
 *     answer "You can't do that here.", 3.9 "You can't do that here!", and 4.0
 *     dropped the message (the string is still in run400.exe, unused).
 *   - the task has already been done and is not repeatable.  The Runner prints
 *     the task's RepeatText if it has one -- in EVERY version, 4.0 included --
 *     and otherwise "You have already done that.", which like the room refusal
 *     is a pre-4.0 message only (` have already done that.` is a UTF-16 literal
 *     in run370/run380/run390 and absent from run400).
 *
 * Measured live against run370 (Castle Quest), run380 (Marooned), run390 (The
 * Hangover plus the p39where probe built by test/adrift4/harness/
 * make_39_whereprobe.py) and run400 (make_400_whereprobe.py), 2026-08-09/10.
 *
 * The conditions are narrow, and the probes say how narrow.  A task refused by
 * a *restriction* that fails silently gets "I don't understand.", as does a
 * command that matches no task pattern at all.  Runner P-code guards the room
 * message with `OUT = "" And FLAG = 1`, so any output at all -- including a
 * standard library answer -- suppresses it, which is why this runs last, only
 * when nothing else claimed the input.
 *
 * The two refusals are ordered, and the order is measured rather than assumed:
 * probe task "theta" is done AND out of its room, and run390 answers "You can't
 * do that here!", so the room half is tested first.  That is why the
 * already-done test carries the room condition with it (task_is_done_refused()).
 *
 * Within one command the Runner scans every task and does not stop at the first
 * refusable one.  The already-done half writes its message as it goes, so the
 * first such task wins and ends the scan; the room half only raises a flag, and
 * each later out-of-room match overwrites it -- so it is the LAST matching
 * out-of-room task that decides, and a catch-all "*" task clears the flag for
 * good (run_task_has_catchall_command()).  Melbourne Beach (3.90) is the
 * measured case: its task 94 is `*` confined to room 0, so run390 answers "I
 * don't understand what you mean!" and not "You can't do that here!" for `play
 * volleyball` and `use shower` typed outside their rooms
 * (Adrift_37_melbourne_beach.txt).
 *
 * Both count as a turn, unlike DontUnderstand: in the probe, an event ticking
 * once a turn fires on either refusal and not on the parser complaint, matching
 * the `handled = 1` the Runner sets alongside the message.  Hence the TRUE
 * return, which lets the caller run the turn.  (Measured for all three pre-4.0
 * answers, and for 4.0's RepeatText on 2026-09-08: p4REPEAT2/p4REPEAT3 carry a
 * once-a-turn event and every RepeatText line in Adrift_951/952 is followed by
 * its "TICK.".)
 *
 * The leading word follows Perspective, which pre-4.0 has only two of: run390
 * answers "I can't do that here!" / "I have already done that." for Perspective
 * 0 and the "You" forms for every other value, third person being a 4.0
 * addition (its inventory says "You are carrying nothing." for Perspective 2 as
 * well).
 */
enum { REFUSAL_NONE = 0, REFUSAL_ROOM, REFUSAL_DONE };

/*
 * Which of the three calls a scan is: the pass ahead of the standard library
 * (which answers the already-done half), the pass after it (the room half),
 * and a silent look-ahead that only reports whether the pre-library pass has
 * something to say.  run_all_commands() needs the answer before it runs the
 * priority commands, because 4.0's RepeatText outranks those as well.
 */
enum run_refusal_pass_t
{
  REFUSAL_PASS_PRE = 0,
  REFUSAL_PASS_POST,
  REFUSAL_PASS_PROBE
};

/*
 * The two halves do NOT sit at the same point in the dispatch order, and
 * chicago.taf (3.90) is the measurement that separates them.  Its task 18 is
 * `listen`, confined to rooms 2 and 8 and not repeatable, and the walkthrough
 * types `listen` twice inside those rooms.  The second time run390 answers
 * "You have already done that." -- not the library's "You hear nothing out of
 * the ordinary.", which run390 certainly has (` hear nothing out of the
 * ordinary.` is a UTF-16 literal in all four Runners, so this is not a
 * vocabulary difference).  The already-done message therefore beats the
 * standard library, exactly as the P-code shape says: the done half writes its
 * message during the task scan, while the room half only raises a flag that
 * `OUT = "" And FLAG = 1` later discards if anything else printed.
 *
 * So the done half runs BEFORE run_standard_commands() and the room half after
 * it, and `done_only` selects which.  The scan itself is identical in both
 * passes, because the room half still has to be tested first WITHIN a pass --
 * probe task "theta" is done AND out of its room and run390 answers the room
 * refusal.  The pre-library pass simply declines to emit a room refusal and
 * leaves it to the post-library one.
 *
 * 4.0 has no already-done message at all, only RepeatText, and it sits ahead
 * of the library too -- ahead of MORE of it, in fact.  Measured 2026-09-08
 * with the p4REPEAT probes: the dispatcher prints the RepeatText and jumps
 * past every general verb, movement and take included, so the 4.0 pre-library
 * pass carries none of the three pre-4.0 conditions.  Only the handlers that
 * run before the dispatcher, or below the jump, keep their answer; they are
 * listed in run_repeat_survivor_400(), and the caller checks them.
 */
static scr_bool
run_task_refusal (scr_gameref_t game, const scr_char *string,
                  run_refusal_pass_t pass)
{
  const scr_bool done_only = pass != REFUSAL_PASS_POST;

  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int version, perspective, task_count, task, direction;
  scr_int refusal, refused_task;
  scr_bool is_room_refused;
  const scr_char *repeattext;

  /*
   * An empty input line element is not a command and gets no complaint of any
   * kind -- the same guard the DontUnderstand fallback uses.  Without it a
   * game with a bare "*" task command outside the player's room turns every
   * press-a-key blank line into a refusal.
   */
  if (scr_strempty (string))
    return FALSE;

  /*
   * A line that has already run a task gets no refusal.  In the Runner the
   * RepeatText is not a late pass at all: it is printed inside the task
   * dispatcher (Proc_19_24_44CCE0, called once at 48A481), which asks the
   * pre-matcher Proc_19_66_454EF0 for exactly ONE task and then either runs
   * it, reverses it, or prints its RepeatText.  So the task that refuses and
   * the task that runs are always the same one, and a line that ran a task
   * can never also be refused -- the restriction-failure pass at 44CCA5 is
   * likewise entered only when no task was found.
   *
   * Measured on easter.taf (run400 Adrift_273_easter.txt:304-308): the
   * winning `show basket to shopkeeper` runs task 63, which is silent and
   * only ends the game, and run400 prints nothing before the WinText -- not
   * the "Since you already have Max's list..." RepeatText that a scan over
   * the other matching tasks turns up here.
   */
  if (run_any_task_ran_this_command ())
    return FALSE;

  version = run_get_version (bundle);

  /*
   * Walk every task, looking for ones whose command matches the input and that
   * the dispatcher passed over for one of the two refusable reasons.  A task
   * blocked by anything else can never raise a refusal.  The already-done half
   * stops the scan where it fires; the room half keeps going, last one wins.
   */
  refusal = REFUSAL_NONE;
  refused_task = -1;
  is_room_refused = FALSE;
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      /* The room half first -- see the note above on probe task "theta". */
      if (version < TAF_VERSION_400)
        {
          for (direction = 0; direction < 2; direction++)
            {
              const scr_bool is_forwards = !direction;

              if (task_is_room_refused (game, task, is_forwards)
                  && run_match_task_commands (game, task, string,
                                              is_forwards, FALSE))
                {
                  is_room_refused =
                      !run_task_has_catchall_command (game, task);
                  break;
                }
            }
        }

      /*
       * A task the current command has just completed is not "already done"
       * for that command -- p39done.taf's silent `* x * scroll *` task runs,
       * prints nothing, and run390 then answers "I don't understand." rather
       * than "You have already done that." (Adrift_18.txt 2026-08-23).
       */
      /*
       * 4.0 also asks the restrictions.  A spent task whose restrictions now
       * fail is not the one the picker hands the dispatcher, and the library
       * gets the line as if the task were not there at all: `A Witch Tale`
       * task 2 is the literal `* north` at the bridge, spent from the first
       * crossing attempt and carrying a RepeatText, but restricted on `say
       * grue` being undone -- once the riddle is answered run400 walks the
       * player north (Adrift_128_witchtale.txt) instead of printing "I try
       * to cross the bridge...".  Only silent failures are covered here; a
       * fail message of its own is the restriction pass's business, not this
       * one's.  Pre-4.0 keeps the shape chicago.taf measured, and so does the
       * post-library pass, which is a fallback rather than a model of the
       * dispatcher: `The Magic Show` reaches its "One rabbit trick is enough
       * for any given act" through a spent task whose restrictions fail, with
       * the library having declined the line first.
       */
      if (!run_task_ran_this_command (task)
          && task_is_done_refused (game, task)
          && (version < TAF_VERSION_400 || !done_only
              || run_task_is_unrestricted (game, task))
          && run_match_task_commands (game, task, string, TRUE, FALSE))
        {
          refusal = REFUSAL_DONE;
          refused_task = task;
          break;
        }
    }
  if (refusal == REFUSAL_NONE && is_room_refused)
    refusal = REFUSAL_ROOM;
  if (refusal == REFUSAL_NONE)
    return FALSE;

  /*
   * The pre-library pass answers the already-done half and nothing else; a
   * room refusal found here waits for the post-library pass, where the
   * Runner's own "did anything print?" guard applies to it.
   */
  if (done_only)
    {
      const scr_char *repeat;

      /*
       * Three conditions, and all three are the bounds of what chicago.taf
       * actually measured rather than rules proved in their own right:
       *
       *   - the refusal has to be the DONE half (the room half is guarded by
       *     the Runner's own "did anything print?" test and stays late);
       *   - the task's command must be a plain literal, or a done wildcard
       *     task starts claiming movement and library verbs wholesale -- the
       *     inverness soft-lock we already refuse to import;
       *   - it must be the DEFAULT " have already done that." message, not an
       *     authored RepeatText.  Different Runner paths: the default is
       *     substituted into the game's message slot at LOAD (run390 openadv,
       *     loc_465A8B..loc_465AB9, when the loaded string is empty), while
       *     RepeatText is a per-task string that 4.0 kept.  `lair-of-the-
       *     cybercow.taf` task 80 (`complete robot`/`fix robot`, literal, with
       *     a RepeatText) keeps answering with the library's "I don't think
       *     you can fix the robot.", so RepeatText does not outrank it.
       *
       * Movement is exempt on top of that: `Vampire.taf` task 61 is the
       * literal `east` in room 11, done and non-repeatable, and the Runner
       * still moves the player east on the next `e`.  The original 2026-08-10
       * probe note said the same thing from the other side -- movement is
       * answered first.
       */
      if (refusal != REFUSAL_DONE)
        return FALSE;

      repeat = prop_get_indexed_string (bundle, "Tasks", refused_task,
                                        "RepeatText");

      /*
       * 4.0 has no default already-done message and none of those three
       * conditions: measured 2026-09-08 (p4REPEAT/p4REPEAT2/p4REPEAT3 under
       * run400, Adrift_950-952), an authored RepeatText is printed by the
       * task dispatcher itself, ahead of nearly the whole library, and takes
       * the line away from a wildcard command and from movement just as
       * readily as from a literal one.  What it does NOT take is listed in
       * run_repeat_survivor_400(), which the caller tests -- the answer is
       * needed before the priority commands run, so a probe pass reports it
       * without printing anything.
       */
      if (version >= TAF_VERSION_400)
        {
          if (scr_strempty (repeat))
            return FALSE;
          if (pass == REFUSAL_PASS_PROBE)
            return TRUE;
        }
      else if (!run_task_command_is_literal (game, refused_task)
               || !scr_strempty (repeat)
               || run_input_is_movement (game, string))
        return FALSE;
    }

  /*
   * An authored RepeatText replaces the already-done message, and is the one
   * part of all this that 4.0 kept.
   */
  repeattext = NULL;
  if (refusal == REFUSAL_DONE)
    {
      repeattext = prop_get_indexed_string (bundle, "Tasks", refused_task,
                                            "RepeatText");
      if (scr_strempty (repeattext))
        {
          repeattext = NULL;
          if (version >= TAF_VERSION_400)
            return FALSE;
        }
    }

  if (repeattext)
    {
      pf_buffer_paragraph_line (filter, repeattext);
      return TRUE;
    }

  perspective = prop_get_global_integer (bundle, "Perspective");

  pf_buffer_string (filter,
                    perspective == LIB_FIRST_PERSON ? "I" : "You");
  if (refusal == REFUSAL_ROOM)
    {
      pf_buffer_paragraph_line (filter,
                                version < TAF_VERSION_390
                                ? " can't do that here."
                                : " can't do that here!");
    }
  else
    pf_buffer_paragraph_line (filter, " have already done that.");
  return TRUE;
}


/*
 * run_all_commands()
 * run_game_task_commands()
 *
 * Alternative facets of run_game_commands_common().  The first is used by the
 * main user input handling loop; the latter by the library when looking for
 * game commands that override standard actions.
 */
static scr_bool
run_all_commands (scr_gameref_t game, const scr_char *string)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_bool status, ask_echo, put_first, refused;
  scr_bool repeat_found, repeat_pending, inv_listed;
  const scr_char *task_string;
  std::string fragment;
  scr_int prior_npc;

  /*
   * Adrift command matching is just weird, perhaps broken.  In theory, a
   * game can override system commands with a properly constructed task and
   * set of command matchers.  However, the Runner isn't terribly consistent
   * in when this will work and when not, and some games rely on that in-
   * consistency.  In particular, a game with a "* object" task that has
   * failing restrictions will not be able to override the system's "take
   * object", whereas a game's "take object", under the same circumstances,
   * will.  Yet if the restrictions pass, a game's "* object" overrides the
   * system's "take object" with no apparent difficulty.
   *
   * For example, "The Woods Are Dark" has a "* ball *" task with the
   * restriction "must be holding ball".  Without special casing it, there's
   * no way to get the ball in the first place.
   *
   * Trying to find the right way to do things here, then, has been tricky.
   * Here's the current process:  First, try "priority" system commands; ones
   * that move objects to inventory.  These system commands will call back
   * into trying game commands for objects taken or dropped, and in those
   * tries, allow overrides only if the game task is explicit about what it's
   * doing -- it doesn't start with "*", or it does but explicitly names a
   * verb (see run_match_task_commands() for the run400 probe results behind
   * that rule) -- and handle restrictions in those tries.  Next, run game
   * commands directly, ignoring any cases where restrictions fail to let the
   * task run.  After that, retry all game commands again with restrictions
   * enabled.  And finally, try all other standard library commands.
   *
   * Priority commands go BEFORE the direct game-command pass (not after, as
   * an earlier version of this had it): probe DONE, task 72 in "Space Boy's
   * First Adventure" is the literal, unrestricted, textless command "drop
   * cape to the floor" (+250 score, no message).  run400 answers the typed
   * command with the library's ordinary "Player drop the cape." and the
   * score UNCHANGED (Adrift_8_pET2.txt/Adrift_10_pET4.txt, 2026-08-23) -- the task
   * never runs at all, even though its own literal pattern matches the raw
   * input exactly.  A same-shaped task using a bare object with no trailing
   * words ("* drop * rock *", i.e. equivalent to the library's own
   * canonical "drop <object>" callback string) DOES run silently alongside
   * the library's message (Adrift_2_pET.txt).  So it is specifically the
   * priority command's own callback -- matching only its constructed
   * "verb OBJECT" short form, not the raw typed line -- that gets first
   * refusal on a recognised system verb; a task whose pattern extends past
   * that short form (trailing words the library only swallows as free
   * %text%) is never reached once the library has already claimed and
   * finished the command.  Swapping the two passes reproduces that: on a
   * recognised system verb, run_priority_commands() (and its short-form
   * task callback) gets the first and often the only look, and the direct,
   * ignore-restrictions task pass only sees what the priority commands
   * didn't recognise as a system verb at all (so "etcontrol"-style
   * non-system task commands are unaffected).
   *
   * But that swap alone regresses the same game's task 27, "{take/get}
   * {them/boots}" (CompleteText "Taken."): the callback's constructed short
   * form is built from the object's display Short name ("get pair of Flight
   * Boots"), never from an alias like "boots", so the callback attempt fails
   * for a reason unrelated to task 72's "trailing words" problem, and
   * priority's own generic take now wins a task the golden walkthrough (and,
   * definitionally, the pre-swap Runner-matching behaviour this port has
   * always had) says must win instead.  (The bare swap is not a near miss:
   * it fails 75 of the 262 corpus walkthroughs, because scarier's callback
   * string is weaker than the Runner's -- Short name only, no aliases -- so
   * far more tasks are unreachable here than there.)  So a peek at the direct
   * task pass runs BEFORE priority after all, and task 72's shape is excluded
   * from it: silent (no CompleteText, ShowRoomDesc, AdditionalMessage or
   * status-setting action) AND unreachable from any library short form for
   * any object ("drop cape" / "drop the cape" can never satisfy a pattern
   * that insists on "to the floor").  Both halves are needed -- silence alone
   * loses "Sommeril" task 35 and 3 more walkthroughs, unreachability alone
   * loses 27 of them.
   *
   * Excluding a task means abandoning the ENTIRE peek, not reading past it to
   * the next task.  ADRIFT resolves a command to the lowest-indexed matching
   * task, so stepping over a match to let a later task win invents a Runner
   * behaviour that does not exist: "The Forum" task 1 (silent literal "x ...
   * hand", which falls through to the library's own examine) was being
   * skipped in favour of task 2, "[examine/read/x/l/look]{at}[%object%]",
   * whose CompleteText then answered every examine in the game.  That cost
   * 8 corpus walkthroughs (forum, forum2, cursed, iqsfot, sommeril, funhouse
   * and both to_hell_and_beyond replays) until the peek was made all-or-
   * nothing.  On abandonment priority gets its look next, and if it does not
   * claim the command the immediately following unexcluded pass re-matches
   * from task 0, unchanged from the original order.
   */
  /*
   * The carrying-capacity accounting toggle is exposed as a Glk port command
   * ("glk capacity"), handled in the front end before input ever reaches the
   * interpreter, so there is no administrative meta-command to match here.
   */
  run_dispatch_input = string;
#ifdef SCARIER_DUMP_TOOLS
  run_trace_last_input = string;
#endif
  run_co_pending_input = string;
  run_co_task_claimed = FALSE;
  run_tasks_ran_this_command.assign (gs_task_count (game), FALSE);
  /*
   * The "(<npc>)" an "ask about" / "talk about" echoes comes out ahead of
   * everything, task matching included; see uip_print_ask_echo().  The
   * characters this line names are noted here too: run400 notes them inside
   * characters(), which every line reaches (48B56E is below the task
   * dispatch's exits), so a task-answered line names its characters just
   * the same.  Both read the register before the noting, so remember what
   * it held for the rewrite further down.
   */
  prior_npc = game->last_npc;
  ask_echo = uip_print_ask_echo (game, string);
  uip_note_named_npcs (game, string);

  /*
   * 4.0 puts are the exception to the peek: the library's put-in / put-on
   * gets the line BEFORE any task does, and a task only ever answers a put
   * the library could not complete.  Measured with the PUT4-PUT7 arena
   * probes (Adrift_81-87, 2026-09-05), each pairing a passing put task with
   * reporter tasks restricted on "object is inside container": run400 puts
   * the pill in the cup and the task never runs, whatever its spelling --
   * wildcard, prefixed or literal -- while run390 on the 3.9 twin (put39.taf,
   * Adrift_88) hands the line to the task and moves nothing.  Read off
   * run400's insides handler (Proc_19_43_46639C): a completed move returns 1
   * and the dispatcher stops there.  The peek pass would have handed the
   * line to a non-silent matching task first, which is what cost "Sommeril"
   * its `put fish in fountain` (task 18's text where the Runner puts the
   * fish, Adrift_78) and left `take wet page` answered on the next turn
   * where run400 says "Take what?".  Pre-4.0 keeps the peek order: Scarier
   * matches run390 on the put39 probe as it stands.
   *
   * A put the 4.0 library REFUSES on size or capacity is different again:
   * every message path of that handler exits without setting its return
   * byte, so the refusal is printed but the line is not claimed, and the
   * task passes then answer it as well, run straight on from the refusal
   * with the two-space separator -- "The rock is too big to fit inside the
   * slot.  PUTBIG." (PUT7, Adrift_87; Zack Smackfoot's `put knife in slot`,
   * Adrift_57).  The handler signals that with run_priority_refuse(); the
   * join is left pending so that a refusal no task follows stands alone.
   * Either way the line is handled once the refusal is printed, so the
   * standard table's duplicate put rows never print it a second time.
   */
  /*
   * A 4.0 line that a spent task answers with its RepeatText is not the
   * priority commands' either -- run400 dispatches tasks at 48A481, above
   * everything but inventory (48A457) and the put/drop list (48A462), so
   * `take coin` on a done `take coin` task prints the RepeatText and takes
   * nothing, and `put all on desk` on a done one prints it rather than the
   * put's own empty-handed answer.  The message itself waits until the task
   * passes below have all declined, because a task that can still RUN
   * outranks one that is merely spent; all that is needed here is whether
   * there IS one, so that the priority commands stand aside.
   * run_repeat_survivor_400() holds the handlers that keep the line anyway;
   * they are still a turn, though, because the dispatcher sets its handled
   * byte before the character pass overwrites the message (`x bob` on a
   * spent task ticks the probe's event, where a plain NPC examine does not
   * -- Adrift_951, and see run_note_repeat_survivor_turn below).
   */
  repeat_found = run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
                 && run_task_refusal (game, string, REFUSAL_PASS_PROBE);
  repeat_pending = repeat_found && !run_repeat_survivor_400 (game, string);

  put_first = run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
              && !repeat_pending
              && run_is_put_command (game, string);
  status = FALSE;
  refused = FALSE;
  /*
   * The inventory listing at 48A457 comes out ahead of the dispatcher too,
   * but unlike the put/drop rows it does not take the line away from the
   * task: 48A481 runs immediately afterwards and appends the CompleteText to
   * the listing.  So print it here and leave `status` alone, letting the
   * task passes below run on the same line; the listing claims whatever they
   * decline.  See run_is_inventory_command().
   */
  inv_listed = run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
               && run_is_inventory_command (game, string)
               && run_priority_commands (game, string);
  if (put_first)
    {
      status = run_priority_commands (game, string);
      refused = !status && run_priority_refused;
      if (refused)
        {
          pf_note_trailing_auto_break (filter);
          pf_buffer_join_pending (filter);
        }
    }
  /*
   * A 4.0 put-in whose direct object named nothing has already rewritten the
   * command line the tasks are dispatched against, and the Runner never puts
   * it back -- run_priority_unnamed_put_object() has the whole of it.  The
   * task passes get the fragment; the standard table, further down, still
   * gets the line as typed, because the catch-all's noun was resolved from
   * that before put_drop_list ever ran.
   */
  task_string = string;
  if (put_first && !status && !refused && run_priority_unnamed_put
      && run_unnamed_put_fragment (string, fragment))
    task_string = fragment.c_str ();

  if (!status && !refused)
    status = run_game_commands_in_parser_context (game, task_string,
                                                  FALSE, TRUE);
  if (!status && !put_first && !inv_listed && !repeat_pending)
    status = run_priority_commands (game, string);
  if (!status)
    status = run_game_commands_in_parser_context (game, task_string,
                                                  FALSE, FALSE);
  if (!status && !inv_listed
      && !run_defer_loud_tasks_to_movement (game, task_string))
    status = run_game_commands_in_parser_context (game, task_string,
                                                  TRUE, FALSE);
  if (refused)
    {
      pf_clear_join_pending (filter);
      status = TRUE;
    }
  if (inv_listed)
    status = TRUE;
  if (!status)
    {
      /*
       * Only now, with every task pass declined, does the Runner rewrite a
       * "give X" with no "to", or an "ask about" / "talk about", around the
       * last character a library command named (uip_rewrite_references()).
       * The order matters: run400's input routine dispatches typed-command
       * tasks at 48A481 (Proc_19_24_44CCE0), well before the give rewrite at
       * loc_48A98A and the Proc_19_0_480674 call at 48B56E that holds the
       * ask/talk rewrite, and a matched task jumps past both (GoTo 48B4E3).
       * So "Sommeril"'s literal task "ask about glass framed page" keeps
       * answering even with the Gargoyle as the last-named character;
       * rewriting first turned it into "ask gargoyle about ..." and lost the
       * task to the library's generic reply.  Its "(GARGOYLE)" is printed
       * all the same, up at the top of this routine -- only the rewritten
       * STRING is the library's; see uip_print_ask_echo().
       *
       * The Runner records the characters a line names inside Proc_19_0
       * (loc_47F3A2) as well, after both rewrites -- so a rewrite always
       * sees the register as the previous command left it.  That noting is
       * done up at the top of this routine now, because characters() runs
       * on every line; see uip_note_named_npcs().
       */
      scr_owned_string rewritten (uip_rewrite_references (game, string,
                                                         prior_npc, ask_echo));
      const scr_char *const library_string =
          rewritten ? rewritten.get () : string;
      run_dispatch_input = library_string;
      /*
       * Pre-4.0 the already-done refusal outranks the standard library; see
       * the note on run_task_refusal().  The room half still runs after it.
       */
      if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400)
        status = run_task_refusal (game, library_string, REFUSAL_PASS_PRE);
      else if (repeat_pending)
        {
          /*
           * Matched on the line as typed, the way the probe above did and
           * the way run400's dispatcher does: the give and ask/talk rewrites
           * below loc_48A98A are further down the routine than 48A481.
           */
          status = run_task_refusal (game, string, REFUSAL_PASS_PRE);
        }
      if (!status)
        status = run_standard_commands (game, library_string);
      if (!status)
        status = run_task_refusal (game, library_string, REFUSAL_PASS_POST);
    }
  /*
   * A survivor answered a line the dispatcher had already claimed: the
   * message is the library's but the turn is the refusal's, so an answer
   * that is normally administrative counts here.  run400 prints the
   * RepeatText at 48A481 and jumps to loc_48B4E3, which is below the stores
   * that would mark the line administrative and above the character pass at
   * 48B56E -- so the NPC examine's own text comes out and the walk and event
   * tick at 48B599 still runs.
   */
  if (status && repeat_found && !repeat_pending)
    game->is_admin = FALSE;

  run_dispatch_input = NULL;
  run_tasks_ran_this_command.clear ();

  return status;
}

scr_bool
run_game_task_commands (scr_gameref_t game, const scr_char *string)
{
  /*
   * Try game commands, and note that this is a library call so that the parse
   * matcher can exclude game commands that begin with a '*' wildcard.
   *
   * Restrictions are honoured -- meaning a task whose restrictions fail with a
   * message gets run for the message, beating the library action -- from 4.0
   * on only.  Probe DONE, task `* get * gem *` restricted to "holding the
   * stone", measured 2026-08-23: run400 answers `get gem` without the stone
   * with "BLOCK-GEM." (Adrift_20.txt), while run390 on the 3.9 twin probe
   * quietly takes the gem instead (Adrift_18.txt).  Pre-4.0 the library wins,
   * so the loud pass is switched off here rather than being allowed to claim
   * the command.  Tasks whose restrictions PASS still run in either version --
   * the first loop below does not consult this flag.
   */
  const scr_bool include_restrictions =
      run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400;

#ifdef SCARIER_DUMP_TOOLS
  if (getenv ("SCR_TRACE_MATCH"))
    fprintf (stderr, "DISPATCH input=[%s]\n", string);
#endif

  return run_game_commands_common (game, string, include_restrictions, TRUE,
                                   FALSE);
}


/*
 * run_player_input()
 *
 * Take a line of player input and buffer it.  Split the line into elements
 * separated by periods.  For the first element, try to match it to either a
 * task or a standard command, and return TRUE if it matched, FALSE otherwise.
 *
 * On subsequent calls, successively work with the next line element until
 * none remain.  In this case, prompt for more player input and continue as
 * above.
 *
 * For the case of "again" or "g", rerun the last successful command element.
 *
 * One extra special special case; if called with a game that is not running,
 * this is a signal to reset all noted line input to initial conditions, and
 * just return.  Sorry about the ugliness.
 */
static scr_bool
run_player_input (scr_gameref_t game)
{
  static scr_char line_buffer[LINE_BUFFER_SIZE];
  static scr_char prior_element[LINE_BUFFER_SIZE];
  static scr_char line_element[LINE_BUFFER_SIZE];

  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_memo_setref_t memento = gs_get_memento (game);
  scr_bool is_rerunning, was_undo_available, status;
  const scr_char *command;

  /* Special case; reset statics if the game isn't running. */
  if (!game->is_running)
    {
      memset (line_buffer, NUL, sizeof (line_buffer));
      memset (prior_element, NUL, sizeof (prior_element));
      memset (line_element, NUL, sizeof (line_element));
      lib_co_400_reset ();
      return TRUE;
    }

  /*
   * Save the settings of the game's do_again and undo_available flags for
   * later checks.
   */
  is_rerunning = game->do_again;
  was_undo_available = game->undo_available;

  /* See if the player asked to rerun a command element. */
  if (game->do_again)
    {
      game->do_again = FALSE;

      /* Check there is a last element to repeat. */
      if (prior_element[0] == NUL)
        {
          pf_buffer_string (filter, "You can hardly repeat that.\n");
          return FALSE;
        }

      /*
       * 4.0 with "References in brackets" ticked (the setting Scarier
       * models) echoes the command it is about to repeat, in round brackets
       * on its own line: run400 generaltasks loc_48A058 walks the history
       * past the "again"s, then at loc_48A095 tests MemVar_4942BA and prints
       * "(" & command & ")" & vbCrLf through Proc_21_19_47B568.  Earlier
       * Runners have no "(" literal.
       */
      if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
        pf_buffer_reference (filter, prior_element);

      /* Make the last element the current input element. */
      strncpy (line_element, prior_element, LINE_BUFFER_SIZE);
    }
  else
    {
      scr_int length, extent;

      /*
       * If there's none buffered, read a new line of player input.  Other-
       * wise, separate output so far with a newline.
       */
      if (line_buffer[0] == NUL)
        {
          if_read_line (line_buffer, sizeof (line_buffer));

          /*
           * Every Runner lower-cases the whole typed line before it parses
           * anything: run400 Form1 loc_45C5D1..45C5E5 echoes `"> " & cmd`
           * first and only then assigns `cmd = LCase(cmd)`, pushing the
           * lower-cased copy into the command history array as well
           * (run390 loc_436235..436249, run380 loc_426FA9, run370
           * loc_422091 -- unconditional in all four, no version gate).
           * The echo is unaffected because it happens above the LCase, and
           * Glk echoes the input line for us here.
           *
           * This matters because the game's own SYNONYM rewrites run AFTER
           * it, so an author's replacement text is the only thing that can
           * put an upper-case letter back into a command -- which is what
           * makes a character unreferenceable in uip_case_folds_name().
           */
          for (scr_char *cursor = line_buffer; *cursor != NUL; cursor++)
            *cursor = scr_tolower (*cursor);
        }
      else
        if_print_character ('\n');

      /*
       * Find the length of the next input line element.  Unless the line
       * buffer is empty, we always take the first character, even if it's a
       * separator.  This catches odd input like "." and turns it into a
       * parser complaint, rather than treating it as two empty commands with
       * a separator between them; this makes it close to what Inform does
       * with similar inputs.
       */
      length = (line_buffer[0] == NUL) ? 0 : 1;
      while (line_buffer[length] != NUL
             && !run_is_separator (line_buffer, length))
        length++;

      /*
       * Make this the current input element, and remove it, the separator,
       * and any trailing whitespace, from the front of the line buffer.
       * Removing whitespace prevents "i. ." looking like "i" and ""; it
       * instead looks like "i" and ".", and results in a parser complaint.
       */
      memcpy (line_element, line_buffer, length);
      line_element[length] = NUL;

      extent = length;
      extent += (line_buffer[length] == NUL
                 || !run_is_separator (line_buffer, length)) ? 0 : 1;
      extent += strspn (line_buffer + extent, WHITESPACE);
      memmove (line_buffer,
               line_buffer + extent, strlen (line_buffer) - extent + 1);
    }

  /* Copy the current game to the temporary undo buffer. */
  gs_copy (game->temporary, game);

  /*
   * Filter the input element for synonyms, then for pronouns.  Both are
   * scr_malloc'd, so own them with RAII -- run_all_commands() below can throw
   * (run_loop_halt / scr_fatal_error), and the old manual scr_free()s sat after
   * that call, leaking on the throw.  .get() still feeds the raw char* to the
   * pointer-aliasing logic that decides which buffer "wins".
   */
  scr_owned_string filtered (pf_filter_input (line_element, bundle));
  scr_owned_string replaced (uip_replace_pronouns (game,
      filtered ? filtered.get () : line_element));

  /*
   * If filtering didn't replace synonyms, and no pronouns were replaced, use
   * the original line element.  The "(to Nobody)" / "(GARGOYLE)" reference
   * rewrites are NOT applied here: the Runner applies them only once its
   * typed-command task dispatcher has declined the line -- see
   * run_all_commands().
   */
  command = replaced ? scr_normalize_string (replaced.get ())
            : (filtered ? scr_normalize_string (filtered.get ()) : line_element);

  /*
   * Upstream SCARE echoed the rewritten command in italic square brackets,
   * for synonyms and for pronouns alike.  No Runner does that: run370 and
   * run380 have no "[" string literal at all, run390's only one is the
   * "[More]" pager, and 4.0's synonym substitution is silent too.  The echo
   * also exposed authoring the player is not meant to see -- "The Warlord,
   * The Princess & The Bulldog" routes "i"/"inv"/"inventory" through a
   * synonym to the keyword its inventory task listens for, so every "i"
   * answered with a bare "[iii]".
   *
   * What 4.0 does print, with "References in brackets" ticked, is the
   * pronoun's antecedent in round brackets on its own line -- "(a trophy)";
   * uip_replace_pronouns() buffers that as it substitutes.
   */

  /*
   * Note whether a 4.0 ambiguity prompt left a question open, and close it:
   * whatever this line turns out to be, the question is spent by the end of
   * it.  See lib_co_400_raise() in sclibrar.cpp.
   */
  lib_co_400_begin_line ();

  /* Try the command line element against command matchers. */
  status = run_all_commands (game, command);

  /*
   * 4.0: with an ambiguity question open, a line that did nothing is an
   * answer to it rather than a line the game misunderstood.  "Did nothing"
   * is either of the two refusals that end a turn empty-handed -- the
   * DontUnderstand path below, and the unhandled-verb catch-all in
   * lib_cmd_verb_object() -- and the turn's own output goes with it, the way
   * the 3.8 prompt replaces a turn wholesale (pf_empty() in
   * lib_co_ambiguity_prompt()).  A line that DID something runs normally and
   * simply spends the question: run400 answers `x tree rock` / `x rock` with
   * "A plain thing." and not with the refusal (Adrift_930).
   *
   * The typed words go in front of the pending noun and the prompt's own
   * candidates are re-scored; a unique winner re-runs the original command
   * with that object forced, and anything else says "That is still
   * ambiguous!".  Measured on p4CO.taf, Adrift_927 and Adrift_929 -- see
   * lib_co_400_answer_object().
   */
  if (lib_co_400_question_pending () && !scr_strempty (command)
      && (!status || lib_co_400_line_refused ()))
    {
      const scr_int answer = lib_co_400_answer_object (game, command);

      pf_empty (filter);
      if (answer >= 0)
        {
          const std::string original (lib_co_400_pending_command ());

          lib_co_400_set_forced (answer);
          status = run_all_commands (game, original.c_str ());
          lib_co_400_set_forced (-1);
        }
      else
        {
          lib_co_400_print_still_ambiguous (game);
          status = TRUE;
        }

      line_buffer[0] = NUL;
      return status;
    }

  if (!status)
    {
      const scr_char *message;

      /*
       * An EMPTY line element complains too.  Upstream SCARE guarded this
       * whole block with `if (!scr_strempty (command))`, so a bare Return
       * printed nothing.  Both Runners answer one with DontUnderstand:
       * `cmdfile_stardust.txt` and `cmdfile_xfiles.txt` are the only CRLF
       * feeds in the Wine harness, so every command in those two runs went in
       * followed by an extra empty Return, and run390 answered all 115 of
       * them with S_Tar_Dus's ALR for the message ("I are confused.  DURHH!",
       * Adrift_38_stardust.txt) and run400 all 22 of xfiles' ("Nope!",
       * Adrift_31_xfiles.txt).  No walk or event line follows one, so the
       * turn does not tick either -- which the FALSE return below already
       * gives us, run_main_loop() ticking only on TRUE.  Only a genuinely
       * empty input line gets here: the splitter above takes the first
       * character even when it is a separator, so "." and "i. ." were
       * complaints before this and still are.
       */

      /*
       * 4.0: a line that names a character and was ended by a task says
       * nothing at all.  run400's tail tests two conditions together --
       * `48B573: If MemVar_4941B0 = "" And var_29C = 0 Then
       * MemVar_4941B0 = MemVar_4941A8` -- where var_29C is set by the walk
       * over the characters at 48B53C-48B569 (uip_line_names_npc() here) and
       * MemVar_4941A8 is the game's DontUnderstand text.  Outside an ending
       * the second condition never shows: a line naming a present character
       * that nothing else answered gets the catch-all from characters()
       * instead, so the buffer is not empty.  Once a task has ended the game
       * that catch-all is suppressed (4805CD, see lib_cmd_verb_npc()), and
       * the empty buffer meets var_29C = 1 and prints nothing.
       *
       * easter.taf's winning `show basket to shopkeeper` is the measured
       * case: run400 Adrift_273_easter.txt:304-308 goes straight from the
       * task's text to the WinText.  Gated on the ending so the general
       * shape of the test cannot disturb an ordinary line.
       */
      if (game->pending_endgame != 0
          && run_get_version (bundle) == TAF_VERSION_400
          && uip_line_names_npc (game, line_element))
        {
          line_buffer[0] = NUL;
          return status;
        }

      /*
       * Command line element not understood.  Own the escaped copy with
       * RAII (as the sibling code above does): var_set_ref_text() can throw
       * (scr_fatal_error), and the old manual scr_free() after it leaked on
       * the throw.
       */
      scr_owned_string escaped (pf_escape (scr_normalize_string (line_element)));
      var_set_ref_text (vars, escaped.get ());
      message = prop_get_global_string (bundle, "DontUnderstand");
      pf_buffer_string (filter, message);
      pf_buffer_character (filter, '\n');

      /*
       * On a line element that's not understood, throw out any remaining
       * input line elements.
       */
      line_buffer[0] = NUL;
      return status;
    }
  else
    {
      /*
       * Unless administrative, back up any valid undo, copy the temporary
       * game into the undo buffer, flag the undo buffer as available, and
       * assign any pronouns used in the command ready for the next iteration.
       */
      if (!game->is_admin)
        {
          if (game->undo_available)
            memo_save_game (memento, game->undo);

          gs_copy (game->undo, game->temporary);
          game->undo_available = TRUE;

          uip_assign_pronouns (game, command);
        }
    }

  /*
   * If do_again is set, we'll come round with the prior command in line
   * element in a moment, so save nothing for that case.  Otherwise save the
   * command in the history.
   */
  if (!scr_strempty (line_element) && !game->do_again)
    {
      /*
       * If this is a failed redo, redo_sequence will be set but do_again will
       * be clear.  Suppress the save for this special case; otherwise, failed
       * redo commands get into the history, where they can cause problems
       * later on.
       */
      if (game->redo_sequence == 0)
        {
          scr_int timestamp;

          timestamp = var_get_elapsed_seconds (vars);
          memo_save_command (memento, line_element, timestamp, game->turns);
        }
      else
        game->redo_sequence = 0;
    }

  /*
   * Special case restart and restore commands; throw out any remaining input
   * and return straight away.  Do the same if this was an undo, detected by
   * noting that undo is no longer available, where it was on entry.
   */
  if (game->do_restart || game->do_restore
      || (was_undo_available && !game->undo_available))
    {
      line_buffer[0] = NUL;
      return status;
    }

  /* If not empty, consider as saving for "again" calls and in the history. */
  if (!scr_strempty (line_element))
    {
      /*
       * Unless "again", note this line element as prior input.  "Again" shows
       * up as do_again set in the game, where it wasn't when we entered here.
       */
      if (!game->do_again && !is_rerunning)
        strncpy (prior_element, line_element, LINE_BUFFER_SIZE);

      /*
       * If this was a request to run a command from the history, copy that
       * command into the prior_element for the next iteration.  The library
       * should have verified the value in redo_sequence, so fetching the
       * command string should not fail.
       */
      if (game->do_again && game->redo_sequence != 0)
        {
          const scr_char *redo_command;

          redo_command = memo_find_command (memento, game->redo_sequence);
          if (redo_command)
            strncpy (prior_element, redo_command, LINE_BUFFER_SIZE);
          else
            {
              scr_error ("run_player_input: invalid redo sequence request\n");
              game->do_again = FALSE;
            }
          game->redo_sequence = 0;
        }
    }

  return status;
}


/*
 * run_text_ends_in_newline()
 *
 * Return TRUE if the displayable form of text -- tags stripped and any <br>
 * mapped to a newline -- ends in a newline, ignoring trailing horizontal
 * whitespace.  Used so the startup intro's own trailing line break is not
 * doubled up by SCARIER's paragraph break before the first room.
 */
static scr_bool
run_text_ends_in_newline (const scr_char *text)
{
  scr_int length;

  std::vector<scr_char> stripped (text, text + strlen (text) + 1);
  pf_strip_tags_for_hints (stripped.data ());

  length = strlen (stripped.data ());
  while (length > 0
         && (stripped[length - 1] == ' ' || stripped[length - 1] == '\t'))
    length--;
  return (length > 0 && stripped[length - 1] == '\n');
}


/*
 * run_prompt_restore()
 *
 * Helper for the game-start name and gender prompts.  If the player types
 * "restore" (or "load") at one of these prompts, initiate a restore, exactly
 * as the equivalent game command would.  On a successful restore this unwinds
 * back into the interpreter loop (via run_loop_halt) and never returns; on a
 * failed or cancelled
 * restore it returns TRUE so the caller re-prompts (rather than treating the
 * typed word as an answer).  Returns FALSE when the reply is not a restore.
 */
static scr_bool
run_prompt_restore (scr_gameref_t game, const scr_char *reply)
{
  const scr_char *string;

  /* Skip leading whitespace. */
  for (string = reply; *string == ' ' || *string == '\t'; string++)
    ;

  if (scr_strcasecmp (string, "restore") == 0
      || scr_strcasecmp (string, "load") == 0)
    {
      run_restore_prompted (game);
      return TRUE;
    }

  return FALSE;
}


/*
 * run_prompt_player_name()
 *
 * When a game's "prompt for player name" option is set, the Runner asks the
 * player to type a name at game start (InputBox "Please enter your name:") and
 * uses it for the player throughout (%player% substitutions); an empty answer
 * becomes "Anonymous".  SCARIER parsed but never honoured the option, so the
 * name stayed at its authored default (often blank -> "Player").  Ask for it
 * here, mirroring the Runner.  Like the gender choice, the answer is stored in
 * the session-persistent property bundle.
 */
static void
run_prompt_player_name (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[2];
  scr_char buffer[LINE_BUFFER_SIZE];
  const scr_char *name;

  if (!prop_get_global_boolean (bundle, "PromptName"))
    return;

  for (;;)
    {
      pf_buffer_string (filter, "Please enter your name: ");
      pf_flush (filter, vars, bundle);

      if_read_line (buffer, sizeof (buffer));      /* Trailing newline stripped. */

      /* "restore"/"load" initiates a restore instead of naming the player. */
      if (run_prompt_restore (game, buffer))
        continue;

      /* Skip leading whitespace; a blank answer becomes "Anonymous". */
      for (name = buffer; *name == ' ' || *name == '\t'; name++)
        ;
      if (*name == NUL)
        name = "Anonymous";
      break;
    }

  vt_key[0].string = "Globals";
  vt_key[1].string = "PlayerName";
  prop_put_string (bundle, "S<-ss", name, vt_key);
}


/*
 * run_prompt_player_gender()
 *
 * Adrift stores the player's gender as Male, Female, or Unknown.  When it is
 * Unknown, the Runner shows a "Please choose player gender" dialog at game
 * start and stores the answer; tasks and restrictions then test it (for
 * example, "the Player is Male").  Without this choice such a restriction can
 * never pass, which can render a game unwinnable (e.g. The Secret of the Lost
 * World gates the castle on it).  Prompt for the choice and record it in the
 * globals, mirroring the Runner.  The value lives in the (session-persistent)
 * property bundle, so it survives save/restore/undo within a session, and a
 * fresh load re-asks -- exactly as the Runner behaves.
 *
 * The stored value is ADRIFT's own gender enumeration -- Male 0, Female 1,
 * Unknown/Neuter 2, the NPC_MALE/NPC_FEMALE/NPC_NEUTER of scprotos.h -- because
 * that is what a type-3 var2=7 restriction compares against (screstrs.cpp case
 * 7 does a bare `gender == var3`).  Recording male as 1 and female as 0, as
 * this used to, silently ran every gender-gated task on the opposite branch:
 * "Provenance" dressed a male player in a house dress, and "The Secret of the
 * Lost World" answered "female" but took the male path (ring to the princess).
 */
static void
run_prompt_player_gender (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[2];
  scr_int gender;

  gender = prop_get_global_integer (bundle, "PlayerGender");

  /* Only an Unknown (2) gender needs a choice; Male (0)/Female (1) are set. */
  if (gender != NPC_NEUTER)
    return;

  for (;;)
    {
      scr_char buffer[LINE_BUFFER_SIZE];
      const scr_char *reply;

      pf_buffer_string (filter,
                        "Please choose the player's gender (male or female): ");
      pf_flush (filter, vars, bundle);

      if_read_line (buffer, sizeof (buffer));

      /* "restore"/"load" initiates a restore instead of choosing a gender. */
      if (run_prompt_restore (game, buffer))
        continue;

      for (reply = buffer; *reply == ' ' || *reply == '\t'; reply++)
        ;
      if (*reply == 'm' || *reply == 'M')
        {
          gender = NPC_MALE;
          break;
        }
      if (*reply == 'f' || *reply == 'F')
        {
          gender = NPC_FEMALE;
          break;
        }
      pf_buffer_string (filter, "Please answer \"male\" or \"female\".\n");
    }

  vt_key[0].string = "Globals";
  vt_key[1].string = "PlayerGender";
  prop_put_integer (bundle, "I<-ss", gender, vt_key);
}


/*
 * run_main_loop()
 *
 * Main interpreter loop.
 */
static void
run_main_loop (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * This may not be the very first time this game has been used, for example
   * saving a game right at the start, or undo-ing back to the start through
   * memos.  Caught by looking to see if the player room is marked as seen.
   */
  if (!gs_room_seen (game, gs_playerroom (game)))
    {
      scr_vartype_t vt_key[2];
      const scr_char *gamename, *startuptext;
      scr_bool disp_first_room;

      /* Initial clear screen. */
      pf_buffer_tag (filter, SCR_TAG_CLS);

      /*
       * Print the game name.  The Runner gives this line a look of its own,
       * not the plain body style: one step larger than normal text, in the
       * secondary ("command") colour -- the same red that <c> spans and the
       * player's own typing come out in.  Measured off a 3.90 Runner shot of
       * rich_text_390.taf: the title's ascenders run 13px against normal
       * text's 11 (12pt -> 14pt), with the stroke weight of normal text, not
       * of bold.
       *
       * Emit it as markup rather than as a port-side special case, so it
       * costs the ports nothing: the Glk port already maps a 14pt font to
       * style_Subheader and <c> to the input colour, and the ANSI port
       * discards both tags, leaving headless output unchanged.
       */
      gamename = prop_get_global_string (bundle, "GameName");
      pf_buffer_string (filter, "<font size=14><c>");
      pf_buffer_string (filter, gamename);
      pf_buffer_string (filter, "</c></font>");
      pf_buffer_character (filter, '\n');

      /*
       * Print the game header.  Adrift StartupText conventionally ends with a
       * <br> tag to set off the intro from the first room.  SCARIER supplies its
       * own paragraph break below (the forced newline here plus the leading
       * newline from lib_cmd_look()), so adding a terminator when the text
       * already ends in a line break leaves the first room preceded by two
       * blank lines.  The Adrift Runner shows just one; only add the
       * terminator when the displayed text doesn't already end in a newline.
       */
      vt_key[0].string = "Header";
      vt_key[1].string = "StartupText";
      startuptext = prop_get_string (bundle, "S<-ss", vt_key);
      pf_buffer_string (filter, startuptext);
      if (!run_text_ends_in_newline (startuptext))
        pf_buffer_character (filter, '\n');

      /*
       * Alignment is a local of the Runner's display routine, so it starts out
       * left on every call and no <center> outlives the one string it was
       * opened in.  SCARIER instead buffers a whole turn's worth of strings and
       * hands the lot to the port as one stream, so a title page that opens
       * <center> and never closes it -- "Cut the Red Wire! No, the Blue Wire!"
       * for one -- would carry on centering the first room description, which
       * the Runner displays in a separate call.  Close the intro's alignment
       * here, at that call boundary.  Games with balanced tags see nothing:
       * the tag lands at the start of a line, where the Glk port breaks no
       * paragraph because the alignment doesn't change and the ANSI port
       * breaks none because there is nothing buffered on the line.
       */
      pf_buffer_tag (filter, SCR_TAG_ENDCENTER);

      /* If the game asks, prompt for the player's name, then (if Unknown) the
       * player's gender -- both at game start, like the Runner. */
      run_prompt_player_name (game);
      run_prompt_player_gender (game);

      /*
       * Start the events that start immediately, before anything is
       * described: both Runners do this during load, so an immediate event's
       * LookText belongs to the OPENING room description and its StartText is
       * never seen (probes EV6 / make_39_fwprobe variant "e", live
       * 2026-08-02 -- see evt_start_load_events()).
       */
      evt_start_load_events (game);

      /* If flagged, describe the initial room. */
      disp_first_room = prop_get_global_boolean (bundle, "DispFirstRoom");
      if (disp_first_room)
        lib_cmd_look (game);

      /* Handle any introductory resources. */
      vt_key[0].string = "Globals";
      vt_key[1].string = "IntroRes";
      res_handle_resource (game, "ss", vt_key);

      /* Set initial values for NPC states. */
      npc_setup_initial (game);

      /* Roll initial battle stamina if the Battle System is enabled. */
      battle_start (game);

      /*
       * Nudge events, but NOT NPC walks: the Runner's walk handler (Sub_20_2)
       * is reachable only from the typed-command evaluator (Sub_20_62, called
       * solely by Form1.evaluate), so walks never tick before the first
       * command -- settled live 2026-08-01 in BOTH Runners (walk probe C: no
       * CharTask fires before the first prompt; Scarier used to move walking
       * NPCs, and fire their CharTask, during startup).
       *
       * The zero-length half of the load start finishes here rather than
       * above: its FinishText, TaskAffected and any restart land BELOW the
       * opening description in the real Runner.
       *
       * Both halves are 3.90+ only.  The startup tick is run390's `tstart'
       * (42E940) calling events() at 42E90B straight after viewroom, and
       * run400's tstart calling 449310 the same way; run380's events()
       * (425094) and run370's (432538) have exactly ONE caller each,
       * generaltasks, so nothing ticks before the first command in those
       * two.  Their load code (run380 448DC9, run370 440083) puts a
       * StarterType=1 event straight into RUNNING with its rolled length --
       * no StartText, the same silent start as above -- and a StarterType=2
       * event into WAITING with its rolled delay, which the first command
       * then decrements: a delay of 1 starts the event on turn 1, not at
       * load.  Measured 2026-09-04 in run380 with haunt.taf (transcript
       * Adven_1_haunt.rtf): its Weather event (delay 1..1, length 4, restart
       * after delay) prints "Thunder rumbles ominously." on the first
       * command turn and cycles from there, where the startup tick had put
       * that line under the intro and every later weather line one turn
       * early.  A zero-length immediate event parks in 3.8 (its clock goes
       * -1, -2, ... past the `= 0' finish test at run380 43A474), so it is
       * not finished at load either; that half is read from the decompile,
       * not measured -- the corpus has one such event, wrecked.taf's EVENT
       * 35, whose only effect is an un-complete of a task nothing has
       * completed yet.
       */
      if (run_get_version (bundle) >= TAF_VERSION_390)
        {
          evt_finish_load_events (game);
          evt_tick_events (game);
        }

      /*
       * Notify the debugger that the game has started.  This is a chance to
       * set watchpoints to catch game startup actions.  Done before setting
       * the initial room visited as this is how the debugger differentiates
       * restarts from restore or undo back to game start.
       */
      debug_game_started (game);

      /* Note the initial room as visited. */
      gs_set_room_seen (game, gs_playerroom (game), TRUE);
    }
  else
    {
      /* Notify the debugger that the game has restarted. */
      debug_game_started (game);
    }

  /*
   * Game loop, exits either when a command parser handler sets the game
   * running flag to FALSE, or by call to run_quit().
   */
  while (game->is_running)
    {
      scr_bool status;

      /*
       * Synchronize any resources in use; do this before flushing so that any
       * appropriate graphics/sound appear before waits or waitkey tag delays
       * invoked by flushing the printfilter.  Also, print any score change
       * notifications.
       */
      res_sync_resources (game);
      run_notify_score_change (game);

      /*
       * Flush printfilter of any accumulated output, and clear any prior
       * notion of administrative commands from input.
       */
      pf_flush (filter, vars, bundle);
      game->is_admin = FALSE;

      /* If waitcounter is zero, accept and try a command. */
      if (game->waitcounter == 0)
        {
          /* Not waiting, so handle a player input line. */
          run_update_status (game);
          status = run_player_input (game);

          /*
           * If waitcounter is now set, decrement it, as this turn counts as
           * one of them.
           */
          if (game->waitcounter > 0)
            game->waitcounter--;
        }
      else
        {
          /*
           * Currently "waiting"; decrement wait turns, then run a turn having
           * taken no input.
           */
          game->waitcounter--;
          status = TRUE;
        }

      /*
       * Do usual turn stuff unless either something stopped the game, or the
       * last command didn't match, or the last command did match but was
       * administrative.
       */
#ifdef SCARIER_DUMP_TOOLS
      {
        static const scr_bool trace_admin = getenv ("SCR_TRACE_ADMIN") != NULL;
        if (trace_admin && status && game->is_admin)
          fprintf (stderr, "ADMIN turn=%ld after [%s]\n", game->turns,
                   run_trace_last_input.c_str ());
      }
#endif
      if (status && !game->is_admin)
        {
          /* Increment turn counter, and clear notifications done flag. */
          game->turns++;
          game->has_notified = FALSE;

          if (game->is_running)
            {
              /* Nudge NPCs then events (Runner: Sub_20_2 before Sub_20_32). */
              npc_tick_npcs (game);
              evt_tick_events (game);

              /* Resolve Battle System combat and recovery for the turn. */
              battle_tick (game);

              /* Update NPC states. */
              npc_turn_update (game);

              /* Note the current room as visited. */
              gs_set_room_seen (game, gs_playerroom (game), TRUE);

              /* Give the debugger a chance to catch watchpoints. */
              debug_turn_update (game);
            }
        }

      /*
       * Pre-4.0: the Runner's object-ambiguity flag, raised by the scan at
       * the top of generaltasks() and read only now, after the events have
       * ticked (run380 @4431B0), replaces the turn's whole output with its
       * "Which <term>.  <list>?" -- unless a game task claimed the line.
       */
      if (!run_co_pending_input.empty ())
        {
          if (!run_co_task_claimed)
            lib_co_ambiguity_prompt (game, run_co_pending_input.c_str ());
          run_co_pending_input.clear ();
        }

      /*
       * End of turn: if an EndGame task action armed an ending, print it now.
       * The Runner's turn driver does exactly this, testing its gameover byte
       * only after Form1.evaluate has returned (0005C681), so anything the
       * rest of the turn did to the score is already in the summary.
       */
      task_print_end_game_message (game);
    }

  /*
   * Catch an ending armed before the loop ever ran -- evt_start_load_events()
   * and the startup evt_tick_events() above can both fire a task.  Harmless
   * when the loop already printed it; the pending ending is cleared as it
   * goes.
   */
  task_print_end_game_message (game);

  /*
   * Final status update, for games that vary it on completion, then notify
   * the debugger that the game has ended, to let it make a last watchpoint
   * scan and offer the dialog if appropriate.
   */
  run_update_status (game);
  debug_game_ended (game);

  /*
   * Final resource sync, score change notification and printfilter flush
   * on game-instigated loop exit.
   */
  res_sync_resources (game);
  run_notify_score_change (game);
  pf_flush (filter, vars, bundle);

  /*
   * Reset static variables inside run_player_input() with a call to it with
   * is_running false; this is a special case.
   */
  assert (!game->is_running);
  run_player_input (game);
}


/*
 * run_create()
 *
 * Create a game context from a callback.
 */
scr_gameref_t
run_create (scr_read_callbackref_t callback, void *opaque)
{
  scr_tafref_t taf;
  scr_prop_setref_t bundle = NULL;
  scr_var_setref_t vars = NULL, temporary_vars = NULL, undo_vars = NULL;
  scr_filterref_t filter = NULL;
  scr_gameref_t game = NULL, temporary_game = NULL, undo_game = NULL;
  assert (callback);

  /* Create a new TAF using the callback; return NULL if this fails. */
  taf = taf_create (callback, opaque);
  if (!taf)
    return NULL;
  else if (if_get_trace_flag (SCR_DUMP_TAF))
    taf_debug_dump (taf);

  /*
   * Any construction step below can throw (scr_fatal on a corrupt game);
   * reclaim whatever has been built so far on that path -- mirroring
   * run_destroy()'s teardown -- then let the throw carry on to the interface
   * boundary, which reports the game as unusable.
   */
  try
    {
      /* Create a properties bundle, and parse the TAF data into it. */
      bundle = prop_create (taf);
      if (!bundle)
        {
          scr_error ("run_create: error parsing game data\n");
          taf_destroy (taf);
          return NULL;
        }
      else if (if_get_trace_flag (SCR_DUMP_PROPERTIES))
        prop_debug_dump (bundle);

      /* Try to set an interpreter locale from the properties bundle. */
      loc_detect_game_locale (bundle);
      if (if_get_trace_flag (SCR_DUMP_LOCALE_TABLES))
        loc_debug_dump ();

      /* Create a set of variables from the bundle. */
      vars = var_create (bundle);
      if (if_get_trace_flag (SCR_DUMP_VARIABLES))
        var_debug_dump (vars);

      /* Create a printfilter for the game. */
      filter = pf_create ();

      /*
       * Create an initial game state, and register it with variables.  Also,
       * create undo buffers, and initialize them in the same way.
       */
      game = gs_create (vars, bundle, filter);
      var_register_game (vars, game);

      temporary_vars = var_create (bundle);
      temporary_game = gs_create (temporary_vars, bundle, filter);
      var_register_game (temporary_vars, temporary_game);

      undo_vars = var_create (bundle);
      undo_game = gs_create (undo_vars, bundle, filter);
      var_register_game (undo_vars, undo_game);

      /* Add the undo buffers and memos to the game, and return it. */
      game->temporary = temporary_game;
      game->undo = undo_game;
      game->memento = memo_create ();
      return game;
    }
  catch (...)
    {
      if (undo_game)
        gs_destroy (undo_game);
      if (undo_vars)
        var_destroy (undo_vars);
      if (temporary_game)
        gs_destroy (temporary_game);
      if (temporary_vars)
        var_destroy (temporary_vars);
      if (game)
        gs_destroy (game);
      if (filter)
        pf_destroy (filter);
      if (vars)
        var_destroy (vars);
      if (bundle)
        prop_destroy (bundle);  /* also destroys the taf it adopted */
      else
        taf_destroy (taf);
      throw;
    }
}


/*
 * run_get_restart_count()
 *
 * How many times a game has been restarted in this process.
 *
 * A restart never comes back through run_interpret's caller: RESTART typed at
 * the prompt unwinds into the loop below, which replays the opening without
 * returning, and even the front end's own scr_restart_game() leaves nothing
 * behind to see.  So a front end with screen furniture to reconsider when the
 * game goes back to the beginning -- the Glk port's map pane -- watches this
 * for a change instead.
 *
 * Deliberately not part of the game state: a restart replaces that wholesale,
 * and a restore or undo would carry an older count back.
 */
static scr_int run_restart_count = 0;

scr_int
run_get_restart_count (void)
{
  return run_restart_count;
}


/*
 * run_restart_handler()
 *
 * Return a game context to initial states to restart a game.
 */
static void
run_restart_handler (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_gameref_t new_game;
  scr_var_setref_t new_vars;

  /*
   * Create a fresh set of variables from the current game properties,
   * then a new game using these variables and existing properties and
   * printfilter.
   */
  new_vars = var_create (bundle);
  new_game = gs_create (new_vars, bundle, filter);
  var_register_game (new_vars, new_game);

  /*
   * Overwrite the dynamic parts of the current game with the new one.
   */
  new_game->temporary = game->temporary;
  new_game->undo = game->undo;
  gs_copy (game, new_game);

  /* Destroy invalid game status strings. */
  game->current_room_name.reset ();
  game->status_line.reset ();

  /*
   * Now it's safely copied, destroy the temporary new game, and its
   * associated variable set.
   */
  gs_destroy (new_game);
  var_destroy (new_vars);

  /* Reset resources handling. */
  res_cancel_resources (game);

  /* The one place a restart can be counted; see run_get_restart_count(). */
  run_restart_count++;
}


/*
 * run_restore_handler()
 *
 * Adjust a game context for continuation after restoring a game.
 */
static void
run_restore_handler (scr_gameref_t game)
{
  /* Invalidate the undo buffer. */
  game->undo_available = FALSE;

  /*
   * Resources handling?  Arguably we should re-offer resources active when
   * the game was saved, but I can't see how this can be achieved with Adrift
   * the way it is.  Canceling is too broad, so I'll go here with just
   * stopping sounds (in case looping).
   *
   * TODO Rationalize what happens here.
   */
  game->stop_sound = TRUE;
}


/*
 * run_quit_handler()
 *
 * Tidy up printfilter and input statics on game quit.
 */
static void
run_quit_handler (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /* Flush printfilter and notifications of any dangling output. */
  run_notify_score_change (game);
  pf_flush (filter, vars, bundle);

  /* Cancel any active resources. */
  res_cancel_resources (game);

  /*
   * Make the special call to reset all of the static variables inside
   * run_player_input().
   */
  assert (!game->is_running);
  run_player_input (game);
}


/*
 * run_interpret()
 *
 * Intepret the game in a game context.
 */
void
run_interpret (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /* Verify the game is not already running, and is runnable. */
  if (game->is_running)
    {
      scr_error ("run_interpret: game is already running\n");
      return;
    }
  if (game->has_completed)
    {
      scr_error ("run_interpret: game has already completed\n");
      return;
    }

  /* Refuse to run a game with no rooms. */
  if (gs_room_count (game) == 0)
    {
      scr_error ("run_interpret: game contains no rooms\n");
      return;
    }

  /* Run the main interpreter loop until no more restarts. */
  game->is_running = TRUE;
  do
    {
      /* Run the game until some form of halt is requested. */
      try
        {
          run_main_loop (game);
        }
      catch (const run_loop_halt &)
        {
          /*
           * run_quit / run_restart / run_restore / run_undo unwound a running
           * game out of the main loop; the do_restart/do_restore flags below
           * decide whether we loop again or stop (matching the old longjmp).
           */
        }

      /*
       * If the halt was a restart or restore, cancel the request, handle
       * restart or restore game adjustments, and set the game running
       * again.
       */
      if (game->do_restart)
        {
          game->do_restart = FALSE;
          run_restart_handler (game);
          game->is_running = TRUE;
        }

      if (game->do_restore)
        {
          game->do_restore = FALSE;
          run_restore_handler (game);
          game->is_running = TRUE;
        }
    }
  while (game->is_running);

  /* Tidy up the printfilter and input statics. */
  run_quit_handler (game);
}


/*
 * run_destroy()
 *
 * Destroy a game context, and free all resources.
 */
void
run_destroy (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /* Can't destroy the context of a running game. */
  if (game->is_running)
    {
      scr_error ("run_destroy: game is running, stop it first\n");
      return;
    }

  /*
   * Cancel any game state debugger -- this frees its resources.  Only the
   * primary game may have acquired a debugger.
   */
  debug_set_enabled (game, FALSE);
  assert (!debug_get_enabled (game->temporary));
  assert (!debug_get_enabled (game->undo));

  /*
   * Destroy the game state, variables, properties bundle, memos, undo
   * buffers and their variables, and filter.  The bundle and printfilter
   * are shared by the main game, the undo game, and the temporary game, so
   * destroy these only once!  The main game has a memento, but it is not
   * visible to these other two games, neither of which have one.
   */
  assert (gs_get_bundle (game->temporary) == gs_get_bundle (game));
  assert (gs_get_filter (game->temporary) == gs_get_filter (game));
  assert (gs_get_vars (game->temporary) != gs_get_vars (game));
  assert (!gs_get_memento (game->temporary));
  var_destroy (gs_get_vars (game->temporary));
  gs_destroy (game->temporary);

  assert (gs_get_bundle (game->undo) == gs_get_bundle (game));
  assert (gs_get_filter (game->undo) == gs_get_filter (game));
  assert (gs_get_vars (game->undo) != gs_get_vars (game));
  assert (!gs_get_memento (game->undo));
  var_destroy (gs_get_vars (game->undo));
  gs_destroy (game->undo);

  prop_destroy (gs_get_bundle (game));
  pf_destroy (gs_get_filter (game));
  var_destroy (gs_get_vars (game));
  memo_destroy (gs_get_memento (game));

  gs_destroy (game);
}


/*
 * run_quit()
 *
 * Quits a running game.  This function throws run_loop_halt to unwind back to
 * run_interpret as if run_main_loop() returned, and so never returns to its
 * caller.
 */
void
run_quit (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /* Disallow quitting a non-running game. */
  if (!game->is_running)
    {
      scr_error ("run_quit: game is not running\n");
      return;
    }

  /* Exit the main loop by unwinding back to run_interpret. */
  game->is_running = FALSE;
  throw run_loop_halt ();
}


/*
 * run_restart()
 *
 * Restarts either a running or a stopped game.  For running games, this
 * function throws run_loop_halt to unwind back to run_interpret as if
 * run_main_loop() returned, and so never returns to its caller.  For stopped
 * games, it returns.
 */
void
run_restart (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /*
   * If the game is running, stop it, request a restart, and exit the main
   * loop by throwing run_loop_halt.
   */
  if (game->is_running)
    {
      game->is_running = FALSE;
      game->do_restart = TRUE;
      throw run_loop_halt ();
    }

  /* Restart locally, and ensure that the game remains stopped. */
  run_restart_handler (game);
  game->is_running = FALSE;
}


/*
 * run_save()
 * run_save_to_file()
 * run_save_prompted()
 *
 * Saves either a running or a stopped game.
 *
 * run_save_to_file() is for a save the player keeps, and writes a pre-4.0
 * game's own save format so the original Runner can read it back; run_save()
 * always writes the full 4.0 layout, for snapshots that have to survive a
 * round trip losslessly (the Spatterlight autosave).  See ser_save_game().
 */
void
run_save (scr_gameref_t game, scr_write_callbackref_t callback, void *opaque)
{
  assert (gs_is_game_valid (game));
  assert (callback);

  ser_save_game (game, callback, opaque);
}

void
run_save_to_file (scr_gameref_t game,
                  scr_write_callbackref_t callback, void *opaque)
{
  assert (gs_is_game_valid (game));
  assert (callback);

  ser_save_game_to_file (game, callback, opaque);
}

scr_bool
run_save_prompted (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  return ser_save_game_prompted (game);
}


/*
 * run_restore_common()
 * run_restore()
 * run_restore_prompted()
 *
 * Restores either a running or a stopped game.  For running games, on
 * successful restore, these functions throw run_loop_halt to unwind back to
 * run_interpret as if run_main_loop() returned, and so never return to their
 * caller.  On failed
 * restore, and for stopped games, they will return, with TRUE if successful,
 * FALSE if restore failed.
 */
static scr_bool
run_restore_common (scr_gameref_t game,
                    scr_read_callbackref_t callback, void *opaque)
{
  scr_bool is_running, status;

  /*
   * Save the game running flag, and call the restore appropriate for the
   * caller.  The indication of a call from run_restore_prompted() is a
   * callback of NULL; callback cannot be NULL for run_restore() calls.
   */
  is_running = game->is_running;
  status = callback ? ser_load_game (game, callback, opaque)
                    : ser_load_game_prompted (game);
  if (status)
    {
      /* Loading a game clears is_running -- restore it here. */
      game->is_running = is_running;

      /*
       * If the game is (was) running, set flags so that the interpreter
       * loop cycles, and exit the main loop by throwing run_loop_halt.
       */
      if (game->is_running)
        {
          game->is_running = FALSE;
          game->do_restore = TRUE;
          throw run_loop_halt ();
        }
    }

  /* Return TRUE on successful restore of a stopped game, FALSE on error. */
  return status;
}

scr_bool
run_restore (scr_gameref_t game, scr_read_callbackref_t callback, void *opaque)
{
  assert (gs_is_game_valid (game));
  assert (callback);

  return run_restore_common (game, callback, opaque);
}

scr_bool
run_restore_prompted (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  return run_restore_common (game, NULL, NULL);
}


/*
 * run_undo()
 *
 * Undo a turn in either a running or a stopped game.  Returns TRUE on
 * successful undo, FALSE if no undo buffer is available.
 */
scr_bool
run_undo (scr_gameref_t game)
{
  const scr_memo_setref_t memento = gs_get_memento (game);
  scr_bool is_running;
  assert (gs_is_game_valid (game));

  /* Save the game's running state, so we can restore it later. */
  is_running = game->is_running;

  /* If there's an undo buffer available, restore it. */
  if (game->undo_available)
    {
      /* Restore the undo buffer, and then restore running flag. */
      gs_copy (game, game->undo);
      game->undo_available = FALSE;
      game->is_running = is_running;

      /* Location may have changed; update status. */
      run_update_status (game);

      /* Bring resources into line with the revised game. */
      res_sync_resources (game);
      return TRUE;
    }

  /*
   * If there is no undo buffer, try to restore one saved previously in a
   * memo.  Handle as if restoring from a file.
   */
  if (memo_load_game (memento, game))
    {
      /* Loading a game clears is_running -- restore it here. */
      game->is_running = is_running;

      /*
       * If the game is (was) running, set flags so that the interpreter
       * loop cycles, and exit the main loop by throwing run_loop_halt.
       */
      if (game->is_running)
        {
          game->is_running = FALSE;
          game->do_restore = TRUE;
          throw run_loop_halt ();
        }

      /* Game undo on non-running game accomplished with memos. */
      return TRUE;
    }

  /* No undo buffer and no memos available. */
  return FALSE;
}


/*
 * run_is_running()
 *
 * Query the game running state.
 */
scr_bool
run_is_running (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  return game->is_running;
}


/*
 * run_has_completed()
 *
 * Query the game completion state.  Completed games cannot be resumed,
 * since they've run the exit task and thus have nowhere to go.
 */
scr_bool
run_has_completed (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  return game->has_completed;
}


/*
 * run_is_undo_available()
 *
 * Query the game turn undo buffer and memo availability.
 */
scr_bool
run_is_undo_available (scr_gameref_t game)
{
  const scr_memo_setref_t memento = gs_get_memento (game);
  assert (gs_is_game_valid (game));

  return game->undo_available || memo_is_load_available (memento);
}


/*
 * run_get_attributes()
 * run_set_attributes()
 *
 * Get and set selected game attributes.
 */
void
run_get_attributes (scr_gameref_t game,
                    const scr_char **game_name, const scr_char **game_author,
                    const scr_char **game_compile_date,
                    scr_int *turns, scr_int *score, scr_int *max_score,
                    const scr_char **current_room_name,
                    const scr_char **status_line, const scr_char **preferred_font,
                    scr_bool *bold_room_names, scr_bool *verbose,
                    scr_bool *notify_score_change)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[2];
  assert (gs_is_game_valid (game));

  /* Return the game name, author, and compile date if requested. */
  if (game_name)
    {
      if (!game->title)
        {
          const scr_char *gamename;
          scr_char *filtered;

          gamename = prop_get_global_string (bundle, "GameName");

          filtered = pf_filter_for_info (gamename, vars);
          pf_strip_tags (filtered);
          game->title.reset (filtered);
        }
      *game_name = game->title.get ();
    }
  if (game_author)
    {
      if (!game->author)
        {
          const scr_char *gameauthor;
          scr_char *filtered;

          gameauthor = prop_get_global_string (bundle, "GameAuthor");

          filtered = pf_filter_for_info (gameauthor, vars);
          pf_strip_tags (filtered);
          game->author.reset (filtered);
        }
      *game_author = game->author.get ();
    }
  if (game_compile_date)
    {
      vt_key[0].string = "CompileDate";
      *game_compile_date = prop_get_string (bundle, "S<-s", vt_key);
    }

  /* Return the current room name and status line if requested. */
  if (current_room_name)
    *current_room_name = game->current_room_name.get ();
  if (status_line)
    *status_line = game->status_line.get ();

  /* Return any game preferred font, or NULL if none. */
  if (preferred_font)
    {
      vt_key[0].string = "CustomFont";
      if (prop_get_boolean (bundle, "B<-s", vt_key))
        {
          vt_key[0].string = "FontNameSize";
          *preferred_font = prop_get_string (bundle, "S<-s", vt_key);
        }
      else
        *preferred_font = NULL;
    }

  /* Return any other selected game attributes. */
  if (turns)
    *turns = game->turns;
  if (score)
    *score = game->score;
  if (max_score)
    {
      *max_score = prop_get_global_integer (bundle, "MaxScore");
    }
  if (bold_room_names)
    *bold_room_names = game->bold_room_names;
  if (verbose)
    *verbose = game->verbose;
  if (notify_score_change)
    *notify_score_change = game->notify_score_change;
}

void
run_set_attributes (scr_gameref_t game,
                    scr_bool bold_room_names, scr_bool verbose,
                    scr_bool notify_score_change)
{
  assert (gs_is_game_valid (game));

  /* Set game options. */
  game->bold_room_names = bold_room_names;
  game->verbose = verbose;
  game->notify_score_change = notify_score_change;
}


/*
 * run_hint_iterate()
 *
 * Return the next hint appropriate to the game state, or the first if
 * hint is NULL.  Returns NULL if none, or no more hints.  This function
 * works with pointers to a task state rather than task indexes so that
 * the token passed in and out is a pointer, and readily made opaque to
 * the client as a void*.
 */
scr_hintref_t
run_hint_iterate (scr_gameref_t game, scr_hintref_t hint)
{
  scr_int task;
  assert (gs_is_game_valid (game));

  /*
   * Hint is a pointer to a task state; convert to a task index, adding one
   * to move on to the next task, or start at the first task if null.
   */
  if (!hint)
    task = 0;
  else
    {
      /* Convert into pointer, and range check. */
      task = hint - game->tasks.data ();
      if (task < 0 || task >= gs_task_count (game))
        {
          scr_error ("run_hint_iterate: invalid iteration hint\n");
          return NULL;
        }

      /* Advance beyond current task. */
      task++;
    }

  /* Scan for the next runnable task that offers a hint. */
  for (; task < gs_task_count (game); task++)
    {
      if (task_can_run_task (game, task) && task_has_hints (game, task))
        break;
    }

  /* Return a pointer to the state of the task identified, or NULL. */
  return task < gs_task_count (game) ? &game->tasks[task] : NULL;
}


/*
 * run_get_hint_common()
 * run_get_hint_question()
 * run_get_subtle_hint()
 * run_get_unsubtle_hint()
 *
 * Return the strings for a hint.  Front-ends to task functions.  Each
 * converts the hint "address" to a task index through pointer arithmetic,
 * then filters it and returns a temporary, valid only until the next hint
 * call.
 *
 * Hint strings are NULL if empty (not defined by the game).
 */
static const scr_char *
run_get_hint_common (scr_gameref_t game, scr_hintref_t hint,
                     const scr_char *(*handler) (scr_gameref_t, scr_int))
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int task;
  const scr_char *string;
  assert (gs_is_game_valid (game));

  /* Verify the caller passed in a valid hint. */
  task = hint - game->tasks.data ();
  if (task < 0 || task >= gs_task_count (game))
    {
      scr_error ("run_get_hint_common: invalid iteration hint\n");
      return NULL;
    }
  else if (!task_has_hints (game, task))
    {
      scr_error ("run_get_hint_common: task has no hint\n");
      return NULL;
    }

  /* Get the required game text by calling the given handler function. */
  string = handler (game, task);
  if (!scr_strempty (string))
    {
      scr_char *filtered;

      /* Filter and strip tags, note in game. */
      filtered = pf_filter (string, vars, bundle);
      pf_strip_tags_for_hints (filtered);
      game->hint_text.reset (filtered);
    }
  else
    {
      /* Hint text is empty; drop any text noted in game. */
      game->hint_text.reset ();
    }

  return game->hint_text.get ();
}

const scr_char *
run_get_hint_question (scr_gameref_t game, scr_hintref_t hint)
{
  return run_get_hint_common (game, hint, task_get_hint_question);
}

const scr_char *
run_get_subtle_hint (scr_gameref_t game, scr_hintref_t hint)
{
  return run_get_hint_common (game, hint, task_get_hint_subtle);
}

const scr_char *
run_get_unsubtle_hint (scr_gameref_t game, scr_hintref_t hint)
{
  return run_get_hint_common (game, hint, task_get_hint_unsubtle);
}
