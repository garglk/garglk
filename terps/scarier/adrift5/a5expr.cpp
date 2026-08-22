/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- the property-expression engine.  See a5expr.h.
 *
 * A port of the Adrift 5 runner Global.ReplaceOOProperty (Global.vb:655-1620): a key (or
 * pipe-joined key list) navigated by ".Function(args)" steps.  The first key is
 * resolved to an object / character / location / group / direction (the VB "final
 * Else" block), then each step either navigates to a new item set
 * (Parent/Children/Contents/Location/Held/Worn/Objects) or terminates
 * (Name/List/Description/Count/Sum/<property>).
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "a5expr.h"
#include "a5parse.h"
#include "a5restr.h"
#include "a5text.h"
#include "a5util.h"
#include "a5xml.h"

static int
contains (const std::string &hay, const char *needle)
{
  return hay.find (needle) != std::string::npos;
}

/* Wired by the run harness to read the live event timers (NULL otherwise). */
int (*a5expr_event_prop_hook) (const char *event_key, const char *prop,
                               long *out) = NULL;

/* ----------------------------------------------------- item-set navigation */

/* A navigated item-set: a list of entity keys, plus an optional carried
   "property key" used by the .Sum aggregator, mirroring sPropertyKey. */
struct Ctx {
  std::vector<std::string> keys;   /* entity keys (objects/characters/locations) */
  std::vector<std::string> dirs;   /* direction names (the lstDirs path)         */
  std::string prop_key;            /* sPropertyKey carried for Sum               */
  int is_list;                     /* lst path (vs a single resolved item)       */
  int is_dirs;                     /* the lstDirs path (a .Exits direction list) */
  int is_event;                    /* an event item (keys[0] = event key)        */
  Ctx () : is_list (0), is_dirs (0), is_event (0) {}
};

/* The twelve directions in DirectionsEnum order (North=0 .. NorthWest=11);
   `.Exits` enumerates them in this order (Global.vb:1468). */
static const char *kEnumDirs[12] = {
  "North", "East", "South", "West", "Up", "Down",
  "In", "Out", "NorthEast", "SouthEast", "SouthWest", "NorthWest"
};

/* The localized, lowercased display name for a direction's `.List`
   (LCase(DirectionName) -- "NorthEast" -> "northeast", or the game's localized
   "Nordøst" -> "nordøst"). */
static std::string
dir_display (const std::string &canon)
{
  const char *nm = a5parse_direction_name (canon.c_str ());
  return lower (nm != NULL ? std::string (nm) : canon);
}

/* Join a direction list the way clsLocation.List does: ", " between, " and "
   (or " or " if the args ask) before the last. */
static std::string
render_dirs (const std::vector<std::string> &dirs, const std::string &args)
{
  std::string sep = contains (lower (args), "or") ? " or " : " and ";
  if (dirs.empty ())
    return "nothing";
  std::string out;
  int n = (int) dirs.size ();
  for (int i = 0; i < n; i++)
    {
      if (i > 0 && i < n - 1) out += ", ";
      if (n > 1 && i == n - 1) out += sep;
      out += dir_display (dirs[i]);
    }
  return out;
}

/* The directions (DirectionsEnum order) in which `charkey` has a usable route
   from `lockey` -- a5restr exit-restriction evaluation, unlike the raw
   a5model_exit_dest read used by a location's own `.Exits`. */
static std::vector<std::string>
char_exit_dirs (a5_state_t *st, const char *charkey, const char *lockey)
{
  std::vector<std::string> dirs;
  if (lockey != NULL)
    for (int i = 0; i < 12; i++)
      if (a5restr_exit_in_direction (st, charkey, lockey, kEnumDirs[i], NULL)
          != NULL)
        dirs.push_back (kEnumDirs[i]);
  return dirs;
}

/* clsCharacter.ListExits(sFromLocation, iExitCount): the character's routes
   evaluated FROM a given location (clsLocation.ViewLocation passes Me.Key, so
   a %DisplayLocation[other]% view lists the OTHER room's exits, not the
   player's -- Head Case's through-the-spyhole Car Park view).  Rendered like
   render_dirs; *count receives the exit count. */
char *
a5expr_list_exits (a5_state_t *st, const char *charkey, const char *lockey,
                   long *count)
{
  std::vector<std::string> dirs = char_exit_dirs (st, charkey, lockey);
  if (count != NULL)
    *count = (long) dirs.size ();
  return strdup (render_dirs (dirs, "").c_str ());
}

/* Objects held (HELD_BY) or worn (WORN_BY) directly by a character key. */
static std::vector<std::string>
objs_carried_by (a5_state_t *st, const char *charkey, a5_owhere_t where)
{
  std::vector<std::string> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    if (st->obj[i].where == where && streq (st->obj[i].key, charkey))
      v.push_back (st->adv->objects[i].key);
  return v;
}

/* Child-set filter for objs_children, mirroring the runner's WhereChildrenEnum
   (clsObject.Children): the `.Children(...)` / `.Contents` arg selects which of
   an object's inside/on children to return. */
enum { A5_CH_BOTH = 0, A5_CH_IN = 1, A5_CH_ON = 2 };

/* Objects directly inside and/or on a container object, filtered by MODE.
   the runner (clsObject.Children) filters strictly: `,On` yields ONLY OnObject
   children, `,In`/Contents ONLY InsideObject, plain/`OnAndIn` yields both --
   so a supporter query must not pick up an object that is merely inside. */
static std::vector<std::string>
objs_children (a5_state_t *st, const char *objkey, int mode)
{
  std::vector<std::string> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    {
      a5_owhere_t w = st->obj[i].where;
      int hit = (mode != A5_CH_ON && w == A5_OWHERE_IN_OBJECT)
              || (mode != A5_CH_IN && w == A5_OWHERE_ON_OBJECT);
      if (hit && streq (st->obj[i].key, objkey))
        v.push_back (st->adv->objects[i].key);
    }
  return v;
}

/* Characters directly inside and/or on an object, filtered by MODE (same
   In/On/Both semantics as objs_children).  A character sits on/in an object via
   char_onobj[i] (the object key) + char_in[i] (1=inside, 0=on surface). */
static std::vector<std::string>
chars_children (a5_state_t *st, const char *objkey, int mode)
{
  std::vector<std::string> v;
  for (int i = 0; i < st->adv->n_characters; i++)
    {
      const char *on = st->char_onobj ? st->char_onobj[i] : NULL;
      if (on == NULL || !streq (on, objkey))
        continue;
      int inside = st->char_in ? st->char_in[i] : 0;
      int hit = (mode != A5_CH_ON && inside)
              || (mode != A5_CH_IN && !inside);
      if (hit)
        v.push_back (st->adv->characters[i].key);
    }
  return v;
}

/* Resolve a `.Children(<args>)` / `.Contents(<args>)` reference for one container
   object KEY into its child set, faithfully mirroring the runner's ReplaceOOProperty
   dispatch (Global.vb:826-911).  BOTH the entity type (Objects/Characters/Both)
   AND the where (In/On/OnAndIn) come from the arg, objects are listed before
   characters, and -- crucially -- an unrecognised combination (a bare `On`, or a
   `Characters,On` on a container with no characters) yields the EMPTY set exactly
   as the runner's Select Case falls through.  A `.Children(Characters,In)` must therefore
   return CHARACTERS, not the objects merely inside -- so Halloween's hole (a
   hook Inside it, no characters) fails dk_ListFirstL1's `.Count>0` gate. */
static std::vector<std::string>
oo_children_set (a5_state_t *st, const char *key, const std::string &fn,
                 const std::string &args)
{
  std::string a = lower (args);
  std::vector<std::string> v;
  if (fn == "Contents")
    {
      /* The runner lowercases but does NOT space-strip Contents' arg; always InsideObject. */
      bool objs = (a == "" || a == "all" || a == "objects");
      bool chrs = (a == "" || a == "all" || a == "characters");
      if (objs) for (auto &c : objs_children (st, key, A5_CH_IN)) v.push_back (c);
      if (chrs) for (auto &c : chars_children (st, key, A5_CH_IN)) v.push_back (c);
      return v;
    }
  std::string b;                       /* Children: space-stripped + lowercased */
  for (char c : a) if (c != ' ') b += c;
  int obj_mode = -1, chr_mode = -1;    /* -1 = type not included */
  if (b == "" || b == "all" || b == "onandin" || b == "all,onandin")
    { obj_mode = A5_CH_BOTH; chr_mode = A5_CH_BOTH; }
  else if (b == "characters,in")                       chr_mode = A5_CH_IN;
  else if (b == "characters,on")                       chr_mode = A5_CH_ON;
  else if (b == "characters,onandin" || b == "characters") chr_mode = A5_CH_BOTH;
  else if (b == "in")             { obj_mode = A5_CH_IN; chr_mode = A5_CH_IN; }
  else if (b == "objects,in")                          obj_mode = A5_CH_IN;
  else if (b == "objects,on")                          obj_mode = A5_CH_ON;
  else if (b == "objects,onandin" || b == "objects")   obj_mode = A5_CH_BOTH;
  /* else: no the runner Select Case matches -> empty set. */
  if (obj_mode >= 0) for (auto &c : objs_children (st, key, obj_mode)) v.push_back (c);
  if (chr_mode >= 0) for (auto &c : chars_children (st, key, chr_mode)) v.push_back (c);
  return v;
}

/* Append every recursive descendant (inside + on a surface) of each key, so a
   list "includes sub-objects" (clsObjectList bIncludeSubObjects=True).  Indexed
   iteration: children appended at the end are themselves expanded. */
static void
add_descendants (a5_state_t *st, std::vector<std::string> &keys)
{
  /* An object has a single in/on parent, so no acyclic descendant is reachable
     twice; a repeat means a containment cycle (A in B, B in A, or a self-
     containing object from a malformed game file).  Track the keys already
     seen so a cycle cannot grow `keys` without bound (infinite loop / OOM). */
  std::unordered_set<std::string> seen (keys.begin (), keys.end ());
  for (size_t i = 0; i < keys.size (); i++)
    for (auto &c : objs_children (st, keys[i].c_str (), A5_CH_BOTH))
      if (seen.insert (c).second)
        keys.push_back (c);
}

/* Objects directly in a location, for the OO `.Objects` property.  The
   runner's ReplaceOOProperty "Objects" (Global.vb:913/1504) is
   loc.ObjectsInLocation.Values with DEFAULT arguments -- AllListedObjects,
   directly -- NOT the restriction evaluator's AllObjects: dynamic objects
   unless ExplicitlyExclude, static objects only when ExplicitlyList
   (clsLocation.vb:220-227).  Return to Castle Coris gates "You can also see
   Location66.Objects.DynamicLocation.List under there." on Under Outcrop's
   un-listed statics resolving to an EMPTY list -> "nothing", which its
   YouCanAlso ALR then erases whole. */
static std::vector<std::string>
objs_in_location (a5_state_t *st, const char *lockey)
{
  std::vector<std::string> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    {
      const a5_object_t *o = &st->adv->objects[i];
      int is_static = st->obj[i].is_static;
      int listed = (!is_static
                    && !a5state_entity_has_prop (st, o->key, "ExplicitlyExclude"))
                 || (is_static
                     && a5state_entity_has_prop (st, o->key, "ExplicitlyList"));
      if (listed && a5state_object_at_location (st, i, lockey, 1))
        v.push_back (o->key);
    }
  return v;
}

/* Characters in a location, for the OO `.Characters` property (the runner's
   ReplaceOOProperty "Characters" on a location, loc.CharactersInLocation).  The
   Player counts -- the runner's list is every character whose location resolves
   to this room, and a task Execute'd over it filters with its own restrictions
   (Quest Giver's `view prospects` gates on HaveProperty daz7CharacterI, so the
   Player drops out there).  a5state_character_location_key resolves a character
   standing on furniture to the furniture's room, as clsCharacter.Location does. */
static std::vector<std::string>
chars_in_location (a5_state_t *st, const char *lockey)
{
  std::vector<std::string> v;
  for (int i = 0; i < st->adv->n_characters; i++)
    {
      const char *lk = a5state_character_location_key (st, i);
      if (lk != NULL && streq (lk, lockey))
        v.push_back (st->adv->characters[i].key);
    }
  return v;
}

/* The parent key of an object (clsObject.Parent, clsObject.vb:663): the thing it
   is in/on/held/worn by/part of -- AND, for an object sitting directly in a
   location (or location group), that location key.  st->obj[oi].key holds the
   container key in every placed case (it is the location key for
   A5_OWHERE_LOCATION/LOCGROUP), so all of these return it; only the unplaced
   cases (Hidden/Everywhere/None) have no parent here. */
static const char *
parent_key (a5_state_t *st, const char *key)
{
  int oi = a5state_object_index (st, key);
  if (oi >= 0)
    {
      a5_owhere_t w = st->obj[oi].where;
      if (w == A5_OWHERE_ON_OBJECT || w == A5_OWHERE_IN_OBJECT
          || w == A5_OWHERE_HELD_BY || w == A5_OWHERE_WORN_BY
          || w == A5_OWHERE_PART_OBJECT || w == A5_OWHERE_PART_CHAR
          || w == A5_OWHERE_LOCATION || w == A5_OWHERE_LOCGROUP)
        return st->obj[oi].key;
    }
  return NULL;
}

/* The location key(s) an object is in (its location roots). */
static std::vector<std::string>
location_roots (a5_state_t *st, const char *objkey)
{
  std::vector<std::string> v;
  int oi = a5state_object_index (st, objkey);
  for (int li = 0; oi >= 0 && li < st->adv->n_locations; li++)
    if (a5state_object_at_location (st, oi, st->adv->locations[li].key, 0))
      v.push_back (st->adv->locations[li].key);
  return v;
}

static char
item_kind (a5_state_t *st, const char *key)
{
  if (key == NULL) return 0;
  if (a5model_object (st->adv, key))    return 'o';
  if (a5model_character (st->adv, key)) return 'c';
  if (a5model_location (st->adv, key))  return 'l';
  for (int i = 0; i < st->adv->n_groups; i++)
    if (streq (st->adv->groups[i].key, key)) return 'g';
  for (int i = 0; i < st->adv->n_events; i++)
    if (streq (st->adv->events[i].key, key)) return 'e';   /* Event.Position/.Length */
  return 0;
}

/* ------------------------------------------------------------- name rendering */

/* The display name of a single item key, with an article (definite/indef/none)
   for objects; characters use their plain name. */
static std::string
item_name (a5_state_t *st, const std::string &key, a5_article_t art)
{
  const a5_object_t *o = a5model_object (st->adv, key.c_str ());
  if (o != NULL)
    {
      char *n = a5text_object_name (st, o, art);
      std::string r = n ? n : "";
      free (n);
      return r;
    }
  const a5_character_t *c = a5model_character (st->adv, key.c_str ());
  if (c != NULL)
    {
      char *n = a5text_character_subjective (st, c);
      std::string r = n ? n : key;
      free (n);
      return r;
    }
  const a5_location_t *l = a5model_location (st->adv, key.c_str ());
  if (l != NULL)
    {
      char *n = a5text_location_short_plain (st, key.c_str ());
      std::string r = n ? n : "";
      free (n);
      return r;
    }
  return key;
}

/* Join a key list as a "List"/"Name" string per the args (and/or, article). */
static std::string
render_list (a5_state_t *st, const std::vector<std::string> &keys,
             const std::string &args)
{
  std::string a = lower (args);
  std::string sep = " and ";
  if (contains (a, "or")) sep = " or ";
  int rows = contains (a, "rows");
  a5_article_t art = A5_ART_DEFINITE;
  if (contains (a, "indefinite")) art = A5_ART_INDEFINITE;
  else if (contains (a, "none"))  art = A5_ART_NONE;

  if (keys.empty ())
    return "nothing";

  std::string out;
  int n = (int) keys.size ();
  for (int i = 0; i < n; i++)
    {
      if (rows)
        { if (i > 0) out += "\n"; }
      else
        {
          if (i > 0 && i < n - 1) out += ", ";
          if (n > 1 && i == n - 1) out += sep;
        }
      /* characters default to indefinite unless the args force definite */
      a5_article_t ia = art;
      if (a5model_character (st->adv, keys[i].c_str ()) != NULL)
        {
          ia = A5_ART_INDEFINITE;
          if (contains (a, "definite") && !contains (a, "indefinite"))
            ia = A5_ART_DEFINITE;
        }
      out += item_name (st, keys[i], ia);
    }
  return out;
}

/* ------------------------------------------------------------- the evaluator */

static std::string oo_prop (a5_state_t *st, Ctx ctx, const std::string &sProperty,
                            int depth, int *ok);

/* Pipe-join a key (or direction) list -- the bare-reference terminal: with no
   further step, the expression's value is the key list itself. */
static std::string
join_keys (const std::vector<std::string> &keys)
{
  std::string r;
  for (size_t i = 0; i < keys.size (); i++)
    { if (i) r += "|"; r += keys[i]; }
  return r;
}

/* Parse "Func(args).Rest" -> function / args / remainder, per ReplaceOOProperty. */
static void
split_property (const std::string &p, std::string &fn, std::string &args,
                std::string &rem)
{
  fn = p; args.clear (); rem.clear ();
  size_t dot = p.find ('.');
  size_t ob  = p.find ('(');
  if (ob != std::string::npos && p.find (')') != std::string::npos)
    {
      size_t cb = p.find (')', ob);
      if (dot != std::string::npos)
        {
          if (dot < ob)
            { rem = p.substr (dot + 1); fn = p.substr (0, dot); }
          else
            { args = p.substr (ob + 1, cb - ob - 1);
              rem  = (cb + 2 <= p.size ()) ? p.substr (cb + 2) : std::string ();
              fn   = p.substr (0, ob); }
        }
      else
        { size_t lcb = p.rfind (')');
          args = p.substr (ob + 1, lcb - ob - 1);
          rem  = (cb + 1 <= p.size ()) ? p.substr (cb + 1) : std::string ();
          fn   = p.substr (0, ob); }
    }
  else if (dot != std::string::npos && dot > 0)
    { rem = p.substr (dot + 1); fn = p.substr (0, dot); }
}

/* Terminal: an entity's description text. */
static std::string
item_description (a5_state_t *st, const std::string &key)
{
  /* The runner's Description.ToString returns the still-marked-up composition (tags
     and "<>" segment-join markers intact); the display pipeline strips them
     once, at the end.  Returning plain text here let a later re-embedding cap
     capitalise across a stripped "<>" join that the runner leaves alone (Starship
     Quest's DeadNative lowercase "native's ..." append). */
  const a5_object_t *o = a5model_object (st->adv, key.c_str ());
  if (o != NULL)
    { char *d = a5text_describe_marked (st, a5xml_child (o->node, "Description"));
      std::string r = d ? d : ""; free (d);
      /* clsObject.Description getter (clsObject.vb:448): an object whose
         composed description renders empty reads as "There is nothing special
         about <the object>."  This is the source of the runner's default examine text
         (ExamineObjects' %object%.Description) AND why Global.DisplayObject's
         own "sees nothing interesting" branch is dead code for objects. */
      if (r.empty ())
        { char *dn = a5text_object_name (st, o, A5_ART_DEFINITE);
          r = std::string ("There is nothing special about ")
              + (dn ? dn : key.c_str ()) + ".";
          free (dn); }
      return r; }
  const a5_character_t *c = a5model_character (st->adv, key.c_str ());
  if (c != NULL)
    { /* v5 rewrites char-scoped bare functions to the keyed form at load
         (FileIO.vb:1897: SearchAndReplace "%CharacterName%" ->
         "%CharacterName[Key]%" over each character's texts), so a bare
         %CharacterName% inside a character's own Description names THAT
         character ("Leon is a tall warrior...", Dragon Diamond), not the
         viewpoint Player.  Scarier's equivalent is the transient ctx_char. */
      const char *prev_ctx = st->ctx_char;
      char *d;
      st->ctx_char = c->key;
      d = a5text_describe_marked (st, a5xml_child (c->node, "Description"));
      st->ctx_char = prev_ctx;
      std::string r = d ? d : ""; free (d); return r; }
  const a5_location_t *l = a5model_location (st->adv, key.c_str ());
  if (l != NULL)
    { char *d = a5text_view_location (st); std::string r = d ? d : ""; free (d);
      return r.empty () ? "There is nothing of interest here." : r; }
  return "";
}

/* Build a Ctx (single item or pipe-list) from a resolved first-key string. */
static Ctx
resolve_first (a5_state_t *st, const std::string &firstkeys)
{
  Ctx ctx;
  /* split on '|' */
  size_t start = 0;
  std::vector<std::string> parts;
  while (start <= firstkeys.size ())
    {
      size_t bar = firstkeys.find ('|', start);
      std::string seg = firstkeys.substr (start, bar == std::string::npos
                                          ? std::string::npos : bar - start);
      if (!seg.empty ()) parts.push_back (seg);
      if (bar == std::string::npos) break;
      start = bar + 1;
    }
  if (parts.size () > 1)
    {
      ctx.is_list = 1;
      ctx.keys = parts;
      return ctx;
    }
  if (parts.size () == 1)
    {
      /* An event key (Global.ReplaceOOProperty's `evt` branch) -- carries no
         entity kind; mark the Ctx so oo_prop reads Position/Length via the
         host hook (clsEvent.TimerFromStartOfEvent / Length.Value). */
      for (int i = 0; i < st->adv->n_events; i++)
        if (streq (st->adv->events[i].key, parts[0].c_str ()))
          { ctx.is_event = 1; ctx.keys.push_back (parts[0]); return ctx; }
      char kind = item_kind (st, parts[0].c_str ());
      if (kind == 'g')
        {
          /* Live members (the runner arlMembers) so a group expression reflects runtime
             Add/Remove*ToGroup, not just the static <Member> list. */
          const char *gk = parts[0].c_str ();
          int n = a5state_group_count (st, gk);
          ctx.is_list = 1;
          for (int m = 0; m < n; m++)
            ctx.keys.push_back (a5state_group_member_at (st, gk, m));
          return ctx;
        }
      if (kind != 0)
        { ctx.keys.push_back (parts[0]); ctx.is_list = 0; return ctx; }
      /* Not an item key: a bare DirectionsEnum name resolves to a one-element
         direction list (Global.vb:1608's fallthrough) -- "South.Name" from a
         substituted %direction% renders as the lowercase display name. */
      for (int i = 0; i < 12; i++)
        if (parts[0] == kEnumDirs[i])
          { ctx.is_dirs = 1; ctx.dirs.push_back (parts[0]); return ctx; }
    }
  return ctx;   /* empty: unresolved */
}

/* A SelectionOnly property carries no value, so on a SINGLE item the runner
   answers the marker itself: "1" when the item has it (ReplaceOOProperty,
   Global.vb:1216 for objects, :1397 characters, :1538 locations) and "0" when
   it does not (the shared missing-property default, vb:1225).  Returning ""
   instead disarms Beginner's Cave: `ready` is gated on `%object%.s_Weapon=1`
   and every combat roll reads `.s_Readyweapo.s_Weapon`.  (On a LIST the same
   property is a filter, not a value -- handled in the list branch below.) */
static int
selection_only_prop (const a5_state_t *st, const std::string &key,
                     const std::string &fn, std::string *out)
{
  const a5_propdef_t *pd = a5model_propdef (st->adv, fn.c_str ());
  if (pd == NULL || !streq (pd->type, "SelectionOnly"))
    return 0;
  *out = a5state_entity_has_prop (st, key.c_str (), fn.c_str ()) ? "1" : "0";
  return 1;
}

/* Shared single-item terminal for a property key `fn` (the trailing "else" of
   the object/character/location branches): the SelectionOnly marker, the
   stored value (walked into when it is itself an entity key and steps remain),
   a rich <Description> Text value rendered from its value_node (the
   flashlight's WhenOn; the Danish library's %character%.dk_BestemtKar name
   properties -- "0" broke Halloween's "Dracula ligger inde i kisten."), the
   exists-but-empty "" (clsProperty.Value of a blank Description -- Magnetic
   Moon's blank Lid.ReadText concatenates `Lid.ReadText & %text%` into "mike",
   not "0"), and the shared missing-property default "0" (ReplaceOOProperty,
   Global.vb:1225). */
static std::string
item_prop_terminal (a5_state_t *st, const std::string &key, const std::string &fn,
                    const std::string &rem, const a5_prop_t *props, int n_props,
                    int depth, int *ok)
{
  std::string sel;
  if (selection_only_prop (st, key, fn, &sel))
    return sel;
  const char *v = a5state_entity_prop (st, key.c_str (), fn.c_str ());
  if (v != NULL)
    {
      /* if the value is itself an entity key and there's a remainder, walk it */
      if (!rem.empty () && item_kind (st, v) != 0)
        { Ctx nc; nc.keys.push_back (v); return oo_prop (st, nc, rem, depth + 1, ok); }
      return v;
    }
  const a5_prop_t *pr = a5_prop_find (props, n_props, fn.c_str ());
  if (pr != NULL && pr->value_node != NULL)
    { char *d = a5text_eval_description (st, pr->value_node);
      std::string r = d ? d : ""; free (d); return r; }
  if (pr != NULL)
    return "";
  if (!rem.empty ()) { *ok = 0; return ""; }
  return "0";
}

/*
 * Evaluate the navigation step `sProperty` against `ctx`.  *ok stays 1 unless the
 * whole expression turns out to be unresolvable (the "#*!~#" sentinel).
 */
static std::string
oo_prop (a5_state_t *st, Ctx ctx, const std::string &sProperty, int depth, int *ok)
{
  std::string fn, args, rem;
  if (depth > 24) { *ok = 0; return ""; }
  split_property (sProperty, fn, args, rem);

  /* ---- direction list (a .Exits result) ---- */
  if (ctx.is_dirs)
    {
      if (fn.empty ())
        return join_keys (ctx.dirs);
      if (fn == "Count")
        return std::to_string (ctx.dirs.size ());
      if (fn == "List" || fn == "Name")
        return render_dirs (ctx.dirs, args);
      return "";
    }

  /* ---- event item (Global.ReplaceOOProperty evt branch) ---- */
  if (ctx.is_event)
    {
      if (fn.empty ())
        return ctx.keys.empty () ? "" : ctx.keys[0];
      if ((fn == "Position" || fn == "Length") && !ctx.keys.empty ())
        {
          long v = 0;
          if (a5expr_event_prop_hook
              && a5expr_event_prop_hook (ctx.keys[0].c_str (), fn.c_str (), &v))
            return std::to_string (v);
          *ok = 0;        /* no host hook (a5text_dump): unresolved */
          return "";
        }
      return "";
    }

  /* ---- list path (also single items routed through here when convenient) ---- */
  if (ctx.is_list)
    {
      if (fn.empty ())
        return join_keys (ctx.keys);
      if (fn == "Count")
        return std::to_string (ctx.keys.size ());
      if (fn == "Sum")
        {
          long sum = 0;
          for (auto &k : ctx.keys)
            { const char *v = a5state_entity_prop (st, k.c_str (), ctx.prop_key.c_str ());
              if (v) sum += strtol (v, NULL, 10); }
          return std::to_string (sum);
        }
      if (fn == "Description")
        { std::string r; for (auto &k : ctx.keys)
            { if (!r.empty ()) r += "  "; r += item_description (st, k); } return r; }
      if (fn == "List" || fn == "Name")
        return render_list (st, ctx.keys, args);
      if (fn == "Parent")
        {
          Ctx nc; nc.is_list = 1;
          for (auto &k : ctx.keys)
            { const char *pk = parent_key (st, k.c_str ());
              if (pk) { int dup = 0; for (auto &e : nc.keys) if (e == pk) dup = 1;
                        if (!dup) nc.keys.push_back (pk); } }
          if (!rem.empty ()) return oo_prop (st, nc, rem, depth + 1, ok);
          return "";
        }
      if (fn == "Children" || fn == "Contents")
        {
          Ctx nc; nc.is_list = 1;
          for (auto &k : ctx.keys)
            for (auto &c : oo_children_set (st, k.c_str (), fn, args))
              nc.keys.push_back (c);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Objects")
        {
          Ctx nc; nc.is_list = 1;
          for (auto &k : ctx.keys)
            if (a5model_location (st->adv, k.c_str ()))
              for (auto &o : objs_in_location (st, k.c_str ()))
                nc.keys.push_back (o);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Characters")
        {
          Ctx nc; nc.is_list = 1;
          for (auto &k : ctx.keys)
            if (a5model_location (st->adv, k.c_str ()))
              for (auto &c : chars_in_location (st, k.c_str ()))
                nc.keys.push_back (c);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      /* else: a property key -> filter / aggregate.  Carry it for .Sum. */
      {
        /* A SelectionOnly (valueless marker) property applied to a list is a
           FILTER, not a value: the runner's ReplaceOOProperty (Global.vb:931-1082) keeps
           the members that carry the marker and yields those member KEYS (a
           reference / sub-collection), because a SelectionOnly prop has no value
           to render.  So `%Player%.Location.Objects.ObjectIsAT` -> the cube's
           key (AlienDiver binds this to set the cube's target number / craft /
           extract), and `...ObjectIsAT.Count` counts only the cubes.  Any other
           property type (Integer/ValueList/StateList/Text/entity-key) keeps the
           long-standing carry-for-.Sum / terminal-first-value behaviour. */
        const a5_propdef_t *pd = a5model_propdef (st->adv, fn.c_str ());
        if (pd != NULL && streq (pd->type, "SelectionOnly"))
          {
            /* An argument inverts the filter: `.Isscore(False)` (or `(0)`)
               keeps the members that do NOT carry the marker (ReplaceOOProperty:
               a has-property member is dropped on "false"/"0", Global.vb:971,
               and a lacks-property member kept, vb:1040).  Symphonica 64's
               inventory line is `%Player%.Held(False).Isscore(False).List(..)`
               -- the non-score items. */
            std::string a = lower (args);
            int want = !(a == "false" || a == "0");
            Ctx nc; nc.is_list = 1;
            for (auto &k : ctx.keys)
              if (!!a5state_entity_has_prop (st, k.c_str (), fn.c_str ()) == want)
                nc.keys.push_back (k);
            if (!rem.empty ())
              return oo_prop (st, nc, rem, depth + 1, ok);
            return join_keys (nc.keys);
          }
        Ctx nc; nc.is_list = 1; nc.keys = ctx.keys; nc.prop_key = fn;
        if (!rem.empty ())
          return oo_prop (st, nc, rem, depth + 1, ok);
        /* terminal property on a list: emit the first item's value */
        for (auto &k : ctx.keys)
          { const char *v = a5state_entity_prop (st, k.c_str (), fn.c_str ());
            if (v) return v; }
        return "";
      }
    }

  /* ---- single item ---- */
  if (ctx.keys.empty ()) { *ok = 0; return ""; }
  const std::string key = ctx.keys[0];
  char kind = item_kind (st, key.c_str ());

  if (kind == 'o')
    {
      const a5_object_t *o = a5model_object (st->adv, key.c_str ());
      if (fn == "Adjective" || fn == "Prefix") return o->prefix ? o->prefix : "";
      if (fn == "Article")                     return o->article ? o->article : "";
      if (fn == "Count")                       return "1";
      if (fn == "Noun")                        return o->n_names > 0 ? o->names[0] : "";
      if (fn.empty ())                         return key;
      if (fn == "Description")                 return item_description (st, key);
      if (fn == "Name" || fn == "List")
        {
          a5_article_t art = A5_ART_DEFINITE;
          if (contains (lower (args), "indefinite")) art = A5_ART_INDEFINITE;
          else if (contains (lower (args), "none"))  art = A5_ART_NONE;
          return item_name (st, key, art);
        }
      if (fn == "Parent")
        {
          const char *pk = parent_key (st, key.c_str ());
          Ctx nc; if (pk) nc.keys.push_back (pk);
          if (pk == NULL) { return ""; }
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Location")
        {
          /* The runner's object .Location resolves to a SINGLE clsLocation when the
             object has one root (ReplaceOOProperty collapses a 1-item list),
             which matters for terminal rich-Text properties: the single-item
             branch renders their <Value><Description> node (The Salvage's
             `%Player%.daz7Ship1.Location.daz7StationNam` station name) where
             the list branch has no value_node fallback. */
          Ctx nc; nc.keys = location_roots (st, key.c_str ());
          if (nc.keys.size () != 1) nc.is_list = 1;
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Children" || fn == "Contents")
        {
          Ctx nc; nc.is_list = 1;
          nc.keys = oo_children_set (st, key.c_str (), fn, args);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      /* a property key on the object */
      return item_prop_terminal (st, key, fn, rem, o->props, o->n_props,
                                 depth, ok);
    }

  if (kind == 'c')
    {
      const a5_character_t *c = a5model_character (st->adv, key.c_str ());
      if (fn == "Count")        return "1";
      if (fn.empty ())          return key;
      if (fn == "Description")  return item_description (st, key);
      if (fn == "Name")
        { char *n = a5text_character_oo_name (st, c, args.c_str ());
          std::string r = n ? n : key; free (n); return r; }
      if (fn == "ProperName")
        { /* clsCharacter.ProperName (Global.vb:1334): the CharacterProperName
             override (typed-in player name) else the model Name, "" ->
             "Anonymous". */
          const char *pn = a5state_entity_prop (st, key.c_str (),
                                                "CharacterProperName");
          if (pn == NULL || pn[0] == '\0')
            pn = (c != NULL && c->name != NULL && c->name[0] != '\0')
                   ? c->name : "Anonymous";
          return pn; }
      if (fn == "Descriptor")   return c->n_descriptors > 0 ? c->descriptors[0] : "";
      if (fn == "Exits")
        {
          int ci = a5state_character_index (st, key.c_str ());
          const char *cl = (ci >= 0) ? st->char_loc[ci] : NULL;
          Ctx nc; nc.is_dirs = 1;
          nc.dirs = char_exit_dirs (st, key.c_str (), cl);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Held" || fn == "Worn")
        {
          /* clsCharacter.Held/Worn(Optional bIncludeSubObjects=True): the
             default recurses into the held/worn containers; (False) is the
             direct list only.  The inventory contents segment keys off
             Held.Count > Held(False).Count to detect sub-objects. */
          std::string a = lower (args);
          int include_sub = !(a == "false" || a == "0");
          Ctx nc; nc.is_list = 1;
          nc.keys = objs_carried_by (st, key.c_str (),
                                     fn == "Held" ? A5_OWHERE_HELD_BY
                                                  : A5_OWHERE_WORN_BY);
          if (include_sub)
            add_descendants (st, nc.keys);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "WornAndHeld")
        {
          /* clsCharacter WornAndHeld OO step (Global.vb:1350): worn objects
             followed by held objects, as one list. */
          Ctx nc; nc.is_list = 1;
          nc.keys = objs_carried_by (st, key.c_str (), A5_OWHERE_WORN_BY);
          for (auto &h : objs_carried_by (st, key.c_str (), A5_OWHERE_HELD_BY))
            nc.keys.push_back (h);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Location")
        {
          int ci = a5state_character_index (st, key.c_str ());
          Ctx nc; if (ci >= 0 && st->char_loc[ci]) nc.keys.push_back (st->char_loc[ci]);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Parent")
        {
          /* clsCharacter.Parent == Location.Key (clsCharacter.vb:189): the object
             the character is on/in (char_onobj) when seated/contained, else the
             location.  parent_key only handles objects, so resolve the character
             case here -- e.g. Grandpa's `(getting off %Player%.Parent.Name first)`
             needs the chair the Player stands on. */
          int ci = a5state_character_index (st, key.c_str ());
          const char *pk = (ci >= 0)
            ? (st->char_onobj[ci] ? st->char_onobj[ci] : st->char_loc[ci])
            : NULL;
          if (pk == NULL) return "";
          Ctx nc; nc.keys.push_back (pk);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      return item_prop_terminal (st, key, fn, rem, c->props, c->n_props,
                                 depth, ok);
    }

  if (kind == 'l')
    {
      const a5_location_t *l = a5model_location (st->adv, key.c_str ());
      if (fn.empty ())          return key;
      if (fn == "Name" || fn == "List")
        { char *n = a5text_location_short_plain (st, key.c_str ());
          std::string r = n ? n : ""; free (n); return r; }
      if (fn == "Description")  return item_description (st, key);
      if (fn == "Exits")
        {
          Ctx nc; nc.is_dirs = 1;
          for (int i = 0; i < 12; i++)
            if (a5model_exit_dest (st->adv, key.c_str (), kEnumDirs[i]) != NULL)
              nc.dirs.push_back (kEnumDirs[i]);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Objects")
        {
          Ctx nc; nc.is_list = 1; nc.keys = objs_in_location (st, key.c_str ());
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Characters")
        {
          Ctx nc; nc.is_list = 1; nc.keys = chars_in_location (st, key.c_str ());
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "LocationTo")
        {
          /* Global.ReplaceOOProperty "LocationTo" (Global.vb:1475): the raw
             destination key(s) of the location's exit(s) in the given
             direction(s) -- arlDirections lookup, NO exit-restriction check.
             The Salvage's ship-walk message ends
             `%Player%.Location.LocationTo(%direction%).Name`. */
          Ctx nc;
          std::vector<std::string> dests;
          std::string dl = lower (args), cur;
          for (size_t i = 0; i <= dl.size (); i++)
            {
              if (i < dl.size () && dl[i] != '|') { cur += dl[i]; continue; }
              for (int d = 0; d < 12; d++)
                if (cur == lower (kEnumDirs[d]))
                  {
                    const char *dest = a5model_exit_dest (st->adv, key.c_str (),
                                                         kEnumDirs[d]);
                    if (dest != NULL)
                      dests.push_back (dest);
                    break;
                  }
              cur.clear ();
            }
          nc.keys = dests;
          if (dests.size () != 1)
            nc.is_list = 1;
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      return item_prop_terminal (st, key, fn, rem, l->props, l->n_props,
                                 depth, ok);
    }

  *ok = 0;
  return "";
}

char *
a5expr_eval (a5_state_t *st, const char *firstkeys, const char *chain,
             int force_list)
{
  if (firstkeys == NULL || chain == NULL)
    return NULL;
  Ctx ctx = resolve_first (st, firstkeys);
  if (ctx.keys.empty () && !ctx.is_list && !ctx.is_dirs)
    return NULL;
  /* A plural reference (%objects%/%characters%) is a collection even when it
     bound a single object, so list aggregators (.Sum/.Count/.List) and the
     list .Parent step apply -- e.g. the stock take task's
     `%objects%.Parent.Takefix.Sum=0` over one ground object must sum the
     parent location's (absent) Takefix as 0, not render "". */
  if (force_list && !ctx.keys.empty ())
    ctx.is_list = 1;

  std::string rem = chain;
  while (!rem.empty () && rem[0] == '.') rem.erase (rem.begin ());
  if (rem.empty ())
    return NULL;

  int ok = 1;
  std::string r = oo_prop (st, ctx, rem, 0, &ok);
  if (!ok)
    return NULL;
  return strdup (r.c_str ());
}

/* ------------------------------------------------------- whole-text OO pass */

static int
is_key_char (char c)
{
  /* The runner's key regex uses \w, which is Unicode-aware in .NET -- accept UTF-8
     continuation/lead bytes so keys like "ClaudeMoné" scan whole. */
  return isalnum ((unsigned char) c) || c == '_' || c == '|'
         || (unsigned char) c >= 0x80;
}

/* The byte just past a ".step(args)?..." chain that begins at `dot`. */
const char *
a5expr_scan_chain (const char *dot)
{
  const char *r = dot;
  while (*r == '.')
    {
      const char *seg = r + 1;
      if (!isalpha ((unsigned char) *seg))
        break;
      r = seg;
      while (is_key_char (*r))
        r++;
      if (*r == '(')
        {
          int depth = 1;
          r++;
          while (*r && depth > 0)
            { if (*r == '(') depth++; else if (*r == ')') depth--; r++; }
        }
    }
  return r;
}

static char *
expr_replace_impl (a5_state_t *st, const char *text, int expression)
{
  std::string out;
  const char *p = text ? text : "";
  while (*p)
    {
      /* A key token starts where the runner's regex could start a `firstkey`
         (Global.vb:630): `%?[A-Za-z][\w\|_]*%?`.  That demands a LETTER, not
         merely a word char, so a digit / '_' / '|' to the left is a legal
         start -- .NET tries every offset in turn and the earlier ones simply
         fail to match.  (A *letter* to the left is different: the greedy
         earlier attempt swallows this token, and since we advance p past a
         whole unresolvable key token we reproduce that skip.)  Requiring a
         full non-word char here made TASP's
         `[kristenconversation...=%rotato%%Character1%.Property3]` stall: with
         the rotating variable rendered the text reads "=2Character1.Property3"
         and the digit blocked the OO pass, so the raw key leaked out and no
         TextOverride matched it. */
      int boundary = (p == text) || !isalpha ((unsigned char) p[-1]);
      if (boundary && (isalpha ((unsigned char) *p)))
        {
          const char *k = p;
          while (is_key_char (*k)) k++;
          if (*k == '.')
            {
              std::string firstkeys (p, (size_t) (k - p));
              /* require the first pipe-segment to be a real entity key --
                 or a bare DirectionsEnum name ("South.Name", Global.vb:1608) */
              std::string first = firstkeys.substr (0, firstkeys.find ('|'));
              int resolvable = item_kind (st, first.c_str ()) != 0;
              for (int i = 0; !resolvable && i < 12; i++)
                if (first == kEnumDirs[i])
                  resolvable = 1;
              if (resolvable)
                {
                  const char *chain_end = a5expr_scan_chain (k);
                  if (chain_end > k)        /* at least one valid ".step" */
                    {
                      std::string chain (k, (size_t) (chain_end - k));
                      char *v = a5expr_eval (st, firstkeys.c_str (), chain.c_str ());
                      if (v != NULL)
                        {
                          /* Expression mode (Global.vb:645): a non-integer OO
                             value becomes a quoted string literal -- unless the
                             source already quotes the match ("Lid.ReadText") or
                             we are inside a quoted run.  Magnetic Moon's
                             `SetProperty Lid ReadText Lid.ReadText & %text%`
                             needs `"" & "mike"`, not ` & "mike"`.
                             Known edge case: the strtol test below treats a
                             digits-then-text value ("3 coins") as non-numeric
                             and quotes it, while a pure "3" stays bare, so the
                             two diverge in `&`/`+` contexts.  Left as-is: it
                             matches the runner for the pure-integer and
                             pure-text cases that occur in practice, and no
                             corpus game feeds a partial-numeric OO value here. */
                          int quote = 0;
                          if (expression && (p == text || p[-1] != '"'))
                            {
                              char *endp = NULL;
                              (void) strtol (v, &endp, 10);
                              quote = (v[0] == '\0' || (endp != NULL && *endp != '\0'));
                            }
                          if (quote) out += '"';
                          out += v;
                          if (quote) out += '"';
                          free (v); p = chain_end; continue;
                        }
                    }
                }
            }
          /* not an expression: copy the whole key token verbatim */
          out.append (p, (size_t) (k - p));
          p = k;
          continue;
        }
      out += *p++;
    }
  return strdup (out.c_str ());
}

char *
a5expr_replace (a5_state_t *st, const char *text)
{
  return expr_replace_impl (st, text, 0);
}

char *
a5expr_replace_expr (a5_state_t *st, const char *text)
{
  return expr_replace_impl (st, text, 1);
}
