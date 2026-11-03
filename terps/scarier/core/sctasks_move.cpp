/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
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
 * Task actions that move things: task_run_move_object_action() and
 * task_run_move_npc_action(), with the destination rules of each Runner.
 */

#include "sctasks_internal.h"

/*
 * task_selector_npc()
 *
 * Decode a task action's or restriction's character selector once it is
 * known not to name the player (0): 1 is the referenced character, -1 when
 * none is referenced, and N >= 2 is character N - 2.  Callers test the
 * player value themselves, since what they do for the player differs at
 * every site.
 */
scr_int
task_selector_npc (scr_var_setref_t vars, scr_int var)
{
  return (var == 1) ? var_get_ref_character (vars) : var - 2;
}


/*
 * task_move_object_held_by()
 *
 * The "held by" destination of task_move_object_to(): var3 is 0 for the
 * player, else a character selector.  Returns FALSE if the move was abandoned.
 */
static scr_bool
task_move_object_held_by (scr_gameref_t game, scr_int object, scr_int var3,
                          scr_bool is_v400, scr_bool *stamp_seen)
{
  const scr_var_setref_t vars = gs_get_vars (game);

  if (var3 == 0)            /* Player */
    {
      gs_object_player_get (game, object);
      gs_set_object_runner_parent (game, object, -1);   /* 48C5E0 */
      *stamp_seen = TRUE;
    }
  else                      /* Ref character or NPC id */
    {
      const scr_int npc = task_selector_npc (vars, var3);

      /*
       * No referenced character: run400 abandons the move entirely,
       * skipping the rest of its mover including the post-move seen
       * re-check (Proc_19_10 tests its referenced-character global
       * against the &HFF unset marker and exits, loc_48C650-48C65C).
       */
      if (npc < 0)
        return FALSE;

      /*
       * For an NPC id, run400 alone stamps a present object before
       * this move too.
       */
      if (var3 != 1 && is_v400
          && obj_indirectly_in_room (game, object, gs_playerroom (game)))
        gs_set_object_seen (game, object, TRUE);
      gs_object_npc_get (game, object, npc);
      gs_set_object_runner_parent (game, object, npc);   /* 48C676/48C6E5 */
      /*
       * 3.9 never stamps an object handed to a character (run390
       * execute_action @456099-4560DA writes fields 22 and 42 only).
       */
      *stamp_seen = is_v400
                    && obj_indirectly_in_room (game, object,
                                               gs_playerroom (game));
    }

  return TRUE;
}


/*
 * task_move_object_to()
 *
 * The destination half of task_move_object(): move the object where var2 and
 * var3 say, noting in stamp_seen whether the Runner would stamp it seen for
 * that destination.  was_possessed, weight and size are the object's before
 * the move, for the "into object" double subtract.  Returns FALSE if the move
 * was abandoned for want of a referenced character, in which case the caller
 * skips the rest of the mover.
 */
static scr_bool
task_move_object_to (scr_gameref_t game, scr_int object, scr_int var2,
                     scr_int var3, scr_bool was_possessed, scr_int weight,
                     scr_int size, scr_bool is_v400, scr_bool *stamp_seen)
{
  const scr_var_setref_t vars = gs_get_vars (game);

  *stamp_seen = FALSE;
  switch (var2)
    {
    case 0:                    /* To room */
      /* 0 is hidden; so is a room that does not exist (-1 always was). */
      if (var3 <= 0 || var3 > gs_room_count (game))
        {
          if (tasks.trace)
            scr_trace ("Task: moving object %ld to hidden\n", object);

          gs_object_make_hidden (game, object);
        }
      else
        {
          if (tasks.trace)
            {
              scr_trace ("Task: moving object %ld to room %ld\n",
                        object, var3 - 1);
            }

          /* var3 != 0 here (the var3 == 0 "hidden" case is handled above). */
          gs_object_to_room (game, object, var3 - 1);
          *stamp_seen = (var3 - 1 == gs_playerroom (game));
        }
      /*
       * Proc_19_10 writes &HFF to the container field on both paths
       * (48C431); see the runner_parent note in scgamest.cpp.
       */
      gs_set_object_runner_parent (game, object, -1);
      break;

    case 1:                    /* To roomgroup part */
      if (tasks.trace)
        {
          scr_trace ("Task: moving object %ld to random room in group %ld\n",
                    object, var3);
        }

      {
        scr_int dest = lib_random_roomgroup_member (game, var3);
        if (dest >= 0)           /* Empty group: leave the object in place. */
          gs_object_to_room (game, object, dest);
      }
      gs_set_object_runner_parent (game, object, -1);   /* 48C471 */
      break;

    case 2:                    /* Into object */
      if (tasks.trace)
        scr_trace ("Task: moving object %ld into %ld\n", object, var3);

      /*
       * Runner quirk, faithfully kept: the "into object" branch repeats the
       * possession-gated subtract (Proc_19_10 loc_48C48B) before it moves the
       * object, on top of the universal one above -- the object is still in
       * the player's possession when the gate re-runs, so a task that moves a
       * carried object into a container takes its size and weight off the
       * totals twice.  The "onto" branch has no such second subtract.
       */
      if (was_possessed)
        gs_carried_adjust (game, -weight, -size);
      gs_object_move_into (game, object, obj_container_object (game, var3));
      *stamp_seen = gs_object_seen (game, obj_container_object (game, var3));
      break;

    case 3:                    /* Onto object */
      if (tasks.trace)
        scr_trace ("Task: moving object %ld onto %ld\n", object, var3);

      gs_object_move_onto (game, object, obj_surface_object (game, var3));
      *stamp_seen = gs_object_seen (game, obj_surface_object (game, var3));
      break;

    case 4:                    /* Held by */
      if (tasks.trace)
        scr_trace ("Task: moving object %ld to held by %ld\n", object, var3);

      if (!task_move_object_held_by (game, object, var3, is_v400, stamp_seen))
        return FALSE;
      break;

    case 5:                    /* Worn by */
      if (tasks.trace)
        scr_trace ("Task: moving object %ld to worn by %ld\n", object, var3);

      if (var3 == 0)            /* Player */
        {
          gs_object_player_wear (game, object);
          gs_set_object_runner_parent (game, object, -1);   /* 48C745 */
          *stamp_seen = TRUE;
        }
      else                      /* Ref character or NPC id */
        {
          const scr_int npc = task_selector_npc (vars, var3);

          /*
           * Unset referenced character: abandoned, as in the "held by"
           * case above (Proc_19_10 loc_48C79D-48C7A9).
           */
          if (npc < 0)
            return FALSE;
          gs_object_npc_wear (game, object, npc);
          gs_set_object_runner_parent (game, object, npc);   /* 48C7C3/48C7F9 */
        }
      break;

    case 6:                    /* Same room as */
      {
        scr_int room, npc;

        if (tasks.trace)
          {
            scr_trace ("Task: moving object %ld to same room as %ld\n",
                      object, var3);
          }

        if (var3 == 0)          /* Player */
          room = gs_playerroom (game);
        else                    /* Ref character or NPC id */
          {
            npc = task_selector_npc (vars, var3);

            /*
             * Unset referenced character: abandoned, as in the "held by"
             * case above.
             */
            if (npc < 0)
              return FALSE;
            room = gs_npc_location (game, npc) - 1;
          }
        gs_object_to_room (game, object, room);
        gs_set_object_runner_parent (game, object, -1);   /* 48C815 */
        *stamp_seen = (room == gs_playerroom (game));
        break;
      }

    default:
      /*
       * Unset (combo index -1) or unknown object move destination; ignored,
       * as the Runner does, rather than treated as fatal.  See the matching
       * note in task_run_move_npc_action().
       */
      if (tasks.trace)
        scr_trace ("Task: ignoring move with unset/unknown"
                  " object move type %ld\n", var2);
      break;
    }

  return TRUE;
}


/*
 * task_move_destination_valid()
 *
 * FALSE if an object move's var3 names a container, surface or character
 * that does not exist; the move is then skipped.  A room past the end is
 * taken as hidden by task_move_object_to() instead, and an unset referenced
 * character is the movers' own business.
 */
static scr_bool
task_move_destination_valid (scr_gameref_t game, scr_int var2, scr_int var3)
{
  switch (var2)
    {
    case 2:                    /* Into object */
      return obj_container_object (game, var3) >= 0;
    case 3:                    /* Onto object */
      return obj_surface_object (game, var3) >= 0;
    case 4:                    /* Held by */
    case 5:                    /* Worn by */
    case 6:                    /* Same room as */
      return var3 <= 1 || gs_npc_valid (game, var3 - 2);
    default:
      return TRUE;
    }
}


/*
 * task_move_object()
 *
 * Move an object to a place.
 */
static void
task_move_object (scr_gameref_t game, scr_int object,
                  scr_int var2, scr_int var3)
{
  /*
   * Ignore negative object indexes, as evt_move_object() does.  Var1 = 2 is
   * "the referenced object", and a task reached by redirection (or matched by
   * a pattern with no %object%) has none, so var_get_ref_object() hands back
   * -1.  The Runner's Select Case simply moves nothing; SCARE used to walk
   * into gs_object_*() and abort on its range assertion.  Mortality's task
   * 314 [? the return] does exactly this.
   */
  if (object < 0)
    {
      if (tasks.trace)
        scr_trace ("Task: ignoring move of unset object %ld\n", object);
      return;
    }

  /*
   * Static objects do not move for a task action.  run400's object mover
   * (Sub_20_11 @0008C200) skips them at @0008C360 -- "If Objects(o).Static = 1
   * Then <next object>" -- before any destination case runs, so the refusal
   * covers every destination, not just the room ones.  Measured live
   * (test/adrift4/notes/RUNNER_TESTS_TODO.md section 9): a task whose action
   * moves the referenced object to "held by player" prints its completion text
   * for "grab plaque" and leaves the plaque exactly where it was, and a
   * move-to-hidden aimed at a static the player is already holding (an event
   * put it there) is refused in the same way.  Only the by-index selector is
   * limited to dynamics, so without this test the "referenced object" selector
   * would reach statics.
   *
   * The event mover keeps its own copy of this decision -- and does not make
   * it, which is why evt_move_object() is the one and only way a static can
   * reach the player's hands.
   */
  if (obj_is_static (game, object))
    {
      if (tasks.trace)
        scr_trace ("Task: ignoring move of static object %ld\n", object);
      return;
    }

  if (!task_move_destination_valid (game, var2, var3))
    {
      if (tasks.trace)
        scr_trace ("Task: ignoring move of object %ld to nonexistent"
                   " destination %ld, %ld\n", object, var2, var3);
      return;
    }

  /*
   * A move action spends the object's "only when not moved" byte, exactly as
   * the library take does.  run400's execute_action @0048C377-@0048C395 runs
   * "If o(132) = 1 Then o(132) = &HFF" on the selected object immediately
   * after the static refusal at @0048C371 and *before* the destination Select
   * Case, so every destination spends it -- including a move back to the room
   * the object started in, and including "to hidden".  Statics never reach it,
   * their branch having jumped to the end of the action.
   *
   * zelda is the row that settled it: the small key is InitialPosition hidden
   * with OnlyWhenNotMoved = 1 and a non-empty InRoomDesc, and the Like-Like
   * task moves it into the Graveyard.  run400 answers "Also here is a small
   * key." there, not the key's own description, because the move spent the
   * byte.  See obj_shows_initial_description() in scobjcts.cpp for the other
   * half of the model.
   */
  gs_set_object_unmoved (game, object, FALSE);

  /*
   * The Runner's task mover does its own carried-total accounting
   * (Proc_19_10 in run400): if the object is currently in the player's
   * possession (held or worn, recursing through carried containers and
   * surfaces -- Proc_21_46), its size and weight come off the totals
   * before the move; afterwards, a move to "held by player" adds size
   * and weight back, and a move to "worn by player" adds weight only
   * (worn things weigh but occupy no hand space, matching the loader's
   * seeding).  The generic position tracker is suspended for the move so
   * these rules replace, not compound, its take/drop model.
   *
   * The predicate is the Runner's own, not obj_indirectly_held_by_player():
   * Proc_21_46 walks the bare container field and never looks at openness,
   * so a task that lifts an object out of a CLOSED container the player is
   * holding still refunds it.  Provenance's well puzzle is the case that
   * settled it -- `close lid`, `lower bucket into well`, and the task
   * swaps the empty canteen inside the shut bucket for a full one.  With
   * the openness-aware predicate the empty canteen's 9/3 never came off,
   * and every `count` for the rest of the game read 9 size and 3 weight
   * high (measured against run400, 2026-08-23).
   */
  {
    const scr_bool was_possessed = gs_runner_possessed (game, object);
    const scr_int weight = obj_get_weight (game, object);
    const scr_int size = obj_get_size (game, object);
    const scr_int taf_version = prop_get_taf_version (gs_get_bundle (game));
    const scr_bool is_v400 = taf_version >= TAF_VERSION_400;
    scr_bool stamp_seen;

    /*
     * The suspend guard lifts the tracker's suspension on the way out,
     * an abandoned move included.
     */
    {
      gs_carried_suspend_guard suspend (game);

      if (was_possessed)
        gs_carried_adjust (game, -weight, -size);
      if (!task_move_object_to (game, object, var2, var3, was_possessed,
                                weight, size, is_v400, &stamp_seen))
        return;
    }

    /* Post-move credit: into the player's hands or onto their back. */
    if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
      gs_carried_adjust (game, weight, size);
    else if (gs_object_position (game, object) == OBJ_WORN_PLAYER)
      gs_carried_adjust (game, weight, 0);

    /*
     * The move-object action's seen stamp.  run400's executor (Proc_19_10)
     * stamps per destination, not by visibility: to a room only when it is
     * the player's (48C414); into or onto an object only when that object is
     * itself already seen (48C511, 48C582); held or worn by the player
     * always (48C5EF, 48C754); held by an NPC when Proc_21_53 -- this port's
     * obj_indirectly_in_room() -- finds it visible afterwards (48C67C,
     * 48C6EB), a named NPC also stamping a present object before the move
     * (48C69A); worn by an NPC never; same room as the player always, as an
     * NPC when that NPC's room is the player's (48C8A6, 48C90E); a roomgroup
     * never.  Alias Undercover Agent is the row: the lunch task puts the
     * plate onto the dinner tray before the player has examined the table,
     * so the plate stays unseen and `take plate` answers "Take what?".
     *
     * run390's execute_action (455C24-4573DA) stamps by destination the
     * same way -- room only when the player's (455E16), into/onto only when
     * the parent is seen (455F53, 455FCA), held or worn by the player always
     * (45602E, 45612E), same room as the player always (456232) and as a
     * character when that character's room is the player's (456287, 4562EE),
     * a roomgroup never -- except that an object handed to or worn by a
     * character is never stamped (456099-4560DA, 45618B-4561DA), where 4.0
     * stamps a held one it can see afterwards.  secret_of_lost_world T52
     * `put tooth in skull` (run390, runner_transcripts/secret_of_lost_world,
     * 2026-09-19) moves the blue gem onto the fresh skeleton before anything
     * has listed the skeleton, so the gem stays unseen, and the next turn's
     * `take blue gem` has one seen "gem" present -- the held green one --
     * for takes()'s co() to name: "You already have the green gem!".  With
     * the visibility stamp Scarier counted two and took the blue gem.
     * Earlier versions keep the visibility stamp, unread.
     */
    if (taf_version >= TAF_VERSION_390
        ? stamp_seen
        : obj_indirectly_in_room (game, object, gs_playerroom (game)))
      gs_set_object_seen (game, object, TRUE);
  }
}


/*
 * task_move_phantom_object()
 *
 * Move the 3.9/4.0 Runner's phantom object at the end of an "all held" move.
 * The Runner dimensions its object array 0 To count but loads only
 * 0..count-1, so the extra slot stays zeroed, and position 0 is "held by
 * the player".  The "all held" scan (run390 execute_action 455C58-455CA0,
 * run400 Proc_19_10 48C237-48C27F) tests a slot before its "reached the
 * count" exit, so it reaches slot [count] and, while that is still held,
 * moves it like any held object: nothing to weigh, not static, off to the
 * destination.  Nothing ever shows it, but a roomgroup destination draws
 * getaroom's Rnd (run390 455E5C).  Once moved anywhere but the player's
 * hands it is no longer held, so this happens at most once a game unless
 * something hands it back.  hhorror's GHOST MOVING STUFF (all held to
 * group 1) is the measured case: the one mid-game getaroom draw in the
 * seed-50 census, on the ghost's first visit with empty hands.
 *
 * The move's tail can also set the referenced-object name from the phantom
 * (run390 456302-456349) when it lands in the player's room; not ported.
 */
static void
task_move_phantom_object (scr_gameref_t game, scr_int var2, scr_int var3)
{
  const scr_var_setref_t vars = gs_get_vars (game);

  if (!gs_runner_phantom_held (game))
    return;

  switch (var2)
    {
    case 1:                    /* To roomgroup part */
      if (tasks.trace)
        scr_trace ("Task: moving phantom object to random room in group"
                   " %ld\n", var3);
      /* An empty group writes getaroom's 0, which is still "held". */
      if (lib_random_roomgroup_member (game, var3) < 0)
        return;
      break;

    case 4:                    /* Held by */
    case 5:                    /* Worn by */
    case 6:                    /* Same room as */
      if (var2 == 4 && var3 == 0)
        return;                 /* Handed straight back to the player. */
      if (var3 == 1 && var_get_ref_character (vars) < 0)
        return;                 /* No referenced character: abandoned. */
      break;

    case 0:                    /* To room (or hidden) */
    case 2:                    /* Into object */
    case 3:                    /* Onto object */
      break;

    default:
      return;
    }

  gs_set_runner_phantom_held (game, FALSE);
}


/*
 * task_run_move_object_action()
 *
 * Demultiplex an object move action and execute it.
 */
void
task_run_move_object_action (scr_gameref_t game,
                             scr_int var1, scr_int var2, scr_int var3)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int object;

  /* Select depending on value in var1. */
  switch (var1)
    {
    case 0:                    /* All held */
      GS_FOR_EACH_OBJECT (game, object)
        {
          if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
            task_move_object (game, object, var2, var3);
        }
      if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
        task_move_phantom_object (game, var2, var3);
      break;

    case 1:                    /* All worn */
      GS_FOR_EACH_OBJECT (game, object)
        {
          if (gs_object_position (game, object) == OBJ_WORN_PLAYER)
            task_move_object (game, object, var2, var3);
        }
      break;

    case 2:                    /* Ref object */
      object = var_get_ref_object (vars);
      task_move_object (game, object, var2, var3);
      break;

    default:                   /* Dynamic object */
      object = obj_dynamic_object (game, var1 - 3);
      task_move_object (game, object, var2, var3);
      break;
    }
}


/*
 * task_move_npc_to_room()
 *
 * Move an NPC to a given room.
 */
static void
task_move_npc_to_room (scr_gameref_t game, scr_int npc, scr_int room)
{
  if (tasks.trace)
    scr_trace ("Task: moving NPC %ld to room %ld\n", npc, room);

  /* Update the NPC's state; below -1 is no room, so hidden like -1. */
  if (room < 0)
    gs_set_npc_location (game, npc, 0);
  else if (room < gs_room_count (game))
    gs_set_npc_location (game, npc, room + 1);
  else
    {
      scr_int dest = lib_random_roomgroup_member (game,
                                                 room - gs_room_count (game));
      if (dest < 0)
        return;                  /* Empty group: leave the NPC in place. */
      gs_set_npc_location (game, npc, dest + 1);
    }

  gs_set_npc_parent (game, npc, -1);
  gs_set_npc_position (game, npc, 0);
}


/*
 * task_same_room_npc_390()
 *
 * The "to same room as" arm (Var2 = 2) at 3.90 names its NPC by raw array
 * index: run390 execute_action copies MemVar_468028(Var3).room for the NPC
 * arm (4567C9-4567EB) and the player arm (456514-456531) alike, with no
 * player or referenced-character entries ahead of the NPCs.  run400 decodes
 * the same field as 0 = player, 1 = referenced, N = NPC N-2 (48CE62-48CED7).
 * 3.7/3.8 have no such action (their movements convert to player and object
 * moves only).  fantasyworld task 92 moves the Royal Knight to Var3 = 27,
 * King Harmon, whom the action before it has just sent to Base of Mountain;
 * read as N-2 it named the Barmaid.  Returns the NPC, or -1 when Var3 names
 * none (the Runner would raise a subscript error).
 */
static scr_int
task_same_room_npc_390 (scr_gameref_t game, scr_int var3)
{
  return (var3 >= 0 && var3 < gs_npc_count (game)) ? var3 : -1;
}


/*
 * task_move_player_action()
 * task_move_npc_action()
 * task_run_move_npc_action()
 *
 * Move player or NPC.  The first two are the Var1 = 0 (player) arm and the
 * arm for an NPC already picked out by Var1; each decides where to move its
 * character to from Var2 and Var3.
 */
static void
task_move_player_action (scr_gameref_t game, scr_int var2, scr_int var3)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_bool is_v390 = prop_get_taf_version (gs_get_bundle (game))
                           < TAF_VERSION_400;
  scr_int npc, room;

  /* Decide where to move the player to. */
  switch (var2)
    {
    case 0:                /* To room */
      if (var3 < 0)
        return;
      /* var3 past the rooms is a group; mark the room actually reached. */
      gs_move_player_to_room (game, var3);
      obj_mark_room_statics_seen (game, gs_playerroom (game));
      return;

    case 1:                /* To roomgroup part */
      if (tasks.trace)
        {
          scr_trace ("Task: moving player to random room in group %ld\n",
                    var3);
        }

      {
        scr_int dest = lib_random_roomgroup_member (game, var3);
        if (dest >= 0)       /* Empty group: leave the player in place. */
          {
            gs_move_player_to_room (game, dest);
            obj_mark_room_statics_seen (game, dest);
          }
      }
      return;

    case 2:                /* To same room as... */
      if (is_v390)
        npc = task_same_room_npc_390 (game, var3);
      else if (var3 == 0)  /* ...player! */
        return;
      else                 /* ...referenced or specified NPC */
        npc = task_selector_npc (vars, var3);
      if (!gs_npc_valid (game, npc))
        return;

      if (tasks.trace)
        scr_trace ("Task: moving player to same room as NPC %ld\n", npc);

      room = gs_npc_location (game, npc) - 1;
      if (room < 0)
        {
          if (tasks.trace)
            scr_trace ("Task: silently suppressed player move to hidden\n");
        }
      else
        {
          gs_move_player_to_room (game, room);
          obj_mark_room_statics_seen (game, room);
        }
      return;

    case 3:                /* To standing on */
    case 4:                /* To sitting on */
    case 5:                /* To lying on */
      {
        /*
         * var2 3/4/5 map to positions 0/1/2; the parent is filtered by
         * what the object can actually be on, so an object that is not
         * standable (or not lieable) leaves the player on the floor in
         * that posture.  Traced because a silent posture change is
         * otherwise invisible: goldilocks' collapsing chair task sets
         * "sitting on" a chair it destroys in the same breath, and the
         * only sign of it is a "(Standing up first)" on the NEXT move.
         */
        const scr_int position = var2 - 3;
        const scr_int parent = (var2 == 5
                                ? obj_lieable_object (game, var3 - 1)
                                : obj_standable_object (game, var3 - 1));

        if (tasks.trace)
          {
            scr_trace ("Task: player position %ld, parent object %ld"
                       " (requested %ld)\n", position, parent, var3 - 1);
          }

        gs_set_playerposition (game, position);
        gs_set_playerparent (game, parent);
        return;
      }

    default:
      /*
       * ADRIFT stores a move destination as a combo-box index, and a value
       * of -1 (and any other out-of-range value) means the game's author
       * left the destination unset.  The Runner's Select Case silently
       * ignores such an action, so we treat it as a no-op rather than
       * fatal.  With move assist on, honour an unset (-1) move whose Var3
       * names a real room as "to room" (see task_set_move_assist()).
       */
      if (tasks.options.move_assist && var2 == -1
          && var3 >= 0 && var3 < gs_room_count (game))
        {
          gs_move_player_to_room (game, var3);
          obj_mark_room_statics_seen (game, var3);
          return;
        }
      if (tasks.trace)
        scr_trace ("Task: ignoring move with unset/unknown"
                  " player move type %ld\n", var2);
      return;
    }
}

static void
task_move_npc_action (scr_gameref_t game,
                      scr_int npc, scr_int var2, scr_int var3)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_bool is_v390 = prop_get_taf_version (gs_get_bundle (game))
                           < TAF_VERSION_400;
  scr_int room, ref_npc = -1;

  /* Decide where to move the NPC to. */
  switch (var2)
    {
    case 0:                /* To room */
      task_move_npc_to_room (game, npc, var3 - 1);
      return;

    case 1:                /* To roomgroup part */
      if (tasks.trace)
        {
          scr_trace ("Task: moving NPC %ld to random room in group %ld\n",
                    npc, var3);
        }

      {
        scr_int dest = lib_random_roomgroup_member (game, var3);
        if (dest >= 0)       /* Empty group: leave the NPC in place. */
          task_move_npc_to_room (game, npc, dest);
      }
      return;

    case 2:                /* To same room as... */
      if (is_v390)
        {
          ref_npc = task_same_room_npc_390 (game, var3);
          if (ref_npc < 0)
            return;
          if (tasks.trace)
            {
              scr_trace ("Task: moving NPC %ld to"
                        " same room as NPC %ld (3.90 raw index)\n",
                        npc, ref_npc);
            }

          room = gs_npc_location (game, ref_npc) - 1;
          task_move_npc_to_room (game, npc, room);
          return;
        }
      switch (var3)
        {
        case 0:            /* ...player */
          if (tasks.trace)
            {
              scr_trace ("Task: moving NPC %ld to same room as player\n",
                        npc);
            }

          task_move_npc_to_room (game, npc, gs_playerroom (game));
          break;
        default:           /* ...referenced or specified NPC */
          /*
           * With no referenced character, run400 indexes its NPC array
           * with the unset marker (48CEA9), and execute_action's error
           * handler prints "** ERROR - Subscript out of range! **" and
           * abandons this one action; the task's other actions still
           * run (measured, p4MVREF probe).  Deliberate deviation: the
           * action is dropped silently, without the Runner's error text.
           */
          ref_npc = task_selector_npc (vars, var3);
          if (!gs_npc_valid (game, ref_npc))
            return;
          if (tasks.trace)
            {
              scr_trace ("Task: moving NPC %ld to same room as %sNPC %ld\n",
                        npc, var3 == 1 ? "referenced " : "", ref_npc);
            }

          room = gs_npc_location (game, ref_npc) - 1;
          task_move_npc_to_room (game, npc, room);
          break;
        }
      return;

    case 3:                /* To standing on */
      gs_set_npc_position (game, npc, 0);
      gs_set_npc_parent (game, npc, obj_standable_object (game, var3));
      return;

    case 4:                /* To sitting on */
      gs_set_npc_position (game, npc, 1);
      gs_set_npc_parent (game, npc, obj_standable_object (game, var3));
      return;

    case 5:                /* To lying on */
      gs_set_npc_position (game, npc, 2);
      gs_set_npc_parent (game, npc, obj_lieable_object (game, var3));
      return;

    default:
      /*
       * Unset/unknown NPC move destination; ignored, as the Runner does.
       * With move assist on, honour an unset (-1) move whose Var3 names a
       * real room as "to room" (1-based, as for case 0).  A positive Var3
       * that names no room is a dangling index (the editor stored it
       * against a differently-based or since-edited list; the offset even
       * differs between corpus games, so it cannot be decoded reliably);
       * every corpus instance of one -- X-Files' diner buzzer, HYPER
       * Battle System's flare rat -- is a "bring this character on-stage
       * here" direction, so summon the NPC to the player's room instead.
       * A Var3 of 0 or less means nothing was ever selected; that stays
       * a no-op.
       */
      if (tasks.options.move_assist && var2 == -1)
        {
          if (var3 - 1 >= 0 && var3 - 1 < gs_room_count (game))
            {
              task_move_npc_to_room (game, npc, var3 - 1);
              return;
            }
          if (var3 > 0)
            {
              task_move_npc_to_room (game, npc, gs_playerroom (game));
              return;
            }
        }
      if (tasks.trace)
        scr_trace ("Task: ignoring move with unset/unknown"
                  " NPC move type %ld\n", var2);
      return;
    }
}

void
task_run_move_npc_action (scr_gameref_t game,
                          scr_int var1, scr_int var2, scr_int var3)
{
  scr_int npc;

  /* Player or NPC? */
  if (var1 == 0)
    {
      task_move_player_action (game, var2, var3);
      return;
    }

  /*
   * NPC -- first find which NPC to move about.  With no referenced
   * character, run400 leaves the action at once (48CDB8, Exit Sub).
   */
  npc = task_selector_npc (gs_get_vars (game), var1);
  if (!gs_npc_valid (game, npc))
    return;

  task_move_npc_action (game, npc, var2, var3);
}
