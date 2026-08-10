/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * scdump.c -- reusable structural-dump / trace instrumentation for SCARIER.
 *
 * This is developer tooling used to reverse-engineer ADRIFT games when
 * deriving deterministic walkthroughs (see terps/scarier/test/adrift4/).
 * It is gated behind the SCARIER_DUMP_TOOLS build macro and is compiled ONLY
 * into the headless walkthrough harness (harness/build.sh passes
 * -DSCARIER_DUMP_TOOLS).  A normal Spatterlight build never sees this file or
 * the call sites in sctasks.c / scnpcs.c, so there is zero footprint in the
 * shipping interpreter.
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
 *                       3 change-var, 4 ChangeScore (Var1=points), 5 exec/unset,
 *                       6 EndGame (Var1: 0 win, 1 lose, 2 death, 3 stop),
 *                       7 change-battle.  No Type-4 => no score; no Type-6 =>
 *                       no ending.
 *                     - restriction Type: 0 obj-location, 1 obj-state,
 *                       2 task-state (Var1 is 1-based task; Var2 0=done,1=undone),
 *                       3 char, 4 variable (Var1-2 = variable index).
 *                     - object "inside" Var3 is a 1-based container index;
 *                       move-object actions move 1-based room destinations.
 *
 *   SCR_TRACE_TASKS  enable SCARIER's built-in task + restriction tracing (prints
 *                   the bracket expression and each restriction PASS/FAIL).
 *
 *   SCR_TRACE_JUDY   per-turn one-line dump of every NPC's current room, for
 *                   pinning down a wandering NPC's deterministic walk.
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
 *                   task/exit/walk
 *                   sections, so it is safe on games whose full SCR_DUMP_TASKS
 *                   output is pathologically large.  Built for combat-
 *                   survivability route planning (which armour/weapon is where).
 */

#include <stdio.h>
#include <stdlib.h>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"

#ifdef SCARIER_DUMP_TOOLS

/* Resolve a restriction/action object reference to its global object index. */
static scr_int
scdump_resolve_object (scr_gameref_t game, scr_int kind, scr_int var1)
{
  /* kind: 0 = dynamic-object ref (var1>=3 -> dynamic var1-3),
   *       1 = stateful-object ref (var1>=1 -> stateful var1-1). */
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
    return NULL;
  k[0].string = "Objects";
  k[1].integer = obj;
  k[2].string = "Short";
  return prop_get (bundle, "S<-sis", &v, k) ? v.string : NULL;
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
  scr_int t, i;

  /* This is called from every task_can_run_task_directional() -- per task,
   * per turn -- and getenv is a locked linear environ scan, so poll the
   * environment once only (the variables can't change mid-run anyway). */
  if (!checked_env)
    {
      checked_env = TRUE;
      trace_tasks = getenv ("SCR_TRACE_TASKS") != NULL;
      dump_objloc = getenv ("SCR_DUMP_OBJLOC") != NULL;
      dump_tasks = getenv ("SCR_DUMP_TASKS") != NULL;
      trace_events = getenv ("SCR_TRACE_EVENTS") != NULL;
    }
  if (!trace_tasks && !dump_objloc && !dump_tasks && !trace_events)
    return;

  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (trace_tasks)
    {
      task_debug_trace (TRUE);
      restr_debug_trace (TRUE);
    }

  /* SCR_TRACE_EVENTS: per-turn event state machine trace (start/tick/finish +
   * the object moves an event performs).  Separate from SCR_TRACE_TASKS because
   * an event that silently never fires is invisible in the task trace. */
  if (trace_events)
    evt_debug_trace (TRUE);

  /* SCR_DUMP_OBJLOC: minimal, isolated object-location + battle-property dump
   * (does NOT touch the heavy task/exit/walk sections, so it is safe on games
   * whose full dump is large). */
  if (dump_objloc && !dumped)
    {
      dumped = TRUE;
      for (i = 0; i < gs_object_count (game); i++)
        {
          scr_vartype_t ok[4], bv;
          scr_int pos, r, room = -1, hit = 0, acc = 0, prot = 0, method = 0;
          const scr_char *s = scdump_object_name (game, i);
          pos = gs_object_position (game, i);
          for (r = 0; r < gs_room_count (game); r++)
            if (obj_directly_in_room (game, i, r)) { room = r; break; }
          ok[0].string = "Objects"; ok[1].integer = i; ok[2].string = "Battle";
          ok[3].string = "HitValue";        if (prop_get (bundle, "I<-siss", &bv, ok)) hit = bv.integer;
          /* A weapon's Accuracy bonus is added straight onto the wielder's
           * rolled accuracy in battle_eff_accuracy(), so it decides whether a
           * fight is winnable at all -- print it beside HitValue. */
          ok[3].string = "Accuracy";        if (prop_get (bundle, "I<-siss", &bv, ok)) acc = bv.integer;
          ok[3].string = "ProtectionValue"; if (prop_get (bundle, "I<-siss", &bv, ok)) prot = bv.integer;
          ok[3].string = "Method";          if (prop_get (bundle, "I<-siss", &bv, ok)) method = bv.integer;
          {
            /* Resolve the ultimate room by chasing parent containers/surfaces
             * (pos -10/-20) or the holding/wearing NPC (pos -200/-300). */
            scr_int eff = room, par = gs_object_parent (game, i), guard = 0;
            if (eff < 0 && (pos == -10 || pos == -20))
              {
                scr_int cur = i;
                while (guard++ < 32 && cur >= 0
                       && (gs_object_position (game, cur) == -10
                           || gs_object_position (game, cur) == -20))
                  cur = gs_object_parent (game, cur);
                if (cur >= 0)
                  for (r = 0; r < gs_room_count (game); r++)
                    if (obj_directly_in_room (game, cur, r)) { eff = r; break; }
              }
            if (eff < 0 && (pos == -200 || pos == -300) && par >= 0)
              eff = gs_npc_location (game, par) - 1;
            /* Raw SizeWeight (tens = size, units = weight) alongside the
             * scaled values, because "too heavy to carry" verdicts hinge on
             * the per-game scale bases and those are invisible otherwise. */
            {
              scr_int sw = 0, swclass = -1;
              scr_vartype_t swk[3];
              swk[0].string = "Objects"; swk[1].integer = i;
              swk[2].string = "SizeWeight";
              if (prop_get (bundle, "I<-sis", &bv, swk)) sw = bv.integer;
              /* A 3.8 game's real datum: the 0..4 class and what it costs
               * against MaxCarried.  Absent (-1/0) for 3.9 and 4.0 games. */
              swk[2].string = "SizeWeightClass";
              if (prop_get (bundle, "I<-sis", &bv, swk)) swclass = bv.integer;
              fprintf (stderr,
                       "OBJLOC obj=%ld pos=%ld room=%ld parent=%ld effroom=%ld"
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

      /* The save-stream shape depends on two things no reader can recover from
       * the stream itself: whether the Battle System interleaves a battle block
       * into the player and NPC records, and how many walk steps each NPC has
       * (written with no count in front of them).  Print both, so a .tas
       * rewriter -- tas40to39.py -- can walk a save it did not write. */
      /* The player's carry limits, plus the two per-game geometric scale bases
       * they are expressed in.  MaxSize/MaxWt are raw "tens*base^units". */
      {
        scr_vartype_t gk[2], gv;
        scr_int max_size = 0, max_wt = 0, sbase = 3, wbase = 3;
        gk[0].string = "Globals";
        gk[1].string = "MaxSize";
        if (prop_get (bundle, "I<-ss", &gv, gk)) max_size = gv.integer;
        gk[1].string = "MaxWt";
        if (prop_get (bundle, "I<-ss", &gv, gk)) max_wt = gv.integer;
        gk[1].string = "SizeMultiple";
        if (prop_get (bundle, "I<-ss", &gv, gk)) sbase = gv.integer;
        gk[1].string = "WeightMultiple";
        if (prop_get (bundle, "I<-ss", &gv, gk)) wbase = gv.integer;
        fprintf (stderr,
                 "PLAYERLIMITS rawmaxsize=%ld rawmaxwt=%ld size=%ld wt=%ld"
                 " sizebase=%ld wtbase=%ld burdenmodel=%ld maxburden=%ld\n",
                 max_size, max_wt,
                 obj_get_player_size_limit (game),
                 obj_get_player_weight_limit (game),
                 sbase, wbase,
                 (scr_int) obj_uses_burden_model (game),
                 obj_uses_burden_model (game)
                   ? obj_get_player_burden_limit (game) : 0);
      }

      fprintf (stderr, "SAVEINFO battle=%ld rooms=%ld objects=%ld npcs=%ld\n",
               (scr_int) battle_is_enabled (game), gs_room_count (game),
               gs_object_count (game), gs_npc_count (game));
      for (i = 0; i < gs_npc_count (game); i++)
        fprintf (stderr, "NPCINFO npc=%ld walksteps=%ld\n",
                 i, gs_npc_walkstep_count (game, i));
      return;
    }

  if (!dump_tasks || dumped)
    return;
  dumped = TRUE;

  /* Header: the two globals every other line has to be read against.
   * perspective is the raw authored value -- pre-4.0 Runners render 1, 2 and 3
   * alike in the second person, which lib_get_perspective() reproduces, so a
   * perspective=2 here on a version<400 game is *not* third-person output. */
  {
    scr_vartype_t gk[2], gv;
    scr_int version = 0, perspective = 0;
    gk[0].string = "Version";
    if (prop_get (bundle, "I<-s", &gv, gk)) version = gv.integer;
    gk[0].string = "Globals";
    gk[1].string = "Perspective";
    if (prop_get (bundle, "I<-ss", &gv, gk)) perspective = gv.integer;
    fprintf (stderr, "GAME version=%ld perspective=%ld\n",
             version, perspective);
  }

  /* Variable table.  ACT type=3 names a variable by this index directly;
   * RESTR type=4 names it by index + 2 (0 and 1 are the two "referenced
   * number" pseudo-variables). */
  {
    scr_vartype_t vk[3], vv;
    scr_int nvars, v;
    vk[0].string = "Variables";
    nvars = prop_get_child_count (bundle, "I<-s", vk);
    for (v = 0; v < nvars; v++)
      {
        const scr_char *name = NULL;
        scr_int type = 0;
        vk[1].integer = v;
        vk[2].string = "Name";
        if (prop_get (bundle, "S<-sis", &vv, vk)) name = vv.string;
        vk[2].string = "Type";
        if (prop_get (bundle, "I<-sis", &vv, vk)) type = vv.integer;
        fprintf (stderr, "VAR %ld type=%ld [%s]\n", v, type, name ? name : "");
      }
  }

  /* Container-index table (1-based "inside" Var3 maps through this). */
  for (i = 0; i < 64; i++)
    {
      scr_int oo = obj_container_object (game, i);
      const scr_char *s;
      if (oo < 0 || oo >= gs_object_count (game))
        break;
      s = scdump_object_name (game, oo);
      fprintf (stderr, "CONTAINER idx=%ld obj=%ld [%s]\n", i, oo, s ? s : "");
    }

  /* Static-object room membership (the "Where" list). Dynamic objects are
   * located via the debugger; statics have no single position, so list every
   * room each static is directly present in -- the only way to pin a locked
   * chest / fixed scenery to a room. */
  for (i = 0; i < gs_object_count (game); i++)
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
      fprintf (stderr, "STATIC obj=%ld [%s] rooms=", i, s ? s : "");
      for (r = 0; r < gs_room_count (game); r++)
        if (obj_directly_in_room (game, i, r))
          fprintf (stderr, "%ld ", r);
      fprintf (stderr, "\n");
    }

  /* Every object's Short name (for spotting ambiguous shared names that the
   * library bare-noun get/drop retry guard cares about), together with the
   * Prefix and Alias list, which is what %object% actually matches against --
   * uip_build_candidate() composes "Prefix Short" and "Prefix Alias", so a
   * walkthrough line that fails with "I see no such thing" is usually naming
   * a partial prefix that is not one of these strings. */
  for (i = 0; i < gs_object_count (game); i++)
    {
      const scr_char *s = scdump_object_name (game, i);
      scr_vartype_t nk[4], nv;
      scr_int alias_count, a;

      nk[0].string = "Objects";
      nk[1].integer = i;
      nk[2].string = "Prefix";
      fprintf (stderr, "OBJNAME obj=%ld [%s] prefix=[%s]", i, s ? s : "",
               prop_get (bundle, "S<-sis", &nv, nk) && nv.string
               ? nv.string : "");

      nk[2].string = "Alias";
      alias_count = prop_get_child_count (bundle, "I<-sis", nk);
      for (a = 0; a < alias_count; a++)
        {
          nk[3].integer = a;
          if (prop_get (bundle, "S<-sisi", &nv, nk) && nv.string
              && nv.string[0] != '\0')
            fprintf (stderr, " alias=[%s]", nv.string);
        }
      fprintf (stderr, "\n");
    }

  /* Lockable objects: the "Key" property is the *dynamic-object index* of the
   * key that opens it; obj_dynamic_object() maps it to a real object id. */
  for (i = 0; i < gs_object_count (game); i++)
    {
      scr_vartype_t kk[3];
      scr_int key_index, the_key, openable;
      kk[0].string = "Objects";
      kk[1].integer = i;
      kk[2].string = "Openable";
      openable = prop_get_integer (bundle, "I<-sis", kk);
      if (openable <= 0)
        continue;
      kk[2].string = "Key";
      key_index = prop_get_integer (bundle, "I<-sis", kk);
      if (key_index < 0)
        continue;
      the_key = obj_dynamic_object (game, key_index);
      fprintf (stderr, "LOCKKEY obj=%ld [%s] keyidx=%ld keyobj=%ld [%s]\n",
               i, scdump_object_name (game, i) ? scdump_object_name (game, i) : "",
               key_index, the_key,
               (the_key >= 0 && the_key < gs_object_count (game))
                 ? (scdump_object_name (game, the_key) ? scdump_object_name (game, the_key) : "")
                 : "?");
    }

  /* Raw Openable/Key, whatever their values.  LOCKKEY above only fires for a
   * resolvable key, and pre-4.0 games never have one (parse_fixup_openable_key
   * always writes Key = -1), so this is the line that lets the gen400
   * conversion oracle check the V390/V380_OBJECT fixup that swaps Openable
   * 5 and 6.  See RUNNER_TESTS_TODO.md section 3(a). */
  for (i = 0; i < gs_object_count (game); i++)
    {
      scr_vartype_t ok[3];
      scr_int openable, key = 0;
      scr_vartype_t ov;
      ok[0].string = "Objects";
      ok[1].integer = i;
      ok[2].string = "Openable";
      openable = prop_get_integer (bundle, "I<-sis", ok);
      if (openable == 0)
        continue;
      ok[2].string = "Key";
      if (prop_get (bundle, "I<-sis", &ov, ok))
        key = ov.integer;
      fprintf (stderr, "OPENABLE obj=%ld [%s] openable=%ld key=%ld\n",
               i, scdump_object_name (game, i)
                    ? scdump_object_name (game, i) : "",
               openable, key);
    }

  /* Surface / container index enumeration (chisel-of-the-ages hunt). */
  {
    scr_int si = 0, ci = 0;
    for (i = 0; i < gs_object_count (game); i++)
      {
        const scr_char *s = scdump_object_name (game, i);
        if (obj_is_surface (game, i))
          fprintf (stderr, "SURFACE idx=%ld obj=%ld [%s]\n", si++, i, s ? s : "");
        if (obj_is_container (game, i))
          fprintf (stderr, "CONTAINERIDX idx=%ld obj=%ld [%s]\n", ci++, i, s ? s : "");
      }
  }

  /* Synonyms (input-rewrite rules applied before task/library matching). */
  {
    scr_vartype_t yk[3];
    scr_int yc, yi;
    yk[0].string = "Synonyms";
    yc = prop_get_child_count (bundle, "I<-s", yk);
    for (yi = 0; yi < yc; yi++)
      {
        const scr_char *orig, *repl;
        yk[1].integer = yi;
        yk[2].string = "Original";
        orig = prop_get_string (bundle, "S<-sis", yk);
        yk[2].string = "Replacement";
        repl = prop_get_string (bundle, "S<-sis", yk);
        fprintf (stderr, "SYNONYM [%s] -> [%s]\n",
                 orig ? orig : "", repl ? repl : "");
      }
  }

  /* Tasks: command, Where, Repeatable, restrictions, actions. */
  for (t = 0; t < gs_task_count (game); t++)
    {
      scr_vartype_t k[5], vt;
      const scr_char *cmd;
      scr_int wtype, wroom, acount, rcount, rep;

      scr_int ccount, ci;
      k[0].string = "Tasks";
      k[1].integer = t;
      k[2].string = "Command";
      k[3].integer = 0;
      /*
       * A task can carry an empty Command list -- The Cleft in the Rock has
       * one, and prop_get_string is fatal on the miss -- so ask with prop_get.
       */
      cmd = prop_get (bundle, "S<-sisi", &vt, k) ? vt.string : NULL;
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
      k[2].string = "Actions";
      acount = prop_get_child_count (bundle, "I<-sis", k);
      k[2].string = "Restrictions";
      rcount = prop_get_child_count (bundle, "I<-sis", k);

      {
        scr_int score = 0;
        const scr_char *mask = NULL;
        scr_int rpt = 0;
        k[2].string = "Score";
        if (prop_get (bundle, "I<-sis", &vt, k)) score = vt.integer;
        k[2].string = "RestrMask";
        if (prop_get (bundle, "S<-sis", &vt, k)) mask = vt.string;
        /* rpt: does the task carry a RepeatText?  A non-repeatable task that
         * has one answers a second typing of its command with that text rather
         * than with "You have already done that." -- see run_task_refusal(). */
        k[2].string = "RepeatText";
        if (prop_get (bundle, "S<-sis", &vt, k) && !scr_strempty (vt.string))
          rpt = 1;
        fprintf (stderr,
                 "TASK %ld where=%ld room=%ld restr=%ld rep=%ld rpt=%ld"
                 " score=%ld mask=[%s] cmd=[%s]\n",
                 t, wtype, wroom, rcount, rep, rpt, score,
                 mask ? mask : "", cmd ? cmd : "");
      }

      /* For SOME_ROOMS (where=2) tasks, dump the room-group membership (the
       * "Where"/"Rooms" boolean array indexed by room; see sctasks.c
       * task_can_run_task_directional). Lets us tell whether a group-scoped
       * death (e.g. the Morac castle timer T327) fires in a given room. */
      if (wtype == ROOMLIST_SOME_ROOMS)
        {
          scr_int rm, nrooms = gs_room_count (game);
          fprintf (stderr, "    WHERE_ROOMS=[");
          for (rm = 0; rm < nrooms; rm++)
            {
              k[2].string = "Where";
              k[3].string = "Rooms";
              k[4].integer = rm;
              if (prop_get_boolean (bundle, "B<-sissi", k))
                fprintf (stderr, "%ld ", rm);
            }
          fprintf (stderr, "]\n");
        }

      /* Built-in author hints attached to this task (Question/Hint1=subtle/
       * Hint2=sledgehammer) -- the game's own context-sensitive walkthrough. */
      {
        const scr_char *hq = NULL, *h1 = NULL, *h2 = NULL;
        scr_vartype_t hv;
        k[2].string = "Question"; if (prop_get (bundle, "S<-sis", &hv, k)) hq = hv.string;
        k[2].string = "Hint1";    if (prop_get (bundle, "S<-sis", &hv, k)) h1 = hv.string;
        k[2].string = "Hint2";    if (prop_get (bundle, "S<-sis", &hv, k)) h2 = hv.string;
        if (hq && *hq) fprintf (stderr, "    HINTQ=[%s]\n", hq);
        if (h1 && *h1) fprintf (stderr, "    HINT1=[%s]\n", h1);
        if (h2 && *h2) fprintf (stderr, "    HINT2=[%s]\n", h2);
      }

      /* Print any extra command alternatives (Command[1..]) -- these are the
       * synonym/wildcard forms ADRIFT matches in addition to Command[0]. */
      for (ci = 1; ci < ccount; ci++)
        {
          const scr_char *ac;
          k[2].string = "Command";
          k[3].integer = ci;
          ac = prop_get_string (bundle, "S<-sisi", k);
          fprintf (stderr, "    ALTCMD[%ld]=[%s]\n", ci, ac ? ac : "");
        }

      for (i = 0; i < rcount; i++)
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
          if (prop_get (bundle, "I<-sisis", &vt, k)) v1 = vt.integer;
          k[4].string = "Var2";
          if (prop_get (bundle, "I<-sisis", &vt, k)) v2 = vt.integer;
          k[4].string = "Var3";
          if (prop_get (bundle, "I<-sisis", &vt, k)) v3 = vt.integer;

          obj = -1;
          if (rtype == 0) obj = scdump_resolve_object (game, 0, v1);
          else if (rtype == 1) obj = scdump_resolve_object (game, 1, v1);
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
              tk[0].string = "Tasks"; tk[1].integer = v1 - 1;
              tk[2].string = "Command"; tk[3].integer = 0;
              s = prop_get_string (bundle, "S<-sisi", tk);
              snprintf (nm, sizeof nm, " task%ld=[%s]", v1 - 1, s ? s : "");
            }
          fprintf (stderr, "    RESTR type=%ld v1=%ld v2=%ld v3=%ld%s\n",
                   rtype, v1, v2, v3, nm);
        }

      for (i = 0; i < acount; i++)
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
          if (prop_get (bundle, "I<-sisis", &vt, k)) v1 = vt.integer;
          k[4].string = "Var2";
          if (prop_get (bundle, "I<-sisis", &vt, k)) v2 = vt.integer;
          k[4].string = "Var3";
          if (prop_get (bundle, "I<-sisis", &vt, k)) v3 = vt.integer;

          obj = -1;
          if (atype == 0) obj = scdump_resolve_object (game, 0, v1);
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
          fprintf (stderr, "    ACT type=%ld v1=%ld v2=%ld v3=%ld%s",
                   atype, v1, v2, v3, nm);
          if (atype == 3)
            {
              const scr_char *expr = NULL;

              /*
               * Not every type-3 action carries an Expr: a plain "set/add a
               * literal" action stores only Var1..Var3, and 3.9-era games never
               * author one at all.  prop_get_string is fatal on a missing
               * property (The Cleft in the Rock died here), so ask with
               * prop_get and accept the miss.
               */
              k[4].string = "Expr";
              if (prop_get (bundle, "S<-sisis", &vt, k)) expr = vt.string;
              if (expr && expr[0] != '\0')
                fprintf (stderr, " expr=[%s]", expr);
            }
          fprintf (stderr, "\n");
        }
    }

  /* Events: starter, starter task, affected task, object moves. */
  {
    scr_vartype_t ek[3];
    scr_int e, ecount;

    ek[0].string = "Events";
    ecount = prop_get_child_count (bundle, "I<-s", ek);
    for (e = 0; e < ecount; e++)
      {
        scr_int st, tn, ta, tf, o2, o2d, o3, o3d, t1, t2, pf;
        scr_int rt, sta, end;
        scr_vartype_t evt;
        const scr_char *en;

        st = tn = ta = tf = o2 = o2d = o3 = o3d = t1 = t2 = pf = 0;
        rt = sta = end = 0;
        en = NULL;
        ek[1].integer = e;
        /* Use tolerant prop_get -- not every game defines every field. */
        ek[2].string = "Short";        if (prop_get (bundle, "S<-sis", &evt, ek)) en = evt.string;
        ek[2].string = "StarterType";  if (prop_get (bundle, "I<-sis", &evt, ek)) st = evt.integer;
        ek[2].string = "TaskNum";      if (prop_get (bundle, "I<-sis", &evt, ek)) tn = evt.integer;
        ek[2].string = "TaskAffected"; if (prop_get (bundle, "I<-sis", &evt, ek)) ta = evt.integer;
        ek[2].string = "TaskFinished"; if (prop_get (bundle, "I<-sis", &evt, ek)) tf = evt.integer;
        ek[2].string = "RestartType";  if (prop_get (bundle, "I<-sis", &evt, ek)) rt = evt.integer;
        ek[2].string = "StartTime";    if (prop_get (bundle, "I<-sis", &evt, ek)) sta = evt.integer;
        ek[2].string = "EndTime";      if (prop_get (bundle, "I<-sis", &evt, ek)) end = evt.integer;
        ek[2].string = "Time1";        if (prop_get (bundle, "I<-sis", &evt, ek)) t1 = evt.integer;
        ek[2].string = "Time2";        if (prop_get (bundle, "I<-sis", &evt, ek)) t2 = evt.integer;
        ek[2].string = "PauseTask";    if (prop_get (bundle, "I<-sis", &evt, ek)) pf = evt.integer;
        ek[2].string = "Obj2";         if (prop_get (bundle, "I<-sis", &evt, ek)) o2 = evt.integer;
        ek[2].string = "Obj2Dest";     if (prop_get (bundle, "I<-sis", &evt, ek)) o2d = evt.integer;
        ek[2].string = "Obj3";         if (prop_get (bundle, "I<-sis", &evt, ek)) o3 = evt.integer;
        ek[2].string = "Obj3Dest";     if (prop_get (bundle, "I<-sis", &evt, ek)) o3d = evt.integer;
        /* Which of the three texts the event actually carries.  This is what
         * decides exposure to the turn-0 ordering divergence (RUNNER_TESTS_TODO
         * section 4): the real Runner starts an immediate event during load, so
         * a StarterType=1 event's StartText is printed into the cleared
         * pre-intro screen and its LookText joins the opening room
         * description. */
        {
          const scr_char *stx = NULL, *ltx = NULL, *ftx = NULL;
          ek[2].string = "StartText";  if (prop_get (bundle, "S<-sis", &evt, ek)) stx = evt.string;
          ek[2].string = "LookText";   if (prop_get (bundle, "S<-sis", &evt, ek)) ltx = evt.string;
          ek[2].string = "FinishText"; if (prop_get (bundle, "S<-sis", &evt, ek)) ftx = evt.string;
          fprintf (stderr,
                   "EVENT %ld [%s] starter=%ld startTask=%ld affTask=%ld(fin=%ld)"
                   " restart=%ld start=%ld..%ld time1=%ld time2=%ld pauseTask=%ld"
                   " o2=%ld->%ld o3=%ld->%ld texts=%c%c%c\n",
                   e, en ? en : "", st, tn, ta, tf, rt, sta, end, t1, t2, pf,
                   o2, o2d, o3, o3d,
                   stx && stx[0] ? 'S' : '-',
                   ltx && ltx[0] ? 'L' : '-',
                   ftx && ftx[0] ? 'F' : '-');
        }
      }
  }

  /* NPCs and their walks (StartTask/CharTask/MeetChar/ObjectTask/Rooms). */
  {
    scr_vartype_t nk[6];
    scr_int n, ncount;

    nk[0].string = "NPCs";
    ncount = prop_get_child_count (bundle, "I<-s", nk);
    for (n = 0; n < ncount; n++)
      {
        scr_int sr = 0, w, wcount;
        const scr_char *nm = NULL;
        scr_vartype_t nv;
        nk[1].integer = n;
        nk[2].string = "Name";      if (prop_get (bundle, "S<-sis", &nv, nk)) nm = nv.string;
        nk[2].string = "StartRoom"; if (prop_get (bundle, "I<-sis", &nv, nk)) sr = nv.integer;
        fprintf (stderr, "NPC %ld [%s] startRoom=%ld\n", n, nm ? nm : "", sr - 1);

        /* Battle System configuration for this NPC, read straight from the
         * bundle (NPCs[n].Battle.<attr>).  4.0 games store <attr>Lo/<attr>Hi
         * pairs; 3.9 games a single <attr>.  Prints stamina + the four ranged
         * combat attributes + attitude/speed + the KilledTask reference, so a
         * "damage stalemate" (target Defence >= attacker Strength) is visible.
         * Only emitted when SCR_DUMP_BATTLE is set, to keep the task dump terse. */
        if (getenv ("SCR_DUMP_BATTLE"))
          {
            static const scr_char *const attrs[] =
              { "Stamina", "Strength", "Accuracy", "Defense", "Agility" };
            scr_vartype_t bk[5], bv;
            size_t a;
            scr_int att = 0, spd = 0, kt = 0;

            bk[0].string = "NPCs"; bk[1].integer = n; bk[2].string = "Battle";
            fprintf (stderr, "  BATTLE npc=%ld", n);
            for (a = 0; a < sizeof attrs / sizeof attrs[0]; a++)
              {
                scr_char key[32];
                scr_int lo = 0, hi = -1;
                snprintf (key, sizeof key, "%sHi", attrs[a]);
                bk[3].string = key;
                if (prop_get (bundle, "I<-siss", &bv, bk)) hi = bv.integer;
                snprintf (key, sizeof key, "%sLo", attrs[a]);
                bk[3].string = key;
                if (prop_get (bundle, "I<-siss", &bv, bk)) lo = bv.integer;
                if (hi < 0)
                  {
                    bk[3].string = (scr_char *) attrs[a];
                    if (prop_get (bundle, "I<-siss", &bv, bk)) lo = hi = bv.integer;
                    else hi = 0;
                  }
                fprintf (stderr, " %s=%ld-%ld", attrs[a], lo, hi);
              }
            bk[3].string = "Attitude"; if (prop_get (bundle, "I<-siss", &bv, bk)) att = bv.integer;
            bk[3].string = "Speed";    if (prop_get (bundle, "I<-siss", &bv, bk)) spd = bv.integer;
            bk[3].string = "KilledTask"; if (prop_get (bundle, "I<-siss", &bv, bk)) kt = bv.integer;
            fprintf (stderr, " attitude=%ld speed=%ld killedTask=%ld\n",
                     att, spd, kt - 1);
          }

        nk[2].string = "Walks";
        wcount = prop_get_child_count (bundle, "I<-sis", nk);
        for (w = 0; w < wcount; w++)
          {
            scr_int loop = 0, st = 0, ct = 0, mo = 0, ot = 0, sp = 0, mc = 0;
            scr_int rc, s;
            nk[3].integer = w;
            nk[4].string = "BLoop";       if (prop_get (bundle, "B<-sisis", &nv, nk)) loop = nv.integer;
            nk[4].string = "StartTask";   if (prop_get (bundle, "I<-sisis", &nv, nk)) st = nv.integer;
            nk[4].string = "CharTask";    if (prop_get (bundle, "I<-sisis", &nv, nk)) ct = nv.integer;
            nk[4].string = "MeetObject";  if (prop_get (bundle, "I<-sisis", &nv, nk)) mo = nv.integer;
            nk[4].string = "ObjectTask";  if (prop_get (bundle, "I<-sisis", &nv, nk)) ot = nv.integer;
            nk[4].string = "StoppingTask";if (prop_get (bundle, "I<-sisis", &nv, nk)) sp = nv.integer;
            nk[4].string = "MeetChar";    if (prop_get (bundle, "I<-sisis", &nv, nk)) mc = nv.integer;
            fprintf (stderr,
                     "  WALK %ld loop=%ld startTask=%ld charTask=%ld(task%ld)"
                     " meetChar=%ld meetObj=%ld objTask=%ld(task%ld) stopTask=%ld\n",
                     w, loop, st, ct, ct - 1, mc, mo, ot, ot - 1, sp);
            nk[4].string = "Rooms";
            rc = prop_get_child_count (bundle, "I<-sisis", nk);
            for (s = 0; s < rc; s++)
              {
                scr_int rm = 0;
                nk[5].integer = s;
                if (prop_get (bundle, "I<-sisisi", &nv, nk)) rm = nv.integer;
                fprintf (stderr, "    step %ld dest=%ld\n", s, rm);
              }
          }
      }
  }

  /* Room exits + their task gates.  The exit array is in ADRIFT's own direction
     order -- the diagonals are NE,SE,SW,NW, NOT NE,NW,SE,SW (sclibrar.cpp's
     DIRNAMES, and mapdraw.cpp's map_dirs, which both engines share).  Getting
     this wrong mislabels every diagonal exit and sends a route derivation into
     the wrong room. */
  {
    scr_vartype_t rk[5];
    scr_int r, rcount;
    static const char *dirs[] =
      { "N","E","S","W","U","D","IN","OUT","NE","SE","SW","NW" };

    /* The game's own win/lose text.  Empty WinText is what makes the engine
     * fall back to its hard-coded "Congratulations!", so seeing this is the
     * quick way to tell an engine-supplied ending line from an authored one
     * when a published transcript and our own disagree about how many there
     * should be. */
    {
      scr_vartype_t hk[2], hv;

      hk[0].string = "Header";
      hk[1].string = "WinText";
      fprintf (stderr, "WINTEXT [%s]\n",
               prop_get (bundle, "S<-ss", &hv, hk) && hv.string
               ? hv.string : "");
    }

    rk[0].string = "Rooms";
    rcount = prop_get_child_count (bundle, "I<-s", rk);
    for (r = 0; r < rcount; r++)
      {
        scr_vartype_t rnv;
        const scr_char *rname = NULL;
        rk[1].integer = r; rk[2].string = "Short";
        if (prop_get (bundle, "S<-sis", &rnv, rk)) rname = rnv.string;
        fprintf (stderr, "ROOM %ld [%s]\n", r, rname ? rname : "");
      }
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
            rk[4].string = "Dest"; if (prop_get (bundle, "I<-sisis", &rv, rk)) dest = rv.integer; else continue;
            rk[4].string = "Var1"; if (prop_get (bundle, "I<-sisis", &rv, rk)) v1 = rv.integer;
            rk[4].string = "Var2"; if (prop_get (bundle, "I<-sisis", &rv, rk)) v2 = rv.integer;
            rk[4].string = "Var3"; if (prop_get (bundle, "I<-sisis", &rv, rk)) v3 = rv.integer;
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
                fprintf (stderr, "EXIT room=%ld %s -> dest=%ld",
                         r, dirs[d], dest - 1);
                if (v1 - 1 < 0)
                  fprintf (stderr, " (no gate)\n");
                else if (v3 == 0)
                  /* lib_can_go restricts when done == (Var2 != 0), so the exit
                     opens on the OPPOSITE of Var2 -- Var2 == 0 means "the task
                     must have been done". */
                  fprintf (stderr, " gateTask=%ld wantDone=%ld\n",
                           v1 - 1, (scr_int) (v2 == 0));
                else
                  {
                    scr_int obj = obj_stateful_object (game, v1 - 1);
                    const scr_char *s = scdump_object_name (game, obj);
                    fprintf (stderr, " gateObj=%ld [%s] wantState=%ld\n",
                             obj, s ? s : "", v2);
                  }
              }
          }
      }

    /* Room description alts.  In 4.0 these are an authored array; in 3.9/3.8
     * they are synthesised by parse_fixup_v390_v380_room_alts() from the
     * fixed AltDesc/Task1/Task2/LastDesc fields, so dumping them is how the
     * gen400 conversion oracle checks that fixup (see RUNNER_TESTS_TODO.md
     * §3a). */
    for (r = 0; r < rcount; r++)
      {
        scr_int a, acount;
        rk[1].integer = r;
        rk[2].string = "Alts";
        acount = prop_get_child_count (bundle, "I<-sis", rk);
        for (a = 0; a < acount; a++)
          {
            scr_int type = 0, v2 = 0, v3 = 0, disp = 0, hide = 0;
            const scr_char *m1 = NULL, *m2 = NULL;
            scr_vartype_t av;
            rk[3].integer = a;
            rk[4].string = "Type"; if (prop_get (bundle, "I<-sisis", &av, rk)) type = av.integer;
            rk[4].string = "Var2"; if (prop_get (bundle, "I<-sisis", &av, rk)) v2 = av.integer;
            rk[4].string = "Var3"; if (prop_get (bundle, "I<-sisis", &av, rk)) v3 = av.integer;
            rk[4].string = "DisplayRoom"; if (prop_get (bundle, "I<-sisis", &av, rk)) disp = av.integer;
            rk[4].string = "HideObjects"; if (prop_get (bundle, "I<-sisis", &av, rk)) hide = av.integer;
            rk[4].string = "M1"; if (prop_get (bundle, "S<-sisis", &av, rk)) m1 = av.string;
            rk[4].string = "M2"; if (prop_get (bundle, "S<-sisis", &av, rk)) m2 = av.string;
            fprintf (stderr,
                     "ALT room=%ld alt=%ld type=%ld v2=%ld v3=%ld"
                     " disp=%ld hide=%ld m1=[%s] m2=[%s]\n",
                     r, a, type, v2, v3, disp, hide,
                     m1 ? m1 : "", m2 ? m2 : "");
          }
      }
  }

  fflush (stderr);
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
      trace_player = getenv ("SCR_TRACE_PLAYER") != NULL;
      trace_obj = getenv ("SCR_TRACE_OBJ");
      trace_judy = getenv ("SCR_TRACE_JUDY") != NULL;
      trace_vars = getenv ("SCR_TRACE_VARS");
    }

  /* SCR_TRACE_PLAYER: just the player's room each turn (maze-mapping aid). */
  if (trace_player)
    {
      fprintf (stderr, "PLAYERROOM room=%ld stamina=%ld",
               gs_playerroom (game), game->playerstamina);
      {
        const scr_char *ov = trace_obj;
        if (ov)
          {
            scr_int oi = atol (ov);
            fprintf (stderr, " obj%ld_pos=%ld state=%ld", oi,
                     gs_object_position (game, oi), gs_object_state (game, oi));
          }
      }
      fprintf (stderr, "\n");
      fflush (stderr);
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
      scr_int count, index;

      vt_key[0].string = "Variables";
      count = prop_get_child_count (bundle, "I<-s", vt_key);

      fprintf (stderr, "VARTRACE");
      for (index = 0; index < count; index++)
        {
          const scr_char *name;
          scr_int var_type;

          vt_key[1].integer = index;
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

          if (!var_get (vars, name, &var_type, &vt_rvalue)
              || var_type != VAR_INTEGER)
            continue;
          if (all)
            fprintf (stderr, " %ld:%s=%ld", index, name, vt_rvalue.integer);
          else
            fprintf (stderr, " %s=%ld", name, vt_rvalue.integer);
        }
      fprintf (stderr, "\n");
      fflush (stderr);
    }

  if (!trace_judy)
    return;
  for (npc = 0; npc < gs_npc_count (game); npc++)
    fprintf (stderr, "JUDYTRACE npc=%ld room=%ld\n",
             npc, gs_npc_location (game, npc) - 1);
  fflush (stderr);
}

#endif /* SCARIER_DUMP_TOOLS */
