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
 * Prototypes for scbattle.cpp: the battle system.
 */

#ifndef SCARIER_BATTLE_H
#define SCARIER_BATTLE_H

#include "sctypes.h"

/* Battle system functions (scbattle.cpp). */
extern scr_bool battle_is_enabled (scr_gameref_t game);
extern void battle_set_combat_assist (scr_bool flag);
extern scr_bool battle_get_combat_assist (void);
extern scr_int battle_attribute_max (scr_gameref_t game, scr_int npc,
                                    const scr_char *base);
extern void battle_start (scr_gameref_t game);
extern void battle_preroll_player_stamina (scr_gameref_t game);
extern void battle_preroll_npc_stamina (scr_gameref_t game);
extern void battle_preroll_legacy (scr_gameref_t game);
extern void battle_change_attribute (scr_gameref_t game, scr_int npc,
                                     scr_int attribute, scr_int value);
extern scr_bool battle_is_weapon (scr_gameref_t game, scr_int object);
extern scr_int battle_weapon_method (scr_gameref_t game, scr_int object);
extern scr_int battle_player_wielded_weapon (scr_gameref_t game);
extern scr_int battle_player_weapon_count (scr_gameref_t game);
extern scr_int battle_player_best_weapon (scr_gameref_t game);
extern scr_int battle_combatant_weapon (scr_gameref_t game, scr_int npc);
extern void battle_attribute_report (scr_gameref_t game, scr_int npc,
                                     const scr_char *base, scr_int *lo,
                                     scr_int *hi, scr_int *current);
extern scr_int battle_attribute_bonus (scr_gameref_t game, scr_int npc,
                                       const scr_char *base);
extern void battle_player_attack (scr_gameref_t game, scr_int npc,
                                  scr_int weapon);
extern void battle_tick_npc (scr_gameref_t game, scr_int npc);
extern void battle_recover_line (scr_gameref_t game);

#endif
