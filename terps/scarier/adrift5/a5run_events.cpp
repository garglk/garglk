/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- events + walks turn-based runtime.
 *
 * A faithful port of clsEvent's / clsWalk's turn-based state machines
 * (IncrementTimer / DoAnySubEvents / lStart / lStop), driven each turn by
 * clsUserSession.TurnBasedStuff and on task completion by the EventControls.
 * Split out of a5run.cpp; the shared run struct and the ev_* entry points it
 * exposes to the core turn loop live in a5run_internal.h.
 */

#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "a5parse.h"
#include "a5rand.h"
#include "a5restr.h"
#include "a5run.h"
#include "a5run_internal.h"
#include "a5sb.h"
#include "a5text.h"

static void ev_lstart (a5_run_t *run, int ei, int restart, sb_t *out);
static void ev_lstop  (a5_run_t *run, int ei, int run_subs, sb_t *out);
static void ev_do_subevents (a5_run_t *run, int ei, sb_t *out);
static void attempt_event_task (a5_run_t *run, const char *key, int depth, sb_t *out);

/* Walk drivers (defined after the events; called from the same hook points). */
static void wk_on_task_completed (a5_run_t *run, const char *task_key);
static void wk_tick_all (a5_run_t *run, sb_t *out);
static void wk_init     (a5_run_t *run, sb_t *out);

static long ev_from_start (const a5_event_rt &rt) { return rt.length_value - rt.timer_to_end; }
static long ev_from_last  (const a5_event_rt &rt) { return ev_from_start (rt) - rt.last_se_time; }

/* clsEvent.Length is a FromTo, and FromTo.Value (Global.vb:3770) resolves
   lazily: the first read of an as-yet-unevaluated Length draws Random(from,to)
   and caches it -- even for an event that has never started.  ev_lstop is the
   one place that can reach a Length that lStart has not already rolled. */
static long
ev_length_value (a5_run_t *run, int ei)
{
  const a5_event_t *e = &run->adv->events[ei];
  a5_event_rt &rt = (*run->events)[ei];
  if (!rt.length_set)
    {
      rt.length_value = a5rand_between (e->length_from, e->length_to);
      rt.length_set = 1;
    }
  return rt.length_value;
}

/* Roll a fresh Length (clsEvent Length.Reset + first .Value): every lStart --
   and the BetweenXY init path -- draws anew, unlike the lazy read above. */
static long
ev_roll_length (a5_run_t *run, int ei)
{
  const a5_event_t *e = &run->adv->events[ei];
  a5_event_rt &rt = (*run->events)[ei];
  rt.length_value = a5rand_between (e->length_from, e->length_to);
  rt.length_set = 1;
  return rt.length_value;
}

/* Emit an already-rendered plain message when it has output, then free it. */
static void
emit_owned (sb_t *out, char *m)
{
  if (msg_has_output (m)) { sb_pspace (out); sb_puts (out, m); }
  free (m);
}

/* Render a subevent/subwalk <DisplayMessage> as REAL output (marking_display=1
   so its <DisplayOnce> segments retire) and emit it when non-empty.  Shared by
   the event and walk DisplayMessage cases. */
static void
emit_marked_description (a5_run_t *run, const a5_xml_node_t *desc, sb_t *out)
{
  a5_state_t *st = run->st;
  char *m;
  {
    a5_mark_guard mg (st, 1);
    m = a5text_describe (st, desc);
  }
  emit_owned (out, m);
}

/* clsTask.UnsetTask via a sub-event/sub-walk: clear the task's Completed flag. */
static void
unset_task (a5_state_t *st, const char *key)
{
  int ti = a5state_task_index (st, key);
  if (ti >= 0)
    st->task_done[ti] = 0;
}

/* The TimerToEndOfEvent property setter: assigning it can start a counted-down
   event or stop a running one (clsEvent.TimerToEndOfEvent Set). */
static void
ev_set_timer_to_end (a5_run_t *run, int ei, long value, sb_t *out)
{
  a5_event_rt &rt = (*run->events)[ei];
  rt.timer_to_end = value;
  if (rt.status == A5_EV_COUNTDOWN && ev_from_start (rt) == 0)
    ev_lstart (run, ei, 1, out);                  /* Start(True) */
  if (rt.status == A5_EV_RUNNING && rt.timer_to_end == 0)
    ev_lstop (run, ei, 1, out);                   /* lStop(True) */
}

static void
ev_run_subevent (a5_run_t *run, int ei, int sei, sb_t *out)
{
  const a5_event_t *e = &run->adv->events[ei];
  a5_event_rt &rt = (*run->events)[ei];
  const a5_subevent_t *se = &e->subevents[sei];

  switch (se->what)
    {
    case A5_SE_EXECTASK:
      if (se->key != NULL)
        attempt_event_task (run, se->key, 0, out);
      break;
    case A5_SE_UNSETTASK:
      if (se->key != NULL)
        unset_task (run->st, se->key);
      break;
    case A5_SE_DISPLAY:
      /* clsEvent.RunSubEvent DisplayMessage: show only if the OnlyApplyAt gate
         (se->key) is set and the player is in that location/group. */
      if (se->description != NULL && se->key != NULL && se->key[0] != '\0'
          && a5state_in_group_or_location (run->st, a5state_player_key (run->st), se->key))
        emit_marked_description (run, se->description, out);
      break;
    case A5_SE_SETLOOK:
      /* clsEvent SetLook: push (OnlyApplyAt gate, rendered text) onto the look
         stack; a5text_view_location consults it (LookText). */
      if (se->description != NULL)
        { char *m = a5text_describe (run->st, se->description);
          a5state_push_look (run->st, se->key, m);
          free (m); }
      break;
    }
  rt.last_se_time  = ev_from_start (rt);
  rt.last_se_index = sei;
}

static void
ev_do_subevents (a5_run_t *run, int ei, sb_t *out)
{
  const a5_event_t *e = &run->adv->events[ei];
  a5_event_rt &rt = (*run->events)[ei];
  int i;
  if (rt.status != A5_EV_RUNNING)
    return;
  for (i = 0; i < e->n_subevents; i++)
    {
      const a5_subevent_t *se = &e->subevents[i];
      long ft = rt.se_ft[i];
      int go = 0;
      switch (se->when)
        {
        case A5_SE_FROM_START:
          if (ev_from_start (rt) == ft && ft <= rt.length_value
              && (ft > 0 || rt.when_start != 1))
            go = 1;
          break;
        case A5_SE_FROM_LAST:
          if (ev_from_last (rt) == ft
              && ((rt.last_se_index == -1 && i == 0)
                  || (i > 0 && rt.last_se_index == i - 1)))
            go = 1;
          break;
        case A5_SE_BEFORE_END:
          if (rt.timer_to_end == ft)
            go = 1;
          break;
        }
      if (go)
        ev_run_subevent (run, ei, i, out);
    }
}

static void
ev_lstart (a5_run_t *run, int ei, int restart, sb_t *out)
{
  const a5_event_t *e = &run->adv->events[ei];
  a5_event_rt &rt = (*run->events)[ei];
  int i;
  if (!(rt.status == A5_EV_NOTYET || rt.status == A5_EV_COUNTDOWN
        || rt.status == A5_EV_FINISHED
        || (rt.status == A5_EV_RUNNING && restart)))
    return;
  rt.status = A5_EV_RUNNING;
  ev_roll_length (run, ei);
  rt.last_se_index = -1;
  rt.last_se_time = 0;
  for (i = 0; i < e->n_subevents; i++)
    rt.se_ft[i] = a5rand_between (e->subevents[i].ft_from, e->subevents[i].ft_to);

  ev_set_timer_to_end (run, ei, rt.length_value, out);
  if (ev_from_start (rt) == 0)
    ev_do_subevents (run, ei, out);              /* 'after 0 turns' sub-events */

  if (rt.when_start == 1)                         /* Immediately -> BetweenXY  */
    rt.when_start = 2;
  rt.just_started = 1;
}

static void
ev_lstop (a5_run_t *run, int ei, int run_subs, sb_t *out)
{
  const a5_event_t *e = &run->adv->events[ei];
  a5_event_rt &rt = (*run->events)[ei];
  if (run_subs)
    ev_do_subevents (run, ei, out);
  if (rt.status == A5_EV_PAUSED)
    return;
  rt.status = A5_EV_FINISHED;
  /* A repeating event whose timer sits at 0 restarts here -- and that includes
     one that has never run at all (TimerToEndOfEvent starts at 0), so a Stop
     control aimed at a not-yet-started repeating event STARTS it.  Tempus
     Fugit depends on it: walking into the Castle Entrance fires the `exit
     castle` system task, whose Stop control kicks off the 15-turn guard event
     one turn before the Great Hall task would have. */
  if (e->repeating && rt.timer_to_end == 0)
    {
      if (ev_length_value (run, ei) > 0)
        {
          if (e->repeat_countdown)
            {
              long delay = a5rand_between (e->start_from, e->start_to);
              rt.status = A5_EV_COUNTDOWN;
              ev_set_timer_to_end (run, ei, delay + rt.length_value, out);
            }
          else
            ev_lstart (run, ei, 1, out);
        }
      /* else: zero-length repeating event -- don't restart (infinite loop) */
    }
}

/* Apply an event Start/Stop/Pause/Resume command to event `ei` immediately
   (clsEvent.Start/Stop/...).  Shared by the deferred path (ev_increment, on the
   pending NextCommand) and the immediate path (ev_control, while a tick runs). */
static void
ev_apply_command (a5_run_t *run, int ei, int cmd, sb_t *out)
{
  a5_event_rt &rt = (*run->events)[ei];
  switch (cmd)
    {
    case A5_CMD_START:  ev_lstart (run, ei, 0, out); break;
    case A5_CMD_STOP:   ev_lstop  (run, ei, 0, out); break;
    case A5_CMD_PAUSE:  if (rt.status == A5_EV_RUNNING) rt.status = A5_EV_PAUSED; break;
    case A5_CMD_RESUME: if (rt.status == A5_EV_PAUSED)  rt.status = A5_EV_RUNNING; break;
    }
}

static void
ev_increment (a5_run_t *run, int ei, sb_t *out)
{
  a5_event_rt &rt = (*run->events)[ei];

  if (rt.next_command != A5_CMD_NONE)
    {
      ev_apply_command (run, ei, rt.next_command, out);
      rt.next_command = A5_CMD_NONE;
      rt.triggering_task.clear ();
    }

  switch (rt.status)
    {
    case A5_EV_COUNTDOWN:
      ev_set_timer_to_end (run, ei, rt.timer_to_end - 1, out);
      break;
    case A5_EV_RUNNING:
      if (!rt.just_started)
        ev_set_timer_to_end (run, ei, rt.timer_to_end - 1, out);
      break;
    default:
      break;
    }

  if (!rt.just_started)
    ev_do_subevents (run, ei, out);
  rt.just_started = 0;
}

/* Defer a control's Start/Stop/etc. unless we are already inside an event tick
   (clsEvent.Start/Stop: NextCommand vs immediate). */
static void
ev_control (a5_run_t *run, int ei, int cmd, const char *task_key, sb_t *out)
{
  a5_event_rt &rt = (*run->events)[ei];
  if (run->events_running)
    ev_apply_command (run, ei, cmd, out);
  else
    rt.next_command = cmd;
  rt.triggering_task = task_key ? task_key : "";
}

/* clsTask.Children(True): is `candidate` a (recursive) Specific-override
   descendant of `ancestor`?  Used by the control loop so a parent task does not
   re-trigger an event/walk control that one of its override children already
   triggered -- clsUserSession.vb:872/893
   `Not task.Children(True).Contains(e.sTriggeringTask)`.  (E.g. the parent
   AttackCharacterWithObject must NOT re-fire a control its override child
   s_AttackTheT already handled, which would shift Spectre's noon-bell event.) */
static int
task_is_descendant (const a5_adventure_t *adv, const char *ancestor,
                    const char *candidate, int depth)
{
  const a5_task_t *c;
  if (depth > 16 || candidate == NULL || ancestor == NULL)
    return 0;
  c = a5model_task (adv, candidate);
  if (c == NULL || !streq (c->type, "Specific") || c->general_key == NULL)
    return 0;
  if (streq (c->general_key, ancestor))
    return 1;
  return task_is_descendant (adv, ancestor, c->general_key, depth + 1);
}

/* The runner's control re-trigger guard (clsUserSession.vb:872/893): a control must
   not re-fire for the very task that triggered it, nor for a parent of that task
   (task.Children(True).Contains).  Shared by the event and walk control loops. */
static int
ctrl_retrigger_blocked (const a5_adventure_t *adv,
                        const std::string &triggering_task, const char *task_key)
{
  return !triggering_task.empty ()
         && (triggering_task == task_key
             || task_is_descendant (adv, task_key, triggering_task.c_str (), 0));
}

/* A control's action as the shared event/walk command enum (clsEvent.Start /
   Stop / Pause / Resume, same for clsWalk).  Shared by both control loops. */
static int
ctrl_to_cmd (a5_ctrl_t ctrl)
{
  switch (ctrl)
    {
    case A5_CTRL_START:   return A5_CMD_START;
    case A5_CTRL_STOP:    return A5_CMD_STOP;
    case A5_CTRL_SUSPEND: return A5_CMD_PAUSE;
    case A5_CTRL_RESUME:  return A5_CMD_RESUME;
    }
  return A5_CMD_NONE;
}

/* clsUserSession: when a task completes (bPass), fire any EventControls. */
void
ev_on_task_completed (a5_run_t *run, const char *task_key, sb_t *out)
{
  int ei;
  if (task_key == NULL)
    return;
  /* clsUserSession loops walks then events: fire WalkControls first. */
  wk_on_task_completed (run, task_key);
  for (ei = 0; ei < run->adv->n_events; ei++)
    {
      const a5_event_t *e = &run->adv->events[ei];
      a5_event_rt &rt = (*run->events)[ei];
      int ci;
      for (ci = 0; ci < e->n_controls; ci++)
        {
          const a5_eventctrl_t *c = &e->controls[ci];
          if (!c->on_completion || !streq (c->task_key, task_key))
            continue;
          /* Guard against a task re-triggering the control it just triggered,
             and (the runner's children check) against a parent re-firing a control one
             of its override descendants already triggered. */
          if (ctrl_retrigger_blocked (run->adv, rt.triggering_task, task_key))
            continue;
          ev_control (run, ei, ctrl_to_cmd (c->control), task_key, out);
        }
    }
}

/* Install an attempt's final response entry's reference items (run->ev_tbl,
   see a5run_internal.h) as the ambient leftover the NEXT event task's
   CopyNewRefs iterates -- the runner's end-of-attempt Display loop
   `NewReferences = refs` (clsUserSession.vb:851-856).  Keys are mapped to
   stable model-key pointers; unknown keys drop out. */
static void
ev_install_leftover (a5_run_t *run, const std::vector<std::string> &items)
{
  a5_state_t *st = run->st;
  st->n_ref_items = 0;
  st->ref_items_type = 'o';
  for (const std::string &k : items)
    {
      const a5_object_t *o = a5model_object (st->adv, k.c_str ());
      const char *stable = NULL;
      if (o != NULL)
        stable = o->key;
      else
        {
          int ci = a5state_character_index (st, k.c_str ());
          if (ci >= 0)
            { stable = st->adv->characters[ci].key;
              st->ref_items_type = 'c'; }
        }
      if (stable != NULL && st->n_ref_items < A5_MAX_ITEMS)
        st->ref_items[st->n_ref_items++] = stable;
    }
}

/* Arm the per-attempt AggregateOutput-draw deferral sink (comp_defers ->
   display_defers).  An event/walk/LocationTrigger-fired task is its own
   AttemptToExecuteTask in the runner (bChildTask=False, clsEvent.vb:389 /
   clsCharacter.vb:1630 / clsUserSession.vb:3424), so ITS responses Display at
   the end of THAT attempt (vb:782,851-856) -- which is where an
   AggregateOutput completion's random draw resolves (ReplaceExpressions inside
   Display).  Arm the sink around run_task and ev_defers_flush this attempt's
   entries right after, so e.g. Symphonica 64's Schtick1 children (After +
   aggregate `<# OneOf(..) #>` taunts, executed in list order) hold their OneOf
   draws until AFTER the last sibling Bystander1's eager `rand(1,100)`
   restriction draw, matching the runner's per-turn draw order. */
static size_t
ev_defers_arm (a5_run_t *run, std::vector<std::string> **prev)
{
  *prev = run->comp_defers;
  run->comp_defers = run->display_defers;
  run->in_ev_attempt++;
  return run->display_defers->size ();
}

static void
ev_defers_flush (a5_run_t *run, std::vector<std::string> *prev, size_t defer0,
                 sb_t *out)
{
  run->in_ev_attempt--;
  run->comp_defers = prev;
  a5run_flush_display_defers_from (run, out, defer0);
}

/* Run a task fired by an event (clsUserSession.AttemptToExecuteTask, bEvent):
   restriction-checked, silent on failure, and itself a control trigger. */
static void
attempt_event_task_impl (a5_run_t *run, const char *key, int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  const a5_task_t *t = a5model_task (st->adv, key);
  int ti;
  if (t == NULL || depth > 16)
    return;
  ti = a5state_task_index (st, key);
  if (ti >= 0 && st->task_done[ti] && !t->repeatable)
    return;

  /* The Adrift 5 runner runs an event-fired task through the *same* AttemptToExecuteTask
     as a command: it copies the leftover command references
     (`CopyNewRefs(NewReferences)`) and `ExecuteSubTasks`-iterates them one item at
     a time.  After a plural `%objects%` command whose final response reference
     still holds N items (`NewReferences = refs` in the Display loop,
     clsUserSession.vb:868), that copy carries all N items, so the event task runs
     once PER item.  This is why Amazon's `get ammo and rifle` ticks
     `ts_tasIncrement` twice (+2 minutes) where `get crown and bottle` -- whose
     two takes leave a single-item final reference -- ticks once.  Scarier's
     `resp_flush` already leaves `st->ref_items` equal to the runner's post-Display
     `NewReferences`, so iterate it here.  A 0/1-item leftover keeps the original
     single, refs-cleared run (the overwhelmingly common case stays byte-exact). */
  if (st->n_ref_items > 1)
    {
      int nleft = st->n_ref_items;
      char tchar = st->ref_items_type;
      const char *grp  = (tchar == 'c') ? "characters" : "objects";
      const char *rbnd = (tchar == 'c') ? "ReferencedCharacters" : "ReferencedObjects";
      std::vector<const char *> items (st->ref_items, st->ref_items + nleft);
      int any_ran = 0;
      std::vector<std::string> *prev_defers;
      size_t defer0 = ev_defers_arm (run, &prev_defers);
      for (const char *it : items)
        {
          /* The runner AttemptToExecuteSubTask ReDims NewReferences to this single item;
             mirror run_general's per-item bind so a task that *does* read its
             reference resolves the right one (the increment reads none). */
          a5state_clear_refs (st);
          st->ref_items[0] = it;
          st->n_ref_items = 1;
          st->ref_items_type = tchar;
          bind_reference (st, grp, it, it);
          a5state_bind_ref (st, rbnd, it);
          if (a5restr_pass (st, t->restrictions))
            { run_task (run, t, depth + 1, out); any_ran = 1; }
        }
      ev_defers_flush (run, prev_defers, defer0, out);
      /* Post-attempt ambient: something displayed -> the last response entry's
         items (the runner's final `NewReferences = refs`); nothing displayed ->
         the per-item AttemptToExecuteSubTask ReDim residue, which for an
         event task is a single task-declared-ref-shaped [nil] -- NOT the
         incoming plural.  (FD trace: at night Quest Giver's daz61DaylightC
         fails its Daytrue restriction silently for both stale quest items and
         daz61DaylightC1 still enters with refs=[nil], so the night counter
         ticks once.)  Only an attempt that early-returns above (Completed &&
         !Repeatable) leaves the ambient untouched. */
      if (run->ev_tbl != NULL && !run->ev_tbl->keys.empty ())
        ev_install_leftover (run, run->ev_tbl->refs.back ());
      else
        st->n_ref_items = 0;
      /* Only "complete" (mark done + fire EventControls) if the task actually
         ran for at least one item -- a task whose restrictions fail for every
         item never bPass'd in the runner, so its completion controls must NOT fire.
         Mirrors the single-item path below (which returns on restriction
         failure).  Without this, a plural `get X and Y` command that happens to
         coincide with a turn-based event executing a restriction-FAILING System
         task (e.g. FBA's `cl_HandfireOu1` in a dark room) spuriously fires that
         task's `Stop Completion` control -- killing the handfire event so it
         never re-extinguishes.  ts_tasIncrement-style no-restriction event tasks
         still complete (any_ran stays 1), so Amazon's double-tick is unchanged. */
      if (any_ran)
        {
          if (ti >= 0)
            st->task_done[ti] = 1;
          ev_on_task_completed (run, key, out);
        }
      return;
    }

  a5state_clear_refs (st);                 /* event tasks carry no command refs */
  if (!a5restr_pass (st, t->restrictions))
    {
      /* The runner's AttemptToExecuteTask evaluates responses even for a failing
         event/System/walk task (bCalledFromEvent=True, bChildTask=False), so a
         failing restriction that carries a <Message> is buffered in
         htblResponsesFail and Displayed (clsUserSession.vb:1244-1259 sMessage =
         sRestrictionText -> AddResponse -> the end-of-task Display loop).  The
         AddResponse bHasOutput gate drops the messageless common case, so only a
         restriction with real fail text surfaces -- e.g. FBA's cl_AtButcherS
         LocationTrigger, whose "rope tied" restriction shows "you pull on his
         leash to stop him" instead of the (passing) theft completion.

         Read the failing restriction's Message from st->restriction_text, which
         a5restr_pass just set as a side effect (the runner's sRestrictionText) -- do NOT
         re-run a5restr_fail_message, which would re-evaluate the restriction
         block and draw any RAND()-valued restriction a SECOND time (e.g. SSB's
         TimeTrapsT `Roller Must BeEqualTo 'RAND(1,16)'`), desyncing the RNG. */
      const a5_xml_node_t *fm = st->restriction_text;
      if (fm != NULL)
        emit_owned (out, a5text_describe (st, fm));
      return;
    }
  {
    std::vector<std::string> *prev_defers;
    size_t defer0 = ev_defers_arm (run, &prev_defers);
    run_task (run, t, depth + 1, out);
    ev_defers_flush (run, prev_defers, defer0, out);
  }
  /* This attempt Displayed at least one response: the runner re-assigns the
     ambient NewReferences to the last-inserted entry's items, which the NEXT
     event task's CopyNewRefs iterates (see ev_tbl in a5run_internal.h). */
  if (run->ev_tbl != NULL && !run->ev_tbl->keys.empty ())
    ev_install_leftover (run, run->ev_tbl->refs.back ());
  if (ti >= 0)
    st->task_done[ti] = 1;
  ev_on_task_completed (run, key, out);
}

/* Each event-fired task is its own AttemptToExecuteTask in the runner, with a fresh
   htblResponsesPass -- so give each attempt_event_task its own dedup scope
   (save/restore the previous one for a nested event task).  Within one scope a
   completion message emitted twice via SetTasks-Execute (run_action, which shares
   the current scope, not a new attempt_event_task) shows once -- e.g. Pathway's
   Task5 "The End" banner, executed by both Task33 and its parent Task30. */
static void
attempt_event_task (a5_run_t *run, const char *key, int depth, sb_t *out)
{
  std::set<std::string> seen;
  exec_resp_scope escope;
  std::set<std::string> *prev = run->ev_seen;
  exec_resp_scope *prev_escope = run->exec_scope;
  /* Each attempt gets its own response table (the runner's per-attempt
     htblResponsesPass); a nested attempt (a completion control starting an
     event whose 0-turn subevents fire more tasks) is isolated the same way. */
  ev_resp_tbl tbl;
  ev_resp_tbl *prev_tbl = run->ev_tbl;
  run->ev_tbl = &tbl;
  run->ev_seen = &seen;
  run->exec_scope = &escope;
  attempt_event_task_impl (run, key, depth, out);
  run->ev_seen = prev;
  run->exec_scope = prev_escope;
  run->ev_tbl = prev_tbl;
  exec_scope_flush (run, &escope, out);
}

/* Drain clsAdventure.qTasksToRun: run each LocationTrigger-armed System task in
   FIFO order (clsUserSession.vb:3421 "While qTasksToRun.Count > 0").  A drained
   task may itself move the Player and arm further triggers, so loop until empty.
   Runs after the turn's command task and before TurnBasedStuff.  Non-static:
   a5run_input drains BEFORE the runner's empty-output check (clsUserSession.vb:3421
   drains ahead of the vb:3425 `If sOutputText = "" Then NotUnderstood()`), so
   a silent task whose Player move armed noisy LocationTrigger tasks -- Marooned
   On Mazoomah's `push radio` win -- counts their output. */
void
drain_tasks_to_run (a5_run_t *run, sb_t *out)
{
  size_t guard = 0;
  while (!run->tasks_to_run->empty () && guard++ < 256)
    {
      std::string key = run->tasks_to_run->front ();
      run->tasks_to_run->erase (run->tasks_to_run->begin ());
      attempt_event_task (run, key.c_str (), 0, out);
    }
}

/* clsUserSession.TurnBasedStuff: tick every turn-based event once. */
void
ev_tick_all (a5_run_t *run, sb_t *out)
{
  int ei;
  /* clsUserSession.ProcessTurn drains qTasksToRun after the command's task,
     before TurnBasedStuff; do the same here (ev_tick_all is the turn's only
     TurnBasedStuff entry). */
  drain_tasks_to_run (run, out);
  /* The command's AggregateOutput-completion random draws (held in display_defers)
     resolve at the end of the runner's command-task AttemptToExecuteTask -- i.e. after the
     LocationTrigger drain (so a drained task's draw lands first, Skybreak's
     SidequestE) but BEFORE TurnBasedStuff (so the completion's own draw precedes
     the per-turn walk/event draws, I Summon Thee's `annihilate` Annihilateflavor
     index).  Flush here rather than at finish_turn, which is past the event tick. */
  a5run_flush_display_defers (run, out);
  if (run->st->game_over)
    return;
  /* TurnBasedStuff: tick the walks first, then the turn-based events. */
  wk_tick_all (run, out);
  run->events_running = 1;
  for (ei = 0; ei < run->adv->n_events && !run->st->game_over; ei++)
    if (streq (run->adv->events[ei].type, "TurnBased")
        || run->adv->events[ei].type == NULL)
      ev_increment (run, ei, out);
  /* Separate loop: an event started by a later event must not also tick. */
  for (ei = 0; ei < run->adv->n_events; ei++)
    (*run->events)[ei].just_started = 0;
  run->events_running = 0;
}

/* clsUserSession.TimeBasedStuff, made deterministic: the real ADRIFT Runner
   ticks TimeBased events off a wall-clock 1-second timer (tmrEvents_Tick);
   this port has no real-time clock, so both it and the FrankenDrift.Headless
   reference tick TimeBased events exactly ONCE at the end of every processed
   input line (one turn == one second).  Real-time games (The Salvage's
   mission/refuel/end-game events) stay playable AND turn-deterministic.
   Mirrors FD's TimeBasedStuff: events only (no walk tick, no qTasksToRun
   drain), and only TimeBased events' just_started flags are cleared. */
void
ev_time_tick_all (a5_run_t *run, sb_t *out)
{
  int ei;
  if (run->st->game_over)
    return;
  run->events_running = 1;
  for (ei = 0; ei < run->adv->n_events && !run->st->game_over; ei++)
    if (streq (run->adv->events[ei].type, "TimeBased"))
      ev_increment (run, ei, out);
  for (ei = 0; ei < run->adv->n_events; ei++)
    if (streq (run->adv->events[ei].type, "TimeBased"))
      (*run->events)[ei].just_started = 0;
  run->events_running = 0;
}

/* Game start: set each event's initial status (clsUserSession init loop). */
void
ev_init (a5_run_t *run, sb_t *out)
{
  int ei;
  run->events_running = 1;
  for (ei = 0; ei < run->adv->n_events; ei++)
    {
      const a5_event_t *e = &run->adv->events[ei];
      a5_event_rt &rt = (*run->events)[ei];
      switch (rt.when_start)
        {
        case 3:                                  /* AfterATask */
          rt.status = A5_EV_NOTYET;
          break;
        case 2:                                  /* BetweenXandYTurns */
          rt.status = A5_EV_COUNTDOWN;
          /* The runner: TimerToEndOfEvent = StartDelay.Value + Length.Value -- VB
             evaluates left-to-right, so StartDelay draws BEFORE Length.  Draw in
             that exact order or the two RNG values get swapped (desyncing the
             whole stream and every random timer downstream). */
          {
            long delay = a5rand_between (e->start_from, e->start_to);
            ev_set_timer_to_end (run, ei, delay + ev_roll_length (run, ei), out);
          }
          break;
        case 1:                                  /* Immediately */
          ev_lstart (run, ei, 0, out);
          break;
        }
    }
  for (ei = 0; ei < run->adv->n_events; ei++)
    (*run->events)[ei].just_started = 0;
  run->events_running = 0;
  /* Start the StartActive walks (clsUserSession game-start init loop). */
  wk_init (run, out);
}

/* =========================================================== walks (clsWalk) */

/* A faithful port of clsWalk's per-turn state machine (clsCharacter.vb:1175+):
   IncrementTimer / DoAnySteps / DoAnySubWalks / lStart / lStop, driven each turn
   by clsUserSession.TurnBasedStuff (before the events) and on task completion by
   the WalkControls.  Walks reuse the event Status/Command enums (no
   CountingDownToStart). */

static void wk_lstart   (a5_run_t *run, int wi, int restart, sb_t *out);
static void wk_lstop    (a5_run_t *run, int wi, int run_subs, int reached_end, sb_t *out);
static void wk_do_steps (a5_run_t *run, int wi, sb_t *out);
static void wk_do_subwalks (a5_run_t *run, int wi, sb_t *out);

static long wk_from_start (const a5_walk_rt &rt) { return rt.length - rt.timer_to_end; }
static long wk_from_last  (const a5_walk_rt &rt) { return wk_from_start (rt) - rt.last_sw_time; }

/* DirectionsEnum order (Global.vb:146) so DirectionTo is deterministic
   regardless of the XML <Movement> order. */
static const char *const DIR_ORDER[12] = {
  "North", "East", "South", "West", "Up", "Down", "In", "Out",
  "NorthEast", "SouthEast", "SouthWest", "NorthWest"
};

/* clsLocation.IsAdjacent: any exit of `lockey` whose destination is `destkey`. */
static int
loc_is_adjacent (a5_state_t *st, const char *lockey, const char *destkey)
{
  int d;
  if (lockey == NULL || destkey == NULL)
    return 0;
  for (d = 0; d < 12; d++)
    if (streq (a5model_exit_dest (st->adv, lockey, DIR_ORDER[d]), destkey))
      return 1;
  return 0;
}

/* clsLocation.DirectionTo: prose direction from `fromkey` to `destkey`.
   Prose per DIR_ORDER slot. */
static const char *const DIR_PROSE[12] = {
  "the north", "the east", "the south", "the west", "above", "below",
  "inside", "outside",
  "the north-east", "the south-east", "the south-west", "the north-west"
};
static std::string
loc_direction_to (a5_state_t *st, const char *fromkey, const char *destkey)
{
  int d;
  if (streq (fromkey, destkey))
    return "not moved";
  for (d = 0; d < 12; d++)
    if (streq (a5model_exit_dest (st->adv, fromkey, DIR_ORDER[d]), destkey))
      return DIR_PROSE[d];
  return "nowhere";
}

/* clsCharacter.GetPropertyValue for a character property: runtime override if
   set, else the static value -- which for a rich Text property is stored as a
   <Description> (value_node) and must be rendered.  Returns a malloc'd string
   when the property is *present* (HasProperty), or NULL when absent so the
   caller can apply its default.  Mirrors the runner's `If .HasProperty(k) Then s =
   .GetPropertyValue(k)`. */
static char *
char_prop_value (a5_state_t *st, const char *charkey, const char *propkey)
{
  const char *ov = a5state_entity_prop (st, charkey, propkey);
  if (ov != NULL)
    return strdup (ov);
  const a5_character_t *c = a5model_character (st->adv, charkey);
  if (c == NULL)
    return NULL;
  const a5_prop_t *p = a5_prop_find (c->props, c->n_props, propkey);
  if (p == NULL)
    return NULL;
  if (p->value_node != NULL)
    return a5text_eval_description (st, p->value_node);
  return strdup (p->value ? p->value : "");
}

/* "<Name> <CharExits/CharEnters-or-default>" -- the head of both ShowEnterExit
   messages. */
static std::string
wk_move_phrase (a5_state_t *st, const char *charkey, const char *propkey,
                const char *defverb)
{
  char *verb = char_prop_value (st, charkey, propkey);
  char *nm = a5text_char_proper_name (st, charkey);
  std::string s = std::string (nm) + " " + (verb ? verb : defverb);
  free (nm);
  free (verb);
  return s;
}

/* Process the composed message like any response the runner Displays -- a
   CharExits/CharEnters property may embed <#...#> expressions (DDF's
   wagon `<# OneOf("trundles away", ...) #>`). */
static void
wk_emit_move_msg (a5_state_t *st, const std::string &s, sb_t *out)
{
  char *proc = a5text_process (st, s.c_str ());
  char *plain = a5text_render_plain (proc);
  sb_pspace (out); sb_puts (out, plain);
  free (proc); free (plain);
}

/* The "ShowEnterExit" message a walking NPC shows when it enters or leaves the
   player's room (clsWalk.DoAnySteps).  Evaluated with the character's *current*
   (pre-move) location vs the player's, and the move destination `dest`. */
static void
wk_show_enter_exit (a5_run_t *run, int ci, const char *dest, sb_t *out)
{
  a5_state_t *st = run->st;
  const a5_character_t *c = &st->adv->characters[ci];
  const char *cloc = st->char_loc[ci];
  const char *ploc = a5state_player_location (st);

  /* The player's own walk never narrates this; only a ShowEnterExit NPC that is
     currently "at location" (not on/in an object). */
  if (ci == a5state_character_index (st, a5state_player_key (st)))
    return;
  if (a5_prop_find (c->props, c->n_props, "ShowEnterExit") == NULL
      && a5state_entity_prop (st, c->key, "ShowEnterExit") == NULL)
    return;
  if (st->char_onobj[ci] != NULL || cloc == NULL)
    return;

  if (streq (cloc, ploc))                      /* leaving the player's room */
    {
      std::string s = wk_move_phrase (st, c->key, "CharExits", "exits");
      if (ploc != NULL && loc_is_adjacent (st, ploc, dest))
        {
          std::string dir = loc_direction_to (st, cloc, dest);
          if (dir != "nowhere")
            {
              s += (dir == "outside" || dir == "inside") ? " " : " to ";
              s += dir;
            }
        }
      s += ".";
      wk_emit_move_msg (st, s, out);
    }
  else if (streq (dest, ploc))                 /* entering the player's room */
    {
      std::string s = wk_move_phrase (st, c->key, "CharEnters", "enters");
      if (ploc != NULL && loc_is_adjacent (st, ploc, cloc))
        {
          std::string dir = loc_direction_to (st, dest, cloc);
          if (dir != "nowhere")
            s += " from " + dir;
        }
      s += ".";
      wk_emit_move_msg (st, s, out);
    }
}

/* clsWalk.DoAnySteps: when the from-start timer reaches a step's cumulative
   offset, move the character toward that step's destination (a location, a
   random adjacent member of a location group, or toward another character). */
static void
wk_do_steps (a5_run_t *run, int wi, sb_t *out)
{
  a5_state_t *st = run->st;
  a5_walk_rt &rt = (*run->walks)[wi];
  const a5_walk_t *wk = rt.walk;
  int ci = rt.char_index;
  long step_len = 0;
  int si;
  /* A *structurally* zero-length looping walk -- the standard "follow the player"
     walk (a lone `Player 0` step, Loops; every step's turn count is exactly 0) --
     can never restart: wk_lstop's `length > 0` guard (a faithful port of the runner's,
     which avoids the infinite lStart<->lStop recursion a 0-length restart would
     cause) leaves it Finished immediately after its lStart.  DoAnySteps' plain
     `Status = Running` gate then means the lone step never fires again and the
     walker never moves at all -- so these walks are dead in the real ADRIFT
     Runner too, not just in FrankenDrift (Son of Camelot's Megan never follows to
     Merlin's grave, so the game is unwinnable; fix offered upstream as
     jcwild/ADRIFT-5#16).  Everything else is already in place for them to work:
     IncrementTimer calls DoAnySteps each tick regardless of status, and with
     length 0 from_start stays 0, so the step matches every tick.  Let a Finished
     structural-0 loop through so its follow step fires each turn.  Test the
     *model* step counts, not the runtime rt.length: a
     normal patrol walk STOPPED before it ever started (e.g. Fortress of Fear's
     Custodian, whose control stops one of three alternative patrols at init) is
     also Finished with rt.length still 0 (never lStart'd), but its steps carry
     real durations -- it must stay stuck, not teleport to its first step. */
  int struct_zero = wk->n_steps > 0;
  for (si = 0; si < wk->n_steps; si++)
    if (wk->steps[si].ft_from != 0 || wk->steps[si].ft_to != 0)
      { struct_zero = 0; break; }
  if (rt.status != A5_EV_RUNNING
      && !(rt.status == A5_EV_FINISHED && wk->loops && struct_zero))
    return;
  for (si = 0; si < wk->n_steps; si++)
    {
      if (step_len == wk_from_start (rt))
        {
          const char *destkey = wk->steps[si].location;
          const char *cloc = st->char_loc[ci];
          const char *dest = NULL;                  /* sDestination */
          int is_group = 0, gn = 0;
          int g, c2;
          if (streq (destkey, "%Player%"))
            destkey = a5state_player_key (st);
          for (g = 0; g < st->adv->n_groups; g++)
            if (streq (st->adv->groups[g].key, destkey))
              { is_group = 1; break; }
          if (is_group)
            gn = a5state_group_count (st, destkey);  /* live members (arlMembers) */

          if (is_group && gn > 0)
            {
              int has_adj = 0, m;
              if (cloc != NULL)
                for (m = 0; m < gn; m++)
                  if (loc_is_adjacent (st, cloc,
                                       a5state_group_member_at (st, destkey, m)))
                    { has_adj = 1; break; }
              if (has_adj)
                {
                  /* DO NOT "optimise" this into a single draw.  clsWalk picks a
                     random group member and, when at least one member is
                     adjacent, re-rolls until the draw lands on an adjacent room
                     -- and FrankenDrift consumes exactly these rejected draws.
                     A code review once flagged the rejection loop as an RNG
                     desync / efficiency bug and replaced it with one
                     a5rand_between + an adjacency gate; that shifted the shared
                     draw stream and regressed AlienDiver (Crafting Fragments
                     5/15 -> 3/15, then the walkthrough desynced).  Confirmed
                     against FrankenDrift's xoshiro-aligned stream
                     (FD_RNG=xoshiro test/a5_groundtruth.sh AlienDiver): the
                     reference makes the same multi-draw sequence, so matching it
                     REQUIRES re-rolling here.  AlienDiver is the canary in
                     test/run_a5_walkthroughs.sh.  The guard only bounds a
                     pathological group with no reachable adjacent member. */
                  int guard = 0;
                  while (dest == NULL && guard++ < 10000)
                    {
                      const char *poss = a5state_group_member_at (st, destkey,
                                             a5rand_between (0, gn - 1));
                      if (cloc == NULL || loc_is_adjacent (st, cloc, poss))
                        dest = poss;
                    }
                }
              else
                dest = a5state_group_member_at (st, destkey,
                                                a5rand_between (0, gn - 1));
            }
          else if ((c2 = a5state_character_index (st, destkey)) >= 0)
            {
              /* Follow a character: only step in if it is in an adjacent room. */
              const char *c2loc = st->char_loc[c2];
              if (!streq (cloc, c2loc) && cloc != NULL && c2loc != NULL
                  && loc_is_adjacent (st, cloc, c2loc))
                dest = c2loc;
            }
          else
            dest = destkey;                          /* a literal location key */

          if (dest != NULL && (streq (dest, "Hidden")
                               || a5model_location (st->adv, dest) != NULL))
            {
              wk_show_enter_exit (run, ci, dest, out);
              st->char_loc[ci] = streq (dest, "Hidden") ? NULL : dest;
              st->char_onobj[ci] = NULL;             /* now "at location" */
            }
        }
      step_len += rt.step_dur[si];
    }
}

/* clsWalk.DoAnySubWalks: the sub-walk activities (DisplayMessage / ExecuteTask /
   UnsetTask), triggered by FromStart / FromLast / BeforeEnd offsets or by the
   ComesAcross transition (the walker entering the subject's room). */
static void
wk_do_subwalks (a5_run_t *run, int wi, sb_t *out)
{
  a5_state_t *st = run->st;
  a5_walk_rt &rt = (*run->walks)[wi];
  const a5_walk_t *wk = rt.walk;
  int ci = rt.char_index;
  int i;
  if (rt.status != A5_EV_RUNNING)
    return;
  for (i = 0; i < wk->n_subwalks; i++)
    {
      const a5_subwalk_t *sw = &wk->subwalks[i];
      long ft = rt.sw_ft[i];
      int go = 0;
      switch (sw->when)
        {
        case A5_SW_FROM_START:
          if (wk_from_start (rt) == ft && ft <= rt.length)
            go = 1;
          break;
        case A5_SW_FROM_LAST:
          if (wk_from_last (rt) == ft
              && ((rt.last_sw_index == -1 && i == 0)
                  || (i > 0 && rt.last_sw_index == i - 1)))
            go = 1;
          break;
        case A5_SW_BEFORE_END:
          if (rt.timer_to_end == ft)
            go = 1;
          break;
        case A5_SW_COMES_ACROSS:
          {
            const char *comekey = sw->come_key;
            int prev = rt.came_across[i], now, cc;
            if (streq (comekey, "%Player%"))
              comekey = a5state_player_key (st);
            cc = a5state_character_index (st, comekey);
            now = (cc >= 0 && st->char_loc[ci] != NULL
                   && streq (st->char_loc[ci], st->char_loc[cc]));
            rt.came_across[i] = (char) now;
            if (!prev && now)
              go = 1;
          }
          break;
        }
      if (!go)
        continue;
      switch (sw->what)
        {
        case A5_SW_DISPLAY:
          if (sw->description != NULL && sw->only_apply_at != NULL
              && sw->only_apply_at[0] != '\0'
              && a5state_in_group_or_location (st, a5state_player_key (st), sw->only_apply_at))
            /* Real output: retire <DisplayOnce> segments (October 31st's
               werewolf howl fires once, not every loop of the 1-step walk). */
            emit_marked_description (run, sw->description, out);
          break;
        case A5_SW_EXECTASK:
          if (sw->task_key != NULL)
            attempt_event_task (run, sw->task_key, 0, out);
          break;
        case A5_SW_UNSETTASK:
          if (sw->task_key != NULL)
            unset_task (st, sw->task_key);
          break;
        }
      rt.last_sw_time  = wk_from_start (rt);
      rt.last_sw_index = i;
    }
}

/* TimerToEndOfWalk setter (clsWalk): hitting 0 while running stops the walk
   (running its sub-walks) and loops it if set to loop. */
static void
wk_set_timer (a5_run_t *run, int wi, long value, sb_t *out)
{
  a5_walk_rt &rt = (*run->walks)[wi];
  rt.timer_to_end = value;
  if (rt.status == A5_EV_RUNNING && rt.timer_to_end == 0)
    wk_lstop (run, wi, 1, 1, out);                /* lStop(True, True) */
}

static void
wk_lstart (a5_run_t *run, int wi, int restart, sb_t *out)
{
  a5_walk_rt &rt = (*run->walks)[wi];
  const a5_walk_t *wk = rt.walk;
  int i;
  if (!(rt.status == A5_EV_NOTYET || rt.status == A5_EV_FINISHED
        || (rt.status == A5_EV_RUNNING && restart)))
    return;
  rt.status = A5_EV_RUNNING;
  /* ResetLength: re-roll each step's duration; Length = their sum.  (clsWalk
     does NOT reset LastSubWalk / iLastSubWalkTime or the sub-walk offsets.) */
  rt.length = 0;
  for (i = 0; i < wk->n_steps; i++)
    {
      rt.step_dur[i] = a5rand_between (wk->steps[i].ft_from, wk->steps[i].ft_to);
      rt.length += rt.step_dur[i];
    }
  wk_set_timer (run, wi, rt.length, out);
  if (wk_from_start (rt) == 0)
    { wk_do_steps (run, wi, out); wk_do_subwalks (run, wi, out); }
  rt.just_started = 1;
}

static void
wk_lstop (a5_run_t *run, int wi, int run_subs, int reached_end, sb_t *out)
{
  a5_walk_rt &rt = (*run->walks)[wi];
  if (run_subs)
    wk_do_subwalks (run, wi, out);
  rt.status = A5_EV_FINISHED;
  if (rt.walk->loops && rt.timer_to_end == 0 && reached_end && rt.length > 0)
    wk_lstart (run, wi, 1, out);                  /* restart a looping walk */
}

static void
wk_increment (a5_run_t *run, int wi, sb_t *out)
{
  a5_walk_rt &rt = (*run->walks)[wi];
  if (rt.next_command != A5_CMD_NONE)
    {
      switch (rt.next_command)
        {
        case A5_CMD_START:   wk_lstart (run, wi, 0, out); break;
        case A5_CMD_STOP:    wk_lstop  (run, wi, 0, 0, out); break;
        case A5_CMD_PAUSE:   if (rt.status == A5_EV_RUNNING) rt.status = A5_EV_PAUSED; break;
        case A5_CMD_RESUME:  if (rt.status == A5_EV_PAUSED)  rt.status = A5_EV_RUNNING; break;
        case A5_CMD_RESTART: wk_lstart (run, wi, 1, out); break;
        }
      rt.next_command = A5_CMD_NONE;
      rt.triggering_task.clear ();
    }
  if (rt.status == A5_EV_RUNNING && !rt.just_started)
    wk_set_timer (run, wi, rt.timer_to_end - 1, out);
  if (!rt.just_started)
    { wk_do_steps (run, wi, out); wk_do_subwalks (run, wi, out); }
  rt.just_started = 0;
}

/* Defer a control's Start/Stop/etc. to the next IncrementTimer (clsWalk.Start /
   Stop / Pause / Resume all set NextCommand; a Start after a pending Stop becomes
   a Restart). */
static void
wk_control (a5_run_t *run, int wi, a5_ctrl_t ctrl, const char *task_key)
{
  a5_walk_rt &rt = (*run->walks)[wi];
  int cmd = ctrl_to_cmd (ctrl);
  if (cmd == A5_CMD_START && rt.next_command == A5_CMD_STOP)
    cmd = A5_CMD_RESTART;
  rt.next_command = cmd;
  rt.triggering_task = task_key ? task_key : "";
}

/* clsUserSession (bPass): fire any WalkControls keyed on a completing task.
   Runs before the EventControls (clsUserSession loops walks then events). */
static void
wk_on_task_completed (a5_run_t *run, const char *task_key)
{
  size_t wi;
  if (task_key == NULL)
    return;
  for (wi = 0; wi < run->walks->size (); wi++)
    {
      a5_walk_rt &rt = (*run->walks)[wi];
      const a5_walk_t *wk = rt.walk;
      int ci;
      for (ci = 0; ci < wk->n_controls; ci++)
        {
          const a5_eventctrl_t *c = &wk->controls[ci];
          if (!c->on_completion || !streq (c->task_key, task_key))
            continue;
          if (ctrl_retrigger_blocked (run->adv, rt.triggering_task, task_key))
            continue;
          wk_control (run, (int) wi, c->control, task_key);
        }
    }
}

/* clsUserSession.TurnBasedStuff: tick every walk once (before the events). */
static void
wk_tick_all (a5_run_t *run, sb_t *out)
{
  size_t wi;
  if (run->st->game_over)
    return;
  for (wi = 0; wi < run->walks->size () && !run->st->game_over; wi++)
    wk_increment (run, (int) wi, out);
}

/* Game start: start the StartActive walks (clsUserSession init loop). */
static void
wk_init (a5_run_t *run, sb_t *out)
{
  size_t wi;
  for (wi = 0; wi < run->walks->size (); wi++)
    if ((*run->walks)[wi].walk->start_active)
      wk_lstart (run, (int) wi, 0, out);
  for (wi = 0; wi < run->walks->size (); wi++)
    (*run->walks)[wi].just_started = 0;
}
