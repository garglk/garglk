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
 * "with" clauses, open, close, lock and unlock.
 *
 * See sclibrar.h for what the library files share and sclibrar_internal.h
 * for what the core files share.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"

/*
 * lib_with_clause_400()
 *
 * run400's therest (Proc_19_85_489F4C) splits a line holding " with " before
 * any verb test (4883C5-488615): the object is scored from the text before
 * it and the instrument from the text after it (463640, present and seen),
 * and either failing leaves therest silently (488430, 4884DB) for the object
 * catch-all.  A dynamic instrument not held answers "<You> don't have <X>."
 * (48856A), a static one "Don't be daft!" (48860D), and a held one becomes
 * var_9C = " with <the X>", which every refusal puts before its full stop:
 * "You can't cut the rope with the coin.", "You push the button with the
 * knife, but nothing happens.", "You can't turn the button on with the
 * knife.".  Measured 2026-09-14 on p4WITHQ.taf
 * (runner_probes/withq.run400.txt, runner_probes/withq.run400.2.txt,
 * runner_probes/withq.run400.3.txt).
 *
 * Each half is 463640 in mode 0: present and seen first, then any seen
 * object, so a half can name something the player saw and left.  An absent
 * instrument is "<You> don't have the gem." like any other not held, and
 * once both halves resolve 4887A0 answers an absent object "<You> can't see
 * the gem." (p4WITHQ2.taf, runner_probes/withq2.run400.txt, 2026-09-14).  The
 * "With what?" arm at 488505 tests an instrument neither present nor seen,
 * which 463640 never returns: it is dead.  run390's twin answers differently,
 * and it serves 3.70 and 3.80 as well; see lib_with_clause_390().
 */

scr_int
lib_with_half_400 (scr_gameref_t game, const scr_char *half)
{
  scr_int object;

  object = lib_verb_object_resolve_400_string (game, half, nullptr, TRUE);
  if (object < 0)
    object = lib_verb_object_resolve_400_string (game, half, nullptr, FALSE);
  return object;
}

/*
 * lib_with_clause_390()
 *
 * run390's whole-word twin (therest 45D123-45D264), measured on p39WITH.taf
 * (runner_probes/with.run390.txt, 2026-09-14), and run370's and run380's too:
 * all three answer `cut/push/fix/lock/turn/clear <object> with <instrument>`
 * alike, "With what?" for an instrument that is not present, "<You> don't have
 * <X>." for a dynamic one not held, and the " with <the X>" suffix when it is
 * held (p*WITHPFX, runner_probes/withpfx.run370.pfx4.rtf /
 * runner_probes/withpfx.run380.ws.rtf / runner_probes/withpfx.run390.ws.txt,
 * make_withprefixprobe.py, 2026-09-20).  It runs when the line references two
 * or more objects; the instrument is the last object named after the split
 * that is present (obhere), else the last one named anywhere (45D0D6).  Then:
 *
 *   not present            "With what?" (45D16D) -- `cut rope with gem`,
 *                          the gem seen or not.  At 3.9 the prefix it saves
 *                          at 45D1A0 DOES continue a line, inside therest
 *                          and nowhere else; p39WITH read it as continuing
 *                          nothing only because a `turns` sat between the
 *                          prompt and the `knife` that answered it, and a
 *                          line anything answers drops the prefix.  See
 *                          lib_with_prefix_390_note().
 *   present, not held      "<You> don't have <X>." (45D1CA: dynamic, and
 *                          position not held); a static instrument falls
 *                          through to the suffix -- 3.9 has no "Don't be
 *                          daft!" (unmeasured, read off the listing).
 *   held                   " with <the X>" before the arm's full stop, for
 *                          the can't-do and nothing-happens arms (`cut`,
 *                          `push`; `break` has no suffix).
 *
 * An absent first object is not measured, and is left to the handlers.
 */
static lib_with_clause_t
lib_with_clause_390 (scr_gameref_t game, const std::string &line,
                     size_t split, scr_int *object, scr_int *instrument,
                     scr_bool quiet = FALSE)
{
  const std::string head = line.substr (0, split);
  const std::string tail = line.substr (split + 6);
  scr_int index_, present = -1, anywhere = -1, referenced = 0;

  *object = -1;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      const scr_bool in_tail
        = lib_verb_object_name_score (game, index_, tail.c_str ()) > 0;
      const scr_bool in_head
        = lib_verb_object_name_score (game, index_, head.c_str ()) > 0;

      if (in_tail)
        {
          anywhere = index_;
          if (obj_indirectly_in_room (game, index_, gs_playerroom (game)))
            present = index_;
        }
      if (in_head && obj_indirectly_in_room (game, index_,
                                             gs_playerroom (game)))
        *object = index_;
      if (in_head || in_tail)
        referenced++;
    }
  *instrument = present >= 0 ? present : anywhere;
  /*
   * The count is of the objects the WHOLE line references, not of one per
   * half: `fff with gem rock`, a prefix continuation whose head names
   * nothing, is still the split's "You don't have the rock." at 3.90
   * (p39WITHPFX, runner_probes/withpfx.run390.pfx6.txt,
   * make_withprefixprobe.py, 2026-09-20).  Only the suffix arm needs a head
   * object, since it is the one that prints it.
   */
  if (referenced < 2 || *instrument < 0 || *object == *instrument)
    return LIB_WITH_NONE;

  if (present < 0)
    {
      if (!quiet)
        {
          pf_buffer_string (gs_get_filter (game), "With what?\n");
          /* 45D1A0; 3.90 alone. See lib_with_prefix_390_note(). */
          lib_with_prefix_390_note (game);
        }
      return LIB_WITH_ANSWERED;
    }
  if (!obj_is_static (game, *instrument)
      && gs_object_position (game, *instrument) != OBJ_HELD_PLAYER)
    {
      if (quiet)
        return LIB_WITH_ANSWERED;
      lib_print_response_object (game, "You don't have ", "I don't have ",
                                 "%player% don't have ", *instrument, ".\n");
      return LIB_WITH_ANSWERED;
    }
  return *object < 0 ? LIB_WITH_NONE : LIB_WITH_SUFFIX;
}

lib_with_clause_t
lib_with_clause_400 (scr_gameref_t game, scr_int *object, scr_int *instrument)
{
  const scr_char *input = run_get_dispatch_input ();
  std::string line;
  size_t split;

  if (!input)
    return LIB_WITH_NONE;
  line = input;
  split = line.find (" with ");
  if (split == std::string::npos)
    return LIB_WITH_NONE;

  if (!lib_is_version_400 (game))
    return lib_with_clause_390 (game, line, split, object, instrument);

  *object = lib_with_half_400 (game, line.substr (0, split).c_str ());
  if (*object < 0)
    return LIB_WITH_DECLINE;
  *instrument = lib_with_half_400 (game, line.substr (split + 6).c_str ());
  if (*instrument < 0)
    return LIB_WITH_DECLINE;

  if (obj_is_static (game, *instrument))
    {
      pf_buffer_string (gs_get_filter (game), "Don't be daft!\n");
      return LIB_WITH_ANSWERED;
    }
  if (!obj_indirectly_held_by_player (game, *instrument))
    {
      lib_print_response_object (game, "You don't have ", "I don't have ",
                                 "%player% don't have ", *instrument, ".\n");
      return LIB_WITH_ANSWERED;
    }
  if (!obj_indirectly_in_room (game, *object, gs_playerroom (game)))
    {
      lib_print_response_object (game, "You can't see ", "I can't see ",
                                 "%player% can't see ", *object, ".\n");
      return LIB_WITH_ANSWERED;
    }
  return LIB_WITH_SUFFIX;
}

/*
 * lib_with_clause_claims()
 *
 * TRUE if the " with " split would take this line, said without printing
 * anything.  run370's therest makes the split before the absent-object test
 * that opens it for every other line: `cut rock with pearl`, the pearl in
 * another room, is "With what?" and not "You can't see the pearl."
 * (p37WITHPFX, runner_probes/withpfx.run370.pfx4.rtf, make_withprefixprobe.py,
 * 2026-09-20).
 */
scr_bool
lib_with_clause_claims (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int object, instrument;
  std::string line;
  size_t split;

  if (!input)
    return FALSE;
  line = input;
  split = line.find (" with ");
  if (split == std::string::npos)
    return FALSE;

  return lib_with_clause_390 (game, line, split, &object, &instrument, TRUE)
         != LIB_WITH_NONE;
}

/*
 * lib_with_arm_390_applies()
 * lib_with_arm_390()
 *
 * run390's therest opens (45D2B7-45D3E7), right after the two-object split
 * of lib_with_clause_390(), with an arm for every other line holding the
 * whole word "with": the split did not claim it (var_D8 = ""), so no verb
 * below it will see an instrument.  It walks the objects co(obj, 0) finds
 * and answers the first whose Short or first Alias sits after "with" --
 * InStr against the lower-cased line, so case-sensitive -- with "I don't
 * understand what you want me to do with <the X>!"; failing that, "With
 * what?".  Either way therest ends there, and characters(), which run390
 * calls below it (460675), finds the message taken: its attack arm wants
 * an empty one.  run380, run370 and run400 have no such arm.
 *
 * Measured on p39NPCAMB (make_3738_npcambprobe.py), run390x
 * runner_probes/npcamb.run390.kill.txt and
 * runner_probes/npcamb.run390.with.txt:
 * `attack/hit/kill/kick/punch/fight/hug/cut/push dave with stone`, `zzz with
 * stone` and `hit cora with stone` (Cora next door) are the "!" line; `hit
 * dave with zzz`, `talk with dave`, `dance with dave`, `zzz with`, `stone
 * with`, `push stone with zzz`, `hit stone with dave` and `kick stone with`
 * are "With what?".  Handlers above therest keep the line (`x dave with
 * stone`, `wait with stone`), and the ask arm overwrites it (`ask dave about
 * key with stone` is "DAVE KEY.").
 *
 * The arm stores the prefix Left(line, InStr("with") + 4) for a question
 * continuation too (45D3E0), and so does the split's own "With what?"
 * (45D1A0).  Both continue; see lib_with_prefix_390_note().
 */
scr_bool
lib_with_arm_390_applies (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int object, instrument;
  std::string line;
  size_t split;

  if (!input || !lib_is_version_390 (game)
      || !lib_input_contains_word (input, "with"))
    return FALSE;

  line = input;
  split = line.find (" with ");
  return split == std::string::npos
         || lib_with_clause_390 (game, line, split, &object, &instrument,
                                 TRUE) != LIB_WITH_SUFFIX;
}

scr_bool
lib_with_arm_390 (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int object, instrument;
  const scr_char *with;
  std::string line;
  size_t split;

  if (!lib_with_arm_390_applies (game))
    return FALSE;

  /* The two-object split's own answers come first. */
  line = input;
  split = line.find (" with ");
  if (split != std::string::npos
      && lib_with_clause_390 (game, line, split, &object, &instrument)
         == LIB_WITH_ANSWERED)
    return TRUE;

  with = strstr (input, "with");
  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_vartype_t vt_key[4];
      const scr_char *name, *found;

      if (!lib_co_pre400 (game, input, object, 0))
        continue;

      name = prop_get_indexed_string (bundle, "Objects", object, "Short");
      found = scr_strempty (name) ? nullptr : strstr (input, name);
      if (!(found && found > with)
          && lib_alias_prepare (bundle, vt_key, "Objects", object) > 0)
        {
          vt_key[3].integer = 0;
          name = prop_get_string (bundle, "S<-sisi", vt_key);
          found = scr_strempty (name) ? nullptr : strstr (input, name);
        }
      if (found && found > with)
        {
          pf_buffer_string (filter,
                            "I don't understand what you want me to do"
                            " with ");
          lib_print_object_np (game, object);
          pf_buffer_string (filter, "!\n");
          lib_non_answer = TRUE;
          return TRUE;
        }
    }

  pf_buffer_string (filter, "With what?\n");
  /* 45D3E0; see lib_with_prefix_390_note(). */
  lib_with_prefix_390_note (game);
  return TRUE;
}

/*
 * lib_cant_do_with_400()
 *
 * The therest refusal "<You> can't <verb> <the object><particle> with <the
 * instrument>." for a line lib_with_clause_400() applies to; *handled is
 * FALSE when it does not apply, and the return is then meaningless.
 */
scr_bool
lib_cant_do_with_400 (scr_gameref_t game, const scr_char *verb,
                      const scr_char *particle, scr_bool *handled)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object = -1, instrument = -1;

  *handled = TRUE;
  switch (lib_with_clause_400 (game, &object, &instrument))
    {
    case LIB_WITH_NONE:
      *handled = FALSE;
      return FALSE;
    case LIB_WITH_DECLINE:
      return FALSE;
    case LIB_WITH_ANSWERED:
      return TRUE;
    case LIB_WITH_SUFFIX:
      break;
    }

  pf_buffer_string (filter,
                    lib_select_response (game, "You can't ", "I can't ",
                                         "%player% can't "));
  pf_buffer_string (filter, verb);
  pf_buffer_character (filter, ' ');
  lib_print_object_np (game, object);
  pf_buffer_string (filter, particle);
  lib_print_wrapped_object (game, " with ", instrument, ".\n");
  return TRUE;
}

/*
 * lib_cant_do_suffix_pre400()
 *
 * The same refusal for a pre-4.0 handler that has no arm of its own and so
 * leaves the line to therest(): "<You> can't <verb> <the object>", plus the
 * " with <the instrument>" the two-object split saved, and a full stop.
 * The split runs first, so its own answers ("With what?", "<You> don't have
 * <X>.") come out instead; see lib_with_clause_390().
 *
 * Measured on p*OPENW.taf, 2026-09-20: `close rock with gem` is "You can't
 * close the rock with the gem." at 3.70, 3.80 and 3.90 alike, and `open rock
 * with slab` / `open slab with gem` are "You can't open the rock with the
 * slab." / "You can't open the slab with the gem." at 3.70
 * (runner_probes/openw.run370.ow.rtf, runner_probes/openw.run370.ox.rtf).  The
 * slab is static, so a static instrument does fall through to the suffix, as
 * lib_with_clause_390()'s comment read off the listing -- 3.9 really has no
 * "Don't be daft!".
 */
static scr_bool
lib_cant_do_suffix_pre400 (scr_gameref_t game, const scr_char *verb,
                           scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int head = -1, instrument = -1;
  const lib_with_clause_t clause
    = lib_with_clause_400 (game, &head, &instrument);

  if (clause == LIB_WITH_ANSWERED)
    return TRUE;

  pf_buffer_string (filter,
                    lib_select_response (game, "You can't ", "I can't ",
                                         "%player% can't "));
  pf_buffer_string (filter, verb);
  pf_buffer_character (filter, ' ');
  lib_print_object_np (game, object);
  if (clause == LIB_WITH_SUFFIX)
    lib_print_wrapped_object (game, " with ", instrument, ".\n");
  else
    pf_buffer_string (filter, ".\n");
  return TRUE;
}


/*
 * lib_open_close_with_400()
 *
 * A 4.0 `open X with Y` or `close X with Y` is therest's refusal whatever X
 * is: "You can't open the button with the knife."
 * (runner_probes/withq.run400.3.txt), and on p4WITHQ2.taf the same for a
 * closed box and an open chest, open or close
 * (runner_probes/withq2.run400.txt).  therest's open and close arms (48880F,
 * 48884E) test only the word, and a locked X whose key is the named instrument
 * is no exception: p4LOCK's box (key = the held coin) answers "You can't open
 * the box with the coin." before and after `unlock box with coin`
 * (runner_probes/lock.run400.txt).  TRUE when the line was taken, with *status
 * the handler's return.
 *
 * But therest only ever sees the line openclose let go.  openclose resolves
 * over the WHOLE typed line, " with " tail and all (open 4756AB, close
 * 4759D5), and a unique present-and-seen winner is the object it acts on --
 * so the tail is not a barrier, it is more candidates.  p4LOCK / run400,
 * make_400_lockprobe.py (runner_probes/lock.run400.c.txt, 2026-09-20): in
 * Alpha, where the box and the coin both score, `open box with coin` ties and
 * falls to therest ("You can't open the box with the coin."), but `open box
 * with zzz` -- zzz naming nothing -- has the box alone and opens it, and from
 * Beta, with the box seen but left behind, `open box with coin` has the held
 * coin alone and answers openclose's own "You can't open the coin!".  Only a
 * tie, or a line nothing present matches at all, reaches the refusal below.
 */
static scr_bool
lib_open_close_with_400 (scr_gameref_t game, const scr_char *verb,
                         scr_bool *status)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int first, whole;
  scr_bool handled;

  if (!lib_is_version_400 (game) || !input || !strstr (input, " with "))
    return FALSE;

  whole = lib_verb_object_resolve_400_string (game, input, nullptr, TRUE);
  if (whole >= 0)
    {
      gs_clear_object_references (game);
      game->object_references[whole] = TRUE;
      return FALSE;
    }

  std::string line (input);
  first = lib_with_half_400 (game, line.substr (0, line.find (" with ")).c_str ());
  if (first < 0)
    return FALSE;

  *status = lib_cant_do_with_400 (game, verb, "", &handled);
  return handled;
}

/*
 * lib_open_close_tie_400()
 *
 * openclose resolves by the whole-line score even when the parser bound an
 * object; a tie leaves it with none, and run400 has two answers for it.  The
 * flat one is the hub T82 case (`open lower right cupboard`: "I can't open
 * that.", ALR-rewritten by the game), but a crowded line sometimes gets the
 * ambiguity question instead: `open rock gem chest` and `open chest gem` are
 * "Which chest.  The gem, the rock or the chest?" (p4OPENW / p4OPENA,
 * runner_probes/openw.run400.ox.txt, runner_probes/opena.run400.txt,
 * 2026-09-20).
 *
 * Which one it is was a long open lead, because by the INDICES of the objects
 * the line names -- word order makes no difference -- the matrix reads
 *
 *     {0,1,3}   the question, the term being the object at 3
 *     {0,1}  {1,2}  {0,1,2}  {0,2,3}  {1,2,3}  {0,1,2,3}   flat
 *
 * in all three probe worlds alike, whichever of them holds the openable
 * object (p4OPENL has the chest at index 0 and answers the very same `open
 * rock gem chest` flat, runner_probes/openl.run400.txt; p4OPENT has closed
 * containers at both 0 and 3 and still asks only about {0,1,3},
 * runner_probes/opent.run400.txt / runner_probes/opent.run400.b.txt).  So it
 * is not openability, not name length and not word order.
 *
 * It is the pending object of the very same 463640 walk a `drop` makes, run
 * here in mode 0 -- one pass, the co(i, 0) gate -- and the whole matrix is
 * that walk's Me(424) index+2 quirk: after a tie at index k the result holds
 * -(k+2), so the NEXT tied object's Short is compared with the Short of the
 * object two indexes past k.  {0,1,3} ties at 1, which makes the next
 * comparison object 3 -- the tied object itself, which of course matches, so
 * Me(424) becomes 3 and the question is raised about it.  Every other set
 * either compares two different Shorts or looks past the end of the object
 * table.  See lib_name_object_resolve_400().
 */
static scr_bool
lib_open_close_tie_400 (scr_gameref_t game, const scr_char *verb,
                        scr_bool *status)
{
  const scr_char *input = run_get_dispatch_input ();
  std::vector<scr_int> marked;
  scr_int object, pending, last_tied, mark_count;

  if (!lib_is_version_400 (game) || !input || strstr (input, " with "))
    return FALSE;

  object = lib_name_object_resolve_400 (game, input, 0, &pending, &last_tied,
                                        &marked, &mark_count);
  if (object != -1)
    return FALSE;

  if (pending >= 0 && (scr_int) marked.size () == mark_count)
    {
      lib_co_400_raise_named (game,
                              lib_drop_named_term_400 (game, pending, input,
                                                       TRUE),
                              marked);
      *status = TRUE;
      return TRUE;
    }

  *status = lib_cant_do_other (game, verb);
  return TRUE;
}

/*
 * lib_open_close_not_carried_400()
 *
 * The 4.0 Runner only opens or closes a dynamic object the player is holding
 * (or wearing, possibly nested in a carried container): its open handler
 * (Proc_19_3, loc_4757CA) allows the open when the object is static Or
 * Proc_21_46 (held-or-worn, recursive) passes, and otherwise answers "<I am>
 * not carrying <the object>!".  The 3.8 and 3.9 handlers have no such test.
 *
 * Deliberate deviation (2026-09-30): Scarier opens it where it stands, as at
 * 3.8/3.9 and as it did before the port (e66709de2).  A chest too heavy to
 * lift was otherwise unopenable.  So this logs the deviation and returns
 * FALSE; the Runner's refusal is kept, switched off, for reference.
 */
static scr_bool
lib_open_close_not_carried_400 (scr_gameref_t game, scr_int object)
{
  static const scr_bool refuse = FALSE;

  if (!lib_is_version_400 (game)
      || obj_is_static (game, object)
      || obj_indirectly_held_by_player (game, object))
    return FALSE;

  SCR_DEVIATION ("openclose_not_carried", "object=%ld", object);
  if (!refuse)
    return FALSE;

  lib_print_response_object (game,
                             "You are not carrying ",
                             "I am not carrying ",
                             "%player% is not carrying ",
                             object, "!\n");
  return TRUE;
}

/*
 * lib_cmd_open_object()
 *
 * Attempt to open the referenced object.
 */
scr_bool
lib_cmd_open_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, openness;
  scr_bool is_ambiguous;

  if (lib_open_close_with_400 (game, "open", &is_ambiguous))
    return is_ambiguous;

  if (lib_open_close_tie_400 (game, "open", &is_ambiguous))
    return is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "open", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Get the current object openness. */
  openness = gs_object_openness (game, object);

  /* React to the request based on openness state. */
  switch (openness)
    {
    case OBJ_OPEN:
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      /* run400 has only " is already open!" (no " are " form). */
      pf_buffer_string (filter, " is already open!\n");
      return TRUE;

    case OBJ_CLOSED:
      /* 4.0's carrying gate, not applied; see
         lib_open_close_not_carried_400(). */
      if (lib_open_close_not_carried_400 (game, object))
        return TRUE;

      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You open ",
                                             "I open ",
                                             "%player% open "));
      lib_print_object_np (game, object);
      pf_buffer_character (filter, '.');

      /*
       * Set open state, and list contents.  The 3.8 open handler
       * (openclose, run380 42F3A4) lists through whatisin1 (42998C: only a
       * dynamic container held directly by the player, position 0 -- not
       * worn, not lying in the room, not nested) and whatisin2 (4297AC:
       * only a static one present in the room); anything else gets the
       * bare "You open X."  Measured on jb2000.taf in run380, 2026-09-04:
       * `open bag` on a suitcase lying in the room prints just "You open
       * the brown suitcase.", while the same command after `take bag` adds
       * "  Inside the brown suitcase is a 9mm hand gun, a lazer watch and
       * a mind learner."  3.9 and 4.0 list regardless.
       */
      gs_set_object_openness (game, object, OBJ_OPEN);
      if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
        {
          /*
           * run370 does not list the held case at all, whatever the
           * decompilation of its whatisin pair suggested: `open box` with
           * the box in the player's hands and a stone and a coin inside it
           * answers the bare "You open the box." (p37DARK,
           * runner_probes/dark.run370.putin.txt:57, and again with one object
           * inside, runner_probes/dark.run370.putin2.txt:33), where run380 on
           * the same turn of the same feed adds "  Inside the box is a stone
           * and a coin." (p38DARK, runner_probes/dark.run380.putin.txt:57 /
           * runner_probes/dark.run380.putin2.txt:33).  2026-09-12.  run370
           * does hold the "  Inside " literal and does print it from `x box`
           * (runner_probes/dark.run370.putin.txt:21), so this is openclose's
           * own reach and not a missing string.  Nor does it list the static
           * arm: p37PUT `open chest`, a static container in the room with a
           * gem inside, is the bare "You open the chest." (run370x
           * runner_probes/put.run370.feed2.rtf, 2026-09-19), where run380 adds
           * "  Inside the chest is a gem."
           * (runner_probes/put.run380.feed2.rtf).
           */
          if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380
              && (obj_is_static (game, object)
                  || gs_object_position (game, object) == OBJ_HELD_PLAYER))
            lib_list_in_object_pre_390 (game, object);
        }
      else
        lib_list_in_object (game, object, TRUE, FALSE);
      pf_buffer_answer_break (filter);
      return TRUE;

    case OBJ_LOCKED:
      lib_print_response_object (game,
                                 "You can't open ",
                                 "I can't open ",
                                 "%player% can't open ",
                                 object, " as it is locked!\n");
      return TRUE;

    default:
      break;
    }

  /*
   * The object isn't openable.  3.7 has no refusal in openclose() (426770),
   * so the line reaches therest()'s can't-do tail, which ends in a period
   * (43D1E0): p37EXAM `open stone` is "You can't open the stone."
   * (run370 runner_probes/exam.run370.txt, 2026-09-14); run380 (42F071) and
   * later end in "!".
   *
   * Being therest's tail, 3.70's also carries the " with <the instrument>"
   * of a two-object split, which openclose's own 3.80 refusal above it
   * does not: `open rock with slab` is "You can't open the rock with the
   * slab." at 3.70 and the bare "You can't open the rock!" at 3.80 and
   * 3.90 (p*OPENW, runner_probes/openw.run370.ow.rtf /
   * runner_probes/openw.run380.ow.rtf / runner_probes/openw.run390.ow.txt,
   * 2026-09-20).
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380)
    return lib_cant_do_suffix_pre400 (game, "open", object);

  lib_print_response_object (game,
                             "You can't open ",
                             "I can't open ",
                             "%player% can't open ",
                             object, "!\n");
  return TRUE;
}


/*
 * lib_cmd_close_object()
 *
 * Attempt to close the referenced object.
 */
scr_bool
lib_cmd_close_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, openness;
  scr_bool is_ambiguous;

  if (lib_open_close_with_400 (game, "close", &is_ambiguous))
    return is_ambiguous;

  if (lib_open_close_tie_400 (game, "close", &is_ambiguous))
    return is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "close", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Get the current object openness. */
  openness = gs_object_openness (game, object);

  /* React to the request based on openness state. */
  switch (openness)
    {
    case OBJ_OPEN:
      /* Same 4.0-only carrying gate as in lib_cmd_open_object above, and
         the same deliberate deviation: Scarier closes it where it stands. */
      if (lib_open_close_not_carried_400 (game, object))
        return TRUE;

      lib_print_response_object (game,
                                 "You close ",
                                 "I close ",
                                 "%player% close ",
                                 object, ".\n");

      /* Set closed state. */
      gs_set_object_openness (game, object, OBJ_CLOSED);
      return TRUE;

    case OBJ_CLOSED:
    case OBJ_LOCKED:
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      /* run400 has only " is already closed!" (no " are " form). */
      pf_buffer_string (filter, " is already closed!\n");
      return TRUE;

    default:
      break;
    }

  /*
   * The object isn't closeable.
   *
   * 4.0 has a dedicated message for this, ending in a bang (run400 475A31,
   * with "!" appended at 475A5F).  No earlier Runner does: openclose() gives
   * `open` a not-openable branch but gives `close` none at all (run380
   * 42F25C..42F322 tests only openness 6 and 5, run390 43A2xx the same), so a
   * present-but-not-closeable object falls out of openclose() with the
   * message still empty and is answered by the generic can't-do tail further
   * down -- which ends in a period (run370 43D231, run380 443D31, run390
   * 45D4BE/45D4CF).  Same sentence, different punctuation.
   *
   * Measured on p39EXAM.taf (3.90), runner_probes/exam.run390.held.txt:
   *   `open stone` -> "You can't open the stone!"
   *   `close stone` -> "You can't close the stone."
   * and on p4EXAM.taf (4.00), runner_probes/exam.run400.txt, where both end in
   * "!".
   *
   * Coming from therest, the pre-4.0 line carries the two-object split's
   * " with <the instrument>" at every version: `close rock with gem` is
   * "You can't close the rock with the gem." under run370, run380 and
   * run390 alike (p*OPENW, runner_probes/openw.run370.ow.rtf /
   * runner_probes/openw.run380.ow.rtf / runner_probes/openw.run390.ow.txt,
   * 2026-09-20).
   */
  if (!lib_is_version_400 (game))
    return lib_cant_do_suffix_pre400 (game, "close", object);

  lib_print_response_object (game,
                             "You can't close ",
                             "I can't close ",
                             "%player% can't close ",
                             object, "!\n");
  return TRUE;
}


/*
 * lib_attempt_key_acquisition()
 *
 * Automatically get an object being used as a key, if possible.
 */
static void
lib_attempt_key_acquisition (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /* Disallow getting static objects. */
  if (obj_is_static (game, object))
    return;

  /* If the object is not seen or available, reject the attempt. */
  if (!((gs_object_seen (game, object)
         || !lib_matcher_requires_seen (game))
        && obj_indirectly_in_room (game, object, gs_playerroom (game))))
    return;

  /*
   * Check if we already have it, or are wearing it, or if a NPC has or is
   * wearing it.
   */
  if (gs_object_position (game, object) == OBJ_HELD_PLAYER
      || gs_object_position (game, object) == OBJ_WORN_PLAYER
      || gs_object_position (game, object) == OBJ_HELD_NPC
      || gs_object_position (game, object) == OBJ_WORN_NPC)
    return;

  /*
   * If the object is contained in or on something we're already holding,
   * capacity checks are meaningless.
   */
  if (!obj_indirectly_held_by_player (game, object))
    {
      if (lib_object_too_heavy (game, object)
          || lib_object_too_large (game, object))
        return;
    }

  /* Retry game commands for the object with a standard "get". */
  if (lib_try_game_command_short (game, "get", object))
    return;

  /* Note what we're doing. */
  if (gs_object_position (game, object) == OBJ_IN_OBJECT
      || gs_object_position (game, object) == OBJ_ON_OBJECT)
    {
      pf_buffer_string (filter, "(Taking ");
      lib_print_object_np (game, object);

      pf_buffer_string (filter, " from ");
      lib_print_object_np (game, gs_object_parent (game, object));
      pf_buffer_string (filter, " first)\n");
    }
  else
    {
      lib_print_wrapped_object (game, "(Picking up ", object, " first)\n");
    }

  /* Take possession of the object.  The implicit take runs the Runner's
   * own `takes`, so it spends OnlyWhenNotMoved mode 1 too. */
  gs_object_player_get (game, object);
  gs_set_object_unmoved (game, object, FALSE);
}


const lib_lock_verb_t LIB_UNLOCK_VERB = {
  OBJ_LOCKED, OBJ_CLOSED,
  "unlock",
  " anything to unlock ",
  {" is not locked!\n", " are not locked!\n"},
  {"You can't unlock ", "I can't unlock ", "%player% can't unlock "},
  {"You unlock ", "I unlock ", "%player% unlock "}
};

const lib_lock_verb_t LIB_LOCK_VERB = {
  OBJ_CLOSED, OBJ_LOCKED,
  "lock",
  " anything to lock ",
  {" is already locked!\n", " are already locked!\n"},
  {"You can't lock ", "I can't lock ", "%player% can't lock "},
  {"You lock ", "I lock ", "%player% lock "}
};

/* What lib_lock_check_openness() made of the object's current state. */
enum {
  LIB_LOCK_PROCEED, LIB_LOCK_REFUSED, LIB_LOCK_NOT_LOCKABLE
};


/*
 * lib_lock_check_openness()
 *
 * Decide whether the object is in a state this verb can work on, printing
 * the refusal itself if it is not.  Locking something that stands open is
 * refused in its own terms; every other openness the verb doesn't act on
 * gets the "is not locked"/"is already locked" complaint.  Anything with no
 * openness at all isn't lockable, and the caller says so.
 */
static scr_int
lib_lock_check_openness (scr_gameref_t game, scr_int object,
                         const lib_lock_verb_t *verb)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int openness;

  openness = gs_object_openness (game, object);
  if (openness == verb->required_openness)
    return LIB_LOCK_PROCEED;

  if (verb->new_openness == OBJ_LOCKED && openness == OBJ_OPEN)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             verb->cant[0],
                                             verb->cant[1],
                                             verb->cant[2]));
      lib_print_object_np (game, object);
      pf_buffer_string (filter, " as it is open.\n");
      return LIB_LOCK_REFUSED;
    }

  if (openness == OBJ_OPEN || openness == OBJ_CLOSED || openness == OBJ_LOCKED)
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      /* run400 has only " is already locked!" (47610D) and " is not
       * locked!" (476448); no " are " form. */
      pf_buffer_string (filter,
                        lib_is_version_400 (game)
                        ? verb->wrong_state[0]
                        : lib_select_plurality (game, object,
                                                verb->wrong_state[0],
                                                verb->wrong_state[1]));
      return LIB_LOCK_REFUSED;
    }

  return LIB_LOCK_NOT_LOCKABLE;
}


/*
 * lib_lock_backend()
 *
 * Attempt to lock or unlock the referenced object.  With with_key set the
 * key comes from the player's own referenced text and has to be the right
 * one; otherwise the object's key is looked up and the player tries to lay
 * hands on it first.
 */
/* 4.0: a keyless object's (un)lock line is therest's refusal; see below. */
static scr_bool
lib_lock_therest_400 (scr_gameref_t game, const lib_lock_verb_t *verb,
                      scr_int object)
{
  scr_bool handled;
  const scr_bool status = lib_cant_do_with_400 (game, verb->verb, "",
                                                &handled);

  if (handled)
    return status;
  pf_buffer_string (gs_get_filter (game),
                    lib_select_response (game, verb->cant[0], verb->cant[1],
                                         verb->cant[2]));
  lib_print_object_np (game, object);
  pf_buffer_string (gs_get_filter (game), ".\n");
  return TRUE;
}

/*
 * lib_lock_absent_object_400()
 *
 * openclose's lock and unlock arms resolve their object with 463640 in mode
 * 0 (475D91, 476141) -- present and seen, then any seen object -- on the
 * text before "with" (475D5D), and nothing between that and the Key and
 * openness tests looks at where the object is.  So a seen object in another
 * room, or shut inside a closed container, still gets its state refusal:
 * sswhore (4.00) `unlock drawer` and `unlock drawer with skeleton key` with
 * the desk drawer seen but inside the closed desk answer "The desk drawer is
 * not locked!" (runner_transcripts/sswhore.txt, T84/T97), where our %object%
 * scope saw nothing and answered "You can't unlock that." and a key prompt.
 *
 * The whole arm runs on that object, not just its state refusal: from Beta,
 * with p4LOCK's box left locked in Alpha and its key -- the coin -- in hand,
 * run400 answers `lock box with coin` "You lock the box with the coin." and
 * then `unlock box` "You unlock the box with the coin.", and only the
 * openness refusals when the box is already in the state asked for
 * (make_400_lockprobe.py / runner_probes/lock.run400.b.txt, 2026-09-20).
 * Deliberate deviation (2026-10-17): the arm reaches only into the player's
 * room.  Locking or unlocking an object that is somewhere else is the same
 * Runner bug as the put fetch removed the same day, and no game needs it;
 * what the gate keeps is sswhore's drawer, seen and in the room but shut
 * inside the closed desk, which is the only corpus case.  An object in
 * another room falls through to the ordinary "You can't lock/unlock that."
 * Logged under "Deliberate deviations" in
 * test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md.
 *
 * Returns the object, or -1 when the present pass matched or tied, when the
 * head names nothing seen or nothing in the room, or when the object has no
 * Openable/Key for the arm to work on.
 */

/*
 * lib_lock_object_in_room_400()
 *
 * Is OBJECT in the player's room, counting one shut inside a closed
 * container there?  obj_indirectly_in_room() stops at a closed parent, so
 * the containment chain is climbed first, whatever each parent's openness.
 */
static scr_bool
lib_lock_object_in_room_400 (scr_gameref_t game, scr_int object)
{
  scr_int depth;

  for (depth = 0; depth < 32 && !obj_is_static (game, object); depth++)
    {
      const scr_int position = gs_object_position (game, object);

      if (position != OBJ_IN_OBJECT && position != OBJ_ON_OBJECT)
        break;
      object = gs_object_parent (game, object);
      if (object < 0)
        return FALSE;
    }
  return obj_indirectly_in_room (game, object, gs_playerroom (game));
}

scr_int
lib_lock_absent_object_400 (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_vartype_t vt_key[3], vt_rvalue;
  std::string head;
  size_t split;
  scr_int object;

  if (!lib_is_version_400 (game) || !input)
    return -1;
  head = input;
  split = head.find (" with ");
  if (split != std::string::npos)
    head = head.substr (0, split);

  if (lib_verb_object_resolve_400_string (game, head.c_str (), nullptr, TRUE)
      != -2)
    return -1;
  object = lib_verb_object_resolve_400_string (game, head.c_str (), nullptr,
                                               FALSE);
  if (object < 0 || !lib_lock_object_in_room_400 (game, object))
    return -1;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Openable";
  if (!prop_get (bundle, "I<-sis", &vt_rvalue, vt_key)
      || vt_rvalue.integer <= 0)
    return -1;
  vt_key[2].string = "Key";
  if (!prop_get (bundle, "I<-sis", &vt_rvalue, vt_key)
      || vt_rvalue.integer < 0)
    return -1;

  return object;
}

/*
 * lib_lock_has_key_400()
 *
 * Return TRUE if OBJECT is one the 4.0 lock and unlock arms act on: openable,
 * with a key.
 */
static scr_bool
lib_lock_has_key_400 (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3], vt_rvalue;

  /*
   * The loader reads a Key only for Openable > 1 and stores -1 otherwise
   * (4907DD-4907F7).  Both properties are fetched tolerantly:
   * prop_get_integer() is fatal on a missing one, and an object with no
   * Openable at all does exist (see scdump.cpp).
   */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Openable";
  if (!prop_get (bundle, "I<-sis", &vt_rvalue, vt_key)
      || vt_rvalue.integer <= 0)
    return FALSE;
  vt_key[2].string = "Key";
  if (!prop_get (bundle, "I<-sis", &vt_rvalue, vt_key)
      || vt_rvalue.integer < 0)
    return FALSE;
  return TRUE;
}

/*
 * lib_lock_with_key()
 *
 * Lock or unlock OBJECT, whose openness allows it, with KEY if WITH_KEY and
 * with its own key otherwise.  Returns FALSE if the object has no key, so is
 * not lockable after all.
 */
static scr_bool
lib_lock_with_key (scr_gameref_t game, const lib_lock_verb_t *verb,
                   scr_int object, scr_int key, scr_bool with_key)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int key_index, the_key;

  key_index = prop_get_indexed_integer (bundle, "Objects", object,
                                        "Key");
  if (key_index == -1)
    return FALSE;

  /* A Key naming no object reads as no key at all. */
  the_key = obj_dynamic_object (game, key_index);
  if (the_key < 0)
    return FALSE;
  if (with_key)
    {
      /*
       * Naming the key is what picks it up: the keyless branch takes
       * the object's Key straight out of the property and tests the
       * hands, while the named one runs the Runner's implicit get.
       * p4WTIE (run400, runner_probes/wtie.run400.10.txt, 2026-09-20),
       * the coin dropped: `unlock box with coin` is "(Picking up the
       * coin first)" then "You unlock the box with the coin." and leaves
       * the coin carried, where bare `unlock box`, `unlock box with
       * stone` and `unlock box with zzz` are all "You don't have
       * anything to unlock the box with!".  The refusals come first
       * either way -- a wrong named key on the floor is the flat "You
       * can't unlock the box with the knife.", and the state refusal
       * "The box is already locked!" precedes both.
       */
      if (the_key != key)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 verb->cant[0],
                                                 verb->cant[1],
                                                 verb->cant[2]));
          lib_print_object_np (game, object);
          lib_print_wrapped_object (game, " with ", key, ".\n");
          return TRUE;
        }
      lib_attempt_key_acquisition (game, key);
    }
  else
    key = the_key;

  /*
   * The runner asks whether the key is indirectly held by the player,
   * not whether it sits in the hands: a key that is worn, or stowed in
   * an open bag being carried, unlocks perfectly well.  Provenance
   * relies on this -- its walkthrough wears the brass key so that the
   * cave's forced "drop all" can't take it away, then unlocks the
   * wooden chest while still only wearing it.
   */
  if (!obj_indirectly_held_by_player (game, key))
    {
      if (with_key)
        {
          lib_print_response_object (game,
                                     "You are not holding ",
                                     "I am not holding ",
                                     "%player% is not holding ",
                                     key, ".\n");
        }
      else
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You don't have",
                                                 "I don't have",
                                                 "%player% don't have"));
          pf_buffer_string (filter, verb->nothing_to);
          lib_print_object_np (game, object);
          pf_buffer_string (filter, " with!\n");
        }
      return TRUE;
    }

  gs_set_object_openness (game, object, verb->new_openness);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         verb->does[0],
                                         verb->does[1],
                                         verb->does[2]));
  lib_print_object_np (game, object);
  lib_print_wrapped_object (game, " with ", key, ".\n");
  return TRUE;
}

scr_bool
lib_lock_backend (scr_gameref_t game, const lib_lock_verb_t *verb,
                  scr_bool with_key)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, key = -1;
  scr_bool is_ambiguous;

  /*
   * No Runner before 4.0 has a lock handler.  run370, run380 and run390 carry
   * no lock wording beyond therest()'s checkverb pair " can't lock " /
   * " can't unlock " (run390 45E468/45E4AD); " is not locked!", " is already
   * locked!", " as it is open." and " don't have anything to unlock " are
   * run400's alone (index/pool.py -s ock).  So below 4.0 every lock and
   * unlock line is therest's, which run390 reaches only after the
   * out-of-room task refusal at loc_45FFE8: thetest (3.90) `unlock door` in
   * the Room of Eternal Dialing, with task 14 `unlock door` confined to room
   * 0, is "You can't do that here!" (runner_transcripts/thetest_win.txt
   * T68-77), not "You can't unlock the door.".  Decline, and let the
   * fallback rows below the refusal answer -- lib_cmd_lock_object_pre_400().
   */
  if (!lib_is_version_400 (game))
    return FALSE;

  /*
   * 4.0 never asks what to use: openclose() starts var_88 at -1 (475C63),
   * sets it only from a " with " half that resolves (475CB0, and the present
   * objects' loop at 475CD2), and a lock arm left at -1 takes the keyless
   * branch -- the object's own key if held (476360), else "<player> don't
   * have anything to unlock <it> with!" (4763ED; lock 4760A6), with no
   * pick-up on the way.  House's `unlock back door with metal key` before the
   * key was ever seen (runner_probes/house_sober.run400.txt, T137).  The
   * question itself is in no Runner's string pool, 3.7 to 4.0, so the older
   * versions keep SCARE's wording only because their arms are unread.
   */
  scr_bool absent_400 = FALSE;

  /*
   * The arm's own object comes first: it resolved the head of the line with
   * 463640 and does not care where the object is, so a seen-but-absent one
   * is locked and unlocked just the same -- in the Runner; Scarier's arm
   * reaches only into the player's room (lib_lock_absent_object_400(), the
   * 2026-10-17 deviation).  Its key then comes from the
   * " with " half alone (475CB0) -- the parser bound no reference text for a
   * line it could not place -- and a half that resolves to nothing leaves
   * var_88 at -1, the keyless branch.
   */
  object = lib_lock_absent_object_400 (game);
  if (object >= 0)
    {
      const scr_char *input = run_get_dispatch_input ();
      const scr_char *tail = input ? strstr (input, " with ") : nullptr;

      absent_400 = TRUE;
      with_key = FALSE;
      if (tail)
        {
          key = lib_with_half_400 (game, tail + 6);
          if (key >= 0)
            with_key = TRUE;
        }
    }
  else
    {
      /* Get the referenced object, and if none, consider complete. */
      object = lib_disambiguate_object (game, verb->verb, &is_ambiguous);
      if (object == -1)
        return is_ambiguous;
    }

  /*
   * run400's lock and unlock arms in openclose (Proc_19_3_476468) resolve
   * the object, leave with `Exit Sub` when nothing scores (475D91, 47614F),
   * and then do all of their work -- "can't lock X as it is open.", "is not
   * locked!", the key checks -- under `If object.Key > 0` (475DAB, 476169).
   * An object with no key falls out of the arm having said nothing, and
   * therest answers: `lock button` is "You can't lock the button."
   * (runner_probes/withq.run400.2.txt, turn 6), `lock button with coin` "You
   * can't lock the button with the coin." (runner_probes/withq.run400.3.txt).
   * hcw's `unlock door with keys` in the parking lot, no door present, is the
   * catch-all "I don't understand what you want to do with Susan's keys."
   * (runner_probes/hcw.run400.txt, turn 189) because therest's " with " split
   * finds no door; see lib_with_clause_400().
   */
  if (lib_is_version_400 (game) && !lib_lock_has_key_400 (game, object))
    return lib_lock_therest_400 (game, verb, object);

  /*
   * Now try to get the key from referenced text, and disambiguate as usual.
   * The absent-object arm above already has its key, from the " with " half.
   */
  if (with_key && !absent_400)
    {
      /*
       * A present object reads its key out of the " with " half exactly
       * as the absent arm above does -- openclose scores that text with
       * 463640 (475CB0) and never consults the parser's references, so
       * a half that names nothing AND a half that ties both leave var_88
       * at -1 and take the keyless branch, silently.  p4WTIE (run400,
       * runner_probes/wtie.run400.3.txt, runner_probes/wtie.run400.4.txt,
       * runner_probes/wtie.run400.5.txt, 2026-09-20): with the coin as the
       * box's key, `unlock box with stone` and `unlock box with gems` -- two
       * "stone" Shorts, two "gems" aliases -- are "You unlock the box with the
       * coin." just like `unlock box with zzz`, and `lock box with stone` is
       * the lock twin.  A half that resolves to the wrong object is still the
       * flat "You can't unlock the box with the knife.".  So 4.0 asks nothing
       * here, and SCARE's old "<verb> that with what?" prompt -- in no
       * Runner's string pool -- is gone: sswhore's `unlock drawer with key`
       * invented it.
       */
      const scr_char *input = run_get_dispatch_input ();
      const scr_char *tail = input ? strstr (input, " with ") : nullptr;

      key = tail ? lib_with_half_400 (game, tail + 6) : -1;
      if (key < 0)
        with_key = FALSE;
    }

  /* React to the request based on openness state. */
  switch (lib_lock_check_openness (game, object, verb))
    {
    case LIB_LOCK_REFUSED:
      return TRUE;

    case LIB_LOCK_PROCEED:
      if (lib_lock_with_key (game, verb, object, key, with_key))
        return TRUE;
      break;

    default:
      break;
    }

  /* The object isn't lockable. */
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         verb->cant[0],
                                         verb->cant[1],
                                         verb->cant[2]));
  lib_print_object_np (game, object);
  pf_buffer_string (filter, ".\n");
  return TRUE;
}


/*
 * lib_cmd_unlock_object_with()
 * lib_cmd_unlock_object()
 * lib_cmd_lock_object_with()
 * lib_cmd_lock_object()
 *
 * Attempt to lock or unlock the referenced object, either with the key the
 * player named or with one selected automatically.
 */
scr_bool
lib_cmd_unlock_object_with (scr_gameref_t game)
{
  return lib_lock_backend (game, &LIB_UNLOCK_VERB, TRUE);
}

scr_bool
lib_cmd_unlock_object (scr_gameref_t game)
{
  return lib_lock_backend (game, &LIB_UNLOCK_VERB, FALSE);
}

scr_bool
lib_cmd_lock_object_with (scr_gameref_t game)
{
  return lib_lock_backend (game, &LIB_LOCK_VERB, TRUE);
}

scr_bool
lib_cmd_lock_object (scr_gameref_t game)
{
  return lib_lock_backend (game, &LIB_LOCK_VERB, FALSE);
}
