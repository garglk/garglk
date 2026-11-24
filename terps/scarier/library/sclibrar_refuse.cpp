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
 * Stock refusals: nothing happens, can't do, don't think, and the
 * "<verb> what?" family.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"


/*
 * run370 and run380 have no checkverb: the only "<Verb> what?" literals
 * they hold are Take, Drop, Wear, Remove, With and 3.8's Open/Close.  A
 * bare verb runs the verb's own arm with no object, so `lock` is "You
 * can't lock that.", `break` "You might need that.", `press` "You press,
 * but nothing happens.".  Neither has a touch arm (touch is 3.9's, run390
 * 45EB9C) and run370 has no shake arm (run380 444A4E), so those lines,
 * bare or not, go to the catch-all.  Their fix arm is `c("fix") Or
 * c("repair") Or c("mend")` with one message, "I don't think you can fix
 * that." (run380 44535C); 3.9 gives each verb its own.  p37/p38NPCAMB
 * (harness/make_3738_npcambprobe.py), run370x/run380x
 * runner_probes/npcamb.run3[78]0.bareverb.rtf.
 *
 * Deliberate deviation: Scarier gives touch and shake their 3.9 arms at
 * every version, and rub its 4.0 one (see its row in scrun_tables.cpp), so the
 * player learns the verb was understood.
 */
static scr_bool
lib_bare_verb_pre390 (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390;
}

static const scr_char *
lib_fix_verb_pre390 (scr_gameref_t game, const scr_char *verb)
{
  return lib_bare_verb_pre390 (game) ? "fix" : verb;
}

/*
 * lib_nothing_happens_common()
 * lib_nothing_happens_object()
 * lib_nothing_happens_other()
 *
 * Central handler for a range of nothing-happens messages.  More
 * uninteresting responses.
 */
static scr_bool
lib_nothing_happens_common (scr_gameref_t game,
                            const scr_char *verb_general,
                            const scr_char *verb_third_person,
                            scr_bool is_object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int perspective, object;
  const scr_char *person, *verb;
  scr_bool is_ambiguous;

  /* Use person and verb tense according to perspective. */
  perspective = lib_get_perspective (game);
  switch (perspective)
    {
    case LIB_FIRST_PERSON:
      person = "I ";
      verb = verb_general;
      break;
    case LIB_SECOND_PERSON:
      person = "You ";
      verb = verb_general;
      break;
    case LIB_THIRD_PERSON:
      person = "%player% ";
      verb = verb_third_person;
      break;
    default:
      scr_error ("lib_nothing_happens: unknown perspective, %ld\n", perspective);
      person = "You ";
      verb = verb_general;
      break;
    }

  /* 4.0's " with " split; see lib_with_clause_400(). */
  {
    scr_int first = -1, instrument = -1;

    switch (lib_with_clause_400 (game, &first, &instrument))
      {
      case LIB_WITH_NONE:
        break;
      case LIB_WITH_DECLINE:
        return FALSE;
      case LIB_WITH_ANSWERED:
        return TRUE;
      case LIB_WITH_SUFFIX:
        pf_buffer_string (filter, person);
        pf_buffer_string (filter, verb);
        pf_buffer_character (filter, ' ');
        lib_print_object_np (game, first);
        lib_print_wrapped_object (game, " with ", instrument,
                                  ", but nothing happens.\n");
        lib_non_answer = TRUE;
        return TRUE;
      }
  }

  /* If the command target was not an object, end it here. */
  if (!is_object)
    {
      pf_buffer_string (filter, person);
      pf_buffer_string (filter, verb);
      pf_buffer_string (filter, ", but nothing happens.\n");
      lib_non_answer = TRUE;
      return TRUE;
    }

  /* Get the referenced object.  If none, return immediately. */
  object = lib_disambiguate_object (game, verb_general, &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Nothing happens. */
  pf_buffer_string (filter, person);
  pf_buffer_string (filter, verb);
  pf_buffer_character (filter, ' ');
  lib_print_object_np (game, object);
  pf_buffer_string (filter, ", but nothing happens.\n");
  lib_non_answer = TRUE;
  return TRUE;
}

static scr_bool
lib_nothing_happens_object (scr_gameref_t game,
                            const scr_char *verb_general,
                            const scr_char *verb_third_person)
{
  return lib_nothing_happens_common (game,
                                     verb_general, verb_third_person, TRUE);
}

static scr_bool
lib_nothing_happens_other (scr_gameref_t game,
                           const scr_char *verb_general,
                           const scr_char *verb_third_person)
{
  return lib_nothing_happens_common (game,
                                     verb_general, verb_third_person, FALSE);
}


/*
 * lib_hit_arm_pre390()
 *
 * The pre-4.0 characters() attack arm (run380 440260), and why `attack
 * dave` is DontUnderstand at 3.7/3.8 while `hit dave` is "Dave avoids your
 * feeble attempts." (run370x/run380x runner_probes/attackarm.run3[78]0.rtf).
 *
 * generaltasks' turn tail (run380 443160) runs characters() only when
 * therest left a message, else prints DontUnderstand and ticks nothing.
 * The arm itself enters for a line holding c("hit"), c("kill"), c("kick"),
 * c("punch") or c("attack") -- 3.8 adds "and no task ran" -- but ONLY when
 * the message is empty or ends ", but nothing happens.", which is what
 * therest's hit, push, pull, press and kick arms leave.  therest has no
 * "attack" arm at all, so a bare `attack dave` reaches the tail with an
 * empty message: DontUnderstand, no tick, and c("attack") in the arm is
 * dead unless another of those verbs carries it -- `push attack dave`,
 * `attack dave push` and `pull attack dave` are all "Dave avoids your
 * feeble attempts." on both Runners, and `push attack cora` with Cora next
 * door is "Cora is not here!".  kill and punch keep therest's own "Now that
 * isn't very nice." / "Who do you think you are, Mike Tyson?" because
 * those do not end ", but nothing happens.".
 *
 * The arm walks the characters in index order and takes the first the line
 * names (c(Name) Or c(Alias(0)), no seen test).  Not in the room: "<Name>
 * is not here!".  In the room, no c("with"): "<Name> avoids <your> feeble
 * attempts.".  With c("with"): a loop over every object the line names
 * (3.8 co(), 3.7 c(Short) Or c(Alias(0))) whose Short or first Alias sits
 * after the "with" -- a BINARY InStr of the raw name against the
 * lower-cased line, so a capitalised Short never passes -- writing, and the
 * LAST match wins: not held "<You> don't have <the X>!", Weapon "<You>
 * swing at <Name> with <the X>, but you miss.", else "I don't think <the X>
 * would be a very affective weapon!".  No match leaves therest's
 * nothing-happens line standing.  3.9 is lib_attack_absent_npc().
 *
 * Returns TRUE having printed; FALSE leaves the caller's own line.
 */
static scr_bool
lib_hit_arm_pre390 (scr_gameref_t game)
{
  static const scr_char *const VERBS[] =
      { "hit", "kill", "kick", "punch", "attack", nullptr };
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_int version = prop_get_taf_version (bundle);
  const scr_char *const *verb;
  scr_int npc;

  if (!input || version >= TAF_VERSION_390 || battle_is_enabled (game))
    return FALSE;

  for (verb = VERBS; *verb; verb++)
    if (run_c_word_pre400 (version, input, *verb) >= 0)
      break;
  if (!*verb)
    return FALSE;

  GS_FOR_EACH_NPC (game, npc)
    {
      std::string lowered (input);
      scr_int object, pick, with_at;
      scr_vartype_t vt_key[4];

      if (!lib_npc_referenced (game, npc, input))
        continue;

      if (!npc_in_room (game, npc, gs_playerroom (game)))
        {
          pf_buffer_string (filter, prop_get_indexed_string (bundle, "NPCs",
                                                             npc, "Name"));
          pf_buffer_string (filter, " is not here!\n");
          return TRUE;
        }

      if (run_c_word_pre400 (version, input, "with") < 0)
        {
          lib_print_npc_np (game, npc);
          pf_buffer_string (filter,
                            lib_select_response (game,
                                       " avoids your feeble attempts.\n",
                                       " avoids my feeble attempts.\n",
                                       " avoids %player%'s feeble attempts.\n"));
          return TRUE;
        }

      /* The Runner's line is lower-cased before the arm; InStr binary. */
      scr_lowercase (lowered);
      with_at = lowered.find ("with");

      pick = -1;
      GS_FOR_EACH_OBJECT (game, object)
        {
          const scr_char *shortname, *alias;
          scr_bool named, after;

          shortname = prop_get_indexed_string (bundle, "Objects",
                                               object, "Short");
          if (lib_alias_prepare (bundle, vt_key, "Objects", object) > 0)
            {
              vt_key[3].integer = 0;
              alias = prop_get_string (bundle, "S<-sisi", vt_key);
            }
          else
            alias = "";

          named = version == TAF_VERSION_380
                  ? lib_catch_all_names_pre390 (game, object)
                  : run_c_word_pre400 (version, input, shortname) >= 0
                    || (alias[0] != NUL
                        && run_c_word_pre400 (version, input, alias) >= 0);
          if (!named)
            continue;

          after = FALSE;
          if (shortname[0] != NUL)
            {
              std::string::size_type at = lowered.find (shortname);
              after = at != std::string::npos && (scr_int) at > with_at;
            }
          if (!after && alias[0] != NUL)
            {
              std::string::size_type at = lowered.find (alias);
              after = at != std::string::npos && (scr_int) at > with_at;
            }
          if (after)
            pick = object;
        }
      if (pick == -1)
        return FALSE;

      if (gs_object_position (game, pick) != OBJ_HELD_PLAYER)
        {
          lib_print_response_object (game, "You don't have ", "I don't have ",
                                     "%player% don't have ", pick, "!\n");
          return TRUE;
        }
      vt_key[0].string = "Objects";
      vt_key[1].integer = pick;
      vt_key[2].string = "Weapon";
      if (prop_get_boolean (bundle, "B<-sis", vt_key))
        {
          lib_print_response_npc (game, "You swing at ", "I swing at ",
                                  "%player% swing at ", npc, " with ");
          lib_print_object_np (game, pick);
          pf_buffer_string (filter,
                            lib_select_response (game, ", but you miss.\n",
                                                 ", but I miss.\n",
                                                 ", but misses.\n"));
        }
      else
        lib_print_wrapped_object (game, "I don't think ", pick,
                                  " would be a very effective weapon!\n");
      return TRUE;
    }
  return FALSE;
}


/*
 * lib_cmd_*()
 *
 * Shake, rattle and roll, and assorted nothing-happens handlers.
 */
scr_bool
lib_cmd_hit_object (scr_gameref_t game)
{
  if (lib_hit_arm_pre390 (game))
    return TRUE;
  return lib_nothing_happens_object (game, "hit", "hits");
}

scr_bool
lib_cmd_kick_object (scr_gameref_t game)
{
  if (lib_hit_arm_pre390 (game))
    return TRUE;
  return lib_nothing_happens_object (game, "kick", "kicks");
}

scr_bool
lib_cmd_press_object (scr_gameref_t game)
{
  if (lib_hit_arm_pre390 (game))
    return TRUE;
  return lib_nothing_happens_object (game, "press", "presses");
}

scr_bool
lib_cmd_push_object (scr_gameref_t game)
{
  if (lib_hit_arm_pre390 (game))
    return TRUE;
  return lib_nothing_happens_object (game, "push", "pushes");
}

scr_bool
lib_cmd_pull_object (scr_gameref_t game)
{
  if (lib_hit_arm_pre390 (game))
    return TRUE;
  return lib_nothing_happens_object (game, "pull", "pulls");
}

scr_bool
lib_cmd_shake_object (scr_gameref_t game)
{
  return lib_nothing_happens_object (game, "shake", "shakes");
}

scr_bool
lib_cmd_hit_other (scr_gameref_t game)
{
  if (lib_hit_arm_pre390 (game))
    return TRUE;
  return lib_nothing_happens_other (game, "hit", "hits");
}

scr_bool
lib_cmd_kick_other (scr_gameref_t game)
{
  if (lib_hit_arm_pre390 (game))
    return TRUE;
  return lib_nothing_happens_other (game, "kick", "kicks");
}

scr_bool
lib_cmd_press_other (scr_gameref_t game)
{
  if (lib_hit_arm_pre390 (game))
    return TRUE;
  return lib_nothing_happens_other (game, "press", "presses");
}

scr_bool
lib_cmd_push_other (scr_gameref_t game)
{
  if (lib_hit_arm_pre390 (game))
    return TRUE;
  return lib_nothing_happens_other (game, "push", "pushes");
}

scr_bool
lib_cmd_pull_other (scr_gameref_t game)
{
  if (lib_hit_arm_pre390 (game))
    return TRUE;
  return lib_nothing_happens_other (game, "pull", "pulls");
}

scr_bool
lib_cmd_shake_other (scr_gameref_t game)
{
  return lib_nothing_happens_other (game, "shake", "shakes");
}


/*
 * lib_cant_do_common()
 * lib_cant_do_object()
 * lib_cant_do_other()
 *
 * Central handler for a range of can't-do messages.  Yet more uninterest-
 * ing responses.  particle follows the object name: run400's therest turn
 * arm composes " can't turn " & <that|name> & " off" when the line holds
 * the whole word "off", else " on" for "on", else nothing (the_pk_girl
 * runner_probes/thepkgirl.run400.site.txt turn 362: `turn on transmitter`
 * -> "You can't turn that on.").  run370/380/390 have the one plain arm.
 */
static scr_bool
lib_cant_do_common (scr_gameref_t game, const scr_char *verb,
                    scr_bool is_object, const scr_char *particle)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  scr_bool is_ambiguous, handled;
  const scr_bool status = lib_cant_do_with_400 (game, verb, particle,
                                                &handled);

  if (handled)
    return status;

  /* If the target is not an object, end it here. */
  if (!is_object)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You can't ",
                                             "I can't ", "%player% can't "));
      pf_buffer_string (filter, verb);
      pf_buffer_string (filter, " that");
      pf_buffer_string (filter, particle);
      pf_buffer_string (filter, ".\n");
      return TRUE;
    }

  /* Get the referenced object.  If none, return immediately. */
  object = lib_disambiguate_object (game, verb, &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Whatever it is, don't do it. */
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You can't ",
                                         "I can't ", "%player% can't "));
  pf_buffer_string (filter, verb);
  pf_buffer_character (filter, ' ');
  lib_print_object_np (game, object);
  pf_buffer_string (filter, particle);
  pf_buffer_string (filter, ".\n");
  return TRUE;
}

scr_bool
lib_cant_do_object (scr_gameref_t game, const scr_char *verb)
{
  return lib_cant_do_common (game, verb, TRUE, "");
}

scr_bool
lib_cant_do_other (scr_gameref_t game, const scr_char *verb)
{
  return lib_cant_do_common (game, verb, FALSE, "");
}

/* The 4.0 turn arm's " off" / " on" particle; see lib_cant_do_common(). */
static const scr_char *
lib_turn_particle (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();

  if (!input || !lib_is_version_400 (game))
    return "";
  if (lib_input_contains_word (input, "off"))
    return " off";
  if (lib_input_contains_word (input, "on"))
    return " on";
  return "";
}


/*
 * lib_cmd_*()
 *
 * Assorted can't-do messages.
 */
scr_bool
lib_cmd_turn_object (scr_gameref_t game)
{
  return lib_cant_do_common (game, "turn", TRUE, lib_turn_particle (game));
}

/*
 * lib_open_close_resolved_400()
 *
 * run400's openclose resolves its object with Proc_21_58_463640 over the
 * whole typed line, not with the name the parser bound: a unique
 * present-and-seen winner is opened or closed even when the line also names
 * an absent object in full.  xfiles T62 `open phone book`, the phone book
 * left in the motel room and the cell phone (alias "Phone") held, answers
 * "Your Cell Phone is already open!" (runner_transcripts/xfiles.txt).  TRUE
 * when the line was taken, with *status the handler's return.
 */
static scr_bool
lib_open_close_resolved_400 (scr_gameref_t game,
                             scr_bool (*handler) (scr_gameref_t),
                             scr_bool *status)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int object;

  if (!lib_is_version_400 (game) || !input || strstr (input, " with "))
    return FALSE;

  object = lib_verb_object_resolve_400_string (game, input, nullptr, TRUE);
  if (object < 0)
    return FALSE;

  gs_clear_object_references (game);
  game->refs.objects[object] = TRUE;
  *status = handler (game);
  return TRUE;
}

/*
 * lib_bare_verb_asks()
 *
 * A bare `open`, `close`, `lock` or `unlock` names nothing, yet every Runner
 * refuses it with "You can't open that." (see lib_cmd_open_other()).
 * Deliberate deviation (WINE-TRANSCRIPTS.md): Scarier asks "<Verb> what?",
 * as SCARE did, unless an ALR restyles the refusal.
 */
static scr_bool
lib_bare_verb_asks (scr_gameref_t game, const scr_char *verb,
                    const scr_char *question_verb)
{
  const scr_char *input = run_get_dispatch_input ();

  if (!input || scr_strcasecmp (input, verb) != 0)
    return FALSE;
  const std::string refusal = std::string ("can't ") + verb + " that";
  if (pf_alr_mentions (gs_get_bundle (game), refusal.c_str ()))
    return FALSE;
  return lib_what (game, question_verb);
}

scr_bool
lib_cmd_close_other (scr_gameref_t game)
{
  scr_bool status;

  if (lib_bare_verb_asks (game, "close", "Close"))
    return TRUE;
  if (lib_open_close_resolved_400 (game, lib_cmd_close_object, &status))
    return status;
  return lib_cant_do_other (game, "close");
}

/*
 * lib_cmd_open_absent()
 * lib_cmd_close_absent()
 *
 * 4.0's openclose() answers for an object it has seen but cannot see now --
 * the open half with the definite name, the close half with the object's own
 * Prefix.  See lib_absent_seen_object().
 *
 * Pre-3.9 splits by version instead: 3.8's openclose() names a seen object
 * by its Prefix in both halves and answers "Open what?"/"Close what?" for an
 * unseen one, and 3.7's therest() names either with the definite form.  See
 * lib_absent_named_object_pre_390().
 */
static scr_bool
lib_open_close_absent_pre_390 (scr_gameref_t game, const scr_char *what)
{
  const scr_int object = lib_absent_named_object_pre_390 (game);

  if (object == -1)
    return FALSE;

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380)
    return lib_cant_see_named_pre_390 (game, object, TRUE, ".\n");

  if (gs_object_seen (game, object))
    return lib_cant_see_named_pre_390 (game, object, FALSE, ".\n");

  return lib_print_message (game, what);
}

scr_bool
lib_cmd_open_absent (scr_gameref_t game)
{
  return lib_open_close_absent_pre_390 (game, "Open what?\n")
         || lib_cant_see_absent_object (game, ".\n", TRUE);
}

scr_bool
lib_cmd_close_absent (scr_gameref_t game)
{
  return lib_open_close_absent_pre_390 (game, "Close what?\n")
         || lib_cant_see_absent_object (game, ".\n", FALSE);
}

/*
 * lib_cmd_open_other()
 *
 * `open <anything the parser could not resolve to an object>`.  SCARE routed
 * this to lib_what(), for "Open what?", but no Runner has ever printed that
 * for an open: every version composes the same flat refusal its `close` twin
 * does.  Measured, not argued, on the examine-refusal probes:
 *
 *   run390, p39EXAM.taf (3.9), runner_probes/exam.run390.txt /
 *     runner_probes/exam.run390.held.txt --
 *     bare `open`, `open door` (a noun no object bears) and `open statue`
 *     (an object seen in another room, absent from this one) all answer
 *     "You can't open that."
 *   run400, p4EXAM.taf (4.0), runner_probes/exam.run400.txt -- bare
 *     `open` and `open door` answer "You can't open that." too.  (4.0's
 *     `open statue` answers "You can't see the statue." instead, but that
 *     is the 4.0 absent-object resolver speaking one layer up, not this
 *     handler.)
 *
 * So this is not a version split: it is scrunner.cpp's `open *` row having
 * been asymmetric with the `close *` row sitting directly beneath it.
 * "Open what?" is in run380/390/400's constant pools, but no probe row has
 * ever reached it.
 */
scr_bool
lib_cmd_open_other (scr_gameref_t game)
{
  scr_bool status;

  if (lib_bare_verb_asks (game, "open", "Open"))
    return TRUE;
  if (lib_open_close_resolved_400 (game, lib_cmd_open_object, &status))
    return status;
  return lib_cant_do_other (game, "open");
}

/*
 * lib_cmd_open_ended_400()
 * lib_cmd_close_ended_400()
 *
 * `open *` / `close *` once a task has ended the game on the line.  run400's
 * openclose (Proc_19_3_476468, called at 48A515) sits above the gameover jump
 * and still answers for an object it resolves, but with none it leaves
 * silently (4756BC).  "You can't open that." is therest's arm, below the
 * jump, so the line falls to DontUnderstand: haremprologue T59 `open door`,
 * task 105 ending the game with no "door" object, answers "I don't
 * understand what you mean!" (runner_transcripts/haremprologue.txt).
 */
scr_bool
lib_cmd_open_ended_400 (scr_gameref_t game)
{
  scr_bool status;

  if (lib_open_close_resolved_400 (game, lib_cmd_open_object, &status))
    return status;
  return FALSE;
}

scr_bool
lib_cmd_close_ended_400 (scr_gameref_t game)
{
  scr_bool status;

  if (lib_open_close_resolved_400 (game, lib_cmd_close_object, &status))
    return status;
  return FALSE;
}

/*
 * 4.0: a lock or unlock line naming a present object the arm declined (see
 * lib_lock_backend()) goes on to the object catch-all, not to "You can't
 * unlock that.".
 */
scr_bool
lib_cmd_lock_other (scr_gameref_t game)
{
  scr_bool handled;
  scr_bool status;

  if (lib_bare_verb_asks (game, "lock", "Lock"))
    return TRUE;
  if (lib_lock_absent_object_400 (game) >= 0)
    return lib_lock_backend (game, &LIB_LOCK_VERB, FALSE);
  status = lib_cant_do_with_400 (game, "lock", "", &handled);
  if (handled)
    return status;
  if (lib_is_version_400 (game) && lib_verb_object_resolve_400 (game) >= 0)
    return FALSE;
  return lib_cant_do_other (game, "lock");
}

scr_bool
lib_cmd_unlock_other (scr_gameref_t game)
{
  scr_bool handled;
  scr_bool status;

  if (lib_bare_verb_asks (game, "unlock", "Unlock"))
    return TRUE;
  if (lib_lock_absent_object_400 (game) >= 0)
    return lib_lock_backend (game, &LIB_UNLOCK_VERB, FALSE);
  status = lib_cant_do_with_400 (game, "unlock", "", &handled);
  if (handled)
    return status;
  if (lib_is_version_400 (game) && lib_verb_object_resolve_400 (game) >= 0)
    return FALSE;
  return lib_cant_do_other (game, "unlock");
}

/*
 * lib_cmd_lock_object_pre_400()
 * lib_cmd_unlock_object_pre_400()
 *
 * Pre-4.0 therest's lock and unlock arms: " can't unlock " & <the object, or
 * "that"> & <the " with " suffix> & ".", with no test of the object's
 * openness or key (see lib_lock_backend(), which declines below 4.0).  They
 * sit in STANDARD_FALLBACK_COMMANDS, below the room refusal.
 */
scr_bool
lib_cmd_lock_object_pre_400 (scr_gameref_t game)
{
  if (lib_is_version_400 (game))
    return FALSE;
  return lib_cant_do_object (game, "lock");
}

scr_bool
lib_cmd_unlock_object_pre_400 (scr_gameref_t game)
{
  if (lib_is_version_400 (game))
    return FALSE;
  return lib_cant_do_object (game, "unlock");
}

scr_bool
lib_cmd_stand_other (scr_gameref_t game)
{
  if (lib_checkverb_bare_400 (game, "stand on", "Stand on")
      || lib_checkverb_bare_400 (game, "stand in", "Stand in"))
    return TRUE;
  return lib_cant_do_other (game, "stand on");
}

scr_bool
lib_cmd_sit_other (scr_gameref_t game)
{
  if (lib_checkverb_bare_400 (game, "sit on", "Sit on")
      || lib_checkverb_bare_400 (game, "sit in", "Sit in"))
    return TRUE;
  return lib_cant_do_other (game, "sit on");
}

scr_bool
lib_cmd_lie_other (scr_gameref_t game)
{
  if (lib_checkverb_bare_400 (game, "lie on", "Lie on")
      || lib_checkverb_bare_400 (game, "lie in", "Lie in")
      || lib_checkverb_bare_400 (game, "lay on", "Lay on")
      || lib_checkverb_bare_400 (game, "lay in", "Lay in"))
    return TRUE;
  return lib_cant_do_other (game, "lie on");
}

scr_bool
lib_cmd_turn_other (scr_gameref_t game)
{
  return lib_cant_do_common (game, "turn", FALSE, lib_turn_particle (game));
}


/*
 * lib_dont_think_common()
 * lib_dont_think_object()
 * lib_dont_think_other()
 *
 * Central handler for a range of don't_think messages.  Still more
 * uninteresting responses.
 */
static scr_bool
lib_dont_think_common (scr_gameref_t game,
                       const scr_char *verb, scr_bool is_object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, instrument;
  scr_bool is_ambiguous;

  /*
   * 4.0's fix, repair and mend arms (therest 489BE7) end in the " with <the
   * X>" suffix of lib_with_clause_400(): "I don't think you can fix the rope
   * with the knife." (p4WITHQ2.taf, runner_probes/withq2.run400.txt).
   */
  switch (lib_with_clause_400 (game, &object, &instrument))
    {
    case LIB_WITH_NONE:
      break;
    case LIB_WITH_DECLINE:
      return FALSE;
    case LIB_WITH_ANSWERED:
      return TRUE;
    case LIB_WITH_SUFFIX:
      pf_buffer_string (filter, "I don't think you can ");
      pf_buffer_string (filter, verb);
      pf_buffer_character (filter, ' ');
      lib_print_object_np (game, object);
      lib_print_wrapped_object (game, " with ", instrument, ".\n");
      return TRUE;
    }

  /* If the target is not an object, end it here. */
  if (!is_object)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "I don't think you can ",
                                             "I don't think I can ",
                                             "I don't think %player% can "));
      pf_buffer_string (filter, verb);
      pf_buffer_string (filter, " that.\n");
      return TRUE;
    }

  /* Get the referenced object.  If none, return immediately. */
  object = lib_disambiguate_object (game, verb, &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Whatever it is, don't do it. */
  pf_buffer_string (filter, "I don't think you can ");
  pf_buffer_string (filter, verb);
  pf_buffer_character (filter, ' ');
  lib_print_object_np (game, object);
  pf_buffer_string (filter, ".\n");
  return TRUE;
}

static scr_bool
lib_dont_think_object (scr_gameref_t game, const scr_char *verb)
{
  return lib_dont_think_common (game, verb, TRUE);
}

static scr_bool
lib_dont_think_other (scr_gameref_t game, const scr_char *verb)
{
  return lib_dont_think_common (game, verb, FALSE);
}


/*
 * lib_cmd_*()
 *
 * Assorted don't-think messages.
 */
scr_bool
lib_cmd_fix_object (scr_gameref_t game)
{
  return lib_dont_think_object (game, "fix");
}

scr_bool
lib_cmd_mend_object (scr_gameref_t game)
{
  return lib_dont_think_object (game, lib_fix_verb_pre390 (game, "mend"));
}

scr_bool
lib_cmd_repair_object (scr_gameref_t game)
{
  return lib_dont_think_object (game, lib_fix_verb_pre390 (game, "repair"));
}

scr_bool
lib_cmd_fix_other (scr_gameref_t game)
{
  return lib_dont_think_other (game, "fix");
}

scr_bool
lib_cmd_mend_other (scr_gameref_t game)
{
  return lib_dont_think_other (game, lib_fix_verb_pre390 (game, "mend"));
}

scr_bool
lib_cmd_repair_other (scr_gameref_t game)
{
  return lib_dont_think_other (game, lib_fix_verb_pre390 (game, "repair"));
}


/*
 * lib_what()
 *
 * Central handler for a verb with no object: asks "<Verb> what?".
 */
scr_bool
lib_what (scr_gameref_t game, const scr_char *verb)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();

  /*
   * checkverb's bare verb leaves the line pending.  At 4.0 drop, take and
   * drink are not checkverb verbs and leave nothing (see
   * lib_question_with_rule()).
   *
   * 3.9 is broader: EVERY "<Verb> what?" answer leaves the line in
   * MemVar_4681D0, which generaltasks puts in front of the next line nothing
   * else answers -- checkverb's arms (push, pull, kick, hit, turn, climb,
   * break, lock, smash ...) and the handlers' own rows alike: `drop` / `coin`
   * drops the coin, `take` / `coin` picks it up, `wear` / `red hat` puts it
   * on, `remove` / `red hat` takes it off, `examine` or `x` / `stone`
   * examines it, `give` / `coin` asks "Give the coin to who?".  The prefix
   * lives one line: `push` / `look` / `stone` is the catch-all (generaltasks
   * clears a prefix the answered line left alone).  The splitter's next
   * element is such a line too: `push, stone` is "Push what?" then "You push
   * the stone.".  p39TASK, run390x runner_probes/task.run390.pfx.txt
   * (harness/make_prefixprobe.py), runner_probes/task.run390.comma.txt
   * (harness/make_3738_taskprobe.py).
   *
   * What checkverb compares is the line as typed, but what it stores is
   * the line as generaltasks holds it by then -- after its bare-give
   * completion.  So `give` prints "(to Nobody)" and "Give what?" and stores
   * "give to nobody"; `coin` then reruns "give to nobody coin", which has
   * its "to" and is not completed again: no second echo, and the give
   * handler asks "Give the coin to who?" (runner_probes/task.run390.pfx.txt
   * T44-45).
   */
  if (input && lib_is_version_390 (game))
    {
      const scr_char *typed = run_get_line_input ();

      /*
       * The four 3.9 object handlers store the line AS TYPED, not only a line
       * that is the bare verb: takes, drops, wears and removes all end with
       * `If msg = "" Then msg = "<Verb> what?" : MemVar_4681D0 = the line`.
       * checkverb's own arms keep the bare-verb test, which is why `push
       * zzz` stores nothing while `wear zzz` does.  Measured p39WHAT (run390x
       * runner_probes/what.run390.txt, harness/make_39_whatprobe.py): `wear
       * zzz` / `hat` puts the hat on, `remove zzz` / `hat` takes it off,
       * `drop zzz` / `hat` drops it, `take zzz` / `hat` picks it up, and
       * `wear zzz` / `wield zzz` answers "Wear what?" a second time before
       * `hat` wears it.  The prefix still lives exactly one line: `drop zzz`
       * / `look` / `hat` is the catch-all.
       *
       * therest's give arm is a fifth: its "Give what?" is followed by the
       * same unconditional `MemVar_4681D0 = MemVar_468118`, with no bare-verb
       * test above it.  What it stores is the line the bare-give completion
       * has already finished, so `blorp give` stores "blorp give to nobody"
       * and the next line runs on from there: it has its "to", is not
       * completed again, and prints no second "(to Nobody)".  That is the
       * whole of run390's `blorp give` / `blorp put` pair -- "Give what?"
       * twice, the second time with no echo -- and `put` names no route of
       * its own (runner_probes/rew.run390.casc.txt,
       * harness/make_rewriteprobe.py; the only "Give what?" in the run390
       * P-code is 45D70B).
       */
      if (strcmp (verb, "Take") == 0 || strcmp (verb, "Drop") == 0
          || strcmp (verb, "Wear") == 0 || strcmp (verb, "Remove") == 0
          || strcmp (verb, "Give") == 0)
        lib_battle_who_pending = input;
      else if (typed && scr_strcasecmp (typed, verb) == 0)
        lib_battle_who_pending = input;
    }
  else if (input && scr_strcasecmp (input, verb) == 0)
    {
      /*
       * Deliberate deviation (WINE-TRANSCRIPTS.md): 4.0's bare drop, take
       * and drink, and every bare verb below 3.9, store nothing, so `take`
       * / `coin` is the object catch-all though the game has just asked
       * "Take what?".  Scarier keeps the bare verb as 3.9 does; the prefix
       * still only completes a line nothing else answers.
       */
      if (lib_is_version_400 (game)
          && strcmp (verb, "Drop") != 0 && strcmp (verb, "Take") != 0
          && strcmp (verb, "Drink") != 0)
        lib_question_prefix_from_line (game);
      else
        lib_battle_who_store (input);
    }

  /*
   * 3.7's takes() and drops() answer the line here when no namesake of the
   * typed term keeps its Prefix word, and the answer stands: the Runner's
   * `take very gem` over gems Prefixed "a very red" and "a very blue" is
   * "Take what?", not co()'s question (p37TAKEQ,
   * runner_probes/takeq.run370.rtf).  When the matcher never bound
   * %object% at all -- the adjective is not a Prefix word, so nothing matched
   * -- this is the only site that sees the line, so the end-of-turn co()
   * prompt is blocked from here as well as from
   * lib_disambiguate_object_common().
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380
      && (strcmp (verb, "Take") == 0 || strcmp (verb, "Drop") == 0))
    disambig.line.prompt_370_blocked = TRUE;

  pf_buffer_string (filter, verb);
  pf_buffer_string (filter, " what?\n");
  return TRUE;
}


/*
 * lib_cmd_*()
 *
 * Assorted "what?" messages.
 */
/*
 * lib_what_or_other()
 *
 * The bare form of most refused verbs: the version's "<Verb> what?" from
 * 3.9 (see lib_what()), and the verb's own catch-all reply below it.
 */
static scr_bool
lib_what_or_other (scr_gameref_t game, const scr_char *verb,
                   scr_bool (*other) (scr_gameref_t game))
{
  if (lib_bare_verb_pre390 (game))
    return other (game);
  return lib_what (game, verb);
}

/*
 * lib_what_or_cant_do()
 *
 * lib_what_or_other() for the verbs whose catch-all is the plain can't-do
 * reply, as a command-table row's handler.  The row's argument is the verb
 * in lower case, as lib_cant_do_other() prints it; "<Verb> what?" wants it
 * capitalised.
 */
scr_bool
lib_what_or_cant_do (scr_gameref_t game, const scr_char *verb)
{
  std::string asked (verb);

  if (lib_bare_verb_pre390 (game))
    return lib_cant_do_other (game, verb);

  asked[0] = scr_toupper (asked[0]);
  return lib_what (game, asked.c_str ());
}

scr_bool
lib_cmd_break_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Break", lib_cmd_break_other);
}

scr_bool
lib_cmd_destroy_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Destroy", lib_cmd_break_other);
}

scr_bool
lib_cmd_smash_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Smash", lib_cmd_break_other);
}

scr_bool
lib_cmd_buy_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Buy", lib_cmd_buy_other);
}

scr_bool
lib_cmd_drink_what (scr_gameref_t game)
{
  /*
   * Below 4.0 therest's drink arm is a plain `If c("drink") Then msg = Ary(0)
   * & " can't drink " & name & "."` (run370 43D398 ... run390 45D64F) with
   * no checkverb, so a bare `drink` is "You can't drink that." and leaves
   * no prefix (`drink` / `stone` is the catch-all).  p39TASK run390x
   * runner_probes/task.run390.pfx.txt.
   */
  if (!lib_is_version_400 (game))
    return lib_cant_do_other (game, "drink");
  return lib_what (game, "Drink");
}

scr_bool
lib_cmd_fix_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Fix", lib_cmd_fix_other);
}

/*
 * run370 and run380 have no checkverb, and no "Hit what?" literal: a bare
 * `hit` is therest's hit arm with no object, "You hit, but nothing
 * happens." (run370x/run380x runner_probes/npcamb.run3[78]0.kill.rtf).
 */
scr_bool
lib_cmd_hit_what (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return lib_cmd_hit_other (game);
  return lib_what (game, "Hit");
}

scr_bool
lib_cmd_kick_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Kick", lib_cmd_kick_other);
}

scr_bool
lib_cmd_mend_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Mend", lib_cmd_mend_other);
}

scr_bool
lib_cmd_press_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Press", lib_cmd_press_other);
}

scr_bool
lib_cmd_pull_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Pull", lib_cmd_pull_other);
}

scr_bool
lib_cmd_push_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Push", lib_cmd_push_other);
}

scr_bool
lib_cmd_repair_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Repair", lib_cmd_repair_other);
}

scr_bool
lib_cmd_sell_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Sell", lib_cmd_sell_other);
}

scr_bool
lib_cmd_shake_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Shake", lib_cmd_shake_other);
}

scr_bool
lib_cmd_turn_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Turn", lib_cmd_turn_other);
}

/*
 * 4.0's put/drop list parser (run400 Proc_19_40_459DB4, entered when the
 * line holds the whole word "put" or "drop") names each piece with
 * name_object (Proc_19_41_46E5D8), which resolves it against the objects
 * PRESENT (463640 mode 2) and, when that finds nothing and no task pattern
 * pre-matches the line (453C50), speaks: "Drop what?" if the line holds the
 * whole word "drop", else "It is not clear which object you are referring
 * to."  Pre-4.0 Runners have no such literal; their `put <absent> in X`
 * stays the flat can't-do tail.  Measured on humbug
 * (runner_probes/humbug.run400.b.txt 3407 `Put powder in chute`, powder
 * never taken, the D chute present; 3538 `Put powder in machine`; 3903 `Put
 * sapphire in chute`).  A present container with an unknown first noun gets
 * this too (p4PUT `put zzz in box`, runner_probes/put.run400.txt); an
 * unknown or absent CONTAINER is answered earlier, by
 * lib_cmd_put_container_400().
 */
scr_bool
lib_cmd_unclear_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter,
                    "It is not clear which object you are referring to.\n");
  return TRUE;
}

/*
 * lib_first_named_object_pre_390()
 *
 * The pre-3.9 drop and remove handlers match their noun with co() (run380
 * @42DE60), which tests every object's Short and alias against the line
 * with no regard to where the object is, and then name the first match in
 * index order that they cannot act on (see lacks_pre_390 in the move verb
 * table).  So `drop cash` with the penny (alias "cash") shut in a dresser
 * two rooms away answers "You don't have a penny!" rather than "Drop what?",
 * which run380 keeps (@438FE6) for a line naming nothing at all.  Measured
 * on Crime_Adventure.taf (3.8) in run380.  Returns the first
 * object the line names below 3.9, else -1.
 */
static scr_int
lib_first_named_object_pre_390 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int object;

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return -1;
  if (!input)
    return -1;
  if (!uip_match ("* %object%", input, game)
      && !uip_match ("* %object% *", input, game))
    return -1;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if (game->refs.objects[object])
        return object;
    }
  return -1;
}

/*
 * lib_seen_named_object_400()
 *
 * 4.0's remove and drop resolver (Proc_21_58_463640) falls back, when
 * nothing PRESENT (or worn/held) answers, to every object the player has
 * SEEN, scored the same whole-word way as lib_take_absent_score() above,
 * and only a genuine unique best score counts -- a tie is left to the
 * plain "what?" the same as no match at all.  Returns the unique winner's
 * index, or -1 for none or a tie.
 */
scr_int
lib_seen_named_object_400 (scr_gameref_t game, const scr_char *input)
{
  scr_int object, best_score, best_count, best_object;

  best_score = 0;
  best_count = 0;
  best_object = -1;

  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *term;
      scr_int score;

      if (!gs_object_seen (game, object))
        continue;

      score = lib_take_absent_score (game, object, input, &term);
      if (score == 0)
        continue;

      if (score > best_score)
        {
          best_score = score;
          best_count = 1;
          best_object = object;
        }
      else if (score == best_score)
        best_count++;
    }

  return (best_count == 1) ? best_object : -1;
}

/*
 * lib_cmd_drop_absent_pre390()
 *
 * The pre-3.9 half of lib_cmd_drop_what(), run above the out-of-room task
 * refusal rather than below it.  run380's drops() sets its "You don't have"
 * message and only then calls tasks(), whose " can't do that here." is
 * written only into an empty message.  Measured on cave.taf (3.8) under
 * run380: `drop robot` up the tree, with task 83 `drop robot` confined to
 * room 17 and the toy robot never held, is "You don't have a toy robot!"
 * (runner_probes/cave.run380.rtf turn 114), where Scarier said "You can't do
 * that here.".
 */
scr_bool
lib_cmd_drop_absent_pre390 (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int object = lib_first_named_object_pre_390 (game);

  if (object == -1)
    return FALSE;

  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You don't have ",
                                         "I don't have ",
                                         "%player% don't have "));
  lib_print_object_raw (game, object);
  pf_buffer_string (filter, "!\n");
  return TRUE;
}

scr_bool
lib_cmd_drop_what (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int object;

  if (lib_is_version_400 (game)
      && input && !lib_input_contains_word (input, "drop"))
    return lib_cmd_unclear_object (game);

  if (lib_cmd_drop_absent_pre390 (game))
    return TRUE;

  /*
   * name_object stayed silent for a drop that named nothing with a task
   * pre-matching (see lib_drop_named_400()); the catch-all answers.
   */
  if (lib_is_version_400 (game) && run_priority_put_was_unnamed ())
    return FALSE;

  /*
   * 4.0 seen-absent unique winner: "You are not holding the uniform."
   * (definite form).  Measured escape_to_new_york turn 156 `drop uniform`
   * (Ticket run400 xoshiro trace 2026-09-12); reached via put_drop_list
   * 459DB4 -> name_object 46E5D8 -> Proc_21_58_463640.  Unseen, or a tie,
   * stays "Drop what?".
   */
  if (lib_is_version_400 (game) && input)
    {
      object = lib_seen_named_object_400 (game, input);
      if (object != -1)
        {
          lib_print_response_object (game,
                                     "You are not holding ",
                                     "I am not holding ",
                                     "%player% is not holding ",
                                     object, ".\n");
          return TRUE;
        }
    }

  return lib_what (game, "Drop");
}

/* The `leave` spellings of the two rows above; see lib_cmd_leave_all_pre400(). */
scr_bool
lib_cmd_leave_absent_pre400 (scr_gameref_t game)
{
  return !lib_is_version_400 (game) && lib_cmd_drop_absent_pre390 (game);
}

scr_bool
lib_cmd_leave_what_pre400 (scr_gameref_t game)
{
  return !lib_is_version_400 (game) && lib_cmd_drop_what (game);
}

scr_bool
lib_cmd_put_unclear (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int index_;

  if (!lib_is_version_400 (game) || !input)
    return FALSE;

  /*
   * A put line with no preposition never reaches the refusal at all: the
   * put-where branch sits ABOVE the refusal in name_object, and it takes
   * every line holding the whole word "put" with neither " in " nor " on "
   * and no "down".  So an unresolvable noun there is "Where do you want to
   * put that?", not this refusal -- which is the 46DD19 arm the branch's
   * own comment already names, reached here for the first time.  Measured
   * on p4REW: run400 answers `blorp put` "Where do you want to put that?"
   * (runner_probes/rew.run400.casc.txt), and put_drop_list enters on the
   * whole word, so `put blorp` walks the same path.  Left to
   * lib_cmd_put_where_400(), further down the standard table.
   */
  if (lib_is_put_where_line_400 (game))
    return FALSE;

  /*
   * name_object already stayed silent for this line: its direct object named
   * nothing and a put/drop-class task pre-matched it (46E15A), so the tasks
   * ran on the clobbered fragment and the catch-all answers, whichever noun
   * %object% would bind below.  House (runner_probes/house_sober.run400.txt
   * T263): `put thyme in kettle` with task 368 `put thyme in kettle` failing
   * its not-holding restriction is "I don't understand what you want me to do
   * with the large cast iron kettle.", the same as T264 `put web in kettle`;
   * "thyme" names an unseen object, which must not reach the refusal here.
   */
  if (run_priority_put_was_unnamed ())
    return FALSE;

  /*
   * A first noun that names something present is name_object's success
   * path, and the command belongs to the put handlers and the "I don't
   * understand what you want me to do with" catch-all below (measured:
   * TheADRIFTProject `put batter in remote`, thelasthour `put bowl near
   * spyhole`, both with the first object held).
   */
  if (uip_match ("put %object% *", input, game)
      || uip_match ("put %object%", input, game))
    {
      GS_FOR_EACH_OBJECT (game, index_)
        {
          if (game->refs.objects[index_]
              && obj_indirectly_in_room (game, index_, gs_playerroom (game)))
            return FALSE;
        }
    }

  /*
   * A seen-but-absent noun is left to the "You can't see" clause.  That is
   * only ever the FIRST noun by the time the line gets here: a container
   * the player has seen but left behind is answered by
   * lib_cmd_put_container_400() ("I don't understand what you want to put
   * things inside.", p4PUT2 `put coin in bag` from the next room,
   * runner_probes/put2.run400.txt), and a PRESENT container with an unknown
   * first noun is this refusal (p4PUT `put zzz in box` -> "It is not clear
   * which object you are referring to.", a turn,
   * runner_probes/put.run400.txt).  Whether a seen-but-absent
   * first noun really gets the can't-see clause is unmeasured (humbug's
   * powder had never been seen); it is kept for that noun alone.
   */
  if (uip_match ("* %object% *", input, game)
      || uip_match ("* %object%", input, game))
    {
      GS_FOR_EACH_OBJECT (game, index_)
        {
          if (game->refs.objects[index_]
              && gs_object_seen (game, index_)
              && !obj_indirectly_in_room (game, index_, gs_playerroom (game)))
            return FALSE;
        }
    }

  return lib_cmd_unclear_object (game);
}

/*
 * lib_take_scored_400()
 *
 * 4.0 names a take's object by whole-word score, so a line whose words did
 * not all parse can still name one; see lib_take_multiple_common().  Also
 * run by run_all_commands() for get_outer's take ahead of the tasks.
 */
scr_bool
lib_take_scored_400 (scr_gameref_t game)
{
  scr_bool status;

  if (!lib_is_version_400 (game)
      || !uip_match ("[get/take/pick up/pick] %text%",
                     run_get_dispatch_input (), game))
    return FALSE;

  lib_take_scored_fallback = TRUE;
  status = lib_take_multiple_common (game, FALSE);
  lib_take_scored_fallback = FALSE;
  return status;
}

/*
 * lib_take_names_dynamic_400()
 *
 * Does the whole-word noun scorer name a present, non-static object on
 * STRING?  get_piece names its piece with 463640 in mode 1, whose
 * candidates must be dynamic (global_24 = 0).  A line naming only statics
 * -- Pilfers' `get off bed`, Glum Fiddle's `get in barrel` -- keeps the
 * flow below, where the tasks and the get-off handler answer it.
 */
scr_bool
lib_take_names_dynamic_400 (scr_gameref_t game, const scr_char *string)
{
  const scr_int object = lib_verb_object_resolve_400_string (game, string,
                                                             nullptr, TRUE);

  return object >= 0 && !obj_is_static (game, object);
}

scr_bool
lib_cmd_get_what (scr_gameref_t game)
{
  if (lib_take_declines_get_motion (game))
    return FALSE;
  if (lib_take_scored_400 (game))
    return TRUE;
  return lib_what (game, "Take");
}

scr_bool
lib_cmd_give_what (scr_gameref_t game)
{
  if (lib_give_defer_catch_all)
    {
      lib_give_defer_catch_all = FALSE;
      return FALSE;
    }

  /*
   * run370/run380 have no "Give what?": therest's give arm names what it
   * found, and with nothing that is "that" (run380 443EF1), so a bare
   * `give` is "(to Nobody) You don't have that."  run370x/run380x
   * runner_probes/npcamb.run3[78]0.bareverb.rtf.
   */
  if (lib_bare_verb_pre390 (game))
    {
      pf_buffer_string (gs_get_filter (game),
                        lib_select_response (game,
                                             "You don't have that.\n",
                                             "I don't have that.\n",
                                             "%player% don't have that.\n"));
      return TRUE;
    }
  lib_question_prefix_from_line (game);
  return lib_what (game, "Give");
}

scr_bool
lib_cmd_remove_what (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int object;

  /* run380 @4300C8, the same first-named-object rule as drop above. */
  object = lib_first_named_object_pre_390 (game);
  if (object != -1)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not wearing ",
                                             "I am not wearing ",
                                             "%player% is not wearing "));
      lib_print_object_raw (game, object);
      pf_buffer_string (filter, "!\n");
      return TRUE;
    }

  /*
   * 4.0 seen-absent unique winner: "You are not wearing the uniform!"
   * (definite form).  Measured escape_to_new_york turn 155 `remove uniform`
   * (Ticket run400 xoshiro trace 2026-09-12); remove calls
   * Proc_21_58_463640(obnum, 3, 0), then its "not wearing" arms, and
   * "Remove what?" only when that message came back empty.  Unseen, or a
   * tie, stays "Remove what?".
   */
  if (lib_is_version_400 (game) && input)
    {
      object = lib_seen_named_object_400 (game, input);
      if (object != -1)
        {
          lib_print_response_object (game,
                                     "You are not wearing ",
                                     "I am not wearing ",
                                     "%player% is not wearing ",
                                     object, "!\n");
          return TRUE;
        }
    }

  lib_question_prefix_from_line (game);
  return lib_what (game, "Remove");
}

scr_bool
lib_cmd_wear_what (scr_gameref_t game)
{
  /* Pre-3.9 wears() refuses a match anywhere in the world as not held
   * (run380); see lib_absent_named_object_pre_390(). */
  const scr_int object = lib_absent_named_object_pre_390 (game);

  /*
   * From 3.8 the put branch answered above wears(); see
   * lib_wear_is_put_line_380().
   */
  if (lib_wear_is_put_line_380 (game))
    return FALSE;

  if (object != -1)
    {
      const scr_filterref_t filter = gs_get_filter (game);

      pf_buffer_string (filter,
                        lib_select_response (game, "You are not holding ",
                                             "I am not holding ",
                                             "%player% is not holding "));
      lib_print_object_raw (game, object);
      pf_buffer_string (filter, ".\n");
      return TRUE;
    }

  lib_question_prefix_from_line (game);
  return lib_what (game, "Wear");
}

scr_bool
lib_cmd_lock_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Lock", lib_cmd_lock_other);
}

scr_bool
lib_cmd_unlock_what (scr_gameref_t game)
{
  return lib_what_or_other (game, "Unlock", lib_cmd_unlock_other);
}
