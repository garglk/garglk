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
 * Room names and headings, room contents, room descriptions and exits.
 */

#include "sclibrar.h"


/*
 * lib_print_room_name_lower()
 *
 * Print a room's name for the where/find/locate answers.  The Runner folds
 * the whole name to lower case there -- it builds the answer as `... &
 * LCase(room.name) & "."` -- so "Drake Stables" comes out "Janai is drake
 * stables." (aegis).
 *      run400 @468143 (objects, whereis) and @47FD19 (characters)
 *
 * Deliberate deviation (WINE-TRANSCRIPTS.md): the fold reads well for
 * "In the pantry", "Your room" or "EXERCISE YARD", but turns a name into
 * a common noun.  Scarier keeps a name as authored when it capitalises two
 * or more words past an opening article or preposition -- "Drake Stables",
 * "Amanda's Living Room" -- or opens with a possessive, "Jane's house", and
 * lowers only that opening word: "the Main Hall".  Everything else is
 * folded as the Runner folds it, and so is every name in a game whose ALRs
 * quote the folded form.
 */
static const scr_char *const LIB_ROOM_OPENING_WORDS[] =
  { "a", "an", "the", "in", "inside", "on", "at", "by", "near", "outside",
    "under", "underneath", "beneath", "below", "above", "behind",
    "beside", "before", "within", "among", "atop", "along", "across",
    "into", "onto", "upon", NULL };

static scr_bool
lib_room_opening_word (const std::string &word)
{
  const scr_char *const *entry;

  for (entry = LIB_ROOM_OPENING_WORDS; *entry; entry++)
    if (scr_strcasecmp (word.c_str (), *entry) == 0)
      return TRUE;
  return FALSE;
}

void
lib_print_room_name_lower (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  std::string name (lib_get_room_name (game, room));
  std::string lower (name);
  size_t index_ = 0, first_start = std::string::npos, first_end = 0;
  scr_int words = 0, capitalised = 0;
  scr_bool possessive = FALSE, opener = FALSE;

  scr_lowercase (lower);
  if (pf_alr_mentions (gs_get_bundle (game), lower.c_str ()))
    {
      pf_buffer_string (filter, lower.c_str ());
      return;
    }

  /* Walk the words outside tags. */
  while (index_ < name.size ())
    {
      size_t end;
      scr_bool has_lower = FALSE;

      if (name[index_] == '<')
        {
          const size_t close = name.find ('>', index_);
          if (close == std::string::npos)
            break;
          index_ = close + 1;
          continue;
        }
      if (!scr_isalpha (name[index_]))
        {
          index_++;
          continue;
        }
      for (end = index_; end < name.size ()
                         && (scr_isalpha (name[end]) || name[end] == '\'');
           end++)
        has_lower |= (scr_toupper (name[end]) != name[end]);

      if (words == 0)
        {
          const std::string word (name, index_, end - index_);

          first_start = index_;
          first_end = end;
          opener = lib_room_opening_word (word);
          possessive = (scr_tolower (name[index_]) != name[index_]) && word.size () > 2
                       && word.compare (word.size () - 2, 2, "'s") == 0;
        }
      if (!(words == 0 && opener)
          && (scr_tolower (name[index_]) != name[index_]) && has_lower)
        capitalised++;
      words++;
      index_ = end;
    }

  if (capitalised < 2 && !possessive)
    {
      pf_buffer_string (filter, lower.c_str ());
      return;
    }
  if (opener)
    for (index_ = first_start; index_ < first_end; index_++)
      name[index_] = scr_tolower (name[index_]);
  pf_buffer_string (filter, name.c_str ());
}


/*
 * lib_room_name_text()
 * lib_long_opens_with_name()
 * lib_room_name_headings()
 *
 * Whether to head a 3.9/4.0 room description with the room's name.
 *
 * The heading is the Runner's "Room names in descriptions" Appearance
 * option ("showshortroom", default on; see lib_describe_player_room()).  It
 * is a player preference, not game data, and some authors wrote with it
 * off: their descriptions carry the room's name themselves, so the Runner's
 * default prints it twice -- CityInFear's "Alley" heading over "ALLEY", or
 * SILKNOIL's "In Silk Tunnel" over its own bold "In Silk Tunnel".  Games
 * upconverted from 3.8, where no Runner printed a heading, do the same.
 *
 * Deliberate deviation (WINE-TRANSCRIPTS.md): a game in which more than half
 * of the rooms with a description open it with the room's name, standing on
 * its own, gets no heading.  Counted across the corpus on 2026-11-04, 20
 * of the 724 3.9/4.0 game files (16 games, plus patched copies) are over
 * that line, every one at 60% or more; the next is at 50% (ssteacher, 1
 * of 2), then 29%.  Scene of the Crime counts only 63%, but every one of its
 * rooms opens with a heading of its own in other words ("Alley" over "AN
 * ALLEY").
 */
static scr_bool
lib_is_word_char (scr_char character)
{
  return scr_isalpha (character) || isdigit ((unsigned char) character);
}

static std::string
lib_room_name_text (const scr_char *text)
{
  std::string out;
  scr_bool space = FALSE;

  for (; *text; text++)
    {
      if (*text == '<')
        {
          const scr_char *close = strchr (text, '>');
          if (!close)
            break;
          out += '\x01';
          space = FALSE;
          text = close;
          continue;
        }
      if (*text == ' ' || *text == '\t')
        {
          space = TRUE;
          continue;
        }
      if (space && !out.empty ())
        out += ' ';
      space = FALSE;
      out += scr_tolower (*text);
    }
  return out;
}

static scr_bool
lib_long_opens_with_name (const scr_char *short_name, const scr_char *long_text)
{
  std::string name = lib_room_name_text (short_name);
  std::string text = lib_room_name_text (long_text);
  size_t start, end, index_;

  /* The name without tags, outer punctuation or space. */
  name.erase (std::remove (name.begin (), name.end (), '\x01'), name.end ());
  for (start = 0; start < name.size () && !lib_is_word_char (name[start]);
       start++)
    ;
  for (end = name.size (); end > start && !lib_is_word_char (name[end - 1]);
       end--)
    ;
  if (end - start < 2)
    return FALSE;
  name = name.substr (start, end - start);

  /* The description's opening, past tags, newlines and quotes. */
  for (index_ = 0; index_ < text.size ()
                   && (text[index_] == '\x01' || text[index_] == '\n'
                       || text[index_] == '\r' || text[index_] == ' '
                       || text[index_] == '"');
       index_++)
    ;
  if (text.compare (index_, name.size (), name) != 0)
    return FALSE;

  /* The name must stand alone, not open a sentence: "Alley" then "\n". */
  for (index_ += name.size (); index_ < text.size () && text[index_] == ' ';
       index_++)
    ;
  return index_ == text.size () || !lib_is_word_char (text[index_]);
}

static scr_bool
lib_has_words (const scr_char *text)
{
  const std::string plain = lib_room_name_text (text);
  size_t index_;

  for (index_ = 0; index_ < plain.size (); index_++)
    if (lib_is_word_char (plain[index_]))
      return TRUE;
  return FALSE;
}

scr_bool
lib_room_name_headings (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (game->room_name_headings == 0)
    {
      scr_vartype_t vt_key[3];
      scr_int room_count, room, described = 0, named = 0;

      vt_key[0].string = "Rooms";
      room_count = prop_get_child_count (bundle, "I<-s", vt_key);
      for (room = 0; room < room_count; room++)
        {
          const scr_char *short_name, *long_text;

          vt_key[1].integer = room;
          vt_key[2].string = "Short";
          short_name = prop_get_string (bundle, "S<-sis", vt_key);
          vt_key[2].string = "Long";
          long_text = prop_get_string (bundle, "S<-sis", vt_key);
          if (!lib_has_words (long_text))
            continue;
          described++;
          if (lib_long_opens_with_name (short_name, long_text))
            named++;
        }
      game->room_name_headings = (described > 0 && named * 2 > described)
                                 ? 2 : 1;
    }
  return game->room_name_headings == 1;
}


void
lib_print_room_name (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *name;

  /*
   * Open a paragraph for the name.  The Runner has no inline room name, so
   * anything already buffered ran straight into the room description there;
   * here it would run straight into a heading instead.  See
   * pf_buffer_paragraph_break().
   */
  pf_buffer_paragraph_break (filter);

  /* Print the room name, possibly in bold. */
  name = lib_get_room_name (game, room);
  if (game->settings.bold_room_names)
    {
      pf_buffer_tag (filter, SCR_TAG_BOLD);
      pf_buffer_string (filter, name);
      pf_buffer_tag (filter, SCR_TAG_ENDBOLD);
    }
  else
    pf_buffer_string (filter, name);

  /*
   * The heading's newline is one the Runner has too, so it is not a section
   * terminator of ours for pf_buffer_join() to pop.  Left unmarked, a room
   * whose description is empty ran its contents straight on after the name:
   *
   *     Inside the Top Hat  A bunny twitches its whiskers at me  <- Scarier
   *     Inside the Top Hat                                       <- run400
   *     A bunny twitches its whiskers at me
   *
   * tophat.taf (4.0), whose one room has an empty Long, in both `up` turns
   * of its transcript; professor t11/t18/t38/t54, viewtohome t20/t56 and
   * woof t8 say the same.  Read with the leading break that
   * pf_buffer_paragraph_break() supplies just above, the measurements fit a
   * Runner that appends a break, the name and a break onto the one output
   * string the turn is building -- pspace() only ever appends a separator,
   * so it leaves that trailing Chr(10) standing and never takes it back.
   * The P-code has not been read; the transcripts are the evidence.
   */
  pf_buffer_character (filter, '\n');
  pf_buffer_hard_break (filter);
}


/*
 * lib_buffer_literal_gap()
 *
 * The Runner's room lister hard-codes two spaces ahead of "Also here", the
 * joined "... is here." sentence and the exits sentence -- literals, not
 * pspace() -- so after a block that ends in a line break, or a bare
 * heading, its line opens with two spaces.  Scarier deliberately keeps the
 * join but not that indent (deviation policy): the gap is left out where
 * the buffer is empty or already stands at the start of a line, unless an
 * author ALR could be matching on it (lib_alrs_see_gap), which keeps the
 * Runner's text so the same ALRs fire.  The exits sentence passes guarded
 * FALSE: no corpus ALR matches on its gap, and les_feux's leading-space ALRs
 * would otherwise keep every French exits line indented.
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
   * live in run400 ("Probe backlog" in
   * test/adrift4/notes/RUNNER_TESTS.md).  The "#" substitution happens in
   * the *loader* (@00091EDF): the text simply becomes "<name> is here." before
   * the game starts, and by the time the room lister (@00072944) runs there is
   * no "#" left to test for.  What it tests instead is the tail -- Right(text,
   * 9) = " is here." -- so a character whose text the author wrote out in full
   * joins the sentence too, contributing the text with those nine characters
   * trimmed off rather than its own name.  Probe NPCs "Delta" ("Delta is
   * here.") and "Foxtrot" ("The stranger is here.") come out as "Alpha,
   * Charlie, Delta and The stranger are here.", so it is the text that is
   * trimmed and not the name that is looked up.  The test is exact and
   * case-sensitive: "Golf is here!" and "Hotel IS HERE." both stay in the
   * second group.
   *
   * And the joined sentence comes *first*, ahead of the characters with their
   * own text.
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
           * twilight (3.8) prints "a monkey is here.", goldilocks (4.0)
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
           * the room.  Measured live on cave.taf (3.8): "By the
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
   * (measured live) proves the routes DIVERGE when an alt
   * describes the room: 3.8 prints the substitute in the Long's place AND
   * the alt after it, where this render-time guard stayed silent.  The 3.8
   * case therefore now lives up at the Long print; only 3.9 keeps this
   * guarded tail.  3.7 has no such string and leaves the
   * room blank; 4.0 dropped it again.
   *
   * Read the guard, not the literal: hanging the sentence off an empty Long
   * alone moves 16 of the 303 corpus goldens.  The guard needs BOTH halves:
   * the alt text empty AND the Long itself empty (`var_A4(4) = vbNullString`
   * at 4478C0, the room record's Long, not what got printed).  yeh.taf
   * (3.9) measured on run390x: "Outside", "Woods", "Spooky
   * area." each have a Long and a start alt that suppresses it with empty
   * text, and the Runner prints the heading and the exits alone.
   *
   * Measured on p39EXAM.taf (3.9), runner_probes/exam.run390.txt and
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
   * Finally, print any relevant event look text.  The Runner appends it dead
   * last in the room block, after the object list and the character lines, run
   * on with its two-space separator (probed live in both runners).  Join onto
   * the description block only if this call printed one, so an event's text
   * can't migrate up onto the room name line.
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
   * (3.9): entering the Dark Cave unlit
   * (runner_probes/dark.run390.unseen.txt) leaves `x stone`, `read stone` and
   * `x box` at the unmatched-noun answer "You can't see that very clearly.",
   * `take stone` at "Take what?" and `get all from box` at "You can't get
   * anything from that." -- the container is not reachable either.
   * runner_probes/dark.run390.seen.txt then walks in with the torch, so the
   * same objects are stamped, drops the torch and returns: now `take stone`
   * succeeds and `x stone` answers the named form, "You can't see the stone
   * very clearly."  Being seen is permanent; being lit is not.
   *
   * run400 does the same, although it has no darkness byte.  Measured on
   * p4DARK.taf, the same world in the 4.0 layout with the cave's darkness
   * as an ordinary type-2 HideObjects alt (make_400_darkprobe.py):
   * entering unlit (runner_probes/dark.run400.unseen.txt) leaves `x stone`,
   * `x pebble`, `x box` and `read stone` at "You see no such thing.", `take
   * stone` at "Take what?" and `get all from box` at "I don't understand
   * where you want to get things from."; after one lit visit
   * (runner_probes/dark.run400.seen.txt) `take stone` succeeds in the dark
   * and `x stone` gives the plain description, not the 3.9 "can't see"
   * form, since that needs the darkness byte run400 never assigns.  So in
   * 4.0 a HideObjects alt hides objects from the noun resolver, but the
   * examine messages are the lit ones.
   */
  if (showobjects)
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
 * lib_direction_name()
 *
 * Return the name of a direction in the eight point list, or NULL if the
 * index is out of range.
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


/*
 * lib_room_has_exits()
 *
 * Return TRUE if the player room has at least one usable exit.  Used to
 * suppress the automatic exit listing appended to room descriptions when a
 * room has no exits at all.  The Runner builds the exit string and, for the
 * room description (as opposed to an explicit "exits" command), discards it
 * if it ends in "any direction!"; testing for usable exits up front is the
 * equivalent.
 */
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
       * Vary text for a lone exit.  SCARE prints "There is an exit "/"There
       * are exits " on turn 0, but no Runner version has that wording at all
       * (run370-run400 binaries carry only " can only move " and " can move
       * "), and the run400 Goldilocks transcript says "I can move ..." even in
       * the game-start room display.
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
      /*
       * The exits list is the `exits` command's answer, and like every other
       * library answer it stops at the full stop -- the room builder appends
       * it to the turn's one string (run400 472C64, run390 44813D) and the
       * walk and event tick that follows joins onto it.  shadowpeak (4.0):
       * "You can move north, east and west.  Seeker hums!"
       * (runner_transcripts/shadowpeak.txt).
       */
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
 * lib_print_message_with_exits()
 *
 * Print a refusal that sends the player back to the compass -- "Unknown
 * place." or "Just a direction will do." -- and follow it on the same line
 * with the exits sentence, when the room has any exits.  No Runner does
 * this; both strings stand alone at every version.  Deliberate deviation
 * (WINE-TRANSCRIPTS.md): SCARE named the exits here, and a nudge toward
 * directions is more useful when it says which ones.  The message is given
 * without its newline.
 */
scr_bool
lib_print_message_with_exits (scr_gameref_t game, const scr_char *message)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, message);
  if (lib_room_has_exits (game, gs_playerroom (game)))
    {
      pf_buffer_string (filter, "  ");
      lib_print_exits_list (game, gs_playerroom (game));
    }
  else
    pf_buffer_character (filter, '\n');
  return TRUE;
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
 * description.  Measured live under Wine: arlo.taf (3.7) in run370,
 * twenty-odd room entries with Verbose on and not one heading, then the same
 * replay with Verbose off giving "Alice's Garden.  Horse walks towards you
 * from the north." for a revisit; and akron.taf (3.8) in run380, 25 room
 * entries, again no heading anywhere.
 */
void
lib_describe_player_room (scr_gameref_t game, scr_bool force_verbose)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool is_verbose;

  is_verbose = force_verbose || game->settings.verbose
               || !gs_room_seen (game, gs_playerroom (game));

  if (prop_get_taf_version (bundle) >= TAF_VERSION_390)
    {
      /*
       * A game whose descriptions open with the room's own name gets no
       * heading over them, unless the brief heading is all there is.  See
       * lib_room_name_headings().
       */
      if (!is_verbose || lib_room_name_headings (game))
        lib_print_room_name (game, gs_playerroom (game));
      else
        pf_buffer_paragraph_break (filter);
    }
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
