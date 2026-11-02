# Scarier vs. SCARE: the ADRIFT 3.7–4.0 side

This compares the ADRIFT 4-and-earlier half of Scarier (`core/`, `library/`,
`runner/` and the Glk seam in `glk/`) with plain SCARE, the interpreter it was
forked from. The ADRIFT 5 engine in `adrift5/` has no SCARE counterpart and is
left out, except where it costs the ADRIFT 4 side something.

"SCARE" here means Simon Baldwin's SCARE 1.3.10, upstream's last release
(2008), in the copy Spatterlight took from Gargoyle in `ed31bf462` (2017) and
shipped until the fork. Figures are from 2026-10-11.

## In short

| | SCARE 1.3.10 | Scarier (ADRIFT ≤ 4 side) |
|---|---|---|
| Versions played | 3.8, 3.9, 4.0 | 3.7, 3.8, 3.9, 4.0 |
| Reference for behaviour | jAsea, plus bug reports | The four original Runners, command by command |
| Battle system | Parsed, never run | 3.8/3.9 and 4.0 systems both implemented |
| Routes that reach their ending (of 663) | 439 | 663 |
| Games those routes cover (of 609) | 432 | 609 |
| Language | ANSI C (C89) | C-like C++11 |
| Size, engine and front ends | about 44,000 lines, 27 files | about 104,000 lines in 100-odd files, plus 13,000 of Glk seam |
| Time for the same 120 routes | 6.1 s | 2.7 s |
| Tests | `sx` script harness, a few dozen scripts | 677 walkthrough rows, 678 Runner transcripts, 616 probe files, unit and autosave suites |
| Maintained | No (last release 2008) | Yes |

The route figures come from the replay described under
[Games that work in Scarier but not in SCARE](#games-that-work-in-scarier-but-not-in-scare).
Read the caveats there before quoting them.

## Where Scarier is better

### It plays more games

- **ADRIFT 3.7.** SCARE does not recognise the format. Scarier plays the two
  known 3.7 games, *Alice's Restaurant Anti-Massacree Adventure* and *Castle
  Quest*.
- **Games SCARE cannot parse or survive.** *Sophie's Adventure* stops at
  "error parsing game data", and eleven more games abort mid-game on an
  internal error or a failed assertion. Scarier loads and finishes all of them. A dangling
  reference that the loader cannot repair now ends the game with a message
  instead of an abort.
- **The battle system.** SCARE reads the battle properties and ignores them.
  Scarier implements both the 3.8/3.9 system and the quite different 4.0 one
  (stamina, weapons, armour, wielding, thrown weapons, the narration), checked
  against the 3.9 and 4.0 Runners. Of the 84 corpus games with the battle flag
  set, SCARE finishes the route in 25, and those are mostly games that set
  the flag without depending on it: the fight is handled by the game's own
  tasks, or is not needed to reach the ending. A game whose ending is
  gated on winning a library fight should not be expected to work in SCARE.
- **Name and gender prompts.** SCARE parses `PromptName` and the player's
  gender but never asks. A game that tests "the Player is Male" can then never
  pass that restriction; *The Secret of the Lost World* gates its castle on it.
- **Games broken by their own data.** 37 games that cannot be won on the
  original Runner either (*Ebony's World*, *Bedlam*, *The Spirit's Flight* and
  others) are patched as they load, each keyed to the exact release. `glk
  patches off` gives the original behaviour.
- **Games broken by version conversion.** Five opt-in assists (`glk assist
  combat`, `move`, `repeat`, `room`, and capacity) cover games that stopped
  working when their authors converted them to 4.0. Thirteen known games get
  the right assist switched on at startup, with a notice.

### It behaves like the Runner the game was written for

SCARE reimplements jAsea's reverse engineering, and treats 3.8 and 3.9 games
mostly as 4.0 games with a different file format. Scarier was measured against
all four Runners under Wine, and each version follows its own rules.

- **ALR text replacement works.** Games rewrite the interpreter's own output by
  exact string match, so an interpreter that words a library message
  differently silently breaks them. This is the main practical reason parity
  matters.
- **Per-version library behaviour.** Take, put, open and refusal wording, PUT
  IN and PUT ON, container listings, the disambiguation prompt, what counts as
  seen, and how restrictions and descriptions are evaluated all differ between
  3.7/3.8, 3.9 and 4.0.
- **Parser and world model.** Noun resolution and tie-breaking, PUT and TAKE
  precedence, wildcard task matching, synonym rewriting, the one-game-task-
  per-line rule, and container capacity as a volume instead of a count.
- **Events and character walks** fire on the same tick as in the Runners.
- **Random numbers** can be drawn from a Runner-compatible stream, so games of
  chance and the walkthroughs that depend on them play out as they do there.
- **Saves.** Scarier reads and writes both the 4.0 and the pre-4.0 Runner save
  formats, so saves move between it and the original Runners in both
  directions. Exchanging saves with the Runners was added after the fork.

### It is nicer to play

- A clickable, zoomable map pane, in ADRIFT 4 games too.
- Pictures and sounds, including media a game names but did not pack.
- Text colours, and Webdings, Wingdings and Symbol text translated to Unicode.
- Spatterlight autosave at every kind of pause: the prompt, a keypress, the
  name and gender questions, a pending "Which one?".
- `glk undo`, `glk restore`, `glk restart` and `glk quit` for moments when the
  game's own commands are out of reach.
- UNDO, Z, LOOK X, GO TO and comma-separated commands are kept even in 3.7 and
  3.8 games, whose Runners lacked them. A command that matches no task under
  the Runner's strict rules is retried under looser ones.

### It is faster and better tested

- The same 120 winning routes take 2.7 s in Scarier and 6.1 s in SCARE. The ALR
  table is indexed, undo snapshots are no longer compressed and reuse unchanged
  sections, and the map's static data is read once.
- 677 walkthrough rows are replayed and strict-diffed against goldens on every
  suite run, 43 s for the full suite and about 4 s for the quick one. Behind
  them sit 678 files of original-Runner transcripts and 616 probe files from
  synthetic games built to isolate one rule each.
- ASan and UBSan builds of the unit harnesses, and an autosave regression
  suite.

## Where SCARE is better

### Portability and footprint

- SCARE is strict ANSI C and has run on Linux, Windows, DOS, classic Mac OS and
  Amiga. Scarier needs a C++11 compiler. It builds with Clang, GCC 9 and Visual
  C++, and nothing older or smaller has been tried.
- SCARE carries its own zlib and needs nothing else. Scarier links the system
  zlib and the shared `common_utils` random number code.
- SCARE is about 44,000 lines in 27 files, and one person can hold it in their
  head. The same job in Scarier is about 104,000 lines, with the command
  library alone going from 11,000 lines to 38,000.
- Most of the extras (map, autosave, media lookup) live in the Glk seam or in
  Spatterlight-specific code. The plain ANSI build gets the engine fixes and
  none of the comforts.

### A stable base other projects share

- SCARE's public API is `scare.h` with `sc_` names. Scarier renamed it to
  `scarier.h` and `scr_`, so it is not a drop-in replacement in Gargoyle or any
  other host that embeds SCARE.
- SCARE has been frozen since 2008. Its bugs are known and fifteen years of
  bug reports and walkthroughs describe its behaviour. Scarier changes weekly,
  1,322 commits so far, and a save or transcript from last month's build may
  not match this month's.
- SCARE ships BUILDING, PORTING, RUNNING, DEBUGGER and TESTING documents.
  Scarier has one README; much of what a porter needs is in source comments
  and commit messages.

### Simplicity of behaviour

- SCARE has one set of rules. Scarier has up to four, selected by game version,
  plus over a hundred source notes marking deliberate deviations from the
  Runners, 37 game-specific
  patches, five assists and a table of games that get assists automatically.
  Explaining why a command got a particular answer takes more work.
- Runner parity cuts both ways. Scarier ports some Runner behaviour that is
  stricter or stranger than SCARE's, and it dropped a dozen commands SCARE had
  invented (STATS, BRIEF, VERBOSE, NOTIFY and others as bare words) because
  they stole input from games' own tasks. A player used to SCARE will find a
  few things missing or reworded. The policy is to keep conveniences and
  grammatical text wherever no game depends on the Runner's accident, but the
  call is made case by case.
- Patching a game at load is an editorial act. It is keyed to exact releases,
  announced, and can be switched off, but SCARE never alters a game.

### Provenance and verification

- SCARE was written by hand by one author over five years. The Scarier README
  describes it as "a lazy AI C++ clone"; most of its 1,322 commits were written
  with AI assistance and reviewed through tests more than through reading.
- The evidence behind Scarier's behaviour is hard to reproduce elsewhere. It
  needs the original Runners under Wine with a custom driver, and a corpus of
  about 700 third-party game files that cannot be committed. The manifest pins
  them by sha256, but several have already vanished or been silently replaced
  upstream.
- The suite proves that Scarier matches its goldens, and the goldens were
  checked against Runner transcripts where one exists. Where none exists, a
  golden records what Scarier does, not what is right.

### Licence

SCARE is GPL version 2 only. Files Scarier inherited keep that; files written
for Scarier are version 2 or later. The combined work is therefore still
version 2 only, with no practical change for users.

## Games that work in Scarier but not in SCARE

### How this was measured

SCARE 1.3.10 was built from `ed31bf462` with its own ANSI front end. Three
harness-only changes were made to `os_ansi.c` so that it reads a script the way
Scarier's test build does: `#` comment lines are skipped, `SCR_SKIP_WAITKEY`
makes key pauses transparent, and the random seed is settable. One change was
made to the engine: it prints the Runner's end-of-game score summary ("You
scored 50 out of the maximum 50!" and the lines after it), at the end of the
turn as the Runner does. SCARE prints no summary, and about a third of the
win markers are a line of it, so without this a route that finished in SCARE
was counted as a failure. Nothing else in the engine was touched.

The run was first made against SCARE 1.3.9, Spatterlight's original 2017
import. 1.3.10's loader fixes let six more games load: three of them then
finish their routes, two run without finishing, and *Through Time* aborts
part-way. Nothing else changed between the two versions.

Every row of `test/adrift4/harness/run_v4_walkthroughs.sh` that has a win
marker (663 of 677) was then replayed through both interpreters. A row counts
as working when the marker text appears in the output, comparing with runs of
white space collapsed because SCARE's front end wraps lines. Lines that answer
Scarier's name and gender prompts were removed from SCARE's input, since SCARE
never asks. SCARE was given five seeds per row and credited if any one reached
the marker.

| | Rows | Games |
|---|---|---|
| Reach the marker in Scarier | 663 | 609 |
| Reach the marker in SCARE | 439 | 432 |
| SCARE: route runs but does not reach the marker | 205 | 163 |
| SCARE: aborts during the route | 16 | 11 |
| SCARE: refuses to load the game | 3 | 3 |

By game version:

| Version | Games | Finish in SCARE | Do not finish | Abort | Not loaded |
|---|---|---|---|---|---|
| 4.0 | 406 | 304 | 92 | 9 | 1 |
| 3.9 | 183 | 120 | 62 | 1 | 0 |
| 3.8 | 18 | 8 | 9 | 1 | 0 |
| 3.7 | 2 | 0 | 0 | 0 | 2 |

### What the numbers do and do not show

- Lists A and B below are hard results: 14 games that SCARE cannot load or
  cannot run to the end of a route.
- List C is weaker. It says that a route which wins in Scarier, and in most
  cases on the original Runner, does not win in SCARE. It does not prove the
  game is unwinnable in SCARE. Some of these can be finished with different
  phrasing, a different order, or luck with the random numbers. SCARE-specific
  routes were tried for the unassisted non-battle rows only; twelve list C
  games finish that way (see "Routes rewritten for SCARE" below).
- The random numbers were not matched. Scarier ran each row with its
  Runner-compatible generator and the row's own seed; SCARE ran with its own
  generator, which draws at different points, so no seed gives it the same
  stream. The five seeds per row are the only allowance made. To see how much
  this matters, every route was rerun in Scarier under five other seeds. Of
  the 163 games in list C, 116 have a route that still wins under all five, so
  chance does not explain SCARE's failure there. For the other 47 the route
  depends on the random stream to some degree (33 rows win under none of the
  five), and SCARE's failure on those may be bad luck and not a defect.
- Giving SCARE the same generator does not close that gap. A SCARE 1.3.10
  build patched to draw from Scarier's xoshiro stream, with the row's seed and
  the same range mapping, was run over all 221 failing rows. One more route
  reached its marker (*Snakes and Ladders*). On the 59 seed-dependent rows the
  two interpreters never made the same number of draws, so SCARE falls out of
  step with the stream the route was written for within a few turns.
- Porting the Runner's draw points into that build closes part of the gap. The
  port copies from Scarier the load-time draws (temp file name, stamina, event
  starts, embedded resources), the exclusive-range event rolls, the 4.0
  room-group walk and the smaller sites; it cannot copy the draws made inside
  a fight, because SCARE has no battle system. With it, 14 of the 221 failing
  rows reach their marker, each one a game that does not otherwise finish in
  SCARE: ADRIFT-O-RAMA, Albert is Lost!, Motion, Rock Band, Scandal on the
  Seven Seas, Snakes and Ladders, Target, The Labyrinth, The Puzzle Box, The
  Village of Love and Lust, Through Time, Where Are My Keys?, Wumpus Run and
  XClue. (*Through Time* stops aborting because the ported walk code tolerates
  an empty room group.) Thirteen of the 59 seed-dependent rows flip; the 46
  that do not all make a different number of draws from Scarier, 16 of them
  in battle games. For those 46, SCARE's failure may still be the stream and
  not the game logic. The tables here are for stock SCARE.
- The routes were written against Scarier, so the test cannot find a game that
  works in SCARE and fails in Scarier. Zero regressions is a property of the
  method, not a finding.
- A marker is not always a full win. For a game its own data makes unwinnable,
  the unpatched row's marker is the furthest point reachable, and SCARE reaches
  that too in a few such games. Their patched rows, the ones that actually
  finish, are Scarier-only by construction.
- The failure rate follows what SCARE lacks. It is 70% for games with the
  battle flag set against 21% for games without it, and 36% for 3.8 and 3.9
  games against 25% for 4.0 games.

### What stops the non-battle games

The 77 list C games that have the battle flag off, win in Scarier under every
seed tried and still fail in the SCARE build with the Runner's draw points
were compared reply by reply with Scarier. The first reply where SCARE goes
wrong was read by hand, so the counts are approximate and name a symptom, not
always a root cause.

- **The game is broken as shipped (19).** The route only finishes with one of
  Scarier's load-time patches or assists. No interpreter that follows the game
  file can win these, the original Runner included.
- **A task's command never matches (about 20).** SCARE answers "I don't
  understand" or lets a built-in verb take the line. Examples: `north` in
  *ImagiDroids*, whose pattern `[n]{orth}` builds one word from adjacent
  groups where SCARE insists on a space; `restart` in *Gorxungula's Curse*,
  which is a task but SCARE's own meta command wins; `get up` in *Greek School
  Adventure* ("Take what?"); `ask ceo about spam` in *SPAM*; `1.)` in *Pieces
  of Eden*.
- **The library refuses a line the game has a task for (about 14).** The
  Runner offers the typed line to the game's tasks before its own refusal;
  SCARE refuses first. `put crate on box` in *Oh, Human* ("You are not holding
  the crate."), `put doll in fire` in *Fair Scare Nightmare* (no implicit
  take), `put hand on green plate` in *The Dead Man* (a failing task's message
  hides the library put the game is waiting for). A restriction read wrongly
  has the same effect: *Topaz* cannot wear its ring because SCARE takes
  "inside container 0" to mean "inside nothing". Weight and capacity sums
  differ too (*The Tempest*: "too heavy").
- **An object name does not resolve (about 9).** "Take what?" for `take nail
  file` in *The Haunted House* and `get credit cards` in *Love For Real*, or a
  question SCARE asks and the Runner does not: `x wood` in *Shadrick's
  Travels*, "The briefcase or the briefcase?" in *Briefcase*.
- **Characters and events are somewhere else (about 13).** The route arrives
  and the vendor, the cousin or Maria is not there (*Menagerie!*, *House of
  the Damned*, *Blue Sky*). Walks and event timers run a turn or more out of
  step with the Runner even with the random stream matched. This is the
  weakest group: most of these routes can be repaired with a few waits, and
  several games first put here turned out to stop for another reason. The
  next section has the causes.
- **The game ends another way (4).** The same commands lead to a death or the
  losing ending (*Blast*, *Hunting Ground*).

### Why the timing differs, and routes rewritten for SCARE

"Out of step" does not mean unwinnable, so the timing group was taken further:
the causes were traced in both interpreters, and the routes were edited until
they won in stock SCARE or a hard block was found. A script inserted or removed
waits at each point where SCARE's reply left Scarier's, for all 72 unassisted
rows of the non-battle group, within a time limit per row; the rest was done
by hand. The edited routes are
in `test/adrift4/notes/scare_routes/`.

Four rules account for the timing:

- **Tick order.** Each turn SCARE ticks events and then characters. The Runner
  and Scarier tick characters first. A SCARE build with only that order
  swapped wins *Blue Sky*, *Showtime at the Gallows* and *Duchess of Desire*
  with the unedited routes.
- **A tick at game start.** SCARE ticks events and characters once before the
  first prompt in every version. Scarier, following the Runner, does so only
  from 3.90, so in a 3.8 game everything in SCARE runs one turn early (*House
  of the Damned*: the cousin and the wolves).
- **Event lengths and restarts.** A repeating event comes round with another
  phase (the zombies in *House of Death*), and an event with a random length
  runs a different number of turns (the train in *The Fugitive*).
- **Walks.** A character on a round is at another stop on a given turn (the
  vendors in *Menagerie!*, the dog in *Fire in the Blood*).

Twelve list C games win in stock SCARE with an edited route, each in nine runs
out of nine:

| Game | Edit |
|---|---|
| Blue Sky | six waits before `give burger to maria` |
| Showtime at the Gallows | three waits before the gate |
| Duchess of Desire | one wait at the end |
| The Fugitive | two waits before the train |
| The Long Journey Home | one wait |
| The Vampire With A Conscience | one wait before `open container` |
| Jason Evans 1 | two waits before `break statue` |
| House of Death | fewer `look` turns in three places |
| Fire in the Blood | one wait; `open package`, `take wad of money` before giving it |
| Troll! | `put crumbs in basin`; take the breadcrumbs out of the basin by hand |
| Cave of Wonders | `remove poppy` before giving it |
| Cloak of Darkness | `remove cloak` before `hang cloak` |

*Menagerie!* is repaired the same way (twelve waits) but is not in list C,
since its unedited route already wins in stock SCARE under some seeds. The
last four rows are not timing at all. SCARE's "holding" test does not count
what the player wears or what lies inside a held container, so the game's own
task fails and the library's refusal is printed.

Games where the route was followed to a block that no rewording got past:

- *House of the Damned* reaches 84 of 84 points with a reordered route, but
  the ending never fires. In a 3.8 file SCARE's loader stores a walk's
  "meet object" one too low, so the character waits for the hidden cabinet key
  and not for the whisky bottle. Unwinnable in SCARE.
- *The Stainless Steel Rat* always dies in the armoured car. The gas event has
  a pausing task (`insert filters`), already done when the event starts. SCARE
  only pauses an event that is running, and this one starts and ends in one
  tick.
- *Glum Fiddle* reaches THE END with 90 of 100. After `take slime in teapot`
  fails its restrictions SCARE stops; Scarier goes on to a later task that
  matches the same line.
- *The Night That Dripped Blood* cannot start: `in` is taken as a direction
  before the shower task sees it.
- The other rows stop at a command SCARE does not match or an object it does
  not resolve, as counted above. The wait-inserting script changes nothing
  there, and they were not rewritten by hand.

Flags in list C: **B** the game turns the battle system on, **P** the game has
a Scarier load-time patch, **A** the route needs one of the assists. The battle
flag could not be read for every file, so B is an undercount.

### A. Refused at load (3)

| Game | File | Version | SCARE says |
|---|---|---|---|
| Alice's Restaurant Anti-Massacree Adventure | `arlo.taf` | 3.7 | not recognised as an Adrift game |
| Castle Quest | `castle.taf` | 3.7 | not recognised as an Adrift game |
| Sophie's Adventure | `sophie.taf` | 4.0 | "error parsing game data" |

### B. Aborts part-way through the route (11)

| Game | File | Version | SCARE says |
|---|---|---|---|
| Blood Relatives | `Blood_Relatives.taf` | 4.0 | assertion in `gs_object_openness` (index out of range) |
| Fun Town | `fun town.taf` | 4.0 | task_run_move_npc_action: unknown NPC move type, -1 |
| Go! | `Go.taf` | 3.9 | assertion in `gs_object_make_hidden` (index out of range) |
| HYPER Battle System | `hyper_b_s.taf` | 4.0 | task_run_move_npc_action: unknown NPC move type, -1 |
| Mortality | `mortality.taf` | 4.0 | assertion in `gs_object_make_hidden` (index out of range) |
| Rocky Raccoon | `Rocky_Raccon_Game.taf` | 4.0 | assertion in `gs_object_player_get` (index out of range) |
| Scene Of The Crime | `Scene of the Crime.taf` | 4.0 | task_run_move_npc_action: unknown player move type, -1 |
| The Nem Rehsif | `nem.taf` | 4.0 | assertion in `gs_npc_location` (index out of range) |
| The Timmy Reid Adventure | `tra.taf` | 3.8 | assertion in `gs_npc_location` (index out of range) |
| The X-Files: A New Beginning | `The_X-Files_A_New_Beginning.taf` | 4.0 | task_run_move_npc_action: unknown NPC move type, -1 |
| Through Time | `Through time.taf` | 4.0 | lib_random_roomgroup_member: no rooms in group 0 |

### C. Route does not reach its ending — ADRIFT 4.0 (92)

3 Minutes to Live; A Day at the Iachini House; A View to a Home; ADRIFT-O-RAMA [B]; Adventure (Colossal Cave); Albert is Lost!: An Adventure in Real Life; Ba'Roo!; Blast; Briefcase; British Fox and the Celebrity Abductions; Can I do it? [B]; Crazy Old Bag Lady [P]; Cursed; Cyber Warp 1 [B]; Cyber Warp 2 [B]; DeL SoL MADNESS [B,P]; Donuts [B]; El Ascensor; Fair Scare Nightmare; Fire in the Blood; G7056 [B]; Get Treasure for Trabula [B]; Ghost Town; Ghoster [B]; Glum Fiddle; Goldilocks Is a FOX!; Goldilocks: Breaking & Entering [P]; Gorxungula's Curse; Greek School Adventure; Hammurabi; Herr Doktor von Nordlingendinkelsbuhlhundchen-am-Rhein; House; Humbug; Hunting Ground; IL GOLEM; Illegal Socks [B,P]; Invasion of the Second-Hand Shirts [B]; Irvine Quik & The Search for the Fish of Traglea; It's Easter, Peeps; Jason vs. Salm [B]; Lair of the Vampire; Les Feux de l'enfer [B]; Light Up [B]; Main Course [B]; Monster Island; Monsters; Motion; Mystery Manor; Noximion [B]; Oh No Not Another Fantasy Adventure [P]; Pieces of Eden; Professor Von Witt's Fabulous Flying Machine [P]; Provenance [P]; Quest for Flesh [B]; Rock Band: Revenge of Gigantor!; Rodney and the Princess [B]; Scandal on the Seven Seas; Selma's Will; Shadowpeak; Showtime at the Gallows; Snakes and Ladders; SPAM; Storm Tossed; Sun Empire: Quest for the Founders [B,P]; Take One; Target; Terrified [P]; The Adventures of Space Boy! Volume II [B]; The Angel, the Devil and the Human; The Dead Man; The Hunter; The Labyrinth; The Long Barrow; The Magic Show; The Night That Dripped Blood; The PK Girl; The Potter and the Mould; The Prostitute; The Puzzle Box; The Successor; The Town of Azra [B]; The Tunnels of Athylon [B,A]; The Village of Love and Lust; The Woodfish Compendium: ImagiDroids; Three Monkeys, One Cage; Ticket to No Where; Topaz; Twenty-One!; Welcome To Wonderland [B,A]; Wes Garden's Halting Nightmare; Where Are My Keys?; Wumpus Run

### C. Route does not reach its ending — ADRIFT 3.9 (62)

Adventures of Thumper - Wonder Wombat; Akari's Story; Alexis: Dalskee [B]; Battle Zone III: Americans [B]; Battle Zone III: Soviets [B]; Blue Sky; Bomb Threat [B]; Colony [B]; Crossworlds Part 4: Scream For Me; Debt; Doomed World of Aquarius (part 1) [B]; Doomed World of Aquarius (part 2) [B]; Drifting In...; Duchess of Desire; Ebony's World [P]; Gateway: Guardian Child [B]; Grand Journey [B]; Handyman; Hangover [P]; House of Death; House Of Horror [P]; Howard. Oblivion. Part 1: Gold Dragon [B]; Howard. Oblivion. Part 2: Book of Existence [B]; Inverness Castle [B]; Jason Evans 1; Jason Evans 3 [B]; Lair of the Cybercrow [B]; Love For Real; Matt's House [B]; Melbourne Beach [B,P]; Merry Murders [P,A]; Mesto prestupleniya 2 [B,P]; Mystery House [P]; Outside [B,P]; Space Run [B,A]; The Alchemist; The Annihilation of Think.com 3 [P]; The Annihilation of Think.com [B]; The Cave of Morpheus - Part 1 [B]; The Fox [B]; The Fugitive [P]; The Long Journey Home [P,A]; The Night That The Moon Shone Grey [B,P]; The Passages [B]; The Quest (by BoyBiz) [B]; The Quest (by Chlestron) [B]; The Quest (by Chris Moody); The Quest For More Hair [B,P]; The Screen Savers on Planet X [B]; The Search for Mr Smith [B]; The Secret of the Lost World [B]; The Spirit's Flight [B,P]; The Stainless Steel Rat; The Town of Azra [B]; The Vampire With A Conscience [P,A]; The Will; Toxically Earth [B]; Troll!; Villains and Kings [B,P]; Where Is Richard?; XClue; Zanoza Bill - Zolotaya likhoradka - Bilet v Vankuver

### C. Route does not reach its ending — ADRIFT 3.8 (9)

Cave of Wonders; Cloak of Darkness; House of the Damned; Marooned (by Bruce Davis); Super Liam; The Great Escape; The Haunted House; The Twilight [P]; Wrecked
