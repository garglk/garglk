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
 * The backend they run is in sclibrar_take.cpp.
 *
 * See sclibrar.h for what the library files share, sclibrar_internal.h for
 * what the core files share and sclibrar_take_internal.h for what the two
 * take files share.
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
   * <parent>" (loc_43E47B; see lib_take_container_unheld).  run370 has the
   * same rewrite (4368C4), but its insides() only takes a get or remove
   * line (see the run370 insides() note above), so for take or pick up a bare take
   * naming something that sits in or on another object names nothing at all
   * and falls to the catch-all.  Measured on the same turn of two probes,
   * `take coin` with the coin inside an open box standing on the cave floor:
   * run370 answers "Take what?" (p37DARK,
   * runner_probes/dark.run370.putin2.txt:48 and again
   * runner_probes/dark.run370.putin3.txt:41) where run380 answers "You are
   * not holding a box." (p38DARK, runner_probes/dark.run380.putin2.txt:48 /
   * runner_probes/dark.run380.putin3.txt:41), i.e. run380 rewrote the line
   * and then refused it on the hold gate.  2026-09-12.  The explicit form is
   * untouched -- run370 plays `get coin from box` perfectly well
   * (runner_probes/dark.run370.putin.txt:27) and applies the same hold gate
   * to it (runner_probes/dark.run370.putin2.txt:18).
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
 * against run400.exe, 2026-08-02).
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
 * the room, after the floor.  run390 takes() (4558CE-455B28) runs, when the
 * line has "all", over every object that lies on the room's floor or is a
 * static present in the room, and is an open container or a surface; for each
 * it sets the line to "get all" and calls insides() with it, which is the
 * take-from handler (its gate is `c("from") Or (c("all") And container > 0)`).
 * The floor's own text is kept only if the floor pass took something (var_86,
 * set at 454FC5); every piece that took something is joined to the previous
 * one by two spaces.  A container with nothing the player has seen in it
 * says nothing, and a closed one is not visited.  No 3.7/3.8/4.0 Runner has
 * the loop ("get all" is a run390-only literal).
 *
 * Measured on p39PUT (run390x runner_probes/put.run390.txt T23;
 * make_3738_putprobe.py, runner_probes/put.run390.takeall.txt /
 * runner_probes/put.run390.takeall2.txt, 2026-09-19): "You take the coin, ...
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
 * General.Sub_22_66 at 000632BE .. 00063387).  Zero means the line names
 * nothing of this object -- a Prefix word alone never does, since the
 * prefix loop is only reached once a name has matched.  *term is left at
 * the name the object answered to, Short before Alias, which is what
 * mdlSpreadTheLoad.Sub_20_43 at 00046BFC hands the ambiguity message.
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
 * style whole-name resolver General.Sub_22_66 at 00073011, whose result is
 * 255 for "the line named nothing", a positive index+256 for one object, and
 * -(index + 2) for an ambiguity; the three answers are printed at the tail
 * of the same procedure -- "Take what?" at 0007332B and again as the very
 * last default at 00073A25, "It is not clear which " & <term> & " you are
 * referring to." at 0007335C, and "There is nothing worth taking here." at
 * 00073A13, which is where a line that resolved an object but took nothing
 * lands (the `If var_92 > 255` at 00073798 is the "take X from Y" test, so a
 * plain named take falls straight through to it).
 *
 * The resolver runs its scan TWICE.  Its first pass (mode 1, at 00063161)
 * only considers what Sub_22_61 calls present -- held, worn, in the room, or
 * inside an open container there -- and if that pass ends with no candidate
 * at all it resets the mode to 0 and jumps back to the top of the procedure
 * (00063465: `If var_9C = 1 And var_A0 = 0 And param_10 > 0`, then Branch
 * 000630BC), where the test at 0006310D is the object's seen byte alone.  So
 * the noun that names nothing here is offered every object the player has
 * ever SEEN, and that second pass is what this function reproduces.
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

  /* Pre-3.9 takes() names the first match out of reach, seen or not; see
   * lib_absent_named_object_pre_390(). */
  object = lib_absent_named_object_pre_390 (game, TRUE);
  if (object != -1)
    return lib_cant_see_named_pre_390 (game, object, FALSE, " from here!\n");

  if (!lib_is_version_400 (game) || !input)
    return FALSE;

  /*
   * An "and" list (473121-473221) resolves each piece on its own in mode
   * 1, which scores only what is here, so absent objects never tie: with
   * nothing present marked, the handler runs to its empty-buffer tail,
   * "There is nothing worth taking here." (473A13) -- never "Take what?".
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
   * ("<The X> is closed." 47395C).  Measured escape_to_new_york turn 152
   * `get parcel`, the parcel seen but absent, inside a closed parent
   * (Ticket run400 xoshiro trace 2026-09-12).
   *
   * An OPEN parent gets past that arm, and the take-from then finds
   * nothing it can reach and ends at "Take what?" (47332B).  British Fox
   * T147 `get silver key`: the key was listed by `open bag` (the lister
   * 46A950 stamps it seen), then an event put the bag back on the dungeon
   * table and the player in the corridor.  run400 answers "Take what?"
   * where the plain refusal was "There is nothing worth taking here."
   * (runner_transcripts britishfox, 2026-09-24).
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

  /* Me(428) is joined by a countdown from the pass-0 count; with more marks
   * than that the Runner runs names together, unmeasured. */
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
 * fragment before it parses anything: 473011 hands the line to the noun
 * scorer 463640 in mode 1 (lib_name_object_resolve_400()), and what comes
 * back decides the piece whatever else the line said.  A unique winner is the
 * only object marked (473022): p4WTIE (run400,
 * runner_probes/wtie.run400.12.txt, runner_probes/wtie.run400.16.txt and
 * runner_probes/wtie.run400.17.txt, 2026-09-20) has `take rope with ruby`
 * take the ruby and `take rope with knife` and `take zzz with knife` answer
 * "You are already carrying the knife." -- the static rope is never a
 * candidate, the held knife only on the second pass.  A tie with a pending
 * object asks (4733BD): "Which " & the LAST tied object's raw Short & ".  " &
 * the pass-0 marks & "?", so `take ruby with stone` is "Which ruby.  The red
 * stone, the blue stone or the ruby?" and `take knife with stone`, the knife
 * skipped on pass 0, is "Which stone.  The red stone or the blue stone?"
 * (runner_probes/wtie.run400.16.txt and runner_probes/wtie.run400.17.txt).  A
 * tie with none (Me(424) < 0, 47333C) is the flat "It is not clear which " &
 * <the last tied object's typed name, 446C74> & " you are referring to."
 * (47335F-4733A5): p4AND `get coin, hat`, both held, "It is not clear which
 * hat you are referring to." (runner_probes/and.run400.txt), the coin scoring
 * because c() ends a word at the comma.
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

  /* A "from" line names its container first (472E03-472F35), so a line
     whose container resolves nothing is "I don't understand where you want
     to get things from." however its piece ties: `take stone from zzz`
     (p4WTIE, runner_probes/wtie.run400.from.txt).  lib_take_from_piece_400()
     scores the piece. */
  if (lib_input_contains_word_400 (line, "from"))
    return FALSE;

  object = lib_name_object_resolve_400 (game, line, 1, &pending, &last_tied,
                                        &marked, &mark_count);
  if (object >= 0)
    {
      gs_clear_multiple_references (game);
      game->multiple_references[object] = TRUE;
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
 * a sweep over the whole object table (loc_447405): for each object with a
 * parent it stores "get " & Short & " from " & parent's Short into the line
 * (ImpAdStStr @00047446, hidden by the decompiler) and, if checktask passes
 * on it, runs tasks(1) there.  The object just taken has its parent cleared
 * (4470EF) and so is skipped.  tra.taf, run380, 2026-09-14: task 11 `get
 * *knives*` (knives in the silverware drawer) runs after `get meat from
 * refrigerator`, and after the bare `get moxie` / `get pop-tarts` that
 * takes() rewrites into a take-from (43E47B) -- "You take old meat from the
 * big white refrigerator.  You take all of the knives from the silverware
 * drawer." (runner_probes/timmy_reid.run380.kn1.txt,
 * runner_probes/timmy_reid.run380.kn3.txt,
 * runner_probes/timmy_reid.run380.kn5.txt; runner_transcripts/timmy_reid.rtf
 * turn 8).  `get garbage container`, off the floor, runs nothing
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
 * lib_take_held_namesake_preempt_pre400()
 *
 * The pre-4.0 takes() walks EVERY object in index order and lets each one
 * whose Short or Alias sits in the typed line write the answer, whether or
 * not it is the object the line meant.  A held one writes "<You>'ve already
 * got <object>!" (run390 @454EC5, run380 @43E03E, run370 @436561) and a
 * worn one "<You> are already wearing <object>!" (run390 @454F31, run380
 * @43E0A2), both raising the outcome rank to 4.  The branch that takes an
 * object out of a container, off a surface or from a character -- the one
 * that rewrites the line into "take X from <parent>" -- is gated on the
 * buffer being still empty (or " pick up "/" from here!") AND rank < 3
 * (run390 @4551E4, run380 @43E3F6/@43E41F), so a held namesake indexed
 * BELOW the target has already spoken and the target is never taken.  An
 * object lying loose in the room is different: "<You> pick up X." overwrites
 * the buffer (run390 @455014), so only the not-in-the-room targets lose.
 *
 * Measured in run390 (runner_transcripts/stardust T38, 2026-09-19): `take
 * needle box` with the sharp needle (object 3) in hand and the needle box
 * (object 12) on the desk answers "You've already got the sharp needle!"
 * and leaves the box where it was -- T99 `put needle in box` is then "You
 * don't have the box.".  secret_of_lost_world T53 `take blue gem` with the
 * green gem (11) held and the blue gem (12) on the fresh skeleton: "You
 * already have the green gem!" (the game's ALR of "You've already got"),
 * the blue gem still listed on the skeleton at T55.  Scarier resolved the
 * line to the box / the blue gem alone and took it.
 *
 * Only the single-named plain take: the "and" list and "all" walk other modes
 * of the loop.  Below 3.9 the object prints its raw prefix, as the
 * already-got list below does (p38EXAM, runner_probes/exam.run380.txt).  3.7
 * has no from-parent branch at all -- a bare take of something in or on an
 * object writes nothing (lib_take_filter) -- so there the namesake speaks
 * from either side of the target; the last one in index order, as every write
 * overwrites.  A 3.8/3.9 namesake ABOVE the target is left alone: the rewrite
 * has run by the time it speaks and what the caller does with the rewritten
 * line is unmeasured.
 */
/*
 * The test the takes() loop puts each object through before letting it
 * write: run390 @454E1D co(obj, 1), run380 @43DFC3 co(obj), run370 @4364E6
 * a bare c(Short).  co() (run390 @43B6BC, run380 @42DE60) is false unless
 * the object's Short or Alias sits in the line; the term found is then
 * counted over every present object answering to it (obhere, and seen from
 * 3.9 -- lib_co_candidate).  With more than one, 3.9's mode 1 recounts
 * only the ones lying loose in the player's room (@43B37B, field 22 =
 * room) and uses that count when it is not zero; if the term is still
 * ambiguous the object passes only when the last word of its own Prefix is
 * in the line (@43B572 / @42DD47), otherwise 3.9 asks that the object
 * itself be present and seen (@43B661) where 3.8 passes it outright
 * (@42DDEA).  So `take blue plate` with the red plate in hand and the blue
 * one seen in the dishwasher does NOT let the red plate speak: two "plate"s
 * present, none loose, and "red" is not in the line -- deardiary T52
 * (runner_transcripts/deardiary, run390): "I take a blue plate from the
 * dishwasher.".  Nor does life_of_mike T28 `take truck keys` (run380) with
 * the mustang keys, aliased "keys" too, in hand.
 */
/* run390 isheld() @42A34C: field 22 is 0 (held), &H9C (worn), or &HF6/&HEC
 * (in/on) with a parent that isheld; nothing else counts. */
scr_bool
lib_isheld_390 (scr_gameref_t game, scr_int object)
{
  scr_int depth;

  for (depth = 0; depth < 64; depth++)
    {
      if (obj_is_static (game, object)
          && gs_object_static_unmoved (game, object))
        return FALSE;
      switch (gs_object_position (game, object))
        {
        case OBJ_HELD_PLAYER:
        case OBJ_WORN_PLAYER:
          return TRUE;
        case OBJ_IN_OBJECT:
        case OBJ_ON_OBJECT:
          object = gs_object_parent (game, object);
          continue;
        default:
          return FALSE;
        }
    }
  return FALSE;
}

scr_bool
lib_co_pre400 (scr_gameref_t game, const scr_char *line, scr_int object,
               scr_int mode)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int taf_version = prop_get_taf_version (bundle);
  const scr_char *shortname, *term;
  scr_int room, other, count;

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  term = lib_co_contains (line, shortname) ? shortname : nullptr;
  if (!term && taf_version >= TAF_VERSION_380)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; alias < alias_count && !term; alias++)
        {
          const scr_char *alias_name;

          vt_key[3].integer = alias;
          alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (lib_co_contains (line, alias_name))
            term = alias_name;
        }
    }
  if (!term || lib_co_term_shadowed (game, line, object, term))
    return FALSE;
  if (taf_version < TAF_VERSION_380)
    return TRUE;

  room = gs_playerroom (game);
  count = 0;
  GS_FOR_EACH_OBJECT (game, other)
    {
      if (lib_co_candidate (game, other, room)
          && lib_co_object_answers_to (game, other, term))
        count++;
    }
  /*
   * Mode 1 is takes()' (run390): the recount is over the loose namesakes.
   * Mode 2 is drops()' (run390 4458CF): the recount is over isheld() (42A34C)
   * -- held, worn, or in or on a parent that isheld, recursively, no openness
   * test.  So `drop hat` with the red hat in hand and the blue one on the
   * floor recounts to one and drops the red hat, while red worn and blue held
   * recounts to two and drops nothing (p39TASK,
   * runner_probes/task.run390.pname.txt /
   * runner_probes/task.run390.pname2.txt, 2026-09-19).
   *
   * Deliberate deviation: run380 has no modes, and no Runner narrows wear
   * or remove, so `wear hat` with the red hat held and the blue one on the
   * floor wears nothing under "Which hat."  Scarier recounts at 3.8 too,
   * and mode 3 (wear: held, not worn) and mode 4 (remove: worn) do the
   * same for those verbs.  See lib_co_mode_fits().
   */
  if (count > 1 && mode != 0)
    {
      scr_int fitting = 0;

      GS_FOR_EACH_OBJECT (game, other)
        {
          if (lib_co_object_answers_to (game, other, term)
              && lib_co_mode_fits (game, other, mode))
            fitting++;
        }
      if (fitting > 0)
        count = fitting;
    }
  if (count > 1)
    return lib_co_names_prefix (game, line, object);
  if (taf_version >= TAF_VERSION_390)
    return lib_co_candidate (game, object, room);
  return TRUE;
}

static scr_bool
lib_take_co_pre400 (scr_gameref_t game, const scr_char *line, scr_int object)
{
  return lib_co_pre400 (game, line, object, 1);
}


/*
 * lib_take_crowd_pre400()
 *
 * Below 4.0 a plain take COUNTS before it acts, and two objects named on
 * one line stop it dead.  takes() pre-passes the whole object table (run390
 * 454B95-454C9D, run380 43DCFB-43DE04, run370 4361ED-436301) and, for every
 * dynamic object lying directly in the player's room that co(obj, 1) -- a
 * bare c(Short) at 3.70 -- finds in the line, raises two counters: one for
 * the match (run390 454BF5, run380 43DD53, run370 436250) and, when the last
 * word of that object's own Prefix is NOT typed, a second (454C25 / 43DD8C /
 * 436289) that also stamps a per-object flag (454C85 / 43DDEC / 4362E9).
 * Only when BOTH counters come out at exactly 1 are the flags wiped again
 * (454CF2 / 43DE66 / 436363), and the loop that takes objects passes over
 * every object whose flag still stands (454E47-454E62 `... And var_118(obj)
 * = 0`, run380 43DFC3).  So a line naming two present, takeable objects
 * takes NEITHER, writes nothing, and comes out of takes() at the buffer-
 * empty catch-all as "Take what?" (run390 45588D).  The "Which <X> would you
 * like to take." prompt two lines above that clear (454CAD / 43DE41 /
 * 43633E) is dead code in all three: its guard is `var_10C > var_11A` and
 * the second counter can never outrun the first.
 *
 * A HELD object is not a candidate -- the pre-pass tests field 22 against
 * the room constant -- so it neither blocks nor is blocked, and the main
 * loop still lets it write "<You>'ve already got <X>!" before a loose object
 * indexed above it overwrites that with the take.  Hence `take hat coin`
 * with the hat in hand is "You pick up the coin." and carries both, while
 * `take coin hat` with both on the floor is "Take what?" and carries
 * neither.  A word that names nothing costs nothing: `take zzz coin` takes
 * the coin.  4.00 dropped the pre-pass; there the line is the noun scorer's
 * and the answer is "It is not clear which <object> you are referring to."
 * for the highest-indexed object it names, which Scarier already gives.
 *
 * Measured 2026-09-21 on p37ORD/p38ORD/p39ORD/p4ORD
 * (runner_probes/ord.run*.mult.*).
 *
 * Returns the object the loop is left free to take, or -1; sets *crowded
 * when every loose candidate was flagged and the take is off.
 */
static scr_int
lib_take_crowd_pre400 (scr_gameref_t game, scr_bool *crowded)
{
  const scr_char *const line = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  scr_int object, counted, flagged, acting, last_flagged, last_acting;

  *crowded = FALSE;
  if (!line
      || prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400
      || lib_input_contains_word (line, "all")
      || lib_input_contains_word (line, "and"))
    return -1;

  counted = flagged = acting = 0;
  last_flagged = last_acting = -1;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (obj_is_static (game, object)
          || gs_object_position (game, object) != room + 1
          || !lib_co_pre400 (game, line, object, 1))
        continue;
      counted++;
      if (lib_co_names_prefix (game, line, object))
        {
          acting++;
          last_acting = object;
        }
      else
        {
          flagged++;
          last_flagged = object;
        }
    }

  if (counted == 1 && flagged == 1)
    return last_flagged;
  if (acting > 0)
    return last_acting;
  if (counted > 0)
    *crowded = TRUE;
  return -1;
}

static scr_bool
lib_take_held_namesake_preempt_pre400 (scr_gameref_t game,
                                       scr_bool unresolved)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int taf_version = prop_get_taf_version (bundle);
  const scr_char *line = run_get_dispatch_input ();
  scr_int object, target, limit, speaker;

  if (!line || taf_version >= TAF_VERSION_400)
    return FALSE;

  /*
   * With no object resolved at all (unresolved: the parser found nothing
   * for the noun, as when the only namesake of the typed words is unseen),
   * takes() still walks every object, and a held or worn one whose co()
   * passes still writes the buffer: secret_of_lost_world T53 `take blue gem`
   * with the blue gem unseen on the fresh skeleton and the green gem in hand
   * answers "You already have the green gem!" (run390, ALR-rewritten
   * "'ve already got "; runner_transcripts/secret_of_lost_world, 2026-09-19).
   */
  /*
   * takes() is entered on c("get") Or c("take") Or c("pick"), And Not
   * c("from"), c("get on"), c("get down") (454476-4544DE), so `take off
   * bathrobe` reaches its loop too and the worn bathrobe writes " are already
   * wearing "; but the loop does not set the function's result (var_86 stays
   * 0 there; 455B28 is the taken exit), so generaltasks goes on to removes()
   * 45F49E, whose own line REPLACES the buffer: the_hangover T50 `take off
   * bathrobe` -> "You remove your bathrobe.", deardiary T5 `take off dress`
   * -> "I remove the blue dress." (runner_transcripts/deardiary).  Scarier's
   * remove rows sit below the take row, so decline here and let them answer.
   * Whether removes() also overwrites a refused `take off <held>` is unread.
   */
  if (unresolved && lib_co_contains (line, "off"))
    return FALSE;

  target = -1;
  if (!unresolved)
    {
      GS_FOR_EACH_OBJECT (game, object)
        {
          if (!game->multiple_references[object])
            continue;
          if (target != -1)
            return FALSE;
          target = object;
        }
      if (target == -1)
        return FALSE;
    }

  switch (unresolved ? OBJ_IN_OBJECT : gs_object_position (game, target))
    {
    case OBJ_IN_OBJECT:
    case OBJ_ON_OBJECT:
      break;
    case OBJ_HELD_NPC:
    case OBJ_WORN_NPC:
      if (taf_version < TAF_VERSION_380)
        return FALSE;
      break;
    default:
      return FALSE;
    }

  limit = taf_version < TAF_VERSION_380 || unresolved
          ? gs_object_count (game) : target;
  speaker = -1;
  for (object = 0; object < limit; object++)
    {
      if (object == target)
        continue;
      if (gs_object_position (game, object) != OBJ_HELD_PLAYER
          && gs_object_position (game, object) != OBJ_WORN_PLAYER)
        continue;
      /* Deliberate deviation: a held namesake whose Prefix the line rules
         out (`take old pin`, the new red pin in hand) does not speak; see
         lib_co_names_prefix(). */
      if (lib_take_co_pre400 (game, line, object)
          && !lib_co_prefix_excluded (game, line, object))
        speaker = object;
    }
  if (speaker == -1)
    return FALSE;

  if (gs_object_position (game, speaker) == OBJ_HELD_PLAYER)
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You've already got ",
                                           "I've already got ",
                                           "%player%'ve already got "));
  else
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You are already wearing ",
                                           "I am already wearing ",
                                           "%player% is already wearing "));
  if (taf_version >= TAF_VERSION_390)
    lib_print_object_np (game, speaker);
  else
    lib_print_object_raw (game, speaker);
  pf_buffer_string (filter, "!\n");
  gs_clear_multiple_references (game);
  return TRUE;
}

/*
 * lib_take_and_none_pre400()
 *
 * takes()'s "and" arm (var_A8/var_B4 = 2, set when the line has c("and")
 * and no c("all"): run390 45455D, run380 43D81A, run370 435EBA) pre-passes
 * every object before taking anything.  A dynamic object the line names is
 * a candidate when it lies loose in the player's room, or -- 3.8 and 3.9 --
 * in or on something loose here or a static present here; it either fits
 * or sets the hands-full flag.  With no candidate at all the summary
 * answers and nothing else in takes() speaks: 3.9 (454B08-454B5B) says
 * "<You> can't get either of them." when exactly two non-static objects
 * were named (var_122), else "... any of them."; run380 43DCC1 and run370
 * 4361B3 have the "any" form only.
 *
 * The versions name and filter differently.  run390 names with co(obj, 1)
 * (45483A) and wants the object seen (o(44)); run380 names with co(obj)
 * (43DA69) and has no seen test, so p38PUT's gem in the open static chest,
 * never listed, is a candidate and `take gem and coin` takes it; run370
 * names by c(Short) (43607F; c(Alias) only rewrites the line first) and
 * counts nothing in or on anything, so there the same line is "You can't
 * get any of them.".  Held objects and characters are named but never
 * candidates: p39ABSNPC `take erin and stone` with the stone held (run390x
 * runner_probes/absnpc.run390.txt T36), and p3xPUT `take coin and stone`,
 * `take nut and coin` (the nut in the held bag), `take statue and cupboard`,
 * `take zzz and qqq` (run370x/run380x/run390x
 * runner_probes/put.run370.takeand.rtf, runner_probes/put.run380.takeand.rtf,
 * runner_probes/put.run390.takeand.txt).  Each Runner's pre-pass also skips
 * an object whose "get <Short>" pre-matches a task; not modelled.
 */
static scr_bool
lib_take_and_none_pre400 (scr_gameref_t game)
{
  const scr_int taf_version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  scr_int object, named = 0;

  if (taf_version >= TAF_VERSION_400
      || !line || lib_input_contains_word (line, "all")
      || !lib_input_contains_word (line, "and"))
    return FALSE;

  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_int position;

      if (obj_is_static (game, object) || !lib_take_co_pre400 (game, line, object))
        continue;
      named++;
      if (taf_version >= TAF_VERSION_390 && !gs_object_seen (game, object))
        continue;
      position = gs_object_position (game, object);
      if (obj_directly_in_room (game, object, room))
        return FALSE;
      if (taf_version >= TAF_VERSION_380
          && (position == OBJ_IN_OBJECT || position == OBJ_ON_OBJECT)
          && obj_directly_in_room (game, gs_object_parent (game, object), room))
        return FALSE;
    }

  pf_buffer_string (gs_get_filter (game),
                    lib_select_response (game, "You", "I", "%player%"));
  pf_buffer_string (gs_get_filter (game),
                    named == 2 && taf_version >= TAF_VERSION_390
                    ? " can't get either of them.\n"
                    : " can't get any of them.\n");
  return TRUE;
}


/*
 * lib_instr_nocase()
 *
 * VB's InStr(1, line, name, vbTextCompare): the 1-based position of the
 * first case-insensitive occurrence of name in line, 0 when absent or when
 * name is empty.
 */
static scr_int
lib_instr_nocase (const scr_char *line, const scr_char *name)
{
  size_t length, offset;

  if (!line || !name || !*name)
    return 0;
  length = strlen (name);
  for (offset = 0; line[offset]; offset++)
    {
      if (scr_strncasecmp (line + offset, name, length) == 0)
        return (scr_int) offset + 1;
    }
  return 0;
}


/*
 * lib_name_instr_range()
 *
 * The lowest and highest InStr positions of an object's names on the line:
 * its Short and, 3.8 up, each Alias (run370 has rewritten aliases into the
 * Short by now).  Names absent from the line count for nothing.  FALSE when
 * none of the names is on the line.
 */
scr_bool
lib_name_instr_range (scr_gameref_t game, const scr_char *line,
                      scr_int object, scr_int *lowest, scr_int *highest)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int position;

  *lowest = 0;
  *highest = 0;
  position = lib_instr_nocase (line,
                               prop_get_indexed_string (bundle, "Objects",
                                                        object, "Short"));
  if (position > 0)
    *lowest = *highest = position;

  if (prop_get_taf_version (bundle) >= TAF_VERSION_380)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; alias < alias_count; alias++)
        {
          vt_key[3].integer = alias;
          position = lib_instr_nocase (line,
                                       prop_get_string (bundle, "S<-sisi",
                                                        vt_key));
          if (position <= 0)
            continue;
          if (*lowest == 0 || position < *lowest)
            *lowest = position;
          if (position > *highest)
            *highest = position;
        }
    }
  return *highest > 0;
}


/*
 * lib_obhere_380()
 *
 * run380's obhere() (4272E8, Form1.frm:4950; run370 has the same).  A
 * dynamic object is here when it is held, worn, loose in the player's room,
 * held or worn by a character in the room, or IN a parent that is here and
 * not closed; ON is not tested at all (only &HF6 recurses), so an object on
 * a surface is never "here" to it.  A static object is here when its room
 * list holds the player's room.
 */
scr_bool
lib_obhere_380 (scr_gameref_t game, scr_int object)
{
  const scr_int room = gs_playerroom (game);
  scr_int parent;

  if (obj_is_static (game, object))
    return obj_indirectly_in_room (game, object, room);

  switch (gs_object_position (game, object))
    {
    case OBJ_HELD_PLAYER:
    case OBJ_WORN_PLAYER:
      return TRUE;
    case OBJ_HELD_NPC:
    case OBJ_WORN_NPC:
      parent = gs_object_parent (game, object);
      return parent >= 0 && parent < gs_npc_count (game)
             && npc_in_room (game, parent, room);
    case OBJ_IN_OBJECT:
      parent = gs_object_parent (game, object);
      return parent >= 0 && parent < gs_object_count (game)
             && lib_obhere_380 (game, parent)
             && !(obj_is_container (game, parent)
                  && gs_object_openness (game, parent) > OBJ_OPEN);
    case OBJ_ON_OBJECT:
      return FALSE;
    default:
      return gs_object_position (game, object) == room + 1;
    }
}


/*
 * lib_present_370()
 *
 * What run370's catch-all counts as present when it names the object of a
 * take-from line it did not answer (lib_take_from_answer_370): a static
 * object in the room, or a dynamic one loose here, held, worn, or in or on
 * something the player carries.  Something in a box on the floor is not
 * present to it -- `get nut from statue` names the statue, not the nut
 * (p37TKA, run370x runner_probes/tka.run370.rtf cell 122, 2026-09-20).
 */
scr_bool
lib_present_370 (scr_gameref_t game, scr_int object)
{
  const scr_int room = gs_playerroom (game);

  if (obj_is_static (game, object))
    return obj_indirectly_in_room (game, object, room);

  switch (gs_object_position (game, object))
    {
    case OBJ_HELD_PLAYER:
    case OBJ_WORN_PLAYER:
      return TRUE;
    case OBJ_IN_OBJECT:
    case OBJ_ON_OBJECT:
      return obj_indirectly_held_by_player (game,
                                            gs_object_parent (game, object));
    default:
      return obj_directly_in_room (game, object, room);
    }
}


/*
 * lib_take_from_head()
 *
 * The words of a take-from line before its "from" (at offset at), minus the
 * verb word and the "up" of "pick up": the names the line gives, trimmed.
 */
std::string
lib_take_from_head (const scr_char *line, scr_int at)
{
  std::string head (line, (size_t) at);
  size_t start, end;

  start = head.find_first_not_of (' ');
  if (start == std::string::npos)
    return "";
  end = head.find (' ', start);
  if (end == std::string::npos)
    return "";
  if (scr_strncasecmp (head.c_str () + start, "pick", 4) == 0 && end - start == 4)
    {
      size_t up = head.find_first_not_of (' ', end);

      if (up != std::string::npos
          && scr_strncasecmp (head.c_str () + up, "up", 2) == 0
          && (head.size () == up + 2 || head[up + 2] == ' '))
        end = up + 2;
    }
  head = head.substr (end);
  start = head.find_first_not_of (' ');
  if (start == std::string::npos)
    return "";
  end = head.find_last_not_of (' ');
  return head.substr (start, end - start + 1);
}


/*
 * lib_take_from_slot_valid_pre390()
 *
 * The container tests run380's insides() applies to its slot, silently:
 * a container or surface (field 29), held or worn when dynamic, not closed.
 */
static scr_bool
lib_take_from_slot_valid_pre390 (scr_gameref_t game, scr_int slot)
{
  if (slot < 0)
    return FALSE;
  if (!(obj_is_container (game, slot) || obj_is_surface (game, slot)))
    return FALSE;
  if (obj_is_container (game, slot)
      && gs_object_openness (game, slot) > OBJ_OPEN)
    return FALSE;
  return !lib_take_container_unheld (game, slot);
}


namespace {

/*
 * The state takes()'s "and" arm carries from one named object to the next:
 * the message so far (from mark on), which of its shapes it is in, and the
 * parent a nested object rewrote the line to.
 */
struct lib_take_and_t
{
  scr_gameref_t game;
  scr_filterref_t filter;
  scr_int taf_version;
  const scr_char *line;
  scr_int room;
  const scr_char *you;
  scr_int candidates, taken, rewrite_parent;
  scr_bool overwritten, appended, cant_see, hands_full, picking;
  size_t mark;

  void count ();
  void seed ();
  void held (scr_int object);
  void worn (scr_int object);
  void loose (scr_int object);
  void in_parent (scr_int object, scr_int position);
  void visit (scr_int object);
  void from_parent ();
};

/* The pre-pass of lib_take_and_none_pre400(), counting this time. */
void
lib_take_and_t::count ()
{
  scr_int object;

  candidates = 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_int position;

      if (obj_is_static (game, object)
          || !lib_take_co_pre400 (game, line, object))
        continue;
      if (taf_version >= TAF_VERSION_390 && !gs_object_seen (game, object))
        continue;
      position = gs_object_position (game, object);
      if (obj_directly_in_room (game, object, room))
        candidates++;
      else if (taf_version >= TAF_VERSION_380
               && (position == OBJ_IN_OBJECT || position == OBJ_ON_OBJECT)
               && obj_directly_in_room (game, gs_object_parent (game, object),
                                        room))
        candidates++;
    }
}

/* Seed the message with "<You> pick up " and reset the loop's flags. */
void
lib_take_and_t::seed ()
{
  you = lib_select_response (game, "You", "I", "%player%");
  mark = pf_buffer_length (filter);
  pf_buffer_string (filter, you);
  pf_buffer_string (filter, " pick up ");
  picking = TRUE;

  taken = 0;
  rewrite_parent = -1;
  overwritten = appended = cant_see = hands_full = FALSE;
}

/* Held: "<You>'ve already got X!", once the message has left the seed. */
void
lib_take_and_t::held (scr_int object)
{
  if (overwritten)
    {
      pf_truncate (filter, mark);
      picking = FALSE;
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You've already got ",
                                             "I've already got ",
                                             "%player%'ve already got "));
      if (taf_version >= TAF_VERSION_390)
        lib_print_object_np (game, object);
      else
        lib_print_object_raw (game, object);
      pf_buffer_character (filter, '!');
      cant_see = FALSE;
    }
}

/* Worn, 3.8 up: "<You> are already wearing X!" replaces the message. */
void
lib_take_and_t::worn (scr_int object)
{
  if (taf_version >= TAF_VERSION_380)
    {
      pf_truncate (filter, mark);
      picking = FALSE;
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are already wearing ",
                                             "I am already wearing ",
                                             "%player% is already wearing "));
      if (taf_version >= TAF_VERSION_390)
        lib_print_object_np (game, object);
      else
        lib_print_object_raw (game, object);
      pf_buffer_character (filter, '!');
      overwritten = TRUE;
      cant_see = FALSE;
    }
}

/* Loose in the room: taken and named, or the hands-full line. */
void
lib_take_and_t::loose (scr_int object)
{
  scr_bool is_size;

  if (lib_take_over_capacity (game, object, &is_size))
    {
      if (!hands_full)
        {
          pf_buffer_string (filter, "  ");
          pf_buffer_string (filter, you);
          pf_buffer_string (filter,
                            lib_select_response (game,
                                " can't take any more, as your hands are full.",
                                " can't take any more, as my hands are full.",
                                " can't take any more, as %player%'s hands are full."));
          hands_full = TRUE;
        }
      appended = TRUE;
    }
  else
    {
      gs_object_player_get (game, object);
      gs_set_object_unmoved (game, object, FALSE);
      taken++;
      candidates--;
      /* The Runner appends the name straight after an overwritten
       * answer ("You are already wearing a hat!the stone.");
       * Scarier deliberately starts a fresh "  You pick up "
       * sentence there (deviation policy). */
      if (!picking)
        {
          pf_buffer_string (filter, "  ");
          pf_buffer_string (filter, you);
          pf_buffer_string (filter, " pick up ");
          picking = TRUE;
        }
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        candidates > 1 ? ", "
                        : candidates == 1 ? " and " : ".");
      appended = TRUE;
    }
}

/*
 * In or on a parent: note the parent for the rewritten line, or answer
 * "<You> can't see X from here!".
 */
void
lib_take_and_t::in_parent (scr_int object, scr_int position)
{
  if (lib_obhere_380 (game, object)
      || (position == OBJ_ON_OBJECT
          && obj_indirectly_in_room (game, object, room)))
    rewrite_parent = gs_object_parent (game, object);
  else
    {
      pf_truncate (filter, mark);
      picking = FALSE;
      pf_buffer_string (filter, you);
      pf_buffer_string (filter, " can't see ");
      if (taf_version >= TAF_VERSION_390)
        lib_print_object_np (game, object);
      else
        lib_print_object_raw (game, object);
      pf_buffer_string (filter, " from here!");
      overwritten = TRUE;
      cant_see = TRUE;
    }
}

/* One named object: pick its arm by where the object is. */
void
lib_take_and_t::visit (scr_int object)
{
  scr_int position;

  position = gs_object_position (game, object);
  if (position == OBJ_HELD_PLAYER)
    held (object);
  else if (position == OBJ_WORN_PLAYER)
    worn (object);
  else if (obj_directly_in_room (game, object, room))
    loose (object);
  else if (taf_version >= TAF_VERSION_380
           && (position == OBJ_IN_OBJECT
               || (taf_version >= TAF_VERSION_390
                   && position == OBJ_ON_OBJECT))
           && ((!overwritten && !appended) || cant_see))
    in_parent (object, position);
}

/* The rewritten line reaches insides(); its answer replaces the tail. */
void
lib_take_and_t::from_parent ()
{
  scr_int object;
  scr_int any = 0;

  gs_clear_object_references (game);
  gs_clear_multiple_references (game);
  GS_FOR_EACH_OBJECT (game, object)
    {
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && !obj_is_static (game, object)
          && gs_object_parent (game, object) == rewrite_parent
          && lib_co_pre400 (game, line, object, 0)
          && (taf_version < TAF_VERSION_390
              || gs_object_seen (game, object)))
        {
          game->object_references[object] = TRUE;
          any++;
        }
    }
  pf_truncate (filter, mark);
  if (any > 0)
    {
      lib_take_from_single_named = FALSE;
      lib_take_from_object_backend (game, rewrite_parent);
    }
  else
    lib_take_from_empty (game, rewrite_parent, FALSE);
  lib_take_from_task_sweep_380 (game);
}

} /* namespace */


/*
 * lib_take_and_pre400()
 *
 * Pre-4.0 takes()'s "and" arm, once lib_take_and_none_pre400() has found a
 * candidate (run390 454B08-455B28, run380 43D788-43E996, run370's twin).
 * The message is seeded with "<You> pick up " and the loop then runs over
 * every non-static object the line names, in INDEX order, never in line
 * order:
 *
 *   held      "<You>'ve already got X!" REPLACES the message, but only once
 *             it no longer starts with the seed (run380 43E00E); while it
 *             does, a held object is passed over in silence.
 *   worn      3.8 up: "<You> are already wearing X!" replaces the message
 *             (43E0A2), with no such gate; 3.7 has no worn arm.
 *   loose     taken; the name is appended with ", ", " and " or "." by the
 *             count of candidates still to come (43E61F-43E6C2), or with
 *             the hands-full line when it will not fit (43E7C1).
 *   in a      3.8 up, while the message is still empty or the seed, or ends
 *   parent    " from here!": obhere() -> the line gets " from <parent>"
 *             appended for insides() to answer (43E47B); else the message
 *             becomes "<You> can't see X from here!".  ON is not tested
 *             in 3.8.  3.9 does take from a surface: The Corruption of
 *             Alex, `get flask and gloves and mask` with all three on the
 *             lab counter, is "You take the flask, the pair of gloves and
 *             the mask from the counter." in run390 (Delron walkthrough
 *             replay, 2026-10-09).
 *
 * so a worn object named first and a loose one after it print as one string,
 * "You are already wearing a hat!the stone." (run380x) / "You are already
 * wearing the hat!the stone." (run390x); a held one is silent: `take hat and
 * stone` with the hat in hand is "You pick up the stone.".  A tail that is
 * still the bare seed is "Please take objects from one place at a time."
 * (43E996; 3.9 4558B9; run370 has no such line), and the rewritten line then
 * reaches insides(), whose answer -- the take-from, or its nothing-inside
 * refusal -- replaces that one: `take nut and cup` with the nut in a held
 * open box is "You take a nut from the box.", while `take nut and key` with
 * the box on the floor stays at "Please take objects from one place at a
 * time." because the box is not held (3.8's slot test).
 *
 * Measured on p37TKA/p38TKA/p39TKA (runner_probes/tka.run370.rtf,
 * runner_probes/tka.run380.rtf, runner_probes/tka.run390.txt, cells 3, 6, 9,
 * 14, 17, 20, 23, 32, 40, 44, 168) and p37TKB/p38TKB/p39TKB
 * (runner_probes/tkb.run370.rtf, runner_probes/tkb.run380.rtf,
 * runner_probes/tkb.run390.txt, cells 3, 8, 31, 38, 44), 2026-09-20.  3.9's
 * own loop (454B08) prints the same strings with definite names.
 *
 * The loop's state is a lib_take_and_t: count() is the pre-pass, seed() the
 * opening message, visit() one named object -- held(), worn(), loose() or
 * in_parent() -- and from_parent() the insides() tail.
 */
static scr_bool
lib_take_and_pre400 (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int taf_version = prop_get_taf_version (bundle);
  const scr_char *line = run_get_dispatch_input ();
  lib_take_and_t take = { game, filter, taf_version, line,
                          gs_playerroom (game), nullptr, 0, 0, -1,
                          FALSE, FALSE, FALSE, FALSE, FALSE, 0 };
  scr_int object;

  if (taf_version >= TAF_VERSION_400 || !line
      || lib_input_contains_word (line, "all")
      || !lib_input_contains_word (line, "and"))
    return FALSE;

  take.count ();
  if (take.candidates == 0)
    return FALSE;

  take.seed ();
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (obj_is_static (game, object)
          || !lib_take_co_pre400 (game, line, object))
        continue;
      if (taf_version >= TAF_VERSION_390 && !gs_object_seen (game, object))
        continue;

      take.visit (object);
    }

  if (!take.overwritten && !take.appended)
    {
      pf_truncate (filter, take.mark);
      if (taf_version < TAF_VERSION_380)
        return FALSE;
      pf_buffer_string (filter, "Please take objects from one place at a time.");
    }

  if (take.taken == 0 && take.rewrite_parent != -1
      && lib_take_from_slot_valid_pre390 (game, take.rewrite_parent))
    take.from_parent ();

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_take_and_400()
 *
 * 4.0's takes() cuts an "and" line into pieces (Proc_19_23_473A34, entered
 * from 4731A2 per piece) and resolves each with the noun scorer on its own,
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
 * run400x runner_probes/tka.run400.txt / runner_probes/tkb.run400.txt,
 * 2026-09-20.  0 when this does not apply and the ordinary parse should run;
 * 1 when the pieces are in multiple_references and *references counts them.
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
      if (!game->multiple_references[piece])
        {
          game->multiple_references[piece] = TRUE;
          (*references)++;
        }
    }
  if (first_parent == -1)
    {
      for (size_t index_ = 0; index_ < refused.size (); index_++)
        {
          if (!game->multiple_references[refused[index_]])
            {
              game->multiple_references[refused[index_]] = TRUE;
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
 * lib_move_named_whole_line_pre400()
 *
 * The noun half of "a library verb is matched anywhere in the line".  Below
 * 4.0 neither takes() nor drops() parses the words after its verb: each one
 * walks the object table in index order and asks co() whether the object's
 * Short or an Alias stands ANYWHERE in the typed line -- takes() with mode 1
 * (run390 454E1D, run380 43DFC3, run370 4364E6) and drops() with mode 2
 * (run390 4458CF, and the same walk in run380 438889 / run370 430689).  So a
 * word the line holds that names nothing costs nothing: `blorp take coin` is
 * "You pick up the coin." and `blorp drop coin` "You drop the coin." in all
 * three older Runners (p37REW/p38REW/p39REW, make_rewriteprobe.py --
 * runner_probes/rew.run370.casc.rtf, runner_probes/rew.run380.casc.rtf,
 * runner_probes/rew.run390.casc.txt, 2026-09-20), and so are `take zzz hat`
 * and `drop zzz hat` at 3.90 (p39WHAT, runner_probes/what.run390.what2.txt,
 * same day).  Scarier binds %text% positionally, so with the verb hoisted to
 * the front by run_hoist_verb_line() the rest of the line went to the noun
 * and the take fell to "Take what?" while the drop fell to the absent row's
 * "You don't have a coin!".
 *
 * Only from the parse's failure branch, so every line whose words the parser
 * does account for keeps the answer it had; and only when ONE object in the
 * whole table answers the line, the caller's own filter accepting it.  Both
 * halves matter.  A crowd still reaches the disambiguation the Runner's own
 * namesake count asks for; and a second co() match the filter would reject
 * is not passed over, because the Runner walks those objects in the same
 * loop and they write their own refusals from it -- drops()' " don't have
 * <X>!" (run390 445CE3, run380 438E13) for a namesake that is not held --
 * and which of the two speaks is decided by index order and a message-empty
 * gate that nothing here measures.  wrecked (run380) `drop room key` is that
 * case: the small key is in hand where the room key is not, and the answer
 * has to stay the refusal it was.  The "all" and "and" arms walk co()
 * themselves (lib_take_and_pre400, lib_drop_and_arm_collect_pre400) and are
 * left alone here.
 */
scr_bool
lib_move_named_whole_line_pre400 (scr_gameref_t game,
                                  scr_bool (*resolver) (scr_gameref_t,
                                                        scr_int, scr_int),
                                  scr_int mode, scr_int *references)
{
  const scr_char *const line = run_get_dispatch_input ();
  scr_int object, found;

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400
      || !line
      || lib_input_contains_word (line, "all")
      || lib_input_contains_word (line, "and"))
    return FALSE;

  found = -1;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (!lib_co_pre400 (game, line, object, mode))
        continue;
      if (found != -1)
        return FALSE;
      found = object;
    }
  if (found == -1 || !resolver (game, found, -1))
    return FALSE;

  gs_clear_multiple_references (game);
  game->multiple_references[found] = TRUE;
  *references = 1;
  return TRUE;
}


/*
 * lib_take_npc_overwrite_tail()
 *
 * The tail every take refusal shares: a present character the line names
 * answers over whatever takes() just wrote, and at 3.80 its take-from arm
 * answers again over that.  See lib_take_npc_overwrite() for the arms and
 * their gates.  Ask it wherever a take writes and returns, not only from
 * the main loop -- the held-namesake pre-empt writes its "You've already
 * got the hat!" and returns, and the Runner overwrites that line too: `take
 * give hat bob` with the hat in hand is "I don't think Bob would appreciate
 * being handled." at 3.7/3.9/4.0 and "Bob is not carrying the hat!" at 3.8
 * (cell 5 of runner_probes/ord.run370.give.rtf,
 * runner_probes/ord.run380.give.rtf, runner_probes/ord.run390.give.txt,
 * runner_probes/ord.run400.give.txt, 2026-09-21).
 */

/*
 * lib_take_ask_overwrite_400()
 *
 * characters()' ask arm, the last text arm of its cascade, answering over the
 * take arm at 4.0.  The block is entered on whole-word `ask` or `talk to`
 * (47F8E5-47F8FF) and, with the character in the player's room, its gate is
 * `(no task And MemVar_4942E0 = 0) Or buffer = "<player> can't talk to
 * that."` (47F900-47F93C) -- no buffer-empty test, unlike its 3.9 twin at
 * 4597FE, which is why 4.0 alone does this.  Inside, c("about") and the
 * character named (45E99C mode 1) and InStr(line, "about") > 0 (47F93F-
 * 47F96F) take the subject as Mid(line, InStr(line, "about") + 6) (47F9A5),
 * and the topic loop ASSIGNS the reply it finds (47FA7C, 47FAA7, 47FAD2).
 * The no-topic "does not respond" (47FB83) writes only over an empty, "can't
 * talk to that." or " can't see " buffer, so an unanswered ask leaves the
 * take refusal standing; the "Use the format" hint for a line with no
 * `about` (47FBB0) is not measured and not modelled.
 *
 * Measured on p4ORD (run400x runner_probes/ord.run400.chr.txt, 2026-09-21):
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
 * player is on something, leave the line to lib_cmd_get_off_object(); and
 * since 2026-10-15 on nothing too, unless lib_get_off_may_take().
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
 * A take line holding "from" that the take-from rows above declined --
 * the container clause named nothing, or two things, or nothing at all --
 * is still a take-from to every Runner (see lib_take_from_and() and
 * lib_take_from_trailing()); pre-4.0 it is never a bare take, so the
 * catch-alls of the take-from-nowhere rows answer it, not the held namesake
 * or "Take what?" (`get coin from zzz` with the coin in hand:
 * run370x/run380x "You can't do that!", run390x "The coin isn't in or on
 * anything!"; p3xTKB runner_probes/tkb.run370.rtf, tkb.run380.rtf,
 * tkb.run390.txt cell 144, 2026-09-20).
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
   * noun scorer 463640 (473011), which counts whole words and skips the
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

  /* Pre-4.0: a held or worn namesake still answers; see
     lib_take_multi_namesake_pre400(). */
  if (!is_except && lib_take_multi_namesake_pre400 (game, TRUE))
    return LIB_TAKE_RETURN_TRUE;

  /* Pre-4.0: takes() names its object over the WHOLE line, co(obj, 1);
     see lib_move_named_whole_line_pre400(). */
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
      game->multiple_references[object] = TRUE;
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
  if (winner != -1 && !game->multiple_references[winner])
    {
      gs_clear_multiple_references (game);
      game->multiple_references[winner] = TRUE;
    }
  return FALSE;
}


/*
 * Pre-4.0, takes() reaches an object only through co(idx, 1) (run390
 * 454E1D, run380 43DFC3, run370 4364E6), so the one object the parser
 * resolved is still not taken when that test fails for it: two seen
 * namesakes present with no loose one and no Prefix word typed is the
 * pending "Which <term>" ambiguity, and NOTHING moves.  secret_of_lost_world
 * T59 `take scroll` with the ancient scroll in hand and the decayed scroll
 * on the fresh skeleton prints "Which scroll.  Ancient scroll or the
 * decayed scroll?" and leaves the decayed scroll where it was, so T74
 * `give princess decayed scroll` fails its held restriction ("You can't do
 * that right now.") and the lily, the red gem and the win never come
 * (run390, runner_transcripts/secret_of_lost_world, 2026-09-19).  Scarier
 * printed the same prompt over a take that had quietly happened.  Decline
 * the row: the ambiguity prompt replaces the turn's output anyway, and a
 * plain miss falls to the catch-all "Take what?" the Runner's empty buffer
 * reaches.
 *
 * 3.70 is not in this: its takes() calls no co() at all, and settles a
 * crowd of its own namesakes in lib_disambiguate_object_common().  A rock
 * ALIASED "gem" beside a gem is no crowd to it -- `take gem` takes both and
 * the rock speaks (p37OPENA, runner_probes/opena.run370.rtf, 2026-09-20) --
 * and this gate used to turn that into "Take what?".
 *
 * TRUE when the row is to be declined.
 */
static scr_bool
lib_take_multi_co_fails_pre400 (scr_gameref_t game)
{
  const scr_char *line = run_get_dispatch_input ();
  scr_int target;

  GS_FOR_EACH_OBJECT (game, target)
    if (game->multiple_references[target])
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

  /* A 3.8 bare take of something in or on an object is a take-from, and
     gets its task sweep; see lib_take_from_task_sweep_380(). */
  scr_bool is_take_from_380 = FALSE;
  if (!is_except)
    {
      scr_int index_;

      GS_FOR_EACH_OBJECT (game, index_)
        if (game->object_references[index_]
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

  /* Pre-4.0, one object named: a held namesake may answer for it, a crowd
     of loose candidates declines the row. */
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
   * insides(), whose container slot walk may pick another object the
   * rewritten line names (8811c2b76).  Deliberate deviation: Scarier takes
   * the object the line named from where it is.
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
