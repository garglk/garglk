/* vi: set ts=8:
 *
 * Copyright (C) 2003-2005  Simon Baldwin and Mark J. Tilford
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 */

/*
 * Module notes:
 *
 * o Move property and key type indicators ('I', 'B', ...) into a header
 *   file.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "scare.h"
#include "scprotos.h"


/* Assorted definitions and constants. */
static const sc_uint32	PROP_MAGIC		= 0x7927B2E0;
static const sc_uint32	PROP_GROW_INCREMENT	= 32;
static const sc_int32	MAX_INTEGER_KEY		= 65535;
static const sc_uint32	NODE_POOL_CAPACITY	= 512;
static const sc_char	NUL			= '\0';

/* Properties trace flag. */
static sc_bool		prop_trace		= FALSE;

/* Property and key type indicators. */
enum {			PROP_INTEGER		= 'I',
			PROP_BOOLEAN		= 'B',
			PROP_STRING		= 'S' };
enum {			PROP_KEY_INTEGER	= 'i',
			PROP_KEY_STRING		= 's' };

/*
 * Property tree node definition, uses a child list representation for
 * fast lookup on indexed nodes.  Name is a variable type, as is property,
 * which is also overloaded to contain the child count for internal nodes.
 */
typedef struct sc_prop_node_s {
	sc_vartype_t			name;
	sc_vartype_t			property;

	struct sc_prop_node_s		**child_list;
} sc_prop_node_t;
typedef	sc_prop_node_t*	sc_prop_noderef_t;

/*
 * Properties set structure.  This is a set of properties, on which the
 * properties functions operate (a properties "object").  Node string
 * names are held in a dictionary to help save space.  To avoid excessive
 * malloc'ing of nodes, new nodes are preallocated in pools.
 */
typedef struct sc_prop_set_s {
	sc_uint32			magic;
	sc_uint32			dictionary_length;
	sc_char				**dictionary;
	sc_uint32			node_pools_length;
	sc_prop_noderef_t		*node_pools;
	sc_uint32			node_count;
	sc_uint32			orphans_length;
	void				**orphans;
	sc_bool				is_readonly;
	sc_prop_noderef_t		root_node;
	sc_tafref_t			taf;
} sc_prop_set_t;


/*
 * prop_round_up()
 *
 * Round up a count of elements to the next block of grow increments.
 */
static sc_uint32
prop_round_up (sc_uint32 elements)
{
	return ((elements + PROP_GROW_INCREMENT - 1) / PROP_GROW_INCREMENT)
		* PROP_GROW_INCREMENT;
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
prop_ensure_capacity (void *array, sc_uint32 old_size, sc_uint32 new_size,
		sc_uint32 element_size)
{
	sc_uint32	current, required;
	assert (new_size >= old_size);

	/* Compute the current element count, and required element count. */
	current  = prop_round_up (old_size);
	required = prop_round_up (new_size);

	/*
	 * See if there's any resize necessary, that is, does the new
	 * size round up to a larger number of elements than the old
	 * size.
	 */
	if (required > current)
	    {
		void		*new_array;

		/* Grow array to the required size, and zero new elements. */
		new_array = sc_realloc (array, required * element_size);
		memset ((sc_byte *) new_array + current * element_size, 0,
				(required - current) * element_size);

		/* Return the new array address. */
		return new_array;
	    }

	/* No grow needed. */
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
prop_trim_capacity (void *array, sc_uint32 size, sc_uint32 element_size)
{
	/* Any spare allocation available to trim? */
	if (prop_round_up (size) > size)
		return sc_realloc (array, size * element_size);

	/* Nothing to trim. */
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
	return strcmp (*(sc_char **)string1, *(sc_char **)string2);
}


/*
 * prop_dictionary_lookup()
 *
 * Find a string in the dictionary.  If the string is not present, the
 * function will add it.  The function returns the string's address, if
 * either added or already present.  Any new dictionary entry will
 * contain a malloced copy of the string passed in.
 */
static sc_char *
prop_dictionary_lookup (sc_prop_setref_t bundle, sc_char *string)
{
	sc_char		**dict_search;
	sc_char		*dict_string;

	/*
	 * Search the existing dictionary for the string.  Although not
	 * GNU libc, some libc's loop or crash when given a list of zero
	 * length, so we need to trap that here.
	 */
	if (bundle->dictionary_length > 0)
	    {
		dict_search = bsearch (&string, bundle->dictionary,
					bundle->dictionary_length,
					sizeof (bundle->dictionary[0]),
					prop_compare);
		if (dict_search != NULL)
			return *dict_search;
	    }

	/* Not found, so copy the string for dictionary insertion. */
	dict_string = sc_malloc (strlen (string) + 1);
	strcpy (dict_string, string);

	/* Extend the dictionary if necessary. */
	bundle->dictionary = prop_ensure_capacity (bundle->dictionary,
					bundle->dictionary_length,
					bundle->dictionary_length + 1,
					sizeof (bundle->dictionary[0]));

	/* Add the new entry to the end of the dictionary array, and sort. */
	bundle->dictionary[bundle->dictionary_length++] = dict_string;
	qsort (bundle->dictionary, bundle->dictionary_length,
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
static sc_prop_noderef_t
prop_new_node (sc_prop_setref_t bundle)
{
	sc_uint32		node_index;
	sc_prop_noderef_t	node;

	/* See if we need to create a new node pool. */
	node_index = bundle->node_count % NODE_POOL_CAPACITY;
	if (node_index == 0)
	    {
		sc_uint32	required;

		/* Extend the node pools array if necessary. */
		bundle->node_pools = prop_ensure_capacity (bundle->node_pools,
					bundle->node_pools_length,
					bundle->node_pools_length + 1,
					sizeof (bundle->node_pools[0]));

		/* Create a new node pool, and increment the length. */
		required = NODE_POOL_CAPACITY * sizeof (*bundle->node_pools[0]);
		bundle->node_pools[bundle->node_pools_length]
							= sc_malloc (required);
		bundle->node_pools_length++;
	    }

	/* Find the next node address, and increment the node counter. */
	node = bundle->node_pools[bundle->node_pools_length - 1] + node_index;
	bundle->node_count++;

	/* Return the new node. */
	return node;
}


/*
 * prop_find_child()
 *
 * Find a child node of the given parent whose name matches that passed in.
 */
static sc_prop_noderef_t
prop_find_child (sc_prop_noderef_t parent, sc_char type, sc_vartype_t name)
{
	/* See if this node has any children. */
	if (parent->child_list != NULL)
	    {
		sc_int32	index;

		/* Do the lookup based on name type. */
		switch (type)
		    {
		    case PROP_KEY_INTEGER:
			/*
			 * As with adding a child below, here we'll range-check
			 * an integer key just to make sure nobody has any
			 * unreal expectations of us.
			 */
			if (name.integer < 0)
			    {
				sc_fatal ("prop_find_child: integer key"
						" cannot be negative\n");
			    }
			else if (name.integer > MAX_INTEGER_KEY)
			    {
				sc_fatal ("prop_find_child: integer key"
						" is too large, max %ld\n",
						MAX_INTEGER_KEY);
			    }

			/*
			 * For integer lookups, return the child at the
			 * indexed offset directly, provided it exists.
			 */
			if (name.integer >= 0
				&& name.integer < parent->property.integer)
			    {
				sc_prop_noderef_t	child;

				child = parent->child_list[name.integer];
				return child;
			    }
			break;

		    case PROP_KEY_STRING:
			/* Scan children for a string name match. */
			for (index = 0;
				index < parent->property.integer; index++)
			    {
				sc_prop_noderef_t	child;

				child = parent->child_list[index];
				if (strcmp (child->name.string,
							name.string) == 0)
				    {
					/*
					 * Before returning the child, try to
					 * improve future scans by moving the
					 * matched entry to index 0 -- this
					 * gives a key set sorted by recent
					 * usage, helpful as the same string
					 * key is used repeatedly in loops.
					 */
					if (index > 0)
					    {
						memmove (parent->child_list + 1,
							parent->child_list,
							index * sizeof (child));
						parent->child_list[0] = child;
					    }
					return child;
				    }
			    }
			break;

		    default:
			sc_fatal ("prop_find_child: invalid key type\n");
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
static sc_prop_noderef_t
prop_add_child (sc_prop_noderef_t parent, sc_char type, sc_vartype_t name,
		sc_prop_setref_t bundle)
{
	sc_prop_noderef_t	child;

	/* Not possible if growable allocations have been trimmed. */
	if (bundle->is_readonly)
	    {
		sc_fatal ("prop_add_child: can't add to"
						" readonly properties\n");
	    }

	/* Create the new node. */
	child = prop_new_node (bundle);
	switch (type)
	    {
	    case PROP_KEY_INTEGER:
		child->name.integer = name.integer;
		break;

	    case PROP_KEY_STRING:
		child->name.string =
				prop_dictionary_lookup (bundle, name.string);
		break;

	    default:
		sc_fatal ("prop_add_child: invalid key type\n");
	    }

	/* Initialize property and child list to visible nulls. */
	child->property.voidp	= NULL;
	child->child_list	= NULL;

	/* Make a brief check for obvious overwrites. */
	if (parent->child_list == NULL
			&& parent->property.voidp != NULL)
	    {
		sc_error ("prop_add_child: probable data loss\n");
	    }

	/* Add the child to the parent, position dependent on key type. */
	switch (type)
	    {
	    case PROP_KEY_INTEGER:
		/*
		 * Range check on integer keys, must be >= 0 for direct
		 * indexing to work, and we'll also apply a reasonableness
		 * constraint, to try to catch errors where string pointers
		 * are passed in as integers, which would otherwise lead to
		 * some extreme malloc() attempts.
		 */
		if (name.integer < 0)
		    {
			sc_fatal ("prop_add_child: integer key"
						" cannot be negative\n");
		    }
		else if (name.integer > MAX_INTEGER_KEY)
		    {
			sc_fatal ("prop_add_child: integer key"
						" is too large, max %ld\n",
						MAX_INTEGER_KEY);
		    }

		/* Resize the parent's child list if necessary. */
		parent->child_list = prop_ensure_capacity (parent->child_list,
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
		parent->child_list = prop_ensure_capacity (parent->child_list,
					parent->property.integer,
					parent->property.integer + 1,
					sizeof (*parent->child_list));

		/* Store the child at the end of the list. */
		parent->child_list[parent->property.integer++] = child;
		break;

	    default:
		sc_fatal ("prop_add_child: invalid key type\n");
	    }

	return child;
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
prop_put (sc_prop_setref_t bundle, sc_char *format,
		sc_vartype_t vt_value, sc_vartype_t vt_key[])
{
	sc_prop_noderef_t	node;
	sc_uint16		index, k_index;
	assert (bundle != NULL && bundle->magic == PROP_MAGIC);

	/* Format check. */
	if (format == NULL || format[0] == NUL
			|| format[1] != '-' || format[2] != '>'
			|| format[3] == NUL)
	    {
		sc_fatal ("prop_put: format error\n");
	    }

	/* Trace property put. */
	if (prop_trace)
	    {
		sc_trace ("Property: put ");
		switch (format[0])
		    {
		    case PROP_STRING:
			sc_trace ("\"%s\"", vt_value.string);
			break;
		    case PROP_INTEGER:
			sc_trace ("%ld", vt_value.integer);
			break;
		    case PROP_BOOLEAN:
			sc_trace ("%s", vt_value.boolean ? "TRUE" : "FALSE");
			break;
		    }
		sc_trace (", key \"%s\" : ", format);
		for (index = 3, k_index = 0;
				index < strlen (format); index++, k_index++)
		    {
			if (k_index != 0)
				sc_trace (",");
			switch (format[index])
			    {
			    case PROP_KEY_STRING:
				sc_trace ("\"%s\"", vt_key[k_index].string);
				break;
			    case PROP_KEY_INTEGER:
				sc_trace ("%ld", vt_key[k_index].integer);
				break;
			    }
		    }
		sc_trace ("\n");
	    }

	/* Run along the keys finding or inserting nodes. */
	node = bundle->root_node;
	for (index = 3, k_index = 0;
			format[index] != NUL; index++, k_index++)
	    {
		sc_prop_noderef_t	child;

		/*
		 * Search this level for a matching name to the key.  If not
		 * found, then add the node to the tree, including the set
		 * so that the dictionary can be extended.
		 */
		child = prop_find_child (node,
					format[index], vt_key[k_index]);
		if (child == NULL)
		    {
			child = prop_add_child (node,
					format[index], vt_key[k_index],
					bundle);
		    }

		/* Move on to child node. */
		node = child;
	    }

	/*
	 * Ensure that we're not about to overwrite an internal node
	 * child count.
	 */
	if (node->child_list != NULL)
	    {
		sc_fatal ("prop_put: overwrite of internal node\n");
	    }

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
		sc_fatal ("prop_put: invalid property type\n");
	    }
}


/*
 * prop_get()
 *
 * Retrieve a property from a properties set.  Format stuff as above, except
 * with "->" replaced with "<-".  Returns FALSE if no such property exists.
 */
sc_bool
prop_get (sc_prop_setref_t bundle, sc_char *format,
		sc_vartype_t *vt_rvalue, sc_vartype_t vt_key[])
{
	sc_prop_noderef_t	node;
	sc_uint16		index, k_index;
	assert (bundle != NULL && bundle->magic == PROP_MAGIC);

	/* Format check. */
	if (format == NULL || format[0] == NUL
			|| format[1] != '<' || format[2] != '-'
			|| format[3] == NUL)
	    {
		sc_fatal ("prop_get: format error\n");
	    }

	/* Trace property get. */
	if (prop_trace)
	    {
		sc_trace ("Property: get, key \"%s\" : ", format);
		for (index = 3, k_index = 0;
				index < strlen (format); index++, k_index++)
		    {
			if (k_index != 0)
				sc_trace (",");
			switch (format[index])
			    {
			    case PROP_KEY_STRING:
				sc_trace ("\"%s\"", vt_key[k_index].string);
				break;
			    case PROP_KEY_INTEGER:
				sc_trace ("%ld", vt_key[k_index].integer);
				break;
			    }
		    }
		sc_trace ("\n");
	    }

	/* Run along the keys finding matching nodes. */
	node = bundle->root_node;
	for (index = 3, k_index = 0;
			format[index] != NUL; index++, k_index++)
	    {
		sc_prop_noderef_t	child;

		/*
		 * Search this level for a matching name to the key.  If not
		 * found, return FALSE.
		 */
		child = prop_find_child (node,
					format[index], vt_key[k_index]);
		if (child == NULL)
		    {
			if (prop_trace)
				sc_trace ("Property: ...get FAILED\n");
			return FALSE;
		    }

		/* Move on to child node. */
		node = child;
	    }

	/*
	 * Enforce integer-only queries on internal nodes, since this is
	 * the only type of query that makes any sense -- any other type is
	 * probably a mistake.
	 */
	if (node->child_list != NULL
			&& format[0] != PROP_INTEGER)
	    {
		sc_fatal ("prop_get: only integer gets on internal nodes\n");
	    }

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
		sc_fatal ("prop_get: invalid property type\n");
	    }

	/* Complete tracing property get. */
	if (prop_trace)
	    {
		sc_trace ("Property: ...get returned : ");
		switch (format[0])
		    {
		    case PROP_STRING:
			sc_trace ("\"%s\"", vt_rvalue->string);
			break;
		    case PROP_INTEGER:
			sc_trace ("%ld", vt_rvalue->integer);
			break;
		    case PROP_BOOLEAN:
			sc_trace ("%s", vt_rvalue->boolean ? "TRUE" : "FALSE");
			break;
		    }
		sc_trace ("\n");
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
prop_trim_node (sc_prop_noderef_t node)
{
	/* End recursion on null node. */
	if (node != NULL)
	    {
		/* Do nothing if not an internal node. */
		if (node->child_list != NULL)
		    {
			sc_int32	index;

			/* Recursively trim allocation on children. */
			for (index = 0; index < node->property.integer; index++)
			    {
				prop_trim_node (node->child_list[index]);
			    }

			/* Trim allocation on this node. */
			node->child_list = prop_trim_capacity
				(node->child_list, node->property.integer,
						sizeof (*node->child_list));
		    }
	    }
}

void
prop_solidify (sc_prop_setref_t bundle)
{
	assert (bundle != NULL && bundle->magic == PROP_MAGIC);

	/*
	 * Trim back the dictionary, orphans, pools array, and every
	 * internal tree node.  The one thing _not_ to trim is the final
	 * node pool -- there are references to nodes within it strewn
	 * all over the properties tree, and it's a large job to try to
	 * find and update them; instead, we just live with a little
	 * wasted heap memory.
	 */
	bundle->dictionary = prop_trim_capacity (bundle->dictionary,
					bundle->dictionary_length,
					sizeof (bundle->dictionary[0]));
	bundle->node_pools = prop_trim_capacity (bundle->node_pools,
					bundle->node_pools_length,
					sizeof (bundle->node_pools[0]));
	bundle->orphans = prop_trim_capacity (bundle->orphans,
					bundle->orphans_length,
					sizeof (bundle->orphans[0]));
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
sc_int32
prop_get_integer (sc_prop_setref_t bundle, sc_char *format,
		sc_vartype_t vt_key[])
{
	sc_vartype_t	vt_rvalue;
	assert (format[0] == PROP_INTEGER);

	if (!prop_get (bundle, format, &vt_rvalue, vt_key))
		sc_fatal ("prop_get_integer: can't retrieve property\n");
	return vt_rvalue.integer;
}

sc_bool
prop_get_boolean (sc_prop_setref_t bundle, sc_char *format,
		sc_vartype_t vt_key[])
{
	sc_vartype_t	vt_rvalue;
	assert (format[0] == PROP_BOOLEAN);

	if (!prop_get (bundle, format, &vt_rvalue, vt_key))
		sc_fatal ("prop_get_boolean: can't retrieve property\n");
	return vt_rvalue.boolean;
}

sc_char *
prop_get_string (sc_prop_setref_t bundle, sc_char *format,
		sc_vartype_t vt_key[])
{
	sc_vartype_t	vt_rvalue;
	assert (format[0] == PROP_STRING);

	if (!prop_get (bundle, format, &vt_rvalue, vt_key))
		sc_fatal ("prop_get_string: can't retrieve property\n");
	return vt_rvalue.string;
}


/*
 * prop_create_empty()
 *
 * Create a new, empty properties set, and return it.
 */
static sc_prop_setref_t
prop_create_empty (void)
{
	sc_prop_setref_t	bundle;

	/* Create a new, empty set. */
	bundle = sc_malloc (sizeof (*bundle));
	bundle->magic = PROP_MAGIC;

	/* Begin with an empty strings dictionary. */
	bundle->dictionary_length		= 0;
	bundle->dictionary			= NULL;

	/* Begin with no allocated node pools. */
	bundle->node_pools_length		= 0;
	bundle->node_pools			= NULL;
	bundle->node_count			= 0;

	/* Begin with no adopted addresses. */
	bundle->orphans_length			= 0;
	bundle->orphans				= NULL;

	/* Leave open for insertions. */
	bundle->is_readonly			= FALSE;

	/*
	 * Start the set off with a root node.  This will also kick off
	 * node pools, ensuring that every set has at least one node and
	 * one allocated pool.
	 */
	bundle->root_node = prop_new_node (bundle);
	bundle->root_node->child_list		= NULL;
	bundle->root_node->name.string		= "ROOT";
	bundle->root_node->property.voidp	= NULL;

	/* No taf is yet connected with this set. */
	bundle->taf				= NULL;

	return bundle;
}


/*
 * prop_destroy_child_list()
 * prop_destroy()
 *
 * Free set memory, and destroy a properties set structure.
 */
static void
prop_destroy_child_list (sc_prop_noderef_t node)
{
	/* End recursion on null node. */
	if (node != NULL)
	    {
		/* If an internal node, handle children. */
		if (node->child_list != NULL)
		    {
			sc_int32	index;

			/* Recursively destroy the children's child lists. */
			for (index = 0; index < node->property.integer; index++)
			    {
				prop_destroy_child_list
						(node->child_list[index]);
			    }

			/* Free our own child list. */
			sc_free (node->child_list);
		    }
	    }
}

void
prop_destroy (sc_prop_setref_t bundle)
{
	sc_uint32	index;
	assert (bundle != NULL && bundle->magic == PROP_MAGIC);

	/* Destroy the dictionary, and free it. */
	for (index = 0; index < bundle->dictionary_length; index++)
		sc_free (bundle->dictionary[index]);
	bundle->dictionary_length	= 0;
	sc_free (bundle->dictionary);
	bundle->dictionary		= NULL;

	/* Free adopted addresses. */
	for (index = 0; index < bundle->orphans_length; index++)
		sc_free (bundle->orphans[index]);
	bundle->orphans_length		= 0;
	sc_free (bundle->orphans);
	bundle->orphans			= NULL;

	/* Walk the tree, destroying the child list for each node found. */
	prop_destroy_child_list (bundle->root_node);
	bundle->root_node		= NULL;

	/* Destroy each node pool. */
	for (index = 0; index < bundle->node_pools_length; index++)
		sc_free (bundle->node_pools[index]);
	bundle->node_pools_length	= 0;
	sc_free (bundle->node_pools);
	bundle->node_pools		= NULL;

	/* Destroy any taf associated with the bundle. */
	if (bundle->taf != NULL)
		taf_destroy (bundle->taf);

	/* Shred and free the bundle. */
	memset (bundle, 0, sizeof (*bundle));
	sc_free (bundle);
}


/*
 * prop_create()
 *
 * Create a new properties set based on a taf, and return it.
 */
sc_prop_setref_t
prop_create (sc_tafref_t taf)
{
	sc_prop_setref_t	bundle;

	/* Create a new, empty set. */
	bundle = prop_create_empty ();

	/* Populate it with data parsed from the taf file. */
	if (!parse_game (taf, bundle))
	    {
		prop_destroy (bundle);
		return NULL;
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
prop_adopt (sc_prop_setref_t bundle, void *addr)
{
	assert (bundle != NULL && bundle->magic == PROP_MAGIC);

	/* Extend the orphans array if necessary. */
	bundle->orphans = prop_ensure_capacity (bundle->orphans,
					bundle->orphans_length,
					bundle->orphans_length + 1,
					sizeof (bundle->orphans[0]));

	/* Add the new address to the end of the array. */
	bundle->orphans[bundle->orphans_length++] = addr;
}


/*
 * prop_debug_dump_node()
 * prop_debug_dump()
 *
 * Print out a complete properties set.
 */
static void
prop_debug_dump_node (sc_prop_noderef_t node)
{
	/* End recursion on null node. */
	if (node != NULL)
	   {
		sc_int32	index;

		/* Print out this node, virtually unformatted. */
		sc_trace ("->node %p, name %p",
				(void *) node, (void *) node->name.string);
		if (node->child_list != NULL)
		    {
			sc_trace (", child count = %ld",
						node->property.integer);
			for (index = 0; index < node->property.integer; index++)
			    {
				if (index % 5 == 0)
					sc_trace ("\n");
				sc_trace ("%14p",
					(void *) node->child_list[index]);
			    }
			sc_trace ("\n");

			/* Recursively dump children. */
			for (index = 0; index < node->property.integer; index++)
				prop_debug_dump_node
						(node->child_list[index]);
		    }
		else
		    {
			sc_trace (", property %p (%ld)\n",
					(void *) node->property.string,
					node->property.integer);
		    }
	   }
}

void
prop_debug_dump (sc_prop_setref_t bundle)
{
	sc_uint32			index;
	assert (bundle != NULL && bundle->magic == PROP_MAGIC);

	/* Dump complete structure. */
	sc_trace ("bundle->is_readonly = %s\n",
					bundle->is_readonly ? "TRUE" : "FALSE");
	sc_trace ("bundle->dictionary_length = %ld\n",
					bundle->dictionary_length);
	sc_trace ("bundle->dictionary =\n");
	for (index = 0; index < bundle->dictionary_length; index++)
	    {
		sc_trace ("%3ld : %p \"%s\"\n", index,
					bundle->dictionary[index],
					bundle->dictionary[index]);
	    }
	sc_trace ("bundle->node_pools_length = %ld\n",
					bundle->node_pools_length);
	sc_trace ("bundle->node_pools =\n");
	for (index = 0; index < bundle->node_pools_length; index++)
	    {
		sc_trace ("%3ld : %p\n", index,
					(void *) bundle->node_pools[index]);
	    }
	sc_trace ("bundle->node_count = %ld\n", bundle->node_count);
	sc_trace ("bundle->root_node =\n");
	prop_debug_dump_node (bundle->root_node);
	sc_trace ("bundle->taf = %p\n", (void *) bundle->taf);
}


/*
 * prop_debug_trace()
 *
 * Set property tracing on/off.
 */
void
prop_debug_trace (sc_bool flag)
{
	prop_trace = flag;
}
