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
 * Wield, kiss, buy, break, smell, sell and eat.
 *
 * See sclibrar.h for what the library files share.
 */

#include "sclibrar.h"


/*
 * lib_cmd_wield()
 *
 * Battle System weapon selection.  "wield <weapon>" sets a held weapon as the
 * player's persistent wield; the wield is cleared -- not swapped for another
 * carried weapon -- when the weapon leaves the player's hands.  There is no
 * "unwield" verb (the Runner answers "I don't understand.").  Falls through to
 * other grammar when the Battle System is disabled, as plain wielding is not
 * otherwise modelled.
 *
 * dobattle's wield block (run390 44C824-44CAB6, run400 47E764-47E9xx) loops
 * over the objects co() names, and handles one only when
 *
 *     InStr(line, Short) > InStr(line, "wield")
 *       Or InStr(line, Alias(0)) > InStr(line, "wield")
 *
 * a binary InStr of the raw field against the lower-cased line.  A Short or
 * first Alias with a capital letter can never pass, and an empty one is
 * InStr's 1, which beats "wield" only when the line does not start with it.
 * With no object passing, the line gets "I don't understand what you are
 * wanting to wield!" -- an ordinary turn.
 *
 * Measured 2026-09-14 on Villains_And_Kings under run390
 * (runner_probes/villains_and_kings.run390.wield.txt): the sword is Prefix
 * "Kinda Sharp", Short "Sword", Alias "blade".  `wield Sword`, `wield kinda
 * sharp sword`, `wield zzz`, `wield rack` (Short "Rack", empty alias) and a
 * bare `wield` all refuse; `wield blade` wields, then "already wielding", then
 * after a drop "aren't carrying"; 21 lines, 21 turns.  The 4.0 twin has the
 * same shape, from the decompile only.
 */
static scr_bool
lib_wield_names_object (scr_gameref_t game, scr_int object,
                        const scr_char *input)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  std::string line (input ? input : "");
  const scr_char *shortname, *alias = "";
  std::string::size_type at;
  scr_int wield_at, short_at, alias_at;
  scr_vartype_t vt_key[4];

  for (std::string::iterator c = line.begin (); c != line.end (); ++c)
    *c = scr_tolower (*c);

  at = line.find ("wield");
  wield_at = at == std::string::npos ? 0 : (scr_int) at + 1;

  /*
   * Deliberate deviation (2026-09-30): the names are lower-cased like the
   * line, and every Alias counts.  The Runner's test finds a Short or first
   * Alias as authored in the lower-cased line, so a weapon called
   * "Excalibur" or "Sword" could never be wielded, and neither could one
   * named by its second Alias (bec2def78).
   */
  auto lowered = [] (const scr_char *name) -> std::string
    {
      std::string text (name ? name : "");

      for (char &c : text)
        c = scr_tolower (c);
      return text;
    };

  /* The Runner's own test, for SCR_TRACE_DEVIATIONS. */
  auto runner_names = [&] () -> scr_bool
    {
      const scr_char *const names[] = {
        prop_get_indexed_string (bundle, "Objects", object, "Short"),
        lib_alias_prepare (bundle, vt_key, "Objects", object) > 0
          ? (vt_key[3].integer = 0,
             prop_get_string (bundle, "S<-sisi", vt_key)) : ""
      };

      for (const scr_char *name : names)
        {
          const std::string::size_type hit = line.find (name ? name : "");
          if ((hit == std::string::npos ? 0 : (scr_int) hit + 1) > wield_at)
            return TRUE;
        }
      return FALSE;
    };

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  at = line.find (lowered (shortname));
  short_at = at == std::string::npos ? 0 : (scr_int) at + 1;
  if (short_at > wield_at)
    {
      if (SCR_TRACING_DEVIATIONS () && !runner_names ())
        SCR_DEVIATION ("wield_name", "object=%ld", object);
      return TRUE;
    }

  alias_at = 0;
  {
    const scr_int alias_count = lib_alias_prepare (bundle, vt_key, "Objects",
                                                   object);
    scr_int index_;

    for (index_ = 0; index_ < alias_count; index_++)
      {
        vt_key[3].integer = index_;
        alias = prop_get_string (bundle, "S<-sisi", vt_key);
        if (scr_strempty (alias))
          continue;
        at = line.find (lowered (alias));
        alias_at = at == std::string::npos ? 0 : (scr_int) at + 1;
        if (alias_at > wield_at)
          {
            if (SCR_TRACING_DEVIATIONS () && !runner_names ())
              SCR_DEVIATION ("wield_name", "object=%ld", object);
            return TRUE;
          }
      }
  }
  return FALSE;
}

static void
lib_wield_not_understood (scr_gameref_t game)
{
  pf_buffer_string (gs_get_filter (game),
                    "I don't understand what you are wanting to wield!\n");
}

scr_bool
lib_cmd_wield (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;

  if (!battle_is_enabled (game))
    return FALSE;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "wield", nullptr);
  if (object == -1)
    return TRUE;

  if (!lib_wield_names_object (game, object, run_get_dispatch_input ()))
    {
      lib_wield_not_understood (game);
      return TRUE;
    }

  /* The weapon must be held, and must actually be a weapon.  The Runner's
   * refusal is "Player aren't carrying the rock!" [sic] (probe pWS2). */
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
    {
      lib_print_response_object (game,
                                 "You aren't carrying ",
                                 "I am not carrying ",
                                 "%player% aren't carrying ",
                                 object, "!\n");
      return TRUE;
    }
  if (!battle_is_weapon (game, object))
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_plurality (game, object, " is", " are"));
      pf_buffer_string (filter, " not a weapon!\n");
      return TRUE;
    }

  /* Wielding the already-wielded weapon is acknowledged, not repeated. */
  if (gs_playerwield (game) == object)
    {
      lib_print_response_object (game,
                                 "You are already wielding ",
                                 "I am already wielding ",
                                 "%player% is already wielding ",
                                 object, ".\n");
      return TRUE;
    }

  gs_set_playerwield (game, object);
  lib_print_response_object (game,
                             "You wield ",
                             "I wield ",
                             "%player% wield ",
                             object, ".\n");
  return TRUE;
}

/* A wield line naming no object; see lib_cmd_wield(). */
scr_bool
lib_cmd_wield_other (scr_gameref_t game)
{
  if (!battle_is_enabled (game))
    return FALSE;

  lib_wield_not_understood (game);
  return TRUE;
}


/*
 * lib_cmd_kiss_npc()
 * lib_cmd_kiss_object()
 * lib_cmd_kiss_other()
 *
 * Reject romantic advances in all cases.
 */
scr_bool
lib_cmd_kiss_npc (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int npc, gender;
  scr_bool is_ambiguous;

  /*
   * Pre-4.0 has no present-character kiss: 3.9's characters() arm names the
   * FIRST character the line names, here or not, and 3.7/3.8 have no arm at
   * all, leaving therest's "I'm not sure it would appreciate that." (run380
   * 4451EC; `kiss dave`, run370x runner_probes/npcamb.run370.one.rtf, run380x
   * runner_probes/npcamb.run380.one.rtf).  Both are lib_cmd_kiss_other()'s;
   * `kiss guard` with Ann and Bob here is Ann's "she" at 3.9 (run390x
   * runner_probes/npcamb.run390.txt).
   */
  if (!lib_is_version_400 (game))
    return FALSE;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, "kiss", &is_ambiguous);
  if (npc == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Gender";
  gender = prop_get_integer (bundle, "I<-sis", vt_key);

  switch (gender)
    {
    case NPC_MALE:
      pf_buffer_string (filter, "I'm not sure he would appreciate that!\n");
      break;

    case NPC_FEMALE:
      pf_buffer_string (filter, "I'm not sure she would appreciate that!\n");
      break;

    case NPC_NEUTER:
      pf_buffer_string (filter, "I'm not sure it would appreciate that!\n");
      break;

    default:
      scr_error ("lib_cmd_kiss_npc: unknown gender, %ld\n", gender);
    }
  return TRUE;
}

scr_bool
lib_cmd_kiss_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "kiss", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  lib_print_wrapped_object (game, "I'm not sure ",
                            object, " would appreciate that.\n");
  return TRUE;
}

/*
 * A kiss naming a character who is not here.  characters()' kiss branch
 * has no in-room test: for the first NPC the line names (lib_npc_referenced)
 * it overwrites an empty buffer, or therest's "I'm not sure it would
 * appreciate that.", with "I'm not sure " & <gender pronoun> & " would
 * appreciate that!" (run400 47F7E2-47F83A, run390 45970A).  Measured on
 * the_pk_girl under run400x (runner_probes/thepkgirl.run400.site.txt, turns
 * 288 and 398): `kiss katryn` with Katryn elsewhere answers "I'm not sure she
 * would appreciate that!".  3.9 is from the decompile alone.
 */
static scr_bool
lib_kiss_named_npc (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int npc;

  if (input && prop_get_taf_version (bundle) >= TAF_VERSION_390)
    {
      GS_FOR_EACH_NPC (game, npc)
        {
          scr_vartype_t vt_key[3];

          if (!lib_npc_referenced (game, npc, input))
            continue;

          vt_key[0].string = "NPCs";
          vt_key[1].integer = npc;
          vt_key[2].string = "Gender";
          switch (prop_get_integer (bundle, "I<-sis", vt_key))
            {
            case NPC_MALE:
              return lib_print_message (game,
                  "I'm not sure he would appreciate that!\n");
            case NPC_FEMALE:
              return lib_print_message (game,
                  "I'm not sure she would appreciate that!\n");
            default:
              return lib_print_message (game,
                  "I'm not sure it would appreciate that!\n");
            }
        }
    }
  return FALSE;
}

scr_bool
lib_cmd_kiss_other (scr_gameref_t game)
{
  if (lib_kiss_named_npc (game))
    return TRUE;

  /* Reject this attempt. */
  return lib_print_message (game, "I'm not sure it would appreciate that.\n");
}

/*
 * lib_cmd_kiss_ended_400()
 *
 * The kiss row of a 4.0 line whose task has just ended the game.  The
 * characters() kiss block (run400 47F7E2-47F83A) runs from the generaltasks
 * tail at 48B56E and sits above the gameover exit at 4805CD, so it still
 * answers; therest's "I'm not sure it would appreciate that." is past the
 * jump at 48AC62 and does not.  night's `kiss rachel` (task 4, no
 * CompleteText, ends the game, Rachel left outside the car) is "I'm not sure
 * she would appreciate that!" ahead of the WinText in
 * runner_transcripts/night.txt.
 */
scr_bool
lib_cmd_kiss_ended_400 (scr_gameref_t game)
{
  return lib_kiss_named_npc (game);
}


/*
 * lib_cmd_buy_object()
 * lib_cmd_buy_other()
 *
 * Standard responses to attempts to buy something.
 */
scr_bool
lib_cmd_buy_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "buy", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  pf_buffer_string (filter, "I don't think ");
  lib_print_object_np (game, object);
  pf_buffer_string (filter,
                    lib_select_plurality (game, object, " is", " are"));
  pf_buffer_string (filter, " for sale.\n");
  lib_non_answer = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_buy_other (scr_gameref_t game)
{
  /* Reject this attempt. */
  lib_non_answer = TRUE;
  return lib_print_message (game, "I don't think that is for sale.\n");
}

/*
 * lib_cmd_buy_absent()
 *
 * 4.0's therest() opens with the same clause, before any of its verb
 * branches; `buy` is the one that has been measured.  See
 * lib_absent_seen_object().
 */
scr_bool
lib_cmd_buy_absent (scr_gameref_t game)
{
  /* 3.7's therest() clause; 3.8 has none.  See lib_absent_named_object_pre_390(). */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380)
    {
      const scr_int object = lib_absent_named_object_pre_390 (game);

      if (object != -1)
        return lib_cant_see_named_pre_390 (game, object, TRUE, ".\n");
    }

  return lib_cant_see_absent_object (game, ".\n", TRUE);
}


/*
 * lib_cmd_break_object()
 * lib_cmd_break_other()
 *
 * Standard responses to attempts to break something.
 *
 * The break arm takes the " with " clause's refusals like every other
 * therest arm -- `break rock with gem`, the gem present and not held, is
 * "<You> don't have the gem." in all four Runners -- but writes no " with
 * <X>" suffix of its own once the instrument is held: the answer is then
 * the plain "<You> might need the rock." (p*WITHPFX,
 * harness/make_withprefixprobe.py; runner_probes/withpfx.run370.pfx4.rtf /
 * runner_probes/withpfx.run380.ws.rtf / runner_probes/withpfx.run390.ws.txt /
 * runner_probes/withpfx.run400.ws.txt, 2026-09-20).  Below 3.90 the arm ends
 * in an exclamation mark, with or without an instrument: run370/run380 answer
 * a bare `break rock` "You might need the rock!" where run390/run400 use a
 * full stop (harness/make_withprefixprobe.py;
 * runner_probes/withpfx.run370.wx.rtf, runner_probes/withpfx.run380.wx.rtf,
 * runner_probes/withpfx.run390.wx.txt, runner_probes/withpfx.run400.wx.txt).
 */
scr_bool
lib_cmd_break_object (scr_gameref_t game)
{
  scr_int object;
  scr_int unused_object, instrument;
  scr_bool is_ambiguous;

  switch (lib_with_clause_400 (game, &unused_object, &instrument))
    {
    case LIB_WITH_NONE:
    case LIB_WITH_SUFFIX:
      break;
    case LIB_WITH_DECLINE:
      return FALSE;
    case LIB_WITH_ANSWERED:
      return TRUE;
    }

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "break", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  lib_print_response_object (game,
                             "You might need ",
                             "I might need ",
                             "%player% might need ",
                             object,
                             (prop_get_taf_version (gs_get_bundle (game))
                              < TAF_VERSION_390) ? "!\n" : ".\n");
  return TRUE;
}

scr_bool
lib_cmd_break_other (scr_gameref_t game)
{
  /* Reject this attempt. */
  return lib_print_response_message (game,
                                     "You might need that.\n",
                                     "I might need that.\n",
                                     "%player% might need that.\n");
}

/*
 * lib_cmd_break_absent()
 *
 * 4.0's therest() opens with the same clause as lib_cmd_buy_absent(), and
 * `break`/`destroy`/`smash` are the arm measured here: escape_to_new_york
 * turn 159 `smash gate`, the gate seen but absent, answers "You can't see
 * the metal gate." rather than break_object's "You might need the metal
 * gate." (Ticket run400 xoshiro trace 2026-09-12).  See
 * lib_absent_seen_object(); run400 composes break_object's own refusal at
 * 489A62-489AF2, and a bare verb falls to "Smash what?" at 4455F8.
 */
scr_bool
lib_cmd_break_absent (scr_gameref_t game)
{
  return lib_cant_see_absent_object (game, ".\n", TRUE);
}

/*
 * lib_cmd_turn_absent()
 *
 * The `turn` arm of the same therest() clause.  hcw (4.00,
 * runner_probes/hcw.run400.txt turn 81) types `turn on intercom` at the park
 * gates with the limousine's intercom seen but elsewhere and gets "You can't
 * see the intercom." -- not turn_other's "You can't turn that.", which run400
 * composes only below the clause, at 489255-489367.
 */
scr_bool
lib_cmd_turn_absent (scr_gameref_t game)
{
  return lib_cant_see_absent_object (game, ".\n", TRUE);
}

/*
 * lib_cmd_verb_absent_400()
 *
 * The same therest() clause for verbs whose own rows take no %object% the
 * clause could read back: the candidates are every object the typed line
 * names anywhere, scored as Proc_21_58_463640 scores them, not just what a
 * pattern bound.  Measured on warlord (4.00,
 * runner_probes/warlord.run400.txt, xoshiro seed 33):
 *
 *     push barrel           the barrel rolled away    You can't see the barrel.
 *     stand on platform     raised platform elsewhere You can't see the raised
 *                                                       platform.
 *     give wine to leonora  wine unseen, "leonora"    You can't see the photo.
 *                           an alias of the photo
 *
 * where Scarier said "You push, but nothing happens.", "You can't stand on
 * that." and "Give what?".  Pull is the same: grumble T207 `pull button`,
 * the button left behind in another room, is "You can't see the button.".
 * The give line shows why the whole line counts: therest's give arm (488A09)
 * is below the clause, and the object it names is the photo, not the wine.
 * 4.0 only; declines otherwise.
 */
scr_bool
lib_cmd_verb_absent_400 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int object;

  if (!input || !lib_is_version_400 (game))
    return FALSE;

  gs_clear_object_references (game);
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (lib_verb_object_name_score (game, object, input) > 0)
        game->object_references[object] = TRUE;
    }
  return lib_cant_see_absent_object (game, ".\n", TRUE);
}


/*
 * lib_cmd_smell_object()
 * lib_cmd_smell_other()
 *
 * Standard responses to attempts to smell something.
 */
scr_bool
lib_cmd_smell_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "smell", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  pf_new_sentence (filter);
  lib_print_object_np (game, object);
  pf_buffer_string (filter, " smells normal.\n");
  return TRUE;
}

scr_bool
lib_cmd_smell_other (scr_gameref_t game)
{
  /* Reject this attempt. */
  return lib_print_message (game, "That smells normal.\n");
}


/*
 * lib_cmd_sell_object()
 * lib_cmd_sell_other()
 *
 * Standard responses to attempts to sell something.
 */
scr_bool
lib_cmd_sell_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "sell", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  lib_print_wrapped_object (game, "No-one is interested in buying ",
                            object, ".\n");
  return TRUE;
}

scr_bool
lib_cmd_sell_other (scr_gameref_t game)
{
  return lib_print_message (game, "No-one is interested in buying that.\n");
}


/*
 * lib_cmd_eat_object()
 *
 * Consume edible objects.
 */
scr_bool
lib_cmd_eat_object (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int object;
  scr_bool edible, is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "eat", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /*
   * Every Runner asks whether the object is edible BEFORE whether it is
   * held: an inedible one is "You can't eat X." wherever it is, and only
   * an edible one gets the held test, whose refusal ends in "!" (run400
   * 4888AF/488921/488993, run390 45D52E, run380 443D8B, run370 43D332).
   * p4WTIE `eat red stone`, the stone on the floor: "You can't eat the red
   * stone." (run400 runner_probes/wtie.run400.it1.txt, 2026-09-21).
   */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Edible";
  edible = !obj_is_static (game, object)
           && prop_get_boolean (bundle, "B<-sis", vt_key);
  if (!edible)
    {
      lib_print_response_object (game,
                                 "You can't eat ",
                                 "I can't eat ",
                                 "%player% can't eat ",
                                 object, ".\n");
      return TRUE;
    }

  /* Check that we have the object to eat. */
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
    {
      lib_print_response_object (game,
                                 "You are not holding ",
                                 "I am not holding ",
                                 "%player% is not holding ",
                                 object, "!\n");
      return TRUE;
    }

  /* Confirm, and hide the object. */
  lib_print_response_object (game,
                             "You eat ",
                             "I eat ",
                             "%player% eat ",
                             object,
                             ".  Not bad, but it could do with a"
                             " pinch of salt!\n");
  gs_object_make_hidden (game, object);
  return TRUE;
}
