/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- runtime state (Phase 2).  See a5state.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5restr.h"
#include "a5state.h"
#include "a5util.h"

/* ----------------------------------------------------------------- helpers */

static int
obj_has_prop (const a5_object_t *o, const char *key)
{
  return a5_prop_find (o->props, o->n_props, key) != NULL;
}

static const char *
chr_prop (const a5_character_t *c, const char *key)
{
  const a5_prop_t *p = a5_prop_find (c->props, c->n_props, key);
  return p ? p->value : NULL;
}

/* The runtime property override for (entity, prop), or NULL. */
static const a5_prop_ov_t *
ov_find (const a5_state_t *st, const char *entkey, const char *propkey)
{
  int i;
  for (i = 0; i < st->n_ov; i++)
    if (streq (st->ov[i].entity, entkey) && streq (st->ov[i].prop, propkey))
      return &st->ov[i];
  return NULL;
}

static const a5_group_t *
find_group (const a5_adventure_t *adv, const char *key)
{
  int i;
  for (i = 0; i < adv->n_groups; i++)
    if (streq (adv->groups[i].key, key))
      return &adv->groups[i];
  return NULL;
}

static int
group_has_static_member (const a5_group_t *g, const char *key)
{
  int m;
  for (m = 0; m < g->n_members; m++)
    if (streq (g->members[m], key))
      return 1;
  return 0;
}

/* Decode an object's location property block into an a5_objloc_t. */
static void
compute_objloc (const a5_object_t *o, a5_objloc_t *loc)
{
  const char *sd = obj_prop (o, "StaticOrDynamic");

  loc->where = A5_OWHERE_NONE;
  loc->key = NULL;
  loc->is_static = !streq (sd, "Dynamic");   /* default Static */

  if (!loc->is_static && obj_has_prop (o, "DynamicLocation"))
    {
      const char *dl = obj_prop (o, "DynamicLocation");
      if (streq (dl, "Held By Character"))
        { loc->where = A5_OWHERE_HELD_BY;   loc->key = obj_prop (o, "HeldByWho"); }
      else if (streq (dl, "Hidden"))
        { loc->where = A5_OWHERE_HIDDEN; }
      else if (streq (dl, "In Location"))
        { loc->where = A5_OWHERE_LOCATION;  loc->key = obj_prop (o, "InLocation"); }
      else if (streq (dl, "Inside Object"))
        { loc->where = A5_OWHERE_IN_OBJECT; loc->key = obj_prop (o, "InsideWhat"); }
      else if (streq (dl, "On Object"))
        { loc->where = A5_OWHERE_ON_OBJECT; loc->key = obj_prop (o, "OnWhat"); }
      else if (streq (dl, "Worn By Character"))
        { loc->where = A5_OWHERE_WORN_BY;   loc->key = obj_prop (o, "WornByWho"); }
    }

  /* A holder key may be stored as the literal variable "%Player%" rather than
     the resolved player key (the Adrift 5 runner resolves it to Adventure.Player.Key
     in every accessor, e.g. clsObjectLocation.Key getter, clsObject.vb:1058).
     The engine uses "Player" as the player key throughout, so normalise here —
     otherwise e.g. a backpack with WornByWho=%Player% is dropped from ListWorn. */
  if (streq (loc->key, "%Player%"))
    loc->key = "Player";
  else if (loc->is_static && obj_has_prop (o, "StaticLocation"))
    {
      const char *sl = obj_prop (o, "StaticLocation");
      if (streq (sl, "Nowhere") || streq (sl, "Hidden"))
        { loc->where = A5_OWHERE_HIDDEN; }
      else if (streq (sl, "Single Location"))
        { loc->where = A5_OWHERE_LOCATION;  loc->key = obj_prop (o, "AtLocation"); }
      else if (streq (sl, "Location Group"))
        { loc->where = A5_OWHERE_LOCGROUP;  loc->key = obj_prop (o, "AtLocationGroup"); }
      else if (streq (sl, "Everywhere"))
        { loc->where = A5_OWHERE_ALLROOMS; }
      else if (streq (sl, "Part of Character"))
        { loc->where = A5_OWHERE_PART_CHAR; loc->key = obj_prop (o, "PartOfWho"); }
      else if (streq (sl, "Part of Object"))
        { loc->where = A5_OWHERE_PART_OBJECT; loc->key = obj_prop (o, "PartOfWhat"); }
    }
}

/* Static CharOnWho/CharInsideWho may be the variable "%Player%" rather than a
   concrete character key; the Adrift runner resolves it to Adventure.Player.Key
   in every accessor (clsCharacterLocation.Key).  Resolve it once at load so the
   carrier chain (a5state_character_location_key / _visible_at_location) finds it
   -- otherwise an On-Character follower with CharOnWho=%Player% (Symphonica's
   "Barry Leitch is following you.") never resolves to the player's room and is
   dropped from every location listing.  st->player_key is already decoded when
   this runs (do NOT hardcode "Player": some games rename the player). */
static const char *
resolve_carrier (const a5_state_t *st, const char *k)
{
  return (k != NULL && streq (k, "%Player%")) ? st->player_key : k;
}

/* ------------------------------------------------------------------- public */

a5_state_t *
a5state_new (const a5_adventure_t *adv)
{
  a5_state_t *st;
  int i;

  if (adv == NULL)
    return NULL;
  st = (a5_state_t *) calloc (1, sizeof *st);
  if (st == NULL)
    return NULL;
  st->adv = adv;

  /* The current player: the character whose Type is "Player" (clsAdventure.Player
     getter).  Its key is the literal "Player" in every corpus game, but decode it
     dynamically so a game that renames the player character still works, and so a
     later ToSwitchWith (BECOME) can retarget it. */
  st->player_key = "Player";
  for (i = 0; i < adv->n_characters; i++)
    if (streq (adv->characters[i].type, "Player"))
      { st->player_key = adv->characters[i].key; break; }
  /* clsAdventure.Player getter fallback: a game with no Player-typed
     character promotes the FIRST character in the table to player (ECOD3D
     defines only the NonPlayer "Steve", who becomes the viewpoint). */
  if (i == adv->n_characters && adv->n_characters > 0)
    st->player_key = adv->characters[0].key;

  if (adv->n_objects > 0)
    {
      st->obj = (a5_objloc_t *) calloc ((size_t) adv->n_objects, sizeof *st->obj);
      for (i = 0; i < adv->n_objects; i++)
        compute_objloc (&adv->objects[i], &st->obj[i]);
    }

  if (adv->n_characters > 0)
    {
      st->char_loc = (const char **) calloc ((size_t) adv->n_characters,
                                             sizeof *st->char_loc);
      st->char_position = (char **) calloc ((size_t) adv->n_characters,
                                            sizeof *st->char_position);
      st->char_onobj = (const char **) calloc ((size_t) adv->n_characters,
                                               sizeof *st->char_onobj);
      st->char_onchar = (const char **) calloc ((size_t) adv->n_characters,
                                                sizeof *st->char_onchar);
      st->char_in = (char *) calloc ((size_t) adv->n_characters, 1);
      for (i = 0; i < adv->n_characters; i++)
        {
          const a5_character_t *c = &adv->characters[i];
          const char *cl = chr_prop (c, "CharacterLocation");
          const char *pos = chr_prop (c, "CharacterPosition");
          /* clsCharacterLocation.ExistWhere / Key decode.  "At Location" stores
             the location in char_loc; "On Object"/"In Object" store the carrier
             object in char_onobj (char_in distinguishes inside vs on the surface)
             and leave char_loc NULL -- the effective location is resolved through
             the object (a5state_character_at_location).  "On Character"/"In
             Character" store the carrier character in char_onchar (Edith rides
             the Player: CharacterLocation "On Character", CharOnWho Player) and
             the effective location resolves through the carrier
             (a5state_character_location_key).  "Hidden" stays NULL. */
          if (cl == NULL || streq (cl, "At Location"))
            st->char_loc[i] = chr_prop (c, "CharacterAtLocation");
          else if (streq (cl, "On Object"))
            st->char_onobj[i] = chr_prop (c, "CharOnWhat");
          else if (streq (cl, "In Object"))
            { st->char_onobj[i] = chr_prop (c, "CharInsideWhat");
              st->char_in[i] = 1; }
          else if (streq (cl, "On Character"))
            st->char_onchar[i] = resolve_carrier (st, chr_prop (c, "CharOnWho"));
          else if (streq (cl, "In Character"))
            st->char_onchar[i] = resolve_carrier (st, chr_prop (c, "CharInsideWho"));
          st->char_position[i] = strdup (pos ? pos : "Standing");
        }
      /* FileIO.vb:851-862: after load, if the Player's location is Hidden or has
         an empty key (some modules leave CharacterAtLocation blank), move the
         player to the FIRST location in the adventure.  Only the player is
         re-homed; other characters may legitimately start Hidden. */
      {
        int pi = a5state_character_index (st, st->player_key);
        if (pi >= 0 && adv->n_locations > 0
            && st->char_onobj[pi] == NULL
            && (st->char_loc[pi] == NULL || st->char_loc[pi][0] == '\0'))
          st->char_loc[pi] = adv->locations[0].key;
        /* A player who STARTS on/in furniture (JACD's Timmy lying on Bed1:
           CharacterLocation "On Object") has char_loc NULL here; the runtime
           ToStandingOn/ToSittingOn path keeps char_loc synced to the
           furniture's room (the runner derives clsCharacterLocation.LocationKey from
           the object), so establish the same invariant at load. */
        if (pi >= 0 && st->char_loc[pi] == NULL
            && st->char_onobj[pi] != NULL)
          {
            int li;
            for (li = 0; li < adv->n_locations; li++)
              if (a5state_object_key_at_location (st, st->char_onobj[pi],
                                                  adv->locations[li].key, 0))
                { st->char_loc[pi] = adv->locations[li].key; break; }
          }
      }
      st->char_seen = (char *) calloc ((size_t) adv->n_characters, 1);
      st->char_introduced = (char *) calloc ((size_t) adv->n_characters, 1);
      st->obj_seen = (char *) calloc ((size_t) adv->n_objects, 1);
      /* Per-character stash for the BECOME viewpoint switch (see a5state.h).
         The active arrays above belong to the starting player. */
      st->seen_stash_obj  = (char **) calloc ((size_t) adv->n_characters, sizeof (char *));
      st->seen_stash_char = (char **) calloc ((size_t) adv->n_characters, sizeof (char *));
      st->seen_stash_loc  = (char **) calloc ((size_t) adv->n_characters, sizeof (char *));
    }
  st->seen_active_ci = a5state_character_index (st, st->player_key);

  if (adv->n_locations > 0)
    {
      st->loc_seen = (char *) calloc ((size_t) adv->n_locations, 1);
      /* Nothing is seen yet, not even the room the player starts in: the runner
         marks that one only after the <RunImmediately> tasks have run
         (clsUserSession.vb:371), so a game that starts by moving the player
         elsewhere -- Alien Diver picks its opening ocean square at random --
         leaves the authored start room unseen for good.  a5run_intro does the
         marking; see the call there. */
    }

  st->conv_char = strdup ("");
  st->conv_node = strdup ("");
  st->ctx_char = NULL;

  st->s_it = strdup ("");
  st->s_them = strdup ("");
  st->s_him = strdup ("");
  st->s_her = strdup ("");

  if (adv->n_variables > 0)
    {
      st->var_num = (long *) calloc ((size_t) adv->n_variables, sizeof *st->var_num);
      st->var_text = (char **) calloc ((size_t) adv->n_variables, sizeof *st->var_text);
      st->var_arr = (long **) calloc ((size_t) adv->n_variables, sizeof *st->var_arr);
      for (i = 0; i < adv->n_variables; i++)
        {
          const a5_variable_t *v = &adv->variables[i];
          if (streq (v->type, "Text"))
            st->var_text[i] = v->initial ? strdup (v->initial) : strdup ("");
          else if (v->array_length > 1)
            {
              /* A Numeric array (FileIO.vb:1944): a comma-separated
                 InitialValue fills elements 1..n via SafeInt; a plain value
                 is copied into EVERY element. */
              int n = v->array_length, e;
              st->var_arr[i] = (long *) calloc ((size_t) n, sizeof (long));
              if (v->initial != NULL && strchr (v->initial, ',') != NULL)
                {
                  const char *p = v->initial;
                  for (e = 0; e < n && p != NULL; e++)
                    {
                      st->var_arr[i][e] = strtol (p, NULL, 10);
                      p = strchr (p, ',');
                      if (p != NULL) p++;
                    }
                }
              else
                {
                  long val = v->initial ? strtol (v->initial, NULL, 10) : 0;
                  for (e = 0; e < n; e++)
                    st->var_arr[i][e] = val;
                }
              st->var_num[i] = st->var_arr[i][0];   /* element-1 mirror */
            }
          else
            st->var_num[i] = v->initial ? strtol (v->initial, NULL, 10) : 0;
        }
    }

  if (adv->n_tasks > 0)
    {
      st->task_done   = (char *) calloc ((size_t) adv->n_tasks, 1);
      st->task_scored = (char *) calloc ((size_t) adv->n_tasks, 1);
    }

  /* Seed the live group-membership list (the runner clsGroup.arlMembers) from each
     group's static <Member>s, in model order, so runtime Add/Remove*ToGroup
     layers on top of it and RandomKey/EverywhereInGroup enumerate correctly. */
  for (i = 0; i < adv->n_groups; i++)
    {
      int m;
      for (m = 0; m < adv->groups[i].n_members; m++)
        a5state_group_add_member (st, adv->groups[i].key,
                                  adv->groups[i].members[m]);
    }

  if (adv->n_objects > 0)
    st->obj_prev = (a5_objloc_t *) calloc ((size_t) adv->n_objects,
                                           sizeof *st->obj_prev);
  if (adv->n_characters > 0)
    st->char_prevpar = (const char **) calloc ((size_t) adv->n_characters,
                                               sizeof *st->char_prevpar);
  a5state_stamp_prev (st);   /* the runner's init-time PrepareForNextTurn */

  return st;
}

void
a5state_stamp_prev (a5_state_t *st)
{
  int i;
  if (st->obj_prev != NULL)
    for (i = 0; i < st->adv->n_objects; i++)
      st->obj_prev[i] = st->obj[i];
  if (st->char_prevpar != NULL)
    for (i = 0; i < st->adv->n_characters; i++)
      {
        /* clsCharacter.Parent == Location.Key: the carrier object or
           character when on/in one, else the location. */
        const char *p = st->char_onobj[i];
        if (p == NULL) p = st->char_onchar[i];
        if (p == NULL) p = st->char_loc[i];
        st->char_prevpar[i] = p;
      }
}

void
a5state_free (a5_state_t *st)
{
  int i;
  if (st == NULL)
    return;
  if (st->var_text != NULL)
    for (i = 0; i < st->adv->n_variables; i++)
      free (st->var_text[i]);
  if (st->char_position != NULL)
    for (i = 0; i < st->adv->n_characters; i++)
      free (st->char_position[i]);
  for (i = 0; i < st->n_ov; i++)
    {
      free (st->ov[i].entity);
      free (st->ov[i].prop);
      free (st->ov[i].value);
    }
  free (st->ov);
  for (i = 0; i < st->n_gm; i++)
    { free (st->gm[i].grp); free (st->gm[i].key); }
  free (st->gm);
  free (st->end_message);
  free (st->obj);
  free (st->obj_prev);
  free ((void *) st->char_prevpar);
  free ((void *) st->char_loc);
  free (st->char_position);
  free (st->char_seen);
  free (st->char_introduced);
  free (st->obj_seen);
  free (st->loc_seen);
  for (i = 0; i < st->adv->n_characters; i++)
    {
      if (st->seen_stash_obj  != NULL) free (st->seen_stash_obj[i]);
      if (st->seen_stash_char != NULL) free (st->seen_stash_char[i]);
      if (st->seen_stash_loc  != NULL) free (st->seen_stash_loc[i]);
    }
  free (st->seen_stash_obj);
  free (st->seen_stash_char);
  free (st->seen_stash_loc);
  free (st->conv_char);
  free (st->conv_node);
  free (st->s_it);
  free (st->s_them);
  free (st->s_him);
  free (st->s_her);
  free ((void *) st->char_onobj);
  free (st->char_in);
  if (st->var_arr != NULL)
    for (i = 0; i < st->adv->n_variables; i++)
      free (st->var_arr[i]);
  free (st->var_arr);
  free (st->var_num);
  free (st->var_text);
  free (st->task_done);
  free (st->task_scored);
  free (st->disp_once);
  for (i = 0; i < st->n_looks; i++)
    { free (st->looks[i].loc_key); free (st->looks[i].text);
      free (st->looks[i].event_key); }
  free (st->looks);
  a5restr_route_cache_free (st);
  a5restr_ever_blocked_free (st);
  free (st);
}

/* ----------------------------------------------------- group/location gate */

int
a5state_in_group_or_location (const a5_state_t *st, const char *charkey,
                              const char *key)
{
  const char *cloc;
  int ci;
  if (key == NULL || key[0] == '\0')
    return 0;
  ci = a5state_character_index (st, charkey ? charkey : a5state_player_key (st));
  cloc = (ci >= 0) ? st->char_loc[ci] : NULL;
  if (cloc == NULL)
    return 0;
  /* A location key: the character is at exactly that location. */
  if (a5model_location (st->adv, key) != NULL)
    return streq (cloc, key);
  /* A group key: the character's location is one of its members. */
  {
    const a5_group_t *g = find_group (st->adv, key);
    return g != NULL && group_has_static_member (g, cloc);
  }
}

/* -------------------------------------------------------- SetLook look stack */

void
a5state_push_look (a5_state_t *st, const char *loc_key, const char *text,
                   const char *event_key)
{
  if (st->n_looks == st->cap_looks)
    {
      st->cap_looks = st->cap_looks ? st->cap_looks * 2 : 4;
      st->looks = (a5_looktext_t *)
        realloc (st->looks, (size_t) st->cap_looks * sizeof *st->looks);
    }
  st->looks[st->n_looks].loc_key   = strdup (loc_key ? loc_key : "");
  st->looks[st->n_looks].text      = strdup (text ? text : "");
  st->looks[st->n_looks].event_key = strdup (event_key ? event_key : "");
  st->n_looks++;
}

const char *
a5state_event_look (const a5_state_t *st, const char *event_key)
{
  int i;
  /* clsEvent.LookText: the event's own stack, LIFO -- the most recently
     pushed entry whose gate matches the player wins. */
  for (i = st->n_looks - 1; i >= 0; i--)
    if (streq (st->looks[i].event_key, event_key)
        && a5state_in_group_or_location (st, a5state_player_key (st),
                                         st->looks[i].loc_key))
      return st->looks[i].text;
  return NULL;
}

/* ----------------------------------------------------------- conversation */

void
a5state_set_conv_char (a5_state_t *st, const char *key)
{
  free (st->conv_char);
  st->conv_char = strdup (key ? key : "");
}

void
a5state_set_conv_node (a5_state_t *st, const char *key)
{
  free (st->conv_node);
  st->conv_node = strdup (key ? key : "");
}

/* ------------------------------------------------------------ DisplayOnce */

int
a5state_disp_once_seen (const a5_state_t *st, const void *node)
{
  int i;
  for (i = 0; i < st->n_disp_once; i++)
    if (st->disp_once[i] == node)
      return 1;
  return 0;
}

void
a5state_disp_once_mark (a5_state_t *st, const void *node)
{
  if (node == NULL || a5state_disp_once_seen (st, node))
    return;
  if (st->n_disp_once >= st->cap_disp_once)
    {
      int nc = st->cap_disp_once ? st->cap_disp_once * 2 : 8;
      st->disp_once = (const void **) realloc (st->disp_once,
                                               (size_t) nc * sizeof *st->disp_once);
      st->cap_disp_once = nc;
    }
  st->disp_once[st->n_disp_once++] = node;
}

void
a5state_disp_once_unmark (a5_state_t *st, const void *node)
{
  int i;
  for (i = 0; i < st->n_disp_once; i++)
    if (st->disp_once[i] == node)
      {
        st->disp_once[i] = st->disp_once[--st->n_disp_once];
        return;
      }
}

/* ------------------------------------------------------- reference bindings */

void
a5state_clear_refs (a5_state_t *st)
{
  st->n_refbind = 0;
  st->n_ref_items = 0;
  st->ref_items_type = 'o';
  st->ref_object1_plural = 0;
  st->ref_character1_plural = 0;
  st->ref_objects_suppress_singular = 0;
}

void
a5state_bind_ref (a5_state_t *st, const char *name, const char *value)
{
  int i;
  if (name == NULL || value == NULL)
    return;
  for (i = 0; i < st->n_refbind; i++)
    if (streq (st->ref_name[i], name))
      {
        snprintf (st->ref_value[i], sizeof st->ref_value[i], "%s", value);
        return;
      }
  if (st->n_refbind >= (int) (sizeof st->ref_name / sizeof st->ref_name[0]))
    return;
  snprintf (st->ref_name[st->n_refbind], sizeof st->ref_name[0], "%s", name);
  snprintf (st->ref_value[st->n_refbind], sizeof st->ref_value[0], "%s", value);
  st->n_refbind++;
}

const char *
a5state_lookup_ref (const a5_state_t *st, const char *name)
{
  int i;
  if (name == NULL)
    return NULL;
  for (i = 0; i < st->n_refbind; i++)
    if (streq (st->ref_name[i], name))
      return st->ref_value[i];
  return NULL;
}

/* ------------------------------------------------------- property overrides */

const char *
a5state_entity_prop (const a5_state_t *st, const char *entkey, const char *propkey)
{
  const a5_object_t *o;
  const a5_character_t *c;
  const a5_location_t *l;
  const a5_prop_t *p;
  const a5_prop_ov_t *ov;

  if (entkey == NULL || propkey == NULL)
    return NULL;
  if ((ov = ov_find (st, entkey, propkey)) != NULL)
    return ov->value;

  /* Fall back to the model's static value. */
  o = a5model_object (st->adv, entkey);
  if (o != NULL && (p = a5_prop_find (o->props, o->n_props, propkey)) != NULL)
    return p->value;
  c = a5model_character (st->adv, entkey);
  if (c != NULL && (p = a5_prop_find (c->props, c->n_props, propkey)) != NULL)
    return p->value;
  l = a5model_location (st->adv, entkey);
  if (l != NULL)
    {
      if ((p = a5_prop_find (l->props, l->n_props, propkey)) != NULL)
        return p->value;
      /* Group-inherited location property (clsItem.htblInheritedProperties):
         e.g. The Salvage's "Planet Tiles" group carries daz7LocationIs1 +
         daz7PlanetId, so its member tiles HAVE those properties. */
      if ((p = a5state_location_group_prop (st, entkey, propkey)) != NULL)
        return p->value;
    }
  return NULL;
}

int
a5state_entity_has_prop (const a5_state_t *st, const char *entkey,
                         const char *propkey)
{
  const a5_object_t *o;
  const a5_character_t *c;
  const a5_location_t *l;
  const a5_prop_ov_t *ov;

  if (entkey == NULL || propkey == NULL)
    return 0;
  /* A runtime SetProperty override wins; an `<Unselected>` value means the
     SelectionOnly property was removed (clsUserSession.vb:2058). */
  if ((ov = ov_find (st, entkey, propkey)) != NULL)
    return ov->value == NULL || strstr (ov->value, "Unselected") == NULL;
  o = a5model_object (st->adv, entkey);
  if (o != NULL && a5_prop_find (o->props, o->n_props, propkey) != NULL)
    return 1;
  c = a5model_character (st->adv, entkey);
  if (c != NULL && a5_prop_find (c->props, c->n_props, propkey) != NULL)
    return 1;
  l = a5model_location (st->adv, entkey);
  if (l != NULL)
    {
      if (a5_prop_find (l->props, l->n_props, propkey) != NULL)
        return 1;
      /* Group-inherited location property (see a5state_entity_prop above). */
      if (a5state_location_group_prop (st, entkey, propkey) != NULL)
        return 1;
    }
  return 0;
}

/* Runtime object-group membership (clsGroup add/remove at runtime: the
   AddObjectToGroup / RemoveObjectFromGroup actions, e.g. the flashlight joins
   "LightSources" while switched on).  We layer it onto the property-override
   store under a synthetic key ("@InGroup:<group>"), so it saves/restores for
   free; '@'/':' never occur in real ADRIFT property keys.  Returns the
   effective membership: a runtime override wins, else the model's static
   <Member> list. */
static void
group_prop_key (char *buf, size_t n, const char *grpkey)
{
  snprintf (buf, n, "@InGroup:%s", grpkey ? grpkey : "");
}

int
a5state_object_in_group (const a5_state_t *st, const char *grpkey,
                         const char *objkey)
{
  char key[160];
  const a5_prop_ov_t *ov;
  const a5_group_t *g;
  if (grpkey == NULL || objkey == NULL)
    return 0;
  group_prop_key (key, sizeof key, grpkey);
  if ((ov = ov_find (st, objkey, key)) != NULL)
    return streq (ov->value, "1");
  g = find_group (st->adv, grpkey);
  return g != NULL && group_has_static_member (g, objkey);
}

/* --- Live group membership (the runner clsGroup.arlMembers): ordered + distinct. --- */

static int
gm_find (const a5_state_t *st, const char *grpkey, const char *key)
{
  int i;
  for (i = 0; i < st->n_gm; i++)
    if (streq (st->gm[i].grp, grpkey) && streq (st->gm[i].key, key))
      return i;
  return -1;
}

/* Distinct append (the runner: `If Not arlMembers.Contains(k) Then arlMembers.Add(k)`). */
void
a5state_group_add_member (a5_state_t *st, const char *grpkey, const char *key)
{
  if (grpkey == NULL || key == NULL || gm_find (st, grpkey, key) >= 0)
    return;
  if (st->n_gm >= st->cap_gm)
    {
      int nc = st->cap_gm ? st->cap_gm * 2 : 16;
      st->gm = (a5_grpmem_t *) realloc (st->gm, (size_t) nc * sizeof *st->gm);
      st->cap_gm = nc;
    }
  st->gm[st->n_gm].grp = strdup (grpkey);
  st->gm[st->n_gm].key = strdup (key);
  st->n_gm++;
}

/* Remove (the runner: `If Contains Then Remove`), preserving the order of the rest. */
void
a5state_group_remove_member (a5_state_t *st, const char *grpkey, const char *key)
{
  int i = gm_find (st, grpkey, key);
  if (i < 0)
    return;
  free (st->gm[i].grp);
  free (st->gm[i].key);
  memmove (&st->gm[i], &st->gm[i + 1],
           (size_t) (st->n_gm - i - 1) * sizeof *st->gm);
  st->n_gm--;
}

int
a5state_group_count (const a5_state_t *st, const char *grpkey)
{
  int i, n = 0;
  if (grpkey == NULL)
    return 0;
  for (i = 0; i < st->n_gm; i++)
    if (streq (st->gm[i].grp, grpkey))
      n++;
  return n;
}

const char *
a5state_group_member_at (const a5_state_t *st, const char *grpkey, int idx)
{
  int i, n = 0;
  if (grpkey == NULL)
    return NULL;
  for (i = 0; i < st->n_gm; i++)
    if (streq (st->gm[i].grp, grpkey))
      { if (n == idx) return st->gm[i].key; n++; }
  return NULL;
}

void
a5state_set_object_in_group (a5_state_t *st, const char *grpkey,
                             const char *objkey, int present)
{
  char key[160];
  if (grpkey == NULL || objkey == NULL)
    return;
  group_prop_key (key, sizeof key, grpkey);
  a5state_set_prop (st, objkey, key, present ? "1" : "0");
  /* Keep the live insertion-ordered member list (the runner arlMembers) in sync so
     RandomKey / EverywhereInGroup see runtime add/remove in the right order. */
  if (present)
    a5state_group_add_member (st, grpkey, objkey);
  else
    a5state_group_remove_member (st, grpkey, objkey);
}

/* Rebuild the live insertion-ordered group-membership list (gm / the runner arlMembers)
   from the restored effective membership.  Our save serialises runtime group
   membership only as the @InGroup:<grp> property overrides (PropOv); the ordered
   arlMembers list that RandomKey / EverywhereInGroup enumerate is NOT stored, so
   after a restore it must be reconstructed or those actions see stale (static-
   only) membership.  Seed each group's still-effective static members in model
   order, then append runtime-added members in the order their override was
   created (st->ov append order approximates the runner's arlMembers insertion order). */
void
a5state_rebuild_live_groups (a5_state_t *st)
{
  int i, m;
  if (st == NULL)
    return;
  for (i = 0; i < st->n_gm; i++)
    { free (st->gm[i].grp); free (st->gm[i].key); }
  st->n_gm = 0;
  for (i = 0; i < st->adv->n_groups; i++)
    {
      const a5_group_t *g = &st->adv->groups[i];
      for (m = 0; m < g->n_members; m++)
        if (a5state_object_in_group (st, g->key, g->members[m]))
          a5state_group_add_member (st, g->key, g->members[m]);
    }
  for (i = 0; i < st->n_ov; i++)
    {
      const char *p = st->ov[i].prop;
      if (strncmp (p, "@InGroup:", 9) == 0 && streq (st->ov[i].value, "1"))
        a5state_group_add_member (st, p + 9, st->ov[i].entity);
    }
}

/* A location's inherited group property (clsItem.htblInheritedProperties layered
   from each Locations-type group it belongs to, clsLocation.GetProperty): scan
   every Locations group whose (runtime-or-static) membership includes `lockey`
   and return the last match for `propkey` (the runner's later property-group wins).
   Used for the dynamic ShortLocationDescription darkness property, which the
   day/night events add to every outdoor location's group at night. */
const a5_prop_t *
a5state_location_group_prop (const a5_state_t *st, const char *lockey,
                             const char *propkey)
{
  const a5_prop_t *found = NULL;
  int i;
  if (lockey == NULL || propkey == NULL)
    return NULL;
  for (i = 0; i < st->adv->n_groups; i++)
    {
      const a5_group_t *g = &st->adv->groups[i];
      const a5_prop_t *p;
      if (g->type == NULL || strcmp (g->type, "Locations") != 0 || g->n_props == 0)
        continue;
      if (!a5state_object_in_group (st, g->key, lockey))
        continue;
      p = a5_prop_find (g->props, g->n_props, propkey);
      if (p != NULL)
        found = p;
    }
  return found;
}

void
a5state_set_prop (a5_state_t *st, const char *entkey, const char *propkey,
                  const char *value)
{
  a5_prop_ov_t *ov;
  if (entkey == NULL || propkey == NULL)
    return;
  if ((ov = (a5_prop_ov_t *) ov_find (st, entkey, propkey)) != NULL)
    {
      free (ov->value);
      ov->value = strdup (value ? value : "");
      return;
    }
  if (st->n_ov >= st->cap_ov)
    {
      int nc = st->cap_ov ? st->cap_ov * 2 : 8;
      st->ov = (a5_prop_ov_t *) realloc (st->ov, (size_t) nc * sizeof *st->ov);
      st->cap_ov = nc;
    }
  st->ov[st->n_ov].entity = strdup (entkey);
  st->ov[st->n_ov].prop = strdup (propkey);
  st->ov[st->n_ov].value = strdup (value ? value : "");
  st->n_ov++;
}

int
a5state_object_index (const a5_state_t *st, const char *key)
{
  return a5model_key_index (st->adv, 'O', key);
}

int
a5state_character_index (const a5_state_t *st, const char *key)
{
  return a5model_key_index (st->adv, 'C', key);
}

int
a5state_variable_index (const a5_state_t *st, const char *key)
{
  return a5model_key_index (st->adv, 'V', key);
}

int
a5state_task_index (const a5_state_t *st, const char *key)
{
  return a5model_key_index (st->adv, 'T', key);
}

int
a5state_location_index (const a5_state_t *st, const char *key)
{
  return a5model_key_index (st->adv, 'L', key);
}

void
a5state_mark_loc_seen (a5_state_t *st, const char *lockey)
{
  int li = a5state_location_index (st, lockey);
  if (li >= 0 && st->loc_seen != NULL)
    st->loc_seen[li] = 1;
}

/* Snapshot one active seen array into its stash slot, then load the incoming
   slot back (allocating a zeroed slot the first time a character is switched
   to). */
static void
swap_seen_array (char *active, char ***stash, int old_ci, int new_ci, size_t n)
{
  if (active == NULL || *stash == NULL || n == 0)
    return;
  if (old_ci >= 0)
    {
      if ((*stash)[old_ci] == NULL)
        (*stash)[old_ci] = (char *) malloc (n);
      memcpy ((*stash)[old_ci], active, n);
    }
  if (new_ci >= 0 && (*stash)[new_ci] != NULL)
    memcpy (active, (*stash)[new_ci], n);
  else
    memset (active, 0, n);
}

void
a5state_switch_seen (a5_state_t *st, int new_ci)
{
  int old_ci = st->seen_active_ci;
  if (new_ci == old_ci)
    return;
  swap_seen_array (st->obj_seen,  &st->seen_stash_obj,  old_ci, new_ci,
                   (size_t) st->adv->n_objects);
  swap_seen_array (st->char_seen, &st->seen_stash_char, old_ci, new_ci,
                   (size_t) st->adv->n_characters);
  swap_seen_array (st->loc_seen,  &st->seen_stash_loc,  old_ci, new_ci,
                   (size_t) st->adv->n_locations);
  st->seen_active_ci = new_ci;
}

const char *
a5state_player_key (const a5_state_t *st)
{
  return (st->player_key != NULL) ? st->player_key : "Player";
}

const char *
a5state_player_location (const a5_state_t *st)
{
  int pi = a5state_character_index (st, a5state_player_key (st));
  if (pi < 0 || st->char_loc == NULL)
    return NULL;
  return st->char_loc[pi];
}

static const char *char_location_key_depth (const a5_state_t *st, int ci,
                                            int depth);

/* The FIRST location (model order) where object `oi` exists -- the direct
   equivalent of probing every location through a5state_object_at_location,
   which profiling showed as an O(n_locations x placement-chain) hotspot: a
   character seated On/In furniture resolves its room through this on every
   visibility test of every object it holds or wears.  Each case mirrors the
   corresponding exists_at (visible=0, directly=0) arm below and returns the
   key of the first location that arm would accept. */
static const char *
obj_first_location_key (const a5_state_t *st, int oi, int depth)
{
  const a5_objloc_t *loc;

  if (oi < 0 || depth > 32)
    return NULL;
  loc = &st->obj[oi];

  switch (loc->where)
    {
    case A5_OWHERE_ALLROOMS:
      return st->adv->n_locations > 0 ? st->adv->locations[0].key : NULL;
    case A5_OWHERE_HIDDEN:
    case A5_OWHERE_NONE:
      /* exists_at accepts only the literal lockey "Hidden", so the probe scan
         finds a match just when a location is actually keyed "Hidden". */
      return a5model_key_index (st->adv, 'L', "Hidden") >= 0 ? "Hidden" : NULL;
    case A5_OWHERE_LOCATION:
      return loc->key != NULL
             && a5model_key_index (st->adv, 'L', loc->key) >= 0
             ? loc->key : NULL;
    case A5_OWHERE_LOCGROUP:
      {
        /* Membership is runtime-mutable and per-location; keep the scan. */
        int li;
        for (li = 0; li < st->adv->n_locations; li++)
          if (a5state_object_in_group (st, loc->key,
                                       st->adv->locations[li].key))
            return st->adv->locations[li].key;
        return NULL;
      }
    case A5_OWHERE_IN_OBJECT:
    case A5_OWHERE_ON_OBJECT:
    case A5_OWHERE_PART_OBJECT:
      return obj_first_location_key (st, a5state_object_index (st, loc->key),
                                     depth + 1);
    case A5_OWHERE_HELD_BY:
    case A5_OWHERE_WORN_BY:
    case A5_OWHERE_PART_CHAR:
      {
        int ci = a5state_character_index (st, loc->key);
        const char *k = char_location_key_depth (st, ci, depth + 1);
        return k != NULL && a5model_key_index (st->adv, 'L', k) >= 0
               ? k : NULL;
      }
    }
  return NULL;
}

/* A character's EFFECTIVE location key: char_loc when set, else resolved
   through the on/in-object carrier (a char whose model start is "On Object"
   -- GFS's Grandpa on his rocking chair -- has char_loc NULL; The runner derives
   clsCharacterLocation.LocationKey from the furniture). */
static const char *
char_location_key_depth (const a5_state_t *st, int ci, int depth)
{
  if (ci < 0 || st->char_loc == NULL || depth > 32)
    return NULL;
  if (st->char_loc[ci] != NULL)
    return st->char_loc[ci];
  if (st->char_onobj != NULL && st->char_onobj[ci] != NULL)
    {
      const char *k = obj_first_location_key (st,
          a5state_object_index (st, st->char_onobj[ci]), depth + 1);
      if (k != NULL)
        return k;
    }
  /* A character riding another character (ExistWhere On/InCharacter -- Edith on
     the Player) inherits the carrier's effective room; the runner derives
     clsCharacterLocation.LocationKey from the carrier.  Recurse with a depth
     guard against a cyclic On-Character placement. */
  if (st->char_onchar != NULL && st->char_onchar[ci] != NULL)
    return char_location_key_depth (st,
             a5state_character_index (st, st->char_onchar[ci]), depth + 1);
  return NULL;
}

const char *
a5state_character_location_key (const a5_state_t *st, int ci)
{
  return char_location_key_depth (st, ci, 0);
}

/* Does container object `parent` *hide* its In-Object contents from view?
   Mirrors clsObject.BoundVisible's InObject branch (clsObject.vb:804): the
   contents are visible when the container is `Not Openable OrElse IsOpen OrElse
   IsTransparent`, so it hides only when it is openable AND closed AND opaque.
   Openable = HasProperty("Openable"); IsOpen = the OpenStatus property is absent
   or "Open"; IsTransparent is always False in the runner (clsObject.vb:308).  Consults
   the runtime override layer first (so a SetProperty OpenStatus is reflected). */
static int
obj_hides_contents (const a5_state_t *st, int parent)
{
  const char *pkey, *os;

  if (parent < 0)
    return 0;
  pkey = st->adv->objects[parent].key;

  /* Openable = HasProperty("Openable"), runtime override winning. */
  if (!a5state_entity_has_prop (st, pkey, "Openable"))
    return 0;

  /* IsOpen: OpenStatus absent (Nothing) or "Open" => open => visible. */
  os = a5state_entity_prop (st, pkey, "OpenStatus");
  if (os == NULL || streq (os, "Open"))
    return 0;

  return 1;                     /* openable, closed, opaque => hides contents */
}

/* Recursive ExistsAtLocation, with a depth guard against cyclic placements.
   When `visible` is set this mirrors clsObject.BoundVisible / IsVisibleAtLocation
   instead of the raw ExistsAtLocation: an object inside a *closed opaque*
   openable container resolves to the container's own key (not the room), so it
   is not visible at any location.  ExistsAtLocation (visible=0) ignores that. */
static int
exists_at (const a5_state_t *st, int oi, const char *lockey, int directly,
           int visible, int depth)
{
  const a5_objloc_t *loc;
  int parent;

  if (oi < 0 || depth > 32)
    return 0;
  loc = &st->obj[oi];

  switch (loc->where)
    {
    case A5_OWHERE_ALLROOMS:
      return 1;
    case A5_OWHERE_HIDDEN:
    case A5_OWHERE_NONE:
      return streq (lockey, "Hidden");
    case A5_OWHERE_LOCATION:
      return streq (loc->key, lockey);
    case A5_OWHERE_LOCGROUP:
      /* Runtime AddLocationToGroup counts (GFS's green button lives in a
         statically-empty group the `x door` task populates), so go through
         the override-aware membership test, not the static <Member>s. */
      return a5state_object_in_group (st, loc->key, lockey);
    case A5_OWHERE_IN_OBJECT:
      if (directly)
        return 0;
      parent = a5state_object_index (st, loc->key);
      /* BoundVisible of a closed opaque container is the container key, so the
         contents are not visible at any room. */
      if (visible && obj_hides_contents (st, parent))
        return streq (lockey, loc->key);
      return exists_at (st, parent, lockey, 0, visible, depth + 1);
    case A5_OWHERE_ON_OBJECT:
    case A5_OWHERE_PART_OBJECT:
      if (directly)
        return 0;
      parent = a5state_object_index (st, loc->key);
      return exists_at (st, parent, lockey, 0, visible, depth + 1);
    case A5_OWHERE_HELD_BY:
    case A5_OWHERE_WORN_BY:
    case A5_OWHERE_PART_CHAR:
      {
        int ci;
        if (directly)
          return 0;
        ci = a5state_character_index (st, loc->key);
        if (ci < 0)
          return 0;
        /* Resolve through the carrier chain, not char_loc alone: a character
           seated On/Inside furniture has char_loc[ci] == NULL and its effective
           room comes from the furniture (a5state_character_location_key).  Held
           and worn objects must inherit that same effective room, or they drop
           out of scope / room listings while their holder is plainly present. */
        return streq (a5state_character_location_key (st, ci), lockey);
      }
    }
  return 0;
}

int
a5state_object_at_location (const a5_state_t *st, int oi, const char *lockey,
                            int directly)
{
  return exists_at (st, oi, lockey, directly, 0, 0);
}

/* Is `lockey` one of object `oi`'s location ROOTS (clsObject.LocationRoots,
   clsObject.vb:460)?  Differs from ExistsAtLocation in the held/worn/part-of-
   character branches: the runner only roots those at the carrier's location
   when the character is strictly AtLocation -- a carrier seated On/In furniture
   or riding another character contributes NO root, so %LocationOf[obj]% renders
   empty for its cargo. */
static int
location_root_at (const a5_state_t *st, int oi, const char *lockey, int depth)
{
  const a5_objloc_t *loc;

  if (oi < 0 || depth > 32)
    return 0;
  loc = &st->obj[oi];

  switch (loc->where)
    {
    case A5_OWHERE_ALLROOMS:
      return 1;
    case A5_OWHERE_HIDDEN:
    case A5_OWHERE_NONE:
      return 0;
    case A5_OWHERE_LOCATION:
      return streq (loc->key, lockey);
    case A5_OWHERE_LOCGROUP:
      return a5state_object_in_group (st, loc->key, lockey);
    case A5_OWHERE_IN_OBJECT:
    case A5_OWHERE_ON_OBJECT:
    case A5_OWHERE_PART_OBJECT:
      return location_root_at (st, a5state_object_index (st, loc->key),
                               lockey, depth + 1);
    case A5_OWHERE_HELD_BY:
    case A5_OWHERE_WORN_BY:
    case A5_OWHERE_PART_CHAR:
      {
        int ci = a5state_character_index (st, loc->key);
        if (ci < 0 || st->char_loc == NULL || st->char_loc[ci] == NULL)
          return 0;
        /* AtLocation only (clsObject.vb:472/509): no root through a seated
           or riding carrier. */
        if ((st->char_onobj != NULL && st->char_onobj[ci] != NULL)
            || (st->char_onchar != NULL && st->char_onchar[ci] != NULL))
          return 0;
        return streq (st->char_loc[ci], lockey);
      }
    }
  return 0;
}

int
a5state_object_location_root (const a5_state_t *st, int oi, const char *lockey)
{
  return location_root_at (st, oi, lockey, 0);
}

int
a5state_object_key_at_location (const a5_state_t *st, const char *objkey,
                                const char *lockey, int directly)
{
  return exists_at (st, a5state_object_index (st, objkey), lockey, directly, 0, 0);
}

int
a5state_object_visible_at_location (const a5_state_t *st, int oi,
                                    const char *lockey, int directly)
{
  return exists_at (st, oi, lockey, directly, 1, 0);
}

int
a5state_character_at_location (const a5_state_t *st, int ci, const char *lockey)
{
  if (ci < 0 || lockey == NULL || st->char_loc == NULL)
    return 0;
  /* "At Location": directly compare the recorded location key. */
  if (st->char_loc[ci] != NULL)
    return streq (st->char_loc[ci], lockey);
  /* "On Object"/"In Object": present wherever the carrier object is (its
     container chain reaches lockey).  Visibility (the closed-opaque-container
     hiding) is a5state_character_visible_at_location's job. */
  if (st->char_onobj != NULL && st->char_onobj[ci] != NULL)
    return a5state_object_key_at_location (st, st->char_onobj[ci], lockey, 0);
  /* "On Character"/"In Character": present wherever the carrier character is
     (Edith rides the Player).  a5state_character_location_key resolves the
     carrier chain with its own depth guard. */
  if (st->char_onchar != NULL && st->char_onchar[ci] != NULL)
    { const char *k = a5state_character_location_key (st, ci);
      return k != NULL && streq (k, lockey); }
  return 0;
}

int
a5state_character_visible_at_location (const a5_state_t *st, int ci,
                                       const char *lockey)
{
  if (ci < 0 || lockey == NULL || st->char_loc == NULL)
    return 0;
  /* On/In an object: the runner never consults the character's own location for
     this -- clsCharacter.BoundVisible (clsCharacter.vb:711) hands straight off
     to the CONTAINER's BoundVisible, so the room is derived live.  That has to
     beat the char_loc short-circuit below, because char_loc is only a snapshot
     taken when the character was put in the object (the sync in
     ExecuteSingleAction, which mirrors clsCharacterLocation.LocationKey's
     sLastLocationKey stickiness) and goes stale the moment the container is
     carried elsewhere: Jabberwocky's Beast, sealed in the wine bottle in the
     chapel, is "here" in every room you carry the bottle into. */
  if (st->char_onobj != NULL && st->char_onobj[ci] != NULL)
    {
      int oi = a5state_object_index (st, st->char_onobj[ci]);
      /* A character inside an openable, closed, opaque container binds to the
         container's own key instead, so it is visible at no room (Halloween's
         Dracula asleep in the closed coffin, October 31st's casket, get no "is
         here" line).  On-object characters inherit the carrier's BoundVisible
         unconditionally. */
      if (st->char_in != NULL && st->char_in[ci] && obj_hides_contents (st, oi))
        return 0;
      return exists_at (st, oi, lockey, 0, 1, 0);
    }
  if (st->char_loc[ci] != NULL)
    return streq (st->char_loc[ci], lockey);
  /* "On Character"/"In Character": a rider inherits the carrier's BoundVisible
     unconditionally (Edith on the Player is visible wherever the Player is). */
  if (st->char_onchar != NULL && st->char_onchar[ci] != NULL)
    { int cc = a5state_character_index (st, st->char_onchar[ci]);
      return cc >= 0 && cc != ci
             && a5state_character_visible_at_location (st, cc, lockey); }
  return 0;
}

long
a5state_var_get_elem (const a5_state_t *st, int vi, long idx)
{
  if (vi < 0 || vi >= st->adv->n_variables)
    return 0;
  if (st->var_arr == NULL || st->var_arr[vi] == NULL)
    return st->var_num[vi];                    /* scalar: index ignored */
  if (idx < 1 || idx > st->adv->variables[vi].array_length)
    return 0;                                  /* clsVariable ErrMsg -> 0 */
  return st->var_arr[vi][idx - 1];
}

void
a5state_var_set_elem (a5_state_t *st, int vi, long idx, long value)
{
  if (vi < 0 || vi >= st->adv->n_variables)
    return;
  if (st->var_arr == NULL || st->var_arr[vi] == NULL)
    { st->var_num[vi] = value; return; }       /* scalar: index ignored */
  if (idx < 1 || idx > st->adv->variables[vi].array_length)
    return;                                    /* clsVariable ErrMsg -> drop */
  st->var_arr[vi][idx - 1] = value;
  if (idx == 1)
    st->var_num[vi] = value;                   /* element-1 mirror */
}

static int
var_index_by_name (const a5_state_t *st, const char *name)
{
  int i;
  for (i = 0; i < st->adv->n_variables; i++)
    if (streq (st->adv->variables[i].name, name))
      return i;
  return -1;
}

int
a5state_var_num_by_name (const a5_state_t *st, const char *name, long *out)
{
  int i = var_index_by_name (st, name);
  if (i < 0)
    return 0;
  if (out != NULL)
    *out = st->var_num[i];
  return 1;
}

const char *
a5state_var_text_by_name (const a5_state_t *st, const char *name)
{
  int i = var_index_by_name (st, name);
  return (i >= 0) ? st->var_text[i] : NULL;
}
