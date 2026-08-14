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
 * The ADRIFT 4 map: a port of run400.exe's Form29.  See scmap.h for the shape
 * of the algorithm and why we are porting it rather than laying rooms out our
 * own way.
 *
 * Rooms are numbered from 1 here, as they are in the runner, because the exit
 * table stores a destination as "room number + 1, or 0 for no exit" and the
 * layout compares raw destination numbers against grid contents.  Renumbering
 * would mean re-deriving every one of those comparisons, including the ones
 * the runner gets subtly wrong.  Room 0 is unused; sc_room() converts to and
 * from SCARE's zero-based numbering at the edges.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "scarier.h"
#include "scprotos.h"
#include "scmap.h"

/* What check_grid() reports about a cell.  In the runner these are the byte
   literals 255, 254, 253 and 252, which its P-code sign-extends to I2 -- hence
   the tests "grid > -1" for "a room is here" and "grid < 0" for "usable".  We
   spell the negative values out. */
#define GRID_EMPTY    (-1)      /* 255: nothing here                         */
#define GRID_FOREIGN  (-2)      /* 254: a room, but it does not link back    */
#define GRID_HLINK    (-3)      /* 253: an east/west connector passes through */
#define GRID_VLINK    (-4)      /* 252: a north/south connector passes through */

/* The runner's grid is 1000x1000 and it starts you in the middle. */
#define GRID_HOME     500
#define GRID_LIMIT    1000
#define MAX_SHIFTS    1000

/* Why the layout gave up (the runner's mapfail byte). */
#define FAIL_OFF_GRID 1
#define FAIL_STUCK    2
#define FAIL_SHIFTS   3

/* One room, as the layout sees it. */
typedef struct {
  int x, y;                     /* grid cell                                 */
  int sb;                       /* already moved during this shuffle pass    */
  int placed;                   /* has been given a cell                     */
  int hide;                     /* author flagged it "hide on map"           */
  int seen;                     /* the player has been here                  */
  int exits[MAP_N_DIRS];        /* destination room number, 0 for none       */
  int restricted[MAP_N_DIRS];   /* the exit has a movement restriction       */
  int shown[MAP_N_DIRS];        /* a connector has been run out this way     */
} sm_room_t;

typedef struct {
  sm_room_t *rooms;             /* [0] unused; rooms are 1..n                */
  int n;
  int minx, miny, maxx, maxy;   /* extent of the *seen* rooms                */
  int fail;                     /* 0, or one of FAIL_*                       */
  int shifts;
} sm_layout_t;

/* Whether the last build gave up, for scmap_failed(). */
static int scmap_last_fail = 0;

static void sm_set_grid (sm_layout_t *L, int rno, int *x, int *y);


/*
 * sm_opp()
 *
 * The opposite direction (Form29.opp).  Note that Up, Down, In and Out -- and
 * anything out of range -- come back as North: the runner's opp() only assigns
 * its return value in the eight compass cases, and Visual Basic leaves an
 * unassigned Integer at 0.  The layout never asks about those directions, so
 * the quirk is harmless; it is reproduced rather than fixed so that the code
 * reads against the original.
 */
static int
sm_opp (int dir)
{
  switch (dir)
    {
    case DIR_N:  return DIR_S;
    case DIR_E:  return DIR_W;
    case DIR_S:  return DIR_N;
    case DIR_W:  return DIR_E;
    case DIR_NE: return DIR_SW;
    case DIR_SE: return DIR_NW;
    case DIR_SW: return DIR_NE;
    case DIR_NW: return DIR_SE;
    default:     return DIR_N;
    }
}


/*
 * sm_check_grid()
 *
 * What is in cell (x,y)?  A room number, or one of the GRID_* markers.
 *
 * The runner keeps no grid array: it answers this by scanning every room's
 * coordinates, every time, which is why the whole layout is O(n^3)-ish.  We
 * scan too, because the answer is not merely "which room is here" -- a cell
 * with no room in it still counts as occupied when a connector runs through
 * it, and that is derived from the rooms on either side.
 *
 * Two asymmetries below are the runner's, not typos: the horizontal scan gives
 * up after the first room it finds to the left in this row (Exit For), while
 * the vertical scan keeps looking; and neither requires the two rooms to be
 * adjacent, so a connector paints every cell it spans.
 */
static int
sm_check_grid (const sm_layout_t *L, int x, int y)
{
  int i, j;

  for (i = 1; i <= L->n; i++)
    if (L->rooms[i].x == x && L->rooms[i].y == y)
      return i;

  for (i = 1; i <= L->n; i++)
    if (L->rooms[i].x < x && L->rooms[i].y == y)
      {
        for (j = 1; j <= L->n; j++)
          if (L->rooms[j].x > x && L->rooms[j].y == y
              && L->rooms[j].exits[DIR_W] == i)
            return GRID_HLINK;
        break;                  /* the runner's Exit For */
      }

  for (i = 1; i <= L->n; i++)
    if (L->rooms[i].x == x && L->rooms[i].y < y)
      {
        for (j = 1; j <= L->n; j++)
          if (L->rooms[j].x == x && L->rooms[j].y > y
              && L->rooms[j].exits[DIR_N] == i)
            return GRID_VLINK;
      }

  return GRID_EMPTY;
}


/*
 * sm_clear_sb(), sm_shuffle(), sm_sh()
 *
 * Moving a room one cell (Form29.shuffle).  A room never moves alone: every
 * room that has to keep its alignment with it -- the one it is about to land
 * on, and the ones beside it whose connector would otherwise stop being
 * straight -- is dragged along, recursively.  The room *behind* it is not: that
 * connector simply gets longer, and check_grid() paints the extra cells.  So a
 * shuffle is a rigid translation of one alignment-connected lump of the map.
 *
 * sh() is the entry point, and the only place a layout can fail: it counts the
 * shoves, and gives up after a thousand of them or if a room would leave the
 * grid.
 */
static void
sm_clear_sb (sm_layout_t *L)
{
  int i;
  for (i = 1; i <= L->n; i++)
    L->rooms[i].sb = 0;
}

static void
sm_note_extent (sm_layout_t *L, int rno)
{
  const sm_room_t *r = &L->rooms[rno];
  if (!r->seen)
    return;
  if (r->x < L->minx)
    L->minx = r->x;
  if (r->x > L->maxx)
    L->maxx = r->x;
  if (r->y < L->miny)
    L->miny = r->y;
  if (r->y > L->maxy)
    L->maxy = r->y;
}

static void
sm_shuffle (sm_layout_t *L, int rno, int dir)
{
  sm_room_t *r = &L->rooms[rno];
  int d;

  switch (dir)
    {
    case DIR_N: r->y--; break;
    case DIR_E: r->x++; break;
    case DIR_S: r->y++; break;
    case DIR_W: r->x--; break;
    default: break;
    }
  r->sb = 1;

  for (d = DIR_N; d <= DIR_W; d++)
    {
      int dest = r->exits[d];
      int drag;

      if (dest <= 0 || dest > L->n)
        continue;
      if (dir == sm_opp (d))    /* never look back along the axis of travel */
        continue;
      if (!L->rooms[dest].placed || L->rooms[dest].sb)
        continue;

      /* Does this neighbour have to come with us?  It does if it is the room
         we are about to land on, or if it sits on the perpendicular axis and
         would be left behind out of line. */
      drag = (d != dir) ? 1 : 0;
      switch (dir)
        {
        case DIR_N:
          if (L->rooms[dest].y == r->y + drag) drag = 2;
          break;
        case DIR_E:
          if (L->rooms[dest].x == r->x - drag) drag = 2;
          break;
        case DIR_S:
          if (L->rooms[dest].y == r->y - drag) drag = 2;
          break;
        case DIR_W:
          if (L->rooms[dest].x == r->x + drag) drag = 2;
          break;
        default:
          break;
        }

      /* The runner updates the extent here, inside the neighbour loop, so a
         room shuffled with no qualifying neighbour never re-extends it.  Kept
         as-is; tidy_up() and the framing pass both re-derive what they need. */
      sm_note_extent (L, rno);

      if (drag == 2)
        sm_shuffle (L, dest, dir);
    }
}

static void
sm_sh (sm_layout_t *L, int rno, int *x, int *y, int dir)
{
  if (*x < 0 || *x > GRID_LIMIT || *y < 0 || *y > GRID_LIMIT)
    {
      L->fail = FAIL_OFF_GRID;
      return;
    }
  if (++L->shifts > MAX_SHIFTS)
    {
      L->fail = FAIL_SHIFTS;
      return;
    }

  sm_clear_sb (L);
  sm_shuffle (L, rno, dir);

  /* Keep the caller's idea of where `rno` is in step with the move. */
  switch (dir)
    {
    case DIR_N: (*y)--; break;
    case DIR_E: (*x)++; break;
    case DIR_S: (*y)++; break;
    case DIR_W: (*x)--; break;
    default: break;
    }
}


/*
 * sm_try_dec_x() and friends
 *
 * "Keep shoving `rno` until it sits correctly relative to the already-placed
 * `xrno`."  `match` = 1 wants rno strictly to one side (the primary axis of the
 * exit); `match` = 0 wants the coordinate equal (lining up the other axis).
 * When no free cell is available they will step onto a cell holding one of
 * rno's own neighbours, and failing that they declare the map too complex.
 *
 * The `- 1` in those last two tests is an off-by-one in the original: grid
 * cells hold room numbers, and it compares them against a destination that is
 * already a room number.  It is systematic across all four routines, so it is
 * the runner's bug rather than a misreading, and it only misfires on a rarely
 * taken fallback path.  Reproduced.
 */
static void
sm_try_dec_x (sm_layout_t *L, int rno, int *x, int *y, int xrno, int match)
{
  sm_room_t *r = &L->rooms[rno];

  while (r->x > L->rooms[xrno].x - match && L->fail == 0)
    {
      if (sm_check_grid (L, r->x - 1, r->y) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_W);
      else if (sm_check_grid (L, r->x, r->y - 1) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_N);
      else if (sm_check_grid (L, r->x, r->y + 1) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_S);
      else if (sm_check_grid (L, r->x, r->y - 1) == r->exits[DIR_N] - 1)
        sm_sh (L, rno, x, y, DIR_N);
      else if (sm_check_grid (L, r->x, r->y + 1) == r->exits[DIR_S] - 1)
        sm_sh (L, rno, x, y, DIR_S);
      else
        L->fail = FAIL_STUCK;
    }
}

static void
sm_try_inc_x (sm_layout_t *L, int rno, int *x, int *y, int xrno, int match)
{
  sm_room_t *r = &L->rooms[rno];
  int last_gap = 0;

  /* Unlike the other three, this one leads with a stall detector rather than a
     free-cell test: shove east for as long as the previous shove actually
     narrowed the gap.  A shuffle that drags `xrno` along too leaves the gap
     unchanged, which is how it notices it is getting nowhere. */
  while (r->x < L->rooms[xrno].x + match && L->fail == 0)
    {
      int gap = L->rooms[xrno].x + match - r->x;

      if (gap != last_gap)
        {
          last_gap = gap;
          sm_sh (L, rno, x, y, DIR_E);
        }
      else if (sm_check_grid (L, r->x, r->y - 1) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_N);
      else if (sm_check_grid (L, r->x, r->y + 1) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_S);
      else if (sm_check_grid (L, r->x, r->y - 1) == r->exits[DIR_N] - 1)
        sm_sh (L, rno, x, y, DIR_N);
      else if (sm_check_grid (L, r->x, r->y + 1) == r->exits[DIR_S] - 1)
        sm_sh (L, rno, x, y, DIR_S);
      else
        L->fail = FAIL_STUCK;
    }
}

static void
sm_try_dec_y (sm_layout_t *L, int rno, int *x, int *y, int xrno, int match)
{
  sm_room_t *r = &L->rooms[rno];

  while (r->y > L->rooms[xrno].y - match && L->fail == 0)
    {
      if (sm_check_grid (L, r->x, r->y - 1) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_N);
      /* The one place the layout shoves the *other* room instead. */
      else if (sm_check_grid (L, L->rooms[xrno].x,
                              L->rooms[xrno].y + 1) == GRID_EMPTY)
        sm_sh (L, xrno, x, y, DIR_S);
      else if (sm_check_grid (L, r->x - 1, r->y) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_W);
      else if (sm_check_grid (L, r->x + 1, r->y) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_E);
      else if (sm_check_grid (L, r->x - 1, r->y) == r->exits[DIR_W] - 1)
        sm_sh (L, rno, x, y, DIR_W);
      else if (sm_check_grid (L, r->x + 1, r->y) == r->exits[DIR_E] - 1)
        sm_sh (L, rno, x, y, DIR_E);
      else
        L->fail = FAIL_STUCK;
    }
}

static void
sm_try_inc_y (sm_layout_t *L, int rno, int *x, int *y, int xrno, int match)
{
  sm_room_t *r = &L->rooms[rno];

  while (r->y < L->rooms[xrno].y + match && L->fail == 0)
    {
      if (sm_check_grid (L, r->x, r->y + 1) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_S);
      else if (sm_check_grid (L, r->x - 1, r->y) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_W);
      else if (sm_check_grid (L, r->x + 1, r->y) == GRID_EMPTY)
        sm_sh (L, rno, x, y, DIR_E);
      else if (sm_check_grid (L, r->x - 1, r->y) == r->exits[DIR_W] - 1)
        sm_sh (L, rno, x, y, DIR_W);
      else if (sm_check_grid (L, r->x + 1, r->y) == r->exits[DIR_E] - 1)
        sm_sh (L, rno, x, y, DIR_E);
      else
        L->fail = FAIL_STUCK;
    }
}


/*
 * sm_set_grid()
 *
 * Place a room, then place everything reachable from it (Form29.set_grid).
 * This is the whole layout.
 */
static void
sm_set_grid (sm_layout_t *L, int rno, int *px, int *py)
{
  sm_room_t *r = &L->rooms[rno];
  int d;
  /* `match` is scoped to the procedure in the runner, not to the loop below,
     and two of the four cases never reset it -- so it can carry a stale value
     from the previous direction, and the second repair pass runs or does not
     run accordingly.  Faithful ports have to reproduce the staleness, so this
     declaration stays where it is. */
  int match = 0;

  if (r->placed || r->hide)
    return;

  r->placed = 1;
  r->x = *px;
  r->y = *py;
  sm_note_extent (L, rno);

  /* Rooms the player has not seen take up a cell but are not expanded through:
     the map grows only as far as you have explored. */
  if (!r->seen)
    return;

  for (d = DIR_N; d <= DIR_NW; d++)
    {
      int dest, nx, ny, g;

      if (d == DIR_UP)
        d = DIR_NE;             /* Up/Down/In/Out do not move you on the grid */

      /* Re-read our cell each time round: a shove may have moved us. */
      *px = r->x;
      *py = r->y;

      dest = r->exits[d];
      if (!(dest > 0 && dest != rno && dest <= L->n))
        continue;

      nx = r->x;
      ny = r->y;
      switch (d)
        {
        case DIR_N:  ny--;        break;
        case DIR_E:  nx++;        break;
        case DIR_S:  ny++;        break;
        case DIR_W:  nx--;        break;
        case DIR_NE: nx++; ny--;  break;
        case DIR_SE: nx++; ny++;  break;
        case DIR_SW: nx--; ny++;  break;
        case DIR_NW: nx--; ny--;  break;
        default: break;
        }

      /* A room already sitting in the target cell only counts as an obstacle
         we must respect if it links back to us; otherwise it is in our way. */
      g = sm_check_grid (L, nx, ny);
      if (g > -1 && L->rooms[g].exits[sm_opp (d)] != rno)
        g = GRID_FOREIGN;
      if (g >= 0)
        continue;               /* the neighbour is already where it belongs */

      if (!L->rooms[dest].placed)
        {
          if (g == GRID_EMPTY)
            sm_set_grid (L, dest, &nx, &ny);
          else
            {
              /* The cell is spoken for.  Rather than find somewhere else for
                 the neighbour, step *ourselves* one cell in the opposite
                 direction and hand it the cell we just vacated -- which, now
                 that we have moved, lies exactly where it should relative to
                 us.  (nx,ny is reused to hold our old cell, as in the runner:
                 sh() moves px,py out from under us.) */
              nx = *px;
              ny = *py;
              sm_sh (L, rno, px, py, sm_opp (d));
              sm_set_grid (L, dest, &nx, &ny);
            }
          continue;
        }

      /* The neighbour is already on the grid, somewhere else.  Nothing is ever
         re-placed, so instead we shove ourselves until the two line up. */
      switch (d)
        {
        case DIR_N:
          /* No "else match = 0" here, nor in the South case: that is what
             makes `match` stale.  The runner is like this. */
          if (L->rooms[dest].x == r->x)
            {
              int j;
              match = 1;
              /* The whole column between us must already be our connector.  As
                 dest lies north of us, this loop runs from a higher y to a
                 lower one and so -- with Visual Basic's implicit step of 1 --
                 never executes at all.  The same is true of the West case
                 below.  North and West are therefore effectively unchecked,
                 an asymmetry in the original that we keep. */
              for (j = r->y - 1; j <= L->rooms[dest].y + 1; j++)
                if (sm_check_grid (L, r->x, j) != GRID_VLINK)
                  match = 0;
            }
          if (match == 0)
            {
              sm_try_inc_y (L, rno, px, py, dest, 1);
              sm_try_inc_x (L, rno, px, py, dest, 0);
              sm_try_dec_x (L, rno, px, py, dest, 0);
            }
          break;

        case DIR_E:
          if (L->rooms[dest].y == r->y)
            {
              int j;
              match = 1;
              for (j = r->x + 1; j <= L->rooms[dest].x - 1; j++)
                if (sm_check_grid (L, j, r->y) != GRID_HLINK)
                  match = 0;
            }
          else
            match = 0;
          if (match == 0)
            {
              sm_try_dec_x (L, rno, px, py, dest, 1);
              sm_try_inc_y (L, rno, px, py, dest, 0);
              sm_try_dec_y (L, rno, px, py, dest, 0);
            }
          break;

        case DIR_S:
          if (L->rooms[dest].x == r->x)
            {
              int j;
              match = 1;
              for (j = r->y + 1; j <= L->rooms[dest].y - 1; j++)
                if (sm_check_grid (L, r->x, j) != GRID_VLINK)
                  match = 0;
            }
          if (match == 0)
            {
              sm_try_dec_y (L, rno, px, py, dest, 1);
              sm_try_inc_x (L, rno, px, py, dest, 0);
              sm_try_dec_x (L, rno, px, py, dest, 0);
            }
          break;

        case DIR_W:
          /* No alignment test at all; West always tries to realign. */
          sm_try_inc_x (L, rno, px, py, dest, 1);
          sm_try_inc_y (L, rno, px, py, dest, 0);
          sm_try_dec_y (L, rno, px, py, dest, 0);
          break;

        default:
          /* Diagonals get no repair pass.  A diagonal whose target cell is
             taken falls into the branch above and calls sh() with a diagonal
             direction, which shuffle() does not understand -- so the shove is a
             no-op and the neighbour is placed on top of us.  That is what the
             runner does; two rooms in one cell is a real ADRIFT 4 map bug, and
             check_grid() then reports whichever has the lower number. */
          break;
        }

      /* Second repair pass: walk the cells between us and the neighbour and,
         wherever one is occupied, sidestep into whichever perpendicular lane is
         free.  (Again, North and West never execute: their loops count the
         wrong way.) */
      if (match == 0)
        {
          int j;
          switch (d)
            {
            case DIR_N:
              for (j = r->y - 1; j <= L->rooms[dest].y + 1; j++)
                if (sm_check_grid (L, r->x, j) != GRID_EMPTY)
                  {
                    if (sm_check_grid (L, r->x - 1, j) == GRID_EMPTY)
                      sm_sh (L, rno, px, py, DIR_W);
                    if (sm_check_grid (L, r->x + 1, j) == GRID_EMPTY)
                      sm_sh (L, rno, px, py, DIR_E);
                  }
              break;
            case DIR_E:
              for (j = r->x + 1; j <= L->rooms[dest].x - 1; j++)
                if (sm_check_grid (L, j, r->y) != GRID_EMPTY)
                  {
                    if (sm_check_grid (L, j, r->y - 1) == GRID_EMPTY)
                      sm_sh (L, rno, px, py, DIR_N);
                    if (sm_check_grid (L, j, r->y + 1) == GRID_EMPTY)
                      sm_sh (L, rno, px, py, DIR_S);
                  }
              break;
            case DIR_S:
              for (j = r->y + 1; j <= L->rooms[dest].y - 1; j++)
                if (sm_check_grid (L, r->x, j) != GRID_EMPTY)
                  {
                    if (sm_check_grid (L, r->x - 1, j) == GRID_EMPTY)
                      sm_sh (L, rno, px, py, DIR_W);
                    if (sm_check_grid (L, r->x + 1, j) == GRID_EMPTY)
                      sm_sh (L, rno, px, py, DIR_E);
                  }
              break;
            case DIR_W:
              for (j = r->x - 1; j <= L->rooms[dest].x + 1; j++)
                if (sm_check_grid (L, j, r->y) != GRID_EMPTY)
                  {
                    if (sm_check_grid (L, j, r->y - 1) == GRID_EMPTY)
                      sm_sh (L, rno, px, py, DIR_N);
                    if (sm_check_grid (L, j, r->y + 1) == GRID_EMPTY)
                      sm_sh (L, rno, px, py, DIR_S);
                  }
              break;
            default:
              break;
            }
        }
    }
}


/*
 * sm_tidy_up()
 *
 * Squeeze out rows and columns that hold no rooms (Form29.tidy_up).  A row may
 * still be crossed by north/south connectors and a column by east/west ones --
 * deleting it just makes those connectors shorter -- so those cells do not
 * count as occupied here.  Without this the map is full of the empty lanes the
 * shoving left behind.
 */
static void
sm_tidy_up (sm_layout_t *L)
{
  int x, y, i;

  for (y = L->miny; y <= L->maxy; y++)
    {
      int empty = 1;
      for (x = L->minx; x <= L->maxx; x++)
        {
          int g = sm_check_grid (L, x, y);
          if (g != GRID_EMPTY && g != GRID_VLINK)
            {
              empty = 0;
              break;
            }
        }
      if (empty)
        {
          for (i = 1; i <= L->n; i++)
            if (L->rooms[i].y >= y)
              L->rooms[i].y--;
          y--;
          L->maxy--;
        }
    }

  for (x = L->minx; x <= L->maxx; x++)
    {
      int empty = 1;
      for (y = L->miny; y <= L->maxy; y++)
        {
          int g = sm_check_grid (L, x, y);
          if (g != GRID_EMPTY && g != GRID_HLINK)
            {
              empty = 0;
              break;
            }
        }
      if (empty)
        {
          for (i = 1; i <= L->n; i++)
            if (L->rooms[i].x >= x)
              L->rooms[i].x--;
          L->maxx--;
          x--;
        }
    }
}


/*
 * ------------------------------------------------------------ game state
 */

/* Rooms are 1-based in here and 0-based in SCARE. */
#define sc_room(rno) ((scr_int) ((rno) - 1))

/* A boolean that a given .taf version may not have at all: HideOnMap and NoMap
   arrived in ADRIFT 3.9, and even then HideOnMap is only written when the game
   has a map.  prop_get_boolean() treats a missing property as fatal, so ask
   with prop_get() and read an absent flag as FALSE. */
static scr_bool
sm_room_bool (scr_gameref_t game, scr_int room, const scr_char *prop)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3], vt_rvalue;

  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = prop;
  if (!prop_get (bundle, "B<-sis", &vt_rvalue, vt_key))
    return FALSE;
  return vt_rvalue.boolean;
}

static scr_bool
sm_global_bool (scr_gameref_t game, const scr_char *prop)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2], vt_rvalue;

  vt_key[0].string = "Globals";
  vt_key[1].string = prop;
  if (!prop_get (bundle, "B<-ss", &vt_rvalue, vt_key))
    return FALSE;
  return vt_rvalue.boolean;
}

/* An exit's raw destination ("room number + 1", 0 for none) and whether the
   author put a movement restriction on it.  A restricted exit is drawn dotted;
   the runner instead painted it in the background colour when the restriction
   was unmet, which made it vanish -- but our renderer already omits an exit
   the player cannot currently use, via the view's exit_dest callback, so a
   dotted line is what is left to say "there is a way here, conditionally". */
static int
sm_exit (scr_gameref_t game, scr_int room, int dir, int *restricted)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5], vt_rvalue;
  int dest;

  if (restricted != NULL)
    *restricted = 0;

  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Exits";
  vt_key[3].integer = dir;
  if (!prop_get (bundle, "I<-sisi", &vt_rvalue, vt_key))
    return 0;

  vt_key[4].string = "Dest";
  if (!prop_get (bundle, "I<-sisis", &vt_rvalue, vt_key))
    return 0;
  dest = (int) vt_rvalue.integer;

  if (restricted != NULL && dest > 0)
    {
      vt_key[4].string = "Var1";
      if (prop_get_integer (bundle, "I<-sisis", vt_key) - 1 >= 0)
        *restricted = 1;
    }
  return dest;
}


/*
 * ------------------------------------------------------------ view
 */

static int
sm_view_seen (void *ctx, const char *lockey)
{
  scr_gameref_t game = (scr_gameref_t) ctx;
  scr_int room;

  if (game == NULL || lockey == NULL)
    return 0;
  room = atol (lockey);
  if (room < 0 || room >= gs_room_count (game))
    return 0;
  return gs_room_seen (game, room) ? 1 : 0;
}

/* The name in the room box, filtered and untagged exactly the way the status
   line's copy of the same name is (scrunner.c run_update_status).  A room's
   Short is raw author text, and authors do put tags in it: warlord.taf names
   its rooms "<d1>Ravine<d2>" and A Spot of Bother "<d1>Hallway<d2>", markers
   the runner's rich-text control simply drops on display.  Handing the raw
   string to the renderer would paint those tags into the boxes, and would also
   leave the map disagreeing with the status line about what the room is
   called.

   pf_filter() hands back an allocation, so the answer is parked in a rotating
   set of strings, for the same reason sm_view_exit_dest() rotates its key
   buffers: callers do only ever use a label before asking for the next one,
   but a single slot would quietly make that a requirement. */
static const char *
sm_view_name (void *ctx, const char *lockey)
{
  static std::string names[8];
  static int next = 0;
  scr_gameref_t game = (scr_gameref_t) ctx;
  scr_int room;
  scr_char *filtered;

  if (game == NULL || lockey == NULL)
    return NULL;
  room = atol (lockey);
  if (room < 0 || room >= gs_room_count (game))
    return NULL;

  filtered = pf_filter (lib_get_room_name (game, room),
                        gs_get_vars (game), gs_get_bundle (game));
  pf_strip_tags (filtered);

  std::string &slot = names[next++ & 7];
  slot.assign (filtered);
  scr_free (filtered);
  return slot.c_str ();
}

/* Where the exit in `dir` currently leads, restrictions applied.  Everything
   the renderer knows about live connectivity -- the stub arrows for exits into
   the unknown, and the click-to-walk routing -- comes through here, so a locked
   door is simply not an exit until it is unlocked, exactly as in the runner. */
static const char *
sm_view_exit_dest (void *ctx, const char *lockey, int dir)
{
  /* Room keys are numbers, so the answer has to be spelled into a buffer we
     own.  Hand out a rotating set of them rather than one: callers do only ever
     use the result before asking again, but a single buffer would make that a
     requirement, and a silent one. */
  static char bufs[8][16];
  static int next = 0;
  char *buf = bufs[next++ & 7];
  scr_gameref_t game = (scr_gameref_t) ctx;
  scr_int room;
  int dest;

  if (game == NULL || lockey == NULL || dir < 0 || dir >= MAP_N_DIRS)
    return NULL;
  room = atol (lockey);
  if (room < 0 || room >= gs_room_count (game))
    return NULL;

  /* Ask whether there is an exit at all before asking whether it can be used:
     a game without the eight-point compass has no Exits[8..11] to look at, and
     lib_can_go() would go looking for their restrictions. */
  dest = sm_exit (game, room, dir, NULL);
  if (dest <= 0)
    return NULL;
  if (!lib_can_go (game, room, dir))
    return NULL;

  snprintf (buf, sizeof bufs[0], "%d", dest - 1);
  return buf;
}

void
scmap_view (scr_gameref_t game, map_view_t *view)
{
  if (view == NULL)
    return;
  view->seen = sm_view_seen;
  view->name = sm_view_name;
  view->exit_dest = sm_view_exit_dest;
  view->ever_blocked = NULL;    /* no ADRIFT 5 route gates on v4 connectors */
  view->ctx = game;
}


/*
 * ------------------------------------------------------------ build
 */

int
scmap_failed (void)
{
  return scmap_last_fail;
}

int
scmap_available (scr_gameref_t game)
{
  if (game == NULL)
    return 0;
  return gs_room_count (game) > 0 && !sm_global_bool (game, "NoMap");
}

/* Does any task command consist of the bare word "map"?  As in ADRIFT 5, the
   host's map is chrome, and a game that defines MAP as a verb of its own must
   keep it. */
int
scmap_command_taken (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int task, count;

  count = gs_task_count (game);
  for (task = 0; task < count; task++)
    {
      scr_vartype_t vt_key[4];
      scr_int ncommands, c;

      vt_key[0].string = "Tasks";
      vt_key[1].integer = task;
      vt_key[2].string = "Command";
      ncommands = prop_get_child_count (bundle, "I<-sis", vt_key);

      for (c = 0; c < ncommands; c++)
        {
          const scr_char *command;

          vt_key[3].integer = c;
          command = prop_get_string (bundle, "S<-sisi", vt_key);
          if (command != NULL && scr_strcasecmp (command, "map") == 0)
            return 1;
        }
    }
  return 0;
}

/* Add `s` to the map's string pool, which owns it.  Plain malloc/free, because
   that is what map_free() disposes of it with. */
static const char *
sm_intern (map_t *m, const char *s)
{
  char **grown;
  char *copy;

  grown = (char **) realloc (m->pool, (m->n_pool + 1) * sizeof (char *));
  if (grown == NULL)
    return NULL;
  m->pool = grown;

  copy = (char *) malloc (strlen (s) + 1);
  if (copy == NULL)
    return NULL;
  strcpy (copy, s);

  m->pool[m->n_pool] = copy;
  return m->pool[m->n_pool++];
}

map_t *
scmap_build (scr_gameref_t game, const map_view_t *view)
{
  sm_layout_t L;
  map_t *m;
  map_page_t *page;
  int i, d, n, start, x, y, ip;

  scmap_last_fail = 0;
  if (game == NULL)
    return NULL;

  /* The author can turn the map off altogether. */
  if (sm_global_bool (game, "NoMap"))
    return NULL;

  n = (int) gs_room_count (game);
  if (n <= 0)
    return NULL;

  /* --- the layout (Form29.init_map, calc_grid, tidy_up) ----------------- */

  memset (&L, 0, sizeof L);
  L.n = n;
  L.rooms = (sm_room_t *) calloc (n + 1, sizeof (sm_room_t));
  if (L.rooms == NULL)
    return NULL;
  L.minx = L.miny = GRID_LIMIT;
  L.maxx = L.maxy = 0;

  for (i = 1; i <= n; i++)
    {
      scr_int room = sc_room (i);
      char key[16];

      snprintf (key, sizeof key, "%d", (int) room);
      L.rooms[i].seen = (view != NULL && view->seen != NULL)
                        ? (view->seen (view->ctx, key) ? 1 : 0)
                        : (gs_room_seen (game, room) ? 1 : 0);
      L.rooms[i].hide = sm_room_bool (game, room, "HideOnMap") ? 1 : 0;
      for (d = 0; d < MAP_N_DIRS; d++)
        L.rooms[i].exits[d] = sm_exit (game, room, d,
                                       &L.rooms[i].restricted[d]);
    }

  /* Standing in a room the author hid from the map, the runner drew no map at
     all (Form29.drawmap bails before laying anything out).  Not a failure --
     the map returns as soon as the player steps out of the room. */
  start = (int) gs_playerroom (game) + 1;
  if (start < 1 || start > n || L.rooms[start].hide)
    {
      free (L.rooms);
      return NULL;
    }

  x = y = GRID_HOME;
  sm_set_grid (&L, start, &x, &y);
  if (L.fail == 0)
    sm_tidy_up (&L);

  if (L.fail != 0)
    {
      scmap_last_fail = L.fail;
      free (L.rooms);
      return NULL;
    }

  /* --- the map ---------------------------------------------------------- */

  m = (map_t *) calloc (1, sizeof (map_t));
  if (m == NULL)
    {
      free (L.rooms);
      return NULL;
    }
  m->pages = (map_page_t *) calloc (1, sizeof (map_page_t));
  if (m->pages == NULL)
    {
      free (L.rooms);
      free (m);
      return NULL;
    }
  m->n_pages = 1;
  m->line_links = 1;            /* Form29.dolink draws with Line controls */
  page = &m->pages[0];
  page->key = 0;
  page->label = NULL;

  /* Everything the layout placed gets a node, seen or not; the renderer is
     what decides to draw only the rooms the player has been to.  (An unseen
     room next door has a cell of its own -- that is how it stops another room
     from being put there -- but no box until you walk into it.) */
  page->nodes = (map_node_t *) calloc (n, sizeof (map_node_t));
  if (page->nodes == NULL)
    {
      free (L.rooms);
      map_free (m);
      return NULL;
    }

  /* One grid cell becomes a room box plus the gap beside it: the runner spaces
     its boxes 1.5 box-widths apart, and we keep that proportion. */
  ip = 0;
  for (i = 1; i <= n; i++)
    {
      map_node_t *node;
      char key[16];

      if (!L.rooms[i].placed)
        continue;

      node = &page->nodes[ip++];
      snprintf (key, sizeof key, "%d", (int) sc_room (i));
      node->key = sm_intern (m, key);
      node->x = (L.rooms[i].x - L.minx) * (MAP_NODE_W * 3 / 2);
      node->y = (L.rooms[i].y - L.miny) * (MAP_NODE_H * 3 / 2);
      node->z = 0;
      node->w = MAP_NODE_W;
      node->h = MAP_NODE_H;
      node->page = 0;
    }
  page->n_nodes = ip;

  /* Connectors.  The eight compass exits are drawn as links between boxes.
     Up, Down, In and Out become badges on the box (link.badge): they move you
     nowhere on the grid, so their destination can be anywhere on the map, and
     drawing them as connectors would rule lines across the whole thing.  The
     runner showed them as a small icon on the room box (Form29.doicon)
     whenever the raw exit exists -- the destination need not be placed, or
     even seen; an unseen destination only switches the icon to its dimmed
     variant, which the renderer does off the badge link's dest.

     Which connectors get run out is not a question about geometry.  Form29
     kept one Line control per room per direction and drew nothing when the
     far room was already showing the line back this way:

         var_9E = opp(direction)
         If Not(lineshow(endroom, var_9E, frm)) Then   ' dolink
             ...draw, and set line<direction>(startroom).Visible = True

     -- a first-come-first-served claim, walked in the order draw_graphics
     used: rooms by ascending number, and within a room N, E, S, W, NE, SE,
     SW, NW (the badge directions go to doicon and never take part).  So of a
     mutual pair only the lower-numbered room's connector is drawn, and an
     exit is dropped whenever its destination is already showing a line in the
     opposite direction -- to anywhere, not just back to us.  That second case
     is what keeps map39's East--NE--Hub off the map: Hub runs its SW
     connector out to SW Corner first, so East's NE back to Hub finds Hub's SW
     line already showing and is never drawn (issue #155).

     Rooms the player has not seen sit out both halves of this.  Their boxes
     are laid out but draw_graphics skipped them, so they neither claim a
     direction nor lose a connector to one.

     A direction is claimed only for a connector we actually draw.  The runner
     also ran dolink for exits into rooms it had not placed -- one hidden from
     the map, or one no route reaches -- inventing a cell for the far end and
     trailing a stub off towards it, and that stub claimed the direction like
     any other line.  We draw no such stubs, so honouring those claims would
     delete a real connector and leave nothing in its place. */
  for (ip = 0; ip < page->n_nodes; ip++)
    {
      map_node_t *node = &page->nodes[ip];
      int rno = (int) atol (node->key) + 1;
      int nl = 0;

      node->links = (map_link_t *) calloc (MAP_N_DIRS, sizeof (map_link_t));
      if (node->links == NULL)
        break;

      for (d = 0; d < MAP_N_DIRS; d++)
        {
          int dest = L.rooms[rno].exits[d];
          int is_badge = (d == DIR_UP || d == DIR_DOWN
                          || d == DIR_IN || d == DIR_OUT);
          char key[16];

          if (dest <= 0 || dest > n || dest == rno)
            continue;
          if (!is_badge && !L.rooms[dest].placed)
            continue;

          if (!is_badge && L.rooms[rno].seen)
            {
              if (L.rooms[dest].shown[sm_opp (d)])
                continue;
              L.rooms[rno].shown[d] = 1;
            }

          node->links[nl].dir = d;
          node->links[nl].badge = is_badge;
          node->links[nl].dst_anchor = sm_opp (d);
          snprintf (key, sizeof key, "%d", (int) sc_room (dest));
          node->links[nl].dest = sm_intern (m, key);
          node->links[nl].dotted = L.rooms[rno].restricted[d];
          nl++;
        }
      node->n_links = nl;
    }

  free (L.rooms);
  return m;
}
