# ADRIFT 4 vs ADRIFT 5 in Scarier

---

## 1. Detection and Dispatch

Entry point is `glk_main()` in `os_glk.cpp`. `gsc_startup_code()` tries `a5model_load()` first — it returns `NULL` cleanly on any non-v5 file. On success, `gsc_is_a5 = TRUE` and `gsc_a5_main()` is called. On failure, `scr_game_from_callback()` loads the v4 TAF and `gsc_main()` runs.

The v4 path stores an opaque `scr_game_s *` handle. The v5 path stores `a5_adventure_t *gsc_a5_adv` and instantiates an `a5_run_t *` per session.

---

## 2. File / Container Format

### ADRIFT 4 (TAF — `sctaffil.cpp`)

Three sub-versions distinguished by a 12-byte magic signature:
- **v4.0**: payload is **zlib-compressed** (deflate). Embedded resources (sounds, graphics) may follow the game data; `parse_add_resources_offset()` records their byte offset.
- **v3.9 / v3.8**: payload is **XOR-obfuscated** using the Visual Basic PRNG (`taf_random()`, seeded at `PRNG_INITIAL_STATE = 0x00a09e86`).

The decompressed stream is a sequence of newline-terminated text fields, with per-version 3-byte multi-line separators (`{0xbd,0xd0,0x00}` for v4.0, `{0x2a,0x2a,0x00}` for v3.9/3.8). This stream is walked by the schema-driven parser `parse_game()` in `sctafpar.cpp` according to one of three parse schema tables (`V400_PARSE_SCHEMA` etc.), producing a flat `scr_prop_setref_t` property bundle.

Saved games use the same zlib-compressed text format, beginning with `SER_VERSION_HEADER = "\xAC400052"`. Battle state is appended after the turn count as a private `ScarierBattleState/2` block.

### ADRIFT 5 (Blorb-wrapped compressed XML — `a5blorb.cpp`, `a5deobf.cpp`, `a5xml.cpp`)

The file is either a **Blorb IFF container** (`FORM…IFRS`) or a raw ADRIFT 5 payload. `a5blorb_find_exec()` locates the `Exec/0` chunk via the `RIdx` resource index when Blorb-wrapped.

The payload has a 12- or 16-byte header (16 bytes when `data[12..15] == "0000"`). The region after the header is **XOR-deobfuscated** with a fixed 1024-byte key (`A5_KEY` in `a5deobf.cpp`, copied from FrankenDrift's `iRandomKey`), then **zlib-inflated** to UTF-8 XML.

The XML is parsed by a custom minimal hand-written DOM parser (`a5xml.cpp` — no external library), then indexed into `a5_adventure_t` by `a5model_from_doc()`.

Media (images, sounds) are **Blorb Pict/Snd resources** addressed by number. `<FileMappings>/<Mapping>` records map source filenames to Blorb resource numbers. No embedded-after-game blobs.

Saved games are XML text (`<SaveState>…</SaveState>`), not zlib-compressed at the engine level.

---

## 3. Object Model

### ADRIFT 4 (`scgamest.h`)

A flat `scr_game_s` struct with parallel arrays:
- `scr_roomstate_t rooms[]` — just a `visited` bool per room; descriptions and exits live in the property bundle.
- `scr_objectstate_t objects[]` — `position` (room+1, or sentinel: `OBJ_HIDDEN=-1`, `OBJ_HELD_PLAYER=0`, `OBJ_WORN_PLAYER=-100`, `OBJ_ON_OBJECT=-20`, etc.), `parent`, `openness`, `state`, `seen`, `unmoved`.
- `scr_taskstate_t tasks[]` — `done`, `scored`.
- `scr_eventstate_t events[]` — `state` (WAITING/RUNNING/AWAITING/FINISHED/PAUSED), `time`.
- `scr_npcstate_t npcs[]` — `location`, `position`, `parent`, `walkstep_count`, `walksteps[]`, `seen`, plus battle fields: `stamina`, `staminacounter`, `attackcounter`, `scr_battle_t battle`.
- Player: `playerroom`, `playerstamina`, `playerstaminacounter`, `playerwield`, `playerbattle`.
- Pronouns: integer indices (`it_object`, `him_npc`, etc.).
- Undo: `game->undo` is a full deep copy of `scr_game_s` (one level).

### ADRIFT 5 (`a5model.h`, `a5state.h`)

Two distinct layers — immutable model and mutable runtime state.

**Immutable model** (`a5_adventure_t`): everything addressed by **string key**, not integer index.
- `a5_object_t[]` — key, article, prefix, `names[]`, `props[]` (`a5_prop_t`: key + scalar value + rich `<Value>` DOM node).
- `a5_location_t[]` — key, `props[]`, DOM node (carries `ShortDescription`, `LongDescription`, `Movement`).
- `a5_character_t[]` — key, name, type (Player/NonPlayer), perspective, `descriptors[]`, `props[]`, `topics[]` (`a5_topic_t`), `walks[]` (`a5_walk_t`).
- `a5_task_t[]` — key, priority (long), type (General/Specific/System), `commands[]`, DOM pointers for restrictions/actions/fail_override; Specific-override linkage (`general_key`, `specifics[]`).
- `a5_variable_t[]` — key, name, type (Numeric/Text), initial value.
- `a5_event_t[]` — key, type, `when_start`, duration ranges, `subevents[]` (`a5_subevent_t`).
- `a5_group_t[]`, `a5_propdef_t[]`, `a5_alr_t[]`, `a5_udf_t[]`, `a5_filemap_t[]`.

**Mutable runtime state** (`a5_state_t`):
- `a5_objloc_t obj[]` — `where` (enum: `A5_OWHERE_NONE/HIDDEN/ALLROOMS/LOCATION/LOCGROUP/ON_OBJECT/IN_OBJECT/HELD_BY/WORN_BY/PART_OBJECT/PART_CHAR`), `key`.
- `char_loc[]`, `char_position[]`, `char_onobj[]`, `char_in[]` — character positions.
- `var_num[]`, `var_text[]` — numeric and text variable values.
- `task_done[]` — per-task completion flags.
- `a5_prop_ov_t ov[]` — runtime property override list (SetProperty layer).
- `conv_char[]`, `conv_node[]` — conversation state.
- `char_seen[]`, `obj_seen[]` — per-turn seen sets.
- `s_it/s_them/s_him/s_her` — pronoun text strings (not indices).
- `ref_name[16][32]`, `ref_value[16][256]` — per-turn reference bindings.
- `a5_looktext_t looks[]` — SetLook event text stack.
- `disp_once[]` — `<DisplayOnce>` DOM nodes already shown.
- `route_error`, `restriction_text` — last restriction message node.
- No battle system fields. Undo lives on `a5_run_t` (the `undo_stack` snapshots), not in `a5_state_t`.

---

## 4. Text Engine

### ADRIFT 4 (`scprintf.cpp`)

`pf_filter()` is the printfilter: expands `%variable%` and `<tag>` markup in two passes (expressions via `scexpr.cpp`, then ALR replacement via `pf_replace_alrs()`). ALRs are sorted by original-string length (longest first) and applied globally.

Output routes through two OS callbacks: `os_print_string()` for text and `os_print_tag(tag, argument)` for 17 `SCR_TAG_*` formatting tags (bold, italics, colour, wait, clear screen, etc.). The Glk host maps these to Glk styles via a font stack (`gsc_font_stack[]`).

Media is delivered as `os_play_sound(filepath, offset, length, is_looping)` and `os_show_graphic(filepath, offset, length)` — file-path plus byte offset into the TAF blob.

### ADRIFT 5 (`a5text.cpp`, `a5expr.cpp`, `a5sexpr.cpp`, `a5arith.cpp`)

`a5text_describe(st, wrapper_node)` evaluates a `<Description>` DOM node into malloc'd UTF-8 plain text. Iterates `<SingleDescription>` elements, each gated by `<DisplayWhen>` restrictions, containing `<Text>`, `<Property>`, `<Function>`, `<If>`, and `<DisplayOnce>` one-shot segments.

`%reference%` chains like `%Player.Location.ShortDescription%` are resolved by `a5expr_eval()` (`a5expr.cpp`) walking `.`-separated property/function names. Numeric expressions for variable assignments use `a5arith.cpp`. String-typed `SetToExpression` uses `a5sexpr.cpp`, supporting `len`, `left`, `right`, `mid`, `upper`, `lower`, `rand`, `urand`, `oneof`, arithmetic, comparisons, and logic operators.

ALR replacement runs at two levels: per-fragment inside `a5text_describe`, and at the Display boundary over the full assembled turn output (`alr_boundary_apply()`). Each ALR's `NewText` is itself a rich description block rendered recursively.

UDFs (`a5_udf_t`) are user-defined text macros — a v5-only feature with no v4 equivalent.

All markup is stripped to plain text by `a5text_render_plain()`. There are **no `os_print_tag` calls from the v5 path** — the Glk host receives plain UTF-8 via `gsc_a5_put_string()`. In interactive mode, presentation tags survive as positional sentinel marks (a5text.h): `<cls>`, `<waitkey>`, `<center>`/`<b>` spans, `<window>` routing, and `<wait N>` timed pauses (`A5_WAIT_MARK`, run through the same cancelable Glk timer delay as the v4 `<wait x.x>` tag — Death Shack types its title one 80-point letter per `<wait 3>`).

Media is a **side-channel**: `<img src="…">` and `<audio play|stop|pause src="…" channel=N loop=Y>` tags are parsed during rendering and queued in `run->media` (`std::vector<a5_media_event_t>`). In interactive mode each tag also leaves a positional mark in the turn text (`A5_IMG_MARK` resource number / `A5_SOUND_MARK` media-list index), so `gsc_a5_display` draws the image or fires the sound *where the tag sits* — crucially, before any later `<waitkey>` pause (Pervert Action Crisis cues a sting ahead of a keypress-paced cutscene). A fired sound is flagged `shown`; `gsc_a5_show_media()`'s after-the-turn sweep then plays only the leftovers (events whose rendering never reached the display) and counts images for the cover-art fallback. Audio channels are 1..8 and **default to 1** when the tag has no `channel=` attribute (Runner Global.vb / clsSound.vb); playing the sound a channel is already playing leaves it alone, playing a different one replaces it (`A5_TRACE_MEDIA=1` traces the dispatch).

---

## 5. Parser / Command Matching

### ADRIFT 4 (`scparser.cpp`)

Single-pass recursive-descent tokenizer (`uip_tokenize_start()`, `uip_next_token()`). Patterns stored as strings with `%text%`, `%object%`, `%character%`, `%number%`, `[optional]`, `{alt/choices}` grammar. Tried against the `V$Command[]` list per task. A hardcoded built-in command table in `scrunner.cpp` handles `take`, `drop`, `examine`, `go`, `wear`, `attack … with`, etc. Match is case-insensitive. Locale synonym expansion via `sclocale.cpp`.

### ADRIFT 5 (`a5parse.cpp`)

`a5parse_match_command(pattern, input, a5_match_t *m)` converts ADRIFT 5 command patterns to POSIX EREs via `ConvertToRE` (ported from FrankenDrift), then applies `regcomp`/`regexec`. Reference groups become capture groups; `a5parse_ref(m, name)` retrieves captures by name.

Direction patterns are localized via `a5parse_set_directions()`, feeding `adv->dir_re[12]` from `<DirectionNorth>…<DirectionOut>` header fields. `a5parse_directions_re()` assembles the full alternation regex.

There is **no hardcoded built-in command table**. All commands — including movement — are expressed as tasks. Disambiguation: when a reference resolves to multiple candidates the engine sets `run->amb_active` / `run->amb_keys` / `run->amb_ref_name` and re-prompts "Which X?" on the next turn. Remembered verb: a bare verb matching a "verb %ref%" pattern stores the verb in `run->remembered_verb` and prepends it to the next input. "get all" / `%objects%` plural references expand to all matching in-scope items; per-item actions loop and deduplicate messages via `run->resp`.

---

## 6. Task / Restriction / Action System

### ADRIFT 4 (`sctasks.cpp`, `screstrs.cpp`)

Tasks stored in the property bundle, indexed by integer. Each task has `V$Command[]`, `$CompleteText`, `$ReverseMessage`, `V<TASK_RESTR>Restrictions[]`, `V<TASK_ACTION>Actions[]`, `$RestrMask` (a string of `A`/`O` tokens for AND/OR combining).

Restriction types: **6** integer types (0 = object location, 1 = object state, 2 = task completion, 3 = character location, 4 = variable comparison, 5 = action-existence check). Each uses up to three integer `Var` fields. Type 5 checks a sentinel value (0xEC) in restriction field `[1A]` and delegates to the action-type counter — confirmed in `run400.exe` Sub_20_3 at the type-5 dispatch branch.

Action types: the executor (`Sub_20_11` in the original Runner, `sctasks.cpp` in Scarier) handles **8** types (0–7). The data model and action type-counter (`Sub_20_13`) recognise **10** types (0–9); types 8 and 9 are parsed and counted but the executor silently ignores them. Type 4 handles both score changes and end-game (confirmed by `"(Your score has increased by "` / `"(Your score has decreased by "` string literals in the type-4 block).

| Type | Meaning |
|------|---------|
| 0 | Move object |
| 1 | Move player |
| 2 | Set/open/close object |
| 3 | Set variable |
| 4 | Score change / end game |
| 5 | Move NPC |
| 6 | Change object state |
| 7 | Change battle attribute |
| 8–9 | Parsed by data model; executor ignores |

Task scoring via `taskstate.scored`; one undo level via `game->undo` deep copy.

### ADRIFT 5 (`a5restr.cpp`, `a5run.cpp`)

Tasks sorted by `priority` (long, descending) into `run->order`. Specific (override) tasks linked to their General parent via `general_key` and `specifics[]`.

Restrictions are DOM nodes. Each `<Restriction>` has a `<RestrictionType>` string — over 30 distinct types including `ObjectInLocation`, `TaskCompleted`, `CharacterInLocation`, `HaveBeenSeenByCharacter`, `HaveRouteInDirection` (which also resolves the exit's own restriction message into `st->route_error`), `BeInState`, `ExpressionMeetsThreshold`, `ConversationInProgress`, `BePartOfCharacter`, `BePartOfObject`, etc. `<BracketSequence>` (`#A#O#` tokens) drives AND/OR nesting with short-circuit evaluation.

Actions are DOM nodes with a `<ActionType>` string — ~25 types: `MoveObject`, `MoveCharacterTo`, `SetVariable`, `IncVariable`, `DecVariable`, `SetProperty`, `ExecuteTask`, `UnSetTask`, `EndGame`, `DisplayMessage`, `AddObjectToGroup`, `RemoveObjectFromGroup`, `SetConversationNode`, `SetTasks`, `ExitConversation`, `DisplayMenu`, etc.

Additional task flags: `continue_lower` (keep scanning after a passing task), `low_priority` (skip once a failing-with-output task has been seen), `aggregate`/`AggregateOutput` (defer room-view rendering to post-action state). Game-level `adv->hp_passing` (`HighestPriorityPassingTask`) controls how failing tasks with output are handled.

System tasks with `run_immediately = TRUE` run before the title. Single-level undo is provided by the frontend snapshotting the pre-turn state (`a5run_snapshot`) and restoring it (`a5run_undo`).

---

## 7. Turn Loop, Events, NPC Walks

### ADRIFT 4 (`scrunner.cpp`, `scevents.cpp`, `scnpcs.cpp`)

`run_main_loop(game)` per command: parse + match + run task → `npc_tick_npcs()` (Sub_20_2 in the original Runner) → `evt_tick_events()` (Sub_20_32). **Events and NPCs are ticked after command execution**, not before — confirmed in `run400.exe` Sub_20_62 (the main turn handler), which ends with Sub_20_2 then Sub_20_32 after all verb dispatch. This matches the ADRIFT 5 ordering (command → walks → events). Scarier's v4 implementation (`run_main_loop`) runs events and NPCs *before* the command, which is the reverse of both originals.

Events have `state` stored as an integer byte (0–4) rather than a named enum; the names WAITING/RUNNING/AWAITING/FINISHED/PAUSED are Scarier's own. Sub-objects are moved at specified timer values. NPC walks use step-records with I2 target room numbers across 13 slots (slot 12 = "stay"); walk tasks fire on room boundary crossings via the room-entry handler. Flow control (quit, restart, save, restore) in the original Runner uses VB6 form unloading and flag variables — `run_loop_halt {}` C++ exceptions are Scarier's own port mechanism, not a property of the ADRIFT 4 model.

### ADRIFT 5 (`a5run.cpp`)

`a5run_input()` per command. After processing, `ev_tick_all()` advances all events and walks (`TurnBasedStuff`). Walks run first (`wk_tick_all`), then events.

Walk runtime (`a5_walk_rt`): status, length, timer, step index, per-subwalk `sw_ft[]` (turn offsets). `wk_do_steps()` moves the character to its next step. `wk_do_subwalks()` fires sub-walk activities (DisplayMessage, ExecuteTask, UnsetTask, ComesAcross meet trigger).

Event runtime (`a5_event_rt`): status, `length_value`, `timer_to_end`, `last_se_time/index`, `just_started`, `next_command`. Sub-events fire at FromLastSubEvent / FromStartOfEvent / BeforeEndOfEvent turn offsets. Event controls (`a5_eventctrl_t`) start/stop/suspend/resume events on task completion; walk controls use the same mechanism. `run->events_running` gates immediate vs deferred Start/Stop.

Meta-commands (quit, restart, save, restore) are handled at the `gsc_a5_main()` host level — they never reach the engine. `a5run_is_over()` signals end-of-game (no `run_loop_halt` exception). The Glk port's `glk ...` command escapes (`GSC_COMMAND_TABLE`) are shared with the v4 path: the a5 loop offers summary, script, inputlog, readlog, version, commands, license and help, while the ADRIFT ≤4 specifics (abbreviations, capacity, combatassist, moveassist, verbose) are filtered out via the table's `in_adrift5` flag (`gsc_command_in_scope()`).

---

## 8. Save / Restore Format

### ADRIFT 4 (`scserial.cpp`)

Text lines, one field per line, in fixed positional order matching the TAF schema. Stream is zlib-compressed via `deflateInit`/`deflate` in `ser_flush()`. Begins with `"\xAC400052"`. Battle state appended as `ScarierBattleState/2`. Undo via deep copy of `scr_game_s`.

### ADRIFT 5 (`a5run.cpp` — `a5run_save` / `a5run_restore`)

XML text (`<SaveState>…</SaveState>`), not engine-compressed. Written/read as a binary blob via `glk_stream_open_file`. Contents (in order): version, RNG state (`<RngNative>` + four `<Rng>` words via `a5rand_get/set_state()`), game flags, conversation state, pronouns, per-object `<Object>` blocks (where + key), per-character `<Character>` blocks (location, position, onobj, in), per-variable `<Variable>` blocks, sparse `<TaskDone>` elements, sparse `<PropOv>` overrides (entity + prop + value), sparse `<ObjSeen>` / `<CharSeen>`, per-event `<Event>` blocks (status, length, timer, subevent fingerprints), per-walk `<Walk>` blocks, `<DisplayOnce>` nodes by DOM fingerprint, `<GroupOv>` runtime membership changes, plus per-non-active-viewpoint `<SeenChar>` blocks (per-character seen sets across a BECOME switch). Single-level undo (`a5run_snapshot`/`a5run_undo`) reuses this same serialisation: the frontend snapshots the pre-turn blob before each `a5run_input`, and undo restores it.

The RNG state is saved — a Scarier-specific extension beyond FrankenDrift's format, so restored games replay the identical deterministic sequence.

---

## 9. RNG / Determinism

Both engines use the same `erkyrath_random()` / `set_erkyrath_random()` from `terps/common_utils/randomness.c`.

**v4** (`scutils.cpp`): `scr_randomint(lo, hi)`. TAF v3.8/3.9 deobfuscation uses its own independent Visual Basic PRNG (`taf_random()`) — non-interacting with the game RNG.

**v5** (`a5rand.cpp`): `a5rand_between(lo, hi)` — if `lo == hi`, returns the fixed value without drawing (so fixed-length event timers don't consume RNG state). RNG state is saved and restored in the XML save format (v4 has no equivalent). `a5sexpr_rng_hook` is pluggable; the run layer wires it to `a5rand_between` so string-expression `rand/urand/oneof` uses the same stream as event timers.

---

## 10. Battle System

**ADRIFT 4**: Full battle system (`scbattle.cpp`), enabled by `Globals.BBattleSystem`. Four attribute slots (Strength, Accuracy, Defence, Agility) with `lo/hi/max` ranges. `scr_battle_t` embedded in both `scr_npcstate_t` and `scr_game_s`. Player wields a weapon object (`playerwield`). Task action type 7 mutates attributes at runtime. Library commands `lib_cmd_attack_npc_with`, `lib_cmd_hit_npc_with`, etc. `battle_is_legacy_version()` adjusts roll behaviour for v3.8/3.9 games.

**ADRIFT 5**: No battle system. Not modelled in `a5model.h`, `a5state.h`, `a5run.h`, or `a5restr.cpp`. ADRIFT 5 removed it entirely.

---

## 11. Glk Integration

**ADRIFT 4**: Communicates through 10 OS callbacks declared in `scarier.h`: `os_print_string()`, `os_print_tag(tag, argument)` (17 `SCR_TAG_*` values mapped to Glk styles via `gsc_font_stack[]`), `os_play_sound()`, `os_stop_sound()`, `os_show_graphic()`, `os_read_line()`, `os_confirm()`, `os_open/write/read/close_file()`, `os_display_hints()`. Status line via `scr_get_game_room()` / `scr_get_game_status_line()`. Locale codepage translation via `gsc_locale_t` / `sclocale.cpp`.

**ADRIFT 5**: No OS callbacks. Engine produces plain UTF-8; host prints it via `gsc_a5_put_string()` (inline UTF-8 → `glk_put_char_uni`). Media via per-turn side channel: `a5run_media_count()` / `a5run_media_get()` return `a5_media_event_t` structs; host dispatches `glk_image_draw` and `glk_schannel_play_ext` on up to 16 channels (`gsc_a5_channels[]`). Resources are Blorb-native: host registers the Blorb file as the Glk resource map (`giblorb_set_resource_map`). Cover image from `Fspc` frontispiece chunk (`gsc_a5_show_cover()`). Status line from plain text accessors `a5run_location_name()` / `a5run_score()` / `a5run_turns()`. Line input via `gsc_a5_read_line()` (Unicode, UTF-8 encoded). Meta-commands handled entirely in `gsc_a5_main()` before the engine is called.

---

## 12. Shared Infrastructure

| Component | v4 | v5 |
|---|---|---|
| `randomness.c` (erkyrath xoshiro128**) | `scutils.cpp` | `a5rand.cpp` |
| zlib | TAF inflate + save deflate | payload inflate |
| Blorb / `gi_blorb.h` | Garglk resource chunks | Full Glk resource map |
| `glk_put_char_uni` | via `gsc_put_char_locale()` | via `gsc_a5_put_string()` |
| Property bundle (`scprops.cpp`) | All game data | Not used |
| `a5xml.cpp` (DOM parser) | Not used | All game data |
| `scarier.h` OS callbacks | Used by engine | Bypassed entirely |
| `sclocale.cpp` (codepage) | Used | Not used (native UTF-8) |
| Debug/trace | `scr_set_trace_flags()`, 10 `SCR_TRACE_*` bits, `scdebug.cpp` | `a5run_trace` / `a5restr_trace` (stderr) |
| Undo | `game->undo` deep copy (1 level) | 1 level via `a5run_snapshot`/`a5run_undo` (save/restore snapshot taken before each turn) |
| Conversation system | Separate `ask … about` library verbs | Full topic/keyword engine in `a5run.cpp` `exec_conversation()` |
| UDFs | None | `a5_udf_t[]`, `%FunctionName%` in text |
| Property definitions | Hardcoded schema in parse tables | `a5_propdef_t[]` (typed, author-defined) |
| Groups | None | `a5_group_t[]` — named sets used in restrictions/actions |
