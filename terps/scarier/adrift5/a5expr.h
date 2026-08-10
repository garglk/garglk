/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- the object-oriented property-expression engine.
 *
 * ADRIFT 5 text embeds "property expressions": a key (or %reference%) followed by
 * one or more ".Property" / ".Function(args)" navigation steps, e.g.
 *
 *     %object%.Description
 *     %objects%.Name
 *     %Player%.Held(False).List(Indefinite, False)
 *     %objects%.Parent.Name
 *
 * The first key resolves to an object/character/location/group (or a pipe-joined
 * list of keys), then each step navigates (Parent/Children/Contents/Held/Worn/
 * Location/Objects) or terminates (Name/List/Description/Count/Sum/<property>).
 *
 * This is a port of the Adrift 5 runner Global.ReplaceOO / ReplaceOOProperty
 * (Global.vb:619-1620).  a5text calls a5expr_eval when it sees a %reference% (or
 * bare key) immediately followed by '.', having already substituted the
 * reference to its resolved entity key(s).
 *
 * Every returned char* is heap-allocated; the caller frees it.
 */

#ifndef SCARIER_A5EXPR_H
#define SCARIER_A5EXPR_H

#include "a5state.h"

/*
 * Evaluate the property expression "<firstkeys><chain>", where `firstkeys` is a
 * single entity key or a "key1|key2|..." pipe list and `chain` is the remainder
 * starting at the first '.' (e.g. ".Name", ".Held(False).List(Indefinite)").
 * Returns the rendered text, or NULL if the expression is not resolvable (the
 * caller then leaves the original token verbatim).
 *
 * `force_list` makes the head a collection even for a single key -- a plural
 * reference (%objects%/%characters%) is always a collection in the Adrift 5 runner
 * (clsObjectList/clsCharacterList), so list aggregators like .Sum/.Count walk it
 * (a missing per-item property contributes 0, not "") even when one item bound.
 */
extern char *a5expr_eval (a5_state_t *st, const char *firstkeys,
                          const char *chain, int force_list = 0);

/*
 * Scan `text` for property expressions "<entitykey>.<chain>" (the first token
 * being a real entity key, or a "key1|key2" pipe list, followed by one or more
 * ".step") and replace each with its evaluated text.  Run after the %function%
 * pass, once references have been substituted to keys (mirrors ReplaceOO at the
 * end of ReplaceFunctions).  Returns a freshly allocated string.
 */
extern char *a5expr_replace (a5_state_t *st, const char *text);

/* The byte just past the ".step(args)?..." chain beginning at `dot`, using the
   same key-character set a5expr_replace scans with (\w plus '|', and UTF-8 high
   bytes, so a key like "ClaudeMoné" scans whole).  a5text needs this to find the
   end of a "%reference%.Prop..." chain before handing it to a5expr_eval. */
extern const char *a5expr_scan_chain (const char *dot);

/* a5expr_replace in EXPRESSION mode (the runner ReplaceOO bExpression=True,
   Global.vb:645): non-integer OO values are emitted as quoted string
   literals so a `X.Prop & %text%` concatenation parses. */
extern char *a5expr_replace_expr (a5_state_t *st, const char *text);

/* clsCharacter.ListExits(sFromLocation, iExitCount): the character's exit list
   evaluated from `lockey` (the room-view exit line; a %DisplayLocation[loc]%
   view lists THAT location's exits).  Caller frees. */
extern char *a5expr_list_exits (a5_state_t *st, const char *charkey,
                                const char *lockey, long *count);

/*
 * Host hook for an event's OO-properties (Global.ReplaceOOProperty, evt branch):
 *   Event.Position -> evt.TimerFromStartOfEvent
 *   Event.Length   -> evt.Length.Value
 * The event runtime lives in the run harness, not a5_state, so a5run wires this
 * to read the live timers.  Returns 1 and sets *out when resolved, else 0 (NULL
 * hook => the description-only a5text_dump build leaves the term unresolved).
 */
extern int (*a5expr_event_prop_hook) (const char *event_key, const char *prop,
                                       long *out);

#endif
