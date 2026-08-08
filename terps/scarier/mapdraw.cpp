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

/* The map model and its rasteriser, shared by both engines.  See mapdraw.h.

   Geometry follows the Adrift 5 runner's Map.vb, and the software
   rasteriser below stands in for the GDI+ calls it makes (FillPolygon /
   DrawBezier / DrawEllipse / DrawString).  The ADRIFT 4 runner draws its map
   out of VB control arrays instead, with a look of its own; we render both
   engines' maps the same way rather than carrying two rasterisers, so what
   differs between them is only where the nodes come from -- authored in
   ADRIFT 5 (a5map.cpp), computed from the exit graph in ADRIFT 4
   (scmap.cpp). */

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "mapdraw.h"

/* DirectionsEnum order (Global.vb:1468), as in a5expr.cpp.  ADRIFT 4 numbers
   its directions the same way (run400 Form29.opp), so the enum serves both. */
const char *const map_dirs[MAP_N_DIRS] = {
  "North", "East", "South", "West", "Up", "Down",
  "In", "Out", "NorthEast", "SouthEast", "SouthWest", "NorthWest"
};

/* Map.vb:33-39 paints a fixed pastel palette.  We colour the map from the
   host's text style instead, so the pane matches the story text in any theme:
   the map and its room boxes take the style's background colour, connectors,
   borders and labels its text colour, and the player's room is drawn inverted
   (text-colour fill, background-colour label) where the runner filled it
   yellow.  The host passes the two colours in (map_set_palette); until it
   does, black on white.  Only the badge accents survive from the
   runner's palette. */
static unsigned int map_bg = 0xFFFFFF;
static unsigned int map_fg = 0x000000;
#define ICON_IN        0x00A000
#define ICON_OUT       0xE06090
#define ICON_UP        0xD0A000
#define ICON_DOWN      0x4060D0

void
map_set_palette (unsigned int background, unsigned int text)
{
  map_bg = background & 0xFFFFFF;
  map_fg = text & 0xFFFFFF;
}

void
map_free (map_t *map)
{
  int p, n, l;
  if (map == NULL)
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

const map_node_t *
map_find (const map_t *map, const char *lockey)
{
  int p, n;
  if (map == NULL || lockey == NULL)
    return NULL;
  for (p = 0; p < map->n_pages; p++)
    for (n = 0; n < map->pages[p].n_nodes; n++)
      {
        const map_node_t *node = &map->pages[p].nodes[n];
        if (node->key != NULL && strcmp (node->key, lockey) == 0)
          return node;
      }
  return NULL;
}

static const map_page_t *
page_by_key (const map_t *map, int key)
{
  int p;
  for (p = 0; p < map->n_pages; p++)
    if (map->pages[p].key == key)
      return &map->pages[p];
  return NULL;
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
    return NULL;
  s = (map_surface_t *) calloc (1, sizeof (map_surface_t));
  if (s == NULL)
    return NULL;
  s->w = w;
  s->h = h;
  s->px = (unsigned int *) calloc ((size_t) w * (size_t) h,
                                   sizeof (unsigned int));
  if (s->px == NULL)
    {
      free (s);
      return NULL;
    }
  return s;
}

void
map_surface_free (map_surface_t *s)
{
  if (s == NULL)
    return;
  free (s->px);
  free (s);
}

static void
blend (map_surface_t *s, int x, int y, unsigned int rgb, int alpha)
{
  unsigned int dst;
  int r, g, b, dr, dg, db;
  if (x < 0 || y < 0 || x >= s->w || y >= s->h || alpha <= 0)
    return;
  if (alpha >= 255)
    {
      s->px[(size_t) y * s->w + x] = rgb & 0xFFFFFF;
      return;
    }
  dst = s->px[(size_t) y * s->w + x];
  r = (int) ((rgb >> 16) & 0xFF);
  g = (int) ((rgb >> 8) & 0xFF);
  b = (int) (rgb & 0xFF);
  dr = (int) ((dst >> 16) & 0xFF);
  dg = (int) ((dst >> 8) & 0xFF);
  db = (int) (dst & 0xFF);
  r = (r * alpha + dr * (255 - alpha)) / 255;
  g = (g * alpha + dg * (255 - alpha)) / 255;
  b = (b * alpha + db * (255 - alpha)) / 255;
  s->px[(size_t) y * s->w + x] =
    ((unsigned int) r << 16) | ((unsigned int) g << 8) | (unsigned int) b;
}

static void
fill_surface (map_surface_t *s, unsigned int rgb)
{
  size_t i, n = (size_t) s->w * s->h;
  for (i = 0; i < n; i++)
    s->px[i] = rgb & 0xFFFFFF;
}

static void
fill_rect (map_surface_t *s, int x0, int y0, int x1, int y1,
           unsigned int rgb, int alpha)
{
  int x, y;
  if (x1 < x0)
    { int t = x0; x0 = x1; x1 = t; }
  if (y1 < y0)
    { int t = y0; y0 = y1; y1 = t; }
  for (y = y0; y <= y1; y++)
    for (x = x0; x <= x1; x++)
      blend (s, x, y, rgb, alpha);
}

static void
draw_rect (map_surface_t *s, int x0, int y0, int x1, int y1,
           unsigned int rgb, int alpha)
{
  int x, y;
  for (x = x0; x <= x1; x++)
    {
      blend (s, x, y0, rgb, alpha);
      blend (s, x, y1, rgb, alpha);
    }
  for (y = y0; y <= y1; y++)
    {
      blend (s, x0, y, rgb, alpha);
      blend (s, x1, y, rgb, alpha);
    }
}

/* A dot of radius `wd/2`, used as the pen nib when stroking. */
static void
nib (map_surface_t *s, int x, int y, int wd, unsigned int rgb, int alpha)
{
  int dx, dy, r;
  if (wd <= 1)
    {
      blend (s, x, y, rgb, alpha);
      return;
    }
  r = wd / 2;
  for (dy = -r; dy <= r; dy++)
    for (dx = -r; dx <= r; dx++)
      if (dx * dx + dy * dy <= r * r + 1)
        blend (s, x + dx, y + dy, rgb, alpha);
}

/* Bresenham with a round nib.  `dash` != 0 draws a dotted pen (DashStyle.Dot):
   the runner's dot pattern is on/off in units of the pen width. */
static void
draw_line (map_surface_t *s, int x0, int y0, int x1, int y1, int wd,
           unsigned int rgb, int alpha, int dash)
{
  int dx = abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2;
  int step = 0;
  int period = (wd > 0 ? wd : 1) * 3;

  for (;;)
    {
      if (!dash || (step % period) < period / 2)
        nib (s, x0, y0, wd, rgb, alpha);
      if (x0 == x1 && y0 == y1)
        break;
      e2 = 2 * err;
      if (e2 >= dy)
        { err += dy; x0 += sx; }
      if (e2 <= dx)
        { err += dx; y0 += sy; }
      step++;
    }
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
  int phase = dash_phase != NULL ? *dash_phase : 0;
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
      int cx = (int) (bx + 0.5), cy = (int) (by + 0.5);
      if (i > 0)
        {
          /* Stroke the segment, keeping the dash phase continuous along the
             whole curve rather than restarting it at every flattened step. */
          int dx = abs (cx - px), dy = abs (cy - py);
          int seg = (dx > dy ? dx : dy);
          if (!dash)
            draw_line (s, px, py, cx, cy, wd, rgb, alpha, 0);
          else if ((phase % period) < period / 2)
            draw_line (s, px, py, cx, cy, wd, rgb, alpha, 0);
          phase += seg > 0 ? seg : 1;
        }
      px = cx;
      py = cy;
    }
  if (dash_phase != NULL)
    *dash_phase = phase;
}

static void
fill_circle (map_surface_t *s, int cx, int cy, int r, unsigned int rgb,
             int alpha)
{
  int dx, dy;
  for (dy = -r; dy <= r; dy++)
    for (dx = -r; dx <= r; dx++)
      if (dx * dx + dy * dy <= r * r)
        blend (s, cx + dx, cy + dy, rgb, alpha);
}

/* A filled arrow head at (x,y) pointing along (dx,dy) -- GDI+
   AdjustableArrowCap(4,4), used for one-way links. */
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
        blend (s, (int) (bx + nx * j + 0.5), (int) (by + ny * j + 0.5),
               rgb, alpha);
    }
}

/* --- 8x8 bitmap text (font8x8_basic, public domain: Marcel Sondaar).
   Glk graphics windows cannot render text, so room labels are drawn as
   glyphs.  LSB of each row byte is the leftmost pixel. */
static const unsigned char kFont8x8[95][8] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* space */
  { 0x18, 0x3c, 0x3c, 0x18, 0x18, 0x00, 0x18, 0x00 },  /* '!' */
  { 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* '"' */
  { 0x36, 0x36, 0x7f, 0x36, 0x7f, 0x36, 0x36, 0x00 },  /* '#' */
  { 0x0c, 0x3e, 0x03, 0x1e, 0x30, 0x1f, 0x0c, 0x00 },  /* '$' */
  { 0x00, 0x63, 0x33, 0x18, 0x0c, 0x66, 0x63, 0x00 },  /* '%' */
  { 0x1c, 0x36, 0x1c, 0x6e, 0x3b, 0x33, 0x6e, 0x00 },  /* '&' */
  { 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* '\'' */
  { 0x18, 0x0c, 0x06, 0x06, 0x06, 0x0c, 0x18, 0x00 },  /* '(' */
  { 0x06, 0x0c, 0x18, 0x18, 0x18, 0x0c, 0x06, 0x00 },  /* ')' */
  { 0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00 },  /* '*' */
  { 0x00, 0x0c, 0x0c, 0x3f, 0x0c, 0x0c, 0x00, 0x00 },  /* '+' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x06 },  /* ',' */
  { 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00 },  /* '-' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x00 },  /* '.' */
  { 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x01, 0x00 },  /* '/' */
  { 0x3e, 0x63, 0x73, 0x7b, 0x6f, 0x67, 0x3e, 0x00 },  /* '0' */
  { 0x0c, 0x0e, 0x0c, 0x0c, 0x0c, 0x0c, 0x3f, 0x00 },  /* '1' */
  { 0x1e, 0x33, 0x30, 0x1c, 0x06, 0x33, 0x3f, 0x00 },  /* '2' */
  { 0x1e, 0x33, 0x30, 0x1c, 0x30, 0x33, 0x1e, 0x00 },  /* '3' */
  { 0x38, 0x3c, 0x36, 0x33, 0x7f, 0x30, 0x78, 0x00 },  /* '4' */
  { 0x3f, 0x03, 0x1f, 0x30, 0x30, 0x33, 0x1e, 0x00 },  /* '5' */
  { 0x1c, 0x06, 0x03, 0x1f, 0x33, 0x33, 0x1e, 0x00 },  /* '6' */
  { 0x3f, 0x33, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x00 },  /* '7' */
  { 0x1e, 0x33, 0x33, 0x1e, 0x33, 0x33, 0x1e, 0x00 },  /* '8' */
  { 0x1e, 0x33, 0x33, 0x3e, 0x30, 0x18, 0x0e, 0x00 },  /* '9' */
  { 0x00, 0x0c, 0x0c, 0x00, 0x00, 0x0c, 0x0c, 0x00 },  /* ':' */
  { 0x00, 0x0c, 0x0c, 0x00, 0x00, 0x0c, 0x0c, 0x06 },  /* ';' */
  { 0x18, 0x0c, 0x06, 0x03, 0x06, 0x0c, 0x18, 0x00 },  /* '<' */
  { 0x00, 0x00, 0x3f, 0x00, 0x00, 0x3f, 0x00, 0x00 },  /* '=' */
  { 0x06, 0x0c, 0x18, 0x30, 0x18, 0x0c, 0x06, 0x00 },  /* '>' */
  { 0x1e, 0x33, 0x30, 0x18, 0x0c, 0x00, 0x0c, 0x00 },  /* '?' */
  { 0x3e, 0x63, 0x7b, 0x7b, 0x7b, 0x03, 0x1e, 0x00 },  /* '@' */
  { 0x0c, 0x1e, 0x33, 0x33, 0x3f, 0x33, 0x33, 0x00 },  /* 'A' */
  { 0x3f, 0x66, 0x66, 0x3e, 0x66, 0x66, 0x3f, 0x00 },  /* 'B' */
  { 0x3c, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3c, 0x00 },  /* 'C' */
  { 0x1f, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1f, 0x00 },  /* 'D' */
  { 0x7f, 0x46, 0x16, 0x1e, 0x16, 0x46, 0x7f, 0x00 },  /* 'E' */
  { 0x7f, 0x46, 0x16, 0x1e, 0x16, 0x06, 0x0f, 0x00 },  /* 'F' */
  { 0x3c, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7c, 0x00 },  /* 'G' */
  { 0x33, 0x33, 0x33, 0x3f, 0x33, 0x33, 0x33, 0x00 },  /* 'H' */
  { 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00 },  /* 'I' */
  { 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1e, 0x00 },  /* 'J' */
  { 0x67, 0x66, 0x36, 0x1e, 0x36, 0x66, 0x67, 0x00 },  /* 'K' */
  { 0x0f, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7f, 0x00 },  /* 'L' */
  { 0x63, 0x77, 0x7f, 0x7f, 0x6b, 0x63, 0x63, 0x00 },  /* 'M' */
  { 0x63, 0x67, 0x6f, 0x7b, 0x73, 0x63, 0x63, 0x00 },  /* 'N' */
  { 0x1c, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1c, 0x00 },  /* 'O' */
  { 0x3f, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x0f, 0x00 },  /* 'P' */
  { 0x1e, 0x33, 0x33, 0x33, 0x3b, 0x1e, 0x38, 0x00 },  /* 'Q' */
  { 0x3f, 0x66, 0x66, 0x3e, 0x36, 0x66, 0x67, 0x00 },  /* 'R' */
  { 0x1e, 0x33, 0x07, 0x0e, 0x38, 0x33, 0x1e, 0x00 },  /* 'S' */
  { 0x3f, 0x2d, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00 },  /* 'T' */
  { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3f, 0x00 },  /* 'U' */
  { 0x33, 0x33, 0x33, 0x33, 0x33, 0x1e, 0x0c, 0x00 },  /* 'V' */
  { 0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00 },  /* 'W' */
  { 0x63, 0x63, 0x36, 0x1c, 0x1c, 0x36, 0x63, 0x00 },  /* 'X' */
  { 0x33, 0x33, 0x33, 0x1e, 0x0c, 0x0c, 0x1e, 0x00 },  /* 'Y' */
  { 0x7f, 0x63, 0x31, 0x18, 0x4c, 0x66, 0x7f, 0x00 },  /* 'Z' */
  { 0x1e, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1e, 0x00 },  /* '[' */
  { 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x40, 0x00 },  /* '\\' */
  { 0x1e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1e, 0x00 },  /* ']' */
  { 0x08, 0x1c, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00 },  /* '^' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff },  /* '_' */
  { 0x0c, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* '`' */
  { 0x00, 0x00, 0x1e, 0x30, 0x3e, 0x33, 0x6e, 0x00 },  /* 'a' */
  { 0x07, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x3b, 0x00 },  /* 'b' */
  { 0x00, 0x00, 0x1e, 0x33, 0x03, 0x33, 0x1e, 0x00 },  /* 'c' */
  { 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6e, 0x00 },  /* 'd' */
  { 0x00, 0x00, 0x1e, 0x33, 0x3f, 0x03, 0x1e, 0x00 },  /* 'e' */
  { 0x1c, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0f, 0x00 },  /* 'f' */
  { 0x00, 0x00, 0x6e, 0x33, 0x33, 0x3e, 0x30, 0x1f },  /* 'g' */
  { 0x07, 0x06, 0x36, 0x6e, 0x66, 0x66, 0x67, 0x00 },  /* 'h' */
  { 0x0c, 0x00, 0x0e, 0x0c, 0x0c, 0x0c, 0x1e, 0x00 },  /* 'i' */
  { 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1e },  /* 'j' */
  { 0x07, 0x06, 0x66, 0x36, 0x1e, 0x36, 0x67, 0x00 },  /* 'k' */
  { 0x0e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00 },  /* 'l' */
  { 0x00, 0x00, 0x33, 0x7f, 0x7f, 0x6b, 0x63, 0x00 },  /* 'm' */
  { 0x00, 0x00, 0x1f, 0x33, 0x33, 0x33, 0x33, 0x00 },  /* 'n' */
  { 0x00, 0x00, 0x1e, 0x33, 0x33, 0x33, 0x1e, 0x00 },  /* 'o' */
  { 0x00, 0x00, 0x3b, 0x66, 0x66, 0x3e, 0x06, 0x0f },  /* 'p' */
  { 0x00, 0x00, 0x6e, 0x33, 0x33, 0x3e, 0x30, 0x78 },  /* 'q' */
  { 0x00, 0x00, 0x3b, 0x6e, 0x66, 0x06, 0x0f, 0x00 },  /* 'r' */
  { 0x00, 0x00, 0x3e, 0x03, 0x1e, 0x30, 0x1f, 0x00 },  /* 's' */
  { 0x08, 0x0c, 0x3e, 0x0c, 0x0c, 0x2c, 0x18, 0x00 },  /* 't' */
  { 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6e, 0x00 },  /* 'u' */
  { 0x00, 0x00, 0x33, 0x33, 0x33, 0x1e, 0x0c, 0x00 },  /* 'v' */
  { 0x00, 0x00, 0x63, 0x6b, 0x7f, 0x7f, 0x36, 0x00 },  /* 'w' */
  { 0x00, 0x00, 0x63, 0x36, 0x1c, 0x36, 0x63, 0x00 },  /* 'x' */
  { 0x00, 0x00, 0x33, 0x33, 0x33, 0x3e, 0x30, 0x1f },  /* 'y' */
  { 0x00, 0x00, 0x3f, 0x19, 0x0c, 0x26, 0x3f, 0x00 },  /* 'z' */
  { 0x38, 0x0c, 0x0c, 0x07, 0x0c, 0x0c, 0x38, 0x00 },  /* '{' */
  { 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00 },  /* '|' */
  { 0x07, 0x0c, 0x0c, 0x38, 0x0c, 0x0c, 0x07, 0x00 },  /* '}' */
  { 0x6e, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }   /* '~' */
};

/* A smaller face, for room names too long to fit their box at 8x8.  Map.vb
   binary-searches a GDI+ font size until the name fits (GetFont); a Glk
   graphics window has no text at all, so instead we carry two bitmap faces and
   drop to the small one when the big one will not fit.  5x7, authored for this
   purpose. */
static const unsigned char kFont5x7[95][7] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* space */
  { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 },  /* '!' */
  { 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* '"' */
  { 0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a },  /* '#' */
  { 0x04, 0x1e, 0x05, 0x0e, 0x14, 0x0f, 0x04 },  /* '$' */
  { 0x13, 0x13, 0x08, 0x04, 0x02, 0x19, 0x19 },  /* '%' */
  { 0x06, 0x09, 0x05, 0x02, 0x15, 0x09, 0x16 },  /* '&' */
  { 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* '\'' */
  { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 },  /* '(' */
  { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 },  /* ')' */
  { 0x00, 0x15, 0x0e, 0x1f, 0x0e, 0x15, 0x00 },  /* '*' */
  { 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00 },  /* '+' */
  { 0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x02 },  /* ',' */
  { 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00 },  /* '-' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06 },  /* '.' */
  { 0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01 },  /* '/' */
  { 0x0e, 0x11, 0x19, 0x15, 0x13, 0x11, 0x0e },  /* '0' */
  { 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x0e },  /* '1' */
  { 0x0e, 0x11, 0x10, 0x08, 0x04, 0x02, 0x1f },  /* '2' */
  { 0x1f, 0x08, 0x04, 0x08, 0x10, 0x11, 0x0e },  /* '3' */
  { 0x08, 0x0c, 0x0a, 0x09, 0x1f, 0x08, 0x08 },  /* '4' */
  { 0x1f, 0x01, 0x0f, 0x10, 0x10, 0x11, 0x0e },  /* '5' */
  { 0x0c, 0x02, 0x01, 0x0f, 0x11, 0x11, 0x0e },  /* '6' */
  { 0x1f, 0x10, 0x08, 0x04, 0x02, 0x02, 0x02 },  /* '7' */
  { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e },  /* '8' */
  { 0x0e, 0x11, 0x11, 0x1e, 0x10, 0x08, 0x06 },  /* '9' */
  { 0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00 },  /* ':' */
  { 0x00, 0x06, 0x06, 0x00, 0x06, 0x04, 0x02 },  /* ';' */
  { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 },  /* '<' */
  { 0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00 },  /* '=' */
  { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 },  /* '>' */
  { 0x0e, 0x11, 0x10, 0x08, 0x04, 0x00, 0x04 },  /* '?' */
  { 0x0e, 0x11, 0x1d, 0x15, 0x1d, 0x01, 0x0e },  /* '@' */
  { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 },  /* 'A' */
  { 0x0f, 0x11, 0x11, 0x0f, 0x11, 0x11, 0x0f },  /* 'B' */
  { 0x0e, 0x11, 0x01, 0x01, 0x01, 0x11, 0x0e },  /* 'C' */
  { 0x07, 0x09, 0x11, 0x11, 0x11, 0x09, 0x07 },  /* 'D' */
  { 0x1f, 0x01, 0x01, 0x0f, 0x01, 0x01, 0x1f },  /* 'E' */
  { 0x1f, 0x01, 0x01, 0x0f, 0x01, 0x01, 0x01 },  /* 'F' */
  { 0x0e, 0x11, 0x01, 0x1d, 0x11, 0x11, 0x0e },  /* 'G' */
  { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 },  /* 'H' */
  { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e },  /* 'I' */
  { 0x1c, 0x08, 0x08, 0x08, 0x08, 0x09, 0x06 },  /* 'J' */
  { 0x11, 0x09, 0x05, 0x03, 0x05, 0x09, 0x11 },  /* 'K' */
  { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1f },  /* 'L' */
  { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 },  /* 'M' */
  { 0x11, 0x13, 0x15, 0x15, 0x19, 0x11, 0x11 },  /* 'N' */
  { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e },  /* 'O' */
  { 0x0f, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x01 },  /* 'P' */
  { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x09, 0x16 },  /* 'Q' */
  { 0x0f, 0x11, 0x11, 0x0f, 0x05, 0x09, 0x11 },  /* 'R' */
  { 0x0e, 0x11, 0x01, 0x0e, 0x10, 0x11, 0x0e },  /* 'S' */
  { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },  /* 'T' */
  { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e },  /* 'U' */
  { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 },  /* 'V' */
  { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11 },  /* 'W' */
  { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 },  /* 'X' */
  { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 },  /* 'Y' */
  { 0x1f, 0x10, 0x08, 0x04, 0x02, 0x01, 0x1f },  /* 'Z' */
  { 0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e },  /* '[' */
  { 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10 },  /* backslash */
  { 0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e },  /* ']' */
  { 0x04, 0x0a, 0x11, 0x00, 0x00, 0x00, 0x00 },  /* '^' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f },  /* '_' */
  { 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* '`' */
  { 0x00, 0x00, 0x0e, 0x10, 0x1e, 0x11, 0x1e },  /* 'a' */
  { 0x01, 0x01, 0x0f, 0x11, 0x11, 0x11, 0x0f },  /* 'b' */
  { 0x00, 0x00, 0x0e, 0x11, 0x01, 0x11, 0x0e },  /* 'c' */
  { 0x10, 0x10, 0x1e, 0x11, 0x11, 0x11, 0x1e },  /* 'd' */
  { 0x00, 0x00, 0x0e, 0x11, 0x1f, 0x01, 0x0e },  /* 'e' */
  { 0x0c, 0x12, 0x02, 0x0f, 0x02, 0x02, 0x02 },  /* 'f' */
  { 0x00, 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x0e },  /* 'g' */
  { 0x01, 0x01, 0x0f, 0x11, 0x11, 0x11, 0x11 },  /* 'h' */
  { 0x04, 0x00, 0x06, 0x04, 0x04, 0x04, 0x0e },  /* 'i' */
  { 0x08, 0x00, 0x0c, 0x08, 0x08, 0x09, 0x06 },  /* 'j' */
  { 0x01, 0x01, 0x09, 0x05, 0x03, 0x05, 0x09 },  /* 'k' */
  { 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e },  /* 'l' */
  { 0x00, 0x00, 0x0b, 0x15, 0x15, 0x11, 0x11 },  /* 'm' */
  { 0x00, 0x00, 0x0f, 0x11, 0x11, 0x11, 0x11 },  /* 'n' */
  { 0x00, 0x00, 0x0e, 0x11, 0x11, 0x11, 0x0e },  /* 'o' */
  { 0x00, 0x00, 0x0f, 0x11, 0x0f, 0x01, 0x01 },  /* 'p' */
  { 0x00, 0x00, 0x1e, 0x11, 0x1e, 0x10, 0x10 },  /* 'q' */
  { 0x00, 0x00, 0x1a, 0x06, 0x02, 0x02, 0x02 },  /* 'r' */
  { 0x00, 0x00, 0x1e, 0x01, 0x0e, 0x10, 0x0f },  /* 's' */
  { 0x02, 0x02, 0x0f, 0x02, 0x02, 0x12, 0x0c },  /* 't' */
  { 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x1e },  /* 'u' */
  { 0x00, 0x00, 0x11, 0x11, 0x11, 0x0a, 0x04 },  /* 'v' */
  { 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0a },  /* 'w' */
  { 0x00, 0x00, 0x11, 0x0a, 0x04, 0x0a, 0x11 },  /* 'x' */
  { 0x00, 0x00, 0x11, 0x11, 0x1e, 0x10, 0x0e },  /* 'y' */
  { 0x00, 0x00, 0x1f, 0x08, 0x04, 0x02, 0x1f },  /* 'z' */
  { 0x18, 0x04, 0x04, 0x02, 0x04, 0x04, 0x18 },  /* '{' */
  { 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },  /* '|' */
  { 0x03, 0x04, 0x04, 0x08, 0x04, 0x04, 0x03 },  /* '}' */
  { 0x00, 0x16, 0x09, 0x00, 0x00, 0x00, 0x00 }   /* '~' */
};

/* One of the two faces above.  `rows` is 95 glyphs of `h` row-bytes each, LSB
   leftmost; `w` is the cell width. */
typedef struct {
  const unsigned char *rows;
  int w, h;
} map_font_t;

static const map_font_t kBigFont = { &kFont8x8[0][0], 8, 8 };
static const map_font_t kSmallFont = { &kFont5x7[0][0], 5, 7 };

/* Proportional advance: trim the glyph's unused right-hand columns so labels
   stay compact in a 60px-wide room box. */
static int
glyph_advance (const map_font_t *f, unsigned char c)
{
  int i, row, used = 0;
  if (c < 32 || c > 126 || c == ' ')
    return f->w / 2 + 1;
  for (row = 0; row < f->h; row++)
    {
      unsigned char bits = f->rows[(c - 32) * f->h + row];
      for (i = f->w - 1; i >= 0; i--)
        if (bits & (1 << i))
          {
            if (i + 1 > used)
              used = i + 1;
            break;
          }
    }
  return used + 1;
}

static void
draw_glyph (map_surface_t *s, const map_font_t *f, unsigned char c, int x,
            int y, unsigned int rgb, int alpha)
{
  int row, col;
  if (c < 32 || c > 126)
    return;
  for (row = 0; row < f->h; row++)
    {
      unsigned char bits = f->rows[(c - 32) * f->h + row];
      for (col = 0; col < f->w; col++)
        if (bits & (1 << col))
          blend (s, x + col, y + row, rgb, alpha);
    }
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

/* Word-wrap `text` to `maxw` pixels in `f`.  `broke` reports whether a word
   had to be split, which means the face is too big for the box. */
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

          /* An advance includes the gap that follows the glyph; the last one
             on a line has nothing after it, so don't make the line pay for
             it.  Without this a name is split for want of a single pixel. */
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

/* Centre the room name in its box.  Stands in for GDI+'s StringFormat centring
   plus Map.vb's GetFont auto-fit: the runner binary-searches a font size that
   makes the name fit, and since a Glk graphics window has no text at all, we
   do the bitmap equivalent -- lay the name out at 8x8, and drop to the 5x7
   face when that would split a word or overflow the box. */
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

  if (text == NULL || text[0] == '\0' || maxw < 4)
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
  if (ty < y0)
    ty = y0;                    /* still taller than the box: start at the top */
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

static void
proj_init (proj_t *p, const map_camera_t *cam, const map_surface_t *dst)
{
  p->cam = cam;
  p->ox = dst->w / 2 - cam->cx;
  p->oy = dst->h / 2 - cam->cy;
}

static int
px_x (const proj_t *p, double ux)
{
  return (int) (ux * p->cam->scale + 0.5) + p->ox;
}

static int
px_y (const proj_t *p, double uy)
{
  return (int) (uy * p->cam->scale + 0.5) + p->oy;
}

/* GetLinkPoint (Map.vb:747): where a connector meets the node's edge. */
static void
link_point (const proj_t *p, const map_node_t *n, int dir, double *x,
            double *y)
{
  double lx = n->x, ly = n->y, w = n->w, h = n->h;
  switch (dir)
    {
    case DIR_N:  *x = lx + w / 2; *y = ly;         break;
    case DIR_NE: *x = lx + w;     *y = ly;         break;
    case DIR_E:  *x = lx + w;     *y = ly + h / 2; break;
    case DIR_SE: *x = lx + w;     *y = ly + h;     break;
    case DIR_S:  *x = lx + w / 2; *y = ly + h;     break;
    case DIR_SW: *x = lx;         *y = ly + h;     break;
    case DIR_W:  *x = lx;         *y = ly + h / 2; break;
    case DIR_NW: *x = lx;         *y = ly;         break;
    default:     *x = lx + w / 2; *y = ly + h / 2; break;   /* Up/Down/In/Out */
    }
  *x = px_x (p, *x);
  *y = px_y (p, *y);
}

/* GetRelativePoint (Map.vb:1659): a point given as a percentage of the node
   box, allowed to fall outside it. */
static void
rel_point (const proj_t *p, const map_node_t *n, double xp, double yp,
           double *x, double *y)
{
  *x = px_x (p, n->x + n->w * xp / 100.0);
  *y = px_y (p, n->y + n->h * yp / 100.0);
}

/* GetBezierAssister (Map.vb:1592): the control point that bows a connector
   out of the node in its own direction. */
static void
bezier_assister (const proj_t *p, const map_node_t *n, int dir, double dist,
                 double *x, double *y)
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
 * ---------------------------------------------------------------- drawing
 */

static int
view_seen (const map_view_t *v, const char *key)
{
  if (v == NULL || v->seen == NULL || key == NULL)
    return 0;
  return v->seen (v->ctx, key);
}

static const map_node_t *
page_node (const map_page_t *page, const char *key)
{
  int i;
  if (page == NULL || key == NULL)
    return NULL;
  for (i = 0; i < page->n_nodes; i++)
    if (page->nodes[i].key != NULL
        && strcmp (page->nodes[i].key, key) == 0)
      return &page->nodes[i];
  return NULL;
}

/* The manual zoom ladder ("glk zoom in/out").  The automatic fit never goes
   above MAP_SCALE_MAX, but a player asking to zoom in can usefully get closer
   than the fit would; past 32 the boxes stop gaining anything. */
int
map_zoom_step (int scale, int dir)
{
  static const int ladder[] = { 3, 4, 5, 6, 8, 10, 12, 16, 20, 26, 32 };
  const int n = (int) (sizeof ladder / sizeof ladder[0]);
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

void
map_frame (const map_t *map, const map_view_t *view,
             const char *player_key, const map_surface_t *dst,
             int zoom, map_camera_t *cam)
{
  const map_node_t *pn;
  const map_page_t *page;
  int i, minx = 0, miny = 0, maxx = 0, maxy = 0, first = 1;
  int sx, sy, scale;

  cam->page = 0;
  cam->scale = 10;
  cam->cx = 0;
  cam->cy = 0;
  if (map == NULL || dst == NULL)
    return;

  /* The runner switches to the page the player is on (SelectNode). */
  pn = map_find (map, player_key);
  if (pn != NULL)
    cam->page = pn->page;
  else if (map->n_pages > 0)
    cam->page = map->pages[0].key;

  page = page_by_key (map, cam->page);
  if (page == NULL)
    return;

  for (i = 0; i < page->n_nodes; i++)
    {
      const map_node_t *n = &page->nodes[i];
      if (!view_seen (view, n->key))
        continue;
      if (first)
        {
          minx = n->x;
          miny = n->y;
          maxx = n->x + n->w;
          maxy = n->y + n->h;
          first = 0;
        }
      else
        {
          if (n->x < minx)
            minx = n->x;
          if (n->y < miny)
            miny = n->y;
          if (n->x + n->w > maxx)
            maxx = n->x + n->w;
          if (n->y + n->h > maxy)
            maxy = n->y + n->h;
        }
    }
  if (first)
    return;                     /* nothing seen yet */

  /* Fit the seen extent, with a margin for the labels and out-arrows -- unless
     a manual zoom has pinned the scale, when only the centring below runs. */
  if (zoom > 0)
    scale = zoom;
  else
    {
      sx = (maxx - minx) > 0 ? (dst->w - 24) / (maxx - minx) : MAP_SCALE_MAX;
      sy = (maxy - miny) > 0 ? (dst->h - 24) / (maxy - miny) : MAP_SCALE_MAX;
      scale = sx < sy ? sx : sy;
      if (scale > MAP_SCALE_MAX)
        scale = MAP_SCALE_MAX;
      if (scale < MAP_SCALE_MIN)
        scale = MAP_SCALE_MIN;
    }
  cam->scale = scale;

  /* Centre the seen extent when it fits at this scale (CentreMap); once it is
     too big to show at once, follow the player instead (LockPlayerCentre) and
     clamp so we never scroll past the edge of the map. */
  cam->cx = (int) ((minx + maxx) / 2.0 * scale);
  cam->cy = (int) ((miny + maxy) / 2.0 * scale);

  if (pn != NULL && view_seen (view, pn->key))
    {
      int spanx = (maxx - minx) * scale;
      int spany = (maxy - miny) * scale;
      int lo, hi;
      if (spanx > dst->w - 24)
        {
          cam->cx = (int) ((pn->x + pn->w / 2.0) * scale);
          lo = minx * scale + (dst->w - 24) / 2;
          hi = maxx * scale - (dst->w - 24) / 2;
          if (cam->cx < lo)
            cam->cx = lo;
          if (cam->cx > hi)
            cam->cx = hi;
        }
      if (spany > dst->h - 24)
        {
          cam->cy = (int) ((pn->y + pn->h / 2.0) * scale);
          lo = miny * scale + (dst->h - 24) / 2;
          hi = maxy * scale - (dst->h - 24) / 2;
          if (cam->cy < lo)
            cam->cy = lo;
          if (cam->cy > hi)
            cam->cy = hi;
        }
    }
}

/* An exit that leads somewhere the player has not been is drawn as a short
   stub arrow out of the node (DrawOutArrow, Map.vb). */
static void
draw_out_arrow (map_surface_t *s, const proj_t *p, const map_node_t *n,
                int dir, int wd, int alpha)
{
  double x0, y0, x1, y1, dx, dy, len;
  double stub = p->cam->scale * 1.2;

  link_point (p, n, dir, &x0, &y0);
  switch (dir)
    {
    case DIR_N:  dx = 0;  dy = -1; break;
    case DIR_NE: dx = 1;  dy = -1; break;
    case DIR_E:  dx = 1;  dy = 0;  break;
    case DIR_SE: dx = 1;  dy = 1;  break;
    case DIR_S:  dx = 0;  dy = 1;  break;
    case DIR_SW: dx = -1; dy = 1;  break;
    case DIR_W:  dx = -1; dy = 0;  break;
    case DIR_NW: dx = -1; dy = -1; break;
    default: return;            /* Up/Down/In/Out get badges, not stubs */
    }
  len = sqrt (dx * dx + dy * dy);
  dx /= len;
  dy /= len;
  x1 = x0 + dx * stub;
  y1 = y0 + dy * stub;
  draw_line (s, (int) x0, (int) y0, (int) x1, (int) y1, wd, map_fg,
             alpha, 0);
  draw_arrowhead (s, x1, y1, dx, dy, wd * 2 + 2, map_fg, alpha);
}

/* The IN / OUT bubble on a node edge (DrawInOutIcon, Map.vb:1530), which we
   extend to Up and Down where those are badge links (ADRIFT 4, whose runner
   put a little icon on the room box instead of drawing a connector). */
static void
draw_dir_icon (map_surface_t *s, const proj_t *p, const map_node_t *n,
               int dir, int alpha)
{
  double cx, cy;
  const char *letter;
  unsigned int rgb;
  int xp, yp;
  int r = p->cam->scale / 2;
  if (r < 3)
    r = 3;
  /* The runner picks the edge from where the destination lies; without the
     full edge bookkeeping we give each badge a fixed spot -- IN and OUT along
     the top edge, UP high on the right edge, DOWN low on the left edge --
     which keeps the four from colliding, and keeps UP and DOWN off the
     corners where the NE and SW connectors attach. */
  switch (dir)
    {
    case DIR_IN:   letter = "I"; rgb = ICON_IN;   xp = 25;  yp = 0;   break;
    case DIR_OUT:  letter = "O"; rgb = ICON_OUT;  xp = 75;  yp = 0;   break;
    case DIR_UP:   letter = "U"; rgb = ICON_UP;   xp = 100; yp = 25;  break;
    case DIR_DOWN: letter = "D"; rgb = ICON_DOWN; xp = 0;   yp = 75;  break;
    default: return;
    }
  rel_point (p, n, xp, yp, &cx, &cy);
  fill_circle (s, (int) cx, (int) cy, r, rgb, alpha);
  draw_text (s, &kSmallFont, letter, 1, (int) cx - 2, (int) cy - 3,
             0xFFFFFF, alpha);
}

/* The ADRIFT 4 runner had two pictures per icon: the normal one when the
   destination has been seen (clicking it recentres the map there), and a
   dimmed one when it has not (Form29.doicon, the seen-flag branch).  We dim
   by alpha instead.  Only badge links carry the distinction; ADRIFT 5's
   DrawInOutIcon has a single look. */
static int
badge_alpha (const map_view_t *view, const map_link_t *lk, int alpha)
{
  if (!lk->badge)
    return alpha;
  if (lk->dest != NULL && view_seen (view, lk->dest))
    return alpha;
  return alpha / 2;
}

void
map_render (const map_t *map, const map_view_t *view,
              const char *player_key, const map_camera_t *cam,
              map_surface_t *dst)
{
  const map_page_t *page;
  const map_node_t *active;
  proj_t p;
  int i, l, wd;

  if (dst == NULL)
    return;
  fill_surface (dst, map_bg);
  if (map == NULL || cam == NULL)
    return;
  page = page_by_key (map, cam->page);
  if (page == NULL)
    return;

  proj_init (&p, cam, dst);
  active = map_find (map, player_key);
  wd = cam->scale / 5;          /* Map.vb:1433, pen width = iScale / 5 */
  if (wd < 1)
    wd = 1;

  /* Pass 1: connectors, so the room boxes sit on top of them. */
  for (i = 0; i < page->n_nodes; i++)
    {
      const map_node_t *n = &page->nodes[i];
      if (!view_seen (view, n->key))
        continue;

      for (l = 0; l < n->n_links; l++)
        {
          const map_link_t *link = &n->links[l];
          const map_node_t *dn;
          double x0, y0, x1, y1, x2, y2, x3, y3, dist;
          int alpha, dash, phase = 0;
          int dst_anchor;

          if (link->dir == DIR_IN || link->dir == DIR_OUT || link->badge)
            continue;           /* drawn as badges below */
          if (link->dest == NULL)
            continue;

          dn = page_node (page, link->dest);
          if (dn == NULL || !view_seen (view, dn->key))
            {
              /* Destination unseen or on another page: stub arrow only. */
              continue;
            }
          if (link->dir == DIR_UP || link->dir == DIR_DOWN)
            {
              /* In plan view Z drops out, so an up/down link degenerates to a
                 straight run between the two room centres; that is exactly
                 what the runner shows until you rotate the map. */
            }

          /* Nodes on a different level than the player's fade out
             (Map.vb:1436). */
          alpha = 100;
          if (active != NULL && n->z != active->z
              && !(dn->z == active->z))
            alpha = 30;

          dash = link->dotted;
          if (link->dotted && view != NULL && view->ever_blocked != NULL)
            {
              /* The ADRIFT 5 runner hides a restricted connector while its
                 restrictions currently fail -- Grandpa's Ranch's Driveway
                 shows no link north until the front door is opened
                 (Map.vb:1429, the DashStyle.Dot HasRouteInDirection gate)... */
              if (view->exit_dest == NULL
                  || view->exit_dest (view->ctx, n->key, link->dir) == NULL)
                continue;
              /* ...and draws it solid until the player has actually been
                 blocked there once (Map.vb:1447, bEverBeenBlocked). */
              if (!view->ever_blocked (view->ctx, n->key, link->dir))
                dash = 0;
            }

          dst_anchor = link->dst_anchor;
          if (dst_anchor < 0)
            dst_anchor = link->dir;

          link_point (&p, n, link->dir, &x0, &y0);
          link_point (&p, dn, dst_anchor, &x3, &y3);
          dist = sqrt ((x3 - x0) * (x3 - x0) + (y3 - y0) * (y3 - y0));
          bezier_assister (&p, n, link->dir, dist, &x1, &y1);
          bezier_assister (&p, dn, dst_anchor, dist, &x2, &y2);

          draw_bezier (dst, x0, y0, x1, y1, x2, y2, x3, y3, wd, map_fg,
                       alpha, dash, &phase);
        }
    }

  /* Pass 2: exits leading somewhere we have not been, as stub arrows.  The
     runner gates these on HasRouteInDirection AndAlso Not HasSeenLocation
     (Map.vb DrawOutArrow) -- an exit back to a room already on the map is
     already drawn as a connector, so it must not also get a stub. */
  if (view != NULL && view->exit_dest != NULL)
    {
      for (i = 0; i < page->n_nodes; i++)
        {
          const map_node_t *n = &page->nodes[i];
          int d;
          if (!view_seen (view, n->key))
            continue;
          for (d = 0; d < 12; d++)
            {
              const char *dest;
              if (d == DIR_IN || d == DIR_OUT)
                continue;
              dest = view->exit_dest (view->ctx, n->key, d);
              if (dest == NULL || dest[0] == '\0')
                continue;
              if (view_seen (view, dest))
                continue;
              draw_out_arrow (dst, &p, n, d, wd, 100);
            }
        }
    }

  /* Pass 3: the room boxes and their labels. */
  for (i = 0; i < page->n_nodes; i++)
    {
      const map_node_t *n = &page->nodes[i];
      int x0, y0, x1, y1, alpha, is_player;
      const map_link_t *b_in = NULL, *b_out = NULL;
      const map_link_t *b_up = NULL, *b_down = NULL;
      unsigned int fill;

      if (!view_seen (view, n->key))
        continue;

      x0 = px_x (&p, n->x);
      y0 = px_y (&p, n->y);
      x1 = px_x (&p, n->x + n->w);
      y1 = px_y (&p, n->y + n->h);
      if (x1 < 0 || y1 < 0 || x0 >= dst->w || y0 >= dst->h)
        continue;               /* off-screen */

      is_player = (player_key != NULL && n->key != NULL
                   && strcmp (n->key, player_key) == 0);

      /* Alpha 200 on the player's level, 50 elsewhere (Map.vb:1172-1194). */
      alpha = 200;
      if (!is_player && active != NULL && n->z != active->z)
        alpha = 50;

      fill = is_player ? map_fg : map_bg;
      fill_rect (dst, x0, y0, x1, y1, fill, alpha);
      draw_rect (dst, x0, y0, x1, y1, map_fg, alpha);

      for (l = 0; l < n->n_links; l++)
        {
          const map_link_t *lk = &n->links[l];
          if (lk->dir == DIR_IN)
            b_in = lk;
          else if (lk->dir == DIR_OUT)
            b_out = lk;
          else if (lk->dir == DIR_UP && lk->badge)
            b_up = lk;
          else if (lk->dir == DIR_DOWN && lk->badge)
            b_down = lk;
        }
      /* The ADRIFT 5 runner's IN/OUT badges only show while the route is
         currently usable (the HasRouteInDirection gates, Map.vb:1328/1337) --
         Grandpa's Ranch's Living Room gains its OUT badge when the front
         door is opened. */
      if (view != NULL && view->ever_blocked != NULL && view->exit_dest != NULL)
        {
          if (b_in != NULL
              && view->exit_dest (view->ctx, n->key, DIR_IN) == NULL)
            b_in = NULL;
          if (b_out != NULL
              && view->exit_dest (view->ctx, n->key, DIR_OUT) == NULL)
            b_out = NULL;
        }
      if (b_in != NULL)
        draw_dir_icon (dst, &p, n, DIR_IN, badge_alpha (view, b_in, alpha));
      if (b_out != NULL)
        draw_dir_icon (dst, &p, n, DIR_OUT, badge_alpha (view, b_out, alpha));
      if (b_up != NULL)
        draw_dir_icon (dst, &p, n, DIR_UP, badge_alpha (view, b_up, alpha));
      if (b_down != NULL)
        draw_dir_icon (dst, &p, n, DIR_DOWN,
                       badge_alpha (view, b_down, alpha));

      if (view != NULL && view->name != NULL)
        {
          const char *label = view->name (view->ctx, n->key);
          if (label != NULL && label[0] != '\0')
            draw_label (dst, label, x0, y0, x1, y1,
                        is_player ? map_bg : map_fg,
                        alpha == 50 ? 90 : 255);
        }
    }
}

const char *
map_hit (const map_t *map, const map_view_t *view,
           const map_camera_t *cam, int w, int h, int mx, int my)
{
  const map_page_t *page;
  map_surface_t dim;
  proj_t p;
  int i;

  if (map == NULL || cam == NULL || w <= 0 || h <= 0)
    return NULL;
  page = page_by_key (map, cam->page);
  if (page == NULL)
    return NULL;
  dim.w = w;
  dim.h = h;
  dim.px = NULL;                /* proj_init only reads the dimensions */
  proj_init (&p, cam, &dim);

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
  return NULL;
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

  if (view == NULL || view->exit_dest == NULL || from == NULL || to == NULL)
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

      for (d = 0; d < 12; d++)
        {
          const char *dest = view->exit_dest (view->ctx, u.c_str (), d);

          if (dest == NULL || dest[0] == '\0')
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
              /* Found it.  Walk the chain back to the room whose predecessor
                 is the start (order[0]), and take the direction that left it. */
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
