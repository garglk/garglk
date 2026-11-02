/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "sctafpar.h"
#include "scgamest_api.h"
#include "scnpcs.h"
#include "scobjcts.h"
#include "sclocale.h"
#include "scdump.h"
#include "scgamest.h"


/* Trace flag, set before running. */
static scr_bool obj_trace = FALSE;


/*
 * Per-object attribute cache.
 *
 * Whether an object is static, a container, a surface, and so on is fixed by
 * the bundle and never changes in play, yet the library and the restriction
 * checks ask for it object by object, many times a turn -- and finding the
 * n'th container, surface or stateful object walked every object.  Each answer
 * was a property tree lookup, and together they were up to a fifth of a
 * walkthrough's run time.  So read every object's attributes once, the first
 * time any is asked for, along with the per-kind numberings.  The cache is
 * keyed by bundle, which the undo and temporary game copies share; the runner
 * calls obj_forget_bundle() before destroying a bundle, so a later one
 * allocated at the same address can't inherit it.
 *
 * Every object version carries Static, Container, Surface, Openable, SitLie
 * and CurrentState, and Wearable exactly when it is not static (see the
 * object grammars in sctafpar.cpp), so the reads below are the ones the old
 * object-by-object lookups made.
 *
 * A static object's Where room list and Parent are kept too: every "is it
 * here" question about a static that no event has moved is answered from
 * them, for every object, several times a turn.
 */
enum
{
  OBJ_KIND_STATIC,
  OBJ_KIND_DYNAMIC,
  OBJ_KIND_CONTAINER,
  OBJ_KIND_SURFACE,
  OBJ_KIND_STATEFUL,
  OBJ_KIND_WEARABLE,
  OBJ_KIND_STANDABLE,
  OBJ_KIND_LIEABLE,
  OBJ_KIND_COUNT
};

/* The two flags held in an object's SitLie property. */
enum
{ OBJ_STANDABLE_MASK = 1 << 0,
  OBJ_LIEABLE_MASK = 1 << 1
};

static scr_prop_setref_t obj_cache_bundle = nullptr;
static std::vector<scr_byte> obj_kinds;       /* per object, 1 << OBJ_KIND_* */
static std::vector<scr_int> obj_numbering[OBJ_KIND_COUNT];


/*
 * A static object's home.  `type` is its Where Type, or OBJ_WHERE_UNCACHED
 * for a dynamic object and for a list that lacks the property its type calls
 * for, which the callers then read from the bundle as they always did.
 * `detail` is Where Room for ROOMLIST_ONE_ROOM and Parent for
 * ROOMLIST_NPC_PART; `rooms` is Where Rooms for ROOMLIST_SOME_ROOMS.
 */
enum { OBJ_WHERE_UNCACHED = -1 };
struct obj_where_t
{
  scr_int type;
  scr_int detail;
  std::vector<scr_byte> rooms;
};
static std::vector<obj_where_t> obj_wheres;


/*
 * obj_forget_bundle()
 *
 * Drop the cache if it was built from the given bundle.
 */
void
obj_forget_bundle (scr_prop_setref_t bundle)
{
  if (obj_cache_bundle == bundle)
    {
      scr_int kind;

      obj_cache_bundle = nullptr;
      obj_kinds.clear ();
      for (kind = 0; kind < OBJ_KIND_COUNT; kind++)
        obj_numbering[kind].clear ();
      obj_wheres.clear ();
    }
}


/*
 * obj_cache_where()
 *
 * Read a static object's Where room list, and its Parent if it is part of a
 * character, into `where`.
 */
static void
obj_cache_where (scr_prop_setref_t bundle, scr_int object, obj_where_t *where)
{
  scr_vartype_t vt_key[5], vt_rvalue;
  scr_int count, index_;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Where";
  vt_key[3].string = "Type";
  if (!prop_get (bundle, "I<-siss", &vt_rvalue, vt_key))
    return;

  switch (vt_rvalue.integer)
    {
    case ROOMLIST_ONE_ROOM:
      vt_key[3].string = "Room";
      if (!prop_get (bundle, "I<-siss", &vt_rvalue, vt_key))
        return;
      where->detail = vt_rvalue.integer;
      where->type = ROOMLIST_ONE_ROOM;
      break;

    case ROOMLIST_SOME_ROOMS:
      vt_key[3].string = "Rooms";
      count = prop_get_child_count (bundle, "I<-siss", vt_key);
      where->rooms.assign (count, FALSE);
      for (index_ = 0; index_ < count; index_++)
        {
          vt_key[4].integer = index_;
          where->rooms[index_] = prop_get_boolean (bundle, "B<-sissi", vt_key);
        }
      where->type = ROOMLIST_SOME_ROOMS;
      break;

    case ROOMLIST_NPC_PART:
      vt_key[2].string = "Parent";
      if (!prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
        return;
      where->detail = vt_rvalue.integer;
      where->type = ROOMLIST_NPC_PART;
      break;

    default:
      where->type = vt_rvalue.integer;
      break;
    }
}


/*
 * obj_cache_kinds()
 *
 * Make sure the cache holds the kinds of the game's objects.  An object is
 * stateful if it is openable, or if it carries a set of states.
 */
static void
obj_cache_kinds (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object;

  if (obj_cache_bundle == bundle)
    return;

  obj_forget_bundle (obj_cache_bundle);
  obj_cache_bundle = bundle;
  obj_kinds.assign (gs_object_count (game), 0);
  obj_wheres.assign (gs_object_count (game), obj_where_t ());
  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_bool is_static =
          prop_get_indexed_boolean (bundle, "Objects", object, "Static");
      const scr_int sit_lie =
          prop_get_indexed_integer (bundle, "Objects", object, "SitLie");
      scr_bool is_kind[OBJ_KIND_COUNT];
      scr_int kind;

      is_kind[OBJ_KIND_STATIC] = is_static;
      is_kind[OBJ_KIND_DYNAMIC] = !is_static;
      is_kind[OBJ_KIND_CONTAINER] =
          prop_get_indexed_boolean (bundle, "Objects", object, "Container");
      is_kind[OBJ_KIND_SURFACE] =
          prop_get_indexed_boolean (bundle, "Objects", object, "Surface");
      is_kind[OBJ_KIND_STATEFUL] =
          prop_get_indexed_integer (bundle, "Objects", object, "Openable") != 0
          || prop_get_indexed_integer (bundle, "Objects", object,
                                       "CurrentState") != 0;
      is_kind[OBJ_KIND_WEARABLE] =
          !is_static
          && prop_get_indexed_boolean (bundle, "Objects", object, "Wearable");
      is_kind[OBJ_KIND_STANDABLE] = (sit_lie & OBJ_STANDABLE_MASK) != 0;
      is_kind[OBJ_KIND_LIEABLE] = (sit_lie & OBJ_LIEABLE_MASK) != 0;

      for (kind = 0; kind < OBJ_KIND_COUNT; kind++)
        {
          if (is_kind[kind])
            {
              obj_kinds[object] |= 1 << kind;
              obj_numbering[kind].push_back (object);
            }
        }

      obj_wheres[object].type = OBJ_WHERE_UNCACHED;
      obj_wheres[object].detail = 0;
      if (is_static)
        obj_cache_where (bundle, object, &obj_wheres[object]);
    }
}


/*
 * obj_is_kind()
 * obj_cached_where()
 *
 * Return TRUE if the object is of the given kind, and the cached home of a
 * static object, NULL where the cache has none.
 */
static scr_bool
obj_is_kind (scr_gameref_t game, scr_int object, scr_int kind)
{
  obj_cache_kinds (game);
  if (object < 0 || object >= (scr_int) obj_kinds.size ())
    scr_fatal ("obj_is_kind: invalid object, %ld\n", object);
  return (obj_kinds[object] & (1 << kind)) != 0;
}

static const obj_where_t *
obj_cached_where (scr_gameref_t game, scr_int object)
{
  obj_cache_kinds (game);
  if (object >= 0 && object < (scr_int) obj_wheres.size ()
      && obj_wheres[object].type != OBJ_WHERE_UNCACHED)
    return &obj_wheres[object];
  return nullptr;
}


/*
 * obj_is_static()
 * obj_is_container()
 * obj_is_surface()
 *
 * Convenience functions to return TRUE for given object attributes.
 */
scr_bool
obj_is_static (scr_gameref_t game, scr_int object)
{
  return obj_is_kind (game, object, OBJ_KIND_STATIC);
}

scr_bool
obj_is_container (scr_gameref_t game, scr_int object)
{
  return obj_is_kind (game, object, OBJ_KIND_CONTAINER);
}

scr_bool
obj_is_surface (scr_gameref_t game, scr_int object)
{
  return obj_is_kind (game, object, OBJ_KIND_SURFACE);
}


/*
 * obj_nth_object()
 *
 * Adrift numbers objects of a given kind separately from objects at large,
 * so each kind needs a conversion from that numbering to objects at large:
 * the n'th object of the kind.  As the walk over the objects this replaced
 * did, a negative n gives -1 and an n past the last of the kind gives the
 * last object.
 */
static scr_int
obj_nth_object (scr_gameref_t game, scr_int kind, scr_int n)
{
  obj_cache_kinds (game);
  if (n < 0)
    return -1;
  if (n < (scr_int) obj_numbering[kind].size ())
    return obj_numbering[kind][n];
  return gs_object_count (game) - 1;
}


/*
 * obj_container_object()
 * obj_surface_object()
 * obj_stateful_object()
 * obj_dynamic_object()
 * obj_wearable_object()
 * obj_standable_object()
 * obj_lieable_object()
 *
 * Convert the numbering of each kind of object to object numbering.
 */
scr_int
obj_container_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, OBJ_KIND_CONTAINER, n);
}

scr_int
obj_surface_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, OBJ_KIND_SURFACE, n);
}

scr_int
obj_stateful_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, OBJ_KIND_STATEFUL, n);
}

scr_int
obj_dynamic_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, OBJ_KIND_DYNAMIC, n);
}

scr_int
obj_wearable_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, OBJ_KIND_WEARABLE, n);
}

scr_int
obj_standable_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, OBJ_KIND_STANDABLE, n);
}

scr_int
obj_lieable_object (scr_gameref_t game, scr_int n)
{
  return obj_nth_object (game, OBJ_KIND_LIEABLE, n);
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
    return nullptr;

  /* Find the end of the state string. */
  for (last = first; last < length; last++)
    {
      if (states[last] == '|')
        break;
    }

  /* Allocate and take a copy of the state string. */
  string = (decltype(string)) scr_malloc (last - first + 1);
  memcpy (string, states + first, last - first);
  string[last - first] = '\0';

  return string;
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
  OBJ_DIMENSION_MULTIPLE = 3,
  OBJ_DIMENSION_LIMIT = 0x7fffffff
};


/*
 * obj_clamp_dimension()
 *
 * Hold a count, a multiple or a scale to the range of the Runner's own
 * integers.  The editor writes single digits and a multiple of three, so only
 * a damaged game is ever clamped; unclamped, its numbers overflowed when
 * multiplied together, which is undefined for a signed type.
 */
static scr_int
obj_clamp_dimension (scr_int value)
{
  if (value > OBJ_DIMENSION_LIMIT)
    return OBJ_DIMENSION_LIMIT;
  if (value < -OBJ_DIMENSION_LIMIT)
    return -OBJ_DIMENSION_LIMIT;
  return value;
}


/*
 * obj_get_dimension_multiple()
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

  /*
   * The index is a digit unless the game is damaged, and then it can be any
   * number at all.  Sixty-four factors are enough to carry any power that
   * grows to the limit, and one that does not grow -- a multiple of 0, 1 or
   * -1 -- only depends on whether the index is odd.
   */
  if (index_ > 64)
    index_ = 64 + index_ % 2;

  multiple = obj_clamp_dimension (multiple);
  for (; index_ > 0; index_--)
    retval = obj_clamp_dimension (retval * multiple);

  return retval;
}


/*
 * Recursion depth cap for the weigh loop below.  The Runner's only cycle
 * defence is its i <> self guard, so an authored parent cycle would hang it;
 * we bottom out instead of hanging with it.  32 comfortably exceeds any real
 * containment nesting.
 */
enum { OBJ_WEIGH_DEPTH_LIMIT = 32 };


/*
 * obj_get_size()
 * obj_get_base_weight()
 * obj_weigh()
 * obj_get_weight()
 *
 * Return the relative size and weight of an object.  For containers, the
 * weight includes the weight of each contained object, which obj_weigh()
 * sums to a bounded depth.
 *
 * Static objects have no SizeWeight, and an event can put one in the player's
 * inventory (evt_move_object is the only mover that will), so both functions
 * have to answer for one.  Zero is right: run400 never counts an event-placed
 * object towards the player's limits at all, static or not -- measured live,
 * test/adrift4/notes/RUNNER_TESTS_TODO.md section 9 -- so a static, which can
 * arrive no other way, contributes nothing to the carried totals in either
 * engine.
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

static scr_int
obj_weigh (scr_gameref_t game, scr_int object, scr_int depth)
{
  scr_int weight, other;

  /* Base object weight; zero for statics. */
  weight = obj_get_base_weight (game, object);

  if (depth >= OBJ_WEIGH_DEPTH_LIMIT)
    return weight;

  /*
   * Add the recursive weights of "child" objects: those in or on this one.
   *
   * Deliberate deviation (2026-10-17): run400's weigh routine (Sub_22_63
   * @447600) matches children on the bare per-object container field
   * ([2E]) with only an i <> self guard -- it checks neither the child's
   * position nor whether the weighed object is a container or surface, so
   * objects whose [2E] is stale (leftover raw Parent data that ordinary
   * takes, drops, and task/event moves never touch, or the NPC index its
   * loader leaves on an NPC-held or NPC-worn object) silently add phantom
   * weight; see the runner_parent notes in scgamest.h.  From 2026-08-22 to
   * 2026-10-17 a 4.0 game matched on the runner_parent shadow of that
   * field, which made Welcome to Wonderland's only weapon untakeable (the
   * Card Guard's rod, letter and key weigh into the knife, 94 > 90) and put
   * 19 of goldilocks' worn watch and dress into the package.  That is a
   * Runner bug no game needs, so every version now weighs the position-
   * filtered membership, as 3.9 always did and as `glk capacity on`
   * (game->capacity_recompute) already did at 4.0.  runner_parent itself
   * stays: the possession predicates (gs_runner_possessed()) still read it.
   * Logged under "Deliberate deviations" in
   * test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md.
   */
  GS_FOR_EACH_OBJECT (game, other)
    {
      scr_bool is_child;

      if (other == object)
        continue;

      is_child = (gs_object_position (game, other) == OBJ_IN_OBJECT
                  || gs_object_position (game, other) == OBJ_ON_OBJECT)
                 && gs_object_parent (game, other) == object;

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
obj_convert_player_limit (scr_gameref_t game, scr_int value,
                          scr_int multiple)
{
  scr_int count;

  /*
   * 'Tens' of value multiplied by the scale factor to the power 'units'.
   * Both Runners' loaders (run390 464B14-464C39, run400 48F980-48FAA5) take
   * Val(Left(Format(value, "000"), 2)) * multiple ^ Val(Right(..., 1)), so a
   * value of 1000 or more keeps only its first two digits as the count; the
   * units digit is still the last one.  No corpus game stores one (the
   * largest is 994); unlike Capacity's 3.9 first-digit decode, nothing below
   * 1000 differs.  3.8's pair is synthesised from MaxCarried by the V380
   * fixup, so it keeps the plain decode.
   */
  count = value / OBJ_DIMENSION_DIVISOR;
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    {
      while (count >= 100)
        count /= OBJ_DIMENSION_DIVISOR;
    }

  return obj_clamp_dimension (count)
         * obj_scale (multiple, value % OBJ_DIMENSION_DIVISOR);
}

scr_int
obj_get_player_size_limit (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  return obj_convert_player_limit (game,
                                   prop_get_global_integer (bundle, "MaxSize"),
                                   obj_get_size_multiple (game));
}

scr_int
obj_get_player_weight_limit (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  return obj_convert_player_limit (game,
                                   prop_get_global_integer (bundle, "MaxWt"),
                                   obj_get_weight_multiple (game));
}


/*
 * The version 3.8 carrying model, which is not the version 4.0 one at all.
 *
 * A 3.8 object has a single "Size/weight" class, 0..4, and the player has a
 * single "MaxCarried".  Measured in the genuine run380.exe (2026-08-03, with
 * probe files patched through the plaintext of a 3.80 .taf, in the Wine Runner
 * harness), those are one *pooled burden*: each class costs 1, 3, 7, 3 or 7,
 * the costs of everything held are summed, and the sum may not exceed
 * MaxCarried.  Pinned at MaxCarried 1, 2, 3, 6, 7 and 8: at 2 a class-1 or
 * class-3 object is refused and a class-0 accepted, at 3 both go; at 6
 * Marooned's tires (class 4) are refused, at 7 they are accepted alone, at 8
 * tires + map fit and tires + flint + map do not.  Tires (7) and a class-2 gas
 * can (7) never coexist at any limit, which is what rules out a separate size
 * axis and weight axis: two axes would have let the two heavy objects sit one
 * on each.
 *
 * The refusal is 3.8's only one -- there is no "too heavy" message.  Crime
 * Adventure's kettle (class 2 = 7 against MaxCarried 5) is refused with "Your
 * hands are full." by the real Runner with empty hands, so the pooled check is
 * the one that speaks and lib_object_too_heavy() stands down (see
 * library/sclibrar_put.cpp).
 *
 * Objects that never carried a class -- static objects, and objects a 4.0 or
 * 3.9 game supplies -- weigh nothing here; obj_uses_burden_model() keeps the
 * whole model off for anything but a 3.8 game.
 */
static const scr_int V380_BURDEN_COST[] = { 1, 3, 7, 3, 7 };


/*
 * obj_uses_burden_model()
 *
 * TRUE if the game carries by the pooled burden above: the loader sets the
 * BurdenModel global for a 3.8 game and for nothing else.
 */
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
 * obj_uses_running_weight()
 *
 * TRUE if the carried WEIGHT is read back from the running total.  3.9 joins
 * 4.0 here, though not for obj_uses_running_load()'s leaks: run390's own
 * arithmetic is exact, but its loader seeds the total the way run400's does,
 * with the BASE weight of each object the player starts out holding (openadv
 * 46557C-4655A4) -- a held container's contents are not rolled in -- and
 * every take and drop after that moves the total by the recursive weight.
 * So a game that starts the player with a loaded container runs that much
 * light for good, and recomputing the load from what is held refuses takes
 * the Runner allows.
 *
 * Measured in run390 on Lara Croft - The Sun Obelisk (croft_count, croft_wa,
 * croft_wd probes, 2026-10-09).  Lara starts with her Magnums (9) and a
 * closed backpack (9) holding a journal (9): `count` says 18, not 27.
 * `drop backpack` takes it to 0 with the Magnums still in hand, `get
 * backpack` back to 18.  With an Aztec coin (3) the total is 21, and the belt
 * pouch (9, holding a glow rod, 9) is taken -- 39 of 45 -- where a recomputed
 * 30 + 18 refused it and stopped the walkthrough.  A restore brings back the
 * total the save was made with (39 again after dropping the pouch to 21), so
 * unlike 4.0 the saved figure is kept rather than reseeded.
 *
 * Size is untouched: no container is charged for the size of its contents,
 * so the base-only seed and a recomputation agree.
 */
scr_bool
obj_uses_running_weight (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390;
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
  scr_int class_, classes, burden;

  if (obj_is_static (game, object))
    return 0;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "SizeWeightClass";
  if (!prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
    return 0;

  /* An out-of-range class costs what class 0 does. */
  class_ = vt_rvalue.integer;
  classes = sizeof (V380_BURDEN_COST) / sizeof (V380_BURDEN_COST[0]);
  burden = V380_BURDEN_COST[(class_ >= 0 && class_ < classes) ? class_ : 0];

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
 * the one refusal for all of this, "The box is full." (see
 * library/sclibrar_put.cpp).
 *
 * Version 3.9 reads only the FIRST digit of the count.  Its loader (run390
 * 46537C-46540C) takes Val(Left(Format(Capacity, "00"), 1)) times the scale
 * factor to the power of Val(Right(..., 1)), so a Capacity of 100 is one
 * object of size 1, not ten.  run400 fixed it: its loader takes
 * Left(s, Len(s) - 1) (4904D3-490574).  Measured on pPUTREF39.taf
 * (make_39_putrefprobe.py) under run390x, 2026-09-19: an empty bag of
 * Capacity 100 takes the stone and then refuses four more size-1 objects
 * with "The lamp can't fit inside the bag at the moment."
 * (runner_probes/putref.run390.v4.txt).
 */
scr_int
obj_get_container_capacity (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int capacity, packed, count;

  packed = prop_get_indexed_integer (bundle, "Objects", object, "Capacity");

  count = packed / OBJ_DIMENSION_DIVISOR;

  /*
   * Deliberate deviation (2026-09-30): run390 reads only the first digit
   * of the count (6db5e9698) -- it would divide count by ten until it fell
   * below ten -- so a 3.9 container authored to hold ten or more objects
   * held one.  Scarier honours the authored count; a game the Runner could
   * finish can never need less room than that.
   */
  if (prop_get_taf_version (bundle) == TAF_VERSION_390
      && count >= OBJ_DIMENSION_DIVISOR)
    SCR_DEVIATION ("capacity_390", "object=%ld count=%ld", object, count);

  capacity = obj_clamp_dimension (count)
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

  GS_FOR_EACH_OBJECT (game, other)
    {
      if (gs_object_position (game, other) == OBJ_IN_OBJECT
          && gs_object_parent (game, other) == object)
        free_space -= obj_get_size (game, other);
    }

  if (obj_trace)
    scr_trace ("Object: object %ld has %ld free\n", object, free_space);

  return free_space;
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
 * Measured cell by cell on probe ISARE (run400,
 * runner_probes/isare.run400.txt, 2026-09-07), twelve objects one per
 * spelling, read back through `where <name>`:
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

  /*
   * Scarier deliberately keeps SCARE's case-insensitive nest instead of the
   * measured chain (deviation policy): the chain's only visible effect is
   * wrong agreement -- "some fungus are", "A bag of apples are", "the NAILS
   * is" -- so "a"/"an" (any case) or an empty prefix is singular, and
   * otherwise a name ending in "s" but not "us" is plural.
   */
  is_plural = FALSE;
  if (!(scr_strempty (prefix)
        || scr_compare_word (prefix, "a", 1)
        || scr_compare_word (prefix, "an", 2)))
    {
      if (length > 0
          && scr_tolower (name[length - 1]) == 's'
          && (length < 2 || scr_tolower (name[length - 2]) != 'u'))
        is_plural = TRUE;
    }

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
  const obj_where_t *where;
  scr_int type;

  /* Static object moved to player or room by event? */
  if (!gs_object_static_unmoved (game, object))
    {
      if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
        return is_indirect ? gs_player_in_room (game, room) : FALSE;
      else if (!gs_object_static_rooms_empty (game, object))
        return gs_object_in_static_rooms (game, object, room);
      else
        return gs_object_position (game, object) - 1 == room;
    }

  /* Check and return the room list for the object. */
  where = obj_cached_where (game, object);
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Where";
  vt_key[3].string = "Type";
  type = where ? where->type : prop_get_integer (bundle, "I<-siss", vt_key);
  switch (type)
    {
    case ROOMLIST_ALL_ROOMS:
      return TRUE;
    case ROOMLIST_NO_ROOMS:
      return FALSE;

    case ROOMLIST_ONE_ROOM:
      if (where)
        return where->detail == room + 1;
      vt_key[3].string = "Room";
      return prop_get_integer (bundle, "I<-siss", vt_key) == room + 1;

    case ROOMLIST_SOME_ROOMS:
      if (where && room + 1 >= 0 && room + 1 < (scr_int) where->rooms.size ())
        return where->rooms[room + 1];
      vt_key[3].string = "Rooms";
      vt_key[4].integer = room + 1;
      return prop_get_boolean (bundle, "B<-sissi", vt_key);

    case ROOMLIST_NPC_PART:
      {
        scr_int npc;

        if (!is_indirect)
          return FALSE;

        vt_key[2].string = "Parent";
        npc = where ? where->detail
                    : prop_get_integer (bundle, "I<-sis", vt_key);
        if (npc == 0 || !gs_npc_valid (game, npc - 1))
          return gs_player_in_room (game, room);
        else
          return npc_in_room (game, npc - 1, room);
      }

    default:
      scr_fatal ("obj_static_in_room: invalid type, %ld\n", type);
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
obj_indirectly_in_room_internal (scr_gameref_t game,
                                 scr_int object, scr_int room)
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

        case OBJ_IN_OBJECT:    /* In another object, if that is open. */
          switch (gs_object_openness (game, parent))
            {
            case OBJ_WONTCLOSE:
            case OBJ_OPEN:
              return obj_indirectly_in_room (game, parent, room);
            default:
              return FALSE;
            }

        case OBJ_ON_OBJECT:    /* On another object. */
          return obj_indirectly_in_room (game, parent, room);

        default:               /* Within a room. */
          if (position > gs_room_count (game) + 1)
            {
              scr_error ("obj_indirectly_in_room:"
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
  /* See if the object is static or dynamic.  A static is held only if an
     event has moved it into the player's hands. */
  if (obj_is_static (game, object))
    {
      return !gs_object_static_unmoved (game, object)
             && gs_object_position (game, object) == OBJ_HELD_PLAYER;
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

        case OBJ_IN_OBJECT:    /* In another object, if that is open. */
          switch (gs_object_openness (game, parent))
            {
            case OBJ_WONTCLOSE:
            case OBJ_OPEN:
              return obj_indirectly_held_by_player (game, parent);
            default:
              return FALSE;
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
 * above them losing a further two, so that a room ends up one-based.
 * obj_shows_initial_description() needs it, and there only the room and
 * "held by the player" codes can ever match anything; run390's post-put
 * sweep (lib_put_task_sweep_390()) reads the same field for a static, whose
 * o(22) the loader never touches again.
 */
scr_int
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
 * Measured in run400's runner_probes/xfiles.run400.txt (The X-Files, 4.00):
 * task 7 "Use Key" carries ShowRoomDesc = 0, so entering Garage 5 through it
 * prints no room description -- and `take knife` there answers "Take what?",
 * although the Small Pocket Knife (object 31, InitialPosition 11 = room 7) is
 * lying loose on the floor.  The very next command, `out`, moves normally, so
 * the player really is in the room; the knife simply does not exist to the
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

  GS_FOR_EACH_OBJECT (game, index_)
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

  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (gs_object_seen (game, index_) || !obj_is_static (game, index_))
        continue;

      if (obj_static_in_room (game, index_, room, TRUE))
        gs_set_object_seen (game, index_, TRUE);
    }
}


/*
 * obj_mark_npc_parts_seen()
 *
 * 4.0: mark seen every part-of-character static whose holder is the player,
 * or a seen character in the player's room.  run400's presence test obhere
 * (452E9C) writes the seen byte itself in its &HE2 branch -- parent 0 at
 * 452E08, a parent NPC in the player room whose own seen byte (26) is 1 at
 * 452E5E -- and the up-front noun resolver 463640, called once per typed line
 * from generaltasks (48A3F5), runs obhere over the whole object table in its
 * first pass (4630D5-46311C).  So a character's parts become referenceable on
 * the first line typed while the seen character stands there, with no room
 * listing needed.  Measured on humbug (runner_transcripts/humbug.txt, T727):
 * Jasper arrives by event, `X Jasper`, `i`, then `X teeth` describes his
 * teeth (object 180, part of Jasper) instead of "Nothing Special.".
 */
void
obj_mark_npc_parts_seen (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  scr_int index_;

  if (prop_get_taf_version (bundle) < TAF_VERSION_400)
    return;

  GS_FOR_EACH_OBJECT (game, index_)
    {
      const obj_where_t *where;
      scr_int type, npc;

      if (gs_object_seen (game, index_) || !obj_is_static (game, index_)
          || !gs_object_static_unmoved (game, index_))
        continue;

      where = obj_cached_where (game, index_);
      vt_key[0].string = "Objects";
      vt_key[1].integer = index_;
      vt_key[2].string = "Where";
      vt_key[3].string = "Type";
      type = where ? where->type : prop_get_integer (bundle, "I<-siss", vt_key);
      if (type != ROOMLIST_NPC_PART)
        continue;

      vt_key[2].string = "Parent";
      npc = where ? where->detail
                  : prop_get_integer (bundle, "I<-sis", vt_key);
      if (npc == 0 || !gs_npc_valid (game, npc - 1)
          || (npc_in_room (game, npc - 1, gs_playerroom (game))
              && gs_npc_seen (game, npc - 1)))
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
