# Scarier vs. FrankenDrift: the ADRIFT 5 side

This compares the ADRIFT 5 half of Scarier (`adrift5/`, with the Glk seam in
`glk/os_glk_a5*.cpp`) with FrankenDrift, Adrian Welcker's cross-platform port
of Campbell Wild's ADRIFT 5 Runner. The ADRIFT 3.7–4.0 engine has no
FrankenDrift counterpart (FrankenDrift refuses those files) and is covered by
[SCARIER_vs_SCARE.md](SCARIER_vs_SCARE.md).

The relationship is not the one between Scarier and SCARE. Scarier's ADRIFT 5
engine is not a fork of FrankenDrift. It was written from a reading of the
Runner source that FrankenDrift ships (`FrankenDrift.Adrift`, BSD) and uses
none of its code. FrankenDrift is also the reference the engine was checked
against, so most of what follows measures how closely Scarier reproduces it,
and a difference is a Scarier bug unless listed under
[Where Scarier deliberately differs](#where-scarier-deliberately-differs).

"FrankenDrift" here means upstream `awlck/frankendrift` at `bfd29fc` (the
0.9.0 development line, July 2026), built as a headless console front end on
.NET 9 with the local patches listed under
[How this was measured](#how-this-was-measured). Figures are from 2026-10-12.

## In short

| | FrankenDrift | Scarier (ADRIFT 5 side) |
|---|---|---|
| Role | Open-source port of the official Runner | Independent C++ reimplementation, checked against FrankenDrift |
| Versions played | ADRIFT 5 only (3.9 and 4.0 files are refused) | ADRIFT 5, in a binary that also plays 3.7–4.0 |
| Language | VB.NET engine, C# front ends, .NET 8 | C-like C++11 |
| Size | about 25,400 lines of engine, plus about 4,400 of front ends and glue | about 31,400 lines in `adrift5/`, plus 2,700 of Glk seam |
| Front ends | Eto.Forms GUI for Windows, macOS and Linux; Glk runners for Gargoyle and WinGlk | Any Glk library; Spatterlight is the main host |
| Corpus rows that match FrankenDrift byte for byte | (reference) | 186 of 205, 199 of 205 counting RNG-stream noise |
| Time for 30 corpus walkthroughs (9,507 commands) | 295 s | 13 s |
| Undo | One turn, as in the Runner | Multi-level |
| Maintained | Yes (315 commits, last 2026-07-30) | Yes |
| Licence | BSD (engine), MIT (front ends) | GPL version 2 only |

The match figures come from the replay described under
[Conformance against FrankenDrift](#conformance-against-frankendrift). Read the
caveats there before quoting them.

## Where Scarier is better

### It is one interpreter for every ADRIFT game

- FrankenDrift stops at "ADRIFT v3.9 and v4 adventures are not supported in
  FrankenDrift". Scarier opens the file, finds out which engine it needs and
  runs it, so one Glk interpreter plays 3.7, 3.8, 3.9, 4.0 and 5.0 games.
- Both engines share the Glk seam: the map pane, status line, side windows,
  pictures and sounds, autosave, the `glk` meta-commands (`glk script`,
  `glk inputlog`, `glk summary` and the rest), and real-time timer events.

### It works as a Glk interpreter and not as an application

- FrankenDrift needs a .NET runtime and, on Linux, GTK 3. Scarier links the
  system zlib and nothing else, and a game loads in milliseconds.
- Spatterlight autosaves at the prompt, at a keypress, at the player-name
  question and at a pending "Which one?". The pop-up questions the Runner asks
  in dialog boxes (the player's name, "Are you male or female?") are asked in
  the story window.
- FrankenDrift's README lists in-line graphics, sound and an experimental map
  as known limitations. Scarier shows pictures and plays sounds, including
  media a game names but did not pack, and draws a clickable map pane
  ported from the Runner's `Map.vb`, with In/Out badges and level changes.
- `glk hints` presents the structured `<Hint>` rows some games author.
  FrankenDrift parses `htblHints` and never reads it again.

### It is faster

- The same 30 corpus walkthroughs take 13.0 s in Scarier and 295 s in
  FrankenDrift, a factor of 22. Both figures are wall-clock time for a whole
  process, so FrankenDrift's includes .NET start-up, which is most of a short
  run: the 39-command *Six Silver Bullets* route takes 0.03 s against 0.96 s.
  A 250-command route through *Anno 1700* takes 0.13 s against 5.2 s. The
  figures are one run each on one machine, over the first 30 corpus rows that
  have both a game file and a script.
- The full ADRIFT 5 replay of 205 rows, each compared with FrankenDrift in two
  RNG modes and put through a save/restore self-check, took 26 s here, but
  only because FrankenDrift's transcripts are cached by content hash. Uncached,
  the FrankenDrift side dominates.
- The engine was profiled in July 2026: a first-word prefilter in front of
  `std::regex`, per-turn undo snapshots built without `snprintf`, and a
  char-pointer-keyed hash table for model keys made the stress runs 2.8 to 4.3
  times faster (*Six Silver Bullets*, 49 s to about 11.5 s).

### It avoids bugs that the Runner and FrankenDrift have

- **UNDO after the game ends.** adrift.co bug 19196 (Critical, open since
  2018): after an UNDO at the post-game prompt the Runner starts stopped events
  and stops running ones, because `clsEvent.NextCommand` is not part of
  `clsGameState`. Scarier's undo point is the whole runtime. A probe row,
  `ProbeUndoAfterEnd`, pins this: it must keep showing exactly four differences
  from FrankenDrift, and a drop to zero would mean Scarier had acquired the bug.
- **Undo depth.** The Runner remembers one turn. Scarier keeps the last 16, and
  repeated UNDO walks back turn by turn.
- **Pre-5.0.22 games.** Files with no `<TaskExecution>` element predate the
  5.0.22 setting and ran with "highest priority *passing* task" in Campbell's
  Runner. FrankenDrift hardcodes "highest priority task" for them, so the
  central puzzle of *Return to Camelot* (v5.000020) and part II of *Death
  Shack* cannot be won. Scarier gates the default on the file version.
- **FrankenDrift issues that do not occur.** Checked July 2026:
  - #43, "matrix is degenerate" when drawing the map of *Stone of Wisdom*:
    FrankenDrift fits NURBS curves with GShark. Scarier ports the Runner's
    cubic-Bézier connector drawing, which has no matrix to invert.
  - #31, an empty `<>` tag swallowing output: FrankenDrift's HTML parser
    corrupted its style stack. Scarier drops unknown tags in one pass with no
    stack, and player input embedded in a message is never rescanned as markup,
    so a typed `<cls>` or `<waitkey>` does nothing.
  - #29, text missing from the transcript (*The Dragon Diamond*, hints build):
    FrankenDrift's transcript writer sits in its display layer. Scarier's
    transcript is a Glk echo stream on the main window, so screen and
    transcript cannot differ.
- **`<del>` across messages.** The two FrankenDrift front ends disagree on how
  `<del>` works (the Glk one deletes from the pending fragment, the Eto one from
  the whole window). Scarier follows what the genuine `run500.exe` 5.0.36 does
  under Wine: one character from the whole accumulated turn buffer, including
  the previous message's last character. All eleven cases in the `delrt` probe
  were measured in the Runner.

### It is held in place by a larger test net

- 205 walkthrough rows, 181 committed goldens (strict-diffed with no .NET
  needed), and 23 synthetic probe rows over 19 hand-written games, most from
  the adrift-5-rs conformance suite and the eight `Samples/*.taf` regression
  scripts from the ADRIFT 5 distribution. Every row also passes a mid-script
  save/restore round trip that must be byte-identical.
- Unit harnesses for the parser, expression evaluator, restrictions, saves,
  walks, objects and disambiguation, with ASan/UBSan builds, and a map raster
  regression (six views hashed against self-blessed PPM goldens).
- Upstream FrankenDrift has no engine test project that I could find in the
  tree, only a console harness.

## Where FrankenDrift is better

### It is the reference

- FrankenDrift.Adrift is Campbell Wild's code, translated and kept in step with
  his releases, and Scarier is a reading of it. A behaviour that no
  walkthrough in the corpus exercises is an unverified port. Anything not
  covered by the corpus (roughly the games that have never been replayed,
  and every code path a replay does not reach) should be treated as "probably
  the same", not "the same".
- Several functions exist in the engine only in FrankenDrift's `Global.vb`
  (`ReplaceFunctions`, the 44 percent-function names, `PreviousFunction`),
  not in the official source tree. Scarier matches them by reading, and a
  function added there tomorrow is not in Scarier until somebody notices.
- When a new official Runner or a FrankenDrift release changes behaviour,
  FrankenDrift takes it by merging. Scarier takes it by hand.
- "Matches FrankenDrift" is not "matches the Runner". FrankenDrift has
  its own bugs (above) and a few known gaps in its front ends, and only
  `<del>` and a handful of probes were measured in `run500.exe` itself.

### Features Scarier lacks

- A stand-alone GUI: FrankenDrift's Eto front end runs on Windows, macOS and
  Linux, and its Glk runners run under Gargoyle and WinGlk, none of which
  needs Spatterlight. Scarier needs a Glk library and,
  for the map pane and the media, a host that provides them.
- `<window>` side windows, such as the status panel in *Alien Diver*, and the
  map window, are real, separate system windows that can be moved around and
  resized freely.
- Text styling is more complete. Bold, italic, underline, centre, right, colour
  (`<c>`, `<font colour=…>`), images, audio and named windows are carried to
  the host. A `<font>` tag's face and size are read for nothing but its colour.

### Portability and runtime

- FrankenDrift ships stand-alone downloads for Windows, macOS, Linux x64 and
  Linux arm64. Scarier is a C++ source tree that builds as part of
  Spatterlight, or with `make glkscarier` against CheapGlk or another Glk
  library, and with Visual C++ for WinGlk.
- Scarier is GPL version 2 only (it shares a binary with SCARE-derived code).
  FrankenDrift's licences are more permissive, which matters to anybody who
  wants to embed the ADRIFT 5 engine in something that is not GPL.

### Provenance

- The Scarier README calls the engine "a lazy AI C++ clone". Most of its
  commits were written with AI assistance and verified by diffing, not by
  reading. FrankenDrift is hand-translated VB.NET that can be compared line for
  line with Campbell's own source.
- The evidence behind Scarier is hard to reproduce elsewhere. It needs
  FrankenDrift with a local headless front end and an `xoshiro` RNG hook (see
  below), about 175 third-party game files that cannot be committed (the
  manifest pins them by sha256, and a few exist only inside a standalone
  `.exe`), and the walkthroughs that were derived by hand or agent.
- The goldens say that Scarier still matches a past run of itself. They were
  checked against FrankenDrift when blessed, but a golden does not notice if
  FrankenDrift later changes.

## Where Scarier deliberately differs

Only three, and anything else that differs is a bug.

1. **`<TaskExecution>` version gate** (`adrift5/a5model.cpp`). See above.
   FrankenDrift is unwinnable on *Return to Camelot* and *Death Shack* unless
   it is patched the same way.
2. **UNDO after game end** (`ProbeUndoAfterEnd`, 0|4 and must stay 4).
3. **OS (`PlugIn.Exe`), 0|1.** This one is Scarier's gap, left open on
   purpose. A `Specific` override on a task's `AfterTextAndActions` sets
   `Aipoints = 0`, and the Runner expands an aggregate message at Display time,
   after those actions, so it prints "Tester wins!". Scarier renders the static
   skeleton at emit time, still sees 22, and prints "AiMReele99 wins.". Closing
   it means deferring whole aggregate messages, and that risks the
   spacing semantics for the whole corpus.

Two conveniences that are not in the Runner are not counted as deviations,
because they are invisible to a transcript: multi-level UNDO, and `glk hints`.

## Conformance against FrankenDrift

### How this was measured

FrankenDrift has no console interface, so a front end was written:
`FrankenDrift.Headless` (about 200 lines, preserved in
`test/adrift5/harness/frankendrift-headless/` and in the fork's
`scarier-headless` branch). It implements the small `UIGlue`, `frmRunner`,
`RichTextBox` and `Map` interfaces and prints `UIGlue.OutputHTML` as plain
text, stripped the way Scarier's Glk window strips it. The sole text sink is
the same one a player would read. Local changes to FrankenDrift:

- The headless front end itself, with `<cls>` and `<del>` handling.
- A fixed RNG seed through reflection on the private `SharedModule.r`, and
  an optional `FD_RNG=xoshiro` that replaces `System.Random` with a generator
  byte-identical to Scarier's xoshiro128**.
- The `<TaskExecution>` version gate in `FileIO.vb` (about line 1248). A clean
  upstream checkout fails *Return to Camelot* (13 hunks) and *Death Shack* (6).

Every row of the MAP at the foot of
`test/adrift5/harness/run_a5_walkthroughs.sh` pairs a game file with a command
script and a budget of differing hunks in two modes:

- **vanilla**: FrankenDrift on its stock `System.Random`. Random text cannot
  line up here, so games with dice, combat or random choices carry a residual.
- **xoshiro**: FrankenDrift forced onto Scarier's stream. Any difference left
  is a real logic or draw-count difference, and a row at `N|0` is
  byte-identical.

A row that differs only in the vanilla column is noise. The xoshiro column is
the verdict. Both columns also assert that a save/restore taken mid-script and
continued gives a transcript identical to an uninterrupted one.

| | Rows |
|---|---|
| In the MAP | 205 |
| Byte-identical to FrankenDrift in both modes (MATCH) | 186 |
| Differ at their recorded budget (DIVERGE) | 17 |
| No script in this checkout (adult games kept local-only) | 2 |
| Over budget (FAIL) | 0 |
| Zero hunks under the xoshiro stream | 199 |

The 205 rows are 176 game files (111 Blorb, 65 bare `.taf`, including alternate
builds and releases of some games, Danish ones among them). The Samples and
probe rows use game files of their own.

### The 17 rows that differ

| Row | vanilla\|xoshiro | Why |
|---|---|---|
| Stone of Wisdom | 2\|0 | RNG noise |
| Jacaranda Jim | 99\|0 | RNG noise, the most random game in the corpus |
| Jacaranda Jim (2011) | 37\|0 | same |
| Six Silver Bullets | 18\|0 | RNG noise (`Roller`, the time trap) |
| Six Silver Bullets, truth ending | 111\|0 | same |
| Lost Labyrinth of Lazaitch | 8\|0 | RNG noise |
| October 31st | 106\|0 | werewolf random walk |
| October 31st (comp release) | 106\|0 | same |
| Oktober 31 (Danish) | 32\|0 | same |
| I Summon Thee | 5\|0 | RNG noise |
| ProbeTaskActions | 1\|0 | RNG noise |
| Alien Diver v1.3 | 0\|26 | an alternate build; 14 times an extra "Sorry, I'm not sure which object you are trying to #." and 12 times `"Playable Card"` with quotes, from a `SetProperty` of a quoted string onto a property the v1.3 object does not carry yet, which Scarier stores raw as the Runner's add-branch does |
| Tempus Fugit | 0\|1 | one stray blank line near line 538; cosmetic, undiagnosed |
| OS | 0\|1 | the aggregate-message gap above |
| ProbePopups | 4\|4 | an unevaluated `%PopUpChoice[…]%` token: FrankenDrift's tokenizer re-serialises it without the spaces after commas, Scarier keeps the source spacing; cosmetic |
| ProbeUndoAfterEnd | 0\|4 | FrankenDrift's bug, not Scarier's |
| ProbeVariables | 1\|1 | `%Version%` only |

For *Beginner's Cave* (about 136 hunks, since every turn of substance is a
Tunnels & Trolls combat round) and *Alien Diver* (vanilla FrankenDrift drops
the crashed ship in a different room), the vanilla column is a Scarier
self-golden and shows 0. Those two are only checked against FrankenDrift
under the xoshiro stream.

### What the numbers do and do not show

- A match is evidence for a route, not for a game. Each row replays one
  walkthrough (most in the 50–250 command range, a few over 2,000, like
  *Symphonica 64* with 2,238), so it covers the verbs and branches the route
  uses. Rows that stop at an authentic dead end in the shipped game (*Penrhyn*,
  *Sorry For Your Loss*, *The Awakeners*, *Tempus Fugit*) are matched up to it,
  and FrankenDrift reproduces each dead end exactly.
- The routes were derived from published walkthroughs and then repaired
  against Scarier. Some were bent to avoid random events. A route that Scarier
  can finish cannot show a game that works in FrankenDrift and fails in
  Scarier. What the replay finds is places where the two differ when both are
  driven the same way. It found a 17-entry ledger of engine bugs, now all
  closed, and 12 divergence groups among the probes.
- The xoshiro hook is an equalizer, not an oracle. It forces FrankenDrift onto
  Scarier's draw sequence, so it can only prove that the two draw the same
  number of times in the same order. How the random text is chosen from a
  draw is what is compared. Earlier work found real draw-count differences, for
  example a dynamic message FrankenDrift evaluates two or three times per
  execution, each evaluation consuming draws. Scarier replays that dance for
  `Look`, and other task messages have not been audited.
- Two more things the net does not cover: the four adult games that are
  deliberately not wired, and the three probe sources that came without a
  command script (`rich_text`, `text_set_variable`, `timed_events`).
- Real-time events fire from a Glk timer at the prompt in the app, like the
  Runner's timer, but the headless replay ticks them once per turn, so the
  wall-clock path is not part of the comparison.

### Further reading

- `test/adrift5/notes/A5_WALKTHROUGH_FINDINGS.md`: the status table, per-game
  write-ups, the probe diagnoses and the `<del>` measurements.
- The comment block above the MAP in `run_a5_walkthroughs.sh`: about 1,400
  lines of dated derivation history.
- `ADRIFT4_vs_ADRIFT5.md`: how the two engines inside Scarier differ.
- `ADRIFT-5-XML.md`: a specification of the ADRIFT 5 adventure and save
  formats.
