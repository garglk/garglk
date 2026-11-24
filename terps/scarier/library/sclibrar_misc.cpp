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
 * The one-line stock responses: profanity and the flavour verbs.
 */

#include "sclibrar.h"


/*
 * lib_cmd_*()
 *
 * Standard response commands.  These are uninteresting catch-all cases,
 * but it's good to make then right as game ALRs may look for them.
 */
scr_bool
lib_cmd_profanity (scr_gameref_t game)
{
  return lib_print_message (game,
                            "I really don't think there's any need for language like"
                            " that!\n");
}

/*
 * lib_cmd_profanity_390()
 * lib_cmd_profanity_pre_400()
 *
 * Two words entered and left the Runner's swearing list.  `bugger` is in the
 * 3.9 and 4.0 lists but in neither 3.7's nor 3.8's, and `bloody` is in
 * 3.7/3.8/3.9 and gone from 4.0.  Where the word is not in that Runner's list
 * these decline, so the input falls through to the rest of the grammar exactly
 * as any other unrecognised word does.
 */
scr_bool
lib_cmd_profanity_390 (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;
  return lib_cmd_profanity (game);
}

scr_bool
lib_cmd_profanity_pre_400 (scr_gameref_t game)
{
  if (lib_is_version_400 (game))
    return FALSE;
  return lib_cmd_profanity (game);
}


scr_bool
lib_cmd_dance (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You do a little dance.\n",
                                     "I do a little dance.\n",
                                     "%player% do a little dance.\n");
}

scr_bool
lib_cmd_eat_other (scr_gameref_t game)
{
  /*
   * A 4.0 string (run400 4889C7).  run380's eat arm (443D6E) and run390's
   * speak only for a present object, so with none the line goes on to the
   * catch-all: `eat statue` from the wrong room is the same-room
   * answer in 3.8 (runner_probes/exam.run380.verb.rtf) and "I don't
   * understand." in 3.9 (runner_probes/exam.run390.verb.txt).
   */
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380
      && !lib_is_version_400 (game))
    return FALSE;

  return lib_print_message (game,
                            "I don't understand what you are trying to eat.\n");
}

scr_bool
lib_cmd_feel (scr_gameref_t game)
{
  return lib_print_response_message (game,
      "You feel nothing out of the ordinary.\n",
      "I feel nothing out of the ordinary.\n",
      "%player% feel nothing out of the ordinary.\n");
}

scr_bool
lib_cmd_fly (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You can't fly.\n",
                                     "I can't fly.\n",
                                     "%player% can't fly.\n");
}

scr_bool
lib_cmd_hint (scr_gameref_t game)
{
  return lib_print_message (game,
                            "You're just going to have to work it out for"
                            " yourself...\n");
}

scr_bool
lib_cmd_hum (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You hum a little tune.\n",
                                     "I hum a little tune.\n",
                                     "%player% hum a little tune.\n");
}

scr_bool
lib_cmd_listen (scr_gameref_t game)
{
  return lib_print_response_message (game,
      "You hear nothing out of the ordinary.\n",
      "I hear nothing out of the ordinary.\n",
      "%player% hear nothing out of the ordinary.\n");
}

scr_bool
lib_cmd_please (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "Your kindness gets you nowhere.\n",
                                     "My kindness gets me nowhere.\n",
                                     "%player%'s kindness gets nowhere.\n");
}

scr_bool
lib_cmd_run (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "Why would you want to run?\n",
                                     "Why would I want to run?\n",
                                     "Why would %player_pronoun% want to run?\n");
}

scr_bool
lib_cmd_say (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *string = nullptr;

  /*
   * run390 and run400 (therest 488DE4) both draw Int(Rnd*6) over six
   * responses; run370 and run380 (4442AA) draw Int(Rnd*5) over the same
   * table, so their sixth response is unreachable.  Keeping the span per
   * version keeps the Runner-mode stream in step with the Wine Runners
   * (inverness, measured 2026-09-12).
   */
  switch (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390
          ? scr_randomint (1, 6) : scr_randomint (1, 5))
    {
    case 1:
      string = "Gosh, that was very impressive.\n";
      break;
    case 2:
      string = lib_select_response (game,
                                    "Not surprisingly, no-one takes any notice"
                                    " of you.\n",
                                    "Not surprisingly, no-one takes any notice"
                                    " of me.\n",
                                    "Not surprisingly, no-one takes any notice"
                                    " of %player%.\n");
      break;
    case 3:
      string = "Wow!  That achieved a lot.\n";
      break;
    case 4:
      string = "Uh huh, yes, very interesting.\n";
      break;
    case 5:
      string = "That's the most interesting thing I've ever heard!\n";
      break;
    default:
      string = lib_select_response (game,
                                    "No-one listens to your rabblings.\n",
                                    "No-one listens to my rabblings.\n",
                                    "No-one listens to %player%'s"
                                    " rabblings.\n");
      break;
    }

  pf_buffer_string (filter, string);
  return TRUE;
}

scr_bool
lib_cmd_sing (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You sing a little song.\n",
                                     "I sing a little song.\n",
                                     "%player% sing a little song.\n");
}

/*
 * 3.7 and 3.8's talk hint (run370 loc_438748, run380 loc_4405D7) is guarded
 * by c("talk") Or c("speak") inside the named-character block -- the words
 * need not be next to each other, and there is no room test.  So `talk with
 * dave` is the hint too, where the `[talk/speak] %character%` rows miss it:
 * 'Use the format "ask Dave about <subject>".' (run370x
 * runner_probes/npcamb.run370.with.rtf) and '... [subject]".' (run380x
 * runner_probes/npcamb.run380.with.rtf).  The last character named wins, as
 * for lib_cmd_talk_to_npc().
 */

static scr_bool
lib_talk_hint_anywhere_pre390 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int index_, npc = -1;

  if (!input
      || prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return FALSE;

  GS_FOR_EACH_NPC (game, index_)
    {
      if (lib_npc_referenced (game, index_, input))
        npc = index_;
    }
  if (npc == -1)
    return FALSE;

  var_set_ref_character (gs_get_vars (game), npc);
  lib_print_wrapped_npc (game, "Use the format \"ask ",
                         npc, lib_ask_format_subject (game));
  return TRUE;
}

scr_bool
lib_cmd_speak_pre390 (scr_gameref_t game)
{
  return lib_talk_hint_anywhere_pre390 (game);
}

/*
 * A `talk` line no character answered.  therest's `c("ask") Or c("talk to")`
 * block (run370 43E9B7 ... run400 488B5C) takes `talk to` as it takes
 * `ask`: without "about", an empty buffer and no task run for the line, it
 * prints the generic 'Use the format "ask [character] about [subject]"'.
 * The rabblings line just below it only fills a buffer still empty, so it
 * is what a bare `talk dirty` gets, and a `talk to` line whose task ran
 * silently (sophie_comp `talk to devil`).
 *
 * Measured on run400x: Aegis `talk to Elder`, Whatever Happened to Uncle
 * Grumble `talk to mayor`, Shards of Memory `talk to reaper` and YADFA `talk
 * to elves` -- none of them a character's Name or Alias -- all answer the
 * generic hint.
 *
 * Both lines want an empty buffer, and 4.0's up-front resolver (see
 * lib_absent_seen_object()) has already filled it for a seen-but-absent
 * object the line names: The Plague - Redux `talk to girl`, with the girls
 * left behind, is "I couldn't see the girls." (run400x).
 */
scr_bool
lib_cmd_talk (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();

  if (lib_talk_hint_anywhere_pre390 (game))
    return TRUE;

  if (lib_cmd_verb_absent_400 (game))
    return TRUE;

  if (input && lib_input_contains_word (input, "talk to")
      && !run_any_task_ran_this_command ())
    return lib_cmd_ask_other (game);

  return lib_print_response_message (game,
      "No-one listens to your rabblings.\n",
      "No-one listens to my rabblings.\n",
      "No-one listens to %player%'s rabblings.\n");
}

scr_bool
lib_cmd_whistle (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You whistle a little tune.\n",
                                     "I whistle a little tune.\n",
                                     "%player% whistle a little tune.\n");
}

scr_bool
lib_cmd_interrogation (scr_gameref_t game)
{
  static const scr_char *const RESPONSES[] = {
    "Why do you want to know?\n",
    "Interesting question.\n",
    "Let me think about that one...\n",
    "I haven't a clue!\n",
    "All these questions are hurting my head.\n",
    "I'm not going to tell you.\n",
    "Someday I'll know the answer to that one.\n",
    "I could tell you, but then I'd have to kill you.\n",
    "Ha, as if I'd tell you!\n",
    "Ask me again later.\n",
    "I don't know - could you ask anyone else?\n",
    "Err, yes?!?\n",
    "Let me just check my memory banks...\n",
    "Because that's just the way it is.\n",
    "Do I ask you all sorts of awkward questions?\n",
    "Questions, questions...\n",
    "Who cares.\n"
  };

  return lib_print_message (game,
                            RESPONSES[scr_randomint (1, 17) - 1]);
}

scr_bool
lib_cmd_xyzzy (scr_gameref_t game)
{
  return lib_print_message (game,
                            "I'm sorry, but XYZZY doesn't do anything special in"
                            " this game!\n");
}

scr_bool
lib_cmd_egotistic (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, "No comment.\n");
  return TRUE;
}

scr_bool
lib_cmd_yes_or_no (scr_gameref_t game)
{
  return lib_print_message (game,
                            "That's interesting, but it doesn't mean much.\n");
}
