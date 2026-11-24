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
 * Where a map page sits in the window: the projection from map units to
 * pixels, fitting and panning the camera over the rooms the player has seen,
 * the zoom ladder, and the floating pan/zoom buttons.
 */

#include "mapdraw.h"
#include "map_internal.h"

using namespace map_internal;


/*
 * ------------------------------------------------------------- projection
 *
 * Plan view: Convert3DtoScreen is the identity at the Runner's default
 * offsets, so screen = map-units * scale, translated by the camera.  Z drops
 * out of the position entirely and survives only as the level fade.
 */


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
void
map_internal::proj_init (proj_t *p, const map_camera_t *cam, int w, int h)
{
  int top = chrome_top (cam, h);

  p->cam = cam;
  p->ox = w / 2 - cam->cx;
  /* Centre the view in the area under the buttons. */
  p->oy = top + (h - top) / 2 - cam->cy;
}

int
map_internal::px_x (const proj_t *p, double ux)
{
  return round_px (ux * p->cam->scale) + p->ox;
}

int
map_internal::px_y (const proj_t *p, double uy)
{
  return round_px (uy * p->cam->scale) + p->oy;
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
