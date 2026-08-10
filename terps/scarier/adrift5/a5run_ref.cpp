/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- reference resolution.
 *
 * Matches a task's captured references to the model keys in scope
 * (InputMatchesObject / InputMatchesCharacter), refines the candidate set by
 * the task's restrictions, and handles multiple-object (%objects%) references.
 * When a reference still names several in-scope candidates it reports an
 * ambiguity (amb_info / RR_AMBIG) for the caller to raise "Which X?".  Split
 * out of a5run.cpp; shared types and entry points are in a5run_internal.h.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "a5parse.h"
#include "a5restr.h"
#include "a5run.h"
#include "a5run_internal.h"
#include "a5text.h"
#include "a5util.h"

/* A reference alias split into its base ("object1" -> "object") and, when
   `num_out` is non-NULL, its trailing digit run ("object1" -> "1").  ADRIFT
   suffixes each singular reference slot with an index (FileIO.vb:647), which
   several call sites strip back off. */
static std::string
ref_base (const std::string &name, std::string *num_out = NULL)
{
  std::string base = name, num;
  while (!base.empty () && isdigit ((unsigned char) base.back ()))
    { num.insert (num.begin (), base.back ()); base.pop_back (); }
  if (num_out != NULL)
    *num_out = num;
  return base;
}

/* Collect an entity's matchable words (article + prefix + names/descriptors). */
static void
collect_words (const char *article, const char *prefix,
               const char **names, int n_names,
               std::vector<std::string> &allowed, std::vector<std::string> &nouns)
{
  if (article && article[0])
    allowed.push_back (lower (article));
  for (auto &w : split_ws (prefix))
    allowed.push_back (lower (w));
  for (int i = 0; names != NULL && i < n_names; i++)
    for (auto &w : split_ws (names[i]))
      { allowed.push_back (lower (w)); nouns.push_back (lower (w)); }
}

/* The input words from index `i` onward must equal exactly one whole name.
   Both `in` and each entry of `names` are pre-lowercased by name_match, so the
   compare is a plain string equality with no per-node allocation. */
static int
name_match_tail (const std::vector<std::string> &in, size_t i,
                 const std::vector<std::vector<std::string>> &names)
{
  size_t rest = in.size () - i;
  if (rest == 0)
    return 0;
  for (auto &nw : names)
    {
      if (nw.size () != rest)
        continue;
      int eq = 1;
      for (size_t k = 0; k < nw.size (); k++)
        if (nw[k] != in[i + k]) { eq = 0; break; }
      if (eq)
        return 1;
    }
  return 0;
}

/* Match the `(prefix_pi )?...(name_alt)` tail of the regex with backtracking:
   each remaining prefix word is independently optional (in order), then one
   whole name must consume the rest.  Crucially this BACKTRACKS -- a prefix word
   that also equals a name (e.g. the ID pass's prefix "ID" vs its name "id")
   must be skippable so the bare noun still resolves, exactly as .NET's regex
   engine does for the Adrift 5 runner's `(ID )?(id|pass)` pattern. */
static int
name_match_prefix (const std::vector<std::string> &in, size_t i,
                   const std::vector<std::string> &pfx, size_t pi,
                   const std::vector<std::vector<std::string>> &names)
{
  if (pi == pfx.size ())
    return name_match_tail (in, i, names);
  /* option 1: skip this prefix word. */
  if (name_match_prefix (in, i, pfx, pi + 1, names))
    return 1;
  /* option 2: consume it if the next input word matches (both pre-lowered). */
  if (i < in.size () && in[i] == pfx[pi])
    if (name_match_prefix (in, i + 1, pfx, pi + 1, names))
      return 1;
  return 0;
}

/* A name alternative goes into the runner's regex VERBATIM (clsObject /
   clsCharacter sRegularExpressionString paste arlNames / ProperName straight
   into the pattern, with no trim), and the pattern is anchored `^...$` against
   the typed text (clsUserSession.vb:5391/5495).  So a name carrying stray
   whitespace -- The Awakeners' NPC is `<Name>Alistair </Name>` -- can never
   match anything the player can type, and the character is simply
   unreferenceable: `x alistair` is "You see no such thing.".  Matching here is
   word-based, which would silently forgive that, so drop any name whose raw
   form is not already the canonical single-space join of its words. */
static int
name_is_matchable (const char *s)
{
  std::string canon;
  if (s == NULL)
    return 0;
  for (auto &w : split_ws (s))
    { if (!canon.empty ()) canon += ' '; canon += w; }
  return canon == s;
}

/* Match `text` against an entity the way clsObject/clsCharacter's
   sRegularExpressionString does: `^(article |the )?(prefix_i )?...(name_alt)$` --
   an optional article, each prefix word independently optional in order, then
   exactly one whole name (a multi-word name like "key rack" must appear in
   full).  The prefix/name tail is matched with backtracking
   (name_match_prefix). */
static int
name_match (const char *article, const char *prefix,
            const char **names, int n_names, const std::string &text)
{
  std::vector<std::string> in = split_ws (text.c_str ());
  if (in.empty ())
    return 0;
  /* Lowercase the input, prefix and every candidate name ONCE up front, so the
     backtracking match (name_match_prefix/_tail) is pure string comparison --
     the old code re-lowered both operands at every recursion node and re-split
     each name on every tail probe. */
  for (auto &w : in) w = lower (w);
  std::vector<std::string> pfx = split_ws (prefix);
  for (auto &w : pfx) w = lower (w);
  std::vector<std::vector<std::string>> names_lc;
  names_lc.reserve ((size_t) n_names);
  for (int n = 0; n < n_names; n++)
    {
      std::vector<std::string> nw;
      if (!name_is_matchable (names[n]))
        continue;
      nw = split_ws (names[n]);
      for (auto &w : nw) w = lower (w);
      names_lc.push_back (std::move (nw));
    }
  /* without the optional leading article. */
  if (name_match_prefix (in, 0, pfx, 0, names_lc))
    return 1;
  /* with the optional leading article (the entity's own, or "the") consumed. */
  {
    const std::string &w = in[0];
    std::string art = article ? lower (article) : "";
    if (w == "the" || (!art.empty () && w == art))
      if (name_match_prefix (in, 1, pfx, 0, names_lc))
        return 1;
  }
  return 0;
}

/* An object's EFFECTIVE naming under the _ObjectArticle/_ObjectPrefix/
   _ObjectNoun mandatory-property overrides (clsUserSession.vb:1972-1982):
   `SetProperty <obj> _ObjectNoun <text>` replaces arlNames(0) ONLY -- the
   other names and the prefix survive -- so after Illumina's `read sign` runs
   `SetProperty cl_Door _ObjectNoun Guard room door`, the door answers to
   "guard room door" (and its remaining aliases) but no longer to "door" or
   prefix+"door" ("southern door").  The overrides live in the SetProperty
   runtime layer (a5state_entity_prop). */
struct obj_naming
{
  const char *article, *prefix;
  std::vector<const char *> names;
};
static obj_naming
effective_naming (a5_state_t *st, const a5_object_t *o)
{
  obj_naming n;
  const char *a  = a5state_entity_prop (st, o->key, "_ObjectArticle");
  const char *p  = a5state_entity_prop (st, o->key, "_ObjectPrefix");
  const char *n0 = a5state_entity_prop (st, o->key, "_ObjectNoun");
  n.article = a != NULL ? a : o->article;
  n.prefix  = p != NULL ? p : o->prefix;
  for (int i = 0; i < o->n_names; i++)
    n.names.push_back (o->names[i]);
  if (n0 != NULL)
    {
      if (!n.names.empty ()) n.names[0] = n0;
      else n.names.push_back (n0);
    }
  return n;
}

/* Collect the matchable words of an object / character key, so the same
   word-set drives both the initial match and the cross-turn clarifier. */
static void
object_words (a5_state_t *st, const a5_object_t *o,
              std::vector<std::string> &allowed, std::vector<std::string> &nouns)
{
  obj_naming n = effective_naming (st, o);
  collect_words (n.article, n.prefix,
                 n.names.empty () ? NULL : &n.names[0], (int) n.names.size (),
                 allowed, nouns);
}
/* Is the character's *proper name* usable as a reference word yet?  Mirrors
   clsCharacter.sRegularExpressionString: the proper name is excluded once the
   game defines a "Known" character property and this character does not (yet)
   have it -- i.e. an unintroduced NPC is addressable only by descriptor
   ("woman"), not by name ("susan").  A character with no descriptors, or a game
   with no Known property, always exposes the proper name. */
int
char_name_usable (a5_state_t *st, const a5_character_t *c)
{
  int game_has_known = 0, i;
  if (c->n_descriptors == 0)
    return 1;
  for (i = 0; i < st->adv->n_propdefs; i++)
    if (streq (st->adv->propdefs[i].key, "Known")
        && streq (st->adv->propdefs[i].property_of, "Characters"))
      { game_has_known = 1; break; }
  if (!game_has_known)
    return 1;
  if (a5_prop_find (c->props, c->n_props, "Known") != NULL)
    return 1;
  return a5state_entity_prop (st, c->key, "Known") != NULL;
}

static void
character_words (a5_state_t *st, const a5_character_t *c,
                 std::vector<std::string> &allowed, std::vector<std::string> &nouns)
{
  /* Article + prefix apply to every noun ("young woman", "the woman"); the
     proper name is exposed only once the character is Known (see above). */
  collect_words (c->article, c->prefix, c->descriptors, c->n_descriptors,
                 allowed, nouns);
  if (char_name_usable (st, c) && c->name != NULL)
    {
      const char *one_name[1];
      one_name[0] = c->name;
      collect_words (c->article, c->prefix, one_name, 1, allowed, nouns);
    }
}

/* Match `text` against an object's effective naming (the singular counterpart
   of name_match_plural below). */
static int
name_match_object (a5_state_t *st, const a5_object_t *o, const std::string &text)
{
  obj_naming n = effective_naming (st, o);
  return name_match (n.article, n.prefix,
                     n.names.empty () ? NULL : &n.names[0],
                     (int) n.names.size (), text);
}

/* Re-order object keys visible-first, then seen, then the rest -- the
   default-pick ordering every object candidate set carries (it is only a hint;
   resolve_refine narrows the set by the Applicable/Visible/Seen tiers). */
static std::vector<std::string>
order_visible_first (a5_state_t *st, const std::vector<std::string> &keys)
{
  std::vector<std::string> vis, seen, rest, out;
  for (auto &k : keys)
    {
      if (obj_visible (st, k.c_str ()))     vis.push_back (k);
      else if (obj_seen_p (st, k.c_str ())) seen.push_back (k);
      else                                  rest.push_back (k);
    }
  out.insert (out.end (), vis.begin (),  vis.end ());
  out.insert (out.end (), seen.begin (), seen.end ());
  out.insert (out.end (), rest.begin (), rest.end ());
  return out;
}

/* All object keys whose names match `text` (the full any-scope match set, as
   clsUserSession.InputMatchesObject builds -- no scope filter), ordered
   visible-first. */
static std::vector<std::string>
resolve_object_candidates (a5_state_t *st, const std::string &text)
{
  std::vector<std::string> matched;
  for (int i = 0; i < st->adv->n_objects; i++)
    if (name_match_object (st, &st->adv->objects[i], text))
      matched.push_back (st->adv->objects[i].key);
  return order_visible_first (st, matched);
}

/* Like name_match, but the candidate nouns are the objects' *plural* forms, so
   a plural reference ("balls"/"boxes"/"knives") matches each object whose noun
   pluralises to it (clsObject.sRegularExpressionString(bPlural:=True), whose arl
   is arlPlurals).  Article + prefix stay as for the singular. */
static int
name_match_plural (a5_state_t *st, const a5_object_t *o, const std::string &text)
{
  obj_naming n = effective_naming (st, o);
  std::vector<std::string> plurals;
  std::vector<const char *> pp;
  for (size_t i = 0; i < n.names.size (); i++)
    plurals.push_back (guess_plural_from_noun (lower (n.names[i])));
  for (auto &s : plurals) pp.push_back (s.c_str ());
  return name_match (n.article, n.prefix,
                     pp.empty () ? NULL : &pp[0], (int) pp.size (), text);
}

/* All character keys whose names match `text`, visible-first (full any-scope
   set, mirroring InputMatchesCharacter). */
static std::vector<std::string>
resolve_character_candidates (a5_state_t *st, const std::string &text)
{
  std::vector<std::string> vis, rest;
  const char *ploc = a5state_player_location (st);
  for (int i = 0; i < st->adv->n_characters; i++)
    {
      const a5_character_t *c = &st->adv->characters[i];
      /* The character's matchable names are its descriptors plus -- only once it
         is Known -- its proper name (clsCharacter.sRegularExpressionString). */
      std::vector<const char *> names;
      for (int d = 0; d < c->n_descriptors; d++)
        names.push_back (c->descriptors[d]);
      if (char_name_usable (st, c) && c->name != NULL)
        names.push_back (c->name);
      if (!name_match (c->article, c->prefix,
                       names.empty () ? NULL : &names[0],
                       (int) names.size (), text))
        continue;
      if (streq (st->char_loc[i], ploc))
        vis.push_back (c->key);
      else
        rest.push_back (c->key);
    }
  vis.insert (vis.end (), rest.begin (), rest.end ());
  return vis;
}

/* PossibleKeys: narrow a candidate list to those entities every word of the
   clarifier input matches (clsUserSession.PossibleKeys).  "the" always matches. */
std::vector<std::string>
possible_keys (a5_state_t *st, const std::vector<std::string> &keys,
               const std::string &input, char type)
{
  std::vector<std::string> out;
  std::vector<std::string> words = split_ws (input.c_str ());
  for (auto &k : keys)
    {
      std::vector<std::string> allowed, nouns;
      if (type == 'o')
        { int oi = a5state_object_index (st, k.c_str ());
          if (oi < 0) continue;
          object_words (st, &st->adv->objects[oi], allowed, nouns); }
      else
        { int ci = a5state_character_index (st, k.c_str ());
          if (ci < 0) continue;
          character_words (st, &st->adv->characters[ci], allowed, nouns); }
      int all = 1;
      for (auto &w : words)
        {
          std::string lw = lower (w);
          int found = (lw == "the");
          for (auto &a : allowed) if (a == lw) { found = 1; break; }
          if (!found) { all = 0; break; }
        }
      if (all) out.push_back (k);
    }
  return out;
}

/*
 * Bind a captured reference under its "Referenced..." aliases (the resolved key)
 * plus a parallel "<alias>$text" carrying the raw typed text (for BeExactText).
 */
void
bind_reference (a5_state_t *st, const char *group, const char *value,
                const char *text)
{
  std::string num;
  std::string base = ref_base (group, &num);

  std::string cap = base;
  if (!cap.empty ()) cap[0] = (char) toupper ((unsigned char) cap[0]);
  /* "objects"/"characters" -> singular "Object"/"Character" stem too */
  std::string stem = cap;
  if (num.empty () && (base == "objects" || base == "characters"))
    stem.pop_back ();

  auto bind = [&](const std::string &alias) {
    a5state_bind_ref (st, alias.c_str (), value);
    if (text != NULL)
      a5state_bind_ref (st, (alias + "$text").c_str (), text);
  };
  /* When the command carries BOTH the plural %objects% and a separate singular
     %object%, the plural's binds must not alias onto ReferencedObject: the runner's
     GetReference resolves ReferencedObject only to the "object1" reference
     (clsUserSession.vb:3990), so a restriction on ReferencedObject keeps
     testing the singular (the container) while the plural items iterate
     (`hide %objects% in %object%`). */
  int suppress = (base == "objects" && st->ref_objects_suppress_singular);
  if (!suppress)
    {
      bind ("Referenced" + stem + num);
      /* The bare "Referenced<Stem>" is the *first* reference (%object% ==
         %object1%); a higher index (%object2%..) must not clobber it -- the runner keeps
         ReferencedObject pinned to ref 1 so 2-specific overrides (e.g.
         PutSomeDry: Gunpowder1 + Keg) resolve per index. */
      if (num.empty () || num == "1") bind ("Referenced" + stem);
      if (num == "1") bind ("Referenced" + stem + "1");
    }
  if (base == "objects")    bind ("ReferencedObjects");
  if (base == "characters") bind ("ReferencedCharacters");

  /* Track whether the *singular* %object%/%object1% (resp. %character%) text
     token should resolve.  In the Adrift 5 runner GetReference("ReferencedObject")
     resolves a reference only when its ReferenceMatch is "object1"
     (clsUserSession.vb:3990); a plural %objects% reference has ReferenceMatch
     "objects", so %object%/%object1% in a message render EMPTY even though the
     plural's first key stays bound for override-key matching and the
     ReferencedObjects restriction path.  E.g. Axe of Kolt's give task R1 message
     "There is nobody here to give %TheObject[%object%]% to!" must render the
     singular as nothing -> "...give nothing to!".  We keep the binding but mark
     the slot plural-derived so a5text suppresses the singular token. */
  if (base == "object" && (num.empty () || num == "1"))
    st->ref_object1_plural = 0;
  else if (base == "objects" && !suppress)
    st->ref_object1_plural = 1;
  if (base == "character" && (num.empty () || num == "1"))
    st->ref_character1_plural = 0;
  else if (base == "characters")
    st->ref_character1_plural = 1;
}

/* The "Which <word>?" noun: the first word of the typed reference text that
   names every candidate (clsUserSession.AmbWord, vb:2656).

   FAITHFUL QUIRK: the runner compares each input word `sWord` to the candidates' whole
   `arlNames` / ProperName / descriptors **case-sensitively** (`sWord = sName`),
   and returns Nothing (rendered "") when no input word is a name of *every*
   candidate.  So `buy ale` -- which matches two objects, one named "Ale" and one
   "ale" -- finds no common case-exact name for the lowercase input word "ale"
   (the "Ale" object fails), so AmbWord is empty and the cantsee renders
   "You can't see any !".  We mirror both: case-sensitive whole-name comparison
   and an empty fallback (NOT the raw text). */
std::string
amb_word (a5_state_t *st, const std::vector<std::string> &keys,
          const std::string &ref_text, char type)
{
  for (auto &w : split_ws (ref_text.c_str ()))
    {
      int in_all = 1;
      for (auto &k : keys)
        {
          int hit = 0;
          if (type == 'o')
            { int oi = a5state_object_index (st, k.c_str ());
              if (oi >= 0)
                for (int n = 0; n < st->adv->objects[oi].n_names && !hit; n++)
                  if (w == st->adv->objects[oi].names[n]) hit = 1; }
          else
            { int ci = a5state_character_index (st, k.c_str ());
              if (ci >= 0)
                { const a5_character_t *c = &st->adv->characters[ci];
                  if (c->name != NULL && w == c->name) hit = 1;
                  for (int d = 0; d < c->n_descriptors && !hit; d++)
                    if (w == c->descriptors[d]) hit = 1; } }
          if (!hit) { in_all = 0; break; }
        }
      if (in_all) return w;
    }
  return "";
}

/* clsObject.GuessPluralFromNoun: a naive English pluraliser (the quirky FRotZ
   one, "feet"->"feets" and all -- ported verbatim so the canned "You can't see
   any <plural>!" message byte-matches the Runner). */
std::string
guess_plural_from_noun (const std::string &n)
{
  if (n.empty ())
    return n;
  static const char *same[] = { "deer","fish","cod","mackerel","trout","moose",
    "sheep","swine","aircraft","blues","cannon" };
  for (const char *s : same) if (n == s) return n;
  if (n == "ox")    return "oxen";
  if (n == "cow")   return "kine";
  if (n == "child") return "children";
  if (n == "foot")  return "feet";
  if (n == "goose") return "geese";
  if (n == "louse") return "lice";
  if (n == "mouse") return "mice";
  if (n == "tooth") return "teeth";
  size_t L = n.size ();
  if (L <= 2)
    return n + "s";
  std::string l3 = n.substr (L - 3), l2 = n.substr (L - 2), l1 = n.substr (L - 1);
  if (l3 == "man") return n.substr (0, L - 2) + "en";
  if (l3 == "ies") return n;
  if (l2 == "sh" || l2 == "ss" || l2 == "ch") return n + "es";
  if (l2 == "ge" || l2 == "se") return n + "s";
  if (l2 == "ex") return n.substr (0, L - 2) + "ices";
  if (l2 == "is") return n.substr (0, L - 2) + "es";
  if (l2 == "um") return n.substr (0, L - 2) + "a";
  if (l2 == "us") return n.substr (0, L - 2) + "i";
  const char *cons = "bcdfghjklmnpqrstvwxz";
  if (l1 == "f")
    {
      if (n == "dwarf" || n == "hoof" || n == "roof") return n + "s";
      return n.substr (0, L - 1) + "ves";
    }
  if (l1 == "o" && strchr (cons, n[L - 2]) != NULL) return n + "es";
  if (l1 == "x") return n + "es";
  if (l1 == "y" && strchr (cons, n[L - 2]) != NULL) return n.substr (0, L - 1) + "ies";
  if (l1 == "s") return n;
  return n + "s";
}

/* Is object `key` in the player's scope this turn (the visibility proxy
   resolve_object_candidates uses)? */
int
obj_in_scope (a5_state_t *st, const char *key)
{
  const char *ploc = a5state_player_location (st);
  int i = a5state_object_index (st, key);
  return ploc != NULL && i >= 0
         && a5state_object_visible_at_location (st, i, ploc, 0);
}

/* Visible/seen predicates the refine tiers use (clsObject.IsVisibleTo /
   SeenBy and clsCharacter.CanSeeCharacter / SeenBy), keyed by entity key. */
int
obj_visible (a5_state_t *st, const char *key) { return obj_in_scope (st, key); }

int
obj_seen_p (a5_state_t *st, const char *key)
{
  int i = a5state_object_index (st, key);
  return i >= 0 && st->obj_seen != NULL && st->obj_seen[i];
}

int
char_visible (a5_state_t *st, const char *key)
{
  /* Through the on/in-object chain (a char seated on furniture has
     char_loc == NULL) -- GFS's Grandpa starts On Object rocking chair. */
  int i = a5state_character_index (st, key);
  return i >= 0
         && a5state_character_at_location (st, i, a5state_player_location (st));
}

int
char_seen_p (a5_state_t *st, const char *key)
{
  int i = a5state_character_index (st, key);
  return i >= 0 && st->char_seen != NULL && st->char_seen[i];
}

/* The same predicates dispatched on the reference type ('o' / 'c'). */
static int
ent_visible (a5_state_t *st, char type, const char *key)
{
  return type == 'o' ? obj_visible (st, key) : char_visible (st, key);
}

static int
ent_seen (a5_state_t *st, char type, const char *key)
{
  return type == 'o' ? obj_seen_p (st, key) : char_seen_p (st, key);
}

/* Is at least one candidate key currently visible?  Gates the "Which X?" prompt
   vs "You can't see any <plural>!" (DisplayAmbiguityQuestion bCanSeeAny). */
static int
any_candidate_visible (a5_state_t *st, const std::vector<std::string> &keys,
                       char type)
{
  for (auto &k : keys)
    if (ent_visible (st, type, k.c_str ()))
      return 1;
  return 0;
}

/* Record an unresolved-reference ambiguity in *amb (when requested) and return
   the verdict the caller propagates: RR_AMBIG when at least one candidate is
   currently visible ("Which X?"), else RR_CANTSEE ("You can't see any ...!").
   `second_chance` flags an ambiguity found only via the runner's existence
   (second-chance) pass, which loses to a clean no-reference fallback. */
static int
set_amb_result (a5_state_t *st, amb_info *amb, const std::string &name,
                char type, const std::string &text,
                const std::vector<std::string> &keys, int second_chance)
{
  if (amb != NULL)
    {
      amb->ref_name = name;
      amb->type = type;
      amb->ref_text = text;
      amb->keys = keys;
      amb->second_chance = second_chance;
    }
  return any_candidate_visible (st, keys, type) ? RR_AMBIG : RR_CANTSEE;
}

/* -------------------------------------------- multiple-object references */

/*
 * InputMatchesObject (clsUserSession.vb:5378): every object whose name matches
 * `input` is recorded.  For a singular reference the matches accumulate into one
 * item (a candidate set, disambiguated later); for a *plural* reference each
 * matching object becomes its own item (so "take balls" grabs every ball).  In
 * the `excepts` pass the matching keys are removed from the items instead.
 * `items` is the per-item candidate-key list being built; returns true if any
 * object matched.
 */
static bool
match_object_one (a5_state_t *st, const std::string &input,
                  std::vector<std::vector<std::string>> &items,
                  int item_num, bool excepts, bool plural)
{
  bool result = false, added = false;
  if (item_num == 0 && plural)
    item_num = -1;
  for (int i = 0; i < st->adv->n_objects; i++)
    {
      const a5_object_t *o = &st->adv->objects[i];
      int hit = plural ? name_match_plural (st, o, input)
                       : name_match_object (st, o, input);
      if (!hit)
        continue;
      result = true;
      if (!excepts)
        {
          if (plural) item_num++;
          if (plural || !added)
            { if ((int) items.size () <= item_num) items.resize (item_num + 1); }
          added = true;
          if (item_num >= 0 && item_num < (int) items.size ())
            items[item_num].push_back (o->key);
        }
      else
        {
          for (auto &itm : items)
            itm.erase (std::remove (itm.begin (), itm.end (),
                                    std::string (o->key)), itm.end ());
        }
    }
  if (excepts)
    items.erase (std::remove_if (items.begin (), items.end (),
                  [] (const std::vector<std::string> &v) { return v.empty (); }),
                 items.end ());
  return result;
}

/* Every object the player has seen (clsObject.SeenBy / htblObjects.SeenBy) ->
   one single-candidate item each, in model order; the source of a bare "all". */
static void
expand_all_objects (a5_state_t *st, std::vector<std::vector<std::string>> &items)
{
  for (int i = 0; i < st->adv->n_objects; i++)
    if (st->obj_seen != NULL && st->obj_seen[i])
      items.push_back (std::vector<std::string> (1, st->adv->objects[i].key));
}

/*
 * InputMatchesObjects (clsUserSession.vb:5305): parse the plural-reference
 * grammar into per-item candidate sets -- "all" / "all <plural>",
 * "X and Y" / comma lists, and a trailing "... except/but/apart from ...".
 * `had_all` is set when a bare/refined "all" appeared (drives the FailOverride).
 * `hard_fail` is set when the input parsed as an explicit "X and Y" / comma
 * multi-reference but a *chunk* named no object: the Adrift 5 runner's and-form path
 * (`If Not InputMatchesObject(...) Then Return False`, vb:5371) returns False
 * outright and -- unlike a single-noun no-match -- does NOT fall through to the
 * second-chance `HasObjectExistRestriction` fallback, so the task does not match
 * at all (-> "Sorry, I didn't understand that command.").
 * Returns true if the text resolved to at least one item.
 */
static bool
match_objects (a5_state_t *st, const std::string &input_raw,
               std::vector<std::vector<std::string>> &items,
               bool excepts, bool plural, int *had_all, int *hard_fail = NULL)
{
  std::string input = trim_ws (input_raw);
  if (input.empty ())
    return false;

  if (!plural)
    {
      /* Split off a trailing "... except|but|apart from <objects2>" (the regex
         is RightToLeft, so the *last* connector wins). */
      std::string head = input, excepts_text;
      static const char *conns[] = { " except ", " but ", " apart from ", NULL };
      std::string lin = lower (input);
      size_t best = std::string::npos, best_len = 0;
      for (int i = 0; conns[i]; i++)
        {
          size_t p = lin.rfind (conns[i]);
          if (p != std::string::npos && (best == std::string::npos || p > best))
            { best = p; best_len = strlen (conns[i]); }
        }
      if (best != std::string::npos)
        { head = trim_ws (input.substr (0, best));
          excepts_text = trim_ws (input.substr (best + best_len)); }

      std::string lhead = lower (head);
      bool head_ok = true;
      if (lhead == "all")
        { if (had_all) *had_all = 1; expand_all_objects (st, items); }
      else if (lhead.rfind ("all ", 0) == 0)
        { if (had_all) *had_all = 1;
          if (!match_objects (st, head.substr (4), items, false, true, had_all))
            head_ok = false; }                         /* GoTo NextCheck */
      else if (!head.empty ())
        { if (!match_objects (st, head, items, excepts, true, had_all))
            head_ok = false; }                         /* GoTo NextCheck */

      if (head_ok)
        {
          if (!excepts_text.empty ())
            match_objects (st, excepts_text, items, true, false, had_all);
          return true;
        }

      /* NextCheck: "(<csv>, )* <o2> and <o3>". */
      size_t andp = lin.rfind (" and ");
      if (andp != std::string::npos)
        {
          std::string left = trim_ws (input.substr (0, andp));
          std::string o3   = trim_ws (input.substr (andp + 5));
          int item_num = 0;
          /* split `left` on ", " into csv items + o2 */
          size_t start = 0;
          while (start <= left.size ())
            {
              size_t comma = left.find (", ", start);
              std::string tok = trim_ws (comma == std::string::npos
                                  ? left.substr (start)
                                  : left.substr (start, comma - start));
              if (!tok.empty ()
                  && !match_object_one (st, tok, items, item_num++, excepts, false))
                { if (hard_fail) *hard_fail = 1; return false; }
              if (comma == std::string::npos) break;
              start = comma + 2;
            }
          if (!match_object_one (st, o3, items, item_num++, excepts, false))
            { if (hard_fail) *hard_fail = 1; return false; }
          return true;
        }
    }

  return match_object_one (st, input, items, 0, excepts, plural);
}

/*
 * Resolve a matched command's references in scope, refining ambiguous
 * object/character references by the task's own restrictions, mirroring
 * clsUserSession.RefineMatchingPossibilitesUsingRestrictions + the post-refine
 * ambiguity check (GetGeneralTask).  Each reference is bound (first surviving
 * candidate) so restriction/action evaluation can read it.
 *
 *   RR_NOMATCH  a non-object/character reference failed to parse -> next task
 *   RR_OK       every reference resolved uniquely and restrictions pass
 *   RR_FAIL     restrictions fail for the (uniquely) resolved references
 *   RR_AMBIG    a reference still has >1 candidate, >=1 visible -> *amb (prompt)
 *   RR_CANTSEE  a reference still has >1 candidate, none visible -> *amb
 *   RR_NOREF    an object/character reference named nothing -> *amb (sNoRefTask:
 *               run the task with the reference unresolved so a `Must Exist`
 *               restriction surfaces "Sorry, I'm not sure which object ...")
 *
 * Candidates begin as the full any-scope name-match set (InputMatchesObject),
 * then a three-tier refine (Applicable / Visible / Seen), each tier resetting a
 * reference to its full original set when it empties -- a port of
 * clsUserSession.RefineMatchingPossibilitesUsingRestrictions + the post-refine
 * count check (GetGeneralTask / DisplayAmbiguityQuestion).
 *
 * `force_name`/`force_key`: when re-running a remembered task to resolve a
 * pending ambiguity, pin that reference to the chosen key.
 */

/* Pick the key that represents an item: the first currently-visible candidate,
   else the first seen, else simply the first (the visible-first default pick
   used throughout resolve_refine). */
static std::string
pick_item_key (a5_state_t *st, const std::vector<std::string> &cands)
{
  for (auto &k : cands) if (obj_visible (st, k.c_str ())) return k;
  for (auto &k : cands) if (obj_seen_p (st, k.c_str ())) return k;
  return cands.empty () ? std::string () : cands[0];
}

/*
 * Resolve the plural %objects% reference (the only reference whose base is
 * "objects") to a concrete item list -- InputMatchesObjects + the single-
 * reference case of RefineMatchingPossibilitesUsingRestrictions.  Parse the
 * all/and/comma/except grammar, then keep the items whose chosen key passes the
 * task's restrictions (with the other references already bound); if at least one
 * passes those are the items, else reset to the full list (so the task runs and
 * fails) and, when the command contained "all", arm the FailOverride.
 *
 * Stores the resolved keys in st->ref_items and binds ReferencedObject = the
 * first item and ReferencedObjects = the "key1|key2|..." pipe list (so the OO /
 * text engine renders the whole set, and the per-item action loop iterates).
 * Returns RR_OK (>=1 passing/forced item), RR_FAIL (items exist, none pass), or
 * RR_NOMATCH (the text named no object at all).
 */
int
resolve_plural (a5_run_t *run, const a5_task_t *t, const std::string &text,
                amb_info *amb)
{
  a5_state_t *st = run->st;
  std::vector<std::vector<std::string>> items;
  int had_all = 0;

  if (!match_objects (st, text, items, false, false, &had_all))
    return RR_NOMATCH;

  /* Per-item ambiguity (clsUserSession.RefineMatchingPossibilitesUsingRestrictions
     + GetGeneralTask).  A single noun (no "and"/comma) that name-matches several
     objects collapses into one Item holding >1 MatchingPossibility -- e.g. the
     "leathers" chunk of `wear leathers and boots` matches two "riding leathers"
     objects, so its Item has Count 2.  (The bare-plural path -- "all", "all
     <plural>", or a plural-noun match -- instead spreads each object into its own
     single-possibility Item, so it is never ambiguous.)  the runner refines every Item
     through the Applicable/Visible/Seen tiers, resetting the whole reference to
     its original Items whenever a tier empties *all* of them; an Item that is
     never reduced to a unique key stays Count>1, and GetGeneralTask then raises
     it as sAmbTask (DisplayAmbiguityQuestion -> "Which <noun>?" if any candidate
     is visible, else "You can't see any <plural>!").  So mirror the tiered refine
     over the per-item candidate sets and, if an Item survives ambiguous, surface
     it here -- the resolved/none-passed machinery below only runs when every Item
     is unique. */
  std::vector<std::vector<std::string>> cur = items;
  {
    /* Applicable: keep, per Item, the candidates that pass the restrictions with
       that single key bound (a key already kept in an earlier Item is not added
       again -- the runner's global lAdded).  Items with no surviving candidate are
       dropped; if *every* Item is dropped, the whole reference resets to its
       original Items (so a Count>1 Item is preserved).

       The per-candidate probe binds an EMPTY typed text: the runner's refine builds a
       fresh single-key clsSingleItem whose sCommandReference is never set
       (clsUserSession.vb:5766 itmSingle0), so a `BeExactText` restriction always
       evaluates False here -- `MustNot BeExactText All` PASSES during the refine
       and only fails at the post-refine task evaluation, where the surviving
       item's original "all" command reference is visible again.  That two-phase
       dance is what keeps the library's RemoveBeforeDrop helper alive through
       the refine on `drop all` (its worn/contained items survive) yet failing
       silently afterwards (restriction 5 has no message), so the scan falls
       through to the real DropObjects task (ThingsThatGoBumpInTheNight). */
    {
      std::vector<std::vector<std::string>> nr;
      std::vector<std::string> added;
      for (auto &item : cur)
        {
          std::vector<std::string> out;
          for (auto &k : item)
            {
              bind_reference (st, "objects", k.c_str (), "");
              if (a5restr_pass (st, t->restrictions)
                  && std::find (added.begin (), added.end (), k) == added.end ())
                { out.push_back (k); added.push_back (k); }
            }
          if (!out.empty ()) nr.push_back (out);
        }
      cur = nr.empty () ? items : nr;
    }

    /* Visible, then Seen: drop non-visible / non-seen candidates per Item; reset
       to the original Items whenever a tier empties them all. */
    for (int tier = 0; tier < 2; tier++)
      {
        std::vector<std::vector<std::string>> nr;
        for (auto &item : cur)
          {
            std::vector<std::string> out;
            for (auto &k : item)
              if (tier == 0 ? obj_visible (st, k.c_str ())
                            : obj_seen_p  (st, k.c_str ()))
                out.push_back (k);
            if (!out.empty ()) nr.push_back (out);
          }
        cur = nr.empty () ? items : nr;
      }

    /* The first Item still holding >1 candidate is the ambiguity (the runner scans Items
       in order; GetGeneralTask sets sAmbTask for the first Count>1). */
    for (auto &item : cur)
      if (item.size () > 1)
        return set_amb_result (st, amb, "objects", 'o', text, item, 0);
  }

  /* Choose one key per item; keep the items whose key passes the restrictions.
     Iterate the REFINED set (the runner's NewReferencesWorking after the tiered refine),
     not the original parse: when the Applicable tier kept a subset, the runner's final
     PassRestrictions only ever sees those survivors.  For a task without
     BeExactText this is the same set the restrictions re-derive from the
     original items (the probes are identical), so nothing moves; it only
     matters when the refine's empty-typed-text probe diverges from the final
     evaluation (RemoveBeforeDrop's `MustNot BeExactText All` above). */
  std::vector<std::string> chosen, all_keys;
  for (auto &cand : cur)
    {
      std::string pick = pick_item_key (st, cand);
      if (pick.empty ()) continue;
      all_keys.push_back (pick);
      bind_reference (st, "objects", pick.c_str (), text.c_str ());
      if (a5restr_pass (st, t->restrictions))
        chosen.push_back (pick);
    }

  if (all_keys.empty ())
    {
      if (had_all && t->fail_override != NULL)
        { run->pending_failover = t->fail_override; return RR_FAIL; }
      return RR_NOMATCH;
    }

  int is_list = text.find (" and ") != std::string::npos
                || text.find (',') != std::string::npos;
  std::vector<std::string> vis, seen;
  for (auto &k : all_keys)
    {
      if (obj_visible (st, k.c_str ()))  vis.push_back (k);
      if (obj_seen_p  (st, k.c_str ()))  seen.push_back (k);
    }
  int none_passed = chosen.empty ();
  if (none_passed && items.size () > 1 && !had_all && !is_list
      && !seen.empty () && vis.empty ())
    {
      /* A single NOUN (not "all", not an explicit "X and Y" list) that name-
         matched several objects, none of which pass the restrictions, is
         ambiguous: the Adrift 5 runner prefixes the task's failure with its
         "ReferencedObject(s) Must Exist" message ("Sorry, I'm not sure which
         object you are trying to take.").  The count is the ORIGINAL match set
         (a plural noun like "keys" that matches ten key objects but narrows to
         one still shows the prefix).  When the failing restriction is itself the
         Exist / HaveBeenSeen one (a never-seen match), that message already IS
         the failure text, so the RR_FAIL consumer suppresses the duplicate; it
         only prepends when a *later* restriction (not visible / not held)
         supplies a different message. */
      const a5_xml_node_t *em = a5restr_exist_message (t->restrictions);
      if (em != NULL)
        {
          char *p = a5text_describe (st, em);
          run->plural_amb_prefix = (p != NULL) ? p : "";
          free (p);
        }
    }
  if (none_passed)
    {
      /* No item passed.  A "get all"-style command shows the FailOverride.
         Otherwise the task runs with the whole (reset) reference set and its
         restrictions decide the message -- a genuine plural %objects% reference
         is NEVER an out-of-scope "You can't see any <plural>!" ambiguity: the runner's
         InputMatchesObject spreads each matching object into its own Item with a
         single MatchingPossibility (clsUserSession.vb:5387-5391), so no Item ever
         holds >1 possibility and DisplayAmbiguityQuestion (which needs Count>1)
         is unreachable from a plural.  Instead GetGeneralTask runs the task and
         PassRestrictions surfaces the first failing restriction's message --
         e.g. Axe of Kolt's `give planks` (nobody present) -> the give task's
         "There is nobody here to give nothing to!", `take seeds` (never seen) ->
         "Sorry, I'm not sure which object you are trying to take.".  (The "You
         can't see any <plural>!" message proper comes only from a *singular*
         %object% reference whose one Item name-matched >1 objects -- the
         resolve_refine RR_CANTSEE path, e.g. `press button`.)

         The reset is the runner's tiered fallback, not a blunt reset to the full set:
         when the Applicable tier empties the whole reference, the runner resets to the
         full set and refines by Visible (each emptied single-possibility item is
         dropped, clsUserSession.vb:5848-5912); only if Visible empties *every*
         item does it reset again and refine by Seen; only if Seen empties
         everything too does the full set survive (vb:5914-5959).  So the
         surviving set is the Visible subset, else the Seen subset, else all --
         which is why `show documents` (travel documents in scope, a second
         out-of-scope "documents" part of distant cabinets) renders only the
         visible "travel documents" in the "take ... out of whatever it is in
         first" message, not both. */
      if (!vis.empty ())       chosen = vis;
      else if (!seen.empty ()) chosen = seen;
      else                     chosen = all_keys;
    }

  /* De-duplicate, preserving order (a noun and "all" may name an object twice). */
  std::vector<std::string> uniq;
  for (auto &k : chosen)
    if (std::find (uniq.begin (), uniq.end (), k) == uniq.end ())
      uniq.push_back (k);

  st->n_ref_items = 0;
  st->ref_items_type = 'o';
  std::string pipe;
  for (auto &k : uniq)
    {
      const a5_object_t *o = a5model_object (st->adv, k.c_str ());
      if (o == NULL) continue;                 /* stable model-key pointer */
      if (st->n_ref_items < A5_MAX_ITEMS)
        st->ref_items[st->n_ref_items++] = o->key;
      if (!pipe.empty ()) pipe += "|";
      pipe += o->key;
    }
  if (st->n_ref_items == 0)
    return RR_NOMATCH;

  bind_reference (st, "objects", st->ref_items[0], text.c_str ());
  a5state_bind_ref (st, "ReferencedObjects", pipe.c_str ());

  if (none_passed)
    {
      if (had_all && t->fail_override != NULL)
        run->pending_failover = t->fail_override;
      return RR_FAIL;
    }
  return RR_OK;
}

int
resolve_refine (a5_run_t *run, const a5_task_t *t, const a5_match_t *m,
                const char *force_name, const char *force_key, amb_info *amb)
{
  a5_state_t *st = run->st;
  struct rref { std::string name, text; char type;
                std::vector<std::string> orig, keys; };
  std::vector<rref> refs;
  int have_noref = 0;
  rref noref_r;
  int plural_idx = -1;
  std::string plural_text;

  /* A reference that names no entity at all.  A task only "matches" such input
     if it has a `Must Exist` restriction of the reference's type
     (clsUserSession's second-chance pass, gated on HasObjectExistRestriction /
     HasCharacterExistRestriction); then the first such reference is remembered
     as the no-reference fallback (sNoRefTask) and left unbound so the caller
     can surface its Must-Exist message.  Returns false -- the command does not
     match this task at all -- when the restriction is missing. */
  auto note_noref = [&] (const rref &r) {
    if (!have_noref)
      {
        if (!a5restr_has_exist (t->restrictions, r.type))
          return false;
        have_noref = 1; noref_r = r;
      }
    return true;
  };
  /* Report the recorded no-reference fallback to the caller (empty amb->keys
     marks it as a no-reference, not an ambiguity). */
  auto noref_result = [&] () {
    if (amb != NULL)
      { amb->ref_name = noref_r.name; amb->type = noref_r.type;
        amb->ref_text = noref_r.text; amb->keys.clear (); }
    return RR_NOREF;
  };

  a5state_clear_refs (st);
  run->pending_failover = NULL;
  run->plural_amb_prefix.clear ();

  /* Pass 1: gather full any-scope candidate sets; bind the default of each. */
  for (int i = 0; i < m->n_refs; i++)
    {
      rref r;
      r.name = m->ref_name[i];
      r.text = m->ref_text[i];
      std::string base = ref_base (r.name);

      /* The plural %objects% reference.  Only a *genuine* multiple-object input
         ("all", "X and Y", a comma list, or a plural noun matching several
         objects) is deferred to resolve_plural; a single-object input is bound
         here as an ordinary object reference so the existing refine / ambiguity
         / "can't see any" / no-ref semantics are byte-identical to a bare
         %object% (clsUserSession.InputMatchesObjects yields one item in that
         case, which RefineMatchingPossibilitesUsingRestrictions then treats like
         any single reference). */
      if (base == "objects")
        {
          std::vector<std::vector<std::string>> items;
          int had_all = 0, hard_fail = 0;
          bool ok = match_objects (st, r.text, items, false, false, &had_all,
                                   &hard_fail);
          /* An explicit "X and Y" / comma list one of whose chunks named no
             object: the Adrift 5 runner returns False without the second-chance
             existence fallback, so the command does not match this task at all
             (no sNoRefTask) -- e.g. `get fleetwind saddle and fleetwind bridle`,
             where neither chunk names an object, is "I didn't understand", not
             the take task's "...trying to take." no-reference message. */
          if (hard_fail)
            return RR_NOMATCH;
          /* A genuine multi-object input *or* any "all" command (even one that
             expands to zero/one seen object) goes through resolve_plural, so a
             "get all" with nothing takeable surfaces the task's FailOverride
             ("There is nothing worth taking here.") rather than the single-ref
             no-reference message. */
          if (ok && (items.size () > 1 || had_all))
            {
              plural_idx = i; plural_text = r.text;
              /* Does this command ALSO carry a singular %object%/%object1% (or
                 %item%) reference?  Then the plural's per-item binds must not
                 alias onto ReferencedObject (see bind_reference) -- the runner keeps the
                 two slots distinct, so `hide %objects% in %object%` restrictions
                 on ReferencedObject test the container throughout the item
                 iteration. */
              for (int j = 0; j < m->n_refs; j++)
                {
                  if (j == i) continue;
                  std::string b2 = ref_base (m->ref_name[j]);
                  if ((b2 == "object" || b2 == "item")
                      && (m->ref_name[j] == b2 || m->ref_name[j] == b2 + "1"))
                    { st->ref_objects_suppress_singular = 1; break; }
                }
              continue;                                           /* multi/all */
            }
          r.type = 'o';
          r.orig = (ok && items.size () == 1)
            ? order_visible_first (st, items[0])
            : resolve_object_candidates (st, r.text);
          if (r.orig.empty ())
            {
              if (!note_noref (r))
                return RR_NOMATCH;
              continue;
            }
          r.keys = r.orig;
          bind_reference (st, r.name.c_str (), r.keys[0].c_str (),
                          r.text.c_str ());
          refs.push_back (r);
          continue;
        }

      if (base == "object" || base == "item")
        r.type = 'o';
      else if (base == "character" || base == "characters")
        r.type = 'c';
      else if (base == "direction")
        { const char *d = a5parse_canonical_direction (r.text.c_str ());
          if (d == NULL) return RR_NOMATCH;
          bind_reference (st, r.name.c_str (), d, r.text.c_str ());
          continue; }
      else /* text, number, location */
        { bind_reference (st, r.name.c_str (), r.text.c_str (), r.text.c_str ());
          continue; }

      if (force_name != NULL && r.name == force_name)
        { r.orig.assign (1, force_key); r.keys = r.orig; }
      else
        {
          r.orig = (r.type == 'o')
            ? resolve_object_candidates (st, r.text)
            : resolve_character_candidates (st, r.text);
          if (r.orig.empty ())
            {
              if (!note_noref (r))
                return RR_NOMATCH;
              continue;
            }
          r.keys = r.orig;
        }
      bind_reference (st, r.name.c_str (), r.keys[0].c_str (), r.text.c_str ());
      refs.push_back (r);
    }

  /* A reference matched nothing -> the whole task is deferred as the no-reference
     fallback (GetGeneralTask sNoRefTask).  But the unmatched reference does NOT
     preempt the *other* references' ambiguity resolution: in the Adrift 5 runner an
     unmatched-but-Must-Exist reference is a second-chance match that adds a
     zero-Item NewReference (InputMatchesObject returns True via
     HasObjectExistRestriction but appends no Item), so the post-refine loop simply
     skips it while a *sibling* reference that is ambiguous-and-invisible still sets
     sAmbTask and wins ("You can't see any oil!" for `pour oil on me`, where object1
     "oil" is out-of-scope-ambiguous and object2 "me" names no object).  So fall
     through to the refine + ambiguity pass on the matched refs and only return the
     no-reference fallback below if none of them is ambiguous. */

  /* Pass 2: three-tier refinement (Applicable / Visible / Seen).  Each tier that
     empties a reference resets it to the full original set; a tier that leaves
     >1 falls through to the next.  Single-candidate references are already
     unique (clsUserSession's reset-on-empty makes refining them a no-op). */
  for (auto &r : refs)
    {
      if (r.keys.size () <= 1)
        continue;

      /* Tier 1: Applicable -- the candidates that pass the task's restrictions.
         The Adrift 5 runner evaluates candidates in *model order* (the order
         InputMatchesObject/Character add them to MatchingPossibilities), and the
         last PassRestrictions call leaves sRestrictionText set to its deciding
         restriction's Message (st->restriction_text).  That carried text is what
         a later malformed-bracket task displays, so iterate in model order here
         even though the surviving set is kept in Scarier's visible-first order
         (the default-pick hint) -- only the *last evaluated* candidate, hence
         st->restriction_text, depends on this. */
      std::vector<std::string> eval_order = r.keys;
      std::stable_sort (eval_order.begin (), eval_order.end (),
        [&] (const std::string &a, const std::string &b) {
          int ia = (r.type == 'o') ? a5state_object_index (st, a.c_str ())
                                   : a5state_character_index (st, a.c_str ());
          int ib = (r.type == 'o') ? a5state_object_index (st, b.c_str ())
                                   : a5state_character_index (st, b.c_str ());
          return ia < ib;
        });
      std::vector<std::string> passset;
      for (auto &k : eval_order)
        {
          bind_reference (st, r.name.c_str (), k.c_str (), r.text.c_str ());
          if (a5restr_pass (st, t->restrictions))
            passset.push_back (k);
        }
      std::vector<std::string> pass;            /* survivors, visible-first */
      for (auto &k : r.keys)
        if (std::find (passset.begin (), passset.end (), k) != passset.end ())
          pass.push_back (k);
      int more;
      if (pass.empty ())          { r.keys = r.orig; more = 1; }
      else if (pass.size () > 1)  { r.keys = pass;   more = 1; }
      else                        { r.keys = pass;   more = 0; }

      /* Tiers 2 and 3: Visible, then Seen. */
      for (int tier = 0; more && tier < 2; tier++)
        {
          std::vector<std::string> keep;
          for (auto &k : r.keys)
            if (tier == 0 ? ent_visible (st, r.type, k.c_str ())
                          : ent_seen (st, r.type, k.c_str ()))
              keep.push_back (k);
          if (keep.empty ())
            r.keys = r.orig;
          else
            { r.keys = keep; more = keep.size () > 1; }
        }

      bind_reference (st, r.name.c_str (), r.keys[0].c_str (), r.text.c_str ());
    }

  /* Is the unmatched reference genuinely *required* by this task -- i.e. is its
     `Must Exist` restriction actually evaluated within the BracketSequence?  If so
     the no-reference fallback wins outright (the runner surfaces the task's Must-Exist
     message, e.g. `blow dart at <absent>` -> "Sorry, I'm not sure which object you
     are trying to blow.", `hang amulet on <absent>` -> "...trying to hang...").  If
     the Must Exist is truncated out of the bracket, the reference is optional and
     does NOT block the task, so a *sibling* reference's out-of-scope ambiguity
     surfaces instead (`pour oil on me`, whose pour task's `#A#A#` omits object2's
     Exist -> "You can't see any oil!"). */
  int noref_required = 0;
  if (have_noref)
    {
      std::string num;
      ref_base (noref_r.name, &num);          /* the "...N" index, if any */
      std::string alias = std::string ("Referenced")
        + (noref_r.type == 'c' ? "Character" : "Object") + num;
      noref_required =
        a5restr_exist_evaluated (t->restrictions, alias.c_str ());
    }
  if (noref_required)
    {
      /* The task has an unmatched but *required* reference, so in the Adrift 5 runner
         it never matches in the first pass; it is found only in the
         second-chance (existence) pass.  If a *sibling* reference is itself
         ambiguous (>1 candidate), the runner's second-chance pass sets sAmbTask for
         this task but a different task's clean 0-item Must-Exist failure
         (GetGeneralTask) wins.  So surface the sibling ambiguity flagged as
         `second_chance` -- the caller yields it to a clean no-reference
         fallback (e.g. `tie wire to girder`: TieObjectT's "wire" is out-of-
         scope-ambiguous and "girder" is the required unmatched ref, so the
         simpler `[tie/bind/secure] %object%` task's "...trying to
         tie/bind/secure." wins over TieObjectT's "...trying to tie.").  When
         every sibling resolves uniquely, the required no-reference fallback
         still wins outright (`blow dart at <absent>` -> "...trying to
         blow."). */
      for (auto &r : refs)
        if (r.keys.size () > 1)
          return set_amb_result (st, amb, r.name, r.type, r.text, r.keys, 1);
      return noref_result ();
    }

  /* Post-refine: a reference left with >1 candidate is an ambiguity.  The Runner
     prompts "Which X?" when at least one candidate is currently visible, else it
     answers "You can't see any <plural>!" (DisplayAmbiguityQuestion bCanSeeAny).
     This precedes the (non-required) no-reference fallback below: a sibling
     reference's ambiguity (sAmbTask) wins over an unmatched optional reference. */
  for (auto &r : refs)
    if (r.keys.size () > 1)
      /* If this task ALSO had a reference that named nothing, it matched only via
         the Adrift 5 runner's second-chance (existence) pass -- so its ambiguity is a
         second-chance sAmbTask, which a *different* task's clean no-reference /
         failing-with-output result (GetGeneralTask) beats.  `remove uniform from
         dummy`: TakeFromCh1 (`%objects% from %character%`) is ambiguous on
         "uniform" but its "dummy" names no character, so RemoveObjects'
         `[remove/take off] %objects%` no-reference message ("...you're referring
         to.") wins over the cantsee.  A pure first-pass ambiguity (no unmatched
         ref) stays second_chance=0 and preempts any no-reference fallback. */
      return set_amb_result (st, amb, r.name, r.type, r.text, r.keys,
                             have_noref);

  /* No sibling reference was ambiguous: honour the deferred (optional)
     no-reference fallback (GetGeneralTask sNoRefTask). */
  if (have_noref)
    return noref_result ();

  /* Bind the resolved singular references, then resolve the deferred plural
     %objects% reference against them. */
  for (auto &r : refs)
    bind_reference (st, r.name.c_str (), r.keys[0].c_str (), r.text.c_str ());

  if (plural_idx >= 0)
    {
      int pr = resolve_plural (run, t, plural_text, amb);
      if (pr == RR_NOMATCH)
        {
          /* The plural reference named no object (e.g. "take all" with nothing
             in scope).  As with a singular reference, the task only matches if
             it has an Object `Must Exist` restriction (so it can surface that
             message); otherwise the command does not match this task. */
          if (!a5restr_has_exist (t->restrictions, 'o'))
            return RR_NOMATCH;
          /* Treat like sNoRefTask: defer so a later task can claim. */
          noref_r.name = "objects"; noref_r.type = 'o';
          noref_r.text = plural_text;
          return noref_result ();
        }
      if (pr != RR_OK)
        return pr;                  /* RR_FAIL / RR_CANTSEE (amb already set) */
    }

  /* Unique references: confirm the whole restriction set passes. */
  return a5restr_pass (st, t->restrictions) ? RR_OK : RR_FAIL;
}
