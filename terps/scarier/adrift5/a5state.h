/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- runtime state (Phase 2).
 *
 * A small mutable layer over the immutable a5model: the bits of world state the
 * text/display engine needs to read and that later phases will mutate -- object
 * and character locations, variable values, and per-task "completed" flags.
 *
 * For Phase 2 (the static world) the state is initialised straight from the
 * model's property values and is not yet driven by a turn loop; it is shaped so
 * Phases 3-4 can mutate it in place (move objects, set variables, complete
 * tasks) without the display code changing.
 *
 * Mirrors the Adrift 5 runner's clsObject.Location / clsCharacter.Location property
 * decoding (clsObject.vb:521, clsObject.ExistsAtLocation) and clsVariable.
 */

#ifndef SCARIER_A5STATE_H
#define SCARIER_A5STATE_H

#include "a5model.h"

/* Upper bound on the number of items a single %objects% reference can resolve to
   (e.g. "take all" in a room of dozens of objects). */
#define A5_MAX_ITEMS 256

/* Where an object is.  key meaning depends on `where` (see comments). */
typedef enum {
  A5_OWHERE_NONE = 0,     /* unplaced / unknown                                */
  A5_OWHERE_HIDDEN,       /* "Hidden"/"Nowhere": not in any room               */
  A5_OWHERE_ALLROOMS,     /* "Everywhere": present in every location           */
  A5_OWHERE_LOCATION,     /* key = location key                                */
  A5_OWHERE_LOCGROUP,     /* key = location-group key                          */
  A5_OWHERE_ON_OBJECT,    /* key = object key (on its surface)                 */
  A5_OWHERE_IN_OBJECT,    /* key = object key (inside it)                      */
  A5_OWHERE_HELD_BY,      /* key = character key                               */
  A5_OWHERE_WORN_BY,      /* key = character key                               */
  A5_OWHERE_PART_OBJECT,  /* key = object key                                  */
  A5_OWHERE_PART_CHAR     /* key = character key                               */
} a5_owhere_t;

typedef struct a5_objloc_s {
  a5_owhere_t where;
  const char *key;        /* aliases into the model/DOM; not owned             */
  int is_static;
} a5_objloc_t;

/* One SetLook stack entry (clsEvent.clsLookText): rendered look text gated on a
   location/group key. */
typedef struct a5_looktext_s {
  char *loc_key;          /* OnlyApplyAt gate (owned)                          */
  char *text;             /* rendered look text (owned)                        */
} a5_looktext_t;

/* A runtime property override (set by SetProperty actions in Phase 3). */
typedef struct a5_prop_ov_s {
  char *entity;           /* entity key (object/character/...)                 */
  char *prop;             /* property key                                      */
  char *value;            /* new value (owned)                                 */
} a5_prop_ov_t;

/* One live group-membership entry (the runner clsGroup.arlMembers): an ordered,
   distinct list per group, seeded from the model's static <Member>s and then
   mutated by Add/Remove*ToGroup at runtime.  Keys alias the model DOM (not
   owned).  RandomKey selection and EverywhereInGroup enumeration read this so
   procedural games (e.g. Skybreak's random-jump world groups) select and clear
   the correct live members in the runner's insertion order. */
typedef struct a5_grpmem_s {
  char *grp;              /* group key (owned)                                 */
  char *key;              /* member key -- location/object/character (owned)   */
} a5_grpmem_t;

typedef struct a5_state_s {
  const a5_adventure_t *adv;

  /* The current player character's key (clsAdventure.Player.Key).  Almost always
     the literal "Player", but a `MoveCharacter ... ToSwitchWith` action that
     involves the player (ADRIFT's BECOME <character> mechanism) retargets it to
     another character's key -- the player viewpoint, %Player% resolution, and
     scope then follow that character.  Points at a stable model key string. */
  const char *player_key;

  a5_objloc_t *obj;       /* [adv->n_objects], parallel to adv->objects        */
  const char **char_loc;  /* [adv->n_characters] location key, or NULL         */
  char **char_position;   /* [adv->n_characters] Standing/Sitting/Lying, owned */
  const char **char_onobj;/* [adv->n_characters] object key the char is on/in,
                             or NULL when "at location" (ExistWhere==AtLocation) */
  char *char_in;          /* [adv->n_characters] when char_onobj set: 1=inside,
                             0=on the surface (ExistWhere InObject vs OnObject)  */
  const char **char_onchar;/* [adv->n_characters] carrier CHARACTER key when the
                              char is On/In another character (ExistWhere
                              On/InCharacter, e.g. Edith riding the Player), else
                              NULL; effective location resolves through the carrier */

  long  *var_num;         /* [adv->n_variables] numeric value                  */
  char **var_text;        /* [adv->n_variables] text value (owned), or NULL    */
  long **var_arr;         /* [adv->n_variables] numeric ARRAY elements (owned;
                             [array_length], 0-based storage of ADRIFT's 1-based
                             indices), non-NULL only for a Numeric variable with
                             <ArrayLength> > 1.  var_num[i] mirrors element 1
                             (the runner's scalar %name% read of an array = IntValue(1)),
                             kept in sync by a5state_var_set_elem.              */

  char  *task_done;       /* [adv->n_tasks] completed flag                     */
  char  *task_scored;     /* [adv->n_tasks] this task has modified Score once
                             (clsTask.Scored); gates its IncVariable Score so a
                             repeatable/re-executed scoring task scores only once */

  a5_prop_ov_t *ov;       /* property overrides set at runtime                 */
  int n_ov, cap_ov;

  a5_grpmem_t *gm;        /* live group membership (the runner arlMembers), ordered    */
  int n_gm, cap_gm;

  int   game_over;        /* set by EndGame: 0 running, else 1                 */
  char *end_message;      /* EndGame enum "Win"/"Lose"/"Neutral" (owned), NULL */
  int   end_displayed;    /* the win/lose/score/restart block has been emitted */
  int   turns;            /* commands processed (clsAdventure.Turns); the score
                             line shows the count BEFORE the ending command     */

  /* Conversation state (clsAdventure.sConversationCharKey / sConversationNode):
     the NPC the player is currently talking to and the current topic node.
     Both owned; "" when not in a conversation. */
  char *conv_char;
  char *conv_node;

  /* Player "seen" state (clsCharacter.HasSeenCharacter, player-centric): set at
     end of turn for every character the player can see, so HaveSeenCharacter
     and the conversation "characters must be seen" gates work.  [n_characters] */
  char *char_seen;

  /* clsCharacter.Introduced: set the first time the character's Name renders
     in displayed output (clsCharacter.vb:333, bMarkAsSeen defaulted True);
     from then on an Indefinite-article descriptor render upgrades to Definite
     ("a tall guillermo" -> "the tall guillermo", vb:331).  The room-view
     character listing reads the name with bMarkAsSeen:=False
     (clsLocation.vb:151) so it upgrades but never introduces.  [n_characters] */
  char *char_introduced;

  /* Player "seen" state for objects (clsCharacter.HasSeenObject): set each turn
     for every object the player can currently see, so a later out-of-scope
     reference can tell "never seen" (HaveBeenSeenByCharacter fails -> "You see
     no such thing.") from "seen but not here now" ("You can't see the X.").
     [n_objects] */
  char *obj_seen;

  /* Player "seen" state for locations (clsCharacter.HasSeenLocation): the runner sets
     the flag inside clsCharacter.Move for every location a character moves to,
     plus the player's start location at session init (clsUserSession.vb:222).
     Player-centric like obj_seen/char_seen; Location HaveBeenSeenByCharacter
     restrictions read it.  [n_locations] */
  char *loc_seen;

  /* Per-character snapshot stash for the three player-centric seen sets above,
     across an ADRIFT BECOME viewpoint switch.  In the runner HasSeenObject/-Location/
     -Character are per-character (clsCharacter), so when the player changes the
     old viewpoint's sightings must NOT carry over to the new one.  On a switch
     the active obj_seen/char_seen/loc_seen are copied into the OLD player's slot
     and the NEW player's slot copied back (a NULL slot = a character that has
     never been the player, i.e. an empty seen set).  Only ever populated for a
     game that switches player (only BugHuntOnMenelaus in the corpus); a lone-
     "Player" game leaves every slot NULL and the active arrays untouched, so it
     is byte-identical to the single-array model.  [n_characters] slots each. */
  char **seen_stash_obj;
  char **seen_stash_char;
  char **seen_stash_loc;
  int    seen_active_ci;   /* character index the active seen arrays hold */

  /* Last-referenced pronoun targets (clsUserSession.sIt/sThem/sHim/sHer), the
     full display name of the object/character most recently named by the player
     in each pronoun class.  GrabIt recomputes these each turn from the (already
     it->name substituted) input; the next turn's "it"/"them"/"him"/"her" is
     textually replaced by the stored name before parsing.  All owned; ""
     until something has been referenced.  Save/restored. */
  char *s_it;
  char *s_them;
  char *s_him;
  char *s_her;

  /* Transient character context for char-scoped text functions (%CharacterName%
     etc.).  v5 rewrites a character's own text "%CharacterName%" ->
     "%CharacterName[Key]%" at load (FileIO SearchAndReplace); we instead set the
     context key while rendering that character's CharHereDesc / topic reply, so
     a bare %CharacterName% resolves to it.  NULL = default (Player). */
  const char *ctx_char;

  /* Per-turn reference bindings (e.g. "ReferencedObject2" -> "Table1"), set by
     the parser before restriction/action evaluation.  A binding value can be a
     whole %objects% pipe-list of keys ("Object27|Object30|...") -- a big
     drop-all/get-all easily tops 256 chars, and a truncated tail key renders as
     garbage ("...the large hammer and Objec") or silently drops items from the
     aggregate list (Fortress of Fear).  Size for ~200 keys. */
#define A5_REFVAL_LEN 2048
  char  ref_name[16][32];
  char  ref_value[16][A5_REFVAL_LEN];
  int   n_refbind;

  /* Multiple-object reference items (the %objects% grammar: "all", "all
     <plural>", "X and Y", comma lists, "... except/but/apart from ..." and
     plural nouns -- clsUserSession.InputMatchesObjects).  When a %objects%/
     %characters% reference resolves to a set, these hold the chosen item keys
     (each aliasing the model, in command order).  ReferencedObject is bound to
     the first; ReferencedObjects is bound to the "key1|key2|..." pipe list so
     the OO/text engine renders the whole set.  The per-item action loop
     (run_action ReferencedObjects) and the bare %objects% list renderer read
     these; n_ref_items == 0 means the ordinary single-object path. */
  const char *ref_items[A5_MAX_ITEMS];
  int   n_ref_items;
  char  ref_items_type;        /* 'o' object / 'c' character                  */

  /* The first object/character slot (%object%==%object1% / %character%) was
     filled by a *plural* %objects%/%characters% reference (the runner ReferenceMatch
     "objects"/"characters", not "object1").  The key stays bound for override-key
     matching and the ReferencedObjects/ReferencedCharacters restriction paths,
     but the singular %object%/%object1% (resp. %character%) text token must
     render EMPTY (GetReference returns Nothing unless ReferenceMatch="object1",
     clsUserSession.vb:3990) -- e.g. a give task's "%TheObject[%object%]%" prints
     "nothing".  Set by bind_reference, reset by a5state_clear_refs. */
  int   ref_object1_plural;
  int   ref_character1_plural;

  /* The matched command carries BOTH a genuine plural %objects% reference and a
     separate singular %object% reference (e.g. `hide %objects% in %object%`).
     the runner's GetReference (clsUserSession.vb:3990) resolves ReferencedObject only
     to the reference whose ReferenceMatch is "object1" -- never to the plural
     -- so the per-item plural binds (resolve_plural's restriction probes,
     run_general's item loop) must NOT clobber the singular alias: a restriction
     like `ReferencedObject Must HaveProperty ...` keeps testing the container,
     not the item being iterated (Dwarf of Direwood's `hide X, Y and Z in
     beard`).  Set by resolve_refine when it defers the plural, reset by
     a5state_clear_refs; read by bind_reference. */
  int   ref_objects_suppress_singular;

  /* SetLook event sub-event "look stack" (clsEvent.stackLookText): each SetLook
     pushes a (location/group gate, rendered text) entry; a5text_view_location
     appends the most-recent entry whose gate matches the player's location.
     Unused by the shipped corpus, but ported for faithfulness.  Owned. */
  a5_looktext_t *looks;
  int n_looks, cap_looks;

  /* <DisplayOnce> description segments that have already been shown (keyed by
     the segment's DOM node).  `marking_display` is set while rendering real
     output (vs a peek/test render) so a segment is only retired once it has
     actually reached the player -- mirrors clsDescription.ToString's
     Displayed flag gated on UserSession.bTestingOutput. */
  const void **disp_once;
  int n_disp_once, cap_disp_once;
  int marking_display;

  /* Set while rendering text that the runner hands to Display() with its %functions%
     still UNREPLACED -- conversation topic replies (ExecuteConversation
     AddResponses the raw Description; Display -> ReplaceALRs -> ReplaceFunctions
     then renders it under bDisplaying=True).  Only such renders run
     clsCharacter.Name's Introduced dance (definite-article upgrade + marking).
     Task completion messages are pre-replaced OUTSIDE bDisplaying
     (clsUserSession.vb:1186), the room view builds its names programmatically
     (clsLocation.vb:151), and the NPC walk announcements compose with .Name
     before Display (clsCharacter.vb:1558) -- none of those upgrade or mark.
     Transient render state, not saved. */
  int intro_active;

  /* Set while evaluating an ALR (TextOverride) NewText: a bare %variable%
     token is emitted as an A5_VARDEF_MARK sentinel instead of its value, so
     it resolves at the Display boundary with the end-of-turn value -- the runner
     expands ALRs (and the functions inside them) only in Display's
     ReplaceALRs (see A5_VARDEF_MARK, a5text.h). */
  int alr_defer_vars;

  /* The runner UserSession.PronounKeys: each rendered %CharacterName[...]% records the
     named character's perspective and its text offset (Global.vb:2108); a
     [1st/2nd/3rd] conjugation bracket then resolves in the perspective of the
     nearest PRECEDING rendered name -- GetPerspective (Global.vb:2481) picks
     the entry with the highest offset <= the bracket's, falling back to the
     player ("The medic [am/are/is] wearing ..." -> "is").  Offsets are
     positions within the message being rendered (the runner's iMatchLoc; an entry at
     offset 0 can never win, mirroring iHighest starting at 0).  Cleared once
     per processed command (PrepareForNextTurn, vb:3823).  Transient: not
     saved, not copied (the runner's States don't record PronounKeys either).
     Each entry also records the character KEY and the mention's requested
     pronoun: clsCharacter.Name (clsCharacter.vb:340) pronoun-replaces a later
     %CharacterName[key]% of an already-mentioned character ("He ignores
     you."), upgrading a requested Objective to Reflective when the previous
     mention was Subjective. */
#define A5_MAX_PRON 64
  int  pron_persp[A5_MAX_PRON];  /* 1=FirstPerson / 2=Second / 3=Third      */
  long pron_off[A5_MAX_PRON];
  const char *pron_key[A5_MAX_PRON];  /* character key (model-owned)        */
  int  pron_pron[A5_MAX_PRON];   /* mention's a5_pronoun_t (-1 = "none")    */
  int  n_pron;
  int  pron_pending;             /* set by the CharacterName eval, captured
                                    with its output offset at the emit site */
  const char *pron_pending_key;  /* character key of the pending mention    */
  int  pron_pending_pron;        /* its requested pronoun (-1 = "none")     */
  int  name_cap_eligible;        /* set by a resolved CharacterName eval:
                                    the runner's PCase "slight fudge" (Global.vb:2103)
                                    capitalises the rendered name when it is
                                    immediately preceded by two spaces / CRLF */

  /* Adventure.sReferencedText(0..4): the TURN-GLOBAL %text% capture slots.
     During the GetGeneralTask scan every command-matched candidate's %textN%
     capture overwrites slot N-1 (clsUserSession.vb:2567) -- even when that
     candidate later fails its references or restrictions -- and a
     `ReferencedTextN Must ...` restriction reads this GLOBAL slot, not a
     per-task binding (vb:4474).  So AoK's s_SayHelloTo ("[say] [hello/hi]
     {to} {edwina/...}", no %text% of its own) passes `ReferencedText Must
     BeContain "hello"` because earlier scanned candidates ("%text%" catch-alls,
     "[say/shout] %text%") already captured the input.  After the scan an empty
     slot 0 defaults to the raw input (vb:3400).  Cleared per processed command
     by a5run_input; the per-task bindings stay authoritative when present. */
  char scan_text[5][256];

  /* The runner's `sOutputText <> ""` is a test on the RAW marked-up display buffer, so
     a task completion that is pure markup -- Bug Hunt's cl_ReadMap `<centre>
     <img src="...">` -- still counts as turn output (no NotUnderstood fallback,
     TurnBasedStuff ticks) even though it strips to nothing.  Scarier's `out`
     only sees the stripped text, so the message renderers set this flag when a
     real (non-test) render produced a non-blank marked-up string.  Reset once
     per processed command by a5run_input. */
  int turn_out_nonempty;

  /* Per-turn route-restriction memo (the Adrift 5 runner's per-character
     dictHasRouteCache/dictRouteErrors) -- owned and typed by a5restr.cpp,
     cleared once per processed command (PrepareForNextTurn, vb:3792) via
     a5restr_route_cache_clear.  Transient: not saved, not copied. */
  void *route_cache;

  /* Exits whose route restrictions have ever evaluated false, the runner's
     clsDirection.bEverBeenBlocked (set by HasRouteInDirection on every failed
     evaluation, clsCharacter.vb:686).  The map keeps a restricted-but-passable
     connector solid until its exit is on this ledger (Map.vb:1447).  Owned and
     typed by a5restr.cpp.  Like the runner's flag it lives for the whole
     session: never cleared, not saved, not copied. */
  void *ever_blocked;

  /* Set by a HaveRouteInDirection evaluation (a5restr pass_character) to the
     blocked exit's *own* restriction <Message> when the exit exists but is
     restriction-gated -- the Adrift 5 runner's sRouteError, which overrides the
     movement restriction's generic "There is no route..." text.  NULL when the
     exit is open or simply absent.  Not owned (a DOM node). */
  const a5_xml_node_t *route_error;

  /* The Adrift 5 runner's sRestrictionText, as a (non-owned) <Message> DOM node.  Every
     PassRestrictions call updates it: a restriction that fails sets it to that
     restriction's Message (NULL when the restriction has none), a passing one on
     the deciding path clears it, and a call that evaluates *no* single
     restriction (a malformed BracketSequence, whose EvaluateRestrictionBlock
     returns False without calling PassSingleRestriction) leaves it untouched.
     Crucially it is NOT reset between commands, so a command-matching task whose
     restrictions never overwrite it (the malformed-bracket case) inherits the
     previous command's leftover -- e.g. Anno 1700's reference-free OpeningHid
     ("##A#"), which thereby fails *with output* and ticks the turn.  NULL == "". */
  const a5_xml_node_t *restriction_text;

  /* Deferred-completion sink (see a5run_action.cpp run_general).  When non-NULL
     during a completion-message render, a random `<# OneOf/Either/Rand #>`
     expression is FROZEN (its operands substituted) and replaced by a
     `\004<idx>\004` sentinel instead of being drawn -- the frozen body is
     appended to *expr_defer (a std::vector<std::string>*), and the owning
     command evaluates it (drawing) at end-of-command Display, mirroring the runner's
     AggregateOutput responses whose ReplaceExpressions runs at flush
     (clsUserSession.vb:1211 skips the draw when AggregateOutput; Display's
     ReplaceALRs->ReplaceExpressions draws it after every action).  Transient:
     set synchronously around one emit, never live across a save. */
  void *expr_defer;
} a5_state_t;

extern a5_state_t *a5state_new  (const a5_adventure_t *adv);
extern void        a5state_free (a5_state_t *st);

/* Index helpers (linear; -1 if absent). */
extern int a5state_object_index    (const a5_state_t *st, const char *key);
extern int a5state_character_index (const a5_state_t *st, const char *key);
extern int a5state_variable_index  (const a5_state_t *st, const char *key);
extern int a5state_task_index      (const a5_state_t *st, const char *key);
extern int a5state_location_index  (const a5_state_t *st, const char *key);

/* Mark a location as seen by the player (clsCharacter.HasSeenLocation = True,
   set on every player move and for the start location).  NULL/unknown keys are
   ignored. */
extern void a5state_mark_loc_seen (a5_state_t *st, const char *lockey);

/* Switch the active player-centric "seen" arrays (obj_seen/char_seen/loc_seen)
   to those of character index `new_ci`, snapshotting the outgoing player's set
   into its stash slot first (ADRIFT BECOME viewpoint switch).  A no-op when the
   target is already active.  See seen_stash_* in a5state.h. */
extern void a5state_switch_seen (a5_state_t *st, int new_ci);

/* The player's current location key (NULL if unknown). */
extern const char *a5state_player_location (const a5_state_t *st);

/* The current player character's key (clsAdventure.Player.Key).  "Player" until a
   ToSwitchWith/BECOME retargets it.  Never NULL. */
extern const char *a5state_player_key (const a5_state_t *st);

/* clsCharacter.IsInGroupOrLocation: is character `charkey` at location `key`, or
   at a location that is a member of group `key`?  charkey NULL => the Player. */
extern int a5state_in_group_or_location (const a5_state_t *st,
                                         const char *charkey, const char *key);

/* SetLook look-text stack (clsEvent): push a rendered look entry; fetch the
   most-recent one whose location/group gate matches the player (or NULL). */
extern void        a5state_push_look (a5_state_t *st, const char *loc_key,
                                      const char *text);
extern const char *a5state_player_look (const a5_state_t *st);

/*
 * Does object `oi` exist at location `lockey`?  When `directly` is set, only a
 * direct placement counts (an object on/in another object, or held by a
 * character, is not "directly" in the room).  Mirrors ExistsAtLocation.
 */
extern int a5state_object_at_location     (const a5_state_t *st, int oi,
                                           const char *lockey, int directly);
extern int a5state_object_key_at_location (const a5_state_t *st, const char *objkey,
                                           const char *lockey, int directly);

/*
 * Is object `oi` *visible* at location `lockey`?  Like a5state_object_at_location
 * but mirrors clsObject.BoundVisible / IsVisibleAtLocation: an object inside a
 * closed opaque openable container resolves to the container key (not the room),
 * so it is hidden.  Used for scope/visibility and end-of-turn seen-tracking,
 * where the runner uses CanSeeObject/IsVisibleTo rather than the raw ExistsAtLocation.
 */
extern int a5state_object_visible_at_location (const a5_state_t *st, int oi,
                                           const char *lockey, int directly);

/*
 * Is character `ci` visible at location `lockey`?  Mirrors
 * clsCharacter.IsVisibleAtLocation (via BoundVisible): a character "At Location"
 * matches that location; one "On Object"/"In Object" matches wherever its
 * carrier object exists (resolved through the container chain).  Used by the
 * location renderer's "characters present" list, which includes characters
 * seated on / inside furniture in the room.
 */
/* The character's effective location key (char_loc, or resolved through the
   on/in-object carrier chain); NULL when hidden. */
extern const char *a5state_character_location_key (const a5_state_t *st, int ci);
extern int a5state_character_at_location (const a5_state_t *st, int ci,
                                          const char *lockey);

/*
 * Is character `ci` *visible* at `lockey`?  Mirrors clsCharacter.BoundVisible /
 * IsVisibleAtLocation: like a5state_character_at_location, but a character
 * inside an openable closed opaque container binds to the container key and is
 * visible nowhere, and an on/in-object carrier is resolved with the object
 * BoundVisible rules.  Used where the runner uses CharactersVisibleAtLocation (the
 * ViewLocation "is here" list).
 */
extern int a5state_character_visible_at_location (const a5_state_t *st, int ci,
                                                  const char *lockey);

/*
 * Property access with the runtime override layer.  Returns the overridden
 * value if SetProperty has changed it, else the model's value, else NULL.
 * `kind` is informational only (overrides are keyed by entity key, which is
 * unique across the adventure).
 */
extern const char *a5state_entity_prop (const a5_state_t *st, const char *entkey,
                                        const char *propkey);
extern void a5state_set_prop (a5_state_t *st, const char *entkey,
                              const char *propkey, const char *value);

/* Does an entity (object/character/location) HAVE a property, regardless of
   value?  Mirrors clsItem.HasProperty over the merged (runtime override +
   static model) table -- a runtime `<Unselected>` override removes it.  Unlike
   a5state_entity_prop this distinguishes a present-but-valueless SelectionOnly
   marker (returns 1, value NULL) from an absent property (returns 0). */
extern int a5state_entity_has_prop (const a5_state_t *st, const char *entkey,
                                    const char *propkey);

/* Runtime object-group membership (AddObjectToGroup/RemoveObjectFromGroup):
   effective membership = runtime override else the model <Member> list. */
extern int  a5state_object_in_group (const a5_state_t *st, const char *grpkey,
                                     const char *objkey);
extern void a5state_set_object_in_group (a5_state_t *st, const char *grpkey,
                                         const char *objkey, int present);

/* Reconstruct the live insertion-ordered arlMembers list after a restore, from
   the @InGroup property overrides (which are what the save actually stores). */
extern void a5state_rebuild_live_groups (a5_state_t *st);

/* Live, insertion-ordered membership of a group (the runner clsGroup.arlMembers): the
   count and the i-th member key.  Reflects runtime Add/Remove*ToGroup on top of
   the static <Member> list -- unlike adv->groups[g].members which is static.
   Used for RandomKey selection and EverywhereInGroup enumeration. */
extern int  a5state_group_count (const a5_state_t *st, const char *grpkey);
extern const char *a5state_group_member_at (const a5_state_t *st,
                                            const char *grpkey, int i);

/* Direct live-list mutators (distinct append / order-preserving remove), the
   runner arlMembers.Add/Remove.  Normally reached via
   a5state_set_object_in_group; exported for the save/restore path, which must
   rebuild the live list in the SAVED member order. */
extern void a5state_group_add_member (a5_state_t *st, const char *grpkey,
                                      const char *key);
extern void a5state_group_remove_member (a5_state_t *st, const char *grpkey,
                                         const char *key);

/* A location's inherited Locations-group property (e.g. the dynamic
   ShortLocationDescription darkness property), or NULL. */
extern const a5_prop_t *a5state_location_group_prop (const a5_state_t *st,
                                  const char *lockey, const char *propkey);

/* Conversation state setters (own the string; "" clears). */
extern void a5state_set_conv_char (a5_state_t *st, const char *key);
extern void a5state_set_conv_node (a5_state_t *st, const char *key);

/* <DisplayOnce> tracking: has this description-segment node already been shown,
   and (when marking) record that it has. */
extern int  a5state_disp_once_seen (const a5_state_t *st, const void *node);
extern void a5state_disp_once_mark (a5_state_t *st, const void *node);
extern void a5state_disp_once_unmark (a5_state_t *st, const void *node);

/* Per-turn reference bindings. */
extern void        a5state_clear_refs  (a5_state_t *st);
extern void        a5state_bind_ref    (a5_state_t *st, const char *name, const char *value);
extern const char *a5state_lookup_ref  (const a5_state_t *st, const char *name);

/* Variable lookup by Name (text engine) or Key (restrictions). */
extern int         a5state_var_num_by_name (const a5_state_t *st, const char *name, long *out);
extern const char *a5state_var_text_by_name (const a5_state_t *st, const char *name);

/* Numeric array-variable element access (idx is ADRIFT's 1-based index).
   Out-of-range reads return 0 and writes are dropped (clsVariable's ErrMsg
   paths).  On a scalar variable idx is ignored (the runner SetVariable: the [index]
   suffix on a Length-1 variable is meaningless and IntValue defaults to 1). */
extern long a5state_var_get_elem (const a5_state_t *st, int vi, long idx);
extern void a5state_var_set_elem (a5_state_t *st, int vi, long idx, long value);

/* Scoped set of st->marking_display -- the runner's ambient bTestingOutput.
   Rendering happens in two modes: a REAL render (marking_display=1) retires
   <DisplayOnce> segments and runs the Introduced dance, a peek/test render
   (0) must leave both untouched.  Every such render has to put the previous
   value back afterwards, and doing that by hand is how a5run_action's
   Execute-Look path ended up needing the same restore on two separate exits.
   Declare the guard instead:  a5_mark_guard mg (st, 1);

   A body may still assign st->marking_display directly within the scope (the
   look-then-test sequences do); the guard restores whatever was current when
   it was constructed. */
struct a5_mark_guard {
  a5_state_t *st;
  int prev;
  a5_mark_guard (a5_state_t *s, int value) : st (s), prev (s->marking_display)
  {
    s->marking_display = value;
  }
  ~a5_mark_guard () { st->marking_display = prev; }
  a5_mark_guard (const a5_mark_guard &) = delete;
  a5_mark_guard &operator= (const a5_mark_guard &) = delete;
};

#endif
