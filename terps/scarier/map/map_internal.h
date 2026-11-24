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
 * The map renderer's private interface: what map/mapdraw.cpp, map_raster.cpp,
 * map_layout.cpp and map_links.cpp share, and nothing outside map/ uses.
 * It lives in namespace map_internal so that generic names like palette and
 * fill_rect cannot meet anything of the host's at link time.
 */

#ifndef MAP_INTERNAL_H
#define MAP_INTERNAL_H

#include "mapdraw.h"

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

namespace map_internal
{

/*
 * --------------------------------------------------------- map_raster.cpp
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

extern map_palette_t palette;

void ensure_derived_palette (void);
unsigned int mix_rgb (unsigned int a, unsigned int b, double t);
void blend (map_surface_t *s, int x, int y, unsigned int rgb, int alpha);
void fill_surface (map_surface_t *s, unsigned int rgb);
void fill_span (map_surface_t *s, int x0, int y0, int x1, int y1,
                unsigned int rgb, int alpha);
void fill_rect (map_surface_t *s, int x0, int y0, int x1, int y1,
                unsigned int rgb, int alpha);
void draw_rect (map_surface_t *s, int x0, int y0, int x1, int y1,
                unsigned int rgb, int alpha);
void draw_line (map_surface_t *s, int x0, int y0, int x1, int y1, int wd,
                unsigned int rgb, int alpha, int dash);
int round_px (double v);
void draw_bezier (map_surface_t *s, double x0, double y0, double x1, double y1,
                  double x2, double y2, double x3, double y3, int wd,
                  unsigned int rgb, int alpha, int dash, int *dash_phase);
void draw_curve (map_surface_t *s, const double *pts, int n, int wd,
                 unsigned int rgb, int alpha, int dash, int *dash_phase);
void fill_circle (map_surface_t *s, int cx, int cy, int r, unsigned int rgb,
                  int alpha);
void fill_ud_triangle (map_surface_t *s, int cx, int cy, int r, int up,
                       unsigned int rgb, int alpha);
void draw_arrowhead (map_surface_t *s, double x, double y, double dx,
                     double dy, int size, unsigned int rgb, int alpha);
void draw_label (map_surface_t *s, const char *text, int x0, int y0, int x1,
                 int y1, unsigned int rgb, int alpha);


/*
 * --------------------------------------------------------- map_layout.cpp
 */

typedef struct {
  const map_camera_t *cam;
  int ox, oy;                   /* pixel offset of map origin */
} proj_t;

void proj_init (proj_t *p, const map_camera_t *cam, int w, int h);
int px_x (const proj_t *p, double ux);
int px_y (const proj_t *p, double uy);


/*
 * ------------------------------------------------------------ mapdraw.cpp
 */

const map_page_t *page_by_key (const map_t *map, int key);
int view_seen (const map_view_t *v, const char *key);
const map_node_t *page_node (const map_page_t *page, const char *key);
const map_link_t *find_dir_link (const map_node_t *n, int dir);
int player_page_key (const map_t *map, const char *player_key, int *key);


/*
 * ---------------------------------------------------------- map_links.cpp
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
 * Which badges a node wears, and on which half-wind / compass site.
 *
 * An In/Out connector is recorded on one room only, but the Runner puts a
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

/* The order the Runner visits the badges in: In, Out, Up, Down. */
static const int badge_order[MAP_N_BADGES] = {
  DIR_IN, DIR_OUT, DIR_UP, DIR_DOWN
};

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

void draw_dir_icon_site (map_surface_t *s, const proj_t *p,
                         const map_node_t *n, int dir, int site, int alpha,
                         int dim);
int badge_dest_unseen (const map_view_t *view, const map_link_t *lk);
inout_badge_t *inout_layout (const map_page_t *page, const map_view_t *view);
void render_links (const render_ctx_t *rc);
void render_exit_stubs (const render_ctx_t *rc);

} /* namespace map_internal */

#endif /* MAP_INTERNAL_H */
