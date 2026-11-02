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
 * Movement: compass directions and "go to" a named place.
 *
 * See sclibrar.h for what the library files share.
 */

#include "sclibrar.h"


/*
 * Direction enumeration.  Used by movement commands, to multiplex them all
 * into a single function.  The values are explicit to ensure they match
 * enumerations in the game data.
 */
enum
{ DIR_NORTH = 0, DIR_EAST = 1, DIR_SOUTH = 2, DIR_WEST = 3,
  DIR_UP = 4, DIR_DOWN = 5, DIR_IN = 6, DIR_OUT = 7,
  DIR_NORTHEAST = 8, DIR_SOUTHEAST = 9, DIR_SOUTHWEST = 10, DIR_NORTHWEST = 11
};


/*
 * lib_set_movement_probe()
 *
 * Put lib_go() into probe mode, in which it prints nothing, moves nobody,
 * and returns TRUE only if the movement it was handed would really have
 * taken the player out of the room.  Used by the version 3.8 movement
 * pre-pass in run_all_commands(); see the commentary there.
 */
static scr_bool lib_movement_probe = FALSE;

void
lib_set_movement_probe (scr_bool probe)
{
  lib_movement_probe = probe;
}


/*
 * lib_go()
 *
 * Central movement command, called by all movement handlers.
 */
static scr_bool
lib_go (scr_gameref_t game, scr_int direction)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_bool is_trapped, is_exitable[12];
  scr_int destination, index_, stale_parent;
  const scr_char *const *dirnames;

  /* Decide on four or eight point compass names list. */
  dirnames = lib_compass_names (game);

  /* Start by seeing if there are any exits at all available. */
  is_trapped = TRUE;
  for (index_ = 0; dirnames[index_]; index_++)
    {
      is_exitable[index_] = lib_room_exit_available (game, gs_playerroom (game),
                                                     index_);
      if (is_exitable[index_])
        is_trapped = FALSE;
    }
  if (is_trapped)
    {
      if (lib_movement_probe)
        return FALSE;

      pf_buffer_string (filter,
                        lib_select_response (game,
                                      "You can't go in any direction!\n",
                                      "I can't go in any direction!\n",
                                      "%player% can't go in any direction!\n"));
      return TRUE;
    }

  /*
   * Check for the exit, and if it doesn't exist, refuse, and list the possible
   * options.
   */
  /*
   * A blocked exit is refused exactly like a missing one.  The Runner's
   * movement refusal (run400 Proc_19_29_475638) knows nothing about why a
   * direction failed: it recounts the exits with Proc_19_28_454684 -- the
   * same restriction-aware test as is_exitable[] above, reading the exit's
   * door state and task gate at 45459A-45463C -- and prints " can only
   * move X." or " can't go in that direction, but ... can move ..." from
   * that count.  No Runner from 3.7 to 4.0 carries an "(at present)"
   * string at all; Scarier's old "can't go in that direction (at present)"
   * for an exit that exists but is currently shut was an invention.
   * Measured on humbug (4.00, runner_probes/humbug.run400.b.txt): `W` into the
   * keypad door, an exit gated on a task, answers "I can't go in that
   * direction, but I can move north, east and south."
   */
  if (!lib_room_exit_destination (game, direction, &destination)
      || !lib_can_go (game, gs_playerroom (game), direction))
    {
      lib_list_t list;

      if (lib_movement_probe)
        return FALSE;

      /* List available exits, found in exit test loop earlier. */
      for (index_ = 0; dirnames[index_]; index_++)
        {
          if (is_exitable[index_])
            list.push_back (index_);
        }

      /*
       * With exactly one usable exit the Runner prints just " can only
       * move X.", with no "can't go in that direction" prefix; the prefix
       * exists only in the several-exits branch (run400 @00474A75 vs
       * @00474AFB in Proc_19_29_475638).
       */
      if (list.size () == 1)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You can only move ",
                                                 "I can only move ",
                                                 "%player% can only move "));
        }
      else
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                             "You can't go in that direction, but you can move ",
                             "I can't go in that direction, but I can move ",
                             "%player% can't go in that direction, but "
                             "%player_pronoun% can move "));
        }
      lib_print_name_list (game, list, dirnames, " and ");
      pf_buffer_string (filter, ".\n");
      return TRUE;
    }

  /* The move would go through; that is all a probe wants to know. */
  if (lib_movement_probe)
    return TRUE;

  if (lib_trace)
    {
      scr_trace ("Library: moving player from %ld to %ld\n",
                gs_playerroom (game), destination);
    }

  /*
   * Indicate if getting off something or standing up first.  Both lines are
   * bracketed *references*: 3.7 and 3.8 print them unconditionally (run370
   * loc_42303C / loc_423078, run380 loc_428244 / loc_428280), and 3.9 AND
   * 4.0 print them behind Options -> Display & Media... -> Appearance ->
   * "References in brackets" (run400 loc_450339 / loc_4503BF test
   * MemVar_4942BA, saved as "showbrackets" @4679A1; run390 moveroom
   * 431A4C tests m_showbrackets.Checked at loc_431911 for "(Getting off "
   * and loc_4319A9 for "(Standing up first)").  An earlier census read
   * run390 as having no "Getting off" literal at all and gated this
   * `< 3.90 || >= 4.00`; the literal lives in run390_3.bas:9909, and the
   * wingman1.taf (3.90) replay of 2026-08-30 (runner_transcripts/wingman1.txt,
   * brackets ON) prints "(Getting off the Barstool first)" before "You
   * move in."
   *
   * Scarier models the Runner with that box ticked -- the reference
   * setting the transcripts are measured under -- so every version prints
   * the lines.  Measured on monsters (4.00) commands 5 and 23, where
   * run400 with brackets on answers "in" from the bed with "(Getting off
   * Sissy's four poster bed first)" on its own line before "I move in."
   * (and later "(Getting off the pink plastic chair first)"); with the box
   * unticked (humbug command 254, 2026-08-24) it prints nothing.
   *
   * The parent-less half -- sitting or lying on the FLOOR, so "(Standing up
   * first)" rather than "(Getting off X first)" -- was measured 2026-09-07 on
   * Main Course.taf (4.00, runner_probes/maincourse.run400.txt), whose player
   * starts sitting with ParentObject 0, and on goldilocks
   * (runner_transcripts/goldilocks.txt) turn 94, where a task
   * action seats the player on an unset object.  Both print the line with the
   * box ticked and nothing without it, and both had earlier brackets-OFF
   * transcripts that read as an engine bug until they were re-driven.
   *
   * From 3.9 the name goes through the object-name composer in mode 0
   * (run390 431943 -> compose_object_name 42B0E8, run400 450354 -> 448710),
   * which answers "that" for an object the player has not seen.  gateway
   * (3.90) seats the player on a chair only a task's text mentions, and
   * run390x answers `east` with "(Getting off that first)"
   * (runner_transcripts/gateway.txt, 2026-09-14).  run370/380 concatenate the
   * name directly, with no seen test.  Scarier deliberately names the object
   * in every version (deviation policy): the player is sitting on it, so
   * hiding its name behind "that" only loses information.
   *
   * Before 3.9 moveroom looks only at the position (run370 422FD0, run380
   * the same): a player standing on an object walks off it with no line, and
   * the parent object survives the move -- only the sit/lie branch clears
   * it.  So `stand on crate`, `s`, `sit`, `stand` is "You move south.", ...,
   * "You stand up from the crate." (p37SIT/p38SIT, run370x
   * runner_probes/sit.run370.sit2.rtf, run380x
   * runner_probes/sit.run380.sit2.rtf, 2026-09-19).  run380's take-from reach
   * test reads the same stale parent (446BA5).
   */
  stale_parent = -1;
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390
      && gs_playerposition (game) == 0)
    stale_parent = gs_playerparent (game);
  else if (gs_playerparent (game) != -1)
    {
      pf_buffer_string (filter, "(Getting off ");
      lib_print_object_np (game, gs_playerparent (game));
      pf_buffer_string (filter, " first)\n");
    }
  else if (gs_playerposition (game) != 0)
    pf_buffer_string (filter, "(Standing up first)\n");

  /* Confirm and then make move. */
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You move ",
                                         "I move ",
                                         "%player% moves "));
  pf_buffer_string (filter, dirnames[direction]);
  pf_buffer_string (filter, ".\n");

  gs_move_player_to_room (game, destination);
  gs_set_playerparent (game, stale_parent);
  game->player_moved_by_command = TRUE;

  /* Describe the new room and return. */
  lib_describe_player_room (game, FALSE);
  return TRUE;
}


/*
 * lib_cmd_go_*()
 *
 * Direction-specific movement commands.
 *
 * `in` also answers to `inside` and `enter`, and `out` to `outside` and
 * `exit`; the other ten directions have no such alternates.  Every Runner
 * tests the three- and four-way alternations right where it tests `in` and
 * `out` themselves, and by equality against the whole command rather than
 * with c(): run370 loc_434AEA / loc_434C02, run380 loc_43B44D / loc_43B556,
 * run390 loc_44FDF6 / loc_44FF01, run400 loc_474FEF / loc_4750A4.
 *
 * `exit` used to sit in the `exits`/`where`/`directions` row below, which is
 * only right for a room with no out exit.  A room that has one is left by
 * `exit`, and where it has none the wording still differs from a real exits
 * request: the Runner seeds the response of all twenty movement words with
 * the exits summary before the direction blocks get their chance to overwrite
 * it (run380 loc_43AA58), and the several-exits form of that seed is prefixed
 * " can't go in that direction, but" for every word except `exits`, `where`
 * and `directions` (loc_43AC7E).  That is exactly lib_go()'s own refusal, so
 * routing `exit` to lib_cmd_go_out() gets both cases right at once.
 *
 * `inside` and `outside` are absent from that twenty-word seed list, so in a
 * room without the matching exit the Runner has nothing to say and drops
 * through to the catch-all.  We print the exits summary there instead, which
 * is what lib_go() does for every other direction word; the deviation is
 * confined to the case where the movement fails.
 */
scr_bool
lib_cmd_go_north (scr_gameref_t game)
{
  return lib_go (game, DIR_NORTH);
}

scr_bool
lib_cmd_go_east (scr_gameref_t game)
{
  return lib_go (game, DIR_EAST);
}

scr_bool
lib_cmd_go_south (scr_gameref_t game)
{
  return lib_go (game, DIR_SOUTH);
}

scr_bool
lib_cmd_go_west (scr_gameref_t game)
{
  return lib_go (game, DIR_WEST);
}

scr_bool
lib_cmd_go_up (scr_gameref_t game)
{
  return lib_go (game, DIR_UP);
}

scr_bool
lib_cmd_go_down (scr_gameref_t game)
{
  return lib_go (game, DIR_DOWN);
}

scr_bool
lib_cmd_go_in (scr_gameref_t game)
{
  return lib_go (game, DIR_IN);
}

scr_bool
lib_cmd_go_out (scr_gameref_t game)
{
  return lib_go (game, DIR_OUT);
}

/*
 * lib_cmd_just_a_direction()
 *
 * Every Runner ends generaltasks' verb sweep with a pair of branches that
 * answer anything still containing the whole word `go` or `enter` -- run370
 * loc_43DD8B / loc_43DDB4, run380 loc_44481C / loc_444845, run390 loc_45DF66
 * / loc_45DF83, run400 loc_489377 / loc_48938E.  Neither is guarded on the
 * response line being empty, so they overwrite whatever an earlier branch
 * had to say; `enter mansion` gets this and not the generic
 * unknown-verb-with-object reply.  A bare `enter`, `in`, `out` and the rest
 * never reach it -- those are movement words, handled above.
 */
scr_bool
lib_cmd_just_a_direction (scr_gameref_t game)
{
  return lib_print_message (game, "Just a direction will do.\n");
}


scr_bool
lib_cmd_go_northeast (scr_gameref_t game)
{
  return lib_go (game, DIR_NORTHEAST);
}

scr_bool
lib_cmd_go_southeast (scr_gameref_t game)
{
  return lib_go (game, DIR_SOUTHEAST);
}

scr_bool
lib_cmd_go_northwest (scr_gameref_t game)
{
  return lib_go (game, DIR_NORTHWEST);
}

scr_bool
lib_cmd_go_southwest (scr_gameref_t game)
{
  return lib_go (game, DIR_SOUTHWEST);
}


/*
 * lib_goto_reachable()
 *
 * The Runner's route finder (run390 Proc_2_8_43EF20, run380 Proc_2_8_434C50,
 * run400 Proc_21_9_465C14): a breadth-first search from the player's room
 * over directions 0 to 7, or 11 with an eight point compass.  An exit is
 * usable when its restriction is zero or task (restriction - 1) has the done
 * state 1 - Var2 -- the restriction type is never read, so an object state
 * restriction is tested as a task too -- and its destination is a room.  4.0
 * routes only through rooms the player has visited (the room's global_84).
 *
 * Returns TRUE if target can be reached, and the room after room in the path
 * in path[room]; the start counts as reachable from itself.
 */
static scr_bool
lib_goto_reachable (scr_gameref_t game, scr_int target,
                    std::vector<scr_int> &path)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_bool is_400 = prop_get_taf_version (bundle) >= TAF_VERSION_400;
  const scr_int rooms = gs_room_count (game);
  const scr_int directions = lib_compass_names (game) == DIRNAMES_8 ? 12 : 8;
  const scr_int start = gs_playerroom (game);
  std::vector<scr_int> parent (rooms, -1), queue;
  std::vector<char> seen (rooms, 0);
  size_t head;

  seen[start] = 1;
  queue.push_back (start);
  for (head = 0; head < queue.size (); head++)
    {
      const scr_int room = queue[head];
      scr_int direction;

      for (direction = 0; direction < directions; direction++)
        {
          scr_vartype_t vt_key[5], vt_rvalue;
          scr_int destination, restriction;

          vt_key[0].string = "Rooms";
          vt_key[1].integer = room;
          vt_key[2].string = "Exits";
          vt_key[3].integer = direction;
          if (!prop_get (bundle, "I<-sisi", &vt_rvalue, vt_key))
            continue;

          vt_key[4].string = "Var1";
          restriction = prop_get_integer (bundle, "I<-sisis", vt_key);
          if (restriction > 0)
            {
              scr_int check;

              vt_key[4].string = "Var2";
              check = prop_get_integer (bundle, "I<-sisis", vt_key);
              if (restriction > gs_task_count (game)
                  || (gs_task_done (game, restriction - 1) ? 1 : 0)
                     != 1 - check)
                continue;
            }

          vt_key[4].string = "Dest";
          destination = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
          if (destination < 0 || destination >= rooms || seen[destination])
            continue;
          if (is_400 && !gs_room_seen (game, destination))
            continue;

          seen[destination] = 1;
          parent[destination] = room;
          queue.push_back (destination);
        }
    }

  if (!seen[target])
    return FALSE;

  path.assign (rooms, -1);
  for (scr_int room = target; room != start; room = parent[room])
    path[parent[room]] = room;
  return TRUE;
}


/*
 * lib_goto_step_name()
 *
 * The direction the Runner types for one step of a walk: the LAST direction
 * out of room whose destination is next, with no restriction test, spelled
 * the way its route finder writes it into the input box.
 */
static const scr_char *
lib_goto_step_name (scr_gameref_t game, scr_int room, scr_int next)
{
  static const scr_char *const STEP_NAMES[] = {
    "North", "East", "South", "West", "Up", "Down", "In", "Out",
    "NorthEast", "SouthEast", "SouthWest", "NorthWest"
  };
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int directions = lib_compass_names (game) == DIRNAMES_8 ? 12 : 8;
  const scr_char *name = nullptr;
  scr_int direction;

  for (direction = 0; direction < directions; direction++)
    {
      scr_vartype_t vt_key[5], vt_rvalue;

      vt_key[0].string = "Rooms";
      vt_key[1].integer = room;
      vt_key[2].string = "Exits";
      vt_key[3].integer = direction;
      vt_key[4].string = "Dest";
      if (prop_get (bundle, "I<-sisis", &vt_rvalue, vt_key)
          && vt_rvalue.integer - 1 == next)
        name = STEP_NAMES[direction];
    }
  return name;
}


/*
 * lib_cmd_go_place()
 *
 * The Runner's gotoplace() (run370 42BD50 area, run380 432054, run390
 * 43C7B0, run400 464998), called from generaltasks after the tasks and the
 * meta commands and before the room refusal and therest(), so it outranks
 * the "Just a direction will do." nudge and every therest verb.
 *
 * It takes a line holding the word "goto", or one starting "go " (3.9 and
 * 4.0) or "go to" (3.7, 3.8: a bare prefix test, so `go tower` enters and
 * asks for a place called "go tower").  "goto", "go to" and "go" alone
 * leave.  It then cuts "goto ", "go to " and (3.9+) "go " from the front, in
 * that order, each wherever c() finds the word.  Deliberate deviation:
 * Scarier takes and cuts "go " at 3.7/3.8 too, see lib_goto_line_enters().
 * It matches what is left
 * against the lower-cased room names: exactly first, and when that does not
 * give exactly one reachable room, as a substring.  4.0 counts only rooms
 * the player has visited.
 *
 * One room walks there: "Moving to <room>..." and then each direction of
 * the route typed into the input box as a line of its own (SendKeys), then
 * "Arrived <room>.".  The goto line itself is not a turn: gotoplace sets it
 * to "&&&", which jumps past the characters/events tick.  Measured on
 * p39GOTO / p38GOTO / p4GOTO (harness/make_39_gotoprobe.py,
 * make_38_gotoprobe.py, make_400_gotoprobe.py), run390x
 * runner_probes/goto.run390.txt, run380x runner_probes/goto.run380.rtf and
 * run400x runner_probes/goto.run400.txt.
 */
/*
 * lib_go_place_off is set by run_line_t::library() for the pass that answers
 * the rest of a goto line as though gotoplace were not there.
 */
scr_bool lib_go_place_off = FALSE;


/*
 * lib_command_slot_370()
 *
 * The game's own word in 3.70 command slot SLOT (MemVar_4460FC(SLOT)),
 * lower-cased; empty from 3.8 on, which has no command block.
 */
std::string
lib_command_slot_370 (scr_prop_setref_t bundle, scr_int slot)
{
  std::string alias;

  if (prop_get_taf_version (bundle) < TAF_VERSION_380)
    {
      scr_vartype_t vt_key[3], vt_rvalue;

      vt_key[0].string = "Commands";
      vt_key[1].integer = slot;
      vt_key[2].string = "Word";
      if (prop_get (bundle, "S<-sis", &vt_rvalue, vt_key)
          && vt_rvalue.string && vt_rvalue.string[0] != NUL)
        {
          alias = vt_rvalue.string;
          for (char &c : alias)
            c = scr_tolower (c);
        }
    }
  return alias;
}

/*
 * lib_goto_alias()
 *
 * run370 also takes the game's own word for "goto" (command slot 15,
 * MemVar_4460FC(&HF)) anywhere in the line, leaves on it alone, and cuts
 * it as its length plus one from the front before the "goto" and "go to"
 * cuts (42B8E9-42BA4F).  `a rove hall` is "Moving to blue hall..." ("e
 * hall"), `rove kitchen` walks, bare `rove` is DontUnderstand and `goto
 * kitchen` still walks: p37GOTOW (harness/make_37_gotoprobe.py), run370x
 * runner_probes/gotow.run370.rtf and runner_probes/gotow.run370.2.rtf.
 * Lower-cased; empty from 3.8 on.
 */
static std::string
lib_goto_alias (scr_prop_setref_t bundle)
{
  return lib_command_slot_370 (bundle, 15);
}


/*
 * lib_goto_line_enters()
 *
 * Whether gotoplace goes past its entry tests on this line: from 3.9 c("goto")
 * or a line starting "go ", below it `(c("goto") And line<>"goto")`, a line
 * starting "go to" longer than five, or 3.7's own goto word; the bare words
 * leave at once.
 *
 * Deliberate deviation: a 3.7/3.8 line starting "go " enters too, as from
 * 3.9.  The 3.7 and 3.8 Runners guard gotoplace on the whole word `goto`
 * or a "go to" prefix only (run370 loc_42B994, run380 loc_431B8D), so `go
 * bedroom` got generaltasks' "Just a direction will do." there, and `go
 * tower` asked for a place called "go tower".  Tasks are matched first, so
 * a game's own `go X` task still wins.
 */
scr_bool
lib_goto_line_enters (scr_gameref_t game, const scr_char *input)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const std::string alias = lib_goto_alias (gs_get_bundle (game));
  const std::string line = input ? input : "";

  const auto has_word = [&] (const scr_char *word) -> scr_bool
    {
      return run_c_word (version, line.c_str (), word);
    };

  if (version >= TAF_VERSION_390)
    {
      if (!has_word ("goto") && line.compare (0, 3, "go ") != 0)
        return FALSE;
      return !(line == "goto" || line == "go to" || line == "go");
    }
  if (!(has_word ("goto") && line != "goto")
      && !(line.compare (0, 5, "go to") == 0 && line.size () > 5)
      && line.compare (0, 3, "go ") != 0
      && !(!alias.empty () && has_word (alias.c_str ())))
    return FALSE;
  return !(line == "goto" || line == "go to" || line == "go"
           || (!alias.empty () && line == alias));
}


/*
 * lib_goto_line_names_object()
 *
 * Whether the line names, whole word, the Short or an Alias of an object
 * the player has or can see -- what a take, drop or openclose needs before
 * it answers a goto line for itself; see run_line_t::library().
 */
scr_bool
lib_goto_line_names_object (scr_gameref_t game, const scr_char *line)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int room = gs_playerroom (game);
  scr_int object;

  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *shortname;
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;
      scr_bool named;

      if (!obj_indirectly_in_room (game, object, room)
          && !obj_indirectly_held_by_player (game, object))
        continue;
      shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
      named = shortname && lib_co_contains (line, shortname);
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; alias < alias_count && !named; alias++)
        {
          const scr_char *alias_name;

          vt_key[3].integer = alias;
          alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
          named = alias_name && alias_name[0] != NUL
                  && lib_co_contains (line, alias_name);
        }
      if (named)
        return TRUE;
    }
  return FALSE;
}


/*
 * lib_goto_line_examines()
 *
 * Whether a pre-4.0 examines() takes the line: one of its entry words
 * anywhere, whole word; see lib_cmd_go_place().
 */
static scr_bool
lib_goto_line_examines (scr_int version, const std::string &line)
{
  static const scr_char *const EXAMINE_WORDS[] = {
    "x", "examine", "look at", "ex", "exam", "read", nullptr
  };

  for (const scr_char *const *word = EXAMINE_WORDS; *word; word++)
    if (run_c_word (version, line.c_str (), *word))
      return TRUE;
  return (version >= TAF_VERSION_380
          && run_c_word (version, line.c_str (), "look in"))
         || (version >= TAF_VERSION_390
             && (run_c_word (version, line.c_str (), "look")
                 || run_c_word (version, line.c_str (), "l")));
}


/*
 * lib_goto_room_names()
 *
 * The rooms' names, lower-cased, as the Runner compares them.
 */
static std::vector<std::string>
lib_goto_room_names (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int rooms = gs_room_count (game);
  std::vector<std::string> names (rooms);
  scr_int room;

  for (room = 0; room < rooms; room++)
    {
      const scr_char *name = prop_get_indexed_string (bundle, "Rooms", room,
                                                      "Short");
      names[room] = name ? name : "";
      for (char &c : names[room])
        c = scr_tolower (c);
    }
  return names;
}


/*
 * lib_goto_print_which()
 *
 * The "Which ...?" question listing the count marked rooms by their
 * authored Short.
 */
static void
lib_goto_print_which (scr_gameref_t game, const std::string &text,
                      const std::vector<char> &marked, scr_int count)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int rooms = gs_room_count (game);
  scr_int room;

  pf_buffer_string (filter, "Which \"");
  pf_buffer_string (filter, text.c_str ());
  pf_buffer_string (filter, "\"?\n");
  for (room = 0; room < rooms; room++)
    {
      if (!marked[room])
        continue;
      pf_buffer_character (filter, '\'');
      pf_buffer_string (filter, prop_get_indexed_string (bundle, "Rooms",
                                                         room, "Short"));
      pf_buffer_character (filter, '\'');
      count--;
      pf_buffer_string (filter, count > 0 ? ", " : ".");
      if (count == 1)
        pf_buffer_string (filter, "or ");
    }
  pf_buffer_answer_break (filter);
}


/*
 * lib_goto_walk()
 *
 * Walk to target.  The route is fixed now and typed blindly: each step is
 * the last direction leading to the next room, whatever happens on the way.
 */
static void
lib_goto_walk (scr_gameref_t game, scr_int target, const std::string &name)
{
  const scr_filterref_t filter = gs_get_filter (game);
  std::vector<scr_int> path;
  scr_int room;

  lib_goto_reachable (game, target, path);
  for (room = gs_playerroom (game); room != target; room = path[room])
    {
      const scr_char *step = lib_goto_step_name (game, room, path[room]);

      if (step)
        run_queue_goto_step (step);
    }
  run_set_goto_arrival (("Arrived " + name + ".\n").c_str ());
  uip_renote_named_npcs (game, "", TRUE);

  pf_buffer_string (filter, "Moving to ");
  pf_buffer_string (filter, name.c_str ());
  pf_buffer_string (filter, "...\n");
  game->is_admin = TRUE;
}


scr_bool
lib_cmd_go_place (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = prop_get_taf_version (bundle);
  const scr_char *const input = run_get_dispatch_input ();
  const scr_int rooms = gs_room_count (game);
  std::vector<scr_int> path;
  std::vector<char> marked (rooms, 0);
  std::string line, text;
  scr_int count, target, room;
  scr_bool named_elsewhere;

  if (!input || lib_go_place_off)
    return FALSE;
  line = input;

  const auto has_word = [&] (const scr_char *word) -> scr_bool
    {
      return run_c_word (version, line.c_str (), word);
    };
  const auto drop_front = [&] (size_t length)
    {
      line = (line.size () > length) ? line.substr (length) : std::string ();
    };

  const std::string alias = lib_goto_alias (bundle);
  const scr_bool has_alias = !alias.empty () && has_word (alias.c_str ());

  if (!lib_goto_line_enters (game, line.c_str ()))
    return FALSE;

  /*
   * Pre-4.0 generaltasks calls examines() first (run390 45F684, run380
   * 4423C3, run370 43BB6F), and examines() takes any line holding one of its
   * entry words anywhere (c(), whole word): x, examine, look at, look in, ex,
   * exam, read, and from 3.9 bare look and l (run390 44B76C-44B833, run380
   * 43C69D, run370 434E2A, which has no "look in").  With no object named it
   * answers from its tail, "Nothing special." in a lit room: run390x
   * runner_probes/goto.run390.gs.txt and runner_probes/goto.run390.gs_b.txt
   * (`go to kitchen and look`, `... and l`, `... and read`, `... with x`, `...
   * and look in`), run380x runner_probes/goto.run380.gs_b.rtf (`go to first
   * room and read`, `... and x`; `go to kitchen and look` is gotoplace's
   * "Unknown place.").  A goto line that also names an object is not measured.
   * 4.0 splits `and look` off first (run400x
   * runner_probes/goto.run400.gs_b.txt).
   */
  if (version < TAF_VERSION_400 && lib_goto_line_examines (version, line))
    return lib_cmd_examine_other (game);

  if (has_alias)
    drop_front (alias.size () + 1);
  if (has_word ("goto"))
    drop_front (5);
  if (has_word ("go to"))
    drop_front (6);
  /* 3.9+; and a deviation below it, see lib_goto_line_enters(). */
  if (has_word ("go"))
    drop_front (3);
  text = line;
  uip_renote_named_npcs (game, line.c_str (), FALSE);

  const std::vector<std::string> names = lib_goto_room_names (game);

  const auto visited = [&] (scr_int candidate) -> scr_bool
    {
      return version < TAF_VERSION_400 || gs_room_seen (game, candidate);
    };

  /* Pass one: exact names. */
  count = 0;
  target = -1;
  named_elsewhere = FALSE;
  for (room = 0; room < rooms; room++)
    {
      if (names[room] == text && visited (room)
          && lib_goto_reachable (game, room, path))
        {
          target = room;
          count++;
          marked[room] = 1;
        }
    }

  /* Pass two: any name holding the text, when pass one found not one. */
  if (count != 1)
    {
      count = 0;
      for (room = 0; room < rooms; room++)
        {
          if (names[room].find (text) == std::string::npos || !visited (room))
            continue;
          named_elsewhere = TRUE;
          if (lib_goto_reachable (game, room, path))
            {
              target = room;
              count++;
              marked[room] = 1;
            }
        }
    }

  if (count == 0)
    {
      if (named_elsewhere)
        return lib_print_response_message (game,
                              "You can't get there from here.\n",
                              "I can't get there from here.\n",
                              "%player% can't get there from here.\n");
      return lib_print_message (game, "Unknown place.\n");
    }

  if (count > 1)
    {
      lib_goto_print_which (game, text, marked, count);
      return TRUE;
    }

  if (target == gs_playerroom (game))
    {
      pf_buffer_string (filter, lib_select_response (game, "You are already ",
                                                     "I am already ",
                                                     "%player% is already "));
      pf_buffer_string (filter, names[target].c_str ());
      pf_buffer_string (filter, "!\n");
      return TRUE;
    }

  lib_goto_walk (game, target, names[target]);
  return TRUE;
}
