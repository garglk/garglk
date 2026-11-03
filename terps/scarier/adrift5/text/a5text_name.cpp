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
 * ADRIFT 5 support for Scarier -- text engine: object and character naming
 * (split from a5text.cpp).  Full names with articles, perspective and
 * pronouns, and the command-reference words (%object%, %character2%, ...).
 * See a5text.h and a5text_internal.h.
 */

#include "a5text_internal.h"

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
  if (st == nullptr || s == nullptr || strchr (s, '%') == nullptr || depth >= 4)
    return s;
  depth++;
  p = a5text_process_noalr ((a5_state_t *) st, s);
  depth--;
  if (p == nullptr)
    return s;
  *owned = p;
  return p;
}

char *
a5text_object_name (const a5_state_t *st, const a5_object_t *o, a5_article_t art)
{
  sb_t sb;
  char *own_noun = nullptr, *own_prefix = nullptr, *own_article = nullptr;
  const char *noun = (o->n_names > 0) ? o->names[0] : o->key;
  const char *prefix = o->prefix;
  const char *article = o->article;

  /* The _ObjectArticle/_ObjectPrefix/_ObjectNoun mandatory-property overrides
     (clsUserSession.vb:1972-1982) feed clsObject.FullName too: a renamed
     object displays -- and lists, and %TheObject%s -- under its new noun.
     st may be NULL in naming-only contexts (no runtime state). */
  if (st != nullptr)
    {
      const char *ov;
      if ((ov = a5state_entity_prop (st, o->key, "_ObjectNoun")) != nullptr)
        noun = ov;
      if ((ov = a5state_entity_prop (st, o->key, "_ObjectPrefix")) != nullptr)
        prefix = ov;
      if ((ov = a5state_entity_prop (st, o->key, "_ObjectArticle")) != nullptr)
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
      if (article != nullptr && article[0] != '\0')
        sb_puts (&sb, article);
      sb_putc (&sb, ' ');
    }
  if (prefix != nullptr && prefix[0] != '\0')
    {
      sb_puts (&sb, prefix);
      sb_putc (&sb, ' ');
    }
  sb_puts (&sb, noun != nullptr ? noun : "");
  free (own_noun);
  free (own_prefix);
  free (own_article);
  return sb_finish (&sb);
}


/* A character's perspective (FirstPerson=1 / SecondPerson=2 / ThirdPerson=3).
   Keyed on whether `c` is the CURRENT player, not its static Type: after a
   ToSwitchWith/BECOME the new player is still typed NonPlayer, yet must narrate
   in the player's perspective ("you pick up ...", not "Lance-Corporal Davey pick
   up ...").  For every game without BECOME this is identical to the old type
   test -- the sole Type=Player character is exactly the current player. */
int
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
  if (c == nullptr)
    return 2;
  if (st == nullptr || !streq (c->key, a5state_player_key (st)))
    return 3;
  const a5_character_t *orig = nullptr;
  for (int i = 0; i < st->adv->n_characters; i++)
    if (st->adv->characters[i].type != nullptr
        && ci_eq (st->adv->characters[i].type, "Player"))
      { orig = &st->adv->characters[i]; break; }
  if (orig == nullptr)
    orig = c;
  if (orig->perspective != nullptr)
    {
      if (ci_eq (orig->perspective, "FirstPerson"))  return 1;
      if (ci_eq (orig->perspective, "SecondPerson")) return 2;
      if (ci_eq (orig->perspective, "ThirdPerson"))  return 3;
    }
  /* No stored <Perspective>: the runner's loader records ThirdPerson for files
     >= 5.00002 and forces SecondPerson for older ones (FileIO.vb:1776-7). */
  if (st->adv->version != nullptr
      && strtod (st->adv->version, nullptr) < 5.00002)
    return 2;
  return 3;
}


/* "article [prefix] first-descriptor" (clsCharacter.Descriptor,
   clsCharacter.vb:572).  A NULL or empty `article` is omitted.  Callers have
   already checked c->n_descriptors > 0. */
std::string
char_descriptor_name (const a5_character_t *c, const char *article)
{
  std::string out;
  if (article != nullptr && article[0] != '\0')
    { out += article; out += ' '; }
  if (c->prefix != nullptr && c->prefix[0] != '\0')
    { out += c->prefix; out += ' '; }
  out += c->descriptors[0];
  return out;
}

/* clsCharacter.ProperName (clsCharacter.vb:455): the runtime
   CharacterProperName override (a typed-in player name, set via SetProperty --
   clsUserSession.vb:2080) wins over the model Name, and an empty name renders
   "Anonymous". */
const char *
char_proper_name_raw (const a5_state_t *st, const a5_character_t *c)
{
  const char *pn = c ? a5state_entity_prop (st, c->key, "CharacterProperName")
                     : nullptr;
  if (pn == nullptr || pn[0] == '\0')
    pn = (c != nullptr && c->name != nullptr && c->name[0] != '\0') ? c->name
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
char *
character_display_name (a5_state_t *st, const a5_character_t *c, int definite,
                        int displaying)
{
  /* clsCharacter.Name's Introduced dance (clsCharacter.vb:331-334), which only
     runs under UserSession.bDisplaying: once a character's name has rendered
     in displayed output, later Indefinite descriptor renders upgrade to the
     Definite article ("a tall guillermo" -> "the tall guillermo"), and the
     render itself marks the character Introduced.  Which renders count as
     bDisplaying ones is spelled out on a5state.h's intro_active. */
  if (displaying && st->intro_active
      && c != nullptr && st->char_introduced != nullptr)
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
  int known_set = c != nullptr && a5state_entity_has_prop (st, c->key, "Known");
  int use_proper = known_set || c == nullptr || c->n_descriptors == 0;
  if (use_proper)
    return strdup (char_proper_name_raw (st, c));
  /* An empty model article stays empty even when definite. */
  const char *article = (c->article != nullptr && c->article[0] != '\0')
                          ? (definite ? "the" : c->article) : nullptr;
  return strdup (char_descriptor_name (c, article).c_str ());
}

/*
 * The displayed name of a character with a pronoun, mirroring clsCharacter.Name:
 * first/second person always render as a pronoun (I/you/...); third person
 * renders the proper/descriptor name (we don't pronoun-replace NPCs without
 * mention tracking, which is the safe default).
 */
char *
character_name (a5_state_t *st, const a5_character_t *c, a5_pronoun_t pr,
                int consult_ledger)
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
  if (consult_ledger && c != nullptr)
    for (int i = st->n_pron - 1; i >= 0; i--)
      if (st->pron_key[i] != nullptr && streq (st->pron_key[i], c->key))
        {
          /* clsCharacter.Name marks Introduced (vb:333) BEFORE the pronoun
             branch, so a reply that pronoun-replaces the name ("He looks up
             at ye") still introduces the character -- the next descriptor
             render is definite.  Same intro_active gate as
             character_display_name. */
          if (st->intro_active && st->char_introduced != nullptr)
            {
              int intro_ci = a5state_character_index (st, c->key);
              if (intro_ci >= 0)
                st->char_introduced[intro_ci] = 1;
            }
          const char *g = a5state_entity_prop (st, c->key, "Gender");
          int male   = g != nullptr && strcasecmp (g, "Male") == 0;
          int female = g != nullptr && strcasecmp (g, "Female") == 0;
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
  std::string a = args != nullptr ? args : "";
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
      if (!explicit_article && !art_none && !art_definite && c != nullptr
          && st->char_introduced != nullptr)
        {
          int ci = a5state_character_index (st, c->key);
          if (ci >= 0 && st->char_introduced[ci]) art_definite = 1;
        }
      if (c != nullptr && st->char_introduced != nullptr)
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
  int known_set = c != nullptr && a5state_entity_has_prop (st, c->key, "Known");
  char *nm;
  if (known_set || c == nullptr || c->n_descriptors == 0)
    nm = strdup (char_proper_name_raw (st, c));
  else
    {
      const char *article = (!art_none && c->article != nullptr && c->article[0] != '\0')
                              ? (art_definite ? "the" : c->article) : nullptr;
      nm = strdup (char_descriptor_name (c, article).c_str ());
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
  char *nm = character_display_name (st, ci >= 0 ? &st->adv->characters[ci] : nullptr, 0,
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

/* Map a command-reference word -- object, object1..object5, objects,
   character(1..5), characters, location(1..5), item(1..5), any case -- to the
   a5state_lookup_ref slot it binds ("ReferencedObject", "ReferencedObject2",
   ...).  Index 1 and no index share the unsuffixed slot.  The runner's
   asymmetry is preserved: "objects" has its own ReferencedObjects slot while
   "characters" reads ReferencedCharacter.  Returns 0 for any other word. */
int
ref_slot_name (const char *name, std::string &slot)
{
  static const struct { const char *word; const char *slot; } bases[] = {
    { "object",    "ReferencedObject" },
    { "character", "ReferencedCharacter" },
    { "location",  "ReferencedLocation" },
    { "item",      "ReferencedItem" },
  };
  if (name == nullptr)
    return 0;
  if (ci_eq (name, "objects"))
    { slot = "ReferencedObjects"; return 1; }
  if (ci_eq (name, "characters"))
    { slot = "ReferencedCharacter"; return 1; }
  for (size_t b = 0; b < sizeof bases / sizeof bases[0]; b++)
    {
      size_t n = strlen (bases[b].word);
      const char *rest;
      if (strncasecmp (name, bases[b].word, n) != 0)
        continue;
      rest = name + n;
      if (rest[0] == '\0')
        { slot = bases[b].slot; return 1; }
      if (rest[0] >= '1' && rest[0] <= '5' && rest[1] == '\0')
        {
          slot = bases[b].slot;
          if (rest[0] != '1')
            slot += rest[0];
          return 1;
        }
    }
  return 0;
}

/* Is `name` an object or character reference word (any index, plural too)?
   Those are the references whose UNBOUND use as a function argument
   substitutes "". */
int
is_obj_char_ref (const char *name)
{
  std::string slot;
  return ref_slot_name (name, slot)
         && (slot.compare (0, 16, "ReferencedObject") == 0
             || slot.compare (0, 19, "ReferencedCharacter") == 0);
}

/*
 * Map a bare %reference% name (the token between the percents, already without
 * them) to the resolved entity key(s) for a property expression %name%.Chain.
 * Returns a malloc'd "key" / "key1|key2" string, or NULL if `name` is not an
 * OO-addressable reference or entity key.  Mirrors the %Player%/%objectN%/%objects%
 * -> key substitution the Adrift 5 runner does before ReplaceOO (Global.vb:1752+).
 */
char *
oo_firstkey (a5_state_t *st, const char *name)
{
  const char *k = nullptr;
  if (ci_eq (name, "player"))
    return strdup (a5state_player_key (st));
  /* The singular %object%/%object1% (resp. %character%/%character1%) token
     resolves to nothing when the slot was filled by a *plural* %objects%/
     %characters% reference -- the runner's GetReference requires ReferenceMatch="object1"
     (clsUserSession.vb:3990), so a %objects% command never sets %object%.  The
     plural %objects% / index forms %object2%.. and override matching are
     unaffected (they read the still-bound key). */
  if ((ci_eq (name, "object") || ci_eq (name, "object1")) && st->ref_object1_plural)
    return nullptr;
  if ((ci_eq (name, "character") || ci_eq (name, "character1"))
      && st->ref_character1_plural)
    return nullptr;
  /* %objectN% / %characterN% / %locationN% / %itemN% command references
     resolve to their bound KEY (Global.vb:1770+ substitutes
     MatchingPossibilities(0) for the ReferenceMatches).  The indexed
     character slots matter: a two-character command (`#-- Enemy attacks
     friend --# %character1% attacks %character2%`, Beginner's Cave's combat
     engine) binds ReferencedCharacter2 through the same SetTasks-Execute
     parameter path.  An %item% may hold an object OR character OR location
     key; ProbeWalk's completion message chains %LocationName[%location1%]%
     off this. */
  std::string slot;
  if (ref_slot_name (name, slot))
    k = a5state_lookup_ref (st, slot.c_str ());
  if (k == nullptr && (ci_eq (name, "object") || ci_eq (name, "objects")))
    k = a5state_lookup_ref (st, "ReferencedObject");
  if (k != nullptr)
    return strdup (k);
  /* a literal entity key (object/character/location/group)? */
  if (a5model_object (st->adv, name) || a5model_character (st->adv, name)
      || a5model_location (st->adv, name))
    return strdup (name);
  for (int i = 0; i < st->adv->n_groups; i++)
    if (streq (st->adv->groups[i].key, name))
      return strdup (name);
  return nullptr;
}
