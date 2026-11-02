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
 * ADRIFT 5 support for Scarier -- the word layer of a turn.
 *
 * The parts of clsUserSession.EvaluateInput that work on the command's WORDS
 * before and after task matching: "it"/"them"/"him"/"her" substitution and
 * the GrabIt pass that picks next turn's referents, the <Synonym> replacement
 * loop, the SystemTasks verbs handled outside the task engine, and the
 * NotUnderstood ladder (unknown word, "<Verb> what?", "I don't understand what
 * you want to do with X", the catch-all) with the known-words list it keys on.
 *
 * Split out of a5run.cpp, which keeps the turn driver that calls these; the
 * entry points are declared in a5run_internal.h.
 */

#include "a5run_internal.h"

/* Seed the known-words list exactly as clsUserSession.NotUnderstood does, lazily
   (the first time an unmatched command needs the word-validity check).

   FAITHFUL QUIRK: the runner stores command verbs, object/character articles, prefixes,
   names and descriptors in listKnownWords with their ORIGINAL case (only the
   character ProperName is `.ToLower`-ed; vb:3489-3531), and the validity check
   compares the *lowercased* input word case-sensitively (`List.Contains`).  So a
   capitalised model word like the object name "Crystal" or prefix "Zykon" is NOT
   a known word for the lowercase input "crystal"/"zykon" -- e.g. Revenge's
   `insert crystal in bracket` reports `I did not understand the word "crystal".`
   (the object is named "Crystal").  We preserve case here to reproduce it; the
   lookup in not_understood lowercases the input word, mirroring the runner's lowercase
   sInput vs mixed-case known words. */
static void
build_known_words (a5_run_t *run)
{
  const a5_adventure_t *adv = run->adv;
  std::set<std::string> *kw = new std::set<std::string>;
  int i, j;

  /* Verbs: every word of every General task command, with the pattern
     punctuation ([]{}/) blanked to spaces (NotUnderstood does the same). */
  for (i = 0; i < adv->n_tasks; i++)
    {
      if (!streq (adv->tasks[i].type, "General"))
        continue;
      for (j = 0; j < adv->tasks[i].n_commands; j++)
        {
          std::string c = adv->tasks[i].commands[j];
          for (char &ch : c)
            if (ch == '[' || ch == ']' || ch == '{' || ch == '}' || ch == '/')
              ch = ' ';
          for (auto &w : split_ws (c.c_str ()))
            kw->insert (w);
        }
    }
  /* Nouns: each object's article + prefix words + names.  The runner adds each
     arlNames entry WHOLE (vb:3531 `Add(sWord)` over the name list -- no word
     split), so a multi-word name like "quest 3" never makes the bare word "3"
     known; Quest Giver's night-rejected menu digit must fall to "I did not
     understand the word \"3\".", not the catch-all. */
  for (i = 0; i < adv->n_objects; i++)
    {
      const a5_object_t *o = &adv->objects[i];
      if (o->article && o->article[0]) kw->insert (o->article);
      for (auto &w : split_ws (o->prefix)) kw->insert (w);
      for (j = 0; j < o->n_names; j++)
        if (o->names[j] && o->names[j][0]) kw->insert (o->names[j]);
    }
  /* Characters: article + prefix words + descriptors (whole entries, like the
     runner's arlDescriptors loop) + proper name (the one field the runner
     lowercases, vb:3522). */
  for (i = 0; i < adv->n_characters; i++)
    {
      const a5_character_t *c = &adv->characters[i];
      if (c->article && c->article[0]) kw->insert (c->article);
      for (auto &w : split_ws (c->prefix)) kw->insert (w);
      for (j = 0; j < c->n_descriptors; j++)
        if (c->descriptors[j] && c->descriptors[j][0])
          kw->insert (c->descriptors[j]);
      if (c->name && c->name[0]) kw->insert (lower (c->name));
    }
  /* Adverbs: every direction word (sDirectionsRE split on "|"). */
  {
    std::string dr = a5parse_directions_re (), cur;
    for (size_t k = 0; k <= dr.size (); k++)
      {
        if (k == dr.size () || dr[k] == '|')
          { if (!cur.empty ()) kw->insert (cur); cur.clear (); }
        else
          cur += dr[k];
      }
  }
  kw->insert ("and");
  run->known_words = kw;
}

/* clsUserSession.SystemTasks: the built-in verbs handled outside the task
   engine, tried after task matching fails but before NotUnderstood.  Only the
   one this corpus exercises is implemented -- "wait"/"z" prints "Time passes..."
   and advances WaitTurns turns of TurnBasedStuff.  Returns 1 if handled. */
int
system_command (a5_run_t *run, const std::string &in, sb_t *out)
{
  if (in == "wait" || in == "z")
    {
      int n = run->adv->wait_turns;
      sb_puts (out, "Time passes...");
      for (int i = 0; i < n; i++)
        ev_tick_all (run, out);
      return 1;
    }
  return 0;
}

/* The clsUserSession.NotUnderstood ladder, reached when no task ran, no task
   failed with output, and there is no pending disambiguation: first reject the
   earliest unrecognised input word ("I did not understand the word ..."), then
   fall back to the adventure's catch-all message. */
void
not_understood (a5_run_t *run, const std::string &in, sb_t *out)
{
  if (run->known_words == nullptr)
    build_known_words (run);

  for (auto &w : split_ws (in.c_str ()))
    if (run->known_words->find (lower (w)) == run->known_words->end ())
      {
        sb_puts (out, "I did not understand the word \"");
        sb_puts (out, w.c_str ());
        sb_puts (out, "\".");
        return;
      }

  /* The user entered just a verb that matches a "verb %ref%" command: prompt for
     the missing reference ("<Verb> what?/who?") and remember the verb so the next
     input is re-tried as "<verb> <input>" (NotUnderstood, vb:3543-3581).  Only a
     single-word input.

     FAITHFUL QUIRK: the Adrift 5 runner tests the *normalised*
     command, where every bare singular reference has been suffixed with "1"
     (%object% -> %object1%, %character% -> %character1%, %direction% ->
     %direction1%; FileIO.vb:647).  The object/character branch checks for the
     substring "%object"/"%character" (no trailing %), which still matches
     %object1%/%character1%; but the direction branch checks for "%direction%"
     WITH the trailing %, which no longer matches %direction1%.  So the "where?"
     prompt is effectively dead code in the Runner -- a bare movement verb falls
     through to the catch-all.  We reproduce that exactly by normalising the
     command before the same three Contains checks. */
  if (in.find (' ') == std::string::npos)
    {
      for (size_t oi = 0; oi < run->order->size (); oi++)
        {
          const a5_task_t *t = &run->adv->tasks[(*run->order)[oi]];
          if (!streq (t->type, "General") || t->n_commands == 0)
            continue;
          const char *cmd0 = t->commands[0];
          /* arlCommands(0) must contain the verb and be a multi-word pattern. */
          if (strstr (cmd0, in.c_str ()) == nullptr || strchr (cmd0, ' ') == nullptr)
            continue;
          std::string norm = cmd0;          /* normalise bare singular refs */
          static const char *bare[] = { "%object%", "%character%", "%direction%",
                                        "%number%", "%text%" };
          static const char *num[]  = { "%object1%", "%character1%", "%direction1%",
                                        "%number1%", "%text1%" };
          for (int b = 0; b < 5; b++)
            { size_t p; const std::string from = bare[b], to = num[b];
              while ((p = norm.find (from)) != std::string::npos)
                norm.replace (p, from.size (), to); }
          const char *q = nullptr;
          if (norm.find ("%object") != std::string::npos)
            q = "what?";
          else if (norm.find ("%character") != std::string::npos)
            q = "who?";
          else if (norm.find ("%direction%") != std::string::npos)
            q = "where?";             /* never true: %direction1%, see above */
          else
            continue;
          /* Confirm a command pattern of this task actually matches verb+dummy. */
          int hit = 0;
          for (int ci = 0; ci < t->n_commands && !hit; ci++)
            { a5_match_t m;
              if (a5parse_match_command (t->commands[ci],
                                         (in + " sdkfjdslkj").c_str (), &m))
                hit = 1; }
          if (!hit)
            continue;
          run->remembered_verb = in;
          {
            std::string vp = in;            /* PCase(verb) */
            if (!vp.empty ()) vp[0] = (char) toupper ((unsigned char) vp[0]);
            sb_puts (out, vp.c_str ());
            sb_puts (out, " ");
            sb_puts (out, q);
          }
          return;
        }
    }

  /* The input names an object the player has seen and can see now, but no task
     accepted it: "I don't understand what you want to do with the X."
     (NotUnderstood's seen-noun branch, vb:3584-3609).  First seen+visible match
     wins, objects before characters (the runner's loop order).

     FAITHFUL QUIRK: the runner matches the entity's sRegularExpressionString *unanchored*
     against the whole input (`re.IsMatch(sInput)`, no `^...$`), so a noun
     alternative need only appear as a SUBSTRING -- the runner's player descriptor "me"
     matches inside "hel<me>t", so `put helmet ... ` reports "...do with Mike
     Erlin.".  Since the regex's article/prefix groups are all optional, IsMatch
     reduces to: does any noun alternative occur as a substring of the lowercased
     input?  The noun set is the object's names, or the character's descriptors
     plus its proper name when usable -- the proper name as a WHOLE string ("mike
     erlin" must appear contiguously, unlike the word-split set used elsewhere),
     so `say erlin ...` does NOT match the player and falls through to
     NotUnderstood, matching the runner. */
  {
    a5_state_t *st = run->st;
    std::string lin = lower (in);
    auto noun_substr = [&] (const std::vector<std::string> &nouns) {
      for (auto &n : nouns)
        if (!n.empty () && lin.find (n) != std::string::npos)
          return 1;
      return 0;
    };
    /* Takes ownership of the heap name. */
    auto dont_understand = [&] (char *name) {
      sb_puts (out, "I don't understand what you want to do with ");
      sb_puts (out, name);
      sb_puts (out, ".");
      free (name);
    };
    for (int oi = 0; oi < st->adv->n_objects; oi++)
      {
        const a5_object_t *o = &st->adv->objects[oi];
        if (st->obj_seen == nullptr || !st->obj_seen[oi]
            || !obj_in_scope (st, o->key))
          continue;
        std::vector<std::string> nouns;
        for (int n = 0; n < o->n_names; n++)
          nouns.push_back (lower (o->names[n]));
        /* The runner's `If arl.Count = 0 Then sRE &= "|"` fudge: a nameless object's
           empty alternation matches anywhere -> always IsMatch. */
        int hit = (o->n_names == 0) ? 1 : noun_substr (nouns);
        if (hit)
          {
            dont_understand (a5text_object_name (st, o, A5_ART_DEFINITE));
            return;
          }
      }

    /* Likewise a seen+visible character: "...what you want to do with <Name>."
       (NotUnderstood's character branch, vb:3597). */
    for (int ci = 0; ci < st->adv->n_characters; ci++)
      {
        const a5_character_t *c = &st->adv->characters[ci];
        if (!char_seen_p (st, c->key) || !char_visible (st, c->key))
          continue;
        std::vector<std::string> nouns;
        for (int d = 0; d < c->n_descriptors; d++)
          nouns.push_back (lower (c->descriptors[d]));
        if (char_name_usable (st, c) && c->name != nullptr)
          nouns.push_back (lower (c->name));
        if (noun_substr (nouns))
          {
            dont_understand (a5text_character_known_name (st, c, 0));
            return;
          }
      }
  }

  /* All words known but nothing matched: Adventure.NotUnderstood (FileIO.vb:850
     defaults a missing <NotUnderstood> to this string). */
  sb_puts (out, "Sorry, I didn't understand that command.");
}

/* clsUserSession.ReplaceWord: replace whole-word occurrences of `find` in `text`
   with `repl` (word boundaries are spaces / string ends; case-sensitive on the
   already-lowercased input, exactly as the runner). */
static void
replace_word (std::string &text, const std::string &find, const std::string &repl)
{
  /* " find " -> " repl " (interior) */
  std::string from = " " + find + " ", to = " " + repl + " ";
  size_t pos = 0;
  while ((pos = text.find (from, pos)) != std::string::npos)
    { text.replace (pos, from.size (), to); pos += to.size (); }
  /* leading / trailing / whole */
  if (text.size () > find.size () + 1
      && text.compare (0, find.size () + 1, find + " ") == 0)
    text = repl + " " + text.substr (find.size () + 1);
  if (text.size () > find.size () + 1
      && text.compare (text.size () - find.size () - 1, find.size () + 1, " " + find) == 0)
    text = text.substr (0, text.size () - find.size () - 1) + " " + repl;
  if (text == find)
    text = repl;
}

/*
 * clsUserSession.GrabIt: after the player's command words are known, work out
 * what "it"/"them"/"him"/"her" should refer to *next* turn.  Scan the input
 * words against in-scope (visible, then seen) objects' names and characters'
 * proper-name/descriptors; an unambiguous single match in a pronoun class sets
 * that class's stored display name.  Objects use the definite full name,
 * characters their (definite) known name.  "them" considers only plural objects;
 * characters split into him/her/it by Gender.  A >1 match is narrowed by a
 * prefix word appearing in the input (the runner's second pass); still-ambiguous classes
 * keep the previous value.
 */
static void
grab_it (a5_run_t *run, const std::string &in)
{
  a5_state_t *st = run->st;
  std::vector<std::string> words = split_ws (lower (in).c_str ());
  std::unordered_set<std::string> wordset (words.begin (), words.end ());
  std::string new_it, new_them, new_him, new_her;
  std::vector<const char *> it_keys, them_keys, him_keys, her_keys;
  int scope;

  /* Whole-word test against the (already lowercased) input words.  Using a
     hash set turns this from an O(n_words) linear scan -- repeated for every
     object name and character descriptor, twice per turn -- into O(1). */
  auto has_word = [&] (const std::string &w) {
    return wordset.count (w) ? 1 : 0;
  };
  auto add_uniq = [] (std::vector<const char *> &v, const char *k) {
    for (auto *e : v) if (streq (e, k)) return;
    v.push_back (k);
  };

  /* Visibility is invariant across both scope tiers (this scan mutates
     nothing), yet the tiered loops asked obj_visible/char_visible -- a key
     hash lookup plus a player-location re-resolution plus a placement-chain
     walk -- up to twice per entity per turn, which profiling showed as the
     single biggest a5run_input_inner cost on large games.  Compute each
     entity's visibility once, by model index, with the player location
     hoisted. */
  const char *ploc = a5state_player_location (st);
  std::vector<char> ovis ((size_t) st->adv->n_objects);
  std::vector<char> cvis ((size_t) st->adv->n_characters);
  for (int i = 0; i < st->adv->n_objects; i++)
    ovis[(size_t) i] = ploc != nullptr
      && a5state_object_visible_at_location (st, i, ploc, 0);
  for (int i = 0; i < st->adv->n_characters; i++)
    cvis[(size_t) i] = a5state_character_at_location (st, i, ploc);

  /* Visible first, then seen (eScope.Visible To eScope.Seen). */
  for (scope = 0; scope < 2; scope++)
    {
      int oi, ci;
      for (oi = 0; oi < st->adv->n_objects; oi++)
        {
          const a5_object_t *o = &st->adv->objects[oi];
          int inscope = (scope == 0)
            ? ovis[(size_t) oi]
            : (!ovis[(size_t) oi]
               && st->obj_seen != nullptr && st->obj_seen[oi]);
          if (!inscope)
            continue;
          int is_plural = (o->article != nullptr && lower (o->article) == "some");
          for (int n = 0; n < o->n_names; n++)
            if (has_word (lower (o->names[n])))
              {
                if (new_it.empty ()) add_uniq (it_keys, o->key);
                if (new_them.empty () && is_plural) add_uniq (them_keys, o->key);
              }
        }
      for (ci = 0; ci < st->adv->n_characters; ci++)
        {
          const a5_character_t *c = &st->adv->characters[ci];
          int inscope = (scope == 0)
            ? cvis[(size_t) ci]
            : (!cvis[(size_t) ci]
               && st->char_seen != nullptr && st->char_seen[ci]);
          if (!inscope)
            continue;
          int matched = 0;
          if (c->name != nullptr && has_word (lower (c->name)))
            matched = 1;
          /* The runner compares descriptors CASE-SENSITIVELY against the lowercased
             input (GrabIt: `If sWord = sName` -- only ProperName gets a
             .ToLower).  So a capitalised descriptor ("Talia") never matches
             and the pronoun keeps its previous referent; lowercasing here
             wrongly updated `her` to Talia Swayne where the runner keeps Sophia. */
          for (int d = 0; !matched && d < c->n_descriptors; d++)
            if (has_word (c->descriptors[d]))
              matched = 1;
          if (!matched)
            continue;
          const char *g = a5state_entity_prop (st, c->key, "Gender");
          if (g != nullptr && lower (g) == "male")
            add_uniq (him_keys, c->key);
          else if (g != nullptr && lower (g) == "female")
            add_uniq (her_keys, c->key);
          else
            add_uniq (it_keys, c->key);
        }

      /* Resolve each class that is now unambiguous (or narrowable by prefix). */
      struct { std::string *out; std::vector<const char *> *keys; } cls[] = {
        { &new_it,   &it_keys },
        { &new_them, &them_keys },
        { &new_him,  &him_keys },
        { &new_her,  &her_keys },
      };
      for (auto &cl : cls)
        {
          if (!cl.out->empty ())
            continue;
          std::vector<const char *> *ks = cl.keys;
          const char *chosen = nullptr;
          if (ks->size () == 1)
            chosen = (*ks)[0];
          else if (ks->size () > 1)
            {
              /* Second pass: keep candidates whose prefix word is in the input. */
              const char *only = nullptr;
              int n = 0;
              for (auto *k : *ks)
                {
                  const a5_object_t *o = a5model_object (st->adv, k);
                  const a5_character_t *c = a5model_character (st->adv, k);
                  const char *prefix = o ? o->prefix : (c ? c->prefix : nullptr);
                  int hit = 0;
                  for (auto &pw : split_ws (prefix))
                    if (has_word (lower (pw))) { hit = 1; break; }
                  if (hit) { only = k; n++; }
                }
              if (n == 1)
                chosen = only;
            }
          if (chosen != nullptr)
            {
              const a5_object_t *o = a5model_object (st->adv, chosen);
              const a5_character_t *c = a5model_character (st->adv, chosen);
              char *nm = o ? a5text_object_name (st, o, A5_ART_DEFINITE)
                           : (c ? a5text_character_known_name (st, c, 1)
                                : strdup (chosen));
              *cl.out = nm ? nm : "";
              free (nm);
            }
        }
    }

  if (!new_it.empty ())   { free (st->s_it);   st->s_it = strdup (new_it.c_str ()); }
  if (!new_them.empty ()) { free (st->s_them); st->s_them = strdup (new_them.c_str ()); }
  if (!new_him.empty ())  { free (st->s_him);  st->s_him = strdup (new_him.c_str ()); }
  if (!new_her.empty ())  { free (st->s_her);  st->s_her = strdup (new_her.c_str ()); }

  /* Defaults so a later "it"/etc. with no referent prints the runner's placeholder. */
  if (st->s_it[0]   == '\0') { free (st->s_it);   st->s_it   = strdup ("Absolutely Nothing"); }
  if (st->s_them[0] == '\0') { free (st->s_them); st->s_them = strdup ("Absolutely Nothing"); }
  if (st->s_him[0]  == '\0') { free (st->s_him);  st->s_him  = strdup ("No male"); }
  if (st->s_her[0]  == '\0') { free (st->s_her);  st->s_her  = strdup ("No female"); }
}

/* Substitute "it"/"them"/"him"/"her" in the player's input with the last
   referenced object/character name, emitting the runner's "(name)" echo line, then
   recompute the referents for next turn (clsUserSession.EvaluateInput). */
void
resolve_pronouns (a5_run_t *run, std::string &in, sb_t *out)
{
  a5_state_t *st = run->st;
  struct { const char *word; char *val; } pr[] = {
    { "it",   st->s_it },
    { "them", st->s_them },
    { "him",  st->s_him },
    { "her",  st->s_her },
  };
  for (auto &p : pr)
    if (conv_contains_word (in, p.word) && p.val != nullptr && p.val[0])
      {
        sb_puts (out, "(");
        sb_puts (out, p.val);
        sb_puts (out, ")\n");
        replace_word (in, p.word, lower (p.val));
      }
  grab_it (run, in);
}

/* <Synonym> word/phrase substitution (clsUserSession.EvaluateInput's synonym
   loop, run right after GrabIt): for each synonym, for each of its <From>
   phrases, if every word of the phrase appears in the input (ContainsWord),
   replace the phrase with its <To> word (ReplaceWord).  Later synonyms see
   earlier synonyms' replacements, exactly as the runner's single `sInput` threaded
   through the loop. */
void
resolve_synonyms (a5_run_t *run, std::string &in)
{
  const a5_adventure_t *adv = run->adv;
  int i, j;
  for (i = 0; i < adv->n_synonyms; i++)
    {
      const a5_synonym_t *syn = &adv->synonyms[i];
      if (syn->to == nullptr)
        continue;
      for (j = 0; j < syn->n_from; j++)
        {
          if (syn->from[j] == nullptr)
            continue;
          std::string from = lower (syn->from[j]);
          if (conv_contains_word (in, from))
            replace_word (in, from, lower (syn->to));
        }
    }
}
