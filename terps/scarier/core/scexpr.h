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
 * Prototypes for scexpr.cpp: expression evaluation.
 */

#ifndef SCARIER_EXPR_H
#define SCARIER_EXPR_H

#include "scvars.h"

/* Expression evaluation functions. */
extern scr_bool expr_eval_numeric_expression (const scr_char *expression,
                                             scr_var_setref_t vars,
                                             scr_int *rvalue);
extern scr_bool expr_eval_string_expression (const scr_char *expression,
                                            scr_var_setref_t vars,
                                            scr_char **rvalue);

#endif
