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
 * Reading objects.
 *
 * See sclibrar.h for what the library files share.
 */

#include "sclibrar.h"


/*
 * lib_cmd_read_object()
 * lib_cmd_read_other()
 *
 * Attempt to read the referenced object, or something else.
 */
/*
 * 4.0 reads inside examines (471F94), whose noun is referencedob's: a line
 * the up-front score ties goes through its passes, and with no Prefix word
 * typed the last object marked wins.  p4WITHQ2.taf
 * (runner_probes/withq2.run400.txt): `read book with knife` prints the
 * book's text, `read rope with knife` answers "You can't read the knife!".
 * -1 when the line did not tie or the answer is not here.
 */
static scr_int
lib_read_tied_object_400 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int object;

  if (!lib_is_version_400 (game) || !input
      || lib_verb_object_resolve_400_string (game, input, nullptr, TRUE) != -1)
    return -1;

  object = lib_examine_referencedob_400 (game, input);
  if (object >= 0
      && !obj_indirectly_in_room (game, object, gs_playerroom (game)))
    return -1;
  return object;
}

static scr_bool lib_read_object (scr_gameref_t game, scr_int object);

/*
 * lib_read_tail_pre400()
 *
 * End a read answer.  Pre-4.0 `read` is answered inside examines(), and
 * every arm of it -- the ReadText, "can't read <the X>!", the description a
 * readable object with no ReadText falls back to, the darkness line -- goes
 * on into the examine tail: the openness state ("  The <Short> is closed.")
 * and whatisinon()'s contents.  run390 44BE30 -> 44BE60-44BEE6, run380
 * 43CF22, run370 435629.  Measured on Lair of the CyberCow (3.90,
 * runner_transcripts/cybercow_win.txt T97): `read envelope` answers
 * "\"Hero.\"  The envelope is closed.".  4.0 is unread and left alone.
 */
static void
lib_read_tail_pre400 (scr_gameref_t game, scr_int object)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
    lib_examine_tail (game, object, TRUE);
  pf_buffer_character (gs_get_filter (game), '\n');
}

scr_bool
lib_cmd_read_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_read_tied_object_400 (game);
  if (object < 0)
    {
      object = lib_disambiguate_object (game, "read", &is_ambiguous);
      if (object == -1)
        return is_ambiguous;
    }
  return lib_read_object (game, object);
}

static scr_bool
lib_read_object (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int task;
  scr_bool is_readable;
  const scr_char *readtext, *description;

  /*
   * Pre-4.0 `read` shares examines() with `x`, and the darkness byte is read
   * at 44BC37 -- before 44BC81, where examines() first asks whether the verb
   * was `read` at all.  So a dark room answers a readable object exactly as
   * it answers an examined one, ReadText and all never consulted.  See
   * lib_room_is_dark() and lib_cmd_examine_object().
   */
  if (lib_room_is_dark (game, gs_playerroom (game)))
    {
      lib_print_response_object (game,
                                 "You can't see ",
                                 "I can't see ",
                                 "%player% can't see ",
                                 object, " very clearly.");
      lib_read_tail_pre400 (game, object);
      return TRUE;
    }

  /* Verify that the object is readable. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Readable";
  is_readable = prop_get_boolean (bundle, "B<-sis", vt_key);
  if (!is_readable)
    {
      lib_print_response_object (game,
                                 "You can't read ",
                                 "I can't read ",
                                 "%player% can't read ",
                                 object, "!");
      lib_read_tail_pre400 (game, object);
      return TRUE;
    }

  /* Get and print the object's read text, if any. */
  vt_key[2].string = "ReadText";
  readtext = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (readtext))
    {
      pf_buffer_string (filter, readtext);
      lib_read_tail_pre400 (game, object);
      return TRUE;
    }

  /* Degrade to a shortened object examine. */
  vt_key[2].string = "Task";
  task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;

  /* Select either the main or the alternate description. */
  if (gs_task_valid (game, task) && gs_task_done (game, task))
    vt_key[2].string = "AltDesc";
  else
    vt_key[2].string = "Description";

  /* Print the description, or a "nothing special" default. */
  description = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (description))
    pf_buffer_string (filter, description);
  else
    {
      pf_buffer_string (filter, "There is nothing special about ");
      lib_print_object_np (game, object);
      pf_buffer_character (filter, '.');
    }

  lib_read_tail_pre400 (game, object);
  return TRUE;
}

scr_bool
lib_cmd_read_other (scr_gameref_t game)
{
  /*
   * Pre-4.0 `read` is not a verb of its own: it is ORed into the words that
   * ENTER examines() (run370 434E2A, run380 43C69D, run390 44B7FF), so a noun
   * that names nothing answers exactly as `x` does -- the flat, person-free
   * "Nothing special." tail, the same one lib_cmd_examine_other prints.  4.0
   * gave read its own handler and its own "<player> see no such thing."
   *
   * Measured: p39EXAM.taf (3.90), runner_probes/exam.run390.txt, `read
   * zzzz` -> "Nothing special."; p4EXAM.taf (4.00),
   * runner_probes/exam.run400.txt, the same command -> "You see no such
   * thing."
   *
   * Sharing examines() means sharing its darkness fork too; see
   * lib_cmd_examine_other().
   */
  if (!lib_is_version_400 (game))
    {
      /*
       * Sharing examines() means sharing its object as well: a `read` line
       * that names more than one is settled exactly as an `x` line is --
       * 3.90 by referencedob()'s last-word pass, 3.70 and 3.80 by the
       * "Which <X> would you like to examine." question -- and only a line
       * naming none reaches the flat tail.  Measured on p*OPENW.taf,
       * 2026-09-20: `read rock gem` / `read gem rock` are "You can't read
       * the gem!" / "You can't read the rock!" under run390 and "Which rock
       * would you like to examine.  The gem or the rock?" under run370 and
       * run380, and `read rock with slab` is "You can't read the slab!" /
       * "Which slab would you like to examine.  The rock or the slab?"
       * (runner_probes/openw.run3[789]0.ow.*,
       * runner_probes/openw.run3[789]0.ox.*).
       */
      const scr_char *line = run_get_dispatch_input ();
      scr_int named, index_, matched = 0;
      scr_bool is_ambiguous = FALSE;

      named = lib_examine_crowded_390 (game);
      if (named == -2)
        return lib_print_message (game,
                                  "Please examine one object at a time.\n");
      /*
       * The parser bound no object to this line, so co() has to be run over
       * it here, the way examines() runs it.  Only a line naming two or
       * more is taken over: one is left to the tail below, where the
       * dispatcher put it.
       */
      if (named < 0 && line)
        {
          GS_FOR_EACH_OBJECT (game, index_)
            {
              game->object_references[index_]
                = lib_co_pre400 (game, line, index_, 0);
              if (game->object_references[index_])
                matched++;
            }
          if (matched > 1)
            named = lib_disambiguate_object (game, "read", &is_ambiguous);
        }
      if (named >= 0)
        return lib_read_object (game, named);
      if (is_ambiguous)
        return TRUE;

      if (lib_room_is_dark (game, gs_playerroom (game)))
        return lib_print_response_message (game,
                                  "You can't see that very clearly.\n",
                                  "I can't see that very clearly.\n",
                                  "%player% can't see that very clearly.\n");
      return lib_print_message (game, "Nothing special.\n");
    }

  /* `read book with knife` names no one object; see lib_cmd_read_object(). */
  {
    const scr_int object = lib_read_tied_object_400 (game);

    if (object >= 0)
      return lib_read_object (game, object);
  }

  /*
   * Reject the attempt -- the same "<name> see no such thing." literal as
   * lib_cmd_examine_other(), unconjugated in the third person, and the same
   * not-a-turn flag (471F02): read is one of examines()' entry words.
   * House's `read defensor` with the book unseen leaves `turns` at 143
   * (runner_probes/house.run400.turnbisect.txt), and the RIFT event 34 turns
   * on shows it.
   */
  lib_print_response_message (game,
                              "You see no such thing.\n",
                              "I see no such thing.\n",
                              "%player% see no such thing.\n");
  game->is_admin = TRUE;
  return TRUE;
}
