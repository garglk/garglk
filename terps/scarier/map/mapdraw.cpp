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
 * This file holds the model, the rooms and the entry points; the rest of the
 * renderer is split by job, sharing map_internal.h:
 *
 *   map_raster.cpp   palette, drawing primitives and bitmap text
 *   map_layout.cpp   projection, camera framing and the pan/zoom buttons
 *   map_links.cpp    link geometry, badge sites, links and exit stubs
 *
 * Geometry follows the ADRIFT 5 Runner's Map.vb, and the software rasteriser
 * in map_raster.cpp stands in for the GDI+ calls it makes (FillPolygon /
 * DrawBezier / DrawCurve / DrawEllipse / DrawString).  The ADRIFT 4 Runner
 * draws its map out of VB control arrays instead, with a look of its own; we
 * render both engines' maps the same way rather than carrying two
 * rasterisers, so what differs between them is only where the nodes come
 * from -- authored in ADRIFT 5 (a5map.cpp), computed from the exit graph in
 * ADRIFT 4 (scmap.cpp).
 */

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "mapdraw.h"
#include "map_internal.h"

using namespace map_internal;

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

/* See mapdraw.h. */
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

const map_page_t *
map_internal::page_by_key (const map_t *map, int key)
{
  int p;
  for (p = 0; p < map->n_pages; p++)
    if (map->pages[p].key == key)
      return &map->pages[p];
  return nullptr;
}


/*
 * -------------------------------------------------------- pages and views
 */

int
map_internal::view_seen (const map_view_t *v, const char *key)
{
  if (v == nullptr || v->seen == nullptr || key == nullptr)
    return 0;
  return v->seen (v->ctx, key);
}

const map_node_t *
map_internal::page_node (const map_page_t *page, const char *key)
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
const map_link_t *
map_internal::find_dir_link (const map_node_t *n, int dir)
{
  int l;

  if (n == nullptr)
    return nullptr;
  for (l = 0; l < n->n_links; l++)
    if (n->links[l].dir == dir)
      return &n->links[l];
  return nullptr;
}


/*
 * The page the Runner would switch to: the player's own (SelectNode), or the
 * first one when the player is somewhere the map does not place.  Returns 0
 * when there is no page to pick, leaving *key alone.
 */
int
map_internal::player_page_key (const map_t *map, const char *player_key,
                               int *key)
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
 * -------------------------------------------------------------- rendering
 */

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
 * clsCharacter.Dijkstra, with the Runner's two constraints: an edge exists
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
            continue;           /* the Runner will not walk you through
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
