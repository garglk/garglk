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
 * Prototypes for scvars.cpp: the variables set and the %reference%
 * registers.
 */

#ifndef SCARIER_VARS_H
#define SCARIER_VARS_H

#include <string>
#include "scprops.h"

/* Variables set enumerations, opaque typedef, and functions. */
enum
{ TAFVAR_NUMERIC = 0,
  TAFVAR_STRING = 1
};
enum
{ VAR_INTEGER = 'I',
  VAR_STRING = 'S'
};

typedef struct scr_var_set_s *scr_var_setref_t;
extern scr_bool var_get (scr_var_setref_t vars,
                        const scr_char *name, scr_int *type,
                        scr_vartype_t *vt_rvalue);
extern void var_put_integer (scr_var_setref_t vars,
                             const scr_char *name, scr_int value);
extern scr_int var_get_integer (scr_var_setref_t vars, const scr_char *name);
extern void var_put_string (scr_var_setref_t vars,
                            const scr_char *name, const scr_char *string);
extern const scr_char *var_get_string (scr_var_setref_t vars,
                                      const scr_char *name);
extern scr_bool var_is_user_ordered (scr_var_setref_t vars,
                                     const scr_char *name);
extern scr_bool var_is_unknown_reference (scr_var_setref_t vars,
                                          const scr_char *name);
extern scr_bool var_get_command_number (scr_var_setref_t vars,
                                        const scr_char *name,
                                        scr_int *number);
extern scr_bool var_get_command_text (scr_var_setref_t vars,
                                      const scr_char *name,
                                      std::string &text);
extern scr_bool var_interpolate_user_ordered (scr_var_setref_t vars,
                                              std::string &text);
extern scr_var_setref_t var_create (scr_prop_setref_t bundle);
extern const scr_char *var_indexed_name (scr_prop_setref_t bundle,
                                        scr_int index_);
extern void var_forget_bundle (scr_prop_setref_t bundle);
extern void var_destroy (scr_var_setref_t vars);
extern void var_register_game (scr_var_setref_t vars, scr_gameref_t game);
extern void var_set_ref_character (scr_var_setref_t vars, scr_int character);
extern void var_set_ref_object (scr_var_setref_t vars, scr_int object);
extern void var_set_ref_number (scr_var_setref_t vars, scr_int number);
extern void var_set_ref_text (scr_var_setref_t vars, const scr_char *text);
extern scr_int var_get_ref_character (scr_var_setref_t vars);
extern scr_int var_get_ref_object (scr_var_setref_t vars);
extern scr_int var_get_ref_number (scr_var_setref_t vars);
extern scr_bool var_is_number_referenced (scr_var_setref_t vars);
extern const scr_char *var_number_word (scr_int number);
extern void var_restore_ref_number (scr_var_setref_t vars,
                                    scr_int number, scr_bool is_referenced);
extern const scr_char *var_get_ref_text (scr_var_setref_t vars);
extern scr_uint var_get_elapsed_seconds (scr_var_setref_t vars);
extern void var_set_elapsed_seconds (scr_var_setref_t vars, scr_uint seconds);
extern void var_debug_trace (scr_bool flag);
extern void var_debug_dump (scr_var_setref_t vars);

#endif
