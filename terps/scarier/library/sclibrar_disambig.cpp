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
 * NPC and object disambiguation: the wording of the "which" question,
 * the NPC pick, and lib_disambiguate_object_common(), the driver every
 * object-taking handler resolves its noun through.
 *
 * See sclibrar.h for what the library files share, sclibrar_internal.h
 * for what the core files share, and sclibrar_disambig_internal.h for
 * what this file shares with the two split out of it.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"
#include "sclibrar_disambig_internal.h"

/* The module state; see sclibrar_internal.h. */
scr_disambig_state_t disambig;

/*
 * lib_disambiguate_npc()
 *
 * Filter, then search the set of NPC matches.  If only one matched, note
 * and return it.  If multiple matched, print a disambiguation message and
 * the list, and return -1 with *is_ambiguous TRUE.  If none matched, return
 * -1 with *is_ambiguous FALSE if requested, otherwise print a message then
 * return -1.
 */

/*
 * lib_which_runner_form()
 * lib_which_head()
 *
 * The Runners word every ambiguity question as a statement followed by a
 * question -- "Which <term>.  <list>?", 3.7/3.8 examines() "Which <Short>
 * would you like to examine.  <list>?" -- and at 4.0 an empty character
 * Prefix leaves its space in the list ("Which woman.   woman or  woman?").
 * Scarier deliberately asks "Which <term>?  <list>?" instead, with no
 * stray spaces (deviation policy: the full stop is a display accident, not
 * text worth matching).  The exception is a game whose authors wrote ALRs
 * against the Runner's form -- cursed's 58 "Which <term>.  <list>?"
 * rewrites, asteroid_after's satellite prompts, Vendetta's "Which girl",
 * DragonShrine's "Which painting." -- where the Runner's form is kept so
 * that the author's replacement fires.  Any ALR Original starting "Which "
 * opts the game in.
 */

scr_bool
lib_which_runner_form (scr_gameref_t game)
{
  scr_prop_setref_t &cached_bundle = disambig.which.bundle;
  scr_bool &cached_result = disambig.which.runner_form;
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key;
  scr_int alr_count, index_;

  if (bundle == cached_bundle)
    return cached_result;

  cached_bundle = bundle;
  cached_result = FALSE;
  vt_key.string = "ALRs";
  alr_count = prop_get_child_count (bundle, "I<-s", &vt_key);
  for (index_ = 0; index_ < alr_count; index_++)
    {
      const scr_char *original;

      original = prop_get_indexed_string (bundle, "ALRs", index_, "Original");
      if (original && strncmp (original, "Which ", 6) == 0)
        {
          cached_result = TRUE;
          break;
        }
    }
  return cached_result;
}

/* "Which <head>?  " (Scarier) or "Which <head>.  " (the Runner's form). */
void
lib_which_head (scr_gameref_t game, const scr_char *head,
                const scr_char *tail)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, "Which ");
  pf_buffer_string (filter, head);
  if (tail)
    pf_buffer_string (filter, tail);
  pf_buffer_string (filter, lib_which_runner_form (game) ? ".  " : "?  ");
}

/*
 * How a pre-4.0 Runner settles a line naming two present characters.  It
 * never asks: characters() is one loop over every NPC in index order, and
 * each verb's arm either assigns the message outright, so the LAST named
 * character wins, or only when the message is still empty, so the FIRST
 * does.  Measured on p37/p38/p39NPCAMB (make_3738_npcambprobe.py; Ann and
 * Bob both "a guard" in the room, Cora a third guard next door), run370x
 * runner_probes/npcamb.run370.b.rtf, run380x
 * runner_probes/npcamb.run380.rtf, run390x
 * runner_probes/npcamb.run390.txt:
 *
 *   LAST    x guard (description), ask guard (hint), ask guard about key
 *           (topic), 3.9 take stone from guard ("Bob is not carrying...")
 *   FIRST   give (to) guard, 3.9 take guard, hit/kick guard, the 3.9
 *           character catch-all (hug/eat/bare `guard`)
 *
 * talk to, where is, 3.9 kiss and 3.7/3.8 take and take-from test no room
 * at all and are settled by their callers.  NPC_PICK_ASK keeps SCARE's own
 * question, which is also what 4.0 does in its own way.  Scarier asks at
 * every version (deliberate deviation), so the pick is a record only.
 */


scr_int
lib_disambiguate_npc (scr_gameref_t game,
                      const scr_char *verb, scr_bool *is_ambiguous)
{
  return lib_disambiguate_npc_pick (game, verb, is_ambiguous, NPC_PICK_ASK);
}

/*
 * lib_last_named_npc()
 *
 * The last NPC, in index order, that the line named, present or not and
 * seen or not -- what a plainly assigning characters() arm with no room test
 * leaves behind.  Call before lib_disambiguate_npc(), which clears the
 * references it filters out.  -1 for none.
 */
scr_int
lib_last_named_npc (scr_gameref_t game)
{
  scr_int index_, npc;

  npc = -1;
  GS_FOR_EACH_NPC (game, index_)
    {
      if (game->npc_references[index_])
        npc = index_;
    }
  return npc;
}

/*
 * lib_print_npc_not_here_pre390()
 *
 * run380 characters()' take arm (44054B-44059A) and run370's alike: for
 * every character the line names, "I don't think <prefix> <alias> would
 * appreciate being handled." if here, else "<Name> is not here!", assigned
 * outright -- so the last named character settles it, with no seen test.
 * `take cora` and `take stone from cora` with Cora next door, never met:
 * "Cora is not here!" (run370x runner_probes/npcamb.run370.one.rtf,
 * run380x runner_probes/npcamb.run380.one.rtf), where 3.9 says "Take
 * what?".  TRUE once it has answered for an absent character.
 */
scr_bool
lib_print_npc_not_here_pre390 (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (npc == -1 || npc_in_room (game, npc, gs_playerroom (game)))
    return FALSE;

  pf_buffer_string (filter, prop_get_indexed_string (gs_get_bundle (game),
                                                     "NPCs", npc, "Name"));
  pf_buffer_string (filter, " is not here!\n");
  return TRUE;
}

/*
 * The contest the NPC and object crowds share: score every reference, and
 * if the top score is not everyone's, drop the references that fall short
 * of it.  count and last are rewritten only when the crowd thins.
 */
template <typename Score>
static void
lib_keep_top_scored (std::vector<scr_bool> &references, scr_int total,
                     scr_int *count, scr_int *last, Score score)
{
  scr_int index_, best, kept;

  best = -1;
  kept = 0;
  for (index_ = 0; index_ < total; index_++)
    {
      scr_int value;

      if (!references[index_])
        continue;
      value = score (index_);
      if (value > best)
        {
          best = value;
          kept = 1;
        }
      else if (value == best)
        kept++;
    }

  if (kept > 0 && kept < *count)
    {
      *count = 0;
      *last = -1;
      for (index_ = 0; index_ < total; index_++)
        {
          if (references[index_] && score (index_) == best)
            {
              (*count)++;
              *last = index_;
            }
          else
            references[index_] = FALSE;
        }
    }
}

scr_int
lib_disambiguate_npc_pick (scr_gameref_t game, const scr_char *verb,
                           scr_bool *is_ambiguous, scr_int /* pick */)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int count, index_, npc, listed;

  /*
   * Filter out all referenced NPCs not actually visible or seen.  Count the
   * number of NPCs remaining as referenced by the last command, and note the
   * last referenced NPC, for where count is 1.
   */
  count = 0;
  npc = -1;
  GS_FOR_EACH_NPC (game, index_)
    {
      if (game->npc_references[index_]
          && gs_npc_seen (game, index_)
          && npc_in_room (game, index_, gs_playerroom (game)))
        {
          count++;
          npc = index_;
        }
      else
        game->npc_references[index_] = FALSE;
    }

  /*
   * 4.0's Prefix contest thins the crowd before any handler sees it: the
   * character whose own Prefix words the line holds most of is THE
   * character, and only a tie at the top is still a crowd.  p4PFX2's
   * `x the red guard` examines Cid ("the red") over Ann ("a big red") and
   * Bob ("a red"), where all three answer to "guard"; see
   * lib_npc_400_prefix_score().  Pre-4.0 has no such contest at all -- see
   * NPC_PICK_FIRST/NPC_PICK_LAST below -- but Scarier holds it at every
   * version (deliberate deviation): `x blue guard` is Bob, not the last
   * guard by index.
   */
  if (count > 1 && run_get_dispatch_input ())
    {
      const scr_char *line = run_get_dispatch_input ();

      lib_keep_top_scored (game->npc_references, gs_npc_count (game), &count, &npc,
                           [&] (scr_int candidate)
        {
          return lib_npc_400_prefix_score (game, candidate, line);
        });
    }

  /* If the reference is unambiguous, set in variables and return it. */
  if (count == 1)
    {
      /* Set this NPC as the referenced character. */
      var_set_ref_character (vars, npc);

      /* Return, setting no ambiguity. */
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      return npc;
    }

  /* If nothing referenced, return no NPC. */
  if (count == 0)
    {
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      else
        {
          pf_buffer_string (filter,
                            "Please be more clear, who do you want to ");
          pf_buffer_string (filter, verb);
          pf_buffer_string (filter, "?\n");
        }
      return -1;
    }

  /*
   * Deliberate deviation: pre-4.0 asks too.  The Runners never do -- each
   * characters() arm takes the first or the last character named, per
   * NPC_PICK_FIRST/NPC_PICK_LAST above -- so `hit guard` with two guards
   * here hit whichever the index order gave.  The callers still name the
   * Runner's pick, as a record of it; the parameter is unnamed.
   */

  /* 4.0 asks its own question instead; see lib_npc_400_raise_for_line(). */
  if (lib_is_version_400 (game) && lib_npc_400_raise_for_line (game))
    {
      if (is_ambiguous)
        *is_ambiguous = TRUE;
      return -1;
    }

  /* The NPC reference is ambiguous, so list the choices. */
  pf_buffer_string (filter, "Please be more clear, who do you want to ");
  pf_buffer_string (filter, verb);
  pf_buffer_string (filter, "?  ");

  pf_new_sentence (filter);
  listed = 0;
  GS_FOR_EACH_NPC (game, index_)
    {
      if (game->npc_references[index_])
        {
          lib_print_npc_np (game, index_);
          listed++;
          if (listed < count)
            pf_buffer_string (filter, (listed < count - 1) ? ", " : " or ");
        }
    }
  pf_buffer_string (filter, "?\n");

  /* Return no NPC for an ambiguous reference. */
  if (is_ambiguous)
    *is_ambiguous = TRUE;
  return -1;
}


#ifdef SCARIER_DUMP_TOOLS
/*
 * SCR_TRACE_CO: report where the Runner's test disagrees with ours at each
 * lib_disambiguate_object_common() call.  `ours=` is our own post-filter
 * reference count, so ours=1 is a real divergence and ours>1 means only the
 * prompt's wording differed before lib_co_ambiguity_prompt() existed.  A
 * measurement harness only: it changes nothing.
 */
static void
lib_trace_runner_co (scr_gameref_t game, const scr_char *verb, scr_int count)
{
  const scr_char *command, *ambig_term;
  scr_int ambig_present;

  if (!SCR_TRACE_ON ("SCR_TRACE_CO"))
    return;
  command = run_get_dispatch_input ();
  if (lib_runner_co_scan (game, command, &ambig_term, nullptr, &ambig_present))
    fprintf (stderr, "CO-AMBIG verb=[%s] input=[%s] term=[%s]"
             " present=%ld ours=%ld\n",
             verb ? verb : "", command, ambig_term, ambig_present, count);
}
#endif

/*
 * lib_disambiguate_object_common()
 * lib_disambiguate_object()
 *
 * Filter, then search the set of object matches.  If only one matched, note
 * and return it.  If multiple matched, print a disambiguation message and
 * the list, and return -1 with *is_ambiguous TRUE.  If none matched, return
 * -1 with *is_ambiguous FALSE if requested, otherwise print a message then
 * return -1.
 *
 * If normal disambiguation returns more than one object, the resolver
 * function, if supplied, is used to see if the multiple objects can be
 * resolved into just one object.  The resolver function can normally be the
 * same as the function used to filter objects for multiple references.
 */

/*
 * lib_co_note_line_top()
 *
 * TRUE once a 3.7 library handler has settled this line's object; see
 * lib_disambiguate_object_common() and lib_co_ambiguity_prompt().  Cleared
 * at the top of every line by run_all_commands().
 */
void
lib_co_note_line_top (scr_gameref_t game)
{
  const scr_char *line = run_get_dispatch_input ();
  scr_int object;

  disambig.line.prompt_370_blocked = FALSE;
  disambig.line.top_mode = line ? lib_co_verb_mode (line) : 0;
  disambig.line.top_fits.assign (gs_object_count (game), FALSE);
  GS_FOR_EACH_OBJECT (game, object)
    disambig.line.top_fits[object]
      = lib_co_mode_fits (game, object, disambig.line.top_mode);
}

/*
 * lib_name_offset_pre400()
 * lib_first_named_pre400()
 *
 * Where the typed line first names an object -- the lowest offset at which
 * its Short or any of its Aliases occurs -- and the referenced object with
 * the lowest such offset.
 *
 * Pre-4.0 a line naming several objects that no handler acts on is answered
 * by the generic can't-do tail in therest(), and that names the FIRST of
 * them by WORD POSITION, not by object index.  Measured on p*OPENW.taf (one
 * lit room holding a gem, a rock, a static slab and a closed chest, the gem
 * held), 2026-09-20:
 *
 *     command             run370               run380/run390
 *     open rock gem       can't open the rock. can't open the gem!
 *     open gem rock       can't open the gem.  can't open the gem!
 *     open slab rock      can't open the slab. can't open the rock!
 *     close rock gem      can't close the rock.  (all three versions)
 *     close gem rock      can't close the gem.   (all three versions)
 *
 * (runner_probes/openw.run370.ow.rtf, runner_probes/openw.run380.ow.rtf,
 * runner_probes/openw.run390.ow.txt, runner_probes/openw.run370.ox.rtf,
 * runner_probes/openw.run380.ox.rtf, runner_probes/openw.run390.ox.txt.)
 * 3.80 gave `open` a refusal of its own inside openclose(), above therest,
 * and that one names the lowest object INDEX instead and ends in a bang;
 * `close` got none until 4.0, so it keeps falling through to therest at
 * every pre-4.0 version.
 */
static scr_int
lib_name_offset_pre400 (scr_gameref_t game, scr_int object,
                        const scr_char *line)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  const scr_char *name, *found;
  scr_int aliases, alias, best = -1;

  name = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!scr_strempty (name) && (found = strstr (line, name)) != nullptr)
    best = found - line;

  aliases = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < aliases; alias++)
    {
      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (scr_strempty (name) || !(found = strstr (line, name)))
        continue;
      if (best < 0 || found - line < best)
        best = found - line;
    }
  return best;
}

static scr_int
lib_first_named_pre400 (scr_gameref_t game, scr_int fallback)
{
  const scr_char *line = run_get_dispatch_input ();
  scr_int index_, best = -1, offset, best_offset = 0;

  if (!line)
    return fallback;

  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (!game->object_references[index_])
        continue;
      offset = lib_name_offset_pre400 (game, index_, line);
      if (offset < 0)
        continue;
      if (best < 0 || offset < best_offset)
        {
          best = index_;
          best_offset = offset;
        }
    }
  return best >= 0 ? best : fallback;
}

/*
 * One lib_disambiguate_object_common() call: the verb and resolver it was
 * given, and the candidates still standing -- COUNT of them flagged in
 * object_references, OBJECT the last one found.  A stage that settles the
 * question returns TRUE and leaves the answer in RESULT.
 */
struct lib_disambig_t
{
  const scr_gameref_t game;
  const scr_char *const verb;
  scr_bool (*const resolver) (scr_gameref_t, scr_int, scr_int);
  const scr_int resolver_arg;
  scr_bool *const is_ambiguous;
  const scr_filterref_t filter;
  const scr_var_setref_t vars;
  const scr_prop_setref_t bundle;
  const scr_int taf_version;
  scr_int count, object, result;

  lib_disambig_t (scr_gameref_t game_, const scr_char *verb_,
                  scr_bool (*resolver_) (scr_gameref_t, scr_int, scr_int),
                  scr_int resolver_arg_, scr_bool *is_ambiguous_)
    : game (game_), verb (verb_), resolver (resolver_),
      resolver_arg (resolver_arg_), is_ambiguous (is_ambiguous_),
      filter (gs_get_filter (game_)), vars (gs_get_vars (game_)),
      bundle (gs_get_bundle (game_)),
      taf_version (prop_get_taf_version (bundle)), count (0), object (-1),
      result (-1) {}

  scr_bool done (scr_int object_) { result = object_; return TRUE; }

  void gather_candidates (void);
  scr_bool narrow_named_pre400 (void);
  void narrow_top_scored_400 (void);
  void narrow_with_resolver (void);
  scr_bool ask_which_examine_pre390 (void);
  void pick_wear_remove_pre380 (void);
  scr_bool pick_take_drop_pre400 (void);
  void pick_open_close_pre400 (void);
  scr_bool settle_single_or_none (void);
  scr_bool resolve_take_400 (void);
  scr_bool raise_crowd_400 (void);
  scr_bool resolve_examine_400 (void);
  scr_int list_choices (void);
};

/*
 * Filter out all referenced objects not actually visible or seen.  Count
 * the number of objects remaining as referenced by the last command, and
 * note the last referenced object, for where count is 1.  Version 3.8
 * games skip the seen test -- see lib_matcher_requires_seen().
 */
void
lib_disambig_t::gather_candidates (void)
{
  const scr_bool requires_seen = lib_matcher_requires_seen (game);
  scr_int index_;

  count = 0;
  object = -1;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (game->object_references[index_]
          && (!requires_seen || gs_object_seen (game, index_))
          && obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        {
          count++;
          object = index_;
        }
      else
        game->object_references[index_] = FALSE;
    }

#ifdef SCARIER_DUMP_TOOLS
  lib_trace_runner_co (game, verb, count);
#endif

  /*
   * 3.7's handlers never call co(): only therest() (run370 43CC86) and
   * insides() do, so the end-of-turn "Which <term>." is raised only by a
   * line the catch-all or put answered.  Note that a handler of this line
   * has its object(s); lib_co_ambiguity_prompt() then stays quiet.
   */
  if (taf_version < TAF_VERSION_380 && count > 0
      && strncmp (verb, "put", 3) != 0 && strcmp (verb, "move") != 0)
    disambig.line.prompt_370_blocked = TRUE;
}

/*
 * 3.8 and 3.9 put each candidate through co() -- run380 co(obj) from
 * drops, wears, removes, openclose, examines, takes and the rest, run390
 * co(obj, mode) (43B6BC) -- BEFORE any handler filter: the term's present
 * namesakes are counted, and with two or more the object passes only if
 * the last word of its Prefix is typed (lib_co_pre400).  So `wear hat`
 * with the red hat in hand and the blue one on the floor wears NOTHING at
 * 3.8/3.9 -- held-ness never narrowed the count -- and the turn is the
 * "Which hat.  The red hat or the blue hat?" the generaltasks scan raises;
 * likewise remove, open, close, take and drop (3.8).  3.9's takes() uses
 * mode 1 (loose in the room) and drops() mode 2 (isheld), which do
 * narrow.  Measured on p38TASK/p39TASK
 * (runner_probes/task.run380.pname.rtf,
 * runner_probes/task.run380.pname2.rtf,
 * runner_probes/task.run390.pname.txt,
 * runner_probes/task.run390.pname2.txt, 2026-09-19).  Scarier narrows take
 * and drop at 3.8 too, and wear and remove at both (deliberate deviation),
 * so that `wear hat` puts on the red hat.
 *
 * 3.9's examine narrows the same way -- referencedob() (42DF43) calls the
 * same co(obj, 0), so the last Prefix word settles a crowd for it too:
 * `x tree red` with trees Prefixed "a red" and "a blue" is "A red tree."
 * (p39PFX, run390x runner_probes/pfx.run390.txt turn 17, 2026-09-20).
 * What it does NOT do is answer when the word narrows nothing: there co()
 * is false for every candidate, referencedob() counts none, and
 * examines() keeps its own "Nothing special." below, under the
 * end-of-turn "Which tree."
 */
scr_bool
lib_disambig_t::narrow_named_pre400 (void)
{
  scr_int index_;

  const scr_bool examine_390 = taf_version >= TAF_VERSION_390
                               && strcmp (verb, "examine") == 0;
  if (count > 1 && taf_version >= TAF_VERSION_380
      && taf_version < TAF_VERSION_400 && run_get_dispatch_input ())
    {
      const scr_char *line = run_get_dispatch_input ();
      scr_int mode = 0, kept = 0;

      /* The wear and remove modes, and all four at 3.8, are Scarier's
         (deliberate deviation); see lib_co_pre400(). */
      if (strcmp (verb, "drop") == 0)
        mode = 2;
      else if (strcmp (verb, "take") == 0)
        mode = 1;
      else if (strcmp (verb, "wear") == 0)
        mode = 3;
      else if (strcmp (verb, "remove") == 0)
        mode = 4;
      GS_FOR_EACH_OBJECT (game, index_)
        {
          if (game->object_references[index_]
              && lib_co_pre400 (game, line, index_, mode))
            kept++;
        }
      if (kept == 0 && !examine_390
          && lib_runner_co_scan (game, line, nullptr, nullptr, nullptr))
        {
          /*
           * Every candidate refused, so the handler's loop acted on
           * nothing and it says so: takes() "Take what?" (3.8 and 3.9),
           * 3.8 examines() "Nothing special.".  The end-of-turn prompt
           * wipes that unless a task ran this turn -- an event's does
           * (p38EVQ2/p39EVQ2, runner_probes/evq2.run380.rtf /
           * runner_probes/evq2.run390.txt, 2026-09-20).
           */
          if (strcmp (verb, "take") == 0)
            lib_what (game, "Take");
          else if (strcmp (verb, "examine") == 0)
            pf_buffer_string (gs_get_filter (game), "Nothing special.\n");
          if (is_ambiguous)
            *is_ambiguous = TRUE;
          return done (-1);
        }
      if (kept > 0 && kept < count)
        {
          count = 0;
          object = -1;
          GS_FOR_EACH_OBJECT (game, index_)
            {
              if (game->object_references[index_]
                  && lib_co_pre400 (game, line, index_, mode))
                {
                  count++;
                  object = index_;
                }
              else
                game->object_references[index_] = FALSE;
            }
        }
    }
  return FALSE;
}

/*
 * 4.0 runs the Prefix contest over a crowded reference instead: co()'s
 * crowded arm (run400 454454, called from Proc_21_39_46486C) scores every
 * candidate the way 463640 does -- the Short as a whole word, any alias,
 * and one more for each word of the Prefix the line holds -- and keeps
 * the strict maximum.  A single winner is the reference, and when several
 * tie at the top it is THOSE the question offers, not the whole reference
 * set.
 *
 * Measured 2026-09-20 on p4PFX2.taf, three trees Prefixed "a big red",
 * "a red" and "the red" beside a "a" rock (run400x
 * runner_probes/pfx2.run400.txt):
 *
 *     x tree          ->  Which tree.  The big red tree, the red tree or
 *                         the red tree?                       (1-1-1)
 *     x red tree      ->  the same three                      (2-2-2)
 *     x a red tree    ->  Which tree.  The big red tree or the red tree?
 *                                                             (3-3-2)
 *     x the red tree  ->  The red tree.                       (2-2-3)
 *     x big tree      ->  A big red tree.                     (2-1-1)
 *     x a tree        ->  Which tree.  The big red tree or the red tree?
 *                                                             (2-2-1)
 *
 * The older p4CO cells still hold because their candidates all tie:
 * `x tree rock` offers "the red tree, the blue tree or the rock" because
 * each scores its own Short and no Prefix word was typed.
 *
 * Below 4.0 the same crowd is settled by the last Prefix word alone; see
 * lib_co_pre400() above.
 */
void
lib_disambig_t::narrow_top_scored_400 (void)
{
  if (count > 1 && taf_version >= TAF_VERSION_400 && run_get_dispatch_input ())
    {
      const scr_char *line = run_get_dispatch_input ();

      lib_keep_top_scored (game->object_references, gs_object_count (game), &count, &object,
                           [&] (scr_int candidate)
        {
          return lib_verb_object_name_score (game, candidate, line);
        });
    }
}

/*
 * If this reference is ambiguous and a resolver was supplied, try to
 * resolve it unambiguously by calling the resolver filter on the remaining
 * set references.
 */
void
lib_disambig_t::narrow_with_resolver (void)
{
  scr_int index_;

  if (resolver && count > 1)
    {
      scr_int retry_count;

      /*
       * Search for objects accepted by the resolver filter, but don't filter
       * references just yet.  Again, note the last referenced.
       */
      retry_count = 0;
      object = -1;
      GS_FOR_EACH_OBJECT (game, index_)
        {
          if (game->object_references[index_]
              && resolver (game, index_, resolver_arg))
            {
              retry_count++;
              object = index_;
            }
        }

      /* See if we narrowed the field without eliminating every object. */
      if (retry_count > 0 && retry_count < count)
        {
          /*
           * If we got down to a single object, the ambiguity is resolved.
           * In this case, set count to 1 so that 'object' is returned.
           */
          if (retry_count == 1)
            count = retry_count;
          else
            {
              /*
               * We got down to fewer objects; reduce references so that the
               * disambiguation message is clearer.  Note that here we still
               * leave with count greater than 1.
               */
              count = 0;
              GS_FOR_EACH_OBJECT (game, index_)
                {
                  if (game->object_references[index_]
                      && resolver (game, index_, resolver_arg))
                    count++;
                  else
                    game->object_references[index_] = FALSE;
                }
            }
        }
    }
}

/*
 * 3.7 has no co() in its handlers, and each settles a namesake its own
 * way (p37TASK, runner_probes/task.run370.pname.rtf /
 * runner_probes/task.run370.pname2.rtf, 2026-09-19; two hats, red held
 * and blue loose, two open boxes loose):
 *
 *   drops (430DDC) / takes (436280): every held (loose) namesake lacking
 *     the last word of its Prefix in the line is marked and skipped,
 *     unless it is the only one there is -- so two in hand is "Drop
 *     what?", two on the floor "Take what?"; the "Which ... drop/take"
 *     strings at 430866 are dead code.
 *   wears (42C9FC) / removes: the loop puts on EVERY held wearable
 *     namesake (takes off every worn one), each overwriting the message,
 *     and "not holding"/"can't wear" goes only into an empty message: red
 *     held and blue loose is "You put on a red hat.", red worn and blue
 *     held "You put on a blue hat.".
 *   openclose (426770): every namesake changes state, the last speaks:
 *     `close box` closes both, "You close the blue box.".
 *   examines (4359D5): "Which hat would you like to examine.  The red hat
 *     or the blue hat?" over the present namesakes, no state change.
 *
 * The wear/remove loops are folded here as "act on all but the last
 * directly, hand the last to the handler", so the handler's own wording
 * answers for the last one exactly as the Runner's overwriting loop
 * leaves it.  openclose's loop is the same fold, but 3.80 and 3.90 run it
 * too, so it lives in the pre-4.0 block below.
 */
/*
 * examines() asks its own question, and it is not openclose's or takes':
 * "Which <Short of the LAST match by index> would you like to examine.
 * <the matches, in index order>?"  3.80 keeps it -- `read rock gem`, `read
 * gem rock` and `examine rock gem` are all "Which rock would you like to
 * examine.  The gem or the rock?" under run370 AND run380, and `read rock
 * with slab` is "Which slab would you like to examine.  The rock or the
 * slab?" (p*OPENW, runner_probes/openw.run370.ow.rtf,
 * runner_probes/openw.run380.ow.rtf, runner_probes/openw.run370.ox.rtf,
 * runner_probes/openw.run380.ox.rtf, 2026-09-20) -- so the word order on
 * the line never picks here, only the index does.  3.90 replaced the
 * question with referencedob()'s last-word pass; see
 * lib_examine_crowded_390().
 *
 * Pre-4.0 `read` is one of examines()' entry words, so it asks the same
 * question about the same verb; see lib_cmd_read_other().
 */
scr_bool
lib_disambig_t::ask_which_examine_pre390 (void)
{
  scr_int index_, listed;

  if (count > 1 && taf_version < TAF_VERSION_390
      && (strcmp (verb, "examine") == 0 || strcmp (verb, "read") == 0))
    {
      lib_which_head (game,
                      prop_get_indexed_string (gs_get_bundle (game),
                                               "Objects", object, "Short"),
                      " would you like to examine");
      pf_new_sentence (filter);
      listed = 0;
      GS_FOR_EACH_OBJECT (game, index_)
        {
          if (!game->object_references[index_])
            continue;
          if (listed > 0)
            pf_buffer_string (filter, listed == count - 1 ? " or " : ", ");
          lib_print_object_np (game, index_);
          listed++;
        }
      pf_buffer_string (filter, "?\n");
      if (is_ambiguous)
        *is_ambiguous = TRUE;
      return done (-1);
    }
  return FALSE;
}

/*
 * run370's wears() and removes() rewrite the line's Alias with
 * replacealias(0) -- a literal 0 (LitI2_Byte at 42C48A / 42958A), not the
 * argument-less every-object form takes() uses -- so only object #0's
 * Alias ever becomes a Short, and every other object must be named by
 * its Short: `don ball` holding a red and a blue ball, both aliased
 * "ball", is "Wear what?" (p37SLOT2, harness/make_37_slotprobe.py,
 * runner_probes/slot2.run370.b.rtf, 2026-09-21).  Deliberate deviation:
 * Scarier lets every Alias name the object, as 3.8 does.
 */
void
lib_disambig_t::pick_wear_remove_pre380 (void)
{
  scr_int index_;

  if (count > 1 && taf_version < TAF_VERSION_380
      && !lib_references_are_namesakes (game))
    {
      const scr_bool is_wear = strcmp (verb, "wear") == 0;
      const scr_bool is_remove = strcmp (verb, "remove") == 0;

      if (is_wear || is_remove)
        {
          scr_int first = -1, last = -1;

          GS_FOR_EACH_OBJECT (game, index_)
            {
              if (!game->object_references[index_])
                continue;
              if (first == -1)
                first = index_;
              if (!(resolver && resolver (game, index_, resolver_arg)))
                continue;
              if (last != -1)
                {
                  if (is_wear)
                    gs_object_player_wear (game, last);
                  else
                    gs_object_player_get (game, last);
                }
              last = index_;
            }
          object = last != -1 ? last : first;
          GS_FOR_EACH_OBJECT (game, index_)
            game->object_references[index_] = (index_ == object);
          count = 1;
        }
    }
}

/*
 * takes() (run370 436280, run380, run390) and drops() (430DDC) walk a
 * crowded line the same way openclose() does: every survivor moves, and
 * the last by index overwrites the message.  Below 3.90 that is the whole
 * rule -- `take red pin` with pins Prefixed "old red" and "new red" is
 * "You pick up new red pin." at 3.70 AND 3.80, and `i` afterwards lists
 * both; `drop red pin` then drops both (p*TAKEP,
 * runner_probes/takep.run370.rtf / runner_probes/takep.run380.rtf,
 * 2026-09-20).  3.90 keeps only the FIRST by index: the same line is "You
 * pick up old red pin." and `i` lists it alone.
 *
 * What survives is the Prefix contest, and which crowd runs it is the
 * version split:
 *
 *   3.80/3.90 have co() under every handler, so the crowd is the TERM's
 *     present namesakes and the block above has already narrowed it.
 *   3.70 has no co() at all, so a crowd is objects sharing a SHORT the
 *     line names (lib_namesake_crowded_pre380), and the filter runs here:
 *     each crowded namesake keeps its reference only if the last word of
 *     its Prefix is typed (lib_co_names_prefix).  With none left the turn
 *     is takes'/drops' own "Take what?" / "Drop what?" -- the "Which ...
 *     would you like to take" strings at 430866 are dead code -- and the
 *     line is answered, so no end-of-turn co() question follows it.
 *
 * Note that a one-word Prefix filters NOTHING below 3.90, because the
 * pre-3.9 co() drops the Prefix's first word before taking its last:
 * `take big gem` over gems Prefixed "big" and "small" is "Take what?" at
 * 3.70 and the co() question at 3.80, exactly as bare `take gem` is,
 * while `take red gem` over "a very red" / "a very blue" answers (p*TAKEP
 * / p*TAKEQ, runner_probes/takep.run*.* and runner_probes/takeq.run*.*).
 */
scr_bool
lib_disambig_t::pick_take_drop_pre400 (void)
{
  scr_int index_;

  if (count > 0 && taf_version < TAF_VERSION_400
      && (strcmp (verb, "take") == 0 || strcmp (verb, "drop") == 0)
      && (count > 1 || taf_version < TAF_VERSION_380))
    {
      const scr_bool is_take = strcmp (verb, "take") == 0;
      const scr_char *line = run_get_dispatch_input ();
      scr_int first = -1, last = -1;

      if (taf_version < TAF_VERSION_380 && line)
        {
          scr_int eligible = 0, kept = 0;

          /*
           * The loop visits every object, not the parser's references: one
           * whose Alias is on the line has it rewritten to its Short first
           * (replacealias, run370 4363F7) and then answers c(Short) like
           * any other.  So an Alias counts as fully as a Short, and `take
           * red ball` over a red and a blue ball both aliased "ball" picks
           * up BOTH, "You pick up the blue ball." -- `drop red ball` drops
           * both the same way (p37SLOT2, harness/make_37_slotprobe.py,
           * run370x runner_probes/slot2.run370.c.rtf, 2026-09-21).  The
           * loop's action is under `If Not c("from")` (43648C), so a
           * take-from line keeps the parser's references.
           */
          for (index_ = 0;
               index_ < gs_object_count (game) && !lib_co_contains (line, "from");
               index_++)
            {
              if (game->object_references[index_]
                  || obj_is_static (game, index_)
                  || !lib_co_candidate (game, index_, gs_playerroom (game))
                  || !lib_names_object_370 (game, line, index_)
                  || lib_names_only_shadowed_370 (game, line, index_))
                continue;
              game->object_references[index_] = TRUE;
              count++;
            }

          /*
           * takes() walks what is loose and drops() what is held, so a
           * namesake in the wrong place is not in the crowd at all: two
           * orbs on the floor are "Drop what?" to nobody, they are drops'
           * ordinary "You don't have a orb!" (p37TAKEP,
           * runner_probes/takep.run370.rtf turn 14).  With nothing
           * eligible the loop never ran, nothing is ambiguous, and the
           * handler's own refusal answers about the first name on the
           * line -- drops' refusal only fills an empty message.  takes'
           * "You've already got" (436561) overwrites, so with every
           * namesake in hand the LAST by index answers: `take ball`
           * holding both balls is "You've already got a blue ball!"
           * (runner_probes/slot2.run370.c.rtf).
           */
          GS_FOR_EACH_OBJECT (game, index_)
            {
              if (game->object_references[index_]
                  && lib_object_held_pre380 (game, index_) != is_take)
                eligible++;
            }
          if (eligible == 0)
            {
              object = -1;
              if (is_take)
                GS_FOR_EACH_OBJECT (game, index_)
                  if (game->object_references[index_]
                      && lib_object_held_pre380 (game, index_))
                    object = index_;
              if (object == -1)
                object = lib_first_named_pre400 (game, -1);
              if (object == -1)
                GS_FOR_EACH_OBJECT (game, index_)
                  if (game->object_references[index_])
                    {
                      object = index_;
                      break;
                    }
              GS_FOR_EACH_OBJECT (game, index_)
                game->object_references[index_] = (index_ == object);
              count = 1;
              return FALSE;
            }

          GS_FOR_EACH_OBJECT (game, index_)
            {
              if (!game->object_references[index_])
                continue;
              if (lib_object_held_pre380 (game, index_) == is_take
                  || (lib_namesake_crowded_pre380 (game, line, index_,
                                                   !is_take)
                      && !lib_co_names_prefix (game, line, index_)))
                game->object_references[index_] = FALSE;
              else
                kept++;
            }
          if (kept == 0)
            {
              lib_what (game, is_take ? "Take" : "Drop");
              disambig.line.prompt_370_blocked = TRUE;
              if (is_ambiguous)
                *is_ambiguous = TRUE;
              return done (-1);
            }
          count = kept;
        }

      GS_FOR_EACH_OBJECT (game, index_)
        {
          if (!game->object_references[index_])
            continue;
          if (first == -1)
            first = index_;
          if (taf_version >= TAF_VERSION_390)
            continue;
          if (last != -1)
            {
              if (is_take)
                gs_object_player_get (game, last);
              else
                gs_object_to_room (game, last, gs_playerroom (game));
            }
          last = index_;
        }

      object = taf_version >= TAF_VERSION_390 ? first : last;
      GS_FOR_EACH_OBJECT (game, index_)
        game->object_references[index_] = (index_ == object);
      count = 1;
    }
  return FALSE;
}

/*
 * No pre-4.0 Runner asks about a crowded open/close line either.
 * openclose() walks the referenced objects and acts on each OPENABLE one,
 * the last by index overwriting the message -- run370, run380 and run390
 * all answer `open rock gem chest` with "You open the chest." (p*OPENW,
 * runner_probes/openw.run370.ox.rtf, runner_probes/openw.run380.ox.rtf,
 * runner_probes/openw.run390.ox.txt, 2026-09-20), and with two closed
 * containers on the line all three answer `open box gem chest` with "You
 * open the chest." and leave the box open as well (p*OPENT, 2026-09-20).
 * So 3.7's every-namesake loop is not 3.7's alone: 3.80 and 3.90 run the
 * same one.
 *
 * The test is "openable at all", not "in the state the verb wants": with
 * the box already open, `open box gem`, `open gem box`, `open gem chest`
 * and `open chest box` are all "The <box|chest> is already open!" at
 * 3.70, 3.80 and 3.90 -- the openable object still wins the line, and the
 * handler's own already-open wording answers for it.
 *
 * With NONE openable the versions part company.  3.80 gave `open` a
 * refusal of its own inside openclose(), which names the LOWEST object
 * index on the line and ends in a bang; `close` never got one, and 3.70
 * has neither, so those fall through to therest()'s can-do tail and its
 * first name by WORD POSITION (lib_first_named_pre400()).
 */
void
lib_disambig_t::pick_open_close_pre400 (void)
{
  scr_int index_;

  if (count > 1 && taf_version < TAF_VERSION_400
      && (strcmp (verb, "open") == 0 || strcmp (verb, "close") == 0)
      && !lib_references_are_namesakes (game))
    {
      const scr_bool is_open = strcmp (verb, "open") == 0;
      const scr_int wanted = is_open ? OBJ_CLOSED : OBJ_OPEN;
      scr_int lowest = -1, last = -1;

      GS_FOR_EACH_OBJECT (game, index_)
        {
          if (!game->object_references[index_])
            continue;
          if (lowest == -1)
            lowest = index_;
          if (gs_object_openness (game, index_) == OBJ_WONTCLOSE)
            continue;
          if (last != -1 && gs_object_openness (game, last) == wanted)
            {
              gs_set_object_openness (game, last,
                                      is_open ? OBJ_OPEN : OBJ_CLOSED);
              /*
               * The overwritten message was still built, contents list and
               * all, and in 3.9 listing is what makes an object seen.  The
               * Gamma Gals (3.90, run390x, gamma_fridge probe 2026-10-09):
               * `open fridge and cupboards` answers for the cupboards
               * alone, and the `get all` after it takes the can of coke
               * from the refrigerator as well as the glass.  3.7 and 3.8
               * are left as they were, unmeasured.
               */
              if (is_open && taf_version >= TAF_VERSION_390)
                {
                  lib_list_t revealed;

                  lib_reveal_children (game, last, OBJ_IN_OBJECT, revealed);
                }
            }
          last = index_;
        }

      if (last != -1)
        object = last;
      else if (is_open && taf_version >= TAF_VERSION_380)
        object = lowest;
      else
        object = lib_first_named_pre400 (game, lowest);
      GS_FOR_EACH_OBJECT (game, index_)
        game->object_references[index_] = (index_ == object);
      count = 1;
    }
}

/*
 * run400's examine narrows by names the LINE contains, not by the longest
 * one: with no present object's Short in the line, every present object
 * whose alias the line holds is a candidate, and a name two of them share
 * raises the question even though a longer alias picks one out.  hub T70
 * `x lower right cupboard` (aliases "lower right cupboard" and "right
 * cupboard" on one, "right cupboard" on the other) answers "Which right
 * cupboard.  The right lower cupboard or the right upper cupboard?"
 * (runner_probes/hub.run400.adj.txt, runner_transcripts/hub.txt).
 * Scarier lets the longer alias pick (deliberate deviation), so that line
 * examines the lower cupboard.
 */
scr_bool
lib_disambig_t::settle_single_or_none (void)
{
  if (count == 1 && lib_is_version_400 (game)
      && strcmp (verb, "examine") == 0
      && lib_co_400_raise_for_contained_aliases (game))
    {
      if (is_ambiguous)
        *is_ambiguous = TRUE;
      return done (-1);
    }

  /* If the reference is unambiguous, set in variables and return it. */
  if (count == 1)
    {
      /* Set this object as referenced. */
      var_set_ref_object (vars, object);

      /* Return, setting no ambiguity. */
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      return done (object);
    }

  /* If nothing referenced, return no object. */
  if (count == 0)
    {
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      else
        {
          pf_buffer_string (filter,
                            "Please be more clear, what do you want to ");
          pf_buffer_string (filter, verb);
          pf_buffer_string (filter, "?\n");
        }
      return done (-1);
    }
  return FALSE;
}

/*
 * A 4.0 take does not ask about every crowd: takes() names its object with
 * 463640 mode 1, whose pending object decides between the question and the
 * flat "It is not clear which <term> you are referring to."  See
 * lib_name_object_resolve_400(), where p4TAKER's ten cells are.
 */
scr_bool
lib_disambig_t::resolve_take_400 (void)
{
  scr_int index_;

  if (count > 1 && lib_is_version_400 (game)
      && strcmp (verb, "take") == 0 && run_get_dispatch_input ())
    {
      const scr_char *line = run_get_dispatch_input ();
      std::vector<scr_int> marked;
      scr_int pending, last_tied, mark_count;

      object = lib_name_object_resolve_400 (game, line, 1, &pending,
                                            &last_tied, &marked,
                                            &mark_count);
      if (object >= 0)
        {
          GS_FOR_EACH_OBJECT (game, index_)
            game->object_references[index_] = (index_ == object);
          var_set_ref_object (vars, object);
          if (is_ambiguous)
            *is_ambiguous = FALSE;
          return done (object);
        }
      if (object == -1 && pending < 0)
        {
          lib_print_not_clear_which_400
            (game, lib_drop_named_term_400 (game, last_tied, line, FALSE));
          pf_buffer_character (filter, '\n');
          if (is_ambiguous)
            *is_ambiguous = TRUE;
          return done (-1);
        }
      /* 4733BD: the term is the LAST tied object's raw Short. */
      if (object == -1 && (scr_int) marked.size () == mark_count)
        {
          lib_co_400_raise_named (game,
                                  prop_get_indexed_string (bundle, "Objects",
                                                           last_tied, "Short"),
                                  marked);
          if (is_ambiguous)
            *is_ambiguous = TRUE;
          return done (-1);
        }
    }
  return FALSE;
}

/*
 * 4.0 asks the Runner's own question instead.  "Please be more clear, what
 * do you want to <verb>?" is a SCARE invention -- the string is in none of
 * the four Runner binaries -- and what run400 really prints where two
 * present objects answer to the typed noun is the same "Which <term>.
 * <list>?" the 3.7/3.8 scan above raises (see lib_co_400_raise()).  Away
 * from examine no 4.0 command reaches the listing at all: a crowd that
 * asks nothing hands the handler no object, and the command goes on to
 * its own %text% row.
 */
scr_bool
lib_disambig_t::raise_crowd_400 (void)
{
  if (lib_is_version_400 (game))
    {
      std::vector<scr_int> crowd;

      /*
       * A line with " with " in it asks about one half; see
       * lib_with_split_crowd_400().  `close box with stone` lists the two
       * stones and not the box, where an examine whose own half ties keeps
       * the whole line's list below.
       */
      const scr_bool examine = strcmp (verb, "examine") == 0;
      scr_bool raised;

      scr_int with_pending, with_head;

      if (lib_with_split_crowd_400 (game, examine, &crowd, &with_pending,
                                    &with_head))
        {
          /* An examine whose head names one object describes it; see
             lib_with_split_crowd_400(). */
          if (examine && with_head >= 0)
            {
              var_set_ref_object (vars, with_head);
              if (is_ambiguous)
                *is_ambiguous = FALSE;
              return done (with_head);
            }
          raised = with_pending >= 0
                   ? lib_co_400_raise_for_with_tail (game, with_pending, crowd)
                   : lib_co_400_raise_for_short_tie (game, crowd);
        }
      else if (examine)
        raised = lib_co_400_raise_for_references (game);
      else
        {
          /*
           * Away from examine the crowd is 463640's, over the whole line,
           * and not the reference set our own `%object% *` row bound: a
           * second noun of the same score joins it, and a crowd it leads
           * asks nothing.  p4WTIE `cut stone knife` is therest's "You
           * can't cut that." where `cut stone` asks
           * (runner_probes/wtie.run400.w6.txt, 2026-09-20) -- the knife
           * is the crowd's first object, so the stones never park a
           * pending object.
           */
          raised = lib_co_400_raise_for_pending_tie (game);
        }
      if (raised)
        {
          if (is_ambiguous)
            *is_ambiguous = TRUE;
          return done (-1);
        }
      if (!examine)
        {
          /*
           * And a 4.0 crowd that asks nothing leaves the handler with no
           * object at all rather than a listing: the command goes on to
           * its own %text% row, which is where "You can't cut that." and
           * "You can't open that." come from
           * (runner_probes/wtie.run400.w6.txt `cut stone knife`, `open
           * box knife`, 2026-09-20).  The invented listing below is
           * 3.9-and-below's alone.
           */
          if (is_ambiguous)
            *is_ambiguous = FALSE;
          return done (-1);
        }
    }
  return FALSE;
}

/*
 * Two or more DIFFERENT present objects on a 4.0 examine line -- `x coin
 * and the hat`, which the splitter keeps whole because "the" names no
 * object -- never raise a question.  examines() takes referencedob's
 * pick (Proc_19_88_457034, lib_examine_referencedob_400()): pass C
 * counts each candidate's Prefix words in the line, and an equal count
 * is &HFE, "Sorry, I'm not sure which object you're referring to."
 * (4719EA, still a turn); no Prefix word typed leaves the last marked.
 * Measured on p4AND (run400 runner_probes/and.run400.txt,
 * runner_probes/and.run400.b.txt): `x coin and a hat` -> the Sorry line,
 * "a" being both objects' Prefix; `x coin and the hat`, `x coin and hat
 * and box` -> one description.
 */
scr_bool
lib_disambig_t::resolve_examine_400 (void)
{
  if (lib_is_version_400 (game) && strcmp (verb, "examine") == 0
      && run_get_dispatch_input ())
    {
      const scr_int pick
        = lib_examine_referencedob_400 (game, run_get_dispatch_input ());

      if (pick == -2)
        {
          pf_buffer_string (filter, "Sorry, I'm not sure which object"
                                    " you're referring to.\n");
          if (is_ambiguous)
            *is_ambiguous = TRUE;
          return done (-1);
        }
      if (pick >= 0)
        {
          var_set_ref_object (vars, pick);
          if (is_ambiguous)
            *is_ambiguous = FALSE;
          return done (pick);
        }
    }
  return FALSE;
}

/*
 * 3.9 examine asks nothing either.  co() is false for each of two seen,
 * present namesakes (it only raises the end-of-turn flag), so
 * referencedob() counts none and returns -1, and examines() answers
 * "Nothing special." (run390 42DF43 -> 44BF94).  The "Which <term>." that
 * lib_co_ambiguity_prompt() prints replaces it when no task ran;
 * cybercow_win T118 `x berry` is the case where the "#Rain" event's task
 * did run.
 *
 * When co() is true for SEVERAL, referencedob() runs a second pass and
 * examines() answers "Please examine one object at a time."; the crowd
 * never reaches here, lib_examine_crowded_390() takes it first.
 */
scr_int
lib_disambig_t::list_choices (void)
{
  scr_int index_, listed;

  if (lib_is_version_390 (game) && strcmp (verb, "examine") == 0)
    {
      pf_buffer_string (filter, "Nothing special.\n");
      if (is_ambiguous)
        *is_ambiguous = TRUE;
      return -1;
    }

  /* The object reference is ambiguous, so list the choices. */
  pf_buffer_string (filter, "Please be more clear, what do you want to ");
  pf_buffer_string (filter, verb);
  pf_buffer_string (filter, "?  ");

  pf_new_sentence (filter);
  listed = 0;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (game->object_references[index_])
        {
          lib_print_object_np (game, index_);
          listed++;
          if (listed < count)
            pf_buffer_string (filter, (listed < count - 1) ? ", " : " or ");
        }
    }
  pf_buffer_string (filter, "?\n");

  /* Return no object for an ambiguous reference. */
  if (is_ambiguous)
    *is_ambiguous = TRUE;
  return -1;
}

scr_int
lib_disambiguate_object_common (scr_gameref_t game, const scr_char *verb,
                               scr_bool (*resolver)
                                   (scr_gameref_t, scr_int, scr_int),
                               scr_int resolver_arg,
                               scr_bool *is_ambiguous)
{
  lib_disambig_t d (game, verb, resolver, resolver_arg, is_ambiguous);

  d.gather_candidates ();
  if (d.narrow_named_pre400 ())
    return d.result;
  d.narrow_top_scored_400 ();
  d.narrow_with_resolver ();
  if (d.ask_which_examine_pre390 ())
    return d.result;
  d.pick_wear_remove_pre380 ();
  if (d.pick_take_drop_pre400 ())
    return d.result;
  d.pick_open_close_pre400 ();
  if (d.settle_single_or_none () || d.resolve_take_400 ()
      || d.raise_crowd_400 () || d.resolve_examine_400 ())
    return d.result;
  return d.list_choices ();
}

scr_int
lib_disambiguate_object (scr_gameref_t game,
                         const scr_char *verb, scr_bool *is_ambiguous)
{
  return lib_disambiguate_object_common (game, verb, nullptr, -1, is_ambiguous);
}


/*
 * lib_absent_seen_object()
 * lib_cant_see_absent_object()
 *
 * 4.0's matcher runs a second pass.  When nothing the noun names is in the
 * room, it looks again over everything the player has *seen*, and the
 * handlers above it then answer "<player> can't see <it>" instead of their
 * own generic refusal.  Pre-4.0 has no such pass: run390's co() simply fails
 * to match an object that is elsewhere, and the command falls through to the
 * flat can't-do tail.
 *
 * Measured on p4EXAM.taf, one statue seen in the North Room and examined
 * from the Test Room (runner_probes/exam.run400.txt, all 32 commands
 * echoed) against p39EXAM.taf under run390 (runner_probes/exam.run390.txt,
 * runner_probes/exam.run390.held.txt):
 *
 *     command        run400                              run390
 *     x statue       You can't see the statue from here! Nothing special.
 *     open statue    You can't see the statue.           You can't open that.
 *     close statue   You can't see a statue.             You can't close that.
 *     buy statue     You can't see the statue.           I don't think that
 *                                                          is for sale.
 *
 * Note the article: `close` alone is indefinite.  That is not a stylistic
 * choice, it is a different piece of code -- run400's openclose() composes
 * the open half from the definite-name helper Proc_21_31_448710 (475966) and
 * the close half by hand, Prefix & " " & Short (475C10-475C2D).  `examine`
 * uses the definite helper and appends " from here!" (471958-471975), and
 * `buy` never reaches its own branch at all: therest()'s very first clause
 * (4887A0-4887F5) prints the definite form and exits before the whole verb
 * chain below it.
 *
 * Only those four verbs are measured, so only those four call this.  The
 * therest() clause is verb-wide in the Runner, and the tail of
 * lib_cmd_verb_object() already models the same sentence for anything that
 * reaches it; what is unmeasured is which of the other therest() verbs 4.0
 * intercepts on the way in.
 *
 * WHERE THE CALLS GO MATTERS.  Every one of the Runner's four sites is
 * guarded by "the output buffer is still empty" (471933, 475952, 475BFC,
 * 4887A0 all test MemVar_4941B0 = vbNullString), i.e. the clause speaks only
 * when nothing else in the turn has.  So the callers are four thin handlers
 * sitting immediately above the catch-all `*` rows in scrunner.cpp, not the
 * `%object%` handlers at the top of the table: humbug names an NPC and an
 * absent object both "robot", and unraveling_god an NPC and an absent object
 * both "people", and in each case the Runner and the golden print the NPC's
 * description.  Checking inside lib_cmd_examine_object() would have stolen
 * the turn from lib_cmd_examine_npc() one row below it.  Each `_absent` row
 * re-matches %object%, which repopulates the references that
 * lib_disambiguate_object_common() cleared on the way past.
 *
 * One deliberate deviation: the close half is spelled Prefix & " " & Short
 * with no fallback, so an object with an empty Prefix would give the Runner
 * "You can't see  statue."  lib_print_object() substitutes the usual "a "
 * there instead.  Nothing has measured an empty-Prefix object in this
 * position, and the double space is almost certainly not what 4.0 meant.
 *
 * The gate is the object's seen byte, +48 in the Runner's object record,
 * written the moment an object is listed or described (run400 471749,
 * 46A142) and read by every one of the four sites above.  It is Scarier's
 * gs_object_seen(), so this needs no new state.
 */
/*
 * Whole-word containment of a single name word in the typed line, run400
 * Proc_21_38_454CB0: case-insensitive, and a hit only where the word is
 * bounded by the line's ends or spaces.  Public for run_all_commands()'s
 * recovery gate, the `c("status")` test at 47DCA1.
 *
 * The Runner's c() also ends a word at ",", "." or "?" (454C07-454C39),
 * which is what lets "coin," score on `drop coin, hat`; the 4.0-only
 * callers below take that through lib_input_contains_word_400().  The
 * 3.9 helper's terminators are unmeasured, so the paths shared with 3.9
 * keep the space-only test.
 */
static scr_bool
lib_input_contains_word_ended (const scr_char *input, const scr_char *word,
                               const scr_char *terminators)
{
  const scr_int length = strlen (word);
  const scr_char *scan;

  if (length == 0)
    return FALSE;

  for (scan = input; *scan != NUL; scan++)
    {
      if ((scan == input || scan[-1] == ' ')
          && scr_strncasecmp (scan, word, length) == 0
          && (scan[length] == NUL
              || strchr (terminators, scan[length]) != nullptr))
        return TRUE;
    }

  return FALSE;
}

scr_bool
lib_input_contains_word (const scr_char *input, const scr_char *word)
{
  return lib_input_contains_word_ended (input, word, " ");
}

scr_bool
lib_input_contains_word_400 (const scr_char *input, const scr_char *word)
{
  return lib_input_contains_word_ended (input, word, " ,.?");
}
