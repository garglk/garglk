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
 * ADRIFT 5 support for Scarier -- text engine: %function% / %variable%
 * replacement (split from a5text.cpp).  The per-builtin fn_* handlers,
 * eval_function, the ordered model-variable pass, replace_functions and the
 * PopUp* host callbacks.  See a5text.h and a5text_internal.h.
 */

#include <algorithm>

#include "a5text_internal.h"

/* ---------------------------------------------------- function replacement */

/* %PopUpInput% host callback (a5text_set_popup_cb); NULL => use the default. */
static a5_popup_cb a5_popup = nullptr;
static void *a5_popup_ctx = nullptr;
/* %PopUpChoice% host callback (a5text_set_popup_choice_cb); NULL => leave the
   token unevaluated, as FD's throwing MsgBox does. */
static a5_popup_choice_cb a5_popup_choice = nullptr;
static void *a5_popup_choice_ctx = nullptr;

/* Trim surrounding whitespace and one layer of "double quotes" from a PopUp*
   function argument (they arrive already function-expanded). */
static void
popup_unquote (std::string &s)
{
  s = trim_ws (s);
  if (s.size () >= 2 && s.front () == '"' && s.back () == '"')
    s = s.substr (1, s.size () - 2);
}

/* Resolve a function argument (a key or display name) to an object key. */
static const char *
resolve_object_arg (a5_state_t *st, const char *args)
{
  if (args == nullptr || args[0] == '\0')
    return nullptr;
  if (a5model_object (st->adv, args) != nullptr)
    return args;
  for (int i = 0; i < st->adv->n_objects; i++)
    {
      const a5_object_t *o = &st->adv->objects[i];
      for (int j = 0; j < o->n_names; j++)
        {
          if (ci_eq (o->names[j], args)) return o->key;
          if (o->prefix && o->prefix[0])
            { std::string full = std::string (o->prefix) + " " + o->names[j];
              if (ci_eq (full.c_str (), args)) return o->key; }
        }
    }
  return nullptr;
}

/* Join a list of object keys as an "a, b and c" list with the given article
   ("" if empty).  Mirrors ObjectHashTable.List (StronglyTypedCollections.vb:183). */
static char *
list_objects_art (a5_state_t *st, const std::vector<const char *> &keys,
                  a5_article_t art)
{
  std::vector<std::string> names;
  for (size_t i = 0; i < keys.size (); i++)
    {
      const a5_object_t *o = a5model_object (st->adv, keys[i]);
      if (o == nullptr)
        { names.push_back (keys[i]); continue; }
      char *nm = a5text_object_name (st, o, art);
      names.push_back (nm);
      free (nm);
    }
  return strdup (join_and (names).c_str ());
}

/* Join a list of object keys as an indefinite "a, b and c" list ("" if empty). */
static char *
list_objects (a5_state_t *st, const std::vector<const char *> &keys)
{
  return list_objects_art (st, keys, A5_ART_INDEFINITE);
}

/* Object keys whose runtime location is `want` at `key`: on/in a container
   object, or held/worn by a character. */
static std::vector<const char *>
objects_where (a5_state_t *st, a5_owhere_t want, const char *key)
{
  std::vector<const char *> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    if (st->obj[i].where == want && streq (st->obj[i].key, key))
      v.push_back (st->adv->objects[i].key);
  return v;
}

/* Objects placed on (want=A5_OWHERE_ON_OBJECT) or in a container object key. */
static std::vector<const char *>
objects_on_in (a5_state_t *st, const char *objkey, a5_owhere_t want)
{
  return objects_where (st, want, objkey);
}

/* Join keys with '|', the runner's list-of-keys function result shape. */
static std::string
join_keys_pipe (const std::vector<const char *> &keys)
{
  std::string out;
  for (size_t i = 0; i < keys.size (); i++)
    {
      if (i > 0)
        out += '|';
      out += keys[i];
    }
  return out;
}

/* Characters on/in an object key. */
static std::vector<const a5_character_t *>
chars_on_in (a5_state_t *st, const char *objkey, int on)
{
  /* The live on/in state is the char_onobj/char_in arrays (kept by both the
     model load -- "On Object"/"In Object" CharacterLocation -- and the runtime
     ToSittingOn/ToLyingOn/ToStandingOn and inside moves), NOT the property
     override table: Head Case's Mysterious Voice starts CharacterLocation
     "In Object"/CharInsideWhat, which the old prop probe ("Inside Object"/
     "InsideWhat" vocabulary) never matched. */
  std::vector<const a5_character_t *> v;
  for (int i = 0; i < st->adv->n_characters; i++)
    {
      if (st->char_onobj == nullptr || st->char_onobj[i] == nullptr)
        continue;
      if (!streq (st->char_onobj[i], objkey))
        continue;
      int in = st->char_in != nullptr && st->char_in[i];
      if ((on && !in) || (!on && in))
        v.push_back (&st->adv->characters[i]);
    }
  return v;
}

/*
 * The on/in children listing shared by DisplayObjectChildren and the
 * List(bIncludeSubObjects) path: "<on-list> is/are on <the object>[, and
 * inside is/are <in-list>]" — the on-list ToProper-cased, both lists using
 * the objects' own (indefinite) articles, the parent definite, no trailing
 * period.  The inside list is suppressed for a closed openable object.
 * Returns "" when there is nothing to show.
 */
static std::string
object_children_block (a5_state_t *st, const char *objkey)
{
  const a5_object_t *o = a5model_object (st->adv, objkey);
  std::vector<const char *> on = objects_on_in (st, objkey, A5_OWHERE_ON_OBJECT);
  std::vector<const char *> in = objects_on_in (st, objkey, A5_OWHERE_IN_OBJECT);

  /* Inside children are only listed when the object is open (or not openable). */
  int openable = o && a5_prop_find (o->props, o->n_props, "Openable") != nullptr;
  if (openable)
    {
      const char *os = a5state_entity_prop (st, objkey, "OpenStatus");
      if (os == nullptr || !streq (os, "Open"))
        in.clear ();
    }

  std::string s;
  char *defn = o ? a5text_object_name (st, o, A5_ART_DEFINITE) : strdup (objkey);
  if (!on.empty ())
    {
      char *lst = list_objects (st, on);
      s += to_proper (lst);
      free (lst);
      s += (on.size () == 1) ? " is on " : " are on ";
      s += defn;
    }
  if (!in.empty ())
    {
      if (!on.empty ())
        s += ", and inside";
      else
        { s += "Inside "; s += defn; }
      s += (in.size () == 1) ? " is " : " are ";
      char *lst = list_objects (st, in);
      s += lst;
      free (lst);
    }
  free (defn);
  return s;
}

/*
 * Port of clsObject.DisplayObjectChildren: the children block above with its
 * closing period, or the canned "Nothing is on or inside ..." line (only
 * surfaced by ListObjectsOnAndIn when the resolved set is a single childless
 * object).
 */
static std::string
display_object_children (a5_state_t *st, const char *objkey)
{
  std::string s = object_children_block (st, objkey);
  if (!s.empty ())
    return s + ".";
  const a5_object_t *o = a5model_object (st->adv, objkey);
  char *defn = o ? a5text_object_name (st, o, A5_ART_DEFINITE) : strdup (objkey);
  s = "Nothing is on or inside ";
  s += defn;
  s += ".";
  free (defn);
  return s;
}

/*
 * Port of ObjectHashTable.List(sep, bIncludeSubObjects:=True, Indefinite)
 * (StronglyTypedCollections.vb:183) -- the indefinite main list followed, for
 * each object with on/inside children, by its DisplayObjectChildren-style block
 * joined with ".  " and *no* trailing period.  Used by %ListWorn% / %ListHeld%
 * (which both pass bIncludeSubObjects=True), so e.g. examining yourself shows
 * "... a scabbard and your clothes.  Inside the backpack are three food rations.
 * Inside the scabbard is a long sword".
 */
static char *
list_objects_subobj (a5_state_t *st, const std::vector<const char *> &keys)
{
  char *main = list_objects (st, keys);
  std::string out = main ? main : "";
  free (main);
  for (const char *ok : keys)
    {
      std::string blk = object_children_block (st, ok);
      if (blk.empty ())
        continue;
      if (!out.empty ())
        out += ".  ";
      out += blk;
    }
  return strdup (out.c_str ());
}

/*
 * Resolve a ListObjectsOnAndIn argument to the set of object keys it names.
 * The argument is the Adrift 5 runner's ReplaceFunctions-processed value: a single key,
 * a "key1|key2" pipe list, or an OO expression like "Player.WornAndHeld" (which
 * a5expr resolves to a pipe list).  Mirrors Global.vb:2055-2069 (split on '|',
 * trim a trailing ",..." per key, keep only real object keys).
 */
static std::vector<std::string>
arg_object_keys (a5_state_t *st, const char *args)
{
  std::vector<std::string> keys;
  if (args == nullptr)
    return keys;
  std::string a = args;
  size_t dot = a.find ('.');
  if (dot != std::string::npos && dot > 0)
    {
      std::string first = a.substr (0, dot);
      std::string chain = a.substr (dot);
      char *r = a5expr_eval (st, first.c_str (), chain.c_str ());
      a = r ? r : "";
      free (r);
    }
  size_t start = 0;
  while (start <= a.size ())
    {
      size_t bar = a.find ('|', start);
      std::string seg = a.substr (start, bar == std::string::npos
                                  ? std::string::npos : bar - start);
      size_t comma = seg.find (',');
      if (comma != std::string::npos)
        seg = seg.substr (0, comma);
      if (!seg.empty () && a5model_object (st->adv, seg.c_str ()) != nullptr)
        keys.push_back (seg);
      if (bar == std::string::npos)
        break;
      start = bar + 1;
    }
  return keys;
}

/* ------------------------------------------ eval_function per-builtin handlers */

static char *
fn_player (a5_state_t *st, const char * /*name*/, const char * /*args*/)
{
  const a5_character_t *p = a5model_character (st->adv, a5state_player_key (st));
  return character_name (st, p, A5_PRO_SUBJ);
}

static char *
fn_popupinput (a5_state_t * /*st*/, const char * /*name*/, const char *args)
{
  /* clsFunction PopUpInput[prompt, default] -> VB InputBox (Global.vb:2296).
     the runner splits sArgs on the single comma, evaluates arg[1] as the default,
     and returns InputBox(prompt, default) wrapped in quotes.  Headless we
     defer to an installed input callback (the host feeds the next script
     line); with none, we evaluate to the default -- matching the Adrift 5 runner's
     InputBox returning its default when unattended.  args are already
     function-expanded; each part may carry surrounding "double quotes". */
  std::string a = args ? args : "";
  std::string prompt = a, dflt;
  size_t comma = a.find (',');
  if (comma != std::string::npos)
    { prompt = a.substr (0, comma); dflt = a.substr (comma + 1); }
  popup_unquote (prompt); popup_unquote (dflt);
  if (a5_popup != nullptr)
    {
      char *ans = a5_popup (a5_popup_ctx, prompt.c_str (), dflt.c_str ());
      if (ans != nullptr) return ans;
    }
  return strdup (dflt.c_str ());
}

static char *
fn_charactername (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* args may be "<key>" or "<key>, <pronoun>"; default key = Player. */
  std::string a = args ? args : "";
  std::string key = a, pron;
  size_t comma = a.find (',');
  if (comma != std::string::npos)
    { key = a.substr (0, comma); pron = a.substr (comma + 1); }
  key = trim_ws (key);
  pron = trim_ws (pron);
  /* A bare %CharacterName% (no key) resolves to the character whose text is
     being rendered (CharHereDesc / topic reply), else the Player -- v5
     rewrites char-scoped %CharacterName% to %CharacterName[Key]% at load. */
  if (key.empty ()) key = st->ctx_char ? st->ctx_char : a5state_player_key (st);
  const a5_character_t *ch = a5model_character (st->adv, key.c_str ());
  a5_pronoun_t pr = A5_PRO_SUBJ;
  int pron_none = 0;
  std::string pl = pron;
  for (char &c : pl) c = (char) tolower ((unsigned char) c);
  if (pl.find ("object") != std::string::npos || pl.find ("target") != std::string::npos)
    pr = A5_PRO_OBJ;
  else if (pl.find ("possess") != std::string::npos) pr = A5_PRO_POSS;
  else if (pl.find ("reflect") != std::string::npos) pr = A5_PRO_REFL;
  else if (pl.find ("none") != std::string::npos)
    /* PronounEnum.None: never pronoun-replace (clsCharacter.vb:358). */
    pron_none = 1;
  /* A resolved key records a PronounKeys entry (Global.vb:2108, gating on
     htblCharacters.ContainsKey) -- the emit site in replace_functions
     captures the pending (perspective, key, pronoun) with the name's
     output offset. */
  if (ch != nullptr)
    {
      st->pron_pending = char_perspective (st, ch);
      st->pron_pending_key = ch->key;
      st->pron_pending_pron = pron_none ? -1 : (int) pr;
      /* The runner's "slight fudge" (Global.vb:2102-2107): a resolved %CharacterName%
         whose two immediately-preceding chars are "  " or CRLF is PCase'd at
         the emit site (the name starts a sentence -- e.g. the ExamineCharacter
         message "%DisplayCharacter%  %CharacterName% ... carrying ..." where
         the second-person name "you" must render "You"). */
      st->name_cap_eligible = 1;
    }
  else
    ch = a5model_character (st->adv, a5state_player_key (st));
  return character_name (st, ch, pr, /*consult_ledger=*/!pron_none);
}

static char *
fn_characterproper (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* %CharacterProper[Key]% and its alias %ProperName[Key]% render
     htblCharacters(Key).ProperName (Global.vb:2115).  Unlike %CharacterName%
     this never consults the descriptors, the Known property or the pronoun
     ledger -- it is the bare name, so it takes no pronoun argument and leaves
     name_cap_eligible clear. */
  std::string key = trim_ws (args ? args : "");
  if (key.empty ())
    key = st->ctx_char ? st->ctx_char : a5state_player_key (st);
  const a5_character_t *c = a5model_character (st->adv, key.c_str ());
  /* The runner reports an unknown key with DisplayError; we have no such
     channel, so an unresolved character takes the same "Anonymous" path
     a5expr's .ProperName does. */
  return strdup (char_proper_name_raw (st, c));
}

static char *
fn_turns (a5_state_t *st, const char * /*name*/, const char * /*args*/)
{
  /* Global.vb:1763 substitutes %turns% with Adventure.Turns.ToString via
     ReplaceIgnoreCase, so %Turns%/%TURNS% resolve the same (ci_eq folds
     case).  The runner's Runner bumps Adventure.Turns *after* Process() returns, so
     the value visible inside a command's own task output is the pre-command
     count.  Scarier bumps st->turns at the top of a5run_input (a5run.cpp),
     so the matching value is st->turns - 1 (the same offset emit_endgame's
     score line uses); clamp to 0 for text rendered before any turn. */
  char buf[32];
  snprintf (buf, sizeof buf, "%d", st->turns > 0 ? st->turns - 1 : 0);
  return strdup (buf);
}

static char *
fn_alonewithchar (a5_state_t *st, const char * /*name*/, const char * /*args*/)
{
  /* The single other character in the player's location, else NoCharacter
     (clsCharacter.AloneWithChar / Global.vb:1768).  Resolved-location
     compare, so a character seated on furniture counts (Lost Children's
     Anne in her rocking chair). */
  const char *ploc = a5state_player_location (st);
  const char *found = nullptr;
  int count = 0, i;
  for (i = 0; ploc != nullptr && i < st->adv->n_characters; i++)
    {
      const char *oloc;
      if (ci_eq (st->adv->characters[i].key, a5state_player_key (st)))
        continue;
      oloc = a5state_character_location_key (st, i);
      if (oloc != nullptr && streq (oloc, ploc))
        { count++; found = st->adv->characters[i].key; }
    }
  return strdup ((count == 1) ? found : "NoCharacter");
}

static char *
fn_locationname (a5_state_t *st, const char * /*name*/, const char *args)
{
  const char *key = (args && args[0]) ? args : a5state_player_location (st);
  if (key != nullptr && a5model_location (st->adv, key) != nullptr)
    return a5text_location_short (st, key);
  return strdup ("");
}

static char *
fn_locationof (a5_state_t *st, const char * /*name*/, const char *args)
{
  int i;
  /* Global.vb:2237: the location KEY of a character (its LocationKey), or an
     object's location root(s) joined by '|'.  Used by Jacaranda's champagne
     teleport: %DisplayLocation[%LocationOf[%Player%]%]%. */
  const char *key = args ? args : "";
  int ci = a5state_character_index (st, key);
  if (ci >= 0)
    {
      /* clsCharacter.LocationKey (clsCharacter.vb:1790): the effective room,
         resolved through on/in-object and on-character carriers; a Hidden
         character reports the literal "Hidden" (Global.vb:25). */
      const char *k = a5state_character_location_key (st, ci);
      return strdup (k != nullptr ? k : "Hidden");
    }
  {
    int oi = a5state_object_index (st, key);
    if (oi >= 0)
      {
        /* clsObject.LocationRoots (clsObject.vb:460) joined by '|': roots
           resolve through container/part-of chains and through an At-Location
           carrier, unlike the previous directly-placed-only test which
           rendered held/contained/part-of objects as empty. */
        sb_t sb; sb_init (&sb);
        for (i = 0; i < st->adv->n_locations; i++)
          if (a5state_object_location_root (st, oi, st->adv->locations[i].key))
            { if (sb.len) sb_putc (&sb, '|');
              sb_puts (&sb, st->adv->locations[i].key); }
        return sb_finish (&sb);
      }
  }
  return strdup ("");
}

static char *
fn_displaylocation (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* Global.vb:2130: render the given location's ViewLocation (or "There is
     nothing of interest here." when empty). */
  const a5_location_t *l = (args && args[0]) ? a5model_location (st->adv, args) : nullptr;
  if (l != nullptr)
    {
      char *s = view_location_impl (st, args);
      if (s == nullptr || s[0] == '\0')
        { free (s); return strdup ("There is nothing of interest here."); }
      /* The view is already fully adjudicated (its own ALR + cap ran on the
         marked-up buffer, mirroring the runner's single Display pass), and it is
         PLAIN -- its <br>s are now bare '\n's.  The enclosing fragment's
         auto-capitalise must not treat those as line starts: in the runner the
         substituted sView still carries its <br> tags, which sit between
         the newline-to-be and the letter and block the cap regex (the
         virtual human's lowercase "the virtual human is here." literal
         must NOT re-cap into the "The virtual human is here." ALR-killed
         form).  Leave the stripped-tag sentinel after each newline, exactly
         like a5text_render_plain does for every other dropped tag. */
      {
        sb_t mb; sb_init (&mb);
        for (const char *p = s; *p != '\0'; p++)
          {
            sb_putc (&mb, *p);
            if (*p == '\n')
              sb_putc (&mb, A5_ALR_MARK);
          }
        free (s);
        s = sb_finish (&mb);
      }
      return s;
    }
  return strdup ("");
}

static char *
fn_object_names (a5_state_t *st, const char *name, const char *args)
{
  /* The %TheObject[key]% / %ObjectName[key]% function form (args present);
     bare %object%/%objects% (no args) fall through to the bound-reference
     block below. args is a key, or a resolved object name. */
  const a5_object_t *o = a5model_object (st->adv, args);
  a5_article_t art = (ci_eq (name, "theobject") || ci_eq (name, "theobjects"))
                       ? A5_ART_DEFINITE
                   : (ci_eq (name, "aobject") || ci_eq (name, "aobjects")
                      /* The runner: Case "ObjectName" -> htblObjects.List(,,
                         ArticleTypeEnum.Indefinite) (Global.vb:2252) --
                         "You unfold a wet face towel.", not "...wet face
                         towel." (Head Case %ObjectName[%object%]%). */
                      || ci_eq (name, "objectname"))
                       ? A5_ART_INDEFINITE
                       : A5_ART_NONE;
  /* A piped multi-object arg (%TheObjects[%objects%]% with a "k1|k2|..."
     binding from resolve_plural) renders as the runner's "the a, the b and the c"
     article list -- ReplaceFunctions builds a temp ObjectHashTable from the
     split keys and returns htblObjects.List (Global.vb:2056/2386). */
  if (o == nullptr && strchr (args, '|') != nullptr)
    {
      std::vector<std::string> ks = arg_object_keys (st, args);
      if (ks.size () > 1)
        {
          std::vector<const char *> kp;
          for (auto &s : ks) kp.push_back (s.c_str ());
          return list_objects_art (st, kp, art);
        }
    }
  if (o == nullptr && args != nullptr && args[0] != '\0')
    {
      /* args may be a key or a display name; resolve_object_arg also
         matches a "prefix + name" full name (e.g. "framed newspaper
         article"), which a bare %object% inner render produces -- the
         plain names[] search missed those and dropped the article. */
      const char *k = resolve_object_arg (st, args);
      if (k != nullptr)
        o = a5model_object (st->adv, k);
    }
  if (o != nullptr)
    return a5text_object_name (st, o, art);
  /* No object resolved.  The Adrift 5 runner builds an ObjectHashTable from the key
     arg and renders htblObjects.List, which returns "nothing" for an empty
     set (Global.vb:804) -- so %TheObject[]% / %ObjectName[]% with an empty
     (or whitespace-only) argument is "nothing", not a blank.  A non-empty but
     unresolved arg is left as-is (it may be an already-rendered name). */
  if (trim_ws (args ? args : "").empty ())
    return strdup ("nothing");
  return strdup (args ? args : "");
}

static char *
fn_propertyvalue (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* %PropertyValue[entity, propkey]% -> the entity's value for that property.
     (Global.vb:2322 literally loops every object/character/location, but in
     practice the call is always made for the single bound entity whose
     property is in context -- the observed Runner output is just
     that one entity's value, so resolve arg0 to a key and read it.)  A Text
     property stores its value as a rich <Description> block (value_node), so
     render that; otherwise return the scalar. */
  std::string a = args;
  size_t comma = a.find (',');
  if (comma == std::string::npos)
    return strdup ("");
  std::string ent = a.substr (0, comma), prop = a.substr (comma + 1);
  auto strip = [](std::string &s){
    s.erase (std::remove (s.begin (), s.end (), ' '), s.end ()); };
  strip (prop);
  /* arg0 is primarily a KEY: the Runner builds the singleton lookup table by
     probing Adventure.htblObjects, htblCharacters *and* htblLocations with it
     (Global.vb:2052-2070) before the PropertyValue arm walks those tables, so
     character and location keys resolve exactly like object keys.  Only when
     none of the three matches do we fall back to resolve_object_arg's object
     display-name lookup (arg0 often arrives pre-expanded from %object1%).
     Missing this made every `%PropertyValue[Character1,Prop]%` render blank --
     TASP keys its whole TextOverride prose off character-state properties,
     so the text collapsed to the raw `[kristenstart=0]` lookup tokens and
     `SetProperty ... %PropertyValue%+2` arithmetic silently stopped
     counting. */
  const char *ekey = nullptr;
  {
    const a5_object_t *eo = a5model_object (st->adv, ent.c_str ());
    const a5_character_t *ec = eo ? nullptr : a5model_character (st->adv, ent.c_str ());
    const a5_location_t *el = (eo || ec) ? nullptr
                                         : a5model_location (st->adv, ent.c_str ());
    if (eo != nullptr)      ekey = eo->key;
    else if (ec != nullptr) ekey = ec->key;
    else if (el != nullptr) ekey = el->key;
    else                 ekey = resolve_object_arg (st, ent.c_str ());
  }
  /* A runtime SetProperty override wins over the static model value, just
     as a5state_entity_prop layers it (e.g. Amazon's CarriersFl1 runs
     `SetProperty Door1 LockKey Key3`, retargeting the lazy-unlock chain's
     `%PropertyValue[Door1,LockKey]%` from the lost silver key to the iron
     key).  The static value_node path below still serves text properties. */
  if (ekey != nullptr)
    for (int oi = 0; oi < st->n_ov; oi++)
      if (streq (st->ov[oi].entity, ekey)
          && streq (st->ov[oi].prop, prop.c_str ()))
        return strdup (st->ov[oi].value ? st->ov[oi].value : "");
  const a5_prop_t *props = nullptr; int n_props = 0;
  const a5_object_t *o = ekey ? a5model_object (st->adv, ekey) : nullptr;
  if (o != nullptr) { props = o->props; n_props = o->n_props; }
  else {
    const a5_character_t *c = ekey ? a5model_character (st->adv, ekey) : nullptr;
    if (c != nullptr) { props = c->props; n_props = c->n_props; }
    else {
      const a5_location_t *l = ekey ? a5model_location (st->adv, ekey) : nullptr;
      if (l != nullptr) { props = l->props; n_props = l->n_props; }
    }
  }
  const a5_prop_t *pr = props ? a5_prop_find (props, n_props, prop.c_str ()) : nullptr;
  if (pr == nullptr)
    return strdup ("");
  if (pr->value_node != nullptr)
    return a5text_eval_description (st, pr->value_node);
  return strdup (pr->value ? pr->value : "");
}

static char *
fn_parentof (a5_state_t *st, const char * /*name*/, const char *args)
{
  int i;
  /* args is a key or a display name (including the "<prefix> <noun>" form
     %objects% yields); emit the parent's key. */
  const char *ok = resolve_object_arg (st, args);
  const a5_object_t *o = ok ? a5model_object (st->adv, ok) : nullptr;
  const char *k = nullptr;
  if (o != nullptr)
    { char *pp = a5expr_eval (st, o->key, ".Parent"); if (pp) return pp; k = ""; }
  /* Characters too: %ParentOf[%Player%]% is the seat/container the
     character is on/in (clsCharacter.Parent) -- the stock
     MoveOffCurrentObject passes it to MoveOffObject (Magnetic Moon's
     `stand up` from the nav-console chair). */
  {
    const a5_character_t *c = a5model_character (st->adv, args);
    if (c == nullptr)
      for (i = 0; i < st->adv->n_characters; i++)
        if (st->adv->characters[i].name != nullptr
            && ci_eq (st->adv->characters[i].name, args))
          { c = &st->adv->characters[i]; break; }
    if (c != nullptr)
      { char *pp = a5expr_eval (st, c->key, ".Parent"); if (pp) return pp; }
  }
  return strdup (k ? k : "");
}

static char *
fn_list_objects_on_in (a5_state_t *st, const char *name, const char *args)
{
  /* Bare indefinite-article list of the children on / in the object,
     mirroring Children(On|Inside).List(, , Indefinite). */
  const char *k = resolve_object_arg (st, args);
  a5_owhere_t want = ci_eq (name, "listobjectson") ? A5_OWHERE_ON_OBJECT
                                                   : A5_OWHERE_IN_OBJECT;
  std::vector<const char *> v;
  if (k != nullptr) v = objects_on_in (st, k, want);
  if (v.empty ())
    return strdup ("nothing");      /* ObjectHashTable.List of an empty set
                                       (StronglyTypedCollections.vb:196) --
                                       Magnetic Moon's "On the desk are
                                       nothing." */
  return list_objects (st, v);
}

static char *
fn_list_objects_on_and_in (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* Global.vb:2213 ListObjectsOnAndIn: iterate the *argument's* object set
     (e.g. %Player%.WornAndHeld for inventory), concatenating
     DisplayObjectChildren (joined by pSpace's two spaces) for each that has
     on/in children -- or, when the set is a single object, unconditionally
     (so its "Nothing is on or inside ..." surfaces). */
  std::vector<std::string> objs = arg_object_keys (st, args);
  std::string out;
  for (const std::string &ok : objs)
    {
      int has_on = !objects_on_in (st, ok.c_str (), A5_OWHERE_ON_OBJECT).empty ();
      int has_in = !objects_on_in (st, ok.c_str (), A5_OWHERE_IN_OBJECT).empty ();
      if (objs.size () != 1 && !has_on && !has_in)
        continue;
      std::string d = display_object_children (st, ok.c_str ());
      if (d.empty ())
        continue;
      if (!out.empty ())
        out += "  ";
      out += d;
    }
  return strdup (out.c_str ());
}

static char *
fn_list_characters (a5_state_t *st, const char *name, const char *args)
{
  const char *k = resolve_object_arg (st, args);
  const a5_object_t *o = k ? a5model_object (st->adv, k) : nullptr;
  std::vector<const a5_character_t *> on, in;
  if (k != nullptr && !ci_eq (name, "listcharactersin")) on = chars_on_in (st, k, 1);
  if (k != nullptr && !ci_eq (name, "listcharacterson")) in = chars_on_in (st, k, 0);
  auto names = [&](std::vector<const a5_character_t *> &cs) {
    std::vector<std::string> v;
    for (size_t i = 0; i < cs.size (); i++)
      { char *nm = a5text_character_subjective (st, cs[i]);
        v.push_back (nm); free (nm); }
    return join_and (v); };
  if (!ci_eq (name, "listcharactersonandin"))
    {
      /* ListCharactersOn/In (Global.vb:2159/2166) are the bare
         CharacterHashTable.List("and") name join -- no "is on the table"
         here-form, no open-container gate (ChildrenCharacters,
         clsObject.vb:940, never looks at OpenStatus), and "noone" when the
         set is empty (StronglyTypedCollections.vb:425).  An unknown object
         key just DisplayErrors and renders blank. */
      if (k == nullptr)
        return strdup ("");
      std::vector<const a5_character_t *> &cs =
          ci_eq (name, "listcharacterson") ? on : in;
      if (cs.empty ())
        return strdup ("noone");
      return strdup (names (cs).c_str ());
    }
  /* OnAndIn -> DisplayCharacterChildren, which lists inside-characters only
     when the object is `Not Openable OrElse IsOpen` (clsObject.vb:383) --
     same gate as the object children above. */
  if (!in.empty () && o != nullptr
      && a5_prop_find (o->props, o->n_props, "Openable") != nullptr)
    {
      const char *os = a5state_entity_prop (st, k, "OpenStatus");
      if (os == nullptr || !streq (os, "Open"))
        in.clear ();
    }
  if (on.empty () && in.empty ())
    return strdup ("");
  /* DisplayCharacterChildren: "X is on/inside the <object>." */
  std::string out;
  char *defn = o ? a5text_object_name (st, o, A5_ART_DEFINITE) : strdup (k ? k : "");
  if (!on.empty ())
    { out += names (on); out += (on.size () == 1) ? " is on " : " are on ";
      out += defn; }
  if (!in.empty ())
    { if (!on.empty ()) out += ", and " + names (in);
      else out += names (in);
      out += (in.size () == 1) ? " is " : " are ";
      out += "inside "; out += defn; }
  out += ".";
  free (defn);
  return strdup (out.c_str ());
}

static char *
fn_list_held_worn (a5_state_t *st, const char *name, const char *args)
{
  const a5_character_t *c = a5model_character (st->adv, args);
  std::vector<const char *> v;
  if (c != nullptr)
    {
      a5_owhere_t want = ci_eq (name, "listheld") ? A5_OWHERE_HELD_BY
                                                  : A5_OWHERE_WORN_BY;
      v = objects_where (st, want, c->key);
    }
  if (v.empty ()) return strdup ("nothing");
  /* %ListWorn% / %ListHeld% pass bIncludeSubObjects=True (Global.vb:2182/
     2225): append each object's on/inside-contents listing. */
  return list_objects_subobj (st, v);
}

static char *
fn_display_char_obj (a5_state_t *st, const char *name, const char *args)
{
  /* Global.vb:2122/2138 DisplayCharacter/DisplayObject: the entity's
     Description.ToString, with the canned "sees nothing interesting about"
     fallback when empty.  a5expr's .Description already renders the node. */
  const char *key = nullptr;
  int is_char = ci_eq (name, "displaycharacter");
  if (is_char)
    { const a5_character_t *c = a5model_character (st->adv, args);
      key = c ? c->key : nullptr; }
  else
    key = resolve_object_arg (st, args);
  if (key != nullptr)
    {
      char *d = a5expr_eval (st, key, ".Description");
      if (d != nullptr && d[0] != '\0')
        return d;
      free (d);
      std::string fb = "%CharacterName% see[//s] nothing interesting about ";
      if (is_char)
        {
          /* The runner's clsCharacter.Name upgrades Objective->Reflective when the
             same character key was already rendered Subjective earlier this
             turn (PronounKeys).  The leading bare %CharacterName% renders the
             viewpoint character subjectively ("You"), so when the examined
             character IS that viewpoint (examine yourself) the target form
             becomes reflexive -- "yourself", not "you".  Emit the reflective
             qualifier in exactly that case; every other target stays Objective
             (Scarier has no turn-wide PronounKeys, but this is the only
             subject==object collision the fallback can produce). */
          const char *subj = st->ctx_char ? st->ctx_char
                                          : a5state_player_key (st);
          const char *qual = (subj != nullptr && streq (subj, key))
                               ? "reflective" : "target";
          fb += "%CharacterName["; fb += key; fb += ", "; fb += qual;
          fb += "]%.";
        }
      else
        { const a5_object_t *o = a5model_object (st->adv, key);
          char *dn = o ? a5text_object_name (st, o, A5_ART_DEFINITE) : strdup (key);
          fb += dn; fb += "."; free (dn); }
      char *proc = a5text_process (st, fb.c_str ());
      char *plain = a5text_render_plain (proc);
      free (proc);
      return plain;
    }
  return nullptr;
}

static char *
fn_list_exits (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* clsCharacter.ListExits: the comma/"and"-joined, lower-cased list of
     directions the character has a (restriction-checked) route in, or
     "nowhere" when there are none.  Reuse a5expr's character .Exits path
     (same DirectionsEnum order + HasRouteInDirection check). */
  char *cnt = a5expr_eval (st, args, ".Exits.Count");
  long n = cnt ? strtol (cnt, nullptr, 10) : 0;
  free (cnt);
  if (n <= 0)
    return strdup ("nowhere");
  char *lst = a5expr_eval (st, args, ".Exits.List");
  return lst ? lst : strdup ("nowhere");
}

static char *
fn_list_objects_at_location (a5_state_t *st, const char * /*name*/, const char *args)
{
  std::vector<const char *> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    if (a5state_object_at_location (st, i, args, 1))
      v.push_back (st->adv->objects[i].key);
  return list_objects (st, v);
}

/* GroupToWords (Global.vb:2513): one three-digit group, "one hundred and
   twenty three" style.  0 renders "". */
static std::string
number_group_to_words (int num)
{
  static const char *one_to_nineteen[] = { "zero", "one", "two", "three",
    "four", "five", "six", "seven", "eight", "nine", "ten", "eleven",
    "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
    "eighteen", "nineteen" };
  static const char *multiples_of_ten[] = { "twenty", "thirty", "forty",
    "fifty", "sixty", "seventy", "eighty", "ninety" };

  if (num == 0)
    return "";
  std::string result;
  if (num > 99)
    {
      result = std::string (one_to_nineteen[num / 100]) + " hundred";
      num %= 100;
      if (num == 0)
        return result;
      result += " and";
    }
  if (num < 20)
    result += std::string (" ") + one_to_nineteen[num];
  else
    {
      result += std::string (" ") + multiples_of_ten[num / 10 - 2];
      if (num % 10 > 0)
        result += std::string (" ") + one_to_nineteen[num % 10];
    }
  while (!result.empty () && result.front () == ' ')
    result.erase (0, 1);
  return result;
}

static char *
fn_number_as_text (a5_state_t * /*st*/, const char * /*name*/, const char *args)
{
  /* %NumberAsText[n]% (Global.vb:2249 -> NumberToString, vb:2563): spell the
     number in words, groups-of-three with US group names ("one thousand,
     two hundred and thirty four").  A non-numeric argument is evaluated as
     an expression first.  Faithful quirks: the decimal branch drops the
     digit BEFORE the point too (Substring(0, pos - 1): "12.9" -> "1" ->
     "one"), and negatives render "zero" (the loop only emits for positive
     group values). */
  static const char *groups[] = { "", "thousand", "million", "billion",
    "trillion", "quadrillion", "quintillion", "sextillion", "septillion",
    "octillion", "nonillion", "decillion", "undecillion", "duodecillion",
    "tredecillion", "quattuordecillion", "quindecillion", "sexdecillion",
    "septendecillion", "octodecillion", "novemdecillion", "vigintillion" };
  const int n_groups_names = (int) (sizeof groups / sizeof groups[0]);

  std::string num = args != nullptr ? args : "";
  /* IsNumeric gate: digits with optional sign/currency/comma/point dressing.
     Anything else goes through the expression engine and its IntValue. */
  int numeric = 0;
  {
    int digits = 0, other = 0;
    for (char c : num)
      {
        if (c >= '0' && c <= '9') digits++;
        else if (c != '$' && c != ',' && c != '.' && c != '+' && c != '-'
                 && c != ' ')
          other++;
      }
    numeric = digits > 0 && other == 0;
  }
  if (!numeric)
    {
      char *ev = a5_eval_sexpr (num.c_str ());
      if (ev != nullptr)
        {
          long iv = strtol (ev, nullptr, 10);
          char buf[32];
          snprintf (buf, sizeof buf, "%ld", iv);
          num = buf;
          free (ev);
        }
    }

  /* Clean: strip "$" and ",", leading zeros, then the decimal tail. */
  std::string cleaned;
  for (char c : num)
    if (c != '$' && c != ',')
      cleaned += c;
  size_t nz = cleaned.find_first_not_of ('0');
  cleaned = (nz == std::string::npos) ? "" : cleaned.substr (nz);
  size_t pos = cleaned.find ('.');
  if (pos == 0)
    return strdup ("zero");
  if (pos != std::string::npos)
    cleaned = cleaned.substr (0, pos - 1);

  std::string result;
  int num_groups = (int) (cleaned.size () + 2) / 3;
  cleaned.insert (0, (size_t) num_groups * 3 - cleaned.size (), ' ');
  for (int group_num = num_groups - 1; group_num >= 0; group_num--)
    {
      int group_value = atoi (cleaned.substr (0, 3).c_str ());
      cleaned = cleaned.substr (3);
      if (group_value > 0)
        result += number_group_to_words (group_value) + " "
                  + (group_num >= n_groups_names ? "?" : groups[group_num])
                  + ", ";
    }
  if (result.size () >= 2 && result.compare (result.size () - 2, 2, ", ") == 0)
    result.erase (result.size () - 2);
  while (!result.empty () && result.back () == ' ')
    result.pop_back ();
  while (!result.empty () && result.front () == ' ')
    result.erase (0, 1);
  if (result.empty ())
    result = "zero";
  return strdup (result.c_str ());
}

static char *
fn_held_worn_keys (a5_state_t *st, const char *name, const char *args)
{
  /* %Held[charkey]% / %Worn[charkey]% -> the "|"-joined keys of the objects
     the character directly holds / wears (Global.vb:2146/2401; HeldObjects /
     WornObjects default bRecursive:=False).  The runner takes the argument as
     a literal character KEY (htblCharacters.ContainsKey); an unknown key just
     DisplayErrors and renders "" (blank allowed, bAllowBlank). */
  const a5_character_t *c = a5model_character (st->adv, args);
  a5_owhere_t want = ci_eq (name, "held") ? A5_OWHERE_HELD_BY
                                          : A5_OWHERE_WORN_BY;
  if (c == nullptr)
    return strdup ("");
  return strdup (join_keys_pipe (objects_where (st, want, c->key)).c_str ());
}

static char *
fn_objectsin (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* %ObjectsIn[objkey]% -> "|"-joined keys of the objects directly inside
     (Global.vb:2255, Children(InsideObject).Values), open or not. */
  const a5_object_t *o = a5model_object (st->adv, args);
  if (o == nullptr)
    return strdup ("");
  return strdup (join_keys_pipe (
      objects_where (st, A5_OWHERE_IN_OBJECT, o->key)).c_str ());
}

static char *
fn_characterdescriptor (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* %CharacterDescriptor[charkey]% -> "article [prefix] first-descriptor"
     (clsCharacter.Descriptor, clsCharacter.vb:572; dispatched Global.vb:2074).
     No descriptors -> "". */
  const a5_character_t *c = a5model_character (st->adv, args);
  if (c == nullptr || c->n_descriptors == 0)
    return strdup ("");
  return strdup (char_descriptor_name (c, c->article).c_str ());
}

static char *
fn_taskcompleted (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* %TaskCompleted[taskkey]% -> the flag as VB Boolean.ToString ("True"/
     "False", Global.vb:2388). */
  int ti = a5state_task_index (st, args);
  if (ti < 0)
    return strdup ("");
  return strdup (st->task_done[ti] ? "True" : "False");
}

static char *
fn_sum (a5_state_t * /*st*/, const char * /*name*/, const char *args)
{
  /* %Sum[text]% (Global.vb:2368): keep digit and '-' chars, blank everything
     else, then total the space-split runs through SafeInt -- a run that is
     not a clean integer ("3-4", "-") contributes 0. */
  std::string t = args ? args : "";
  for (char &ch : t)
    if (!(ch >= '0' && ch <= '9') && ch != '-')
      ch = ' ';
  long total = 0;
  size_t p = 0;
  while (p < t.size ())
    {
      size_t e = t.find (' ', p);
      if (e == std::string::npos) e = t.size ();
      if (e > p)
        {
          std::string tok = t.substr (p, e - p);
          char *end = nullptr;
          long v = strtol (tok.c_str (), &end, 10);
          if (end != nullptr && *end == '\0')
            total += v;
        }
      p = e + 1;
    }
  char buf[32];
  snprintf (buf, sizeof buf, "%ld", total);
  return strdup (buf);
}

static char *
fn_popupchoice (a5_state_t * /*st*/, const char * /*name*/, const char *args)
{
  /* %PopUpChoice[prompt, choice1, choice2]% (Global.vb:2278): a Yes/No dialog
     where Yes picks choice1, No picks choice2.  Unlike PopUpInput, the runner
     asks with MsgBox, NOT the scripted-input channel, and MsgBox throws
     off-Windows, so FD lands in the ReplaceFunctions catch (Global.vb:2483)
     and the token stays verbatim -- no output, no script line consumed, and a
     SetVariable RHS keeps the literal %PopUpChoice[...]% text.  Beagle2's
     startup Autorun depends on all three: its gender stays Male because
     NewGender never equals "female".  That is what we do with no host callback
     installed; in particular never route the question through the a5_popup
     script feed, which would desync the transcript by a line.

     A host that CAN ask (a5text_set_popup_choice_cb -- the interactive Glk
     frontend, not the headless harness) gets the Windows behaviour instead:
     the chosen text replaces the token.  A malformed call with fewer than
     three arguments has nothing to choose between, so it stays verbatim too. */
  if (a5_popup_choice == nullptr || args == nullptr)
    return nullptr;

  std::string a = args;
  size_t c1 = a.find (','), c2 = (c1 == std::string::npos)
                                 ? std::string::npos : a.find (',', c1 + 1);
  if (c2 == std::string::npos)
    return nullptr;

  std::string prompt = a.substr (0, c1);
  std::string yes = a.substr (c1 + 1, c2 - c1 - 1);
  std::string no = a.substr (c2 + 1);
  popup_unquote (prompt); popup_unquote (yes); popup_unquote (no);

  int picked = a5_popup_choice (a5_popup_choice_ctx, prompt.c_str (),
                                yes.c_str (), no.c_str ());
  if (picked < 0)
    return nullptr;                /* host declined to ask: leave it verbatim */
  return strdup (picked ? yes.c_str () : no.c_str ());
}

static char *
fn_prevparentof (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* %PrevParentOf[key]% (Global.vb:2320): the item's PrevParent -- its parent
     as of the last turn start (the PrepareForNextTurn stamp, vb:5214-5218).
     Objects officially also re-stamp on each Move, so the turn-start snapshot
     only diverges when one object moves twice within a single turn. */
  int oi = a5state_object_index (st, args);
  if (oi >= 0 && st->obj_prev != nullptr)
    {
      a5_owhere_t w = st->obj_prev[oi].where;
      if (w == A5_OWHERE_ON_OBJECT || w == A5_OWHERE_IN_OBJECT
          || w == A5_OWHERE_HELD_BY || w == A5_OWHERE_WORN_BY
          || w == A5_OWHERE_PART_OBJECT || w == A5_OWHERE_PART_CHAR
          || w == A5_OWHERE_LOCATION || w == A5_OWHERE_LOCGROUP)
        return strdup (st->obj_prev[oi].key);
      return strdup ("");
    }
  int ci = a5state_character_index (st, args);
  if (ci >= 0 && st->char_prevpar != nullptr)
    return strdup (st->char_prevpar[ci] != nullptr ? st->char_prevpar[ci] : "");
  return strdup ("");
}

static char *
fn_prevlistobjectson (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* %PrevListObjectsOn[objkey]% (Global.vb:2315): ListObjectsOn evaluated
     against the previous game state (PreviousFunction restores States.Peek,
     Global.vb:596) -- computed here from the turn-start placement snapshot. */
  const char *k = resolve_object_arg (st, args);
  std::vector<const char *> v;
  if (k != nullptr && st->obj_prev != nullptr)
    for (int i = 0; i < st->adv->n_objects; i++)
      if (st->obj_prev[i].where == A5_OWHERE_ON_OBJECT
          && streq (st->obj_prev[i].key, k))
        v.push_back (st->adv->objects[i].key);
  if (v.empty ())
    return strdup ("nothing");
  return list_objects (st, v);
}

static char *
fn_case (a5_state_t * /*st*/, const char *name, const char *args)
{
  int i;
  char *s = strdup (args ? args : "");
  if (ci_eq (name, "ucase"))
    for (i = 0; s[i]; i++) s[i] = toupper ((unsigned char) s[i]);
  else if (ci_eq (name, "lcase"))
    for (i = 0; s[i]; i++) s[i] = tolower ((unsigned char) s[i]);
  else if (s[0])
    s[0] = toupper ((unsigned char) s[0]);
  return s;
}

// EvaluateUDF's expression sniff (Global.vb:1706): the VB regex
// `\d( )*[+-/*^]( )*\d`, whose `[+-/]` is the character RANGE '+'..'/' -- so
// the operator set is  * + , - . / ^  with any number of spaces around it.  A
// UDF argument matching this is folded through EvaluateExpression ("3 + 4" ->
// "7"); a plain number ("21") is left alone.  (Line comments here because the
// regex above contains a literal /*, which -Wcomment flags inside a block.)
static int
udf_arg_is_expression (const char *s)
{
  for (const char *p = s; *p != '\0'; p++)
    {
      if (!isdigit ((unsigned char) *p))
        continue;
      const char *q = p + 1;
      while (*q == ' ') q++;
      if (*q == '\0' || strchr ("*+,-./^", *q) == nullptr)
        continue;
      q++;
      while (*q == ' ') q++;
      if (isdigit ((unsigned char) *q))
        return 1;
    }
  return 0;
}

static char *
fn_convcharacter (a5_state_t *st, const char * /*name*/, const char * /*args*/)
{
  /* Global.vb:1762 substitutes %ConvCharacter% with the conversation char KEY. */
  return strdup (st->conv_char ? st->conv_char : "");
}

static char *
fn_replace (a5_state_t * /*st*/, const char * /*name*/, const char * /*args*/)
{
  /* In TEXT context the runner's function Select has no Replace arm --
     sResult stays "" ("Bad Function - Nothing output") and the token
     renders empty; the usable replace(a,b,c) lives in expressions
     (a5sexpr.cpp). */
  return strdup ("");
}

static char *
fn_version (a5_state_t * /*st*/, const char * /*name*/, const char * /*args*/)
{
  /* %version% (Global.vb:1765): the Runner's assembly version "5.0.36.6"
     (AssemblyInfo.vb) with the first two dots elided by the substring
     splice -> "5036.6". */
  return strdup ("5036.6");
}

static char *
fn_release (a5_state_t *st, const char * /*name*/, const char * /*args*/)
{
  /* %release% (Global.vb:1766): the <ifindex> block's <release><version>
     (BabelTreatyInfo ... Release.Version) -- which DEFAULTS to 1
     (Babel.vb:354 iVersion As Integer = 1), so a file carrying no release
     metadata still renders "1". */
  return strdup (st->adv->release != nullptr ? st->adv->release : "1");
}

/* The built-in functions, in the order eval_function() tries them.  A name
   is a call in one of three shapes: %Name% and %Name[args]% alike, only with
   an [args] block, or only without one; in any other shape it falls through
   to the references and variables. */
enum fn_args_t { FN_ARGS_ANY, FN_ARGS_REQUIRED, FN_ARGS_NONE };

static const struct
{
  const char *name;
  fn_args_t args;
  char *(*handler) (a5_state_t *st, const char *name, const char *args);
} eval_builtins[] = {
  {"player", FN_ARGS_ANY, fn_player},
  /* Bare %PopUpInput% (no [args]) is not a function call in the runner --
     the args-form regex doesn't match it, so it survives verbatim (ProbePopups
     intro).  Evaluating it here would also eat a script line as the popup
     answer, desyncing the transcript. */
  {"popupinput", FN_ARGS_REQUIRED, fn_popupinput},
  {"charactername", FN_ARGS_ANY, fn_charactername},
  {"characterproper", FN_ARGS_ANY, fn_characterproper},
  {"propername", FN_ARGS_ANY, fn_characterproper},
  {"convcharacter", FN_ARGS_ANY, fn_convcharacter},
  {"turns", FN_ARGS_ANY, fn_turns},
  {"alonewithchar", FN_ARGS_ANY, fn_alonewithchar},
  {"locationname", FN_ARGS_ANY, fn_locationname},
  {"locationof", FN_ARGS_ANY, fn_locationof},
  {"displaylocation", FN_ARGS_ANY, fn_displaylocation},
  {"theobject", FN_ARGS_REQUIRED, fn_object_names},
  {"aobject", FN_ARGS_REQUIRED, fn_object_names},
  {"objectname", FN_ARGS_REQUIRED, fn_object_names},
  {"object", FN_ARGS_REQUIRED, fn_object_names},
  {"theobjects", FN_ARGS_REQUIRED, fn_object_names},
  {"aobjects", FN_ARGS_REQUIRED, fn_object_names},
  {"objects", FN_ARGS_REQUIRED, fn_object_names},
  {"propertyvalue", FN_ARGS_REQUIRED, fn_propertyvalue},
  {"parentof", FN_ARGS_REQUIRED, fn_parentof},
  {"listobjectson", FN_ARGS_REQUIRED, fn_list_objects_on_in},
  {"listobjectsin", FN_ARGS_REQUIRED, fn_list_objects_on_in},
  {"listobjectsonandin", FN_ARGS_REQUIRED, fn_list_objects_on_and_in},
  {"listcharacterson", FN_ARGS_REQUIRED, fn_list_characters},
  {"listcharactersin", FN_ARGS_REQUIRED, fn_list_characters},
  {"listcharactersonandin", FN_ARGS_REQUIRED, fn_list_characters},
  {"listheld", FN_ARGS_REQUIRED, fn_list_held_worn},
  {"listworn", FN_ARGS_REQUIRED, fn_list_held_worn},
  {"displaycharacter", FN_ARGS_REQUIRED, fn_display_char_obj},
  {"displayobject", FN_ARGS_REQUIRED, fn_display_char_obj},
  {"listexits", FN_ARGS_REQUIRED, fn_list_exits},
  {"listobjectsatlocation", FN_ARGS_REQUIRED, fn_list_objects_at_location},
  {"numberastext", FN_ARGS_REQUIRED, fn_number_as_text},
  {"pcase", FN_ARGS_ANY, fn_case},
  {"ucase", FN_ARGS_ANY, fn_case},
  {"lcase", FN_ARGS_ANY, fn_case},
  {"held", FN_ARGS_REQUIRED, fn_held_worn_keys},
  {"worn", FN_ARGS_REQUIRED, fn_held_worn_keys},
  {"objectsin", FN_ARGS_REQUIRED, fn_objectsin},
  {"characterdescriptor", FN_ARGS_REQUIRED, fn_characterdescriptor},
  {"taskcompleted", FN_ARGS_REQUIRED, fn_taskcompleted},
  {"sum", FN_ARGS_REQUIRED, fn_sum},
  {"popupchoice", FN_ARGS_REQUIRED, fn_popupchoice},
  {"prevparentof", FN_ARGS_REQUIRED, fn_prevparentof},
  {"prevlistobjectson", FN_ARGS_REQUIRED, fn_prevlistobjectson},
  {"replace", FN_ARGS_REQUIRED, fn_replace},
  {"version", FN_ARGS_NONE, fn_version},
  {"release", FN_ARGS_NONE, fn_release},
};

/* User-defined <Function>s (clsUserFunction): the runner's ReplaceFunctions runs
   EvaluateUDF over every UDF FIRST (Global.vb:1748), matching the exact
   case-sensitive name with an optional [args] block.  The Output is a
   restriction-gated description whose declared <Argument> names are
   substituted with the (already function-expanded) argument values -- The
   Last Expedition's %Lantern[]% renders "with his free gloved hand" while
   the lantern is held, and its %Red[text]%-style wrappers colour the arg. */
static char *
eval_udf (a5_state_t *st, const char *name, const char *args)
{
  int i;

  static int udf_depth = 0;
  for (i = 0; i < st->adv->n_udfs; i++)
    {
      const a5_udf_t *u = &st->adv->udfs[i];
      if (u->name == nullptr || strcmp (u->name, name) != 0)
        continue;
      const a5_xml_node_t *outp = a5xml_child (u->node, "Output");
      if (outp == nullptr || udf_depth > 8)   /* The runner: recursive UDF = error */
        return strdup ("");
      /* EvaluateUDF (Global.vb:1653) substitutes the arguments into the RAW
         Output text -- BEFORE any function/expression evaluation -- then
         re-runs ReplaceFunctions on the substituted body
         (`sText = ReplaceFunctions(re.Replace(...))`) while the `<#...#>`
         expressions stay GUID-stashed for the Display-time
         ReplaceExpressions pass.  So `Double of %n% is <# %n% * 2 #>.` on
         %Double[21]% must become "Double of 21 is <# 21 * 2 #>." here, with
         the outer replace_expressions pass reducing it to 42 -- evaluating
         the body first computed <# %n% * 2 #> with %n% unbound (= 0).
         (Not mirrored: the restriction-key Parameter-<name> rewrite and the
         refsUDF %objects% NewReferences swap during the render -- Scarier
         gates the Output's restrictions against the live state instead.) */
      udf_depth++;
      char *raw = a5text_eval_description (st, outp);
      std::string body = (raw != nullptr) ? raw : "";
      free (raw);
      if (args != nullptr && args[0] != '\0')
        {
          /* SplitArgs (Global.vb:1623): split on commas at bracket depth 0
             ('('/'[' up, ')'/']' down); empty chunks are dropped, nothing
             is trimmed. */
          std::vector<std::string> vals;
          {
            std::string cur;
            int level = 0;
            for (const char *p = args; *p != '\0'; p++)
              {
                if (*p == '(' || *p == '[') level++;
                else if (*p == ')' || *p == ']') level--;
                if (*p == ',' && level == 0)
                  { if (!cur.empty ()) vals.push_back (cur); cur.clear (); }
                else cur += *p;
              }
            if (!cur.empty ()) vals.push_back (cur);
          }
          size_t vi = 0;
          for (const a5_xml_node_t *an = u->node->first_child;
               an != nullptr; an = an->next)
            {
              if (strcmp (an->name, "Argument") != 0)
                continue;
              const char *anm = a5xml_child_text (an, "Name");
              if (anm != nullptr && vi < vals.size ())
                {
                  std::string v = vals[vi];
                  /* "Our function argument could be an expression": fold a
                     digit-op-digit argument through EvaluateExpression
                     (%Double[3 + 4]% -> body sees 7); keep the original on
                     a failed evaluation (EvaluateExpression = Nothing). */
                  if (udf_arg_is_expression (v.c_str ()))
                    {
                      char *ev = a5text_eval_expression (st, v.c_str ());
                      if (ev != nullptr && ev[0] != '\0')
                        v = ev;
                      free (ev);
                    }
                  std::string tok = std::string ("%") + anm + "%";
                  size_t at = 0;
                  while ((at = body.find (tok, at)) != std::string::npos)
                    { body.replace (at, tok.size (), v); at += v.size (); }
                }
              vi++;
            }
        }
      /* The runner's trailing ReplaceFunctions recursion: expand any
         functions the substituted body carries (%TheObject[ObjectBall]%),
         keeping its <#...#> bodies untouched for the outer expression
         pass. */
      std::vector<std::string> saved;
      std::string prot = protect_exprs (body.c_str (), saved);
      char *funcs = replace_functions (st, prot.c_str ());
      std::string rest = restore_exprs (funcs, saved);
      free (funcs);
      udf_depth--;
      return strdup (rest.c_str ());
    }

  return nullptr;
}

/* A bound reference produced by the parser this turn (%direction%, %text%).
   Sets *unbound when the name is none, or one with nothing to render, so
   that the caller goes on to the variables; a NULL return without it is
   final. */
static char *
eval_bound_reference (a5_state_t *st, const char *name, int *unbound)
{
  const char *bound = nullptr;
  std::string slot;     /* ReferencedCharacterN / LocationN / ItemN */
  int is_ref = ref_slot_name (name, slot);
  if (ci_eq (name, "direction") || ci_eq (name, "direction1"))
    bound = a5state_lookup_ref (st, "ReferencedDirection");
  else if (ci_eq (name, "text") || ci_eq (name, "text1"))
    bound = a5state_lookup_ref (st, "ReferencedText");
  else if (ci_eq (name, "number") || ci_eq (name, "number1"))
    bound = a5state_lookup_ref (st, "ReferencedNumber");
  else if (ci_eq (name, "object") || ci_eq (name, "objects")
           || ci_eq (name, "object1") || ci_eq (name, "object2"))
    {
      /* A plural %objects% that resolved to several items renders the whole
         set as an "a, b and c" list of full names (mirroring the Adrift 5 runner's
         %objects% -> "key1|key2|..." -> OO list render; each name without an
         article, like a bare %object%). */
      if (ci_eq (name, "objects") && st->n_ref_items > 1)
        {
          std::vector<const char *> keys (st->ref_items,
                                          st->ref_items + st->n_ref_items);
          return list_objects_art (st, keys, A5_ART_NONE);
        }
      /* A bare %objectN% renders the entity KEY, mirroring the runner's
         ReplaceFunctions (nr.Items(0).MatchingPossibilities(0), Global.vb:
         1795): ReplaceOO only rewrites `key.Property` tokens, so a standalone
         key survives verbatim.  For an ADRIFT-default noun-key that is the
         noun WITHOUT its adjective/prefix (`Nymphs`, not `Comely Nymphs`),
         and -- crucially -- it lets an OO reference-list argument whose head
         is `%object%.Children(...)` chain from the real key. */
      /* The singular %object%/%object1% never resolves off a plural
         %objects% binding here either (the runner GetReference needs
         ReferenceMatch "object1", clsUserSession.vb:3990) -- so the
         Blender's "You put %object1%.Name on the Table." on a `put
         %objects% on %object2%` command stays VERBATIM like the runner. */
      if ((ci_eq (name, "object") || ci_eq (name, "object1"))
          && st->ref_object1_plural)
        return nullptr;
      const char *ref = ci_eq (name, "object2") ? "ReferencedObject2"
                                                 : "ReferencedObject";
      const char *key = a5state_lookup_ref (st, ref);
      const a5_object_t *o = key ? a5model_object (st->adv, key) : nullptr;
      if (o != nullptr)
        return strdup (o->key);
    }
  else if (is_ref && slot.compare (0, 19, "ReferencedCharacter") == 0)
    {
      const char *key = a5state_lookup_ref (st, slot.c_str ());
      const a5_character_t *ch = key ? a5model_character (st->adv, key) : nullptr;
      if (ch != nullptr && ch->name != nullptr)
        return strdup (ch->name);
    }
  else if (is_ref && slot.compare (0, 16, "ReferencedObject") != 0)
    {
      /* %locationN% / %itemN%.  Like %objectN%: the runner's
         ReplaceFunctions substitutes the bound entity KEY
         (nr.Items(0).MatchingPossibilities(0)) for a resolved reference;
         an unbound one stays verbatim (in its numbered form -- see the
         singular rewrite below).  (A bare %object3%..%object5% is not
         resolved on this path; only the function-argument path in
         replace_functions keys off those slots.) */
      bound = a5state_lookup_ref (st, slot.c_str ());
    }
  if (bound != nullptr)
    return strdup (bound);

  *unbound = 1;
  return nullptr;
}

/* A bare %VariableName% (or %Score% etc.). */
static char *
eval_variable (a5_state_t *st, const char *name)
{
  int i;

  for (i = 0; i < st->adv->n_variables; i++)
    if (ci_eq (st->adv->variables[i].name, name))
      {
        /* Inside an ALR NewText the value must be the END-OF-TURN one
           (the runner expands ALRs only at Display): emit a deferred sentinel,
           resolved by a5text_expand_var_defers in finish_turn. */
        if (st->alr_defer_vars)
          {
            size_t nl = strlen (st->adv->variables[i].name);
            char *s = (char *) malloc (nl + 3);
            s[0] = A5_VARDEF_MARK;
            memcpy (s + 1, st->adv->variables[i].name, nl);
            s[nl + 1] = A5_VARDEF_MARK;
            s[nl + 2] = '\0';
            return s;
          }
        if (streq (st->adv->variables[i].type, "Text"))
          return strdup (st->var_text[i] ? st->var_text[i] : "");
        else
          {
            char buf[32];
            snprintf (buf, sizeof buf, "%ld", st->var_num[i]);
            return strdup (buf);
          }
      }

  return nullptr;
}

/* An array-variable element: %VariableName[index]% (1-based, mirroring
   ADRIFT's clsVariable string/int arrays).  A Text array stores its elements
   newline-separated in the single var_text string (the InitialValue layout),
   so split on '\n' and return the index'th line; out-of-range -> empty. */
static char *
eval_variable_element (a5_state_t *st, const char *name, const char *args)
{
  int i;

  for (i = 0; i < st->adv->n_variables; i++)
    if (ci_eq (st->adv->variables[i].name, name))
      {
        char *end = nullptr;
        long idx = strtol (args, &end, 10);
        while (end != nullptr && (*end == ' ' || *end == '\t'))
          end++;
        /* A non-literal index (e.g. %notallowed[RAND(1,6)]%) must be
           evaluated as an expression -- the runner's clsVariable indexing runs the
           bracket contents through EvaluateExpression.  Only fall through to
           the plain-literal fast path when args is exactly an integer. */
        if (end == args || (end != nullptr && *end != '\0'))
          {
            char *ev = a5text_eval_expression (st, args);
            char *e2 = nullptr;
            long v = strtol (ev, &e2, 10);
            int oknum = (e2 != ev);
            free (ev);
            if (!oknum)
              break;            /* still not numeric -> leave verbatim */
            idx = v;
          }
        if (streq (st->adv->variables[i].type, "Text"))
          {
            const char *s = st->var_text[i] ? st->var_text[i] : "";
            long line = 1;
            const char *ls = s;
            while (line < idx && *s != '\0')
              if (*s++ == '\n')
                { line++; ls = s; }
            if (line != idx)
              return strdup ("");
            const char *le = strchr (ls, '\n');
            size_t n = le ? (size_t) (le - ls) : strlen (ls);
            return strndup (ls, n);
          }
        else
          {
            /* Numeric array element (clsVariable.IntValue(idx), 1-based;
               out-of-range -> ErrMsg + 0).  A scalar Numeric with a stray
               [index] falls back to its value (index defaults to 1). */
            char buf[32];
            snprintf (buf, sizeof buf, "%ld",
                      a5state_var_get_elem (st, i, idx));
            return strdup (buf);
          }
      }

  return nullptr;
}

/* Evaluate one %Name[args]% (args already function-expanded), or NULL. */
char *
eval_function (a5_state_t *st, const char *name, const char *args)
{
  st->name_cap_eligible = 0;      /* only a resolved CharacterName sets it */

  char *udf = eval_udf (st, name, args);
  if (udf != nullptr)
    return udf;

  for (const auto &b : eval_builtins)
    if (ci_eq (name, b.name)
        && (b.args == FN_ARGS_ANY
            || (b.args == FN_ARGS_REQUIRED) == (args != nullptr)))
      return b.handler (st, name, args);

  if (args != nullptr)
    return eval_variable_element (st, name, args);

  int unbound = 0;
  char *bound = eval_bound_reference (st, name, &unbound);
  if (!unbound)
    return bound;

  /* NULL = unhandled: leave the token verbatim. */
  return eval_variable (st, name);
}

/* Capture a pending CharacterName render as a PronounKeys entry at output
   offset `off` -- the ledger half of the runner's Global.vb:2108 (the eval side sets
   st->pron_pending; ReplaceFunctions adds in expression mode too). */
void
pron_capture (a5_state_t *st, long off)
{
  if (!st->pron_pending)
    return;
  if (st->n_pron < A5_MAX_PRON)
    {
      st->pron_persp[st->n_pron] = st->pron_pending;
      st->pron_off[st->n_pron] = off;
      st->pron_key[st->n_pron] = st->pron_pending_key;
      st->pron_pron[st->n_pron] = st->pron_pending_pron;
      st->n_pron++;
    }
  st->pron_pending = 0;
  st->pron_pending_key = nullptr;
  st->pron_pending_pron = 0;
}

/* The runner's ReplaceFunctions resolves model-variable tokens (%name% and %name[idx]%)
   in variable-declaration order, in a pass (Global.vb:1972-2019, a
   `For Each var In Adventure.htblVariables.Values` loop) that runs BEFORE the
   left-to-right generic-function scan.  Scarier's replace pass is otherwise
   strictly text-order, which only diverges when a single text holds two or more
   variable tokens whose index/value evaluation has a side effect -- in practice
   an embedded Rand.  Lost Coastlines' inventory line
   "%defaultshirt[Rand(1,10)]% and %defaultpants[Rand(1,10)]%" is the corpus case:
   the `defaultpants` variable is declared before `defaultshirt`, so the runner draws the
   pants index first.  For pure (side-effect-free) reads the resolved value is
   independent of order, so no other corpus text is affected.  This pre-pass
   mirrors the runner: walk the model variables in order and splice in each variable's
   tokens (via the same eval_function the inline path uses), leaving function and
   reference tokens for the text-order scan that follows. */
static char *
resolve_model_vars_ordered (a5_state_t *st, const char *src)
{
  char *cur = strdup (src);
  int vi;

  if (strchr (src, '%') == nullptr)
    return cur;                         /* nothing to substitute */

  for (vi = 0; vi < st->adv->n_variables; vi++)
    {
      const char *vname = st->adv->variables[vi].name;
      size_t vlen;
      int guard = 0;

      if (vname == nullptr || vname[0] == '\0')
        continue;
      vlen = strlen (vname);

      while (guard++ < 4096)
        {
          char *hit = nullptr, *scan = cur;
          const char *after;
          char *name, *rawargs = nullptr, *value = nullptr, *token;
          char *tok_end;

          /* Find the next "%vname" (case-insensitive) delimited by '%' or '['. */
          while ((scan = strchr (scan, '%')) != nullptr)
            {
              const char *nm = scan + 1;
              if (strncasecmp (nm, vname, vlen) == 0
                  && (nm[vlen] == '%' || nm[vlen] == '['))
                { hit = scan; break; }
              scan++;
            }
          if (hit == nullptr)
            break;

          after = hit + 1 + vlen;
          name = strndup (hit + 1, vlen);
          if (*after == '%')
            {
              tok_end = (char *) after + 1;
              value = eval_function (st, name, nullptr);
            }
          else                          /* '[' : an array element */
            {
              int depth = 1;
              const char *a = after + 1;
              const char *astart = a;
              char *args;
              while (*a && depth > 0)
                {
                  if (*a == '[') depth++;
                  else if (*a == ']') depth--;
                  if (depth > 0) a++;
                }
              if (!(*a == ']' && a[1] == '%'))
                { free (name); break; }  /* unterminated -> leave for main loop */
              tok_end = (char *) a + 2;
              rawargs = strndup (astart, (size_t) (a - astart));
              /* Deferred draw: inside an AggregateOutput completion (st->expr_defer
                 armed), a random array index (`%flavorskybreak[rand(1,25)]%`) has
                 its draw held to end-of-turn Display, just like a `<#Rand#>`
                 expression -- the runner skips this substitution now (AggregateOutput ->
                 ReplaceFunctions runs at Display).  Emit a `\004<idx>\004` sentinel
                 and push the raw token (tagged \001) to the sink; the flush
                 re-resolves it, drawing the index then.  So Skybreak's dock flavor
                 draws AFTER the location-trigger task's `SidequestE = RAND(1,10)`. */
              if (st->expr_defer != nullptr && expr_bears_random (rawargs))
                {
                  std::vector<std::string> *sink =
                      (std::vector<std::string> *) st->expr_defer;
                  char mark[24];
                  snprintf (mark, sizeof mark, "\004%d\004", (int) sink->size ());
                  std::string tok = "\001%";
                  tok += name; tok += "["; tok += rawargs; tok += "]%";
                  sink->push_back (tok);
                  value = strdup (mark);
                }
              else
                {
                  args = replace_functions (st, rawargs, 1);
                  if (strchr (args, '.') != nullptr)
                    {
                      char *ooargs = a5expr_replace (st, args);
                      free (args); args = ooargs;
                    }
                  value = eval_function (st, name, args);
                  free (args);
                }
            }

          if (value == nullptr)             /* not resolvable here */
            { free (name); free (rawargs); break; }

          token = strndup (hit, (size_t) (tok_end - hit));
          {
            char *next = str_replace_all (cur, token, value);
            free (cur); cur = next;
          }
          /* Guard against a value that re-introduces the same token. */
          if (strstr (value, token) != nullptr)
            { free (token); free (name); free (rawargs); free (value); break; }
          free (token); free (name); free (rawargs); free (value);
        }
    }
  return cur;
}

/* Evaluate a %Name[args]% call whose "[" is at q: the args are replaced
   recursively (as an argument list) and any OO chain left in them resolved
   first.  Sets *value and returns the position past the closing "%", or
   nullptr if the brackets do not close into "]%". */
static const char *
eval_bracket_call (a5_state_t *st, const char *name_start, size_t nlen,
                   const char *q, char **value)
{
  int depth = 1;
  const char *a = q + 1;
  const char *astart = a;
  while (*a && depth > 0)
    {
      if (*a == '[') depth++;
      else if (*a == ']') depth--;
      if (depth > 0) a++;
    }
  if (*a != ']' || a[1] != '%')
    return nullptr;

  char *rawargs = strndup (astart, (size_t) (a - astart));
  char *name = strndup (name_start, nlen);
  char *args = replace_functions (st, rawargs, 1);
  /* Resolve any OO property chain left in the args (e.g.
     %LCase[%object%.OpenStatus]% -> args "Closet.OpenStatus"
     -> "Closed") before the outer function transforms it.
     the Adrift 5 runner's ReplaceFunctions(sArgs) recursion runs its
     own ReplaceOO pass on the args (Global.vb:2046); Scarier's
     OO pass otherwise runs only after the whole replace pass,
     too late for a function that consumed the chain as an
     argument (LCase would lower-case the raw "Closet.OpenStatus"
     to "closet.openstatus").  a5expr_replace is a no-op unless
     args holds a real <key>.<chain>. */
  if (strchr (args, '.') != nullptr)
    {
      char *ooargs = a5expr_replace (st, args);
      free (args);
      args = ooargs;
    }
  *value = eval_function (st, name, args);
  free (rawargs);
  free (args);
  free (name);
  return a + 2;
}

/* Emit a resolved token value into sb (and free it). */
static void
emit_function_value (a5_state_t *st, sb_t *sb, char *value)
{
  /* CharacterName sentence-start fudge (the runner Global.vb:2103): when
     the resolved name is immediately preceded by two spaces or a
     CRLF, capitalise its first character.  iMatchLoc>3 there means
     at least 3 chars precede the token (sb->len >= 3 here). */
  if (st->name_cap_eligible && sb->len >= 3 && value[0] != '\0'
      && ((sb->p[sb->len - 2] == ' ' && sb->p[sb->len - 1] == ' ')
          || (sb->p[sb->len - 2] == '\r' && sb->p[sb->len - 1] == '\n')))
    value[0] = (char) toupper ((unsigned char) value[0]);
  st->name_cap_eligible = 0;
  pron_capture (st, (long) sb->len);
  sb_puts (sb, value);
  free (value);
}

/* Emit the unresolved token p..q (exclusive) into sb. */
static void
emit_unresolved_token (sb_t *sb, const char *p, const char *q)
{
  /* leave the original token verbatim -- but the runner rewrites
     EVERY bare singular reference alias to its numbered form
     BEFORE resolving (ReplaceIgnoreCase, Global.vb:1754-1760:
     %object% %character% %location% %direction% %item% %text%
     %number% -> %...1%), so an unresolved singular surfaces
     numbered: the Blender's "You put %object%.Name on the
     table.", ProbeRefCapture's intro "...%number1%,
     %location1%, %item1%, and %characters%." (the plurals are
     NOT rewritten). */
  static const char *const singulars[] =
    { "object", "character", "location", "direction",
      "item", "text", "number", nullptr };
  size_t tlen = (size_t) (q - p);
  int rewrote = 0;
  for (int si = 0; singulars[si] != nullptr; si++)
    {
      size_t sl = strlen (singulars[si]);
      if (tlen == sl + 2 && p[0] == '%' && p[tlen - 1] == '%'
          && strncasecmp (p + 1, singulars[si], sl) == 0)
        {
          sb_putc (sb, '%');
          sb_puts (sb, singulars[si]);
          sb_puts (sb, "1%");
          rewrote = 1;
          break;
        }
    }
  if (!rewrote)
    sb_putn (sb, p, (size_t) (q - p));
}

/* Replace %function%/%variable% tokens in src (single pass; args recurse).
   as_arg: the text is the argument list of an enclosing %Func[...]%, so a
   bare object/character reference resolves to its KEY, not its name. */
char *
replace_functions (a5_state_t *st, const char *src, int as_arg)
{
  sb_t sb;
  char *pre = resolve_model_vars_ordered (st, src);
  const char *p = pre;

  sb_init (&sb);
  while (*p != '\0')
    {
      if (*p == '%')
        {
          const char *q = p + 1;
          const char *name_start = q;
          char *name, *value = nullptr;
          int ok = 0;

          while (*q && (isalnum ((unsigned char) *q) || *q == '_' || *q == '-'))
            q++;
          if (q > name_start)
            {
              size_t nlen = (size_t) (q - name_start);
              if (*q == '%')
                {
                  name = strndup (name_start, nlen);
                  /* %reference%.Property...  : emit the resolved entity key and
                     leave the ".chain" as literal text for the later OO pass
                     (mirrors the Adrift 5 runner substituting %object%->key before
                     ReplaceOO, so embedded %Func[]% are gone by then).  Require
                     a real property step after the dot (an alpha char) so a
                     sentence-ending period ("you take %objects%.") is not
                     mistaken for a chain -- that renders via the bare path
                     below (the name / multi-object list), not the raw key. */
                  if (q[1] == '.' && isalpha ((unsigned char) q[2]))
                    {
                      char *fk = oo_firstkey (st, name);
                      if (fk != nullptr)
                        {
                          sb_puts (&sb, fk);
                          free (fk); free (name);
                          p = q + 1;        /* past the closing %, before the '.' */
                          continue;
                        }
                    }
                  /* A bare reference (%object%, %character%, ...) used as the
                     argument of an outer %Func[...]% must resolve to the entity
                     *key*, not its display name: the Adrift 5 runner substitutes
                     %object%->MatchingPossibilities(0) (the key) before any
                     %Func[]% is evaluated (Global.vb:1752+), so e.g.
                     "%TheObject[%object%]%" keys off the exact referenced object
                     rather than re-resolving the rendered noun by name (which
                     would pick the first object sharing that noun). */
                  if (as_arg)
                    {
                      char *fk = oo_firstkey (st, name);
                      if (fk != nullptr)
                        {
                          sb_puts (&sb, fk);
                          free (fk); free (name);
                          p = q + 1;
                          continue;
                        }
                      /* An *unbound* object/character reference keyword used as a
                         function argument resolves to the empty string, just as
                         the Adrift 5 runner's GetReference("ReferencedObject") returns ""
                         when no reference was captured (a reference-free task).
                         The outer %TheObject[]%/%ObjectName[]% then renders
                         "nothing" (htblObjects.List on an empty set) -- e.g. Anno's
                         OpeningHid displaying the carried "...can't see
                         %TheObject[%object%]%." as "You can't see nothing." */
                      if (is_obj_char_ref (name))
                        {
                          free (name);
                          p = q + 1;
                          continue;        /* substitute "" */
                        }
                    }
                  value = eval_function (st, name, nullptr);
                  free (name);
                  q++;
                  ok = 1;
                }
              else if (*q == '[')
                {
                  const char *end = eval_bracket_call (st, name_start, nlen, q,
                                                       &value);
                  if (end != nullptr)
                    {
                      q = end;
                      ok = 1;
                    }
                }
            }

          if (ok)
            {
              if (value != nullptr)
                emit_function_value (st, &sb, value);
              else
                emit_unresolved_token (&sb, p, q);
              p = q;
              continue;
            }
        }
      sb_putc (&sb, *p);
      p++;
    }
  free (pre);
  return sb_finish (&sb);
}

/* ---------------------------------------------------- PopUpInput side channel */

void
a5text_set_popup_cb (a5_popup_cb cb, void *ctx)
{
  a5_popup = cb;
  a5_popup_ctx = ctx;
}

void
a5text_set_popup_choice_cb (a5_popup_choice_cb cb, void *ctx)
{
  a5_popup_choice = cb;
  a5_popup_choice_ctx = ctx;
}
