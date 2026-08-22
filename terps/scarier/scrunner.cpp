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

/*
 * Module notes:
 *
 * o ...
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
 * run_is_task_function()
 *
 * Check for the presence of a command function in the first task command,
 * and action it if found.  This is a 4.0.42 compatibility hack -- at
 * present, only getdynfromroom() exists.  Returns TRUE if function found
 * and handled.
 */
static scr_bool
run_is_task_function (const scr_char *pattern, scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[3];
  scr_int room, object;

  /* Simple comparison against the one known task expression. */
  std::vector<scr_char> argument (strlen (pattern) + 1);
  if (sscanf (pattern, " # %%object%% = getdynfromroom (%[^)])",
              argument.data ()) == 0)
    return FALSE;

  /*
   * Compare the argument read in against known room names.
   *
   * TODO Is this simple room name comparison good enough?
   */
  vt_key[0].string = "Rooms";
  for (room = 0; room < gs_room_count (game); room++)
    {
      const scr_char *name;

      vt_key[1].integer = room;
      vt_key[2].string = "Short";
      name = prop_get_string (bundle, "S<-sis", vt_key);
      if (scr_strcasecmp (name, argument.data ()) == 0)
        break;
    }
  if (room == gs_room_count (game))
    return FALSE;

  /*
   * Select a dynamic object from the room.
   *
   * TODO What are the selection criteria supposed to be?  Here we use "on
   * the floor".
   */
  vt_key[0].string = "Objects";
  for (object = 0; object < gs_object_count (game); object++)
    {
      scr_bool bstatic;

      vt_key[1].integer = object;
      vt_key[2].string = "Static";
      bstatic = prop_get_boolean (bundle, "B<-sis", vt_key);
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
  {"{go {to {the}}} [in]", lib_cmd_go_in},
  {"{go {to {the}}} [out/o]", lib_cmd_go_out},
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
  {"{go {to {the}}} [in]", lib_cmd_go_in},
  {"{go {to {the}}} [out/o]", lib_cmd_go_out},
  {"{go {to {the}}} [northeast/north-east/ne]", lib_cmd_go_northeast},
  {"{go {to {the}}} [southeast/south-east/se]", lib_cmd_go_southeast},
  {"{go {to {the}}} [northwest/north-west/nw]", lib_cmd_go_northwest},
  {"{go {to {the}}} [southwest/south-west/sw]", lib_cmd_go_southwest},
  {NULL, NULL}
};

/* "Priority" library commands, may take precedence over the game. */
static scr_commands_t PRIORITY_COMMANDS[] = {

  /* Acquisition of and disposal of inventory. */
  {"[[get/take/remove/extract] [all/everything] from/empty] %object%",
   lib_cmd_take_all_from},
  {"[[get/take/remove/extract] [all/everything] from/empty] %object%"
   " [[except/but] {for}/apart from] %text%",
   lib_cmd_take_from_except_multiple},
  {"[get/take/remove/extract] [all/everything]"
   " [[except/but] {for}/apart from] %text% from %object%",
   lib_cmd_take_from_except_multiple},
  {"[get/take/remove/extract] %text% from %object%",
   lib_cmd_take_from_multiple},
  {"[get/take] [all/everything] from %character%", lib_cmd_take_all_from_npc},
  {"[get/take] [all/everything] from %character%"
   " [[except/but] {for}/apart from] %text%",
   lib_cmd_take_from_npc_except_multiple},
  {"[get/take] [all/everything]"
   " [[except/but] {for}/apart from] %text% from %character%",
   lib_cmd_take_from_npc_except_multiple},
  {"[get/take] %text% from %character%", lib_cmd_take_from_npc_multiple},
  {"[[get/take/pick up] [all/everything]/pick [all/everything] up]",
   lib_cmd_take_all},
  {"[get/take/pick up] [all/everything] [[except/but] {for}/apart from] %text%",
   lib_cmd_take_except_multiple},
  {"[get/take/pick up] %text%", lib_cmd_take_multiple},
  {"pick %text% up", lib_cmd_take_multiple},
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
  {"[ex/exam/examine/look {at}] {{the} [room/location]}", lib_cmd_look},
  {"[ex/exam/examine/look {at/in}] %object%", lib_cmd_examine_object},
  {"[ex/exam/examine/look {at}] %character%", lib_cmd_examine_npc},
  {"[ex/exam/examine/look {at}] [me/self/myself]", lib_cmd_examine_self},
  {"[ex/exam/examine/look {at}] all", lib_cmd_examine_all},
#else
  {"[inventory/inv/i]", lib_cmd_inventory},
  {"[x/ex/exam/examine/l/look {at}] {{the} [room/location]}", lib_cmd_look},
  {"[x/ex/exam/examine/look {at/in}] %object%", lib_cmd_examine_object},
  {"[x/ex/exam/examine/look {at}] %character%", lib_cmd_examine_npc},
  {"[x/ex/exam/examine/look {at}] [me/self/myself]", lib_cmd_examine_self},
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
  {"close %object%", lib_cmd_close_object},
  {"unlock %object% with %text%", lib_cmd_unlock_object_with},
  {"lock %object% with %text%", lib_cmd_lock_object_with},
  {"unlock %object%", lib_cmd_unlock_object},
  {"lock %object%", lib_cmd_lock_object},
  {"read %object%", lib_cmd_read_object},
  {"read *", lib_cmd_read_other},
  {"give %object% to %character%", lib_cmd_give_object_npc},
  {"sit {down/up} [on/in] %object%", lib_cmd_sit_on_object},
  {"stand {up/down} [on/in] %object%", lib_cmd_stand_on_object},
  {"[lie/lay] on %object%", lib_cmd_lie_on_object},
  {"get {down/up} off %object%", lib_cmd_get_off_object},
  {"get off", lib_cmd_get_off},
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
  {"[attack/kick/slap] %character% with %object%", lib_cmd_attack_npc_with},
  {"chop %character% with %object%", lib_cmd_chop_npc_with},
  {"cut %character% with %object%", lib_cmd_cut_npc_with},
  {"hit %character% with %object%", lib_cmd_hit_npc_with},
  {"shoot %character% with %object%", lib_cmd_shoot_npc_with},
  {"stab %character% with %object%", lib_cmd_stab_npc_with},
  {"throw %object% at %character%", lib_cmd_throw_npc_with},
  {"kill %character% with %object%", lib_cmd_kill_npc_with},
  {"fight %character% with %object%", lib_cmd_fight_npc_with},
  {"[attack/kick/slap] %character%", lib_cmd_attack_npc},
  {"chop %character%", lib_cmd_chop_npc},
  {"cut %character%", lib_cmd_cut_npc},
  {"shoot %character%", lib_cmd_shoot_npc},
  {"stab %character%", lib_cmd_stab_npc},
  {"kill %character%", lib_cmd_kill_npc},
  {"fight %character%", lib_cmd_fight_npc},

  /* More movement, waiting, and miscellaneous administrative commands. */
  {"[goto/go {to}] %text%", lib_cmd_go_room},
  {"[goto/go {to}] *", lib_cmd_print_room_exits},
  {"[exit/exits/directions/where]", lib_cmd_print_room_exits},
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[wait] %number%", lib_cmd_wait_number},
  {"[wait]", lib_cmd_wait},
#else
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
  {"[redo /!]%number%", lib_cmd_redo_number},
  {"[redo /!]%text%", lib_cmd_redo_text},
  {"[redo/!]", lib_cmd_redo_last},
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[quit]", lib_cmd_quit},
#else
  {"[quit/q]", lib_cmd_quit},
#endif
  {"turns", lib_cmd_turns},
  {"score", lib_cmd_score},
  {"undo", lib_cmd_undo},
  {"[hist/history] %number%", lib_cmd_history_number},
  {"[hist/history]", lib_cmd_history},
  {"[hint/hints]", lib_cmd_hints},
  {"verbose", lib_cmd_verbose},
  {"brief", lib_cmd_brief},
  {"[notify/notification] %text%", lib_cmd_notify_on_off},
  {"[notify/notification]", lib_cmd_notify},
  {"time", lib_cmd_time},
  {"date", lib_cmd_date},
  {"[help/commands]", lib_cmd_help},
  {"[gpl/license]", lib_cmd_license},
  {"[about/info/information/author]", lib_cmd_information},
  {"[clear/cls/clr]", lib_cmd_clear},
  {"statusline", lib_cmd_statusline},
  {"status %character%", lib_cmd_status_npc},
  {"[status/stats]", lib_cmd_status_player},
  {"wield %object%", lib_cmd_wield},
  {"version", lib_cmd_version},

  {"[locate/where {is/are}/find] %object%", lib_cmd_locate_object},
  {"[locate/where {is}/find] %character%", lib_cmd_locate_npc},

  {"[count/num]", lib_cmd_count},

  /* Standard response commands; no real action, just output. */
  {"[get/take/pick up] *", lib_cmd_get_what},
  {"open *", lib_cmd_open_what},
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
  {"[shit/fuck/bastard/cunt/crap/hell/shag/bollocks/bollox/bugger] *",
   lib_cmd_profanity},
  {"[x/examine/look {at}] *", lib_cmd_examine_other},
  {"[locate/where {is/are}/find] *", lib_cmd_locate_other},
  {"[cp/mv/ln/ls] *", lib_cmd_unix_like},
  {"dir *", lib_cmd_dos_like},
  {"ask %character% *", lib_cmd_ask_npc},
  {"ask %object% *", lib_cmd_ask_object},
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
static scr_bool run_priority_pass_active = FALSE;
static scr_bool run_priority_deferred = FALSE;

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

static scr_bool
run_priority_commands (scr_gameref_t game, const scr_char *string)
{
  scr_commandsref_t command;

  run_priority_pass_active = TRUE;
  run_priority_deferred = FALSE;
  for (command = PRIORITY_COMMANDS; command->command; command++)
    {
      if (uip_match (command->command, string, game))
        {
          if (command->handler (game))
            {
              run_priority_pass_active = FALSE;
              return TRUE;
            }
          if (run_priority_deferred)
            break;
        }
    }
  run_priority_pass_active = FALSE;

  /* Nothing matched match the string.  Or if it did, its handler failed. */
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
  scr_bool eightpointcompass, is_movement = FALSE;
  scr_commandsref_t command;

  eightpointcompass = prop_get_global_boolean (bundle, "EightPointCompass");
  command = eightpointcompass ? MOVE_COMMANDS_8 : MOVE_COMMANDS_4;

  lib_set_movement_probe (TRUE);
  for (; command->command; command++)
    {
      if (uip_match (command->command, string, game))
        {
          if (command->handler (game))
            {
              is_movement = TRUE;
              break;
            }
        }
    }
  lib_set_movement_probe (FALSE);

  return is_movement;
}


static scr_bool
run_standard_commands (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2];
  scr_bool eightpointcompass;
  scr_commandsref_t command;

  /* Select the appropriate movement commands. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "EightPointCompass";
  eightpointcompass = prop_get_boolean (bundle, "B<-ss", vt_key);
  command = eightpointcompass ? MOVE_COMMANDS_8 : MOVE_COMMANDS_4;

  /*
   * Search movement commands first, returning TRUE if any matching command
   * handler succeeded.  Then repeat for standard library commands.
   */
  for (; command->command; command++)
    {
      if (uip_match (command->command, string, game))
        {
          if (command->handler (game))
            return TRUE;
        }
    }

  for (command = STANDARD_COMMANDS; command->command; command++)
    {
      if (uip_match (command->command, string, game))
        {
          if (command->handler (game))
            return TRUE;
        }
    }

  /* Nothing matched match the string.  Or if it did, its handler failed. */
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
  scr_vartype_t vt_key[2];
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
  vt_key[0].string = "Globals";
  vt_key[1].string = "StatusBox";
  statusbox = prop_get_boolean (bundle, "B<-ss", vt_key);
  if (statusbox)
    {
      /* Get the status line, and filter and untag it. */
      vt_key[1].string = "StatusBoxText";
      status = prop_get_string (bundle, "S<-ss", vt_key);
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
 * run_match_task_common() is called for every task on every player command,
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
 * run_match_task_common()
 * run_match_task_commands()
 * run_match_task_functions()
 *
 * Helpers for run_game_commands_common().
 *
 * Search task command for a match to the string passed in, returning TRUE
 * if a task command matches, FALSE otherwise.  Ordinary or reverse commands
 * are selected by 'forwards'.
 */
static scr_bool
run_match_task_common (scr_gameref_t game,
                       scr_int task, const scr_char *string, scr_bool forwards,
                       scr_bool is_library, scr_bool is_normal)
{
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, forwards);
  const scr_int command_count = (scr_int) patterns.size ();
  scr_int command;
  scr_bool is_matched;

  /* Iterate over commands, looking for patterns that match string. */
  is_matched = FALSE;
  for (command = 0; command < command_count; command++)
    {
      const scr_char *pattern;
      scr_int first;

      /* Retrieve the pattern for this command, find its first character. */
      pattern = patterns[command];
      first = strspn (pattern, WHITESPACE);

      /* Match using either the parser, or the special function matcher. */
      if (is_normal)
        {
          if (pattern[first] != SPECIAL_PATTERN)
            {
              /*
               * Make a special case of library calls and commands that begin
               * with a wildcard; these we ignore for this match attempt.
               */
              if (is_library && pattern[first] == WILDCARD_PATTERN)
                is_matched = FALSE;
              else
                is_matched = uip_match (pattern, string, game);
            }
        }
      else
        {
          if (pattern[first] == SPECIAL_PATTERN)
            is_matched = run_is_task_function (pattern, game);
        }

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

static scr_bool
run_match_task_commands (scr_gameref_t game,
                         scr_int task, const scr_char *string,
                         scr_bool forwards, scr_bool is_library)
{
  /*
   * Match tasks using the normal pattern matcher, with or without any note
   * about whether the call is from the library.
   */
  return run_match_task_common (game, task, string, forwards, is_library, TRUE);
}

static scr_bool
run_match_task_functions (scr_gameref_t game,
                          scr_int task, const scr_char *string, scr_bool forwards)
{
  /* Match tasks against "task command functions". */
  return run_match_task_common (game, task, string, forwards, FALSE, FALSE);
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
 * run_game_commands_common()
 * run_game_commands_in_parser_context()
 * run_game_commands_in_library_context()
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
 */
static scr_bool
run_game_commands_common (scr_gameref_t game, const scr_char *string,
                          scr_bool include_restrictions, scr_bool is_library)
{
  scr_bool is_matched = FALSE, is_handled = FALSE;
  scr_int task_count, task, direction;

  /*
   * Matching is expensive, so it helps to use a cache of results from the
   * first loop in the second.  If we're using the second, that is.  The cache
   * stays empty when restrictions are off (the second loop is then skipped).
   */
  task_count = gs_task_count (game);
  std::vector<scr_bool> is_matching;
  if (include_restrictions)
    is_matching.assign (task_count, FALSE);

  /*
   * Iterate over every task, ignoring those not runnable.  For each runnable
   * task, try matching task commands, and on matches, check restrictions and
   * if they pass, try running the task.
   */
  for (task = 0; task < task_count; task++)
    {
      if (!task_can_run_task (game, task))
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

          if (task_can_run_task_directional (game, task, is_forwards)
              && run_match_task_commands (game, task, string,
                                          is_forwards, is_library))
            {
              if (run_task_is_unrestricted (game, task))
                {
                  if (task_run_task (game, task, is_forwards))
                    is_handled = TRUE;
                  is_matched = TRUE;
                  break;
                }

              if (!is_matching.empty ())
                is_matching[task] = TRUE;
            }
        }
      if (is_matched)
        break;
    }

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
          if (!is_matching[task] || !task_can_run_task (game, task))
            continue;

          /*
           * Check matches of forwards and reverse commands.  If there's a
           * match for restricted tasks (ones that have and will print a fail
           * message if we try to run them), run the task to get the print of
           * the fail message, and we're done.
           */
          for (direction = 0; direction < 2; direction++)
            {
              const scr_bool is_forwards = !direction;

              if (task_can_run_task_directional (game, task, is_forwards)
                  && run_match_task_commands (game, task, string,
                                              is_forwards, is_library))
                {
                  if (run_task_is_loudly_restricted (game, task))
                    {
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
                                     scr_bool include_restrictions)
{
  /*
   * Try game commands, either with or without restrictions, and all full and
   * complete parse matching (no special case for game commands that begin
   * with a '*' wildcard).
   */
  return run_game_commands_common (game, string, include_restrictions, FALSE);
}

static scr_bool
run_game_commands_in_library_context (scr_gameref_t game, const scr_char *string)
{
  /*
   * Try game commands, including restrictions, and noting that this is a
   * library call so that the parse matcher can exclude game commands that
   * begin with a '*' wildcard.
   */
  return run_game_commands_common (game, string, TRUE, TRUE);
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
 */
scr_bool
run_does_command_match (scr_gameref_t game, const scr_char *string)
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
 * run_game_functions()
 *
 * Iterate over every task, ignoring those not runnable, searching just for
 * "task command functions".  These seem to happen in addition to any regular
 * command matches, so we try them as a separate action.
 */
static void
run_game_functions (scr_gameref_t game, const scr_char *string)
{
  scr_int task_count, task, direction;

  /* Iterate over every task, ignoring those not runnable. */
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      if (!task_can_run_task (game, task))
        continue;

      /*
       * Try matching forwards and reverse commands.  I don't know if it's
       * valid to put a function in a reverse command, but nevertheless...
       */
      for (direction = 0; direction < 2; direction++)
        {
          const scr_bool is_forwards = !direction;

          if (task_can_run_task_directional (game, task, is_forwards)
              && run_match_task_functions (game, task, string, is_forwards))
            {
              if (run_task_is_unrestricted (game, task))
                task_run_task (game, task, is_forwards);
            }
        }
    }
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
void
run_npc_walk_task (scr_gameref_t game, scr_int walktask)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key;
  scr_int version;

  vt_key.string = "Version";
  version = prop_get_integer (bundle, "I<-s", &vt_key);

  if (version < TAF_VERSION_400)
    run_task_command_dispatch (game, walktask);
  else if (task_can_run_task_directional (game, walktask, TRUE))
    task_run_task (game, walktask, TRUE);
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
void
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
  scr_vartype_t vt_key;
  scr_int version;

  vt_key.string = "Version";
  version = prop_get_integer (bundle, "I<-s", &vt_key);
  if (version > TAF_VERSION_380)
    return FALSE;

  return run_movement_succeeds (game, string);
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
 * Both count as a turn, unlike DontUnderstand: in the probe, an event ticking
 * once a turn fires on either refusal and not on the parser complaint, matching
 * the `handled = 1` the Runner sets alongside the message.  Hence the TRUE
 * return, which lets the caller run the turn.  (Measured for all three pre-4.0
 * answers; 4.0's RepeatText is assumed to tick as well, its probe having no
 * event in it.)
 *
 * The leading word follows Perspective, which pre-4.0 has only two of: run390
 * answers "I can't do that here!" / "I have already done that." for Perspective
 * 0 and the "You" forms for every other value, third person being a 4.0
 * addition (its inventory says "You are carrying nothing." for Perspective 2 as
 * well).
 */
enum { REFUSAL_NONE = 0, REFUSAL_ROOM, REFUSAL_DONE };

static scr_bool
run_task_refusal (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  scr_vartype_t vt_key[3];
  scr_int version, perspective, task_count, task, direction;
  scr_int refusal, refused_task;
  const scr_char *repeattext;

  /*
   * An empty input line element is not a command and gets no complaint of any
   * kind -- the same guard the DontUnderstand fallback uses.  Without it a
   * game with a bare "*" task command outside the player's room turns every
   * press-a-key blank line into a refusal.
   */
  if (scr_strempty (string))
    return FALSE;

  vt_key[0].string = "Version";
  version = prop_get_integer (bundle, "I<-s", vt_key);

  /*
   * Look for the first task in list order whose command matches the input and
   * that the dispatcher passed over for one of the two refusable reasons.  A
   * task blocked by anything else can never raise a refusal.
   */
  refusal = REFUSAL_NONE;
  refused_task = -1;
  task_count = gs_task_count (game);
  for (task = 0; task < task_count && refusal == REFUSAL_NONE; task++)
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
                  refusal = REFUSAL_ROOM;
                  refused_task = task;
                  break;
                }
            }
        }

      if (refusal == REFUSAL_NONE
          && task_is_done_refused (game, task)
          && run_match_task_commands (game, task, string, TRUE, FALSE))
        {
          refusal = REFUSAL_DONE;
          refused_task = task;
        }
    }
  if (refusal == REFUSAL_NONE)
    return FALSE;

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
 * Alternative facets of run_commands_common().  The first is used by the
 * main user input handling loop; the latter by the library when looking for
 * game commands that override standard actions.
 */
static scr_bool
run_all_commands (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool status;

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
   * Here's the current process:  First, run game commands, ignoring any
   * cases where restrictions fail to let the task run.  Next, try "priority"
   * system commands; ones that move objects to inventory.  These system
   * commands will call back into trying game commands for objects taken or
   * dropped, and in those tries, allow overrides only if the game task is
   * explicit about what it's doing (that is, doesn't start with "*"), and
   * handle restrictions in those tries.  After that, retry all game commands
   * again with restrictions enabled.  And finally, try all other standard
   * library commands.
   *
   * TODO This is the fourth or fifth attempt at getting this to match the
   * Runner, which is surprisingly inconsistent in this area.  What on earth
   * is the real behavior supposed to be?
   */
  /*
   * The carrying-capacity accounting toggle is exposed as a Glk port command
   * ("glk capacity"), handled in the front end before input ever reaches the
   * interpreter, so there is no administrative meta-command to match here.
   */
  status = run_game_commands_in_parser_context (game, string, FALSE);
  if (!status)
    status = run_priority_commands (game, string);
  if (!status && !run_defer_loud_tasks_to_movement (game, string))
    status = run_game_commands_in_parser_context (game, string, TRUE);
  if (!status)
    status = run_standard_commands (game, string);
  if (!status)
    status = run_task_refusal (game, string);

  /*
   * For version 4.0 games, it seems that if any command succeeded, we need
   * need to scan for and run any matching "task command functions", in
   * addition to anything done above.
   */
  if (status && !game->is_admin)
    {
      scr_vartype_t vt_key;
      scr_int version;

      /* Check "task command functions" for version 4.0 only. */
      vt_key.string = "Version";
      version = prop_get_integer (bundle, "I<-s", &vt_key);
      if (version == TAF_VERSION_400)
        run_game_functions (game, string);
    }

  return status;
}

scr_bool
run_game_task_commands (scr_gameref_t game, const scr_char *string)
{
  return run_game_commands_in_library_context (game, string);
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
        if_read_line (line_buffer, sizeof (line_buffer));
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
   * If filtering didn't replace synonyms, or no pronouns were replaced, use
   * the original line element.
   */
  command = replaced ? scr_normalize_string (replaced.get ())
            : (filtered ? scr_normalize_string (filtered.get ()) : line_element);

  /*
   * Echo the rewritten command, but only when a pronoun fired.  Upstream SCARE
   * echoed it for synonyms too, which is noise the Runner never prints and which
   * exposes authoring the player is not meant to see: "The Warlord, The Princess
   * & The Bulldog" routes "i"/"inv"/"inventory" through a synonym to the keyword
   * its inventory task listens for, so every "i" answered with a bare "[iii]".
   * A pronoun is different -- "it" and "her" are ambiguous by nature, and the
   * echo is the only way the player learns what they bound to.
   */
  if (replaced)
    {
      if_print_tag (SCR_TAG_ITALICS, "");
      if_print_character ('[');
      if_print_string (command);
      if_print_character (']');
      if_print_tag (SCR_TAG_ENDITALICS, "");
      if_print_character ('\n');
    }

  /* Try the command line element against command matchers. */
  status = run_all_commands (game, command);
  if (!status)
    {
      /* Only complain on non-empty command input line elements. */
      if (!scr_strempty (command))
        {
          const scr_char *message;

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

  vt_key[0].string = "Globals";
  vt_key[1].string = "PromptName";
  if (!prop_get_boolean (bundle, "B<-ss", vt_key))
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
 */
static void
run_prompt_player_gender (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[2];
  scr_int gender;

  vt_key[0].string = "Globals";
  vt_key[1].string = "PlayerGender";
  gender = prop_get_integer (bundle, "I<-ss", vt_key);

  /* Only an Unknown (2) gender needs a choice; Male (1)/Female (0) are set. */
  if (gender != 2)
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
          gender = 1;
          break;
        }
      if (*reply == 'f' || *reply == 'F')
        {
          gender = 0;
          break;
        }
      pf_buffer_string (filter, "Please answer \"male\" or \"female\".\n");
    }

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
      vt_key[0].string = "Globals";
      vt_key[1].string = "GameName";
      gamename = prop_get_string (bundle, "S<-ss", vt_key);
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
      vt_key[0].string = "Globals";
      vt_key[1].string = "DispFirstRoom";
      disp_first_room = prop_get_boolean (bundle, "B<-ss", vt_key);
      if (disp_first_room)
        lib_cmd_look (game);

      /* Handle any introductory resources. */
      vt_key[0].string = "Globals";
      vt_key[1].string = "IntroRes";
      res_handle_resource (game, "ss", vt_key);

      /* Set initial values for NPC and object states. */
      npc_setup_initial (game);
      obj_setup_initial (game);

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
       */
      evt_finish_load_events (game);
      evt_tick_events (game);

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

              /* Update NPC and object states. */
              npc_turn_update (game);
              obj_turn_update (game);

              /* Note the current room as visited. */
              gs_set_room_seen (game, gs_playerroom (game), TRUE);

              /* Give the debugger a chance to catch watchpoints. */
              debug_turn_update (game);
            }
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
