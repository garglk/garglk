/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- private shared declarations for the turn-loop
 * translation units (a5run.cpp and the a5run_*.cpp it was split into).  Holds
 * the runtime-state struct (a5_run_s) and the small types/helpers those files
 * pass between one another.  NOT a public interface -- host code uses a5run.h,
 * which keeps a5_run_t opaque.
 */

#ifndef SCARIER_A5RUN_INTERNAL_H
#define SCARIER_A5RUN_INTERNAL_H

#include <string.h>

#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "a5parse.h"
#include "a5run.h"
#include "a5sb.h"
#include "a5util.h"

/* Per-event runtime (clsEvent instance state).  Status mirrors
   clsEvent.StatusEnum; the timers mirror TimerToEndOfEvent / iLastSubEventTime /
   bJustStarted / NextCommand.  Random ranges (Length / sub-event offsets) are
   resolved per run via a5rand_between (a from==to range draws no RNG). */
enum { A5_EV_NOTYET = 0, A5_EV_RUNNING = 1, A5_EV_COUNTDOWN = 2,
       A5_EV_PAUSED = 3, A5_EV_FINISHED = 4 };
enum { A5_CMD_NONE = 0, A5_CMD_START = 1, A5_CMD_STOP = 2,
       A5_CMD_PAUSE = 3, A5_CMD_RESUME = 4, A5_CMD_RESTART = 5 };
struct a5_event_rt {
  int  status;
  long length_value;       /* resolved Length for this run                   */
  int  length_set;         /* Length (a FromTo) has been evaluated at least
                              once -- FromTo.Value is lazy (Global.vb:3770),
                              so an event that never started still resolves
                              its Length the first time anything asks        */
  long timer_to_end;
  long last_se_time;       /* iLastSubEventTime                              */
  int  last_se_index;      /* -1 = none (LastSubEvent Is Nothing)            */
  int  just_started;
  int  next_command;
  int  when_start;         /* mutable: Immediately -> BetweenXY after start  */
  std::string triggering_task;
  std::vector<long> se_ft; /* resolved per-sub-event turn offset             */
};

/* Per-walk runtime (clsWalk instance state).  Walks share the event status /
   command enums; CountingDownToStart is unused (walks have no StartDelay).  The
   resolved step durations (Length) re-roll on each lStart; the sub-walk offsets
   are resolved once (clsWalk never resets them). */
struct a5_walk_rt {
  const a5_walk_t *walk;
  int  char_index;         /* owning character index in adv->characters       */
  int  status;
  long length;             /* sum of resolved step durations (Length)         */
  long timer_to_end;       /* TimerToEndOfWalk                                 */
  long last_sw_time;       /* iLastSubWalkTime                                 */
  int  last_sw_index;      /* -1 = none (LastSubWalk Is Nothing)              */
  int  just_started;
  int  next_command;
  std::string triggering_task;
  std::vector<long> step_dur;     /* resolved per-step duration               */
  std::vector<long> sw_ft;        /* resolved per-sub-walk offset             */
  std::vector<char> came_across;  /* per-sub-walk ComesAcross prev-same-loc   */
};

/* ---------------------------------------- per-command response aggregation
   The collector types live here because a5run_action.cpp and a5run_resp.cpp
   both need them: the action layer builds and inspects entries while the
   resp layer merges and flushes them.  a5_run_s below holds a resp_map *. */

/* ----------------------------------------- per-command response aggregation */

/* The Adrift 5 runner's clsUserSession collects every task completion / restriction-
   fail message of a single top-level command into two ordered, message-keyed
   maps (htblResponsesPass / htblResponsesFail) and flushes them once at the end
   (AttemptToExecuteTask, clsUserSession.vb:782-863).  Two behaviours fall out:

     * a plural %objects% reference is run one item at a time (ExecuteSubTasks,
       vb:695), so each item independently selects its Specific overrides; and
     * identical messages dedup, while an AggregateOutput task's raw template is
       the key so two items through the same task collapse to one entry whose
       %objects% list grows (AddResponse, vb:1295) -- e.g. "the tiara and the
       shoes".  A pass for an item drops that item's earlier fail message.

   Scarier mirrors this only on the plural path (run_general); the single-
   reference path keeps writing straight to `out`. */
/* Snapshot of the per-turn reference bindings, captured when a deferred
   (AggregateOutput) message is queued and restored before it is re-rendered at
   flush.  The Adrift 5 runner stores the message's NewReferences alongside it and
   reassigns them at Display (clsUserSession.vb:851-854), so the deferred
   "%object%.Description"/"(to %character%)"/"%direction%" tokens resolve against
   the entity the command referenced -- not whatever the binding table holds by
   the end of the command. */
struct ref_snap {
  char ref_name[16][32];
  char ref_value[16][A5_REFVAL_LEN];
  int  n_refbind;
  int  ref_object1_plural;
  int  ref_character1_plural;
};

/* ref_snap mirrors a5_state_t's binding table, and ref_snap_take/restore memcpy
   with the DESTINATION size -- so widening either dimension in a5state.h would
   silently truncate every deferred message's restored references (or overread)
   rather than fail to build.  Keep the two in lockstep here. */
static_assert (sizeof (ref_snap::ref_name) == sizeof (a5_state_t::ref_name),
               "ref_snap::ref_name must mirror a5_state_t::ref_name exactly");
static_assert (sizeof (ref_snap::ref_value) == sizeof (a5_state_t::ref_value),
               "ref_snap::ref_value must mirror a5_state_t::ref_value exactly");

struct resp_entry {
  bool is_pass;
  bool aggregate;                 /* key is comp ptr; render deferred to flush  */
  bool is_look;                   /* deferred room view: render_look_string()   */
  bool has_snap;                  /* snap holds the refs to restore at flush    */
  const a5_xml_node_t *comp;      /* aggregate: re-rendered at flush            */
  const a5_xml_node_t *fail_comp; /* fail: the restriction <Message> node       */
  std::string rendered;           /* non-aggregate / fail: final text           */
  std::string key;                /* dedup key: the still-MARKED render (below)  */
  std::vector<std::string> obj_keys;  /* merged %objects% items (aggregate)     */
  std::string obj2;               /* %object2% snapshot (first occurrence)      */
  std::string item;               /* the single item this entry was made for    */
  ref_snap snap;
  resp_entry () : is_pass (false), aggregate (false), is_look (false),
                  has_snap (false), comp (NULL), fail_comp (NULL) {}
};

/* Capture / restore the live reference-binding table (a5state_bind_ref store +
   the plural-derived flags), so a deferred message renders with the references
   the command had when it produced the message. */

struct resp_map {
  std::vector<resp_entry> ents;
  std::string cur_item;           /* item currently being iterated              */
  size_t nmut = 0;                /* count of real adds AND aggregate merges     */
};

/* Runaway guard for `SetTasks Execute` chains (a5run_action.cpp act_set_tasks).
   The runner's ExecuteTask is a full re-entrant AttemptToExecuteTask, so a task
   whose actions Execute a second task that Executes the first recurses forever
   when both are Repeatable (the Completed gate that stops the non-repeatable
   case never trips).  a5run_action's other recursion limits cannot catch this:
   its `depth` counter is deliberately RESET at each Execute -- the sub-task is a
   fresh AttemptToExecuteTask, and its own override nesting starts at 0 -- so the
   Execute chain needs a counter of its own.  The deepest real nesting measured
   over the 116-game walkthrough corpus is 7 (The Royal Puzzle; 45 games peak at
   2, none of the rest above 4), so decline the re-dispatch an order of magnitude
   above that rather than overflow the C stack -- a synthetic pair of mutually
   Executing Repeatable tasks used to SIGSEGV here.  (What the real runner does
   on such data is untested; .NET would recurse too, but nothing in the corpus
   exercises it, so this bound is a crash guard, not a fidelity claim.) */
#define A5_MAX_EXEC_DEPTH 64

struct a5_run_s {
  const a5_adventure_t *adv;
  a5_state_t *st;
  /* Embedded-media events captured from this turn's rendered text (a5text media
     sink); rebuilt each a5run_intro / a5run_input.  See a5run_media_* . */
  std::vector<a5_media_event_t> *media;
  std::vector<int> *order;   /* task indices, ascending priority */

  /* The Adrift 5 runner's per-top-level-command response aggregation layer
     (clsUserSession htblResponsesPass/Fail).  Non-NULL only while a plural
     %objects% command iterates its items (run_general): completion + override-
     fail messages are collected here keyed by message/template, deduped and
     ref-merged, then flushed once at the end (mirroring AttemptToExecuteTask's
     final Display loop).  NULL on the single-reference path, so non-plural
     commands keep the byte-identical direct-to-`out` behaviour. */
  resp_map *resp;

  /* Event runtime, one slot per adv->events (clsEvent instances). */
  std::vector<a5_event_rt> *events;

  /* Walk runtime, one slot per character walk (flattened over all characters). */
  std::vector<a5_walk_rt> *walks;
  int events_running;        /* clsUserSession.bEventsRunning (immediate vs
                                deferred Start/Stop on control triggers)      */
  sb_t *event_out;           /* sink for sub-event output during a tick       */

  /* Pending general-reference disambiguation (clsUserSession sAmbTask /
     NewReferences).  When amb_active, the previous turn matched a task whose
     reference resolved to several in-scope candidates; the next input is first
     tried as a clarifier (narrowing amb_keys) before being re-parsed as a fresh
     command. */
  int amb_active;
  int amb_task_index;        /* the remembered task to re-run                  */
  int amb_command_index;     /* which of its command patterns matched          */
  std::string amb_input;     /* the original proper input (sLastProperInput)   */
  std::string amb_ref_name;  /* the ambiguous reference ("object1", ...)       */
  char amb_ref_type;         /* 'o' object / 'c' character                     */
  std::string amb_word;      /* the noun echoed in "Which <word>?"             */
  std::vector<std::string> amb_keys;   /* the surviving candidate keys         */

  /* clsUserSession.sRememberedVerb: a bare verb that matched a "verb %ref%"
     command but supplied no reference prompted "<Verb> what/who/where?"; the
     next input is re-tried as "<verb> <input>" (NotUnderstood, vb:3471). */
  std::string remembered_verb;

  /* Every word the game recognises (clsAdventure.listKnownWords), built lazily
     the first time an unmatched command needs the "I did not understand the
     word ..." check (clsUserSession.NotUnderstood). */
  std::set<std::string> *known_words;

  /* Set by resolve_refine when a "get all"-style command resolved its %objects%
     reference to no passing item: the matched task's <FailOverride> to show
     instead of a restriction message ("There is nothing worth taking here." --
     clsUserSession.vb:788, shown only when the input contains "all").  Cleared
     each resolve. */
  const a5_xml_node_t *pending_failover;

  /* Set by resolve_plural when a %objects% NOUN matched more than one object and
     none passed the restrictions: the Adrift 5 runner then treats the multi-match as
     ambiguous and prefixes the task's failure with its "ReferencedObjects Must
     Exist" message ("Sorry, I'm not sure which object you are trying to take.").
     The RR_FAIL consumer prepends this ahead of the restriction message and
     clears it.  Empty when there is no such ambiguity. */
  std::string plural_amb_prefix;

  /* clsAdventure.qTasksToRun: System tasks armed by a Player move into their
     <LocationTrigger> (clsCharacter.Move); drained after the turn's task runs,
     before TurnBasedStuff (clsUserSession.vb:3421). */
  std::vector<std::string> *tasks_to_run;

  /* Deferred room-view render.  The stock Look task's CompletionMessage is
     "%Player%.Location.Description" with no <Aggregate> => AggregateOutput=True,
     so the runner defers its function replacement to final Display (clsUserSession.vb:
     1184/1210): the room view reflects state changed by AfterText/Actions
     children that run after the parent's `Execute Look`.  When defer_look is set
     (a parent with After children runs on the direct path), emit_look records
     the insertion offset instead of rendering; the view is then rendered once --
     at final state -- and spliced in at that offset, ahead of the children's own
     output.  Grandpa's Ranch `go west`: vnl_GoWestBust (AfterTextAndActions)
     moves Buster, so the new room must list him "in the western enclosure". */
  int    defer_look;
  int    look_pending;
  size_t look_pos;

  /* Index of the task whose <Actions> are currently executing (clsUserSession
     ExecuteSingleAction's `task` param), or -1.  Used to gate the Score variable
     so a task can only modify Score once (clsTask.Scored, vb:2144). */
  int    cur_score_ti;

  /* Nesting depth of the `SetTasks Execute` chain currently running, bounded by
     A5_MAX_EXEC_DEPTH (see above).  Bumped by act_set_tasks around each
     re-dispatch; 0 outside one. */
  int    exec_depth = 0;

  /* When the Look dance's two test renders differ (a random pick in the room
     view moved between them -- clsUserSession.vb:1200), the runner pins the response to
     the FIRST render instead of deferring the raw aggregate message to Display.
     This carries that pinned text to the deferred-splice site (owned; NULL =
     not pinned, render at final state as usual). */
  char  *look_pinned;

  /* Set only while the System <RunImmediately> startup tasks run (before the
     title).  emit_completion then uses the runner's bHasOutput (markup-aware) instead of
     the plain whitespace test, so a title-music task's `<audio ...> ` keeps its
     trailing space to join onto the centred title.  Off everywhere else, so the
     per-turn completion-message emission is byte-identical to before. */
  int    immediate_emit;

  /* Immediate-Display splice point (clsUserSession Display(sRestrictionText),
     vb:1748): an action that Displays DIRECTLY -- today only a failed
     MoveCharacter InDirection's route-restriction message -- goes to the
     runner's sOutputText ahead of the task's buffered response, so on the flat
     path run_task records where this task's Before message begins and the
     action splices its text THERE instead of appending.  imm_sink NULL = no
     splice point (resp/Look paths); the action then appends at its own
     position, which already precedes an After message. */
  sb_t  *imm_sink = NULL;
  long   imm_pos = -1;

  /* Sticky per-After-children-scan flag: some completion message evaluated
     since the caller cleared it had a NON-BLANK raw template -- the runner's
     AddResponse bHasOutput test, which counts a whitespace-only "\n" template
     as task output even though the rendered plain text trims to nothing.  The
     After-children stop rule reads it alongside buffer growth (The Salvage's
     station-known "\n" child must stop the scan before the fuel task). */
  int    task_raw_output;

  /* Depth of event/walk/LocationTrigger-fired task attempts currently running
     (attempt_event_task_impl).  Inside such an attempt the runner's end-of-attempt
     Display loop re-assigns the ambient NewReferences to the LAST-inserted
     response entry's reference items (clsUserSession.vb:851-856) -- so a
     SetTasks-Execute over a group whose members share one response entry
     (identical raw output, incl. a whitespace-only completion, which the runner's
     bHasOutput counts) leaves a PLURAL leftover that the NEXT event task's
     CopyNewRefs iterates once per item.  Quest Giver's per-turn daz6CheckIfAny4
     sweep over the active-quest group is the canonical case: its blank "\n\n"
     completion merges every active quest into one response, and daz61DaylightC
     (0 declared refs) then decrements daylight once per quest.

     Because the ambient only changes AT the Display (not while the attempt's
     own actions still run -- they read the attempt-entry InReferences copy),
     the winner is collected in the ev_lo stash below and installed into
     st->ref_items only when the attempt finishes.  All of it is gated on this
     flag, so the command path (view cards' per-member card printing) is
     untouched. */
  int    in_ev_attempt;

  /* The attempt's ordered response table (the runner's htblResponsesPass, one
     per AttemptToExecuteTask): every completion with a non-blank RAW template
     (bHasOutput) inserts an entry keyed by its CompletionMessage node, and a
     repeat of the same completion MERGES (that is also what the ev_seen text
     dedup drops from the output).  act_set_tasks' event group-Execute attaches
     each member to the entry its run last touched, so the final entry's
     accumulated members are the attempt's post-Display leftover.  Installed by
     attempt_event_task (stack-allocated per attempt, nested attempts isolated);
     NULL on every command path. */
  struct ev_resp_tbl *ev_tbl;

  /* Set while the once-per-input TimeBased tick (ev_time_tick_all) and its
     finish_turn run.  The runner's TimeBasedStuff FLUSHES the tick's own output
     (Display("", True)) before CheckEndOfGame emits the banner as a fresh
     commit, so no pSpace/blank-line top-up separates them -- The Salvage's
     "Daza.\n*** You have won ***".  emit_endgame skips its separator top-up
     under this flag. */
  int    in_time_tick;

  /* Real-time mode (interactive Glk hosts): the host drives TimeBased events
     from a wall-clock 1-second timer (a5run_time_tick), like the real Runner's
     tmrEvents_Tick, so a5run_input skips its deterministic once-per-input
     substitute tick.  Off by default -- headless harnesses and determinism
     mode keep the reproducible one-turn-one-second model. */
  int    real_time;

  /* clsUserSession htblResponsesPass dedup for an event-fired task chain.  The runner
     runs an event's ExecuteTask through AttemptToExecuteTask, which keys every
     completion message by its rendered text and shows each once (vb:783/1295).
     Non-NULL only while an outermost event task and its SetTasks-Execute subtree
     run (installed in attempt_event_task), so a task reached twice within one
     event contributes its message once -- e.g. Pathway to Destruction's Task5
     "The End" banner, executed by both Task33 and its parent Task30.  On the
     direct command path the plural/movement resp_map already covers this; on the
     single-command path there is never a same-turn duplicate. */
  std::set<std::string> *ev_seen;

  /* clsUserSession htblResponsesFail for a SetTasks-Execute'd task whose
     restrictions fail WITH a message.  The runner's ExecuteTask is a full
     AttemptToExecuteTask: the failing restriction's sRestrictionText is
     AddResponse'd (bPass=False), deduped by text, and displayed at the
     response flush ONLY when no pass response carries the same reference
     items (the pass-cancels-fail rule, clsUserSession.vb:804-849) -- in
     either order, since the cancellation happens at flush.  Examples (TBN):
       * dark-room `get dirt`: cl_TakeCharac passes silently, both its
         `Execute cl_TakeObject/TakeObjects (%objects%)` fail on the
         LightSources restriction -> "It is too dark to find the dirt."
         shown once (dedup);
       * `get grapnel`: the take override passes for the hooked grapnel (and
         hides it), the follow-up `Execute TakeObjects` fails Visible on the
         SAME object -> cancelled (pass after fail: Grandpa's flashlight,
         where TakeFromLazy fails "not on or inside another object!" BEFORE
         the plain take passes).
     So the direct path buffers Execute-fail messages here (with the object
     keys they were evaluated against) and flushes the survivors at the end
     of the scope (run_general / attempt_event_task).  When the plural/
     movement resp_map is active it handles fail responses itself instead. */
  struct exec_resp_scope *exec_scope;

  /* Deferred-completion sink pointer for the currently running command.  On the
     single-reference path this points at `display_defers` (below); a random draw
     inside an AggregateOutput completion message -- either a `<#OneOf/Rand#>`
     expression or a `%var[rand()]%` model-variable index -- is rendered as a
     `\004<idx>\004` sentinel and its body pushed to the sink, then drawn + spliced
     back at end-of-turn Display (a5run_flush_display_defers).  Also armed for
     each event/walk/LocationTrigger-fired task attempt (attempt_event_task),
     whose entries flush at the end of that attempt instead (the runner's
     bChildTask=False Display, a5run_flush_display_defers_from).  NULL on the
     plural/movement (resp map) path and outside a command. */
  std::vector<std::string> *comp_defers;

  /* Persistent per-turn Display-deferral sink (owned; cleared each finish_turn).
     Holds AggregateOutput-completion draws whose evaluation the runner defers to the
     final Display loop -- i.e. AFTER the command's task, the LocationTrigger
     drain (drain_tasks_to_run) and the event tick.  Skybreak's dock: StorylineL1's
     `After` completion ends in %DisplayLocation% (the `%flavorskybreak[rand]%`
     draws); The runner holds those to Display so the Skybreak location-trigger task's
     `SidequestE = RAND(1,10)` draws first.  Flushing here (not at run_general's
     end) reproduces that interleave.  Each body is either a frozen `<#..#>` sexpr
     or, tagged with a leading \001, a raw `%var[rand()]%` token re-resolved at
     flush. */
  std::vector<std::string> *display_defers;

  /* Undo stack: serialised a5run_save snapshots of the state as it stood BEFORE
     each of the last few processed turns, oldest first (empty until the first
     turn is snapshotted; each undo pops one entry, so repeated undo walks back
     turn by turn until the stack drains).  Pushed by a5run_snapshot from the
     frontend just before a5run_input; a5run_undo restores the newest.  Depth is
     capped in a5run_snapshot (A5_UNDO_DEPTH) to match the v4 engine's memo ring
     -- a deliberate superset of the Adrift 5 runner's one-deep undo. */
  std::vector<std::string> undo_stack;

  /* The final composed text of the last processed turn (the runner's
     clsUserSession sTurnOutput), and each undo snapshot's copy of it as it
     stood when that snapshot was pushed (undo_turn_text parallels undo_stack).
     A post-game-over UNDO replays it -- the runner's Undo() Displays "Undone."
     followed by the restored session's sTurnOutput. */
  std::string last_turn_text;
  std::vector<std::string> undo_turn_text;

  /* Size of the last a5run_save_blob result: pre-sizes the next blob's buffer
     so the per-turn undo snapshot skips the double-and-copy realloc ladder. */
  size_t save_blob_hint = 0;

  /* Cached pre-order DOM node list of adv->doc (immutable after load) plus a
     node->index map, built lazily once and reused by save_scarier_body /
     restore_scarier_body to map <DisplayOnce> segments to and from their stable
     index.  adv->doc never changes during a run, so this avoids a full DOM walk
     (and, on save, an O(n_disp_once x nodes) linear pointer search) on every
     per-turn undo snapshot.  Owned; NULL until first use, freed in a5run_free. */
  std::vector<const a5_xml_node_t *> *dom_nodes = NULL;
  std::unordered_map<const void *, int> *dom_index = NULL;
};

/* a5run_sort.cpp: the .NET introspective sort (clsTask.Children /
   TaskPrioritySortComparer).  Sorts task INDICES into adv->tasks by Priority,
   reproducing .NET's unstable introsort exactly -- equal-priority overrides must
   land where the runner leaves them, not where a stable sort would. */
extern void net_introspective_sort (const a5_adventure_t *adv,
                                    std::vector<int> &keys);

/* One event/walk/LocationTrigger attempt's ordered response table (see ev_tbl
   above) -- just enough of the runner's htblResponsesPass to know which entry
   was inserted LAST and which reference items merged into each entry.  An entry
   is keyed by its CompletionMessage node PLUS the eagerly-evaluated raw branch
   text (the runner keys by the per-subtask CompletionMessage.ToString: the
   restriction-gated branch is selected per item, and only identical raw texts
   merge -- Quest Giver's daz61DaylightC shows "Is is now late afternoon.." for
   the item that lands the counter on 5 AND the comment branch for the next
   item, two separate responses).  `touches` counts every insert-or-merge so
   act_set_tasks can tell whether a group member's run reached the table at
   all; `last` is the entry it reached. */
struct ev_resp_tbl {
  std::vector<const void *> keys;
  std::vector<std::string> texts;
  std::vector<std::vector<std::string>> refs;
  int touches = 0;
  int last = -1;
};

/* One SetTasks-Execute response scope (see exec_scope above). */
struct exec_resp_scope {
  std::set<std::string> pass_refs;   /* object keys of pass responses w/ output */
  std::set<std::string> fail_seen;   /* text dedup (htblResponsesFail keying)   */
  /* Pass-response text dedup (htblResponsesPass keying): the runner shows each
     distinct response once per command, so a command that Executes two tasks
     with the same completion text -- or `Execute Look` twice -- prints it
     once.  Euripides' `press on` runs cl_ToCrawler11 AND cl_ToCrawler12
     (identical journey paragraphs), each ending in `Execute Look`. */
  std::set<std::string> pass_seen;
  /* buffered fail messages, in order: text + the object keys bound when the
     restriction failed.  Empty key set = a ref-less fail: cancelled iff any pass
     response occurred this command (the runner's bAllMatch stays True vs the first pass),
     else shown. */
  std::vector<std::pair<std::string, std::vector<std::string>>> fails;
  /* Specific-override restriction fails, buffered on the DIRECT (single-ref)
     path with the task's POSITIONAL reference vector.  The runner's flush
     (clsUserSession.vb:804-834) drops a fail only when some pass response's
     refs match it POSITIONALLY over the fail's full length -- a 2-ref fail
     (`get ashes` re-dispatched as TakeObjectsFromOthers [ashes, firepit])
     survives a 1-ref pass ("(from the firepit)" note, [ashes]), while AoK's
     `show talisman` 1-ref fail [talisman] is cancelled by the 1-ref passing
     override [talisman]. */
  std::vector<std::pair<std::string, std::vector<std::string>>> ov_fails;
  /* Positional ref signatures of every task frame that produced pass output on
     the direct path (execute_task_with_overrides), for the ov_fails rule. */
  std::vector<std::vector<std::string>> pass_sigs;
  /* The single aggregate room-view response slot (the runner htblResponsesPass keyed
     on the stock Look's RAW template): where this command's first direct-path
     `Execute Look` view landed in its sink.  A LATER Execute Look whose view
     text differs (the player moved between them -- The Salvage's request
     docking runs Look at the docked pad, then again on the bridge) REPLACES
     those bytes in place: one view, first slot's position, last render's text,
     exactly the runner's one-key-per-raw-template response map. */
  sb_t  *look_sink = NULL;
  long   look_off = -1;
  size_t look_len = 0;
};

/* Outcome of resolve_refine (reference resolution -> disambiguation). */
enum { RR_NOMATCH = 0, RR_OK, RR_FAIL, RR_AMBIG, RR_CANTSEE, RR_NOREF };

/* What the caller needs to raise (or carry forward) a disambiguation prompt. */
struct amb_info {
  std::string ref_name;          /* "object1", "character1", ...              */
  char        type;              /* 'o' / 'c'                                 */
  std::string ref_text;          /* the captured reference text typed         */
  std::vector<std::string> keys; /* surviving candidate keys                  */
  /* This ambiguity arose only because the *same task* also has an unmatched
     but *required* (`Must Exist`) reference, so in the Adrift 5 runner the task does
     not match in the first pass and is found only in the second-chance
     (existence) pass.  There it sets sAmbTask, but a *different* task's
     0-item Must-Exist failure (GetGeneralTask) wins over it.  So unlike a
     first-pass ambiguity (which preempts any sNoRefTask), a second-chance
     ambiguity must YIELD to a clean no-reference fallback. */
  int         second_chance = 0;
};

/* ------------------------------------------------ cross-file entry points ---
   Functions shared between the turn-loop translation units.  Grouped by the
   file that defines each; every other file reaches them through here. */

/* a5run.cpp (core turn loop) */
int  msg_has_output (const char *m);
int  fd_has_output (a5_state_t *st, const char *raw);
std::vector<std::string> split_ws (const char *s);

/* a5run_events.cpp (events + walks runtime) */
void ev_init          (a5_run_t *run, sb_t *out);
void ev_tick_all      (a5_run_t *run, sb_t *out);
void ev_time_tick_all (a5_run_t *run, sb_t *out);
void drain_tasks_to_run (a5_run_t *run, sb_t *out);
void a5run_flush_display_defers (a5_run_t *run, sb_t *out);
void a5run_flush_display_defers_from (a5_run_t *run, sb_t *out, size_t from);
void ev_on_task_completed (a5_run_t *run, const char *task_key, sb_t *out);
void ev_on_task_uncompleted (a5_run_t *run, const char *task_key, sb_t *out);

/* a5run_ref.cpp (reference resolution + multiple-object references) */
void bind_reference (a5_state_t *st, const char *group, const char *value,
                     const char *text);
int  obj_in_scope     (a5_state_t *st, const char *key);
int  obj_visible      (a5_state_t *st, const char *key);
int  obj_seen_p       (a5_state_t *st, const char *key);
int  char_visible     (a5_state_t *st, const char *key);
int  char_seen_p      (a5_state_t *st, const char *key);
int  char_name_usable (a5_state_t *st, const a5_character_t *c);
std::string guess_plural_from_noun (const std::string &n);
std::string amb_word (a5_state_t *st, const std::vector<std::string> &keys,
                     const std::string &ref_text, char type);
std::vector<std::string> possible_keys (a5_state_t *st,
                     const std::vector<std::string> &keys,
                     const std::string &input, char type);
int  resolve_plural (a5_run_t *run, const a5_task_t *t, const std::string &text,
                     amb_info *amb);
int  resolve_refine (a5_run_t *run, const a5_task_t *t, const a5_match_t *m,
                     const char *force_name, const char *force_key,
                     amb_info *amb);

/* a5run_action.cpp (action helpers, task dispatch, response aggregation,
   conversation, action execution, run one task) */
void run_task (a5_run_t *run, const a5_task_t *t, int depth, sb_t *out);
void run_general (a5_run_t *run, const a5_task_t *parent, const a5_match_t *m,
                  sb_t *out);
void update_seen (a5_state_t *st);
void mark_player_arrival_seen (a5_state_t *st, const char *loc);
int  conv_contains_word (const std::string &sentence, const std::string &check);
void run_action (a5_run_t *run, const char *kind, const char *body, int depth,
                 sb_t *out);

/* a5run_conv.cpp (clsUserSession FindConversationNode / ExecuteConversation).
   Conversation type bits are clsAction.ConversationEnum; exec_conversation takes
   them OR-ed (the Command path also probes Command|Farewell).  Topic actions run
   back through run_action above, mirroring the runner's ExecuteConversation ->
   ExecuteActions. */
enum { A5_CONV_GREET = 1, A5_CONV_ASK = 2, A5_CONV_TELL = 4,
       A5_CONV_COMMAND = 8, A5_CONV_FAREWELL = 16 };
void exec_conversation (a5_run_t *run, const char *char_key, int conv_type,
                        const char *subject, sb_t *out);
void clear_conv_if_partner_gone (a5_run_t *run, sb_t *out);
void exec_scope_flush (a5_run_t *run, struct exec_resp_scope *sc, sb_t *out);

/* a5run_resp.cpp (clsUserSession htblResponsesPass/Fail).  `pos` is the slot a
   "Before" message reserved ahead of its actions (iResponsePosition); -1 appends.
   sink_len is the "did this frame produce output" probe -- the response-mutation
   count when a map is active (an aggregate MERGE is output too, though it adds no
   entry), else the byte length of `out`. */
void   ref_snap_take (a5_state_t *st, ref_snap *s);
void   ref_snap_restore (a5_state_t *st, const ref_snap *s);
size_t sink_len (a5_run_t *run, sb_t *out);
char  *render_comp_test (a5_state_t *st, const a5_xml_node_t *comp);
char  *render_comp_test_ex (a5_state_t *st, const a5_xml_node_t *comp,
                            char **marked);
int    comp_bears_function (const a5_xml_node_t *n);
void   resp_insert (resp_map *rm, resp_entry &e, int pos);
void   resp_add_comp (a5_run_t *run, const a5_task_t *t,
                      const a5_xml_node_t *comp, bool is_pass, int pos = -1);
void   resp_add_text (a5_run_t *run, const char *text, bool is_pass,
                      int pos = -1);
void   resp_add_fail (a5_run_t *run, const a5_xml_node_t *fm);
void   resp_flush (a5_run_t *run, resp_map *rm, sb_t *out);
std::string render_look_marked (a5_run_t *run);

#endif
