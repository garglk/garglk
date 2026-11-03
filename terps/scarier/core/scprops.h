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
 * Prototypes for scprops.cpp: the properties store a parsed game lives in.
 */

#ifndef SCARIER_PROPS_H
#define SCARIER_PROPS_H

#include "sctaffil.h"

/* Properties store enumerations, opaque typedef, and functions. */
enum
{ PROP_KEY_STRING = 's',
  PROP_KEY_INTEGER = 'i'
};
enum
{ PROP_INTEGER = 'I',
  PROP_BOOLEAN = 'B',
  PROP_STRING = 'S'
};

typedef struct scr_prop_set_s *scr_prop_setref_t;
extern scr_prop_setref_t prop_create (const scr_tafref_t taf);
extern void prop_destroy (scr_prop_setref_t bundle);
extern void prop_put (scr_prop_setref_t bundle,
                      const scr_char *format, scr_vartype_t vt_value,
                      const scr_vartype_t vt_key[]);
extern scr_bool prop_get (scr_prop_setref_t bundle,
                         const scr_char *format, scr_vartype_t *vt_value,
                         const scr_vartype_t vt_key[]);
extern void prop_solidify (scr_prop_setref_t bundle);
extern scr_int prop_get_integer (scr_prop_setref_t bundle,
                                const scr_char *format,
                                const scr_vartype_t vt_key[]);
extern scr_bool prop_get_boolean (scr_prop_setref_t bundle,
                                 const scr_char *format,
                                 const scr_vartype_t vt_key[]);
extern const scr_char *prop_get_string (scr_prop_setref_t bundle,
                                       const scr_char *format,
                                       const scr_vartype_t vt_key[]);
extern scr_int prop_get_child_count (scr_prop_setref_t bundle,
                                    const scr_char *format,
                                    const scr_vartype_t vt_key[]);
/* bundle["Globals"][name] -- the game header properties. */
extern scr_int prop_get_global_integer (scr_prop_setref_t bundle,
                                       const scr_char *name);
extern scr_bool prop_get_global_boolean (scr_prop_setref_t bundle,
                                        const scr_char *name);
extern const scr_char *prop_get_global_string (scr_prop_setref_t bundle,
                                              const scr_char *name);
/* bundle[...][Var1..Var3] -- the first count integers of a task entry. */
extern void prop_get_var_integers (scr_prop_setref_t bundle,
                                   scr_vartype_t vt_key[], scr_int count,
                                   scr_int *var1, scr_int *var2,
                                   scr_int *var3);
/* bundle["Version"] -- the TAF_VERSION_* constant of the parsed .taf. */
extern scr_int prop_get_taf_version (scr_prop_setref_t bundle);
/* bundle[class_][index_][name] -- one field of one indexed entity. */
extern scr_int prop_get_indexed_integer (scr_prop_setref_t bundle,
                                        const scr_char *class_,
                                        scr_int index_, const scr_char *name);
extern scr_bool prop_get_indexed_boolean (scr_prop_setref_t bundle,
                                         const scr_char *class_,
                                         scr_int index_, const scr_char *name);
extern const scr_char *prop_get_indexed_string (scr_prop_setref_t bundle,
                                               const scr_char *class_,
                                               scr_int index_,
                                               const scr_char *name);
extern scr_bool prop_put_integer (scr_prop_setref_t bundle,
                                 const scr_char *format,
                                 scr_int value, const scr_vartype_t vt_key[]);
extern scr_bool prop_put_string (scr_prop_setref_t bundle,
                                const scr_char *format,
                                const scr_char *value,
                                const scr_vartype_t vt_key[]);
extern void prop_adopt (scr_prop_setref_t bundle, void *addr);
/* The TAF the properties were parsed from, still owned by the bundle. */
extern scr_tafref_t prop_get_taf (scr_prop_setref_t bundle);
extern void prop_debug_trace (scr_bool flag);
extern void prop_debug_dump (scr_prop_setref_t bundle);

#endif
