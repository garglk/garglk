/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

/*
 * os_glk_map.cpp: the Glk map pane, shared by both engines -- the map
 * window and its preferences, the "glk map" and "glk zoom" commands, and
 * the map-click walks.  Split out of os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

/* The map, shown in a graphics window to the right of the story on request
   ("map").  Both engines use this pane; what differs is where the map comes
   from.  An ADRIFT 5 map is authored data, parsed once per adventure, aliasing
   its XML, and so dropped whenever the adventure is.  An ADRIFT 4 map has to be
   derived from the exit graph, and it depends on the room you are standing in
   and on how much you have explored -- so it is rebuilt on every redraw, as the
   ADRIFT 4 runner rebuilt it every time it drew (drawmap with mode = 1). */
winid_t gsc_map_window = nullptr;
map_t *gsc_map = nullptr;
int gsc_map_shown = FALSE;
/* Whether the map pane is wanted at all: the player's remembered choice, or
   failing that the layout the game shipped (gsc_map_auto_reveal).  Wanted is
   not the same as shown -- a map with nothing on it yet is held back until
   there is something to see (gsc_map_worth_opening), and the per-turn redraw
   opens it then.  FALSE until something decides. */
int gsc_map_want = FALSE;
/* Where the map pane sits: the standard 40% pane to the right of the story,
   or -- for games with wide maps -- a 30% band across the top of the whole
   screen, above the status line ("glk map top").  The choice is kept, so
   hiding and re-showing the map does not move it, and it is remembered with
   the visibility (gsc_map_pref_write) so neither does restarting. */
int gsc_map_at_top = FALSE;
/* Set when the game defines a MAP command of its own (Lost Coastlines has a
   sea chart): the game's command wins, and the pane is reached with the
   "glk map" escape instead. */
int gsc_map_taken = FALSE;
/* The restart the pane was last set up for (run_get_restart_count), so that an
   ADRIFT 4 restart -- which happens entirely inside the interpreter -- is
   noticed as the replayed opening arrives (gsc_map_notice_restart). */
static scr_int gsc_map_restarts = 0;

/* The camera, button state and pixel size of the last map redraw, so a mouse
   click can be hit-tested against exactly what is on screen. */
map_camera_t gsc_map_cam;
static map_chrome_t gsc_map_chrome;
static int gsc_map_px_w = 0, gsc_map_px_h = 0;

/* A manual zoom ("glk zoom in/out/N", or the zoom buttons), as pixels per map
   unit; 0 while the map is fitting itself to its window ("glk zoom auto", the
   default).  A manual zoom is kept until "auto" puts it back. */
int gsc_map_zoom = 0;

/* Whether a map too big for its window keeps the player's room in view.  A
   pan button turns that off, so the view stays where the player put it; it
   comes back when they walk into a different room, which gsc_map_last_player
   (the room of the last redraw; a room key, as long as gsc_a5_walk_to's) is
   there to notice. */
int gsc_map_follow = TRUE;
char gsc_map_last_player[256] = "";

static int gsc_map_chrome_click (int px, int py);
static int gsc_map_zoom_apply (int dir);

/* The pixels currently on screen, kept so that a redraw need only send the rows
   that have changed.  The map is redrawn at every prompt, but most turns do not
   move the player and so do not change a single pixel; a turn that does move him
   usually changes only part of the pane.  Sending the whole surface regardless
   is what costs: Glk has no blit, so each row of the map arrives at the display
   as a run of glk_window_fill_rect() calls, and Spatterlight turns every one of
   them into an NSRectFill.  Comparing against these pixels first is much cheaper
   than drawing them again.
   The comparison is only valid while the window still holds what we last put
   there, so gsc_map_full_flush forces the whole surface out again whenever it
   may not: after an arrange or redraw event (the window is resized, and its
   backing image with it), and when the pane is opened or cleared. */
static map_surface_t *gsc_map_screen = nullptr;
int gsc_map_full_flush = TRUE;

void
gsc_map_screen_drop (void)
{
  map_surface_free (gsc_map_screen);
  gsc_map_screen = nullptr;
  gsc_map_full_flush = TRUE;
}

/* A walk in progress, from clicking a room on the map (clsCharacter.WalkTo /
   DoWalk): the target room, and the room we set off from on the last step so a
   step that fails to move us can abort the walk, as DoWalk's sLastPosition
   does.  Each step is submitted as an ordinary direction command, one room per
   turn. */
char gsc_a5_walk_to[256] = "";
/* A click beside the player's room rather than on another one walks a single
   step that way, wherever it leads: the direction to take, or -1.  Both
   engines share it, as only one of them is ever running. */
static int gsc_map_step_dir = -1;
static char gsc_a5_walk_last[256] = "";
int gsc_a5_walk_clicked = FALSE;
static int gsc_a5_walk_steps = 0;
/* A walk is bounded: a game that shuffles the player around could otherwise
   keep the route recomputing for ever. */
#define GSC_A5_WALK_MAX 100
static void gsc_a5_map_view (a5_state_t *st, map_view_t *view);

void
gsc_a5_walk_stop (void)
{
  gsc_a5_walk_to[0] = '\0';
  gsc_a5_walk_last[0] = '\0';
  gsc_a5_walk_steps = 0;
  gsc_map_step_dir = -1;
}

/* The same walk, for ADRIFT 4.  Room keys here are just room numbers, and the
   step is submitted as a direction word through SCARE's own parser. */
static char gsc_sc_walk_to[16] = "";
static char gsc_sc_walk_last[16] = "";
int gsc_sc_walk_clicked = FALSE;
static int gsc_sc_walk_steps = 0;
#define GSC_SC_WALK_MAX 100

static int gsc_map_available (void);

static void
gsc_sc_walk_stop (void)
{
  gsc_sc_walk_to[0] = '\0';
  gsc_sc_walk_last[0] = '\0';
  gsc_sc_walk_steps = 0;
  gsc_map_step_dir = -1;
}


/*
 * gsc_map_click()
 *
 * Act on a mouse click that arrived while a line of input was pending.  A
 * click on a room of the map walks the player there, one room per turn
 * (Map.vb imgMap_MouseDown: Player.WalkTo = node; DoWalk()): the pending
 * line request is cancelled so that the line read can issue the first step
 * in its place, and whatever the player had half-typed is discarded.
 * Returns TRUE when a walk was started, and `event` is then the LineInput
 * event the cancel produced.  A click just beside the player's own room walks
 * one step through the exit on that side, if it has one.  A click on other
 * empty map, on the player's own room or on one with no way there re-arms the
 * mouse request and returns FALSE; a click anywhere but the map returns
 * FALSE.
 */
int
gsc_map_click (event_t *event)
{
  map_view_t view;
  char here_room[16];
  const char *hit, *here;
  int step = -1;

  if (event->win != gsc_map_window)
    return FALSE;

  if (gsc_is_a5)
    {
      a5_state_t *st;

      if (gsc_a5_run == nullptr)
        return FALSE;
      st = a5run_state (gsc_a5_run);
      gsc_a5_map_view (st, &view);
      here = a5state_player_location (st);
    }
  else
    {
      if (gsc_map == nullptr || gsc_game == nullptr)
        return FALSE;
      scmap_view ((scr_gameref_t) gsc_game, &view);
      snprintf (here_room, sizeof here_room, "%ld",
                (long) gs_playerroom ((scr_gameref_t) gsc_game));
      here = here_room;
    }

  /* The floating pan/zoom buttons: act on them, re-arm, and keep waiting. */
  if (gsc_map_chrome_click ((int) event->val1, (int) event->val2))
    {
      if (glk_gestalt (gestalt_MouseInput, wintype_Graphics))
        glk_request_mouse_event (gsc_map_window);
      return FALSE;
    }

  hit = map_hit (gsc_map, &view, &gsc_map_cam, gsc_map_px_w, gsc_map_px_h,
                 (int) event->val1, (int) event->val2);
  /* Only a room there is a way to now starts a walk.  One behind a shut door
     is left alone like empty map, before the line request is touched:
     cancelling it would end the prompt's line, and the walk would then give
     up at once with nothing to show for it. */
  if (hit != nullptr && here != nullptr
      && (strcmp (hit, here) == 0 || map_walk_step (&view, here, hit) < 0))
    hit = nullptr;
  /* A click on no room, but just off a side or corner of the player's own,
     is one step out through the exit on that side.  The walk is then to
     wherever that exit leads, with its first step already chosen. */
  else if (hit == nullptr && here != nullptr)
    {
      step = map_hit_dir (gsc_map, &view, &gsc_map_cam, gsc_map_px_w,
                          gsc_map_px_h, here, (int) event->val1,
                          (int) event->val2);
      if (step >= 0)
        hit = view.exit_dest (view.ctx, here, step);
    }
  if (hit != nullptr)
    {
      if (gsc_is_a5)
        {
          gsc_a5_walk_stop ();
          snprintf (gsc_a5_walk_to, sizeof gsc_a5_walk_to, "%s", hit);
          gsc_a5_walk_clicked = TRUE;
        }
      else
        {
          gsc_sc_walk_stop ();
          snprintf (gsc_sc_walk_to, sizeof gsc_sc_walk_to, "%s", hit);
          gsc_sc_walk_clicked = TRUE;
        }
      gsc_map_step_dir = step;
      glk_cancel_line_event (gsc_main_window, event);
      return TRUE;
    }

  /* A click on empty map: re-arm and keep waiting. */
  if (glk_gestalt (gestalt_MouseInput, wintype_Graphics))
    glk_request_mouse_event (gsc_map_window);
  return FALSE;
}

/*
 * gsc_map_click_view()
 *
 * The same click with no line of input pending -- at a <waitkey> pause, a
 * timed <wait>, a yes-or-no question.  There is no command to give, so a
 * room is not walked to, but the pan and zoom buttons work as they do at
 * the prompt: a cutscene can hold the screen for a long while, and the map
 * beside it is still the player's to look around.
 */
void
gsc_map_click_view (event_t *event)
{
  if (event->win != gsc_map_window)
    return;
  gsc_map_chrome_click ((int) event->val1, (int) event->val2);
  if (glk_gestalt (gestalt_MouseInput, wintype_Graphics))
    glk_request_mouse_event (gsc_map_window);
}


/*---------------------------------------------------------------------*/
/*  ADRIFT 5 map window                                                */
/*---------------------------------------------------------------------*/

/*
 * The map view callbacks: what a5map needs to know about this run.  They read
 * the same state the runner's map does -- the player's seen-set, room short
 * names, and the restriction-checked exits.
 */
static int
gsc_a5_map_seen (void *ctx, const char *lockey)
{
  a5_state_t *st = (a5_state_t *) ctx;
  int li = a5state_location_index (st, lockey);

  return li >= 0 && st->loc_seen != nullptr && st->loc_seen[li];
}

/*
 * Room labels.  a5text_location_short_plain allocates, but a5map wants a
 * borrowed string, so cache the names for the lifetime of one redraw.
 */
#define GSC_A5_MAP_NAMES 512
static char *gsc_a5_map_name_cache[GSC_A5_MAP_NAMES];
static const char *gsc_a5_map_name_key[GSC_A5_MAP_NAMES];
static int gsc_a5_map_n_names = 0;

void
gsc_a5_map_names_clear (void)
{
  int i;

  for (i = 0; i < gsc_a5_map_n_names; i++)
    free (gsc_a5_map_name_cache[i]);
  gsc_a5_map_n_names = 0;
}

static const char *
gsc_a5_map_name (void *ctx, const char *lockey)
{
  a5_state_t *st = (a5_state_t *) ctx;
  char *name;
  int i;

  for (i = 0; i < gsc_a5_map_n_names; i++)
    if (gsc_a5_map_name_key[i] == lockey)
      return gsc_a5_map_name_cache[i];

  name = a5text_location_short_plain (st, lockey);
  if (name == nullptr)
    return nullptr;
  /* Map labels are drawn outside the story window, where the turn loop's own
     strip pass never reaches: a room whose name lost a tag would otherwise
     carry the stripped-tag sentinel into the box. */
  a5text_strip_pres_marks (name);
  if (gsc_a5_map_n_names >= GSC_A5_MAP_NAMES)
    {
      /* Cache full (a huge page): draw this one and drop it. */
      static char scratch[128];
      snprintf (scratch, sizeof scratch, "%s", name);
      free (name);
      return scratch;
    }
  gsc_a5_map_name_key[gsc_a5_map_n_names] = lockey;
  gsc_a5_map_name_cache[gsc_a5_map_n_names] = name;
  gsc_a5_map_n_names++;
  return name;
}

static const char *
gsc_a5_map_exit_dest (void *ctx, const char *lockey, int dir)
{
  a5_state_t *st = (a5_state_t *) ctx;

  return a5restr_exit_in_direction (st, a5state_player_key (st), lockey,
                                    map_dirs[dir], nullptr);
}

static int
gsc_a5_map_ever_blocked (void *ctx, const char *lockey, int dir)
{
  a5_state_t *st = (a5_state_t *) ctx;

  return a5restr_ever_blocked (st, lockey, map_dirs[dir]);
}

static void
gsc_a5_map_view (a5_state_t *st, map_view_t *view)
{
  /* The runner draws its map only after PrepareForNextTurn has emptied the
     per-turn route memo, so a route un-blocked late in the turn is re-checked
     fresh by the draw; drop the memo here to match.  (The turn loop clears it
     again at the next command, so entries seeded by drawing never leak into
     game-visible restriction checks.) */
  a5restr_route_cache_clear (st);
  view->seen = gsc_a5_map_seen;
  view->name = gsc_a5_map_name;
  view->exit_dest = gsc_a5_map_exit_dest;
  view->ever_blocked = gsc_a5_map_ever_blocked;
  view->ctx = st;
}

/*
 * gsc_map_current()
 *
 * The map to draw, and the room to centre it on, for whichever engine is
 * running.  For ADRIFT 5 that is the map parsed at load; for ADRIFT 4 there is
 * nothing to parse, so the layout is recomputed here -- it is derived from the
 * room you are in and the rooms you have seen, both of which change as you
 * play, and the runner likewise recomputed it every time it drew.
 *
 * Returns FALSE if there is no map to show.  Mid-game ADRIFT 4 HideOnMap keeps
 * the previous layout so the pan/zoom view can freeze on the last-known map
 * instead of wiping the pane.
 */
static int
gsc_map_current (map_view_t *view, const char **player, char *keybuf,
                 int keysize)
{
  map_t *built;

  if (gsc_is_a5)
    {
      a5_state_t *st;

      if (gsc_a5_run == nullptr || gsc_map == nullptr)
        return FALSE;
      st = a5run_state (gsc_a5_run);
      gsc_a5_map_view (st, view);
      *player = a5state_player_location (st);
      return TRUE;
    }

  if (gsc_game == nullptr)
    return FALSE;

  scmap_view ((scr_gameref_t) gsc_game, view);
  built = scmap_build ((scr_gameref_t) gsc_game, view);
  if (built != nullptr)
    {
      map_free (gsc_map);
      gsc_map = built;
    }
  else if (gsc_map == nullptr)
    return FALSE;
  /* else no layout from this room (HideOnMap mid-game): keep the last one. */

  snprintf (keybuf, (size_t) keysize, "%ld",
            (long) gs_playerroom ((scr_gameref_t) gsc_game));
  *player = keybuf;
  return TRUE;
}

/*
 * gsc_map_worth_opening()
 *
 * Whether opening the pane right now would show the player anything.
 *
 * ADRIFT 5 games routinely run their title, credits and options screens from a
 * staging room the map hides (Location <Hide>), and a page whose only seen room
 * is hidden renders as an empty rectangle -- The Axe of Kolt spends three
 * screens there before the game proper begins.  Rather than put up a blank
 * pane, opening waits; every redraw asks again, so the map arrives with the
 * first real room.
 *
 * ADRIFT 4 hides its map the same way, but a step earlier: the layout is
 * derived from the room you are standing in, and from a room the author flagged
 * HideOnMap the runner drew nothing at all (Form29.drawmap bails, so scmap_build
 * hands back no map).  The PK Girl opens in such a room and plays its whole
 * introduction there.  That is the same "wait for a real room" case, not a
 * failure, so it defers too.
 *
 * A map that cannot be built for any other reason answers TRUE, because that is
 * a different complaint and one the pane makes for itself: the game may have no
 * map to show, or ADRIFT 4's layout may have given up ("too complex"), and the
 * player asking for the map deserves to be told so.
 */
static int
gsc_map_worth_opening (void)
{
  map_view_t view;
  const char *ploc = nullptr;
  char keybuf[16];

  if (!gsc_map_current (&view, &ploc, keybuf, sizeof keybuf))
    return gsc_is_a5 || !gsc_map_available () || scmap_failed () != 0;
  return map_has_content (gsc_map, &view, ploc);
}

/*
 * gsc_map_zoom_apply()
 *
 * Step the manual zoom in (dir > 0) or out (dir <= 0) along map_zoom_step's
 * ladder, from the manual zoom or from wherever the automatic fit last landed.
 * Zooming out from a manual zoom above the automatic fit stops at the fit,
 * and goes back to automatic there; from the fit it goes on down the ladder,
 * to MAP_ZOOM_MIN.  Returns TRUE if the zoom changed.
 */
static int
gsc_map_zoom_apply (int dir)
{
  int scale = gsc_map_zoom > 0 ? gsc_map_zoom : gsc_map_cam.scale;
  int zoom;

  /* A map with nothing drawn yet steps from the runner's default. */
  if (scale < MAP_ZOOM_MIN)
    scale = MAP_SCALE_MIN;
  zoom = map_zoom_step (scale, dir);
  if (dir > 0)
    {
      if (zoom == scale)
        return FALSE;
    }
  else
    {
      if (zoom == scale)
        return FALSE;
      if (zoom <= gsc_map_chrome.fit_scale
          && scale > gsc_map_chrome.fit_scale)
        zoom = 0;
    }

  if (zoom == gsc_map_zoom)
    return FALSE;
  gsc_map_zoom = zoom;
  gsc_map_redraw ();
  return TRUE;
}

/*
 * gsc_map_chrome_click()
 *
 * Handle a click on the floating pan/zoom buttons.  Returns TRUE if the click
 * landed on one, greyed or not: a greyed button swallows its click, so that it
 * does not fall through to the map underneath and walk to a room.
 */
static int
gsc_map_chrome_click (int px, int py)
{
  int id = map_chrome_hit (gsc_map_px_w, &gsc_map_chrome, px, py);

  if (id == MAP_CHROME_NONE)
    return FALSE;
  if (!map_chrome_enabled (&gsc_map_chrome, id))
    return TRUE;

  switch (id)
    {
    case MAP_CHROME_ZOOM_IN:
      gsc_map_zoom_apply (1);
      break;

    case MAP_CHROME_ZOOM_OUT:
      gsc_map_zoom_apply (-1);
      break;

    default:
      /* A pan: the view now stays put until the player changes rooms. */
      gsc_map_follow = FALSE;
      map_pan (&gsc_map_cam, gsc_map_px_w, gsc_map_px_h, id);
      gsc_map_redraw ();
      break;
    }
  return TRUE;
}

/* One block of a colour, for rgbsurf_send_rects. */
static void
gsc_map_rect (void * /* ctx */, unsigned int rgb, int x, int y, int w, int h)
{
  glk_window_fill_rect (gsc_map_window, rgb, x, y, (glui32) w, (glui32) h);
}

/*
 * gsc_map_redraw()
 *
 * Rasterise the map and blit it into the graphics window.  Glk has no drawing
 * primitives beyond a filled rectangle, so -- as in the Comprehend port -- the
 * page is rendered into an RGB surface and flushed as run-length-encoded
 * spans, with identical spans on consecutive rows merged into one taller
 * rectangle (rgbsurf_send_rects): far cheaper than one fill_rect per pixel,
 * or per row.
 *
 * Only the rows that differ from what is already on screen (gsc_map_screen) are
 * flushed, so a turn that leaves the map alone -- most of them -- sends nothing
 * at all.
 */
void
gsc_map_redraw (void)
{
  map_surface_t *surf;
  map_view_t view;
  const char *ploc = nullptr;
  char keybuf[16];
  glui32 w, h;
  int buttons;

  /* A map that was asked for while there was nothing on it: try again now
     that the game has moved on.  (gsc_map_show comes straight back here, but
     with gsc_map_shown set, so this runs once.) */
  if (gsc_map_want && !gsc_map_shown && gsc_map_worth_opening ())
    gsc_map_show ();

  if (!gsc_map_window)
    return;

  glk_window_get_size (gsc_map_window, &w, &h);
  if (w == 0 || h == 0)
    return;

  /* The map is drawn in the story's colours: the buffer's normal style
     supplies the background and text colour.  Colour mode publishes those as
     Normal stylehints (and rebuilds windows so they stick); libraries that
     ignore author styles leave measure on the theme, matching the transcript.
     A Glk that cannot measure leaves the palette at its black-on-white
     default.  The measurement comes from gsc_normal_measure's cache -- the
     map redraws at every prompt, and each miss is two synchronous
     round-trips to the application. */
  {
    glui32 bg, fg;

    if (gsc_normal_measure (&fg, &bg))
      {
        map_set_palette (bg, fg);
        /* So the clear below, and any exposed edge, match the surface. */
        glk_window_set_background_color (gsc_map_window, bg);
      }
  }

  /* Nothing to draw yet: no map has ever been built (opening staging room).
     Mid-game HideOnMap keeps the previous layout via gsc_map_current. */
  if (!gsc_map_current (&view, &ploc, keybuf, sizeof keybuf))
    {
      glk_window_clear (gsc_map_window);
      gsc_map_screen_drop ();
      return;
    }

  /* Moving to a different visible room restores follow-the-player.  A hidden
     room leaves the camera where it was.  The first redraw after open/restore
     only records the room -- it must not override a restored pan. */
  if (ploc != nullptr)
    {
      if (gsc_map_last_player[0] != '\0'
          && strcmp (ploc, gsc_map_last_player) != 0)
        {
          const map_node_t *pn = map_find (gsc_map, ploc);

          if (pn != nullptr && !pn->hidden)
            gsc_map_follow = TRUE;
        }
      snprintf (gsc_map_last_player, sizeof gsc_map_last_player, "%s", ploc);
    }

  surf = map_surface_new ((int) w, (int) h);
  if (surf == nullptr)
    return;

  /* The pan/zoom buttons only earn their strip of the window where they can
     be clicked. */
  buttons = glk_gestalt (gestalt_MouseInput, wintype_Graphics) != 0;
  gsc_map_cam.chrome_h = buttons ? MAP_CHROME_H : 0;
  map_frame (gsc_map, &view, ploc, surf, gsc_map_zoom, gsc_map_follow,
             &gsc_map_cam, &gsc_map_chrome);
  map_render (gsc_map, &view, ploc, &gsc_map_cam, surf);
  if (buttons)
    map_chrome_draw (surf, &gsc_map_chrome);
  gsc_map_px_w = surf->w;
  gsc_map_px_h = surf->h;
  if (gsc_is_a5)
    gsc_a5_map_names_clear ();

  /* The window was resized under us, so the pixels we think are on screen are
     not the ones that are. */
  if (gsc_map_screen != nullptr
      && (gsc_map_screen->w != surf->w || gsc_map_screen->h != surf->h))
    gsc_map_screen_drop ();

  /* Untouched rows: what is on screen is already right. */
  rgbsurf_send_rects (surf, gsc_map_full_flush ? nullptr : gsc_map_screen,
                      gsc_map_rect, nullptr);

  /* These pixels are the screen now. */
  map_surface_free (gsc_map_screen);
  gsc_map_screen = surf;
  gsc_map_full_flush = FALSE;

  /* Arm the click that walks the player to a room.  Mouse requests are
     one-shot, so this is re-armed after every redraw. */
  if (glk_gestalt (gestalt_MouseInput, wintype_Graphics))
    glk_request_mouse_event (gsc_map_window);
}

/*
 * gsc_sc_walk_next()
 *
 * The next step of a map-click walk in an ADRIFT 4 game, as the direction
 * command that makes it.  The route only runs through rooms already visited,
 * and only along exits whose restrictions are currently satisfied, so it can
 * always be walked.  Gives up on arrival, when no route remains, or when the
 * last step did not actually move us -- something in the game blocked it.
 */
int
gsc_sc_walk_next (scr_char *buffer, scr_int length)
{
  map_view_t view;
  char from[16];
  const scr_char *word;
  int dir;

  if (gsc_sc_walk_to[0] == '\0' || gsc_game == nullptr)
    return FALSE;
  if (++gsc_sc_walk_steps > GSC_SC_WALK_MAX)
    {
      gsc_sc_walk_stop ();
      return FALSE;
    }

  /* A single step beside the room: take it, and that is the whole walk. */
  if (gsc_map_step_dir >= 0)
    {
      word = lib_direction_name (gsc_map_step_dir);
      gsc_sc_walk_stop ();
      if (word == nullptr)
        return FALSE;
      snprintf (buffer, (size_t) length, "%s", word);
      return TRUE;
    }

  snprintf (from, sizeof from, "%ld",
            (long) gs_playerroom ((scr_gameref_t) gsc_game));
  if (strcmp (from, gsc_sc_walk_to) == 0
      || (gsc_sc_walk_last[0] != '\0' && strcmp (from, gsc_sc_walk_last) == 0))
    {
      gsc_sc_walk_stop ();
      return FALSE;
    }

  scmap_view ((scr_gameref_t) gsc_game, &view);
  dir = map_walk_step (&view, from, gsc_sc_walk_to);
  word = (dir >= 0) ? lib_direction_name (dir) : nullptr;
  if (word == nullptr)
    {
      gsc_sc_walk_stop ();
      return FALSE;
    }

  snprintf (buffer, (size_t) length, "%s", word);
  snprintf (gsc_sc_walk_last, sizeof gsc_sc_walk_last, "%s", from);
  return TRUE;
}

/*
 * gsc_a5_walk_next()
 *
 * The next step of a map-click walk, as the game command that makes it (the
 * localized direction word, so the game's own parser takes it).  Returns FALSE
 * when the walk is over -- we have arrived, no route remains, or the last step
 * did not move us (something blocked the way), which is DoWalk's
 * sLastPosition bail-out.
 */
int
gsc_a5_walk_next (a5_run_t *run, char *buf, int bufsize)
{
  map_view_t view;
  a5_state_t *st;
  const char *from, *word;
  int dir;

  if (gsc_a5_walk_to[0] == '\0' || run == nullptr)
    return FALSE;
  if (a5run_is_over (run))
    return FALSE;                       /* the walk ended the game */
  if (++gsc_a5_walk_steps > GSC_A5_WALK_MAX)
    return FALSE;

  /* A single step beside the room: take it, and that is the whole walk. */
  if (gsc_map_step_dir >= 0)
    {
      word = a5parse_direction_name (map_dirs[gsc_map_step_dir]);
      gsc_a5_walk_stop ();
      if (word == nullptr)
        return FALSE;
      snprintf (buf, (size_t) bufsize, "%s", word);
      return TRUE;
    }

  st = a5run_state (run);
  from = a5state_player_location (st);
  if (from == nullptr)
    return FALSE;

  if (strcmp (from, gsc_a5_walk_to) == 0)
    return FALSE;                       /* arrived */
  if (gsc_a5_walk_last[0] != '\0' && strcmp (from, gsc_a5_walk_last) == 0)
    return FALSE;                       /* the last step did not move us */

  gsc_a5_map_view (st, &view);
  dir = map_walk_step (&view, from, gsc_a5_walk_to);
  if (dir < 0)
    return FALSE;

  word = a5parse_direction_name (map_dirs[dir]);
  if (word == nullptr)
    return FALSE;

  snprintf (buf, (size_t) bufsize, "%s", word);
  snprintf (gsc_a5_walk_last, sizeof gsc_a5_walk_last, "%s", from);
  return TRUE;
}

/*
 * gsc_map_available()
 *
 * Whether this game has a map at all.  In ADRIFT 5 that is settled at load:
 * it either shipped map data or it did not.  In ADRIFT 4 every game with
 * rooms has one, unless the author switched it off -- and gsc_map itself is
 * no guide there, being built lazily at the first redraw.  Whether there is
 * anything to draw *right now* is a separate question (map_has_content).
 */
static int
gsc_map_available (void)
{
  if (gsc_is_a5)
    return gsc_map != nullptr;
  return gsc_game != nullptr && scmap_available ((scr_gameref_t) gsc_game);
}

/*
 * gsc_map_default_shown()
 *
 * Whether this game would start with the map showing if the player had never
 * said otherwise: what the author's shipped Runner layout asks for in ADRIFT
 * 5, and nothing at all in ADRIFT 4, whose Runner opened with the map closed
 * however the last game left it.
 */
int
gsc_map_default_shown (void)
{
  return gsc_is_a5 && gsc_a5_adv != nullptr && gsc_a5_adv->map_pane_open == 1;
}

/*
 * gsc_map_pref_ref()
 *
 * A fileref for this game's remembered map choice, or NULL when there is
 * nothing to remember or nowhere to keep it.
 *
 * The ADRIFT 5 Runner keeps the player's window layout per game, in
 * RunnerLayout-<IFID>.xml, and writes it out on quit; that file is also what
 * overrides the layout an author shipped inside the Blorb, because the Runner
 * only unpacks the shipped one when no file for that IFID exists yet
 * (Blorb.vb:343).  This is the same store, under the same key -- two bytes,
 * '1' or '0' for whether the map is wanted and 't' or 'r' for where it goes,
 * rather than a window layout.
 *
 * Games the Runner has no key for get one of ours (gsc_game_key): ADRIFT 4
 * never had an IFID, and its Runner kept the map as a global View-menu
 * setting in the registry rather than per game, so there is nothing to be
 * compatible with -- and a few ADRIFT 5 .taf files were built without an
 * <ifid> block too.
 *
 * Games with no map and hosts with no graphics have nothing to record, and
 * must not leave a file behind for it.
 */
static frefid_t
gsc_map_pref_ref (void)
{
  const char *key;
  char name[128];

  if (!gsc_map_available ())
    return nullptr;
  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    return nullptr;

  if (gsc_is_a5 && gsc_a5_adv != nullptr
      && gsc_a5_adv->ifid != nullptr && gsc_a5_adv->ifid[0] != '\0')
    key = gsc_a5_adv->ifid;
  else if (gsc_game_key[0] != '\0')
    key = gsc_game_key;
  else
    return nullptr;

  snprintf (name, sizeof name, "scarier-map-%s", key);
  return glk_fileref_create_by_name (fileusage_Data | fileusage_TextMode,
                                     name, 0);
}

/*
 * gsc_map_pref_read()
 *
 * The player's remembered choice for this game: 1 to show the map, 0 to hide
 * it, -1 when they have never said.  Where they last put it is returned
 * through at_top on the same terms -- 1 for the top band, 0 for the pane at
 * the right, -1 for never said.  Files written before the map could be moved
 * hold only the first byte, so a missing byte reads as never said rather than
 * as a choice.  (A third byte, left by the removed "glk map colour", is
 * ignored.)
 */
int
gsc_map_pref_read (int *at_top)
{
  frefid_t fileref;
  int value = -1;

  if (at_top != nullptr)
    *at_top = -1;

  fileref = gsc_map_pref_ref ();
  if (fileref == nullptr)
    return -1;

  if (glk_fileref_does_file_exist (fileref))
    {
      strid_t stream = glk_stream_open_file (fileref, filemode_Read, 0);

      if (stream)
        {
          glui32 c = glk_get_char_stream (stream);

          if (c == '0' || c == '1')
            value = (int) (c - '0');

          c = glk_get_char_stream (stream);
          if (at_top != nullptr && (c == 't' || c == 'r'))
            *at_top = (c == 't');

          glk_stream_close (stream, nullptr);
        }
    }

  glk_fileref_destroy (fileref);
  return value;
}

/*
 * gsc_map_pref_write()
 *
 * Remember that the player asked for the map to be shown or hidden in this
 * game and where they had it, so that the next session opens the way they left
 * it.  The position is recorded even when the map is off, so that turning it
 * back on later still puts it where they last had it.
 *
 * Only a game whose map the player has actually moved away from its default
 * gets a file; one they have put back where it started has theirs removed
 * again.  So the common case -- ADRIFT 4, where the map starts hidden and
 * most players leave it that way -- costs nothing at all, and the files that
 * do accumulate are only for games the player made a decision about.
 */
static void
gsc_map_pref_write (int shown, int at_top)
{
  frefid_t fileref;
  strid_t stream;

  fileref = gsc_map_pref_ref ();
  if (fileref == nullptr)
    return;

  if (!shown == !gsc_map_default_shown () && !at_top)
    {
      if (glk_fileref_does_file_exist (fileref))
        glk_fileref_delete_file (fileref);
      glk_fileref_destroy (fileref);
      return;
    }

  stream = glk_stream_open_file (fileref, filemode_Write, 0);
  if (stream)
    {
      glk_put_char_stream (stream, (unsigned char) (shown ? '1' : '0'));
      glk_put_char_stream (stream, (unsigned char) (at_top ? 't' : 'r'));
      glk_stream_close (stream, nullptr);
    }
  glk_fileref_destroy (fileref);
}

/*
 * gsc_map_show()
 *
 * Open the map pane, in whichever position gsc_map_at_top asks for: the
 * standard pane to the right of the story, or a band across the top of the
 * screen.  The top band splits the root window, not the story, so that it
 * sits above the status line and spans the display's full width.
 */
void
gsc_map_show (void)
{
  if (!gsc_map_available ())
    {
      gsc_normal_string ("This game has no map.\n");
      return;
    }

  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    {
      gsc_normal_string ("This interpreter cannot display the map.\n");
      return;
    }

  if (gsc_map_at_top)
    gsc_map_window = glk_window_open (glk_window_get_root (),
                                         winmethod_Above
                                         | winmethod_Proportional,
                                         30, wintype_Graphics, 0);
  else
    gsc_map_window = glk_window_open (gsc_main_window,
                                         winmethod_Right
                                         | winmethod_Proportional,
                                         40, wintype_Graphics, 0);
  if (!gsc_map_window)
    {
      gsc_normal_string ("Sorry, the map window could not be opened.\n");
      return;
    }
  gsc_map_shown = TRUE;
  /* A newly opened map starts out on the player, whatever was panned to in
     the last one. */
  gsc_map_follow = TRUE;
  gsc_map_last_player[0] = '\0';
  gsc_map_screen_drop ();       /* a fresh window holds nothing */
  gsc_map_redraw ();

  /* The ADRIFT 4 layout can give up ("Cannot draw map - too complex.",
     Form29.showmapfail).  Say so here, once, rather than at every prompt: the
     pane stays open, because a layout that fails from one room may well succeed
     from the next. */
  if (!gsc_is_a5 && gsc_map == nullptr && scmap_failed () != 0)
    gsc_normal_string ("Sorry, this game's map is too complex to draw.\n");

  glk_set_window (gsc_main_window);
}

/*
 * gsc_map_hide()
 *
 * Close the map pane, dropping the record of what was drawn there.
 */
void
gsc_map_hide (void)
{
  if (gsc_map_window)
    {
      glk_window_close (gsc_map_window, nullptr);
      gsc_map_window = nullptr;
    }
  gsc_map_shown = FALSE;
  gsc_map_screen_drop ();
}

/*
 * gsc_map_set()
 *
 * Show or hide the map pane at the player's request, and remember the choice
 * for this game.  The window is opened on first use (and only for games that
 * actually have map data), then kept, so toggling is cheap.
 *
 * Remembering matters because a game can ask to start with its map showing
 * (gsc_map_auto_reveal); the player's own last word has to outlast the
 * session, or "glk map off" would be undone by the next launch.
 */
static void
gsc_map_set (int shown)
{
  int deferred = FALSE;

  if (shown)
    {
      if (!gsc_map_shown)
        {
          if (gsc_map_worth_opening ())
            gsc_map_show ();
          else
            {
              deferred = TRUE;
              gsc_normal_string ("There is nothing to put on the map from"
                                 " here; it will open by itself as soon as"
                                 " there is.\n");
            }
        }
    }
  else if (gsc_map_shown)
    {
      gsc_map_hide ();
      gsc_normal_string ("Map hidden.\n");
    }
  else if (gsc_map_want)
    /* Called off before it ever got to open. */
    gsc_normal_string ("Map hidden.\n");

  /* What actually happened, not what was asked for: opening can fail outright
     (no map, no graphics), and a pane that cannot exist is not a preference
     worth keeping.  A deferred open is the opposite case -- the player did
     ask, and the map is on its way. */
  gsc_map_want = gsc_map_shown || deferred;
  gsc_map_pref_write (gsc_map_want, gsc_map_at_top);
}

/*
 * gsc_map_toggle()
 *
 * Show the map pane if it is hidden, hide it if it is shown -- or cancel it if
 * it is merely on its way.
 */
void
gsc_map_toggle (void)
{
  gsc_map_set (!(gsc_map_shown || gsc_map_want));
}

/*
 * gsc_map_auto_reveal()
 *
 * Open the map pane at the start of a game that asks for it.
 *
 * ADRIFT 5 games can carry the author's own Runner window layout in their
 * Blorb (a5blorb_find_layout), and run500.exe starts with whatever panes that
 * layout has open -- which is the whole of why a handful of games "come with
 * the map already showing" and the rest do not.  There is no authored
 * map-visibility setting, and ADRIFT 4 has no equivalent at all: run400's map
 * was a View-menu toggle stored globally in the registry, never per game.
 *
 * A choice the player made themselves wins over the author's, exactly as the
 * Runner's own saved RunnerLayout-<IFID>.xml wins over the shipped one -- in
 * either direction, so a player who once opened the map in this game gets it
 * back too.  That part applies to ADRIFT 4 as well, which is why this runs on
 * both paths: the game has nothing to say about the map, but the player does.
 * Silent throughout: a host that cannot draw a map, or a game with none, just
 * gets no pane.
 */
void
gsc_map_auto_reveal (void)
{
  int pref, at_top;

  if (gsc_map_shown)
    return;

  gsc_map_want = FALSE;
  if (!gsc_map_available ())
    return;
  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    return;

  pref = gsc_map_pref_read (&at_top);

  /* Where the map goes is remembered whether or not it is opened now, so that
     a later "glk map on" puts it back the way the player last had it. */
  if (at_top >= 0)
    gsc_map_at_top = at_top;

  if (pref < 0)
    {
      /* Never asked: follow the layout the author shipped, if any. */
      if (!gsc_is_a5 || gsc_a5_adv == nullptr || gsc_a5_adv->map_pane_open != 1)
        return;
    }
  else if (pref == 0)
    return;

  /* Wanted -- but an opening played out from a room the map hides has nothing
     to put in the pane, so the map may have to wait for the first real room
     (gsc_map_worth_opening, and the retry in gsc_map_redraw). */
  gsc_map_want = TRUE;
  if (gsc_map_worth_opening ())
    gsc_map_show ();
}

/*
 * gsc_map_notice_restart()
 *
 * Take the map pane down for a replayed ADRIFT 4 opening, and let it be decided
 * again from scratch.
 *
 * The ADRIFT 5 path does this where the restart happens (gsc_a5_restart_run);
 * ADRIFT 4 has nowhere to do it, because the restart never surfaces here.  Both
 * RESTART at the prompt and the restart offered when the game ends are handled
 * inside scr_interpret_game, which just replays the opening -- so the pane goes
 * into the new game still showing the old one's layout, and for a game whose
 * opening plays out in a room the map hides, like The PK Girl, that means the
 * empty rectangle gsc_map_worth_opening exists to keep off the screen.  Watch
 * the interpreter's restart count for a change instead (run_get_restart_count).
 *
 * The zoom goes with the old run for the same reason it does in ADRIFT 5: the
 * new one is back to a single room, which a scale chosen for a whole map would
 * show far too close in.
 *
 * Called at the first sign of the replayed opening -- its first line of output,
 * or failing that its first prompt -- so that the opening is laid out at the
 * width it will keep.  Opening and closing windows moves the current stream
 * about (gsc_map_show leaves output pointed at the story window), which the
 * caller mid-print is not expecting, so put it back.
 */
void
gsc_map_notice_restart (void)
{
  strid_t stream;
  scr_int restarts;

  if (gsc_is_a5 || gsc_game == nullptr)
    return;

  restarts = run_get_restart_count ();
  if (restarts == gsc_map_restarts)
    return;
  gsc_map_restarts = restarts;

  stream = glk_stream_get_current ();
  gsc_map_hide ();
  gsc_map_zoom = 0;
  gsc_map_auto_reveal ();
  glk_stream_set_current (stream);
}

/*
 * gsc_map_place()
 *
 * Put the map pane at the top of the screen ("glk map top", for games with
 * wide maps) or back at the right of the story ("glk map right").  A map
 * already shown in the other position is closed and reopened in the new one.
 */
static void
gsc_map_place (int at_top)
{
  if (gsc_map_shown && gsc_map_at_top == at_top)
    {
      gsc_normal_string (at_top
                         ? "The map is already at the top of the screen.\n"
                         : "The map is already at the right.\n");
      return;
    }

  gsc_map_hide ();
  gsc_map_at_top = at_top;

  /* Asking where the map goes is asking for a map -- through gsc_map_set, so
     that the request is remembered and honoured later even if there is nothing
     to draw from where the player is standing. */
  gsc_map_set (TRUE);
}

/*
 * gsc_command_map()
 *
 * "glk map [on|off]".  Always available, even for the rare game that defines a
 * MAP command of its own (see gsc_map_taken).
 */
void
gsc_command_map (const char *argument)
{
  if (gsc_is_a5 ? gsc_a5_run == nullptr : gsc_game == nullptr)
    return;

  if (strlen (argument) == 0)
    {
      gsc_map_toggle ();
      return;
    }
  /* Explicit on/off go through gsc_map_set even when they change nothing, so
     that "glk map off" in a game that opens with its map showing is recorded
     as a veto rather than shrugged off as a no-op. */
  if (scr_strcasecmp (argument, "on") == 0)
    {
      gsc_map_set (TRUE);
    }
  else if (scr_strcasecmp (argument, "off") == 0)
    {
      gsc_map_set (FALSE);
    }
  else if (scr_strcasecmp (argument, "top") == 0
           || scr_strcasecmp (argument, "above") == 0)
    {
      gsc_map_place (TRUE);
    }
  else if (scr_strcasecmp (argument, "right") == 0
           || scr_strcasecmp (argument, "side") == 0)
    {
      gsc_map_place (FALSE);
    }
  else if (scr_strncasecmp (argument, "zoom", 4) == 0
           && (argument[4] == '\0' || argument[4] == ' '
               || argument[4] == '\t'))
    {
      /* "glk map zoom ..." is "glk zoom ..." by another name. */
      gsc_command_zoom (argument + 4 + strspn (argument + 4, "\t "));
    }
  else
    gsc_command_usage ("map");
}

/*
 * gsc_command_zoom()
 *
 * "glk zoom [in | out | auto | N]".  Plain "glk zoom" zooms in, and "default"
 * is a synonym for "auto".  A number pins the scale to that many pixels per
 * map unit, which must lie between MAP_ZOOM_MIN and MAP_ZOOM_MAX.  A manual
 * zoom is kept until "auto" puts the map back to fitting itself to its
 * window; meanwhile a map too big for the window follows the player around
 * (map_frame).
 */
void
gsc_command_zoom (const char *argument)
{
  char buf[96];
  char *end;
  long pinned;
  int numeric, dir = 0;

  if (gsc_is_a5 ? gsc_a5_run == nullptr : gsc_game == nullptr)
    return;

  /* "glk map zoom help" arrives here directly; the dispatcher catches the
     plain "glk zoom help" before the handler is ever called. */
  if (scr_strcasecmp (argument, "help") == 0)
    {
      gsc_command_help ("zoom");
      return;
    }

  if (scr_strcasecmp (argument, "auto") == 0
      || scr_strcasecmp (argument, "default") == 0)
    {
      if (gsc_map_zoom == 0)
        gsc_normal_string ("The map is already zooming to fit its window.\n");
      else
        {
          gsc_map_zoom = 0;
          gsc_map_redraw ();
          gsc_normal_string ("Map zoom returned to automatic.\n");
        }
      return;
    }

  pinned = strtol (argument, &end, 10);
  numeric = end != argument && *end == '\0';
  if (!numeric)
    {
      if (strlen (argument) == 0 || scr_strcasecmp (argument, "in") == 0)
        dir = 1;
      else if (scr_strcasecmp (argument, "out") == 0)
        dir = -1;
      else
        {
          gsc_command_usage ("zoom");
          return;
        }
    }

  /* The rest zoom relative to, or are limited by, the map as drawn. */
  if (!gsc_map_shown)
    {
      gsc_normal_string ("The map is not open.  Use ");
      gsc_standout_string ("glk map on");
      gsc_normal_string (" to open it first.\n");
      return;
    }

  if (!numeric)
    {
      if (!gsc_map_zoom_apply (dir))
        gsc_normal_string (dir > 0
                           ? "The map is already at its maximum zoom.\n"
                           : "The map is already at its minimum zoom.\n");
    }
  else if (pinned < MAP_ZOOM_MIN || pinned > MAP_ZOOM_MAX)
    {
      snprintf (buf, sizeof buf, "Map zoom must be between %d and %d.\n",
                MAP_ZOOM_MIN, MAP_ZOOM_MAX);
      gsc_normal_string (buf);
    }
  else if (pinned == gsc_map_zoom)
    {
      snprintf (buf, sizeof buf, "The map is already at zoom %ld.\n", pinned);
      gsc_normal_string (buf);
    }
  else
    {
      gsc_map_zoom = (int) pinned;
      gsc_map_redraw ();
    }
}
