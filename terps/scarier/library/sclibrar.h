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

#ifndef SCLIBRAR_H
#define SCLIBRAR_H

/*
 * Internal header of the library, shared by all the topic files
 * sclibrar_*.cpp.  Nothing here is part of the interface the rest of Scarier
 * uses; that stays in core/sclibrar_api.h.  What the eleven core topic files
 * share only among themselves is in sclibrar_internal.h.
 */

/*
 * Every topic file includes this header first; the system and engine headers
 * the library draws on are gathered here so the topic files need nothing
 * else.
 */
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "core/scarier.h"
#include "core/scprotos.h"
#include "core/scgamest.h"

/* Assorted definitions and constants. */
static const scr_char COMMA = ',';
enum
{ SECS_PER_MINUTE = 60,
  MINS_PER_HOUR = 60,
  SECS_PER_HOUR = 3600
};
enum { LIB_ALLOCATION_AVOIDANCE_SIZE = 128 };

/*
 * A gathered list of objects, NPCs, or directions, and the printer used for
 * one of its elements.  See lib_print_list() below.
 */
typedef std::vector<scr_int> lib_list_t;
typedef void (*lib_print_item_t) (scr_gameref_t game, scr_int item);

/* Trace flag, set before running. */
extern scr_bool lib_trace;


/* Types shared across the library files. */

enum { LIB_NPC_HERE_LENGTH = 9 };       /* strlen (" is here.") */

enum { NPC_PICK_ASK, NPC_PICK_FIRST, NPC_PICK_LAST };

enum lib_with_clause_t
{ LIB_WITH_NONE, LIB_WITH_DECLINE, LIB_WITH_ANSWERED, LIB_WITH_SUFFIX };


/* State shared across the library files, each defined where named. */

/* sclibrar_room.cpp */
extern const scr_char *const DIRNAMES_8[];

/* sclibrar_battle.cpp */
extern std::string lib_battle_who_pending;


/* Functions shared across the library files, each defined where named. */

/* sclibrar_print.cpp */
extern scr_bool lib_use_room_alt (scr_gameref_t game, scr_int room,
                                  scr_int alt);
extern scr_bool lib_room_is_dark (scr_gameref_t game, scr_int room);
extern scr_int lib_find_starting_alt (scr_gameref_t game, scr_int room);
extern void lib_print_room_name_lower (scr_gameref_t game, scr_int room);
extern scr_bool lib_compare_article_binary (const scr_char *string,
                                            const scr_char *word,
                                            scr_int length);
extern void lib_print_object (scr_gameref_t game, scr_int object);
extern void lib_print_object_raw (scr_gameref_t game, scr_int object);
extern scr_bool lib_is_version_400 (scr_gameref_t game);
extern scr_bool lib_is_version_390 (scr_gameref_t game);
extern void lib_set_admin (scr_gameref_t game);
extern scr_bool lib_matcher_requires_seen (scr_gameref_t game);
extern const scr_char * lib_select_response (scr_gameref_t game,
                                             const scr_char *second_person,
                                             const scr_char *first_person,
                                             const scr_char *third_person);
extern const scr_char *
lib_select_plurality (scr_gameref_t game, scr_int object,
                      const scr_char *singular, const scr_char *plural);
extern const scr_char *
lib_select_list_plurality (scr_gameref_t game, scr_int container,
                           const lib_list_t &list, const scr_char *singular,
                           const scr_char *plural);
extern void lib_print_wrapped_object (scr_gameref_t game,
                                      const scr_char *prefix, scr_int object,
                                      const scr_char *suffix);
extern void lib_print_wrapped_npc (scr_gameref_t game, const scr_char *prefix,
                                   scr_int npc, const scr_char *suffix);
extern void lib_print_response_object (scr_gameref_t game,
                                       const scr_char *second_person,
                                       const scr_char *first_person,
                                       const scr_char *third_person,
                                       scr_int object, const scr_char *suffix);
extern void lib_print_response_npc (scr_gameref_t game,
                                    const scr_char *second_person,
                                    const scr_char *first_person,
                                    const scr_char *third_person,
                                    scr_int npc, const scr_char *suffix);
extern scr_bool lib_print_message (scr_gameref_t game,
                                   const scr_char *message);
extern scr_bool lib_print_response_message (scr_gameref_t game,
                                            const scr_char *second_person,
                                            const scr_char *first_person,
                                            const scr_char *third_person);
extern void lib_new_clause (scr_gameref_t game, scr_bool has_printed);
extern void lib_print_clause (scr_gameref_t game, scr_bool has_printed,
                              const scr_char *second_person,
                              const scr_char *first_person,
                              const scr_char *third_person);
extern void lib_collect_children (scr_gameref_t game, scr_int parent,
                                  scr_int position, lib_list_t &list);
extern scr_int lib_reveal_children (scr_gameref_t game, scr_int parent,
                                    scr_int position, lib_list_t &list);
extern scr_int lib_count_npc_references (scr_gameref_t game, scr_int *npc);
extern void lib_print_list (scr_gameref_t game, const lib_list_t &list,
                            lib_print_item_t print_item,
                            const scr_char *conjunction);
extern void lib_print_name_list (scr_gameref_t game, const lib_list_t &list,
                                 const scr_char *const *names,
                                 const scr_char *conjunction);
extern scr_bool
lib_print_object_list (scr_gameref_t game, scr_bool has_printed,
                       const lib_list_t &list, const scr_char *conjunction,
                       scr_char terminator, const scr_char *second_person,
                       const scr_char *first_person,
                       const scr_char *third_person,
                       lib_print_item_t print_item = lib_print_object_np);
extern const scr_char *
lib_get_npc_inroom_text (scr_gameref_t game, scr_int npc);
extern scr_bool lib_npc_text_is_default (const scr_char *description);
extern scr_bool lib_inroomdesc_is_absent (const scr_char *inroomdesc);

/* sclibrar_room.cpp */
extern const scr_char *const * lib_compass_names (scr_gameref_t game);
extern scr_bool lib_room_exit_available (scr_gameref_t game, scr_int room,
                                         scr_int direction);
extern scr_bool lib_room_exit_destination (scr_gameref_t game,
                                           scr_int direction,
                                           scr_int *destination);
extern void lib_describe_player_room (scr_gameref_t game,
                                      scr_bool force_verbose);

/* sclibrar_go.cpp */
extern std::string lib_command_slot_370 (scr_prop_setref_t bundle,
                                         scr_int slot);

/* sclibrar_disambig.cpp, _disambig_scan, _disambig_400 */
extern scr_int lib_disambiguate_npc (scr_gameref_t game, const scr_char *verb,
                                     scr_bool *is_ambiguous);
extern scr_int lib_last_named_npc (scr_gameref_t game);
extern scr_int lib_disambiguate_npc_pick (scr_gameref_t game,
                                          const scr_char *verb,
                                          scr_bool *is_ambiguous,
                                          scr_int pick);
extern scr_bool lib_co_contains (const scr_char *command,
                                 const scr_char *term);
extern scr_int lib_alias_prepare (const scr_prop_setref_t bundle,
                                  scr_vartype_t *vt_key,
                                  const scr_char *category, scr_int index);
extern const scr_char * lib_first_alias (const scr_prop_setref_t bundle,
                                         scr_vartype_t *vt_key,
                                         const scr_char *category,
                                         scr_int index);
extern scr_int lib_disambiguate_object (scr_gameref_t game,
                                        const scr_char *verb,
                                        scr_bool *is_ambiguous);

/* sclibrar_examine.cpp */
extern scr_bool lib_cant_see_absent_object (scr_gameref_t game,
                                            const scr_char *suffix,
                                            scr_bool is_definite);
extern scr_int
lib_absent_named_object_pre_390 (scr_gameref_t game, scr_bool last = FALSE);
extern scr_bool lib_cant_see_named_pre_390 (scr_gameref_t game,
                                            scr_int object,
                                            scr_bool is_definite,
                                            const scr_char *suffix);
extern scr_int lib_examine_referencedob_400 (scr_gameref_t game,
                                             const scr_char *input);
extern scr_bool lib_examine_tail (scr_gameref_t game, scr_int object,
                                  scr_bool is_described);
extern scr_int lib_examine_crowded_390 (scr_gameref_t game);

/* sclibrar_take_multiple.cpp */
extern scr_bool lib_co_pre400 (scr_gameref_t game, const scr_char *line,
                               scr_int object, scr_int mode);

/* sclibrar_open.cpp */
extern lib_with_clause_t
lib_with_clause_400 (scr_gameref_t game, scr_int *object, scr_int *instrument);

/* sclibrar_topic.cpp */
extern scr_bool lib_npc_reply_to (scr_gameref_t game, scr_int npc,
                                  scr_int topic);
extern scr_int lib_npc_find_topic (scr_gameref_t game, scr_int npc);

/* sclibrar_battle.cpp */
extern scr_bool lib_npc_named_in_line (scr_gameref_t game, scr_int npc,
                                       const scr_char *input);
extern scr_bool lib_npc_referenced (scr_gameref_t game, scr_int npc,
                                    const scr_char *input);

/* sclibrar_sitstand.cpp */
extern scr_int lib_player_parent_here (scr_gameref_t game);
extern scr_bool lib_get_off_may_take (scr_gameref_t game);

/* sclibrar_talk.cpp */
extern const scr_char * lib_ask_format_subject (scr_gameref_t game);

/* sclibrar_refuse.cpp */
extern scr_bool lib_what (scr_gameref_t game, const scr_char *verb);

/* sclibrar_verbobj.cpp */
extern scr_int lib_verb_object_name_score (scr_gameref_t game, scr_int object,
                                           const scr_char *input);
extern scr_int
lib_verb_object_resolve_400_string (scr_gameref_t game, const scr_char *input,
                                    std::vector<scr_int> *tied,
                                    scr_bool present_only);

#endif
