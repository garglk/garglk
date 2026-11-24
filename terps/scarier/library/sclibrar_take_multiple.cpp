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
 * Taking objects, the command forms: "take all", a noun that names nothing
 * here, "take X and Y", and the multiple-object handler they all end in.
 * The backend they run is in sclibrar_take.cpp; the pre-4.0 co() name test,
 * the takes() loop's answers and the 3.7-3.9 helpers are in
 * sclibrar_take_pre400.cpp.
 */

#include "sclibrar_take_internal.h"


/*
 * lib_take_filter()
 *
 * Helper function for deciding if an object may be acquired in this context.
 * Returns TRUE if an object may be acquired, FALSE otherwise.
 */
scr_bool
lib_take_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  /*
   * To be take-able by name, an object must be visible in the room, not
   * static, and not already held or worn by the player or an NPC.  Note
   * that obj_indirectly_in_room() recurses only through open containers
   * and surfaces, so a named take (or "take X and Y") reaches objects
   * inside an open container or on a surface present in the room -- the
   * Runners' "and"-list take mode does the same -- while excluding the
   * contents of closed containers.
   *
   * Not on 3.7, though, unless the line says get.  Reaching inside is
   * run380's takes() rewrite of a plain "take X" into "take X from
   * <parent>" (43E47B; see lib_take_container_unheld).  run370 has the same
   * rewrite, but its insides() only takes a get or remove line (see the
   * run370 insides() note above), so for take or pick up a bare take naming
   * something that sits in or on another object names nothing at all and
   * falls to the catch-all.  Measured on the same turn of two probes, `take
   * coin` with the coin inside an open box standing on the cave floor: run370
   * answers "Take what?" (p37DARK, runner_probes/dark.run370.putin2.txt:48)
   * where run380 answers "You are not holding a box." (p38DARK,
   * runner_probes/dark.run380.putin2.txt:48), i.e. run380 rewrote the line
   * and then refused it on the hold gate.  2026-09-12.  The explicit form is
   * untouched -- run370 plays `get coin from box` perfectly well
   * (runner_probes/dark.run370.putin.txt:27) and applies the same hold gate to
   * it (runner_probes/dark.run370.putin2.txt:18).
   */
  return obj_indirectly_in_room (game, object, gs_playerroom (game))
         && !obj_is_static (game, object)
         && !(gs_object_position (game, object) == OBJ_HELD_PLAYER
              || gs_object_position (game, object) == OBJ_WORN_PLAYER)
         && !(gs_object_position (game, object) == OBJ_HELD_NPC
              || gs_object_position (game, object) == OBJ_WORN_NPC);
}


/*
 * lib_take_all_filter()
 *
 * The universe that "all" ranges over, which is much narrower than the set
 * a named take can reach: every Runner builds the "all" candidate list from
 * dynamic objects lying directly on the floor of the player's room (run380
 * takes() requires location == current room and static == 0; run400's take
 * handler, Proc_19_6, does the same plus a seen test).  Objects inside or
 * on top of other objects are never included, even when the container is
 * open and its contents revealed -- only naming them, or "take all from X",
 * reaches inside.  A direct consequence is that "all" also leaves alone
 * anything already in the player's possession, including the contents of a
 * carried open container: in Ticket to No Where, holding the open bag of
 * shopping and typing "get all" answers "You take the pamphlet." and leaves
 * the tights, pet food, deodorant and gloves in the bag, while "get paper"
 * still lifts the scrap of paper out of the carried wallet (verified live
 * against run400.exe).
 *
 * The seen test does need porting: since obj_mark_room_objects_seen() moved
 * the room's marking into the room lister, a loose object in a room whose
 * description never printed is unseen, and 4.0 leaves it where it lies.
 */
static scr_bool
lib_take_all_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return !obj_is_static (game, object)
         && gs_object_position (game, object) == gs_playerroom (game) + 1
         && (!lib_matcher_requires_seen (game)
             || gs_object_seen (game, object));
}


/*
 * lib_take_all_sweep_390()
 *
 * 3.9's `take all` also empties every open container and every surface in
 * the room, after the floor.  run390 takes() (4558CE) runs, when the line has
 * "all", over every object that lies on the room's floor or is a static
 * present in the room, and is an open container or a surface; for each it
 * sets the line to "get all" and calls insides() with it, which is the
 * take-from handler (its gate is `c("from") Or (c("all") And container > 0)`).
 * The floor's own text is kept only if the floor pass took something; every
 * piece that took something is joined to the previous one by two spaces.  A
 * container with nothing the player has seen in it says nothing, and a closed
 * one is not visited.  No 3.7/3.8/4.0 Runner has the loop ("get all" is a
 * run390-only literal).
 *
 * Measured on p39PUT (run390x runner_probes/put.run390.txt T23;
 * make_3738_putprobe.py, runner_probes/put.run390.takeall*.txt): "You take
 * the coin, ...
 * and the table from the cupboard." with nothing on the floor; "You pick up
 * the table.  You take the gem from the cupboard." with the open chest empty
 * after it; "You take the gem from the cupboard.  You take the coin from the
 * chest."; an unseen gem in the open chest is left behind, and a closed chest
 * is skipped.  The capacity arms (insides() returning 2, and the "can't take
 * any more/anything, as ... hands are full." rewrite at 455A68) are not
 * modelled.
 */
static scr_bool
lib_take_all_sweep_390 (scr_gameref_t game, scr_bool has_printed)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int associate, objects;

  GS_FOR_EACH_OBJECT (game, associate)
    {
      if (!obj_directly_in_room (game, associate, gs_playerroom (game)))
        continue;
      if (!((obj_is_container (game, associate)
             && gs_object_openness (game, associate) <= OBJ_OPEN)
            || obj_is_surface (game, associate)))
        continue;

      gs_set_multiple_references (game);
      objects = lib_apply_filter (game, lib_take_from_filter,
                                  associate, FALSE, nullptr);
      gs_clear_multiple_references (game);
      if (objects == 0)
        continue;

      if (has_printed)
        pf_buffer_string (filter, "  ");
      lib_take_from_single_named = FALSE;
      lib_take_from_object_backend (game, associate);
      has_printed = TRUE;
    }
  return has_printed;
}


/*
 * lib_cmd_take_all()
 *
 * Attempt to take all objects currently visible to the player.
 */
scr_bool
lib_cmd_take_all (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects;
  scr_bool has_printed;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_take_all_filter, -1, FALSE, nullptr);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_take_backend (game);
  has_printed = objects > 0;
  if (prop_get_taf_version (gs_get_bundle (game)) == TAF_VERSION_390)
    has_printed = lib_take_all_sweep_390 (game, has_printed);
  if (!has_printed)
    pf_buffer_string (filter,
                      lib_is_version_400 (game)
                      ? "There is nothing worth taking here."
                      : "There is nothing to pick up here.");
  lib_take_refusal_redispatch = FALSE;

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_take_absent_score()
 *
 * The Runner's co() score for one object against the typed line: 1 if the
 * line contains the whole Short, 1 more for the first Alias it contains,
 * and 1 more for each word of the Prefix that appears in the line (run400
 * General.Sub_22_66).  Zero means the line names nothing of this object -- a
 * Prefix word alone never does, since the prefix loop is only reached once a
 * name has matched.  *term is left at the name the object answered to, Short
 * before Alias, which is what mdlSpreadTheLoad.Sub_20_43 hands the ambiguity
 * message.
 */
scr_int
lib_take_absent_score (scr_gameref_t game, scr_int object,
                       const scr_char *input, const scr_char **term)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *shortname, *prefix;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias, score;

  score = 0;
  *term = nullptr;

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!scr_strempty (shortname) && lib_co_contains (input, shortname))
    {
      score++;
      *term = shortname;
    }

  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count; alias++)
    {
      const scr_char *name;

      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (scr_strempty (name) || !lib_co_contains (input, name))
        continue;

      score++;
      if (!*term)
        *term = name;
      break;
    }

  if (score == 0)
    return 0;

  /* Each Prefix word the line also carries is worth another point. */
  prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");
  if (!scr_strempty (prefix))
    {
      std::string word;
      const scr_char *cursor;

      for (cursor = prefix; ; cursor++)
        {
          if (*cursor != NUL && !scr_isspace (*cursor))
            {
              word.append (1, *cursor);
              continue;
            }

          if (!word.empty () && lib_co_contains (input, word.c_str ()))
            score++;
          word.clear ();

          if (*cursor == NUL)
            break;
        }
    }

  return score;
}


/*
 * lib_cmd_take_absent()
 *
 * 4.0's named take once nothing here can answer to the noun.  The Runner's
 * take handler (run400 Proc_19_6) resolves the direct object with the co()
 * style whole-name resolver General.Sub_22_66, whose result is 255 for "the
 * line named nothing", a positive index+256 for one object, and -(index + 2)
 * for an ambiguity; the three answers are printed at the tail of the same
 * procedure -- "Take what?" (and again as the very last default), "It is not
 * clear which " & <term> & " you are referring to.", and "There is nothing
 * worth taking here.", which is where a line that resolved an object but took
 * nothing lands (the `> 255` test at 00073798 is the "take X from Y" test, so
 * a plain named take falls straight through to it).
 *
 * The resolver runs its scan TWICE.  Its first pass (mode 1) only considers
 * what Sub_22_61 calls present -- held, worn, in the room, or inside an open
 * container there -- and if that pass ends with no candidate at all it resets
 * the mode to 0 and jumps back to the top of the procedure (00063465), where
 * the test is the object's seen byte alone.  So the noun that names nothing
 * here is offered every object the player has ever SEEN, and that second pass
 * is what this function reproduces.
 *
 * What it ranges over is measured rather than argued:
 * harness/make_400_takeprobe.py builds p4TAKE (a coin loose in Alpha, a
 * statue static in Alpha, a widget and a gizmo loose in Bravo), with --tie
 * p4TAKE2 (a red widget and a blue widget, both Short "widget" with Prefix "a
 * red"/"a blue", plus a lamp aliased "light", all in Bravo), and with
 * --hidden p4TAKE3 (the same pair, with tasks that move either or both of
 * them to hidden).  Six feeds under Wine, run400, transcripts
 * runner_probes/take.run400.*.txt:
 *
 *     take widget   (Bravo never entered)        ->  Take what?
 *     take widget   (from Alpha, Bravo seen)     ->  There is nothing worth
 *                                                    taking here.
 *     take statue   (from Bravo, Alpha seen)     ->  There is nothing worth
 *                                                    taking here.
 *     take light    (from Alpha, an ALIAS)       ->  There is nothing worth
 *                                                    taking here.
 *     take widget   (two seen absent namesakes)  ->  It is not clear which
 *                                                    widget you are referring
 *                                                    to.
 *     take widget   (one, then both, hidden)     ->  It is not clear which
 *                                                    widget you are referring
 *                                                    to.
 *     take red      (a Prefix word only)         ->  Take what?
 *     take zzz                                   ->  Take what?
 *     take widget   (both present, in Bravo)     ->  Which widget.  The red
 *                                                    widget or the blue
 *                                                    widget?
 *
 * So the candidate set is the objects the player has SEEN -- the same seen
 * byte the examine path reads, and the same one that makes `x widget` answer
 * "You can't see the widget from here!" in the very next command of
 * runner_probes/take.run400.2.txt -- matched by WHOLE name, Short or Alias,
 * with a Prefix word naming nothing on its own.  Statics count (the statue)
 * and so do hidden objects (p4TAKE3's pair still ties with both of them
 * nowhere).  A candidate that is present hands the line back to the ordinary
 * path, which is where the present tie's co() prompt is raised.
 *
 * The scores then decide between the two refusals, and only a genuine tie for
 * the best score is ambiguous.  That is what zelda's `get key` turns on: its
 * super-hot key is Short "key" with an Alias "key" as well, so it scores 2
 * against the iron key's 1 and wins outright -- run400 answers with the flat
 * refusal (runner_transcripts/zelda.txt:573) even though both keys are seen
 * and both are called "key".
 */
scr_bool
lib_cmd_take_absent (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *best_term = nullptr;
  scr_int object, best_score, best_count, best_object;

  if (lib_take_declines_get_motion (game))
    return FALSE;

  /* Pre-3.9 takes() names the first match out of reach, seen or not; see
   * lib_absent_named_object_pre_390(). */
  object = lib_absent_named_object_pre_390 (game, TRUE);
  if (object != -1)
    return lib_cant_see_named_pre_390 (game, object, FALSE, " from here!\n");

  if (!lib_is_version_400 (game) || !input)
    return FALSE;

  /*
   * An "and" list resolves each piece on its own in mode 1, which scores
   * only what is here, so absent objects never tie: with nothing present
   * marked, the handler runs to its empty-buffer tail, "There is nothing
   * worth taking here." -- never "Take what?".
   * Measured on p4AND from Bravo, `get hat and coin` with both seen in Alpha
   * (run400 runner_probes/and.run400.b.txt).
   */
  if (lib_input_contains_word_400 (input, "and"))
    {
      GS_FOR_EACH_OBJECT (game, object)
        {
          const scr_char *term;

          if (lib_take_absent_score (game, object, input, &term) > 0
              && obj_indirectly_in_room (game, object, gs_playerroom (game)))
            return FALSE;
        }
      pf_buffer_string (filter, "There is nothing worth taking here.\n");
      return TRUE;
    }

  best_score = 0;
  best_count = 0;
  best_object = -1;

  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *term;
      scr_int score;

      score = lib_take_absent_score (game, object, input, &term);
      if (score == 0)
        continue;

      /* Something the noun names is here; the ordinary path handles it. */
      if (obj_indirectly_in_room (game, object, gs_playerroom (game)))
        return FALSE;

      if (!gs_object_seen (game, object))
        continue;

      if (score > best_score)
        {
          best_score = score;
          best_count = 1;
          best_term = term;
          best_object = object;
        }
      else if (score == best_score)
        {
          best_count++;
          best_term = term;
        }
    }

  /* Nothing the player has seen answers to it; "Take what?" says so. */
  if (best_count == 0)
    return FALSE;

  if (best_count > 1)
    {
      lib_print_not_clear_which_400 (game, best_term);
      pf_buffer_character (filter, '\n');
      return TRUE;
    }

  /*
   * A unique seen-absent winner sitting inside a closed container answers
   * for the container instead: the plain take is rewritten into an
   * implicit "take from" (run400 mdlSpreadTheLoad auto-from rewrite
   * 47302F) and lands in the SAME closed-container arm as an explicit one
   * ("<The X> is closed.").  Measured escape_to_new_york turn 152
   * `get parcel`, the parcel seen but absent, inside a closed parent
   * (Ticket run400 xoshiro trace).
   *
   * An OPEN parent gets past that arm, and the take-from then finds
   * nothing it can reach and ends at "Take what?".  British Fox
   * T147 `get silver key`: the key was listed by `open bag` (the lister
   * 46A950 stamps it seen), then an event put the bag back on the dungeon
   * table and the player in the corridor.  run400 answers "Take what?"
   * where the plain refusal was "There is nothing worth taking here."
   * (runner_transcripts britishfox).
   */
  if (gs_object_position (game, best_object) == OBJ_IN_OBJECT)
    {
      scr_int parent = gs_object_parent (game, best_object);

      if (obj_is_container (game, parent)
          && gs_object_openness (game, parent) > OBJ_OPEN)
        {
          pf_new_sentence (filter);
          lib_print_object_np (game, parent);
          pf_buffer_string (filter, " is closed.\n");
          return TRUE;
        }
      if (obj_is_container (game, parent))
        return FALSE;
    }

  pf_buffer_string (filter, "There is nothing worth taking here.\n");
  return TRUE;
}


/* Set by lib_cmd_get_what() only; see the scored fallback below. */
scr_bool lib_take_scored_fallback = FALSE;

/*
 * lib_take_tie_400()
 *
 * get_piece's two answers to a tie from 463640 (see
 * lib_take_whole_line_400()): the flat "It is not clear which ..." with no
 * pending object, else the Which question.  Returns FALSE, having printed
 * nothing, only for a list the Runner would run together.
 */
scr_bool
lib_take_tie_400 (scr_gameref_t game, const scr_char *line, scr_int pending,
                  scr_int last_tied, const std::vector<scr_int> &marked,
                  scr_int mark_count)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (pending < 0)
    {
      lib_print_not_clear_which_400
        (game, lib_drop_named_term_400 (game, last_tied, line, FALSE));
      pf_buffer_answer_break (filter);
      gs_clear_multiple_references (game);
      return TRUE;
    }

  /*
   * As in lib_drop_named_400(), more marks than the pass-0 count are
   * unmeasured.
   */
  if ((scr_int) marked.size () != mark_count)
    return FALSE;

  lib_co_400_raise_named (game,
                          prop_get_indexed_string (gs_get_bundle (game),
                                                   "Objects", last_tied,
                                                   "Short"),
                          marked);
  gs_clear_multiple_references (game);
  return TRUE;
}

/*
 * lib_take_whole_line_400()
 *
 * get_piece (Proc_19_23_473A34) names a take's object from the WHOLE
 * fragment before it parses anything: it hands the line to the noun scorer
 * 463640 in mode 1 (lib_name_object_resolve_400()), and what comes back
 * decides the piece whatever else the line said.  A unique winner is the only
 * object marked: p4WTIE (run400, runner_probes/wtie.run400.{12,16,17}.txt)
 * has `take rope with ruby` take the ruby and `take rope with knife` and
 * `take zzz with knife` answer "You are already carrying the knife." -- the
 * static rope is never a
 * candidate, the held knife only on the second pass.  A tie with a pending
 * object asks: "Which " & the LAST tied object's raw Short & ".  " & the
 * pass-0 marks & "?", so `take ruby with stone` is "Which ruby.  The red
 * stone, the blue stone or the ruby?" and `take knife with stone`, the knife
 * skipped on pass 0, is "Which stone.  The red stone or the blue stone?"
 * (runner_probes/wtie.run400.1[67].txt).  A tie with none (Me(424) < 0) is
 * the flat "It is not clear which " & <the last tied object's typed name,
 * 446C74> & " you are referring to.": p4AND `get coin, hat`, both held, "It
 * is not clear which hat you are referring to."
 * (runner_probes/and.run400.txt), the coin scoring because c() ends a word at
 * the comma.
 *
 * Returns TRUE when it has answered the line.  *BOUND is set when a unique
 * winner is now the one referenced object, and the caller skips its own
 * parse; a line the scorer names nothing on keeps the parser's flow.
 */
static scr_bool
lib_take_whole_line_400 (scr_gameref_t game, scr_int *references,
                         scr_bool *bound)
{
  const scr_char *line = run_get_dispatch_input ();
  std::vector<scr_int> marked;
  scr_int object, pending, last_tied, mark_count;

  *bound = FALSE;
  if (!lib_is_version_400 (game) || !line
      || lib_input_contains_word_400 (line, "all")
      || lib_input_contains_word_400 (line, "and"))
    return FALSE;

  /*
   * A "from" line names its container first, so a line
   * whose container resolves nothing is "I don't understand where you want
   * to get things from." however its piece ties: `take stone from zzz`
   * (p4WTIE, runner_probes/wtie.run400.from.txt).  lib_take_from_piece_400()
   * scores the piece.
   */
  if (lib_input_contains_word_400 (line, "from"))
    return FALSE;

  object = lib_name_object_resolve_400 (game, line, 1, &pending, &last_tied,
                                        &marked, &mark_count);
  if (object >= 0)
    {
      gs_clear_multiple_references (game);
      game->refs.multiple[object] = TRUE;
      *references = 1;
      *bound = TRUE;
      return FALSE;
    }
  if (object != -1)
    return FALSE;

  return lib_take_tie_400 (game, line, pending, last_tied, marked,
                           mark_count);
}

/*
 * lib_take_from_task_sweep_380()
 *
 * run380's insides() ends every take-from that found its source object with
 * a sweep over the whole object table (447405): for each object with a
 * parent it stores "get " & Short & " from " & parent's Short into the line
 * (ImpAdStStr @00047446, hidden by the decompiler) and, if checktask passes
 * on it, runs tasks(1) there.  The object just taken has its parent cleared
 * and so is skipped.  tra.taf, run380: task 11 `get *knives*` (knives in the
 * silverware drawer) runs after `get meat from refrigerator`, and after the
 * bare `get moxie` / `get pop-tarts` that takes() rewrites into a take-from
 * (43E47B) -- "You take old meat from the big white refrigerator.  You take
 * all of the knives from the silverware drawer."
 * (runner_probes/timmy_reid.run380.kn[135].txt;
 * runner_transcripts/timmy_reid.rtf turn 8).  `get garbage container`, off
 * the floor, runs nothing
 * (runner_probes/timmy_reid.run380.kn6.txt).  run370 has no sweep, and run390
 * none either.
 */
void
lib_take_from_task_sweep_380 (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = prop_get_taf_version (bundle);
  scr_int object;

  if (version < TAF_VERSION_380 || version >= TAF_VERSION_390)
    return;

  /* The task's text joins the take's own line after two spaces. */
  pf_buffer_join_pending (gs_get_filter (game));
  GS_FOR_EACH_OBJECT (game, object)
    {
      std::string line;

      if (gs_object_position (game, object) != OBJ_IN_OBJECT
          && gs_object_position (game, object) != OBJ_ON_OBJECT)
        continue;

      line = "get ";
      line += prop_get_indexed_string (bundle, "Objects", object, "Short");
      line += " from ";
      line += prop_get_indexed_string (bundle, "Objects",
                                       gs_object_parent (game, object),
                                       "Short");
      run_game_task_commands (game, line.c_str ());
    }
  pf_clear_join_pending (gs_get_filter (game));
}


/*
 * lib_take_and_400()
 *
 * 4.0's takes() cuts an "and" line into pieces (Proc_19_23_473A34, entered
 * once per piece) and resolves each with the noun scorer on its own,
 * then treats the pieces as one take:
 *
 *   - a piece that names an object in or on something makes the whole line
 *     a take-from of the FIRST such piece's parent, and only the pieces in
 *     or on that parent move: `take stone and nut` with the nut in an open
 *     box is "You take the nut from the box." and the stone stays put (p4TKB
 *     cells 3, 8, 13, 19, 25, 31, 38, 44; p4TKA 23, 168);
 *   - otherwise every piece is taken in index order, a held or worn one
 *     answering "You are already carrying X." after the takes and a static
 *     one "You can't take X!" (p4TKA cells 3, 14, 17, 20, 32, 36, 40, 44);
 *   - a piece that names an object elsewhere (the gem in the Cave) costs
 *     nothing; a piece that names nothing at all keeps the parser's own
 *     answers, as does a tie.
 *
 * run400x runner_probes/tk[ab].run400.txt.  0 when
 * this does not apply and the ordinary parse should run; 1 when the pieces are
 * in refs.multiple and *references counts them.
 */
static scr_int
lib_take_and_400 (scr_gameref_t game, scr_int *references)
{
  const scr_char *text = var_get_ref_text (gs_get_vars (game));
  std::vector<scr_int> pieces, refused;
  std::string copy;
  size_t start;
  scr_int object, first_parent = -1;

  if (!lib_is_version_400 (game) || !text
      || lib_input_contains_word (text, "all")
      || !lib_input_contains_word (text, "and"))
    return 0;

  copy = text;
  start = 0;
  while (start <= copy.size ())
    {
      size_t end = copy.find (" and ", start);
      std::string piece;
      std::vector<scr_int> tied;
      scr_int other;
      scr_bool known;

      piece = copy.substr (start, end == std::string::npos
                                  ? std::string::npos : end - start);
      start = end == std::string::npos ? copy.size () + 1 : end + 5;

      {
        size_t from = piece.find_first_not_of (' ');
        size_t to = piece.find_last_not_of (' ');

        if (from == std::string::npos)
          return 0;
        piece = piece.substr (from, to - from + 1);
      }

      object = lib_take_resolve_400_string (game, piece.c_str (), &tied);
      if (object == -1)
        return 0;
      if (object >= 0)
        {
          pieces.push_back (object);
          continue;
        }

      object = lib_verb_object_resolve_400_string (game, piece.c_str (),
                                                   nullptr, TRUE);
      if (object >= 0 && obj_is_static (game, object))
        {
          refused.push_back (object);
          continue;
        }

      known = FALSE;
      for (other = 0; other < gs_object_count (game) && !known; other++)
        known = lib_verb_object_name_score (game, other, piece.c_str ()) > 0;
      if (!known)
        return 0;
    }
  if (pieces.empty ())
    return 0;

  for (size_t index_ = 0; index_ < pieces.size (); index_++)
    {
      const scr_int position = gs_object_position (game, pieces[index_]);

      if (position == OBJ_IN_OBJECT || position == OBJ_ON_OBJECT)
        {
          first_parent = gs_object_parent (game, pieces[index_]);
          break;
        }
    }

  gs_clear_multiple_references (game);
  *references = 0;
  for (size_t index_ = 0; index_ < pieces.size (); index_++)
    {
      const scr_int piece = pieces[index_];
      const scr_int position = gs_object_position (game, piece);

      if (first_parent != -1
          && !((position == OBJ_IN_OBJECT || position == OBJ_ON_OBJECT)
               && gs_object_parent (game, piece) == first_parent))
        continue;
      if (!game->refs.multiple[piece])
        {
          game->refs.multiple[piece] = TRUE;
          (*references)++;
        }
    }
  if (first_parent == -1)
    {
      for (size_t index_ = 0; index_ < refused.size (); index_++)
        {
          if (!game->refs.multiple[refused[index_]])
            {
              game->refs.multiple[refused[index_]] = TRUE;
              (*references)++;
            }
        }
    }
  if (*references == 0)
    return 0;

  lib_take_and_pieces_400 = TRUE;
  return 1;
}


/*
 * lib_take_npc_overwrite_tail()
 *
 * The tail every take refusal shares: a present character the line names
 * answers over whatever takes() just wrote, and at 3.8 its take-from arm
 * answers again over that.  See lib_take_npc_overwrite() for the arms and
 * their gates.  Ask it wherever a take writes and returns, not only from
 * the main loop -- the held-namesake pre-empt writes its "You've already
 * got the hat!" and returns, and the Runner overwrites that line too: `take
 * give hat bob` with the hat in hand is "I don't think Bob would appreciate
 * being handled." at 3.7/3.9/4.0 and "Bob is not carrying the hat!" at 3.8
 * (cell 5 of runner_probes/ord.run*.give.*).
 */

/*
 * lib_take_ask_overwrite_400()
 *
 * characters()' ask arm, the last text arm of its cascade, answering over the
 * take arm at 4.0.  The block is entered on whole-word `ask` or `talk to`
 * and, with the character in the player's room, its gate is
 * `(no task And MemVar_4942E0 = 0) Or buffer = "<player> can't talk to
 * that."` (47F900) -- no buffer-empty test, unlike its 3.9 twin at 4597FE,
 * which is why 4.0 alone does this.  Inside, c("about") and the character
 * named (45E99C mode 1) and InStr(line, "about") > 0 take the subject as
 * Mid(line, InStr(line, "about") + 6), and the topic loop ASSIGNS the reply
 * it finds (47FA7C).  The no-topic "does not respond" writes only over an
 * empty, "can't talk to that." or " can't see " buffer, so an unanswered ask
 * leaves the take refusal standing; the "Use the format" hint for a line
 * with no `about` is not measured and not modelled.
 *
 * Measured on p4ORD (run400x runner_probes/ord.run400.chr.txt):
 * `take ask bob about hat` is "Bob says, 'That is a fine hat.'" and the hat
 * is still taken; 3.7/3.8/3.9 keep their take-arm answers.
 *
 * TRUE once it has spoken, having truncated the buffer back to mark first.
 */
static scr_bool
lib_take_ask_overwrite_400 (scr_gameref_t game, scr_int npc, size_t mark)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *about;
  scr_int topic;

  if (!input || !lib_is_version_400 (game)
      || !(lib_input_contains_word (input, "ask")
           || lib_input_contains_word (input, "talk to"))
      || !lib_input_contains_word (input, "about")
      || !npc_in_room (game, npc, gs_playerroom (game)))
    return FALSE;
  about = strstr (input, "about");
  if (!about)
    return FALSE;
  about += strlen (about) >= 6 ? 6 : strlen (about);

  /* The topic loop reads the subject, not whatever the take referenced. */
  const std::string saved (var_get_ref_text (vars));
  var_set_ref_text (vars, about);
  topic = lib_npc_find_topic (game, npc);
  var_set_ref_text (vars, saved.c_str ());
  if (topic == -1)
    return FALSE;

  pf_truncate (gs_get_filter (game), mark);
  return lib_npc_reply_to (game, npc, topic);
}

static scr_bool
lib_take_npc_overwrite_tail (scr_gameref_t game, size_t mark)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int npc = lib_take_npc_overwrite (game);

  if (npc == -1 || lib_take_refusal_redispatch)
    return FALSE;

  lib_take_single_named = FALSE;
  lib_take_refusal_claimed = FALSE;
  pf_truncate (filter, mark);
  lib_print_take_npc_refusal (game, npc);
  lib_take_from_npc_overwrite_380 (game, npc, mark);
  lib_take_ask_overwrite_400 (game, npc, mark);
  return TRUE;
}


/*
 * What a stage of lib_take_multiple_common() tells the driver: go on to the
 * next stage, or return TRUE (the line is answered) or FALSE (not ours: the
 * later rows get the line).
 */
enum lib_take_stage_t
{
  LIB_TAKE_GO_ON,
  LIB_TAKE_RETURN_TRUE,
  LIB_TAKE_RETURN_FALSE
};


/*
 * `get off X` reaches the take rows first, and below 4.0 takes() names X
 * over the whole line, so a player standing on the stool picked it up
 * (run370x/run380x/run390x p3xSIT).  Deliberate deviation: while the
 * player is on something, leave the line to lib_cmd_get_off_object(); and on
 * nothing too, unless lib_get_off_may_take().
 */
static scr_bool
lib_take_multi_get_off_line (void)
{
  const scr_char *line = run_get_dispatch_input ();

  return line && (scr_strncasecmp (line, "get off ", 8) == 0
                  || scr_strncasecmp (line, "get down off ", 13) == 0
                  || scr_strncasecmp (line, "get up off ", 11) == 0);
}


/*
 * run390's takes() gate (4544C6) is `c(get|take|pick) And Not c("from") And
 * Not c("get on") And Not c("get down")`, so at 3.9 `get on stool` with the
 * stool a portable standable is sitstand's "You stand on the stool.", and
 * `get on coin` falls through to "I don't understand what you want me to do
 * with the coin." (p39SIT, runner_probes/sit.run390.geton.txt).  Neither
 * literal exists in run370/run380, which pick the stool up
 * (sit.run370.geton.rtf, sit.run380.geton.rtf), and 4.0's get_outer (4582D8)
 * has no such exclusion either: "You take the stool." (sit.run400.geton.txt).
 * At 3.9 the take rows and their catch-alls (lib_cmd_take_absent(),
 * lib_cmd_get_what()) step aside on those two literals, since takes() never
 * ran at all, and the `get on %object%` row or the verb catch-all answers.
 *
 * Deliberate deviation (2026-11-07): every version steps aside on a
 * get-motion line -- `get in/into/inside/on/onto X`, `get out of X` -- which
 * is sitstand's, not a take.  Every Runner takes the named object
 * instead: `get in stool` "You pick up the stool." / "You take the stool.",
 * `get in chair` on a static "You can't take a chair." / "You can't take the
 * chair!", `get out of stool` held "You've already got a stool!" (p37SIT..
 * p4SIT, runner_probes/sit.run*.getin.*; The X-Files T76 `get in the van`
 * "You take VW Van.", xfiles.run400.t76.txt).  A take is never what the
 * player meant by any of them, so the lines go to lib_cmd_get_in_object(),
 * lib_cmd_get_on_object() and lib_cmd_get_out_of_object(), and a line they
 * decline falls to the verb catch-all.  A game's own task still wins,
 * tasks being matched first; 4.0's get_outer leaves these lines alone too
 * (run_line_t::get_outer_400()).
 */
scr_bool
lib_get_motion_line (const scr_char *line)
{
  static const scr_char *const MOTION[] = {
    "in ", "into ", "inside ", "on ", "onto ", "out of ", nullptr
  };
  const scr_char *rest;
  scr_int index_;

  if (!line || scr_strncasecmp (line, "get ", 4) != 0)
    return FALSE;
  rest = line + 4;
  rest += strspn (rest, " ");
  for (index_ = 0; MOTION[index_]; index_++)
    {
      const size_t length = strlen (MOTION[index_]);

      if (scr_strncasecmp (rest, MOTION[index_], length) == 0
          && rest[length + strspn (rest + length, " ")] != NUL)
        return TRUE;
    }
  return FALSE;
}

scr_bool
lib_take_declines_get_motion (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();

  if (!line)
    return FALSE;
  if (lib_get_motion_line (line))
    {
      SCR_DEVIATION ("take_get_motion", nullptr);
      return TRUE;
    }
  if (version < TAF_VERSION_390 || version >= TAF_VERSION_400)
    return FALSE;
  return run_c_word_pre400 (version, line, "get on") >= 0
         || run_c_word_pre400 (version, line, "get down") >= 0;
}


/*
 * `take off <...>` and `take <...> off` reach the generic take row before the
 * remove rows, so `take off all` picked up the whole room, `take off A and B`
 * said "I am already wearing my sleepshirt! I pick up my sweatshirt.", and
 * `take sweatshirt off` refused with "already wearing" (AkarisStory.taf, 3.9).
 * Deliberate deviation (no Runner probe covers a take-off list): leave the
 * line to the remove rows when it is a list or `all`, or when it names
 * something the player is wearing.  The single-noun cells are measured with
 * the hat worn (p37ORD/p38ORD, runner_probes/ord.run370.takeoff.rtf and
 * ord.run380.takeoff.rtf, 2026-11-07): run380 answers both `take hat off`
 * and `take off hat` "You are already wearing a hat!"; run370 says "You
 * can't see a hat from here!" to `take hat off` and removes the hat for
 * `take off hat`.  Scarier removes it in 3.7 for both and in 3.8 for `take
 * off hat`; 3.8 `take hat off` keeps the measured "already wearing".  With
 * the hat held but not worn, `take off hat` still says "You've already got
 * the hat!" (lib_take_held_namesake_preempt_pre400()).
 */
static scr_bool
lib_take_multi_take_off_line (scr_gameref_t game)
{
  const scr_char *line = run_get_dispatch_input ();
  const scr_char *rest;
  scr_int object;
  size_t length;

  if (!line || scr_strncasecmp (line, "take ", 5) != 0)
    return FALSE;
  rest = line + 5 + strspn (line + 5, " ");
  length = strlen (line);
  while (length > 0 && line[length - 1] == ' ')
    length--;
  if (!(scr_strncasecmp (rest, "off ", 4) == 0
        || (length >= 4 && scr_strncasecmp (line + length - 4, " off", 4) == 0
            && line + length - 4 > rest)))
    return FALSE;

  if (strchr (rest, ',') || lib_input_contains_word (rest, "and")
      || lib_input_contains_word (rest, "all")
      || lib_input_contains_word (rest, "everything"))
    return TRUE;

  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *term;

      if (gs_object_position (game, object) == OBJ_WORN_PLAYER
          && lib_take_absent_score (game, object, rest, &term) > 0)
        return TRUE;
    }
  return FALSE;
}


/*
 * A take line holding "from" that the take-from rows above declined --
 * the container clause named nothing, or two things, or nothing at all --
 * is still a take-from to every Runner (see lib_take_from_and() and
 * lib_take_from_trailing()); pre-4.0 it is never a bare take, so the
 * catch-alls of the take-from-nowhere rows answer it, not the held namesake
 * or "Take what?" (`get coin from zzz` with the coin in hand:
 * run370x/run380x "You can't do that!", run390x "The coin isn't in or on
 * anything!"; p3xTKB runner_probes/tkb.run3[789]0.* cell 144).
 */
static lib_take_stage_t
lib_take_multi_from_line (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();
  const scr_int at = line ? run_c_word_pre400 (version, line, "from") : -1;

  if (at >= 0)
    {
      if (line[at + 4 + strspn (line + at + 4, " ")] == NUL)
        return lib_take_from_trailing (game) ? LIB_TAKE_RETURN_TRUE
                                             : LIB_TAKE_RETURN_FALSE;
      /* 4.0 `take from box` is no take of the box; see lib_take_from_empty_head_400(). */
      if (version >= TAF_VERSION_400 && lib_take_from_empty_head_400 (game))
        return LIB_TAKE_RETURN_TRUE;
      if (lib_take_from_and_line (game))
        return lib_take_from_and (game) ? LIB_TAKE_RETURN_TRUE
                                        : LIB_TAKE_RETURN_FALSE;
      if (version < TAF_VERSION_400)
        return LIB_TAKE_RETURN_FALSE;
    }
  return LIB_TAKE_GO_ON;
}


/*
 * Pre-4.0: a held or worn namesake answers a noun that resolved to nothing
 * (unresolved), or -- indexed below the one object the line named -- for
 * that object when it is not loose in the room; see
 * lib_take_held_namesake_preempt_pre400().  A present character the line
 * names answers over it; see lib_take_npc_overwrite_tail().  TRUE once the
 * line is answered.
 */
static scr_bool
lib_take_multi_namesake_pre400 (scr_gameref_t game, scr_bool unresolved)
{
  const size_t mark = pf_buffer_length (gs_get_filter (game));

  if (lib_take_held_namesake_preempt_pre400 (game, unresolved))
    {
      lib_take_npc_overwrite_tail (game, mark);
      return TRUE;
    }
  return FALSE;
}


/*
 * The object parser found nothing in the line: the readings that can still
 * name an object for the take.
 */
static lib_take_stage_t
lib_take_multi_unparsed (scr_gameref_t game, scr_bool is_except,
                         scr_bool (*resolver) (scr_gameref_t, scr_int,
                                               scr_int),
                         scr_int *references)
{
  /*
   * 4.0's get_piece (Proc_19_23_473A34) names the piece's object with the
   * noun scorer 463640, which counts whole words and skips the
   * rest, so a word it does not know costs nothing.  Professor.taf,
   * run400: `get x rope` in the square refuses "You can't take the
   * Mailbox on-a Rope!" (runner_probes/professor.run400.mail2.txt T22),
   * the alias "rope" scoring; in the Laboratory it ends on DontUnderstand
   * after the refusal's pre-match (T25,
   * runner_probes/professor.run400.mail6.txt T26).  Scarier's object
   * parser wanted every word, and the line fell to "Take what?".  The
   * X-Files, run400 runner_transcripts/xfiles.txt:248: `take phone book`
   * -> "You take Your Cell Phone from Your Backpack.".  One piece only; a
   * list keeps the parser's own answers.  Only from the "Take what?"
   * catch-all, so every other take row has had the line first (Pilfers
   * `get off bed`).
   */
  const scr_char *text = var_get_ref_text (gs_get_vars (game));
  scr_int object;

  /*
   * Pre-4.0: a held or worn namesake still answers; see
   * lib_take_multi_namesake_pre400().
   */
  if (!is_except && lib_take_multi_namesake_pre400 (game, TRUE))
    return LIB_TAKE_RETURN_TRUE;

  /*
   * Pre-4.0: takes() names its object over the WHOLE line, co(obj, 1);
   * see lib_move_named_whole_line_pre400().
   */
  if (!is_except
      && lib_move_named_whole_line_pre400 (game, resolver, 1, references))
    ;
  else if (!lib_take_scored_fallback || is_except || !text
           || strchr (text, ',') || lib_input_contains_word (text, "and"))
    return LIB_TAKE_RETURN_FALSE;
  else
    {
      object = lib_verb_object_resolve_400_string (game, text,
                                                   nullptr, TRUE);
      if (object < 0)
        return LIB_TAKE_RETURN_FALSE;
      gs_clear_multiple_references (game);
      game->refs.multiple[object] = TRUE;
      *references = 1;
    }
  return LIB_TAKE_GO_ON;
}


/*
 * Pre-4.0, the pre-pass counts the loose candidates first and flags them;
 * see lib_take_crowd_pre400().  Two of them and the take never happens, so
 * decline the row and let the catch-all answer "Take what?"; one, and it
 * is that object the loop takes, whatever the parser bound -- `take hat
 * coin` with the hat in hand picks up the coin.
 *
 * TRUE when the row is to be declined.
 */
static scr_bool
lib_take_multi_crowded_pre400 (scr_gameref_t game)
{
  scr_bool crowded;
  const scr_int winner = lib_take_crowd_pre400 (game, &crowded);

  if (crowded)
    {
      gs_clear_multiple_references (game);
      return TRUE;
    }
  if (winner != -1 && !game->refs.multiple[winner])
    {
      gs_clear_multiple_references (game);
      game->refs.multiple[winner] = TRUE;
    }
  return FALSE;
}


/*
 * Pre-4.0, takes() reaches an object only through co(idx, 1) (run390
 * 454E1D), so the one object the parser
 * resolved is still not taken when that test fails for it: two seen
 * namesakes present with no loose one and no Prefix word typed is the
 * pending "Which <term>" ambiguity, and NOTHING moves.  secret_of_lost_world
 * T59 `take scroll` with the ancient scroll in hand and the decayed scroll
 * on the fresh skeleton prints "Which scroll.  Ancient scroll or the
 * decayed scroll?" and leaves the decayed scroll where it was, so T74
 * `give princess decayed scroll` fails its held restriction ("You can't do
 * that right now.") and the lily, the red gem and the win never come
 * (run390, runner_transcripts/secret_of_lost_world).  Scarier
 * printed the same prompt over a take that had quietly happened.  Decline
 * the row: the ambiguity prompt replaces the turn's output anyway, and a
 * plain miss falls to the catch-all "Take what?" the Runner's empty buffer
 * reaches.
 *
 * 3.7 is not in this: its takes() calls no co() at all, and settles a crowd
 * of its own namesakes in lib_disambiguate_object_common().  A rock ALIASED
 * "gem" beside a gem is no crowd to it -- `take gem` takes both and the rock
 * speaks (p37OPENA, runner_probes/opena.run370.rtf) -- and this gate must not
 * turn that into "Take what?".
 *
 * TRUE when the row is to be declined.
 */
static scr_bool
lib_take_multi_co_fails_pre400 (scr_gameref_t game)
{
  const scr_char *line = run_get_dispatch_input ();
  scr_int target;

  GS_FOR_EACH_OBJECT (game, target)
    if (game->refs.multiple[target])
      break;
  if (line && target < gs_object_count (game)
      && !lib_take_co_pre400 (game, line, target))
    {
      gs_clear_multiple_references (game);
      return TRUE;
    }
  return FALSE;
}


/*
 * The take itself, over the references the stages above settled: filter,
 * hand to the backend, and close the answer.
 */
static scr_bool
lib_take_multi_run (scr_gameref_t game, scr_bool is_except,
                    scr_bool (*resolver) (scr_gameref_t, scr_int, scr_int),
                    scr_int references)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects;

  /* Note single-object takes; the backend words some refusals differently. */
  lib_take_single_named = !is_except && references == 1;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              resolver, -1, is_except,
                              &references);

  /*
   * A 3.8 bare take of something in or on an object is a take-from, and
   * gets its task sweep; see lib_take_from_task_sweep_380().
   */
  scr_bool is_take_from_380 = FALSE;
  if (!is_except)
    {
      scr_int index_;

      GS_FOR_EACH_OBJECT (game, index_)
        if (game->refs.objects[index_]
            && (gs_object_position (game, index_) == OBJ_IN_OBJECT
                || gs_object_position (game, index_) == OBJ_ON_OBJECT))
          is_take_from_380 = TRUE;
    }

  const size_t take_mark = pf_buffer_length (filter);

  if (objects > 0 || references > 0)
    {
      lib_take_backend (game);

      /* A present character the line names overwrites the answer. */
      if (lib_take_npc_overwrite_tail (game, take_mark))
        return TRUE;
    }
  else if (lib_is_version_400 (game))
    {
      /*
       * 4.0 has one flat refusal and no "else" form: run400 answers
       * "There is nothing worth taking here." to `take all except rock`
       * whether or not the rock is the only thing left in the room.
       */
      pf_buffer_string (filter, "There is nothing worth taking here.");
    }
  else
    {
      pf_buffer_string (filter, "There is nothing");
      if (is_except && objects == 0)
        pf_buffer_string (filter, " else");
      pf_buffer_string (filter, " to pick up here.");
    }
  lib_take_single_named = FALSE;
  lib_take_and_pieces_400 = FALSE;

  if (is_take_from_380 && !lib_take_refusal_claimed)
    lib_take_from_task_sweep_380 (game);
  if (!lib_take_refusal_claimed)
    pf_buffer_answer_break (filter);
  lib_take_refusal_claimed = FALSE;
  if (lib_take_refusal_redispatch)
    {
      lib_take_refusal_redispatch = FALSE;
      lib_take_refusal_redispatch_400 (game);
    }
  return TRUE;
}


/*
 * lib_take_multiple_common()
 *
 * Take the objects available to the player and listed in %text%, or -- for
 * is_except -- every one of them but those listed.
 */
scr_bool
lib_take_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  scr_bool (*resolver) (scr_gameref_t, scr_int, scr_int);
  scr_int references;
  scr_bool parsed = FALSE;
  lib_take_stage_t stage;

  /* `get off X` is a get-off, not a take; see lib_get_off_may_take(). */
  if (!is_except && lib_take_multi_get_off_line ()
      && (lib_player_parent_here (game) != -1
          || !lib_get_off_may_take (game)))
    return FALSE;

  /* `get on/in/out of X` is sitstand's; see lib_take_declines_get_motion(). */
  if (!is_except && lib_take_declines_get_motion (game))
    return FALSE;

  /* `take off all` and friends undress; see lib_take_multi_take_off_line(). */
  if (!is_except && lib_take_multi_take_off_line (game))
    return FALSE;

  if (!is_except)
    {
      stage = lib_take_multi_from_line (game);
      if (stage != LIB_TAKE_GO_ON)
        return stage == LIB_TAKE_RETURN_TRUE;
    }

  if (!is_except && lib_take_and_none_pre400 (game))
    return TRUE;
  if (!is_except && lib_take_and_pre400 (game))
    return TRUE;

  /*
   * "take all except ..." works over the "all" universe, which excludes
   * whatever the player already carries; a named take reaches further, into
   * a carried open container (see lib_take_all_filter).
   */
  resolver = is_except ? lib_take_all_filter : lib_take_filter;

  /* 4.0: an "and" line resolves piece by piece; see lib_take_and_400(). */
  if (!is_except)
    parsed = lib_take_and_400 (game, &references) == 1;

  /* 4.0: the whole fragment scored first; see lib_take_whole_line_400(). */
  if (!is_except && !parsed)
    {
      scr_bool bound;

      if (lib_take_whole_line_400 (game, &references, &bound))
        return TRUE;
      parsed = bound;
    }

  /* Parse the multiple objects list to find the target objects. */
  if (parsed)
    ;
  else if (!lib_parse_multiple_objects (game, is_except ? "leave" : "take",
                                        resolver, -1,
                                        &references))
    {
      stage = lib_take_multi_unparsed (game, is_except, resolver,
                                       &references);
      if (stage != LIB_TAKE_GO_ON)
        return stage == LIB_TAKE_RETURN_TRUE;
    }
  else if (references == 0)
    return TRUE;

  /*
   * Pre-4.0, one object named: a held namesake may answer for it, a crowd
   * of loose candidates declines the row.
   */
  if (!is_except && references == 1)
    {
      if (lib_take_multi_namesake_pre400 (game, FALSE))
        return TRUE;
      if (lib_take_multi_crowded_pre400 (game))
        return FALSE;
    }

  /* 3.8 and 3.9: a failed co() on that one object declines the row too. */
  if (!is_except && references == 1
      && prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380
      && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400
      && lib_take_multi_co_fails_pre400 (game))
    return FALSE;

  /*
   * run390 takes() rewrites a bare take of something in or on an object to
   * "<line> from <parent's Prefix Short>" (4552EE) and hands it to
   * insides(), whose container slot walk may pick another object the rewritten
   * line names.  Deliberate deviation: Scarier takes the object the line named
   * from where it is.
   */

  return lib_take_multi_run (game, is_except, resolver, references);
}


/*
 * lib_cmd_take_except_multiple()
 * lib_cmd_take_multiple()
 *
 * Facets of lib_take_multiple_common().
 */
scr_bool
lib_cmd_take_except_multiple (scr_gameref_t game)
{
  return lib_take_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_take_multiple (scr_gameref_t game)
{
  return lib_take_multiple_common (game, FALSE);
}
