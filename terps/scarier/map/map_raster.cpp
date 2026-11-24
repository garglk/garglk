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
 * The map's palette and its software rasteriser: the colours derived from the
 * host's text style, and the primitives and bitmap text the renderer draws
 * with.  See map_internal.h for what the rest of map/ uses.
 */

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#include "mapdraw.h"
#include "map_internal.h"

using namespace map_internal;


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

map_palette_t map_internal::palette;


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

unsigned int
map_internal::mix_rgb (unsigned int a, unsigned int b, double t)
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

void
map_internal::ensure_derived_palette (void)
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

void
map_internal::blend (map_surface_t *s, int x, int y, unsigned int rgb,
                     int alpha)
{
  rgbsurf_blend (s, x, y, rgb, alpha);
}

void
map_internal::fill_surface (map_surface_t *s, unsigned int rgb)
{
  size_t i, n = (size_t) s->w * s->h;
  for (i = 0; i < n; i++)
    s->px[i] = rgb & 0xFFFFFF;
}

/* [x0,x1] x [y0,y1] inclusive, as given (an inverted range is empty). */
void
map_internal::fill_span (map_surface_t *s, int x0, int y0, int x1, int y1,
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
void
map_internal::fill_rect (map_surface_t *s, int x0, int y0, int x1, int y1,
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
void
map_internal::draw_rect (map_surface_t *s, int x0, int y0, int x1, int y1,
                         unsigned int rgb, int alpha)
{
  fill_span (s, x0, y0, x1, y0, rgb, alpha);
  fill_span (s, x0, y1, x1, y1, rgb, alpha);
  fill_span (s, x0, y0, x0, y1, rgb, alpha);
  fill_span (s, x1, y0, x1, y1, rgb, alpha);
}

/*
 * Bresenham with a round nib of radius `wd/2`.  `dash` != 0 draws a dotted
 * pen (DashStyle.Dot): the Runner's dot pattern is on/off in units of the pen
 * width.
 */
void
map_internal::draw_line (map_surface_t *s, int x0, int y0, int x1, int y1,
                         int wd, unsigned int rgb, int alpha, int dash)
{
  rgbsurf_line (s, x0, y0, x1, y1, wd, RGBSURF_NIB_ROUND, rgb, alpha, dash);
}

/*
 * Round to the nearest pixel.  floor() rather than a bare (int) cast: the cast
 * truncates toward zero, which rounds every negative coordinate one pixel
 * toward the origin and so closes the map up by a pixel where it crosses
 * zero.
 */
int
map_internal::round_px (double v)
{
  return (int) floor (v + 0.5);
}

/* Flatten a cubic Bezier (GDI+ DrawBezier) and stroke it. */
void
map_internal::draw_bezier (map_surface_t *s, double x0, double y0, double x1,
                           double y1, double x2, double y2, double x3,
                           double y3, int wd, unsigned int rgb, int alpha,
                           int dash, int *dash_phase)
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
void
map_internal::draw_curve (map_surface_t *s, const double *pts, int n, int wd,
                          unsigned int rgb, int alpha, int dash,
                          int *dash_phase)
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

void
map_internal::fill_circle (map_surface_t *s, int cx, int cy, int r,
                           unsigned int rgb, int alpha)
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
  const double s3 = 0.8660254037844386; /* sqrt(3)/2 */
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
   * circle that is nearest the equilateral one, (sqrt(3)/2, 1/2) of the way
   * out, with its distance in from the circle counted three times over.
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

void
map_internal::fill_ud_triangle (map_surface_t *s, int cx, int cy, int r,
                                int up, unsigned int rgb, int alpha)
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
void
map_internal::draw_arrowhead (map_surface_t *s, double x, double y, double dx,
                              double dy, int size, unsigned int rgb, int alpha)
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
 * plus Map.vb's GetFont auto-fit: the Runner binary-searches a font size that
 * makes the name fit, and since a Glk graphics window has no text at all, we
 * do the bitmap equivalent -- lay the name out at 8x8, and drop to the 5x7
 * face when that would split a word or overflow the box.
 */
void
map_internal::draw_label (map_surface_t *s, const char *text, int x0, int y0,
                          int x1, int y1, unsigned int rgb, int alpha)
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
