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
 * Asking NPCs about topics.
 *
 * See sclibrar.h for what the library files share.
 */

#include "sclibrar.h"


/*
 * lib_subject_in_text_390()
 *
 * The 3.9/4.0 subject test, the Runner's c(subject, text) (run390 4334B0):
 * both lower-cased, the first InStr hit that starts the text or follows a
 * space decides -- a match if it runs to the end of the text or is followed
 * by a space, "," or ".", no match otherwise, without looking further.  A hit
 * inside a word is skipped and the search resumes one character on.  The
 * subject is comma-terminated at posn, less its leading spaces, as nextsub()
 * hands it over.
 */
static scr_bool
lib_subject_in_text_390 (const scr_char *subject, scr_int posn,
                         const scr_char *string)
{
  std::string word, text (string);
  scr_int end;
  size_t from, hit;

  while (subject[posn] != NUL && scr_isspace (subject[posn]))
    posn++;
  for (end = posn; subject[end] != NUL && subject[end] != COMMA;)
    end++;
  word.assign (subject + posn, end - posn);
  if (word.empty ())
    return FALSE;
  for (auto &c : word)
    c = scr_tolower (c);
  for (auto &c : text)
    c = scr_tolower (c);

  for (from = 0; (hit = text.find (word, from)) != std::string::npos;
       from = hit + 1)
    {
      if (hit == 0 || text[hit - 1] == ' ')
        {
          const size_t after = hit + word.size ();
          return after == text.size () || text[after] == ' '
                 || text[after] == ',' || text[after] == '.';
        }
    }
  return FALSE;
}

#ifdef SCARIER_DUMP_TOOLS
/* The 3.7/3.8 InStr subject test, for SCR_TRACE_DEVIATIONS only; see
   lib_npc_find_topic(). */
static scr_bool
lib_subject_in_text_3738 (const scr_char *subject, scr_int posn,
                          const scr_char *string)
{
  std::string word, text (string);
  scr_int end;

  while (subject[posn] != NUL && scr_isspace (subject[posn]))
    posn++;
  for (end = posn; subject[end] != NUL && subject[end] != COMMA;)
    end++;
  word.assign (subject + posn, end - posn);
  for (auto &c : text)
    c = scr_tolower (c);

  return !word.empty () && text.find (word) != std::string::npos;
}
#endif

/*
 * lib_npc_topic_response()
 * lib_npc_reply_to()
 *
 * The text an NPC replies with on a given topic, empty if none, and the reply
 * itself.  Helpers for ask.
 */
static const scr_char *
lib_npc_topic_response (scr_gameref_t game, scr_int npc, scr_int topic)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int task;

  /* Find any associated task to control response. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Topics";
  vt_key[3].integer = topic;
  vt_key[4].string = "Task";
  task = prop_get_integer (bundle, "I<-sisis", vt_key);

  if (gs_task_valid (game, task - 1) && gs_task_done (game, task - 1))
    vt_key[4].string = "AltReply";
  else
    vt_key[4].string = "Reply";
  return prop_get_string (bundle, "S<-sisis", vt_key);
}

scr_bool
lib_npc_reply_to (scr_gameref_t game, scr_int npc, scr_int topic)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *const response = lib_npc_topic_response (game, npc, topic);

  /* Print the response if anything there. */
  if (!scr_strempty (response))
    {
      pf_buffer_string (filter, response);
      pf_buffer_answer_break (filter);
      return TRUE;
    }

  /* No response to this combination. */
  return FALSE;
}



/*
 * lib_ask_npc_about()
 * lib_cmd_ask_npc_about()
 * lib_cmd_talk_to_npc_about()
 *
 * Converse with NPC.
 *
 * `talk to X about Y` enters the same branch as `ask X about Y` in every
 * Runner -- its guard is `c("ask") Or c("talk to")` (run370 loc_4387F4,
 * run380 loc_440683, run390 loc_4597F2, run400 loc_47F8F7) -- but it differs
 * in what happens when no topic matches.  The ask-format hint branch runs
 * just before it and has already claimed the response line for anything
 * containing `talk to` (see lib_cmd_talk_to_npc), and the no-topic reply is
 * written only over an empty one (run380 loc_4409E8).  A matching topic does
 * overwrite it (loc_440918), so the topic still wins where there is one.
 */
/*
 * lib_npc_find_topic()
 *
 * The topic that answers an ask about the referenced text, -1 if none.  Every
 * Runner walks the topics and their comma-separated subjects with no break
 * (run370 438A23, run380 4408B2, run390 4599C6, run400 47F9D8):
 *
 *   if subject matches Or subject = "*":
 *     if subject <> "*" Or found = 0:
 *       reply = (Task > 0 And task done) ? AltReply : Reply
 *       if reply <> "": msg = reply; found = 1
 *
 * So the last topic with a non-empty reply answers, a "*" subject answers only
 * while nothing has, and a topic whose chosen reply is empty leaves an earlier
 * answer standing.  The match test is InStr below 3.9 (see
 * lib_subject_in_text_3738) and c() from 3.9 (lib_subject_in_text_390).
 * Measured on p39ASK.taf / p4ASK.taf (make_39_askprobe.py, run390x and run400x
 * alike): "red key" then "key" topics answer `the red key` and `red key` with
 * "key"; `the key.` finds "key"; "coin" then "coin"-with-an-empty-AltReply
 * answers from the first once the task is done; a "*" before "coin" is
 * overwritten by it.  The 3.7/3.8 substring test is measured on wrecked T211
 * (run380x): `ask her about good time` gets Suzie's "me, myself" reply, since
 * "time" contains "me".
 *
 * Deliberate deviation (2026-09-30): 3.7/3.8 use the 3.9 whole-word test,
 * case-free.  The 3.7/3.8 InStr answered `ask bob about good time` from a
 * "me" topic and `ask bob about paper` from an "ape" one, and since it
 * lower-cased only the text, a subject authored as "Key" never matched
 * (baf1e529d).
 */
scr_int
lib_npc_find_topic (scr_gameref_t game, scr_int npc)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *const text = var_get_ref_text (vars);
  scr_vartype_t vt_key[5];
  scr_int topic_count, topic, answer;
#ifdef SCARIER_DUMP_TOOLS
  scr_int runner_answer = -1;
  const scr_bool trace_3738 = SCR_TRACING_DEVIATIONS ()
                              && prop_get_taf_version (bundle) < TAF_VERSION_390;
#endif

  /* Get the topics the NPC converses about. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Topics";
  topic_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  answer = -1;
  for (topic = 0; topic < topic_count; topic++)
    {
      const scr_char *subjects;
      scr_int posn;

      /* Get subject list for this topic. */
      vt_key[3].integer = topic;
      vt_key[4].string = "Subject";
      subjects = prop_get_string (bundle, "S<-sisis", vt_key);

      /* Split into subjects by comma delimiter. */
      for (posn = 0; subjects[posn] != NUL;)
        {
          scr_int start, end;
          scr_bool is_star;

          /* nextsub(): skip leading spaces and commas; "*" is the subject. */
          for (start = posn; subjects[start] == ' ' || subjects[start] == COMMA;)
            start++;
          if (subjects[start] == NUL)
            break;
          for (end = start; subjects[end] != NUL && subjects[end] != COMMA;)
            end++;
          is_star = (end - start == 1 && subjects[start] == '*');

          if (lib_trace)
            scr_trace ("Library: subject %s[%ld]\n", subjects, start);

          if ((is_star || lib_subject_in_text_390 (subjects, start, text))
              && (!is_star || answer == -1)
              && !scr_strempty (lib_npc_topic_response (game, npc, topic)))
            {
              if (lib_trace)
                scr_trace ("Library: topic %ld answers\n", topic);

              answer = topic;
            }
#ifdef SCARIER_DUMP_TOOLS
          if (trace_3738
              && (is_star || lib_subject_in_text_3738 (subjects, start, text))
              && (!is_star || runner_answer == -1)
              && !scr_strempty (lib_npc_topic_response (game, npc, topic)))
            runner_answer = topic;
#endif

          posn = end;
        }
    }
#ifdef SCARIER_DUMP_TOOLS
  if (trace_3738 && runner_answer != answer)
    SCR_DEVIATION ("topic_whole_word", "npc=%ld runner=%ld ours=%ld", npc,
                   runner_answer, answer);
#endif
  return answer;
}

static void lib_print_npc_no_response (scr_gameref_t game, scr_int npc);

static scr_bool
lib_ask_npc_about (scr_gameref_t game, const scr_char *verb,
                   scr_bool hint_when_silent)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int npc, topic;
  scr_bool is_ambiguous;

  /*
   * Pre-4.0 `talk to X about Y`: the talk hint arm assigns the message for
   * every character named, here or not, and only a present character's
   * conversation arm, which runs after it, overwrites it.  So the LAST
   * character named settles it: with Ann and Bob here and Cora next door,
   * all three guards, `talk to guard about key` is Cora's hint, not "BOB
   * KEY." (run370x/run380x/run390x, runner_probes/npcamb.run370.b.rtf,
   * runner_probes/npcamb.run380.rtf, runner_probes/npcamb.run390.txt).
   */
  if (hint_when_silent && !lib_is_version_400 (game))
    {
      npc = lib_last_named_npc (game);
      if (npc != -1 && !npc_in_room (game, npc, gs_playerroom (game)))
        {
          var_set_ref_character (gs_get_vars (game), npc);
          lib_print_wrapped_npc (game, "Use the format \"ask ",
                                 npc, lib_ask_format_subject (game));
          return TRUE;
        }
    }

  /* Get the referenced npc, and if none, consider complete.  Pre-4.0 takes
     the LAST present character: `ask guard about key` is "BOB KEY.". */
  npc = lib_disambiguate_npc_pick (game, verb, &is_ambiguous, NPC_PICK_LAST);
  if (npc == -1)
    return is_ambiguous;

  if (lib_trace)
    scr_trace ("Library: asking NPC %ld\n", npc);

  topic = lib_npc_find_topic (game, npc);
  if (topic != -1 && lib_npc_reply_to (game, npc, topic))
    return TRUE;

  /*
   * 3.7/3.8 answer an unanswered `<subject>` -- the placeholder of the
   * ask-format hint, typed literally -- with "Smart Alec!", over the talk-to
   * hint too (run380 4409B7, run370 438B28).  run390 and run400 keep the
   * test (459B12, 47FB0E) but can never reach it: their Return handlers
   * escape every "<" as "&lt;" (run390 436130, run400 45C4E8), which the
   * 3.7/3.8 ones do not.  Measured on p38ASK.taf (make_38_askprobe.py,
   * run380x) against p39ASK/p4ASK: `ask erin about <subject>`.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390
      && strcmp (var_get_ref_text (gs_get_vars (game)), "<subject>") == 0)
    {
      pf_buffer_string (filter, "Smart Alec!\n");
      return TRUE;
    }

  /* No topic matched, so `talk to` falls back on the hint it displaced. */
  if (hint_when_silent)
    {
      lib_print_wrapped_npc (game, "Use the format \"ask ",
                             npc, lib_ask_format_subject (game));
      return TRUE;
    }

  /* NPC has no response. */
  lib_print_npc_no_response (game, npc);
  return TRUE;
}

/*
 * lib_print_npc_no_response()
 *
 * The no-topic answer of every ask: "<Name> does not respond to your
 * question." (run390 459B46, run400 47FB83).
 */
static void
lib_print_npc_no_response (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_new_sentence (filter);
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                " does not respond to your question.\n",
                                " does not respond to my question.\n",
                                " does not respond to %player%'s question.\n"));
}

scr_bool
lib_cmd_ask_npc_about (scr_gameref_t game)
{
  return lib_ask_npc_about (game, "ask", FALSE);
}

scr_bool
lib_cmd_talk_to_npc_about (scr_gameref_t game)
{
  return lib_ask_npc_about (game, "talk to", TRUE);
}

/*
 * lib_ask_npc_topic_after_task_390()
 *
 * 3.9: a topic answers an ask even when a task has already answered the line,
 * and replaces what the task printed.  run390's character handler enters its
 * `c("ask") Or c("talk to")` block (4597FE) with no test of the task-ran flag
 * MemVar_468198, and every topic reply is a plain assignment to the message
 * buffer (459A7A, 459AA7, 459AD4), so the task's text is overwritten, not
 * joined.  Only a reply does that: the no-topic answer (459B46) is written over
 * an empty or "can't talk to that." buffer alone, and the NPC must be in the
 * player's room (459941).  4.0 gates the same block on the flag (run400
 * 47F900), so there a task keeps the line.
 *
 * Measured on Zombies Are Cool (ZAC.taf, 3.90; run390x
 * runner_transcripts/zombies.txt, turns 10-14, 29 and 30): the task `talk to
 * stu` / `ask stu about *` prints "Stu shakes his head, as if he doesn't
 * understand the question.", and run390 shows Stu's topic reply alone on
 * every one of them.
 *
 * The task that ran may also have printed NOTHING: then the buffer the ask
 * block finds is empty, and with no topic the no-response answer (459B46) is
 * written after all.  That is the toronto case (A Day In Toronto, 3.90; run390x
 * runner_transcripts/toronto.txt T9): task 3 `ask waiter about burger` has no
 * CompleteText and only moves the burger in, and the Runner answers the line
 * with the Waiter's topic "ok" where the silent-task rule alone would have
 * printed the game's DontUnderstand "huh?".  How a silent task's line reaches
 * characters() at all is run_all_commands()' business: see the note on
 * lib_line_names_npc_390().
 *
 * Called once uip_match() has matched "ask %character% about %text%" or its
 * talk-to twin.  Replies, cutting the buffer back to mark first, and returns
 * TRUE; prints nothing and returns FALSE when no present NPC or topic answers
 * -- unless over_empty_buffer says the buffer is empty at mark, when the
 * present NPC's no-response answer is printed instead and TRUE returned.
 */
scr_bool
lib_ask_npc_topic_after_task_390 (scr_gameref_t game, size_t mark,
                                  scr_bool over_empty_buffer)
{
  scr_int npc, topic;

  if (npc_count_referenced_present (game, &npc) != 1)
    return FALSE;

  topic = lib_npc_find_topic (game, npc);
  if (topic == -1)
    {
      if (!over_empty_buffer)
        return FALSE;
      pf_truncate (gs_get_filter (game), mark);
      var_set_ref_character (gs_get_vars (game), npc);
      lib_print_npc_no_response (game, npc);
      return TRUE;
    }

  pf_truncate (gs_get_filter (game), mark);
  var_set_ref_character (gs_get_vars (game), npc);
  return lib_npc_reply_to (game, npc, topic);
}

/*
 * lib_line_names_npc_390()
 *
 * run390 generaltasks' end-of-line scan of the characters, 4605EB-460638: for
 * every NPC in the game, present or not, `c(Name, 0) Or c(Alias(0), 0)` --
 * the same Name-or-first-Alias whole-word test as lib_npc_named_in_line().
 * Its result (var_350) is the second term of the DontUnderstand substitution
 * at 46063E-46065A, `If msg = "" And var_350 = 0 Then msg = DontUnderstand:
 * GoTo 46067F`, which also jumps the calls to characters() and events().  So
 * a line that names a character keeps its empty buffer, and characters() and
 * events() still run for it: it is a turn, and the ask block 4597FE gets to
 * write into the buffer.  The one measured reach is a silent task's ask line
 * (toronto T9, see lib_ask_npc_topic_after_task_390()); 4.0 has the same
 * two-term test at 48B573 but its characters() is gated on the task-ran flag,
 * so nothing there ever fills the buffer.
 */
scr_bool
lib_line_names_npc_390 (scr_gameref_t game, const scr_char *input)
{
  scr_int npc;

  GS_FOR_EACH_NPC (game, npc)
    {
      if (lib_npc_named_in_line (game, npc, input))
        return TRUE;
    }
  return FALSE;
}
