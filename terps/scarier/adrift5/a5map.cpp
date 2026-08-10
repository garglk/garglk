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
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "a5map.h"
#include "a5xml.h"
#include "a5model.h"

static int
dir_index (const char *s)
{
  int i;
  if (s == NULL)
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
  if (dotted != NULL)
    *dotted = 0;
  m = a5model_movement (loc, NULL, dir);
  if (m == NULL)
    return NULL;
  dest = a5xml_child_text (m, "Destination");
  if (dest == NULL || dest[0] == '\0')
    return NULL;
  /* A restricted movement is drawn dotted (Map.vb:1441, DottedLink). */
  r = a5xml_child (m, "Restrictions");
  if (dotted != NULL && r != NULL && r->n_children > 0)
    *dotted = 1;
  return dest;
}

static int
node_int (const a5_xml_node_t *n, const char *name, int dflt)
{
  const char *s = a5xml_child_text (n, name);
  if (s == NULL || s[0] == '\0')
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
  if (link->mids == NULL)
    return;

  im = 0;
  for (an = lk->first_child; an != NULL; an = an->next)
    {
      if (strcmp (an->name, "Anchor") != 0)
        continue;
      if (im >= n_mids)
        break;
      link->mids[im].x = node_int (an, "X", 0);
      link->mids[im].y = node_int (an, "Y", 0);
      link->mids[im].z = node_int (an, "Z", 0);
      im++;
    }
  link->n_mids = im;
}

/* One connector leaving a node (<Link>).  Returns 0 if the link has no usable
   source anchor, in which case the slot is left untouched and skipped. */
static int
parse_link (map_link_t *link, const a5_xml_node_t *lk,
            const a5_location_t *loc)
{
  const char *src = a5xml_child_text (lk, "SourceAnchor");
  int d = dir_index (src);

  if (d < 0)
    return 0;
  link->dir = d;
  link->dst_anchor = dir_index (a5xml_child_text (lk, "DestinationAnchor"));
  link->dest = movement_dest (loc, src, &link->dotted);
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
  if (loc != NULL && loc->node != NULL)
    node->hidden = a5xml_bool (a5xml_child_text (loc->node, "Hide"));

  n_links = a5xml_count (nd, "Link");
  if (n_links <= 0)
    return;
  node->links = (map_link_t *) calloc ((size_t) n_links, sizeof (map_link_t));
  if (node->links == NULL)
    return;

  il = 0;
  for (lk = nd->first_child; lk != NULL; lk = lk->next)
    {
      if (strcmp (lk->name, "Link") != 0)
        continue;
      if (il >= n_links)
        break;
      if (parse_link (&node->links[il], lk, loc))
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
  page->label = a5xml_child_text (pg, "Label");
  page->n_nodes = a5xml_count (pg, "Node");
  if (page->n_nodes > 0)
    page->nodes = (map_node_t *) calloc ((size_t) page->n_nodes,
                                           sizeof (map_node_t));

  in = 0;
  for (nd = pg->first_child; nd != NULL && page->nodes != NULL; nd = nd->next)
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

  if (adv == NULL || adv->root == NULL)
    return NULL;
  map = a5xml_child (adv->root, "Map");
  if (map == NULL)
    return NULL;

  m = (map_t *) calloc (1, sizeof (map_t));
  if (m == NULL)
    return NULL;

  m->n_pages = a5xml_count (map, "Page");
  if (m->n_pages <= 0)
    {
      free (m);
      return NULL;
    }
  m->pages = (map_page_t *) calloc ((size_t) m->n_pages,
                                      sizeof (map_page_t));
  if (m->pages == NULL)
    {
      free (m);
      return NULL;
    }

  ip = 0;
  for (pg = map->first_child; pg != NULL; pg = pg->next)
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
      return NULL;
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

  if (adv == NULL)
    return 0;
  for (t = 0; t < adv->n_tasks; t++)
    {
      const a5_task_t *task = &adv->tasks[t];
      for (c = 0; c < task->n_commands; c++)
        {
          const char *s = task->commands[c];
          while (s != NULL && *s != '\0')
            {
              const char *end = strchr (s, '/');
              size_t len = (end != NULL) ? (size_t) (end - s) : strlen (s);
              size_t b = 0;

              while (b < len && (s[b] == ' ' || s[b] == '\t'))
                b++;
              while (len > b && (s[len - 1] == ' ' || s[len - 1] == '\t'))
                len--;
              if (equals_ci (s + b, len - b, "map"))
                return 1;
              if (end == NULL)
                break;
              s = end + 1;
            }
        }
    }
  return 0;
}
