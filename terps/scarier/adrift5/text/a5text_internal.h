/* vi: set ts=8:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * Written from a reading of the ADRIFT 5 Runner source as released in
 * FrankenDrift, to reproduce its behaviour; no code from it is used.
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
 *
 * ADRIFT 5 support for Scarier -- private shared declarations for the text
 * engine translation units (a5text.cpp and the a5text_*.cpp it was split
 * into).  NOT a public interface -- host code uses a5text.h.
 */

#ifndef SCARIER_A5TEXT_INTERNAL_H
#define SCARIER_A5TEXT_INTERNAL_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../a5expr.h"
#include "../a5restr.h"
#include "../a5sb.h"     /* sb_t growable string builder (shared with a5run_*) */
#include "../a5sexpr.h"
#include "../a5text.h"
#include "../a5util.h"

/* strcasecmp() and strndup() where the toolchain lacks them. */
#include "../../common_utils/sc_garglk.h"

/* ----------------------------------------------------------- small helpers */

static inline int
ci_eq (const char *a, const char *b)
{
  if (a == nullptr || b == nullptr)
    return 0;
  for (; *a && *b; a++, b++)
    if (tolower ((unsigned char) *a) != tolower ((unsigned char) *b))
      return 0;
  return *a == *b;
}

/* Empty an sb_t in place, keeping its allocation. */
static inline void
sb_clear (sb_t *sb)
{
  sb->len = 0;
  if (sb->p)
    sb->p[0] = '\0';
}

/* Join names the way the runner's list builders do: "a", "a and b",
   "a, b and c". */
static inline std::string
join_and (const std::vector<std::string> &names)
{
  std::string out;
  for (size_t i = 0; i < names.size (); i++)
    {
      if (i > 0)
        out += (i + 1 == names.size ()) ? " and " : ", ";
      out += names[i];
    }
  return out;
}

/* The pronoun a character name renders as (clsCharacter.Name's PronounEnum). */
typedef enum { A5_PRO_SUBJ, A5_PRO_OBJ, A5_PRO_POSS, A5_PRO_REFL } a5_pronoun_t;

/* ------------------------------------------- a5text.cpp: pipeline and ALRs */

/* Does s end with text that warrants a double space before the next segment? */
int   add_space (const char *s, size_t len);
char *location_long_desc (a5_state_t *st, const char *lockey);
char *auto_capitalise (const char *src);
/* %functions% / OO / <#exprs#> / ALRs, no capitalisation (depth = ALR recursion). */
char *process_inner (a5_state_t *st, const char *src, int depth);
char *replace_alrs (a5_state_t *st, const char *src, int depth);
char *seal_alr_sites (a5_state_t *st, char *text);
char *ps_mark_trailing (const char *proc, char *plain);
/* Shield <#...#> blocks from the function/OO passes, and put them back. */
std::string protect_exprs (const char *src, std::vector<std::string> &saved);
std::string restore_exprs (const char *src, const std::vector<std::string> &saved);
/* Replace every occurrence of `find` in `src` with `repl`. */
char *str_replace_all (const char *src, const char *find, const char *repl);
int   expr_bears_random (const char *body);

/* -------------------------------------- a5text_name.cpp: entity naming */

/* FirstPerson=1 / SecondPerson=2 / ThirdPerson=3 for the CURRENT player. */
int   char_perspective (const a5_state_t *st, const a5_character_t *c);
/* "article [prefix] first-descriptor"; NULL/empty article is omitted. */
std::string char_descriptor_name (const a5_character_t *c, const char *article);
/* clsCharacter.ProperName: the runtime CharacterProperName, else Name, else
   "Anonymous". */
const char *char_proper_name_raw (const a5_state_t *st, const a5_character_t *c);
/* clsCharacter.Name with a pronoun; consult_ledger = pronoun-replace a
   same-turn mention (the %CharacterName% path only). */
char *character_name (a5_state_t *st, const a5_character_t *c, a5_pronoun_t pr,
                      int consult_ledger = 0);
/* Proper name once Known, else the descriptor form; displaying = run
   clsCharacter.Name's Introduced upgrade under bDisplaying. */
char *character_display_name (a5_state_t *st, const a5_character_t *c, int definite,
                              int displaying = 1);
/* A bare %reference% name -> malloc'd "key" / "key1|key2", or NULL. */
char *oo_firstkey (a5_state_t *st, const char *name);
/* "object2" -> "ReferencedObject2" etc.; 0 when not a reference word. */
int   ref_slot_name (const char *name, std::string &slot);
int   is_obj_char_ref (const char *name);

/* ------------------------------- a5text_func.cpp: %function% replacement */

/* One built-in %name[args]%; NULL = unhandled, leave the token verbatim. */
char *eval_function (a5_state_t *st, const char *name, const char *args);
/* Record a pending %CharacterName% render as a PronounKeys entry at `off`. */
void  pron_capture (a5_state_t *st, long off);
/* Replace %function%/%variable% tokens (single pass; args recurse).  as_arg:
   src is the argument list of an enclosing %Func[...]%, so a bare
   object/character reference resolves to its KEY, not its name. */
char *replace_functions (a5_state_t *st, const char *src, int as_arg = 0);

/* ------------------------------------------ a5text_view.cpp: the room view */

/* clsLocation.ViewLocation for `lockey` (NULL = the player's location). */
char *view_location_impl (a5_state_t *st, const char *lockey);

#endif /* SCARIER_A5TEXT_INTERNAL_H */
