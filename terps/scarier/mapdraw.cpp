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
 * The map model and its rasteriser, shared by both engines.  See mapdraw.h.
 *
 * Geometry follows the Adrift 5 runner's Map.vb, and the software rasteriser
 * below stands in for the GDI+ calls it makes (FillPolygon / DrawBezier /
 * DrawCurve / DrawEllipse / DrawString).  The ADRIFT 4 runner draws its map
 * out of VB control arrays instead, with a look of its own; we render both
 * engines' maps the same way rather than carrying two rasterisers, so what
 * differs between them is only where the nodes come from -- authored in
 * ADRIFT 5 (a5map.cpp), computed from the exit graph in ADRIFT 4
 * (scmap.cpp).
 */

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "mapdraw.h"

/*
 * DirectionsEnum order (Global.vb:1468), as in a5expr.cpp.  ADRIFT 4 numbers
 * its directions the same way (run400 Form29.opp), so the enum serves both.
 */
const char *const map_dirs[MAP_N_DIRS] = {
  "North", "East", "South", "West", "Up", "Down",
  "In", "Out", "NorthEast", "SouthEast", "SouthWest", "NorthWest"
};

/*
 * The plan offsets in the same order (see mapdraw.h).  The zeros for the
 * badge directions are what make them the centre of the box in link_point()
 * and no stub at all in draw_out_arrow().
 */
const int map_dir_dx[MAP_N_DIRS] = { 0, 1, 0, -1, 0, 0, 0, 0, 1, 1, -1, -1 };
const int map_dir_dy[MAP_N_DIRS] = { -1, 0, 1, 0, 0, 0, 0, 0, -1, 1, 1, -1 };

/*
 * Map.vb:33-39 paints a fixed pastel palette.  We colour the map from the
 * host's text style instead, so the pane matches the story text in any theme.
 * The host passes the two colours in (map_set_palette); until it does, black
 * on white.  Only the badge accents are fixed.
 *
 * Paper and ink seed a small hierarchy:
 *
 *   canvas      = background
 *   room fill   = a little ink mixed into paper
 *   here fill   = the room card mixed further toward ink, so the player's
 *                 room reads as the filled-in box
 *   strokes     = ink
 *   labels      = ink or paper, on the other side of the fill
 *   links/stubs = ink faded toward paper
 *
 * Links are then drawn near-opaque, since the fading is already in the
 * colour.
 *
 * This is the module's only mutable state, and it is the host's, not a
 * game's: it lasts for the process, is not saved, and a new game inherits
 * it.  Everything else here works on the map_t and surface it is handed.
 */
struct map_palette_s
{
  /* What the host passed to map_set_palette(). */
  unsigned int bg = 0xFFFFFF;
  unsigned int fg = 0x000000;

  /* Resolved from those by rebuild_derived_palette(). */
  struct
  {
    int ready = 0;
    unsigned int room_fill = 0xFFFFFF;
    unsigned int room_stroke = 0x000000;
    unsigned int here_fill = 0x000000;
    unsigned int here_stroke = 0x000000;
    unsigned int here_label = 0xFFFFFF;
    unsigned int label = 0x000000;
    unsigned int link = 0x000000;
    unsigned int stub = 0x000000;
    int link_alpha = 100;       /* connectors on the player's level */
    int link_alpha_far = 30;    /* ... and on another level */
  } derived;
};
typedef struct map_palette_s map_palette_t;

static map_palette_t palette;

/* The badge discs, one hue per way out of a room, and the mark on them. */
#define ICON_IN        0x00A000
#define ICON_OUT       0xE06090
#define ICON_UP        0xFEBC2E     /* the Finder window's yellow button */
#define ICON_DOWN      0x4060D0
#define ICON_MARK      0xFFFFFF

static int
rgb_chan (unsigned int rgb, int shift)
{
  return (int) ((rgb >> shift) & 0xFF);
}

static unsigned int
pack_rgb (int r, int g, int b)
{
  if (r < 0) r = 0;
  if (r > 255) r = 255;
  if (g < 0) g = 0;
  if (g > 255) g = 255;
  if (b < 0) b = 0;
  if (b > 255) b = 255;
  return ((unsigned int) r << 16) | ((unsigned int) g << 8)
         | (unsigned int) b;
}

static unsigned int
mix_rgb (unsigned int a, unsigned int b, double t)
{
  int ar = rgb_chan (a, 16), ag = rgb_chan (a, 8), ab = rgb_chan (a, 0);
  int br = rgb_chan (b, 16), bg = rgb_chan (b, 8), bb = rgb_chan (b, 0);
  return pack_rgb ((int) (ar + (br - ar) * t + 0.5),
                   (int) (ag + (bg - ag) * t + 0.5),
                   (int) (ab + (bb - ab) * t + 0.5));
}

static double
rgb_luminance (unsigned int rgb)
{
  return 0.2126 * (rgb_chan (rgb, 16) / 255.0)
       + 0.7152 * (rgb_chan (rgb, 8) / 255.0)
       + 0.0722 * (rgb_chan (rgb, 0) / 255.0);
}

/*
 * Room fills are drawn at this alpha over the canvas (Map.vb ~200).  Label
 * contrast must be measured against the blended on-screen colour, not the
 * raw fill -- otherwise a mid amber over black picks black ink that then
 * sits on a much darker card.
 */
#define MAP_ROOM_FILL_ALPHA 200

/*
 * A room on another level than the player's: card, name and badges.  Well
 * below Map.vb's 50, which left the other levels too close to the player's
 * own to tell apart at a glance.
 */
#define MAP_FAR_ROOM_ALPHA 28
#define MAP_FAR_LABEL_ALPHA 56

/*
 * Paper/ink labels: put the mark on the other side of mid-luminance from
 * the fill.  WCAG on an alpha-blended mid grey still prefers dark ink, which
 * then fails to mark "you are here" as the filled-in box.
 */
static unsigned int
paper_ink_label_on (unsigned int fill)
{
  double fill_l = rgb_luminance (fill);
  double fg_l = rgb_luminance (palette.fg);
  double bg_l = rgb_luminance (palette.bg);
  double mid;

  if (fg_l == bg_l)
    return palette.fg;
  mid = (fg_l + bg_l) * 0.5;
  if (fill_l >= mid)
    return fg_l > bg_l ? palette.bg : palette.fg;
  return fg_l > bg_l ? palette.fg : palette.bg;
}

static void
rebuild_derived_palette (void)
{
  int dark = rgb_luminance (palette.bg) < 0.45;
  double room_t = dark ? 0.18 : 0.12;
  double fill_a = MAP_ROOM_FILL_ALPHA / 255.0;
  unsigned int room_eff, here_eff;

  palette.derived.ready = 1;

  /*
   * Paper and ink only: rooms sit a little off the canvas; the player's
   * room is mixed further toward ink so it reads as the filled-in box.
   * Shallow here-mixes landed on a mid grey whose label then failed to
   * invert.
   */
  palette.derived.room_fill = mix_rgb (palette.bg, palette.fg, room_t);
  palette.derived.room_stroke = palette.fg;
  palette.derived.here_fill = mix_rgb (palette.derived.room_fill, palette.fg,
                                       dark ? 0.85 : 0.90);
  palette.derived.here_stroke = palette.fg;
  room_eff = mix_rgb (palette.bg, palette.derived.room_fill, fill_a);
  here_eff = mix_rgb (palette.bg, palette.derived.here_fill, fill_a);
  palette.derived.label = paper_ink_label_on (room_eff);
  palette.derived.here_label = paper_ink_label_on (here_eff);
  palette.derived.link = mix_rgb (palette.bg, palette.fg, dark ? 0.50 : 0.60);
  palette.derived.stub = mix_rgb (palette.bg, palette.fg, dark ? 0.35 : 0.40);
  palette.derived.link_alpha = 220;
  palette.derived.link_alpha_far = 44;
}

static void
ensure_derived_palette (void)
{
  if (!palette.derived.ready)
    rebuild_derived_palette ();
}

void
map_set_palette (unsigned int background, unsigned int text)
{
  palette.bg = background & 0xFFFFFF;
  palette.fg = text & 0xFFFFFF;
  rebuild_derived_palette ();
}

void
map_free (map_t *map)
{
  int p, n, l;
  if (map == nullptr)
    return;
  for (p = 0; p < map->n_pages; p++)
    {
      map_page_t *page = &map->pages[p];
      for (n = 0; n < page->n_nodes; n++)
        {
          map_node_t *node = &page->nodes[n];
          for (l = 0; l < node->n_links; l++)
            free (node->links[l].mids);
          free (node->links);
        }
      free (page->nodes);
    }
  free (map->pages);
  for (p = 0; p < map->n_pool; p++)
    free (map->pool[p]);
  free (map->pool);
  free (map);
}

/*
 * Find the node for a room.  NULL if the room was never placed on the map:
 * ADRIFT 5 rooms created procedurally have no node, and the ADRIFT 4 layout
 * passes over rooms the author flagged to hide.
 */
const map_node_t *
map_find (const map_t *map, const char *lockey)
{
  int p, n;
  if (map == nullptr || lockey == nullptr)
    return nullptr;
  for (p = 0; p < map->n_pages; p++)
    for (n = 0; n < map->pages[p].n_nodes; n++)
      {
        const map_node_t *node = &map->pages[p].nodes[n];
        if (node->key != nullptr && strcmp (node->key, lockey) == 0)
          return node;
      }
  return nullptr;
}

static const map_page_t *
page_by_key (const map_t *map, int key)
{
  int p;
  for (p = 0; p < map->n_pages; p++)
    if (map->pages[p].key == key)
      return &map->pages[p];
  return nullptr;
}


/*
 * ------------------------------------------------------------- rasteriser
 *
 * A minimal 2D back end: alpha-blended rectangles, wide (optionally dotted)
 * polylines, flattened cubic Beziers, filled circles and 8x8 bitmap text.
 * That is the whole set of GDI+ primitives Map.vb actually uses.
 */

map_surface_t *
map_surface_new (int w, int h)
{
  map_surface_t *s;
  if (w <= 0 || h <= 0)
    return nullptr;
  s = (map_surface_t *) calloc (1, sizeof (map_surface_t));
  if (s == nullptr)
    return nullptr;
  s->w = w;
  s->h = h;
  s->px = (unsigned int *) calloc ((size_t) w * (size_t) h,
                                   sizeof (unsigned int));
  if (s->px == nullptr)
    {
      free (s);
      return nullptr;
    }
  return s;
}

void
map_surface_free (map_surface_t *s)
{
  if (s == nullptr)
    return;
  free (s->px);
  free (s);
}

static void
blend (map_surface_t *s, int x, int y, unsigned int rgb, int alpha)
{
  rgbsurf_blend (s, x, y, rgb, alpha);
}

static void
fill_surface (map_surface_t *s, unsigned int rgb)
{
  size_t i, n = (size_t) s->w * s->h;
  for (i = 0; i < n; i++)
    s->px[i] = rgb & 0xFFFFFF;
}

/* [x0,x1] x [y0,y1] inclusive, as given (an inverted range is empty). */
static void
fill_span (map_surface_t *s, int x0, int y0, int x1, int y1,
           unsigned int rgb, int alpha)
{
  if (x1 >= s->w)
    x1 = s->w - 1;
  if (y1 >= s->h)
    y1 = s->h - 1;
  rgbsurf_fill_rect (s, x0, y0, x1 + 1, y1 + 1, rgb, alpha);
}

/*
 * Fill the rectangle with corners (x0,y0) and (x1,y1), both inclusive, in
 * either order.
 */
static void
fill_rect (map_surface_t *s, int x0, int y0, int x1, int y1,
           unsigned int rgb, int alpha)
{
  if (x1 < x0)
    { int t = x0; x0 = x1; x1 = t; }
  if (y1 < y0)
    { int t = y0; y0 = y1; y1 = t; }
  fill_span (s, x0, y0, x1, y1, rgb, alpha);
}

/*
 * The outline, one pixel wide, corners (x0,y0) and (x1,y1) inclusive.  Each
 * corner belongs to two sides and is painted by both.
 */
static void
draw_rect (map_surface_t *s, int x0, int y0, int x1, int y1,
           unsigned int rgb, int alpha)
{
  fill_span (s, x0, y0, x1, y0, rgb, alpha);
  fill_span (s, x0, y1, x1, y1, rgb, alpha);
  fill_span (s, x0, y0, x0, y1, rgb, alpha);
  fill_span (s, x1, y0, x1, y1, rgb, alpha);
}

/*
 * Bresenham with a round nib of radius `wd/2`.  `dash` != 0 draws a dotted
 * pen (DashStyle.Dot): the runner's dot pattern is on/off in units of the pen
 * width.
 */
static void
draw_line (map_surface_t *s, int x0, int y0, int x1, int y1, int wd,
           unsigned int rgb, int alpha, int dash)
{
  rgbsurf_line (s, x0, y0, x1, y1, wd, RGBSURF_NIB_ROUND, rgb, alpha, dash);
}

/*
 * Round to the nearest pixel.  floor() rather than a bare (int) cast: the cast
 * truncates toward zero, which rounds every negative coordinate one pixel
 * toward the origin and so closes the map up by a pixel where it crosses
 * zero.
 */
static int
round_px (double v)
{
  return (int) floor (v + 0.5);
}

/* Flatten a cubic Bezier (GDI+ DrawBezier) and stroke it. */
static void
draw_bezier (map_surface_t *s, double x0, double y0, double x1, double y1,
             double x2, double y2, double x3, double y3, int wd,
             unsigned int rgb, int alpha, int dash, int *dash_phase)
{
  int i, steps;
  double len;
  int px = 0, py = 0;
  int phase = dash_phase != nullptr ? *dash_phase : 0;
  int period = (wd > 0 ? wd : 1) * 3;

  len = fabs (x3 - x0) + fabs (y3 - y0)
      + fabs (x1 - x0) + fabs (y1 - y0) + fabs (x3 - x2) + fabs (y3 - y2);
  steps = (int) (len / 2.0);
  if (steps < 8)
    steps = 8;
  if (steps > 512)
    steps = 512;

  for (i = 0; i <= steps; i++)
    {
      double t = (double) i / steps;
      double u = 1.0 - t;
      double bx = u * u * u * x0 + 3 * u * u * t * x1
                + 3 * u * t * t * x2 + t * t * t * x3;
      double by = u * u * u * y0 + 3 * u * u * t * y1
                + 3 * u * t * t * y2 + t * t * t * y3;
      int cx = round_px (bx), cy = round_px (by);
      if (i > 0)
        {
          /*
           * Stroke the segment, keeping the dash phase continuous along the
           * whole curve rather than restarting it at every flattened step.
           */
          int dx = abs (cx - px), dy = abs (cy - py);
          int seg = (dx > dy ? dx : dy);
          if (!dash || (phase % period) < period / 2)
            draw_line (s, px, py, cx, cy, wd, rgb, alpha, 0);
          phase += seg > 0 ? seg : 1;
        }
      px = cx;
      py = cy;
    }
  if (dash_phase != nullptr)
    *dash_phase = phase;
}

/*
 * GDI+ DrawCurve (default tension 0.5): Cardinal spline through `pts`.
 * Map.vb uses this when a Link has author-dragged OrigMidPoints / <Anchor>s
 * (RecalculateLinks + DrawLinks), instead of the four-point Bezier.
 */
static void
draw_curve (map_surface_t *s, const double *pts, int n, int wd,
            unsigned int rgb, int alpha, int dash, int *dash_phase)
{
  const double tension = 0.5;
  int i;

  if (n < 2 || pts == nullptr)
    return;
  if (n == 2)
    {
      draw_line (s, (int) pts[0], (int) pts[1], (int) pts[2], (int) pts[3],
                 wd, rgb, alpha, dash);
      return;
    }
  for (i = 0; i < n - 1; i++)
    {
      double x0 = pts[2 * i], y0 = pts[2 * i + 1];
      double x3 = pts[2 * (i + 1)], y3 = pts[2 * (i + 1) + 1];
      double xm1 = (i > 0) ? pts[2 * (i - 1)] : x0;
      double ym1 = (i > 0) ? pts[2 * (i - 1) + 1] : y0;
      double xp2 = (i + 2 < n) ? pts[2 * (i + 2)] : x3;
      double yp2 = (i + 2 < n) ? pts[2 * (i + 2) + 1] : y3;
      double x1 = x0 + (x3 - xm1) * tension / 3.0;
      double y1 = y0 + (y3 - ym1) * tension / 3.0;
      double x2 = x3 - (xp2 - x0) * tension / 3.0;
      double y2 = y3 - (yp2 - y0) * tension / 3.0;
      draw_bezier (s, x0, y0, x1, y1, x2, y2, x3, y3, wd, rgb, alpha, dash,
                   dash_phase);
    }
}

static void
fill_circle (map_surface_t *s, int cx, int cy, int r, unsigned int rgb,
             int alpha)
{
  rgbsurf_fill_circle (s, cx, cy, r, rgb, alpha);
}

/*
 * Pixel-centre barycentric fill.  These triangles are a handful of pixels
 * across (a badge's inner disc), so a bounding-box walk is cheap.
 */
static double
orient2d (double ax, double ay, double bx, double by, double cx, double cy)
{
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static void
fill_triangle (map_surface_t *s,
               double x0, double y0, double x1, double y1,
               double x2, double y2, unsigned int rgb, int alpha)
{
  int minx, maxx, miny, maxy, x, y;
  double a = orient2d (x0, y0, x1, y1, x2, y2);

  if (a == 0.0)
    return;
  if (a < 0.0)
    {
      double tx = x1, ty = y1;
      x1 = x2;
      y1 = y2;
      x2 = tx;
      y2 = ty;
    }
  minx = (int) floor (x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2));
  maxx = (int) ceil  (x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2));
  miny = (int) floor (y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2));
  maxy = (int) ceil  (y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2));
  for (y = miny; y <= maxy; y++)
    for (x = minx; x <= maxx; x++)
      {
        double px = x + 0.5, py = y + 0.5;
        if (orient2d (x0, y0, x1, y1, px, py) >= -0.5
            && orient2d (x1, y1, x2, y2, px, py) >= -0.5
            && orient2d (x2, y2, x0, y0, px, py) >= -0.5)
          blend (s, x, y, rgb, alpha);
      }
}

/*
 * The Up/Down arrow on a badge of radius `r`: an upright triangle with its
 * tip `t` above the badge's centre and its base corners `h` to either side
 * and `b` below.  The corners lie on a circle 3px inside the disc's edge, or
 * as near as whole pixels come, and as near equilateral as they come too:
 * any flatter and the mark reads as a slice of the disc, not an arrow.
 */
static void
ud_triangle_offsets (int r, double *t, double *h, double *b)
{
  const double s3 = 0.8660254037844386; /* √3/2 */
  int inner = r - 3, hh, bb;
  double best = 1e9;

  /* The two smallest are drawn by hand. */
  if (r <= 5)
    {
      /* Scales 3-11: the badge radius stops at 4 below scale 8. */
      *t = *h = *b = 2;
      return;
    }
  if (r == 6)
    {
      /* Scales 12-13: wider than the whole-pixel equilateral fit. */
      *t = *h = 3.5;
      *b = 2;
      return;
    }

  /*
   * The tip goes on the circle.  A base corner goes on the pixel within the
   * circle that is nearest the equilateral one, (√3/2, 1/2) of the way out,
   * with its distance in from the circle counted three times over.
   */
  *t = inner;
  *h = *b = 1;
  for (hh = 1; hh <= inner; hh++)
    for (bb = 1; hh * hh + bb * bb <= inner * inner; bb++)
      {
        double in = inner - sqrt ((double) (hh * hh + bb * bb));
        double dh = hh - s3 * inner, db = bb - 0.5 * inner;
        double score = 3.0 * in * in + dh * dh + db * db;

        if (score < best)
          {
            best = score;
            *h = hh;
            *b = bb;
          }
      }
}

static void
fill_ud_triangle (map_surface_t *s, int cx, int cy, int r, int up,
                  unsigned int rgb, int alpha)
{
  /* Pixel centres, matching the disc whose visual centre is (cx,cy). */
  double ox = cx + 0.5, oy = cy + 0.5;
  double t, h, b;

  ud_triangle_offsets (r, &t, &h, &b);
  if (!up)
    {
      t = -t;
      b = -b;
    }
  fill_triangle (s, ox, oy - t, ox - h, oy + b, ox + h, oy + b, rgb, alpha);
}

/*
 * A filled arrow head at (x,y) pointing along (dx,dy) -- GDI+
 * AdjustableArrowCap(4,4), used for one-way links.
 */
static void
draw_arrowhead (map_surface_t *s, double x, double y, double dx, double dy,
                int size, unsigned int rgb, int alpha)
{
  double len = sqrt (dx * dx + dy * dy);
  double ux, uy, nx, ny;
  int i;
  if (len < 0.001)
    return;
  ux = dx / len;
  uy = dy / len;
  nx = -uy;
  ny = ux;
  /* Scanline the triangle by walking back from the tip. */
  for (i = 0; i <= size; i++)
    {
      double bx = x - ux * i;
      double by = y - uy * i;
      double half = (double) i / 2.0;
      double j;
      for (j = -half; j <= half; j += 0.5)
        blend (s, round_px (bx + nx * j), round_px (by + ny * j), rgb, alpha);
    }
}


/*
 * ------------------------------------------------------------ bitmap text
 *
 * Glk graphics windows cannot render text, so room labels are drawn as
 * glyphs, in the shared rasteriser's two faces: 8x8 (font8x8_basic) and, for
 * room names too long to fit their box at 8x8, 5x7.  Map.vb binary-searches a
 * GDI+ font size until the name fits (GetFont); we carry two bitmap faces
 * instead, and drop to the small one when the big one will not fit.
 */
typedef rgbsurf_font_t map_font_t;

static const map_font_t &kBigFont = rgbsurf_font8x8;
static const map_font_t &kSmallFont = rgbsurf_font5x7;

/*
 * Proportional advance: trim the glyph's unused right-hand columns so labels
 * stay compact in a 60px-wide room box.
 */
static int
glyph_advance (const map_font_t *f, unsigned char c)
{
  if (c < 32 || c > 126 || c == ' ')
    return f->w / 2 + 1;
  return rgbsurf_glyph_cols (f, c) + 1;
}

static void
draw_glyph (map_surface_t *s, const map_font_t *f, unsigned char c, int x,
            int y, unsigned int rgb, int alpha)
{
  rgbsurf_draw_glyph (s, f, c, x, y, rgb, alpha);
}

static int
text_width (const map_font_t *f, const char *t, size_t len)
{
  size_t i;
  int w = 0;
  for (i = 0; i < len; i++)
    w += glyph_advance (f, (unsigned char) t[i]);
  return w;
}

static void
draw_text (map_surface_t *s, const map_font_t *f, const char *t,
           size_t len, int x, int y, unsigned int rgb, int alpha)
{
  size_t i;
  for (i = 0; i < len; i++)
    {
      draw_glyph (s, f, (unsigned char) t[i], x, y, rgb, alpha);
      x += glyph_advance (f, (unsigned char) t[i]);
    }
}

/*
 * Word-wrap `text` to `maxw` pixels in `f`.  `broke` reports whether a word
 * had to be split, which means the face is too big for the box.
 */
static void
wrap_text (const map_font_t *f, const char *text, int maxw,
           std::vector<std::string> &lines, int *broke)
{
  size_t i = 0, n = strlen (text);

  *broke = 0;
  while (i < n)
    {
      size_t start = i, brk = 0, j;
      int w = 0;

      for (j = i; j < n; j++)
        {
          int gw = glyph_advance (f, (unsigned char) text[j]);

          /*
           * An advance includes the gap that follows the glyph; the last one
           * on a line has nothing after it, so don't make the line pay for
           * it.  Without this a name is split for want of a single pixel.
           */
          if (w + gw - 1 > maxw)
            break;
          w += gw;
          if (text[j] == ' ')
            brk = j;
        }
      if (j >= n)
        {
          lines.push_back (std::string (text + start, n - start));
          break;
        }
      if (brk > start)
        {
          lines.push_back (std::string (text + start, brk - start));
          i = brk + 1;
        }
      else
        {
          /* A single word too wide for the box: split it. */
          size_t take = (j > start) ? j - start : 1;
          lines.push_back (std::string (text + start, take));
          i = start + take;
          *broke = 1;
        }
    }
}

/*
 * Centre the room name in its box.  Stands in for GDI+'s StringFormat centring
 * plus Map.vb's GetFont auto-fit: the runner binary-searches a font size that
 * makes the name fit, and since a Glk graphics window has no text at all, we
 * do the bitmap equivalent -- lay the name out at 8x8, and drop to the 5x7
 * face when that would split a word or overflow the box.
 */
static void
draw_label (map_surface_t *s, const char *text, int x0, int y0, int x1,
            int y1, unsigned int rgb, int alpha)
{
  std::vector<std::string> lines;
  const map_font_t *f = &kBigFont;
  int boxw = x1 - x0, boxh = y1 - y0;
  int maxw = boxw - 1;
  int broke, total, ty;
  size_t i;

  if (text == nullptr || text[0] == '\0' || maxw < 4)
    return;

  wrap_text (f, text, maxw, lines, &broke);
  if (broke || (int) lines.size () * f->h > boxh)
    {
      lines.clear ();
      f = &kSmallFont;
      wrap_text (f, text, maxw, lines, &broke);
    }

  total = (int) lines.size () * f->h;
  ty = y0 + (boxh - total) / 2;
  /* Still taller than the box: start at the top. */
  if (ty < y0)
    ty = y0;
  for (i = 0; i < lines.size (); i++)
    {
      int tw = text_width (f, lines[i].c_str (), lines[i].size ()) - 1;
      int tx = x0 + (boxw - tw) / 2;
      int yy = ty + (int) i * f->h;

      if (yy + f->h > y1 && i > 0)
        break;                  /* out of room even at 5x7: clip the tail */
      draw_text (s, f, lines[i].c_str (), lines[i].size (), tx, yy, rgb, alpha);
    }
}


/*
 * ------------------------------------------------------------- projection
 *
 * Plan view: Convert3DtoScreen is the identity at the runner's default
 * offsets, so screen = map-units * scale, translated by the camera.  Z drops
 * out of the position entirely and survives only as the level fade.
 */

typedef struct {
  const map_camera_t *cam;
  int ox, oy;                   /* pixel offset of map origin */
} proj_t;

/*
 * The height of the strip the camera keeps clear for the pan/zoom buttons,
 * in a window `h` pixels high: the map has what is left underneath it.
 */
static int
chrome_top (const map_camera_t *cam, int h)
{
  if (cam->chrome_h <= 0 || h <= 0)
    return 0;
  return cam->chrome_h < h ? cam->chrome_h : h;
}

/* Set up the projection into a window `w` by `h` pixels. */
static void
proj_init (proj_t *p, const map_camera_t *cam, int w, int h)
{
  int top = chrome_top (cam, h);

  p->cam = cam;
  p->ox = w / 2 - cam->cx;
  /* Centre the view in the area under the buttons. */
  p->oy = top + (h - top) / 2 - cam->cy;
}

static int
px_x (const proj_t *p, double ux)
{
  return round_px (ux * p->cam->scale) + p->ox;
}

static int
px_y (const proj_t *p, double uy)
{
  return round_px (uy * p->cam->scale) + p->oy;
}

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
 * In and Out have no direction on the plan, so the runner picks the edge they
 * leave by from where the other room lies (GetLinkPoint, Map.vb:775-816),
 * remembering it on the node as eInEdge / eOutEdge.  That is what puts a room's
 * IN badge on the side facing the room it lets you into.
 */
static int
inout_edge (const map_node_t *n, const map_node_t *dn)
{
  if (dn == nullptr)
    return DIR_N;               /* no node to aim at: the runner's fallback */
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
 * Badge sites: the eight half-winds between the drawn compass stubs.
 * Cardinals (N/E/S/W) and diagonals (NE/SE/SW/NW) own the edge centres and
 * corners; badges sit on the subdivisions between them -- which is also
 * where ADRIFT 5's ptIn/ptOut land (Map.vb:902-928).
 *      NNW          NNE
 *   WNW                ENE
 *   WSW                ESE
 *      SSW          SSE
 */
enum {
  BADGE_NNE, BADGE_ENE, BADGE_ESE, BADGE_SSE,
  BADGE_SSW, BADGE_WSW, BADGE_WNW, BADGE_NNW,
  /* Compass ports: used when a badge exit coincides with a compass exit. */
  BADGE_N, BADGE_E, BADGE_S, BADGE_W,
  BADGE_NE, BADGE_SE, BADGE_SW, BADGE_NW
};

/*
 * Where each site sits, as percentages of the node box (GetRelativePoint),
 * in the order of the enum above.
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

/* In/Out on a facing edge: opposite half-winds (the runner's ptIn/ptOut). */
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


/*
 * -------------------------------------------------------- pages and views
 */

static int
view_seen (const map_view_t *v, const char *key)
{
  if (v == nullptr || v->seen == nullptr || key == nullptr)
    return 0;
  return v->seen (v->ctx, key);
}

static const map_node_t *
page_node (const map_page_t *page, const char *key)
{
  int i;
  if (page == nullptr || key == nullptr)
    return nullptr;
  for (i = 0; i < page->n_nodes; i++)
    if (page->nodes[i].key != nullptr
        && strcmp (page->nodes[i].key, key) == 0)
      return &page->nodes[i];
  return nullptr;
}

/* The node's own link in direction `dir`, if it has one. */
static const map_link_t *
find_dir_link (const map_node_t *n, int dir)
{
  int l;

  if (n == nullptr)
    return nullptr;
  for (l = 0; l < n->n_links; l++)
    if (n->links[l].dir == dir)
      return &n->links[l];
  return nullptr;
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
 * Dummy on another page, while Middle of Woods Up -> oak.  The runner has no
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
 * The page the runner would switch to: the player's own (SelectNode), or the
 * first one when the player is somewhere the map does not place.  Returns 0
 * when there is no page to pick, leaving *key alone.
 */
static int
player_page_key (const map_t *map, const char *player_key, int *key)
{
  const map_node_t *pn;

  if (map == nullptr)
    return 0;
  pn = map_find (map, player_key);
  if (pn != nullptr)
    {
      *key = pn->page;
      return 1;
    }
  if (map->n_pages > 0)
    {
      *key = map->pages[0].key;
      return 1;
    }
  return 0;
}

int
map_has_content (const map_t *map, const map_view_t *view,
                 const char *player_key)
{
  const map_page_t *page;
  int key = 0, i;

  if (!player_page_key (map, player_key, &key))
    return 0;
  page = page_by_key (map, key);
  if (page == nullptr)
    return 0;

  /*
   * The same two tests render_node applies before it draws a box.  A hidden
   * room counts for nothing even when seen, which is exactly the case that
   * matters: the staging rooms games park the player in during their opening
   * screens are hidden.
   */
  for (i = 0; i < page->n_nodes; i++)
    {
      if (page->nodes[i].hidden)
        continue;
      if (view_seen (view, page->nodes[i].key))
        return 1;
    }
  return 0;
}


/*
 * ---------------------------------------------------------------- framing
 */

/*
 * Pixels kept clear around the seen rooms when fitting them to the window,
 * for the labels and out-arrows that overhang the boxes.
 */
#define MAP_FRAME_MARGIN 24

static int
clamp_int (int v, int lo, int hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

/* The box around every room on `page` the player has seen, in map units. */
typedef struct {
  int x0, y0, x1, y1;
} map_extent_t;

/* Returns 0, leaving `ext` alone, when no room on the page has been seen. */
static int
seen_extent (const map_page_t *page, const map_view_t *view,
             map_extent_t *ext)
{
  int i, found = 0;

  for (i = 0; i < page->n_nodes; i++)
    {
      const map_node_t *n = &page->nodes[i];

      if (!view_seen (view, n->key))
        continue;
      if (!found || n->x < ext->x0)
        ext->x0 = n->x;
      if (!found || n->y < ext->y0)
        ext->y0 = n->y;
      if (!found || n->x + n->w > ext->x1)
        ext->x1 = n->x + n->w;
      if (!found || n->y + n->h > ext->y1)
        ext->y1 = n->y + n->h;
      found = 1;
    }
  return found;
}

/* The largest scale at which `span` map units go into `room` pixels. */
static int
fit_axis (int span, int room)
{
  return span > 0 ? room / span : MAP_SCALE_MAX;
}

/*
 * Place the camera along one axis: the seen extent `lo`..`hi` (map units)
 * against `room` pixels of window.  An extent that fits is centred, and
 * cannot pan.  One that overflows is centred on `want` as nearly as keeping
 * the window full allows, and can still pan whichever ways it stopped short
 * of an end: those buttons are added to `enabled`.
 */
static int
frame_axis (int lo, int hi, int scale, int room, int want,
            int back_btn, int fwd_btn, unsigned int *enabled)
{
  int first, last;

  if ((hi - lo) * scale <= room)
    return (int) ((lo + hi) / 2.0 * scale);

  first = lo * scale + room / 2;
  last = hi * scale - room / 2;
  want = clamp_int (want, first, last);
  if (want > first)
    *enabled |= map_chrome_bit (back_btn);
  if (want < last)
    *enabled |= map_chrome_bit (fwd_btn);
  return want;
}

/*
 * map_frame's work: choose the page, the scale and the centre.  Returns the
 * pan buttons that have somewhere to go, and passes back the scale the
 * automatic fit would choose.
 */
static unsigned int
frame_camera (const map_t *map, const map_view_t *view,
              const char *player_key, const map_surface_t *dst,
              int zoom, int follow, map_camera_t *cam, int *fit_scale)
{
  const map_node_t *pn = map_find (map, player_key);
  const map_page_t *page;
  map_extent_t ext;
  int room_w, room_h, sx, sy, scale, want_x, want_y;
  unsigned int pan = 0;

  /*
   * Stay on the player's page while they are on a visible map room; a hidden
   * or unplaced room keeps the previous page so the view does not jump.
   */
  if (pn != nullptr && !pn->hidden)
    cam->page = pn->page;
  else if (page_by_key (map, cam->page) == nullptr
           && !player_page_key (map, player_key, &cam->page))
    cam->page = 0;

  page = page_by_key (map, cam->page);
  if (page == nullptr || !seen_extent (page, view, &ext))
    return 0;                   /* nothing seen yet */

  /* Fit into the area under the button strip, less the margin. */
  room_w = dst->w - MAP_FRAME_MARGIN;
  room_h = dst->h - chrome_top (cam, dst->h) - MAP_FRAME_MARGIN;
  sx = fit_axis (ext.x1 - ext.x0, room_w);
  sy = fit_axis (ext.y1 - ext.y0, room_h);
  *fit_scale = clamp_int (sx < sy ? sx : sy, MAP_SCALE_MIN, MAP_SCALE_MAX);

  scale = zoom > 0 ? clamp_int (zoom, MAP_ZOOM_MIN, MAP_ZOOM_MAX)
                   : *fit_scale;
  cam->scale = scale;

  /*
   * Where an axis that overflows would like its centre: on the player while
   * following (LockPlayerCentre), otherwise wherever the last frame, or a
   * pan button since, left it.  A hidden or unseen room is not followed.
   */
  if (follow && pn != nullptr && !pn->hidden && view_seen (view, pn->key))
    {
      want_x = (int) ((pn->x + pn->w / 2.0) * scale);
      want_y = (int) ((pn->y + pn->h / 2.0) * scale);
    }
  else
    {
      want_x = cam->cx;
      want_y = cam->cy;
    }
  cam->cx = frame_axis (ext.x0, ext.x1, scale, room_w, want_x,
                        MAP_CHROME_PAN_L, MAP_CHROME_PAN_R, &pan);
  cam->cy = frame_axis (ext.y0, ext.y1, scale, room_h, want_y,
                        MAP_CHROME_PAN_U, MAP_CHROME_PAN_D, &pan);
  return pan;
}

void
map_frame (const map_t *map, const map_view_t *view,
           const char *player_key, const map_surface_t *dst,
           int zoom, int follow, map_camera_t *cam, map_chrome_t *chrome)
{
  unsigned int enabled = 0;
  int fit_scale = MAP_SCALE_MAX;

  if (cam == nullptr)
    return;
  cam->scale = MAP_SCALE_MIN;
  if (map != nullptr && dst != nullptr)
    enabled = frame_camera (map, view, player_key, dst, zoom, follow, cam,
                            &fit_scale);
  if (chrome == nullptr)
    return;

  if (cam->scale < MAP_ZOOM_MAX)
    enabled |= map_chrome_bit (MAP_CHROME_ZOOM_IN);
  if (cam->scale > MAP_ZOOM_MIN)
    enabled |= map_chrome_bit (MAP_CHROME_ZOOM_OUT);
  chrome->fit_scale = fit_scale;
  chrome->enabled = enabled;
}

void
map_pan (map_camera_t *cam, int w, int h, int button)
{
  /* A quarter of the view at a time, but never a crawl. */
  int dx = w / 4, dy = (h - chrome_top (cam, h)) / 4;

  if (dx < 16)
    dx = 16;
  if (dy < 16)
    dy = 16;
  switch (button)
    {
    case MAP_CHROME_PAN_L: cam->cx -= dx; break;
    case MAP_CHROME_PAN_R: cam->cx += dx; break;
    case MAP_CHROME_PAN_U: cam->cy -= dy; break;
    case MAP_CHROME_PAN_D: cam->cy += dy; break;
    default: break;
    }
}

/*
 * The manual zoom ladder ("glk zoom in/out"), from MAP_ZOOM_MIN to
 * MAP_ZOOM_MAX.  The automatic fit stays within MAP_SCALE_MIN and
 * MAP_SCALE_MAX, but a player asking to zoom in can usefully get closer than
 * the fit would, and out to see the whole of a big map at once; past 32 the
 * boxes stop gaining anything, and below 3 a room is smaller than its badges
 * and too small to click.
 */
static const int map_zoom_ladder[] = {
  MAP_ZOOM_MIN, 4, 5, 7, MAP_SCALE_MIN, 12, 16, 20, 26, MAP_ZOOM_MAX
};
#define MAP_ZOOM_LADDER_N \
  ((int) (sizeof map_zoom_ladder / sizeof map_zoom_ladder[0]))

int
map_zoom_step (int scale, int dir)
{
  const int *ladder = map_zoom_ladder;
  const int n = MAP_ZOOM_LADDER_N;
  int i;

  if (dir > 0)
    {
      for (i = 0; i < n; i++)
        if (ladder[i] > scale)
          return ladder[i];
    }
  else
    {
      for (i = n - 1; i >= 0; i--)
        if (ladder[i] < scale)
          return ladder[i];
    }
  return scale;
}


/*
 * ----------------------------------------------- floating pan/zoom chrome
 */

/* Even, for the even marks of draw_chrome_btn. */
#define MAP_BTN_SIZE 14
#define MAP_BTN_PAD 3
#define MAP_BTN_ALPHA 230       /* ~90% opaque */
#define MAP_BTN_MAX 6

#define MAP_CHROME_PAN_BITS \
  (map_chrome_bit (MAP_CHROME_PAN_L) | map_chrome_bit (MAP_CHROME_PAN_R) \
   | map_chrome_bit (MAP_CHROME_PAN_U) | map_chrome_bit (MAP_CHROME_PAN_D))

/* A button's top-left corner; they are all MAP_BTN_SIZE square. */
typedef struct {
  int id;
  int x, y;
} map_btn_t;

/*
 * Lay the button row out into `out` (room for MAP_BTN_MAX), right-aligned in
 * a window `win_w` wide, and return the count: the four pan buttons, then
 * zoom in and out.  The pan buttons are left out while the whole map fits,
 * which is to say while none of them is enabled.
 */
static int
map_chrome_layout (int win_w, const map_chrome_t *chrome, map_btn_t *out)
{
  static const int ids[MAP_BTN_MAX] = {
    MAP_CHROME_PAN_L, MAP_CHROME_PAN_R, MAP_CHROME_PAN_U, MAP_CHROME_PAN_D,
    MAP_CHROME_ZOOM_IN, MAP_CHROME_ZOOM_OUT
  };
  int pans = (chrome->enabled & MAP_CHROME_PAN_BITS) != 0;
  int first = pans ? 0 : 4, n = MAP_BTN_MAX - first;
  int i, x, total;

  /* Neighbours share their border, one pixel column. */
  total = n * MAP_BTN_SIZE - (n - 1);
  x = win_w - MAP_BTN_PAD - total;
  if (x < MAP_BTN_PAD)
    x = MAP_BTN_PAD;

  for (i = 0; i < n; i++)
    {
      out[i].id = ids[first + i];
      out[i].x = x;
      out[i].y = MAP_BTN_PAD;
      x += MAP_BTN_SIZE - 1;
    }
  return n;
}

/*
 * One pixel of a button's mark, at (u,v) on the MAP_BTN_SIZE - 2 square
 * inside its border.
 */
static void
chrome_mark_px (map_surface_t *s, const map_btn_t *b, int u, int v,
                unsigned int rgb, int alpha)
{
  blend (s, b->x + 1 + u, b->y + 1 + v, rgb, alpha);
}

static unsigned int
chrome_ink (int enabled)
{
  return enabled ? palette.fg : mix_rgb (palette.fg, palette.bg, 0.55);
}

/* One edge of a button's border, in the ink for `enabled`. */
static void
chrome_edge (map_surface_t *s, int x0, int y0, int x1, int y1, int enabled)
{
  fill_span (s, x0, y0, x1, y1, chrome_ink (enabled),
             enabled ? MAP_BTN_ALPHA : MAP_BTN_ALPHA / 2);
}

/*
 * A button's border, each pixel painted once so that the see-through ink is
 * even all round.  Its left edge is the previous button's right edge, if
 * there is one (`joined_left`), and so is not drawn again here; its right
 * edge, shared with the next button, is drawn live if either side is
 * (`right_enabled`).
 */
static void
draw_chrome_border (map_surface_t *s, const map_btn_t *b, int enabled,
                    int joined_left, int right_enabled)
{
  int x0 = b->x, y0 = b->y;
  int x1 = x0 + MAP_BTN_SIZE - 1, y1 = y0 + MAP_BTN_SIZE - 1;
  int from = joined_left ? x0 + 1 : x0;

  chrome_edge (s, from, y0, x1 - 1, y0, enabled);
  chrome_edge (s, from, y1, x1 - 1, y1, enabled);
  if (!joined_left)
    chrome_edge (s, x0, y0 + 1, x0, y1 - 1, enabled);
  chrome_edge (s, x1, y0, x1, y1, right_enabled);
}

/*
 * A button's face and its mark.  A greyed button stays in its place, so the
 * row does not shuffle about under the pointer as the view moves.
 */
static void
draw_chrome_btn (map_surface_t *s, const map_btn_t *b, int enabled)
{
  unsigned int face = mix_rgb (palette.bg, palette.fg, enabled ? 0.12 : 0.06);
  unsigned int ink = chrome_ink (enabled);
  int mid = (MAP_BTN_SIZE - 2) / 2; /* first pixel past the inside's centre */
  int mark_alpha = enabled ? 255 : 140;
  int i, k;

  /* Inside the border only, so that a shared border is not blended twice. */
  fill_rect (s, b->x + 1, b->y + 1, b->x + MAP_BTN_SIZE - 2,
             b->y + MAP_BTN_SIZE - 2, face, MAP_BTN_ALPHA);

  /*
   * The marks are drawn by hand rather than taken from the font, so that
   * each is an even number of pixels across and sits with the same gap on
   * either side.  An arrow is a triangle 4 deep and 8 wide, a plus 6 square
   * with strokes 2 thick: the same 20 pixels of ink apiece.
   */
  if (b->id == MAP_CHROME_ZOOM_IN || b->id == MAP_CHROME_ZOOM_OUT)
    {
      for (i = -3; i < 3; i++)
        for (k = -1; k < 1; k++)
          {
            chrome_mark_px (s, b, mid + i, mid + k, ink, mark_alpha);
            if (b->id == MAP_CHROME_ZOOM_IN && (i < -1 || i >= 1))
              chrome_mark_px (s, b, mid + k, mid + i, ink, mark_alpha);
          }
      return;
    }
  for (k = 0; k < 4; k++)       /* k steps from the base toward the tip */
    for (i = -(4 - k); i < 4 - k; i++)
      {
        int along = (b->id == MAP_CHROME_PAN_R || b->id == MAP_CHROME_PAN_D)
                    ? mid - 2 + k : mid + 1 - k;

        if (b->id == MAP_CHROME_PAN_L || b->id == MAP_CHROME_PAN_R)
          chrome_mark_px (s, b, along, mid + i, ink, mark_alpha);
        else
          chrome_mark_px (s, b, mid + i, along, ink, mark_alpha);
      }
}

void
map_chrome_draw (map_surface_t *dst, const map_chrome_t *chrome)
{
  map_btn_t btns[MAP_BTN_MAX];
  int n, i;

  if (dst == nullptr || chrome == nullptr)
    return;
  n = map_chrome_layout (dst->w, chrome, btns);
  for (i = 0; i < n; i++)
    draw_chrome_btn (dst, &btns[i], map_chrome_enabled (chrome, btns[i].id));
  for (i = 0; i < n; i++)
    {
      int on = map_chrome_enabled (chrome, btns[i].id);
      int next_on = i + 1 < n && map_chrome_enabled (chrome, btns[i + 1].id);

      draw_chrome_border (dst, &btns[i], on, i > 0, on || next_on);
    }
}

int
map_chrome_hit (int w, const map_chrome_t *chrome, int px, int py)
{
  map_btn_t btns[MAP_BTN_MAX];
  int n, i;

  if (chrome == nullptr)
    return MAP_CHROME_NONE;
  n = map_chrome_layout (w, chrome, btns);
  for (i = 0; i < n; i++)
    if (px >= btns[i].x && px < btns[i].x + MAP_BTN_SIZE
        && py >= btns[i].y && py < btns[i].y + MAP_BTN_SIZE)
      return btns[i].id;
  return MAP_CHROME_NONE;
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
static void
draw_dir_icon_site (map_surface_t *s, const proj_t *p, const map_node_t *n,
                    int dir, int site, int alpha, int dim)
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
 * The ADRIFT 4 runner had two pictures per icon: the normal one when the
 * destination has been seen (clicking it recentres the map there), and a
 * dimmed one when it has not (Form29.doicon, the seen-flag branch).
 */
static int
badge_dest_unseen (const map_view_t *view, const map_link_t *lk)
{
  return lk->dest == nullptr || !view_seen (view, lk->dest);
}

/*
 * Which badges a node wears, and on which half-wind / compass site.
 *
 * An In/Out connector is recorded on one room only, but the runner puts a
 * badge on both ends of it: DrawLinks finishes by drawing the opposite badge
 * on the destination node (Map.vb:1517), which is how the room you step into
 * gets its OUT even though the <Link> lives on the room outside.  `far` is
 * that badge; `own` is the one the node draws for a link of its own
 * (Map.vb:1326-1343), and only `own` answers to HasRouteInDirection.
 *
 * Up/Down get the same far-end treatment for A5 badge connectors.  Far-end
 * badges gate on the destination's Movements (bHasIn/Out/Up/Down), not on
 * duplex -- DrawInOutIcon (Map.vb:1535) -- so a one-way In still yields an
 * Out on a dest that has its own Out leading elsewhere.
 *
 * Spatterlight extension: when a badge exit shares its destination and
 * restriction style with a compass exit on the same node, the badge sits on
 * that compass port (`on_compass_*`) and its badge-to-badge connector is
 * omitted -- the compass link already draws the line.  A far badge that
 * arrives via a coincident compass exit parks on that link's
 * DestinationAnchor port (e.g. Behind Bar Down+South -> Cellar: D on South,
 * U on Cellar's North) so it meets the line instead of floating on a
 * half-wind.
 *
 * All fields are indexed by map_badge (Up, Down, In, Out).
 */
typedef struct {
  int edge[MAP_N_BADGES];              /* facing edges, pre-site           */
  int site[MAP_N_BADGES];              /* half-wind / compass sites        */
  unsigned char is_far[MAP_N_BADGES];
  unsigned char has[MAP_N_BADGES];
  unsigned char on_compass[MAP_N_BADGES];
  unsigned char site_fixed[MAP_N_BADGES];
} inout_badge_t;

/* The order the runner visits the badges in: In, Out, Up, Down. */
static const int badge_order[MAP_N_BADGES] = {
  DIR_IN, DIR_OUT, DIR_UP, DIR_DOWN
};

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
static inout_badge_t *
inout_layout (const map_page_t *page, const map_view_t *view)
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
       * In→In Longhouse as a Movement only.  Compass directions already get
       * pass-2 stubs the same way; badge dirs were skipped there.
       *
       * Up and Down follow Spatterlight's badge convention rather than those
       * lines literally: the runner has no Up/Down badge at all, so it sends
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
               * the same for eInEdge).  Missing/off-page → North default.
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

/* What the passes of map_render share. */
typedef struct {
  const map_t *map;
  const map_view_t *view;
  const map_page_t *page;
  const map_node_t *active;       /* The player's node, or NULL. */
  const inout_badge_t *badges;    /* inout_layout's sites; NULL for A3/A4. */
  const proj_t *p;
  map_surface_t *dst;
  int wd;                         /* Pen width. */
} render_ctx_t;

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
       * The ADRIFT 5 runner hides a restricted connector while its
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
   * Skip self-Down the way the runner does.  This sits below the route
   * gates because the runner reaches it with the pen already built: a
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
           * corners the runner uses.)
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
static void
render_links (const render_ctx_t *rc)
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
 * arrows.  The runner gates these on HasRouteInDirection AndAlso Not
 * HasSeenLocation (Map.vb DrawOutArrow) -- an exit back to a room already on
 * the map is already drawn as a connector, so it must not also get a stub.
 */
static void
render_exit_stubs (const render_ctx_t *rc)
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

/*
 * ADRIFT 3/4 badge sites: fixed half-winds, one per badge, arranged so that
 * each pair sits on opposite corners of the box -- Up at NNE and Down at SSW,
 * In at WNW and Out at ESE.  The half-wind enum is shared with A5 layout;
 * A3/A4 still do not chase compass stubs.  Indexed by map_badge().
 */
static const int a4_badge_site[MAP_N_BADGES] = {
  BADGE_NNE, BADGE_SSW, BADGE_WNW, BADGE_ESE
};

/*
 * Pass 3 of map_render for the node at index i: its room box, then its
 * label, then its badges on top.  At low scales a badge overlaps the name,
 * and the name's ink across it would make its mark unreadable.
 */
static void
render_node (const render_ctx_t *rc, int i, const char *player_key)
{
  const map_node_t *n = &rc->page->nodes[i];
  int x0, y0, x1, y1, alpha, is_player, is_far, k, l;
  const map_link_t *bl[MAP_N_BADGES] = { nullptr, nullptr, nullptr, nullptr };
  unsigned int fill;

  if (!view_seen (rc->view, n->key))
    return;
  if (n->hidden)
    return;               /* Location <Hide>: no box (Map.vb:1156) */

  x0 = px_x (rc->p, n->x);
  y0 = px_y (rc->p, n->y);
  x1 = px_x (rc->p, n->x + n->w);
  y1 = px_y (rc->p, n->y + n->h);
  if (x1 < 0 || y1 < 0 || x0 >= rc->dst->w || y0 >= rc->dst->h)
    return;               /* off-screen */

  is_player = (player_key != nullptr && n->key != nullptr
               && strcmp (n->key, player_key) == 0);

  /*
   * MAP_ROOM_FILL_ALPHA on the player's level, MAP_FAR_ROOM_ALPHA
   * elsewhere (Map.vb:1172-1194, which fades to 50).  The derived scheme
   * picked its label colours against the on-level blend.
   */
  is_far = (!is_player && rc->active != nullptr && n->z != rc->active->z);
  alpha = is_far ? MAP_FAR_ROOM_ALPHA : MAP_ROOM_FILL_ALPHA;

  fill = is_player ? palette.derived.here_fill : palette.derived.room_fill;
  fill_rect (rc->dst, x0, y0, x1, y1, fill, alpha);
  draw_rect (rc->dst, x0, y0, x1, y1,
             is_player ? palette.derived.here_stroke
                       : palette.derived.room_stroke,
             alpha);

  /* Below MAP_SCALE_MIN a name would be a smudge across the box. */
  if (rc->view != nullptr && rc->view->name != nullptr
      && rc->p->cam->scale >= MAP_SCALE_MIN)
    {
      const char *label = rc->view->name (rc->view->ctx, n->key);
      if (label != nullptr && label[0] != '\0')
        draw_label (rc->dst, label, x0, y0, x1, y1,
                    is_player ? palette.derived.here_label
                              : palette.derived.label,
                    is_far ? MAP_FAR_LABEL_ALPHA : 255);
    }

  for (l = 0; l < n->n_links; l++)
    if (map_is_badge_dir (n->links[l].dir))
      bl[map_badge (n->links[l].dir)] = &n->links[l];
  /*
   * Badge icons only show while the route is currently usable (the
   * HasRouteInDirection gates, Map.vb:1328/1337 for In/Out -- Grandpa's
   * Ranch's Living Room gains its OUT badge when the front door is opened).
   * ADRIFT 4 leaves ever_blocked NULL, so its badges always show.  The
   * matching connector gate is in pass 1, render_link.
   */
  if (rc->view != nullptr && rc->view->ever_blocked != nullptr
      && rc->view->exit_dest != nullptr)
    {
      for (k = 0; k < MAP_N_BADGES; k++)
        {
          int dir = badge_order[k];
          if (bl[map_badge (dir)] != nullptr
              && rc->view->exit_dest (rc->view->ctx, n->key, dir) == nullptr)
            bl[map_badge (dir)] = nullptr;
        }
    }
  /*
   * A3/A4: opaque, and dimmed when the destination is unseen.
   * A5: opaque on the player's level; fades with the card off-level
   * (there is no second level on A4's flat map).
   */
  if (rc->badges == nullptr)
    {
      /* A3/A4: fixed sites (U NNE, D SSW, I WNW, O ESE). */
      for (k = 0; k < MAP_N_BADGES; k++)
        {
          int dir = badge_order[k];
          const map_link_t *lk = bl[map_badge (dir)];
          if (lk != nullptr)
            draw_dir_icon_site (rc->dst, rc->p, n, dir,
                                a4_badge_site[map_badge (dir)], 255,
                                badge_dest_unseen (rc->view, lk));
        }
    }
  else
    {
      /*
       * A5 uses the sites inout_layout resolved.  Draw for: an own Link
       * badge (bl[] non-NULL after the route gate), a far badge from
       * somebody else's Link (DrawLinks, not route-gated), or a
       * Movement-only badge toward an unseen room (no <Link>, Map.vb
       * DrawNode stub path -- has[] set without a matching SourceAnchor
       * Link).  A Link whose route is currently blocked leaves has[] set
       * but bl[] NULL; those must stay hidden.
       */
      int badge_a = is_far ? MAP_FAR_ROOM_ALPHA : 255;
      for (k = 0; k < MAP_N_BADGES; k++)
        {
          int dir = badge_order[k];
          const map_link_t *lk = bl[map_badge (dir)];
          const inout_badge_t *x = &rc->badges[i];
          if (lk != nullptr || x->is_far[map_badge (dir)]
              || (x->has[map_badge (dir)]
                  && find_dir_link (n, dir) == nullptr))
            draw_dir_icon_site (rc->dst, rc->p, n, dir,
                                x->site[map_badge (dir)], badge_a, 0);
        }
    }
}

void
map_render (const map_t *map, const map_view_t *view,
            const char *player_key, const map_camera_t *cam,
            map_surface_t *dst)
{
  const map_page_t *page;
  inout_badge_t *badges;
  render_ctx_t rc;
  proj_t p;
  int i, wd;

  if (dst == nullptr)
    return;
  ensure_derived_palette ();
  fill_surface (dst, palette.bg);
  if (map == nullptr || cam == nullptr)
    return;
  page = page_by_key (map, cam->page);
  if (page == nullptr)
    return;

  proj_init (&p, cam, dst->w, dst->h);
  wd = cam->scale / 5;          /* Map.vb:1433, pen width = iScale / 5 */
  if (wd < 1)
    wd = 1;
  badges = inout_layout (page, view);

  rc.map = map;
  rc.view = view;
  rc.page = page;
  rc.active = map_find (map, player_key);
  rc.badges = badges;
  rc.p = &p;
  rc.dst = dst;
  rc.wd = wd;

  render_links (&rc);
  render_exit_stubs (&rc);
  for (i = 0; i < page->n_nodes; i++)
    render_node (&rc, i, player_key);

  free (badges);
}


/*
 * ------------------------------------------------ hit testing and walking
 */

const char *
map_hit (const map_t *map, const map_view_t *view,
         const map_camera_t *cam, int w, int h, int mx, int my)
{
  const map_page_t *page;
  proj_t p;
  int i;

  if (map == nullptr || cam == nullptr || w <= 0 || h <= 0)
    return nullptr;
  page = page_by_key (map, cam->page);
  if (page == nullptr)
    return nullptr;
  proj_init (&p, cam, w, h);

  for (i = page->n_nodes - 1; i >= 0; i--)
    {
      const map_node_t *n = &page->nodes[i];
      int x0, y0, x1, y1;
      if (!view_seen (view, n->key))
        continue;
      x0 = px_x (&p, n->x);
      y0 = px_y (&p, n->y);
      x1 = px_x (&p, n->x + n->w);
      y1 = px_y (&p, n->y + n->h);
      if (mx >= x0 && mx <= x1 && my >= y0 && my <= y1)
        return n->key;
    }
  return nullptr;
}

int
map_hit_dir (const map_t *map, const map_view_t *view,
             const map_camera_t *cam, int w, int h,
             const char *player_key, int mx, int my)
{
  const map_page_t *page;
  const map_node_t *n;
  const char *dest;
  proj_t p;
  int x0, y0, x1, y1, reach_x, reach_y, sx, sy, d;

  if (map == nullptr || view == nullptr || view->exit_dest == nullptr
      || cam == nullptr || player_key == nullptr || w <= 0 || h <= 0)
    return -1;
  page = page_by_key (map, cam->page);
  n = page_node (page, player_key);
  if (n == nullptr || n->hidden)
    return -1;                  /* no box to click around */
  proj_init (&p, cam, w, h);

  x0 = px_x (&p, n->x);
  y0 = px_y (&p, n->y);
  x1 = px_x (&p, n->x + n->w);
  y1 = px_y (&p, n->y + n->h);
  /*
   * One room's pitch out from the box: the gap the ADRIFT 4 layout leaves
   * between two rooms, half a box, and the neighbouring box itself.
   */
  reach_x = round_px (n->w * 1.5 * cam->scale);
  reach_y = round_px (n->h * 1.5 * cam->scale);
  if (mx < x0 - reach_x || mx > x1 + reach_x
      || my < y0 - reach_y || my > y1 + reach_y)
    return -1;

  sx = (mx < x0) ? -1 : (mx > x1) ? 1 : 0;
  sy = (my < y0) ? -1 : (my > y1) ? 1 : 0;
  if (sx == 0 && sy == 0)
    return -1;                  /* on the box itself */
  for (d = 0; d < MAP_N_DIRS; d++)
    if (!map_is_badge_dir (d) && map_dir_dx[d] == sx && map_dir_dy[d] == sy)
      break;
  if (d == MAP_N_DIRS)
    return -1;

  dest = view->exit_dest (view->ctx, player_key, d);
  return (dest != nullptr && dest[0] != '\0') ? d : -1;
}

/*
 * clsCharacter.Dijkstra, with the runner's two constraints: an edge exists
 * only where HasRouteInDirection says the exit is currently usable (the
 * restrictions are applied), and only rooms the player has already seen are
 * walkable.  Every edge costs 1, so Dijkstra on a unit graph is a
 * breadth-first search; the queue below is one.
 */
int
map_walk_step (const map_view_t *view, const char *from, const char *to)
{
  std::vector<std::string> order;      /* discovered rooms, in BFS order   */
  std::map<std::string, int> prev;     /* index in `order` of predecessor  */
  std::map<std::string, int> via;      /* direction taken to reach the room */
  size_t head = 0;

  if (view == nullptr || view->exit_dest == nullptr
      || from == nullptr || to == nullptr)
    return -1;
  if (strcmp (from, to) == 0)
    return -1;                  /* already there */

  order.push_back (from);
  prev[from] = -1;
  via[from] = -1;

  while (head < order.size ())
    {
      std::string u = order[head++];
      int d;

      for (d = 0; d < MAP_N_DIRS; d++)
        {
          const char *dest = view->exit_dest (view->ctx, u.c_str (), d);

          if (dest == nullptr || dest[0] == '\0')
            continue;
          if (!view_seen (view, dest))
            continue;           /* the runner will not walk you through
                                   somewhere you have never been */
          if (prev.find (dest) != prev.end ())
            continue;           /* already reached, and by a shorter route */

          prev[dest] = (int) head - 1;
          via[dest] = d;
          if (strcmp (dest, to) == 0)
            {
              /*
               * Found it.  Walk the chain back to the room whose predecessor
               * is the start (order[0]), and take the direction that left
               * it.
               */
              std::string cur = dest;
              while (prev[cur] != 0)
                {
                  int p = prev[cur];
                  if (p < 0)
                    return -1;  /* only `from` has no predecessor */
                  cur = order[p];
                }
              return via[cur];
            }
          order.push_back (dest);
        }
    }
  return -1;                    /* unreachable through seen rooms */
}
