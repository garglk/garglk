/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- turn loop (Phase 3).
 *
 * Drives the player turn: lower-cases the input, walks the General tasks in
 * priority order (the Adrift 5 runner TaskSorter / GetGeneralTask), matches each task's
 * command patterns (a5parse), resolves the captured references to model keys in
 * scope, evaluates restrictions (a5restr), and -- on the first fully-passing
 * task -- runs its actions (a5run executes the core action set: MoveObject,
 * MoveCharacter, Set/Inc/DecVariable, SetProperty, SetTasks Execute, EndGame)
 * and prints its completion message, else surfaces the first failing
 * restriction's message.  Movement, take/drop, open and examine work.
 *
 * Mirrors clsUserSession.EvaluateInput / GetGeneralTask / ExecuteActions,
 * including general-reference disambiguation ("Which X?" + cross-turn clarifier);
 * the full event/character/walk/score machinery is the rest of Phase 4.
 */

#ifndef SCARIER_A5RUN_H
#define SCARIER_A5RUN_H

#include "a5model.h"
#include "a5state.h"

typedef struct a5_run_s a5_run_t;

extern a5_run_t *a5run_new  (const a5_adventure_t *adv);
extern void      a5run_free (a5_run_t *run);

/* The runtime state (for harness inspection). */
extern a5_state_t *a5run_state (a5_run_t *run);

/* Introduction + the opening LOOK, as plain text (caller frees). */
extern char *a5run_intro (a5_run_t *run);

/* Process one command line; returns this turn's output as plain text (freed by
   caller).  Never NULL. */
extern char *a5run_input (a5_run_t *run, const char *line);

/* Real-time TimeBased events (interactive hosts): hand the 1-second event tick
   to the host.  With real time ON, a5run_input skips its deterministic
   once-per-input substitute tick, and the host calls a5run_time_tick from a
   wall-clock 1-second timer instead -- the real Runner's tmrEvents_Tick.
   a5run_time_tick returns the tick's rendered output (caller frees), or NULL
   when the tick flushed nothing; a non-NULL return rebuilds the media-event
   list (a5run_media_*) with the tick's own images/sounds.  The flag is per-run:
   set it again after every a5run_new.  a5run_has_time_events reports whether
   the game defines any TimeBased event at all, so hosts can skip the timer
   entirely for the (many) games with none. */
extern void  a5run_set_real_time   (a5_run_t *run, int on);
extern char *a5run_time_tick       (a5_run_t *run);
extern int   a5run_has_time_events (a5_run_t *run);

/* Non-zero once an EndGame action has fired. */
extern int a5run_is_over (a5_run_t *run);

/* Status-line accessors.  a5run_location_name returns the current room NAME as
   plain text (caller frees; NULL if unknown); the others read Adventure.Score /
   MaxScore / Turns (0 when the game defines no Score/MaxScore variable). */
extern char *a5run_location_name (a5_run_t *run);
extern long  a5run_score         (a5_run_t *run);
extern long  a5run_maxscore      (a5_run_t *run);
extern int   a5run_turns         (a5_run_t *run);

/* Embedded-media events produced by the most recent a5run_intro / a5run_input,
   in display order, for the host to show images / play sounds.  `kind` is one of
   the A5_MEDIA_* values (a5text.h); `number` is the Blorb resource number (from
   <FileMappings>), or -1 when unresolved / for sound-stop.  The list is rebuilt
   each turn; pointers from a5run_media_get are valid until the next turn. */
typedef struct {
  int kind;          /* A5_MEDIA_IMAGE / A5_MEDIA_SOUND / _SOUND_STOP / _PAUSE   */
  int number;        /* Blorb resource number, or -1                            */
  int channel;       /* sound channel 1..8 (audio; 1 = the default), else 0     */
  int loop;          /* sound loop flag                                         */
  int shown;         /* host bookkeeping: presented at its positional mark      */
} a5_media_event_t;

extern int                     a5run_media_count (a5_run_t *run);
extern const a5_media_event_t *a5run_media_get   (a5_run_t *run, int i);
/* Flag event i `shown`: the host presented it at its positional A5_SOUND_MARK
   in the turn text, so the after-the-turn media sweep must not replay it.
   Cleared with the rest of the event when the list is rebuilt next turn. */
extern void                    a5run_media_note_shown (a5_run_t *run, int i);

/* Save/restore (Phase 5).  a5run_save serialises the full mutable runtime state
   -- object/character locations, variable values, completed tasks, property
   overrides, the "seen" sets, event/walk timers, displayed <DisplayOnce>
   segments, conversation state and the RNG -- to a malloc'd, NUL-terminated XML
   buffer (caller frees); *out_len receives its byte length (excluding the NUL).
   Returns NULL on failure.  a5run_restore applies such a buffer to `run` (which
   must have been created from the *same* game), returning 1 on success, 0 on a
   malformed/incompatible buffer (in which case `run` is left unchanged on the
   structural level, though a partial apply may have occurred -- callers should
   treat a 0 return as fatal for the session).  The buffer is the v5 save body;
   the Glk/Spatterlight layer may compress/wrap it. */
extern char *a5run_save    (a5_run_t *run, size_t *out_len);
extern int   a5run_restore (a5_run_t *run, const char *data, size_t len);

/* Multi-level undo (a snapshot stack built on save/restore; the depth matches
   the v4 engine's memo ring).  a5run_snapshot pushes the current state as an
   undo point -- call it from the frontend just BEFORE a5run_input each turn
   (a5run_input increments the turn counter on entry, so snapshotting inside it
   would capture mid-turn state).  a5run_undo restores the newest snapshot and
   consumes it, so repeated undo walks back turn by turn; it returns 1 on
   success, 0 once the stack is drained.  a5run_undo_forget drops every undo
   point without restoring (call after a RESTORE from file so undo does not
   jump back across the restore). */
extern void a5run_snapshot    (a5_run_t *run);
extern int  a5run_undo        (a5_run_t *run);
extern void a5run_undo_forget (a5_run_t *run);

/* Render the current room view (the stock Look task's view, darkness override
   included) as standalone display text, consuming no turn -- for the frontend
   to re-orient the player after a successful UNDO.  Caller frees. */
extern char *a5run_look (a5_run_t *run);

/* Undo-stack export/import, for the Spatterlight autosave: the stack entries
   are plain serialised snapshots (a5run_save buffers) plus the parallel
   turn-output texts, so they can be written into the autosave file and pushed
   back after an autorestore.  a5run_undo_peek indexes oldest-first and returns
   NULL past the end; a5run_undo_push_blob appends as the newest entry
   (dropping the oldest at the depth cap, like a5run_snapshot).  The last
   processed turn's composed output (the runner's sTurnOutput, replayed by a
   post-game-over UNDO) crosses over separately via a5run_get/set_turn_text. */
extern int         a5run_undo_depth (a5_run_t *run);
extern const char *a5run_undo_peek  (a5_run_t *run, int i, size_t *len,
                                     const char **turn_text, size_t *tt_len);
extern void        a5run_undo_push_blob (a5_run_t *run,
                                         const char *blob, size_t len,
                                         const char *turn_text, size_t tt_len);
extern void a5run_get_turn_text (a5_run_t *run, const char **text, size_t *len);
extern void a5run_set_turn_text (a5_run_t *run, const char *text, size_t len);

/* Non-zero when the next input line is a cross-turn parser continuation rather
   than a fresh command: a pending "Which X?" disambiguation or a remembered
   bare verb ("Take what?").  That transient state is not part of the save
   format, so an autosave taken at such a prompt would not restore coherently
   -- the autosave layer skips those prompts and keeps the previous save. */
extern int a5run_input_pending (a5_run_t *run);

/* Trace task matching / action execution to stderr. */
extern int a5run_trace;

#endif
