/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjölund
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
 * scdump.cpp -- reusable structural-dump / trace instrumentation for Scarier.
 *
 * This is developer tooling used to reverse-engineer ADRIFT games when
 * deriving deterministic walkthroughs (see terps/scarier/test/adrift4/).
 * It is gated behind the SCARIER_DUMP_TOOLS build macro and is compiled ONLY
 * into the headless walkthrough harness (Makefile.headless and
 * test/adrift4/harness/build.sh pass -DSCARIER_DUMP_TOOLS).  The shipping
 * builds (Makefile, the Xcode project) do not compile tools/ at all, and the
 * guarded call sites in sctasks.cpp / scnpcs.cpp compile to nothing without
 * the macro, so there is zero footprint in the shipping interpreter.
 * tools/scrun_census.cpp is the other file under the same guard.
 *
 * The three entry points, each driven by an environment variable so a run can
 * opt in without recompiling:
 *
 *   SCR_DUMP_TASKS   one-shot structural dump to stderr -- every task's command
 *                   pattern, Where (firing room), Repeatable flag, and each
 *                   restriction and action with its Type and Var1/Var2/Var3,
 *                   plus a container-index table and the full event table.
 *                   Object/task references are resolved to names.  Decoding
 *                   keys (verified against the original 3.90/4.0 Runner):
 *                     - action Type: 0 move-obj, 1 move-char, 2 obj-status,
 *                       3 change-var, 4 ChangeScore (Var1=points),
 *                       5 exec/unset, 6 EndGame (Var1: 0 win, 1 lose,
 *                       2 death, 3 stop), 7 change-battle.  No Type-4 =>
 *                       no score; no Type-6 => no ending.
 *                     - restriction Type: 0 obj-location, 1 obj-state,
 *                       2 task-state (Var1 is 1-based task; Var2 0=done,
 *                       1=undone), 3 char, 4 variable (Var1-2 = variable
 *                       index).
 *                     - object "inside" Var3 is a 1-based container index;
 *                       move-object actions move 1-based room destinations.
 *
 *   SCR_TRACE_TASKS  enable Scarier's built-in task + restriction tracing
 *                   (prints the bracket expression and each restriction
 *                   PASS/FAIL).
 *
 *   SCR_TRACE_JUDY   per-turn one-line dump of every NPC's current room, for
 *                   pinning down a wandering NPC's deterministic walk.
 *
 *   SCR_TRACE_OBJ    per-turn dump of object positions and states, either every
 *                   object ("1"/"all") or a comma-separated index list.  The
 *                   companion to SCR_DUMP_OBJLOC for games that move objects
 *                   about at run time.
 *
 *   SCR_TRACE_VARS   per-turn dump of the game's own named integer variables,
 *                   either all of them ("1"/"all") or a comma-separated
 *                   subset.  For games whose timers, NPC moods and progress
 *                   flags all live in variables (Three Monkeys One Cage).
 *
 *   SCR_DUMP_OBJLOC  one-shot, isolated per-object dump: starting position
 *                   (gs_object_position code: >=1 room+1, 0 held, -1 hidden,
 *                   -10/-20 in/on object, -100/-200/-300 worn-player/held-NPC/
 *                   worn-NPC), the room it is directly in (or -1), and its
 *                   Battle weapon/armour properties (HitValue, Accuracy,
 *                   ProtectionValue, Method).  Deliberately skips the heavy
 *                   task/exit/walk sections, so it is safe on games whose
 *                   full SCR_DUMP_TASKS output is pathologically large.
 *                   Built for combat-survivability route planning (which
 *                   armour/weapon is where).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <set>
#include <string>

#include "core/scarier.h"
#include "core/scprotos.h"
#include "core/scgamest.h"
#include "runner/scrunner.h"

#ifdef SCARIER_DUMP_TOOLS


/*
 * scr_dump_log()
 * scr_dump_flush()
 *
 * The one sink every dump, trace and census line in tools/ goes through.
 * It is stderr, so the harnesses can keep the instrumentation apart from
 * the game's own stdout transcript (and 2>&1 interleaves them in order,
 * because stderr is unbuffered).  A harness that wants the output
 * somewhere else changes these two functions and nothing else.
 */
void
scr_dump_log (const char *format, ...)
{
  va_list ap;

  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);
}

void
scr_dump_flush (void)
{
  fflush (stderr);
}


/*
 * scdump_resolve_object()
 * scdump_object_name()
 *
 * Resolve a restriction/action object reference to its global object index,
 * and look up an object's Short name, NULL if the index is out of range.
 */
static scr_int
scdump_resolve_object (scr_gameref_t game, scr_int kind, scr_int var1)
{
  /*
   * kind: 0 = dynamic-object ref (var1>=3 -> dynamic var1-3),
   *       1 = stateful-object ref (var1>=1 -> stateful var1-1).
   */
  if (kind == 0 && var1 >= 3)
    return obj_dynamic_object (game, var1 - 3);
  if (kind == 1 && var1 >= 1)
    return obj_stateful_object (game, var1 - 1);
  return -1;
}

static const scr_char *
scdump_object_name (scr_gameref_t game, scr_int obj)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t k[3], v;

  if (obj < 0 || obj >= gs_object_count (game))
    return nullptr;
  k[0].string = "Objects";
  k[1].integer = obj;
  k[2].string = "Short";
  return prop_get (bundle, "S<-sis", &v, k) ? v.string : nullptr;
}


/*
 * scdump_rule_table()
 *
 * Print a table of Original -> Replacement rules (ALRs or Synonyms), one
 * "<label> [orig] -> [repl]" line each.
 */
static void
scdump_rule_table (scr_prop_setref_t bundle, const scr_char *collection,
                   const scr_char *label)
{
  scr_vartype_t yk[3];
  scr_int yc, yi;

  yk[0].string = collection;
  yc = prop_get_child_count (bundle, "I<-s", yk);
  for (yi = 0; yi < yc; yi++)
    {
      const scr_char *orig, *repl;

      yk[1].integer = yi;
      yk[2].string = "Original";
      orig = prop_get_string (bundle, "S<-sis", yk);
      yk[2].string = "Replacement";
      repl = prop_get_string (bundle, "S<-sis", yk);
      scr_dump_log ("%s [%s] -> [%s]\n",
            label, orig ? orig : "", repl ? repl : "");
    }
}


/*
 * scdump_battle_ranges()
 *
 * Print " <attr>=lo-hi" for the five ranged battle attributes, reading
 * <attr>Lo/<attr>Hi (4.0) or a single <attr> (3.9) through key[slot], the
 * rest of the key being the player's or an NPC's Battle record.
 */
static void
scdump_battle_ranges (scr_prop_setref_t bundle, const scr_char *format,
                      scr_vartype_t key[], scr_int slot)
{
  static const scr_char *const attrs[] =
    { "Stamina", "Strength", "Accuracy", "Defense", "Agility" };
  scr_vartype_t bv;
  size_t a;

  for (a = 0; a < sizeof attrs / sizeof attrs[0]; a++)
    {
      scr_char name[32];
      scr_int lo = 0, hi = -1;

      snprintf (name, sizeof name, "%sHi", attrs[a]);
      key[slot].string = name;
      if (prop_get (bundle, format, &bv, key))
        hi = bv.integer;
      snprintf (name, sizeof name, "%sLo", attrs[a]);
      key[slot].string = name;
      if (prop_get (bundle, format, &bv, key))
        lo = bv.integer;
      if (hi < 0)
        {
          key[slot].string = (scr_char *) attrs[a];
          if (prop_get (bundle, format, &bv, key))
            lo = hi = bv.integer;
          else
            hi = 0;
        }
      scr_dump_log (" %s=%ld-%ld", attrs[a], lo, hi);
    }
}


/*
 * scdump_alr_table()
 *
 * SCR_DUMP_ALRS: the ALR table in filter order, length-prefixed.
 */
static void
scdump_alr_table (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * SCR_DUMP_ALRS: the ALR table in the order the output filter walks it
   * (ALRs2, longest original first).  compare_wine_transcript.py uses it to
   * recognise an echoed command the Runner rewrote -- a Russian game's
   * [north] -> [север] comes back as "> север", which is not a lost command.
   * Length-prefixed so an original may contain ']' or a newline.  Stops
   * before the task dump.
   */
  scr_vartype_t yk[3];
  scr_int yc, yi;

  yk[0].string = "ALRs";
  yc = prop_get_child_count (bundle, "I<-s", yk);
  scr_dump_log ("ALRCOUNT %ld\n", yc);
  for (yi = 0; yi < yc; yi++)
    {
      scr_int alr;
      const scr_char *orig, *repl;

      yk[0].string = "ALRs2";
      yk[1].integer = yi;
      yk[2].string = "ALRIndex";
      alr = prop_get_integer (bundle, "I<-sis", yk);
      yk[0].string = "ALRs";
      yk[1].integer = alr;
      yk[2].string = "Original";
      orig = prop_get_string (bundle, "S<-sis", yk);
      yk[2].string = "Replacement";
      repl = prop_get_string (bundle, "S<-sis", yk);
      if (!orig)
        orig = "";
      if (!repl)
        repl = "";
      scr_dump_log ("ALR %lu %lu\n",
            (unsigned long) strlen (orig),
            (unsigned long) strlen (repl));
      scr_dump_log ("%s%s\n", orig, repl);
    }
}


/*
 * scdump_objloc_object()
 *
 * One OBJLOC-RAWPAR and OBJLOC line for object I.
 */
static void
scdump_objloc_object (scr_gameref_t game, scr_int i)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t ok[4], bv;
  scr_int pos, r, room = -1, hit = 0, acc = 0, prot = 0, method = 0;
  const scr_char *s = scdump_object_name (game, i);

  pos = gs_object_position (game, i);
  GS_FOR_EACH_ROOM (game, r)
    {
      if (obj_directly_in_room (game, i, r))
        {
          room = r;
          break;
        }
    }
  ok[0].string = "Objects";
  ok[1].integer = i;
  ok[2].string = "Battle";
  ok[3].string = "HitValue";
  if (prop_get (bundle, "I<-siss", &bv, ok))
    hit = bv.integer;
  /*
   * A weapon's Accuracy bonus is added straight onto the wielder's
   * rolled accuracy in battle_eff_accuracy(), so it decides whether a
   * fight is winnable at all -- print it beside HitValue.
   */
  ok[3].string = "Accuracy";
  if (prop_get (bundle, "I<-siss", &bv, ok))
    acc = bv.integer;
  ok[3].string = "ProtectionValue";
  if (prop_get (bundle, "I<-siss", &bv, ok))
    prot = bv.integer;
  ok[3].string = "Method";
  if (prop_get (bundle, "I<-siss", &bv, ok))
    method = bv.integer;
  {
    /*
     * Resolve the ultimate room by chasing parent containers/surfaces
     * (pos -10/-20) or the holding/wearing NPC (pos -200/-300).
     */
    scr_int eff = room, par = gs_object_parent (game, i), guard = 0;

    if (eff < 0 && (pos == -10 || pos == -20))
      {
        scr_int cur = i;

        while (guard++ < 32 && gs_object_valid (game, cur)
               && (gs_object_position (game, cur) == -10
                   || gs_object_position (game, cur) == -20))
          cur = gs_object_parent (game, cur);
        if (gs_object_valid (game, cur))
          {
            GS_FOR_EACH_ROOM (game, r)
              {
                if (obj_directly_in_room (game, cur, r))
                  {
                    eff = r;
                    break;
                  }
              }
          }
      }
    if (eff < 0 && (pos == -200 || pos == -300)
        && gs_npc_valid (game, par))
      eff = gs_npc_location (game, par) - 1;
    /*
     * Raw SizeWeight (tens = size, units = weight) alongside the
     * scaled values, because "too heavy to carry" verdicts hinge on
     * the per-game scale bases and those are invisible otherwise.
     */
    {
      scr_int sw = 0, swclass = -1;
      scr_vartype_t swk[3];

      swk[0].string = "Objects";
      swk[1].integer = i;
      swk[2].string = "SizeWeight";
      if (prop_get (bundle, "I<-sis", &bv, swk))
        sw = bv.integer;
      /*
       * A 3.8 game's real datum: the 0..4 class and what it costs
       * against MaxCarried.  Absent (-1/0) for 3.9 and 4.0 games.
       */
      swk[2].string = "SizeWeightClass";
      if (prop_get (bundle, "I<-sis", &bv, swk))
        swclass = bv.integer;
      /*
       * The raw .taf Parent field, printed even when Position says the
       * object is not inside/on anything: the Runner's recursive
       * weight scan (Sub_22_63) matches children on this field alone,
       * with no position check, so a stale Parent silently adds an
       * elsewhere-located object's weight to this container's.
       */
      {
        scr_int rawpar = -1;

        swk[2].string = "Parent";
        if (prop_get (bundle, "I<-sis", &bv, swk))
          rawpar = bv.integer;
        scr_dump_log ("OBJLOC-RAWPAR obj=%ld rawparent=%ld"
              " runner_parent=%ld\n",
              i, rawpar, gs_object_runner_parent (game, i));
      }
      scr_dump_log ("OBJLOC obj=%ld pos=%ld room=%ld parent=%ld effroom=%ld"
                    " static=%ld unmoved=%ld open=%ld state=%ld hit=%ld"
                    " acc=%ld prot=%ld method=%ld sw=%ld size=%ld wt=%ld"
                    " class=%ld burden=%ld [%s]\n",
                    i, pos, room, par, eff,
                    (scr_int) obj_is_static (game, i),
                    (scr_int) gs_object_static_unmoved (game, i),
                    gs_object_openness (game, i),
                    gs_object_state (game, i),
                    hit, acc, prot, method,
                    sw, obj_get_size (game, i), obj_get_weight (game, i),
                    swclass,
                    obj_uses_burden_model (game)
                      ? obj_get_burden (game, i) : 0,
                    s ? s : "");
    }
  }
}


/*
 * scdump_objloc_limits()
 *
 * The player's carry limits, plus the two per-game geometric scale bases
 * they are expressed in.  MaxSize/MaxWt are raw "tens*base^units".
 */
static void
scdump_objloc_limits (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t gk[2], gv;
  scr_int max_size = 0, max_wt = 0, sbase = 3, wbase = 3;

  gk[0].string = "Globals";
  gk[1].string = "MaxSize";
  if (prop_get (bundle, "I<-ss", &gv, gk))
    max_size = gv.integer;
  gk[1].string = "MaxWt";
  if (prop_get (bundle, "I<-ss", &gv, gk))
    max_wt = gv.integer;
  gk[1].string = "SizeMultiple";
  if (prop_get (bundle, "I<-ss", &gv, gk))
    sbase = gv.integer;
  gk[1].string = "WeightMultiple";
  if (prop_get (bundle, "I<-ss", &gv, gk))
    wbase = gv.integer;
  scr_dump_log ("PLAYERLIMITS rawmaxsize=%ld rawmaxwt=%ld size=%ld wt=%ld"
                " sizebase=%ld wtbase=%ld burdenmodel=%ld maxburden=%ld\n",
                max_size, max_wt,
                obj_get_player_size_limit (game),
                obj_get_player_weight_limit (game),
                sbase, wbase,
                (scr_int) obj_uses_burden_model (game),
                obj_uses_burden_model (game)
                  ? obj_get_player_burden_limit (game) : 0);
}


/*
 * scdump_objloc_saveinfo()
 *
 * The save-stream shape depends on two things no reader can recover from
 * the stream itself: whether the Battle System interleaves a battle block
 * into the player and NPC records, and how many walk steps each NPC has
 * (written with no count in front of them).  Print both, so a .tas
 * rewriter -- tas40to39.py -- can walk a save it did not write.
 */
static void
scdump_objloc_saveinfo (scr_gameref_t game)
{
  scr_int i;

  scr_dump_log ("SAVEINFO battle=%ld rooms=%ld objects=%ld npcs=%ld\n",
        (scr_int) battle_is_enabled (game), gs_room_count (game),
        gs_object_count (game), gs_npc_count (game));
  GS_FOR_EACH_NPC (game, i)
    scr_dump_log ("NPCINFO npc=%ld walksteps=%ld\n",
          i, gs_npc_walkstep_count (game, i));
}


/*
 * scdump_objloc()
 *
 * SCR_DUMP_OBJLOC: object locations, battle properties, carry limits,
 * and the save-stream shape.
 */
static void
scdump_objloc (scr_gameref_t game)
{
  scr_int i;

  GS_FOR_EACH_OBJECT (game, i)
    scdump_objloc_object (game, i);
  scdump_objloc_limits (game);
  scdump_objloc_saveinfo (game);
}


/*
 * scdump_globals()
 *
 * The header globals and the variable table.
 */
static void
scdump_globals (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * Header: the two globals every other line has to be read against.
   * perspective is the raw authored value -- pre-4.0 Runners render 1, 2 and 3
   * alike in the second person, which lib_get_perspective() reproduces, so a
   * perspective=2 here on a version<400 game is *not* third-person output.
   */
  {
    scr_vartype_t gk[2], gv;
    scr_int version = 0, perspective = 0;
    const scr_char *game_name, *game_author;

    gk[0].string = "Version";
    if (prop_get (bundle, "I<-s", &gv, gk))
      version = gv.integer;
    gk[0].string = "Globals";
    gk[1].string = "Perspective";
    if (prop_get (bundle, "I<-ss", &gv, gk))
      perspective = gv.integer;

    /*
     * Name and author verbatim: the game-patch table in sctafpar.cpp matches
     * on this exact pair, so an entry is written by copying it from here.
     */
    game_name = prop_get_global_string (bundle, "GameName");
    game_author = prop_get_global_string (bundle, "GameAuthor");
    scr_dump_log ("GAME version=%ld perspective=%ld name=[%s] author=[%s]\n",
          version, perspective,
          game_name ? game_name : "", game_author ? game_author : "");
  }

  /*
   * Variable table.  ACT type=3 names a variable by this index directly;
   * RESTR type=4 names it by index + 2 (0 and 1 are the two "referenced
   * number" pseudo-variables).
   */
  {
    scr_vartype_t vk[3], vv;
    scr_int nvars, v;

    vk[0].string = "Variables";
    nvars = prop_get_child_count (bundle, "I<-s", vk);
    for (v = 0; v < nvars; v++)
      {
        const scr_char *name = nullptr;
        scr_int type = 0;

        vk[1].integer = v;
        vk[2].string = "Name";
        if (prop_get (bundle, "S<-sis", &vv, vk))
          name = vv.string;
        vk[2].string = "Type";
        if (prop_get (bundle, "I<-sis", &vv, vk))
          type = vv.integer;
        scr_dump_log ("VAR %ld type=%ld [%s]\n", v, type, name ? name : "");
      }
  }
}


/*
 * scdump_objects_containers()
 *
 * Container-index table (1-based "inside" Var3 maps through this).
 */
static void
scdump_objects_containers (scr_gameref_t game)
{
  scr_int i;

  for (i = 0; i < 64; i++)
    {
      scr_int oo = obj_container_object (game, i);
      const scr_char *s;

      if (oo < 0 || oo >= gs_object_count (game))
        break;
      s = scdump_object_name (game, oo);
      scr_dump_log ("CONTAINER idx=%ld obj=%ld [%s]\n", i, oo, s ? s : "");
    }
}


/*
 * scdump_objects_stateful()
 *
 * Stateful-object enumeration (Openable != 0 or CurrentState != 0, in
 * object order).  Task object-state restrictions and change-object-status
 * actions address this list 1-based; room-alt type 1 Var2 does NOT (it is
 * a 1-based global object number -- see lib_use_room_alt).
 */
static void
scdump_objects_stateful (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int i, idx = 0;

  GS_FOR_EACH_OBJECT (game, i)
    {
      scr_vartype_t pk[3], pv;
      scr_int openable, curstate;
      const scr_char *states = nullptr, *s;

      pk[0].string = "Objects";
      pk[1].integer = i;
      pk[2].string = "Openable";
      openable = prop_get (bundle, "I<-sis", &pv, pk) ? pv.integer : 0;
      pk[2].string = "CurrentState";
      curstate = prop_get (bundle, "I<-sis", &pv, pk) ? pv.integer : 0;
      if (openable == 0 && curstate == 0)
        continue;
      idx++;
      pk[2].string = "States";
      if (prop_get (bundle, "S<-sis", &pv, pk))
        states = pv.string;
      s = scdump_object_name (game, i);
      scr_dump_log ("STATEFUL idx=%ld obj=%ld [%s] openable=%ld curstate=%ld"
                    " states=[%s]\n",
                    idx, i, s ? s : "", openable, curstate,
                    states ? states : "");
    }
}


/*
 * scdump_objects_statics()
 *
 * Static-object room membership (the "Where" list). Dynamic objects are
 * located via the debugger; statics have no single position, so list every
 * room each static is directly present in -- the only way to pin a locked
 * chest / fixed scenery to a room.
 */
static void
scdump_objects_statics (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int i;

  GS_FOR_EACH_OBJECT (game, i)
    {
      scr_vartype_t sk[3];
      scr_int r;
      const scr_char *s;

      sk[0].string = "Objects";
      sk[1].integer = i;
      sk[2].string = "Static";
      if (!prop_get_boolean (bundle, "B<-sis", sk))
        continue;
      s = scdump_object_name (game, i);
      scr_dump_log ("STATIC obj=%ld [%s] rooms=", i, s ? s : "");
      GS_FOR_EACH_ROOM (game, r)
        if (obj_directly_in_room (game, i, r))
          scr_dump_log ("%ld ", r);
      scr_dump_log ("\n");
    }
}


/*
 * scdump_objects_names()
 *
 * Every object's Short name (for spotting ambiguous shared names that the
 * library bare-noun get/drop retry guard cares about), together with the
 * Prefix and Alias list, which is what %object% actually matches against --
 * uip_build_candidate() composes "Prefix Short" and "Prefix Alias", so a
 * walkthrough line that fails with "I see no such thing" is usually naming
 * a partial prefix that is not one of these strings.
 */
static void
scdump_objects_names (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int i;

  GS_FOR_EACH_OBJECT (game, i)
    {
      const scr_char *s = scdump_object_name (game, i);
      scr_vartype_t nk[4], nv;
      scr_int alias_count, a;

      nk[0].string = "Objects";
      nk[1].integer = i;
      nk[2].string = "Prefix";
      scr_dump_log ("OBJNAME obj=%ld [%s] prefix=[%s]", i, s ? s : "",
            prop_get (bundle, "S<-sis", &nv, nk) && nv.string
            ? nv.string : "");

      nk[2].string = "Alias";
      alias_count = prop_get_child_count (bundle, "I<-sis", nk);
      for (a = 0; a < alias_count; a++)
        {
          nk[3].integer = a;
          if (prop_get (bundle, "S<-sisi", &nv, nk) && nv.string
              && nv.string[0] != '\0')
            scr_dump_log (" alias=[%s]", nv.string);
        }
      scr_dump_log ("\n");
    }
}


/*
 * scdump_objects_lockkeys()
 *
 * Lockable objects: the "Key" property is the *dynamic-object index* of the
 * key that opens it; obj_dynamic_object() maps it to a real object id.
 */
static void
scdump_objects_lockkeys (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int i;

  GS_FOR_EACH_OBJECT (game, i)
    {
      scr_vartype_t kk[3], kv;
      scr_int key_index, the_key, openable;
      const scr_char *s, *ks;

      kk[0].string = "Objects";
      kk[1].integer = i;
      kk[2].string = "Openable";
      /*
       * Tolerant fetch: prop_get_integer() is FATAL on a missing property, and
       * "Openable" really can be absent -- Where Is Richard? (V390) has such
       * an object and the whole dump used to die on it.  Skip instead.
       */
      if (!prop_get (bundle, "I<-sis", &kv, kk))
        continue;
      openable = kv.integer;
      if (openable <= 0)
        continue;
      kk[2].string = "Key";
      if (!prop_get (bundle, "I<-sis", &kv, kk))
        continue;
      key_index = kv.integer;
      if (key_index < 0)
        continue;
      the_key = obj_dynamic_object (game, key_index);
      s = scdump_object_name (game, i);
      ks = "?";
      if (the_key >= 0 && the_key < gs_object_count (game))
        {
          ks = scdump_object_name (game, the_key);
          if (!ks)
            ks = "";
        }
      scr_dump_log ("LOCKKEY obj=%ld [%s] keyidx=%ld keyobj=%ld [%s]\n",
            i, s ? s : "", key_index, the_key, ks);
    }
}


/*
 * scdump_objects_openable()
 *
 * Raw Openable/Key, whatever their values.  LOCKKEY above only fires for a
 * resolvable key, and pre-4.0 games never have one (parse_fixup_openable_key
 * always writes Key = -1), so this is the line that lets the gen400
 * conversion oracle check the V390/V380_OBJECT fixup that swaps Openable
 * 5 and 6.  See test/adrift4/notes/RUNNER_TESTS_TODO.md section 3(a).
 */
static void
scdump_objects_openable (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int i;

  GS_FOR_EACH_OBJECT (game, i)
    {
      scr_vartype_t ok[3];
      scr_int openable, key = 0;
      scr_vartype_t ov;
      const scr_char *s;

      ok[0].string = "Objects";
      ok[1].integer = i;
      ok[2].string = "Openable";
      if (!prop_get (bundle, "I<-sis", &ov, ok))
        continue;
      openable = ov.integer;
      if (openable == 0)
        continue;
      ok[2].string = "Key";
      if (prop_get (bundle, "I<-sis", &ov, ok))
        key = ov.integer;
      s = scdump_object_name (game, i);
      scr_dump_log ("OPENABLE obj=%ld [%s] openable=%ld key=%ld\n",
            i, s ? s : "", openable, key);
    }
}


/*
 * scdump_objects_surfaces()
 *
 * Surface / container index enumeration (chisel-of-the-ages hunt).
 */
static void
scdump_objects_surfaces (scr_gameref_t game)
{
  scr_int i, si = 0, ci = 0;

  GS_FOR_EACH_OBJECT (game, i)
    {
      const scr_char *s = scdump_object_name (game, i);

      if (obj_is_surface (game, i))
        scr_dump_log ("SURFACE idx=%ld obj=%ld [%s]\n",
              si++, i, s ? s : "");
      if (obj_is_container (game, i))
        scr_dump_log ("CONTAINERIDX idx=%ld obj=%ld [%s]\n",
              ci++, i, s ? s : "");
    }
}


/*
 * scdump_objects()
 *
 * The object tables: container indices, states, static rooms, names,
 * locks, surfaces.
 */
static void
scdump_objects (scr_gameref_t game)
{
  scdump_objects_containers (game);
  scdump_objects_stateful (game);
  scdump_objects_statics (game);
  scdump_objects_names (game);
  scdump_objects_lockkeys (game);
  scdump_objects_openable (game);
  scdump_objects_surfaces (game);
}


/*
 * scdump_rewrite_rules()
 *
 * The ALR and synonym tables.
 */
static void
scdump_rewrite_rules (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * ALRs (output-rewrite rules applied to the finished turn text).
   *
   * Worth dumping for a reason that has nothing to do with the game: an ALR's
   * *Original* string is the author's own transcription of a **Runner**
   * message, typed while looking at the real Runner's output.  That makes the
   * corpus' ALR tables a free, offline oracle for library wording -- see the
   * asteroid_after measurement in test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md,
   * where `Which valve.` and `Which satellite.   satellite,  satellite or
   * satellite?` pin run400's ambiguity message and its
   * `tense(Prefix) & " " & Short` list construction without launching Wine.
   */
  scdump_rule_table (bundle, "ALRs", "ALR");

  /* Synonyms (input-rewrite rules applied before task/library matching). */
  scdump_rule_table (bundle, "Synonyms", "SYNONYM");
}


/*
 * scdump_task_t
 *
 * One task of the dump, for scdump_tasks(): the property key the sections
 * share, and what summary() reads that the later ones need -- the Where type
 * and the command, restriction and action counts.
 */
namespace {

struct scdump_task_t
{
  scr_gameref_t game;
  scr_prop_setref_t bundle;
  scr_int t;

  scr_vartype_t k[5], vt;
  scr_int wtype, ccount, rcount, acount;

  void summary ();
  void where_rooms ();
  void hints ();
  void texts ();
  void alt_commands ();
  void reverse_commands ();
  void restriction (scr_int i);
  void action (scr_int i);
};


/*
 * scdump_task_t::summary()
 *
 * The TASK line.
 */
void
scdump_task_t::summary ()
{
  const scr_char *cmd;
  scr_int wroom, rep, rev;

  k[0].string = "Tasks";
  k[1].integer = t;
  k[2].string = "Command";
  k[3].integer = 0;
  /*
   * A task can carry an empty Command list -- The Cleft in the Rock has
   * one, and prop_get_string is fatal on the miss -- so ask with prop_get.
   */
  cmd = prop_get (bundle, "S<-sisi", &vt, k) ? vt.string : nullptr;
  ccount = prop_get_child_count (bundle, "I<-sis", k);

  k[2].string = "Where";
  k[3].string = "Type";
  wtype = prop_get_integer (bundle, "I<-siss", k);
  wroom = -1;
  if (wtype == ROOMLIST_ONE_ROOM)
    {
      k[3].string = "Room";
      wroom = prop_get_integer (bundle, "I<-siss", k);
    }

  k[2].string = "Repeatable";
  rep = prop_get_boolean (bundle, "B<-sis", k);
  k[2].string = "Reversible";
  rev = prop_get_boolean (bundle, "B<-sis", k);
  k[2].string = "Actions";
  acount = prop_get_child_count (bundle, "I<-sis", k);
  k[2].string = "Restrictions";
  rcount = prop_get_child_count (bundle, "I<-sis", k);

  {
    scr_int score = 0;
    const scr_char *mask = nullptr;
    scr_int rpt = 0;
    scr_int srd = 0;

    k[2].string = "Score";
    if (prop_get (bundle, "I<-sis", &vt, k))
      score = vt.integer;
    /*
     * srd: the task's ShowRoomDesc, as the raw field -- 0 for none,
     * otherwise room + 1.  See test/adrift4/notes/RUNNER_TESTS_TODO.md
     * section 4, where run400 is measured printing that room's description
     * with no room name heading, and task_run_task_unrestricted() prints
     * both.
     */
    k[2].string = "ShowRoomDesc";
    if (prop_get (bundle, "I<-sis", &vt, k))
      srd = vt.integer;
    k[2].string = "RestrMask";
    if (prop_get (bundle, "S<-sis", &vt, k))
      mask = vt.string;
    /*
     * rpt: does the task carry a RepeatText?  A non-repeatable task that
     * has one answers a second typing of its command with that text rather
     * than with "You have already done that." -- see run_task_refusal().
     */
    k[2].string = "RepeatText";
    if (prop_get (bundle, "S<-sis", &vt, k) && !scr_strempty (vt.string))
      rpt = 1;
    scr_dump_log
      ("TASK %ld where=%ld room=%ld restr=%ld rep=%ld rev=%ld rpt=%ld"
       " score=%ld srd=%ld mask=[%s] cmd=[%s]\n",
       t, wtype, wroom, rcount, rep, rev, rpt, score, srd,
       mask ? mask : "", cmd ? cmd : "");
  }
}


/*
 * scdump_task_t::where_rooms()
 *
 * For SOME_ROOMS (where=2) tasks, dump the room-group membership (the
 * "Where"/"Rooms" boolean array indexed by room; see sctasks.c
 * task_can_run_task_directional). Lets us tell whether a group-scoped
 * death (e.g. the Morac castle timer T327) fires in a given room.
 */
void
scdump_task_t::where_rooms ()
{
  if (wtype == ROOMLIST_SOME_ROOMS)
    {
      scr_int rm, nrooms = gs_room_count (game);

      scr_dump_log ("    WHERE_ROOMS=[");
      for (rm = 0; rm < nrooms; rm++)
        {
          k[2].string = "Where";
          k[3].string = "Rooms";
          k[4].integer = rm;
          if (prop_get_boolean (bundle, "B<-sissi", k))
            scr_dump_log ("%ld ", rm);
        }
      scr_dump_log ("]\n");
    }
}


/*
 * scdump_task_t::hints()
 *
 * Built-in author hints attached to this task (Question/Hint1=subtle/
 * Hint2=sledgehammer) -- the game's own context-sensitive walkthrough.
 */
void
scdump_task_t::hints ()
{
  const scr_char *hq = nullptr, *h1 = nullptr, *h2 = nullptr;
  scr_vartype_t hv;

  k[2].string = "Question";
  if (prop_get (bundle, "S<-sis", &hv, k))
    hq = hv.string;
  k[2].string = "Hint1";
  if (prop_get (bundle, "S<-sis", &hv, k))
    h1 = hv.string;
  k[2].string = "Hint2";
  if (prop_get (bundle, "S<-sis", &hv, k))
    h2 = hv.string;
  if (hq && *hq)
    scr_dump_log ("    HINTQ=[%s]\n", hq);
  if (h1 && *h1)
    scr_dump_log ("    HINT1=[%s]\n", h1);
  if (h2 && *h2)
    scr_dump_log ("    HINT2=[%s]\n", h2);
}


/*
 * scdump_task_t::texts()
 *
 * The task's own text.  A wording measurement against a Runner
 * transcript stands or falls on knowing which words the author wrote:
 * the dump used to show only the command, so an engine-supplied phrase
 * and an authored one looked alike.  CompleteText is what the task
 * prints when it runs, AdditionalMessage what task_run_task_
 * unrestricted() appends after the actions, RepeatText what answers a
 * second typing (see run_task_refusal()), and ReverseMessage what an
 * "un-doing" of the task says.  humbug's task 80 "put * sweet on *
 * plinth" is the worked example: its CompleteText is a bare "I put the
 * sweet on the plinth.", so neither of the two "Okay."s the Runner
 * prefixes to it is authored -- both come from the game's own
 * self-containing ALR [I put ] -> [Okay.  I put ], one per filter pass
 * the turn runs (see pf_refilter()).
 */
void
scdump_task_t::texts ()
{
  const scr_char *tt;
  static const char *const text_fields[] =
    { "CompleteText", "AdditionalMessage", "RepeatText",
      "ReverseMessage", nullptr };
  static const char *const text_labels[] =
    { "COMPLETE", "ADDMSG", "REPEATTEXT", "REVERSE", nullptr };
  scr_int f;

  for (f = 0; text_fields[f]; f++)
    {
      k[2].string = text_fields[f];
      if (prop_get (bundle, "S<-sis", &vt, k)
          && !scr_strempty (vt.string))
        {
          tt = vt.string;
          scr_dump_log ("    %s=[%s]\n", text_labels[f], tt);
        }
    }
}


/*
 * scdump_task_t::alt_commands()
 *
 * Print any extra command alternatives (Command[1..]) -- these are the
 * synonym/wildcard forms ADRIFT matches in addition to Command[0].
 */
void
scdump_task_t::alt_commands ()
{
  scr_int ci;

  for (ci = 1; ci < ccount; ci++)
    {
      const scr_char *ac;

      k[2].string = "Command";
      k[3].integer = ci;
      ac = prop_get_string (bundle, "S<-sisi", k);
      scr_dump_log ("    ALTCMD[%ld]=[%s]\n", ci, ac ? ac : "");
    }
}


/*
 * scdump_task_t::reverse_commands()
 *
 * The reverse half of a reversible task.  A reversible task matches its
 * ReverseCommand patterns as well as its Command ones, so those strings
 * are typeable too -- and when the ReverseMessage is empty the reverse
 * run prints nothing, which makes the whole turn silent and sends a
 * pre-4.0 Runner to the game's DontUnderstand string instead of the
 * library (see silent-task rule).  lifesimulation's task 10 "turn on tv"
 * is the worked example: its ReverseCommand is the bare literal "turn
 * off tv" with no ReverseMessage, and only that exact string diverges,
 * which is why five probe drives could not find a pattern in it
 * (2026-09-05).  The dump used to show neither the flag nor the reverse
 * commands, so the task looked like an ordinary one-way task.
 */
void
scdump_task_t::reverse_commands ()
{
  if (prop_get_indexed_boolean (bundle, "Tasks", t, "Reversible"))
    {
      scr_int rc, rccount;

      k[2].string = "ReverseCommand";
      rccount = prop_get_child_count (bundle, "I<-sis", k);
      scr_dump_log ("    REVERSIBLE rcmds=%ld\n", rccount);
      for (rc = 0; rc < rccount; rc++)
        {
          const scr_char *rcs;

          k[2].string = "ReverseCommand";
          k[3].integer = rc;
          rcs = prop_get_string (bundle, "S<-sisi", k);
          scr_dump_log ("    REVCMD[%ld]=[%s]\n", rc, rcs ? rcs : "");
        }
    }
}


/*
 * scdump_task_t::restriction()
 * scdump_task_t::action()
 *
 * One RESTR line and one ACT line.
 */
void
scdump_task_t::restriction (scr_int i)
{
  scr_int rtype, v1, v2, v3, obj;
  char nm[96];

  nm[0] = '\0';
  k[2].string = "Restrictions";
  k[3].integer = i;
  k[4].string = "Type";
  rtype = prop_get_integer (bundle, "I<-sisis", k);
  v1 = v2 = v3 = -999;
  k[4].string = "Var1";
  if (prop_get (bundle, "I<-sisis", &vt, k))
    v1 = vt.integer;
  k[4].string = "Var2";
  if (prop_get (bundle, "I<-sisis", &vt, k))
    v2 = vt.integer;
  k[4].string = "Var3";
  if (prop_get (bundle, "I<-sisis", &vt, k))
    v3 = vt.integer;

  obj = -1;
  if (rtype == 0)
    obj = scdump_resolve_object (game, 0, v1);
  else if (rtype == 1)
    obj = scdump_resolve_object (game, 1, v1);
  if (obj >= 0)
    {
      const scr_char *s = scdump_object_name (game, obj);

      snprintf (nm, sizeof nm, " obj%ld=[%s]", obj, s ? s : "");
    }
  else if (rtype == 2 && v1 >= 1 && v1 - 1 < gs_task_count (game))
    {
      /* Task-state ref is 1-based; resolve to its command. */
      scr_vartype_t tk[4];
      const scr_char *s;

      tk[0].string = "Tasks";
      tk[1].integer = v1 - 1;
      tk[2].string = "Command";
      tk[3].integer = 0;
      s = prop_get_string (bundle, "S<-sisi", tk);
      snprintf (nm, sizeof nm, " task%ld=[%s]", v1 - 1, s ? s : "");
    }
  /*
   * The FailMessage decides how a failing restriction looks from the
   * outside: with one, 4.0 prints it and eats the command; with none,
   * the refusal is silent and the turn falls through to the library
   * and then to "I don't understand ...".  Two divergences have now
   * been chased that turn on exactly that difference, so dump it.
   */
  {
    const scr_char *fail;

    k[4].string = "FailMessage";
    fail = prop_get_string (bundle, "S<-sisis", k);
    scr_dump_log ("    RESTR type=%ld v1=%ld v2=%ld v3=%ld%s fail=[%s]\n",
                  rtype, v1, v2, v3, nm, fail ? fail : "");
  }
}

void
scdump_task_t::action (scr_int i)
{
  scr_int atype, v1, v2, v3, obj;
  char nm[96];

  nm[0] = '\0';
  k[2].string = "Actions";
  k[3].integer = i;
  k[4].string = "Type";
  atype = prop_get_integer (bundle, "I<-sisis", k);
  v1 = v2 = v3 = -999;
  k[4].string = "Var1";
  if (prop_get (bundle, "I<-sisis", &vt, k))
    v1 = vt.integer;
  k[4].string = "Var2";
  if (prop_get (bundle, "I<-sisis", &vt, k))
    v2 = vt.integer;
  k[4].string = "Var3";
  if (prop_get (bundle, "I<-sisis", &vt, k))
    v3 = vt.integer;

  obj = -1;
  if (atype == 0)
    obj = scdump_resolve_object (game, 0, v1);
  else if (atype == 2)
    {
      /*
       * Object-status ACTIONS index the stateful-object list 0-based
       * (sctasks.c task_run_change_object_status: obj_stateful_object
       * (var1)), whereas the matching type-1 RESTRICTION is 1-based
       * (var1-1, with 0 = "the referenced object"; screstrs.c).  This
       * is a genuine ADRIFT schema asymmetry, not a bug.  Pass v1+1 so
       * the resolver's -1 cancels and we label the object the runtime
       * actually changes (e.g. Space Boy's task 44 "unlock door" sets
       * stateful obj 18 = the Room Door, not obj 17).
       */
      obj = scdump_resolve_object (game, 1, v1 + 1);
    }
  if (obj >= 0)
    {
      const scr_char *s = scdump_object_name (game, obj);

      snprintf (nm, sizeof nm, " obj%ld=[%s]", obj, s ? s : "");
    }
  scr_dump_log ("    ACT type=%ld v1=%ld v2=%ld v3=%ld%s",
        atype, v1, v2, v3, nm);
  if (atype == 3)
    {
      const scr_char *expr = nullptr;

      /*
       * Not every type-3 action carries an Expr: a plain "set/add a
       * literal" action stores only Var1..Var3, and 3.9-era games never
       * author one at all.  prop_get_string is fatal on a missing
       * property (The Cleft in the Rock died here), so ask with
       * prop_get and accept the miss.
       */
      k[4].string = "Expr";
      if (prop_get (bundle, "S<-sisis", &vt, k))
        expr = vt.string;
      if (expr && expr[0] != '\0')
        scr_dump_log (" expr=[%s]", expr);
    }
  scr_dump_log ("\n");
}

} /* namespace */


/*
 * scdump_tasks()
 *
 * Tasks: command, Where, Repeatable, restrictions, actions.
 */
static void
scdump_tasks (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int t, i;

  /* Tasks: command, Where, Repeatable, restrictions, actions. */
  GS_FOR_EACH_TASK (game, t)
    {
      scdump_task_t task = {};

      task.game = game;
      task.bundle = bundle;
      task.t = t;

      task.summary ();
      task.where_rooms ();
      task.hints ();
      task.texts ();
      task.alt_commands ();
      task.reverse_commands ();

      for (i = 0; i < task.rcount; i++)
        task.restriction (i);
      for (i = 0; i < task.acount; i++)
        task.action (i);
    }
}


/*
 * scdump_event_pref_texts()
 *
 * PrefTime1/2 -- "show this N turns before the event ends", compared
 * against the POST-decrement clock.  These are the only event texts
 * that are neither Start, Look nor Finish, and a measurement that
 * does not know an event carries one reads its wording as an
 * unexplained divergence (FarFromHome 2026-09-05 spent a session on
 * "The tide washes in" before finding it here).
 */
static void
scdump_event_pref_texts (scr_prop_setref_t bundle, scr_vartype_t *ek)
{
  scr_vartype_t evt;
  scr_int pt;
  const scr_char *px;
  int which;

  for (which = 1; which <= 2; which++)
    {
      char field[16];

      pt = 0;
      px = nullptr;
      snprintf (field, sizeof field, "PrefTime%d", which);
      ek[2].string = field;
      if (prop_get (bundle, "I<-sis", &evt, ek))
        pt = evt.integer;
      snprintf (field, sizeof field, "PrefText%d", which);
      ek[2].string = field;
      if (prop_get (bundle, "S<-sis", &evt, ek))
        px = evt.string;
      if (px && px[0])
        scr_dump_log ("   P%d(at %ld): %s\n", which, pt, px);
    }
}


/*
 * scdump_event_where()
 *
 * The event's own room list, which gates every one of its texts.
 */
static void
scdump_event_where (scr_prop_setref_t bundle, scr_int e)
{
  scr_vartype_t wk[5], evt;
  scr_int type, rc, r;

  wk[0].string = "Events";
  wk[1].integer = e;
  wk[2].string = "Where";
  wk[3].string = "Type";
  type = prop_get (bundle, "I<-siss", &evt, wk) ? evt.integer : -1;
  scr_dump_log ("   where type=%ld", type);
  if (type == ROOMLIST_ONE_ROOM)
    {
      wk[3].string = "Room";
      if (prop_get (bundle, "I<-siss", &evt, wk))
        scr_dump_log (" room=%ld", evt.integer);
    }
  else if (type == ROOMLIST_SOME_ROOMS)
    {
      wk[3].string = "Rooms";
      rc = prop_get_child_count (bundle, "I<-siss", wk);
      scr_dump_log (" rooms[%ld]:", rc);
      for (r = 0; r < rc; r++)
        {
          wk[4].integer = r;
          if (prop_get (bundle, "B<-sissi", &evt, wk) && evt.boolean)
            scr_dump_log (" %ld", r);
        }
    }
  scr_dump_log ("\n");
}


/*
 * scdump_event()
 *
 * One EVENT line and its texts for event E.
 */
static void
scdump_event (scr_gameref_t game, scr_int e)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t ek[3];
  scr_int st, tn, ta, tf, o2, o2d, o3, o3d, t1, t2, pf;
  scr_int rt, sta, end;
  scr_vartype_t evt;
  const scr_char *en;

  ek[0].string = "Events";
  st = tn = ta = tf = o2 = o2d = o3 = o3d = t1 = t2 = pf = 0;
  rt = sta = end = 0;
  en = nullptr;
  ek[1].integer = e;
  /* Use tolerant prop_get -- not every game defines every field. */
  ek[2].string = "Short";
  if (prop_get (bundle, "S<-sis", &evt, ek))
    en = evt.string;
  ek[2].string = "StarterType";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    st = evt.integer;
  ek[2].string = "TaskNum";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    tn = evt.integer;
  ek[2].string = "TaskAffected";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    ta = evt.integer;
  ek[2].string = "TaskFinished";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    tf = evt.integer;
  ek[2].string = "RestartType";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    rt = evt.integer;
  ek[2].string = "StartTime";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    sta = evt.integer;
  ek[2].string = "EndTime";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    end = evt.integer;
  ek[2].string = "Time1";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    t1 = evt.integer;
  ek[2].string = "Time2";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    t2 = evt.integer;
  ek[2].string = "PauseTask";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    pf = evt.integer;
  ek[2].string = "Obj2";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    o2 = evt.integer;
  ek[2].string = "Obj2Dest";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    o2d = evt.integer;
  ek[2].string = "Obj3";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    o3 = evt.integer;
  ek[2].string = "Obj3Dest";
  if (prop_get (bundle, "I<-sis", &evt, ek))
    o3d = evt.integer;
  /*
   * Which of the three texts the event actually carries.  This is what
   * decides exposure to the turn-0 ordering divergence (RUNNER_TESTS_TODO
   * section 4): the real Runner starts an immediate event during load, so
   * a StarterType=1 event's StartText is printed into the cleared
   * pre-intro screen and its LookText joins the opening room
   * description.
   */
  {
    const scr_char *stx = nullptr, *ltx = nullptr, *ftx = nullptr;

    ek[2].string = "StartText";
    if (prop_get (bundle, "S<-sis", &evt, ek))
      stx = evt.string;
    ek[2].string = "LookText";
    if (prop_get (bundle, "S<-sis", &evt, ek))
      ltx = evt.string;
    ek[2].string = "FinishText";
    if (prop_get (bundle, "S<-sis", &evt, ek))
      ftx = evt.string;
    scr_dump_log
      ("EVENT %ld [%s] starter=%ld startTask=%ld"
       " affTask=%ld(fin=%ld) restart=%ld start=%ld..%ld"
       " time1=%ld time2=%ld pauseTask=%ld"
       " o2=%ld->%ld o3=%ld->%ld texts=%c%c%c\n",
       e, en ? en : "", st, tn, ta, tf, rt, sta, end, t1, t2, pf,
       o2, o2d, o3, o3d,
       stx && stx[0] ? 'S' : '-',
       ltx && ltx[0] ? 'L' : '-',
       ftx && ftx[0] ? 'F' : '-');
    if (stx && stx[0])
      scr_dump_log ("   S: %s\n", stx);
    if (ltx && ltx[0])
      scr_dump_log ("   L: %s\n", ltx);
    if (ftx && ftx[0])
      scr_dump_log ("   F: %s\n", ftx);
    scdump_event_pref_texts (bundle, ek);
    scdump_event_where (bundle, e);
  }
}


/*
 * scdump_events()
 *
 * Events: starter, starter task, affected task, object moves.
 */
static void
scdump_events (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t ek[3];
  scr_int e, ecount;

  ek[0].string = "Events";
  ecount = prop_get_child_count (bundle, "I<-s", ek);
  for (e = 0; e < ecount; e++)
    scdump_event (game, e);
}


/*
 * scdump_battle()
 *
 * The player's Battle System ranges, under SCR_DUMP_BATTLE.
 */
static void
scdump_battle (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * The player's Battle System ranges (Globals.Battle.<attr>Lo/Hi), same
   * shape as the per-NPC BATTLE line below.  A player Stamina of 0..0 is the
   * case that matters: the Runner rolls stamina 0 at load and an enemy NPC
   * in the room then kills the player with its first blow (Battles.bas
   * Proc_11_14 selects 0-stamina targets, Proc_11_0 -> Proc_21_62).
   */
  if (getenv ("SCR_DUMP_BATTLE"))
    {
      scr_vartype_t bk[4], bv;
      scr_int rec = 0;

      bk[0].string = "Globals";
      bk[1].string = "Battle";
      scr_dump_log ("  BATTLE player system=%s",
            prop_get_global_boolean (bundle, "BattleSystem") ? "on" : "off");
      scdump_battle_ranges (bundle, "I<-sss", bk, 2);
      bk[2].string = "Recovery";
      if (prop_get (bundle, "I<-sss", &bv, bk))
        rec = bv.integer;
      scr_dump_log (" recovery=%ld\n", rec);
    }
}


/*
 * scdump_npcs()
 *
 * NPCs and their walks.
 */
static void
scdump_npcs (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /* NPCs and their walks (StartTask/CharTask/MeetChar/ObjectTask/Rooms). */
  {
    scr_vartype_t nk[6];
    scr_int n, ncount;

    nk[0].string = "NPCs";
    ncount = prop_get_child_count (bundle, "I<-s", nk);
    for (n = 0; n < ncount; n++)
      {
        scr_int sr = 0, w, wcount;
        const scr_char *nm = nullptr;
        scr_vartype_t nv;

        nk[1].integer = n;
        nk[2].string = "Name";
        if (prop_get (bundle, "S<-sis", &nv, nk))
          nm = nv.string;
        nk[2].string = "StartRoom";
        if (prop_get (bundle, "I<-sis", &nv, nk))
          sr = nv.integer;
        scr_dump_log ("NPC %ld [%s] startRoom=%ld", n, nm ? nm : "", sr - 1);

        /*
         * Prefix and aliases: the 4.0 battle narration names an NPC by
         * "<Prefix> <Alias[0]>" rather than by Name (see battle_npc_name()
         * in scbattle.cpp), so a battle diff wants them visible.
         */
        {
          scr_int alias_count, a;

          nk[2].string = "Prefix";
          if (prop_get (bundle, "S<-sis", &nv, nk) && nv.string
              && nv.string[0] != '\0')
            scr_dump_log (" prefix=[%s]", nv.string);

          nk[2].string = "Alias";
          alias_count = prop_get_child_count (bundle, "I<-sis", nk);
          for (a = 0; a < alias_count; a++)
            {
              nk[3].integer = a;
              if (prop_get (bundle, "S<-sisi", &nv, nk) && nv.string
                  && nv.string[0] != '\0')
                scr_dump_log (" alias=[%s]", nv.string);
            }
        }
        scr_dump_log ("\n");

        /*
         * Battle System configuration for this NPC, read straight from the
         * bundle (NPCs[n].Battle.<attr>).  4.0 games store <attr>Lo/<attr>Hi
         * pairs; 3.9 games a single <attr>.  Prints stamina + the four ranged
         * combat attributes + attitude/speed + the KilledTask reference, so a
         * "damage stalemate" (target Defence >= attacker Strength) is visible.
         * Only emitted when SCR_DUMP_BATTLE is set, to keep the task dump
         * terse.
         */
        if (getenv ("SCR_DUMP_BATTLE"))
          {
            scr_vartype_t bk[5], bv;
            scr_int att = 0, spd = 0, kt = 0;

            bk[0].string = "NPCs";
            bk[1].integer = n;
            bk[2].string = "Battle";
            scr_dump_log ("  BATTLE npc=%ld", n);
            scdump_battle_ranges (bundle, "I<-siss", bk, 3);
            bk[3].string = "Attitude";
            if (prop_get (bundle, "I<-siss", &bv, bk))
              att = bv.integer;
            bk[3].string = "Speed";
            if (prop_get (bundle, "I<-siss", &bv, bk))
              spd = bv.integer;
            bk[3].string = "KilledTask";
            if (prop_get (bundle, "I<-siss", &bv, bk))
              kt = bv.integer;
            scr_dump_log (" attitude=%ld speed=%ld killedTask=%ld\n",
                  att, spd, kt - 1);
          }

        nk[2].string = "Walks";
        wcount = prop_get_child_count (bundle, "I<-sis", nk);
        for (w = 0; w < wcount; w++)
          {
            scr_int loop = 0, st = 0, ct = 0, mo = 0, ot = 0, sp = 0, mc = 0;
            scr_int rc, s;

            nk[3].integer = w;
            nk[4].string = "Loop";
            if (prop_get (bundle, "B<-sisis", &nv, nk))
              loop = nv.boolean;
            nk[4].string = "StartTask";
            if (prop_get (bundle, "I<-sisis", &nv, nk))
              st = nv.integer;
            nk[4].string = "CharTask";
            if (prop_get (bundle, "I<-sisis", &nv, nk))
              ct = nv.integer;
            nk[4].string = "MeetObject";
            if (prop_get (bundle, "I<-sisis", &nv, nk))
              mo = nv.integer;
            nk[4].string = "ObjectTask";
            if (prop_get (bundle, "I<-sisis", &nv, nk))
              ot = nv.integer;
            nk[4].string = "StoppingTask";
            if (prop_get (bundle, "I<-sisis", &nv, nk))
              sp = nv.integer;
            nk[4].string = "MeetChar";
            if (prop_get (bundle, "I<-sisis", &nv, nk))
              mc = nv.integer;
            scr_dump_log
              ("  WALK %ld loop=%ld startTask=%ld charTask=%ld(task%ld)"
               " meetChar=%ld meetObj=%ld objTask=%ld(task%ld)"
               " stopTask=%ld\n",
               w, loop, st, ct, ct - 1, mc, mo, ot, ot - 1, sp);
            nk[4].string = "ChangedDesc";
            scr_dump_log ("    changeddesc [%s]\n",
                  prop_get (bundle, "S<-sisis", &nv, nk) && nv.string
                  ? nv.string : "");
            nk[4].string = "Rooms";
            rc = prop_get_child_count (bundle, "I<-sisis", nk);
            for (s = 0; s < rc; s++)
              {
                scr_int rm = 0, tm = 0;

                nk[5].integer = s;
                if (prop_get (bundle, "I<-sisisi", &nv, nk))
                  rm = nv.integer;
                nk[4].string = "Times";
                if (prop_get (bundle, "I<-sisisi", &nv, nk))
                  tm = nv.integer;
                nk[4].string = "Rooms";
                scr_dump_log ("    step %ld dest=%ld times=%ld\n",
                      s, rm, tm);
              }
          }
      }
  }
}


/*
 * scdump_win_text()
 *
 * The game's own win/lose text.  When a published transcript and our own
 * disagree about how many lines an ending has, this is the quick way to
 * tell an authored line from an engine-supplied one: an empty WinText
 * means every line before the score summary came from the winning task's
 * CompleteText.  (The engine adds nothing of its own there -- the
 * Runners' "Congratulations!" is a status bar caption, not game text.
 * See test/adrift4/notes/RUNNER_TESTS_TODO.md section 4.)
 */
static void
scdump_win_text (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t hk[2], hv;

  hk[0].string = "Header";
  hk[1].string = "WinText";
  scr_dump_log ("WINTEXT [%s]\n",
        prop_get (bundle, "S<-ss", &hv, hk) && hv.string
        ? hv.string : "");
}


/*
 * scdump_room_names()
 *
 * One ROOM line per room.
 */
static void
scdump_room_names (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t rk[5];
  scr_int r, rcount;

  rk[0].string = "Rooms";
  rcount = prop_get_child_count (bundle, "I<-s", rk);
  for (r = 0; r < rcount; r++)
    {
      scr_vartype_t rnv;
      const scr_char *rname = nullptr;

      rk[1].integer = r;
      rk[2].string = "Short";
      if (prop_get (bundle, "S<-sis", &rnv, rk))
        rname = rnv.string;
      scr_dump_log ("ROOM %ld [%s]\n", r, rname ? rname : "");
    }
}


/*
 * scdump_room_exits()
 *
 * Room exits + their task gates.  The exit array is in ADRIFT's own direction
 * order -- the diagonals are NE,SE,SW,NW, NOT NE,NW,SE,SW (DIRNAMES_8 in
 * sclibrar_room.cpp, and mapdraw.cpp's map_dirs, which both engines share).  Getting
 * this wrong mislabels every diagonal exit and sends a route derivation into
 * the wrong room.
 */
static void
scdump_room_exits (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t rk[5];
  scr_int r, rcount;
  static const char *dirs[] =
    { "N", "E", "S", "W", "U", "D", "IN", "OUT", "NE", "SE", "SW", "NW" };

  rk[0].string = "Rooms";
  rcount = prop_get_child_count (bundle, "I<-s", rk);
  for (r = 0; r < rcount; r++)
    {
      scr_int d;

      for (d = 0; d < 12; d++)
        {
          scr_int dest = 0, v1 = 0, v2 = 0, v3 = 0;
          scr_vartype_t rv;

          rk[1].integer = r;
          rk[2].string = "Exits";
          rk[3].integer = d;
          rk[4].string = "Dest";
          if (!prop_get (bundle, "I<-sisis", &rv, rk))
            continue;
          dest = rv.integer;
          rk[4].string = "Var1";
          if (prop_get (bundle, "I<-sisis", &rv, rk))
            v1 = rv.integer;
          rk[4].string = "Var2";
          if (prop_get (bundle, "I<-sisis", &rv, rk))
            v2 = rv.integer;
          rk[4].string = "Var3";
          if (prop_get (bundle, "I<-sisis", &rv, rk))
            v3 = rv.integer;
          if (dest > 0)
            {
              /*
               * An exit's gate is NOT always a task.  lib_can_go() reads Var3
               * as the restriction *type* -- 0 = task state, 1 = object state
               * -- and only then is Var1-1 a task index; for type 1 it is a
               * stateful-object index and Var2 the wanted openness/state.
               * Printing it as "gateTask" unconditionally sent the Tear
               * derivation looking for a task that opens the cottage door
               * when the door just needed opening.
               */
              scr_dump_log ("EXIT room=%ld %s -> dest=%ld",
                    r, dirs[d], dest - 1);
              if (v1 - 1 < 0)
                scr_dump_log (" (no gate)\n");
              else if (v3 == 0)
                /*
                 * lib_can_go restricts when done == (Var2 != 0), so the exit
                 * opens on the OPPOSITE of Var2 -- Var2 == 0 means "the task
                 * must have been done".
                 */
                scr_dump_log (" gateTask=%ld wantDone=%ld\n",
                      v1 - 1, (scr_int) (v2 == 0));
              else
                {
                  scr_int obj = obj_stateful_object (game, v1 - 1);
                  const scr_char *s = scdump_object_name (game, obj);

                  scr_dump_log (" gateObj=%ld [%s] wantState=%ld"
                        " rawV1=%ld\n",
                        obj, s ? s : "", v2, v1);
                }
            }
        }
    }
}


/*
 * scdump_room_alts()
 *
 * Room description alts.  In 4.0 these are an authored array; in 3.9/3.8
 * they are synthesised by parse_fixup_v390_v380_room_alts() from the
 * fixed AltDesc/Task1/Task2/LastDesc fields, so dumping them is how the
 * gen400 conversion oracle checks that fixup (see
 * test/adrift4/notes/RUNNER_TESTS_TODO.md §3a).
 */
static void
scdump_room_alts (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t rk[5];
  scr_int r, rcount;

  rk[0].string = "Rooms";
  rcount = prop_get_child_count (bundle, "I<-s", rk);
  for (r = 0; r < rcount; r++)
    {
      scr_int a, acount;

      rk[1].integer = r;
      rk[2].string = "Alts";
      acount = prop_get_child_count (bundle, "I<-sis", rk);
      for (a = 0; a < acount; a++)
        {
          scr_int type = 0, v2 = 0, v3 = 0, disp = 0, hide = 0;
          const scr_char *m1 = nullptr, *m2 = nullptr;
          scr_vartype_t av;

          rk[3].integer = a;
          rk[4].string = "Type";
          if (prop_get (bundle, "I<-sisis", &av, rk))
            type = av.integer;
          rk[4].string = "Var2";
          if (prop_get (bundle, "I<-sisis", &av, rk))
            v2 = av.integer;
          rk[4].string = "Var3";
          if (prop_get (bundle, "I<-sisis", &av, rk))
            v3 = av.integer;
          rk[4].string = "DisplayRoom";
          if (prop_get (bundle, "I<-sisis", &av, rk))
            disp = av.integer;
          rk[4].string = "HideObjects";
          if (prop_get (bundle, "I<-sisis", &av, rk))
            hide = av.integer;
          rk[4].string = "M1";
          if (prop_get (bundle, "S<-sisis", &av, rk))
            m1 = av.string;
          rk[4].string = "M2";
          if (prop_get (bundle, "S<-sisis", &av, rk))
            m2 = av.string;
          scr_dump_log ("ALT room=%ld alt=%ld type=%ld v2=%ld v3=%ld"
                        " disp=%ld hide=%ld m1=[%s] m2=[%s]\n",
                        r, a, type, v2, v3, disp, hide,
                        m1 ? m1 : "", m2 ? m2 : "");
        }
    }
}


/*
 * scdump_exits()
 *
 * Room exits and their task gates.
 */
static void
scdump_exits (scr_gameref_t game)
{
  scdump_win_text (game);
  scdump_room_names (game);
  scdump_room_exits (game);
  scdump_room_alts (game);
}


/*
 * scr_dump_structure_once()
 *
 * One-shot (guarded by a static flag) structural dump, triggered by the
 * SCR_DUMP_TASKS environment variable.  Also honours SCR_TRACE_TASKS.
 */
void
scr_dump_structure_once (scr_gameref_t game)
{
  static scr_bool dumped = FALSE;
  static scr_bool checked_env = FALSE;
  static scr_bool trace_tasks, dump_objloc, dump_tasks, trace_events;
  static scr_bool dump_alrs;

  /*
   * This is called from every task_can_run_task_directional() -- per task,
   * per turn -- and getenv is a locked linear environ scan, so poll the
   * environment once only (the variables can't change mid-run anyway).
   */
  if (!checked_env)
    {
      checked_env = TRUE;
      trace_tasks = getenv ("SCR_TRACE_TASKS") != nullptr;
      dump_objloc = getenv ("SCR_DUMP_OBJLOC") != nullptr;
      dump_tasks = getenv ("SCR_DUMP_TASKS") != nullptr;
      trace_events = getenv ("SCR_TRACE_EVENTS") != nullptr;
      dump_alrs = getenv ("SCR_DUMP_ALRS") != nullptr;
    }
  if (!trace_tasks && !dump_objloc && !dump_tasks && !trace_events
      && !dump_alrs)
    return;

  if (dump_alrs && !dump_tasks && !dumped)
    {
      dumped = TRUE;
      scdump_alr_table (game);
      return;
    }

  if (trace_tasks)
    {
      task_debug_trace (TRUE);
      restr_debug_trace (TRUE);
    }

  /*
   * SCR_TRACE_EVENTS: per-turn event state machine trace (start/tick/finish +
   * the object moves an event performs).  Separate from SCR_TRACE_TASKS because
   * an event that silently never fires is invisible in the task trace.
   */
  if (trace_events)
    evt_debug_trace (TRUE);

  /*
   * SCR_DUMP_OBJLOC: minimal, isolated object-location + battle-property dump
   * (does NOT touch the heavy task/exit/walk sections, so it is safe on games
   * whose full dump is large).
   */
  if (dump_objloc && !dumped)
    {
      dumped = TRUE;
      scdump_objloc (game);
      return;
    }

  if (!dump_tasks || dumped)
    return;
  dumped = TRUE;

  scdump_globals (game);
  scdump_objects (game);
  scdump_rewrite_rules (game);
  scdump_tasks (game);
  scdump_events (game);
  scdump_battle (game);
  scdump_npcs (game);
  scdump_exits (game);

  scr_dump_flush ();
}


/*
 * scr_dump_npc_trace()
 *
 * Per-turn one-line dump of every NPC's room, triggered by SCR_TRACE_JUDY.
 * Call once per turn (from npc_tick_npcs, after NPCs have moved).
 */
void
scr_dump_npc_trace (scr_gameref_t game)
{
  static scr_bool checked_env = FALSE;
  static scr_bool trace_player, trace_judy;
  static const scr_char *trace_obj, *trace_vars;
  scr_int npc;

  /* Called once per turn; poll the environment once only. */
  if (!checked_env)
    {
      checked_env = TRUE;
      trace_player = getenv ("SCR_TRACE_PLAYER") != nullptr;
      trace_obj = getenv ("SCR_TRACE_OBJ");
      trace_judy = getenv ("SCR_TRACE_JUDY") != nullptr;
      trace_vars = getenv ("SCR_TRACE_VARS");
    }

  /* SCR_TRACE_PLAYER: just the player's room each turn (maze-mapping aid). */
  if (trace_player)
    {
      scr_dump_log ("PLAYERROOM room=%ld stamina=%ld\n",
            gs_playerroom (game), game->playerstamina);
      scr_dump_flush ();
    }

  /*
   * SCR_TRACE_OBJ: per-turn position and state of the objects named by a
   * comma-separated index list, or of every object for "1"/"all".  Games that
   * scatter their objects to random rooms at run time (House Of Horror opens
   * its front door and re-rolls twenty of them) cannot be routed from the
   * static OBJLOC dump at all; this is where the objects actually are.
   */
  if (trace_obj)
    {
      scr_bool all = (strcmp (trace_obj, "1") == 0
                      || strcmp (trace_obj, "all") == 0);
      scr_int count = gs_object_count (game), object;

      for (object = 0; object < count; object++)
        {
          if (!all)
            {
              const scr_char *scan = trace_obj;
              scr_bool wanted = FALSE;

              /* Match `object` against an entry of the comma-separated list. */
              while (scan && *scan)
                {
                  if (atol (scan) == object)
                    {
                      wanted = TRUE;
                      break;
                    }
                  scan = strchr (scan, ',');
                  if (scan)
                    scan++;
                }
              if (!wanted)
                continue;
            }
          scr_dump_log ("OBJTRACE obj=%ld pos=%ld parent=%ld state=%ld\n",
                object, gs_object_position (game, object),
                gs_object_parent (game, object),
                gs_object_state (game, object));
        }
      scr_dump_flush ();
    }

  /*
   * SCR_TRACE_VARS: per-turn dump of the game's own named variables.  Games
   * that run their whole state machine out of variables (timers, NPC moods,
   * "have I done X yet" flags) are otherwise opaque; this makes the machine
   * visible one turn at a time.  Set to "1"/"all" for every integer variable,
   * or to a comma-separated name list to follow just a few.
   */
  if (trace_vars)
    {
      const scr_prop_setref_t bundle = gs_get_bundle (game);
      const scr_var_setref_t vars = gs_get_vars (game);
      scr_bool all = (strcmp (trace_vars, "1") == 0
                      || strcmp (trace_vars, "all") == 0);
      scr_vartype_t vt_key[3], vt_rvalue;
      scr_int count, index_;

      vt_key[0].string = "Variables";
      count = prop_get_child_count (bundle, "I<-s", vt_key);

      scr_dump_log ("VARTRACE");
      for (index_ = 0; index_ < count; index_++)
        {
          const scr_char *name;
          scr_int var_type;

          vt_key[1].integer = index_;
          vt_key[2].string = "Name";
          name = prop_get_string (bundle, "S<-sis", vt_key);
          if (!name)
            continue;

          /* Filter: a bare name has to appear in the comma-separated list. */
          if (!all)
            {
              const scr_char *hit = strstr (trace_vars, name);
              size_t len = strlen (name);

              if (!hit
                  || (hit != trace_vars && hit[-1] != ',')
                  || (hit[len] != '\0' && hit[len] != ','))
                continue;
            }

          if (!var_get (vars, var_indexed_name (bundle, index_), &var_type,
                        &vt_rvalue)
              || var_type != VAR_INTEGER)
            continue;
          if (all)
            scr_dump_log (" %ld:%s=%ld", index_, name, vt_rvalue.integer);
          else
            scr_dump_log (" %s=%ld", name, vt_rvalue.integer);
        }
      scr_dump_log ("\n");
      scr_dump_flush ();
    }

  if (!trace_judy)
    return;
  GS_FOR_EACH_NPC (game, npc)
    scr_dump_log ("JUDYTRACE npc=%ld room=%ld\n",
          npc, gs_npc_location (game, npc) - 1);
  scr_dump_flush ();
}


/*
 * The deviations already reported on the runner's current input line
 * (run_trace_last_input, from runner/scrunner.h).
 */
static std::set<std::string> scr_deviations_seen;


/*
 * scr_tracing_deviations()
 * scr_trace_deviation()
 * scr_trace_deviation_line()
 *
 * SCR_TRACE_DEVIATIONS: one stderr line per deliberate deviation that
 * decided something on the current input line --
 *
 *   DEV <id>\t[<input line>]\t<detail>
 *
 * -- each id and detail once per line.  For auditing what the deviations
 * let a player do; see test/adrift4/harness/trace_deviations.sh.
 */
scr_bool
scr_tracing_deviations (void)
{
  static int enabled = -1;

  if (enabled < 0)
    enabled = getenv ("SCR_TRACE_DEVIATIONS") != nullptr;
  return enabled;
}

void
scr_trace_deviation_line (void)
{
  scr_deviations_seen.clear ();
}

void
scr_trace_deviation (const char *id, const char *format, ...)
{
  char detail[1024] = "";

  if (!scr_tracing_deviations ())
    return;
  if (format)
    {
      va_list ap;

      va_start (ap, format);
      vsnprintf (detail, sizeof (detail), format, ap);
      va_end (ap);
    }
  if (!scr_deviations_seen.insert (std::string (id) + '\t' + detail).second)
    return;
  scr_dump_log ("DEV %s\t[%s]\t%s\n", id, run_trace_last_input.c_str (),
        detail);
  scr_dump_flush ();
}

#endif /* SCARIER_DUMP_TOOLS */
