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
 * ADRIFT 5 support for Scarier -- the action set.
 *
 * clsUserSession's ExecuteActions / ExecuteSingleAction: the per-kind handlers
 * for MoveObject / MoveCharacter, AddObjectToGroup and the other group edits,
 * Set / Inc / Dec and the SetTasks FOR body, SetProperty, SetTasks Execute /
 * Unset, Conversation, Time and EndGame, with the run_action dispatcher that
 * selects one by action kind.  The seen-state bookkeeping the movement actions
 * drive (update_seen, mark_player_arrival_seen, the LocationTrigger queue) sits
 * in front of them.
 *
 * Split out of a5run_action.cpp, which keeps the task side: the specific-
 * override dispatch and the run_task / run_general drivers.  The two are
 * mutually recursive by design (SetTasks Execute re-enters
 * execute_task_with_overrides; a task's actions run through run_action), and
 * what they share -- ref_info, A5_MAX_ACTION_DEPTH, the operand helpers -- is
 * declared in a5run_internal.h.
 */

#include "a5run_internal.h"

/* Debug/trace switches, resolved from the environment ONCE: both are tested per
   variable action, so a function-local static keeps the lookup off that path
   (as a5run_action.cpp does for its own switches). */
static const char *
dbg_setvar (void)
{
  static const char *v = getenv ("A5_TRACE_SETVAR");
  return v;
}

static const char *
dbg_score (void)
{
  static const char *v = getenv ("A5_DEBUG_SCORE");
  return v;
}

/* ---------------------------------------------------------- operand helpers */

/* The adventure's own (stable, DOM-aliased) pointer for an entity key.  Object,
   character and location go through the model's hashed key index; only groups
   (which the index does not cover) still need a scan.  The family order is
   load-bearing -- a key present in two families resolves to the earlier one --
   so it is preserved exactly. */
static const char *
canon_key (const a5_adventure_t *adv, const char *k)
{
  int i;
  if (k == nullptr) return nullptr;
  i = a5model_key_index (adv, 'O', k);
  if (i >= 0) return adv->objects[i].key;
  i = a5model_key_index (adv, 'C', k);
  if (i >= 0) return adv->characters[i].key;
  i = a5model_key_index (adv, 'L', k);
  if (i >= 0) return adv->locations[i].key;
  for (i = 0; i < adv->n_groups; i++)
    if (streq (adv->groups[i].key, k))     return adv->groups[i].key;
  return nullptr;
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
  if (tok == nullptr) return nullptr;
  if (streq (tok, "%Player%"))
    resolved = a5state_player_key (st);  /* dynamic: whoever is the player now */
  else if (streq (tok, "Player"))
    resolved = "Player";                 /* literal model key of the player slot */
  else
    { b = a5state_lookup_ref (st, tok); resolved = b ? b : tok; }
  c = canon_key (st->adv, resolved);
  if (c != nullptr)
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
  if (raw != nullptr && strncmp (raw, "RAND", 4) == 0)
    {
      /* RAND (min, max): inclusive random in [min, max] via the shared
         erkyrath_random (the Adrift 5 runner Global.Random(iMin, iMax)).  The data
         writes it as "RAND (1, 4)" (spaces optional).  A bound may itself be a
         %variable% (LostCoastlines' item valuation writes `RAND (0, %valuator%)`);
         the runner resolves it via ReplaceFunctions before Global.Random, so substitute
         %tokens% first -- otherwise the bound parses as 0 and the whole
         age/decoration/aura draw collapses to a no-op RAND(0,0). */
      char *sub = (strchr (raw, '%') != nullptr)
                    ? a5text_process_noalr (st, raw) : nullptr;
      long drawn = a5rand_between_args ((sub ? sub : raw) + 4);
      free (sub);
      return drawn;
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
     IS that evaluator, ^ included (clsVariable reduces ^ on run 2, the +/-
     tier, left-to-right -- see FrankenDrift clsVariable.vb).  %vars% are
     already substituted, so Tingalan's Roller "rand(0,%playerwits%)" arrives
     as rand(0,N) and draws via the same
     a5rand_between as the uppercase RAND path.

     The value is Val() of the *result* -- its leading integer, else 0.  So a
     non-numeric value ("Object3") reads as 0, as does an out-of-range result
     the runner guards with SafeInt(Val(...)): a division by zero evaluates to
     "\xe2\x88\x9e" (Val 0), not the leading digit of the raw "5/0". */
  {
    char *sv = a5_eval_sexpr (proc);
    long e = strtol (sv, nullptr, 10);      /* Val(): leading integer, else 0 */
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

/* ------------------------------------------------- seen-state bookkeeping */

/* Mark every character the player can currently see as "seen" (player-centric
   clsCharacter.HasSeenCharacter, set each turn by clsUserSession's
   PrepareForNextTurn).  The player is always considered self-seen. */
void
update_seen (a5_state_t *st)
{
  const char *ploc = a5state_player_location (st);
  int i;
  if (st->char_seen == nullptr)
    return;
  for (i = 0; i < st->adv->n_characters; i++)
    {
      if (streq (st->adv->characters[i].key, a5state_player_key (st)))
        { st->char_seen[i] = 1; continue; }
      /* Resolve through the on/in-object chain, not char_loc alone -- a
         character whose model start is "On Object" (GFS's Grandpa on his
         rocking chair) has char_loc == NULL and is located via the
         furniture. */
      if (ploc != nullptr && a5state_character_at_location (st, i, ploc))
        st->char_seen[i] = 1;
      /* PrepareForNextTurn also RESETS Introduced for any non-player character
         the player can no longer see (clsUserSession.vb:3794-3797), so a
         character re-encountered later gets the indefinite article again
         (SampleConversation: after leaving the garden, `look` in the south room
         still says "an old lady", not "the old lady"). */
      else if (st->char_introduced != nullptr)
        st->char_introduced[i] = 0;
    }
  /* Mark every object currently visible in the player's room as seen
     (clsCharacter.HasSeenObject), so HaveBeenSeenByCharacter persists once an
     object has been viewed. */
  if (st->obj_seen != nullptr && ploc != nullptr)
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
static void
mark_player_arrival_seen (a5_state_t *st, const char *loc)
{
  int i;
  a5state_mark_loc_seen (st, loc);
  if (loc == nullptr)
    return;
  if (st->obj_seen != nullptr)
    for (i = 0; i < st->adv->n_objects; i++)
      if (a5state_object_visible_at_location (st, i, loc, 0))
        st->obj_seen[i] = 1;
  if (st->char_seen != nullptr)
    for (i = 0; i < st->adv->n_characters; i++)
      if (!streq (st->adv->characters[i].key, a5state_player_key (st))
          && a5state_character_at_location (st, i, loc))
        st->char_seen[i] = 1;
}

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
  if (new_loc == nullptr || streq (old_loc, new_loc))
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

/* ------------------------------------------------------------------ actions */

/* The run_action per-kind handlers.  They share one signature so the dispatch
   table at the bottom of the section stays uniform; the operands a handler does
   not use are left unnamed. */

/* The optional property-value operand of a Move/group action: FileIO parses
   `<what> <key1> [value tokens...] <to> <key2>`, joining elements 2..len-3
   (space-separated) into act.sPropertyValue whenever the payload has more than
   4 tokens (FileIO.vb:498-517; the MoveCharacter and Add/RemoveLocationToGroup
   branches repeat the identical loop). */
static std::string
withprop_value (const std::vector<std::string> &tk)
{
  std::string v;
  if (tk.size () > 4)
    for (size_t i = 2; i + 2 < tk.size (); i++)
      { if (i > 2) v += ' '; v += tk[i]; }
  return v;
}

/* The With-Property selector test shared by the MoveObject / MoveCharacter /
   MoveLocation bulk sources (clsUserSession.vb:2058/2292/2542): HasProperty,
   and -- unless the property is a SelectionOnly (valueless) marker, where
   presence alone matches -- GetPropertyValue must string-equal the action's
   sPropertyValue operand.  The merged (runtime + static) table is consulted so
   a property added at runtime via SetProperty is seen here. */
static int
withprop_matches (const a5_state_t *st, const char *entkey,
                  const char *propkey, const std::string &propval)
{
  const a5_propdef_t *pd;
  const char *v;
  if (!a5state_entity_has_prop (st, entkey, propkey))
    return 0;
  pd = a5model_propdef (st->adv, propkey);
  if (pd != nullptr && streq (pd->type, "SelectionOnly"))
    return 1;
  v = a5state_entity_prop (st, entkey, propkey);
  return propval == (v != nullptr ? v : "");
}

/* clsUserSession.vb:1479 MoveObjectWhat: the source of an object-set action is
   either a single Object or one of the "Everything*" sets (a group's members,
   everything held/worn by a character, inside/on an object, at a location, or
   carrying a property).  MoveObject and AddObjectToGroup/RemoveObjectFromGroup
   share the enum, so they share this collector.  Returns 0 for an unrecognised
   selector (the caller then does nothing, as the runner does). */
static int
collect_object_source (a5_state_t *st, const std::string &what,
                       const char *srckey, const char *rawkey,
                       const std::string &propval,
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
      /* clsUserSession.vb:1484: `Not ob.IsStatic AndAlso DynamicExistWhere =
         InLocation` -- the sweep takes DYNAMIC objects directly in the room
         only; static furniture stays put. */
      for (int i = 0; i < st->adv->n_objects; i++)
        if (!st->obj[i].is_static
            && st->obj[i].where == A5_OWHERE_LOCATION
            && streq (st->obj[i].key, srckey))
          targets.push_back (i);
    }
  else if (what == "EverythingWithProperty")
    {
      for (int i = 0; i < st->adv->n_objects; i++)
        if (withprop_matches (st, st->adv->objects[i].key, rawkey, propval))
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
  /* tk[0] is the source selector, tk[1] the source entity; the destination
     kind/key pair reads from the END of the payload, because a WithProperty
     selector may carry a multi-token property value in between
     (withprop_value).  Collect the affected object indices, then apply the
     same per-object move to each. */
  const std::string &what = tk[0];
  const char *srckey = act_key (st, tk[1].c_str ());
  const std::string &to = tk[tk.size () - 2];
  const char *k2 = act_key (st, tk[tk.size () - 1].c_str ());
  std::vector<int> targets;

  if (!collect_object_source (st, what, srckey, tk[1].c_str (),
                              withprop_value (tk), targets))
    return;

  for (int oi : targets)
    {
      a5_objloc_t *L = &st->obj[oi];
      if (to == "ToCarriedBy")      { L->where = A5_OWHERE_HELD_BY;   L->key = k2; }
      else if (to == "ToWornBy")    { L->where = A5_OWHERE_WORN_BY;   L->key = k2; }
      else if (to == "OntoObject")  { L->where = A5_OWHERE_ON_OBJECT; L->key = k2; }
      else if (to == "InsideObject") { L->where = A5_OWHERE_IN_OBJECT; L->key = k2; }
      else if (to == "ToPartOfObject") { L->where = A5_OWHERE_PART_OBJECT; L->key = k2; }
      else if (to == "ToPartOfCharacter") { L->where = A5_OWHERE_PART_CHAR; L->key = k2; }
      else if (to == "ToLocation")
        { if (streq (k2, "Hidden")) { L->where = A5_OWHERE_HIDDEN; L->key = nullptr; }
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
              else { L->where = A5_OWHERE_HIDDEN; L->key = nullptr; }
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
              if (loc != nullptr) { L->where = A5_OWHERE_LOCATION; L->key = loc; }
              else             { L->where = A5_OWHERE_HIDDEN;   L->key = nullptr; }
            }
          else
            {
              int oj = a5state_object_index (st, k2);
              if (oj >= 0) { L->where = st->obj[oj].where; L->key = st->obj[oj].key; }
              else         { L->where = A5_OWHERE_HIDDEN;  L->key = nullptr; }
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
  const char *grp = tk[tk.size () - 1].c_str ();
  const char *srckey = act_key (st, tk[1].c_str ());
  std::vector<int> targets;
  /* The selector enum is MoveObject's (clsAction shares AddToGroupWhat with
     MoveObjectWhat), so the whole "Everything*" family is legal here too --
     Quest Giver's setup task seeds its live deck with
     `AddObjectToGroup EverythingInGroup daz6QuestDeckM ToGroup daz6CurrentQue`,
     and with only the single-Object form implemented the deck stayed at its
     three authored members, so the game dealt from 3 cards instead of 20.
     The group key reads from the END: a WithProperty selector may put a
     multi-token property value between key1 and "ToGroup". */
  if (!collect_object_source (st, tk[0], srckey, tk[1].c_str (),
                              withprop_value (tk), targets))
    return;
  for (int oi : targets)
    a5state_set_object_in_group (st, grp, st->adv->objects[oi].key, add);
}

/* The "who" selectors shared by MoveCharacter and Add/RemoveCharacterToGroup
   (clsAction reuses MoveCharacterWhoEnum, clsUserSession.vb:1689/1841): collect
   the affected character indices from the action's tokens, tk[0] the selector
   and tk[1] its key.  Returns 0 for a selector it does not know. */
static int
collect_character_who (a5_state_t *st, const std::vector<std::string> &tk,
                       std::vector<int> &cis)
{
  const std::string &who = tk[0];
  const char *whok = act_key (st, tk[1].c_str ());

  if (who == "Character")
    { int ci = a5state_character_index (st, whok); if (ci >= 0) cis.push_back (ci); }
  else if (who == "EveryoneAtLocation")
    {
      /* clsLocation.CharactersDirectlyInLocation(True): directly at the
         location (not on/in an object), Player included. */
      for (int i = 0; i < st->adv->n_characters; i++)
        if (streq (st->char_loc[i], whok) && st->char_onobj[i] == nullptr)
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
  else if (who == "EveryoneWithProperty")
    {
      /* Here tk[1] is the property, taken as written, and its value follows. */
      std::string propval = withprop_value (tk);
      for (int i = 0; i < st->adv->n_characters; i++)
        if (withprop_matches (st, st->adv->characters[i].key,
                              tk[1].c_str (), propval))
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
  const char *grp = tk[tk.size () - 1].c_str ();
  int add = streq (kind, "AddCharacterToGroup");
  std::vector<int> cis;
  if (!collect_character_who (st, tk, cis))
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
    st->char_onobj[ci] = nullptr;
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
    return nullptr;
  if (cur != nullptr && a5state_object_at_location (st, oj, cur, directly))
    return cur;
  for (int li = 0; li < st->adv->n_locations; li++)
    if (a5state_object_at_location (st, oj, st->adv->locations[li].key, directly))
      return st->adv->locations[li].key;
  return nullptr;
}

/* Display(sText) straight to the runner's sOutputText (clsUserSession.vb:298):
   an action's immediate output precedes the task's buffered response, so when
   run_task published a splice point, insert there (with the pSpace join rules
   on both seams); otherwise append -- an After message already follows the
   action position.  Recorded offsets into the sink (the deferred-Look slot)
   are shifted past the insertion. */
static void
imm_display (a5_run_t *run, const char *msg, sb_t *out)
{
  sb_t *sink = run->imm_sink;
  size_t at;
  if (sink == nullptr || run->imm_pos < 0 || (size_t) run->imm_pos > sink->len)
    {
      sb_pspace (out);
      sb_puts (out, msg);
      return;
    }
  at = (size_t) run->imm_pos;
  std::string ins;
  if (at > 0 && sink->p[at - 1] != '\n' && sink->p[at - 1] != A5_CLS_MARK)
    ins += "  ";
  ins += msg;
  if (at < sink->len && sink->p[at] != '\n' && sink->p[at] != ' ')
    ins += "  ";
  sb_splice (sink, at, 0, ins.c_str ());
  run->imm_pos = (long) (at + ins.size ());
  if (run->look_pending && run->look_pos >= at)
    run->look_pos += ins.size ();
  if (run->exec_scope != nullptr && run->exec_scope->look_sink == sink
      && run->exec_scope->look_off >= (long) at)
    run->exec_scope->look_off += (long) ins.size ();
}

/* The MoveCharacter destinations, one function per MoveCharacterToEnum form
   that takes more than a line: character `ci` goes to (or by) target `k2`.
   act_move_character holds what the forms share.  */

static void
move_char_in_direction (a5_run_t *run, int ci, const char *k2, sb_t *out)
{
  a5_state_t *st = run->st;
  const char *k1 = st->adv->characters[ci].key;
  const char *canon = a5parse_canonical_direction (k2);
  const char *here = st->char_loc[ci];
  const a5_xml_node_t *blocked = nullptr;
  const char *dest;
  if (canon == nullptr) canon = k2;    /* already canonical */
  dest = here ? a5restr_exit_in_direction (st, k1, here, canon,
                                           &blocked)
              : nullptr;
  if (dest == nullptr && blocked != nullptr)
    {
      /* Route restriction failed: the runner Displays the blocking
         restriction's message and abandons the move (clsUserSession.vb:
         1748 Display(sRestrictionText)) -- immediate output, so it
         precedes the task's buffered completion message.  It is a
         literal Display() call, hence bDisplaying. */
      char *fmsg;
      { a5_intro_guard ig (st, 1);
        fmsg = a5text_describe (st, blocked); }
      if (msg_has_output (fmsg))
        imm_display (run, fmsg, out);
      free (fmsg);
    }
  if (dest != nullptr)
    {
      /* destination may be a location group -> first member */
      if (a5model_location (st->adv, dest) == nullptr)
        {
          for (int g = 0; g < st->adv->n_groups; g++)
            if (streq (st->adv->groups[g].key, dest)
                && st->adv->groups[g].n_members > 0)
              { dest = st->adv->groups[g].members[0]; break; }
        }
      relocate_char (run, ci, dest, 1, out);   /* now "at location" */
    }
}

static void
move_char_to_position (a5_run_t *run, int ci, const std::string &to,
                       const char *k2, sb_t *out)
{
  a5_state_t *st = run->st;
  const char *k1 = st->adv->characters[ci].key;
  const char *pos = (to == "ToSittingOn") ? "Sitting"
                  : (to == "ToLyingOn")   ? "Lying" : "Standing";
  free (st->char_position[ci]);
  st->char_position[ci] = strdup (pos);
  a5state_set_prop (st, k1, "CharacterPosition", pos);
  /* "TheFloor" means standing/sitting/lying at the location, not on an
     object (clsCharacterLocation.ExistsWhere AtLocation vs OnObject). */
  if (k2 == nullptr || streq (k2, "TheFloor"))
    st->char_onobj[ci] = nullptr;
  else
    {
      st->char_onobj[ci] = k2; st->char_in[ci] = 0;
      /* Seating someone on a chair in ANOTHER room moves them there --
         GFS's John Boom "invites you in" seats the player on the
         living-room cosy chair.  Scarier stores the room explicitly:
         sync it. */
      const char *loc = object_host_location (st, k2, st->char_loc[ci], 1);
      if (loc != nullptr && !streq (loc, st->char_loc[ci]))
        relocate_char (run, ci, loc, 0, out);
    }
}

static void
move_char_into_object (a5_run_t *run, int ci, const std::string &to,
                       const char *k2, sb_t *out)
{
  a5_state_t *st = run->st;
  /* clsUserSession MoveCharacterToEnum.InsideObject / OntoObject:
     the character is now in/on the object
     (clsCharacterLocation.ExistsWhere InsideObject/OnObject).
     char_in distinguishes the two so BeInsideObject / BeOnObject
     read it back correctly -- e.g. FBA's `hide in niche`
     (MoveCharacter Player InsideObject cl_Niche1) which the custodian
     "goes past" check gates on. */
  if (k2 != nullptr)
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
      if (loc != nullptr && !streq (loc, st->char_loc[ci]))
        relocate_char (run, ci, loc, 0, out);
    }
}

static void
move_char_onto_character (a5_state_t *st, int ci, const char *k2)
{
  const char *k1 = st->adv->characters[ci].key;
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
        || (st->char_onchar[ti] != nullptr
            && streq (st->char_onchar[ti], k1)));
  if (ti >= 0 && !recursive)
    {
      st->char_onchar[ci] = k2;
      st->char_loc[ci] = nullptr;      /* location resolves through carrier */
      st->char_onobj[ci] = nullptr;
      st->char_in[ci] = 0;
    }
}

static void
move_char_to_same_location (a5_run_t *run, int ci, const char *k2, sb_t *out)
{
  a5_state_t *st = run->st;
  const char *k1 = st->adv->characters[ci].key;
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
      st->char_loc[ci] = (k2 != nullptr)
        ? object_host_location (st, k2, nullptr, 1) : nullptr;
      st->char_onobj[ci] = nullptr;
    }
  if (streq (k1, a5state_player_key (st)))
    {
      enqueue_loc_trigger_tasks (run, old_loc, st->char_loc[ci]);
      clear_conv_if_partner_gone (run, out);
    }
}

static void
move_char_switch_with (a5_run_t *run, int ci, const char *k2, sb_t *out)
{
  a5_state_t *st = run->st;
  const char *k1 = st->adv->characters[ci].key;
  /* clsUserSession MoveCharacterToEnum.ToSwitchWith: swap character k1
     with k2.  When either is the current player, DON'T move anyone --
     change which character IS the player (ADRIFT's BECOME <character>
     mechanism).  The player viewpoint, %Player% resolution and scope
     then follow the new character; the old player stays put, now an
     NPC.  Otherwise, actually exchange the two characters' locations. */
  const char *pk = a5state_player_key (st);
  int bi = k2 ? a5state_character_index (st, k2) : -1;
  if (k2 == nullptr || bi < 0)
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
      /* Neither is the player.  The runner's swap (clsUserSession.vb:
         1837) exchanges the two clsCharacterLocation references -- but
         `dest` was pre-filled from ch's ORIGINAL location (vb:1727) and
         the shared epilogue `ch.Move(dest)` (vb:1902) then moves k1
         straight back.  Net observable effect, bug and all: k2 ends up
         at k1's original place; k1 stays exactly where it was. */
      st->char_loc[bi]    = st->char_loc[ci];
      st->char_onobj[bi]  = st->char_onobj[ci];
      st->char_in[bi]     = st->char_in[ci];
      st->char_onchar[bi] = st->char_onchar[ci];
    }
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
  std::vector<int> cis;
  if (!collect_character_who (st, tk, cis))
    return;
  if (cis.empty ())
    return;

  /* The destination pair reads from the END of the payload (a WithProperty
     selector may carry a multi-token property value in between). */
  const std::string &to = (tk.size () >= 4) ? tk[tk.size () - 2] : tk[2];
  /* Every MoveCharacterTo variant that takes a target (location, group,
     furniture, container, character, direction) reads it from the same token;
     act_key is a pure lookup, so resolve it once for the whole action. */
  const char *k2 = (tk.size () >= 4)
    ? act_key (st, tk[tk.size () - 1].c_str ()) : nullptr;
  /* ToLocationGroup: the runner computes dest.Key = group.RandomKey ONCE per action
     (clsUserSession.vb:1767), so the whole MoveCharacter draws a single
     RandomKey and sends every affected character to the SAME room -- e.g.
     Jacaranda's champagne `MoveCharacter %Player% ToLocationGroup Group7`. */
  const char *group_dest = nullptr;
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
      if (to != "OntoCharacter" && to != "ToSwitchWith"
          && st->char_onchar != nullptr)
        st->char_onchar[ci] = nullptr;
      if (to == "ToLocationGroup")
        relocate_char (run, ci, group_dest, 1, out);
      else if (to == "InDirection")
        move_char_in_direction (run, ci, k2, out);
      else if (to == "ToLocation")
        {
          const char *new_loc = streq (k2, "Hidden") ? nullptr : k2;
          relocate_char (run, ci, new_loc, 1, out);    /* now "at location" */
        }
      else if (to == "ToStandingOn" || to == "ToSittingOn" || to == "ToLyingOn")
        move_char_to_position (run, ci, to, k2, out);
      else if (to == "InsideObject" || to == "OntoObject")
        move_char_into_object (run, ci, to, k2, out);
      else if (to == "OntoCharacter")
        move_char_onto_character (st, ci, k2);
      else if (to == "ToParentLocation")
        {
          /* clsUserSession MoveCharacterToEnum.ToParentLocation: drop out of
             the containing object back onto the floor of the SAME location
             (clsCharacter.Move ... the character's location is unchanged, only
             the on/in-object binding clears) -- FBA's `out` of the niche. */
          st->char_onobj[ci] = nullptr;
          st->char_in[ci] = 0;
        }
      else if (to == "ToSameLocationAs")
        move_char_to_same_location (run, ci, k2, out);
      else if (to == "ToSwitchWith")
        move_char_switch_with (run, ci, k2, out);
      /* other MoveCharacterToEnum forms: best-effort no-op */

      /* clsCharacter.Move marks the destination location -- and, on an
         AtLocation arrival, its objects and visible characters -- seen for
         the moving character at move time; our set is player-centric (like
         obj_seen).  See mark_player_arrival_seen. */
      if (streq (k1, a5state_player_key (st)))
        {
          if (st->char_onobj[ci] == nullptr)
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
    const char *s = st->var_text[vi] != nullptr ? st->var_text[vi] : "";
    for (long i = 0; i < len && *s != '\0'; i++)
      {
        const char *nl = strchr (s, '\n');
        lines[(size_t) i].assign (s, nl != nullptr ? (size_t) (nl - s) : strlen (s));
        if (nl == nullptr)
          break;
        s = nl + 1;
      }
  }
  lines[(size_t) elem - 1] = val != nullptr ? val : "";
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

/* Run a SetVariable body of the form
   "FOR <var> = a TO b : SET <assignment> : NEXT <var>" (FileIO.vb:365-374,
   the SetVariable twin of act_set_tasks' Execute loop).  Unlike SetTasks,
   the counter IS visible to the body: the runner substitutes it wherever the
   loop variable appears, which in practice is the element index
   (`SET Points[Loop] = 0`) and the %Loop% reference.  OS/PlugIn.Exe deals
   its blackjack hand with `FOR Loop = 1 TO 3 : SET Bjvalue[Loop] =
   %cardValue[RAND(1, 13)]% : NEXT Loop`, so without this the whole hand
   stays 0 and every card renders as "Zero of ...".  Returns false if the body
   is not a FOR loop, and so is still to be run as a plain assignment. */
static bool
set_variable_for_loop (a5_run_t *run, const char *kind, const char *body,
                       int depth, sb_t *out)
{
  a5_state_t *st = run->st;
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
              long from = strtol (head.c_str () + eq + 1, nullptr, 10);
              long to   = strtol (head.c_str () + tp + 4, nullptr, 10);
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
              return true;
            }
        }
    }
  return false;
}

/* Resolve the element index `idxstr` of an assignment to variable `vi`,
   like clsUserSession.vb:2136-2144: a
   "ReferencedNumber" prefix reads the parser's number reference, a
   numeric literal is used as-is, anything else is a variable KEY whose
   (element-1) value is the index. */
static long
set_variable_element_index (a5_state_t *st, int vi, const std::string &idxstr)
{
  long elem_idx = 1;
  if (st->adv->variables[vi].array_length > 1 && !idxstr.empty ())
    {
      if (idxstr.compare (0, 16, "ReferencedNumber") == 0)
        {
          const char *rn = a5state_lookup_ref (st, "ReferencedNumber");
          elem_idx = rn != nullptr ? strtol (rn, nullptr, 10) : 0;
        }
      else if (idxstr.find_first_not_of ("0123456789 -") == std::string::npos)
        elem_idx = strtol (idxstr.c_str (), nullptr, 10);
      else
        {
          int ivi = a5state_variable_index (st, idxstr.c_str ());
          elem_idx = ivi >= 0 ? a5state_var_get_elem (st, ivi, 1) : 0;
        }
    }
  return elem_idx;
}

/* SetVariable on a Text variable: store the evaluated `value` (the RHS of
   `body`, less one layer of quotes) into element `text_elem` of variable
   `vi`, or the whole variable if text_elem is 0. */
static void
set_text_variable (a5_state_t *st, int vi, long text_elem,
                   const std::string &name, const std::string &value,
                   const char *body)
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
    double fver = (st->adv->version != nullptr) ? atof (st->adv->version) : 0;
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
              if (st->adv->variables[rvi].name != nullptr
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

/* Set, Inc or DecVariable (`kind`) on a numeric variable: apply `value` to
   element `elem_idx` of variable `vi`. */
static void
set_numeric_variable (a5_run_t *run, const char *kind, int vi, long elem_idx,
                      const std::string &value)
{
  a5_state_t *st = run->st;
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

static void
act_set_variable (a5_run_t *run, const char *kind,
                  const std::vector<std::string> & /*tk*/, const char *body,
                  int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  std::string name, value, idxstr;
  int vi;
  if (set_variable_for_loop (run, kind, body, depth, out))
    return;
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
  long elem_idx = set_variable_element_index (st, vi, idxstr);
  /* A subscript-less assignment to a Text array keeps the historic wholesale
     overwrite (it is how the newline-joined InitialValue is set en bloc); only
     an explicit `Var[i] =` replaces a single element. */
  long text_elem = idxstr.empty () ? 0 : elem_idx;
  if (streq (st->adv->variables[vi].type, "Text") && streq (kind, "SetVariable"))
    set_text_variable (st, vi, text_elem, name, value, body);
  else
    set_numeric_variable (run, kind, vi, elem_idx, value);
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
  const char *pt = pd ? pd->type : nullptr;
  int value_type = pt != nullptr
                   && (streq (pt, "Integer") || streq (pt, "Text")
                       || streq (pt, "StateList") || streq (pt, "ValueList"));
  int has_prop = a5state_entity_prop (st, k1, tk[1].c_str ()) != nullptr;
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
      if (ev != nullptr && ev[0] != '\0') val = ev;
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
    const char *kt = pd ? pd->type : nullptr;
    int is_key_prop = kt != nullptr
                      && (streq (kt, "ObjectKey") || streq (kt, "CharacterKey")
                          || streq (kt, "LocationKey"));
    if (is_key_prop && val.find ('%') == std::string::npos
        && val != "Nothing")
      {
        const char *rk = (val == "Player")
                         ? a5state_player_key (st)
                         : a5state_lookup_ref (st, val.c_str ());
        if (rk != nullptr && rk[0] != '\0') val = rk;
      }
  }
  /* A ValueList property stores the label's mapped INTEGER (FD clsProperty
     Value setter, clsProperty.vb:180-186; the model's static values get the
     same mapping at load, a5_fix_valuelist_props). */
  if (pt != nullptr && streq (pt, "ValueList"))
    {
      const char *mv = a5model_valuelist_value (pd, val.c_str ());
      if (mv != nullptr) val = mv;
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
          const char *w = val.c_str (), *kp = nullptr;
          if (streq (w, "Hidden"))
            { L->where = A5_OWHERE_HIDDEN;    L->key = nullptr; }
          else if (streq (w, "Held By Character"))
            { L->where = A5_OWHERE_HELD_BY;   kp = "HeldByWho"; }
          else if (streq (w, "Worn By Character"))
            { L->where = A5_OWHERE_WORN_BY;   kp = "WornByWho"; }
          else if (streq (w, "In Location"))
            { L->where = A5_OWHERE_LOCATION;  kp = "InLocation"; }
          else if (streq (w, "Inside Object"))
            { L->where = A5_OWHERE_IN_OBJECT; kp = "InsideWhat"; }
          else if (streq (w, "On Object"))
            { L->where = A5_OWHERE_ON_OBJECT; kp = "OnWhat"; }
          if (kp != nullptr)
            {
              const char *kv = a5state_entity_prop (st, k1, kp);
              if (kv == nullptr)
                {
                  const a5_object_t *mo = a5model_object (st->adv, k1);
                  for (int pi = 0; mo != nullptr && pi < mo->n_props; pi++)
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
      char *endp = nullptr;
      n = strtol (sv.c_str (), &endp, 10);
      if (endp == sv.c_str ())        /* not a literal: EvaluateExpression */
        {
          char *ev = a5text_eval_expression (st, sv.c_str ());
          if (ev != nullptr) { n = strtol (ev, nullptr, 10); free (ev); }
        }
    }
  if (run->resp != nullptr)
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

/* SetTasks
   "FOR Loop = a TO b : Execute <key> : NEXT Loop" (FileIO.vb:365-374:
   IntValue = element 3, sPropertyValue = element 5, the Execute starts
   at element 7).  clsUserSession's SetTasks case runs the plain Execute
   b-a+1 times; the loop counter itself is not visible to the executed
   task (only SetVariable-in-a-FOR substitutes %Loop%). */
static void
set_tasks_for_loop (a5_run_t *run, const std::vector<std::string> &tk,
                    int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  long from = strtol (tk[3].c_str (), nullptr, 10);
  long to   = strtol (tk[5].c_str (), nullptr, 10);
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
}

/* SetTasks "Execute Look": the built-in room view plus the Look task's own
   overrides and actions. */
static void
set_tasks_execute_look (a5_run_t *run, int depth, sb_t *out)
{
  a5_state_t *st = run->st;
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
  if (lt != nullptr && depth < A5_MAX_ACTION_DEPTH)
    execute_task_with_overrides (run, lt, std::vector<ref_info> (),
                                 depth, out);
  else
    emit_look (run, out);
}

/* One SetTasks "Execute <task> (<args>)": the target task, the arguments to
   bind to its references, and the member list when one argument is plural
   (giter is that argument's index, else -1).  */
struct exec_task_t {
  a5_run_t *run;
  a5_state_t *st;
  const a5_task_t *tt;
  sb_t *out;
  std::vector<std::string> args;
  std::vector<std::string> rnames;
  std::vector<std::string> gmembers;
  int giter;
  bool have_args;
  bool tt_done_at_entry;
  bool ran_any;

  exec_task_t (a5_run_t *run_, const a5_task_t *tt_, sb_t *out_)
    : run (run_), st (run_->st), tt (tt_), out (out_), giter (-1),
      have_args (false), tt_done_at_entry (false), ran_any (false) {}

  void find_plural_argument (void);
  void bind_arguments (void);
  void show_restriction_failure (void);
  void run_one (void);
  void run_members (ref_snap *parent_refs);
};

void
exec_task_t::find_plural_argument (void)
{
  /* A bare `Group.<Property>` argument (e.g. `Objects1.StaticOrDynamic`)
     is an OO reference that the runner expands to the group's member LIST and
     runs the executed task once per member (ExecuteSubTasks iterating
     the reference's items).  Detect it and iterate; a plain argument
     leaves giter < 0 and runs the task exactly once, as before. */
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
        if (val != nullptr && strchr (val, '|') != nullptr)
          {
            gmembers = split_pipe (val);
            giter = (int) i;
          }
        free (val);
        if (giter >= 0) break;
      }
}

void
exec_task_t::bind_arguments (void)
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

void
exec_task_t::show_restriction_failure (void)
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
  if (fm != nullptr)
    {
      /* vb:1247 hands sRestrictionText to AddResponse raw, with no
         ReplaceFunctions on any path, so it expands inside Display. */
      char *fmsg;
      { a5_intro_guard ig (st, 1); fmsg = a5text_describe (st, fm); }
      if (msg_has_output (fmsg))
        {
          if (run->resp != nullptr)
            resp_add_text (run, fmsg, false);
          else if (run->exec_scope != nullptr)
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
}

/* One execution of the target task with the references bound from
   the current `args` (the group member substituted into args[giter]
   on each pass).  Returns via early-out on a Completed/Repeatable or
   restriction gate -- the runner's AttemptToExecuteSubTask per item. */
void
exec_task_t::run_one (void)
{
  if (have_args)
    bind_arguments ();
  /* AttemptToExecuteTask: gate on Completed/Repeatable and the
     task's own restrictions (with any refs just bound above) --
     the stock list-runner tasks (e.g. the bomb cascade) execute a
     list of candidate tasks and rely on each one's restrictions to
     pick the one that fires. */
  if (tt_done_at_entry && !tt->repeatable)
    return;
  if (!a5restr_pass (st, tt->restrictions))
    {
      show_restriction_failure ();
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
}

void
exec_task_t::run_members (ref_snap *parent_refs)
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
      ref_snap_restore (st, parent_refs);
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
      int t0 = run->ev_tbl != nullptr ? run->ev_tbl->touches : 0;
      run_one ();
      if (run->in_ev_attempt && run->ev_tbl != nullptr
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

/* SetTasks "Execute <task>": run the task named by tk[1] as a sub-task of
   the one whose action this is. */
static void
set_tasks_execute (a5_run_t *run, const std::vector<std::string> &tk,
                   const char *body, int depth, sb_t *out)
{
  a5_state_t *st = run->st;
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
      set_tasks_execute_look (run, depth, out);
      return;
    }
  const a5_task_t *tt = a5model_task (st->adv, key.c_str ());
  if (tt == nullptr || depth >= A5_MAX_ACTION_DEPTH)
    return;
  exec_task_t x (run, tt, out);

  /* Optional "(arg1|arg2|...)" reference passing: bind the sub-task's
     command references to the evaluated argument keys, mirroring
     ExecuteTask's parameter substitution (used by the stock
     "take from others lazy" -> "take from others" re-dispatch). */
  std::string b = body ? body : "";
  {
    size_t lp = b.find ('(');
    size_t rp = b.rfind (')');
    if (lp != std::string::npos && rp != std::string::npos && rp > lp)
      {
        x.have_args = true;
        x.args = split_pipe (b.substr (lp + 1, rp - lp - 1).c_str ());
      }
  }
  x.rnames = command_refs (tt);
  x.find_plural_argument ();

  int tti = a5state_task_index (st, key.c_str ());

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
     card and over-awarded coloured fragments. */
  x.tt_done_at_entry = (tti >= 0 && st->task_done[tti]);

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
  if (x.giter >= 0)
    x.run_members (parent_refs.get ());
  else
    x.run_one ();
  ref_snap_restore (st, parent_refs.get ());

  /* Mark done + fire completion controls once per AttemptToExecuteTask
     (the runner flips task.Completed once), and only when the task actually
     ran -- a wholly-failing Execute leaves it uncompleted, as before. */
  if (x.ran_any)
    {
      if (tti >= 0)
        st->task_done[tti] = 1;
      ev_on_task_completed (run, key.c_str (), out);
    }
}

static void
act_set_tasks (a5_run_t *run, const char * /*kind*/,
               const std::vector<std::string> &tk, const char *body,
               int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  if (tk.empty ()) return;
  if (tk[0] == "FOR" && tk.size () >= 9 && tk[4] == "TO" && tk[6] == ":")
    set_tasks_for_loop (run, tk, depth, out);
  else if (tk[0] == "Execute" && tk.size () >= 2)
    set_tasks_execute (run, tk, body, depth, out);
  else if (tk[0] == "Unset" || tk[0] == "Clear")
    {
      if (tk.size () >= 2)
        {
          int ti = a5state_task_index (st, tk[1].c_str ());
          if (ti >= 0 && st->task_done[ti])
            {
              /* Unsetting a *completed* task fires UnCompletion controls
                 before the flag clears (clsUserSession's SetTasks-Unset case,
                 vb:2938-2980); unsetting an already-unset task is a no-op. */
              ev_on_task_uncompleted (run, st->adv->tasks[ti].key, out);
              st->task_done[ti] = 0;
            }
        }
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
      keytok = tk.back ();                                /* last = key */
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
  /* The group key reads from the END of the payload: an EverywhereWithProperty
     selector may carry a multi-token property value in between (FileIO.vb's
     Add/RemoveLocationToGroup parse repeats the same middle-join). */
  const char *grp = tk[tk.size () - 1].c_str ();
  int add = streq (kind, "AddLocationToGroup");
  std::vector<std::string> locs;
  if (what == "Location")
    locs.push_back (k1);
  else if (what == "LocationOf")
    {
      int ci = a5state_character_index (st, k1);
      if (ci >= 0)
        { const char *lk = st->char_loc[ci];
          if (lk != nullptr && !streq (lk, "Hidden")) locs.push_back (lk); }
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
      std::string propval = withprop_value (tk);
      for (int i = 0; i < st->adv->n_locations; i++)
        { const a5_location_t *l = &st->adv->locations[i];
          if (withprop_matches (st, l->key, tk[1].c_str (), propval))
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
