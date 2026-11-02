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
 * ADRIFT 5 support for Scarier -- task execution.
 *
 * What clsUserSession does once a task is chosen: the specific-override task
 * dispatch (execute_task_with_overrides and its reference matching), the
 * run_task / run_general drivers with their completion-message rendering, and
 * the SetTasks-Execute response-scope flush.
 *
 * Split out of a5run.cpp, and since split further -- the subsystems this file
 * used to carry as well now sit beside it:
 *
 *   a5run_act.cpp    ExecuteActions / ExecuteSingleAction: the core action set
 *                    (MoveObject/MoveCharacter, Set/Inc/Dec, SetProperty,
 *                    SetTasks Execute, conversation, EndGame) and the seen-state
 *                    bookkeeping
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

#include "a5run_internal.h"

/* Debug/trace switches, resolved from the environment ONCE.  These sit on hot
   paths -- A5DBG_SPEC is tested per override child per item -- and the rest of
   the engine already resolves its trace switch a single time (the a5run_trace
   global, set by the host).  Function-local statics keep them private to this
   file rather than adding more globals to a5run.h for switches only these
   routines use; C++11 guarantees the one-time initialisation.  (a5run_act.cpp
   keeps the per-variable-action switches the same way.) */
static const char *
dbg_spec (void)
{
  static const char *v = getenv ("A5DBG_SPEC");
  return v;
}

static const char *
dbg_task (void)
{
  static const char *v = getenv ("A5_TRACE_TASK");
  return v;
}

/* Split a '|'-separated list, keeping empty fields (the shape of the runner's
   Items array for a pipe-list reference). */
std::vector<std::string>
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

/* Event/walk/LocationTrigger attempt bookkeeping: find the ev_resp_tbl entry
   for completion node COMP with raw text TEXT, inserting one when there is none,
   and make it the attempt's latest touched entry (the runner's AddResponse: a
   repeat of the same completion MERGES into its existing response; the table's
   final entry decides the post-Display NewReferences leftover -- see ev_tbl in
   a5run_internal.h).  Returns true when the entry is new. */
static bool
ev_tbl_touch (ev_resp_tbl *tb, const a5_xml_node_t *comp, const std::string &text)
{
  size_t e;
  for (e = 0; e < tb->keys.size (); e++)
    if (tb->keys[e] == (const void *) comp && tb->texts[e] == text)
      break;
  bool inserted = (e == tb->keys.size ());
  if (inserted)
    {
      tb->keys.push_back ((const void *) comp);
      tb->texts.push_back (text);
      tb->refs.push_back (std::vector<std::string> ());
    }
  tb->last = (int) e;
  tb->touches++;
  return inserted;
}

/* The object keys currently bound as this task-chain's %objects%/%object%
   reference -- the ReferencedObjects pipe list when present, else the singular
   ReferencedObject1.  Used for the runner's pass-cancels-fail response rule (see
   exec_pass_refs in a5run_internal.h). */
std::vector<std::string>
current_obj_ref_keys (a5_state_t *st)
{
  std::vector<std::string> v;
  const char *p = a5state_lookup_ref (st, "ReferencedObjects");
  if (p != nullptr && p[0] != '\0')
    {
      for (const std::string &k : split_pipe (p))
        if (!k.empty ()) v.push_back (k);
      return v;
    }
  p = a5state_lookup_ref (st, "ReferencedObject1");
  if (p != nullptr && p[0] != '\0')
    v.push_back (p);
  return v;
}

static std::string render_look_string (a5_run_t *run);
static void emit_completion (a5_run_t *run, const a5_xml_node_t *comp, sb_t *out);

/* Render the room view as REAL output -- marking_display=1 so its <DisplayOnce>
   segments retire -- restoring the previous marking flag afterwards.  The common
   wrapper around render_look_string at every final-state Display site (and, via
   a5run_look, the frontend's post-UNDO room redisplay).

   Unlike the game-start/restore views (Display(ViewLocation) at vb:229/3142,
   whose ViewLocation getter evaluates OUTSIDE Display, bDisplaying=False), a
   look/movement room view is the stock Look task's AggregateOutput
   "%Player%.Location.Description", replaced INSIDE Display under
   bDisplaying=True -- so its %CharacterName% renders run clsCharacter.Name's
   Introduced dance (definite-article upgrade + marking, clsCharacter.vb:331-334;
   SampleConversation's second room shows "the old lady" after the conversation
   introduced her).  Set intro_active for the render to match. */
std::string
render_look_marked (a5_run_t *run)
{
  a5_mark_guard mg (run->st, 1);
  int pia = run->st->intro_active;
  run->st->intro_active = 1;
  std::string v = render_look_string (run);
  run->st->intro_active = pia;
  return v;
}

/* ----------------------------------------- specific-override task dispatch */

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
  ref_info ri; ri.type = 't'; ri.key = nullptr;
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
  if (ri.key == nullptr && !num.empty () && num != "1")
    ri.key = a5state_lookup_ref (st, ("Referenced" + stem + num).c_str ());
  if (ri.key == nullptr)
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
std::vector<std::string>
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
std::vector<ref_info>
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
char *
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
      const char *k = nullptr;
      if (lname == "objects")          k = a5state_lookup_ref (st, "ReferencedObjects");
      else if (lname.rfind ("object", 0) == 0)
        { std::string cap = "ReferencedObject"; if (lname.size () > 6) cap += lname.substr (6);
          k = a5state_lookup_ref (st, cap.c_str ());
          if (!k) k = a5state_lookup_ref (st, "ReferencedObject"); }
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
      if (k != nullptr) return strdup (k);
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
bool
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
bool
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
  if (pd == nullptr) return false;
  /* A mandatory property (the runner's clsProperty.Mandatory) is added to every
     applicable item at load, so clsItem.HasProperty is always true for it --
     StaticOrDynamic is the canonical case.  Otherwise keep the members that
     HAVE the property (clsItem.HasProperty), presence not value -- a valueless
     SelectionOnly marker (AlienDiver's `BlankCards.ObjectIsAC` = the available
     blank cards) counts even though a5state_entity_prop returns NULL for it. */
  const char *mnd = pd->node ? a5xml_child_text (pd->node, "Mandatory") : nullptr;
  int mandatory = (mnd != nullptr && strcmp (mnd, "0") != 0);

  int n = a5state_group_count (st, grp.c_str ());
  for (int i = 0; i < n; i++)
    {
      const char *mk = a5state_group_member_at (st, grp.c_str (), i);
      if (mk == nullptr) continue;
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
  if (key == nullptr || key[0] == '\0')
    return nullptr;                                  /* "" = match any */
  if (streq (key, "%Player%"))
    return a5state_player_key (st);
  if (strchr (key, '%') != nullptr)
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
  if (sp->n_keys == 1 && specific_key (st, sp->keys[0]) == nullptr) return 1;
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
          if (want == nullptr) { matched = 1; break; }       /* empty key = any */
          /* The runner compares the Specific key to the reference's matched possibility
             case-insensitively (RefsMatchSpecifics, clsUserSession.vb:634).  For a
             Text specific (e.g. Grandpa's `say %text%` -> `vnl_SayKennedy`,
             key "kennedy") the reference resolves to the typed text, so match it
             case-insensitively; entity keys stay exact. */
          if (refs[i].key != nullptr
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
  else if (parent != nullptr
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
  if (dbg_spec () != nullptr)
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

/* Run the After-override children into `buf` in priority order (the runner's
   AttemptToExecuteSubTask child scan): stop after the first that produces output
   unless it is ContinueAlways -- the bContinue rule.  Two After overrides of the
   same general+spec that both pass (Galen's Quest `put yellow gem in ceiling
   receptacle` matches PutAYellow AND PutYellowG1, both +2) must not both fire --
   only the higher-priority PutAYellow does, so the score tracks the runner (40,
   not 42).

   An After child is evaluated AFTER the parent's actions ran, so its
   restrictions see the post-action state.  With SHOW_FAILS a failure shows its
   restriction message like any child fail (vb:1244; the parent's own output has
   already been Displayed, so no cancel applies) and claims the scan.  Head
   Case's `open rear doors`: the AfterTextAndActions Daza8Task24 ("MustNot
   BeInState Open" -> "They are already open.") always fails against the
   freshly-opened doors, and the runner prints "You open the van rear doors.
   They are already open."  The look-deferred and defer-text branches of
   execute_task_with_overrides run the children into a side buffer without it. */
static void
run_after_children (a5_run_t *run, const std::vector<const a5_task_t *> &after,
                    sb_t *buf, int show_fails)
{
  a5_state_t *st = run->st;
  for (auto *child : after)
    if (a5restr_pass (st, child->restrictions))
      {
        size_t alen0 = sink_len (run, buf);
        run->task_raw_output = 0;
        run_task (run, child, 0, buf);
        int ai = a5state_task_index (st, child->key);
        if (ai >= 0) st->task_done[ai] = 1;   /* task.Completed = True */
        ev_on_task_completed (run, child->key, buf);   /* fire its controls too */
        if ((sink_len (run, buf) > alen0 || run->task_raw_output)
            && !child->continue_lower) break;
      }
    else if (show_fails)
      {
        const a5_xml_node_t *fm = a5restr_fail_message (st, child->restrictions);
        if (fm != nullptr)
          {
            /* vb:1247 hands sRestrictionText to AddResponse raw, with no
               ReplaceFunctions on any path, so it expands inside Display. */
            char *fmsg;
            { a5_intro_guard ig (st, 1); fmsg = a5text_describe (st, fm); }
            if (msg_has_output (fmsg))
              {
                sb_pspace (buf);
                sb_puts (buf, fmsg);
                free (fmsg);
                break;               /* fail with output claims the scan */
              }
            free (fmsg);
          }
      }
}

/*
 * One execute_task_with_overrides() frame: the task being run with the
 * references it was matched on, and what the scan of its Specific override
 * children has decided so far -- whether the parent's own text and actions
 * still run, and which After children wait behind them.
 */
struct override_frame_t {
  a5_run_t *run;
  const a5_task_t *parent;
  const std::vector<ref_info> &refs;
  int depth;
  sb_t *out;
  a5_state_t *st;

  int parent_text, parent_actions;
  int any_child_fail_output;    /* The runner's bAnyChildHasOutput (vb:1053/1141) */
  std::vector<const a5_task_t *> after;
  size_t frame_len0;

  override_frame_t (a5_run_t *run_, const a5_task_t *parent_,
                    const std::vector<ref_info> &refs_, int depth_,
                    sb_t *out_)
    : run (run_), parent (parent_), refs (refs_), depth (depth_), out (out_),
      st (run_->st), parent_text (1), parent_actions (1),
      any_child_fail_output (0), frame_len0 (sink_len (run_, out_)) {}

  std::vector<int> children () const;
  bool child_failed (const a5_task_t *child, size_t len0);
  bool child_passed (int cidx, const a5_task_t *child, size_t len0);
  void recheck_item_restrictions ();
  void run_parent (int defer_text);
  void emit_deferred_look (const char *pinned_view);
  void emit_deferred_text ();
  void record_pass_signature ();
  void execute ();
};

/* The parent's override children, in the order the runner scans them. */
std::vector<int>
override_frame_t::children () const
{
  /* A command-bearing General has override children; a Specific override
     (depth>0) is itself scanned for its OWN nested children -- e.g. GetAWooden
     overrides TakeObjectFromLocation, which overrides TakeObjects.  Build the
     child list the way clsTask.Children does: the parent's Specific tasks in
     XML load order, completed non-repeatable ones filtered out (the property's
     bIncludeCompleted=False gate -- the runner's vb:730 entry check never sees them),
     then .NET-introsorted by Priority so equal-priority ties land exactly
     where the runner's unstable sort puts them (net_introspective_sort,
     a5run_sort.cpp). */
  std::vector<int> kids;
  if ((parent->n_commands > 0 || depth > 0) && depth < A5_MAX_ACTION_DEPTH)
    {
      for (int i = 0; i < run->adv->n_tasks; i++)
        {
          const a5_task_t *t = &run->adv->tasks[i];
          if (!streq (t->type, "Specific")) continue;
          if (t->general_key == nullptr || !streq (t->general_key, parent->key))
            continue;
          if (!t->repeatable && st->task_done[i]) continue;
          kids.push_back (i);
        }
      net_introspective_sort (run->adv, kids);
    }
  return kids;
}

/*
 * CHILD matches the references but fails its restrictions.  Return true if
 * that ends the scan of the parent's children.
 */
bool
override_frame_t::child_failed (const a5_task_t *child, size_t len0)
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
    return true;
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
  int lazy = run->resp != nullptr && child_all_any (st, child);
  int fail_claims = 0;
  if (fm != nullptr)
    {
      if (run->resp != nullptr)
        { resp_add_fail (run, fm); any_child_fail_output = 1; }
      else
        {
          /* vb:1247 hands sRestrictionText to AddResponse raw, with no
             ReplaceFunctions on any path, so it expands inside Display. */
          char *fmsg;
          { a5_intro_guard ig (st, 1); fmsg = a5text_describe (st, fm); }
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
              if (run->exec_scope != nullptr)
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
    return true;                  /* fail with output: claims the turn */
  return false;                   /* no output (or HPPT): keep scanning */
}

/*
 * CHILD matches the references and passes its restrictions: run it in place
 * of, or ahead of, the parent.  Return true if that ends the scan.
 */
bool
override_frame_t::child_passed (int cidx, const a5_task_t *child, size_t len0)
{
  const char *ot = child->override_type;

  /* Recurse so the child's own Specific overrides fire in its place
     (AttemptToExecuteSubTask -> recursive AttemptToExecuteTask). */
  size_t ovf0 = run->exec_scope != nullptr
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
  if (ot == nullptr || streq (ot, "Override"))
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
  int subtree_fail_output = run->exec_scope != nullptr
                            && run->exec_scope->ov_fails.size () > ovf0;
  if ((sink_len (run, out) > len0
       || (subtree_fail_output && !st->adv->hp_passing))
      && !child->continue_lower)
    return true;
  return false;
}

void
override_frame_t::recheck_item_restrictions ()
{
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
  if ((parent_text || parent_actions) && depth == 0 && run->resp != nullptr
      && !run->resp->cur_item.empty ()
      && !a5restr_pass (st, parent->restrictions))
    {
      const a5_xml_node_t *fm = st->restriction_text;
      if (fm != nullptr)
        resp_add_fail (run, fm);
      parent_text = parent_actions = 0;
    }
}

/* Run what the override children left of the parent's own text and actions. */
void
override_frame_t::run_parent (int defer_text)
{
  if (parent_text || parent_actions)
    {
      /* run_task emits both text and actions; gate via temporary copies. */
      if (!parent_text && parent_actions)
        {
          const a5_xml_node_t *act = parent->actions;
          if (act != nullptr)
            for (const a5_xml_node_t *c = act->first_child; c; c = c->next)
              run_action (run, c->name, c->text, 0, out);
        }
      else if (parent_text && !defer_text)
        run_task (run, parent, 0, out);
    }
}

/*
 * The parent's room view was deferred behind its After children: run them,
 * then emit the view (PINNED_VIEW if the Look dance pinned one) ahead of
 * their output.
 */
void
override_frame_t::emit_deferred_look (const char *pinned_view)
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
  run_after_children (run, after, &after_buf, 0);
  /* A pinned view (the Look dance's two test renders differed -- a random
     pick moved) is emitted verbatim; otherwise render at the final state
     (real output, so <DisplayOnce> segments retire). */
  std::string view;
  if (pinned_view != nullptr)
    view = std::string (pinned_view);
  else
    view = render_look_marked (run);
  /* The runner's response map holds ONE slot for the stock Look's raw template:
     a second Execute Look in the same command (The Salvage's request
     docking Looks at the docked pad, then on the bridge) re-renders that
     slot rather than appending -- first slot's position, last text. */
  exec_resp_scope *sc = run->exec_scope;
  if (sc != nullptr && sc->look_off >= 0 && sc->look_sink == out
      && (size_t) sc->look_off + sc->look_len <= out->len)
    {
      sb_splice (out, (size_t) sc->look_off, sc->look_len, view.c_str ());
      sc->look_len = view.size ();
    }
  else
    {
      sb_pspace (out);
      if (sc != nullptr)
        { sc->look_sink = out; sc->look_off = (long) out->len;
          sc->look_len = view.size (); }
      sb_puts (out, view.c_str ());
    }
  if (after_buf.len > 0) { sb_pspace (out); sb_puts (out, after_buf.p); }
  free (after_buf.p);
}

/*
 * An aggregate parent's text was deferred behind its After children: run
 * them, then emit the parent text ahead of their output.
 */
void
override_frame_t::emit_deferred_text ()
{
  /* Children run into a side buffer so the parent text can be spliced ahead
     of them once their actions have updated state. */
  sb_t after_buf; sb_init (&after_buf);
  run_after_children (run, after, &after_buf, 0);
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
  if (streq (parent->key, "Look") && parent->aggregate && sc != nullptr
      && run->display_defers != nullptr)
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
  else if (comp != nullptr)
    {
      /* defer_text is gated on parent->aggregate, so this render stands in
         for the runner's Display-time expansion: bDisplaying. */
      a5_intro_guard ig (st, 1);
      emit_completion (run, comp, out);
    }
  if (after_buf.len > 0) { sb_pspace (out); sb_puts (out, after_buf.p); }
  free (after_buf.p);
}

void
override_frame_t::record_pass_signature ()
{
  /* This frame produced pass output on the direct path: record its POSITIONAL
     reference signature (the runner keys every htblResponsesPass entry with the
     subtask's NewReferences) for the exec_scope ov_fails cancel rule. */
  if (run->resp == nullptr && run->exec_scope != nullptr
      && sink_len (run, out) > frame_len0)
    {
      std::vector<std::string> sig;
      for (auto &ri : refs)
        sig.push_back (ri.key ? ri.key : "");
      run->exec_scope->pass_sigs.push_back (sig);
    }
}

void
override_frame_t::execute ()
{
  if (dbg_task ())
    fprintf (stderr, "TASK %-14s @draw %ld depth=%d\n",
             parent->key ? parent->key : "?", a5rand_draw_count, depth);

  for (int cidx : children ())
    {
      const a5_task_t *child = &run->adv->tasks[cidx];
      if (!refs_match_specifics (st, child, parent, refs)) continue;

      const char *ot = child->override_type;
      int is_after = ot != nullptr && strncmp (ot, "After", 5) == 0;
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
          if (child_failed (child, len0))
            break;
          continue;
        }
      if (child_passed (cidx, child, len0))
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
  int defer_text = run->resp == nullptr && parent_text && parent->aggregate
                   && parent->actions == nullptr && !after.empty ();

  /* If the parent has After children that run actions on the direct path, defer
     its room view (Execute Look) so it renders at the post-child state, mirroring
     the runner's AggregateOutput Look deferral (see emit_look). */
  int prev_defer_look = run->defer_look;
  int prev_look_pending = run->look_pending;
  size_t prev_look_pos = run->look_pos;
  char *prev_look_pinned = run->look_pinned;
  int enable_look_defer = run->resp == nullptr && !defer_text && !after.empty ();
  if (enable_look_defer)
    { run->defer_look = 1; run->look_pending = 0; run->look_pinned = nullptr; }

  recheck_item_restrictions ();
  run_parent (defer_text);

  run->defer_look = prev_defer_look;
  /* Only the frame that enabled deferral consumes the pending look; a nested
     non-enabling frame (e.g. MovePlayer, run via SetTasks Execute, which emits
     the actual Look) leaves it pending for the enabling parent to render after
     its After children run.  The Look's offset is into the enabling frame's
     `out`, which is the same sink threaded through the nested calls. */
  int look_deferred = 0;
  char *pinned_view = nullptr;
  if (enable_look_defer)
    {
      if (run->look_pending) look_deferred = 1;
      pinned_view = run->look_pinned;          /* consumed below (or freed) */
      run->look_pending = prev_look_pending;
      run->look_pos = prev_look_pos;
      run->look_pinned = prev_look_pinned;
    }

  if (look_deferred)
    emit_deferred_look (pinned_view);
  else if (defer_text)
    emit_deferred_text ();
  else
    run_after_children (run, after, out, 1);
  free (pinned_view);

  record_pass_signature ();
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
void
execute_task_with_overrides (a5_run_t *run, const a5_task_t *parent,
                             const std::vector<ref_info> &refs, int depth,
                             sb_t *out)
{
  override_frame_t frame (run, parent, refs, depth, out);
  frame.execute ();
}

/* Run a matched General task (the directly command-matched parent), honouring
   its Specific child overrides. */
void
run_general (a5_run_t *run, const a5_task_t *parent, const a5_match_t *m,
             sb_t *out)
{
  a5_state_t *st = run->st;

  /* Where the plural %objects% and the %direction% references sit in the
     matched command, if at all. */
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
  bool all_failover = (objidx >= 0 && parent->fail_override != nullptr
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
      run->resp = nullptr;
      /* No child override passed for an "all" command: replace the buffered
         per-item fail messages with the general task's FailOverride (the runner's
         htblResponsesPass.Count=0 branch).  vb:789 renders it with a literal
         Display(task.FailOverride.ToString), hence under bDisplaying. */
      if (all_failover)
        {
          bool any_pass = false;
          for (auto &e : rm.ents)
            if (e.is_pass) { any_pass = true; break; }
          if (!any_pass)
            {
              char *fo;
              { a5_intro_guard ig (st, 1);
                fo = a5text_describe (st, parent->fail_override); }
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

char *
a5run_draw_defer_entry (a5_run_t *run, size_t idx,
                        std::unordered_map<size_t, std::string> &vals)
{
  const std::string &entry = (*run->display_defers)[idx];
  const char *body = entry.c_str ();
  long orig = -1;
  if (body[0] == '\005')
    {
      /* Repeat of entry <orig> within one block: draw, then show <orig>. */
      char *q = nullptr;
      orig = strtol (body + 1, &q, 10);
      body = (q != nullptr && *q == '\005') ? q + 1 : body;
    }
  char *val;
  if (body[0] == '\003')
    /* Raw `<#..#>` body held whole (its %ref[RAND]% substitution draws):
       substitute AND reduce here, at the flush. */
    val = a5text_eval_expression (run->st, body + 1);
  else
    val = (body[0] == '\001')
            ? a5text_process_noalr (run->st, body + 1)
            : a5_eval_sexpr (body);
  if (val == nullptr)
    val = strdup ("");
  if (orig >= 0 && vals.count ((size_t) orig))
    {
      free (val);
      val = strdup (vals[(size_t) orig].c_str ());
    }
  vals[idx] = val;
  return val;
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
  if (sink == nullptr || sink->size () <= from)
    return;
  void *prev_ed = run->st->expr_defer;
  run->st->expr_defer = nullptr;                 /* no re-deferral during the draw */

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
        const char *at = out->p != nullptr ? strstr (out->p, mark) : nullptr;
        /* A sentinel with no text position was emitted inside a tag that the
           renderer then stripped (OS/PlugIn.Exe holds its pause in
           `<wait %Tpaus%>`, whose `<#rand(5, 9)#>` still has to DRAW in text
           order).  Give it the previous entry's key so the stable sort keeps
           it in push order rather than banishing it to the end. */
        if (at != nullptr)
          lastpos = (long) (at - out->p);
        sslots.push_back (k - from);
        spos.push_back (std::make_pair (lastpos, k));
      }
    std::stable_sort (spos.begin (), spos.end ());
    for (size_t i = 0; i < sslots.size (); i++)
      slot_entry[sslots[i]] = spos[i].second;
  }

  std::unordered_map<size_t, std::string> drawn;
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
      else
        val = a5run_draw_defer_entry (run, k, drawn);
      char mark[24];
      int ml = snprintf (mark, sizeof mark, "\004%d\004", (int) k);
      char *at = out->p != nullptr ? strstr (out->p, mark) : nullptr;
      if (at != nullptr)
        {
          size_t pos = (size_t) (at - out->p);
          /* The runner reduces expressions INSIDE Display, so its
             CapAfterFullStop pass still sees the drawn value; ours ran back at
             a5text_process time, before this splice.  Re-apply the cap for a
             value that lands at a sentence start (OS/PlugIn.Exe's card line
             begins with the deferred `<#if(..)#>`, "four of Diamonds"). */
          if (val != nullptr && islower ((unsigned char) val[0])
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
exec_scope_flush (a5_run_t * /* run */, exec_resp_scope *sc, sb_t *out)
{
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

/* ------------------------------------------------------------- run one task */

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
      const char *dk = dedup_key != nullptr ? dedup_key : m;
      /* Within an event-fired task chain, dedup identical completion messages
         (the runner's htblResponsesPass, keyed by text): show the first, drop repeats.
         Keeps the first occurrence's position -- which, for a message emitted
         before a later <cls>, means it is wiped by that clear (Pathway's banner). */
      if (run->ev_seen != nullptr)
        {
          if (run->ev_seen->count (dk)) { free (m); return; }
          run->ev_seen->insert (dk);
        }
      /* Per-command pass-response text dedup (the runner's htblResponsesPass keying):
         a command that reaches the same completion text twice -- e.g. two
         Execute'd tasks with identical messages (Euripides' `press on` runs
         cl_ToCrawler11 AND cl_ToCrawler12, both the crawler journey) -- shows
         it once, keeping the first occurrence's position. */
      if (run->exec_scope != nullptr
          && !run->exec_scope->pass_seen.insert (dk).second)
        { free (m); return; }
      /* A pass response with output: record its bound object references for
         the pass-cancels-fail rule (the runner keys every AddResponse with the
         subtask's reference keys and drops a fail whose refs all passed). */
      if (run->exec_scope != nullptr)
        for (auto &k : current_obj_ref_keys (run->st))
          run->exec_scope->pass_refs.insert (k);
      sb_pspace (out); sb_puts (out, m);
    }
  else if (pre_alr_ink && m != nullptr && m[0] != '\0')
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
  char *marked = nullptr;
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
  if (run->in_ev_attempt && run->ev_tbl != nullptr && raw_nonblank)
    ev_tbl_touch (run->ev_tbl, comp, std::string ());
  emit_message_body (run, m, pre_alr_ink, out, marked);
  free (marked);
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
      look != nullptr ? a5xml_child (look->node, "CompletionMessage") : nullptr;
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

  if (comp != nullptr)
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
          if (restr != nullptr && !a5restr_pass (st, restr))
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

void
emit_look (a5_run_t *run, sb_t *out)
{
  /* Defer the room view until the command's After children have run (the runner's
     AggregateOutput Look render at final Display).  Record the insertion offset;
     execute_task_with_overrides renders and splices it once at final state. */
  if (run->defer_look && run->resp == nullptr)
    {
      if (!run->look_pending)
        { run->look_pending = 1; run->look_pos = out->len; }
      return;
    }

  if (run->resp != nullptr)
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

/* The stock Look task, run as its own routine: the completion is the ADRIFT
   property expression "%Player%.Location.Description" plus a restriction-gated
   darkness override, so the CompletionMessage is rendered (render_look_string)
   instead of the room view directly, and a dark location shows the game's
   "too dark" line.  SELF_TI is the task's index for the done mark and the
   score attribution. */
static void
run_task_look (a5_run_t *run, const a5_task_t *t, int depth, sb_t *out,
               int self_ti)
{
  a5_state_t *st = run->st;

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
  /* The probes run inside Display in the runner (bDisplaying=True AND
     bTestingOutput=True, vb:1177-1203), so they DO run clsCharacter.Name's
     Introduced dance and record PronounKeys entries -- which is why, with a
     character in view, the second probe pronoun-replaces the name recorded
     by the first ("she") and the response pins to the FIRST probe's text
     (vb:1200).  That pinned text must therefore be a bDisplaying render:
     set intro_active for both probes (marking stays 0 -- bTestingOutput
     keeps DisplayOnce retirement off).  SampleConversation `s` after the
     chat: the pinned view says "the old lady", and the probe's Introduced
     mark plus the end-of-turn visibility reset makes the next `look` say
     "an old lady" again. */
  auto probe_view = [&] (void) -> std::string {
      int pia = st->intro_active;
      st->intro_active = 1;
      std::string v;
      { a5_mark_guard mg (st, 0); v = render_look_string (run); }
      st->intro_active = pia;
      return v;
  };
  std::string e1 = probe_view ();

  /* The response slot is reserved BEFORE the actions run (vb:1189
     iResponsePosition), so any output the actions produce follows the
     room view -- Grandpa's tutorial lines land after the view. */
  int resp_pos = -1;
  int claimed_pending = 0;
  sb_t abuf; sb_init (&abuf);
  if (run->resp != nullptr)
    resp_pos = (int) run->resp->ents.size ();
  else if (run->defer_look && !run->look_pending)
    { run->look_pending = 1; run->look_pos = out->len; claimed_pending = 1; }

  if (self_ti >= 0) st->task_done[self_ti] = 1;
  int saved_sti = run->cur_score_ti;
  run->cur_score_ti = self_ti;
  if (t->actions != nullptr)
    for (const a5_xml_node_t *c = t->actions->first_child; c; c = c->next)
      run_action (run, c->name, c->text, depth,
                  (run->resp == nullptr && !run->defer_look) ? &abuf : out);
  run->cur_score_ti = saved_sti;

  std::string e2 = probe_view ();

  if (e1 != e2)
    {
      /* Pinned to the pre-actions render (vb:1200 sMessage = sBefore...). */
      if (run->resp != nullptr)
        resp_add_text (run, e1.c_str (), true, resp_pos);
      else if (run->defer_look)
        {
          if (claimed_pending)
            { free (run->look_pinned);
              run->look_pinned = strdup (e1.c_str ()); }
        }
      else if (run->exec_scope == nullptr
               || run->exec_scope->pass_seen.insert (e1).second)
        { sb_pspace (out); sb_puts (out, e1.c_str ()); }
    }
  else if (run->resp != nullptr)
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
      if (run->exec_scope == nullptr
          || run->exec_scope->pass_seen.insert (v).second)
        {
          exec_resp_scope *sc = run->exec_scope;
          if (sc != nullptr && t->aggregate && sc->look_off >= 0
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
              if (sc != nullptr && t->aggregate)
                { sc->look_sink = out; sc->look_off = (long) out->len; }
              sb_puts (out, v.c_str ());
              if (sc != nullptr && t->aggregate)
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
}

/* What the phases of a general run_task share.  The Before message is
   rendered ahead of the actions and compared with a second render after
   them, so its pre-action state outlives the action loop.  Built by
   run_task, which owns the sequence:
   run_task_before_message, the done mark, run_task_actions,
   run_task_before_finalize, run_task_after_message. */
struct run_task_ctx
{
  a5_run_t *run;
  a5_state_t *st;
  const a5_task_t *t;
  int depth;
  sb_t *out;
  int before;                         /* MessageBeforeOrAfter is not After */
  const a5_xml_node_t *comp;          /* CompletionMessage, or null */
  const a5_xml_node_t *act;           /* the action list, or null */
  int self_ti;                        /* this task's index, -1 if unknown */

  /* Before message on the resp path: the pre-action test render, its
     marked form, and the response slot it claimed. */
  char *resp_pre = nullptr;
  char *resp_pre_marked = nullptr;
  int   resp_pos = -1;

  /* Before message on the flat interleaved path: frozen template, the
     pre-action snapshot, and the side buffer the actions write into. */
  char *flat_raw = nullptr;
  char *flat_pre = nullptr;
  char *flat_pre_marked = nullptr;
  int   flat_pre_ink = 0;
  int   flat_inter = 0;
  sb_t  flat_abuf;

  /* Immediate-Display splice point: where this task's output begins in `out`,
     recorded BEFORE the Before message is emitted.  An action Display that the
     runner sends straight to sOutputText (vb:1748) precedes the buffered
     response, i.e. lands here.  (imm frame set around the action loop,
     flat paths only.) */
  long imm0 = 0;
};

/* Phase 1: the Before completion message, rendered ahead of the actions.
   On the map path a "Before" message follows the Adrift 5 runner's
   sBeforeActionsMessage logic (clsUserSession.vb:1176-1205): it claims its
   slot in the response order now, is rendered once before the actions, and
   after the actions is either pinned to that pre-action text (if the actions
   changed its rendering -- e.g. TakeObjectsFromOthersLazy's "(from
   %objects%.Parent.Name)", whose parent flips to the player once the take
   runs) or, for an unchanged AggregateOutput task, left deferred so a second
   item merges into its %objects% list. */
static void
run_task_before_message (run_task_ctx &c)
{
  a5_run_t *run = c.run;
  a5_state_t *st = c.st;
  const a5_task_t *t = c.t;
  const a5_xml_node_t *comp = c.comp;
  sb_t *out = c.out;

  c.imm0 = (long) out->len;
  if (!(c.before && comp != nullptr))
    return;

  if (run->resp != nullptr)
    {
      c.resp_pre = render_comp_test_ex (st, comp, &c.resp_pre_marked);
      c.resp_pos = (int) run->resp->ents.size ();
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
          a5_mark_guard mg (st, 1);
          free (a5text_eval_description (st, comp));
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
      char *m1 = nullptr, *m2 = nullptr;
      char *e1 = render_comp_test_ex (st, comp, &m1);
      char *e2 = render_comp_test_ex (st, comp, &m2);
      int renders_differ = (m1 != nullptr && m2 != nullptr && strcmp (m1, m2) != 0);
      free (m2);
      if (e1 != nullptr && e2 != nullptr && renders_differ)
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
          /* Finalize.  For a non-aggregate task this is vb:1185/1204's
             eager replace, the one completion render the runner makes with
             bDisplaying clear; an aggregate task skips that line entirely
             and its raw template expands inside Display instead. */
          a5_intro_guard ig (st, t->aggregate);
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
         actions into a side buffer (run_task_actions; the message still
         precedes their output, vb:1189 iResponsePosition), compare after
         them (run_task_before_finalize).

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
        a5_mark_guard mg (st, 1);
        c.flat_raw = a5text_eval_description (st, comp);
        st->marking_display = 0;   /* the frozen re-render is a test */
        /* vb:1177's probe: bTestingOutput AND bDisplaying (the freeze above
           is CompletionMessage.ToString, which precedes both). */
        a5_intro_guard ig (st, 1);
        c.flat_pre = a5text_process_frozen (st, c.flat_raw, &c.flat_pre_ink,
                                            &c.flat_pre_marked);
      }
      c.flat_inter = 1;
    }
}

/* Phase 2: the action list, with an `Execute Look` that is not the last
   action deferred and spliced back in at the final state.
   The runner's stock Look is an AggregateOutput task, so its room view is
   deferred to the command's final Display -- a later action in THIS task's
   list that moves an object out of the room is reflected in the view even
   though the view is positioned ahead of that action's output.  FoF's `give
   pail to baker` runs Task317 (Baker Bakes Bread), whose action list does
   `Execute Look` and THEN `Execute Task2572` (Farrier Takes Pail, hiding the
   pail); The runner shows the bakery WITHOUT the pail followed by the
   farrier's "picks up the pail" line.  When an `Execute Look` is not the last
   action here, defer it (its slot is recorded at look_pos) and splice the
   final-state view back in after the remaining actions have run.  The
   last-action case (the usual "move then look") is unchanged. */
static void
run_task_actions (run_task_ctx &c)
{
  a5_run_t *run = c.run;
  const a5_xml_node_t *act = c.act;
  sb_t *out = c.out;
  int local_defer_look = 0;
  int prev_dl = run->defer_look, prev_lp = run->look_pending;
  size_t prev_lpos = run->look_pos;
  char *prev_lpin = run->look_pinned;

  if (act != nullptr && !c.flat_inter && run->resp == nullptr
      && !run->defer_look)
    {
      for (const a5_xml_node_t *n = act->first_child; n != nullptr; n = n->next)
        if (streq (n->name, "SetTasks") && n->next != nullptr
            && streq (n->text, "Execute Look"))
          { local_defer_look = 1; break; }
      if (local_defer_look)
        { run->defer_look = 1; run->look_pending = 0; run->look_pinned = nullptr; }
    }

  if (act != nullptr)
    {
      int saved_sti = run->cur_score_ti;
      sb_t *prev_isink = run->imm_sink;
      long  prev_ipos = run->imm_pos;
      run->cur_score_ti = c.self_ti;
      /* Flat paths only: the resp path's flush order and the Look dance keep
         their existing append behaviour.  defer_look is fine -- the splice
         helper shifts look_pos when it inserts ahead of a recorded slot. */
      if (run->resp == nullptr)
        { run->imm_sink = out; run->imm_pos = c.imm0; }
      const a5_xml_node_t *n;
      for (n = act->first_child; n != nullptr; n = n->next)
        run_action (run, n->name, n->text, c.depth,
                    c.flat_inter ? &c.flat_abuf : out);
      run->cur_score_ti = saved_sti;
      run->imm_sink = prev_isink;
      run->imm_pos = prev_ipos;
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
          if (lpin != nullptr)
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
}

/* Phase 3: the Before message's post-action compare and finalize, on
   whichever path phase 1 took, and the release of its renders. */
static void
run_task_before_finalize (run_task_ctx &c)
{
  a5_run_t *run = c.run;
  a5_state_t *st = c.st;
  const a5_task_t *t = c.t;
  const a5_xml_node_t *comp = c.comp;
  sb_t *out = c.out;

  if (c.flat_inter)
    {
      /* Post-action compare + finalize (clsUserSession.vb:1200-1205), on the
         FROZEN template: pin the pre-action snapshot when the renders differ
         (the runner displays the test render; no third draw), else render once more
         for display (the finalize draw, real marking). */
      char *post_marked = nullptr;
      char *post;
      {
        a5_mark_guard mg (st, 0);
        a5_intro_guard ig (st, 1);        /* vb:1199's probe */
        post = a5text_process_frozen (st, c.flat_raw, nullptr, &post_marked);
      }
      /* Compare the MARKED renders: that is the string the runner holds in
         sMessage (clsUserSession.vb:1200), so a change confined to markup --
         Beginner's Cave's `<# "<" & rand(0,1000000) & ">" #>` cache-buster --
         counts, and the runner then pins the pre-action text instead of
         drawing a third time. */
      int changed = (c.flat_pre_marked != nullptr && post_marked != nullptr
                     && strcmp (c.flat_pre_marked, post_marked) != 0);
      free (post_marked);
      if (c.flat_pre != nullptr && post != nullptr && changed)
        { emit_message_body (run, c.flat_pre, c.flat_pre_ink, out,
                             c.flat_pre_marked);
          c.flat_pre = nullptr; }
      else
        {
          int fin_ink = 0;
          char *fin_marked = nullptr;
          char *fin;
          {
            a5_mark_guard mg (st, 1);
            a5_intro_guard ig (st, t->aggregate);   /* vb:1204 finalize */
            fin = a5text_process_frozen (st, c.flat_raw, &fin_ink,
                                         &fin_marked);
          }
          emit_message_body (run, fin, fin_ink, out, fin_marked);
          free (fin_marked);
        }
      free (post);
    }
  free (c.flat_pre);
  free (c.flat_pre_marked);
  free (c.flat_raw);
  if (c.flat_abuf.len > 0) { sb_pspace (out); sb_puts (out, c.flat_abuf.p); }
  free (c.flat_abuf.p);

  if (c.before && comp != nullptr && run->resp != nullptr)
    {
      char *post_marked = nullptr;
      char *post = render_comp_test_ex (st, comp, &post_marked);
      /* Marked compare, as in the flat path above (vb:1200's sMessage still
         carries its markup). */
      int same = (c.resp_pre_marked != nullptr && post_marked != nullptr
                  && strcmp (c.resp_pre_marked, post_marked) == 0);
      free (post_marked);
      if (t->aggregate && c.resp_pre != nullptr && post != nullptr && same)
        /* deferred + merge */
        resp_add_comp (run, t, comp, true, c.resp_pos);
      else if (c.resp_pre != nullptr)
        /* pinned pre-action */
        resp_add_text (run, c.resp_pre, true, c.resp_pos);
      free (post);
    }
  free (c.resp_pre);
  free (c.resp_pre_marked);
}

/* Phase 4: an After completion message, rendered once the actions ran. */
static void
run_task_after_message (run_task_ctx &c)
{
  a5_run_t *run = c.run;
  a5_state_t *st = c.st;
  const a5_task_t *t = c.t;
  const a5_xml_node_t *comp = c.comp;
  sb_t *out = c.out;

  if (c.before || comp == nullptr)
    return;

  if (run->resp != nullptr)
    resp_add_comp (run, t, comp, true);
  else if (run->comp_defers != nullptr && t->aggregate
           && run->in_ev_attempt && run->ev_tbl != nullptr)
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
      char *raw;
      {
        a5_mark_guard mg (st, 1);
        raw = a5text_eval_description (st, comp);
      }
      std::string rawtext = raw != nullptr ? raw : "";
      free (raw);
      if (!rawtext.empty ())
        {
          run->task_raw_output = 1;      /* the runner's bHasOutput */
          if (ev_tbl_touch (run->ev_tbl, comp, rawtext))
            {
              size_t sink0 = run->comp_defers->size ();
              size_t out0 = out->len;
              st->expr_defer = run->comp_defers;
              int ink = 0;
              char *mk = nullptr;
              char *m;
              {
                a5_mark_guard mg (st, 1);
                a5_intro_guard ig (st, 1);   /* aggregate: Display render */
                m = a5text_process_frozen (st, rawtext.c_str (),
                                           &ink, &mk);
              }
              st->expr_defer = nullptr;
              emit_message_body (run, m, ink, out, mk);
              free (mk);
              if (out->len == out0)
                run->comp_defers->resize (sink0);
            }
        }
    }
  else if (run->comp_defers != nullptr && t->aggregate)
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
      st->expr_defer = run->comp_defers;
      {
        a5_intro_guard ig (st, 1);     /* aggregate: Display render */
        emit_completion (run, comp, out);
      }
      st->expr_defer = nullptr;
      if (out->len == out0)
        run->comp_defers->resize (sink0);
    }
  else
    {
      /* vb:1211's eager finalize for a non-aggregate After message (the
         flag is clear); an aggregate landing here has no defer sink to
         hold its draws, but it is still a Display-time expansion. */
      a5_intro_guard ig (st, t->aggregate);
      emit_completion (run, comp, out);
    }
}

void
run_task (a5_run_t *run, const a5_task_t *t, int depth, sb_t *out)
{
  run_task_ctx c;
  c.run = run;
  c.st = run->st;
  c.t = t;
  c.depth = depth;
  c.out = out;
  /* v5's FileIO.Load defaults a missing <MessageBeforeOrAfter> to Before (the
     class default After is overridden at load time, FileIO.vb:1618), so the
     completion message is evaluated and emitted *before* the actions run.  This
     is what makes TakeObjectsFromOthersLazy's "(from %objects%.Parent.Name)"
     render "(from the Table)" (parent still the source) ahead of the
     SetTasks-Execute sub-task's "You take ... from the Table." line. */
  const char *when = a5xml_child_text (t->node, "MessageBeforeOrAfter");
  c.before = !streq (when, "After");
  c.comp = a5xml_child (t->node, "CompletionMessage");
  c.act = t->actions;
  c.self_ti = a5state_task_index (c.st, t->key);
  sb_init (&c.flat_abuf);

  if (a5run_trace)
    fprintf (stderr, "  [run task %s]\n", t->key ? t->key : "?");

  if (streq (t->key, "Look"))
    {
      run_task_look (run, t, depth, out, c.self_ti);
      return;
    }

  run_task_before_message (c);

  /* clsUserSession.vb:1193 sets `task.Completed = True` *before* ExecuteActions
     (vb:1194), so an action that runs or gates on this task's own completion sees
     it complete -- e.g. Anno's Translatin runs `SetTasks Execute SettingOff`, and
     SettingOff requires `Translatin Must BeComplete`.  (Callers also mark it after
     run_task; this earlier mark is the one the runner relies on.) */
  if (c.self_ti >= 0)
    c.st->task_done[c.self_ti] = 1;

  run_task_actions (c);
  run_task_before_finalize (c);
  run_task_after_message (c);
}

/* The events + walks turn-based runtime (ev_* / wk_*) lives in
   a5run_events.cpp; its ev_init / ev_tick_all / ev_on_task_completed
   entry points are declared in a5run_internal.h. */
