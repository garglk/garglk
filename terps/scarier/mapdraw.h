/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjolund
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
 * The map: room boxes, the connectors between them, and a software rasteriser
 * that draws them.
 *
 * Both ADRIFT engines end up here, but they arrive from opposite directions.
 * In ADRIFT 5 the layout is authored data -- the Developer writes an
 * <Adventure><Map> of <Page>s of <Node>s, each carrying its own X/Y/Z -- so
 * the interpreter only has to read it (a5map.cpp).  ADRIFT 4 stores no
 * coordinates anywhere in the .taf; its runner derives a layout at run time,
 * walking the exit graph outwards from the player's room and shoving
 * already-placed rooms aside whenever two want the same cell.  We port that
 * algorithm rather than invent one (scmap.cpp).
 *
 * Once either engine has produced a map_t, almost everything below is common,
 * so a v4 map and a v5 map look alike in Spatterlight -- even though the
 * ADRIFT 4 runner's own map, built out of Visual Basic control arrays, looked
 * nothing like the ADRIFT 5 one.  Geometry follows the Adrift 5 runner's
 * Map.vb, whose plan view is the one a player sees unless they drag the map:
 * X to the right, Y downward, one map unit = `scale` pixels.  The one thing
 * that does not carry over is the shape of a connector, because a VB Line
 * control cannot bow: see map_s.line_links.
 *
 * This module is deliberately free of Glk and of both engines: it rasterises
 * into a plain RGB surface, so a map can be rendered and diffed headlessly
 * (test/adrift5/harness/a5map_dump.cpp).
 */

#ifndef MAPDRAW_H
#define MAPDRAW_H

/* The twelve directions, in the order both engines number them: ADRIFT 5's
   DirectionsEnum (Global.vb:1468) and ADRIFT 4's exit array (run400 Form29's
   opp(), and SCARE's DIRNAMES) agree exactly, so one enum serves both. */
enum {
  DIR_N = 0, DIR_E, DIR_S, DIR_W, DIR_UP, DIR_DOWN,
  DIR_IN, DIR_OUT, DIR_NE, DIR_SE, DIR_SW, DIR_NW
};
#define MAP_N_DIRS 12
extern const char *const map_dirs[MAP_N_DIRS];

/* The runner's node defaults (FileIO.vb only writes Width/Height when they
   differ from these).  The ADRIFT 4 mapper adopts them too, so a room box is
   the same size whichever engine placed it. */
#define MAP_NODE_W 6
#define MAP_NODE_H 4

/* A point in map units. */
typedef struct map_pt_s {
  int x, y, z;
} map_pt_t;

/* One drawn connector leaving a node (clsMap.MapLink). */
typedef struct map_link_s {
  int dir;                    /* source direction, 0..11                     */
  int dst_anchor;             /* direction the connector enters the dest by  */
  const char *dest;           /* destination room key                        */
  int dotted;                 /* the movement is restricted                  */
  int duplex;                 /* dest's DestinationAnchor movement returns
                                 here.  A one-way (not duplex) connector
                                 gets an arrow head at the destination end.
                                 Set by a5map; ADRIFT 4 leaves it 0 and is
                                 excluded by map.line_links               */
  /* A compass Movement shares this badge link's dest (+ restriction style):
     the badge parks on that port and draws no connector of its own.  Set by
     a5map, which needs the movement table to find it.  The flag is what the
     renderer tests, because a loader that calloc()s its links -- scmap does
     -- would otherwise be claiming a twin due North. */
  unsigned char has_compass_twin;
  int compass_twin;           /* DIR_N..DIR_NW, when has_compass_twin        */
  int badge;                  /* drawn as a badge on the box, not a line
                                 (ADRIFT 4 Up/Down/In/Out; A5 draws Up/Down
                                 connectors badge-to-badge instead) */
  map_pt_t *mids;             /* author-dragged waypoints (ADRIFT 5 only)    */
  int n_mids;
} map_link_t;

/* One room box on a page (clsMap.MapNode). */
typedef struct map_node_s {
  const char *key;            /* room key                                    */
  int x, y, z;                /* top-left corner, in map units               */
  int w, h;                   /* size in map units                           */
  int page;
  int hidden;                 /* Location <Hide>: the runner omits the box
                                 but still draws connectors to a seen hidden
                                 room (Map.vb:1156 DrawNode vs DrawLinks)    */
  /* Location has a Movement in this badge direction (FileIO.vb bHasIn/Out/
     Up/Down).  Far-end In/Out icons gate on these, not on duplex. */
  unsigned char has_in, has_out, has_up, has_down;
  map_link_t *links;
  int n_links;
} map_node_t;

typedef struct map_page_s {
  int key;
  const char *label;          /* <Label> ("Page 1", ...)                     */
  map_node_t *nodes;
  int n_nodes;
} map_page_t;

typedef struct map_s {
  map_page_t *pages;
  int n_pages;
  /* ADRIFT 4 connectors are Visual Basic Line controls (Form29.dolink), so
     they are straight by construction, and axis-locked: only one of each
     line's two coordinates comes from the destination.  ADRIFT 5's Map.vb
     bows a compass connector into a cubic Bezier between the two rooms' own
     anchor points instead (GetBezierAssister).  The ADRIFT 4 mapper sets this;
     the ADRIFT 5 loader leaves it clear. */
  int line_links;
  /* Keys and labels normally alias the loader's own storage (in ADRIFT 5, the
     XML document).  A loader with nothing to alias -- the ADRIFT 4 mapper has
     only room numbers, so it must spell its keys out -- parks the strings here
     for map_free() to dispose of. */
  char **pool;
  int n_pool;
} map_t;

extern void map_free (map_t *map);

/* Find the node for a room.  NULL if the room was never placed on the map:
   ADRIFT 5 rooms created procedurally have no node, and the ADRIFT 4 layout
   passes over rooms the author flagged to hide. */
extern const map_node_t *map_find (const map_t *map, const char *lockey);

/* --- rendering ---------------------------------------------------------- */

/* A 24-bit RGB surface, 0x00RRGGBB per pixel, row-major. */
typedef struct map_surface_s {
  int w, h;
  unsigned int *px;
} map_surface_t;

extern map_surface_t *map_surface_new (int w, int h);
extern void map_surface_free (map_surface_t *s);

/* The host's text style, normally the Glk buffer's style_Normal.  Black on
   white until the host says otherwise.  How the two colours are spent on the
   drawing depends on the scheme below. */
extern void map_set_palette (unsigned int background, unsigned int text);

/* MAP_SCHEME_STANDARD spends them flat: `background` fills the map and the
   room boxes, `text` draws the connectors, borders and labels, and the
   player's room is their inversion.  MAP_SCHEME_DERIVED treats them as paper
   and ink and mixes room fills, a you-are-here amber, faded link greys and
   contrast-picked labels out of them.  Standard until the host says
   otherwise; the setting is the host's to remember. */
enum {
  MAP_SCHEME_STANDARD = 0,
  MAP_SCHEME_DERIVED = 1
};
extern void map_set_colour_scheme (int scheme);

/* What the renderer needs to know about the run.  Keeping this a callback
   table is what lets the map be drawn from the headless harness (and diffed)
   without linking the Glk layer, and lets one renderer serve two engines whose
   game state has nothing in common. */
typedef struct map_view_s {
  /* Has the viewpoint character seen this room? */
  int (*seen) (void *ctx, const char *lockey);
  /* The room's short description, for the label. */
  const char *(*name) (void *ctx, const char *lockey);
  /* Where a usable exit in `dir` leads (restrictions applied, as in the
     runner's HasRouteInDirection), or NULL for no exit.  An exit whose
     destination has not been seen is drawn as a stub arrow.  May be NULL. */
  const char *(*exit_dest) (void *ctx, const char *lockey, int dir);
  /* Has this exit's movement restriction ever evaluated false (the ADRIFT 5
     runner's clsDirection.bEverBeenBlocked)?  When set, the renderer applies
     the ADRIFT 5 runner's route gates: a restricted connector is hidden while
     its restrictions currently fail and drawn solid until the player has been
     blocked there once (Map.vb:1429/1447), and the IN/OUT badges only show
     while their route is open (Map.vb:1328/1337).  NULL for ADRIFT 4, whose
     runner drew every connector between rooms it laid out. */
  int (*ever_blocked) (void *ctx, const char *lockey, int dir);
  void *ctx;
} map_view_t;

/* Where the map is currently looking. */
typedef struct map_camera_s {
  int page;                   /* page to draw                                */
  int scale;                  /* pixels per map unit (runner default 10)     */
  int cx, cy;                 /* centre of the view, in map units * scale    */
} map_camera_t;

/* Pick the page the player is on and frame it.  With `zoom` 0, fits the seen
   nodes to `dst` (clamped between MAP_SCALE_MIN and MAP_SCALE_MAX) and centres
   on the player, like the runner's LockPlayerCentre.  A positive `zoom` pins
   the scale to that many pixels per map unit instead (a manual "glk zoom");
   the centring still runs, so the view pans to keep the player on-screen. */
#define MAP_SCALE_MIN 3
#define MAP_SCALE_MAX 16
extern void map_frame (const map_t *map, const map_view_t *view,
                       const char *player_key, const map_surface_t *dst,
                       int zoom, map_camera_t *cam);

/* The next manual zoom level in from (dir > 0) or out from (dir <= 0) `scale`
   pixels per map unit.  Returns `scale` unchanged at the end of the range,
   which is how a caller knows to warn instead of redraw. */
extern int map_zoom_step (int scale, int dir);

/* Draw the map.  Only rooms the player has seen are drawn (as in both
   runners); the player's own room is highlighted. */
extern void map_render (const map_t *map, const map_view_t *view,
                        const char *player_key, const map_camera_t *cam,
                        map_surface_t *dst);

/* Whether map_render would put anything at all on the page the player is on:
   at least one room that has been seen and is not hidden.  Games that run
   their title and options screens from a hidden staging room draw a blank
   rectangle until the player reaches somewhere real, which is worth not
   opening a pane for. */
extern int map_has_content (const map_t *map, const map_view_t *view,
                            const char *player_key);

/* Which room is at pixel (px,py) of a `w` x `h` map view?  NULL if none.
   Lets a click walk there. */
extern const char *map_hit (const map_t *map, const map_view_t *view,
                            const map_camera_t *cam, int w, int h,
                            int px, int py);

/* The first step of the shortest route from `from` to `to`: a direction index
   (0..11), or -1 if there is no route.  This is the runner's map-click walk
   (clsCharacter.Dijkstra / DoWalk): edges are the restriction-checked exits,
   and only rooms the player has already seen may be walked through.  The
   caller submits that direction as an ordinary command, one room per turn,
   exactly as DoWalk does. */
extern int map_walk_step (const map_view_t *view, const char *from,
                          const char *to);

#endif /* MAPDRAW_H */
