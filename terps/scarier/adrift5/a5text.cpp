/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- text engine (Phase 2 MVP).  See a5text.h.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <algorithm>
#include <string>
#include <vector>

#include "a5expr.h"
#include "a5restr.h"
#include "a5sb.h"     /* sb_t growable string builder (shared with a5run_*) */
#include "a5sexpr.h"
#include "a5text.h"
#include "a5util.h"

#include "../common_utils/sc_garglk.h"

/* ----------------------------------------------------------- small helpers */

static int
ci_eq (const char *a, const char *b)
{
  if (a == NULL || b == NULL)
    return 0;
  for (; *a && *b; a++, b++)
    if (tolower ((unsigned char) *a) != tolower ((unsigned char) *b))
      return 0;
  return *a == *b;
}

/* Does s contain an OO property expression "X.Prop..."?  the runner's AddSpace regex
   `.*?(%?[A-Za-z][\w\|_-]*%?)(\.%?[A-Za-z][\w\|_-]*%?(\([A-Za-z ,_]+?\))?)+`
   is an UNANCHORED IsMatch -- a contains test, not an ends-with test -- and
   spaces are only legal inside a parenthesised argument list, never in the
   identifiers themselves.  A match reduces to: some '.' whose left neighbour
   run `[\w|-]*%?` (walked over identifier chars, one optional '%' adjacent to
   the dot) contains a letter, and whose right side is `%?[A-Za-z]`.  (The old
   backward walk here allowed spaces in the run, so a plain sentence tail like
   "...has its lid closed" walked back through whole sentences to an earlier
   full stop and matched -- adding a two-space join where the runner adds none:
   DieFeuerfaust's chieftain's-tent chest.) */
static int
contains_oo_expr (const char *s, size_t len)
{
  size_t i;
  for (i = 1; i + 1 < len; i++)
    {
      size_t j, k;
      int saw_alpha = 0;
      if (s[i] != '.')
        continue;
      /* right side: optional '%', then a letter */
      k = i + 1;
      if (s[k] == '%' && k + 1 < len)
        k++;
      if (!isalpha ((unsigned char) s[k]))
        continue;
      /* left side: optional '%' adjacent to the dot, then identifier chars
         [A-Za-z0-9_|-]; the run must contain a letter */
      j = i;
      if (s[j - 1] == '%' && j > 1)
        j--;
      while (j > 0)
        {
          char c = s[j - 1];
          if (isalnum ((unsigned char) c) || c == '_' || c == '|' || c == '-')
            {
              if (isalpha ((unsigned char) c))
                saw_alpha = 1;
              j--;
              continue;
            }
          break;
        }
      if (saw_alpha)
        return 1;
    }
  return 0;
}

/* Does s end with text that warrants a double space before the next segment? */
static int
add_space (const char *s, size_t len)
{
  char c;
  if (len == 0)
    return 0;
  c = s[len - 1];
  if (c == ' ' || c == '\n')
    return 0;
  if (c == '.' || c == '!' || c == '?' || c == '%')
    return 1;
  /* an OO expression anywhere in the text (the runner's unanchored regex IsMatch,
     Global.vb:3873) -- it will expand at display time. */
  return contains_oo_expr (s, len);
}

/* ----------------------------------------------------- description -> source */

/* Process every <Description> child of `wrapper` into the running buffer `sb`,
   threading clsDescription.ToString's `first`/`default_text` state so several
   wrappers can be concatenated into one logical Description (the runner's
   oShortDesc.Copy + appended group-property SingleDescriptions, clsLocation.vb:62).
   Returns 1 when a DisplayOnce segment terminated the description (no later
   wrapper should be considered), else 0. */
static int
eval_desc_into (a5_state_t *st, sb_t *psb, int *pfirst, const char **pdefault,
                const a5_xml_node_t *wrapper)
{
  sb_t sb = *psb;
  const a5_xml_node_t *c;
  const char *default_text = *pdefault;
  int first = *pfirst;
  int terminated = 0;

  if (wrapper == NULL)
    { *psb = sb; return 0; }

  for (c = wrapper->first_child; c != NULL; c = c->next)
    {
      const a5_xml_node_t *restr;
      const char *when, *text;
      int once;
      if (strcmp (c->name, "Description") != 0)
        continue;
      text = a5xml_child_text (c, "Text");
      if (text == NULL)
        text = "";
      if (first)
        default_text = text;

      /* A <DisplayOnce> segment is suppressed once it has been shown (the start
         room's "soft rain" line in Stone of Wisdom shows at game start, then not
         on a later LOOK) -- clsDescription.ToString: "If Not sd.DisplayOnce
         OrElse Not sd.Displayed". */
      /* FileIO.GetBool semantics: RtC serialises <DisplayOnce>True</DisplayOnce>,
         not "1", so a literal "1" compare missed it and the first-visit segment
         never short-circuited (later StartDescriptionWithThis segments overrode
         it).  Route through a5xml_bool like every other model boolean. */
      once = a5xml_bool (a5xml_child_text (c, "DisplayOnce"));
      if (once && a5state_disp_once_seen (st, c))
        {
          first = 0;
          continue;
        }

      restr = a5xml_child (c, "Restrictions");
      if (restr != NULL && !a5restr_pass (st, restr))
        {
          first = 0;
          /* clsDescription.ToString does `Return sb.ToString` inside
             `If .DisplayOnce` *regardless* of whether the restrictions passed
             (it only sets Displayed=True when they did).  So a not-yet-shown
             DisplayOnce segment whose restrictions FAIL still terminates the
             loop -- later segments are not considered -- but is left un-retired
             so it can show on a future turn.  (Anno's Room 101 has two identical
             "narrow opening" appends straddling a DisplayOnce faint segment; The runner
             shows the line once because the failing faint segment returns before
             the second append.)  */
          if (once)
            { terminated = 1; break; }
          continue;             /* (Phase 4: show the description's FailMessage) */
        }

      /* Retire the segment only when this is real output, not a value peek
         (Displayed is set unless UserSession.bTestingOutput). */
      if (once && st->marking_display)
        {
          a5state_disp_once_mark (st, c);
          /* <ReturnToDefault>: after this segment displays, clsDescription.
             ToString resets Displayed on every segment up to AND INCLUDING
             itself (`sd2.Displayed = False : If sd2 Is sd Then Exit For`), so
             the DisplayOnce sequence cycles back to the first segment on the
             next render.  RunBronwynn's Task12 "Run Count" atmosphere rotates
             4 such lines (woodcutter / 3 bloodhound urgency variants) through
             exactly this mechanism. */
          if (a5xml_bool (a5xml_child_text (c, "ReturnToDefault")))
            for (const a5_xml_node_t *r = wrapper->first_child; r != NULL;
                 r = r->next)
              {
                if (strcmp (r->name, "Description") == 0)
                  a5state_disp_once_unmark (st, r);
                if (r == c)
                  break;
              }
        }

      when = a5xml_child_text (c, "DisplayWhen");
      if (streq (when, "StartDescriptionWithThis"))
        {
          sb.len = 0;
          if (sb.p) sb.p[0] = '\0';
          sb_puts (&sb, text);
        }
      else if (streq (when, "StartAfterDefaultDescription"))
        {
          sb.len = 0;
          if (sb.p) sb.p[0] = '\0';
          if (!first)
            {
              sb_puts (&sb, default_text);
              if (add_space (sb.p, sb.len))
                sb_puts (&sb, "  ");
            }
          sb_puts (&sb, text);
        }
      else /* AppendToPreviousDescription (default) */
        {
          if (add_space (sb.p, sb.len))
            sb_puts (&sb, "  ");
          /* The "<>" marker (the Adrift 5 runner ToString) is a zero-width separator: it
             keeps an unexpanded "%x%.Prop" at the join from merging with the next
             segment's text into one corrupt token.  render_plain drops it. */
          sb_puts (&sb, "<>");
          sb_puts (&sb, text);
        }
      first = 0;
      /* A DisplayOnce segment terminates the description: clsDescription.ToString
         does `Return sb.ToString` inside `If .DisplayOnce`, so once a not-yet-
         shown DisplayOnce segment is output, later segments are NOT considered.
         This is what makes a location's "long first-visit / short thereafter"
         pair work -- the long block is DisplayOnce + StartDescriptionWithThis, so
         a plain StartDescriptionWithThis follow-up only wins once the first has
         been retired. */
      if (once)
        { terminated = 1; break; }
    }

  *psb = sb;
  *pfirst = first;
  *pdefault = default_text;
  return terminated;
}

char *
a5text_eval_description (a5_state_t *st, const a5_xml_node_t *wrapper)
{
  sb_t sb;
  const char *default_text = NULL;
  int first = 1;
  sb_init (&sb);
  eval_desc_into (st, &sb, &first, &default_text, wrapper);
  return sb_finish (&sb);
}

/* The effective Short/LongDescription of a location (clsLocation.vb:62/78): the
   base <`tag`> Description, with any inherited `group_prop` group property's
   SingleDescriptions appended.

   Short: a DarkLocations member with no LightSources in scope renders
   "Everything is dark" (its dark alternate is StartDescriptionWithThis +
   restricted).  Routed through by %LocationName%, %DisplayLocation%'s room
   name, and the `.ShortDescription` OO reads, mirroring the runner where every
   short-name read goes through the property getter.

   Long: both properties are created GroupOnly by FileIO (vb:2429/2441), so a
   whole region can be described once on its group: Tempus Fugit gives its 21
   numbered Volcano rooms no LongDescription of their own and lets the Volcano
   group say "You are on the base of the Volcano of Eruptus."  Without this the
   room bodies come out empty, which also flips the object listing to the v5
   empty-room "There is a shovel here." instead of "Also here is a shovel." */
static char *
location_desc (a5_state_t *st, const char *lockey, const char *tag,
               const char *group_prop)
{
  const a5_location_t *l = lockey ? a5model_location (st->adv, lockey) : NULL;
  const a5_prop_t *grp;
  sb_t sb;
  const char *default_text = NULL;
  int first = 1, term;
  sb_init (&sb);
  if (l == NULL)
    return sb_finish (&sb);
  term = eval_desc_into (st, &sb, &first, &default_text,
                         a5xml_child (l->node, tag));
  grp = a5state_location_group_prop (st, lockey, group_prop);
  if (!term && grp != NULL && grp->value_node != NULL)
    eval_desc_into (st, &sb, &first, &default_text, grp->value_node);
  return sb_finish (&sb);
}

char *
a5text_location_short (a5_state_t *st, const char *lockey)
{
  return location_desc (st, lockey, "ShortDescription",
                        "ShortLocationDescription");
}

static char *
location_long_desc (a5_state_t *st, const char *lockey)
{
  return location_desc (st, lockey, "LongDescription",
                        "LongLocationDescription");
}

/* --------------------------------------------------------- object naming */

/* An object whose Name/Prefix/Article is itself a %function% reference --
   Dreamspun's dream pearl is literally named "%dreamtext[%dreamRNG%]%", so
   every pearl announces a different dream -- must render expanded wherever the
   name is used, not just where the author wrote the reference out by hand.
   The runner gets this for free: ReplaceFunctions loops the whole substitution
   block to a fixpoint (Global.vb:2441), so a name pasted into a message text is
   rescanned on the next round.  Scarier's replace_functions is a single
   left-to-right pass and never revisits what it emitted, so expand the name
   parts here instead.  Guarded against a name that references itself. */
static const char *
expand_name_part (const a5_state_t *st, const char *s, char **owned)
{
  static int depth = 0;
  char *p;
  if (st == NULL || s == NULL || strchr (s, '%') == NULL || depth >= 4)
    return s;
  depth++;
  p = a5text_process_noalr ((a5_state_t *) st, s);
  depth--;
  if (p == NULL)
    return s;
  *owned = p;
  return p;
}

char *
a5text_object_name (const a5_state_t *st, const a5_object_t *o, a5_article_t art)
{
  sb_t sb;
  char *own_noun = NULL, *own_prefix = NULL, *own_article = NULL;
  const char *noun = (o->n_names > 0) ? o->names[0] : o->key;
  const char *prefix = o->prefix;
  const char *article = o->article;

  /* The _ObjectArticle/_ObjectPrefix/_ObjectNoun mandatory-property overrides
     (clsUserSession.vb:1972-1982) feed clsObject.FullName too: a renamed
     object displays -- and lists, and %TheObject%s -- under its new noun.
     st may be NULL in naming-only contexts (no runtime state). */
  if (st != NULL)
    {
      const char *ov;
      if ((ov = a5state_entity_prop (st, o->key, "_ObjectNoun")) != NULL)
        noun = ov;
      if ((ov = a5state_entity_prop (st, o->key, "_ObjectPrefix")) != NULL)
        prefix = ov;
      if ((ov = a5state_entity_prop (st, o->key, "_ObjectArticle")) != NULL)
        article = ov;
    }

  noun    = expand_name_part (st, noun,    &own_noun);
  prefix  = expand_name_part (st, prefix,  &own_prefix);
  article = expand_name_part (st, article, &own_article);

  sb_init (&sb);
  if (art == A5_ART_DEFINITE)
    sb_puts (&sb, "the ");
  else if (art == A5_ART_INDEFINITE)
    {
      /* The runner clsObject.FullName: Indefinite -> sArticle2 = sArticle & " ",
         i.e. the space is appended even when the article is empty, so an
         empty-article object renders with a leading space (e.g. the worn
         "your clothes" -> " your clothes" in an inventory list). */
      if (article != NULL && article[0] != '\0')
        sb_puts (&sb, article);
      sb_putc (&sb, ' ');
    }
  if (prefix != NULL && prefix[0] != '\0')
    {
      sb_puts (&sb, prefix);
      sb_putc (&sb, ' ');
    }
  sb_puts (&sb, noun != NULL ? noun : "");
  free (own_noun);
  free (own_prefix);
  free (own_article);
  return sb_finish (&sb);
}

/* ---------------------------------------------------- function replacement */

static char *replace_functions (a5_state_t *st, const char *src, int as_arg = 0);

/* %PopUpInput% host callback (a5text_set_popup_cb); NULL => use the default. */
static a5_popup_cb a5_popup = NULL;
static void *a5_popup_ctx = NULL;
static char *view_location_impl (a5_state_t *st, const char *lockey);

/* Resolve a function argument (a key or display name) to an object key. */
static const char *
resolve_object_arg (a5_state_t *st, const char *args)
{
  if (args == NULL || args[0] == '\0')
    return NULL;
  if (a5model_object (st->adv, args) != NULL)
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
  return NULL;
}

/* Join a list of object keys as an "a, b and c" list with the given article
   ("" if empty).  Mirrors ObjectHashTable.List (StronglyTypedCollections.vb:183). */
static char *
list_objects_art (a5_state_t *st, const std::vector<const char *> &keys,
                  a5_article_t art)
{
  sb_t sb; sb_init (&sb);
  int n = (int) keys.size ();
  for (int i = 0; i < n; i++)
    {
      const a5_object_t *o = a5model_object (st->adv, keys[i]);
      char *nm = o ? a5text_object_name (st, o, art) : strdup (keys[i]);
      if (i > 0) sb_puts (&sb, (i == n - 1) ? " and " : ", ");
      sb_puts (&sb, nm);
      free (nm);
    }
  return sb_finish (&sb);
}

/* Join a list of object keys as an indefinite "a, b and c" list ("" if empty). */
static char *
list_objects (a5_state_t *st, const std::vector<const char *> &keys)
{
  return list_objects_art (st, keys, A5_ART_INDEFINITE);
}

/* Objects placed on (want=A5_OWHERE_ON_OBJECT) or in a container object key. */
static std::vector<const char *>
objects_on_in (a5_state_t *st, const char *objkey, a5_owhere_t want)
{
  std::vector<const char *> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    if (st->obj[i].where == want && streq (st->obj[i].key, objkey))
      v.push_back (st->adv->objects[i].key);
  return v;
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
      if (st->char_onobj == NULL || st->char_onobj[i] == NULL)
        continue;
      if (!streq (st->char_onobj[i], objkey))
        continue;
      int in = st->char_in != NULL && st->char_in[i];
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
  int openable = o && a5_prop_find (o->props, o->n_props, "Openable") != NULL;
  if (openable)
    {
      const char *os = a5state_entity_prop (st, objkey, "OpenStatus");
      if (os == NULL || !streq (os, "Open"))
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
  if (args == NULL)
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
      if (!seg.empty () && a5model_object (st->adv, seg.c_str ()) != NULL)
        keys.push_back (seg);
      if (bar == std::string::npos)
        break;
      start = bar + 1;
    }
  return keys;
}

/* A character's perspective (FirstPerson=1 / SecondPerson=2 / ThirdPerson=3).
   Keyed on whether `c` is the CURRENT player, not its static Type: after a
   ToSwitchWith/BECOME the new player is still typed NonPlayer, yet must narrate
   in the player's perspective ("you pick up ...", not "Lance-Corporal Davey pick
   up ...").  For every game without BECOME this is identical to the old type
   test -- the sole Type=Player character is exactly the current player. */
static int
char_perspective (const a5_state_t *st, const a5_character_t *c)
{
  /* clsCharacter.Perspective's getter: NonPlayer-TYPE characters are ALWAYS
     third person; the Player-type one uses its stored perspective.  A BECOME
     (MoveCharacter ToSwitchWith the player, clsUserSession.vb:1837) flips the
     types AND copies the old player's perspective onto the new player -- so
     the CURRENT player always renders with the ORIGINAL Player character's
     load-time perspective (Xanix's King Kelson stays "you"; The Salvage's
     Captain Pearson renders by name, because its Player char has no
     <Perspective>), and everyone else renders third person. */
  if (c == NULL)
    return 2;
  if (st == NULL || !streq (c->key, a5state_player_key (st)))
    return 3;
  {
    const a5_character_t *orig = NULL;
    for (int i = 0; i < st->adv->n_characters; i++)
      if (st->adv->characters[i].type != NULL
          && ci_eq (st->adv->characters[i].type, "Player"))
        { orig = &st->adv->characters[i]; break; }
    if (orig == NULL)
      orig = c;
    if (orig->perspective != NULL)
      {
        if (ci_eq (orig->perspective, "FirstPerson"))  return 1;
        if (ci_eq (orig->perspective, "SecondPerson")) return 2;
        if (ci_eq (orig->perspective, "ThirdPerson"))  return 3;
      }
    /* No stored <Perspective>: the runner's loader records ThirdPerson for files
       >= 5.00002 and forces SecondPerson for older ones (FileIO.vb:1776-7). */
    if (st->adv->version != NULL
        && strtod (st->adv->version, NULL) < 5.00002)
      return 2;
    return 3;
  }
}

typedef enum { A5_PRO_SUBJ, A5_PRO_OBJ, A5_PRO_POSS, A5_PRO_REFL } a5_pronoun_t;

/* clsCharacter.ProperName (clsCharacter.vb:455): the runtime
   CharacterProperName override (a typed-in player name, set via SetProperty --
   clsUserSession.vb:2080) wins over the model Name, and an empty name renders
   "Anonymous". */
static const char *
char_proper_name_raw (const a5_state_t *st, const a5_character_t *c)
{
  const char *pn = c ? a5state_entity_prop (st, c->key, "CharacterProperName")
                     : NULL;
  if (pn == NULL || pn[0] == '\0')
    pn = (c != NULL && c->name != NULL && c->name[0] != '\0') ? c->name
                                                              : "Anonymous";
  return pn;
}

/* The possessive form of a heap-allocated name: append 's, consuming nm. */
static char *
possessive (char *nm)
{
  size_t n = strlen (nm);
  char *s = (char *) malloc (n + 3);
  memcpy (s, nm, n);
  s[n] = '\'';
  s[n + 1] = 's';
  s[n + 2] = '\0';
  free (nm);
  return s;
}

/*
 * A third-person character's displayed name, mirroring clsCharacter.Name's
 * non-pronoun branch: a "Known"-and-selected character (or one with no
 * descriptor) shows its ProperName; otherwise its Descriptor(Article) --
 * "a young woman" (Indefinite) / "the young woman" (Definite).  The "Known"
 * property may be set at runtime (SetProperty Susan Known <Selected>), so check
 * the override layer too.
 */
static char *
character_display_name (a5_state_t *st, const a5_character_t *c, int definite,
                        int displaying = 1)
{
  /* clsCharacter.Name's Introduced dance (clsCharacter.vb:331-334), which only
     runs under UserSession.bDisplaying: once a character's name has rendered
     in displayed output, later Indefinite descriptor renders upgrade to the
     Definite article ("a tall guillermo" -> "the tall guillermo"), and the
     render itself marks the character Introduced.  In the runner the only renders
     that happen under bDisplaying are of text still carrying its %functions%
     when it reaches Display() -- see a5state.h's intro_active. */
  if (displaying && st->intro_active
      && c != NULL && st->char_introduced != NULL)
    {
      int intro_ci = a5state_character_index (st, c->key);
      if (intro_ci >= 0)
        {
          if (st->char_introduced[intro_ci])
            definite = 1;
          st->char_introduced[intro_ci] = 1;
        }
    }
  /* clsCharacter.Name: a character renders by ProperName once its "Known"
     SelectionOnly property is set, otherwise by its descriptor.  Use the same
     merged, Unselected-aware test the rest of the engine uses
     (a5state_entity_has_prop) instead of a substring match on the value: a bare
     strstr (known, "Selected") also matches the "Unselected" token and would
     wrongly treat an unselected character as known, leaking its name.  Fall
     back to the proper name when there is no descriptor to render -- this also
     guarantees the descriptor branch below only runs with n_descriptors > 0,
     so c->descriptors[0] is never a NULL dereference. */
  int known_set = c != NULL && a5state_entity_has_prop (st, c->key, "Known");
  int use_proper = known_set || c == NULL || c->n_descriptors == 0;
  if (use_proper)
    return strdup (char_proper_name_raw (st, c));
  {
    sb_t sb;
    sb_init (&sb);
    if (c->article != NULL && c->article[0] != '\0')
      { sb_puts (&sb, definite ? "the" : c->article); sb_putc (&sb, ' '); }
    if (c->prefix != NULL && c->prefix[0] != '\0')
      { sb_puts (&sb, c->prefix); sb_putc (&sb, ' '); }
    sb_puts (&sb, c->descriptors[0]);
    return sb_finish (&sb);
  }
}

/*
 * The displayed name of a character with a pronoun, mirroring clsCharacter.Name:
 * first/second person always render as a pronoun (I/you/...); third person
 * renders the proper/descriptor name (we don't pronoun-replace NPCs without
 * mention tracking, which is the safe default).
 */
static char *
character_name (a5_state_t *st, const a5_character_t *c, a5_pronoun_t pr,
                int consult_ledger = 0)
{
  int persp = char_perspective (st, c);
  if (persp == 1)
    return strdup (pr == A5_PRO_OBJ ? "me" : pr == A5_PRO_POSS ? "my"
                 : pr == A5_PRO_REFL ? "myself" : "I");
  if (persp == 2)
    return strdup (pr == A5_PRO_POSS ? "your" : pr == A5_PRO_REFL ? "yourself"
                 : "you");
  /* Third person: a previous PronounKeys mention of this SAME character in the
     current command's output pronoun-replaces the name -- clsCharacter.Name
     (clsCharacter.vb:340) scans the ledger backward for a same-key entry;
     when found, the name renders as a gendered pronoun ("%CharacterName[
     Landlord]% ignores you." -> "He ignores you.", DieFeuerfaust's
     conversation fallback), and a requested Objective upgrades to Reflective
     when the previous mention was Subjective (vb:352).  Only the
     %CharacterName% function path consults (the runner's other Name() call sites pass
     bAllowPronouns:=False or don't run under bDisplaying). */
  if (consult_ledger && c != NULL)
    for (int i = st->n_pron - 1; i >= 0; i--)
      if (st->pron_key[i] != NULL && streq (st->pron_key[i], c->key))
        {
          /* clsCharacter.Name marks Introduced (vb:333) BEFORE the pronoun
             branch, so a reply that pronoun-replaces the name ("He looks up
             at ye") still introduces the character -- the next descriptor
             render is definite.  Same intro_active gate as
             character_display_name. */
          if (st->intro_active && st->char_introduced != NULL)
            {
              int intro_ci = a5state_character_index (st, c->key);
              if (intro_ci >= 0)
                st->char_introduced[intro_ci] = 1;
            }
          const char *g = a5state_entity_prop (st, c->key, "Gender");
          int male   = g != NULL && strcasecmp (g, "Male") == 0;
          int female = g != NULL && strcasecmp (g, "Female") == 0;
          a5_pronoun_t want = pr;
          if (st->pron_pron[i] == (int) A5_PRO_SUBJ && pr == A5_PRO_OBJ)
            want = A5_PRO_REFL;
          return strdup (
              want == A5_PRO_OBJ  ? (male ? "him" : female ? "her" : "it")
            : want == A5_PRO_POSS ? (male ? "his" : female ? "her" : "its")
            : want == A5_PRO_REFL ? (male ? "himself" : female ? "herself"
                                                               : "itself")
            :                       (male ? "he" : female ? "she" : "it"));
        }
  /* third person: the display name (possessive adds 's) */
  {
    char *nm = character_display_name (st, c, 0);
    return (pr == A5_PRO_POSS) ? possessive (nm) : nm;
  }
}

char *
a5text_character_subjective (a5_state_t *st, const a5_character_t *c)
{
  return character_name (st, c, A5_PRO_SUBJ);
}

/* Global.vb:1286-1319, the character branch of ReplaceOOProperty's "Name":
   parse the (args) into clsCharacter.Name's Pronoun / Article / bForcePronoun /
   bExplicitArticle, then run clsCharacter.vb:323's own logic.  Two things
   differ from the OBJECT `.Name` the rest of a5expr shares: the default article
   is DEFINITE ("opposite default from objects" -- so `%character%.Name` on an
   introduced NPC is "the black rat"), and the args can name a PRONOUN, with a
   bare "None" meaning "never pronoun-replace" rather than "no article". */
char *
a5text_character_oo_name (a5_state_t *st, const a5_character_t *c,
                          const char *args)
{
  std::string a = args != NULL ? args : "";
  for (char &ch : a) ch = (char) tolower ((unsigned char) ch);
  auto has = [&](const char *n) { return a.find (n) != std::string::npos; };
  /* ContainsWord: whole-word, so "indefinite" does not answer for "definite". */
  auto word = [&](const char *n) {
    size_t nl = strlen (n), p = 0;
    while ((p = a.find (n, p)) != std::string::npos)
      {
        int lok = (p == 0) || !isalnum ((unsigned char) a[p - 1]);
        int rok = (p + nl >= a.size ()) || !isalnum ((unsigned char) a[p + nl]);
        if (lok && rok) return true;
        p += nl;
      }
    return false;
  };

  int art_definite = 1, art_none = 0, explicit_article = 0;
  a5_pronoun_t pr = A5_PRO_SUBJ;
  int pronoun_none = 0, force_pronoun = 0;

  if (has ("none"))
    {
      /* "None" is ambiguous between the article and the pronoun; the runner
         disambiguates on the other words present, defaulting to the pronoun. */
      if (has ("definite"))                       /* covers "indefinite" too */
        pronoun_none = 1;
      else if (has ("force") || has ("objective") || has ("possessive")
               || has ("reflective"))
        art_none = 1;
      else
        pronoun_none = 1;
    }
  if (has ("force")) force_pronoun = 1;
  if (has ("objective") || has ("object") || has ("target")) pr = A5_PRO_OBJ;
  if (has ("possessive") || has ("possess"))                 pr = A5_PRO_POSS;
  if (has ("reflective") || has ("reflect"))                 pr = A5_PRO_REFL;
  if (word ("definite"))
    { art_definite = 1; art_none = 0; explicit_article = 1; }
  if (word ("indefinite"))
    { art_definite = 0; art_none = 0; explicit_article = 1; }

  /* clsCharacter.Name's bDisplaying block (vb:331-334). */
  int replace_with_pronoun = force_pronoun;
  if (st->intro_active)
    {
      if (!explicit_article && !art_none && !art_definite && c != NULL
          && st->char_introduced != NULL)
        {
          int ci = a5state_character_index (st, c->key);
          if (ci >= 0 && st->char_introduced[ci]) art_definite = 1;
        }
      if (c != NULL && st->char_introduced != NULL)
        {
          int ci = a5state_character_index (st, c->key);
          if (ci >= 0) st->char_introduced[ci] = 1;
        }
      int persp = char_perspective (st, c);
      if (persp == 1 || persp == 2) replace_with_pronoun = 1;
    }
  if (pronoun_none) replace_with_pronoun = 0;

  if (replace_with_pronoun)
    return character_name (st, c, pr);

  /* The non-pronoun branch: Known -> ProperName, else Descriptor(Article). */
  int known_set = c != NULL && a5state_entity_has_prop (st, c->key, "Known");
  char *nm;
  if (known_set || c == NULL || c->n_descriptors == 0)
    nm = strdup (char_proper_name_raw (st, c));
  else
    {
      sb_t sb;
      sb_init (&sb);
      if (!art_none && c->article != NULL && c->article[0] != '\0')
        { sb_puts (&sb, art_definite ? "the" : c->article); sb_putc (&sb, ' '); }
      if (c->prefix != NULL && c->prefix[0] != '\0')
        { sb_puts (&sb, c->prefix); sb_putc (&sb, ' '); }
      sb_puts (&sb, c->descriptors[0]);
      nm = sb_finish (&sb);
    }
  return (pr == A5_PRO_POSS) ? possessive (nm) : nm;
}

char *
a5text_char_proper_name (a5_state_t *st, const char *charkey)
{
  int ci = a5state_character_index (st, charkey);
  /* The walk enter/exit announcements build ToProper(.Name) OUTSIDE
     bDisplaying (clsCharacter.vb:1558/1576 -- Display gets the composed
     string later), so no Introduced article upgrade ("Your double enters",
     not "The double enters") and no marking. */
  char *nm = character_display_name (st, ci >= 0 ? &st->adv->characters[ci] : NULL, 0,
                                     /*displaying*/ 0);
  /* ToProper(.Name, bForceRestLower:=False): upper-case the first char only. */
  if (nm[0])
    nm[0] = (char) toupper ((unsigned char) nm[0]);
  return nm;
}

char *
a5text_character_known_name (a5_state_t *st, const a5_character_t *c, int definite)
{
  return character_display_name (st, c, definite);
}

/*
 * Map a bare %reference% name (the token between the percents, already without
 * them) to the resolved entity key(s) for a property expression %name%.Chain.
 * Returns a malloc'd "key" / "key1|key2" string, or NULL if `name` is not an
 * OO-addressable reference or entity key.  Mirrors the %Player%/%objectN%/%objects%
 * -> key substitution the Adrift 5 runner does before ReplaceOO (Global.vb:1752+).
 */
static char *
oo_firstkey (a5_state_t *st, const char *name)
{
  const char *k = NULL;
  if (ci_eq (name, "player"))
    return strdup (a5state_player_key (st));
  /* The singular %object%/%object1% (resp. %character%/%character1%) token
     resolves to nothing when the slot was filled by a *plural* %objects%/
     %characters% reference -- the runner's GetReference requires ReferenceMatch="object1"
     (clsUserSession.vb:3990), so a %objects% command never sets %object%.  The
     plural %objects% / index forms %object2%.. and override matching are
     unaffected (they read the still-bound key). */
  if ((ci_eq (name, "object") || ci_eq (name, "object1")) && st->ref_object1_plural)
    return NULL;
  if ((ci_eq (name, "character") || ci_eq (name, "character1"))
      && st->ref_character1_plural)
    return NULL;
  if (ci_eq (name, "objects"))
    k = a5state_lookup_ref (st, "ReferencedObjects");
  else if (ci_eq (name, "object") || ci_eq (name, "object1"))
    k = a5state_lookup_ref (st, "ReferencedObject");
  else if (ci_eq (name, "object2")) k = a5state_lookup_ref (st, "ReferencedObject2");
  else if (ci_eq (name, "object3")) k = a5state_lookup_ref (st, "ReferencedObject3");
  else if (ci_eq (name, "object4")) k = a5state_lookup_ref (st, "ReferencedObject4");
  else if (ci_eq (name, "object5")) k = a5state_lookup_ref (st, "ReferencedObject5");
  else if (ci_eq (name, "characters") || ci_eq (name, "character")
           || ci_eq (name, "character1"))
    k = a5state_lookup_ref (st, "ReferencedCharacter");
  /* ...and the indexed character slots, exactly like %object2%..%object5%.
     A two-character command (`#-- Enemy attacks friend --# %character1%
     attacks %character2%`, Beginner's Cave's combat engine) binds
     ReferencedCharacter2 through the same SetTasks-Execute parameter path;
     without these its message rendered the token verbatim. */
  else if (ci_eq (name, "character2")) k = a5state_lookup_ref (st, "ReferencedCharacter2");
  else if (ci_eq (name, "character3")) k = a5state_lookup_ref (st, "ReferencedCharacter3");
  else if (ci_eq (name, "character4")) k = a5state_lookup_ref (st, "ReferencedCharacter4");
  else if (ci_eq (name, "character5")) k = a5state_lookup_ref (st, "ReferencedCharacter5");
  if (k == NULL && (ci_eq (name, "object") || ci_eq (name, "objects")))
    k = a5state_lookup_ref (st, "ReferencedObject");
  if (k != NULL)
    return strdup (k);
  /* a literal entity key (object/character/location/group)? */
  if (a5model_object (st->adv, name) || a5model_character (st->adv, name)
      || a5model_location (st->adv, name))
    return strdup (name);
  for (int i = 0; i < st->adv->n_groups; i++)
    if (streq (st->adv->groups[i].key, name))
      return strdup (name);
  return NULL;
}


/* ---- eval_function per-builtin handlers (extracted from eval_function) ---- */

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
  auto unquote = [](std::string &s){
    size_t b = s.find_first_not_of (" \t");
    size_t e = s.find_last_not_of (" \t");
    s = (b == std::string::npos) ? "" : s.substr (b, e - b + 1);
    if (s.size () >= 2 && s.front () == '"' && s.back () == '"')
      s = s.substr (1, s.size () - 2); };
  unquote (prompt); unquote (dflt);
  if (a5_popup != NULL)
    {
      char *ans = a5_popup (a5_popup_ctx, prompt.c_str (), dflt.c_str ());
      if (ans != NULL) return ans;
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
  /* trim */
  auto trim = [](std::string &s){
    size_t b = s.find_first_not_of (" \t");
    size_t e = s.find_last_not_of (" \t");
    s = (b == std::string::npos) ? "" : s.substr (b, e - b + 1); };
  trim (key); trim (pron);
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
  if (ch != NULL)
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
  std::string key = args ? args : "";
  size_t b = key.find_first_not_of (" \t");
  size_t e = key.find_last_not_of (" \t");
  key = (b == std::string::npos) ? "" : key.substr (b, e - b + 1);
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
  const char *found = NULL;
  int count = 0, i;
  for (i = 0; ploc != NULL && i < st->adv->n_characters; i++)
    {
      const char *oloc;
      if (ci_eq (st->adv->characters[i].key, a5state_player_key (st)))
        continue;
      oloc = a5state_character_location_key (st, i);
      if (oloc != NULL && streq (oloc, ploc))
        { count++; found = st->adv->characters[i].key; }
    }
  return strdup ((count == 1) ? found : "NoCharacter");
}

static char *
fn_locationname (a5_state_t *st, const char * /*name*/, const char *args)
{
  const char *key = (args && args[0]) ? args : a5state_player_location (st);
  if (key != NULL && a5model_location (st->adv, key) != NULL)
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
    return strdup (st->char_loc[ci] ? st->char_loc[ci] : "");
  {
    int oi = a5state_object_index (st, key);
    if (oi >= 0)
      {
        sb_t sb; sb_init (&sb);
        for (i = 0; i < st->adv->n_locations; i++)
          if (a5state_object_at_location (st, oi, st->adv->locations[i].key, 1))
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
  const a5_location_t *l = (args && args[0]) ? a5model_location (st->adv, args) : NULL;
  if (l != NULL)
    {
      char *s = view_location_impl (st, args);
      if (s == NULL || s[0] == '\0')
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
  if (o == NULL && strchr (args, '|') != NULL)
    {
      std::vector<std::string> ks = arg_object_keys (st, args);
      if (ks.size () > 1)
        {
          std::vector<const char *> kp;
          for (auto &s : ks) kp.push_back (s.c_str ());
          return list_objects_art (st, kp, art);
        }
    }
  if (o == NULL && args != NULL && args[0] != '\0')
    {
      /* args may be a key or a display name; resolve_object_arg also
         matches a "prefix + name" full name (e.g. "framed newspaper
         article"), which a bare %object% inner render produces -- the
         plain names[] search missed those and dropped the article. */
      const char *k = resolve_object_arg (st, args);
      if (k != NULL)
        o = a5model_object (st->adv, k);
    }
  if (o != NULL)
    return a5text_object_name (st, o, art);
  /* No object resolved.  The Adrift 5 runner builds an ObjectHashTable from the key
     arg and renders htblObjects.List, which returns "nothing" for an empty
     set (Global.vb:804) -- so %TheObject[]% / %ObjectName[]% with an empty
     (or whitespace-only) argument is "nothing", not a blank.  A non-empty but
     unresolved arg is left as-is (it may be an already-rendered name). */
  {
    const char *a = args ? args : "";
    while (*a == ' ' || *a == '\t') a++;
    if (*a == '\0')
      return strdup ("nothing");
  }
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
  /* arg0 may be a key or a display name; map to a key. */
  const char *ekey = resolve_object_arg (st, ent.c_str ());
  /* A runtime SetProperty override wins over the static model value, just
     as a5state_entity_prop layers it (e.g. Amazon's CarriersFl1 runs
     `SetProperty Door1 LockKey Key3`, retargeting the lazy-unlock chain's
     `%PropertyValue[Door1,LockKey]%` from the lost silver key to the iron
     key).  The static value_node path below still serves text properties. */
  if (ekey != NULL)
    for (int oi = 0; oi < st->n_ov; oi++)
      if (streq (st->ov[oi].entity, ekey)
          && streq (st->ov[oi].prop, prop.c_str ()))
        return strdup (st->ov[oi].value ? st->ov[oi].value : "");
  const a5_prop_t *props = NULL; int n_props = 0;
  const a5_object_t *o = ekey ? a5model_object (st->adv, ekey) : NULL;
  if (o != NULL) { props = o->props; n_props = o->n_props; }
  else {
    const a5_character_t *c = ekey ? a5model_character (st->adv, ekey) : NULL;
    if (c != NULL) { props = c->props; n_props = c->n_props; }
    else {
      const a5_location_t *l = ekey ? a5model_location (st->adv, ekey) : NULL;
      if (l != NULL) { props = l->props; n_props = l->n_props; }
    }
  }
  const a5_prop_t *pr = props ? a5_prop_find (props, n_props, prop.c_str ()) : NULL;
  if (pr == NULL)
    return strdup ("");
  if (pr->value_node != NULL)
    return a5text_eval_description (st, pr->value_node);
  return strdup (pr->value ? pr->value : "");
}

static char *
fn_parentof (a5_state_t *st, const char * /*name*/, const char *args)
{
  int i;
  /* args is a key or a display name; emit the parent's key. */
  const a5_object_t *o = a5model_object (st->adv, args);
  const char *k = NULL;
  if (o == NULL)
    for (i = 0; i < st->adv->n_objects; i++)
      { const a5_object_t *cand = &st->adv->objects[i];
        for (int j = 0; j < cand->n_names; j++)
          {
            if (ci_eq (cand->names[j], args)) { o = cand; break; }
            /* also match "<prefix> <noun>" (the display form %objects% yields) */
            if (cand->prefix && cand->prefix[0])
              { std::string full = std::string (cand->prefix) + " " + cand->names[j];
                if (ci_eq (full.c_str (), args)) { o = cand; break; } }
          }
        if (o != NULL) break; }
  if (o != NULL)
    { char *pp = a5expr_eval (st, o->key, ".Parent"); if (pp) return pp; k = ""; }
  /* Characters too: %ParentOf[%Player%]% is the seat/container the
     character is on/in (clsCharacter.Parent) -- the stock
     MoveOffCurrentObject passes it to MoveOffObject (Magnetic Moon's
     `stand up` from the nav-console chair). */
  {
    const a5_character_t *c = a5model_character (st->adv, args);
    if (c == NULL)
      for (i = 0; i < st->adv->n_characters; i++)
        if (st->adv->characters[i].name != NULL
            && ci_eq (st->adv->characters[i].name, args))
          { c = &st->adv->characters[i]; break; }
    if (c != NULL)
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
  if (k != NULL) v = objects_on_in (st, k, want);
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
  const a5_object_t *o = k ? a5model_object (st->adv, k) : NULL;
  std::vector<const a5_character_t *> on, in;
  if (k != NULL && !ci_eq (name, "listcharactersin")) on = chars_on_in (st, k, 1);
  if (k != NULL && !ci_eq (name, "listcharacterson")) in = chars_on_in (st, k, 0);
  /* DisplayCharacterChildren lists inside-characters only when the object
     is `Not Openable OrElse IsOpen` (clsObject.vb:383) -- same gate as the
     object children above. */
  if (!in.empty () && o != NULL
      && a5_prop_find (o->props, o->n_props, "Openable") != NULL)
    {
      const char *os = a5state_entity_prop (st, k, "OpenStatus");
      if (os == NULL || !streq (os, "Open"))
        in.clear ();
    }
  if (on.empty () && in.empty ())
    return strdup ("");
  /* DisplayCharacterChildren: "X is on/inside the <object>." */
  std::string out;
  char *defn = o ? a5text_object_name (st, o, A5_ART_DEFINITE) : strdup (k ? k : "");
  auto names = [&](std::vector<const a5_character_t *> &cs) {
    std::string s; int n = (int) cs.size ();
    for (int i = 0; i < n; i++)
      { char *nm = a5text_character_subjective (st, cs[i]);
        if (i > 0) s += (i == n - 1) ? " and " : ", ";
        s += nm; free (nm); }
    return s; };
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
  if (c != NULL)
    {
      a5_owhere_t want = ci_eq (name, "listheld") ? A5_OWHERE_HELD_BY
                                                  : A5_OWHERE_WORN_BY;
      for (int i = 0; i < st->adv->n_objects; i++)
        if (st->obj[i].where == want && streq (st->obj[i].key, c->key))
          v.push_back (st->adv->objects[i].key);
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
  const char *key = NULL;
  int is_char = ci_eq (name, "displaycharacter");
  if (is_char)
    { const a5_character_t *c = a5model_character (st->adv, args);
      key = c ? c->key : NULL; }
  else
    key = resolve_object_arg (st, args);
  if (key != NULL)
    {
      char *d = a5expr_eval (st, key, ".Description");
      if (d != NULL && d[0] != '\0')
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
          const char *qual = (subj != NULL && streq (subj, key))
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
  return NULL;
}

static char *
fn_list_exits (a5_state_t *st, const char * /*name*/, const char *args)
{
  /* clsCharacter.ListExits: the comma/"and"-joined, lower-cased list of
     directions the character has a (restriction-checked) route in, or
     "nowhere" when there are none.  Reuse a5expr's character .Exits path
     (same DirectionsEnum order + HasRouteInDirection check). */
  char *cnt = a5expr_eval (st, args, ".Exits.Count");
  long n = cnt ? strtol (cnt, NULL, 10) : 0;
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

static char *
fn_number_as_text (a5_state_t * /*st*/, const char * /*name*/, const char *args)
{
  static const char *ones[] = { "zero","one","two","three","four","five",
    "six","seven","eight","nine","ten","eleven","twelve" };
  long n = strtol (args, NULL, 10);
  if (n >= 0 && n <= 12) return strdup (ones[n]);
  return strdup (args);
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

/* Evaluate one %Name[args]% (args already function-expanded), or NULL. */
static char *
eval_function (a5_state_t *st, const char *name, const char *args)
{
  int i;

  st->name_cap_eligible = 0;      /* only a resolved CharacterName sets it */

  /* User-defined <Function>s (clsUserFunction): the runner's ReplaceFunctions runs
     EvaluateUDF over every UDF FIRST (Global.vb:1748), matching the exact
     case-sensitive name with an optional [args] block.  The Output is a
     restriction-gated description whose declared <Argument> names are
     substituted with the (already function-expanded) argument values -- The
     Last Expedition's %Lantern[]% renders "with his free gloved hand" while
     the lantern is held, and its %Red[text]%-style wrappers colour the arg. */
  {
    static int udf_depth = 0;
    for (i = 0; i < st->adv->n_udfs; i++)
      {
        const a5_udf_t *u = &st->adv->udfs[i];
        if (u->name == NULL || strcmp (u->name, name) != 0)
          continue;
        const a5_xml_node_t *outp = a5xml_child (u->node, "Output");
        if (outp == NULL || udf_depth > 8)   /* The runner: recursive UDF = error */
          return strdup ("");
        udf_depth++;
        char *r = a5text_describe (st, outp);
        udf_depth--;
        if (r != NULL && args != NULL && args[0] != '\0')
          {
            /* Split the [.] block on top-level commas (the runner SplitArgs) and
               replace each declared argument's %Name% token in the render. */
            std::vector<std::string> vals;
            {
              std::string cur;
              int q = 0;
              for (const char *p = args; *p != '\0'; p++)
                {
                  if (*p == '"') q = !q;
                  if (*p == ',' && !q) { vals.push_back (cur); cur.clear (); }
                  else cur += *p;
                }
              vals.push_back (cur);
            }
            std::string out = r;
            size_t vi = 0;
            for (const a5_xml_node_t *an = u->node->first_child;
                 an != NULL; an = an->next)
              {
                if (strcmp (an->name, "Argument") != 0)
                  continue;
                const char *anm = a5xml_child_text (an, "Name");
                if (anm != NULL && vi < vals.size ())
                  {
                    std::string tok = std::string ("%") + anm + "%";
                    size_t at;
                    while ((at = out.find (tok)) != std::string::npos)
                      out.replace (at, tok.size (), vals[vi]);
                  }
                vi++;
              }
            free (r);
            r = strdup (out.c_str ());
          }
        return r;
      }
  }

  if (ci_eq (name, "player"))
    return fn_player (st, name, args);
  if (ci_eq (name, "popupinput"))
    return fn_popupinput (st, name, args);
  if (ci_eq (name, "charactername"))
    return fn_charactername (st, name, args);
  if (ci_eq (name, "characterproper") || ci_eq (name, "propername"))
    return fn_characterproper (st, name, args);
  if (ci_eq (name, "convcharacter"))
    /* Global.vb:1762 substitutes %ConvCharacter% with the conversation char KEY. */
    return strdup (st->conv_char ? st->conv_char : "");
  if (ci_eq (name, "turns"))
    return fn_turns (st, name, args);
  if (ci_eq (name, "alonewithchar"))
    return fn_alonewithchar (st, name, args);
  if (ci_eq (name, "locationname"))
    return fn_locationname (st, name, args);
  if (ci_eq (name, "locationof"))
    return fn_locationof (st, name, args);
  if (ci_eq (name, "displaylocation"))
    return fn_displaylocation (st, name, args);
  if (args != NULL
      && (ci_eq (name, "theobject") || ci_eq (name, "aobject")
          || ci_eq (name, "objectname") || ci_eq (name, "object")
          || ci_eq (name, "theobjects") || ci_eq (name, "aobjects")
          || ci_eq (name, "objects")))
    return fn_object_names (st, name, args);
  if (ci_eq (name, "propertyvalue") && args != NULL)
    return fn_propertyvalue (st, name, args);
  if (ci_eq (name, "parentof") && args != NULL)
    return fn_parentof (st, name, args);
  if (args != NULL
      && (ci_eq (name, "listobjectson") || ci_eq (name, "listobjectsin")))
    return fn_list_objects_on_in (st, name, args);
  if (args != NULL && ci_eq (name, "listobjectsonandin"))
    return fn_list_objects_on_and_in (st, name, args);
  if (args != NULL
      && (ci_eq (name, "listcharacterson") || ci_eq (name, "listcharactersin")
          || ci_eq (name, "listcharactersonandin")))
    return fn_list_characters (st, name, args);
  if (args != NULL && (ci_eq (name, "listheld") || ci_eq (name, "listworn")))
    return fn_list_held_worn (st, name, args);
  if (args != NULL && (ci_eq (name, "displaycharacter") || ci_eq (name, "displayobject")))
    return fn_display_char_obj (st, name, args);
  if (args != NULL && ci_eq (name, "listexits"))
    return fn_list_exits (st, name, args);
  if (args != NULL && ci_eq (name, "listobjectsatlocation"))
    return fn_list_objects_at_location (st, name, args);
  if (ci_eq (name, "numberastext") && args != NULL)
    return fn_number_as_text (st, name, args);
  if (ci_eq (name, "pcase") || ci_eq (name, "ucase") || ci_eq (name, "lcase"))
    return fn_case (st, name, args);

  /* A bound reference produced by the parser this turn (%direction%, %text%). */
  if (args == NULL)
    {
      const char *bound = NULL;
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
              sb_t sb; sb_init (&sb);
              for (int i = 0; i < st->n_ref_items; i++)
                {
                  const a5_object_t *o = a5model_object (st->adv, st->ref_items[i]);
                  char *nm = o ? a5text_object_name (st, o, A5_ART_NONE)
                               : strdup (st->ref_items[i]);
                  if (i > 0)
                    sb_puts (&sb, (i == st->n_ref_items - 1) ? " and " : ", ");
                  sb_puts (&sb, nm);
                  free (nm);
                }
              return sb_finish (&sb);
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
            return NULL;
          const char *ref = ci_eq (name, "object2") ? "ReferencedObject2"
                                                     : "ReferencedObject";
          const char *key = a5state_lookup_ref (st, ref);
          const a5_object_t *o = key ? a5model_object (st->adv, key) : NULL;
          if (o != NULL)
            return strdup (o->key);
        }
      else if (ci_eq (name, "character") || ci_eq (name, "characters")
               || ci_eq (name, "character1") || ci_eq (name, "character2")
               || ci_eq (name, "character3") || ci_eq (name, "character4")
               || ci_eq (name, "character5"))
        {
          const char *cref = "ReferencedCharacter";
          char cbuf[24];
          if (isdigit ((unsigned char) name[strlen (name) - 1])
              && name[strlen (name) - 1] != '1')
            { snprintf (cbuf, sizeof cbuf, "ReferencedCharacter%c",
                        name[strlen (name) - 1]);
              cref = cbuf; }
          const char *key = a5state_lookup_ref (st, cref);
          const a5_character_t *ch = key ? a5model_character (st->adv, key) : NULL;
          if (ch != NULL && ch->name != NULL)
            return strdup (ch->name);
        }
      if (bound != NULL)
        return strdup (bound);
    }

  /* A bare %VariableName% (or %Score% etc.). */
  if (args == NULL)
    {
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
    }
  /* An array-variable element: %VariableName[index]% (1-based, mirroring
     ADRIFT's clsVariable string/int arrays).  A Text array stores its elements
     newline-separated in the single var_text string (the InitialValue layout),
     so split on '\n' and return the index'th line; out-of-range -> empty. */
  else
    {
      for (i = 0; i < st->adv->n_variables; i++)
        if (ci_eq (st->adv->variables[i].name, name))
          {
            char *end = NULL;
            long idx = strtol (args, &end, 10);
            while (end != NULL && (*end == ' ' || *end == '\t'))
              end++;
            /* A non-literal index (e.g. %notallowed[RAND(1,6)]%) must be
               evaluated as an expression -- the runner's clsVariable indexing runs the
               bracket contents through EvaluateExpression.  Only fall through to
               the plain-literal fast path when args is exactly an integer. */
            if (end == args || (end != NULL && *end != '\0'))
              {
                char *ev = a5text_eval_expression (st, args);
                char *e2 = NULL;
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
    }

  return NULL;                  /* unhandled: leave the token verbatim */
}

/* Replace %function%/%variable% tokens in src (single pass; args recurse). */
/* Capture a pending CharacterName render as a PronounKeys entry at output
   offset `off` -- the ledger half of the runner's Global.vb:2108 (the eval side sets
   st->pron_pending; ReplaceFunctions adds in expression mode too). */
static void
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
  st->pron_pending_key = NULL;
  st->pron_pending_pron = 0;
}

static char *str_replace_all (const char *src, const char *find,
                              const char *repl);
static int expr_bears_random (const char *body);

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

  if (strchr (src, '%') == NULL)
    return cur;                         /* nothing to substitute */

  for (vi = 0; vi < st->adv->n_variables; vi++)
    {
      const char *vname = st->adv->variables[vi].name;
      size_t vlen;
      int guard = 0;

      if (vname == NULL || vname[0] == '\0')
        continue;
      vlen = strlen (vname);

      while (guard++ < 4096)
        {
          char *hit = NULL, *scan = cur;
          const char *after;
          char *name, *rawargs = NULL, *value = NULL, *token;
          char *tok_end;

          /* Find the next "%vname" (case-insensitive) delimited by '%' or '['. */
          while ((scan = strchr (scan, '%')) != NULL)
            {
              const char *nm = scan + 1;
              if (strncasecmp (nm, vname, vlen) == 0
                  && (nm[vlen] == '%' || nm[vlen] == '['))
                { hit = scan; break; }
              scan++;
            }
          if (hit == NULL)
            break;

          after = hit + 1 + vlen;
          name = strndup (hit + 1, vlen);
          if (*after == '%')
            {
              tok_end = (char *) after + 1;
              value = eval_function (st, name, NULL);
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
              if (st->expr_defer != NULL && expr_bears_random (rawargs))
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
                  if (strchr (args, '.') != NULL)
                    {
                      char *ooargs = a5expr_replace (st, args);
                      free (args); args = ooargs;
                    }
                  value = eval_function (st, name, args);
                  free (args);
                }
            }

          if (value == NULL)             /* not resolvable here */
            { free (name); free (rawargs); break; }

          token = strndup (hit, (size_t) (tok_end - hit));
          {
            char *next = str_replace_all (cur, token, value);
            free (cur); cur = next;
          }
          /* Guard against a value that re-introduces the same token. */
          if (strstr (value, token) != NULL)
            { free (token); free (name); free (rawargs); free (value); break; }
          free (token); free (name); free (rawargs); free (value);
        }
    }
  return cur;
}

static char *
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
          char *name, *args = NULL, *value = NULL;
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
                      if (fk != NULL)
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
                      if (fk != NULL)
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
                      if (ci_eq (name, "object") || ci_eq (name, "objects")
                          || ci_eq (name, "object1") || ci_eq (name, "object2")
                          || ci_eq (name, "object3") || ci_eq (name, "object4")
                          || ci_eq (name, "object5") || ci_eq (name, "character")
                          || ci_eq (name, "characters") || ci_eq (name, "character1")
                          || ci_eq (name, "character2") || ci_eq (name, "character3")
                          || ci_eq (name, "character4") || ci_eq (name, "character5"))
                        {
                          free (name);
                          p = q + 1;
                          continue;        /* substitute "" */
                        }
                    }
                  value = eval_function (st, name, NULL);
                  free (name);
                  q++;
                  ok = 1;
                }
              else if (*q == '[')
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
                  if (*a == ']' && a[1] == '%')
                    {
                      char *rawargs = strndup (astart, (size_t) (a - astart));
                      name = strndup (name_start, nlen);
                      args = replace_functions (st, rawargs, 1);
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
                      if (strchr (args, '.') != NULL)
                        {
                          char *ooargs = a5expr_replace (st, args);
                          free (args);
                          args = ooargs;
                        }
                      value = eval_function (st, name, args);
                      free (rawargs);
                      free (args);
                      free (name);
                      q = a + 2;
                      ok = 1;
                    }
                }
            }

          if (ok)
            {
              if (value != NULL)
                {
                  /* CharacterName sentence-start fudge (the runner Global.vb:2103): when
                     the resolved name is immediately preceded by two spaces or a
                     CRLF, capitalise its first character.  iMatchLoc>3 there means
                     at least 3 chars precede the token (sb.len >= 3 here). */
                  if (st->name_cap_eligible && sb.len >= 3 && value[0] != '\0'
                      && ((sb.p[sb.len - 2] == ' ' && sb.p[sb.len - 1] == ' ')
                          || (sb.p[sb.len - 2] == '\r' && sb.p[sb.len - 1] == '\n')))
                    value[0] = (char) toupper ((unsigned char) value[0]);
                  st->name_cap_eligible = 0;
                  pron_capture (st, (long) sb.len);
                  sb_puts (&sb, value);
                  free (value);
                }
              else
                {
                  /* leave the original token verbatim -- but the runner rewrites the
                     bare %object% alias to %object1% BEFORE resolving
                     (ReplaceIgnoreCase, Global.vb:1754), so an unresolved
                     singular on a plural %objects% command surfaces as
                     "%object1%..." (the Blender's "You put %object%.Name on
                     the table." task text). */
                  if ((size_t) (q - p) == 8 && strncmp (p, "%object%", 8) == 0)
                    sb_puts (&sb, "%object1%");
                  else
                    sb_putn (&sb, p, (size_t) (q - p));
                }
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

/* --------------------------------------------------------- ALR replacement */

static char *process_inner (a5_state_t *st, const char *src, int depth);

/* Replace every occurrence of `find` in `src` with `repl`. */
static char *
str_replace_all (const char *src, const char *find, const char *repl)
{
  sb_t sb;
  size_t flen = strlen (find);
  const char *p = src;
  sb_init (&sb);
  if (flen == 0)
    return strdup (src);
  while (*p)
    {
      const char *hit = strstr (p, find);
      if (hit == NULL)
        {
          sb_puts (&sb, p);
          break;
        }
      sb_putn (&sb, p, (size_t) (hit - p));
      sb_puts (&sb, repl);
      p = hit + flen;
    }
  return sb_finish (&sb);
}

static char *
replace_alrs (a5_state_t *st, const char *src, int depth)
{
  char *cur = strdup (src);
  int i;
  if (depth > 8)
    return cur;
  for (i = 0; i < st->adv->n_alrs; i++)
    {
      const a5_alr_t *alr = &st->adv->alrs[i];
      if (alr->old_text == NULL || alr->old_text[0] == '\0')
        continue;
      if (strstr (cur, alr->old_text) != NULL)
        {
          if (getenv ("A5_TRACE_ALR"))
            fprintf (stderr, "[ALR fragment] hit old=\"%s\" in \"%.60s\"\n",
                     alr->old_text, cur);
          /* Defer bare %variable% tokens inside the NewText to the Display
             boundary (A5_VARDEF_MARK): the runner only expands ALRs at Display time,
             so their variables read the end-of-turn state.  The boundary pass
             (a5text_display_alr) expands eagerly -- it IS the boundary. */
          int prev_defer = st->alr_defer_vars;
          st->alr_defer_vars = 1;
          char *raw = a5text_eval_description (st, alr->new_text);
          /* Mirror the runner's `If sText = sALR Then Exit For` (Global.vb:532): when the
             whole current text already equals the (unprocessed) replacement, stop
             processing ALRs entirely -- crucially BEFORE recursing.  This is what
             terminates identity/self-referential ALRs (e.g. this game's ~25 copies
             of a 25-asterisk banner line whose OldText == NewText): without it,
             process_inner recurses on the replacement, which re-matches all the
             identity ALRs, branching ~N-fold per level down to the depth-8 cap. */
          if (streq (cur, raw))
            {
              free (raw);
              st->alr_defer_vars = prev_defer;
              break;
            }
          char *val = process_inner (st, raw, depth + 1);
          st->alr_defer_vars = prev_defer;
          char *next = str_replace_all (cur, alr->old_text, val);
          free (cur);
          cur = next;
          free (raw);
          free (val);
        }
    }
  return cur;
}

/* ------------------------------------------------------ auto-capitalisation */

/* Mirror the Adrift 5 runner's CapAfterFullStop regex
   "^(?<cap>[a-z])|\n(?<cap>[a-z])|[a-z][\.\!\?] ( )?(?<cap>[a-z])"
   (Global.vb:539).  The third alternative requires a *lowercase letter*
   immediately before the .!? so an ellipsis ("...") or a digit/upper char
   before the stop does NOT start a new sentence. */
#define A5_IS_LOWER(c) ((c) >= 'a' && (c) <= 'z')

static int
is_sentence_start (const char *s, size_t i, int allow_line_start)
{
  /* The `^` and `\n` alternatives (start-of-text / start-of-line).  The runner applies
     CapAfterFullStop to still-MARKED-UP text, where a formatting tag (<del>,
     <c>, <b>, <font...>) between the line break and the letter blocks these two
     alternatives (the regex sees the tag's '>' immediately before the letter,
     not '^'/'\n').  Scarier's per-fragment a5text_process runs the full cap on
     marked-up text too, so intra-fragment line starts are already handled there;
     the Display-boundary re-cap (a5text_display_alr) runs on PLAIN text, where
     those tags are gone, so honouring `^`/`\n` there wrongly capitalises a letter
     the runner left alone (e.g. Call of the Shaman's `<del>https://...` credits URLs).
     The boundary therefore passes allow_line_start=0 -- only real sentence
     punctuation (which no tag can hide) drives a cap in the second pass. */
  if (allow_line_start)
    {
      if (i == 0)
        return 1;
      if (s[i - 1] == '\n')
        return 1;
    }
  /* "x. y" where x is a lowercase letter */
  if (i >= 3 && s[i - 1] == ' '
      && (s[i - 2] == '.' || s[i - 2] == '!' || s[i - 2] == '?')
      && A5_IS_LOWER (s[i - 3]))
    return 1;
  /* "x.  y" (two spaces) where x is a lowercase letter */
  if (i >= 4 && s[i - 1] == ' ' && s[i - 2] == ' '
      && (s[i - 3] == '.' || s[i - 3] == '!' || s[i - 3] == '?')
      && A5_IS_LOWER (s[i - 4]))
    return 1;
  return 0;
}

static char *
auto_capitalise_ex (const char *src, int allow_line_start)
{
  char *s = strdup (src);
  size_t i;
  for (i = 0; s[i]; i++)
    if (islower ((unsigned char) s[i]) && is_sentence_start (s, i, allow_line_start))
      s[i] = toupper ((unsigned char) s[i]);
  return s;
}

static char *
auto_capitalise (const char *src)
{
  return auto_capitalise_ex (src, 1);
}

int
a5text_is_sentence_start (const char *text, size_t pos)
{
  return is_sentence_start (text, pos, 1);
}

/* --------------------------------------------------- perspective conjugation */

/* Player perspective -> index into a [1st/2nd/3rd] verb-ending bracket.
   The CURRENT player (BECOME retargets it) through the same clsCharacter
   .Perspective getter char_perspective mirrors -- The Salvage's NonPlayer
   captain conjugates third person ("Captain Pearson opens the Crate"). */
static int
perspective_index (a5_state_t *st)
{
  const a5_character_t *p = a5model_character (st->adv,
                                               a5state_player_key (st));
  return char_perspective (st, p) - 1;
}

/* The runner GetPerspective (Global.vb:2481): the PronounKeys entry with the highest
   offset at or before `pos` decides the perspective of a conjugation bracket
   there; iHighest starts at 0, so an entry at offset 0 can never win.  0 =
   no preceding rendered name (caller falls back to the player). */
static int
perspective_at (const a5_state_t *st, long pos)
{
  int persp = 0;
  long highest = 0;
  for (int i = 0; i < st->n_pron; i++)
    if (pos >= st->pron_off[i] && st->pron_off[i] > highest)
      {
        persp = st->pron_persp[i];
        highest = st->pron_off[i];
      }
  return persp;
}

/*
 * Resolve ADRIFT verb-conjugation brackets: "move[//s]" -> "move"/"moves" by the
 * perspective of the nearest preceding rendered character name (PronounKeys),
 * defaulting to the player's ("The medic [am/are/is] wearing ..." -> "is", but
 * "you [am/are/is] carrying" with no name before it -> "are").  Only a "[a/b/c]"
 * group (slash-separated, no nested markup) is touched; everything else passes
 * through.  Mirrors the perspective half of the Adrift 5 runner Global.ReplaceFunctions
 * / _FrankenDrift_Pronouns.
 */
static char *
resolve_perspective (a5_state_t *st, const char *src)
{
  sb_t sb;
  const char *p = src;
  int pidx = perspective_index (st);

  sb_init (&sb);
  while (*p != '\0')
    {
      if (*p == '[')
        {
          /* The runner's rePerspective regex is \[(first)/(second)/(third)\]: the
             bracket must hold at least TWO slashes (split at the first two;
             the third part is the rest, slashes and all) and lie on one line
             ('.' doesn't span \n).  A one-slash bracket is NOT a perspective
             group -- GFS's "[more (<u>more</u> means ...)]" and "[type ...
             <b>E</b> to exit]" pass through literally (the tags render away
             later). */
          const char *q = p + 1;
          const char *s1 = NULL, *s2 = NULL;
          while (*q && *q != ']' && *q != '[' && *q != '\n')
            {
              if (*q == '/')
                { if (s1 == NULL) s1 = q; else if (s2 == NULL) s2 = q; }
              q++;
            }
          if (*q == ']' && s2 != NULL)
            {
              const char *a, *b;
              int np = perspective_at (st, (long) (p - src));
              int idx = np ? np - 1 : pidx;
              if (idx == 0)      { a = p + 1;  b = s1; }
              else if (idx == 1) { a = s1 + 1; b = s2; }
              else               { a = s2 + 1; b = q;  }
              for (const char *c = a; c < b; c++)
                sb_putc (&sb, *c);
              p = q + 1;
              continue;
            }
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

/* ------------------------------------------------- embedded <#...#> expressions */

/* A signed decimal integer? (the Adrift 5 runner's bIntValue -- expression results
   that are pure integers are left unquoted; everything else is quoted.) */
static int
is_signed_int (const char *s)
{
  if (s == NULL || *s == '\0')
    return 0;
  const char *p = s;
  if (*p == '+' || *p == '-') p++;
  if (*p == '\0') return 0;
  for (; *p; p++)
    if (!isdigit ((unsigned char) *p))
      return 0;
  return 1;
}

/* Emit a substituted value into `sb`, quoting it (the Adrift 5 runner's bExpression
   path) unless it is a bare integer or we are already inside a quoted literal. */
static void
sb_quote_val (sb_t *sb, const char *val, int in_quote)
{
  if (in_quote || is_signed_int (val))
    sb_puts (sb, val);
  else
    { sb_putc (sb, '"'); sb_puts (sb, val); sb_putc (sb, '"'); }
}

/*
 * Substitute the %references%, %variables% and OO property-expressions inside a
 * `<#...#>` expression body, quoting non-numeric results so the evaluator sees a
 * self-contained expression.  Ports the Adrift 5 runner's ReplaceFunctions(expr, True)
 * + ReplaceOO(bExpression:=True) (Global.vb:639-647): a substituted value is
 * wrapped in double quotes unless it is an integer or already lies between
 * quotes in the source.
 */
static char *
expr_substitute (a5_state_t *st, const char *src)
{
  sb_t sb;
  const char *p = src;
  int in_quote = 0;

  sb_init (&sb);
  while (*p != '\0')
    {
      if (*p == '"')
        { in_quote = !in_quote; sb_putc (&sb, '"'); p++; continue; }
      if (*p == '%')
        {
          const char *q = p + 1;
          const char *name_start = q;
          while (*q && (isalnum ((unsigned char) *q) || *q == '_' || *q == '-')) q++;
          if (q > name_start && *q == '%')
            {
              size_t nlen = (size_t) (q - name_start);
              char *name = strndup (name_start, nlen);
              /* %reference%.Property... : resolve the OO chain (quoted) */
              if (q[1] == '.')
                {
                  char *fk = oo_firstkey (st, name);
                  if (fk != NULL)
                    {
                      const char *chain_end = a5expr_scan_chain (q + 1);
                      char *chain = strndup (q + 1, (size_t) (chain_end - (q + 1)));
                      int flist = ci_eq (name, "objects")
                                  || ci_eq (name, "characters");
                      char *val = a5expr_eval (st, fk, chain, flist);
                      sb_quote_val (&sb, val ? val : "", in_quote);
                      free (val); free (chain); free (fk); free (name);
                      p = chain_end;
                      continue;
                    }
                }
              /* plain %name% */
              {
                char *val = eval_function (st, name, NULL);
                free (name);
                if (val != NULL)
                  { pron_capture (st, (long) sb.len);
                    sb_quote_val (&sb, val, in_quote); free (val); p = q + 1; continue; }
              }
              /* unresolved: leave the %name% token verbatim */
              sb_putn (&sb, p, (size_t) (q + 1 - p));
              p = q + 1;
              continue;
            }
          if (q > name_start && *q == '[')
            {
              int depth = 1;
              const char *a = q + 1;
              const char *astart = a;
              while (*a && depth > 0)
                { if (*a == '[') depth++; else if (*a == ']') depth--; if (depth > 0) a++; }
              if (*a == ']' && a[1] == '%')
                {
                  char *rawargs = strndup (astart, (size_t) (a - astart));
                  char *name = strndup (name_start, (size_t) (q - name_start));
                  char *args = replace_functions (st, rawargs, 1);
                  char *val = eval_function (st, name, args);
                  free (rawargs); free (args); free (name);
                  if (val != NULL)
                    { pron_capture (st, (long) sb.len);
                      sb_quote_val (&sb, val, in_quote); free (val); p = a + 2; continue; }
                  /* unresolved: leave verbatim */
                  sb_putn (&sb, p, (size_t) (a + 2 - p));
                  p = a + 2;
                  continue;
                }
            }
          /* a lone '%' */
          sb_putc (&sb, '%');
          p++;
          continue;
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

/* Does this `<#...#>` body invoke one of the RNG-drawing reducer functions
   (either/oneof/rand/urand)?  Matched as a call: the function name (case-
   insensitive), then optional spaces, then '(' -- so a bare word "random" in a
   string literal does not falsely trigger.  Used to gate the deferred-draw
   sentinel: only a body that actually draws needs its evaluation held to
   end-of-command (a non-random `<#IF(..)#>` must keep resolving inline, at its
   emit-time state). */
static int
expr_bears_random (const char *body)
{
  static const char *const fns[] = { "oneof", "either", "urand", "rand" };
  for (const char *p = body; *p != '\0'; p++)
    {
      for (size_t f = 0; f < sizeof fns / sizeof fns[0]; f++)
        {
          size_t n = strlen (fns[f]);
          if (strncasecmp (p, fns[f], n) != 0)
            continue;
          const char *q = p + n;
          while (*q == ' ' || *q == '\t')
            q++;
          if (*q == '(')
            return 1;
        }
    }
  return 0;
}

/* Would SUBSTITUTING this `<#...#>` body draw?  True when it holds a
   `%name[args]%` reference whose args invoke an RNG function -- OS/PlugIn.Exe
   deals its cards with `<#if(%bjValue[1]%=10, %cardPicture[RAND(1, 4)]%, ..)#>`.
   Such a body cannot be frozen operand-first (the usual deferral): the draw
   would land at render time, ahead of the `%cardType[RAND(1, 4)]%` that follows
   it in the text, while the runner -- which holds the WHOLE aggregate message to
   Display -- draws them in text order.  So defer the raw body instead. */
static int
expr_sub_draws_random (const char *body)
{
  for (const char *p = body; *p != '\0'; p++)
    {
      if (*p != '%')
        continue;
      const char *q = p + 1;
      while (*q != '\0' && (isalnum ((unsigned char) *q) || *q == '_'
                            || *q == '-'))
        q++;
      if (q == p + 1 || *q != '[')
        continue;
      const char *cb = strchr (q, ']');
      if (cb == NULL || cb[1] != '%')
        continue;
      {
        char *args = strndup (q + 1, (size_t) (cb - q - 1));
        int r = expr_bears_random (args);
        free (args);
        if (r)
          return 1;
      }
      p = cb;
    }
  return 0;
}

/* Evaluate every `<#...#>` expression in `src` (the Adrift 5 runner ReplaceExpressions:
   substitute the body, then reduce it to its string value). */
static char *
replace_expressions (a5_state_t *st, const char *src)
{
  sb_t sb;
  const char *p = src;

  if (strstr (src, "<#") == NULL)
    return strdup (src);

  sb_init (&sb);
  while (*p != '\0')
    {
      if (p[0] == '<' && p[1] == '#')
        {
          const char *end = strstr (p + 2, "#>");
          if (end != NULL)
            {
              char *inner = strndup (p + 2, (size_t) (end - (p + 2)));
              if (st->expr_defer != NULL && expr_sub_draws_random (inner))
                {
                  /* Raw-body deferral (see expr_sub_draws_random): push the
                     UNSUBSTITUTED body, tagged \003, so both its substitution
                     draws and its reduce happen at the end-of-command flush,
                     in this block's text position. */
                  std::vector<std::string> *sink =
                      (std::vector<std::string> *) st->expr_defer;
                  char mark[24];
                  snprintf (mark, sizeof mark, "\004%d\004", (int) sink->size ());
                  sink->push_back (std::string ("\003") + inner);
                  sb_puts (&sb, mark);
                  free (inner);
                  p = end + 2;
                  continue;
                }
              char *sub = expr_substitute (st, inner);
              /* Resolve any BARE OO-chain (no leading %) left in the body, e.g.
                 `<#LCASE(cl_Door1.OpenStatus)#>` in a room description --
                 expr_substitute only handles %ref%.Prop, and the outer
                 a5expr_replace pass skipped this body because protect_exprs hid
                 it.  Mirrors a5text_eval_expression's second pass (the runner's
                 EvaluateExpression -> ReplaceFunctions includes ReplaceOO).  A
                 `<#..#>` body IS an expression, so use the EXPRESSION-mode
                 replace (bExpression=True): a multi-word OO value like
                 `CrashedShi.Location.Name` ("Ocean M053") must be emitted as a
                 quoted string literal, or the sexpr tokeniser splits it at the
                 space and drops the tail -- and a bare concat argument to
                 `if(..)` then reduces to nothing, blanking AlienDiver's "Crashed
                 Ship Location:" status line. */
              char *oo = a5expr_replace_expr (st, sub);
              /* Deferred draw: an AggregateOutput completion message renders its
                 static skeleton NOW (correct position and separators) but holds
                 the RNG draw of a random `<#..#>` to end-of-command, matching
                 the runner's Display-time ReplaceExpressions (so Lost Coastlines'
                 `Execute EnemyIsMer` ship-name OneOf draws AFTER the following
                 `Execute ShipCombat` action draws, not inline before them).  The
                 operands are frozen here (oo is self-contained) so only the
                 reduce moves; a `\004<idx>\004` sentinel marks the value slot. */
              if (st->expr_defer != NULL && expr_bears_random (oo))
                {
                  std::vector<std::string> *sink =
                      (std::vector<std::string> *) st->expr_defer;
                  char mark[24];
                  snprintf (mark, sizeof mark, "\004%d\004", (int) sink->size ());
                  sink->push_back (oo);
                  sb_puts (&sb, mark);
                  free (inner); free (sub); free (oo);
                  p = end + 2;
                  continue;
                }
              char *val = a5_eval_sexpr (oo);
              sb_puts (&sb, val);
              free (inner); free (sub); free (oo); free (val);
              p = end + 2;
              continue;
            }
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

/* Evaluate a raw expression string to its string value, mirroring
   Global.EvaluateExpression -> clsVariable.SetToExpression: substitute the
   %references%/%variables%/OO chains (bExpression quoting), then reduce.  Used
   for <Expression>-type task restrictions and SetVariable-to-expression.
   Returns a heap string the caller frees; never NULL. */
char *
a5text_eval_expression (a5_state_t *st, const char *expr)
{
  char *sub, *val;
  if (expr == NULL)
    return strdup ("");
  sub = expr_substitute (st, expr);
  /* Resolve any BARE OO-chain (no leading %), e.g. `Event12.Position` in a
     rain-gate restriction -- expr_substitute only handles %ref%.Prop, so the
     bare-key form needs the same ReplaceOO pass that process_inner runs on
     display text (the runner's EvaluateExpression -> ReplaceFunctions includes it).
     Expression mode: string values are quoted (Global.vb:645). */
  {
    char *oo = a5expr_replace_expr (st, sub);
    free (sub);
    sub = oo;
  }
  val = a5_eval_sexpr (sub);
  /* A5_TRACE_EXPR=1 shows each restriction/SetVariable expression as raw ->
     substituted -> value; the substituted form is where most divergences from
     the runner live (an OO chain that resolved to "" instead of "0", say). */
  if (getenv ("A5_TRACE_EXPR") != NULL)
    fprintf (stderr, "A5_EXPR: %s  ==>  %s  ==>  %s\n", expr, sub, val);
  free (sub);
  return val;   /* a5_eval_sexpr never returns NULL */
}

/* Swap each `<#...#>` for a \x01<n>\x01 sentinel so the %function%/OO passes
   leave the expression body untouched (the Adrift 5 runner guards them behind GUIDs);
   the sentinel survives both passes verbatim and is restored before
   replace_expressions evaluates the bodies. */
static std::string
protect_exprs (const char *src, std::vector<std::string> &saved)
{
  std::string out;
  const char *p = src;
  while (*p != '\0')
    {
      if (p[0] == '<' && p[1] == '#')
        {
          const char *end = strstr (p + 2, "#>");
          if (end != NULL)
            {
              char buf[24];
              snprintf (buf, sizeof buf, "\x01%d\x01", (int) saved.size ());
              saved.push_back (std::string (p, (size_t) (end + 2 - p)));
              out += buf;
              p = end + 2;
              continue;
            }
        }
      out += *p++;
    }
  return out;
}

static std::string
restore_exprs (const char *src, const std::vector<std::string> &saved)
{
  std::string out;
  const char *p = src;
  while (*p != '\0')
    {
      if (*p == '\x01')
        {
          const char *q = p + 1;
          while (isdigit ((unsigned char) *q)) q++;
          if (*q == '\x01' && q > p + 1)
            {
              int idx = atoi (p + 1);
              if (idx >= 0 && idx < (int) saved.size ())
                out += saved[idx];
              p = q + 1;
              continue;
            }
        }
      out += *p++;
    }
  return out;
}

/* ------------------------------------------------------------- process core */

static char *
process_inner_ex (a5_state_t *st, const char *src, int depth, int *pre_alr_ink)
{
  std::vector<std::string> saved;
  std::string prot = protect_exprs (src, saved);
  char *funcs = replace_functions (st, prot.c_str ());
  char *oo = a5expr_replace (st, funcs);    /* resolve key.Property expressions */
  std::string restored = restore_exprs (oo, saved);
  char *exprs = replace_expressions (st, restored.c_str ());  /* <#...#> */
  char *alrs;
  free (funcs);
  free (oo);
  if (pre_alr_ink != NULL)
    {
      /* Did the text have visible content BEFORE the ALR pass?  the runner stores task
         responses at this stage (functions/expressions replaced, ALRs not yet
         -- ReplaceALRs runs inside Display, vb:308) and its bHasOutput keeps
         the message on this form.  A game ALR that maps a phrase to nothing
         (Tribute's `(from the enormous mirror)` TextOverride) therefore blanks
         the text only AFTER its response slot exists, leaving an empty
         paragraph where Scarier's post-ALR whitespace test would drop the
         whole message. */
      char *pp = a5text_render_plain (exprs);
      const char *q = pp;
      *pre_alr_ink = 0;
      /* The interactive-mode presentation marks stand for the same stripped
         tags an A5_ALR_MARK does headlessly, so they are not ink either --
         the ink verdict must come out identical in both modes. */
      for (; *q; q++)
        {
          if (*q == A5_IMG_MARK || *q == A5_WINDOW_MARK || *q == A5_SOUND_MARK
              || *q == A5_WAIT_MARK)
            {
              /* Skip the \006<number>\006 / \022<name>\022 / \024<index>\024
                 / \026<seconds>\026 span (the window name is a routing tag,
                 not visible ink), or a stray unpaired mark. */
              const char *e = strchr (q + 1, *q);
              if (e == NULL)
                continue;
              q = e;
            }
          else if (*q != '\n' && *q != '\r' && *q != ' ' && *q != '\t'
                   && *q != A5_ALR_MARK && *q != A5_WAITKEY_MARK
                   && *q != A5_CENTER_MARK && *q != A5_ENDCENTER_MARK
                   && *q != A5_BOLD_MARK && *q != A5_ENDBOLD_MARK
                   && *q != A5_ENDWINDOW_MARK)
            { *pre_alr_ink = 1; break; }
        }
      free (pp);
    }
  if (depth > 8)
    return exprs;
  alrs = replace_alrs (st, exprs, depth);
  free (exprs);
  return alrs;
}

/* Seal a text's ALR adjudication.  The runner runs ReplaceALRs once per Display()
   call (clsUserSession.vb:308) and never revisits that text, so any OldText
   occurrence still present after an emit-time ALR pass -- an override whose
   restriction-gated alternative rendered IDENTICAL to its OldText (XXR's
   cl_EenyMeenyA camel-tether postfix while the camels are untethered), or one
   materialised by another ALR's replacement -- is DEAD: the Display-boundary
   pass (a5text_display_alr) must not re-evaluate it with end-of-turn state,
   after the turn's tasks have run.  Insert an A5_ALR_MARK sentinel after the
   first byte of each surviving site so the boundary strstr cannot match
   across it; the boundary still catches OldText spans assembled ACROSS
   separately-emitted texts (which carry no mark), and finish_turn strips the
   sentinels from the output. */
static char *
seal_alr_sites (a5_state_t *st, char *text)
{
  int i;
  if (st == NULL || st->adv == NULL || st->adv->n_alrs == 0 || text == NULL)
    return text;
  for (i = 0; i < st->adv->n_alrs; i++)
    {
      const a5_alr_t *alr = &st->adv->alrs[i];
      const char *old = alr->old_text;
      if (old == NULL || old[0] == '\0' || strstr (text, old) == NULL)
        continue;
      size_t ol = strlen (old);
      sb_t sb;
      const char *p = text;
      sb_init (&sb);
      while (*p != '\0')
        {
          if (strncmp (p, old, ol) == 0)
            {
              sb_putc (&sb, old[0]);
              sb_putc (&sb, A5_ALR_MARK);
              sb_puts (&sb, old + 1);
              p += ol;
            }
          else
            sb_putc (&sb, *p++);
        }
      free (text);
      text = sb_finish (&sb);
    }
  return text;
}

static char *
process_inner (a5_state_t *st, const char *src, int depth)
{
  return process_inner_ex (st, src, depth, NULL);
}


char *
a5text_process (a5_state_t *st, const char *src)
{
  char *inner = process_inner (st, src, 0);
  char *persp = resolve_perspective (st, inner);
  char *capped = auto_capitalise (persp);
  free (inner);
  free (persp);
  return capped;
}

char *
a5text_process_nocap (a5_state_t *st, const char *src)
{
  char *inner = process_inner (st, src, 0);
  char *persp = resolve_perspective (st, inner);
  free (inner);
  return persp;
}

char *
a5text_process_noalr (a5_state_t *st, const char *src)
{
  /* %functions%/OO/<#exprs#> WITHOUT the ALR pass: for evaluating ACTION
     values.  The runner's SetVariable path goes through EvaluateExpression
     (ReplaceFunctions only) -- ReplaceALRs runs at Display time (vb:308)
     and must not rewrite stored values.  GFS defines a display ALR
     "17000" -> "1.700.0" that would otherwise corrupt
     `IncVariable cl_Money = "1700000"` into 1.  No auto-capitalisation
     either (that is Display-time too). */
  std::vector<std::string> saved;
  std::string prot = protect_exprs (src ? src : "", saved);
  char *funcs = replace_functions (st, prot.c_str ());
  char *oo = a5expr_replace (st, funcs);
  std::string restored = restore_exprs (oo, saved);
  char *exprs = replace_expressions (st, restored.c_str ());
  free (funcs);
  free (oo);
  return exprs;
}

/* ------------------------------------------------------- media side channel */

static a5_media_cb a5_media_sink = NULL;
static void *a5_media_sink_ctx = NULL;

void
a5text_set_media_sink (a5_media_cb cb, void *ctx)
{
  a5_media_sink = cb;
  a5_media_sink_ctx = ctx;
}

/* Interactive-presentation mode: keep <cls>/<waitkey>/<img> as positional
   marks for the host instead of pre-resolving them (see a5text.h). */
static int a5_interactive_mode = 0;

void
a5text_set_interactive (int on)
{
  a5_interactive_mode = on;
}

int
a5text_interactive (void)
{
  return a5_interactive_mode;
}

/* ---------------------------------------------------- PopUpInput side channel */

void
a5text_set_popup_cb (a5_popup_cb cb, void *ctx)
{
  a5_popup = cb;
  a5_popup_ctx = ctx;
}

/* Parse an <img>/<audio> tag body (without the angle brackets) and report it to
   the installed media sink: src="..." (image/sound file), and for <audio> the
   play/stop verb, channel=N and an optional loop flag.  Returns the sink's
   resolved resource number for an image, its recorded-event slot for a sound
   (see a5_media_cb), -1 when the sink has nothing for the caller to mark. */
static int
a5_emit_media (const std::string &tag, int is_img)
{
  std::string src;
  size_t sp = tag.find ("src=");
  if (sp != std::string::npos)
    {
      sp += 4;
      char quote = 0;
      if (sp < tag.size () && (tag[sp] == '"' || tag[sp] == '\''))
        quote = tag[sp++];
      while (sp < tag.size ()
             && (quote ? tag[sp] != quote : (tag[sp] != ' ' && tag[sp] != '>')))
        src.push_back (tag[sp++]);
    }

  if (is_img)
    return a5_media_sink (a5_media_sink_ctx, A5_MEDIA_IMAGE, src.c_str (), 0, 0);

  /* <audio>: play (default), stop or pause, with channel=N and an optional
     loop flag.  This mirrors the Runner's tag parse (Global.vb, DisplayText):
     the verb is found by substring (" pause" / " stop", else play), the
     channel DEFAULTS TO 1 when there is no channel= attribute, and attribute
     values may be quoted.  The channel-1 default matters: games freely mix
     bare <audio play src=...> with <audio ... channel=1>, and both must land
     on the same channel so a new background track replaces the old one
     instead of layering over it. */
  {
    std::string low = tag;
    for (size_t k = 0; k < low.size (); k++)
      low[k] = (char) tolower ((unsigned char) low[k]);
    int is_pause = low.find (" pause") != std::string::npos;
    int is_stop = !is_pause && low.find (" stop") != std::string::npos;
    /* Loop only when explicitly loop=Y / loop=1 / loop=True.  ADRIFT writes
       loop=Y / loop=N, so a bare substring test for "loop" wrongly loops the
       (common) loop=N case forever. */
    int loop = 0;
    size_t lp = low.find ("loop=");
    if (lp != std::string::npos)
      {
        lp += 5;
        if (lp < low.size () && (low[lp] == '"' || low[lp] == '\''))
          lp++;
        loop = lp < low.size ()
               && (low[lp] == 'y' || low[lp] == '1' || low[lp] == 't');
      }
    int channel = 1;
    size_t cp = low.find ("channel=");
    if (cp != std::string::npos)
      {
        cp += 8;
        if (cp < tag.size () && (tag[cp] == '"' || tag[cp] == '\''))
          cp++;
        channel = atoi (tag.c_str () + cp);
      }
    if (is_pause)
      return a5_media_sink (a5_media_sink_ctx, A5_MEDIA_SOUND_PAUSE, NULL,
                            channel, 0);
    else if (is_stop)
      return a5_media_sink (a5_media_sink_ctx, A5_MEDIA_SOUND_STOP, NULL,
                            channel, 0);
    else
      return a5_media_sink (a5_media_sink_ctx, A5_MEDIA_SOUND, src.c_str (),
                            channel, loop);
  }
}

/* ----------------------------------------------------------- plain renderer */

char *
a5text_render_plain (const char *src)
{
  sb_t sb;
  const char *p = src;
  sb_init (&sb);

  while (*p != '\0')
    {
      if (*p == '<')
        {
          const char *q = p + 1;
          char tag[16];
          size_t tl = 0;
          while (*q && *q != '>')
            {
              if (tl + 1 < sizeof tag)
                tag[tl++] = (char) tolower ((unsigned char) *q);
              q++;
            }
          tag[tl] = '\0';
          const char *tagend = q;     /* at '>' or '\0' */
          if (*q == '>')
            q++;
          /* Tag name = the leading non-space run (img, audio, br, cls, ...). */
          char name[16];
          {
            size_t k = 0;
            while (tag[k] != '\0' && tag[k] != ' ' && k + 1 < sizeof name)
              { name[k] = tag[k]; k++; }
            name[k] = '\0';
          }
          if (strcmp (name, "br") == 0)
            sb_putc (&sb, '\n');
          else if (strcmp (name, "cls") == 0)
            {
              /* Screen clear: drop everything buffered so far in THIS fragment,
                 mirroring the GlkHtmlWin / FrankenDrift.Headless handling of
                 <cls>.  An Anno 1700-style intro ends with a <cls>, so its
                 credits/preamble are wiped and only the post-clear text survives.
                 But the runner's Display accumulates the whole turn's text and renders it
                 once, so a <cls> also wipes everything emitted EARLIER in the
                 turn (other fragments already in `out`).  Leave a marker byte so
                 the per-turn flush can drop that earlier text too.

                 Interactively the pre-clear text is a real page the player must
                 see (the Runner shows it, then clears its window): keep the
                 text, leave only the mark for the host to clear at. */
              if (!a5_interactive_mode)
                {
                  sb.len = 0;
                  if (sb.p != NULL) sb.p[0] = '\0';
                }
              sb_putc (&sb, A5_CLS_MARK);
            }
          else if (a5_interactive_mode && strcmp (name, "waitkey") == 0)
            /* Interactive pause point; doubles as the usual ALR tag blocker. */
            sb_putc (&sb, A5_WAITKEY_MARK);
          else if (a5_interactive_mode && strcmp (name, "wait") == 0)
            {
              /* Timed pause <wait N>: leave the delay argument as a
                 positional \026<seconds>\026 span for the host to run a
                 cancelable timer at, the way the Runner's rendering pauses
                 mid-Display (<wait is in its valid-tag list,
                 clsUserSession.vb).  Death Shack's title cards type one
                 80-point letter per <wait 3>. */
              const char *arg = tag[4] == ' ' ? tag + 5 : "";
              sb_putc (&sb, A5_WAIT_MARK);
              sb_puts (&sb, arg);
              sb_putc (&sb, A5_WAIT_MARK);
            }
          else if (a5_interactive_mode
                   && (strcmp (name, "center") == 0
                       || strcmp (name, "centre") == 0))
            /* Centered span opens; like waitkey, the mark also blocks ALRs. */
            sb_putc (&sb, A5_CENTER_MARK);
          else if (a5_interactive_mode
                   && (strcmp (name, "/center") == 0
                       || strcmp (name, "/centre") == 0))
            sb_putc (&sb, A5_ENDCENTER_MARK);
          else if (a5_interactive_mode && strcmp (name, "b") == 0)
            /* Bold span opens; like center, the mark also blocks ALRs (a bare
               control byte can't sit inside a game OldText, so the boundary
               strstr cannot match across it).  Headlessly <b> still drops to
               A5_ALR_MARK below, so the ground-truth text is unchanged. */
            sb_putc (&sb, A5_BOLD_MARK);
          else if (a5_interactive_mode && strcmp (name, "/b") == 0)
            sb_putc (&sb, A5_ENDBOLD_MARK);
          else if (a5_interactive_mode && strcmp (name, "window") == 0)
            {
              /* Secondary output window opens: leave the window name delimited
                 by A5_WINDOW_MARK (like an image span) so the host can route the
                 enclosed text to that named side window.  The name comes from
                 the tag argument (lowercased with the rest of the tag); default
                 to "main" when the author wrote a bare <window>.  Headlessly this
                 tag drops to A5_ALR_MARK below, so ground truth is unchanged. */
              const char *wn = tag;                /* skip the "window" name */
              while (*wn != '\0' && *wn != ' ') wn++;
              while (*wn == ' ') wn++;             /* then any separating spaces */
              sb_putc (&sb, A5_WINDOW_MARK);
              sb_puts (&sb, *wn != '\0' ? wn : "main");
              sb_putc (&sb, A5_WINDOW_MARK);
            }
          else if (a5_interactive_mode && strcmp (name, "/window") == 0)
            sb_putc (&sb, A5_ENDWINDOW_MARK);
          else if (a5_media_sink != NULL
                   && (strcmp (name, "img") == 0 || strcmp (name, "audio") == 0))
            {
              /* Embedded media: report it out of band; it still drops from the
                 plain text (so the text output is unchanged).  Interactively an
                 image also leaves \006<resource>\006 at the tag's position so
                 the host draws it where the author placed it, and a sound
                 leaves \024<event index>\024 so the host starts/stops it there
                 -- ahead of any later <waitkey>, as the Runner does. */
              int is_img = strcmp (name, "img") == 0;
              int res = a5_emit_media (std::string (p + 1, tagend), is_img);
              if (a5_interactive_mode && is_img && res > 0)
                {
                  char nbuf[16];
                  snprintf (nbuf, sizeof nbuf, "%c%d%c",
                            A5_IMG_MARK, res, A5_IMG_MARK);
                  sb_puts (&sb, nbuf);
                }
              else if (a5_interactive_mode && !is_img && res >= 0)
                {
                  char nbuf[16];
                  snprintf (nbuf, sizeof nbuf, "%c%d%c",
                            A5_SOUND_MARK, res, A5_SOUND_MARK);
                  sb_puts (&sb, nbuf);
                }
              else
                sb_putc (&sb, A5_ALR_MARK);
            }
          else
            /* every other tag (<>, <c>, </c>, <b>, <i>, <font...>, <waitkey>...)
               drops -- but leave A5_ALR_MARK so the display-boundary ALR pass
               cannot match an OldText ACROSS the stripped tag (the runner's ALR sees the
               tag and is blocked; see a5text.h).  finish_turn strips the mark. */
            sb_putc (&sb, A5_ALR_MARK);
          p = q;
          continue;
        }
      if (*p == '&')
        {
          if (strncmp (p, "&perc;", 6) == 0) { sb_putc (&sb, '%'); p += 6; continue; }
          if (strncmp (p, "&amp;", 5) == 0)  { sb_putc (&sb, '&'); p += 5; continue; }
          if (strncmp (p, "&lt;", 4) == 0)   { sb_putc (&sb, '<'); p += 4; continue; }
          if (strncmp (p, "&gt;", 4) == 0)   { sb_putc (&sb, '>'); p += 4; continue; }
          if (strncmp (p, "&quot;", 6) == 0) { sb_putc (&sb, '"'); p += 6; continue; }
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

/* str_replace_all, except an occurrence of `find` that is part of an ALREADY-
   PRESENT copy of `repl` is left as it is.  The Display-boundary first ALR
   round runs over text whose per-fragment ALR pass has already applied every
   override, whereas the runner's single Display round (Global.vb:530) sees
   unprocessed text -- so an ALR whose NewText CONTAINS its own OldText
   (Magnetic Moon's "some electrical cable" -> "some electrical cable[.]"
   tied-state suffix) must not expand a second time here.  Only a NewText
   that contains the OldText leaves applied sites still matching `find`, so
   the skip triggers solely on that shape, aligned at the contained offset --
   a mere shared prefix (GFS's "You are wearing nothing, and" -> "You") is an
   UNAPPLIED site and replaces as usual, as does any occurrence the fragment
   passes never saw whole (e.g. text assembled across fragments). */
static char *
str_replace_unapplied (const char *src, const char *find, const char *repl)
{
  sb_t sb;
  size_t flen = strlen (find), rlen = strlen (repl);
  const char *inner = strstr (repl, find);  /* OldText inside NewText? */
  long k = inner != NULL ? (long) (inner - repl) : -1;
  const char *p = src;
  sb_init (&sb);
  while (*p != '\0')
    {
      if (strncmp (p, find, flen) == 0)
        {
          if (k >= 0 && (long) (p - src) >= k
              && strncmp (p - k, repl, rlen) == 0)
            {
              /* already applied: keep the old text verbatim */
              sb_putn (&sb, p, flen);
            }
          else
            sb_puts (&sb, repl);
          p += flen;
          continue;
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

/* One ALR pass for the Display boundary: like replace_alrs, but the input and
   output are PLAIN text -- each NewText is fully rendered (functions/expr/ALR +
   markup -> plain) before substitution, so an override that carries markup (e.g.
   "<br>Tiden gaar...<br>") lands as plain text rather than leaking literal tags.
   The whole-output markup renderer is deliberately not used (it would eat a
   literal '<' the descriptions legitimately carry).  `skip_applied` selects the
   already-applied-occurrence skip (str_replace_unapplied): the FIRST boundary
   round mirrors the runner's single Display round over per-fragment-processed text, so
   it must skip; the bChanged-gated SECOND round runs over ALREADY-substituted
   text in the runner too, so it re-expands exactly like the runner (plain replace). */
static char *
boundary_alr_once (a5_state_t *st, const char *src, int skip_applied)
{
  char *cur = strdup (src);
  int i;
  for (i = 0; i < st->adv->n_alrs; i++)
    {
      const a5_alr_t *alr = &st->adv->alrs[i];
      if (alr->old_text == NULL || alr->old_text[0] == '\0')
        continue;
      if (strstr (cur, alr->old_text) != NULL)
        {
          if (getenv ("A5_TRACE_ALR"))
            fprintf (stderr, "[ALR boundary skip=%d] hit old=\"%s\"\n",
                     skip_applied, alr->old_text);
          char *raw = a5text_eval_description (st, alr->new_text);
          char *proc = process_inner (st, raw, 1);
          char *val = a5text_render_plain (proc);
          if (!streq (cur, val))     /* guard against self-referential ALRs */
            {
              char *next = skip_applied
                             ? str_replace_unapplied (cur, alr->old_text, val)
                             : str_replace_all (cur, alr->old_text, val);
              free (cur);
              cur = next;
            }
          free (raw);
          free (proc);
          free (val);
        }
    }
  return cur;
}

char *
a5text_display_alr (a5_state_t *st, const char *plain)
{
  char *a1, *cap, *a2;
  if (plain == NULL)
    return strdup ("");
  /* No ALRs -> the boundary is a pure pass-through (English games never pay for
     this; their per-fragment ALR already ran).  Mirrors Display() doing nothing
     when htblALRs is empty. */
  if (st == NULL || st->adv == NULL || st->adv->n_alrs == 0)
    return strdup (plain);
  /* ReplaceALRs: ALR loop -> auto-capitalise -> a second ALR round (the input is
     already plain and per-fragment-processed, so the function/expression passes
     have nothing left to expand). */
  a1 = boundary_alr_once (st, plain, 1);
  /* Boundary re-cap runs on plain text: suppress the `^`/`\n` line-start rules
     (a5text_process already applied them per-fragment on marked-up text) so a
     stripped formatting tag cannot expose a line-leading word to a spurious cap;
     only true sentence punctuation drives this pass (see is_sentence_start).
     And run it ONLY when ALR round 1 actually rewrote something: the runner's display
     cap operates on the still-MARKED-UP buffer, where a description-segment
     "<>" join or any formatting tag sits between the full stop and the next
     letter and blocks the regex -- Scarier's per-fragment cap (which also sees
     the markup) has already adjudicated those positions identically, so the
     only faithful NEW caps here are on text an ALR substitution just changed
     (Starship Quest's appended lowercase "native's ..." examine segment must
     stay lowercase, as in the runner). */
  if (streq (a1, plain))
    cap = strdup (a1);
  else
    cap = auto_capitalise_ex (a1, 0);
  /* The runner's second ALR round runs ONLY `If bChanged` -- when the auto-cap
     actually rewrote something (Global.vb:550).  Unconditionally re-running
     it doubles any ALR whose NewText still contains its OldText (Magnetic
     Moon's suits disambiguation "(type X EVA SUITS ...)" suffix). */
  if (streq (cap, a1))
    a2 = strdup (cap);
  else
    a2 = boundary_alr_once (st, cap, 0);
  free (a1);
  free (cap);
  return a2;
}

/* Expand the A5_VARDEF_MARK deferred-variable sentinels (a bare %variable%
   inside an eagerly-applied ALR NewText, see a5text.h) with the variable's
   CURRENT -- i.e. end-of-turn -- value.  finish_turn runs this on the
   assembled turn output before the boundary ALR pass, mirroring the runner Display's
   ReplaceFunctions-before-ReplaceALRs order, and once more after it for any
   sentinel a boundary-applied ALR introduced. */
char *
a5text_expand_var_defers (a5_state_t *st, const char *text)
{
  sb_t sb;
  const char *p = text != NULL ? text : "";
  if (strchr (p, A5_VARDEF_MARK) == NULL)
    return strdup (p);
  sb_init (&sb);
  while (*p != '\0')
    {
      const char *e;
      if (*p == A5_VARDEF_MARK && (e = strchr (p + 1, A5_VARDEF_MARK)) != NULL)
        {
          std::string name (p + 1, (size_t) (e - (p + 1)));
          int i, done = 0;
          for (i = 0; i < st->adv->n_variables; i++)
            if (ci_eq (st->adv->variables[i].name, name.c_str ()))
              {
                if (streq (st->adv->variables[i].type, "Text"))
                  sb_puts (&sb, st->var_text[i] ? st->var_text[i] : "");
                else
                  {
                    char buf[32];
                    snprintf (buf, sizeof buf, "%ld", st->var_num[i]);
                    sb_puts (&sb, buf);
                  }
                done = 1;
                break;
              }
          if (!done)
            sb_puts (&sb, name.c_str ());
          p = e + 1;
          continue;
        }
      sb_putc (&sb, *p++);
    }
  return sb_finish (&sb);
}

/* The runner applies pSpace to its RAW (markup-bearing) output buffer, so a message whose
   raw text ends in something other than a real newline -- a stripped tag
   (`...\n<font color=X>`), a trailing `<br>`, or an entity -- leaves the buffer
   non-vbLf and the NEXT message space-joins with two leading spaces.  Scarier
   strips markup per message, so its stripped text ends in the '\n' that preceded
   the trailing tag and it would drop that join.  When the pre-strip text ends
   non-'\n' but the stripped text ends in '\n', append A5_PS_MARK so sb_pspace
   still treats this message as a non-newline tail (finish_turn strips the mark). */
static char *
ps_mark_trailing (const char *proc, char *plain)
{
  size_t pl = strlen (proc), tl = strlen (plain);
  if (pl > 0 && proc[pl - 1] != '\n' && tl > 0 && plain[tl - 1] == '\n')
    {
      char *np = (char *) realloc (plain, tl + 2);
      if (np != NULL) { np[tl] = A5_PS_MARK; np[tl + 1] = '\0'; return np; }
    }
  return plain;
}

char *
a5text_describe (a5_state_t *st, const a5_xml_node_t *wrapper, int *raw_nonblank)
{
  char *raw = a5text_eval_description (st, wrapper);
  if (raw_nonblank != NULL)
    *raw_nonblank = (raw != NULL && raw[0] != '\0');
  char *proc = a5text_process (st, raw);
  char *plain = a5text_render_plain (proc);
  plain = ps_mark_trailing (proc, plain);
  free (raw);
  free (proc);
  return plain;
}

/* a5text_describe WITHOUT the final markup strip: the processed text keeps its
   formatting tags and the "<>" description-segment join markers.  This is what
   an OO `.Description` read must return -- the runner's Description.ToString hands the
   still-MARKED-UP composition to the caller (tags survive comparisons and
   re-embedding; the UI strips them once, at the very end).  When the value is
   re-embedded in a task response, the later a5text_process cap sees the "<>"
   between a full stop and a lowercase segment start and leaves it alone,
   exactly like the runner's Display-time CapAfterFullStop (Starship Quest's DeadNative
   "native's only clothing ..." append stays lowercase); the response's own
   final render_plain drops the markers from the display text. */
char *
a5text_describe_marked (a5_state_t *st, const a5_xml_node_t *wrapper)
{
  char *raw = a5text_eval_description (st, wrapper);
  char *proc = a5text_process (st, raw);
  free (raw);
  return proc;
}

/* a5text_describe, additionally reporting whether the text had visible content
   before the ALR pass (the runner's bHasOutput on the stored response -- see
   process_inner_ex).  *pre_alr_ink = 1 with a whitespace-only result means a
   game ALR blanked the message at Display time; the caller should keep the
   whitespace remainder so the message's paragraph slot survives, as it does in
   the runner's output stream. */
/* The runner's Display() appends the still-MARKED-UP string to sOutputText, so its
   `sOutputText = ""` NotUnderstood test counts markup-only messages as output
   (see a5_state_t.turn_out_nonempty).  Flag any non-blank marked-up render. */
static void
note_marked_output (a5_state_t *st, const char *marked)
{
  if (st->turn_out_nonempty)
    return;
  for (const char *q = marked; *q; q++)
    if (*q != '\n' && *q != '\r' && *q != ' ' && *q != '\t'
        && *q != A5_ALR_MARK)
      { st->turn_out_nonempty = 1; return; }
}

char *
a5text_describe_ex (a5_state_t *st, const a5_xml_node_t *wrapper,
                    int *pre_alr_ink, int *raw_nonblank, char **marked)
{
  char *raw = a5text_eval_description (st, wrapper);
  if (raw_nonblank != NULL)
    *raw_nonblank = (raw != NULL && raw[0] != '\0');
  char *plain = a5text_process_frozen (st, raw, pre_alr_ink, marked);
  free (raw);
  return plain;
}

/* a5text_describe_ex applied to a PRE-FROZEN raw description template (a string
   a5text_eval_description returned earlier): the %function%/OO/<#...#>/ALR
   passes only -- segment selection is NOT re-evaluated.  This is how the runner
   re-renders a Before completion message around its actions
   (clsUserSession.vb:1178/1200/1204): sMessage is captured ONCE via
   CompletionMessage.ToString and the later ReplaceExpressions(ReplaceFunctions())
   calls rework that frozen string, so a restriction-gated segment whose gate the
   actions flip (Spectre of Castle Coris's banish hiding the Spectre mid-task)
   still re-renders -- and still draws its <# Either #> -- on the post-action
   compare. */
char *
a5text_process_frozen (a5_state_t *st, const char *raw, int *pre_alr_ink,
                       char **marked)
{
  char *inner = process_inner_ex (st, raw, 0, pre_alr_ink);
  char *persp = resolve_perspective (st, inner);
  char *capped = auto_capitalise (persp);
  char *plain = a5text_render_plain (capped);
  plain = ps_mark_trailing (capped, plain);
  note_marked_output (st, capped);
  if (marked != NULL)
    *marked = strdup (capped != NULL ? capped : "");
  free (inner);
  free (persp);
  free (capped);
  return plain;
}

/* a5text_describe applied to a location's effective short description (base +
   inherited darkness alternate) -- the fully-rendered room NAME. */
char *
a5text_location_short_plain (a5_state_t *st, const char *lockey)
{
  char *raw = a5text_location_short (st, lockey);
  char *proc = a5text_process (st, raw);
  char *plain = a5text_render_plain (proc);
  free (raw);
  free (proc);
  return plain;
}

/* ----------------------------------------------------------- LOOK / location */

/* clsObject.HasProperty over the merged static + runtime property table: a
   runtime SetProperty override wins (`<Unselected>` = the SelectionOnly
   property was removed, clsUserSession.vb:2058), else the model's static prop
   set.  The room-view listing filters must consult the runtime layer --
   Euripides' drone / boom box run `SetProperty <obj> ExplicitlyExclude
   <Selected>` to drop themselves from the "Also here is ..." auto-list once
   their presence is conveyed by prose. */
static int
object_has_prop_rt (a5_state_t *st, const a5_object_t *o, const char *propkey)
{
  int i;
  for (i = 0; i < st->n_ov; i++)
    if (streq (st->ov[i].entity, o->key) && streq (st->ov[i].prop, propkey))
      return strstr (st->ov[i].value, "Unselected") == NULL;
  /* Static presence: a SelectionOnly property carries no <Value>, so test the
     prop's existence, not its (possibly NULL) value. */
  return a5_prop_find (o->props, o->n_props, propkey) != NULL;
}

/* Get an object's "list description" text (static/dynamic), or NULL. */
static char *
object_list_desc (a5_state_t *st, const a5_object_t *o, int is_static)
{
  const char *key = is_static ? "ListDescription" : "ListDescriptionDynamic";
  const a5_prop_t *prop = a5_prop_find (o->props, o->n_props, key);
  if (prop == NULL)
    return NULL;
  if (prop->value_node != NULL)
    return a5text_eval_description (st, prop->value_node);
  if (prop->value != NULL && prop->value[0] != '\0')
    return strdup (prop->value);
  return NULL;
}

/* A present character's "is here" line: its CharHereDesc property rendered (with
   the character as the text context, retiring its DisplayOnce segments), else
   the default "<Name> is here." (clsLocation.ViewLocation: IsHereDesc / fallback). */
static char *
char_here_desc (a5_state_t *st, const a5_character_t *c)
{
  const a5_prop_t *p = a5_prop_find (c->props, c->n_props, "CharHereDesc");
  const char *prev_ctx = st->ctx_char;
  char *result;
  st->ctx_char = c->key;
  if (p != NULL)
    {
      /* The property is present (clsItem.HasProperty == ContainsKey), so it
         overrides the default -- even when its value is blank, in which case
         IsHereDesc returns "" and the character is suppressed from the present
         list (the Customs Official in Revenge: its room prose already says "A
         customs official stands here."). */
      if (p->value_node != NULL)
        {
          char *raw;
          {
            a5_mark_guard mg (st, 1);
            raw = a5text_eval_description (st, p->value_node);
          }
          result = process_inner (st, raw, 0);     /* %functions% / OO pass */
          free (raw);
        }
      else
        result = strdup ("");
    }
  else
    {
      char *nm = character_display_name (st, c, 0, /*displaying*/ 0);
      size_t need = strlen (nm) + 11;
      result = (char *) malloc (need);
      snprintf (result, need, "%s is here.", nm);
      free (nm);
    }
  st->ctx_char = prev_ctx;
  return result;
}

/* Case-insensitive replace-all of `from` with `to` in `s`. */
static std::string
ci_replace_all (const std::string &s, const std::string &from, const std::string &to)
{
  if (from.empty ()) return s;
  std::string out, low_from, low_s;
  for (char ch : from) low_from += (char) tolower ((unsigned char) ch);
  for (char ch : s)    low_s    += (char) tolower ((unsigned char) ch);
  size_t i = 0;
  while (i < s.size ())
    {
      if (i + from.size () <= s.size ()
          && low_s.compare (i, from.size (), low_from) == 0)
        { out += to; i += from.size (); }
      else
        out += s[i++];
    }
  return out;
}

/* Render a specific location's ViewLocation (clsLocation.ViewLocation).  lockey
   NULL => the Player's current location (the common case; also the `%Player%`
   look path).  Used directly for `%DisplayLocation[loc]%`. */
static char *
view_location_impl (a5_state_t *st, const char *lockey)
{
  const a5_location_t *loc;
  sb_t sb;
  char *plain;
  int i, listed = 0, body_empty = 1;
  size_t body_start = 0;

  if (lockey == NULL)
    lockey = a5state_player_location (st);
  sb_init (&sb);
  if (lockey == NULL)
    return strdup ("");
  loc = a5model_location (st->adv, lockey);
  if (loc == NULL)
    return strdup ("");

  /* A v5 location view is prefixed with a blank line before the room name
     (clsLocation.ViewLocation: "If Adventure.dVersion >= 5 Then sView = vbCrLf
     & sView"), so after a "> look" echo the room name has a paragraph break. */
  sb_putc (&sb, '\n');

  /* Room name (bold), then the long description.  The runner embeds the raw
     ShortDescription (still bearing its %functions%) inside "<b>...</b>" on the
     marked-up view (clsLocation.vb:188) and only its ONE Display-time
     ReplaceFunctions + ReplaceALRs + CapAfterFullStop runs over the whole sView;
     the '\n' before the name is followed by the '<' of the tag, so the
     line-start cap rule never fires on the name.  So process the name WITHOUT
     the per-piece cap (a5text_process_nocap keeps markup + resolves
     functions/OO/ALR), wrap it in the bold tags, and let the room-view
     replace_alrs + auto_capitalise pass below adjudicate it exactly as the runner does.
     A genuinely lowercase generated name -- LostCoastlines' "cabal plain",
     "flock sea" -- is thus left lowercase like the runner, while an already-capitalised
     name (its case baked into the source) is unaffected. */
  {
    char *raw = a5text_location_short (st, lockey);
    char *name = a5text_process_nocap (st, raw);
    sb_puts (&sb, "<b>");
    sb_puts (&sb, name);
    sb_puts (&sb, "</b>\n");
    free (raw);
    free (name);
  }
  body_start = sb.len;
  {
    /* Whether this render retires <DisplayOnce> segments follows the CALLER's
       marking_display, mirroring the runner's ambient bTestingOutput: the Look dance's
       two pre/post-action TEST renders (run_task, marking 0) must NOT retire a
       first-visit segment -- the runner's bTestingOutput=True renders both as the
       segment, they compare EQUAL, and the single Display render is what
       retires it.  Forcing 1 here made the test renders unequal (e1 kept the
       DisplayOnce text, e2 lost it), which pinned the view as plain text and
       let a movement task's own "%Player%.Location.Description" completion
       re-render the post-retire variant as a SECOND room view (Murder Most
       Foul's Slave Holdings / east-wing rooms).  Real-output call sites
       (emit_look, resp_flush, the game-start view) set marking_display=1. */
    char *raw, *proc;
    raw = location_long_desc (st, lockey);
    proc = a5text_process (st, raw);
    free (raw);
    /* Special-listed objects directly here.  The runner's ViewLocation SELECTS them
       via ObjectsInLocation(AllSpecialListedObjects), whose `ob.ListDescription
       <> ""` test is a REAL Description render (clsLocation.vb:232) -- it
       retires <DisplayOnce> segments -- and the append loop then renders the
       property AGAIN.  Mirror the two-pass shape: scan every candidate first
       (render + discard), then re-render the non-empty ones for output.  Lost
       Children's rabbit needs this: its DisplayOnce lead-in "Several metres
       ... you see " terminates the scan render, and the appended second render
       rebuilds it as the StartAfterDefaultDescription default prefix + "a
       large rabbit nibbling ...". */
    {
      std::vector<int> specials;
      for (i = 0; i < st->adv->n_objects; i++)
        {
          const a5_object_t *o = &st->adv->objects[i];
          int is_static = st->obj[i].is_static;
          int include = (!is_static && !object_has_prop_rt (st, o, "ExplicitlyExclude"))
                      || (is_static && object_has_prop_rt (st, o, "ExplicitlyList"));
          char *ld;
          if (!include || !a5state_object_at_location (st, i, lockey, 1))
            continue;
          ld = object_list_desc (st, o, is_static);          /* scan render */
          if (ld != NULL && ld[0] != '\0')
            specials.push_back (i);
          free (ld);
        }
      for (int si : specials)
        {
          const a5_object_t *o = &st->adv->objects[si];
          char *ld = object_list_desc (st, o, st->obj[si].is_static);
          if (ld != NULL && ld[0] != '\0')
            {
              char *p = process_inner (st, ld, 0);
              size_t plen = strlen (proc);
              /* pSpace: two spaces unless the previous text ends with a newline. */
              const char *sep = (plen == 0 || proc[plen - 1] == '\n') ? "" : "  ";
              size_t need = plen + 2 + strlen (p) + 1;
              char *joined = (char *) malloc (need);
              snprintf (joined, need, "%s%s%s", proc, sep, p);
              free (proc);
              proc = joined;
              free (p);
            }
          free (ld);
        }
    }
    /* The runner's ViewLocation sView at the general-listed test = LongDescription +
       special objects (the room NAME is NOT part of it): a v5 room whose body is
       empty lists its general objects as "There is X here." instead of the
       trailing "Also here is X." -- clsLocation.vb:132-139. */
    body_empty = (proc[0] == '\0');
    sb_puts (&sb, proc);
    free (proc);
  }

  /* General listed objects ("Also here is ..." / v5-empty-room "There is ...
     here."). */
  {
    const char **names = (const char **) malloc (sizeof (char *)
                          * (size_t) (st->adv->n_objects + 1));
    char **owned = (char **) malloc (sizeof (char *)
                          * (size_t) (st->adv->n_objects + 1));
    int n = 0;
    for (i = 0; i < st->adv->n_objects; i++)
      {
        const a5_object_t *o = &st->adv->objects[i];
        int is_static = st->obj[i].is_static;
        int include = (!is_static && !object_has_prop_rt (st, o, "ExplicitlyExclude"))
                    || (is_static && object_has_prop_rt (st, o, "ExplicitlyList"));
        char *ld;
        if (!include || !a5state_object_at_location (st, i, lockey, 1))
          continue;
        ld = object_list_desc (st, o, is_static);
        if (ld != NULL && ld[0] != '\0') { free (ld); continue; } /* special-listed */
        free (ld);
        owned[n] = a5text_object_name (st, o, A5_ART_INDEFINITE);
        names[n] = owned[n];
        n++;
      }
    if (n > 0)
      {
        /* v5 empty-body room: "There is <list> here." with no leading pSpace
           (clsLocation.vb:138).  Otherwise the trailing "Also here is <list>.",
           prefixed by pSpace(sView) -- two spaces unless the buffer is empty or
           already ends in a newline (clsLocation.vb:134). */
        int as_there_is = body_empty
          && (st->adv->version == NULL || atof (st->adv->version) >= 5.0);
        if (!as_there_is && sb.len > 0 && sb.p[sb.len - 1] != '\n')
          sb_puts (&sb, "  ");
        sb_puts (&sb, as_there_is ? "There is " : "Also here is ");
        for (i = 0; i < n; i++)
          {
            if (i > 0)
              sb_puts (&sb, (i == n - 1) ? " and " : ", ");
            sb_puts (&sb, names[i]);
          }
        sb_puts (&sb, as_there_is ? " here." : ".");
        listed = 1;
      }
    for (i = 0; i < n; i++)
      free (owned[i]);
    free (owned);
    free ((void *) names);
  }
  (void) listed;

  /* Event SetLook text (clsLocation.ViewLocation: "For Each e ... e.LookText()"):
     a SetLook sub-event pushed a location-gated look line onto the look stack;
     append the most-recent entry matching the player's location. */
  {
    const char *lt = a5state_player_look (st);
    if (lt != NULL && lt[0] != '\0')
      {
        /* pSpace(sView) (clsLocation.ViewLocation:144): always two spaces unless
           the buffer ends in a newline -- NOT add_space's sentence-aware test, so
           a description ending in a trailing space still gets the two (e.g.
           SixSilverBullets' Hotel "...grim and gray. " + "  " before the
           Purple Agent's "is here" line -> three spaces). */
        if (sb.len > 0 && sb.p[sb.len - 1] != '\n')
          sb_puts (&sb, "  ");
        sb_puts (&sb, lt);
      }
  }

  /* Present NPCs (clsLocation.ViewLocation character loop): each visible
     character contributes its CharHereDesc (or "<Name> is here."); identical
     descriptions are grouped, the name folded out via ##CHARNAME##, the verb
     pluralised (" is "->" are ") for a shared description, and the names listed. */
  {
    struct cgroup { std::string keyd, first_desc; std::vector<std::string> names; };
    std::vector<cgroup> groups;
    for (i = 0; i < st->adv->n_characters; i++)
      {
        const a5_character_t *c = &st->adv->characters[i];
        char *nmr, *descr;
        if (ci_eq (c->key, a5state_player_key (st)))
          continue;
        if (!a5state_character_visible_at_location (st, i, lockey))
          continue;                          /* incl. on/in furniture, but not
                                                inside a closed opaque container
                                                (CharactersVisibleAtLocation) */
        nmr = character_display_name (st, c, 0, /*displaying*/ 0);
        descr = char_here_desc (st, c);
        std::string sName = nmr ? nmr : "";
        std::string sDesc = descr ? descr : "";
        free (nmr); free (descr);
        if (sDesc.empty ())
          continue;
        std::string keyd = ci_replace_all (sDesc, sName, "##CHARNAME##");
        size_t g;
        for (g = 0; g < groups.size (); g++)
          if (groups[g].keyd == keyd)
            break;
        if (g == groups.size ())
          { cgroup ng; ng.keyd = keyd; ng.first_desc = sDesc; groups.push_back (ng); }
        if (std::find (groups[g].names.begin (), groups[g].names.end (), sName)
            == groups[g].names.end ())
          groups[g].names.push_back (sName);
      }
    for (size_t g = 0; g < groups.size (); g++)
      {
        const std::vector<std::string> &nm = groups[g].names;
        std::string d;
        /* The runner substitutes the name back into the de-named form for EVERY group,
           single characters included (vb:171 `sDesc.Replace("##CHARNAME##",
           .List)`), so an authored lowercase name is case-normalised to the
           character's Name -- LostLabyrinth's caged "white stallion" CharHereDesc
           renders "White Stallion".  Only a shared description also pluralises
           the verb. */
        d = groups[g].keyd;
        if (nm.size () > 1)
          d = ci_replace_all (d, " is ", " are ");
        {
          std::string list;
          for (size_t j = 0; j < nm.size (); j++)
            {
              if (j > 0)
                list += (j == nm.size () - 1) ? " and " : ", ";
              list += nm[j];
            }
          d = ci_replace_all (d, "##CHARNAME##", list);
        }
        if (sb.len > 0 && sb.p[sb.len - 1] != '\n')   /* pSpace, vb:166 */
          sb_puts (&sb, "  ");
        sb_puts (&sb, d.c_str ());
      }
  }

  /* Exit listing (clsLocation.ViewLocation: "If Adventure.ShowExits Then ...").
     ListExits enumerates the player's routes (restriction-unchecked, matching
     HasRouteInDirection(d, False)); >1 exits => "Exits are <list>.", exactly one
     => "An exit leads <dir>.", none => nothing. */
  if (st->adv->show_exits)
    {
      /* The runner: Adventure.Player.ListExits(Me.Key) -- the player's routes FROM
         THE VIEWED LOCATION (Me.Key), so a %DisplayLocation[other]% view
         (Head Case's spyhole) lists the other room's exits. */
      long n = 0;
      char *lst = a5expr_list_exits (st, a5state_player_key (st), lockey, &n);
      /* clsLocation.ViewLocation calls pSpace(sView) *unconditionally* (vb:177)
         before the iExitCount check, so a room with NO exits ends with two
         dangling trailing spaces -- which a following same-turn event message
         then joins onto with another pSpace, giving four spaces (the JJ police
         cell "...out of the cell.    Alan appears..." case). */
      if (sb.len > 0 && sb.p[sb.len - 1] != '\n')   /* pSpace, vb:177 */
        sb_puts (&sb, "  ");
      if (n >= 1)
        {
          if (n > 1)
            { sb_puts (&sb, "Exits are "); sb_puts (&sb, lst ? lst : ""); }
          else
            { sb_puts (&sb, "An exit leads "); sb_puts (&sb, lst ? lst : ""); }
          sb_putc (&sb, '.');
        }
      free (lst);
    }

  /* Empty-view fallback (clsLocation.ViewLocation vb:185: `If sView = "" Then
     sView = "Nothing special."`).  The runner's sView at this point is only the BODY
     (long description + listed objects + event look text + NPC lines + exits);
     the bold room name is prepended after the check.  Scarier builds the name
     into the same buffer up-front, so compare against the post-header mark
     (canyoustandup's "Beginning" start room: no description at all). */
  if (sb.len == body_start)
    sb_puts (&sb, "Nothing special.");

  /* Capitalise the assembled room view on its still-MARKED-UP buffer, exactly
     as the runner's Display-time CapAfterFullStop runs over the whole (marked-up) turn
     text.  This is where the room's NPC "is here" list (built here with lowercase
     articles, e.g. "the cook and the kitchen maid are here.") gets its line-start
     capital: the runner caps it because the newline before it is followed directly by the
     letter, with no intervening tag.  Doing it here (rather than only at the
     a5text_display_alr boundary, which sees PLAIN text and must suppress the
     `^`/`\n` rules to avoid capitalising a letter the runner left alone behind a stripped
     tag) keeps the room view correct for ALR games without the boundary needing
     the tag-sensitive line-start rules.  Non-ALR games always matched with their
     room views already upper-cased at line start, so this is a no-op for them. */
  {
    /* The runner's Display runs ReplaceALRs BEFORE CapAfterFullStop (Global.vb:527-539),
       so a game TextOverride matches the still-lowercase NPC listing -- AoK's
       Override12 blanks "the dwark is here." -- and only the survivors get their
       line-start capital.  Run ALR round 1 on the uncapped marked-up buffer
       here; the cap follows, and the runner's post-cap round 2 falls out of the
       ordinary downstream ALR passes over the capped text. */
    char *alrd = replace_alrs (st, sb.p ? sb.p : "", 0);
    /* This ALR round is the runner's Display-time pass for the room view: its verdicts
       are final.  Seal any surviving OldText site (an override whose passing
       alternative was the identity text, e.g. XXR's camel-tether postfix while
       the camels are untethered) so the end-of-turn boundary pass cannot
       re-adjudicate it with post-task state -- the runner never revisits Display()ed
       text.  finish_turn strips the sentinels. */
    alrd = seal_alr_sites (st, alrd);
    char *capped = auto_capitalise (alrd);
    free (alrd);
    plain = a5text_render_plain (capped);
    free (capped);
  }
  free (sb.p);
  return plain;
}

char *
a5text_view_location (a5_state_t *st)
{
  return view_location_impl (st, NULL);
}
