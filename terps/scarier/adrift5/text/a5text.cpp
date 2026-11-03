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
 * ADRIFT 5 support for Scarier -- text engine.  See a5text.h.
 *
 * This file holds the description evaluator and the display pipeline (ALR
 * overrides, auto-capitalisation, perspective, <#expressions#>, the
 * describe_* entry points).  The rest of the engine is split across
 * a5text_func.cpp (%function% replacement), a5text_name.cpp (entity naming),
 * a5text_render.cpp (markup to plain text) and a5text_view.cpp (the room
 * view); a5text_internal.h declares what they share.
 */

#include <unordered_map>

#include "a5text_internal.h"

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
int
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
  sb_t *sb = psb;
  const a5_xml_node_t *c;
  const char *default_text = *pdefault;
  int first = *pfirst;
  int terminated = 0;

  if (wrapper == nullptr)
    return 0;

  for (c = wrapper->first_child; c != nullptr; c = c->next)
    {
      const a5_xml_node_t *restr;
      const char *when, *text;
      int once;
      if (strcmp (c->name, "Description") != 0)
        continue;
      text = a5xml_child_text (c, "Text");
      if (text == nullptr)
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
      if (restr != nullptr && !a5restr_pass (st, restr))
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
          continue;
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
            for (const a5_xml_node_t *r = wrapper->first_child; r != nullptr;
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
          sb_clear (sb);
          sb_puts (sb, text);
        }
      else if (streq (when, "StartAfterDefaultDescription"))
        {
          sb_clear (sb);
          if (!first)
            {
              sb_puts (sb, default_text);
              if (add_space (sb->p, sb->len))
                sb_puts (sb, "  ");
            }
          sb_puts (sb, text);
        }
      else /* AppendToPreviousDescription (default) */
        {
          if (add_space (sb->p, sb->len))
            sb_puts (sb, "  ");
          /* The "<>" marker (the Adrift 5 runner ToString) is a zero-width separator: it
             keeps an unexpanded "%x%.Prop" at the join from merging with the next
             segment's text into one corrupt token.  render_plain drops it. */
          sb_puts (sb, "<>");
          sb_puts (sb, text);
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

  *pfirst = first;
  *pdefault = default_text;
  return terminated;
}

char *
a5text_eval_description (a5_state_t *st, const a5_xml_node_t *wrapper)
{
  sb_t sb;
  const char *default_text = nullptr;
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
  const a5_location_t *l = lockey ? a5model_location (st->adv, lockey) : nullptr;
  const a5_prop_t *grp;
  sb_t sb;
  const char *default_text = nullptr;
  int first = 1, term;
  sb_init (&sb);
  if (l == nullptr)
    return sb_finish (&sb);
  term = eval_desc_into (st, &sb, &first, &default_text,
                         a5xml_child (l->node, tag));
  grp = a5state_location_group_prop (st, lockey, group_prop);
  if (!term && grp != nullptr && grp->value_node != nullptr)
    eval_desc_into (st, &sb, &first, &default_text, grp->value_node);
  return sb_finish (&sb);
}

char *
a5text_location_short (a5_state_t *st, const char *lockey)
{
  return location_desc (st, lockey, "ShortDescription",
                        "ShortLocationDescription");
}

char *
location_long_desc (a5_state_t *st, const char *lockey)
{
  return location_desc (st, lockey, "LongDescription",
                        "LongLocationDescription");
}

/* --------------------------------------------------------- ALR replacement */

/* Replace every occurrence of `find` in `src` with `repl`. */
char *
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
      if (hit == nullptr)
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

char *
replace_alrs (a5_state_t *st, const char *src, int depth)
{
  char *cur = strdup (src);
  int i;
  if (depth > 8)
    return cur;
  for (i = 0; i < st->adv->n_alrs; i++)
    {
      const a5_alr_t *alr = &st->adv->alrs[i];
      if (alr->old_text == nullptr || alr->old_text[0] == '\0')
        continue;
      if (strstr (cur, alr->old_text) != nullptr)
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
static inline int
a5_is_lower (char c)
{
  return c >= 'a' && c <= 'z';
}

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
      && a5_is_lower (s[i - 3]))
    return 1;
  /* "x.  y" (two spaces) where x is a lowercase letter */
  if (i >= 4 && s[i - 1] == ' ' && s[i - 2] == ' '
      && (s[i - 3] == '.' || s[i - 3] == '!' || s[i - 3] == '?')
      && a5_is_lower (s[i - 4]))
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

char *
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
          const char *s1 = nullptr, *s2 = nullptr;
          while (*q && *q != ']' && *q != '[' && *q != '\n')
            {
              if (*q == '/')
                { if (s1 == nullptr) s1 = q; else if (s2 == nullptr) s2 = q; }
              q++;
            }
          if (*q == ']' && s2 != nullptr)
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
  if (s == nullptr || *s == '\0')
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
                  if (fk != nullptr)
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
                char *val = eval_function (st, name, nullptr);
                free (name);
                if (val != nullptr)
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
                  if (val != nullptr)
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
int
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
      if (cb == nullptr || cb[1] != '%')
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

/* `\005<orig>\005` prefix for a display_defers entry that repeats entry
   <orig>'s `<#..#>` body within one block (see replace_expressions). */
static std::string
defer_dup_prefix (size_t orig)
{
  char buf[32];
  snprintf (buf, sizeof buf, "\005%d\005", (int) orig);
  return std::string (buf);
}

/* Evaluate every `<#...#>` expression in `src` (the Adrift 5 runner ReplaceExpressions:
   substitute the body, then reduce it to its string value). */
static char *
replace_expressions (a5_state_t *st, const char *src)
{
  sb_t sb;
  const char *p = src;
  /* Identical `<#..#>` bodies in ONE block show ONE value.  The runner's
     ReplaceExpressions (Global.vb:510) collects its regex matches up front
     and then does `sText = sText.Replace(m.Value, EvaluateExpression(..))`
     per match -- a replace-ALL, so the first match's value lands in every
     identical slot, while the later matches still call EvaluateExpression
     (their OneOf/Rand/URand DRAW, keeping the RNG stream in step) and have
     nowhere left to land.  The Last Expedition's "<#oneOf("Eight","Seven",..)#>
     minutes later they both came to. .. <#same#> minutes later they both came
     to their sense again" and Lost Coastlines' "<# Oneof("two","three","four")#>
     return empty handed...<#same#> do not return at all" print the same number
     twice in the runner.  Keyed on the raw body (the substituted text is
     identical for identical raw bodies within one block); a deferred repeat
     is pushed as `\005<orig>\005body` so the flush draws it but displays
     entry <orig>'s value (a5run_draw_defer_entry). */
  std::unordered_map<std::string, std::string> seen_val;
  std::unordered_map<std::string, size_t> seen_slot;

  /* Push `body` on the deferred-expression sink and leave a `\004<idx>\004`
     sentinel in the text where its value belongs. */
  auto defer = [&] (const std::string &key, bool repeat, std::string body)
    {
      std::vector<std::string> *sink =
          (std::vector<std::string> *) st->expr_defer;
      char mark[24];

      snprintf (mark, sizeof mark, "\004%d\004", (int) sink->size ());
      if (repeat && seen_slot.count (key))
        body = defer_dup_prefix (seen_slot[key]) + body;
      else
        seen_slot[key] = sink->size ();
      sink->push_back (body);
      sb_puts (&sb, mark);
    };

  if (strstr (src, "<#") == nullptr)
    return strdup (src);

  sb_init (&sb);
  while (*p != '\0')
    {
      if (p[0] == '<' && p[1] == '#')
        {
          const char *end = strstr (p + 2, "#>");
          if (end != nullptr)
            {
              char *inner = strndup (p + 2, (size_t) (end - (p + 2)));
              const std::string key (inner);
              const bool repeat = seen_val.count (key) || seen_slot.count (key);
              if (st->expr_defer != nullptr && expr_sub_draws_random (inner))
                {
                  /* Raw-body deferral (see expr_sub_draws_random): push the
                     UNSUBSTITUTED body, tagged \003, so both its substitution
                     draws and its reduce happen at the end-of-command flush,
                     in this block's text position. */
                  defer (key, repeat, std::string ("\003") + inner);
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
              if (st->expr_defer != nullptr && expr_bears_random (oo))
                {
                  defer (key, repeat, oo);
                  free (inner); free (sub); free (oo);
                  p = end + 2;
                  continue;
                }
              char *val = a5_eval_sexpr (oo);     /* draws even on a repeat */
              if (repeat && seen_val.count (key))
                sb_puts (&sb, seen_val[key].c_str ());
              else
                { seen_val[key] = val; sb_puts (&sb, val); }
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
  if (expr == nullptr)
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
  if (getenv ("A5_TRACE_EXPR") != nullptr)
    fprintf (stderr, "A5_EXPR: %s  ==>  %s  ==>  %s\n", expr, sub, val);
  free (sub);
  return val;   /* a5_eval_sexpr never returns NULL */
}

/* Swap each `<#...#>` for a \x01<n>\x01 sentinel so the %function%/OO passes
   leave the expression body untouched (the Adrift 5 runner guards them behind GUIDs);
   the sentinel survives both passes verbatim and is restored before
   replace_expressions evaluates the bodies. */
std::string
protect_exprs (const char *src, std::vector<std::string> &saved)
{
  std::string out;
  const char *p = src;
  while (*p != '\0')
    {
      if (p[0] == '<' && p[1] == '#')
        {
          const char *end = strstr (p + 2, "#>");
          if (end != nullptr)
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

std::string
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

/* The substitution half of the runner's text pipeline, in its order:
   %functions%/%variables%, key.Property OO chains, then <#expressions#>.
   The <#...#> blocks are shielded from the first two passes so a function
   result cannot be re-parsed as expression syntax. */
static char *
substitute_text (a5_state_t *st, const char *src)
{
  std::vector<std::string> saved;
  std::string prot = protect_exprs (src, saved);
  char *funcs = replace_functions (st, prot.c_str ());
  char *oo = a5expr_replace (st, funcs);
  std::string restored = restore_exprs (oo, saved);
  char *exprs = replace_expressions (st, restored.c_str ());
  free (funcs);
  free (oo);
  return exprs;
}

static char *
process_inner_ex (a5_state_t *st, const char *src, int depth, int *pre_alr_ink)
{
  char *exprs = substitute_text (st, src);
  char *alrs;
  if (pre_alr_ink != nullptr)
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
          if (a5_is_span_mark (*q))
            {
              /* Skip the \006<number>\006 / \022<name>\022 / \024<index>\024
                 / \026<seconds>\026 / \027<colour>\027 span (the window name
                 and the colour are presentation, not visible ink), or a stray
                 unpaired mark. */
              const char *e = strchr (q + 1, *q);
              if (e == nullptr)
                continue;
              q = e;
            }
          else if (*q != '\n' && *q != '\r' && *q != ' ' && *q != '\t'
                   && !a5_is_pres_mark (*q))
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
char *
seal_alr_sites (a5_state_t *st, char *text)
{
  int i;
  if (st == nullptr || st->adv == nullptr || st->adv->n_alrs == 0 || text == nullptr)
    return text;
  for (i = 0; i < st->adv->n_alrs; i++)
    {
      const a5_alr_t *alr = &st->adv->alrs[i];
      const char *old = alr->old_text;
      if (old == nullptr || old[0] == '\0' || strstr (text, old) == nullptr)
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

char *
process_inner (a5_state_t *st, const char *src, int depth)
{
  return process_inner_ex (st, src, depth, nullptr);
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
  return substitute_text (st, src ? src : "");
}

void
a5text_strip_pres_marks (char *s)
{
  char *r, *w;

  if (s == nullptr)
    return;
  for (r = w = s; *r != '\0'; r++)
    {
      if (a5_is_span_mark (*r))
        {
          /* A spanning mark goes with its payload: \006<number>\006 and
             friends, or nothing at all if the closing mark is missing. */
          const char *e = strchr (r + 1, *r);

          if (e != nullptr)
            r = (char *) e;
          continue;
        }
      if (a5_is_pres_mark (*r)
          || *r == A5_CLS_MARK || *r == A5_PS_MARK || *r == A5_COMMIT_MARK)
        continue;
      *w++ = *r;
    }
  *w = '\0';
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
  long k = inner != nullptr ? (long) (inner - repl) : -1;
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
      if (alr->old_text == nullptr || alr->old_text[0] == '\0')
        continue;
      if (strstr (cur, alr->old_text) != nullptr)
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
  if (plain == nullptr)
    return strdup ("");
  /* No ALRs -> the boundary is a pure pass-through (English games never pay for
     this; their per-fragment ALR already ran).  Mirrors Display() doing nothing
     when htblALRs is empty. */
  if (st == nullptr || st->adv == nullptr || st->adv->n_alrs == 0)
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
  const char *p = text != nullptr ? text : "";
  if (strchr (p, A5_VARDEF_MARK) == nullptr)
    return strdup (p);
  sb_init (&sb);
  while (*p != '\0')
    {
      const char *e;
      if (*p == A5_VARDEF_MARK && (e = strchr (p + 1, A5_VARDEF_MARK)) != nullptr)
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
   (`...\n<font colour=X>`), a trailing `<br>`, or an entity -- leaves the buffer
   non-vbLf and the NEXT message space-joins with two leading spaces.  Scarier
   strips markup per message, so its stripped text ends in the '\n' that preceded
   the trailing tag and it would drop that join.  When the pre-strip text ends
   non-'\n' but the stripped text ends in '\n', append A5_PS_MARK so sb_pspace
   still treats this message as a non-newline tail (finish_turn strips the mark). */
char *
ps_mark_trailing (const char *proc, char *plain)
{
  size_t pl = strlen (proc), tl = strlen (plain);
  if (pl > 0 && proc[pl - 1] != '\n' && tl > 0 && plain[tl - 1] == '\n')
    {
      char *np = (char *) realloc (plain, tl + 2);
      if (np != nullptr) { np[tl] = A5_PS_MARK; np[tl + 1] = '\0'; return np; }
    }
  return plain;
}

char *
a5text_describe (a5_state_t *st, const a5_xml_node_t *wrapper, int *raw_nonblank)
{
  char *raw = a5text_eval_description (st, wrapper);
  if (raw_nonblank != nullptr)
    *raw_nonblank = (raw != nullptr && raw[0] != '\0');
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
  if (raw_nonblank != nullptr)
    *raw_nonblank = (raw != nullptr && raw[0] != '\0');
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
  if (marked != nullptr)
    *marked = strdup (capped != nullptr ? capped : "");
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
