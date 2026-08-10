/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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
 * o ADRIFT's optional Battle System, split out of scnpcs.c.  The public
 *   entry points are declared in scprotos.h and the per-character mutable
 *   state (scr_battle_s) lives in scgamest.h.
 */

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "scarier.h"
#include "scprotos.h"
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
 * SCARIER otherwise implements the 4.0 model, whose hit test is accuracy >
 * agility.  A 3.9 game has no Accuracy/Agility properties, so both read as 0
 * and the test 0 > 0 never passes, silently making all combat an endless
 * stalemate.  When battle_legacy is set (detected from the game version at
 * battle_start), battle_resolve skips the accuracy/agility test and always
 * connects, letting the existing Strength - Defence damage path -- which already
 * matches the 3.9 formula and message -- decide the outcome.
 */
static scr_bool battle_legacy = FALSE;

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
  scr_vartype_t vt_key[2];

  vt_key[0].string = "Globals";
  vt_key[1].string = "BattleSystem";
  return prop_get_boolean (bundle, "B<-ss", vt_key);
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
  const scr_battle_t *battle = (npc < 0) ? gs_player_battle (game)
                                        : gs_npc_battle (game, npc);
  const scr_int slot = battle_attribute_slot (base);

  if (slot >= 0)
    {
      *lo = battle->lo[slot];
      *hi = battle->hi[slot];
      return;
    }
  battle_bundle_range (game, npc, base, lo, hi);
}

/*
 * battle_attribute()
 *
 * Return a fresh random roll of an attribute within its current range.
 */
scr_int
battle_attribute (scr_gameref_t game, scr_int npc, const scr_char *base)
{
  scr_int lo, hi;

  battle_attribute_range (game, npc, base, &lo, &hi);
  return scr_randomint (lo, hi);
}

scr_int
battle_attribute_max (scr_gameref_t game, scr_int npc, const scr_char *base)
{
  const scr_battle_t *battle = (npc < 0) ? gs_player_battle (game)
                                        : gs_npc_battle (game, npc);
  const scr_int slot = battle_attribute_slot (base);
  scr_int lo, hi;

  if (slot >= 0)
    return battle->max[slot];
  if (strcmp (base, "Stamina") == 0)
    return battle->maxstamina;

  battle_bundle_range (game, npc, base, &lo, &hi);
  return hi;
}


/* Forward declaration; defined with the combat helpers below. */
static scr_int battle_speed_roll (scr_gameref_t game, scr_int npc);

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
  scr_battle_t *battle = (npc < 0) ? gs_player_battle (game)
                                  : gs_npc_battle (game, npc);
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
 * battle_start()
 *
 * Initialise battle state at game start.  Seed every mutable attribute from
 * the bundle, then roll a starting stamina within range for the player and for
 * every NPC and zero the recovery counters.  Each NPC's attack cadence counter
 * is primed from its (now seeded) Speed setting.  A no-op when the Battle
 * System is disabled.
 */
void
battle_start (scr_gameref_t game)
{
  scr_int npc, lo, hi;

  if (!battle_is_enabled (game))
    return;

  /* Version 3.9/3.8 games use the legacy strength-vs-defence hit model. */
  battle_legacy = battle_is_legacy_version (game);

  battle_seed_attributes (game, -1);
  battle_bundle_range (game, -1, "Stamina", &lo, &hi);
  gs_set_playerstamina (game, (hi > 0) ? scr_randomint (lo, hi) : 0);
  gs_set_playerstaminacounter (game, 0);

  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      battle_seed_attributes (game, npc);
      battle_bundle_range (game, npc, "Stamina", &lo, &hi);
      gs_set_npc_stamina (game, npc, (hi > 0) ? scr_randomint (lo, hi) : 0);
      gs_set_npc_staminacounter (game, npc, 0);
      gs_set_npc_attackcounter (game, npc, battle_speed_roll (game, npc));
    }

  /*
   * Detect "unconfigured combat" for the optional combat-assist mode: a version
   * 3.9 game upgraded to the 4.0 file format, whose combat was authored for the
   * 3.9 strength-vs-defence model.  Such a game has the upgrade fingerprint --
   * no Accuracy or Agility configured on any character AND every attribute range
   * degenerate (see battle_all_ranges_degenerate) -- so the 4.0 accuracy>agility
   * hit test (0 > 0) stalemates it.  Only such games get auto-hit; native 4.0
   * games (which configure accuracy/agility, or use true Lo<Hi ranges) are left
   * untouched even with the assist on.  True 3.9/3.8-signature games are handled
   * unconditionally by battle_legacy and do not depend on this.
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
      battle_unconfigured = !any_accuracy && battle_all_ranges_degenerate (game);
    }
}


/*
 * battle_attitude_from_ui()
 *
 * Remap the "Change Attitude" task-action enum (the editor's combo order,
 * 0 = Ally, 1 = Neutral, 2 = Enemy) to the internal attitude encoding used by
 * the combat code and the bundle (0 = Neutral, 1 = Ally, 2 = Enemy).
 */
static scr_int
battle_attitude_from_ui (scr_int value)
{
  switch (value)
    {
    case 0:  return 1;       /* Ally. */
    case 1:  return 0;       /* Neutral. */
    default: return 2;       /* Enemy. */
    }
}

/*
 * battle_change_attribute()
 *
 * Apply a type-7 "Change battle attribute" task action to the player (npc < 0)
 * or an NPC.  attribute is the ADRIFT attribute index (0 = Attitude, 1 =
 * Stamina, 2 = Max Stamina, 3/5/7/9 = Strength/Accuracy/Defence/Agility, 4/6/8/
 * 0xA = their Max caps, 0xB = Speed).  Attitude and Speed are set to the given
 * enum value; every other attribute changes by the signed delta.  Values are
 * floored at zero, and current stamina is re-clamped to its (possibly changed)
 * maximum.
 */
void
battle_change_attribute (scr_gameref_t game, scr_int npc,
                         scr_int attribute, scr_int value)
{
  scr_battle_t *battle = (npc < 0) ? gs_player_battle (game)
                                  : gs_npc_battle (game, npc);
  scr_int slot, stamina;

  switch (attribute)
    {
    case 0:                            /* Attitude (set, NPCs only). */
      battle->attitude = battle_attitude_from_ui (value);
      break;

    case 1:                            /* Stamina (current, delta). */
      stamina = (npc < 0) ? gs_playerstamina (game)
                          : gs_npc_stamina (game, npc);
      stamina += value;
      if (stamina < 0)
        stamina = 0;
      if (stamina > battle->maxstamina)
        stamina = battle->maxstamina;
      if (npc < 0)
        gs_set_playerstamina (game, stamina);
      else
        gs_set_npc_stamina (game, npc, stamina);
      break;

    case 2:                            /* Max Stamina (delta). */
      battle->maxstamina += value;
      if (battle->maxstamina < 0)
        battle->maxstamina = 0;
      stamina = (npc < 0) ? gs_playerstamina (game)
                          : gs_npc_stamina (game, npc);
      if (stamina > battle->maxstamina)
        {
          if (npc < 0)
            gs_set_playerstamina (game, battle->maxstamina);
          else
            gs_set_npc_stamina (game, npc, battle->maxstamina);
        }
      break;

    case 3: case 5: case 7: case 9:    /* Str/Acc/Def/Agi range (delta). */
      slot = (attribute - 3) / 2;
      battle->lo[slot] += value;
      if (battle->lo[slot] < 0)
        battle->lo[slot] = 0;
      battle->hi[slot] += value;
      if (battle->hi[slot] < 0)
        battle->hi[slot] = 0;
      break;

    case 4: case 6: case 8: case 0xA:  /* Max Str/Acc/Def/Agi (delta). */
      slot = (attribute - 4) / 2;
      battle->max[slot] += value;
      if (battle->max[slot] < 0)
        battle->max[slot] = 0;
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
 * lo + Int(rnd * (hi - lo)).  Degenerate ranges return their single value.
 */
static scr_int
battle_roll (scr_int lo, scr_int hi)
{
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

  for (object = 0; object < gs_object_count (game); object++)
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
      /* A "shoot" weapon (method 3) supplies all of the strength itself. */
      if (battle_object_battle (game, weapon, "Method") == 3)
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
  scr_int lo, hi, value, object;

  battle_attribute_range (game, npc, "Defense", &lo, &hi);
  value = battle_roll (lo, hi);
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (battle_object_worn_by (game, object, npc))
        value += battle_object_battle (game, object, "ProtectionValue");
    }
  return value;
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

  switch (speed)
    {
    case 1:  return scr_randomint (1, 2);   /* Most turns. */
    case 2:  return 2;                     /* Every second turn. */
    case 3:  return 3;                     /* Every third turn. */
    case 4:  return 4;                     /* Every fourth turn. */
    default: return 1;                     /* Every turn. */
    }
}

/*
 * battle_print_combatant()
 *
 * Print the name of a combatant.  form selects the grammatical form: 0 for a
 * capitalised subject ("You" / "Goblin"), 1 for an object/lowercase form
 * ("you" / "Goblin"), 2 for a possessive ("your" / "Goblin's").
 */
static void
battle_print_combatant (scr_gameref_t game, scr_int npc, scr_int form)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *name;

  if (npc < 0)
    {
      pf_buffer_string (filter, (form == 0) ? "You"
                                : (form == 1) ? "you" : "your");
      return;
    }

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Name";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  pf_buffer_string (filter, name);
  if (form == 2)
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
  if (prop_get (bundle, "I<-siss", &vt_rvalue, vt_key))
    return vt_rvalue.integer - 1;
  return -1;
}

/*
 * battle_kill()
 *
 * Handle a combatant reaching zero stamina.  The player's death ends the game
 * through SCARIER's normal completion path, so the interpreter offers its
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
      pf_buffer_string (filter, "\nI'm afraid you are dead!\n");
      game->is_running = FALSE;
      game->has_completed = TRUE;
      return;
    }

  /* Note where the NPC dies before anything can relocate it. */
  room = gs_npc_location (game, npc) - 1;

  task = battle_npc_battle_task (game, npc, "KilledTask");
  if (task >= 0)
    task_run_task (game, task, TRUE);
  else if (visible)
    {
      pf_buffer_character (filter, '\n');
      battle_print_combatant (game, npc, 0);
      pf_buffer_string (filter, " falls down, dead.\n");
    }

  /* Re-home the dead NPC's held and worn objects to that room, so its
   * inventory is not orphaned along with the hidden character. */
  if (room >= 0)
    {
      for (object = 0; object < gs_object_count (game); object++)
        {
          const scr_int position = gs_object_position (game, object);

          if ((position == OBJ_HELD_NPC || position == OBJ_WORN_NPC)
              && gs_object_parent (game, object) == npc)
            gs_object_to_room (game, object, room);
        }
    }

  /* Remove the dead NPC from play (location zero is "hidden"). */
  gs_set_npc_location (game, npc, 0);
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

  stamina = (npc < 0) ? gs_playerstamina (game) : gs_npc_stamina (game, npc);
  stamina -= damage;

  if (stamina <= 0)
    {
      if (npc < 0)
        gs_set_playerstamina (game, 0);
      else
        gs_set_npc_stamina (game, npc, 0);
      battle_kill (game, npc, visible);
      return;
    }

  if (npc < 0)
    gs_set_playerstamina (game, stamina);
  else
    gs_set_npc_stamina (game, npc, stamina);

  /* Run the NPC's low-stamina task when dropping below 10% of maximum. */
  maximum = battle_attribute_max (game, npc, "Stamina");
  if (maximum > 0 && stamina < maximum / 10)
    {
      task = battle_npc_battle_task (game, npc, "StaminaTask");
      if (task >= 0)
        task_run_task (game, task, TRUE);
    }
}

/*
 * battle_resolve()
 *
 * Resolve a single attack from attacker against target with the given wielded
 * weapon (-1 for none).  Combat messages are printed only when visible.
 */
static void
battle_resolve (scr_gameref_t game, scr_int attacker, scr_int target,
                scr_int weapon, scr_bool visible)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (battle_unconfigured
      || battle_legacy
      || battle_eff_accuracy (game, attacker, weapon)
         > battle_eff_agility (game, target))
    {
      scr_int damage = battle_eff_strength (game, attacker, weapon)
                      - battle_eff_defence (game, target);

      if (visible)
        {
          battle_print_combatant (game, attacker, 0);
          pf_buffer_string (filter, (attacker < 0) ? " hit " : " hits ");
          battle_print_combatant (game, target, 1);
        }
      if (damage > 0)
        {
          if (visible)
            pf_buffer_string (filter, ".\n");
          battle_apply_damage (game, target, damage, visible);
        }
      else if (visible)
        pf_buffer_string (filter,
                          ", but it doesn't seem to do any damage.\n");
    }
  else if (visible)
    {
      battle_print_combatant (game, target, 0);
      pf_buffer_string (filter, (target < 0) ? " manage to avoid "
                                             : " manages to avoid ");
      battle_print_combatant (game, attacker, 2);
      pf_buffer_string (filter, " attack.\n");
    }
}

/*
 * battle_select_target()
 *
 * Choose a target for an attacking NPC, following the Runner: neutrals never
 * attack; allies target enemies and vice versa (their attitudes summing to
 * three), and enemies also target the player.  A uniformly random choice is
 * made among the candidates sharing the NPC's room.  Returns an NPC index,
 * BATTLE_PLAYER, or BATTLE_NONE.
 */
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
  for (other = 0; other < gs_npc_count (game); other++)
    {
      if (other != npc
          && gs_npc_location (game, other) == location
          && gs_npc_stamina (game, other) > 0
          && battle_attitude (game, other) == 3 - attitude)
        count++;
    }
  if (attitude == 2 && location - 1 == gs_playerroom (game)
      && gs_playerstamina (game) > 0)
    count++;

  if (count == 0)
    return BATTLE_NONE;

  /* Pick one candidate at random, re-walking the same candidate order. */
  pick = scr_randomint (1, count);
  for (other = 0; other < gs_npc_count (game); other++)
    {
      if (other != npc
          && gs_npc_location (game, other) == location
          && gs_npc_stamina (game, other) > 0
          && battle_attitude (game, other) == 3 - attitude)
        {
          if (--pick == 0)
            return other;
        }
    }
  return BATTLE_PLAYER;
}

/*
 * battle_recover()
 *
 * Apply automatic stamina recovery for the player (npc < 0) or an NPC: every
 * Recovery turns, restore one point of stamina up to the maximum.
 */
static void
battle_recover (scr_gameref_t game, scr_int npc)
{
  scr_int recovery, counter, stamina, maximum;

  recovery = battle_get_property (game, npc, "Recovery", 0);
  if (recovery <= 0)
    return;

  counter = (npc < 0)
            ? gs_playerstaminacounter (game) : gs_npc_staminacounter (game, npc);
  stamina = (npc < 0) ? gs_playerstamina (game) : gs_npc_stamina (game, npc);
  maximum = battle_attribute_max (game, npc, "Stamina");

  if (counter == 0)
    {
      counter = recovery;
      if (stamina < maximum)
        {
          stamina++;
          if (npc < 0)
            gs_set_playerstamina (game, stamina);
          else
            gs_set_npc_stamina (game, npc, stamina);
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
 * battle_player_default_weapon()
 *
 * Return the weapon the player would attack with by default: the explicitly
 * wielded weapon if it is still held and a weapon, otherwise the best carried
 * weapon (-1 for bare hands).
 */
scr_int
battle_player_default_weapon (scr_gameref_t game)
{
  const scr_int wielded = gs_playerwield (game);

  if (wielded >= 0
      && gs_object_position (game, wielded) == OBJ_HELD_PLAYER
      && battle_object_is_weapon (game, wielded))
    return wielded;
  return battle_best_weapon (game, -1);
}

/*
 * battle_combatant_weapon()
 *
 * Return the weapon a combatant fights with: the player's default weapon
 * (npc < 0) or an NPC's best carried weapon (-1 for bare hands).
 */
scr_int
battle_combatant_weapon (scr_gameref_t game, scr_int npc)
{
  return (npc < 0) ? battle_player_default_weapon (game)
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
  const scr_int weapon = battle_combatant_weapon (game, npc);

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
 * battle_player_attack()
 *
 * Resolve a player-initiated attack on an NPC.  When weapon is -1 the player's
 * default weapon (wielded, else best carried, else bare hands) is used.
 */
void
battle_player_attack (scr_gameref_t game, scr_int npc, scr_int weapon)
{
  if (weapon < 0)
    weapon = battle_player_default_weapon (game);
  battle_resolve (game, BATTLE_PLAYER, npc, weapon, TRUE);
}

/*
 * battle_tick()
 *
 * Per-turn battle processing: every NPC that is due to attack selects a target
 * and strikes, then automatic stamina recovery is applied to all combatants.
 * A no-op when the Battle System is disabled.
 */
void
battle_tick (scr_gameref_t game)
{
  scr_int npc;

  if (!battle_is_enabled (game))
    return;

  /* NPC attacks, governed by attitude and per-NPC attack cadence. */
  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      scr_int counter;

      if (gs_npc_stamina (game, npc) <= 0 || battle_attitude (game, npc) == 0)
        continue;

      counter = gs_npc_attackcounter (game, npc) - 1;
      if (counter <= 0)
        {
          scr_int target = battle_select_target (game, npc);

          if (target != BATTLE_NONE)
            {
              scr_bool visible = (target == BATTLE_PLAYER)
                  || (gs_npc_location (game, npc) - 1 == gs_playerroom (game));
              battle_resolve (game, npc, target,
                              battle_best_weapon (game, npc), visible);
            }
          counter = battle_speed_roll (game, npc);
        }
      gs_set_npc_attackcounter (game, npc, counter);

      /* Stop processing if the player was killed mid-round. */
      if (!game->is_running)
        return;
    }

  /* Automatic stamina recovery for the player and all surviving NPCs. */
  battle_recover (game, -1);
  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      if (gs_npc_stamina (game, npc) > 0)
        battle_recover (game, npc);
    }
}
