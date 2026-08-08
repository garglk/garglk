/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- command-pattern matcher (Phase 3).
 *
 * Ports the Adrift 5 runner's ConvertToRE / GetRegularExpression / InputMatchesCommand
 * syntactic layer: an author command pattern such as
 *
 *     [get/take/pick up/remove] %objects% from %object2%
 *
 * is compiled to a regular expression (alternation [a/b], optional {..}, the
 * '_' literal-space and '*' wildcard) whose general references (%object1%,
 * %objects%, %character1%, %direction1%, %number1%, %text1%, ...) become capture
 * groups.  Matching an input line yields the raw text each reference captured;
 * the turn loop (a5run) then resolves that text to a concrete model key in scope.
 *
 * Singular references (%object%, %character%, %text%, ...) are normalised to the
 * numbered form (%object1% ...) exactly as the Adrift 5 runner does at load
 * (FileIO.vb:647), so callers may pass patterns verbatim from the model.
 *
 * Self-contained: uses the C++ <regex> already pulled in at the container edge;
 * the surface is plain C structs so the C-like engine can call it.
 */

#ifndef SCARIER_A5PARSE_H
#define SCARIER_A5PARSE_H

#ifdef __cplusplus
extern "C" {
#endif

#define A5_MAX_REFS 8
#define A5_REF_NAMELEN 24
#define A5_REF_TEXTLEN 256

/* The bound references produced by a successful command match, in pattern order. */
typedef struct a5_match_s {
  int  n_refs;
  char ref_name[A5_MAX_REFS][A5_REF_NAMELEN];  /* "object1","objects","direction1",... */
  char ref_text[A5_MAX_REFS][A5_REF_TEXTLEN];  /* the captured, trimmed substring        */
} a5_match_t;

/*
 * Try to match `input` against a single author command `pattern`.  On a match
 * returns 1 and fills `m` with the captured references (m may be NULL if the
 * caller only wants a yes/no).  Returns 0 on no match (or on a malformed
 * pattern that fails to compile).
 */
extern int a5parse_match_command (const char *pattern, const char *input,
                                  a5_match_t *m);

/*
 * Wildcard-variant matching (clsUserSession.GetRegularExpression): a command
 * containing '*' yields several candidate regexes, tried in order -- variant 0
 * has ALL wildcards removed (so a %reference% consumes the whole tail), the
 * last variant is the original command.  Returns -1 when `variant` is past the
 * variant count, else 0/1 for regex match.  The caller advances to the next
 * variant when a matched variant's object/character reference text names
 * nothing (the runner's InputMatchesObject failure -> DoesntMatch -> next regex).
 * A command without wildcards has exactly one variant.
 */
extern int a5parse_match_command_v (const char *pattern, const char *input,
                                    a5_match_t *m, int variant);

/*
 * clsUserSession.CorrectCommand: bracket-normalise a command pattern before it
 * is matched.  Moves a literal space adjacent to an optional `{...}` group
 * *inside* it (so the space becomes optional) and wraps an optional alternation
 * in `[...]` -- e.g. `[look] [around] {me/you}` -> `[look] [around]{ [me/you]}`,
 * letting a bare "look around" (or a bare-direction "n") match.  The Adrift 5 runner
 * applies this to every task command at game-start init; a5model does the same
 * at load, and a5parse_match_command therefore expects the corrected form.
 * Returns a malloc'd string the caller must free.
 */
extern char *a5_correct_command (const char *cmd);

/* Look up the value of a named reference in a match, or NULL if absent. */
extern const char *a5parse_ref (const a5_match_t *m, const char *name);

/*
 * Canonicalise a direction word ("se", "south-east", "South East") to the
 * PascalCase key ADRIFT uses in a location's Movement table ("SouthEast"), or
 * NULL if the text is not a direction.
 */
extern const char *a5parse_canonical_direction (const char *text);

/* The full lowercased direction-word alternation ("north|n|east|e|..."),
   split on "|" to seed the known-words list (clsUserSession.NotUnderstood). */
extern const char *a5parse_directions_re (void);

/*
 * Install the game's localized direction synonyms (the localization subsystem).
 * `raw_by_enum` is a 12-element array of <Direction*> field strings indexed by
 * DirectionsEnum (North=0, East=1, South=2, West=3, Up=4, Down=5, In=6, Out=7,
 * NorthEast=8, SouthEast=9, SouthWest=10, NorthWest=11); a NULL or empty entry
 * keeps the English default.  Resets to all-English first, so it is idempotent
 * and a later game cannot inherit an earlier one's localization.  Call once at
 * a5run_new before any command matching.  `raw_by_enum` itself may be NULL.
 */
extern void a5parse_set_directions (const char *const *raw_by_enum);

/* The localized display name of a canonical direction key ("North" -> "Nord"),
   or the canonical key for the English default; NULL if not a direction.
   Mirrors Global.DirectionName (the first synonym before the first '/'). */
extern const char *a5parse_direction_name (const char *canonical);

#ifdef __cplusplus
}
#endif

#endif
