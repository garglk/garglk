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

#include <vector>

#include "scarier.h"
#include "scprotos.h"


/* Assorted definitions and constants. */
static const scr_uint PROP_MAGIC = 0x7927b2e0;
enum
{ PROP_GROW_INCREMENT = 32,
  MAX_INTEGER_KEY = 65535,
  NODE_POOL_CAPACITY = 512
};
static const scr_char NUL = '\0';

/* Properties trace flag. */
static scr_bool prop_trace = FALSE;


/*
 * Property tree node definition, uses a child list representation for
 * fast lookup on indexed nodes.  Name is a variable type, as is property,
 * which is also overloaded to contain the child count for internal nodes.
 */
typedef struct scr_prop_node_s
{
  scr_vartype_t name;
  scr_vartype_t property;

  struct scr_prop_node_s **child_list;

  /*
   * TRUE when a string-keyed child_list is sorted by name for binary search
   * (see prop_find_child).  Cleared whenever a string child is appended;
   * meaningless for integer-keyed nodes, whose children are positional.
   */
  scr_bool is_sorted;

  /*
   * The string-keyed child returned by the last successful lookup, verified
   * by strcmp before reuse.  Loops asking a node for the same key repeatedly
   * (the task scan asking the root for "Tasks", say) hit here in one compare
   * instead of a binary search.
   */
  struct scr_prop_node_s *last_found;
} scr_prop_node_t;
typedef scr_prop_node_t *scr_prop_noderef_t;

/*
 * Properties set structure.  This is a set of properties, on which the
 * properties functions operate (a properties "object").  Node string
 * names are held in a dictionary to help save space.  To avoid excessive
 * malloc'ing of nodes, new nodes are preallocated in pools.
 *
 * The bookkeeping arrays are std::vector (P3 RAII); their *contents* stay
 * raw -- the dictionary strings, node pool slabs, and orphans are owned
 * allocations freed by prop_destroy().  The node slabs themselves and the
 * per-node child lists are deliberately left as raw arrays: tree nodes are
 * referenced by address all over the tree, so the slabs must never move,
 * and the child list scan is the engine's hottest loop (see P4 notes).
 */
typedef struct scr_prop_set_s
{
  scr_uint magic;
  std::vector<scr_char *> dictionary;
  std::vector<scr_prop_noderef_t> node_pools;
  scr_int node_count;
  std::vector<void *> orphans;
  scr_bool is_readonly;
  scr_prop_noderef_t root_node;
  scr_tafref_t taf;
} scr_prop_set_t;


/*
 * prop_is_valid()
 *
 * Return TRUE if pointer is a valid properties set, FALSE otherwise.
 */
static scr_bool
prop_is_valid (scr_prop_setref_t bundle)
{
  return bundle && bundle->magic == PROP_MAGIC;
}


/*
 * prop_round_up()
 *
 * Round up a count of elements to the next block of grow increments.
 */
static scr_int
prop_round_up (scr_int elements)
{
  scr_int extended;

  extended = elements + PROP_GROW_INCREMENT - 1;
  return (extended / PROP_GROW_INCREMENT) * PROP_GROW_INCREMENT;
}


/*
 * prop_ensure_capacity()
 *
 * Ensure that capacity exists in a growable array for a given number of
 * elements, growing if necessary.
 *
 * Some libc's allocate generously on realloc(), and some not.  Those that
 * don't will thrash badly if we realloc() on each grow, so here we try to
 * realloc() in blocks of elements, and thus need to realloc() much less
 * frequently.
 */
static void *
prop_ensure_capacity (void *array,
                      scr_int old_size, scr_int new_size, scr_int element_size)
{
  scr_int current, required;

  /*
   * See if there's any resize necessary, that is, does the new size round up
   * to a larger number of elements than the old size.
   */
  current = prop_round_up (old_size);
  required = prop_round_up (new_size);
  if (required > current)
    {
      scr_byte *new_array, *start_clearing;

      /*
       * Grow array to the required size, and zero new elements.  scr_realloc()
       * does not clear the bytes it grows into (only a fresh allocation is
       * zeroed), so clear from old_size -- not the rounded-up `current` -- to
       * cover the whole not-yet-live region [old_size, required).  In the
       * common grow path [old_size, current) was already zeroed by the previous
       * grow, so this is a harmless re-zero; but an array whose allocation was
       * shrunk by prop_trim_capacity() and then grown again (the orphans array,
       * which stays growable past prop_solidify() for runtime-mutable strings)
       * would otherwise read indeterminate realloc tail bytes.
       */
      new_array = (decltype(new_array)) scr_realloc (array, required * element_size);
      start_clearing = new_array + old_size * element_size;
      memset (start_clearing, 0, (required - old_size) * element_size);

      return new_array;
    }

  /* No resize necessary. */
  return array;
}


/*
 * prop_trim_capacity()
 *
 * Trim an array allocation back to the bare minimum required.  This will
 * "unblock" the allocations of prop_ensure_capacity().  Once trimmed,
 * the array cannot ever be grown safely again.
 */
static void *
prop_trim_capacity (void *array, scr_int size, scr_int element_size)
{
  if (prop_round_up (size) > size)
    return scr_realloc (array, size * element_size);
  else
    return array;
}


/*
 * prop_compare()
 *
 * String comparison routine for sorting and searching dictionary strings.
 * The function has return type "int" to match the libc implementations of
 * bsearch() and qsort().
 */
static int
prop_compare (const void *string1, const void *string2)
{
  return strcmp (*(scr_char *const *) string1, *(scr_char *const *) string2);
}


/*
 * prop_dictionary_lookup()
 *
 * Find a string in the dictionary.  If the string is not present, the
 * function will add it.  The function returns the string's address, if
 * either added or already present.  Any new dictionary entry will
 * contain a malloced copy of the string passed in.
 */
static const scr_char *
prop_dictionary_lookup (scr_prop_setref_t bundle, const scr_char *string)
{
  scr_char *dict_string;

  /*
   * Search the existing dictionary for the string.  Although not GNU libc,
   * some libc's loop or crash when given a list of zero length, so we need to
   * trap that here.
   */
  if (!bundle->dictionary.empty ())
    {
      const scr_char *const *dict_search;

      dict_search = (decltype(dict_search)) bsearch (&string, bundle->dictionary.data (),
                             bundle->dictionary.size (),
                             sizeof (bundle->dictionary[0]), prop_compare);
      if (dict_search)
        return *dict_search;
    }

  /* Not found, so copy the string for dictionary insertion. */
  dict_string = (decltype(dict_string)) scr_malloc (strlen (string) + 1);
  memcpy (dict_string, string, strlen (string) + 1);

  /* Add the new entry to the end of the dictionary array, and sort. */
  bundle->dictionary.push_back (dict_string);
  qsort (bundle->dictionary.data (),
         bundle->dictionary.size (),
         sizeof (bundle->dictionary[0]), prop_compare);

  /* Return the address of the new string. */
  return dict_string;
}


/*
 * prop_new_node()
 *
 * Return the address of the next free properties node from the node pool.
 * Using a pool gives a performance boost; the number of properties nodes
 * for even a small game is large, and preallocating pools avoids excessive
 * malloc's of small individual nodes.
 */
static scr_prop_noderef_t
prop_new_node (scr_prop_setref_t bundle)
{
  scr_int node_index;
  scr_prop_noderef_t node;

  /* See if we need to create a new node pool. */
  node_index = bundle->node_count % NODE_POOL_CAPACITY;
  if (node_index == 0)
    {
      scr_int required;

      /* Create a new node pool.  The slab itself stays a raw allocation --
         nodes are referenced by address throughout the tree, so it must
         never move. */
      required = NODE_POOL_CAPACITY * sizeof (*bundle->node_pools[0]);
      bundle->node_pools.push_back ((scr_prop_node_s *) scr_malloc (required));
    }

  /* Find the next node address, and increment the node counter. */
  node = bundle->node_pools.back () + node_index;
  bundle->node_count++;

  /* Return the new node. */
  return node;
}


/*
 * prop_compare_child_names()
 *
 * qsort() comparator ordering string-keyed child nodes by their interned
 * names, for the binary search in prop_find_child().
 */
static int
prop_compare_child_names (const void *child1, const void *child2)
{
  return strcmp ((*(scr_prop_noderef_t const *) child1)->name.string,
                 (*(scr_prop_noderef_t const *) child2)->name.string);
}


/*
 * prop_find_child()
 *
 * Find a child node of the given parent whose name matches that passed in.
 */
static scr_prop_noderef_t
prop_find_child (scr_prop_noderef_t parent, scr_int type, scr_vartype_t name)
{
  /* See if this node has any children. */
  if (parent->child_list)
    {
      scr_int index_;
      scr_prop_noderef_t child;

      /* Do the lookup based on name type. */
      switch (type)
        {
        case PROP_KEY_INTEGER:
          /*
           * As with adding a child below, here we'll range-check an integer
           * key just to make sure nobody has any unreal expectations of us.
           */
          if (name.integer < 0)
            scr_fatal ("prop_find_child: integer key cannot be negative\n");
          else if (name.integer > MAX_INTEGER_KEY)
            scr_fatal ("prop_find_child: integer key is too large\n");

          /*
           * For integer lookups, return the child at the indexed offset
           * directly, provided it exists.
           */
          if (name.integer >= 0 && name.integer < parent->property.integer)
            {
              child = parent->child_list[name.integer];
              return child;
            }
          break;

        case PROP_KEY_STRING:
          /*
           * Look up a string name among the children.  This is the engine's
           * single hottest inner loop (tens of millions of iterations on a
           * full walkthrough -- see the P4 profiling notes and the v4
           * profiling memo).  String children carry no positional meaning
           * (this lookup is the only reader, and an earlier move-to-front
           * scheme reordered them freely), so keep each list sorted by name
           * and binary search it.  Lists are sorted lazily on first lookup;
           * prop_add_child clears is_sorted when it appends, so load-time
           * find/add interleaving stays correct and the steady state after
           * load is one sort per node, then log2(n) strcmp probes per lookup
           * in place of a linear scan plus a move-to-front memmove.
           */
          {
            const scr_char *const key = name.string;
            scr_int low, high;

            /* Re-check the last hit first; loops repeat the same key. */
            child = parent->last_found;
            if (child && strcmp (key, child->name.string) == 0)
              return child;

            if (!parent->is_sorted)
              {
                qsort (parent->child_list, parent->property.integer,
                       sizeof (*parent->child_list), prop_compare_child_names);
                parent->is_sorted = TRUE;
              }

            low = 0;
            high = parent->property.integer - 1;
            while (low <= high)
              {
                const scr_int middle = (low + high) / 2;
                const int comparison =
                    strcmp (key, parent->child_list[middle]->name.string);

                if (comparison == 0)
                  {
                    parent->last_found = parent->child_list[middle];
                    return parent->child_list[middle];
                  }
                else if (comparison < 0)
                  high = middle - 1;
                else
                  low = middle + 1;
              }
          }
          break;

        default:
          scr_fatal ("prop_find_child: invalid key type\n");
        }
    }

  /* No matching child found. */
  return NULL;
}


/*
 * prop_add_child()
 *
 * Add a new child node to the given parent.  Return its reference.  Set
 * needs to be passed so that string names can be added to the dictionary.
 */
static scr_prop_noderef_t
prop_add_child (scr_prop_noderef_t parent,
                scr_int type, scr_vartype_t name, scr_prop_setref_t bundle)
{
  scr_prop_noderef_t child;

  /* Not possible if growable allocations have been trimmed. */
  if (bundle->is_readonly)
    scr_fatal ("prop_add_child: can't add to readonly properties\n");

  /* Create the new node. */
  child = prop_new_node (bundle);
  switch (type)
    {
    case PROP_KEY_INTEGER:
      child->name.integer = name.integer;
      break;
    case PROP_KEY_STRING:
      child->name.string = prop_dictionary_lookup (bundle, name.string);
      break;

    default:
      scr_fatal ("prop_add_child: invalid key type\n");
    }

  /* Initialize property and child list to visible nulls. */
  child->property.voidp = NULL;
  child->child_list = NULL;
  child->is_sorted = FALSE;
  child->last_found = NULL;

  /* Make a brief check for obvious overwrites. */
  if (!parent->child_list && parent->property.voidp)
    scr_error ("prop_add_child: node overwritten, probable data loss\n");

  /* Add the child to the parent, position dependent on key type. */
  switch (type)
    {
    case PROP_KEY_INTEGER:
      /*
       * Range check on integer keys, must be >= 0 for direct indexing to work,
       * and we'll also apply a reasonableness constraint, to try to catch
       * errors where string pointers are passed in as integers, which would
       * otherwise lead to some extreme malloc() attempts.
       */
      if (name.integer < 0)
        scr_fatal ("prop_add_child: integer key cannot be negative\n");
      else if (name.integer > MAX_INTEGER_KEY)
        scr_fatal ("prop_add_child: integer key is too large\n");

      /* Resize the parent's child list if necessary. */
      parent->child_list = (decltype(parent->child_list)) prop_ensure_capacity (parent->child_list,
                                                 parent->property.integer,
                                                 name.integer + 1,
                                                 sizeof (*parent->child_list));

      /* Update the child count if the new node increases it. */
      if (parent->property.integer <= name.integer)
        parent->property.integer = name.integer + 1;

      /* Store the child in its indexed list location. */
      parent->child_list[name.integer] = child;
      break;

    case PROP_KEY_STRING:
      /* Add a single entry to the child list, and resize. */
      parent->child_list = (decltype(parent->child_list)) prop_ensure_capacity (parent->child_list,
                                                 parent->property.integer,
                                                 parent->property.integer + 1,
                                                 sizeof (*parent->child_list));

      /* Store the child at the end of the list; the append invalidates any
       * sorted order prop_find_child established (re-sorted on next find). */
      parent->child_list[parent->property.integer++] = child;
      parent->is_sorted = FALSE;
      break;

    default:
      scr_fatal ("prop_add_child: invalid key type\n");
    }

  return child;
}


/*
 * prop_trace_value()
 * prop_trace_keys()
 *
 * Trace helpers shared by prop_put() and prop_get().  The first prints one
 * property value according to the type character in format[0]; the second
 * prints the comma-separated key elements described by format[3...].  Both
 * are only ever called with prop_trace set.
 */
static void
prop_trace_value (scr_char type, const scr_vartype_t vt_value)
{
  switch (type)
    {
    case PROP_STRING:
      scr_trace ("\"%s\"", vt_value.string);
      break;
    case PROP_INTEGER:
      scr_trace ("%ld", vt_value.integer);
      break;
    case PROP_BOOLEAN:
      scr_trace ("%s", vt_value.boolean ? "true" : "false");
      break;

    default:
      scr_trace ("%p [invalid type]", vt_value.voidp);
      break;
    }
}

static void
prop_trace_keys (const scr_char *format, const scr_vartype_t vt_key[])
{
  scr_int index_;

  for (index_ = 0; format[index_ + 3] != NUL; index_++)
    {
      scr_trace ("%s", index_ > 0 ? "," : "");
      switch (format[index_ + 3])
        {
        case PROP_KEY_STRING:
          scr_trace ("\"%s\"", vt_key[index_].string);
          break;
        case PROP_KEY_INTEGER:
          scr_trace ("%ld", vt_key[index_].integer);
          break;

        default:
          scr_trace ("%p [invalid type]", vt_key[index_].voidp);
          break;
        }
    }
}


/*
 * prop_put()
 *
 * Add a property to a properties set.  Duplicate entries will replace
 * prior ones.
 *
 * Stores a value of variable type as a property.  The value type is one of
 * 'I', 'B', or 'S', for integer, boolean, and string values, held in the
 * first character of format.  The next two characters of format are "->",
 * and are syntactic sugar.  The remainder of format shows the key makeup,
 * with 'i' indicating integer, and 's' string key elements.  Example format:
 * "I->sssis", stores an integer, with a key composed of three strings, an
 * integer, and another string.
 */
void
prop_put (scr_prop_setref_t bundle, const scr_char *format,
          scr_vartype_t vt_value, const scr_vartype_t vt_key[])
{
  scr_prop_noderef_t node;
  scr_int index_;
  assert (prop_is_valid (bundle));

  /* Format check. */
  if (!format || format[0] == NUL
      || format[1] != '-' || format[2] != '>' || format[3] == NUL)
    scr_fatal ("prop_put: format error\n");

  /* Trace property put. */
  if (prop_trace)
    {
      scr_trace ("Property: put ");
      prop_trace_value (format[0], vt_value);
      scr_trace (", key \"%s\" : ", format);
      prop_trace_keys (format, vt_key);
      scr_trace ("\n");
    }

  /*
   * Iterate keys, finding matching child nodes at each level.  If no matching
   * child is found, insert one and continue.
   */
  node = bundle->root_node;
  for (index_ = 0; format[index_ + 3] != NUL; index_++)
    {
      scr_prop_noderef_t child;
      scr_int type;

      /*
       * Search this level for a name matching the key.  If found, advance
       * to that child node.  Otherwise, add the node to the tree, including
       * the set so that the dictionary can be extended.
       */
      type = format[index_ + 3];
      child = prop_find_child (node, type, vt_key[index_]);
      if (child)
        node = child;
      else
        node = prop_add_child (node, type, vt_key[index_], bundle);
    }

  /*
   * Ensure that we're not about to overwrite an internal node child count.
   */
  if (node->child_list)
    scr_fatal ("prop_put: overwrite of internal node\n");

  /* Set our properties in the final node. */
  switch (format[0])
    {
    case PROP_INTEGER:
      node->property.integer = vt_value.integer;
      break;
    case PROP_BOOLEAN:
      node->property.boolean = vt_value.boolean;
      break;
    case PROP_STRING:
      node->property.string = vt_value.string;
      break;

    default:
      scr_fatal ("prop_put: invalid property type\n");
    }
}


/*
 * prop_get()
 *
 * Retrieve a property from a properties set.  Format stuff as above, except
 * with "->" replaced with "<-".  Returns FALSE if no such property exists.
 */
/*
 * prop_put_integer()
 *
 * Update the integer value of an already-present leaf node, addressed the same
 * way as prop_get().  Used for the rare runtime-mutable global (player gender,
 * chosen at game start).  Returns FALSE if the addressed node doesn't exist.
 */
scr_bool
prop_put_integer (scr_prop_setref_t bundle, const scr_char *format,
                  scr_int value, const scr_vartype_t vt_key[])
{
  scr_prop_noderef_t node;
  scr_int index_;
  assert (prop_is_valid (bundle));

  if (!format || format[0] == NUL
      || format[1] != '<' || format[2] != '-' || format[3] == NUL)
    scr_fatal ("prop_put_integer: format error\n");

  node = bundle->root_node;
  for (index_ = 0; format[index_ + 3] != NUL; index_++)
    {
      node = prop_find_child (node, format[index_ + 3], vt_key[index_]);
      if (!node)
        return FALSE;
    }
  node->property.integer = value;
  return TRUE;
}


/*
 * prop_put_string()
 *
 * Update the string value of an already-present leaf node.  This runs at game
 * start (the runtime-mutable player name), which is after prop_solidify() has
 * trimmed and frozen the string dictionary, so the value cannot be interned
 * there -- growing the trimmed dictionary overruns it.  Instead the bundle
 * takes a private copy and adopts it (freed on teardown like any orphan); the
 * orphans list is deliberately left growable past solidify for exactly this.
 * Returns FALSE if the addressed node doesn't exist.
 */
scr_bool
prop_put_string (scr_prop_setref_t bundle, const scr_char *format,
                 const scr_char *value, const scr_vartype_t vt_key[])
{
  scr_prop_noderef_t node;
  scr_int index_;
  scr_char *copy;
  assert (prop_is_valid (bundle));

  if (!format || format[0] == NUL
      || format[1] != '<' || format[2] != '-' || format[3] == NUL)
    scr_fatal ("prop_put_string: format error\n");

  node = bundle->root_node;
  for (index_ = 0; format[index_ + 3] != NUL; index_++)
    {
      node = prop_find_child (node, format[index_ + 3], vt_key[index_]);
      if (!node)
        return FALSE;
    }

  /* Take a bundle-owned copy of the value and point the node at it. */
  copy = (decltype(copy)) scr_malloc (strlen (value) + 1);
  memcpy (copy, value, strlen (value) + 1);
  prop_adopt (bundle, copy);
  node->property.string = copy;
  return TRUE;
}


scr_bool
prop_get (scr_prop_setref_t bundle, const scr_char *format,
          scr_vartype_t *vt_rvalue, const scr_vartype_t vt_key[])
{
  scr_prop_noderef_t node;
  scr_int index_;
  assert (prop_is_valid (bundle));

  /* Format check. */
  if (!format || format[0] == NUL
      || format[1] != '<' || format[2] != '-' || format[3] == NUL)
    scr_fatal ("prop_get: format error\n");

  /* Trace property get. */
  if (prop_trace)
    {
      scr_trace ("Property: get, key \"%s\" : ", format);
      prop_trace_keys (format, vt_key);
      scr_trace ("\n");
    }

  /*
   * Iterate keys, finding matching child nodes at each level.  Stop if no
   * matching child is found.
   */
  node = bundle->root_node;
  for (index_ = 0; format[index_ + 3] != NUL; index_++)
    {
      scr_int type;

      /* Move node down to the matching child, NULL if no match. */
      type = format[index_ + 3 ];
      node = prop_find_child (node, type, vt_key[index_]);
      if (!node)
        break;
    }

  /* If key iteration halted because no child was found, return FALSE. */
  if (!node)
    {
      if (prop_trace)
        scr_trace ("Property: ...get FAILED\n");

      return FALSE;
    }

  /*
   * Enforce integer-only queries on internal nodes, since this is the only
   * type of query that makes sense -- any other type is probably a mistake.
   */
  if (node->child_list && format[0] != PROP_INTEGER)
    scr_fatal ("prop_get: only integer gets on internal nodes\n");

  /* Return the properties of the final node. */
  switch (format[0])
    {
    case PROP_INTEGER:
      vt_rvalue->integer = node->property.integer;
      break;
    case PROP_BOOLEAN:
      vt_rvalue->boolean = node->property.boolean;
      break;
    case PROP_STRING:
      vt_rvalue->string = node->property.string;
      break;

    default:
      scr_fatal ("prop_get: invalid property type\n");
    }

  /* Complete tracing property get. */
  if (prop_trace)
    {
      scr_trace ("Property: ...get returned : ");
      prop_trace_value (format[0], *vt_rvalue);
      scr_trace ("\n");
    }
  return TRUE;
}


/*
 * prop_trim_node()
 * prop_solidify()
 *
 * Trim excess allocation from growable arrays, and fix the properties set
 * so that no further property insertions are allowed.
 */
static void
prop_trim_node (scr_prop_noderef_t node)
{
  /* End recursion on null or childless node. */
  if (node && node->child_list)
    {
      scr_int index_;

      /* Recursively trim allocation on children. */
      for (index_ = 0; index_ < node->property.integer; index_++)
        prop_trim_node (node->child_list[index_]);

      /* Trim allocation on this node. */
      node->child_list = (decltype(node->child_list)) prop_trim_capacity (node->child_list,
                                             node->property.integer,
                                             sizeof (*node->child_list));
    }
}

void
prop_solidify (scr_prop_setref_t bundle)
{
  assert (prop_is_valid (bundle));

  /*
   * Trim back the dictionary, pools array, and every internal tree node.  Two
   * things are deliberately _not_ trimmed.  The final node pool, because there
   * are references to nodes within it strewn all over the properties tree, and
   * it's a large job to try to find and update them; instead, we just live
   * with a little wasted heap memory.  And the orphans array, because it stays
   * growable past solidify so that runtime-mutable string properties (the
   * player name, set at game start) can adopt a bundle-owned copy of their
   * value -- see prop_put_string().
   */
  bundle->dictionary.shrink_to_fit ();
  bundle->node_pools.shrink_to_fit ();
  prop_trim_node (bundle->root_node);

  /* Set the bundle so that no more properties can be added. */
  bundle->is_readonly = TRUE;
}


/*
 * prop_get_integer()
 * prop_get_boolean()
 * prop_get_string()
 *
 * Convenience functions to retrieve a property of a known type directly.
 * It is an error for the property not to exist on retrieval.
 */
scr_int
prop_get_integer (scr_prop_setref_t bundle,
                  const scr_char *format, const scr_vartype_t vt_key[])
{
  scr_vartype_t vt_rvalue;
  assert (format[0] == PROP_INTEGER);

  if (!prop_get (bundle, format, &vt_rvalue, vt_key))
    scr_fatal ("prop_get_integer: can't retrieve property\n");

  return vt_rvalue.integer;
}

scr_bool
prop_get_boolean (scr_prop_setref_t bundle,
                  const scr_char *format, const scr_vartype_t vt_key[])
{
  scr_vartype_t vt_rvalue;
  assert (format[0] == PROP_BOOLEAN);

  if (!prop_get (bundle, format, &vt_rvalue, vt_key))
    scr_fatal ("prop_get_boolean: can't retrieve property\n");

  return vt_rvalue.boolean;
}

const scr_char *
prop_get_string (scr_prop_setref_t bundle,
                 const scr_char *format, const scr_vartype_t vt_key[])
{
  scr_vartype_t vt_rvalue;
  assert (format[0] == PROP_STRING);

  if (!prop_get (bundle, format, &vt_rvalue, vt_key))
    scr_fatal ("prop_get_string: can't retrieve property\n");

  return vt_rvalue.string;
}


/*
 * prop_get_global_integer()
 * prop_get_global_boolean()
 * prop_get_global_string()
 *
 * Fetch bundle["Globals"][name], the game's header properties (GameName,
 * Perspective, MaxScore, BattleSystem, ...).  Every caller of these built the
 * same two-element vt_key by hand; there is nothing per-call-site about it
 * beyond the name, so it lives here next to the prop_get_* it wraps.
 */
scr_int
prop_get_global_integer (scr_prop_setref_t bundle, const scr_char *name)
{
  scr_vartype_t vt_key[2];

  vt_key[0].string = "Globals";
  vt_key[1].string = name;
  return prop_get_integer (bundle, "I<-ss", vt_key);
}

scr_bool
prop_get_global_boolean (scr_prop_setref_t bundle, const scr_char *name)
{
  scr_vartype_t vt_key[2];

  vt_key[0].string = "Globals";
  vt_key[1].string = name;
  return prop_get_boolean (bundle, "B<-ss", vt_key);
}

const scr_char *
prop_get_global_string (scr_prop_setref_t bundle, const scr_char *name)
{
  scr_vartype_t vt_key[2];

  vt_key[0].string = "Globals";
  vt_key[1].string = name;
  return prop_get_string (bundle, "S<-ss", vt_key);
}


/*
 * prop_get_indexed_integer()
 * prop_get_indexed_boolean()
 * prop_get_indexed_string()
 *
 * Fetch bundle[class][index][name] -- one field of one member of an indexed
 * collection, which is how nearly every game entity is read: "Objects" 42
 * "Short", "NPCs" 3 "Name", "Tasks" 7 "Command".  `class_` is the collection
 * name as it appears in the TAF schema (see sctafpar.cpp).
 *
 * Deeper keys (per-alt, per-exit, per-action rows -- the "I<-sisis" family)
 * still build their vt_key by hand: the extra index has no single spelling
 * worth freezing into a signature here.
 */
scr_int
prop_get_indexed_integer (scr_prop_setref_t bundle, const scr_char *class_,
                          scr_int index_, const scr_char *name)
{
  scr_vartype_t vt_key[3];

  vt_key[0].string = class_;
  vt_key[1].integer = index_;
  vt_key[2].string = name;
  return prop_get_integer (bundle, "I<-sis", vt_key);
}

scr_bool
prop_get_indexed_boolean (scr_prop_setref_t bundle, const scr_char *class_,
                          scr_int index_, const scr_char *name)
{
  scr_vartype_t vt_key[3];

  vt_key[0].string = class_;
  vt_key[1].integer = index_;
  vt_key[2].string = name;
  return prop_get_boolean (bundle, "B<-sis", vt_key);
}

const scr_char *
prop_get_indexed_string (scr_prop_setref_t bundle, const scr_char *class_,
                         scr_int index_, const scr_char *name)
{
  scr_vartype_t vt_key[3];

  vt_key[0].string = class_;
  vt_key[1].integer = index_;
  vt_key[2].string = name;
  return prop_get_string (bundle, "S<-sis", vt_key);
}


/*
 * prop_get_child_count()
 *
 * Convenience function to retrieve a count of child properties available
 * for a given property.  Returns zero if the property does not exist.
 */
scr_int
prop_get_child_count (scr_prop_setref_t bundle,
                      const scr_char *format, const scr_vartype_t vt_key[])
{
  scr_vartype_t vt_rvalue;
  assert (format[0] == PROP_INTEGER);

  if (!prop_get (bundle, format, &vt_rvalue, vt_key))
    return 0;

  /* Return overloaded integer property value, the child count. */
  return vt_rvalue.integer;
}


/*
 * prop_create_empty()
 *
 * Create a new, empty properties set, and return it.
 */
static scr_prop_setref_t
prop_create_empty (void)
{
  scr_prop_setref_t bundle;

  /* Create a new, empty set.  The set holds std::vector members (P3 RAII),
     so it is non-POD and must be new()'d -- value-init zeroes the POD fields
     and default-constructs the vectors empty. */
  bundle = new scr_prop_set_s ();
  bundle->magic = PROP_MAGIC;
  bundle->node_count = 0;

  /* Leave open for insertions. */
  bundle->is_readonly = FALSE;

  /*
   * Start the set off with a root node.  This will also kick off node pools,
   * ensuring that every set has at least one node and one allocated pool.
   */
  bundle->root_node = prop_new_node (bundle);
  bundle->root_node->child_list = NULL;
  bundle->root_node->name.string = "ROOT";
  bundle->root_node->property.voidp = NULL;
  bundle->root_node->is_sorted = FALSE;
  bundle->root_node->last_found = NULL;

  /* No taf is yet connected with this set. */
  bundle->taf = NULL;

  return bundle;
}


/*
 * prop_destroy_child_list()
 * prop_destroy()
 *
 * Free set memory, and destroy a properties set structure.
 */
static void
prop_destroy_child_list (scr_prop_noderef_t node)
{
  /* End recursion on null or childless node. */
  if (node && node->child_list)
    {
      scr_int index_;

      /* Recursively destroy the children's child lists. */
      for (index_ = 0; index_ < node->property.integer; index_++)
        prop_destroy_child_list (node->child_list[index_]);

      /* Free our own child list. */
      scr_free (node->child_list);
    }
}

void
prop_destroy (scr_prop_setref_t bundle)
{
  size_t index_;
  assert (prop_is_valid (bundle));

  /* Destroy the dictionary strings. */
  for (index_ = 0; index_ < bundle->dictionary.size (); index_++)
    scr_free (bundle->dictionary[index_]);

  /* Free adopted addresses. */
  for (index_ = 0; index_ < bundle->orphans.size (); index_++)
    scr_free (bundle->orphans[index_]);

  /* Walk the tree, destroying the child list for each node found. */
  prop_destroy_child_list (bundle->root_node);
  bundle->root_node = NULL;

  /* Destroy each node pool slab. */
  for (index_ = 0; index_ < bundle->node_pools.size (); index_++)
    scr_free (bundle->node_pools[index_]);

  /* Destroy any taf associated with the bundle. */
  if (bundle->taf)
    taf_destroy (bundle->taf);

  /* Free the bundle; its destructor releases the bookkeeping vectors.  (The
     old 0xaa poison is gone -- it would corrupt the vector members the
     destructor is about to free, the same reason the scr_filter_s struct
     moved to new/delete in P3.) */
  bundle->magic = 0;
  delete bundle;
}


/*
 * prop_create()
 *
 * Create a new properties set based on a taf, and return it.
 */
scr_prop_setref_t
prop_create (const scr_tafref_t taf)
{
  scr_prop_setref_t bundle;

  /* Create a new, empty set. */
  bundle = prop_create_empty ();

  /*
   * Populate it with data parsed from the taf file.  A corrupt game can make
   * the parser throw (scr_fatal) rather than return FALSE; reclaim the
   * partially built bundle on that path too, then let the throw carry on to
   * the interface boundary.  The taf is not yet adopted by the bundle at this
   * point, so its ownership stays with the caller on both failure paths.
   */
  try
    {
      if (!parse_game (taf, bundle))
        {
          prop_destroy (bundle);
          return NULL;
        }
    }
  catch (...)
    {
      prop_destroy (bundle);
      throw;
    }

  /* Note the taf for destruction later, and return the new set. */
  bundle->taf = taf;
  return bundle;
}


/*
 * prop_adopt()
 *
 * Adopt a memory address for free'ing on destroy.
 */
void
prop_adopt (scr_prop_setref_t bundle, void *addr)
{
  assert (prop_is_valid (bundle));

  /* Add the new address to the end of the array.  (The orphans vector is the
     one bookkeeping array that grows past prop_solidify() -- see
     prop_put_string().) */
  bundle->orphans.push_back (addr);
}


/*
 * prop_get_taf()
 *
 * The TAF the properties were parsed from.  The bundle keeps it for the life of
 * the game (prop_destroy is what finally frees it), so a caller that wants the
 * authored strings as they were written -- rather than as the property tree
 * files them -- can read them back out of it.
 */
scr_tafref_t
prop_get_taf (scr_prop_setref_t bundle)
{
  assert (prop_is_valid (bundle));

  return bundle->taf;
}


/*
 * prop_debug_is_dictionary_string()
 * prop_debug_dump_node()
 * prop_debug_dump()
 *
 * Print out a complete properties set.
 */
static scr_bool
prop_debug_is_dictionary_string (scr_prop_setref_t bundle, const void *pointer)
{
  const scr_char *const pointer_ = (const scr_char *) pointer;
  size_t index_;

  /* Compare by pointer directly, not by string value comparisons. */
  for (index_ = 0; index_ < bundle->dictionary.size (); index_++)
    {
      if (bundle->dictionary[index_] == pointer_)
        return TRUE;
    }

  return FALSE;
}

static void
prop_debug_dump_node (scr_prop_setref_t bundle,
                      scr_int depth, scr_int child_index, scr_prop_noderef_t node)
{
  scr_int index_;

  /* Write node preamble, indented two spaces for each depth count. */
  for (index_ = 0; index_ < depth; index_++)
    scr_trace ("  ");
  scr_trace ("%ld : %p", child_index, (void *) node);

  /* Write node, or just a newline if none. */
  if (node)
    {
      /* Print out the node's key, as hex and either string or decimal. */
      scr_trace (", name %p", node->name.voidp);
      if (node != bundle->root_node)
        {
          if (prop_debug_is_dictionary_string (bundle, node->name.string))
            scr_trace (" \"%s\"", node->name.string);
          else
            scr_trace (" %ld", node->name.integer);
        }

      if (node->child_list)
        {
          /* Recursively dump children. */
          scr_trace (", child count %ld\n", node->property.integer);
          for (index_ = 0; index_ < node->property.integer; index_++)
            {
              prop_debug_dump_node (bundle, depth + 1,
                                    index_, node->child_list[index_]);
            }
        }
      else
        {
          /* Print out the node's property, again hex and string or decimal. */
          scr_trace (", property %p", node->property.voidp);
          if (taf_debug_is_taf_string (bundle->taf, node->property.string))
            scr_trace (" \"%s\"\n", node->property.string);
          else
            scr_trace (" %ld\n", node->property.integer);
        }
    }
  else
    scr_trace ("\n");
}

void
prop_debug_dump (scr_prop_setref_t bundle)
{
  scr_int index_;
  assert (prop_is_valid (bundle));

  /* Dump complete structure. */
  scr_trace ("Property: debug dump follows...\n");
  scr_trace ("bundle->is_readonly = %s\n",
            bundle->is_readonly ? "true" : "false");
  scr_trace ("bundle->dictionary_length = %ld\n",
            (scr_int) bundle->dictionary.size ());

  scr_trace ("bundle->dictionary =\n");
  for (index_ = 0; index_ < (scr_int) bundle->dictionary.size (); index_++)
    {
      scr_trace ("%3ld : %p \"%s\"\n", index_,
                bundle->dictionary[index_], bundle->dictionary[index_]);
    }

  scr_trace ("bundle->node_pools_length = %ld\n",
            (scr_int) bundle->node_pools.size ());

  scr_trace ("bundle->node_pools =\n");
  for (index_ = 0; index_ < (scr_int) bundle->node_pools.size (); index_++)
    scr_trace ("%3ld : %p\n", index_, (void *) bundle->node_pools[index_]);

  scr_trace ("bundle->node_count = %ld\n", bundle->node_count);
  scr_trace ("bundle->root_node = {\n");
  prop_debug_dump_node (bundle, 0, 0, bundle->root_node);
  scr_trace ("}\nbundle->taf = %p\n", (void *) bundle->taf);
}


/*
 * prop_debug_trace()
 *
 * Set property tracing on/off.
 */
void
prop_debug_trace (scr_bool flag)
{
  prop_trace = flag;
}
