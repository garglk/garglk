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
 * Module notes:
 *
 * o ADRIFT's optional Battle System, split out of scnpcs.cpp.  The public
 *   entry points are declared in scbattle.h and the per-character mutable
 *   state (scr_battle_s) lives in scgamest.h.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <string>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "scprintf.h"
#include "scgamest_api.h"
#include "sclibrar_api.h"
#include "scrunner_api.h"
#include "sctasks.h"
#include "scnpcs.h"
#include "scbattle.h"
#include "sclocale.h"
#include "scdump.h"
#include "scgamest.h"


/*
 * Battle system.
 *
 * ADRIFT's optional Battle System gives the player and characters combat
 * attributes -- stamina, strength, accuracy, defence and agility -- and
 * resolves fights when enemies share a room.  The system is enabled per game
 * by the Globals.BattleSystem flag.  Attribute values are configured as [lo,
 * hi] ranges (or single values in version 3.9 games), and, with the exception
 * of stamina, are re-rolled randomly within their range each time they are
 * needed.  Stamina is rolled once at game start and then tracked as mutable
 * game state, as it is depleted by attacks and topped up by recovery.
 *
 * This first group of functions covers reading the configured attributes and
 * initialising stamina; combat resolution builds on these.
 */


/*
 * Optional "combat assist" mode (off by default, opt-in via
 * scr_set_combat_assist).  Many amateur ADRIFT games enable the Battle System
 * but leave every character's Accuracy and Agility at the editor's default of
 * 0.  Because ADRIFT decides a hit with the strict test accuracy > agility
 * before applying strength - defence damage, 0 > 0 never passes and no blow
 * ever lands, silently disabling combat the author plainly intended (they did
 * configure strength/defence/stamina).  When this mode is on AND a game has no
 * configured accuracy or agility anywhere (battle_unconfigured, detected at
 * battle_start), the hit roll is treated as an automatic hit, so combat plays
 * out on the author's strength-vs-defence basis.  This deliberately diverges
 * from the reference Runner, so it is strictly opt-in; games that do configure
 * accuracy/agility (e.g. Sun Empire) are never affected, even with it on.
 */
static scr_bool battle_combat_assist = FALSE;
static scr_bool battle_unconfigured = FALSE;

/* Trace flag, read once from the environment (SCR_TRACE_BATTLE). */
static const scr_bool battle_trace = (getenv ("SCR_TRACE_BATTLE") != nullptr);


/*
 * Legacy (version 3.9 / 3.8) combat model.
 *
 * The reverse-engineered ADRIFT 3.9 Runner (run390) uses a much simpler combat
 * model than version 4.0: characters have only Stamina, Strength and Defence
 * (single scalars, not [lo,hi] ranges), and there is no Accuracy, Agility,
 * Recovery, Max-Stamina or StaminaTask.  An attack is resolved by the strictly
 * deterministic test "hit strength (Strength + weapon HitValue) > armour
 * strength (Defence + worn ProtectionValue)"; there is no separate accuracy/
 * agility dodge step and, crucially, no "manages to avoid" outcome -- every
 * attack connects, and armour merely absorbs the blow ("...but it doesn't seem
 * to do any damage." when Defence >= Strength).
 *
 * Scarier otherwise implements the 4.0 model, whose hit test is accuracy >
 * agility.  A 3.9 game has no Accuracy/Agility properties, so both read as 0
 * and the test 0 > 0 never passes, silently making all combat an endless
 * stalemate.  When battle_legacy is set (detected from the game version at
 * battle_start), battle_resolve skips the accuracy/agility test and always
 * connects, letting the existing Strength - Defence damage path -- which
 * already matches the 3.9 formula and message -- decide the outcome.
 */
static scr_bool battle_legacy = FALSE;


/*
 * battle_set_combat_assist()
 * battle_get_combat_assist()
 *
 * Set and read the opt-in combat assist flag described above.
 */
void
battle_set_combat_assist (scr_bool flag)
{
  battle_combat_assist = flag;
}

scr_bool
battle_get_combat_assist (void)
{
  return battle_combat_assist;
}


/*
 * battle_is_legacy_version()
 *
 * Return TRUE for a version 3.9 or 3.8 game, read from the bundle's top-level
 * "Version" integer (written by the parser).  A missing or unrecognised version
 * is treated as modern (4.0), leaving behaviour unchanged.
 */
static scr_bool
battle_is_legacy_version (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key, vt_rvalue;

  vt_key.string = "Version";
  if (prop_get (bundle, "I<-s", &vt_rvalue, &vt_key))
    return vt_rvalue.integer < TAF_VERSION_400;
  return FALSE;
}


/*
 * battle_is_enabled()
 *
 * Return TRUE if the game has the Battle System turned on.
 */
scr_bool
battle_is_enabled (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  return prop_get_global_boolean (bundle, "BattleSystem");
}


/*
 * battle_get_property()
 *
 * Read a named integer from the Battle properties of the player (npc < 0) or
 * of a given NPC.  Returns the supplied fallback if the property is absent.
 */
static scr_int
battle_get_property (scr_gameref_t game, scr_int npc,
                     const scr_char *name, scr_int fallback)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4], vt_rvalue;

  if (npc < 0)
    {
      vt_key[0].string = "Globals";
      vt_key[1].string = "Battle";
      vt_key[2].string = name;
      if (prop_get (bundle, "I<-sss", &vt_rvalue, vt_key))
        return vt_rvalue.integer;
    }
  else
    {
      vt_key[0].string = "NPCs";
      vt_key[1].integer = npc;
      vt_key[2].string = "Battle";
      vt_key[3].string = name;
      if (prop_get (bundle, "I<-siss", &vt_rvalue, vt_key))
        return vt_rvalue.integer;
    }
  return fallback;
}


/*
 * battle_bundle_range()
 *
 * Return the configured [lo,hi] range of a named attribute ("Stamina",
 * "Strength", "Accuracy", "Defense", "Agility") for the player (npc < 0) or an
 * NPC, read directly from the game bundle.  Version 4.0 games store separate
 * <name>Lo and <name>Hi values; version 3.9 games store a single <name> value,
 * which we treat as a degenerate range.  Character attributes are never
 * negative, so a negative probe result unambiguously signals an absent Hi
 * property.  This is the configured (immutable) seed; runtime reads go through
 * battle_attribute_range() below, which serves the mutable state.
 */
static void
battle_bundle_range (scr_gameref_t game, scr_int npc,
                     const scr_char *base, scr_int *lo, scr_int *hi)
{
  scr_char name[32];
  scr_int high;

  snprintf (name, sizeof (name), "%sHi", base);
  high = battle_get_property (game, npc, name, -1);
  if (high < 0)
    {
      /* No Lo/Hi split; fall back to a single version 3.9 value. */
      *lo = *hi = battle_get_property (game, npc, base, 0);
      return;
    }

  snprintf (name, sizeof (name), "%sLo", base);
  *lo = battle_get_property (game, npc, name, 0);
  *hi = (high < *lo) ? *lo : high;
}


/*
 * battle_attribute_slot()
 *
 * Map a ranged combat attribute name to its mutable-state slot, or -1 for
 * a non-ranged attribute such as "Stamina".
 */
static scr_int
battle_attribute_slot (const scr_char *base)
{
  if (strcmp (base, "Strength") == 0)
    return BATTLE_STRENGTH;
  if (strcmp (base, "Accuracy") == 0)
    return BATTLE_ACCURACY;
  if (strcmp (base, "Defense") == 0)
    return BATTLE_DEFENSE;
  if (strcmp (base, "Agility") == 0)
    return BATTLE_AGILITY;
  return -1;
}


/*
 * battle_state()
 *
 * Return the mutable battle state of the player (npc < 0) or an NPC.
 */
static scr_battle_t *
battle_state (scr_gameref_t game, scr_int npc)
{
  return (npc < 0) ? gs_player_battle (game) : gs_npc_battle (game, npc);
}


/*
 * battle_attribute_range()
 * battle_attribute_max()
 *
 * Return respectively the current [lo,hi] roll range of a named attribute, and
 * its maximum cap, read from the mutable per-character battle state (seeded by
 * battle_start() and altered by type-7 task actions).  The four ranged combat
 * attributes are served from their slots; "Stamina" reports its mutable max.
 * Anything else falls back to the configured bundle value defensively.
 */
static void
battle_attribute_range (scr_gameref_t game, scr_int npc,
                        const scr_char *base, scr_int *lo, scr_int *hi)
{
  const scr_battle_t *battle = battle_state (game, npc);
  const scr_int slot = battle_attribute_slot (base);

  if (slot >= 0)
    {
      *lo = battle->lo[slot];
      *hi = battle->hi[slot];
      return;
    }
  battle_bundle_range (game, npc, base, lo, hi);
}

scr_int
battle_attribute_max (scr_gameref_t game, scr_int npc, const scr_char *base)
{
  const scr_battle_t *battle = battle_state (game, npc);
  const scr_int slot = battle_attribute_slot (base);
  scr_int lo, hi;

  if (slot >= 0)
    return battle->max[slot];
  if (strcmp (base, "Stamina") == 0)
    return battle->maxstamina;

  battle_bundle_range (game, npc, base, &lo, &hi);
  return hi;
}


/*
 * battle_get_stamina()
 * battle_set_stamina()
 *
 * Read and write the current stamina of the player (npc < 0) or an NPC.
 */
static scr_int
battle_get_stamina (scr_gameref_t game, scr_int npc)
{
  return (npc < 0) ? gs_playerstamina (game) : gs_npc_stamina (game, npc);
}

static void
battle_set_stamina (scr_gameref_t game, scr_int npc, scr_int stamina)
{
  if (npc < 0)
    gs_set_playerstamina (game, stamina);
  else
    gs_set_npc_stamina (game, npc, stamina);
}


/*
 * battle_attitude()
 * battle_speed_roll()
 *
 * Read an NPC's current attitude (0 = neutral, 1 = ally, 2 = enemy) from the
 * mutable battle state, and roll the number of turns until its next attack
 * from its current Speed setting (0 = every turn, 1 = most turns, 2/3/4 =
 * every 2nd/3rd/4th turn).
 */
static scr_int
battle_attitude (scr_gameref_t game, scr_int npc)
{
  return gs_npc_battle (game, npc)->attitude;
}

static scr_int
battle_speed_roll (scr_gameref_t game, scr_int npc)
{
  const scr_int speed = gs_npc_battle (game, npc)->speed;

  /*
   * "Most turns" is run400's `Int(Rnd * 2) + 1` (Proc_11_13) but run390's
   * `Int(Rnd * 1) + 1` (getnexthit 42C60A): a draw that can only be 1, so a
   * 3.9 Speed 1 NPC attacks every turn like Speed 0 -- yeh's vine re-arms
   * to 1 and takes Leon the turn after it takes the player.  Drawn either
   * way to keep the stream in step with the Runner.
   */
  switch (speed)
    {
    case 1:  return battle_legacy ? scr_randomint (1, 1)
                                  : scr_randomint (1, 2);   /* Most turns. */
    case 2:  return 2;                     /* Every second turn. */
    case 3:  return 3;                     /* Every third turn. */
    case 4:  return 4;                     /* Every fourth turn. */
    default: return 1;                     /* Every turn. */
    }
}


/* Forward declaration; defined with the combat resolution below. */
static void battle_kill (scr_gameref_t game, scr_int npc, scr_bool visible);


/*
 * battle_seed_attributes()
 *
 * Seed the mutable battle attributes of the player (npc < 0) or an NPC from
 * the configured bundle values: the [lo,hi] range and max cap of each ranged
 * attribute, max stamina, and (NPCs only) attitude and speed.  The configured
 * Hi doubles as the initial max cap.  Attitude and Speed are absent for the
 * player, so battle_get_property returns its 0 fallback there.
 */
static void
battle_seed_attributes (scr_gameref_t game, scr_int npc)
{
  static const scr_char *const names[BATTLE_ATTR_COUNT] = {
    "Strength", "Accuracy", "Defense", "Agility"
  };
  scr_battle_t *battle = battle_state (game, npc);
  scr_int slot, lo, hi;

  for (slot = 0; slot < BATTLE_ATTR_COUNT; slot++)
    {
      battle_bundle_range (game, npc, names[slot], &lo, &hi);
      battle->lo[slot] = lo;
      battle->hi[slot] = hi;
      battle->max[slot] = hi;
    }

  battle_bundle_range (game, npc, "Stamina", &lo, &hi);
  battle->maxstamina = hi;
  battle->attitude = battle_get_property (game, npc, "Attitude", 0);
  battle->speed = battle_get_property (game, npc, "Speed", 0);
  battle->seeded = TRUE;
}


/*
 * battle_all_ranges_degenerate()
 *
 * Return TRUE if every configured attribute range -- Stamina, Strength,
 * Accuracy, Defence and Agility, for the player and every NPC -- is degenerate
 * (Lo == Hi).  This is the fingerprint of a version 3.9 game mechanically
 * upgraded to the 4.0 file format by the ADRIFT 4 editor: the importer turns
 * each 3.9 scalar into a Lo == Hi pair, whereas a native 4.0 author wiring up
 * combat would normally leave at least one true Lo < Hi range.  Combined with
 * an absence of any Accuracy/Agility (see battle_start), it identifies a game
 * whose combat was authored for the 3.9 strength-vs-defence model.
 */
static scr_bool
battle_all_ranges_degenerate (scr_gameref_t game)
{
  static const scr_char *const names[] = {
    "Stamina", "Strength", "Accuracy", "Defense", "Agility"
  };
  scr_int n;
  size_t i;

  for (n = -1; n < gs_npc_count (game); n++)
    {
      for (i = 0; i < sizeof names / sizeof names[0]; i++)
        {
          scr_int lo, hi;

          battle_bundle_range (game, n, names[i], &lo, &hi);
          if (lo != hi)
            return FALSE;
        }
    }
  return TRUE;
}


/*
 * battle_preroll_player_stamina()
 * battle_preroll_npc_stamina()
 *
 * Runner-compatible RNG mode (SCR_RNG=xoshiro) only.  run400 rolls the
 * player's starting stamina while it reads the header (openadv 48F48E) and
 * each NPC's while it reads that NPC (4920B1), with the events' start rolls
 * in between; battle_start() runs after all of that.  These roll where the
 * Runner rolls, with its formula -- `Int(Rnd * (hi - lo)) + lo`, no guard on
 * an empty range -- and battle_start() then keeps the values instead of
 * rolling again.  run_runner_load_draws() is the only caller of all three.
 */
static scr_bool battle_prerolled = FALSE;

void
battle_preroll_player_stamina (scr_gameref_t game)
{
  scr_int lo, hi;

  if (!battle_is_enabled (game))
    return;

  battle_bundle_range (game, -1, "Stamina", &lo, &hi);
  gs_set_playerstamina (game, scr_randomint_exclusive (lo, hi));
  battle_prerolled = TRUE;
}

void
battle_preroll_npc_stamina (scr_gameref_t game)
{
  scr_int npc, lo, hi;

  if (!battle_is_enabled (game))
    return;

  /*
   * The Runner's NPC loader rolls each NPC's stamina (4920B1) and then, still
   * inside that NPC's iteration, its first attack counter (4921FE, Proc_11_13
   * -- a draw only for Speed 1), so the two interleave per NPC rather than
   * all stamina first.  Seed the mutable attributes here so the speed roll
   * can read the NPC's Speed; battle_start() re-seeds harmlessly.
   */
  GS_FOR_EACH_NPC (game, npc)
    {
      battle_seed_attributes (game, npc);
      battle_bundle_range (game, npc, "Stamina", &lo, &hi);
      gs_set_npc_stamina (game, npc, scr_randomint_exclusive (lo, hi));
      gs_set_npc_attackcounter (game, npc, battle_speed_roll (game, npc));
    }
  battle_prerolled = TRUE;
}


/*
 * battle_preroll_legacy()
 *
 * The same for a 3.9 (or 3.8) game, whose loader rolls nothing on the game
 * stream: 3.9 attributes are single values, so stamina is read, not rolled,
 * and the only load-time battle draw is getnexthit (run390 466A43) for each
 * NPC -- `Int(Rnd * 1) + 1` for Speed 1, a fixed count otherwise -- made on
 * the VB runtime's codec stream before `Randomize Timer` (see
 * taf_runtime_rnd).  Battle System games only (MemVar_46821A at 4669BF).
 */
void
battle_preroll_legacy (scr_gameref_t game)
{
  scr_int npc, lo, hi, speed;

  battle_legacy = TRUE;
  if (!battle_is_enabled (game))
    return;

  battle_bundle_range (game, -1, "Stamina", &lo, &hi);
  gs_set_playerstamina (game, lo);

  GS_FOR_EACH_NPC (game, npc)
    {
      battle_seed_attributes (game, npc);
      battle_bundle_range (game, npc, "Stamina", &lo, &hi);
      gs_set_npc_stamina (game, npc, lo);
      speed = gs_npc_battle (game, npc)->speed;
      if (speed == 1)
        scr_vb_rnd ();
      gs_set_npc_attackcounter (game, npc,
                                (speed >= 2 && speed <= 4) ? speed : 1);
    }
  battle_prerolled = TRUE;
}


/*
 * battle_start()
 *
 * Initialise battle state at game start.  Seed every mutable attribute from
 * the bundle, then roll a starting stamina within range for the player and for
 * every NPC (unless a preroll already did) and prime the recovery counters.
 * Each NPC's attack cadence counter is primed from its (now seeded) Speed
 * setting.  A no-op when the Battle System is disabled.
 */
void
battle_start (scr_gameref_t game)
{
  const scr_bool prerolled = battle_prerolled;
  scr_int npc, lo, hi;

  battle_prerolled = FALSE;
  if (!battle_is_enabled (game))
    return;

  /* Version 3.9/3.8 games use the legacy strength-vs-defence hit model. */
  battle_legacy = battle_is_legacy_version (game);

  /*
   * The recovery counter starts at Recovery, not at 0: run400's loader
   * stores the NPC's Recovery into the counter slot at 49222F and the
   * player's at 48F5DC, so the first restored point comes Recovery lines in,
   * where a zero seed would give it on the first line (battle_recover()
   * restores when the counter reads 0 and then reloads it).  3.9 files have
   * no Recovery property, so the seed stays 0 there and recovery never runs.
   */
  battle_seed_attributes (game, -1);
  battle_bundle_range (game, -1, "Stamina", &lo, &hi);
  if (!prerolled)
    gs_set_playerstamina (game, (hi > 0) ? scr_randomint (lo, hi) : 0);
  gs_set_playerstaminacounter (game,
                               battle_get_property (game, -1, "Recovery", 0));

  GS_FOR_EACH_NPC (game, npc)
    {
      battle_seed_attributes (game, npc);
      battle_bundle_range (game, npc, "Stamina", &lo, &hi);
      if (!prerolled)
        {
          gs_set_npc_stamina (game, npc,
                              (hi > 0) ? scr_randomint (lo, hi) : 0);
          gs_set_npc_attackcounter (game, npc, battle_speed_roll (game, npc));
        }
      gs_set_npc_staminacounter (game, npc,
                                 battle_get_property (game, npc,
                                                      "Recovery", 0));
    }

  /*
   * Detect "unconfigured combat" for the optional combat-assist mode: a
   * version 3.9 game upgraded to the 4.0 file format, whose combat was
   * authored for the 3.9 strength-vs-defence model.  Such a game has the
   * upgrade fingerprint -- no Accuracy or Agility configured on any
   * character AND every attribute range degenerate (see
   * battle_all_ranges_degenerate) -- so the 4.0 accuracy>agility hit test
   * (0 > 0) stalemates it.  Only such games get auto-hit; native 4.0 games
   * (which configure accuracy/agility, or use true Lo<Hi ranges) are left
   * untouched even with the assist on.  True 3.9/3.8-signature games are
   * handled unconditionally by battle_legacy and do not depend on this.
   */
  battle_unconfigured = FALSE;
  if (battle_combat_assist)
    {
      scr_bool any_accuracy = FALSE;
      scr_int n;

      for (n = -1; n < gs_npc_count (game) && !any_accuracy; n++)
        {
          if (battle_attribute_max (game, n, "Accuracy") > 0
              || battle_attribute_max (game, n, "Agility") > 0)
            any_accuracy = TRUE;
        }
      battle_unconfigured = !any_accuracy
                            && battle_all_ranges_degenerate (game);
    }
}


/*
 * battle_kill_drained()
 *
 * The death sequence shared by a damaging blow and a stamina change that
 * leaves its target at or below zero: store the drained stamina, then kill.
 * run390 keeps an NPC's stamina below zero, as its damage path does
 * (battle_apply_damage); 4.0 and the player floor it at zero.
 */
static void
battle_kill_drained (scr_gameref_t game, scr_int npc, scr_int stamina,
                     scr_bool visible)
{
  battle_set_stamina (game, npc, (npc >= 0 && battle_legacy) ? stamina : 0);
  battle_kill (game, npc, visible);
}


/*
 * battle_capped()
 *
 * The lesser of a raised attribute bound and its max cap.
 */
static scr_int
battle_capped (scr_int value, scr_int cap)
{
  return (value < cap) ? value : cap;
}


/*
 * battle_change_attribute()
 *
 * Apply a type-7 "Change battle attribute" task action to the player (npc < 0)
 * or an NPC.  attribute is the ADRIFT attribute index (0 = Attitude, 1 =
 * Stamina, 2 = Max Stamina, 3/5/7/9 = Strength/Accuracy/Defence/Agility, 4/6/8/
 * 0xA = their Max caps, 0xB = Speed).  Attitude and Speed are set to the given
 * enum value; every other attribute changes by the signed delta.  Current
 * stamina is re-clamped to its (possibly changed) maximum.  A range change
 * is capped at the attribute's max and a max change is a plain add, in both
 * Runners.  A 3.9 file's attribute numbering (5 = defence, 7 = speed) has
 * already been rewritten to this one by the parser
 * (parse_fixup_v390_battle_attribute).
 */
void
battle_change_attribute (scr_gameref_t game, scr_int npc,
                         scr_int attribute, scr_int value)
{
  scr_battle_t *battle = battle_state (game, npc);
  scr_int slot, stamina;

  switch (attribute)
    {
    case 0:                            /* Attitude (set, NPCs only). */
      /*
       * The value is written to the NPC's attitude byte as it stands, in
       * both Runners: run400 execute_action 48D747-48D78A and run390
       * 456BB1-456BF4 store CByte(var3) into record 172 / 108, the same
       * byte the loader fills from the .taf (0 = neutral, 1 = ally, 2 =
       * enemy, the encoding charhitwho's "3 - attitude" test reads).  Scarier
       * used to reorder it as if it were a Generator combo index (0 = ally,
       * 1 = neutral), which kept deaths' Joshua an ally through the `kill
       * ross` action that sets him to 0: the Runner then has one candidate
       * for Ross's and the demon's blows, and it is always the player
       * (runner_transcripts/deaths T42, T48, T49; 11 = 11 draws either way).
       */
      battle->attitude = value;
      break;

    case 1:                            /* Stamina (current, delta). */
      stamina = battle_get_stamina (game, npc) + value;
      if (stamina > battle->maxstamina)
        stamina = battle->maxstamina;
      /*
       * A change that leaves an NPC at or below zero kills it, in both
       * Runners: run390 execute_action 4573DC stores min(max, cur + delta)
       * and calls killchar (42D410) when the result is <= 0 (456CD7 for the
       * referenced character, 456D8D for NPC var2-2); run400 does the same
       * at 48D89B / 48D929 into Proc_11_3_44B13C, the death sub the damage
       * path uses.  The player dies of it only in 4.0 (48D803 ->
       * Proc_21_62_4524FC); run390's player arm (456C20) has no test.
       * cybercow_win T103: task 167 "#kill sane robot" takes 1000 off the
       * stamina-0 Robot (NPC 4), which run390 then removes from Bottom of
       * the Well -- "CyberCow is here.", not "CyberCow and Robot are here.".
       */
      if (stamina <= 0
          && (npc >= 0 || !battle_is_legacy_version (game)))
        {
          battle_kill_drained (game, npc, stamina, TRUE);
          break;
        }
      battle_set_stamina (game, npc, (stamina < 0) ? 0 : stamina);
      break;

    case 2:                            /* Max Stamina (delta). */
      battle->maxstamina += value;
      if (battle->maxstamina < 0)
        battle->maxstamina = 0;
      if (battle_get_stamina (game, npc) > battle->maxstamina)
        battle_set_stamina (game, npc, battle->maxstamina);
      break;

    case 3: case 5: case 7: case 9:    /* Str/Acc/Def/Agi range (delta). */
      slot = (attribute - 3) / 2;
      /*
       * run400 execute_action type 7 (48E08D for Defence, the same shape
       * for each ranged attribute) sets lo = Proc_21_1(lo + delta, max)
       * and hi = Proc_21_1(hi + delta, max), and Proc_21_1_442D5C is
       * plain min(): the raise is CAPPED at the attribute's max, with
       * no zero floor.  wes_ghn T76: Defence 10..20 (max 20) +15 is
       * 20..20 in the Runner, so Hope's 30-strength sword still cuts;
       * an uncapped 25..35 made it "doesn't seem to do any damage".
       *
       * run390 is the same rule: execute_action 4573DC writes strength =
       * Proc_2_1_427948(max, cur + delta) at 456EFA (player) / 456F64
       * (NPC), defence at 45714E / 4571B8, and Proc_2_1_427948 is min()
       * too (Scarier used to floor at zero and not cap at 3.9).  solw's
       * green potion (+20 strength, +20 defence, then +20 to both maxes)
       * therefore leaves the player at 30/30: the ghost's 50 lands with
       * damage ("A ghost hits you.", runner_transcripts/secret_of_lost_world
       * T125-T126, Kronos T164), where the uncapped 50 defence printed
       * ", but it doesn't seem to do any damage.".  spirits_flight's cake
       * (+3 defence, max 5) changes nothing, so the witch's 6 lands at T17;
       * Moyru's death lifts the max to 10 and the cheese's +3 then counts,
       * so the sorceress's 6 does no damage at T27 -- both as the Runner.
       */
      battle->lo[slot] = battle_capped (battle->lo[slot] + value,
                                        battle->max[slot]);
      battle->hi[slot] = battle_capped (battle->hi[slot] + value,
                                        battle->max[slot]);
      break;

    case 4: case 6: case 8: case 0xA:  /* Max Str/Acc/Def/Agi (delta). */
      slot = (attribute - 4) / 2;
      /* run400 48E2A6 and run390 457048 (max strength) / 4572A1 (max
         defence): a plain add, no floor and no re-clamp of lo/hi. */
      battle->max[slot] += value;
      break;

    case 0xB:                          /* Speed (set, NPCs only). */
      battle->speed = value;
      break;

    default:
      break;
    }
}


/*
 * Battle system combat resolution.
 *
 * The combat model is reverse-engineered from the ADRIFT 4 Runner.  Each time
 * an attribute is required it is freshly rolled within its [lo,hi] range as
 * lo + Int(rnd * (hi - lo)) -- note that the high bound is exclusive.  An
 * attack hits when the attacker's effective accuracy strictly exceeds the
 * target's effective agility; a hit does (effective strength - effective
 * defence) points of stamina damage, applied only when positive.  Strength
 * and accuracy gain the wielded weapon's bonuses (a "shoot" weapon replaces
 * base strength entirely); defence gains the protection of all worn armour.
 */

/* Target sentinels returned by battle_select_target(). */
enum { BATTLE_PLAYER = -1, BATTLE_NONE = -2 };


/*
 * battle_roll()
 *
 * Roll an attribute value within [lo, hi), matching the Runner's
 * lo + Int(rnd * (hi - lo)).  Degenerate ranges return their single value
 * without a draw -- except in Runner-compatible RNG mode, where the draw
 * happens anyway: run400's attribute getters (Battles.bas Proc_11_5/7/8/9)
 * consume Rnd unconditionally, so a fixed-attribute NPC (Lo == Hi through-
 * out, the common upgraded-3.9 shape) still advances the stream by two
 * words per attack and four per hit, and Scarier has to keep step.
 */
static scr_int
battle_roll (scr_int lo, scr_int hi)
{
  /*
   * run390 has no attribute getters and no draw at all in chardohit (442C7C):
   * hitstrength and armourstrength are the record's single values plus the
   * weapon's HitValue / worn ProtectionValue.  Its whole per-attack RNG cost
   * is charhitwho's target pick and getnexthit's re-arm.
   */
  if (battle_legacy)
    return lo;
  if (scr_is_runner_random ())
    return scr_randomint_exclusive (lo, hi);
  return (hi > lo) ? scr_randomint (lo, hi - 1) : lo;
}


/*
 * battle_object_battle()
 *
 * Read a named integer from an object's OBJ_BATTLE properties (weapon hit and
 * accuracy, armour protection, weapon method), or zero if absent.
 */
static scr_int
battle_object_battle (scr_gameref_t game, scr_int object, const scr_char *name)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4], vt_rvalue;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Battle";
  vt_key[3].string = name;
  if (prop_get (bundle, "I<-siss", &vt_rvalue, vt_key))
    return vt_rvalue.integer;
  return 0;
}


/*
 * battle_object_is_weapon()
 *
 * Return TRUE if the object is flagged as a weapon.
 */
static scr_bool
battle_object_is_weapon (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3], vt_rvalue;

  /* Static objects carry no Weapon property, so probe non-fatally. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Weapon";
  if (prop_get (bundle, "B<-sis", &vt_rvalue, vt_key))
    return vt_rvalue.boolean;
  return FALSE;
}


/*
 * battle_object_owned_by()
 * battle_object_worn_by()
 *
 * Tests for an object held/worn (owned) or specifically worn by the player
 * (npc < 0) or a given NPC.
 */
static scr_bool
battle_object_owned_by (scr_gameref_t game, scr_int object, scr_int npc)
{
  const scr_int position = gs_object_position (game, object);

  if (npc < 0)
    return position == OBJ_HELD_PLAYER || position == OBJ_WORN_PLAYER;
  return (position == OBJ_HELD_NPC || position == OBJ_WORN_NPC)
         && gs_object_parent (game, object) == npc;
}

static scr_bool
battle_object_worn_by (scr_gameref_t game, scr_int object, scr_int npc)
{
  const scr_int position = gs_object_position (game, object);

  if (npc < 0)
    return position == OBJ_WORN_PLAYER;
  return position == OBJ_WORN_NPC && gs_object_parent (game, object) == npc;
}


/*
 * battle_worn_protection()
 *
 * Sum the ProtectionValue of the armour worn by the player (npc < 0) or an
 * NPC.  An object adds its value only when it is > -1: the Generator stores
 * -1 on objects that are not armour, and both Runners skip it -- run390
 * armourstrength 42DB6C, run400 4502D4 (4501EC/4501FA player, 450294/4502AC
 * NPC).  matt wears a -1 object beside its Protection-5 armour, so its Giant
 * Rat (Strength 5) lands no damage (run390x, runner_transcripts/matt.txt,
 * `room a`).
 */
static scr_int
battle_worn_protection (scr_gameref_t game, scr_int npc)
{
  scr_int object, value = 0;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if (battle_object_worn_by (game, object, npc))
        {
          const scr_int protection
            = battle_object_battle (game, object, "ProtectionValue");

          if (protection > -1)
            value += protection;
        }
    }
  return value;
}


/*
 * battle_best_weapon()
 *
 * Return the object index of the best weapon wielded by the player (npc < 0)
 * or an NPC -- the owned weapon with the highest hit value -- or -1 for none.
 * Following the Runner, the player wields only carried (not worn) weapons.
 */
static scr_int
battle_best_weapon (scr_gameref_t game, scr_int npc)
{
  scr_int object, best = -1, best_hit = 0;

  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_int hit;

      if (!battle_object_is_weapon (game, object))
        continue;
      if (npc < 0)
        {
          if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
            continue;
        }
      else if (!battle_object_owned_by (game, object, npc))
        continue;

      hit = battle_object_battle (game, object, "HitValue");
      if (best == -1 || hit > best_hit)
        {
          best = object;
          best_hit = hit;
        }
    }
  return best;
}


/*
 * battle_eff_strength()
 * battle_eff_accuracy()
 * battle_eff_agility()
 * battle_eff_defence()
 *
 * Compute a fresh roll of each effective combat attribute for the player
 * (npc < 0) or an NPC, applying the bonuses of the supplied wielded weapon
 * (-1 for none) and any worn armour.
 */
static scr_int
battle_eff_strength (scr_gameref_t game, scr_int npc, scr_int weapon)
{
  scr_int lo, hi, value;

  battle_attribute_range (game, npc, "Strength", &lo, &hi);
  value = battle_roll (lo, hi);
  if (weapon >= 0)
    {
      /*
       * A "shoot" weapon (method 3) supplies all of the strength itself --
       * but only from version 4.0 on.  The 3.9 Runner adds HitValue to base
       * strength regardless of method: a Str-10 player with a HitValue-30
       * Method-3 blaster one-shots a 35-stamina enemy in run390, where
       * run400 needs two 30-damage hits
       * (test/adrift4/notes/RUNNER_TESTS_TODO.md, 2026-08-01).
       */
      if (!battle_legacy
          && battle_object_battle (game, weapon, "Method") == 3)
        value = 0;
      value += battle_object_battle (game, weapon, "HitValue");
    }
  return value;
}

static scr_int
battle_eff_accuracy (scr_gameref_t game, scr_int npc, scr_int weapon)
{
  scr_int lo, hi, value;

  battle_attribute_range (game, npc, "Accuracy", &lo, &hi);
  value = battle_roll (lo, hi);
  if (weapon >= 0)
    value += battle_object_battle (game, weapon, "Accuracy");
  return value;
}

static scr_int
battle_eff_agility (scr_gameref_t game, scr_int npc)
{
  scr_int lo, hi;

  battle_attribute_range (game, npc, "Agility", &lo, &hi);
  return battle_roll (lo, hi);
}

static scr_int
battle_eff_defence (scr_gameref_t game, scr_int npc)
{
  scr_int lo, hi;

  battle_attribute_range (game, npc, "Defense", &lo, &hi);
  return battle_roll (lo, hi) + battle_worn_protection (game, npc);
}


/*
 * How the battle narration names an NPC.  BATTLE_NAME_NAME is the plain
 * Name; the other two prefer "<Prefix> <Alias[0]>" to it, unconditionally or
 * only from an enemy.
 */
enum
{
  BATTLE_NAME_NAME = 0,
  BATTLE_NAME_ALIAS = 1,
  BATTLE_NAME_ENEMY_ALIAS = 2
};


/*
 * The grammatical form a combatant's name is printed in.  SUBJECT and
 * SUBJECT_CAPITALISED differ only for an NPC: the Runner puts some of its
 * leading names, and only some, through its one-line capitaliser.  See
 * battle_print_combatant().
 */
enum
{
  BATTLE_FORM_SUBJECT = 0,
  BATTLE_FORM_OBJECT = 1,
  BATTLE_FORM_POSSESSIVE = 2,
  BATTLE_FORM_SUBJECT_CAPITALISED = 3
};

/* Attack verbs by weapon method code, base (player) form; NPCs append "s". */
static const scr_char *const BATTLE_METHOD_VERBS[6]
    = {"chop", "cut", "hit", "shoot", "stab", "throw"};


/*
 * battle_npc_name()
 *
 * Build (into name) an NPC as a battle message names it, returning TRUE when
 * the game's ALRs quote that name in the Runner's raw shape.  The Runner's
 * two attack procedures do not use the NPC's Name: given a first alias they
 * narrate the fight with "<Prefix> <Alias[0]>" instead -- Orient Express
 * calls its enemy "Igotta Bigbottom" in the room listing but "the large man"
 * in every blow, and "Ivill Getyou" is "BIG BOSS" (measured against run400's
 * own transcript, runner_probes/orient_express.run400.txt, 2026-08-25).
 *
 * The two procedures differ in when they take the alias.  Proc_11_1, the
 * player's blow (Battles.bas @45E1CE), takes it from any NPC with one.
 * Proc_11_2, an NPC's blow (@464F20 for the attacker, @464FF2 for the
 * target), tests the combatant's Battle.Attitude -- the record byte at +172 --
 * and takes the alias only from an enemy (attitude 2); an ally or a neutral
 * keeps its Name.  Both join the prefix in raw, so an authored "the young "
 * prints its own second space.
 *
 * Nothing else follows the rule: the corpse line reads the Name field
 * directly (@44B115), and so does every room listing.
 *
 * 3.9 does the same thing, in the same two shapes -- the rule is NOT 4.0-only
 * and carries no version gate.  run390's player blow is Sub dohit(char,
 * weapon) @438B50: entry 43881C reads the NPC record, and 43882A..43886F is
 * literally `if Alias(0) <> "" then (Prefix <> "" ? Prefix & " " & Alias :
 * Alias) else Name`, with no attitude test -- the same as Proc_11_1.  An
 * NPC's blow is Sub chardohit(char1, char2) @442C7C, which builds the
 * attacker's name at 4423B4 and the target's at 442483 through the identical
 * ladder plus `And record(108) = 2` -- the 3.9 record's attitude byte, where
 * 4.0's sits at +172 -- so an ally or a neutral keeps its Name, the same as
 * Proc_11_2.  Measured 2026-09-08 against the whole-corpus run390 capture:
 * ALEXIS.TAF's NPC 4 is Name "Wolf", Prefix "a grey", Alias "wolf", and its
 * transcript reads "You hit a grey wolf with the magic cube.  A grey wolf
 * hits you." where scarier printed "Wolf" both times.  The pre-4.0 gate this
 * comment used to claim came from misreading run390 @4595DB, which is not a
 * battle site at all; 3.7 and 3.8 have no battle system whatsoever (the
 * string "doesn't seem to do any damage" is absent from both binaries), so
 * battle_legacy only ever meant 3.9 here.
 */
static scr_bool
battle_npc_name (scr_gameref_t game, scr_int npc, scr_int naming,
                 std::string &name)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *alias, *prefix;
  std::string runner;
  scr_vartype_t vt_key[4];

  alias = nullptr;
  if (!(naming == BATTLE_NAME_NAME
        || (naming == BATTLE_NAME_ENEMY_ALIAS
            && battle_attitude (game, npc) != 2)))
    {
      vt_key[0].string = "NPCs";
      vt_key[1].integer = npc;
      vt_key[2].string = "Alias";
      vt_key[3].integer = 0;
      alias = prop_get_child_count (bundle, "I<-sis", vt_key) > 0
              ? prop_get_string (bundle, "S<-sisi", vt_key) : nullptr;
    }
  if (!alias || alias[0] == '\0')
    {
      const scr_char *npc_name;

      npc_name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
      name = npc_name ? npc_name : "";
      return pf_alr_mentions (bundle, name.c_str ());
    }

  prefix = prop_get_indexed_string (bundle, "NPCs", npc, "Prefix");
  if (prefix && prefix[0] != '\0')
    runner = std::string (prefix) + " " + alias;
  else
    runner = alias;

  /*
   * Deliberate deviation: the Runner splices "<Prefix> <Alias>" raw, so an
   * authored "The serpentine" prints its capital mid-sentence ("You stab The
   * serpentine guardian") and an alias with no prefix loses its article ("You
   * stab hound of hades").  Scarier keeps the Runner's choice of alias but
   * gives it a grammatical shape: a leading article is lowercased here (the
   * caller capitalises a name that leads a sentence), and a bare lowercase
   * alias gets "the".  A game whose ALRs quote the Runner's shape (Les Feux
   * de l'enfer: "assassin manages " -> "L'assassin se debrouille") keeps it,
   * uncapitalised too, since the author's replacements are written to it.
   */
  if (pf_alr_mentions (bundle, runner.c_str ()))
    {
      name = runner;
      return TRUE;
    }
  if (prefix && prefix[0] != '\0')
    {
      name = runner;
      if (scr_compare_word (prefix, "the", 3)
          || scr_compare_word (prefix, "a", 1)
          || scr_compare_word (prefix, "an", 2))
        name[0] = scr_tolower (name[0]);
    }
  else if (alias[0] >= 'a' && alias[0] <= 'z'
           && !scr_compare_word (alias, "the", 3)
           && !scr_compare_word (alias, "a", 1)
           && !scr_compare_word (alias, "an", 2)
           && !scr_compare_word (alias, "some", 4))
    name = std::string ("the ") + alias;
  else
    name = alias;
  return FALSE;
}


/*
 * battle_print_combatant()
 *
 * Print the name of a combatant.  form selects the grammatical form: SUBJECT
 * and SUBJECT_CAPITALISED for a subject ("You" / "Goblin"), OBJECT for an
 * object/lowercase form ("you" / "Goblin"), POSSESSIVE for a possessive
 * ("your" / "Goblin's").  naming picks how an NPC is named; see
 * battle_npc_name().
 *
 * The player's forms follow Globals/Perspective, because the Runner splices
 * them from the same seven-element pronoun array the library uses -- filled by
 * perspective at run400 48F60C-48F798 (run390 464800-4648A8, two perspectives
 * only): Ary(0) is "I" / "You" / the player's name and Ary(2) "me" / "you" /
 * the player's name.  Battle reads exactly two of the seven slots, and it reads
 * them positionally, not grammatically: Ary(0) wherever the player leads the
 * sentence -- the whole of Proc_11_1, the player's blow, and the bare-handed
 * dodge in Proc_11_2 at loc_46515B -- and Ary(2) wherever the player sits
 * inside one, as the target of an NPC's blow (Proc_11_2 loc_464FDA feeding
 * var_8C, plus the armed miss's two direct reads at loc_4653BB/loc_4653FF).
 *
 * POSSESSIVE is the exception, and stays "your" in every perspective: the only
 * possessive the player has in battle is the one in Proc_11_1's own misses, and
 * the Runner writes it as part of the literal -- " manages to avoid your
 * attack." at loc_45E2EB and " manages to avoid your attack with " at
 * loc_45E519.  Neither touches the array, so a first-person game really does
 * read "The witch manages to avoid your attack." between two lines that say
 * "I".  The verb agreement is fixed in the same way, by which Runner branch is
 * printing rather than by perspective: Proc_11_2 picks its "manage"/"manages"
 * by comparing the target's rendered name against Ary(2) (loc_46514E), which a
 * third-person game satisfies with the player's name on both sides, so the
 * `target < 0` tests below are right for all three.
 *
 * SUBJECT_CAPITALISED forces the NPC's name to an initial capital, the way
 * the Runner's one-line capitaliser Proc_21_3_446BB4 does -- run400.bas
 * @84060, literally UCase(Left(s, 1)) & Right(s, Len(s) - 1), with an early
 * exit on the empty string.  It matters because a battle name is usually the
 * NPC's *alias*, and an alias is authored in the lowercase form it takes
 * mid-sentence: trabula.taf names its soldier "a soldier", so the blow that
 * opens a turn reads "A soldier attacks you with the rapier, but you manage
 * to avoid it." while the corpse line, printed from the Name field, reads
 * "Soldier falls down, dead." (measured, runner_transcripts/trabula.txt).
 *
 * The Runner capitalises at exactly five sites, all of them in Proc_11_2 (an
 * NPC's blow) and all of them the *attacker* leading the sentence: the two
 * bare-handed hits (Battles.bas loc_4650C6 landed, loc_46510D no damage), the
 * armed hit before the method verb is chosen (loc_4651FA, so a throw is
 * capitalised too), and both armed misses (loc_4653A3 against the player,
 * loc_46543F against another NPC).  Nothing else is: the bare-handed miss
 * leads with the raw target name (loc_465185 pushes var_8C unwrapped) and
 * names the attacker raw in the possessive after it, Proc_11_1 -- the player's
 * blow -- has no call to the capitaliser at all, and neither does the corpse
 * line (Proc_11_3 @44B115 reads the Name field directly).  Proc_11_1 needs no
 * capitaliser because it opens with Ary(0), which is already capitalised in the
 * first and second persons -- and in the third it is the player's name, which
 * the Runner splices exactly as authored.
 */
static void
battle_print_combatant (scr_gameref_t game, scr_int npc, scr_int form,
                        scr_int naming)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (npc < 0)
    {
      /* Deliberate deviation: the Runner's "your" is literal in every
         perspective ("I hit ... The witch manages to avoid your attack.");
         Scarier matches the possessive to the perspective. */
      if (form == BATTLE_FORM_POSSESSIVE)
        {
          const scr_int perspective = lib_get_perspective (game);
          pf_buffer_string (filter,
                            (perspective == LIB_FIRST_PERSON) ? "my"
                            : (perspective == LIB_THIRD_PERSON) ? "%player%'s"
                            : "your");
          return;
        }

      switch (lib_get_perspective (game))
        {
        case LIB_FIRST_PERSON:
          pf_buffer_string (filter,
                            (form == BATTLE_FORM_OBJECT) ? "me" : "I");
          break;

        case LIB_THIRD_PERSON:
          /* Ary(0) and Ary(2) are both the player's name here, so the two
             forms coincide; %player% is how the rest of the library carries
             it, and pf_flush() interpolates it the way it does anywhere. */
          pf_buffer_string (filter, "%player%");
          break;

        default:
          pf_buffer_string (filter,
                            (form == BATTLE_FORM_OBJECT) ? "you" : "You");
          break;
        }
      return;
    }

  /* Deliberate deviation: every SUBJECT site leads its sentence, so Scarier
     capitalises all of them, not only the Runner's five; the Runner's raw
     "a soldier manages to avoid your attack." is a display accident.  A name
     the game's ALRs quote keeps the Runner's shape; see battle_npc_name(). */
  std::string name;
  const scr_bool pinned = battle_npc_name (game, npc, naming, name);

  if (form == BATTLE_FORM_SUBJECT_CAPITALISED
      || (form == BATTLE_FORM_SUBJECT && !pinned))
    pf_new_sentence (filter);
  pf_buffer_string (filter, name.c_str ());
  if (form == BATTLE_FORM_POSSESSIVE)
    pf_buffer_string (filter, "'s");
}


/*
 * battle_npc_battle_task()
 *
 * Read an NPC's 1-based battle task reference (KilledTask or StaminaTask),
 * returning the 0-based task index or -1 if none is set.
 */
static scr_int
battle_npc_battle_task (scr_gameref_t game, scr_int npc, const scr_char *name)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4], vt_rvalue;

  if (npc < 0)
    return -1;
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Battle";
  vt_key[3].string = name;
  if (prop_get (bundle, "I<-siss", &vt_rvalue, vt_key)
      && gs_task_valid (game, vt_rvalue.integer - 1))
    return vt_rvalue.integer - 1;
  return -1;
}


/*
 * battle_kill()
 *
 * Handle a combatant reaching zero stamina.  The player's death ends the game
 * through Scarier's normal completion path, so the interpreter offers its
 * restart/restore prompt.  An NPC runs its KilledTask if set, otherwise a
 * default death message is shown; the NPC's held and worn objects are dropped
 * into the room it died in, and the NPC is then removed from play.
 */
static void
battle_kill (scr_gameref_t game, scr_int npc, scr_bool visible)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int task, room, object;

  if (npc < 0)
    {
      pf_buffer_character (filter, '\n');
      pf_buffer_string (filter, lib_get_death_message (game));
      pf_buffer_character (filter, '\n');
      /* The score summary follows, as it does after an EndGame death: run400
         Battles.Sub_12_1 calls the shared death sub General.Sub_22_70
         (4524FC) from the call site 44AE95; endmessage's EndGame-death
         branch calls that same sub from 45E10B, and run390 Form1.chardohit
         builds the same lines inline after the death sentence (442B7F).
         Measured live: run400 on light_up_4summer_comp.taf
         (runner_probes/light_up.run400.txt turn 352, "You scored 58 out of
         the maximum 0!"), and
         jason_vs_salm / wes_ghn / mr_smith. */
      task_print_end_game_summary (game, FALSE, TRUE);
      task_print_end_keyprompt (game);
      game->is_running = FALSE;
      game->has_completed = TRUE;
      return;
    }

  /* Note where the NPC dies before anything can relocate it. */
  room = gs_npc_location (game, npc) - 1;

  /*
   * Re-home the dead NPC's held and worn objects to that room, so its
   * inventory is not orphaned along with the hidden character.  The Runner
   * does this before the KilledTask runs (Battles.bas Proc_11_3, the
   * -200/-300 sweep ahead of the task dispatch), so a KilledTask restriction
   * can already see the corpse's effects lying in the room.
   */
  if (room >= 0)
    {
      GS_FOR_EACH_OBJECT (game, object)
        {
          const scr_int position = gs_object_position (game, object);

          if ((position == OBJ_HELD_NPC || position == OBJ_WORN_NPC)
              && gs_object_parent (game, object) == npc)
            gs_object_to_room (game, object, room);
        }
    }

  /*
   * A set KilledTask replaces the default death message outright -- verified
   * live in run400 2026-08-01 (probe pKT: "Player hit Robot.  KILLEDTASK
   * FIRED.", no "falls down, dead."), and the 3.9 Runner dispatches it the
   * same way (probe p39kt).  The default corpse line is 4.0-only: run390
   * prints NOTHING when a task-less NPC dies (probed live, and the string
   * " falls down, dead." does not exist anywhere in its binary).
   *
   * The dispatch goes through the Runner's general run-task routine
   * (Battles.bas Sub_12_4 -> mdlSpreadTheLoad.Sub_20_22), which refuses a
   * task the player is not currently eligible to run -- silently, with no
   * corpse line either, since the KilledTask is still set.  Verified live in
   * run400 2026-08-02 with Del Sol: killing the stamina-0 teacher Moreland in
   * Chemistry prints only the hit line, and her chem-dream-only KilledTask
   * (the game's win) never fires.  Same gate as the type-5 exec action.
   */
  task = battle_npc_battle_task (game, npc, "KilledTask");
  if (task >= 0)
    {
      /* A dispatch the player is not eligible for is dropped in silence --
         no task text and no corpse line either (probe KT2: a done
         non-repeatable KilledTask re-killed prints only the hit line). */
      if (task_can_run_task_directional (game, task, TRUE))
        run_task_run_by_index (game, task);
    }
  else if (visible && !battle_legacy)
    {
      pf_buffer_join_open (filter);
      battle_print_combatant (game, npc,
                              BATTLE_FORM_SUBJECT, BATTLE_NAME_NAME);
      pf_buffer_string (filter, " falls down, dead.");
      pf_buffer_answer_break (filter);
    }

  /*
   * Remove the dead NPC from play.  The Runner stamps -5 into the room field
   * (run400 Battles.bas @44B127) -- "not a room", like the 0 we use, but with
   * two readers of its own; see the `dead` flag in scgamest.h.  This runs
   * after the KilledTask, so a task that moved the corpse is overwritten at
   * both engines.
   */
  gs_set_npc_location (game, npc, 0);

  /*
   * Deliberate deviation (2026-09-30): an NPC whose KilledTask ran and gave
   * it stamina back is not dead for good, so its walk can bring it back as
   * the author meant.  The Town of Azra's KilledTasks (#banditkristdies,
   * #deerdies) hide the NPC and restore its stamina so that a walk
   * respawns it, and with the NPC dead for good (bc16dfe2d) the house goal
   * could no longer be reached.  An NPC with no KilledTask, or one left at
   * no stamina, stays dead.  The Runner side is measured, not inferred:
   * run390x through Azra's bandit kill and 70 waits brings no bandit back
   * and answers `where is the bandit` with "Bandit is dead!"
   * (runner_probes/the_town_of_azra_v390.run390.revive.txt, 2026-10-17);
   * Scarier on the same feed has him back after eight waits.
   */
  if (!(task >= 0 && gs_npc_stamina (game, npc) > 0))
    gs_set_npc_dead (game, npc, TRUE);
  else
    SCR_DEVIATION ("killed_npc_revivable", "npc=%ld task=%ld", npc, task);
}


/*
 * battle_apply_damage()
 *
 * Subtract stamina damage from a combatant, triggering death at zero or the
 * NPC's low-stamina task when dropping below 10% of maximum stamina.
 */
static void
battle_apply_damage (scr_gameref_t game, scr_int npc, scr_int damage,
                     scr_bool visible)
{
  scr_int stamina, maximum, task;

  stamina = battle_get_stamina (game, npc) - damage;

  /*
   * run390 keeps a killed NPC's stamina below zero: Outside's Joe, killed on
   * the road by the monster, shows "Stamina: -40 (150)" in 3.9 status
   * (runner_probes/outside.run390.statx.txt).  Nothing reads it but status
   * -- the corpse is out of play and 3.9 has no Recovery.
   */
  if (stamina <= 0)
    {
      battle_kill_drained (game, npc, stamina, visible);
      return;
    }

  battle_set_stamina (game, npc, stamina);

  /*
   * Run the NPC's low-stamina task when dropping below 10% of maximum.  The
   * Runner tests `stamina < max / 10` in floating point (Battles.bas
   * Proc_11_0, CDbl throughout) and re-runs the task on EVERY qualifying hit
   * -- verified live in run400 2026-08-01 (probe pKT: STAMINATASK FIRED on
   * both hits inside the window, default death line on the killing blow).
   * `stamina * 10 < maximum` is the integer-exact form of the float test;
   * plain integer `maximum / 10` truncates and misses the boundary when
   * maximum is not a multiple of 10 (e.g. max 95, stamina 9).
   */
  maximum = battle_attribute_max (game, npc, "Stamina");
  if (maximum > 0 && stamina * 10 < maximum)
    {
      /* StaminaTask goes through the same gated run-task routine as
         KilledTask (Battles.bas Sub_12_1 -> Sub_20_22, no gating at the call
         site) -- see the room-eligibility note in battle_kill. */
      task = battle_npc_battle_task (game, npc, "StaminaTask");
      if (task >= 0 && task_can_run_task_directional (game, task, TRUE))
        run_task_run_by_index (game, task);
    }
}


/*
 * battle_npc_pronoun()
 *
 * Return an NPC's subject pronoun from its Gender: "he", "she" or "it", or
 * "" for any other value, as the Runner's Proc_21_51_4496C8 maps it.
 */
static const scr_char *
battle_npc_pronoun (scr_gameref_t game, scr_int npc)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Gender";
  switch (prop_get_integer (bundle, "I<-sis", vt_key))
    {
    case NPC_MALE:   return "he";
    case NPC_FEMALE: return "she";
    case NPC_NEUTER: return "it";
    default:         return "";
    }
}


/*
 * battle_blow_join()
 *
 * Open an NPC's blow onto the turn's string, the way the Runner does.
 * chardohit -- the NPC's blow -- calls pspace() at the head of every one of
 * its four printing branches, so the sentence runs on after whatever the turn
 * has already said rather than starting a line of its own: run390 442C7C at
 * 4424E6 (bare-handed landed / no damage), 442610 (armed, both outcomes),
 * 4427EF and 442911 (the same two against the other class of target), and
 * run400 4654F8 at the matching 465072, 46513D, 4651EF and 46537F.  killchar
 * does it too, ahead of " falls down, dead." (run400 44B105).
 *
 * The player's own blow does NOT: dohit (run390 438B50, run400 45E578) writes
 * MemVar_4941B0 = MemVar_4941B0 & Ary(0) & " hit " & ... with no separator at
 * all, which is the same bare concatenation that glues one line's several
 * strikes together (see lib_battle_attack_many).  So only an NPC attacker
 * joins here.
 *
 * Measured across the archive: a player strike and the counter-blow it draws
 * share a line -- "You stab a bandit with the hunting sword.  A bandit hits
 * you." -- and so does anything the NPC tick prints after one
 * (runner_transcripts, sweep_wine_breaks.py).
 */
static void
battle_blow_join (scr_gameref_t game, scr_int attacker)
{
  if (attacker != BATTLE_PLAYER)
    pf_buffer_join_open (gs_get_filter (game));
}


/*
 * battle_print_hit()
 *
 * The visible narration of a landed blow, up to the damage clause;
 * bare is TRUE where "you"/"I" leads as the attacker.  See battle_resolve().
 */
static void
battle_print_hit (scr_gameref_t game, scr_int attacker, scr_int target,
                  scr_int weapon, scr_int method, scr_int naming,
                  scr_bool bare)
{
  const scr_filterref_t filter = gs_get_filter (game);

  battle_blow_join (game, attacker);
  battle_print_combatant (game, attacker,
                          BATTLE_FORM_SUBJECT_CAPITALISED, naming);
  if (method == 5)
    {
      pf_buffer_string (filter, bare ? " throw "
                                               : " throws ");
      lib_print_object_np (game, weapon);
      pf_buffer_string (filter, " at ");
      battle_print_combatant (game, target,
                              BATTLE_FORM_OBJECT, naming);
    }
  else if (method >= 0)
    {
      pf_buffer_character (filter, ' ');
      pf_buffer_string (filter, BATTLE_METHOD_VERBS[method]);
      if (!bare)
        pf_buffer_character (filter, 's');
      pf_buffer_character (filter, ' ');
      battle_print_combatant (game, target,
                              BATTLE_FORM_OBJECT, naming);
      pf_buffer_string (filter, " with ");
      lib_print_object_np (game, weapon);
    }
  else
    {
      pf_buffer_string (filter, bare ? " hit " : " hits ");
      battle_print_combatant (game, target,
                              BATTLE_FORM_OBJECT, naming);
    }
}


/*
 * battle_print_miss()
 *
 * The visible narration of a missed blow; bare is TRUE where "you"/"I"
 * leads as the target.  See battle_resolve().
 */
static void
battle_print_miss (scr_gameref_t game, scr_int attacker, scr_int target,
                   scr_int weapon, scr_int method, scr_int naming,
                   scr_bool bare)
{
  const scr_filterref_t filter = gs_get_filter (game);

  battle_blow_join (game, attacker);
  if (method < 0)
    {
      battle_print_combatant (game, target, BATTLE_FORM_SUBJECT, naming);
      pf_buffer_string (filter, bare ? " manage to avoid "
                                             : " manages to avoid ");
      battle_print_combatant (game, attacker,
                              BATTLE_FORM_POSSESSIVE, naming);
      pf_buffer_string (filter, " attack.");
      pf_buffer_answer_break (filter);
    }
  else if (attacker < 0)
    {
      battle_print_combatant (game, target, BATTLE_FORM_SUBJECT, naming);
      pf_buffer_string (filter, " manages to avoid ");
      battle_print_combatant (game, attacker,
                              BATTLE_FORM_POSSESSIVE, naming);
      pf_buffer_string (filter, " attack with ");
      lib_print_object_np (game, weapon);
      pf_buffer_character (filter, '.');
      pf_buffer_answer_break (filter);
    }
  else
    {
      battle_print_combatant (game, attacker,
                              BATTLE_FORM_SUBJECT_CAPITALISED, naming);
      pf_buffer_string (filter, " attacks ");
      battle_print_combatant (game, target, BATTLE_FORM_OBJECT, naming);
      pf_buffer_string (filter, " with ");
      lib_print_object_np (game, weapon);
      pf_buffer_string (filter, ", but ");
      if (target >= 0 && !battle_legacy)
        {
          /*
           * An NPC dodging another NPC's armed blow is named by pronoun,
           * not by name: run400 Proc_11_2 @465495 splices
           * Proc_21_51_4496C8(target, 0), which maps the record's Gender
           * byte to "he" / "she" / "it" (and anything else to "").
           * wes_ghn T81: "Hope attacks Charity Bell with the Stripper
           * Sword, but she manages to avoid it."  The player's own dodge
           * (@4653FF) still reads Ary(2).  3.9 unmeasured, left as it was.
           */
          const scr_char *pronoun = battle_npc_pronoun (game, target);

          if (pronoun[0] != '\0')
            pf_buffer_string (filter, pronoun);
        }
      else
        battle_print_combatant (game, target, BATTLE_FORM_OBJECT, naming);
      pf_buffer_string (filter, bare ? " manage to avoid it."
                                             : " manages to avoid it.");
      pf_buffer_answer_break (filter);
    }
}


/*
 * battle_resolve()
 *
 * Resolve a single attack from attacker against target with the given wielded
 * weapon (-1 for none).  Combat messages are printed only when visible, and
 * follow the Runner's narration (Battles.bas Proc_11_1/Proc_11_2): an armed
 * attack names the weapon with its method verb ("You shoot Robot with the
 * blaster.", "You throw the knife at Robot."), an armed miss is "<npc> manages
 * to avoid your attack with <weapon>." / "<npc> attacks you with <weapon>,
 * but you manage to avoid it.", and bare hands keep the plain "hit"/"avoid
 * <x>'s attack" forms.  A landed player throw also drops the weapon in the
 * room (see the comment in the hit branch below).
 */
static void
battle_resolve (scr_gameref_t game, scr_int attacker, scr_int target,
                scr_int weapon, scr_bool visible)
{
  const scr_filterref_t filter = gs_get_filter (game);
  /* The player's blow is Proc_11_1 (3.9: dohit) and an NPC's is Proc_11_2
     (3.9: chardohit), and they name their combatants by different rules --
     neither of them version-gated; see battle_npc_name(). */
  const scr_int naming = (attacker == BATTLE_PLAYER) ? BATTLE_NAME_ALIAS
                         : BATTLE_NAME_ENEMY_ALIAS;
  /* Deliberate deviation: the Runner splices the player's verbs bare after
     Ary(0), so a third-person game reads "Anonymous stab the goblin".
     Scarier conjugates them; `bare` is TRUE only where "you"/"I" leads. */
  const scr_bool third = (lib_get_perspective (game) == LIB_THIRD_PERSON);
  const scr_bool bare_attacker = (attacker < 0 && !third);
  const scr_bool bare_target = (target < 0 && !third);
  scr_int method, accuracy, agility;

  method = (weapon >= 0) ? battle_object_battle (game, weapon, "Method") : -1;
  if (method < 0 || method > 5)
    method = -1;

  /*
   * Every armed player attack persists the weapon as the wield, before the
   * hit test -- so a miss persists it too (Battles.bas Proc_11_1 sets
   * global_78 on entry).  A landed throw clears it again below.
   */
  if (attacker == BATTLE_PLAYER && weapon >= 0)
    gs_set_playerwield (game, weapon);

  /*
   * The Runner rolls the attacker's accuracy before the target's agility
   * (Proc_11_7 is pushed as Proc_11_8's argument), and on a hit the
   * attacker's strength before the target's defence; C leaves the order of
   * `>` and `-` operands unspecified, so sequence the rolls explicitly.
   */
  if (battle_unconfigured || battle_legacy)
    accuracy = agility = 0;
  else
    {
      accuracy = battle_eff_accuracy (game, attacker, weapon);
      agility = battle_eff_agility (game, target);
    }
  if (battle_unconfigured || battle_legacy || accuracy > agility)
    {
      /*
       * A landed player throw (method 5) leaves the weapon behind, in BOTH
       * Runners (settled live 2026-08-01, probes pTD/p39td): it lands in the
       * player's room and the wielded ref is cleared.  run400 clears the ref
       * *before* the damage roll (Battles.bas loc_45E457), so a 4.0 throw
       * deals base strength only -- the weapon's HitValue never contributes.
       * run390 one-shots the same probe: 3.9 adds HitValue regardless of
       * method (its usual rule), so only the 4.0 half excludes the weapon
       * from the strength roll.  The weapon's Accuracy still applies to the
       * hit test above, and a *missed* throw keeps the weapon (decompile:
       * the move is inside the hit branch only).  NPC throws neither drop
       * nor lose HitValue (Proc_11_2 has no equivalent of either).
       */
      const scr_bool player_throw = (method == 5 && attacker < 0);
      const scr_int hit_weapon
        = (player_throw && !battle_legacy) ? -1 : weapon;
      const scr_int strength = battle_eff_strength (game, attacker,
                                                    hit_weapon);
      const scr_int defence = battle_eff_defence (game, target);
      const scr_int damage = strength - defence;

      if (battle_trace)
        fprintf (stderr, "BATTLE: %ld hits %ld weapon %ld: accuracy %ld"
                 " agility %ld strength %ld defence %ld\n", attacker, target,
                 weapon, accuracy, agility, strength, defence);

      if (visible)
        battle_print_hit (game, attacker, target, weapon, method, naming,
                          bare_attacker);
      if (player_throw)
        {
          gs_object_to_room (game, weapon, gs_playerroom (game));
          gs_set_playerwield (game, -1);
        }
      if (damage > 0)
        {
          if (visible)
            {
              pf_buffer_character (filter, '.');
              pf_buffer_answer_break (filter);
            }
          battle_apply_damage (game, target, damage, visible);
        }
      else if (visible)
        {
          pf_buffer_string (filter,
                            ", but it doesn't seem to do any damage.");
          pf_buffer_answer_break (filter);
        }
    }
  else if (visible)
    battle_print_miss (game, attacker, target, weapon, method, naming,
                       bare_target);
}


/*
 * battle_is_foe()
 * battle_select_target()
 *
 * Choose a target for an attacking NPC, following the Runner: neutrals never
 * attack; allies target enemies and vice versa (their attitudes summing to
 * three, which is battle_is_foe), and enemies also target the player.  A
 * uniformly random choice is made among the candidates sharing the NPC's
 * room.  Returns an NPC index, BATTLE_PLAYER, or BATTLE_NONE.
 *
 * run400 Battles.bas Proc_11_14 (451DF0) tests NOTHING about a candidate's
 * stamina: the count loop (451CA6-451D31) takes every other NPC in the same
 * room whose attitude is 3 - own, the player is added (451D31-451D48) when
 * the attacker is an enemy standing in the player's room, one draw picks
 * (451D48) and the second loop (451D65-451DDC) re-walks the same order.  A
 * character at 0 stamina is therefore a legitimate target.  Killed NPCs are
 * out of every room (room -5 there, location 0 here), so this only reaches
 * characters that ROLLED 0 at load -- an NPC whose Stamina range starts at 0
 * (Shadowpeak's Holga, 0..50, before recovery brings her back), or the player
 * of a game whose player Stamina is 0..0.  The Runner then plays the blow
 * out: chardohit -> Proc_11_0, damage >= stamina -> death (Proc_21_62 for the
 * player, "falls down, dead." and the KilledTask for an NPC), so a 0-stamina
 * player dies to the first hostile in the room, and an ally rolled at 0 is
 * killed by the first enemy that picks it.  Scarier used to skip 0-stamina
 * candidates in both loops, which made those characters unhittable and
 * changed the draw cadence (no pick draw when they were the only candidate).
 */
static scr_bool
battle_is_foe (scr_gameref_t game, scr_int npc, scr_int other)
{
  return other != npc
         && gs_npc_location (game, other) == gs_npc_location (game, npc)
         && battle_attitude (game, other) == 3 - battle_attitude (game, npc);
}

static scr_int
battle_select_target (scr_gameref_t game, scr_int npc)
{
  const scr_int attitude = battle_attitude (game, npc);
  const scr_int location = gs_npc_location (game, npc);
  scr_int other, count, pick;

  if (attitude == 0 || location <= 0)
    return BATTLE_NONE;

  /* Count candidate foes co-located with the NPC, plus the player. */
  count = 0;
  GS_FOR_EACH_NPC (game, other)
    {
      if (battle_is_foe (game, npc, other))
        count++;
    }
  if (attitude == 2 && location - 1 == gs_playerroom (game))
    count++;

  if (count == 0)
    return BATTLE_NONE;

  /* Pick one candidate at random, re-walking the same candidate order. */
  pick = scr_randomint (1, count);
  GS_FOR_EACH_NPC (game, other)
    {
      if (battle_is_foe (game, npc, other) && --pick == 0)
        return other;
    }
  return BATTLE_PLAYER;
}


/*
 * battle_recover()
 *
 * Apply automatic stamina recovery for the player (npc < 0) or an NPC: every
 * Recovery lines, restore one point of stamina up to the maximum.  run400
 * Battles.bas 47E682-47E764: a Recovery of 0 opts out; a counter at 0 is
 * reloaded with Recovery and the point restored; the counter then counts
 * down.  There is NO alive test -- a character at 0 stamina recovers like
 * any other, which is how Shadowpeak's Holga (rolled 0 of 0..50, Recovery
 * 10) comes to attack the player at all.
 */
static void
battle_recover (scr_gameref_t game, scr_int npc)
{
  scr_int recovery, counter, stamina, maximum;

  recovery = battle_get_property (game, npc, "Recovery", 0);
  if (recovery <= 0)
    return;

  counter = (npc < 0) ? gs_playerstaminacounter (game)
                      : gs_npc_staminacounter (game, npc);
  stamina = battle_get_stamina (game, npc);
  maximum = battle_attribute_max (game, npc, "Stamina");

  if (counter == 0)
    {
      counter = recovery;
      if (stamina < maximum)
        {
          stamina++;
          battle_set_stamina (game, npc, stamina);
          if (battle_trace)
            fprintf (stderr, "BATTLE: %s %ld recovers to stamina %ld of %ld\n",
                     (npc < 0) ? "player" : "npc", npc, stamina, maximum);
        }
    }
  counter--;

  if (npc < 0)
    gs_set_playerstaminacounter (game, counter);
  else
    gs_set_npc_staminacounter (game, npc, counter);
}


/*
 * battle_is_weapon()
 * battle_weapon_method()
 *
 * Public predicates for the command layer.  battle_is_weapon reports whether an
 * object is flagged as a weapon; battle_weapon_method returns a weapon's attack
 * method code (0 chop, 1 cut, 2 hit, 3 shoot, 4 stab, 5 throw), or -1 when the
 * object is not a weapon.
 */
scr_bool
battle_is_weapon (scr_gameref_t game, scr_int object)
{
  return object >= 0 && battle_object_is_weapon (game, object);
}

scr_int
battle_weapon_method (scr_gameref_t game, scr_int object)
{
  if (!battle_is_weapon (game, object))
    return -1;
  return battle_object_battle (game, object, "Method");
}


/*
 * battle_player_wielded_weapon()
 *
 * Return the weapon the player has wielded, or -1 for none.  The wield is a
 * persistent reference, not a per-attack default (Runner: Battles.bas
 * global_78, settled live 2026-08-01): "wield", "attack ... with" and every
 * other armed blow set it, and it is cleared -- never replaced by another
 * carried weapon -- when the weapon leaves the player's hands.  The held and
 * is-a-weapon checks are belt-and-braces against a stale reference.
 */
scr_int
battle_player_wielded_weapon (scr_gameref_t game)
{
  const scr_int wielded = gs_playerwield (game);

  if (wielded >= 0
      && gs_object_position (game, wielded) == OBJ_HELD_PLAYER
      && battle_object_is_weapon (game, wielded))
    return wielded;
  return -1;
}


/*
 * battle_player_weapon_count()
 * battle_player_best_weapon()
 *
 * Count the weapons the player is carrying, and return the carried weapon
 * with the highest hit value (-1 for none).  With no wield set, a bare attack
 * auto-selects a solitary carried weapon, but asks rather than pick among two
 * or more (Battles.bas attack handler, Proc_11_10/Proc_11_12).
 */
scr_int
battle_player_weapon_count (scr_gameref_t game)
{
  scr_int object, count = 0;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if (battle_object_is_weapon (game, object)
          && gs_object_position (game, object) == OBJ_HELD_PLAYER)
        count++;
    }
  return count;
}

scr_int
battle_player_best_weapon (scr_gameref_t game)
{
  return battle_best_weapon (game, -1);
}


/*
 * battle_combatant_weapon()
 *
 * Return the weapon a combatant fights with: the player's wielded weapon
 * (npc < 0) or an NPC's best carried weapon (-1 for bare hands).
 */
scr_int
battle_combatant_weapon (scr_gameref_t game, scr_int npc)
{
  return (npc < 0) ? battle_player_wielded_weapon (game)
                   : battle_best_weapon (game, npc);
}


/*
 * battle_attribute_report()
 *
 * Status-command helper.  For a named attribute of the player (npc < 0) or an
 * NPC, report the configured [lo, hi] range plus a fresh effective roll that
 * includes the combatant's wielded-weapon and worn-armour bonuses, exactly as
 * the Runner's status display does.
 */
void
battle_attribute_report (scr_gameref_t game, scr_int npc, const scr_char *base,
                         scr_int *lo, scr_int *hi, scr_int *current)
{
  scr_int weapon = battle_combatant_weapon (game, npc);

  /*
   * run390's hitstrength (42B7EC) adds the best held weapon's HitValue for
   * the player whether or not it is wielded, so 3.9 status counts a carried
   * weapon before any attack sets the wield.  Outside, holding the shovel
   * (HitValue 50) on arrival in the tunnel: "Hit strength: 100 (50)" in
   * run390x (runner_probes/outside.run390.statx.txt), where the wield alone
   * gave 50.
   */
  if (npc < 0 && weapon < 0 && battle_is_legacy_version (game))
    weapon = battle_player_best_weapon (game);

  battle_attribute_range (game, npc, base, lo, hi);

  if (strcmp (base, "Strength") == 0)
    *current = battle_eff_strength (game, npc, weapon);
  else if (strcmp (base, "Accuracy") == 0)
    *current = battle_eff_accuracy (game, npc, weapon);
  else if (strcmp (base, "Defense") == 0)
    *current = battle_eff_defence (game, npc);
  else if (strcmp (base, "Agility") == 0)
    *current = battle_eff_agility (game, npc);
  else
    *current = battle_roll (*lo, *hi);
}


/*
 * battle_attribute_bonus()
 *
 * The equipment share of an effective attribute, for the status table's
 * parenthesized column: the wielded weapon's HitValue for strength and its
 * Accuracy for accuracy, the worn armour's summed ProtectionValue for
 * defence.  Agility and stamina take no equipment bonus (the Runner's status
 * prints no parenthesis on those rows).
 */
scr_int
battle_attribute_bonus (scr_gameref_t game, scr_int npc, const scr_char *base)
{
  const scr_int weapon = battle_combatant_weapon (game, npc);

  if (strcmp (base, "Strength") == 0)
    return (weapon >= 0) ? battle_object_battle (game, weapon, "HitValue") : 0;
  if (strcmp (base, "Accuracy") == 0)
    return (weapon >= 0) ? battle_object_battle (game, weapon, "Accuracy") : 0;
  if (strcmp (base, "Defense") == 0)
    return battle_worn_protection (game, npc);
  return 0;
}


/*
 * battle_player_attack()
 *
 * Resolve a player-initiated attack on an NPC.  When weapon is -1 the wielded
 * weapon, if any, is used (bare hands otherwise) -- the bare-attack auto-select
 * and the ask-with-two-carried-weapons cases live in the command layer.
 */
void
battle_player_attack (scr_gameref_t game, scr_int npc, scr_int weapon)
{
  if (weapon < 0)
    weapon = battle_player_wielded_weapon (game);
  battle_resolve (game, BATTLE_PLAYER, npc, weapon, TRUE);
}


/*
 * battle_tick_npc()
 *
 * One NPC's battle turn, the Runner's Proc_11_15: count its attack counter
 * down and, when it reaches zero, select a target and strike, then re-arm
 * the counter from its Speed.  npc_tick_npcs() calls this right after the
 * NPC's own walk tick, because that is where run400 calls it (468D79, at the
 * end of every iteration of the walk loop Proc_19_1) -- so NPC 2's attack
 * prints before NPC 3's walk announcement, and its draws come between the
 * two walks.  Only stamina gates the call (468D61); a neutral NPC still
 * counts down and re-arms (its target select is what yields nothing), so
 * its cadence is live the moment a task turns it hostile, and a Speed-1
 * neutral keeps drawing.  A no-op when the Battle System is disabled.
 */
void
battle_tick_npc (scr_gameref_t game, scr_int npc)
{
  scr_int counter;

  if (!battle_is_enabled (game) || gs_npc_stamina (game, npc) <= 0)
    return;

  counter = gs_npc_attackcounter (game, npc) - 1;
  if (battle_trace)
    fprintf (stderr, "BATTLE: npc %ld counter %ld attitude %ld speed %ld"
             " stamina %ld room %ld\n", npc, counter,
             battle_attitude (game, npc), gs_npc_battle (game, npc)->speed,
             gs_npc_stamina (game, npc), gs_npc_location (game, npc));
  if (counter <= 0)
    {
      scr_int target = battle_select_target (game, npc);

      if (battle_trace)
        fprintf (stderr, "BATTLE: npc %ld target %ld\n", npc, target);
      if (target != BATTLE_NONE)
        {
          const scr_bool visible
            = target == BATTLE_PLAYER
              || gs_npc_location (game, npc) - 1 == gs_playerroom (game);

          battle_resolve (game, npc, target,
                          battle_best_weapon (game, npc), visible);
        }
      counter = battle_speed_roll (game, npc);
    }
  gs_set_npc_attackcounter (game, npc, counter);
}


/*
 * battle_recover_line()
 *
 * Stamina recovery for every NPC in index order and then the player, run400
 * Battles.bas 47E682-47E764.  This is the head of dobattle (Proc_11_4,
 * 47F084), which generaltasks calls once per line element at 48A4A2 when the
 * Battle System is on, so it is NOT an end-of-turn tick: it runs before the
 * library verbs, on lines that are not turns as well (`score`, gibberish),
 * and not at all on a line the inventory listing, the put/drop rows, the
 * get rows or the task dispatcher claimed (each of those exits generaltasks
 * to loc_48B4E3, past the call).  Skipped when the line holds the whole
 * word "status" with nothing yet in the message buffer, which is the status
 * path at 47DCA1-47DCB9 setting the not-a-turn byte MemVar_494281 that the
 * loop at 47E682 tests.  The caller, run_all_commands(), holds those gates.
 */
void
battle_recover_line (scr_gameref_t game)
{
  scr_int npc;

  if (!battle_is_enabled (game))
    return;

  GS_FOR_EACH_NPC (game, npc)
    battle_recover (game, npc);
  battle_recover (game, -1);
}
