/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- task execution.
 *
 * The action layer clsUserSession runs once a task is chosen: ExecuteActions /
 * ExecuteSingleAction (the core action set -- MoveObject/MoveCharacter,
 * Set/Inc/Dec, SetProperty, SetTasks Execute, conversation, EndGame), the
 * specific-override task dispatch, and the run_task / run_general drivers.
 *
 * Split out of a5run.cpp, and since split further -- the three subsystems this
 * file used to carry as well now sit beside it:
 *
 *   a5run_resp.cpp   per-command response aggregation (htblResponsesPass/Fail:
 *                    dedup, %objects% merging, the deferred-message reference
 *                    snapshots, the final flush)
 *   a5run_conv.cpp   the conversation layer (FindConversationNode /
 *                    ExecuteConversation, ContainsWord, partner-gone teardown)
 *   a5run_sort.cpp   the .NET introsort that orders Specific override children
 *
 * The shared run struct, the response-collector types and every cross-file entry
 * point (run_task, run_general, run_action, update_seen, conv_contains_word, the
 * resp_* and exec_conversation calls) live in a5run_internal.h.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "a5expr.h"
#include "a5parse.h"
#include "a5rand.h"
#include "a5restr.h"
#include "a5run.h"
#include "a5run_internal.h"
#include "a5sb.h"
#include "a5sexpr.h"
#include "a5text.h"
#include "a5util.h"

/* Debug/trace switches, resolved from the environment ONCE.  These sit on hot
   paths -- A5DBG_SPEC is tested per override child per item, A5_TRACE_SETVAR and
   A5_DEBUG_SCORE per variable action -- and the rest of the engine already
   resolves its trace switch a single time (the a5run_trace global, set by the
   host).  Function-local statics keep them private to this file rather than
   adding four more globals to a5run.h for switches only these routines use;
   C++11 guarantees the one-time initialisation. */
static const char *dbg_spec   (void) { static const char *v = getenv ("A5DBG_SPEC");      return v; }
static const char *dbg_task   (void) { static const char *v = getenv ("A5_TRACE_TASK");   return v; }
static const char *dbg_setvar (void) { static const char *v = getenv ("A5_TRACE_SETVAR"); return v; }
static const char *dbg_score  (void) { static const char *v = getenv ("A5_DEBUG_SCORE");  return v; }

/* Recursion limit shared by the structural descents in this file: Specific-
   override nesting (execute_task_with_overrides), the SetTasks FOR body, the
   `Execute` dispatch and run_action's plural %objects% expansion.  Distinct from
   A5_MAX_EXEC_DEPTH, which bounds the Execute CHAIN across those resets. */
#define A5_MAX_ACTION_DEPTH 16

/* The adventure's own (stable, DOM-aliased) pointer for an entity key.  Object,
   character and location go through the model's hashed key index; only groups
   (which the index does not cover) still need a scan.  The family order is
   load-bearing -- a key present in two families resolves to the earlier one --
   so it is preserved exactly. */
static const char *
canon_key (const a5_adventure_t *adv, const char *k)
{
  int i;
  if (k == NULL) return NULL;
  i = a5model_key_index (adv, 'O', k);
  if (i >= 0) return adv->objects[i].key;
  i = a5model_key_index (adv, 'C', k);
  if (i >= 0) return adv->characters[i].key;
  i = a5model_key_index (adv, 'L', k);
  if (i >= 0) return adv->locations[i].key;
  for (i = 0; i < adv->n_groups; i++)
    if (streq (adv->groups[i].key, k))     return adv->groups[i].key;
  return NULL;
}

/*
 * Resolve an action operand to a key.  Crucially returns a *stable* pointer for
 * anything that may be stored in the world state (object/character locations):
 * the model's own key string, never a pointer into a temporary token or the
 * per-turn binding buffer (which would dangle).
 */
static const char *
act_key (a5_state_t *st, const char *tok)
{
  const char *b, *resolved, *c;
  if (tok == NULL) return NULL;
  if (streq (tok, "%Player%"))
    resolved = a5state_player_key (st);  /* dynamic: whoever is the player now */
  else if (streq (tok, "Player"))
    resolved = "Player";                 /* literal model key of the player slot */
  else
    { b = a5state_lookup_ref (st, tok); resolved = b ? b : tok; }
  c = canon_key (st->adv, resolved);
  if (c != NULL)
    return c;
  if (streq (resolved, "Hidden"))
    return "Hidden";
  return resolved;   /* non-entity (direction/literal): used transiently only */
}

/* Apply a variable value string ("1", "%roller%", "RAND (1, 4)") -> number. */
static long
eval_num_value (a5_state_t *st, const char *raw)
{
  char *proc;
  if (raw != NULL && strncmp (raw, "RAND", 4) == 0)
    {
      /* RAND (min, max): inclusive random in [min, max] via the shared
         erkyrath_random (the Adrift 5 runner Global.Random(iMin, iMax)).  The data
         writes it as "RAND (1, 4)" (spaces optional).  A bound may itself be a
         %variable% (LostCoastlines' item valuation writes `RAND (0, %valuator%)`);
         the runner resolves it via ReplaceFunctions before Global.Random, so substitute
         %tokens% first -- otherwise the bound parses as 0 and the whole
         age/decoration/aura draw collapses to a no-op RAND(0,0). */
      long lo = 0, hi = 0;
      char *end;
      char *sub = (strchr (raw, '%') != NULL)
                    ? a5text_process_noalr (st, raw) : NULL;
      const char *rr = sub ? sub : raw;
      const char *p = rr + 4;
      while (*p && !(*p == '-' || (*p >= '0' && *p <= '9'))) p++;
      lo = strtol (p, &end, 10);
      p = end;
      while (*p && !(*p == '-' || (*p >= '0' && *p <= '9'))) p++;
      hi = (*p) ? strtol (p, NULL, 10) : lo;
      free (sub);
      return a5rand_between (lo, hi);
    }
  /* NO ALR pass here: the runner evaluates action values via EvaluateExpression
     (ReplaceFunctions only); ReplaceALRs is Display-time.  GFS's display ALR
     "17000" -> "1.700.0" must not turn IncVariable cl_Money = "1700000"
     into 1. */
  proc = a5text_process_noalr (st, raw ? raw : "0");
  /* The runner evaluates a SetVariable value through EvaluateExpression
     (clsVariable) and reads the numeric result as VB Val(): full arithmetic
     (+ - * / ^ mod, with runner precedence and away-from-zero division) plus
     function calls -- min/max/if and the rand()/oneof() draws.  a5_eval_sexpr
     IS that evaluator; it supersedes the old integer-only a5arith fast path,
     whose ^ handled precedence/associativity differently from the runner
     (clsVariable reduces ^ on run 2, the +/- tier, left-to-right -- see
     FrankenDrift clsVariable.vb).  %vars% are already substituted, so Tingalan's
     Roller "rand(0,%playerwits%)" is now rand(0,N) and draws via the same
     a5rand_between as the uppercase RAND path.

     The value is Val() of the *result* -- its leading integer, else 0.  So a
     non-numeric value ("Object3") reads as 0, as does an out-of-range result
     the runner guards with SafeInt(Val(...)): a division by zero evaluates to
     "\xe2\x88\x9e" (Val 0, the former a5arith div/mod-by-zero guard), not the
     leading digit of the raw "5/0". */
  {
    char *sv = a5_eval_sexpr (proc);
    long e = strtol (sv, NULL, 10);      /* Val(): leading integer, else 0 */
    free (sv);
    free (proc);
    return e;
  }
}

/* Split a `Name = "value"` action body into name and (unquoted) value. */
static int
split_assignment (const std::string &body, std::string &name, std::string &value)
{
  size_t eq = body.find ('=');
  if (eq == std::string::npos)
    return 0;
  name = body.substr (0, eq);
  /* trim name */
  while (!name.empty () && isspace ((unsigned char) name.back ())) name.pop_back ();
  size_t a = name.find_first_not_of (" \t");
  name = (a == std::string::npos) ? "" : name.substr (a);
  /* value: strip spaces then surrounding quotes */
  std::string v = body.substr (eq + 1);
  a = v.find_first_not_of (" \t");
  if (a != std::string::npos) v = v.substr (a);
  while (!v.empty () && isspace ((unsigned char) v.back ())) v.pop_back ();
  if (v.size () >= 2 && v.front () == '"' && v.back () == '"')
    v = v.substr (1, v.size () - 2);
  value = v;
  return 1;
}

/* Split a '|'-separated list, keeping empty fields (the shape of the runner's
   Items array for a pipe-list reference). */
static std::vector<std::string>
split_pipe (const char *s)
{
  std::vector<std::string> v;
  std::string cur;
  for (const char *p = s; ; p++)
    {
      if (*p == '|' || *p == '\0')
        { v.push_back (cur); cur.clear (); if (*p == '\0') break; }
      else
        cur += *p;
    }
  return v;
}

/* True if `v` is, after trimming, a single top-level call "name(...)" spanning
   the whole string -- not e.g. "f(a) & g(b)" or a plain literal.  `name` is
   set to the (unlowered) identifier. */
static bool
a5_bare_function_call (const std::string &v, std::string &name)
{
  size_t i = 0, n = v.size ();
  while (i < n && isspace ((unsigned char) v[i])) i++;
  size_t start = i;
  while (i < n && (isalpha ((unsigned char) v[i]) || v[i] == '_')) i++;
  size_t name_end = i;
  /* The runner's tokenizer strips redundant spaces before parsing, so "RAND (a, b)"
     (LostCoastlines writes its RAND assignments with a space) is the same call
     as "RAND(a,b)".  Skip any gap between the identifier and its '('. */
  while (i < n && isspace ((unsigned char) v[i])) i++;
  if (name_end == start || i >= n || v[i] != '(')
    return false;
  name = v.substr (start, name_end - start);
  size_t j = n;
  while (j > start && isspace ((unsigned char) v[j - 1])) j--;
  if (j == 0 || v[j - 1] != ')')
    return false;
  int depth = 0;
  for (size_t k = i; k < j; k++)
    {
      if (v[k] == '(') depth++;
      else if (v[k] == ')') { depth--; if (depth == 0 && k != j - 1) return false; }
    }
  return depth == 0;
}

static void emit_look (a5_run_t *run, sb_t *out);
static std::string render_look_string (a5_run_t *run);
static void emit_completion (a5_run_t *run, const a5_xml_node_t *comp, sb_t *out);
static void emit_message_body (a5_run_t *run, char *m, int pre_alr_ink, sb_t *out,
                               const char *dedup_key = 0);
static std::vector<std::string> current_obj_ref_keys (a5_state_t *st);

/* Render the room view as REAL output -- marking_display=1 so its <DisplayOnce>
   segments retire -- restoring the previous marking flag afterwards.  The common
   wrapper around render_look_string at every final-state Display site (and, via
   a5run_look, the frontend's post-UNDO room redisplay). */
std::string
render_look_marked (a5_run_t *run)
{
  a5_mark_guard mg (run->st, 1);
  return render_look_string (run);
}

/* ----------------------------------------- specific-override task dispatch */

/* One resolved command reference: its general type and resolved entity key. */
struct ref_info { char type; const char *key; };   /* 'o'/'c'/'d'/'t'/'n'/'l' */

/* Map one command reference name ("objects", "object2", "character1", ...) to
   its (general type, resolved entity key) using this turn's bindings.  The bare
   "Referenced<Stem>" is reference 1; "Referenced<Stem>N" is the N-th, so a
   2-specific override keys each Specific off its own reference. */
static ref_info
ref_info_for_name (a5_state_t *st, const std::string &name)
{
  std::string base = name, num;
  while (!base.empty () && isdigit ((unsigned char) base.back ()))
    { num.insert (num.begin (), base.back ()); base.pop_back (); }
  ref_info ri; ri.type = 't'; ri.key = NULL;
  std::string stem;
  if (base == "object" || base == "objects" || base == "item")
    { stem = "Object"; ri.type = 'o'; }
  else if (base == "character" || base == "characters")
    { stem = "Character"; ri.type = 'c'; }
  else if (base == "direction") { stem = "Direction"; ri.type = 'd'; }
  else if (base == "number")    { stem = "Number"; ri.type = 'n'; }
  else if (base == "location")  { stem = "Location"; ri.type = 'l'; }
  else if (base == "text")      { stem = "Text"; ri.type = 't'; }
  else return ri;
  /* The plural %objects%/%characters% slot resolves through its OWN alias.
     When the command also carries a singular %object% (`hide %objects% in
     %object%`), bind_reference deliberately does NOT alias the plural onto
     ReferencedObject (the runner's GetReference keys ReferencedObject to the "object1"
     reference only, clsUserSession.vb:3990), so reading ReferencedObject here
     would yield the singular container -- Dwarf's `hide axe in vegetation`
     matched cl_HideObjInV1's [Sword, cl_Vegetation] specifics against
     [vegetation, vegetation] and missed.  ReferencedObjects holds the current
     item on both the per-item plural loop and the single-item direct path. */
  if (base == "objects")
    ri.key = a5state_lookup_ref (st, "ReferencedObjects");
  else if (base == "characters")
    ri.key = a5state_lookup_ref (st, "ReferencedCharacters");
  if (ri.key == NULL && !num.empty () && num != "1")
    ri.key = a5state_lookup_ref (st, ("Referenced" + stem + num).c_str ());
  if (ri.key == NULL)
    ri.key = a5state_lookup_ref (st, ("Referenced" + stem).c_str ());
  return ri;
}

/* Recompute the ordered, resolved references for a matched command, using the
   bindings resolve_refine already established this turn. */
static std::vector<ref_info>
collect_refs (a5_state_t *st, const a5_match_t *m)
{
  std::vector<ref_info> v;
  for (int i = 0; i < m->n_refs; i++)
    v.push_back (ref_info_for_name (st, m->ref_name[i]));
  return v;
}

/* Normalise a bare singular reference name to its "...1" indexed form
   (FileIO.vb:647: %object% -> %object1%, etc.).  Plural "objects"/"characters"
   and already-indexed names are left unchanged. */
static void
normalize_singular_ref (std::string &base)
{
  if (base == "object")    base = "object1";
  if (base == "character") base = "character1";
  if (base == "direction") base = "direction1";
  if (base == "number")    base = "number1";
  if (base == "text")      base = "text1";
}

/* The ordered reference names (%objects%, %object2%, ...) in a task's first
   command, normalised to their binding base ("objects","object2"). */
static std::vector<std::string>
command_refs (const a5_task_t *t)
{
  std::vector<std::string> v;
  if (t->n_commands == 0) return v;
  const char *p = t->commands[0];
  while (*p)
    {
      if (*p == '%')
        {
          const char *q = p + 1;
          while (*q && *q != '%') q++;
          if (*q == '%')
            {
              std::string base (p + 1, (size_t) (q - (p + 1)));
              /* singular %object% normalises to object1 only for matching, but
                 the binding base keeps "objects" plural distinct. */
              normalize_singular_ref (base);
              v.push_back (base);
              p = q + 1;
              continue;
            }
        }
      p++;
    }
  return v;
}

/* clsUserSession.RefsMatchSpecifics / GetAlternateRef: a Specific child's
   Specifics are always defined in the order of the parent's FIRST command, but
   the input may have matched a later command whose references appear in a
   different order (SayToCharacter: `say %text% to %character%` vs `say to
   %character% %text%`).  Reorder the resolved refs into first-command order --
   a no-op when the first command matched -- so the 1:1 Specific matching in
   refs_match_specifics lines up.  When the matched command's reference names
   are not a permutation of the first command's (the runner's GetAlternateRef falls
   back to the unmapped index there), keep the matched order. */
static std::vector<ref_info>
collect_refs_ordered (a5_state_t *st, const a5_match_t *m, const a5_task_t *t)
{
  std::vector<ref_info> v = collect_refs (st, m);
  std::vector<std::string> first = command_refs (t);
  if ((int) first.size () != m->n_refs)
    return v;
  std::vector<ref_info> w;
  std::vector<char> used ((size_t) m->n_refs, 0);
  for (const std::string &nm : first)
    {
      int found = -1;
      for (int i = 0; i < m->n_refs; i++)
        if (!used[i] && nm == m->ref_name[i]) { found = i; break; }
      if (found < 0)
        return v;
      used[found] = 1;
      w.push_back (v[found]);
    }
  return w;
}

/* Build the resolved references for TASK from its first command's reference
   names and this turn's bindings -- used when re-dispatching a task via
   SetTasks Execute, where there is no a5_match_t to derive them from. */
static std::vector<ref_info>
refs_from_bindings (a5_state_t *st, const a5_task_t *t)
{
  std::vector<ref_info> v;
  for (auto &nm : command_refs (t))
    v.push_back (ref_info_for_name (st, nm));
  return v;
}

/* Evaluate a SetTasks "Execute (...)" argument to an entity KEY (not a display
   name): a plain %reference% yields its bound key; %ParentOf[..]% and the like
   evaluate via the text engine (which already returns keys). */
static char *
eval_arg_to_key (a5_state_t *st, const std::string &arg)
{
  std::string a = trim_ws (arg);
  /* A bare entity key (no %...% at all, e.g. `vnl_Dial`) is already a key -- pass
     it through verbatim.  It must NOT go through a5text_process, whose display
     pipeline auto-capitalises the first letter ("vnl_Dial" -> "Vnl_Dial") and so
     corrupts the case-sensitive object key. */
  if (a.find ('%') == std::string::npos)
    return strdup (a.c_str ());
  if (a.size () >= 2 && a.front () == '%' && a.back () == '%'
      && a.find ('[') == std::string::npos)
    {
      std::string nm = a.substr (1, a.size () - 2);
      if (nm == "Player") return strdup (a5state_player_key (st));
      std::string lname = lower (nm);
      const char *k = NULL;
      if (lname == "objects")          k = a5state_lookup_ref (st, "ReferencedObjects");
      else if (lname.rfind ("object", 0) == 0)
        { std::string cap = "ReferencedObject"; if (lname.size () > 6) cap += lname.substr (6);
          k = a5state_lookup_ref (st, cap.c_str ()); if (!k) k = a5state_lookup_ref (st, "ReferencedObject"); }
      else if (lname.rfind ("character", 0) == 0)
        { /* Indexed exactly like the object branch above: `%character2%` is
             ReferencedCharacter2, NOT the bare ReferencedCharacter.  Beginner's
             Cave's combat loop passes the DEFENDER on as
             `Execute EnemyHits (%character2%)` from a task whose %character1%
             is the attacker, so collapsing the index bound the damage task to
             the attacking rat -- every enemy hit wounded the attacker and
             skipped the player's armour absorption. */
          std::string cap = "ReferencedCharacter";
          if (lname.size () > 9) cap += lname.substr (9);
          k = a5state_lookup_ref (st, cap.c_str ());
          if (!k) k = a5state_lookup_ref (st, "ReferencedCharacter"); }
      if (k != NULL) return strdup (k);
    }
  /* A function/OO arg (e.g. %PropertyValue[%object1%,LockKey]%) evaluates to an
     entity KEY -- run the substitution passes but NOT auto-capitalisation, which
     would turn "s_SkeletonKe" into "S_SkeletonKe" and break the case-sensitive
     lookup (same class of bug as the bare-key `vnl_Dial` case above). */
  return a5text_process_nocap (st, a.c_str ());
}

/* True when a SetTasks-Execute argument names the target task's own reference
   (`%objects%` for "objects", singular `%object%` normalising to "object1") --
   the runner's pass-the-live-reference-through case (clsUserSession.vb:2188,
   `sParam = sRef`), which must leave the existing binding untouched. */
static bool
arg_is_ref_passthrough (const std::string &arg, const std::string &rname)
{
  std::string an = trim_ws (arg);
  if (an.size () < 2 || an.front () != '%' || an.back () != '%')
    return false;
  std::string base = an.substr (1, an.size () - 2);
  normalize_singular_ref (base);
  return base == rname;
}

/* A bare `Group.<Property>` SetTasks-Execute argument is an OO reference that
   the Adrift 5 runner resolves (ReplaceOO -> ReplaceOOProperty, Global.vb:1597/930) to
   the group's ordered member LIST filtered to members that carry <Property> --
   e.g. `Objects1.StaticOrDynamic`, where the mandatory StateList property is on
   every object, yields all 125 member keys.  The runner packs those keys as the Items of
   a single reference so AttemptToExecuteTask -> ExecuteSubTasks runs the executed
   General task once per member, binding its reference to each key in turn (this
   is how the world-generation creation tasks value every object/secret/story).

   Fill OUT with the member keys in arlMembers order and return true when ARG is
   such a reference; return false (OUT untouched by the caller) otherwise so the
   argument falls back to the ordinary single-key resolution. */
static bool
group_prop_member_keys (a5_state_t *st, const std::string &arg,
                        std::vector<std::string> &out)
{
  out.clear ();
  std::string a = trim_ws (arg);
  if (a.empty ()) return false;
  /* A bare OO reference carries no %tokens% (those go through the text engine)
     and is a single-level `Group.Property`. */
  if (a.find ('%') != std::string::npos) return false;
  size_t dot = a.find ('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= a.size ())
    return false;
  std::string grp = a.substr (0, dot);
  std::string prop = a.substr (dot + 1);
  if (prop.find ('.') != std::string::npos) return false;

  const a5_adventure_t *adv = st->adv;
  int gi;
  for (gi = 0; gi < adv->n_groups; gi++)
    if (streq (adv->groups[gi].key, grp.c_str ())) break;
  if (gi >= adv->n_groups) return false;

  const a5_propdef_t *pd = a5model_propdef (adv, prop.c_str ());
  if (pd == NULL) return false;
  /* A mandatory property (the runner's clsProperty.Mandatory) is added to every
     applicable item at load, so clsItem.HasProperty is always true for it --
     StaticOrDynamic is the canonical case.  Otherwise keep the members that
     HAVE the property (clsItem.HasProperty), presence not value -- a valueless
     SelectionOnly marker (AlienDiver's `BlankCards.ObjectIsAC` = the available
     blank cards) counts even though a5state_entity_prop returns NULL for it. */
  const char *mnd = pd->node ? a5xml_child_text (pd->node, "Mandatory") : NULL;
  int mandatory = (mnd != NULL && strcmp (mnd, "0") != 0);

  int n = a5state_group_count (st, grp.c_str ());
  for (int i = 0; i < n; i++)
    {
      const char *mk = a5state_group_member_at (st, grp.c_str (), i);
      if (mk == NULL) continue;
      if (mandatory || a5state_entity_has_prop (st, mk, prop.c_str ()))
        out.push_back (mk);
    }
  /* The runner still builds one (empty-keyed) Item for an empty resolution, so the
     executed task runs once with an unbound reference (its BeInGroup restriction
     then fails silently).  Preserve that single no-op run. */
  if (out.empty ())
    out.push_back ("");
  return true;
}

/* Resolve a Specific's key to a concrete key (handles %Player%/references). */
static const char *
specific_key (a5_state_t *st, const char *key)
{
  if (key == NULL || key[0] == '\0')
    return NULL;                                  /* "" = match any */
  if (streq (key, "%Player%"))
    return a5state_player_key (st);
  if (strchr (key, '%') != NULL)
    {
      const char *b = a5state_lookup_ref (st, key);   /* best-effort */
      return b ? b : key;
    }
  return key;
}

/* A Specific that constrains nothing (no Key elems, or a single empty Key) --
   the runner's `Keys.Count = 0 OrElse (Keys.Count = 1 AndAlso Keys(0) = "")`. */
static int
spec_is_any (a5_state_t *st, const a5_specific_t *sp)
{
  if (sp->n_keys == 0) return 1;
  if (sp->n_keys == 1 && specific_key (st, sp->keys[0]) == NULL) return 1;
  return 0;
}

/* Match a resolved-specifics vector 1:1 against the references (one Specific per
   reference, each non-empty key matching its reference's resolved key). */
static int
match_specifics_vec (a5_state_t *st, const std::vector<const a5_specific_t *> &specs,
                     const std::vector<ref_info> &refs)
{
  if (specs.size () != refs.size ())
    return 0;
  for (size_t i = 0; i < specs.size (); i++)
    {
      const a5_specific_t *sp = specs[i];
      int matched = (sp->n_keys == 0);            /* no Key elems = match any */
      for (int k = 0; k < sp->n_keys; k++)
        {
          const char *want = specific_key (st, sp->keys[k]);
          if (want == NULL) { matched = 1; break; }       /* empty key = any */
          /* The runner compares the Specific key to the reference's matched possibility
             case-insensitively (RefsMatchSpecifics, clsUserSession.vb:634).  For a
             Text specific (e.g. Grandpa's `say %text%` -> `vnl_SayKennedy`,
             key "kennedy") the reference resolves to the typed text, so match it
             case-insensitively; entity keys stay exact. */
          if (refs[i].key != NULL
              && (streq (want, refs[i].key)
                  || (refs[i].type == 't'
                      && lower (want) == lower (refs[i].key))))
            { matched = 1; break; }
        }
      if (!matched)
        return 0;
    }
  return 1;
}

/* RefsMatchSpecifics: the child's Specifics must line up 1:1 with the resolved
   references.  A *nested* override may carry FEWER specifics than the references
   (it only re-constrains some of them) -- the runner (clsUserSession.vb:1060-1071) then
   expands the child against its parent: keep each parent specific that is itself
   constrained, and fill the parent's "any" slots from the child's specifics in
   order.  E.g. Jacaranda's `give tape to pirate`: Task49 (Object=tape) overrides
   Task48 (Object=any, Character=pirate) -> effective specifics [tape, pirate]. */
static int
refs_match_specifics (a5_state_t *st, const a5_task_t *child,
                      const a5_task_t *parent, const std::vector<ref_info> &refs)
{
  std::vector<const a5_specific_t *> specs;
  if (child->n_specifics == (int) refs.size ())
    {
      for (int i = 0; i < child->n_specifics; i++)
        specs.push_back (&child->specifics[i]);
    }
  else if (parent != NULL
           && child->n_specifics < parent->n_specifics
           && parent->n_specifics == (int) refs.size ())
    {
      int ichild = 0;
      for (int i = 0; i < parent->n_specifics; i++)
        {
          const a5_specific_t *psp = &parent->specifics[i];
          if (spec_is_any (st, psp) && ichild < child->n_specifics)
            specs.push_back (&child->specifics[ichild++]);
          else
            specs.push_back (psp);
        }
    }
  else
    return 0;
  int r = match_specifics_vec (st, specs, refs);
  if (dbg_spec () != NULL)
    {
      fprintf (stderr, "[spec] child=%s parent=%s r=%d refs:", child->key,
               parent ? parent->key : "-", r);
      for (auto &ri : refs)
        fprintf (stderr, " (%c,%s)", ri.type, ri.key ? ri.key : "NULL");
      fprintf (stderr, " specs:");
      for (auto *sp : specs)
        for (int k = 0; k < sp->n_keys; k++)
          fprintf (stderr, " [%s]", sp->keys[k]);
      fprintf (stderr, "\n");
    }
  return r;
}

/* The per-command response aggregation layer (clsUserSession
   htblResponsesPass/Fail: dedup, %objects% merging, the deferred-message
   reference snapshots and the final flush) lives in a5run_resp.cpp.  Its
   types and entry points are declared in a5run_internal.h. */


/* True when every Specific of a child override matches "any" (an empty key) --
   the "lazy" catch-all pattern (e.g. TakeObjectsFromOthersLazy, Key empty).
   Such an override that *fails* its restrictions simply declines the item and
   lets the general parent run; an object-specific override (Key=Ballgown) that
   fails instead asserts a per-object failure and suppresses the parent. */
static int
child_all_any (a5_state_t *st, const a5_task_t *child)
{
  if (child->n_specifics == 0) return 0;
  for (int i = 0; i < child->n_specifics; i++)
    if (!spec_is_any (st, &child->specifics[i])) return 0;
  return 1;
}

/* The .NET introspective sort that orders a parent's Specific override children
   (clsTask.Children / TaskPrioritySortComparer) lives in a5run_sort.cpp; its
   net_introspective_sort entry point is declared in a5run_internal.h. */

/* Run the After-override children into `buf` in priority order, stopping after
   the first that produces output (unless ContinueAlways) -- the runner's bContinue
   rule.  Shared by the look-deferred and defer-text branches of
   execute_task_with_overrides, which each run the children into a side buffer. */
static void
run_after_children (a5_run_t *run, const std::vector<const a5_task_t *> &after,
                    sb_t *buf)
{
  a5_state_t *st = run->st;
  for (auto *child : after)
    if (a5restr_pass (st, child->restrictions))
      {
        size_t alen0 = buf->len;
        run->task_raw_output = 0;
        run_task (run, child, 0, buf);
        int ai = a5state_task_index (st, child->key);
        if (ai >= 0) st->task_done[ai] = 1;
        ev_on_task_completed (run, child->key, buf);
        if ((buf->len > alen0 || run->task_raw_output)
            && !child->continue_lower) break;
      }
}

/*
 * Run PARENT honouring its Specific child overrides (clsTask Children +
 * SpecificOverrideType), recursively -- a faithful port of the runner's
 * AttemptToExecuteTask -> AttemptToExecuteSubTask -> AttemptToExecuteTask.  A
 * child whose Specifics match REFS and whose own restrictions pass runs in place
 * of (Override) or before/after the parent's text+actions, and is itself
 * resolved for nested overrides.  Reusable from both the command-matched parent
 * (run_general) and a SetTasks-Execute re-dispatch.
 */
static void
execute_task_with_overrides (a5_run_t *run, const a5_task_t *parent,
                             const std::vector<ref_info> &refs, int depth,
                             sb_t *out)
{
  a5_state_t *st = run->st;
  if (dbg_task ())
    fprintf (stderr, "TASK %-14s @draw %ld depth=%d\n",
             parent->key ? parent->key : "?", a5rand_draw_count, depth);
  int parent_text = 1, parent_actions = 1;
  int any_child_fail_output = 0;    /* The runner's bAnyChildHasOutput (vb:1053/1141) */
  std::vector<const a5_task_t *> after;
  size_t frame_len0 = sink_len (run, out);

  /* A command-bearing General has override children; a Specific override
     (depth>0) is itself scanned for its OWN nested children -- e.g. GetAWooden
     overrides TakeObjectFromLocation, which overrides TakeObjects.  Build the
     child list the way clsTask.Children does: the parent's Specific tasks in
     XML load order, completed non-repeatable ones filtered out (the property's
     bIncludeCompleted=False gate -- the runner's vb:730 entry check never sees them),
     then .NET-introsorted by Priority so equal-priority ties land exactly
     where the runner's unstable sort puts them (see net_introspective_sort above). */
  std::vector<int> kids;
  if ((parent->n_commands > 0 || depth > 0) && depth < A5_MAX_ACTION_DEPTH)
    {
      for (int i = 0; i < run->adv->n_tasks; i++)
        {
          const a5_task_t *t = &run->adv->tasks[i];
          if (!streq (t->type, "Specific")) continue;
          if (t->general_key == NULL || !streq (t->general_key, parent->key))
            continue;
          if (!t->repeatable && st->task_done[i]) continue;
          kids.push_back (i);
        }
      net_introspective_sort (run->adv, kids);
    }
  for (int cidx : kids)
      {
        const a5_task_t *child = &run->adv->tasks[cidx];
        if (!refs_match_specifics (st, child, parent, refs)) continue;

        const char *ot = child->override_type;
        int is_after = ot != NULL && strncmp (ot, "After", 5) == 0;
        if (is_after)
          { after.push_back (child); continue; }

        /* clsUserSession.AttemptToExecuteSubTask iterates EVERY matching child in
           priority order (clsUserSession.vb:1056-1160), not just the first.  After
           running a child it keeps going unless that child claims the turn -- the
           bContinue rule (clsUserSession.vb:911-936): continue past a child that
           produced no output (or whose Continue is ContinueAlways), stop after one
           that did.  So Amazon's per-move travel-time override (Delay*, no output)
           runs and then falls through to Beforeplay (the "You move <dir>." +
           Date/Time override, with output), which then stops the scan.  Detect a
           child's output by the growth of `out`. */
        size_t len0 = sink_len (run, out);

        if (!a5restr_pass (st, child->restrictions))
          {
            /* The runner's bPassingOnly abort: once an earlier sibling child has failed
               WITH output (bAnyChildHasOutput, set only in the fail branch,
               vb:1141), every later child is attempted with bPassingOnly=True.
               A refs-matching child that then FAILS its restrictions is silent
               (the fail-message block is gated `If Not bPassingOnly`, vb:1244)
               and AttemptToExecuteTask returns early ("If bPassingOnly AndAlso
               Not bPass Then Return False", before the vb:911 bContinue rules)
               -- so bContinueExecutingTasks stays False and the child scan
               stops dead (vb:1146 Exit For).  The parent keeps whatever
               suppression earlier children set; later siblings (even a passing
               one) never run.  FoF `ask clockmaker about dials` after the
               clock explanation: Task2932 (7480) fails with "I have already
               explained...", then Task2949 (7495) matches and fails silently,
               aborting the scan BEFORE the wearily catch-all Task2950 (7496,
               +2 score) is reached -- the runner shows only Task2932's message. */
            if (any_child_fail_output)
              break;
            /* A Specific task that matches the references but whose restrictions
               fail still claims the turn if it has a fail message: show it and
               suppress the parent.  In the default HighestPriorityTask mode a
               fail *with* output stops the scan; in HighestPriorityPassingTask
               mode, or with no output at all, scanning continues so a later
               override (or the parent) can run. */
            const a5_xml_node_t *fm =
              a5restr_fail_message (st, child->restrictions);
            /* On the plural map path, a "lazy" match-any override (empty
               Specific key) that fails simply declines this item -- its fail
               message is collected (and dropped later if the item passes the
               general task), but the parent still runs.  An object-specific
               override that fails asserts a per-object failure: show it and
               suppress the parent (clsUserSession's bShouldParentExecuteTasks).
               Mirrors why Amazon's bottle (lazy fail) is still picked up while
               RunBronwynn's ballgown (RemoveBall, Key=Ballgown) is not. */
            int lazy = run->resp != NULL && child_all_any (st, child);
            int fail_claims = 0;
            if (fm != NULL)
              {
                if (run->resp != NULL)
                  { resp_add_fail (run, fm); any_child_fail_output = 1; }
                else
                  {
                    char *fmsg = a5text_describe (st, fm);
                    /* The runner buffers this restriction text in htblResponsesFail and
                       flushes it after the passes; a later sibling override that
                       PASSES with output for the same reference cancels it
                       (clsUserSession.vb:804-834).  Buffer it in the command's
                       exec scope instead of writing to `out`, so AoK's `show
                       talisman` drops Task236's "Right idea - wrong location!"
                       once Task1427 passes.  When nothing passes, the scope
                       flush at the end of run_general still shows it. */
                    if (msg_has_output (fmsg))
                      {
                        fail_claims = 1;
                        any_child_fail_output = 1;
                        if (run->exec_scope != NULL)
                          {
                            if (run->exec_scope->fail_seen.insert (fmsg).second)
                              {
                                std::vector<std::string> pos;
                                for (auto &ri : refs)
                                  pos.push_back (ri.key ? ri.key : "");
                                run->exec_scope->ov_fails.push_back
                                  (std::make_pair (std::string (fmsg), pos));
                              }
                          }
                        else
                          { sb_pspace (out); sb_puts (out, fmsg); }
                      }
                    free (fmsg);
                  }
                if (!lazy) { parent_text = 0; parent_actions = 0; }
              }
            if (!lazy && (sink_len (run, out) > len0 || fail_claims)
                && !child->continue_lower && !st->adv->hp_passing)
              break;                        /* fail with output: claims the turn */
            continue;                       /* no output (or HPPT): keep scanning */
          }

        /* Recurse so the child's own Specific overrides fire in its place
           (AttemptToExecuteSubTask -> recursive AttemptToExecuteTask). */
        size_t ovf0 = run->exec_scope != NULL
                      ? run->exec_scope->ov_fails.size () : 0;
        execute_task_with_overrides (run, child, refs, depth + 1, out);
        st->task_done[cidx] = 1;        /* clsUserSession.vb:1193 task.Completed = True */
        /* A completing override child fires its own EventControls/WalkControls,
           exactly like the parent (the runner's AttemptToExecuteSubTask -> recursive
           AttemptToExecuteTask runs the control loop for every task).  E.g. Stone
           of Wisdom's ring-knockout `s_AttackTheT` is a Specific override of
           AttackCharacterWithObject; only its completion fires StartTroll's
           `Stop Completion s_AttackTheT`, disarming the troll's death subevent. */
        ev_on_task_completed (run, child->key, out);
        if (ot == NULL || streq (ot, "Override"))
          { parent_text = 0; parent_actions = 0; }
        else if (streq (ot, "BeforeTextOnly"))      parent_actions = 0;
        else if (streq (ot, "BeforeActionsOnly"))   parent_text = 0;
        /* BeforeTextAndActions: parent still runs both (child ran first).
           the runner (clsUserSession.vb:1106/1111): parent ACTIONS run for
           BeforeActionsOnly|BeforeTextAndActions; parent TEXT is output for
           BeforeTextOnly|BeforeTextAndActions.  So BeforeActionsOnly keeps the
           parent's actions but suppresses its completion message (Grandpa's
           `vnl_OpenGate1` "safe enough to open" override: the gate still opens,
           but the generic "You open the gate." is not shown). */

        /* Stop only when this passing child produced output (and isn't
           ContinueAlways); otherwise fall through to the next matching child.
           A nested override that failed WITH a buffered message (ov_fails grew
           during the recursion) counts as output too: the runner's bContinue rule sees
           the htblResponsesFail entry, so the sibling scan stops and a later
           sibling's own restriction fail is never shown.  Temperamentum `get
           mirror`: TakeObjectFromLocation's GetALargeM buffers "You can't get
           at the mirror while the little girl is standing there." -- without
           this stop the scan fell through to TakeObjectsFromOthers, appending
           its "The large mirror is not on or inside another object!".  Gated
           on !hp_passing like the direct-fail claim above. */
        int subtree_fail_output = run->exec_scope != NULL
                                  && run->exec_scope->ov_fails.size () > ovf0;
        if ((sink_len (run, out) > len0
             || (subtree_fail_output && !st->adv->hp_passing))
            && !child->continue_lower)
          break;
      }

  /* The Adrift 5 runner defers an AggregateOutput task's completion-message function
     replacement to final Display (clsUserSession.vb:1184/1210 replace eagerly
     only when NOT AggregateOutput).  So an aggregate parent's text reflects
     state changed by its AfterTextAndActions / AfterActionsOnly children, which
     run before Display.  Mirror that: when an aggregate parent with no actions
     of its own has After-children, run the children first, then render the
     parent text -- keeping the parent text ahead of the children's output.
     Stone of Wisdom's `x window` needs this: ExamineAnO (AfterTextAndActions)
     increments Windowcoun, and ExamineObjects' %object%.Description segments are
     gated on it, so the "movement"/"creatures" lines must reflect the post-
     increment count on the same turn the runner shows them. */
  int defer_text = run->resp == NULL && parent_text && parent->aggregate
                   && parent->actions == NULL && !after.empty ();

  /* If the parent has After children that run actions on the direct path, defer
     its room view (Execute Look) so it renders at the post-child state, mirroring
     the runner's AggregateOutput Look deferral (see emit_look). */
  int prev_defer_look = run->defer_look;
  int prev_look_pending = run->look_pending;
  size_t prev_look_pos = run->look_pos;
  char *prev_look_pinned = run->look_pinned;
  int enable_look_defer = run->resp == NULL && !defer_text && !after.empty ();
  if (enable_look_defer)
    { run->defer_look = 1; run->look_pending = 0; run->look_pinned = NULL; }

  /* The runner's per-item AttemptToExecuteTask evaluates the task's restrictions at
     EXECUTION time (ExecuteSubTasks leaf), so an earlier item's actions can
     fail a later item of the same plural command -- Magnetic Moon's `put all
     in box`: once the (worn-then-held) backpack has been moved inside the
     box, the grapnel/cable/glue/camera nested in it are no longer
     BeHeldByCharacter and the runner refuses them ("You are not carrying ...!",
     items merged into one fail response).  Scarier's match phase refined the
     item set against the PRE-command state, so re-check the parent's
     restrictions here, on the plural per-item path only (cur_item set) --
     the singular/movement paths keep their single match-time evaluation and
     draw stream. */
  if ((parent_text || parent_actions) && depth == 0 && run->resp != NULL
      && !run->resp->cur_item.empty ()
      && !a5restr_pass (st, parent->restrictions))
    {
      const a5_xml_node_t *fm = st->restriction_text;
      if (fm != NULL)
        resp_add_fail (run, fm);
      parent_text = parent_actions = 0;
    }

  if (parent_text || parent_actions)
    {
      /* run_task emits both text and actions; gate via temporary copies. */
      if (!parent_text && parent_actions)
        {
          const a5_xml_node_t *act = parent->actions;
          if (act != NULL)
            for (const a5_xml_node_t *c = act->first_child; c; c = c->next)
              run_action (run, c->name, c->text, 0, out);
        }
      else if (parent_text && !defer_text)
        run_task (run, parent, 0, out);
    }

  run->defer_look = prev_defer_look;
  /* Only the frame that enabled deferral consumes the pending look; a nested
     non-enabling frame (e.g. MovePlayer, run via SetTasks Execute, which emits
     the actual Look) leaves it pending for the enabling parent to render after
     its After children run.  The Look's offset is into the enabling frame's
     `out`, which is the same sink threaded through the nested calls. */
  int look_deferred = 0;
  char *pinned_view = NULL;
  if (enable_look_defer)
    {
      if (run->look_pending) look_deferred = 1;
      pinned_view = run->look_pinned;          /* consumed below (or freed) */
      run->look_pending = prev_look_pending;
      run->look_pos = prev_look_pos;
      run->look_pinned = prev_look_pinned;
    }

  if (look_deferred)
    {
      /* The parent's room view was skipped.  Run the After children into a side
         buffer (their actions update what the room lists -- e.g. Grandpa's `go
         west` moves Buster into the western enclosure), render the view once at
         the resulting final state, then emit view-then-children so the room
         reflects the moves and the children's lines follow it.  Using a side
         buffer (rather than splicing) lets sb_pspace compute each separator
         against real preceding text -- e.g. Jacaranda's "...Exits are ... in.
         [pSpace]It is raining out here." */
      sb_t after_buf; sb_init (&after_buf);
      run_after_children (run, after, &after_buf);
      /* A pinned view (the Look dance's two test renders differed -- a random
         pick moved) is emitted verbatim; otherwise render at the final state
         (real output, so <DisplayOnce> segments retire). */
      std::string view;
      if (pinned_view != NULL)
        view = std::string (pinned_view);
      else
        view = render_look_marked (run);
      /* The runner's response map holds ONE slot for the stock Look's raw template:
         a second Execute Look in the same command (The Salvage's request
         docking Looks at the docked pad, then on the bridge) re-renders that
         slot rather than appending -- first slot's position, last text. */
      exec_resp_scope *sc = run->exec_scope;
      if (sc != NULL && sc->look_off >= 0 && sc->look_sink == out
          && (size_t) sc->look_off + sc->look_len <= out->len)
        {
          sb_splice (out, (size_t) sc->look_off, sc->look_len, view.c_str ());
          sc->look_len = view.size ();
        }
      else
        {
          sb_pspace (out);
          if (sc != NULL)
            { sc->look_sink = out; sc->look_off = (long) out->len;
              sc->look_len = view.size (); }
          sb_puts (out, view.c_str ());
        }
      if (after_buf.len > 0) { sb_pspace (out); sb_puts (out, after_buf.p); }
      free (after_buf.p);
    }
  else if (defer_text)
    {
      /* Children run into a side buffer so the parent text can be spliced ahead
         of them once their actions have updated state. */
      sb_t after_buf; sb_init (&after_buf);
      run_after_children (run, after, &after_buf);
      int self_ti = a5state_task_index (st, parent->key);
      if (self_ti >= 0) st->task_done[self_ti] = 1;
      const a5_xml_node_t *comp = a5xml_child (parent->node, "CompletionMessage");
      /* The stock aggregate Look on the direct path: the runner's response map holds
         ONE slot keyed on its raw template, rendered at final Display -- after
         the LocationTrigger drain and event tick.  So (1) a command's SECOND
         Execute Look contributes nothing (one view, first slot's position,
         final state's text -- The Salvage's request docking Looks at the
         docked pad, then again on the bridge), and (2) the view reflects
         drain-time moves (McKinney relocating to the Airlock during `board
         avadora`).  Emit a display-defer sentinel; a5run_flush_display_defers
         renders the view into it at Display. */
      exec_resp_scope *sc = run->exec_scope;
      if (streq (parent->key, "Look") && parent->aggregate && sc != NULL
          && run->display_defers != NULL)
        {
          if (!(sc->look_off >= 0 && sc->look_sink == out))
            {
              sb_pspace (out);
              sc->look_sink = out;
              sc->look_off = (long) out->len;
              char mark[24];
              snprintf (mark, sizeof mark, "\004%d\004",
                        (int) run->display_defers->size ());
              sb_puts (out, mark);
              sc->look_len = strlen (mark);
              run->display_defers->push_back ("\002");
            }
        }
      else if (comp != NULL)
        emit_completion (run, comp, out);
      if (after_buf.len > 0) { sb_pspace (out); sb_puts (out, after_buf.p); }
      free (after_buf.p);
    }
  else
    for (auto *child : after)
      if (a5restr_pass (st, child->restrictions))
        {
          /* The runner's bContinue rule (AttemptToExecuteSubTask): stop the child scan
             once a passing child produces output, unless it is ContinueAlways.
             Two After overrides of the same general+spec that both pass (Galen's
             Quest `put yellow gem in ceiling receptacle` matches PutAYellow AND
             PutYellowG1, both +2) must not both fire -- only the higher-priority
             PutAYellow does, so the score tracks the runner (40, not 42). */
          size_t olen0 = sink_len (run, out);
          run->task_raw_output = 0;
          run_task (run, child, 0, out);
          int ai = a5state_task_index (st, child->key);
          if (ai >= 0) st->task_done[ai] = 1;   /* task.Completed = True */
          ev_on_task_completed (run, child->key, out);   /* fire its controls too */
          if ((sink_len (run, out) > olen0 || run->task_raw_output)
              && !child->continue_lower) break;
        }
      else
        {
          /* An After child is evaluated AFTER the parent's actions ran, so its
             restrictions see the post-action state, and a failure shows its
             restriction message like any child fail (vb:1244; the parent's own
             output has already been Displayed, so no cancel applies).  Head
             Case's `open rear doors`: the AfterTextAndActions Daza8Task24
             ("MustNot BeInState Open" -> "They are already open.") always
             fails against the freshly-opened doors, and the runner prints "You open
             the van rear doors.  They are already open." */
          const a5_xml_node_t *fm =
            a5restr_fail_message (st, child->restrictions);
          if (fm != NULL)
            {
              char *fmsg = a5text_describe (st, fm);
              if (msg_has_output (fmsg))
                {
                  sb_pspace (out);
                  sb_puts (out, fmsg);
                  free (fmsg);
                  break;               /* fail with output claims the scan */
                }
              free (fmsg);
            }
        }
  free (pinned_view);

  /* This frame produced pass output on the direct path: record its POSITIONAL
     reference signature (the runner keys every htblResponsesPass entry with the
     subtask's NewReferences) for the exec_scope ov_fails cancel rule. */
  if (run->resp == NULL && run->exec_scope != NULL
      && sink_len (run, out) > frame_len0)
    {
      std::vector<std::string> sig;
      for (auto &ri : refs)
        sig.push_back (ri.key ? ri.key : "");
      run->exec_scope->pass_sigs.push_back (sig);
    }
}

/* Run a matched General task (the directly command-matched parent), honouring
   its Specific child overrides. */
void
run_general (a5_run_t *run, const a5_task_t *parent, const a5_match_t *m,
             sb_t *out)
{
  a5_state_t *st = run->st;

  /* A genuine plural %objects% command (resolve_plural bound ref_items this
     turn) is run one item at a time, the-Adrift-5-runner-style (ExecuteSubTasks):
     each item independently selects its Specific overrides, and the resulting
     completion / fail messages are aggregated (dedup + %objects% merge) and
     flushed once.  A single resolved item keeps the ordinary direct path so
     non-plural commands are byte-identical. */
  int objidx = -1, diridx = -1;
  for (int i = 0; i < m->n_refs; i++)
    {
      if (strcmp (m->ref_name[i], "objects") == 0 && objidx < 0) objidx = i;
      if (strncmp (m->ref_name[i], "direction", 9) == 0) diridx = i;
    }

  /* A command runs under a per-command response map (the Adrift 5 runner's
     htblResponsesPass/Fail, cleared at the top of AttemptToExecuteTask and flushed
     once at the end) in two cases.  (1) A genuine plural %objects% command
     iterates its items one at a time (ExecuteSubTasks), aggregating completion /
     fail messages.  (2) A *movement* command (a %direction% reference): its
     `Beforeplay` override and its `Execute Look` -> `Beforeplay1` both run
     `ts_tasCheckTime`, emitting the same "Date: ..." line, and the map dedups the
     two into one (clsUserSession.vb:783).  Other single-reference commands stay
     on the direct path -- they never produce a same-turn duplicate, and routing
     conversation / multi-restriction output through the map's pass-then-fail
     reorder would perturb their byte-exact ordering.  A SetTasks-Execute sub-task
     shares whichever map is active (it does not flush). */
  bool plural = (objidx >= 0 && st->n_ref_items > 1);
  bool movement = (diridx >= 0);
  /* An "all" command whose %objects% resolved to items of which NONE pass shows
     the general task's <FailOverride> ("You are not carrying anything."),
     discarding the per-item override fail messages -- the runner's AttemptToExecuteTask
     finalize (clsUserSession.vb:789: htblResponsesPass.Count=0 AndAlso
     FailOverride<>"" AndAlso ContainsWord(sInput,"all") -> Display(FailOverride)).
     This must fire even when "all" collapses to a SINGLE surviving item: a
     genuine singular command (no "all") stays on the direct path and keeps its
     object-specific fail message (`put food in bag` -> "makes a mess"), but `put
     all in bag` with only the un-baggable food held must show the FailOverride,
     not the food's "makes a mess" (AoS `put all in bag` after the pouch/whetstone
     are already bagged).  Route it through the response map so the child fail
     messages buffer and can be replaced by the FailOverride at flush. */
  bool all_failover = (objidx >= 0 && parent->fail_override != NULL
                       && conv_contains_word (m->ref_text[objidx], "all"));
  bool use_map = plural || movement || all_failover;

  resp_map rm;
  if (use_map) run->resp = &rm;

  /* Per-command response scope for SetTasks-Execute'd tasks (the runner's
     htblResponsesPass/Fail, live for the whole AttemptToExecuteTask): buffers
     Execute'd-task restriction-fail messages and cancels the ones whose
     objects also produced a pass response, at the flush below. */
  exec_resp_scope escope;
  exec_resp_scope *prev_escope = run->exec_scope;
  run->exec_scope = &escope;

  /* Deferred-completion sink for this command: an AggregateOutput completion's
     random draw (a `<#..#>` expression or a `%var[rand]%` index) is held in the
     run-level display_defers sink and drawn at end-of-turn Display
     (a5run_flush_display_defers), after the command's task, the LocationTrigger
     drain and the event tick -- matching the runner's Display-time ReplaceExpressions.
     Only armed on the single-reference (resp==NULL) path; the plural/movement map
     re-renders aggregate completions at its own flush. */
  std::vector<std::string> *prev_defers = run->comp_defers;
  run->comp_defers = use_map ? prev_defers : run->display_defers;

  if (plural)
    {
      std::vector<const char *> items (st->ref_items,
                                       st->ref_items + st->n_ref_items);
      const char *rtext = m->ref_text[objidx];
      for (const char *it : items)
        {
          rm.cur_item = it;
          st->ref_items[0] = it;
          st->n_ref_items = 1;
          bind_reference (st, "objects", it, rtext);
          a5state_bind_ref (st, "ReferencedObjects", it);
          std::vector<ref_info> refs = collect_refs_ordered (st, m, parent);
          execute_task_with_overrides (run, parent, refs, 0, out);
        }
    }
  else
    {
      std::vector<ref_info> refs = collect_refs_ordered (st, m, parent);
      execute_task_with_overrides (run, parent, refs, 0, out);
    }

  run->exec_scope = prev_escope;
  if (use_map)
    {
      run->resp = NULL;
      /* No child override passed for an "all" command: replace the buffered
         per-item fail messages with the general task's FailOverride (the runner's
         htblResponsesPass.Count=0 branch). */
      if (all_failover)
        {
          bool any_pass = false;
          for (auto &e : rm.ents)
            if (e.is_pass) { any_pass = true; break; }
          if (!any_pass)
            {
              char *fo = a5text_describe (st, parent->fail_override);
              rm.ents.clear ();
              rm.nmut = 0;
              if (msg_has_output (fo))
                {
                  resp_entry e;
                  e.is_pass = true;
                  e.rendered = fo ? fo : "";
                  rm.ents.push_back (e);
                  rm.nmut = 1;
                }
              free (fo);
            }
        }
      resp_flush (run, &rm, out);
    }
  exec_scope_flush (run, &escope, out);

  /* Deferred completion draws accumulated in run->display_defers are NOT flushed
     here: the runner holds them to the final Display loop, after the LocationTrigger
     drain and event tick.  a5run_flush_display_defers (called from finish_turn)
     draws + splices them then. */
  run->comp_defers = prev_defers;
}

/* End-of-turn Display flush (the runner's ReplaceALRs->ReplaceExpressions loop): draw
   each deferred AggregateOutput-completion body in emit order (== the runner htblResponses
   order) and splice the value into its `\004<idx>\004` sentinel slot in `out`.
   A body tagged with a leading \001 is a raw `%var[rand()]%` token re-resolved
   through the text pipeline (drawing its index); an untagged body is a frozen
   `<#..#>` sexpr reduced by a5_eval_sexpr (drawing its OneOf/Rand). */
void
a5run_flush_display_defers (a5_run_t *run, sb_t *out)
{
  a5run_flush_display_defers_from (run, out, 0);
}

/* Partial flush: draw + splice only the sink entries from index `from` on,
   leaving earlier entries (and their sentinels in `out`) pending.  An
   event/walk/LocationTrigger-fired task is its own AttemptToExecuteTask in the
   runner (bChildTask=False), so ITS aggregate responses Display -- and draw --
   at the end of that attempt (clsUserSession.vb:782,851-856), while the
   player command's own deferred draws stay held to the later command flush.
   attempt_event_task flushes its attempt's entries through here. */
void
a5run_flush_display_defers_from (a5_run_t *run, sb_t *out, size_t from)
{
  std::vector<std::string> *sink = run->display_defers;
  if (sink == NULL || sink->size () <= from)
    return;
  void *prev_ed = run->st->expr_defer;
  run->st->expr_defer = NULL;                 /* no re-deferral during the draw */

  /* Draw order.  The runner's Display expands ONE response at a time: its
     ReplaceFunctions pass first (each `%var[rand]%` -- our \001 entries -- in
     that pass's own scan order), then ReplaceExpressions over the fully
     OO-substituted text, which reduces every `<#..#>` LEFT TO RIGHT -- so a
     `<#OneOf#>` nested inside an OO description read (Symphonica's Rory
     `CharHereDesc` inside `Player.Location.Description`) draws AFTER a
     textually-earlier top-level `<#OneOf#>`, even though Scarier's render
     pushed the nested one to the sink first (the OO pass runs before the
     expression scan).  Re-order ONLY the untagged sexpr entries among
     themselves by their sentinel's text position; \001 (function-pass) and
     \002 (room view) entries keep their push slots, which already mirror the
     runner's per-response pass order. */
  std::vector<size_t> slot_entry;             /* entry index drawn at each slot */
  for (size_t k = from; k < sink->size (); k++)
    slot_entry.push_back (k);
  {
    std::vector<size_t> sslots;               /* slots holding sexpr entries */
    std::vector<std::pair<long, size_t> > spos;    /* (text pos, entry idx)  */
    long lastpos = -1;
    for (size_t k = from; k < sink->size (); k++)
      {
        const std::string &body = (*sink)[k];
        if (body == "\002" || (!body.empty () && body[0] == '\001'))
          continue;
        char mark[24];
        snprintf (mark, sizeof mark, "\004%d\004", (int) k);
        const char *at = out->p != NULL ? strstr (out->p, mark) : NULL;
        /* A sentinel with no text position was emitted inside a tag that the
           renderer then stripped (OS/PlugIn.Exe holds its pause in
           `<wait %Tpaus%>`, whose `<#rand(5, 9)#>` still has to DRAW in text
           order).  Give it the previous entry's key so the stable sort keeps
           it in push order rather than banishing it to the end. */
        if (at != NULL)
          lastpos = (long) (at - out->p);
        sslots.push_back (k - from);
        spos.push_back (std::make_pair (lastpos, k));
      }
    std::stable_sort (spos.begin (), spos.end ());
    for (size_t i = 0; i < sslots.size (); i++)
      slot_entry[sslots[i]] = spos[i].second;
  }

  for (size_t s = 0; s < slot_entry.size (); s++)
    {
      size_t k = slot_entry[s];
      const std::string &body = (*sink)[k];
      char *val;
      if (body == "\002")
        {
          /* Deferred stock-Look room view: render at Display state (real
             output -- DisplayOnce segments retire). */
          std::string v = render_look_marked (run);
          val = strdup (v.c_str ());
        }
      else if (!body.empty () && body[0] == '\003')
        /* Raw `<#..#>` body held whole (its %ref[RAND]% substitution draws):
           substitute AND reduce here, at the flush. */
        val = a5text_eval_expression (run->st, body.c_str () + 1);
      else
        val = (!body.empty () && body[0] == '\001')
                ? a5text_process_noalr (run->st, body.c_str () + 1)
                : a5_eval_sexpr (body.c_str ());
      char mark[24];
      int ml = snprintf (mark, sizeof mark, "\004%d\004", (int) k);
      char *at = out->p != NULL ? strstr (out->p, mark) : NULL;
      if (at != NULL)
        {
          size_t pos = (size_t) (at - out->p);
          /* The runner reduces expressions INSIDE Display, so its
             CapAfterFullStop pass still sees the drawn value; ours ran back at
             a5text_process time, before this splice.  Re-apply the cap for a
             value that lands at a sentence start (OS/PlugIn.Exe's card line
             begins with the deferred `<#if(..)#>`, "four of Diamonds"). */
          if (val != NULL && islower ((unsigned char) val[0])
              && a5text_is_sentence_start (out->p, pos))
            val[0] = (char) toupper ((unsigned char) val[0]);
          std::string tail (out->p + pos + ml, out->len - pos - (size_t) ml);
          out->len = pos;
          out->p[pos] = '\0';
          sb_puts (out, val ? val : "");
          sb_puts (out, tail.c_str ());
        }
      free (val);
    }
  run->st->expr_defer = prev_ed;
  sink->resize (from);
}

/* Flush a SetTasks-Execute response scope: emit each buffered fail message
   (in order) unless every object it was evaluated against also produced a
   pass response -- the runner's flush loop, clsUserSession.vb:804-849, where a fail
   whose reference items all match a pass's is dropped and the survivors are
   Display'd after the pass responses. */
void
exec_scope_flush (a5_run_t *run, exec_resp_scope *sc, sb_t *out)
{
  (void) run;
  for (auto &f : sc->fails)
    {
      bool cancelled;
      if (f.second.empty ())
        /* No bound references: the runner's pass-cancels-fail loop (clsUserSession.vb:804)
           leaves bAllMatch True against the first pass message, because its per-ref
           check `For iRef = 0 To refsFail.Length-1` iterates 0 To -1 and never runs.
           So a ref-less Execute'd-task fail is cancelled by the mere presence of ANY
           pass response this command -- yet shown when there were none.  Tingalan's
           Search task passes with "You find nothing here", which cancels the
           Execute'd encounter's ref-less "...something follows..." restriction fail;
           the runner stays silent where Scarier used to leak the extra paragraph. */
        cancelled = !sc->pass_seen.empty ();
      else
        {
          cancelled = true;
          for (auto &k : f.second)
            if (!sc->pass_refs.count (k)) { cancelled = false; break; }
        }
      if (!cancelled)
        { sb_pspace (out); sb_puts (out, f.first.c_str ()); }
    }
  sc->fails.clear ();

  /* Specific-override fails buffered on the direct path: the runner's positional
     cancel (clsUserSession.vb:812-834) -- the fail is dropped only when some
     pass response's reference vector matches it position-for-position over the
     fail's FULL length (a pass with fewer refs cannot cancel: refsPass.Length
     <= iRef => bAllMatch False).  A ref-less fail keeps the bAllMatch-True
     quirk: cancelled by the mere presence of any pass response. */
  for (auto &f : sc->ov_fails)
    {
      bool cancelled;
      if (f.second.empty ())
        cancelled = !sc->pass_seen.empty () || !sc->pass_sigs.empty ();
      else
        {
          cancelled = false;
          for (auto &sig : sc->pass_sigs)
            {
              bool all = true;
              for (size_t i = 0; i < f.second.size (); i++)
                if (f.second[i].empty () || i >= sig.size ()
                    || sig[i] != f.second[i])
                  { all = false; break; }
              if (all) { cancelled = true; break; }
            }
        }
      if (!cancelled)
        { sb_pspace (out); sb_puts (out, f.first.c_str ()); }
    }
  sc->ov_fails.clear ();
}

/* ------------------------------------------------------------ conversation */

/* Mark every character the player can currently see as "seen" (player-centric
   clsCharacter.HasSeenCharacter, set each turn by clsUserSession's
   PrepareForNextTurn).  The player is always considered self-seen. */
void
update_seen (a5_state_t *st)
{
  const char *ploc = a5state_player_location (st);
  int i;
  if (st->char_seen == NULL)
    return;
  for (i = 0; i < st->adv->n_characters; i++)
    {
      if (streq (st->adv->characters[i].key, a5state_player_key (st)))
        { st->char_seen[i] = 1; continue; }
      /* Resolve through the on/in-object chain, not char_loc alone -- a
         character whose model start is "On Object" (GFS's Grandpa on his
         rocking chair) has char_loc == NULL and is located via the
         furniture. */
      if (ploc != NULL && a5state_character_at_location (st, i, ploc))
        st->char_seen[i] = 1;
    }
  /* Mark every object currently visible in the player's room as seen
     (clsCharacter.HasSeenObject), so HaveBeenSeenByCharacter persists once an
     object has been viewed. */
  if (st->obj_seen != NULL && ploc != NULL)
    for (i = 0; i < st->adv->n_objects; i++)
      if (a5state_object_visible_at_location (st, i, ploc, 0))
        st->obj_seen[i] = 1;
  /* Catch-all for player moves that bypass the MoveCharacter action (event
     moves, walks): the location the player ends the turn in has been seen. */
  a5state_mark_loc_seen (st, ploc);
}

/* Mark the destination location, its objects and its visible characters as seen
   the MOMENT the player arrives (clsCharacter.Move, clsCharacter.vb:524-533):
   the runner updates HasSeenObject/-Character/-Location at move time -- BEFORE the room
   render and BEFORE the movement's AfterTextAndActions overrides run -- so a
   same-turn `Must HaveBeenSeenByCharacter %Player%` restriction driven by such an
   override already sees the arrival.  (update_seen alone only refreshes at turn
   boundaries, which lags a discovery that a movement-override task tests in the
   same turn -- e.g. Alien Diver's ResetRollC -> CheckIfPla1 setting Shipfound as
   soon as the player walks onto the crashed ship.)  Player-centric, like the rest
   of our obj_seen/char_seen sets; only marks on an AtLocation arrival, matching
   the runner's `ToWhere.ExistWhere = AtLocation` gate. */
void
mark_player_arrival_seen (a5_state_t *st, const char *loc)
{
  int i;
  a5state_mark_loc_seen (st, loc);
  if (loc == NULL)
    return;
  if (st->obj_seen != NULL)
    for (i = 0; i < st->adv->n_objects; i++)
      if (a5state_object_visible_at_location (st, i, loc, 0))
        st->obj_seen[i] = 1;
  if (st->char_seen != NULL)
    for (i = 0; i < st->adv->n_characters; i++)
      if (!streq (st->adv->characters[i].key, a5state_player_key (st))
          && a5state_character_at_location (st, i, loc))
        st->char_seen[i] = 1;
}

/* The conversation layer (FindConversationNode / ExecuteConversation, the
   ContainsWord keyword test and the partner-walked-away teardown) lives in
   a5run_conv.cpp; its entry points are declared in a5run_internal.h. */

/* clsCharacter.Move (Player branch): when the Player moves into a new location,
   arm every System task whose <LocationTrigger> is that location and that may
   still run (Repeatable or not yet Completed), by enqueuing it onto
   qTasksToRun.  The queue is drained after the turn's task, before the event
   tick (ev_tick_all).  `old_loc` is where the Player was *before* the move. */
static void
enqueue_loc_trigger_tasks (a5_run_t *run, const char *old_loc, const char *new_loc)
{
  a5_state_t *st = run->st;
  int i;
  if (new_loc == NULL || streq (old_loc, new_loc))
    return;
  for (i = 0; i < st->adv->n_tasks; i++)
    {
      const a5_task_t *t = &st->adv->tasks[i];
      int ti;
      if (!streq (t->type, "System") || !streq (t->location_trigger, new_loc))
        continue;
      ti = a5state_task_index (st, t->key);
      if (!t->repeatable && ti >= 0 && st->task_done[ti])
        continue;
      run->tasks_to_run->push_back (t->key);
    }
}

/* ----------------------------------------------------------- action: execute */

/* ---- run_action per-kind handlers (extracted from run_action) ---- */

/* clsUserSession.vb:1479 MoveObjectWhat: the source of an object-set action is
   either a single Object or one of the "Everything*" sets (a group's members,
   everything held/worn by a character, inside/on an object, at a location, or
   carrying a property).  MoveObject and AddObjectToGroup/RemoveObjectFromGroup
   share the enum, so they share this collector.  Returns 0 for an unrecognised
   selector (the caller then does nothing, as the runner does). */
static int
collect_object_source (a5_state_t *st, const std::string &what,
                       const char *srckey, const char *rawkey,
                       std::vector<int> &targets)
{
  if (what == "Object")
    {
      int oi = a5state_object_index (st, srckey);
      if (oi >= 0) targets.push_back (oi);
    }
  else if (what == "EverythingInGroup")
    {
      /* Walk the group's LIVE, insertion-ordered membership (the runner
         arlMembers), not the model's object table: the runner hands the set to
         the action in member order, and that order is observable whenever the
         objects are appended to another group -- Quest Giver stamps its deck's
         card IDs by member position, so a model-order copy deals the wrong card
         for a given roll.  a5state_group_member_at IS the merged view (a
         RemoveObjectFromGroup drops the member even though its static <Member>
         entry survives). */
      int n = a5state_group_count (st, srckey);
      for (int m = 0; m < n; m++)
        {
          int oi = a5state_object_index (st,
                     a5state_group_member_at (st, srckey, m));
          if (oi >= 0) targets.push_back (oi);
        }
    }
  else if (what == "EverythingHeldBy" || what == "EverythingWornBy")
    {
      a5_owhere_t w = (what == "EverythingHeldBy") ? A5_OWHERE_HELD_BY
                                                   : A5_OWHERE_WORN_BY;
      for (int i = 0; i < st->adv->n_objects; i++)
        if (st->obj[i].where == w && streq (st->obj[i].key, srckey))
          targets.push_back (i);
    }
  else if (what == "EverythingInside" || what == "EverythingOn")
    {
      a5_owhere_t w = (what == "EverythingInside") ? A5_OWHERE_IN_OBJECT
                                                   : A5_OWHERE_ON_OBJECT;
      for (int i = 0; i < st->adv->n_objects; i++)
        if (st->obj[i].where == w && streq (st->obj[i].key, srckey))
          targets.push_back (i);
    }
  else if (what == "EverythingAtLocation")
    {
      for (int i = 0; i < st->adv->n_objects; i++)
        if (st->obj[i].where == A5_OWHERE_LOCATION
            && streq (st->obj[i].key, srckey))
          targets.push_back (i);
    }
  else if (what == "EverythingWithProperty")
    {
      /* Merged (runtime + static) test, matching clsObject.HasProperty: a
         property added at runtime via SetProperty must be seen here, exactly as
         the character EveryoneWithProperty branch already does. */
      for (int i = 0; i < st->adv->n_objects; i++)
        if (a5state_entity_has_prop (st, st->adv->objects[i].key, rawkey))
          targets.push_back (i);
    }
  else
    return 0;
  return 1;
}

static void
act_move_object (a5_run_t *run, const char * /*kind*/,
                 const std::vector<std::string> &tk, const char * /*body*/,
                 int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
  if (tk.size () < 4)
    return;
  /* tk[0] is the source selector, tk[1] the source entity; tk[2] is the
     destination kind and tk[3] the destination key.  Collect the affected
     object indices, then apply the same per-object move to each. */
  const std::string &what = tk[0];
  const char *srckey = act_key (st, tk[1].c_str ());
  const std::string &to = tk[2];
  const char *k2 = act_key (st, tk[3].c_str ());
  std::vector<int> targets;

  if (!collect_object_source (st, what, srckey, tk[1].c_str (), targets))
    return;

  for (int oi : targets)
    {
      a5_objloc_t *L = &st->obj[oi];
      if (to == "ToCarriedBy")      { L->where = A5_OWHERE_HELD_BY;   L->key = k2; }
      else if (to == "ToWornBy")    { L->where = A5_OWHERE_WORN_BY;   L->key = k2; }
      else if (to == "OntoObject")  { L->where = A5_OWHERE_ON_OBJECT; L->key = k2; }
      else if (to == "InsideObject"){ L->where = A5_OWHERE_IN_OBJECT; L->key = k2; }
      else if (to == "ToPartOfObject"){ L->where = A5_OWHERE_PART_OBJECT; L->key = k2; }
      else if (to == "ToPartOfCharacter"){ L->where = A5_OWHERE_PART_CHAR; L->key = k2; }
      else if (to == "ToLocation")
        { if (streq (k2, "Hidden")) { L->where = A5_OWHERE_HIDDEN; L->key = NULL; }
          else { L->where = A5_OWHERE_LOCATION; L->key = k2; } }
      else if (to == "ToLocationGroup")
        {
          /* The runner MoveObject->ToLocationGroup (clsUserSession.vb:1560): a STATIC
             object occupies the WHOLE group; a DYNAMIC object lands in ONE
             random member room (group.RandomKey).  AlienDiver scatters the
             crashed ship / sea creatures this way -- Seacreatur4 (dynamic) to
             a random ocean room, then the ship ToSameLocationAs it -- so the
             ship "must be located" and exit-ship can map back to a single
             room.  Setting a dynamic object to the whole group left the ship
             nowhere-in-particular and broke exit-ship. */
          if (L->is_static) { L->where = A5_OWHERE_LOCGROUP; L->key = k2; }
          else
            {
              int n = a5state_group_count (st, k2);
              if (n > 0)
                {
                  const char *m = a5state_group_member_at
                    (st, k2, a5rand_between (0, n - 1));
                  const a5_location_t *ld = a5model_location (st->adv, m);
                  L->where = A5_OWHERE_LOCATION; L->key = ld ? ld->key : m;
                }
              else { L->where = A5_OWHERE_HIDDEN; L->key = NULL; }
            }
        }
      else if (to == "ToSameLocationAs")
        {
          /* The target may be a character or an object (clsUserSession.vb:1570).
             Character: place the object in the character's room (the common
             "drop" case).  Object: copy the target object's full location
             (where + key) — e.g. eating "four food rations" reveals the hidden
             "three food rations" inside the same backpack. */
          int ci = a5state_character_index (st, k2);
          if (ci >= 0)
            {
              /* Resolve through the carrier chain, not char_loc alone: a
                 follower riding the player (OntoCharacter) has char_loc NULL
                 and its room resolves through the carrier -- Symphonica's
                 `attack ickle spider with foot` drops the squashed arachnid
                 ToSameLocationAs the spider *while it rides the player*, so
                 the raw char_loc would hide the ingredient forever. */
              const char *loc = a5state_character_location_key (st, ci);
              if (loc != NULL) { L->where = A5_OWHERE_LOCATION; L->key = loc; }
              else             { L->where = A5_OWHERE_HIDDEN;   L->key = NULL; }
            }
          else
            {
              int oj = a5state_object_index (st, k2);
              if (oj >= 0) { L->where = st->obj[oj].where; L->key = st->obj[oj].key; }
              else         { L->where = A5_OWHERE_HIDDEN;  L->key = NULL; }
            }
        }
    }
}

static void
act_object_group (a5_run_t *run, const char *kind,
                  const std::vector<std::string> &tk, const char * /*body*/,
                  int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
  /* "Object <obj> ToGroup <grp>" / "Object <obj> FromGroup <grp>"
     (clsAction AddObjectToGroup/RemoveObjectFromGroup): runtime membership,
     e.g. the flashlight joins LightSources while switched on so the darkness
     override's "MustNot BeInSameLocationAsObject LightSources" sees it.
     The bulk "EverythingWithProperty <prop> ToGroup <grp>" form sweeps every
     object carrying <prop> (The Salvage's startup seeds its Salvageable
     group this way before fanning placement out over the members). */
  if (tk.size () < 4)
    return;
  int add = streq (kind, "AddObjectToGroup");
  const char *grp = tk[3].c_str ();
  const char *srckey = act_key (st, tk[1].c_str ());
  std::vector<int> targets;
  /* The selector enum is MoveObject's (clsAction shares AddToGroupWhat with
     MoveObjectWhat), so the whole "Everything*" family is legal here too --
     Quest Giver's setup task seeds its live deck with
     `AddObjectToGroup EverythingInGroup daz6QuestDeckM ToGroup daz6CurrentQue`,
     and with only the single-Object form implemented the deck stayed at its
     three authored members, so the game dealt from 3 cards instead of 20. */
  if (!collect_object_source (st, tk[0], srckey, tk[1].c_str (), targets))
    return;
  for (int oi : targets)
    a5state_set_object_in_group (st, grp, st->adv->objects[oi].key, add);
}

/* The "who" selectors shared by MoveCharacter and Add/RemoveCharacterToGroup
   (clsAction reuses MoveCharacterWhoEnum, clsUserSession.vb:1689/1841): collect
   the affected character indices.  Returns 0 for a selector it does not cover
   -- EveryoneWithProperty's presence test differs between the two actions, so
   it stays with each caller. */
static int
collect_character_who (a5_state_t *st, const std::string &who, const char *whok,
                       std::vector<int> &cis)
{
  if (who == "Character")
    { int ci = a5state_character_index (st, whok); if (ci >= 0) cis.push_back (ci); }
  else if (who == "EveryoneAtLocation")
    {
      /* clsLocation.CharactersDirectlyInLocation(True): directly at the
         location (not on/in an object), Player included. */
      for (int i = 0; i < st->adv->n_characters; i++)
        if (streq (st->char_loc[i], whok) && st->char_onobj[i] == NULL)
          cis.push_back (i);
    }
  else if (who == "EveryoneInGroup")
    {
      int n = a5state_group_count (st, whok);
      for (int m = 0; m < n; m++)
        { int ci = a5state_character_index (st,
                      a5state_group_member_at (st, whok, m));
          if (ci >= 0) cis.push_back (ci); }
    }
  else if (who == "EveryoneInside" || who == "EveryoneOn")
    {
      char want_in = (who == "EveryoneInside") ? 1 : 0;
      for (int i = 0; i < st->adv->n_characters; i++)
        if (streq (st->char_onobj[i], whok) && st->char_in[i] == want_in)
          cis.push_back (i);
    }
  else
    return 0;
  return 1;
}

/* Add/RemoveCharacterToGroup (clsUserSession.vb:1841): same shape as the object
   and location group actions, over MoveCharacter's "who" enum.  Quest Giver
   files each hired adventurer into its "Current Actively Hired Adventurers"
   group, which is what VIEW HIRED enumerates. */
static void
act_character_group (a5_run_t *run, const char *kind,
                     const std::vector<std::string> &tk, const char * /*body*/,
                     int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
  if (tk.size () < 4)
    return;
  const std::string &who = tk[0];
  const char *whok = act_key (st, tk[1].c_str ());
  const char *grp = tk[3].c_str ();
  int add = streq (kind, "AddCharacterToGroup");
  std::vector<int> cis;
  if (who == "EveryoneWithProperty")
    {
      for (int i = 0; i < st->adv->n_characters; i++)
        if (a5state_entity_has_prop (st, st->adv->characters[i].key,
                                     tk[1].c_str ()))
          cis.push_back (i);
    }
  else if (!collect_character_who (st, who, whok, cis))
    return;
  for (int ci : cis)
    a5state_set_object_in_group (st, grp, st->adv->characters[ci].key, add);
}

/* Move character CI to room NEW_LOC (AtLocation), with the player-move
   bookkeeping in the runner's order: the LocationTrigger enqueue against the
   old room first, then the assignment, then the conversation-partner check.
   CLEAR_ONOBJ also drops any on/in-object binding before the partner check
   (the "now at location" destinations); the furniture-sync callers keep the
   binding they just set. */
static void
relocate_char (a5_run_t *run, int ci, const char *new_loc, int clear_onobj,
               sb_t *out)
{
  a5_state_t *st = run->st;
  int is_player = streq (st->adv->characters[ci].key, a5state_player_key (st));
  if (is_player)
    enqueue_loc_trigger_tasks (run, st->char_loc[ci], new_loc);
  st->char_loc[ci] = new_loc;
  if (clear_onobj)
    st->char_onobj[ci] = NULL;
  if (is_player)
    clear_conv_if_partner_gone (run, out);
}

/* The room that hosts object `objkey`.  A character seated on / put inside an
   object has its location keyed off that object (the runner's
   clsCharacterLocation.LocationKey follows it), so this is what the
   furniture/container branches sync char_loc to.  `cur` (the character's
   current room, or NULL for a plain first-match scan) wins when the object is
   present there too: a static placed at a location GROUP exists at many rooms
   (AoK's cell floor, Group74), and a bare first-match scan would teleport the
   player out of the cell on `lie down`.  NULL if the object is nowhere. */
static const char *
object_host_location (a5_state_t *st, const char *objkey, const char *cur,
                      int directly)
{
  int oj = a5state_object_index (st, objkey);
  if (oj < 0)
    return NULL;
  if (cur != NULL && a5state_object_at_location (st, oj, cur, directly))
    return cur;
  for (int li = 0; li < st->adv->n_locations; li++)
    if (a5state_object_at_location (st, oj, st->adv->locations[li].key, directly))
      return st->adv->locations[li].key;
  return NULL;
}

static void
act_move_character (a5_run_t *run, const char * /*kind*/,
                    const std::vector<std::string> &tk, const char * /*body*/,
                    int /*depth*/, sb_t *out)
{
  a5_state_t *st = run->st;
  if (tk.size () < 3)
    return;
  /* The "who" set: a single Character, or one of the runner's bulk source forms
     (clsAction.MoveCharacterWhoEnum, clsUserSession.vb:1689) -- e.g. the
     wand-teleport `EveryoneAtLocation Location33 ToLocation Location34`.
     Token layout is identical to MoveObject: tk[0]=who, tk[1]=who-key,
     tk[2]=to, tk[3]=to-key. */
  const std::string &who = tk[0];
  const char *whok = act_key (st, tk[1].c_str ());
  std::vector<int> cis;
  if (who == "EveryoneWithProperty")
    {
      for (int i = 0; i < st->adv->n_characters; i++)
        if (a5_prop_find (st->adv->characters[i].props,
                          st->adv->characters[i].n_props, whok) != NULL
            || a5state_entity_prop (st, st->adv->characters[i].key, whok) != NULL)
          cis.push_back (i);
    }
  else if (!collect_character_who (st, who, whok, cis))
    return;
  if (cis.empty ())
    return;

  const std::string &to = tk[2];
  /* Every MoveCharacterTo variant that takes a target (location, group,
     furniture, container, character, direction) reads it from the same token;
     act_key is a pure lookup, so resolve it once for the whole action. */
  const char *k2 = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
  /* ToLocationGroup: the runner computes dest.Key = group.RandomKey ONCE per action
     (clsUserSession.vb:1767), so the whole MoveCharacter draws a single
     RandomKey and sends every affected character to the SAME room -- e.g.
     Jacaranda's champagne `MoveCharacter %Player% ToLocationGroup Group7`. */
  const char *group_dest = NULL;
  if (to == "ToLocationGroup" && tk.size () >= 4)
    {
      /* Read the group's LIVE membership (the runner arlMembers), not the static
         <Member> list: procedural games (Skybreak) populate the destination
         group at runtime via AddLocationToGroup right before the jump, so a
         static read finds 0 members and the player lands nowhere. */
      int n = a5state_group_count (st, k2);
      if (n > 0)
        {
          const char *m = a5state_group_member_at (st, k2,
                                                   a5rand_between (0, n - 1));
          /* Canonicalise to the stable model location key: the gm entry is
             an owned copy that is freed when the group is cleared later this
             turn (the post-jump EverywhereInGroup sweep), so storing it in
             char_loc would dangle. */
          const a5_location_t *ld = a5model_location (st->adv, m);
          group_dest = ld ? ld->key : m;
        }
    }
  for (int ci : cis)
    {
      const char *k1 = st->adv->characters[ci].key;
      /* Every MoveCharacter but OntoCharacter re-establishes the character's
         clsCharacterLocation from scratch (FD's fresh `dest` -> ch.Move), so a
         character that was riding a carrier stops riding it the moment it is
         sent anywhere else -- Penrhyn drops Arkell to Location2/Hidden once he
         is broken, and he must no longer be "on Ralph's shoulder". */
      if (to != "OntoCharacter" && st->char_onchar != NULL)
        st->char_onchar[ci] = NULL;
      if (to == "ToLocationGroup")
        relocate_char (run, ci, group_dest, 1, out);
      else if (to == "InDirection")
        {
          const char *canon = a5parse_canonical_direction (k2);
          const char *here = st->char_loc[ci];
          const char *dest;
          if (canon == NULL) canon = k2;    /* already canonical */
          dest = here ? a5restr_exit_in_direction (st, k1, here, canon, NULL)
                      : NULL;
          if (dest != NULL)
            {
              /* destination may be a location group -> first member */
              if (a5model_location (st->adv, dest) == NULL)
                {
                  for (int g = 0; g < st->adv->n_groups; g++)
                    if (streq (st->adv->groups[g].key, dest)
                        && st->adv->groups[g].n_members > 0)
                      { dest = st->adv->groups[g].members[0]; break; }
                }
              relocate_char (run, ci, dest, 1, out);   /* now "at location" */
            }
        }
      else if (to == "ToLocation")
        {
          const char *new_loc = streq (k2, "Hidden") ? NULL : k2;
          relocate_char (run, ci, new_loc, 1, out);    /* now "at location" */
        }
      else if (to == "ToStandingOn" || to == "ToSittingOn" || to == "ToLyingOn")
        {
          const char *pos = (to == "ToSittingOn") ? "Sitting"
                          : (to == "ToLyingOn")   ? "Lying" : "Standing";
          free (st->char_position[ci]);
          st->char_position[ci] = strdup (pos);
          a5state_set_prop (st, k1, "CharacterPosition", pos);
          /* "TheFloor" means standing/sitting/lying at the location, not on an
             object (clsCharacterLocation.ExistsWhere AtLocation vs OnObject). */
          if (k2 == NULL || streq (k2, "TheFloor"))
            st->char_onobj[ci] = NULL;
          else
            {
              st->char_onobj[ci] = k2; st->char_in[ci] = 0;
              /* Seating someone on a chair in ANOTHER room moves them there --
                 GFS's John Boom "invites you in" seats the player on the
                 living-room cosy chair.  Scarier stores the room explicitly:
                 sync it. */
              const char *loc = object_host_location (st, k2, st->char_loc[ci], 1);
              if (loc != NULL && !streq (loc, st->char_loc[ci]))
                relocate_char (run, ci, loc, 0, out);
            }
        }
      else if (to == "InsideObject" || to == "OntoObject")
        {
          /* clsUserSession MoveCharacterToEnum.InsideObject / OntoObject:
             the character is now in/on the object
             (clsCharacterLocation.ExistsWhere InsideObject/OnObject).
             char_in distinguishes the two so BeInsideObject / BeOnObject
             read it back correctly -- e.g. FBA's `hide in niche`
             (MoveCharacter Player InsideObject cl_Niche1) which the custodian
             "goes past" check gates on. */
          if (k2 != NULL)
            {
              st->char_onobj[ci] = k2;
              st->char_in[ci] = (to == "InsideObject") ? 1 : 0;
              /* Putting someone inside an object in ANOTHER room moves them
                 there -- Axe of Kolt sends Grat from his burrow (Loc112)
                 into the loo hut (Object358 @Loc107); leaving char_loc at
                 the burrow made the Caught-By-Grat death (gated on him
                 being AT Loc112) fire while he was in the loo.  Sync it,
                 like the sit-on-furniture branch above. */
              const char *loc = object_host_location (st, k2, st->char_loc[ci], 0);
              if (loc != NULL && !streq (loc, st->char_loc[ci]))
                relocate_char (run, ci, loc, 0, out);
            }
        }
      else if (to == "OntoCharacter")
        {
          /* clsUserSession MoveCharacterToEnum.OntoCharacter
             (clsUserSession.vb:1884): the character now rides ANOTHER character
             (clsCharacterLocation.ExistsWhere OnCharacter, Key = carrier), the
             dynamic twin of the "On Character" start state the loader decodes
             into char_onchar (Edith rides the Player).  Penrhyn's ArkellFoll2
             System task runs `MoveCharacter Arkell ToSameLocationAs %Player%`
             then `... OntoCharacter %Player%` every turn -- the second action
             overrides the first, so Arkell perches on Ralph's shoulder and its
             room resolves through the carrier.  FD guards against a recursive
             placement (target is self or already riding this character) and
             leaves the character put in that case. */
          int ti = k2 ? a5state_character_index (st, k2) : -1;
          int recursive = (ti >= 0)
            && (streq (k2, k1)
                || (st->char_onchar[ti] != NULL
                    && streq (st->char_onchar[ti], k1)));
          if (ti >= 0 && !recursive)
            {
              st->char_onchar[ci] = k2;
              st->char_loc[ci] = NULL;      /* location resolves through carrier */
              st->char_onobj[ci] = NULL;
              st->char_in[ci] = 0;
            }
        }
      else if (to == "ToParentLocation")
        {
          /* clsUserSession MoveCharacterToEnum.ToParentLocation: drop out of
             the containing object back onto the floor of the SAME location
             (clsCharacter.Move ... the character's location is unchanged, only
             the on/in-object binding clears) -- FBA's `out` of the niche. */
          st->char_onobj[ci] = NULL;
          st->char_in[ci] = 0;
        }
      else if (to == "ToSameLocationAs")
        {
          /* Place the character in the same place as the target character or
             object (clsUserSession.vb:1777).  Character target: copy its
             ExistWhere/Key (location + on/in-furniture) -- this is how Alan's
             "follow" task `MoveCharacter Character8 ToSameLocationAs %Player%`
             brings him along on a teleport.  Object target: the object's
             containing location. */
          int ti = k2 ? a5state_character_index (st, k2) : -1;
          const char *old_loc = st->char_loc[ci];
          if (ti >= 0)
            {
              st->char_loc[ci]   = st->char_loc[ti];
              st->char_onobj[ci] = st->char_onobj[ti];
              st->char_in[ci]    = st->char_in[ti];
            }
          else
            {
              /* NULL => Hidden */
              st->char_loc[ci] = (k2 != NULL)
                ? object_host_location (st, k2, NULL, 1) : NULL;
              st->char_onobj[ci] = NULL;
            }
          if (streq (k1, a5state_player_key (st)))
            {
              enqueue_loc_trigger_tasks (run, old_loc, st->char_loc[ci]);
              clear_conv_if_partner_gone (run, out);
            }
        }
      else if (to == "ToSwitchWith")
        {
          /* clsUserSession MoveCharacterToEnum.ToSwitchWith: swap character k1
             with k2.  When either is the current player, DON'T move anyone --
             change which character IS the player (ADRIFT's BECOME <character>
             mechanism).  The player viewpoint, %Player% resolution and scope
             then follow the new character; the old player stays put, now an
             NPC.  Otherwise, actually exchange the two characters' locations. */
          const char *pk = a5state_player_key (st);
          int bi = k2 ? a5state_character_index (st, k2) : -1;
          if (k2 == NULL || bi < 0)
            { /* unresolved target: no-op */ }
          else if (streq (k1, pk) || streq (k2, pk))
            {
              const char *old_loc = a5state_player_location (st);
              int new_ci = streq (k1, pk) ? bi : ci;
              const char *new_player = st->adv->characters[new_ci].key;
              st->player_key = new_player;   /* stable model key pointer */
              /* HasSeenObject/-Location/-Character are per-character in the runner:
                 the new viewpoint must not inherit the old player's
                 sightings (BugHunt: Jones must not "have seen" the elevator
                 sign that Davey saw). */
              a5state_switch_seen (st, new_ci);
              enqueue_loc_trigger_tasks (run, old_loc,
                                         a5state_player_location (st));
              clear_conv_if_partner_gone (run, out);
              a5state_mark_loc_seen (st, a5state_player_location (st));
            }
          else
            {
              /* Neither is the player: exchange the two characters' places. */
              const char *tloc = st->char_loc[bi];
              const char *tonobj = st->char_onobj[bi];
              char tin = st->char_in[bi];
              st->char_loc[bi]   = st->char_loc[ci];
              st->char_onobj[bi] = st->char_onobj[ci];
              st->char_in[bi]    = st->char_in[ci];
              st->char_loc[ci]   = tloc;
              st->char_onobj[ci] = tonobj;
              st->char_in[ci]    = tin;
            }
        }
      /* other MoveCharacterToEnum forms: best-effort no-op */

      /* clsCharacter.Move marks the destination location -- and, on an
         AtLocation arrival, its objects and visible characters -- seen for
         the moving character at move time; our set is player-centric (like
         obj_seen).  See mark_player_arrival_seen. */
      if (streq (k1, a5state_player_key (st)))
        {
          if (st->char_onobj[ci] == NULL)
            mark_player_arrival_seen (st, st->char_loc[ci]);
          else
            a5state_mark_loc_seen (st, st->char_loc[ci]);
        }
    }
}

/* Store `val` into a Text variable, honouring an array subscript.  A Text array
   keeps its elements newline-separated inside the one var_text string (the
   InitialValue layout that expr_substitute reads back), so an element store has
   to split, pad to the declared length, replace, and rejoin -- assigning the
   whole string would wipe the siblings.  elem <= 0 or a non-array variable
   overwrites wholesale, as before.  Takes ownership of `val`. */
static void
store_text_var (a5_state_t *st, int vi, long elem, char *val)
{
  long len = st->adv->variables[vi].array_length;
  if (len <= 1 || elem <= 0)
    {
      free (st->var_text[vi]);
      st->var_text[vi] = val;
      return;
    }
  if (elem > len)
    len = elem;
  std::vector<std::string> lines ((size_t) len);
  {
    const char *s = st->var_text[vi] != NULL ? st->var_text[vi] : "";
    for (long i = 0; i < len && *s != '\0'; i++)
      {
        const char *nl = strchr (s, '\n');
        lines[(size_t) i].assign (s, nl != NULL ? (size_t) (nl - s) : strlen (s));
        if (nl == NULL)
          break;
        s = nl + 1;
      }
  }
  lines[(size_t) elem - 1] = val != NULL ? val : "";
  free (val);
  std::string joined;
  for (long i = 0; i < len; i++)
    {
      if (i > 0)
        joined += '\n';
      joined += lines[(size_t) i];
    }
  free (st->var_text[vi]);
  st->var_text[vi] = strdup (joined.c_str ());
}

static void
act_set_variable (a5_run_t *run, const char *kind,
                  const std::vector<std::string> & /*tk*/, const char *body,
                  int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  std::string name, value, idxstr;
  int vi;
  /* "FOR <var> = a TO b : SET <assignment> : NEXT <var>" (FileIO.vb:365-374,
     the SetVariable twin of act_set_tasks' Execute loop).  Unlike SetTasks,
     the counter IS visible to the body: the runner substitutes it wherever the
     loop variable appears, which in practice is the element index
     (`SET Points[Loop] = 0`) and the %Loop% reference.  OS/PlugIn.Exe deals
     its blackjack hand with `FOR Loop = 1 TO 3 : SET Bjvalue[Loop] =
     %cardValue[RAND(1, 13)]% : NEXT Loop`, so without this the whole hand
     stays 0 and every card renders as "Zero of ...". */
  {
    std::string b = body;
    size_t bs = b.find_first_not_of (" \t");
    if (bs != std::string::npos && strncasecmp (b.c_str () + bs, "FOR ", 4) == 0)
      {
        size_t c1 = b.find (':', bs);
        size_t c2 = b.rfind (':');
        if (c1 != std::string::npos && c2 != std::string::npos && c2 > c1)
          {
            std::string head = b.substr (bs + 4, c1 - bs - 4);
            std::string inner = b.substr (c1 + 1, c2 - c1 - 1);
            size_t eq = head.find ('=');
            size_t tp = head.find (" TO ");
            if (eq != std::string::npos && tp != std::string::npos && tp > eq)
              {
                std::string lv = head.substr (0, eq);
                while (!lv.empty () && isspace ((unsigned char) lv.back ()))
                  lv.pop_back ();
                size_t a = lv.find_first_not_of (" \t");
                lv = (a == std::string::npos) ? "" : lv.substr (a);
                long from = strtol (head.c_str () + eq + 1, NULL, 10);
                long to   = strtol (head.c_str () + tp + 4, NULL, 10);
                a = inner.find_first_not_of (" \t");
                if (a != std::string::npos) inner = inner.substr (a);
                while (!inner.empty () && isspace ((unsigned char) inner.back ()))
                  inner.pop_back ();
                if (strncasecmp (inner.c_str (), "SET ", 4) == 0)
                  inner = inner.substr (4);
                if (!lv.empty () && !inner.empty ()
                    && depth < A5_MAX_ACTION_DEPTH)
                  {
                    std::string pct = "%" + lv + "%", brk = "[" + lv + "]";
                    for (long l = from; l <= to && !st->game_over; l++)
                      {
                        char num[24];
                        snprintf (num, sizeof num, "%ld", l);
                        std::string it = inner, rep;
                        size_t p;
                        while ((p = it.find (pct)) != std::string::npos)
                          it.replace (p, pct.size (), num);
                        rep = std::string ("[") + num + "]";
                        while ((p = it.find (brk)) != std::string::npos)
                          it.replace (p, brk.size (), rep);
                        run_action (run, kind, it.c_str (), depth + 1, out);
                      }
                  }
                return;
              }
          }
      }
  }
  if (!split_assignment (body, name, value))
    return;
  /* The runner's action parser splits the LHS at '[' (FileIO.vb: sKey1 =
     sElements(0).Split("[")(0), sKey2 = the bracketed index).  The index is
     ignored for a Length-1 variable (clsUserSession.vb:2135, IntValue
     defaults to 1) -- AoK's push-doors task writes
     "Variable330[Hidden] = ""1""" and the lookup must see Variable330.
     For an ArrayLength>1 variable it selects the element to assign
     (WW2 Elevator Escape's cl_Buttonarra[cl_Variable1] = "1"). */
  size_t br = name.find ('[');
  if (br != std::string::npos)
    {
      size_t cb = name.find (']', br);
      idxstr = name.substr (br + 1, (cb == std::string::npos)
                                        ? std::string::npos : cb - br - 1);
      name.erase (br);
      while (!name.empty () && isspace ((unsigned char) name.back ()))
        name.pop_back ();
    }
  vi = a5state_variable_index (st, name.c_str ());
  if (vi < 0) return;
  /* Resolve the element index like clsUserSession.vb:2136-2144: a
     "ReferencedNumber" prefix reads the parser's number reference, a
     numeric literal is used as-is, anything else is a variable KEY whose
     (element-1) value is the index. */
  long elem_idx = 1;
  if (st->adv->variables[vi].array_length > 1 && !idxstr.empty ())
    {
      if (idxstr.compare (0, 16, "ReferencedNumber") == 0)
        {
          const char *rn = a5state_lookup_ref (st, "ReferencedNumber");
          elem_idx = rn != NULL ? strtol (rn, NULL, 10) : 0;
        }
      else if (idxstr.find_first_not_of ("0123456789 -") == std::string::npos)
        elem_idx = strtol (idxstr.c_str (), NULL, 10);
      else
        {
          int ivi = a5state_variable_index (st, idxstr.c_str ());
          elem_idx = ivi >= 0 ? a5state_var_get_elem (st, ivi, 1) : 0;
        }
    }
  /* A subscript-less assignment to a Text array keeps the historic wholesale
     overwrite (it is how the newline-joined InitialValue is set en bloc); only
     an explicit `Var[i] =` replaces a single element. */
  long text_elem = idxstr.empty () ? 0 : elem_idx;
  if (streq (st->adv->variables[vi].type, "Text") && streq (kind, "SetVariable"))
    {
      /* The runner evaluates the RHS as an expression, so a bare string-literal value
         unwraps to its contents.  ADRIFT writes such literals with their own
         quotes inside the assignment's delimiters (Playeraxe = ""an"" or
         = "'an'"); split_assignment stripped one outer layer, leaving "an" /
         'an'.  Strip a remaining fully-surrounding matched quote pair so
         %playeraxe% renders `an`, not `'an'`/`"an"` (Tingalan inventory).
         Only when it is a lone literal (no interior same-quote), so a
         concatenation like "a" & "b" is left for a5text_process untouched. */
      std::string tv = value;
      if (dbg_setvar ())
        fprintf (stderr, "[SETVAR] %s = <<%s>>\n", name.c_str (), tv.c_str ());
      /* A bare top-level function call ("UCASE(%text%)", Skybreak's
         Playername1 assignment) is a real expression, not literal text --
         route it through the full evaluator so %text% resolves and
         quotes (expr_substitute's bExpression quoting) before UCASE runs,
         instead of falling into the plain %ref%-substitution below, which
         left the literal "UCASE(...)" wrapper in the stored string. */
      std::string call_name;
      if (a5_bare_function_call (tv, call_name) && a5sexpr_is_function (lower (call_name)))
        {
          char *ev = a5text_eval_expression (st, tv.c_str ());
          store_text_var (st, vi, text_elem, ev);
          return;
        }
      /* A string-concatenation expression -- a `+` operator outside any
         quoted literal, joining %functions%/%vars%/"literals" (the runner's
         SetToExpression is uniform: `+` on non-numeric operands concatenates,
         and its bExpression ReplaceFunctions drops the quotes around each
         substituted value).  LostCoastlines builds every generated place name
         this way: `%names[%pointer%]%+%space%+%types[%locationtype%]%`
         (%space% = " ") -> "cabal plain", and `"The"+%space%+...+"of"+...`.
         The plain %ref%-substitution path below would leave the literal `+`
         and the quote-stripped " " as `cabal+ +plain`. */
      {
        int in_q = 0;  char qc = 0;  int has_plus = 0;
        for (char c : tv)
          {
            if (in_q) { if (c == qc) in_q = 0; }
            else if (c == '"' || c == '\'') { in_q = 1; qc = c; }
            else if (c == '+') { has_plus = 1; break; }
          }
        if (has_plus)
          {
            char *ev = a5text_eval_expression (st, tv.c_str ());
            store_text_var (st, vi, text_elem, ev);
            return;
          }
      }
      /* The runner evaluates the RHS through SetToExpression, whose
         ReplaceFunctions (bExpression=True) substitutes every TEXT
         variable reference WITH surrounding quotes (Global.vb:1977).
         When the RHS is itself a single quoted literal, that nested
         quoting splits the literal and leaves a bare word token, so
         GetToken/classification throws "Bad token", the exception is
         swallowed (clsVariable.vb SetToExpression's blanket Try) and the
         assignment NEVER LANDS: the variable silently keeps its old
         value.  the virtual human sets Variable28 = "that dream %s/he%
         had" this way and the runner's %human_thinking% stays empty for the rest
         of the game.  Numeric variables substitute as bare digits and
         survive, so only a Text-variable reference kills the assignment.
         split_assignment already peeled the literal's quotes off `value`,
         so re-inspect the raw RHS for the quoted-literal shape. */
      {
        /* Which string does the runner's SetToExpression actually see?  Files
           newer than 5.0.32 (dFileVersion > 5.0000321, FileIO.vb:426)
           store the expression wrapped in one EXTRA quote layer that the runner
           peels at load -- split_assignment already mirrored that peel,
           so the runner's view is `tv`.  Older files (the virtual human,
           5.000017) are stored verbatim and the runner does NOT peel, so the runner's
           view is the raw RHS, quotes and all. */
        std::string rhs;
        double fver = (st->adv->version != NULL) ? atof (st->adv->version) : 0;
        if (fver > 5.0000321)
          rhs = tv;
        else
          {
            rhs = body;
            size_t eqp = rhs.find ('=');
            rhs = (eqp == std::string::npos) ? "" : rhs.substr (eqp + 1);
            size_t b = rhs.find_first_not_of (" \t");
            rhs = (b == std::string::npos) ? "" : rhs.substr (b);
            while (!rhs.empty () && isspace ((unsigned char) rhs.back ()))
              rhs.pop_back ();
          }
        if (rhs.size () >= 2 && (rhs[0] == '"' || rhs[0] == '\'')
            && rhs[rhs.size () - 1] == rhs[0]
            && rhs.find (rhs[0], 1) == rhs.size () - 1)
          {
            size_t sp = 1;
            while ((sp = rhs.find ('%', sp)) != std::string::npos
                   && sp + 1 < rhs.size () - 1)
              {
                size_t ep = rhs.find ('%', sp + 1);
                if (ep == std::string::npos || ep >= rhs.size () - 1)
                  break;
                std::string ref = rhs.substr (sp + 1, ep - sp - 1);
                int rvi;
                for (rvi = 0; rvi < st->adv->n_variables; rvi++)
                  if (st->adv->variables[rvi].name != NULL
                      && strcasecmp (st->adv->variables[rvi].name,
                                     ref.c_str ()) == 0)
                    break;                 /* The runner matches by NAME, any case */
                if (rvi < st->adv->n_variables
                    && streq (st->adv->variables[rvi].type, "Text"))
                  {
                    if (dbg_setvar ())
                      fprintf (stderr, "[SETVAR-DROP] %s = <<%s>>\n",
                               name.c_str (), rhs.c_str ());
                    return;                /* The runner: bad expression, no-op */
                  }
                sp = ep + 1;
              }
          }
      }
      if (tv.size () >= 2 && (tv[0] == '"' || tv[0] == '\'')
          && tv[tv.size () - 1] == tv[0]
          && tv.find (tv[0], 1) == tv.size () - 1)
        tv = tv.substr (1, tv.size () - 2);
      /* Store WITHOUT the display-time auto-capitalisation: the runner keeps the raw
         evaluated value (Playeraxe = "an") and only capitalises when it is
         rendered in sentence context.  a5text_process would cap the lone value
         at position 0 ("an" -> "An"), so %playeraxe% mid-sentence wrongly
         shows "You have An axe" instead of "an axe". */
      char *proc = a5text_process_nocap (st, tv.c_str ());
      store_text_var (st, vi, text_elem, proc);
    }
  else
    {
      /* clsUserSession.vb:2144: a task modifies the built-in Score at most
         once ever -- gated on clsTask.Scored, set on first Score change and
         never reset (persisted across save/restore).  Without this a
         repeatable scoring task, or one System scoring task Execute'd by two
         distinct non-repeatable tasks (FBA's cl_MakeLeash, reached via both
         `make a leash` cl_TieRopeToD1 and `tie rope to dog` cl_TieDogWith),
         re-awards its points.  Non-Score variables are never gated.

         The guard's other half: the runner only touches Score at all when a task
         is executing (`var.Key <> "Score" OrElse task IsNot Nothing`) --
         actions run from a conversation TOPIC or an event pass task =
         Nothing, so their Score changes silently never land (Thy
         Balconyman's intro/lips/haircut topics all IncVariable Score). */
      int is_score = streq (st->adv->variables[vi].key, "Score");
      if (is_score && run->cur_score_ti < 0)
        return;                           /* no owning task -- Score frozen */
      if (is_score)
        {
          if (st->task_scored[run->cur_score_ti])
            return;                       /* already scored -- suppress */
          st->task_scored[run->cur_score_ti] = 1;
        }
      long delta = eval_num_value (st, value.c_str ());
      /* A5_DEBUG_SCORE=<varkey> also traces a game's CUSTOM score
         variable (e.g. Murder Most Foul counts points in "Scor1"),
         without the once-only Score gate (debug print only).  */
      {
        const char *dbg = dbg_score ();
        if (dbg && !is_score && streq (st->adv->variables[vi].key, dbg))
          fprintf (stderr, "[score] turn=%d task=%s %s %s %ld (now %ld)\n",
                   st->turns,
                   run->cur_score_ti >= 0 ? st->adv->tasks[run->cur_score_ti].key : "?",
                   st->adv->variables[vi].key,
                   kind, delta, st->var_num[vi] + (streq (kind, "IncVariable") ? delta : 0));
      }
      if (is_score && dbg_score ())
        fprintf (stderr, "[score] turn=%d task=%s %s %ld (now %ld)\n",
                 st->turns,
                 run->cur_score_ti >= 0 ? st->adv->tasks[run->cur_score_ti].key : "?",
                 kind, delta, st->var_num[vi] + (streq (kind, "IncVariable") ? delta : 0));
      /* The runner stores through SetToExpression(sExpr, iIndex): Inc/Dec build
         "%name% +/- value", and %name% on an array variable reads element
         1 (GetToken's bare var- token), so the result lands at elem_idx
         but the BASE is always element 1 (== the var_num mirror). */
      long newval;
      if (streq (kind, "SetVariable"))      newval = delta;
      else if (streq (kind, "IncVariable")) newval = st->var_num[vi] + delta;
      else                                  newval = st->var_num[vi] - delta;
      a5state_var_set_elem (st, vi, elem_idx, newval);
    }
}

static void
act_set_property (a5_run_t *run, const char * /*kind*/,
                  const std::vector<std::string> &tk, const char * /*body*/,
                  int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
  if (tk.size () < 3) return;
  const char *k1 = act_key (st, tk[0].c_str ());
  std::string val;
  for (size_t i = 2; i < tk.size (); i++)
    { if (i > 2) val += " "; val += tk[i]; }
  /* clsUserSession SetProperties: an Integer/Text/StateList/ValueList
     property value is an expression run through EvaluateExpression
     (clsUserSession.vb:2010) -- e.g. "PCASE(%text%)" for the player's typed
     name, or "RAND (25, 50)" for a randomised item Value.  Key-typed
     properties (Object/Character/LocationKey) and SelectionOnly are stored
     verbatim (the runner's non-evaluating branches).

     the runner only reaches that EvaluateExpression on the branch where the property
     ALREADY EXISTS on the entity (`prop IsNot Nothing`); when a property is
     being freshly ADDED, only SelectionOnly/ObjectKey are handled and an
     Integer/Text/... value is left at its clone default -- never evaluated.
     So gate the value-type evaluation on current presence: force it for a
     present value-typed property (this is what draws the RNG for
     LostCoastlines' `SetProperty <obj> Value RAND(a,b)` world-gen block),
     and otherwise fall back to the %reference% heuristic so a bare
     key/state value -- or a value on a not-yet-present property (Illumina) --
     is stored raw (the runner's add-branch / Nothing->raw behaviour). */
  const a5_propdef_t *pd = a5model_propdef (st->adv, tk[1].c_str ());
  const char *pt = pd ? pd->type : NULL;
  int value_type = pt != NULL
                   && (streq (pt, "Integer") || streq (pt, "Text")
                       || streq (pt, "StateList") || streq (pt, "ValueList"));
  int has_prop = a5state_entity_prop (st, k1, tk[1].c_str ()) != NULL;
  /* The runner's SetProperties add-branch is asymmetric (clsUserSession.vb): an ABSENT
     property is cloned-and-added freely for OBJECTS (vb:1989), but for a
     CHARACTER only a SelectionOnly-<Selected> (vb:2044) or CharacterProperName
     (vb:2036) is added -- a value-typed property (Integer/Text/StateList/
     ValueList) set to a plain value on an absent slot is DebugPrinted and
     ignored (vb:2056).  Galen's Quest shrinks the player with `SetProperty
     %Player% MaxBulk 90`, but the player has no MaxBulk by default, so in the runner
     the property is never added and carry stays unlimited -- which is why the
     729-bulk iron key remains takeable while shrunk inside the miniature
     castle (where `cast grow on me` is blocked).  Mirror the no-op. */
  if (!has_prop && value_type
      && a5state_character_index (st, k1) >= 0
      && a5state_object_index (st, k1) < 0
      && tk[1] != "CharacterProperName")
    return;
  if ((value_type && has_prop) || val.find ('%') != std::string::npos)
    {
      char *ev = a5text_eval_expression (st, val.c_str ());
      if (ev != NULL && ev[0] != '\0') val = ev;
      free (ev);
    }
  /* A key-typed property (ObjectKey/CharacterKey/LocationKey) set to a bare
     reference sentinel -- "ReferencedObject", "ReferencedCharacter1", ... or
     "Player" -- must be resolved to the currently-bound entity KEY, the way
     the runner stores the resolved key (clsUserSession.SetProperties key branch) and
     not the sentinel word.  LMK's `equip %object%` runs `SetProperty %Player%
     EquippedWe ReferencedObject`; left raw, EquippedWe holds the literal
     "ReferencedObject" and never equals the weapon key the combat
     restrictions test (Knife / s_Sword / Nothing), so NO hit-or-miss subtask
     fires and `kill squid` falls through to NotUnderstood.  A literal key or
     "Nothing" is not a per-turn ref binding, so lookup returns NULL and the
     value is kept verbatim. */
  {
    const char *kt = pd ? pd->type : NULL;
    int is_key_prop = kt != NULL
                      && (streq (kt, "ObjectKey") || streq (kt, "CharacterKey")
                          || streq (kt, "LocationKey"));
    if (is_key_prop && val.find ('%') == std::string::npos
        && val != "Nothing")
      {
        const char *rk = (val == "Player")
                         ? a5state_player_key (st)
                         : a5state_lookup_ref (st, val.c_str ());
        if (rk != NULL && rk[0] != '\0') val = rk;
      }
  }
  a5state_set_prop (st, k1, tk[1].c_str (), val.c_str ());
  /* The runner derives an object's location live from its DynamicLocation property, so
     `SetProperty <obj> DynamicLocation Hidden` (Galen's Quest hides the meteor
     once the fountain has swallowed it, via GiveMeteor) immediately relocates
     it.  Scarier caches location in st->obj[], so mirror MoveObject's mapping
     here or the object lingers stale (a phantom meteorite in the player's
     hands).  Only DynamicLocation drives relocation; the keyed variants read
     their companion key property (runtime override, else the static value). */
  if (tk[1] == "DynamicLocation")
    {
      int oi = a5state_object_index (st, k1);
      if (oi >= 0)
        {
          a5_objloc_t *L = &st->obj[oi];
          const char *w = val.c_str (), *kp = NULL;
          if (streq (w, "Hidden"))           { L->where = A5_OWHERE_HIDDEN;      L->key = NULL; }
          else if (streq (w, "Held By Character")) { L->where = A5_OWHERE_HELD_BY;   kp = "HeldByWho"; }
          else if (streq (w, "Worn By Character")) { L->where = A5_OWHERE_WORN_BY;   kp = "WornByWho"; }
          else if (streq (w, "In Location"))       { L->where = A5_OWHERE_LOCATION;  kp = "InLocation"; }
          else if (streq (w, "Inside Object"))     { L->where = A5_OWHERE_IN_OBJECT; kp = "InsideWhat"; }
          else if (streq (w, "On Object"))         { L->where = A5_OWHERE_ON_OBJECT; kp = "OnWhat"; }
          if (kp != NULL)
            {
              const char *kv = a5state_entity_prop (st, k1, kp);
              if (kv == NULL)
                {
                  const a5_object_t *mo = a5model_object (st->adv, k1);
                  for (int pi = 0; mo != NULL && pi < mo->n_props; pi++)
                    if (streq (mo->props[pi].key, kp))
                      { kv = mo->props[pi].value; break; }
                }
              if (streq (kv, "%Player%")) kv = st->player_key;
              L->key = kv;
            }
        }
    }
  /* clsCharacter.ProperName tracks the CharacterProperName property
     (clsUserSession.vb:2080), so .Name/.ProperName render the new value. */
  if (tk[1] == "CharacterPosition")
    {
      int ci = a5state_character_index (st, k1);
      if (ci >= 0) { free (st->char_position[ci]); st->char_position[ci] = strdup (val.c_str ()); }
    }
}

static void
act_end_game (a5_run_t *run, const char * /*kind*/,
              const std::vector<std::string> &tk, const char * /*body*/,
              int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
  st->game_over = 1;
  free (st->end_message);
  st->end_message = strdup (tk.empty () ? "" : tk[0].c_str ());
}

static void
act_time (a5_run_t *run, const char * /*kind*/,
          const std::vector<std::string> &tk, const char * /*body*/,
          int /*depth*/, sb_t *out)
{
  a5_state_t *st = run->st;
  /* clsUserSession Time action (`Skip "N" turns`, vb:2357): Display the
     buffered pass responses, then run TurnBasedStuff N times, then clear
     the response buffer.  Lost Children's WaitZ task advances the
     Flight-1 countdown 3 extra turns per `z`. */
  long n = 0;
  if (tk.size () >= 2)
    {
      std::string sv = tk[1];
      if (sv.size () >= 2 && sv.front () == '"' && sv.back () == '"')
        sv = sv.substr (1, sv.size () - 2);
      char *endp = NULL;
      n = strtol (sv.c_str (), &endp, 10);
      if (endp == sv.c_str ())        /* not a literal: EvaluateExpression */
        {
          char *ev = a5text_eval_expression (st, sv.c_str ());
          if (ev != NULL) { n = strtol (ev, NULL, 10); free (ev); }
        }
    }
  if (run->resp != NULL)
    {
      /* The runner flushes htblResponsesPass before the ticks so the command's own
         message precedes any event output, then Clear()s it (keep nmut --
         it is the "did the task produce output" history probe). */
      resp_flush (run, run->resp, out);
      run->resp->ents.clear ();
    }
  for (long i = 0; i < n && !st->game_over; i++)
    ev_tick_all (run, out);
}

/* Scoped bump of run->exec_depth (the SetTasks-Execute runaway guard,
   A5_MAX_EXEC_DEPTH in a5run_internal.h).  A guard object rather than a bare
   ++/-- pair because the Execute block below returns from several places. */
struct exec_depth_guard {
  a5_run_t *run;
  explicit exec_depth_guard (a5_run_t *r) : run (r) { run->exec_depth++; }
  ~exec_depth_guard () { run->exec_depth--; }
private:
  exec_depth_guard (const exec_depth_guard &);
  exec_depth_guard &operator= (const exec_depth_guard &);
};

static void
act_set_tasks (a5_run_t *run, const char * /*kind*/,
               const std::vector<std::string> &tk, const char *body,
               int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  if (tk.empty ()) return;
  /* "FOR Loop = a TO b : Execute <key> : NEXT Loop" (FileIO.vb:365-374:
     IntValue = element 3, sPropertyValue = element 5, the Execute starts
     at element 7).  clsUserSession's SetTasks case runs the plain Execute
     b-a+1 times; the loop counter itself is not visible to the executed
     task (only SetVariable-in-a-FOR substitutes %Loop%). */
  if (tk[0] == "FOR" && tk.size () >= 9 && tk[4] == "TO" && tk[6] == ":")
    {
      long from = strtol (tk[3].c_str (), NULL, 10);
      long to   = strtol (tk[5].c_str (), NULL, 10);
      std::string inner;
      for (size_t i = 7; i < tk.size (); i++)
        {
          if (tk[i] == ":")             /* ": NEXT Loop" tail */
            break;
          if (!inner.empty ()) inner += " ";
          inner += tk[i];
        }
      if (!inner.empty () && depth < A5_MAX_ACTION_DEPTH)
        for (long l = from; l <= to && !st->game_over; l++)
          run_action (run, "SetTasks", inner.c_str (), depth + 1, out);
      return;
    }
  if (tk[0] == "Execute" && tk.size () >= 2)
    {
      /* Runaway `Execute` chain (two Repeatable tasks Executing each other):
         the per-Execute `depth` reset means nothing else bounds this, and each
         frame is large, so the C stack goes before any game-visible limit does.
         Decline the re-dispatch past A5_MAX_EXEC_DEPTH -- silently, exactly as
         the depth guards below decline theirs. */
      if (run->exec_depth >= A5_MAX_EXEC_DEPTH)
        return;
      exec_depth_guard edg (run);

      std::string key = tk[1];
      if (key == "Look")            /* built-in room view + the Look task's actions */
        {
          /* The runner's ExecuteTask(Look) is a full AttemptToExecuteTask(Look): its
             Specific overrides run before the view, then the room view
             (Look's AggregateOutput CompletionMessage), then Look's own
             <Actions>.  Routing through execute_task_with_overrides fires
             both -- e.g. Amazon's `Beforeplay1` -> `Execute ts_tasCheckTime`
             (the "Date: ..." line: startup, re-looks and per-move) AND
             Grandpa's `vnl_TutorialSt` chain off Look's actions.

             The per-move duplicate is real: PlayerMovement's `Beforeplay`
             override already ran ts_tasCheckTime, so its `Execute Look` ->
             `Beforeplay1` emits the same "Date: ..." a second time.  The runner's
             per-turn response dedup (htblResponses keyed by message text,
             clsUserSession.vb:783) collapses the two -- which run_general's
             movement response map now reproduces (the two Date lines share
             ts_tasCheckTime's AggregateOutput comp node and merge to one). */
          const a5_task_t *lt = a5model_task (st->adv, "Look");
          if (lt != NULL && depth < A5_MAX_ACTION_DEPTH)
            {
              std::vector<ref_info> noref;
              execute_task_with_overrides (run, lt, noref, depth, out);
            }
          else
            emit_look (run, out);
          return;
        }
      const a5_task_t *tt = a5model_task (st->adv, key.c_str ());
      if (tt != NULL && depth < A5_MAX_ACTION_DEPTH)
        {
          /* Optional "(arg1|arg2|...)" reference passing: bind the sub-task's
             command references to the evaluated argument keys, mirroring
             ExecuteTask's parameter substitution (used by the stock
             "take from others lazy" -> "take from others" re-dispatch). */
          std::string b = body ? body : "";
          std::vector<std::string> args;
          bool have_args = false;
          {
            size_t lp = b.find ('(');
            size_t rp = b.rfind (')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp)
              {
                have_args = true;
                args = split_pipe (b.substr (lp + 1, rp - lp - 1).c_str ());
              }
          }
          std::vector<std::string> rnames = command_refs (tt);

          /* A bare `Group.<Property>` argument (e.g. `Objects1.StaticOrDynamic`)
             is an OO reference that the runner expands to the group's member LIST and
             runs the executed task once per member (ExecuteSubTasks iterating
             the reference's items).  Detect it and iterate; a plain argument
             leaves giter < 0 and runs the task exactly once, as before. */
          int giter = -1;
          std::vector<std::string> gmembers;
          for (size_t i = 0; i < args.size (); i++)
            if (group_prop_member_keys (st, args[i], gmembers))
              { giter = (int) i; break; }

          /* The same plurality reaches the executed task through any OO chain
             that lands on a COLLECTION -- `%Player%.Held.<marker>`,
             `%Player%.Location.Characters` -- not just the `Group.Property`
             form above.  a5expr renders such a chain as a "key1|key2|..." pipe
             list, which is exactly the runner's reference carrying several
             Items, so ExecuteSubTasks runs the task once per item.  Binding the
             whole pipe list to one reference instead ran the task ONCE with a
             list-valued reference: Quest Giver's `view cards` prints its three
             held quests as a single merged card ("Quest 12) A showman ...,
             Peacemaker needed and A wealthy man ...") with one ID stamp,
             where the runner prints one card per quest.  Resolve here, against
             the parent's live references (run_one restores them per member). */
          if (giter < 0)
            for (size_t i = 0; i < args.size () && i < rnames.size (); i++)
              {
                if (arg_is_ref_passthrough (args[i], rnames[i]))
                  continue;             /* passed through with its own items */
                char *val = eval_arg_to_key (st, args[i]);
                if (val != NULL && strchr (val, '|') != NULL)
                  {
                    gmembers = split_pipe (val);
                    giter = (int) i;
                  }
                free (val);
                if (giter >= 0) break;
              }

          int tti = a5state_task_index (st, key.c_str ());
          bool ran_any = false;
          int  ran_count = 0;        /* per-call: run_one reached execution */

          /* The Completed/Repeatable gate is the runner's AttemptToExecuteTask entry
             test, evaluated ONCE per Execute -- so a non-repeatable task run
             over a PLURAL group reference still fires its actions for every
             member (the runner's ExecuteSubTasks iterates the items inside one task
             execution; Completed flips only afterwards).  Snapshot the flag
             at entry and gate on that, or a task that marks itself done on
             member 0 (execute_task_with_overrides) would skip members 1..n --
             AlienDiver's ShowCardsP3 (Repeatable=0, `Unset` then Execute'd
             over the crafted cards to stamp each a unique CardId) then stamped
             only the first card, so `pc`'s CardId==%number% matched every held
             card and over-awarded colored fragments. */
          bool tt_done_at_entry = (tti >= 0 && st->task_done[tti]);

          /* One execution of the target task with the references bound from
             the current `args` (the group member substituted into args[giter]
             on each pass).  Returns via early-out on a Completed/Repeatable or
             restriction gate -- the runner's AttemptToExecuteSubTask per item. */
          auto run_one = [&] (void) {
              if (have_args)
                {
                  /* The runner (clsUserSession.vb:2169-2298) builds a FRESH reference
                     array (ReferencesNew) for the sub-task and only swaps it in
                     (`NewReferences = ReferencesNew`) AFTER every parameter has
                     been resolved -- so each computed parameter's OO chain is
                     evaluated against the PARENT's live references, never
                     against an earlier parameter's freshly-bound value.  Mirror
                     that with a two-phase pass: resolve every non-passthrough
                     argument against the current (parent) state FIRST, then
                     apply all the binds.  A single-pass bind would let e.g.
                     AlienDiver's `Execute CraftACard8
                     (%Player%.Location.Objects.ObjectIsAT|%object%)` read the
                     cube that arg0 just stamped onto ReferencedObject1 when
                     resolving arg1's `%object%`, mis-binding ReferencedObject2
                     to the cube instead of the parent's crafted card. */
                  std::vector<std::pair<size_t, std::string> > deferred;
                  for (size_t i = 0; i < args.size () && i < rnames.size (); i++)
                    {
                      /* The runner's SetTasks handler (clsUserSession.vb:2188) passes a
                         parameter that names the target task's own reference
                         (`sParam = sRef`) STRAIGHT THROUGH -- the live
                         clsNewReference object, items and sCommandReference
                         intact.  So `Execute cl_TakeObject (%objects%)` keeps
                         each item's original typed text ("all" under a bare
                         `get all`), which is what lets the executed task's
                         `MustNot BeExactText All` restriction fail silently per
                         item (ThingsThatGoBumpInTheNight's take chain).  Mirror
                         that by leaving the existing binding (key AND $text)
                         untouched.  A computed argument (%ParentOf[..]%, a
                         literal key, or an expanded group member) instead builds
                         the runner's UserDefinedRef, whose fresh items carry NO
                         sCommandReference -- bind an empty typed text. */
                      if (arg_is_ref_passthrough (args[i], rnames[i]))
                        continue;                   /* pass the ref through */
                      char *val = eval_arg_to_key (st, args[i]);
                      deferred.push_back (std::make_pair (i, std::string (val)));
                      free (val);
                    }
                  for (auto &d : deferred)
                    bind_reference (st, rnames[d.first].c_str (),
                                    d.second.c_str (), "");
                }
              /* AttemptToExecuteTask: gate on Completed/Repeatable and the
                 task's own restrictions (with any refs just bound above) --
                 the stock list-runner tasks (e.g. the bomb cascade) execute a
                 list of candidate tasks and rely on each one's restrictions to
                 pick the one that fires. */
              if (tt_done_at_entry && !tt->repeatable)
                return;
              if (!a5restr_pass (st, tt->restrictions))
                {
                  /* The runner's ExecuteTask is a full AttemptToExecuteTask: a
                     restriction failure DISPLAYS the failing restriction's
                     message (AttemptToExecuteSubTask, clsUserSession.vb:1246
                     `sMessage = sRestrictionText` -> AddResponse bPass=False,
                     deduped by text in htblResponsesFail).  a5restr_pass just
                     left that node in st->restriction_text (NULL for a
                     message-less failure, e.g. `MustNot BeExactText All` under
                     a bare `get all` -- those stay silent).  TBN's dark-room
                     `get dirt` needs the emission: cl_TakeCharac passes but
                     both Execute'd take tasks fail on the LightSources
                     restriction with "It is too dark to find the dirt.". */
                  const a5_xml_node_t *fm = st->restriction_text;
                  if (fm != NULL)
                    {
                      char *fmsg = a5text_describe (st, fm);
                      if (msg_has_output (fmsg))
                        {
                          if (run->resp != NULL)
                            resp_add_text (run, fmsg, false);
                          else if (run->exec_scope != NULL)
                            {
                              /* Buffer for the end-of-scope flush, where the runner's
                                 pass-cancels-fail rule is applied (a pass for
                                 the same objects can arrive before OR after
                                 this fail); dedup by text (htblResponsesFail
                                 keying). */
                              if (run->exec_scope->fail_seen.insert (fmsg).second)
                                run->exec_scope->fails.push_back
                                  (std::make_pair (std::string (fmsg),
                                                   current_obj_ref_keys (st)));
                            }
                          else
                            { sb_pspace (out); sb_puts (out, fmsg); }
                        }
                      free (fmsg);
                    }
                  return;
                }
              /* The runner's ExecuteTask is a full AttemptToExecuteTask, so the
                 executed task's own Specific overrides apply too -- e.g. the
                 lazy take-from-others re-dispatch lets PutSomeDry (the "skull"
                 override of TakeObjectsFromOthers) fire, moving Anno's
                 gunpowder into the held skull.  The downstream cannon puzzle
                 reads it via the now recursive BeHoldingObject. */
              std::vector<ref_info> refs = refs_from_bindings (st, tt);
              execute_task_with_overrides (run, tt, refs, 0, out);
              ran_any = true;
              ran_count++;
          };

          /* The runner wraps each SetTasks Execute in `oExistingRefs = NewReferences`
             ... `NewReferences = oExistingRefs` (clsUserSession.vb:2172/2310),
             so the sub-task's reference bindings are LOCAL to that one Execute
             and the parent's references are restored afterwards.  AlienDiver's
             craft chain relies on this: CraftACard2 runs `Execute CraftACard5`,
             `Execute CraftACard8`, ... in sequence, each `(...ObjectIsAT |
             %object%)`.  The first Execute binds object1 (and thus the bare
             ReferencedObject alias) to the cube, and without a restore the NEXT
             sibling's `%object%` would read that cube instead of the parent's
             crafted card -- mis-imprinting CraftedCar onto the cube and losing
             the play-match bonus.  Snapshot the parent bindings here, restore
             them before each group member and once after the whole Execute.

             Heap, not stack: a ref_snap is ~33 KB (16 x A5_REFVAL_LEN), and
             this frame sits on the Execute recursion cycle, so a by-value
             local made every nested Execute cost 33 KB of C stack.  Plain
             `new` (no value-init): a vector element was zeroing all 33 KB
             before ref_snap_take overwrote it, once per SetTasks Execute. */
          std::unique_ptr<ref_snap> parent_refs (new ref_snap);
          ref_snap_take (st, parent_refs.get ());
          if (giter >= 0)
            {
              /* Iterate the executed task once per resolved group member,
                 substituting each member key into the group argument slot.
                 A prior action in this same task may have already ended the
                 game (e.g. I Summon Thee's win task runs `EndGame Win`
                 BEFORE `Execute CheckEscap (Everything.StaticOrDynamic)`,
                 which tallies Objects Destroyed): the runner keeps iterating every
                 member of a sub-task regardless -- game-over is only checked
                 back in the main loop.  So break only on a game-over NEWLY
                 triggered by a member's own execution, not one inherited
                 from before the loop, or the tally stops after member 0.
                 Restore the parent bindings before each member so every
                 member's non-iterated args resolve against the parent (the runner
                 fixes them once in ReferencesNew and reuses across items). */
              bool was_over = st->game_over;
              std::string saved = args[giter];
              for (const std::string &mk : gmembers)
                {
                  ref_snap_restore (st, parent_refs.get ());
                  args[giter] = mk;
                  /* Inside an event/walk/LocationTrigger-fired attempt, attach
                     this member to the response entry its run reached (the
                     runner AddResponse keys every subtask completion with the
                     iterated reference; a repeat of the same completion MERGES
                     its item into the existing entry).  The attempt's final
                     entry becomes the post-Display NewReferences leftover the
                     next event task iterates -- see ev_tbl in a5run_internal.h.
                     Quest Giver: daz6CheckIfAny4's blank "\n\n" completion
                     merges every active quest into one entry, so
                     daz61DaylightC then decrements daylight once per quest.
                     The COMMAND path (ev_tbl NULL) is untouched -- view cards'
                     per-member card printing stays per-member. */
                  int t0 = run->ev_tbl != NULL ? run->ev_tbl->touches : 0;
                  run_one ();
                  if (run->in_ev_attempt && run->ev_tbl != NULL
                      && run->ev_tbl->touches > t0 && run->ev_tbl->last >= 0
                      && (size_t) run->ev_tbl->last < run->ev_tbl->refs.size ())
                    {
                      std::vector<std::string> &ms
                          = run->ev_tbl->refs[run->ev_tbl->last];
                      if (std::find (ms.begin (), ms.end (), mk) == ms.end ())
                        ms.push_back (mk);
                    }
                  if (st->game_over && !was_over)
                    break;
                }
              args[giter] = saved;
            }
          else
            run_one ();
          ref_snap_restore (st, parent_refs.get ());

          /* Mark done + fire completion controls once per AttemptToExecuteTask
             (the runner flips task.Completed once), and only when the task actually
             ran -- a wholly-failing Execute leaves it uncompleted, as before. */
          if (ran_any)
            {
              if (tti >= 0)
                st->task_done[tti] = 1;
              ev_on_task_completed (run, key.c_str (), out);
            }
        }
    }
  else if (tk[0] == "Unset" || tk[0] == "Clear")
    {
      if (tk.size () >= 2)
        { int ti = a5state_task_index (st, tk[1].c_str ()); if (ti >= 0) st->task_done[ti] = 0; }
    }
}

static void
act_conversation (a5_run_t *run, const char * /*kind*/,
                  const std::vector<std::string> &tk, const char * /*body*/,
                  int /*depth*/, sb_t *out)
{
  a5_state_t *st = run->st;
  /* FileIO conversation-action parse: GREET/FAREWELL/ASK/TELL <key> [About|with
     '<subject>'], SAY '<subject>' to <key>, ENTERWITH/LEAVEWITH <key>.  The
     key (often ReferencedCharacter) and subject (often %text%) are resolved
     here, then ExecuteConversation runs. */
  if (tk.empty ())
    return;
  std::string verb = tk[0];
  for (char &c : verb) c = (char) toupper ((unsigned char) c);

  if (verb == "ENTERWITH")
    { if (tk.size () >= 2) a5state_set_conv_char (st, act_key (st, tk[1].c_str ())); return; }
  if (verb == "LEAVEWITH")
    { if (tk.size () >= 2 && streq (st->conv_char, act_key (st, tk[1].c_str ())))
        { a5state_set_conv_char (st, ""); a5state_set_conv_node (st, ""); }
      return; }

  int conv_type = 0;
  std::string keytok, subjraw;
  if (verb == "GREET" || verb == "FAREWELL")
    {
      conv_type = (verb == "GREET") ? A5_CONV_GREET : A5_CONV_FAREWELL;
      if (tk.size () >= 2) keytok = tk[1];
      for (size_t i = 3; i < tk.size (); i++)        /* tk[2] = "with" */
        { if (i > 3) subjraw += " "; subjraw += tk[i]; }
    }
  else if (verb == "ASK" || verb == "TELL")
    {
      conv_type = (verb == "ASK") ? A5_CONV_ASK : A5_CONV_TELL;
      if (tk.size () >= 2) keytok = tk[1];
      for (size_t i = 3; i < tk.size (); i++)        /* tk[2] = "About" */
        { if (i > 3) subjraw += " "; subjraw += tk[i]; }
    }
  else if (verb == "SAY")
    {
      conv_type = A5_CONV_COMMAND;
      if (tk.size () >= 1) keytok = tk[tk.size () - 1];   /* last = key */
      for (size_t i = 1; i + 2 < tk.size (); i++)         /* before "to <key>" */
        { if (i > 1) subjraw += " "; subjraw += tk[i]; }
    }
  else
    return;

  /* strip surrounding single quotes from the subject */
  if (subjraw.size () >= 1 && subjraw.front () == '\'')
    subjraw.erase (subjraw.begin ());
  if (subjraw.size () >= 1 && subjraw.back () == '\'')
    subjraw.pop_back ();

  const char *ckey = act_key (st, keytok.c_str ());
  char *subj = a5text_process (st, subjraw.c_str ());
  exec_conversation (run, ckey, conv_type, subj, out);
  free (subj);
}

static void
act_location_group (a5_run_t *run, const char *kind,
                    const std::vector<std::string> &tk, const char * /*body*/,
                    int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
  /* "<what> <key1> [To|From]Group <grp>" (clsUserSession.vb:1917): build the
     location set from the selector, then add/remove each from the target
     group's runtime membership.  Jacaranda's day/night events run
     `AddLocationToGroup EverywhereInGroup Group2 ToGroup DarkLocations` (dusk)
     and the matching Remove (dawn), so every outdoor location inherits the
     DarkLocations ShortLocationDescription ("Everything is dark") at night. */
  if (tk.size () < 4)
    return;
  const std::string &what = tk[0];
  /* Resolve the source key through act_key so a `%Player%` / `%objectN%`
     operand maps to its entity key (the runner's ReferenceControl expansion).  A
     group key (EverywhereInGroup) is not an entity, so act_key returns it
     verbatim -- harmless.  Without this `LocationOf %Player%` looked up a
     character literally named "%Player%" (none), so SixSilverBullets'
     `RemoveLocationFromGroup LocationOf %Player% FromGroup TimeTraps` removed
     nothing and The Hotel kept tolling the bell every turn. */
  const char *k1  = act_key (st, tk[1].c_str ());
  const char *grp = tk[3].c_str ();
  int add = streq (kind, "AddLocationToGroup");
  std::vector<std::string> locs;
  if (what == "Location")
    locs.push_back (k1);
  else if (what == "LocationOf")
    {
      int ci = a5state_character_index (st, k1);
      if (ci >= 0)
        { const char *lk = st->char_loc[ci];
          if (lk != NULL && !streq (lk, "Hidden")) locs.push_back (lk); }
      else
        { int oi = a5state_object_index (st, k1);
          if (oi >= 0)
            for (int i = 0; i < st->adv->n_locations; i++)
              if (a5state_object_at_location (st, oi, st->adv->locations[i].key, 1))
                locs.push_back (st->adv->locations[i].key); }
    }
  else if (what == "EverywhereInGroup")
    {
      /* Live members (the runner arlMembers): the group-clear that follows each
         Skybreak jump is `RemoveLocationFromGroup EverywhereInGroup G
         FromGroup G`, which must see the runtime-added members to empty the
         group -- a static read leaves them and the group accretes forever. */
      int n = a5state_group_count (st, k1);
      for (int m = 0; m < n; m++)
        locs.push_back (a5state_group_member_at (st, k1, m));
    }
  else if (what == "EverywhereWithProperty")
    {
      /* Merged (runtime + static, incl. group-inherited) test -- see the
         MoveObject branch. */
      for (int i = 0; i < st->adv->n_locations; i++)
        { const a5_location_t *l = &st->adv->locations[i];
          if (a5state_entity_has_prop (st, l->key, k1))
            locs.push_back (l->key); }
    }
  else
    return;
  for (const std::string &lk : locs)
    a5state_set_object_in_group (st, grp, lk.c_str (), add);
}

void
run_action (a5_run_t *run, const char *kind, const char *body, int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  std::vector<std::string> tk = split_ws (body);

  if (a5run_trace)
    fprintf (stderr, "    [action %s] %s\n", kind, body ? body : "");

  /* Multiple-object expansion (clsUserSession.ExecuteSingleAction's
     ReferencedObjects loop): when an operand names the plural %objects%
     reference and it resolved to a set, run this action once per item with the
     item's key substituted for "ReferencedObjects".  The recursive call sees a
     concrete key in place of the token, so it does not loop again. */
  if (st->n_ref_items > 0 && depth < A5_MAX_ACTION_DEPTH)
    for (auto &w : tk)
      if (w == "ReferencedObjects")
        {
          for (int it = 0; it < st->n_ref_items; it++)
            {
              std::string b2;
              for (size_t wi = 0; wi < tk.size (); wi++)
                { if (wi) b2 += ' ';
                  b2 += (tk[wi] == "ReferencedObjects") ? st->ref_items[it]
                                                        : tk[wi]; }
              run_action (run, kind, b2.c_str (), depth + 1, out);
            }
          return;
        }

  if (streq (kind, "MoveObject"))
    { act_move_object (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "AddObjectToGroup") || streq (kind, "RemoveObjectFromGroup"))
    { act_object_group (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "MoveCharacter"))
    { act_move_character (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "SetVariable") || streq (kind, "IncVariable")
      || streq (kind, "DecVariable"))
    { act_set_variable (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "SetProperty"))
    { act_set_property (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "EndGame"))
    { act_end_game (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "Time"))
    { act_time (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "SetTasks"))
    { act_set_tasks (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "Conversation"))
    { act_conversation (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "AddLocationToGroup") || streq (kind, "RemoveLocationFromGroup"))
    { act_location_group (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "AddCharacterToGroup") || streq (kind, "RemoveCharacterFromGroup"))
    { act_character_group (run, kind, tk, body, depth, out); return; }
}


/* ------------------------------------------------------------- run one task */

/* The object keys currently bound as this task-chain's %objects%/%object%
   reference -- the ReferencedObjects pipe list when present, else the singular
   ReferencedObject1.  Used for the runner's pass-cancels-fail response rule (see
   exec_pass_refs in a5run_internal.h). */
static std::vector<std::string>
current_obj_ref_keys (a5_state_t *st)
{
  std::vector<std::string> v;
  const char *p = a5state_lookup_ref (st, "ReferencedObjects");
  if (p != NULL && p[0] != '\0')
    {
      for (const std::string &k : split_pipe (p))
        if (!k.empty ()) v.push_back (k);
      return v;
    }
  p = a5state_lookup_ref (st, "ReferencedObject1");
  if (p != NULL && p[0] != '\0')
    v.push_back (p);
  return v;
}

/* Render a completion message and emit it as one response: trailing whitespace
   (the newline the XML <Text> carries before </Text>) is trimmed so a "Before"
   message followed by a sub-task's output is not separated by a spurious blank
   line -- the Adrift 5 runner trims each response the same way. */
static void
emit_completion (a5_run_t *run, const a5_xml_node_t *comp, sb_t *out)
{
  /* A completion message is real output, so its <DisplayOnce> segments must be
     retired after the first show (clsDescription.ToString marks Displayed when
     bTestingOutput is false) -- otherwise the first-visit segment shows every
     time.  Mirror a5text_view_location: render under marking_display.  Fixes
     Anno's MovingFrom5 ("step into the hotel" once, "step into the reception"
     thereafter). */
  if (run->immediate_emit)
    {
      /* Startup RunImmediately path: render in two stages so the markup-bearing
         form (`proc`, what the runner's Display sees) drives the output test, while the
         plain form is emitted.  The runner's bHasOutput keeps a message when stripping
         tags leaves any character -- so a title-music task's `<audio ...> ` keeps
         its trailing space to join onto the title.  a5text_describe ==
         eval_description -> process -> render_plain. */
      char *raw, *proc, *plain;
      {
        a5_mark_guard mg (run->st, 1);
        raw   = a5text_eval_description (run->st, comp);
        proc  = a5text_process (run->st, raw);
        plain = a5text_render_plain (proc);
      }
      if (fd_has_output (run->st, proc)) { sb_pspace (out); sb_puts (out, plain); }
      free (raw); free (proc); free (plain);
      return;
    }
  int pre_alr_ink = 0;
  int raw_nonblank = 0;
  char *marked = NULL;
  char *m;
  {
    a5_mark_guard mg (run->st, 1);
    m = a5text_describe_ex (run->st, comp, &pre_alr_ink, &raw_nonblank,
                            &marked);
  }
  if (raw_nonblank)
    run->task_raw_output = 1;
  /* Event/walk/LocationTrigger attempt (non-aggregate path -- the aggregate
     one keys on the raw branch text up in run_task): a completion whose RAW
     template is non-blank (the runner's AddResponse bHasOutput -- a
     whitespace-only "\n\n" counts) inserts or merges a response-table entry
     keyed by its CompletionMessage node.  The table's final entry decides the
     attempt's post-Display NewReferences leftover (see ev_tbl,
     a5run_internal.h). */
  if (run->in_ev_attempt && run->ev_tbl != NULL && raw_nonblank)
    {
      ev_resp_tbl *tb = run->ev_tbl;
      size_t e;
      for (e = 0; e < tb->keys.size (); e++)
        if (tb->keys[e] == (const void *) comp && tb->texts[e].empty ())
          break;
      if (e == tb->keys.size ())
        {
          tb->keys.push_back ((const void *) comp);
          tb->texts.push_back (std::string ());
          tb->refs.push_back (std::vector<std::string> ());
        }
      tb->last = (int) e;
      tb->touches++;
    }
  emit_message_body (run, m, pre_alr_ink, out, marked);
  free (marked);
}

/* Append an already-rendered completion message `m` (ownership taken; freed here)
   exactly as the runner Display() does: pSpace-join to the running output, then the
   rendered text verbatim.  `m` is already-plain, so msg_has_output is the faithful
   bHasOutput here: it keeps a whitespace-only message (e.g. a `<Text> </Text>`
   completion), which then space-joins to the next response -- that is how the runner
   renders the leading indent before a search-triggered encounter title (Tingalan's
   Search " " completion -> Execute encounter). */
static void
emit_message_body (a5_run_t *run, char *m, int pre_alr_ink, sb_t *out,
                   const char *dedup_key)
{
  if (msg_has_output (m))
    {
      /* The runner keys htblResponsesPass on the still-marked-up message, so a
         difference that lives inside a tag Display later strips still makes two
         responses distinct; `m` is post-strip, so prefer the marked-up form. */
      const char *dk = dedup_key != NULL ? dedup_key : m;
      /* Within an event-fired task chain, dedup identical completion messages
         (the runner's htblResponsesPass, keyed by text): show the first, drop repeats.
         Keeps the first occurrence's position -- which, for a message emitted
         before a later <cls>, means it is wiped by that clear (Pathway's banner). */
      if (run->ev_seen != NULL)
        {
          if (run->ev_seen->count (dk)) { free (m); return; }
          run->ev_seen->insert (dk);
        }
      /* Per-command pass-response text dedup (the runner's htblResponsesPass keying):
         a command that reaches the same completion text twice -- e.g. two
         Execute'd tasks with identical messages (Euripides' `press on` runs
         cl_ToCrawler11 AND cl_ToCrawler12, both the crawler journey) -- shows
         it once, keeping the first occurrence's position. */
      if (run->exec_scope != NULL
          && !run->exec_scope->pass_seen.insert (dk).second)
        { free (m); return; }
      /* A pass response with output: record its bound object references for
         the pass-cancels-fail rule (the runner keys every AddResponse with the
         subtask's reference keys and drops a fail whose refs all passed). */
      if (run->exec_scope != NULL)
        for (auto &k : current_obj_ref_keys (run->st))
          run->exec_scope->pass_refs.insert (k);
      sb_pspace (out); sb_puts (out, m);
    }
  else if (pre_alr_ink && m != NULL && m[0] != '\0')
    {
      /* The message had real text until a game ALR blanked it (Tribute maps
         the auto-note "(from the enormous mirror)" to an empty TextOverride).
         the runner stores the response pre-ALR and only blanks it inside Display(),
         so its markup/newline skeleton still pSpace-joins the output and
         leaves an empty paragraph slot.  Emit the whitespace remainder
         verbatim to keep that slot. */
      sb_pspace (out); sb_puts (out, m);
    }
  free (m);
}

/* Render the room view for the runner's "Execute Look" (on movement / "look").  The
   stock Look task's CompletionMessage is "%Player%.Location.Description" (== the
   room view) followed by an optional restriction-gated darkness override -- a
   StartDescriptionWithThis description that, when the player is in a
   DarkLocations-group location with no LightSources object in scope, replaces
   the whole view with the game's "It is too dark..." line (clsDescription.ToString;
   the override lives in the Look task data, not the engine).  We render the room
   view exactly as before (a5text_view_location -- not back through the text
   pipeline, which would perturb whitespace in the common no-override case), then
   apply any further passing descriptions just as ToString would. */
static std::string
render_look_string (a5_run_t *run)
{
  a5_state_t *st = run->st;
  const a5_task_t *look = a5model_task (st->adv, "Look");
  const a5_xml_node_t *comp =
      look != NULL ? a5xml_child (look->node, "CompletionMessage") : NULL;
  char *view = a5text_view_location (st);
  std::string result = view ? view : "";
  /* The room view IS the CompletionMessage's default (Me(0) ==
     "%Player%.Location.Description").  A StartAfterDefaultDescription segment
     rebuilds from this default (the runner clsDescription.ToString / eval_desc_into),
     discarding any StartDescriptionWithThis override that fired before it -- so
     Magor's fire-lit main chamber/library keep the room view even though the
     Look task's restricted "It is too dark..." override passes (its restrictions
     do pass; the following "Remember ... LUMINO" StartAfterDefault segment resets
     the buffer to default+hint). */
  std::string default_view = result;
  free (view);

  if (comp != NULL)
    {
      int idx = 0;
      for (const a5_xml_node_t *c = comp->first_child; c; c = c->next)
        {
          const a5_xml_node_t *restr;
          const char *when, *raw;
          char *proc, *plain;
          int once;
          if (strcmp (c->name, "Description") != 0)
            continue;
          if (idx++ == 0)
            continue;                          /* the room-view default */
          /* A <DisplayOnce> segment already shown is suppressed (mirrors
             eval_desc_into / clsDescription.ToString: "If Not sd.DisplayOnce
             OrElse Not sd.Displayed").  Without this the Look aggregate's
             first-time hints -- e.g. Book of Jax's "(Don't forget ... use the
             LUMINO spell.)" -- reappeared on every room view where the runner shows them
             once. */
          once = a5xml_bool (a5xml_child_text (c, "DisplayOnce"));
          if (once && a5state_disp_once_seen (st, c))
            continue;
          restr = a5xml_child (c, "Restrictions");
          if (restr != NULL && !a5restr_pass (st, restr))
            continue;
          /* Retire the segment only on real output, not a bTestingOutput peek
             (the two pre/post-action test renders set marking_display=0; the
             final Display render sets it to 1). */
          if (once && st->marking_display)
            a5state_disp_once_mark (st, c);
          when = a5xml_child_text (c, "DisplayWhen");
          raw  = a5xml_child_text (c, "Text");
          proc = a5text_process (st, raw ? raw : "");
          plain = a5text_render_plain (proc);
          free (proc);
          if (streq (when, "StartDescriptionWithThis"))
            result = plain;
          else if (streq (when, "StartAfterDefaultDescription"))
            {                                   /* rebuild from the default view */
              result = default_view;
              if (!result.empty ()) result += "  ";
              result += plain;
            }
          else                                  /* AppendToPreviousDescription */
            { if (!result.empty ()) result += "  "; result += plain; }
          free (plain);
        }
    }
  return result;
}

static void
emit_look (a5_run_t *run, sb_t *out)
{
  /* Defer the room view until the command's After children have run (the runner's
     AggregateOutput Look render at final Display).  Record the insertion offset;
     execute_task_with_overrides renders and splices it once at final state. */
  if (run->defer_look && run->resp == NULL)
    {
      if (!run->look_pending)
        { run->look_pending = 1; run->look_pos = out->len; }
      return;
    }

  if (run->resp != NULL)
    {
      /* Defer the room view to flush (final command state): the runner's stock Look
         CompletionMessage is AggregateOutput, so its render is deferred to
         Display.  Recording a look entry (rather than rendering now) lets a
         command whose After children move things have the view reflect them. */
      resp_entry e;
      e.is_pass = true; e.is_look = true;
      e.item = run->resp->cur_item;
      resp_insert (run->resp, e, -1);
      return;
    }
  /* Real output: retire any <DisplayOnce> completion segments this view shows. */
  std::string result = render_look_marked (run);
  sb_pspace (out);
  sb_puts (out, result.c_str ());
}

void
run_task (a5_run_t *run, const a5_task_t *t, int depth, sb_t *out)
{
  /* v5's FileIO.Load defaults a missing <MessageBeforeOrAfter> to Before (the
     class default After is overridden at load time, FileIO.vb:1618), so the
     completion message is evaluated and emitted *before* the actions run.  This
     is what makes TakeObjectsFromOthersLazy's "(from %objects%.Parent.Name)"
     render "(from the Table)" (parent still the source) ahead of the
     SetTasks-Execute sub-task's "You take ... from the Table." line. */
  const char *when = a5xml_child_text (t->node, "MessageBeforeOrAfter");
  int before = !streq (when, "After");
  const a5_xml_node_t *comp = a5xml_child (t->node, "CompletionMessage");
  const a5_xml_node_t *act = t->actions;
  int self_ti = a5state_task_index (run->st, t->key);

  if (a5run_trace)
    fprintf (stderr, "  [run task %s]\n", t->key ? t->key : "?");

  /* The built-in Look task's completion is the ADRIFT property expression
     "%Player%.Location.Description" plus a restriction-gated darkness override;
     render that CompletionMessage (emit_look) instead of the room view directly,
     so a dark location shows the game's "too dark" line. */
  if (streq (t->key, "Look"))
    {
      /* The runner executes the stock Look like any Before + AggregateOutput task
         (clsUserSession.vb:1164-1207): the message is test-rendered BEFORE and
         AFTER its actions (bTestingOutput=True; each render re-draws any
         <# OneOf #>/RAND in the room view), and when the two renders differ (a
         random pick moved between them) the response is PINNED to the first
         render.  Only when they agree does the raw aggregate message survive to
         be re-rendered once more at final Display -- which is the defer_look /
         is_look deferral emit_look implements.  Doing both test renders here
         keeps the xoshiro draw stream aligned with the runner for random room views
         (LostLabyrinth's riding OneOf: 2 draws, first one shown); for the usual
         static view the two renders draw nothing and agree, so this is
         draw- and output-neutral. */
      std::string e1;
      { a5_mark_guard mg (run->st, 0); e1 = render_look_string (run); }

      /* The response slot is reserved BEFORE the actions run (vb:1189
         iResponsePosition), so any output the actions produce follows the
         room view -- Grandpa's tutorial lines land after the view. */
      int resp_pos = -1;
      int claimed_pending = 0;
      sb_t abuf; sb_init (&abuf);
      if (run->resp != NULL)
        resp_pos = (int) run->resp->ents.size ();
      else if (run->defer_look && !run->look_pending)
        { run->look_pending = 1; run->look_pos = out->len; claimed_pending = 1; }

      if (self_ti >= 0) run->st->task_done[self_ti] = 1;
      int saved_sti = run->cur_score_ti;
      run->cur_score_ti = self_ti;
      if (t->actions != NULL)
        for (const a5_xml_node_t *c = t->actions->first_child; c; c = c->next)
          run_action (run, c->name, c->text, depth,
                      (run->resp == NULL && !run->defer_look) ? &abuf : out);
      run->cur_score_ti = saved_sti;

      std::string e2;
      { a5_mark_guard mg (run->st, 0); e2 = render_look_string (run); }

      if (e1 != e2)
        {
          /* Pinned to the pre-actions render (vb:1200 sMessage = sBefore...). */
          if (run->resp != NULL)
            resp_add_text (run, e1.c_str (), true, resp_pos);
          else if (run->defer_look)
            {
              if (claimed_pending)
                { free (run->look_pinned);
                  run->look_pinned = strdup (e1.c_str ()); }
            }
          else if (run->exec_scope == NULL
                   || run->exec_scope->pass_seen.insert (e1).second)
            { sb_pspace (out); sb_puts (out, e1.c_str ()); }
        }
      else if (run->resp != NULL)
        {
          if (!t->aggregate)
            {
              /* A NON-aggregate stock Look (this game's library version --
                 e.g. Murder Most Foul) renders its response text at task
                 time in the runner (AddResponse keys the rendered text), so the view
                 must NOT be deferred to the flush: a LocationTrigger System
                 task that fires after the move (the corridor's +2 servant
                 scene) would otherwise leak its score into the view's SCORE
                 header, which the runner renders pre-award. */
              std::string v = render_look_marked (run);
              resp_add_text (run, v.c_str (), true, resp_pos);
            }
          else
            {
              resp_entry e;
              e.is_pass = true; e.is_look = true;
              e.item = run->resp->cur_item;
              resp_insert (run->resp, e, resp_pos);
            }
        }
      else if (!run->defer_look)
        {
          /* The per-command response dedup applies to the room view too: the runner
             keys the Look task's raw AggregateOutput template, so a command
             that Executes Look twice shows one view (Euripides `press on`).
             Real output: retire any <DisplayOnce> completion segments. */
          std::string v = render_look_marked (run);
          if (run->exec_scope == NULL
              || run->exec_scope->pass_seen.insert (v).second)
            {
              exec_resp_scope *sc = run->exec_scope;
              if (sc != NULL && t->aggregate && sc->look_off >= 0
                  && sc->look_sink == out
                  && (size_t) sc->look_off + sc->look_len <= out->len)
                {
                  /* Second aggregate view this command, DIFFERENT text (the
                     player moved between the Executes): the runner's response map has
                     ONE slot for the Look template, so the later render
                     replaces the earlier view in place (first slot's position,
                     last text). */
                  sb_splice (out, (size_t) sc->look_off, sc->look_len,
                             v.c_str ());
                  sc->look_len = v.size ();
                }
              else
                {
                  sb_pspace (out);
                  if (sc != NULL && t->aggregate)
                    { sc->look_sink = out; sc->look_off = (long) out->len; }
                  sb_puts (out, v.c_str ());
                  if (sc != NULL && t->aggregate)
                    sc->look_len = v.size ();
                }
            }
        }
      else if (!t->aggregate && claimed_pending)
        {
          /* Direct-path deferral (the outer task's Execute Look splice) of a
             NON-aggregate Look: pin the view rendered NOW, at Execute Look
             time, so later actions/system tasks can't retro-date the SCORE
             header (the runner renders the non-aggregate response eagerly). */
          std::string v = render_look_marked (run);
          free (run->look_pinned);
          run->look_pinned = strdup (v.c_str ());
        }
      /* defer_look + unchanged (aggregate): the pending slot claimed above is
         rendered at the enabling frame's final state, exactly as before. */

      if (abuf.len > 0) { sb_pspace (out); sb_puts (out, abuf.p); }
      free (abuf.p);
      return;
    }

  /* On the map path a "Before" message follows the Adrift 5 runner's
     sBeforeActionsMessage logic (clsUserSession.vb:1176-1205): it claims its
     slot in the response order now, is rendered once before the actions, and
     after the actions is either pinned to that pre-action text (if the actions
     changed its rendering -- e.g. TakeObjectsFromOthersLazy's "(from
     %objects%.Parent.Name)", whose parent flips to the player once the take
     runs) or, for an unchanged AggregateOutput task, left deferred so a second
     item merges into its %objects% list. */
  char *resp_pre = NULL;
  char *resp_pre_marked = NULL;
  int   resp_pos = -1;
  char *flat_raw = NULL;         /* flat-path frozen Before template + snapshot */
  char *flat_pre = NULL;
  char *flat_pre_marked = NULL;
  int   flat_pre_ink = 0;
  int   flat_inter = 0;
  sb_t  flat_abuf; sb_init (&flat_abuf);
  if (before && comp != NULL)
    {
      if (run->resp != NULL)
        {
          resp_pre = render_comp_test_ex (run->st, comp, &resp_pre_marked);
          resp_pos = (int) run->resp->ents.size ();
          /* The runner renders task.CompletionMessage.ToString with bTestingOutput
             False BEFORE the actions (clsUserSession.vb:1167), retiring any
             <DisplayOnce> segment even though the response text itself is
             pinned/re-rendered later.  Mirror the marking for a non-aggregate
             task with a segment-selection eval AFTER the pre-action test
             render (so resp_pre keeps the first-time segment) -- eval only,
             no %function% replacement, so no RNG draws.  An aggregate comp is
             re-rendered at flush, where Scarier's segment choice must stay
             live, so it keeps the old behaviour.  Euripides' crevice squeeze
             (a movement override with a DisplayOnce first paragraph) showed
             the first-time text on every later squeeze without this. */
          if (!t->aggregate)
            {
              a5_mark_guard mg (run->st, 1);
              free (a5text_eval_description (run->st, comp));
            }
        }
      else if (!comp_bears_function (comp) || run->defer_look)
        {
          /* The runner (FileIO.vb:1618 defaults a missing <MessageBeforeOrAfter> to
             Before) renders a Before completion message up to THREE times
             (clsUserSession.vb:1176-1205): a pre-action snapshot
             `sBeforeActionsMessage`, a post-action compare, and a finalize
             `sMessage = ReplaceExpressions(ReplaceFunctions(sMessage))`.  A message
             bearing a text-changing function (`RAND(..)`, `<#OneOf#>`) draws on
             each render, so the runner draws 3× and shows the finalize when the first two
             renders agree, or draws 2× and pins the first when they differ.
             Scarier's flat resp==NULL buffer emitted once -- desyncing the xoshiro
             stream on e.g. Tingalan's `read book of ancient lore`, whose
             CompletionMessage is `This book contains %booksoflore[RAND(1,25)]%`
             (the runner drew RAND(1,25) 3× and showed array index 3; Scarier drew once and
             showed index 1, diverging every downstream encounter roll).  A static
             completion (no `%function%`) renders identically every time and draws
             nothing, so the two extra renders are inert for the corpus's plain
             messages -- this fast path keeps them byte-stable.  (defer_look also
             stays here: the interleaved path below buffers action output, which
             would break look_pos indexing into `out`.) */
          char *m1 = NULL, *m2 = NULL;
          char *e1 = render_comp_test_ex (run->st, comp, &m1);
          char *e2 = render_comp_test_ex (run->st, comp, &m2);
          int renders_differ = (m1 != NULL && m2 != NULL && strcmp (m1, m2) != 0);
          free (m2);
          if (e1 != NULL && e2 != NULL && renders_differ)
            {
              /* First two renders differ: the runner pins the pre-action snapshot and the
                 finalize replace is a no-op on the already-plain text (no 3rd
                 draw). */
              free (e2);
              emit_message_body (run, e1, 0, out, m1);
              free (m1);
            }
          else
            {
              free (e1);
              free (e2);
              free (m1);
              emit_completion (run, comp, out);   /* finalize: 3rd render + display */
            }
        }
      else
        {
          /* The message draws AND this task's actions may draw too (AoK's
             Task999 magpie miss: message = <# Either(...) #>, actions =
             Execute Task998 = Variable206 RAND(1,100)).  Evaluating the
             compare/finalize before the actions (as above) would put the
             Either draws before the roll, while the runner's real order is render1 ->
             actions -> render2 (-> finalize) -- that misordering desynced the
             whole xoshiro stream from the magpie chase onward.  Interleave:
             freeze the template and take the pre-action snapshot now, run the
             actions into a side buffer below (the message still precedes their
             output, vb:1189 iResponsePosition), compare after them.

             The template is frozen via eval_description ONCE, exactly like
             the runner's `sMessage = task.CompletionMessage.ToString` (segment
             selection + DisplayOnce retirement happen HERE, vb:1167 -- real
             render, not bTestingOutput): the pre/post/finalize renders rework
             that string with the function/expression pass only.  Re-selecting
             segments per render instead broke Spectre of Castle Coris: the
             banish task's actions hide the Spectre, so a live re-select
             dropped the Either-bearing segment from the post-action compare
             and its draw vanished from the stream. */
          {
            a5_mark_guard mg (run->st, 1);
            flat_raw = a5text_eval_description (run->st, comp);
            run->st->marking_display = 0;   /* the frozen re-render is a test */
            flat_pre = a5text_process_frozen (run->st, flat_raw, &flat_pre_ink,
                                              &flat_pre_marked);
          }
          flat_inter = 1;
        }
    }

  /* clsUserSession.vb:1193 sets `task.Completed = True` *before* ExecuteActions
     (vb:1194), so an action that runs or gates on this task's own completion sees
     it complete -- e.g. Anno's Translatin runs `SetTasks Execute SettingOff`, and
     SettingOff requires `Translatin Must BeComplete`.  (Callers also mark it after
     run_task; this earlier mark is the one the runner relies on.) */
  if (self_ti >= 0)
    run->st->task_done[self_ti] = 1;

  /* The runner's stock Look is an AggregateOutput task, so its room view is deferred to
     the command's final Display -- a later action in THIS task's list that moves
     an object out of the room is reflected in the view even though the view is
     positioned ahead of that action's output.  FoF's `give pail to baker` runs
     Task317 (Baker Bakes Bread), whose action list does `Execute Look` and THEN
     `Execute Task2572` (Farrier Takes Pail, hiding the pail); The runner shows the bakery
     WITHOUT the pail followed by the farrier's "picks up the pail" line.  When an
     `Execute Look` is not the last action here, defer it (its slot is recorded at
     look_pos) and splice the final-state view back in after the remaining actions
     have run.  The last-action case (the usual "move then look") is unchanged. */
  int local_defer_look = 0;
  int prev_dl = run->defer_look, prev_lp = run->look_pending;
  size_t prev_lpos = run->look_pos;
  char *prev_lpin = run->look_pinned;
  if (act != NULL && !flat_inter && run->resp == NULL && !run->defer_look)
    {
      for (const a5_xml_node_t *c = act->first_child; c != NULL; c = c->next)
        if (streq (c->name, "SetTasks") && c->next != NULL
            && streq (c->text, "Execute Look"))
          { local_defer_look = 1; break; }
      if (local_defer_look)
        { run->defer_look = 1; run->look_pending = 0; run->look_pinned = NULL; }
    }

  if (act != NULL)
    {
      int saved_sti = run->cur_score_ti;
      run->cur_score_ti = self_ti;
      const a5_xml_node_t *c;
      for (c = act->first_child; c != NULL; c = c->next)
        run_action (run, c->name, c->text, depth, flat_inter ? &flat_abuf : out);
      run->cur_score_ti = saved_sti;
    }

  if (local_defer_look)
    {
      int did_defer = run->look_pending;
      size_t lpos = run->look_pos;
      char *lpin = run->look_pinned;
      run->defer_look = prev_dl;
      run->look_pending = prev_lp;
      run->look_pos = prev_lpos;
      run->look_pinned = prev_lpin;
      if (did_defer && lpos <= (size_t) out->len)
        {
          std::string view;
          if (lpin != NULL)
            view = lpin;                       /* pinned pre-action render */
          else
            view = render_look_marked (run);   /* final-state room view */
          /* Splice the view at its recorded slot: head | view | tail, where tail
             (the output the remaining actions produced) already carries its own
             leading pSpace separator. */
          std::string tail (out->p + lpos, out->len - lpos);
          out->len = lpos;
          if (out->p) out->p[lpos] = '\0';
          sb_pspace (out);
          sb_puts (out, view.c_str ());
          sb_puts (out, tail.c_str ());
        }
      free (lpin);
    }

  if (flat_inter)
    {
      /* Post-action compare + finalize (clsUserSession.vb:1200-1205), on the
         FROZEN template: pin the pre-action snapshot when the renders differ
         (the runner displays the test render; no third draw), else render once more
         for display (the finalize draw, real marking). */
      char *post_marked = NULL;
      char *post;
      {
        a5_mark_guard mg (run->st, 0);
        post = a5text_process_frozen (run->st, flat_raw, NULL, &post_marked);
      }
      /* Compare the MARKED renders: that is the string the runner holds in
         sMessage (clsUserSession.vb:1200), so a change confined to markup --
         Beginner's Cave's `<# "<" & rand(0,1000000) & ">" #>` cache-buster --
         counts, and the runner then pins the pre-action text instead of
         drawing a third time. */
      int changed = (flat_pre_marked != NULL && post_marked != NULL
                     && strcmp (flat_pre_marked, post_marked) != 0);
      free (post_marked);
      if (flat_pre != NULL && post != NULL && changed)
        { emit_message_body (run, flat_pre, flat_pre_ink, out, flat_pre_marked);
          flat_pre = NULL; }
      else
        {
          int fin_ink = 0;
          char *fin_marked = NULL;
          char *fin;
          {
            a5_mark_guard mg (run->st, 1);
            fin = a5text_process_frozen (run->st, flat_raw, &fin_ink,
                                         &fin_marked);
          }
          emit_message_body (run, fin, fin_ink, out, fin_marked);
          free (fin_marked);
        }
      free (post);
    }
  free (flat_pre);
  free (flat_pre_marked);
  free (flat_raw);
  if (flat_abuf.len > 0) { sb_pspace (out); sb_puts (out, flat_abuf.p); }
  free (flat_abuf.p);

  if (before && comp != NULL && run->resp != NULL)
    {
      char *post_marked = NULL;
      char *post = render_comp_test_ex (run->st, comp, &post_marked);
      /* Marked compare, as in the flat path above (vb:1200's sMessage still
         carries its markup). */
      int same = (resp_pre_marked != NULL && post_marked != NULL
                  && strcmp (resp_pre_marked, post_marked) == 0);
      free (post_marked);
      if (t->aggregate && resp_pre != NULL && post != NULL && same)
        resp_add_comp (run, t, comp, true, resp_pos);      /* deferred + merge */
      else if (resp_pre != NULL)
        resp_add_text (run, resp_pre, true, resp_pos);     /* pinned pre-action */
      free (post);
    }
  free (resp_pre);
  free (resp_pre_marked);

  if (!before && comp != NULL)
    {
      if (run->resp != NULL)
        resp_add_comp (run, t, comp, true);
      else if (run->comp_defers != NULL && t->aggregate
               && run->in_ev_attempt && run->ev_tbl != NULL)
        {
          /* Event/walk/LocationTrigger attempt: the runner adds this After
             completion per subtask pass via CompletionMessage.ToString -- the
             restriction-gated branch is selected EAGERLY (at this item's
             state, retiring DisplayOnce), and the resulting RAW branch text
             keys the response table.  A repeat of the same raw MERGES: one
             response, no second render, no second `<#..#>` draw.  A DIFFERENT
             raw from the same completion node is its own response (Quest
             Giver's daylight task landing on 5 mid-iteration).  Functions in a
             NEW entry render with the `<#..#>` draw deferred to the attempt's
             Display flush, mirroring the runner's aggregate expansion. */
          a5_state_t *st2 = run->st;
          char *raw;
          {
            a5_mark_guard mg (st2, 1);
            raw = a5text_eval_description (st2, comp);
          }
          std::string rawtext = raw != NULL ? raw : "";
          free (raw);
          if (!rawtext.empty ())
            {
              run->task_raw_output = 1;      /* the runner's bHasOutput */
              ev_resp_tbl *tb = run->ev_tbl;
              size_t e;
              for (e = 0; e < tb->keys.size (); e++)
                if (tb->keys[e] == (const void *) comp
                    && tb->texts[e] == rawtext)
                  break;
              if (e < tb->keys.size ())
                { tb->last = (int) e; tb->touches++; }
              else
                {
                  tb->keys.push_back ((const void *) comp);
                  tb->texts.push_back (rawtext);
                  tb->refs.push_back (std::vector<std::string> ());
                  tb->last = (int) tb->keys.size () - 1;
                  tb->touches++;
                  size_t sink0 = run->comp_defers->size ();
                  size_t out0 = out->len;
                  st2->expr_defer = run->comp_defers;
                  int ink = 0;
                  char *mk = NULL;
                  char *m;
                  {
                    a5_mark_guard mg (st2, 1);
                    m = a5text_process_frozen (st2, rawtext.c_str (),
                                               &ink, &mk);
                  }
                  st2->expr_defer = NULL;
                  emit_message_body (run, m, ink, out, mk);
                  free (mk);
                  if (out->len == out0)
                    run->comp_defers->resize (sink0);
                }
            }
        }
      else if (run->comp_defers != NULL && t->aggregate)
        {
          /* AggregateOutput completion on the eager command path: render the
             static skeleton now (so its position and pSpace separators match),
             but hold any random `<#..#>` draw to the end-of-command flush --
             the runner's AggregateOutput responses run ReplaceExpressions at Display,
             after the following actions' draws.  If the message renders empty
             (or is deduped away), roll back any recorded expressions so no
             orphan draw fires at flush. */
          size_t sink0 = run->comp_defers->size ();
          size_t out0 = out->len;
          run->st->expr_defer = run->comp_defers;
          emit_completion (run, comp, out);
          run->st->expr_defer = NULL;
          if (out->len == out0)
            run->comp_defers->resize (sink0);
        }
      else
        emit_completion (run, comp, out);
    }
}

/* The events + walks turn-based runtime (ev_* / wk_*) lives in
   a5run_events.cpp; its ev_init / ev_tick_all / ev_on_task_completed
   entry points are declared in a5run_internal.h. */
