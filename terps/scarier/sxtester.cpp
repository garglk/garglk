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
#include <stdio.h>
#include <stdlib.h>

#include "scarier.h"
#include "sxprotos.h"


/*
 * test_run_game_script()
 *
 * Run the game using the given script.  Returns the count of errors from
 * the script's monitoring.
 */
static scr_int
test_run_game_script (scr_game game, sxr_script script)
{
  scr_int errors;

  /* Ensure completely repeatable random number sequences. */
  scr_reseed_random_sequence (1);

  /* Start interpreting the script stream. */
  scr_start_script (game, script);
  scr_interpret_game (game);

  /* Wrap up the script interpreter and capture error count. */
  errors = scr_finalize_script ();
  return errors;
}


/*
 * test_run_game_tests()
 *
 * Run each test in the given descriptor array, reporting the results and
 * accumulating an error count overall.  Return the total error count.
 */
scr_int
test_run_game_tests (const sxr_test_descriptor_t tests[],
                     scr_int count, scr_bool is_verbose)
{
  const sxr_test_descriptor_t *test;
  scr_int errors;
  assert (tests);

  errors = 0;

  /* Execute each game in turn. */
  for (test = tests; test < tests + count; test++)
    {
      scr_int test_errors;

      if (is_verbose)
        {
          sxr_trace ("--- Running Test \"%s\" [\"%s\", by %s]...\n",
                    test->name,
                    scr_get_game_name (test->game),
                    scr_get_game_author (test->game));
        }

      test_errors = test_run_game_script (test->game, test->script);
      errors += test_errors;

      if (is_verbose)
        {
          sxr_trace ("--- Test \"%s\": ", test->name);
          if (test_errors > 0)
            sxr_trace ("%s [%ld error%s]\n",
                     "FAIL", test_errors, test_errors == 1 ? "" : "s");
          else
            sxr_trace ("%s\n", "PASS");
        }
      else
        sxr_trace ("%s", test_errors > 0 ? "F" : ".");
      fflush (stdout);
      fflush (stderr);
    }
  sxr_trace ("%s", is_verbose ? "" : "\n");

  return errors;
}
