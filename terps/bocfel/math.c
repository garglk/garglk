/*-
 * Copyright 2010 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

#include "math.h"
#include "branch.h"
#include "process.h"
#include "stack.h"
#include "util.h"
#include "zterp.h"

void zinc(void)
{
  store_variable(zargs[0], variable(zargs[0]) + 1);
}

void zdec(void)
{
  store_variable(zargs[0], variable(zargs[0]) - 1);
}

void znot(void)
{
  store(~zargs[0]);
}

void zdec_chk(void)
{
  int16_t new;
  int16_t val = zargs[1];

  zdec();

  /* The z-spec 1.1 requires indirect variable references to the stack not to push/pop */
  if(zargs[0] == 0) new = *stack_top_element();
  else              new = variable(zargs[0]);

  branch_if(new < val);
}

void zinc_chk(void)
{
  int16_t new;
  int16_t val = zargs[1];

  zinc();

  /* The z-spec 1.1 requires indirect variable references to the stack not to push/pop */
  if(zargs[0] == 0) new = *stack_top_element();
  else              new = variable(zargs[0]);

  branch_if(new > val);
}

void ztest(void)
{
  branch_if( (zargs[0] & zargs[1]) == zargs[1] );
}

void zor(void)
{
  store(zargs[0] | zargs[1]);
}

void zand(void)
{
  store(zargs[0] & zargs[1]);
}

void zadd(void)
{
  store((int16_t)zargs[0] + (int16_t)zargs[1]);
}

void zsub(void)
{
  store((int16_t)zargs[0] - (int16_t)zargs[1]);
}

void zmul(void)
{
  store((int16_t)zargs[0] * (int16_t)zargs[1]);
}

void zdiv(void)
{
  ZASSERT(zargs[1] != 0, "divide by zero");
  store((int16_t)zargs[0] / (int16_t)zargs[1]);
}

void zmod(void)
{
  ZASSERT(zargs[1] != 0, "divide by zero");
  store((int16_t)zargs[0] % (int16_t)zargs[1]);
}

void zlog_shift(void)
{
  int16_t places = zargs[1];

  ZASSERT(places >= -15 && places <= 15, "attempting to shift more than 15 bits: %d", (int)places);

  if(places < 0) store(zargs[0] >> -places);
  else           store(zargs[0] <<  places);
}

void zart_shift(void)
{
  int16_t number = zargs[0], places = zargs[1];

  ZASSERT(places >= -15 && places <= 15, "attempting to shift more than 15 bits: %d", (int)places);

  /* Shifting a negative value in C has some consequences:
   * • Shifting a negative value left is undefined.
   * • Shifting a negative value right is implementation defined.
   *
   * Thus these are done by hand.  The Z-machine requires a right-shift
   * of a negative value to propagate the sign bit.  This is easily
   * accomplished by complementing the value (yielding a positive
   * number), shifting it right (zero filling), and complementing again
   * (flip the shifted-in zeroes to ones).
   *
   * For a left-shift, the result should presumably be the same as a
   * logical shift, so do that.
   */
  if(number < 0)
  {
    if(places < 0) store(~(~number >> -places));
    else           store((zargs[0] <<  places));
  }
  else
  {
    if(places < 0) store(number >> -places);
    else           store(number <<  places);
  }
}
