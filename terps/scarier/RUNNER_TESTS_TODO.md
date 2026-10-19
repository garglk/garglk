# ADRIFT Runner fidelity: arbitration record

Every question Scarier has taken to the **real** ADRIFT Runners, and how it was
answered. Companion to `ADRIFT4_vs_ADRIFT5.md` (which records semantics already
settled) and `test/adrift4/notes/WALKTHROUGH_TODO.md` (which is about route
derivation, not engine fidelity).

**Every section is closed** — §§1–8 between 2026-08-01 and 2026-08-09; §9, the
backlog of code-comment TODOs, on 2026-08-17; and §10, the event-length roll
when `Time1 ≠ Time2`, raised, measured in both Runners and ported all on
2026-08-17. The `## Closure log` section at the foot is the record of *how*
each closed, not a plan. What is still live is the method — how to stage a probe against each
Runner (§ *Running the Runners*, §5's `where` probe, §6's 3.70 codec, §7, §8) —
and the **§4 divergence table**, which is the standing list of every place
Scarier and the Runner are known to differ, with the reason each one is kept,
ported or ignored. Read §4 before changing engine behaviour; add a row to it when
a new divergence is settled.

The premise changed in 2026-08: **both** Runners now execute on this machine, so
almost everything below was answerable by *playing* rather than by reading
P-code. Prefer a live run; fall back on the disassembly only when the question
can't be staged.

## Running the Runners

- Harness: `~/adrift-battle/runner/wine/` (README + `winlist.swift`,
  `winpos.swift`, `click.swift`, `cmd.sh`). x86_64 Wine under Rosetta.
- `run400.exe` = 4.0. `run390.exe` = 3.9, extracted from
  `~/Downloads/ADRIFT39/run390.CAB`; it runs in the same prefix and needs no
  `regsvr32`. `run400` **refuses** a 3.9 file ("Incorrect version"), so identify
  the file first — `sctaffil.cpp:53-65`, bytes 8 and 10 discriminate.
- `gen400.exe` sits beside run400. Its UTF-16 UI strings spell out the Generator
  dropdown enums *in order* — the fastest way to decode any `Var` mapping — and
  it can also **perform** a 3.9 → 4.0 conversion (see §3).
- ⭐ **The generators upconvert, and that is how you get a same-game
  cross-version cell.** Each Runner loads only its own `.taf` version, so
  "run400 vs run380 on the same game" looks impossible — but `gen400.exe`
  opens a **3.80** file directly (status bar: `File version 3.80`) and `File →
  Save As` writes 4.00, and `gen390.exe` does the same to 3.90. That turns one
  game into three files whose data is identical, which is the difference
  between comparing engines and comparing games. Downward does not work:
  `gen370.exe` handed a 3.80 file just opens Untitled. Recipe —
  `wine 'C:\adrift\gen400.exe' 'C:\adrift\game.taf'` (the command-line argument
  *is* honoured, but a modal "Tip of the Day" hides the loaded game until you
  dismiss it), `File → Save As…`, type a **bare** filename: the dialog rejects
  `/` outright ("Invalid character(s) in path"), and osascript `keystroke`
  turns typed backslashes into forward slashes. **Always verify the field you
  are about to measure survived**: diff the deobfuscated plaintexts, because
  gen390's 3.8 → 3.9 pass demonstrably rewrites `SizeWeight` (§3). Some things
  do not survive at all — the 3.90 copy of `microwaveman.taf` lost the noun
  aliases, so `take clothes` became `Take what?` and the route needs the full
  `take aluminum clothes`.
- ⭐ **For 3.7/3.8, editing the game is cheaper than converting it.** Those
  files are a bare PRNG XOR (see below), so to measure how a Runner renders
  some field you can just rewrite that field in a real corpus game and
  re-encode — the plaintext may change length, since the stream is indexed by
  absolute offset and you re-run it over the whole file. That is how the 3.70
  cell of the `some`-prefix row was taken: `arlo.taf`'s bone object had its
  `a` prefix rewritten to `some`, and run370 answered `You pick up some bone.`
- `run380.exe` = 3.8 and `run370.exe` = 3.7, both from delron.org.uk's
  `adrift38.zip` / `adrift37.zip` via the Wayback Machine, both installed in the
  same prefix with their Generators beside them. `run370` additionally needs
  `COMCTL32.OCX` in `syswow64` (extract just that member from the cached
  `VB60SP6-KB2708437-x86-ENU.msi` and `regsvr32` it; winetricks' `comctl32ocx`
  hangs on a 7z overwrite prompt). Their `.taf` files are plain CRLF text XOR'd
  with the VB6 PRNG from seed `0x00a09e86` — no signature, no length header, no
  trailer — so a probe is just "decode, rewrite a line, re-encode", and the
  plaintext may change length freely. See §6.
- P-code: `~/Desktop/run400.txt`, `~/Desktop/run390.txt` (`grep -a`; both contain
  stray binary).
- ⚠️ **Tick Options → Display & Media… → Appearance → "Room names in
  descriptions" before measuring anything that involves a room block.** It is
  **off by default and is not saved** — a fresh Runner start always comes up
  with it off — and while it is off the Runner prints **no room-name heading
  anywhere**: not after movement, not at game start, not on `look`, not on a
  task's `ShowRoomDesc`. That is a Runner *preference*, not engine behaviour;
  Scarier's equivalent (`game->bold_room_names`, default TRUE) is the
  ticked state. Every live measurement taken in this prefix before 2026-08-15
  was taken with the box unticked. The dialog has no keyboard path that works
  under Wine — open the menu with `swift click.swift 95 72`, then
  `swift click.swift 127 240` for "Display & Media…", tick at `176 239` and OK
  at `110 300` (coordinates for the default window position).
- Probe games: hand-author a one-restriction / one-task variant, repack with
  `test/adrift4/harness/taftool.py` or the Runner rejects it. Recipe and
  field offsets in the memory `scare-restriction-statics-run400`.
- ⭐ **Read the Runner through its own transcript, not through screenshots.**
  Every Runner writes one; the menu differs by version, and Wine never delivers
  Alt from macOS, so the menu title has to be *clicked* — it sits +25/+35 from
  the window's top-left corner — and the item picked by its accelerator letter.
  - **run390 / run400**: `Adventure → Start &Transcript` opens a "Select
    Transcript File" dialog pre-filled with `C:\adrift\Adrift_<N>.txt`;
    `key code 36` accepts it, and everything from then on is appended live.
    Scripted by `~/adrift-battle/runner/wine/runner_transcript.sh <game.taf>
    <cmdfile> [runner.exe]`.
  - **run370 / run380**: no live transcript — `Adventure → Save &Transcript`
    dumps the whole scrollback to `C:\adrift\Adven_<N>.rtf` *at the moment you
    click it* and pops a MsgBox to dismiss, so play first and save last
    (`run380 Form1.frm`, `transcript_Click` at `42A378`). Scripted by
    `runner_savetranscript.sh`; read the result with
    `textutil -convert txt -stdout Adven_1_marooned.rtf`.
  - Answer any load-time modal (name InputBox, "Please choose the gender of the
    player") **before** clicking the menu, or the click lands on a blocked
    window. `drive_ckpt.sh` aborts the run if an unexpected window appears
    mid-script, which is how those get caught.
- Screenshots remain the fallback: `screencapture -x -o -l<winID>`; classify
  cheaply by ink pixel count in the response band rather than OCR.
- **Input is unreliable.** The first keystroke of a scripted command is routinely
  lost, the MORE bar eats a key, and Auto complete rewrites the box *before* the
  echo. Always read the echo before believing a "the Runner doesn't support X"
  result. For turn-timed or RNG games, don't script a replay at all — write a
  `.tas` from Scarier and transplant it.

---

## 1. Battle System — now partially verified against the running Runner

The whole port was reverse-engineered from `Battles.bas` (DotFix decompile) plus
the v4 manual, and validated against a *synthetic* game
(`test/adrift4/harness/battle_test.taf`) built so every attribute has `Lo == Hi` and the rolls
collapse. **2026-08-01: the core formulas have now been diffed live against
run400** with authored arena probes (recipe below) — hit test, roll bounds,
damage floor, worn armour and the upgraded-3.9 stalemate all match the port.
The cadence, recovery and death items were measured live later the same day
and are ported too — **every checkbox below is ticked and §1 is CLOSED**.

Highest value first:

- [x] **Upgraded-3.9 games really do stalemate in run400.** *(2026-08-01, live.)*
      SCARE `.tas` written at Northern Trail (sword bought + wielded, no
      assist), transplanted and restored in run400, `n`, `attack bandit` ×11:
      every turn "a bandit manages to avoid your attack with the hunting
      sword." / "You manage to avoid a bandit's attack." Direct proof from the
      Runner's own `status`: after its conversion the player shows
      **Accuracy 0-0 / 0 / 0 and Agility 0-0 / 0 / 0** (Hit strength 1-1,
      current 6 (5) from the sword). `0 > 0` can never pass. Keeping
      `SCR_ASSUME_COMBAT` opt-in is faithful; the two golden rows keep their
      assist flags.
- [x] **Hit test is strictly `effAcc > effAgi`.** *(2026-08-01, live.)* Probe
      `pEQ2` (acc 5-5 vs agi 5-5 both ways, all weapon flags stripped): four
      clean `attack robot` turns, both directions "manages to avoid" every
      time. SCARE on the same file: identical. Beware the contaminated first
      attempt: with a held weapon in the room the Runner **auto-selects it for
      a generic `attack`** (see surface notes below), and its +Accuracy turned
      the equality case into 25 > 5.
- [x] **Attribute roll has an exclusive Hi**: `lo + Int(Rnd*(hi-lo))`.
      *(2026-08-01, live.)* Probe `pXH` (enemy Str 5-6, guaranteed hits, player
      Def 0): 8 hits, stamina 200→160 — every roll 5 (p ≈ 0.4% if Hi were
      inclusive; damage does re-roll per attack — a 4-hit run with Str 5-15
      summed 35, not divisible by 4). Probe `pZ1` (Str 0-1): always rolls 0.
      SCARE identical on both (160 / 200).
- [x] **Damage floor.** *(2026-08-01, live.)* Probe `pZ1` (Str 0-1 → roll 0 vs
      Def 0): every turn "Robot hits Player, but it doesn't seem to do any
      damage.", stamina untouched. SCARE: same message (with "you"), same
      stamina.
- [x] **Worn-armour defence path.** *(2026-08-01, live.)* Probe `pAR` (enemy
      Str 10-10 guaranteed hits, player Def 0-0, vest ProtectionValue 5 worn at
      start): 9 hits (an `i` turn also ticks combat — in both engines),
      stamina 200→155 = exactly 5/hit; the Runner's `status` shows Defense
      "0-0 0 **5 (5)**". SCARE identical (155). The highest-risk formula is
      confirmed.
- [x] **shoot (Method 3) zeroes base strength — 4.0 half confirmed live.**
      *(2026-08-01.)* Probe `pM3` (robot stamina 35, harmless; player Str 10 +
      blaster HitValue 30 Method 3): run400 kills on the **second** attack —
      30/hit, base Str replaced, exactly Scarier's rule; SCARE identical. The
      **3.9 half settled live too**: `test/adrift4/harness/make_39_probe.py` authors and
      packs a real V390 file (obfuscation + the `sPassword` field's own
      `Mid(5,4)=="Wild"` check, which run390 validates — same rule as the 4.0
      trailer), and run390 **one-shots** the 35-stamina robot — damage 40 =
      Str 10 + HitValue 30, added regardless of Method. Scarier's zeroing was
      wrong for 3.9 and is now version-gated on `battle_legacy` (commit
      `7a4cb7c2`); only ALEXIS shifted in the corpus (battle flavor, still
      wins, re-blessed). Bonus run390 observations: battle messages use
      second person ("Robot hits you") where run400 prints the player's
      name, no corpse line prints on NPC death ("Robot isn't here!" next
      turn), and a parse-error turn *does* tick combat in run390.
- [x] **Speed / cadence.** *(2026-08-01, live.)* Speed 2 → hits on turns
      2,4,6,8; Speed 3 → 3,6,9 (first attack on turn N, countdown starts at
      Speed); Speed 1 → irregular 1–2-turn gaps (5 hits/10 turns) consistent
      with `rnd(1..2)`. SCARE identical on 2/3 (byte-same hit turns).
- [x] **Recovery counter.** *(2026-08-01, live.)* Probe `pRC` (Recovery 3, take
      3×5 damage, retreat, status per turn): run400 regains +1 at turns 2, 5, 8
      — the same curve SCARE produces (its statuses step 187/188/189 at
      5/8/11). Phase and period match.
- [x] **Target select.** *(2026-08-01, live — and it found a real Scarier
      bug.)* Probe `pTS` (Aly att 1, Foe att 2, Bystander att 0, all
      co-located): run400's Foe picks the player *or* the ally per turn
      (P,A,P,A,A over five turns); Aly attacks Foe every turn; Bystander
      never acts; nobody targets the neutral. Scarier's Foe picked the SAME
      target every turn of a session (12/12), because `scr_randomint` mapped
      the congruential generator with `% range` and an LCG mod 2^32 has
      period-2 low bits — `(state>>1) % 2` alternates strictly, and a fixed
      even draw cadence pins every pick. **Fixed** (scutils.cpp): multiply-
      shift on the full 31-bit value, which is also what VB6's `Int(Rnd*N)`
      does; `scexpr.cpp`'s EITHER() pick had the same modulo. The fix
      re-sequenced every seeded transcript: v4 corpus re-blessed (see note
      below), four rows re-seeded (snakes 2, jason 11, light_up 2, circus
      2→17, les_feux 138), and the three Shadowpeak routes — battle lengths
      threaded too tightly to survive any new sequence (no seed in 1–800
      works) — initially pinned the old mapping via `SCR_LEGACY_RANDMAP=1`.
      ~~a documented harness-only compatibility hook~~ **Hook RETIRED
      2026-08-02**: the routes were re-derived under the fixed mapping
      (seeds 13/87/657 — clean-upstream sweep + `shadowpeak_chase.py`, same
      scores 700/715/735; see Shadowpeak_walkthrough.md session 25) and the
      hook deleted from `scutils.cpp`/`seed.cpp`.
- [x] **Death path (no KilledTask).** *(2026-08-01, live, via `pM3`.)*
      "Robot falls down, dead." (byte-same in SCARE), corpse leaves scope:
      run400 answers "Robot isn't here!" / "Player cannot see Robot from
      here." where SCARE says "I don't understand." / "Player sees no such
      thing." — semantics match (location `0xFB`), wording differs.
- [x] **StaminaTask/KilledTask — settled live 2026-08-01** (probe `pKT` in
      `make_arena_probe.py`, which now authors tasks; 3.9 half via
      `make_39_ktprobe.py` in run390).  run400: a set KilledTask **replaces**
      the "falls down, dead." line ("Player hit Robot.  KILLEDTASK FIRED.");
      StaminaTask fires on **every** hit that leaves `0 < stamina <
      max/10` — twice in the probe window (hits leaving 8 and 4 of max
      100) — and does NOT fire on the killing blow.  run390 dispatches
      KilledTask identically ("You shoot Robot with the blaster.
      KILLEDTASK FIRED.").  Decompile (Battles.bas Proc_11_0/Proc_11_3)
      pins two boundary details the probes can't: the threshold divide is
      **floating point** (`CDbl(max)/10`), and the corpse's held/worn
      objects are re-homed to the death room *before* the KilledTask runs.
      Three faithfulness fixes ported (scbattle.cpp): re-home before task,
      `stamina * 10 < maximum` (integer-exact float form), and the default
      corpse line version-gated on `!battle_legacy` — **run390 prints
      NOTHING when a task-less NPC dies** (probed live; the string
      " falls down, dead." does not exist in its binary).
      Corpus: secret_of_lost_world (3.9, Ghost death) re-blessed; all
      76 rows PASS.
- [x] **Battle-task dispatch is GATED on player task-eligibility — settled
      live 2026-08-02** (Del Sol + probe `KT2` in `make_arena_probe.py`).
      run400 routes KilledTask/StaminaTask through its general run-task
      routine (Battles.bas Sub_12_4/Sub_12_1 -> mdlSpreadTheLoad.Sub_20_22),
      which silently drops a dispatch the player is not eligible for — BOTH
      halves proven: (a) *room list* — Del Sol's teacher Moreland (stamina 0,
      killable by one player blow; the stamina<=0 skip is NPC target-selection
      only) dies in Chemistry but her chem-dream-only KilledTask `# super win`
      never fires, making the game faithfully UNWINNABLE; (b) *done state* —
      KT2: a done non-repeatable KilledTask re-killed prints ONLY the hit
      line, no re-fire and no corpse line either.  Ported: `battle_kill` /
      `battle_apply_damage` now gate on `task_can_run_task_directional`
      (same gate as the type-5 exec channel).  Corpus: the two Shadowpeak
      combat goldens re-blessed (each loses one out-of-room StaminaTask line,
      Haraxis's StaminaTask = the room-45-only `get salt` task); 127/127
      PASS.  Related: run400 strips ALL leading `#` from typed input
      (Form1.Text1_KeyPress `While Left(input,1)="#"`), so `#` tasks are
      untypeable — SCARE's SPECIAL_PATTERN exclusion is the faithful
      equivalent.
- [x] **Player-facing surface — settled live 2026-08-01** (probe `pWS` in
      `make_arena_probe.py`: sword Method 1 / HitValue 10 / Acc 15 + axe
      Method 0 / HitValue 20 / Acc 5, both held; unseen Ghost NPC in a second
      room).  run400's model is a persistent wield ref, NOT a per-attack
      default:
      * Start: "Player is wielding nothing" and the status Current values are
        BARE — no would-be weapon folded (Str 10, Acc 20 with both weapons
        held).  Once something is wielded, status folds ONLY that weapon,
        with the bonus in parentheses ("30   (20)").
      * Bare `attack X`: uses the wielded weapon if set; else with exactly
        ONE held weapon it auto-selects it AND SETS the wield (status shows
        it afterwards); else with 2+ held weapons it asks "What do you want
        to attack Robot with?" — rhetorical (a bare noun reply is a parse
        error; no combat tick), you must retype `attack X with Y`.  So the
        player-side "best weapon = highest HitValue" silent pick never
        happens in run400 — Proc_11_12's best-by-HitValue is the NPC picker
        (and the single-held trivial case).
      * `attack X with Y` and `wield Y` ("Player wield the sword.") both set
        the wield.  **There is NO `unwield` verb** ("I don't understand." —
        it is a SCARE invention).  `drop` of the wielded weapon clears the
        wield to NOTHING — no fallback to another held weapon (status back
        to bare values).
      * Method verbs: wrong-method wielded → "Player can't cut with the
        axe!" (and combat DOES tick); matching → normal attack; nothing
        wielded/held → a plain bare blow ("Player hit Robot.") — Scarier's
        unarmed-verb interpretation confirmed.
      * `status <unseen npc>` does NOT print the "can't get status of a
        character you've not seen yet!" string — the %character% simply
        fails to match and it falls back to the plain player `status`.
      * Bonus: `attack <typo>` → "Who do you want to attack?" DOES tick
        combat in run400; "I don't understand." parse errors don't.
      **PORTED 2026-08-01** (scbattle.cpp/sclibrar.cpp/scgamest.cpp): the
      wield is now a persistent ref -- every armed player blow persists it
      (as Proc_11_1 does, before the hit test, so a miss persists too); bare
      `attack` auto-selects a solitary carried weapon, fights bare-handed
      with none, and with 2+ asks the rhetorical "What do you want to attack
      X with?" (is_admin, so no combat turn passes and the reply is not read
      as an answer); the wield clears when the weapon leaves the player's
      hands (gs_carried_track chokepoint -- drop/throw/give/put/wear; and
      re-taking does NOT re-wield) with no best-carried fallback; `unwield`
      removed; `wield` says "You wield the sword." / "You are already
      wielding the sword."; status always prints the wielding line
      ("nothing" when unarmed) and folds only the actual wield.
      **Erratum:** the port's original "all 77 goldens passed unchanged"
      validation ran against a STALE harness binary (run_v4_walkthroughs.sh
      only rebuilt `scare` when missing -- now fixed to rebuild on newer
      sources).  Real fallout, found and repaired later the same day: three
      goldens had wield-wording lines, and the Shadowpeak routes broke
      exactly as this row predicted -- they drop the sword in the chapel
      (clearing the wield), re-take it (no re-wield), and their next bare
      `attack` with two carried weapons ASKED instead of silently picking.
      Repaired with a zero-turn-cost edit: the first post-retake attack is
      now explicit (`attack cat with sword`), which persists the wield for
      every later bare attack.  ~~Still diverging (cosmetic): status layout;
      "not holding" wording.~~  **Cosmetics PORTED 2026-08-01** after a
      second probe (`pWS2` in `make_arena_probe.py`: the Robot always hits
      for exactly 5, so live stamina drops below max and the status table's
      Stamina cells become distinguishable):
      * Status is a four-column table — header `Range / Max / Current value
        (inc weapons/armour)` (indented past the label column; run400 pads
        it with an *invisible* `<0>`-colored "Stamina:" chunk and vbTabs),
        labels `Stamina: / Hit strength: / Accuracy: / Defense value: /
        Agility:`, and NO "You have:" lead-in for player or NPC.  The
        Stamina row is **live / max / live** (no lo-hi, no parens — pinned
        by pWS2's damaged 195/200/195 and the Robot's 10/30/10); the three
        equipment rows are `lo-hi / max / current / (equipment share)`;
        Agility has no paren.  The trailing line is indented to the first
        column and names the weapon with its *article prefix* ("Player is
        wielding a sword."), "nothing" when unarmed; NPC status is the same
        table ending "Robot is wielding nothing."
      * Wielding a non-carried object: "Player aren't carrying the rock!"
        [sic] — but `attack X with <non-carried>` says "Player **is not**
        carrying the rock!" (both probed live; the two paths genuinely use
        different verb forms).  Both tick combat.  The non-weapon refusal
        ends "!" (" is not a weapon!").
      Ported in sclibrar.cpp (`lib_print_battle_status`/`_attribute`,
      `lib_cmd_wield`, `lib_battle_attack_with`) + scbattle.cpp
      (`battle_attribute_bonus`).  Zero golden fallout (no corpus
      walkthrough runs `status` or a failing wield; colony's "not holding"
      line is the untouched `wear` path); all 76 rows re-verified PASS.
- [x] **RNG — re-opened and closed again: the won't-fix stands.** *(2026-08-01.)*
      run400 has **9 `Randomize` call sites**: `Form1.Form_Load` seeds from
      `Timer` at startup; `Form1.dencode` and `Sub_22_30` use the deterministic
      `Rnd(-1)` / `Randomize 1976` codec idiom; `Sub_20_6` (the load/restore
      machinery) mixes `1976` codec seeds with **`Randomize Timer` re-seeds**.
      Live proof of nondeterminism: probe `probeRNG` (enemy Acc 0-10 vs Agi
      0-10, Str 5-15), three fresh sessions, identical input (8 × `wait`), three
      different hit/miss sequences (H A H A H H A A / A A H H A H A H /
      H A H A A A A A) and staminas (165 / 168 / 187). Per-turn combat cannot
      be regressed byte-exact. **But**: the *load-time* attribute "current"
      roll is drawn from the deterministic 1976 stream *before* the Timer
      re-seed — the same file gives the same value every launch (Agility 0-10
      rolled 7 in all three sessions; a variant file with three extra text
      bytes rolled 9), so any load-time-rolled value is reproducible per file.
      `taftool.py` carries the exact VB6 LCG
      (`s' = (s*0x43fd43fd + 0xc39ec3) & 0xffffff`, post-1976 state
      `0x00a09e86`) if that ever becomes useful.

### Arena-probe recipe (2026-08-01) and surface observations

The probes above were authored from `test/adrift4/harness/make_battle_taf.py`: copy it, edit
the player/NPC battle stat lines (they are plain `s(lo); s(hi)` pairs), have it
also dump the uncompressed CRLF body, then
`taftool.py pack <body> <any real 4.0 taf> <out.taf>` — run400 accepts the
result. One probe = one ~2-minute Wine session. Degenerate (`Lo == Hi`) stats
make a probe immune to the per-session RNG, so single sessions are conclusive
for formula questions. Remember the settle-Return first, and count event lines
in the transcript instead of trusting the intended turn count.

Now in-repo: **`test/adrift4/harness/make_arena_probe.py`** (parameterized 4.0 probes — rooms
with exits, multiple NPCs with attitudes/speed/recovery, weapon/armour
objects; the M3/SP*/TS/RC configs are inline; a config may set `persp` to
author Globals/Perspective, which defaults to 2 — the `TK` take-wording probe
uses `persp=1` so the Runner narrates in the second person) and **`test/adrift4/harness/make_39_probe.py`**
(a genuine V390 file run390 loads: VB-PRNG obfuscation from absolute offset
14, and `sPassword` must be `pw[0:4]+"Wild"+pw[4:8]` — run390 checks
`Mid(5,4)`, the analogue of the 4.0 trailer check; `"    Wild    "` is the
no-password form).

**Corpus re-bless note (2026-08-01):** the `scr_randomint` low-bit fix (see
Target select above) changed every seeded transcript. All v4 goldens were
re-blessed after triage: 26 rows differed only in random flavor (event timing,
battle-roll variance) with win/score markers intact; five rows needed a new
per-row `SCR_SEED` to re-thread; the three Shadowpeak rows initially ran under
`SCR_LEGACY_RANDMAP=1`. ~~Re-deriving Shadowpeak under the fixed mapping is
open follow-up work~~ **DONE 2026-08-02** — only the Damastus chase (and one
Cerberus block) was actually fragile; the upstream through `press stone
button` is seed-robust (~1 in 26 seeds clean). New seeds 13/87/657, same
scores 700/715/735, and the legacy-randmap hook is deleted (see the Target
select item and Shadowpeak_walkthrough.md session 25).

Surface facts learned on the way (all consistent between engines unless noted):

- The Runner does **not** auto-wield at battle start (`status`: "Player is
  wielding nothing"), but a generic `attack X` **auto-selects a held weapon**
  and narrates with the weapon's Method verb ("Player shoot Robot with the
  blaster."). Scarier's mechanics already matched (per-attack default weapon,
  no state change), and **the Method-verb narration is now ported too
  (2026-08-01)**, from the DotFix `Battles.bas` decompile
  (`~/Adrift_decompile/run400-analysed/`, Proc_11_1/Proc_11_2): armed hit =
  "You shoot Robot with the blaster." (+"s" for NPC attackers), method 5 =
  "You throw the knife at Robot.", armed player miss = "<npc> manages to
  avoid your attack with <weapon>.", armed NPC miss = "<npc> attacks you
  with <weapon>, but you manage to avoid it.". Bare hands keep the plain
  forms.  **Verified live in run390 too** (p39 probe: "You shoot Robot with
  the blaster." — identical wording, second person), so no version gate.
  Corroboration from the corpus: les_feux's French ALR table was authored
  against these full sentences — the old generic wording only half-matched
  ("Vous frappez  demon."), the ported wording translates cleanly ("Vous
  tranchez un démon avec une épée longue.").  20 goldens re-blessed (battle
  flavour only; all win markers intact).  **Throw (method 5) mechanics,
  settled live AND ported 2026-08-01** (probes `pTD` in run400, `p39td` in
  run390 — both via `make_arena_probe.py`/`make_39_probe.py` variants): a
  landed player throw **moves the weapon to the current room in BOTH
  Runners** ("Player is carrying nothing." / look: "Also here is a spear.";
  `get` → throw again works), and its damage is version-split: **run400
  deals base Strength only — HitValue never contributes** (10 observed, not
  15; Battles.bas clears the wielded ref `global_78 = &HFF` at
  `loc_45E457` *before* the damage roll), while **run390 adds HitValue as
  usual** (one-shots a 35-stamina robot with Str 10 + HitValue 30 — its
  regardless-of-method rule).  No persistent accuracy penalty (status
  Accuracy 60→60 after throwing); a weaponless `attack` does NOT pick a
  floor weapon back up; **a *missed* throw keeps the weapon — probed live
  in run400 2026-08-02** (`make_arena_probe.py` variant TDM: player
  Accuracy 0-0 with a zero-accuracy spear against Agility 5-5 can never
  clear the strict `accuracy > agility` test, and after two missed throws
  the Runner still answers "Player is carrying a spear." with a bare floor),
  and **a throw that lands but does no damage still drops it** (variant TDZ,
  Defence 50 swallows Strength 10: "Player throw the spear at Robot, but it
  doesn't seem to do any damage." then "Player is carrying nothing." and
  "Also here is a spear.") — the drop sits ahead of the damage roll, as the
  decompile has it, and Scarier matches both transcripts.  The 3.9 half of
  the miss question does not exist: the legacy model has no accuracy/agility
  step, so every 3.9 attack connects.  NPC throws neither
  drop nor lose HitValue (Proc_11_2 has no equivalent).  Ported into
  `battle_resolve` (drop un-gated, Str-only gated on `!battle_legacy`);
  `light_up` was the one corpus casualty — its Chip fight and Death
  gauntlet were re-derived (582-command route, still 73 pts + THE END) and
  its golden re-blessed.
  The `status` "wielding" line matches since the wield-model port
  (2026-08-01): "wielding nothing" (and bare stats) until a wield is set —
  see the "Player-facing surface" item above (persistent wield ref;
  auto-select persists; asks on 2+ held weapons; no `unwield`; drop clears
  to nothing — all ported).
- Runner battle messages use the player's *name* where SCARE substitutes
  "you"/"your", with second-person verb agreement kept ("Player manage to
  avoid Robot's attack." — sic). Miss messages otherwise identical.
- `i` (inventory) consumes a battle turn in both engines.
- `save` during an active battle is refused ("That is not an option or
  command." in SCARE); write probe saves in a room *before* the enemy.
- An unrecognised NPC name in `attack <x>` gets "Who do you want to attack?".

## 2. Wildcard / any-turn task turn-ordering divergence — SETTLED 2026-08-01

**Both candidate causes were real, and neither was what the original notes
guessed.** Wildcard tasks are input-matched in both engines — there is no
end-of-turn wildcard pass at all.  The same-turn firing in `thetest` comes
from its always-restarting one-turn *events*, and the sole-response mystery
was the author's own ALRs.  Six authored probes
(`test/adrift4/harness/make_39_wildprobe.py` and inline variants), a neutralized-ALR rebuild
of thetest, and cross-checks of the same gen400-converted probe in **both**
run390 and run400 settled everything.  Fixed in `scevents.cpp` /
`scrunner.cpp` (`run_event_task`) / `sclibrar.cpp`; goldens re-blessed
(thetest, gateway, inverness + padded route; Shadowpeak byte-identical again
after the version gate).

What was actually established, each verified live:

- [x] **No end-of-turn wildcard pass.** A `*` task gated on "rock held", with
      no events in the game, does NOT fire on the `take rock` turn in run390 —
      it fires on the next command, exactly like Scarier always did.  Both
      positive (v2=1) and negated (v2=7/8) restrictions behave the same way.
- [x] **Events with a TaskAffected are the real mechanism.** thetest runs an
      always-restarting 1-turn event every turn.  In the **3.9 Runner** the
      event's "execute task" is dispatched by command text through the normal
      task matcher: the first task in list order that matches the text (`*`
      matches anything) *and* passes where+restrictions fires — so a runnable
      wildcard earlier in the list **steals the execution outright** (the
      affected task does not run that turn), a restricted match is passed
      over silently (its FailMessage is NOT printed), and restrictions are
      evaluated against post-library state.  Order decides: with the ticker
      task before the wildcard, no steal ever happens.
- [x] **The 4.0 Runner reverted (or never had) all of that.** The *same*
      gen400-converted probe in run400: no interception ever, the affected
      task runs directly, and a failing restriction prints its FailMessage
      loudly every turn.  That is exactly SCARE's original
      `task_can_run_task_directional → task_run_task` code — unsurprising,
      since SCARE was written against the 4.0 Runner.  Scarier now version-
      gates: `< TAF_VERSION_400` dispatches through `run_event_task()`, 4.0
      keeps the direct run.  Shadowpeak's ambient bell/rat lines are 4.0
      FailMessage prints and survive unchanged; gateway's four
      "execution is about to be start." lines were 3.9 FailMessage prints
      and correctly vanish.
- [x] **The thetest ALRs manufacture the "sole response".** The raw run390
      output for `drop clothes` (ALR patterns neutralized in a rebuilt .taf)
      is "You drop your clothes.  Nice try fish face!" — library message plus
      stolen-event task text, one paragraph.  The author's ALR rewrites that
      exact combined string to "Nice try fish face!", and a second ALR turns
      the `remove clothes` variant into "Lunatic, eh?  Yes?  Well tough." —
      the observed stray "!" is the leftover the pattern doesn't consume.
      Scarier emits the two texts as separate lines, so the combined-pattern
      ALR cannot match: **known residual cosmetic divergence** (the Runner
      joins a turn's output with two spaces into one paragraph and ALRs the
      whole; Scarier ALRs per string).
- [x] **Library `drop` of a worn item implicitly removes it** — in BOTH
      Runners ("You drop the cloak.", inventory empty after), while `drop
      all` leaves worn items alone (also verified).  Scarier used to refuse
      ("You are not holding..."); fixed via `lib_drop_named_filter` (named
      drops accept worn, the drop-all universe stays held-only).
- [x] **Scarier deliberately does NOT implement the Runner's completed-`*`
      claiming.** In run390 a completed non-repeatable `*` task answers every
      later command "You have already done that." — which **soft-locks
      inverness in the real 3.9 Runner**: after the dressing-room scene the
      catch event's execution and every movement command are eaten, and the
      game cannot proceed (verified live to the lock).  Scarier skips
      completed tasks in both the input matcher and the event dispatch, so
      inverness stays winnable (route padded with two `z`; the catch fires
      one turn later than run390 would have, because Scarier's event-start
      scan runs before the steal completes the gating task within the same
      event phase).  Same faithful-vs-playable call as Topaz.  The
      2026-08-10 port of the *exact-command* already-done refusal (§4, §5)
      does not disturb this: it runs last in `run_all_commands()`, after
      movement and the standard library, so a completed `*` task still
      cannot claim commands the game has an answer for.

Residual small divergences, noted not fixed: run390 appends task text to
`i`/inventory output where Scarier lets the wildcard replace it; the Runner
substitutes the player's name with second-person verb forms ("Player drop the
cloak."); and the ALR-over-joined-paragraph difference above.

- [x] **A multi-turn stop runs its walk's CharTask/ObjectTask on the arrival
      turn only — settled live 2026-08-02** (walk probe variant H in both
      generators: Times = 3 in the player's room, 2 away, no wildcard).  Both
      run400 and run390 fire `CHARTASK FIRED.` on the turn the walk counter
      hits that step's suffix-sum and never again during the stay: the 4.0
      probe fires on turns 1 and 6 of a five-turn cycle, and the 3.9 probe
      shows Bob examinable on turns 2-4, gone on turn 5, back with the task on
      turn 6.  Scarier used to fire on every co-located tick.  Fixed in
      `npc_tick_npc_walk` with an `is_arrival` gate that covers fixed-room
      stops; **follow-player stops joined it 2026-08-02** (next row).  A
      roomgroup stop does not behave this way -- "Ticket to No Where"'s lost
      girl wanders a roomgroup on a single Times=4 stop and live run400 has
      her speak on consecutive turns, i.e. the whole step re-runs every tick
      -- so roomgroup stops keep the every-tick behaviour.  Corpus fallout:
      four rows re-blessed (shadowpeak ×3 lose repeated "Seeker hums!"
      lines, melbourne_beach shifts RNG).
- [x] **Follow-player stops warp on arrival ticks only, and the player
      walking in on a mid-stay walker is a 4.0-only CharTask trigger —
      settled live 2026-08-02** (probe K = follow stop Times 3/2 with the
      rooms joined north/south, L = the fixed-stop twin, M = the ObjectTask
      twin; all in both generators, run in both Runners).  Findings:
      (1) BOTH Runners move a follow-stop walker to the player's room only
      on the walk-counter refresh tick -- on the stay turns Bob stands
      still even when the player walks away, and no catch-up ever comes
      (K turns 7-8).  Classic every-turn trailing is just a Times=1 follow
      stop, where every tick is an arrival tick.  (2) BOTH Runners fire the
      CharTask on an arrival tick even when the walker never moved -- K
      turn 11 prints no enter line but fires the task.  (3) run400 ALSO
      fires the CharTask when the PLAYER moves into the walker's room --
      at any stop, fixed, follow or the away stop, on every re-entry (L
      turns 3/8/10, K session 1 turn 8) -- while run390 prints only "Bob
      is standing here" on the identical moves.  SCARE already had exactly
      this check (the undo-gamestate block in `npc_tick_npcs`), so the fix
      was to version-gate it >= 4.0, not to add it.  (4) The player-side
      re-check is CharTask-only: probe M's rock (walk MeetObject/ObjectTask)
      does not fire when the player walks in on it, carries it in, or drops
      it beside the mid-stay walker -- object meets happen on the walk's
      own arrival ticks alone, which is what Scarier already did.
      `look` never fires anything (K session 2 turn 2).  Corpus fallout:
      six rows re-blessed (funhouse/donuts_intro/xfiles lose every-turn
      chaser trailing, tcom/inverness/melbourne_beach lose 3.9 player-move
      fires), and the three Shadowpeak routes re-threaded per the usual
      recipe (upstream seed sweep + `test/adrift4/harness/shadowpeak_chase.py`; new
      seeds 1/20/155, scores unchanged 700/715/735).
- [x] **run390 drops a walker's leave announcement when it cannot name a
      direction — settled live 2026-08-02** (3.9 walk probes H and J).  The
      earlier note "run390 prints no ExitText at all" was wrong: with the two
      probe rooms unconnected run390 prints `Bob BOB ENTERS..` on arrival and
      *nothing* on departure, but with the same walk over rooms joined
      north/south it prints both `BOB ENTERS. from the north.` and
      `BOB LEAVES. to the north.`.  Since a walk's stops are room indexes
      rather than exits, a walker can step between rooms that share no exit,
      and the pre-4.0 Runner suppresses the leave line for exactly that case
      (arrivals still print, directionless).  run400 prints the directionless
      leave line, so this is version-gated in `npc_announce`.  Corpus
      exposure, all re-blessed: "Melbourne Beach" (Judy, twice), "Lair of the
      CyberCow" (Vluurinik) and "thetest" (the Robot Guard, six times).
      Melbourne's enter/exit texts are `%jwalksin%`-style variables resolved
      through ALRs, which is why the live Runner's departure verbs vary.
- [x] **Walk CharTask/ObjectTask dispatch is wildcard-interceptable in the
      3.9 Runner, and a direct run in the 4.0 Runner — settled live
      2026-08-01** (`test/adrift4/harness/make_39_walkprobe.py` / `test/adrift4/harness/make_400_walkprobe.py`,
      variants E/F/G: looping two-stop walk, NPC Bob meets the player every
      other turn, CharTask = an un-typeable `#met` task).  run390's arrival
      turns print `WILDCARD FIRED.  Bob BOB ENTERS..  WILDCARD FIRED.` with
      the `*` task listed first (the second is the stolen walk dispatch;
      `CHARTASK FIRED.` never appears), and `... CHARTASK FIRED.` with the
      task order swapped — list order decides, exactly the event dispatch
      semantics, and statically the same P-code (Form1.characters at
      0005AAD5/0005AB88 = Form1.checkevent at 00048D83: copy
      `tasks[n-1].command[0]`, call `Form1.tasks(1)`).  A restricted walk
      task is skipped *silently* in run390 but prints its FailMessage
      (`METFAIL.`) in run400, whose walk handler (Sub_20_2 at
      00068B8E/00068BED) calls the same direct by-index runner (Sub_20_22)
      as its events.  Fixed: `run_npc_walk_task()` now version-gates —
      pre-4.0 shares the event dispatch (`run_task_command_dispatch()`),
      4.0 runs directly via `task_run_task` (loud FailMessage).  Whole
      corpus unchanged (110 PASS) — no corpus game has a stealable walk.
      The two "noted not chased" tails of this item were chased 2026-08-01,
      and each was half wrong:
      * **The 1-stop non-looping walk is a version split, not a shared
        no-op.**  run390 truly never runs it (wkC_390 screenshot: Bob never
        arrives, "You cannot see Bob from here.") -- but run400, probed live
        with a fresh 4.0 variant-C file (wk4C_400), runs it fine: the NPC
        arrives on turn 1, the CharTask fires exactly once, and nothing
        happens on the expiry turn.  The old "either live Runner" claim had
        no run400 evidence behind it.  Mechanics (P-code): the Runner's walk
        handler `Sub_20_2` lives in `Sub_20_62`, which is called ONLY from
        `Form1.evaluate` -- walks never tick at startup; the walk counter is
        seeded ΣTimes+1 (`Sub_20_12`), arrivals fire on exact suffix-sum
        boundary matches, and a non-looping walk's final decrement to 0
        marks it finished (0xFF) with no arrival processing.  **All fixed in
        Scarier 2026-08-01**: the startup `npc_tick_npcs` call is gone
        (Scarier used to move walkers and fire their CharTask before the
        first prompt, then AGAIN on the expiry tick -- a double divergence),
        a non-looping walk now expires silently, and a pre-4.0 one-stop
        non-looping *game-start* walk never starts (narrow gate: only the
        StartTask=0 case was probed live, and "deaths" (3.9) needs its
        task-triggered one-stop walk to keep running -- the demon at the
        end walks in on one).  Corpus fallout: 21 rows re-blessed (NPC
        arrivals shift one turn later), sun_empire's route gained a `z`
        (Jeriah arrives a turn later than its wait loop allowed).
      * **The empty-input `*` claim was simply wrong**: Scarier DOES match
        wildcard tasks against an empty input line, and did when the note
        was written (verified with the same probe at that commit and at
        HEAD).  Both live Runners agree -- in the wkE_390/wk4E_400 sessions
        the settle-Return itself fired the wildcard and ticked a turn.  No
        divergence exists here in any direction.

## 3. 3.9 → 4.0 conversion

Two separate problems that keep getting conflated.

### (a) Scarier's own V390 parse fixups

`sctafpar.cpp` carries a set of 3.9-only schema rewrites, each of which was a
reasoned guess: `V390_TASK_ACTION: Type>4?#Type++`,
`V390_TASK_RESTR: Var1>0?#Var1++`, `V390_OBJECT: _Openable_,Key`,
`V390_TASK: $RestrMask`, `V390_V380_ROOM_EXIT`, and
`V390_V380_ALT_TYPEHIDE_MULT = 10`.

- [x] **Whole-corpus 3.9 differential — DONE 2026-08-01. Two real bugs found and
      fixed; every other fixup is confirmed.** Method: gen400 as a *structural*
      oracle rather than a transcript one (below), plus live run390 probes for
      the two disputes it raised.
- [x] **gen400 as an oracle — DONE, and it works far better than a transcript
      diff.** File → Open a 3.9 `.taf` in gen400, then Save As 4.0; play both
      files under Scarier with `SCR_DUMP_TASKS=1` and diff the structural dumps
      (`scdump.cpp` — the ALT and OPENABLE sections were added for exactly this).
      All 28 3.9 games in `test/adrift4/games/` were converted. **Caveat
      learned the hard way: gen400 is not ground truth, only a second opinion.**
      Both times the dumps disagreed, run390 sided with Scarier's guards or
      against the Generator's arithmetic — so treat a mismatch as a question,
      then answer it by playing.

Verdicts, fixup by fixup:

- [x] **`V390_OBJECT: _Openable_,Key` (the 5 ↔ 6 swap)** — verified. The new
      `OPENABLE` dump section prints raw Openable/Key for every object (the
      older `LOCKKEY` line only fires for a resolvable key, and pre-4.0 games
      never have one). Zero mismatches corpus-wide.
- [x] **`V390_TASK_RESTR: Var1>0?#Var1++`, `V390_TASK: $RestrMask`,
      `V390_TASK_ACTION: Type>4?#Type++`, `V390_V380_ROOM_EXIT`** — verified.
      Every TASK / RESTR / EXIT / EVENT / WALK line matches the Generator's own
      conversion in all 28 games.
- [x] **Room-alt *ordering* — was wrong, now fixed (run390-verified).**
      `lib_find_starting_alt()` scans the alt array **backwards** for the first
      matching method-0/1 alt, so the least specific alt must be emitted first
      and the most specific last. `parse_fixup_v390_v380_room_alts()` emitted
      them the other way round. Correct order is
      `[LastDesc catch-all (disp 2), Task1 alt, Task2 alt, object alt (disp 0)]`.
      Three authored probes settled it live (`test/adrift4/harness/make_39_altprobe{,2,3}.py`):
      (1) after the gating task completes the Runner prints the AddDesc
      **instead of** the LastDesc, and with both tasks done Task2 wins;
      (2) an alt whose Task1/Obj is **0** is ignored entirely — Scarier's
      zero-guards are right and gen400 over-converts, which is all that the
      residual `cv13`/`cv20`/`cv23` ALT diffs are; (3) an applicable object alt
      outranks both task alts. Eight goldens re-blessed, corpus all-PASS.
- [x] **"Change battle attribute" attribute index — was wrong, now fixed
      (run390-verified).** A 3.9 character record is only
      `Stamina / Strength / Defence` (plus `Attitude` and `Speed` for NPCs) —
      no Accuracy, no Agility — so the 3.9 dropdown is eight entries and 4.0's
      twelve are that list with the Accuracy and Agility pairs spliced in:
      **0-4 unchanged, 5 → 7 (Defence), 6 → 8 (Max Defence), 7 → 11 (Speed)**.
      Scarier was feeding the raw 3.9 index to the 4.0 table, so a 3.9 game that
      handed the player armour instead made them a better shot. Fixed as
      `|V390_TASK_ACTION:_BattleAttr_|`; affects six corpus games (Matt's House,
      Phoenix_Destiny, SecretOfLostWorld, The_Spirits_Flight, deaths, gateway),
      three goldens re-blessed (all pure combat flavour — the player now
      correctly stops taking damage, and `deaths` even kills the demon).
      Probe: `test/adrift4/harness/make_39_battleattr_probe.py` — Robot Strength 20 vs player
      Defence 0, `zop` adds 40 to attribute 6, `zap` adds 40 to attribute 5.
      run390's `status` prints exactly three player lines,
      `Stamina / Hit strength / Defense value`, as `current (max)`; after `zop`
      it reads `Defense value: 0 (40)` and hits still take 20, after `zap` it
      reads `40 (40)` and every hit is "doesn't seem to do any damage".
      **gen400 gets this wrong**: it maps 6 → 8 and 7 → 11 correctly but turns
      5 into 11 as well, which is what a cascade of un-chained `If`s does
      (5 → 7, and then that same 7 → 11). Those 17 remaining dump mismatches are
      Generator bugs, not Scarier bugs, and are the *only* thing left in the
      differential.

### (b) Author-side conversion damage (the parked deep-dive)

Are any untested **4.0** games unwinnable because their author converted them
from 3.9 in the Generator and the conversion broke tasks? The distinction to
draw per game is *faithful data damage* (the real Runner fails too → document,
do not patch) vs *Scarier divergence* (→ engine fix).

- [x] **`Through time` — SETTLED, do not re-open.** It used to be the strongest
      candidate (82% of its tasks are `Where = No Rooms`, which Scarier blocks in
      `task_can_run_task_directional`), on the theory that the Runner's
      `Sub_20_74` had a conditional True path for where-type 0. That theory was a
      misread — the routine read was not the task room gate, and its "where-type 0"
      branch was really *reference*-type 0. (Do not trust the `Sub_20_74` label
      either: in the current decompile tree that name resolves to `457AC8`, which
      is the room alternate-description evaluator and nothing to do with scope.)
      The 2026-06-25 verdict was "**RESOLVED — faithful, unplayable-by-design;
      do NOT patch**" (in the derivation log, since pruned; recover it with
      `git log --follow -p -- test/adrift4/notes/WALKTHROUGH_TODO.md`). The game is an unfinished
      demo whose porch wall is the author's own in-game message
      (`test/adrift4/notes/Through_time_walkthrough.md`), and it has a passing
      golden row. The old decode plan lived in
      `test/adrift4/notes/TODO_decode_sub_20_74.md`, pruned in `aa30ba4f`; read
      it with
      `git show aa30ba4f^:terps/scarier/adrift-walkthroughs/TODO_decode_sub_20_74.md`.
- [x] **`Les Feux de l'enfer` — CLOSED 2026-08-01: it was never a conversion
      at all.** Data proof: of its 131 "change battle attribute" actions, 51
      use attribute indices 5/9/10 (the Max-Accuracy and Agility families),
      which **do not exist in 3.9** — a 3.9→4.0 conversion can only emit
      {0,1,2,3,4,7,8,11}.  The game was authored natively in the 4.0
      Generator (its header byte already said native 4.0;
      `Les_Feux_de_l_enfer_walkthrough.md` and the 2026-06-26/27
      WALKTHROUGH_TODO entries had separately established that its
      unwinnability is BY DESIGN — zero win-type EndGame actions — and that
      the one unreachable +10 is an authored restriction orphan the real
      Runner fails identically).  Nothing here for the engine.
- [x] Established and worth not re-deriving: no 4.0 game has an out-of-range
      task/event reference, so any conversion damage is subtle (off-by-one,
      wrong field meaning), never wholesale.  With Les Feux reclassified as
      native, **no author-side conversion-damage candidate remains — §3(b)
      is CLOSED.**

## 4. Semantics arbitrated against the Runners — the standing divergence table

Every row is settled — measured live, ported, deliberately kept, or refuted —
with two exceptions. *Restriction evaluation order* rests on the P-code alone
because no ADRIFT 4 restriction has a side effect, so there is nothing to
probe. (*An event's length when `Time1 ≠ Time2`* was the other exception until
2026-08-17, when §10's probes settled it in both Runners.) The second is the
*task whose only action is `End game`* row, added 2026-08-23: the divergence
itself is measured, and the decision not to port it stands, but the row names
an untaken probe — unrecognised verb with an unambiguous object **in hand**,
global refusal or verb-object refusal? — that would be needed before it could
be ported. This table is the live part of the file — keep it current.

| Item | Scarier | Runner | Status |
|---|---|---|---|
| Negated `Var2` inside the any/no-object quantifier | negates once around the whole quantifier (the meaningful reading) | its per-object switch handles `Var2` 0–5 only, so "any" always fails and "no" always passes | **Deliberate, confirmed live.** No corpus game authors it. Keep. |
| Dynamic-object index past the end (`Var1 ≥ 3 + ndynamics`) | clamps to the last object | raises "Subscript out of range" and dies | **Deliberate.** Unreachable in any shipped game. Keep. |
| Body-part statics in a `Var1 = 2` restriction | positioned at `OBJ_PART_NPC` | ~~statics have no location field, so they read hidden~~ **theory refuted live** — the Runner answers exactly like Scarier | **Settled 2026-08-01, NO divergence** (probes `pBP`/`pBP2` in `make_arena_probe.py`): with the parent NPC present, is-hidden FAILS, visible-to PASSES, not-hidden PASSES — for an NPC part and a player part alike, byte-identical to Scarier; visible-to tracks the parent NPC's room. With the parent absent, the Runner's `%object%` scope filter refuses the part ("I don't understand.") — that is the separate scope-filter row below, not a body-part issue. |
| Object scope when matching a task command | `uip_match_entity()` has no scope filter at all — matches anything | won't match an object that isn't present ("I don't understand what you mean!") | **Confirmed divergence, unfixed — corpus impact measured 2026-08-01: ZERO.** `SCR_TRACE_SCOPE=1` (scrunner.cpp, needs `SCARIER_DUMP_TOOLS`) logs any golden turn where a matched `%object%` task binds an absent object (SCOPE-MISS = Runner would refuse the command; SCOPE-BIND = Runner would bind a different, present object): all 76 rows replayed clean, zero hits. Static exposure is small too — only 5 games author `%object%` task commands at all (adriftorama 67, goldilocks 20, Screen Savers 19, SRSintro 2, X-Files 2). So the divergence is reachable only by off-route input. A faithful fix means porting the Runner's command-reference filter (presence for objects, and per the `pWS` probe *seen-ness* for `%character%`) — known from probes only, since the routine has yet to be located in the decompile: the old `Sub_20_74` label resolves to `457AC8`, which turns out to be the room alternate-description evaluator — plus a disambiguation rule for present-vs-absent name clashes; not worth it until some game demands it. |
| Optional `{word}` whose look-ahead fails after consuming text | ~~`uip_match_optional()` fell into `uip_match_alternatives()` at the position the *failed* look-ahead had already advanced to~~ **FIXED 2026-08-03**: rewind to `start_posn` on the failure path too | matches | **Was a real divergence, now fixed.** Ground truth is the author's own published transcript of *Monsters (Release 2)*, which shows `shine flashlight on the brainsucker` working. Task 2's pattern is `[defeat/shine/turn/put] {the} [flashlight/light] {on} {the} {brainsucker} {brain}  {monster}`; the look-ahead from `{brainsucker}` let `{brain}` eat the first five letters of "brainsucker" (`uip_match_word()` is a prefix compare with no word-boundary check), failed on the trailing "sucker", and — because `uip_match_list()` has no backtracking of its own — the alternatives were then tried from "sucker". Cost the game 5 of its 40 points. `uip_match_wildcard()` always restored position on failure; only the optional matcher didn't. Zero golden churn across the 154-row v4 suite. |
| A `/` outside any `[]`/`{}` group | ~~`uip_parse_list()` treated it as an alternatives separator *at every depth*, so the top-level list returned early — **without appending its `NODE_EOS`** — and the pattern degenerated to everything before the first slash, prefix-matching any input that started with it~~ **FIXED 2026-08-04**: a group-depth counter; at depth 0 the `/` parses as a literal word node instead | a bare `/` is an ordinary literal character — `Proc_9_4_45D940` (the whole of run400's command matcher, decompiled in `~/Adrift_decompile/run400-analysed/NewParse.bas`) only ever looks for `/` *between* `[]`/`{}` delimiters: it dispatches on `Left(pattern,1)` being `[` or `{`, and any other leading run is compared with a straight `Left(input,n) = Left(pattern,n)` up to the next `[`/`{` | **Was a real divergence, now fixed.** Found in *Ba'Roo!*: TASK 62's `take/get/eat stew` collapsed to `take`, so **every** `take X` in the Communal Dining Hall was answered "Unfortunately you don't have a way to eat it…" and TASK 63 (`[take/pull/tear] [meat/animal/roast]`) was unreachable — which blocks the walkthrough, since the meat is the game's only food and the endgame climb needs it. In run400 the pattern matches only the literal string `take/get/eat stew`, i.e. the task is dead, which is what the author's own transcript shows. Corpus exposure is small and now measured: 8 games author a bare top-level `/` in 36 task commands, and all but two are `#`-style labels or dashes (`-2/3/4`, `1 - kridlor66 who killed you/how died`) that were silently prefix-matching. Zero golden churn across the 190-row v4 suite. |
| Input synonyms whose replacement is itself synonymed | ~~`pf_filter_input()` walked the input word by word, took the **first** synonym that matched, spliced it in and skipped past it — no later synonym ever saw the text it wrote~~ **FIXED 2026-08-04**: after one fires, the rest of the list still gets a look at the replacement, but only *as a whole* — a later synonym fires again only when its original is the entire replacement region (`extent == span`), and then replaces all of it | later synonyms do act on an earlier one's output, but only on the whole of it | **Was a real divergence, now fixed; both halves of the rule are pinned by a game, each read off run400 under Wine.** *Lair of the Vampire* maps `harris`→`steve` **and** `steve`→`harris`, the author's way of letting both spellings reach one NPC; run400 accepts `ask harris about key` (the game's own walkthrough opens with it) and `x harris`, whereas first-match-wins left a `steve` the character has no alias for and made the cellmate — who holds the picklock the whole game hinges on — unaddressable. The whole-region half comes from *Yak Shaving for Kicks and Giggles!*, which maps `flags`, then `line`, then `clothes` all onto `clothes line`: run400 answers `x flags` with the laundry description, so `line`/`clothes` must **not** fire on the words *inside* the `clothes line` the first synonym wrote. Letting them (the naive "apply all that match") is not merely wrong but non-terminating — `x flags` grows one `clothes line` per pass and the run dies on the harness's 30 s `ulimit -t`. An intermediate model, one whole-string pass per synonym in list order, terminates and fixes Lair but garbles Yak to `x clothes line clothes line line` → "Either that isn't here, or it's not important."; it was falsified against the Runner and abandoned. Zero golden churn across the 190-row v4 suite. |
| `%object%` given a *partial* prefix | ~~only `"Prefix Short"` and the bare `Short` were matchable~~ **FIXED 2026-08-03**: `uip_build_candidate()` also stores the prefix with its leading words dropped one at a time | matches a partial prefix | **Was a real divergence, now fixed, with two independent transcripts as ground truth.** *Monsters (Release 2)*: Prefix `Sissy's four poster` + Short `bed`, and the transcript prints the description for `examine the four poster bed` where Scarier said "I see no such thing". Re-running the suite then changed exactly one line in one other golden — *Shadrick's Travels*, `climb oak tree`, "You can't climb that." → "You can't climb the old oak tree." — which is verbatim what line 80 of *that* game's upstream transcript says. The Short itself is never cut down, so a multi-word Short must still be given whole. `SCR_DUMP_TASKS`'s `OBJNAME` line now prints `prefix=[...]` and each `alias=[...]` so this class of failure is a lookup rather than a guess. |
| 3.9 shoot-Method strength | version-gated: 3.9 adds `HitValue` to base Str, 4.0 replaces | both confirmed live (run390 one-shot / run400 two hits) | **Fixed 2026-08-01** (`7a4cb7c2`). |
| Upgraded-3.9 combat | `SCR_ASSUME_COMBAT` opt-in; matches author intent | **stalemates, confirmed live 2026-08-01** (Azra: converted acc/agi all 0-0) | Settled — opt-in stays. |
| Restriction evaluation order | evaluates all, no short-circuit | `Sub_20_65` replaces `#` with T/F in a bool-expr string, so it can't short-circuit either | Believed matched (ADRIFT 4 restrictions cannot have side effects — no restriction type mutates state — so "verify a side effect runs" is unprobeable and moot; the P-code reading stands on its own). |
| Integer division rounding | `Round((a/b) + 0.000001)` — banker's rounding with a +∞-biased epsilon (scexpr.cpp's asymmetric compare) | same | **Confirmed live 2026-08-01** (probes `pDIV`/`pDIV2` in `make_arena_probe.py`, which now authors variables and set-var actions): with *true* negative dividends (via `%v1%/2`), run400 answers −5/2 = −2, −7/2 = −3, −1/2 = 0, and 5/2 = 3, 7/2 = 4, 1/2 = 1, 22/7 = 3 — byte-identical to Scarier. Only 5 corpus games author expressions at all (47 exprs; `circus` has the only divisions, positive). |
| Unary minus in expressions | folded into the literal: `-5/2` = (−5)/2 = **−2** | tokenised as an *operator* that reduces after `/`: `-5/2` = 0−(5/2) = 0−3 = **−3** | **NEW divergence, found 2026-08-01 by `pDIV` (its only diverging cell — the `pDIV2` variable forms all agree, which is what pins the cause to the tokeniser, not the rounding).** Zero corpus exposure: none of the 47 authored expressions uses a unary minus (`SCR_DUMP_TASKS` now prints `expr=[...]` on type-3 ACT lines). Documented, not fixed — reshaping scexpr's parser to give unary minus binary-minus precedence risks more than it buys. ~~Open tangent: ADRIFT 5 shares this token engine, so a5sexpr's literal `-5/2` deserves the same one-probe check.~~ **Probed 2026-08-01: NO divergence on the ADRIFT 5 side.** A 44-row battery through the REAL `clsVariable.SetToExpression` (scratch C# driver against the FrankenDrift.Adrift Release dll — no adventure loaded, bare `clsAdventure` + registered vars) matches a5sexpr row-for-row on both the raw string and the `SafeInt(Val())` readback. clsVariable *does* tokenise leading `-` as an operator (`GetToken` clsVariable.vb:134) and reduces the dangling `op expr` pair on run 2 (clsVariable.vb:959-972), after `/` rounded on run 1 — but v5's `Math.Round(AwayFromZero)` is symmetric (`round(-q) == -round(q)`), so the operator parse and a5sexpr's folded parse coincide everywhere, including the var-token vs textual-substitution split (`%v1%/2` with v1=−5). The v4 divergence exists only because run400's `+0.000001` epsilon rounding is asymmetric. Sole divergent row: `-2^-1` (= −0.5) reads back −1 in FD under an English locale (`SafeInt` = VB `Int()` floor) vs 0 from scarier's strtol — and even the real runner is locale-dependent there (comma-decimal `Val("-0,5")` → 0). Kept as-is: fractional results need `^` with a negative outcome, zero corpus exposure. Battery banked as unary-minus cases in `test/adrift5/harness/a5sexpr_test.cpp`. |
| ADRIFT 5 paren group ahead of a `*`/`/` chain | a5sexpr is a recursive-descent evaluator, uniformly **left-associative**: `(A+0)/B/C` = `((A+0)/B)/C` (probed live: `(100+0)/10/5` = 2) | clsVariable's run-based reducer: a paren group whose contents need run 2 (`+ - mod ^`, unary minus, comparisons, a delayed nested group or funct arg) cannot collapse on run 1, so `*`/`/` pairs to its RIGHT reduce first and the trailing chain effectively **right-associates** — `(A+0)/B/C` = `A/(B/C)` (would answer 50) while bare `(A)/B/C` collapses immediately and stays left-assoc (gate: `If run = 2 Or optoken.Value = "*" Or optoken.Value = "/"`, clsVariable.vb:866 in the open-sourced ADRIFT 5.0.36.5, :826 in FrankenDrift — identical, so **not fixed in the original**) | **Documented, not fixed — reported live by ralphmerridew (intfiction 81359/37), confirmed by source reading 2026-08-22.** Observable only through the per-division `Math.Round(AwayFromZero)` — pure-`*` regrouping is numerically exact and the unary-minus fold is invisible (symmetric rounding, see the row above). NOT the 5.0.26 "Adventure Upgrade" issue: that dialog (a5model.cpp) only rewrites restriction `#A#O#` BracketSequences and never touches arithmetic. **Zero corpus exposure, measured 2026-08-22**: all 174 a5 corpus games dumped to XML via `harness/a5dump`; 85,566 candidate expressions (`<#…#>` embeds, Set/Inc/DecVariable RHS, quoted Variable-restriction values, Expression restrictions, plus a whole-file raw-text net) scanned for the exact divergence condition (delayed group followed by ≥2 `*`/`/` ops incl. ≥1 `/`): 0 hits; the 22 coarse paren+division candidates are all safe shapes (`(a*b)/c`, `a*(b/c)` — run-1-only group contents). Mirroring would mean grafting the run-gated reduction order onto a5sexpr for a behavior no game reaches; the left-associative choice is deliberate and pinned by the paren/division-chain block in `test/adrift5/harness/a5sexpr_test.cpp` (runner-divergent values noted per case). |
| ADRIFT 5 identical `<#…#>` bodies in ONE text block | ~~each occurrence evaluated AND displayed independently — The Last Expedition's `<#oneOf("Eight","Seven","Ten","Six","Nine and a half")#> minutes later they both came to. … <#same#> minutes later they both came to their sense again` could print two different numbers, Lost Coastlines' `<# Oneof("two","three","four")#> return empty handed...<#same#> do not return at all` likewise~~ **PORTED 2026-08-29** | `Global.ReplaceExpressions` (Global.vb:510) collects its regex matches up front, then per match does `sText = sText.Replace(m.Value, EvaluateExpression(..))` — a replace-**all** — so the FIRST match's value lands in every identical slot, while the later matches still call `EvaluateExpression` (their OneOf/Rand/URand **draw**, keeping the RNG stream in step) and have nowhere left to land. Reported by saabie via ralphmerridew (intfiction 81359/39). | **Ported 2026-08-29.** `replace_expressions` (a5text.cpp) keys on the raw body per call: a repeat still reduces (draws) but emits the first value; a repeat on the deferred end-of-Display path is pushed as `\005<orig>\005body` and `a5run_draw_defer_entry` (a5run_action.cpp, shared with the early sentinel resolver in a5run_resp.cpp) draws it, then shows entry `<orig>`'s value. Corpus scan 2026-08-29 (174 games, `harness/a5dump` → XML): 25 random duplicate-tag blocks, all in Lost Coastlines (23, incl. the 14-way chalk scrawls) and The Last Expedition (2); none on a walkthrough path, which is why the FrankenDrift comparison never saw it. Pinned by `test/adrift5/harness/a5_exprdup_test.cpp` (`make -f Makefile.headless a5duptest`, part of `test`): 40 seeds × inline + deferred paths, repeat shows one value, and an `A A B` block costs exactly the RNG draws of its `A A' B` twin. |
| ADRIFT 5 `<#…#>` tag INSIDE an expression's own result | the emitted value is never re-scanned: a tag produced by an expression stays literal | same up-front match list, so (1) a tag inside a result and (3) an earlier tag re-introduced by a later result are **also** left unevaluated — but (2) when the SAME tag also appears directly elsewhere in the block, the replace-all sweeps the copy inside the result too, evaluating it with the direct copy's value (ralphmerridew, intfiction 81359/41) | **Documented, not fixed 2026-08-29.** Cases 1 and 3 already agree. Case 2 would need replace-all semantics over the evolving string; zero corpus exposure (no string literal, string variable or function text in the 174 games contains `<#`). Revisit only if a game turns up. |
| ADRIFT 5 evaluation order of two `rand`/`urand` calls when one has a compound argument | a5sexpr is recursive descent: `100 * urand(0+0, 5) + urand(0, 5)` draws the LEFT call first, same as `100 * urand(0,5) + urand(0,5)` | clsVariable's run-based reducer cannot collapse a call whose argument still needs a run-2 op (`+ - mod ^`, unary minus), so the textually LATER simple call reduces — and draws — first; with `urand`'s no-repeat pools the two forms answer differently (ralphmerridew, intfiction 81359/39) | **Documented, not fixed 2026-08-29.** Same family as the paren/division row above: grafting the run-gated reduction order onto a5sexpr for a shape no game authors. Corpus scan 2026-08-29: expressions with ≥2 random calls and any compound argument = 1 hit, a false positive (Lost Coastlines' `Oneof(%names[RAND (1, 1000)]%, %names[RAND (500, 1000)]%)` — the RANDs are `%var[idx]%` substitutions resolved before the evaluator runs). Note for the thread: `if()`/`AND`/`OR` inside expressions evaluate every operand in BOTH engines (the reducer only folds `if(test,a,b)` once all three are `expr` tokens; a5sexpr builds the arg vector first) — Scarier's short-circuiting is in restriction lists, not expressions. |
| Combat RNG | own generator | VB6 `Rnd`, `Randomize Timer` on the load path | Won't-fix confirmed live (§1): per-turn combat differs across identical fresh sessions. |
| Battle messages | second person ("you"/"your") | run400 uses player's name with 2nd-person verb forms ("Player manage to avoid…"); run390 uses second person | Presentational only; "you" kept (matches run390). Method-verb weapon narration **ported 2026-08-01**, verified live in BOTH Runners; noted §1 surface facts. |
| Wield model | ~~per-attack default~~ **PORTED 2026-08-01**: persistent wield ref, matching the Runner | persistent wield ref; single-held auto-select persists; ASKS with 2+ held ("What do you want to attack X with?"); NO `unwield` verb; drop clears to nothing; status folds only the actual wield | **Fully settled live 2026-08-01** (probe `pWS`) and **ported the same day**. Corpus fallout (found late — stale-binary erratum, see §1): 3 wording goldens + the Shadowpeak routes' post-chapel bare attacks, all repaired/re-blessed. ~~Remaining cosmetic gaps: status layout (Max column, "(bonus)" parens), "not holding" wording.~~ **Cosmetics ported 2026-08-01** (probe `pWS2`; see §1): four-column status table (Range/Max/Current + equipment share in parens, Stamina row = live/max/live, no "You have:" header, article-prefix wielding line), wield refusal "aren't carrying …!" vs attack-with "is not carrying …!". |
| Thrown (method 5) weapon | drop + version-split damage ported | moves to the room on a player throw in BOTH Runners; damage = Str-only in run400 (HitValue ignored), Str+HitValue in run390 | **Confirmed live in both Runners and PORTED 2026-08-01** (probes `pTD`/`p39td`; see §1 surface facts). `light_up` route re-derived. |
| Enemy target selection | was pinned to one target per session (LCG low-bit + `% range`) | uniform per-turn pick among ally/player | **Fixed 2026-08-01** (`scr_randomint` multiply-shift); corpus re-blessed. |
| Event TaskAffected execution | version-gated: 3.9 = matcher dispatch (wildcard steal, silent restricted skip), 4.0 = direct run (loud FailMessage) | run390 and run400 genuinely differ — same converted probe, opposite behavior | **Fixed 2026-08-01** (§2); both halves verified live. |
| Named `drop` of a worn item | implicitly removes then drops (named only; `drop all` skips worn) | same, in BOTH Runners | **Fixed 2026-08-01** (`lib_drop_named_filter`). |
| Completed non-repeatable `*` task | skipped by matcher and event dispatch | claims every later command: "You have already done that." — soft-locks inverness for real | **Deliberate divergence.** Do not import; see §2. |
| A **spent** (done, non-repeatable) task whose restrictions now FAIL with a message | ~~restrictions first in every version, so the fail message printed~~ **version-gated 2026-08-23** | **4.0**: restrictions are checked *before* the done state, so the fail message prints and eats the command — on the library-callback path too; **3.9**: done state first, so the answer is "You have already done that." whether the restrictions pass or fail, and the fail message never prints | **Measured live and FIXED 2026-08-23.** Probe `DONE` in `make_arena_probe.py` (run400, `Adrift_14.txt`) against its 3.9 twin `make_39_doneprobe.py` (run390, `Adrift_18.txt`): with the stone dropped, run400 answers `alpha` "BLOCK-ALPHA." and `get gem` "BLOCK-GEM.", run390 answers both "You have already done that." / the plain library take. `scrunner.cpp` gates the spent-task arm of `run_game_commands_common()` on `TAF_VERSION_400`, and pre-4.0 falls through to `run_task_refusal()` as before. This is what the two long-standing 3.90 corpus FAILs (`cybercow_win`, `melbourne_beach`) were telling us. |
| An **unspent** task whose restrictions fail with a message, matched from the **library take/drop callback** | ~~loud pass ran in every version, so the fail message beat the library~~ **version-gated 2026-08-23** | **4.0**: the fail message wins (`get gem` without the stone → "BLOCK-GEM.", run400 `Adrift_20.txt`); **pre-4.0**: the library wins silently (run390 takes the gem, `Adrift_18.txt`; run380 on the real `haunt.taf` answers `take fish` "You take dead fish from fish tank." where its `take * fish` task's "task 15 not done" restriction fails with the message "-", `Adven_2.rtf`) | **Measured live and FIXED 2026-08-23.** `run_game_task_commands()` now passes `include_restrictions` only from 4.0 on; tasks whose restrictions *pass* still run in either version. Corpus fallout: one golden, `haunt_solution.txt`, re-blessed **against the run380 replay of the same 64 commands** rather than on engine authority. |
| Below 4.0, a task match that says nothing still **claims** the command | falls through to the standard library verb | claims it: `x book` on a spent `* x * book *` task answers "You have already done that." (not the book's description — `look at book`, matching no task, does print it, run390 `Adrift_19.txt`), and a silent task that runs and prints nothing leaves "I don't understand." rather than the library answer (`Adrift_18.txt`). run400 falls through in both cells. | **Measured 2026-08-23, deliberately NOT imported.** Same family as the `*`-task row above, and it fails the same way: recording the silent match and skipping `run_standard_commands()` below 4.0 costs **15** v4-corpus goldens, four of them walkthroughs that stop winning (`inverness` included — the very soft-lock §2 already refuses to import). The rule as stated is too broad and the narrowing is unmeasured. `scrunner.cpp` carries the note where the flag would go. |
| A 4.0 task whose **only** action is `End game`, with no CompleteText | counts the ending as output, so the task **claims** the command and the library never sees it | falls through to the library like any other silent match — the ending prints at *end of turn*, so at match time the task has said nothing | **Measured 2026-08-23, NOT ported.** `relojero.taf` ("La hija del relojero") task 5 is `arreglar *fenix`, gated on holding the Phoenix and the broken cord, with a single `type=6` action and no text of its own — the ending prose is the game's WINTEXT. run400 answers the winning `arreglar fenix` with the game's **global** "Disculpa pero no te entiendo." and *then* the WINTEXT; Scarier prints the WINTEXT alone. That one line is the only difference across the whole 11-command transcript (`pfx/drive_c/adrift/relojero.txt`); strip it and the remainder is character-identical. The cause is `task_run_end_game_action()` returning `var1 != 3`, claiming output that its own comment documents as printed later by `task_print_end_game_message()`. Returning FALSE is **not** sufficient: with `is_running` already cleared the library catch-all `* %object% *` (`lib_cmd_verb_object`) then claims the command silently, and were it to speak it would print "I don't understand what you want me to do with the Fenix de laton." — the verb-object refusal, not the global one run400 gave. Why run400 reached the global message with the referenced object in hand is **unmeasured**; see the probe named in the closure log below. Cosmetic, one line, at the last command of the game, and the fix lands in the dispatch core — left alone until that probe is taken. |
| `drop <thing> in/on <container>` | ~~the priority `drop %text%` pattern swallowed the `in <container>` tail and answered "Drop what?"~~ **FIXED 2026-08-02** | routes it to the put-in / put-on handlers: `drop wallet in bin` → "You put your wallet inside the rubbish bin.", `drop wallet on bin` → the put-on refusal "You can't put anything onto the rubbish bin!" | **Confirmed live against run400 and FIXED 2026-08-02** while deriving `Ticket to No Where`, whose author-route disposes of five bits of litter with `drop <litter> in bin` (2 points each — the difference between 100 and its full 110). Six patterns added to `PRIORITY_COMMANDS[]` (`scrunner.cpp`), covering `drop`/`put down` × `in`/`on` × plain/`all`/`all except`, placed *before* the plain drop patterns so the `%text%` no longer eats the tail. No corpus fallout. |
| What `all` ranges over | ~~everything a named take can reach, including the contents of a carried open container~~ **FIXED 2026-08-02** | leaves alone anything already in the player's possession; a *named* take still reaches into a carried open container | **Confirmed live against run400 and FIXED 2026-08-02.** In `Ticket to No Where`, holding the open bag of shopping and typing `get all` answers "You take the pamphlet." and leaves the tights, pet food, deodorant and gloves in the bag, while `get paper` still lifts the scrap out of the carried wallet ("You take the scrap of paper from your wallet."). `lib_take_all_filter()` (`sclibrar.cpp`) = `lib_take_filter && !obj_indirectly_held_by_player`, used by `lib_cmd_take_all` and by the `take all except` resolver in `lib_take_multiple_common`. The visible symptom was four bogus items of inventory weight later refusing `get banana skin`. Corpus fallout: the two ALEXIS goldens, re-blessed after proving the change correct there too (its leather bag is player-carried, so the Runner would never have emptied it either). |
| "get all" with nothing takeable | ~~"There is nothing to pick up here." in every version~~ **version-gated 2026-08-15** | pre-4.0 "There is nothing to pick up here."; 4.0 "There is nothing worth taking here." | **Settled live and PORTED 2026-08-15**, together with the bare-take row below — same handler, same split. run370 (`p37pos.taf`), run380 (`marooned.taf`) and run390 (`p39held.taf`) all answer "There is nothing to pick up here."; run400 (probe `TK`) answers "There is nothing worth taking here." 4.0 also has **no "else" form**: `take all except rock` with nothing else left answers the same flat line, where Scarier composed "There is nothing else to pick up here." Both refusal sites in `sclibrar.cpp` (`lib_cmd_take_all`, `lib_take_multiple_common`) now go through `lib_is_version_400()`. |
| `%character%` / `%object%` as the **last element inside a `[...]` or `{...}` group** | ~~can never match: `uip_match_remainder()` built an empty remainder list and `uip_match_list()` fails empty lists by design, so every candidate was rejected~~ **FIXED 2026-08-02** | matches | **Confirmed live against run400 and FIXED 2026-08-02** while deriving `ADRIFTMAS Party`. In the real Runner `kiss mystery` on the Front Steps answers "You lay a Happy Holidays kiss on Mystery.  Turns out that Mystery didn't appreciate it much and belts you right in the kisser." — i.e. it matches TASK 20 `[kiss {the} %character%]`; Scarier fell through to the library's `lib_cmd_kiss_npc`. Root cause in `scparser.cpp`: `uip_parse_list()` appends a `NODE_EOS` **only** on `TOK_EOS`, so a reference that ends a group has `right_sibling == NULL`, and the temporary remainder list `uip_match_remainder()` builds is empty. Fix: when `node->right_sibling` is NULL the remainder is vacuously satisfied — return TRUE. Top level is unaffected (the EOS sibling still enforces end-of-string after the group), and with an empty remainder the existing `max_extent` logic already picks the longest candidate. Two very common idioms were dead: `[kiss {the} %character%]` and `[smack/hit/punch/kick]{the}[%character%]`. Corpus fallout: one row, `ticket_solution.txt`, where TASK 405 `[say][hello][to]{the}[%character%]` now fires on `say hello to john tailer` instead of falling through to the game's default response — re-blessed; the only other transcript change there is RNG drift on a random Trainspotter utterance (it also comes and goes across seeds in the *old* binary, so it is stream shift, not semantics). `SCR_TRACE_MATCH=1` over all 320 commands of that route shows exactly one added MATCH line and no removals. ~~Open tangent: `uip_match_text()` has the same shape and so a trailing `%text%` inside a group is presumably equally dead — unprobed, no corpus exposure found.~~ **Probed 2026-08-02 — half right, and NOT a fix candidate: see the next row.** A trailing `%text%` inside a group really is dead in Scarier, but it is dead in run400 too, so that half is agreement, not divergence. |
| `*` or `%text%` **inside** a `[...]` / `{...}` group | two cells match that the Runner refuses: a group-trailing `*` consuming **zero** words (`[eat *]` fires on bare `eat`), and a **mid-group** `%text%` (`[quip %text% hard]` fires on `quip hi hard`, capturing "hi"). Everything else agrees. | **run400 does not support either token inside a group at all** — a pattern containing one is dead for every input: `[echo %text%]`, `[jot %text%]`, `[mark %text%] now`, `[quip %text% hard]`, `[eat *]`, `[nib/nab *]`, `[snag *]` all answer "I don't understand.", bare or with a tail. `%character%` in a group is fine (`[poke {the} %character%]`, `[prod %character% hard]` both fire), as are literal groups (`[wham]`, `[zap/zop] thing`) and a top-level `mimic %text%`. | **Probed live 2026-08-02** (probes `TX`/`TX2` in `make_arena_probe.py`; note a probe's CompleteText must echo a capture as `[%text%]` — `<%text%>` is eaten as markup and prints empty). This replaces the `uip_match_text()` tangent on the row above: the presumed "dead in Scarier" cells are *shared* dead behavior (`uip_match_text()` builds the same empty remainder list as the old `uip_match_entity()` bug, but returns FALSE, which is what run400 does anyway), and the only real divergences run the other way — Scarier matching where run400 refuses. **Zero corpus exposure, measured**: unpacking all 99 v4-era corpus `.taf` (67 zlib v4.0 + 32 PRNG-XOR v3.9/3.8; `BeThere.taf` is an ADRIFT 5 file and belongs to the a5 corpus) and scanning every bracket group turns up exactly ONE group-embedded token anywhere — `SRSintro`'s `[ask] {the} [woman/trader] [about] [%text%]`, which is the *shared-dead* shape: Scarier answers the game's own "Use the format …" fallback there, exactly as the Runner would. No corpus game puts a `*` inside a group at all. **Documented, not fixed** — suppressing the two cells means teaching `uip_match_wildcard()`/`uip_match_text()` that they sit inside a group, and the zero-word `*` rule they'd have to special-case is *correct* at top level (row below); nothing in the corpus, and no known game, pays for the risk. (Scan script committed as `test/adrift4/harness/taf_pattern_scan.py` — it de-obfuscates both TAF generations and greps task-command lines. It reads the raw file rather than going through `SCR_DUMP_TASKS` because it needs the *pattern text* of every task in 99 games at once, which no dump prints; `test/adrift4/harness/build.sh` does define `-DSCARIER_DUMP_TOOLS`, so `SCR_DUMP_TASKS` itself works fine — an earlier note here claiming otherwise was reading a stale binary. Re-run the scan before deciding any future parser question by exposure.) |
| `*` matching **zero** words in a task command | matches | **matches too — NO divergence** | **Settled live 2026-08-02** (probe `ST` in `make_arena_probe.py`): run400 fires `foo * bar` on `foo bar`, `qux *` on `qux`, `* yop` on `yop`, and a zero-word match with a failing restriction still prints the FailMessage. Both engines identical.  The `TheADRIFTProject` mystery that motivated this row had a different cause entirely — see the put-family precedence row below. |
| Which failing restriction's FailMessage prints | lowest-indexed failing restriction; empty message → fall through to the library | same — lowest-indexed, incl. under a mixed `#A#A#O#` mask | **Settled live 2026-08-02** (probes `FM`/`FM2`): run400 answers AFIRST / fallthrough / CFIRST / DFIRST and ETWO / ZTWO / HTHREE — byte-identical to Scarier on every cell.  No divergence. |
| Put-family precedence over a matched-but-failing task | ~~loud-fail phase ran before the standard put-in/put-on handlers, so the task's fail message always claimed the input~~ **PORTED 2026-08-02** | a `put`/`drop X in/on Y` that the library can resolve **and complete** runs instead of the fail message ("put pill in cup" answers "You put the small pill inside the coffee cup." with a failing `put * pill in cup` task matched); when the library would **refuse** (target not a container/surface) the fail message wins ("drop pill in slime" prints the task message, not the refusal); unresolvable nouns also leave the message ("put pill in goo") | **THE ACTUAL `TheADRIFTProject` divergence — settled live 2026-08-02** (a `.tas` transplant reproduced the author's 2004 comp transcript on our run400, then probes `FM4`–`FM7` isolated the rule; `FM5` vs `FM7` is the minimal pair — same task, the nouns' existence flips the outcome).  Ported: the six `put …` patterns joined their `drop` twins in `PRIORITY_COMMANDS` (scrunner.cpp), and `lib_put_in_is_valid`/`lib_put_on_is_valid` **defer** instead of refusing during that pass (`run_priority_defer()`; the scan stops so the plain-drop `%text%` rows can't swallow the input, and STANDARD_COMMANDS keeps duplicates of all twelve rows to print the refusal when no task claims it).  Also: the two-object canonical retry (`lib_try_game_command_common`) now tries only the fully-prefixed form — the prefix-less "put pill in cup" retry was exactly the string the wildcarded task steals, while Wax Worx's `get * head` still claims "get marie" via the prefixed "get Marie Antoinette's head" (that row regressed under a broader any-wildcard exclusion and pinned the rule).  Corpus: 102/102 PASS; `TheADRIFTProject` reverted to the author's route (former Repair 1 dropped) and re-blessed. |
| Single-object library success vs failing explicit-verb task | library acts (e.g. `take rock` takes the rock when no task passes) — **which is run390's behavior** | **version split, both probed live 2026-08-02**: run400 prints the task's fail message (`take rock` with a failing `take * rock` task answers TFAIL, rock stays put); run390 runs the library take ("You pick up the rock.") — Scarier's exact behavior.  `wear rock` prints WFAIL in BOTH Runners (and in Scarier). | **Documented, not fixed** (probe `FM4` in run400; `make_39_fwprobe.py` in run390).  Scarier sides with run390 on take and with both Runners on wear; only the run400 take half diverges.  run400 also does NOT rewrite `take` to `get` (a passing `get * rock` task did not fire on `take rock` there, where Scarier's canonical-verb retry fires it).  Zero corpus impact; reconciling the run400 half would mean a version-gated rework of the take retry model, deferred until a game demands it.  **Passing-task complement probed in run390 2026-08-02** (`make_39_fwprobe.py` variant `p` — same four tasks, restrictions dropped): `take rock` → TAKEPASS., `wear rock` → WEARPASS., `put pill in cup` → PUTPASS., and the library never runs alongside (rock stays on the floor, `examine cup` shows no pill).  So run390's library-take-on-failing-task is a *restriction fallback*, not a take-family task bypass — a passing task claims all three verbs outright, and Scarier byte-matches the whole session on the same `.taf`.  The passing put cell also completes the put-family precedence picture on the 3.9 side: only a matched-but-**failing** task is demoted below the library put. |
| Zero-length always-restarting event | ~~re-armed each finish, so the checker fired at startup twice and then EVERY turn~~ **FIXED 2026-08-02** | fires its texts/TaskAffected exactly ONCE, at game start, and never restarts — **identical in run390 AND run400** (probed live: `make_39_fwprobe.py` variant `b` / probe `EV` in `make_arena_probe.py`, pill-starts-in-cup so the checker's restriction holds from turn one: one "PILLCHECK FIRED." appended to the opening room description, silence forever after; and IceCream's zero-length "Customer2" event played live in run400 prints its customer paragraph once, not per turn) | **Engine fixed** (`scevents.cpp`: a `Time1=Time2=0` event on the restart-immediately paths goes dormant after its first finish; the unprobed zero-length-with-delayed-restart shapes keep the old re-arm).  Two goldens were carrying the unfaithful per-turn firing and are re-blessed: `icecream` (the per-turn customer nag was Scarier-only) and `TheADRIFTProject` (the radioactive mix now lands on the `take slime` turn via TASK 40's action chain, byte-matching the author's 2004 transcript — the earlier "ticks before the command" reading of that turn was wrong, the event simply never re-fires).  Corpus 104/104 PASS.  **Superseded 2026-08-02 by the row below** — the remaining zero-length shapes were probed, and "goes dormant" turned out to be only one of the Runner's three answers. |
| Zero-length events, the whole shape table | ~~a `Time1=Time2=0` event finishes on the turn it starts, whatever started it, and (before the row above) re-armed for ever~~ **PORTED 2026-08-02** | **what starts the event decides what it does** — run400 has three distinct answers, all probed live: (1) **started at game load** (StarterType=1) → starts and finishes on turn 0, once; (2) **started off a clock** (a StarterType=2 delay, or a restart-after-delay countdown) → prints its StartText and then **parks**: it stays running for the rest of the game, its LookText appears in every later room description, and its FinishText and TaskAffected never run; (3) **started by its starter task** (StarterType=3) → starts *and* finishes on the trigger turn, once.  Restart is a fourth axis: RestartType=1 (immediately) really does start the event again — a second StartText, then it parks per (2) — while RestartType=2 (after delay) on an immediate or task starter goes quiet for good, with no LookText, so it is not sitting in a running state either. | **Probed live 2026-08-02** — `make_arena_probe.py` configs `EV2` (the three starter types, all zero-length, all RestartType=2), `EV3` (the same delayed start with a *non-zero* length, proving the generator's StartTime/EndTime layout and so that `EV2`'s silence is semantics, not a bad file), `EV4` (Del Sol's exact shape with all three texts — this is where the parking showed up: `G1 START.` on the delay turn, `G2 FINISH.` from the length-2 control the turn after, `G1 FINISH.` never, and `G1 LOOK.` still in the room description eight turns later), and `EV5` (all three starters *with* texts and affected tasks, which is what separated "restarts and parks" from "goes dormant": `H1 START. … H1 START.` at turn 0 and `H1 LOOK.` for ever after; `H2`/`H3` silent after their single firing).  **Engine ported** (`scevents.cpp`): `evt_is_zero_length()` names the shape; the ES_WAITING countdown path starts such an event without finishing it; ES_RUNNING leaves a parked event's clock alone instead of decrementing it negative; and the finish-time gate now covers RestartType=2 with starter 1 **or 3** (the starter-task cell used to re-fire the affected task *every turn* after its first trigger) while RestartType=1 is deliberately no longer gated, so it restarts and parks like the Runner.  Corpus: v4 104/104 PASS, a5 unaffected; one golden re-blessed — `TheADRIFTProject`, where the extra length roll on the restart shifts the RNG stream and Joshua's gum tastes of carpet instead of octopus.  Del Sol's EVENT 11 "physics distraction 3" is the corpus's only live instance of shape (2), and it is room-gated: the walkthrough is never in the physics room at turn 25, so no golden moved. |
| When immediate events start relative to the opening room description | ~~they start during the first tick, i.e. **after** the opening description: their StartText prints below it, and their LookText is missing from it~~ **FIXED 2026-08-02** | both Runners start them during load, **before** the description: the opening room text carries their LookText, their StartText is nowhere to be seen (printed into the pre-intro screen and cleared), and only the finish half lands under the description.  Probe `EV5` turn 0, run400: `A bare arena.  H1 LOOK.  H2 LOOK.  H1 FINISH.  H1 TASK.  H1 START.  …` against the old Scarier's `A bare arena.` / `H1 START.` / `H1 FINISH.` / `H1 TASK.` / … | **Probed live in BOTH Runners and FIXED 2026-08-02.**  The probe the old note asked for is `EV6` in `make_arena_probe.py` — a plain **length-3** immediate event carrying all three texts, so the zero-length parking model can't be confused with the start model.  run400 puts `K1 LOOK.` in the opening description, never prints `K1 START.`, and still finishes on the third command turn; `make_39_fwprobe.py` variant `e` gets the same answer from run390, so this is **not** a version split.  Ported in `scevents.cpp` as `evt_start_load_events()` / `evt_finish_load_events()`, called either side of the `DispFirstRoom` block in `scrunner.cpp`: the start half runs before the description (silent — `evt_start_event()` gained a `silent` flag — and +1 on the clock so the startup tick's decrement still lands on the rolled length), the finish half runs just before that tick, because a zero-length immediate event's FinishText/TaskAffected/RestartType=1 restart all land *below* the room text in `EV5`.  **Corpus exposure, measured** (`scdump.cpp`'s EVENT line now ends `texts=SLF`): 590 events in 75 of 121 games, 280 of them immediate-start across 49 games; **7 immediate events carry a LookText** (Shadowpeak, `The Town Of Azra` ×2 file copies, tq3) and **16 carry a StartText** (tq3, Azra ×2, adriftorama, Shadowpeak, Colony, yak_shaving, Main Course, Del Sol); 65 events in 19 games have a LookText at all.  Corpus 128/128 PASS after re-blessing: the direct fallout is Colony/Del Sol/Main Course (turn-0 StartText gone), Azra and villains_and_kings (LookText now inside the opening description), and the rest is RNG drift — rolling immediate-event lengths at load moves them *ahead of* `battle_start()`'s stamina rolls, which churns light_up, circus, melbourne_beach, alexis and Main Course's NPC walks.  That reordering is unverifiable by construction (the Runners seed from `Timer`, so their combat differs run to run — §1), and it is the order the load-start model implies; accepted deliberately. |
| Where an event's LookText sits **inside** the room block | ~~inside the description paragraph, before the object list and the character lines: `A bare arena.  K1 LOOK.` / `Also here is a rock.` / `Robot is here…`~~ **FIXED 2026-08-02** | **dead last, after everything**: `A bare arena.  Also here is a rock.  Robot is here, looking dangerous.  K1 LOOK.` | **Probed live 2026-08-02 (probes `EV7`/`EV8` in run400, run390 agreeing on the 3.9 twin) and FIXED the same day.**  The LookText loop in `lib_print_room_description()` (`sclibrar.cpp`) now runs *after* `lib_print_room_contents()`, joined on with a new `pf_buffer_join()` (`scprintf.cpp`): it removes the single terminating newline our section printers add, then separates with the Runner's two spaces unless the preceding text ends with an author's own break — so `Fetlar the overly fetid is here.  It is raining...` joins on one line, while a `<br>`-led LookText (CyberCow's night-time lines) still starts its own.  A buffer-length guard keeps the join from migrating LookText up onto the room name line when the room has no description or contents.  It also retired a small old wart: a LookText after a `<br>`-terminated description used to print with a stray leading two-space indent (Shadowpeak's `  It is raining...`).  Corpus fallout, all verified to be pure relocation before re-blessing: 14 rows across 10 games (Shadowpeak ×3, CyberCow ×2, villains_and_kings ×2, Azra, orient_express, screen_savers, secret_of_lost_world, ticket, tq3, JGrim); 127/127 PASS, a5 suite untouched, sanitizers clean. |
| Startup event tick, pre-3.9 games | ~~the startup `evt_tick_events()` and the `+1` on every StarterType 2 delay ran for every version, so a 3.7/3.8 delayed event of delay N started at load (N=1) or on turn N-1~~ **FIXED 2026-09-04** | run380/run370 tick events ONLY from the generaltasks tail (run380 `events()` 425094, run370 432538 -- one caller each; run390 42E90B / run400 449310 tstart call it after the opening viewroom): a delay-N event starts on turn N, immediates decrement first on turn 1 | **Measured live 2026-09-04** (run380 haunt.taf, `Adven_1_haunt.rtf`, 84/84 turns): both Weather (delay 1) and Wolves (delay 1) started on turn 1, not at load -- 40 divergent turns.  Gated `>= TAF_VERSION_390` in scrunner.cpp / scevents.cpp; seven 3.80 goldens re-blessed, wrecked re-pinned to seed 106.  See `notes/WINE-TRANSCRIPTS-TODO.md` haunt section. |
| Administrative turns, pre-3.9 games | ~~`score`, `turns`, `count`, `hint`, `help`, `about`, `clear`, `history`, `where` skipped the NPC/event tick for every version~~ **FIXED 2026-09-04** | run380 generaltasks tail (443160-44317E; run370 43C88D-43C8AB) runs `characters()` + `events()` after EVERY command that produced output unless the game has ended; only `opensave()` (save/restore/restart) and quit bypass it, and the turn counter (44F138) increments every command.  run390 (460D6C) sets flag 468219 only for history/score/count/information/end/turns | **Measured live 2026-09-04** (run380 haunt.taf turn 83): `score` was followed by the Weather FinishText and the clock line.  `lib_set_admin()` in sclibrar.cpp makes `is_admin` 3.90+ only.  Open: nothing has measured `hint`/`help`/`clear`/`where` under run390, which are not in its 468219 list. |
| Pre-parse verb rewrites (`take` -> `get` and friends), pre-3.9 games | ~~only the game's own SYNONYM table rewrote the input, so in a 3.80 game whose tasks say `get X` a typed `take X` fell through to the library ("You take the X.") while run380 ran the task~~ **FIXED 2026-09-04** | run380 generaltasks applies a whole-word rewrite table to the raw input BEFORE matching: `everything`->`all` 441C3F, `slap`->`hit` 441C50, `take`->`get` 441C61, `except`->`but` 441C72.  run370 (43B41F/43B430) has only the first two; run390 has no `change()` at all; run400 rewrites take->get only inside its get handler, after task matching.  So the take->get rewrite is **3.80-only**, and it cuts both ways: a task written `take picasso` never fires from a typed `take picasso` in run380 (great.taf turn 41, "You can't take the picasso.") | BUILTIN rewrite table in `scprintf.cpp` `pf_filter_input()`, applied after the game's SYNONYM pass, version-gated per row.  Measured in run380 on jb2000 (`Adven_1_jb2000c.rtf`: 22/22, 0 differences after the port), Crime_Adventure (`Adven_1_crime.rtf`, 0 differences) and great (`Adven_1_greatf.rtf`, 121/121 pre-chase, 0 differences); great line 41 re-derived to `steal picasso` |
| Object-ambiguity prompt `Which <term>.  <list>?`, pre-3.9 games | ~~the positional `%object%` matcher bound `take truck keys` to the truck keys silently~~ **PORTED 2026-09-04 for 3.7/3.8** | run380 calls co() for every object on every command (generaltasks 441D5D): term = Short if the command contains it else Alias, >1 present object answering to the term flags it unless the last word of the Prefix was typed; at the end of the turn (4431B0) the WHOLE output is replaced by `Which <term>.  <Short list>?` unless a task ran -- state changes stand (the keys ARE taken).  run370: co() only via therest() (run when nothing answered yet, 43C647) / insides(), end-of-turn 43C8D3 on the flag alone.  **run390 is different**: co() 43B6BC is only called from characters() 45ACD8 and sitstand() 444A04, takes/drops/wears have their own `Which X would you like to take/drop.  list?` (454CAD/4459BD/43C9D5), and the hangover run390 replay shows no prompt for `open cabinet` with two cabinets | `sclibrar.cpp` `lib_co_ambiguity_prompt()`, called from `run_main_loop()` after the tick when no task claimed the turn (`run_note_task_ran`), gated `< TAF_VERSION_390`.  A global port regressed six 3.90 goldens.  mikes cmd 27 now prints run380's exact line (`Adven_8_mikes.rtf`, `Adven_1_mikesb.rtf`); the 3.9 handler-scoped prompts and the take/drop/wear ones are NOT ported |
| An event's **length** when `Time1 ≠ Time2` | ~~`evt_start_event()` rolled `scr_randomint (time1, time2)`, **inclusive of both bounds**, so a `0..1` event was length 0 about half the time~~ **FIXED 2026-08-17**: `scr_randomint_exclusive()` (`scutils.cpp`) at the three event-timing call sites — the length roll in `evt_start_event()`, the restart-after-delay re-roll in `evt_finish_event()`, and the load-time StarterType=2 delay in `scgamest.cpp`.  It draws once even when `hi <= lo` (VB6 evaluates `Rnd` unconditionally), so every `Time1 == Time2` event keeps its exact stream position and the blast radius stays confined to games with real ranges | **exclusive upper bound, measured live in BOTH Runners**: on a `1..3` range every draw is 1 or 2, never 3 — the event length at load, the StarterType=2 start delay, and the restart-after-delay re-roll all alike.  The P-code's two `+1` sibling sites never surfaced in any event-timing family | **Settled live and PORTED 2026-08-17, the same day the question was raised** — §10 has the probe recipe and results (config `EL` in `make_arena_probe.py` for run400, `make_39_evlenprobe.py` for run390: 5 sessions, ~240 draws across three roll families, all in {1,2}), and the closure log has the corpus fallout.  The fix went in the *callers*, not `scr_randomint`, whose other call sites carry their own arbitrated semantics.  *Provenance* now wins on the default `SCR_SEED=1` and its `SCR_SEED=2` pin is off; the stream shift cost ten routes their wins — six re-pinned to new seeds, the three Shadowpeak variants re-derived by the documented recipe, Vampire and iqsfot re-derived by hand — and the suite is back to 260/260 PASS.  The a5 corpora never moved: the change touches v4 event code only. |
| Put-family precedence, 3.9 half | same port, ungated | **run390 agrees with run400**: `put pill in cup` with a matched-but-failing task runs the library put ("You put the pill inside the cup.", fail message suppressed) | **Verified live 2026-08-02** (`make_39_fwprobe.py`): the ungated port is faithful on both sides. |
| Which of the two container-listing styles a container gets | ~~postfixed ("*An umbrella is inside the umbrella stand.*") only for a **dynamic** container holding exactly one object, or one that is part of an NPC; everything else prefixed ("*Inside X is …*") — recorded in `lib_list_in_object()` as "frankly, a mystery"~~ **FIXED 2026-08-03** | purely a **count**: 1 or 2 contained objects → postfixed, 3+ → prefixed, with **no static-vs-dynamic test anywhere in the chain** | **Derived from run400.txt and confirmed against a real Runner transcript 2026-08-03**, while wiring *It's Easter, Peeps!* — see the dated note below. |
| `take <object lying loose in the room>` | ~~"You pick up the creme egg." in every version~~ **version-gated 2026-08-15** | pre-4.0 "You pick up the creme egg."; 4.0 "You **take** the creme egg." — both agree on the container case, "You take the lollipop from the newspaper rack." | **Settled live and PORTED 2026-08-15.** All four Runners probed on a bare take of a loose room object: run370 (`p37pos.taf`, `drop rock` / `take rock`) → "You pick up the small rock."; run390 (`p39held.taf`, `drop key` then `take key` / `get key` / `take all`) → "You pick up the key."; run400 (probe `TK`) → "You take the rock." for `take rock`, `get rock`, the literal verb `pick up rock`, and `take all` ("You take the rock, the box and the lamp."). run380 shows the pre-4.0 half of the same handler through its "nothing to pick up here" refusal, and a UTF-16LE literal census confirms "There is nothing worth taking here." exists **only** in run400.exe (0x183e4). The container branch and `drop` are byte-identical in both generations ("You take the rock from the box." / "You drop the rock, the box, the lamp and the gem."), so only the `parent == -1` template moved: `lib_is_version_400()` picks it in `sclibrar.cpp`. Corpus fallout: 71 goldens re-blessed, every changed line the take rewording and every changed game a 4.0-signature `.taf`; full suite green. |
| What `g` / `again` echoes and how a pronoun resolves | nothing — just the command's own output | the same, **unless** the Runner's *References in brackets* checkbox is ticked, and it is **off by default** | **Settled live 2026-08-15, NO divergence — the row was measured from a transcript made with a non-default checkbox.** run400 out of the box answers `g` with the repeated task's own text and nothing else, byte for byte what Scarier prints; replayed on `easter.taf` itself, the very game and turn the row was written from. The bracket line is a display option: Options → Display & Media → Appearance → **References in brackets**, which makes the Runner echo *what a reference resolved to* in the typed colour on its own line before the reply — `g` → `(hit pinata with umbrella)`, `drop it` → `(an umbrella)`. Ticking it reproduces the author's `EasterWalk.txt` exactly. The P-code agrees: `mdlSpreadTheLoad.Sub_20_62` walks back over consecutive `g`s (`0008A060`–`0008A089`, the history scan that makes `g g g` repeat the *original*), then builds `"(" & cmd & ")"` at `0008A0AD`–`0008A0BB` and prints it through `General.Sub_22_27` — all of it behind a byte flag tested `= 1` at `0008A095`, the setting loaded as `showbrackets` at `00074503`. Like its three neighbours on that tab (*Room names in descriptions*, *Prompt for typed commands*, *Auto pause long text*) the box **never persists**: `showbrackets=1` in the registry still comes up unticked, so no Runner session shows it unless a human ticks it that run. **Two words ported** the same day: the six the Runner takes are `!!`, `again`, `last`, `previous`, `!` and `g` (`00089FE2`), and all six are confirmed live — `previous` repeats an examine, `!!` repeats it again. Scarier had `again`/`g` and SCARE's own `!`-history; `last` and `previous` are now in the same table entry (`scrunner.cpp`). No golden moves. One wrinkle for whoever probes this next: the history scan skips only entries *identical* to the word just typed, so `last` typed straight after `previous` repeats the literal word `previous` and is refused — a probe that chains the synonyms measures nothing. |
| End-of-game score summary | ~~nothing — the transcript simply stops after the game's own ending text~~ **PORTED 2026-08-15** | prints a summary of its own after it: `You scored ` N ` out of the maximum ` M `!`, `That is ` P `% of the game!`, and — **on a win only** — either `Well done - you scored maximum points!` or `You finished ` (M−N) ` points short.` | **Settled live 2026-08-15 in run400 AND run390, then ported** (`task_print_end_game_summary()`, `sctasks.cpp`). The three questions this row asked are now pinned. *Rounding*: `Int(score * (100 / MaxScore))` — a floating divide, then truncate, so 3 of 8 prints **37%**, not 38. *Always printed?* In 4.0, no — the whole block is guarded on **`MaxScore > 0`**; with MaxScore 0 the win prints its banner and stops, not even the trailing blank line. **That guard is 4.0-only** — see the separate row below, added 2026-08-15 after run390 was caught printing the summary for a scoreless game. *What triggers the pair?* The **win branch alone**, and inside it `score = MaxScore` chooses "Well done" over "points short". The five cells, MaxScore 8 (probe `SC` / `SC0` in `make_arena_probe.py` for run400, `make_39_endprobe.py` for run390 — both agree line for line): win → WinText, the two lines, the pair, blank; lose → `Better luck next time.`, the two lines, blank; death → `I'm afraid you are dead!`, the two lines, blank; "just stop" (Var1=3) → **nothing at all**, no summary either; MaxScore 0 win → banner only. Not gated on 4.0: a UTF-16LE census puts every fragment in all four binaries, and run390 was driven live to confirm the code path, not just the strings. **3.7 and 3.8 have since been driven live too** (2026-08-15): rather than build synthetic probes, two short real corpus walkthroughs were replayed in the genuine Runners — `microwaveman.taf` (3.80, 9 commands) under run380 prints `You scored 100 out of the maximum 100!` / `That is 100% of the game!` / `Well done - you scored maximum points!`, and `castle.taf` (3.70, 17 commands) under run370 prints the same three lines at 50/50. Both match the blessed goldens exactly, so the 18 pre-3.9 goldens the port touched rest on measurement, not on a string census. *Replaying a short corpus game is cheaper than authoring a probe and is stronger evidence, because it exercises the shipped data path too — prefer it whenever the corpus has a short enough walkthrough.* Corpus: **134 goldens re-blessed** together with the score-notification row beneath. Independent corroboration came out of that blessing — five corpus games carry ALRs keyed on these exact literals, which only fire now: `panic.taf` (a **3.9** game) rewrites `You scored`→`You achieved`, `out of the maximum`→`points from a possible`, `of the game`→`of <i>'Panic</i>. I pray you'll play again.`, `points short`→`in deficit.` and `I'm afraid you are dead!`→`Even Hell hath no place for you...`, and `qui_a_tue_dana` / `enquete_a_hauts_risques` / `largo_winch` translate the whole summary into French. Authors do not write ALRs against strings their Runner never prints. |
| `(Your score has increased by N)` after a scoring turn | ~~printed whenever the game's `NoScoreNotify` global is clear, i.e. for almost every game~~ **PORTED 2026-08-15: starts off** | run370/380/390 **never** print it — the string is not in those binaries at all — and run400 prints it only when Options → High Scores/Scoring → **"Notify when score changes"** is ticked, which it is not by default | **Settled live 2026-08-15.** The Runner never reads the TAF's `NoScoreNotify`; the toggle is a persisted *user* preference, `GetSetting("ADRIFT", "Runner", "NotifyScore", CStr(False))` at startup (`0x00074276`) and `SaveSetting` in `notify_Click` (`0x000457A6`), read at the print site through `Form1.notify.Checked` (`0x0008D47B`). Measured both ways: neither run400 nor run390 said anything when the probe's `scpoint` task scored +3 on an ordinary (non-ending) turn, and the run400 menu shows the item unticked on a fresh prefix. `scgamest.cpp` now starts the flag FALSE and lets the `notify` command stand in for the menu item. This was **1613 lines removed** across 71 goldens — by far the larger half of the re-blessing. |
| Pre-3.9 loss and death messages | ~~prints `Better luck next time.` / `I'm afraid you are dead!` in every version~~ **death message version- and perspective-gated 2026-08-15** | `Better luck next time.` is genuinely **absent from run370 and run380** — it first appears in run390. The death line is a different story: run370/380/390 all carry the *pair* `I'm afraid ` + ` dead!` and assemble the sentence around a perspective word — **`"I'm afraid " & Ary(5) & " " & Ary(4) & " dead!"`** — where run400 holds `I'm afraid you are dead!` as one fixed literal and never varies it | **Settled live and PORTED 2026-08-15; the loss half closed as unreachable.** *Death.* The assembly appears twice in run390, in the EndGame printer at `0003F5C8` and again in `Form1.chardohit` at `00042B14`, so a Battle System death is worded like a task death. run400 has the finished sentence once, in `General.Sub_22_70` (`000523DC`), and `Battles.Sub_12_1` calls that very sub (`0004AE95`) — so 4.0 says "you are" even in the first person. (run400.exe still *contains* the `I'm afraid ` / ` dead!` pair, but P32Dasm shows no code referencing it: dead weight, which is why a census alone could not settle this.) Measured on the **same EndGame with only `Perspective` flipped**, one probe per Runner generation: `castle.taf` (3.70) with its `south` death task rewired to a trivially runnable `zdie` → run370 answers **`I'm afraid I am dead!`** at Perspective 0 and `I'm afraid you are dead!` at 1; `wrecked.taf` (3.80) task 42 rewired the same way → run380 gives the identical split, and Perspective **2** answers like 1, which `lib_get_perspective()`'s pre-4.0 clamp already covered; the synthetic `p39end` probe with line 9 flipped to `0` → run390 gives it too. Ported as `lib_get_death_message()` (`sclibrar.cpp`), used by both `task_print_end_game_message()` case 2 and `battle_kill()`. All five probes now match their Runner byte for byte. **Zero corpus movement**: `wrecked.taf` is the only pre-4.0 first-person game with `KillsPlayer` tasks and its walkthrough wins, so no golden changed. *Loss.* Nothing to port — the branch is **unreachable** for a pre-3.9 game. `sctafpar.cpp` synthesizes a pre-3.9 EndGame action from the task's boolean flags only, `KillsPlayer` → `(6,1,2,…)` and `WinGame` → `(6,1,0,…)` (`|V380_TASK:_Actions_|` ~2612, `|V370_TASK:_Actions_|` ~3035), so Var1 is only ever 0 or 2 and case 1 can never run. That agrees with the binaries, where `Better luck next time.` is absent from run370/run380 (UTF-16LE census). Corroboration that 3.9 *does* print the loss line: `druggy_lane.taf` (3.90) carries an ALR `Better luck next time.` → `Hope you do better next time.` The status-line `You are dead!` is a separate fixed literal, present unchanged in all four Runners and untouched by any of this. |
| Where a pre-4.0 EndGame task's WinText goes | ~~on its own line under the task's CompleteText, in every version~~ **PORTED 2026-08-15** | **pre-4.0** joins it onto the end of the task's text as **one** flowed paragraph — run390 `END scwin.  You have won.`, and run380 running `microwaveman.taf` gives `You win the game.You have destroyed Coffee Man...` — where run400 does put it on a line of its own | **Settled live and PORTED 2026-08-15.** `Form1.endmessage` composes the win as `out = out & WinText & vbCrLf` (`0005DDF8` — two `ImpAdLdI4`s, `ConcatStr`, then the `General.Sub_22_12` that returns vbCrLf), i.e. **no separator at all**, so where WinText lands is decided entirely by whether the turn's accumulated text is already terminated. That is the real version split: 4.0 terminates a task's text block and pre-4.0 does not. run400 finishing `ptbad.taf` prints `...a few more minutes of your time.` and `You Win! Yay!` on separate lines, and that task's CompleteText ends at the full stop with no trailing `<br>` (`taftool.py unpack`), so the break is the Runner's own. Ported as `pf_undo_auto_break()` (`scprintf.cpp`): `pf_buffer_paragraph_line()` now remembers the buffer position of a terminator **it** supplied, and the endgame message takes it back for a pre-4.0 game only. 35 pre-4.0 goldens re-flowed. |
| Empty `WinText` → `Congratulations!` | ~~falls back to printing `Congratulations!` as a line of game text~~ **FIXED 2026-08-15: prints nothing** | prints **nothing**; `Congratulations!` is a *status bar caption* | **Settled live and FIXED 2026-08-15.** In run400's P-code the literal is written to `Form1.StatusBar1` panel 1 at `0x000571AC`, immediately alongside `You are dead!` at `0x00057205`, and those are the only references either literal has. Driven live at both ends of the version range on games whose `Header/WinText` is empty: `TheAmulet.taf` (4.00) under run400 and `castle.taf` (3.70) under run370 both print the winning task's own CompleteText and then go straight to the score summary, with the location panel — not the transcript — switched to `Congratulations!`. `ECOD3.taf` (3.90) confirms the middle of the range. TheAmulet is the sharpest cell: its CompleteText *ends* with the author's own "Congratulations!", so Scarier printed the word **twice** where the Runner printed it once. Corpus: 79 `Congratulations!` lines removed across 87 goldens, and 19 rows in `run_v4_walkthroughs.sh` had been using that engine line as their **win marker** — every one now points at a line the game itself prints (its last authored line, or its `You scored N out of the maximum M!`). |
| Summary when `MaxScore` is 0 | ~~suppressed in every version~~ **version-gated 2026-08-15** | 4.0 suppresses it; **pre-4.0 prints it anyway**, reporting the scoreless game as `That is 100% of the game!` however many points were actually scored | **Settled live and PORTED 2026-08-15.** run400 stays silent (arena config `SC0`, and `TheAmulet.taf` whose MaxScore is 0), but run390 prints all three lines: `ECOD3.taf` (0 of 0) ends `You scored 0 out of the maximum 0!` / `That is 100% of the game!` / `Well done - you scored maximum points!`, and `chicago.taf` — 75 points banked against a MaxScore of 0 — ends `You scored 75 out of the maximum 0!` / `That is 100% of the game!` / `You finished -75 points short.`, so the 100% is **not** the score-equals-maximum case and the shortfall really does print negative. Note the `score` command disagrees with its own summary in the same binary: run390 answers `Your score is 65 out of a maximum of 0.  (0%)` — Scarier already matched that. Whether 3.7/3.8 behave like 3.9 is untested and **unexposed**: no pre-3.9 corpus game has a MaxScore of 0, so `sctasks.cpp` treats the whole pre-4.0 range alike. Corroboration from the re-blessing: `druggy_lane.taf` (3.90, MaxScore 0) carries ALRs on `You scored 0 out of the maximum 0!` and `That is 100% of the game!` and `Well done - you scored maximum points!` — three lines an author would never rewrite if their Runner did not print them — and `renuntio.taf` translates two of the three into Spanish. |
| When the end-of-game message is printed | ~~inline, at the moment the EndGame **action** runs, before the task's remaining actions~~ **PORTED 2026-08-15** | at the **end of the turn**: `Form1.checkx` calls `Form1.evaluate` for the whole turn and only then, if the gameover byte is set, `Form1.endmessage` (`0x0005C681`) | **Settled live and PORTED 2026-08-15.** `chicago.taf` task 23 is `ACT type=6` (EndGame, win) followed by `ACT type=4 v1=10` (+10 points). run390 ends the game reporting **75**; Scarier reported **65**, because it printed the summary at the action and then stopped. A scan of the corpus for tasks whose EndGame action is not the last one finds **42 tasks in 32 games**, most of them the same `[6, 4]` shape. The port holds the pending ending in game state (`pending_endgame` in `scgamest.h`, Var1 + 1, matching the Runner's own gameover byte) and emits it from `task_print_end_game_message()` at the end of the turn in `scrunner.cpp`; a second EndGame action in the same turn does not displace the first, because run400's EndGame handler tests the byte before writing it (`0x0008D621`). `Form1.endmessage` (`0x0005DDAC`) reads MaxScore and score for itself, which is why every score, variable and text change the rest of the turn makes now lands *before* the summary is composed. **31 goldens moved**, and the shape of the movement is itself the corroboration: 25 games change score and 18 of those land on precisely MaxScore (100/100, 300/300, 1000/1000, 200/200, 140/140) — authors put the final scoring action *behind* the EndGame action, which only reads right under the Runner's ordering. Three rows had been pinned to a now-superseded score and were re-pointed (`shadricks_travels` 50→100, `largo_winch` 96→97, `merry_murders` 125→135). Independently confirmed on a **live 3.80 replay**: `marooned.taf` (119 commands) under run380 ends with the winning task's AdditionalMessage `Congratulations, you are no longer Marooned!` and *then* the summary, at 80/140 — exactly the reordering the port produces, on a game whose golden had the two the other way round. |
| A trailing `AdditionalMessage` the Runner never prints | ~~prints it~~ **PORTED 2026-08-15: suppressed in 3.80 games when the turn's text already ends in two spaces** | **run380 only** drops it — run370, run390 and run400 all print it | **Settled live and PORTED 2026-08-15; the rule is a 3.80 typo, and the guess in the previous version of this row was wrong.** The Runners build a turn's output by concatenating onto one string with two spaces between the pieces, checking first whether the string already ends in a separator so an author's own trailing spaces are not doubled. run380 bundled that check into the *same* `If` as "is there a message at all" and put the append inside both — `If AdditionalMessage <> "" And Right(out, 2) <> "  " Then out = out & "  " & AdditionalMessage` at `0004D001`—`0004D074` — so the message is lost whenever what came before ended in two spaces. Its neighbours in the same sub are written correctly: the CompleteText (`0004CF26`) and the room description (`0004CFB4`) guard only the separator and append either way. run370 (`00041C60`) has no check at all; **3.90 and 4.00 moved the test into a sub of its own** — `Form1.pspace` at `0002C880` in run390, `General.Sub_22_58` at `0004A948` in run400, both `Right(s,2) <> "  " And Right(s,1) <> Chr(10) And Right(s,4) <> "<br>"` — which fixes it. `jb2000.taf` task 14 ends its CompleteText with 42 spaces, which is why its sign-off never appeared. Probes hijacking jb2000's four unrestricted tasks pin the rule down live in run380: **one** trailing space prints, two or more do not; a trailing tab prints; 42 trailing *dots* print, so it is not length; a 42-space run in the *middle* of the text prints; leading spaces on the AdditionalMessage itself change nothing; and switching the task's `ShowRoomDesc` on restores the message, because the description lands in between and the string no longer ends in spaces. Ported as `task_suppresses_additional_message()` (`sctasks.cpp`) over a new `pf_ends_with_double_space()` (`scprintf.cpp`) that ignores the auto-break Scarier adds and the Runner does not. Corpus movement: **three** goldens, all 3.80 — `jb2000` (the sign-off), `superliam` (`eat necko wafers` loses "Junior runs in and drinks some root beer.", re-measured live in run380) and `tra` (`feed the dog` loses "Your score has gone up by 5 for making friends."). |
| A task's `ShowRoomDesc` prints the room **name** | prints the room name as a heading, then the description | ~~prints the **description only**~~ **NOT A DIVERGENCE — the measurement was an artefact, closed 2026-08-15** | **Closed 2026-08-15 after the second cell.**  The missing heading had nothing to do with `ShowRoomDesc`: the Runner has an **Options -> Display & Media... -> Appearance -> "Room names in descriptions"** checkbox, it was **off in this Wine prefix at the time**, and while it is off the Runner prints **no room-name heading anywhere** — not after movement, not at game start, not on `look`, not on `ShowRoomDesc`.  Measured on `ptbad.taf` in run400: with the box unticked, `down` answers `You slip on a slinky that was not there.   The bottom of a pit. ...` with no `Pit` line, and the game's own opening runs the stairwell description straight on under the intro; with the box ticked, the same `down` answers `You slip on a slinky that was not there.` / **`Pit`** (bold) / `The bottom of a pit. ...`, i.e. the heading is printed by the `ShowRoomDesc` path exactly as `task_run_task_unrestricted()` does it.  The P-code agrees: `mdlSpreadTheLoad.Sub_20_64` (`000723EA`) gates the whole heading on that one byte -- `if (opt = 1 And mode = 0) { if (Len(out) > 0 And Right(out, 1) <> Chr(10)) out = out & vbCrLf; out = out & "<b>" & Rooms(r).Short & "</b>" & vbCrLf }` -- and the same byte is the bold gate in the `mode = 1` short form, so "bold room names" and "show room names" are one setting in this Runner.  Nothing to port.  Two things fall out of it, both recorded rather than ported: (a) **every live measurement taken in this Wine prefix before 2026-08-15 was taken with room names off** — check the box before measuring anything that involves a room block. **Corrected 2026-08-24:** the two claims struck above were wrong. The setting *is* persisted — `pfx/user.reg` now carries `"showshortroom"="1"` under `[Software\\VB and VBA Program Settings\\ADRIFT\\Runner]`, and run390's `m_showshortroom_Click` is a plain `SaveSetting` — and its **default is ON**, not off: run400's options loader `Proc_21_24_4747F8` (`run400.bas:89290`) reads it as `Proc_21_25_44AC08("showshortroom", 1)`, alongside `showbrackets` 1, `showgt` 0, `autopause` 1, `Sound` 1, `Graphics` 1, `Myfont` 0 (args are pushed in reverse, so the byte before the key is the default). The 2026-08-15 observation was of a prefix in which the box had been *unticked* and that untick had persisted — not of a default. Room names are on for every measurement taken since; (b) with the box ticked the Runner puts **one** newline before the heading (and none if the text already ended in one), where `lib_print_room_name()` opens a paragraph and so emits a blank line — that is the already-accepted "the Runner joins a turn's output into one paragraph, Scarier prints sections" divergence in section 3, not a new one.  Corpus exposure, now measurable because `SCR_DUMP_TASKS` prints `srd=`: **3447 tasks across 127 games carry a `ShowRoomDesc`** (4.00: 3147 in 83 games; 3.90: 250 in 34; 3.80: 33 in 8; 3.70: 17 in 2). |
| Whether a `some` prefix is normalized to `the`, and whose prefix the take-from-container message uses | ~~replaces any leading `a`/`an`/**`some`** with `the` in every version, on both nouns of the from-container take~~ **PORTED 2026-08-15** | **two separate version splits.** (i) The normalized prefix gained `some` in **3.9**: 3.7 and 3.8 replace `a`/`an` only and leave `some` exactly as authored. (ii) The **taken** object in the from-container take prints its **raw** prefix pre-4.0 and the normalized one in 4.0; the **container** is normalized in all versions, so it follows rule (i). | **Settled live and PORTED 2026-08-15**, on *one game carried across three .taf versions by the genuine ADRIFT generators*, so the data behind the three cells is provably identical. `gen400.exe` opens a **3.80** file directly and Saves As 4.00; `gen390.exe` does the same to 3.90; `taftool.py unpack` / the 3.x PRNG deobfuscator confirm the object's `Prefix` field comes out of both conversions byte-identical (`some`, `aluminum clothes`). Then `microwaveman.taf`: `take clothes` -> run380 `You pick up **some** aluminum clothes.`, run390 `You pick up **the** aluminum clothes.`, run400 `You take **the** aluminum clothes.` -- and `wear clothes` moves with it. **3.70 answers like 3.80**: a copy of `arlo.taf` (3.70) whose bone object had its `a` prefix rewritten to `some` and the file re-obfuscated gives run370 `You pick up **some** bone.` The other side of the same screenshot gives rule (ii): `take gun` out of the clothes prints run380 `You take **a** small pistol from **some** aluminum clothes.`, run390 `You take **a** small pistol from **the** aluminum clothes.`, run400 `You take **the** small pistol from **the** aluminum clothes.` Only the pistol's own prefix moves at 4.0. Two independent corroborations, on different games and a different person: run380 on `wrecked.taf` answers `get jacket` with `I take **a** tweed jacket from the bench.`, and run390 on our own `capacity_nest.taf` size probe answers the nested `take w1` with `You take **a** w1 from the m12.` -- that last one caught `capacity_nest_expected.txt` recording the wrong line, which is now fixed. Ported as a `>= TAF_VERSION_390` guard on the `some` arm of `lib_print_object_np()` and a `parent == -1 || lib_is_version_400 (game)` choice between `lib_print_object_np` and `lib_print_object` in the take handler (`sclibrar.cpp`). Corpus fallout: **41 walkthrough goldens** re-blessed (28 at 3.90, 13 at 3.80; no 4.00 or 3.70 golden moved) plus the size probe -- every diff line is one of the two rules and nothing else. |
| Blank lines between the ending text and the summary | ~~none: an empty `WinText` printed nothing at all, and the loss and death messages went straight under the ending text~~ **PORTED 2026-08-15** | the `vbCrLf` terminators are unconditional, so an **empty** WinText still costs a blank line, and both losing endings open with **two** of their own | **Settled live and PORTED 2026-08-15**, same root as the row above: the messages are concatenated onto the turn's text, which 4.0 has already terminated. Three measurements, all run400. *Win, empty WinText*: `microbe_willie.taf` leaves one blank line between the winning task's text and `You scored 7 out of the maximum 7!` — the `& vbCrLf` of `0005DDF8` fires whether or not there is a WinText. *Death*: `QuestI.taf` (task 3 `#Check for Death from Hunger`, `ACT type=6 v1=2`) leaves **three** line pitches — two blank lines — before `I'm afraid you are dead!`, which is `out & vbCrLf & vbCrLf & msg & vbCrLf` read literally (the lose branch inline at `0005DFDC`, the death branch in the shared `General.Sub_22_70` at `000523BF`, which is also where a Battle System death lands). *Pre-4.0*: run390 finishing `ECOD3.taf`, whose ending text ends `<br><br>` and whose WinText is empty, shows two blank lines before `You scored 0…` (36px line pitch, 108px gap) — the author's two breaks, with the Runner's own terminator ending the last of them rather than adding a third. 22 goldens gained a blank line. Note the v4 harness pipes transcripts through `cat -s`, so one blank line and two are indistinguishable in a golden: only the *presence* of a blank line is regression-tested, not its count. |
| TAF 3.8 object "Size/weight" class | **now modelled**: the class is kept verbatim in `SizeWeightClass` and enforced as a pooled burden (`obj_get_burden` / `obj_get_player_burden_limit`, `scobjcts.cpp`), while `SizeWeight` stays normalised to 4.0 "normal" (`22`) so a container's `Capacity*10+2` remains the plain **object count** 3.8 meant it to be (`\|V380_OBJECT:_SizeWeight_\|` in `sctafpar.cpp`) | **SETTLED against the genuine `run380.exe` 2026-08-03: a single pooled burden with per-class costs `0→1 1→3 2→7 3→3 4→7`, and a capacity of exactly `MaxCarried`.** Neither Scarier's normalisation (every class costs 1) nor gen390's `0→22 1→23 2→24 3→32 4→42` matches it. | **Divergence CONFIRMED, measured, and FIXED 2026-08-03.** `run380.exe` is *not* lost: David Whyld's dead delron.org.uk still serves `adrift38.zip` through the Wayback Machine, and it is installed in the adrift-battle Wine prefix (`~/adrift-battle/runner/wine/README.md`). Probe method: a 3.80 `.taf` is plaintext CRLF fields XOR'd with the VB6 PRNG from seed `0x00a09e86` — no length header, no zlib, no "Wild" trailer — so it round-trips losslessly through a codec, and a probe builder patches `#MaxCarried` (line after `$GameAuthor`), `#StartRoom` (line after the first `**`, a **direct 0-based room index**, unlike objects' `+3`) and any object's `#SizeWeight` (its short-name line **+11**). (The scripts used for *this* measurement — `dec38.py`, `mkprobe2.py` — were session scratch and were never committed; the surviving, better tools for the same job are `~/adrift-battle/runner/wine/taf38schema.py` and `make38probe.py`, which patch by field *name* rather than by hand-counted line offset. Use those.) Pinned at MaxCarried 1/2/3/6/7/8 in the real Runner: at 2 a class-1 or class-3 object is refused and a class-0 accepted, at 3 both go; at 6 Marooned's tires (class 4) are refused, at 7 they are accepted **alone**, at 8 tires+map fit and tires+flint+map do not, and tires + a class-2 gas can never do — which also disproves the two-axis reading, since a separate size and weight axis would have let the two heavies coexist. Cross-checked on the corpus's other 3.8 game: `Crime_Adventure.taf` (MaxCarried 5) refuses `get kettle` (class 2 = 7) **with empty hands** while its five class-0 kitchen items all fit, so that game's "Get all the stuff in Fenwick kitchen" hint is an author fault, not a conversion fault. gen390's table is therefore *directionally* right and wrong by one step — the 4.0 packed `base^digit` model cannot express 7, so the top class rounds to `3^2 = 9` against a limit of 8, which is exactly why converted 3.8 games stop being finishable. **The fix:** the object fixup now also writes `SizeWeightClass`, the globals fixup raises `Globals.BurdenModel`, and `scobjcts.cpp` gains the 1/3/7 cost table plus a `MaxCarried` limit. `sclibrar.cpp` routes every take through the pooled sum: `lib_object_too_large()` performs the check (3.8's only refusal is "Your hands are full.") and `lib_object_too_heavy()` stands down entirely, since 3.8 has no "too heavy" message. The 4.0 `MaxSize`/`MaxWt` pair is still written — the save serialiser reads it, and because every class costs ≥ 1 the pooled burden can never be looser than the object-count limit the size axis enforces, so those checks are subsumed and can never fire first. **3.8 containers measured 2026-08-03, closing the one gap this row left open.** Three answers, all from `run380` with probes patched through a real schema parser (`~/adrift-battle/runner/wine/taf38schema.py`, which walks `V380_PARSE_SCHEMA` and records each field's line index, so probes patch by *name*): (1) **a carried container's contents are free** — against `MaxCarried` 1, a class-0 box (cost 1) holding a class-4 object (cost 7) is picked up and only the *next* cost-1 object is refused, where charging the contents would have made the box alone cost 8; (2) **`Capacity` is a plain object count** and the Size/weight class is charged against it no more than against the carrier — a Capacity of 2 swallows a class-4 then a class-1, and two class-0 objects fill it; (3) a **`Capacity` of 0 is full from the start**, not unlimited. So Scarier's arithmetic on both axes was already right, and the normalisation to `22` is vindicated. What was wrong was the wording and one rule: 3.8 says a flat **"Your hands are full."** with no " at the moment" (at the time this was done by reporting 3.8 objects as unportable; superseded 2026-08-23, when the suffix turned out to be a SCARE invention no Runner has — see the size-refusal row below), it answers **"The box is full."** to *every* container refusal rather than 4.0's "too big to fit inside"/"can't fit inside … at the moment" pair (`lib_put_in_backend` consumes the leftovers under the burden model and prints it once), and it **only fills a dynamic container the player is holding** — "You are not holding a saucepan.", with the object's own prefix rather than the usual "the" (`lib_put_in_is_valid`). Static containers are exempt, which matters: `Wrecked`'s static red locker takes a coin where it stands, and nothing could ever pick one up. That last rule cost `Crime_Adventure` its route — its stew was loaded into a saucepan on the kitchen floor, which `run380` refuses **in that very game** (verified directly, not just in a synthetic probe) — so the route now takes the saucepan first and the later redundant `get saucepan` is gone; same 75/95 win, re-blessed. Corpus impact: the two 3.80 games with routes both needed new ones — `marooned`'s single-trip route broke at `get map` and is now a four-trip ferrying route, and `wrecked` needed two drops (`drop card`, `drop glass`) against its limit of 10; both re-blessed and PASSing, full corpus back to 163 PASS / exit 0. See `test/adrift4/notes/Marooned_walkthrough.md`. |
| A matched task whose restrictions FAIL swallows the command (no fall-through to movement) | prints the task's FailMessage and ends the turn, even when the FailMessage is a one-character placeholder and the room has an exit in that direction | **identical** | **Settled 2026-08-03, NO divergence** (`wrecked.taf`, TAF 3.80, via a gen390 conversion under Wine). Task 96 (`in pub with scuba` / `in`) is Campbell Wild's "you can't come in looking like that" blocker, and its FailMessage is the literal placeholder `x`; once the outfit is off, `run390.exe` prints just `x` and refuses entry, exactly as Scarier does. Task 84 at the Post Office roof (`climb *roof*`, alts `up` / `u` / `get *roof*` / `go *roof`, restricted on task 83 `climb *statue*` **not** done) has the same shape, and gen390 re-encodes its restriction byte-identically to our parse (`RESTR type=2 v1=84 v2=1`). Both are escapable only because `go in` and `go up` are absent from the tasks' command lists, so nothing matches and the movement runs — also confirmed live for `go in`. Nothing to fix; recorded so the next `x`-shaped mystery is not re-investigated. |

| ADRIFT 4 `$RestrMask` operator precedence | ~~C precedence: an OR-expression over AND-expressions, both left-associative~~ **now equal precedence, left-associative** | `A` and `O` have **equal** precedence and associate to the **LEFT**: `#O#A#` is `(1 OR 2) AND 3`, never `1 OR (2 AND 3)` | **Divergence found and FIXED 2026-08-03** (`screstrs.cpp`, `restr_expr()`). Ground truth is run400's own `mdlSpreadTheLoad.Sub_20_57` ("evaluaterestrictions", `00055CAC..00055EB9`): it scans the mask for the last top-level `A`/`O`, evaluates the tail operand, and recurses on the **head** — peeling from the right and recursing left is *left* association, and `A`/`O` are two arms of a single `If`, so there is no second precedence level anywhere in the routine. `Sub_20_58` ("evaluate2") has the same shape for its annotated T/F display string, and the driver `Sub_20_65` substitutes T/F for every restriction in index order with **no short circuit**. The old parse agrees whenever every `A` precedes every `O` at a bracket level and differs the moment an `O` comes first. **20 corpus games author a mixed level** (measured 2026-08-03 by a `maskscan.py` scratch script that is not committed — the list below *is* the result; to re-measure, scan every task's `$RestrMask` for a level containing both an `A` and an `O`): unauthorized (30 tasks), iqsfot (11), unravel (7), humbug (5), cursed (4), the_pk_girl / Vendetta / yonastoundingcastle / 3monkeys (3 each), EscapeToNewYork / The Plague - Redux (2), and one each in ARGH_sGreatEscape, DragonShrineR43, Glum Fiddle, Main Course, TheSisters, Trabula, mishmash, ticket. Found through *Three Monkeys One Cage*, whose author-written `winnable` self-check (T21, 55 restrictions — the corpus maximum) reported "no longer winnable" from turn 1: its group `#O(#A#)A#` is "(the bucket is on the hook OR the coconut is set up) AND the gate is still shut", which the C parse read as "bucket OR (coconut AND gate)" and so answered TRUE with the gate already open. Whole v4 suite re-run after the fix: **161/161 PASS, no golden moved.** A live run400 confirmation is still possible (`3monkeys.taf` is staged in the Wine prefix) but the P-code is unambiguous. |

| A task command typed **outside** the task's `Where` rooms | ~~no message of any kind: `task_can_run_task()` returned FALSE and the command fell through to the standard library or to "I don't understand."~~ **PORTED 2026-08-10** | **pre-4.0 only**: prints `You can't do that here.` (3.7/3.8) / `You can't do that here!` (3.9) and **consumes a turn**; run400 dropped the message and answers DontUnderstand | **Divergence measured live and FIXED 2026-08-10.** See §5's follow-up below for the probe and the full cell table. Condition is narrow: the task's command must match and the room list must be the *only* thing blocking it — a failing restriction gets DontUnderstand instead, and anything the standard library already handled wins outright. Implemented as `run_where_refusal()` (`scrunner.cpp`), last in `run_all_commands()`, over the new `task_is_room_refused()` predicate (`sctasks.cpp`). Leading word follows `Globals/Perspective`: "I" for first person, "You" otherwise (pre-4.0 has only the two — run390 says "You" for 1, 2 and 3 alike). **Zero corpus movement: 203/203 PASS, nothing re-blessed** — a solved route never types a task command in a room the task can't run in, which is why the feature carries its own synthetic regression (`make -f Makefile.headless wheretest`, two probe games covering both perspectives, part of `make test`). |
| A **completed non-repeatable** task, typed again by its own command, **empty RepeatText** | ~~"I don't understand." (or whatever the library says) — the matcher skips the done task and the input falls through~~ **PORTED 2026-08-10** | **pre-4.0 only**: `You have already done that.`, and it **consumes a turn**; run400 has no such string and answers DontUnderstand | **Divergence measured live and FIXED 2026-08-10.** Found alongside the `Where` refusal, on the same 3.90 probe (task `epsilon`, `Where` = all rooms, not repeatable, empty RepeatText, typed twice: run390 fires it, then answers "You have already done that." with the ticking event still ticking). The string is a UTF-16 literal in run370/run380/run390 and **absent from run400** — the same pre-4.0-only pattern as the row above; the leading space in ` have already done that.` proves it is concatenated with the perspective pronoun, exactly like the room refusal. Still distinct from the `*`-wildcard row further up: that one is about a done wildcard task *claiming every later command* and soft-locking `inverness`, and stays a deliberate divergence. This row is the narrow exact-command case, and the port keeps the distinction — the refusal runs last in `run_all_commands()`, so movement and library commands are answered first. See §5's follow-up. |
| A **completed non-repeatable** task, typed again by its own command, **non-empty RepeatText** | ~~"I don't understand." in every version — the done task never reached the RepeatText branch~~ **PORTED 2026-08-10** | prints the task's **RepeatText**, and it consumes a turn — in **every version, 4.0 included** | **NEW divergence, measured live 2026-08-10 and FIXED the same day.** run400's answer to `epsilon` (no RepeatText) is DontUnderstand but to a twin task *with* one is the RepeatText itself, so the RepeatText half of this behaviour survived into 4.0 even though the bare message did not — implemented ungated, with only the bare message gated on `version < TAF_VERSION_400`. The one part of this family with real corpus exposure: **62 of 196** v4 corpus games author at least one non-repeatable task with a RepeatText, **528 such tasks** in total (`SCR_DUMP_TASKS=1`, new `rpt=` column). No golden moved regardless — a solved route does not re-type a completed one-shot task. |
| Perspective 2 (third person) in a pre-4.0 game | ~~renders third person — inventory reads "Player is carrying nothing.", and the capacity probe narrates "Player puts the b1 inside the c52t."~~ **PORTED 2026-08-10** | **pre-4.0 has only two perspectives**: run390 answers second person for `Globals/Perspective` 1, 2 **and** 3 — "You are carrying nothing.", "You put the b1 inside the c52t." — and first person only for 0 | **Divergence observed live 2026-08-10 and FIXED the same day.** Noticed while pinning which word the `Where` refusal leads with (the refusal itself was already correct — it follows the same two-way split), and independently on the 3.9 capacity probe, which is authored Perspective 2. Ported as `lib_get_perspective()` (`sclibrar.cpp`), which returns `LIB_SECOND_PERSON` for any non-zero Perspective when `version < TAF_VERSION_400`; the two switch sites that render a person (`lib_select_response()`, `lib_nothing_happens_common()`) now read through it, so no third-person string had to be version-gated one by one. Out-of-range values reach the existing error branch for 4.0 only — pre-4.0 they are simply second person, which is what run390 does with 3. **Corpus census** (new `GAME version= perspective=` dump line): of 192 measurable games, **not one pre-4.0 game authors Perspective 2** — the three that do (*Main Course*, *iqsfot*, *yonastoundingcastle*) are all 4.0 and keep their third person, verified live ("SoMorph hits, but nothing happens."). So the 203 walkthrough goldens did not move. The one thing that did: the **capacity probe goldens**, 144 lines each of "Player picks up" → "You pick up" / "Player puts … inside" → "You put … inside" — moving them **onto** the run390 transcript recorded in §"container capacity", not away from it. |
| Do a task's remaining actions run after an action that ends the game? | ~~doc comment claimed "if any action ends the game, return immediately"~~ — the loop never did that: it **ran** the remaining actions with the print filter muted, and the doc comment was simply stale (fixed 2026-08-09). The one real divergence, **PORTED 2026-08-09**: a trailing Execute-Task action used to be dispatched too, so its callee's score and state changes landed. | **in-line actions after the ending still run** — a task whose actions are `end game` then `+7 points` finishes on 7 out of 7 — but an **Execute Task** action after the ending is a **no-op** | **Settled live 2026-08-09** with a new `EG` arena probe (`make_arena_probe.py`), read off run400's own end-of-game summary with `MaxScore=7` so a trailing `+7` shows as "100% of the game! / Well done" and a dropped one as "0%". One ending per session, so one Runner launch per cell: `scorefirst` (`+7`, `end`) → **7/7**, the control proving the summary reports the engine score; `scorelast` (`end`, `+7`) → **7/7** — *this is the answer: the trailing action runs*; `execlast` (`exec` a task that ends the game, then `+7`) → **7/7**, Three Monkeys' actual shape; `printfirst` (`exec` a task that prints and scores, then `end`) → **7/7** with the callee's text shown; `printlast` (`end`, then that same `exec`) → **0/7 with no text at all**, which is what pinned the divergence — the callee is demonstrably fine, so the Runner is dropping the dispatch, not muting it. Scarier agreed on the first four and awarded the 7 on the fifth, and all five cells now match. **Mechanism confirmed in the P-code 2026-08-09**, answering the question the probe alone could not — the Runner refuses the *dispatch*, not the callee's completion. The chain is `Sub_20_22` (RunTask) → `Sub_20_12` (mark complete) → `Sub_20_11` (run actions) → back to `Sub_20_22` for a type-5 action. **`mdlSpreadTheLoad.Sub_20_22` opens, at file offset `0005F750`, with `ImpAdLdUI1 <gameover> / CI2UI1 / LitI2_Byte 0 / GtI2 / BranchF / ExitProc`** — `If gameOver > 0 Then Exit Sub`, ahead of restrictions, CompleteText and actions alike. The type-5 branch of the action executor `Sub_20_11` (`@0008D588`, forwards arm `@0008D5AC`) calls it at `@0008D5D1` **unguarded**, bracketing the call only with stores of `0`/`1` to an unrelated global. The action *loop* has no gameover test at all: `Sub_20_11` reads that flag exactly **once**, at `@0008D621` inside the EndGame (type-6) handler, where it guards the ending *display* (`Sub_20_33`) against re-printing — which is precisely why in-line actions behind an ending still run. Same-variable proof: the flag is import slot `0x7E` in `mdlSpreadTheLoad` and `0x4F` in the form modules (P32Dasm does not print `ImpAd*` operands; read the 2 bytes after the `fd a0`/`fd b0` opcode at the listing address, which is a file offset into `run400.exe`), the load/store census splits perfectly along module lines, and the EndGame action's Var1→flag mapping (`0→1`, `1→3`, `2→2`, `3→4`, at `@08D694`/`@08D6AB`/`@08D6C2`/`@08D6D9`) matches `Form1.evaluate`'s reads of `0x4F` exactly (`1` = "Congratulations!", `2` or `4` = "You are dead!", `3` = "Game ended"). `Sub_20_2`, the NPC tick, carries the same `> 0 → Exit Sub` guard. **Ported structurally**: the guard now sits at the top of `task_run_task()` rather than on the type-5 action, mirroring the Runner; every other caller is already behind the main loop's `if (game->is_running)`, so this is a no-op for them. Whole v4 suite re-run after both the port and the move: **203/203 PASS, no golden moved.** **Verdict for *Three Monkeys One Cage*: "98/100 is the ceiling" is a fact about the game, not about our engine.** Task 603 is `exec 604` / `exec 608` / `player_moves--` / `player_score += 2`; the two callees are mutually exclusive and both end the game, but the `+2` is an **in-line** action, so it is credited in *both* engines — it is simply never displayable, because the game is over and this game's score is an author variable that only the `score` command ever prints. 98 is the highest score a player can ever *see*; the 100th point is banked in state at the instant of the win. |
| Article for an **empty `Prefix`** when an object is named | the two printers guess differently: `lib_print_object()` (sclibrar.cpp ~503) defaults an empty prefix to `"a "`, `lib_print_object_np()` (~428, empty-prefix branch ~472) defaults it to `"the "`. So the take message reads **`You take the Fenix de laton de el cajon`**. The `_np` default carries a standing `TODO This is empirical ... a real PITA` comment, i.e. it was guessed, not measured. | **identical — `the` on the take path, `a` on the description path.** Arbitrated by running the witness game itself in run400: `abrir cajon` → `Extendiendo mi mano abrì el cajon.  Encontrè un Fenix de laton dentro del cajon.`, then `coger fenix` → `You take the Fenix de laton de el cajon.` | **SETTLED 2026-08-14, NO divergence.** Both of Scarier's empty-`Prefix` defaults are what the real Runner does, and the two goldens' lines are byte-identical to run400's (`relojero_solution.expected.txt:66-67` and `:72`). The probe was the witness itself rather than a synthetic file — *La hija del relojero*'s 49-entry replacement table makes the article directly readable in the output, and the game is already staged in the prefix. Launched as `WINEPREFIX=$PWD/pfx WINEDLLOVERRIDES="mmdevapi=d" wine 'C:\adrift\run400.exe' 'C:\adrift\relojero.taf'` (audio off, then dismiss the "Cannot play sounds" dialog — see `wine-audio-desktop-softlock`). **So the author's `You take a` → `Con sumo cuidado cogi el` pair never fired in the real Runner either**: he wrote it speculatively, which is exactly the caveat this row flagged, and it is *not* evidence of an `a` default on the take path. His `A` → `Encontre un` pair, by contrast, fires in run400 — that is the `Encontrè un Fenix…` above — confirming `lib_print_object()`'s `"a "` default too. (A bonus tell that the table is a blind string replace, not a message lookup: the room description's `A mi lado hay…` comes out as `Encontrè un mi lado hay…` in run400 as well.) The standing `TODO This is empirical … a real PITA` comment in `sclibrar.cpp` can be retired: it is measured now. The 235 goldens riding on `"the "` are safe. (That settles the *empty*-prefix default only, and only for 4.0 — it is the one version where the take message normalizes both nouns. The **non-empty** case is the row above, settled and ported 2026-08-15: pre-3.9 leaves a `some` prefix alone, and pre-4.0 prints the *taken* object's prefix raw on the from-container path. Neither rule touches the empty-prefix defaults, which are still `"the "` for `_np` and `"a "` for the plain printer; what the pre-4.0 Runners do with an **empty** prefix on the from-container path is not measured, and no corpus golden exercises it.) |
| `getdynfromroom()` — when it runs, and what it picks | ~~a matcher: a separate pass over every task on every player command, evaluating the function in **any** command (primary included), firing the task itself when it matched~~ **PORTED 2026-08-17**: not a matcher at all — evaluated only as a preamble to running a task **by index**, over the task's **alternate** commands only | run400's `mdlSpreadTheLoad.Sub_20_22` (the by-index task runner: an *execute task* action, an event's TaskAffected, a walk's CharTask/ObjectTask, the battle system) evaluates the function before running the task; the typed-command matcher never does, and nothing fires spontaneously. Syntax: squeeze **all** spaces, require `#%object%=getdynfromroom(`, and require the **raw** command to end `)`. Argument compared case-insensitively to the room's `Short`; first **non-static** object standing directly in that room wins, in object order. run390 has no `getdynfromroom` at all (zero occurrences in its P-code) | **Settled live 2026-08-17 and PORTED** (probes `GD1`–`GDR` of `make_arena_probe.py`, driven in run400 under Wine; §9). `run_task_run_by_index()` (scrunner.cpp) is the new by-index runner and carries the preamble; `run_game_functions()` and the whole `is_normal == FALSE` matcher branch are **deleted**. Humbug is the only corpus user — its always-restarting one-turn EVENT 45 *robot in cellar* runs TASK 310 by index every turn, and the getdynfromroom in its `ALTCMD[1]` is what tells the robot which object to sweep down the chute; the faithful implementation reproduces the existing golden **byte for byte** (the old spontaneous pass had been firing the task a *second* time per turn, which the golden happened to survive). **deliberate:** two run400 fenceposts are not reproduced, both of which can only lose a match the author meant: (1) its room scan is `For r = 0 To roomCount - 1` over a 1-based array, so the game's **last room** is unreachable — proved live, `larder` as room 10 of 10 matched nothing and returned its pie the moment a spare 11th room was appended; its object loop has no such bug; (2) it squeezes the argument but compares against the **unsqueezed** room name, so no room whose name contains a space is reachable — not even by the manual's own worked example, `getdynfromroom(The Park)`. |
| Can a **task action** move a **static** object? | ~~yes — only the *by-index* object selector was limited to dynamics, so the "referenced object", "all held" and "all worn" selectors moved statics like anything else~~ **PORTED 2026-08-17** | **no, never, for any selector or any destination.** run400's object mover skips `Static = 1` before any destination case runs, so `grab plaque` (action: move the referenced object to "held by player") prints the task's CompleteText and moves nothing, and a move-to-hidden aimed at a static the player is *already* holding is refused the same way | **Settled live 2026-08-17 and PORTED** (`SM` probe of `make_arena_probe.py`, run400 under Wine; §9). `task_move_object()` (`sctasks.cpp`) now returns early for a static, next to the existing negative-index guard. The consequence is the answer to the other half of the same question: **`evt_move_object()` is the one and only place a static object moves**, which is what the `scevents.cpp` static-unmoved comment was asking. Corpus: 251/251 v4 walkthroughs and the whole headless suite unmoved. |
| What an object an **event** puts in the player's hands weighs | recomputed from object positions, like everything else: the object is listed by `inventory`, counts towards `count`/`weigh`, and can be dropped | **the Runner half-moves it.** `inventory` lists it, but nothing else treats it as held: `count` stays 0 and `drop` refuses it. The staleness runs the other way too — take an object by hand (`count` = 1), then let an event move it to another room, and it leaves the listing while still occupying its size indefinitely. Not a running counter: grabbing the same object twice with a task action leaves `count` at 1, so the totals *are* recomputed — over a held-ness the event mover never writes | **`deliberate:` divergence, measured live 2026-08-17, NOT ported** (`SM` probe, four run400 sessions; §9). Reproducing it would mean carrying a second held-state through game state and the `.tas` save format to inherit a bug that hands out free carrying capacity in one direction and confiscates it permanently in the other, with no authorial use either way. What *is* ported from the same measurement is the part with a defensible reading: statics contribute 0 size and 0 weight (`obj_get_size`/`obj_get_weight`), which is what run400 effectively does too, since a static can only ever reach the hand by the event route and so is never counted. |
| Which characters join the *"X, Y and Z are here."* sentence, and in what order | ~~only the ones whose in-room text is literally `#`, contributing their **`Name`**, and the sentence printed **after** the characters with texts of their own~~ **PORTED 2026-08-17**: every character whose text ends in the nine characters `" is here."`, contributing that text with the suffix trimmed off, and the sentence printed **first** | the `#` never reaches the lister: the **loader** (@00091EDF) rewrites the text to `<name> is here.` before the game starts, and the lister (@00072944) tests `Right(text, 9) = " is here."` — exact and case-sensitive, over whatever text is there. So an author who types the default out in full joins the sentence too, under the words *they* typed rather than the character's name, and a near miss of punctuation or case does not | **Settled live 2026-08-17 and PORTED** (probe `NH` of `make_arena_probe.py`, driven in run400 under Wine; §9). Eight characters in one room came out as `Alpha, Charlie, Delta and The stranger are here.  Bravo lurks in the corner.  Golf is here!  Hotel IS HERE.` — `Delta` (author-written `Delta is here.`) and `Foxtrot` (`The stranger is here.`) fold, `Golf is here!` and `Hotel IS HERE.` do not, and the character with an empty text is not mentioned at all. `lib_print_room_contents()` (sclibrar.cpp) now builds the joined list first, from both routes, and the custom-text loop skips what the list took. Rebless: 81 v4 goldens, all of it the fold, the reorder, and the line rewrapping they cause. **deliberate:** the joined sentence no longer opens with a blank line. It always had one, back when it could hold nothing but `#` characters and so always stood alone; now that it shares the block with the characters it belongs among, a blank line between them reads as an accident. The Runner has no line breaks here at all — the whole room block is one paragraph (§3) — so nothing is being contradicted, and one list to a line is what the rest of the block already does. |
| A walk's **`StoppingTask`**: does completing it pause the walk or end it? | ~~pause, and only that: the tick was skipped and the step counter left exactly where it stood, so un-completing the task carried the walk on mid-cycle~~ **PORTED 2026-08-17**: the walk is held at the **top** of its cycle for as long as the task is complete | neither reading is right. The walk is not finished — un-completing the task starts it moving again — but it does not resume where it stopped either: it runs a **fresh cycle**, arriving at stop 0 on the turn the task is un-completed | **Settled live 2026-08-17 and PORTED** (probe `S` of `make_400_walkprobe.py`, three run400 sessions under Wine; §9). A looping two-stop walk, `Times` 2 and 2, frozen at three different points of the cycle and released after three different waits, ends in the same state every time. Frozen away from stop 0, the release turn prints `RESUME TASK DONE.  Bob BOB ENTERS..` in one breath; frozen standing at stop 0, the release turn prints nothing at all and the next visible step is two turns later — the same fresh cycle, whose arrival at stop 0 is a move to where the walker already is. The middle session puts `stopit` on the turn the walk was due to move and it does not move, which places the check after the player's task in the turn and rules out a merely-frozen counter. `npc_tick_npc()` (scnpcs.cpp) now calls `npc_start_npc_walk()` on every stopped tick instead of skipping. 4.0 only, and not by choice: V390 has no *unset task* action (its action types 5 and up are V400's 6 and up), so a 3.9 stopping task can never be un-completed and the question cannot be posed there. No golden moved. |
| Does a **worn** object count against `MaxCarried` in the 3.7/3.8 pooled burden? | ~~yes — `lib_carried_burden()` summed everything the player held *or wore*~~ **FIXED 2026-08-23**: worn objects are skipped | **worn is free.** Against `MaxCarried` 2 with one wearable and two others, run370 and run380 both accept all three once the wearable is on, and `count` reports only the two held. Taking it off costs a slot again | **Was a real divergence, now fixed** (`pworn` and its 3.70 twin `qworn`). Wearing does not spend capacity and neither does keeping it on, so the pooled model charges for the hands only. Zero golden churn — no pre-3.9 walkthrough wears anything while at its carrying limit. |
| What `count` prints under the 3.8 pooled model | ~~the 4.0 shape with a `Burden:` label and a bare number: `Burden:\tYou have 0.  The most you can hold is 2.`~~ **FIXED 2026-08-23** | one unlabelled line counting **objects**, with no singular: `You have 0 objects.  The most you can hold is 2.`, and `I have 1 objects. …` in the first person | **Was a real divergence, now fixed** (`pcount1`, run380, both persons). The 4.0 two-line tab-stopped Size/Weight form has no pre-3.9 ancestor — there are no size and weight axes to report. No golden exercised `count` in a pre-3.9 game. |
| Which article the **wear** success message uses | ~~the definite printer in every version: `You put on the w0.`~~ **FIXED 2026-08-23**: pre-3.9 uses `lib_print_object`, 3.9 and 4.0 keep `lib_print_object_np` | **a version split at 3.9.** run370/run380 echo the object's own prefix through the *indefinite* printer — `You put on a rusty w3.` for prefix `a rusty`, `You put on a w1.` for an empty one — while run390 (`You put on the w0, the w1, and the shiny w2.`) and run400 (`You put on the w0.`) normalize | **Was a real divergence, now fixed** (`pwear`, one probe carried across all four Runners). The `a rusty` cell is what decides it: the definite printer would have said `the rusty w3`, so this is the plain printer and not a normalizer that defaults empty prefixes to `a`. The empty-prefix default is a bare `a` even before a vowel — `You put on a apple.` (`pwearv`, run380) — which is also what the pre-3.9 inventory `You are wearing …` list has always done, since that call site already used the indefinite printer. Five 3.80 goldens re-blessed: `wrecked`, `akron`, `cave`, `timmy_reid`, `life_of_mike`; every 3.9 and 4.0 golden held, which is the version gate's own regression test. |
| Does a 3.9 game keep run400's **leaky running carried total**? | ~~yes — Scarier kept `gs_carried_size`/`gs_carried_weight` for every version with the two axes, leaks included~~ **FIXED 2026-08-23**: `obj_uses_running_load()` is true for 4.0 alone, so 3.9 recomputes | **no.** `p39leak` in run390: take bag + take rock → 36, put rock in bag → **9**, drop bag → **0**, take/wear/drop cape → 9/9/**0**. run400 on the gen400 twin of the same file: 36/36/27/36/27/**18**. `p39lim` confirms the *check* moves with the report — after the put-and-drop run390 accepts a size-27 crate our running total refused | **Was a real divergence, now fixed.** run390 does keep the two globals but has no equivalent of run400's one generic setter `Proc_21_54`; it adjusts them from each command handler in turn and the arithmetic comes out exact, so recomputing is indistinguishable from it. 3.9 therefore also takes the position-filtered membership in `obj_weigh()` (no running total for a stale parent to poison) and `glk capacity` is a no-op for it. Zero golden churn from the model itself — no corpus walkthrough drops a loaded container or cycles a worn object against a tight enough limit. |
| Is the **size refusal** ever qualified with `" at the moment"`? | ~~yes, when empty hands would have taken the object (upstream SCARE's `is_portable` hedge)~~ **FIXED 2026-08-23**: never | **never, in any version.** `" hands are full."` is a single string with the period baked in — run370 @436D20, run380 @43E9A0, run390 @455B34, run400 @47C83C/@463C30/@46302C/@473A34 — and run390 answered a flat "Your hands are full." to the `p39lim` refusal we qualified | **Was a real divergence, now fixed.** The `is_portable` out-parameter is gone from `lib_object_too_large()` and `lib_object_too_heavy()`; nothing else consulted it. |
| The **weight refusal** wording | ~~one shape for every version, with a plural (`is`/`are`) and the `" at the moment"` hedge: `The lump is too heavy for you to carry.`~~ **FIXED 2026-08-23**: `lib_print_too_heavy()` prints the version's single shape | **two shapes, and neither hedges.** run390: **"That is too heavy for you to carry."** — no object name, no suffix. run400: **"The lump is too heavy for Player to carry at the moment."** — names the object, always qualifies. Measured on `p39wt` (MaxWt 30, a brick that fits in empty hands and a lump that does not): each Runner gave its one shape for both cases | **Was a real divergence, now fixed.** Neither binary contains an `" are too heavy"` string, so the plural SCARE selected between never existed either. Two 3.90 goldens moved on the same line, `alexis` and `alexis_worn_cube`. |

---

## 5. `Where` = "No rooms" on a player-typed task — SETTLED 2026-08-04, NO divergence

**Question.** ADRIFT 4's per-task `Where` field is a room list whose Type is
one of `ROOMLIST_NO_ROOMS = 0`, `ONE_ROOM = 1`, `SOME_ROOMS = 2`,
`ALL_ROOMS = 3` (`scprotos.h:215`). Scarier's
`task_can_run_task_directional()` (`sctasks.cpp`) returns FALSE for Type 0, so
such a task can never be matched against player input in any room. Does the
real Runner agree, or does it read 0 as "unrestricted"?

**Why it came up.** *The Plague - Redux* authors its entire `[F] Fight /
[E] Escape` combat system — seven blocks, one per zombie encounter — with
**every** task at Type 0 (243 of the game's 696 tasks are there), and nothing
`ExecTask`s the `[f]`/`[e]` pair. On Scarier's reading the fights cannot be
entered at all and the author's own walkthrough dead-ends at the first
mandatory fight. The same game also contains an obviously dead duplicate
movement task (`TASK 331 where=0 [* n *]`, no restrictions, moves the player to
room 6) parked between two live `where=1` movement tasks — which would hijack
every `n` in the game if Type 0 were runnable. Both cannot be true.

**Probe.** `test/adrift4/harness/make_400_whereprobe.py` (new) writes a minimal 4.0 plain body
with two rooms and three tasks:

| task | Where | expected if Type 0 is "no rooms" |
|---|---|---|
| `alpha` | Type 0 (No rooms) | refused |
| `beta`  | Type 3 (All rooms) | fires — proves the probe is wired |
| `gamma` | Type 1, room 2 only | refused — proves room scoping works |

```
python3 make_400_whereprobe.py p4WHERE.plain
python3 taftool.py pack p4WHERE.plain <donor>.taf p4WHERE.taf
```

(The donor supplies the 15-byte "Wild" password trailer run400 validates.)

**Result — `run400.exe` behaves identically to Scarier.** Typed in room 1:
`alpha` → the game's own "I don't understand.", `beta` → `BETA FIRED.`,
`gamma` → "I don't understand." **Type 0 really does mean "runnable nowhere".**

**Second, game-level confirmation.** A copy of *The Plague - Redux* with
`#StartRoom` patched `0` → `15` (Women's Toilets; line 80 of the unpacked plain
body, immediately after the `bd d0` separator at line 79) and repacked with
`taftool.py`, loaded in `run400.exe`, reaches the byte-identical cubicle scene
and answers `f` with "That didn't make any sense!" — same refusal as Scarier.

**Verdict.** No divergence; no code change. `Where`/Type 0 is the ADRIFT
authoring idiom for "disable this task", and two shipped games in the corpus
have been killed by using it by accident:

* **The Hangover** — `give the doctor some french fries` and `give approval
  notes to platypus` are both Type 0, confirmed against `run390.exe`; ceiling
  5/7.
* **The Plague - Redux** — the whole combat system; **unfinishable as shipped**.

Diagnostic worth keeping: when a walkthrough asks for a command the game flatly
does not understand, dump the task table and read `where=` before suspecting the
parser.

### Follow-up (2026-08-10): the two task refusals — "You can't do that here!" and "You have already done that." — PORTED

The probe above answered "is a Type 0 task runnable?" (no) but not "what does
the Runner *say* when a task command is typed in the wrong room?". run400 says
nothing special, which is why the original probe never noticed; the pre-4.0
Runners have a dedicated message. Both run390 and run400 carry the string
` can't do that here!` (VB6 UTF-16 — decode the .exe as `utf-16-le`, plain
`strings` misses it), and run370/run380 carry the period form
` can't do that here.`; only the pre-4.0 binaries ever print it.

**Probe.** `test/adrift4/harness/make_39_whereprobe.py` (new) writes a minimal
**3.90** plain body — XOR codec, no packing step needed — taking
`out.taf [perspective] [variant]`. Variant `e` adds a one-turn
always-restarting event printing `TICK.`, so a real turn is visible in the
transcript and "does the refusal consume a turn?" is answerable by eye. Tasks:

| task | shape | purpose |
|---|---|---|
| `alpha` | `Where` Type 0 (no rooms) | is Type 0 refused with the message or silently? |
| `beta` | Type 3 (all rooms) | control — must fire |
| `gamma` | Type 1, room 2 | the plain out-of-room case |
| `delta echo` | Type 2, room 2 | does Type 2 behave like Type 1? |
| `epsilon` | Type 3, not repeatable, empty RepeatText | second typing → "You have already done that." |
| `zeta` | Type 3, always-failing restriction, empty FailMessage | does a failing *restriction* raise the refusal? |
| `eta` | Type 3, not repeatable, RepeatText "ETA REPEAT." | does an authored RepeatText displace the message? |
| `theta` | Type 1, room 1, not repeatable | **both** blockers at once — which message wins? |

`make_400_whereprobe.py` gained the matching 4.0 pair, `delta` (not repeatable,
empty RepeatText) and `epsilon` (not repeatable, RepeatText
"EPSILON REPEAT."), to ask the same two questions of run400. The 3.9 probe's
two rooms are now joined north/south so `theta` can be typed from outside its
room after it has already been completed inside it.

**Measured**, run390 under Wine plus run370/run380/run400 on real corpus games:

| Runner | out-of-room task command | `Where`/Type 0 task | nonsense word | library-handled command | silently-failing restriction | done non-repeatable |
|---|---|---|---|---|---|---|
| run370 (castle.taf)      | "You can't do that here." | — | DontUnderstand | — | — | — |
| run380 (marooned.taf)    | "You can't do that here." | — | DontUnderstand | — | — | — |
| run390 (probe, hangover) | "You can't do that here!" (Types 1 **and** 2) | same refusal | DontUnderstand | library wins | DontUnderstand | "You have already done that." |
| run400 (4.0 probe)       | DontUnderstand | DontUnderstand | DontUnderstand | — | — | DontUnderstand |

So the condition is purely the room: pattern matched, and the `Where` list the
only blocker. Restrictions do not raise it, and anything the library already
answered suppresses it. Punctuation follows the period (3.7/3.8 `.`, 3.9 `!`),
the leading word follows `Globals/Perspective` ("I" for `LIB_FIRST_PERSON`,
"You" otherwise), and it counts as a turn — with the `TICK.` event running,
`gamma` prints the refusal *and* the tick where a nonsense word prints
DontUnderstand alone.

The already-done half measured the same way, and answered three further
questions:

| question | probe | answer |
|---|---|---|
| does an authored RepeatText displace the message? | `eta` twice | yes — run390 prints "ETA REPEAT.", not the refusal |
| is *that* half pre-4.0 too? | run400 `delta` vs `epsilon` | **no** — run400 answers DontUnderstand for the empty one but prints "EPSILON REPEAT." for the other, so RepeatText survived into 4.0 while the bare message did not |
| which blocker wins when both apply? | `theta`, completed, then typed from the next room | **the room**: "You can't do that here!", so the already-done test carries the room condition, not the other way round |

Both refusals consume a turn (the `TICK.` event proves it directly for 3.9;
4.0's RepeatText tick is inferred from its 3.9 twin, that probe having no
event). Perspective applies to the already-done message as well —
"I have already done that." under `LIB_FIRST_PERSON`.

**Ported** as `run_task_refusal()` (`scrunner.cpp`), called last in
`run_all_commands()` after `run_standard_commands()`, over two new predicates
in `sctasks.cpp` — `task_is_room_refused()` and `task_is_done_refused()`.
`task_can_run_task_directional()` was split into `task_state_allows_run()` /
`task_where_allows_run()` with the room half inverted, and the state half
factored further into cached `task_is_repeatable()` /
`task_repeattext_is_empty()` accessors. The scan checks the room half of each
task before its already-done half, which is what reproduces `theta`; the room
predicate deliberately does **not** consult the task state for the forwards
direction, or a task that is both done and out of its room would fall through
to DontUnderstand. An **empty input line returns early**, mirroring the
DontUnderstand fallback's own guard: without it, a game with a bare `*` task
command outside the player's room turns every press-a-key blank line into a
refusal (`archie_solution.txt`, whose first line is deliberately blank, caught
exactly that).

**Regression.** The corpus proved nothing here — 203/203 PASS with **no golden
re-blessed**, twice (once per refusal), because a solved route never types a
task command in a room the task cannot run in, nor re-types a completed
one-shot task. That is despite real static exposure for the RepeatText half:
**62 of 196** corpus games author at least one non-repeatable task with a
RepeatText, 528 tasks in all (`SCR_DUMP_TASKS=1`; `scdump.cpp`'s `TASK` line
gained an `rpt=` flag for exactly this count). Coverage is therefore
synthetic:
`make -f Makefile.headless wheretest` replays
`harness/where_refusal_script.txt` against the two generated probe games
(Perspective 1 and Perspective 0) and diffs against
`harness/where_refusal_expected.txt` / `..._1p_expected.txt`; it runs as part of
`make test` and under `make sanitize`.

**Known gap, accepted.** The 3.7/3.8 period wording is proved live (run370
*Castle Quest*, run380 *Marooned*) and gated on `version < TAF_VERSION_390`,
but has no synthetic regression — the generator writes 3.90 only, and the
V380/V370 GLOBAL, ROOM, OBJECT and TASK schemas differ enough to need a second
generator. The 3.7/3.8 corpus rows pass unchanged.

The sibling finding from the same probe run — pre-4.0 has only two
perspectives where Scarier rendered a third — was **ported on 2026-08-10 as
well** (§4). The probe is built a third time, at Perspective 2, and the script
types `i`: `where_refusal_3p_expected.txt` must stay **byte-identical** to
`where_refusal_expected.txt`, which is the regression. Nothing else about the
two refusals changed — they already took their leading word from the same
two-way split.


## 6. ADRIFT 3.70 — every inferred semantic measured, SETTLED 2026-08-04

**Question.** Scarier's new `V370_PARSE_SCHEMA` (`sctafpar.cpp`) was written by
diffing the two surviving 3.70 games against the 3.80 schema. Four layout
differences and one behaviour were *inferred* from those two files. Does the
genuine `run370.exe` agree?

**Probe method.** A 3.70 `.taf` is the same container as 3.80 — CRLF plaintext
XOR'd with the VB6 PRNG from seed `0x00a09e86`, indexed from offset 0, no
signature and no trailer — so it round-trips losslessly and the plaintext may
change length. `taf37schema.py` (`~/adrift-battle/runner/wine/`) is
`taf38schema.py` with the 3.70 TASK record and the trailing 17-word block,
and records each field's **line index**, so a probe is "parse, `L[idx] = value`,
re-encode". Six probes in `probes37/`, all patching `castle.taf`, all writing
into `pfx/drive_c/adrift/`. Turn Options → **Auto complete** off first or the
Runner rewrites the input box before the echo.

| probe | question | result |
|---|---|---|
| `mkprobe37f.py` | is the extra header integer the winning task? | **yes, 0-based.** Setting it to 0 makes task 0 (retyped as `test`) end the game with the victory text. |
| `mkprobe37.py` | what is the flat movement destination list? | **`0` hidden, `1` held by the player, `2` the player's room, `3+n` room n.** Moves two objects out of a distant room, so the answer cannot be confused with "already there". |
| `mkprobe37c.py` | what is the object initial-position list, and how far does it go? | **`0` hidden, `1` held by the player, `2` inside/on `#Parent`, `3..3+R-1` room n, `3+R` worn by the player.** Values past that leave the object out of play. |
| `mkprobe37b.py` | does `#Parent` pick the holder when an object starts held or worn? | **No — it is ignored.** Two objects set to worn with `#Parent` 0 and 1 both end up worn by the *player*. 3.7 cannot start an object on an NPC. |
| `mkprobe37d.py` | is the burden model 3.80's? | **Yes, identical**: pooled burden, class costs `0→1 1→3 2→7 3→3 4→7`, capacity exactly `#MaxCarried`, refusal `"Your hands are full."` |
| `mkprobe37e.py` | are the 17 renameable built-in command words replacements or additions? | **Additive.** Renaming slot 10 to `inspect` leaves `examine` working; slot 8 to `gaze` leaves `look` working. So the 4.0 synonym `{Original: author's word, Replacement: standard word}` is the right shape. |

**Two real bugs fell out of this**, both now fixed in `sctafpar.cpp`:

* `parse_fixup_v370_movement()` mapped "held by the player" to 4.0 `var3 = 1`,
  which `sctasks.cpp`'s `case 4` reads as the *referenced character*, not the
  player. Now 0.
* The shared 3.8/3.7 initial-positions fixup left `#Parent = -1` alone on a held
  or worn object, and `gs_create()` then computed `npc = -2`, raised
  `"object worn by nonexistent NPC, -2"` and hid the object. It now normalises
  an unset parent to the player. This also closes the long-standing `tra.taf`
  open lead from the 3.80 corpus smoke run: `i` there now answers
  "You are wearing an old red sox hat, and you are carrying some loose change.",
  byte-identical to `run380`.

**Verdict.** The schema is confirmed on every point that was guessed, and both
games load and play. `castle.taf`'s opening inventory now matches `run370`
exactly.


## 7. Whitespace between adjacent `[]` / `{}` groups — FIXED 2026-08-04 on
##    internal evidence, ARBITRATED LIVE the same day: NO divergence

**Question.** ADRIFT task command patterns routinely place two groups next to
each other with no space in the pattern text. Does the Runner require a space
in the *input* at that point, forbid one, or accept either?

**What Scarier did.** `uip_parse_list()` (`scparser.cpp`) interposed an
invented `NODE_WHITESPACE` between adjacent `NODE_CHOICE`/`NODE_OPTIONAL`
nodes, and `uip_match_whitespace()` demands a space — or, via two escape
hatches, a preceding space (a word boundary already crossed) or end-of-string.
So `[open/pull/push]{the}{wooden}[door]` matched "open door" and "open the
door" (each group boundary sits on a real space), but a pattern whose groups
build up **one word** could never match it.

**Why it came up.** *ImagiDroids* (`imagi.taf`, Woodfish) writes its exits as

```
TASK 38  {go/walk/move}[n/escape/out]{orth/out}       # n, north, escape, out
TASK  4  {move/run/walk/go/climb} {to/towards} {the} [d/out/in]{own}
TASK  6  [s]{outh}{ /-}[w]{est}
```

Only the bare `escape` / `out` forms worked, so the game's own shipped
walkthrough (`test/adrift4/downloaded/Imagidroids_walkthrough.txt`) could not get out of the
first room. *The Forum* (`forum.taf`) has the same shape in TASK 15,
`... {with}{the}{wooden}[clog]{s}` — "clogs" never matched, and the golden had
been blessed with the resulting "I don't understand what you want me to do with
the pair of wooden clogs." baked into it.

**The evidence for "either".** Two patterns in one game settle it in opposite
directions and only "optional" satisfies both:

* `[open/pull/push]{the}{wooden}[door]` has no spaces at all yet must accept
  "open door" — so a space between groups must be *allowed*.
* `[s]{outh}{ /-}[w]{est}` spells the space in "south west" out as an explicit
  `{ /-}` alternative — so adjacency alone must not *imply* one. An author who
  had an implicit separator would not write that group.

TASK 4's `[d/out/in]{own}` is the same argument in one line: the pattern carries
explicit spaces between its first three groups and none before `{own}`.

**Fix.** New `NODE_JOIN` node type, used only for the invented separator;
`uip_match_join()` eats whitespace if it is there and never fails. Explicit
whitespace written in the pattern still parses to `NODE_WHITESPACE` and keeps
the old strict-ish behaviour.

**Corpus effect.** Exactly one row of the 198 changed: `forum_solution.txt`,
where the second blow now lands and Ds is defeated with the full cure text
instead of a parser refusal — an independent game's published route repaired by
the same change, which is the strongest confirmation available short of the
Runner. Re-blessed; every other row is byte-identical.

- [x] **Arbitrated live — run400 agrees with the fix on every cell, and the
      NODE_WHITESPACE / NODE_JOIN distinction is real.** *(2026-08-04.)*
      `test/adrift4/harness/make_400_wsprobe.py` authors a one-room 4.0 probe with four
      repeatable all-rooms tasks — `[al]{pha}` → "JOIN FIRED.",
      `[be] {ta}` → "SPACE FIRED.", `[ga][mma]` → "CHOICE FIRED." and a bare
      `ping` control — packed with `taftool.py` and played in `run400.exe`
      under Wine (Auto complete off; every echo read back off the screenshot):

      | input | pattern | run400 | Scarier |
      |---|---|---|---|
      | `ping`   | `ping`      | PING FIRED.        | PING FIRED. |
      | `alpha`  | `[al]{pha}` | JOIN FIRED.        | JOIN FIRED. |
      | `al pha` | `[al]{pha}` | JOIN FIRED.        | JOIN FIRED. |
      | `al`     | `[al]{pha}` | JOIN FIRED.        | JOIN FIRED. |
      | `beta`   | `[be] {ta}` | **I don't understand.** | I don't understand. |
      | `be ta`  | `[be] {ta}` | SPACE FIRED.       | SPACE FIRED. |
      | `be`     | `[be] {ta}` | SPACE FIRED.       | SPACE FIRED. |
      | `gamma`  | `[ga][mma]` | CHOICE FIRED.      | CHOICE FIRED. |
      | `ga mma` | `[ga][mma]` | CHOICE FIRED.      | CHOICE FIRED. |

      So the two node types answer to two different rules and the collapse the
      item speculated about must **not** happen: adjacency accepts a space and
      does not require one (`NODE_JOIN`), while whitespace an author actually
      wrote is **required** in the input (`beta` fails) — the one exception
      being that a pattern ending in `space + optional group` still matches an
      input that stops before it (`be` fires), which is exactly
      `uip_match_whitespace()`'s end-of-string escape hatch. Choice/choice
      adjacency behaves like choice/optional adjacency. No code change; the
      internal-evidence fix and the ImagiDroids/Forum reading it was built on
      are confirmed by the Runner.

## 8. The 3.9/3.8 immediate-restart fixup — ARBITRATED LIVE 2026-08-04:
##    run390 re-arms **silently** and keeps the **full** period

**Question.** When a pre-4.0 event with `RestartType=1` (restart immediately)
finishes, does the Runner run its **start actions** again — StartText, Obj1
move — or does it silently re-arm?  And does the restarted event get its full
authored length, or one turn less?

**What Scarier did.** `evt_fixup_v390_v380_immediate_restart()`
(`scevents.cpp`) open-coded the restart for any taf below 4.0: state to
`ES_RUNNING`, clock to one less than a fresh length roll, and nothing else.
The comment inherited from SCARE said 3.9 and 3.8 "'miss' the event start
actions and move one step into the event without comment. It's arguable if
this is a feature or a bug."  Nobody had checked.

**The false start.** Earlier the same day this was "fixed" from a published
transcript: *Panic!* (`panic.taf`, Stewart J. McAbney, 3.90) builds its
cathedral atmosphere out of `RestartType=1`, `Time1=Time2=1` events, and the
author's walkthrough is a full session transcript in which the priest's cough
appears 66 times where Scarier printed it once.  The fixup was changed to call
`evt_start_event()` — printing StartText on every restart — keeping the
one-turn-short clock.  **Both halves of that are wrong**, and the transcript
was the wrong oracle.

**What the Runner actually does.**  Probe `test/adrift4/harness/make_39_evtimeprobe.py`
(self-packing V390: one room, one `ping` task, one event) against `run390.exe`
under Wine, and its 4.0 twin — config `EV9` in `make_arena_probe.py` — against
`run400.exe`:

| probe | shape | run390 says |
| --- | --- | --- |
| base | starter 1, restart 1, `Time1=Time2=5` | "E FINISH." on turns **5, 10, 15**; no "E START." on either restart |
| `b` | the same with `Time1=Time2=1` | FinishText every turn, StartText only on the very first start |
| `c` | starter 2 (3-turn delay), restart 1, time 5 | as the base: silent restarts |
| `e` | starter 3 (after `ping`), restart 1, time 1 | silent restarts |
| `f` | starter 3, StartText **+ LookText**, no FinishText — the exact "Priest Coughs" shape | one StartText at the trigger, then silence; the LookText only in an explicit `look` |
| `d` | starter 2, **restart 2** (after a delay), time 5 | StartText on **every** re-arm — that path goes back through `ES_WAITING` and the normal start |

So the period is the **full** authored length (5, not 4), and the immediate
restart is **silent** for all three starter types.  Only the delayed restart
re-runs the start actions.

`run400.exe` on the same event in a 4.0 taf prints "E FINISH.  E START."
every 5 turns — the version gate on the text is real, the one on the timing is
not.  (Also established: run400 refuses a 3.9 taf outright, "Incorrect
version", so a 3.9 game cannot be checked against the 4.0 Runner at all.)

**The published transcript disagrees with the Runner on its own file.**
Playing `panic.taf` in `run390` reproduces the walkthrough's hymnbook prose
line for line but prints the cough StartText **once**, not 66 times — and
`run400` cannot load the file.  Whatever produced that transcript, it was not
a Runner that ships this behaviour.  The dump also undercuts the argument that
was built on it: EVENT 15 "Priest Coughs" is `texts=SL-`, not `texts=S--`, so
its per-turn line could have been a LookText all along (variant `f` says it is
not printed per turn either way).  Only EVENT 1 (Muttering Priest), EVENT 4
(Stigmata) and EVENT 13 (Ghost Shimmers) are `texts=S--`.

**Fix.** `evt_fixup_v390_v380_immediate_restart()` now calls
`evt_start_event (game, event, TRUE)` — a silent re-arm, `silent` suppressing
the StartText alone — and does **not** touch the clock.  The length roll comes
from `evt_start_event()` alone; the fixup's own `scr_randomint()` was removed
in the first pass and stays removed, because two rolls where the Runner has
one churns the RNG stream.  Obj1's move and the start resource on a 3.9
restart are *not* measured; what the probes pin down is the text.

**Corpus effect.** 24 of 203 rows moved and were re-blessed; three winning
routes needed re-deriving, all three because the cadence of the RNG changed,
none because the route logic did:

* **circus** — `SCR_SEED=17` → `SCR_SEED=12` (swept 1..30).
* **haunt** — dropped the redundant `look in umbrella stand` turn; the next
  `take ticket` reveals the ticket anyway, and the spare turn now lets the
  wolf catch up.
* **thetest_win** — re-derived by the new `test/adrift4/harness/thetest_rederive.py`.  Its
  three try-until-it-happens blocks (`unlock door` until the colour-changing
  key matches twice, six `shout <triangle number>` blocks until the Robot
  Guard is in the room, `teleport` until the Morse Room) are pure dice-rolling
  pads, so the script replays the prefix and grows each block until its marker
  appears: 333 commands → 175.  *(Note for that game specifically: `#` comment
  lines are not free — thetest has keypress waits, and `os_ansi.cpp` only
  skips comments at a line prompt, so a comment gets eaten as a keypress.)*

Everything else is either a StartText line that no longer repeats (panic's
cough and wraith, twilight's apparition, timmy_reid's Billy, tq3's
rattlesnakes, alices_restaurant's whole station paragraph,
secret_of_lost_world's rain and volcano, marooned's rescue ship, wrecked's
"It is raining", enquete's hijack announcement) or a battle/NPC line that
moved with the RNG stream (alexis, spirits_flight, fantasyworld, troll,
inverness, melbourne_beach, phoenix_destiny, fugitive).  No win marker was
lost.

- [x] **Arbitrate the timing half** — done, above: the restarted period is the
      full authored length, and the restart is silent.  Both halves of the
      SCARE-era comment were wrong, in opposite directions.
- [x] **The residual — event visibility while on or inside an object.**
      Probed and refuted.  `test/adrift4/harness/make_39_evseeprobe.py` authors a V390 file
      with one room, a `Where`-limited-to-that-room always-restarting one-turn
      event carrying all three texts, and two statics with `SitLie = 3`: a
      surface (`chair`) and a container (`crate`).  In run390 the FinishText
      prints on **every** turn — sitting on the chair, sitting in the crate,
      and standing on the floor alike — and the LookText still appears in a
      `look` taken while parented.  So posture is not part of the Runner's
      event visibility test, and `evt_can_see_event()`'s room-only check is
      right as it stands.  Scarier's own output on the probe is identical.

      The 24-vs-21 count that raised this does not survive either.  It came
      from the transcript this section just showed disagrees with run390 on
      this game, and with the restart behaviour corrected both the wraith and
      the cough now print **once** in our run, so there is nothing left to
      compare.  The premise was wrong twice over: in Scarier's run of the
      route the rope throw *fails* ("you are unlucky in your endeavour") and
      `u` answers "You can't go in that direction (at present)", so the player
      is never up the statue on those three turns at all — they are turns
      spent failing the climb, an RNG divergence from the published run, not
      a visibility one.

## 9. Probe backlog (2026-08-17) — the code-comment TODOs not yet taken to a
##    Runner.  All five closed the same day; the last one on corpus data.

The 2026-08-17 TODO sweep resolved what it could without a Runner session
(the "affective weapon!" string was settled from the four Runners' string
tables — all of run370–run400 say `I don't think X would be a very affective
weapon!`, now ported; three corpus-settled question-comments were retired in
place). The five below were the questions that remained; all five closed on
2026-08-17, four in front of a running Runner and the last in front of the
corpus. None was believed to have corpus exposure — that is why they were still
open — and two of them turned out to have it after all. (`getdynfromroom` has
exactly one user, Humbug: the "no corpus exposure" reading came from grepping
for *execute task* actions targeting the task and forgetting that an event's
`TaskAffected` runs a task by index too. The negative resource lengths were
mis-parsing a game's pictures.)

- [x] **Static objects moved into the inventory** — **SETTLED LIVE AND
      PORTED 2026-08-17.** Both code-comment TODOs are retired. Three answers,
      all measured in run400 with the `SM` arena probe (a coin, a static
      plaque, events that move either one to the player's hands or to the far
      room, and two tasks whose actions move the *referenced* object):
      1. **Zero was right.** An event-placed object contributes nothing to the
         player's `count` totals — not just statics, the dynamic coin too — so
         a static, which can arrive no other way, never weighs anything.
         `obj_get_size`/`obj_get_weight` keep their 0 and now say why.
      2. **A task action cannot move a static, by any selector.** run400's
         mover skips `Static = 1` before any destination case, so the refusal
         is total: `grab plaque` (action = move referenced object to "held by
         player") prints its completion text and moves nothing, and a
         move-to-hidden aimed at a static the player is *already* holding is
         refused the same way. Scarier moved statics happily — only the
         by-index selector was limited to dynamics — so `task_move_object()`
         now carries the same test. **This makes `evt_move_object()` the one
         and only place a static moves**, which is the answer the
         `scevents.cpp` comment was asking for.
      3. `deliberate:` **the event mover's "held by player" is a half-move,
         and Scarier does not reproduce that.** run400's `inventory` lists the
         object but nothing else treats it as held: `count` stays 0, and
         `drop` refuses it. The stale state cuts the other way too — take the
         coin by hand (`count` = 1), then let an event move it to another
         room, and it vanishes from the listing while still occupying its size
         forever. The totals are *not* a running counter: grabbing the same
         object twice with a task action leaves `count` at 1, so the summer
         recomputes, over a held-ness that the event mover simply never
         writes. Scarier recomputes from object positions and is
         self-consistent — the event-held coin weighs 1 and can be dropped.
         Porting the Runner's split would mean carrying a second held-state
         through the save format to reproduce a bug that both grants free
         carrying capacity and permanently confiscates it.
- [x] **`#` in-room text for NPCs** — **SETTLED LIVE AND PORTED 2026-08-17.**
      See the §4 row and the batch note at the end of this file. The `#` is
      not a marker the room lister ever sees: the **loader** (@00091EDF)
      replaces the whole text with `<name> is here.` before the game starts,
      and what the lister (@00072944) tests is the *tail* —
      `Right(text, 9) = " is here."`. Three consequences, all measured, all
      ported:
      1. A character whose text the **author** wrote out in full joins the
         same sentence, contributing that text **minus the nine characters**
         rather than its own name — probe NPC `Foxtrot`, text
         `The stranger is here.`, comes out as *The stranger*.
      2. The test is exact and **case-sensitive**: `Golf is here!` and
         `Hotel IS HERE.` both stay in the second group and print their own
         text verbatim.
      3. The joined sentence comes **first**, ahead of the characters with
         something of their own to say — Scarier had the two groups in the
         opposite order. Empty text still drops the character entirely.
- [x] **Walk `StoppingTask` = pause or finish?** — **SETTLED LIVE AND PORTED
      2026-08-17.** Neither, quite. See the §4 row and the batch note at the
      end of this file. A completed stopping task holds the walk at the **top
      of its cycle**: un-completing the task does start it moving again (so
      not "finish"), but it does not carry on from where it froze (so not
      "pause" either) — it runs a **fresh cycle**, arriving at stop 0 on the
      very turn the task is un-completed. Three run400 sessions over probe
      `S` of `make_400_walkprobe.py`, freezing the walk at three different
      points of the cycle, all end in the same state. Ported as a one-line
      re-arm (`npc_start_npc_walk()`) in place of the old bare `continue`; no
      golden moved. Only 4.0 can pose the question: V390 has no "unset task"
      action at all (its action types 5+ are V400's 6+), so a 3.9 stopping
      task, once complete, is complete for good.
- [x] **`getdynfromroom` selection criteria** — **SETTLED LIVE AND PORTED
      2026-08-17.** See the §4 row and the batch note at the end of this file.
      The two code-comment TODOs are retired; the question turned out to be
      the smaller half of the answer, because the function is not a matcher
      at all. run400 evaluates it only as a preamble to running a task **by
      index** (`mdlSpreadTheLoad.Sub_20_22`), over that task's **alternate**
      commands only, and Scarier's separate "scan every task on every player
      command and fire it" pass — `run_game_functions()` — had no counterpart
      in the Runner and is gone. Selection: squeeze all spaces, require
      `#%object%=getdynfromroom(` and a **raw** trailing `)`, compare the
      argument case-insensitively to the room `Short`, take the first
      non-static object directly in that room. 4.0 only (run390's P-code has
      not one occurrence). Two run400 fenceposts deliberately not
      reproduced: the last room of the game is unreachable, and no room whose
      name contains a space is reachable. Humbug — the one corpus user, via
      EVENT 45's by-index run of TASK 310 — reproduces its golden byte for
      byte.
- [x] **Negative resource lengths in 4.0 TAFs** — **DECODED AND FIXED
      2026-08-17.** See the batch note at the end of this file. It is a
      back-reference and nothing else: `-N` means *entry N of the game's
      resource table*, counting from one over distinct resource **names** in
      the order the parse meets them, with the trailing `##` looping flag
      stripped first and with named-but-unembedded (zero-length) resources
      counted just the same as embedded ones. The rule reproduces all **535**
      negative records in the 427-game corpus exactly — the old
      `-(length+2)` guess had missed both of the reasons the count slips.
      Settled from corpus data rather than a Runner session, as the item
      itself predicted, and cross-checked against the files' own magic bytes.
      Scarier resolves back-references by name, which reaches the same entry,
      so the decode changes no behaviour by itself — but it did expose a live
      bug at the one place where name and index part company, and that is
      fixed: **472/472** embedded resources in the corpus now land on their
      own first byte.

## 10. The event-length roll when `Time1 ≠ Time2` (raised, measured and ported 2026-08-17)

**CLOSED the day it was raised** — probed live in both Runners, exclusive
upper bound everywhere, ported at the three event-timing call sites; the
measured answer and the corpus fallout are in the checklist results below and
in the closure log. The narrative that follows is the state of knowledge at
the moment the question was raised, kept as the record of *why* the probes
took the shape they did. `evt_start_event()` (`scevents.cpp`) set an event's
length with

```c
gs_set_event_time (game, event, scr_randomint (time1, time2));
```

and `scr_randomint` is **inclusive of both bounds**, so a `Time1=0 Time2=1`
event is length 0 on about half the seeds.

Nothing in this file has ever measured that, because **every** event-timing
probe written so far — all of §4's zero-length and starter-type rows,
`make_39_evtimeprobe.py` in its entirety, and the `EV*` configs of
`make_arena_probe.py` — sets `Time1 == Time2`, where the range semantics are
invisible. The question stayed hidden until a corpus game landed on it.

**What the P-code already says.** run400 has no inclusive two-field range roll
anywhere near the event machinery. Every `Rnd` site in the binary that rolls
between two *record fields* compiles to

```
FMemLdI2 lo ; CR8I2 ; Rnd ; FMemLdI2 hi ; FMemLdI2 lo ; SubI2 ; MulR8 ;
FnIntR8 ; AddR8            ' = lo + Int(Rnd * (hi - lo))
```

— an **exclusive** upper bound. There are sixteen such sites; the four in the
event/timer code (`0006FD44` and `000705E3`, both immediately after a timer
countdown reaches zero and both in `mdlSpreadTheLoad` territory, plus `0008F48E`
and `000920B1` on the file-reading path) all take exactly that shape, and two
siblings (`0006FE23`, `00091628`, on restart and load branches) take it with a
further `+ 1` *outside* the `Int`, i.e. a `[lo+1, hi]` range. The battle
attribute rolls agree (`Battles.bas:658/677`). The **only** inclusive form in
the whole binary is `Express.bas:648` — `Int(Rnd * ((hi - lo) + 1)) + lo` — and
that is the author-facing `rand(x,y)` *expression* function, a different thing
with its own §4-settled semantics.

So the shape of the answer is no longer in doubt: `scr_randomint(time1, time2)`
is wrong for event lengths in at least one of the two possible ways. What is
still open is **which field each site rolls** — the dump prints no operand for
`FMemLdI2`, so `Time1/Time2` (length) and `StartTime/EndTime` (the StarterType=2
delay) cannot be told apart by reading, and one of the two families carries that
extra `+1`. That is what the probe is for.

**The one data point.** *Provenance* (`test/adrift4/`) has EVENT 7: immediate
starter, `Time1=0 Time2=1`, TaskAffected `#Run Gender Task`, whose `Where` list
is the two rooms the player has left by the end of turn 1. So the roll is
directly observable from the player's clothing:

- length 0 → the event finishes inside `evt_finish_load_events()`, the `Where`
  check passes, and turn 1's `i` says *You are wearing a brown tweed suit*;
- length 1 → it comes due at the end of turn 1, from a room the `Where` list
  does not cover, and the suit is never worn.

Four live run400 sessions on the shipped `provenance.taf` all show the suit.
That is 4/4 for length 0 — consistent with the exclusive reading, and possible
but unlikely (1 in 16) under the inclusive one. The walkthrough row pins
`SCR_SEED=2`, a seed that happens to roll 0, and the reasoning is written up in
`test/adrift4/notes/Provenance_walkthrough.md`.

**Why it is not simply fixed.** The event-length roll is on the shared RNG
stream, so narrowing the range changes the *sequence*, not just this draw:
every later `scr_randomint` in every game with a random-length event shifts.
That is a corpus-wide re-bless, and the P-code above narrows the answer without
pinning it — a fix that is exclusive where the Runner is exclusive-plus-one is
no better than the bug. Measure first.

- [x] **Probe the range semantics in run400** — **DONE 2026-08-17**, as config
      `EL` in `make_arena_probe.py`, which authors all three roll families in
      one file so a single 24×`z` session measures them together: E1 immediate
      + restart-immediately, `Time1=1 Time2=3` (successive-finish gaps = fresh
      length rolls); E2 delayed start `1..1` + restart-after-the-same-delay,
      `Time1=1 Time2=3` (start→finish spans = fresh length rolls); E3 delayed
      start `1..3`, `Time1=Time2=1` (successive-start gaps = fresh **delay**
      rolls). The `1..3` range separates all three candidate readings by
      value alone: exclusive-hi → {1,2}, inclusive → {1,2,3}, the `+1` form →
      {2,3}. Three run400 sessions (s1–s3, ~145 draws): **every draw in
      {1,2}, both values abundant in every family** — exclusive upper bound
      for the event length, the StarterType=2 delay, and the restart re-roll
      alike, and the `+1` sibling sites belong to neither family.
- [x] **Then run390** — **DONE the same day**, as `make_39_evlenprobe.py`, the
      V390 twin of `EL` (same three events; its docstring carries both
      Runners' numbers). Two sessions (r1/r2, ~95 draws): identical — every
      draw in {1,2}, never a 3, in all three families. No version split.
- [x] **Only then decide.** **PORTED 2026-08-17**: the upper bound is
      exclusive, so `scr_randomint_exclusive()` went into `scutils.cpp` and
      the three event-timing call sites — `evt_start_event()`'s length roll,
      `evt_finish_event()`'s restart-after-delay re-roll, and `scgamest.cpp`'s
      load-time StarterType=2 delay — now use it; `scr_randomint` itself is
      untouched, exactly as this item prescribed. It draws once even for a
      degenerate `hi <= lo` range, mirroring VB6's unconditional `Rnd`
      evaluation, so `Time1 == Time2` events (the overwhelming majority)
      keep their exact stream cadence. Full corpus re-blessed — ten routes
      lost their wins to the stream shift and were repaired (see the closure
      log) — **260/260 PASS**, and *Provenance* wins on the default
      `SCR_SEED=1`: that row's seed pin is off.

## Closure log (was: "Suggested order")

The five items below were the original plan; all five are closed, and what
follows them is the dated running commentary of everything settled afterwards.
Kept as the chronology — the *conclusions* live in §4's table and at the fix
sites.

*(2026-08-01: the old item 1 is done — stalemate, hit test, exclusive Hi,
damage floor, worn armour and the RNG question are all settled live; see §1.)*

1. §1 remainder — *(done 2026-08-01, second batch: cadence, recovery, target
   select + the scr_randomint fix, death path, and the shoot rule in BOTH
   Runners.)* The player-facing wield/status surface was settled AND ported
   2026-08-01 (see the divergence table).  *(Third batch, same day:
   StaminaTask/KilledTask settled live in both Runners and ported —
   `make_arena_probe.py` now authors tasks and statics.  §1 is CLOSED.)*
2. §3(a) whole-corpus 3.9 differential — *(done 2026-08-01 via the gen400
   structural oracle plus four run390 probes: room-alt ordering and the battle
   attribute index were both wrong and are now fixed; every other V390 fixup is
   confirmed.)*
3. §2 wildcard ordering — *(done 2026-08-01: no end-of-turn pass exists; the
   mechanism was event task-execution dispatch, version-split between the two
   Runners, plus the worn-drop library rule and thetest's ALRs.  Fixed and
   re-blessed; inverness soft-locks in the real run390 and Scarier
   deliberately doesn't import that.)*
4. §4 body-part statics — *(done 2026-08-01: NO divergence, theory refuted
   live; see the table.)*  Scope filter — *(measured 2026-08-01: zero corpus
   impact, 5 games statically exposed; stays unfixed, see the table.)*
   Division rounding — *(confirmed live 2026-08-01, and the probe surfaced a
   NEW unary-minus tokeniser divergence, zero corpus exposure; see the
   table.)*  §4 is CLOSED except for implementing nothing — every row is now
   settled, measured, or deliberately kept.
5. §3(b) `Les Feux de l'enfer` — *(closed 2026-08-01: its battle-attribute
   actions use 4.0-only attribute indices, so it is native 4.0, not a
   conversion; unwinnability was already established as by-design.  §3(b)
   has no remaining candidates.)*

**2026-08-01: every numbered item in this file is now settled, measured, or
deliberately kept.** What remains open is recorded inline: the scope filter
and the unary-minus tokeniser (both zero-corpus-impact, documented in §4's
table), and ~~re-deriving Shadowpeak under the fixed RNG mapping (§1 corpus
note)~~ *(done 2026-08-02 — seeds 13/87/657, legacy hook retired)*.  The
a5sexpr `-5/2` tangent was probed the same day: NO divergence on
the ADRIFT 5 side (§4 table row) — away-from-zero rounding is symmetric, so
clsVariable's operator-tokenised unary minus and a5sexpr's folded one agree.

**2026-08-02 addendum — the `TheADRIFTProject` zero-word-`*` row is closed,
and it wasn't the wildcard.** Five probe rounds (`ST`, `FM`–`FM7` in
`make_arena_probe.py`) plus a `.tas` transplant of the game itself into
run400 settled it: zero-word `*` and fail-message selection are identical in
both engines; the real rule is that run400's put-in/put-on family runs ahead
of a matched-but-failing task **when the library action can complete**, and
defers to the task's fail message when it would refuse.  Ported (put rows into
`PRIORITY_COMMANDS` with deferred refusals; two-object canonical retries now
prefixed-form-only — Wax Worx pinned the retry rule), corpus 102/102, the
game's route reverted to the author's order.  Two new documented-not-fixed
rows came out of the same probes: run400 lets a failing explicit-verb task
beat single-object take/wear (and does no take→get rewrite), and its
zero-length checker events tick before the command rather than after.

**Same-day follow-up — the 3.9 halves, and the event row was wrong.**
`make_39_fwprobe.py` ran the same cells in run390: `wear` fail-message and
the put-family precedence agree with run400 (the ungated port is right), but
the take half is a version split — run390 runs the library take, exactly as
Scarier does, so only run400's TFAIL diverges.  And the "ticks before the
command" event theory died on a cleaner probe (pill starts in the cup): a
zero-length always-restarting event fires ONCE at game start and never
again, in BOTH Runners.  That one is now FIXED in `scevents.cpp`, with
`icecream` (per-turn customer nag was Scarier-only, confirmed live) and
`TheADRIFTProject` (mix now on the `take slime` turn, byte-matching the
author's transcript) re-blessed; corpus 104/104.

**Same day again — zero-length events have three behaviours, not one.**
Probes `EV2`–`EV5` finished the shape off.  "Fires once and goes dormant" is
what a zero-length event does when the *game start* or its *starter task*
starts it; when a **clock** starts it — a StarterType=2 delay, or a
restart-after-delay countdown — run400 prints the StartText and then leaves
the event running for good: LookText in every later room description,
FinishText and TaskAffected never.  And RestartType=1 genuinely restarts,
printing a second StartText before parking, which is why that path is no
longer gated.  Ported in `scevents.cpp` (`evt_is_zero_length()`, a
non-finishing countdown start, a parked ES_RUNNING clock, and the dormancy
gate widened to the starter-task cell that used to re-fire every turn);
corpus 104/104 with only `TheADRIFTProject` re-blessed for an RNG shift.
One new open row fell out of it: run400 starts immediate events *before* the
opening room description, so their LookText belongs in that description and
their StartText is never seen — a tick-order question with a corpus-wide
blast radius, deliberately left unprobed here.

**Same day, last open row — immediate events really do start at load, in
BOTH Runners.**  `EV6` (a length-3 immediate event with all three texts) took
the zero-length parking model out of the reading, and `make_39_fwprobe.py`
variant `e` got the identical answer from run390: LookText in the opening
description, StartText never seen, length and finish turn unchanged.  Ported
as the two `evt_*_load_events()` halves either side of `DispFirstRoom`
(`scevents.cpp` / `scrunner.cpp`); exposure counted with `scdump.cpp`'s new
`texts=SLF` field (7 immediate events with a LookText, 16 with a StartText,
across 121 corpus games); corpus 128/128 after re-blessing the turn-0 rows
and the games whose battle RNG drifted behind the earlier length rolls.  The
probes that proved it also turned up a *second*, smaller divergence: both
Runners print event LookText **last** in the room block, after the object
list and the character lines, where Scarier printed it inside the description
paragraph.  **That one is now FIXED too (2026-08-02, its own pass)** — the
loop moved past `lib_print_room_contents()` with a two-space
`pf_buffer_join()`, 14 goldens re-blessed as pure relocation, 127/127 PASS
(see the §4 table row).

**2026-08-02 — "held by the player" reaches into a CLOSED carried container,
in run390 too.**  The one remaining unprobed half of the held-by rule (commit
`584f7402` had confirmed carried and worn containers, but every probe used an
*open* one) came up while re-auditing `inverness`: its desk unlocks with the
old key still sealed inside the riddle box, and the whole "the route doesn't
actually need the box opened" note rested on that.  `test/adrift4/harness/make_39_heldprobe.py`
authors a two-object V390 game — an openable box held by the player with a key
starting inside it — whose only task is `probe`, restriction Type 0 / Var1 4
(dynamic object 1) / Var2 1 / Var3 0, reporting HELD or NOT HELD.  run390.exe
under Wine answers, in order:

| state | run390 | Scarier |
|---|---|---|
| key inside the **open** carried box | HELD | HELD |
| key inside the **closed** carried box | **HELD** | HELD |
| closed box dropped on the floor | NOT HELD | NOT HELD |
| the same closed box picked up again | HELD | HELD |
| box reopened while carried | HELD | HELD |

So openness is genuinely not consulted, exactly as `restr_object_in_place()`
case 1/7 has it — no change needed, and the comment there now cites this probe.
A second fact fell out for free: a 3.9 object with `InitialPosition = 2` takes
a **0-based** container-sublist index in `Parent`, in the Runner as well as in
`gs_create()` — writing 0 for the first container puts the key in the box in
both engines.

*New open row (documented, not chased):* the Runner's **carry/container size
arithmetic is stricter than ours**.  With the box at Capacity 52 / SizeWeight 21
(inverness's own numbers) and a SizeWeight 0 key, `put key in box` answers
"The key is too big to fit inside the box." in run390 while Scarier performs it;
a five-item variant (`p39size`, SizeWeight 0/1/10/11/20, player MaxSize/MaxWt
95) had run390 refuse every `take` as "That is too heavy for you to carry." /
"Your hands are full." where Scarier carried and stowed all five.  Whatever
run390 decodes the packed tens/units of `SizeWeight`, `Capacity`, `MaxSize` and
`MaxWt` into, it is not Scarier's reading (`obj_get_size` = `3^tens`,
`obj_get_weight` = `3^units`, player limit = `tens × 3^units`).  It did not matter for the held-by question (the key
was placed by `InitialPosition`, not by `put`) and no corpus row depends on it,
but a `put X in Y` route in some game plausibly could — a size/capacity matrix
probe is the way to settle it.

**2026-08-03 — the size/capacity matrix probe: two globals we were throwing
away, and a container capacity that is a volume, not a count.**  The row above
is settled, and both halves of it were wrong in an instructive way.

*First, the probes themselves were broken.*  The two GLOBAL fields the parser
listed as `iUnk1` and `iUnk2` are the **size and weight scale bases**.  Every
decoded dimension is `base ** index` and every player limit is
`tens(value) × base ** units(value)`; `make_sizeprobe.py` was writing 0 for
both, so index-0 objects decoded to `0 ** 0 = 1` and everything else to 0.
That is the whole of the anomaly recorded above: a Capacity of 52 becomes
`5 × 0² = 0`, which makes a size-1 key "too big to fit inside the box", and a
MaxSize/MaxWt of 95 becomes `9 × 0⁵ = 0`, which makes every `take` fail with
full hands and a too-heavy load.  **run390 was never stricter than Scarier —
it was reading fields Scarier ignored, out of files that set them to zero.**
Proved from the other direction with `iUnk1 = 2`, `iUnk2 = 5`,
`MaxSize = MaxWt = 102`: run400's own debugger (`Help → Debugger…`, Player tab,
which prints Size and Weight Current/Max as decoded integers) shows Max
**40** / **250** — `10 × 2²` and `10 × 5²`, not `10 × 3²` twice — and carrying
one SizeWeight-22 object plus one SizeWeight-12 object shows Current
**6** / **50**.  The ADRIFT editor always writes 3 for both, which is why a
hardwired 3 survived this long; all 121 games in the walkthrough corpus write
3/3, so nothing in the corpus moves.  `sctafpar.cpp` now parses them as
`#SizeMultiple` / `#WeightMultiple` (V400 and V390 alike), and `scobjcts.cpp`
reads them, falling back to 3 for the v3.8 schema that has no such fields.

*Second, container Capacity is a volume the contents spend, not a number of
objects.*  `test/adrift4/harness/make_sizeprobe.py cap2` and `cap3` build matrices where the
two readings disagree, and run400 and run390 answer identically:

| container | Capacity | pool | old count model | volume model | Runners |
|---|---|---|---|---|---|
| c12t | 12 → 1×3² = 9 | 12 × size 1 | 1 fits | 9 fit | **9** |
| c52t | 52 → 5×3² = 45 | 12 × size 1 | 5 fit | all 12 | **all 12**, then a size-9 object on top, then a size-27 one |
| c13t | 13 → 1×3³ = 27 | 12 × size 1 | 1 fits | all 12 | **all 12** |
| c22x | 22 → 2×3² = 18 | 12 × size 3 | 2 fit | 6 fit | **6** |
| c12m | 12 → 9 | 12 × size 9 | 1 fits | 1 fits | **1** (the tie, a control) |
| c12b | 12 → 9 | 12 × size 27 | refused | refused | refused |
| z20 | 20 → 2×3⁰ = 2 | 3 × size 1 | 2 fit | 2 fit | **2** (units-digit-0 control) |
| z02 | 2 → 0×3² = 0 | 3 × size 1 | none | none | **none** |

The two refusal strings are what separate the gates, and they are not the
gates we had.  "*X is too big to fit inside Y*" is `size > the container's
TOTAL volume` — c12b's 9 against a size of 27, and z02's 0 against a size of 1.
"*X can't fit inside Y at the moment*" is `size > what is left`.  The
units digit is **no per-object ceiling at all**: a size-27 object went into
c52t, whose units digit is 2, because 27 ≤ 45.  Fifteen containers in the
corpus have a tens digit of 0, and they change verdict outright — a volume of
0 calls everything too big where an object count of 0 called it a momentary
shortage.

*Third, the volume counts direct contents only.*  `cap3` fills m12 (size 9,
volume 9) with nine size-1 objects and puts it inside n22 (volume 18); a
size-9 object then still goes in, and only the one after that is refused.  So
a nested container spends its own size and not a byte more, unlike weight,
which the Runner does sum recursively (`Sub_22_63` @00047600).

`obj_get_container_maxsize()` is therefore gone, replaced by
`obj_get_container_free_space()`, and `lib_put_in_backend()` spends the budget
as it goes.  Scarier now reproduces every row above, message for message,
under both the 3.9 and the 4.0 probe, and `make -f Makefile.headless
capacitytest` (in `test`, and in `sanitize`) replays both probes against
committed goldens.

How much moved: of the 468 containers in the 121-game corpus, 414 became
roomier (any container whose units digit is non-zero holds more than one
object now), 15 became strictly refusing, and 42 are unchanged.  Not one
walkthrough changed — all 127 still pass — because no derived route ever
pushes a container to its limit.  That is worth stating plainly rather than
reading as reassurance: the corpus proves the fix broke nothing, not that the
old reading was harmless.

*Two divergences noticed in passing, neither size-related.*  The first —
run390 renders a Perspective-2 game in the **second** person ("You pick up the
a1.", "You put the b1 inside the c52t.") where run400 renders the very same
value in the **third** ("Player put the d2 inside the c52t."), and Scarier
followed run400 for both — was **chased and ported on 2026-08-10**
(`lib_get_perspective()`; §4).  These two probes are authored Perspective 2, so
their goldens were re-blessed onto exactly the lines quoted here, 144 per
probe.  The second stands: run400 writes "%player% **put**" where Scarier
writes "%player% **puts**" — one word, but it will be one word in every
third-person clause, not just this one, and it is now the only remaining
perspective divergence.

*Runner mechanics worth keeping:* the debugger form is reached at
`Help → Debugger…`, and `Form1.debugger_Click` @0004B284 opens it without a
prompt when the game password is `"    Wild    "` or empty (otherwise it wants
`"doorWildback"`).  `Form6.updatedebugger` runs from the turn loop, so the
Player tab refreshes every turn.  **Auto complete defaults to ON** and rewrites
input before the echo — turn it off from the Options menu with the `a`
accelerator, and read the echo, never the keystrokes you sent.  `Edit Mode`
kills run400 with `Run-time error '70': Permission denied`.  `Start Transcript`
wrote a 0-byte file even after turns were played, so screenshots remain the
only trustworthy read-out.

**2026-08-03 — the container-listing style selector was never a mystery: it is
a count of two.**  `lib_list_in_object()` carried a comment saying the Runner's
choice between "*Inside the box is a rock and a key.*" and "*A rock and a key
are inside the box.*" "is, frankly, a mystery", and guessed at it with a
static-vs-dynamic test plus a one-object special case.  The listing helper at
`0006A418` in `~/Desktop/run400.txt` settles it: it counts the objects whose
position is 246 (in object) and whose parent is this container into `var_98`,
and then

```
0006A49E   var_98 == 1 && var_9E == 0  ->  "<obj> is inside <cont>."
0006A607   var_98 == 2 && var_9E == 0  ->  "<a> and <b> are inside <cont>."
0006A786   otherwise                   ->  "Inside <cont> is <list>."
```

One or two objects take the postfixed form, three or more the prefixed one, and
**nothing in that chain looks at whether the container is static or dynamic**.
(`var_9E == 1` is the nested arm, printing ", and inside is <list>"; Scarier
does not model it and no corpus game has exercised it.)

Confirmed against a **real Runner transcript** rather than only the listing:
the shipped `EasterWalk.txt` for *It's Easter, Peeps!* (One Room Game Comp
2006 — it ships inside the game's own distribution archive, not in this repo) is a run400 session that hits all four cells — a static container with 1
("An umbrella is inside the umbrella stand."), a static with 2 ("A crumpled
note and a candy coin are inside the pay phone."), a dynamic with 2 ("A few
bills and a couple of photographs are inside your wallet.", in every `i`) and a
dynamic with 6 ("Inside the Easter basket is a strip of candy dots, …").  Under
the old rule three of those four printed the wrong way round.

`lib_list_in_object()` now counts and selects on `count == 1 || count == 2`.
The part-of-NPC test is kept as an extra alternative — it is not in run400's
chain, but keeping it means containers worn by or attached to an NPC hold the
format they had before this rule was derived, and it can now only matter at
three or more contained objects.

Corpus fallout: **37 walkthrough goldens** plus `test/adrift4/harness/capacity_nest_expected.txt`
(its `n22` holds two objects), all re-blessed after reading the diff line by
line — every change is the same rephrase.  Two of them are corroboration rather
than churn, because they are places where the *author's own ALR* only matches
the postfixed wording and so had never fired:

```
yak_shaving:  Inside the pile of snow is a pair of chopsticks.
           -> Sticking out of the pile of snow are a pair of chopsticks.

              You open the seat.  Inside the seat is a hairdryer.
           -> You lift the seat to reveal a concealed storage area. The only
              thing it contains, apart from a few dust-bunnies, is an electric
              hairdryer.
```

An author writing an ALR against the Runner's output is a second, independent
witness to what that output was.  Full `make -f Makefile.headless test` green
afterwards: v4 129/129, capacity both probes, a5 suite untouched.

**2026-08-04 — §8 closed: the 3.9 immediate restart is silent, keeps its full
period, and posture does not hide events.**  Full write-up in §8 above; the
short version is that the SCARE-era comment was wrong in one direction, the
morning's transcript-driven "fix" was wrong in the other, and `run390.exe`
settles both: `evt_start_event (game, event, TRUE)` with the clock left alone.
Six variants of `test/adrift4/harness/make_39_evtimeprobe.py` and the `EV9` twin in
`make_arena_probe.py` cover starter types 1/2/3, restart types 1/2 and both
Runners; `test/adrift4/harness/make_39_evseeprobe.py` closes the visibility residual (event
text still prints while sitting on a surface or in a container).  Corpus: 24
rows re-blessed, three winning routes re-derived — circus at `SCR_SEED=12`,
haunt one turn shorter, and thetest_win via the new
`test/adrift4/harness/thetest_rederive.py`, which grows each of that game's
try-until-it-happens blocks by prefix replay instead of by hand (333 commands
→ 175).  Full `make -f Makefile.headless test` green afterwards.

**2026-08-14 — the last OPEN marker in the file is gone: the empty-`Prefix`
article is not a divergence.**  §4's newest row had a witness but no
measurement, and the measurement refutes the witness.  Method worth reusing:
**when a localised game is the witness, run *that game* rather than building a
synthetic probe** — its replacement table is what makes the answer legible in
the first place, and it is already the thing whose behaviour is in question.
`relojero.taf` in `run400.exe` prints `You take the Fenix de laton de el
cajon.` and `Encontrè un Fenix de laton dentro del cajon.`, so both of
`sclibrar.cpp`'s empty-prefix defaults (`"the "` on the np path, `"a "` on the
other) are faithful and the last "empirical … a mystery" comment in that file
is retired.  The corollary is the one that generalises: **an ALR pair that does
*not* fire is not evidence of anything.**  The pairs that fire are Runner
transcript; the pairs that miss may simply be an author guessing, and this
author guessed `You take a`.  §4 is back to a single P-code-only row
(restriction evaluation order, which is unprobeable by construction).

**2026-08-15 — the take wording is a 4.0 rewording, measured in all four
Runners and ported.**  §4's oldest "documented, not fixed" row asked for one
specific experiment — a live run400 pair on a bare take of an object lying
loose in the room — and it turned out to want three more.  New arena config
`TK` (four loose objects, one of them a container, `persp=1`): run400 answers
**"You take the rock."** to `take rock`, `get rock`, the literal verb `pick up
rock`, and `take all` ("You take the rock, the box and the lamp."), where
Scarier said "You pick up …".  The pre-4.0 half is the other template and was
measured rather than inferred: `p39held.taf` (`make_39_heldprobe.py`, whose key
starts inside a carried box — `drop key` first, then take it off the floor) has
run390 answering **"You pick up the key."** to `take key`, `get key` and `take
all` alike, and `p37pos.taf` has run370 answering "You pick up the small rock."
run380 shows the same handler's pre-4.0 half through its refusal.  Everything
else on the take path already agreed in both generations — the from-container
branch ("You take the rock from the box.") and the whole of `drop`.

Two rows collapse into one fix, because they are two exits from one Runner
handler: the empty-room refusal moves the same way ("There is nothing to pick
up here." pre-4.0, "There is nothing worth taking here." in 4.0), and a
UTF-16LE literal census across the four binaries settles the boundary
independently — "worth taking" exists only in `run400.exe`.  The census also
predicted, and a follow-up run400 session confirmed, that **4.0 has no "else"
form**: `take all except rock` with nothing else left answers the flat line,
where Scarier composed "There is nothing else to pick up here."  Ported as
`lib_is_version_400()` in `sclibrar.cpp`, gating the `parent == -1` take
template and both refusal sites.

Corpus: **71 goldens re-blessed** — every changed line is the rewording, and a
signature check confirms every changed game is a 4.0 `.taf`, i.e. no pre-4.0
row moved.  Full `make -f Makefile.headless test` green.  The method note worth
keeping: when a divergence row names a *pair* of Runner strings, census the
binaries for both strings before designing the probe — here that turned a
two-Runner question into a four-Runner one, found the missing "else" cell that
no transcript would have shown, and cost one extra grep.

**2026-08-15, second batch — the end-of-game score summary and the score-change
notification, both measured in two Runners and ported.**  §4's "End-of-game
score summary" row had sat "documented, not fixed" since 2026-08-03 with an
explicit precondition attached: *the exact assembly of the percentage line
(rounding, whether it is always printed, what triggers the "points short" pair)
has never been captured live; pin those first if this is ever ported.*  All
three are now pinned, and the deferral is spent.

The algorithm came out of `Form1.endmessage`'s P-code with the operand slots
resolved against the binary (`0x4F` gameover — which independently matches the
`0x4F` found for §4's Var1 row — `0x50` MaxScore, `0x51` score, `0x53` the text
accumulator), then every branch was driven live rather than trusted:

    If MaxScore > 0 Then
      "You scored" & Str(score) & " out of the maximum" & Str(MaxScore) & "!"
      "That is" & Str(Int(score * (100 / MaxScore))) & "% of the game!"
      [win branch only]  score = MaxScore ? "Well done - you scored maximum
                         points!" : "You finished " & CStr(MaxScore - score)
                         & " points short."
      & CRLF & CRLF
    "[Press any key to end]"

`Str()` prepends a space for a non-negative number and `CStr()` does not, which
is exactly where the spacing in the ported strings comes from.  The percentage
divides in floating point and truncates: 3 of 8 is **37%**.  The in-game
`score` command's own `(score * 100) / MaxScore` happens to agree on that case
but is a different expression, so `task_print_end_game_summary()` keeps the
Runner's form.

Two probes, five cells each, agreeing line for line: `SC`/`SC0` in
`make_arena_probe.py` under run400, and the new `make_39_endprobe.py` under
run390 (3.9 task actions are the 4.0 ones with every Type > 4 shifted down, and
the 3.9 EndGame action stores only Var1 — see `sctafpar.cpp`).  The summary is
therefore **not** version-gated, which the four-binary UTF-16LE census had
already predicted and the run390 session then proved for the code path, not
just the strings.

The same probes answered a question nobody had asked: run400 printed no
`(Your score has increased by 3)` where Scarier printed one.  That string is in
`run400.exe` alone, and run400 gates it on a menu item —
`GetSetting("ADRIFT", "Runner", "NotifyScore", CStr(False))` — that ships
unticked, never on the TAF's `NoScoreNotify` global.  So Scarier had the source
*and* the default wrong; `scgamest.cpp` now starts the flag FALSE.

Corpus: **134 goldens re-blessed**, 1618 lines out and 362 in.  The combined
diff contains nothing but these two changes, and the blessing produced its own
corroboration — five games carry ALRs keyed on the exact Runner literals
(`panic.taf`, a 3.9 game, rewrites the whole summary *and* the death line;
three French games translate it), which fire only now that Scarier emits the
strings their authors were rewriting.  Full `make -f Makefile.headless test`
green.  Two follow-ups are recorded as new §4 rows rather than chased here: the
pre-3.9 loss/death messages (run370/380 hold neither string) and run390 joining
a task's CompleteText to its WinText on one line.

### 2026-08-15, second batch — the pre-3.9 half of the same summary, and what fell out of measuring it

The 18 pre-3.9 goldens the port above touched had been re-blessed on the
strength of a *string census* alone: `You scored`, `That is`, `% of the game!`
and the rest are all present in `run370.exe` and `run380.exe`, so the summary
was assumed to be the same code path there.  Assumed is not measured, and the
corpus supplied two games short enough to just replay: **`microwaveman.taf`
(3.80, 9 commands)** under run380 and **`castle.taf` (3.70, 17 commands)**
under run370.  Both print the three lines exactly as blessed — 100/100 and
50/50, `Well done - you scored maximum points!` on each.  All 18 now rest on
live evidence.

*Replaying a short corpus game beats authoring a probe whenever the corpus
has one: it costs less, and it is stronger evidence, because it exercises the
shipped data path as well as the code path.*  Three of this batch's four
findings came out of replays, not probes.

**1. `Congratulations!` was never game text — removed.**  Scarier printed it
whenever a winning EndGame task had no WinText.  In run400's P-code the
literal goes to `Form1.StatusBar1` panel 1 at `0x000571AC`, right beside
`You are dead!` at `0x00057205`, and neither literal is referenced anywhere
else.  Three live endings with empty WinText, one at each end of the range —
`TheAmulet.taf` under run400, `ECOD3.taf` under run390, `castle.taf` under
run370 — print the winning task's own CompleteText and go straight to the
summary, with the *location panel* reading "Congratulations!".  TheAmulet is
the cell that makes it unmistakable: its CompleteText ends with the author's
own "Congratulations!", and Scarier was printing the word twice.  Cost: 79
lines out of 87 goldens, and **19 rows in `run_v4_walkthroughs.sh` had been
using that engine line as their win marker** — a marker that proved only that
Scarier had emitted its own string.  Each now points at a line the game
itself prints.

**2. The `MaxScore > 0` guard is 4.0-only — version-gated.**  run390 prints
the summary for a scoreless game and calls it `That is 100% of the game!`.
`ECOD3.taf` shows it at 0 out of 0; `chicago.taf`, which banks 75 points
against a MaxScore of 0, shows `You scored 75 out of the maximum 0!` /
`That is 100% of the game!` / `You finished -75 points short.`, which settles
that the 100% is not the score-equals-maximum case and that the shortfall
prints negative.  `druggy_lane.taf` corroborates from the other direction: it
carries ALRs on all three of those lines.  3.7/3.8 have no corpus exposure
(no pre-3.9 game has a MaxScore of 0) and are treated like 3.9.

**3. The end message is printed at end of turn, not at the action.**  Found
while explaining why chicago's 75 was our 65: `Form1.checkx` runs
`Form1.evaluate` for the whole turn and calls `Form1.endmessage` only
afterwards (`0x0005C681`), so a task's actions *after* its EndGame action
still run and still move the score the summary reports.  Scarier prints and
stops.  **42 tasks across 32 games** put an EndGame action somewhere other
than last, most of them the `[EndGame, ChangeScore]` shape, so this is worth
porting on its own — recorded as a §4 row, not done here.

**4. One trailing blank line versus two.**  A text block ending `<br><br>`
gets the Runner's own line terminator on top of it, so ECOD3's ending shows
two blank lines before the summary where Scarier shows one (measured on the
screenshot: 36px line pitch, 108px gap).  castle and microwaveman, whose
endings have no trailing `<br>`, agree in both engines.  Probably the same
root as finding 3 — re-measure after that port.

Also corrected in passing: the earlier claim that run370/380 hold *neither*
pre-3.9 ending string is half wrong.  `Better luck next time.` really is
absent before 3.9, but all four binaries carry the death line — the pre-4.0
ones as the assembled pair `I'm afraid ` + ` dead!` around a perspective
word, which the first census's `afraid you are dead` search could not match.
That narrows the open row to a perspective divergence plus a genuinely
missing loss line.

Corpus: **87 goldens re-blessed**, 87 lines out and 53 in; 19 win markers
re-pointed.  Full `make -f Makefile.headless test` green.

### 2026-08-15, third batch — the ending moves to the end of the turn, and a false lead worth writing down

**1. `Form1.endmessage` runs after the whole turn, so the summary reads the
turn's final score.**  Ported.  run400's turn driver is `If gameover = 0 Then
Form1.evaluate` (`0005C65C`) … `If gameover > 0 Then Form1.endmessage`
(`0005C681`): the EndGame action only sets a byte, and `Form1.endmessage`
(`0005DDAC`) reads MaxScore and score for itself once everything else in the
turn has run.  Scarier now holds the armed ending in `pending_endgame`
(Var1 + 1) and prints it from `task_print_end_game_message()` at the foot of
the main loop.  *(Corrected 2026-08-15, fourth batch: `pending_endgame` is
**not** the Runner's own byte.  The EndGame handler at `0008D66E` permutes
Var1 0,1,2,3 into 1,3,2,4, and `Form1.endmessage` switches on that; it also
writes the byte **unconditionally**, so a second EndGame action in the same
turn displaces the first.  The `0008D621` test that looked like a first-writer-
wins guard belongs to the *execute task* action, not to this one.)*  chicago's live
75 is reproduced exactly.

The re-blessing is the strongest part of the evidence, and it was not
designed: **31 goldens moved, 25 of them changing score, and 18 of those 25
land on precisely MaxScore** — 100/100, 300/300, 1000/1000, 200/200, 140/140.
Authors put the last scoring action *behind* the EndGame action, so the round
numbers only appear under the Runner's ordering.  Three win markers pinned to
the old, lower score were re-pointed.  `largo_winch` newly reaches 97 of 97
and prints `C'est 99% du jeu!`: `Int(score * (100 / MaxScore))` is a floating
divide, and 97 × (100/97) falls a hair under 100.  That is the Runner's
formula, so the 99 is faithful, not a rounding bug of ours.

**2. A live 3.80 replay confirmed the new ordering on a real game.**
`marooned.taf`, 119 commands under run380, ends with the winning task's
AdditionalMessage `Congratulations, you are no longer Marooned!` and *then*
`You scored 80 out of the maximum 140!` — which is the order the port
produces and the reverse of what the golden used to hold.  Nothing about the
walkthrough had to be adjusted: the Runner replayed all 119 commands and
finished on the same 80.

**3. The jb2000 missing sign-off is not an ending rule.**  This looked at
first like a second divergence riding along with finding 1: run380 finishing
`jb2000.taf` never prints task 14's `" THANK YOU!  THAT WAS MY GAME! ..."`,
so the natural model was "an armed ending silences the rest of the task".
Implementing that guard cost six more goldens their closing line, which was
the signal to go and measure instead of generalise.  Four cells killed it:
microwaveman prints an AdditionalMessage mid-game, marooned prints one on a
task that *does* end the game, a patched `jbt.taf` carrying the two words
`TRAILER TEST` in that field is equally silent, and a second patch clearing
the task's `WinGame` flag leaves the game running with the trailer still
missing.  The guard was reverted; the anomaly is now a §4 row of its own,
scoped to the one task, with our own 3.80 field mapping as the prime suspect.

*The general lesson: when a fix makes several unrelated goldens lose text,
that is evidence about the fix, not about the goldens.  Patch the game and
re-measure before believing a rule that only one cell supports.*

**4. Two things fell out of the topaz measurement.**  run400 replaying
`topaz.taf` (23 commands) prints a `ShowRoomDesc` room *description* with no
room-name heading, where we print the name too — filed as its own §4 row, one
cell, not ported.  And the same screenshot re-confirms the two rows settled in
the second batch from a fourth game: the status bar reads `Congratulations!`
while the transcript does not, and a MaxScore of 0 in a 4.0 game gets no
summary at all.

Corpus: **31 goldens re-blessed**, 3 win markers re-pointed.  Full
`make -f Makefile.headless test` green.

### 2026-08-15, fourth batch — the endgame message is *concatenated*, and that is the whole story

**1. There is no separator, in any version.**  `Form1.endmessage` builds the
win as `out = out & WinText & vbCrLf` (`0005DDF8`) and both losing endings as
`out & vbCrLf & vbCrLf & <message> & vbCrLf` (`0005DFDC`, and
`General.Sub_22_70` at `000523BF` for the death).  Nothing in any of the three
looks at what `out` already ends with.  So every question about the spacing
around an ending — WinText on its own line or run on, one blank line or two —
is really one question: **is the turn's accumulated text already terminated?**

**2. It is in 4.0, and it is not before 4.0.**  Four live cells, no
extrapolation.  run400 finishing `ptbad.taf` prints `...a few more minutes of
your time.` and `You Win! Yay!` on separate lines, and that task's
CompleteText ends at the full stop with no trailing `<br>` (`taftool.py
unpack`), so the break can only be the Runner's.  run400 finishing
`microbe_willie.taf`, whose WinText is empty, still leaves a blank line before
`You scored 7 out of the maximum 7!`.  run400 finishing `QuestI.taf` leaves
*two* blank lines before `I'm afraid you are dead!` — three line pitches,
which is `& vbCrLf & vbCrLf` read literally on top of a terminated `out`.
Against that, run380 finishing `microwaveman.taf` runs the two together as
`You win the game.You have destroyed Coffee Man...`, and run390 finishing
`ECOD3.taf` shows the author's `<br><br>` as two blank lines rather than
three.

**3. Ported as one primitive.**  `pf_buffer_paragraph_line()` supplies a
trailing newline when the text it printed lacks one; it now records the buffer
*position* of that newline (`auto_break_at` in `scr_filter_t`), and the new
`pf_undo_auto_break()` takes it back if nothing has been buffered since.  The
endgame message calls it for a pre-4.0 game only, then emits WinText (or
nothing) and an unconditional `\n`.  Position-based tracking is what makes it
survive the `pf_transfer_buffer()` / `pf_prepend_string()` round trip a task
with actions performs — the first attempt used a flag and silently lost it,
which is why microwaveman would not join.

**4. What the corpus says.**  57 goldens moved, and they split exactly along
the version line the model predicts: all 35 re-flowed paragraphs are pre-4.0
(10 × 3.80, 25 × 3.90) and all 22 gained blank lines are the unconditional
terminator (20 × 4.00, 2 × 3.90).  Not one 4.0 golden re-flowed.  That split
was not designed — the first cut of the port applied the undo in every version
and moved 15 4.0 goldens the wrong way; run400 on ptbad is what caught it.
Twelve pre-4.0 win markers had been pinned to text the re-flow re-wraps, and
were re-pointed at the score summary line (or, for tq3, at a short line the
wrap cannot move).

*The general lesson, again: a rule measured on 3.8 and 3.9 games is a rule
about 3.8 and 3.9.  Run the 4.0 Runner before believing it everywhere.*

Corpus: **57 goldens re-blessed**, 12 win markers re-pointed.  Full
`make -f Makefile.headless test` green.


### 2026-08-15, fifth batch — the room-name heading is a *checkbox*, and the row that hung on it dissolves

Two rows, no code change, and one measurement footgun that had been silently
poisoning this prefix from the start.

**1. `ShowRoomDesc` does not swallow the room name — the Runner was just
configured not to print room names.** The row opened yesterday on a single
topaz cell ("run400 prints the description with no `Forest Clearing`
heading"). Getting the second cell meant teaching `SCR_DUMP_TASKS` to print
`srd=`, which turned up a one-command 4.0 probe in `ptbad.taf` — `TASK 1 …
srd=2 cmd=[[down]]` — and run400 duly reproduced the topaz behaviour: no
heading, and the description ran on the *same line* as the task's text. But
the game's own opening did the same thing: `This is a stairwell.` came out
directly under the intro with no `Stairwell` above it. A room-name heading
that is missing on ordinary movement too is not a `ShowRoomDesc` bug.

The disassembly named the culprit — `mdlSpreadTheLoad.Sub_20_64` gates the
whole heading on one byte (`if (opt = 1 And mode = 0) …`), and the *same* byte
is the `<b>` gate in the routine's short form, so "bold room names" and "show
room names" are a single setting. The setting is
**Options → Display & Media… → Appearance → "Room names in descriptions"**, it
starts unticked, and it is not written to the registry, so every Runner launch
in this prefix has had it off. Ticking it and repeating `down` gives
`You slip on a slinky that was not there.` / **`Pit`** / `The bottom of a
pit. …` — the heading, from the `ShowRoomDesc` path, exactly where Scarier
puts it. Row closed as not-a-divergence; the harness section now carries the
warning and the click coordinates.

What survives the closure is a spacing observation, and it is not new: with the
box ticked the Runner emits **one** newline before the heading (none if the
text already ends in one) and joins the object list on with two spaces, where
Scarier opens paragraphs. That was the standing "the Runner joins a turn's
output into one paragraph, Scarier prints sections" divergence in §3 —
**closed 2026-09-07 for the room block**, which is now built as the Runner's
one concatenated string (see "Ported 2026-09-07: the room block is one string,
joined by pspace()" in test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md). The
divergence still stands for the rest of a turn's output, where a library
message and a task's text remain separate strings.

**2. A `some` prefix survives into the take message in 3.8.** Spotted in the
run380 screenshot taken for the row above: `You pick up some aluminum
clothes.`, where our golden says `the aluminum clothes`. `haunted.taf` in
run380 pins the shape — `an old candle` and `an empty pan` *are* normalized
(`You pick up the old candle.`), but `some grains of rice` is not — and the
other side is normalized in both later Runners (run390/`S_Tar_Dus.taf`:
`You pick up the TP.`; run400/`Through time.taf`: `You take the old crumbled
magazines.`). So `lib_print_object_np()`'s `a`/`an`/`some` → `the` rule is
right from 3.9 on and one third wrong before it. Filed, not ported: 3.70 is
unmeasured, and the same screenshot shows a second thread — run380's
*from-container* take prints raw prefixes for both nouns (`a small pistol from
some aluminum clothes`) — that wants its own cells before either is gated.

*The lesson this time: before trusting a divergence about how a room block
looks, check that the Runner has been told to print one. A preference that
defaults off and never persists looks exactly like an engine difference.*

### 2026-08-15, sixth batch — one game, four Runners: the generators as a version bridge

The `some`-prefix row filed an hour earlier had the right shape and the wrong
confidence, because every cell in it came from a *different game*. Four games
across three Runners can only ever say "these games differ"; to say "these
*engines* differ" you need one game in every version, and each Runner loads
only its own `.taf` version, so that looked impossible.

It is not. **`gen400.exe` opens a 3.80 file directly** — the command-line
argument is honoured, a modal "Tip of the Day" was simply hiding the loaded
game — and `File → Save As` writes 4.00. `gen390.exe` does the same to 3.90.
`microwaveman.taf` therefore became three files, and the deobfuscated
plaintexts confirm the object's `Prefix` field (`some`) comes through both
conversions byte-identical, so the three transcripts differ only in the engine
that produced them. Downward is closed — `gen370.exe` handed a 3.80 file opens
Untitled — but 3.70 does not need a conversion, because a 3.7 `.taf` is a bare
PRNG XOR: rewriting `arlo.taf`'s bone prefix from `a` to `some` and re-encoding
the whole file gives a legitimate 3.70 probe in about a minute.

With that, the row resolves into **two** rules, not one:

1. **The normalized prefix gained `some` in 3.9.** `take clothes` on the same
   object: run370-equivalent and run380 `You pick up some aluminum clothes.`,
   run390 `You pick up the aluminum clothes.`, run400 `You take the aluminum
   clothes.` (and `arlo` in run370: `You pick up some bone.`). 3.7 and 3.8
   normalize `a`/`an` only.
2. **The take-from-container message changed *which noun* it normalizes at
   4.0** — and this one was hiding inside the same screenshot, mistaken at
   first for "pre-3.9 doesn't normalize the from-container path at all".
   `take gun`: run380 `a small pistol from some aluminum clothes`, run390 `a
   small pistol from **the** aluminum clothes`, run400 `**the** small pistol
   from the aluminum clothes`. The container was always normalized; it only
   *looked* raw in run380 because rule 1 leaves `some` alone there. What
   actually moves at 4.0 is the **taken** object, from the plain printer to the
   normalized one.

Two corroborations arrived unbidden, and the second one is the interesting one.
run380 on `wrecked.taf` — a different game, first person — answers `get jacket`
with `I take a tweed jacket from the bench.`, exactly as the new code does. And
run390 on **our own `capacity_nest.taf` size probe** answers the nested
`take w1` with `You take a w1 from the m12.`, which means
`capacity_nest_expected.txt` had been recording a line the real Runner never
prints. That golden is a *self*-blessed transcript, so it had quietly inherited
the bug it was meant to guard; the walkthrough corpus caught it only because
this change made the probe fail. Worth remembering: a synthetic probe's golden
is only as good as the last time somebody ran the probe in the Runner.

Ported as a `>= TAF_VERSION_390` guard on the `some` arm of
`lib_print_object_np()` and a `parent == -1 || lib_is_version_400 (game)`
choice between `lib_print_object_np` and `lib_print_object` in the take
handler. **41 walkthrough goldens** re-blessed — 28 at 3.90, 13 at 3.80, none
at 4.00 or 3.70 — plus the size probe. Every diff line is one of the two rules
and nothing else, which is what a correctly scoped version gate should look
like.

*The lesson this time: when two Runners refuse each other's files, reach for
the Generators before reaching for a second game. A cross-version measurement
taken on one game is a different class of evidence from four measurements taken
on four.*

### 2026-08-15, seventh batch — the last open §4 row: a death sentence with a perspective in it

`I'm afraid you are dead!` looked like a fixed engine line, and in 4.0 it is.
Pre-4.0 it is assembled: `"I'm afraid " & Ary(5) & " " & Ary(4) & " dead!"`,
where `Ary(5)`/`Ary(4)` are the perspective's pronoun and copula. The first
census for this row had searched for the *finished* sentence and so concluded
the string was missing from run370/380/390 for some other reason; the exact
pair census is what found it.

Two traps sat on either side of the obvious reading:

- **run400.exe still contains the pair.** A UTF-16LE census alone would say 4.0
  assembles it too. P32Dasm settles it — no code references those two strings in
  run400; the only referenced form is the single literal in
  `General.Sub_22_70` (`000523DC`). Unreferenced strings survive in a VB6
  binary, so a census can prove presence but never use.
- **The Battle System is a second death site.** run390 assembles the sentence
  twice, once in the EndGame printer (`0003F5C8`) and once in `Form1.chardohit`
  (`00042B14`); run400's `Battles.Sub_12_1` calls the same fixed-literal sub
  (`0004AE95`). So `scbattle.cpp`'s `battle_kill()` had to move with
  `sctasks.cpp`, and the two now share `lib_get_death_message()`.

Measured the way the sixth batch taught: **one EndGame, `Perspective` the only
thing that changes.** `castle.taf` (3.70) and `wrecked.taf` (3.80) each had
their existing death task rewired to a trivially runnable `zdie` — 3.7/3.8
`.taf`s are a bare PRNG XOR, so that is a three-line edit — and the synthetic
`p39end` probe needed only line 9 flipped. run370, run380 and run390 all answer
**`I'm afraid I am dead!`** at Perspective 0 and `I'm afraid you are dead!` at
Perspective 1; 3.80 at Perspective 2 answers like 1, which is the pre-4.0
clamp `lib_get_perspective()` already had. Scarier now reproduces all five
probes exactly.

**The loss half closed without a probe at all.** `Better luck next time.` really
is absent from run370/run380 — but Scarier could not print it there either, and
for a reason that has nothing to do with the message: `sctafpar.cpp` builds a
pre-3.9 EndGame action out of the task's boolean flags, `KillsPlayer` → Var1 2
and `WinGame` → Var1 0, and there is no third flag. Var1 1 is unreachable, so
`case 1` is dead code for any 3.7/3.8 game. A row that has been open on
"needs a 3.7/3.8 EndGame probe" for weeks turned out to be answerable by
reading the parser.

Corpus movement: **none**. `wrecked.taf` is the only pre-4.0 first-person game
in the corpus carrying `KillsPlayer` tasks, and its walkthrough wins. That is
the honest cost of this row — a real divergence, correctly fixed, that no
golden was ever going to catch. What is left open in section 4 is only what
measurement has already been spent on and failed to explain: the `jb2000.taf`
trailer (four probes, still unexplained, and most likely our 3.80 schema rather
than the Runner — *wrong on both counts; see the eighth batch below*) and the
cosmetic `g` / `again` echo.

*The lesson this time: a string census tells you what is in a binary, not what
the binary does with it. When the answer changes the shape of a fix — one
literal or a template — go to the P-code before you go to the corpus.*

### 2026-08-15, eighth batch — the jb2000 trailer: a one-line typo in run380

The last unexplained row in section 4 had four probes against it and a written
guess that our 3.80 schema was mislocating the field. Both halves of the guess
were wrong. A byte-level walk of task 14's record against its neighbours put
the schema exactly on the boundaries — the task spans lines 1096–1150 and task
15 starts at 1151 — so the `AdditionalMessage` we were reading was genuine, and
the Runner really was refusing to print it.

What replaced the guess was a **four-slot probe**. `jb2000.taf` happens to have
four unrestricted, all-rooms tasks (`look at key card`, `shout`, `ask q about
lazer watch`, `wear learner`); giving each of them `BBB.` as its
`AdditionalMessage` and a different `CompleteText` measures four hypotheses per
Runner launch instead of one, which is the difference between a bisect that is
worth doing and one that is not. Five rounds:

- prefixes 129/258/387/516 of the original text → only the full 516 is silent,
  so the cause lives in the last 129 characters;
- `rstrip()`ed text, the same 516 length ending in 42 *dots*, and the cut at the
  internal 42-space run → all three print, so it is not length, not the space
  run in the middle, and not the content;
- `AAA. ` prints, `AAA.  ` does not, `AAA.` + 42 spaces does not, `AAA.` + tab
  does — **two trailing spaces, and only at the very end**;
- `AdditionalMessage` given its own leading spaces is still dropped, so the test
  is on the buffer, not the message;
- and the one that named the mechanism: with `ShowRoomDesc` switched on, the
  message comes **back**, because the room description lands in between.

That is a description of a separator check gone wrong, and the P-code says so
outright. Decompiling `run380.exe` and `run370.exe` — the first time either has
been disassembled here — shows all three appends in the same sub. CompleteText
(`0004CF26`) and the room description (`0004CFB4`) guard only the two-space
separator and do their work either way; the AdditionalMessage at `0004D001`
puts the append **inside** the guard:

    If AdditionalMessage <> "" And Right(out, 2) <> "  " Then
        out = out & "  " & AdditionalMessage
    End If

run370 (`00041C60`) has no check at all, and 3.90/4.00 hoisted the whole test
into a sub of its own — `Form1.pspace` (`0002C880`) and `General.Sub_22_58`
(`0004A948`), identical three-clause bodies — which is exactly the refactor that
fixes it. So the divergence is **3.80 and nothing else**, and the version gate
in `task_suppresses_additional_message()` is an equality test rather than the
usual `< TAF_VERSION_400`.

Porting it needed one new print-filter query, `pf_ends_with_double_space()`.
The Runner is asking about its own turn string; ours has a line terminator on
the end that the Runner's does not, so the helper skips exactly the auto-break
`pf_buffer_paragraph_line()` supplied — the same `auto_break_at` bookkeeping
`pf_undo_auto_break()` already relied on.

Corpus movement: **three** goldens, where the row had promised zero. `jb2000`
loses its sign-off as expected, but `superliam` also loses "Junior runs in and
drinks some root beer." from `eat necko wafers`, and `tra` loses "Your score has
gone up by 5 for making friends." from `feed the dog` — both authored the same
way, a CompleteText ending in two spaces. run380 replaying superliam's first 27
commands confirms the drop live. 156 corpus games have *some* line ending in
two spaces, so the shape is common; what is rare is that line being a task's
CompleteText with a message behind it.

*The lesson this time: when a probe rules out every property of the thing that
is missing, start varying the thing before it. And a written hypothesis is a
liability once it stops being tested — this row carried "most likely our schema"
for long enough that it framed every later attempt.*

### 2026-08-15, ninth batch — the `g` echo was a checkbox, and §4 has no open rows left

The last cosmetic row said the Runner echoes the command `g` repeats and we do
not. Replaying *It's Easter, Peeps!* in run400 — the same game, the same three
pinata swings the row was written from — prints exactly what Scarier prints and
nothing more. The row was wrong.

Where it came from is worth the paragraph. Its evidence was `EasterWalk.txt`,
the author's own 2006 transcript shipped inside `Easter.zip`, and that file
really does say `(hit pinata with umbrella)`. But a transcript records the
*settings* of the person who made it as faithfully as it records the engine.
Options → Display & Media → Appearance has four checkboxes; ticking **References
in brackets** reproduces the author's file line for line. It is not a `g`
feature at all — it echoes what any *reference* resolved to, so `drop it`
answers `(an umbrella)` first — and it is off by default. The same transcript's
`>`-like prompt character is a second box on that tab, and the `verbose` line we
once dropped as "not a command" is the third setting, the Ctrl+V toggle. Three
of that file's oddities, three preferences.

Two dead ends before the checkbox, both instructive. The registry has
`showbrackets` and `showgt` under `ADRIFT\Runner`, and the P-code loads them
with sensible defaults at `00074503` — but writing `1` to both changed nothing,
because the Appearance boxes never restore from the registry. That is the same
rule already written down for *Room names in descriptions*, now known to hold
for the whole tab. And the exe's own About string reads "Version 4.00 Release
0" while the registry says `4000052`, which sent me looking for a build
difference that was not there.

The P-code, once we knew what to look for, is unambiguous:
`mdlSpreadTheLoad.Sub_20_62` builds `"(" & cmd & ")"` at `0008A0AD`–`0008A0BB`
and prints it through `General.Sub_22_27`, the whole thing behind a byte tested
`= 1` at `0008A095`. The same sub opens with the six words the Runner takes for
"do that again" — `!!`, `again`, `last`, `previous`, `!`, `g` — and that is the
one thing here that *was* a real gap: Scarier had `again`, `g` and SCARE's
`!`-history, but not `last` or `previous`. Both measured live (`previous`
repeats an examine, `!!` repeats it again) and added to the same table entry in
`scrunner.cpp`. No golden moves; 243 v4 rows still PASS.

A probe footgun fell out of it. The history scan at `0008A060`–`0008A089` walks
back over entries *identical* to the word just typed, which is what makes
`g g g` repeat the original — but it does **not** skip the other five
synonyms. So `last / previous / ! / !!` typed in a row all fail: each repeats
the literal word before it. My first probe did exactly that and read as "only
`g` and `again` work". Interleave a real command between synonyms.

*The lesson this time: a shipped author transcript is ground truth about the
game, not about the engine. Every option the Runner offers is a way for two
correct sessions to disagree — before believing a transcript proves a
divergence, reproduce it yourself with the defaults, and if it does not
reproduce, go looking for the checkbox.*

**§4 now has no open rows.** What remains in the table is three deliberate
divergences (dynamic-object overrun, negated `Var2`, the completed-`*` task),
two measured-and-parked ones with zero corpus reach (object scope on a task
command, run400's take-vs-failing-task split), the won't-fix combat RNG, and
restriction evaluation order, which rests on the P-code because no ADRIFT 4
restriction can have a side effect to probe.

**2026-08-17: SYNONYM substitution runs before task matching in run390 too —
NO divergence.** *Lara Croft: The Sun Obelisk* (`croft.taf`, 3.90) declares
one `SYNONYM`. The game is adult AIF, so the strings are written schematically
here — `V` is the verb the walkthrough types, `V'` the verb the tasks are
written with, `N` the shared noun; the literal commands are in
`test/adrift4/goldens/croft_solution.txt`, which is gitignored. The table is
`SYNONYM [V] -> [V']`, and the shipped walkthrough types `V N
with jade` annotated +3. Scarier scores nothing there: the rewrite happens
before matching, so the matcher sees `V' N with jade`, and TASK 117 `V'
N*` — unrestricted, six indices in front of the +3 task, trailing `*`
swallowing the rest of the line — claims it, having already fired for its own
+2 earlier in the scene. That reasoning is entirely from the dump, i.e. it is
evidence about *Scarier*, not about the Runner, so it was measured.

Replaying all 101 croft commands in `run390.exe` is not viable — the game's
picture window takes focus and the scripted keystrokes desync within a few
turns (the first attempt sat in the Ante Chamber at score 0 after 78
commands). So the shape was reduced to a probe:
`test/adrift4/harness/make_39_synprobe.py`. It is built from the game's own
vocabulary and is therefore gitignored alongside the solution, so it exists
only on this machine. One room, no objects, `MaxScore
3`, `bNoAutoComplete 1` (the game turns the Runner's input mangling off for
itself), the same `SYNONYM`, and two unrestricted repeatable all-rooms tasks —
TASK 1 `V' N*` printing "TASK1 FIRED." for no points, TASK 2 `me and jade
V' * N*` / `V' * N* with jade` printing "TASK2 FIRED." for +3.

`run390.exe` and `harness/scare` agree line for line:

| command | run390 | Scarier |
|---|---|---|
| `V' N with jade` | TASK1 FIRED, score 0 | TASK1 FIRED, score 0 |
| `V N with jade` | TASK1 FIRED, score 0 | TASK1 FIRED, score 0 |
| `me and jade V N` | TASK2 FIRED, score 3 | TASK2 FIRED, score 3 |

Three findings at once, all confirmed in the Runner: the `SYNONYM` rewrite
precedes task matching (row 2 is indistinguishable from row 1); a lower-indexed
unrestricted task whose command ends in `*` **does** steal the line from a
higher-indexed task that spells the same command out; and a **medial `*`
matches zero words** (row 3 matches with nothing between `jade` and `N`).
`croftwlk.txt` therefore tops out at 147 in the very Runner it was written for.

Two harness notes, both re-learned the hard way. **The first scripted command
after launch is routinely lost** — the first probe run showed a blank echo and
"I don't understand." where the command should have been, and padding the
script with two `look`s fixed it. And **the echo is the only trustworthy record
of what the Runner received**, so a probe worth running is one where every
command echoes visibly and the answer is a distinct printed string rather than
an absence.

### 2026-08-17 — `getdynfromroom` is not a matcher, and the probe that proved it

The §9 item read like a small one: *what are the selection criteria?* The
answer is that the criteria were the easy half. The hard half is **when** the
function runs, and Scarier had that wrong from the start.

**The probe.** `make_arena_probe.py` config `GD` (rebuild and deploy with
`python3 make_arena_probe.py GD p4GD.plain && python3 taftool.py pack
p4GD.plain pronoun_test.taf p4GD.taf`, then drop the `.taf` into
`~/adrift-battle/runner/wine/pfx/drive_c/adrift/`). Every syntax variant gets
a room of its own holding exactly one dynamic object nothing else claims —
the probes are destructive, so two sharing a room would leave the second
silent for the wrong reason — and every CompleteText reads `GDn
[%theobject%]`. That last detail is what makes the whole thing legible:
run400 expands `%theobject%` to "the coin", prints `%object%` **verbatim**,
and leaves `%theobject%` unexpanded when no reference is set. So the
read-out distinguishes *set to X*, *set to nothing* and *never evaluated*
without any second channel.

**The finding that reframed the item.** Typing a probe's command does
nothing. Nothing fires spontaneously either — sitting in the room and typing
`look` produces silence, turn after turn. The function is evaluated in exactly
one place: `mdlSpreadTheLoad.Sub_20_22` at `@0005F750`, the routine that runs
a task **by its index**, as a preamble before the run. Its six callers are the
whole story — `Battles.Sub_12_1`/`Sub_12_4`, the NPC walk
(`mdlSpreadTheLoad.Sub_20_2` at `00068B8E`/`00068BED`), and `Sub_20_11` /
`Sub_20_33`. The typed-command matcher is not among them. The only way to
make any probe speak was to wrap it in a second task whose action is *execute
task N* — which is how `GDX1`…`GDXR` came to exist.

And the preamble scans the task's **alternate** commands only. A TAF task
stores a primary `Command` plus an `ALTCMD` array, and the array is what
`Sub_20_22` walks, so `GD0` — whose *sole* command is the function — never
evaluates it, while `GD2` (function written second) does. This was
mis-diagnosed as a 1-based/0-based off-by-one for two rounds before the TAF
record shape explained it.

**The criteria, measured variant by variant.** Squeeze every space out of the
command; it must open `#%object%=getdynfromroom(`; the **raw** command must
end `)` (`getdynfromroom(larder)x` is not a function — `GDL`, retested twice).
The squeezed argument is compared to the room's `Short`
**case-insensitively** in both directions (`attic`≡`Attic`, `STUDY`≡`Study`),
so the `LCase()` in the P-code is redundant — an early "Option Compare Binary"
reading was wrong. The first **non-static** object standing directly in the
room wins, in object order (`vault` holds *ring* then *gem* and yields the
ring; a cellar holding only a static yields nothing, and yields the mop once
one is put there). The reference survives the rest of the turn.

**Two fenceposts, both deliberately not ported.** The room scan is `For r = 0
To roomCount - 1` over a 1-based array, so the game's **last room** can never
match: `larder` as room 10 of 10 returned nothing, and returned its pie the
moment a sacrificial 11th room was appended. (Its object loop has no such bug
— the mop, added last, is still found. That asymmetry is why the probe parks a
spare at each end.) And it squeezes the *argument* but compares against the
*unsqueezed* room name, so no room whose name contains a space is reachable —
not `dark cave`, not `wine cellar`, not `winecellar`, and not the manual's own
worked example `getdynfromroom(The Park)`. Both bugs can only lose a match the
author meant to make, so Scarier squeezes both sides and scans every room, and
says so at the fix site.

**The port, and the corpus surprise.** `run_task_run_by_index()` (scrunner.cpp)
is the new by-index runner carrying the preamble; the five `task_run_task()`
sites that correspond to `Sub_20_22`'s callers now go through it (scbattle.cpp
×2, scevents.cpp's 4.0 branch, `run_npc_walk_task()`'s 4.0 branch,
`task_run_set_task_action()`). `run_game_functions()` and the entire
`is_normal == FALSE` branch of the task matcher are deleted, which collapses
`run_match_task_common()` back into `run_match_task_commands()`.

§9 recorded this item as having no corpus exposure. It has exactly one user,
and the "no exposure" reading was simply a bad grep: *Humbug*'s TASK 310
(`# Robot cleaning` + `ALTCMD[1] = #%object% = getdynfromroom(Cellar)`) is not
the target of any *execute task* action — but an event's `TaskAffected` runs a
task by index just the same, and Humbug's always-restarting one-turn EVENT 45
*robot in cellar* points straight at it. The getdynfromroom is what tells the
robot which object to sweep down the chute. The faithful implementation
reproduces the committed golden **byte for byte**; the old spontaneous pass
had been firing the same task a *second* time every turn, and the golden
happened to survive it. Turning the by-index preamble on while leaving the
spontaneous pass in place is what broke Humbug — the robot ate the treasure
map — and is how the double-fire was noticed at all. Corpus: v4 walkthroughs
all PASS.

### 2026-08-17, second batch — a static in your hands: what put it there, and what it weighs

The §9 item that started as "statics have no `SizeWeight` — what should
`obj_get_size`/`obj_get_weight` say?" turned out to be three questions, and the
one in the code comments was the least interesting of them. The `SM` arena
probe (`make_arena_probe.py`) carries a dynamic coin, a static plaque, events
that move either one to the player's hands or to the far room, and two tasks
whose actions move the *referenced* object — `grab %object%` (to held by
player) and `hide %object%` (to hidden). Four run400 sessions under Wine:

1. **Zero was right, but not for the stated reason.** `count` with the plaque
   in the inventory reads `Size: 0 / Weight: 0` — but so does `count` with the
   *coin* there, if an event is what put it there. The static's 0 was never
   isolable against a baseline that was itself 0. It is right anyway: a static
   can reach the hand by no other route, so run400 never charges for one.
2. **A task action cannot move a static, by any selector or to any
   destination.** `grab plaque` prints `SM: grabbed [the plaque].` — so the
   `%object%` reference resolves to a static perfectly well — and then leaves
   the plaque where it was. `hide plaque`, aimed at a static an event had
   already put in the player's hands, is refused identically. That matches the
   standing static-analysis note on run400's mover (`Sub_20_11 @0008C200`,
   `If Objects(o).Static = 1 Then <next object>` at `@0008C360`, ahead of the
   destination `Select Case`), and it was a genuine divergence: Scarier's
   `task_move_object()` only limited the *by-index* selector to dynamics, so
   the "referenced object", "all held" and "all worn" selectors moved statics
   freely. Ported as an early return next to the existing negative-index
   guard. This is the answer the `scevents.cpp ~317` comment wanted:
   **`evt_move_object()` is the only place a static moves**, and its comment
   now says so instead of asking.
3. **The Runner's event mover half-moves an object into the hand, and that we
   do not copy.** The inventory lister shows it; nothing else agrees. `count`
   stays 0 while the coin is listed, `drop coin` answers `You are not holding
   the coin.`, and `drop plaque` gets the out-of-scope `I don't understand
   what you want me to do with the plaque.` Take the coin by hand instead and
   `count` reads 1 — then let an event move it to the far room, and it leaves
   the listing while `count` *stays* at 1. Both directions of stale. It is not
   a running counter: `grab coin` twice in a row leaves `count` at 1 and
   `drop coin` returns it to 0, so the totals are recomputed each time, over a
   held-ness that the event mover simply never writes. Scarier recomputes from
   object positions and is self-consistent — event-held coin weighs 1, and can
   be dropped. Carried as `deliberate:`: reproducing the split would mean a
   second held-state threaded through game state and the `.tas` save format,
   to inherit a bug that grants free carrying capacity in one direction and
   permanently confiscates it in the other.

Scarier now reproduces (1) and (2) cell for cell against the run400
transcripts, including the exact refusal texts. Regression: 251/251 v4
walkthroughs, the full headless suite green (five `PASS: 0 failure(s)`
blocks), a5 walkthroughs `MATCH=173, NOSCRIPT=2, SKIP=24` — no golden moved,
which is what you would expect of a rule that only ever *refuses* a move no
solved route asks for.

### 2026-08-17, third batch — the `#` is gone before the lister runs

The §9 item read "a leading `#` on an NPC's in-room description is taken as
*use the default X is here*", and the probe was meant to settle what happens
when `#`, empty and plain texts share a room. It settled something else: by
the time the room lister runs there is **no `#` left to find**. The loader
(@00091EDF) substitutes the whole text with `<name> is here.` when the game is
read in, so the lister (@00072944) has nothing but ordinary texts in front of
it, and the test it makes is on the **tail** — VB's `Right(text, 9)` against
the literal `" is here."`.

The `NH` arena probe (`make_arena_probe.py`) puts eight characters in one
room: `Alpha` and `Charlie` with `#`, `Bravo` with a text of its own, `Echo`
with an empty one, `Delta` with the default typed out in full
(`Delta is here.`), `Foxtrot` with the default *shape* but someone else's name
(`The stranger is here.`), and `Golf`/`Hotel` as near misses on punctuation
and case. run400 prints, all on one line:

```
A bare probe room.  Alpha, Charlie, Delta and The stranger are here.  Bravo
lurks in the corner.  Golf is here!  Hotel IS HERE.
```

which answers every part of it at once:

1. **The author's own default folds in too.** `Delta` and `Foxtrot` join the
   sentence. Nothing looked up a name to put there: `Foxtrot` contributes
   *The stranger*, because what the sentence is built from is the text with
   its last nine characters cut off.
2. **The tail test is exact and case-sensitive.** `Golf is here!` misses on
   the punctuation, `Hotel IS HERE.` on the case, and both print verbatim in
   the second group — so this is a `Binary` comparison, unlike the string
   compares in the same module that the `GD` probes found running under
   `Option Compare Text`.
3. **The joined sentence comes first.** Scarier had the two groups the other
   way round, which was invisible for as long as the first group could only
   hold `#` characters that no author bothers to mix with anything.
4. **An empty text drops the character.** `Echo` is not mentioned. Unchanged
   — Scarier already did this.

`lib_print_room_contents()` (sclibrar.cpp) now collects the joined list first,
from both routes, and the custom-text loop skips whatever the list took. Two
things had to be handled that the Runner never sees, because the Runner has no
line breaks in a room block at all (§3, then the standing
section-vs-paragraph divergence):

- Authors routinely start a character's in-room text with `\n` or `<br>` so
  the character gets its own line. The custom-text loop already stripped those
  when the buffer was on a fresh line; the fold has to strip them *before* the
  tail test as well, or `<br>Delta is here.` folds and then contributes
  `<br>Delta`. `lib_skip_leading_breaks()` is shared by both sides so they
  agree on what the text is.
- The joined sentence used to be preceded by an unconditional break, and so by
  a blank line. That is now conditional — one line break, never two —
  documented at the fix site and in the §4 row as a **deliberate** change: a
  group that now shares the block with the characters it belongs among should
  not be fenced off from them, and one list to a line is what the rest of the
  room block already does.

**Both bullets were retired on 2026-09-07**, when the section-vs-paragraph
divergence itself was closed: the room block is now the Runner's single
concatenated string, so there is no Scarier-side line break for either bullet
to reason about. `lib_skip_leading_breaks()` is gone — run400 tests
`Right(text, 9) = " is here."` @004729A7 and trims `Left(text, Len - 9)`
@004729FE on the raw text, so `<br>Delta is here.` really does contribute
`<br>Delta`, and the author's break lands after the two separator spaces. The
joined sentence is preceded by the Runner's own hard-coded `"  "` @0047295B.
See "Ported 2026-09-07: the room block is one string, joined by pspace()" in
test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md.

Regression: 81 v4 goldens re-blessed, and every hunk in them is the fold, the
reorder, or the line rewrapping those two cause — 547 of the deleted lines are
the blank line above, and the only text that changes case is a folded
`a monkey is here.` that now starts a line and so starts a sentence. After the
rebless, 251/251 v4 walkthroughs, the full headless suite green (five
`PASS: 0 failure(s)` blocks), a5 walkthroughs `MATCH=173, NOSCRIPT=2,
SKIP=24`.

### 2026-08-17, fourth batch — a stopped walk is not paused, it is rewound

The §9 item offered two answers — a completed `StoppingTask` pauses the walk,
or it ends it — and the right one is a third. Probe `S` of
`make_400_walkprobe.py` is a looping two-stop walk, `Times` 2 and 2, with the
`stopit` task as its `StoppingTask` and a `resume` task whose only action is
the *unset task* action aimed back at `stopit`. Three sessions in run400 under
Wine, each padded with two leading looks, freezing the walk at a different
point of the cycle:

| session | `stopit` on | `resume` on | what the release turn printed | next visible step |
|---|---|---|---|---|
| 1 | turn 6, standing at stop 0 | turn 10 | nothing | turn 12, `BOB LEAVES` |
| 2 | turn 7, the turn the walk was **due to move** | turn 10 | nothing | turn 12, `BOB LEAVES` |
| 3 | turn 4, standing **away** from stop 0 | turn 7 | `RESUME TASK DONE.  Bob BOB ENTERS..` | turn 9, `BOB LEAVES` |

Read together they say one thing: while the stopping task is complete the walk
is held at the **top of its cycle**, and the moment the task is un-completed it
runs a fresh cycle, arriving at stop 0 on that very turn.

- Session 3 is the one that shows it plainly. The walker is at the far stop,
  and the release turn does not merely un-freeze it — it moves it, in the same
  breath as the task's own completion text.
- Sessions 1 and 2 look at first like a lost tick: the release turn is silent
  and the next move is two turns away rather than one. It is the same fresh
  cycle. The walker is already standing at stop 0, so the cycle's first act —
  arrive at stop 0 — moves it to where it already is, and a move that goes
  nowhere prints no enter line (the same rule walk probe K found for a
  follow-player stop the walker was already sharing with the player).
- Session 2 also settles the turn order, which was never in question but is
  worth having on the record: `stopit` lands on the turn the walk was due to
  move, and the walk does not move. The stopping-task check runs after the
  player's task, on state the task has already changed.
- And it rules out the reading Scarier had. A merely-frozen counter would have
  had one tick left in session 2 and two in session 1, so the two sessions
  would have released one turn apart. They did not.

The port is one line: where `npc_tick_npc()` (scnpcs.cpp) skipped the tick and
left the counter alone, it now calls `npc_start_npc_walk()` first. All three
sessions then replay cell for cell against the run400 transcripts. Regression:
256/256 v4 walkthroughs, the full headless suite green, a5 walkthroughs
`MATCH=173, NOSCRIPT=2, SKIP=24` — **no golden moved**, which is what a corpus
of solved walkthroughs should look like for a rule that can only be reached by
un-completing a task on purpose.

There is no 3.9 half to this. V390 has no *unset task* action at all — its
action types 5 and up are V400's 6 and up, which is why the parser's
`V390_TASK_ACTION:Type>4?#Type++` fixup exists — so a 3.9 stopping task, once
complete, stays complete, and a stopped walk stays stopped whatever the model.

### 2026-08-17, fifth batch — the negative length is a back-reference, and it was hiding a real bug

The last §9 item said up front that it was not a probe, and it was not: it is
data archaeology, and the data is the 427-game version 4.0 corpus itself. A
one-line trace in `parse_get_v400_resource_offset()` printing every resource
record as the parse meets it (name, length) turns the question into arithmetic.

The rule, and it is exact:

> A negative resource length `-N` means **entry N of the game's resource
> table**, counting from one. The table holds each distinct resource *name* in
> the order the parse encounters it.

Two details are what the old `-(length+2)` guess was missing, and each one is
worth the trace line that found it:

1. **The `##` looping-sound flag is stripped before the name comparison.**
   `.\Words_be.mid##` and `.\Words_be.mid` are one entry, not two.
   `To_Hell_And_Beyond` alone has 21 records that line up only once this is
   done — it is the game that settles the question, since deduplicating on the
   raw name gets every one of them wrong and no game prefers the raw name.
2. **A named resource with a length of zero still takes an entry.** These are
   resources the author referred to but never embedded, and they are invisible
   to Scarier because `parse_handle_v400_resource()` drops zero-length slots
   before the table ever sees them. They are the reason the count "slips": in
   `the_pk_girl` the first three back-references are dead on (`-1`, `-2`, `-3`)
   and every later one is two too high, and the two extra entries are a pair of
   truncated junk names — `./././././sounds/./.` and `./././././sounds/././s` —
   sitting between `katryn.jpg` and `aileen.jpg` with no data behind them.

Checked over the whole corpus: **535 negative records, 535 matches, zero
misses.** (Under the raw-name variant, 514 of 535.)

Scarier never counts. It resolves a back-reference by looking the name up in
its own table, which reaches the same entry by a different road and needs no
index — so the decode, by itself, changes nothing. The value of having it is
that it says exactly where the two roads part: **a back-reference to a name
that was only ever seen with a zero length.** Adrift has an entry for it;
Scarier has none, the lookup fails, and the record fell through to the code
that *adds* a resource — entering the back-reference itself as the length.
Since each entry's offset is measured from the one before it
(`offset = prev.offset + prev.length + 1`), a negative length there runs the
whole chain backwards for every resource that follows.

That was live, in two of the six games that have a dangling back-reference:

| game | dangling name | cost |
|---|---|---|
| `MikeDesert_SuburbanProdigy3` | `.\gold.jpg`, entry 28 | the **ten** resources embedded after it, all 27 bytes early — both endings' pictures among them |
| `House` | `././sounds/././sounds/././sounds`, entry 9 | `Atmos1.wav`, 8 bytes early |

The other four (`Dream Quest`, `To_Hell_And_Beyond`, `Orient_Express`,
`The_Shuffling_Room`) embed no resources at all, so their chains had nothing to
corrupt.  (`Dream Quest`'s negative lengths are a red herring in a second way
too: run400 refuses to open that game at all, and the reason is a command-less
task, not a resource — see the `dreamquest` entry in
`test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md`.) `House` turns out to be one of them in practice — its `Embedded` flag
is off, so its offsets are never used — which leaves `MikeDesert` as the one
game a player would have noticed, and only if they got to an ending.

The fix is to report *no data* for an unresolvable back-reference instead of
adding a table entry for it: the resource genuinely is not in the file, so
length 0 is the truthful answer and the chain stays intact. Verified against
the files' own bytes rather than against ourselves — take every embedded
resource in the corpus, add the game's resource base to its computed offset,
and read the first four bytes: **472/472 land on their own `GIF8` / `RIFF` /
`MThd` / JPEG SOI marker.** Before the fix, `MikeDesert`'s last ten read as
noise.

Two trace lines stay behind, both under `SCR_TRACE_PARSE`: one naming each
resource with its resolved length and offset, and one for the unembedded slots
Adrift counts and we drop — which is the whole story of this item in two lines
of output whenever a game misbehaves again.

Regression: 256/256 v4 walkthroughs, the full headless suite green, a5
walkthroughs `MATCH=173, NOSCRIPT=2, SKIP=24`. No golden moved; the headless
harness has no resource playback, so the corpus cannot see this fix — the magic
bytes are the test.

With this, **§9 is closed**: five items, four settled in front of a running
Runner and one in front of the corpus, all five ported.

**2026-08-17: §10 — the event-length roll is EXCLUSIVE of `Time2`, in both
Runners, in every event-timing family; ported the same day, and the corpus
paid for it.** The measurement is exactly the recipe §10 prescribed, plus one
refinement: instead of one event per question, config `EL` in
`make_arena_probe.py` (and its V390 twin `make_39_evlenprobe.py`) authors all
three roll families into one file — immediate/restart-immediately length
`1..3`, delayed-start length `1..3`, and a `1..3` **delay** with fixed length —
so a single 24×`z` session yields ~50 draws across every family at once, and
the `1..3` range separates the three candidate readings by value alone
(exclusive → {1,2}, inclusive → {1,2,3}, the `+1` form → {2,3}). run400,
three sessions, ~145 draws; run390, two sessions, ~95 draws: **every single
draw in {1,2}, both values abundant everywhere.** So both Runners roll
`lo + Int(Rnd * (hi - lo))` for the event length, the StarterType=2 start
delay, and the restart-after-delay re-roll alike; the P-code's two `+1`
sibling sites belong to some other record pair entirely, and there is no
version split.

The port is `scr_randomint_exclusive()` (`scutils.cpp`), used at the three
event-timing call sites — `evt_start_event()`'s length roll,
`evt_finish_event()`'s restart re-roll, `scgamest.cpp`'s load-time delay —
and nowhere else; `scr_randomint`'s other callers keep their own arbitrated
semantics, per this section's own instruction. The one design decision worth
recording: the function **draws even when `hi <= lo`**, because VB6 evaluates
`Rnd` unconditionally, so every `Time1 == Time2` event — the overwhelming
majority — consumes exactly the RNG draw it always did and keeps its stream
position. That confined the blast radius to games authoring real ranges.

Confined, not small. *Provenance* flipped the right way — EVENT 7's `0..1`
roll is now 0 on every seed, the game wins on the default `SCR_SEED=1`
(260/300), and its `SCR_SEED=2` pin came off — but the corpus `--bless` run
refused **ten** rows whose win markers had vanished, and a control run of the
reverted build passed everything except Provenance, proving all ten were the
roll change and nothing else. (Worth internalizing: exclusive rolls make
ranged respawn and ambient events fire *sooner* on average, so games with
attrition mechanics got genuinely harder.) The repairs, cheapest first:

- **Six rows re-pinned** by seed sweep, route text untouched: `light_up`
  45→16, `azra` +26, `adriftorama` +18, `ticket` +10, `wrecked` +234,
  `textident_evil` +4.
- **The three Shadowpeak variants re-derived by their documented recipe**
  (`test/adrift4/notes/Shadowpeak_walkthrough.md`): sweep seeds for a clean
  upstream through `press stone button` (score, room 157, Damastus alive via
  `SCR_TRACE_JUDY`), then `harness/shadowpeak_chase.py` re-derives the maze
  chase. New seeds 7 / 13 (allgargoyles) / 149 (killwraith), new chase blocks
  spliced in, all three WIN. The recipe held up exactly as written — no
  brute-force whole-route sweeping needed.
- **Vampire re-derived by hand at seed 1.** EVENT 4 [CarsAtRingRoute3]
  (`Time 10..15`) is that file's only ranged roll, and re-timing it moved
  every draw behind Simonsen's random-walk to the Bozo club — he now arrives
  a fixed half hour later, whatever the player does. The route absorbs it
  with 29 `x girl` time-killers, `buy beer` hoisted into the dead window, and
  the waits cut 6→4; the taxi queue goes in at 23:39, one minute before T83
  starts refusing. Zero slack, still 100/100.
- **iqsfot's chapter-5 fight re-choreographed at seed 1.** The four mook
  respawn events (15–18) and the heal event (45) all carry ranged timers, so
  their cycles shortened and re-phased and the old attack weave desynced.
  Re-derived adaptively from the mechanics in
  `notes/Irvine_Quik_walkthrough.md` (correct verb always KOs, whiff means
  absent, exits blocked while anyone stands, `claw` only with the elite
  alone). Still a WIN.

Final state: **260/260 v4 rows PASS**, Provenance unpinned, nine other pins
updated in `run_v4_walkthroughs.sh` with the reasoning in each row's comment.
The a5 corpora never moved — the change touches v4 event code only, and
`scr_randomint_exclusive` is a pure addition to the shared `scutils.cpp`.

With this, **§10 is closed** — raised, measured in both Runners, ported and
re-blessed inside one day — and the file has **no open section**.

### 2026-08-23 — the pooled burden does not drift, and three things fell out of proving it

The question this batch set out to answer was narrow: `lib_carried_burden()`
recomputes the 3.7/3.8 pooled burden from object positions on every call, and
`glk capacity` is a **no-op** for a pre-3.9 game on the grounds that there is
no Runner arithmetic to escape. Both rest on the claim that the pre-3.9
Runners do not keep a running total the way run400's `Proc_21_54` does. That
had never been measured — 4.0's two leak shapes were, in both directions, so
the pre-3.9 twin was an inference.

It is now measured, in both real Runners, and the inference holds. The probes
are `make38leakprobe.py` (3.80, built out of `marooned.taf`) and its 3.70 twin
`make37leakprobe.py` (built out of `arlo.taf`, since run370 rejects a 3.80 file
outright — "Incorrect version"). Both use refusals as the readout rather than
`count`: with every object class 0, `MaxCarried` 2 and the sole refusal "Your
hands are full.", *how many more you can pick up* is the total, read out loud.

- **`pleakc` / `qleakc` — contents leak.** take box, take q0, put q0 in box,
  drop box, then take r0/s0/t0. Both Runners: r0 and s0 accepted, t0 refused.
  Recomputed. (In 4.0 the same sequence refuses s0 — the contents' size never
  comes off the total.)
- **`pleakw` / `qleakw` — wear double-debit.** take w0, wear w0, drop w0, then
  the same three. Both Runners: r0 and s0 accepted, t0 refused. Recomputed —
  4.0's cycle would have accepted t0 on a total gone negative.

So neither shape of leak has anything to leak here: a container is not charged
for its contents under the pooled model, and — see below — wearing is not a
charge at all. `lib_carried_burden()`'s always-recompute is faithful, and the
`glk capacity` no-op for 3.7/3.8 is correct as it stands.

Three divergences fell out of the probing, all three ported, all three in §4:

1. **Worn objects are free of `MaxCarried`** (`pworn` / `qworn`). We counted
   them. Both Runners take 1 worn + 2 held against a limit of 2, and `count`
   reports 0 while the wearable is on. Fixed in `lib_carried_burden()`.
2. **The 3.8 `count` wording** (`pcount1`) is one unlabelled line counting
   *objects*, no singular form, no `Burden:` label, no Size/Weight axes.
3. **The pre-3.9 wear message uses the indefinite printer** (`pwear`, carried
   across all four Runners; `pwearv` for the empty-prefix-before-a-vowel cell).
   `lib_print_object_list()` grew an optional item-printer parameter for it,
   defaulting to the normalizing one so every other call site is untouched.

Regression: **261/261 v4 walkthroughs PASS**, the full headless suite green
(capacity matrix and nest both PASS), a5 unaffected — nothing here reaches
past the v4 library. Five 3.80 goldens re-blessed for the wear article and
nothing else; the diffs are one line each apart from `life_of_mike`'s five.

One thing worth writing down for whoever drives the Runners next: run370's
window is 559x498 and is **not** maximised like the others, so the hardcoded
`(400, 825)` click in `drive_ckpt.sh` landed in its scrollback and silently
mangled every keystroke. `drive_ckpt.sh` now honours `CLICK_X`/`CLICK_Y` and
`runner_savetranscript.sh` sets them per executable. Two smaller traps in the
same session: `marooned` ships an author password, which stops the generators
from opening a probe for upconversion (write the passwordless `    Wild    `
into the field), and a probe object named `box` collides with `arlo`'s static
mailbox — name probe objects something no game would.

### 2026-08-23, second batch — 3.9 does not leak, and the two capacity refusals are 4.0 rewordings

The batch above closed the pre-3.9 half of the carried-load question and left
the 3.9 half where it had always been: on an inference. Scarier kept run400's
running size/weight totals — leaks and all — for *every* game with those two
axes, on the strength of "3.9 has the same two axes", and nothing but that.
Since the pre-3.9 half had just failed the same inference, the 3.9 half was
worth a Runner.

`make_39_leakprobe.py` hand-rolls three 3.90 probes (a bag, a rock and a cape
whose sizes and weights are chosen so each leak shows up as a number that
names its own culprit; object 0 is a never-touched decoy pebble, because every
in-room object writes `Parent 0` and the phantom-weight rule would otherwise
make the whole probe set children of index 0). `gen400.exe` then Saves As 4.00
to give each one a twin, so the same commands run in both Runners.

**`p39leak`, `count` after each command** (limit 90, nothing ever refused, so
this is the readout with no refusal arithmetic in the way):

| | take bag | take rock | put rock in bag | drop bag | take cape | wear cape | drop cape |
|---|---|---|---|---|---|---|---|
| run390 size | 9 | 36 | **9** | **0** | 9 | **9** | **0** |
| run400 size | 9 | 36 | 36 | 27 | 36 | 27 | 18 |
| both weight | 3 | 6 | 6 | 0 | 9 | 9 | 0 |

**run390 does not leak.** Its size counts only what is *directly* held or
worn, own size and not contents; its weight recurses into carried containers.
That is exactly what recomputing gives, and `p39lim` (MaxSize tuned to 30, so
the *check* speaks rather than the *report*) closes the "prints one number,
checks another" loophole: after the put-and-drop, run390 accepts a size-27
crate that our running total refused.

run390 does keep the two globals — it adds to them on a successful take at
`loc_454FDE`/`loc_454FF7` — but it has no equivalent of run400's one generic
setter `Proc_21_54`, adjusting them from each command handler in turn instead,
and its arithmetic comes out exact. So the new `obj_uses_running_load()` is
true for 4.0 alone; 3.9 joins 3.7/3.8 on the recomputing side, which also
gives it the position-filtered membership in `obj_weigh()` (no running total,
nothing for a stale parent to accumulate into) and makes `glk capacity` a
no-op for it.

Two wording divergences fell out of the same probes, and both are things
upstream SCARE inferred rather than measured. SCARE gates a trailing
`" at the moment"` on *portability* — whether empty hands would have taken the
object. No Runner does anything of the kind:

1. **The size refusal is never qualified.** `" hands are full."` is one string
   with the period baked in, in all four binaries (`run370` @436D20, `run380`
   @43E9A0, `run390` @455B34, `run400` @47C83C/@463C30/@46302C/@473A34), and
   run390 answered a flat "Your hands are full." to the `p39lim` refusal that
   our engine qualified. The `is_portable` out-parameter is gone from both
   `lib_object_too_large()` and `lib_object_too_heavy()`.
2. **The weight refusal is a 4.0 rewording, and its suffix is unconditional.**
   Measured on `p39wt` (MaxWt 30; a brick that fits in empty hands and a lump
   that does not), one Runner each:

   * run390: **"That is too heavy for you to carry."** — no object name, no
     suffix, the same sentence for both.
   * run400: **"The lump is too heavy for Player to carry at the moment."** —
     names the object, always qualifies.

   Neither binary has an `" are too heavy"` string, so the plural SCARE
   selected between never existed. `lib_print_too_heavy()` now prints the
   version's single shape.

Ported: `obj_uses_running_load()` in `scobjcts.cpp`, the gate in
`lib_carried_size()`/`lib_carried_weight()` and `obj_weigh()`, and
`lib_print_too_heavy()`. `glk capacity`'s help text said "Only 3.9 and 4.0
games keep such a total" and now says 4.0 alone.

Regression: **261/261 v4 walkthroughs PASS**, headless suite green (capacity
matrix and nest both PASS). Two goldens moved, both 3.90 and both the same
line: `alexis` and `alexis_worn_cube`, where the large knife's refusal is now
"That is too heavy for you to carry." No golden moved for the load model
itself — no walkthrough in the corpus drops a loaded container or cycles a
worn object against a limit tight enough to notice.

Left open, seen in the same transcripts and not chased: run390 and run400 both
print `count` in the third person as **"Player have 0.  The most he can hold
is 90."** — a Runner grammar bug ("have", not "has") plus a gendered pronoun
where we substitute the player's name. Our third person reads "Player has 0.
The most Player can hold is 90." It surfaced only because `gen400`'s
upconversion drops `Perspective`; no third-person game in the corpus exercises
`count`.

### 2026-08-23, third batch — the ending is not output yet, and `relojero` is the game that shows it

`relojero.taf` ("La hija del relojero", 4.00, Spanish, one room, eight tasks)
was replayed through run400 to check the port against a Runner on a game
nobody had measured before. The 11-command solution already in
`goldens/relojero_solution.txt` was typed straight in; the transcript is
`pfx/drive_c/adrift/relojero.txt`.

**Ten of the eleven commands are identical** once the Runner's space padding
is normalised — the two `x` blocks with the long backstory, the `hablar`
task, the drawer, the take, `tirar cuerda`, and the recurring pain-moan
event landing on the same turns in both. The eleventh differs by exactly one
line:

```
arreglar fenix
Disculpa pero no te entiendo.          <- run400 only
Con sumo cuidado ato los dos extremos de la cuerda y tiro suavemente.
```

Strip that line and the rest of the block is character-for-character equal,
1223 bytes each, WINTEXT included.

**Why.** Task 5 (`arreglar *fenix`, alternates `arreglar/unir/atar *cuerda`,
two holding restrictions) has **no CompleteText**; its only action is
`type=6`, and the ending prose lives in WINTEXT. So the task says nothing at
match time, run400 falls through to the library, the library cannot parse a
Spanish verb, and the game's global "don't understand" message prints —
*then* the end-of-turn handler prints WINTEXT. That is the same
silent-match-falls-through rule the ET probes established for the peek work,
and the row above it in §4 records run400 doing it in the 3.x cells too.

Scarier does model the fallthrough — `run_game_commands_common()` returns
`is_handled`, which is set only when `task_run_task()` actually printed — but
`task_run_end_game_action()` returns `var1 != 3`, reporting output for an
action whose own comment says the message is printed later, at the end of the
turn, by `task_print_end_game_message()`. So the command counts as handled
and the library never gets its look.

**The one-line fix was tried and reverted, because it is not the whole fix.**
With `task_run_end_game_action()` returning FALSE the peek does fall through
correctly (`peek=0, prio=0, unrestr=0, restr=0`), but the game has ended by
then, and the library catch-all `* %object% *` → `lib_cmd_verb_object` claims
the command and returns TRUE without printing. Worse, if it *did* print it
would say "I don't understand what you want me to do with the Fenix de
laton." — the verb-object refusal — where run400 gave the **global** one. The
same command with the restrictions unmet (Phoenix still in the drawer, so
`count != 1` in `lib_cmd_verb_object`) does reach the global message in
Scarier, which is what pins the difference on scope rather than on wording.

**The probe that would settle it, not yet taken:** an unrecognised verb with
an unambiguous object *in hand* — does run400 answer with its verb-object
refusal or with the global "don't understand"? One arena task, one command,
one transcript. It governs a whole class of commands, not just this ending,
and it is the missing measurement behind the new §4 row. Until it is taken,
the divergence stays: one cosmetic line, at the very last command of the
game, against a change in the dispatch core.

No engine change, no golden change. v4 corpus 261/261 after reverting the
experiment.

### 2026-09-05 — the 3.9 bracket echo was there all along, and `x me` gets its full stop

Two findings from the first 3.90 replay with a pronoun in it (Archie's
Birthday, run390, 205/205 echoed), both ported the same day, both then
measured a second time on a purpose-built probe.

**The 3.9 pronoun echo.** Scarier printed the `(a X)` reference line for
3.7, 3.8 and 4.0 but not 3.9, on a reading of run390 that had it keeping
`showbrackets` only for the "ask about"/"talk about" rewrite (loc_459036 /
459107).  That reading missed `Sub its()` @43D968: it tests `m_showbrackets`
at loc_43D6C2 (`it`), 43D7BF (`them`) and 43D884 (`one`) and prints
`"(" & MemVar_46811C & ")"` through Proc_2_28_45CBD0, and the menu item is
loaded from `ADRIFT\Runner showbrackets` at loc_44526F with default True
(loc_44526B).  Measured: `x camcorder` then `take it` answers
`(a camcorder)` before `You take a camcorder from the desk.`  Gate removed
in `uip_assign_pronouns()` -- every Runner echoes.

What 3.9 does NOT share with 4.0 is the article.  Its antecedent composer
Proc_2_36_42B0E8 (mode, obj, out) has the same two modes as run400's
448710 (1 = authored Prefix, 0 = tense'd "the"), but `takes` (@455067,
4550F0) and `drops` (@445BE6, 445C6D) call it in mode 1, so a taken object
stays "(a X)".  Measured on `veteran.taf`: `x bag`, `take it`, `open it`
echo `(a bag)` both times where run400 would say `(the bag)` after the
take.  `uip_definite_form()` therefore stays 4.00-only.  Two mode-0 / raw
sites remain unmeasured in 3.9: `co()` @43B69E (mode 0, the
obhere-and-seen branch) and @43B610 / @46031E (bare Short, no article).

**The examine-self full stop.** Archie's PlayerDesc is the ALR key
`[player=%player val%]`; run390 printed the substituted paragraph ending
`...self-delusions.` and Scarier ended it bare.  run390 `examines()`
@44C488, loc_44C1F1-44C21E: after the description and the position clause,
`If Right$(text, 1) <> "." Then text = text & "."` -- on the raw text, so
the key gets the stop and the ALR paragraph inherits it.  run400
`Proc_19_87_471F94`, loc_471C6D-471D40: the same, exempting `!`, `)`, `%`
and `?` as well.  Ported to `lib_cmd_examine_self()` for 3.9+, then the 4.0
half measured directly: a one-command run400 probe on `yak_shaving.taf`
answers `x me` with `You are somewhat raggedy looking after your journey.`
for a PlayerDesc with no stop.

Not ported: 3.7/3.8.  Their `examines` (run370 @435C9C, run380 @43D5EC)
never touch the PlayerDesc text -- the player branch (run380 @43D3EC, run370
@435A9B, `c("me") Or c("myself")` inside the empty-message fallback; dark
room "can just make out that you are okay." @43D5C1/@435C70) is "as well
as can be expected" + position + an unconditional `"."` (@43D53B/@435BEA)
-- which, read literally,
answers a plain `x me` with a lone `.`.  That is too odd to port from
P-code; it needs a run380 probe (`x me` on any 3.80 game, plus a `look`
so the .rtf holds it).  Corpus exposure today is nil: no 3.80 game in
test/adrift4/games has a PlayerDesc, and none of the 13 goldens that
examine the player is pre-3.9 (11 are 4.00, `archie` and `cybercow_win`
3.90).

Goldens: `archie`, `veteran`, `cruel` gain bracket lines, `archie` and
`yak_shaving` the full stop; four re-blessed, v4 corpus 428/428.
