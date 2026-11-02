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
 * Targeted game patches for the TAF parser, applied by parse_game() to the
 * parsed property bundle just before it is made readonly.  The notes below
 * say what a patch may and may not do.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "scarier.h"
#include "sctafpar_internal.h"


/*
 * Targeted game patches
 *
 * A few published ADRIFT games are unwinnable for want of a byte or two in
 * their own data -- an action the author never filled in, a variable index
 * off by one, an NPC name the parser can't split.  The general "assist"
 * switches can't help there: the game isn't asking the engine to do anything
 * the Runner does differently, it is simply asking for the wrong thing.  What
 * those games need is the correction the author would have made, so this
 * table carries them, and applies them to the parsed property bundle just
 * before it is made readonly.
 *
 * Rules the table sticks to, so that a patched game stays a game the engine
 * (and its saved games) can reason about:
 *
 *   o Match on GameName and GameAuthor, and then on content: every edit
 *     names the value the broken release holds, and a game that does not
 *     still hold all of them is left alone, silently.  A later release, a
 *     fan fix, or a same-named game by the same author therefore runs
 *     faithfully, and nothing here can half-apply.
 *
 *   o Edit fields only.  Values change, and an action may be added to a task
 *     that has none, but no task, object, room, NPC or variable is ever added
 *     or removed, so every index the engine and the save format hold on to
 *     keeps meaning what it meant.
 *
 * Off by default; parse_set_game_patches() turns it on, and a host that does
 * so can ask parse_get_applied_patch() what, if anything, it ended up doing.
 */
enum { PATCH_PATH_LENGTH = 64, PATCH_MAX_KEYS = 8 };

typedef struct
{
  const scr_char *path;       /* Bundle path, e.g. "Tasks/19/Actions/0/Var1" */
  scr_char mode;              /* See below. */
  scr_int from_integer;       /* Value the broken release holds. */
  scr_int to_integer;         /* Value to write instead. */
  const scr_char *from_string;
  const scr_char *to_string;
} scr_patch_edit_t;

/*
 * 'I' change an integer that is present and holds from_integer; 'B' the same
 * for a boolean, such as one room's flag in a Where room list; 'A' add an
 * integer to a path that holds nothing at all; 'N' add a string to a path
 * that holds nothing at all; 'S' change a string that is present and holds
 * from_string; 'V' verify a string and change nothing, which is how an edit
 * pins down the task it thinks it is editing.
 */
#define PATCH_SET(path, from, to) \
  { path, 'I', (from), (to), nullptr, nullptr }
#define PATCH_SET_BOOL(path, from, to) \
  { path, 'B', (from), (to), nullptr, nullptr }
#define PATCH_ADD(path, to) \
  { path, 'A', 0, (to), nullptr, nullptr }
#define PATCH_ADD_STRING(path, to) \
  { path, 'N', 0, 0, nullptr, (to) }
#define PATCH_STRING(path, from, to) { path, 'S', 0, 0, (from), (to) }
#define PATCH_VERIFY(path, value)   { path, 'V', 0, 0, (value), nullptr }

/*
 * Ebony's World (Laurie Griffiths).  Winning needs the dial pair, the lever
 * and the valve all set, but "turn valve on"/"turn valve off" were built by
 * copying the lever tasks and changing only the text: both still test and
 * write variable 2, the lever, instead of variable 3, the valve.  The valve
 * can therefore never be opened, and worse, the valve commands undo the
 * lever, which the route has already set.  Restriction Var1 is the variable
 * index plus two; action Var1 is the index itself.
 */
static const scr_patch_edit_t PATCH_EBONYS_WORLD[] = {
  PATCH_VERIFY ("Tasks/19/Command/0", "turn valve on"),
  PATCH_SET ("Tasks/19/Restrictions/0/Var1", 4, 5),
  PATCH_SET ("Tasks/19/Actions/0/Var1", 2, 3),
  PATCH_VERIFY ("Tasks/20/Command/0", "turn valve off"),
  PATCH_SET ("Tasks/20/Restrictions/0/Var1", 4, 5),
  PATCH_SET ("Tasks/20/Actions/0/Var1", 2, 3)
};

/*
 * Bedlam (Wild N Mild).  "ask barbara about keys" describes Barbara fetching
 * the car keys and throwing them at you, but the task has no actions at all,
 * so the keys never move.  "start car", the game's ending, wants them held
 * (its restriction is object location, dynamic object 15, held by player),
 * and without them the preview can't be finished.  The added action is the
 * one the neighbouring "take mitts" task uses, pointed at the keys.
 */
static const scr_patch_edit_t PATCH_BEDLAM[] = {
  PATCH_VERIFY ("Tasks/37/Command/0", "ask barbara about keys"),
  PATCH_ADD ("Tasks/37/Actions/0/Type", 0),
  PATCH_ADD ("Tasks/37/Actions/0/Var1", 18),
  PATCH_ADD ("Tasks/37/Actions/0/Var2", 4),
  PATCH_ADD ("Tasks/37/Actions/0/Var3", 0)
};

/*
 * Mystery House (KeKe Studio).  "open chest" answers "Wow! A Chest full of
 * what???" but, again, carries no actions, so the chest stays closed.  The
 * game ends on "drop treasure Chest", whose first restriction is that the
 * chest (stateful object 0) is open, so the win is unreachable.  The added
 * action opens it: change object status, stateful object 0, openness open.
 */
static const scr_patch_edit_t PATCH_MYSTERY_HOUSE[] = {
  PATCH_VERIFY ("Tasks/3/Command/0", "open chest"),
  PATCH_ADD ("Tasks/3/Actions/0/Type", 2),
  PATCH_ADD ("Tasks/3/Actions/0/Var1", 0),
  PATCH_ADD ("Tasks/3/Actions/0/Var2", 0)
};

/*
 * Illegal Socks (Trace).  The game's two villain NPCs are both named
 * "Dr. Myanus Hurts".  Version 4.0 splits an NPC's full name on ". ", taking
 * what follows as the name proper, so the engine ends up looking for a
 * character called "Myanus Hurts" and "attack dr" finds nobody to fight.  An
 * alias can't rescue it -- the 4.0 battle target match deliberately doesn't
 * fall back on aliases -- so the stop is the name itself.  Dropping the full
 * stop leaves the name as the author wrote it everywhere it is printed from
 * room and object text, which spells it out in full anyway.
 */
static const scr_patch_edit_t PATCH_ILLEGAL_SOCKS[] = {
  PATCH_STRING ("NPCs/6/Name", "Dr. Myanus Hurts", "Dr Myanus Hurts"),
  PATCH_STRING ("NPCs/13/Name", "Dr. Myanus Hurts", "Dr Myanus Hurts")
};

/*
 * Sandy's Lost Doll (Sandy).  Three "look in toilet" tasks run in sequence,
 * counting the variable "mom" up as mother catches you at it, and the third
 * ends the game.  The first two test "mom" properly (restriction Var1 = 2,
 * variable 0), but the third tests Var1 = 0, which is not a variable at all:
 * it is the number the player's command referred to.  Nothing in this game
 * has a %number% in its pattern, so that value is 0 for ever, the third look
 * is refused, and the game cannot be finished.  Var1 = 2 is what the other
 * two tasks say the author meant.
 */
static const scr_patch_edit_t PATCH_SANDY[] = {
  PATCH_VERIFY ("Tasks/8/Command/0", "look in toilet"),
  PATCH_SET ("Tasks/8/Restrictions/0/Var1", 0, 2)
};

/*
 * The Hangover (Red Conine).  The two tasks the endgame turns on carry a
 * room list of type 0 -- "no rooms" -- so neither can run anywhere at all,
 * while every other room-scoped task in the game names its one room.  Give
 * them the rooms their own text describes: task 10 the Psycho Hospital
 * Cafeteria (room 20), where the Doctor is, and task 14 the Form Process
 * Office (room 14), where the game's own task 13 puts the Platypus.
 *
 * Two more tasks need a hand.  Task 8, "open the filing cabinet", scores its
 * point but carries no action to open the cabinet and no text, and a silent
 * task claims its line before 4.0, so the library never opens it either and
 * the first approval form stays shut inside -- Mystery House's bug exactly.
 * The added action opens it (stateful object 2).
 *
 * Task 10 needs one more thing.  Its text has the doctor hand over both his
 * keys and "the second approval form", but its actions move only the keys,
 * and task 14 wants all three forms.  The added action moves the second form
 * (dynamic object 9) to the player, alongside the keys the task already
 * gives, which is the difference between 6 of 7 points and the game's win.
 */
static const scr_patch_edit_t PATCH_HANGOVER[] = {
  PATCH_VERIFY ("Tasks/8/Command/0", "open the filing cabinet"),
  PATCH_ADD ("Tasks/8/Actions/1/Type", 2),
  PATCH_ADD ("Tasks/8/Actions/1/Var1", 2),
  PATCH_ADD ("Tasks/8/Actions/1/Var2", 0),
  PATCH_VERIFY ("Tasks/10/Command/0", "give the doctor some french fries"),
  PATCH_SET ("Tasks/10/Where/Type", 0, 1),
  PATCH_ADD ("Tasks/10/Where/Room", 20),
  PATCH_ADD ("Tasks/10/Actions/4/Type", 0),
  PATCH_ADD ("Tasks/10/Actions/4/Var1", 12),
  PATCH_ADD ("Tasks/10/Actions/4/Var2", 4),
  PATCH_ADD ("Tasks/10/Actions/4/Var3", 0),
  PATCH_VERIFY ("Tasks/14/Command/0", "give approval notes to platypus"),
  PATCH_SET ("Tasks/14/Where/Type", 0, 1),
  PATCH_ADD ("Tasks/14/Where/Room", 14)
};

/*
 * Blood Relatives (davidw).  Two of the game's thirteen points hang off the
 * bronze key, which only task 391 ("open drawer") can produce, and task 391
 * first wants task 385 done.  Task 385's pattern is "exam desk" -- but the
 * game's own synonym table rewrites the player's "exam" (and "examine") to
 * "x" before any task pattern is tried, and that rewrite is never applied to
 * the patterns themselves, so the words stored here are words no player can
 * type.  "x desk" is what the synonym leaves; task 385 is the only one of the
 * game's dead "exam ..." patterns that gates a scoring task, so it is the
 * only one this touches.
 *
 * The vault ending stays out of reach, and deliberately so: task 344, the
 * door into it, tests "referenced number == 1", and no pattern in the game
 * has a %number% for a player to fill in -- but nothing in the data says
 * which variable the author meant instead, and guessing one would be a
 * rewrite, not a correction.
 */
static const scr_patch_edit_t PATCH_BLOOD_RELATIVES[] = {
  PATCH_VERIFY ("Tasks/391/Command/0", "open drawer"),
  PATCH_STRING ("Tasks/385/Command/0", "exam desk", "x desk")
};

/*
 * The Spirit's Flight (Andy Ko & Karen Kwan).  Four elemental guardians die
 * and leave their artifact behind: Fergo's task drops the Orb of Storms,
 * Kelorano's the Amber of Flames, Acuru's the Grass Amulet.  Crynasalda,
 * the water guardian, is the one whose task forgets to.  Her completion text
 * says it happens -- "the water elemental leaves her body and transforms
 * itself into an object" -- and her task does move the Sea Serpent's Scales
 * onto the player, but no action in the file moves the Ice Totem, whose only
 * two appearances in the whole game are as a restriction: it can be required
 * but never obtained.  That seals "invoke elementals" (task 21), and with it
 * the only exit into the earth cluster, so the game stops at 50 of 95.  The
 * added action is the one tasks 1 and 18 already carry, aimed at the totem:
 * drop it in the room the player is standing in.
 *
 * The chant then has to be sayable.  Task 29, the game's sole end-game
 * action, is where "Type: 0" -- "no rooms" -- so it can run nowhere, and
 * nothing executes it either.  Its own four restrictions say where it was
 * meant to run: all four artifacts in room 0, the Stone Circle, which is
 * where task 28's "pull trigger" transports the player, and whose
 * task-28 description ends "Replace the artifacts on the Granite Tablet and
 * say The Chant".  Give it that one room.
 *
 * Even in that room, though, the chant as the author typed it can never
 * reach a task.  Its one command is the four-line verse the tome in the
 * High Wizard's Tower prints, commas, full stops and all, and every Runner
 * from 3.9 up cuts input at the first "," and at the first ". " before it
 * looks at any task (run_find_split_pre400()), so the line arrives as
 * eleven fragments, each its own turn -- "wind", "water", ... and a "north"
 * that walks the player out of the circle.  The verse is unsayable by
 * construction, in run390 as much as here.  So the task gets a second
 * command, the one the game's own message asks the player for, leaving the
 * verse in place as command 0.
 *
 * Last, Carnifern.  The three guardians between the player and the Spirit
 * Paladin are built alike -- Griffon 20/20/20, the Paladin 30/30/27 -- but
 * Carnifern's Defense is 250 where its Stamina and Strength are 25 and 20.
 * Damage is strength less defence, and the best the player can reach is 25
 * with the golden axe's 10 on top, so nothing ever hurts it; its KilledTask
 * is what opens the way on, and the game ends there.  Read as the stray
 * trailing zero it looks like, 25 puts it between its two siblings.
 */
static const scr_patch_edit_t PATCH_SPIRITS_FLIGHT[] = {
  PATCH_VERIFY ("Tasks/12/Command/0", "Crynasalda t1"),
  PATCH_ADD ("Tasks/12/Actions/6/Type", 0),
  PATCH_ADD ("Tasks/12/Actions/6/Var1", 4),
  PATCH_ADD ("Tasks/12/Actions/6/Var2", 6),
  PATCH_ADD ("Tasks/12/Actions/6/Var3", 0),
  PATCH_VERIFY ("Tasks/29/Command/0",
                "Wind, water, fire, earth. Joy, pain, power, mirth."
                " The spirit's calling, in peace they rest."
                " The elementals four, north, suth, east, and west"),
  PATCH_SET ("Tasks/29/Where/Type", 0, 1),
  PATCH_ADD ("Tasks/29/Where/Room", 0),
  PATCH_ADD_STRING ("Tasks/29/Command/1", "say the chant"),
  PATCH_VERIFY ("NPCs/16/Name", "Carnifern"),
  PATCH_SET ("NPCs/16/Battle/Defense", 250, 25)
};

/*
 * Tenebrae Semper (Seciden Menarde).  Written for EctoComp's three-hour
 * slot, and the half of it that happens at night can never be entered.
 * Task 16, the stairwell door in the Science Center Hallway, narrates the
 * whole move -- "you see a forest path beyond ... you're already on the
 * path" -- and carries no actions at all, so the player stays in the
 * hallway.  Everything past it is in room 6 and room 7: event 2 waits
 * seven turns and then executes task 17, the beast, which is a room 6
 * task; task 19 shoots the beast and moves to room 7; task 23 examines the
 * pillow and moves back to room 6; and task 24, the game's only win
 * (EndGame v1 = 0, "Ending 4 of 5"), is a room 6 task too.  The added
 * action is the one tasks 19 and 23 already carry, aimed at the forest
 * path, and it starts the countdown the event is waiting on as well.
 *
 * Task 35 is the same omission a second time.  Its name is "# Go back to
 * science center", it answers `down` in the third-floor hallway once the
 * notebook trip is over, and it says "You walk across campus back to the
 * science center" -- but it carries no actions either, and the two tasks
 * that do walk there, 7 and 33, are both one-shot.  So a player who takes
 * the author's own route through the classroom cannot reach the stairwell
 * door a second time, whatever else is fixed.  It gets the same action,
 * aimed at SC-124.
 */
static const scr_patch_edit_t PATCH_TENEBRAE_SEMPER[] = {
  PATCH_VERIFY ("Tasks/16/CompleteText",
                "You enter the stairwell and climb to the upper floor. You"
                " open the door there, but rather than the expected hallway,"
                " you see a forest path beyond. You hesitate; your mind must"
                " be playing tricks on you. Probably best if you don't step"
                " through the door... but you're already on the path. The"
                " stairwell - and the rest of the science building - has"
                " vanished."),
  PATCH_ADD ("Tasks/16/Actions/0/Type", 1),
  PATCH_ADD ("Tasks/16/Actions/0/Var1", 0),
  PATCH_ADD ("Tasks/16/Actions/0/Var2", 0),
  PATCH_ADD ("Tasks/16/Actions/0/Var3", 6),
  PATCH_VERIFY ("Tasks/35/CompleteText",
                "You walk across campus back to the science center."),
  PATCH_ADD ("Tasks/35/Actions/0/Type", 1),
  PATCH_ADD ("Tasks/35/Actions/0/Var1", 0),
  PATCH_ADD ("Tasks/35/Actions/0/Var2", 0),
  PATCH_ADD ("Tasks/35/Actions/0/Var3", 5)
};

/*
 * The Annihilation of Think.com 3 (Pofty).  A numbered-choice chase, each
 * choice a task that moves the player to the room holding the next pair of
 * choices.  Task 20, choice B at the FINAL RESTING PLACE, is the one that
 * forgets: "Nexus confronts you, then turns and runs to your left" should
 * land the player in room 18, the Small cache, whose description asks the
 * last question of the game ("1. grab the sword and stab him or 2. Attack
 * him with Bil the Red iguana?").  No exit reaches room 18 and no task
 * moves anyone there, so the two tasks that live in it -- the last death
 * and task 22, the game's single point and its only winning EndGame --
 * cannot run, and the file tops out at 0 of 1.
 */
static const scr_patch_edit_t PATCH_TAOT3[] = {
  PATCH_VERIFY ("Tasks/20/CompleteText",
                "You keep hooking him at a safe distance and kick his sword"
                " to David, Nexus confronts yo, then turns and runs to your"
                " left."),
  PATCH_ADD ("Tasks/20/Actions/0/Type", 1),
  PATCH_ADD ("Tasks/20/Actions/0/Var1", 0),
  PATCH_ADD ("Tasks/20/Actions/0/Var2", 0),
  PATCH_ADD ("Tasks/20/Actions/0/Var3", 18)
};

/*
 * Crazy Old Bag Lady (Sprite).  Four townspeople have to be recruited before
 * the Golden Trolley can be hauled out of the river, and the hobos want
 * newspapers for their help.  Task 40, "talk to shop gal", ends with
 * "leaving her newspaper on the counter.  Searching behind the counter, you
 * find stacks of old newspapers.  Perfect!" and duly moves the newspapers
 * onto object 34, the counter -- but the counter is a static whose room list
 * is ROOMLIST_NO_ROOMS, so it stands in no room at all and nothing placed on
 * it is ever in scope again.  The newspapers cannot be taken, the hobos are
 * never paid, "People helping" stops at 3, and the game's only ending is out
 * of reach at 160 of 230.  The Corner Shop's own description says where the
 * counter belongs ("a cash register on the counter"), and every other fixture
 * in the game carries the one room it is written into, so the patch gives the
 * counter the Corner Shop (room 14, stored one-based).
 */
static const scr_patch_edit_t PATCH_COBL[] = {
  PATCH_VERIFY ("Objects/34/Short", "counter"),
  PATCH_VERIFY ("Objects/34/Description",
                "The old and battered counter has an equally battered cash"
                " register on it. It looks like it wouldn't stand up to a"
                " well-placed kick, but nobody's tried to rob it yet..."
                " probably because it's actually just a poorly written object"
                " that can't actually be opened. Handy, that."),
  PATCH_SET ("Objects/34/Where/Type", 0, 1),
  PATCH_ADD ("Objects/34/Where/Room", 15)
};

/*
 * DeL SoL MADNESS (Alvin Echeverria).  Four school periods on a fixed clock,
 * then a chemistry nightmare to fight out of.  MaxScore is 46; 26 is all the
 * game can hand out, and the missing twenty and the win are three unfilled
 * fields.
 *
 * Hina's note in Physics asks "Did you make out with Jarvin today in
 * woodshop?".  Task 14 (`no`, +10) wants kissing_jarvin == 0 and task 18
 * ("You are such a LIAR, Noslen") wants it >= 1 -- a clean pair, except the
 * variable is initialised to 1, so a truthful no is always called a lie and
 * the ten points cannot be scored.  Nothing sets the variable to 0 either:
 * the only decrement is PE's `practice` (-2), a period later than the
 * Physics-only task that reads it, and the PE trio practice/check out/make
 * out nets exactly zero -- which is the balance the author was writing for.
 *
 * Task 27 (`# bring hina`, +10, also kissing_jarvin == 0) is orphaned: its
 * `#` command cannot be typed, no event's affTask names it and no type-5 exec
 * action reaches it.  Its text -- "Hina has seen that you truly love her and
 * has come to help you beat the evil sponges" -- and its action, move Hina to
 * the player, both belong to the moment task 4 (`# move to chem dream`) drops
 * the player and the sponges into room 6, so task 4 gets the exec.
 *
 * The win, task 26 (`# super win`), is gated to room 6 and dispatched by a
 * KilledTask -- carried by NPC 9, the waking Ms Moreland, who has no battle
 * stats at all, stands in room 3 and is never moved, so the dispatch is
 * refused for ineligibility and dies silently (verified in run400).  The
 * nightmare's own MoReLaND, NPC 10, is the one in room 6 with the stats, and
 * her KilledTask is unset; the slingshot from woodshop kills her in a few
 * throws.  She gets the task her waking twin was given by mistake.
 */
static const scr_patch_edit_t PATCH_DEL_SOL[] = {
  PATCH_VERIFY ("Variables/3/Name", "kissing_jarvin"),
  PATCH_STRING ("Variables/3/Value", "1", "0"),
  PATCH_VERIFY ("Tasks/4/Command/0", "# move to chem dream"),
  PATCH_VERIFY ("Tasks/27/Command/0", "# bring hina"),
  PATCH_ADD ("Tasks/4/Actions/4/Type", 5),
  PATCH_ADD ("Tasks/4/Actions/4/Var1", 0),
  PATCH_ADD ("Tasks/4/Actions/4/Var2", 27),
  PATCH_VERIFY ("NPCs/10/Name", "MoReLaND"),
  PATCH_VERIFY ("NPCs/9/Name", "Moreland"),
  PATCH_SET ("NPCs/10/Battle/KilledTask", 0, 27)
};

/*
 * The Quest For More Hair, or AMU Part 1: The Smugglers (Matt, Dark Baron;
 * the .taf still carries the generator's placeholder, quotes and all).
 * MaxScore is 100, 60 is reachable, and the last forty and the ending are
 * behind two unfilled fields.
 *
 * `rope cliff` (task 36) is Reversible with itself as its reverse command:
 * tie the rope to climb, type it again to untie and walk away.  Both
 * directions are gated by the same restriction, "rope held by player", and
 * the forward action moves the rope to hidden -- so the moment it succeeds
 * its own reverse becomes impossible.  The Airport's south and west exits
 * are the ones that need it undone, so the briefcases, the ring 987 phone
 * call, the tapes and the tape player -- twenty points -- are walled off
 * behind a task that can only ever be done once.  The rope stays in hand
 * instead of vanishing, which is what "you will have to de-attach the rope
 * if you want to go far away" describes anyway.
 *
 * Hamish the evil smuggler (NPC 9) stands in room 44 with 50 stamina and is
 * the game's last fight, and task 43 (`^^smugglerdiesevent^^`, +5, gated to
 * room 44) is written for the moment he dies -- "The smuggler dies, as he
 * does so he says..." with "You need to get to the hair shop!" after it.
 * Nothing runs it: its `^^...^^` command cannot be typed, there are no
 * events in the file, and Hamish's KilledTask is unset.  He gets it, and
 * with it the hair shop, `buy hair` (+10) and `talk king` (+5), the ending.
 */
static const scr_patch_edit_t PATCH_LIQID[] = {
  PATCH_VERIFY ("Tasks/36/Command/0", "*rope* *cliff*"),
  PATCH_VERIFY ("Tasks/36/ReverseCommand/0", "*rope* *cliff*"),
  PATCH_VERIFY ("Objects/19/Short", "Rope"),
  PATCH_SET ("Tasks/36/Repeatable", 1, 0),
  PATCH_SET ("Tasks/36/Actions/0/Var2", 0, 4),
  PATCH_VERIFY ("Tasks/43/Command/0", "^^smugglerdiesevent^^"),
  PATCH_VERIFY ("NPCs/9/Name", "Hamish"),
  PATCH_SET ("NPCs/9/Battle/KilledTask", 0, 44)
};

/*
 * The Vampire with a Conscience (Ole Olsen).  Two hours in the Oslo Plaza
 * before the grand vampire convention, and one unticked checkbox between
 * the game and its ending.
 *
 * Task 61 is the way back east out of Bozo's backyard into the nightclub:
 * its commands are the four spellings of east, it is gated to room 11 and
 * its one action moves the player to room 6.  The backyard has no exits of
 * its own, so the task is the only way out -- and it has to be taken twice,
 * once after planting the stick in the container's handles and once after
 * raising Jon from the coffin.  The author knew: the CompleteText reads
 * "You enter the nightclub again." and the RepeatText holds a single space,
 * a blank turn put there to hide "You have already done that.".  What the
 * author did not do was tick Repeatable, so the second east is claimed by
 * the pre-4.0 spent-task rule, prints the blank line, and the game walls at
 * 70 of 100 in the backyard with Jon awake and nowhere to take him
 * (RUNNER-MEASURED 2026-08-31, run390 runner_probes/vampire.run390.txt).
 * Ticking the box is what the CompleteText already claims: the RepeatText
 * is whitespace, so the task re-runs forwards and says its line again.
 */
static const scr_patch_edit_t PATCH_VAMPIRE[] = {
  PATCH_VERIFY ("Tasks/61/Command/0", "east"),
  PATCH_VERIFY ("Tasks/61/CompleteText", "You enter the nightclub again."),
  PATCH_VERIFY ("Tasks/61/RepeatText", " "),
  PATCH_SET ("Tasks/61/Repeatable", 0, 1)
};

/*
 * The Merry Murders (Mel S.).  A seven-act whodunit at the SynTex Christmas
 * party, 135 points of which the last fifteen are behind a door that cannot
 * be walked through.
 *
 * Task 46 unlocks the Archives door from the Computer Lab: "I stuck the key
 * into the door, and twisted it. The lock opened, allowing me access into
 * the archives."  It only unlocks -- it has no movement action -- and the
 * Computer Lab's north exit is gated on the task being done, so the player
 * is meant to type the unlock and then walk.  But the task's first command
 * slot is `n` itself, so the unlock consumes the walk: the spent task then
 * claims every later `n` in the room ("I have already done that.") and the
 * Archives, the Hidden Room and the roof finale are unreachable at 120 of
 * 135 (RUNNER-MEASURED 2026-08-31, run390
 * runner_probes/merry_murders.run390.txt).  The slot is redundant -- the
 * same task is reached by `open door`, `unlock door` and four `use key`
 * spellings, all of them in the list already -- so it is renamed to a
 * phrase that is not a direction, and north goes back to being north.
 */
static const scr_patch_edit_t PATCH_MERRY_MURDERS[] = {
  PATCH_VERIFY ("Tasks/46/CompleteText",
                "I stuck the key into the door, and twisted it. The lock"
                " opened, allowing me access into the archives."),
  PATCH_VERIFY ("Tasks/46/Command/3", "open door"),
  PATCH_STRING ("Tasks/46/Command/0", "n", "unlock the archive door")
};

/*
 * Fun Town (Mandrake).  A hub-and-spoke resort of twenty ten-point episodes,
 * a declared MaxScore of exactly 200, a treasure chest in a cave, and a
 * WinText that congratulates the player for accumulating "the full 200
 * possible points needed to win the game".  Task 117, `open treasure chest`,
 * is the file's only EndGame v1=0, and it asks for all twenty episodes by
 * task number -- twenty Type 2 restrictions, each with the same "Sorry,
 * there is no way to open the chest before you accumulate 200 points."
 *
 * Nineteen of the twenty named tasks award their ten points.  The twentieth,
 * restriction 17, names task 105 (index 104), `* fuc* in Room #1 * no
 * condom`, which awards nothing and ends the game: its last action is
 * EndGame v1=2, a death.  Its twin one slot further on, task 106 (index
 * 105), `* fuc* in Room #1 * with condom`, is the survivable half of the
 * same scene, awards the ten points the chest's arithmetic needs, and is not
 * in the gate at all -- its own refusal text spells the pair out, "Thank God
 * you remembered to wear a condom....  You would have died otherwise."  So
 * the chest asks the player to be dead, and a full 200/200 playthrough is
 * told it has not accumulated 200 points (confirmed at 200/200 in the
 * faithful row).  The restriction is moved one task along, onto the twin
 * that scores, and the twenty awards then sum to the declared MaxScore.
 */
static const scr_patch_edit_t PATCH_FUNTOWN[] = {
  PATCH_VERIFY ("Tasks/117/Command/0", "* open * treasure * chest *"),
  PATCH_VERIFY ("Tasks/104/Command/0", "* fuc* in Room #1 * no condom"),
  PATCH_VERIFY ("Tasks/105/Command/0", "* fuc* in Room #1 * with condom"),
  PATCH_SET ("Tasks/117/Restrictions/17/Var1", 105, 106)
};

/*
 * Sommeril, H. Lee Parten
 *
 * The game is a hunt for the five torn pages of a book, and the wet page is
 * the fish's.  Dropping the fish into the fountain runs task 18, which moves
 * the fish into the fountain's room and the WET PAGE into the FOUNTAIN, and
 * says so: "A wet page has floated to the surface of the water."  Task 6,
 * "take wet page", is the page's ten points, and it also tells the fish to
 * dive -- "With a flick of his tail, the fish swims out of sight, deep into
 * the dark water."
 *
 * Task 6's one restriction asks that the WET PAGE be held by the FISH.  An
 * NPC never holds it: task 18 put it inside the FOUNTAIN, which is where the
 * task's own text says it is floating, so the restriction is false at the
 * only moment the task can be typed.  The task is refused, the line falls
 * through to the library take, and the player is told "You take the WET PAGE
 * from the FOUNTAIN." without the ten points and without the fish diving.
 * With the page taken the task can never be reached again, and the game is
 * stranded ten short of the 95 its scoring tasks can sum to.
 *
 * The restriction is moved from the fish onto the fountain the fish is in:
 * "inside" rather than "held by", and the FOUNTAIN, the sixth container, in
 * place of the first NPC.  Both are the one dropdown pair on the Generator's
 * object-location restriction, so this is the selection the author's own
 * text describes.
 */
static const scr_patch_edit_t PATCH_SOMMERIL[] = {
  PATCH_VERIFY ("Tasks/6/Command/0", "take wet page"),
  PATCH_VERIFY ("Tasks/6/CompleteText",
                "With a flick of his tail, the fish swims out of sight, deep"
                " into the dark water."),
  PATCH_VERIFY ("Tasks/18/AdditionalMessage",
                "The fish swims in small circles near the edge of the"
                " fountain. A wet page has floated to the surface of the"
                " water."),
  PATCH_SET ("Tasks/6/Restrictions/0/Var2", 1, 4),
  PATCH_SET ("Tasks/6/Restrictions/0/Var3", 2, 6)
};

/*
 * Locked Out, Graeme Allen
 *
 * The last twenty points are task 20, "put rock on lid": the rock that broke
 * the box open earlier is weighted onto the trash can lid so the raccoons
 * stay out, and the task announces its own award, "..::You've earned 20
 * points!::..".  It carries no restrictions.
 *
 * Its Where is "No rooms", so the task is allowed to run nowhere at all.  A
 * player standing in the alley with the rock in hand and the lid in front of
 * them types the command the task was written for and gets the library's
 * "You put the large rock onto trash can lid." instead: the rock lands on
 * the lid, the raccoon line is never printed, and the game tops out at
 * 90/110 with no way to reach the twenty it still advertises.
 *
 * The Where is opened to "All rooms".  The task has no other gate, but it is
 * unrepeatable and names both objects in its command, so it can only be
 * completed once, in the one place the rock and the lid are together.
 */
static const scr_patch_edit_t PATCH_LOCKEDOUT[] = {
  PATCH_VERIFY ("Tasks/20/Command/0", "put rock on lid"),
  PATCH_VERIFY ("Tasks/20/CompleteText",
                "You place the rock you used to bash the life out of your"
                " little box earlier on top of the trash can lid. You get the"
                " feeling that it will prevent the raccons from getting in"
                " the trash again."),
  PATCH_SET ("Tasks/20/Where/Type", 0, 3)
};

/*
 * Villains and Kings, Neal, the GREAT
 *
 * The soap on a rope sits inside the Broken Window, and task 5,
 * "take * soap * * *", is the point for fishing it out: it moves the soap
 * into the player's hands, awards one, and answers "you snatch up the soap.
 * good for you."  Its one restriction asks that the window be open, and its
 * refusal is written for a player who has not opened it yet -- "your keen
 * sense of smell detects soap, yet you do not see any."
 *
 * Its Where is "No rooms", so the task never runs anywhere.  The library
 * take hands the soap over instead, unscored, and the game's declared 37
 * cannot be beaten by more than 36.  The Where is opened to "All rooms";
 * the window restriction is what the task was gated on, and it still is.
 */
static const scr_patch_edit_t PATCH_VILLAINS_AND_KINGS[] = {
  PATCH_VERIFY ("Tasks/5/Command/0", "take * soap * * *"),
  PATCH_VERIFY ("Tasks/5/CompleteText", "you snatch up the soap.  good for you."),
  PATCH_SET ("Tasks/5/Where/Type", 0, 3)
};

/*
 * FunHouse, C. A. Gist
 *
 * The declared MaxScore is 410 and four ChangeScore actions sum to it: the
 * ring (+110) and three +100 tasks that all pay out for the hundred-dollar
 * bill at the ticket booth.  The author wired one phrasing per task rather
 * than one task with three phrasings -- task 12 `take hundred dollars`
 * (Where: room 0), task 11 `pick up money` (all rooms) and task 10 `take
 * money` -- and none of the three carries a restriction, so all three are
 * meant to be typed and the arithmetic only reaches 410 if they are.
 *
 * Task 10's Where is "No rooms", so the third hundred can never be banked and
 * the game tops out at 310 (the faithful row's score).  Its Where is opened
 * to "All rooms", which is what task 11, the twin one slot along, already
 * holds; the task is unrepeatable, so the hundred is still paid once.  Seven
 * other tasks in this game share the empty room list and are left alone: none
 * of them scores, and the patch table does not tidy, it unblocks.
 */
static const scr_patch_edit_t PATCH_FUNHOUSE[] = {
  PATCH_VERIFY ("Tasks/10/Command/0", "take money"),
  PATCH_VERIFY ("Tasks/10/CompleteText", "You deserve it!"),
  PATCH_VERIFY ("Tasks/11/Command/0", "pick up money"),
  PATCH_VERIFY ("Tasks/12/Command/0", "take hundred dollars"),
  PATCH_SET ("Tasks/10/Where/Type", 0, 3)
};

/*
 * Goldilocks - Breaking & Entering, L.C.N.
 *
 * Fourteen ChangeScore tasks sum to the declared 35, and two of them are
 * unreachable, three points in all.
 *
 * The golden egg (+2) sits inside the refrigerator, and task 46 `get egg` is
 * the task that pockets it -- "You pickup the golden egg and put it in your
 * sack." -- gated on one object-state restriction whose fail message is
 * "There is no egg here.".  That restriction addresses Var1 = 0, which is not
 * the refrigerator but "the referenced object", and the task's pattern has no
 * %object% in it to bind one, so the test can never pass, the line falls
 * through to the library take, and the egg is handed over unscored.  Var1 = 2
 * is the refrigerator, the second of the game's two stateful objects, and
 * Var2 = 0 is the openness the fail message describes: with the fridge open
 * the egg is there, with it shut it is not.
 *
 * The rope (+1) is the same pair of tasks written twice.  Task 52 has the
 * score and the action that hides the tied rope but Where "No rooms"; task
 * 53, the re-write, has the Large Bedroom, a wider pattern that also accepts
 * "bars", and the gate the puzzle needs -- task 51, the tabasco poured over
 * the window bars, must be done, "That would be pointless." otherwise -- but
 * no actions at all.  53 is the one that runs, and it is where the missing
 * point belongs: it gains the ChangeScore its dead twin carries.  Moving the
 * rope is deliberately not copied with it; 52 sends the rope to hidden, and
 * the climb down that follows names it.
 */
static const scr_patch_edit_t PATCH_GOLDILOCKS_BE[] = {
  PATCH_VERIFY ("Tasks/46/CompleteText",
                "You pickup the golden egg and put it in your sack."),
  PATCH_VERIFY ("Tasks/46/Restrictions/0/FailMessage", "There is no egg here."),
  PATCH_SET ("Tasks/46/Restrictions/0/Var1", 0, 2),
  PATCH_VERIFY ("Tasks/53/CompleteText",
                "You quickly tie the tope to the small piece of bar poking out"
                " of the window frame."),
  PATCH_VERIFY ("Tasks/53/Restrictions/0/FailMessage",
                "That would be pointless.<br>"),
  PATCH_ADD ("Tasks/53/Actions/0/Type", 4),
  PATCH_ADD ("Tasks/53/Actions/0/Var1", 1)
};

/*
 * Melbourne Beach, David Good.
 *
 * Seventeen ChangeScore tasks sum to the declared 41, and the one worth the
 * Laundry's last point cannot be typed at all.  The synonym table ends with a
 * pair the author clearly meant as "either spelling will do": synonym 3
 * rewrites `dryer` to `drier`, synonym 4 rewrites `drier` back to `dryer`.
 * The Runner applies the table as a sequence, each rule rewriting the
 * previous rule's output -- see pf_filter_input() in scprintf_input.cpp, and the
 * Vardock Bates transcript that pins the order -- so both spellings arrive at
 * the tasks as `dryer`, and task 48's pattern `turn* drier` is out of reach of
 * any ordinary line, in run390 exactly as here.  "turn on drier" falls instead
 * to task 56 eight slots along, the same room and the same restriction and the
 * same first action but no ChangeScore, and answers "You set the dryer for an
 * appropriate period of time and turn it on." -- measured, score unmoved.
 * The only line that does reach 48 is one built to defeat the rewrite's
 * whole-word gate, `turn driers drier`, and taking it is a trap: see below.
 *
 * So the pattern is re-spelt to what the rewrite actually produces, which is
 * also what 56 already accepts (`turn* dr*`); 48 is the lower-numbered and
 * takes the line back.  Its second action needs the same repair to be worth
 * having: 56 puts the dry clothes into the dryer (dynamic object 22, action
 * Var1 25), while 48 names Var1 28, three higher -- the dirty clothes the
 * wash cycle has just hidden -- and hauls those back into the drum with the
 * dry ones left nowhere.  That is the trap in the forced line: `wash clothes`
 * cannot be repeated, so there is no second load to dry, and the fold (+5)
 * goes for a point.  Fixed, the task does what its twin does, and what its
 * own "You turn on the drier." says it does.
 *
 * That is one point and not three, because the other two are beyond any
 * route.  `drink coffee` is written four times over, once per cup and once
 * per state of `drink oil` (task 7, the game's only penalty; a task-state
 * restriction names it 8, one-based).  Tasks 41 (yellow cup, oil not drunk)
 * and 42 (red cup, oil not drunk) would be the pair to have, but the red cup
 * stays hidden until the Captain trade in task 36, and that wants the oil
 * drunk -- which shuts 42 and opens 39, whose point the -1 then pays for.  At
 * most two of the three ever score, and never for more than one point net.
 * 39 of 41 is the ceiling; faithful, the route reaches 38.
 */
static const scr_patch_edit_t PATCH_MELBOURNE_BEACH[] = {
  PATCH_VERIFY ("Synonyms/3/Original", "dryer"),
  PATCH_VERIFY ("Synonyms/3/Replacement", "drier"),
  PATCH_VERIFY ("Synonyms/4/Original", "drier"),
  PATCH_VERIFY ("Synonyms/4/Replacement", "dryer"),
  PATCH_VERIFY ("Tasks/48/CompleteText",
                "You turn on the drier.  What a great guest, you even help"
                " out with the laundry. "),
  PATCH_STRING ("Tasks/48/Command/0", "turn* drier", "turn* dryer"),
  PATCH_SET ("Tasks/48/Actions/1/Var1", 28, 25)
};

/*
 * Mesto prestupleniya 2 ("Crime Scene 2"), Sirius.  Russian, cp1251; the
 * strings below are escaped so this file stays ASCII.
 *
 * The game's synonym table translates the player's Russian verbs into the
 * English the library parses -- vzyat' to get, otkryt' to open, razbit' to
 * smash, yug to south, sever to north -- and the Runner applies the table to
 * the typed line before any task sees it (run390 45F18C, the gate ported as
 * pf_apply_synonym()).  But seventeen task commands are written with the
 * Russian verb itself, so nothing typed can ever reach them, in run390
 * exactly as here (the faithful row is word-for-word identical to a Wine
 * run390 transcript).  Among them are every evidence pickup (the glass, the
 * cartridge case, the knife, the documents), the wardrobe, the cane, the
 * mirror and the safe, and every `yug` that drives the second act out of the
 * house: to the lab, where the evidence is handed over, and to the police
 * station, where the report that ends the game is written.  Shipped, the
 * game tops out at 23 of 70 with no win.
 *
 * Each command is re-spelt with the verb its own synonym leaves behind, so
 * `vzyat' stakan` now arrives as `get stakan` and finds the task written for
 * it.  Nothing else changes: the restrictions, actions and texts are the
 * author's, including the sever/yug tasks' gates (Rozhe's protest, the lab
 * guard's badge check) that the library walk used to skip.
 */
static const scr_patch_edit_t PATCH_CRIME_SCENE_2[] = {
  PATCH_VERIFY ("Synonyms/5/Original", "\xfe\xe3"),
  PATCH_VERIFY ("Synonyms/5/Replacement", "south"),
  PATCH_VERIFY ("Synonyms/6/Original", "\xf1\xe5\xe2\xe5\xf0"),
  PATCH_VERIFY ("Synonyms/6/Replacement", "north"),
  PATCH_VERIFY ("Synonyms/25/Original", "\xe2\xe7\xff\xf2\xfc"),
  PATCH_VERIFY ("Synonyms/25/Replacement", "get"),
  PATCH_VERIFY ("Synonyms/54/Original", "\xee\xf2\xea\xf0\xfb\xf2\xfc"),
  PATCH_VERIFY ("Synonyms/54/Replacement", "open"),
  PATCH_VERIFY ("Synonyms/67/Original", "\xf0\xe0\xe7\xe1\xe8\xf2\xfc"),
  PATCH_VERIFY ("Synonyms/67/Replacement", "smash"),
  PATCH_STRING ("Tasks/0/Command/0",
                "\xf1\xe5\xe2\xe5\xf0",
                "north"),
  PATCH_STRING ("Tasks/10/Command/0",
                "\xf0\xe0\xe7\xe1\xe8\xf2\xfc \xe7\xe5\xf0\xea\xe0\xeb\xee",
                "smash \xe7\xe5\xf0\xea\xe0\xeb\xee"),
  PATCH_STRING ("Tasks/11/Command/0",
                "\xee\xf2\xea\xf0\xfb\xf2\xfc \xf8\xea\xe0\xf4",
                "open \xf8\xea\xe0\xf4"),
  PATCH_STRING ("Tasks/13/Command/0",
                "\xe2\xe7\xff\xf2\xfc \xf2\xf0\xee\xf1\xf2\xfc",
                "get \xf2\xf0\xee\xf1\xf2\xfc"),
  PATCH_STRING ("Tasks/15/Command/0",
                "\xf0\xe0\xe7\xe1\xe8\xf2\xfc \xe7\xe5\xf0\xea\xe0\xeb\xee \xf2\xf0\xee\xf1\xf2\xfc*",
                "smash \xe7\xe5\xf0\xea\xe0\xeb\xee \xf2\xf0\xee\xf1\xf2\xfc*"),
  PATCH_STRING ("Tasks/19/Command/0",
                "\xe2\xe7\xff\xf2\xfc \xf1\xf2\xe0\xea\xe0\xed",
                "get \xf1\xf2\xe0\xea\xe0\xed"),
  PATCH_STRING ("Tasks/28/Command/0",
                "\xe2\xe7\xff\xf2\xfc \xe3\xe8\xeb\xfc\xe7*",
                "get \xe3\xe8\xeb\xfc\xe7*"),
  PATCH_STRING ("Tasks/29/Command/0",
                "\xe2\xe7\xff\xf2\xfc \xed\xee*",
                "get \xed\xee*"),
  PATCH_STRING ("Tasks/58/Command/0",
                "\xee\xf2\xea\xf0\xfb\xf2\xfc \xf1\xe5\xe9\xf4 \xea\xeb\xfe\xf7*",
                "open \xf1\xe5\xe9\xf4 \xea\xeb\xfe\xf7*"),
  PATCH_STRING ("Tasks/60/Command/0",
                "\xe2\xe7\xff\xf2\xfc \xe4\xee\xea\xf3\xec\xe5\xed\xf2*",
                "get \xe4\xee\xea\xf3\xec\xe5\xed\xf2*"),
  PATCH_STRING ("Tasks/61/Command/0",
                "\xfe\xe3",
                "south"),
  PATCH_STRING ("Tasks/66/Command/0",
                "\xfe\xe3",
                "south"),
  PATCH_STRING ("Tasks/68/Command/0",
                "\xf1\xe5\xe2\xe5\xf0",
                "north"),
  PATCH_STRING ("Tasks/71/Command/0",
                "\xfe\xe3",
                "south"),
  PATCH_STRING ("Tasks/79/Command/0",
                "\xe2\xe7\xff\xf2\xfc \xe1\xf3\xec\xe0\xe3*",
                "get \xe1\xf3\xec\xe0\xe3*"),
  PATCH_STRING ("Tasks/80/Command/0",
                "\xe2\xe7\xff\xf2\xfc \xf0\xf3\xf7\xea*",
                "get \xf0\xf3\xf7\xea*"),
  PATCH_STRING ("Tasks/81/Command/0",
                "\xee\xf2\xea\xf0\xfb\xf2\xfc \xf1\xf2\xee\xeb",
                "open \xf1\xf2\xee\xeb")
};

/*
 * The Fugitive, Renata Burianova (arctica0@hotmail.com).
 *
 * Task 73, `look * mirror` in the three drivable cars, is worth ten points
 * and cannot be typed.  Synonym 49 rewrites `look` to `l` (the author making
 * the one-letter form work everywhere), and the Runner applies the table to
 * the typed line before any task sees it (run390 45F18C, pf_apply_synonym()),
 * so `look in mirror` reaches the tasks as `l in mirror`; `see` and `stare`
 * are rewritten to `look` only after that, and synonym 112 turns what they
 * leave into `x`.  Nothing typed carries the literal `look`, in run390
 * exactly as here, and the game tops out at 656 of 666.
 *
 * The pattern is re-spelt `l * mirror`, what synonym 49 leaves behind.  The
 * ceiling does not move: the mirror is in the cars, and the taxi is the only
 * ride with a driver to `fight` for his pistol (+10), which is what turns the
 * punkers' ambush into the one that leaves the beer (+10).  The choice of
 * vehicle is exclusive, so a car route that banks the mirror still gives up
 * twenty; the patch makes the car the author's lesser choice rather than a
 * silent one.
 */
static const scr_patch_edit_t PATCH_FUGITIVE[] = {
  PATCH_VERIFY ("Synonyms/49/Original", "look"),
  PATCH_VERIFY ("Synonyms/49/Replacement", "l"),
  PATCH_VERIFY ("Tasks/73/CompleteText",
                "You see a weary, tired face looking back at you. You need"
                " more sleep, that's for sure."),
  PATCH_STRING ("Tasks/73/Command/0", "look * mirror", "l * mirror")
};

/*
 * Professor Von Witt's Fabulous Flying Machine, Mystery.
 *
 * The green button ends the game three ways, and the best of the three cannot
 * be pressed.  Task 52 is the button with the machine unridden -- "task 52
 * must NOT be done", one-based, so task 51, the climb aboard -- and the
 * contraption trundles off into the crowd without you.  Task 53 is the button
 * while riding, and it flies away with you, "never seen again".  Task 54 is
 * the same button, riding, and the bottle cap held (dynamic object 11, so
 * Var1 = 14), and it is the win: EndGame Var1 = 0, and the game's largest
 * ChangeScore, 75 of a declared 229 that twenty-three tasks sum to exactly.
 * But 53's single restriction is a subset of 54's two and 53 has the lower
 * number, so the dispatcher stops there every time, whatever the player is
 * carrying.  The author's own bundled walkthrough is the proof: it holds the
 * cap when it presses the button and still gets 53's ending, and the game
 * ships with 154 of 229 as its ceiling.
 *
 * What 53 is missing is the complement of the gate 54 adds -- the case it was
 * written for is riding WITHOUT the cap -- so it gains it: object restriction
 * Var1 = 14 again, Var2 = 7, the negated "held by the player" (6..11 are the
 * negated forms, and the runner evaluates them for a named object; see
 * restr_pass_task_object_location()).  RestrMask goes from "#" to "#A#" with
 * it, because a mask that names one restriction leaves any others
 * unevaluated.  Nothing else moves: 52, 53 and 54 keep their commands, their
 * endings and their scores, and the cap becomes what its place in 54 always
 * said it was -- the one thing that decides which way the flight goes.
 *
 * With that, the bundled walkthrough's own command list wins on 229 of 229.
 */
static const scr_patch_edit_t PATCH_PROFESSOR[] = {
  PATCH_VERIFY ("Tasks/52/Command/0",
                "[press/push/hit]{the}{small}{green}[button]"),
  PATCH_VERIFY ("Tasks/53/Command/0",
                "[press/push/hit]{the}{small}{green}[button]"),
  PATCH_VERIFY ("Tasks/53/Command/3", "* hit * button *"),
  PATCH_VERIFY ("Tasks/54/Command/0",
                "[push/press/hit]{the}{small}{green}[button]"),
  PATCH_STRING ("Tasks/53/RestrMask", "#", "#A#"),
  PATCH_ADD ("Tasks/53/Restrictions/1/Type", 0),
  PATCH_ADD ("Tasks/53/Restrictions/1/Var1", 14),
  PATCH_ADD ("Tasks/53/Restrictions/1/Var2", 7),
  PATCH_ADD ("Tasks/53/Restrictions/1/Var3", 0),
  PATCH_ADD_STRING ("Tasks/53/Restrictions/1/FailMessage", "")
};

/*
 * The Adventures of Space Boy! Volume I, David Parish.
 *
 * Task 11 hands over the Ice Gloves in the Castle's Ice Room and is worth
 * thirty points, and its command is `{take\get} {them/it/} {the} gloves` --
 * a backslash where every other alternation in the game has a slash.  The
 * pattern parser has no meaning for it, so the only line that could match is
 * one containing the literal `take\get`, which nothing types; the line falls
 * through to the library take instead, which fetches the gloves perfectly
 * well and scores nothing.  Both engines behave the same way, and the game
 * plays through to its ending with the thirty missing, so the slip is
 * invisible unless the score is counted.
 *
 * The one character is corrected.  A get/take line that a take-family task
 * matches is dispatched to the task before the library take ever sees it
 * (run400 get_piece 473A34 ahead of the dispatcher at 48A481, modelled in
 * run_priority_commands()), so `take gloves` now runs task 11: the same
 * "You have the gloves.", the same move of the gloves into the player's
 * hands, and the thirty points the author wrote for it.
 *
 * That leaves the declared 1374 still out of reach, and not by a patchable
 * margin.  The twenty-three ChangeScore tasks sum to 1319, fifty-five short
 * of the advertised maximum before anything else is said; task 72, the cape
 * dropped on the floor for 250, is a command the 4.0 library's own drop
 * answers outright without asking the tasks (see
 * run_task_reachable_by_library_callback()); and the two +30 transporter
 * buttons cannot both be pressed on one route, because the Moon Base's
 * `enter transporter` (+50) brings the player home instead of the red button
 * that would.  1039 of 1374 is the honest ceiling, and the walkthrough row
 * reaches it.
 */
static const scr_patch_edit_t PATCH_SPACE_BOY[] = {
  PATCH_VERIFY ("Tasks/11/CompleteText", "You have the gloves."),
  PATCH_STRING ("Tasks/11/Command/0",
                "{take\\get} {them/it/} {the} gloves",
                "{take/get} {them/it/} {the} gloves")
};

/*
 * The Night The Moon Shone Grey, Matthew Moya.
 *
 * Task 17 beheads the dark elf's body in the Library he loved, and is worth
 * twenty of the declared 400.  Its two restrictions cannot both hold.  The
 * first is the one the puzzle is about -- dynamic object 19, the dead dark
 * elf, in room 30, "The dark elf's body must be in the library he loved so
 * much." -- and the second asks for "the referenced object" to be held by the
 * player, "You do not have %object%.", plainly meant as "and a blade in your
 * hand", the task's third command being `behead dark elf with %object%`.  But
 * the reference a `behead` line leaves behind is the body, not the weapon:
 * `behead dark elf` binds it directly, and `behead dark elf with longsword`
 * binds it too, because the earlier pattern's %object% walk runs first
 * (run390 checktask, modelled in
 * run_command_match_t::bind_unmatched_object_390()).  So the second
 * restriction asks for the corpse to be carried while the first asks for it to
 * be lying in the Library, and the task answers "You do not have dead dark
 * elf." to every spelling.  `behead drow`, the author's own first command, has
 * no object in it at all and fails the same restriction for the opposite
 * reason -- in run390 with no reference bound it simply fails (451CDD).
 *
 * The weapon test is therefore written the only way the data can say it
 * without naming one weapon out of the game's several: Var1 = 1, "any object",
 * held by the player.  The blade in hand is still required, the body must
 * still be in the Library, and `behead drow` with a sword drawn now scores its
 * twenty.
 *
 * The game's other missing twenty is left alone, because repairing it would
 * not put a point within reach so much as scatter it.  Task 12, "wolf's
 * remains", is the wolf's KilledTask, and the wolf starts hidden; the only
 * thing that places it is task 4, "moving wolf to random room", and the
 * "Baying at the moon" event that howls about wolves on turn 8, 9 or 10 has
 * TaskAffected 0 -- no task at all.  Pointing the event at task 4 would drop
 * the wolf into one unpredictable room out of the twenty the event covers, on
 * one turn, with no restart, and no author's route to it; the howl stays the
 * empty threat the shipped data makes it.  With the beheading, 380 of 400.
 */
static const scr_patch_edit_t PATCH_NIGHTMOON[] = {
  PATCH_VERIFY ("Tasks/17/Command/0", "behead drow"),
  PATCH_VERIFY ("Tasks/17/Restrictions/0/FailMessage",
                "The dark elf's body must be in the library he loved so much."),
  PATCH_VERIFY ("Tasks/17/Restrictions/1/FailMessage",
                "You do not have %object%."),
  PATCH_SET ("Tasks/17/Restrictions/1/Var1", 2, 1)
};

/*
 * Sun Empire: Quest for the Founders (Part I), Daniel Hiebert.
 *
 * The genetic sampler is the game's collection puzzle -- a task per
 * creature, five points each -- and the sample off Skynd, the second of the
 * two Orgaan who storm the laboratory, cannot be taken.  Its task and Skyrv's
 * are the same task written twice, four restrictions apiece: the sampler held,
 * the sample not already taken, the live NPC in the room, and the corpse
 * object in the room.  The last two are never true together, since the corpse
 * is what replaces the NPC.  Skyrv's mask joins its pair with OR --
 * "#A#A(#O#)" -- so either half carries it and the sample can be pressed
 * before the death or after it.  Skynd's says "#A#A(#A#)", and asks for the
 * Orgaan to be alive in the room and dead on the floor at once.  The same
 * expression is evaluated by the Runner (restr_eval_task_restrictions() here,
 * evaluaterestrictions there), so Skynd's five points are out of reach in
 * run400 too, and the game ships with "You finished 5 points short." as its
 * best ending.
 *
 * One character: the inner operator becomes the OR its twin has.  Nothing else
 * about the task moves -- same command, same five points, same refusals when
 * the sampler is elsewhere or the sample already taken -- and `get sample from
 * skynd` now works exactly where `get sample from skyrv` does, in the middle
 * of the firefight the two of them start.  With it, 145 of 145.
 */
static const scr_patch_edit_t PATCH_SUN_EMPIRE[] = {
  PATCH_VERIFY ("Tasks/58/RestrMask", "#A#A(#O#)"),
  PATCH_VERIFY ("Tasks/59/Restrictions/2/FailMessage",
                "Skynd is not here to take a genetic sample from."),
  PATCH_VERIFY ("Tasks/59/Restrictions/3/FailMessage",
                "Skynd is not here to take a genetic sample from."),
  PATCH_STRING ("Tasks/59/RestrMask", "#A#A(#A#)", "#A#A(#O#)")
};

/*
 * Terrified, Eric T. Dorrath.
 *
 * Climbing the fence out of the compound is worth five of the declared
 * sixty-five, and the five are awarded by a task of their own: task 89, no
 * command a player would type ("- score for getting over fence"), no
 * restrictions, one action, ChangeScore 5.  Four tasks cross that fence --
 * with the gloves, the rag, the shirt or the trousers wrapped round your hands
 * -- and each of them ends by executing 89.  Each also moves the player out of
 * room 19 first, and it is the move that is written first.  Task 89's own
 * scope is "one room, 19", so by the time it runs the player is already on the
 * other side of the fence and the task cannot run at all.  Nothing else in the
 * file executes it, and no route reaches the five points.
 *
 * The scope is widened to "all rooms", which is what a scoring task called by
 * another task wants in the first place; its four callers stay the only things
 * that ever run it, and they still only run where the fence is.  The rest of
 * the game is untouched, and the fence crossing pays what the author wrote for
 * it: 65 of 65.
 */
static const scr_patch_edit_t PATCH_TERRIFIED[] = {
  PATCH_VERIFY ("Tasks/89/Command/0", "- score for getting over fence"),
  PATCH_VERIFY ("Tasks/84/CompleteText",
                "The gloves help you to carefully climb over the fence and"
                " move west."),
  PATCH_VERIFY ("Tasks/85/CompleteText",
                "Wrapping the rag around your hands you carefully climb over"
                " the fence and move west."),
  PATCH_SET ("Tasks/89/Where/Type", 1, 3)
};

/*
 * The Twilight, PJK.
 *
 * Cooking the moldy cheese in the kitchen stove is the largest single score in
 * the game, fifteen of five hundred, and the task refuses every time: "You
 * can't do that yet."  Version 3.8 gives a task one object-state restriction,
 * written as an object, a relation and a target, and this one reads "object 57
 * inside container 7".  Container 7 is the gas stove, counting the seven
 * containers in file order, and that is right; object 57, one-based, is the
 * stove as well.  The author picked the holder twice and never named the
 * cheese, which is object 56 and sits immediately before it in the list.
 *
 * A static object is in nothing and held by nobody, so the test cannot pass --
 * in run380 no more than here (parse_fixup_v380_objstate_restr() records what
 * the Runner does with a static named in this field, measured against
 * runner_transcripts/twilight.rtf, where `put cheese in stove` then `cook
 * cheese` answers "You can't do that yet." exactly as the shipped file does
 * here).  The game's own tally says so too: task 113 prints "Your score is 485
 * out of a maximum of 500" on a route that has done everything else.
 *
 * The conversion lands the test on the cheese already -- the preceding dynamic
 * object is what a static's index falls back to -- so the repair is the holder
 * the author meant to leave in place: the container index, zeroed by the
 * fallback, goes back to 7, the stove.  `put cheese in stove` and then `cook
 * cheese`, the two commands the walkthrough already types, now fire the task
 * the CompleteText was written for ("you fire it up, and cook the moldy cheese
 * in the oven"), and The Twilight can be finished on 500 of 500.
 */
static const scr_patch_edit_t PATCH_TWILIGHT[] = {
  PATCH_VERIFY ("Objects/55/Short", "cheese"),
  PATCH_VERIFY ("Objects/56/Short", "stove"),
  PATCH_VERIFY ("Tasks/59/CompleteText",
                "Finding a match in a small tin on top of the stove, you fire"
                " it up, and cook the moldy cheese in the oven.  It turns a"
                " nice brown color."),
  PATCH_VERIFY ("Tasks/59/Restrictions/0/FailMessage",
                "You can't do that yet."),
  PATCH_SET ("Tasks/59/Restrictions/0/Var3", 0, 7)
};

/*
 * House Of Horror, Thomas Mulkerrins.
 *
 * The nine treasures are tallied by nine tasks of their own, tasks 101 to 109,
 * one per treasure, each ten points, each a single restriction: "this treasure
 * is in room 36" -- Home Free!, the drive away that ends the game.  Eight of
 * them say 36.  Task 109, the bag of doubloons, says 0, and a zero there is
 * not a room at all: the object-location restriction reads it as "hidden"
 * (restr_object_in_place(), where var3 == 0 tests OBJ_HIDDEN and never touches
 * the room).  The doubloons do start hidden, and the one thing that places
 * them is the blunderbuss shot that kills the zombie, which drops them onto
 * his corpse.  From that moment they exist, can be carried and can be driven
 * home -- and can never again satisfy task 109.  Not shooting the zombie keeps
 * the ten, at the cost of the twenty his task pays, so the shipped game's
 * ceiling is 145 of 155 either way.
 *
 * The missing room number is restored, and 109 asks what its eight siblings
 * ask.  The zombie is still shot, the doubloons are still picked up off the
 * corpse, and the nine treasures carried out of the house are now worth the
 * ninety the tally was built to pay: 155 of 155.
 */
static const scr_patch_edit_t PATCH_HOUSE_OF_HORROR[] = {
  PATCH_VERIFY ("Tasks/108/Command/0", "8"),
  PATCH_VERIFY ("Tasks/109/Command/0", "9"),
  PATCH_VERIFY ("Objects/11/Short", "doubloons"),
  PATCH_SET ("Tasks/109/Restrictions/0/Var3", 0, 36)
};

/*
 * The Studio, Christopher Cole.
 *
 * Twenty-seven ChangeScore tasks sum to the declared 100, and the four points
 * of the Bedroom shoot's opening act can never be scored.
 *
 * Task 110 is the Bedroom's staging of a scene the Main Studio already has:
 * task 73, the same command pattern written the same way, thirty-seven slots
 * earlier and locked to room 0.  73 is the task that claims the line, and out
 * of its room it answers "You can't do that here!", so the only way 110 ever
 * runs is on the fall-through past a room list that rules 73 out.  110 *is*
 * reached -- what stops it there is its own first restriction, a character test
 * addressing Var1 = 0, "the Player", against Var3 = 0, "the Player": the Player
 * must be in the same room as the Player.  Version 3.90's passrest leaves that
 * exit with the result still zero and without copying the FailMessage
 * ("What?"), so the restriction fails silently, the fall-through lands back
 * on 73's refusal, and the four points are gone
 * (runner_transcripts/studio.txt), so the Runner's ceiling is 96.  Scarier no
 * longer reproduces that exit: a deliberate deviation (self_room_390 in
 * restr_pass_task_char(), screstrs.cpp) passes the restriction as 4.0 does,
 * so 110 runs unpatched too -- but then whether or not Shelby is in the room.
 *
 * Var1 = 2 is Shelby, the first of the game's two characters, and that is what
 * the restriction was meant to say.  Its two immediate neighbours, tasks 111
 * and 112, stage the two acts that follow, and both carry Var1 = 2 against
 * Var3 = 0 behind the identical "What?" -- Shelby must be in the same room as
 * the Player.  Addressed the way 111 and 112 are, 110 runs where the route
 * already stands and the game scores the 100 it declares.
 */
static const scr_patch_edit_t PATCH_STUDIO[] = {
  PATCH_VERIFY ("NPCs/0/Name", "Shelby"),
  PATCH_VERIFY ("Tasks/73/Restrictions/3/FailMessage",
                "You've got plenty of pics of that."),
  PATCH_VERIFY ("Tasks/110/Restrictions/0/FailMessage", "What?"),
  PATCH_VERIFY ("Tasks/111/Restrictions/0/FailMessage", "What?"),
  PATCH_SET ("Tasks/110/Restrictions/0/Var1", 0, 2)
};

/*
 * The Long Journey Home, Danny Chabino.
 *
 * Ten ChangeScore awards sum to the declared 90, and three of them are
 * male/female twins -- tasks 10/11 climbing out of Despaire, 74/75 lighting
 * the torch in Terror, 24/25 releasing the pressure in Rage -- between which
 * the game makes you choose on the first move, in the bedroom, with `male` or
 * `female` (VAR 1 [gender]).  90 is therefore two careers and not one, and the
 * female half of the twin at the bottom of the game cannot be reached at all.
 *
 * Task 24 `#12 release pressure (f)` is that female twin: it wants VAR 1 == 2
 * and task 20 (`mount pipe fitting`) done, it blows the Queen of Diamonds into
 * Rage, and it awards +10.  Two things stop it ever running.  Its Where list
 * is ROOMLIST_NO_ROOMS, which task_where_allows_run() in sctasks.cpp answers
 * with a flat FALSE -- the task is runnable in no room at all -- and its
 * command list holds nothing but the author's own internal label, where the
 * male task 25 carries the four patterns a player would actually type
 * (`* turn * valve *`, `* open * valve *`, `* release * valve *`,
 * `* use * valve *`).  Both are repaired as the male twin is written: Where
 * becomes the one room 9, Rage, which is where task 25 stands and the only
 * room the task's own text can happen in, and the four patterns are copied
 * across.
 *
 * That is still not enough, because the valve is stolen before either twin
 * sees it.  Task 22 `#12 turn valve debris here` is the author's "you haven't
 * cleared the debris yet" refusal, and it carries those same four patterns
 * with NO restrictions at all, three slots lower down, so forward first-match
 * answers every phrasing of the valve with "the valve will not move with the
 * debris in the way" forever -- with the debris cleared and the pipe fittings
 * mounted, and for a male player too.  The author got the same shape right
 * twice in this one room: task 18 (`remove debris` holding the mast) sits
 * before task 19 (the same words bare-handed, which kills you), and task 20
 * (`mount pipe fitting`, gated on the valve being closed) before task 21 (`the
 * heat ... makes it impossible to mount the pipe fittings`).  So task 22 gains
 * the condition its own text states: task state, task 18 must NOT be done,
 * with an empty FailMessage, so that once the debris is gone the line falls
 * through to 23, 24 and 25 exactly as it falls through to 22 before then.
 *
 * With that a female player banks 30 of the 90 and carries the Queen of
 * Diamonds out of the puzzle the author's own hint text describes -- clear the
 * debris, close the valve, mount the fittings, open the valve -- which is the
 * chain task 24 asks for and task 25 does not.  The game stays unwinnable and
 * nothing here pretends otherwise.  Rage's only exit is gated on task 25
 * SPECIFICALLY (`EXIT room=9 N -> dest=7 gateTask=25 wantDone=1`), so she ends
 * in the room where her points are; the two other female twins are broken in
 * their own ways (task 75 drops no card where task 74 drops the King of
 * Spades, and the Gnarled Woods' only way back is gated on task 74); and the
 * card game that would end the game has no command that starts it, for anyone.
 * 30 is what the patched female career is worth, the same as the unpatched
 * one, which reaches Terror instead and comes home with nothing.  The male
 * task 25, unstolen, is worth 10 more to a male player, who can still walk out
 * of Rage.
 *
 * The route also needs the repeat assist, which is not a patch: the Lair's
 * task 3 is a one-shot with a bare `*` pattern, and pre-4.0's spent-task claim
 * (run_spent_task_390()) then answers every later command in that room with
 * "You have already done that.", which is where the faithful row stops at
 * 5/90.  No data edit repairs that -- tasks 4 and 5 claim all twenty direction
 * words on the same terms -- so the walkthrough row carries
 * SCR_ASSUME_REPEATS=1 alongside SCR_ASSUME_PATCHES=1.
 */
static const scr_patch_edit_t PATCH_JOURN2[] = {
  PATCH_VERIFY ("Tasks/22/Command/0", "#12 turn valve debris here"),
  PATCH_VERIFY ("Tasks/24/Command/0", "#12 release pressure (f)"),
  PATCH_VERIFY ("Tasks/25/Command/0", "#12 release pressure"),
  PATCH_VERIFY ("Tasks/25/Command/1", "* turn * valve *"),
  PATCH_VERIFY ("Tasks/25/Command/2", "* open * valve *"),
  PATCH_VERIFY ("Tasks/25/Command/3", "* release * valve *"),
  PATCH_VERIFY ("Tasks/25/Command/4", "* use * valve *"),
  PATCH_ADD_STRING ("Tasks/22/RestrMask", "#"),
  PATCH_ADD ("Tasks/22/Restrictions/0/Type", 2),
  PATCH_ADD ("Tasks/22/Restrictions/0/Var1", 19),
  PATCH_ADD ("Tasks/22/Restrictions/0/Var2", 1),
  PATCH_ADD_STRING ("Tasks/22/Restrictions/0/FailMessage", ""),
  PATCH_SET ("Tasks/24/Where/Type", 0, 1),
  PATCH_ADD ("Tasks/24/Where/Room", 9),
  PATCH_ADD_STRING ("Tasks/24/Command/1", "* turn * valve *"),
  PATCH_ADD_STRING ("Tasks/24/Command/2", "* open * valve *"),
  PATCH_ADD_STRING ("Tasks/24/Command/3", "* release * valve *"),
  PATCH_ADD_STRING ("Tasks/24/Command/4", "* use * valve *")
};

/*
 * Provenance (Corey W Arnett).  MaxScore is 300 and the most any route can
 * bank is 285: two scoring tasks can never run, each for want of one field.
 *
 * The crow on the scarecrow.  Task 141 (`x crow`, holding the skeleton key)
 * has the crow pluck the key out of your hand and swallow it, and moves the
 * key to hidden.  Task 142 (`give strawberry to crow`, +10) then wants
 * task 141 done -- and the key *held by the player*, restriction 1, Var2 1.
 * Only task 143 (`catch crow`) brings the key back up, and task 143 wants
 * task 142 done, so the three form a loop that can never be entered.  Every
 * other clause of task 142 is written for the crow that has already eaten
 * the key: its two refusals are the same scene word for word ("the crow shows
 * no interest in the strawberry ... more and more agitated"), and its text
 * has the bird jump down for the berry, which is what lets task 143 frighten
 * the key back out of it.  So the key restriction was meant the other way
 * round: Var2 7, not held by the player, which task 141 has already made
 * true, and the chain runs strawberry, catch, key.
 *
 * The crystal ball.  Rubbing it plays a vision chosen by room: task 502 in
 * room 174, task 503 in room 172, task 505 (+5) in room 7, and task 504, the
 * repeatable "Clouds swirl up inside but moments later they dissipate", as
 * the fallback everywhere else.  But 504 was given every room, room 7
 * included, and sits one slot above 505, so first-match answers the rub in
 * room 7 with the fallback forever.  502 and 503 work only because they come
 * before it.  Task 504 loses room 7, the one room its own later twin claims.
 */
static const scr_patch_edit_t PATCH_PROVENANCE[] = {
  PATCH_VERIFY ("Tasks/141/Command/0", "* x * crow *"),
  PATCH_VERIFY ("Tasks/142/Command/0", "* give * crow * strawberry *"),
  PATCH_VERIFY ("Tasks/143/Command/0", "* catch * crow *"),
  PATCH_SET ("Tasks/142/Restrictions/1/Var2", 1, 7),
  PATCH_VERIFY ("Tasks/504/Command/0", "* rub * crystal ball *"),
  PATCH_VERIFY ("Tasks/505/Command/0", "* rub * crystal ball *"),
  PATCH_VERIFY ("Objects/2/Short", "key"),
  PATCH_SET_BOOL ("Tasks/504/Where/Rooms/7", 1, 0)
};

/*
 * Outside (Matt Ingrey), a 3.90 game whose own guide admits it went out
 * untested.  Its last act is three rooms -- Roses House, Phureo, Alexes
 * House and its back room -- and each of the three doors between them is a
 * task that forgets to open:
 *
 *   o Roses House.  Task 79, `se` / `go* phu*`, is the only way out.  It
 *     prints "You have 30 turns before you will be recaptured." and starts
 *     the countdown, but has no actions, so the player stays put; it is not
 *     repeatable, and 3.9 answers every later attempt with "You have already
 *     done that."  The added action is the move to Phureo the text stands
 *     for.
 *
 *   o Phureo.  Tasks 72 and 73 both answer `ne` / `go* alex*`.  72, "The
 *     door to Alexes house is locked.", has no restrictions and comes first;
 *     73, which walks the player in once `*unlock*` (task 74) is done, can
 *     therefore never run, and its own refusal for the locked case is empty.
 *     72's line becomes 73's refusal, and 72 is given no room to run in.
 *
 *   o Alexes House.  Task 69, `go * back room*`, prints "You wander into the
 *     room", turns Alex hostile and starts his walk to the back room, where
 *     task 71 ends the game when he dies -- but it leaves the player behind,
 *     in a room where task 70, `*`, answers everything else "Incorrect."
 *     once the control panel is on.  A second action moves the player to the
 *     back room, room 19.
 *
 * And the fight there cannot be won either: task 71,
 * `thatbadoldalexgotkilled`, is the file's only winning EndGame and is
 * spelled like the KilledTask of every other fighter in the game
 * (`thefirstprisonguardisdead`, `youkilledthemonsterthingy`), but Alex, NPC
 * 12, has no KilledTask at all, so he would die and nothing would follow.
 * He is given it.  The guide's last line is "***defeat alex***"; with the
 * road monster fought rather than fled, the sword bought and the chain mail
 * worn, that takes nine blows.
 *
 * The three doors are the Runner's behaviour too
 * (runner_transcripts/outside.txt and the run390 probes of `goto phureo` /
 * `goto alexes house` / `goto back room`).
 */
static const scr_patch_edit_t PATCH_OUTSIDE[] = {
  PATCH_VERIFY ("Tasks/79/Command/0", "se"),
  PATCH_VERIFY ("Tasks/79/CompleteText",
                "You have 30 turns before you will be recaptured."),
  PATCH_ADD ("Tasks/79/Actions/0/Type", 1),
  PATCH_ADD ("Tasks/79/Actions/0/Var1", 0),
  PATCH_ADD ("Tasks/79/Actions/0/Var2", 0),
  PATCH_ADD ("Tasks/79/Actions/0/Var3", 16),
  PATCH_VERIFY ("Tasks/72/Command/0", "ne"),
  PATCH_VERIFY ("Tasks/72/CompleteText",
                "The door to Alexes house is locked."),
  PATCH_VERIFY ("Tasks/73/Command/0", "ne"),
  PATCH_STRING ("Tasks/73/Restrictions/0/FailMessage", "",
                "The door to Alexes house is locked."),
  PATCH_SET ("Tasks/72/Where/Type", 1, 0),
  PATCH_VERIFY ("Tasks/69/Command/0", "go * back room*"),
  PATCH_ADD ("Tasks/69/Actions/1/Type", 1),
  PATCH_ADD ("Tasks/69/Actions/1/Var1", 0),
  PATCH_ADD ("Tasks/69/Actions/1/Var2", 0),
  PATCH_ADD ("Tasks/69/Actions/1/Var3", 19),
  PATCH_VERIFY ("Tasks/71/Command/0", "thatbadoldalexgotkilled"),
  PATCH_VERIFY ("NPCs/12/Name", "Alex"),
  PATCH_SET ("NPCs/12/Battle/KilledTask", 0, 72)
};

/*
 * Oh No, Not Another Fantasy Adventure (David Whyld).  Asking the cook for
 * food makes him throw biscuits, cookies, a loaf and a "little jam cake" on
 * the Kitchen floor, and Mother Mabel trades her rose for the cake.  But the
 * Kitchen also has task 325, `get *jam*` / `get *tarts*`, a scenery refusal
 * for the jam tarts lying on the cook's helper, and the Runner matches tasks
 * against the command with the object's full name in it: every way of asking
 * for the cake reads "get little jam cake" and is answered "the cook's
 * helper ... slobbers all over them".  `get all` takes everything but the
 * cake.  The 4.0 Runner does the same (runner_transcripts, the Delron
 * route's `get cake`), so the cake, the rose and what the rose buys can't be
 * had.  The refusal is narrowed to the tarts it describes.
 */
static const scr_patch_edit_t PATCH_ONNAFA[] = {
  PATCH_VERIFY ("Tasks/325/Command/1", "get *tarts*"),
  PATCH_STRING ("Tasks/325/Command/0", "get *jam*", "get *jam tart*")
};

/*
 * Cowboy Blues (David Whyld).  Variable 0 is the difficulty: 0 hard, 1
 * medium, 2 easy.  Each item the game splits by setting gives the easier
 * settings one task and hard another -- the marble, task 375 (`> 0`) against
 * tasks 376-378 (`== 0`) -- except the battery.  Task 365, the easy
 * `x house` that hands it over at once, asks `difficulty > 1`, while hard's
 * look-twice pair 366/367 asks `== 0`, so on medium no battery task matches,
 * `x house` falls through to task 369, and without the battery there is no
 * map and no way past the cave.  Task 365 asks `> 0`, as task 375 does.
 */
static const scr_patch_edit_t PATCH_COWBOY_BLUES[] = {
  PATCH_VERIFY ("Tasks/365/Command/0", "x *house*"),
  PATCH_VERIFY ("Tasks/366/Command/0", "x *house*"),
  PATCH_SET ("Tasks/365/Restrictions/0/Var3", 1, 0)
};

typedef struct
{
  const scr_char *name;            /* Globals/GameName */
  const scr_char *author;          /* Globals/GameAuthor */
  const scr_char *summary;         /* One line, for a host to show. */
  const scr_patch_edit_t *edits;
  scr_int edit_count;
} scr_patch_game_t;

#define PATCH_GAME(name, author, summary, edits) \
  { name, author, summary, edits, sizeof (edits) / sizeof (edits[0]) }

static const scr_patch_game_t PATCH_TABLE[] = {
  PATCH_GAME ("ebonys world", "laurie griffiths",
              "the valve commands now set the valve, not the lever",
              PATCH_EBONYS_WORLD),
  PATCH_GAME ("BEDLAM", "Wild N Mild",
              "asking Barbara about the keys now hands them over",
              PATCH_BEDLAM),
  PATCH_GAME ("MysteryHouse", "KeKe Studio",
              "opening the chest now actually opens it",
              PATCH_MYSTERY_HOUSE),
  PATCH_GAME ("Illeagal Socks", "Trace",
              "the villain's name is now one the parser can match",
              PATCH_ILLEGAL_SOCKS),
  PATCH_GAME ("Sandy's Lost Doll<cls>", "Sandy",
              "the last look in the toilet now tests the right thing",
              PATCH_SANDY),
  PATCH_GAME ("The Hangover", "Red Conine",
              "the doctor and the platypus now have rooms to be given things in",
              PATCH_HANGOVER),
  PATCH_GAME ("<font size=1>Blood Relatives", "davidw",
              "the desk task now uses the word the game's own synonym leaves",
              PATCH_BLOOD_RELATIVES),
  PATCH_GAME ("The Spirit's Flight", "Andy Ko & Karen Kwan",
              "the water guardian now leaves her artifact, the chant has a"
              " circle and a sayable command, and Carnifern can be fought",
              PATCH_SPIRITS_FLIGHT),
  PATCH_GAME ("Tenebrae Semper", "Seciden Menarde",
              "the stairwell door opens onto the forest path, and the walk"
              " back across campus arrives",
              PATCH_TENEBRAE_SEMPER),
  PATCH_GAME ("The Annihilation of Think.com 3", "Pofty",
              "choice B now leads to the room the last choice is asked in",
              PATCH_TAOT3),
  PATCH_GAME ("<centre>Crazy Old Bag Lady</centre>", "Sprite",
              "the newsagent's counter stands in the Corner Shop, so the"
              " newspapers left on it can be picked up",
              PATCH_COBL),
  PATCH_GAME ("DeL SoL MADNESS", "Alvin Echeverria",
              "the note answer scores, Hina joins the nightmare, and the"
              " nightmare Moreland carries the win",
              PATCH_DEL_SOL),
  PATCH_GAME ("'!NameOfGame!'", "Matt, Dark Baron",
              "the cliff rope can be untied again, and killing Hamish runs"
              " the smuggler's death task that opens the hair shop",
              PATCH_LIQID),
  PATCH_GAME ("The Vampire with a Conscience", "Ole Olsen",
              "the way east out of Bozo's backyard can be taken again, as its"
              " own \"You enter the nightclub again.\" promises",
              PATCH_VAMPIRE),
  PATCH_GAME ("The Merry Murders", "Mel S.",
              "unlocking the Archives door no longer eats the `n` that walks"
              " through it",
              PATCH_MERRY_MURDERS),
  PATCH_GAME ("Fun Town", "Mandrake",
              "the treasure chest asks for the episode that scores rather"
              " than its twin that kills you",
              PATCH_FUNTOWN),
  PATCH_GAME ("\"Sommeril\" by H. Lee Parten<br>", "H. Lee Parten",
              "taking the wet page looks for it in the fountain the fish"
              " dropped it in, rather than in the fish's hands",
              PATCH_SOMMERIL),
  PATCH_GAME ("<font size=+5><center><i><b><u>Locked Out</u></b></i></center>",
              "Graeme Allen (www.postbrickfilms.com/littlebrick)",
              "weighting the trash can lid down with the rock is allowed in"
              " the alley it can be done in",
              PATCH_LOCKEDOUT),
  PATCH_GAME ("Villains and Kings", "Neal, the GREAT",
              "taking the soap out of the broken window is allowed in the"
              " room the window is in",
              PATCH_VILLAINS_AND_KINGS),
  PATCH_GAME ("FunHouse", "C. A. Gist",
              "the third hundred-dollar task can be typed on the midway, so"
              " the declared 410 can be scored",
              PATCH_FUNHOUSE),
  PATCH_GAME ("<b>Goldilocks - Breaking & Entering<b>", "L.C.N.",
              "taking the egg looks at the refrigerator it is in, and tying"
              " the rope to the window bars scores the point its dead twin"
              " carries",
              PATCH_GOLDILOCKS_BE),
  PATCH_GAME ("Melbourne Beach", "David Good",
              "turning on the drier is spelled the way the game's own"
              " synonyms rewrite it, and dries the clothes the wash left",
              PATCH_MELBOURNE_BEACH),
  PATCH_GAME ("\xcc\xe5\xf1\xf2\xee \xef\xf0\xe5\xf1\xf2\xf3\xef\xeb\xe5\xed\xe8\xff 2",
              "Sirius",
              "the evidence, the safe and the drives to the lab and the"
              " office are spelled the way the game's own synonyms rewrite"
              " them",
              PATCH_CRIME_SCENE_2),
  PATCH_GAME ("The Fugitive", "arctica0@hotmail.com",
              "looking in a car's mirror is spelled the way the game's own"
              " synonyms rewrite it",
              PATCH_FUGITIVE),
  PATCH_GAME ("Professor Von Witt's Fabulous Flying Machine", "Mystery",
              "pressing the green button while holding the bottle cap now"
              " flies the machine, instead of the ending written for pressing"
              " it without",
              PATCH_PROFESSOR),
  PATCH_GAME ("                   The Adventures of Space Boy! Volume I",
              "David Parish (c) 2005",
              "taking the ice gloves now scores, the task's alternation"
              " written with a slash instead of a backslash",
              PATCH_SPACE_BOY),
  PATCH_GAME ("The Night The Moon Shone Grey", "Matthew Moya",
              "beheading the dark elf asks for a blade in hand, not for his"
              " body to be carried and lying in the Library at once",
              PATCH_NIGHTMOON),
  PATCH_GAME ("Sun Empire: Quest for the Founders (Part I) (C) Copyright 2003"
              " - All Rights Reserved", "Daniel Hiebert",
              "sampling Skynd asks for the Orgaan alive or dead, as sampling"
              " Skyrv does, not both at once",
              PATCH_SUN_EMPIRE),
  PATCH_GAME ("Terrified", "Eric T. Dorrath",
              "the score for climbing the fence is awarded in the room the"
              " crossing leaves you in",
              PATCH_TERRIFIED),
  PATCH_GAME ("The Twilight", "PJK",
              "cooking the cheese asks for the cheese to be in the stove,"
              " rather than the stove to be inside itself",
              PATCH_TWILIGHT),
  PATCH_GAME ("House Of Horror", "Thomas Mulkerrins",
              "the bag of doubloons is counted in the room it is carried to,"
              " like the other eight treasures",
              PATCH_HOUSE_OF_HORROR),
  PATCH_GAME ("The Studio", "Christopher Cole",
              "the Bedroom shoot's opening act asks for Shelby to be in the"
              " room, as the two acts after it do, not for the Player to be"
              " where the Player is",
              PATCH_STUDIO),
  PATCH_GAME ("The Long Journey Home", "Danny Chabino",
              "the female twin of the valve in Rage is runnable nowhere and"
              " has no command a player could type, and the debris refusal"
              " above it steals every phrasing of the valve from both twins",
              PATCH_JOURN2),
  PATCH_GAME ("PROVENANCE", "Corey W Arnett",
              "the strawberry is offered to the crow that has swallowed the"
              " key, and the crystal ball's vision in room 7 is no longer"
              " answered by the fallback rub",
              PATCH_PROVENANCE),
  PATCH_GAME ("<font size=1>Cowboy Blues", "David Whyld",
              "the battery at the house is handed over on the medium setting"
              " as well as easy, as the marble is, rather than on no setting"
              " between easy and hard",
              PATCH_COWBOY_BLUES),
  PATCH_GAME ("Outside", "Matt Ingrey",
              "leaving Roses house, entering Alexes house once it is"
              " unlocked, and going into its back room now move you there",
              PATCH_OUTSIDE),
  PATCH_GAME ("<font size=1>ONNAFA", "David Whyld",
              "the little jam cake can be picked up, the refusal for the jam"
              " tarts no longer answering for it",
              PATCH_ONNAFA)
};
enum { PATCH_TABLE_SIZE = sizeof (PATCH_TABLE) / sizeof (PATCH_TABLE[0]) };

/* Patching setting, and a note of the patch applied by the last parse. */
static scr_bool parse_patches_enabled = FALSE;
static const scr_patch_game_t *parse_applied_patch = nullptr;


/*
 * parse_patch_edit()
 *
 * Check, and on demand carry out, one table edit.  Returns TRUE if the bundle
 * holds what the edit expects to find.  Called once over a game's edits to
 * check them all, and only then, if every one agreed, a second time to apply.
 */
static scr_bool
parse_patch_edit (scr_prop_setref_t bundle,
                  const scr_patch_edit_t *edit, scr_bool apply)
{
  scr_char buffer[PATCH_PATH_LENGTH];
  scr_char format_get[PATCH_MAX_KEYS + 4], format_put[PATCH_MAX_KEYS + 4];
  scr_vartype_t vt_key[PATCH_MAX_KEYS], vt_value, vt_rvalue;
  const scr_char *cursor;
  scr_int count;
  scr_bool is_string;

  if (strlen (edit->path) >= sizeof (buffer))
    scr_fatal ("parse_patch_edit: path too long, %s\n", edit->path);
  memcpy (buffer, edit->path, strlen (edit->path) + 1);

  /*
   * Split the path on '/' into key elements, in place.  An element of all
   * digits is an integer key, anything else a string key; both kinds point
   * into buffer, which outlives the prop calls below.
   */
  count = 0;
  for (cursor = strtok (buffer, "/"); cursor; cursor = strtok (nullptr, "/"))
    {
      if (count == PATCH_MAX_KEYS)
        scr_fatal ("parse_patch_edit: path too deep, %s\n", edit->path);

      if (strspn (cursor, "0123456789") == strlen (cursor))
        {
          vt_key[count].integer = atol (cursor);
          format_get[count + 3] = PROP_KEY_INTEGER;
        }
      else
        {
          vt_key[count].string = cursor;
          format_get[count + 3] = PROP_KEY_STRING;
        }
      count++;
    }
  format_get[count + 3] = NUL;

  is_string = (edit->mode == 'S' || edit->mode == 'V' || edit->mode == 'N');
  format_get[0] = is_string ? PROP_STRING
                  : edit->mode == 'B' ? PROP_BOOLEAN : PROP_INTEGER;
  format_get[1] = '<';
  format_get[2] = '-';
  snprintf (format_put, sizeof (format_put), "%s", format_get);
  format_put[1] = '-';
  format_put[2] = '>';

  switch (edit->mode)
    {
    case 'I':
      if (!prop_get (bundle, format_get, &vt_rvalue, vt_key)
          || vt_rvalue.integer != edit->from_integer)
        return FALSE;
      if (apply)
        {
          vt_value.integer = edit->to_integer;
          prop_put (bundle, format_put, vt_value, vt_key);
        }
      return TRUE;

    case 'B':
      if (!prop_get (bundle, format_get, &vt_rvalue, vt_key)
          || !vt_rvalue.boolean != !edit->from_integer)
        return FALSE;
      if (apply)
        {
          vt_value.boolean = edit->to_integer != 0;
          prop_put (bundle, format_put, vt_value, vt_key);
        }
      return TRUE;

    case 'A':
      if (prop_get (bundle, format_get, &vt_rvalue, vt_key))
        return FALSE;
      if (apply)
        {
          vt_value.integer = edit->to_integer;
          prop_put (bundle, format_put, vt_value, vt_key);
        }
      return TRUE;

    case 'N':
      if (prop_get (bundle, format_get, &vt_rvalue, vt_key))
        return FALSE;
      if (apply)
        {
          vt_value.string = edit->to_string;
          prop_put (bundle, format_put, vt_value, vt_key);
        }
      return TRUE;

    case 'S':
    case 'V':
      if (!prop_get (bundle, format_get, &vt_rvalue, vt_key)
          || !vt_rvalue.string
          || strcmp (vt_rvalue.string, edit->from_string) != 0)
        return FALSE;
      if (apply && edit->mode == 'S')
        {
          /* String table values are literals, so putting the pointer is
             enough; the bundle never frees what it didn't allocate. */
          vt_value.string = edit->to_string;
          prop_put (bundle, format_put, vt_value, vt_key);
        }
      return TRUE;

    default:
      scr_fatal ("parse_patch_edit: unknown mode '%c'\n", edit->mode);
    }

  return FALSE;
}


/*
 * parse_apply_game_patches()
 *
 * Look the freshly parsed game up in the patch table, and if it is there and
 * still holds every value its entry expects, apply that entry's edits.
 */
void
parse_apply_game_patches (scr_prop_setref_t bundle)
{
  const scr_char *name, *author;
  scr_int game, edit;

  parse_applied_patch = nullptr;
  if (!parse_patches_enabled)
    return;

  name = prop_get_global_string (bundle, "GameName");
  author = prop_get_global_string (bundle, "GameAuthor");
  if (!name || !author)
    return;

  for (game = 0; game < PATCH_TABLE_SIZE; game++)
    {
      const scr_patch_game_t *const entry = PATCH_TABLE + game;

      if (strcmp (name, entry->name) != 0
          || strcmp (author, entry->author) != 0)
        continue;

      /* Check every edit before making any of them. */
      for (edit = 0; edit < entry->edit_count; edit++)
        {
          if (!parse_patch_edit (bundle, entry->edits + edit, FALSE))
            {
              if (parse_trace)
                {
                  scr_trace ("Parse: patch for \"%s\" skipped, %s differs\n",
                             entry->name, entry->edits[edit].path);
                }
              return;
            }
        }

      for (edit = 0; edit < entry->edit_count; edit++)
        parse_patch_edit (bundle, entry->edits + edit, TRUE);

      if (parse_trace)
        scr_trace ("Parse: patched \"%s\": %s\n", entry->name, entry->summary);

      parse_applied_patch = entry;
      return;
    }
}


/*
 * parse_set_game_patches()
 * parse_get_game_patches()
 * parse_get_applied_patch()
 *
 * Turn the patch table on or off, and report what the last parse did with it.
 * The applied-patch note is cleared by every parse, so it always describes
 * the game most recently loaded.
 */
void
parse_set_game_patches (scr_bool flag)
{
  parse_patches_enabled = flag;
}

scr_bool
parse_get_game_patches (void)
{
  return parse_patches_enabled;
}

const scr_char *
parse_get_applied_patch (void)
{
  return parse_applied_patch ? parse_applied_patch->summary : nullptr;
}
