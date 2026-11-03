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
 * The shared move backend, drop, give, wear, remove and inventory.
 *
 * See sclibrar.h for what the library files share and sclibrar_internal.h
 * for what the core files share.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"

static void
lib_move_to_room (scr_gameref_t game, scr_int object, scr_int /* target */)
{
  gs_object_to_room (game, object, gs_playerroom (game));
}

static void
lib_move_to_player (scr_gameref_t game, scr_int object,
                    scr_int /* target */)
{
  gs_object_player_get (game, object);
}

static void
lib_move_onto (scr_gameref_t game, scr_int object, scr_int target)
{
  gs_object_move_onto (game, object, target);
}

static const lib_move_verb_t LIB_DROP_VERB = {
  lib_move_to_room, nullptr,
  {"You drop ", "I drop ", "%player% drop "},
  {"You are not holding ", "I am not holding ", "%player% is not holding "},
  '.',
  /* run380 @438E13: `MemVar_44F108(0) & " don't have " & ...` -- the
   * third-person form really is "<name> don't have". */
  {"You don't have ", "I don't have ", "%player% don't have "}, '!', TRUE
};

static const lib_move_verb_t LIB_REMOVE_VERB = {
  lib_move_to_player, nullptr,
  {"You remove ", "I remove ", "%player% remove "},
  {"You are not wearing ", "I am not wearing ", "%player% is not wearing "},
  '!',
  {"You are not wearing ", "I am not wearing ", "%player% is not wearing "}, '!',
  FALSE
};

const lib_move_verb_t LIB_PUT_ON_VERB = {
  lib_move_onto, " onto ",
  {"You put ", "I put ", "%player% put "},
  {"You are not holding ", "I am not holding ", "%player% is not holding "},
  '.',
  {nullptr, nullptr, nullptr}, '.', FALSE
};


/*
 * lib_move_try_commands()
 *
 * Try game commands for all referenced objects.  If any succeed, remove that
 * reference from the list.  Returns TRUE if any game command printed, which
 * is what tells the caller's lists whether they have to indent past it.
 */
static scr_bool
lib_move_try_commands (scr_gameref_t game, const scr_char *command,
                       scr_bool use_definite)
{
  scr_int object_count, object;
  scr_bool has_printed;

  has_printed = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      if (use_definite
          ? lib_try_game_command_short_definite (game, command, object)
          : lib_try_game_command_short (game, command, object))
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
        }
    }

  return has_printed;
}


/*
 * lib_move_backend()
 *
 * Shared tail of the drop, remove, and put-on handlers.  Moves and lists
 * every object still flagged in object_references, then lists the ones left
 * in multiple_references as objects the player hasn't got.  The caller has
 * already offered the objects to the game's own handlers, and says with
 * has_printed whether any of those printed anything.
 *
 * Returns TRUE if the backend printed anything of its OWN -- a move clause or
 * a leftover list.  A caller that ends the line with a newline needs to know:
 * when every referenced object went to a game command there is nothing of the
 * library's on the line, and run400's drop routine leaves without adding one.
 */
scr_bool
lib_move_backend (scr_gameref_t game, const lib_move_verb_t *verb,
                  scr_int target, scr_bool has_printed)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object_count, object;
  scr_bool library_printed;
  lib_list_t list;

  object_count = gs_object_count (game);

  /* Move every object that remains referenced. */
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      list.push_back (object);
      verb->move (game, object, target);
    }

  if (!list.empty ())
    {
      lib_print_clause (game, has_printed,
                        verb->does[0],
                        verb->does[1],
                        verb->does[2]);
      /*
       * The Runner's pre-3.9 report uses the authored prefix ("You remove
       * a grubby sweatshirt."); Scarier deliberately names the object
       * definitely here in every version (deviation policy, see
       * lib_print_object_raw).
       */
      lib_print_list (game, list, lib_print_object_np, " and ");
      if (verb->onto)
        {
          pf_buffer_string (filter, verb->onto);
          lib_print_object_np (game, target);
        }
      pf_buffer_character (filter, '.');
    }
  has_printed |= !list.empty ();
  library_printed = !list.empty ();

  /* Note any remaining multiple references left out of the operation. */
  lib_drain_multiple_references (game, object_count, list);

  /*
   * Pre-3.9: first leftover only, raw prefix, and only while nothing has
   * been said (see lacks_pre_390 above).  Measured on Crime_Adventure.taf
   * (3.80) in run380, 2026-09-04: `remove shoes` with the golf shoes lying
   * unworn in the shop is "You are not wearing a pair of golf shoes!", where
   * Scarier said "the pair of golf shoes"; `drop cash` naming a penny it
   * never held is "You don't have a penny!".
   */
  if (verb->lacks_pre_390[0]
      && (prop_get_taf_version (bundle) < TAF_VERSION_390
          || (verb->lacks_single_390
              && prop_get_taf_version (bundle) < TAF_VERSION_400)))
    {
      if (!has_printed && !list.empty ())
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 verb->lacks_pre_390[0],
                                                 verb->lacks_pre_390[1],
                                                 verb->lacks_pre_390[2]));
          if (prop_get_taf_version (bundle) < TAF_VERSION_390)
            lib_print_object_raw (game, list[0]);
          else
            lib_print_object_np (game, list[0]);
          pf_buffer_character (filter, verb->lacks_end_pre_390);
          library_printed = TRUE;
        }
      return library_printed;
    }

  lib_print_object_list (game, has_printed, list, " or ", verb->lacks_end,
                         verb->lacks[0], verb->lacks[1], verb->lacks[2]);
  return library_printed || !list.empty ();
}


/*
 * lib_drop_backend()
 *
 * Common backend handler for dropping objects.  Drops all objects currently
 * referenced in the game, trying game commands first, and then moving other
 * unhandled objects to the player room floor.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
static scr_bool
lib_drop_backend (scr_gameref_t game)
{
  scr_bool has_printed;

  has_printed = lib_move_try_commands (game, "drop",
                                       lib_is_version_400 (game));

  /*
   * A named object the player is not holding still gets its line offered to
   * the game's tasks before the library refuses it.  Oh, Human (Oh_Human.taf,
   * 4.00): the electrical device sits on the floor with the light circling
   * it, and `drop device` runs task 6 "[drop/get rid of/lose/put down/set
   * down/remove/take off] {the/a} [device] {...}" -- the game's whole
   * free-the-light puzzle -- where the library alone would answer "You are
   * not holding the device." and the game would be unwinnable.  The rebuilt
   * spelling is the same definite form the held objects get, so a task whose
   * command is the bare typed line (p4REPEAT3 task 2 `drop hat`, run400
   * runner_probes/repeat3.run400.txt) still loses to the library.
   */
  if (lib_is_version_400 (game))
    {
      const scr_int object_count = gs_object_count (game);
      scr_int object;

      for (object = 0; object < object_count; object++)
        {
          if (!game->multiple_references[object])
            continue;

          if (lib_try_game_command_short_definite (game, "drop", object))
            {
              game->multiple_references[object] = FALSE;
              has_printed = TRUE;
            }
        }
    }

  return lib_move_backend (game, &LIB_DROP_VERB, -1, has_printed);
}


/*
 * lib_drop_filter()
 *
 * Helper function for deciding if an object may be dropped in this context.
 * Returns TRUE if an object may be dropped, FALSE otherwise.
 */
static scr_bool
lib_drop_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return !obj_is_static (game, object)
         && gs_object_position (game, object) == OBJ_HELD_PLAYER;
}

/*
 * lib_drop_named_filter()
 *
 * Variant of the above for explicitly named objects.  The Runner also drops
 * a *worn* object named in a drop command, implicitly removing it first --
 * "drop cloak" while wearing it answers "You drop the cloak." -- while a bare
 * "drop all" leaves worn items alone.  Both verified live against run390;
 * see test/adrift4/notes/RUNNER_TESTS_TODO.md section 2.
 *
 * It also drops an object out of a container the player is carrying, with no
 * announcement of the removal.  Both arms agree: on p39DARK under run390 the
 * coin inside the held box answers `drop coin` with "You drop the coin."
 * (runner_probes/dark.run390.putin2.txt:61), and on p4TFROM under run400 the
 * same turn is "You drop the coin."
 * (runner_probes/tfrom.run400.putin2.txt:52).  Scarier used to answer "You
 * are not holding the coin." on both.  Measured 2026-09-12.
 */
static scr_bool
lib_drop_named_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return !obj_is_static (game, object)
         && (gs_object_position (game, object) == OBJ_HELD_PLAYER
             || gs_object_position (game, object) == OBJ_WORN_PLAYER
             || obj_indirectly_held_by_player (game, object));
}


/*
 * lib_cmd_drop_all()
 *
 * Drop all objects currently held by the player.
 */
scr_bool
lib_cmd_drop_all (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_drop_filter, -1, FALSE, nullptr);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_drop_backend (game);
  else
    {
      /*
       * Not a contraction, and 4.0 reworded it.  Every Runner assembles this
       * from the pronoun array as Ary(0) & " " & Ary(4) & <literal>, so the
       * copula is spelled out: run400 name_object 46E5A0 appends
       * " carrying nothing!", run390 445867 (and its 3.7/3.8 twins)
       * " not carrying anything.".  "carrying nothing!" is absent from all
       * three pre-4.0 binaries and the contraction from all four.  Measured
       * live 2026-09-05, run400 on ptbad.taf: a second `drop all` answers
       * "You are carrying nothing!".
       */
      pf_buffer_string (filter,
                        lib_is_version_400 (game)
                        ? lib_select_response (game,
                                          "You are carrying nothing!",
                                          "I am carrying nothing!",
                                          "%player% is carrying nothing!")
                        : lib_select_response (game,
                                          "You are not carrying anything.",
                                          "I am not carrying anything.",
                                          "%player% is not carrying anything."));
    }

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_name_object_resolve_400()
 *
 * The noun resolver a plain 4.0 `drop X` or `take X` goes through:
 * put_drop_list 459DB4 hands the whole line to name_object 46E5D8, which
 * calls Proc_21_58_463640(line, 2, 0), and the take handler the same thing
 * with mode 1.  Both modes are two passes over the objects in index order:
 * pass 0 takes only one side of the room -- what the player holds or wears,
 * directly or inside or on something held, for mode 2 (Proc_21_46_44615C,
 * 46323F), and for mode 1 exactly what it will not (lib_resolve_admit_take())
 * -- and pass 1 everything present (Proc_21_44_452E9C, 463252); mode 2 has
 * no seen gate.  Each object is scored as lib_verb_object_name_score() does,
 * and
 *
 *   - the first hit of a pass (var_86 < 0 and var_A0 = 0, 46338C) becomes
 *     the result whatever its score, and pass 0 marks it;
 *   - a hit equal to the best score (var_94, 4633C3) is a tie: the result
 *     becomes -(object+2) and pass 0 marks the object -- but first
 *     Me(424), the pending object (MemVar_4941EC), is set to this object if
 *     its Short equals the Short of object Abs(var_86) (4633F0).  After the
 *     first tie var_86 is -(k+2), so the comparison is with the object TWO
 *     indexes past the last tied one, not with the tied object itself;
 *   - a higher score (463421) takes the result outright, resets the count
 *     and Me(424), and leaves every earlier mark standing.
 *
 * The best score is not reset between the passes.  A pass-0 tie (or
 * nothing) saves the count and Me(424) and runs pass 1 (46361D); pass 1
 * restores the saved Me(424) when it counted more hits than pass 0
 * (46355E).  The list the prompt shows (Me(428)) is always the pass-0
 * marks.
 *
 * name_object then says "It is not clear which <term> you are referring
 * to." for a tie with no pending object (46E192, term 446C74 of the last
 * tied object: its Short if the line has it, else its first alias the line
 * has), and generaltasks prompts "Which <term>.  <list>?" for one with a
 * pending object (48B6B1-48B80C, term its Short replaced by the last of its
 * aliases the line has) -- administrative, as every such prompt is.
 * Measured on wilkins T110-T117 (runner_transcripts/wilkins.txt): with the
 * base tincture and the tinctures of alice, gertrude, irene, leanor, marie,
 * mary and rose held, `drop tincture of alice`, `... gertrude` and
 * `... marie` are not clear, `... irene` and `... mary` ask "Which
 * tincture.", `... leanor` and `... rose` drop, and `drop tincture of wai
 * lin` (on the floor) drops the base tincture, the one held object the line
 * names.
 *
 * IS_TAKE swaps pass 0 for the take side, and p4TAKER measured that this one
 * model decides which of the two answers a crowded take reaches as well
 * (run400 runner_probes/taker.run400.txt, 2026-09-20).  Its world is five
 * pairs of namesakes, every Prefix "a" so no Prefix word narrows anything,
 * and the ten lines fall out cell for cell:
 *
 *   take gem   both loose, one Short    Which gem.  The gem or the gem?
 *   take cog   one held, one loose      You take the cog.
 *   take pad   both HELD, one Short     It is not clear which pad ...
 *   take orb   both loose, one by ALIAS It is not clear which orb ...
 *   take tin   held Short, loose alias  You take the pin.
 *   drop gem   both loose               It is not clear which gem ...
 *   drop cog   both held, one Short     Which cog.  The cog or the cog?
 *   drop pad   both held, one Short     Which pad.  The pad or the pad?
 *   drop orb   both loose, one by ALIAS It is not clear which orb ...
 *   drop tin   both held, one by ALIAS  It is not clear which tin ...
 *
 * So the question needs BOTH a tie the verb's own pass makes -- pass 1
 * restoring the saved Me(424) is what flattens `take pad` and `drop gem`,
 * where the verb's side of the room held nothing and pass 1 counted more --
 * and two tied objects with the same Short, which is what flattens every
 * pair joined by an alias.  It also retro-explains p4OPENA's `take gem`
 * (runner_probes/opena.run400.txt): the gem and the rock aliased "gem" were
 * both loose by then, so it is the `take orb` cell.
 *
 * MODE 4 is mode 1 with a container named (get_piece's var_92 <> &HFF): one
 * pass over what is visibly in or on disambig.resolve_parent_400; see
 * lib_take_from_piece_400().
 *
 * MODE 0 is the plain one-pass scan most of the Runner's callers use, the
 * co(i, 0) gate and no second chance (lib_resolve_admit_mode0()); openclose
 * is one of them, and the pending object is what decides whether a crowded
 * `open X Y Z` asks or is flat.  See lib_open_close_tie_400().
 *
 * Returns the resolved object, -1 for a tie (with *PENDING, *LAST_TIED and
 * MARKED filled), or -2 when nothing scored in the last pass either, the
 * fallback the existing path covers.
 */

scr_int
lib_name_object_resolve_400 (scr_gameref_t game, const scr_char *input,
                             scr_int mode, scr_int *pending,
                             scr_int *last_tied,
                             std::vector<scr_int> *marked, scr_int *mark_count)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int object_count = gs_object_count (game);
  std::vector<scr_bool> marks (object_count, FALSE);
  scr_int pass, result, best, count, me, saved_count, saved_me, object;

  best = 0;
  saved_count = 0;
  saved_me = -1;
  result = -1;
  count = 0;
  me = -1;
  for (pass = 0; pass < 2; pass++)
    {
      result = -1;
      me = -1;
      for (object = 0; object < object_count; object++)
        {
          scr_int score;

          if (pass == 0)
            {
              marks[object] = FALSE;
              if (mode == 0
                  ? !lib_resolve_admit_mode0 (game, object, 0)
                  : mode == 4
                  ? !lib_resolve_admit_parent (game, object, 0)
                  : mode == 1
                    ? !lib_resolve_admit_take (game, object, 0)
                    : !(gs_object_position (game, object) == OBJ_HELD_PLAYER
                        || gs_object_position (game, object)
                           == OBJ_WORN_PLAYER
                        || obj_indirectly_held_by_player (game, object)))
                continue;
            }
          else if (mode == 1
                   ? !lib_resolve_admit_take (game, object, 1)
                   : !obj_indirectly_in_room (game, object,
                                              gs_playerroom (game)))
            continue;

          score = lib_verb_object_name_score (game, object, input);
          if (score == 0)
            continue;

          if (result < 0 && count == 0)
            {
              result = object;
              count = 1;
              if (pass == 0)
                marks[object] = TRUE;
            }
          else if (score == best)
            {
              const scr_int other = result < 0 ? -result : result;

              if (other < object_count)
                {
                  const scr_char *name, *other_name;

                  name = prop_get_indexed_string (bundle, "Objects", object,
                                                  "Short");
                  other_name = prop_get_indexed_string (bundle, "Objects",
                                                        other, "Short");
                  if (strcmp (name, other_name) == 0)
                    me = object;
                }
              result = -object - 2;
              count++;
              if (pass == 0)
                marks[object] = TRUE;
            }

          if (score > best)
            {
              result = object;
              best = score;
              me = -1;
              count = 1;
            }
        }

      if (pass == 0)
        {
          saved_count = count;
          saved_me = me;
          if (result >= 0 || mode == 0 || mode == 4)
            break;
          count = 0;
        }
      else if (count > 1 && count > saved_count)
        me = saved_me;
    }

  if (result >= 0)
    return result;
  if (result == -1)
    return -2;

  marked->clear ();
  for (object = 0; object < object_count; object++)
    {
      if (marks[object])
        marked->push_back (object);
    }
  *mark_count = saved_count;
  *pending = me;
  *last_tied = -result - 2;
  return -1;
}

/*
 * The name 446C74 gives an object in the line: its Short when that is a
 * whole word of it, else the first alias that is.
 */
const scr_char *
lib_drop_named_term_400 (scr_gameref_t game, scr_int object,
                         const scr_char *input, scr_bool last_alias)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *term;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias;

  term = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!last_alias && lib_input_contains_word_400 (input, term))
    return term;

  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count; alias++)
    {
      const scr_char *name;

      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (scr_strempty (name) || !lib_input_contains_word_400 (input, name))
        continue;
      term = name;
      if (!last_alias)
        break;
    }
  return term;
}

/*
 * The flat reply to a tie nothing breaks, "It is not clear which <term> you
 * are referring to.", left without its line end for the caller to close.
 */
void
lib_print_not_clear_which_400 (scr_gameref_t game, const scr_char *term)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, "It is not clear which ");
  pf_buffer_string (filter, term);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         " you are referring to.",
                                         " I am referring to.",
                                         " %player% is referring to."));
}

/*
 * lib_co_400_raise_for_pending_tie()
 *
 * The unhandled-verb line's question, read off the pending object the way
 * generaltasks reads it.  The 463640 walk at the top of the line (48A3F5,
 * mode 0, and therest's own at 48862B) leaves Me(424) = MemVar_4941EC
 * behind, and 48B6B1 asks only when that is an object: the term is its
 * Short, replaced by the last of ITS aliases that is a whole word of the
 * line (48B73C-48B78C), and the list is the walk's marks.  So which object
 * the question is asked about is the tie arm's index+2 quirk (4633F0; see
 * lib_name_object_resolve_400()), and p4CO's six cells fall out of it (run400
 * runner_probes/co.run400.co12.txt and runner_probes/co.run400.co13.txt,
 * 2026-09-20) -- red tree 0, blue tree 1, rock 2, mustang key 3 and truck key
 * 4 (both aliased "keys"), hut 5 (aliased "shed"), shed 6, and a tree 7 in
 * the other room:
 *
 *     chop tree hut         tie at 1 matches Short(0): pending 1; the hut
 *                           is tested against Short(3)       Which tree.
 *     chop shed tree        the same, the shed against Short(7)
 *                                                            Which tree.
 *     chop keys tree        after the tie at 1 the mustang key is tested
 *                           against Short(3) -- itself: pending 3, whose
 *                           alias the line holds             Which keys.
 *     chop shed keys tree   pending 3 again; the hut and the shed are
 *                           tested against Short(6), Short(7)  Which keys.
 *     chop keys             the truck key against Short(3)   NO IDEA.
 *     chop shed             the shed against Short(5)        NO IDEA.
 *
 * This replaces the model that read the term off the crowd's own names
 * (lib_co_400_raise_for_short_tie(), still used for a " with " half),
 * which could not say why the hut and the shed never park an object.
 */
scr_bool
lib_co_400_raise_for_pending_tie (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  std::vector<scr_int> marked;
  scr_int object, pending, last_tied, mark_count;

  if (!input)
    return FALSE;

  /* An open question takes the element first; see
     lib_co_400_raise_for_short_tie(). */
  if (lib_co_400_question_pending ())
    {
      lib_co_400_note_refusal ();
      return FALSE;
    }

  object = lib_name_object_resolve_400 (game, input, 0, &pending, &last_tied,
                                        &marked, &mark_count);
  if (object != -1 || pending < 0
      || (scr_int) marked.size () != mark_count)
    return FALSE;

  lib_co_400_raise (game, lib_drop_named_term_400 (game, pending, input, TRUE),
                    marked);
  return TRUE;
}

/*
 * The 4.0 plain `drop X`: see lib_name_object_resolve_400().  Returns TRUE when the
 * line is answered here -- a prompt or the not-clear refusal -- and
 * otherwise, for a unique held winner, leaves just that object in the
 * multiple references with *REFERENCES 1, or *REFERENCES -1 to leave the
 * line to the ordinary parse.
 */

static scr_bool
lib_drop_named_400 (scr_gameref_t game, scr_int *references)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  std::vector<scr_int> marked;
  std::string line;
  scr_int object, pending, last_tied, mark_count;

  /*
   * A comma is no bar: the splitter keeps `drop coin, hat` whole when "hat"
   * names an object (p4AND, run400 runner_probes/and.run400.txt), and
   * name_object then scores the whole fragment, "coin," counting because c()
   * ends a word at a comma -- so the coin and the hat tie and the answer is
   * "It is not clear which hat you are referring to.".
   */
  *references = -1;
  if (!lib_is_version_400 (game) || !input
      || !lib_input_contains_word_400 (input, "drop")
      || lib_input_contains_word_400 (input, "all")
      || lib_input_contains_word_400 (input, "everything")
      || lib_input_contains_word_400 (input, "and"))
    return FALSE;

  line = run_normalise_put_line (input);
  if (lib_put_split_400 (game, line, FALSE, TRUE, nullptr) != std::string::npos)
    return FALSE;

  object = lib_name_object_resolve_400 (game, input, 2, &pending, &last_tied,
                                        &marked, &mark_count);
  if (object == -2)
    {
      /*
       * Nothing present answers, so 463640 falls back to the seen objects,
       * and insides says "not holding" (465F1D) inside put_drop_list, above
       * the task dispatcher.  ghoster T17 `drop body`, the body left in
       * another room: "You are not holding your lifeless body.", where the
       * task pass let task 8's restriction refusal speak first
       * (runner_transcripts/ghoster.txt).  lib_cmd_drop_what() is the same
       * answer for lines no task matches.
       */
      scr_int kind;

      object = lib_seen_named_object_400 (game, input);
      /*
       * Named nothing at all: put_drop_list's name_object (46E5D8) takes
       * the same 46E142 exit as a put-in's unnamed direct object, with the
       * whole line as the fragment (no preposition, split = Len(line) at
       * 459C55), so the line it leaves installed is the "put X" rewrite.
       * With a put/drop-class task pre-matching the typed line the Runner
       * stays silent and the tasks never see "drop": advent350b's `drop bear`
       * at the troll is "I don't understand what you want to do with large
       * cave bear.", where task 112 "[drop/free] bear" would have run
       * (run400x runner_probes/advent350b.run400.dropbear.txt T275, and the
       * same answer with task 112's restriction failing; `free bear` runs
       * it).  With none, the line goes on to the library's "Drop what?" as
       * before.  See run_priority_unnamed_put_object().
       */
      if (object == -1 && run_in_priority_pass ()
          && lib_task_prematches_input (game, 2))
        run_priority_unnamed_put_object ();
      if (object == -1 || obj_indirectly_in_room (game, object,
                                                  gs_playerroom (game)))
        return FALSE;
      /* A task that matches with its restrictions passing still runs:
         freedom (escape.taf) `drop jeep` while driving it is task 43
         "drop *jeep*", "Done." (runner_transcripts/freedom.txt).  ghoster's
         task 8 only matches through its failing restriction's message
         (kind 3), and the Runner says "not holding".  Fitted to these two
         rows; where 44CCE0 makes the call is not traced. */
      if (lib_task_prematches_line (game, input, 0, &kind) && kind != 3)
        return FALSE;
      lib_print_response_object (game,
                                 "You are not holding ",
                                 "I am not holding ",
                                 "%player% is not holding ",
                                 object, ".\n");
      return TRUE;
    }

  if (object == -1)
    {
      if (pending < 0)
        {
          lib_print_not_clear_which_400
            (game, lib_drop_named_term_400 (game, last_tied, input, FALSE));
          pf_buffer_character (filter, '\n');
          return TRUE;
        }

      /* Me(428) is joined by a countdown from the pass-0 count; with more
       * marks than that the Runner runs names together, unmeasured. */
      if ((scr_int) marked.size () == mark_count)
        {
          lib_co_400_raise_named (game,
                                  lib_drop_named_term_400 (game, pending,
                                                           input, TRUE),
                                  marked);
          return TRUE;
        }
      return FALSE;
    }

  if (!(gs_object_position (game, object) == OBJ_HELD_PLAYER
        || gs_object_position (game, object) == OBJ_WORN_PLAYER
        || obj_indirectly_held_by_player (game, object)))
    return FALSE;

  /*
   * A held object is insides' (46639C), and put_drop_list has already
   * rewritten the global line "drop " -> "put " (459B3D).  insides pre-matches
   * that line with no class filter (465DC8); a hit hands the definite form
   * to put-family tasks only and claims the line whatever they do, so with
   * none running generaltasks' tail answers DontUnderstand, not a turn.
   * mysterymanor T3 `drop cell phone`: task 3's `* cell phone` matches "put
   * cell phone" and the Runner prints "What's that? A spook got your
   * tongue?" (runner_transcripts/mysterymanor.txt).  p4REPEAT3's literal
   * `drop hat` task does not match "put hat", so its drop stays the library's
   * (runner_probes/repeat3.run400.txt).  lib_put_held_unsplit_400() is the
   * put twin.
   *
   * A put-family task on the rewritten line does NOT claim it: ADRIFTMAS
   * T59 `drop suitcase` hits task 30 "[wear/put] {on} {the} [%object%]" and
   * advent350b T270 `drop lamp` hits task 77 "[drop/put] [lantern/light]",
   * and the Runner answers both from the library ("You drop the suitcase.",
   * "You drop the brass lantern.").  So only a line that no class-2 task
   * pre-matches is claimed -- a rule fitted to these four rows, not read
   * out of 44CCE0.
   */
  if (gs_object_position (game, object) == OBJ_HELD_PLAYER
      && lib_task_prematches_line (game, line.c_str (), 0)
      && !lib_task_prematches_line (game, line.c_str (), 2))
    {
      if (!lib_try_game_command_short_definite (game, "drop", object))
        {
          pf_buffer_string (filter,
                            prop_get_global_string (gs_get_bundle (game),
                                                    "DontUnderstand"));
          pf_buffer_character (filter, '\n');
          game->is_admin = TRUE;
        }
      return TRUE;
    }

  gs_clear_multiple_references (game);
  game->multiple_references[object] = TRUE;
  *references = 1;
  return FALSE;
}


/*
 * lib_drop_and_arm_pre400()
 *
 * Before 4.0 drops() picks its arm by c() on the whole line: "all" first,
 * then "and" (run390 4455E5/445607, run380 43869F/4386CD, run370 the same
 * code).  The "and" arm walks every object, counts the ones held or worn
 * whose name co() finds and that no "drop <Short>" task claims, and drops
 * those.  When it counted none it sets " not carrying anything." (run390
 * 445841-44586B, run380 4388E6) and does not name the objects, so the
 * one-object "don't have <X>!" refusal never speaks on this arm.  pPUTFULL39
 * `drop coin and stone on junk`, both on the floor, is "You are not carrying
 * anything." (run390x runner_probes/putfull.run390.txt, 2026-09-19).
 */
static scr_bool
lib_drop_and_arm_pre400 (scr_gameref_t game)
{
  const scr_char *const line = run_get_dispatch_input ();

  return !lib_is_version_400 (game) && line
         && !lib_co_contains (line, "all") && lib_co_contains (line, "and");
}


/*
 * The pre-3.90 drop gate -- not ported.
 *
 * Below 3.90 a drop line gets exactly ONE look at the task matcher, and
 * that look is inside drops().  run380 438659 (run370 430475 is the same
 * code) takes every line saying drop / put down / leave / the game's own
 * verb 12, and generaltasks GoTo's past the general tasks(0) pass whenever
 * drops() CLAIMS -- which is not the same as writing a message, because the
 * message buffer is global and survives a handler that hands the line on.
 * drops() claims by setting its handled flag var_94, and the only places
 * that happens on the one-object arm are 438E94 (run370 430C55), reached
 * for any object held or worn at all, and 4389xx (430A1D), the print loop's
 * own act-on-an-object arm.  So:
 *
 *   - Carrying and wearing NOTHING, drops() writes "Drop what?" into the
 *     buffer at 438FE6 (430DCF) and hands the line on anyway.  A task then
 *     runs from tasks(0) and overwrites it; nothing matching leaves the
 *     "Drop what?" standing.
 *   - Carrying or wearing ANYTHING, drops() claims, and the line's one
 *     chance at a task is 438F4F (430D38), at the bottom of the object walk
 *     438E68 (430C29) behind 438E8E (430C4F) "field 22 is 0 or &H9C" and
 *     438EF4 (430CDD) "co() finds this object's name in the line".  Name
 *     nothing held or worn and the matched task never runs, while 4386F5
 *     (430511) has already set var_A6 = 0 and 438AD5 (4308B0) has shut the
 *     whole library print loop -- so the buffer stays empty and 438FE6
 *     writes "Drop what?".
 *
 * Measured 2026-09-20 on run370 and run380, which answer identically.
 * p3xDROPGATE (make_3738_dropgateprobe.py: loose coin and pebble, held bean
 * and cloak, tasks `drop a coin` `drop coin bean` `drop zzz` `drop a cloak`),
 * runner_probes/dropgate.run370.rtf / runner_probes/dropgate.run380.rtf:
 *
 *   drop a coin     "Drop what?"  -- the task matched and did not run
 *   drop coin bean  "DROPCB."     -- the held bean is named, the coin is not
 *   drop zzz        "Drop what?"  -- the line names nothing that exists
 *   drop a cloak    "DROPCLOAK."  -- after `wear cloak`, the &H9C arm
 *   drop a coin     "DROPCOIN."   -- once the coin is in hand
 *   leave zzz       "Drop what?"  -- `leave` claims too, and `put down`
 *   leave bean      "You drop the bean."
 *   leave a coin    "You don't have a coin!"  -- no task matched, so the
 *                   print loop spoke and the claim only hid the task arm
 *
 * p3xEMPTYHAND (loose coin and pebble, nothing held, tasks `drop a coin`
 * and `drop zzz`), transcripts eh37 / eh38 / eh237 / eh238:
 *
 *   drop a coin     "DROPCOIN."   -- empty-handed, drops() did not claim
 *   drop a pebble   "You don't have a pebble!"
 *   drop zzz        "DROPZZZ."
 *   drop qqq        "Drop what?"  -- the buffer nothing overwrote
 *   leave qqq       "Drop what?"
 *   put down qqq    "Drop what?"
 *   take a coin / drop a coin     "DROPCOIN."  -- now named and held
 *
 * Alice's Restaurant (arlo.taf, 3.70) is the corpus case: `leave station` at
 * the police station, carrying nothing, runs its task (run370;
 * runner_transcripts/alices_restaurant.rtf).  With the empty-handed arm
 * missing, Scarier answered "Drop what?" and lost the ending.
 *
 * 3.90 keeps the same walk (445D79-445DAF) and its own "Drop what?" at 445F0B
 * stays live -- p39DROPGATE `drop qqq` answers it -- but 44562A runs a
 * matching task at the checktask gate itself, before the walk, so the walk's
 * gate never bites: p39DROPGATE answers DROPCOIN and DROPZZZ with nothing in
 * hand (runner_probes/dropgate.run390.txt,
 * runner_probes/dropgate.run390.b.txt).  Hence the 3.90 cut.  Only the
 * one-object arm is measured; the "all" and "and" arms rewrite the line per
 * object and are left alone.
  *
 * Deliberate deviation (2026-09-27): not ported.  It only ever takes an
 * author's task away -- cave's `leave pool` and timmy's `leave the police
 * station` answer "Drop what?" while the player holds anything at all -- so
 * a drop line reaches the tasks in the ordinary passes whatever is in hand.
 */


/*
 * lib_move_what_after_silent_task_pre400()
 *
 * A pre-4.0 take or drop line that a task matched, where the task ran and
 * printed nothing.  drops() dispatches the task from inside its held-or-worn
 * walk (run380 438F4F, run370 430D38; run390 runs it at the checktask gate
 * 44562A) and, the task being silent, writes "Drop what?" into the still
 * empty buffer (438FE6 / 430DCF / 445F0B), the walk's handled flag claiming
 * the line.  The library print loop is gated shut by the match (4386F5), so
 * nothing is dropped.  run390's takes() answers the same way, "Take what?"
 * with nothing taken.  Measured 2026-09-26 on p3xBEYOND (make_beyondprobe.py;
 * runner_probes/beyond.run370.b.rtf, runner_probes/beyond.run380.b.rtf,
 * runner_probes/beyond.run390.b.txt): silent literal tasks "drop cape to the
 * floor" (+250) and "drop cape gently" (+100) with the cape held score and
 * answer "Drop what?" at 3.7, 3.8 and 3.9, the cape still in hand; "take orb"
 * (+5) with the orb on the floor is "Take what?" at 3.9, the orb left there.
 * 3.8's take row outranks the task (the orb is taken, no score), and 3.7's is
 * "I don't understand." after two runs of the task -- neither is handled
 * here; 3.7's is the silent-task claim's in run_all_commands().
 */
scr_bool
lib_move_what_after_silent_task_pre400 (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *const line = run_get_dispatch_input ();
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  scr_int object;

  if (!line || version >= TAF_VERSION_400)
    return FALSE;

  /* Deliberate deviation (2026-09-30): verb words inside a longer object
     or character name are no verbs; see lib_mask_long_names(). */
  const std::string masked = lib_mask_long_names (game, line);
  const scr_char *const words = masked.c_str ();
  auto take_what = [version] (const scr_char *text) -> scr_bool
    {
      return version == TAF_VERSION_390
             && (lib_co_contains (text, "get") || lib_co_contains (text, "take")
                 || lib_co_contains (text, "pick"))
             && !lib_co_contains (text, "from")
             && !lib_co_contains (text, "get on")
             && !lib_co_contains (text, "get down");
    };
  auto drop_word = [] (const scr_char *text) -> scr_bool
    {
      return lib_co_contains (text, "drop")
             || lib_co_contains (text, "put down")
             || lib_co_contains (text, "leave")
             || (lib_co_contains (text, "put")
                 && lib_co_contains (text, "down"));
    };

  if (SCR_TRACING_DEVIATIONS ()
      && (take_what (line) != take_what (words)
          || drop_word (line) != drop_word (words)))
    SCR_DEVIATION ("mask_what_prompt", "masked=\"%s\"", words);

  if (take_what (words))
    {
      pf_buffer_string (filter, "Take what?");
      pf_buffer_answer_break (filter);
      return TRUE;
    }

  if (!drop_word (words))
    return FALSE;
  if (lib_co_contains (words, "all") || lib_co_contains (words, "and"))
    return FALSE;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if ((gs_object_position (game, object) == OBJ_HELD_PLAYER
           || gs_object_position (game, object) == OBJ_WORN_PLAYER)
          && lib_co_pre400 (game, line, object, 0))
        {
          pf_buffer_string (filter, "Drop what?");
          pf_buffer_answer_break (filter);
          return TRUE;
        }
    }
  return FALSE;
}

/*
 * lib_drop_and_arm_collect_pre400()
 *
 * The "and" arm's own walk (run390 4457A0-445813): it never parses the list,
 * so a word that names nothing is simply not found.  Every object held or
 * worn directly (o(22) 0 or &H9C, not one inside a held container) whose
 * name co(obj, 0) finds is marked in multiple_references; returns the count.
 * p37PUT/p38PUT/p39PUT `drop foo and bar` is "You are not carrying anything."
 * and `drop coin and foo` is "You drop the coin." (run370x
 * runner_probes/put.run370.drop.rtf, run380x
 * runner_probes/put.run380.drop.rtf, run390x
 * runner_probes/put.run390.drop.txt, 2026-09-19), where Scarier said "Drop
 * what?" and "I only understood you as far as wanting to drop the coin.".
 */
static scr_int
lib_drop_and_arm_collect_pre400 (scr_gameref_t game)
{
  const scr_char *const line = run_get_dispatch_input ();
  scr_int object, count;

  gs_clear_multiple_references (game);
  count = 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if ((gs_object_position (game, object) == OBJ_HELD_PLAYER
           || gs_object_position (game, object) == OBJ_WORN_PLAYER)
          && lib_co_pre400 (game, line, object, 0))
        {
          game->multiple_references[object] = TRUE;
          count++;
        }
    }
  return count;
}


/*
 * lib_drop_multiple_common()
 *
 * Drop the objects held by the player and listed in %text%, or -- for
 * is_except -- every one of them but those listed.
 */
static scr_bool
lib_drop_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_bool (*resolver) (scr_gameref_t, scr_int, scr_int);
  scr_int objects, references;
  scr_bool library_printed;

  references = -1;
  if (!is_except && lib_drop_named_400 (game, &references))
    return TRUE;

  /*
   * Named objects may also be dropped from worn; the "all" universe that
   * "drop all except ..." works over is held objects only.
   */
  resolver = is_except ? lib_drop_filter : lib_drop_named_filter;

  /* Parse the multiple objects list to find the target objects. */
  if (references == 1)
    ;
  else if (!is_except && lib_drop_and_arm_pre400 (game))
    references = lib_drop_and_arm_collect_pre400 (game);
  else if (!lib_parse_multiple_objects (game, is_except ? "retain" : "drop",
                                        resolver, -1,
                                        &references))
    {
      /* Pre-4.0: drops() names its object over the WHOLE line, co(obj, 2);
         see lib_move_named_whole_line_pre400(). */
      if (is_except
          || !lib_move_named_whole_line_pre400 (game, resolver, 2, &references))
        return FALSE;
    }
  else if (references == 0)
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              resolver, -1, is_except,
                              &references);
  if (objects == 0 && !is_except && lib_drop_and_arm_pre400 (game))
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not carrying anything.",
                                             "I am not carrying anything.",
                                             "%player% is not carrying anything."));
      library_printed = TRUE;
    }
  else if (objects > 0 || references > 0)
    library_printed = lib_drop_backend (game);
  else
    {
      lib_print_nothing_held (game, FALSE, is_except && objects == 0, ".");
      library_printed = TRUE;
    }

  /*
   * 4.0 runs this handler above the task dispatcher, so a drop every one of
   * whose objects went to a game command leaves the line entirely to that
   * task -- and run400's drop routine returns from its per-object arm without
   * closing a line it never opened (@46F358).  Adding one here put a blank
   * line after Glum Fiddle's `drop tray` and JGrim's `drop mud`.
   */
  if (library_printed || !lib_is_version_400 (game))
    pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_cmd_drop_except_multiple()
 * lib_cmd_drop_multiple()
 *
 * Facets of lib_drop_multiple_common().
 */
/*
 * Before 4.0 there is no except list either.  drops()' c("all") arm drops
 * everything held or worn and skips only an object whose name falls after
 * " but " (run380 438793-4387BD, run390 4456AB; run380 changes "except" to
 * "but" first, 441C72), and run370 has no "but" at all, so its `drop all
 * except X` is `drop all`.  Nothing left to drop is the all arm's " not
 * carrying anything." (run380 4388E6), never a complaint about the exception:
 * p37PUT / p38PUT `drop all except coin` with nothing held (run370x
 * runner_probes/put.run370.feed2.rtf / run380x
 * runner_probes/put.run380.feed2.rtf, 2026-09-19).  Deliberate deviation: 3.7
 * keeps the exception too, as 3.8 does.
 */
scr_bool
lib_cmd_drop_except_multiple (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (lib_is_version_400 (game))
    return lib_drop_multiple_common (game, TRUE);

  gs_set_multiple_references (game);
  if (lib_apply_filter (game, lib_drop_filter, -1, FALSE, nullptr) == 0)
    {
      gs_clear_multiple_references (game);
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not carrying anything.",
                                             "I am not carrying anything.",
                                             "%player% is not carrying anything."));
      pf_buffer_answer_break (filter);
      return TRUE;
    }
  gs_clear_multiple_references (game);
  return lib_drop_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_drop_multiple (scr_gameref_t game)
{
  return lib_drop_multiple_common (game, FALSE);
}


/*
 * lib_cmd_leave_all_pre400()
 * lib_cmd_leave_except_multiple_pre400()
 * lib_cmd_leave_multiple_pre400()
 * lib_cmd_leave_absent_pre400()
 * lib_cmd_leave_what_pre400()
 *
 * Below 4.0 `leave` is a third spelling of `drop`, and not a verb of its
 * own: every pre-4.0 drops() opens on `c("drop") Or c("put down") Or
 * c("leave") Or (c("put") And c("down") And <flag> = 0)` -- run370 430475,
 * run380 438659, run390 44554E-4455B3 -- so a `leave` line enters the drop
 * handler and takes whichever of its arms the rest of the line picks.  4.0's
 * drops() still tests the word (46F15E-46F197), but nothing routes a `leave`
 * line to it: run400 answers every one of them from the catch-all, which is
 * what Scarier already does, so these rows decline there.
 *
 * Measured 2026-09-20 on p37/p38/p39/p4DROPGATE (loose coin and pebble, held
 * bean and cloak; make_3738_dropgateprobe.py, which now builds the 4.0 world
 * too), transcripts runner_probes/dropgate.run*.leavegate.* and
 * runner_probes/dropgate.run370.leaveall.rtf:
 *
 *   leave bean     "You drop the bean."       -- named and held
 *   leave coin     "You don't have a coin!"   -- named, loose in the room
 *                  ("You don't have the coin!" at 3.9)
 *   leave qqq      "Drop what?"               -- names nothing
 *   leave          "Drop what?"
 *   leave north    "Drop what?", and the player does not move: generaltasks
 *                  calls drops() well above moves() (run370 43B958 vs
 *                  43BAEB) and a claimed line jumps to the turn tail
 *   leave all      "You drop the bean and the cloak."
 *   leave bean and cloak / leave all except bean -- `drop`'s own answers
 *   leave everything, hands empty   "You are not carrying anything."
 *
 * 3.70, 3.80 and 3.90 answer that feed identically bar the 3.9 article, and
 * Scarier already matched every cell of it with `drop` typed in place of
 * `leave`.  run400 answers the same feed "I don't understand." and "I don't
 * understand what you want me to do with the bean." throughout
 * (runner_probes/dropgate.run400.leavegate.txt).
 *
 * Only the drop arms take the new spelling; the put rows keep `drop` and `put
 * down` alone.  `drop X in Y` is not a put before 4.0 either -- run390
 * answers `drop lamp in box` with "You drop the lamp." (p39SURF,
 * runner_probes/surf.run390.leaveput.txt) where Scarier puts the lamp inside
 * the box -- but that is the put rows' own deviation, and it is written up in
 * WINE-TRANSCRIPTS-TODO rather than changed here.
 */
scr_bool
lib_cmd_leave_all_pre400 (scr_gameref_t game)
{
  return !lib_is_version_400 (game) && lib_cmd_drop_all (game);
}

scr_bool
lib_cmd_leave_except_multiple_pre400 (scr_gameref_t game)
{
  return !lib_is_version_400 (game) && lib_cmd_drop_except_multiple (game);
}

scr_bool
lib_cmd_leave_multiple_pre400 (scr_gameref_t game)
{
  return !lib_is_version_400 (game) && lib_cmd_drop_multiple (game);
}


/*
 * 4.0's give NPC loop (488AE8-488B5C) walks every character for a present,
 * seen one the line refers to anywhere (45E99C mode 0), so a continued
 * `give to Nobody coin dave` still finds Dave where no give pattern parses
 * the line.  Measured runner_probes/withq.run400.txt: `give`, `coin`, `dave`.
 */
static scr_int
lib_give_present_npc_400 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int index_;

  if (!lib_is_version_400 (game) || !input)
    return -1;
  GS_FOR_EACH_NPC (game, index_)
    {
      if (gs_npc_seen (game, index_)
          && npc_in_room (game, index_, gs_playerroom (game))
          && lib_npc_referenced (game, index_, input))
        return index_;
    }
  return -1;
}

/* The 4.0 refusal: not a turn (runner_probes/withq.run400.2.txt,
   ghosttown). */
static scr_bool
lib_give_not_interested_400 (scr_gameref_t game, scr_int npc, scr_int object)
{
  game->is_admin = TRUE;
  pf_new_sentence (gs_get_filter (game));
  lib_print_npc_np (game, npc);
  lib_print_wrapped_object (game, " doesn't seem interested in ",
                            object, ".\n");
  return TRUE;
}

/*
 * lib_give_not_held_pre390()
 *
 * Every Runner's character-handler give refuses a thing the player is not
 * holding with a FULL STOP, not the bang the take and drop refusals use:
 * run370 4390DB-4390E2, run380 440FEE-440FF5, run390 45A1D3-45A1F3, run400
 * 480338-480375 all build `<player> & " don't have " & Prefix & " " & Short`
 * and append ".".  From 3.90 a SECOND give answers first and hides it --
 * run390's therest give (45D696) and run400's generaltasks_verbs give
 * (488A96), both with the bang -- while the character handler's own give
 * only fills an empty buffer.  Below 3.90 there is no second give, so the
 * full stop is what the player sees: `give coin to bob`, `give nut bob` and a
 * completed `give hat` are all "You don't have the coin." at 3.7 and 3.8
 * (cells 4, 16 and 20 of runner_probes/ord.run370.give.rtf and
 * runner_probes/ord.run380.give.rtf, against
 * runner_probes/ord.run390.give.txt, 2026-09-21).
 */
static void
lib_give_not_held_pre390 (scr_gameref_t game, scr_int object)
{
  const scr_bool stop
      = prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390;

  lib_print_response_object (game,
                             "You don't have ",
                             "I don't have ",
                             "%player% don't have ",
                             object, stop ? ".\n" : "!\n");
}

/*
 * lib_cmd_give_object_npc()
 * lib_cmd_give_object()
 *
 * Attempt to give an object to an NPC.
 */
scr_bool
lib_cmd_give_object_npc (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, npc;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "give", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  if (lib_is_version_400 (game))
    {
      /*
       * 4.0's therest() checks holding before it ever resolves the NPC:
       * run400 mdlSpreadTheLoad 488A7C "Give what?", 488AA2 " don't have
       * ", then the NPC test that ends " to who?" (488B1E-488B45).
       */
      if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
        {
          lib_print_response_object (game,
                                     "You don't have ",
                                     "I don't have ",
                                     "%player% don't have ",
                                     object, "!\n");
          return TRUE;
        }

      /*
       * A named-but-absent NPC gets the plain give's "to who?" answer, not
       * lib_disambiguate_npc()'s "Please be more clear" -- measured
       * escape_to_new_york turn 193 `give key to purser` (Ticket run400
       * xoshiro trace 2026-09-12), the key held and the purser elsewhere.
       * A genuinely ambiguous NPC reference (count > 1) is unaffected;
       * is_ambiguous mutes only the "none at all" branch.
       */
      npc = lib_disambiguate_npc (game, "give to", &is_ambiguous);
      if (npc == -1)
        {
          if (is_ambiguous)
            return TRUE;
          npc = lib_give_present_npc_400 (game);
          if (npc >= 0)
            return lib_give_not_interested_400 (game, npc, object);
          lib_question_prefix_from_line (game);
          lib_print_wrapped_object (game, "Give ", object, " to who?\n");
          return TRUE;
        }

      return lib_give_not_interested_400 (game, npc, object);
    }

  /*
   * Get the referenced npc.  None present is not this handler's to answer:
   * the Runners' character give wants a PRESENT NPC (run380 440E8C), and
   * without one the line falls through past the out-of-room task refusal to
   * therest().  "Please be more clear, who do you want to give to?" is in no
   * Runner's string pool.  Measured on great.taf (3.80) under run380: `give
   * picasso to julie` in Mrs Walters' living room, Julie elsewhere and task
   * 22 confined to room 4, is "You can't do that here."
   * (runner_probes/great_escape.run380.rtf).
   */
  npc = lib_disambiguate_npc_pick (game, "give to", &is_ambiguous,
                                   NPC_PICK_FIRST);
  if (npc == -1)
    return is_ambiguous;

  /* Reject if not holding the object offered; below 3.90 with a full stop,
     see lib_give_not_held_pre390(). */
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
    {
      lib_give_not_held_pre390 (game, object);
      return TRUE;
    }

  /* After all that, the npc is disinterested. */
  pf_new_sentence (filter);
  lib_print_npc_np (game, npc);
  lib_print_wrapped_object (game, " doesn't seem interested in ",
                            object, ".\n");
  return TRUE;
}

/*
 * Set by lib_cmd_give_object() when 3.9's give leaves the line to the object
 * catch-all; the `give *` row right below it then declines too, once.
 */
scr_bool lib_give_defer_catch_all = FALSE;

scr_bool
lib_cmd_give_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "give", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject if not holding the object offered; below 3.90 with a full stop,
     see lib_give_not_held_pre390(). */
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
    {
      lib_give_not_held_pre390 (game, object);
      return TRUE;
    }

  /*
   * run390's therest give (45D696-45D816) asks "to who?" only when no
   * character's Name or first Alias is anywhere in the line, whatever room
   * they are in.  A named one elsewhere leaves the buffer empty, and the
   * line falls to the object catch-all (46024A), which runs before
   * characters() (460675).  Measured p39ABSNPC
   * (runner_probes/absnpc.run390.txt): `give stone to erin` (seen), `give
   * stone to fred` (never seen), `give erin stone` and `give stone to girl`
   * (Alias) from the next room all answer "I don't understand what you want
   * me to do with the stone.".
   */
  if (prop_get_taf_version (gs_get_bundle (game)) == TAF_VERSION_390)
    {
      const scr_char *input = run_get_dispatch_input ();
      scr_int npc;

      for (npc = 0; input && npc < gs_npc_count (game); npc++)
        {
          if (lib_npc_referenced (game, npc, input))
            {
              lib_give_defer_catch_all = TRUE;
              return FALSE;
            }
        }
    }

  /* After all that, we have to ask (and shouldn't this be "to whom?"). */
  {
    const scr_int npc = lib_give_present_npc_400 (game);

    if (npc >= 0)
      return lib_give_not_interested_400 (game, npc, object);
  }
  lib_question_prefix_from_line (game);
  lib_print_wrapped_object (game, "Give ", object, " to who?\n");
  return TRUE;
}


/*
 * lib_wear_backend()
 *
 * Common backend handler for wearing objects.  Puts on all objects currently
 * referenced in the game, moving objects to worn by player.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
static void
lib_wear_backend (scr_gameref_t game)
{
  scr_int object_count, object;
  scr_bool has_printed;
  lib_list_t list;

  /*
   * Try game commands for all referenced objects first.  If any succeed,
   * remove that reference from the list.
   */
  has_printed = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      /*
       * 4.0's wears (463C30) pre-matches nothing: its body has no call to
       * either dispatcher (453C50 / 44CCE0), so only the typed line ever
       * reaches the tasks.  British Fox's `wear british fox costume` misses
       * task 477's "wear {British Fox's} costume" and run400 answers "You put
       * on British Fox's costume." from the library, where the rebuilt
       * "wear British Fox's costume" (or bare "wear costume", which hits
       * 'wear {my} costume') ran the task (runner_transcripts britishfox
       * T16, and T97 `wear eleanor's clothes`).
       */
      if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400)
        continue;

      if (lib_try_game_command_short (game, "wear", object))
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
        }
    }

  /* Wear every object referenced. */
  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      list.push_back (object);
      gs_object_player_wear (game, object);
    }

  /*
   * Pre-3.9 announces the wear with the object's own prefix where every other
   * report normalizes it: run370 and run380 answer "You put on a rusty w3."
   * and "You put on a w1." for an empty prefix, against "You pick up the rusty
   * w3." on the take path in the same session.  A "some" or "the ..." prefix
   * comes out verbatim either way, so the "a ..." and empty cases are what
   * separate the two printers.  3.9 normalized this along with the rest:
   * run390 and run400 both say "You put on the w0, ..." for the very same
   * objects.  Measured on the pwear probe carried across all four Runners
   * (2026-08-23); the empty-prefix default is a bare "a" even before a vowel,
   * measured separately on pwearv.  See
   * test/adrift4/notes/RUNNER_TESTS_TODO.md section 4.
   *
   * Scarier deliberately uses the definite form in every version (deviation
   * policy): "You put on a underwear." is the substituted article at its
   * worst, and an indefinite one on the object just put on reads wrong.
   */
  has_printed |= lib_print_object_list (game, has_printed, list, " and ", '.',
                                        "You put on ",
                                        "I put on ",
                                        "%player% put on ",
                                        lib_print_object_np);

  /* Note any remaining multiple references left out of the wear operation. */
  lib_drain_multiple_references_if
    (game, object_count, list, [game] (scr_int item)
     { return gs_object_position (game, item) == OBJ_WORN_PLAYER; });

  /*
   * The 4.0 Runner ends this with "!" (4638FE: ... & var_CC where var_CC is
   * "!"); the pre-4.0 Runners build the wear-path variant of the message
   * with NO terminator at all -- run390 43CF8B and run380 432FCB
   * (`... & " already wearing " & var_208(0) & " " & var_208(4)`) and run370
   * 42C7B6 all stop at the name.  Below 3.9 that name is the plain
   * Prefix & " " & Short the concatenation shows; 3.9 and 4.0 call the
   * definite printer instead (42B0E8 / 448710).
   *
   * p37TWO/p38TWO/p39TWO `wear hat` with the hat already worn (cells 9 and 22
   * of runner_probes/two.run370.verb3.rtf /
   * runner_probes/two.run380.verb3.rtf / runner_probes/two.run390.verb3.txt,
   * 2026-09-21): "You are already wearing a hat" twice over at 3.7 and 3.8,
   * "You are already wearing the hat" at 3.9, in every case with the sentence
   * left open.  Scarier deliberately keeps the full stop there (deviation
   * policy: the open sentence is a display accident, not text worth
   * matching).
   */
  has_printed |= lib_print_object_list (game, has_printed, list, " and ",
                                        lib_is_version_400 (game) ? '!' : '.',
                                        "You are already wearing ",
                                        "I am already wearing ",
                                        "%player% is already wearing ",
                                        lib_object_printer_390 (game));

  lib_drain_multiple_references_if
    (game, object_count, list, [game] (scr_int item)
     { return gs_object_position (game, item) != OBJ_HELD_PLAYER; });

  /* Pre-3.9 wears() names these by Prefix & " " & Short (run380 433218,
   * 4331B1): p38EXAM `wear stone` with the stone on the floor is "You are not
   * holding a stone." (runner_probes/exam.run380.txt, and run370
   * runner_probes/exam.run370.txt). */
  const lib_print_item_t wear_item = lib_object_printer_390 (game);

  has_printed |= lib_print_object_list (game, has_printed, list, " or ", '.',
                                        "You are not holding ",
                                        "I am not holding ",
                                        "%player% is not holding ",
                                        wear_item);

  lib_drain_multiple_references (game, object_count, list);

  lib_print_object_list (game, has_printed, list, " or ", '.',
                         "You can't wear ",
                         "I can't wear ",
                         "%player% can't wear ", wear_item);
}


/*
 * lib_wear_filter()
 *
 * Helper function for deciding if an object may be worn in this context.
 * Returns TRUE if an object may be worn, FALSE otherwise.
 */
static scr_bool
lib_wear_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  assert (unused == -1);

  /*
   * The object is wearable if the player is holding it, and it's not static
   * (static moved to player inventory by event), and if it's marked wearable
   * in properties.
   */
  if (gs_object_position (game, object) == OBJ_HELD_PLAYER
      && !obj_is_static (game, object))
    {
      scr_vartype_t vt_key[3];

      /* Return wearability from the object properties. */
      vt_key[0].string = "Objects";
      vt_key[1].integer = object;
      vt_key[2].string = "Wearable";
      return prop_get_boolean (bundle, "B<-sis", vt_key);
    }

  return FALSE;
}


/*
 * lib_cmd_wear_all()
 *
 * Wear all wearable objects currently held by the player.
 */
scr_bool
lib_cmd_wear_all (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_wear_filter, -1, FALSE, nullptr);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_wear_backend (game);
  else
    {
      /*
       * " that can be worn." is SCARE's own: no Runner holds it, or any
       * fragment of it.  All four print the one literal
       * " don't have anything to wear." after Ary(0) -- un-conjugated, so
       * the third person reads "%player% don't have ...".  Measured live
       * 2026-09-05, run400 on ptbad.taf with an empty inventory.
       */
      pf_buffer_string (filter,
                        lib_select_response (game,
                                           "You don't have anything to wear.",
                                           "I don't have anything to wear.",
                                           "%player% don't have anything to wear."));
    }

  pf_buffer_answer_break (filter);
  return TRUE;
}


/* How many of a 4.0 resolver tie the player could put on. */
static scr_int
lib_wear_tie_wearables (scr_gameref_t game, const std::vector<scr_int> &tied)
{
  scr_int count = 0;
  std::size_t index_;

  if (tied.size () < 2)
    return 0;
  for (index_ = 0; index_ < tied.size (); index_++)
    {
      if (lib_wear_filter (game, tied[index_], -1))
        count++;
    }
  return count;
}

/*
 * lib_wear_multiple_common()
 *
 * Wear the wearable objects held by the player and listed in %text%, or --
 * for is_except -- every one of them but those listed.
 */
static scr_bool
lib_wear_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects, references;

  /*
   * 4.0 wears() on a line with neither "all" nor "and" marks only the
   * Proc_21_58_463640 winner of the whole line (46384F): present and seen,
   * then seen.  A tie or no match marks nothing, and the empty message
   * becomes "Wear what?" (463C19).  Measured beer turn 11 `wear jumper`,
   * the held woolly jumper tying the fountain's "several people" (alias
   * jumper).
   *
   * "Wear what?" is not the last word on a tie, though.  The line-start
   * 463640 walk (48A3F5, mode 0) has already parked the pending object in
   * Me(424) before wears() runs, and wears() only resets it after putting
   * something on (463AC0), so the turn tail's question (48B6B1) replaces
   * the refusal, no turn: p4WTIE `wear stone` with both stones on the floor
   * is "Which stone.  The red stone or the blue stone?" (run400
   * runner_probes/wtie.run400.it1.txt, 2026-09-21).
   *
   * Deliberate deviation: a tie with exactly one held wearable among the
   * tied objects wears that one, through the ordinary parse below, as it
   * did before 782dab11e.  beer's `wear jumper` puts on the woolly jumper.
   */
  if (!is_except && prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    {
      const scr_char *input = run_get_dispatch_input ();
      std::vector<scr_int> tied;

      if (input && !lib_input_contains_word (input, "all")
          && !lib_input_contains_word (input, "and")
          && lib_verb_object_resolve_400_string (game, input, &tied, TRUE) < 0
          && lib_wear_tie_wearables (game, tied) != 1
          && lib_verb_object_resolve_400_string (game, input, &tied, FALSE) < 0
          && lib_wear_tie_wearables (game, tied) != 1)
        {
          lib_question_prefix_from_line (game);
          if (lib_co_400_raise_for_pending_tie (game))
            return TRUE;
          return lib_what (game, "Wear");
        }
    }

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "retain" : "wear",
                                   lib_wear_filter, -1,
                                   &references))
    return FALSE;
  else if (references == 0)
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              lib_wear_filter, -1, is_except,
                              &references);
  if (objects > 0 || references > 0)
    lib_wear_backend (game);
  else
    lib_print_nothing_held (game, FALSE, is_except && objects == 0,
                            " that can be worn.");

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_cmd_wear_except_multiple()
 * lib_cmd_wear_multiple()
 *
 * Facets of lib_wear_multiple_common().
 */
scr_bool
lib_cmd_wear_except_multiple (scr_gameref_t game)
{
  return lib_wear_multiple_common (game, TRUE);
}

/*
 * lib_wear_is_put_line_380()
 *
 * At 3.80 and 3.90 a put line is never a wear, because insides() moved.
 * All three Runners enter insides()' put branch on the same test -- c("put")
 * with c("inside"), c("into"), c("in") or c("on") (run370 4399CD, run380
 * 445746, run390 460EDC) -- but 3.70 calls insides() from the BOTTOM of
 * generaltasks, below tasks(0), wears(), removes(), the hints and even the
 * screen clear, and as a plain `Call` that can never claim (43BA0A).  3.80
 * hoisted it above tasks(0) and made it claiming, `If CBool(insides()) Then
 * GoTo` (4421DA), and 3.90 kept it there (45F471) -- so from 3.80 up the put
 * branch answers first and wears() never sees the line.  4.0 gave the wear
 * back: `put on X` is a wear there whenever the object is held or worn.
 *
 * p37TWO/p38TWO/p39TWO/p4TWO (runner_probes/two.run*.puton.*, 2026-09-21):
 * `put on hat` with the hat held, worn and on the floor is "You put on a
 * hat." / "You are already wearing a hat" / "You are not holding a hat." at
 * 3.70 -- wears()' own three answers, its entry taking c("put on") outright
 * (run370 42C533) -- and at 3.80 it is "You can't do that!" all three times,
 * insides() refusing a line that co() names fewer than twice (445A2A) before
 * it looks at any target.  3.90 answers `put on hat` the same way, but `put
 * hat on` "Put the hat onto what?": its target pass takes only a name
 * standing after InStr(line, "on") (461000), so the trailing spelling leaves
 * no target and the object's own question wins.  Both fall out of the
 * ordinary put handlers once the wear declines.
 */
scr_bool
lib_wear_is_put_line_380 (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();

  if (version < TAF_VERSION_380 || version >= TAF_VERSION_400 || !line)
    return FALSE;
  if (run_c_word_pre400 (version, line, "put") < 0)
    return FALSE;
  return run_c_word_pre400 (version, line, "inside") >= 0
         || run_c_word_pre400 (version, line, "into") >= 0
         || run_c_word_pre400 (version, line, "in") >= 0
         || run_c_word_pre400 (version, line, "on") >= 0;
}


static scr_bool lib_remove_filter (scr_gameref_t game, scr_int object,
                                   scr_int unused);

/*
 * lib_wear_would_act_390()
 *
 * TRUE when wears() would ACT on this line: put on an object the player
 * holds, or answer "You are already wearing ..." for one already on.
 *
 * Those two arms write their message unguarded -- run390's already-wearing
 * literal at 43CF8B and run400's at 4638DE/4638FE, the put-on move and its
 * report at 463965/4639F2 -- where BOTH of wears()' refusals are guarded by
 * an EMPTY message buffer: run390 43D1EF `If var_18C(22) <> 0 And
 * MemVar_468154 = "" Then` in front of " not holding " (43D220) and run400
 * 463B8B the same test in front of 463BBC, with " can't wear " behind a
 * buffer that is empty or still holds that very refusal (run390
 * 43D188-43D1A2, run400 463AC6-463B3F).
 *
 * The object walk is each Runner's own: co() over the whole line below 4.0,
 * the 463640 scorer at 4.0 (see lib_wear_multiple_common()).
 */
scr_bool
lib_wear_would_act_390 (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();
  scr_int object;

  if (!line || line[0] == NUL)
    return FALSE;

  if (version >= TAF_VERSION_400)
    {
      object = lib_verb_object_resolve_400_string (game, line, nullptr, TRUE);
      if (object < 0)
        object = lib_verb_object_resolve_400_string (game, line,
                                                     nullptr, FALSE);
      return object >= 0
             && (lib_wear_filter (game, object, -1)
                 || lib_remove_filter (game, object, -1));
    }

  GS_FOR_EACH_OBJECT (game, object)
    {
      if (!lib_co_pre400 (game, line, object, 1))
        continue;
      if (lib_wear_filter (game, object, -1)
          || lib_remove_filter (game, object, -1))
        return TRUE;
    }
  return FALSE;
}


/*
 * lib_wear_yields_to_put_390()
 *
 * From 3.90 a put refusal no longer settles a put/wear line by itself: the
 * handler that wrote it can leave the line UNCLAIMED, and wears() runs
 * afterwards and overwrites the message whenever it can act, staying silent
 * behind its empty-buffer guards when it cannot (lib_wear_would_act_390()).
 * 3.80 is the odd one out at either spelling: insides() claims outright
 * there (4421DA), so a put line is never a wear whatever the object's
 * state.
 *
 * Which refusal it is decides the matter at 3.90, and the spelling decides
 * which refusal.  insides()' target pass takes a name standing after
 * InStr(line, "on") (461000), so `put on hat` hands it the hat as the
 * CONTAINER, the line names fewer than twice, and the "You can't do that!"
 * arm (461646) CLAIMS; `put hat on` leaves it no target at all and the
 * object's own "Put the hat onto what?" question (461754) does not.  Only
 * the trailing spelling can become a wear there -- which is also the only
 * one of the two that wears() would enter on its third clause, `c("put")
 * And Right(line, 2) = "on"` (43CD0B-43CD29).  4.0 draws no such line:
 * put_drop_list's question never claims, and either spelling is a wear.
 *
 * p3xTWO/p4TWO (runner_probes/two.run*.puton.* and
 * runner_probes/two.run*.puton2.*), 2026-09-21.  With the hat held `put hat
 * on` is "You put on the hat." at 3.70, 3.90 and 4.00 and "You can't do
 * that!" at 3.80; worn, it is "You are already wearing the hat" everywhere
 * but 3.80.  `put on hat` is that same wear at 3.70 and 4.00 and "You can't
 * do that!" at 3.80 AND 3.90, held, worn or on the floor.  With the coin --
 * held, not wearable -- `put coin on` is 3.90's "Put the coin onto what?" and
 * 4.00's "Where do you want to put the coin?", the put refusals standing
 * because the wear could not act.
 */
static scr_bool
lib_wear_yields_to_put_390 (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();

  if (version >= TAF_VERSION_400)
    {
      /* The rows above have already cut the spelling down to `put on X`
       * and `put X on`, which is wears()' own entry (463C30, c("put on")
       * or a line ending " on"); put_drop_list enters on the bare word. */
      if (!line || !lib_input_contains_word (line, "put"))
        return FALSE;
      return !lib_wear_would_act_390 (game);
    }

  if (!lib_wear_is_put_line_380 (game))
    return FALSE;

  /*
   * Deliberate deviation: at 3.80 and 3.90 either spelling is a wear
   * whenever wears() could act, as at 4.0.  The Runners' answers, "You
   * can't do that!" (3.80 both, 3.90 `put on hat`), are described above.
   */
  return !lib_wear_would_act_390 (game);
}


scr_bool
lib_cmd_wear_multiple (scr_gameref_t game)
{
  if (lib_wear_yields_to_put_390 (game))
    return FALSE;
  return lib_wear_multiple_common (game, FALSE);
}


/*
 * lib_remove_backend()
 *
 * Common backend handler for removing objects.  Takes off on all objects
 * currently referenced in the game, moving objects to held by player.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
static void
lib_remove_backend (scr_gameref_t game)
{
  scr_bool has_printed;

  has_printed = lib_move_try_commands (game, "remove", FALSE);
  lib_move_backend (game, &LIB_REMOVE_VERB, -1, has_printed);
}


/*
 * lib_remove_filter()
 *
 * Helper function for deciding if an object may be removed in this context.
 * Returns TRUE if an object is currently being worn, FALSE otherwise.
 */
static scr_bool
lib_remove_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return !obj_is_static (game, object)
         && gs_object_position (game, object) == OBJ_WORN_PLAYER;
}


/*
 * lib_two_verb_line_pre400()
 *
 * A line naming TWO library verbs, below 4.0.  Every one of the five
 * handlers Scarier anchors enters on a whole-word c() test over the WHOLE
 * line, so `x get coin` satisfies takes() and examines() alike, and
 * generaltasks calls them in ONE fixed order (run380 4421A3-442201, run370
 * 43B958-43B98A, run390 45F439-45F49E):
 *
 *     takes, drops, inventory, insides, tasks, wears, removes, ... therest
 *
 * A handler that ACTS claims the line -- `If CBool(takes()) Then GoTo` the
 * turn tail -- while one that only writes a refusal does not, and the next
 * handler runs on and may overwrite it.  So WORD ORDER NEVER DECIDES below
 * 4.0: `take drop coin` and `drop take coin` both answer from the same
 * order, and which of them speaks depends only on where the object is.
 *
 * Measured on p3xREW (runner_probes/rew.run370.verb.rtf,
 * runner_probes/rew.run380.verb.rtf, runner_probes/rew.run390.verb.txt,
 * runner_probes/rew.run370.verb2.rtf, runner_probes/rew.run380.verb2.rtf,
 * runner_probes/rew.run390.verb2.txt), and on the new wearable probe p3xTWO
 * (make_twoverbprobe.py; runner_probes/two.run370.verb3.rtf,
 * runner_probes/two.run380.verb3.rtf, runner_probes/two.run390.verb3.txt),
 * 2026-09-21.  The cells that carry the rule:
 *
 *   coin loose   `x get coin`     "You pick up the coin."   takes acts
 *   coin held    `get x coin`     "A gold coin."            takes cannot,
 *                                                           examines does
 *   coin held    `wear take coin` "You've already got the coin!"
 *   coin loose   `remove drop coin` "You don't have the coin!"
 *   coin loose   `remove wear coin` "You are not holding the coin."
 *   hat loose    `remove take hat` "You pick up the hat."    takes acts
 *   hat worn     `take remove hat` "You remove the hat."     takes cannot,
 *                                                            removes does
 *   hat worn     `drop take hat`  "You drop the hat."        drops takes a
 *                                                            worn object
 *   hat worn     `x take hat`     "A felt hat."
 *   hat held     `x take off hat` "A felt hat."
 *   hat held     `take off hat`   "You've already got the hat!"  (3.70;
 *                                 3.80 rewrites take->get first, 441C61,
 *                                 so removes never enters there either)
 *   hat worn     `take off hat`   "You remove the hat."      (3.70/3.90)
 *   nothing      `drop take`      "Take what?"
 *   nothing      `examine wear`   "Wear what?"
 *
 * The last two are why examines is not simply last: with an object named it
 * overwrites whatever refusal the four above it left (it is down in therest,
 * which runs below everything), but with NOTHING named its "Nothing
 * special." only ever fills an EMPTY buffer, so the earlier handler's
 * question survives.
 *
 * Scarier answers a line from one anchored row, so the port is a re-spelling
 * like run_hoist_verb_line()'s: work out which handler the Runner's order
 * would leave speaking, and hand the row that handler's own line.  Narrow on
 * purpose -- a list line ("all", "and"), a static object and a name two
 * objects answer to are all left exactly as they were, the first two because
 * their arms walk co() themselves and the last because the disambiguation
 * the Runner's namesake count asks for is not this rule's business.
 */
enum
{
  LIB_PRE400_TAKE, LIB_PRE400_DROP, LIB_PRE400_WEAR,
  LIB_PRE400_REMOVE, LIB_PRE400_EXAMINE, LIB_PRE400_HANDLERS
};

/* Every word the five entry tests hold, longest phrases first, so that
   "take off" is stripped whole where "take" alone would leave "off". */
static const scr_char *const LIB_PRE400_VERB_WORDS[] = {
  "put down", "put on", "take off", "look at", "look in", "put",
  "get", "take", "pick", "drop", "leave", "wear", "remove",
  "examine", "exam", "read", "ex", "x", nullptr
};

static const scr_char *const LIB_PRE400_CANONICAL[LIB_PRE400_HANDLERS] = {
  "take", "drop", "wear", "remove", "examine"
};

/*
 * run370 adds the game's own word for each handler from its command block,
 * c(MemVar_4460FC(N)) beside its own spellings: examines 434E2A slot 10,
 * takes 435E28 slot 11, drops 430475 slot 12, wears 42C533 slot 13,
 * removes 4295FF slot 14.  The word stays in the line -- run370 has no
 * synonyms, so a task hears `grab blip` as typed -- and it is only one
 * more entry word: take still outranks examine on `peer grab bag`, and the
 * word can be half an object's name (`x grab bag` picks the bag up).  The
 * take slot's test is `c(slot) And Not c("from")`, the And binding before
 * the Or, so `grab coin from box` never enters takes while `take coin from
 * box` does.  p37SLOT (harness/make_37_slotprobe.py), run370x
 * runner_probes/slot.run370.rtf and its scrollback dump
 * runner_probes/slot.run370.dump.txt, 2026-09-21.  The slot words, by
 * handler, empty where the game kept the standard word (which the own
 * spellings already hold) or below 3.70's command block.
 */
static const scr_int LIB_PRE400_SLOT[LIB_PRE400_HANDLERS] = {
  11, 12, 13, 14, 10
};
static const scr_char *const LIB_PRE400_SLOT_DEFAULT[LIB_PRE400_HANDLERS] = {
  "pick up", "put down", "wear", "remove", "examine"
};

static void
lib_pre400_slot_words (scr_gameref_t game,
                       std::string words[LIB_PRE400_HANDLERS])
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int handler;

  for (handler = 0; handler < LIB_PRE400_HANDLERS; handler++)
    {
      words[handler] = lib_command_slot_370 (bundle, LIB_PRE400_SLOT[handler]);
      if (words[handler] == LIB_PRE400_SLOT_DEFAULT[handler])
        words[handler].clear ();
    }
}

/*
 * The five entry tests themselves, as each handler's own c() calls spell
 * them (takes run380 4421A3, drops 4421B8, wears 4421E9, removes 4421F5,
 * examines 441F9A), with 3.70's slot words above.  Counts the handlers the
 * line would enter; SLOT_LINE, if given, says whether a slot word is one of
 * the reasons.
 */
static scr_int
lib_pre400_handler_words_in (scr_gameref_t game, const scr_char *line,
                             scr_bool present[LIB_PRE400_HANDLERS],
                             scr_bool *slot_line)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  std::string slots[LIB_PRE400_HANDLERS];
  scr_int handler, count;

  /* The Runner's c(): whether the line contains WORD. */
  const auto c_word = [&] (const scr_char *word) -> scr_bool
    {
      return run_c_word_pre400 (version, line, word) >= 0;
    };

  present[LIB_PRE400_TAKE] = c_word ("get") || c_word ("take")
                             || (c_word ("pick") && !c_word ("from"));
  present[LIB_PRE400_DROP] = c_word ("drop") || c_word ("put down")
                             || c_word ("leave")
                             || (c_word ("put") && c_word ("down"));
  present[LIB_PRE400_WEAR] = c_word ("wear") || c_word ("put on");
  present[LIB_PRE400_REMOVE] = c_word ("remove") || c_word ("take off");
  present[LIB_PRE400_EXAMINE] = c_word ("x") || c_word ("examine")
                                || c_word ("look at")
                                || c_word ("ex") || c_word ("exam")
                                || c_word ("read")
                                || (version >= TAF_VERSION_380
                                    && c_word ("look in"));

  if (slot_line)
    *slot_line = FALSE;
  lib_pre400_slot_words (game, slots);
  for (handler = 0; handler < LIB_PRE400_HANDLERS; handler++)
    {
      if (slots[handler].empty () || !c_word (slots[handler].c_str ())
          || (handler == LIB_PRE400_TAKE && c_word ("from")))
        continue;
      present[handler] = TRUE;
      if (slot_line)
        *slot_line = TRUE;
    }

  count = 0;
  for (handler = 0; handler < LIB_PRE400_HANDLERS; handler++)
    count += present[handler] ? 1 : 0;
  return count;
}

static scr_int
lib_pre400_handler_words (scr_gameref_t game, const scr_char *line,
                          scr_bool present[LIB_PRE400_HANDLERS],
                          scr_bool *slot_line = nullptr)
{
  /* Deliberate deviation (2026-09-30): a handler word inside a longer
     object or character name enters no handler; see lib_mask_long_names(). */
  const std::string masked = lib_mask_long_names (game, line);

  if (SCR_TRACING_DEVIATIONS () && masked != line)
    {
      scr_bool runner[LIB_PRE400_HANDLERS] = { FALSE };
      scr_bool ours[LIB_PRE400_HANDLERS] = { FALSE };

      lib_pre400_handler_words_in (game, line, runner, nullptr);
      lib_pre400_handler_words_in (game, masked.c_str (), ours, nullptr);
      if (memcmp (runner, ours, sizeof runner) != 0)
        SCR_DEVIATION ("mask_handlers", "masked=\"%s\"", masked.c_str ());
    }
  return lib_pre400_handler_words_in (game, masked.c_str (), present,
                                      slot_line);
}


/*
 * lib_earlier_handler_claims_pre400()
 *
 * TRUE when the line names one of the five handlers generaltasks calls
 * ahead of therest(), so therest's verb cascade never gets to speak for it.
 * run370's therest already knew this -- its absent-object refusal skips a
 * line holding any earlier handler's word (run_therest_absent_370) -- and
 * the verb arms below it are the same story at every pre-4.0 version: with
 * the coin in hand `push take coin` is "You've already got a coin!" and
 * `push drop coin` (coin on the floor) "You don't have a coin!", not the
 * push arm's "but nothing happens", and `push examine coin` is the coin's
 * description at 3.7, 3.8 AND 3.9 -- where Scarier answered 3.9's own therest
 * examine arm, "Nothing special.".  p3xREW cells 31, 35 and 39
 * (runner_probes/rew.run370.verb.rtf, runner_probes/rew.run380.verb.rtf,
 * runner_probes/rew.run390.verb.txt, 2026-09-21).
 */
scr_bool
lib_earlier_handler_claims_pre400 (scr_gameref_t game, const scr_char *line)
{
  scr_bool present[LIB_PRE400_HANDLERS];

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400
      || !line || line[0] == NUL)
    return FALSE;

  return lib_pre400_handler_words (game, line, present) > 0;
}


/*
 * lib_two_verb_rewrite_pre400()
 *
 * The tail of lib_two_verb_line_pre400(): re-spell LINE as HANDLER's canonical
 * verb followed by what is left of the line, FOUND being the one object the
 * line names, or -1.
 */
static scr_bool
lib_two_verb_rewrite_pre400 (scr_gameref_t game, const scr_char *line,
                             scr_int handler, scr_int found,
                             scr_bool bare_put, const std::string *slots,
                             std::string *rewritten)
{
  scr_int handler_word;
  std::string shortname;
  const scr_char *scan;
  std::string rest;

  if (found != -1)
    {
      const scr_char *const name = prop_get_indexed_string (
          gs_get_bundle (game), "Objects", found, "Short");

      shortname = name ? name : "";
    }
  /* The line with every verb word taken out of it, which is what the
     winning handler's own object walk would have been left looking at. */
  for (scan = line; *scan != NUL; )
    {
      const scr_char *const *word;
      scr_int matched = 0;

      if (scan == line || scan[-1] == ' ')
        {
          for (word = LIB_PRE400_VERB_WORDS; *word && !matched; word++)
            {
              const scr_int length = strlen (*word);

              if (!bare_put && strcmp (*word, "put") == 0)
                continue;
              if (scr_strncasecmp (scan, *word, length) == 0
                  && (scan[length] == NUL || scan[length] == ' '))
                matched = length;
            }
          /* A slot word goes too, unless it begins the object's own name:
             `x grab bag` is a take of the grab bag. */
          for (handler_word = 0;
               handler_word < LIB_PRE400_HANDLERS && !matched; handler_word++)
            {
              const std::string &slot = slots[handler_word];

              if (!slot.empty ()
                  && scr_strncasecmp (scan, slot.c_str (), slot.size ()) == 0
                  && (scan[slot.size ()] == NUL || scan[slot.size ()] == ' ')
                  && !(shortname.size () > 0
                       && scr_strncasecmp (scan, shortname.c_str (),
                                           shortname.size ()) == 0
                       && (scan[shortname.size ()] == NUL
                           || scan[shortname.size ()] == ' ')))
                matched = slot.size ();
            }
        }
      if (matched > 0)
        {
          scan += matched;
          scan += strspn (scan, " ");
          continue;
        }
      rest.push_back (*scan++);
    }

  *rewritten = LIB_PRE400_CANONICAL[handler];
  if (!rest.empty ())
    {
      *rewritten += " ";
      *rewritten += rest;
    }
  return TRUE;
}

scr_bool
lib_two_verb_line_pre400 (scr_gameref_t game, const scr_char *line,
                          std::string *rewritten)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  scr_bool present[LIB_PRE400_HANDLERS], acts[LIB_PRE400_HANDLERS];
  scr_int handler, object, found, count;
  scr_bool bare_put = FALSE, slot_line;
  std::string slots[LIB_PRE400_HANDLERS];

  /* The Runner's c(): whether the line contains WORD. */
  const auto c_word = [&] (const scr_char *word) -> scr_bool
    {
      return run_c_word_pre400 (version, line, word) >= 0;
    };
  /* Hand the line to HANDLER, FOUND being the one object it names if any. */
  const auto rewrite = [&] () -> scr_bool
    {
      return lib_two_verb_rewrite_pre400 (game, line, handler, found,
                                          bare_put, slots, rewritten);
    };

  if (version >= TAF_VERSION_400 || !line || line[0] == NUL)
    return FALSE;

  count = lib_pre400_handler_words (game, line, present, &slot_line);
  lib_pre400_slot_words (game, slots);
  /*
   * A put with no container clause is no handler's: run370/380/390's put
   * row wants its in/on, and the bare verb falls through to the catch-all.
   * So beside one of the five it is only a word, and the handler answers:
   * `put x coin` is "A gold coin.", `put take coin` with the coin held
   * "You've already got a coin!", `put drop coin` "You drop the coin.".
   * Cells 56, 69 and 76 (run370x runner_probes/ord.run370.rest.rtf, run380x
   * runner_probes/ord.run380.rest.rtf, run390x
   * runner_probes/ord.run390.rest.txt, 2026-09-21).
   */
  if (c_word ("put") && !c_word ("in") && !c_word ("into")
      && !c_word ("inside") && !c_word ("on") && !c_word ("onto")
      && !c_word ("down"))
    {
      bare_put = TRUE;
      count++;
    }
  /*
   * A 3.70 slot word is no word the rows below know, so a line holding one
   * is re-spelled even with one handler in it: `grab stone` becomes `take
   * stone` for the take row, while the tasks have already heard it as typed.
   */
  if (count < 2 && !slot_line)
    return FALSE;
  if (count < 2 && !bare_put)
    {
      for (handler = 0; !present[handler]; handler++)
        ;
      found = -1;
      return rewrite ();
    }

  /*
   * A list line (`all`, `and`) goes to the list arm of the first handler
   * generaltasks calls that holds one, takes then drops, and that arm walks
   * co() over the whole line on its own: `x take all` and `wear take all`
   * are "You pick up the coin and the box.", `drop x all` "You drop the hat
   * and the coin.", `x take coin and hat` with the hat held "You pick up the
   * coin.".  p37ORD..p39ORD (run370x runner_probes/ord.run370.rest.rtf,
   * run380x runner_probes/ord.run380.rest.rtf, run390x
   * runner_probes/ord.run390.rest.txt, 2026-09-21).  A line with both a take
   * and a drop word is not measured.
   */
  if (c_word ("all") || c_word ("and"))
    {
      if (present[LIB_PRE400_TAKE] == present[LIB_PRE400_DROP])
        return FALSE;
      handler = present[LIB_PRE400_TAKE] ? LIB_PRE400_TAKE : LIB_PRE400_DROP;
      found = -1;
      return rewrite ();
    }

  /* The one object the line names, by co() over the whole line. */
  found = -1;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (!lib_co_pre400 (game, line, object, 1))
        continue;
      if (found != -1)
        {
          /* Left as it was -- unless a slot word would then reach rows that
             do not know it, where the first handler in the call order has
             it instead. */
          found = -1;
          if (!slot_line)
            return FALSE;
          for (handler = 0; !present[handler]; handler++)
            ;
          return rewrite ();
        }
      found = object;
    }
  /*
   * A static object acts for none of the four, and each of their refusals
   * leaves the line open -- except drops()' below 3.90, which claims it --
   * so examines speaks over the rest: `x take bench`, `take x bench`, `wear
   * x bench` and `remove x bench` are all "A stone bench." at 3.70, 3.80 and
   * 3.90, while `drop x bench` is "You don't have a bench!" at 3.70/3.80 and
   * the description at 3.90.  With no examine word the first handler in the
   * call order answers: `drop take bench`, `take drop bench` and `wear take
   * bench` are "You can't take a bench.".  The 3.70 slot words go the same
   * way (`peer grab box`, `doff peer box`; `dump grab box` is the take
   * refusal).  p37SURF..p39SURF (run370x
   * runner_probes/surf.run370.stat2v.rtf, run380x
   * runner_probes/surf.run380.stat2v.rtf, run390x
   * runner_probes/surf.run390.stat2v.txt) and p37SLOT2
   * (runner_probes/slot2.run370.b.rtf plus its scrollback dump
   * runner_probes/slot2.run370.b_dump.txt), 2026-09-21.
   */
  if (found != -1 && obj_is_static (game, found))
    {
      if (version < TAF_VERSION_390 && present[LIB_PRE400_DROP])
        handler = present[LIB_PRE400_TAKE] ? LIB_PRE400_TAKE : LIB_PRE400_DROP;
      else if (present[LIB_PRE400_EXAMINE])
        handler = LIB_PRE400_EXAMINE;
      else
        for (handler = 0; !present[handler]; handler++)
          ;
      return rewrite ();
    }

  acts[LIB_PRE400_TAKE] = found != -1 && lib_take_filter (game, found, -1);
  /* drops() takes a worn object off the player too; see
     lib_drop_named_filter(). */
  acts[LIB_PRE400_DROP] = found != -1
                          && (lib_drop_filter (game, found, -1)
                              || lib_remove_filter (game, found, -1));
  acts[LIB_PRE400_WEAR] = found != -1 && lib_wear_filter (game, found, -1);
  acts[LIB_PRE400_REMOVE] = found != -1 && lib_remove_filter (game, found, -1);
  acts[LIB_PRE400_EXAMINE] = FALSE;

  handler = -1;
  for (object = LIB_PRE400_TAKE; object <= LIB_PRE400_REMOVE; object++)
    {
      if (present[object] && acts[object])
        {
          handler = object;
          break;
        }
    }
  if (handler == -1 && present[LIB_PRE400_EXAMINE] && found != -1)
    handler = LIB_PRE400_EXAMINE;
  for (object = LIB_PRE400_TAKE; handler == -1 && object <= LIB_PRE400_REMOVE;
       object++)
    {
      if (present[object])
        handler = object;
    }
  if (handler == -1)
    handler = LIB_PRE400_EXAMINE;
  return rewrite ();
}


/*
 * lib_cmd_remove_all()
 *
 * Remove all objects currently held by the player.
 */
scr_bool
lib_cmd_remove_all (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_remove_filter, -1, FALSE, nullptr);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_remove_backend (game);
  else
    {
      /*
       * As above: " that can be removed." is SCARE's, and the Runners all
       * assemble Ary(0) & " " & Ary(4) & " not wearing anything.".
       * Measured live 2026-09-05, run400 on ptbad.taf.
       */
      pf_buffer_string (filter,
                        lib_select_response (game,
                                           "You are not wearing anything.",
                                           "I am not wearing anything.",
                                           "%player% is not wearing anything."));
    }

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_remove_multiple_common()
 *
 * Remove the objects worn by the player and listed in %text%, or -- for
 * is_except -- every one of them but those listed.
 *
 * The two forms disagree over the empty complaint: the except form says
 * "not wearing", the plain one "not holding".  Kept as it stands upstream.
 */
static scr_bool
lib_remove_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects, references;

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "retain" : "remove",
                                   lib_remove_filter, -1,
                                   &references))
    return FALSE;
  else if (references == 0)
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              lib_remove_filter, -1, is_except,
                              &references);
  if (objects > 0 || references > 0)
    lib_remove_backend (game);
  else
    lib_print_nothing_held (game, is_except, is_except && objects == 0,
                            " that can be removed.");

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_cmd_remove_except_multiple()
 * lib_cmd_remove_multiple()
 *
 * Facets of lib_remove_multiple_common().
 */
scr_bool
lib_cmd_remove_except_multiple (scr_gameref_t game)
{
  return lib_remove_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_remove_multiple (scr_gameref_t game)
{
  return lib_remove_multiple_common (game, FALSE);
}


/*
 * lib_cmd_inventory()
 *
 * List objects carried and worn by the player.
 *
 * Listing reveals: every object this prints is marked seen, exactly as the
 * NPC lister marks an NPC's possessions (lib_list_npc_inventory).  That is
 * the ONLY thing that reveals a possession the player was never shown --
 * there is no standing "anything you hold is seen" rule, which is what this
 * port used to have.
 *
 * Probe SEEN, driven in run400 2026-09-07 (runner_probes/seen.run400.txt),
 * walks the four ways into the player's possession with four hidden objects
 * and asks `x <name>` after each:
 *
 *   zza  task action, move object -> held by player   x alpha  "A probe object."
 *   zzb  task action, move object -> worn by player   x bravo  "A probe object."
 *   zzc  task action, move object -> the player's
 *        room, no room description printed            x gamma  "A probe object."
 *   zzd  a task that starts an event whose Obj1
 *        goes -> held by player                       x delta  "You see no such
 *                                                               thing."
 *
 * The first three are the task mover's own post-move seen stamp (see
 * task_move_object); the event mover has no such stamp off the player-room
 * branch (evt_move_object), so an event can put something in the player's
 * hands and leave it unreferenceable.  `i` on the next line lists all four,
 * and `x delta` immediately after it answers "A probe object." -- the
 * listing, and nothing else, is what let go of it.
 *
 * yak_shaving is the row that asked the question: its jar of pickled eggs is
 * object 0, InitialPosition hidden, and the Dada Lama's event hands it to the
 * player.  run400 answers `x eggs`, `open eggs` and `give eggs to acolyte`
 * with the not-here refusals for the whole game -- the player never once
 * refers to the jar by noun -- while `give*eggs*lama` and `give*eggs*yeti`,
 * task patterns that resolve no noun, fire normally.
 */
scr_bool
lib_cmd_inventory (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  scr_bool wearing;
  lib_list_t list;

  /* Find and list each object worn by the player. */
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (gs_object_position (game, object) == OBJ_WORN_PLAYER)
        {
          list.push_back (object);
          gs_set_object_seen (game, object, TRUE);
        }
    }
  wearing = !list.empty ();
  if (wearing)
    {
      lib_print_clause (game, FALSE,
                        "You are wearing ",
                        "I am wearing ",
                        "%player% is wearing ");
      lib_print_list (game, list, lib_print_object, " and ");
    }

  /* Find and list each object owned by the player. */
  list.clear ();
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
        {
          list.push_back (object);
          gs_set_object_seen (game, object, TRUE);
        }
    }
  if (!list.empty ())
    {
      if (wearing)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                            ", and you are carrying ",
                                            ", and I am carrying ",
                                            ", and %player_pronoun% is carrying "));
        }
      else
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You are carrying ",
                                                 "I am carrying ",
                                                 "%player% is carrying "));
        }
      lib_print_list (game, list, lib_print_object, " and ");
      pf_buffer_character (filter, '.');

      /*
       * Print contents of every container and surface carried.  Pre-3.9 the
       * lister is whatisin1 (run380 42998C, run370 42B78E): "  Inside <the
       * X> is <a list>." for a surface as much as a container -- `i` with a
       * key on a held tray is "Inside the tray is a key." in run370x and
       * run380x -- and run380 skips a closed container where run370 lists
       * its contents too ("Inside the chest is a ring." with the chest
       * closed; p37TKB/p38TKB cells 2/157, runner_probes/tkb.run370.rtf and
       * runner_probes/tkb.run380.rtf, 2026-09-20).
       * Scarier deliberately keeps its own listing at every version instead
       * (deviation policy): a surface's contents are "on" it, and a closed
       * container shows nothing, as everywhere else in the game.  What the
       * Runner's listing reveals is still revealed -- a run370 closed
       * container's contents are marked seen though not printed.
       */
      const scr_int inventory_version = prop_get_taf_version (gs_get_bundle (game));
      GS_FOR_EACH_OBJECT (game, object)
        {
          if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
            continue;
          if (inventory_version < TAF_VERSION_390
              && (obj_is_container (game, object)
                  || obj_is_surface (game, object))
              && !(inventory_version >= TAF_VERSION_380
                   && obj_is_container (game, object)
                   && gs_object_openness (game, object) > OBJ_OPEN))
            {
              scr_int contained;

              for (contained = 0; contained < gs_object_count (game);
                   contained++)
                {
                  if ((gs_object_position (game, contained) == OBJ_IN_OBJECT
                       || gs_object_position (game, contained)
                          == OBJ_ON_OBJECT)
                      && gs_object_parent (game, contained) == object)
                    gs_set_object_seen (game, contained, TRUE);
                }
            }
          lib_list_in_on_object (game, object, TRUE);
        }
      pf_buffer_answer_break (filter);
    }
  else
    {
      if (wearing)
        {
          pf_buffer_string (filter, ", and ");
          pf_buffer_string (filter,
                            lib_select_response (game,
                                            "you are carrying nothing.\n",
                                            "I am carrying nothing.\n",
                                            "%player% is carrying nothing.\n"));
        }
      else
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                            "You are carrying nothing.\n",
                                            "I am carrying nothing.\n",
                                            "%player% is carrying nothing.\n"));
        }
    }

  /* Successful command. */
  return TRUE;
}


/*
 * lib_list_in_object_pre_390()
 *
 * The listing the 3.8 open handler appends: whatisin1 (run380 @42998C) and
 * whatisin2 (@4297AC) each gather every object whose parent field names the
 * opened object -- in or on, they never look at which -- and print them as
 * "  Inside <the object> is a, b and c." from the one "  Inside " literal
 * the Runner has (see lib_list_in_object).  The object only has to be a
 * surface or container of some kind (field 29 nonzero) and not closed, so
 * an openable SURFACE lists this way too: Crime_Adventure.taf (3.80) keeps
 * a penny and a golf ball on a closed dresser (SurfaceContainer 2), and
 * run380 answers `open dresser` with "You open the dresser.  Inside the
 * dresser is a penny and a golf ball." (measured 2026-09-04) where the
 * container-only lister said nothing.  whatisin1, the held case, marks each
 * listed object seen; whatisin2, the static case, does not.  run370 is the
 * same pair (@42B78E).
 */
scr_bool
lib_list_in_object_pre_390 (scr_gameref_t game, scr_int container)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  lib_list_t list;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && gs_object_parent (game, object) == container)
        {
          list.push_back (object);
          if (!obj_is_static (game, container))
            gs_set_object_seen (game, object, TRUE);
        }
    }
  if (list.empty ())
    return FALSE;

  /*
   * The Runner prints a literal " is " whatever the count.  Scarier
   * deliberately makes the verb agree with the list instead (deviation
   * policy; see lib_select_list_plurality).
   */
  pf_buffer_string (filter, "  Inside ");
  lib_print_object_np (game, container);
  pf_buffer_string (filter,
                    lib_select_list_plurality (game, container, list,
                                               " is ", " are "));
  lib_print_list (game, list, lib_print_object, " and ");
  pf_buffer_character (filter, '.');
  return TRUE;
}
