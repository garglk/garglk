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
 * The end game task action: the Runners' x87 score percentage, the score
 * summary, the key prompt, and task_run_end_game_action().
 */

#include "sctasks_internal.h"

/*
 * task_percent_x87()
 *
 * The Runners' Int(score * (100 / MaxScore)) (run390 43F22E), as the VB6
 * runtime evaluates it: on the x87 with a 64-bit mantissa, each of the
 * divide and the multiply rounded to nearest-even.  IEEE doubles disagree:
 * 161 out of 161 is 100% on run390 (matt) where doubles give 99.99999999999999,
 * and 149/149 is 99% where doubles give exactly 100.  Measured with
 * make_39_endprobe.py --max/--score on run390x (2026-09-24): 97/97 99%,
 * 21/35 59%, 149/149 99% -- none of them what exact arithmetic says either.
 * AArch64 has no 80-bit long double, so the two roundings are done here in
 * integers.  max_score is positive.
 */
/* clang-cl has the type but links against the MSVC runtime, which has no
   128-bit division helper (__udivti3), so it takes the other branch too. */
#if defined(__SIZEOF_INT128__) && !defined(_MSC_VER)
static scr_int
task_percent_x87 (scr_int score, scr_int max_score)
{
  typedef unsigned __int128 u128;
  const scr_bool negative = score < 0;
  const unsigned long long magnitude = negative ? -(unsigned long long) score
                                                : (unsigned long long) score;
  u128 numerator, quotient, remainder, product, half, low;
  int k, bits, shift, exponent;
  unsigned long long mantissa, whole;

  if (magnitude == 0)
    return 0;

  /* q = 100 / max_score as mantissa * 2^-k, mantissa in [2^63, 2^64). */
  k = 0;
  while (((u128) 100 << k) / (u128) max_score < ((u128) 1 << 63))
    k++;
  numerator = (u128) 100 << k;
  quotient = numerator / (u128) max_score;
  remainder = numerator % (u128) max_score;
  if (2 * remainder > (u128) max_score
      || (2 * remainder == (u128) max_score && (quotient & 1)))
    quotient++;
  if (quotient >> 64)
    {
      quotient >>= 1;
      k--;
    }
  mantissa = (unsigned long long) quotient;

  /* |score| * q, rounded back to a 64-bit mantissa. */
  product = (u128) magnitude * mantissa;
  for (bits = 0; bits < 128 && (product >> bits) != 0; bits++)
    ;
  shift = bits > 64 ? bits - 64 : 0;
  if (shift > 0)
    {
      half = (u128) 1 << (shift - 1);
      low = product & (((u128) 1 << shift) - 1);
      product >>= shift;
      if (low > half || (low == half && (product & 1)))
        product++;
    }

  /* Int() floors: toward zero for a positive value, away for a negative. */
  exponent = shift - k;
  if (exponent >= 0)
    return (scr_int) (negative ? -(long long) (product << exponent)
                               : (long long) (product << exponent));
  whole = (unsigned long long) (product >> -exponent);
  if (!negative)
    return (scr_int) whole;
  if ((product & (((u128) 1 << -exponent) - 1)) != 0)
    whole++;
  return -(scr_int) whole;
}

#else /* !__SIZEOF_INT128__ || _MSC_VER */

/*
 * Compilers with no 128-bit integer type (MSVC, and GCC and Clang on 32-bit
 * targets) get the same arithmetic on a pair of 64-bit halves.  Shift counts
 * are always in [0, 128).
 */
typedef struct
{
  unsigned long long hi, lo;
} sc_u128_t;

static sc_u128_t
task_u128 (unsigned long long hi, unsigned long long lo)
{
  sc_u128_t result;
  result.hi = hi;
  result.lo = lo;
  return result;
}

static sc_u128_t
task_u128_shl (sc_u128_t value, int count)
{
  if (count == 0)
    return value;
  if (count >= 64)
    return task_u128 (value.lo << (count - 64), 0);
  return task_u128 ((value.hi << count) | (value.lo >> (64 - count)),
                    value.lo << count);
}

static sc_u128_t
task_u128_shr (sc_u128_t value, int count)
{
  if (count == 0)
    return value;
  if (count >= 64)
    return task_u128 (0, value.hi >> (count - 64));
  return task_u128 (value.hi >> count,
                    (value.lo >> count) | (value.hi << (64 - count)));
}

/* The low count bits of value, count in [1, 128). */
static sc_u128_t
task_u128_low_bits (sc_u128_t value, int count)
{
  if (count >= 64)
    return task_u128 (count == 64 ? 0
                                  : value.hi & ((1ULL << (count - 64)) - 1),
                      value.lo);
  return task_u128 (0, value.lo & ((1ULL << count) - 1));
}

static int
task_u128_compare (sc_u128_t a, sc_u128_t b)
{
  if (a.hi != b.hi)
    return a.hi < b.hi ? -1 : 1;
  if (a.lo != b.lo)
    return a.lo < b.lo ? -1 : 1;
  return 0;
}

static sc_u128_t
task_u128_increment (sc_u128_t value)
{
  value.lo++;
  if (value.lo == 0)
    value.hi++;
  return value;
}

static sc_u128_t
task_u128_multiply (unsigned long long a, unsigned long long b)
{
  const unsigned long long a_lo = a & 0xffffffffULL, a_hi = a >> 32;
  const unsigned long long b_lo = b & 0xffffffffULL, b_hi = b >> 32;
  const unsigned long long lo_lo = a_lo * b_lo, hi_lo = a_hi * b_lo;
  const unsigned long long lo_hi = a_lo * b_hi, hi_hi = a_hi * b_hi;
  const unsigned long long cross = (lo_lo >> 32) + (hi_lo & 0xffffffffULL)
                                   + lo_hi;

  return task_u128 (hi_hi + (hi_lo >> 32) + (cross >> 32),
                    (cross << 32) | (lo_lo & 0xffffffffULL));
}

/* Long division by a divisor below 2^63, so the running remainder fits. */
static sc_u128_t
task_u128_divide (sc_u128_t dividend, unsigned long long divisor,
                  unsigned long long *remainder)
{
  sc_u128_t quotient = task_u128 (0, 0);
  unsigned long long rest = 0;
  int bit;

  for (bit = 127; bit >= 0; bit--)
    {
      rest = (rest << 1) | (task_u128_shr (dividend, bit).lo & 1);
      quotient = task_u128_shl (quotient, 1);
      if (rest >= divisor)
        {
          rest -= divisor;
          quotient.lo |= 1;
        }
    }

  *remainder = rest;
  return quotient;
}

static scr_int
task_percent_x87 (scr_int score, scr_int max_score)
{
  const scr_bool negative = score < 0;
  const unsigned long long magnitude = negative ? -(unsigned long long) score
                                                : (unsigned long long) score;
  const unsigned long long divisor = (unsigned long long) max_score;
  sc_u128_t quotient, product, half, low;
  unsigned long long remainder, mantissa, whole;
  int k, bits, shift, exponent;

  if (magnitude == 0)
    return 0;

  /* q = 100 / max_score as mantissa * 2^-k, mantissa in [2^63, 2^64). */
  k = 0;
  for (;;)
    {
      quotient = task_u128_divide (task_u128_shl (task_u128 (0, 100), k),
                                   divisor, &remainder);
      if (quotient.hi != 0 || quotient.lo >= (1ULL << 63))
        break;
      k++;
    }
  if (2 * remainder > divisor
      || (2 * remainder == divisor && (quotient.lo & 1)))
    quotient = task_u128_increment (quotient);
  if (quotient.hi != 0)
    {
      quotient = task_u128_shr (quotient, 1);
      k--;
    }
  mantissa = quotient.lo;

  /* |score| * q, rounded back to a 64-bit mantissa. */
  product = task_u128_multiply (magnitude, mantissa);
  for (bits = 0; bits < 128 && task_u128_compare (task_u128_shr (product, bits),
                                                  task_u128 (0, 0)) != 0;
       bits++)
    ;
  shift = bits > 64 ? bits - 64 : 0;
  if (shift > 0)
    {
      half = task_u128_shl (task_u128 (0, 1), shift - 1);
      low = task_u128_low_bits (product, shift);
      product = task_u128_shr (product, shift);
      if (task_u128_compare (low, half) > 0
          || (task_u128_compare (low, half) == 0 && (product.lo & 1)))
        product = task_u128_increment (product);
    }

  /* Int() floors: toward zero for a positive value, away for a negative. */
  exponent = shift - k;
  if (exponent >= 0)
    {
      const long long value = (long long) task_u128_shl (product, exponent).lo;
      return (scr_int) (negative ? -value : value);
    }
  whole = task_u128_shr (product, -exponent).lo;
  if (!negative)
    return (scr_int) whole;
  low = task_u128_low_bits (product, -exponent);
  if (low.hi != 0 || low.lo != 0)
    whole++;
  return -(scr_int) whole;
}
#endif /* !__SIZEOF_INT128__ || _MSC_VER */


/*
 * task_print_end_game_summary()
 *
 * Print the Runner's end-of-game score summary.  Form1.endmessage builds it as
 *
 *   If MaxScore > 0 Then
 *     t &= "You scored" & Str(score) & " out of the maximum" & Str(MaxScore)
 *          & "!" & CRLF
 *     t &= "That is" & Str(Int(score * (100 / MaxScore))) & "% of the game!"
 *          & CRLF
 *     If <the win branch> Then                       ' NOT on a loss or death
 *       If score = MaxScore Then t &= "Well done - you scored maximum points!"
 *                           Else t &= "You finished " & CStr(MaxScore - score)
 *                                     & " points short."
 *       t &= CRLF & CRLF
 *   t &= "[Press any key to end]"
 *
 * VB's Str() prepends a space for a non-negative number, which is where the
 * single spaces around the two figures come from; CStr() does not, hence the
 * literal trailing space in "You finished ".  The percentage divides in
 * x87 floating point and truncates with Int() (see task_percent_x87()), so 3
 * out of 8 is 37%, not 38 -- the
 * `score` command's integer (score * 100) / MaxScore agrees here but is not the
 * same expression, so keep this one as written.
 *
 * Measured live, MaxScore 8 and score 3, in run400 (arena config SC in
 * make_arena_probe.py, plus SC0 for MaxScore 0) and in run390
 * (make_39_endprobe.py), which agree line for line:
 *
 *   win    WinText / the two lines / "You finished 5 points short." / blank
 *   lose   "Better luck next time." / the two lines / blank
 *   dead   "I'm afraid you are dead!" / the two lines / blank
 *          (pre-4.0 words that sentence from the perspective; see
 *          lib_get_death_message())
 *   ended  nothing at all
 *   MaxScore 0, win: run400 prints the banner and nothing else, not even the
 *                    blank line; run390 prints the summary anyway, at 100%
 *
 * A UTF-16LE census puts every string of the summary in all four Runners
 * (run370/380/390/400), so this is not gated on 4.0, and 3.7 and 3.8 have since
 * been driven live too: run370 finishing castle.taf and run380 finishing
 * microwaveman.taf print these lines exactly as below.  See
 * test/adrift4/notes/RUNNER_TESTS_TODO.md section 4.
 *
 * A death, whether from an EndGame action or from the Battle System, is the
 * shared run400 General.Sub_22_70 (Proc_21_62, 452384) -- endmessage calls it
 * at 45E10B and Battles.Sub_12_1 at 44AE95 -- and run390 builds the same lines
 * inline in Form1.chardohit (442B14).  Neither has the MaxScore > 0 guard;
 * both substitute 100 for the percentage when MaxScore is 0 (4523A0, 442AD5).
 */
void
task_print_end_game_summary (scr_gameref_t game, scr_bool is_win,
                             scr_bool is_death)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int max_score, percent, version;
  scr_char buffer[32];

  max_score = prop_get_global_integer (bundle, "MaxScore");
  version = prop_get_taf_version (bundle);

  /*
   * The MaxScore > 0 guard arrived with 4.0.  A scoreless 4.0 game gets no
   * summary and no trailing blank line -- measured in run400 on arena config
   * SC0 and again on TheAmulet.taf, whose MaxScore is 0 -- but run390 prints
   * the summary regardless, and reports a scoreless game as 100% complete:
   * ECOD3.taf (MaxScore 0) ends with "You scored 0 out of the maximum 0!" /
   * "That is 100% of the game!" / "Well done - you scored maximum points!".
   * Whether 3.9 special-cases the divide or only the equal-scores case cannot
   * be told apart at 0 out of 0, and no 3.7 or 3.8 game in the corpus has a
   * MaxScore of 0, so this treats the whole pre-4.0 range alike.
   *
   * The guard sits on endmessage's win and lose branches only; the death sub
   * prints regardless.  Measured live in run400: ticktick.taf (EndGame death,
   * runner_transcripts/ticktick.txt) and light_up_4summer_comp.taf (battle
   * death, runner_probes/light_up.run400.txt turn 352) both end "You scored N
   * out of the maximum 0!" / "That is 100% of
   * the game!".
   */
  if (max_score <= 0)
    {
      if (version >= TAF_VERSION_400 && !is_death)
        return;
      percent = 100;
    }
  else
    percent = task_percent_x87 (game->score, max_score);

  pf_buffer_string (filter, "You scored ");
  snprintf (buffer, sizeof (buffer), "%ld", game->score);
  pf_buffer_string (filter, buffer);
  pf_buffer_string (filter, " out of the maximum ");
  snprintf (buffer, sizeof (buffer), "%ld", max_score);
  pf_buffer_string (filter, buffer);
  pf_buffer_string (filter, "!\n");

  pf_buffer_string (filter, "That is ");
  snprintf (buffer, sizeof (buffer), "%ld", percent);
  pf_buffer_string (filter, buffer);
  pf_buffer_string (filter, "% of the game!\n");

  /* Only the win branch weighs the score against the maximum. */
  if (is_win)
    {
      if (game->score == max_score)
        pf_buffer_string (filter, "Well done - you scored maximum points!\n");
      else
        {
          pf_buffer_string (filter, "You finished ");
          snprintf (buffer, sizeof (buffer), "%ld", max_score - game->score);
          pf_buffer_string (filter, buffer);
          pf_buffer_string (filter, " points short.\n");
        }
    }

  /*
   * Every branch closes the summary with a *second* CRLF, leaving a blank line
   * under its last line: run400's win branch at 45DF05 ("Well done...") and
   * 45DF3D/45DF4D ("... points short."), its lose branch at 45E0C0 ("... % of
   * the game!") and the shared death sub at 45246E all end "& CRLF & CRLF",
   * and run390 builds the same pairs inline (43F35D, 43F53B, 43F6C4).  That
   * blank line is the one in the measured shapes above, and it is all that
   * separates the summary from the end-of-session prompt, which the Runner
   * appends with no separator of its own; see task_print_end_keyprompt().
   */
  pf_buffer_character (filter, '\n');
}


/*
 * task_print_end_keyprompt()
 *
 * Append the Runner's end-of-session prompt, "[Press any key to end]", to the
 * text of the turn that ended the game.
 *
 * It is the last thing every ending adds, after the score summary and outside
 * the MaxScore > 0 guard, and it is appended with no separator whatever:
 *
 *     out = out & "[Press any key to end]"            ' run400 45DF82 (win),
 *                                                     ' 45E0F5 (lose),
 *                                                     ' 4524DA (death)
 *
 * -- after which the Runner blocks on a keypress and the process is gone.  The
 * blank line above it in a transcript is not the prompt's: it is the second
 * CRLF of whatever printed last, the score summary's
 * (task_print_end_game_summary()) or, with no summary, the ending branch's
 * own.  So nothing is added here beyond the literal.
 *
 * It goes into the turn's text buffer like any other line, which is why a
 * game's ALR table can rewrite it (Vardock Bates turns it into "[Pulsa
 * cualquier tecla para terminar]"), and every one of the four Runners carries
 * the literal, so it is not gated on a version.  The `endgame` command writes
 * the same tail inline; see lib_cmd_endgame().
 *
 * The "just stop" ending is the one that does not get it: run400's var_88 = 4
 * branch (45E11F) only disables the Adventure menu, and neither run390 nor the
 * older pair has a fourth prompt site either.
 *
 * Hosts opt in: run_set_end_keyprompt().  A Glk host that offers its own
 * ending choices instead of blocking on a key leaves it off.
 */
void
task_print_end_keyprompt (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (!run_get_end_keyprompt ())
    return;

  pf_buffer_string (filter, "[Press any key to end]");
}


/*
 * task_print_end_game_message()
 *
 * Print the ending armed by an EndGame task action, if any, and clear it.
 *
 * The Runners do this at the *end of the turn*, not where the action ran.
 * run400's turn driver is
 *
 *     If gameover = 0 Then Form1.evaluate       ' 0005C65C
 *     If gameover > 0 Then Form1.endmessage     ' 0005C681
 *
 * so the EndGame action only sets the gameover byte (0008D66E, mapping Var1
 * 0,1,2,3 to 1,3,2,4) and every score, variable and text change the rest of
 * the turn makes lands *before* the summary is composed.  Form1.endmessage
 * (0005DDAC) then reads MaxScore and score for itself.
 * Measured live: chicago.taf task 23 is "end game (win)" followed by "+10
 * points", and run390 finishes on 75 where printing at the action gives 65.
 * The corpus has 42 tasks in 32 games that put an EndGame action somewhere
 * other than last.  See test/adrift4/notes/RUNNER_TESTS_TODO.md section 4.
 */
void
task_print_end_game_message (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool is_pre_400;
  scr_int var1;

  if (game->pending_endgame == 0)
    return;

  var1 = game->pending_endgame - 1;
  game->pending_endgame = 0;

  /*
   * Every message below is concatenated onto the turn's accumulated text with
   * no separator of its own, so whether that text is already terminated is the
   * whole difference between the versions: 4.0 ends a task's text block with a
   * line terminator, pre-4.0 does not.  Ours always does
   * (pf_buffer_paragraph_line()), so for a pre-4.0 game take that terminator
   * back first.
   */
  is_pre_400 = prop_get_taf_version (bundle) < TAF_VERSION_400;

  /* Print a message based on var1. */
  switch (var1)
    {
    case 0:
      {
        scr_vartype_t vt_key[2];
        const scr_char *wintext;

        /* Get game WinText. */
        vt_key[0].string = "Header";
        vt_key[1].string = "WinText";
        wintext = prop_get_string (bundle, "S<-ss", vt_key);

        /*
         * run400's endmessage is, for a win, exactly
         *
         *     out = out & WinText & vbCrLf                  ' 0005DDF8
         *
         * -- no separator of any kind, and the line terminator regardless of
         * whether the WinText is empty.  The terminator is measured in run400
         * itself: microbe_willie.taf has an empty WinText, and run400 leaves a
         * blank line between the winning task's text and the score summary.
         *
         * Whether WinText lands on a line of its own therefore depends on what
         * the turn's accumulated text already ends with, and that is where the
         * versions part company.  4.0 terminates a task's text block, so run400
         * finishing ptbad.taf prints "...a few more minutes of your time." and
         * then "You Win! Yay!" underneath -- and that task's CompleteText ends
         * at the full stop, with no trailing <br> of its own (taftool.py unpack
         * of ptbad.taf), so the break is the Runner's.  Pre-4.0 does not, so
         * run380 finishing microwaveman.taf runs the two together as "You win
         * the game.You have destroyed Coffee Man...", and run390 finishing
         * ECOD3.taf, whose task text ends "<br><br>", leaves *two* blank lines
         * before the summary rather than one.  All three measured live.
         *
         * So for a pre-4.0 game take our own section terminator back, and in
         * every version add one after.
         *
         * Print WinText, if any is defined.  There is no default: the
         * Runners' "Congratulations!" is a *status bar caption*, not a line of
         * game text.  run400's endmessage writes it to Form1.StatusBar1 panel
         * 1 (decompiled at 0x000571AC, immediately alongside "You are dead!"
         * at 0x00057205), and those are the literals' only references.
         * Measured live on two empty-WinText games, one at each end of the
         * version range: run400 finishing TheAmulet.taf and run370 finishing
         * castle.taf both print the winning task's own CompleteText and then
         * go straight to the score summary, with the location panel switched
         * to "Congratulations!".  See test/adrift4/notes/RUNNER_TESTS_TODO.md
         * section 4.
         */
        if (is_pre_400)
          {
            pf_undo_auto_break (filter);

            /*
             * 3.9 -- and 3.9 alone -- passes the accumulated text through
             * pspace() before appending the WinText, unconditionally: the
             * win branch of execute_task calls it even for an empty WinText
             * (run390 loc_43F255, the sub itself @42C920).  Measured live on
             * Richard.taf, whose winning task text and WinText join as
             * "...you return to the staging area.  Rich smiles..."
             * (runner_transcripts/richard.txt) with neither side carrying
             * authored spaces (the COMPLETE/WINTEXT dumps end "area." and
             * start "Rich").
             *
             * 3.7 and 3.8 have no pspace sub at all and join inline, with
             * ONE space: run380 tasks() builds the win branch as
             * `out = out & " " & WinText` (44E33C).  The butt-join this
             * comment used to claim is what a whitespace-normalised note of
             * the microwaveman measurement looked like -- the .rtf itself
             * has "You shoot Coffee Man with your gun.  You win the game.
             * You have destroyed Coffee Man...", one space between the
             * winning task's CompleteText and the WinText
             * (runner_transcripts/microwave_man.rtf).
             *
             * Nine pre-3.9 corpus rows carry the join and all nine agree,
             * including the four whose text already ends in spaces, where
             * this one lands on top: akron's four, haunted_house's three
             * and super_liam's five are all one more than the authored
             * text holds.
             *
             * The single space is also why haunt's ending reads "Horace
             * lurches in from above.   You drop down into the laboratory,"
             * with THREE: the walk announcement leaves "above.", the win
             * task's own separator adds the two spaces it would have put in
             * front of a CompleteText (task 23 is `- win`, with none), and
             * this space makes the third.  We do not flush that empty
             * task's separator -- see the "Turn sectioning" lead in
             * test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md -- so Scarier prints
             * one space where the Runner prints three, which no compare sees.
             */
            if (prop_get_taf_version (bundle) >= TAF_VERSION_390)
              pf_buffer_pspace (filter);
            else if (!scr_strempty (wintext))
              pf_buffer_character (filter, ' ');
          }
        if (!scr_strempty (wintext))
          pf_buffer_string (filter, wintext);
        pf_buffer_character (filter, '\n');

        /* Handle any associated WinRes resource. */
        vt_key[0].string = "Globals";
        vt_key[1].string = "WinRes";
        res_handle_resource (game, "ss", vt_key);

        task_print_end_game_summary (game, TRUE, FALSE);
        task_print_end_keyprompt (game);
        break;
      }

    /*
     * Both losing endings open with two line breaks of their own, on top of
     * whatever the turn's text left behind -- run400 builds them as
     * "out & vbCrLf & vbCrLf & <message> & vbCrLf", the lose branch inline at
     * 0005DFDC and the death branch in the shared General.Sub_22_70
     * (000523BF), which is also where a Battle System death lands.  So a 4.0
     * game shows *two* blank lines before the message: measured live on
     * QuestI.taf, whose task 3 "#Check for Death from Hunger" is an ACT type=6
     * v1=2, where run400 leaves three line pitches between the task's text and
     * "I'm afraid you are dead!".
     */
    case 1:
      if (is_pre_400)
        pf_undo_auto_break (filter);
      pf_buffer_string (filter, "\n\nBetter luck next time.\n");
      task_print_end_game_summary (game, FALSE, FALSE);
      task_print_end_keyprompt (game);
      break;

    case 2:
      if (is_pre_400)
        pf_undo_auto_break (filter);
      pf_buffer_string (filter, "\n\n");
      pf_buffer_string (filter, lib_get_death_message (game));
      pf_buffer_character (filter, '\n');
      task_print_end_game_summary (game, FALSE, TRUE);
      task_print_end_keyprompt (game);
      break;

    case 3:
      /*
       * "Just stop": the Runner prints nothing at all -- no message, no
       * summary, and not even the end-of-session prompt.  endmessage's var_88
       * = 4 branch (45E11F) disables the Adventure menu and falls straight to
       * the flush.
       */
      break;

    default:
      scr_fatal ("task_print_end_game_message: invalid type, %ld\n", var1);
    }
}


/*
 * task_run_end_game_action()
 *
 * End of game task action.  Arms the ending and stops the game; the message
 * and the score summary are printed at the end of the turn, by
 * task_print_end_game_message() above.  A second EndGame action in the same
 * turn displaces the first: run400's handler (0008D66E) writes its gameover
 * byte unconditionally, and it is the "execute task" action, not this one,
 * that first tests the byte and skips (0008D621).
 */
scr_bool
task_run_end_game_action (scr_gameref_t game, scr_int var1)
{
  if (var1 < 0 || var1 > 3)
    scr_fatal ("task_run_end_game_action: invalid type, %ld\n", var1);

  game->pending_endgame = var1 + 1;

  /* Stop the game, and note that it's not resumeable. */
  game->is_running = FALSE;
  game->has_completed = TRUE;

  /*
   * Nothing is printed here, whatever the ending, so this reports nothing
   * done -- and in the Runner that is what decides whether the command is
   * finished.  run400's task dispatcher (Proc_19_24_44CCE0, the `tasks()`
   * of the older source) returns True only when the turn's output buffer
   * (MemVar_4941B0) is non-empty once the task has run (loc_44CCC0..44CCCD);
   * an End-Game action only sets the gameover byte, so a task whose only
   * output would be the ending falls through to the library like any other
   * silent task, and the ending is composed after that.  Measured 2026-08-29
   * on relojero.taf (4.00), runner_transcripts/relojero.txt: task 5
   * `arreglar *fenix` has no text and one End-Game (win) action, and run400
   * answers "Disculpa pero no te entiendo." (the game's DontUnderstand) and
   * THEN the WinText.  Returning TRUE for a win or a loss here hid that
   * refusal.
   *
   * This is a 4.0 rule only.  run390's tasks() (run390_3.bas 42BDC4) sets
   * its result to True the moment checktask() finds a task and
   * execute_task() has run it, with no look at what was printed, and
   * run370's (run370.bas 4426B8; run380.bas 44E6AD) reports the gameover
   * status byte; neither consults the output buffer, so the older Runners
   * treat a silent End-Game task as a finished command -- no refusal before
   * the ending.  Before 4.0 the old rule stands: every ending but "Just stop"
   * reports done.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
    return var1 != 3;
  return FALSE;
}
