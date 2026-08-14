# ADRIFT Runner fidelity: arbitration record

Every question Scarier has taken to the **real** ADRIFT Runners, and how it was
answered. Companion to `ADRIFT4_vs_ADRIFT5.md` (which records semantics already
settled) and `test/adrift4/notes/WALKTHROUGH_TODO.md` (which is about route
derivation, not engine fidelity).

**Nothing here is an open task any more.** The file kept its `_TODO` name for the
sake of the several dozen code comments that cite it, but every numbered section
below closed between 2026-08-01 and 2026-08-09, and the `## Closure log` section
at the foot is the record of *how* each one closed, not a plan. What is still
live is the method — how to stage a probe against each
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
- Probe games: hand-author a one-restriction / one-task variant, repack with
  `test/adrift4/harness/taftool.py` or the Runner rejects it. Recipe and
  field offsets in the memory `scare-restriction-statics-run400`.
- Read transcripts with `screencapture -x -o -l<winID>`; classify cheaply by ink
  pixel count in the response band rather than OCR.
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
objects; the M3/SP*/TS/RC configs are inline) and **`test/adrift4/harness/make_39_probe.py`**
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
  (`~/adrift-battle/decompiled/`, Proc_11_1/Proc_11_2): armed hit =
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
      misread: `Sub_20_74` is a command-reference / exit-scope filter, not the
      task room gate, and its "where-type 0" branch is really *reference*-type 0.
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

Every row is settled: measured live, ported, deliberately kept, or refuted. The
one exception is *Restriction evaluation order*, which rests on the P-code alone
because no ADRIFT 4 restriction has a side effect, so there is nothing to probe.
This table is the live part of the file — keep it current.

| Item | Scarier | Runner | Status |
|---|---|---|---|
| Negated `Var2` inside the any/no-object quantifier | negates once around the whole quantifier (the meaningful reading) | its per-object switch handles `Var2` 0–5 only, so "any" always fails and "no" always passes | **Deliberate, confirmed live.** No corpus game authors it. Keep. |
| Dynamic-object index past the end (`Var1 ≥ 3 + ndynamics`) | clamps to the last object | raises "Subscript out of range" and dies | **Deliberate.** Unreachable in any shipped game. Keep. |
| Body-part statics in a `Var1 = 2` restriction | positioned at `OBJ_PART_NPC` | ~~statics have no location field, so they read hidden~~ **theory refuted live** — the Runner answers exactly like Scarier | **Settled 2026-08-01, NO divergence** (probes `pBP`/`pBP2` in `make_arena_probe.py`): with the parent NPC present, is-hidden FAILS, visible-to PASSES, not-hidden PASSES — for an NPC part and a player part alike, byte-identical to Scarier; visible-to tracks the parent NPC's room. With the parent absent, the Runner's `%object%` scope filter refuses the part ("I don't understand.") — that is the separate scope-filter row below, not a body-part issue. |
| Object scope when matching a task command | `uip_match_entity()` has no scope filter at all — matches anything | won't match an object that isn't present ("I don't understand what you mean!") | **Confirmed divergence, unfixed — corpus impact measured 2026-08-01: ZERO.** `SCR_TRACE_SCOPE=1` (scrunner.cpp, needs `SCARIER_DUMP_TOOLS`) logs any golden turn where a matched `%object%` task binds an absent object (SCOPE-MISS = Runner would refuse the command; SCOPE-BIND = Runner would bind a different, present object): all 76 rows replayed clean, zero hits. Static exposure is small too — only 5 games author `%object%` task commands at all (adriftorama 67, goldilocks 20, Screen Savers 19, SRSintro 2, X-Files 2). So the divergence is reachable only by off-route input. A faithful fix means porting `Sub_20_74`'s scope rules (the Runner's command-reference filter — presence for objects, and per the `pWS` probe *seen-ness* for `%character%`), plus a disambiguation rule for present-vs-absent name clashes; not worth it until some game demands it. |
| Optional `{word}` whose look-ahead fails after consuming text | ~~`uip_match_optional()` fell into `uip_match_alternatives()` at the position the *failed* look-ahead had already advanced to~~ **FIXED 2026-08-03**: rewind to `start_posn` on the failure path too | matches | **Was a real divergence, now fixed.** Ground truth is the author's own published transcript of *Monsters (Release 2)*, which shows `shine flashlight on the brainsucker` working. Task 2's pattern is `[defeat/shine/turn/put] {the} [flashlight/light] {on} {the} {brainsucker} {brain}  {monster}`; the look-ahead from `{brainsucker}` let `{brain}` eat the first five letters of "brainsucker" (`uip_match_word()` is a prefix compare with no word-boundary check), failed on the trailing "sucker", and — because `uip_match_list()` has no backtracking of its own — the alternatives were then tried from "sucker". Cost the game 5 of its 40 points. `uip_match_wildcard()` always restored position on failure; only the optional matcher didn't. Zero golden churn across the 154-row v4 suite. |
| A `/` outside any `[]`/`{}` group | ~~`uip_parse_list()` treated it as an alternatives separator *at every depth*, so the top-level list returned early — **without appending its `NODE_EOS`** — and the pattern degenerated to everything before the first slash, prefix-matching any input that started with it~~ **FIXED 2026-08-04**: a group-depth counter; at depth 0 the `/` parses as a literal word node instead | a bare `/` is an ordinary literal character — `Proc_9_4_45D940` (the whole of run400's command matcher, decompiled in `~/adrift-battle/decompiled/NewParse.bas`) only ever looks for `/` *between* `[]`/`{}` delimiters: it dispatches on `Left(pattern,1)` being `[` or `{`, and any other leading run is compared with a straight `Left(input,n) = Left(pattern,n)` up to the next `[`/`{` | **Was a real divergence, now fixed.** Found in *Ba'Roo!*: TASK 62's `take/get/eat stew` collapsed to `take`, so **every** `take X` in the Communal Dining Hall was answered "Unfortunately you don't have a way to eat it…" and TASK 63 (`[take/pull/tear] [meat/animal/roast]`) was unreachable — which blocks the walkthrough, since the meat is the game's only food and the endgame climb needs it. In run400 the pattern matches only the literal string `take/get/eat stew`, i.e. the task is dead, which is what the author's own transcript shows. Corpus exposure is small and now measured: 8 games author a bare top-level `/` in 36 task commands, and all but two are `#`-style labels or dashes (`-2/3/4`, `1 - kridlor66 who killed you/how died`) that were silently prefix-matching. Zero golden churn across the 190-row v4 suite. |
| Input synonyms whose replacement is itself synonymed | ~~`pf_filter_input()` walked the input word by word, took the **first** synonym that matched, spliced it in and skipped past it — no later synonym ever saw the text it wrote~~ **FIXED 2026-08-04**: after one fires, the rest of the list still gets a look at the replacement, but only *as a whole* — a later synonym fires again only when its original is the entire replacement region (`extent == span`), and then replaces all of it | later synonyms do act on an earlier one's output, but only on the whole of it | **Was a real divergence, now fixed; both halves of the rule are pinned by a game, each read off run400 under Wine.** *Lair of the Vampire* maps `harris`→`steve` **and** `steve`→`harris`, the author's way of letting both spellings reach one NPC; run400 accepts `ask harris about key` (the game's own walkthrough opens with it) and `x harris`, whereas first-match-wins left a `steve` the character has no alias for and made the cellmate — who holds the picklock the whole game hinges on — unaddressable. The whole-region half comes from *Yak Shaving for Kicks and Giggles!*, which maps `flags`, then `line`, then `clothes` all onto `clothes line`: run400 answers `x flags` with the laundry description, so `line`/`clothes` must **not** fire on the words *inside* the `clothes line` the first synonym wrote. Letting them (the naive "apply all that match") is not merely wrong but non-terminating — `x flags` grows one `clothes line` per pass and the run dies on the harness's 30 s `ulimit -t`. An intermediate model, one whole-string pass per synonym in list order, terminates and fixes Lair but garbles Yak to `x clothes line clothes line line` → "Either that isn't here, or it's not important."; it was falsified against the Runner and abandoned. Zero golden churn across the 190-row v4 suite. |
| `%object%` given a *partial* prefix | ~~only `"Prefix Short"` and the bare `Short` were matchable~~ **FIXED 2026-08-03**: `uip_build_candidate()` also stores the prefix with its leading words dropped one at a time | matches a partial prefix | **Was a real divergence, now fixed, with two independent transcripts as ground truth.** *Monsters (Release 2)*: Prefix `Sissy's four poster` + Short `bed`, and the transcript prints the description for `examine the four poster bed` where Scarier said "I see no such thing". Re-running the suite then changed exactly one line in one other golden — *Shadrick's Travels*, `climb oak tree`, "You can't climb that." → "You can't climb the old oak tree." — which is verbatim what line 80 of *that* game's upstream transcript says. The Short itself is never cut down, so a multi-word Short must still be given whole. `SCR_DUMP_TASKS`'s `OBJNAME` line now prints `prefix=[...]` and each `alias=[...]` so this class of failure is a lookup rather than a guess. |
| 3.9 shoot-Method strength | version-gated: 3.9 adds `HitValue` to base Str, 4.0 replaces | both confirmed live (run390 one-shot / run400 two hits) | **Fixed 2026-08-01** (`7a4cb7c2`). |
| Upgraded-3.9 combat | `SCR_ASSUME_COMBAT` opt-in; matches author intent | **stalemates, confirmed live 2026-08-01** (Azra: converted acc/agi all 0-0) | Settled — opt-in stays. |
| Restriction evaluation order | evaluates all, no short-circuit | `Sub_20_65` replaces `#` with T/F in a bool-expr string, so it can't short-circuit either | Believed matched (ADRIFT 4 restrictions cannot have side effects — no restriction type mutates state — so "verify a side effect runs" is unprobeable and moot; the P-code reading stands on its own). |
| Integer division rounding | `Round((a/b) + 0.000001)` — banker's rounding with a +∞-biased epsilon (scexpr.cpp's asymmetric compare) | same | **Confirmed live 2026-08-01** (probes `pDIV`/`pDIV2` in `make_arena_probe.py`, which now authors variables and set-var actions): with *true* negative dividends (via `%v1%/2`), run400 answers −5/2 = −2, −7/2 = −3, −1/2 = 0, and 5/2 = 3, 7/2 = 4, 1/2 = 1, 22/7 = 3 — byte-identical to Scarier. Only 5 corpus games author expressions at all (47 exprs; `circus` has the only divisions, positive). |
| Unary minus in expressions | folded into the literal: `-5/2` = (−5)/2 = **−2** | tokenised as an *operator* that reduces after `/`: `-5/2` = 0−(5/2) = 0−3 = **−3** | **NEW divergence, found 2026-08-01 by `pDIV` (its only diverging cell — the `pDIV2` variable forms all agree, which is what pins the cause to the tokeniser, not the rounding).** Zero corpus exposure: none of the 47 authored expressions uses a unary minus (`SCR_DUMP_TASKS` now prints `expr=[...]` on type-3 ACT lines). Documented, not fixed — reshaping scexpr's parser to give unary minus binary-minus precedence risks more than it buys. ~~Open tangent: ADRIFT 5 shares this token engine, so a5sexpr's literal `-5/2` deserves the same one-probe check.~~ **Probed 2026-08-01: NO divergence on the ADRIFT 5 side.** A 44-row battery through the REAL `clsVariable.SetToExpression` (scratch C# driver against the FrankenDrift.Adrift Release dll — no adventure loaded, bare `clsAdventure` + registered vars) matches a5sexpr row-for-row on both the raw string and the `SafeInt(Val())` readback. clsVariable *does* tokenise leading `-` as an operator (`GetToken` clsVariable.vb:134) and reduces the dangling `op expr` pair on run 2 (clsVariable.vb:959-972), after `/` rounded on run 1 — but v5's `Math.Round(AwayFromZero)` is symmetric (`round(-q) == -round(q)`), so the operator parse and a5sexpr's folded parse coincide everywhere, including the var-token vs textual-substitution split (`%v1%/2` with v1=−5). The v4 divergence exists only because run400's `+0.000001` epsilon rounding is asymmetric. Sole divergent row: `-2^-1` (= −0.5) reads back −1 in FD under an English locale (`SafeInt` = VB `Int()` floor) vs 0 from scarier's strtol — and even the real runner is locale-dependent there (comma-decimal `Val("-0,5")` → 0). Kept as-is: fractional results need `^` with a negative outcome, zero corpus exposure. Battery banked as unary-minus cases in `test/adrift5/harness/a5sexpr_test.cpp`. |
| Combat RNG | own generator | VB6 `Rnd`, `Randomize Timer` on the load path | Won't-fix confirmed live (§1): per-turn combat differs across identical fresh sessions. |
| Battle messages | second person ("you"/"your") | run400 uses player's name with 2nd-person verb forms ("Player manage to avoid…"); run390 uses second person | Presentational only; "you" kept (matches run390). Method-verb weapon narration **ported 2026-08-01**, verified live in BOTH Runners; noted §1 surface facts. |
| Wield model | ~~per-attack default~~ **PORTED 2026-08-01**: persistent wield ref, matching the Runner | persistent wield ref; single-held auto-select persists; ASKS with 2+ held ("What do you want to attack X with?"); NO `unwield` verb; drop clears to nothing; status folds only the actual wield | **Fully settled live 2026-08-01** (probe `pWS`) and **ported the same day**. Corpus fallout (found late — stale-binary erratum, see §1): 3 wording goldens + the Shadowpeak routes' post-chapel bare attacks, all repaired/re-blessed. ~~Remaining cosmetic gaps: status layout (Max column, "(bonus)" parens), "not holding" wording.~~ **Cosmetics ported 2026-08-01** (probe `pWS2`; see §1): four-column status table (Range/Max/Current + equipment share in parens, Stamina row = live/max/live, no "You have:" header, article-prefix wielding line), wield refusal "aren't carrying …!" vs attack-with "is not carrying …!". |
| Thrown (method 5) weapon | drop + version-split damage ported | moves to the room on a player throw in BOTH Runners; damage = Str-only in run400 (HitValue ignored), Str+HitValue in run390 | **Confirmed live in both Runners and PORTED 2026-08-01** (probes `pTD`/`p39td`; see §1 surface facts). `light_up` route re-derived. |
| Enemy target selection | was pinned to one target per session (LCG low-bit + `% range`) | uniform per-turn pick among ally/player | **Fixed 2026-08-01** (`scr_randomint` multiply-shift); corpus re-blessed. |
| Event TaskAffected execution | version-gated: 3.9 = matcher dispatch (wildcard steal, silent restricted skip), 4.0 = direct run (loud FailMessage) | run390 and run400 genuinely differ — same converted probe, opposite behavior | **Fixed 2026-08-01** (§2); both halves verified live. |
| Named `drop` of a worn item | implicitly removes then drops (named only; `drop all` skips worn) | same, in BOTH Runners | **Fixed 2026-08-01** (`lib_drop_named_filter`). |
| Completed non-repeatable `*` task | skipped by matcher and event dispatch | claims every later command: "You have already done that." — soft-locks inverness for real | **Deliberate divergence.** Do not import; see §2. |
| `drop <thing> in/on <container>` | ~~the priority `drop %text%` pattern swallowed the `in <container>` tail and answered "Drop what?"~~ **FIXED 2026-08-02** | routes it to the put-in / put-on handlers: `drop wallet in bin` → "You put your wallet inside the rubbish bin.", `drop wallet on bin` → the put-on refusal "You can't put anything onto the rubbish bin!" | **Confirmed live against run400 and FIXED 2026-08-02** while deriving `Ticket to No Where`, whose author-route disposes of five bits of litter with `drop <litter> in bin` (2 points each — the difference between 100 and its full 110). Six patterns added to `PRIORITY_COMMANDS[]` (`scrunner.cpp`), covering `drop`/`put down` × `in`/`on` × plain/`all`/`all except`, placed *before* the plain drop patterns so the `%text%` no longer eats the tail. No corpus fallout. |
| What `all` ranges over | ~~everything a named take can reach, including the contents of a carried open container~~ **FIXED 2026-08-02** | leaves alone anything already in the player's possession; a *named* take still reaches into a carried open container | **Confirmed live against run400 and FIXED 2026-08-02.** In `Ticket to No Where`, holding the open bag of shopping and typing `get all` answers "You take the pamphlet." and leaves the tights, pet food, deodorant and gloves in the bag, while `get paper` still lifts the scrap out of the carried wallet ("You take the scrap of paper from your wallet."). `lib_take_all_filter()` (`sclibrar.cpp`) = `lib_take_filter && !obj_indirectly_held_by_player`, used by `lib_cmd_take_all` and by the `take all except` resolver in `lib_take_multiple_common`. The visible symptom was four bogus items of inventory weight later refusing `get banana skin`. Corpus fallout: the two ALEXIS goldens, re-blessed after proving the change correct there too (its leather bag is player-carried, so the Runner would never have emptied it either). |
| "get all" with nothing takeable | "There is nothing to pick up here." | "There is nothing worth taking here." | **Cosmetic divergence, observed live 2026-08-02, deliberately unfixed.** Wording only; changing it would churn goldens across the corpus for no behavioural gain. |
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
| Put-family precedence, 3.9 half | same port, ungated | **run390 agrees with run400**: `put pill in cup` with a matched-but-failing task runs the library put ("You put the pill inside the cup.", fail message suppressed) | **Verified live 2026-08-02** (`make_39_fwprobe.py`): the ungated port is faithful on both sides. |
| Which of the two container-listing styles a container gets | ~~postfixed ("*An umbrella is inside the umbrella stand.*") only for a **dynamic** container holding exactly one object, or one that is part of an NPC; everything else prefixed ("*Inside X is …*") — recorded in `lib_list_in_object()` as "frankly, a mystery"~~ **FIXED 2026-08-03** | purely a **count**: 1 or 2 contained objects → postfixed, 3+ → prefixed, with **no static-vs-dynamic test anywhere in the chain** | **Derived from run400.txt and confirmed against a real Runner transcript 2026-08-03**, while wiring *It's Easter, Peeps!* — see the dated note below. |
| `take <object lying loose in the room>` | "You pick up the creme egg." | run400 (transcript): "You **take** the creme egg." — both agree on the container case, "You take the lollipop from the newspaper rack." | **Documented, not fixed 2026-08-03.** Probably the same version split as the "Single-object library success vs failing explicit-verb task" row above, where run390 was probed live and answered "You pick up the rock." run400 carries both templates in separate handlers (`0007B652` builds `pick up` + "There is nothing to pick up here."; `00073402` builds `take … from …` + "Take what?" / "There is nothing worth taking here."), so which one a bare `take`/`get` reaches needs a live run400 pair before anything moves — 37 goldens carry 128 "You pick up …" lines. |
| What `g` / `again` echoes | only the implicit-tool line, `(with umbrella)` | the whole expanded command, `(hit pinata with umbrella)` | **Cosmetic divergence, seen in a run400 transcript 2026-08-03, unfixed.** Semantics are identical (`g` really is *again*; see the `scare-g-means-get` note — the Runner's Auto complete once faked a `g` divergence that wasn't there). Only the echo text differs. |
| End-of-game score summary | nothing — the transcript simply stops after the game's own ending text | prints a summary of its own after it: `You scored` N ` out of the maximum` M, a `That is` P `% of the game!` line, and `Well done - you scored maximum points!` at 100%; there is also a `You finished ` … ` points short.` pair | **Documented, not fixed 2026-08-03.** All five fragments are UTF-16 literals in **both** binaries (run390 `0xfd38`/`0xfd64`/`0xfda4`/`0x146ec`/`0x14a18`, run400 `0x14830`/`0x1484c`/`0x14884`/`0x146ec`/`0x148a8`), so this is not a version split; the in-game `score` command's separate ` out of a maximum of ` template is present too and Scarier already matches that one. Observed live: run390 playing `thetest` to the end printed "Well done!  You won!  …" *and then* "You scored 20 out of the maximum 25!", where our `thetest_win_solution` golden ends at the game's own text (see `test/adrift4/notes/thetest_walkthrough.md`). **Not implemented deliberately** — it would append two or three lines to every winning golden in the corpus (60+ rows) for no behavioural gain, and the exact assembly of the percentage line (rounding, whether it is always printed, what triggers the "points short" pair) has never been captured live; pin those first if this is ever ported. |
| TAF 3.8 object "Size/weight" class | **now modelled**: the class is kept verbatim in `SizeWeightClass` and enforced as a pooled burden (`obj_get_burden` / `obj_get_player_burden_limit`, `scobjcts.cpp`), while `SizeWeight` stays normalised to 4.0 "normal" (`22`) so a container's `Capacity*10+2` remains the plain **object count** 3.8 meant it to be (`\|V380_OBJECT:_SizeWeight_\|` in `sctafpar.cpp`) | **SETTLED against the genuine `run380.exe` 2026-08-03: a single pooled burden with per-class costs `0→1 1→3 2→7 3→3 4→7`, and a capacity of exactly `MaxCarried`.** Neither Scarier's normalisation (every class costs 1) nor gen390's `0→22 1→23 2→24 3→32 4→42` matches it. | **Divergence CONFIRMED, measured, and FIXED 2026-08-03.** `run380.exe` is *not* lost: David Whyld's dead delron.org.uk still serves `adrift38.zip` through the Wayback Machine, and it is installed in the adrift-battle Wine prefix (`~/adrift-battle/runner/wine/README.md`). Probe method: a 3.80 `.taf` is plaintext CRLF fields XOR'd with the VB6 PRNG from seed `0x00a09e86` — no length header, no zlib, no "Wild" trailer — so it round-trips losslessly through a codec, and a probe builder patches `#MaxCarried` (line after `$GameAuthor`), `#StartRoom` (line after the first `**`, a **direct 0-based room index**, unlike objects' `+3`) and any object's `#SizeWeight` (its short-name line **+11**). (The scripts used for *this* measurement — `dec38.py`, `mkprobe2.py` — were session scratch and were never committed; the surviving, better tools for the same job are `~/adrift-battle/runner/wine/taf38schema.py` and `make38probe.py`, which patch by field *name* rather than by hand-counted line offset. Use those.) Pinned at MaxCarried 1/2/3/6/7/8 in the real Runner: at 2 a class-1 or class-3 object is refused and a class-0 accepted, at 3 both go; at 6 Marooned's tires (class 4) are refused, at 7 they are accepted **alone**, at 8 tires+map fit and tires+flint+map do not, and tires + a class-2 gas can never do — which also disproves the two-axis reading, since a separate size and weight axis would have let the two heavies coexist. Cross-checked on the corpus's other 3.8 game: `Crime_Adventure.taf` (MaxCarried 5) refuses `get kettle` (class 2 = 7) **with empty hands** while its five class-0 kitchen items all fit, so that game's "Get all the stuff in Fenwick kitchen" hint is an author fault, not a conversion fault. gen390's table is therefore *directionally* right and wrong by one step — the 4.0 packed `base^digit` model cannot express 7, so the top class rounds to `3^2 = 9` against a limit of 8, which is exactly why converted 3.8 games stop being finishable. **The fix:** the object fixup now also writes `SizeWeightClass`, the globals fixup raises `Globals.BurdenModel`, and `scobjcts.cpp` gains the 1/3/7 cost table plus a `MaxCarried` limit. `sclibrar.cpp` routes every take through the pooled sum: `lib_object_too_large()` performs the check (3.8's only refusal is "Your hands are full.") and `lib_object_too_heavy()` stands down entirely, since 3.8 has no "too heavy" message. The 4.0 `MaxSize`/`MaxWt` pair is still written — the save serialiser reads it, and because every class costs ≥ 1 the pooled burden can never be looser than the object-count limit the size axis enforces, so those checks are subsumed and can never fire first. **3.8 containers measured 2026-08-03, closing the one gap this row left open.** Three answers, all from `run380` with probes patched through a real schema parser (`~/adrift-battle/runner/wine/taf38schema.py`, which walks `V380_PARSE_SCHEMA` and records each field's line index, so probes patch by *name*): (1) **a carried container's contents are free** — against `MaxCarried` 1, a class-0 box (cost 1) holding a class-4 object (cost 7) is picked up and only the *next* cost-1 object is refused, where charging the contents would have made the box alone cost 8; (2) **`Capacity` is a plain object count** and the Size/weight class is charged against it no more than against the carrier — a Capacity of 2 swallows a class-4 then a class-1, and two class-0 objects fill it; (3) a **`Capacity` of 0 is full from the start**, not unlimited. So Scarier's arithmetic on both axes was already right, and the normalisation to `22` is vindicated. What was wrong was the wording and one rule: 3.8 says a flat **"Your hands are full."** with no " at the moment" (`lib_object_too_large` now reports 3.8 objects as unportable to suppress the suffix), it answers **"The box is full."** to *every* container refusal rather than 4.0's "too big to fit inside"/"can't fit inside … at the moment" pair (`lib_put_in_backend` consumes the leftovers under the burden model and prints it once), and it **only fills a dynamic container the player is holding** — "You are not holding a saucepan.", with the object's own prefix rather than the usual "the" (`lib_put_in_is_valid`). Static containers are exempt, which matters: `Wrecked`'s static red locker takes a coin where it stands, and nothing could ever pick one up. That last rule cost `Crime_Adventure` its route — its stew was loaded into a saucepan on the kitchen floor, which `run380` refuses **in that very game** (verified directly, not just in a synthetic probe) — so the route now takes the saucepan first and the later redundant `get saucepan` is gone; same 75/95 win, re-blessed. Corpus impact: the two 3.80 games with routes both needed new ones — `marooned`'s single-trip route broke at `get map` and is now a four-trip ferrying route, and `wrecked` needed two drops (`drop card`, `drop glass`) against its limit of 10; both re-blessed and PASSing, full corpus back to 163 PASS / exit 0. See `test/adrift4/notes/Marooned_walkthrough.md`. |
| A matched task whose restrictions FAIL swallows the command (no fall-through to movement) | prints the task's FailMessage and ends the turn, even when the FailMessage is a one-character placeholder and the room has an exit in that direction | **identical** | **Settled 2026-08-03, NO divergence** (`wrecked.taf`, TAF 3.80, via a gen390 conversion under Wine). Task 96 (`in pub with scuba` / `in`) is Campbell Wild's "you can't come in looking like that" blocker, and its FailMessage is the literal placeholder `x`; once the outfit is off, `run390.exe` prints just `x` and refuses entry, exactly as Scarier does. Task 84 at the Post Office roof (`climb *roof*`, alts `up` / `u` / `get *roof*` / `go *roof`, restricted on task 83 `climb *statue*` **not** done) has the same shape, and gen390 re-encodes its restriction byte-identically to our parse (`RESTR type=2 v1=84 v2=1`). Both are escapable only because `go in` and `go up` are absent from the tasks' command lists, so nothing matches and the movement runs — also confirmed live for `go in`. Nothing to fix; recorded so the next `x`-shaped mystery is not re-investigated. |

| ADRIFT 4 `$RestrMask` operator precedence | ~~C precedence: an OR-expression over AND-expressions, both left-associative~~ **now equal precedence, left-associative** | `A` and `O` have **equal** precedence and associate to the **LEFT**: `#O#A#` is `(1 OR 2) AND 3`, never `1 OR (2 AND 3)` | **Divergence found and FIXED 2026-08-03** (`screstrs.cpp`, `restr_expr()`). Ground truth is run400's own `mdlSpreadTheLoad.Sub_20_57` ("evaluaterestrictions", `00055CAC..00055EB9`): it scans the mask for the last top-level `A`/`O`, evaluates the tail operand, and recurses on the **head** — peeling from the right and recursing left is *left* association, and `A`/`O` are two arms of a single `If`, so there is no second precedence level anywhere in the routine. `Sub_20_58` ("evaluate2") has the same shape for its annotated T/F display string, and the driver `Sub_20_65` substitutes T/F for every restriction in index order with **no short circuit**. The old parse agrees whenever every `A` precedes every `O` at a bracket level and differs the moment an `O` comes first. **20 corpus games author a mixed level** (measured 2026-08-03 by a `maskscan.py` scratch script that is not committed — the list below *is* the result; to re-measure, scan every task's `$RestrMask` for a level containing both an `A` and an `O`): unauthorized (30 tasks), iqsfot (11), unravel (7), humbug (5), cursed (4), the_pk_girl / Vendetta / yonastoundingcastle / 3monkeys (3 each), EscapeToNewYork / The Plague - Redux (2), and one each in ARGH_sGreatEscape, DragonShrineR43, Glum Fiddle, Main Course, TheSisters, Trabula, mishmash, ticket. Found through *Three Monkeys One Cage*, whose author-written `winnable` self-check (T21, 55 restrictions — the corpus maximum) reported "no longer winnable" from turn 1: its group `#O(#A#)A#` is "(the bucket is on the hook OR the coconut is set up) AND the gate is still shut", which the C parse read as "bucket OR (coconut AND gate)" and so answered TRUE with the gate already open. Whole v4 suite re-run after the fix: **161/161 PASS, no golden moved.** A live run400 confirmation is still possible (`3monkeys.taf` is staged in the Wine prefix) but the P-code is unambiguous. |

| A task command typed **outside** the task's `Where` rooms | ~~no message of any kind: `task_can_run_task()` returned FALSE and the command fell through to the standard library or to "I don't understand."~~ **PORTED 2026-08-10** | **pre-4.0 only**: prints `You can't do that here.` (3.7/3.8) / `You can't do that here!` (3.9) and **consumes a turn**; run400 dropped the message and answers DontUnderstand | **Divergence measured live and FIXED 2026-08-10.** See §5's follow-up below for the probe and the full cell table. Condition is narrow: the task's command must match and the room list must be the *only* thing blocking it — a failing restriction gets DontUnderstand instead, and anything the standard library already handled wins outright. Implemented as `run_where_refusal()` (`scrunner.cpp`), last in `run_all_commands()`, over the new `task_is_room_refused()` predicate (`sctasks.cpp`). Leading word follows `Globals/Perspective`: "I" for first person, "You" otherwise (pre-4.0 has only the two — run390 says "You" for 1, 2 and 3 alike). **Zero corpus movement: 203/203 PASS, nothing re-blessed** — a solved route never types a task command in a room the task can't run in, which is why the feature carries its own synthetic regression (`make -f Makefile.headless wheretest`, two probe games covering both perspectives, part of `make test`). |
| A **completed non-repeatable** task, typed again by its own command, **empty RepeatText** | ~~"I don't understand." (or whatever the library says) — the matcher skips the done task and the input falls through~~ **PORTED 2026-08-10** | **pre-4.0 only**: `You have already done that.`, and it **consumes a turn**; run400 has no such string and answers DontUnderstand | **Divergence measured live and FIXED 2026-08-10.** Found alongside the `Where` refusal, on the same 3.90 probe (task `epsilon`, `Where` = all rooms, not repeatable, empty RepeatText, typed twice: run390 fires it, then answers "You have already done that." with the ticking event still ticking). The string is a UTF-16 literal in run370/run380/run390 and **absent from run400** — the same pre-4.0-only pattern as the row above; the leading space in ` have already done that.` proves it is concatenated with the perspective pronoun, exactly like the room refusal. Still distinct from the `*`-wildcard row further up: that one is about a done wildcard task *claiming every later command* and soft-locking `inverness`, and stays a deliberate divergence. This row is the narrow exact-command case, and the port keeps the distinction — the refusal runs last in `run_all_commands()`, so movement and library commands are answered first. See §5's follow-up. |
| A **completed non-repeatable** task, typed again by its own command, **non-empty RepeatText** | ~~"I don't understand." in every version — the done task never reached the RepeatText branch~~ **PORTED 2026-08-10** | prints the task's **RepeatText**, and it consumes a turn — in **every version, 4.0 included** | **NEW divergence, measured live 2026-08-10 and FIXED the same day.** run400's answer to `epsilon` (no RepeatText) is DontUnderstand but to a twin task *with* one is the RepeatText itself, so the RepeatText half of this behaviour survived into 4.0 even though the bare message did not — implemented ungated, with only the bare message gated on `version < TAF_VERSION_400`. The one part of this family with real corpus exposure: **62 of 196** v4 corpus games author at least one non-repeatable task with a RepeatText, **528 such tasks** in total (`SCR_DUMP_TASKS=1`, new `rpt=` column). No golden moved regardless — a solved route does not re-type a completed one-shot task. |
| Perspective 2 (third person) in a pre-4.0 game | ~~renders third person — inventory reads "Player is carrying nothing.", and the capacity probe narrates "Player puts the b1 inside the c52t."~~ **PORTED 2026-08-10** | **pre-4.0 has only two perspectives**: run390 answers second person for `Globals/Perspective` 1, 2 **and** 3 — "You are carrying nothing.", "You put the b1 inside the c52t." — and first person only for 0 | **Divergence observed live 2026-08-10 and FIXED the same day.** Noticed while pinning which word the `Where` refusal leads with (the refusal itself was already correct — it follows the same two-way split), and independently on the 3.9 capacity probe, which is authored Perspective 2. Ported as `lib_get_perspective()` (`sclibrar.cpp`), which returns `LIB_SECOND_PERSON` for any non-zero Perspective when `version < TAF_VERSION_400`; the two switch sites that render a person (`lib_select_response()`, `lib_nothing_happens_common()`) now read through it, so no third-person string had to be version-gated one by one. Out-of-range values reach the existing error branch for 4.0 only — pre-4.0 they are simply second person, which is what run390 does with 3. **Corpus census** (new `GAME version= perspective=` dump line): of 192 measurable games, **not one pre-4.0 game authors Perspective 2** — the three that do (*Main Course*, *iqsfot*, *yonastoundingcastle*) are all 4.0 and keep their third person, verified live ("SoMorph hits, but nothing happens."). So the 203 walkthrough goldens did not move. The one thing that did: the **capacity probe goldens**, 144 lines each of "Player picks up" → "You pick up" / "Player puts … inside" → "You put … inside" — moving them **onto** the run390 transcript recorded in §"container capacity", not away from it. |
| Do a task's remaining actions run after an action that ends the game? | ~~doc comment claimed "if any action ends the game, return immediately"~~ — the loop never did that: it **ran** the remaining actions with the print filter muted, and the doc comment was simply stale (fixed 2026-08-09). The one real divergence, **PORTED 2026-08-09**: a trailing Execute-Task action used to be dispatched too, so its callee's score and state changes landed. | **in-line actions after the ending still run** — a task whose actions are `end game` then `+7 points` finishes on 7 out of 7 — but an **Execute Task** action after the ending is a **no-op** | **Settled live 2026-08-09** with a new `EG` arena probe (`make_arena_probe.py`), read off run400's own end-of-game summary with `MaxScore=7` so a trailing `+7` shows as "100% of the game! / Well done" and a dropped one as "0%". One ending per session, so one Runner launch per cell: `scorefirst` (`+7`, `end`) → **7/7**, the control proving the summary reports the engine score; `scorelast` (`end`, `+7`) → **7/7** — *this is the answer: the trailing action runs*; `execlast` (`exec` a task that ends the game, then `+7`) → **7/7**, Three Monkeys' actual shape; `printfirst` (`exec` a task that prints and scores, then `end`) → **7/7** with the callee's text shown; `printlast` (`end`, then that same `exec`) → **0/7 with no text at all**, which is what pinned the divergence — the callee is demonstrably fine, so the Runner is dropping the dispatch, not muting it. Scarier agreed on the first four and awarded the 7 on the fifth, and all five cells now match. **Mechanism confirmed in the P-code 2026-08-09**, answering the question the probe alone could not — the Runner refuses the *dispatch*, not the callee's completion. The chain is `Sub_20_22` (RunTask) → `Sub_20_12` (mark complete) → `Sub_20_11` (run actions) → back to `Sub_20_22` for a type-5 action. **`mdlSpreadTheLoad.Sub_20_22` opens, at file offset `0005F750`, with `ImpAdLdUI1 <gameover> / CI2UI1 / LitI2_Byte 0 / GtI2 / BranchF / ExitProc`** — `If gameOver > 0 Then Exit Sub`, ahead of restrictions, CompleteText and actions alike. The type-5 branch of the action executor `Sub_20_11` (`@0008D588`, forwards arm `@0008D5AC`) calls it at `@0008D5D1` **unguarded**, bracketing the call only with stores of `0`/`1` to an unrelated global. The action *loop* has no gameover test at all: `Sub_20_11` reads that flag exactly **once**, at `@0008D621` inside the EndGame (type-6) handler, where it guards the ending *display* (`Sub_20_33`) against re-printing — which is precisely why in-line actions behind an ending still run. Same-variable proof: the flag is import slot `0x7E` in `mdlSpreadTheLoad` and `0x4F` in the form modules (P32Dasm does not print `ImpAd*` operands; read the 2 bytes after the `fd a0`/`fd b0` opcode at the listing address, which is a file offset into `run400.exe`), the load/store census splits perfectly along module lines, and the EndGame action's Var1→flag mapping (`0→1`, `1→3`, `2→2`, `3→4`, at `@08D694`/`@08D6AB`/`@08D6C2`/`@08D6D9`) matches `Form1.evaluate`'s reads of `0x4F` exactly (`1` = "Congratulations!", `2` or `4` = "You are dead!", `3` = "Game ended"). `Sub_20_2`, the NPC tick, carries the same `> 0 → Exit Sub` guard. **Ported structurally**: the guard now sits at the top of `task_run_task()` rather than on the type-5 action, mirroring the Runner; every other caller is already behind the main loop's `if (game->is_running)`, so this is a no-op for them. Whole v4 suite re-run after both the port and the move: **203/203 PASS, no golden moved.** **Verdict for *Three Monkeys One Cage*: "98/100 is the ceiling" is a fact about the game, not about our engine.** Task 603 is `exec 604` / `exec 608` / `player_moves--` / `player_score += 2`; the two callees are mutually exclusive and both end the game, but the `+2` is an **in-line** action, so it is credited in *both* engines — it is simply never displayable, because the game is over and this game's score is an author variable that only the `score` command ever prints. 98 is the highest score a player can ever *see*; the 100th point is banked in state at the instant of the win. |
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
