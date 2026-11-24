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
 * Version and perspective helpers, response selection, clauses and list
 * printing.
 */

#include "sclibrar.h"


/* Trace flag, set before running. */
scr_bool lib_trace = FALSE;


/*
 * lib_set_admin()
 *
 * Mark a built-in meta-command as an administrative turn -- but only for
 * 3.9/4.0 games.  Administrative turns exist only from 3.9: run390
 * generaltasks (460D6C) sets its flag 468219 for
 * history/score/count/information/end/turns, and its end-of-turn
 * characters()+events() is guarded by that flag.  run380 generaltasks
 * (44349C) and run370 (43B5xx) have no such flag: their tail ticks
 * characters()+events() after EVERY command that left output, unless the
 * game has ended; only opensave() (save/restore/restart) and quit (unloads
 * the form) bypass it.  Measured in run380 haunt.taf: the `score` at turn 83
 * is followed by the Weather event's finish text and the grandfather-clock
 * line, which treating it as an administrative turn would suppress.  The 3.8
 * turn counter (44F138, 441A21) likewise increments on every command, so the
 * pre-3.9 non-admin path is right on both counts.  Callers are the
 * meta-commands 3.8 recognises and answers (score, turns, count, hint, help,
 * about, clear, history, where); verbs 3.8 does not know at all keep their
 * 4.0 behaviour.
 */
void
lib_set_admin (scr_gameref_t game)
{
  game->turn.is_admin = prop_get_taf_version (gs_get_bundle (game))
                   >= TAF_VERSION_390;
}


/*
 * lib_matcher_requires_seen()
 *
 * TRUE if the parser's object matcher should reject objects the player
 * hasn't seen yet.  The 3.9 and 4.0 Runners both gate every name match on
 * the object's seen flag (run390 co() tests the flag alongside obhere();
 * run400's matcher requires it in every match mode), so an object inside
 * an unlisted container "doesn't exist" until a room description, contents
 * listing, or examination reveals it.  The 3.8 Runner's co()/obhere() have
 * no such test.
 *
 * Pinned in the decompiles 2026-08-24.  The flag is at a different struct
 * offset in each Runner -- 40 in run370 and run380, 44 in run390, 48 in
 * run400 -- and co() grew from 50 lines (run370 4261B4) and 75 (run380
 * 42DE60) to 408 (run390 43B6BC) and 530 (run400 46486C).  Only the last
 * two read it, in each case after obhere() has failed.  Before 3.9 exactly
 * two routines consult the flag, and neither is the matcher: whereis()
 * (run370 430320, run380 437954), and therest()'s "With what?" check on the
 * second noun of <verb> X with Y (run370 43EAA8, run380 4455B4).
 */
scr_bool
lib_matcher_requires_seen (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390;
}


/*
 * lib_get_death_message()
 *
 * The sentence a Runner prints when the player dies, either from an EndGame
 * task action or from losing a Battle System fight.
 *
 * The pre-4.0 Runners build it around the perspective, "I'm afraid " & Ary(5)
 * & " " & Ary(4) & " dead!"; 4.0 replaced the whole thing with one fixed
 * literal and no longer varies it.  In run390 that assembly appears twice, in
 * the EndGame printer at 0003F5C8 and again in Form1.chardohit at 00042B14, so
 * the battle death is worded the same way as the task death.  run400 holds
 * "I'm afraid you are dead!" as a single literal in General.Sub_22_70
 * (000523DC), and Battles.Sub_12_1 calls that same sub (0004AE95), so 4.0 says
 * "you are" even in the first person.  A UTF-16LE census agrees: the assembled
 * pair is in all four Runners, the finished sentence only in run400.
 *
 * Measured live on the same EndGame with Perspective flipped: run370 on
 * castle.taf (its "south" death task rewired to a trivially runnable "zdie")
 * and run380 on wrecked.taf (task 42 rewired the same way) and run390 on the
 * synthetic p39end probe all answer "I'm afraid I am dead!" at Perspective 0
 * and "I'm afraid you are dead!" at Perspective 1.  Perspective 2 answers like
 * 1 pre-4.0, which lib_get_perspective() already folds in.
 *
 * The status line stays "You are dead!" throughout -- that is a separate fixed
 * literal, present unchanged in all four Runners.  See the standing
 * divergence table in test/adrift4/notes/RUNNER_TESTS.md.
 */
const scr_char *
lib_get_death_message (scr_gameref_t game)
{
  if (!lib_is_version_400 (game)
      && lib_get_perspective (game) == LIB_FIRST_PERSON)
    return "I'm afraid I am dead!";

  return "I'm afraid you are dead!";
}


/*
 * lib_select_response()
 * lib_select_plurality()
 *
 * Convenience functions for multiple handlers.  Returns the appropriate
 * response string for a game, based on perspective or object plurality.
 *
 * The third-person strings are NOT the second-person ones conjugated.  Every
 * Runner keeps one literal per message and prefixes it with a slot from a
 * seven-element pronoun array, filled at run400 48F60C-48F798: for the third
 * person Ary(0) and Ary(2) are the player's name, Ary(1) and Ary(3) the name
 * plus "'s", Ary(4) "is", Ary(5) "he" or "she" by Globals/PlayerGender, and
 * Ary(6) "s".  The verb lives in the literal, in its second-person form, and
 * only ONE message in the whole library appends Ary(6): the player's own
 * movement, " move" & Ary(6) & " north." at 474C3C and its eleven siblings.
 * So 4.0 really does print "%player% take the acorn." and "%player% sing a
 * little song.", and games work around it -- herrdoktor.taf ships the ALRs
 * "[The good doctor take] -> [The good doctor takes]" and "[doctor eat] ->
 * [doctor eats]", which turned SCARE's already-conjugated "takes" into
 * "takess".  Measured live against run400 on Main Course.taf
 * (Perspective 2, no ALRs to confound it): listen, sing, dance, sit down,
 * stand up and lie down all come back bare.
 *
 * Where a message reads Ary(5) rather than Ary(0) the third-person string
 * carries the internal %player_pronoun% token instead (see scvars.cpp);
 * pre-4.0 never reaches either, because lib_get_perspective() clamps.
 *
 * Scarier deliberately does not print the Runner's "%player% take the acorn."
 * (deviation policy: a subject-verb disagreement is a display accident).  The
 * source keeps the Runner's bare literals, and lib_select_response() hands
 * them to lib_conjugate_third_person(), which conjugates the verb -- except
 * where the game ships an ALR that already rewrites the bare verb, as
 * herrdoktor, iqsfot and Albert is Lost do ("Irvine open" -> "Irvine
 * uncloses"): there the bare text is what the author's ALR expects, and
 * conjugating it first gave SCARE's old "takess" and "unclosess".
 */

/*
 * lib_conjugate_verb()
 *
 * The third person singular of a bare second-person verb word, or an empty
 * string when the word is not one to conjugate (a modal, or already third
 * person).
 */
static std::string
lib_conjugate_verb (const std::string &word)
{
  static const scr_char *const KEEP[] = {
    "is", "was", "has", "does", "can", "can't", "cannot", "could", "couldn't",
    "must", "might", "may", "will", "won't", "would", "wouldn't", "should",
    "shouldn't", "isn't", "wasn't", "hasn't", "doesn't", "already", "not",
    nullptr
  };
  static const scr_char *const IRREGULAR[][2] = {
    {"don't", "doesn't"}, {"haven't", "hasn't"}, {"aren't", "isn't"},
    {"are", "is"}, {"have", "has"}, {"do", "does"}, {"go", "goes"},
    {nullptr, nullptr}
  };
  scr_int index_;
  const size_t length = word.length ();

  if (length == 0)
    return std::string ();
  for (index_ = 0; KEEP[index_]; index_++)
    {
      if (word == KEEP[index_])
        return std::string ();
    }
  for (index_ = 0; IRREGULAR[index_][0]; index_++)
    {
      if (word == IRREGULAR[index_][0])
        return IRREGULAR[index_][1];
    }
  if (word[length - 1] == 's' || word.find ('\'') != std::string::npos)
    return std::string ();

  if (word[length - 1] == 'x' || word[length - 1] == 'z'
      || word[length - 1] == 'o'
      || (length > 1 && (word.compare (length - 2, 2, "sh") == 0
                         || word.compare (length - 2, 2, "ch") == 0)))
    return word + "es";
  if (length > 1 && word[length - 1] == 'y'
      && !strchr ("aeiou", word[length - 2]))
    return word.substr (0, length - 1) + "ies";
  return word + "s";
}

/*
 * lib_conjugate_third_person()
 *
 * Conjugate the verb after a leading "%player% " (or the "'ve" of a leading
 * "%player%'ve") in a third-person library literal; see the comment above.
 * Returns the literal itself when there is nothing to change.
 */
static const scr_char *
lib_conjugate_third_person (scr_gameref_t game, const scr_char *literal)
{
  static const scr_char PLAYER[] = "%player%";
  enum { RING_SIZE = 16 };
  static std::string ring[RING_SIZE];
  static scr_int ring_next = 0;
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const size_t prefix = sizeof (PLAYER) - 1;
  const scr_char *rest, *playername;
  std::string word, conjugated, bare;
  size_t word_length, name_length;

  if (!literal || strncmp (literal, PLAYER, prefix) != 0)
    return literal;
  rest = literal + prefix;

  if (strncmp (rest, "'ve ", 4) == 0)
    {
      /* "%player%'ve already got " -> "%player% has already got ". */
      word_length = 3;
      conjugated = " has";
    }
  else
    {
      if (rest[0] != ' ')
        return literal;
      /*
       * The verb is one of the library's own English words: ASCII, and
       * not the locale's alphabet, is the right test here.
       */
      for (word_length = 1;
           rest[word_length] && ((rest[word_length] >= 'a'
                                  && rest[word_length] <= 'z')
                                 || rest[word_length] == '\'');
           word_length++)
        ;
      word.assign (rest + 1, word_length - 1);
      conjugated = lib_conjugate_verb (word);
      if (conjugated.empty ())
        return literal;
      conjugated = " " + conjugated;
    }

  /* Leave the bare verb to a game ALR written to rewrite it. */
  playername = prop_get_global_string (bundle, "PlayerName");
  if (scr_strempty (playername))
    playername = "Anonymous";
  name_length = strlen (playername);
  bare = std::string (playername) + rest;
  if (pf_alr_rewrites_span (bundle, bare.c_str (),
                            rest[0] == ' ' ? name_length + 1 : name_length,
                            name_length + word_length))
    return literal;

  ring_next = (ring_next + 1) % RING_SIZE;
  ring[ring_next] = std::string (PLAYER) + conjugated + (rest + word_length);
  return ring[ring_next].c_str ();
}

const scr_char *
lib_select_response (scr_gameref_t game,
                     const scr_char *second_person,
                     const scr_char *first_person,
                     const scr_char *third_person)
{
  scr_int perspective;
  const scr_char *response;

  /* Return the response appropriate for Perspective. */
  perspective = lib_get_perspective (game);
  switch (perspective)
    {
    case LIB_FIRST_PERSON:
      response = first_person;
      break;
    case LIB_SECOND_PERSON:
      response = second_person;
      break;
    case LIB_THIRD_PERSON:
      response = lib_conjugate_third_person (game, third_person);
      break;
    default:
      scr_error ("lib_select_response:"
                " unknown perspective, %ld\n", perspective);
      response = second_person;
      break;
    }

  return response;
}

const scr_char *
lib_select_plurality (scr_gameref_t game, scr_int object,
                      const scr_char *singular, const scr_char *plural)
{
  return obj_appears_plural (game, object) ? plural : singular;
}

/*
 * lib_select_list_plurality()
 *
 * The verb that agrees with a gathered list: plural for two or more
 * objects, else by the one object's own plurality.  The contents listings
 * ("Inside the desk are ...", "On the table are ...") use this where the
 * Runner prints a literal " is " whatever the count; Scarier deliberately
 * keeps the agreement (see test/adrift4/notes/WINE-TRANSCRIPTS.md,
 * deliberate deviations).
 */
const scr_char *
lib_select_list_plurality (scr_gameref_t game, scr_int container,
                           const lib_list_t &list,
                           const scr_char *singular, const scr_char *plural)
{
  if (lib_alrs_see_list_verb (game, container))
    return singular;
  if (list.size () > 1)
    return plural;
  return list.empty () ? singular
                       : lib_select_plurality (game, list[0], singular, plural);
}


/*
 * lib_print_wrapped_object()
 * lib_print_wrapped_npc()
 * lib_print_response_object()
 * lib_print_response_npc()
 *
 * Print an object or NPC name framed by a prefix and a suffix, the response
 * variants selecting the prefix by Perspective.  These fold a three-call
 * sequence repeated throughout the module.
 */
void
lib_print_wrapped_object (scr_gameref_t game, const scr_char *prefix,
                          scr_int object, const scr_char *suffix)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, prefix);
  lib_print_object_np (game, object);
  pf_buffer_string (filter, suffix);
}

void
lib_print_wrapped_npc (scr_gameref_t game, const scr_char *prefix,
                       scr_int npc, const scr_char *suffix)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, prefix);
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter, suffix);
}

void
lib_print_response_object (scr_gameref_t game,
                           const scr_char *second_person,
                           const scr_char *first_person,
                           const scr_char *third_person,
                           scr_int object, const scr_char *suffix)
{
  lib_print_wrapped_object (game,
                            lib_select_response (game, second_person,
                                                 first_person, third_person),
                            object, suffix);
}

void
lib_print_response_npc (scr_gameref_t game,
                        const scr_char *second_person,
                        const scr_char *first_person,
                        const scr_char *third_person,
                        scr_int npc, const scr_char *suffix)
{
  lib_print_wrapped_npc (game,
                         lib_select_response (game, second_person,
                                              first_person, third_person),
                         npc, suffix);
}

/*
 * lib_print_message()
 *
 * Buffer a fixed response message and indicate a handled command; the whole
 * action of the many chit-chat handlers near the end of the module.
 */
scr_bool
lib_print_message (scr_gameref_t game, const scr_char *message)
{
  pf_buffer_string (gs_get_filter (game), message);
  return TRUE;
}

scr_bool
lib_print_response_message (scr_gameref_t game,
                            const scr_char *second_person,
                            const scr_char *first_person,
                            const scr_char *third_person)
{
  return lib_print_message (game,
                            lib_select_response (game, second_person,
                                                 first_person, third_person));
}


/*
 * lib_new_clause()
 * lib_print_clause()
 *
 * Start a new clause in the turn's output.  Where something has already been
 * printed this turn, the clause is set off from it by two spaces; the rest is
 * either a fresh sentence, or the phrase appropriate to the perspective.
 *
 * What came before may be a game task's text rather than the library's own:
 * `drop all` offers every held object to the tasks first, and House (4.0)
 * answers the desiccated herb with task 359's "The thyme has aged and
 * dried.  As you watch it dissipates." before the library lists the rest.
 * That text was terminated by pf_buffer_paragraph_line(), so the "  " landed
 * at the start of the next line ("\n  You drop the note, ...").  The Runner
 * keeps the turn's text in one string and appends the library's line to it,
 * so the separator first takes back a terminator of our own; an author's
 * own trailing break is left alone.  The two spaces stay unconditional: the
 * Runner stacks them onto an author's trailing pair (trabula "shabby.    The
 * shabby hut is locked.", baroo, xfiles goldens), which pf_buffer_pspace()'s
 * double-space guard would fold.
 */
static void
lib_clause_separator (scr_filterref_t filter)
{
  pf_undo_auto_break (filter);
  pf_buffer_string (filter, "  ");
}

void
lib_new_clause (scr_gameref_t game, scr_bool has_printed)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (has_printed)
    lib_clause_separator (filter);
  pf_new_sentence (filter);
}

void
lib_print_clause (scr_gameref_t game, scr_bool has_printed,
                  const scr_char *second_person,
                  const scr_char *first_person,
                  const scr_char *third_person)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (has_printed)
    lib_clause_separator (filter);
  pf_buffer_string (filter,
                    lib_select_response (game, second_person,
                                         first_person, third_person));
}


/*
 * lib_collect_children()
 * lib_reveal_children()
 *
 * Gather, in index order, the objects at the given position whose parent is
 * the given object or NPC: the contents of a container, the load on a
 * surface, or a character's held or worn possessions.  The reveal form also
 * marks each one seen, which is what listing them does in the Runner, and
 * returns how many there were.
 */
void
lib_collect_children (scr_gameref_t game, scr_int parent,
                      scr_int position, lib_list_t &list)
{
  scr_int object;

  list.clear ();
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (gs_object_position (game, object) == position
          && gs_object_parent (game, object) == parent)
        list.push_back (object);
    }
}

scr_int
lib_reveal_children (scr_gameref_t game, scr_int parent,
                     scr_int position, lib_list_t &list)
{
  lib_collect_children (game, parent, position, list);
  for (const scr_int object : list)
    gs_set_object_seen (game, object, TRUE);
  return (scr_int) list.size ();
}


/*
 * lib_count_npc_references()
 *
 * Count the NPCs the command referenced, leaving the last of them in *npc,
 * or -1 if there was none.
 */
scr_int
lib_count_npc_references (scr_gameref_t game, scr_int *npc)
{
  scr_int index_, count;

  count = 0;
  *npc = -1;
  GS_FOR_EACH_NPC (game, index_)
    {
      if (game->refs.npcs[index_])
        {
          count++;
          *npc = index_;
        }
    }
  return count;
}


/*
 * lib_print_list()
 * lib_print_name_list()
 *
 * Print a gathered list as "a, b, c and d", the last separator being the
 * given conjunction -- " and " for most lists, " or " where the game is
 * offering a choice.  Callers collect into a lib_list_t first rather than
 * printing as they iterate, so that the clause introducing the list can be
 * chosen from the finished list rather than reconstructed on the fly.
 */
void
lib_print_list (scr_gameref_t game, const lib_list_t &list,
                lib_print_item_t print_item, const scr_char *conjunction)
{
  const scr_filterref_t filter = gs_get_filter (game);
  size_t index_;

  for (index_ = 0; index_ < list.size (); index_++)
    {
      if (index_ > 0)
        pf_buffer_string (filter,
                          index_ == list.size () - 1 ? conjunction : ", ");
      print_item (game, list[index_]);
    }
}

void
lib_print_name_list (scr_gameref_t game, const lib_list_t &list,
                     const scr_char *const *names,
                     const scr_char *conjunction)
{
  const scr_filterref_t filter = gs_get_filter (game);
  size_t index_;

  for (index_ = 0; index_ < list.size (); index_++)
    {
      if (index_ > 0)
        pf_buffer_string (filter,
                          index_ == list.size () - 1 ? conjunction : ", ");
      pf_buffer_string (filter, names[list[index_]]);
    }
}


/*
 * lib_print_object_list()
 *
 * Print "<clause>a, b and c." for a gathered list of objects -- the shape
 * every "You can't wear ..."-style report about a batch of objects takes.
 * Prints nothing for an empty list.  Returns TRUE if it printed anything,
 * so that callers can fold the result into their has_printed.
 *
 * Names are normalized ("the ...") unless the caller asks for lib_print_object
 * instead; the pre-3.9 wear report is the one place that does.
 */
scr_bool
lib_print_object_list (scr_gameref_t game, scr_bool has_printed,
                       const lib_list_t &list,
                       const scr_char *conjunction, scr_char terminator,
                       const scr_char *second_person,
                       const scr_char *first_person,
                       const scr_char *third_person,
                       lib_print_item_t print_item)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (list.empty ())
    return FALSE;

  lib_print_clause (game, has_printed,
                    second_person, first_person, third_person);
  lib_print_list (game, list, print_item, conjunction);
  /* '\0' leaves the sentence open (no caller does so at present). */
  if (terminator != '\0')
    pf_buffer_character (filter, terminator);
  return TRUE;
}


/*
 * lib_get_npc_inroom_text()
 *
 * Returns the inroom description to be use for an NPC; if the NPC has
 * gone walkabout and offers a changed description, return that; otherwise
 * return the standard inroom text.
 */
const scr_char *
lib_get_npc_inroom_text (scr_gameref_t game, scr_int npc)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int walk_count, walk;
  const scr_char *inroomtext;

  /* Get the count of NPC walks. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Walks";
  walk_count = prop_get_child_count (bundle, "I<-sis", vt_key);

  /*
   * Check for any active walk with a description, return if found.
   *
   * "Active" here is the walk's task state -- StartTask complete, StoppingTask
   * not -- and nothing to do with how much counter the walk has left; the
   * Runner's room lister looks at those two fields and the ChangedDesc, and at
   * no counter (run400 viewroom Proc_19_63_472CA4, the NPC loop, re-read
   * from the P-code 2026-08-24; run390 28995; 3.7 and 3.8 have no
   * ChangedDesc at all, and their walk schema leaves it empty).  So a walk
   * that has run its course goes on describing the NPC for the rest of the
   * game.  In "The Fun House" (4.0) that is the difference between "The tall
   * man is pleasant." and dropping him into the room's "... are here."
   * sentence from turn 16 on.
   *
   * The Runner scans upwards and keeps overwriting, so the highest-numbered
   * walk with a description wins; scanning down and returning is the same
   * pick.
   */
  for (walk = walk_count - 1; walk >= 0; walk--)
    {
      if (npc_walk_is_enabled (game, npc, walk))
        {
          const scr_char *changeddesc;

          /* Get and check any walk active description. */
          vt_key[3].integer = walk;
          vt_key[4].string = "ChangedDesc";
          changeddesc = prop_get_string (bundle, "S<-sisis", vt_key);
          if (!scr_strempty (changeddesc))
            return changeddesc;
        }
    }

  /* Return the standard inroom text. */
  vt_key[2].string = "InRoomText";
  inroomtext = prop_get_string (bundle, "S<-sis", vt_key);
  return inroomtext;
}


/*
 * lib_npc_text_is_default()
 *
 * TRUE if an NPC's in-room text is one the room lister folds into its joined
 * "X, Y and Z are here." sentence -- that is, one ending in " is here.".  The
 * Runner's "#" has already become such a text by the time its lister runs;
 * see the note in lib_print_room_contents().  Exact and case-sensitive, as
 * measured: "Golf is here!" and "Hotel IS HERE." are not folded.
 */

scr_bool
lib_npc_text_is_default (const scr_char *description)
{
  static const scr_char *const SUFFIX = " is here.";

  scr_int length = strlen (description);

  return length > LIB_NPC_HERE_LENGTH
         && strcmp (description + length - LIB_NPC_HERE_LENGTH, SUFFIX) == 0;
}


/*
 * lib_inroomdesc_is_absent()
 *
 * TRUE when an object has no in-room description at all.  The Runner's test
 * is an exact `InRoomDesc = ""` (run400 @00472589 for the print, @00449B0C
 * inside the helper that decides the "Also here is" list), NOT a
 * whitespace-trimmed one, so an in-room description of a single SPACE counts
 * as present: the Runner prints it -- invisibly -- and, because it printed
 * it, leaves the object out of the fallback list.  Authors use exactly that
 * to suppress an object the room's own long text already mentions.
 *
 * topaz.taf is the corpus case.  Its sword, object 5, sits in the Darkness
 * where the room text already says "The sword lies on the floor, a soft amber
 * glow shining from the jewel in its hilt", and its InRoomDesc is " ".
 * scr_strempty() trims, so Scarier read that as no description and added "Also
 * here is a Topaz." to a room that had just described the sword (measured in
 * run400 under Wine, runner_transcripts/topaz.txt turn 11).
 */
scr_bool
lib_inroomdesc_is_absent (const scr_char *inroomdesc)
{
  return inroomdesc == nullptr || inroomdesc[0] == NUL;
}
