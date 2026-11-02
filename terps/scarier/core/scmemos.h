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
 * Prototypes for scmemos.cpp: undo memos and the command history.
 */

#ifndef SCARIER_MEMOS_H
#define SCARIER_MEMOS_H

#include <string>
#include "sctypes.h"

/* Game memo opaque typedef and functions. */
typedef struct scr_memo_set_s *scr_memo_setref_t;
extern scr_memo_setref_t memo_create (void);
extern void memo_destroy (scr_memo_setref_t memento);
extern void memo_save_game (scr_memo_setref_t memento, scr_gameref_t game,
                            const scr_char *text);
extern scr_bool memo_load_game (scr_memo_setref_t memento, scr_gameref_t game,
                                std::string *text);
extern scr_bool memo_is_load_available (scr_memo_setref_t memento);
extern scr_int memo_get_undo_count (scr_memo_setref_t memento);
extern const scr_byte *memo_get_undo (scr_memo_setref_t memento,
                                      scr_int index_, scr_int *length);
extern void memo_append_undo (scr_memo_setref_t memento,
                              const scr_byte *data, scr_int length);
extern const scr_char *memo_get_undo_text (scr_memo_setref_t memento,
                                           scr_int index_);
extern void memo_set_undo_text (scr_memo_setref_t memento, scr_int index_,
                                const scr_char *text);
extern void memo_save_command (scr_memo_setref_t memento,
                               const scr_char *command, scr_int timestamp,
                               scr_int turns);
extern void memo_restore_command (scr_memo_setref_t memento,
                                  const scr_char *command, scr_int sequence,
                                  scr_int timestamp, scr_int turns);
extern void memo_unsave_command (scr_memo_setref_t memento);
extern scr_int memo_get_command_count (scr_memo_setref_t memento);
extern void memo_first_command (scr_memo_setref_t memento);
extern void memo_next_command (scr_memo_setref_t memento,
                               const scr_char **command, scr_int *sequence,
                               scr_int *timestamp, scr_int *turns);
extern scr_bool memo_more_commands (scr_memo_setref_t memento);
extern const scr_char *memo_find_command (scr_memo_setref_t memento,
                                         scr_int sequence);
extern void memo_clear_commands (scr_memo_setref_t memento);

#endif
