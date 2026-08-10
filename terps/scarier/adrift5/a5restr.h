/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- restriction evaluation (Phase 2 subset).
 *
 * Evaluates a <Restrictions> block (the restricted-description and, later,
 * task gating mechanism) against the runtime state.  A restriction is a single
 * line "<key1> Must|MustNot <Operator> <key2>" wrapped in a type element
 * (<Object>/<Location>/<Character>/<Variable>/<Task>/...), combined by the
 * block's <BracketSequence> ('#' = a restriction, 'A' = and, 'O' = or, with
 * '('/'[' and ')'/']' brackets).
 *
 * Phase 2 implements the object/location/variable/task operators the static
 * world's descriptions depend on, plus best-effort character handling; the
 * structure (a ported EvaluateRestrictionBlock + a typed PassSingleRestriction)
 * is the one Phase 3 grows for the full task engine.  Mirrors the Adrift 5 runner
 * clsUserSession.PassRestrictions / EvaluateRestrictionBlock / PassSingleRestriction.
 */

#ifndef SCARIER_A5RESTR_H
#define SCARIER_A5RESTR_H

#include "a5state.h"
#include "a5xml.h"

/* 1 if the restriction block passes (an empty/absent block passes). */
extern int a5restr_pass (a5_state_t *st, const a5_xml_node_t *restrictions);

/*
 * 1 if the block contains a restriction with the `Exist` operator on the given
 * reference type ('o' = Object, 'c' = Character).  Mirrors clsTask's
 * HasObjectExistRestriction / HasCharacterExistRestriction: it is the gate that
 * decides whether a command whose reference resolves to nothing nonetheless
 * "matches" the task (so the task can surface its Must-Exist message on the
 * second-chance pass) rather than not matching at all.  Must/MustNot is ignored,
 * exactly like the originals (they test eObject/eCharacter == Exist only).
 */
extern int a5restr_has_exist (const a5_xml_node_t *restrictions, char type);

/* The <Message> node of the task's first "ReferencedObject(s) Must Exist" Object
 * restriction, or NULL.  Used to prefix a failing multi-match %objects% noun with
 * the Adrift 5 runner's "Sorry, I'm not sure which object ..." ambiguity indicator. */
extern const a5_xml_node_t *a5restr_exist_message (const a5_xml_node_t *restrictions);

/*
 * Does a `<ref_alias> Must Exist` restriction (e.g. "ReferencedObject2") fall
 * within the task's *evaluated* BracketSequence (the first M restrictions, M =
 * number of '#' in the bracket)?  Lets resolve_refine decide whether an unmatched
 * reference is genuinely required: a Must Exist truncated out of the bracket means
 * the reference is optional, so a sibling reference's out-of-scope ambiguity
 * surfaces instead of this reference's no-reference fallback.
 */
extern int a5restr_exist_evaluated (const a5_xml_node_t *restrictions,
                                    const char *ref_alias);

/*
 * The <Message> description node of the first restriction that individually
 * fails (document order), or NULL.  Used to surface a task's failure text;
 * faithful for the common AND-chain case.
 */
extern const a5_xml_node_t *a5restr_fail_message (a5_state_t *st,
                                                  const a5_xml_node_t *restrictions);

/*
 * The destination key for `charkey` moving out of `lockey` in the canonical
 * direction `dir` ("SouthEast", "Up", ...), honouring the exit's own
 * restrictions, or NULL if there is no (passable) route.  The result may be a
 * location key or a location-group key (the caller resolves a group to a
 * concrete room).
 *
 * When `blocked_msg` is non-NULL it is set to the blocking exit-restriction's
 * <Message> node if the exit exists but its restriction fails (left untouched
 * otherwise) -- the Adrift 5 runner's sRouteError, which overrides the movement task's
 * generic "There is no route..." message.
 *
 * The evaluation is memoised per (charkey, lockey, dir) for the current turn,
 * mirroring clsCharacter.dictHasRouteCache: the first check in a turn decides,
 * even if a later action changes the gating state.  a5restr_route_cache_clear
 * is the PrepareForNextTurn clear (clsUserSession.vb:3792); _free releases the
 * storage with the state.
 */
extern const char *a5restr_exit_in_direction (a5_state_t *st, const char *charkey,
                                              const char *lockey, const char *dir,
                                              const a5_xml_node_t **blocked_msg);
extern void a5restr_route_cache_clear (a5_state_t *st);
extern void a5restr_route_cache_free (a5_state_t *st);

/* Has this exit's route restriction ever evaluated false?  The runner's
   clsDirection.bEverBeenBlocked, which HasRouteInDirection sets on every
   failed evaluation and nothing ever resets.  The map draws a restricted
   connector solid until this says the player has actually been blocked there
   (Map.vb:1447).  _free releases the ledger with the state. */
extern int a5restr_ever_blocked (a5_state_t *st, const char *lockey,
                                 const char *dir);
extern void a5restr_ever_blocked_free (a5_state_t *st);

/* Trace restriction evaluation to stderr (driven by the harness/A5_TRACE). */
extern int a5restr_trace;

#endif
