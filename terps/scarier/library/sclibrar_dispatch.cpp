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
 * Retrying library commands as game tasks, and parsing "all"/"except"
 * multiple-object lists.
 *
 * See sclibrar.h for what the library files share and sclibrar_internal.h
 * for what the core files share.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"

/*
 * lib_save_object_references()
 * lib_restore_object_references()
 *
 * Helpers for trying game commands.  Save and restore game references
 * so that parsing game commands doesn't interfere with backend loops that
 * are working through game references set by prior commands.  Saving
 * references uses the buffer passed in if possible, otherwise allocates
 * its own buffer; testing the return value shows which happened.
 */
static scr_bool *
lib_save_object_references (scr_gameref_t game, scr_bool buffer[], scr_int length)
{
  scr_int required, available;
  scr_bool *references;

  /*
   * Calculate the required bytes for references, and then either allocate or
   * use the buffer supplied.
   */
  required = gs_object_count (game) * sizeof (*references);
  available = length * sizeof (buffer[0]);
  references = required > available ? (decltype(+buffer)) scr_malloc (required) : buffer;

  /*
   * Copy over references from the game, and return the saved copy.  A game
   * with no objects has an empty vector whose data() may be NULL, and memcpy
   * must not be handed NULL even for zero bytes.
   */
  if (required > 0)
    memcpy (references, game->object_references.data (), required);
  return references;
}

static void
lib_restore_object_references (scr_gameref_t game, const scr_bool references[])
{
  scr_int bytes;

  /* Calculate the bytes in the references array, and copy back to the game. */
  bytes = gs_object_count (game) * sizeof (references[0]);
  if (bytes > 0)
    memcpy (game->object_references.data (), references, bytes);
}


/*
 * lib_try_game_command_common()
 * lib_try_game_command_short()
 * lib_try_game_command_with_object()
 * lib_try_game_command_with_npc()
 *
 * Try a game command with a standard verb.  Used by get and drop handlers
 * to retry game commands using standard "get " and "drop " commands.  This
 * makes "take/pick up/put down" work with a game's overridden get/drop.
 */
/*
 * lib_object_short_name_is_ambiguous()
 *
 * Return TRUE if any object other than the one passed shares its Short name
 * (case-insensitively).  Used to suppress the bare-name game-command retry
 * below: if "key" names several objects, a generic game task matched by the
 * bare noun ("get key") is a disambiguation/catch-all handler, not a specific
 * override for the addressed object, and must not block the standard take of a
 * fully-qualified reference ("get brass key").  The Runner never reconstructs a
 * bare noun like this, so it never lets such a task hijack the take; mirror that
 * by only attempting the bare-name retry when the name is unambiguous.
 */
static scr_bool
lib_object_short_name_is_ambiguous (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *name;
  scr_int other, object_count;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  if (!name || name[0] == NUL)
    return FALSE;

  object_count = gs_object_count (game);
  for (other = 0; other < object_count; other++)
    {
      const scr_char *other_name;

      if (other == object)
        continue;
      vt_key[1].integer = other;
      other_name = prop_get_string (bundle, "S<-sis", vt_key);
      if (other_name && scr_strcasecmp (name, other_name) == 0)
        return TRUE;
    }
  return FALSE;
}

/*
 * lib_typed_verb()
 *
 * The retry below is built from a canonical library verb ("get", "drop"),
 * but the Runner matches tasks against the words the player actually typed.
 * "TenebraeSemper.taf" task 0 is "get * pen(s)": run400 runs it for
 * `get pens` ("You take a pen from the drawer.") and NOT for `take pens`,
 * which completes the library take of the pens object untouched (probes
 * runner_probes/tenebraesemper.run400.probe.txt,
 * runner_probes/tenebraesemper.run400.probe3.txt, 2026-08-30) -- so the game's
 * "have a pen" gate on leaving the dorm really does require typing `get`.
 * Retrying with a canonical "get" let Scarier's `take pens` fire that task and
 * walk past the gate.  The precedents the retry was tuned on all keep the
 * typed verb (Wax Worx `get marie` -> "get * head", Sommeril `take silver orb`
 * -> "take silver orb"), so hand the retry the verb the player used: the
 * library's own synonym of the canonical verb that opens the dispatched
 * command element, or the canonical verb when none does.
 */
static const scr_char *
lib_typed_verb (const scr_char *verb)
{
  static const scr_char *const GET_FORMS[] = {"get", "take", "pick up", "pick"};
  /* `leave` is a drop spelling below 4.0 only, but a 4.0 `leave` line never
   * reaches a drop handler, so it never asks here; see
   * lib_cmd_leave_all_pre400(). */
  static const scr_char *const DROP_FORMS[] = {"drop", "put down", "leave"};
  const scr_char *const *forms;
  scr_int count, index_;
  const scr_char *input;

  if (strcmp (verb, "get") == 0)
    {
      forms = GET_FORMS;
      count = sizeof (GET_FORMS) / sizeof (GET_FORMS[0]);
    }
  else if (strcmp (verb, "drop") == 0)
    {
      forms = DROP_FORMS;
      count = sizeof (DROP_FORMS) / sizeof (DROP_FORMS[0]);
    }
  else
    return verb;

  input = run_get_dispatch_input ();
  if (!input)
    return verb;
  while (scr_isspace (*input))
    input++;

  for (index_ = 0; index_ < count; index_++)
    {
      const scr_int length = strlen (forms[index_]);
      if (scr_strncasecmp (input, forms[index_], length) == 0
          && (input[length] == NUL || scr_isspace (input[length])))
        return forms[index_];
    }
  return verb;
}

/*
 * lib_definite_prefix_400()
 *
 * The prefix as run400's name builder Proc_21_31_448710 composes it in its
 * normalizing mode 0: through tense (Proc_21_13_44F474), which rewrites a
 * whole "a", "an" or "some" to "the" and a leading "a ", "an " or "some " to
 * "the ", and leaves everything else alone.  An empty Prefix is already "a"
 * by the time 4.0 has loaded the game (loader @4900EC), so it composes as
 * "the" too.
 *
 * Not sclibrar_print.cpp's lib_definite_prefix(): that one opens the name
 * lib_definite_object_name() prints, carries the pre-3.9 tense() arm, and
 * also skips a "the" already present.
 */
static const scr_char *
lib_definite_prefix_400 (const scr_char *prefix, scr_char *buffer, size_t size)
{
  static const scr_char *const ARTICLES[] = { "a", "an", "some" };
  size_t index_;

  if (scr_strempty (prefix))
    return "the";
  for (index_ = 0; index_ < sizeof (ARTICLES) / sizeof (ARTICLES[0]); index_++)
    {
      const scr_char *const article = ARTICLES[index_];
      const size_t length = strlen (article);

      if (scr_strcasecmp (prefix, article) == 0)
        return "the";
      if (scr_strncasecmp (prefix, article, length) == 0
          && prefix[length] == ' ')
        {
          if (strlen (prefix) - length + 4 > size)
            return prefix;
          snprintf (buffer, size, "the%s", prefix + length);
          return buffer;
        }
    }
  return prefix;
}

/*
 * lib_task_prematches_input()
 *
 * run400's task pre-matcher Proc_19_35_453C50 on the typed line: does any
 * task in scope pattern-match it with its restrictions passing, or fail a
 * restriction that has a message to print?  See run_does_command_match() for
 * the two passes and the House measurement.  Object references are left
 * exactly as they were.
 * class_filter is the pre-matcher's mode byte -- 1 for the take-family
 * look-ups, 2 for the put/drop family, 0 for none; see
 * run_set_task_class_filter().
 */

scr_bool
lib_task_prematches_input (scr_gameref_t game, scr_int class_filter)
{
  const scr_char *input = run_get_dispatch_input ();

  return input && lib_task_prematches_line (game, input, class_filter);
}

/* The same, answering the pre-matcher's result: 0 miss, 1 a task with text
   of its own (or a fallback hit), 2 a silent first-pass hit, 3 a failing
   restriction with a message; see run_does_command_match(). */
scr_int
lib_task_prematch_kind_input (scr_gameref_t game, scr_int class_filter)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int kind = 0;

  if (!input || !lib_task_prematches_line (game, input, class_filter, &kind))
    return 0;
  return kind;
}

/* The same pre-match on a line the Runner has rewritten in place. */
scr_bool
lib_task_prematches_line (scr_gameref_t game, const scr_char *input,
                          scr_int class_filter, scr_int *match_kind)
{
  scr_bool references_buffer[LIB_ALLOCATION_AVOIDANCE_SIZE];
  scr_bool *references, status;

  references = lib_save_object_references (game, references_buffer,
                                           LIB_ALLOCATION_AVOIDANCE_SIZE);
  run_set_task_class_filter (class_filter);
  status = run_does_command_match (game, input, TRUE, match_kind);
  run_set_task_class_filter (0);
  SCR_TRACE_IF ("SCR_TRACE_MATCH", "PREMATCH mode=%ld input=[%s] %s\n",
                (long) class_filter, input, status ? "HIT" : "miss");
  lib_restore_object_references (game, references);
  if (references != references_buffer)
    scr_free (references);
  return status;
}

/*
 * lib_run_rebuilt_line_400()
 *
 * Offer a 4.0 library-rebuilt line ("get the X", "put the X in the Y") to
 * the tasks the way the take piece (Proc_19_39_46302C @462B0D-462C85) and
 * the insides handler (Proc_19_43_46639C @465D21-465EB5) do: pre-match an
 * LCase()d copy, and on a hit dispatch the RAW line, whose capitals the
 * wildcard matcher compares binary (see uip_set_binary_input()).  The hit
 * claims even when the raw dispatch then runs nothing.  hcw (4.00,
 * runner_probes/hcw.run400.txt turn 162): `put susan in trunk` with the Fembot
 * holding "sleeping Susan" pre-matches task 477 `get * susan` on "get
 * sleeping susan", dispatches "get sleeping Susan", runs nothing, and ends
 * on "I don't understand what you mean." -- task 243 `put * susan *` misses
 * the same way on the rebuilt put.  A line with no capitals is unchanged.
 */
scr_bool lib_rebuilt_raw_dispatch = FALSE;

/*
 * Set by the library replies that only say it has nothing for the line --
 * the verb catch-all's "I don't understand what you want me to do with X."
 * and its takefrom and open-with twins, the buy refusals, and the
 * "... but nothing happens." of push, pull and the like.  Read by
 * run_all_commands() after a line it handed the library first; see
 * run_line_yields_to_library().
 */
scr_bool lib_non_answer = FALSE;

/*
 * Set by lib_try_game_command_take_from_parent_400() only: the take piece
 * 46302C exits on a pre-match return of 1 and lets a 2 (a silent task)
 * dispatch and then fall through to the library take (@462C71-462C85).
 */
scr_bool lib_rebuilt_silent_continues = FALSE;

/*
 * Set by the static take refusal only (get_piece 473A34 @473241): a pre-match
 * hit there on a failing restriction's message prints nothing (45404C
 * restores the buffer) and get_piece exits, so generaltasks' dispatcher runs
 * the line as TYPED, with no referenced object.  Professor in the Laboratory,
 * mailbox up: `take mailbox`, `pick up mailbox`, `take rope` and `take the
 * mailbox on-a rope` pre-match task 9's "already up by the window" and answer
 * "What was that?..." (runner_probes/professor.run400.mail8.txt), while `get
 * mailbox` gets task 9's message (runner_probes/professor.run400.mail7.txt
 * T15).  A first-pass hit still dispatches the rebuilt line case-kept
 * (runner_probes/professor.run400.mail2.txt T24).
 */
scr_bool lib_rebuilt_fallback_typed = FALSE;

static scr_bool
lib_run_rebuilt_line_400 (scr_gameref_t game, const scr_char *command)
{
  std::string lowered (command);

  for (auto &c : lowered)
    c = scr_tolower (c);

  if (lib_rebuilt_silent_continues)
    {
      scr_int kind;
      scr_bool ran;

      if (!run_does_command_match (game, lowered.c_str (), TRUE, &kind))
        return FALSE;
      if (kind == 3 && !lib_rebuilt_fallback_typed)
        kind = 1;
      if (kind == 3)
        {
          const scr_char *typed = run_get_dispatch_input ();
          const scr_var_setref_t vars = gs_get_vars (game);
          const scr_int ref_object = var_get_ref_object (vars);
          const scr_int ref_character = var_get_ref_character (vars);

          var_set_ref_object (vars, -1);
          var_set_ref_character (vars, -1);
          ran = typed && run_game_task_commands (game, typed);
          var_set_ref_object (vars, ref_object);
          var_set_ref_character (vars, ref_character);
          kind = 1;
        }
      else if (lowered == command)
        ran = run_game_task_commands (game, command);
      else
        {
          const scr_binary_input_guard binary_input (TRUE);

          ran = run_game_task_commands (game, command);
        }

      /*
       * A 1 whose case-kept dispatch runs nothing claims the line with
       * nothing said; generaltasks' tail answers DontUnderstand, not a turn
       * (p4AUTOFROM `take mail`: "Mailbox on-a Rope" pre-matches `get * rope`
       * lower-cased and misses it as typed).
       */
      if (kind == 1 && !ran)
        {
          pf_buffer_string (gs_get_filter (game),
                            prop_get_global_string (gs_get_bundle (game),
                                                    "DontUnderstand"));
          pf_buffer_character (gs_get_filter (game), '\n');
          game->is_admin = TRUE;
        }
      return kind == 1;
    }

  if (lowered == command)
    return run_game_task_commands (game, command);

  if (!run_does_command_match (game, lowered.c_str (), TRUE))
    return FALSE;

  lib_rebuilt_raw_dispatch = FALSE;
  {
    const scr_binary_input_guard binary_input (TRUE);

    run_game_task_commands (game, command);
  }
  lib_rebuilt_raw_dispatch = TRUE;
  return TRUE;
}

static scr_bool
lib_try_game_command_common (scr_gameref_t game,
                             const scr_char *verb, scr_int object,
                             const scr_char *preposition,
                             scr_int associate,
                             scr_bool is_associate_object,
                             scr_bool is_associate_npc,
                             scr_bool use_typed_verb,
                             scr_bool use_definite = FALSE)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  /*
   * No pre-4.0 Runner rebuilds the line from the resolved object's authored
   * Prefix.  Measured on p37PRETRY / p38PRETRY / p39PRETRY (2026-09-20,
   * make_39_pretryprobe.py and make_3738_pretryprobe.py; transcripts
   * runner_probes/pretry.run390.txt, runner_probes/pretry2.run390.txt,
   * runner_probes/pretry.run370.rtf, runner_probes/pretry.run370.b.rtf,
   * runner_probes/pretry.run380.rtf, runner_probes/pretry.run380.b.rtf): all
   * three answer `take pebble` against a task `take a pebble`, `get stone`
   * against `get a stone`, `put bean in jar` against `put a bean in a jar` and
   * `drop coin` against `drop a coin` out of the library, with every one of
   * those tasks alive when its own spelling is typed.  The crossed pairs (`get
   * pebble`, `take stone`) rule out a canonical-verb rebuild as well as a
   * typed-verb one.  Only the bare-name form below survives, and only for one
   * object.
   */
  /*
   * Deliberate deviation (2026-09-27): on a line no task matches the
   * Runner's way (run_lenient_task_matching()), the retry is made at every
   * version and with the canonical verb, so a task spelled `take a pebble`
   * answers `take pebble` below 4.0 too, and TenebraeSemper's `get * pen(s)`
   * answers `take pens`.
   */
  const scr_bool lenient = run_lenient_task_matching ();
  const scr_bool no_prefixed_retry
    = prop_get_taf_version (bundle) < TAF_VERSION_400 && !lenient;

  if (use_typed_verb && !lenient)
    verb = lib_typed_verb (verb);
  scr_vartype_t vt_key[3];
  scr_char buffer[LIB_ALLOCATION_AVOIDANCE_SIZE];
  scr_bool references_buffer[LIB_ALLOCATION_AVOIDANCE_SIZE];
  const scr_char *prefix, *name;
  scr_char *command;
  scr_bool *references, status;
  assert (!is_associate_object || !is_associate_npc);

  /* Save the game's references, for restore later on. */
  references = lib_save_object_references (game, references_buffer,
                                           LIB_ALLOCATION_AVOIDANCE_SIZE);

  /*
   * 4.0 builds these lines before any handler has stored the object in
   * MemVar_494208 (the takes write it at 47B8F9 only once the take goes
   * ahead), so a "referenced object" restriction sees none.  Professor's
   * `take mailbox` in the square: the refusal's "get the Mailbox on-a Rope"
   * skips task 7 and runs task 8 (runner_probes/professor.run400.mail2.txt).
   */
  const scr_var_setref_t ref_vars = gs_get_vars (game);
  const scr_int saved_ref_object = var_get_ref_object (ref_vars);
  if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
    var_set_ref_object (ref_vars, -1);

  /* Get the addressed object's prefix and main name. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);

  /* Construct and try for game commands with a standard verb. */
  if (is_associate_object || is_associate_npc)
    {
      const scr_char *associate_prefix, *associate_name;
      scr_char definite_prefix[64], definite_associate_prefix[64];
      scr_int required;

      /* Get the associate's prefix and main name. */
      if (is_associate_object)
        {
          associate_prefix = prop_get_indexed_string (bundle, "Objects",
                                                      associate, "Prefix");
          associate_name = prop_get_indexed_string (bundle, "Objects",
                                                    associate, "Short");
        }
      else
        {
          assert (is_associate_npc);
          associate_prefix = prop_get_indexed_string (bundle, "NPCs",
                                                      associate, "Prefix");
          associate_name = prop_get_indexed_string (bundle, "NPCs", associate,
                                                    "Name");
        }
      /*
       * 4.0's put handler composes both names in the name builder's
       * normalizing mode 0 (run400 insides @465DED-465E51, two calls to
       * Proc_21_31_448710 with mode 0), so its canonical line reads "put the
       * bean in the jar", never "put a bean in a jar"; see
       * lib_definite_prefix_400().
       */
      if (use_definite && is_associate_object)
        {
          prefix = lib_definite_prefix_400 (prefix, definite_prefix,
                                            sizeof (definite_prefix));
          associate_prefix
            = lib_definite_prefix_400 (associate_prefix,
                                       definite_associate_prefix,
                                       sizeof (definite_associate_prefix));
        }

      assert (preposition);
      required = strlen (verb) + strlen (prefix) + strlen (name)
                 + strlen (preposition) + strlen (associate_prefix)
                 + strlen (associate_name) + 6;
      command = required > (scr_int) sizeof (buffer)
                ? (decltype(+buffer)) scr_malloc (required) : buffer;

      /*
       * Try the command with prefixes on both the target object and the
       * associate.  This used to also try the prefix-dropped combinations,
       * but the real Runner does not: probed live 2026-08-02 (FM7 + a
       * TheADRIFTProject .tas transplant in run400), "put pill in cup"
       * completes the library put untouched by a matched-but-failing
       * "put * pill in cup" task -- the prefix-less retry is exactly the
       * form that task would steal.  (The single-object retry below keeps
       * its prefixed form too, which is what lets Wax Worx's "get * head"
       * claim "get marie" via "get Marie Antoinette's head" -- the wildcard
       * absorbs the prefix there, matching the Runner.)
       *
       * Pre-4.0 there is no rebuilt line at all: the put handler moves the
       * bean and the task never sees a thing.
       */
      snprintf (command, command == buffer ? sizeof (buffer) : (size_t) required,
                "%s %s %s %s %s %s", verb,
                prefix, name, preposition, associate_prefix, associate_name);
      status = no_prefixed_retry
               ? FALSE
               : (lib_rebuilt_raw_dispatch
                  ? lib_run_rebuilt_line_400 (game, command)
                  : run_game_task_commands (game, command));
    }
  else
    {
      scr_char definite_prefix[64];
      scr_int required;

      /*
       * The implicit take of 4.0's put (run400 Proc_19_39_46302C @462AED)
       * pre-matches "get " & name(obj, mode 0) -- the definite form, one
       * spelling, no prefix-less retry.
       */
      if (use_definite)
        prefix = lib_definite_prefix_400 (prefix, definite_prefix,
                                          sizeof (definite_prefix));
      required = strlen (verb) + strlen (prefix) + strlen (name) + 3;
      command = required > (scr_int) sizeof (buffer)
                ? (decltype(+buffer)) scr_malloc (required) : buffer;

      /* Try the command with and without prefixes on the addressed object.
       * The prefix-less retry can re-hit a task that already matched and
       * failed its restrictions this turn; that is what the Runner shows
       * too (cobl: "take medicine" after "look in rubbish" prints the
       * task's fail text, not the library take -- run400 probe
       * 2026-08-30).  Pre-4.0 keeps only the prefix-less form: there the
       * typed line has already been past the tasks, so the retry is a way
       * for an ALIAS to reach one, not a second spelling of the noun the
       * player used.
       */
      snprintf (command, command == buffer ? sizeof (buffer) : (size_t) required,
                "%s %s %s", verb, prefix, name);
      status = no_prefixed_retry
               ? FALSE
               : (lib_rebuilt_raw_dispatch
                  ? lib_run_rebuilt_line_400 (game, command)
                  : run_game_task_commands (game, command));
      /*
       * Deliberate deviation (2026-09-30): on a line no task matches
       * strictly, the definite rebuild gets the prefix-less retry too.
       * run400's take piece offers the tasks only "get the X" (9298e56b0),
       * so `take mailbox` went past an author's literal `get mailbox` task
       * and its "bolted to the post" restriction, and took the mailbox.
       */
      if (!status && (!use_definite || run_lenient_task_matching ())
          && !lib_object_short_name_is_ambiguous (game, object))
        {
          snprintf (command,
                    command == buffer ? sizeof (buffer) : (size_t) required,
                    "%s %s", verb, name);
          status = run_game_task_commands (game, command);
          if (status && use_definite)
            SCR_DEVIATION ("definite_retry", "ran=\"%s\"", command);
        }
    }

  /* Restore the game object references back to their state on entry. */
  lib_restore_object_references (game, references);
  var_set_ref_object (ref_vars, saved_ref_object);

  /* Free any allocations, and return the game command status. */
  if (command != buffer)
    scr_free (command);
  if (references != references_buffer)
    scr_free (references);
  return status;
}

scr_bool
lib_try_game_command_short (scr_gameref_t game,
                            const scr_char *verb, scr_int object)
{
  return lib_try_game_command_common (game, verb, object,
                                      nullptr, -1, FALSE, FALSE, TRUE);
}

/*
 * The refusal-exit pre-match of run400's per-piece take (Proc_19_23_473A34
 * @473241) is built from the RESOLVED object and the canonical "get", not
 * from the typed words: `take poster` on man overboard.taf runs "Get *
 * poster", a task with no take form at all
 * (runner_probes/man_overboard.run400.txt:39,
 * runner_probes/man_overboard.run400.probe.txt, 2026-08-29/30).  Only the
 * pre-action retry above is verb-literal.
 */
scr_bool
lib_try_game_command_short_canonical (scr_gameref_t game,
                                      const scr_char *verb, scr_int object)
{
  return lib_try_game_command_common (game, verb, object,
                                      nullptr, -1, FALSE, FALSE, FALSE);
}

/*
 * lib_try_game_command_short_definite()
 *
 * The 4.0 drop handler's per-object task look-up, and the drop half of the
 * same rule lib_try_game_command_with_object_400() documents for put: the
 * line offered to the tasks is rebuilt from the RESOLVED object in the
 * normalizing mode 0 -- "drop " & name(obj, 0), so "drop the board" -- and
 * that one spelling is all the tasks ever see.  No authored-prefix form, no
 * prefix-less retry (run400 @46F33B-46F358, class-filter mode 2).
 *
 * The two measurements it reconciles are the same shape as put's.  dusk.taf
 * task 48 `drop * board`, the board in hand, claims `drop board` in run400
 * (runner_transcripts/dusk.txt:80) -- the wildcard absorbs the article the
 * rebuild puts in.  p4REPEAT3.taf task 3, whose command is the literal `drop
 * hat`, does not: run400 answers both `drop hat` turns out of the library,
 * "You drop the hat." and then "You are not holding the hat.", and neither the
 * CompleteText nor the RepeatText is ever printed
 * (runner_probes/repeat3.run400.txt, 2026-09-08).  "drop the hat" simply is
 * not `drop hat`.
 *
 * Pre-4.0 has no authored-prefix form either -- run370/380/390 all answer
 * `drop coin` against a task `drop a coin` out of the library (p3xPRETRY,
 * 2026-09-20) -- and keeps only the bare-name retry, the typed line having
 * already been past the tasks before the library sees it.
 */
scr_bool
lib_try_game_command_short_definite (scr_gameref_t game,
                                     const scr_char *verb, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool references_buffer[LIB_ALLOCATION_AVOIDANCE_SIZE];
  scr_vartype_t vt_key[4];
  scr_bool *references, status;
  scr_int alias_count, alias;

  assert (lib_is_version_400 (game));

  run_set_task_class_filter (2);
  status = lib_try_game_command_common (game, verb, object,
                                        nullptr, -1, FALSE, FALSE, FALSE, TRUE);

  /*
   * Any of the object's names can fill the noun slot, not just its Short.
   * frustrated.taf's `drop tree` names the upper half of the trunk by an
   * alias, and run400 gives the line to `*drop*tree*`
   * (runner_transcripts/frustrated.txt) -- "drop the upper half of the trunk"
   * is not what that pattern matches, "drop the tree" is.
   */
  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count && !status; alias++)
    {
      scr_char buffer[LIB_ALLOCATION_AVOIDANCE_SIZE], definite_prefix[64];
      const scr_char *name, *prefix;

      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (scr_strempty (name))
        continue;

      prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");
      prefix = lib_definite_prefix_400 (prefix, definite_prefix,
                                        sizeof (definite_prefix));
      if (strlen (verb) + strlen (prefix) + strlen (name) + 3
          > sizeof (buffer))
        continue;

      snprintf (buffer, sizeof (buffer), "%s %s %s", verb, prefix, name);
      references = lib_save_object_references (game, references_buffer,
                                               LIB_ALLOCATION_AVOIDANCE_SIZE);
      status = run_game_task_commands (game, buffer);
      lib_restore_object_references (game, references);
      if (references != references_buffer)
        scr_free (references);
    }
  run_set_task_class_filter (0);
  return status;
}

/*
 * lib_try_game_command_take_definite()
 *
 * The task look-up inside 4.0's implicit take (run400 Proc_19_39_46302C,
 * @462AED-462C85): "get " & name(obj, 0), or "get " & name(obj, 0) & " from "
 * & name(holder, 0) when the object sits inside a container, both names in
 * the normalizing mode 0 ("get the wood", "get the coin from the box").  A
 * hit runs that line through the task dispatcher and claims; the library
 * take only follows a miss.
 */
scr_bool
lib_try_game_command_take_definite (scr_gameref_t game, scr_int object)
{
  scr_bool status;

  /* The take piece's look-ups run in the pre-matcher's mode 1 (@462B12,
   * @462B84): only tasks carrying the take flag can answer. */
  run_set_task_class_filter (1);
  lib_rebuilt_raw_dispatch = TRUE;
  lib_rebuilt_silent_continues = TRUE;
  if (gs_object_position (game, object) == OBJ_IN_OBJECT)
    status = lib_try_game_command_common (game, "get", object,
                                          "from",
                                          gs_object_parent (game, object),
                                          TRUE, FALSE, FALSE, TRUE);
  else
    status = lib_try_game_command_common (game, "get", object, nullptr, -1,
                                          FALSE, FALSE, FALSE, TRUE);
  lib_rebuilt_silent_continues = FALSE;
  lib_rebuilt_raw_dispatch = FALSE;
  run_set_task_class_filter (0);
  return status;
}

/*
 * lib_try_game_command_take_from_parent_400()
 *
 * 4.0's take retakes an object "from" whatever holds it.  run400's per-piece
 * get handler (Proc_19_23_473A34) resolves the noun with 463640 in mode 1
 * (473011) -- visible where it is, not static, not held, seen -- and when
 * the line named no "from" and the object sits in (&HF6) or on (&HEC) a
 * parent, it hands the parent to the take piece (47301F-4730A8).  The piece
 * (Proc_19_39_46302C) then pre-matches LCase("get " & name(obj, 0) & " from "
 * & name(parent, 0)) in the take-family mode (462B3E-462B97), dispatches the
 * case-kept line (462C65), exits on a return of 1 and falls through to the
 * take on a 2.  The and-loop does the same per object.
 *
 * Measured on p4AUTOFROM.taf (make_400_autofromprobe.py, run400,
 * runner_probes/autofrom.run400.txt, 2026-09-13):
 *
 *   get treat     treat on the stove, `get *stove*`   -> "T2 BOLTED."
 *   Get token     `get * token from * table`,
 *                 restricted on the token being there -> "T3 TOKEN TASK."
 *   take letter   `get * cord`, fails with a message  -> "T5 CORD FAIL."
 *   take mail     `get * rope` against "Mailbox on-a Rope": the pre-match
 *                 hits, the case-kept dispatch misses  -> DontUnderstand,
 *                 no turn
 *   get tin and string  `get the tin from *`          -> the string taken,
 *                 then "T8 TIN.", the tin left alone
 *
 * warlord (runner_probes/warlord.run400.txt T104/T112/T122) is the first of
 * these: task 2103 `move/push/get *stove*` answers the treat, the bone and the
 * cudgel with "The stove is bolted to the floor.".
 *
 * *looked_up says whether the object qualified; when it did, this look-up
 * is the only one the take gives the tasks.
 */
scr_bool
lib_try_game_command_take_from_parent_400 (scr_gameref_t game, scr_int object,
                                           scr_bool *looked_up)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int position, parent;
  scr_bool status;

  *looked_up = FALSE;
  if (!lib_is_version_400 (game))
    return FALSE;

  position = gs_object_position (game, object);
  if (position != OBJ_IN_OBJECT && position != OBJ_ON_OBJECT)
    return FALSE;
  parent = gs_object_parent (game, object);
  if (parent < 0
      || obj_is_static (game, object)
      || !gs_object_seen (game, object)
      || obj_indirectly_held_by_player (game, object))
    return FALSE;
  if (input && (lib_input_contains_word (input, "from")
                || lib_input_contains_word (input, "all")))
    return FALSE;

  /*
   * The typed line is pre-matched first (472DC8, ahead of the rewrite), and
   * a 1 claims it there: ticket.taf's `get notepad` fails task 113
   * `[get]{the}[notepad]` loudly with "The Station Master stops you." and
   * never reaches task 415 `get *desk*` (runner_probes/ticket.run400.txt).
   */
  *looked_up = TRUE;
  lib_rebuilt_raw_dispatch = TRUE;
  lib_rebuilt_silent_continues = TRUE;
  run_set_task_class_filter (1);
  status = lib_try_game_command_short (game, "get", object);

  /*
   * Deliberate deviation (2026-09-30): the "from" retry is skipped when
   * "get <parent>" alone already matches a task, which is then the
   * parent's task and not the object's.  run400 lets warlord's `get
   * *stove*` ("The stove is bolted to the floor.") answer `get treat` for
   * the treat on the stove, so the treat, the bone and the cudgel could
   * never be taken (0806086b4).  Only a task that names the parent and not
   * the object counts: humbug's `[get/take] {a/the} %object% {from
   * [dennis/fireman]}` matches "get table" and "get token" alike, and must
   * not keep `get token` from its `get * token from * table`.
   */
  if (!status)
    {
      const scr_prop_setref_t bundle = gs_get_bundle (game);
      const scr_char *parent_name =
          prop_get_indexed_string (bundle, "Objects", parent, "Short");
      const scr_char *object_name =
          prop_get_indexed_string (bundle, "Objects", object, "Short");
      const std::string parent_line = std::string ("get ")
                                      + (parent_name ? parent_name : "");
      const std::string object_line = std::string ("get ")
                                      + (object_name ? object_name : "");

      if (parent_name
          && run_line_matches_task_strictly (game, parent_line.c_str (),
                                             object_line.c_str ()))
        {
          SCR_DEVIATION ("from_retry_skipped", "parent=\"%s\"",
                         parent_line.c_str ());
          lib_rebuilt_silent_continues = FALSE;
          lib_rebuilt_raw_dispatch = FALSE;
          run_set_task_class_filter (0);
          return FALSE;
        }
    }
  if (!status)
    status = lib_try_game_command_common (game, "get", object, "from", parent,
                                          TRUE, FALSE, FALSE, TRUE);
  lib_rebuilt_silent_continues = FALSE;
  lib_rebuilt_raw_dispatch = FALSE;
  run_set_task_class_filter (0);
  return status;
}

scr_bool
lib_try_game_command_with_object (scr_gameref_t game,
                                  const scr_char *verb, scr_int object,
                                  const scr_char *preposition,
                                  scr_int other_object)
{
  return lib_try_game_command_common (game, verb, object,
                                      preposition, other_object, TRUE, FALSE,
                                      TRUE);
}

/*
 * lib_try_game_command_with_object_400()
 *
 * The task look-up of 4.0's put-in / put-on handler (run400 insides
 * Proc_19_43_46639C, body 465CA4-46639A).  With a target in hand it rebuilds
 * the line in canonical form -- "put " & name(obj) & " " & Left(prep, 2) &
 * " " & name(target), both names in the normalizing mode 0, so "put the bean
 * in the jar" -- and pre-matches THAT with Proc_19_35_453C50 (@465CBB-465D39,
 * restrictions ignored); the typed line is only consulted when there is no
 * target (@465DC8, the drop and put-down forms).  On a hit the same rebuild
 * goes to the task dispatcher Proc_19_24_44CCE0 at @465EB5, restriction-
 * failure pass included, and a claim exits the handler with 2 before the
 * possession, size and capacity tests.  No claim, and the handler carries
 * on to its own refusal or move.  Here the pre-match and the dispatch are the
 * one run_game_task_commands() call on the definite line.
 *
 * The definite form is what reconciles the measurements: `put * firewood in
 * * fireplace` wins `put firewood in fireplace` in run400
 * (runner_probes/goldilocks.run400.txt 383, prefixes "a pile of" / "the"), as
 * does `put * battery * flashlight` (runner_transcripts/Tear.txt), while the
 * PUT5 arena probe's `put a bean in a jar`, `put * pill in cup` and `put coin
 * in box` tasks (runner_probes/put5.run400.txt, 2026-09-05) and sommeril's
 * `put fish in fountain` (runner_probes/sommeril.run400.probe.txt, both
 * prefixes empty) all lose to the library: none of those patterns matches "put
 * the bean in the jar" / "put the FISH in the FOUNTAIN".  The typed spelling
 * never reaches the tasks through this handler at all -- The ADRIFT Project's
 * `put battery in charger` comes out of the synonym table as "put
 * nickel-cadmium nickel-cadmium accumulator in charger", which no pattern of
 * its `#Charge battery` task matches, and the author's own run400 transcript
 * still shows the task firing: it is the rebuild "put the small battery in the
 * battery charger" that `put * battery * charger *` claims.  Pre-4.0 offers
 * the tasks nothing at all here: the p3xPRETRY probes put the bean in the jar
 * on all three Runners with `put a bean in a jar` matched and passing
 * (2026-09-20), and there the typed line has already been through the tasks
 * before the library sees it.
 */
scr_bool
lib_try_game_command_with_object_400 (scr_gameref_t game,
                                      const scr_char *verb, scr_int object,
                                      const scr_char *preposition,
                                      scr_int other_object)
{
  scr_bool status;

  assert (lib_is_version_400 (game));

  /* The insides handler pre-matches its rebuilt line in mode 2 (@465D39):
   * only tasks carrying the put/drop flag are consulted. */
  /* The take report ahead of it, if any, is a clause of its own:
     "Your hands are full.  You dump the wood ..." (House, patched). */
  const scr_bool joined = pf_buffer_tentative_join (gs_get_filter (game));

  run_set_task_class_filter (2);
  lib_rebuilt_raw_dispatch = TRUE;
  status = lib_try_game_command_common (game, verb, object,
                                        preposition, other_object, TRUE, FALSE,
                                        FALSE, TRUE);
  lib_rebuilt_raw_dispatch = FALSE;
  run_set_task_class_filter (0);

  if (joined)
    pf_retract_tentative_join (gs_get_filter (game));
  return status;
}

/*
 * lib_try_typed_put_line_400()
 *
 * The typed line of a one-object 4.0 put, offered to the put-family tasks
 * after the canonical rebuild above has missed them all.  run400 never does
 * this -- the rebuild is the handler's only look-up, and its refusal or move
 * then claims the line -- so a task written for exactly what the player
 * typed can be unreachable: House's task 459 "[put/place/drop] {some} [wood]
 * {in/into/in to} {the} [fireplace/fire place]" matches `put wood in
 * fireplace` word for word but has no slot for the rebuild's first "the",
 * and the Runner answers "You are not holding the wood." with the fire left
 * unlaid; with the wood already in hand the library's own put claims it,
 * and the fire is just as dead.  Deliberate deviation (2026-09-27): the
 * author's task wins -- but only a task that runs.  A task whose
 * restrictions fail leaves the put to the library, message or no message:
 * see run_passing_task_commands().  It also wins ahead of the size and
 * capacity refusals, which run400 prints before a task the rebuild did
 * reach through run_all_commands() ("Your penknife is too big to fit inside
 * the slot.  A quick push and the button ...", zacksmackfoot).
 */
scr_bool
lib_try_typed_put_line_400 (scr_gameref_t game)
{
  const scr_char *const typed = run_get_dispatch_input ();
  scr_bool status;

  assert (lib_is_version_400 (game));
  if (!typed)
    return FALSE;

  /*
   * Task matching re-binds the references, and a miss must leave them as
   * the handler had them.
   */
  const std::vector<scr_bool> references = game->object_references;
  const std::vector<scr_bool> multiple = game->multiple_references;

  /* The take report ahead of it, if any, is a clause of its own. */
  const scr_bool joined = pf_buffer_tentative_join (gs_get_filter (game));

  run_set_task_class_filter (2);
  status = run_passing_task_commands (game, typed);
  run_set_task_class_filter (0);

  if (joined)
    pf_retract_tentative_join (gs_get_filter (game));
  if (!status)
    {
      game->object_references = references;
      game->multiple_references = multiple;
    }
  return status;
}

scr_bool
lib_try_game_command_with_npc (scr_gameref_t game,
                               const scr_char *verb, scr_int object,
                               const scr_char *preposition, scr_int npc)
{
  return lib_try_game_command_common (game, verb, object,
                                      preposition, npc, FALSE, TRUE, TRUE);
}


/*
 * lib_parse_next_object()
 *
 * Helper for lib_parse_multiple_objects().  Extracts the next object, if any,
 * from referenced text, and returns it.  Disambiguates any ambiguous objects
 * using the verb supplied, and sets are_more_objects if we found an object
 * but there appear to be more following it.
 */
static scr_bool
lib_parse_next_object (scr_gameref_t game, const scr_char *verb,
                       scr_bool (*resolver) (scr_gameref_t, scr_int, scr_int),
                       scr_int resolver_arg,
                       scr_int *object,
                       scr_bool *are_more_objects, scr_bool *is_ambiguous)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *list;
  scr_bool is_matched;

  /*
   * Look for "object" or "object and ...", and set match and more flags.
   *
   * Fall back to "object <trailing text>" -- a single object followed by
   * anything that isn't "and" -- if neither matches.  run400 tolerates
   * filler after a single object reference (probe DONE 2026-08-23: Space
   * Boy's First Adventure Task 72's own command is the literal, unrestricted,
   * textless "drop cape to the floor"; typing it live gets the library's
   * ordinary "Player drop the cape." with the score unchanged,
   * runner_probes/et2.run400.txt, runner_probes/et4.run400.txt), where
   * scarier's exact-match-only %object% previously failed to parse "cape to
   * the floor" at all, so the library's drop never ran and the command fell
   * through to "Drop what?" instead.  This fallback is tried last so it never
   * preempts a real "X and Y" list.
   */
  list = var_get_ref_text (vars);
  if (uip_match ("%object%", list, game))
    {
      *are_more_objects = FALSE;
      is_matched = TRUE;
    }
  else if (uip_match ("%object% and %text%", list, game))
    {
      *are_more_objects = TRUE;
      is_matched = TRUE;
    }
  else if (!uip_match ("%object% from %text%", list, game)
           && uip_match ("%object% %text%", list, game))
    {
      /*
       * "from" is never filler.  `get stone from zzzz` reaches here whenever
       * nothing answers to the noun after it, and swallowing the tail turned
       * it into a plain take -- "You take the stone." -- where run390 says
       * "You can't do that!" and run400 "I don't understand where you want to
       * get things from." (p39DARK/p4TFROM,
       * runner_probes/dark.run390.feed3.txt, runner_probes/tfrom.run400.txt,
       * 2026-09-10).  The take-from catch-alls carry both; see
       * lib_cmd_take_from_nowhere().
       */
      *are_more_objects = FALSE;
      is_matched = TRUE;
    }
  else
    is_matched = FALSE;

  /* If we extracted an object from referenced text, disambiguate. */
  if (is_matched)
    *object = lib_disambiguate_object_common (game, verb,
                                              resolver, resolver_arg,
                                              is_ambiguous);
  else
    *is_ambiguous = FALSE;

  /* Return TRUE if we matched anything. */
  return is_matched;
}


/*
 * lib_parse_multiple_objects()
 *
 * Parser for commands that take multiple object targets from a %text% match.
 * Parses object lists such as "object" and "object and object" and returns
 * the multiple objects in the game's multiple_references.
 */
scr_bool
lib_parse_multiple_objects (scr_gameref_t game, const scr_char *verb,
                            scr_bool (*resolver) (scr_gameref_t, scr_int, scr_int),
                            scr_int resolver_arg,
                            scr_int *count)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int count_, object;
  scr_bool are_more_objects, is_ambiguous;

  /* Initialize variables to avoid gcc warnings. */
  object = -1;
  are_more_objects = FALSE;

  /* Clear all current multiple object references, and the count. */
  gs_clear_multiple_references (game);
  count_ = 0;

  /*
   * Parse the first object from the list.  If we get nothing here, return
   * FALSE if it didn't look like a multiple object list, TRUE if ambiguous.
   * Beyond here, we always return TRUE, since after this point _something_
   * looked believable...
   */
  if (!lib_parse_next_object (game, verb,
                              resolver, resolver_arg,
                              &object, &are_more_objects, &is_ambiguous))
    return FALSE;
  else if (object == -1)
    {
      if (is_ambiguous)
        {
          /*
           * Return TRUE, with zero count, to cause caller to return.  We get
           * here if the first parsed object was ambiguous.  In this case,
           * the disambiguation has printed a message, so we want our caller
           * to simply return TRUE to indicate that the command was handled,
           * albeit not fully successfully.
           */
          *count = count_;
          return TRUE;
        }
      else
        {
          /*
           * No object matched after disambiguation, so return FALSE to have
           * our caller ignore the command.
           */
          return FALSE;
        }
    }

  /* Mark this first object as referenced in the return array. */
  game->multiple_references[object] = TRUE;
  count_++;

  /* Now parse each additional object from the list. */
  while (are_more_objects)
    {
      scr_int last_object;

      /*
       * If no next object, leave the loop.  If no disambiguation message
       * then it was probably garble, so print a message for that case.  We
       * also catch repeated objects here.
       */
      last_object = object;
      if (!lib_parse_next_object (game, verb,
                                  resolver, resolver_arg,
                                  &object, &are_more_objects, &is_ambiguous)
          || object == -1
          || game->multiple_references[object])
        {
          if (!is_ambiguous)
            {
              pf_buffer_string (filter,
                                "I only understood you as far as wanting to ");
              pf_buffer_string (filter, verb);
              pf_buffer_character (filter, ' ');
              lib_print_object_np (game, last_object);
              pf_buffer_string (filter, ".\n");
            }

          /* Zero count to indicate an error somewhere in the list. */
          count_ = 0;
          break;
        }

      /* Mark the object as referenced in the return array. */
      game->multiple_references[object] = TRUE;
      count_++;
    }

  /* We found at least enough of an object list to say we matched. */
  *count = count_;
  return TRUE;
}


/*
 * lib_print_nothing_held()
 *
 * The complaint the "<verb> all [except ...]" commands make when the filter
 * left them with nothing to work on: "You are not holding anything[ else]",
 * or the wearing form, rounded off by `tail`.
 */
void
lib_print_nothing_held (scr_gameref_t game, scr_bool worn,
                        scr_bool add_else, const scr_char *tail)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (worn)
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You are not wearing anything",
                                           "I am not wearing anything",
                                           "%player% is not wearing anything"));
  else
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You are not holding anything",
                                           "I am not holding anything",
                                           "%player% is not holding anything"));
  if (add_else)
    pf_buffer_string (filter, " else");
  pf_buffer_string (filter, tail);
}


/*
 * lib_multiple_retains_associate()
 *
 * The "put the box in the box" case: the object list named the very object
 * the command acts through.  Complains and returns TRUE where it did.
 */
scr_bool
lib_multiple_retains_associate (scr_gameref_t game, scr_int associate,
                                const scr_char *verb)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (!game->multiple_references[associate])
    return FALSE;

  pf_buffer_string (filter, "I only understood you as far as wanting to ");
  pf_buffer_string (filter, verb);
  pf_buffer_character (filter, ' ');
  lib_print_object_np (game, associate);
  pf_buffer_string (filter, ".\n");
  return TRUE;
}


/*
 * lib_apply_filter()
 *
 * Apply filters for multiple object frontends.  Transfer multiple object
 * references into standard object references, using the supplied filter.
 * `is_except` inverts the sense of the parsed list: its objects are the ones
 * to leave behind, and everything else the filter admits is referenced.
 */
scr_int
lib_apply_filter (scr_gameref_t game,
                  scr_bool (*filter) (scr_gameref_t, scr_int, scr_int),
                  scr_int filter_arg, scr_bool is_except, scr_int *references)
{
  scr_int count, object, references_;

  /* Clear all object references initially. */
  gs_clear_object_references (game);

  /*
   * Find objects included by the filter, and transfer the reference of each
   * from the multiple references into standard references.
   */
  count = 0;
  references_ = references ? *references : 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_bool is_listed;

      if (!filter (game, object, filter_arg))
        continue;

      /* Consume the list entry, if this object was one. */
      is_listed = game->multiple_references[object];
      if (is_listed)
        {
          game->multiple_references[object] = FALSE;
          references_--;
        }

      /* Reference the listed objects, or -- for "all except" -- the rest. */
      if (is_except ? !is_listed : is_listed)
        {
          game->object_references[object] = TRUE;
          count++;
        }
    }

  /* Copy back the updated reference count, return count. */
  if (references)
    *references = references_;
  return count;
}
