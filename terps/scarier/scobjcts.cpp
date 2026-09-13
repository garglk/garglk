/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Module notes:
 *
 * o ...
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const scr_char NUL = '\0';

/* Trace flag, set before running. */
static scr_bool obj_trace = FALSE;


/*
 * obj_get_flag()
 *
 * Return the given boolean property of an object.
 */
static scr_bool
obj_get_flag (scr_gameref_t game, scr_int object, const scr_char *name)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  return prop_get_indexed_boolean (bundle, "Objects", object, name);
}


/*
 * obj_is_static()
 * obj_is_surface()
 * obj_is_container()
 *
 * Convenience functions to return TRUE for given object attributes.
 */
scr_bool
obj_is_static (scr_gameref_t game, scr_int object)
{
  return obj_get_flag (game, object, "Static");
}

scr_bool
obj_is_container (scr_gameref_t game, scr_int object)
{
  return obj_get_flag (game, object, "Container");
}

scr_bool
obj_is_surface (scr_gameref_t game, scr_int object)
{
  return obj_get_flag (game, object, "Surface");
}


/*
 * obj_nth_object()
 * obj_object_index()
 *
 * Adrift numbers objects of a given kind separately from objects at large,
 * so most kinds need a pair of functions to convert between the two: the
 * n'th object matching some property, and the count of matching objects that
 * precede a given one.  These walk the objects for any such property.
 *
 * They are inverses, in that obj_nth_object (obj_object_index (o)) == o for
 * any matching object o.
 */
typedef scr_bool (*obj_matcherref_t) (scr_gameref_t game, scr_int object);

static scr_int
obj_nth_object (scr_gameref_t game, obj_matcherref_t matches, scr_int n)
{
  scr_int object, count;

  /* Progress through objects until n matches found. */
  count = n;
  for (object = 0; object < gs_object_count (game) && count >= 0; object++)
    {
      if (matches (game, object))
        count--;
    }
  return object - 1;
}

static scr_int
obj_object_index (scr_gameref_t game, obj_matcherref_t matches, scr_int objnum)
{
  scr_int object, count;

  /* Progress through objects up to objnum. */
  count = 0;
  for (object = 0; object < objnum; object++)
    {
      if (matches (game, object))
        count++;
    }
  return count;
}


/*
 * obj_container_object()
 * obj_container_index()
 *
 * Convert between object and container numbering.
 */
scr_int
obj_container_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, obj_is_container, n);
}

scr_int
obj_container_index (scr_gameref_t game, scr_int objnum)
{
  return obj_object_index (game, obj_is_container, objnum);
}


/*
 * obj_surface_object()
 * obj_surface_index()
 *
 * Convert between object and surface numbering.
 */
scr_int
obj_surface_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, obj_is_surface, n);
}

scr_int
obj_surface_index (scr_gameref_t game, scr_int objnum)
{
  return obj_object_index (game, obj_is_surface, objnum);
}


/*
 * obj_is_stateful()
 * obj_stateful_object()
 * obj_stateful_index()
 *
 * Convert between object and stateful object numbering.  An object is
 * stateful if it is openable, or if it carries a set of states.
 */
static scr_bool
obj_is_stateful (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool is_openable, is_statussed;

  is_openable = prop_get_indexed_integer (bundle, "Objects", object,
                                          "Openable") != 0;
  is_statussed = prop_get_indexed_integer (bundle, "Objects", object,
                                           "CurrentState") != 0;
  return is_openable || is_statussed;
}

scr_int
obj_stateful_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, obj_is_stateful, n);
}

scr_int
obj_stateful_index (scr_gameref_t game, scr_int objnum)
{
  return obj_object_index (game, obj_is_stateful, objnum);
}


/*
 * obj_state_name()
 *
 * Return the string name of the state of a given stateful object.  The
 * string is malloc'ed, and needs to be freed by the caller.  Returns NULL
 * if no valid state string found.
 */
scr_char *
obj_state_name (scr_gameref_t game, scr_int objnum)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *states;
  scr_int length, state, count, first, last;
  scr_char *string;

  /* Get the list of state strings for the object. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = objnum;
  vt_key[2].string = "States";
  states = prop_get_string (bundle, "S<-sis", vt_key);

  /* Find the start of the element for the current state. */
  state = gs_object_state (game, objnum);
  length = strlen (states);
  for (first = 0, count = state; first < length && count > 1; first++)
    {
      if (states[first] == '|')
        count--;
    }
  if (count != 1)
    return NULL;

  /* Find the end of the state string. */
  for (last = first; last < length; last++)
    {
      if (states[last] == '|')
        break;
    }

  /* Allocate and take a copy of the state string. */
  string = (decltype(string)) scr_malloc (last - first + 1);
  memcpy (string, states + first, last - first);
  string[last - first] = NUL;

  return string;
}


/*
 * obj_is_dynamic()
 * obj_dynamic_object()
 *
 * Return the index of the n'th non-static object found.
 */
static scr_bool
obj_is_dynamic (scr_gameref_t game, scr_int object)
{
  return !obj_is_static (game, object);
}

scr_int
obj_dynamic_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, obj_is_dynamic, n);
}


/*
 * obj_is_wearable()
 * obj_wearable_object()
 *
 * Return the index of the n'th wearable object found.
 */
static scr_bool
obj_is_wearable (scr_gameref_t game, scr_int object)
{
  return !obj_is_static (game, object)
         && obj_get_flag (game, object, "Wearable");
}

scr_int
obj_wearable_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, obj_is_wearable, n);
}


/*
 * Size is held in the ten's digit of SizeWeight, and weight in the units.
 * Size and weight are indices into a geometric scale -- the relative size and
 * weight of objects rises by a constant factor for each incremental index.
 * The same scale gives a container's capacity, which is a volume rather than
 * an object count: 'tens' of Capacity multiplied by the scale factor raised to
 * the 'units'.
 *
 * The two scale factors are per-game, stored in the pair of globals that this
 * parser used to discard as iUnk1/iUnk2.  The ADRIFT editor always writes 3
 * for both, which is why a hardwired 3 went unnoticed for so long, but the
 * Runner really does read them: a game built with a size base of 2 and a
 * weight base of 5 and MaxSize = MaxWt = 102 reports its player limits as
 * 40 and 250 in the Runner's own debugger, not 90 and 90.  Files too old to
 * carry the fields (v3.8 and earlier) get the editor's 3.
 */
enum
{ OBJ_DIMENSION_DIVISOR = 10,
  OBJ_DIMENSION_MULTIPLE = 3
};

/*
 * obj_get_size_multiple()
 * obj_get_weight_multiple()
 *
 * Return the game's size and weight scale factors.
 */
static scr_int
obj_get_dimension_multiple (scr_gameref_t game, const scr_char *name)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2], vt_rvalue;

  vt_key[0].string = "Globals";
  vt_key[1].string = name;
  if (!prop_get (bundle, "I<-ss", &vt_rvalue, vt_key))
    return OBJ_DIMENSION_MULTIPLE;

  return vt_rvalue.integer;
}

static scr_int
obj_get_size_multiple (scr_gameref_t game)
{
  return obj_get_dimension_multiple (game, "SizeMultiple");
}

static scr_int
obj_get_weight_multiple (scr_gameref_t game)
{
  return obj_get_dimension_multiple (game, "WeightMultiple");
}

/*
 * obj_scale()
 *
 * Return multiple raised to the power index_.  A zero multiple gives 1 for
 * index zero and 0 for everything above it; that is not a special case worth
 * defending against, since it is precisely what the Runner computes.
 */
static scr_int
obj_scale (scr_int multiple, scr_int index_)
{
  scr_int retval = 1;

  for (; index_ > 0; index_--)
    retval *= multiple;

  return retval;
}

/*
 * obj_get_size()
 * obj_get_weight()
 *
 * Return the relative size and weight of an object.  For containers, the
 * weight includes the weight of each contained object.
 *
 * Static objects have no SizeWeight, and an event can put one in the player's
 * inventory (evt_move_object is the only mover that will), so both functions
 * have to answer for one.  Zero is right: run400 never counts an event-placed
 * object towards the player's limits at all, static or not -- measured live,
 * RUNNER_TESTS_TODO.md section 9 -- so a static, which can arrive no other
 * way, contributes nothing to the carried totals in either engine.
 */
scr_int
obj_get_size (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int size, count;

  /* Static objects have no size; see the note above. */
  if (obj_is_static (game, object))
    return 0;

  /* Size is the 'tens' component of SizeWeight. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "SizeWeight";
  count = prop_get_integer (bundle, "I<-sis", vt_key) / OBJ_DIMENSION_DIVISOR;

  /*
   * Calculate base object size.  Unlike weights below, we take this as simply
   * being the maximum size; that is, when a container carries other objects
   * its weight increases by the sum of objects carried, but its size remains
   * constant.
   */
  size = obj_scale (obj_get_size_multiple (game), count);

  if (obj_trace)
    scr_trace ("Object: object %ld is size %ld\n", object, size);

  /* Return total size. */
  return size;
}

scr_int
obj_get_base_weight (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int count;

  /* Static objects have no weight; see the note above obj_get_size. */
  if (obj_is_static (game, object))
    return 0;

  /* Weight is the 'units' component of SizeWeight. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "SizeWeight";
  count = prop_get_integer (bundle, "I<-sis", vt_key) % OBJ_DIMENSION_DIVISOR;

  /* Calculate base object weight. */
  return obj_scale (obj_get_weight_multiple (game), count);
}

/*
 * Recursion depth cap for the weigh loop below.  The Runner's only cycle
 * defence is its i <> self guard, so an authored parent cycle would hang it;
 * we bottom out instead of hanging with it.  32 comfortably exceeds any real
 * containment nesting.
 */
enum { OBJ_WEIGH_DEPTH_LIMIT = 32 };

static scr_int
obj_weigh (scr_gameref_t game, scr_int object, scr_int depth)
{
  scr_int weight, other;

  /* Base object weight; zero for statics. */
  weight = obj_get_base_weight (game, object);

  if (depth >= OBJ_WEIGH_DEPTH_LIMIT)
    return weight;

  /*
   * Add the recursive weights of "child" objects.  run400's weigh routine
   * (Sub_22_63 @447600) matches children on the bare per-object container
   * field ([2E]) with only an i <> self guard -- it checks neither the
   * child's position nor whether the weighed object is a container or
   * surface, so objects whose [2E] is stale (leftover raw Parent data that
   * ordinary takes, drops, and task/event moves never touch) silently add
   * phantom weight; see the runner_parent notes in scgamest.h.  We match on the
   * runner_parent shadow of that field, but only for the version that keeps
   * a running carried total to corrupt (obj_uses_running_load, i.e. 4.0).
   * A 3.7 or 3.8 game never calls this on the take path at all, and those
   * versions' loaders rewrote the raw Parent values our shadow is seeded
   * from; a 3.9 game recomputes its totals, so a stale parent has nothing to
   * accumulate into.  All three keep the position-filtered membership.
   *
   * `glk capacity on` (game->capacity_recompute) keeps the same older
   * membership: that switch exists to hand a player a load model free of the
   * Runner's accounting bugs, and phantom weight is one of them -- with the
   * Runner rule left in place here, the escape hatch cured the running total's
   * size leaks but still weighed a stale parent's children.  Goldilocks is the
   * case that shows it: `drop package` refunds 29 where the package and its
   * leaflet weigh 10, the extra 19 being worn objects whose leftover raw
   * Parent still points at the package.
   */
  for (other = 0; other < gs_object_count (game); other++)
    {
      scr_bool is_child;

      if (other == object)
        continue;

      if (!obj_uses_running_load (game) || game->capacity_recompute)
        is_child = (gs_object_position (game, other) == OBJ_IN_OBJECT
                    || gs_object_position (game, other) == OBJ_ON_OBJECT)
                   && gs_object_parent (game, other) == object;
      else
        is_child = gs_object_runner_parent (game, other) == object;

      if (is_child)
        weight += obj_weigh (game, other, depth + 1);
    }

  return weight;
}

scr_int
obj_get_weight (scr_gameref_t game, scr_int object)
{
  const scr_int weight = obj_weigh (game, object, 0);

  if (obj_trace)
    scr_trace ("Object: object %ld is weight %ld\n", object, weight);

  return weight;
}


/*
 * obj_convert_player_limit()
 * obj_get_player_size_limit()
 * obj_get_player_weight_limit()
 *
 * Return the limits set on the sizes and weights a player can handle.  Not
 * really object-related except that they deal with sizing multiples.
 */
static scr_int
obj_convert_player_limit (scr_int value, scr_int multiple)
{
  /* 'Tens' of value multiplied by the scale factor to the power 'units'. */
  return (value / OBJ_DIMENSION_DIVISOR)
         * obj_scale (multiple, value % OBJ_DIMENSION_DIVISOR);
}

scr_int
obj_get_player_size_limit (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int max_size;

  max_size = prop_get_global_integer (bundle, "MaxSize");

  return obj_convert_player_limit (max_size, obj_get_size_multiple (game));
}

scr_int
obj_get_player_weight_limit (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int max_weight;

  max_weight = prop_get_global_integer (bundle, "MaxWt");

  return obj_convert_player_limit (max_weight, obj_get_weight_multiple (game));
}


/*
 * The version 3.8 carrying model, which is not the version 4.0 one at all.
 *
 * A 3.8 object has a single "Size/weight" class, 0..4, and the player has a
 * single "MaxCarried".  Measured in the genuine run380.exe (2026-08-03, with
 * probe files patched through the plaintext of a 3.80 .taf -- see
 * ~/adrift-battle/runner/wine/README.md), those are one *pooled burden*: each
 * class costs 1, 3, 7, 3 or 7, the costs of everything held are summed, and
 * the sum may not exceed MaxCarried.  Pinned at MaxCarried 1, 2, 3, 6, 7 and
 * 8: at 2 a class-1 or class-3 object is refused and a class-0 accepted, at 3
 * both go; at 6 Marooned's tires (class 4) are refused, at 7 they are accepted
 * alone, at 8 tires + map fit and tires + flint + map do not.  Tires (7) and a
 * class-2 gas can (7) never coexist at any limit, which is what rules out a
 * separate size axis and weight axis: two axes would have let the two heavy
 * objects sit one on each.
 *
 * The refusal is 3.8's only one -- there is no "too heavy" message.  Crime
 * Adventure's kettle (class 2 = 7 against MaxCarried 5) is refused with "Your
 * hands are full." by the real Runner with empty hands, so the pooled check is
 * the one that speaks and lib_object_too_heavy() stands down (see sclibrar.c).
 *
 * Objects that never carried a class -- static objects, and objects a 4.0 or
 * 3.9 game supplies -- weigh nothing here; obj_uses_burden_model() keeps the
 * whole model off for anything but a 3.8 game.
 */
static const scr_int V380_BURDEN_COST[] = { 1, 3, 7, 3, 7 };

scr_bool
obj_uses_burden_model (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2], vt_rvalue;

  vt_key[0].string = "Globals";
  vt_key[1].string = "BurdenModel";
  if (!prop_get (bundle, "I<-ss", &vt_rvalue, vt_key))
    return FALSE;

  return vt_rvalue.integer != 0;
}


/*
 * obj_uses_running_load()
 *
 * TRUE if the Runner keeps a *running* carried size and weight, updated as
 * objects move, rather than recomputing the totals from what is held.  Only
 * 4.0 does.
 *
 * run400 keeps both totals in the player record and edits them from one
 * central move routine (Proc_21_54 @4528D8), which is where its two leaks
 * live: dropping a carried container refunds only the container's own size,
 * and wearing then dropping something subtracts its size twice.  run390 has
 * no such central routine -- it adjusts the same two globals from each
 * command handler in turn -- and its arithmetic comes out exact, so a 3.9
 * game is indistinguishable from recomputing the totals afresh.
 *
 * Measured on the p39leak probe, one Runner each, same commands (2026-08-23).
 * Size after take bag + take rock, put rock in bag, drop bag, take cape,
 * wear cape, drop cape:
 *
 *   run390   36   9   0   9   9   0
 *   run400   36  36  27  36  27  18
 *
 * Weight tracks 0/6/6/0/9/9/0 in run390 and 0/6/6/0/9/9/0 in run400 up to the
 * drop, both refunding a dropped container recursively.  The 3.7/3.8 pooled
 * burden is recomputed too (see lib_carried_burden), so 4.0 is alone here.
 */
scr_bool
obj_uses_running_load (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400;
}


/*
 * obj_get_burden()
 *
 * Return what the given object costs against the player's 3.8 carry limit.
 * Unlike weight, a container is not charged for what it holds, measured in
 * run380 2026-08-03: against a MaxCarried of 1, a class-0 box (cost 1) holding
 * a class-4 object (cost 7) is picked up without complaint, and only the next
 * cost-1 object is refused.  Charging the contents would have made the box
 * alone cost 8 and refused everything after it.
 */
scr_int
obj_get_burden (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3], vt_rvalue;
  scr_int class_, burden;

  if (obj_is_static (game, object))
    return 0;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "SizeWeightClass";
  if (!prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
    return 0;

  class_ = vt_rvalue.integer;
  burden = (class_ >= 0 && class_ < (scr_int) (sizeof (V380_BURDEN_COST)
                                               / sizeof (V380_BURDEN_COST[0])))
           ? V380_BURDEN_COST[class_] : V380_BURDEN_COST[0];

  if (obj_trace)
    scr_trace ("Object: object %ld is class %ld, burden %ld\n",
               object, class_, burden);

  return burden;
}

/*
 * obj_get_player_burden_limit()
 *
 * Return the total burden the player can carry: version 3.8's MaxCarried,
 * used raw.
 */
scr_int
obj_get_player_burden_limit (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  return prop_get_global_integer (bundle, "MaxCarried");
}


/*
 * obj_get_container_capacity()
 * obj_get_container_free_space()
 *
 * Return the total space inside a container, and the space still unused.
 *
 * Capacity packs an object count in its 'tens' and a size index in its
 * 'units', but the Runner multiplies the two out into a single volume and
 * then spends that volume on whatever is put in, however many objects that
 * turns out to be.  Probed against run400 with a Capacity of 12 (1 x 3^2 = 9)
 * and twelve size-1 objects: nine go in and the tenth is refused, where an
 * object count of 1 would have refused the second.  A Capacity of 52
 * (5 x 3^2 = 45) swallows all twelve, then a size-9 object on top, and only
 * then refuses a size-27 one -- so the 'units' digit is no per-object ceiling
 * either.  The one hard per-object rule is that nothing larger than the whole
 * container ever fits, which is the difference between the Runner's two
 * refusals: "is too big to fit inside" against the total, "can't fit inside
 * at the moment" against what is left.
 *
 * Free space counts the direct contents only.  A container inside a container
 * spends its own size and not a bit more, whatever it happens to be holding --
 * unlike weight, which the Runner sums recursively.
 *
 * Version 3.8's Capacity is a plain object count instead, which is why the
 * V380 fixups normalise every object to a "size" of 22 and store Capacity as
 * count*10+2: one object then costs exactly one count.  Measured in run380
 * 2026-08-03 with a Capacity of 2, which swallows a class-4 object and then a
 * class-1 one, and is filled by two class-0 ones -- so the Size/weight class
 * is not charged against the container any more than against its carrier.  A
 * Capacity of 0 is full from the start rather than unlimited.  3.8 has only
 * the one refusal for all of this, "The box is full." (see sclibrar.c).
 */
scr_int
obj_get_container_capacity (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int capacity, packed;

  packed = prop_get_indexed_integer (bundle, "Objects", object, "Capacity");

  capacity = (packed / OBJ_DIMENSION_DIVISOR)
             * obj_scale (obj_get_size_multiple (game),
                          packed % OBJ_DIMENSION_DIVISOR);

  if (obj_trace)
    scr_trace ("Object: object %ld has capacity %ld\n", object, capacity);

  return capacity;
}

scr_int
obj_get_container_free_space (scr_gameref_t game, scr_int object)
{
  scr_int free_space, other;

  free_space = obj_get_container_capacity (game, object);

  for (other = 0; other < gs_object_count (game); other++)
    {
      if (gs_object_position (game, other) == OBJ_IN_OBJECT
          && gs_object_parent (game, other) == object)
        free_space -= obj_get_size (game, other);
    }

  if (obj_trace)
    scr_trace ("Object: object %ld has %ld free\n", object, free_space);

  return free_space;
}


/* Sit/lie bit mask enumerations. */
enum
{ OBJ_STANDABLE_MASK = 1 << 0,
  OBJ_LIEABLE_MASK = 1 << 1
};

/*
 * obj_has_sit_lie()
 * obj_is_standable()
 * obj_is_lieable()
 * obj_standable_object()
 * obj_lieable_object()
 *
 * Return the index of the n'th standable or lieable object found.  Both
 * flags live in the one SitLie property.
 */
static scr_bool
obj_has_sit_lie (scr_gameref_t game, scr_int object, scr_int mask)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  return (prop_get_indexed_integer (bundle, "Objects", object,
                                    "SitLie") & mask) != 0;
}

static scr_bool
obj_is_standable (scr_gameref_t game, scr_int object)
{
  return obj_has_sit_lie (game, object, OBJ_STANDABLE_MASK);
}

static scr_bool
obj_is_lieable (scr_gameref_t game, scr_int object)
{
  return obj_has_sit_lie (game, object, OBJ_LIEABLE_MASK);
}

scr_int
obj_standable_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, obj_is_standable, n);
}

scr_int
obj_lieable_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, obj_is_lieable, n);
}


/*
 * obj_appears_plural()
 *
 * Return TRUE if the object appears to be plural, i.e. if the Runner's
 * isare() helper would answer " are " rather than " is " for it.
 *
 * All four Runners carry the same helper -- run370 @423E5C, run380 @428EAC,
 * run390 @431038, run400 @4507BC (Proc_19_69) -- and it is this:
 *
 *     r = " is "
 *     If Left(prefix, 4) = "some" And Right(name, 1) = "s" Then r = " are "
 *     If Right(name, 1) = "s" Then
 *       If Mid(name, Len(name) - 1, 1) <> "u" Then r = " are "
 *     End If
 *     If prefix = "a"  Or Left(prefix, 2) = "a "  Then r = " is "
 *     If prefix = "an" Or Left(prefix, 3) = "an " Then r = " is "
 *
 * Three things in that differ from the guess inherited SCARE made:
 *
 *  - it is a chain of overwrites, not a nest, so the "some" clause reaches a
 *    name that the -us exception then does NOT spare.  "some walrus" is
 *    plural to every Runner, and so are pestilence's "some zeus" and "some
 *    apparatus" and Sophie's "some fungus".
 *  - only "a" and "an" force the singular.  An empty prefix does not need to
 *    be tested for, because by the time isare() sees it the loader has
 *    already turned it into a literal "a" (see parse_trim_object_names in
 *    sctafpar.cpp); an authored whitespace-only prefix, which the loader
 *    leaves genuinely empty, really is plural-capable.
 *  - every comparison is VB6 Option Compare Binary, i.e. case-SENSITIVE, the
 *    same fact probe PFX measured for the article normalizer.  A prefix
 *    written "A" is not an "a" prefix, and a name ending in a capital "S" is
 *    not a name ending in "s".
 *
 * Measured cell by cell on probe ISARE (run400, Adrift_isare.txt, 2026-09-07),
 * twelve objects one per spelling, read back through `where <name>`:
 *
 *     where boots   (Prefix "")       The boots is test arena.
 *     where cactus  (Prefix "")       The cactus is test arena.
 *     where gloves  (Prefix "some")   The gloves are test arena.
 *     where walrus  (Prefix "some")   The walrus are test arena.
 *     where beads   (Prefix "a")      The beads is test arena.
 *     where eggs    (Prefix "an")     The eggs is test arena.
 *     where shoes   (Prefix "A")      A shoes are test arena.
 *     where keys    (Prefix "the")    The keys are test arena.
 *     where NAILS   (Prefix "the")    The NAILS is test arena.
 *     where pins    (Prefix "a big")  The big pins is test arena.
 *     where socks   (Prefix "Some")   Some socks are test arena.
 *     where rings   (Prefix " ")       rings are test arena.
 *
 * Twelve objects across six corpus games move: The X-Files: A New Beginning's
 * "A Pair Of Dockers", "A Pair Of Blue Jeans", "A Pair Of Nikes", "A Bowl Of
 * Peanuts" and "A Set Of Directions" (capital article), pestilence's "some
 * zeus" and "some apparatus", sa's and sophie's "some fungus", xycanthus's "A
 * pile of debris", and yeh's "A bag of apples" and "A Bow of Icy Arrows".
 * All twelve go from " is " to " are ".
 */
scr_bool
obj_appears_plural (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *prefix, *name;
  scr_int length;
  scr_bool is_plural;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  length = strlen (name);

  is_plural = FALSE;

  if (length > 0 && name[length - 1] == 's')
    {
      if (strncmp (prefix, "some", 4) == 0)
        is_plural = TRUE;

      /*
       * VB6 Mid(name, Len(name) - 1, 1) is a runtime error for a name of one
       * character, so a Short of exactly "s" faults the Runner rather than
       * answering; nothing in the corpus has one, and not being plural is the
       * closest thing to an answer available here.
       */
      if (length > 1 && name[length - 2] != 'u')
        is_plural = TRUE;
    }

  if (strcmp (prefix, "a") == 0 || strncmp (prefix, "a ", 2) == 0)
    is_plural = FALSE;
  if (strcmp (prefix, "an") == 0 || strncmp (prefix, "an ", 3) == 0)
    is_plural = FALSE;

  return is_plural;
}


/*
 * obj_static_in_room()
 *
 * Return TRUE if a given static object is currently in a given room.  Static
 * objects have no position of their own; instead they carry a list of the
 * rooms they appear in, unless an event has moved them.
 *
 * Two of the cases only count where the caller is willing to look through a
 * holder: an object an event has moved into the player's hands, and one that
 * is part of an NPC.  `is_indirect` says whether it is.
 */
static scr_bool
obj_static_in_room (scr_gameref_t game, scr_int object, scr_int room,
                    scr_bool is_indirect)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int type;

  /* Static object moved to player or room by event? */
  if (!gs_object_static_unmoved (game, object))
    {
      if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
        return is_indirect ? gs_player_in_room (game, room) : FALSE;
      else
        return gs_object_position (game, object) - 1 == room;
    }

  /* Check and return the room list for the object. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Where";
  vt_key[3].string = "Type";
  type = prop_get_integer (bundle, "I<-siss", vt_key);
  switch (type)
    {
    case ROOMLIST_ALL_ROOMS:
      return TRUE;
    case ROOMLIST_NO_ROOMS:
      return FALSE;

    case ROOMLIST_ONE_ROOM:
      vt_key[3].string = "Room";
      return prop_get_integer (bundle, "I<-siss", vt_key) == room + 1;

    case ROOMLIST_SOME_ROOMS:
      vt_key[3].string = "Rooms";
      vt_key[4].integer = room + 1;
      return prop_get_boolean (bundle, "B<-sissi", vt_key);

    case ROOMLIST_NPC_PART:
      {
        scr_int npc;

        if (!is_indirect)
          return FALSE;

        vt_key[2].string = "Parent";
        npc = prop_get_integer (bundle, "I<-sis", vt_key);
        if (npc == 0)
          return gs_player_in_room (game, room);
        else
          return npc_in_room (game, npc - 1, room);
      }

    default:
      scr_fatal ("obj_static_in_room: invalid type, %ld\n", type);
      return FALSE;
    }
}


/*
 * obj_directly_in_room_internal()
 * obj_directly_in_room()
 *
 * Return TRUE if a given object is currently on the floor of a given room.
 */
static scr_bool
obj_directly_in_room_internal (scr_gameref_t game, scr_int object, scr_int room)
{
  /* See if the object is static or dynamic. */
  if (obj_is_static (game, object))
    return obj_static_in_room (game, object, room, FALSE);
  else
    return gs_object_position (game, object) == room + 1;
}

scr_bool
obj_directly_in_room (scr_gameref_t game, scr_int object, scr_int room)
{
  scr_bool result;

  /* Check, trace result, and return. */
  result = obj_directly_in_room_internal (game, object, room);

  if (obj_trace)
    {
      scr_trace ("Object: checking for object %ld directly in room %ld, %s\n",
                object, room, result ? "true" : "false");
    }

  return result;
}


/*
 * obj_indirectly_in_room_internal()
 * obj_indirectly_in_room()
 *
 * Return TRUE if a given object is currently in a given room, either
 * directly, on an object indirectly, in an open object indirectly, or
 * carried by an NPC in the room.
 */
static scr_bool
obj_indirectly_in_room_internal (scr_gameref_t game, scr_int object, scr_int room)
{
  /* See if the object is static or dynamic. */
  if (obj_is_static (game, object))
    return obj_static_in_room (game, object, room, TRUE);
  else
    {
      scr_int parent, position;

      /* Get dynamic object's parent and position. */
      parent = gs_object_parent (game, object);
      position = gs_object_position (game, object);

      /* Decide depending on positioning. */
      switch (position)
        {
        case OBJ_HIDDEN:       /* Hidden. */
          return FALSE;

        case OBJ_HELD_PLAYER:  /* Held by player. */
        case OBJ_WORN_PLAYER:  /* Worn by player. */
          return gs_player_in_room (game, room);

        case OBJ_HELD_NPC:     /* Held by NPC. */
        case OBJ_WORN_NPC:     /* Worn by NPC. */
          return npc_in_room (game, parent, room);

        case OBJ_IN_OBJECT:    /* In another object. */
          {
            scr_int openness;

            openness = gs_object_openness (game, parent);
            switch (openness)
              {
              case OBJ_WONTCLOSE:
              case OBJ_OPEN:
                return obj_indirectly_in_room (game, parent, room);
              default:
                return FALSE;
              }
          }

        case OBJ_ON_OBJECT:    /* On another object. */
          return obj_indirectly_in_room (game, parent, room);

        default:               /* Within a room. */
          if (position > gs_room_count (game) + 1)
            {
              scr_error ("scr_object_indirectly_in_room:"
                        " position out of bounds, %ld\n", position);
            }
          return position - 1 == room;
        }
    }
}

scr_bool
obj_indirectly_in_room (scr_gameref_t game,
                        scr_int object, scr_int room)
{
  scr_bool result;

  /* Check, trace result, and return. */
  result = obj_indirectly_in_room_internal (game, object, room);

  if (obj_trace)
    {
      scr_trace ("Object: checking for object %ld indirectly in room %ld, %s\n",
                object, room, result ? "true" : "false");
    }

  return result;
}


/*
 * obj_indirectly_held_by_player_internal()
 * obj_indirectly_held_by_player()
 *
 * Return TRUE if a given object is currently held by the player, either
 * directly, on an object indirectly, or in an open object indirectly.
 */
static scr_bool
obj_indirectly_held_by_player_internal (scr_gameref_t game,
                                        scr_int object)
{
  /* See if the object is static or dynamic. */
  if (obj_is_static (game, object))
    {
      /* Static object moved to player or room by event? */
      if (!gs_object_static_unmoved (game, object))
        {
          if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
            return TRUE;
          else
            return FALSE;
        }

      /* An unmoved static object is not held by the player. */
      return FALSE;
    }
  else
    {
      scr_int parent, position;

      /* Get dynamic object's parent and position. */
      parent = gs_object_parent (game, object);
      position = gs_object_position (game, object);

      /* Decide depending on positioning. */
      switch (position)
        {
        case OBJ_HIDDEN:       /* Hidden. */
          return FALSE;

        case OBJ_HELD_PLAYER:  /* Held by player. */
        case OBJ_WORN_PLAYER:  /* Worn by player. */
          return TRUE;

        case OBJ_HELD_NPC:     /* Held by NPC. */
        case OBJ_WORN_NPC:     /* Worn by NPC. */
          return FALSE;

        case OBJ_IN_OBJECT:    /* In another object. */
          {
            scr_int openness;

            openness = gs_object_openness (game, parent);
            switch (openness)
              {
              case OBJ_WONTCLOSE:
              case OBJ_OPEN:
                return obj_indirectly_held_by_player (game, parent);
              default:
                return FALSE;
              }
          }

        case OBJ_ON_OBJECT:    /* On another object. */
          return obj_indirectly_held_by_player (game, parent);

        default:               /* Within a room. */
          return FALSE;
        }
    }
}

scr_bool
obj_indirectly_held_by_player (scr_gameref_t game, scr_int object)
{
  scr_bool result;

  /* Check, trace result, and return. */
  result = obj_indirectly_held_by_player_internal (game, object);

  if (obj_trace)
    {
      scr_trace ("Object: checking for object %ld indirectly"
                " held by player, %s\n", object, result ? "true" : "false");
    }

  return result;
}


/*
 * obj_initial_location_code()
 *
 * The Runner's o(26) exactly as its loader computes it (run400
 * @00490255-@004902BD): the authored InitialPosition less one, with "in a
 * container" and "on a surface" folded onto &HF6 and &HEC and everything
 * above them losing a further two, so that a room ends up one-based.  Only
 * obj_shows_initial_description() needs it, and there only the room and
 * "held by the player" codes can ever match anything.
 */
static scr_int
obj_initial_location_code (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int initialposition;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "InitialPosition";
  initialposition = prop_get_integer (bundle, "I<-sis", vt_key);

  switch (initialposition)
    {
    case 0:                     /* Hidden. */
      return -1;
    case 1:                     /* Held. */
      return 0;
    case 2:                     /* In a container, run400's &HF6. */
      return -10;
    case 3:                     /* On a surface, run400's &HEC. */
      return -20;
    default:                    /* In a room, one-based. */
      return initialposition - 3;
    }
}


/*
 * obj_shows_initial_description()
 *
 * Return TRUE if the room lister prints this object's in-room description
 * INSTEAD of adding the object to the "Also here is" list.
 *
 * This is run400's Proc_19_75_449B6C @00449B6C, and both halves of viewroom
 * @00472CA4 hang off it: the printing loop @00472515 takes the objects it
 * says yes to, and the two listing loops @004725D0 and @004726A9 take
 * exactly the ones it says no to.  Its shape is not "has the object been
 * moved" at all.  The loader freezes the OnlyWhenNotMoved byte as it reads
 * it (run400 @00490B96, `If o(132) = 2 Then o(132) = o(26) + 1`), so the
 * three authored modes survive as three *values*, compared literally:
 *
 *   0  matches only on the branch where the InRoomDesc is non-empty
 *   1  matches unconditionally -- until the library take handler spends it,
 *      which is the ONLY write back to the byte in the whole of run400
 *      (`takes` @0047BF66 turns a 1 into -1, and -1 never matches again)
 *   2  has become the object's initial location code + 1, so it matches
 *      only while the object still sits where it started
 *
 * so mode 1 is "until the player first picks it up" -- a drop, a put, a task
 * moving the object, an NPC taking it all leave it showing -- and mode 0
 * with an EMPTY InRoomDesc means "print nothing AND stay out of the list",
 * which is how a 4.0 author silences an object the room's own long text
 * already mentions.  Measured 2026-09-06 under Wine: camelot15's four
 * bottles (mode 1, empty InRoomDesc) and takeone's jewel (mode 2, empty
 * InRoomDesc, still in its initial room) are both invisible to run400's
 * lister where Scarier used to list them, while zelda's shield (mode 1, a
 * description, dropped back on the ground) is listed rather than described,
 * because the take spent its byte.
 *
 * Pre-4.0 games carry neither property, and their defaults -- empty
 * InRoomDesc, mode 0 -- land on "not shown, therefore listed", which is what
 * they did before, so this needs no version gate.
 */
scr_bool
obj_shows_initial_description (scr_gameref_t game, scr_int object,
                               scr_int room, scr_bool inroomdesc_absent)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int onlywhennotmoved, frozen;

  /* Get the only-when-not-moved property, and freeze it as the loader does. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "OnlyWhenNotMoved";
  onlywhennotmoved = prop_get_integer (bundle, "I<-sis", vt_key);

  if (onlywhennotmoved == 1)
    frozen = gs_object_unmoved (game, object) ? 1 : -1;
  else if (onlywhennotmoved == 2)
    frozen = obj_initial_location_code (game, object) + 1;
  else
    frozen = onlywhennotmoved;

  /*
   * Compare against the Runner's one-based location codes: its lister is
   * handed room + 1, and tests the frozen byte against that plus one.
   */
  if (frozen == 1 || frozen == room + 2)
    return TRUE;

  return frozen == 0 && !inroomdesc_absent;
}


/*
 * obj_mark_room_objects_seen()
 *
 * Mark seen the objects a room description reveals: static objects present
 * in the room (including parts of a present NPC), and dynamic objects lying
 * directly in it.  Objects inside or on top of other objects, and NPC
 * possessions, are NOT seen until a contents listing reveals them
 * (examine/open/look in the parent, or examining the NPC), so don't recurse
 * through holders here.
 *
 * This runs from the room lister, not once a turn, because that is where
 * every Runner does it.  The seen byte is object field 40 in 3.8 and field
 * 44 in 3.9/4.0, and a census of its writers finds exactly one site in the
 * room path: run380 viewroom sets it at 00439735 for a static whose room
 * array covers the player room and at 00439792 for a dynamic whose location
 * is the room being listed, and run390 viewroom does the same pair at
 * 00447B9C and 00447BFC.  Both sit *below* the lister's brief-mode exit
 * (run390 @00447B10, "If verbose = 1 Then Exit Sub"), so a room the Runner
 * did not describe in full marks nothing.  The Runner's other writers are
 * all reveal sites Scarier already has: openadv (held or worn at load),
 * examines, charinv, inventory, drops, whatisinon, insides, afteroa,
 * checkevent.
 *
 * Measured in run400's Adrift_22_xfiles.txt (The X-Files, 4.00): task 7 "Use Key"
 * carries ShowRoomDesc = 0, so entering Garage 5 through it prints no room
 * description -- and `take knife` there answers "Take what?", although the
 * Small Pocket Knife (object 31, InitialPosition 11 = room 7) is lying
 * loose on the floor.  The very next command, `out`, moves normally, so the
 * player really is in the room; the knife simply does not exist to the
 * parser until something lists it.  Scarier used to mark it seen every turn
 * regardless and took it.
 *
 * Both tests use the room being listed.  run400's viewroom reads the
 * static's presence array at its own room argument (@00472639), while
 * run390's reads it at the player-room global (unk_4082E6.global_0) and
 * tests only dynamics against the argument; the two differ solely when a
 * task displays a room the player is not in, which is rare enough that the
 * 4.0 reading is the one worth carrying.
 */
void
obj_mark_room_objects_seen (scr_gameref_t game, scr_int room)
{
  scr_int index_;

  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      scr_bool is_visible;

      if (gs_object_seen (game, index_))
        continue;

      if (obj_is_static (game, index_))
        is_visible = obj_static_in_room (game, index_, room, TRUE);
      else
        is_visible = gs_object_position (game, index_) == room + 1;

      if (is_visible)
        gs_set_object_seen (game, index_, TRUE);
    }
}



/*
 * obj_mark_room_statics_seen()
 *
 * Mark seen every static object present in a room.  A task action that moves
 * the player runs this over the destination: run400's executor sweeps the
 * whole object table after each of its three player-room destinations ("to
 * room" @0048CA32, "to roomgroup part" @0048CADD, "to same room as"
 * @0048CB48) and sets the seen byte on every object whose static flag is set
 * and whose presence array covers the new player room.  Dynamic objects are
 * not touched -- each sweep tests global_24 = 1 first -- so an object lying
 * loose in the destination stays unknown until something lists it.
 *
 * The sweep matters because it is the only reveal on that path: a task that
 * moves the player without ShowRoomDesc prints no room description, so the
 * room lister never runs.  Measured against Professor.taf, whose "move
 * contraption" task carries the player to Glen's Lookout and prints the room
 * text from its own ALR-expanded CompleteText -- `examine button` there
 * resolves the green button (object 86, a static of that room) although no
 * lister ever mentioned it.
 */
void
obj_mark_room_statics_seen (scr_gameref_t game, scr_int room)
{
  scr_int index_;

  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (gs_object_seen (game, index_) || !obj_is_static (game, index_))
        continue;

      if (obj_static_in_room (game, index_, room, TRUE))
        gs_set_object_seen (game, index_, TRUE);
    }
}

/*
 * obj_debug_trace()
 *
 * Set object tracing on/off.
 */
void
obj_debug_trace (scr_bool flag)
{
  obj_trace = flag;
}
