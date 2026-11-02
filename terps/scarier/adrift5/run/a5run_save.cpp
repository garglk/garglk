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
 * ADRIFT 5 support for Scarier -- save / restore, undo and pending state.
 *
 * clsState / FileIO.SaveState|LoadState: the on-disk <Game> document an ADRIFT
 * 5 Runner can load, carrying Scarier's own lossless <ScarierExt> snapshot;
 * the reader for both, and for a foreign Runner save without the extension;
 * the per-turn undo stack built from lean snapshots; and the cross-turn
 * parser continuation (pending disambiguation, remembered verb) the
 * Spatterlight autosave serialises through sessrec.
 *
 * Split out of a5run.cpp.  Everything here reaches the turn loop only through
 * the public a5run.h entry points.
 */

#include "a5run_internal.h"

#include "../../sessrec.h"

/* ============================================================ save / restore */

/* Save/restore (clsState / FileIO.SaveState|LoadState).
 *
 * The on-disk save is ADRIFT 5 Runner-compatible: a `<Game>` document keyed by
 * entity <Key> (FileIO.SaveState / LoadState schema; see ADRIFT4_vs_ADRIFT5.md
 * section 8).  An ADRIFT 5 Runner save restores into
 * Scarier, and a Scarier save loads in the Adrift 5 runner.
 *
 * To keep Scarier<->Scarier round-trips (undo snapshots, deterministic
 * replay) lossless -- the Adrift 5 runner's schema omits the RNG state and the full
 * event/walk internals -- every Scarier save also carries a <ScarierExt> child
 * of <Game> holding the complete native snapshot (the old <SaveState> body).
 * The Adrift 5 runner's reader queries only the tags it knows via SelectNodes("/Game/
 * <tag>") and silently ignores <ScarierExt>, so the extension is invisible to
 * it.  On restore we prefer <ScarierExt> when present (lossless); a foreign save
 * without it is read from the Adrift 5 runner fields (RNG then re-seeds to 1234).
 *
 * A pre-existing raw (uncompressed) `<SaveState>` file -- the format Scarier
 * wrote before this change -- still restores through the legacy body path. */

static void
xml_esc (sb_t *b, const char *s)
{
  const char *p, *plain;
  if (s == nullptr)
    return;
  /* Copy unescaped runs in bulk; the per-turn undo snapshot calls this for
     every entity key, so byte-at-a-time sb_putc was a measured hotspot. */
  for (p = plain = s; *p; p++)
    {
      const char *rep;
      size_t rn;
      switch (*p)
        {
        case '&': rep = "&amp;";  rn = 5; break;
        case '<': rep = "&lt;";   rn = 4; break;
        case '>': rep = "&gt;";   rn = 4; break;
        case '"': rep = "&quot;"; rn = 6; break;
        default:  continue;
        }
      if (p > plain)
        sb_putn (b, plain, (size_t) (p - plain));
      sb_putn (b, rep, rn);
      plain = p + 1;
    }
  if (p > plain)
    sb_putn (b, plain, (size_t) (p - plain));
}

static void
sb_elem (sb_t *b, const char *tag, const char *val)
{
  /* Fast path: assemble "<tag>escaped-val</tag>\n" in one stack buffer and
     append it with a single sb_putn.  Long values (or long tags) fall back to
     piecewise appends; both spell the same bytes. */
  size_t tl = strlen (tag);
  if (val != nullptr && tl + 10 <= 96)
    {
      char buf[192];
      size_t n = 0, guard = sizeof buf - (tl + 4);  /* keep room for "</tag>\n" */
      const char *p;
      buf[n++] = '<';
      memcpy (buf + n, tag, tl); n += tl;
      buf[n++] = '>';
      for (p = val; *p; p++)
        {
          if (n + 6 > guard)
            break;
          switch (*p)
            {
            case '&': memcpy (buf + n, "&amp;", 5);  n += 5; break;
            case '<': memcpy (buf + n, "&lt;", 4);   n += 4; break;
            case '>': memcpy (buf + n, "&gt;", 4);   n += 4; break;
            case '"': memcpy (buf + n, "&quot;", 6); n += 6; break;
            default:  buf[n++] = *p; break;
            }
        }
      if (*p == '\0')
        {
          buf[n++] = '<'; buf[n++] = '/';
          memcpy (buf + n, tag, tl); n += tl;
          buf[n++] = '>'; buf[n++] = '\n';
          sb_putn (b, buf, n);
          return;
        }
    }
  sb_putn (b, "<", 1); sb_putn (b, tag, tl); sb_putn (b, ">", 1);
  xml_esc (b, val);
  sb_putn (b, "</", 2); sb_putn (b, tag, tl); sb_putn (b, ">\n", 2);
}

/* Render `v` in decimal into the tail of caller-provided buf[cap]; returns a
   pointer to the first digit (snprintf takes macOS locale locks -- measured
   directly in the per-turn snapshot profile). */
static char *
render_long (char *buf, size_t cap, long v)
{
  char *q = buf + cap;
  unsigned long u = v < 0 ? (unsigned long) -(v + 1) + 1 : (unsigned long) v;
  *--q = '\0';
  do
    {
      *--q = (char) ('0' + (u % 10));
      u /= 10;
    }
  while (u != 0);
  if (v < 0)
    *--q = '-';
  return q;
}

static void
sb_elem_l (sb_t *b, const char *tag, long v)
{
  char num[32];
  sb_elem (b, tag, render_long (num, sizeof num, v));
}

/* Write a <tag> naming the key of each entity whose flag is set: a "seen"
   array, sparse by key.  An array that was never allocated writes nothing. */
template <typename Entity>
static void
sb_flagged_keys (sb_t *b, const char *tag, const char *flags,
                 const Entity *entities, int count)
{
  if (flags == nullptr)
    return;
  for (int i = 0; i < count; i++)
    if (flags[i])
      sb_elem (b, tag, entities[i].key);
}

/* Pre-order list of every node in the game DOM, so a <DisplayOnce> segment
   (tracked by node pointer) has a stable index across a save/restore that
   re-parses an identical game file. */
static void
collect_dom_nodes (const a5_xml_node_t *n,
                   std::vector<const a5_xml_node_t *> &out)
{
  for (; n != nullptr; n = n->next)
    {
      out.push_back (n);
      collect_dom_nodes (n->first_child, out);
    }
}

/* Build (once) the run's cached pre-order DOM node list + node->index map from
   adv->doc, and reuse it thereafter.  adv->doc is immutable after load, so the
   walk and map are computed on first use and shared by every subsequent
   save/restore, instead of re-walking the whole DOM on each per-turn snapshot. */
static void
ensure_dom_cache (a5_run_t *run)
{
  if (run->dom_nodes != nullptr)
    return;
  run->dom_nodes = new std::vector<const a5_xml_node_t *>;
  run->dom_index = new std::unordered_map<const void *, int>;
  collect_dom_nodes (a5xml_root (run->adv->doc), *run->dom_nodes);
  run->dom_index->reserve (run->dom_nodes->size () * 2);
  for (size_t k = 0; k < run->dom_nodes->size (); k++)
    (*run->dom_index)[(const void *) (*run->dom_nodes)[k]] = (int) k;
}

/* Resolve a holder/location key to the model's own stable string pointer (so it
   survives the save buffer being freed).  Keys are globally unique in ADRIFT. */
static const char *
intern_key (const a5_adventure_t *adv, const char *key)
{
  int i;
  const a5_object_t *o;
  const a5_location_t *l;
  const a5_character_t *c;
  if (key == nullptr || key[0] == '\0')
    return nullptr;
  if ((o = a5model_object (adv, key)) != nullptr)    return o->key;
  if ((l = a5model_location (adv, key)) != nullptr)  return l->key;
  if ((c = a5model_character (adv, key)) != nullptr) return c->key;
  for (i = 0; i < adv->n_groups; i++)
    if (streq (adv->groups[i].key, key))
      return adv->groups[i].key;
  return nullptr;
}

/* ---------------------------------------------------------- Scarier body ----
   The full-fidelity native snapshot: RNG, every entity's runtime fields, event
   and walk internals, the sparse seen/override/displayed/look sets.  Emitted
   verbatim inside <ScarierExt> (and historically as the bare <SaveState> root);
   read back by restore_scarier_body.  Entity arrays are positional (model
   order); the sparse sets are keyed.  This is what makes undo and Scarier<->
   Scarier restore byte-for-byte deterministic despite the Adrift 5 runner's lossier
   schema. */
namespace {

/* One writing of the snapshot: the buffer and the run it describes, with a
   member per section, in the order save_scarier_body calls them (which is the
   order of the elements in the file). */
struct scarier_save_t
{
  sb_t *b;
  a5_run_t *run;
  a5_state_t *st;
  const a5_adventure_t *adv;

  /* Write one section through its cache: `from` is the n bytes of state that
     decide what `emit` writes. */
  template <typename Emit>
  void
  cached (int section, const char *from, size_t n, Emit emit)
  {
    a5_run_t::section_cache_t &c = run->section_cache[section];
    if (c.valid && c.from.size () == n
        && (n == 0 || memcmp (c.from.data (), from, n) == 0))
      {
        sb_putn (b, c.bytes.data (), c.bytes.size ());
        return;
      }
    const size_t start = b->len;
    emit ();
    c.valid = b->len >= start && (b->p != nullptr || b->len == 0);
    if (c.valid)
      {
        c.from.assign (from, n);
        c.bytes.assign (b->p != nullptr ? b->p + start : "", b->len - start);
      }
  }

  /* A "seen"/"done" array as a cached run of <tag> elements. */
  template <typename Entity>
  void
  cached_flags (int section, const char *tag, const char *flags,
                const Entity *entities, int count)
  {
    sb_t *out = b;
    cached (section, flags, flags != nullptr ? (size_t) count : 0,
            [=] { sb_flagged_keys (out, tag, flags, entities, count); });
  }

  void globals ();
  void objects ();
  void characters ();
  void variables ();
  void marks ();
  void seen_chars ();
  void events ();
  void walks ();
  void displayed ();
  void looks ();
};

void
scarier_save_t::globals ()
{
  int i, native;
  unsigned int rng[4];

  sb_elem_l (b, "Version", 1);

  a5rand_get_state (&native, rng);
  sb_elem_l (b, "RngNative", native);
  for (i = 0; i < 4; i++)
    sb_elem_l (b, "Rng", (long) rng[i]);

  sb_elem_l (b, "EventsRunning", run->events_running);
  sb_elem_l (b, "GameOver", st->game_over);
  sb_elem_l (b, "Turns", st->turns);
  sb_elem_l (b, "EndDisplayed", st->end_displayed);
  if (st->end_message != nullptr)
    sb_elem (b, "EndMessage", st->end_message);
  if (st->conv_char != nullptr && st->conv_char[0])
    sb_elem (b, "ConvChar", st->conv_char);
  if (st->conv_node != nullptr && st->conv_node[0])
    sb_elem (b, "ConvNode", st->conv_node);
  if (st->s_it != nullptr && st->s_it[0])
    sb_elem (b, "ItRef", st->s_it);
  if (st->s_them != nullptr && st->s_them[0])
    sb_elem (b, "ThemRef", st->s_them);
  if (st->s_him != nullptr && st->s_him[0])
    sb_elem (b, "HimRef", st->s_him);
  if (st->s_her != nullptr && st->s_her[0])
    sb_elem (b, "HerRef", st->s_her);
}

void
scarier_save_t::objects ()
{
  int i;
  std::string &from = run->section_from;

  /* What the section is written from: each object's where/static pair and the
     TEXT of its key -- a key can alias a property override, so its address
     does not vouch for its contents. */
  from.clear ();
  for (i = 0; i < adv->n_objects; i++)
    {
      const int pair[2] = { (int) st->obj[i].where, st->obj[i].is_static };
      from.append ((const char *) pair, sizeof pair);
      if (st->obj[i].key != nullptr)
        from.append (st->obj[i].key);
      from.push_back ('\0');
    }

  /* Objects (model order). */
  cached (a5_run_t::SEC_OBJECTS, from.data (), from.size (), [this] {
    for (int o = 0; o < adv->n_objects; o++)
      {
        sb_puts (b, "<Object>\n");
        sb_elem_l (b, "Where", (long) st->obj[o].where);
        sb_elem_l (b, "Static", st->obj[o].is_static);
        if (st->obj[o].key != nullptr && st->obj[o].key[0])
          sb_elem (b, "Key", st->obj[o].key);
        sb_puts (b, "</Object>\n");
      }
  });
}

void
scarier_save_t::characters ()
{
  int i;

  /* Characters (model order). */
  for (i = 0; i < adv->n_characters; i++)
    {
      sb_puts (b, "<Character>\n");
      if (st->char_loc[i] != nullptr && st->char_loc[i][0])
        sb_elem (b, "Loc", st->char_loc[i]);
      if (st->char_position[i] != nullptr)
        sb_elem (b, "Position", st->char_position[i]);
      if (st->char_onobj[i] != nullptr && st->char_onobj[i][0])
        sb_elem (b, "OnObj", st->char_onobj[i]);
      /* Carrier CHARACTER key: a follower riding another character (set at
         runtime by MoveCharacter ToOntoCharacter -- e.g. Symphonica's Rory /
         Konkey Dong following the player).  char_loc is NULL in that case and
         the room resolves through the carrier, so without persisting this the
         follower would vanish from every room description after restore. */
      if (st->char_onchar != nullptr && st->char_onchar[i] != nullptr
          && st->char_onchar[i][0])
        sb_elem (b, "OnChar", st->char_onchar[i]);
      sb_elem_l (b, "In", st->char_in ? st->char_in[i] : 0);
      sb_puts (b, "</Character>\n");
    }

  /* Player character key (clsAdventure.Player.Key; a ToSwitchWith/BECOME
     retargets it away from the literal "Player"). */
  if (st->player_key != nullptr && st->player_key[0])
    sb_elem (b, "PlayerKey", st->player_key);
}

void
scarier_save_t::variables ()
{
  int i;

  /* Variables (model order). */
  for (i = 0; i < adv->n_variables; i++)
    {
      sb_puts (b, "<Variable>\n");
      sb_elem_l (b, "Num", st->var_num[i]);
      if (st->var_text[i] != nullptr)
        sb_elem (b, "Text", st->var_text[i]);
      if (st->var_arr != nullptr && st->var_arr[i] != nullptr)
        {
          /* Numeric array elements, comma-joined (ScarierExt-only). */
          sb_t ab; sb_init (&ab);
          int e;
          for (e = 0; e < adv->variables[i].array_length; e++)
            {
              char nb[32];
              const char *num = render_long (nb, sizeof nb, st->var_arr[i][e]);
              if (e > 0)
                sb_putn (&ab, ",", 1);
              sb_putn (&ab, num, (size_t) (nb + sizeof nb - 1 - num));
            }
          char *joined = sb_finish (&ab);
          sb_elem (b, "Arr", joined);
          free (joined);
        }
      sb_puts (b, "</Variable>\n");
    }
}

void
scarier_save_t::marks ()
{
  int i;

  /* Completed tasks (sparse, by key). */
  cached_flags (a5_run_t::SEC_TASK_DONE, "TaskDone", st->task_done,
                adv->tasks, adv->n_tasks);

  /* Tasks that have already scored (clsTask.Scored), sparse by key.  This lives
     in <ScarierExt> because our own restore walks only ScarierExt: the full
     <Task><Scored> in the runner body is read solely for foreign saves
     (restore_fd_game).  Without it a repeatable scoring task re-awards its
     points once after restore (inflating the score). */
  cached_flags (a5_run_t::SEC_TASK_SCORED, "TaskScored", st->task_scored,
                adv->tasks, adv->n_tasks);

  /* Property overrides. */
  for (i = 0; i < st->n_ov; i++)
    {
      sb_puts (b, "<PropOv>\n");
      sb_elem (b, "Entity", st->ov[i].entity);
      sb_elem (b, "Prop", st->ov[i].prop);
      sb_elem (b, "Value", st->ov[i].value);
      sb_puts (b, "</PropOv>\n");
    }

  /* "Seen" sets (sparse, by key). */
  cached_flags (a5_run_t::SEC_OBJ_SEEN, "ObjSeen", st->obj_seen,
                adv->objects, adv->n_objects);
  cached_flags (a5_run_t::SEC_CHAR_SEEN, "CharSeen", st->char_seen,
                adv->characters, adv->n_characters);
  cached_flags (a5_run_t::SEC_CHAR_INTRODUCED, "CharIntroduced",
                st->char_introduced, adv->characters, adv->n_characters);
  cached_flags (a5_run_t::SEC_LOC_SEEN, "LocSeen", st->loc_seen,
                adv->locations, adv->n_locations);
}

void
scarier_save_t::seen_chars ()
{
  int i;

  /* Per-character seen sets for any OTHER viewpoint a BECOME game has switched
     away from (the active player's set is the top-level ObjSeen/CharSeen/LocSeen
     above).  Mirrors the runner's per-character clsCharacterState.lSeenKeys so a saved
     BugHunt restores each marine's own sightings.  Empty for a lone-"Player"
     game. */
  for (i = 0; i < adv->n_characters; i++)
    {
      char *os = st->seen_stash_obj  ? st->seen_stash_obj[i]  : nullptr;
      char *cs = st->seen_stash_char ? st->seen_stash_char[i] : nullptr;
      char *ls = st->seen_stash_loc  ? st->seen_stash_loc[i]  : nullptr;
      if (i == st->seen_active_ci || (os == nullptr && cs == nullptr && ls == nullptr))
        continue;
      sb_puts (b, "<SeenChar>\n");
      sb_elem (b, "Key", adv->characters[i].key);
      sb_flagged_keys (b, "ObjSeen", os, adv->objects, adv->n_objects);
      sb_flagged_keys (b, "CharSeen", cs, adv->characters, adv->n_characters);
      sb_flagged_keys (b, "LocSeen", ls, adv->locations, adv->n_locations);
      sb_puts (b, "</SeenChar>\n");
    }
}

void
scarier_save_t::events ()
{
  int i;

  /* Events (model order). */
  for (i = 0; i < (int) run->events->size (); i++)
    {
      a5_event_rt &e = (*run->events)[i];
      size_t s;
      sb_puts (b, "<Event>\n");
      sb_elem_l (b, "Status", e.status);
      sb_elem_l (b, "Length", e.length_value);
      sb_elem_l (b, "Timer", e.timer_to_end);
      sb_elem_l (b, "LastSeTime", e.last_se_time);
      sb_elem_l (b, "LastSeIndex", e.last_se_index);
      sb_elem_l (b, "JustStarted", e.just_started);
      sb_elem_l (b, "NextCommand", e.next_command);
      sb_elem_l (b, "WhenStart", e.when_start);
      sb_elem (b, "Trigger", e.triggering_task.c_str ());
      for (s = 0; s < e.se_ft.size (); s++)
        sb_elem_l (b, "SeFt", e.se_ft[s]);
      sb_puts (b, "</Event>\n");
    }
}

void
scarier_save_t::walks ()
{
  int i;

  /* Walks (flattened order, as built by a5run_new). */
  for (i = 0; i < (int) run->walks->size (); i++)
    {
      a5_walk_rt &w = (*run->walks)[i];
      size_t s;
      sb_puts (b, "<Walk>\n");
      sb_elem_l (b, "Status", w.status);
      sb_elem_l (b, "Length", w.length);
      sb_elem_l (b, "Timer", w.timer_to_end);
      sb_elem_l (b, "LastSwTime", w.last_sw_time);
      sb_elem_l (b, "LastSwIndex", w.last_sw_index);
      sb_elem_l (b, "JustStarted", w.just_started);
      sb_elem_l (b, "NextCommand", w.next_command);
      sb_elem (b, "Trigger", w.triggering_task.c_str ());
      for (s = 0; s < w.step_dur.size (); s++)
        sb_elem_l (b, "StepDur", w.step_dur[s]);
      for (s = 0; s < w.sw_ft.size (); s++)
        sb_elem_l (b, "SwFt", w.sw_ft[s]);
      for (s = 0; s < w.came_across.size (); s++)
        sb_elem_l (b, "CameAcross", w.came_across[s]);
      sb_puts (b, "</Walk>\n");
    }
}

void
scarier_save_t::displayed ()
{
  /* Displayed <DisplayOnce> segments (by DOM pre-order index), found through
     the cached node->index map, one hash lookup per displayed segment. */
  if (st->n_disp_once > 0)
    {
      int j;
      ensure_dom_cache (run);
      for (j = 0; j < st->n_disp_once; j++)
        {
          std::unordered_map<const void *, int>::const_iterator it
            = run->dom_index->find ((const void *) st->disp_once[j]);
          if (it != run->dom_index->end ())
            sb_elem_l (b, "Displayed", (long) it->second);
        }
    }
}

void
scarier_save_t::looks ()
{
  int i;

  /* SetLook stack. */
  for (i = 0; i < st->n_looks; i++)
    {
      sb_puts (b, "<Look>\n");
      sb_elem (b, "LocKey", st->looks[i].loc_key);
      sb_elem (b, "Text", st->looks[i].text);
      sb_elem (b, "Event", st->looks[i].event_key);
      sb_puts (b, "</Look>\n");
    }
}

} /* namespace */

static void
save_scarier_body (sb_t *b, a5_run_t *run)
{
  scarier_save_t save = { b, run, run->st, run->adv };

  save.globals ();
  save.objects ();
  save.characters ();
  save.variables ();
  save.marks ();
  save.seen_chars ();
  save.events ();
  save.walks ();
  save.displayed ();
  save.looks ();
}

/* ----------------------------------------------------- the Adrift 5 runner <Game> ---
   The interop schema.  Maps a5_state_t onto the Adrift 5 runner's clsGameState fields,
   keyed by entity <Key>, with the where-fields written as their .ToString enum
   names.  The writers below are the mapping; the design doc that derived it,
   TODO_a5_frankendrift_save_compat.md, was pruned in ab6bc4af.  For its
   field-by-field derivation from the runner's source, run
   `git show ab6bc4af^:terps/scarier/TODO_a5_frankendrift_save_compat.md`. */

/* Split a5_objloc_t onto the runner's two where-enums + LocationKey (SaveState @83).
   *dyn / *stat are NULL when at their default (Hidden / NoRooms, so the runner omits
   them); *lkey is "" when no key applies. */
static void
fd_object_location (const a5_objloc_t *l, const char **dyn, const char **stat,
                    const char **lkey)
{
  *dyn = nullptr;
  *stat = nullptr;
  *lkey = "";
  switch (l->where)
    {
    case A5_OWHERE_NONE:
    case A5_OWHERE_HIDDEN:
      break;
    case A5_OWHERE_ALLROOMS:
      *stat = "AllRooms";
      break;
    case A5_OWHERE_LOCATION:
      if (l->is_static) *stat = "SingleLocation";
      else              *dyn  = "InLocation";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_LOCGROUP:
      *stat = "LocationGroup";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_ON_OBJECT:
      *dyn = "OnObject";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_IN_OBJECT:
      *dyn = "InObject";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_HELD_BY:
      *dyn = "HeldByCharacter";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_WORN_BY:
      *dyn = "WornByCharacter";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_PART_OBJECT:
      *stat = "PartOfObject";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_PART_CHAR:
      *stat = "PartOfCharacter";
      *lkey = l->key ? l->key : "";
      break;
    }
}

/* clsEvent.StatusEnum / clsWalk.StatusEnum name for the runtime status int.
   clsWalk's enum has no CountingDownToStart (walks have no StartDelay), so for
   a walk that value is unrepresentable and falls back to NotYetStarted. */
static const char *
fd_status_name (int status, int is_walk)
{
  switch (status)
    {
    case A5_EV_NOTYET:    return "NotYetStarted";
    case A5_EV_RUNNING:   return "Running";
    case A5_EV_COUNTDOWN: return is_walk ? "NotYetStarted" : "CountingDownToStart";
    case A5_EV_PAUSED:    return "Paused";
    case A5_EV_FINISHED:  return "Finished";
    default:              return "NotYetStarted";
    }
}

/* Emit <Property> children for an entity: its base (model) properties with the
   runtime-effective value, plus any override that added a property not in the
   base list.  The Adrift 5 runner's RestoreState *removes* any current property absent
   from the state, so a partial list would strip the entity -- we emit the full
   effective set to be faithful. */
static void
fd_write_props (sb_t *b, a5_state_t *st, const char *entkey,
                const a5_prop_t *props, int n_props)
{
  int i, j;

  for (i = 0; i < n_props; i++)
    {
      const char *val = a5state_entity_prop (st, entkey, props[i].key);
      if (val == nullptr)
        val = props[i].value;   /* rich <Value> block: no scalar -> "" */
      sb_puts (b, "<Property>\n");
      sb_elem (b, "Key", props[i].key);
      sb_elem (b, "Value", val ? val : "");
      sb_puts (b, "</Property>\n");
    }
  for (j = 0; j < st->n_ov; j++)
    {
      int is_base = 0;
      if (!streq (st->ov[j].entity, entkey))
        continue;
      for (i = 0; i < n_props; i++)
        if (streq (props[i].key, st->ov[j].prop)) { is_base = 1; break; }
      if (is_base)
        continue;
      sb_puts (b, "<Property>\n");
      sb_elem (b, "Key", st->ov[j].prop);
      sb_elem (b, "Value", st->ov[j].value ? st->ov[j].value : "");
      sb_puts (b, "</Property>\n");
    }
}

/* Candidate member key space for a group by its type.  Runtime membership
   (AddObjectToGroup / AddLocationToGroup / AddCharacterToGroup) layers into the
   same override store keyed by the member's own key, so save/restore reconcile
   the effective membership against the whole matching entity space. */
static int
group_candidate_count (const a5_adventure_t *adv, const a5_group_t *g)
{
  if (streq (g->type, "Locations"))  return adv->n_locations;
  if (streq (g->type, "Characters")) return adv->n_characters;
  return adv->n_objects;
}

static const char *
group_candidate_key (const a5_adventure_t *adv, const a5_group_t *g, int j)
{
  if (streq (g->type, "Locations"))  return adv->locations[j].key;
  if (streq (g->type, "Characters")) return adv->characters[j].key;
  return adv->objects[j].key;
}

/* The FrankenDrift-interop per-entity body of a <Game> save: locations,
   objects, tasks, events, characters and variables keyed by <Key>.  Written
   only by a full (on-disk) save -- see save_fd_game. */
static void
save_fd_entities (sb_t *b, a5_run_t *run)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;
  const char *player = a5state_player_key (st);
  int i, w;

  /* Locations: property overrides only (base props are static; The runner recomputes
     inherited ones).  Emit a <Location> only when it has a runtime override so
     the runner does not strip its (unlisted) actual properties. */
  for (i = 0; i < adv->n_locations; i++)
    {
      int has_ov = 0, j;
      for (j = 0; j < st->n_ov; j++)
        if (streq (st->ov[j].entity, adv->locations[i].key)) { has_ov = 1; break; }
      if (!has_ov)
        continue;
      sb_puts (b, "<Location>\n");
      sb_elem (b, "Key", adv->locations[i].key);
      fd_write_props (b, st, adv->locations[i].key,
                      adv->locations[i].props, adv->locations[i].n_props);
      sb_puts (b, "</Location>\n");
    }

  /* Objects. */
  for (i = 0; i < adv->n_objects; i++)
    {
      const char *dyn, *stat, *lkey;
      /* <Key> is the object's own identity; the holder/location it sits in is
         the LocationKey (st->obj[i].key, resolved by fd_object_location). */
      fd_object_location (&st->obj[i], &dyn, &stat, &lkey);
      sb_puts (b, "<Object>\n");
      sb_elem (b, "Key", adv->objects[i].key);
      if (dyn != nullptr)
        sb_elem (b, "DynamicExistWhere", dyn);
      if (stat != nullptr)
        sb_elem (b, "StaticExistWhere", stat);
      sb_elem (b, "LocationKey", lkey);
      fd_write_props (b, st, adv->objects[i].key,
                      adv->objects[i].props, adv->objects[i].n_props);
      sb_puts (b, "</Object>\n");
    }

  /* Tasks (Completed / Scored), emitted for every task, matching the runner's
     writer. */
  for (i = 0; i < adv->n_tasks; i++)
    {
      sb_puts (b, "<Task>\n");
      sb_elem (b, "Key", adv->tasks[i].key);
      sb_elem (b, "Completed", st->task_done[i] ? "True" : "False");
      sb_elem (b, "Scored", st->task_scored[i] ? "True" : "False");
      sb_puts (b, "</Task>\n");
    }

  /* Events. */
  for (i = 0; i < adv->n_events && i < (int) run->events->size (); i++)
    {
      a5_event_rt &e = (*run->events)[i];
      /* Scarier's last_se_index is -1 for "no sub-event has run"; The Adrift 5 runner
         has no such sentinel and would do SubEvents(-1) (its only guard is
         Length > index, which -1 passes) -> IndexOutOfRange.  Emit a value >=
         the sub-event count so the runner's guard fails and it leaves LastSubEvent as
         Nothing, matching the -1 meaning. */
      long sei = e.last_se_index < 0 ? adv->events[i].n_subevents
                                     : e.last_se_index;
      sb_puts (b, "<Event>\n");
      sb_elem (b, "Key", adv->events[i].key);
      sb_elem (b, "Status", fd_status_name (e.status, 0));
      sb_elem_l (b, "Timer", e.timer_to_end);
      sb_elem_l (b, "SubEventTime", e.last_se_time);
      sb_elem_l (b, "SubEventIndex", sei);
      sb_puts (b, "</Event>\n");
    }

  /* Characters. */
  for (i = 0; i < adv->n_characters; i++)
    {
      const char *key = adv->characters[i].key;
      const char *where = nullptr, *lkey = nullptr;
      const char *pos = st->char_position[i];
      if (st->char_onobj[i] != nullptr && st->char_onobj[i][0])
        {
          where = (st->char_in && st->char_in[i]) ? "InObject" : "OnObject";
          lkey = st->char_onobj[i];
        }
      else if (st->char_loc[i] != nullptr && st->char_loc[i][0])
        {
          where = "AtLocation";
          lkey = st->char_loc[i];
        }
      /* else: Hidden -- the runner omits ExistWhere/LocationKey. */

      sb_puts (b, "<Character>\n");
      sb_elem (b, "Key", key);
      if (where != nullptr)
        sb_elem (b, "ExistWhere", where);
      if (pos != nullptr && !streq (pos, "Standing"))
        sb_elem (b, "Position", pos);
      if (lkey != nullptr && lkey[0])
        sb_elem (b, "LocationKey", lkey);

      /* Walks owned by this character, in flattened order. */
      for (w = 0; w < (int) run->walks->size (); w++)
        {
          a5_walk_rt &wk = (*run->walks)[w];
          if (wk.char_index != i)
            continue;
          sb_puts (b, "<Walk>\n");
          sb_elem (b, "Status", fd_status_name (wk.status, 1));
          sb_elem_l (b, "Timer", wk.timer_to_end);
          sb_puts (b, "</Walk>\n");
        }

      /* Seen keys: Scarier tracks only the player's, so emit them under the
         player character (the runner stores per-character; NPC seen-sets are untracked). */
      if (streq (key, player))
        {
          sb_flagged_keys (b, "Seen", st->obj_seen,
                           adv->objects, adv->n_objects);
          sb_flagged_keys (b, "Seen", st->char_seen,
                           adv->characters, adv->n_characters);
          sb_flagged_keys (b, "Seen", st->loc_seen,
                           adv->locations, adv->n_locations);
        }

      fd_write_props (b, st, key,
                      adv->characters[i].props, adv->characters[i].n_props);
      sb_puts (b, "</Character>\n");
    }

  /* Variables (single-valued -> Value_0). */
  for (i = 0; i < adv->n_variables; i++)
    {
      int numeric = !streq (adv->variables[i].type, "Text");
      sb_puts (b, "<Variable>\n");
      sb_elem (b, "Key", adv->variables[i].key);
      if (numeric)
        {
          if (st->var_num[i] != 0)
            sb_elem_l (b, "Value_0", st->var_num[i]);
        }
      else if (st->var_text[i] != nullptr && st->var_text[i][0])
        sb_elem (b, "Value_0", st->var_text[i]);
      sb_puts (b, "</Variable>\n");
    }
}

static void
save_fd_game (sb_t *b, a5_run_t *run, int lean)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;
  int i;

  /* In "lean" mode -- the per-turn undo snapshot (a5run_snapshot) -- skip the
     FrankenDrift-interop per-entity body.  Our own restore reads ALL state back
     from <ScarierExt> (restore_scarier_body) and consumes only the <Group>
     order list emitted below (restore_group_order); nothing in that body
     (Location/Object/Task/Event/Character/Variable) is read on the undo path.
     Its cost is dominated by fd_write_props writing every property of every
     entity every turn -- the single biggest per-turn hotspot (~59% of the
     Fortress-of-Fear walkthrough in profiling).  The body exists purely so a
     foreign ADRIFT Runner / FrankenDrift can load a Scarier *file* save; the
     undo stack is never written to disk, so lean snapshots restore byte-for-byte
     identically without it. */
  if (!lean)
    save_fd_entities (b, run);

  /* Groups: live runtime membership for every group type, in arlMembers
     INSERTION order -- the runner's clsGameState writer does `For Each sMember
     In grp.arlMembers`, and that order is load-bearing: RandomKey picks a
     member BY INDEX from the live list, so a save that reorders members (e.g.
     into model-candidate order) changes which world a random
     MoveCharacter-ToLocationGroup jump lands on after a restore (Skybreak).

     Bucket the live list ONCE, in a single pass, rather than filtering gm per
     group: a per-group filter costs n_groups x n_gm strcmps per save (Fortress
     of Fear: 195 groups x 2002 memberships = 390K compares per turn), and this
     save runs every turn to feed the undo stack -- it was over half the
     snapshot's cost.  Bucketing walks gm once and emits every group's members
     in gm order, i.e. exactly the bytes the per-group filter produced.

     The section is kept with the stamp of the list it was written from, and
     copied while that stamp stands. */
  if (run->group_section_valid && run->group_section_stamp == st->gm_stamp)
    sb_putn (b, run->group_section.data (), run->group_section.size ());
  else
  {
    const size_t start = b->len;
    std::unordered_map<std::string, int> gidx;
    std::vector<std::vector<int> > members ((size_t) adv->n_groups);
    int j;
    for (i = 0; i < adv->n_groups; i++)
      if (adv->groups[i].key != nullptr)
        gidx[adv->groups[i].key] = i;
    for (j = 0; j < st->n_gm; j++)
      {
        if (st->gm[j].grp == nullptr)
          continue;
        std::unordered_map<std::string, int>::const_iterator it
          = gidx.find (st->gm[j].grp);
        if (it != gidx.end ())
          members[(size_t) it->second].push_back (j);
      }
    for (i = 0; i < adv->n_groups; i++)
      {
        const std::vector<int> &m = members[(size_t) i];
        sb_puts (b, "<Group>\n");
        sb_elem (b, "Key", adv->groups[i].key);
        for (size_t k = 0; k < m.size (); k++)
          sb_elem (b, "Member", st->gm[m[k]].key);
        sb_puts (b, "</Group>\n");
      }
    if (b->p != nullptr && b->len >= start)
      {
        run->group_section.assign (b->p + start, b->len - start);
        run->group_section_stamp = st->gm_stamp;
        run->group_section_valid = true;
      }
  }

  sb_elem_l (b, "Turns", st->turns);
}

/* Serialise the runtime to a <Game> blob.  `lean` drops the FrankenDrift-interop
   per-entity body (save_fd_game keeps only the <Group> order list restore needs)
   -- used by the per-turn undo snapshot, which never leaves the process.  A full
   (lean=0) blob is what a5run_save hands the frontend for on-disk file saves. */
static char *
a5run_save_blob (a5_run_t *run, size_t *out_len, int lean)
{
  sb_t b;

  if (run == nullptr)
    return nullptr;
  sb_init (&b);
  if (run->save_blob_hint > 0)
    sb_reserve (&b, run->save_blob_hint + 128);
  /* Match the real ADRIFT 5 Runner's FileIO framing: a UTF-8 BOM and an XML
     declaration, and -- critically -- NO trailing newline after the root
     </Game>.  The Runner's .NET XmlReader treats a byte after the root element
     as trailing document content; a trailing '\n' pushes it onto a phantom
     next line where it scans one byte into the decompressor's zero padding and
     aborts with "illegal hex value 0x00".  A genuine Runner save ends exactly
     at "</Game>", so the reader finalises on the end-element and never scans
     further.  Our own a5xml_parse skips the BOM and <?xml?> prolog, so these
     additions are transparent to Scarier's restore. */
  sb_puts (&b, "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
  sb_puts (&b, "<Game>\n");
  save_fd_game (&b, run, lean);
  /* Scarier-private, full-fidelity snapshot; The Adrift 5 runner's LoadState ignores
     any <Game> child it does not query, so <ScarierExt> is invisible to it. */
  sb_puts (&b, "<ScarierExt>\n");
  save_scarier_body (&b, run);
  sb_puts (&b, "</ScarierExt>\n");
  sb_puts (&b, "</Game>");
  run->save_blob_hint = b.len;
  if (out_len != nullptr)
    *out_len = b.len;
  return sb_finish (&b);
}

char *
a5run_save (a5_run_t *run, size_t *out_len)
{
  return a5run_save_blob (run, out_len, 0);
}

/* strtol on a child's text (0 when absent). */
static long
child_long (const a5_xml_node_t *n, const char *name)
{
  const char *t = a5xml_child_text (n, name);
  return t != nullptr ? strtol (t, nullptr, 10) : 0;
}

/* strtol on a node's own text (0 when absent). */
static long
node_long (const a5_xml_node_t *n)
{
  return (n != nullptr && n->text != nullptr) ? strtol (n->text, nullptr, 10) : 0;
}

/* Zero/clear the accumulating + sparse state before applying a save (shared by
   both the native and the Adrift 5 runner readers). */
static void
restore_reset (a5_run_t *run)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;
  int i;

  for (i = 0; i < adv->n_tasks; i++)
    { st->task_done[i] = 0; st->task_scored[i] = 0; }
  a5state_clear_overrides (st);
  if (st->obj_seen != nullptr)
    memset (st->obj_seen, 0, (size_t) adv->n_objects);
  if (st->char_seen != nullptr)
    memset (st->char_seen, 0, (size_t) adv->n_characters);
  if (st->char_introduced != nullptr)
    memset (st->char_introduced, 0, (size_t) adv->n_characters);
  if (st->loc_seen != nullptr)
    memset (st->loc_seen, 0, (size_t) adv->n_locations);
  /* Drop the per-character BECOME seen-stash and re-home the active arrays on
     the starting player (restore/restart rebuilds the player-centric set). */
  for (i = 0; i < adv->n_characters; i++)
    {
      if (st->seen_stash_obj != nullptr)
        { free (st->seen_stash_obj[i]);  st->seen_stash_obj[i] = nullptr; }
      if (st->seen_stash_char != nullptr)
        { free (st->seen_stash_char[i]); st->seen_stash_char[i] = nullptr; }
      if (st->seen_stash_loc != nullptr)
        { free (st->seen_stash_loc[i]);  st->seen_stash_loc[i] = nullptr; }
    }
  st->seen_active_ci = a5state_character_index (st, a5state_player_key (st));
  st->n_disp_once = 0;
  for (i = 0; i < st->n_looks; i++)
    { free (st->looks[i].loc_key); free (st->looks[i].text); }
  st->n_looks = 0;
  free (st->end_message);
  st->end_message = nullptr;
  st->game_over = 0;
  st->turns = 0;
  st->end_displayed = 0;
  free (st->s_it);   st->s_it   = strdup ("");
  free (st->s_them); st->s_them = strdup ("");
  free (st->s_him);  st->s_him  = strdup ("");
  free (st->s_her);  st->s_her  = strdup ("");
}

namespace {

/* One application of the full-fidelity Scarier snapshot: the run it lands in,
   the RNG words gathered on the way (set last, by the driver), and the cursors
   of the positional entity arrays (model order, as save_scarier_body wrote
   them).  apply() sorts one child of the container to the member that reads
   it. */
struct scarier_restore_t
{
  a5_run_t *run;
  a5_state_t *st;
  const a5_adventure_t *adv;
  unsigned int rng[4];
  int native, rng_i;
  int obj_i, char_i, var_i, ev_i, wk_i;

  explicit scarier_restore_t (a5_run_t *run_);
  void apply (const a5_xml_node_t *n);
  bool globals (const a5_xml_node_t *n);
  void object (const a5_xml_node_t *n);
  void character (const a5_xml_node_t *n);
  void variable (const a5_xml_node_t *n);
  void marks (const a5_xml_node_t *n);
  void seen_char (const a5_xml_node_t *n);
  void event (const a5_xml_node_t *n);
  void walk (const a5_xml_node_t *n);
};

scarier_restore_t::scarier_restore_t (a5_run_t *run_)
  : run (run_), st (run_->st), adv (run_->adv), rng { 0, 0, 0, 0 },
    native (0), rng_i (0), obj_i (0), char_i (0), var_i (0), ev_i (0),
    wk_i (0)
{
}

void
scarier_restore_t::apply (const a5_xml_node_t *n)
{
  const char *nm = n->name;
  if (streq (nm, "Object"))
    object (n);
  else if (streq (nm, "Character"))
    character (n);
  else if (streq (nm, "Variable"))
    variable (n);
  else if (streq (nm, "SeenChar"))
    seen_char (n);
  else if (streq (nm, "Event"))
    event (n);
  else if (streq (nm, "Walk"))
    walk (n);
  else if (!globals (n))
    marks (n);
}

/* The one-per-save scalars: RNG, turn and end-game registers, conversation,
   pronoun referents, the player.  False when the node is none of them. */
bool
scarier_restore_t::globals (const a5_xml_node_t *n)
{
  const char *nm = n->name;
  if (streq (nm, "RngNative"))
    native = (int) node_long (n);
  else if (streq (nm, "Rng"))
    { if (rng_i < 4) rng[rng_i++] = (unsigned int) strtoul (n->text ? n->text : "0", nullptr, 10); }
  else if (streq (nm, "EventsRunning"))
    run->events_running = (int) node_long (n);
  else if (streq (nm, "GameOver"))
    st->game_over = (int) node_long (n);
  else if (streq (nm, "Turns"))
    st->turns = (int) node_long (n);
  else if (streq (nm, "EndDisplayed"))
    st->end_displayed = (int) node_long (n);
  else if (streq (nm, "EndMessage"))
    st->end_message = strdup (n->text ? n->text : "");
  else if (streq (nm, "ConvChar"))
    a5state_set_conv_char (st, n->text ? n->text : "");
  else if (streq (nm, "ConvNode"))
    a5state_set_conv_node (st, n->text ? n->text : "");
  else if (streq (nm, "ItRef"))
    { free (st->s_it);   st->s_it   = strdup (n->text ? n->text : ""); }
  else if (streq (nm, "ThemRef"))
    { free (st->s_them); st->s_them = strdup (n->text ? n->text : ""); }
  else if (streq (nm, "HimRef"))
    { free (st->s_him);  st->s_him  = strdup (n->text ? n->text : ""); }
  else if (streq (nm, "HerRef"))
    { free (st->s_her);  st->s_her  = strdup (n->text ? n->text : ""); }
  else if (streq (nm, "PlayerKey"))
    {
      const char *pk = intern_key (adv, n->text);
      if (pk != nullptr)
        {
          st->player_key = pk;
          st->seen_active_ci = a5state_character_index (st, pk);
        }
    }
  else
    return false;
  return true;
}

void
scarier_restore_t::object (const a5_xml_node_t *n)
{
  if (obj_i < adv->n_objects)
    {
      const char *stat = a5xml_child_text (n, "Static");
      st->obj[obj_i].where = (a5_owhere_t) child_long (n, "Where");
      if (stat != nullptr)
        st->obj[obj_i].is_static = (int) strtol (stat, nullptr, 10);
      st->obj[obj_i].key = intern_key (adv, a5xml_child_text (n, "Key"));
      obj_i++;
    }
}

void
scarier_restore_t::character (const a5_xml_node_t *n)
{
  if (char_i < adv->n_characters)
    {
      const char *loc = a5xml_child_text (n, "Loc");
      const char *pos = a5xml_child_text (n, "Position");
      const char *onobj = a5xml_child_text (n, "OnObj");
      st->char_loc[char_i] = intern_key (adv, loc);
      free (st->char_position[char_i]);
      st->char_position[char_i] = strdup (pos ? pos : "Standing");
      st->char_onobj[char_i] = intern_key (adv, onobj);
      /* Carrier character (follower riding another character).  Absent
         element -> intern_key(NULL) clears it, correctly un-following a
         character that stopped riding since the save was model-init. */
      if (st->char_onchar != nullptr)
        st->char_onchar[char_i] =
          intern_key (adv, a5xml_child_text (n, "OnChar"));
      if (st->char_in != nullptr)
        st->char_in[char_i] = (char) child_long (n, "In");
      char_i++;
    }
}

void
scarier_restore_t::variable (const a5_xml_node_t *n)
{
  if (var_i < adv->n_variables)
    {
      const char *txt = a5xml_child_text (n, "Text");
      const char *arr = a5xml_child_text (n, "Arr");
      st->var_num[var_i] = child_long (n, "Num");
      free (st->var_text[var_i]);
      st->var_text[var_i] = txt != nullptr ? strdup (txt) : nullptr;
      if (arr != nullptr && st->var_arr != nullptr
          && st->var_arr[var_i] != nullptr)
        {
          const char *p = arr;
          int e;
          for (e = 0; e < adv->variables[var_i].array_length
                      && p != nullptr; e++)
            {
              st->var_arr[var_i][e] = strtol (p, nullptr, 10);
              p = strchr (p, ',');
              if (p != nullptr) p++;
            }
        }
      var_i++;
    }
}

/* The sparse, keyed records: task done / scored, property overrides, the
   active viewpoint's seen sets, shown-once nodes and remembered looks. */
void
scarier_restore_t::marks (const a5_xml_node_t *n)
{
  const char *nm = n->name;
  if (streq (nm, "TaskDone"))
    {
      int ti = a5state_task_index (st, n->text ? n->text : "");
      if (ti >= 0)
        st->task_done[ti] = 1;
    }
  else if (streq (nm, "TaskScored"))
    {
      int ti = a5state_task_index (st, n->text ? n->text : "");
      if (ti >= 0)
        st->task_scored[ti] = 1;
    }
  else if (streq (nm, "PropOv"))
    {
      const char *e = a5xml_child_text (n, "Entity");
      const char *p = a5xml_child_text (n, "Prop");
      const char *v = a5xml_child_text (n, "Value");
      if (e != nullptr && p != nullptr)
        a5state_set_prop (st, e, p, v ? v : "");
    }
  else if (streq (nm, "ObjSeen"))
    {
      int oi = a5state_object_index (st, n->text ? n->text : "");
      if (oi >= 0 && st->obj_seen != nullptr)
        st->obj_seen[oi] = 1;
    }
  else if (streq (nm, "CharSeen"))
    {
      int ci = a5state_character_index (st, n->text ? n->text : "");
      if (ci >= 0 && st->char_seen != nullptr)
        st->char_seen[ci] = 1;
    }
  else if (streq (nm, "CharIntroduced"))
    {
      int ci = a5state_character_index (st, n->text ? n->text : "");
      if (ci >= 0 && st->char_introduced != nullptr)
        st->char_introduced[ci] = 1;
    }
  else if (streq (nm, "LocSeen"))
    a5state_mark_loc_seen (st, n->text ? n->text : "");
  else if (streq (nm, "Displayed"))
    {
      long k = strtol (n->text ? n->text : "-1", nullptr, 10);
      ensure_dom_cache (run);
      if (k >= 0 && k < (long) run->dom_nodes->size ())
        a5state_disp_once_mark (st, (*run->dom_nodes)[(size_t) k]);
    }
  else if (streq (nm, "Look"))
    {
      a5state_push_look (st, a5xml_child_text (n, "LocKey"),
                         a5xml_child_text (n, "Text"),
                         a5xml_child_text (n, "Event"));
    }
}

/* A non-active viewpoint's per-character seen set (BECOME games).
   Populate its stash slot, or the active arrays if it is (unexpectedly)
   the current player. */
void
scarier_restore_t::seen_char (const a5_xml_node_t *n)
{
  int ci = a5state_character_index (st, a5xml_child_text (n, "Key"));
  if (ci >= 0)
    {
      const a5_xml_node_t *c;
      int active = (ci == st->seen_active_ci);
      char *os = st->obj_seen, *cs = st->char_seen, *ls = st->loc_seen;
      if (!active)
        {
          if (st->seen_stash_obj[ci] == nullptr)
            st->seen_stash_obj[ci]
              = (char *) calloc ((size_t) adv->n_objects, 1);
          if (st->seen_stash_char[ci] == nullptr)
            st->seen_stash_char[ci]
              = (char *) calloc ((size_t) adv->n_characters, 1);
          if (st->seen_stash_loc[ci] == nullptr)
            st->seen_stash_loc[ci]
              = (char *) calloc ((size_t) adv->n_locations, 1);
          os = st->seen_stash_obj[ci]; cs = st->seen_stash_char[ci]; ls = st->seen_stash_loc[ci];
        }
      for (c = n->first_child; c != nullptr; c = c->next)
        {
          if (streq (c->name, "ObjSeen"))
            { int oi = a5state_object_index (st, c->text ? c->text : "");
              if (oi >= 0 && os) os[oi] = 1; }
          else if (streq (c->name, "CharSeen"))
            { int xi = a5state_character_index (st, c->text ? c->text : "");
              if (xi >= 0 && cs) cs[xi] = 1; }
          else if (streq (c->name, "LocSeen"))
            { int li = a5state_location_index (st, c->text ? c->text : "");
              if (li >= 0 && ls) ls[li] = 1; }
        }
    }
}

void
scarier_restore_t::event (const a5_xml_node_t *n)
{
  if (ev_i < (int) run->events->size ())
    {
      a5_event_rt &e = (*run->events)[ev_i];
      const a5_xml_node_t *c;
      const char *trg = a5xml_child_text (n, "Trigger");
      int s = 0;
      e.status       = (int)  child_long (n, "Status");
      e.length_value =        child_long (n, "Length");
      e.length_set   = 1;                /* the save holds Length.Value */
      e.timer_to_end =        child_long (n, "Timer");
      e.last_se_time =        child_long (n, "LastSeTime");
      e.last_se_index = (int) child_long (n, "LastSeIndex");
      e.just_started = (int)  child_long (n, "JustStarted");
      e.next_command = (int)  child_long (n, "NextCommand");
      e.when_start   = (int)  child_long (n, "WhenStart");
      e.triggering_task = trg ? trg : "";
      for (c = n->first_child; c != nullptr; c = c->next)
        if (streq (c->name, "SeFt") && s < (int) e.se_ft.size ())
          e.se_ft[s++] = node_long (c);
      ev_i++;
    }
}

void
scarier_restore_t::walk (const a5_xml_node_t *n)
{
  if (wk_i < (int) run->walks->size ())
    {
      a5_walk_rt &w = (*run->walks)[wk_i];
      const a5_xml_node_t *c;
      const char *trg = a5xml_child_text (n, "Trigger");
      int sd = 0, sf = 0, ca = 0;
      w.status       = (int) child_long (n, "Status");
      w.length       =       child_long (n, "Length");
      w.timer_to_end =       child_long (n, "Timer");
      w.last_sw_time =       child_long (n, "LastSwTime");
      w.last_sw_index = (int) child_long (n, "LastSwIndex");
      w.just_started = (int) child_long (n, "JustStarted");
      w.next_command = (int) child_long (n, "NextCommand");
      w.triggering_task = trg ? trg : "";
      for (c = n->first_child; c != nullptr; c = c->next)
        {
          if (streq (c->name, "StepDur") && sd < (int) w.step_dur.size ())
            w.step_dur[sd++] = node_long (c);
          else if (streq (c->name, "SwFt") && sf < (int) w.sw_ft.size ())
            w.sw_ft[sf++] = node_long (c);
          else if (streq (c->name, "CameAcross") && ca < (int) w.came_across.size ())
            w.came_across[ca++] = (char) node_long (c);
        }
      wk_i++;
    }
}

} /* namespace */

/* Apply the full-fidelity Scarier snapshot from `container` (the <ScarierExt>
   node of a <Game> save, or the root of a legacy raw <SaveState> file). */
static void
restore_scarier_body (a5_run_t *run, const a5_xml_node_t *container)
{
  const a5_xml_node_t *n;

  restore_reset (run);

  scarier_restore_t restore (run);
  for (n = container->first_child; n != nullptr; n = n->next)
    restore.apply (n);

  /* PropOv restored the @InGroup overrides but not the ordered arlMembers list;
     rebuild it so RandomKey / EverywhereInGroup enumerate runtime membership. */
  a5state_rebuild_live_groups (run->st);

  a5rand_set_state (restore.native, restore.rng);
}

/* ------------------------------------------------ the Adrift 5 runner <Game> reader ---
   Applied only to a foreign save (an ADRIFT 5 Runner file with no
   <ScarierExt>).  Keyed lookups, tolerant of missing/extra entities; the RNG
   re-seeds to 1234 (the runner saves carry none), so a game that draws randomness right
   after such a restore diverges from the session that wrote the save.  Accepted:
   only foreign saves are affected, and Scarier's own carry <ScarierExt>. */

static a5_owhere_t
fd_decode_object_where (const char *dyn, const char *stat, int *is_static)
{
  *is_static = 0;
  if (stat != nullptr && !streq (stat, "NoRooms"))
    {
      *is_static = 1;
      if (streq (stat, "SingleLocation"))  return A5_OWHERE_LOCATION;
      if (streq (stat, "AllRooms"))        return A5_OWHERE_ALLROOMS;
      if (streq (stat, "LocationGroup"))   return A5_OWHERE_LOCGROUP;
      if (streq (stat, "PartOfObject"))    return A5_OWHERE_PART_OBJECT;
      if (streq (stat, "PartOfCharacter")) return A5_OWHERE_PART_CHAR;
      return A5_OWHERE_HIDDEN;
    }
  if (dyn != nullptr)
    {
      if (streq (dyn, "InLocation"))       return A5_OWHERE_LOCATION;
      if (streq (dyn, "OnObject"))         return A5_OWHERE_ON_OBJECT;
      if (streq (dyn, "InObject"))         return A5_OWHERE_IN_OBJECT;
      if (streq (dyn, "HeldByCharacter"))  return A5_OWHERE_HELD_BY;
      if (streq (dyn, "WornByCharacter"))  return A5_OWHERE_WORN_BY;
    }
  return A5_OWHERE_HIDDEN;
}

static int
fd_decode_event_status (const char *s)
{
  if (s == nullptr)                            return A5_EV_NOTYET;
  if (streq (s, "Running"))                 return A5_EV_RUNNING;
  if (streq (s, "CountingDownToStart"))     return A5_EV_COUNTDOWN;
  if (streq (s, "Paused"))                  return A5_EV_PAUSED;
  if (streq (s, "Finished"))                return A5_EV_FINISHED;
  return A5_EV_NOTYET;
}

static int
fd_decode_walk_status (const char *s)
{
  if (s == nullptr)              return A5_EV_NOTYET;
  if (streq (s, "Running"))   return A5_EV_RUNNING;
  if (streq (s, "Paused"))    return A5_EV_PAUSED;
  if (streq (s, "Finished"))  return A5_EV_FINISHED;
  return A5_EV_NOTYET;
}

static int
fd_event_index (const a5_adventure_t *adv, const char *key)
{
  int i;
  if (key == nullptr)
    return -1;
  for (i = 0; i < adv->n_events; i++)
    if (streq (adv->events[i].key, key))
      return i;
  return -1;
}

/* Apply every <Property> child of `n` as a runtime override on `entkey`. */
static void
restore_props (a5_state_t *st, const a5_xml_node_t *n, const char *entkey)
{
  const a5_xml_node_t *c;
  for (c = n->first_child; c != nullptr; c = c->next)
    if (streq (c->name, "Property"))
      {
        const char *pk = a5xml_child_text (c, "Key");
        const char *pv = a5xml_child_text (c, "Value");
        if (pk != nullptr)
          a5state_set_prop (st, entkey, pk, pv ? pv : "");
      }
}

/* Restore one saved <Object> node. */
static void
restore_fd_object (a5_run_t *run, const a5_xml_node_t *n)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;

  int oi = a5state_object_index (st, a5xml_child_text (n, "Key"));
  if (oi >= 0)
    {
      int is_static = 0;
      a5_owhere_t w = fd_decode_object_where (
          a5xml_child_text (n, "DynamicExistWhere"),
          a5xml_child_text (n, "StaticExistWhere"), &is_static);
      st->obj[oi].where = w;
      st->obj[oi].is_static = is_static;
      if (w == A5_OWHERE_NONE || w == A5_OWHERE_HIDDEN
          || w == A5_OWHERE_ALLROOMS)
        st->obj[oi].key = nullptr;
      else
        st->obj[oi].key =
          intern_key (adv, a5xml_child_text (n, "LocationKey"));
      restore_props (st, n, adv->objects[oi].key);
    }
}

/* Restore one saved <Task> node. */
static void
restore_fd_task (a5_run_t *run, const a5_xml_node_t *n)
{
  a5_state_t *st = run->st;

  int ti = a5state_task_index (st, a5xml_child_text (n, "Key"));
  if (ti >= 0)
    {
      st->task_done[ti] = a5xml_bool (a5xml_child_text (n, "Completed"));
      st->task_scored[ti] = a5xml_bool (a5xml_child_text (n, "Scored"));
    }
}

/* Restore one saved <Event> node. */
static void
restore_fd_event (a5_run_t *run, const a5_xml_node_t *n)
{
  const a5_adventure_t *adv = run->adv;

  int ei = fd_event_index (adv, a5xml_child_text (n, "Key"));
  if (ei >= 0 && ei < (int) run->events->size ())
    {
      a5_event_rt &e = (*run->events)[ei];
      int sei = (int) child_long (n, "SubEventIndex");
      e.status = fd_decode_event_status (a5xml_child_text (n, "Status"));
      e.timer_to_end = child_long (n, "Timer");
      e.last_se_time = child_long (n, "SubEventTime");
      /* Our own writer emits count for "none"; also clamp any index at
         or past the sub-event list back to Scarier's -1 sentinel. */
      e.last_se_index = (sei >= 0 && sei < adv->events[ei].n_subevents)
                          ? sei : -1;
    }
}

/* Restore one saved <Character> node. */
static void
restore_fd_character (a5_run_t *run, const a5_xml_node_t *n)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;
  const char *player = a5state_player_key (st);
  const a5_xml_node_t *c;

  int ci = a5state_character_index (st, a5xml_child_text (n, "Key"));
  if (ci >= 0)
    {
      const char *where = a5xml_child_text (n, "ExistWhere");
      const char *pos   = a5xml_child_text (n, "Position");
      const char *lkey  = a5xml_child_text (n, "LocationKey");
      int wslot = 0;

      st->char_loc[ci] = nullptr;
      st->char_onobj[ci] = nullptr;
      if (st->char_in != nullptr)
        st->char_in[ci] = 0;
      if (where == nullptr || streq (where, "Hidden")
          || streq (where, "Uninitialised"))
        { /* hidden: no placement */ }
      else if (streq (where, "AtLocation")
               || streq (where, "OnCharacter"))
        st->char_loc[ci] = intern_key (adv, lkey);
      else if (streq (where, "OnObject"))
        st->char_onobj[ci] = intern_key (adv, lkey);
      else if (streq (where, "InObject"))
        {
          st->char_onobj[ci] = intern_key (adv, lkey);
          if (st->char_in != nullptr)
            st->char_in[ci] = 1;
        }
      free (st->char_position[ci]);
      st->char_position[ci] = strdup (pos ? pos : "Standing");

      /* Walks: nested <Walk> in order map onto this character's slots. */
      for (c = n->first_child; c != nullptr; c = c->next)
        if (streq (c->name, "Walk"))
          {
            int found = -1, count = 0, w;
            for (w = 0; w < (int) run->walks->size (); w++)
              if ((*run->walks)[w].char_index == ci)
                { if (count == wslot) { found = w; break; } count++; }
            if (found >= 0)
              {
                a5_walk_rt &wk = (*run->walks)[found];
                wk.status =
                  fd_decode_walk_status (a5xml_child_text (c, "Status"));
                wk.timer_to_end = child_long (c, "Timer");
              }
            wslot++;
          }

      /* Seen keys (player only -- Scarier tracks no NPC seen-sets). */
      if (streq (adv->characters[ci].key, player))
        for (c = n->first_child; c != nullptr; c = c->next)
          if (streq (c->name, "Seen"))
            {
              const char *k = c->text ? c->text : "";
              int oi = a5state_object_index (st, k);
              int cc = a5state_character_index (st, k);
              if (oi >= 0 && st->obj_seen != nullptr)
                st->obj_seen[oi] = 1;
              else if (cc >= 0 && st->char_seen != nullptr)
                st->char_seen[cc] = 1;
              else
                a5state_mark_loc_seen (st, k);
            }

      restore_props (st, n, adv->characters[ci].key);
    }
}

/* Restore one saved <Variable> node. */
static void
restore_fd_variable (a5_run_t *run, const a5_xml_node_t *n)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;

  int vi = a5state_variable_index (st, a5xml_child_text (n, "Key"));
  if (vi >= 0)
    {
      const char *v0 = a5xml_child_text (n, "Value_0");
      if (!streq (adv->variables[vi].type, "Text"))
        {
          st->var_num[vi] = v0 != nullptr ? strtol (v0, nullptr, 10) : 0;
          /* An array variable saves each element as Value_0..
             Value_(n-1) (FileIO.vb:174; zero-valued elements are
             omitted from the save, so default to 0). */
          if (st->var_arr != nullptr && st->var_arr[vi] != nullptr)
            for (int e = 0; e < adv->variables[vi].array_length; e++)
              {
                char tag[24];
                snprintf (tag, sizeof tag, "Value_%d", e);
                const char *ve = a5xml_child_text (n, tag);
                st->var_arr[vi][e] = ve != nullptr ? strtol (ve, nullptr, 10) : 0;
              }
        }
      else
        {
          free (st->var_text[vi]);
          st->var_text[vi] = strdup (v0 ? v0 : "");
        }
    }
}

/* Restore one saved <Group> node. */
static void
restore_fd_group (a5_run_t *run, const a5_xml_node_t *n)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;
  const a5_xml_node_t *c;

  const char *gk = a5xml_child_text (n, "Key");
  int gi;
  for (gi = 0; gi < adv->n_groups; gi++)
    if (streq (adv->groups[gi].key, gk))
      break;
  if (gi < adv->n_groups)
    {
      const a5_group_t *g = &adv->groups[gi];
      int j, ncand = group_candidate_count (adv, g);
      /* Effective membership = the listed <Member> set (add/remove vs
         the model list via the runtime override layer).  Reconcile
         across the whole candidate space so runtime removals are also
         restored, for Objects, Locations and Characters groups alike. */
      for (j = 0; j < ncand; j++)
        {
          const char *ck = group_candidate_key (adv, g, j);
          int listed = 0;
          for (c = n->first_child; c != nullptr; c = c->next)
            if (streq (c->name, "Member") && streq (c->text, ck))
              { listed = 1; break; }
          if (listed != a5state_object_in_group (st, gk, ck))
            a5state_set_object_in_group (st, gk, ck, listed);
        }
    }
}

static void
restore_fd_game (a5_run_t *run, const a5_xml_node_t *root)
{
  a5_state_t *st = run->st;
  const a5_xml_node_t *n;

  restore_reset (run);

  for (n = root->first_child; n != nullptr; n = n->next)
    {
      const char *nm = n->name;
      if (streq (nm, "Turns"))
        st->turns = (int) node_long (n);
      else if (streq (nm, "Location"))
        {
          const char *lk = a5xml_child_text (n, "Key");
          if (lk != nullptr)
            restore_props (st, n, lk);
        }
      else if (streq (nm, "Object"))
        restore_fd_object (run, n);
      else if (streq (nm, "Task"))
        restore_fd_task (run, n);
      else if (streq (nm, "Event"))
        restore_fd_event (run, n);
      else if (streq (nm, "Character"))
        restore_fd_character (run, n);
      else if (streq (nm, "Variable"))
        restore_fd_variable (run, n);
      else if (streq (nm, "Group"))
        restore_fd_group (run, n);
    }
  /* No RNG in an ADRIFT 5 Runner save: the run keeps its current (fresh: 1234) seed. */
}

/* Rebuild each group's live arlMembers list in the save's <Group>/<Member>
   document order (the runner: `arlMembers.Clear()` then Add per saved member,
   clsState.vb RestoreState / FileIO.vb LoadGameState).  Runs after the
   membership SET has been restored (PropOv overrides for our own saves, the
   candidate-space reconcile for foreign Runner saves): both of those leave the
   live list in model/creation order, and that order is load-bearing --
   RandomKey picks a member BY INDEX, so a Skybreak random jump lands on a
   different world after a restore unless the saved insertion order is kept. */
static void
restore_group_order (a5_run_t *run, const a5_xml_node_t *root)
{
  a5_state_t *st = run->st;
  const a5_xml_node_t *n, *c;
  for (n = root->first_child; n != nullptr; n = n->next)
    {
      const char *gk;
      if (!streq (n->name, "Group"))
        continue;
      gk = a5xml_child_text (n, "Key");
      if (gk == nullptr)
        continue;
      while (a5state_group_count (st, gk) > 0)
        a5state_group_remove_member (st, gk,
                                     a5state_group_member_at (st, gk, 0));
      for (c = n->first_child; c != nullptr; c = c->next)
        if (streq (c->name, "Member") && c->text != nullptr && c->text[0])
          a5state_group_add_member (st, gk, c->text);
    }
}

int
a5run_restore (a5_run_t *run, const char *data, size_t len)
{
  a5_xml_doc_t *doc;
  a5_xml_node_t *root;
  char *buf;

  if (run == nullptr || data == nullptr)
    return 0;
  buf = (char *) malloc (len + 1);
  if (buf == nullptr)
    return 0;
  memcpy (buf, data, len);
  buf[len] = '\0';
  doc = a5xml_parse (buf, (uint32_t) len);
  if (doc == nullptr)
    { free (buf); return 0; }
  root = a5xml_root (doc);
  if (root == nullptr)
    { a5xml_free (doc); return 0; }

  if (streq (root->name, "SaveState"))
    {
      /* Legacy raw Scarier save (pre-interop). */
      restore_scarier_body (run, root);
    }
  else if (streq (root->name, "Game"))
    {
      const a5_xml_node_t *ext = a5xml_child (root, "ScarierExt");
      if (ext != nullptr)
        restore_scarier_body (run, ext);   /* our own save: full fidelity */
      else
        restore_fd_game (run, root);       /* a foreign ADRIFT 5 Runner save */
      /* Either path restored membership as a SET (live list in model order);
         re-impose the saved arlMembers insertion order. */
      restore_group_order (run, root);
    }
  else
    { a5xml_free (doc); return 0; }

  a5xml_free (doc);
  return 1;
}

/* ------------------------------------------------------------------- undo */

/* Turns of undo kept (undo_stack, oldest first).  Matches the v4 engine's
   MEMO_UNDO_TABLE_SIZE.  Snapshots are raw a5run_save XML, so the worst case in
   the corpus (Tingalan, ~400K a snapshot) holds about 6MB. */
enum { A5_UNDO_DEPTH = 16 };

/* Push a snapshot + its turn text, dropping the oldest entry at depth. */
static void
undo_push (a5_run_t *run, std::string blob, std::string turn_text)
{
  if (run->undo_stack.size () >= A5_UNDO_DEPTH)
    {
      run->undo_stack.erase (run->undo_stack.begin ());
      run->undo_turn_text.erase (run->undo_turn_text.begin ());
    }
  run->undo_stack.push_back (std::move (blob));
  run->undo_turn_text.push_back (std::move (turn_text));
}

/* Capture the current runtime as an undo point (a5run_save pushed onto the
   run's undo stack).  Call from the frontend just BEFORE a5run_input, so the
   snapshot is the pre-turn state (a5run_input increments st->turns on entry).
   The oldest snapshot drops off once the stack holds A5_UNDO_DEPTH turns. */
void
a5run_snapshot (a5_run_t *run)
{
  char *blob;
  size_t len = 0;

  if (run == nullptr)
    return;
  blob = a5run_save_blob (run, &len, 1);   /* lean: skip the FD-interop body */
  if (blob == nullptr)
    return;
  undo_push (run, std::string (blob, len), run->last_turn_text);
  free (blob);
}

/* Discard all undo points without restoring (e.g. after a RESTORE from file, so
   UNDO does not silently jump back across the restore boundary). */
void
a5run_undo_forget (a5_run_t *run)
{
  if (run == nullptr)
    return;
  run->undo_stack.clear ();
  run->undo_turn_text.clear ();
}

/* Drop any open "Which X?" question, with everything remembered about it. */
static void
amb_forget (a5_run_t *run)
{
  run->amb_active = 0;
  run->amb_task_index = run->amb_command_index = -1;
  run->amb_keys.clear ();
  run->amb_input.clear ();
  run->amb_ref_name.clear ();
  run->amb_word.clear ();
}

/* Restore the newest snapshot (undo the last turn).  Returns 1 on success, 0
   when no undo point remains or the restore failed.  Each snapshot is consumed
   on a successful undo, so repeated UNDO walks back up to A5_UNDO_DEPTH turns
   before reporting "no undo available" (deeper than the Adrift 5 runner's
   one-deep model -- a deliberate superset; the two diverge only on consecutive
   UNDOs, which no golden walkthrough issues).  Cross-turn parser transients
   that predate the restored turn (a pending disambiguation, a remembered verb,
   a pending FailOverride) are cleared so the restored state is clean. */
int
a5run_undo (a5_run_t *run)
{
  int ok;

  if (run == nullptr || run->undo_stack.empty ())
    return 0;
  /* a5run_restore copies its input before mutating state, but detach the
     snapshot first so we never read through storage the restore might touch. */
  std::string blob = std::move (run->undo_stack.back ());
  run->undo_stack.pop_back ();
  std::string turn_text = std::move (run->undo_turn_text.back ());
  run->undo_turn_text.pop_back ();
  ok = a5run_restore (run, blob.data (), blob.size ());
  if (!ok)
    return 0;
  /* The runner's Undo() restores the session's sTurnOutput along with the
     state; the post-game guard replays it after "Undone.". */
  run->last_turn_text = std::move (turn_text);
  amb_forget (run);
  run->remembered_verb.clear ();
  run->pending_failover = nullptr;
  return 1;
}

/* Undo-stack export/import for the Spatterlight autosave (see a5run.h).  The
   entries are the raw snapshot/turn-text strings, oldest first. */
int
a5run_undo_depth (a5_run_t *run)
{
  return run == nullptr ? 0 : (int) run->undo_stack.size ();
}

const char *
a5run_undo_peek (a5_run_t *run, int i, size_t *len,
                 const char **turn_text, size_t *tt_len)
{
  if (run == nullptr || i < 0 || (size_t) i >= run->undo_stack.size ())
    return nullptr;
  *len = run->undo_stack[(size_t) i].size ();
  *turn_text = run->undo_turn_text[(size_t) i].data ();
  *tt_len = run->undo_turn_text[(size_t) i].size ();
  return run->undo_stack[(size_t) i].data ();
}

void
a5run_undo_push_blob (a5_run_t *run, const char *blob, size_t len,
                      const char *turn_text, size_t tt_len)
{
  if (run == nullptr || blob == nullptr)
    return;
  undo_push (run, std::string (blob, len),
             std::string (turn_text == nullptr ? "" : turn_text,
                          turn_text == nullptr ? 0 : tt_len));
}

void
a5run_get_turn_text (a5_run_t *run, const char **text, size_t *len)
{
  *text = run == nullptr ? "" : run->last_turn_text.data ();
  *len = run == nullptr ? 0 : run->last_turn_text.size ();
}

void
a5run_set_turn_text (a5_run_t *run, const char *text, size_t len)
{
  if (run == nullptr)
    return;
  run->last_turn_text.assign (text == nullptr ? "" : text,
                              text == nullptr ? 0 : len);
}

/* The cross-turn parser continuation as a byte string, for the Spatterlight
   autosave (see a5run.h).  A run of session records (sessrec.h), the framing
   scrunner's session state uses; unknown keys are skipped on the way back,
   so records can be added without invalidating an older autosave. */
char *
a5run_pending_save (a5_run_t *run, size_t *out_len)
{
  std::string out;

  if (run == nullptr)
    return nullptr;
  if (run->amb_active)
    {
      sessrec_put (out, "amb_task", std::to_string ((long) run->amb_task_index));
      sessrec_put (out, "amb_command",
                   std::to_string ((long) run->amb_command_index));
      sessrec_put (out, "amb_input", run->amb_input);
      sessrec_put (out, "amb_ref_name", run->amb_ref_name);
      sessrec_put (out, "amb_ref_type", std::string (1, run->amb_ref_type));
      sessrec_put (out, "amb_word", run->amb_word);
      for (const std::string &key : run->amb_keys)
        sessrec_put (out, "amb_key", key);
    }
  if (!run->remembered_verb.empty ())
    sessrec_put (out, "remembered_verb", run->remembered_verb);

  char *blob = (char *) malloc (out.size () + 1);
  if (blob == nullptr)
    return nullptr;
  memcpy (blob, out.data (), out.size ());
  blob[out.size ()] = '\0';
  if (out_len != nullptr)
    *out_len = out.size ();
  return blob;
}

int
a5run_pending_restore (a5_run_t *run, const char *data, size_t len)
{
  const std::string state (data == nullptr ? "" : data, data == nullptr ? 0 : len);
  long task = -1, command = -1;
  std::string input, ref_name, word, verb;
  std::vector<std::string> keys;
  char ref_type = 'o';
  int has_amb = 0;
  size_t pos = 0;

  if (run == nullptr)
    return 0;
  while (pos < state.size ())
    {
      std::string key, value;

      if (!sessrec_next (state, &pos, &key, &value))
        return 0;

      if (key == "amb_task")
        task = strtol (value.c_str (), nullptr, 10), has_amb = 1;
      else if (key == "amb_command")
        command = strtol (value.c_str (), nullptr, 10);
      else if (key == "amb_input")
        input = value;
      else if (key == "amb_ref_name")
        ref_name = value;
      else if (key == "amb_ref_type" && !value.empty ())
        ref_type = value[0];
      else if (key == "amb_word")
        word = value;
      else if (key == "amb_key")
        keys.push_back (value);
      else if (key == "remembered_verb")
        verb = value;
    }

  amb_forget (run);
  run->remembered_verb = verb;
  if (has_amb)
    {
      /* The remembered task and command pattern index this game's tables;
         a question that no longer fits them is dropped, not half-restored. */
      if (task < 0 || task >= run->adv->n_tasks
          || command < 0
          || command >= run->adv->tasks[task].n_commands
          || (ref_type != 'o' && ref_type != 'c') || keys.empty ())
        return 0;
      run->amb_active = 1;
      run->amb_task_index = (int) task;
      run->amb_command_index = (int) command;
      run->amb_input = input;
      run->amb_ref_name = ref_name;
      run->amb_ref_type = ref_type;
      run->amb_word = word;
      run->amb_keys = keys;
    }
  return 1;
}
