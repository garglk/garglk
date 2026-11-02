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
 * ADRIFT 5 support for Scarier -- turn loop core.  See a5run.h.
 *
 * A port of the player-turn half of clsUserSession: GetGeneralTask (walk tasks
 * in priority order, match command, resolve references, refine by restrictions)
 * plus the per-turn driver (a5run_input), run state, status-line / media
 * accessors, the "Which X?" disambiguation prompt (DisplayAmbiguityQuestion /
 * ResolveAmbiguity), the intro, the end-of-game block and the Display()
 * boundary every turn ends with (finish_turn).  The turn loop was split across
 * sibling translation units; the shared run struct and cross-file entry points
 * live in a5run_internal.h:
 *
 *   a5run_words.cpp   EvaluateInput's word layer: pronoun and synonym
 *                     substitution, GrabIt, the SystemTasks verbs and the
 *                     NotUnderstood ladder
 *   a5run_ref.cpp     reference resolution (InputMatchesObject/Character) and
 *                     multiple-object (%objects%) references
 *   a5run_action.cpp  task dispatch, run_task/run_general; a5run_act.cpp the
 *                     ExecuteActions / ExecuteSingleAction action set, and
 *                     a5run_resp/conv/sort.cpp the rest of that layer
 *   a5run_events.cpp  the clsEvent / clsWalk turn-based runtime
 *   a5run_save.cpp    save / restore (the Runner-compatible <Game> file with
 *                     its <ScarierExt> snapshot), undo and the pending-state
 *                     export for the autosave
 *   a5sb.cpp          the output buffer (sb_t) shared by all of the above
 */

#include "a5run_internal.h"

int a5run_trace = 0;

/* -------------------------------------------------------------- message tests */

/* The runner's bHasOutput (clsUserSession.vb:1272) for an ALREADY-RENDERED plain message
   (the form Scarier holds at every response/emit site, after markup has been
   converted to plain).  The runner keeps a message unless StripCarats leaves the empty
   string AND it is not a known formatting tag / ALR key.  On plain text the tags
   are already gone, so the faithful test is simply "non-empty" -- and crucially
   WHITESPACE COUNTS: a `<Text> </Text>` completion renders to " ", which the runner keeps
   (StripCarats(" ")=" " != "") and space-joins to the next response.  The old
   version dropped whitespace-only messages, which swallowed the leading indent the runner
   renders before search-triggered encounter titles (Tingalan) and blank-line
   separators the runner emits between responses (Bug Hunt).  For a message still carrying
   MARKUP (what the runner's Display sees pre-render), use fd_has_output, which runs the
   actual StripCarats + known-tag + ALR fallback. */
int
msg_has_output (const char *m)
{
  /* A5_ALR_MARK bytes are stripped-tag sentinels, not output: a message whose
     plain rendering is only stripped tags (e.g. "<font colour=X>") was empty in
     the runner's stripped view too.  The interactive-mode presentation marks (waitkey
     pause points and \006<number>\006 image slots) are likewise not output --
     they stand for the same stripped tags, so a message must be judged empty
     or not identically in both modes. */
  if (m == nullptr)
    return 0;
  for (; *m != '\0'; m++)
    {
      if (a5_is_span_mark (*m))
        {
          /* Skip the \006<number>\006 / \022<name>\022 / \024<index>\024 /
             \026<seconds>\026 / \027<colour>\027 span (or a stray mark). */
          const char *e = strchr (m + 1, *m);
          if (e == nullptr)
            continue;
          m = e;
        }
      else if (!a5_is_pres_mark (*m))
        return 1;
    }
  return 0;
}

/* Is the last VISIBLE act of an already-rendered message a <cls>?  The trailing
   presentation sentinels (stripped-tag A5_ALR_MARK, waitkey/centre/bold marks)
   are not output -- the runner never saw them -- so markup sitting after the
   wipe marker does not make the <cls> any less final.  The Axe of Kolt comp
   intro ends "<waitkey><cls></b>", and that stripped </b> leaves an
   A5_ALR_MARK behind the marker; a raw last-byte test then misreads it as
   "text after the wipe" and adds a paragraph break the runner never shows. */
static int
msg_ends_with_cls (const char *m)
{
  size_t n = m != nullptr ? strlen (m) : 0;
  while (n > 0)
    {
      char c = m[n - 1];
      if (c == A5_CLS_MARK)
        return 1;
      if (c == A5_SOUND_MARK || c == A5_WAIT_MARK || c == A5_COLOUR_MARK)
        {
          /* A trailing \024<index>\024 sound, \026<seconds>\026 wait or
             \027<colour>\027 colour span is presentation, not output -- step
             back over the whole span (a stray unpaired mark reads as text,
             like any other unrecognised byte). */
          size_t j = n - 1;
          while (j > 0 && m[j - 1] != c)
            j--;
          if (j == 0)
            return 0;
          n = j - 1;
          continue;
        }
      if (!a5_is_pres_mark (c))
        return 0;
      n--;
    }
  return 0;
}

/* A faithful port of clsUserSession.bHasOutput (vb:1272), applied to a message
   string with its markup STILL EMBEDDED (the value the runner's Display() sees, before
   the final HTML->plain pass).  Unlike msg_has_output (which inspects the
   already-plain text), this runs the runner's actual test: it keeps a message when
   stripping all <...> tags leaves ANY character behind -- including a bare space.
   This is why Amazon's title music task `<audio play src="...">` followed by a
   space counts as output: StripCarats leaves " ", so the space joins onto the
   centred title (giving its leading indent).  When stripping leaves nothing, the runner
   keeps the message only if it carries a known formatting/media tag, or the whole
   string is an ALR (Text Override) trigger key. */
int
fd_has_output (a5_state_t *st, const char *raw)
{
  const char *p;
  int i;
  if (raw == nullptr || raw[0] == '\0')
    return 0;
  /* StripCarats: is there any character outside a <...> tag? */
  for (p = raw; *p; )
    {
      if (*p == '<')
        { while (*p && *p != '>') p++; if (*p == '>') p++; }
      else
        return 1;                       /* a non-tag char survives -> output */
    }
  /* All tags: the runner keeps it only for a recognised formatting/media tag. */
  {
    static const char *const known[] = {
      "<br>", "<center>", "<centre>", "<i>", "</i>", "<b>", "</b>", "<u>",
      "</u>", "<c>", "</c>", "<font", "</font>", "<right>", "</right>",
      "<left>", "</left>", "<del>", "<wait", "<cls>", "<img " };
    std::string low;
    for (p = raw; *p; p++)
      low += (char) tolower ((unsigned char) *p);
    for (size_t k = 0; k < sizeof known / sizeof known[0]; k++)
      if (low.find (known[k]) != std::string::npos)
        return 1;
  }
  /* An ALR may rewrite the whole tag-only string into something visible. */
  for (i = 0; i < st->adv->n_alrs; i++)
    if (st->adv->alrs[i].old_text != nullptr
        && strcmp (st->adv->alrs[i].old_text, raw) == 0)
      return 1;
  return 0;
}

/* The live run, for the a5expr event-property hook (event timers live here, not
   in a5_state).  Set in a5run_new; single-run headless harness, so a bare static
   is sufficient (mirrors the single global a5sexpr_rng_hook). */
static a5_run_t *g_event_hook_run = nullptr;

/* a5expr_event_prop_hook: Event.Position -> TimerFromStartOfEvent
   (Length.Value - TimerToEndOfEvent), Event.Length -> Length.Value. */
static int
a5run_event_prop (const char *event_key, const char *prop, long *out)
{
  a5_run_t *run = g_event_hook_run;
  if (run == nullptr || event_key == nullptr) return 0;
  for (int i = 0; i < run->adv->n_events; i++)
    if (streq (run->adv->events[i].key, event_key))
      {
        const a5_event_rt &rt = (*run->events)[i];
        if (streq (prop, "Position")) { *out = rt.length_value - rt.timer_to_end; return 1; }
        if (streq (prop, "Length"))   { *out = rt.length_value; return 1; }
        return 0;
      }
  return 0;
}

a5_state_t *
a5run_state (a5_run_t *run) { return run->st; }

int
a5run_is_over (a5_run_t *run) { return run->st->game_over; }

/* ----------------------------------------------------- status-line accessors */

/* The player's current location short description (room NAME) as plain text,
   caller frees; NULL if the location is unknown.  This one goes straight to
   the host's status line, so the presentation sentinels come out here: the
   turn loop strips them from story text at finish_turn, and nothing strips
   them from a room name. */
char *
a5run_location_name (a5_run_t *run)
{
  const char *loc = a5state_player_location (run->st);
  char *name;

  if (loc == nullptr)
    return nullptr;
  name = a5text_location_short_plain (run->st, loc);
  a5text_strip_pres_marks (name);
  return name;
}

/* Adventure.Score / MaxScore (the runner reads htblVariables("Score"/"MaxScore"); see
   emit_endgame).  0 when the game defines no such variable. */
long
a5run_score (a5_run_t *run)
{
  int i = a5state_variable_index (run->st, "Score");
  return i >= 0 ? run->st->var_num[i] : 0;
}

long
a5run_maxscore (a5_run_t *run)
{
  int i = a5state_variable_index (run->st, "MaxScore");
  return i >= 0 ? run->st->var_num[i] : 0;
}

int
a5run_turns (a5_run_t *run) { return run->st->turns; }

/* ------------------------------------------------------------- media capture */

static a5run_media_resolver_t a5run_media_resolver = nullptr;
static void *a5run_media_resolver_ctx = nullptr;

void
a5run_set_media_resolver (a5run_media_resolver_t resolver, void *ctx)
{
  a5run_media_resolver = resolver;
  a5run_media_resolver_ctx = ctx;
}

/* a5text media sink: resolve the src path to a Blorb resource number (via the
   game's <FileMappings>, then any host resolver) and record the event on the
   run's per-turn list.
   For an image, returns the resolved number so the renderer can leave a
   positional image mark in interactive mode (see A5_IMG_MARK); for a sound,
   the recorded event's list index, which the renderer likewise leaves as a
   positional A5_SOUND_MARK so the host can fire the event where the tag sits
   (before a later <waitkey>) and flag it shown (a5run_media_note_shown). */
static int
a5run_media_cb (void *ctx, int kind, const char *src, int channel, int loop)
{
  a5_run_t *run = (a5_run_t *) ctx;
  a5_media_event_t ev;
  ev.kind = kind;
  ev.channel = channel;
  ev.loop = loop;
  ev.number = (src != nullptr) ? a5model_resource_for_file (run->adv, src) : -1;
  if (src != nullptr && a5run_media_resolver != nullptr)
    ev.number = a5run_media_resolver (a5run_media_resolver_ctx, src,
                                      kind == A5_MEDIA_IMAGE, ev.number);
  ev.shown = 0;
  /* A description may be rendered more than once internally during a turn; drop
     an image already recorded this turn so it is not shown twice.  Unresolved
     images all share number -1, so those are the same image only if their
     srcs match. */
  if (kind == A5_MEDIA_IMAGE)
    for (size_t i = 0; i < run->media->size (); i++)
      if ((*run->media)[i].kind == A5_MEDIA_IMAGE
          && (*run->media)[i].number == ev.number
          && (ev.number > 0 || (*run->media_src)[i] == (src != nullptr ? src : "")))
        return ev.number;
  run->media->push_back (ev);
  run->media_src->push_back (src != nullptr ? src : "");
  return kind == A5_MEDIA_IMAGE ? ev.number
                                : (int) run->media->size () - 1;
}

/* Install/remove the media sink around a turn's text generation. */
static void
a5run_media_begin (a5_run_t *run)
{
  run->media->clear ();
  run->media_src->clear ();
  a5text_set_media_sink (a5run_media_cb, run);
}

static void
a5run_media_end (void)
{
  a5text_set_media_sink (nullptr, nullptr);
}

int
a5run_media_count (a5_run_t *run) { return (int) run->media->size (); }

const a5_media_event_t *
a5run_media_get (a5_run_t *run, int i)
{
  if (i < 0 || (size_t) i >= run->media->size ())
    return nullptr;
  return &(*run->media)[i];
}

const char *
a5run_media_src (a5_run_t *run, int i)
{
  if (i < 0 || (size_t) i >= run->media_src->size ())
    return nullptr;
  return (*run->media_src)[i].c_str ();
}

void
a5run_media_note_shown (a5_run_t *run, int i)
{
  if (i >= 0 && (size_t) i < run->media->size ())
    (*run->media)[i].shown = 1;
}

a5_run_t *
a5run_new (const a5_adventure_t *adv)
{
  a5_run_t *run;
  int i;
  if (adv == nullptr)
    return nullptr;
  /* Deterministic RNG sequence per game (seed 1234), like the other Scarier
     headless engines; routes RAND through the shared erkyrath_random.  When the
     v5 path is wired into Spatterlight this should honour the determinism /
     native-seed toggle, as scutils.cpp does for the v4 engine. */
  a5rand_seed (1234u);
  /* Let embedded <#...#> expressions (OneOf/Either/Rand) draw from the same RNG
     stream as the rest of the engine -- a5sexpr is otherwise RNG-less. */
  a5sexpr_rng_hook = a5rand_between;
  a5sexpr_urand_hook = a5rand_norepeat;
  /* Let `Event.Position`/`Event.Length` OO-expressions read the live event
     timers (e.g. JacarandaJim's rain `<# If(RAND(8)=1, ...) #>` gated on
     `Event12.Position > 0`). */
  a5expr_event_prop_hook = a5run_event_prop;
  /* Install the game's localized direction synonyms (the localization
     subsystem); English when the <Direction*> fields are absent.  Resets the
     parser's direction table, so every game starts from a known state. */
  a5parse_set_directions (adv->dir_re);
  run = new a5_run_s;
  run->adv = adv;
  run->st = a5state_new (adv);
  run->media = new std::vector<a5_media_event_t>;
  run->media_src = new std::vector<std::string>;
  run->order = new std::vector<int>;
  run->events = new std::vector<a5_event_rt> (adv->n_events);
  run->events_running = 0;
  run->event_out = nullptr;
  for (i = 0; i < adv->n_events; i++)
    {
      a5_event_rt &rt = (*run->events)[i];
      rt.status = A5_EV_NOTYET;
      rt.length_value = 0;
      rt.length_set = 0;
      rt.timer_to_end = 0;
      rt.last_se_time = 0;
      rt.last_se_index = -1;
      rt.just_started = 0;
      rt.next_command = A5_CMD_NONE;
      rt.when_start = adv->events[i].when_start;
      rt.se_ft.assign ((size_t) adv->events[i].n_subevents, 0);
    }
  /* Let the location viewer ask "is event #i Running?" for the SetLook gate
     (clsEvent.LookText's Status = Running check). */
  run->st->ev_running = [] (void *ctx, int ei) -> int
    {
      a5_run_t *r = (a5_run_t *) ctx;
      return ei >= 0 && ei < (int) r->events->size ()
             && (*r->events)[ei].status == A5_EV_RUNNING;
    };
  run->st->ev_running_ctx = run;
  /* Flatten every character's walks into one runtime list (clsWalk instances).
     Sub-walk offsets resolve once here (clsWalk never resets them); step
     durations re-roll on each lStart. */
  run->walks = new std::vector<a5_walk_rt>;
  for (i = 0; i < adv->n_characters; i++)
    for (int w = 0; w < adv->characters[i].n_walks; w++)
      {
        const a5_walk_t *wk = &adv->characters[i].walks[w];
        a5_walk_rt rt;
        rt.walk = wk;
        rt.char_index = i;
        rt.status = A5_EV_NOTYET;
        rt.length = rt.timer_to_end = rt.last_sw_time = 0;
        rt.last_sw_index = -1;
        rt.just_started = 0;
        rt.next_command = A5_CMD_NONE;
        rt.step_dur.assign ((size_t) wk->n_steps, 0);
        rt.sw_ft.assign ((size_t) wk->n_subwalks, 0);
        rt.came_across.assign ((size_t) wk->n_subwalks, 0);
        for (int s = 0; s < wk->n_subwalks; s++)
          rt.sw_ft[s] = a5rand_between (wk->subwalks[s].ft_from,
                                        wk->subwalks[s].ft_to);
        run->walks->push_back (rt);
      }
  run->amb_active = 0;
  run->amb_task_index = run->amb_command_index = -1;
  run->amb_ref_type = 'o';
  run->known_words = nullptr;
  run->resp = nullptr;
  run->ev_seen = nullptr;
  run->exec_scope = nullptr;
  run->comp_defers = nullptr;
  run->display_defers = new std::vector<std::string>;
  run->defer_look = 0;
  run->look_pending = 0;
  run->look_pos = 0;
  run->task_raw_output = 0;
  run->in_ev_attempt = 0;
  run->ev_tbl = nullptr;
  run->in_time_tick = 0;
  run->real_time = 0;
  run->cur_score_ti = -1;
  run->look_pinned = nullptr;
  run->pending_failover = nullptr;   /* not value-initialized by `new a5_run_s` */
  run->tasks_to_run = new std::vector<std::string>;
  for (i = 0; i < adv->n_tasks; i++)
    run->order->push_back (i);
  /* TaskSorter: ascending Priority (lower value checked first). */
  std::stable_sort (run->order->begin (), run->order->end (),
                    [adv](int a, int b) {
                      return adv->tasks[a].priority < adv->tasks[b].priority;
                    });
  g_event_hook_run = run;
  return run;
}

void
a5run_free (a5_run_t *run)
{
  if (run == nullptr)
    return;
  a5state_free (run->st);
  free (run->look_pinned);
  delete run->media;
  delete run->media_src;
  delete run->order;
  delete run->events;
  delete run->walks;
  delete run->known_words;
  delete run->tasks_to_run;
  delete run->display_defers;
  delete run->dom_nodes;
  delete run->dom_index;
  delete run;
}

/* ------------------------------------------------------------ tokenisation */

std::vector<std::string>
split_ws (const char *s)
{
  std::vector<std::string> v;
  std::string cur;
  for (const char *p = s ? s : ""; ; p++)
    {
      if (*p == '\0' || isspace ((unsigned char) *p))
        { if (!cur.empty ()) { v.push_back (cur); cur.clear (); } if (*p == '\0') break; }
      else
        cur += *p;
    }
  return v;
}

/* The lowercased, whitespace-trimmed input line (clsUserSession lowercases
   sInput up front). */
static std::string
normalise_input (const char *line)
{
  return trim_ws (lower (line ? line : ""));
}

/* ------------------------------------------------------ disambiguation prompt */

/* An entity's definite display name ("the Yellow Note" / a character's name). */
static std::string
entity_def_name (a5_state_t *st, const std::string &key, char type)
{
  if (type == 'o')
    {
      int oi = a5state_object_index (st, key.c_str ());
      if (oi >= 0)
        { char *n = a5text_object_name (st, &st->adv->objects[oi], A5_ART_DEFINITE);
          std::string s = n ? n : ""; free (n); return s; }
    }
  else
    {
      int ci = a5state_character_index (st, key.c_str ());
      if (ci >= 0 && st->adv->characters[ci].name)
        return st->adv->characters[ci].name;
    }
  return key;
}

/* "Which <word>?  The a, the b or the c."  Mirrors DisplayAmbiguityQuestion:
   the candidate list uses definite names joined ", "/" or ", run through ToProper
   with its default bStrict=False (capitalise only the first letter, leave the
   rest as-is so e.g. "PC door" keeps its casing), with pSpace's two spaces. */
static std::string
build_amb_prompt (a5_state_t *st, const std::string &word,
                  const std::vector<std::string> &keys, char type)
{
  std::string list;
  for (size_t i = 0; i < keys.size (); i++)
    {
      if (i > 0)
        list += (i + 1 == keys.size ()) ? " or " : ", ";
      list += entity_def_name (st, keys[i], type);
    }
  /* ToProper(bStrict=False): capitalise only the first character; the remainder
     keeps its original casing (clsUserSession.ToProper, the Adrift 5 runner). */
  if (!list.empty ()) list[0] = (char) toupper ((unsigned char) list[0]);
  return "Which " + word + "?  " + list + ".";
}

/* Re-run the remembered task with its ambiguous reference pinned to `chosen`;
   returns 1 if it ran (RR_OK). */
static int
run_remembered (a5_run_t *run, const char *chosen, sb_t *out)
{
  a5_state_t *st = run->st;
  if (run->amb_task_index < 0)
    return 0;
  const a5_task_t *t = &run->adv->tasks[run->amb_task_index];
  a5_match_t m;
  int resolved = 0;
  for (int vi = 0; !resolved; vi++)
    {
      int mr = a5parse_match_command_v (t->commands[run->amb_command_index],
                                        run->amb_input.c_str (), &m, vi);
      if (mr < 0)
        return 0;
      if (mr == 0)
        continue;
      if (resolve_refine (run, t, &m, run->amb_ref_name.c_str (), chosen, nullptr)
          == RR_OK)
        resolved = 1;
    }
  run_general (run, t, &m, out);
  st->task_done[run->amb_task_index] = 1;
  ev_on_task_completed (run, t->key, out);
  return 1;
}

/* Emit the "You can't see any <plural>!" message for an out-of-scope object
   reference (clsUserSession.DisplayAmbiguityQuestion, bCanSeeAny=False). */
static void
emit_cantsee (a5_state_t *st, const amb_info *amb, sb_t *out)
{
  std::string w = amb_word (st, amb->keys, amb->ref_text, amb->type);
  /* Characters use the bare AmbWord (DisplayAmbiguityQuestion does not
     pluralise the character case). */
  if (amb->type == 'c')
    {
      sb_puts (out, "You can't see any ");
      sb_puts (out, w.c_str ());
      sb_puts (out, "!");
      return;
    }
  /* clsObject.IsPlural == (Article == "some"): a mass/plural object
     ("some firewood") keeps the bare noun; others are pluralised. */
  int any_plural = 0;
  for (auto &k : amb->keys)
    {
      int ki = a5state_object_index (st, k.c_str ());
      if (ki >= 0 && streq (st->adv->objects[ki].article, "some"))
        { any_plural = 1; break; }
    }
  std::string pl = any_plural ? w : guess_plural_from_noun (w);
  sb_puts (out, "You can't see any ");
  sb_puts (out, pl.c_str ());
  sb_puts (out, "!");
}

/* Re-match (ti,ci) against `in` with the wildcard-variant order scan_tasks
   used, restoring the binding state of the variant it surfaced: the first
   variant with a real resolution, else the first NOREF variant. */
static int
rematch_resolve (a5_run_t *run, int ti, int ci, const std::string &in,
                 a5_match_t *m)
{
  const a5_task_t *t = &run->adv->tasks[ti];
  int r = RR_NOMATCH;
  a5_match_t noref_m;
  int have_noref = 0;
  for (int vi = 0; ; vi++)
    {
      int mr = a5parse_match_command_v (t->commands[ci], in.c_str (), m, vi);
      if (mr < 0)
        break;
      if (mr == 0)
        continue;
      r = resolve_refine (run, t, m, nullptr, nullptr, nullptr);
      if (r == RR_NOREF && !have_noref)
        { noref_m = *m; have_noref = 1; }
      if (r != RR_NOMATCH && r != RR_NOREF)
        return 1;
    }
  if (have_noref)
    {
      *m = noref_m;
      resolve_refine (run, t, m, nullptr, nullptr, nullptr);
      return 1;
    }
  return r != RR_NOMATCH;
}

/* Run the deferred sNoRefTask (clsUserSession.GetGeneralTask, GetGeneralTask =
   sNoRefTask): re-bind the resolvable references and surface the task's
   restriction message for the unresolved one ("Sorry, I'm not sure which object
   you are trying to <verb>.").  Returns 1 if it produced output.  With a NULL
   `out` this is the pure predicate the runner's `sRestrictionText <> ""` gate
   uses (would the task fail *with output*?), letting a higher-priority
   no-reference task override a recorded one under HighestPriorityPassingTask --
   so an examine task whose noun is unknown keeps its "You see no such thing."
   rather than yielding to a higher refless task that fails silently. */
static int
run_noref (a5_run_t *run, int ti, int ci, const std::string &in, sb_t *out)
{
  a5_state_t *st = run->st;
  const a5_task_t *t = &run->adv->tasks[ti];
  a5_match_t m;
  if (!rematch_resolve (run, ti, ci, in, &m))     /* binds the resolvable refs */
    return 0;
  const a5_xml_node_t *fm = a5restr_fail_message (st, t->restrictions);
  if (fm == nullptr)
    return 0;
  /* bDisplaying only when this render is the one the player sees.  With
     out == NULL the caller is the HighestPriorityPassingTask predictor, which
     in the runner is a bare `sRestrictionText <> ""` test on the RAW template
     (vb:6076) -- no expansion happens there at all, so a discarded render must
     not mark characters Introduced (cf. the CharHereDesc probe that used to
     mark Anno 1700's Susan a turn early). */
  char *fmsg;
  { a5_intro_guard ig (st, out != nullptr);
    fmsg = a5text_describe (st, fm); }
  int has = fmsg[0] != '\0';
  if (has && out != nullptr)
    sb_puts (out, fmsg);
  free (fmsg);
  return has;
}

/*
 * One scan_tasks() pass over the general tasks: the input, where the output
 * and the recorded candidates go, and how far a ContinueAlways task or a
 * failing-with-output task has narrowed the tasks still eligible.
 */
struct task_scan_t {
  a5_run_t *run;
  const std::string &in;
  sb_t *out;
  int *have_amb;
  amb_info *amb;
  int *amb_ti, *amb_ci;
  int *amb_cantsee;
  int *have_fail;
  std::string *fail_text;
  int *have_noref;
  int *noref_ti, *noref_ci;
  a5_state_t *st;

  int fail_priority;
  /* When a <Continue>ContinueAlways task runs, the Adrift 5 runner continues task
     selection via EvaluateInput(Priority+1) -> GetGeneralTask with
     iMinimumPriority = iPriorityFail = that task's Priority + 1.  So on the
     continuation only *strictly higher* priority tasks are eligible, and a
     LowPriority task is skipped unless its priority equals the floor (the
     `Not (LowPriority AndAlso Priority > iPriorityFail)` guard,
     clsUserSession.vb:5981).  E.g. RunBronwynn's GetOffBeforeMoving (50432,
     ContinueAlways) prints "...get off the bed first?" and the LowPriority
     PlayerMovement (50434) is then NOT run, so the player stays put. */
  int cont_active;
  long cont_floor;        /* = the continue task's Priority (min = floor+1) */

  void leak_text_references (const a5_match_t &m);
  int match_command (const a5_task_t *t, int ci, a5_match_t &m,
                     amb_info &this_amb, bool *aborted);
  bool run_passing (int ti, const a5_task_t *t, a5_match_t &m);
  void record_noref (int ti, int ci);
  bool record_failure (const a5_task_t *t);
  int scan ();
};

void
task_scan_t::leak_text_references (const a5_match_t &m)
{
  /* clsUserSession.vb:2567: a command-matched candidate's %textN%
     captures overwrite the turn-global sReferencedText slots as soon
     as its references are processed -- BEFORE (and regardless of) its
     reference resolution or restriction outcome, so a later task's
     `ReferencedText Must ...` restriction sees the leakage. */
  for (int mi = 0; mi < m.n_refs; mi++)
    if (strncmp (m.ref_name[mi], "text", 4) == 0)
      {
        int slot = 0;
        if (isdigit ((unsigned char) m.ref_name[mi][4]))
          slot = atoi (m.ref_name[mi] + 4) - 1;
        if (slot < 0) slot = 0;
        if (slot > 4) slot = 4;
        strncpy (st->scan_text[slot], m.ref_text[mi],
                 sizeof st->scan_text[slot] - 1);
        st->scan_text[slot][sizeof st->scan_text[slot] - 1] = '\0';
      }
}

/*
 * Match the input against command CI of T and resolve its references into M,
 * returning the RR_ code (THIS_AMB filled for an ambiguity).  Sets *ABORTED
 * when the match kills the whole scan.
 */
int
task_scan_t::match_command (const a5_task_t *t, int ci, a5_match_t &m,
                            amb_info &this_amb, bool *aborted)
{
  /* Wildcard variants (GetRegularExpression): try the command's
     candidate regexes in order -- all wildcards removed first, the
     original pattern last -- advancing to the next variant when the
     matched variant's reference text names nothing (InputMatchesObject
     failure -> DoesntMatch -> next regex).  `find bell tower entrance`
     must bind the whole tail to %object% (the unseen Bell Tower door)
     before falling back to the lazy "%object% *" split ("bell").  The
     first variant that yields a real resolution wins; a NOREF (second-
     chance Must-Exist) result keeps the FIRST such variant's reference
     text, as the runner's second-chance pass re-matches regex list in order. */
  int r = RR_NOMATCH;
  a5_match_t noref_m;
  int have_noref_var = 0;
  for (int vi = 0; ; vi++)
    {
      int mr = a5parse_match_command_v (t->commands[ci], in.c_str (),
                                        &m, vi);
      if (mr < 0)
        break;
      if (mr == 0)
        continue;
      /* FAITHFUL BUG: a typed command matching a task with a plural
         %characters% reference kills the WHOLE scan.  %characters% is
         missing from Global.ReferenceNames() (Global.vb:473), so the
         task's References list never counts it and NewReferences is
         ReDim'd too short (clsUserSession.vb:275); InputMatchesCommand's
         "characters" group handler then assigns past the end
         (vb:5630) and the IndexOutOfRangeException unwinds into
         GetGeneralTask's Catch (vb:6118), which returns Nothing --
         discarding every candidate recorded so far AND skipping the
         second-chance pass.  The input falls through to NotUnderstood
         ("I don't understand what you want to do with Bob.").
         (SetTasks-Execute parameter binds bypass this: they never
         re-enter InputMatchesCommand.)  Simplification: the runner
         only throws once the groups BEFORE "characters" all resolved;
         a mixed command whose earlier %object% names nothing would
         DoesntMatch first -- not mirrored, the abort here fires on any
         regex match. */
      for (int mi = 0; mi < m.n_refs; mi++)
        if (strcmp (m.ref_name[mi], "characters") == 0)
          {
            *have_amb = 0; *have_fail = 0; *have_noref = 0;
            fail_text->clear ();
            /* On a ContinueAlways continuation the claiming task
               already ran; the aborted continuation still reports the
               turn handled (NotUnderstood is gated on
               iMinimumPriority=0, see the cont_active return in scan ()). */
            *aborted = true;
            return RR_NOMATCH;
          }
      leak_text_references (m);
      r = resolve_refine (run, t, &m, nullptr, nullptr, &this_amb);
      if (r == RR_NOREF && !have_noref_var)
        { noref_m = m; have_noref_var = 1; }
      if (r != RR_NOMATCH && r != RR_NOREF)
        break;
    }
  if ((r == RR_NOMATCH || r == RR_NOREF) && have_noref_var)
    {
      /* No variant resolved; surface the first NOREF variant (its
         bindings may have been clobbered by later variants' probes --
         re-resolve to restore them and the amb info). */
      m = noref_m;
      r = resolve_refine (run, t, &m, nullptr, nullptr, &this_amb);
    }
  return r;
}

/*
 * T matched uniquely and passes: run it.  Return true if it claims the turn,
 * false if the scan continues at higher priorities.
 */
bool
task_scan_t::run_passing (int ti, const a5_task_t *t, a5_match_t &m)
{
  size_t out_before = out->len;
  run_general (run, t, &m, out);
  int produced = (out->len > out_before);
  st->task_done[ti] = 1;
  ev_on_task_completed (run, t->key, out);
  /* <Continue>ContinueAlways: run lower-priority matching tasks too
     (clsUserSession GetGeneralTask -> EvaluateInput(Priority+1)).
     E.g. GetOffBeforeMoving prints "(getting off X first)" then lets
     the actual movement task run.

     Also continue when the task PASSED but produced NO OUTPUT:
     the Adrift 5 runner's AttemptToExecuteTask (clsUserSession.vb:916-921)
     sets bContinue and re-evaluates at Priority+1 for a passing task
     with no output, so a higher-priority matching task that DOES emit
     (or fails with output) claims the turn instead.  E.g. Axe of Kolt's
     generic "Buy Object" (Task54, no completion message) passes for
     `buy beer` but must yield to the higher-priority "Buy Beer In
     Tavern" task that emits "...run out of change" and sets the
     change-needed flag.  If nothing higher claims, the cont_active
     fall-through below still reports the turn handled (the silent task
     already ran its actions). */
  if (t->continue_lower || !produced)
    {
      /* If that passing task also ended the game (an EndGame action
         with no completion message, e.g. FoF's silent Task1448
         "Pull Bell Ropes - Door Wedged": IncScore + Execute + EndGame
         Win), the Adrift 5 runner's Priority+1 re-entry into EvaluateInput
         hits its top guard with eGameState != Running: SystemTasks(True)
         rejects the command (not restart/restore/quit/undo) and it
         prints "Please give one of the answers above." before returning
         (clsUserSession.vb:3372-3379).  The win/lose banner then follows
         from CheckEndOfGame (emit_endgame here).  Emit that line and
         claim the turn -- the re-entry scans no further task. */
      if (st->game_over)
        {
          sb_puts (out, "Please give one of the answers above.\n");
          return true;
        }
      /* EvaluateInput(Priority+1): only strictly-higher-priority
         tasks remain eligible, with the LowPriority/iPriorityFail
         guard.  A second ContinueAlways task raises the floor. */
      if (!cont_active || t->priority > cont_floor)
        { cont_active = 1; cont_floor = t->priority; }
      /* The claiming task supersedes every candidate recorded so
         far: the runner's continuation is a FRESH EvaluateInput whose
         GetGeneralTask starts with empty sAmbTask/sNoRefTask/
         sRestrictionText -- a pre-continue "which?"/noref/fail
         message must not resurface if the continuation itself
         finds nothing.  (Candidates recorded AFTER this point are
         the continuation's own and DO surface -- see the
         cont_active return below.) */
      *have_fail = 0; fail_text->clear ();
      *have_amb = 0; *have_noref = 0;
      return false;   /* stop trying this task's commands; keep scanning */
    }
  return true;
}

void
task_scan_t::record_noref (int ti, int ci)
{
  /* A reference named nothing: remember this task as the
     no-reference fallback (clsUserSession.GetGeneralTask sNoRefTask).
     Keep scanning -- a later task may resolve uniquely or fail with
     output; only if nothing claims does the caller run this task
     with the reference unresolved.

     These are the runner's second-chance pass tasks (matched only via
     HasObjectExistRestriction, with zero-Item references): there they
     fail their `Must Exist` restriction *with output*, so the same
     TaskExecution ordering applies.  Under HighestPriorityPassingTask
     the highest-priority such task that fails *with output* wins
     (clsUserSession.vb:6076, gated on `sRestrictionText <> ""`) --
     run->order is ascending, so overwrite each time a higher task
     would produce a message; under the default HighestPriorityTask the
     first (lowest-priority) one is kept.  E.g. Spectre's `remove
     bricks` matches both RemoveObjects (P50736, "...you're referring
     to.") and RemoveObje (P50749, "...trying to remove."); The runner surfaces
     the latter.  The output gate keeps Axe of Kolt's `examine
     <unknown noun>` on its "You see no such thing." rather than
     yielding to a higher refless task that fails silently. */
  if (!*have_noref)
    { *have_noref = 1; *noref_ti = ti; *noref_ci = ci; }
  else if (st->adv->hp_passing
           && run_noref (run, ti, ci, in, nullptr))
    { *noref_ti = ti; *noref_ci = ci; }
}

/*
 * T matched the command but its restrictions fail.  Return true if that
 * claims the turn outright; otherwise record any fail message as a candidate.
 */
bool
task_scan_t::record_failure (const a5_task_t *t)
{
  /* A "get all"-style command whose %objects% resolved to no passing
     item: the task's FailOverride claims the turn ("There is nothing
     worth taking here.") -- clsUserSession.vb:788, shown when no
     response passed and the input contained "all".  vb:789 is a
     literal Display(task.FailOverride.ToString) over a raw
     clsDescription, so the expansion happens with bDisplaying set. */
  if (run->pending_failover != nullptr)
    {
      char *fo;
      { a5_intro_guard ig (st, 1);
        fo = a5text_describe (st, run->pending_failover); }
      run->pending_failover = nullptr;
      if (msg_has_output (fo)) { sb_pspace (out); sb_puts (out, fo); }
      free (fo);
      return true;
    }
  /* The task matched the command but its restrictions fail.  If a
     failing restriction has output, this is a candidate fail message
     (clsUserSession.GetGeneralTask: sRestrictionText = the failing
     restriction's Message; refless and reference-bearing tasks are
     treated identically -- htblCompleteableGeneralTasks has no
     HasReferences/IsCompleteable filter; that lives only in the
     background-thread predictor).  The TaskExecution mode decides
     whether it claims the turn now:
       - HighestPriorityTask (the v5 default): the highest-priority
         command-matching task that fails *with output* claims the
         turn -- show its message, no lower task runs.
       - HighestPriorityPassingTask: it does NOT claim; record the
         first (highest-priority) such message and keep scanning for
         a lower-priority *passing* task, which overrides it.  Six
         Silver Bullets sets this, so its refless "LOOK" peephole
         task falls through to the real Look. */
  const a5_xml_node_t *fm = a5restr_fail_message (st, t->restrictions);
  if (fm == nullptr)
    /* The task's own restrictions named no failing message (e.g. a
       malformed BracketSequence whose evaluation calls no single
       restriction).  The Adrift 5 runner then still has sRestrictionText
       set to the *previous* command's leftover message, so the task
       fails *with output* and claims the turn -- ticking
       TurnBasedStuff.  Anno 1700's reference-free OpeningHid ("##A#")
       matches "open closet", fails, and inherits the prior turn's
       "%CharacterName% can't see %TheObject[%object%]%." (rendered
       with its empty references as "You can't see nothing."). */
    fm = st->restriction_text;
  if (fm != nullptr)
    {
      /* Raw into AddResponse (vb:1247): expanded inside Display. */
      char *fmsg;
      { a5_intro_guard ig (st, 1);
        fmsg = a5text_describe (st, fm); }
      /* Prefix a failing multi-match %objects% noun with the runner's
         "Sorry, I'm not sure which object ..." ambiguity message
         (set by resolve_plural).  Skip when the failure message is
         already that text (a never-seen match fails the Exist /
         HaveBeenSeen restriction, whose message IS the prefix). */
      std::string fcombined = fmsg;
      if (fmsg[0] && !run->plural_amb_prefix.empty ()
          && strncmp (fmsg, run->plural_amb_prefix.c_str (),
                      run->plural_amb_prefix.size ()) != 0)
        fcombined = run->plural_amb_prefix + "  " + fmsg;
      if (fmsg[0])
        {
          /* Record the first (lowest-Priority, since run->order is
             ascending) failing-with-output task and keep scanning.
             Under HighestPriorityPassingTask any later passing task
             (any priority) overrides it; under the default
             HighestPriorityTask only an equal-priority passing task
             does (enforced by the loop-top guard, which stops once a
             strictly lower priority is reached).

             Under HighestPriorityPassingTask the runner reassigns
             GetGeneralTask for *every* failing-with-output task
             (clsUserSession.vb:6076), so the *highest*-priority such
             task wins -- run->order is ascending, so overwrite each
             time.  The LowPriority/iPriorityFail guard at the loop top
             (above) already excludes the LowPriority tasks the runner skips
             once a fail floor is set, which keeps Six Silver Bullets'
             turn timer aligned (a bare keep-highest without that guard
             ticked early).  Under the default HighestPriorityTask the
             loop-top guard stops the scan once a strictly lower
             priority is reached, so keeping the first within the band
             is correct there. */
          if (!*have_fail || st->adv->hp_passing)
            {
              *have_fail = 1;
              *fail_text = fcombined;
              fail_priority = t->priority;
            }
        }
      free (fmsg);
    }
  return false;
}

int
task_scan_t::scan ()
{
  for (size_t oi = 0; oi < run->order->size (); oi++)
    {
      int ti = (*run->order)[oi];
      const a5_task_t *t = &run->adv->tasks[ti];
      if (!streq (t->type, "General"))
        continue;
      if (st->task_done[ti] && !t->repeatable)
        continue;
      if (cont_active)
        {
          if (t->priority <= cont_floor)              /* below iMinimumPriority */
            continue;
          if (t->low_priority && t->priority > cont_floor + 1) /* iPriorityFail */
            continue;
        }

      /* HighestPriorityTask mode: a recorded failing-with-output task claims the
         turn over any *lower*-priority task (higher Priority value).  But an
         *equal*-priority task that passes still overrides it -- the Adrift 5 runner's
         GetGeneralTask reaches such a passing task first when its (unstable)
         priority sort happens to order it ahead of the failing one (e.g. the
         library "say %text%" pair: the "Say" fallback needs an active
         conversation and fails with a message, while the equal-priority
         "SayLazy" passes when alone with a character and prints "(to <char>)").
         So keep scanning within the same priority band, but stop the moment we
         descend to a strictly lower priority: emit the recorded message. */
      if (*have_fail && !st->adv->hp_passing && t->priority > fail_priority)
        return 0;

      /* HighestPriorityPassingTask: once a task fails *with output*, the runner sets
         iPriorityFail to its Priority, so the loop-top guard
         `Not (LowPriority AndAlso Priority > iPriorityFail)`
         (clsUserSession.vb:5981) then skips any LowPriority task above that
         floor.  This is what keeps Six Silver Bullets' turn timer aligned when
         the highest-priority failing task wins (below): a LowPriority failing
         task above the floor must not be recorded (and so claim/tick the turn)
         when the runner would have skipped it outright. */
      if (st->adv->hp_passing && *have_fail
          && t->low_priority && t->priority > fail_priority)
        continue;

      for (int ci = 0; ci < t->n_commands; ci++)
        {
          a5_match_t m;
          amb_info this_amb;
          bool aborted = false;
          int r = match_command (t, ci, m, this_amb, &aborted);
          if (aborted)
            return cont_active ? 1 : 0;
          if (r == RR_NOMATCH)
            continue;
          if (a5run_trace)
            fprintf (stderr, "[match task %s cmd \"%s\" -> %d]\n",
                     t->key, t->commands[ci], r);
          if (r == RR_OK)
            {
              if (run_passing (ti, t, m))
                return 1;
              break;
            }
          if (r == RR_CANTSEE
              && (!*have_amb || (amb->second_chance && !this_amb.second_chance)))
            {
              /* "You can't see any <plural>!" -- an out-of-scope object
                 reference.  Like a "which?" ambiguity (sAmbTask), this does NOT
                 claim the turn: a lower-priority task that uniquely resolves or
                 fails its restrictions *with output* (e.g. ExamineCharacter
                 resolving the same noun to an NPC and reporting "You can't see
                 the woman!") overrides it.  Mirrors GetGeneralTask, where
                 sAmbTask is shown only when no later task claims sTaskKey.  Record
                 the first such reference and keep scanning.  A genuine first-pass
                 ambiguity replaces an earlier *second-chance* one (the runner finds all
                 first-pass results before the second-chance pass). */
              *have_amb = 1; *amb_cantsee = 1; *amb = this_amb;
              *amb_ti = ti; *amb_ci = ci;
              continue;
            }
          if (r == RR_NOREF)
            {
              record_noref (ti, ci);
              continue;
            }
          if (r == RR_AMBIG
              && (!*have_amb || (amb->second_chance && !this_amb.second_chance)))
            { *have_amb = 1; *amb_cantsee = 0; *amb = this_amb;
              *amb_ti = ti; *amb_ci = ci; }
          if (r == RR_FAIL && record_failure (t))
            return 1;
          /* command matched but did not resolve to a unique pass: keep scanning */
        }
    }
  /* A <Continue>ContinueAlways (or passing-but-silent) task already ran and
     claimed this turn; the continuation (the runner's EvaluateInput at
     iMinimumPriority>0) is a full task-selection pass of its own, so a
     failing-with-output / ambiguity / noref candidate it recorded surfaces
     exactly as on a first pass (e.g. Stuck Piggy's empty `open clock` stub
     passes silently and the continuation must still show OpenObjects' "You
     can't open the old grandfather clock as it is locked!").  Only when the
     continuation found nothing at all is the difference: it must NOT print
     "I didn't understand" (that path is gated on iMinimumPriority=0), so
     report the turn as handled. */
  if (cont_active)
    return (*have_fail || *have_amb || *have_noref) ? 0 : 1;
  return 0;
}

/* Scan general tasks for `in`.  Runs (and returns 1) on the first unique passing
   match.  Otherwise returns 0, recording -- for the caller to act on after the
   scan -- the first ambiguity (via have_amb/amb/amb_ti/amb_ci; amb_cantsee
   distinguishes the "can't see any" out-of-scope case from a "which?" prompt)
   and the first restriction-failure message (via have_fail/fail_text). */
static int
scan_tasks (a5_run_t *run, const std::string &in, sb_t *out,
            int *have_amb, amb_info *amb, int *amb_ti, int *amb_ci,
            int *amb_cantsee,
            int *have_fail, std::string *fail_text,
            int *have_noref, int *noref_ti, int *noref_ci)
{
  task_scan_t scan = { run, in, out, have_amb, amb, amb_ti, amb_ci,
                       amb_cantsee, have_fail, fail_text, have_noref,
                       noref_ti, noref_ci, run->st, 0, 0, 0 };
  return scan.scan ();
}

/* --------------------------------------------------------------- public turn */

/* The "Would you like to restart, restore a saved game, quit or undo the last
   command?" prompt (CheckEndOfGame, clsUserSession.vb:533).  The runner
   re-Displays it on EVERY post-game command -- the prompt sits outside the
   bDisplayedWinLose gate -- so both the first CheckEndOfGame and each
   guard-rejected post-game input emit it.

   The runner: Display("Would you like to <c>restart</c>, <c>restore</c> a saved
   game, <c>quit</c> or <c>undo</c> the last command?").  The <c> tags sit
   in the buffer during ReplaceALRs, so an OldText can match any contiguous
   run BETWEEN tags (Halloween translates the prompt piecewise: "restart"
   -> "genstarte") but never ACROSS one (Shattered Memory's full-sentence
   blanking Override does not fire in the runner).  A5_ALR_MARK is Scarier's
   stripped-tag stand-in: emit one at each tag position. */
static void
emit_end_prompt (sb_t *out)
{
  static const char *kPromptSegs[] = {
    "Would you like to ", "restart", ", ", "restore", " a saved game, ",
    "quit", " or ", "undo", " the last command?", nullptr
  };
  for (int si = 0; kPromptSegs[si] != nullptr; si++)
    {
      if (si > 0)
        sb_putc (out, A5_ALR_MARK);
      sb_puts (out, kPromptSegs[si]);
    }
  sb_puts (out, "\n\n");
}

/* Append the end-of-game block (clsUserSession.CheckEndOfGame): the win/lose
   banner, the optional EndGameText (WinningText), the score line when MaxScore>0,
   and the restart prompt.  Emitted once, the first turn game_over is set, before
   `turns` is bumped for the ending command -- so the score shows the count of
   commands processed *before* this one (the runner displays during Process, ahead of its
   per-command Adventure.Turns increment). */
static void
emit_endgame (a5_run_t *run, sb_t *out)
{
  a5_state_t *st = run->st;
  const char *es = st->end_message;     /* "Win" / "Lose" / "Neutral" */
  long score, maxscore;

  /* The Adrift 5 runner's CheckEndOfGame Displays the banner through pSpace, so a
     paragraph break always separates it from the turn's preceding output.  A
     game that ends via a turn-based event (e.g. StarshipQuest's hyperspace
     death, MagneticMoon's "took too long" timer) emits its death text with a
     single trailing newline -- without this the banner would abut it, whereas a
     command-ending game's last response already ends in a blank line.  Top up
     the trailing newlines to a blank-line separator.

     Only newlines at the very end count: trailing blanks are text as far as the
     break is concerned, so a message whose last line is all spaces still needs
     the full break (Dreamspun's winning CLIMB DOWN TO EARTH response ends
     "...\n\n     ", and the runner puts the banner two lines below it). */
  if (out->len > 0 && !run->in_time_tick)
    {
      size_t k = out->len;
      int nl = 0;
      while (k > 0 && (out->p[k - 1] == '\n' || out->p[k - 1] == '\r'))
        {
          if (out->p[k - 1] == '\n') nl++;
          k--;
        }
      if (nl < 2)
        sb_puts (out, nl == 0 ? "\n\n" : "\n");
    }

  if (es != nullptr && streq (es, "Win"))
    sb_puts (out, "*** You have won ***\n");
  else if (es != nullptr && streq (es, "Lose"))
    sb_puts (out, "*** You have lost ***\n");
  /* Neutral (and an unset/unknown enum): no banner, like the runner. */

  if (run->adv->end_game_text != nullptr)
    {
      /* The runner's CheckEndOfGame Displays the win/lose banner with bCommit=True
         (clsUserSession.vb:515) BEFORE the WinningText (vb:528), so each is a
         separate Display commit and the headless renderer clears only within a
         commit (Program.cs EmitHtml's per-call StringBuilder).  A <cls> embedded
         in the EndGameText therefore wipes only the EndGameText's own commit, NOT
         the already-emitted banner and turn output.  Alien Diver's winning text
         opens with <cls>: resolve it here against a floor pinned right after the
         banner so it can't reach back and swallow the "*** You have won ***"
         line and the take-off narrative (finish_turn's later floor-0 pass then
         has no EndGameText <cls> left to over-wipe). */
      size_t after_banner = out->len;
      char *wt = a5text_describe (st, run->adv->end_game_text);
      if (wt[0]) { sb_puts (out, wt); sb_puts (out, "\n"); }
      free (wt);
      sb_resolve_cls (out, after_banner);
    }

  /* The runner reads Adventure.Score / MaxScore = htblVariables("Score"/"MaxScore"),
     which is keyed by the variable KEY, not its (user-facing) Name.  Magnetic
     Moon's score variable has key "Score" but Name "__Points_Scored", so a
     by-Name lookup misses it.  a5run_score / a5run_maxscore look up by key. */
  score = a5run_score (run);
  maxscore = a5run_maxscore (run);
  if (maxscore > 0)
    {
      char line[160];
      snprintf (line, sizeof line,
                "In that game you scored %ld out of a possible %ld, in %d turns.\n\n",
                score, maxscore, st->turns - 1);
      sb_puts (out, line);
    }

  /* The restart prompt is the Runner UI's modal question in the runner, not part of
     the CheckEndOfGame Display -- so a game ALR TextOverride must never
     rewrite it (Shattered Memory blanks the whole "Would you like to
     restart..." sentence with an Override meant for the post-restore replay).
     Interleave A5_ALR_MARK sentinels so the boundary ALR pass cannot match
     any OldText across it; finish_turn's existing mark strip removes them
     before the text is shown. */
  emit_end_prompt (out);
  st->end_displayed = 1;
}

/* Compact `s` in place, dropping every occurrence of the byte `c`. */
static void
strip_byte (char *s, char c)
{
  char *r, *w;
  if (s == nullptr)
    return;
  for (r = w = s; *r != '\0'; r++)
    if (*r != c) *w++ = *r;
  *w = '\0';
}

/* a5text_expand_var_defers over a heap string, replacing it in place. */
static void
expand_var_defers_owned (a5_state_t *st, char **s)
{
  char *xp = a5text_expand_var_defers (st, *s);
  free (*s);
  *s = xp;
}

/* Expand any deferred-variable sentinels (A5_VARDEF_MARK, a5text.h) currently
   in the turn buffer with the variables' CURRENT values.  Called at the
   Scarier equivalents of the runner's Display commits (see the claimed-command path)
   so an ALR-borne %scor% reads the state of ITS commit, not end-of-turn. */
static void
expand_var_defers_sb (a5_run_t *run, sb_t *out)
{
  if (out->p == nullptr || out->len == 0
      || memchr (out->p, A5_VARDEF_MARK, out->len) == nullptr)
    return;
  char *xp = a5text_expand_var_defers (run->st, out->p);
  out->len = 0;
  out->p[0] = '\0';
  sb_puts (out, xp);
  free (xp);
}

/* Finalise a turn: take the assembled output and run it through the Display()
   ALR boundary (clsUserSession.Display -> Global.ReplaceALRs), so the engine's
   stock literals are translated for a non-English game.  A no-op (bar a copy)
   when the game has no ALRs. */
static char *
finish_turn (a5_run_t *run, sb_t *out)
{
  char *raw, *fin;
  size_t n;
  /* The runner's Display loop: draw every AggregateOutput completion whose evaluation was
     held during the command, now that the LocationTrigger drain and event tick
     have run (their draws come first).  Splices each drawn value into its
     `\004<idx>\004` sentinel slot left in `out`. */
  a5run_flush_display_defers (run, out);
  if (run->st->game_over && !run->st->end_displayed)
    emit_endgame (run, out);
  /* Honour any <cls> relayed by the plain renderer (A5_CLS_MARK).  A command
     turn is a single the runner commit (its output accumulates in sOutputText and flushes
     once, clsUserSession.vb:3766), so a <cls> anywhere in the turn wipes the whole
     turn's prior output -- e.g. Pathway to Destruction's win narrative, whose
     embedded <cls> drops the move message, room view and "The End" banner.  (The
     multi-commit intro resolves its own segments before calling finish_turn, so
     no markers reach here from a5run_intro.) */
  sb_resolve_cls (out, 0);
  /* Same for a <del> the plain renderer could not satisfy inside its own
     message (A5_DEL_MARK): the runner's Display buffer spans the turn, so such
     a delete eats the pSpace join and then the previous message's tail.  After
     the <cls> pass, so a marker a <cls> wiped never fires. */
  sb_resolve_del (out);
  raw = sb_finish (out);
  /* The pSpace markers (A5_PS_MARK) have already done their job during buffer
     accumulation -- sb_pspace saw them as non-newline tails and inserted the join
     spaces.  Strip them now, before the ALR match and the trailing-whitespace
     trim, so they never reach the output. */
  strip_byte (raw, A5_PS_MARK);
  /* Deferred-variable sentinels (a bare %var% inside an eagerly-applied ALR
     NewText) resolve here with the end-of-turn value -- the runner Display's
     ReplaceFunctions runs before its ReplaceALRs (Global.vb:523).  A second
     pass after the boundary ALR catches sentinels a boundary-applied ALR
     itself introduced. */
  expand_var_defers_owned (run->st, &raw);
  fin = a5text_display_alr (run->st, raw);   /* may render ALR <img>/<audio> too */
  free (raw);
  expand_var_defers_owned (run->st, &fin);
  /* The stripped-tag sentinels (A5_ALR_MARK) have done their job: the boundary
     ALR pass above could not match an OldText across a stripped tag, exactly
     like the runner's Display-time ReplaceALRs over the still-marked-up buffer.  Drop
     them before the output is shown. */
  strip_byte (fin, A5_ALR_MARK);
  /* Normalise only the very end of the turn: the runner's pSpace model leaves a message
     ending in trailing spaces or a paragraph break, and the runner then appends its own
     end-of-turn vbCrLf pair.  The interior pSpace joins are what matter for
     conformance; the turn's tail is cosmetic (the diff harness collapses it).
     Trim trailing whitespace/newlines and re-add a single '\n' so each turn ends
     the same shape it always did -- the dump harness supplies the blank line. */
  n = strlen (fin);
  while (n > 0 && (fin[n - 1] == '\n' || fin[n - 1] == '\r'
                   || fin[n - 1] == ' ' || fin[n - 1] == '\t'))
    n--;
  if (n > 0)
    {
      char *trimmed = (char *) realloc (fin, n + 2);
      if (trimmed != nullptr)
        fin = trimmed;          /* else the old, larger block serves */
      fin[n] = '\n';
      fin[n + 1] = '\0';
    }
  else if (fin != nullptr)
    fin[0] = '\0';
  a5run_media_end ();
  return fin;
}

/* Run the System tasks flagged <RunImmediately> once at game start, in file
   order, exactly as clsUserSession's init loop does (vb:209-216) -- before the
   title is shown.  Their completion-message output (e.g. a title-music task's
   audio markup + trailing space) accumulates in the same buffer so it joins onto
   the title via pSpace.  Events are not yet initialised at this point (the runner starts
   them after the intro), so no event-completion hooks fire here. */
static void
run_immediate_tasks (a5_run_t *run, sb_t *out)
{
  a5_state_t *st = run->st;
  int i;
  run->immediate_emit = 1;
  for (i = 0; i < run->adv->n_tasks; i++)
    {
      const a5_task_t *t = &run->adv->tasks[i];
      int ti;
      if (!t->run_immediately || !streq (t->type, "System"))
        continue;
      ti = a5state_task_index (st, t->key);
      if (ti >= 0 && st->task_done[ti] && !t->repeatable)
        continue;
      if (!a5restr_pass (st, t->restrictions))
        continue;
      run_task (run, t, 0, out);
      ev_on_task_completed (run, t->key, out);
    }
  run->immediate_emit = 0;
}

char *
a5run_intro (a5_run_t *run)
{
  sb_t out;
  char *intro, *look;
  /* The runner renders the intro as several separate commits (Adventure-Upgrade prompt,
     the RunImmediately output + title at vb:226, the Introduction at vb:227, the
     room view at vb:229), so a <cls> in one unit clears only that unit's buffer
     -- e.g. an Introduction that opens with <cls> must NOT wipe the already-
     committed title (Achtung Panzer).  Resolve each commit unit's markers before
     the next is appended; `cf` is the current unit's start offset. */
  size_t cf = 0;
  int had_prompt = 0;
  sb_init (&out);
  a5run_media_begin (run);
  /*
   * "Adventure Upgrade" auto-correct prompt (FileIO.vb:634).  When the file
   * format version is older than 5.0.26 and any restriction block carries an
   * "#A#O#" (AND-then-OR) BracketSequence, the runner asks once at load whether to
   * auto-correct those tasks (a5model_upgrade_pending).  The host frontend
   * asks the question through its NORMAL input channel -- the next command-
   * script line headless (FrankenDrift.Headless consumes a literal yes/no
   * only, else answers no without consuming), a typed line in the Glk
   * frontend, like the ADRIFT 4 gender prompt -- and calls
   * a5model_upgrade_answer BEFORE building the run.  In that case the
   * question (and, on yes, the "N tasks have been updated." info) has already
   * been shown out-of-band, so here we only mirror the joins: the runner emits both
   * via raw writes that never touch the Display/pSpace accumulator, so the
   * first real Display of game-start (the centred title, vb:226) runs with an
   * empty accumulator and a non-empty title concatenates directly onto them
   * ("...restrictions.Son of Camelot", "...updated.the House").
   *
   * With a frontend that never asks (upgrade still pending here), the runner's
   * unattended behaviour holds: the question prose is still EMITTED before
   * the intro with an implicit NO answer -- the correction is NOT applied and
   * the BracketSequence is read verbatim.  That legacy path prepends the
   * question verbatim after finish_turn (had_prompt == 2): being out-of-band,
   * it must never meet ReplaceALRs or CapAfterFullStop -- a game whose ALRs
   * rewrite ordinary output (the virtual human's lowercasing single-letter
   * overrides) must NOT touch it.
   */
  if (run->adv->upgrade_silent)
    had_prompt = 0;                     /* host resolved it silently: clean intro */
  else if (a5model_upgrade_pending (run->adv))
    had_prompt = 2;                     /* unasked: prepend question, imply NO */
  else if (run->adv->upgrade_prompted)
    had_prompt = 1;                     /* host asked: only mirror the joins */
  /* System <RunImmediately> tasks run before the title (the runner clsUserSession init,
     vb:209-216); a title-music task's audio markup leaves a trailing space that
     joins onto the centred title. */
  sb_resolve_cls (&out, cf); cf = out.len;   /* commit: Adventure-Upgrade prompt */
  run_immediate_tasks (run, &out);
  /* Only NOW is the player's room marked seen (clsUserSession.vb:371, after the
     RunImmediately loop at vb:355).  The room the adventure names as the start
     is not seen by virtue of being the start: a game whose immediate tasks move
     the player away -- Alien Diver drops the diver on a random ocean square --
     never marks the authored one, and the runner's map draws a single node
     where marking at session creation would draw two. */
  a5state_mark_loc_seen (run->st, a5state_player_location (run->st));
  /* The centred title: the runner's Display("<c>" & Adventure.Title & "</c>" & vbCrLf)
     at vb:226, emitted through the same buffer so the RunImmediately output above
     joins onto it via pSpace.  An empty title emits nothing (the runner's "<c></c>"
     renders to a bare blank that the turn-tail normalisation drops); a non-empty
     one space-joins to any preceding RunImmediately output, then the intro's own
     leading break follows. */
  if (run->adv->title != nullptr && run->adv->title[0] != '\0')
    {
      /* After the out-of-band Adventure-Upgrade prompt the runner's pSpace accumulator is
         empty (the prompt was a raw Console.Write), so a non-empty title joins
         with NO separator -- unless real Display output (RunImmediately) already
         sits after the prompt commit, in which case it pSpace-joins to that. */
      if (!had_prompt || out.len != cf)
        sb_pspace (&out);
      /* The runner builds "<c>" & Adventure.Title & "</c>" and hands the whole
         string to ONE HTML rendering pass, so the InputColour wrap and any
         markup INSIDE the title -- Trapped's is
         "<centre><b>'Trapped'  by Driftwood</b></centre>" -- are stripped
         together.  Render the wrap the same way, in a single pass: interactive
         mode then leaves an A5_COLOUR_MARK span for the host, headless drops
         <c> to A5_ALR_MARK (stripped in finish_turn) so goldens are unchanged.
         Rendering the title first and wrapping the result would feed rendered
         text back through the parser, and a title whose entities decode to
         markup ("&lt;b&gt;") would be re-parsed as a tag and vanish. */
      {
        std::string wrapped = std::string ("<c>") + run->adv->title + "</c>";
        char *tp = a5text_render_plain (wrapped.c_str ());
        sb_puts (&out, tp);
        free (tp);
      }
      sb_puts (&out, "\n");
    }
  else if (had_prompt)
    {
      /* Prompt with an empty title (RtC): the runner's empty "<c></c>" Display still
         emits a line break and the room view that follows carries its own leading
         vbCrLf (vb:229), so the prompt is separated from the intro/room by a blank
         line.  Scarier's room path has no leading break, so restore the paragraph
         break the prompt's dropped "\n\n" used to provide. */
      sb_puts (&out, "\n\n");
    }
  sb_resolve_cls (&out, cf); cf = out.len;   /* commit: RunImmediately + title */
  intro = a5text_describe (run->st, run->adv->introduction);
  /* No paragraph break after an intro whose last visible act is a <cls> (DDF's
     multi-page intro ends "...<waitkey><cls>"): the break would land AFTER the
     wipe marker and survive the resolve below as two stray blank lines at the
     top of the cleared screen, which the runner never shows. */
  if (intro[0])
    {
      sb_puts (&out, intro);
      if (!msg_ends_with_cls (intro))
        sb_puts (&out, "\n\n");
    }
  free (intro);
  sb_resolve_cls (&out, cf); cf = out.len;   /* commit: Introduction (vb:227) */
  /* Show the start room only when <ShowFirstLocation> is set (the default);
     games like Six Silver Bullets clear it and reveal the room via a LOOK or
     the first move (clsUserSession game-start: gated on Adventure.ShowFirstRoom). */
  if (run->adv->show_first_location)
    {
      /* Real output: retire DisplayOnce segments (view_location_impl honours
         the ambient marking flag; see a5text.cpp). */
      {
        a5_mark_guard mg (run->st, 1);
        look = a5text_view_location (run->st);
      }
      sb_puts (&out, look);
      free (look);
    }
  sb_resolve_cls (&out, cf); cf = out.len;   /* commit: room view (vb:229) */
  /* Start the Immediately events (clsUserSession init loop, after intro). */
  ev_init (run, &out);
  sb_resolve_cls (&out, cf);                 /* commit: start-of-game events */
  update_seen (run->st);
  a5state_stamp_prev (run->st);
  {
    char *turn = finish_turn (run, &out);
    if (had_prompt != 2)
      return turn;
    /* Legacy unasked path: prepend the out-of-band Adventure-Upgrade prompt
       verbatim: the runner wrote it raw to the console before the engine's first
       Display, so it reaches the player un-ALR'd and un-capitalised, directly
       abutting the first real output (see the had_prompt joins above). */
    sb_t pb;
    sb_init (&pb);
    sb_puts (&pb, "Adventure Upgrade\n");
    sb_puts (&pb, a5model_upgrade_question ());
    sb_puts (&pb, turn);
    free (turn);
    return sb_finish (&pb);
  }
}

/* Remember the pending ambiguity on `run`, emit its "Which ...?" prompt, and
   finish the turn.  Shared by the first-pass and second-chance ambiguity arms
   of a5run_input, which set up identical state before re-prompting. */
static char *
remember_amb_and_prompt (a5_run_t *run, a5_state_t *st, const std::string &in,
                         const amb_info &amb, int amb_ti, int amb_ci, sb_t *out)
{
  run->amb_active = 1;
  run->amb_task_index = amb_ti;
  run->amb_command_index = amb_ci;
  run->amb_input = in;
  run->amb_ref_name = amb.ref_name;
  run->amb_ref_type = amb.type;
  run->amb_word = amb_word (st, amb.keys, amb.ref_text, amb.type);
  run->amb_keys = amb.keys;
  sb_puts (out,
           build_amb_prompt (st, run->amb_word, amb.keys, amb.type).c_str ());
  return finish_turn (run, out);
}

static char *a5run_input_inner (a5_run_t *run, const char *line);

/* One processed input line, from the turn claim to finish_turn.  Built by
   a5run_input_inner, which owns the sequence: prepare (the turn claim and
   PrepareForNextTurn), clarify (a pending ambiguity's clarifier), scan
   (the general task scan and a claimed turn), settle (the post-scan
   ladder: fail text, re-prompt, cantsee, ambiguity, noref) and fallback
   (system command, remembered-verb retry, NotUnderstood).  Every stage
   after prepare returns the finished turn's text, or nullptr to hand on. */
namespace {

struct input_turn_t
{
  a5_run_t *run;
  a5_state_t *st;
  sb_t out;
  std::string in, rverb, fail_text;
  int have_fail, have_amb, amb_ti, amb_ci, amb_cantsee;
  int have_noref, noref_ti, noref_ci;
  amb_info amb;
  int resolving_amb;
  long amb_narrowed;

  input_turn_t (a5_run_t *run_, const char *line);
  void prepare ();
  char *clarify ();
  char *scan ();
  char *settle ();
  char *fallback ();
};

input_turn_t::input_turn_t (a5_run_t *run_, const char *line)
  : run (run_), st (run_->st), in (normalise_input (line)),
    have_fail (0), have_amb (0), amb_ti (-1), amb_ci (-1), amb_cantsee (0),
    have_noref (0), noref_ti (-1), noref_ci (-1), resolving_amb (0),
    amb_narrowed (-1)
{
  sb_init (&out);
}


void
input_turn_t::prepare ()
{
  /* This command claims a turn (clsAdventure.Turns, bumped once per processed
     command by the Runner).  The end-of-game score line shows the count *before*
     this command, so emit_endgame reads turns-1; bump here, after the blank-line
     skip, so empty input does not count.  A remembered-verb retry decrements
     first (below) to keep one user command == one turn. */
  st->turns++;

  /* Consume any remembered verb for this turn (sRememberedVerb): captured here so
     a turn that does anything naturally clears it (clsUserSession clears it on a
     successful task); a system command re-sets it (the runner keeps it across SystemTasks)
     and the NotUnderstood fallback re-tries the input as "<verb> <input>". */
  rverb = run->remembered_verb;
  run->remembered_verb.clear ();

  /* Refresh the player's "seen" set from where things ended up last turn
     (clsUserSession.PrepareForNextTurn), so HaveSeenCharacter / "characters
     must be seen" gates reflect everyone visible by now. */
  update_seen (st);

  /* PrepareForNextTurn also stamps every object's/character's PrevParent
     (vb:5214-5218), the state the %Prev...% functions read this turn. */
  a5state_stamp_prev (st);

  /* PrepareForNextTurn also empties every character's per-turn route memo
     (dictHasRouteCache/dictRouteErrors, vb:3792) -- see
     a5restr_exit_in_direction. */
  a5restr_route_cache_clear (st);

  /* PrepareForNextTurn also clears the rendered-character-name ledger
     (PronounKeys.Clear, vb:3823).  The runner clears at the END of each processed
     command (vb:504) -- nothing renders between that and the next input, so
     clearing here is equivalent.  The init path clears too: OpenAdventure
     displays the title/intro/first room (vb:227-229) and only THEN runs its
     init-time PrepareForNextTurn (vb:283), so no intro entry survives into the
     first command (SampleConversation: the start room's CharHereDesc mention of
     the old lady must not pronoun-replace her conversation intro to "She"). */
  st->n_pron = 0;
  st->pron_pending = 0;

  /* Resolve "it"/"them"/"him"/"her" to the last-referenced entity (echoing the
     "(name)" line) and recompute the referents for next turn, before the input
     is parsed -- clsUserSession.EvaluateInput.  Skipped while resolving a pending
     disambiguation clarifier, which is not a fresh command. */
  if (!run->amb_active)
    {
      resolve_pronouns (run, in, &out);
      resolve_synonyms (run, in);
    }
}

char *
input_turn_t::clarify ()
{
  /* Pending disambiguation (clsUserSession.ResolveAmbiguity): try `in` as a
     clarifier first -- if it narrows the remembered candidates to exactly one,
     re-run the remembered task with that pick.  Otherwise fall through and let
     `in` be parsed as a fresh command; only if nothing runs do we re-prompt. */
  if (run->amb_active)
    {
      std::vector<std::string> narrowed =
        possible_keys (st, run->amb_keys, in, run->amb_ref_type);
      if (narrowed.size () == 1
          && run_remembered (run, narrowed[0].c_str (), &out))
        { run->amb_active = 0; ev_tick_all (run, &out); return finish_turn (run, &out); }
      /* Not resolved to one.  The runner keeps sAmbTask set while trying the new input as
         a fresh command (GetGeneralTask sets sAmbTask only when it is Nothing,
         and the second-chance noref pass runs only when sAmbTask Is Nothing), so
         a fresh ambiguity / noref CANNOT override the remembered ambiguity --
         only a passing or failing-with-output task can claim the turn.  After
         that, DisplayAmbiguityQuestion re-runs on the remembered reference now
         narrowed by PossibleKeys: Count 0 -> "Sorry, that does not clarify the
         ambiguity."; Count >1 -> "Which X?". */
      resolving_amb = 1;
      amb_narrowed = (long) narrowed.size ();
      /* Keep the narrowing progress.  Reaching here with size()==1 means the
         clarifier picked a unique referent but run_remembered() could not run
         the remembered task (no matching command variant); still commit the
         narrowed set so a later re-prompt shows the pared-down candidates
         rather than the original full "Which X?" list. */
      if (narrowed.size () >= 1)
        run->amb_keys = narrowed;
    }

  return nullptr;
}

char *
input_turn_t::scan ()
{
  /* Adventure.sReferencedText: cleared right before a fresh GetGeneralTask scan
     (clsUserSession.vb:3389; the ambiguity-resolution path above keeps the
     original command's slots).  scan_tasks refills the slots from every
     command-matched candidate's %text% captures; afterwards an empty slot 0
     defaults to the raw input (vb:3400) for the post-scan paths (noref/fail
     runs, event ticks). */
  for (int si = 0; si < 5; si++)
    st->scan_text[si][0] = '\0';
  st->turn_out_nonempty = 0;

  int scan_claimed = scan_tasks (run, in, &out, &have_amb, &amb, &amb_ti,
                                 &amb_ci, &amb_cantsee, &have_fail, &fail_text,
                                 &have_noref, &noref_ti, &noref_ci);
  if (st->scan_text[0][0] == '\0')
    {
      strncpy (st->scan_text[0], in.c_str (), sizeof st->scan_text[0] - 1);
      st->scan_text[0][sizeof st->scan_text[0] - 1] = '\0';
    }
  if (scan_claimed)
    {
      run->amb_active = 0;
      /* clsUserSession.vb:3421-3431: the chosen task ran, the qTasksToRun
         LocationTrigger queue is drained, and only THEN does the runner test
         `If sOutputText = "" Then NotUnderstood()` -- skipping TurnBasedStuff
         entirely on that branch, so events do not tick either.  AoK's "Talk to
         Oilman + 2" override has actions (Variable33=1) but no completion
         message: the runner greets silently then prints the NotUnderstood ladder's "I
         don't understand what you want to do with the armourer."  The drain
         must come first (Mazoomah's `push radio` win text arrives via the
         YouHaveWon LocationTrigger tasks), and `sOutputText` is the raw
         marked-up buffer, so a markup-only completion counts as output
         (turn_out_nonempty; Bug Hunt's cl_ReadMap `<img>` map).  A won/lost
         game skips the fallback outright -- the runner's endgame turns always
         displayed something. */
      /* The runner expands ALR-borne %variable% tokens at each Display COMMIT, not
         once per turn: the command's own responses Display before the
         LocationTrigger drain runs (so a drain task's IncVariable must not
         retro-date the room header's %scor%), and the drain's output
         Displays before the event tick.  Expand the deferred-variable
         sentinels accumulated so far at each of those boundaries;
         finish_turn catches whatever the event tick emits. */
      expand_var_defers_sb (run, &out);
      drain_tasks_to_run (run, &out);
      if (out.len == 0 && !st->turn_out_nonempty && !st->game_over)
        {
          not_understood (run, in, &out);
          return finish_turn (run, &out);
        }
      expand_var_defers_sb (run, &out);
      ev_tick_all (run, &out);
      return finish_turn (run, &out);
    }

  return nullptr;
}

char *
input_turn_t::settle ()
{
  if (have_fail)
    {
      /* A HighestPriorityPassingTask game: no task passed, so show the
         highest-priority failing task's recorded message.  In the Adrift 5 runner this
         sets GetGeneralTask (non-Nothing), so it preempts the sNoRefTask and the
         ambiguity display.  It is non-empty output, so the turn advances. */
      run->amb_active = 0;
      sb_puts (&out, fail_text.c_str ());
      ev_tick_all (run, &out);
      return finish_turn (run, &out);
    }

  if (resolving_amb)
    {
      /* The new input neither resolved the ambiguity nor ran a fresh task.  Show
         the remembered ambiguity's DisplayAmbiguityQuestion result (the fresh
         command's own ambiguity/noref is suppressed, as the runner leaves sAmbTask set
         throughout resolution).  A clarifier matching none of the candidates
         narrows the reference to zero -> "Sorry, that does not clarify the
         ambiguity." (clsUserSession.vb:2780); a still-ambiguous clarifier
         (Count >1) re-asks "Which X?". */
      if (amb_narrowed == 0)
        {
          run->amb_active = 0;
          sb_puts (&out, "Sorry, that does not clarify the ambiguity.\n");
        }
      else
        sb_puts (&out, build_amb_prompt (st, run->amb_word, run->amb_keys,
                                         run->amb_ref_type).c_str ());
      return finish_turn (run, &out);
    }

  if (have_amb && amb_cantsee && !amb.second_chance)
    {
      /* No later task claimed the turn: emit the deferred "You can't see any
         <plural>!" (the out-of-scope reference is a dead end, not a pending
         prompt).  This does NOT tick TurnBasedStuff: in the Adrift 5 runner an
         unresolvable reference leaves GetGeneralTask returning Nothing, so
         ProcessTurn takes the `sTaskKey Is Nothing` branch (clsUserSession.vb
         :3436) which shows the message but never calls TurnBasedStuff -- unlike
         a failing-but-matched task (the noref/have_fail paths), which does tick.
         Ticking here drifts turn-based event countdowns early (StarshipQuest's
         hyperspace timer fired ~8 turns too soon).

         A cantsee preempts the sNoRefTask below: in the runner a cantsee/ambiguity is a
         *first-pass* result (the reference name-matched >1 objects) that sets
         sAmbTask, whereas the exist-restriction sNoRefTask is only found in the
         *second-chance* pass -- and that pass runs solely when sAmbTask Is
         Nothing (GetGeneralTask, clsUserSession.vb:5965 recursion guard).  So a
         command like "get flashlight from backpack", where TakeObjectsFromOthers
         (%objects% from %object2%) name-resolves "flashlight" to several unseen
         objects (cantsee) but GetObjectC (%objects%) swallows the whole
         "flashlight from backpack" as one unmatched reference (a Must-Exist
         noref), must show "You can't see any flashlights!" -- not the noref's
         "I'm not sure which object...".  Likewise for a visible ambiguity below. */
      run->amb_active = 0;
      emit_cantsee (st, &amb, &out);
      return finish_turn (run, &out);
    }

  if (have_amb && !amb.second_chance)
    {
      /* Remember the ambiguity and prompt; the next turn resolves it.  Like the
         cantsee above, a first-pass ambiguity preempts the second-chance
         sNoRefTask below (the runner enters the exist pass only when sAmbTask Is
         Nothing). */
      return remember_amb_and_prompt (run, st, in, amb, amb_ti, amb_ci, &out);
    }

  if (have_noref && run_noref (run, noref_ti, noref_ci, in, &out))
    {
      /* sNoRefTask: a command matched but a reference named nothing.  Running
         the task with the reference unresolved surfaced its `Must Exist`
         message.  Reached only when no task produced a cantsee/ambiguity above
         -- in the runner the sNoRefTask is the result of the second-chance (existence)
         pass, which GetGeneralTask runs only if the first pass left sAmbTask
         Nothing (clsUserSession.vb:5965).  It does claim the turn and ticks. */
      run->amb_active = 0;
      ev_tick_all (run, &out);
      return finish_turn (run, &out);
    }

  if (have_amb)
    {
      /* A *second-chance* ambiguity: the task it came from matched only because
         it also had an unmatched required reference, so the runner found it in the
         second-chance pass (sAmbTask).  It loses to a clean sNoRefTask above;
         only when none claimed the turn does it surface here (the runner shows sAmbTask
         when GetGeneralTask is Nothing).  Same cantsee-vs-prompt split as the
         first-pass case above. */
      if (amb_cantsee)
        {
          run->amb_active = 0;
          emit_cantsee (st, &amb, &out);
          return finish_turn (run, &out);
        }
      return remember_amb_and_prompt (run, st, in, amb, amb_ti, amb_ci, &out);
    }

  if (run->amb_active)
    {
      /* A clarifier that neither resolved nor ran a fresh command: re-prompt. */
      sb_puts (&out, build_amb_prompt (st, run->amb_word, run->amb_keys,
                                       run->amb_ref_type).c_str ());
      return finish_turn (run, &out);
    }

  return nullptr;
}

char *
input_turn_t::fallback ()
{
  if (system_command (run, in, &out))
    { run->amb_active = 0; run->remembered_verb = rverb; }   /* SystemTasks keeps it */
  else if (!rverb.empty ())
    {
      /* Remembered-verb retry: re-run the whole turn as "<verb> <input>".  The
         retried turn always produces output (its own NotUnderstood at worst), so
         it claims the turn (the runner: "If sOutputText <> '' Then Exit Sub"). */
      run->amb_active = 0;
      free (out.p);
      st->turns--;            /* the retry re-bumps it; one user command == one turn */
      return a5run_input_inner (run, (rverb + " " + in).c_str ());
    }
  else
    not_understood (run, in, &out);
  return finish_turn (run, &out);
}

} /* namespace */

static char *
a5run_input_inner (a5_run_t *run, const char *line)
{
  input_turn_t turn (run, line);
  char *done;

  a5run_media_begin (run);
  if (a5run_trace)
    fprintf (stderr, "\n=== INPUT: \"%s\" ===\n", turn.in.c_str ());
  if (turn.in.empty ())
    return finish_turn (run, &turn.out);

  turn.prepare ();
  if ((done = turn.clarify ()) != nullptr)
    return done;
  if ((done = turn.scan ()) != nullptr)
    return done;
  if ((done = turn.settle ()) != nullptr)
    return done;
  return turn.fallback ();
}

/* One 1-second TimeBased tick plus its own finish_turn commit (the runner's
   TimeBasedStuff -> Display("", True) -> CheckEndOfGame).  Returns the tick's
   rendered text (caller frees), or NULL when the tick flushed nothing -- no
   output and no fresh end-of-game banner. */
static char *
time_tick_commit (a5_run_t *run)
{
  sb_t tout;
  char *extra = nullptr;
  sb_init (&tout);
  run->in_time_tick = 1;
  ev_time_tick_all (run, &tout);
  if (tout.len != 0 || (run->st->game_over && !run->st->end_displayed))
    extra = finish_turn (run, &tout);
  else
    free (tout.p);
  run->in_time_tick = 0;
  return extra;
}

/* clsUserSession.EvaluateInput's top guard, reached for every command entered
   AFTER the game has ended (vb:3360-3369): SystemTasks(True) honours only
   restart / restore / quit / undo (save, wait and everything else return False
   when bEarly), and a rejected input Displays "Please give one of the answers
   above." followed by CheckEndOfGame's re-printed restart prompt -- the prompt
   sits outside the bDisplayedWinLose gate, so the runner re-asks on every
   rejected post-game command.

   UNDO is honoured here in full: the engine owns the undo stack, and the
   runner's Undo() (vb:3178) Displays "Undone." followed by the restored
   session's sTurnOutput, after which play resumes.  RESTART / RESTORE / QUIT
   return empty text for the HOST to act on (rebuild the run, run a file
   dialog, exit) -- the Glk frontend runs its own endgame loop and never
   reaches this path; the a5run_dump harness honours quit and warns on
   restart/restore.  No turn is consumed and no events tick (the runner's
   guard returns before any turn processing). */
static char *
endgame_guard_input (a5_run_t *run, const char *line)
{
  sb_t out;
  std::string in = normalise_input (line);

  sb_init (&out);
  a5run_media_begin (run);
  if (in == "undo")
    {
      if (a5run_undo (run))
        {
          /* The runner's SetLastState does NOT roll the turn counter back
             across the undone command: measured on October31st, a win at "153
             turns" followed by undo + an immediate re-kill reports "154
             turns" (155 with one command in between).  Scarier's snapshot is
             pre-turn, so re-count the undone command's turn.  Only here: the
             mid-game A5_UNDO_AT self-check replays the SAME command after
             a5run_undo and must stay byte-identical, so the compensation
             cannot live inside a5run_undo itself. */
          run->st->turns++;
          sb_puts (&out, "Undone.");
          if (!run->last_turn_text.empty ())
            {
              sb_pspace (&out);
              sb_puts (&out, run->last_turn_text.c_str ());
            }
        }
      else
        /* The runner: "Sorry, <c>undo</c> is not currently available." */
        sb_puts (&out, "Sorry, undo is not currently available.");
    }
  else if (!(in == "restart" || in == "restore" || in == "quit"
             || in.empty ()))
    {
      sb_puts (&out, "Please give one of the answers above.\n");
      emit_end_prompt (&out);
    }
  return finish_turn (run, &out);
}

char *
a5run_input (a5_run_t *run, const char *line)
{
  if (run->st->game_over)
    return endgame_guard_input (run, line);
  char *txt = a5run_input_inner (run, line);
  /* The real Runner's 1-second tmrEvents timer, deterministically: tick every
     TimeBased event once per processed input line (see ev_time_tick_all).  The
     tick's output flushes as its own commit AFTER the command's, so it gets its
     own finish_turn pass and is appended to the command's text.  In real-time
     mode the host drives this tick from a wall-clock timer instead
     (a5run_time_tick), as the Runner itself does. */
  if (!run->st->game_over && !run->real_time)
    {
      char *extra = time_tick_commit (run);
      if (extra != nullptr && extra[0] != '\0')
        {
          size_t tn = txt != nullptr ? strlen (txt) : 0;
          size_t en = strlen (extra);
          char *joined = (char *) malloc (tn + en + 2);
          if (joined != nullptr)
            {
              memcpy (joined, txt != nullptr ? txt : "", tn);
              joined[tn] = '\n';
              memcpy (joined + tn + 1, extra, en + 1);
              free (txt);
              txt = joined;
            }
        }
      free (extra);
    }
  /* Retain this turn's final text (the runner's sTurnOutput): a5run_snapshot
     copies it alongside each undo snapshot so a post-game UNDO can replay the
     restored turn's output, as the runner's Undo() does. */
  run->last_turn_text = txt != nullptr ? txt : "";
  return txt;
}

void
a5run_set_real_time (a5_run_t *run, int on)
{
  if (run != nullptr)
    run->real_time = on != 0;
}

int
a5run_has_time_events (a5_run_t *run)
{
  int ei;
  if (run == nullptr)
    return 0;
  for (ei = 0; ei < run->adv->n_events; ei++)
    if (run->adv->events[ei].type != nullptr
        && streq (run->adv->events[ei].type, "TimeBased"))
      return 1;
  return 0;
}

char *
a5run_time_tick (a5_run_t *run)
{
  char *txt;
  if (run == nullptr || run->st->game_over)
    return nullptr;
  /* A fresh media list, as at the top of a turn, so the tick's own <img>/
     <audio> markup reaches the host.  finish_turn closes the sink; close it
     here too when the tick flushes nothing and finish_turn never runs. */
  a5run_media_begin (run);
  txt = time_tick_commit (run);
  if (txt == nullptr)
    a5run_media_end ();
  return txt;
}

/* ---------------------------------------------------------------- room view */

/* Render the current room view as a standalone piece of display text -- the
   stock Look task's view (darkness override and Look-aggregate segments
   included) run through the normal end-of-turn pipeline (ALRs, deferred
   variables, presentation marks), but consuming no turn: the turn counter is
   untouched and no events tick.  The Glk frontend shows this after a
   successful UNDO, re-orienting the player the way the v4 engine's room-name
   line does.  Note that a description whose expressions draw from the RNG
   advances the stream, exactly as a typed LOOK would.  Caller frees. */
char *
a5run_look (a5_run_t *run)
{
  sb_t out;

  if (run == nullptr)
    return nullptr;
  a5run_media_begin (run);
  sb_init (&out);
  sb_puts (&out, render_look_marked (run).c_str ());
  return finish_turn (run, &out);
}
