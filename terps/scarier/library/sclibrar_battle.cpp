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
 * Battle System attacks and the who/with question continuations.
 *
 * See sclibrar.h for what the library files share and sclibrar_internal.h
 * for what the core files share.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"

/*
 * lib_battle_player_strike()
 *
 * Shared Battle System helper: enforce a method verb's weapon-method
 * requirement, then deliver the blow with the given weapon (-1 for bare
 * hands).  method is -1 for a generic attack, or a method code 0..5
 * (chop/cut/hit/shoot/stab/throw) that the weapon must match; verb names the
 * action for the mismatch message.  No requirement is imposed when striking
 * bare-handed, so an unarmed method verb lands a plain blow (Runner-verified).
 */
static void
lib_battle_player_strike (scr_gameref_t game, scr_int npc,
                          const scr_char *verb, scr_int method, scr_int weapon)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (method >= 0 && weapon >= 0
      && battle_weapon_method (game, weapon) != method)
    {
      /*
       * dobattle ASSIGNS this one (run390 44D079, run400 47EED3), where the
       * blows and the "can't attack X with Y" refusal append, so several
       * targets leave a single copy and it wipes whatever the line printed
       * before it: `hit guard` with the chopping sword and two guards in the
       * room is one "You can't hit with the sword!" (run390x
       * runner_probes/batt.run390.txt).
       */
      pf_empty (filter);
      /* Ary(0) & " can't " & verb (run390 44D041, run400 47EE9C): the
         perspective subject, so a third-person game reads "Player can't
         shoot with the sword!" (run400x p4BATTLEWPN,
         runner_probes/battlewpn.run400.withq.txt, 2026-09-25). */
      lib_print_response_message (game, "You can't ", "I can't ",
                                  "%player% can't ");
      pf_buffer_string (filter, verb);
      lib_print_wrapped_object (game, " with ", weapon, "!\n");
      return;
    }

  battle_player_attack (game, npc, weapon);
}

/*
 * lib_battle_attack_bare()
 * lib_battle_attack_with()
 *
 * Cores for the player's attack commands, respectively without and with an
 * explicit weapon.  verb names the action and method is its required weapon
 * method (-1 for a generic attack).  With the Battle System enabled a real
 * blow is resolved.  Otherwise: a "legacy" verb (one with a traditional combat
 * grammar) prints the usual flavour response, while a verb introduced solely
 * for the Battle System falls through, leaving non-battle games' behaviour for
 * these words exactly as it was.
 */
/*
 * lib_npc_named_in_line()
 *
 * The Runner's "does this line refer to that character" test, as the two
 * absent-NPC answers below make it: whole-word containment of the record's
 * Name (field 0) or of its FIRST Alias (field 8) in the typed line, and
 * nothing else -- no prefix, no second alias, no parse position.  run390
 * dobattle spells it out at 44D130/44D14B (`c(global_0) Or c(global_8)`),
 * and characters()' catch-all repeats it at 45AC24/45AC4D.
 *
 * ALEXIS.TAF's forest dwelling goblin is why this matters: its Name is
 * "Forester Goblin" and `attack goblin` names it only by the alias.
 */
scr_bool
lib_npc_named_in_line (scr_gameref_t game, scr_int npc, const scr_char *input)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  const scr_char *name;

  name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
  if (name && name[0] != NUL && lib_input_contains_word (input, name))
    return TRUE;

  {
    const scr_char *alias = lib_first_alias (bundle, vt_key, "NPCs", npc);

    if (alias && alias[0] != NUL && lib_input_contains_word (input, alias))
      return TRUE;
  }

  return FALSE;
}

/*
 * lib_npc_referenced()
 *
 * characters()' per-NPC gate.  run400 Proc_21_40_45E99C(index, 1) accepts
 * the Name or ANY alias as a whole word of the line; run390's characters()
 * tests the Name or the first Alias only (lib_npc_named_in_line()).
 * the_pk_girl's peddler is Named "the peddler" with aliases man, peddler,
 * so only the 4.0 test finds him in `ask peddler about silo`.
 */
scr_bool
lib_npc_referenced (scr_gameref_t game, scr_int npc, const scr_char *input)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  const scr_char *name;
  scr_int alias_count, alias;

  if (!lib_is_version_400 (game))
    return lib_npc_named_in_line (game, npc, input);

  name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
  if (!scr_strempty (name) && lib_input_contains_word (input, name))
    return TRUE;

  alias_count = lib_alias_prepare (bundle, vt_key, "NPCs", npc);
  for (alias = 0; alias < alias_count; alias++)
    {
      const scr_char *word;

      vt_key[3].integer = alias;
      word = prop_get_string (bundle, "S<-sisi", vt_key);
      if (!scr_strempty (word) && lib_input_contains_word (input, word))
        return TRUE;
    }

  return FALSE;
}

/*
 * lib_co_400_line_leaves_which_pending()
 *
 * The object analogue of lib_npc_400_line_names_namesakes().  run400's
 * characters() (Proc_19_0_480674) has two loops calling co(object, 0)
 * (Proc_21_39_46486C) for EVERY object.  The one at 480180 sits inside the
 * examine arm (47FE63-48022A), which needs an examine verb and no task run
 * for the line, so on the task-ran lines this rule serves it never runs.
 * The other is the `give` branch (48022F-480384): c("give") and an NPC in
 * the player's room, not gated by the task-ran flag.  Each call picks the
 * object's name word
 * (lib_co_400_name_word()) and counts the present, seen objects answering to
 * it.  Exactly one sets the pending-disambiguation index Me(424) =
 * MemVar_4941EC back to -1 (46485E).  Two or more take the "Which" arm at
 * 464560, which leaves it at -2 (4645D4) or at the object (464767).  A word
 * nobody present answers to leaves it alone.  So the last object whose word
 * is on the line decides.  Anything but -1 skips the tick at 48B5B5.  With a
 * task run for the line, 48B60C prints the task's text and asks nothing.
 *
 * The decompiler writes these sentinels as `&HFF`/`&HFE`.  They are
 * LitI2_Byte, sign-extended into an Integer, which is why co() tests
 * `Me(424) < 0`.
 *
 * Measured on Cyberclones II (4.00): `give electric uniform to lightning`,
 * with the Fire Uniform worn and the Electric Uniform held, runs task 6 and
 * draws nothing that turn (runner_probes/cyber2.run400.[abc].txt).  `poke toy`
 * in p4TAMB.taf, with two present toys and no NPC, IS a turn.
 *
 * British Fox (4.00) `welsh fox rub tits` (synonym -> touch) runs task 44
 * with Welsh Fox's tits and the player's own tits both present, and IS a
 * turn: run400x's Me(424) watchpoint shows no write at all on the line
 * (runner_probes/britishfox.run400.watch.txt /
 * runner_probes/britishfox.run400.watch_trace.txt, 2026-09-24), where `x tits`
 * asks "Which tits." -- the named-NPC scan was an over-reach.
 *
 * Not modelled: the arm's 454454 prefix contest can hand the write to a
 * namesake with more Prefix words typed, which leaves the index at -1 only
 * if an earlier one-namesake word already reset it.
 */
static scr_bool lib_co_400_scan_objects_pending (scr_gameref_t game,
                                                 const scr_char *line);

scr_bool
lib_co_400_line_leaves_which_pending (scr_gameref_t game, const scr_char *line)
{
  const scr_int room = gs_playerroom (game);
  scr_bool scans;
  scr_int npc;

  if (!line || !lib_is_version_400 (game))
    return FALSE;

  scans = FALSE;
  if (!lib_input_contains_word (line, "give"))
    return FALSE;
  for (npc = 0; npc < gs_npc_count (game) && !scans; npc++)
    {
      if (npc_in_room (game, npc, room))
        scans = TRUE;
    }
  if (!scans)
    return FALSE;

  return lib_co_400_scan_objects_pending (game, line);
}

/*
 * The co(object, 0) loop itself, over every object in index order from a
 * Me(424) of -1; see lib_co_400_line_leaves_which_pending() above for what
 * each call does.  Returns TRUE when the loop leaves the index at an object
 * or at -2, so that a "Which" question is pending.
 */
static scr_bool
lib_co_400_scan_objects_pending (scr_gameref_t game, const scr_char *line)
{
  scr_bool pending;
  scr_int object;

  pending = FALSE;
  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *word = lib_co_400_name_word (game, object, line);
      scr_int count;

      if (!word)
        continue;
      count = lib_co_400_present_namesakes (game, word);
      if (count == 1)
        pending = FALSE;
      else if (count > 1)
        pending = TRUE;
    }
  return pending;
}

/*
 * lib_openclose_with_half_400()
 *
 * Why `take stone with knife` is a turn and `take stone` is not, both
 * answered "Which stone.  The red stone or the blue stone?" (p4WTIE,
 * run400 runner_probes/wtie.run400.1[1-7].txt and
 * runner_probes/wtie.run400.watch.txt, 2026-09-20).  get_piece's
 * prompt leaves Me(424) = MemVar_4941EC at the pending object and returns
 * FALSE from get_outer, so generaltasks goes on down its handler list to
 * openclose (Proc_19_44_476468, called at 48A515), and openclose begins
 * every line holding the whole word "with" the same way (475C63-475D6C):
 *
 *   var_88 = -1
 *   If c("with") Then var_88 = 463640(Right(line, Len(line) -
 *                                      InStr(1, line, "with")), 0, 0)
 *   If var_88 < 0 Then For each object: If co(object, 0) Then
 *       If InStr(line, Short) > InStr(line, "with") Then var_88 = object
 *
 * The scorer's restart label (4630BC) writes Me(424) = -1 before it looks
 * at a thing, so a tail after "with" that names one seen object -- the
 * knife, the box, the rope, the held coin -- leaves the index at -1 and
 * the loop is skipped: the tick test at 48B5B5 passes and 48B60C prints
 * the buffer as it stands, registering no question.  A tail that names
 * nothing (`take stone with zzz`, `take stone with`) or two namesakes
 * (`take knife with stone`) sends the loop over EVERY object, and co()
 * puts the index back: one present seen namesake for an object's word on
 * the line resets it to -1, two or more set it to the object, the last
 * object named in index order deciding -- so `take ruby with stone` (the
 * ruby, index 6, after the stones) ticks and `take rope with stone` (the
 * rope, index 0, before them) does not.  Watched on the run400x
 * MemVar_4941EC watchpoint (VBRNG_WATCH=4941EC, wtie_watch_trace.txt):
 * the last write on `take stone with knife` is the 5 -> -1 of 463640
 * called from 475CB5, and on `take stone with zzz` the -1 -> 4 -> 5 of
 * co() called from 475CF4, then the register tail 48BBF3.
 *
 * Only a prompt raised BEFORE openclose is exposed: take (get_outer
 * 48A46D), drop and put (put_drop_list 48A462), wear and remove.  therest's
 * own crowd (`cut stone with knife`) is raised after it and stays a
 * question, and so does the generaltasks scan's.  The Runner's tail runs
 * from the character after the "w" of the first "with" in the line (InStr,
 * not a whole-word find); "ith" scores nothing, so the text after the word
 * is what counts.
 *
 * Returns TRUE when openclose left Me(424) at -1: the line is a turn and no
 * question stands.
 */
scr_bool
lib_openclose_with_half_400 (scr_gameref_t game, const scr_char *line)
{
  const scr_char *with;

  if (!line || !lib_is_version_400 (game)
      || !lib_input_contains_word_400 (line, "with"))
    return FALSE;

  with = strstr (line, "with");
  if (!with)
    return FALSE;
  if (lib_with_half_400 (game, with + 1) >= 0)
    return TRUE;

  return !lib_co_400_scan_objects_pending (game, line);
}

/*
 * lib_openclose_with_antecedent_400()
 *
 * openclose's loop runs for every line that reaches it, question or not,
 * and co()'s -2 and park arms hand the antecedent setter a name as they go
 * (46460F, 464788).  So `cut rope with stone` leaves "it" at the bare
 * "stone" and `cut red stone with stone` at "a red stone", even where the
 * line's own question is somebody else's (therest's crowd).  The open and
 * close arms come first and Exit Sub when the whole line scores nothing
 * (4756BC, 4759E6): `open box with stone` never reaches the loop and leaves
 * "it" where it was (p4WTIE3, run400 runner_probes/wtie3.run400.it2.txt,
 * 2026-09-21).
 *
 * Notes the antecedent only; the question is
 * lib_openclose_with_half_raise_400()'s.
 */
void
lib_openclose_with_antecedent_400 (scr_gameref_t game, const scr_char *line)
{
  std::vector<scr_int> marked;
  const scr_char *with;
  scr_int object, me, last_tied, mark_count;
  scr_bool list_ok;

  if (!line || !lib_is_version_400 (game) || uip_pronoun_was_used ()
      || !lib_input_contains_word_400 (line, "with"))
    return;

  if ((lib_input_contains_word_400 (line, "open")
       || lib_input_contains_word_400 (line, "close"))
      && lib_name_object_resolve_400 (game, line, 0, &me, &last_tied,
                                      &marked, &mark_count) < 0)
    return;

  with = strstr (line, "with");
  if (!with)
    return;
  marked.clear ();
  object = lib_name_object_resolve_400 (game, with + 1, 0, &me, &last_tied,
                                        &marked, &mark_count);
  if (object >= 0)
    return;
  if (object != -1)
    me = -1;
  list_ok = (scr_int) marked.size () == mark_count;

  GS_FOR_EACH_OBJECT (game, object)
    lib_co_400_walk_step (game, object, line, &me, &marked, &list_ok);
}

/*
 * lib_openclose_with_half_raise_400()
 *
 * And where openclose's loop leaves Me(424) at an object, generaltasks ASKS
 * (48B6B1), whatever the handlers printed -- unless a task ran for the line
 * (48B60C), the line was claimed above openclose (tasks, put_drop_list,
 * get_outer: GoTo 48B4E3), or therest, which runs only while nothing has
 * been said and scores the line's halves again with 463640's restart,
 * decided the index after it (lib_with_split_crowd_400()).  The list is
 * Me(428) as the tail's 463640 left it and co() rebuilt it, the term the
 * pending object's Short replaced by the last of its own aliases the line
 * holds.  p4WTIE2, the red stone aliased "flint" and the blue "pebble", the
 * ruby and the emerald both "gems" (run400 runner_probes/wtie2.run400.19.txt
 * and runner_probes/wtie2.run400.20.txt, 2026-09-21):
 *
 *   x rope with stone         Which stone.  (the stones park after the rope)
 *   x ruby with stone         A plain thing.  (the ruby resets them, last)
 *   x emerald with stone      A plain thing.
 *   x pebble with stone       Which pebble.  The red stone or the blue stone?
 *   x flint with stone        Which stone.   (the blue stone is the one)
 *   x gems with stone         Which gems.  The ruby or the emerald?
 *   x stone with pebble       A plain thing.  (the tail names one)
 *   wear flint with stone     Which stone.  (not "not holding")
 *   take flint with stone     You take the red stone.  (get_outer claimed)
 *   zzz with stone            NO IDEA.  (therest: a head naming nothing)
 *
 * Returns TRUE when it asked.
 */
scr_bool
lib_openclose_with_half_raise_400 (scr_gameref_t game, const scr_char *line)
{
  std::vector<scr_int> marked;
  const scr_char *with;
  scr_int object, me, last_tied, mark_count;
  scr_bool list_ok;

  if (!line || !lib_is_version_400 (game) || disambig.element.therest_split
      || disambig.offered.flagged || disambig.element.named_raised
      || lib_co_400_question_pending ()
      || !lib_input_contains_word_400 (line, "with"))
    return FALSE;

  /*
   * openclose's own lock arm answers after the loop and leaves no question:
   * `unlock box with stone` and `unlock box with gems` unlock with the coin,
   * the box's key, and `lock box with stone` locks (p4WTIE,
   * runner_probes/wtie.run400.[1-4].txt).
   */
  if (lib_input_contains_word_400 (line, "lock")
      || lib_input_contains_word_400 (line, "unlock"))
    return FALSE;

  with = strstr (line, "with");
  if (!with)
    return FALSE;
  /* var_88: a tail naming one object skips the loop. */
  object = lib_name_object_resolve_400 (game, with + 1, 0, &me, &last_tied,
                                        &marked, &mark_count);
  if (object >= 0)
    return FALSE;
  if (object != -1)
    me = -1;
  list_ok = (scr_int) marked.size () == mark_count;

  GS_FOR_EACH_OBJECT (game, object)
    lib_co_400_walk_step (game, object, line, &me, &marked, &list_ok);

  if (me < 0 || !list_ok || marked.size () < 2)
    return FALSE;

  pf_empty (gs_get_filter (game));
  lib_co_400_raise (game, lib_drop_named_term_400 (game, me, line, TRUE),
                    marked);
  return TRUE;
}

/*
 * lib_battle_absent_npc()
 *
 * The 4.0 battle parser dobattle (Proc_11_4_47F084, entered from
 * generaltasks 48A4A2 whenever the Battle System is on) walks the NPCs in
 * index order and, for each whose Name is a whole word of the line
 * (47EB46), attacks it if it is in the player's room.  A named NPC who is
 * NOT here (47EF5E-47EFF4) prints "<Name> isn't here!" (47EFE5) when the
 * NPC has been seen (var_194(26) = 1), no earlier-named NPC was present
 * (var_92 = 0) and no NPC the line refers to is present (45E99C loop,
 * var_8A = 0).  Unlike the catch-all this is an ordinary turn: 494281 is
 * left alone, and the walk+event tick runs.
 *
 * Measured 2026-09-06 on Shadowpeak
 * (runner_probes/shadowpeak.run400.margo.txt): `attack margo with
 * sword` from the Torture Chamber, Margo seen and elsewhere, answers
 * "Margo isn't here!  Seeker hums!" -- the walker's line proves the tick.
 * The corpus's six `attack holga` lines are the same case.  Scarier used
 * to fall through to the catch-all ("I don't understand what you want me
 * to do with the body.") or the game's DontUnderstand text.
 *
 * 3.9 does the same, and the clause used to be 4.0-only only because
 * nothing had measured it.  run390's dobattle is @44D25C, and the branch at
 * 44D188..44D1BF is the same three tests in the same order -- seen
 * (var_158(26) = 1), no earlier-named NPC present (var_92 = 0), and an
 * inner loop over every NPC (44D10C..44D17A, testing the record's Name
 * *and* its first Alias) that clears var_8A when one the line refers to is
 * here -- ending in `Name & " isn't here!"` at 44D1B4.  The line's own
 * reference test is that same pair (lib_npc_named_in_line() above), which is
 * not the same thing as the Name alone: ALEXIS.TAF's `attack goblin` names
 * the "Forester Goblin" by its alias, and testing only the Name dropped the
 * answer through to the catch-all.  Measured against
 * the 2026-09-08 whole-corpus capture: ALEXIS.TAF driven with the cube
 * worn answers `attack wolf` with "Wolf isn't here!" from the rooms the
 * wolf has left, five turns of it (runner_probes/alexis_worn_cube.run390.txt
 * t17-19, t25-26), where Scarier said "Command not understood".  Before 3.9
 * there is no battle system at all -- neither run370.exe nor run380.exe
 * contains the string "doesn't seem to do any damage" -- so 3.90 is the floor.
 *
 * Returns TRUE having printed for every such NPC.
 */
static scr_bool
lib_battle_absent_npc (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int index_;
  scr_bool printed;

  if (prop_get_taf_version (bundle) < TAF_VERSION_390
      || !battle_is_enabled (game) || !input)
    return FALSE;

  printed = FALSE;
  GS_FOR_EACH_NPC (game, index_)
    {
      const scr_char *name;

      name = prop_get_indexed_string (bundle, "NPCs", index_, "Name");
      /*
       * 4.0 tests the Name alone.  Enigma's `kill orc guard`, with the Orc
       * guard present and the Goblin guard (Alias "guard") seen elsewhere,
       * is not "Goblin guard isn't here!" under run400x.
       */
      if (!name || name[0] == NUL
          || !(lib_is_version_400 (game)
               ? lib_input_contains_word (input, name)
               : lib_npc_named_in_line (game, index_, input))
          || !gs_npc_seen (game, index_)
          || npc_in_room (game, index_, gs_playerroom (game)))
        continue;

      pf_buffer_string (filter, name);
      pf_buffer_string (filter, " isn't here!\n");
      printed = TRUE;
    }

  return printed;
}

/*
 * lib_attack_absent_npc()
 *
 * With the Battle System OFF, characters()' per-NPC attack branch answers a
 * named NPC who is elsewhere.  run400 Proc_19_0_480674, inside its
 * `If Proc_21_40_45E99C(index, 1)` (the line names the NPC by Name or any
 * alias and no present, seen namesake shares the word), 47F40D-47F70B: the
 * line must hold one of hit/kill/kick/punch/attack as a whole word, no task
 * ran (4941F8 = 0), battle off (494282 = 0), and the buffer must be empty
 * (or end ", but nothing happens.", or hold " can't see ").  An NPC in the
 * player's room gets the avoids/with arms; one elsewhere, with the buffer
 * still empty, gets `Proc_21_3_446BB4(Name) & " is not here!"` (47F700) --
 * the Name with its first letter capitalised.  No seen test, and an ordinary
 * turn: the Runner ticks.  run390's twin in characters() @45ACD8 tests Name
 * or first Alias and prints the Name raw (45960F).
 *
 * Measured 2026-09-14 on the_pk_girl under run400x
 * (runner_probes/thepkgirl.run400.site.txt): Chadwick, Named "~the
 * ~[CH=%know_chadwick%]Chadwick" and elsewhere, answers `attack chadwick` with
 * "The man is not here!", where Scarier said the game's "Pardon me?".  3.9 is
 * from the decompile alone; 3.7/3.8's per-verb sites (run370 43865D, run380
 * 4404D9) are lib_hit_arm_pre390().
 *
 * Returns TRUE having printed for the first such NPC.
 */
static scr_bool
lib_attack_absent_npc (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_bool is_400 = lib_is_version_400 (game);
  scr_int index_;

  if (prop_get_taf_version (bundle) < TAF_VERSION_390
      || battle_is_enabled (game) || !input)
    return FALSE;

  if (!lib_input_contains_word (input, "hit")
      && !lib_input_contains_word (input, "kill")
      && !lib_input_contains_word (input, "kick")
      && !lib_input_contains_word (input, "punch")
      && !lib_input_contains_word (input, "attack"))
    return FALSE;

  /*
   * 4.0's absent arm (47F6E8) needs the buffer still empty, and therest's
   * checkverb arms (4455F8, 48940C-4896A0) have already filled it for any
   * line holding hit/kick/push/pull/press/shake, so the nothing-happens line
   * stands.  amnesiakid `kick tom`, Tom elsewhere: "You kick, but nothing
   * happens." (runner_transcripts/amnesiakid.txt).  run390 45960F overwrites.
   */
  if (is_400
      && (lib_input_contains_word (input, "hit")
          || lib_input_contains_word (input, "kick")
          || lib_input_contains_word (input, "push")
          || lib_input_contains_word (input, "pull")
          || lib_input_contains_word (input, "press")
          || lib_input_contains_word (input, "shake")))
    return FALSE;

  GS_FOR_EACH_NPC (game, index_)
    {
      const scr_char *name;

      name = prop_get_indexed_string (bundle, "NPCs", index_, "Name");
      if (scr_strempty (name)
          || !lib_npc_referenced (game, index_, input)
          || npc_in_room (game, index_, gs_playerroom (game)))
        continue;

      if (is_400)
        pf_new_sentence (filter);
      pf_buffer_string (filter, name);
      pf_buffer_string (filter, " is not here!\n");
      return TRUE;
    }

  return FALSE;
}

/*
 * dobattle's own reference test for a single target -- not ported.
 *
 * dobattle picks its target with its own reference test, not the parser's.
 * run400 walks the NPCs and tests the Name (field 0) alone -- 47EB2D pushes
 * var_194(0), LCases it and asks Proc_21_38_454CB0 for a whole word of the
 * line, and nothing between there and the in-room test at 47EBA7 looks at
 * an alias.  run390's outer loop at 44CC1C tests the Name and then the first
 * Alias (44CC57 on var_158(0), 44CCC5 on var_158(8)), the same pair as
 * lib_npc_named_in_line().  A line that names no NPC that way ends with
 * var_8A = 0 and "Who do you want to attack?" (run400 47F01A, run390
 * 44D1E9), and 494281 is left alone, so it is a real turn.
 *
 * Measured 2026-09-13 on Shadowpeak under run400x
 * (runner_probes/shadowpeak.run400.txt turn 361): the witch's cat is Named
 * "Shadow", with aliases "cat", "black cat" and "shadow the black cat".
 * `attack cat with sword` in its room answers "Who do you want to attack?
 * Seeker hums!" -- the walker's line proves the tick -- where Scarier bound
 * the alias and killed the cat.
 *
 * Being named is not enough: the NPC is struck only when var_90 -- the
 * FIRST of dobattle's verbs, in its own order, that is a whole word of the
 * line -- comes before the name (run400 47EBC2-47EBC9, run390 44CD44), and
 * otherwise var_8A stays 0 and the same question follows, with its
 * continuation (47F025).  Ghoster's robot is Named "Attack Robot", so `kill
 * attack robot` takes "attack" as the verb, at the name's own position, and
 * the Runner answers "Who do you want to attack?" (runner_transcripts
 * ghoster turn 21); `attack attack robot` strikes.
 *
 * Deliberate deviation: Scarier strikes the character the grammar resolved,
 * by any Alias and whatever battle verb comes first, where the Runner
 * answers "Who do you want to attack?".  A line naming several characters
 * still goes through dobattle's test (lib_battle_strike_loop()).
 */
static scr_bool lib_battle_attack_many (scr_gameref_t game,
                                        scr_bool with_object);
static scr_bool lib_battle_line_names_many (scr_gameref_t game);
static void lib_battle_weapon_question (scr_gameref_t game, scr_int npc);
static void lib_battle_continue_after_kill (scr_gameref_t game, scr_int npc,
                                            const scr_char *verb);

/*
 * dobattle refuses a non-weapon with its only such message, 47EC7D (run390
 * " can't attack " in the same procedure): Ary(0) & " can't attack " &
 * Name & " with " & the object's mode-0 name, no full stop.  " is not a
 * weapon!" is wield's (47E93F), not attack's.  A real turn.  Measured
 * 2026-09-13 on p4BATTLEWPN (runner_probes/battlewpn.run400.txt): "Player
 * can't attack Gargoyle #3 with the rock".  Scarier deliberately closes the
 * sentence with a full stop (deviation policy).
 */
static void
lib_battle_cant_attack (scr_gameref_t game, scr_int npc, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *name;

  name = prop_get_indexed_string (gs_get_bundle (game), "NPCs", npc, "Name");
  pf_buffer_string (filter, lib_select_response (game,
                                                 "You can't attack ",
                                                 "I can't attack ",
                                                 "%player% can't attack "));
  pf_buffer_string (filter, name ? name : "");
  pf_buffer_string (filter, " with ");
  lib_print_object_np (game, object);
  pf_buffer_character (filter, '.');
  pf_buffer_answer_break (filter);
}

/*
 * lib_battle_attack_target()
 *
 * The opening the bare and the "with" attack share: the Battle System gate,
 * the hand-off of a line naming several characters, and the choice of the
 * one character attacked.  Returns TRUE when that settles the command, with
 * the command's result in *status; otherwise *npc is the target.
 */
static scr_bool
lib_battle_attack_target (scr_gameref_t game, const scr_char *verb,
                          scr_bool legacy, scr_bool is_with,
                          scr_int *npc, scr_bool *status)
{
  scr_bool is_ambiguous;

  /* A Battle-System-only verb defers to other grammar when battle is off. */
  *status = FALSE;
  if (!battle_is_enabled (game) && !legacy)
    return TRUE;

  /* A line naming several characters is dobattle's; lib_battle_attack_many(). */
  *status = TRUE;
  if (battle_is_enabled (game)
      && (lib_is_version_400 (game)
          ? lib_npc_400_find_namesakes (game, nullptr, nullptr)
          : lib_battle_line_names_many (game))
      && lib_battle_attack_many (game, is_with))
    return TRUE;

  /* Get the referenced npc, and if none, consider complete. */
  *npc = lib_disambiguate_npc_pick (game, verb, &is_ambiguous,
                                    battle_is_enabled (game)
                                    ? NPC_PICK_ASK : NPC_PICK_FIRST);
  if (*npc != -1)
    return FALSE;

  /* 3.9+: a seen NPC named in the line but elsewhere "isn't here!" */
  if (!is_ambiguous && lib_battle_absent_npc (game))
    return TRUE;
  /* Battle off, 3.9+: a named NPC elsewhere "is not here!" */
  if (!is_ambiguous && lib_attack_absent_npc (game))
    return TRUE;
  *status = is_ambiguous;
  return TRUE;
}

static scr_bool lib_battle_line_names_any_object (scr_gameref_t game,
                                                  const scr_char *input);

static scr_bool
lib_battle_attack_bare (scr_gameref_t game, const scr_char *verb,
                        scr_int method, scr_bool legacy)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int npc;
  scr_bool status;

  if (lib_battle_attack_target (game, verb, legacy, FALSE, &npc, &status))
    return status;

  /*
   * A "with" after the target (47EBDE, var_8A = 2) makes the blow the
   * with-loop's: nothing present named after it leaves dobattle silent
   * (the with row above declined the same line), and the line goes on to
   * the catch-alls.  See lib_battle_line_names_object().
   */
  if (battle_is_enabled (game)
      && prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390
      && run_get_dispatch_input ())
    {
      const scr_char *with = strstr (run_get_dispatch_input (), " with ");

      if (with && !lib_battle_line_names_any_object (game, with + 6))
        return FALSE;
    }

  /* With the Battle System enabled, resolve a real attack. */
  if (battle_is_enabled (game))
    {
      scr_int weapon = battle_player_wielded_weapon (game);

      /*
       * With no wield set the Runner auto-selects a solitary carried weapon
       * (the blow then persists it as the wield), fights bare-handed when
       * carrying none, and with two or more carried weapons asks -- a
       * question, always worded with "attack" whatever the verb, that costs
       * no combat turn in 4.0 (settled live 2026-08-01; run400's light_up
       * transcript, runner_transcripts/light_up.txt `attack higher`, shows
       * the NPC's blows only on the NEXT line).  The next line can answer
       * it; see lib_battle_weapon_question().
       *
       * In 3.9 the question IS a turn.  run390 dobattle 44CE85-44CEDF only
       * appends the question and parks the "attack <npc> with" prefix; it
       * never sets the not-a-turn byte MemVar_468219, and the tick gate
       * (generaltasks 460650-460672) needs nothing more than a non-empty
       * output buffer to call characters() and events().  Measured on
       * Govard (runner_transcripts/govard.txt, turn 213): "Чем мне атаковать
       * Волк with?" is followed on the same turn by the wolf's bite and the
       * events' output, where 4.0 would print the question alone.
       *
       * Deliberate deviation (2026-10-17): the question is free at every
       * version.  A missing byte store in run390 gave the NPC a free blow
       * while the game asked the player a question, which is how a player
       * reads it: a bug, not a rule.  Logged under "Deliberate deviations"
       * in test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md; govard's golden was
       * re-derived.
       */
      if (weapon < 0)
        {
          const scr_int count = battle_player_weapon_count (game);

          if (count > 1)
            {
              lib_battle_weapon_question (game, npc);
              game->is_admin = TRUE;
              return TRUE;
            }
          if (count == 1)
            weapon = battle_player_best_weapon (game);
        }
      lib_battle_player_strike (game, npc, verb, method, weapon);
      lib_battle_continue_after_kill (game, npc, verb);
      return TRUE;
    }

  /* Print a standard response. */
  pf_new_sentence (filter);
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                      " avoids your feeble attempts.\n",
                                      " avoids my feeble attempts.\n",
                                      " avoids %player%'s feeble attempts.\n"));
  return TRUE;
}

static scr_int lib_battle_scan_with (scr_gameref_t game, scr_int npc,
                                     const scr_char *input,
                                     scr_bool *refused);

static scr_bool
lib_battle_attack_with (scr_gameref_t game, const scr_char *verb,
                        scr_int method, scr_bool legacy)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, npc;
  scr_vartype_t vt_key[3];
  scr_bool weapon, status;

  if (lib_battle_attack_target (game, verb, legacy, TRUE, &npc, &status))
    return status;

  /*
   * dobattle (run390 44CD63-44CE44, run400 47EC16) walks every object the
   * text after " with " names, with no break, and the last weapon wins:
   * thesorc T246 `attack king with staff`, the long mage staff and the
   * Master Staff both held, strikes with the Master Staff
   * (runner_transcripts/thesorc.txt).  Asking would drop the blow.  Only the
   * text after " with ": the whole line refuses the seen static "shadow" in
   * Shadowpeak's `attack shadow with sword`.
   */
  object = -1;
  if (battle_is_enabled (game)
      && prop_get_taf_version (bundle) >= TAF_VERSION_390
      && run_get_dispatch_input ())
    {
      const scr_char *with = strstr (run_get_dispatch_input (), " with ");
      scr_bool refused = FALSE;

      if (with)
        object = lib_battle_scan_with (game, npc, with + 6, &refused);
      if (refused)
        return TRUE;
      /* A "with" naming nothing present prints nothing in dobattle, and
       * the line goes on to the catch-alls (lib_battle_line_names_object). */
      if (with && object == -1)
        return FALSE;
    }

  /* Get the referenced object, and if none, consider complete. */
  if (object == -1)
    object = lib_disambiguate_object (game, verb, nullptr);
  if (object == -1)
    return TRUE;

  /* dobattle tests the weapon flag before it looks for the object. */
  if (battle_is_enabled (game) && !battle_is_weapon (game, object))
    {
      lib_battle_cant_attack (game, npc, object);
      return TRUE;
    }

  /* Ensure the referenced object is held.  The Runner: "Player is not
   * carrying the rock!" (probe pWS2 -- unlike wield's "aren't carrying").
   * Battle off, pre-4.0 characters()' own with-loop says "<You> don't have
   * <the X>!" (run380 44047B, run370 4385FF): `hit dave with stone`, the
   * stone dropped (run370x runner_probes/npcamb.run370.with.rtf, run380x
   * runner_probes/npcamb.run380.with.rtf). */
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER
      && !battle_is_enabled (game) && !lib_is_version_400 (game))
    {
      lib_print_response_object (game, "You don't have ", "I don't have ",
                                 "%player% don't have ", object, "!\n");
      return TRUE;
    }
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
    {
      lib_print_response_object (game,
                                 "You are not carrying ",
                                 "I am not carrying ",
                                 "%player% is not carrying ",
                                 object, "!\n");
      return TRUE;
    }

  /* With the Battle System enabled, resolve a real attack with the weapon. */
  if (battle_is_enabled (game))
    {
      lib_battle_player_strike (game, npc, verb, method, object);
      lib_battle_continue_after_kill (game, npc, verb);
      return TRUE;
    }

  /* Check for static object moved to player by event. */
  if (obj_is_static (game, object))
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_plurality (game, object, " is", " are"));
      pf_buffer_string (filter, " not a weapon.\n");
      return TRUE;
    }

  /* Print standard response depending on if the object is a weapon. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Weapon";
  weapon = prop_get_boolean (bundle, "B<-sis", vt_key);
  if (weapon)
    {
      lib_print_response_npc (game,
                              "You swing at ",
                              "I swing at ",
                              "%player% swing at ",
                              npc, " with ");
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             " but you miss.\n",
                                             " but I miss.\n",
                                             " but misses.\n"));
    }
  else
    {
      /*
       * "affective" [sic] and the "!" are the Runner's: every Runner string
       * table, run370 through run400, carries "I don't think X would be a
       * very affective weapon!" verbatim (measured 2026-08-17).
       */
      lib_print_wrapped_object (game, "I don't think ",
                                object, " would be a very effective weapon!\n");
    }
  return TRUE;
}

/*
 * lib_battle_attack_many()
 *
 * One line, several targets.  dobattle's target loop (run400 47EB0E-47F006,
 * run390 44CC1C-44D1D5) has no break: every NPC the line names and who is in
 * the player's room runs the whole attack branch -- weapon choice, the "What
 * do you want to attack X with?" question, the blow -- and then falls to
 * `Next` (47EF5B GoTo 47EFF8).  The loop is in NPC index order, not the
 * order the names were typed, and an NPC counts only when the verb word
 * var_90 comes before its name in the line (47EBC9).  var_90 is the first of
 * attack, fight, kill, kick, chop, cut, hit, shoot, stab, throw that is a
 * whole word of the line (47E9EF-47EADB).
 *
 * Measured 2026-09-13 on harness/make_400_battlemultiprobe.py's second build
 * (p4BATTLEMULTI2: Guard with alias "sentry", Droid, Robot), run400x
 * runner_probes/battlemulti2.run400.txt:
 *
 *     attack droid guard       ->  Player shoot a sentry with the blaster.
 *                                  Player shoot Droid with the blaster.
 *     attack guard droid       ->  (the same, in the same order)
 *     attack robot droid guard ->  (Guard, Droid, Robot)
 *     attack sentry droid      ->  Player shoot Droid with the blaster.
 *     attack guard with droid  ->  I don't understand what you want to do
 *                                  with Guard.
 *     attack guard, droid      ->  (the splitter's two lines: Guard struck,
 *                                  then the catch-all on Droid)
 *
 * Scarier's grammar binds one %character%, so those lines never reached a
 * handler and went to the catch-all.  These two rows sit behind every
 * %character% row and take the line as text.  With no target they print
 * "Who do you want to attack?" unless an absent NPC is named; with one they
 * strike it; with several they ask which was meant and strike nobody until
 * the answer -- a deliberate deviation from the loop above, see
 * lib_battle_attack_many().
 */
static const struct
{
  const scr_char *const verb;
  const scr_int method;
} LIB_BATTLE_VERBS[] = {
  {"attack", -1}, {"fight", -1}, {"kill", -1}, {"kick", -1},
  {"chop", 0}, {"cut", 1}, {"hit", 2}, {"shoot", 3}, {"stab", 4},
  {"throw", 5}, {nullptr, 0}
};

static scr_bool
lib_battle_npc_is_target (scr_gameref_t game, scr_int npc,
                          const scr_char *input, scr_int verb_index,
                          const scr_char **named_by_out = nullptr)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *name, *named_by;

  name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
  named_by = nullptr;
  if (!scr_strempty (name) && lib_input_contains_word (input, name))
    named_by = name;
  else if (!lib_is_version_400 (game)
           && lib_npc_named_in_line (game, npc, input))
    {
      scr_vartype_t vt_key[4];

      named_by = lib_first_alias (bundle, vt_key, "NPCs", npc);
    }
  if (!named_by || !npc_in_room (game, npc, gs_playerroom (game)))
    return FALSE;

  if (named_by_out)
    *named_by_out = named_by;
  return lib_instr (input, LIB_BATTLE_VERBS[verb_index].verb)
         < lib_instr (input, named_by);
}

static std::vector<scr_int>
lib_battle_named_targets (scr_gameref_t game, const scr_char *input,
                          scr_int verb_index)
{
  std::vector<scr_int> targets;
  scr_int npc;

  GS_FOR_EACH_NPC (game, npc)
    {
      if (lib_battle_npc_is_target (game, npc, input, verb_index))
        targets.push_back (npc);
    }
  return targets;
}

/*
 * lib_battle_killed_task_line()
 *
 * run390's killchar (42D344-42D40C) dispatches an NPC's KilledTask by text:
 * when the record's task field (124) is set it stores the task's first
 * Command in MemVar_468118, the typed line, and calls tasks(1) (42D3E6).
 * dobattle's target loop reads that same variable for every NPC it tests
 * (the Name and Alias c() tests at 44CC57/44CCC5, the verb InStr at 44CD44),
 * so after a kill with a KilledTask the rest of the loop tests the task's
 * command, not what the player typed.  Outside (3.90), `attack first guard`
 * with both guards aliased "guard": the first guard dies, the line becomes
 * "thefirstprisonguardisdead", and the second guard is left for Joe
 * (runner_probes/outside.run390.trap.txt, turn 11).  run400's killchar
 * (44B0E5-44B0FD) runs the task by index and leaves the line alone.
 *
 * Returns the task's command when `npc` has just died with a KilledTask at
 * 3.9, else NULL.
 */
static const scr_char *
lib_battle_killed_task_line (scr_gameref_t game, scr_int npc)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4], vt_rvalue;
  scr_int task;

  if (lib_is_version_400 (game) || !gs_npc_dead (game, npc))
    return nullptr;

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Battle";
  vt_key[3].string = "KilledTask";
  if (!prop_get (bundle, "I<-siss", &vt_rvalue, vt_key))
    return nullptr;
  task = vt_rvalue.integer - 1;
  if (task < 0 || task >= gs_task_count (game))
    return nullptr;

  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "Command";
  vt_key[3].integer = 0;
  return prop_get_string (bundle, "S<-sisi", vt_key);
}

/*
 * lib_battle_line_names_many()
 *
 * TRUE when the line names two or more present characters by dobattle's own
 * test -- pre-4.0's stand-in for lib_npc_400_find_namesakes().  run390's
 * dobattle (44CC1C-44D1D5) has no break either, so `attack guard` with two
 * guards in the room strikes both, and there is no namesake question at 3.9
 * to take their place: SCARE's "Please be more clear, who do you want to
 * attack?" is an invention at every version.  Measured 2026-09-20 on
 * p39BATT (make_battlenpcprobe.py; Ann and Bob both "a guard"), run390x
 * runner_probes/batt.run390.txt: `attack/kill/kick guard` are two chops, `hit
 * guard` (the sword chops, so the method is wrong) one refusal, `attack guard
 * with stone` two "can't attack" refusals and `attack guard with club` one
 * "not carrying".  The one-target lines keep the %character% rows' own path.
 */
static scr_bool
lib_battle_line_names_many (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int verb_index;

  if (!input)
    return FALSE;

  for (verb_index = 0; LIB_BATTLE_VERBS[verb_index].verb; verb_index++)
    {
      if (lib_input_contains_word (input, LIB_BATTLE_VERBS[verb_index].verb))
        break;
    }
  if (!LIB_BATTLE_VERBS[verb_index].verb)
    return FALSE;

  return lib_battle_named_targets (game, input, verb_index).size () > 1;
}

/* TRUE if the line names, by dobattle's test, an NPC not in the room. */
static scr_bool
lib_battle_names_absent_npc (scr_gameref_t game, const scr_char *input)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int npc;

  GS_FOR_EACH_NPC (game, npc)
    {
      const scr_char *name = prop_get_indexed_string (bundle, "NPCs", npc,
                                                      "Name");
      scr_bool named;

      named = !scr_strempty (name) && lib_input_contains_word (input, name);
      if (!named && !lib_is_version_400 (game))
        named = lib_npc_named_in_line (game, npc, input);
      if (named && !npc_in_room (game, npc, gs_playerroom (game)))
        return TRUE;
    }
  return FALSE;
}


/*
 * lib_battle_who_*()
 *
 * "Who do you want to attack?" is a question the next line can answer.
 * dobattle's Who path (run400 47F01E, run390 44D1F4) leaves a line prefix in
 * MemVar_494234 (run390 MemVar_4681D0): the verb, then " with " and the
 * definite name (the mode-0 name builder) of every seen object the line
 * names, index order -- `attack with blaster` leaves "attack with the
 * blaster".  generaltasks consumes it at 48AFF3 (run390 460022): when nothing
 * has answered the line by then, the prefix is not the line itself and holds
 * no "|", the line becomes prefix & " " & line, the prefix is cleared, and
 * the handler runs again from 489FEB.  That test sits above the object and
 * character catch-alls and DontUnderstand, so a bare `gargoyle #1` or
 * `blaster` is continued, and a library command that answers is not.
 *
 * The prefix dies with the line after the one that raised it: 489FDA copies
 * it into var_98 for each freshly typed line (not for the split queue, which
 * re-enters at 489FEB), and the end of every element, 48B5FC, clears both
 * when the prefix still equals var_98.  So a Who that merely raises Who
 * again (`attack`, `attack`) is spent, `hit` then `kick` leaves "kick", and
 * `attack then gargoyle #2` answers its own question.  Measured 2026-09-13 on
 * p4BATTLEHASH (runner_probes/battlehash.run400.who.txt): 21 lines, 21 draws,
 * "You have taken 15 turns" and later 17.
 *
 * The weapon question at 47ED3E stores `"attack " & name & " with"` through
 * the same variable; that one is not ported.  Whether an empty line would be
 * continued is not measured, and it is not here.
 */
std::string lib_battle_who_pending;
static std::string lib_battle_who_at_line;
static scr_bool lib_battle_who_unanswered = FALSE;
static scr_bool lib_battle_400_claimed = FALSE;

void
lib_battle_who_reset (void)
{
  lib_battle_who_pending.clear ();
  lib_battle_who_at_line.clear ();
  lib_battle_who_unanswered = FALSE;
  lib_battle_400_claimed = FALSE;
}

/*
 * lib_battle_400_claim_line()
 * lib_battle_400_line_claimed()
 *
 * Set by lib_battle_attack_many() when it has struck the one target a line
 * naming several settled on, or asked which one was meant.  run_line_t::
 * settle() then leaves the line alone: generaltasks' namesake question
 * would wipe the blow, or the question, and ask "Which guard.  A guard or
 * a guard?" over it -- the Runner's outcome, which the deviation replaces
 * (see the comment in lib_battle_attack_many()).  Cleared with each
 * element.
 */
void
lib_battle_400_claim_line (void)
{
  lib_battle_400_claimed = TRUE;
}

scr_bool
lib_battle_400_line_claimed (void)
{
  return lib_battle_400_claimed;
}

/*
 * lib_battle_which_pending()
 *
 * TRUE while lib_battle_ask_which_target()'s question is open, so that the
 * answer -- a bare `guard`, which at 4.0 names the very namesakes asked
 * about -- is left to lib_battle_who_continuation() rather than to the
 * scan's own "Which guard." (run_line_t::settle()).
 */
scr_bool
lib_battle_which_pending (void)
{
  return lib_battle_who_pending.compare (0, 2, "||") == 0;
}

/* The prefix between two lines, for a Spatterlight autosave. */
void
lib_battle_who_get_prefix (std::string *pending, std::string *at_line)
{
  *pending = lib_battle_who_pending;
  *at_line = lib_battle_who_at_line;
}

void
lib_battle_who_set_prefix (const std::string &pending,
                           const std::string &at_line)
{
  lib_battle_who_reset ();
  lib_battle_who_pending = pending;
  lib_battle_who_at_line = at_line;
}

/*
 * Called before each element is dispatched.  The previous element's 48B5FC
 * clear goes first, then a freshly typed line takes its copy of the prefix.
 */
void
lib_battle_who_begin_element (scr_bool new_line)
{
  if (!lib_battle_who_at_line.empty ()
      && lib_battle_who_pending == lib_battle_who_at_line)
    {
      lib_battle_who_pending.clear ();
      lib_battle_who_at_line.clear ();
    }
  if (new_line)
    lib_battle_who_at_line = lib_battle_who_pending;
  lib_battle_who_unanswered = FALSE;
  lib_battle_400_claimed = FALSE;
}

/*
 * The object and character catch-alls answer lines here that the Runner
 * reaches only after 48AFF3, so they leave the line open to the prefix.
 */
void
lib_battle_who_note_unanswered (void)
{
  lib_battle_who_unanswered = TRUE;
}

void
lib_battle_who_store (const std::string &pending)
{
  lib_battle_who_pending = pending;
}

/* The line to run instead, or empty when the prefix does not apply. */
std::string
lib_battle_who_continuation (const scr_char *command, scr_bool status)
{
  std::string rerun;
  size_t bar;

  if (lib_battle_who_pending.empty () || scr_strempty (command)
      || (status && !lib_battle_who_unanswered)
      || lib_battle_who_pending == command)
    return rerun;

  /*
   * A 3.9 "Which <term>.  <list>?" left Short & "|" & line
   * (lib_co_ambiguity_prompt).  run390 460022-460188: the term is cut off
   * the front, the line is what remains, and where the line holds the term
   * the answer goes in its place -- followed by the term itself unless the
   * answer already has it as a word (c(), 46010A) -- and the rest of the
   * line after it.  A line without the term takes the plain prefix form
   * (4601A5), line & " " & answer.
   */
  /*
   * lib_battle_ask_which_target()'s own form, "||" & before & "|" & after:
   * the answer goes between the two halves, `attack` + `bob` + ` with club`.
   */
  if (lib_battle_who_pending.compare (0, 2, "||") == 0)
    {
      const size_t mid = lib_battle_who_pending.find ('|', 2);
      const std::string before (lib_battle_who_pending.substr (2, mid - 2));
      const std::string after (mid == std::string::npos
                               ? "" : lib_battle_who_pending.substr (mid + 1));

      lib_battle_who_pending.clear ();
      return before + " " + command + after;
    }

  bar = lib_battle_who_pending.find ('|');
  if (bar != std::string::npos)
    {
      const std::string term (lib_battle_who_pending.substr (0, bar));
      const std::string line (lib_battle_who_pending.substr (bar + 1));
      size_t at = line.find (term);

      lib_battle_who_pending.clear ();
      if (at == std::string::npos)
        return line + " " + command;

      rerun = line.substr (0, at) + command;
      if (!lib_co_contains (command, term.c_str ()))
        rerun += " " + term;
      rerun += " " + line.substr (at + term.length ());
      return rerun;
    }

  rerun = lib_battle_who_pending + " " + command;
  lib_battle_who_pending.clear ();
  return rerun;
}

/*
 * lib_with_prefix_390_*()
 *
 * 3.90's own question prefix, and no other Runner's: both of therest's
 * "With what?" answers -- the two-object split's (45D1A0) and the with-arm's
 * (45D3E0) -- store `Left(line, InStr(line, "with") + 4)`, the line cut just
 * past the word, the instrument half thrown away.  It is not the prefix
 * lib_battle_who_pending models: what it continues is the NEXT line that is
 * not understood on its own, run as `<prefix><line>` through therest only --
 * never the task matcher -- and what spends it is a line that anything
 * answers.
 *
 * Measured on p*WITHPFX (make_withprefixprobe.py;
 * runner_probes/withpfx.run390.wr.txt, runner_probes/withpfx.run390.wt.txt,
 * runner_probes/withpfx.run390.pfx6.txt, runner_probes/withpfx.run390.wv.txt,
 * 2026-09-20).  With task 15 wired as `fff with ggg`:
 *
 *   fff / with zzz / ggg          "With what?" twice, PFX5. never printed
 *   fff / with zzz / gem          "I don't ... do with the gem!" -- the
 *                                 with-arm on `fff with gem`
 *   hhh gem / with zzz / rock     "You don't have the rock." -- the split
 *                                 on `hhh gem with rock`, two objects named
 *   hhh gem / with zzz / gem      "With what?" again: the gem's first
 *                                 occurrence in the joined line is BEFORE
 *                                 "with", so the arm has nothing to name
 *   fff / with zzz / probe        the task answers and the prefix is gone;
 *                                 `ggg` after it is "I don't understand."
 *   fff / with zzz / cut rock     therest answers and the prefix is gone
 *   fff / with zzz / push | i     a bare verb or an inventory likewise
 *
 * The prefix lives exactly one line, like every other, but a retry that ends
 * in "With what?" stores it again from the line it just ran -- and since the
 * cut is at the FIRST "with", that is the same string -- which is how
 * `hhh gem` / `with zzz` / `ggg` / `rock` still reaches the rock.
 */
static std::string lib_with_prefix_390;
static scr_bool lib_with_prefix_390_fresh = FALSE;

void
lib_with_prefix_390_reset (void)
{
  lib_with_prefix_390.clear ();
  lib_with_prefix_390_fresh = FALSE;
}

/* The prefix between two lines, for a Spatterlight autosave. */
void
lib_with_prefix_390_get (std::string *pending)
{
  *pending = lib_with_prefix_390;
}

void
lib_with_prefix_390_set (const std::string &pending)
{
  lib_with_prefix_390 = pending;
  lib_with_prefix_390_fresh = FALSE;
}

void
lib_with_prefix_390_begin_element (void)
{
  lib_with_prefix_390_fresh = FALSE;
}

/* Called where therest prints "With what?", on the line as it was run. */
void
lib_with_prefix_390_note (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *with;

  if (!lib_is_version_390 (game) || !input)
    return;
  with = strstr (input, "with");
  if (!with)
    return;

  lib_with_prefix_390 = std::string (input, with - input + 4);
  if (input[with - input + 4] == ' ')
    lib_with_prefix_390 += ' ';
  lib_with_prefix_390_fresh = TRUE;
}

/* The line to run instead, or empty when the prefix does not apply. */
std::string
lib_with_prefix_390_continuation (const scr_char *command, scr_bool status)
{
  std::string rerun;

  if (lib_with_prefix_390.empty () || scr_strempty (command)
      || (status && !lib_battle_who_unanswered)
      || lib_with_prefix_390 == command)
    return rerun;

  rerun = lib_with_prefix_390;
  if (rerun[rerun.size () - 1] != ' ')
    rerun += ' ';
  rerun += command;
  lib_with_prefix_390.clear ();
  return rerun;
}

/* End of the element: a line that did not end in "With what?" spends it. */
void
lib_with_prefix_390_end_element (void)
{
  if (!lib_with_prefix_390_fresh)
    lib_with_prefix_390.clear ();
}

/*
 * "Wear what?" (run400 463C19) and "Remove what?" (462477) leave the typed
 * line itself, MemVar_494174, in the same prefix (463C23, 462481), so
 * `wear zzz` then `goggles` runs `wear zzz goggles` and puts them on, and
 * `remove zzz` then `wield zzz` answers "Remove what?" again.  The other
 * 4.0 questions measured beside them do not: `drop zzz` and `take zzz` then
 * `goggles` are the object catch-all.  Measured 2026-09-14 on ptbad.taf
 * (runner_probes/tbad.run400.whatcont.txt and
 * runner_probes/tbad.run400.probe3.txt).  run390's wears sets its prefix too
 * (43D289) and its removes does not; neither is measured, so this stays 4.0.
 */
void
lib_question_prefix_from_line (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();

  if (lib_is_version_400 (game) && input)
    lib_battle_who_pending = input;
}

/*
 * "Give what?" (488A7C), "Give <the obj> to who?" (488B45) and checkverb's
 * "<Verb> what?" for a line that IS the verb (4455F8) store the line the same
 * way; bare `drop` and `take` do not.  And every line, whoever answered it,
 * passes 48B4E3-48B530: a message that is "With what?" or whose Right 5 is
 * "with?" stores line & " with " and sets the not-a-turn byte MemVar_494281.
 * So a task printing "What with?" to `saw rope` does not tick, and `knife`
 * then runs `saw rope with  knife` -- two spaces, which no task command
 * matches at 4.0.  "Whittle it with what?" is neither and ticks.  Measured
 * 2026-09-14 on p4WITHQ.taf (harness/make_400_withqprobe.py;
 * runner_probes/withq.run400.txt, runner_probes/withq.run400.2.txt).  Returns
 * TRUE when the line is not a turn for it, which is 4.0 only; the prefix half
 * is 3.9's too (see inside).
 */
scr_bool
lib_question_with_rule (scr_gameref_t game, const scr_char *line)
{
  const scr_char *buffer = pf_get_buffer (gs_get_filter (game));
  std::string message;

  if (!buffer || scr_strempty (line)
      || prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;

  message = buffer;
  while (!message.empty ()
         && (message.back () == '\n' || message.back () == ' '))
    message.pop_back ();
  if (message != "With what?"
      && (message.size () < 5
          || message.compare (message.size () - 5, 5, "with?") != 0))
    return FALSE;

  /*
   * run390's twin, 460589-4605D2 at the tail of generaltasks, makes the
   * same test and stores the same line & " with " into MemVar_4681D0
   * (4605CE) -- the one prefix variable, which is why it lands on top of
   * dobattle's parked "attack <name> with" (44CEDF) exactly as 48B530 lands
   * on 47ED3E -- but never touches the not-a-turn byte MemVar_468219, so at
   * 3.9 the line that asked is a turn.  Measured 2026-09-25 on p39WITHQ
   * (make_39_withqprobe.py, run390x runner_probes/withq.run390.txt): a task
   * printing "What with?" / "With what?" / "What do you want to cut it with?"
   * ticks, `knife` then runs `saw rope with  knife` -- two spaces, and at 3.9
   * the task matcher sees them: the task wired with two spaces fires, the
   * one-space twin does not -- and `shoot robot` / `sword` is "You can't shoot
   * with the sword!", the verb the line typed, not dobattle's "attack".
   * run400x on p4BATTLEWPN (harness/make_400_battlewpnprobe.py,
   * runner_probes/battlewpn.run400.withq.txt) refuses
   * the same way, so the overwrite is both Runners'.  3.7/3.8 have no
   * "with?" literal at all.
   */
  lib_battle_who_pending = std::string (line) + " with ";
  return lib_is_version_400 (game);
}

/* checkverb's bare verb: "<Label> what?" and the line as the prefix. */
scr_bool
lib_checkverb_bare_400 (scr_gameref_t game, const scr_char *verb,
                        const scr_char *label)
{
  const scr_char *input = run_get_dispatch_input ();

  if (!lib_is_version_400 (game) || !input
      || scr_strcasecmp (input, verb) != 0)
    return FALSE;

  pf_buffer_string (gs_get_filter (game), label);
  pf_buffer_string (gs_get_filter (game), " what?\n");
  lib_battle_who_pending = input;
  return TRUE;
}

/* The mode-0 name, as the Runner spells it from 3.9 on.  This one is a
   command line re-parsed next turn, not text, so it keeps the Runner's
   spelling where lib_print_object_np() deliberately tidies it. */
static std::string
lib_battle_definite_name (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *prefix, *name;
  std::string result;

  prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");
  name = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!prefix)
    prefix = "";
  if (lib_compare_article_binary (prefix, "a", 1))
    result = std::string ("the") + (prefix + 1);
  else if (lib_compare_article_binary (prefix, "an", 2))
    result = std::string ("the") + (prefix + 2);
  else if (lib_compare_article_binary (prefix, "some", 4))
    result = std::string ("the") + (prefix + 4);
  else
    result = prefix;
  result += " ";
  result += name ? name : "";
  return result;
}

/*
 * dobattle asks co() about each object (47EC16, run390 44CDB0), and co()
 * resolves only a PRESENT object -- obhere(), and from 3.9 seen as well
 * (464360-46437E; the tail test 4647C5-464819 is obhere And seen at mode
 * 0).  A seen weapon lying elsewhere is not named at all, so dobattle
 * prints nothing for it and the line falls to the catch-all's seen+absent
 * arm, a turn: wonderland T7-T10 `attack card guard with knife`, the
 * ethereal knife two rooms back, is "You must be in the same room as the
 * ethereal knife to be able to do anything with it." with the Card Guard's
 * blow appended (runner_transcripts/wonderland.txt, 2026-09-25), not
 * dobattle's "not carrying", which is for a present weapon not held (the
 * dropped rock of probe pWS2).
 */
static scr_bool
lib_battle_line_names_object (scr_gameref_t game, scr_int object,
                              const scr_char *input)
{
  return lib_co_candidate (game, object, gs_playerroom (game))
         && gs_object_seen (game, object)
         && lib_verb_object_name_score (game, object, input) > 0;
}

static void
lib_battle_who_raise (scr_gameref_t game, const scr_char *input,
                      const scr_char *verb)
{
  scr_int object;

  lib_battle_who_pending = verb;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (lib_battle_line_names_object (game, object, input))
        {
          lib_battle_who_pending += " with ";
          lib_battle_who_pending += lib_battle_definite_name (game, object);
        }
    }
}

/*
 * "What do you want to attack X with?" (47ED0B) leaves its own prefix in the
 * same variable, `"attack " & LCase(Name) & " with"` (47ED3E), so `sword`
 * next runs as `attack gargoyle #2 with sword`.  Several targets each ask,
 * and the last one's prefix stands.  Measured 2026-09-13 on p4BATTLEWPN
 * (blaster, sword and rock held, nothing wielded), run400x
 * runner_probes/battlewpn.run400.txt: `attack gargoyle #2` / `sword` strikes
 * with the sword, and the sword stays wielded; `attack gargoyle #3` / `rock`
 * is the rock's refusal, a turn; `kick gargoyle #3` / `nonsense words` is the
 * character catch-all; `look`, a repeated question and `turns` spend the
 * prefix as they do Who's.  17 draws both sides.
 */
static void
lib_battle_weapon_question (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *name;
  std::string lower;

  pf_buffer_string (filter, "What do you want to attack ");
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter, " with?\n");

  name = prop_get_indexed_string (gs_get_bundle (game), "NPCs", npc, "Name");
  for (const scr_char *cursor = name ? name : ""; *cursor != NUL; cursor++)
    lower += scr_tolower (*cursor);
  lib_battle_who_pending = "attack " + lower + " with";
}

static scr_bool
lib_battle_line_names_any_object (scr_gameref_t game, const scr_char *input)
{
  scr_int object;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if (lib_battle_line_names_object (game, object, input))
        return TRUE;
    }
  return FALSE;
}

/*
 * A "with" the object row could not bind: run400 47EC16 walks every object
 * the line names, for each target.  A weapon becomes the weapon, with no
 * break, so the last one named wins; anything else is refused on the way
 * (lib_battle_cant_attack()).  Returns the weapon, or -1.
 *
 * That walk is measured on thesorc, a 3.90 game, and 4.0 is NOT the same.
 * Measured over 68 cases in run400
 * (runner_probes/illegalsocks_patched.run400.socks6.txt /
 * runner_probes/illegalsocks_patched.run400.socks7.txt /
 * runner_probes/battlew*.run400.txt, 2026-09-27), 4.0 binds the weapon like
 * this: a word matches a Short case-SENSITIVELY but an Alias or a Prefix word
 * without regard to case, always whole; an object is a candidate only if its
 * Short or an Alias matched, never a Prefix alone; it scores the number of
 * distinct input words it answers to; the highest score wins, a tie going to
 * the last such object; and the winner is then REFUSED if every word it
 * matched is also answered by another object, or its matched Alias is another
 * object's name, or it matched by Alias while another candidate exists.  A
 * refusal is turn-free, and the catch-all naming it comes from a second,
 * ordinary resolver (lib_verb_object_name_score()'s shape), whose own ties
 * name nothing and so give the character catch-all.  The full record -- the
 * variant table, every measurement and the predictions that confirmed it --
 * is test/adrift4/harness/make_400_battlewith5probe.py.
 *
 * Deliberate deviation: Scarier keeps the 3.90 walk at 4.0 as well.  The 4.0
 * rule is a parser accident rather than a design -- it makes an author's own
 * naming unreachable (a Short the .taf capitalises can never be typed, and an
 * object whose Alias is its neighbour's Short can never be wielded), so
 * implementing it would refuse commands the game was written to accept.  The
 * visible cost is small and one-directional: where run400 dies turn-free in a
 * catch-all we ask "Which Sword?" and swing on the repeat, i.e. we accept
 * strictly more phrasings and never fewer.  Illegal Socks is the one corpus
 * game it shows up in, and its golden plays `with sword2`, which binds under
 * both models (test/adrift4/notes/IllegalSocks_walkthrough.md).
 */
static scr_int
lib_battle_scan_with (scr_gameref_t game, scr_int npc, const scr_char *input,
                      scr_bool *refused)
{
  scr_int object, weapon;

  weapon = -1;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (!lib_battle_line_names_object (game, object, input))
        continue;
      if (battle_is_weapon (game, object))
        weapon = object;
      else
        {
          lib_battle_cant_attack (game, npc, object);
          *refused = TRUE;
        }
    }
  return weapon;
}

/*
 * lib_battle_400_namesake_tail()
 *
 * 4.0 asks about namesakes only AFTER dobattle has run, and the question
 * replaces everything the line printed; the blows themselves stand.  (In
 * Scarier the blows no longer happen on such a line -- lib_battle_attack_many()
 * asks first, a deliberate deviation -- so this tail serves the Who path
 * alone: a line naming namesakes by an alias at 4.0, below.)
 * Measured 2026-09-13 on p4BATTLEMULTI: `attack guard` against the two
 * stamina-500 Guards prints only "Which Guard.  A guard or a guard?" yet
 * draws for two blows (runner_probes/battlemulti.run400.which_a.txt), and
 * against a stamina-1 copy (p4BATTLEMULTI3) it prints both blows and both
 * deaths with no question at all, the Guards being gone by the time
 * generaltasks looks for them (runner_probes/battlemulti3.run400.which_k.txt).
 * `attack droid guard with blaster` then `look` leaves the room empty
 * (runner_probes/battlemulti3.run400.which_l.txt).  Whether the question still
 * makes the line a turn is not measured; it is left admin, as the object
 * question is.
 *
 * It replaces the Who question too, which is how a line naming namesakes by
 * an ALIAS reads at 4.0: dobattle names its targets by Name alone (47EB46),
 * so `attack guard` against Ann and Bob, both aliased "guard", finds none
 * and asks Who -- and generaltasks then wipes it (p4BATT, run400x
 * runner_probes/batt.run400.txt, 2026-09-20).
 */
static void
lib_battle_400_namesake_tail (scr_gameref_t game)
{
  if (lib_is_version_400 (game)
      && lib_npc_400_find_namesakes (game, nullptr, nullptr))
    {
      pf_empty (gs_get_filter (game));
      lib_npc_400_raise_for_line (game);
    }
}

/*
 * lib_battle_prefix_contest()
 *
 * 4.0's Prefix contest over the line's battle targets: the character whose
 * own Prefix words the line holds most of is THE target, a tie keeps the
 * crowd, and no Prefix word at all keeps everyone.  Scarier holds it at
 * every version, as lib_disambiguate_npc_pick() does for %character%, so
 * `attack red riven` with Red Riven and Blue Riven present is one blow.
 */
static void
lib_battle_prefix_contest (scr_gameref_t game, std::vector<scr_int> *targets,
                           const scr_char *input)
{
  std::vector<scr_int> kept;
  scr_int best, index_;

  best = 0;
  for (index_ = 0; index_ < (scr_int) targets->size (); index_++)
    best = std::max (best, lib_npc_400_prefix_score (game,
                                                     (*targets)[index_],
                                                     input));
  if (best <= 0)
    return;
  for (index_ = 0; index_ < (scr_int) targets->size (); index_++)
    {
      if (lib_npc_400_prefix_score (game, (*targets)[index_], input) == best)
        kept.push_back ((*targets)[index_]);
    }
  *targets = kept;
}

/*
 * lib_battle_npc_signature()
 * lib_battle_dedupe_targets()
 *
 * Everything an answer could single a character out by, lower-cased: its
 * Name and Prefix, and before 4.0 its aliases too (dobattle names its
 * targets by Name alone at 4.0, 47EB46, and the 4.0 question's answer is
 * settled by the Prefix contest, so an alias there helps nobody:
 * p4BATTLEMULTI's `sentry` answered to "Which Guard.  A guard or a guard?"
 * only asks again).  Two characters with the same signature can never be
 * told apart, so of each such set only the first by index is kept: a line
 * naming two identical Guards and a Droid asks "Guard or Droid?", and
 * `guard` then strikes the first Guard, as a pre-4.0 `hit guard` is settled
 * (NPC_PICK_FIRST), and `attack guard` alone strikes it without a question.
 */
static std::vector<std::string>
lib_battle_npc_signature (scr_gameref_t game, scr_int npc)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  std::vector<std::string> words;
  scr_vartype_t vt_key[4];
  const scr_char *text;
  scr_int alias_count, alias;

  text = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
  words.push_back (text ? text : "");
  text = prop_get_indexed_string (bundle, "NPCs", npc, "Prefix");
  words.push_back (text ? text : "");
  alias_count = lib_is_version_400 (game)
                ? 0 : lib_alias_prepare (bundle, vt_key, "NPCs", npc);
  for (alias = 0; alias < alias_count; alias++)
    {
      vt_key[3].integer = alias;
      text = prop_get_string (bundle, "S<-sisi", vt_key);
      words.push_back (text ? text : "");
    }
  for (auto &word : words)
    scr_lowercase (word);
  return words;
}

/*
 * lib_battle_remove_word()
 *
 * Take every whole-word occurrence of `word` (one or more words, matched
 * without case) out of `line`, leaving single spaces.
 */
static void
lib_battle_remove_word (std::string *line, const scr_char *word)
{
  const size_t length = word ? strlen (word) : 0;
  size_t at;

  if (length == 0)
    return;
  for (at = 0; at + length <= line->size (); )
    {
      const scr_bool starts = at == 0 || (*line)[at - 1] == ' ';
      const scr_bool ends = at + length == line->size ()
                            || (*line)[at + length] == ' ';

      if (starts && ends
          && scr_strncasecmp (line->c_str () + at, word, length) == 0)
        line->erase (at, length);
      else
        at++;
    }
  while ((at = line->find ("  ")) != std::string::npos)
    line->erase (at, 1);
  while (!line->empty () && line->back () == ' ')
    line->pop_back ();
  while (!line->empty () && line->front () == ' ')
    line->erase (0, 1);
}

static void
lib_battle_dedupe_targets (scr_gameref_t game, std::vector<scr_int> *targets)
{
  std::vector<std::vector<std::string>> seen;
  std::vector<scr_int> kept;
  scr_int index_;

  for (index_ = 0; index_ < (scr_int) targets->size (); index_++)
    {
      const std::vector<std::string> signature
        = lib_battle_npc_signature (game, (*targets)[index_]);

      if (std::find (seen.begin (), seen.end (), signature) != seen.end ())
        continue;
      seen.push_back (signature);
      kept.push_back ((*targets)[index_]);
    }
  *targets = kept;
}

/*
 * lib_battle_prefer_named_targets()
 *
 * Pre-4.0 names a target by its Name or its first alias.  When a line
 * names some targets by Name and others only by alias, the Name wins:
 * the answer `ann` to "Who do you want to attack?  Ann or Bob?" re-runs
 * as `attack ann guard`, which names Ann by Name and Bob by his alias
 * "guard" -- and must not ask again.
 */
static void
lib_battle_prefer_named_targets (scr_gameref_t game,
                                 std::vector<scr_int> *targets,
                                 const scr_char *input, scr_int verb_index)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  std::vector<scr_int> by_name;
  scr_int index_;

  for (index_ = 0; index_ < (scr_int) targets->size (); index_++)
    {
      const scr_int npc = (*targets)[index_];
      const scr_char *named_by = nullptr, *name;

      lib_battle_npc_is_target (game, npc, input, verb_index, &named_by);
      name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
      if (named_by && name && scr_strcasecmp (named_by, name) == 0)
        by_name.push_back (npc);
    }
  if (!by_name.empty () && by_name.size () < targets->size ())
    *targets = by_name;
}

/*
 * lib_battle_ask_which_target()
 *
 * The question a line naming several present targets gets instead of the
 * Runner's blows.  At 4.0, when the targets are exactly the namesakes the
 * end-of-turn scan would have asked about, it is that scan's own question
 * -- "Which riven?  Red riven or Blue riven?" -- and its answer slot: the
 * answer words are spliced in before the term and the line re-run
 * (lib_co_400_object_answer_line()), where the Prefix contest settles it.
 *
 * Otherwise -- pre-4.0, or a line naming two different characters
 * (`attack droid guard`) -- it is "Who do you want to attack?" with the
 * targets listed, each by Name, or by Prefix and name where two share a
 * Name.  The answer is carried by the Who prefix in its own form, "||" &
 * before & "|" & after (lib_battle_who_continuation()): the line up to and
 * including the verb, then the rest of it with every target's naming word
 * and the "and"s between them taken out.  `attack ann bob with club` / `bob`
 * runs `attack bob with club`; `attack guard` / `ann` runs `attack ann`.
 * (The 3.9 `term|line` splice keeps the term, and would name everyone
 * again.)
 *
 * Not a turn at any version: nobody was struck, and the Runner has no
 * such question to keep parity with.
 */
static void
lib_battle_ask_which_target (scr_gameref_t game, const scr_char *input,
                             scr_int verb_index,
                             const std::vector<scr_int> &targets)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *verb = LIB_BATTLE_VERBS[verb_index].verb;
  std::string before, after;
  scr_int index_;
  size_t at;

  game->is_admin = TRUE;

  if (lib_is_version_400 (game))
    {
      std::vector<scr_int> namesakes, all_targets;
      std::string unused;

      /* The scan's namesakes against every target the line names, before
       * the dedupe: it lists identical namesakes itself. */
      all_targets = lib_battle_named_targets (game, input, verb_index);
      lib_battle_prefix_contest (game, &all_targets, input);
      if (lib_npc_400_find_namesakes (game, &unused, &namesakes))
        {
          std::sort (namesakes.begin (), namesakes.end ());
          std::sort (all_targets.begin (), all_targets.end ());
          if (namesakes == all_targets && lib_npc_400_raise_for_line (game))
            return;
        }
    }

  /* The line up to the verb, and the rest of it without the targets. */
  before = input;
  after.clear ();
  for (at = 0; at < before.size (); at++)
    {
      const size_t length = strlen (verb);

      if ((at == 0 || before[at - 1] == ' ')
          && scr_strncasecmp (before.c_str () + at, verb, length) == 0
          && (at + length == before.size () || before[at + length] == ' '))
        {
          after = before.substr (at + length);
          before.erase (at + length);
          break;
        }
    }
  if (at >= before.size ())
    {
      before = verb;
      after.clear ();
    }
  for (index_ = 0; index_ < (scr_int) targets.size (); index_++)
    {
      const scr_char *named_by = nullptr;

      lib_battle_npc_is_target (game, targets[index_], input, verb_index,
                                &named_by);
      if (named_by)
        lib_battle_remove_word (&after, named_by);
    }
  lib_battle_remove_word (&after, "and");
  lib_battle_who_pending = "||" + before + "|"
                           + (after.empty () ? "" : " " + after);
  lib_battle_400_claim_line ();

  pf_buffer_string (filter, "Who do you want to ");
  pf_buffer_string (filter, verb);
  pf_buffer_string (filter, "?  ");
  pf_new_sentence (filter);
  for (index_ = 0; index_ < (scr_int) targets.size (); index_++)
    {
      const scr_char *name, *prefix;
      scr_bool shared;
      scr_int other;

      if (index_ > 0)
        pf_buffer_string (filter,
                          index_ == (scr_int) targets.size () - 1
                          ? " or " : ", ");

      /* By Name, as the room lists them; Prefix and name where two share
       * one, as the 4.0 question lists them ("Red riven"). */
      name = prop_get_indexed_string (bundle, "NPCs", targets[index_], "Name");
      shared = FALSE;
      for (other = 0; other < (scr_int) targets.size (); other++)
        {
          const scr_char *other_name;

          if (other == index_)
            continue;
          other_name = prop_get_indexed_string (bundle, "NPCs",
                                                targets[other], "Name");
          if (name && other_name && scr_strcasecmp (name, other_name) == 0)
            shared = TRUE;
        }
      prefix = prop_get_indexed_string (bundle, "NPCs", targets[index_],
                                        "Prefix");
      if (shared && !scr_strempty (prefix))
        {
          std::string lower (name ? name : "");

          scr_lowercase (lower);
          pf_buffer_string (filter, prefix);
          pf_buffer_character (filter, ' ');
          pf_buffer_string (filter, lower.c_str ());
        }
      else
        lib_print_npc_np (game, targets[index_]);
    }
  pf_buffer_string (filter, "?\n");
}

/*
 * lib_battle_strike_loop()
 *
 * dobattle's target loop from NPC `start` on.  It tests each NPC as it
 * reaches it, against `line`: the typed line until a 3.9 kill swaps in its
 * KilledTask's command (lib_battle_killed_task_line()), after which "with"
 * and the weapons are read from that command too (44CD66, the co() walk at
 * 44CDB0).  `replaced` says `line` is already such a command.
 */
static void
lib_battle_strike_loop (scr_gameref_t game, scr_int verb_index,
                        scr_int start, const scr_char *start_line,
                        scr_bool replaced, scr_bool with_object,
                        scr_int object, scr_bool scan,
                        scr_bool *struck, scr_bool *refused,
                        const std::vector<scr_int> *only = nullptr)
{
  const scr_filterref_t filter = gs_get_filter (game);
  std::string line = start_line;
  scr_int index_;

  for (index_ = start; index_ < gs_npc_count (game); index_++)
    {
      const scr_int npc = index_;
      scr_int weapon;
      const scr_char *killed_line;

      if (!lib_battle_npc_is_target (game, npc, line.c_str (), verb_index))
        continue;
      /* The one target the typed line settled on; a 3.9 KilledTask's
       * command, once swapped in, is tested as the Runner tests it. */
      if (only && !replaced
          && std::find (only->begin (), only->end (), npc) == only->end ())
        continue;
      if (replaced)
        {
          with_object = FALSE;
          scan = lib_input_contains_word (line.c_str (), "with")
                 && lib_battle_line_names_any_object (game, line.c_str ());
        }

      if (with_object || scan)
        {
          if (scan)
            weapon = lib_battle_scan_with (game, npc, line.c_str (),
                                           refused);
          else if (!battle_is_weapon (game, object))
            {
              lib_battle_cant_attack (game, npc, object);
              *refused = TRUE;
              weapon = -1;
            }
          else
            weapon = object;
          if (weapon < 0)
            continue;
          if (gs_object_position (game, weapon) != OBJ_HELD_PLAYER)
            {
              /* 4.0 appends this one (47EF41); 3.9 assigns it (44D0E7), so
               * two targets leave one "You are not carrying the club!". */
              if (!lib_is_version_400 (game))
                pf_empty (filter);
              lib_print_response_object (game,
                                         "You are not carrying ",
                                         "I am not carrying ",
                                         "%player% is not carrying ",
                                         weapon, "!\n");
              *refused = TRUE;
              continue;
            }
        }
      else
        {
          weapon = battle_player_wielded_weapon (game);
          if (weapon < 0)
            {
              const scr_int count = battle_player_weapon_count (game);

              if (count > 1)
                {
                  lib_battle_weapon_question (game, npc);
                  continue;
                }
              if (count == 1)
                weapon = battle_player_best_weapon (game);
            }
        }
      lib_battle_player_strike (game, npc, LIB_BATTLE_VERBS[verb_index].verb,
                                LIB_BATTLE_VERBS[verb_index].method, weapon);
      *struck = TRUE;

      killed_line = lib_battle_killed_task_line (game, npc);
      if (killed_line)
        {
          line = killed_line;
          replaced = TRUE;
        }
    }
}

/*
 * lib_battle_continue_after_kill()
 *
 * A one-target blow is still one pass of that same loop: when it killed an
 * NPC with a KilledTask at 3.9, the loop goes on past it against the task's
 * command.  thenightmoon T23 `attack elf with longsword` kills the Dark elf
 * (NPC 9), whose KilledTask "drow giving in" brings the injured Drow (NPC
 * 10, Named "Drow") into the library; the loop reaches it, finds "drow" in
 * that line with no "attack" before it, and strikes: run390 adds "You hit
 * injured dark elf with your longsword." (runner_transcripts/
 * thenightmoon.txt).
 */
static void
lib_battle_continue_after_kill (scr_gameref_t game, scr_int npc,
                                const scr_char *verb)
{
  const scr_char *killed_line = lib_battle_killed_task_line (game, npc);
  scr_int verb_index;
  scr_bool struck = FALSE, refused = FALSE;

  if (!killed_line || !battle_is_enabled (game))
    return;
  for (verb_index = 0; LIB_BATTLE_VERBS[verb_index].verb; verb_index++)
    {
      if (scr_strcasecmp (LIB_BATTLE_VERBS[verb_index].verb, verb) == 0)
        break;
    }
  if (!LIB_BATTLE_VERBS[verb_index].verb)
    return;
  lib_battle_strike_loop (game, verb_index, npc + 1, killed_line, TRUE,
                          FALSE, -1, FALSE, &struck, &refused);
}

static scr_bool
lib_battle_attack_many (scr_gameref_t game, scr_bool with_object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  std::vector<scr_int> targets;
  scr_int verb_index, object;
  scr_bool struck, refused, scan;

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390
      || !battle_is_enabled (game) || !input)
    return FALSE;

  for (verb_index = 0; LIB_BATTLE_VERBS[verb_index].verb; verb_index++)
    {
      if (lib_input_contains_word (input, LIB_BATTLE_VERBS[verb_index].verb))
        break;
    }
  if (!LIB_BATTLE_VERBS[verb_index].verb)
    return FALSE;

  /*
   * Every %character% row has already declined the line, so even one
   * target is ours: `attack sentry droid` strikes the droid alone (4.0
   * reads no alias), where the catch-all would name the guard.
   */
  targets = lib_battle_named_targets (game, input, verb_index);
  if (targets.size () > 1)
    {
      /*
       * Deliberate deviation: ONE blow per line.  dobattle's target loop
       * has no exit (run400 47EB0E-47F006, run390 44CC1C), so the Runner
       * strikes every present character the line names -- `attack guard`
       * with two Guards, `attack droid and guard` -- four draws and real
       * damage each, and 4.0 then asks "Which guard." over the blows'
       * text, with no way left to take the question's answer as the one
       * target (runner_probes/battlemulti.run400.txt).  The manual (v4.0,
       * p.73) gives the command as `attack <character> [with <object>]`,
       * and the multi-foe rule (p.68) is written for characters only, who
       * "randomly pick between" their foes; the question's very existence
       * says one target was meant.  So Scarier asks FIRST and strikes
       * nobody until the answer names one (lib_battle_ask_which_target()),
       * names that cannot be told apart collapse to the first of them
       * (lib_battle_dedupe_targets()), and the 4.0 scan's question is kept
       * off the line it settles (lib_battle_400_claim_line()).  Logged in
       * test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md, "Deliberate
       * deviations".
       */
      lib_battle_prefix_contest (game, &targets, input);
      lib_battle_prefer_named_targets (game, &targets, input, verb_index);
      lib_battle_dedupe_targets (game, &targets);
      if (targets.size () > 1)
        {
          lib_battle_ask_which_target (game, input, verb_index, targets);
          return TRUE;
        }
    }
  if (targets.empty ())
    {
      /*
       * var_8A is still 0 after the loop only when no NPC anywhere is named:
       * an absent namesake sets it on its way out (47EFF4), seen or not.
       * Then 47F01E prints "Who do you want to attack?", before any weapon
       * test and whatever follows "with", and leaves 494281 alone, so the
       * line is a turn.  Measured 2026-09-13 on p4BATTLEHASH (NPCs Named
       * `Gargoyle #1`..`#3`): `attack gargoyle` answers exactly that
       * (runner_probes/battlehash.run400.txt), where Scarier fell to the
       * catch-all.
       */
      if (lib_battle_names_absent_npc (game, input))
        return FALSE;
      lib_battle_who_raise (game, input, LIB_BATTLE_VERBS[verb_index].verb);
      pf_buffer_string (filter, "Who do you want to attack?\n");
      lib_battle_400_namesake_tail (game);
      return TRUE;
    }

  /* An explicit weapon is resolved once; each target then tests it. */
  object = -1;
  scan = FALSE;
  if (with_object
      && !lib_battle_line_names_any_object (game, input))
    {
      /* The row's object is elsewhere: co() names nothing present, so
       * dobattle is silent (lib_battle_line_names_object). */
      return FALSE;
    }
  if (with_object)
    {
      object = lib_disambiguate_object (game,
                                        LIB_BATTLE_VERBS[verb_index].verb,
                                        nullptr);
      if (object == -1)
        return TRUE;
    }
  else if (lib_input_contains_word (input, "with"))
    {
      /*
       * A "with" naming no object prints nothing in dobattle, and the line
       * goes on to the character catch-all
       * (runner_probes/battlewpn.run400.txt `nonsense words`).
       */
      if (!lib_battle_line_names_any_object (game, input))
        return FALSE;
      scan = TRUE;
    }

  struck = FALSE;
  refused = FALSE;
  lib_battle_strike_loop (game, verb_index, 0, input, FALSE, with_object,
                          object, scan, &struck, &refused, &targets);
  if (struck)
    lib_battle_400_claim_line ();

  /* Only the question, and no blow: as for one target, not a turn (in 3.9
   * the Runner ticks, a deliberate deviation; see the single-target path in
   * lib_battle_attack_bare). */
  if (!struck && !refused)
    game->is_admin = TRUE;

  return TRUE;
}

scr_bool
lib_cmd_attack_npcs (scr_gameref_t game)
{
  return lib_battle_attack_many (game, FALSE);
}

scr_bool
lib_cmd_attack_npcs_with (scr_gameref_t game)
{
  return lib_battle_attack_many (game, TRUE);
}

/*
 * lib_battle_line_verb()
 * lib_battle_line_npc()
 *
 * dobattle's var_90 (run400 47E9EF-47EADB, run390 44CB5x): the first of its
 * verbs the line holds as a whole word, NULL with the Battle System off or
 * below 3.90.  And the first character the line names by dobattle's own
 * test who is in the player's room, or -1.  run_battle_line() in
 * scrunner.cpp reads both.
 */
const scr_char *
lib_battle_line_verb (scr_gameref_t game, const scr_char *input)
{
  scr_int verb_index;

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390
      || !battle_is_enabled (game) || !input)
    return nullptr;
  for (verb_index = 0; LIB_BATTLE_VERBS[verb_index].verb; verb_index++)
    {
      if (lib_input_contains_word (input, LIB_BATTLE_VERBS[verb_index].verb))
        return LIB_BATTLE_VERBS[verb_index].verb;
    }
  return nullptr;
}

scr_int
lib_battle_line_npc (scr_gameref_t game, const scr_char *input)
{
  scr_int npc;

  GS_FOR_EACH_NPC (game, npc)
    {
      if (lib_npc_named_in_line (game, npc, input)
          && npc_in_room (game, npc, gs_playerroom (game)))
        return npc;
    }
  return -1;
}


/*
 * lib_cmd_attack_npc()
 * lib_cmd_attack_npc_with()
 * lib_cmd_*_npc(), lib_cmd_*_npc_with()
 *
 * Attack an NPC, with and without weaponry.  The generic verbs (attack, fight,
 * kill, kick, slap) impose no weapon-method requirement; the remaining verbs
 * require a wielded weapon whose method matches (chop 0, cut 1, hit 2,
 * shoot 3, stab 4, throw 5) when the Battle System is enabled.
 */
/*
 * lib_attack_line_pre390()
 *
 * 3.7/3.8 with the Battle System off answer `attack dave` with
 * DontUnderstand, while `hit dave` and `kick dave` get "Dave avoids your
 * feeble attempts." (run370x runner_probes/npcamb.run370.one.rtf, run380x
 * runner_probes/npcamb.run380.one.rtf, 2026-09-19).  Read 2026-09-20: the
 * feeble line is characters()' attack arm, which the turn tail runs only
 * behind a therest message ending ", but nothing happens." -- and therest has
 * hit/kick/push/pull/press arms but no "attack" arm, so a bare `attack` line
 * reaches the tail with nothing written: DontUnderstand, no tick.  The arm's
 * c("attack") only ever fires on a line that also holds one of those verbs
 * (`push attack dave`), which is lib_hit_arm_pre390() under the
 * push/pull/press handlers.  TRUE for such a line, which the caller leaves
 * unhandled.
 */
static scr_bool
lib_attack_line_pre390 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();

  return prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390
         && !battle_is_enabled (game) && input
         && scr_strncasecmp (input, "attack", 6) == 0
         && (input[6] == ' ' || input[6] == NUL);
}

scr_bool
lib_cmd_attack_npc (scr_gameref_t game)
{
  if (lib_attack_line_pre390 (game))
    return FALSE;
  return lib_battle_attack_bare (game, "attack", -1, TRUE);
}

scr_bool
lib_cmd_attack_npc_with (scr_gameref_t game)
{
  if (lib_attack_line_pre390 (game))
    return FALSE;
  return lib_battle_attack_with (game, "attack", -1, TRUE);
}

/*
 * lib_cmd_slap_*()
 *
 * `slap` is not a verb in any Runner's grammar.  It is a pre-parse text
 * rewrite to `hit`, applied to the command line just after the game's own
 * synonyms and alongside "everything" -> "all" and "except" -> "but":
 *
 *   run370 loc_43B430, run380 loc_441C50   change("slap", "hit")
 *   run390 loc_45F246                      Replace("slap", "hit")
 *   run400 loc_48A330                      Replace(" slap ", " hit ")
 *
 * 4.0's form is space-bounded on both sides, so a command that *begins*
 * with `slap` is never rewritten.  That is why run400 answers `slap gizmo`
 * with the "I'm afraid that I wasn't anticipating that particular input."
 * a nonsense verb gets, while run390 gives it the combat dispatch (both
 * measured live 2026-08-18, on easter.taf and The Town of Azra).
 * Deliberate deviation: Scarier takes a leading `slap` at 4.0 too, so these
 * behave as `hit` -- not `attack` -- everywhere: `hit` carries battle attack
 * index 2, `attack`/`kick` carry -1.
 *
 * `smack` and `strike` are synonyms at no version: neither literal occurs
 * anywhere in run370.bas, run380.bas, run390 Form1.frm or run400.bas.
 */
scr_bool
lib_cmd_slap_npc (scr_gameref_t game)
{
  return lib_cmd_hit_npc (game);
}

scr_bool
lib_cmd_slap_npc_with (scr_gameref_t game)
{
  return lib_cmd_hit_npc_with (game);
}

scr_bool
lib_cmd_slap_object (scr_gameref_t game)
{
  return lib_cmd_hit_object (game);
}

scr_bool
lib_cmd_slap_other (scr_gameref_t game)
{
  return lib_cmd_hit_other (game);
}

scr_bool
lib_cmd_slap_what (scr_gameref_t game)
{
  return lib_cmd_hit_what (game);
}

scr_bool
lib_cmd_chop_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "chop", 0, FALSE);
}

scr_bool
lib_cmd_chop_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "chop", 0, FALSE);
}

scr_bool
lib_cmd_cut_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "cut", 1, FALSE);
}

scr_bool
lib_cmd_cut_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "cut", 1, FALSE);
}

scr_bool
lib_cmd_hit_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "hit", 2, TRUE);
}

scr_bool
lib_cmd_hit_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "hit", 2, TRUE);
}

scr_bool
lib_cmd_shoot_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "shoot", 3, TRUE);
}

scr_bool
lib_cmd_shoot_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "shoot", 3, TRUE);
}

scr_bool
lib_cmd_stab_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "stab", 4, FALSE);
}

scr_bool
lib_cmd_stab_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "stab", 4, TRUE);
}

scr_bool
lib_cmd_throw_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "throw", 5, FALSE);
}

/*
 * lib_cmd_kill_npc(), lib_cmd_kill_npc_with()
 * lib_cmd_fight_npc(), lib_cmd_fight_npc_with()
 *
 * The Adrift Runner lists "kill" and "fight" among the Battle System's generic
 * attack verbs, so with the Battle System enabled they resolve a real blow like
 * "attack".  They are not "legacy" verbs, though: with the Battle System off
 * these fall through (return FALSE) to the later "kill *"/"fight *" grammar,
 * preserving the traditional flavour responses lib_cmd_kill_other() and
 * lib_cmd_fight() provide.
 */
scr_bool
lib_cmd_kill_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "kill", -1, FALSE);
}

scr_bool
lib_cmd_kill_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "kill", -1, FALSE);
}

scr_bool
lib_cmd_fight_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "fight", -1, FALSE);
}

scr_bool
lib_cmd_fight_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "fight", -1, FALSE);
}
