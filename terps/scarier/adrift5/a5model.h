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
 * ADRIFT 5 support for Scarier -- in-memory object model.
 *
 * A typed index over the parsed XML DOM (a5xml).  Each record carries the hot,
 * cross-cutting fields most code needs (keys, names, types, property maps)
 * plus a pointer back into the DOM for the structure that the state, text and
 * run layers interpret (descriptions, restrictions, actions, walks, topics,
 * sub-events).  The model owns the underlying a5_xml_doc, so all the const
 * char * fields alias into it and stay valid until a5model_free().
 *
 * Mirrors the Runner's clsAdventure collections (FileIO.Load500).
 */

#ifndef SCARIER_A5MODEL_H
#define SCARIER_A5MODEL_H

#include <stdint.h>

#include "a5xml.h"

/*
 * One ADRIFT property value on an entity.  ADRIFT is property-driven: most
 * world state lives here.  A property is either a flag (just a Key ->
 * value/value_node both NULL), a simple scalar (value set), or a rich text
 * value (value_node set to the <Value> description block).
 */
typedef struct a5_prop_s {
  const char *key;
  const char *value;                  /* simple scalar value, or NULL        */
  const a5_xml_node_t *value_node;    /* rich <Value> block, or NULL         */
} a5_prop_t;

typedef struct a5_object_s {
  const char *key;
  const char *article;
  const char *prefix;
  /* aliases (<Name> list) */
  const char **names;       int n_names;
  a5_prop_t *props;         int n_props;
  const a5_xml_node_t *node;
} a5_object_t;

typedef struct a5_location_s {
  const char *key;
  a5_prop_t *props;         int n_props;
  /* ShortDescription/LongDescription/Movement here */
  const a5_xml_node_t *node;
} a5_location_t;

/* One conversation node of a character (clsTopic): a reply gated by keywords
   or a command pattern, with its own restrictions and actions.  Greet/Farewell
   topics carry no keyword; Ask/Tell match on comma-separated keywords; Command
   matches the player's subject against a command pattern. */
typedef struct a5_topic_s {
  const char *key;
  const char *parent_key;           /* <ParentKey> ("" = top-level node)     */
  const char *keywords;             /* <Keywords> (comma list or pattern)    */
  int is_intro, is_ask, is_tell, is_command, is_farewell, stay_in_node;
  /* the reply <Description> block */
  const a5_xml_node_t *conversation;
  const a5_xml_node_t *restrictions; /* <Restrictions>, or NULL              */
  const a5_xml_node_t *actions;      /* <Actions>, or NULL                   */
} a5_topic_t;

/* One <Control> "Start|Stop|Suspend|Resume Completion|UnCompletion <TaskKey>"
   (EventOrWalkControl): trigger an event or walk on a task (un)completing.
   Shared by events and walks, so it is defined before both. */
typedef enum {
  A5_CTRL_START   = 0,
  A5_CTRL_STOP    = 1,
  A5_CTRL_SUSPEND = 2,
  A5_CTRL_RESUME  = 3
} a5_ctrl_t;
typedef struct a5_eventctrl_s {
  a5_ctrl_t control;
  int on_completion;                /* 1 = Completion, 0 = UnCompletion      */
  const char *task_key;
} a5_eventctrl_t;

/* One <Step> of a walk (clsWalk.clsStep): a destination key (location / group
   / character / "%Player%") reached after a turn duration (random range). */
typedef struct a5_walkstep_s {
  const char *location;             /* destination key                       */
  long ft_from, ft_to;              /* duration in turns (random range)      */
} a5_walkstep_t;

/* One <Activity> of a walk (clsWalk.SubWalk): like an event SubEvent, but with
   the extra ComesAcross trigger and an OnlyApplyAt (sKey3) display gate. */
typedef enum {
  A5_SW_FROM_LAST    = 0,           /* FromLastSubWalk                       */
  A5_SW_FROM_START   = 1,           /* FromStartOfWalk                       */
  A5_SW_BEFORE_END   = 2,           /* BeforeEndOfWalk                       */
  A5_SW_COMES_ACROSS = 3            /* ComesAcross <char> (meets the char)   */
} a5_sw_when_t;
typedef enum {
  A5_SW_DISPLAY   = 0,              /* DisplayMessage (oDescription)         */
  A5_SW_EXECTASK  = 1,              /* ExecuteTask sKey2                     */
  A5_SW_UNSETTASK = 2               /* UnsetTask sKey2                       */
} a5_sw_what_t;
typedef struct a5_subwalk_s {
  a5_sw_when_t when;
  long ft_from, ft_to;              /* turn offset (random range)            */
  const char *come_key;             /* ComesAcross subject (%Player%/key)    */
  a5_sw_what_t what;
  const char *task_key;             /* sKey2 (ExecuteTask/UnsetTask)         */
  const char *only_apply_at;        /* sKey3 (OnlyApplyAt loc/group gate)    */
  const a5_xml_node_t *description; /* <Action> DisplayMessage block         */
} a5_subwalk_t;

/* One character walk (clsWalk): a turn-based state machine that moves the
   character along its steps, runs sub-walk activities, and is started/stopped
   by WalkControls on task (un)completion. */
typedef struct a5_walk_s {
  const char *char_key;             /* owning character key                  */
  const char *description;
  int loops;                        /* <Loops>                               */
  int start_active;                 /* <StartActive>                         */
  a5_walkstep_t  *steps;     int n_steps;
  a5_subwalk_t   *subwalks;  int n_subwalks;
  a5_eventctrl_t *controls;  int n_controls;
} a5_walk_t;

typedef struct a5_character_s {
  const char *key;
  const char *name;
  const char *article;
  const char *prefix;
  const char *type;                 /* Player / NonPlayer                    */
  const char *perspective;          /* FirstPerson / SecondPerson / ...      */
  const char **descriptors; int n_descriptors;
  a5_prop_t *props;         int n_props;
  a5_topic_t *topics;       int n_topics;
  a5_walk_t  *walks;        int n_walks;
  const a5_xml_node_t *node;
} a5_character_t;

/* One <Specific> constraint of a Specific (override) task: a reference index
   of a given type, optionally restricted to one or more keys ("" = match any).
   */
typedef struct a5_specific_s {
  /* Object / Character / Location / ... */
  const char *type;
  /* allowed keys ("" entry = any) */
  const char **keys;        int n_keys;
} a5_specific_t;

typedef struct a5_task_s {
  const char *key;
  long priority;
  const char *type;                 /* General / Specific / System           */
  /* <RunImmediately>: a System task the Runner runs once at game start
     (clsUserSession init loop, vb:209-216) before the title -- e.g. PlayTune
     (title music) whose audio markup contributes the title's leading space, or
     ts_tasInitialise (set time). */
  int run_immediately;
  /* <LocationTrigger>: a System task armed when the Player moves into this
     location (clsCharacter.Move), or NULL */
  const char *location_trigger;
  const char **commands;    int n_commands;
  int repeatable;
  /* <Continue>ContinueAlways: keep running lower-priority matching tasks after
     this */
  int continue_lower;
  /* <LowPriority>: clsTask.LowPriority -- a low-priority task is skipped once
     a higher-priority failing-with-output task (or a continuation floor) has
     been seen (GetGeneralTask iPriorityFail filter) */
  int low_priority;
  /* clsTask.AggregateOutput: the Runner defers an aggregate task's
     completion-message function replacement to final Display, so its text
     reflects state changed by its After* override children.  Default True;
     False only on <Aggregate>0</Aggregate>. */
  int aggregate;
  const a5_xml_node_t *restrictions; /* <Restrictions> node, or NULL         */
  const a5_xml_node_t *actions;      /* <Actions> node, or NULL              */
  /* <FailOverride> Description, or NULL: shown for a "get all"-style command
     when no item passed (clsTask.FailOverride, clsUserSession.vb:788) --
     "There is nothing worth taking here." */
  const a5_xml_node_t *fail_override;
  const a5_xml_node_t *node;

  /* Specific-override task linkage (clsTask Specifics / GeneralKey). */
  const char *general_key;          /* <GeneralTask> parent key, or NULL     */
  const char *override_type;        /* <SpecificOverrideType>, or NULL       */
  a5_specific_t *specifics; int n_specifics;
} a5_task_t;

/* Upper bound the loader clamps <ArrayLength> to (the array is allocated and
   filled up front, so the file's value cannot be trusted as-is). */
enum { A5_MAX_ARRAY_LENGTH = 65536 };

/* Upper bound the loader clamps <WaitTurns> to. */
enum { A5_MAX_WAIT_TURNS = 1000 };

typedef struct a5_variable_s {
  const char *key;
  const char *name;
  const char *type;                 /* Numeric / Text                        */
  const char *initial;              /* InitialValue text                     */
  /* <ArrayLength> (clsVariable.Length); 1 = scalar */
  int         array_length;
  const a5_xml_node_t *node;
} a5_variable_t;

/* One <SubEvent>: at `when` (an offset in turns or, when `seconds` is set,
   in seconds -- a random range [ft_from, ft_to]) do `what` to `key`
   (clsEvent.SubEvent). */
typedef enum {
  A5_SE_FROM_LAST  = 0,             /* FromLastSubEvent                      */
  A5_SE_FROM_START = 1,             /* FromStartOfEvent                      */
  A5_SE_BEFORE_END = 2              /* BeforeEndOfEvent                      */
} a5_se_when_t;
typedef enum {
  A5_SE_DISPLAY   = 0,              /* DisplayMessage (oDescription)         */
  A5_SE_SETLOOK   = 1,              /* SetLook                               */
  A5_SE_EXECTASK  = 2,              /* ExecuteTask sKey                      */
  A5_SE_UNSETTASK = 3               /* UnsetTask sKey                        */
} a5_se_what_t;
typedef struct a5_subevent_s {
  a5_se_when_t when;
  long ft_from, ft_to;              /* turn/second offset (random range)     */
  /* <Measure>Seconds</Measure> (SubEvent.eMeasure; default Turns).  On a
     TurnBased event such a sub-event runs off its own timer instead of the
     turn count -- see ev_tick_second_timers in run/a5run_events.cpp.  On a
     TimeBased event the measure makes no difference. */
  int seconds;
  a5_se_what_t what;
  const char *key;                  /* task/location key (ExecuteTask/...)   */
  const a5_xml_node_t *description; /* <Action> Description (DisplayMessage) */
} a5_subevent_t;

typedef struct a5_event_s {
  const char *key;
  const char *type;                 /* TurnBased / TimeBased                 */
  /* 1 Immediately/2 BetweenXY/3 AfterATask */
  int when_start;
  long start_from, start_to;        /* StartDelay random range               */
  long length_from, length_to;      /* Length random range                   */
  int repeating;
  int repeat_countdown;
  a5_subevent_t  *subevents; int n_subevents;
  a5_eventctrl_t *controls;  int n_controls;
  const a5_xml_node_t *node;
} a5_event_t;

typedef struct a5_group_s {
  const char *key;
  const char *type;                 /* Locations / Objects / Characters      */
  const char *name;
  const char **members;     int n_members;
  /* group <Property> list; for Objects groups these are inherited by member
     objects (clsItem htblInheritedProperties) */
  a5_prop_t *props;         int n_props;
  const a5_xml_node_t *node;
} a5_group_t;

typedef struct a5_propdef_s {
  const char *key;
  const char *type;                 /* Boolean / Integer / Text / ...        */
  const char *property_of;          /* Objects / Characters / Locations      */
  const char *dependent_key;
  /* <AppendTo>: a StateList whose value is appended to another property's
     state list (e.g. LockStatus -> OpenStatus).  the Runner's BeInState
     ignores appended props. */
  const char *append_to;
  const a5_xml_node_t *node;
} a5_propdef_t;

typedef struct a5_alr_s {
  const char *key;
  const char *old_text;             /* OldText                               */
  /* NewText description block */
  const a5_xml_node_t *new_text;
  const a5_xml_node_t *node;
} a5_alr_t;

typedef struct a5_udf_s {
  const char *key;
  const char *name;
  const a5_xml_node_t *node;
} a5_udf_t;

/* <Hint> (clsHint): one author-written question with two answers -- a subtle
   nudge and a "sledgehammer" that just tells you -- gated by the hint's own
   restriction block, so a hint only offers itself once its puzzle is live.
   The Question is a plain String property (clsHint.sQuestion), not a
   Description; only the two answers are restricted-description blocks.
   The Runner reaches these through a GUI hints window rather than a command,
   so the port drives them from "glk hints" (os_glk gsc_a5_display_hints). */
typedef struct a5_hint_s {
  const char *key;
  /* <Question> plain text */
  const char *question;
  /* <Subtle> wrapper, or NULL */
  const a5_xml_node_t *subtle;
  /* <Sledgehammer> wrapper, or NULL */
  const a5_xml_node_t *sledgehammer;
  /* <Restrictions>, or NULL (= always) */
  const a5_xml_node_t *restrictions;
  const a5_xml_node_t *node;
} a5_hint_t;

/* <Synonym>: a global word/phrase substitution applied to the player's raw
   input before task matching (clsUserSession.EvaluateInput's synonym loop).
   Several <From> phrases (often duplicated in the XML) map to one <To>
   replacement word. */
typedef struct a5_synonym_s {
  const char *key;
  /* From */
  const char **from;  int n_from;
  /* To */
  const char *to;
  const a5_xml_node_t *node;
} a5_synonym_t;

/* <FileMappings>/<Mapping>: maps an embedded-media resource number (the number
   used by <img src>/<audio src> references, via the original file path) to the
   Blorb resource number.  In ADRIFT 5 Blorbs the <Resource> number IS the
   Blorb resource number (a single sequence shared across Pict and Snd usages),
   so the driver loads giblorb (Pict|Snd, number) directly. */
typedef struct a5_filemap_s {
  /* <Resource> (== Blorb resource number) */
  int number;
  /* <File> original path (aliases the doc) */
  const char *file;
} a5_filemap_t;

typedef struct a5_adventure_s {
  a5_xml_doc_t *doc;                /* owned                                 */
  const a5_xml_node_t *root;
  const a5_xml_node_t *introduction;
  /* <EndGameText> (clsAdventure.WinningText), shown after the win/lose line,
     or NULL */
  const a5_xml_node_t *end_game_text;

  const char *title;
  const char *author;
  const char *version;
  /* Babel IFID from the plaintext <ifid> tag in the <ifindex> metadata block
     (owned; NULL when the file embeds none) */
  const char *ifid;
  const char *release;              /* <release><version> from the same
                                       <ifindex> block (%release%'s
                                       BabelTreatyInfo ... Release.Version);
                                       owned; NULL when absent               */
  /* The author's Runner window layout, if the Blorb carries one (a5blorb.h,
     a5blorb_find_layout): 1 when its Map pane is open, 0 when closed, -1 when
     the game ships no layout at all.  This is what makes a game start with its
     map showing in run500.exe */
  int map_pane_open;
  /* <ShowFirstLocation> (default 1): show the start room after the intro */
  int show_first_location;
  /* <BackgroundColour>, <InputColour>, */
  uint32_t bg_colour;
  /* <OutputColour> and <LinkColour>: the */
  uint32_t input_colour;
  /* author's Runner palette, held as */
  uint32_t output_colour;
  /* 0xRRGGBB.  The XML carries them as OLE (blue-green-red) integers in
     decimal and omits any that matches the Runner default, so these are always
     set -- see ADRIFT-5-XML.md 2.6 */
  uint32_t link_colour;
  /* <ShowExits> (default 1): append the "Exits are .../An exit leads ..."
     listing to each location view (clsAdventure.  bShowExits default True) */
  int show_exits;
  /* <WaitTurns> (default 3): turns a single "wait"/"z" advances */
  int wait_turns;
  /* <TaskExecution> == HighestPriorityPassingTask: a failing-with-output task
     does NOT claim the turn; the scan keeps looking for a passing task
     (clsAdventure.TaskExecution, default HighestPriorityTask => 0) */
  int hp_passing;
  /* <DirectionNorth>..<DirectionDown>: localized direction synonym specs,
     indexed by DirectionsEnum (North=0, East=1, South=2, West=3, Up=4, Down=5,
     In=6, Out=7, NorthEast=8, SouthEast=9, SouthWest=10, NorthWest=11); NULL
     when the game omits the field (=> English default).  Slash-separated
     synonyms, first = display name (clsAdventure.sDirectionsRE /
     Global.DirectionName) */
  const char *dir_re[12];

  a5_object_t    *objects;    int n_objects;
  a5_location_t  *locations;  int n_locations;
  a5_character_t *characters; int n_characters;
  a5_task_t      *tasks;      int n_tasks;
  a5_variable_t  *variables;  int n_variables;
  a5_event_t     *events;     int n_events;
  a5_group_t     *groups;     int n_groups;
  a5_propdef_t   *propdefs;   int n_propdefs;
  a5_alr_t       *alrs;       int n_alrs;
  a5_udf_t       *udfs;       int n_udfs;
  a5_hint_t      *hints;      int n_hints;
  a5_filemap_t   *filemaps;   int n_filemaps;
  a5_synonym_t   *synonyms;   int n_synonyms;

  void *key_index;            /* opaque key->array-index hashes (a5model.cpp),
                                 built once at load: the corpus replays spend
                                 most of their time in key lookups otherwise
                                 (linear strcmp scans over ~2000 tasks etc.) */

  /* "Adventure Upgrade" bracket auto-correction (FileIO.vb:634): a file saved
     before 5.0.26 whose restriction blocks contain an AND-then-OR
     BracketSequence ("#A#O#") gets a one-time question at load; answering yes
     rewrites each "#A#O#..." into "#A(#O#...)" (the Runner's
     CorrectBracketSequence).  See a5model_upgrade_pending /
     a5model_upgrade_answer below. */
  int upgrade_scanned;        /* pending-condition memoised                  */
  int upgrade_wanted;         /* version < 5.0.26 and some block has #A#O#   */
  int upgrade_prompted;       /* the host asked the question (either answer) */
  /* host resolved it WITHOUT showing the dialog: a5run_intro emits no question
     prose and starts the intro cleanly (Glk frontend default) */
  int upgrade_silent;
  int upgrade_applied;        /* answered yes; sequences corrected           */
  /* The Runner iCorrectedTasks (one per Replace call) */
  int upgrade_count;
  char **upgrade_owned;       /* corrected BracketSequence copies (owned)    */
  int n_upgrade_owned;
} a5_adventure_t;

/* Build the model from an already-parsed doc (takes ownership of doc). */
extern a5_adventure_t *a5model_from_doc (a5_xml_doc_t *doc);

/* key -> array index via the load-time hash (kind: 'O' objects, 'L' locations,
   'C' characters, 'T' tasks, 'V' variables, 'G' groups); -1 when absent.
   Falls back to the linear scan when the index is not built (hand-assembled
   test models). */
extern int a5model_key_index (const a5_adventure_t *a, int kind, const char *key);

/* Is `key` in group number `grp`'s static <Member> list? */
extern int a5model_group_has_member (const a5_adventure_t *a, int grp,
                                     const char *key);

/* Full pipeline: read a Blorb/.taf file, deobfuscate, inflate, parse, model.
   */
extern a5_adventure_t *a5model_load (const char *path);

/* Same pipeline from an in-memory file image.  Takes ownership of file_buf
   (always frees it, including on failure).  Used by Glk hosts that already
   have the game open as a stream (e.g. Emglken with FILESYSTEM=0). */
extern a5_adventure_t *a5model_load_buffer (uint8_t *file_buf, uint32_t file_len);

/*
 * The "Adventure Upgrade" question (FileIO.vb:634, headless answer semantics
 * in FrankenDrift.Headless AskYesNoQuestion): when a5model_upgrade_pending
 * returns non-zero the host should show a5model_upgrade_question (title
 * "Adventure Upgrade") and read the player's yes/no through its NORMAL input
 * channel (the next command-script line headless, a typed line in the Glk
 * frontend -- like the ADRIFT 4 gender prompt), then call
 * a5model_upgrade_answer.  On yes the BracketSequence nodes are corrected in
 * place and the call returns the number of Replace passes (the Runner's "N
 * tasks have been updated." count); on no it returns 0 and the sequences stay
 * verbatim.  A host that never asks leaves the model unchanged, and
 * a5run_intro then renders the question itself with an implicit "no" (the
 * pre-existing behaviour, matching an unattended the Runner).
 */
extern int a5model_upgrade_pending (const a5_adventure_t *a);
extern const char *a5model_upgrade_question (void);
extern int a5model_upgrade_answer (a5_adventure_t *a, int yes);

/* Whether an interactive host that suppresses the dialog should force YES for
   this game (hard-wired allow-list, keyed on the Babel IFID); default is NO.
   */
extern int a5model_upgrade_forced_yes (const a5_adventure_t *a);

/*
 * Non-NULL when the adventure cannot be played at all, in which case the
 * return is the Runner's own error text for the host to show.  Today the only
 * such case is an adventure with zero locations: clsUserSession.OpenAdventure
 * vb:433 (FrankenDrift's clsUserSession.vb:278) ends the load with
 * ErrMsg("This adventure has no locations.  Cannot continue.") / Return False.
 *
 * The check sits AFTER the title and Introduction Displays (vb:377-378), so a
 * host must call this once the intro has been presented, not at load time --
 * the real Runner does open its window and print the game title before putting
 * up the "ADRIFT Error" dialog (verified against run500.exe 5.0.36).
 */
extern const char *a5model_load_error (const a5_adventure_t *a);

/* Non-zero when any of the four palette fields differs from the Runner's own
   default, i.e. when the author chose a colour scheme rather than accepting
   the one every ADRIFT 5 game starts from.  A host that would otherwise show
   the story in its own theme uses this to decide whether the adventure has a
   look worth honouring. */
extern int a5model_custom_palette (const a5_adventure_t *a);

/* Offer every authored string in the adventure to `accept`, depth-first, and
   return non-zero as soon as one is accepted (nothing further is visited).
   The strings arrive as the XML parser left them -- entity-decoded, so any
   markup the author embedded reads as the "<font ...>" the Runner sees.  This
   is a raw walk of the parse tree rather than of the object model: it carries
   no promise about which record a string belongs to, only that a caller
   grepping for authored markup sees all of it. */
extern int a5model_scan_text (const a5_adventure_t *a,
                              int (*accept) (const char *text, void *opaque),
                              void *opaque);

extern void a5model_free (a5_adventure_t *adv);

/* Property lookup within a record's property array. */
extern const a5_prop_t *a5_prop_find (const a5_prop_t *props, int n,
                                      const char *key);

/* The scalar value of an object's authored (model) property, or NULL when the
   object has no such property.  Note this is the *authored* value: a runtime
   override is only visible through a5state_entity_prop(). */
static inline const char *
obj_prop (const a5_object_t *o, const char *key)
{
  const a5_prop_t *p = a5_prop_find (o->props, o->n_props, key);
  return p ? p->value : nullptr;
}

/* Per-type key lookups. */
extern const a5_object_t    *a5model_object    (const a5_adventure_t *a, const char *key);
/* The object, character and location carrying `key` (each NULL when there is
   none), from a single hash of the key. */
extern void a5model_entity (const a5_adventure_t *a, const char *key,
                            const a5_object_t **o,
                            const a5_character_t **c,
                            const a5_location_t **l);
extern const a5_location_t  *a5model_location  (const a5_adventure_t *a, const char *key);
extern const a5_character_t *a5model_character (const a5_adventure_t *a, const char *key);
extern const a5_task_t      *a5model_task      (const a5_adventure_t *a, const char *key);
extern const a5_propdef_t   *a5model_propdef   (const a5_adventure_t *a, const char *key);
extern const char *a5model_valuelist_value (const a5_propdef_t *pd,
                                            const char *label);

/* Resolve an <img>/<audio> src reference (an original file path) to its Blorb
   resource number via <FileMappings>, matching on the path's basename.
   Returns -1 when there is no matching mapping. */
extern int a5model_resource_for_file (const a5_adventure_t *a, const char *src);

/* ------------------------------------------------------------ exit lookups */

/* The location's <Movement> child for canonical direction `dir`, or NULL.
   Pass the previous result back as `after` to resume the scan: a location may
   hold more than one Movement for a direction, and a5restr_exit_in_direction
   walks past the route-restricted ones to reach the next candidate.  Pass
   after=NULL to start from the first child. */
extern const a5_xml_node_t *a5model_movement (const a5_location_t *loc,
                                              const a5_xml_node_t *after,
                                              const char *dir);

/* The raw Destination key of `lockey`'s first exit in `dir`, or NULL when
   there is no such exit (or its Destination is empty).  This is the Runner's
   arlDirections(d).LocationKey read: the *unconditional* map, with no route
   restriction evaluated -- unlike a character's HasRouteInDirection, which
   goes through a5restr_exit_in_direction. */
extern const char *a5model_exit_dest (const a5_adventure_t *a,
                                      const char *lockey, const char *dir);

#endif
