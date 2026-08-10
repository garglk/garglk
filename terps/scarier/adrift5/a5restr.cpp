/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- restriction evaluation.  See a5restr.h.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <map>
#include <set>
#include <string>

#include "a5parse.h"
#include "a5rand.h"
#include "a5restr.h"
#include "a5text.h"
#include "a5util.h"

/* strndup() where the toolchain has none; empty everywhere else. */
#include "../common_utils/sc_garglk.h"

int a5restr_trace = 0;

/* CBool-ish (Global.SafeBool): "true"/"false" by name, else a numeric string is
   true iff non-zero; anything else is false. */
static int
safe_bool (const char *s)
{
  char *end;
  double d;
  if (s == NULL) return 0;
  while (*s == ' ') s++;
  if (strcasecmp (s, "true") == 0)  return 1;
  if (strcasecmp (s, "false") == 0) return 0;
  d = strtod (s, &end);
  while (*end == ' ') end++;
  return (end != s && *end == '\0') ? (d != 0.0) : 0;
}

#define ANYOBJECT     "AnyObject"
#define NOOBJECT      "NoObject"
#define ANYCHARACTER  "AnyCharacter"
#define NOCHARACTER   "NoCharacter"
#define PLAYERLOCATION "PlayerLocation"
#define ANYDIRECTION  "AnyDirection"
#define THEFLOOR      "TheFloor"

/* ---------------------------------------------------------- one restriction */

typedef struct {
  const char *type;   /* Object / Location / Character / Variable / Task / ...  */
  char *buf;          /* owned tokenised copy of the spec; key1/op/key2 alias it */
  const char *raw;    /* the original (un-tokenised) spec text -- stable DOM     */
  const char *key1;
  int must_not;
  const char *op;
  const char *key2;   /* may be "" */
} a5_restr_t;

/* Split "key1 Must|MustNot Op key2..." in place.  buf is owned by the restr. */
static void
parse_spec (a5_restr_t *r, const char *spec)
{
  char *p, *tok;
  r->buf = spec ? strdup (spec) : strdup ("");
  r->raw = spec;
  r->key1 = "";
  r->op = "";
  r->key2 = "";
  r->must_not = 0;

  p = r->buf;
  /* key1 */
  while (*p == ' ') p++;
  tok = p;
  while (*p && *p != ' ') p++;
  if (*p) *p++ = '\0';
  r->key1 = tok;
  /* Must / MustNot */
  while (*p == ' ') p++;
  tok = p;
  while (*p && *p != ' ') p++;
  if (*p) *p++ = '\0';
  if (strcmp (tok, "MustNot") == 0)
    r->must_not = 1;
  /* operator */
  while (*p == ' ') p++;
  tok = p;
  while (*p && *p != ' ') p++;
  if (*p) *p++ = '\0';
  r->op = tok;
  /* remainder = key2 (may contain spaces) */
  while (*p == ' ') p++;
  r->key2 = p;
}

/* The type element of a <Restriction> is its first child named a known type. */
static const a5_xml_node_t *
restr_type_node (const a5_xml_node_t *restriction)
{
  static const char *types[] = {
    "Object", "Location", "Character", "Variable", "Task",
    "Item", "Property", "Direction", "Expression", NULL
  };
  const a5_xml_node_t *c;
  for (c = restriction->first_child; c != NULL; c = c->next)
    {
      int i;
      for (i = 0; types[i] != NULL; i++)
        if (strcmp (c->name, types[i]) == 0)
          return c;
    }
  return NULL;
}

/* ------------------------------------------------------------- value helpers */

static const char *
resolve_key (a5_state_t *st, const char *k)
{
  const char *bound;
  if (streq (k, "%Player%"))
    return a5state_player_key (st);
  /* A per-turn binding (ReferencedObject2, ReferencedDirection, ...) wins. */
  bound = a5state_lookup_ref (st, k);
  /* The runner's GetReference("ReferencedObjects") carries NO ReferenceMatch condition
     (clsUserSession.vb:3990): it answers from the first Object-type reference
     in the command whatever its slot, so a restriction keyed "ReferencedObjects"
     also resolves when the command captured a singular %object% (LostLabyrinth's
     "sheath sword" PutObjInSc: `ReferencedObjects Must BeObject Sword`).  Same
     for ReferencedCharacters (vb:4019).  Resolved at lookup (not bound at
     capture) so failed match attempts earlier in the turn can't leak a stale
     alias into a later task's restrictions. */
  if (bound == NULL && streq (k, "ReferencedObjects"))
    bound = a5state_lookup_ref (st, "ReferencedObject");
  if (bound == NULL && streq (k, "ReferencedCharacters"))
    bound = a5state_lookup_ref (st, "ReferencedCharacter");
  if (bound != NULL)
    {
      /* A piped multi-object binding (ReferencedObjects = "k1|k2|...", set by
         resolve_plural so the text engine can render the whole set) resolves,
         for restriction purposes, to its FIRST item -- mirroring the runner's
         GetReference("ReferencedObjects"), which returns Items(0).  Per-item
         narrowing already happened in RefineMatchingPossibilitesUsingRestrictions;
         the final PassRestrictions only ever checks the first item. */
      const char *bar = strchr (bound, '|');
      if (bar != NULL)
        {
          char tmp[128];
          size_t n = (size_t) (bar - bound);
          if (n < sizeof tmp)
            {
              memcpy (tmp, bound, n);
              tmp[n] = '\0';
              const a5_object_t *o = a5model_object (st->adv, tmp);
              if (o != NULL) return o->key;   /* stable model-key pointer */
            }
        }
      return bound;
    }
  if (streq (k, "ReferencedCharacter"))
    return a5state_player_key (st);
  return k;
}

/* A string that is a plain (optionally signed) integer literal, no other bytes. */
static int
is_clean_int (const char *s)
{
  if (s == NULL || *s == '\0') return 0;
  if (*s == '-' || *s == '+') s++;
  if (*s == '\0') return 0;
  for (; *s; s++)
    if (!isdigit ((unsigned char) *s)) return 0;
  return 1;
}

/* Evaluate key2 as a number: a variable key's value, a `RAND(min,max)`
   expression, else an integer literal.  An expression RHS is wrapped in single
   quotes in the model (the runner's clsRestriction summary quotes it; the stored
   StringValue is the bare expression), and the runner's PassSingleRestriction evaluates
   it via EvaluateExpression -> clsVariable.SetToExpression, which calls
   Global.Random(min,max) for a RAND() token (clsUserSession.vb:4486).  So
   SixSilverBullets' TimeTrapsT `Roller Must BeEqualTo 'RAND(1,16)'` draws one
   random per evaluation -- a draw Scarier previously skipped (strtol of the
   quoted string yields 0, so the chime never fired and the RAND stream desynced
   from the runner, mistiming the sniper/bomb events). */
static long
num_value (a5_state_t *st, const char *k)
{
  int vi;
  const char *s = k;
  char unq[64];
  if (s != NULL && s[0] == '\'')
    {
      /* strip surrounding single quotes into a small scratch buffer */
      size_t len = strlen (s);
      if (len >= 2 && s[len - 1] == '\'' && len - 2 < sizeof unq)
        { memcpy (unq, s + 1, len - 2); unq[len - 2] = '\0'; s = unq; }
    }
  if (s != NULL && strncmp (s, "RAND", 4) == 0)
    {
      long lo = 0, hi = 0;
      char *end;
      const char *p = s + 4;
      while (*p && !(*p == '-' || (*p >= '0' && *p <= '9'))) p++;
      lo = strtol (p, &end, 10);
      p = end;
      while (*p && !(*p == '-' || (*p >= '0' && *p <= '9'))) p++;
      hi = (*p) ? strtol (p, NULL, 10) : lo;
      return a5rand_between (lo, hi);
    }
  /* A bare numeric restriction value is an integer LITERAL, never a variable
     key: the runner's FileIO.LoadRestrictions stores `IntValue = CInt(sElements(3))`
     when `sElements.Length = 4 AndAlso IsNumeric(sElements(3))`, and only takes
     the "key to a variable" branch for a NON-numeric token.  LostCoastlines has
     a Text variable literally keyed "511", so a variable-key lookup on the RHS
     of `Encounter Must BeEqualTo 511` wrongly read that (0-valued) variable and
     made every `Encounter == <n>` where a variable happens to be keyed <n> pass
     spuriously.  Test for the integer form before the variable lookup. */
  if (is_clean_int (s))
    return strtol (s, NULL, 10);
  vi = a5state_variable_index (st, s);
  if (vi >= 0)
    return st->var_num[vi];
  /* Not a RAND literal and not a bare variable key: the value may be an
     expression carrying %references% and/or arithmetic (e.g. Tingalan's
     `'%randbetween1and9%'` or `'%randbetween1and9%+%randbetween1and2%'`).  The runner
     resolves the RHS via EvaluateExpression (substitute %vars%, reduce); strtol
     alone left a leading `%` as 0, so an *unmet* survival restriction such as
     Dysentaryk's `Poopcounte Must BeGreaterThanOrEqualTo '%randbetween1and9%'`
     fired as 0>=0 -- inflicting a spurious `Wounds += 12` mortal wound.  When the
     value contains a %reference%, evaluate it through the same expression engine
     the action side uses (a5text_eval_expression) and read back the number.  The
     `%`-only trigger keeps every non-reference RHS byte-identical to before. */
  if (s != NULL && strchr (s, '%') != NULL)
    {
      char *ev = a5text_eval_expression (st, s);
      long e = strtol (ev, NULL, 10);
      free (ev);
      return e;
    }
  return strtol (s != NULL ? s : "0", NULL, 10);
}

/* clsCharacter.IsHoldingObject (clsCharacter.vb:895): an object counts as held
   by `charkey` (or ANYCHARACTER) when it is directly HeldByCharacter, OR is
   inside/on an object that is (recursively) held.  So the silver key inside the
   carried jewelry box is "held" and unlocks the door. */
static int
is_holding_object (a5_state_t *st, int oi, const char *charkey)
{
  /* Walk iteratively up the in/on chain, bounded by the object count: a walk
     longer than that means a containment cycle (A in B, B in A, or an object
     in itself, which a malformed game file can create), so bail rather than
     recurse forever. */
  int guard = 0;
  while (oi >= 0)
    {
      if (guard++ > st->adv->n_objects)
        return 0;
      switch (st->obj[oi].where)
        {
        case A5_OWHERE_HELD_BY:
          return streq (charkey, ANYCHARACTER)
                 || streq (st->obj[oi].key, charkey);
        case A5_OWHERE_IN_OBJECT:
        case A5_OWHERE_ON_OBJECT:
          oi = a5state_object_index (st, st->obj[oi].key);
          continue;
        default:
          return 0;
        }
    }
  return 0;
}

/* clsObject.IsInside / IsOn (clsObject.vb:255 / 240): an object is "inside"
   (resp. "on") k2 when it is directly in (on) it, or when it sits anywhere in
   the in/on parent chain whose link to k2 is of the wanted kind.  So the Venus
   inside the Borgen Vase inside the backpack IS inside the backpack -- Museum
   Heist's endgame loot tally scores nested treasure through exactly this
   restriction. */
static int
object_is_in_or_on (a5_state_t *st, int oi, const char *k2, a5_owhere_t want)
{
  /* Iterative bounded walk (see is_holding_object) so a containment cycle
     cannot overflow the stack. */
  int guard = 0;
  while (oi >= 0)
    {
      if (guard++ > st->adv->n_objects)
        return 0;
      if (st->obj[oi].where != A5_OWHERE_IN_OBJECT
          && st->obj[oi].where != A5_OWHERE_ON_OBJECT)
        return 0;
      if (st->obj[oi].where == want && streq (st->obj[oi].key, k2))
        return 1;
      oi = a5state_object_index (st, st->obj[oi].key);
    }
  return 0;
}

/* ------------------------------------------------------ object sub-evaluator */

/* The ANYOBJECT / NOOBJECT quantifier shared by pass_object's whole-table
   scans: true for ANYOBJECT when some object satisfies `pred`, for NOOBJECT
   when none does. */
template <typename F>
static int
any_no_object (a5_state_t *st, const char *k1, F pred)
{
  int i, any = 0;
  for (i = 0; i < st->adv->n_objects; i++)
    if (pred (i)) { any = 1; break; }
  return streq (k1, ANYOBJECT) ? any : !any;
}

static int
pass_object (a5_state_t *st, a5_restr_t *r)
{
  const char *k1 = resolve_key (st, r->key1);
  const char *k2 = resolve_key (st, r->key2);
  int oi = a5state_object_index (st, k1);

  if (streq (r->op, "BeAtLocation"))
    {
      if (streq (k1, NOOBJECT) || streq (k1, ANYOBJECT))
        return any_no_object (st, k1, [&] (int i)
                 { return a5state_object_at_location (st, i, k2, 0); });
      return a5state_object_at_location (st, oi, k2, 0);
    }
  if (streq (r->op, "BeOnObject") || streq (r->op, "BeInsideObject"))
    {
      a5_owhere_t want = streq (r->op, "BeOnObject")
                           ? A5_OWHERE_ON_OBJECT : A5_OWHERE_IN_OBJECT;
      if (streq (k1, ANYOBJECT) || streq (k1, NOOBJECT))
        return any_no_object (st, k1, [&] (int i)
                 { return st->obj[i].where == want
                          && streq (st->obj[i].key, k2); });
      if (oi < 0) return 0;
      if (streq (k2, ANYOBJECT))
        return st->obj[oi].where == want;
      /* Specific target: recursive through the in/on chain (clsObject.IsInside
         / IsOn), NOT a direct-parent compare. */
      return object_is_in_or_on (st, oi, k2, want);
    }
  if (streq (r->op, "BeHeldByCharacter") || streq (r->op, "BeWornByCharacter"))
    {
      a5_owhere_t want = streq (r->op, "BeHeldByCharacter")
                           ? A5_OWHERE_HELD_BY : A5_OWHERE_WORN_BY;
      if (streq (k1, ANYOBJECT) || streq (k1, NOOBJECT))
        return any_no_object (st, k1, [&] (int i)
                 { return st->obj[i].where == want
                          && (streq (k2, ANYCHARACTER)
                              || streq (st->obj[i].key, k2)); });
      if (oi < 0) return 0;
      /* BeHeldByCharacter recurses through held containers (the runner's
         IsHoldingObject); BeWornByCharacter is a direct check. */
      if (want == A5_OWHERE_HELD_BY)
        return is_holding_object (st, oi, k2);
      if (streq (k2, ANYCHARACTER))
        return st->obj[oi].where == want;
      return st->obj[oi].where == want && streq (st->obj[oi].key, k2);
    }
  if (streq (r->op, "BePartOfCharacter"))
    {
      /* clsUserSession.vb:4291: object's where == PartOfCharacter and its
         holder key == k2 (a specific character).  A Hidden/elsewhere object
         fails -- this gates MI/TBN's "handfire winks out" System task. */
      if (oi < 0) return 0;
      return st->obj[oi].where == A5_OWHERE_PART_CHAR && streq (st->obj[oi].key, k2);
    }
  if (streq (r->op, "BePartOfObject"))
    {
      /* clsUserSession.vb:4306: where == PartOfObject; k2 == ANYOBJECT checks
         the where alone, else also the parent object key. */
      if (oi < 0) return 0;
      if (streq (k2, ANYOBJECT))
        return st->obj[oi].where == A5_OWHERE_PART_OBJECT;
      return st->obj[oi].where == A5_OWHERE_PART_OBJECT && streq (st->obj[oi].key, k2);
    }
  if (streq (r->op, "BeHidden"))
    return oi >= 0 && st->obj[oi].where == A5_OWHERE_HIDDEN;
  if (streq (r->op, "Exist"))
    {
      if (streq (k1, NOOBJECT)) return st->adv->n_objects == 0;
      if (streq (k1, ANYOBJECT)) return st->adv->n_objects > 0;
      return oi >= 0;
    }
  if (streq (r->op, "HaveProperty"))
    {
      /* clsObject.HasProperty over the merged (static + runtime) table:
         a5state_entity_has_prop consults the SetProperty override layer
         (`<Unselected>` = removed, clsUserSession.vb:2058) before the model's
         static props -- SelectionOnly props such as Purchased are only ever
         added at runtime. */
      if (streq (k1, ANYOBJECT) || streq (k1, NOOBJECT))
        return any_no_object (st, k1, [&] (int i)
                 { return a5state_entity_has_prop (st, st->adv->objects[i].key,
                                                   k2); });
      return oi >= 0 && a5state_entity_has_prop (st, k1, k2);
    }
  if (streq (r->op, "BeInState"))
    {
      int i;
      if (oi < 0) return 0;
      /* Consult the runtime override layer so SetProperty (e.g. OpenStatus) is
         reflected; fall back to the model's static value.  Mirror the runner's
         BeInState (clsUserSession.vb:4253): only a StateList property whose
         AppendToProperty is empty counts -- an appended pseudo-state property
         (e.g. LockStatus, which appends "Locked" onto OpenStatus) is skipped,
         so unlocking (OpenStatus -> Closed) clears the "Locked" state even
         though the child LockStatus property still reads "Locked". */
      for (i = 0; i < st->adv->objects[oi].n_props; i++)
        {
          const char *pk = st->adv->objects[oi].props[i].key;
          const a5_propdef_t *pd = a5model_propdef (st->adv, pk);
          if (pd != NULL)
            {
              if (pd->type != NULL && !streq (pd->type, "StateList"))
                continue;
              if (pd->append_to != NULL && pd->append_to[0] != '\0')
                continue;
            }
          const char *val = a5state_entity_prop (st, k1, pk);
          if (streq (val, k2))
            return 1;
        }
      return 0;
    }
  if (streq (r->op, "BeInGroup"))
    {
      /* clsUserSession.vb:4193: ANYOBJECT / NOOBJECT quantify over the group's
         effective member count (static <Member>s plus runtime add/remove). */
      if (streq (k1, ANYOBJECT) || streq (k1, NOOBJECT))
        return any_no_object (st, k1, [&] (int i)
                 { return a5state_object_in_group (st, k2,
                                                   st->adv->objects[i].key); });
      return a5state_object_in_group (st, k2, k1);
    }
  if (streq (r->op, "BeExactText"))
    {
      /* True if the player's typed reference text equals k2 (e.g. "All"). */
      char alias[64];
      const char *typed;
      snprintf (alias, sizeof alias, "%s$text", r->key1);
      typed = a5state_lookup_ref (st, alias);
      if (typed == NULL)
        return 0;
      while (*typed && *k2 && tolower ((unsigned char) *typed) == tolower ((unsigned char) *k2))
        { typed++; k2++; }
      return *typed == '\0' && *k2 == '\0';
    }
  if (streq (r->op, "BeObject"))
    return streq (k1, k2);
  if (streq (r->op, "BeVisibleToCharacter"))
    {
      /* The runner's CanSeeObject (clsCharacter.vb:772) compares BoundVisible keys, so
         an object inside a closed opaque container is not visible.
         clsUserSession.vb:4338: ANYOBJECT = the observer sees at least one
         object, NOOBJECT = the observer sees none. */
      int ci = a5state_character_index (st, k2);
      const char *cloc = a5state_character_location_key (st, ci);
      if (streq (k1, ANYOBJECT) || streq (k1, NOOBJECT))
        return any_no_object (st, k1, [&] (int i)
                 { return cloc != NULL
                          && a5state_object_visible_at_location (st, i, cloc, 0); });
      return oi >= 0 && cloc != NULL
             && a5state_object_visible_at_location (st, oi, cloc, 0);
    }
  if (streq (r->op, "HaveBeenSeenByCharacter"))
    {
      /* Player-centric seen set (clsCharacter.HasSeenObject); a non-player
         observer falls back to "seen" so its tasks aren't over-suppressed. */
      if (k2 != NULL && !streq (k2, a5state_player_key (st)))
        return 1;
      return oi >= 0 && st->obj_seen != NULL && st->obj_seen[oi];
    }
  return 1;                   /* unknown operator: don't suppress text */
}

/* --------------------------------------------------- location sub-evaluator */

static int
pass_location (a5_state_t *st, a5_restr_t *r)
{
  const char *k1 = r->key1;
  const char *here = a5state_player_location (st);
  /* "ReferencedLocation" (a %location% command reference, e.g. The Salvage's
     `# %location%` group fan-out tasks) resolves through the per-turn bindings
     exactly like ReferencedObject does in pass_object; a literal location key
     passes through resolve_key untouched. */
  const char *loc = streq (k1, PLAYERLOCATION) ? here : resolve_key (st, k1);

  if (streq (r->op, "BeLocation"))
    return streq (loc, r->key2);
  if (streq (r->op, "BeInGroup"))
    /* Effective membership = runtime override (AddLocationToGroup /
       RemoveLocationFromGroup, e.g. Tingalan's Search tasks stamping the current
       location into SearchedLo, or Jacaranda's dusk/dawn DarkLocations toggles)
       layered over the static <Member> list -- exactly what a5state_object_in_group
       computes and what the object BeInGroup handler already uses.  The old
       static-only scan here missed every runtime AddLocationToGroup, so e.g.
       `Location MustNot BeInGroup SearchedLo` never blocked a re-search. */
    return loc != NULL && a5state_object_in_group (st, r->key2, loc);
  if (streq (r->op, "HaveProperty"))
    {
      /* Through the state accessor so SetProperty overrides AND group-inherited
         location properties (clsItem.htblInheritedProperties, e.g. The Salvage's
         "Planet Tiles" group carrying daz7LocationIs1) both count. */
      if (loc == NULL || a5model_location (st->adv, loc) == NULL) return 0;
      return a5state_entity_has_prop (st, loc, r->key2);
    }
  if (streq (r->op, "Exist"))
    return 1;
  if (streq (r->op, "HaveBeenSeenByCharacter"))
    {
      /* clsLocation.SeenBy(char) == Character.HasSeenLocation(loc).  In the runner that
         flag is set in exactly one place -- clsUserSession.vb:222, for the
         PLAYER only, on every player move -- so an NPC observer has NEVER seen
         any location and the restriction is always False for it.  (AoS gates the
         Flour Store `Up` exit on `cl_Mission1 HaveBeenSeenByCharacter Finnjart`,
         which stays blocked until the game explicitly advances it.)  Mirror that:
         the player consults the seen set; any other character is unseen. */
      const char *ch = resolve_key (st, r->key2);
      int li;
      if (ch != NULL && ch[0] != '\0' && !streq (ch, a5state_player_key (st)))
        return 0;
      li = a5state_location_index (st, loc);
      return li >= 0 && st->loc_seen != NULL && st->loc_seen[li];
    }
  return 1;
}

/* ------------------------------------------------------------ exit lookup */

/* Per-turn route memo, the Adrift 5 runner's clsCharacter.dictHasRouteCache /
   dictRouteErrors: each character's route-restriction evaluation for a
   (location, direction) is performed at most once per turn, and every later
   check that turn reuses the verdict even if an action has since changed the
   gating state.  TTP's security door depends on it: `e`'s PlayerMovement
   restriction passes while the door is open, the BeforeActionsOnly override
   `s_GoToTheEas` then slams it shut, and the MoveCharacter action still moves
   the player on the cached pass.  Cleared once per processed command
   (PrepareForNextTurn, vb:3792); transient, never saved. */
struct a5_route_entry {
  const char *dest;                /* scan result (model-owned key or NULL)  */
  const a5_xml_node_t *blocked;    /* blocking exit-restriction's <Message>  */
};
typedef std::map<std::string, a5_route_entry> a5_route_cache_t;

static a5_route_cache_t *
route_cache (a5_state_t *st)
{
  if (st->route_cache == NULL)
    st->route_cache = new a5_route_cache_t;
  return (a5_route_cache_t *) st->route_cache;
}

void
a5restr_route_cache_clear (a5_state_t *st)
{
  if (st->route_cache != NULL)
    ((a5_route_cache_t *) st->route_cache)->clear ();
}

void
a5restr_route_cache_free (a5_state_t *st)
{
  delete (a5_route_cache_t *) st->route_cache;
  st->route_cache = NULL;
}

/* The session ledger behind clsDirection.bEverBeenBlocked: every failed route
   evaluation records its (location, direction) here, whoever asked -- a
   movement attempt, a HaveRouteInDirection restriction, %Exits%, or the map
   painting itself, exactly the callers of the runner's HasRouteInDirection.
   Unlike the route memo it is never cleared: the runner's flag stays set for
   the rest of the session (and, like it, is not saved). */
typedef std::set<std::string> a5_blocked_ledger_t;

static std::string
blocked_ledger_key (const char *lockey, const char *dir)
{
  return std::string (lockey) + '\001' + dir;
}

static void
blocked_ledger_add (a5_state_t *st, const char *lockey, const char *dir)
{
  if (st->ever_blocked == NULL)
    st->ever_blocked = new a5_blocked_ledger_t;
  ((a5_blocked_ledger_t *) st->ever_blocked)
    ->insert (blocked_ledger_key (lockey, dir));
}

int
a5restr_ever_blocked (a5_state_t *st, const char *lockey, const char *dir)
{
  if (st->ever_blocked == NULL || lockey == NULL || dir == NULL)
    return 0;
  return ((a5_blocked_ledger_t *) st->ever_blocked)
    ->count (blocked_ledger_key (lockey, dir)) != 0;
}

void
a5restr_ever_blocked_free (a5_state_t *st)
{
  delete (a5_blocked_ledger_t *) st->ever_blocked;
  st->ever_blocked = NULL;
}

const char *
a5restr_exit_in_direction (a5_state_t *st, const char *charkey,
                           const char *lockey, const char *dir,
                           const a5_xml_node_t **blocked_msg)
{
  const a5_location_t *l;
  const a5_xml_node_t *c;
  const a5_xml_node_t *bl = NULL;
  const char *result = NULL;
  a5_route_cache_t *rc = NULL;
  std::string ck;
  if (lockey == NULL || dir == NULL)
    return NULL;
  if (charkey != NULL)
    {
      rc = route_cache (st);
      ck.assign (charkey).append (1, '\001')
        .append (lockey).append (1, '\001').append (dir);
      a5_route_cache_t::const_iterator it = rc->find (ck);
      if (it != rc->end ())
        {
          if (blocked_msg != NULL && it->second.blocked != NULL)
            *blocked_msg = it->second.blocked;
          return it->second.dest;
        }
    }
  l = a5model_location (st->adv, lockey);
  if (l == NULL)
    return NULL;
  /* A location may carry more than one <Movement> for a direction; a route
     blocked by its own restrictions is skipped and the scan resumes at the
     next candidate, so this walks a5model_movement rather than taking its
     first hit. */
  for (c = a5model_movement (l, NULL, dir); c != NULL;
       c = a5model_movement (l, c, dir))
    {
      const char *dest;
      const a5_xml_node_t *mr;
      mr = a5xml_child (c, "Restrictions");
      if (mr != NULL && !a5restr_pass (st, mr))
        {                               /* route blocked by its restriction */
          /* Capture the blocking message unconditionally (the runner stores
             sErrorMessage in dictRouteErrors on every miss), so a same-turn
             re-check that DOES want it replays it without re-evaluating. */
          bl = a5restr_fail_message (st, mr);
          if (blocked_msg != NULL)
            *blocked_msg = bl;
          /* d.bEverBeenBlocked = True (clsCharacter.vb:686). */
          blocked_ledger_add (st, lockey, dir);
          continue;
        }
      dest = a5xml_child_text (c, "Destination");
      result = (dest != NULL && dest[0] != '\0') ? dest : NULL;
      break;
    }
  if (rc != NULL)
    {
      a5_route_entry e;
      e.dest = result;
      e.blocked = bl;
      (*rc)[ck] = e;
    }
  return result;
}

/* -------------------------------------------------- character sub-evaluator */

/* The OTHER characters sharing `ci`'s resolved room, capped at 2 (enough to
   distinguish alone / exactly-one / crowd).  Compares resolved
   Location.LocationKey values, so a character seated on / inside furniture
   counts as present (Lost Children's Anne rocks in her chair while the player
   says hello).  When the count is exactly 1, *other receives that
   character's key. */
static int
chars_sharing_room (a5_state_t *st, int ci, const char **other)
{
  const char *cloc = (ci >= 0) ? a5state_character_location_key (st, ci) : NULL;
  int count = 0, i;
  if (other != NULL)
    *other = NULL;
  if (cloc == NULL)
    return 0;
  for (i = 0; i < st->adv->n_characters && count <= 1; i++)
    {
      const char *oloc;
      if (i == ci)
        continue;
      oloc = a5state_character_location_key (st, i);
      if (oloc != NULL && streq (oloc, cloc))
        {
          count++;
          if (other != NULL)
            *other = st->adv->characters[i].key;
        }
    }
  return count;
}

/* AloneWithChar (clsCharacter.AloneWithChar): the single other character in the
   same location as `charkey`, or NULL if there are zero or more than one. */
static const char *
alone_with_char (a5_state_t *st, const char *charkey)
{
  const char *other;
  int ci = a5state_character_index (st, charkey);
  return (chars_sharing_room (st, ci, &other) == 1) ? other : NULL;
}

/* Does character `charkey` hold (where==HELD_BY) or wear (WORN_BY) any object? */
static int
char_holds_any (a5_state_t *st, const char *charkey, a5_owhere_t where)
{
  int i;
  for (i = 0; i < st->adv->n_objects; i++)
    if (st->obj[i].where == where && streq (st->obj[i].key, charkey))
      return 1;
  return 0;
}

/* clsCharacter.IsHoldingObject(sObKey): true if CHARKEY holds OBJKEY, counting
   objects nested inside containers/on supporters the character (transitively)
   holds -- InObject/OnObject recurse on the parent, HeldByCharacter terminates
   (clsCharacter.vb:895).  WornByCharacter does NOT count as held, so the
   recursion stops (returns 0) at a worn parent.  bDirectly disables the recursion
   (the directly-held-only check). */
static int
char_holds_object (a5_state_t *st, const char *charkey, const char *objkey,
                   int directly)
{
  /* Iterative bounded walk (see is_holding_object) so a containment cycle
     cannot overflow the stack. */
  int guard = 0;
  const char *key = objkey;
  while (key != NULL)
    {
      int oi = a5state_object_index (st, key);
      if (oi < 0 || guard++ > st->adv->n_objects)
        return 0;
      a5_owhere_t w = st->obj[oi].where;
      const char *k = st->obj[oi].key;
      if (w == A5_OWHERE_HELD_BY)
        return k != NULL && (streq (k, charkey)
                             || (streq (k, "%Player%")
                                 && streq (charkey, a5state_player_key (st))));
      if (!directly && (w == A5_OWHERE_IN_OBJECT || w == A5_OWHERE_ON_OBJECT))
        {
          key = k;
          continue;
        }
      return 0;
    }
  return 0;
}

static int
pass_character (a5_state_t *st, a5_restr_t *r)
{
  const char *k1 = resolve_key (st, r->key1);
  const char *k2 = resolve_key (st, r->key2);
  int ci = a5state_character_index (st, k1);
  const char *cloc = (ci >= 0) ? st->char_loc[ci] : NULL;

  if (streq (r->op, "BeAtLocation"))
    {
      /* clsRestriction.CharacterEnum.BeAtLocation (clsUserSession.vb:4571):
         ANYCHARACTER = true when ANY character's Location.LocationKey equals
         k2 (DieFeuerfaust's look-through-flap segment is gated on
         `AnyCharacter Must BeAtLocation ChieftainS` after the chieftain and
         his wife are bulk-moved into the main chamber; this case previously
         fell through to the ci<0 NULL compare and always failed).  Both
         branches compare the runner's *resolved* LocationKey -- the root room through
         an on/in-object carrier (clsCharacter.vb:1773) -- not the raw
         char_loc, which is NULL for a character seated on furniture. */
      if (streq (k1, ANYCHARACTER))
        {
          for (int s = 0; s < st->adv->n_characters; s++)
            if (streq (a5state_character_location_key (st, s), k2))
              return 1;
          return 0;
        }
      return streq (a5state_character_location_key (st, ci), k2);
    }
  if (streq (r->op, "BeOfGender"))
    {
      /* clsCharacter.Gender / clsRestriction.CharacterEnum.BeOfGender: an
         exact match against the Mandatory StateList "Gender" property
         (Male/Female/Unknown).  k2 is the literal state name (resolve_key is
         a no-op on it -- no leading '%' and no matching per-turn ref
         binding).  Without this case the operator fell through to the
         best-effort `return 1`, so a gender-gated branch (e.g. Pathway to
         Destruction's
         `Player Must BeOfGender Female` swapping in an alternate character
         name/pronoun set) always fired even when the player's Gender is still
         its unset "Unknown" default.  Pathway's Gender never gets set at all:
         the generated GenTask1 that would ask via %PopUpChoice% is a System
         task WITHOUT RunImmediately, and nothing ever dispatches one of those
         (clsUserSession.vb:213 auto-runs only the RunImmediately ones), so the
         Unknown default is what the runner plays with too. */
      const char *g = a5state_entity_prop (st, k1, "Gender");
      return g != NULL && strcasecmp (g, k2) == 0;
    }
  if (streq (r->op, "BeHoldingObject"))
    {
      /* ANYOBJECT -> holding at least one object (clsCharacter.IsHoldingObject:
         "If sObKey = ANYOBJECT Then Return HeldObjects.Count > 0"). */
      if (streq (k2, ANYOBJECT))
        return char_holds_any (st, k1, A5_OWHERE_HELD_BY);
      /* ANYCHARACTER subject: htblObjects(k2).IsHeldByAnyone
         (clsUserSession.vb:4674) -- held directly, or nested in a
         container/surface whose own chain ends held (clsObject.vb:737
         recurses into the parent).  A worn or located object is not "held
         by anyone". */
      if (streq (k1, ANYCHARACTER))
        {
          int oi = a5state_object_index (st, k2), hops;
          for (hops = 0; oi >= 0 && hops < st->adv->n_objects; hops++)
            {
              if (st->obj[oi].where == A5_OWHERE_HELD_BY)
                return 1;
              if (st->obj[oi].where != A5_OWHERE_IN_OBJECT
                  && st->obj[oi].where != A5_OWHERE_ON_OBJECT)
                return 0;
              oi = a5state_object_index (st, st->obj[oi].key);
            }
          return 0;
        }
      /* A specific object counts as held if it is directly held OR nested in a
         container/supporter the character holds (clsCharacter.IsHoldingObject is
         recursive) -- e.g. Anno's gunpowder poured into the held skull still
         satisfies "Player Must BeHoldingObject Gunpowder1". */
      return char_holds_object (st, k1, k2, 0);
    }
  if (streq (r->op, "BeWearingObject"))
    {
      if (streq (k2, ANYOBJECT))     /* WornObjects.Count > 0 */
        return char_holds_any (st, k1, A5_OWHERE_WORN_BY);
      int oi = a5state_object_index (st, k2);
      /* ANYCHARACTER subject: htblObjects(k2).IsWornByAnyone
         (clsUserSession.vb:4868) -- a direct WornBy check only, NOT
         recursive (clsObject.vb:756). */
      if (streq (k1, ANYCHARACTER))
        return oi >= 0 && st->obj[oi].where == A5_OWHERE_WORN_BY;
      return oi >= 0 && st->obj[oi].where == A5_OWHERE_WORN_BY
             && streq (st->obj[oi].key, k1);
    }
  if (streq (r->op, "BeInSameLocationAsObject"))
    {
      int oi;
      if (cloc == NULL)
        return 0;
      /* k2 may be an object GROUP (e.g. LightSources): true if any member is in
         scope, mirroring clsCharacter.CanSeeObject's group expansion (the
         darkness override's "MustNot BeInSameLocationAsObject LightSources"
         must see a held torch). */
      for (int gi = 0; gi < st->adv->n_groups; gi++)
        if (streq (st->adv->groups[gi].key, k2))
          {
            /* Effective membership = static <Member>s plus runtime adds minus
               removes; scan every object so runtime-added members (e.g. a lit
               flashlight in LightSources) count too. */
            for (int oi = 0; oi < st->adv->n_objects; oi++)
              if (a5state_object_in_group (st, k2, st->adv->objects[oi].key)
                  && a5state_object_visible_at_location (st, oi, cloc, 0))
                return 1;
            return 0;
          }
      oi = a5state_object_index (st, k2);
      return oi >= 0 && a5state_object_visible_at_location (st, oi, cloc, 0);
    }
  if (streq (r->op, "BeInSameLocationAsCharacter"))
    {
      /* clsUserSession.vb:4681-4698.  clsCharacter.Location.LocationKey resolves a
         seated/carried character to its room, so the comparison must walk an
         on/in-object character (e.g. Anne seated on Chair1) through its carrier
         rather than read the raw char_loc (NULL for a non-"At Location"
         character).  `same_room` does that resolution for either operand (prefer
         the recorded room, else scan locations for the on/in-object case). */
      auto same_room = [&] (int a, int b) -> int {
        if (a < 0 || b < 0)
          return 0;
        const char *la = (st->char_loc != NULL) ? st->char_loc[a] : NULL;
        if (la != NULL)
          return a5state_character_at_location (st, b, la);
        for (int L = 0; L < st->adv->n_locations; L++)
          { const char *lk = st->adv->locations[L].key;
            if (a5state_character_at_location (st, a, lk)
                && a5state_character_at_location (st, b, lk))
              return 1; }
        return 0;
      };
      int c2 = a5state_character_index (st, k2);
      /* k1 = ANYCHARACTER: r = Not htblCharacters(k2).IsAlone -- some *other*
         character shares k2's room (vb:4684). */
      if (streq (k1, ANYCHARACTER))
        {
          if (c2 < 0)
            return 0;
          for (int s = 0; s < st->adv->n_characters; s++)
            if (s != c2 && same_room (s, c2))
              return 1;
          return 0;
        }
      /* k1 = specific, k2 = ANYCHARACTER: any *other* character co-located with
         k1 (vb:4687-4694) -- e.g. Revenge's `Player Must
         BeInSameLocationAsCharacter AnyCharacter` ("There's nobody here to show
         anything to!"), true when the customs official is present. */
      if (streq (k2, ANYCHARACTER))
        {
          if (ci < 0)
            return 0;
          for (int c = 0; c < st->adv->n_characters; c++)
            if (c != ci && same_room (c, ci))
              return 1;
          return 0;
        }
      /* k1, k2 both specific: identical rooms (vb:4696). */
      return same_room (ci, c2);
    }
  if (streq (r->op, "BeCharacter"))
    {
      /* clsRestriction.CharacterEnum.BeCharacter (clsUserSession.vb:4579): the
         subject character k1 is a specific character (ANYCHARACTER = any) -- an
         identity test on the resolved keys (mirrors the object `BeObject`).  Without
         this case the operator fell through to the best-effort `return 1`, so e.g.
         RunBronwynn's `ExamineCha1` ("Examine Me", `ReferencedCharacter Must
         BeCharacter Player`) wrongly fired for any examined character (x horse ->
         Fleetwind), showing its description where the runner's higher-priority
         ExamineCharacter fails the HaveSeen check with "You see no such thing." */
      if (streq (k1, ANYCHARACTER))
        return 1;
      return streq (k1, k2);
    }
  if (streq (r->op, "BeVisibleToCharacter"))
    {
      /* clsRestriction.CharacterEnum.BeVisibleToCharacter (clsUserSession.vb:4700):
         true when the subject character k1 (ANYCHARACTER = any) can be seen by the
         observer k2 -- clsCharacter.CanSeeCharacter -> IsVisibleTo: the two share a
         BoundVisible location and the subject is not Hidden.  Reduce to "the subject
         is at the observer's location" (a5state_character_at_location resolves an
         on/in-object subject through its carrier and treats Hidden as not-present).
         Without this case the operator fell through to the best-effort `return 1`,
         so e.g. Grandpa's `vnl_JustTalk` ("talk" near Molly) wrongly fired when
         Molly was elsewhere, swallowing the turn. */
      const char *obs_loc;
      if (streq (k2, ANYCHARACTER))
        {
          /* any observer: the subject (or any subject) co-located with any *other*
             character. */
          for (int s = 0; s < st->adv->n_characters; s++)
            {
              if (!streq (k1, ANYCHARACTER) && !streq (st->adv->characters[s].key, k1))
                continue;
              for (int o = 0; o < st->adv->n_characters; o++)
                if (o != s && st->char_loc[o] != NULL
                    && a5state_character_at_location (st, s, st->char_loc[o]))
                  return 1;
            }
          return 0;
        }
      if (k2 == NULL || streq (k2, a5state_player_key (st)))
        obs_loc = a5state_player_location (st);
      else
        /* effective location: an observer seated ON furniture (Grandpa on
           his rocking chair) has char_loc NULL. */
        obs_loc = a5state_character_location_key (st,
                    a5state_character_index (st, k2));
      if (obs_loc == NULL)
        return 0;
      if (streq (k1, ANYCHARACTER))
        {
          for (int s = 0; s < st->adv->n_characters; s++)
            if (a5state_character_at_location (st, s, obs_loc))
              return 1;
          return 0;
        }
      return a5state_character_at_location (st, ci, obs_loc);
    }
  if (streq (r->op, "BeWithinLocationGroup"))
    {
      /* Membership must consult the runtime override layer, not just the static
         model members: SixSilverBullets' TimeTraps1 fires the bell then runs
         `RemoveLocationFromGroup LocationOf %Player% FromGroup TimeTraps`, so a
         once-tolled location leaves the group and must not re-toll.  Likewise
         AddLocationToGroup (Jacaranda's dark-locations) must count. */
      return cloc != NULL && a5state_object_in_group (st, k2, cloc);
    }
  if (streq (r->op, "HaveProperty"))
    {
      /* clsCharacter.HasProperty (clsItem.vb:386) consults the *merged*
         property table, so a property added at runtime via SetProperty
         (e.g. `SetProperty Susan Known <Selected>`, clsUserSession.vb:2042)
         must count -- not just the static model props.  The runner's
         SetPropertyValue(Boolean) removes a SelectionOnly property when set
         to <Unselected>, so an override carrying that sentinel reads as
         absent (mirroring the display-name "Selected" check at a5text.cpp). */
      if (streq (k1, ANYCHARACTER))
        {
          for (int s = 0; s < st->adv->n_characters; s++)
            if (a5state_entity_has_prop (st, st->adv->characters[s].key, k2))
              return 1;
          return 0;
        }
      return ci >= 0 && a5state_entity_has_prop (st, k1, k2);
    }
  if (streq (r->op, "Exist"))
    {
      /* clsUserSession.vb:4600: ANYCHARACTER -> any character defined at all. */
      if (streq (k1, ANYCHARACTER))
        return st->adv->n_characters > 0;
      return ci >= 0;
    }
  if (streq (r->op, "BeInGroup"))
    {
      /* clsUserSession.vb:4779: ANYCHARACTER -> any character among the
         group's LIVE members (runtime Add/RemoveCharacterToGroup counts);
         else membership of k1 itself.  Previously unhandled, so the check
         fell through to the pass-everything default and a MustNot BeInGroup
         restriction could never fail. */
      if (streq (k1, ANYCHARACTER))
        {
          for (int s = 0; s < st->adv->n_characters; s++)
            if (a5state_object_in_group (st, k2, st->adv->characters[s].key))
              return 1;
          return 0;
        }
      return a5state_object_in_group (st, k2, k1);
    }
  if (streq (r->op, "HaveRouteInDirection"))
    {
      const char *dir = a5parse_canonical_direction (k2);
      const char *cl = streq (k1, a5state_player_key (st))
                         ? a5state_player_location (st) : cloc;
      const a5_xml_node_t *blocked = NULL;
      const char *exit;
      if (dir == NULL)
        dir = k2;             /* already canonical (e.g. bound ReferencedDirection) */
      /* sRouteError: clear, then capture the blocked exit's own message (if the
         exit exists but is restriction-gated) so the deciding fail message
         override below can prefer it over the generic "no route" text. */
      st->route_error = NULL;
      if (streq (dir, ANYDIRECTION))
        {
          /* clsUserSession.vb:4608: AnyDirection loops every DirectionsEnum
             value through HasRouteInDirection -- true when at least one
             direction has an open route (enum order, matching the runner's
             short-circuit). */
          static const char *const kEnumDirs[12] = {
            "North", "East", "South", "West", "Up", "Down",
            "In", "Out", "NorthEast", "SouthEast", "SouthWest", "NorthWest" };
          if (cl == NULL)
            return 0;
          for (int d = 0; d < 12; d++)
            {
              const a5_xml_node_t *b = NULL;
              if (a5restr_exit_in_direction (st, k1, cl, kEnumDirs[d], &b) != NULL)
                return 1;
              if (b != NULL)
                blocked = b;    /* the runner keeps the LAST specific error */
            }
          if (blocked != NULL)
            st->route_error = blocked;
          return 0;
        }
      exit = (cl != NULL) ? a5restr_exit_in_direction (st, k1, cl, dir, &blocked)
                          : NULL;
      if (exit == NULL && blocked != NULL)
        st->route_error = blocked;
      return exit != NULL;
    }
  if (streq (r->op, "BeInPosition"))
    {
      /* clsUserSession.vb:4902-4915 reads the raw CharacterPosition PROPERTY,
         gated on HasProperty -- NOT the Position getter (which defaults to
         Standing).  A character that never gained the property tests False
         for every position: the library-injected default Player carries no
         CharacterPosition <Property>, and the SetProperty action on an
         absent character property is a no-op (vb:2056), so `stand` always
         passes StandOnFloor's `MustNot BeInPosition Standing` for it.  Only
         a character whose XML sets the property, or one moved via
         ToStandingOn/ToSittingOn/ToLyingOn (the Position setter creates the
         property, clsCharacter.vb:1853), ever tests True. */
      if (streq (k1, ANYCHARACTER))
        {
          for (int s = 0; s < st->adv->n_characters; s++)
            {
              const char *pos = a5state_entity_prop
                  (st, st->adv->characters[s].key, "CharacterPosition");
              if (pos != NULL && streq (pos, k2))
                return 1;
            }
          return 0;
        }
      const char *pos = (ci >= 0)
          ? a5state_entity_prop (st, k1, "CharacterPosition") : NULL;
      return pos != NULL && streq (pos, k2);
    }
  /* On/in object, optionally constrained by stance.  Mirrors clsUserSession's
     BeOnObject / BeInsideObject / Be{Standing,Sitting,Lying}OnObject cases:
     ANYOBJECT -> just the ExistWhere; THEFLOOR / else -> object-key compare. */
  if (streq (r->op, "BeOnObject") || streq (r->op, "BeInsideObject")
      || streq (r->op, "BeStandingOnObject") || streq (r->op, "BeSittingOnObject")
      || streq (r->op, "BeLyingOnObject"))
    {
      const char *onobj = (ci >= 0 && st->char_onobj) ? st->char_onobj[ci] : NULL;
      int inside = (ci >= 0 && st->char_in) ? st->char_in[ci] : 0;
      const char *want_pos =
          streq (r->op, "BeStandingOnObject") ? "Standing"
        : streq (r->op, "BeSittingOnObject")  ? "Sitting"
        : streq (r->op, "BeLyingOnObject")    ? "Lying" : NULL;
      int want_in = streq (r->op, "BeInsideObject");
      const char *pos = (ci >= 0 && st->char_position) ? st->char_position[ci] : NULL;
      /* Subject ANYCHARACTER: the runner's clsUserSession CharacterOnObject/InObject
         AnyCharacter case counts the object's ChildrenCharacters (vb:4930/
         4952) -- true when ANY character is on/in the (specific) object.  Head
         Case's examine template gates %ListCharactersOnAndIn[%object%]% on
         "AnyCharacter Must BeInsideObject ReferencedObject" (the Mysterious
         Voice sitting inside the black van). */
      if (streq (k1, ANYCHARACTER))
        {
          if (streq (k2, ANYOBJECT))
            return 0;                     /* not a shipped combination */
          if (streq (k2, THEFLOOR))
            {
              /* clsUserSession.vb:4834: TheFloor = the stance with ExistWhere
                 AtLocation -- on the ground, not on/in any object. */
              for (int i = 0; i < st->adv->n_characters; i++)
                {
                  if (want_in)
                    continue;             /* can't be inside the floor */
                  if (st->char_onobj != NULL && st->char_onobj[i] != NULL)
                    continue;
                  if (st->char_loc == NULL || st->char_loc[i] == NULL)
                    continue;
                  if (want_pos != NULL
                      && !streq (st->char_position ? st->char_position[i] : NULL,
                                 want_pos))
                    continue;
                  return 1;
                }
              return 0;
            }
          for (int i = 0; i < st->adv->n_characters; i++)
            {
              if (st->char_onobj == NULL || st->char_onobj[i] == NULL
                  || !streq (st->char_onobj[i], k2))
                continue;
              int in_i = st->char_in != NULL && st->char_in[i];
              if ((in_i ? 1 : 0) != want_in)
                continue;
              if (want_pos != NULL
                  && !streq (st->char_position ? st->char_position[i] : NULL,
                             want_pos))
                continue;
              return 1;
            }
          return 0;
        }
      if (ci < 0) return 0;
      if (want_pos != NULL && !streq (pos, want_pos))
        return 0;
      if (streq (k2, ANYOBJECT))
        return onobj != NULL && (inside ? 1 : 0) == want_in;
      if (streq (k2, THEFLOOR))
        {
          /* clsUserSession.vb:4834: TheFloor = the stance with ExistWhere
             AtLocation -- on the ground, not on/in any object (the position
             itself was already checked above).  The player's room comes from
             a5state_player_location; char_loc is the NPC store. */
          const char *atloc = streq (k1, a5state_player_key (st))
                                ? a5state_player_location (st) : cloc;
          return !want_in && onobj == NULL && atloc != NULL;
        }
      /* Any concrete object key: must be on/in that key. */
      return onobj != NULL && (inside ? 1 : 0) == want_in && streq (onobj, k2);
    }
  /* Conversation / acquaintance operators (clsUserSession character cases). */
  if (streq (r->op, "BeInConversationWith"))
    {
      if (streq (k2, ANYCHARACTER))
        return st->conv_char != NULL && st->conv_char[0] != '\0';
      return streq (st->conv_char, k2);
    }
  if (streq (r->op, "HaveSeenCharacter"))
    {
      /* key1 is the observer (typically Player), key2 the target.  We track the
         player's "seen" set; a non-player observer is treated as having seen
         (best-effort, no shipped restriction needs the general case). */
      int c2 = a5state_character_index (st, k2);
      if (c2 < 0)
        return 0;
      if (!streq (k1, a5state_player_key (st)))
        return 1;
      return st->char_seen != NULL && st->char_seen[c2];
    }
  if (streq (r->op, "HaveSeenObject"))
    {
      /* key1 is the observer, key2 the target object.  The runner funnels this and the
         object-subject HaveBeenSeenByCharacter to the same per-character
         clsCharacter.HasSeenObject table (clsUserSession.vb:4677 / :4341), so
         mirror the object-side handler exactly: the player consults the tracked
         seen set; a non-player observer falls back to "seen" (best-effort).
         Without this case the operator fell through to `return 1`, so Thy
         Dunjohnman's gated exits (`%Player% Must HaveSeenObject Platform` /
         Coins) were open from turn one (the runner keeps them shut). */
      int o2 = a5state_object_index (st, k2);
      if (o2 < 0)
        return 0;
      if (!streq (k1, a5state_player_key (st)))
        return 1;
      return st->obj_seen != NULL && st->obj_seen[o2];
    }
  if (streq (r->op, "HaveSeenLocation"))
    {
      /* key1 is the observer (typically Player), key2 the target location.
         the runner sets clsCharacter.HasSeenLocation only for the Player
         (clsUserSession.vb:222, on every player move), so a non-player observer
         has never seen any location -- mirror the location-subject
         HaveBeenSeenByCharacter handler.  Without this case the operator fell
         through to the best-effort `return 1`, so RunBronwynn's Caught121
         (`Player Must HaveSeenLocation BridgetSLi`) fired the moment the player
         reached the cathedral-square street -- BEFORE ever entering Bridget's
         house -- ending the game prematurely (the runner plays on). */
      int li = a5state_location_index (st, k2);
      if (li < 0)
        return 0;
      if (!streq (k1, a5state_player_key (st)))
        return 0;
      return st->loc_seen != NULL && st->loc_seen[li];
    }
  if (streq (r->op, "BeAloneWith"))
    {
      const char *aw = alone_with_char (st, k1);
      if (streq (k2, ANYCHARACTER))
        return aw != NULL;
      return streq (aw, k2);
    }
  if (streq (r->op, "BeAlone"))
    /* clsCharacter.IsAlone: resolved Location.LocationKey compare, so a
       character on/in furniture still breaks solitude. */
    return chars_sharing_room (st, ci, NULL) == 0;
  if (streq (r->op, "BeOnCharacter"))
    {
      /* clsCharacterLocation.ExistsWhereEnum.OnCharacter: the character rides
         *another character*.  The carrier lives in char_onchar (set by the
         loader for an "On Character" start state and by the OntoCharacter
         MoveCharacter action) -- char_onobj only ever names objects, so reading
         it here always failed (Penrhyn's Arkell-on-Ralph follow message). */
      const char *onk = (ci >= 0 && st->char_onchar) ? st->char_onchar[ci] : NULL;
      if (onk == NULL || a5state_character_index (st, onk) < 0)
        return 0;
      if (streq (k2, ANYCHARACTER))
        return 1;
      return streq (onk, k2);
    }
  return 1;                   /* other character ops: best-effort pass */
}

/* --------------------------------------------------- variable sub-evaluator */

/* A restriction's comparison value as a string (FileIO.LoadRestrictions +
   EvaluateExpression): a quoted literal is de-quoted then expression-evaluated
   (so `"" the ""` -> `" the "` -> ` the `); a bare token naming a variable
   yields that variable's value; otherwise the token is expression-evaluated. */
static char *
restr_text_value (a5_state_t *st, const char *key2)
{
  size_t n;
  if (key2 == NULL)
    return strdup ("");
  n = strlen (key2);
  if (n >= 2 && ((key2[0] == '"' && key2[n - 1] == '"')
              || (key2[0] == '\'' && key2[n - 1] == '\'')))
    {
      char *inner = strndup (key2 + 1, n - 2);
      char *v = a5text_eval_expression (st, inner);
      free (inner);
      return v;
    }
  {
    int vj = a5state_variable_index (st, key2);
    if (vj >= 0 && st->var_text[vj] != NULL)
      return strdup (st->var_text[vj]);
  }
  return a5text_eval_expression (st, key2);
}

static int
str_compare_op (const char *op, const char *cur, const char *rhs)
{
  int cmp = strcmp (cur, rhs);
  if (streq (op, "BeEqualTo"))               return cmp == 0;
  if (streq (op, "BeGreaterThan"))           return cmp > 0;
  if (streq (op, "BeGreaterThanOrEqualTo"))  return cmp >= 0;
  if (streq (op, "BeLessThan"))              return cmp < 0;
  if (streq (op, "BeLessThanOrEqualTo"))     return cmp <= 0;
  if (streq (op, "BeContain"))               return strstr (cur, rhs) != NULL;
  return 1;
}

/* The numeric comparison operators over longs, shared by the ReferencedNumber
   and numeric-variable branches of pass_variable ("Be"-prefixed ops) and the
   inequality branch of pass_property (bare ops).  No BeContain (numbers do not
   substring-match); an unknown op is the lenient pass. */
static int
num_compare_op (const char *op, long cur, long rhs)
{
  if (strncmp (op, "Be", 2) == 0)
    op += 2;
  if (streq (op, "EqualTo"))               return cur == rhs;
  if (streq (op, "GreaterThan"))           return cur > rhs;
  if (streq (op, "GreaterThanOrEqualTo"))  return cur >= rhs;
  if (streq (op, "LessThan"))              return cur < rhs;
  if (streq (op, "LessThanOrEqualTo"))     return cur <= rhs;
  return 1;
}

static int
pass_variable (a5_state_t *st, a5_restr_t *r)
{
  int vi;
  const a5_variable_t *v;

  /* ReferencedText[n] / ReferencedNumber[n]: the parser-matched text/number,
     not a user variable (clsUserSession.vb:4459-4470).  These are the values
     the stock library tasks test against %text%/%number%. */
  if (strncmp (r->key1, "ReferencedText", 14) == 0)
    {
      const char *cur = a5state_lookup_ref (st, r->key1);
      char *rhs;
      int res;
      if (cur == NULL) cur = a5state_lookup_ref (st, "ReferencedText");
      if (cur == NULL)
        {
          /* The runner reads the turn-global Adventure.sReferencedText slot
             (clsUserSession.vb:4474), filled by every command-matched
             candidate's %text% capture during the scan and defaulted to the
             raw input after it (see a5_state_t.scan_text).  A task with no
             %text% of its own -- AoK's s_SayHelloTo, restriction
             `ReferencedText Must BeContain "hello"` -- tests that leakage,
             not an unbound empty string. */
          int slot = 0;
          if (isdigit ((unsigned char) r->key1[14]))
            slot = atoi (r->key1 + 14) - 1;
          if (slot < 0) slot = 0;
          if (slot > 4) slot = 4;
          cur = st->scan_text[slot];
        }
      rhs = restr_text_value (st, r->key2);
      res = str_compare_op (r->op, cur, rhs);
      free (rhs);
      return res;
    }
  if (strncmp (r->key1, "ReferencedNumber", 16) == 0)
    {
      const char *cur_s = a5state_lookup_ref (st, r->key1);
      char *rhs_s;
      long cur, rhs;
      if (cur_s == NULL) cur_s = a5state_lookup_ref (st, "ReferencedNumber");
      cur = cur_s ? strtol (cur_s, NULL, 10) : 0;
      rhs_s = restr_text_value (st, r->key2);
      rhs = strtol (rhs_s, NULL, 10);
      free (rhs_s);
      return num_compare_op (r->op, cur, rhs);
    }

  vi = a5state_variable_index (st, r->key1);
  if (vi < 0)
    return 1;
  v = &st->adv->variables[vi];

  if (streq (v->type, "Text"))
    {
      const char *cur = st->var_text[vi] ? st->var_text[vi] : "";
      /* The RHS is an expression, not a raw token: the runner compares against
         EvaluateExpression(value).  A string-literal value like `"'greater'"`
         reduces to `greater` (the SetVariable side already stores it unquoted via
         a5text_process), and a %reference% is substituted.  restr_text_value does
         exactly that reduction (same helper the ReferencedText path uses); using
         the raw r->key2 here left the quotes/`%` in place, so e.g. Tingalan's
         `Kindofhung1 BeContain "'greater'"` compared `greater` against the literal
         `"'greater'"` and never matched -- the greater-thirst wound never landed. */
      char *rhs = restr_text_value (st, r->key2);
      int res = str_compare_op (r->op, cur, rhs);
      free (rhs);
      return res;
    }
  else
    {
      long cur = st->var_num[vi];
      long rhs = num_value (st, r->key2);
      return num_compare_op (r->op, cur, rhs);
    }
}

/* --------------------------------------------------- property sub-evaluator */

static int
is_op_numeric_inequality (const char *op)
{
  return strcmp (op, "GreaterThanOrEqualTo") == 0
      || strcmp (op, "GreaterThan") == 0
      || strcmp (op, "LessThanOrEqualTo") == 0
      || strcmp (op, "LessThan") == 0;
}

/*
 * Port of clsUserSession.PassSingleRestriction's Property case (clsUserSession
 * .vb:5060).  The spec is "<propKey> <itemRef> Must|MustNot <Op> <value>" -- it
 * carries an extra item token that parse_spec (built for the one-key Object/
 * Character layout) cannot represent, so re-tokenise the raw text here.
 *
 * An item lacking the property fails the test (bItemContainsProperty=False).
 * EqualTo compares the (override-aware) property value against the resolved
 * value as a string -- covering StaticOrDynamic / OpenStatus / state lists /
 * integer flags / object-key properties.  The numeric inequality operators are
 * evaluated only when the value is a plain integer; an expression value
 * (%Player%-relative weight/bulk sums) is not yet computed, so the restriction
 * passes (the lenient default, matching the previous always-pass stub).
 */
static int
pass_property (a5_state_t *st, a5_restr_t *r)
{
  char *buf = strdup (r->raw ? r->raw : "");
  char *p = buf, *propkey, *item, *musttok, *op, *value;
  int must_not, rr;
  const char *itemkey, *pv;

#define NEXT_TOK(dst)                                                          \
  do { while (*p == ' ') p++; (dst) = p;                                       \
       while (*p && *p != ' ') p++; if (*p) *p++ = '\0'; } while (0)
  NEXT_TOK (propkey);
  NEXT_TOK (item);
  NEXT_TOK (musttok);
  NEXT_TOK (op);
  while (*p == ' ') p++;
  value = p;
#undef NEXT_TOK

  must_not = (strcmp (musttok, "MustNot") == 0);
  itemkey = resolve_key (st, item);

  if (strcmp (op, "EqualTo") == 0)
    {
      const char *rhs = resolve_key (st, value);
      char *rhs_sub = NULL;
      /* resolve_key handles per-turn bindings (%object%) and entity keys but
         returns a bare %variable% unchanged.  A variable RHS -- AlienDiver's
         `CardId ReferencedObject EqualTo %RollForBlankCard%` (match the drawn
         blank card) -- must be substituted to its value or the compare never
         matches.  Only when resolve_key left it untouched AND it still holds a
         '%' (so an object binding is not re-rendered as a display name). */
      if (rhs == value && strchr (value, '%') != NULL)
        { rhs_sub = restr_text_value (st, value); rhs = rhs_sub; }
      /* A Text-property value is compared as a STRING EXPRESSION, so an empty
         one is authored (and serialised) as the VB literal `""` -- Quest Giver
         gates its "You see nothing worthy of note about ..." fallback on
         `daz6LookAtDesc ReferencedCharacter Must EqualTo ""`.  Unquote a
         literal so it compares as the empty string it denotes. */
      if (rhs != NULL && strlen (rhs) >= 2 && rhs[0] == '"'
          && rhs[strlen (rhs) - 1] == '"')
        { char *u = strndup (rhs + 1, strlen (rhs) - 2);
          free (rhs_sub); rhs_sub = u; rhs = rhs_sub; }
      pv = a5state_entity_prop (st, itemkey, propkey);
      if (pv == NULL && strcmp (propkey, "StaticOrDynamic") == 0)
        pv = "Static";          /* the StaticOrDynamic default for an object */
      /* The runner's test is bItemContainsProperty (clsItem.HasProperty), THEN
         a value compare: a property the item carries with no value at all -- an
         un-filled Text property -- reads as "", it does not make the item
         property-less.  Only an item genuinely lacking the property fails. */
      if (pv == NULL && a5state_entity_has_prop (st, itemkey, propkey))
        pv = "";
      rr = (pv != NULL) && streq (pv, rhs);
      free (rhs_sub);
    }
  else if (is_op_numeric_inequality (op)
           && (is_clean_int (value) || strchr (value, '%') != NULL))
    {
      /* A numeric inequality against an integer or a %reference% expression.
         The ADRIFT library carry-capacity restrictions on Take/Put pass a
         %Player%-relative arithmetic expression, e.g.
           MaxBulk %Player% Must GreaterThanOrEqualTo
                                 %Player%.Held.Size.Sum+%objects%.Size.Sum
         Evaluate such an RHS through the OO expression engine (which handles
         .Held/.Size.Sum aggregation) and compare numerically -- previously this
         branch fell through to the lenient always-pass stub, so Scarier never
         enforced the bulk/weight limits the Adrift 5 runner does.  A bare non-integer
         RHS with no reference (e.g. a state word like "Open" in an oddly
         authored `OpenStatus Obj Must LessThan Open`) is not arithmetic, so it
         keeps the lenient pass in the final else. */
      long lv, rv;
      char *rhs_ev = NULL;
      pv = a5state_entity_prop (st, itemkey, propkey);
      lv = pv ? strtol (pv, NULL, 10) : 0;
      if (is_clean_int (value))
        rv = strtol (value, NULL, 10);
      else
        { rhs_ev = a5text_eval_expression (st, value);
          rv = rhs_ev ? strtol (rhs_ev, NULL, 10) : 0; }
      rr = num_compare_op (op, lv, rv);
      free (rhs_ev);
    }
  else
    rr = 1;                     /* non-numeric, non-reference value: pass */

  if (a5restr_trace)
    fprintf (stderr, "    [restr Property: %s %s %s%s %s] -> %d\n", propkey,
             item, must_not ? "MustNot " : "", op, value, must_not ? !rr : rr);
  free (buf);
  return must_not ? !rr : rr;
}

/* ------------------------------------------------------- item sub-evaluator */

/* The Item restriction type (clsUserSession.vb:4972): key1 -- typically the
   ReferencedItem binding of an %item% command token -- is resolved against
   objects, then characters, then locations; a key found in none of them fails
   outright.  Only the operators the runner ties to that three-way lookup are
   mirrored; anything else keeps the best-effort pass. */
static int
pass_item (a5_state_t *st, a5_restr_t *r)
{
  const char *k1 = resolve_key (st, r->key1);
  const char *k2 = resolve_key (st, r->key2);
  const a5_object_t *ob = a5model_object (st->adv, k1);
  const a5_character_t *ch = (ob == NULL) ? a5model_character (st->adv, k1) : NULL;
  const a5_location_t *loc = (ob == NULL && ch == NULL)
                               ? a5model_location (st->adv, k1) : NULL;

  if (ob == NULL && ch == NULL && loc == NULL)
    return 0;                   /* vb:4978: unresolved key -> False */

  if (streq (r->op, "BeType"))
    /* vb:5047: the resolved item's table decides its type. */
    return (streq (k2, "Object") && ob != NULL)
           || (streq (k2, "Character") && ch != NULL)
           || (streq (k2, "Location") && loc != NULL);
  if (streq (r->op, "Exist"))
    return 1;                   /* vb:5049: found in some table above */
  if (streq (r->op, "HaveProperty"))
    return a5state_entity_has_prop (st, k1, k2);
  if (streq (r->op, "BeObject"))
    return ob != NULL && streq (k1, k2);
  if (streq (r->op, "BeCharacter"))
    return ch != NULL && streq (k1, k2);
  if (streq (r->op, "BeLocation"))
    return loc != NULL && streq (k1, k2);
  if (streq (r->op, "BeAtLocation"))
    {
      /* vb:4985: object -> IsVisibleAtLocation; character -> resolved
         Location.LocationKey; location -> key identity. */
      if (ob != NULL)
        return a5state_object_visible_at_location (st,
                 a5state_object_index (st, k1), k2, 0);
      if (ch != NULL)
        return streq (a5state_character_location_key (st,
                        a5state_character_index (st, k1)), k2);
      return streq (k1, k2);
    }
  if (streq (r->op, "BeInGroup"))
    return a5state_object_in_group (st, k2, k1);
  return 1;                     /* unknown operator: don't suppress text */
}

/* ---------------------------------------------------------- single + block */

static int
pass_single (a5_state_t *st, a5_restr_t *r)
{
  int result;
  if (streq (r->type, "Object"))         result = pass_object (st, r);
  else if (streq (r->type, "Location"))  result = pass_location (st, r);
  else if (streq (r->type, "Character")) result = pass_character (st, r);
  else if (streq (r->type, "Variable"))  result = pass_variable (st, r);
  else if (streq (r->type, "Item"))      result = pass_item (st, r);
  else if (streq (r->type, "Property"))  return pass_property (st, r);
  else if (streq (r->type, "Task"))
    {
      int ti = a5state_task_index (st, resolve_key (st, r->key1));
      result = ti >= 0 && st->task_done[ti];
    }
  else if (streq (r->type, "Expression"))
    {
      /* clsUserSession.vb:5165: r = SafeBool(EvaluateExpression(StringValue)).
         The whole element text is the expression (parse_spec's key1/op/key2
         split is meaningless here -- use the un-tokenised raw). */
      char *v = a5text_eval_expression (st, r->raw);
      result = safe_bool (v);
      free (v);
    }
  else if (streq (r->type, "Direction"))
    {
      /* clsUserSession.vb:5161: r = (sKey1 == ReferencedDirection).
         The element text is "<Must|MustNot> Be<Dir>" (FileIO.vb:611-615:
         eMust=token0, sKey1=token1 with the leading "Be" trimmed).  parse_spec's
         generic key1/op split mishandles this, so re-tokenise r->raw here and
         return directly (the trailing must_not negation below would double-flip
         since parse_spec couldn't see the "MustNot"). */
      char tok0[32] = "", tok1[32] = "";
      const char *raw = r->raw ? r->raw : "";
      const char *want, *cur;
      int mnot, rr;
      sscanf (raw, "%31s %31s", tok0, tok1);
      mnot = (strcmp (tok0, "MustNot") == 0);
      want = (strncmp (tok1, "Be", 2) == 0) ? tok1 + 2 : tok1;
      cur = a5state_lookup_ref (st, "ReferencedDirection");
      rr = (cur != NULL && strcmp (want, cur) == 0);
      result = mnot ? !rr : rr;
      if (a5restr_trace)
        fprintf (stderr, "    [restr Direction: %s%s vs %s] -> %d\n",
                 mnot ? "MustNot " : "Must ", want, cur ? cur : "(none)",
                 result);
      return result;
    }
  else
    result = 1;               /* Item: Phase 3-4 */

  if (r->must_not)
    result = !result;
  if (a5restr_trace)
    fprintf (stderr, "    [restr %s: %s %s%s %s] -> %d\n", r->type, r->key1,
             r->must_not ? "MustNot " : "", r->op, r->key2, result);
  return result;
}

/*
 * Ported EvaluateRestrictionBlock (clsUserSession.vb:5190): consume '#' tokens
 * from `block` against rs[0..n) left-to-right, AND binding tighter than OR.
 * *idx tracks the next restriction; advances past short-circuited #'s.
 */
static int count_hashes (const char *s)
{
  int n = 0;
  for (; *s; s++) if (*s == '#') n++;
  return n;
}

/* Normalise "A#O" -> "(...A#)O..." so AND binds before OR (as the Adrift 5 runner). */
static char *
normalise_block (const char *in)
{
  char *s = strdup (in);
  char *hit;
  while ((hit = strstr (s, "A#O")) != NULL)
    {
      int i = (int) (hit - s);
      /* find last '(' before i, insert '(' after it (or at start) */
      int j = -1, k;
      for (k = 0; k < i; k++)
        if (s[k] == '(') j = k;
      j += 1;
      /* build: left[0,j) + "(" + mid[j,i) + "A#)O" + right[i+3..] */
      {
        int len = (int) strlen (s);
        char *out = (char *) malloc ((size_t) len + 8);
        int p = 0, q;
        for (q = 0; q < j; q++) out[p++] = s[q];
        out[p++] = '(';
        for (q = j; q < i; q++) out[p++] = s[q];
        out[p++] = 'A'; out[p++] = '#'; out[p++] = ')'; out[p++] = 'O';
        for (q = i + 3; q < len; q++) out[p++] = s[q];
        out[p] = '\0';
        free (s);
        s = out;
      }
    }
  return s;
}

/*
 * Port of EvaluateRestrictionBlock: normalise at entry, evaluate the leading
 * term (a bracketed sub-block or a single '#'), then recurse on the remainder.
 * AND short-circuits false, OR short-circuits true; *idx advances over every
 * '#' actually consumed (including short-circuited ones).
 *
 * `last_fail` mirrors the Adrift 5 runner's sRestrictionText side effect: every single
 * restriction actually evaluated clears it on a pass and sets it to its own
 * index on a fail, so after the (short-circuiting) walk it names the failing
 * restriction on the deciding path -- the one whose Message the Runner shows.
 * Short-circuited '#'s are skipped (no PassSingleRestriction call), so they do
 * not touch it.  `nodes[i]` is the XML <Restriction> for rs[i].
 */
static int
eval_block (a5_state_t *st, a5_restr_t *rs, const a5_xml_node_t **nodes, int n,
            const char *block, int *idx, int *last_fail)
{
  char *norm = normalise_block (block);
  const char *rest;       /* remainder after the leading term + operator */
  char op;
  int first;
  int result;

  if (norm[0] == '(')
    {
      int depth = 1, off = 1;
      int sublen;
      char *sub;
      while (norm[off] && depth > 0)
        {
          if (norm[off] == '(') depth++;
          else if (norm[off] == ')') depth--;
          off++;
        }
      sublen = off - 2;
      if (sublen < 0) sublen = 0;
      sub = (char *) malloc ((size_t) sublen + 1);
      memcpy (sub, norm + 1, (size_t) sublen);
      sub[sublen] = '\0';
      first = eval_block (st, rs, nodes, n, sub, idx, last_fail);
      free (sub);
      op = norm[off];
      rest = (op == '\0') ? NULL : norm + off + 1;
    }
  else if (norm[0] == '#')
    {
      op = norm[1];
      if (op != '\0' && op != 'A' && op != 'O')
        {
          /* The Adrift 5 runner's inner "Case Else": a '#' followed by neither an
             operator nor end-of-block (e.g. the spurious leading '#' of a
             malformed "##A#").  EvaluateRestrictionBlock takes Case Else and
             falls off the end -> default False, WITHOUT calling
             PassSingleRestriction.  So the restriction is never evaluated and
             sRestrictionText (here *last_fail) is left untouched -- preserving
             any carried value (the basis for Anno's reference-free OpeningHid
             failing *with* the previous command's leftover message). */
          (*idx)++;             /* The runner still runs iRestNum += 1 */
          free (norm);
          return 0;
        }
      {
        /* Out of range means the block has more '#' terms than the task has
           restrictions, i.e. a malformed game file; treat it as a pass so a
           broken block cannot lock a task out.  The runner has no equivalent
           branch -- RestrictionArrayList.Item returns Nothing on a range error
           and restx.Copy then throws, which GetGeneralTask swallows into
           "I didn't understand that command".  Note idx is *our own local*,
           passed down through this recursion, whereas the runner's iRestNum is a
           session field: a HaveRouteInDirection restriction re-enters restriction
           evaluation and used to leave the outer walk on the wrong index (fixed
           in ADRIFT 5.0.36 by saving/restoring it; see
           test/BugHuntOnMenelaus_walkthrough.txt).  We are immune by
           construction, so this branch is unreachable for well-formed games. */
        int my = (*idx)++;
        first = (my < n) ? pass_single (st, &rs[my]) : 1;
        if (my < n)
          *last_fail = first ? -1 : my; /* clear on pass, record on fail */
      }
      rest = (op == '\0') ? NULL : norm + 2;
    }
  else
    {
      free (norm);
      /* The Adrift 5 runner's EvaluateRestrictionBlock hits "Case Else" (a block that
         starts with neither '(' nor '#') and falls off the end with no Return,
         so the Boolean function yields its default False -- a malformed bracket
         sequence FAILS, it does not pass. */
      return 0;
    }

  if (rest == NULL)
    result = first;
  else if (op == 'A')
    {
      if (!first) { *idx += count_hashes (rest); result = 0; }
      else result = eval_block (st, rs, nodes, n, rest, idx, last_fail);
    }
  else if (op == 'O')
    {
      if (first) { *idx += count_hashes (rest); result = 1; }
      else result = eval_block (st, rs, nodes, n, rest, idx, last_fail);
    }
  else
    /* The character after the leading term is neither 'A' nor 'O' (e.g. the
       extra '#' in the malformed "##A#").  The Adrift 5 runner's inner Select Case
       takes "Case Else" and falls off the end -> default False.  Some games
       (Anno 1700's OpeningClo2) ship such sequences expecting the task to FAIL,
       so the engine moves on to the general task. */
    result = 0;

  free (norm);
  return result;
}

/*
 * Shared core for a5restr_pass / a5restr_fail_message: parse the restriction
 * specs, build the (square-bracket-expanded) BracketSequence and evaluate it.
 * Returns the boolean result; when `fail_node_out` is non-NULL it is set to the
 * <Restriction> on the deciding failing path (the Adrift 5 runner's sRestrictionText
 * source) -- NULL if the block passes or no single restriction failed.
 */
static int
eval_restrictions (a5_state_t *st, const a5_xml_node_t *restrictions,
                   const a5_xml_node_t **fail_node_out)
{
  a5_restr_t *rs;
  const a5_xml_node_t **nodes;
  const a5_xml_node_t *c;
  const char *bracket;
  int n, i, idx = 0, result, last_fail = -2;   /* -2 = no restriction evaluated */
  char *normbrackets = NULL;

  if (fail_node_out != NULL)
    *fail_node_out = NULL;
  if (restrictions == NULL)
    return 1;
  n = a5xml_count (restrictions, "Restriction");
  if (n == 0)
    return 1;

  rs = (a5_restr_t *) calloc ((size_t) n, sizeof *rs);
  nodes = (const a5_xml_node_t **) calloc ((size_t) n, sizeof *nodes);
  i = 0;
  for (c = restrictions->first_child; c != NULL; c = c->next)
    if (strcmp (c->name, "Restriction") == 0)
      {
        const a5_xml_node_t *tn = restr_type_node (c);
        rs[i].type = tn ? tn->name : "";
        parse_spec (&rs[i], tn ? tn->text : "");
        nodes[i] = c;
        i++;
      }

  bracket = a5xml_child_text (restrictions, "BracketSequence");
  if (bracket == NULL || bracket[0] == '\0')
    {
      /* default: AND all restrictions -> "#A#A#..." */
      int sz = n * 2;
      normbrackets = (char *) malloc ((size_t) sz + 1);
      {
        int p = 0, j;
        for (j = 0; j < n; j++)
          {
            if (j) normbrackets[p++] = 'A';
            normbrackets[p++] = '#';
          }
        normbrackets[p] = '\0';
      }
      bracket = normbrackets;
    }

  /*
   * Expand the square-bracket grouping ADRIFT uses: '[' -> "((", ']' -> "))"
   * (FileIO.vb:640).  This is not a stray-bracket cleanup -- the doubling is what
   * keeps sequences such as "#A#A#A#A[#A#A#A#A#)O#)A#A#" balanced.
   */
  {
    char *b = (char *) malloc (strlen (bracket) * 2 + 1);
    char *w = b;
    const char *q;
    for (q = bracket; *q; q++)
      {
        if (*q == '[')      { *w++ = '('; *w++ = '('; }
        else if (*q == ']') { *w++ = ')'; *w++ = ')'; }
        else                  *w++ = *q;
      }
    *w = '\0';
    result = eval_block (st, rs, nodes, n, b, &idx, &last_fail);
    free (b);
  }

  /* The Adrift 5 runner's sRestrictionText side effect (clsUserSession.vb:5176/5181):
     every PassSingleRestriction on the deciding path clears it on a pass and
     sets it to the restriction's Message on a fail; a call that evaluates *no*
     single restriction (malformed bracket -> last_fail still -2) leaves it
     untouched, so the carried value from a previous command survives.  Persists
     across commands -- never reset per turn. */
  if (last_fail == -1)
    st->restriction_text = NULL;                 /* deciding restriction passed */
  else if (last_fail >= 0 && last_fail < n)
    st->restriction_text = a5xml_child (nodes[last_fail], "Message");

  if (fail_node_out != NULL && !result && last_fail >= 0 && last_fail < n)
    {
      *fail_node_out = nodes[last_fail];
      /* sRouteError override: keep st->route_error set only when the deciding
         failing restriction is the HaveRouteInDirection one whose exit was
         restriction-blocked (PassSingleRestriction: "If sRouteError <>
         sRestrictionText AndAlso sRouteError <> ''").  The getter then prefers
         it over the restriction's generic "There is no route..." message. */
      if (!(st->route_error != NULL
            && streq (rs[last_fail].type, "Character")
            && streq (rs[last_fail].op, "HaveRouteInDirection")))
        st->route_error = NULL;
    }

  free (normbrackets);
  for (i = 0; i < n; i++)
    free (rs[i].buf);
  free (rs);
  free (nodes);
  return result;
}

const a5_xml_node_t *
a5restr_fail_message (a5_state_t *st, const a5_xml_node_t *restrictions)
{
  const a5_xml_node_t *fail_node = NULL;
  st->route_error = NULL;
  eval_restrictions (st, restrictions, &fail_node);
  if (st->route_error != NULL)
    return st->route_error;             /* blocked-exit message wins */
  return fail_node ? a5xml_child (fail_node, "Message") : NULL;
}

int
a5restr_pass (a5_state_t *st, const a5_xml_node_t *restrictions)
{
  return eval_restrictions (st, restrictions, NULL);
}

/* The first <Restriction> child whose parsed spec satisfies `match`, or NULL.
 * `limit` caps how many restrictions are looked at (-1 = all); `match` is
 * handed the type node (its name is the restriction's type -- "Object",
 * "Character", ...) and the parsed spec, whose buffer this owns. */
template <typename F>
static const a5_xml_node_t *
restr_find (const a5_xml_node_t *restrictions, int limit, F match)
{
  const a5_xml_node_t *c;
  int i = 0;

  if (restrictions == NULL)
    return NULL;
  for (c = restrictions->first_child; c != NULL; c = c->next)
    {
      const a5_xml_node_t *tn;
      a5_restr_t r;
      int hit;
      if (strcmp (c->name, "Restriction") != 0)
        continue;
      if (limit >= 0 && i++ >= limit)
        break;
      tn = restr_type_node (c);
      if (tn == NULL)
        continue;
      parse_spec (&r, tn->text);
      hit = match (tn, &r);
      free (r.buf);
      if (hit)
        return c;
    }
  return NULL;
}

/* Does a `<ref_alias> Must Exist` restriction (e.g. "ReferencedObject2") fall
 * *within* the task's evaluated BracketSequence?  ADRIFT evaluates only the first
 * M restrictions, where M is the number of '#' placeholders in the bracket (the
 * restrictions list may be longer than the bracket -- a truncated/malformed bracket
 * simply leaves the trailing restrictions unevaluated, exactly as eval_block does).
 * Used to decide whether an unmatched reference is genuinely *required*: if its
 * Must Exist is never evaluated, the task does not actually need it, so a sibling
 * reference's out-of-scope ambiguity ("You can't see any oil!") surfaces instead of
 * this reference's no-reference fallback ("Sorry, I'm not sure which object...").
 * Mirrors the pour task (`#A#A#`, object2's Exist truncated -> cantsee) vs the blow
 * / hang tasks (object2's Exist inside the bracket -> no-reference message). */
int
a5restr_exist_evaluated (const a5_xml_node_t *restrictions, const char *ref_alias)
{
  const a5_xml_node_t *c;
  const char *bracket;
  int n = 0, M;

  if (restrictions == NULL || ref_alias == NULL)
    return 0;
  for (c = restrictions->first_child; c != NULL; c = c->next)
    if (strcmp (c->name, "Restriction") == 0)
      n++;
  bracket = a5xml_child_text (restrictions, "BracketSequence");
  /* an absent bracket ANDs every restriction */
  M = (bracket == NULL || bracket[0] == '\0') ? n : count_hashes (bracket);

  return restr_find (restrictions, M,          /* only the bracket prefix */
                     [ref_alias] (const a5_xml_node_t *tn, const a5_restr_t *r)
                     {
                       (void) tn;
                       return strcmp (r->key1, ref_alias) == 0
                              && streq (r->op, "Exist");
                     }) != NULL;
}

int
a5restr_has_exist (const a5_xml_node_t *restrictions, char type)
{
  const char *want_type = (type == 'c') ? "Character"
                        : (type == 'l') ? "Location" : "Object";

  return restr_find (restrictions, -1,
                     [want_type] (const a5_xml_node_t *tn, const a5_restr_t *r)
                     {
                       return strcmp (tn->name, want_type) == 0
                              && streq (r->op, "Exist");
                     }) != NULL;
}

/* The <Message> node of the first Object restriction "<ReferencedObject...> Must
 * Exist" in the task.  The Adrift 5 runner surfaces this as the ambiguity indicator when
 * a %objects% NOUN matches more than one object (e.g. `get fish` -> two fish ->
 * "Sorry, I'm not sure which object you are trying to take."): the multi-match is
 * inherently ambiguous, so the task's own reference-existence message is shown as
 * a prefix ahead of the specific failure.  Returns NULL if the task has no such
 * restriction. */
const a5_xml_node_t *
a5restr_exist_message (const a5_xml_node_t *restrictions)
{
  const a5_xml_node_t *c =
    restr_find (restrictions, -1,
                [] (const a5_xml_node_t *tn, const a5_restr_t *r)
                {
                  return strcmp (tn->name, "Object") == 0
                         && streq (r->op, "Exist")
                         && r->key1 != NULL
                         && strncmp (r->key1, "ReferencedObject", 16) == 0;
                });
  return (c != NULL) ? a5xml_child (c, "Message") : NULL;
}
