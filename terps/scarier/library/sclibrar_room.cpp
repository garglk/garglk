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
 * Room contents, room descriptions and exits.
 *
 * See sclibrar.h for what the library files share.
 */

#include "sclibrar.h"


/*
 * lib_alrs_see_gap()
 *
 * TRUE if any of the game's ALRs could match across the Runner's literal
 * two-space gap after a line break: an Original that opens with a space, or
 * one that spans a break.  perspectives' ' Also here is a gun. ' is one --
 * it only fires on the Runner's "<br><br>  Also here is a gun.  " -- and
 * which ALRs fire is game behaviour, not text shape.
 */
static scr_bool
lib_alrs_see_gap (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int alr_count, alr;

  vt_key[0].string = "ALRs";
  alr_count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (alr = 0; alr < alr_count; alr++)
    {
      const scr_char *original, *cursor;

      vt_key[1].integer = alr;
      vt_key[2].string = "Original";
      original = prop_get_string (bundle, "S<-sis", vt_key);
      if (original[0] == ' ' || strchr (original, '\n'))
        return TRUE;
      for (cursor = strchr (original, '<'); cursor;
           cursor = strchr (cursor + 1, '<'))
        {
          if (scr_strncasecmp (cursor, "<br>", 4) == 0)
            return TRUE;
        }
    }
  return FALSE;
}


/*
 * lib_buffer_literal_gap()
 *
 * The Runner's room lister hard-codes two spaces ahead of "Also here", the
 * joined "... is here." sentence and the exits sentence -- literals, not
 * pspace() -- so after a block that ends in a line break, or a bare
 * heading, its line opens with two spaces.  Scarier deliberately keeps the
 * join but not that indent (deviation policy): the gap is left out where
 * the buffer is empty or already stands at the start of a line, unless an author ALR
 * could be matching on it (lib_alrs_see_gap), which keeps the Runner's text
 * so the same ALRs fire.  The exits sentence passes guarded FALSE: no corpus
 * ALR matches on its gap, and les_feux's leading-space ALRs would otherwise
 * keep every French exits line indented.
 */
static void
lib_buffer_literal_gap (scr_gameref_t game, scr_bool guarded)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *buffered = pf_get_buffer (filter);

  if ((buffered && buffered[0] != '\0'
       && !pf_text_ends_with_break (buffered))
      || (guarded && lib_alrs_see_gap (game)))
    pf_buffer_string (filter, "  ");
}


/*
 * lib_room_contents_inroomdescs()
 * lib_room_contents_also_here()
 * lib_room_contents_npc_sentence()
 * lib_room_contents_npc_texts()
 * lib_room_contents_terminate()
 *
 * The parts of lib_print_room_contents(), in the order it prints them:
 * the objects showing their in-room text, the "Also here" list of the
 * rest, the joined "<names> are here." sentence, the characters with
 * in-room text of their own, and the room block's terminating break.
 */
static void
lib_room_contents_inroomdescs (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int object;

  /*
   * List all objects that show their initial description.
   *
   * The Runner's room lister (run400 @00472515, re-checked by its helper
   * @00449B6C) prints an object's InRoomDesc only when ListFlag matches the
   * object's kind: a dynamic object needs ListFlag clear, while a STATIC
   * object needs ListFlag ("specifically list") SET.  A static object with
   * an InRoomDesc but no ListFlag prints nothing -- authors use that for
   * text mirrored in the room description itself (Goldilocks' hall trapdoor,
   * whose "[TRAP=...]" sentence is already part of the room's long text).
   * Pre-4.0 games have no InRoomDesc property at all, so the gate only ever
   * bites on version 4.0 games.
   */
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (obj_directly_in_room (game, object, room))
        {
          const scr_char *inroomdesc;
          scr_bool listflag;

          vt_key[0].string = "Objects";
          vt_key[1].integer = object;
          vt_key[2].string = "ListFlag";
          listflag = prop_get_boolean (bundle, "B<-sis", vt_key);
          if (listflag != obj_is_static (game, object))
            continue;

          /* Find and print in room description. */
          vt_key[2].string = "InRoomDesc";
          inroomdesc = prop_get_string (bundle, "S<-sis", vt_key);
          if (!obj_shows_initial_description (game, object, room,
                                              lib_inroomdesc_is_absent
                                                (inroomdesc)))
            continue;
          if (!lib_inroomdesc_is_absent (inroomdesc))
            {
              pf_buffer_join (filter, inroomdesc);
            }
        }
    }
}


static void
lib_room_contents_also_here (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int object;
  lib_list_t list;

  /*
   * List dynamic objects directly located in the room, and not already listed
   * above since they lack, or suppress, an in room description.
   *
   * If an object sets ListFlag, then if dynamic it's suppressed from the list
   * where it would normally be included, but if static it's included where it
   * would normally be excluded.
   */
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (obj_directly_in_room (game, object, room))
        {
          const scr_char *inroomdesc;

          vt_key[0].string = "Objects";
          vt_key[1].integer = object;
          vt_key[2].string = "InRoomDesc";
          inroomdesc = prop_get_string (bundle, "S<-sis", vt_key);

          if (!obj_shows_initial_description (game, object, room,
                                              lib_inroomdesc_is_absent
                                                (inroomdesc)))
            {
              scr_bool listflag;

              vt_key[2].string = "ListFlag";
              listflag = prop_get_boolean (bundle, "B<-sis", vt_key);

              if (listflag == obj_is_static (game, object))
                list.push_back (object);
            }
        }
    }
  if (!list.empty ())
    {
      /*
       * The two spaces are the Runner's own, hard-coded into the literal
       * "  Also here" it appends (run400 @00472696) -- not a pspace() call,
       * so they go in even after a description that already ended in a
       * break.  Take back our section terminator first; it stands where the
       * Runner's string simply carried on.
       */
      pf_undo_auto_break (filter);
      lib_buffer_literal_gap (game, TRUE);
      pf_buffer_string (filter,
                        lib_select_plurality (game, list[0],
                                              "Also here is ",
                                              "Also here are "));
      lib_print_list (game, list, lib_print_object, " and ");
      pf_buffer_string (filter, ".");
    }
}


static void
lib_room_contents_npc_sentence (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  std::vector<std::string> joined;
  scr_int npc;

  /*
   * List the NPCs in the room.  A "#" in-room text asks for the default
   * "<name> is here.", and the Runner splits the room's characters into the
   * ones saying exactly that -- joined into one sentence -- and the ones with
   * something of their own to say.
   *
   * Two things about that split are not what they look like, both measured
   * live in run400 (test/adrift4/notes/RUNNER_TESTS_TODO.md section 9).  The
   * "#" substitution happens in the *loader* (@00091EDF): the text simply
   * becomes "<name> is here." before the game starts, and by the time the room
   * lister (@00072944) runs there is no "#" left to test for.  What it tests
   * instead is the tail -- Right(text, 9) = " is here." -- so a character
   * whose text the author wrote out in full joins the sentence too,
   * contributing the text with those nine characters trimmed off rather than
   * its own name.  Probe NPCs "Delta" ("Delta is here.") and "Foxtrot" ("The
   * stranger is here.") come out as "Alpha, Charlie, Delta and The stranger
   * are here.", so it is the text that is trimmed and not the name that is
   * looked up.  The test is exact and case-sensitive: "Golf is here!" and
   * "Hotel IS HERE." both stay in the second group.
   *
   * And the joined sentence comes *first*, ahead of the characters with their
   * own text, which is the other half of what Scarier had backwards.
   */
  GS_FOR_EACH_NPC (game, npc)
    {
      const scr_char *description;

      if (!npc_in_room (game, npc, room))
        continue;

      description = lib_get_npc_inroom_text (game, npc);
      if (!scr_strcasecmp (description, "#"))
        {
          /*
           * The loader's "#" substitution is where the case is decided, and
           * it is a version split: run400 builds Proc_21_3_446BB4(Name) &
           * " is here." (@00491EF3), capitalising the first letter, while
           * run390 (@00466932), run380 (@00449463) and run370 (@0044071D)
           * append the raw Name.  Nothing downstream capitalises, so an
           * author's own " is here." text keeps its case in every version.
           * twilight (3.80) prints "a monkey is here.", goldilocks (4.0)
           * "My Fairy Godmother is here."  Scarier deliberately still opens
           * the sentence with a capital in every version (deviation
           * policy); see the pf_new_sentence() below.
           */
          std::string name = prop_get_indexed_string (bundle, "NPCs",
                                                      npc, "Name");
          if (lib_is_version_400 (game) && !name.empty ())
            name[0] = scr_toupper (name[0]);
          joined.push_back (name);
        }
      else
        {
          /*
           * Trim the suffix to get the name the Runner joins in.  Both the
           * test and the trim run on the text exactly as the author wrote
           * it: run400 @004729A7 asks Right(text, 9) = " is here." and
           * @004729FE takes Left(text, Len(text) - 9), and neither looks
           * past a leading break.  So a character whose in-room text opens
           * with "<br>" keeps that break, and it lands *after* the two
           * separator spaces rather than instead of them --
           * runner_transcripts/spooked.txt lines 120-122 show the room
           * description's trailing "  ", then the author's blank line, then
           * "Samuel, your scientist pal, is here."
           */
          if (lib_npc_text_is_default (description))
            {
              joined.push_back (std::string (description,
                                             strlen (description)
                                             - LIB_NPC_HERE_LENGTH));
            }
        }
    }

  if (!joined.empty ())
    {
      size_t index_;

      /*
       * Two spaces, hard-coded like the object list's (run400 @0047295B),
       * and again not a pspace() call -- the Runner puts them in whatever
       * the string already ends with.  Take back our own terminator first.
       */
      pf_undo_auto_break (filter);
      lib_buffer_literal_gap (game, TRUE);
      /* A sentence opening in lower case is a display accident. */
      pf_new_sentence (filter);
      for (index_ = 0; index_ < joined.size (); index_++)
        {
          if (index_ > 0)
            {
              pf_buffer_string (filter,
                                index_ == joined.size () - 1
                                ? " and " : ", ");
            }
          pf_buffer_string (filter, joined[index_].c_str ());
        }
      pf_buffer_string (filter,
                        joined.size () == 1 ? " is here" : " are here");
      pf_buffer_string (filter, ".");
    }
}


static void
lib_room_contents_npc_texts (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int npc;

  /* List NPCs directly in the room that have an in room description. */
  GS_FOR_EACH_NPC (game, npc)
    {
      if (npc_in_room (game, npc, room))
        {
          const scr_char *description;

          /*
           * Print any text not already folded into the sentence above.  The
           * test has to be the same one the collection loop made, on the same
           * raw text, or a character would either be listed twice or vanish.
           */
          description = lib_get_npc_inroom_text (game, npc);
          if (!scr_strempty (description)
              && scr_strcasecmp (description, "#")
              && !lib_npc_text_is_default (description))
            {
              /*
               * Authors typically begin a character's InRoomText with a line
               * break -- a literal newline or a "<br>" tag -- so that the
               * character appears on a line of its own.  The Runner keeps
               * that break and adds nothing of its own beyond pspace()'s
               * clause gap (run400 @00472B01 calls it, @00472B06 appends the
               * text verbatim), so the break the author wrote is the one that
               * does the spacing.  pf_buffer_join() is pspace(), plus taking
               * back our own section terminator, which the Runner's string
               * never had.
               */
              pf_buffer_join (filter, description);
            }
        }
    }
}


static void
lib_room_contents_terminate (scr_gameref_t game, size_t entry_length)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *buffered = pf_get_buffer (filter);

  /*
   * Terminate the room block, and record the newline as ours: everything
   * appended after it -- an event's LookText, a task's message, the next
   * turn's text -- joins onto the Runner's single room string, so whoever
   * comes next may take this break back again.
   */
  if (buffered && strlen (buffered) > entry_length
      && !pf_text_ends_with_break (buffered))
    {
      pf_buffer_character (filter, '\n');
      pf_note_trailing_auto_break (filter);
    }
}


/*
 * lib_print_room_contents()
 *
 * Print a list of the contents of a room.
 */
static void
lib_print_room_contents (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *entry_buffer = pf_get_buffer (filter);
  const size_t entry_length = entry_buffer ? strlen (entry_buffer) : 0;

  lib_room_contents_inroomdescs (game, room);
  lib_room_contents_also_here (game, room);
  lib_room_contents_npc_sentence (game, room);
  lib_room_contents_npc_texts (game, room);
  lib_room_contents_terminate (game, entry_length);
}


/*
 * lib_room_desc_long()
 * lib_room_desc_alts()
 * lib_room_desc_nothing_390()
 * lib_room_desc_event_looks()
 *
 * The parts of lib_print_room_description(), in the order it prints them:
 * the room's own Long unless the starting alt suppresses it, the alternates
 * from the starting one on (which also decide whether the objects show), 3.9's
 * substitute for a room nothing described, and -- after the contents, which
 * the caller prints -- the LookText of the events running there.
 */
static void
lib_room_desc_long (scr_gameref_t game, scr_int room, scr_int start,
                    scr_bool *is_described)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_bool is_suppressed;

  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Alts";

  /* Print the standard description unless a start alt indicates not. */
  if (start == -1)
    is_suppressed = FALSE;
  else
    {
      scr_int method;

      vt_key[3].integer = start;
      vt_key[4].string = "DisplayRoom";
      method = prop_get_integer (bundle, "I<-sisis", vt_key);

      is_suppressed = (method == 0);
    }
  if (!is_suppressed)
    {
      const scr_char *description;

      vt_key[0].string = "Rooms";
      vt_key[1].integer = room;
      vt_key[2].string = "Long";
      description = prop_get_string (bundle, "S<-sis", vt_key);
      if (!scr_strempty (description))
        {
          pf_buffer_string (filter, description);
          *is_described = TRUE;
        }
      else if (prop_get_taf_version (bundle) == TAF_VERSION_380)
        {
          /*
           * 3.8 substitutes "There is nothing of interest here." into an
           * empty Long at LOAD time (run380 447FEE), so the sentence prints
           * wherever the Long would -- even when an alt goes on to describe
           * the room.  Measured live 2026-08-31 on cave.taf (3.80): "By the
           * old shack" and "By the large tree" have empty Longs and a
           * LastDesc alt, and run380 prints "There is nothing of interest
           * here.  You are standing..." on every show.  3.9's render-time
           * guard, lib_room_desc_nothing_390(), fires only when nothing
           * else described the room.
           */
          pf_buffer_string (filter, "There is nothing of interest here.");
          *is_described = TRUE;
        }

      vt_key[2].string = "Res";
      res_handle_resource (game, "sis", vt_key);
    }
}

static void
lib_room_desc_alts (scr_gameref_t game, scr_int room, scr_int start,
                    scr_bool *is_described, scr_bool *showobjects)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int alt_count, alt;

  /* Get count of room alternates. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Alts";
  alt_count = prop_get_child_count (bundle, "I<-sis", vt_key);

  /*
   * Run forwards through all alts lower than our starting point, or all alts
   * if no starting point overrider found.
   */
  *showobjects = TRUE;
  /*
   * run400 viewroom (Proc_19_63_472CA4) reads HideObjects from EVERY alt
   * that holds (472263), not only from the starter on: homelessharry T11,
   * the Cardboard Box's always-true alt 0 hiding objects under the later
   * display alt, lists no "Toothless Willy is here."
   * (runner_transcripts/homelessharry.txt).
   */
  if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
    for (alt = 0; alt < alt_count; alt++)
      if (lib_use_room_alt (game, room, alt))
        {
          vt_key[3].integer = alt;
          vt_key[4].string = "HideObjects";
          if (prop_get_integer (bundle, "I<-sisis", vt_key) == 1)
            *showobjects = FALSE;
        }
  for (alt = (start != -1) ? start : 0; alt < alt_count; alt++)
    {
      /* Ignore all non-method-2 alts except for the starter. */
      if (alt != start)
        {
          scr_int method;

          vt_key[3].integer = alt;
          vt_key[4].string = "DisplayRoom";
          method = prop_get_integer (bundle, "I<-sisis", vt_key);

          if (method != 2)
            continue;
        }

      if (lib_use_room_alt (game, room, alt))
        {
          const scr_char *m1;
          scr_int hideobjects;

          vt_key[3].integer = alt;
          vt_key[4].string = "M1";
          m1 = prop_get_string (bundle, "S<-sisis", vt_key);
          if (!scr_strempty (m1))
            {
              if (*is_described)
                pf_buffer_string (filter, "  ");
              pf_buffer_string (filter, m1);
              *is_described = TRUE;
            }

          vt_key[4].string = "Res1";
          res_handle_resource (game, "sisis", vt_key);

          vt_key[4].string = "HideObjects";
          hideobjects = prop_get_integer (bundle, "I<-sisis", vt_key);
          if (hideobjects == 1)
            *showobjects = FALSE;
        }
      else
        {
          const scr_char *m2;

          vt_key[3].integer = alt;
          vt_key[4].string = "M2";
          m2 = prop_get_string (bundle, "S<-sisis", vt_key);
          if (!scr_strempty (m2))
            {
              if (*is_described)
                pf_buffer_string (filter, "  ");
              pf_buffer_string (filter, m2);
              *is_described = TRUE;
            }

          vt_key[4].string = "Res2";
          res_handle_resource (game, "sisis", vt_key);
        }
    }
}

static void
lib_room_desc_nothing_390 (scr_gameref_t game, scr_int room,
                           scr_bool *is_described)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];

  /*
   * A room that ends up with no description at all.
   *
   * 3.8 and 3.9 both supply one.  3.9 does it while rendering: viewroom()
   * appends "There is nothing of interest here." when the branch's own
   * alternative text is empty AND the room's Long is empty -- the same
   * guarded shape at all four of its exits (run390 4478CA/4479A5/447A3A/
   * 447ACF, each preceded by a `<alt string> <> vbNullString` branch that
   * jumps clean past it).  3.8 does it at LOAD time instead, substituting the
   * sentence into the empty Long itself (run380 447FEE) -- and cave.taf
   * (measured live 2026-08-31) proves the routes DIVERGE when an alt
   * describes the room: 3.8 prints the substitute in the Long's place AND
   * the alt after it, where this render-time guard stayed silent.  The 3.80
   * case therefore now lives up at the Long print; only 3.90 keeps this
   * guarded tail.  3.7 has no such string and leaves the
   * room blank; 4.0 dropped it again.
   *
   * Read the guard, not the literal: hanging the sentence off an empty Long
   * alone moves 16 of the 303 corpus goldens.  The guard needs BOTH halves:
   * the alt text empty AND the Long itself empty (`var_A4(4) = vbNullString`
   * at 4478C0, the room record's Long, not what got printed).  yeh.taf
   * (3.90) measured on run390x 2026-09-12: "Outside", "Woods", "Spooky
   * area." each have a Long and a start alt that suppresses it with empty
   * text, and the Runner prints the heading and the exits alone.
   *
   * Measured on p39EXAM.taf (3.90), runner_probes/exam.run390.txt and
   * runner_probes/exam.run390.held.txt: the Void Room has an empty Long, no
   * alts and no objects, and both `e` and `look` answer "There is nothing of
   * interest here.  You can only move west." -- the sentence joined to the
   * exits line with the ordinary two-space clause gap.  The 4.0 twin
   * p4EXAM.taf, runner_probes/exam.run400.txt, prints the exits alone.
   */
  if (!*is_described)
    {
      const scr_int version = prop_get_taf_version (bundle);

      vt_key[0].string = "Rooms";
      vt_key[1].integer = room;
      vt_key[2].string = "Long";
      if (version == TAF_VERSION_390
          && scr_strempty (prop_get_string (bundle, "S<-sis", vt_key)))
        {
          pf_buffer_string (filter, "There is nothing of interest here.");
          *is_described = TRUE;
        }
    }
}

static void
lib_room_desc_event_looks (scr_gameref_t game, scr_int room,
                           scr_bool is_described)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  scr_bool looked;
  scr_int event;

  /*
   * Finally, print any relevant event look text.  The runner appends it dead
   * last in the room block, after the object list and the character lines,
   * run on with its two-space separator (probed live in both runners,
   * 2026-08-02).  Join onto the description block only if this call printed
   * one, so an event's text can't migrate up onto the room name line.
   *
   * The visibility test asks about `room`, the room being described, not
   * about the player -- run400's viewroom indexes the event's room list with
   * its own room argument (loc_472B53) and the two differ whenever a task
   * with ShowRoomDesc displays a room before its actions have moved anyone.
   * See evt_can_see_event_in_room().
   */
  looked = FALSE;
  GS_FOR_EACH_EVENT (game, event)
    {
      if (gs_event_state (game, event) == ES_RUNNING
          && evt_can_see_event_in_room (game, event, room))
        {
          const scr_char *looktext;

          vt_key[0].string = "Events";
          vt_key[1].integer = event;
          vt_key[2].string = "LookText";
          looktext = prop_get_string (bundle, "S<-sis", vt_key);
          if (!scr_strempty (looktext))
            {
              if (is_described || looked)
                pf_buffer_join (filter, looktext);
              else
                pf_buffer_string (filter, looktext);
              looked = TRUE;
            }

          vt_key[2].string = "Res";
          vt_key[3].integer = 1;
          res_handle_resource (game, "sisi", vt_key);
        }
    }
  if (looked)
    {
      const scr_char *buffered = pf_get_buffer (filter);

      if (!(buffered && pf_text_ends_with_break (buffered)))
        pf_buffer_answer_break (filter);
    }
}


/*
 * lib_print_room_description()
 *
 * Print out the long description for a given room.
 */
void
lib_print_room_description (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool showobjects, is_described;
  scr_int start;

  /* Start with no description, and get our starting point in the alts list. */
  is_described = FALSE;
  start = lib_find_starting_alt (game, room);

  lib_room_desc_long (game, room, start, &is_described);
  lib_room_desc_alts (game, room, start, &is_described, &showobjects);
  lib_room_desc_nothing_390 (game, room, &is_described);

  /*
   * Terminate the description block with a single line break.  Many ADRIFT
   * room descriptions already end with a trailing "<br>" of their own; if we
   * unconditionally added a newline here it would double up with that break,
   * leaving a stray blank line before the contents.  Add the break only when
   * the buffer does not already end with one.
   *
   * Record it as ours.  The Runner has no terminator here at all -- viewroom
   * keeps concatenating onto the one room string, and the contents list joins
   * straight on with the two spaces of its own literal "  Also here"
   * (@00472696) -- so lib_print_room_contents() takes this newline back
   * again the moment it has anything to say.  What is left standing is the
   * room block's terminator for the case where it has nothing.
   */
  if (is_described)
    {
      const scr_char *buffered = pf_get_buffer (filter);

      if (!(buffered && pf_text_ends_with_break (buffered)))
        pf_buffer_character (filter, '\n');
      pf_note_trailing_auto_break (filter);
    }

  /*
   * Reveal what a full room description reveals.
   *
   * Pre-4.0 this is gated on showobjects, because the Runner's marking loops
   * sit *below* its HideObjects jump, not above it.  run390 viewroom takes
   * the dark branch at 4477AD, prints the alt text, and at 4477F9 tests the
   * room's own HideObjects field: set, it jumps to 448124 -- past 447B0A,
   * where the static sweep stamps the seen byte (447B9C), and past the "Also
   * here" loop at 447BB9, whose body stamps each dynamic it lists (447BFC).
   * So a HideObjects alt suppresses the knowledge as well as the sentence,
   * and the objects stay unreferenceable: co() ANDs the seen byte into every
   * match.
   *
   * That is the whole of ADRIFT 4's darkness.  Measured on p39DARK.taf
   * (3.90): entering the Dark Cave unlit
   * (runner_probes/dark.run390.unseen.txt) leaves `x stone`, `read stone` and
   * `x box` at the unmatched-noun answer "You can't see that very clearly.",
   * `take stone` at "Take what?" and `get all from box` at "You can't get
   * anything from that." -- the container is not reachable either.
   * runner_probes/dark.run390.seen.txt then walks in with the torch, so the
   * same objects are stamped, drops the torch and returns: now `take stone`
   * succeeds and `x stone` answers the named form, "You can't see the stone
   * very clearly."  Being seen is permanent; being lit is not.
   *
   * 4.0 is left alone: nothing has measured its marking loops against a
   * HideObjects alt, and lib_room_is_dark() shows run400 never even assigns
   * its darkness byte.
   */
  if (showobjects || prop_get_taf_version (bundle) >= TAF_VERSION_400)
    obj_mark_room_objects_seen (game, room);

  /* Print room contents. */
  if (showobjects)
    {
      const scr_char *buffered = pf_get_buffer (filter);
      size_t noted = buffered ? strlen (buffered) : 0;

      lib_print_room_contents (game, room);

      buffered = pf_get_buffer (filter);
      if (buffered && strlen (buffered) > noted)
        is_described = TRUE;
    }

  lib_room_desc_event_looks (game, room, is_described);
}


/*
 * lib_can_go()
 *
 * Return TRUE if the player can move in the given direction.  Also the map's
 * test for which connectors are currently usable (scmap.cpp).
 */
scr_bool
lib_can_go (scr_gameref_t game, scr_int room, scr_int direction)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int restriction;
  scr_bool is_restricted = FALSE;

  /* Set up invariant parts of key. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Exits";
  vt_key[3].integer = direction;

  /* Check for any movement restrictions. */
  vt_key[4].string = "Var1";
  restriction = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
  if (restriction >= 0)
    {
      scr_int type;

      if (lib_trace)
        scr_trace ("Library: hit move restriction\n");

      /* Get restriction type. */
      vt_key[4].string = "Var3";
      type = prop_get_integer (bundle, "I<-sisis", vt_key);
      switch (type)
        {
        case 0:                /* Task type restriction */
          {
            scr_int check;

            /* Get the expected completion state. */
            vt_key[4].string = "Var2";
            check = prop_get_integer (bundle, "I<-sisis", vt_key);

            if (lib_trace)
              {
                scr_trace ("Library: task %ld, check %ld\n",
                          restriction, check);
              }

            /* Restrict if task isn't done/not done as expected. */
            if (gs_task_valid (game, restriction)
                && (check != 0) == gs_task_done (game, restriction))
              is_restricted = TRUE;
            break;
          }

        case 1:                /* Object state restriction */
          {
            scr_int object, check, openable;

            /* Get the target object; with none, nothing restricts. */
            object = obj_stateful_object (game, restriction);
            if (object < 0)
              break;

            /* Get the expected object state. */
            vt_key[4].string = "Var2";
            check = prop_get_integer (bundle, "I<-sisis", vt_key);

            if (lib_trace)
              scr_trace ("Library: object %ld, check %ld\n", object, check);

            /* Check openable and lockable objects. */
            vt_key[0].string = "Objects";
            vt_key[1].integer = object;
            vt_key[2].string = "Openable";
            openable = prop_get_integer (bundle, "I<-sis", vt_key);
            if (openable > 0)
              {
                scr_int lockable;

                /* See if lockable. */
                vt_key[2].string = "Key";
                lockable = prop_get_integer (bundle, "I<-sis", vt_key);
                if (lockable >= 0)
                  {
                    /* Lockable. */
                    if (check <= 2)
                      {
                        if (gs_object_openness (game, object) != check + 5)
                          is_restricted = TRUE;
                      }
                    else
                      {
                        if (gs_object_state (game, object) != check - 2)
                          is_restricted = TRUE;
                      }
                  }
                else
                  {
                    /* Not lockable, though openable. */
                    if (check <= 1)
                      {
                        if (gs_object_openness (game, object) != check + 5)
                          is_restricted = TRUE;
                      }
                    else
                      {
                        if (gs_object_state (game, object) != check - 1)
                          is_restricted = TRUE;
                      }
                  }
              }
            else
              {
                /* Not openable. */
                if (gs_object_state (game, object) != check + 1)
                  is_restricted = TRUE;
              }
            break;
          }
        }
    }

  /* Return TRUE if not restricted. */
  return !is_restricted;
}


/* List of direction names, for printing and counting exits. */
static const scr_char *const DIRNAMES_4[] = {
  "north", "east", "south", "west", "up", "down", "in", "out",
  nullptr
};
const scr_char *const DIRNAMES_8[] = {
  "north", "east", "south", "west", "up", "down", "in", "out",
  "northeast", "southeast", "southwest", "northwest",
  nullptr
};


/*
 * lib_room_has_exits()
 *
 * Return TRUE if the player room has at least one usable exit.  Used to
 * suppress the automatic exit listing appended to room descriptions when a
 * room has no exits at all.  The original ADRIFT runner builds the exit
 * string and, for the room description (as opposed to an explicit "exits"
 * command), discards it if it ends in "any direction!"; testing for usable
 * exits up front is the equivalent.
 */
const scr_char *
lib_direction_name (scr_int direction)
{
  const scr_int count = sizeof DIRNAMES_8 / sizeof DIRNAMES_8[0] - 1;

  if (direction < 0 || direction >= count)
    return nullptr;
  return DIRNAMES_8[direction];
}


/*
 * lib_compass_names()
 *
 * Return the direction names list for the game's compass, eight point or
 * four.
 */
const scr_char *const *
lib_compass_names (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  return prop_get_global_boolean (bundle, "EightPointCompass")
         ? DIRNAMES_8 : DIRNAMES_4;
}


/*
 * lib_room_exit_available()
 *
 * Return TRUE if the given room defines an exit in the given direction and
 * nothing currently blocks its use.
 */
scr_bool
lib_room_exit_available (scr_gameref_t game, scr_int room, scr_int direction)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4], vt_rvalue;

  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Exits";
  vt_key[3].integer = direction;
  return prop_get (bundle, "I<-sisi", &vt_rvalue, vt_key)
         && lib_can_go (game, room, direction);
}


/*
 * lib_room_exit_destination()
 *
 * Return TRUE and write the destination of the player room's exit in the
 * given direction, FALSE if the room defines no such exit.
 */
scr_bool
lib_room_exit_destination (scr_gameref_t game,
                           scr_int direction, scr_int *destination)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5], vt_rvalue;

  vt_key[0].string = "Rooms";
  vt_key[1].integer = gs_playerroom (game);
  vt_key[2].string = "Exits";
  vt_key[3].integer = direction;
  vt_key[4].string = "Dest";
  if (!prop_get (bundle, "I<-sisis", &vt_rvalue, vt_key))
    return FALSE;

  *destination = vt_rvalue.integer - 1;
  return TRUE;
}


static scr_bool
lib_room_has_exits (scr_gameref_t game, scr_int room)
{
  const scr_char *const *dirnames;
  scr_int index_;

  /* Return on the first valid, usable exit found. */
  dirnames = lib_compass_names (game);
  for (index_ = 0; dirnames[index_]; index_++)
    {
      if (lib_room_exit_available (game, room, index_))
        return TRUE;
    }
  return FALSE;
}


/*
 * lib_print_exits_list()
 *
 * Print a list of exits from the given room.
 */
static void
lib_print_exits_list (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *const *dirnames;
  scr_int index_;
  lib_list_t list;

  /* Poll for an exit for each valid direction name. */
  dirnames = lib_compass_names (game);
  for (index_ = 0; dirnames[index_]; index_++)
    {
      if (lib_room_exit_available (game, room, index_))
        list.push_back (index_);
    }
  if (!list.empty ())
    {
      /*
       * Vary text for a lone exit.  SCARE used to print "There is an
       * exit "/"There are exits " on turn 0, but no Runner version has
       * that wording at all (run370-run400 binaries carry only " can
       * only move " and " can move "), and the run400 Goldilocks
       * transcript says "I can move ..." even in the game-start room
       * display.
       */
      if (list.size () == 1)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You can only move ",
                                                 "I can only move ",
                                                 "%player% can only move "));
        }
      else
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You can move ",
                                                 "I can move ",
                                                 "%player% can move "));
        }
      lib_print_name_list (game, list, dirnames, " and ");
      /* The exits list is the `exits` command's answer, and like every other
         library answer it stops at the full stop -- the room builder appends
         it to the turn's one string (run400 472C64, run390 44813D) and the
         walk and event tick that follows joins onto it.  shadowpeak (4.00):
         "You can move north, east and west.  Seeker hums!"
         (runner_transcripts/shadowpeak.txt). */
      pf_buffer_character (filter, '.');
      pf_buffer_answer_break (filter);
    }
  else
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                      "You can't go in any direction!",
                                      "I can't go in any direction!",
                                      "%player% can't go in any direction!"));
      pf_buffer_answer_break (filter);
    }
}


/*
 * lib_cmd_print_room_exits()
 *
 * Command handler for "exits"; lists exits from the player room.
 */
scr_bool
lib_cmd_print_room_exits (scr_gameref_t game)
{
  lib_print_exits_list (game, gs_playerroom (game));
  return TRUE;
}


/*
 * lib_print_room_exits()
 *
 * Append the exits list to a room description if the ShowExits global
 * requests it.  The run400 room builder itself runs the "exits" command at
 * the end of every room display when MemVar_4941E9 (ShowExits) is set, and
 * strips the result again if it ends "any direction!" (@00472BFF-00472C64
 * in Proc_19_63_472CA4) -- so this applies to task ShowRoomDesc displays
 * just as much as to player-room ones.
 *
 * When the list is kept, the builder first PRINTS the turn's text so far --
 * it saves the message buffer (472C18), lets "exits" overwrite it, and sends
 * the saved text & "  " through the filtering print routine 47B568 (472C64)
 * -- so every %variable% and ALR in the move line and the room description
 * is resolved there and then, before the NPC walks and events tick.  Only the
 * exits list and what follows waits for the end of the turn.  All four
 * Runners do this: run390 44813D-44818D, run380 439B83, run370 433108.
 * Measured on wumpusrun (run400x, seed 72, equal 115-draw streams): its ALR
 * "You move" -> "{move%move%}" reads the variable EVENT 0's task redraws
 * every turn, and the Runner printed the value from before the tick on all
 * four move turns (depart/boldly go/head/boldly go = 2,5,8,5), where
 * interpolating at the flush printed the redrawn value.  pf_print_so_far()
 * filters the text there and freezes it, so it is not filtered again: a
 * plain checkpoint left it to the flush's second ALR walk, which put
 * adrift_maze's "twisty" through its twist ALR twice and patched Qui a tue
 * Dana's "Vous vous deplacez in." to the "Vous entrez." run400 never prints.
 *
 * That "  " is a literal, not pspace(), so the exits sentence joins the room
 * block on its line whatever ends it, and follows a bare heading with two
 * leading spaces.  Every Runner shows it: `look` in the p2give probe world is
 * "... Bob is here.  You can only move north." at all four versions
 * (runner_probes/ord.run370.give.rtf and runner_probes/ord.run380.give.rtf are
 * the scrollback itself), and egghunt's empty-Long rooms print "The Holy City"
 * then "  You can move north, south and west."
 * (runner_transcripts/egghunt.txt).  Scarier deliberately drops the two spaces
 * where they would open a line (deviation policy; see lib_buffer_literal_gap).
 */
void
lib_print_room_exits (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (prop_get_global_boolean (bundle, "ShowExits")
      && lib_room_has_exits (game, room))
    {
      pf_undo_auto_break (filter);
      lib_buffer_literal_gap (game, FALSE);
      pf_print_so_far (filter, gs_get_vars (game), bundle);
      lib_print_exits_list (game, room);
    }
}


/*
 * lib_describe_player_room()
 *
 * Print out details of the player room, in brief if verbose not set and the
 * room has already been visited.
 *
 * The room-name heading above the description is a 3.9 feature.  It is the
 * "showshortroom" Appearance option, and the string occurs eight times in the
 * run390 P-code and twice in run400's, but not once in run370's or run380's --
 * their Options -> Appearance submenu offers colours and a font size only.  So
 * the pre-3.9 Runners print no heading at all in verbose mode, and in brief
 * mode print the room name *with a trailing period* as the whole of the
 * description.  Measured live under Wine 2026-08-24: arlo.taf (3.70) in
 * run370, twenty-odd room entries with Verbose on and not one heading, then
 * the same replay with Verbose off giving "Alice's Garden.  Horse walks
 * towards you from the north." for a revisit; and akron.taf (3.80) in run380,
 * 25 room entries, again no heading anywhere.
 */
void
lib_describe_player_room (scr_gameref_t game, scr_bool force_verbose)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool is_verbose;

  is_verbose = force_verbose
               || game->verbose || !gs_room_seen (game, gs_playerroom (game));

  if (prop_get_taf_version (bundle) >= TAF_VERSION_390)
    lib_print_room_name (game, gs_playerroom (game));
  else if (!is_verbose)
    {
      /*
       * Pre-3.9 brief: the name, with a period, is the whole description.  No
       * paragraph break -- the pre-3.9 room text runs on the line below the
       * move message rather than after a blank line, "You move south." then
       * "Alice's Garden.  Horse walks towards you from the north."
       */
      pf_buffer_string (filter, lib_get_room_name (game, gs_playerroom (game)));
      pf_buffer_string (filter, ".\n");
    }

  /* Print other room details if applicable. */
  if (is_verbose)
    {
      /* Print room description, and objects and NPCs. */
      lib_print_room_description (game, gs_playerroom (game));

      /* Print exits if the ShowExits global requests it. */
      lib_print_room_exits (game, gs_playerroom (game));
    }
}
