/*  automap.c: main automapping code
    Copyright (C) 1999  Evin Robertson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

    The author can be reached at nitfol@deja.com
*/

#include "nitfol.h"

#ifdef DEBUGGING

struct dirinfo {
  const char *name;
  int deltax, deltay;
  char symbol;
  char oneway;
};

static const struct dirinfo dirways[] = {
  { "n",   0, -1, '|',  '^' },
  { "s",   0,  1, '|',  'v' },
  { "w",  -1,  0, '-',  '<' },
  { "e",   1,  0, '-',  '>' },
  { "nw", -1, -1, '\\', '^' },
  { "se",  1,  1, '\\', 'v' },
  { "ne",  1, -1, '/',  '^' },
  { "sw", -1,  1, '/',  'v' },
  { "up",  0,  0, 0,    0   },
  { "down", 0, 0, 0,    0   },
  { "wait", 0, 0, 0,    0   }
};

#define NUM_EXITS (sizeof(dirways) / sizeof(*dirways))
#define REVERSE_DIR(dir) (dir ^ 1)

#define NUM_DIRS  (NUM_EXITS - 3)
#define NUM_WALK  (NUM_EXITS - 1)

#define DIR_UP   (NUM_DIRS + 0)
#define DIR_DOWN (NUM_DIRS + 1)
#define DIR_WAIT (NUM_DIRS + 2)

char *roomsymbol = NULL;

#define ROOM_SYMBOL(is_player, is_up, is_down, is_real) (is_real ? roomsymbol[(is_up != 0) | ((is_down != 0) << 1) | ((is_player != 0) << 2)] : roomsymbol[8])


typedef struct edge edge;
typedef struct loc_node loc_node;

struct edge {
  loc_node *dest[2];                    /* Two endpoints of passage */
  BOOL is_oneway; /* Oneway passages are always created dest[0]--->dest[1] */
  BOOL touched;
  int min_length;
  int guess_length;
};

struct loc_node {
  zword number;
  BOOL found, real, touched;
  edge *outgoing[NUM_DIRS];             /* Drawn map connections */
  loc_node *exits[NUM_EXITS];           /* Actual connections */
  glui32 dist;                          /* For automap_find_path */
};


typedef struct edgelist edgelist;
struct edgelist {
  edgelist *next;
  edge *node;
};

static edgelist *all_edges;

static edge *automap_new_edge(loc_node *src, loc_node *dest, BOOL is_oneway)
{
  edgelist newedge;
  newedge.node = n_malloc(sizeof(edge));
  newedge.node->dest[0] = src;
  newedge.node->dest[1] = dest;
  newedge.node->is_oneway = is_oneway;
  newedge.node->touched = FALSE;
  newedge.node->min_length = is_oneway ? 4 : 2;
  newedge.node->guess_length = is_oneway ? 4 : 2;  
  LEadd(all_edges, newedge);
  return newedge.node;
}


static void automap_remove_edge(edge *e)
{
  unsigned n, i;
  edgelist *p, *t;
  if(e == NULL)
    return;
  for(n = 0; n < 2; n++) {
    loc_node *thisdest = e->dest[n];
    if(thisdest)
      for(i = 0; i < NUM_DIRS; i++) {
	if(thisdest->outgoing[i] == e)
	  thisdest->outgoing[i] = NULL;
      }
  }
  LEsearchremove(all_edges, p, t, p->node == e, n_free(p->node));
}


static void automap_edges_untouch(void)
{
  edgelist *p;
  for(p = all_edges; p; p=p->next) {
    p->node->touched = FALSE;
  }
}


static void automap_edges_mindist(void)
{
  edgelist *p;
  for(p = all_edges; p; p=p->next) {
    int len = p->node->is_oneway ? 4 : 2;
    p->node->min_length = p->node->guess_length = len;
  }
}


static hash_table rooms;
static char *loc_exp;
static zwinid automap_win;


void automap_kill(void)
{
  mymap_kill();
  n_free(loc_exp);
  loc_exp = NULL;
  LEdestruct(all_edges, n_free(all_edges->node));
  n_hash_free_table(&rooms, n_free);
  z_kill_window(automap_win);
}


BOOL automap_init(int numobj, const char *location_exp)
{
  automap_kill();

  if(!roomsymbol)
    roomsymbol = n_strdup("*udb@UDB+");
  
  if(location_exp)
    loc_exp = n_strdup(location_exp);

  n_hash_construct_table(&rooms, numobj / 2);

  automap_win = z_split_screen(wintype_TextGrid,
			       automap_split | winmethod_Fixed,
			       automap_draw_callback, automap_mouse_callback);
  return TRUE;
}


static loc_node *room_find(glui32 location, BOOL is_real)
{
  const char *preface = is_real ? "" : "fake";
  const char *key = n_static_number(preface, location);
  return (loc_node *) n_hash_lookup(key, &rooms);  
}


static loc_node *room_find_or_create(glui32 location, BOOL is_real)
{
  loc_node *r;
  const char *preface = is_real ? "" : "fake";
  const char *key = n_static_number(preface, location);
  r = (loc_node *) n_hash_lookup(key, &rooms);
  if(r == NULL) {
    unsigned n;
    r = (loc_node *) n_malloc(sizeof(loc_node));
    r->number = location;
    r->found = FALSE;
    r->real = is_real;
    r->touched = FALSE;
    for(n = 0; n < NUM_EXITS; n++) {
      r->exits[n] = NULL;
      if(n < NUM_DIRS)
	r->outgoing[n] = NULL;
    }
    n_hash_insert(key, r, &rooms);
  }
  return r;
}


static void room_remove(loc_node *room)
{
  unsigned n;
  if(room) {
    const char *preface = room->real ? "" : "fake";
    for(n = 0; n < NUM_DIRS; n++)
      automap_remove_edge(room->outgoing[n]);
    n_free(n_hash_del(n_static_number(preface, room->number), &rooms));
  }
}



typedef struct automap_path automap_path;
struct automap_path {
  automap_path *next;
  loc_node *loc; /* A location */
  int dir;       /* And the direction we're going from it */
};

typedef struct interlist interlist;
struct interlist {
  interlist *next;

  loc_node *a, *b;
};


static BOOL mymap_plot(int x, int y, char symbol, loc_node *node);
static edge *automap_get_edge(loc_node *location, int dir);
static void automap_calc_location(loc_node *location, loc_node *last,
				  int x, int y);
static automap_path *automap_find_path(loc_node *location, loc_node *dest,
				       BOOL by_walking);
static int automap_edge_oneway(loc_node *location, int dir);
static loc_node *automap_edge_follow(loc_node *location, int dir);


static interlist *interferences = NULL;

static void automap_forget_interference(void)
{
  LEdestroy(interferences);
}

static void automap_remember_interference(loc_node *a, loc_node *b)
{
  /*  interlist *p;
  LEsearch(interferences, p, (p->a==a && p->b==b) || (p->a==b && p->b==a));
  if(!p) {*/
    interlist newnode;
    newnode.a = a;
    newnode.b = b;
    LEadd(interferences, newnode);
    /*  }*/
}


static int automap_find_and_count_interference(loc_node *center)
{
  interlist *i;
  int count;
  
  automap_cycles_fill_values();
  automap_forget_interference();
  mymap_reinit();
  n_hash_enumerate(&rooms, make_untouched);
  automap_edges_untouch();
  automap_calc_location(center, NULL, 0, 0);
  
  count = 0;
  for(i = interferences; i; i=i->next)
    count++;
  
  return count;
}


/* Returns TRUE if it improved any */
static BOOL automap_increase_along_path(automap_path *path, int oldcount,
					loc_node *center, int effort)
{
  automap_path *p;
  int exploring;
  int explore_max = effort > 1;
  if(!effort)
    return FALSE;

  /* Takes two passes at trying to improve the situation.
     The first time (!exploring), it tries increasing the length of each
     edge along the path, observing the results and then undoing the increase.
     If it was able to improve anything, it returns with the best improvement.
     Otherwise it tries increasing the length of each edge and calling itself;
     If its child is able to improve things, then it returns with both
     lengthenings in effect. */
  
  for(exploring = 0; exploring <= explore_max; exploring++) {
    edge *best_edge = NULL;
    int best_count = oldcount;
    int smallest_new = 10000;
    
    for(p = path; p; p=p->next) {
      int newcount;
      edge *e = automap_get_edge(p->loc, p->dir);
      int old_min_length = e->min_length;
      int old_guess_length = e->guess_length;

      if(p->next && p->next->loc != automap_edge_follow(p->loc, p->dir))
	  n_show_error(E_SYSTEM, "path doesn't follow itself", 0);

      e->guess_length += 2;
      e->min_length = e->guess_length;
      
      if(!exploring) {
	newcount = automap_find_and_count_interference(center);
	if(newcount < best_count
	   || (newcount == best_count && newcount < oldcount
	       && e->min_length < smallest_new)) {
	  best_edge = e;
	  best_count = newcount;
	  smallest_new = e->min_length;
	}
      } else {
	if(automap_increase_along_path(p, oldcount, center, effort-1))
	  return TRUE;
      }
    
      e->min_length   = old_min_length;
      e->guess_length = old_guess_length;
    }

    if(!exploring && best_edge) {
      best_edge->guess_length += 2;
      best_edge->min_length = best_edge->guess_length;
      automap_find_and_count_interference(center);
      return TRUE;
    }
  }
  return FALSE;
}


/* Returns true if all interferences have been resolved */
static BOOL automap_resolve_interference(loc_node *center, int effort)
{
  int skip_interferences = 0;
  int n;
  
  while(interferences) {
    interlist *oldinter = interferences;
    interlist *i;
    automap_path *path;
    int oldcount;

    oldcount = 0;
    for(i = oldinter; i; i=i->next)
      oldcount++;

    if(skip_interferences >= oldcount)
      return FALSE;
    
    i = oldinter;
    for(n = 0; n < skip_interferences; n++)
      i=i->next;
    
    path = automap_find_path(i->a, i->b, FALSE);
    if(!path)
      return FALSE;

    interferences = NULL;
    
    if(!automap_increase_along_path(path, oldcount, center, effort))
      skip_interferences++;

    LEdestroy(oldinter);
    LEdestroy(path);
  }
  return TRUE;
}


static void automap_set_virtual_connection(loc_node *location, int d,
					   loc_node *dest, BOOL is_oneway)
{
  if(location->outgoing[d]) {
    int way = automap_edge_oneway(location, d);
    if(dest || way != 2)
      automap_remove_edge(location->outgoing[d]);
  }

  if(dest) {
    edge *p = automap_new_edge(location, dest, is_oneway);

    location->outgoing[d] = p;

    if(dest->outgoing[REVERSE_DIR(d)])
      automap_remove_edge(dest->outgoing[REVERSE_DIR(d)]);
    dest->outgoing[REVERSE_DIR(d)] = p;
  }
}


static void automap_set_connection(int location, int d, int dest, BOOL is_real)
{
  loc_node *r, *t;

  r = room_find_or_create(location, is_real);
  t = room_find_or_create(dest, is_real);

  if(t == r)
    t = NULL;
  
  r->exits[d] = t;
}


static edge *automap_get_edge(loc_node *location, int dir)
{
  return location->outgoing[dir];
}


static loc_node *automap_edge_follow(loc_node *location, int dir)
{
  if(location->outgoing[dir] == NULL)
    return NULL;
  
  if(location->outgoing[dir]->dest[0] == location)
    return location->outgoing[dir]->dest[1];
  if(location->outgoing[dir]->dest[1] == location)
    return location->outgoing[dir]->dest[0];
  
  n_show_error(E_SYSTEM, "edge isn't connected to what it should be", 0);
  return NULL;
}


static int automap_edge_length(loc_node *location, int dir)
{
  return location->outgoing[dir]->guess_length;
}


/* Returns 0 if not oneway, 1 if oneway in the given direction, and 2 if
   oneway in the other direction */
static int automap_edge_oneway(loc_node *location, int dir)
{
  if(location->outgoing[dir] == NULL)
    return 0;
  if(location->outgoing[dir]->dest[0] == location)
    return location->outgoing[dir]->is_oneway;
  return (location->outgoing[dir]->is_oneway) << 1;
}


static BOOL automap_draw_edge(loc_node *location, int dir, int *x, int *y)
{
  int deltax, deltay;
  int s;
  int len;
  int oneway;
  edge *e = automap_get_edge(location, dir);
  
  if(e->touched)
    return TRUE;
  e->touched = TRUE;

  deltax = dirways[dir].deltax;
  deltay = dirways[dir].deltay;
  len = automap_edge_length(location, dir);
  oneway = automap_edge_oneway(location, dir);
  
  *x += deltax;
  *y += deltay;

  if(oneway)
    len--;
  
  if(oneway == 2) {
    mymap_plot(*x, *y, dirways[REVERSE_DIR(dir)].oneway, location);
    *x += deltax;
    *y += deltay;
  }
    
  for(s = 1; s < len; s++) {
    mymap_plot(*x, *y, dirways[dir].symbol, location);
    *x += deltax;
    *y += deltay;
  }

  if(oneway == 1) {
    mymap_plot(*x, *y, dirways[dir].oneway, location);
    *x += deltax;
    *y += deltay;
  }
  return TRUE;
}


static void automap_adjust_length(loc_node *location, int dir, int newlen)
{
  location->outgoing[dir]->min_length = newlen;
}


static int mapwidth;
static int mapheight;

static char *mymap = NULL;
static loc_node **mymapnode = NULL;

static char mymap_read(int x, int y)
{
  x += mapwidth / 2; y += mapheight / 2;
  if(x < 0 || x >= mapwidth || y < 0 || y >= mapheight)
    return ' ';
  return mymap[x + y * mapheight];
}


static BOOL mymap_plot(int x, int y, char symbol, loc_node *node)
{
  BOOL status = TRUE;
  char *dest;
  x += mapwidth / 2; y += mapheight / 2;
  if(x < 0 || x >= mapwidth || y < 0 || y >= mapheight)
    return status;
  dest = &mymap[x + y * mapwidth];
  if(*dest != ' ') {
    if((*dest=='/' && symbol=='\\') || (*dest=='\\' && symbol=='/'))
      symbol = 'X';
    else if((*dest=='-' && symbol=='|') || (*dest=='|' && symbol=='-'))
      symbol = '+';
    else
      status = FALSE;
  } else {
    if(mymapnode[x + y * mapwidth])
      status = FALSE;
  }
  if(status) {
    *dest = symbol;
    mymapnode[x + y * mapwidth] = node;
  } else {
    loc_node *interfere = mymapnode[x + y * mapwidth];
    automap_remember_interference(node, interfere);
  }
  return status;
}


void mymap_init(int width, int height)
{
  int i;
  int max;
  mapwidth = width * 2;
  mapheight = height * 2;
  max = mapwidth * mapheight;
  n_free(mymap);
  n_free(mymapnode);
  mymap = (char *) n_malloc(max);
  mymapnode = (loc_node **) n_malloc(max * sizeof(*mymapnode));
  for(i = 0; i < max; i++) {
    mymap[i] = ' ';
    mymapnode[i] = NULL;
  }
}


int automap_get_height(void)
{
  return mapheight / 2;
}


void mymap_reinit(void)
{
  mymap_init(mapwidth/2, mapheight/2);
}


void mymap_kill(void)
{
  n_free(mymap);
  mymap = NULL;    
  n_free(mymapnode);
  mymapnode = NULL;
}


static int xoffset, yoffset;

static void mymap_draw(void)
{
  int x, y;
  int firsty, firstx, lasty, lastx;
  int height, width;

  firsty = mapheight; firstx = mapwidth;
  lasty = 0; lastx = 0;
  for(y = 0; y < mapheight; y++) {
    for(x = 0; x < mapwidth; x++)
      if(mymap[x + y * mapwidth] != ' ') {
	if(y < firsty)
	  firsty = y;
	if(y > lasty)
	  lasty = y;
	if(x < firstx)
	  firstx = x;
	if(x > lastx)
	  lastx = x;
      }
  }

  height = lasty - firsty; width = lastx - firstx;
  
  xoffset = firstx + (width - mapwidth/2) / 2;
  yoffset = firsty + (height - mapheight/2) / 2;

  if(yoffset >= mapheight/2)
    yoffset = mapheight/2 - 1;
  if(yoffset <= 1)
    yoffset = 2;
  if(xoffset >= mapwidth/2)
    xoffset = mapwidth/2 - 1;
  if(xoffset <= 1)
    xoffset = 2;
  
  for(y = 0; y < mapheight/2; y++) {
    for(x = 0; x < mapwidth/2; x++)
      glk_put_char(mymap[x+xoffset + (y+yoffset) * mapwidth]);
  }
}

static glui32 selected_room_number = 0;

static void automap_write_loc(int x, int y)
{
  loc_node *room;
  selected_room_number = 0;
  x += xoffset; y += yoffset;
  if(x < 0 || x >= mapwidth || y < 0 || y >= mapheight)
    return;
  room = mymapnode[x + y * mapwidth];
  if(!room || !room->found || !room->real)
    return;
  selected_room_number = room->number;
}


glui32 automap_draw_callback(winid_t win, glui32 width, glui32 height)
{
  if(win == NULL)
    return automap_size;

  mymap_init(width, height);
  automap_set_locations(automap_location);
  
  glk_stream_set_current(glk_window_get_stream(win));
  mymap_draw();

  if(selected_room_number) {
    offset short_name_off = object_name(selected_room_number);
    glk_window_move_cursor(win, 0, 0);

    if(short_name_off)
      decodezscii(short_name_off, w_glk_put_char);
    else
      w_glk_put_string("<nameless>");
    w_glk_put_string(" (");
    g_print_number(selected_room_number);
    glk_put_char(')');
  }
  return automap_size;
}


BOOL automap_mouse_callback(BOOL is_char_event,
			    winid_t win, glui32 x, glui32 y)
{
  automap_write_loc(x, y);
  return FALSE;
}


static void automap_calc_location(loc_node *location, loc_node *last,
				  int x, int y)
{
  unsigned i;
  char symbol;
  loc_node *is_up, *is_down;

  if(!location)
    return;

  if(location->touched)
    return;
  location->touched = TRUE;

  /* Make sure unfound locations are blanked */
  if(!location->found) {
    mymap_plot(x, y, ' ', location);
    return;
  }


  /* Don't draw up/down exits if there's a normal passage leading that way */
  is_up = location->exits[DIR_UP];
  is_down = location->exits[DIR_DOWN];
  for(i = 0; i < NUM_DIRS; i++) {
    loc_node *thisdest = automap_edge_follow(location, i);
    if(thisdest && !thisdest->real)
      thisdest = location->exits[i];
    if(thisdest == is_up)
      is_up = 0;
    if(thisdest == is_down)
      is_down = 0;
  }

  symbol = ROOM_SYMBOL((x==0 && y==0), is_up, is_down, location->real);

  mymap_plot(x, y, symbol, location);

  for(i = 0; i < NUM_DIRS; i++) {
    loc_node *thisdest = automap_edge_follow(location, i);
    if(thisdest && thisdest != last) {
      int destx = x;
      int desty = y;
      automap_draw_edge(location, i, &destx, &desty);
      automap_calc_location(thisdest, location, destx, desty);
    }
  }
}


/* Returns magic cookies to identify fake locations */
static glui32 automap_get_cookie(void) {
  /* FIXME: When the glui32 wraps around Bad Things will happen if we return a
     cookie still in use.  Should reissue cookies to everyone when we wrap
     around. */
  static glui32 cookie = 0;
  return cookie++;
}


static void automap_calc_exits(loc_node *location, int depth)
{
  unsigned i, n;
  loc_node *proposed[NUM_DIRS];         /* Store proposed edges here */
  BOOL is_oneway[NUM_DIRS];
  
  /* Remove fake locations */
  for(i = 0; i < NUM_DIRS; i++) {
    loc_node *curdest = automap_edge_follow(location, i);
    if(curdest && !curdest->real)
      room_remove(curdest);
  }

  /* Default to things going the way they actually do */
  for(i = 0; i < NUM_DIRS; i++) {
    proposed[i] = location->exits[i];
    is_oneway[i] = FALSE;
  }
  
  /* Get rid of superfluous exits */
  for(i = 0; i < NUM_DIRS; i++) {
    if(proposed[i]) {
      for(n = i+1; n < NUM_DIRS; n++) {
	if(proposed[n] == proposed[i]) {
	  if(proposed[i]->exits[REVERSE_DIR(n)] == location) {
	    proposed[i] = NULL;
	    break;
	  }
	  if(proposed[i]->exits[REVERSE_DIR(i)] == location)
	    proposed[n] = NULL;
	}
      }
    }
  }

  /* Handle forced movement */
  for(i = 0; i < NUM_DIRS; i++) {
    if(proposed[i] && proposed[i] == location->exits[DIR_WAIT]) {
      if(proposed[i]->exits[REVERSE_DIR(i)] != location)
	proposed[i] = NULL;
    }
  }
  
  /* Check for one way and bent passages */
  for(i = 0; i < NUM_DIRS; i++) {
    if(proposed[i] && proposed[i]->found
       && proposed[i]->exits[REVERSE_DIR(i)] != location) {
      is_oneway[i] = TRUE;
      for(n = 0; n < NUM_DIRS; n++) {
	if(n != i && proposed[i]->exits[n] == location) {
	  loc_node *newnode = room_find_or_create(automap_get_cookie(), FALSE);
	    
	  is_oneway[i] = FALSE;
	  newnode->found = TRUE;
	    
	  automap_set_virtual_connection(proposed[i], n, newnode, FALSE);
	  proposed[i] = newnode;
	}
      }
    }
  }

  /* If it's a one way passage, but there are up/down exits connecting the two,
     ignore the passage */
  for(i = 0; i < NUM_DIRS; i++) {
    if(is_oneway[i] && proposed[i]
	 && ((location->exits[DIR_UP] == proposed[i]
	      && proposed[i]->exits[DIR_DOWN] == location)
	     || (location->exits[DIR_DOWN] == proposed[i]
		 && proposed[i]->exits[DIR_UP] == location))) {
      proposed[i] = 0;
      is_oneway[i] = FALSE;
    }
  }

  /* Create the proposed passages */
  for(i = 0; i < NUM_DIRS; i++)
    automap_set_virtual_connection(location, i, proposed[i], is_oneway[i]);
  
  /* Explore neighbors */
  if(depth) {
    for(i = 0; i < NUM_DIRS; i++)
      automap_calc_exits(location->exits[i], depth-1);
  }
}


#define INFINITY 1000000L

static void make_distant(const char *unused_key, void *r)
{
  loc_node *t = (loc_node *) r;
  t->dist = INFINITY;
}


static void automap_calc_distances(loc_node *location, glui32 distance,
				   BOOL by_walking)
{
  unsigned i;
  unsigned maxdir = by_walking ? NUM_EXITS : NUM_DIRS;
  if(location->dist < distance)
    return;
  location->dist = distance;
  for(i = 0; i < maxdir; i++) {
    loc_node *thisdest;
    if(by_walking)
      thisdest = location->exits[i];
    else
      thisdest = automap_edge_follow(location, i);

    if(thisdest)
      automap_calc_distances(thisdest, distance+1, by_walking);
  }
}


static automap_path *automap_find_path(loc_node *location, loc_node *dest,
				       BOOL by_walking)
{
  automap_path *path = NULL;
  automap_path *rev;
  automap_path newnode;
  loc_node *p;

  /* Find the distances of all nodes from dest */
  n_hash_enumerate(&rooms, make_distant);
  automap_calc_distances(dest, 0, by_walking);

  /* If dest isn't reachable, location's distance will still be infinite */
  if(location->dist == INFINITY)
    return NULL;

  /* At each step, go toward a nearer node 'till we're there */
  p = location;
  while(p != dest) {
    unsigned i;
    unsigned best_dir;
    glui32 best_dist = INFINITY;
    loc_node *best_node = NULL;
    unsigned maxdir = by_walking ? NUM_EXITS : NUM_DIRS;
    for(i = 0; i < maxdir; i++) {
      loc_node *thisdest;
      if(by_walking)
	thisdest = p->exits[i];
      else
	thisdest = automap_edge_follow(p, i);
      
      if(thisdest && thisdest->dist < best_dist) {
	best_dir = i;
	best_dist = thisdest->dist;
	best_node = thisdest;
      }
    }
    if(!best_node) {
      n_show_error(E_SYSTEM, "couldn't find path there", 0);
      return NULL;
    }
    newnode.loc = p;
    newnode.dir = best_dir;
    LEadd(path, newnode);
    p = best_node;
  }

  rev = NULL;
  while(path) {
    LEadd(rev, *path);
    LEremove(path);
  }

  return rev;
}


static void automap_find_cycles(loc_node *location, automap_path *curpath)
{
  unsigned i;
  location->touched = TRUE;
  for(i = 0; i < NUM_DIRS; i++) {
    loc_node *thisdest = automap_edge_follow(location, i);
    if(thisdest && thisdest->found) {
      automap_path newnode;
      newnode.dir = i;
      newnode.loc = location;
      LEadd(curpath, newnode);

      if(thisdest->touched) {           /* Found a cycle! */
	int cyclelength = 0;
	automap_path *p;
	cycleequation *cycle = NULL;
	cycleequation newcycle;
	for(p = curpath; p; p=p->next) {
	  int dir = p->dir;
	  newcycle.var = &(p->loc->outgoing[dir]->guess_length);
	  newcycle.min = &(p->loc->outgoing[dir]->min_length);
	  newcycle.xcoefficient = dirways[dir].deltax;
	  newcycle.ycoefficient = dirways[dir].deltay;
	  LEadd(cycle, newcycle);
	  
	  cyclelength++;
	  if(p->loc == thisdest) {      /* Found the relevant endpoint */
	    if(cyclelength <= 2)     /* Ignore two nodes going to each other */
	      LEdestroy(cycle);
	    else
	      automap_add_cycle(cycle); /* automap_add_cycle gets ownership */
	    break;
	  }
	}
	if(!p) {                        /* The cycle had already been found */
	  LEdestroy(cycle);
	}
      } else {
	automap_find_cycles(thisdest, curpath);
      }
      LEremove(curpath);
    }
  }
}


static void automap_calc_cycles(loc_node *start)
{
  automap_delete_cycles();
  automap_find_cycles(start, NULL);
  automap_cycle_elimination();
  automap_cycles_fill_values();
}


void make_untouched(const char *unused_key, void *r)
{
  loc_node *t = (loc_node *) r;
  t->touched = FALSE;
}


void automap_set_locations(int center)
{
  loc_node *r;

  r = room_find(center, TRUE);

  n_hash_enumerate(&rooms, make_untouched);
  automap_edges_mindist();
  automap_calc_cycles(r);

  n_hash_enumerate(&rooms, make_untouched);
  automap_forget_interference();
  mymap_reinit();
  automap_edges_untouch();
  automap_calc_location(r, NULL, 0, 0);

  automap_resolve_interference(r, 2);
}


static int automap_dir = NUM_EXITS;
static BOOL automap_explored = FALSE;
zword automap_location = 0;


/* Returns a direction it wants you to explore in.  Take the direction and
   call automap_unexplore, which'll take you back to the @read.
   If it returns NULL, we're finished; get player input */
const char *automap_explore(void)
{
  if(automap_explored) {
    n_show_error(E_SAVE, "tried to explore when we just did so", automap_explored);
    return NULL;
  }

  if(!loc_exp)
    return NULL;

  if(automap_dir == NUM_EXITS) {
    fast_saveundo();
    automap_location = evaluate_expression(loc_exp, stack_get_depth()).v;    
    automap_dir = 0;
  } else {
    automap_dir++;
    if(automap_dir == NUM_EXITS) {
      loc_node *r = room_find(automap_location, TRUE);
      r->found = TRUE;
      automap_calc_exits(r, 0);
      allow_saveundo = TRUE;
      allow_output = TRUE;
      return NULL;
    }    
  }

  allow_saveundo = FALSE;
  allow_output = FALSE;
  automap_explored = TRUE;

  return dirways[automap_dir].name;
}


/* Undoes the work of automap_explore - call whenever a turn is 'over' */
BOOL automap_unexplore(void)
{
  zword dest;

  if(!automap_explored || !loc_exp)
    return FALSE;
  automap_explored = FALSE;
  
  dest = evaluate_expression(loc_exp, stack_get_depth()).v;
  
  automap_set_connection(automap_location, automap_dir, dest, TRUE);
  
  fast_restoreundo();
  return TRUE;
}

#else

char *roomsymbol = NULL;

BOOL automap_unexplore(void)
{
  return FALSE;
}

#endif
