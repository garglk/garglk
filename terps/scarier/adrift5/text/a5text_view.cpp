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
 * ADRIFT 5 support for Scarier -- text engine: the room view (split from
 * a5text.cpp).  clsLocation.ViewLocation: long description, listed objects,
 * characters present and exits.  See a5text.h and a5text_internal.h.
 */

#include <algorithm>

#include "a5text_internal.h"

/* ----------------------------------------------------------- LOOK / location */

/* Does object `oi` belong in the room view of `lockey`?  A dynamic object is
   listed unless it is ExplicitlyExclude, a static one only if it is
   ExplicitlyList -- and either way it must be directly at the location.
   The property tests go through the merged static + runtime table
   (clsObject.HasProperty; a runtime SetProperty override wins): Euripides'
   drone / boom box run `SetProperty <obj> ExplicitlyExclude <Selected>` to
   drop themselves from the "Also here is ..." auto-list once their presence
   is conveyed by prose. */
static int
object_listed_at (a5_state_t *st, int oi, const char *lockey)
{
  const a5_object_t *o = &st->adv->objects[oi];
  int listed = st->obj[oi].is_static
      ? a5state_entity_has_prop (st, o->key, "ExplicitlyList")
      : !a5state_entity_has_prop (st, o->key, "ExplicitlyExclude");

  return listed && a5state_object_at_location (st, oi, lockey, 1);
}

/* Get an object's "list description" text (static/dynamic), or NULL. */
static char *
object_list_desc (a5_state_t *st, const a5_object_t *o, int is_static)
{
  const char *key = is_static ? "ListDescription" : "ListDescriptionDynamic";
  const a5_prop_t *prop = a5_prop_find (o->props, o->n_props, key);
  if (prop == nullptr)
    return nullptr;
  if (prop->value_node != nullptr)
    return a5text_eval_description (st, prop->value_node);
  if (prop->value != nullptr && prop->value[0] != '\0')
    return strdup (prop->value);
  return nullptr;
}

/* A present character's "is here" line: its CharHereDesc property rendered (with
   the character as the text context), else the default "<Name> is here."
   (clsLocation.ViewLocation: IsHereDesc / fallback).

   The render follows the AMBIENT marking_display like every other description
   in the view -- clsDescription.ToString sets Displayed only `If Not
   UserSession.bTestingOutput`, and IsHereDesc is not special-cased.  Forcing
   the mark here made the stock Look's two test renders disagree whenever a
   present character had a DisplayOnce CharHereDesc: the pre-action probe
   retired the first-visit segment, so the post-action probe fell through to the
   next one.  The view still came out right (the response pins to the first
   probe), but the discarded second render expanded that segment's %functions%
   under bDisplaying and so marked the character Introduced a turn early --
   Anno 1700's Susan answered "say hello" as "(to the young woman)" instead of
   "(to a young woman)". */
static char *
char_here_desc (a5_state_t *st, const a5_character_t *c)
{
  const a5_prop_t *p = a5_prop_find (c->props, c->n_props, "CharHereDesc");
  const char *prev_ctx = st->ctx_char;
  char *result;
  st->ctx_char = c->key;
  if (p != nullptr)
    {
      /* The property is present (clsItem.HasProperty == ContainsKey), so it
         overrides the default -- even when its value is blank, in which case
         IsHereDesc returns "" and the character is suppressed from the present
         list (the Customs Official in Revenge: its room prose already says "A
         customs official stands here."). */
      if (p->value_node != nullptr)
        {
          char *raw = a5text_eval_description (st, p->value_node);
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

/* Room name (bold), then the long description.  The runner embeds the raw
   ShortDescription (still bearing its %functions%) inside "<b>...</b>" on the
   marked-up view (clsLocation.vb:188) and only its ONE Display-time
   ReplaceFunctions + ReplaceALRs + CapAfterFullStop runs over the whole sView;
   the '\n' before the name is followed by the '<' of the tag, so the
   line-start cap rule never fires on the name.  So process the name WITHOUT
   the per-piece cap (a5text_process_nocap keeps markup + resolves
   functions/OO/ALR), wrap it in the bold tags, and let the room-view
   replace_alrs + auto_capitalise pass in view_loc_finish() adjudicate it as the runner does.
   A genuinely lowercase generated name -- LostCoastlines' "cabal plain",
   "flock sea" -- is thus left lowercase like the runner, while an already-capitalised
   name (its case baked into the source) is unaffected. */
static void
view_loc_name (a5_state_t *st, const char *lockey, sb_t &sb)
{
  char *raw = a5text_location_short (st, lockey);
  char *name = a5text_process_nocap (st, raw);
  sb_puts (&sb, "<b>");
  sb_puts (&sb, name);
  sb_puts (&sb, "</b>\n");
  free (raw);
  free (name);
}

/* The long description with the special-listed objects appended.  Returns
   whether that body came out empty. */
static int
view_loc_body (a5_state_t *st, const char *lockey, sb_t &sb)
{
  int i, body_empty;

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
        char *ld;
        if (!object_listed_at (st, i, lockey))
          continue;
        ld = object_list_desc (st, o, is_static);          /* scan render */
        if (ld != nullptr && ld[0] != '\0')
          specials.push_back (i);
        free (ld);
      }
    for (int si : specials)
      {
        const a5_object_t *o = &st->adv->objects[si];
        char *ld = object_list_desc (st, o, st->obj[si].is_static);
        if (ld != nullptr && ld[0] != '\0')
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
  return body_empty;
}

/* General listed objects ("Also here is ..." / v5-empty-room "There is ...
   here."). */
static void
view_loc_objects (a5_state_t *st, const char *lockey, sb_t &sb, int body_empty)
{
  int i;
  std::vector<std::string> names;
  for (i = 0; i < st->adv->n_objects; i++)
    {
      const a5_object_t *o = &st->adv->objects[i];
      int is_static = st->obj[i].is_static;
      char *ld, *nm;
      if (!object_listed_at (st, i, lockey))
        continue;
      ld = object_list_desc (st, o, is_static);
      if (ld != nullptr && ld[0] != '\0') { free (ld); continue; } /* special-listed */
      free (ld);
      nm = a5text_object_name (st, o, A5_ART_INDEFINITE);
      names.push_back (nm);
      free (nm);
    }
  if (!names.empty ())
    {
      /* v5 empty-body room: "There is <list> here." with no leading pSpace
         (clsLocation.vb:138).  Otherwise the trailing "Also here is <list>.",
         prefixed by pSpace(sView) -- two spaces unless the buffer is empty or
         already ends in a newline (clsLocation.vb:134). */
      int as_there_is = body_empty
        && (st->adv->version == nullptr || atof (st->adv->version) >= 5.0);
      if (!as_there_is && sb.len > 0 && sb.p[sb.len - 1] != '\n')
        sb_puts (&sb, "  ");
      sb_puts (&sb, as_there_is ? "There is " : "Also here is ");
      sb_puts (&sb, join_and (names).c_str ());
      sb_puts (&sb, as_there_is ? " here." : ".");
    }
}

/* Event SetLook text (clsLocation.ViewLocation: "For Each e ... e.LookText()"):
   walk the model's events in order; each RUNNING event contributes its own
   most-recent look entry matching the player's location (clsEvent.LookText
   answers only while Status = Running, so a finished or paused event's
   SetLook text disappears from the room view). */
static void
view_loc_event_looks (a5_state_t *st, sb_t &sb)
{
  for (int ei = 0; ei < st->adv->n_events; ei++)
    {
      if (st->ev_running == nullptr
          || !st->ev_running (st->ev_running_ctx, ei))
        continue;
      const char *lt = a5state_event_look (st, st->adv->events[ei].key);
      if (lt != nullptr && lt[0] != '\0')
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
}

/* Present NPCs (clsLocation.ViewLocation character loop): each visible
   character contributes its CharHereDesc (or "<Name> is here."); identical
   descriptions are grouped, the name folded out via ##CHARNAME##, the verb
   pluralised (" is "->" are ") for a shared description, and the names listed. */
static void
view_loc_characters (a5_state_t *st, const char *lockey, sb_t &sb)
{
  int i;
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
      d = ci_replace_all (d, "##CHARNAME##", join_and (nm));
      if (sb.len > 0 && sb.p[sb.len - 1] != '\n')   /* pSpace, vb:166 */
        sb_puts (&sb, "  ");
      sb_puts (&sb, d.c_str ());
    }
}

/* Exit listing (clsLocation.ViewLocation: "If Adventure.ShowExits Then ...").
   ListExits enumerates the player's routes (restriction-unchecked, matching
   HasRouteInDirection(d, False)); >1 exits => "Exits are <list>.", exactly one
   => "An exit leads <dir>.", none => nothing. */
static void
view_loc_exits (a5_state_t *st, const char *lockey, sb_t &sb)
{
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
}

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
static char *
view_loc_finish (a5_state_t *st, sb_t &sb)
{
  char *plain;

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
  /* A long description ending in a raw <br> leaves the runner's buffer
     non-vbLf, so the next message (a LocationTrigger task's completion
     text, say) still space-joins -- AFP2's "You hop the railing...<br><br>". */
  plain = ps_mark_trailing (capped, plain);
  free (capped);
  return plain;
}

/* Render a specific location's ViewLocation (clsLocation.ViewLocation).  lockey
   NULL => the Player's current location (the common case; also the `%Player%`
   look path).  Used directly for `%DisplayLocation[loc]%`. */
char *
view_location_impl (a5_state_t *st, const char *lockey)
{
  const a5_location_t *loc;
  sb_t sb;
  char *plain;
  int body_empty;
  size_t body_start;

  if (lockey == nullptr)
    lockey = a5state_player_location (st);
  sb_init (&sb);
  if (lockey == nullptr)
    return strdup ("");
  loc = a5model_location (st->adv, lockey);
  if (loc == nullptr)
    return strdup ("");

  /* A v5 location view is prefixed with a blank line before the room name
     (clsLocation.ViewLocation: "If Adventure.dVersion >= 5 Then sView = vbCrLf
     & sView"), so after a "> look" echo the room name has a paragraph break. */
  sb_putc (&sb, '\n');

  view_loc_name (st, lockey, sb);
  body_start = sb.len;
  body_empty = view_loc_body (st, lockey, sb);
  view_loc_objects (st, lockey, sb, body_empty);
  view_loc_event_looks (st, sb);
  view_loc_characters (st, lockey, sb);
  view_loc_exits (st, lockey, sb);

  /* Empty-view fallback (clsLocation.ViewLocation vb:185: `If sView = "" Then
     sView = "Nothing special."`).  The runner's sView at this point is only the BODY
     (long description + listed objects + event look text + NPC lines + exits);
     the bold room name is prepended after the check.  Scarier builds the name
     into the same buffer up-front, so compare against the post-header mark
     (canyoustandup's "Beginning" start room: no description at all). */
  if (sb.len == body_start)
    sb_puts (&sb, "Nothing special.");

  plain = view_loc_finish (st, sb);
  free (sb.p);
  return plain;
}

char *
a5text_view_location (a5_state_t *st)
{
  return view_location_impl (st, nullptr);
}
