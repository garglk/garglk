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
 * Taking objects below 4.0: the Runner's co() name test (lib_co_pre400,
 * which every pre-4.0 handler uses) and its 3.7-3.9 helpers (isheld,
 * obhere, present, the InStr name ranges), the takes() loop's crowd and
 * held-namesake answers, and the "take X and Y" arm.  The command forms
 * that call them, and the 4.0 counterparts, are in
 * sclibrar_take_multiple.cpp.
 */

#include "sclibrar_take_internal.h"


/*
 * lib_take_held_namesake_preempt_pre400()
 *
 * The pre-4.0 takes() walks EVERY object in index order and lets each one
 * whose Short or Alias sits in the typed line write the answer, whether or
 * not it is the object the line meant.  A held one writes "<You>'ve already
 * got <object>!" (run390 @454EC5) and a worn one "<You> are already wearing
 * <object>!", both raising the outcome rank to 4.  The branch that takes an
 * object out of a container, off a surface or from a character -- the one
 * that rewrites the line into "take X from <parent>" -- is gated on the
 * buffer being still empty (or " pick up "/" from here!") AND rank < 3
 * (run390 @4551E4), so a held namesake indexed BELOW the target has already
 * spoken and the target is never taken.  An object lying loose in the room
 * is different: "<You> pick up X." overwrites the buffer, so only the
 * not-in-the-room targets lose.
 *
 * Measured in run390 (runner_transcripts/stardust T38): `take
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
 * a bare c(Short).  co() (run390 @43B6BC) is false unless the object's
 * Short or Alias sits in the line; the term found is then counted over every
 * present object answering to it (obhere, and seen from 3.9 --
 * lib_co_candidate).  With more than one, 3.9's mode 1 recounts only the
 * ones lying loose in the player's room (field 22 = room) and uses that
 * count when it is not zero; if the term is still ambiguous the object
 * passes only when the last word of its own Prefix is in the line, otherwise
 * 3.9 asks that the object itself be present and seen (@43B661) where 3.8
 * passes it outright (@42DDEA).  So `take blue plate` with the red plate in
 * hand and the blue
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
   * Mode 2 is drops()' (run390 4458CF): the recount is over isheld() -- held,
   * worn, or in or on a parent that isheld, recursively, no openness test.
   * So `drop hat` with the red hat in hand and the blue one on the floor
   * recounts to one and drops the red hat, while red worn and blue held
   * recounts to two and drops nothing (p39TASK,
   * runner_probes/task.run390.pname*.txt).
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

scr_bool
lib_take_co_pre400 (scr_gameref_t game, const scr_char *line, scr_int object)
{
  return lib_co_pre400 (game, line, object, 1);
}


/*
 * lib_take_crowd_pre400()
 *
 * Below 4.0 a plain take COUNTS before it acts, and two objects named on
 * one line stop it dead.  takes() pre-passes the whole object table (run390
 * 454B95) and, for every dynamic object lying directly in the player's room
 * that co(obj, 1) -- a bare c(Short) at 3.7 -- finds in the line, raises two
 * counters: one for the match and, when the last word of that object's own
 * Prefix is NOT typed, a second that also stamps a per-object flag.  Only
 * when BOTH counters come out at exactly 1 are the flags wiped again, and
 * the loop that takes objects passes over every object whose flag still
 * stands (run390 454E47).  So a line naming two present, takeable objects
 * takes NEITHER, writes nothing, and comes out of takes() at the buffer-
 * empty catch-all as "Take what?" (run390 45588D).  The "Which <X> would you
 * like to take." prompt two lines above that clear (run390 454CAD) is dead
 * code in all three: its guard is the second counter exceeding the first,
 * and the second counter can never outrun the first.
 *
 * A HELD object is not a candidate -- the pre-pass tests field 22 against
 * the room constant -- so it neither blocks nor is blocked, and the main
 * loop still lets it write "<You>'ve already got <X>!" before a loose object
 * indexed above it overwrites that with the take.  Hence `take hat coin`
 * with the hat in hand is "You pick up the coin." and carries both, while
 * `take coin hat` with both on the floor is "Take what?" and carries
 * neither.  A word that names nothing costs nothing: `take zzz coin` takes
 * the coin.  4.0 dropped the pre-pass; there the line is the noun scorer's
 * and the answer is "It is not clear which <object> you are referring to."
 * for the highest-indexed object it names, which Scarier already gives.
 *
 * Measured on p37ORD/p38ORD/p39ORD/p4ORD
 * (runner_probes/ord.run*.mult.*).
 *
 * Returns the object the loop is left free to take, or -1; sets *crowded
 * when every loose candidate was flagged and the take is off.
 */
scr_int
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

scr_bool
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
   * "'ve already got "; runner_transcripts/secret_of_lost_world).
   */
  /*
   * takes() is entered on c("get") Or c("take") Or c("pick"), And Not
   * c("from"), c("get on"), c("get down") (run390), so `take off bathrobe`
   * reaches its loop too and the worn bathrobe writes
   * " are already wearing "; but the loop does not set the function's
   * result (it stays 0 there; 455B28 is the taken exit), so generaltasks
   * goes on to removes() 45F49E, whose own line REPLACES the buffer:
   * the_hangover T50 `take off
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
          if (!game->refs.multiple[object])
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
      /*
       * Deliberate deviation: a held namesake whose Prefix the line rules
       * out (`take old pin`, the new red pin in hand) does not speak; see
       * lib_co_names_prefix().
       */
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
 * takes()'s "and" arm (its mode set to 2 when the line has c("and") and no
 * c("all"): run390 45455D) pre-passes every object before taking anything.
 * A dynamic object the line names is a candidate when it lies loose in the
 * player's room, or -- 3.8 and 3.9 -- in or on something loose here or a
 * static present here; it either fits or sets the hands-full flag.  With no
 * candidate at all the summary answers and nothing else in takes() speaks:
 * 3.9 (454B08) says "<You> can't get either of them." when exactly two
 * non-static objects were named, else "... any of them."; run380 (43DCC1)
 * and run370 have the "any" form only.
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
 * runner_probes/put.run3[789]0.takeand.*).  Each Runner's pre-pass also skips
 * an object whose "get <Short>" pre-matches a task; not modelled.
 */
scr_bool
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
 * (p37TKA, run370x runner_probes/tka.run370.rtf cell 122).
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
          game->refs.objects[object] = TRUE;
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
 * candidate (run380 43D788-43E996, run370's twin).
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
 *             replay).
 *
 * so a worn object named first and a loose one after it print as one string,
 * "You are already wearing a hat!the stone." (run380x) / "You are already
 * wearing the hat!the stone." (run390x); a held one is silent: `take hat and
 * stone` with the hat in hand is "You pick up the stone.".  A tail that is
 * still the bare seed is "Please take objects from one place at a time."
 * (43E996; run370 has no such line), and the rewritten line then
 * reaches insides(), whose answer -- the take-from, or its nothing-inside
 * refusal -- replaces that one: `take nut and cup` with the nut in a held
 * open box is "You take a nut from the box.", while `take nut and key` with
 * the box on the floor stays at "Please take objects from one place at a
 * time." because the box is not held (3.8's slot test).
 *
 * Measured on p37TKA/p38TKA/p39TKA (runner_probes/tka.run3[789]0.*, cells
 * 3, 6, 9, 14, 17, 20, 23, 32, 40, 44, 168) and p37TKB/p38TKB/p39TKB
 * (runner_probes/tkb.run3[789]0.*, cells 3, 8, 31, 38, 44).  3.9's own loop
 * (454B08) prints the same strings with definite names.
 *
 * The loop's state is a lib_take_and_t: count() is the pre-pass, seed() the
 * opening message, visit() one named object -- held(), worn(), loose() or
 * in_parent() -- and from_parent() the insides() tail.
 */
scr_bool
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
 * lib_move_named_whole_line_pre400()
 *
 * The noun half of "a library verb is matched anywhere in the line".  Below
 * 4.0 neither takes() nor drops() parses the words after its verb: each one
 * walks the object table in index order and asks co() whether the object's
 * Short or an Alias stands ANYWHERE in the typed line -- takes() with mode 1
 * (run390 454E1D) and drops() with mode 2 (run390 4458CF, and the same walk
 * in run380 and run370).  So a word the line holds that names nothing costs
 * nothing: `blorp take coin` is "You pick up the coin." and
 * `blorp drop coin` "You drop the coin." in all three older Runners
 * (p37REW/p38REW/p39REW, make_rewriteprobe.py --
 * runner_probes/rew.run3[789]0.casc.*), and so are `take zzz hat`
 * and `drop zzz hat` at 3.9 (p39WHAT, runner_probes/what.run390.what2.txt,
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
 * <X>!" (run390 445CE3) for a namesake that is not held --
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
  game->refs.multiple[found] = TRUE;
  *references = 1;
  return TRUE;
}
