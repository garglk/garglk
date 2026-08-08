/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- command-pattern matcher.  See a5parse.h.
 *
 * A direct port of the Adrift 5 runner clsUserSession.ConvertToRE (clsUserSession.vb:
 * 6298) and the syntactic half of InputMatchesCommand (5575): the structural
 * transforms ([a/b] alternation, {..} optional, '_' space, '*' wildcard) and the
 * reference -> capture-group substitution, then a std::regex match that hands the
 * captured reference text back to the (semantic) resolver in a5run.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "a5parse.h"

/* Lowercase a UTF-8 byte stream the way VB.NET's LCase does (the Adrift 5 runner's
   direction RE / known-word set).  A byte-wise tolower() folds only ASCII, so a
   game's accented capitals -- Danish 'Ø' (C3 98), 'Æ' (C3 86), 'Å' (C3 85),
   German 'Ä/Ö/Ü', French 'É' ... -- survive uppercased and never match the
   lowercase word the player types (e.g. DirectionEast "Øst/Ø/..." vs input
   "øst").  Fold the Latin-1 supplement U+00C0..U+00DE (lead byte 0xC3, trail
   0x80..0x9E, excluding 0x97 = MULTIPLICATION SIGN, which has no case) by adding
   the 0x20 upper->lower offset to the trail byte; everything else is ASCII. */
static std::string
lower_utf8 (const char *s)
{
  std::string o;
  const unsigned char *p = (const unsigned char *) s;
  while (*p)
    {
      if (p[0] == 0xC3 && p[1] >= 0x80 && p[1] <= 0x9E && p[1] != 0x97)
        { o += (char) 0xC3; o += (char) (p[1] + 0x20); p += 2; }
      else
        { o += (char) tolower (*p); p++; }
    }
  return o;
}

/* --------------------------------------------------------- direction table */

/*
 * Maps every spelling ADRIFT accepts to the PascalCase key used in a location's
 * <Movement><Direction> (clsAdventure.vb:748 sDirectionsRE).  The lowercase
 * alternation strings here feed both the regex and a5parse_canonical_direction.
 */
struct dir_entry { const char *canonical; const char *re; };
static const dir_entry kDirs[] = {
  { "North",     "north|n" },
  { "NorthEast", "northeast|ne|north-east|n-e" },
  { "East",      "east|e" },
  { "SouthEast", "southeast|se|south-east|s-e" },
  { "South",     "south|s" },
  { "SouthWest", "southwest|sw|south-west|s-w" },
  { "West",      "west|w" },
  { "NorthWest", "northwest|nw|north-west|n-w" },
  { "In",        "in|inside" },
  { "Out",       "out|o|outside" },
  { "Up",        "up|u" },
  { "Down",      "down|d" },
};
static const int kNumDirs = (int) (sizeof kDirs / sizeof kDirs[0]);

/*
 * Per-direction runtime spec (the localization subsystem).  Defaults to the
 * English kDirs; a5parse_set_directions overrides selected entries from the
 * game's <Direction*> header fields (clsAdventure.sDirectionsRE).  `re` is the
 * lowercased "|"-separated synonym alternation used for parsing/regex; `name`
 * is the display name -- the first synonym in its authored case
 * (Global.DirectionName, the substring before the first separator).
 */
static struct { std::string re; std::string name; } g_dirs[kNumDirs];
static bool g_dirs_init = false;

/* DirectionsEnum value -> canonical key, for mapping the model's enum-ordered
   <Direction*> array onto kDirs. */
static const char *const kDirEnum[12] = {
  "North", "East", "South", "West", "Up", "Down",
  "In", "Out", "NorthEast", "SouthEast", "SouthWest", "NorthWest" };

/* The English display name of a canonical direction is the canonical key
   itself ("North/N" -> DirectionName "North"). */
static void
ensure_dirs_default ()
{
  int j;
  if (g_dirs_init)
    return;
  for (j = 0; j < kNumDirs; j++)
    {
      g_dirs[j].re = kDirs[j].re;
      g_dirs[j].name = kDirs[j].canonical;
    }
  g_dirs_init = true;
}

static std::string cached_dirs_re;   /* invalidated by a5parse_set_directions */

/* Compiled-regex cache, keyed by the ORIGINAL command pattern; each entry
   holds every wildcard variant pre-expanded and pre-compiled.  convert_to_re +
   std::regex compilation measured ~8-10% of run time -- it recompiled the same
   pattern on every task-command probe, every turn -- yet a pattern always
   yields the same regexes and ref names.  Caching the whole variant list also
   drops the per-probe variant_pattern() string rebuild and the hash of the
   (longer) expanded string that the old per-variant cache paid.  Invalidated
   alongside cached_dirs_re when the direction table changes, since a
   %direction% group embeds directions_re() output.

   `prefixes` is a first-word prefilter derived from the compiled regex: when
   non-empty, any input the regex could accept MUST start (ASCII
   case-insensitively) with one of these lowercase literals, so a cheap prefix
   test rejects most of the task list without touching std::regex execution --
   which profiling showed dominating scan_tasks (50%+ of a stress replay).
   Empty means the filter could not be derived; the regex always runs. */
struct a5_compiled_pattern
{
  bool ok = false;
  std::regex re;
  std::vector<std::string> refs;
  std::vector<std::string> prefixes;
};

struct a5_pattern_variants
{
  int total = 1;
  std::vector<a5_compiled_pattern> v;   /* size == total */
};
static std::unordered_map<std::string, a5_pattern_variants> cached_variants;

void
a5parse_set_directions (const char *const *raw_by_enum)
{
  int e, j;
  /* Reset to the English defaults first so the call is idempotent and a later
     game with no overrides cannot inherit an earlier game's localization. */
  g_dirs_init = false;
  ensure_dirs_default ();
  cached_dirs_re.clear ();
  cached_variants.clear ();
  if (raw_by_enum == NULL)
    return;
  for (e = 0; e < 12; e++)
    {
      const char *raw = raw_by_enum[e];
      if (raw == NULL || raw[0] == '\0')
        continue;
      for (j = 0; j < kNumDirs; j++)
        if (strcmp (kDirs[j].canonical, kDirEnum[e]) == 0)
          {
            /* Display name = first synonym (authored case), before the first
               '/' (Global.DirectionName). */
            const char *slash = strchr (raw, '/');
            g_dirs[j].name = slash ? std::string (raw, slash - raw) : std::string (raw);
            /* Parse/regex form: lowercased (UTF-8 aware, so localized accented
               synonyms like "Øst" fold to "øst"), '/' -> '|'
               (Global.DirectionRE). */
            std::string re = lower_utf8 (raw);
            for (char &c : re) if (c == '/') c = '|';
            g_dirs[j].re = re;
            break;
          }
    }
}

const char *
a5parse_direction_name (const char *canonical)
{
  int j;
  if (canonical == NULL)
    return NULL;
  ensure_dirs_default ();
  for (j = 0; j < kNumDirs; j++)
    if (strcmp (kDirs[j].canonical, canonical) == 0)
      return g_dirs[j].name.c_str ();
  return NULL;
}

/* The full direction alternation, lowercased, built once.  The Adrift 5 runner's
   %direction% builder loops `For eDirection = North To NorthWest` -- and by the
   DirectionsEnum VALUES (North=0, East=1, South=2, West=3, Up=4, Down=5, In=6,
   Out=7, NorthEast=8, SouthEast=9, SouthWest=10, NorthWest=11) that range spans
   ALL TWELVE directions, including Up/Down/In/Out.  (An earlier reading took the
   names literally and emitted only the 8 compass points, which made bare "up"/
   "u"/"in"/"out" fail to match movement tasks -- e.g. Anno 1700's upstairs.) */
static std::string
directions_re ()
{
  ensure_dirs_default ();
  if (!cached_dirs_re.empty ())
    return cached_dirs_re;
  std::string s;
  const int n = 12;
  for (int i = 0; i < n; i++)
    {
      for (int j = 0; j < kNumDirs; j++)
        if (strcmp (kDirs[j].canonical, kDirEnum[i]) == 0)
          { s += g_dirs[j].re; break; }
      if (i != n - 1)
        s += "|";
    }
  cached_dirs_re = s;
  return cached_dirs_re;
}

/* The full lowercased direction alternation ("north|n|...|down|d"), exposed so
   the turn loop can seed listKnownWords (clsUserSession NotUnderstood splits
   sDirectionsRE on "|"). */
const char *
a5parse_directions_re (void)
{
  if (cached_dirs_re.empty ())
    cached_dirs_re = directions_re ();
  return cached_dirs_re.c_str ();
}

const char *
a5parse_canonical_direction (const char *text)
{
  std::string t;
  if (text == NULL)
    return NULL;
  /* UTF-8-aware lowercase (so "Øst"/"ØST" canonicalize like "øst"), then drop
     whitespace so "south east" matches the "southeast" synonym. */
  std::string low = lower_utf8 (text);
  for (char c : low)
    if (!isspace ((unsigned char) c))
      t += c;
  if (t.empty ())
    return NULL;
  ensure_dirs_default ();
  /* Resolve synonyms in DirectionsEnum order (kDirEnum), NOT raw compass order,
     mirroring the Adrift 5 runner's `For eDirection = North To NorthWest` scan.  This
     only matters when a game's <Direction*> overrides make two directions share
     a synonym: Space Detective ep.4 redefines NorthEast's words to "west"/"w"
     while West keeps its default "west"/"w", so a compass-order scan (NorthEast
     precedes West) mis-binds "west" to the non-existent NorthEast exit.  Enum
     order puts West (=3) ahead of NorthEast (=8), so West wins the tie like the runner.
     Default games have disjoint synonyms, so the order is inert for them. */
  for (int i = 0; i < 12; i++)
    {
      int j;
      for (j = 0; j < kNumDirs; j++)
        if (strcmp (kDirs[j].canonical, kDirEnum[i]) == 0)
          break;
      if (j >= kNumDirs)
        continue;
      std::string re = g_dirs[j].re;
      size_t start = 0, bar;
      do {
        bar = re.find ('|', start);
        std::string alt = re.substr (start, bar - start);
        /* compare ignoring '-' and whitespace so "south-east" == "southeast";
           the whitespace-blind compare must apply to BOTH sides -- a localized
           multi-word synonym (Thy Dunjohnman's DirectionEast "chamber pot")
           otherwise never canonicalizes, because the input `t` above was
           space-stripped but the alternative kept its space. */
        std::string a, b;
        for (char c : alt) if (c != '-' && !isspace ((unsigned char) c)) a += c;
        for (char c : t)   if (c != '-') b += c;
        if (a == b)
          return kDirs[j].canonical;
        start = bar + 1;
      } while (bar != std::string::npos);
    }
  return NULL;
}

/* ---------------------------------------------------------- string helpers */

static void
replace_all (std::string &s, const std::string &from, const std::string &to)
{
  if (from.empty ())
    return;
  size_t pos = 0;
  while ((pos = s.find (from, pos)) != std::string::npos)
    {
      s.replace (pos, from.size (), to);
      pos += to.size ();
    }
}

/* Normalise singular references to the numbered form (FileIO.vb:647). */
static void
normalise_refs (std::string &s)
{
  replace_all (s, "%object%",    "%object1%");
  replace_all (s, "%character%", "%character1%");
  replace_all (s, "%location%",  "%location1%");
  replace_all (s, "%number%",    "%number1%");
  replace_all (s, "%text%",      "%text1%");
  replace_all (s, "%item%",      "%item1%");
  replace_all (s, "%direction%", "%direction1%");
}

/* ------------------------------------------------------------- ConvertToRE */

/*
 * Build the regex pattern for a command and record the reference group names in
 * capture order.  Structural groups are non-capturing ((?:...)) so the only
 * capturing groups -- and hence the only ones std::smatch reports -- are the
 * references, left to right in `out_refs`.
 */
static bool
convert_to_re (const char *pattern, std::string &out_pattern,
               std::vector<std::string> &out_refs)
{
  std::string sC = pattern ? pattern : "";

  /* A leading '#' marks a disabled/comment command. */
  {
    size_t i = 0;
    while (i < sC.size () && sC[i] == ' ') i++;
    if (i < sC.size () && sC[i] == '#')
      return false;
  }

  normalise_refs (sC);

  /* Escape literal regex specials that ADRIFT treats as text. */
  replace_all (sC, "+", "\\+");
  replace_all (sC, "(", "\\(");
  replace_all (sC, ")", "\\)");

  /* Wildcards (non-capturing variants of the Adrift 5 runner's groups).  Like
     the Adrift 5 runner (ConvertToRE), substitute a placeholder for the wildcard body
     and expand it to ".*?" only at the very end -- otherwise the bare-'*' pass
     re-rewrites the '*' inside an already-inserted ".*?", corrupting the regex
     (e.g. "{the} *[string/twig]" never matched "get twig"). */
  replace_all (sC, " * ", " (?:\x01 )?");
  replace_all (sC, "* ",  "(?:\x01 )?");
  replace_all (sC, " *",  "(?: \x01)?");
  replace_all (sC, "*",   "\x01");
  replace_all (sC, "\x01", ".*?");

  replace_all (sC, "_", " ");

  /* Optional / mandatory text and alternation -> non-capturing groups. */
  replace_all (sC, "{", "(?:");
  replace_all (sC, "}", ")?");
  replace_all (sC, "[", "(?:");
  replace_all (sC, "]", ")");
  replace_all (sC, "/", "|");

  /* Replace references with capturing groups, recording names in order. */
  std::string dirRE = directions_re ();
  std::string result;
  result.reserve (sC.size () + 32);
  for (size_t i = 0; i < sC.size (); )
    {
      if (sC[i] == '%')
        {
          size_t j = i + 1;
          while (j < sC.size () && (isalnum ((unsigned char) sC[j]) || sC[j] == '_'))
            j++;
          if (j < sC.size () && sC[j] == '%' && j > i + 1)
            {
              std::string name = sC.substr (i + 1, j - i - 1);
              /* base = name without trailing digits, to pick the value class */
              std::string base = name;
              while (!base.empty () && isdigit ((unsigned char) base.back ()))
                base.pop_back ();
              if (base == "number")
                result += "(-?[0-9]+)";
              else if (base == "direction")
                result += "(" + dirRE + ")";
              else /* object(s), character(s), text, location, item */
                result += "(.+?)";
              out_refs.push_back (name);
              i = j + 1;
              continue;
            }
        }
      result += sC[i];
      i++;
    }

  /*
   * NOTE: Scarier used to relax a literal space following an optional group
   * (")? " -> ")? ?") here, to fake the bare-form matching that real ADRIFT gets
   * from clsUserSession.CorrectCommand restructuring `{x} y` into `{x }y`.  Now
   * that a5model applies CorrectCommand (a5_correct_command) at load -- exactly
   * as the Adrift 5 runner does at game-start init -- this matcher must mirror the runner's
   * ConvertToRE *verbatim* (no extra space relaxation), or the two double-relax
   * and over-match.
   */

  /* Trim and anchor. */
  size_t a = result.find_first_not_of (' ');
  size_t b = result.find_last_not_of (' ');
  if (a == std::string::npos)
    result.clear ();
  else
    result = result.substr (a, b - a + 1);
  out_pattern = "^" + result + "$";
  if (getenv ("A5_TRACE_RE"))
    fprintf (stderr, "A5_TRACE_RE: \"%s\" -> %s\n", pattern, out_pattern.c_str ());
  return true;
}

/* ------------------------------------------------ CorrectCommand (bracket fix) */

/*
 * A verbatim port of clsUserSession.CorrectCommand / ProcessBlock / GetSubBlock
 * / ContainsMandatoryText (clsUserSession.vb:6126-6295).  The Adrift 5 runner applies
 * this to every task command at game-start init, restructuring an optional
 * group so an adjacent literal space becomes optional too: `{x} y` -> `{x }y`,
 * `{a/b}` -> `{ [a/b]}`.  a5model calls a5_correct_command at load on every
 * command, after which a5parse_match_command's ConvertToRE mirrors the runner exactly.
 */

/* GetSubBlock: peel the next token off sBlock (modified in place) -- either the
   leading plain text up to a top-level bracket, a complete bracketed block, or
   text up to and including a top-level '/'. */
static std::string
cc_get_sub_block (std::string &sBlock)
{
  int iDepth = 0;
  std::string sNewBlock;
  int len = (int) sBlock.size ();

  for (int i = 0; i < len; i++)
    {
      char c = sBlock[i];
      sNewBlock += c;
      switch (c)
        {
        case '{':
        case '[':
          if (iDepth == 0 && i > 0)
            {
              sBlock = sBlock.substr (i);       /* keep bracket char onward */
              return sNewBlock.substr (0, i);   /* sLeft(sNewBlock, i)      */
            }
          iDepth += 1;
          break;
        case ']':
        case '}':
          iDepth -= 1;
          if (iDepth == 0)
            {
              sBlock = sBlock.substr (i + 1);
              return sNewBlock;
            }
          break;
        case '/':
          if (iDepth == 0)
            {
              sBlock = sBlock.substr (i + 1);
              return sNewBlock;
            }
          break;
        }
    }
  sBlock = "";
  return sNewBlock;
}

/* ContainsMandatoryText: true if any part of the block is mandatory (inside []
   or not in {} brackets at all -- spaces ignored). */
static bool
cc_contains_mandatory (const std::string &sBlock)
{
  if (sBlock.empty ())
    return false;

  int iLevel = 0;
  for (char c : sBlock)
    {
      switch (c)
        {
        case ' ':
          break;
        case '{':
          iLevel += 1;
          break;
        case '}':
          iLevel -= 1;
          break;
        default:
          if (iLevel == 0)
            return true;
        }
    }
  return false;
}

static std::string
cc_process_block (const std::string &sBlock)
{
  std::string sAfter = sBlock;
  std::string sNextBlock;
  std::string sBefore;

  do
    {
      sNextBlock = cc_get_sub_block (sAfter);
      if (!sNextBlock.empty ())
        {
          if (sNextBlock[0] == '{')
            {
              bool bContainsMandatory = cc_contains_mandatory (sAfter);
              if (bContainsMandatory && !sAfter.empty () && sAfter[0] == ' ')
                {
                  /* before final [] block: _{x}_ => _{x_} */
                  if (sBefore.empty () || sBefore.back () == ' '
                      || sBefore.back () == '}')
                    {
                      if (sNextBlock.find ('/') != std::string::npos)
                        sNextBlock = "{["
                            + sNextBlock.substr (1, sNextBlock.size () - 2)
                            + "] }";
                      else
                        sNextBlock = sNextBlock.substr (0, sNextBlock.size () - 1)
                            + " }";
                      sAfter = sAfter.substr (1);
                    }
                }
              else if (!bContainsMandatory && !sBefore.empty ()
                       && sBefore.back () == ' ')
                {
                  /* after final [] block: _{x}_ => {_x}_ */
                  if (sNextBlock.find ('/') != std::string::npos)
                    sNextBlock = "{ ["
                        + sNextBlock.substr (1, sNextBlock.size () - 2) + "]}";
                  else
                    sNextBlock = "{ " + sNextBlock.substr (1);
                  sBefore = sBefore.substr (0, sBefore.size () - 1);
                }

              /* End block: " {#}" => "{ #}" or "{ [#/#]}" */
              if (!sBefore.empty () && sBefore.back () == ' ' && sAfter.empty ())
                {
                  if (sNextBlock.find ('/') != std::string::npos)
                    sNextBlock = "{ ["
                        + sNextBlock.substr (1, sNextBlock.size () - 2) + "]}";
                  else
                    sNextBlock = "{ " + sNextBlock.substr (1);
                  sBefore = sBefore.substr (0, sBefore.size () - 1);
                }
              sBefore += "{"
                  + cc_process_block (sNextBlock.substr (1, sNextBlock.size () - 2))
                  + "}";
            }
          else if (sNextBlock[0] == '[')
            {
              sBefore += "["
                  + cc_process_block (sNextBlock.substr (1, sNextBlock.size () - 2))
                  + "]";
            }
          else if (sNextBlock.back () == '/')
            {
              sBefore += cc_process_block (
                             sNextBlock.substr (0, sNextBlock.size () - 1))
                  + "/";
            }
          else
            {
              sBefore += sNextBlock;
            }
        }
    }
  while (!sAfter.empty ());
  return sBefore;
}

char *
a5_correct_command (const char *cmd)
{
  std::string in = cmd ? cmd : "";
  std::string out = cc_process_block (in);
  char *r = (char *) malloc (out.size () + 1);
  if (r != NULL)
    memcpy (r, out.c_str (), out.size () + 1);
  return r;
}

/* ---------------------------------------------------------------- matching */

static std::string
trim (const std::string &s)
{
  size_t a = s.find_first_not_of (" \t");
  size_t b = s.find_last_not_of (" \t");
  if (a == std::string::npos)
    return "";
  return s.substr (a, b - a + 1);
}

/* ------------------------------------------------- first-word prefilter */

/* ASCII-only lowercase: deterministic regardless of the process locale (C
   tolower is locale-sensitive; std::regex::icase folds through the classic
   locale's ctype, which is ASCII-only -- match that exactly). */
static char
ascii_lower (unsigned char c)
{
  return (char) (c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
}

/* Index of the ')' closing the '(' at s[open], honouring escapes and
   character classes; npos when unbalanced. */
static size_t
re_group_end (const std::string &s, size_t open)
{
  int depth = 0;
  bool cls = false;
  for (size_t i = open; i < s.size (); i++)
    {
      char c = s[i];
      if (c == '\\') { i++; continue; }
      if (cls) { if (c == ']') cls = false; continue; }
      if (c == '[') { cls = true; continue; }
      if (c == '(')
        depth++;
      else if (c == ')')
        {
          depth--;
          if (depth == 0)
            return i;
        }
    }
  return std::string::npos;
}

/* Does the regex contain a '|' at paren depth 0?  ADRIFT's global '/'->'|'
   leaves bare word alternations UNSCOPED ("^x on|against y$"), where only the
   first branch is '^'-anchored -- a first-word test is unsound there. */
static bool
re_has_toplevel_alt (const std::string &s)
{
  int depth = 0;
  bool cls = false;
  for (size_t i = 0; i < s.size (); i++)
    {
      char c = s[i];
      if (c == '\\') { i++; continue; }
      if (cls) { if (c == ']') cls = false; continue; }
      if (c == '[') { cls = true; continue; }
      if (c == '(')
        depth++;
      else if (c == ')')
        depth--;
      else if (c == '|' && depth == 0)
        return true;
    }
  return false;
}

/* The literal chars a match MUST begin with at s[i]: scan until a regex
   metacharacter, a non-ASCII byte (std::regex::icase folds bytes, not UTF-8;
   stopping keeps the test byte-exact), or a char made optional by a following
   quantifier.  Stopping early only WEAKENS the filter, never breaks it. */
static std::string
re_leading_literal (const std::string &s, size_t i)
{
  static const char meta[] = "\\^$.|?*+()[]{}";
  std::string out;
  for (; i < s.size (); i++)
    {
      unsigned char c = (unsigned char) s[i];
      if (c >= 0x80 || strchr (meta, (char) c) != NULL)
        break;
      if (i + 1 < s.size () && strchr ("?*+{", s[i + 1]) != NULL)
        break;
      out += ascii_lower (c);
    }
  return out;
}

/* Accumulate the possible first-literal set at s[i].  A leading non-capturing
   group of alternatives contributes each alternative's leading literal; when
   the group is optional ("(?:the )?north") the continuation's literals are
   possible too.  Returns false (filter unavailable) whenever any path could
   start with something non-literal -- references, wildcards, quantified heads. */
static bool
collect_prefixes (const std::string &s, size_t i,
                  std::vector<std::string> &out, int depth)
{
  if (depth > 3 || out.size () > 8)
    return false;
  if (i >= s.size ())
    return false;
  if (s.compare (i, 3, "(?:") == 0)
    {
      size_t close = re_group_end (s, i);
      if (close == std::string::npos)
        return false;
      char q = close + 1 < s.size () ? s[close + 1] : '\0';
      if (q == '*' || q == '+' || q == '{')
        return false;
      /* Split the group body on its top-level '|'s. */
      std::string body = s.substr (i + 3, close - (i + 3));
      size_t start = 0;
      int d = 0;
      bool cls = false;
      for (size_t k = 0; k <= body.size (); k++)
        {
          char c = k < body.size () ? body[k] : '|';
          if (k < body.size ())
            {
              if (c == '\\') { k++; continue; }
              if (cls) { if (c == ']') cls = false; continue; }
              if (c == '[') { cls = true; continue; }
              if (c == '(') { d++; continue; }
              if (c == ')') { d--; continue; }
              if (c != '|' || d != 0)
                continue;
            }
          std::string lit = re_leading_literal (body.substr (start, k - start), 0);
          if (lit.empty () || out.size () >= 8)
            return false;
          out.push_back (lit);
          start = k + 1;
        }
      if (q == '?')
        return collect_prefixes (s, close + 2, out, depth + 1);
      return true;
    }
  std::string lit = re_leading_literal (s, i);
  if (lit.empty ())
    return false;
  out.push_back (lit);
  return true;
}

static void
derive_prefilter (const std::string &re_str, std::vector<std::string> &out)
{
  out.clear ();
  if (re_str.size () < 2 || re_str[0] != '^')
    return;
  if (re_has_toplevel_alt (re_str))
    return;
  std::vector<std::string> tmp;
  if (!collect_prefixes (re_str, 1, tmp, 0) || tmp.empty ())
    out.clear ();
  else
    out = tmp;
}

/* Does `input` start (ASCII case-insensitively) with one of cp's literals?
   An empty literal list means the filter is unavailable -> pass. */
static bool
prefilter_pass (const a5_compiled_pattern &cp, const char *input)
{
  if (cp.prefixes.empty ())
    return true;
  for (const std::string &p : cp.prefixes)
    {
      size_t k = 0;
      for (; k < p.size (); k++)
        {
          char c = input[k];
          if (c == '\0' || ascii_lower ((unsigned char) c) != p[k])
            break;
        }
      if (k == p.size ())
        return true;
    }
  return false;
}

/* Compile one already-variant-expanded pattern string. */
static void
compile_pattern (const std::string &pattern, a5_compiled_pattern &cp)
{
  std::string re_str;
  std::vector<std::string> refs;
  if (convert_to_re (pattern.c_str (), re_str, refs))
    {
      try
        {
          cp.re = std::regex (re_str,
                              std::regex::ECMAScript | std::regex::icase);
          cp.refs = std::move (refs);
          cp.ok = true;
          derive_prefilter (re_str, cp.prefixes);
        }
      catch (const std::regex_error &)
        {
          cp.ok = false;
        }
    }
}

static std::string variant_pattern (const char *pattern, int variant,
                                    int *total);

/* The cached, fully-compiled wildcard-variant list for a command pattern. */
static const a5_pattern_variants &
pattern_variants (const char *pattern)
{
  std::string key = pattern ? pattern : "";
  auto it = cached_variants.find (key);
  if (it != cached_variants.end ())
    return it->second;
  a5_pattern_variants pv;
  int total = 0;
  variant_pattern (key.c_str (), -1, &total);   /* count only */
  pv.total = total;
  pv.v.resize ((size_t) total);
  for (int vi = 0; vi < total; vi++)
    {
      int t = 0;
      compile_pattern (variant_pattern (key.c_str (), vi, &t), pv.v[vi]);
    }
  return cached_variants.emplace (std::move (key), std::move (pv)).first->second;
}

/* Match `input` against one pre-compiled pattern variant. */
static int
match_one_pattern (const a5_compiled_pattern &cp, const char *input,
                   a5_match_t *m)
{
  if (m != NULL)
    m->n_refs = 0;
  if (!cp.ok)
    return 0;
  if (input != NULL && !prefilter_pass (cp, input))
    return 0;

  std::smatch mt;
  std::string in = input ? input : "";
  /* The Adrift 5 runner (and real ADRIFT's clsUserSession) test the command regex with
     .NET Regex.IsMatch, which is a SEARCH, not a full-string match.  For the
     usual well-formed "^...$" pattern the two are identical, but ADRIFT's
     ConvertToRE does a *global* '/'->'|' that leaves a bare (un-bracketed) word
     alternation UNSCOPED: e.g. "[fork] on/against [box]" becomes
     "^...(fork) on|against (box)$", where '|' has lowest precedence so '^' binds
     only to the left branch and '$' only to the right.  Under IsMatch/search the
     left branch "^...(fork) on" still matches the input's prefix, so the runner (and
     ADRIFT) accept "put fork on box"; a full-string std::regex_match would reject
     both branches.  Mirror the runner's search semantics so those bare-'/' connectors
     match identically (SoC "put fork on box" -> the game's own Task, not the
     generic library put). */
  if (!std::regex_search (in, mt, cp.re))
    return 0;
  if (m != NULL)
    {
      int n = 0;
      for (size_t g = 1; g < mt.size () && n < A5_MAX_REFS; g++)
        {
          /* capture group g corresponds to refs[g-1] (refs are the only
             capturing groups, in order). */
          if (g - 1 >= cp.refs.size ())
            break;
          std::string val = trim (mt[g].str ());
          snprintf (m->ref_name[n], A5_REF_NAMELEN, "%s", cp.refs[g - 1].c_str ());
          snprintf (m->ref_text[n], A5_REF_TEXTLEN, "%s", val.c_str ());
          n++;
        }
      m->n_refs = n;
    }
  return 1;
}

/*
 * Wildcard variants (clsUserSession.GetRegularExpression): a command containing
 * '*' produces several candidate regexes, tried in order -- first with ALL
 * wildcards removed, then progressively restoring them left-to-right, the
 * original command (every wildcard intact) last.  The runner builds the list as
 * iAsterix = N-1 .. -1, each iteration stripping the first iAsterix+1
 * asterisks, "otherwise we may always end up matching the object name in *":
 * `find %object% *` must first try %object% = the WHOLE tail ("bell tower
 * entrance" -> the Bell Tower door) and only fall back to the lazy
 * %object%/wildcard split ("bell" + "tower entrance") when the longer text
 * names no object.  The caller advances to the next variant on that name-match
 * failure (InputMatchesObject -> DoesntMatch -> next regex).
 */
static std::string
variant_pattern (const char *pattern, int variant, int *total)
{
  std::string pat = pattern ? pattern : "";
  replace_all (pat, "**", "*");
  int n = 0;
  for (char c : pat)
    if (c == '*')
      n++;
  *total = (n > 0) ? n + 1 : 1;
  if (variant < 0 || variant >= *total)
    return std::string ();
  int remove = n - variant;             /* variant 0 removes all N wildcards */
  std::string out;
  out.reserve (pat.size ());
  for (char c : pat)
    {
      if (c == '*' && remove > 0)
        { remove--; continue; }
      out += c;
    }
  return out;
}

int
a5parse_match_command_v (const char *pattern, const char *input, a5_match_t *m,
                         int variant)
{
  const a5_pattern_variants &pv = pattern_variants (pattern);
  if (variant < 0 || variant >= pv.total)
    return -1;
  return match_one_pattern (pv.v[(size_t) variant], input, m);
}

int
a5parse_match_command (const char *pattern, const char *input, a5_match_t *m)
{
  /* The single-pattern form matches the original command verbatim (all
     wildcards intact) -- the LAST variant. */
  const a5_pattern_variants &pv = pattern_variants (pattern);
  return match_one_pattern (pv.v[(size_t) (pv.total - 1)], input, m) == 1;
}

const char *
a5parse_ref (const a5_match_t *m, const char *name)
{
  if (m == NULL || name == NULL)
    return NULL;
  for (int i = 0; i < m->n_refs; i++)
    if (strcmp (m->ref_name[i], name) == 0)
      return m->ref_text[i];
  return NULL;
}
