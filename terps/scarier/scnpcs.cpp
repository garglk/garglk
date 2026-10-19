/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Module notes:
 *
 * o ...
 */

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Trace flag, set before running. */
static scr_bool npc_trace = FALSE;

/*
 * The counter the 4.0 Runner stamps on a non-looping walk that has just
 * run out; see npc_tick_npc().  The 4.0 ticker dropped the pre-4.0 "only
 * looping walks restart" test from its restart branch, so it needs a
 * sentinel that no longer compares greater than zero: a walk sitting on
 * it is never decremented, never restarted, never run, and -- the part
 * that matters for the_pk_girl -- never takes precedence over a
 * lower-numbered walk.  In the P-code the literal is the same "&HFF"
 * that the descending stop scan uses as its Step, i.e. -1.
 */
static const scr_int NPC_WALK_EXPIRED = -1;


/*
 * npc_walk_meetobject_needs_fixup()
 *
 * A walk's MeetObject is stored in the TAF as a 1-based dynamic-object
 * index, but the runtime needs a global object index.  The parser rewrites
 * it (the "|V380_WALK:_MeetObject_|" fixup in sctafpar.c) only for version
 * 3.8 games; the version 3.9 and 4.0 WALK schemas read it raw.  Return TRUE
 * when the conversion still has to be done at run time, i.e. for any game
 * newer than 3.8.  Without it a walk's ObjectTask checks the wrong object
 * and never fires (e.g. the milk-bowl fairy lure in "Lair of the CyberCow"
 * was uncatchable, making the game unwinnable).
 */
static scr_bool
npc_walk_meetobject_needs_fixup (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key, vt_rvalue;

  vt_key.string = "Version";
  if (prop_get (bundle, "I<-s", &vt_rvalue, &vt_key))
    return vt_rvalue.integer > TAF_VERSION_380;
  return TRUE;
}


/*
 * npc_version()
 *
 * Return the game's TAF version constant.
 */
static scr_int
npc_version (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key;

  vt_key.string = "Version";
  return prop_get_integer (bundle, "I<-s", &vt_key);
}


/*
 * npc_in_room()
 *
 * Return TRUE if a given NPC is currently in a given room.
 */
scr_bool
npc_in_room (scr_gameref_t game, scr_int npc, scr_int room)
{
  if (npc_trace)
    {
      scr_trace ("NPC: checking NPC %ld in room %ld (NPC is in %ld)\n",
                npc, room, gs_npc_location (game, npc));
    }

  return gs_npc_location (game, npc) - 1 == room;
}


/*
 * npc_count_in_room()
 *
 * Return the count of characters in the room, including the player.
 */
scr_int
npc_count_in_room (scr_gameref_t game, scr_int room)
{
  scr_int count, npc;

  /* Start with the player. */
  count = gs_player_in_room (game, room) ? 1 : 0;

  /* Total up other NPCs inhabiting the room. */
  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      if (gs_npc_location (game, npc) - 1 == room)
        count++;
    }
  return count;
}


/*
 * npc_walk_property()
 * npc_walk_is_loop()
 * npc_walk_total_time()
 *
 * Read a walk's properties.  npc_walk_total_time() is the Runner's
 * "For j = 0 To NumStops - 1: counter = counter + Times(j)" loop, and is
 * both the value a walk's counter is (re)seeded to and the largest value
 * the stop scan can ever match.
 */
static scr_int
npc_walk_property (scr_gameref_t game, scr_int npc, scr_int walk,
                   const scr_char *name)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Walks";
  vt_key[3].integer = walk;
  vt_key[4].string = name;
  return prop_get_integer (bundle, "I<-sisis", vt_key);
}

static scr_bool
npc_walk_is_loop (scr_gameref_t game, scr_int npc, scr_int walk)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Walks";
  vt_key[3].integer = walk;
  vt_key[4].string = "Loop";
  return prop_get_boolean (bundle, "B<-sisis", vt_key);
}

static scr_int
npc_walk_total_time (scr_gameref_t game, scr_int npc, scr_int walk)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[6];
  scr_int stops, stop, total;

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Walks";
  vt_key[3].integer = walk;
  vt_key[4].string = "Times";
  stops = prop_get_child_count (bundle, "I<-sisis", vt_key);

  total = 0;
  for (stop = 0; stop < stops; stop++)
    {
      vt_key[5].integer = stop;
      total += prop_get_integer (bundle, "I<-sisisi", vt_key);
    }
  return total;
}


/*
 * npc_walk_is_enabled()
 *
 * TRUE if a walk's tasks currently allow it to run -- the Runner's "ok"
 * flag, before higher-numbered walks get their say.  A walk is enabled when
 * its StartTask is absent or complete, and is disabled again by a completed
 * StoppingTask.
 *
 * Note what is *not* here: the walk's counter.  Both the walk ticker
 * (run400 468DA0, run390 45A3xx, run380 4412CA) and the room lister's
 * changed-description pick (run400 4727FE, run390 28995) decide from the
 * task state alone, so a walk that has run out of counter is still "the
 * walk this NPC is on" as far as its ChangedDesc is concerned.  3.7 and 3.8
 * have no StoppingTask; their walk schema leaves the property zero, which
 * reads here as "no stopping task" and needs no version test.
 */
scr_bool
npc_walk_is_enabled (scr_gameref_t game, scr_int npc, scr_int walk)
{
  scr_int starttask, stoppingtask;

  starttask = npc_walk_property (game, npc, walk, "StartTask");
  if (starttask > 0 && !gs_task_done (game, starttask - 1))
    return FALSE;

  stoppingtask = npc_walk_property (game, npc, walk, "StoppingTask");
  if (stoppingtask > 0 && gs_task_done (game, stoppingtask - 1))
    return FALSE;

  return TRUE;
}


/*
 * npc_walk_preempts()
 *
 * TRUE if this walk suppresses every lower-numbered walk of the same NPC.
 *
 * An NPC runs at most one walk at a time, and the Runner picks it with this
 * test rather than with any notion of "the active walk": ticking walk w, it
 * loops over w+1 .. NumWalks-1 and clears its "ok" flag if any of them
 * preempts (run380 441411, run390 45A6xx, run400 4686E7 -- the same code in
 * all three).  A game-start walk (StartTask 0) preempts unconditionally,
 * even though it may be long finished or have no stops at all; a
 * task-started walk preempts only while its counter is still running.
 * Either way a completed StoppingTask takes the walk out of the running
 * (3.7/3.8 have no StoppingTask and so always suppress).
 *
 * This is what stops "The Fun House" (4.00) walking its Bouncer: NPC 5's
 * WALK 2 is an empty game-start walk, which silently pins WALK 1 shut for
 * the whole game, so the bouncer never approaches the player and his
 * meet-object task never fires.  Scarier used to test "is walk w+1 still
 * counting down" instead, which let the empty walk expire on turn one and
 * released the walk the Runner keeps closed.
 */
static scr_bool
npc_walk_preempts (scr_gameref_t game, scr_int npc, scr_int walk)
{
  scr_int starttask, stoppingtask;

  starttask = npc_walk_property (game, npc, walk, "StartTask");
  if (starttask > 0)
    {
      if (!gs_task_done (game, starttask - 1))
        return FALSE;
      if (gs_npc_walkstep (game, npc, walk) <= 0)
        return FALSE;
    }

  stoppingtask = npc_walk_property (game, npc, walk, "StoppingTask");
  if (stoppingtask > 0 && gs_task_done (game, stoppingtask - 1))
    return FALSE;

  return TRUE;
}


/*
 * npc_start_walk_is_390_noop()
 *
 * Pre-4.0 Runners never run a NON-LOOPING game-start walk at all: the NPC
 * stays in its start room for the whole game and the walk's CharTask never
 * fires.  The 4.0 Runner runs the same walk: it arrives on turn one and
 * fires its CharTask once -- a genuine version split, like the walk-task
 * dispatch.
 *
 * This is what the decompiled ticker says.  There is no game-start seeding
 * anywhere in the Runner; the ONLY thing that ever puts a counter on a walk
 * that no task has started is the ticker's own "restart a spent walk"
 * branch, and pre-4.0 that branch is gated on the walk looping (run380
 * 441389, run390 45A585) while 4.0 takes it unconditionally.  A non-looping
 * game-start walk therefore sits on a zero counter for ever before 4.0.
 * npc_setup_initial() below seeds one turn more than the walk's total so
 * that the first tick lands exactly on stop zero, which is the same arrival
 * the ticker's own restart branch produces -- so for the walks that DO run
 * the seeding is redundant, and this predicate takes out the ones that do
 * not.
 *
 * Measured twice, both live:
 *
 *   - walk probe C (2026-08-01, run390) -- a one-stop non-looping game-start
 *     walk: Bob never arrives, "You cannot see Bob from here.", and the
 *     CharTask never fires.  run400 walks the same NPC in on turn one.
 *
 *   - "Melbourne Beach" (3.90) under run390, Adrift_37_melbourne_beach.txt
 *     (2026-08-25) -- Judy's walk is a SIX-stop non-looping game-start walk
 *     (Kitchen 10, Eating area 10, Den 5, Judy's bedroom 15, follow 5,
 *     Outside den 1).  Scarier used to walk her, which put her in her
 *     bedroom on turns 26-40; the Runner still has her in her start room
 *     (the Kitchen) on turn 18, and all twenty `give trumpet to judy` in the
 *     bedroom on turns 36-55 answer with task 17's third restriction, "You
 *     can't do that in your present company." -- the "player in the same
 *     room as Judy" test, failing.  No shift of the walk's start can fit
 *     both observations; the walk simply never runs.
 *
 * So the rule is the wide one after all.  Only the game-start (StartTask 0)
 * case is covered: a walk a TASK starts is seeded by npc_start_npc_walk()
 * when that task completes, whatever its version and whether or not it
 * loops, and "deaths" (3.9) ends with a demon walked in by exactly such a
 * one-stop walk.
 */
static scr_bool
npc_start_walk_is_390_noop (scr_gameref_t game, scr_int npc, scr_int walk)
{
  if (npc_version (game) >= TAF_VERSION_400)
    return FALSE;

  return !npc_walk_is_loop (game, npc, walk);
}


/*
 * npc_start_npc_walk()
 *
 * Start the given walk for the given NPC.  This is the seeding done when a
 * walk's StartTask completes: the Runner sets the counter to one and then
 * adds every stop's Times, so the tick later in the same turn brings it down
 * to the walk's total and arrives at stop zero.
 */
void
npc_start_npc_walk (scr_gameref_t game, scr_int npc, scr_int walk)
{
  gs_set_npc_walkstep (game, npc, walk,
                       npc_walk_total_time (game, npc, walk) + 1);
}


/*
 * npc_turn_update()
 * npc_setup_initial()
 *
 * Set initial values for NPC states, and update on turns.
 */
void
npc_turn_update (scr_gameref_t game)
{
  scr_int index_;

  /* Set current values for NPC seen states. */
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (!gs_npc_seen (game, index_)
          && npc_in_room (game, index_, gs_playerroom (game)))
        gs_set_npc_seen (game, index_, TRUE);
    }
}

void
npc_setup_initial (scr_gameref_t game)
{
  scr_int npc;

  /*
   * Start any walk that does not depend on a StartTask.  The Runner has no
   * such loop in the walk ticker -- there a spent counter is reseeded only
   * for a looping walk (pre-4.0) or unconditionally (4.0) -- so the seeding
   * of a game-start walk happens somewhere else in it; see the note in
   * npc_start_walk_is_390_noop() for what is measured and what is not.
   */
  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      scr_int walk;

      for (walk = gs_npc_walkstep_count (game, npc) - 1; walk >= 0; walk--)
        {
          if (npc_walk_property (game, npc, walk, "StartTask") == 0
              && !npc_start_walk_is_390_noop (game, npc, walk))
            npc_start_npc_walk (game, npc, walk);
        }
    }

  /* Update seen flags for initial states. */
  npc_turn_update (game);
}


/*
 * npc_room_in_roomgroup()
 *
 * Return TRUE if a given room is in a given group.
 */
static scr_bool
npc_room_in_roomgroup (scr_gameref_t game, scr_int room, scr_int group)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  scr_int member;

  /* Check roomgroup membership. */
  vt_key[0].string = "RoomGroups";
  vt_key[1].integer = group;
  vt_key[2].string = "List";
  vt_key[3].integer = room;
  member = prop_get_integer (bundle, "I<-sisi", vt_key);
  return member != 0;
}


/* List of direction names, for printing entry/exit messages. */
static const scr_char *const DIRNAMES_4[] = {
  "the north", "the east", "the south", "the west", "above", "below",
  "inside", "outside",
  NULL
};
static const scr_char *const DIRNAMES_8[] = {
  "the north", "the east", "the south", "the west", "above", "below",
  "inside", "outside",
  "the north-east", "the south-east", "the south-west", "the north-west",
  NULL
};

/*
 * npc_random_adjacent_roomgroup_member()
 *
 * Return a random member of group adjacent to given room.
 */
static scr_int
npc_random_adjacent_roomgroup_member (scr_gameref_t game,
                                      scr_int room, scr_int group)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_bool eightpointcompass;
  scr_int roomlist[12], count, length, index_;

  /* If given room is "hidden", return nothing. */
  if (room == -1)
    return -1;

  /* How many exits to consider? */
  eightpointcompass = prop_get_global_boolean (bundle, "EightPointCompass");
  if (eightpointcompass)
    length = sizeof (DIRNAMES_8) / sizeof (DIRNAMES_8[0]) - 1;
  else
    length = sizeof (DIRNAMES_4) / sizeof (DIRNAMES_4[0]) - 1;

  /* Poll adjacent rooms. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Exits";
  count = 0;
  for (index_ = 0; index_ < length; index_++)
    {
      scr_int adjacent;

      vt_key[3].integer = index_;
      vt_key[4].string = "Dest";
      adjacent = prop_get_child_count (bundle, "I<-sisis", vt_key);

      if (adjacent > 0 && npc_room_in_roomgroup (game, adjacent - 1, group))
        {
          roomlist[count] = adjacent - 1;
          count++;
        }
    }

  /* Return a random adjacent room, or -1 if nothing is adjacent. */
  return (count > 0) ? roomlist[scr_randomint (0, count - 1)] : -1;
}


/*
 * The direction names the Runner's wherefrom() answers with, in exit order.
 * wherefrom(roomfrom, roomto) scans *roomfrom's* exits for roomto and then
 * reports that exit's *opposite* -- it names the direction roomfrom lies in
 * as seen from roomto.  "Alice walks off to the north." therefore means the
 * room Alice is heading for has a south exit back into the player's room.  On
 * a symmetric map that is the same answer SCARE's old forward scan of the
 * player's own room gave; on a one-way map it is not.  This table is
 * DIRNAMES_8 reversed, index for index.
 */
static const scr_char *const WHEREFROM_DIRNAMES[] = {
  "the south", "the west", "the north", "the east", "below", "above",
  "outside", "inside",
  "the south-west", "the north-west", "the north-east", "the south-east"
};

/* wherefrom()'s two non-direction answers.  Both are compared as strings by
   the Runner, and "not moved" is even printed as one -- see npc_announce(). */
static const scr_char NOT_MOVED[] = "not moved";
static const scr_char NOWHERE[] = "nowhere";

/*
 * npc_wherefrom()
 *
 * Port of the Runner's wherefrom() (run370 @422F8C, run380 @42800C, run390
 * @430200, run400 Proc_19_20 @45234C -- one routine, unchanged across all
 * four).  Rooms are numbered from zero here and -1 is "hidden"; the Runner
 * numbers them from one and spells "not a room" two ways (0 for an NPC that
 * has never been anywhere, 0xFF for one a walk has just hidden), which is why
 * it needs both of its guards where we need one.
 *
 * Note there is no early exit from the scan: the *last* matching exit wins,
 * not the first, so a room reachable by two exits is named by the higher one.
 * Pre-4.0 scans eight exits, 4.0 twelve, and neither consults
 * EightPointCompass -- so a diagonal move is nameless before 4.0.
 */
static const scr_char *
npc_wherefrom (scr_gameref_t game, scr_int roomfrom, scr_int roomto)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5], vt_rvalue;
  const scr_char *retval;
  scr_int length, dir;

  retval = NULL;
  if (roomfrom == roomto)
    retval = NOT_MOVED;
  if (roomfrom == -1)
    return NOWHERE;

  length = (npc_version (game) >= TAF_VERSION_400) ? 12 : 8;

  vt_key[0].string = "Rooms";
  vt_key[1].integer = roomfrom;
  vt_key[2].string = "Exits";
  for (dir = 0; dir < length; dir++)
    {
      vt_key[3].integer = dir;
      if (prop_get (bundle, "I<-sisi", &vt_rvalue, vt_key))
        {
          scr_int dest;

          vt_key[4].string = "Dest";
          dest = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
          if (dest == roomto)
            retval = WHEREFROM_DIRNAMES[dir];
        }
    }

  if (!retval)
    retval = NOWHERE;

  /* The Runner's "roomto is not a room" guard, and it overrides everything
     above -- including "not moved" for a walk from hidden to hidden. */
  if (roomto == -1)
    retval = NOWHERE;

  return retval;
}


/*
 * npc_announce()
 *
 * Print an NPC's walk departure or arrival line.  other_room is the room at
 * the far end of the move -- where the NPC is going for a departure, where it
 * came from for an arrival -- and the caller has already established that the
 * player is in the room the NPC is leaving or entering.
 *
 * Both lines are built the same way (run370 @4393BB / @43955E, run380
 * @44153F / @4416F4, run390 @45A7DB / @45A97B, run400 @4688C0 / @468A64):
 * the NPC's name, its ExitText or EnterText, and then a direction clause
 * naming *the other room* as seen from the player's.  The departure's version
 * split is in the gates, not the wording:
 *
 *   3.7  suppressed only on "nowhere", so a walk whose stop *is* the player's
 *        room prints the sentinel verbatim -- "Alice walks off to not
 *        moved." really is what run370 says, and arlo says it twice;
 *   3.8  and 3.9 suppress both "nowhere" and "not moved";
 *   4.0  suppresses only "not moved", and prints a bare "X walks off." when
 *        the direction comes back "nowhere".
 *
 * The arrival has no direction gate at all -- a "nowhere" answer just drops
 * the " from ..." clause -- because its caller has already tested that the
 * NPC came from somewhere else.
 *
 * "outside" is special-cased in every Runner: "walks off outside." rather
 * than "walks off to outside.".
 *
 * The line is JOINED onto whatever the turn has printed so far, with the
 * Runner's two-space separator, rather than given a line of its own.  All
 * four Runners do it, the older two with the test written out inline and the
 * newer two through pspace:
 *
 *   run370 loc_4395AA / run380 loc_441740
 *       If Right(buf, 1) <> Chr(10) And Len(buf) > 0 Then buf = buf & "  "
 *   run390 loc_45A99E / run400 loc_468A67
 *       Call pspace()   ' the same test plus "already ends in two spaces"
 *                       ' and "already ends in <br>"
 *
 * which is what pf_buffer_join() is.  This is not cosmetic: the arrival
 * sentence lands in the same buffer the ALR pass later walks, so an author
 * can -- and David Whyld routinely does -- write an ALR whose Original spans
 * the join and deletes the arrival at a named spot.  sophie.taf carries a
 * whole family of them, e.g.
 *
 *   'quiet.  Grumble complaining of beer deprivation staggers in from the
 *    west.'  ->  'quiet.'
 *
 * and that is why eight of its first fifty turns say nothing about Grumble.
 * Buffered on a line of its own, as this used to be, no such ALR can ever
 * match.  Measured with harness/make_400_walkalrprobe.py under run400
 * (Adrift_47.txt): its cross-the-join ALRs both fire, and the single-space
 * twin of one of them does not, so the separator really is two spaces.
 *
 * Whether the Name is capitalised is a version split of its own, and one the
 * join makes visible: 3.7, 3.8 and 3.9 concatenate the Name verbatim
 * (run370 loc_43961A / run380 loc_4417B0 / run390 loc_45AA0F all push the raw
 * field), while 4.0 puts it through the Runner's one-line capitaliser first
 * (run400 loc_468A79 and loc_4688E0 both call
 *     Proc_21_3_446BB4 = UCase(Left(s, 1)) & Right(s, Len(s) - 1)
 * General.bas:75).  It matters for exactly the NPCs whose Name starts
 * lower-case, which is not a hypothetical: baroo.taf (4.00) names its walkers
 * "wizard" and "warlock" with Prefix "the", so 4.0 really does print "Wizard
 * strides off to the east." while a 3.9 game in the same shape would print
 * "wizard".  SCARE used to capitalise unconditionally, which was right for
 * 4.0 and wrong for everything older.
 *
 * The join carries the version split pf_buffer_join_always() exists for: 3.9
 * and 4.0 go through pspace, which does not add a separator to text that
 * already ends in one, while 3.7 and 3.8 add theirs unconditionally and so
 * really do make four spaces there.
 */
static void
npc_announce (scr_gameref_t game, scr_int npc,
              scr_bool is_exit, scr_int other_room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  const scr_char *text, *name, *dir;
  scr_bool showenterexit, is_400;

  /* If no announcement required, return immediately. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "ShowEnterExit";
  showenterexit = prop_get_boolean (bundle, "B<-sis", vt_key);
  if (!showenterexit)
    return;

  is_400 = npc_version (game) >= TAF_VERSION_400;
  dir = npc_wherefrom (game, other_room, gs_playerroom (game));

  if (is_exit)
    {
      if (strcmp (dir, NOT_MOVED) == 0 && npc_version (game) > TAF_VERSION_370)
        return;
      if (strcmp (dir, NOWHERE) == 0 && !is_400)
        return;
    }

  /* Get exit or entry text, and NPC name. */
  vt_key[2].string = is_exit ? "ExitText" : "EnterText";
  text = prop_get_string (bundle, "S<-sis", vt_key);
  vt_key[2].string = "Name";
  name = prop_get_string (bundle, "S<-sis", vt_key);

  /* Print NPC exit/entry details, run on from the turn's text so far. */
  if (is_400)
    pf_new_sentence (filter);
  if (npc_version (game) >= TAF_VERSION_390)
    pf_buffer_join (filter, name);
  else
    pf_buffer_join_always (filter, name);
  pf_buffer_character (filter, ' ');
  pf_buffer_string (filter, text);
  if (strcmp (dir, NOWHERE) != 0)
    {
      if (is_exit)
        pf_buffer_string (filter, strcmp (dir, "outside") == 0 ? " " : " to ");
      else
        pf_buffer_string (filter, " from ");
      pf_buffer_string (filter, dir);
    }
  pf_buffer_string (filter, ".\n");

  /* Handle any associated resource. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Res";
  vt_key[3].integer = is_exit ? 3 : 2;
  res_handle_resource (game, "sisi", vt_key);
}


/*
 * npc_announce_hidden()
 *
 * Print the directionless departure line a walk stop of "Hidden" gets.  This
 * is a separate branch in the Runner -- the else of its "the stop is a room"
 * test -- and it is not the same code as npc_announce(): there is no
 * direction clause and no wherefrom() call, just the name, the ExitText and a
 * full stop.  arlo's "Rude Customer walks off." is one of these.
 *
 * It exists in 3.7 (run370 loc_4397A3) and in 4.0 (run400 loc_468CF9) only.
 * run380 loc_4418DD and run390 have nothing in that branch but the "stamp the
 * NPC nowhere" assignment, so a 3.8 or 3.9 walker vanishes in silence.
 *
 * Both sites append a bare "  " with no pspace call and push the raw Name --
 * so this branch joins unconditionally, and 4.0 does NOT capitalise here even
 * though its two npc_announce() sites do.
 */
static void
npc_announce_hidden (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  const scr_char *text, *name;
  scr_int version;

  version = npc_version (game);
  if (version != TAF_VERSION_370 && version < TAF_VERSION_400)
    return;

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "ShowEnterExit";
  if (!prop_get_boolean (bundle, "B<-sis", vt_key))
    return;

  vt_key[2].string = "ExitText";
  text = prop_get_string (bundle, "S<-sis", vt_key);
  vt_key[2].string = "Name";
  name = prop_get_string (bundle, "S<-sis", vt_key);

  /*
   * Joined on like npc_announce()'s lines, though here the two Runners that
   * have this branch at all append the separator unguarded --
   *
   *   run370 loc_4397A3   buf = buf & "  " & Name & " " & ExitText & "."
   *   run400 loc_468CF9   the same, no pspace call
   *
   * -- so a turn whose only output is a hidden departure really does open
   * with two spaces there, and text already ending in two spaces gets two
   * more in both.  The empty-buffer half of pf_buffer_join_always()'s guard
   * is kept: it can differ only in that leading-whitespace case, which
   * nothing can depend on.
   */
  pf_buffer_join_always (filter, name);
  pf_buffer_character (filter, ' ');
  pf_buffer_string (filter, text);
  pf_buffer_string (filter, ".\n");

  /* No resource here: unlike the two announcements above, neither run370's
     loc_4397A3 nor run400's loc_468CF9 touches the NPC's Res entries. */
}


/*
 * npc_tick_npc_walk()
 *
 * Helper for npc_tick_npc().
 */
static void
npc_tick_npc_walk (scr_gameref_t game, scr_int npc, scr_int walk)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[6];
  scr_int roomgroups, movetimes, walkstep, start, dest, destnum;
  scr_int chartask, objecttask;
  scr_bool is_arrival, is_exact;

  if (npc_trace)
    {
      scr_trace ("NPC: ticking NPC %ld, walk %ld: step %ld\n",
                npc, walk, gs_npc_walkstep (game, npc, walk));
    }

  /* Count roomgroups for later use. */
  vt_key[0].string = "RoomGroups";
  roomgroups = prop_get_child_count (bundle, "I<-s", vt_key);

  /* Get move times array length. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Walks";
  vt_key[3].integer = walk;
  vt_key[4].string = "MoveTimes";
  movetimes = prop_get_child_count (bundle, "I<-sisis", vt_key);

  /* Find a step to match the movetime. */
  for (walkstep = 0; walkstep < movetimes - 1; walkstep++)
    {
      scr_int  movetime;

      vt_key[5].integer = walkstep + 1;
      movetime = prop_get_integer (bundle, "I<-sisisi", vt_key);
      if (gs_npc_walkstep (game, npc, walk) > movetime)
        break;
    }

  /*
   * A stop lasting several turns is only *arrived at* on the turn the
   * counter hits that step's suffix-sum exactly; the turns after it are the
   * NPC standing around.  The Runner runs the walk's CharTask/ObjectTask
   * from its arrival handler, so a multi-turn stay at a fixed room fires
   * them once, not once per turn (run400 walk probe H, live 2026-08-02:
   * Times = 3 in the player's room, 2 away, `CHARTASK FIRED.` on the arrival
   * turn only, and again five turns later when the loop brings Bob back).
   * Scarier used to fire on every co-located tick.
   *
   * The gate covers fixed-room AND follow-player stops.  A roomgroup stop
   * does NOT behave this way -- in "Ticket to No Where" the lost girl
   * wanders a roomgroup on a single Times=4 stop, and live run400 has her
   * speak on two consecutive turns per cycle and move on consecutive turns,
   * i.e. it re-runs the whole walk step every tick rather than once per
   * stay.  Firing every tick is what Scarier already does there, so leave
   * roomgroup stops alone.
   *
   * Follow-player stops (walk probe K, live in BOTH Runners 2026-08-02,
   * Times = 3 following / 2 away): the walker warps to the player's room
   * only on the arrival tick, and stands still on the stay turns even when
   * the player then walks away -- there is no per-turn trailing.  (Classic
   * "follows you around" behaviour is a Times=1 follow stop, where every
   * tick is an arrival tick.)  The arrival tick fires the CharTask even
   * when the walker was already in the player's room and so never moved --
   * probe K turn 11 prints no enter line but fires the task -- which the
   * plain is_arrival computation below already gets right.
   */
  vt_key[5].integer = walkstep;
  is_arrival = gs_npc_walkstep (game, npc, walk)
               == prop_get_integer (bundle, "I<-sisisi", vt_key);

  /* The same test, kept unmodified by the destination cases below, because
     the Runner's walk announcements hang off it and not off is_arrival. */
  is_exact = is_arrival;

  /* Sort out a destination. */
  dest = start = gs_npc_location (game, npc) - 1;

  vt_key[4].string = "Rooms";
  vt_key[5].integer = walkstep;
  destnum = prop_get_integer (bundle, "I<-sisisi", vt_key);

  if (destnum == 0)          /* Hidden. */
    {
      /* Hide only on the exact tick: run390 stamps the &HFF inside the
         counter==suffix gate (loc_45ABB8), run400 likewise (the whole step
         at loc_468841); until the counter lands, the walker stays visible. */
      if (is_exact)
        dest = -1;
      is_arrival = TRUE;
    }
  else if (destnum == 1)     /* Follow player. */
    {
      /* Warp to the player only on the arrival tick; between arrivals the
         walker stands still even if the player moves away (probe K). */
      if (is_arrival)
        dest = gs_playerroom (game);
    }
  else if (destnum < gs_room_count (game) + 2)
    {
      /* To room -- but only on the exact arrival tick.  run390 loc_45A780
         and run400 loc_468841 gate the *entire* walk step, the move
         included, on counter == suffix_sum, so between arrivals the walker
         stands wherever it is -- even when a task has displaced it.  Live
         run390, Merry_Murders 2026-08-31: task 9 starts Trey's walk to the
         Plaza (arrives next tick), then task 27 moves him into the Hallway,
         and he stays there for the rest of the game; Scarier used to warp
         him back every turn.  Same mechanism as provenance's butler. */
      if (is_exact)
        dest = destnum - 2;
    }
  else if (destnum < gs_room_count (game) + 2 + roomgroups)
    {
      scr_int initial;

      is_arrival = TRUE;

      /* For roomgroup walks, move only if walksteps has just refreshed. */
      vt_key[4].string = "MoveTimes";
      vt_key[5].integer = 0;
      initial = prop_get_integer (bundle, "I<-sisisi", vt_key);
      if (gs_npc_walkstep (game, npc, walk) == initial)
        {
          scr_int group;

          group = destnum - 2 - gs_room_count (game);
          dest = npc_random_adjacent_roomgroup_member (game, start, group);
          if (dest == -1)
            dest = lib_random_roomgroup_member (game, group);
          if (dest == -1)
            dest = start;        /* Empty group: the NPC stays put. */
        }
    }

  /*
   * Announce the departure, move, then announce the arrival -- the Runner's
   * order, and its departure gate reads the NPC's location *before* the move,
   * so a stop naming the room the walker already stands in still gets a line
   * in 3.7 (see npc_announce()).  It fires only on the turn the counter lands
   * exactly on this stop's suffix sum: the Runner does the whole
   * move-and-announce inside that test (run370 loc_439360, run400
   * loc_468841), so a multi-turn stay is announced once, not once per turn.
   * That is is_exact, not is_arrival, which the Hidden and roomgroup cases
   * above force true for the benefit of the meet-task dispatch below.
   *
   * A "follow the player" stop is announced by no Runner at all, even though
   * the walker is about to warp to the player: 4.0 gates its departure branch
   * on the resolved destination being a real room (run400 @4688B0, var_BE > 0,
   * and a follow stop resolves to the not-a-room zero), while 3.7/3.8/3.9 do
   * reach the branch but hand wherefrom() that same zero, whose exit-less
   * dummy room answers "nowhere" -- which every one of them suppresses.  One
   * destnum > 1 test therefore stands in for both shapes.
   */
  if (is_exact && gs_player_in_room (game, start))
    {
      if (destnum == 0)
        npc_announce_hidden (game, npc);
      else if (destnum > 1)
        npc_announce (game, npc, TRUE, dest);
    }

  /* See if the NPC actually moved. */
  if (start != dest)
    {
      scr_bool was_walk_hidden;

      if (npc_trace)
        scr_trace ("NPC: walking NPC %ld moved to %ld\n", npc, dest);

      /* Move NPC to destination, remembering first whether the location we
         are leaving was a walk's Hidden stamp -- the setter clears it. */
      was_walk_hidden = gs_npc_walk_hidden (game, npc);
      gs_set_npc_location (game, npc, dest + 1);

      /*
       * Announce the arrival, unless the player watched the departure.
       * 3.8, 3.9 and 4.0 add one more test: the walker's pre-move location
       * must not be the Runner's *never placed* not-a-room zero (run380
       * @4416F4, run390 loc_45A99B, run400 @468A5D -- the whole gate there is
       * ShowEnterExit AND old <> playerroom AND old <> 0).  A walk that hides
       * its NPC stamps &HFF instead (run400 loc_468D4A), which passes that
       * test, so a hidden walker's next arrival still gets a line -- just a
       * directionless one, wherefrom() bailing out on the &HFF.  Both are
       * location 0 here, which is what gs_npc_walk_hidden() tells apart.
       * 3.7 has no such test at all (run370 @43955E).
       */
      if (!gs_player_in_room (game, start) && gs_player_in_room (game, dest)
          && (start != -1
              || was_walk_hidden
              || npc_version (game) == TAF_VERSION_370))
        npc_announce (game, npc, FALSE, start);
    }

  /*
   * Stamp the walk-hidden marker, and do it whether or not the step above
   * was a move.  The Runner writes its &HFF into the location field from the
   * Hidden branch itself (run400 loc_468D4A), with no "did it move" test in
   * front of it, so a walker that was ALREADY nowhere when the Hidden stop
   * came round still comes out of it carrying the marker -- and its next
   * arrival therefore passes the "old <> 0" half of the announcement gate
   * above.  Scarier used to stamp only inside the move branch, which left
   * such a walker on a genuine zero for ever and swallowed the arrival.
   *
   * That is The Skydiver's Pelican exactly: StartRoom 0 and a walk whose
   * first stop is Hidden, arriving on turn 16 with run400 printing "Pelican
   * A pelican flocked toward me.." and Scarier printing nothing.  Measured
   * with harness/make_400_walkhiddenprobe.py under run400 (Adrift_910.txt,
   * 2026-09-07): of three walkers all arriving in the player's room, the one
   * on the Skydiver's shape (nowhere already, Hidden stop, then the room)
   * announces "Hid wanders in." with no direction, the one that arrives from
   * a never-touched zero says nothing, and the one leaving a real room
   * announces with a direction.  The middle cell is what keeps "old <> 0" in
   * the gate; this stamp is what lets the first past it.
   *
   * 3.9 answers the same three ways: make_39_walkhiddenprobe.py under
   * run390 (Adrift_911.txt, 2026-09-07) reproduces the cells exactly, so the
   * stamp is unconditional there too (run390 loc_45ABB8, gate loc_45A99B).
   * ALEXIS.TAF is the 3.90 corpus row that moves with it -- seeded, hence
   * unmeasurable itself, which is what the probe stands in for.  3.7 has no
   * "old <> 0" term in its gate at all (run370 @43955E), so the stamp is
   * invisible there; 3.8 shares 3.9's gate (run380 @4416F4).
   */
  if (is_exact && destnum == 0)
    gs_set_npc_walk_hidden (game, npc, TRUE);

  /* Handle meeting characters and objects -- arrival turns only. */
  if (!is_arrival)
    return;

  vt_key[4].string = "CharTask";
  chartask = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
  if (chartask >= 0)
    {
      scr_int meetchar;

      /* Run meetchar task if appropriate. */
      vt_key[4].string = "MeetChar";
      meetchar = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
      if ((meetchar == -1 && gs_player_in_room (game, dest))
          || (meetchar >= 0 && dest == gs_npc_location (game, meetchar) - 1))
        {
          run_npc_walk_task (game, chartask);
        }
    }

  vt_key[4].string = "ObjectTask";
  objecttask = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
  if (objecttask >= 0)
    {
      scr_int meetobject;

      /* Run meetobject task if appropriate. */
      vt_key[4].string = "MeetObject";
      meetobject = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
      /* Convert a dynamic-object index to a global one where required. */
      if (meetobject >= 0 && npc_walk_meetobject_needs_fixup (game))
        meetobject = obj_dynamic_object (game, meetobject);
      if (meetobject >= 0 && obj_directly_in_room (game, meetobject, dest))
        {
          run_npc_walk_task (game, objecttask);
        }
    }
}


/*
 * npc_tick_npc()
 *
 * Move an NPC one step along its current walk.
 *
 * This is a straight port of the Runner's per-NPC walk loop (run400 468DA0
 * from loc_4685B0, and the same shape in run390 45A4AE and the readable
 * run380 4412CA).  For each walk, in ascending order:
 *
 *   - decrement a running counter, and in 4.0 stamp a non-looping walk that
 *     has just run out with 0xFF;
 *   - work out whether the walk's tasks enable it;
 *   - if it is enabled and its counter has run out, seed the counter with
 *     the walk's total time -- 4.0 unconditionally, earlier Runners only for
 *     a looping walk;
 *   - let any higher-numbered walk preempt it;
 *   - and if it is still enabled and counting, take the step.
 *
 * Ascending order matters: the precedence test reads the higher walks'
 * counters before they are decremented this turn.
 *
 * The 4.0 expiry stamp is not a "walk finished" flag the Runner tests
 * anywhere -- it is just a large counter.  It keeps the walk preempting
 * lower ones (0xFF is greater than zero) and it keeps counting down, so a
 * finished 4.0 walk quietly replays itself once every 256 turns as the
 * counter descends back through the stop times.  Earlier Runners leave the
 * counter at zero instead, and a finished walk there both stops preempting
 * and stays finished.
 */
static void
npc_tick_npc (scr_gameref_t game, scr_int npc)
{
  const scr_bool is_400 = npc_version (game) >= TAF_VERSION_400;
  scr_int walk_count, walk;

  if (npc_trace)
    scr_trace ("NPC: ticking NPC %ld\n", npc);

  walk_count = gs_npc_walkstep_count (game, npc);
  for (walk = 0; walk < walk_count; walk++)
    {
      scr_int total, other;
      scr_bool enabled;

      /*
       * A corpse takes no more steps.  The Runner opens the walk loop body
       * with this test and jumps clear of the loop -- run400 @4685B6 to
       * @468D61, one past the Next at @468D5C, and run390 @45A4BC to
       * @45ABD0 -- so a dead NPC's counters do not even run down.  It is
       * tested per walk rather than once, because the loop body can run a
       * meet task.
       */
      if (gs_npc_dead (game, npc))
        break;

      /* Count the walk down, and mark a finished 4.0 walk. */
      if (gs_npc_walkstep (game, npc, walk) > 0)
        {
          gs_decrement_npc_walkstep (game, npc, walk);
          if (is_400
              && gs_npc_walkstep (game, npc, walk) == 0
              && !npc_walk_is_loop (game, npc, walk))
            gs_set_npc_walkstep (game, npc, walk, NPC_WALK_EXPIRED);
        }

      enabled = npc_walk_is_enabled (game, npc, walk);

      /* Restart a walk that has run out of counter. */
      total = npc_walk_total_time (game, npc, walk);
      if (enabled
          && gs_npc_walkstep (game, npc, walk) == 0
          && (is_400 || npc_walk_is_loop (game, npc, walk)))
        gs_set_npc_walkstep (game, npc, walk, total);

      /* Higher-numbered walks take precedence over this one. */
      for (other = walk + 1; other < walk_count; other++)
        {
          if (npc_walk_preempts (game, npc, other))
            {
              if (npc_trace)
                {
                  scr_trace ("NPC: NPC %ld walk %ld preempted by walk %ld\n",
                            npc, walk, other);
                }
              enabled = FALSE;
            }
        }

      /*
       * Take the step.  The Runner's gate is just "enabled and counting",
       * its stop scan then doing nothing at all unless the counter matches
       * one of the stop times exactly; the extra test against the walk's
       * total is the same thing said in advance, and keeps npc_tick_npc_walk
       * -- which acts on more than the exact arrival tick -- away from the
       * 0xFF expiry stamp.
       */
      if (enabled
          && gs_npc_walkstep (game, npc, walk) > 0
          && gs_npc_walkstep (game, npc, walk) <= total)
        npc_tick_npc_walk (game, npc, walk);
    }
}


/*
 * npc_tick_npcs()
 *
 * Move each NPC one step along current walk.
 */
void
npc_tick_npcs (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_gameref_t undo = game->undo;
  scr_int npc;

  /*
   * Compare the player location to last turn, to see if the player has moved
   * this turn.  If moved, look for meetings with NPCs.
   *
   * This player-side meet is a 4.0-only rule.  Probed live 2026-08-02
   * (make_400_walkprobe.py L turns 3/8/10, K session 1 turn 8): run400 runs
   * a walk's CharTask whenever the PLAYER moves into the walker's room --
   * at any stop of the walk, fixed, follow-player or the away stop, again
   * on every re-entry -- while run390 prints only "Bob is standing here"
   * on the identical moves (pWKL39, pWKK39) and fires meets solely from
   * the walk's own arrival ticks.  The re-check is also CharTask-only:
   * probe M shows the ObjectTask does not fire when the player walks in on
   * (or drops) the MeetObject beside a mid-stay walker, so this block
   * rightly never looks at ObjectTask.
   *
   * Running this before ticking the NPCs, rather than after, is what puts
   * the messages in the Runner's order; the probes above and the walkthrough
   * corpus both validate the placement.
   *
   * Also, note that we take the shortcut of using the undo gamestate here,
   * rather than properly recording the prior location of the player, and
   * perhaps also NPCs, in the live gamestate.
   */
  if (npc_version (game) >= TAF_VERSION_400
      && undo && !gs_player_in_room (undo, gs_playerroom (game)))
    {
      for (npc = 0; npc < gs_npc_count (game); npc++)
        {
          scr_int walk;

          /* Iterate each NPC's walks. */
          for (walk = gs_npc_walkstep_count (game, npc) - 1; walk >= 0; walk--)
            {
              scr_vartype_t vt_key[5];
              scr_int chartask;

              /* Ignore finished walks. */
              if (gs_npc_walkstep (game, npc, walk) <= 0)
                continue;

              /* Retrieve any character meeting task for the NPC. */
              vt_key[0].string = "NPCs";
              vt_key[1].integer = npc;
              vt_key[2].string = "Walks";
              vt_key[3].integer = walk;
              vt_key[4].string = "CharTask";
              chartask = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
              if (chartask >= 0)
                {
                  scr_int meetchar;

                  /* Run meetchar task if appropriate. */
                  vt_key[4].string = "MeetChar";
                  meetchar = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
                  if (meetchar == -1 &&
                      gs_player_in_room (game, gs_npc_location (game, npc) - 1))
                    {
                      run_npc_walk_task (game, chartask);
                    }
                }
            }
        }
    }

  /* Iterate and tick each individual NPC. */
  for (npc = 0; npc < gs_npc_count (game); npc++)
    npc_tick_npc (game, npc);

#ifdef SCARIER_DUMP_TOOLS
  scr_dump_npc_trace (game);     /* Per-turn NPC-location trace; see scdump.c. */
#endif
}


/*
 * npc_debug_trace()
 *
 * Set NPC tracing on/off.
 */
void
npc_debug_trace (scr_bool flag)
{
  npc_trace = flag;
}
