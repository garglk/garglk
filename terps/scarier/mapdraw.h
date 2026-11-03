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

#include "common_utils/rgbsurface.h"

/*
 * The twelve directions, in the order both engines number them: ADRIFT 5's
 * DirectionsEnum (Global.vb:1468) and ADRIFT 4's exit array (run400 Form29's
 * opp(), and SCARE's DIRNAMES) agree exactly, so one enum serves both.
 */
enum
{
  DIR_N = 0, DIR_E, DIR_S, DIR_W, DIR_UP, DIR_DOWN,
  DIR_IN, DIR_OUT, DIR_NE, DIR_SE, DIR_SW, DIR_NW
};
#define MAP_N_DIRS 12
extern const char *const map_dirs[MAP_N_DIRS];

/*
 * Each direction's unit offset on the plan, (dx, dy) in {-1, 0, 1} with Y
 * downward: North is (0, -1), SouthEast (1, 1).  Up, Down, In and Out have no
 * bearing and sit at (0, 0).
 */
extern const int map_dir_dx[MAP_N_DIRS];
extern const int map_dir_dy[MAP_N_DIRS];

/*
 * Up, Down, In and Out are the "badge" directions: they have no bearing on the
 * plan, so they are drawn as icons on the room box rather than as compass
 * connectors.  They are contiguous in the enum, which map_badge() exploits to
 * index per-badge arrays.
 */
#define MAP_N_BADGES 4

static inline int
map_badge (int dir)
{
  return dir - DIR_UP;
}

static inline int
map_is_badge_dir (int dir)
{
  return dir >= DIR_UP && dir <= DIR_OUT;
}

/*
 * The runner's node defaults (FileIO.vb only writes Width/Height when they
 * differ from these).  The ADRIFT 4 mapper adopts them too, so a room box is
 * the same size whichever engine placed it.
 */
#define MAP_NODE_W 6
#define MAP_NODE_H 4

/* A point in map units. */
typedef struct map_pt_s
{
  int x, y;
} map_pt_t;

/* One drawn connector leaving a node (clsMap.MapLink). */
typedef struct map_link_s
{
  int dir;                    /* source direction, 0..11                     */
  int dst_anchor;             /* direction the connector enters the dest by  */
  const char *dest;           /* destination room key                        */
  int dotted;                 /* the movement is restricted                  */
  int duplex;                 /* dest's DestinationAnchor movement returns
                                 here.  A one-way (not duplex) connector
                                 gets an arrow head at the destination end.
                                 Set by a5map; ADRIFT 4 leaves it 0 and is
                                 excluded by map.line_links               */
  /*
   * A compass Movement shares this badge link's dest (+ restriction style):
   * the badge parks on that port and draws no connector of its own.  Set by
   * a5map, which needs the movement table to find it.  The flag is what the
   * renderer tests, because a loader that calloc()s its links -- scmap does
   * -- would otherwise be claiming a twin due North.
   */
  unsigned char has_compass_twin;
  int compass_twin;           /* DIR_N..DIR_NW, when has_compass_twin        */
  int badge;                  /* drawn as a badge on the box, not a line
                                 (ADRIFT 4 Up/Down/In/Out; A5 draws Up/Down
                                 connectors badge-to-badge instead) */
  map_pt_t *mids;             /* author-dragged waypoints (ADRIFT 5 only)    */
  int n_mids;
} map_link_t;

/* One room box on a page (clsMap.MapNode). */
typedef struct map_node_s
{
  const char *key;            /* room key                                    */
  int x, y, z;                /* top-left corner, in map units               */
  int w, h;                   /* size in map units                           */
  int page;
  int hidden;                 /* Location <Hide>: the runner omits the box
                                 but still draws connectors to a seen hidden
                                 room (Map.vb:1156 DrawNode vs DrawLinks)    */
  /*
   * Location has a Movement in this badge direction (FileIO.vb bHasIn/Out/
   * Up/Down), indexed by map_badge().  Far-end In/Out icons gate on these, not
   * on duplex.
   */
  unsigned char has_badge[MAP_N_BADGES];
  map_link_t *links;
  int n_links;
} map_node_t;

typedef struct map_page_s
{
  int key;
  map_node_t *nodes;
  int n_nodes;
} map_page_t;

typedef struct map_s
{
  map_page_t *pages;
  int n_pages;
  /*
   * ADRIFT 4 connectors are Visual Basic Line controls (Form29.dolink), so
   * they are straight by construction, and axis-locked: only one of each
   * line's two coordinates comes from the destination.  ADRIFT 5's Map.vb
   * bows a compass connector into a cubic Bezier between the two rooms' own
   * anchor points instead (GetBezierAssister).  The ADRIFT 4 mapper sets this;
   * the ADRIFT 5 loader leaves it clear.
   */
  int line_links;
  /*
   * Keys and labels normally alias the loader's own storage (in ADRIFT 5, the
   * XML document).  A loader with nothing to alias -- the ADRIFT 4 mapper has
   * only room numbers, so it must spell its keys out -- parks the strings here
   * for map_free() to dispose of.
   */
  char **pool;
  int n_pool;
} map_t;

extern void map_free (map_t *map);

/*
 * Find the node for a room.  NULL if the room was never placed on the map:
 * ADRIFT 5 rooms created procedurally have no node, and the ADRIFT 4 layout
 * passes over rooms the author flagged to hide.
 */
extern const map_node_t *map_find (const map_t *map, const char *lockey);

/* --- rendering ---------------------------------------------------------- */

/*
 * A 24-bit RGB surface, 0x00RRGGBB per pixel, row-major: the shared
 * rasteriser's (common_utils/rgbsurface.h), which draws every primitive
 * below.
 */
typedef rgbsurf_t map_surface_t;

extern map_surface_t *map_surface_new (int w, int h);
extern void map_surface_free (map_surface_t *s);

/*
 * The host's text style, normally the Glk buffer's style_Normal.  Black on
 * white until the host says otherwise.  The map mixes them as paper and ink:
 * shaded room cards, a filled-in player's room, and faded connectors.
 */
extern void map_set_palette (unsigned int background, unsigned int text);

/*
 * What the renderer needs to know about the run.  Keeping this a callback
 * table is what lets the map be drawn from the headless harness (and diffed)
 * without linking the Glk layer, and lets one renderer serve two engines whose
 * game state has nothing in common.
 */
typedef struct map_view_s
{
  /* Has the viewpoint character seen this room? */
  int (*seen) (void *ctx, const char *lockey);
  /* The room's short description, for the label. */
  const char *(*name) (void *ctx, const char *lockey);
  /*
   * Where a usable exit in `dir` leads (restrictions applied, as in the
   * runner's HasRouteInDirection), or NULL for no exit.  An exit whose
   * destination has not been seen is drawn as a stub arrow.  May be NULL.
   */
  const char *(*exit_dest) (void *ctx, const char *lockey, int dir);
  /*
   * Has this exit's movement restriction ever evaluated false (the ADRIFT 5
   * runner's clsDirection.bEverBeenBlocked)?  When set, the renderer applies
   * the ADRIFT 5 runner's route gates: a restricted connector is hidden while
   * its restrictions currently fail and drawn solid until the player has been
   * blocked there once (Map.vb:1429/1447), and the IN/OUT badges only show
   * while their route is open (Map.vb:1328/1337).  NULL for ADRIFT 4, whose
   * runner drew every connector between rooms it laid out.
   */
  int (*ever_blocked) (void *ctx, const char *lockey, int dir);
  void *ctx;
} map_view_t;

/* Where the map is currently looking. */
typedef struct map_camera_s
{
  int page;                   /* page to draw                                */
  int scale;                  /* pixels per map unit (runner default 10)     */
  int cx, cy;                 /* centre of the view, in map units * scale    */
  int chrome_h;               /* strip along the top that the host keeps for
                                 the pan/zoom buttons (MAP_CHROME_H); the map
                                 is framed and drawn in what is left below
                                 it.  0 for no buttons.                      */
} map_camera_t;

/*
 * Pixels per map unit.  The automatic fit stays between MAP_SCALE_MIN, the
 * runner's own scale, and MAP_SCALE_MAX; a map too big for its window at the
 * minimum is panned across rather than shrunk until its labels are unreadable.
 * A manual zoom may go on up to MAP_ZOOM_MAX, or down to MAP_ZOOM_MIN for an
 * overview of a big map; below MAP_SCALE_MIN the rooms are drawn without
 * their names, which no longer fit in them.
 */
#define MAP_ZOOM_MIN 3
#define MAP_SCALE_MIN 10
#define MAP_SCALE_MAX 16
#define MAP_ZOOM_MAX 32

/* The floating pan/zoom buttons, drawn over the top-right of the map. */
enum
{
  MAP_CHROME_NONE = 0,
  MAP_CHROME_PAN_L,
  MAP_CHROME_PAN_R,
  MAP_CHROME_PAN_U,
  MAP_CHROME_PAN_D,
  MAP_CHROME_ZOOM_IN,
  MAP_CHROME_ZOOM_OUT
};

static inline unsigned int
map_chrome_bit (int id)
{
  return 1u << id;
}

/*
 * Height of the button row, padding included: what a host that wants the
 * buttons puts in map_camera_t.chrome_h.
 */
#define MAP_CHROME_H 20

/* What map_frame found out about the view, which is what the buttons need. */
typedef struct map_chrome_s
{
  int fit_scale;              /* the scale the automatic fit would pick, even
                                 while a manual zoom is overriding it        */
  unsigned int enabled;       /* map_chrome_bit() of each button that would
                                 change the view: a pan with travel left, a
                                 zoom in below MAP_ZOOM_MAX, a zoom out above
                                 MAP_ZOOM_MIN.  The rest are drawn greyed.   */
} map_chrome_t;

static inline int
map_chrome_enabled (const map_chrome_t *chrome, int id)
{
  return (chrome->enabled & map_chrome_bit (id)) != 0;
}

/*
 * Point the camera: pick the page, and frame the rooms seen on it into `dst`
 * under cam->chrome_h, which the caller sets.  With `zoom` 0 the scale is the
 * automatic fit; a positive `zoom` pins it (within MAP_ZOOM_MIN and
 * MAP_ZOOM_MAX).  An axis the rooms fit along is centred on them.  One they
 * overflow is centred on the player's room if `follow` (the runner's
 * LockPlayerCentre), and otherwise keeps the cam->cx/cy passed in -- the last
 * frame's, or a map_pan since -- and either way stops at the edge of the
 * rooms.  A hidden or missing player room is never followed.  `chrome`, if
 * not NULL, is filled in for the buttons.
 */
extern void map_frame (const map_t *map, const map_view_t *view,
                       const char *player_key, const map_surface_t *dst,
                       int zoom, int follow, map_camera_t *cam,
                       map_chrome_t *chrome);

/*
 * The next manual zoom level in from (dir > 0) or out from (dir <= 0) `scale`
 * pixels per map unit.  Returns `scale` unchanged at the end of the range,
 * which is how a caller knows to warn instead of redraw.
 */
extern int map_zoom_step (int scale, int dir);

/*
 * Draw the button row over a rendered map.  The pan buttons are only there
 * while the map overflows its window.
 */
extern void map_chrome_draw (map_surface_t *dst, const map_chrome_t *chrome);

/*
 * Which button of a `w`-pixel-wide map is at (px,py), or MAP_CHROME_NONE.  A
 * greyed button still answers, so that the host can swallow the click rather
 * than let it through to the room underneath: ask map_chrome_enabled.
 */
extern int map_chrome_hit (int w, const map_chrome_t *chrome, int px, int py);

/*
 * Move the camera one press of pan button `button` across a `w` x `h` view:
 * about a quarter of it.  The next map_frame (with `follow` off) stops it at
 * the edge of the map.
 */
extern void map_pan (map_camera_t *cam, int w, int h, int button);

/*
 * Draw the map.  Only rooms the player has seen are drawn (as in both
 * runners); the player's own room is highlighted.
 */
extern void map_render (const map_t *map, const map_view_t *view,
                        const char *player_key, const map_camera_t *cam,
                        map_surface_t *dst);

/*
 * Whether map_render would put anything at all on the page the player is on:
 * at least one room that has been seen and is not hidden.  Games that run
 * their title and options screens from a hidden staging room draw a blank
 * rectangle until the player reaches somewhere real, which is worth not
 * opening a pane for.
 */
extern int map_has_content (const map_t *map, const map_view_t *view,
                            const char *player_key);

/*
 * Which room is at pixel (px,py) of a `w` x `h` map view?  NULL if none.
 * Lets a click walk there.
 */
extern const char *map_hit (const map_t *map, const map_view_t *view,
                            const map_camera_t *cam, int w, int h,
                            int px, int py);

/*
 * Which way out of the player's room a click at (px,py) beside it points: the
 * compass direction (DIR_N..DIR_NW, never a badge direction) of the side or
 * corner of the room box the click is off, or -1.  Only a click within one
 * room's pitch of the box counts, and only if there is a usable exit that
 * way -- which, unlike a walk to a room, may lead somewhere not yet seen.
 */
extern int map_hit_dir (const map_t *map, const map_view_t *view,
                        const map_camera_t *cam, int w, int h,
                        const char *player_key, int px, int py);

/*
 * The first step of the shortest route from `from` to `to`: a direction index
 * (0..11), or -1 if there is no route.  This is the runner's map-click walk
 * (clsCharacter.Dijkstra / DoWalk): edges are the restriction-checked exits,
 * and only rooms the player has already seen may be walked through.  The
 * caller submits that direction as an ordinary command, one room per turn,
 * exactly as DoWalk does.
 */
extern int map_walk_step (const map_view_t *view, const char *from,
                          const char *to);

#endif /* MAPDRAW_H */
