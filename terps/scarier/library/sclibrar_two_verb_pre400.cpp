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
 */

/*
 * A line naming two library verbs below 4.0 (`x get coin`, `take off hat`,
 * `drop take coin`): the five pre-4.0 handlers -- takes, drops, wears,
 * removes, examines -- each enter on a whole-word test over the whole line,
 * so the Runner's fixed handler order, not the word order, decides which
 * one speaks.  lib_two_verb_line_pre400() re-spells such a line for the
 * handler that would, and lib_earlier_handler_claims_pre400() tells the
 * runner tables when an earlier handler claims a line.  Both are called from
 * runner/; the drop, wear and remove filters they test with are in
 * sclibrar_drop.cpp.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"


/*
 * lib_two_verb_line_pre400()
 *
 * A line naming TWO library verbs, below 4.0.  Every one of the five
 * handlers Scarier anchors enters on a whole-word c() test over the WHOLE
 * line, so `x get coin` satisfies takes() and examines() alike, and
 * generaltasks calls them in ONE fixed order (run380 from 4421A3):
 *
 *     takes, drops, inventory, insides, tasks, wears, removes, ... therest
 *
 * A handler that ACTS claims the line -- `If CBool(takes()) Then GoTo` the
 * turn tail -- while one that only writes a refusal does not, and the next
 * handler runs on and may overwrite it.  So WORD ORDER NEVER DECIDES below
 * 4.0: `take drop coin` and `drop take coin` both answer from the same
 * order, and which of them speaks depends only on where the object is.
 *
 * Measured on p3xREW (runner_probes/rew.run3*.verb*), and on the wearable
 * probe p3xTWO (make_twoverbprobe.py; runner_probes/two.run3*.verb3.*).
 * The cells that carry the rule:
 *
 *   coin loose   `x get coin`     "You pick up the coin."   takes acts
 *   coin held    `get x coin`     "A gold coin."            takes cannot,
 *                                                           examines does
 *   coin held    `wear take coin` "You've already got the coin!"
 *   coin loose   `remove drop coin` "You don't have the coin!"
 *   coin loose   `remove wear coin` "You are not holding the coin."
 *   hat loose    `remove take hat` "You pick up the hat."    takes acts
 *   hat worn     `take remove hat` "You remove the hat."     takes cannot,
 *                                                            removes does
 *   hat worn     `drop take hat`  "You drop the hat."        drops takes a
 *                                                            worn object
 *   hat worn     `x take hat`     "A felt hat."
 *   hat held     `x take off hat` "A felt hat."
 *   hat held     `take off hat`   "You've already got the hat!"  (3.7;
 *                                 3.8 rewrites take->get first, so
 *                                 removes never enters there either)
 *   hat worn     `take off hat`   "You remove the hat."      (3.7/3.9)
 *   nothing      `drop take`      "Take what?"
 *   nothing      `examine wear`   "Wear what?"
 *
 * The last two are why examines is not simply last: with an object named it
 * overwrites whatever refusal the four above it left (it is down in therest,
 * which runs below everything), but with NOTHING named its "Nothing
 * special." only ever fills an EMPTY buffer, so the earlier handler's
 * question survives.
 *
 * Scarier answers a line from one anchored row, so the port is a re-spelling
 * like run_hoist_verb_line()'s: work out which handler the Runner's order
 * would leave speaking, and hand the row that handler's own line.  Narrow on
 * purpose -- a list line ("all", "and"), a static object and a name two
 * objects answer to are all left exactly as they were, the first two because
 * their arms walk co() themselves and the last because the disambiguation
 * the Runner's namesake count asks for is not this rule's business.
 */
enum
{
  LIB_PRE400_TAKE, LIB_PRE400_DROP, LIB_PRE400_WEAR,
  LIB_PRE400_REMOVE, LIB_PRE400_EXAMINE, LIB_PRE400_HANDLERS
};

/*
 * Every word the five entry tests hold, longest phrases first, so that
 * "take off" is stripped whole where "take" alone would leave "off".
 */
static const scr_char *const LIB_PRE400_VERB_WORDS[] = {
  "put down", "put on", "take off", "look at", "look in", "put",
  "get", "take", "pick", "drop", "leave", "wear", "remove",
  "examine", "exam", "read", "ex", "x", nullptr
};

static const scr_char *const LIB_PRE400_CANONICAL[LIB_PRE400_HANDLERS] = {
  "take", "drop", "wear", "remove", "examine"
};

/*
 * run370 adds the game's own word for each handler from its command block,
 * c(MemVar_4460FC(N)) beside its own spellings: examines 434E2A slot 10,
 * takes 435E28 slot 11, drops 430475 slot 12, wears 42C533 slot 13,
 * removes 4295FF slot 14.  The word stays in the line -- run370 has no
 * synonyms, so a task hears `grab blip` as typed -- and it is only one
 * more entry word: take still outranks examine on `peer grab bag`, and the
 * word can be half an object's name (`x grab bag` picks the bag up).  The
 * take slot's test is `c(slot) And Not c("from")`, the And binding before
 * the Or, so `grab coin from box` never enters takes while `take coin from
 * box` does.  p37SLOT (harness/make_37_slotprobe.py;
 * runner_probes/slot.run370.*).  The slot words, by
 * handler, empty where the game kept the standard word (which the own
 * spellings already hold) or below 3.7's command block.
 */
static const scr_int LIB_PRE400_SLOT[LIB_PRE400_HANDLERS] = {
  11, 12, 13, 14, 10
};
static const scr_char *const LIB_PRE400_SLOT_DEFAULT[LIB_PRE400_HANDLERS] = {
  "pick up", "put down", "wear", "remove", "examine"
};

static void
lib_pre400_slot_words (scr_gameref_t game,
                       std::string words[LIB_PRE400_HANDLERS])
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int handler;

  for (handler = 0; handler < LIB_PRE400_HANDLERS; handler++)
    {
      words[handler] = lib_command_slot_370 (bundle, LIB_PRE400_SLOT[handler]);
      if (words[handler] == LIB_PRE400_SLOT_DEFAULT[handler])
        words[handler].clear ();
    }
}

/*
 * The five entry tests themselves, as each handler's own c() calls spell
 * them, with 3.7's slot words above.  Counts the handlers the
 * line would enter; SLOT_LINE, if given, says whether a slot word is one of
 * the reasons.
 */
static scr_int
lib_pre400_handler_words_in (scr_gameref_t game, const scr_char *line,
                             scr_bool present[LIB_PRE400_HANDLERS],
                             scr_bool *slot_line)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  std::string slots[LIB_PRE400_HANDLERS];
  scr_int handler, count;

  /* The Runner's c(): whether the line contains WORD. */
  const auto c_word = [&] (const scr_char *word) -> scr_bool
    {
      return run_c_word_pre400 (version, line, word) >= 0;
    };

  present[LIB_PRE400_TAKE] = c_word ("get") || c_word ("take")
                             || (c_word ("pick") && !c_word ("from"));
  present[LIB_PRE400_DROP] = c_word ("drop") || c_word ("put down")
                             || c_word ("leave")
                             || (c_word ("put") && c_word ("down"));
  present[LIB_PRE400_WEAR] = c_word ("wear") || c_word ("put on");
  present[LIB_PRE400_REMOVE] = c_word ("remove") || c_word ("take off");
  present[LIB_PRE400_EXAMINE] = c_word ("x") || c_word ("examine")
                                || c_word ("look at")
                                || c_word ("ex") || c_word ("exam")
                                || c_word ("read")
                                || (version >= TAF_VERSION_380
                                    && c_word ("look in"));

  if (slot_line)
    *slot_line = FALSE;
  lib_pre400_slot_words (game, slots);
  for (handler = 0; handler < LIB_PRE400_HANDLERS; handler++)
    {
      if (slots[handler].empty () || !c_word (slots[handler].c_str ())
          || (handler == LIB_PRE400_TAKE && c_word ("from")))
        continue;
      present[handler] = TRUE;
      if (slot_line)
        *slot_line = TRUE;
    }

  count = 0;
  for (handler = 0; handler < LIB_PRE400_HANDLERS; handler++)
    count += present[handler] ? 1 : 0;
  return count;
}

static scr_int
lib_pre400_handler_words (scr_gameref_t game, const scr_char *line,
                          scr_bool present[LIB_PRE400_HANDLERS],
                          scr_bool *slot_line = nullptr)
{
  /*
   * Deliberate deviation: a handler word inside a longer object or character
   * name enters no handler; see lib_mask_long_names().
   */
  const std::string masked = lib_mask_long_names (game, line);

  if (SCR_TRACING_DEVIATIONS () && masked != line)
    {
      scr_bool runner[LIB_PRE400_HANDLERS] = { FALSE };
      scr_bool ours[LIB_PRE400_HANDLERS] = { FALSE };

      lib_pre400_handler_words_in (game, line, runner, nullptr);
      lib_pre400_handler_words_in (game, masked.c_str (), ours, nullptr);
      if (memcmp (runner, ours, sizeof runner) != 0)
        SCR_DEVIATION ("mask_handlers", "masked=\"%s\"", masked.c_str ());
    }
  return lib_pre400_handler_words_in (game, masked.c_str (), present,
                                      slot_line);
}


/*
 * lib_earlier_handler_claims_pre400()
 *
 * TRUE when the line names one of the five handlers generaltasks calls
 * ahead of therest(), so therest's verb cascade never gets to speak for it.
 * run370's therest already knew this -- its absent-object refusal skips a
 * line holding any earlier handler's word (run_therest_absent_370) -- and
 * the verb arms below it are the same story at every pre-4.0 version: with
 * the coin in hand `push take coin` is "You've already got a coin!" and
 * `push drop coin` (coin on the floor) "You don't have a coin!", not the
 * push arm's "but nothing happens", and `push examine coin` is the coin's
 * description at 3.7, 3.8 AND 3.9 -- where Scarier answered 3.9's own therest
 * examine arm, "Nothing special.".  p3xREW cells 31, 35 and 39
 * (runner_probes/rew.run3*.verb.*).
 */
scr_bool
lib_earlier_handler_claims_pre400 (scr_gameref_t game, const scr_char *line)
{
  scr_bool present[LIB_PRE400_HANDLERS];

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400
      || !line || line[0] == NUL)
    return FALSE;

  return lib_pre400_handler_words (game, line, present) > 0;
}


/*
 * lib_two_verb_rewrite_pre400()
 *
 * The tail of lib_two_verb_line_pre400(): re-spell LINE as HANDLER's canonical
 * verb followed by what is left of the line, FOUND being the one object the
 * line names, or -1.
 */
static scr_bool
lib_two_verb_rewrite_pre400 (scr_gameref_t game, const scr_char *line,
                             scr_int handler, scr_int found,
                             scr_bool bare_put, const std::string *slots,
                             std::string *rewritten)
{
  scr_int handler_word;
  std::string shortname;
  const scr_char *scan;
  std::string rest;

  if (found != -1)
    {
      const scr_char *const name = prop_get_indexed_string (
          gs_get_bundle (game), "Objects", found, "Short");

      shortname = name ? name : "";
    }
  /*
   * The line with every verb word taken out of it, which is what the
   * winning handler's own object walk would have been left looking at.
   */
  for (scan = line; *scan != NUL; )
    {
      const scr_char *const *word;
      scr_int matched = 0;

      if (scan == line || scan[-1] == ' ')
        {
          for (word = LIB_PRE400_VERB_WORDS; *word && !matched; word++)
            {
              const scr_int length = strlen (*word);

              if (!bare_put && strcmp (*word, "put") == 0)
                continue;
              if (scr_strncasecmp (scan, *word, length) == 0
                  && (scan[length] == NUL || scan[length] == ' '))
                matched = length;
            }
          /*
           * A slot word goes too, unless it begins the object's own name:
           * `x grab bag` is a take of the grab bag.
           */
          for (handler_word = 0;
               handler_word < LIB_PRE400_HANDLERS && !matched; handler_word++)
            {
              const std::string &slot = slots[handler_word];

              if (!slot.empty ()
                  && scr_strncasecmp (scan, slot.c_str (), slot.size ()) == 0
                  && (scan[slot.size ()] == NUL || scan[slot.size ()] == ' ')
                  && !(shortname.size () > 0
                       && scr_strncasecmp (scan, shortname.c_str (),
                                           shortname.size ()) == 0
                       && (scan[shortname.size ()] == NUL
                           || scan[shortname.size ()] == ' ')))
                matched = slot.size ();
            }
        }
      if (matched > 0)
        {
          scan += matched;
          scan += strspn (scan, " ");
          continue;
        }
      rest.push_back (*scan++);
    }

  *rewritten = LIB_PRE400_CANONICAL[handler];
  if (!rest.empty ())
    {
      *rewritten += " ";
      *rewritten += rest;
    }
  return TRUE;
}

scr_bool
lib_two_verb_line_pre400 (scr_gameref_t game, const scr_char *line,
                          std::string *rewritten)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  scr_bool present[LIB_PRE400_HANDLERS], acts[LIB_PRE400_HANDLERS];
  scr_int handler, object, found, count;
  scr_bool bare_put = FALSE, slot_line;
  std::string slots[LIB_PRE400_HANDLERS];

  /* The Runner's c(): whether the line contains WORD. */
  const auto c_word = [&] (const scr_char *word) -> scr_bool
    {
      return run_c_word_pre400 (version, line, word) >= 0;
    };
  /* Hand the line to HANDLER, FOUND being the one object it names if any. */
  const auto rewrite = [&] () -> scr_bool
    {
      return lib_two_verb_rewrite_pre400 (game, line, handler, found,
                                          bare_put, slots, rewritten);
    };

  if (version >= TAF_VERSION_400 || !line || line[0] == NUL)
    return FALSE;

  count = lib_pre400_handler_words (game, line, present, &slot_line);
  lib_pre400_slot_words (game, slots);
  /*
   * A put with no container clause is no handler's: run370/380/390's put
   * row wants its in/on, and the bare verb falls through to the catch-all.
   * So beside one of the five it is only a word, and the handler answers:
   * `put x coin` is "A gold coin.", `put take coin` with the coin held
   * "You've already got a coin!", `put drop coin` "You drop the coin.".
   * Cells 56, 69 and 76 (runner_probes/ord.run3*.rest.*).
   */
  if (c_word ("put") && !c_word ("in") && !c_word ("into")
      && !c_word ("inside") && !c_word ("on") && !c_word ("onto")
      && !c_word ("down"))
    {
      bare_put = TRUE;
      count++;
    }
  /*
   * A 3.7 slot word is no word the rows below know, so a line holding one
   * is re-spelled even with one handler in it: `grab stone` becomes `take
   * stone` for the take row, while the tasks have already heard it as typed.
   */
  if (count < 2 && !slot_line)
    return FALSE;
  if (count < 2 && !bare_put)
    {
      for (handler = 0; !present[handler]; handler++)
        ;
      found = -1;
      return rewrite ();
    }

  /*
   * A list line (`all`, `and`) goes to the list arm of the first handler
   * generaltasks calls that holds one, takes then drops, and that arm walks
   * co() over the whole line on its own: `x take all` and `wear take all`
   * are "You pick up the coin and the box.", `drop x all` "You drop the hat
   * and the coin.", `x take coin and hat` with the hat held "You pick up the
   * coin.".  p37ORD..p39ORD (runner_probes/ord.run3*.rest.*).  A line with
   * both a take
   * and a drop word is not measured.
   */
  if (c_word ("all") || c_word ("and"))
    {
      /*
       * Deliberate deviation: the "take" of a leading `take off` is that
       * verb's own word and no second verb, so `take off all` and `take off
       * sleepshirt and sweatshirt` undress instead of picking the room up
       * (AkarisStory.taf).  No Runner probe covers a take-off list; the
       * Runner's c("take") would enter takes().  Any other handler word in
       * the line keeps the measured order above.
       */
      if (scr_strncasecmp (line, "take off ", 9) == 0)
        {
          const std::string respelled = std::string ("remove ") + (line + 9);
          scr_bool alone[LIB_PRE400_HANDLERS];

          if (lib_pre400_handler_words (game, respelled.c_str (), alone) == 1
              && alone[LIB_PRE400_REMOVE])
            {
              handler = LIB_PRE400_REMOVE;
              found = -1;
              return rewrite ();
            }
        }
      if (present[LIB_PRE400_TAKE] == present[LIB_PRE400_DROP])
        return FALSE;
      handler = present[LIB_PRE400_TAKE] ? LIB_PRE400_TAKE : LIB_PRE400_DROP;
      found = -1;
      return rewrite ();
    }

  /* The one object the line names, by co() over the whole line. */
  found = -1;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (!lib_co_pre400 (game, line, object, 1))
        continue;
      if (found != -1)
        {
          /*
           * Left as it was -- unless a slot word would then reach rows that
           * do not know it, where the first handler in the call order has
           * it instead.
           */
          found = -1;
          if (!slot_line)
            return FALSE;
          for (handler = 0; !present[handler]; handler++)
            ;
          return rewrite ();
        }
      found = object;
    }
  /*
   * A static object acts for none of the four, and each of their refusals
   * leaves the line open -- except drops()' below 3.9, which claims it --
   * so examines speaks over the rest: `x take bench`, `take x bench`, `wear
   * x bench` and `remove x bench` are all "A stone bench." at 3.7, 3.8 and
   * 3.9, while `drop x bench` is "You don't have a bench!" at 3.7/3.8 and
   * the description at 3.90.  With no examine word the first handler in the
   * call order answers: `drop take bench`, `take drop bench` and `wear take
   * bench` are "You can't take a bench.".  The 3.7 slot words go the same
   * way (`peer grab box`, `doff peer box`; `dump grab box` is the take
   * refusal).  p37SURF..p39SURF (runner_probes/surf.run3*.stat2v.*) and
   * p37SLOT2 (runner_probes/slot2.run370.b*).
   */
  if (found != -1 && obj_is_static (game, found))
    {
      if (version < TAF_VERSION_390 && present[LIB_PRE400_DROP])
        handler = present[LIB_PRE400_TAKE] ? LIB_PRE400_TAKE : LIB_PRE400_DROP;
      else if (present[LIB_PRE400_EXAMINE])
        handler = LIB_PRE400_EXAMINE;
      else
        for (handler = 0; !present[handler]; handler++)
          ;
      return rewrite ();
    }

  acts[LIB_PRE400_TAKE] = found != -1 && lib_take_filter (game, found, -1);
  /*
   * drops() takes a worn object off the player too; see
   * lib_drop_named_filter().
   */
  acts[LIB_PRE400_DROP] = found != -1
                          && (lib_drop_filter (game, found, -1)
                              || lib_remove_filter (game, found, -1));
  acts[LIB_PRE400_WEAR] = found != -1 && lib_wear_filter (game, found, -1);
  acts[LIB_PRE400_REMOVE] = found != -1 && lib_remove_filter (game, found, -1);
  acts[LIB_PRE400_EXAMINE] = FALSE;

  handler = -1;
  for (object = LIB_PRE400_TAKE; object <= LIB_PRE400_REMOVE; object++)
    {
      if (present[object] && acts[object])
        {
          handler = object;
          break;
        }
    }
  if (handler == -1 && present[LIB_PRE400_EXAMINE] && found != -1)
    handler = LIB_PRE400_EXAMINE;
  for (object = LIB_PRE400_TAKE; handler == -1 && object <= LIB_PRE400_REMOVE;
       object++)
    {
      if (present[object])
        handler = object;
    }
  if (handler == -1)
    handler = LIB_PRE400_EXAMINE;
  return rewrite ();
}
