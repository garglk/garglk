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
 * The connectors between rooms: where a link meets its node, the sites and
 * layout of the Up/Down/In/Out badges, and the first two passes of
 * map_render -- the links themselves and the stubs of unexplored exits.
 */

#include <cstdlib>
#include <cstring>
#include <cmath>

#include "mapdraw.h"
#include "map_internal.h"

using namespace map_internal;


/* The badge discs, one hue per way out of a room, and the mark on them. */
#define ICON_IN        0x00A000
#define ICON_OUT       0xE06090
#define ICON_UP        0xFEBC2E     /* the Finder window's yellow button */
#define ICON_DOWN      0x4060D0
#define ICON_MARK      0xFFFFFF


/*
 * ---------------------------------------------------------- link geometry
 */

/* GetLinkPoint (Map.vb:747): where a connector meets the node's edge. */
static void
link_point (const proj_t *p, const map_node_t *n, int dir, double *x,
            double *y)
{
  double lx = n->x, ly = n->y, w = n->w, h = n->h;
  int dx = 0, dy = 0;           /* Up/Down/In/Out: the centre of the box */

  if (dir >= 0 && dir < MAP_N_DIRS)
    {
      dx = map_dir_dx[dir];
      dy = map_dir_dy[dir];
    }
  /* -1, 0, 1 pick the near edge, the middle and the far edge of the box. */
  *x = px_x (p, lx + w * (1 + dx) / 2);
  *y = px_y (p, ly + h * (1 + dy) / 2);
}

/*
 * GetRelativePoint (Map.vb:1659): a point given as a percentage of the node
 * box, allowed to fall outside it.
 */
static void
rel_point (const proj_t *p, const map_node_t *n, double xp, double yp,
           double *x, double *y)
{
  *x = px_x (p, n->x + n->w * xp / 100.0);
  *y = px_y (p, n->y + n->h * yp / 100.0);
}

/*
 * In and Out have no direction on the plan, so the Runner picks the edge they
 * leave by from where the other room lies (GetLinkPoint, Map.vb:775-816),
 * remembering it on the node as eInEdge / eOutEdge.  That is what puts a
 * room's IN badge on the side facing the room it lets you into.
 */
static int
inout_edge (const map_node_t *n, const map_node_t *dn)
{
  if (dn == nullptr)
    return DIR_N;               /* no node to aim at: the Runner's fallback */
  if (dn->x > n->x + n->w)
    return DIR_E;
  if (dn->x + dn->w < n->x)
    return DIR_W;
  if (dn->y > n->y)
    return DIR_S;
  return DIR_N;
}

/*
 * GetBezierAssister (Map.vb:1592): the control point that bows a connector
 * out of the node in its own direction.  In and Out are absent here on
 * purpose -- see the straight-line note in render_link.
 */
static void
bezier_assister (const proj_t *p, const map_node_t *n, int dir,
                 double dist, double *x, double *y)
{
  double ox, oy;
  int scale = p->cam->scale > 0 ? p->cam->scale : 1;

  if (dist == 0)
    dist = 1;
  ox = dist * 40.0 / scale / n->w;
  oy = dist * 40.0 / scale / n->h;

  switch (dir)
    {
    case DIR_N:  rel_point (p, n, 50, -oy, x, y); break;
    case DIR_NE: rel_point (p, n, 100 + 3 * ox / 4, -oy / 2, x, y); break;
    case DIR_E:  rel_point (p, n, 100 + ox, 50, x, y); break;
    case DIR_SE: rel_point (p, n, 100 + 3 * ox / 4, 100 + oy / 2, x, y); break;
    case DIR_S:  rel_point (p, n, 50, 100 + oy, x, y); break;
    case DIR_SW: rel_point (p, n, -3 * ox / 4, 100 + oy / 2, x, y); break;
    case DIR_W:  rel_point (p, n, -ox, 50, x, y); break;
    case DIR_NW: rel_point (p, n, -3 * ox / 4, -oy / 2, x, y); break;
    default:     rel_point (p, n, 50, 50, x, y); break;
    }
}


/*
 * ------------------------------------------------------------ badge sites
 */


/*
 * Where each site sits, as percentages of the node box (GetRelativePoint),
 * in the order of the BADGE_ enum (map_internal.h).
 */
typedef struct {
  double xp, yp;
} badge_pct_t;

static const badge_pct_t badge_site_pct[] = {
  { 75, 0 }, { 100, 25 }, { 100, 75 }, { 75, 100 },     /* NNE ENE ESE SSE */
  { 25, 100 }, { 0, 75 }, { 0, 25 }, { 25, 0 },         /* SSW WSW WNW NNW */
  { 50, 0 }, { 100, 50 }, { 50, 100 }, { 0, 50 },       /* N E S W         */
  { 100, 0 }, { 100, 100 }, { 0, 100 }, { 0, 0 }        /* NE SE SW NW     */
};
#define N_BADGE_SITES \
  ((int) (sizeof badge_site_pct / sizeof badge_site_pct[0]))

static void
badge_pct (int site, double *xp, double *yp)
{
  if (site < 0 || site >= N_BADGE_SITES)
    site = BADGE_NNW;
  *xp = badge_site_pct[site].xp;
  *yp = badge_site_pct[site].yp;
}

/* The compass port of each direction, -1 for the four badge directions. */
static const int compass_port[MAP_N_DIRS] = {
  BADGE_N, BADGE_E, BADGE_S, BADGE_W, -1, -1, -1, -1,
  BADGE_NE, BADGE_SE, BADGE_SW, BADGE_NW
};

static int
compass_site (int dir)
{
  if (dir < 0 || dir >= MAP_N_DIRS)
    return -1;
  return compass_port[dir];
}

/* In/Out on a facing edge: opposite half-winds (the Runner's ptIn/ptOut). */
static int
inout_site (int dir, int edge)
{
  int in = (dir == DIR_IN);

  switch (edge)
    {
    case DIR_E: return in ? BADGE_ENE : BADGE_ESE;
    case DIR_W: return in ? BADGE_WSW : BADGE_WNW;
    case DIR_S: return in ? BADGE_SSE : BADGE_SSW;
    default:    return in ? BADGE_NNW : BADGE_NNE;
    }
}

/*
 * Up/Down primary half-wind on a facing edge (never the cardinal midpoint,
 * so they clear N/E/S/W stubs).
 */
static int
ud_site_primary (int dir, int edge)
{
  int up = (dir == DIR_UP);

  switch (edge)
    {
    case DIR_E: return up ? BADGE_ENE : BADGE_ESE;
    case DIR_W: return up ? BADGE_WNW : BADGE_WSW;
    case DIR_S: return up ? BADGE_SSE : BADGE_SSW;
    default:    return up ? BADGE_NNE : BADGE_NNW;
    }
}

/* Adjacent corner half-wind when Up/Down's primary is taken by In/Out. */
static int
ud_site_alt (int site)
{
  switch (site)
    {
    case BADGE_ENE: return BADGE_NNE;
    case BADGE_ESE: return BADGE_SSE;
    case BADGE_WNW: return BADGE_NNW;
    case BADGE_WSW: return BADGE_SSW;
    case BADGE_NNE: return BADGE_ENE;
    case BADGE_NNW: return BADGE_WNW;
    case BADGE_SSE: return BADGE_ESE;
    default:        return BADGE_WSW; /* BADGE_SSW */
    }
}

static void
badge_site_point (const proj_t *p, const map_node_t *n, int site,
                  double *x, double *y)
{
  double xp, yp;
  badge_pct (site, &xp, &yp);
  rel_point (p, n, xp, yp, x, y);
}


/* Opposite badge direction (Up<->Down, In<->Out). */
static int
badge_opposite (int dir)
{
  switch (dir)
    {
    case DIR_UP:   return DIR_DOWN;
    case DIR_DOWN: return DIR_UP;
    case DIR_IN:   return DIR_OUT;
    case DIR_OUT:  return DIR_IN;
    default:       return -1;
    }
}

/*
 * Node to aim a badge at for edge placement: the movement destination, when it
 * has a node on this page.
 *
 * Spatterlight extension when it does not: look for a same-page room whose
 * return badge exit (DestinationAnchor, else the opposite of SourceAnchor)
 * points back here, closest match winning -- AoS High In Oak Tree Down ->
 * Dummy on another page, while Middle of Woods Up -> oak.  The Runner has no
 * counterpart; it would leave the badge on its default site.  This only
 * chooses which edge the badge sits on, so the worst a wrong guess does is
 * put it on an odd side of the box.
 */
static const map_node_t *
badge_face_node (const map_page_t *page, const map_node_t *n,
                 const map_link_t *link)
{
  const map_node_t *dn, *best = nullptr;
  int want, i, best_d2 = -1;

  if (page == nullptr || n == nullptr || n->key == nullptr || link == nullptr)
    return nullptr;
  dn = (link->dest != nullptr) ? page_node (page, link->dest) : nullptr;
  if (dn != nullptr)
    return dn;

  want = link->dst_anchor;
  if (want != DIR_UP && want != DIR_DOWN && want != DIR_IN && want != DIR_OUT)
    want = badge_opposite (link->dir);
  if (want < 0)
    return nullptr;

  for (i = 0; i < page->n_nodes; i++)
    {
      const map_node_t *m = &page->nodes[i];
      const map_link_t *back;
      int dx, dy, d2;

      if (m == n || m->key == nullptr)
        continue;
      back = find_dir_link (m, want);
      if (back == nullptr || back->dest == nullptr
          || strcmp (back->dest, n->key) != 0)
        continue;
      dx = m->x - n->x;
      dy = m->y - n->y;
      d2 = dx * dx + dy * dy;
      if (best == nullptr || d2 < best_d2)
        {
          best = m;
          best_d2 = d2;
        }
    }
  return best;
}


/*
 * -------------------------------------------------------------- rendering
 */

/*
 * An exit that leads somewhere the player has not been is drawn as a short
 * stub arrow out of the node (DrawOutArrow, Map.vb).
 */
static void
draw_out_arrow (map_surface_t *s, const proj_t *p, const map_node_t *n,
                int dir, int wd, int alpha)
{
  double x0, y0, x1, y1, dx, dy, len;
  double stub = p->cam->scale * 1.2;

  if (dir < 0 || dir >= MAP_N_DIRS || map_is_badge_dir (dir))
    return;                     /* Up/Down/In/Out get badges, not stubs */
  link_point (p, n, dir, &x0, &y0);
  dx = map_dir_dx[dir];
  dy = map_dir_dy[dir];
  len = sqrt (dx * dx + dy * dy);
  dx /= len;
  dy /= len;
  x1 = x0 + dx * stub;
  y1 = y0 + dy * stub;
  draw_line (s, (int) x0, (int) y0, (int) x1, (int) y1, wd,
             palette.derived.stub, alpha, 0);
  draw_arrowhead (s, x1, y1, dx, dy, wd * 2 + 2, palette.derived.stub, alpha);
}

/*
 * The In/Out mark: a serifed I or a square-shouldered O, built from bars to
 * suit the size of the badge, since no font cell sits centred on a disc this
 * small.  The first row whose min_r the badge's radius reaches is used.
 */
typedef struct {
  int min_r;
  int thick;                    /* weight of every stroke                   */
  int half_h;                   /* rows above and below the centre          */
  int bar_hw;                   /* half-width of I's serifs, and of O's top
                                   and bottom                               */
  int side_x;                   /* outer column of O's walls                */
  int stem_hw;                  /* half-width of I's stem                   */
} io_mark_t;

static const io_mark_t io_marks[] = {
  { 10, 3, 6, 4, 4, 1 },        /* scale 20 and up: 9 wide, 13 high         */
  {  6, 1, 3, 1, 2, 0 },        /* scales 12-19: 7 high                     */
  {  0, 1, 2, 1, 2, 0 }         /* scales 3-11: 5 high                      */
};

static void
draw_io_letter (map_surface_t *s, int cx, int cy, int r, char letter,
                unsigned int rgb, int alpha)
{
  const io_mark_t *m = io_marks;
  int x, y, i, hw;

  while (r < m->min_r)
    m++;

  /*
   * The bars across the top and bottom.  O's taper toward its outer rows
   * (1, 3, 5... either side of the centre column), which rounds its
   * shoulders once the strokes are thick enough to have any.
   */
  for (i = 0; i < m->thick; i++)
    {
      hw = m->bar_hw;
      if (letter == 'O' && hw > 1 + 2 * i)
        hw = 1 + 2 * i;
      for (x = -hw; x <= hw; x++)
        {
          blend (s, cx + x, cy - m->half_h + i, rgb, alpha);
          blend (s, cx + x, cy + m->half_h - i, rgb, alpha);
        }
    }

  /* Between them, I's stem or O's two walls. */
  for (y = -m->half_h + m->thick; y <= m->half_h - m->thick; y++)
    {
      if (letter == 'I')
        {
          for (x = -m->stem_hw; x <= m->stem_hw; x++)
            blend (s, cx + x, cy + y, rgb, alpha);
        }
      else
        {
          for (i = 0; i < m->thick; i++)
            {
              blend (s, cx - m->side_x + i, cy + y, rgb, alpha);
              blend (s, cx + m->side_x - i, cy + y, rgb, alpha);
            }
        }
    }
}

/*
 * The IN / OUT / UP / DOWN bubble on a node edge (DrawInOutIcon, Map.vb:1530;
 * Form29.doicon for ADRIFT 4 Up/Down), at badge site `site`: a disc in the
 * direction's colour, marked in white with an I, an O or an arrow.  The
 * arrows are there because a U and a D are hard to tell apart, and from an
 * O, at the size of a badge.
 *
 * `alpha` fades the whole badge with its card.  `dim` is the badge for a way
 * that leads somewhere not yet seen: the disc washed out toward the paper,
 * and its mark fainter.  It is washed out rather than made see-through
 * because the badges are drawn over the room's name, which at small scales
 * runs underneath them.
 */
void
map_internal::draw_dir_icon_site (map_surface_t *s, const proj_t *p,
                                  const map_node_t *n, int dir, int site,
                                  int alpha, int dim)
{
  double x, y;
  unsigned int rgb;
  int cx, cy, mark_alpha = alpha;
  /*
   * Half a map unit, but no smaller than a disc that can still carry its
   * mark: below MAP_SCALE_MIN the badges stop shrinking with the rooms.
   */
  int r = p->cam->scale / 2 < 4 ? 4 : p->cam->scale / 2;

  switch (dir)
    {
    case DIR_IN:   rgb = ICON_IN;   break;
    case DIR_OUT:  rgb = ICON_OUT;  break;
    case DIR_UP:   rgb = ICON_UP;   break;
    case DIR_DOWN: rgb = ICON_DOWN; break;
    default: return;
    }
  if (dim)
    {
      rgb = mix_rgb (rgb, palette.bg, 0.5);
      mark_alpha /= 2;
    }

  badge_site_point (p, n, site, &x, &y);
  cx = (int) x;
  cy = (int) y;
  fill_circle (s, cx, cy, r, rgb, alpha);
  if (dir == DIR_IN || dir == DIR_OUT)
    draw_io_letter (s, cx, cy, r, dir == DIR_IN ? 'I' : 'O', ICON_MARK,
                    mark_alpha);
  else
    fill_ud_triangle (s, cx, cy, r, dir == DIR_UP, ICON_MARK, mark_alpha);
}

/*
 * The ADRIFT 4 Runner had two pictures per icon: the normal one when the
 * destination has been seen (clicking it recentres the map there), and a
 * dimmed one when it has not (Form29.doicon, the seen-flag branch).
 */
int
map_internal::badge_dest_unseen (const map_view_t *view, const map_link_t *lk)
{
  return lk->dest == nullptr || !view_seen (view, lk->dest);
}


static int
site_taken_io (const inout_badge_t *b, int site)
{
  return (b->has[map_badge (DIR_IN)] && b->site[map_badge (DIR_IN)] == site)
      || (b->has[map_badge (DIR_OUT)] && b->site[map_badge (DIR_OUT)] == site);
}

/*
 * Geometric opposite for compass dirs (movement-only twins have no Map Link
 * DestinationAnchor): the direction whose plan offset is the negation of
 * this one's.  -1 for the badge directions, which have no bearing.
 */
static int
compass_opposite (int dir)
{
  int d;

  if (dir < 0 || dir >= MAP_N_DIRS || map_is_badge_dir (dir))
    return -1;
  for (d = 0; d < MAP_N_DIRS; d++)
    if (map_dir_dx[d] == -map_dir_dx[dir]
        && map_dir_dy[d] == -map_dir_dy[dir])
      return d;
  return -1;
}

/*
 * When `link` coincides with a compass exit, the port on the destination where
 * that compass connector arrives (twin Map Link's DestinationAnchor, else the
 * geometric opposite of the twin direction).  -1 if no twin.
 */
static int
badge_compass_arrival (const map_node_t *n, const map_link_t *link)
{
  const map_link_t *twin_lk;
  int twin;

  if (n == nullptr || link == nullptr || link->dest == nullptr)
    return -1;
  /*
   * compass_twin is set by a5map from Movements (Map Links alone miss
   * compass exits that only exist as movements).
   */
  if (!link->has_compass_twin)
    return -1;
  twin = link->compass_twin;
  twin_lk = find_dir_link (n, twin);
  if (twin_lk != nullptr && twin_lk->dst_anchor >= 0
      && compass_site (twin_lk->dst_anchor) >= 0)
    return twin_lk->dst_anchor;
  return compass_opposite (twin);
}

/* Pin a far badge to a compass arrival port so it sits on the compass line. */
static void
fix_far_compass_site (inout_badge_t *b, int dst_anchor, int arrival_dir)
{
  int cs = compass_site (arrival_dir);

  if (cs < 0 || b == nullptr || !map_is_badge_dir (dst_anchor))
    return;
  b->site[map_badge (dst_anchor)] = cs;
  b->site_fixed[map_badge (dst_anchor)] = 1;
}

/*
 * If this badge direction has an own link that twins a compass exit, park the
 * badge on that compass port and remember to skip its badge connector.
 */
static void
try_compass_port (const map_node_t *n, int dir,
                  int *site, unsigned char *on_compass)
{
  const map_link_t *lk = find_dir_link (n, dir);
  int twin, cs;

  if (lk == nullptr || lk->dest == nullptr || !lk->has_compass_twin)
    return;
  twin = lk->compass_twin;
  cs = compass_site (twin);
  if (cs < 0)
    return;
  *site = cs;
  *on_compass = 1;
}

static void
finalize_badge_sites (inout_badge_t *b, const map_page_t *page)
{
  int i;

  for (i = 0; i < page->n_nodes; i++)
    {
      inout_badge_t *x = &b[i];
      const map_node_t *n = &page->nodes[i];
      const int up = map_badge (DIR_UP), down = map_badge (DIR_DOWN);
      int k;

      /* In and Out first: they own their half-winds outright. */
      for (k = map_badge (DIR_IN); k <= map_badge (DIR_OUT); k++)
        if (x->has[k] && !x->site_fixed[k])
          {
            x->site[k] = inout_site (DIR_UP + k, x->edge[k]);
            try_compass_port (n, DIR_UP + k, &x->site[k], &x->on_compass[k]);
          }

      /* Up and Down step aside when In/Out (or each other) sit there. */
      if (x->has[up] && !x->site_fixed[up])
        {
          x->site[up] = ud_site_primary (DIR_UP, x->edge[up]);
          if (site_taken_io (x, x->site[up]))
            {
              int alt = ud_site_alt (x->site[up]);
              if (!site_taken_io (x, alt))
                x->site[up] = alt;
            }
          try_compass_port (n, DIR_UP, &x->site[up], &x->on_compass[up]);
        }
      if (x->has[down] && !x->site_fixed[down])
        {
          x->site[down] = ud_site_primary (DIR_DOWN, x->edge[down]);
          if (site_taken_io (x, x->site[down])
              || (x->has[up] && x->site[up] == x->site[down]))
            {
              int alt = ud_site_alt (x->site[down]);
              if (!site_taken_io (x, alt)
                  && !(x->has[up] && x->site[up] == alt))
                x->site[down] = alt;
            }
          try_compass_port (n, DIR_DOWN, &x->site[down], &x->on_compass[down]);
        }
    }
}

/*
 * Record the edge a badge sits on, and whether it got there from a link
 * ending at this node rather than one leaving it.
 */
static void
badge_mark (inout_badge_t *b, int dir, int edge, int far)
{
  b->edge[map_badge (dir)] = edge;
  b->has[map_badge (dir)] = 1;
  b->is_far[map_badge (dir)] |= (unsigned char) (far != 0);
}

/*
 * Dest has a Movement on `dir` (FileIO.vb bHasIn / bHasOut / bHasUp /
 * bHasDown).  DrawLinks' far In/Out icon gates on this, not duplex.
 */
static int
node_has_badge_dir (const map_node_t *n, int dir)
{
  if (n == nullptr || !map_is_badge_dir (dir))
    return 0;
  return n->has_badge[map_badge (dir)];
}

/*
 * Arrival half-wind on dest for a badge connector.  Prefer the layout site
 * when dest already wears that badge; otherwise the facing-edge primary.
 */
static int
arrival_badge_site (const inout_badge_t *b, const map_node_t *dn,
                    const map_node_t *src, int dst_anchor)
{
  int edge;

  if (!map_is_badge_dir (dst_anchor))
    return BADGE_NNE;
  if (b != nullptr && b->has[map_badge (dst_anchor)])
    return b->site[map_badge (dst_anchor)];
  edge = inout_edge (dn, src);
  return (dst_anchor == DIR_IN || dst_anchor == DIR_OUT)
         ? inout_site (dst_anchor, edge)
         : ud_site_primary (dst_anchor, edge);
}

/* Ensure the per-page badge scratch array exists. */
static inout_badge_t *
inout_badge_buf (inout_badge_t *b, const map_page_t *page)
{
  if (b != nullptr)
    return b;
  return (inout_badge_t *) calloc ((size_t) page->n_nodes,
                                   sizeof (inout_badge_t));
}

/*
 * Walk the page's In/Out (and A5 Up/Down) links once and record both ends of
 * each, the way RecalculateLinks does (Map.vb:824).  Far-end badges are
 * recorded when the destination has a Movement on DestinationAnchor
 * (bHas*), matching DrawInOutIcon -- including one-way links whose dest
 * Out/In goes elsewhere.  ADRIFT 4's badge links have no geometry to share,
 * so this returns NULL and the fixed a4_badge_site table places them.
 * Returns NULL when the page has nothing to record, which most do.
 */
inout_badge_t *
map_internal::inout_layout (const map_page_t *page, const map_view_t *view)
{
  inout_badge_t *b = nullptr;
  int i, l;

  for (i = 0; i < page->n_nodes; i++)
    {
      const map_node_t *n = &page->nodes[i];
      if (!view_seen (view, n->key))
        continue;

      for (l = 0; l < n->n_links; l++)
        {
          const map_link_t *link = &n->links[l];
          const map_node_t *dn;
          int dst_anchor, arrival;

          if (link->badge)
            continue;
          if (!map_is_badge_dir (link->dir))
            continue;
          b = inout_badge_buf (b, page);
          if (b == nullptr)
            return nullptr;

          dn = (link->dest != nullptr) ? page_node (page, link->dest) : nullptr;
          badge_mark (&b[i], link->dir,
                      inout_edge (n, badge_face_node (page, n, link)), 0);
          if (dn == nullptr || !view_seen (view, dn->key))
            continue;
          dst_anchor = link->dst_anchor;
          /* Arrival badge if dest has that Movement (not duplex-only). */
          if (!node_has_badge_dir (dn, dst_anchor))
            continue;

          badge_mark (&b[dn - page->nodes], dst_anchor, inout_edge (dn, n), 1);
          /*
           * Coincident with a compass exit: sit the far badge on that
           * connector's arrival port (Behind Bar Down+South -> U on Cellar
           * North) instead of a half-wind that misses the line.
           */
          arrival = badge_compass_arrival (n, link);
          if (arrival >= 0)
            fix_far_compass_site (&b[dn - page->nodes], dst_anchor, arrival);
        }

      /*
       * Map.vb DrawNode (1312-1321): a Movement to an *unseen* room draws an
       * In/Out icon (or Up stub) even when the author never wrote a Map
       * <Link> for that direction -- Alyas of Starhollow's By Longhouse has
       * In -> In Longhouse as a Movement only.  Compass directions already get
       * pass-2 stubs the same way; badge dirs were skipped there.
       *
       * Up and Down follow Spatterlight's badge convention rather than those
       * lines literally: the Runner has no Up/Down badge at all, so it sends
       * Up to DrawOutArrow and skips Down outright (the loop is wrapped in
       * `If eDir <> DirectionsEnum.Down`).  Since we already draw a seen
       * Up/Down Link as a U/D disc, an unseen one gets the same disc, and
       * Down comes along for symmetry.
       */
      if (view != nullptr && view->exit_dest != nullptr)
        {
          int di;

          for (di = 0; di < MAP_N_BADGES; di++)
            {
              int dir = badge_order[di];
              const char *dest;

              if (!node_has_badge_dir (n, dir))
                continue;
              if (find_dir_link (n, dir) != nullptr)
                continue;       /* Link path above already recorded this */
              if (b != nullptr && b[i].has[map_badge (dir)])
                continue;       /* far badge from someone else's Link */
              dest = view->exit_dest (view->ctx, n->key, dir);
              if (dest == nullptr || dest[0] == '\0')
                continue;
              if (view_seen (view, dest))
                continue;       /* seen dest needs a Link for a badge */
              b = inout_badge_buf (b, page);
              if (b == nullptr)
                return nullptr;
              /*
               * Face the dest's map node even while unseen (RecalculateNodes
               * walks every node; GetLinkPoint on a reverse DestAnchor does
               * the same for eInEdge).  Missing/off-page -> North default.
               */
              badge_mark (&b[i], dir,
                          inout_edge (n, page_node (page, dest)), 0);
            }
        }
    }
  if (b != nullptr)
    finalize_badge_sites (b, page);
  return b;
}


/*
 * Pass 1 of map_render for one link of the node at index i: its connector,
 * and the arrowhead of a one-way link.
 */
static void
render_link (const render_ctx_t *rc, int i, const map_node_t *n,
             const map_link_t *link)
{
  const map_node_t *dn;
  double x0, y0, x1, y1, x2, y2, x3, y3, dist;
  int alpha, dash, phase = 0;
  int dst_anchor;

  if (link->badge)
    return;                     /* ADRIFT 4 Up/Down/In/Out: icons only */
  if (link->dest == nullptr)
    return;

  /* Destination unseen or on another page: stub arrow only. */
  dn = page_node (rc->page, link->dest);
  if (dn == nullptr || !view_seen (rc->view, dn->key))
    return;

  /*
   * Badge-style exits (In/Out/Up/Down) only draw a connector while the route
   * is currently allowed -- the same HasRouteInDirection gate the badges
   * themselves use.  Compass links keep the dotted-only gate below, matching
   * Map.vb's DashStyle.Dot path.
   */
  if (map_is_badge_dir (link->dir)
      && rc->view != nullptr && rc->view->exit_dest != nullptr
      && rc->view->exit_dest (rc->view->ctx, n->key, link->dir) == nullptr)
    return;

  /*
   * Badge exit coincides with a compass exit: the compass connector
   * already draws the line; sit the badge on that port instead.
   */
  if (rc->badges != nullptr && map_is_badge_dir (link->dir)
      && rc->badges[i].on_compass[map_badge (link->dir)])
    return;

  /* Nodes on a different level than the player's fade out (Map.vb:1436). */
  alpha = palette.derived.link_alpha;
  if (rc->active != nullptr && n->z != rc->active->z && dn->z != rc->active->z)
    alpha = palette.derived.link_alpha_far;

  dash = link->dotted;
  if (link->dotted && rc->view != nullptr && rc->view->ever_blocked != nullptr)
    {
      /*
       * The ADRIFT 5 Runner hides a restricted connector while its
       * restrictions currently fail -- Grandpa's Ranch's Driveway shows no
       * link north until the front door is opened (Map.vb:1429, the
       * DashStyle.Dot HasRouteInDirection gate)...
       */
      if (rc->view->exit_dest == nullptr
          || rc->view->exit_dest (rc->view->ctx, n->key, link->dir) == nullptr)
        return;
      /*
       * ...and draws it solid until the player has actually been blocked
       * there once (Map.vb:1447, bEverBeenBlocked).
       */
      if (!rc->view->ever_blocked (rc->view->ctx, n->key, link->dir))
        dash = 0;
    }

  /*
   * Self-link: DrawOutArrow, not a curve through the box (Map.vb:1474).
   * Skip self-Down the way the Runner does.  This sits below the route
   * gates because the Runner reaches it with the pen already built: a
   * restricted self-link whose restrictions currently fail has left
   * DrawLinks at Map.vb:1429 (Cloak of Darkness's Foyer, North -> itself
   * behind "Task6 Must BeComplete", draws nothing), and an off-level one
   * carries the faded alpha rather than a flat 100.  ADRIFT 4's runner has
   * no such rule -- a Line control from a room to itself is just a point --
   * so line_links keeps its own behaviour.
   */
  if (!rc->map->line_links
      && n->key != nullptr && strcmp (link->dest, n->key) == 0)
    {
      if (link->dir == DIR_DOWN)
        return;
      if (link->dir != DIR_IN && link->dir != DIR_OUT)
        draw_out_arrow (rc->dst, rc->p, n, link->dir, rc->wd, alpha);
      return;
    }

  dst_anchor = link->dst_anchor;
  if (dst_anchor < 0)
    dst_anchor = link->dir;

  /*
   * In/Out and Up/Down connectors run badge to badge at the half-wind sites
   * inout_layout resolved.  Compass links still leave from a face midpoint.
   */
  if (map_is_badge_dir (link->dir))
    {
      if (rc->badges == nullptr)
        return;
      badge_site_point (rc->p, n, rc->badges[i].site[map_badge (link->dir)],
                        &x0, &y0);
    }
  else
    link_point (rc->p, n, link->dir, &x0, &y0);
  if (map_is_badge_dir (dst_anchor))
    {
      int j = (int) (dn - rc->page->nodes);
      int site;

      if (rc->badges == nullptr)
        return;
      site = arrival_badge_site (&rc->badges[j], dn, n, dst_anchor);
      badge_site_point (rc->p, dn, site, &x3, &y3);
    }
  else
    link_point (rc->p, dn, dst_anchor, &x3, &y3);

  dist = sqrt ((x3 - x0) * (x3 - x0) + (y3 - y0) * (y3 - y0));
  if (!rc->map->line_links && link->n_mids > 0 && link->mids != nullptr)
    {
      /*
       * Author-dragged <Anchor> midpoints: DrawCurve through start, mids,
       * end (Map.vb RecalculateLinks / DrawLinks).  Absolute map-unit
       * coords, same as node X/Y.
       */
      int np = link->n_mids + 2;
      double *pts = (double *) malloc ((size_t) np * 2 * sizeof (double));
      int mi;

      if (pts == nullptr)
        return;
      pts[0] = x0;
      pts[1] = y0;
      for (mi = 0; mi < link->n_mids; mi++)
        {
          pts[2 * (mi + 1)] = px_x (rc->p, link->mids[mi].x);
          pts[2 * (mi + 1) + 1] = px_y (rc->p, link->mids[mi].y);
        }
      pts[2 * (np - 1)] = x3;
      pts[2 * (np - 1) + 1] = y3;
      draw_curve (rc->dst, pts, np, rc->wd, palette.derived.link, alpha, dash,
                  &phase);
      /* Tangent for a one-way arrow: last mid -> end. */
      x1 = pts[2 * (np - 2)];
      y1 = pts[2 * (np - 2) + 1];
      x2 = x1;
      y2 = y1;
      free (pts);
    }
  else
    {
      /*
       * A cubic Bezier from (x0,y0) to (x3,y3); the two control points
       * decide whether it bows.
       */
      if (rc->map->line_links)
        {
          /*
           * ADRIFT 4.  Form29.dolink sets the X1/Y1/X2/Y2 of a Line control,
           * which is a straight segment, and it takes only one of the two
           * coordinates from the destination: a North or South link is
           * vertical at the *source* room's centre column and an East or West
           * one horizontal at the source's centre row, whatever column or row
           * the destination ended up in.  So a skewed link sets off towards
           * the destination's row and stops level with it without ever
           * meeting the box -- which is what run400 draws.  (The eight-point
           * diagonals need no such fix: their two anchors are already the
           * corners the Runner uses.)
           */
          switch (link->dir)
            {
            case DIR_N: case DIR_S: x3 = x0; break;
            case DIR_E: case DIR_W: y3 = y0; break;
            default: break;
            }
          x1 = x0; y1 = y0;
          x2 = x3; y2 = y3;
        }
      else if (map_is_badge_dir (link->dir)
               || map_is_badge_dir (dst_anchor))
        {
          /*
           * No bow: a badge connector is a straight run between the two
           * badges however far apart they are.  Checked against run500 5.0.36
           * on Alyas of Starhollow, whose In Longhouse -> By Longhouse link
           * crosses ten map units diagonally and still arrives dead straight,
           * where a compass link over that distance visibly bellies out.
           */
          x1 = x0; y1 = y0;
          x2 = x3; y2 = y3;
        }
      else
        {
          bezier_assister (rc->p, n, link->dir, dist, &x1, &y1);
          bezier_assister (rc->p, dn, dst_anchor, dist, &x2, &y2);
        }
      draw_bezier (rc->dst, x0, y0, x1, y1, x2, y2, x3, y3, rc->wd,
                   palette.derived.link, alpha, dash, &phase);
    }

  /*
   * One-way (Not Duplex): AdjustableArrowCap at the destination end
   * (Map.vb:1450).  Duplex links stay round-capped.
   */
  if (!link->duplex && !rc->map->line_links)
    {
      double adx = x3 - x2, ady = y3 - y2;
      if (adx * adx + ady * ady < 0.01)
        {
          adx = x3 - x0;
          ady = y3 - y0;
        }
      draw_arrowhead (rc->dst, x3, y3, adx, ady, rc->wd * 2 + 2,
                      palette.derived.link, alpha);
    }
}

/* Pass 1 of map_render: connectors, so the room boxes sit on top of them. */
void
map_internal::render_links (const render_ctx_t *rc)
{
  int i, l;

  for (i = 0; i < rc->page->n_nodes; i++)
    {
      const map_node_t *n = &rc->page->nodes[i];
      if (!view_seen (rc->view, n->key))
        continue;

      for (l = 0; l < n->n_links; l++)
        render_link (rc, i, n, &n->links[l]);
    }
}

/*
 * Pass 2 of map_render: exits leading somewhere we have not been, as stub
 * arrows.  The Runner gates these on HasRouteInDirection AndAlso Not
 * HasSeenLocation (Map.vb DrawOutArrow) -- an exit back to a room already on
 * the map is already drawn as a connector, so it must not also get a stub.
 */
void
map_internal::render_exit_stubs (const render_ctx_t *rc)
{
  int i, d;

  if (rc->view == nullptr || rc->view->exit_dest == nullptr)
    return;
  for (i = 0; i < rc->page->n_nodes; i++)
    {
      const map_node_t *n = &rc->page->nodes[i];

      if (!view_seen (rc->view, n->key))
        continue;
      for (d = 0; d < MAP_N_DIRS; d++)
        {
          const char *dest;

          /* In/Out and Up/Down are badge icons, not compass stubs. */
          if (map_is_badge_dir (d))
            continue;
          dest = rc->view->exit_dest (rc->view->ctx, n->key, d);
          if (dest == nullptr || dest[0] == '\0')
            continue;
          if (view_seen (rc->view, dest))
            continue;
          draw_out_arrow (rc->dst, rc->p, n, d, rc->wd,
                          palette.derived.link_alpha);
        }
    }
}
