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

/* The ADRIFT 5 map loader.  See a5map.h.  This reads the authored <Map> block
   into the engine-neutral map_t that mapdraw.cpp draws. */

#include <cstdlib>
#include <cstring>

#include "a5map.h"
#include "a5xml.h"
#include "a5model.h"

static int
dir_index (const char *s)
{
  int i;
  if (s == nullptr)
    return -1;
  for (i = 0; i < MAP_N_DIRS; i++)
    if (strcmp (s, map_dirs[i]) == 0)
      return i;
  return -1;
}

/*
 * ---------------------------------------------------------------- parsing
 */

/* The destination of an exit, straight from the location's Movement table --
   the map XML records only which anchors the connector was drawn between, so
   the runner re-derives the target from arlDirections (FileIO.vb:4100). */
static const char *
movement_dest (const a5_location_t *loc, const char *dir, int *dotted)
{
  const a5_xml_node_t *m, *r;
  const char *dest;
  if (dotted != nullptr)
    *dotted = 0;
  m = a5model_movement (loc, nullptr, dir);
  if (m == nullptr)
    return nullptr;
  dest = a5xml_child_text (m, "Destination");
  if (dest == nullptr || dest[0] == '\0')
    return nullptr;
  /* A restricted movement is drawn dotted (Map.vb:1441, DottedLink). */
  r = a5xml_child (m, "Restrictions");
  if (dotted != nullptr && r != nullptr && r->n_children > 0)
    *dotted = 1;
  return dest;
}

static int
node_int (const a5_xml_node_t *n, const char *name, int dflt)
{
  const char *s = a5xml_child_text (n, name);
  if (s == nullptr || s[0] == '\0')
    return dflt;
  return atoi (s);
}

/* The author-dragged waypoints of a connector (<Anchor> children of a
   <Link>).  ADRIFT 5 only; a connector without them is drawn straight. */
static void
parse_anchors (map_link_t *link, const a5_xml_node_t *lk)
{
  const a5_xml_node_t *an;
  int n_mids, im;

  n_mids = a5xml_count (lk, "Anchor");
  if (n_mids <= 0)
    return;
  link->mids = (map_pt_t *) calloc ((size_t) n_mids, sizeof (map_pt_t));
  if (link->mids == nullptr)
    return;

  im = 0;
  for (an = lk->first_child; an != nullptr; an = an->next)
    {
      if (strcmp (an->name, "Anchor") != 0)
        continue;
      if (im >= n_mids)
        break;
      link->mids[im].x = node_int (an, "X", 0);
      link->mids[im].y = node_int (an, "Y", 0);
      im++;
    }
  link->n_mids = im;
}

/* True when `dir` is a plan-view compass direction (not Up/Down/In/Out). */
static int
is_compass_dir (int dir)
{
  return dir >= 0 && dir < MAP_N_DIRS && !map_is_badge_dir (dir);
}

/* Compass exit on `loc` that shares destination and restriction style with a
   badge link.  Basement Up→Kitchen coincides with East→Kitchen even though only
   Kitchen authors the East-West Map Link. */
static int
find_compass_twin_same_dest (const a5_location_t *loc, const char *dest,
                             int dotted)
{
  int d, cdotted;
  const char *md;

  if (loc == nullptr || dest == nullptr)
    return -1;
  for (d = 0; d < MAP_N_DIRS; d++)
    {
      if (!is_compass_dir (d))
        continue;
      md = movement_dest (loc, map_dirs[d], &cdotted);
      if (md != nullptr && strcmp (md, dest) == 0 && cdotted == dotted)
        return d;
    }
  return -1;
}

/* One connector leaving a node (<Link>).  Returns 0 if the link has no usable
   source anchor, in which case the slot is left untouched and skipped. */
static int
parse_link (map_link_t *link, const a5_xml_node_t *lk,
            const a5_adventure_t *adv, const a5_location_t *loc)
{
  const char *src = a5xml_child_text (lk, "SourceAnchor");
  int d = dir_index (src);
  const a5_location_t *dest_loc;
  const char *back;

  if (d < 0)
    return 0;
  link->dir = d;
  link->dst_anchor = dir_index (a5xml_child_text (lk, "DestinationAnchor"));
  link->dest = movement_dest (loc, src, &link->dotted);
  link->has_compass_twin = 0;
  link->compass_twin = -1;
  /* Up/Down stay as ordinary links here: mapdraw draws U/D badges and a
     badge-to-badge connector (same pattern as In/Out).  ADRIFT 4 sets
     link.badge so its Up/Down stay icon-only. */
  /* Duplex from movements (FileIO.vb), not from whether dest has a Map Link:
     Grandpa Ranch's Tunnel has Up to Basement but no <Link>, while Basement
     has the Down <Link> with DestinationAnchor Up. */
  link->duplex = 0;
  if (link->dest != nullptr && link->dst_anchor >= 0 && loc != nullptr
      && loc->key != nullptr)
    {
      dest_loc = a5model_location (adv, link->dest);
      back = movement_dest (dest_loc, map_dirs[link->dst_anchor], nullptr);
      if (back != nullptr && strcmp (back, loc->key) == 0)
        link->duplex = 1;
    }
  /* Badge/compass coincidence from movements (Map Links alone miss Basement
     East, which is only a Movement while Kitchen owns the West Map Link). */
  if (map_is_badge_dir (d) && link->dest != nullptr)
    {
      link->compass_twin
        = find_compass_twin_same_dest (loc, link->dest, link->dotted);
      link->has_compass_twin = (link->compass_twin >= 0);
    }
  parse_anchors (link, lk);
  return 1;
}

/* One room box on a page (<Node>): its geometry and the connectors leaving
   it. */
static void
parse_node (map_node_t *node, const a5_xml_node_t *nd,
            const a5_adventure_t *adv, int page_key)
{
  const a5_location_t *loc;
  const a5_xml_node_t *lk;
  int n_links, il;

  node->key = a5xml_child_text (nd, "Key");
  node->x = node_int (nd, "X", 0);
  node->y = node_int (nd, "Y", 0);
  node->z = node_int (nd, "Z", 0);
  node->w = node_int (nd, "Width", MAP_NODE_W);
  node->h = node_int (nd, "Height", MAP_NODE_H);
  if (node->w <= 0)
    node->w = MAP_NODE_W;
  if (node->h <= 0)
    node->h = MAP_NODE_H;
  node->page = page_key;

  loc = a5model_location (adv, node->key);
  if (loc != nullptr && loc->node != nullptr)
    node->hidden = a5xml_bool (a5xml_child_text (loc->node, "Hide"));
  /* FileIO.vb:4071-4074 -- Movements, not Map Links, set bHas*. */
  for (il = DIR_UP; il <= DIR_OUT; il++)
    if (movement_dest (loc, map_dirs[il], nullptr) != nullptr)
      node->has_badge[map_badge (il)] = 1;

  n_links = a5xml_count (nd, "Link");
  if (n_links <= 0)
    return;
  node->links = (map_link_t *) calloc ((size_t) n_links, sizeof (map_link_t));
  if (node->links == nullptr)
    return;

  il = 0;
  for (lk = nd->first_child; lk != nullptr; lk = lk->next)
    {
      if (strcmp (lk->name, "Link") != 0)
        continue;
      if (il >= n_links)
        break;
      if (parse_link (&node->links[il], lk, adv, loc))
        il++;
    }
  node->n_links = il;
}

/* One page (<Page>) and the nodes on it.  Returns the number of nodes placed,
   which the caller totals to tell an empty map from a populated one. */
static int
parse_page (map_page_t *page, const a5_xml_node_t *pg,
            const a5_adventure_t *adv, int dflt_key)
{
  const a5_xml_node_t *nd;
  int in;

  page->key = node_int (pg, "Key", dflt_key);
  page->n_nodes = a5xml_count (pg, "Node");
  if (page->n_nodes > 0)
    page->nodes = (map_node_t *) calloc ((size_t) page->n_nodes,
                                           sizeof (map_node_t));

  in = 0;
  for (nd = pg->first_child; nd != nullptr && page->nodes != nullptr; nd = nd->next)
    {
      if (strcmp (nd->name, "Node") != 0)
        continue;
      if (in >= page->n_nodes)
        break;
      parse_node (&page->nodes[in], nd, adv, page->key);
      in++;
    }
  page->n_nodes = in;
  return in;
}

map_t *
a5map_load (const a5_adventure_t *adv)
{
  const a5_xml_node_t *map, *pg;
  map_t *m;
  int total_nodes = 0;
  int ip;

  if (adv == nullptr || adv->root == nullptr)
    return nullptr;
  map = a5xml_child (adv->root, "Map");
  if (map == nullptr)
    return nullptr;

  m = (map_t *) calloc (1, sizeof (map_t));
  if (m == nullptr)
    return nullptr;

  m->n_pages = a5xml_count (map, "Page");
  if (m->n_pages <= 0)
    {
      free (m);
      return nullptr;
    }
  m->pages = (map_page_t *) calloc ((size_t) m->n_pages,
                                      sizeof (map_page_t));
  if (m->pages == nullptr)
    {
      free (m);
      return nullptr;
    }

  ip = 0;
  for (pg = map->first_child; pg != nullptr; pg = pg->next)
    {
      if (strcmp (pg->name, "Page") != 0)
        continue;
      if (ip >= m->n_pages)
        break;
      total_nodes += parse_page (&m->pages[ip], pg, adv, ip);
      ip++;
    }
  m->n_pages = ip;

  if (total_nodes == 0)
    {
      map_free (m);
      return nullptr;
    }
  return m;
}

/* Does the `len`-byte span at `s` equal the NUL-terminated `lit`, ignoring
   ASCII case?  (Task commands are plain ASCII, so no <cctype>/locale.) */
static int
equals_ci (const char *s, size_t len, const char *lit)
{
  size_t i;
  for (i = 0; i < len; i++)
    {
      char a = s[i], b = lit[i];
      if (b == '\0')
        return 0;
      if (a >= 'A' && a <= 'Z')
        a = (char) (a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z')
        b = (char) (b - 'A' + 'a');
      if (a != b)
        return 0;
    }
  return lit[len] == '\0';
}

/* Does any task command consist of the bare word "map"?  Task commands are
   slash-separated alternatives ("map/chart"), so each alternative is tested;
   surrounding space is ignored, and the match is case-insensitive.  Anything
   with more to it ("look at map", "map %object%") is not a bare MAP and does
   not conflict with a host MAP command. */
int
a5map_command_taken (const a5_adventure_t *adv)
{
  int t, c;

  if (adv == nullptr)
    return 0;
  for (t = 0; t < adv->n_tasks; t++)
    {
      const a5_task_t *task = &adv->tasks[t];
      for (c = 0; c < task->n_commands; c++)
        {
          const char *s = task->commands[c];
          while (s != nullptr && *s != '\0')
            {
              const char *end = strchr (s, '/');
              size_t len = (end != nullptr) ? (size_t) (end - s) : strlen (s);
              size_t b = 0;

              while (b < len && (s[b] == ' ' || s[b] == '\t'))
                b++;
              while (len > b && (s[len - 1] == ' ' || s[len - 1] == '\t'))
                len--;
              if (equals_ci (s + b, len - b, "map"))
                return 1;
              if (end == nullptr)
                break;
              s = end + 1;
            }
        }
    }
  return 0;
}
