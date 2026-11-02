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
 * Carrying capacity, "count", and taking objects: the backend.  The command
 * forms that run it ("take all", "take X and Y", the multiple-object
 * handler) are in sclibrar_take_multiple.cpp.
 *
 * See sclibrar.h for what the library files share, sclibrar_internal.h for
 * what the core files share and sclibrar_take_internal.h for what the two
 * take files share.
 */

#include "sclibrar_take_internal.h"

/*
 * lib_carried_burden()
 *
 * Return the total burden of everything the player is currently holding, under
 * the version 3.8 pooled model (see obj_get_burden()).
 *
 * Worn objects are free.  Wearing something does not spend any of MaxCarried,
 * and neither does keeping it on -- measured with a MaxCarried 2 probe holding
 * a wearable and two others: run370 and run380 both accept all three, and
 * "count" reports the two held (pworn/qworn, 2026-08-23).  Taking anything off
 * therefore costs a slot again.
 *
 * Always computed afresh, and that is faithful: unlike 4.0's running size and
 * weight totals, the pooled burden does not drift.  A container is not charged
 * for its contents and wearing is not a charge at all, so neither of the two
 * shapes of 4.0 leak has anything to leak here -- confirmed against both
 * Runners with the drop-a-loaded-container and wear-then-drop sequences
 * (pleakc/pleakw and their 3.70 twins).  This is why "glk capacity" is a no-op
 * for a 3.7/3.8 game: there is no Runner arithmetic to escape.  It is a no-op
 * for a 3.9 game too, for the same reason by a different route -- run390 keeps
 * a running total but keeps it exactly; see obj_uses_running_load().
 */
static scr_int
lib_carried_burden (scr_gameref_t game)
{
  scr_int index_, burden = 0;

  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (gs_object_position (game, index_) == OBJ_HELD_PLAYER)
        burden += obj_get_burden (game, index_);
    }

  return burden;
}


/*
 * lib_carried_size()
 * lib_carried_weight()
 *
 * The player's current carried size and weight as the capacity checks see
 * them.  For a 4.0 game this is by default the Runner's running total
 * (gs_carried_*, with its take/drop double-count); otherwise, and in legacy
 * mode, it is recomputed afresh from what is currently held or worn.
 *
 * Only 4.0 keeps a running size worth reproducing: run390 adjusts the same
 * two globals but does so exactly, so recomputing is what a 3.9 game's size
 * looks like from the outside -- see obj_uses_running_load() for the probe
 * that separates the two.  Its weight is another matter, because the total
 * starts out short of a held container's contents and stays short; see
 * obj_uses_running_weight().  Legacy mode also drops the Runner's stale parent rule
 * from obj_weigh(), so it is free of the phantom weight too; a 3.9 game gets
 * that for the same reason, having no running total to poison.
 */
static scr_int
lib_sum_carried (scr_gameref_t game,
                 scr_int (*measure) (scr_gameref_t, scr_int))
{
  scr_int index_, total;

  total = 0;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (gs_object_position (game, index_) == OBJ_HELD_PLAYER
          || gs_object_position (game, index_) == OBJ_WORN_PLAYER)
        total += measure (game, index_);
    }

  return total;
}

static scr_int
lib_carried_size (scr_gameref_t game)
{
  if (obj_uses_running_load (game) && !game->capacity_recompute)
    return gs_carried_size (game);

  return lib_sum_carried (game, obj_get_size);
}

static scr_int
lib_carried_weight (scr_gameref_t game)
{
  if (obj_uses_running_weight (game) && !game->capacity_recompute)
    return gs_carried_weight (game);

  return lib_sum_carried (game, obj_get_weight);
}


/*
 * lib_cmd_count()
 *
 * Display player weight and size limits and amounts currently carried.
 */
scr_bool
lib_cmd_count (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int size, weight;

  /*
   * A version 3.8 game has neither of these axes, and its "count" is a single
   * unlabelled line counting objects rather than the two tab-stopped Size and
   * Weight lines below: "You have 0 objects.  The most you can hold is 2."
   * There is no singular -- one object still reads "1 objects" -- and the
   * first person is "I have ... The most I can hold is ...".  Measured on
   * run380 with the pcount/pcount1 probes, 2026-08-23.
   */
  if (obj_uses_burden_model (game))
    {
      pf_buffer_string (filter,
                        lib_select_response (game, "You have ", "I have ",
                                             "%player% have "));
      pf_buffer_integer (filter, lib_carried_burden (game));
      pf_buffer_string (filter, " objects");
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             ".  The most you can hold is ",
                                             ".  The most I can hold is ",
                                             ".  The most %player_pronoun% can hold is "));
      pf_buffer_integer (filter, obj_get_player_burden_limit (game));
      pf_buffer_string (filter, ".\n");

      lib_set_admin (game);
      return TRUE;
    }

  /* Report the same carried totals the capacity checks use. */
  size = lib_carried_size (game);
  weight = lib_carried_weight (game);

  /*
   * Print the player limits and amounts used, in the Runner's own layout --
   * a tab stop after each label (two tabs for the shorter "Size:") and the
   * game's perspective for the pronouns: "Size:\t\tI have 0.  The most I
   * can hold is 108." (run400, goldilocks/iachini transcripts 2026-08-22).
   */
  pf_buffer_string (filter, "Size:\t\t");
  pf_buffer_string (filter,
                    lib_select_response (game, "You have ", "I have ",
                                         "%player% have "));
  pf_buffer_integer (filter, size);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         ".  The most you can hold is ",
                                         ".  The most I can hold is ",
                                         ".  The most %player_pronoun% can hold is "));
  pf_buffer_integer (filter, obj_get_player_size_limit (game));
  pf_buffer_string (filter, ".\n");

  pf_buffer_string (filter, "Weight:\t");
  pf_buffer_string (filter,
                    lib_select_response (game, "You have ", "I have ",
                                         "%player% have "));
  pf_buffer_integer (filter, weight);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         ".  The most you can hold is ",
                                         ".  The most I can hold is ",
                                         ".  The most %player_pronoun% can hold is "));
  pf_buffer_integer (filter, obj_get_player_weight_limit (game));
  pf_buffer_string (filter, ".\n");

  lib_set_admin (game);
  return TRUE;
}


/*
 * lib_object_too_heavy()
 *
 * Return TRUE if the given object is too heavy for the player to carry.
 */
scr_bool
lib_object_too_heavy (scr_gameref_t game, scr_int object)
{
  scr_int player_limit, weight, object_weight;

  /*
   * Version 3.8 has no weight axis, and no "too heavy" refusal to go with one
   * -- its single pooled burden is checked in lib_object_too_large() below,
   * and answers "Your hands are full." to everything it refuses.
   */
  if (obj_uses_burden_model (game))
    return FALSE;

  /* Get the player limit and the given object weight. */
  player_limit = obj_get_player_weight_limit (game);
  object_weight = obj_get_weight (game, object);

  /* Establish the player's current carried weight. */
  weight = lib_carried_weight (game);

  /* Return TRUE if the new object exceeds limit. */
  return weight + object_weight > player_limit;
}


/*
 * lib_print_too_heavy()
 *
 * Print the weight refusal for one object, into a clause the caller has
 * already opened.
 *
 * The whole sentence is a 4.0 rewording.  run390 answers a fixed "That is too
 * heavy for you to carry." -- it names no object, and it never qualifies the
 * refusal.  run400 names the object and always qualifies it: "The lump is too
 * heavy for Player to carry at the moment."  Both were measured on the p39wt
 * probe (2026-08-23), one Runner each, against a brick that fits in empty
 * hands and a lump that does not: each Runner gave its single shape for both,
 * so the "at the moment" is not the portability hedge SCARE took it for.
 * Neither Runner has a plural form -- there is no "are too heavy" string in
 * either binary.
 *
 * A 3.7 or 3.8 game never reaches here: the pooled burden has no weight axis
 * and lib_object_too_heavy() stands down for it.
 */
void
lib_print_too_heavy (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
    {
      pf_buffer_string (filter,
                        lib_select_response
                          (game,
                           "That is too heavy for you to carry.",
                           "That is too heavy for me to carry.",
                           "That is too heavy for %player% to carry."));
      return;
    }

  lib_print_object_np (game, object);
  pf_buffer_string (filter,
                    lib_select_response
                      (game,
                       " is too heavy for you to carry at the moment.",
                       " is too heavy for me to carry at the moment.",
                       " is too heavy for %player% to carry at the moment."));
}


/*
 * lib_object_too_large()
 *
 * Return TRUE if the given object is too large for the player to carry.
 */
scr_bool
lib_object_too_large (scr_gameref_t game, scr_int object)
{
  scr_int player_limit, size, object_size;

  /*
   * Version 3.8's pooled burden against its plain MaxCarried limit, which
   * replaces both 4.0 axes outright: every class costs at least 1, so the
   * burden is never looser than the object count the size axis would have
   * enforced against normalised 3.8 objects.
   */
  if (obj_uses_burden_model (game))
    {
      scr_int object_burden = obj_get_burden (game, object);

      player_limit = obj_get_player_burden_limit (game);

      return lib_carried_burden (game) + object_burden > player_limit;
    }

  /* Get the player limit and the given object size. */
  player_limit = obj_get_player_size_limit (game);
  object_size = obj_get_size (game, object);

  /* Establish the player's current carried size. */
  size = lib_carried_size (game);

  /* Return TRUE if the new object exceeds limit. */
  return size + object_size > player_limit;
}


/*
 * lib_take_over_capacity()
 *
 * Return TRUE if taking the object would put the player over a carrying
 * limit, setting *is_size to say which refusal applies.
 *
 * 4.0's per-object take (run400 get_piece_inner 46302C) tests size first --
 * the object's own size (44) on top of the running total, "<Your> hands are
 * full." at 462ECD -- and only then weight, recursive over contents (447680),
 * "<The X> is too heavy..." at 462F62.  It tests both whatever holds the
 * object, a container the player carries included: the contents are already
 * in both running totals, so a take out of a carried bag counts them twice.
 * wilkins `take times` at 99/90 on both axes answers "My hands are full."
 * (runner_transcripts/wilkins.txt line 84), businessasusual `take all` with
 * limits 9/9 and every object 9/9 takes one and refuses the next by size, and
 * provenance `get rope` out of the carried rucksack is refused by weight.
 *
 * Earlier versions test weight first and skip objects inside or on something
 * the player already holds.
 */
scr_bool
lib_take_over_capacity (scr_gameref_t game, scr_int object, scr_bool *is_size)
{
  if (lib_is_version_400 (game))
    {
      *is_size = lib_object_too_large (game, object);
      return *is_size || lib_object_too_heavy (game, object);
    }

  if ((gs_object_position (game, object) == OBJ_IN_OBJECT
       || gs_object_position (game, object) == OBJ_ON_OBJECT)
      && obj_indirectly_held_by_player (game, gs_object_parent (game, object)))
    return FALSE;

  *is_size = FALSE;
  if (lib_object_too_heavy (game, object))
    return TRUE;
  *is_size = lib_object_too_large (game, object);
  return *is_size;
}


/*
 * lib_take_from_over_capacity_390()
 *
 * 3.9's take-from has its own capacity test, in insides() rather than takes().
 * It tests size first (4638C8) and weight second (4638DE).  The weight test
 * is waived when the container itself is held by the player (container
 * [22] = 0); the size test never is.  ALEXIS `get all from large stone table`
 * refuses the size-81 knife by size: "You can't take any more, as your hands
 * are full."  (runner_transcripts/alexis.txt:119.)
 */
static scr_bool
lib_take_from_over_capacity_390 (scr_gameref_t game, scr_int object,
                                 scr_int container, scr_bool *is_size)
{
  *is_size = lib_object_too_large (game, object);
  if (*is_size)
    return TRUE;
  return gs_object_position (game, container) != OBJ_HELD_PLAYER
         && lib_object_too_heavy (game, object);
}


/*
 * lib_print_take_npc_refusal()
 *
 * The take-a-character refusal, for lib_cmd_take_npc() and the 4.0 overwrite
 * in lib_take_multiple_common().
 */
void
lib_print_take_npc_refusal (scr_gameref_t game, scr_int npc)
{
  /*
   * Reject this attempt.  The Runner names the NPC by its Prefix and first
   * Alias here, not its Name: run400 47F750-47F7BC and run390 45969B-4596C6
   * print "I don't think " & [Prefix & " "] & Alias(0) when the alias is
   * set (the prefix only when it is too), and fall back to the Name when it
   * is empty; run380 44057D and run370 4386EE always print Prefix & " " &
   * Alias(0), empty or not.  House (4.00), Cathy alias "girl", no prefix:
   * "I don't think girl would appreciate being handled."
   * (runner_probes/house.run400.girl.txt, 2026-09-06).
   */
  {
    const scr_filterref_t filter = gs_get_filter (game);
    const scr_prop_setref_t bundle = gs_get_bundle (game);
    const scr_bool is_390_plus =
        prop_get_taf_version (bundle) >= TAF_VERSION_390;
    const scr_char *prefix, *alias = nullptr;
    scr_vartype_t vt_key[4];

    prefix = prop_get_indexed_string (bundle, "NPCs", npc, "Prefix");
    alias = lib_first_alias (bundle, vt_key, "NPCs", npc);
    if (!prefix)
      prefix = "";
    if (!alias)
      alias = "";

    pf_buffer_string (filter, "I don't think ");
    if (!is_390_plus)
      {
        pf_buffer_string (filter, prefix);
        pf_buffer_string (filter, " ");
        pf_buffer_string (filter, alias);
      }
    else if (alias[0] != NUL)
      {
        if (prefix[0] != NUL)
          {
            pf_buffer_string (filter, prefix);
            pf_buffer_string (filter, " ");
          }
        pf_buffer_string (filter, alias);
      }
    else
      lib_print_npc_np (game, npc);
    pf_buffer_string (filter, " would appreciate being handled.\n");
  }
}

/*
 * lib_take_npc_overwrite()
 *
 * The present character an object take line also names, or -1.  The same
 * characters() pass as lib_examine_npc_overwrite_400() has a take arm
 * (run400 47F70B-47F7BC, run390 459658-4596C6, run380 44054B-44057D,
 * run370 4386BC-4386EE: whole-word take/get/pick up and the NPC in the room)
 * that ASSIGNS the refusal to the message buffer without testing it, so the
 * object's take answer is thrown away while the take itself stands.  All
 * four start the sentence with `MemVar = "I don't think "` -- run400 47F753,
 * run390 45966D, and the single-expression assignments at run380 44057D and
 * run370 4386EE -- so every version clobbers.  (An earlier reading of the
 * run390 listing, where the decompiler hangs the literal off the Prefix test
 * at 45969B and leaves the alias-free arm at 4596BB looking like an append,
 * said 3.9 appended and was not ported; the pushes at 45966A say otherwise
 * and so does the Runner.)
 *
 * The "no task ran" halves differ by VB precedence and are not modelled:
 * 3.9/4.0 gate all three verbs on it, 3.7 gates none, and 3.8 gates only
 * "pick up" (`c("take") Or c("get") Or c("pick up") And notask`).  Nothing
 * reaches here with a task already run for the line.
 *
 * Measured on ONNAFA (4.00): `get key of pure harry` with Red Harry (alias
 * Harry) present answers only "I don't think Harry would appreciate being
 * handled." (runner_transcripts/onnafa.txt:1622), and the key is used later.
 * The pre-4.0 halves are p3xORD (make_orderprobe.py): `take hat bob`, `take
 * coin bob`, `get hat bob`, `pick up hat bob`, `take bob hat` and `take box
 * bob` all answer only the refusal and all still take the object
 * (runner_probes/ord.run*.chr.*).
 */
scr_int
lib_take_npc_overwrite (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();

  if (!input
      || !(lib_input_contains_word (input, "take")
           || lib_input_contains_word (input, "get")
           || lib_input_contains_word (input, "pick up")))
    return -1;
  return lib_examine_npc_overwrite_400 (game);
}

/*
 * lib_take_from_npc_overwrite_380()
 *
 * The 3.8 take-from arm, the LAST text arm of characters() (441093-4412AB,
 * just above the walk ticker), so it overwrites everything above it --
 * including the take refusal that lib_take_npc_overwrite() just printed and
 * including the ask topic.  It is entered on whole-word get or take with the
 * character in the player's room, and then walks the WHOLE object table:
 * every object whose Short or first Alias the line names by c() ASSIGNS a
 * sentence, so the highest-index match speaks.  There is no scope test of
 * any kind and, unlike its 3.9 twin at 45A27C, no c("from") gate on the "is
 * not carrying" halves -- that gate is exactly what 3.9 added.
 *
 * An object the character itself holds or wears is "<Name> refuses to give
 * you <the object>!" (4411AA); anything else is "<Name> is not carrying <the
 * object>!" (441227 for an object inside something else, 44128A for a loose
 * one).  3.7 has no such arm at all.
 *
 * Measured on p38ORD (runner_probes/ord.run380.chr.rtf): `take hat bob` and
 * `get hat bob` are "Bob is not carrying the hat!" while the hat is loose on
 * the floor and still gets taken, `take nut bob` names the nut shut inside
 * the box, `take hat coin bob` speaks for the coin (the higher index), `take
 * ask bob about hat` loses the topic reply to it, and `pick up hat bob` keeps
 * the take refusal because "pick up" is not one of this arm's two verbs.
 * 3.7's same feed answers the take refusal throughout
 * (runner_probes/ord.run370.chr.rtf).
 *
 * TRUE once it has spoken, having truncated the buffer back to mark first.
 */
scr_bool
lib_take_from_npc_overwrite_380 (scr_gameref_t game, scr_int npc, size_t mark)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int object, found = -1;

  if (!input
      || prop_get_taf_version (bundle) != TAF_VERSION_380
      || !(lib_input_contains_word (input, "get")
           || lib_input_contains_word (input, "take")))
    return FALSE;

  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_vartype_t vt_key[4];
      const scr_char *word;

      word = prop_get_indexed_string (bundle, "Objects", object, "Short");
      if (!scr_strempty (word) && lib_input_contains_word (input, word))
        {
          found = object;
          continue;
        }
      word = lib_first_alias (bundle, vt_key, "Objects", object);
      if (word && !scr_strempty (word)
          && lib_input_contains_word (input, word))
        found = object;
    }

  if (found == -1)
    return FALSE;

  pf_truncate (filter, mark);
  lib_print_npc_np (game, npc);
  if ((gs_object_position (game, found) == OBJ_HELD_NPC
       || gs_object_position (game, found) == OBJ_WORN_NPC)
      && gs_object_parent (game, found) == npc)
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           " refuses to give you ",
                                           " refuses to give me ",
                                           " refuses to give %player% "));
  else
    pf_buffer_string (filter, " is not carrying ");
  lib_print_object_np (game, found);
  pf_buffer_string (filter, "!\n");
  return TRUE;
}

/*
 * lib_cmd_take_npc()
 *
 * Reject attempts to take an npc.
 */
scr_bool
lib_cmd_take_npc (scr_gameref_t game)
{
  scr_int npc;
  scr_bool is_ambiguous;

  /*
   * 3.7/3.8: the last character the line names answers, here or not; see
   * lib_print_npc_not_here_pre390().  `take guard` with Ann and Bob here and
   * Cora next door is "Cora is not here!" (run370x
   * runner_probes/npcamb.run370.b.rtf, run380x
   * runner_probes/npcamb.run380.rtf).
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    {
      npc = lib_last_named_npc (game);
      if (npc == -1)
        return FALSE;
      if (!lib_print_npc_not_here_pre390 (game, npc))
        {
          const size_t mark = pf_buffer_length (gs_get_filter (game));

          var_set_ref_character (gs_get_vars (game), npc);
          lib_print_take_npc_refusal (game, npc);
          /* 3.8's take-from arm speaks below it; it names an object the
             take resolver never saw, as `take nut bob` does for a nut shut
             inside a CLOSED box (runner_probes/ord.run380.chr.rtf). */
          lib_take_from_npc_overwrite_380 (game, npc, mark);
        }
      return TRUE;
    }

  /* Get the referenced npc, and if none, consider complete.  3.9 fills an
     empty buffer, so the FIRST present character answers
     (runner_probes/npcamb.run390.txt). */
  npc = lib_disambiguate_npc_pick (game, "take", &is_ambiguous,
                                   NPC_PICK_FIRST);
  if (npc == -1)
    return is_ambiguous;

  lib_print_take_npc_refusal (game, npc);
  return TRUE;
}


/*
 * lib_take_backend_common()
 *
 * Common backend handler for taking objects.  Takes all objects currently
 * referenced in the game, trying game commands first, and then moving other
 * unhandled objects to the player inventory.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */

/* Set when the take command named exactly one object; cleared by the
   backend.  Selects the single-take "already carrying" refusal wording. */
scr_bool lib_take_single_named = FALSE;
scr_bool lib_take_refusal_claimed = FALSE;
/* Set when a 4.0 take line was split on "and" into pieces the take
   resolver named one by one; see lib_take_and_400().  Each piece gets the
   single-take wording ("already carrying", "can't take X!"). */
scr_bool lib_take_and_pieces_400 = FALSE;

/*
 * lib_take_refusal_redispatch_400()
 *
 * A 4.0 take refusal does not claim its line.  The "can't take X!" path of
 * run400's per-piece take (Proc_19_23_473A34 @47329D) leaves by ExitProcI2
 * at @4733FC without setting the handler's result, so get_outer
 * (Proc_19_22_4582D8) does not GoTo loc_48B4E3 and generaltasks' task
 * dispatcher at 48A481 gets the line, its text joined onto the refusal with
 * the two-space separator.  The line it matches is the typed one with the
 * take verbs rewritten, "remove "/"pick "/"take " -> "get " (get_outer's
 * Replace chain, @458127-458176), never a rebuild from the resolved object.
 * Professor.taf with the Mailbox on-a Rope static, measured in run400 on
 * 2026-09-14 (runner_probes/professor.run400.mail2.txt,
 * runner_probes/professor.run400.profmail3.txt,
 * runner_probes/professor.run400.mail4.txt,
 * runner_probes/professor.run400.mail5.txt):
 *
 *   `take mailbox`, `take the mailbox` -> "You can't take the Mailbox on-a
 *     Rope!  The mailbox is already down." (task 8's
 *     `[check/look/get]{in/the}[mailbox]{on}{a}{rope}`), or its CompleteText
 *     while the mailbox is still up;
 *   `pick up mailbox` ("get up mailbox"), `take rope` ("get rope") and `take
 *     mailbox on-a rope` (the hyphen misses {on}{a}) -> the refusal alone.
 *
 * It is the one place run400 runs a second task on a typed line: `take
 * mailbox` has already run task 7 `[take/get/pick up]...` silently, its
 * type-1 restriction on the referenced object failing with no message.  A
 * rewrite that leaves the line unchanged offers nothing the task passes did
 * not already see, so it is skipped.
 */
scr_bool lib_take_refusal_redispatch = FALSE;

void
lib_take_refusal_redispatch_400 (scr_gameref_t game)
{
  static const scr_char *const verbs[] = {"remove ", "pick ", "take "};
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *typed = run_get_dispatch_input ();
  std::string line;

  if (!typed)
    return;
  line = typed;
  for (const scr_char *verb : verbs)
    {
      const size_t length = strlen (verb);
      size_t at = 0;

      while ((at = line.find (verb, at)) != std::string::npos)
        {
          line.replace (at, length, "get ");
          at += 4;
        }
    }
  if (line == typed)
    return;

  /*
   * The dispatcher sees no referenced object: generaltasks cleared it at the
   * top of the command (48A004) and the refusal path never binds one, so
   * task 7's type-1 restriction on "the referenced object" fails silently
   * and task 8 answers, where the library's resolved mailbox would have
   * printed task 7's "The mailbox isn't down here right now."
   */
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_int ref_object = var_get_ref_object (vars);
  const scr_int ref_character = var_get_ref_character (vars);

  var_set_ref_object (vars, -1);
  var_set_ref_character (vars, -1);
  pf_note_trailing_auto_break (filter);
  pf_buffer_join_pending (filter);
  run_game_task_commands (game, line.c_str ());
  pf_clear_join_pending (filter);
  var_set_ref_object (vars, ref_object);
  var_set_ref_character (vars, ref_character);
}

/* Set when a "take from <object>" command named exactly one object; cleared
   by the backend.  Only that single-named form echoes the taken object's
   raw prefix in pre-4.0 games (see the wording comment in the backend). */
scr_bool lib_take_from_single_named = FALSE;

/*
 * lib_take_from_verb()
 *
 * "You take " is a 3.80 rewording.  run370 spells the from-container take
 * "You get ", and the census is unambiguous about it: the literal " take "
 * is in run380, run390 and run400 and in none of run370, while " get " is
 * in all four, and run370 carries "You can't get anything from that." where
 * 380 and 390 carry that and " can't take anything from " both.
 *
 * Measured on the same turn of two probes, `get coin from box` with the box
 * held and the coin inside it: run370 answers "You get a coin from the box."
 * (p37DARK, runner_probes/dark.run370.putin.txt:27) and run380 "You take a
 * coin from the box." (p38DARK, runner_probes/dark.run380.putin.txt:27),
 * 2026-09-12.  The typed verb was "get" on both, so this is the handler's own
 * wording and not an echo of the input.
 *
 * The bare-verb refusal in lib_take_from_unseen_refusal() prints the same
 * phrase from the same place in the Runner and moves with it; that half is
 * the census's, not a measurement.
 */
const scr_char *
lib_take_from_verb (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380)
    return lib_select_response (game,
                                "You get ", "I get ", "%player% get ");

  return lib_select_response (game,
                              "You take ", "I take ", "%player% take ");
}


/*
 * lib_take_container_unheld()
 * lib_print_not_holding()
 *
 * The pre-3.9 hold gate on the container or surface an object is taken
 * from.  run380's takes() (loc_43E47B) rewrites a plain "take X" whose X sits
 * in or on a present object into "take X from <parent>", and its insides()
 * from-branch (loc_446CBB..446CFB) then insists that a DYNAMIC parent is held
 * (position 0) or worn (&H9C) by the player, else "<You> <are> not holding
 * <raw prefix> <short>." -- before the closed test, before any game task is
 * tried for "get X from Y".  A static parent only has to be in the room.
 * run370's insides() (43B118, "not holding" at 439D33/43AB5E) is the same;
 * run390 has no "not holding" on any take path and run400 none at all, so
 * from 3.90 on a contained object is taken wherever its parent is.
 *
 * Measured on jb2000.taf (James Bond - Happy Landings, 3.80) in run380,
 * 2026-09-04: `take key card` with the card inside a jacket lying in the
 * room answers "You are not holding a brown jacket.", and `take learner`
 * from a suitcase on the floor "You are not holding a brown suitcase.";
 * `take jacket` first, and the card comes out with "You take the cockpit
 * key card from the brown jacket."  The prefix is the author's, untensed
 * (var_41C(0) & " " & var_41C(4)), hence lib_print_object.  Held means
 * held directly: a container nested inside a carried one has position
 * &HF6, not 0, and is refused like any other.
 */
scr_bool
lib_take_container_unheld (scr_gameref_t game, scr_int container)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return FALSE;
  if (obj_is_static (game, container))
    return FALSE;

  return !(gs_object_position (game, container) == OBJ_HELD_PLAYER
           || gs_object_position (game, container) == OBJ_WORN_PLAYER);
}

void
lib_print_not_holding (scr_gameref_t game, scr_int container,
                       const scr_char *suffix)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You are not holding ",
                                         "I am not holding ",
                                         "%player% is not holding "));
  lib_print_object (game, container);
  pf_buffer_string (filter, suffix);
}

/*
 * lib_cannot_reach_container()
 *
 * Pre-4.0 refuses to reach into a container or onto a surface while the
 * player is sitting, standing or lying on some OTHER object.
 *
 * run390's insides() (463EEC) tests the player record's +16 byte -- the
 * object the player is on or in, &HFF when free -- against the container
 * being reached into, and prints the refusal unless it is one or the other
 * (loc_4631D0..4631E1):
 *
 *     If player.on = &HFF Or player.on = container Then ok
 *     Else <You> & " can't reach " & the(container) & " from " &
 *          the(player.on) & "!"
 *
 * Both names come out of the definite-name helper, so it is "the " & Short
 * with the author's capitalisation kept: "You can't reach the dresser from
 * the Bed!"
 *
 * Measured 2026-09-05 on A_Morning_with_a_Headache.taf (3.90) in run390: the
 * game starts the player in bed, and `take alarm clock` -- the clock sits on
 * the dresser -- is refused exactly that way, as is `take suit` off the
 * chair.  The literal " can't reach " is in run370, run380 and run390's
 * constant pools and is ABSENT from run400's, so 4.0 dropped the rule
 * outright; the gate is < 4.00 rather than the usual 3.9 split.
 *
 * Two things here are inferred rather than measured, and are noted in
 * test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md: whether a game task for the
 * same command would run first (nothing in that game claims `take alarm
 * clock`), and how this orders against 3.7/3.8's own "not holding" gate, which
 * is why it sits after lib_take_container_unheld() rather than before it.
 */
static scr_bool
lib_cannot_reach_container (scr_gameref_t game, scr_int container)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    return FALSE;

  return gs_playerparent (game) != -1 && gs_playerparent (game) != container;
}

static void
lib_print_cannot_reach (scr_gameref_t game, scr_int container)
{
  const scr_filterref_t filter = gs_get_filter (game);

  lib_print_response_object (game,
                             "You can't reach ",
                             "I can't reach ",
                             "%player% can't reach ",
                             container, " from ");
  lib_print_object_np (game, gs_playerparent (game));
  pf_buffer_string (filter, "!");
}

/* The unfiltered form of lib_drain_multiple_references_if(), whose doc
   comment sits with the template in sclibrar_internal.h. */
void
lib_drain_multiple_references (scr_gameref_t game, scr_int object_count,
                               lib_list_t &list)
{
  lib_drain_multiple_references_if (game, object_count, list,
                                    [] (scr_int) { return TRUE; });
}


/*
 * lib_object_printer_390()
 *
 * The printer for an object named in a refusal: the composed name from 3.9,
 * the raw Prefix & " " & Short before it (run380 43E03E).
 */
lib_print_item_t
lib_object_printer_390 (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390
         ? lib_print_object_np : lib_print_object_raw;
}


/*
 * One lib_take_backend_common() call: what is being taken from, the objects
 * taken or refused so far, and whether the turn has printed anything yet.
 */
struct lib_take_t
{
  const scr_gameref_t game;
  const scr_int associate;
  const scr_bool is_associate_object, is_associate_npc;
  const scr_filterref_t filter;
  scr_int object_count, total;
  scr_bool has_printed;
  lib_list_t list;
  /*
   * Objects that would take the player over capacity, in the order they
   * were encountered.  The Runner's multi-take loop appends a separate
   * "<Name> is too heavy for me to carry at the moment."/" hands are full."
   * sentence for every such object (run400 @0047C127/@0047C0F0), not just
   * the first.
   */
  lib_list_t over_capacity, over_is_size;
  /*
   * 3.9 take-from: the capacity test lives in insides(), with its own order
   * and, for the all/and forms, its own reporting; see below.
   */
  const scr_bool from_390, from_390_multi;
  scr_int fits;

  lib_take_t (scr_gameref_t game_, scr_int associate_,
              scr_bool is_associate_object_, scr_bool is_associate_npc_)
    : game (game_), associate (associate_),
      is_associate_object (is_associate_object_),
      is_associate_npc (is_associate_npc_), filter (gs_get_filter (game_)),
      object_count (gs_object_count (game_)), total (0), has_printed (FALSE),
      from_390 (is_associate_object_
                && prop_get_taf_version (gs_get_bundle (game_))
                   >= TAF_VERSION_390
                && !lib_is_version_400 (game_)),
      from_390_multi (from_390 && !lib_take_from_single_named), fits (0) {}

  scr_bool try_game_commands (void);
  scr_bool count_fits_390 (void);
  void print_taken (scr_int parent);
  void acquire (void);
  void report_over_capacity (void);
  void report_not_in_associate (void);
  void report_npc_not_carrying (void);
  void report_held (void);
  void report_worn (void);
  void report_npc_held (void);
  void report_cannot_take (void);
};

/*
 * Try game commands for all referenced objects first.  If any succeed,
 * remove that reference from the list.  At the same time, filter out and
 * flag any object that takes us over the player's capacity.  Returns FALSE
 * if the whole take was refused.
 */
scr_bool
lib_take_t::try_game_commands (void)
{
  scr_int object;

  for (object = 0; object < object_count; object++)
    {
      scr_bool status;

      if (!game->object_references[object])
        continue;

      /*
       * Pre-3.9 refuses the whole take when the object's parent is a
       * dynamic container or surface the player isn't holding; see
       * lib_take_container_unheld().  With an associate the parent was
       * already vetted by lib_take_from_is_valid().
       */
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && lib_take_container_unheld (game,
                                        gs_object_parent (game, object)))
        {
          scr_int index_;

          lib_print_not_holding (game, gs_object_parent (game, object), ".");
          for (index_ = 0; index_ < object_count; index_++)
            game->object_references[index_] = FALSE;
          return FALSE;
        }

      /*
       * And pre-4.0 refuses it outright while the player is on or in some
       * other object; see lib_cannot_reach_container().
       */
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && lib_cannot_reach_container (game,
                                         gs_object_parent (game, object)))
        {
          scr_int index_;

          lib_print_cannot_reach (game, gs_object_parent (game, object));
          for (index_ = 0; index_ < object_count; index_++)
            game->object_references[index_] = FALSE;
          return FALSE;
        }

      /*
       * See if the object takes us beyond capacity.  If it does, note it and
       * continue.
       */
      {
        scr_bool is_size;

        if (!from_390 && lib_take_over_capacity (game, object, &is_size))
          {
            over_capacity.push_back (object);
            over_is_size.push_back (is_size);
            game->object_references[object] = FALSE;
            continue;
          }
      }

      /* Now try for a game command, using the associate if supplied. */
      if (is_associate_object)
        status = lib_try_game_command_with_object (game, "get",
                                                   object, "from", associate);
      else if (is_associate_npc)
        status = lib_try_game_command_with_npc (game, "get",
                                                object, "from", associate);
      else
        {
          scr_bool looked_up;

          status = lib_try_game_command_take_from_parent_400 (game, object,
                                                              &looked_up);
          /*
           * 4.0's take piece looks up "get " & name(obj, 0) only
           * (462B0D): `take pebble` never reaches a task `take a pebble` and
           * takes the pebble (p4WITHQ2.taf, runner_probes/withq2.run400.txt).
           */
          if (!looked_up && lib_is_version_400 (game))
            status = lib_try_game_command_take_definite (game, object);
          else if (!looked_up)
            status = lib_try_game_command_short (game, "get", object);
        }
      if (status)
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
        }
    }
  return TRUE;
}

/*
 * 3.9's all/and take-from first counts the objects that would fit, adding
 * each fitting one to running size and weight totals (run390 463394-4634F4,
 * with no held-container waiver).  If none fits it answers "<Your> hands are
 * full." when any failed on size, otherwise "That is too heavy.", and takes
 * nothing (4634F9-463545), returning FALSE.
 */
scr_bool
lib_take_t::count_fits_390 (void)
{
  scr_int object;

  if (from_390_multi)
    {
      scr_int size, weight;
      scr_bool any_size, any_weight;

      size = lib_carried_size (game);
      weight = lib_carried_weight (game);
      fits = 0;
      any_size = any_weight = FALSE;
      for (object = 0; object < object_count; object++)
        {
          if (!game->object_references[object]
              || !(gs_object_position (game, object) == OBJ_IN_OBJECT
                   || gs_object_position (game, object) == OBJ_ON_OBJECT)
              || gs_object_parent (game, object) != associate)
            continue;

          if (size + obj_get_size (game, object)
              > obj_get_player_size_limit (game))
            any_size = TRUE;
          else if (weight + obj_get_weight (game, object)
                   > obj_get_player_weight_limit (game))
            any_weight = TRUE;
          else
            {
              size += obj_get_size (game, object);
              weight += obj_get_weight (game, object);
              fits++;
            }
        }

      if (fits == 0 && (any_size || any_weight))
        {
          if (any_size)
            lib_print_clause (game, has_printed,
                              "Your hands are full.",
                              "My hands are full.",
                              "%player%'s hands are full.");
          else
            lib_print_clause (game, has_printed,
                              "That is too heavy.",
                              "That is too heavy.",
                              "That is too heavy.");
          for (object = 0; object < object_count; object++)
            game->object_references[object] = FALSE;
          return FALSE;
        }
    }
  return TRUE;
}

/* Print the line for the objects in list, just taken from PARENT, or from
   the room if PARENT is -1. */
void
lib_take_t::print_taken (scr_int parent)
{
  /*
   * 4.0 builds the take line in front of whatever the turn has
   * printed so far: get_piece copies the buffer aside (run400
   * @0047359A), clears it, writes "You take ...." and then, if
   * the copy was not empty, pspace()s and appends it back
   * (@004736B6).  So a task claiming one of the objects prints
   * after the library's line.  fullcircle `get all`, with a `get
   * *branch*` task, answers "You take the helm and the locket.
   * You take the branch."  (runner_probes/fullcircle.run400.txt
   * line 356.)  run390 orders it the same way: The Will's `get
   * all` at the Narrow Squeeze, where task 142 `* get * clover`
   * claims the clover, answers "You take the rare stamp, ... and
   * the gold ring.  You take the jade clover." (run390x,
   * runner_transcripts/thewill.txt T197).
   */
  scr_owned_string saved;
  if (has_printed && total == 0
      && (lib_is_version_400 (game) || lib_is_version_390 (game)))
    saved.reset (pf_transfer_buffer (filter));
  else if (has_printed)
    pf_buffer_string (filter, total == 0 ? "\n" : "  ");
  /*
   * 4.0 reworded the loose-in-the-room take; the from-container
   * branch keeps the same verb in every version.  Measured live on
   * a bare `take rock` / `get rock` / `take all`: run370 and
   * run390 answer "You pick up the rock.", run400 "You take the
   * rock."  (run380 shows the pre-4.0 half of the same handler
   * through its "nothing to pick up here" refusal.)
   *
   * What 4.0 did change in the from-container branch is the
   * *taken* object's prefix: pre-4.0 prints it raw, 4.0 normalizes
   * it like everything else.  The container is normalized in both.
   * Measured on the one game carried across three .taf versions by
   * the generators (see lib_print_object_np above): `take gun` out
   * of a "some"-prefixed container holding an "a"-prefixed pistol
   * gives run380 "You take a small pistol from some aluminum
   * clothes.", run390 "You take a small pistol from the aluminum
   * clothes.", run400 "You take the small pistol from the aluminum
   * clothes."  Only the pistol's own prefix moves at 4.0; the
   * clothes move at 3.9, which is the "some" rule, not this one.
   *
   * The raw pre-4.0 prefix belongs to the Runner's SINGLE-take
   * handler only -- any take that named exactly one object,
   * with or without an explicit "from" clause (the pistol probe
   * above was a bare `take gun`).  The multi-take loop
   * normalizes in every version: run390 on ALEXIS ("a"-prefixed
   * diary) answers `take diary from table` with "You take a
   * diary from the old oak table." but `take diary and cloak
   * from table` with "You take the diary and the woven cloak
   * from the old oak table." and `get all from table` with "You
   * take the diary, the brass lantern, the woven cloak and the
   * nice food from the old oak table."  (Measured live
   * 2026-08-22.)
   *
   * That multi-take exception is a 3.9 change of its own: pre-3.9
   * the multi-take loop prints raw too, so from 3.7 to 3.8 every
   * from-container take is raw whatever it named.  Measured live
   * under Wine 2026-08-24 on mikes.taf (3.80) in run380, whose
   * dresser holds four objects with EMPTY prefixes (deobfuscated
   * from the .taf, so this is not a guess): `take all from
   * dresser` answers "You take a socks, a shirt, a underwear and
   * a pair of pants from the dresser."  Empty is the case that
   * separates the two printers -- raw defaults it to "a ", the
   * normalizing one to "the " -- and the same replay pins both
   * halves at once, since the container itself comes out "the
   * dresser".  3.9 normalizes the same shape: the ALEXIS `get all
   * from table` above.
   */
  if (parent == -1)
    pf_buffer_string (filter,
                      lib_is_version_400 (game)
                      ? lib_select_response (game,
                                             "You take ",
                                             "I take ",
                                             "%player% take ")
                      : lib_select_response (game,
                                             "You pick up ",
                                             "I pick up ",
                                             "%player% pick up "));
  else if (!from_390_multi)
    pf_buffer_string (filter, lib_take_from_verb (game));
  if (from_390_multi)
    {
      /*
       * 3.9's all/and take-from punctuates its list by counting
       * down from the pre-pass count of objects that fit, not
       * from the number actually taken (run390 463AAA-463B18):
       * ", " while more than one remains, " and " before the
       * last, " from <container>." on reaching zero, and nothing
       * after that.  The verb is printed only if anything fit.
       * Objects the take loop accepts past the count are glued
       * on after the full stop.  Measured on Namiki's `take all
       * from bag`, seven objects with a count of five: "I take
       * my bandana, ... and the Kenzo jacket from the tennis
       * bag.the Dior skirtmy jewelry".
       */
      scr_int remaining = fits;
      size_t index;

      if (fits > 0)
        pf_buffer_string (filter, lib_take_from_verb (game));
      for (index = 0; index < list.size (); index++)
        {
          lib_print_object_np (game, list[index]);
          remaining--;
          if (remaining > 1)
            pf_buffer_string (filter, ", ");
          else if (remaining == 1)
            pf_buffer_string (filter, " and ");
          else if (remaining == 0)
            {
              pf_buffer_string (filter, " from ");
              lib_print_object_np (game, parent);
              pf_buffer_character (filter, '.');
            }
        }
    }
  else
    {
      lib_print_list (game, list,
                      parent == -1 || lib_is_version_400 (game)
                      || (prop_get_taf_version (gs_get_bundle (game))
                          >= TAF_VERSION_390
                          && !(lib_take_single_named
                               || lib_take_from_single_named))
                      ? lib_print_object_np : lib_print_object,
                      " and ");
      if (parent != -1)
        {
          pf_buffer_string (filter, " from ");
          lib_print_object_np (game, parent);
        }
      pf_buffer_character (filter, '.');
    }
  if (saved)
    {
      const scr_char *text = saved.get ();

      while (*text == '\n' || *text == ' ')
        text++;
      pf_buffer_pspace (filter);
      pf_buffer_string (filter, text);
    }
}

/*
 * We attempt acquisition of get-able objects here only for cases where
 * there is either no associate, or where the associate is an object.  If
 * the associate is an NPC, we're going to refuse all acquisitions later
 * on, by forcing object references.
 */
void
lib_take_t::acquire (void)
{
  scr_int object, parent, start, limit;

  /*
   * Attempt to acquire each remaining get-able object in turn, looping
   * on each possible parent object in turn, with an initial parent of
   * -1 for objects not contained or supported.
   *
   * If we're dealing with only objects from a known container or
   * supporter, eliminate all but one iteration of the parent search.
   */
  start = is_associate_object ? associate : -1;
  limit = is_associate_object ? associate : object_count - 1;
  for (parent = start; parent <= limit; parent++)
    {
      list.clear ();
      for (object = 0; object < object_count; object++)
        {
          if (!game->object_references[object])
            continue;

          /*
           * If parent is -1, ignore contained objects, otherwise ignore
           * objects not contained, or if contained, not contained by the
           * current parent.
           */
          if (parent == -1)
            {
              if (gs_object_position (game, object) == OBJ_IN_OBJECT
                  || gs_object_position (game, object) == OBJ_ON_OBJECT)
                continue;
            }
          else
            {
              if (!(gs_object_position (game, object) == OBJ_IN_OBJECT
                    || gs_object_position (game, object) == OBJ_ON_OBJECT))
                continue;
              if (gs_object_parent (game, object) != parent)
                continue;
            }

          /*
           * Here we have to repeat capacity checks.  As objects are
           * acquired more and more of the player's capacity gets used up.
           * This means a check directly before each acquisition.
           */
          {
            scr_bool is_size;

            if (from_390
                ? lib_take_from_over_capacity_390 (game, object, parent,
                                                   &is_size)
                : lib_take_over_capacity (game, object, &is_size))
              {
                over_capacity.push_back (object);
                over_is_size.push_back (is_size);
                continue;
              }
          }

          list.push_back (object);
          gs_object_player_get (game, object);
          /* A successful library take spends OnlyWhenNotMoved mode 1
           * (run400 `takes` @0047BF66).  Nothing else does. */
          gs_set_object_unmoved (game, object, FALSE);
        }

      if (!list.empty ())
        print_taken (parent);
      total += (scr_int) list.size ();
      has_printed |= !list.empty ();
    }
}

void
lib_take_t::report_over_capacity (void)
{
  /*
   * If we ran out of capacity, either in weight or in size, print the
   * details.  Each over-weight object gets its own sentence, in the order
   * encountered; a "hands are full" size refusal is printed at most once.
   */
  /*
   * 3.9's all/and take-from refuses nothing object by object.  After the loop
   * it appends one summary: the weight wording if any object failed on weight,
   * otherwise the size one (run390 463BB5-463C2B).
   */
  if (from_390_multi && !over_capacity.empty ())
    {
      scr_bool any_weight = FALSE;
      size_t over;

      for (over = 0; over < over_capacity.size (); over++)
        any_weight |= !over_is_size[over];
      if (any_weight)
        lib_print_clause (game, has_printed,
                          "You can't take any more, as it is too heavy.",
                          "I can't take any more, as it is too heavy.",
                          "%player% can't take any more, as it is too heavy.");
      else
        lib_print_clause (game, has_printed,
                          "You can't take any more, as your hands are full.",
                          "I can't take any more, as my hands are full.",
                          "%player% can't take any more, as %player%'s hands"
                          " are full.");
      has_printed = TRUE;
      over_capacity.clear ();
    }

  {
    scr_bool size_reported;
    size_t over;

    size_reported = FALSE;
    for (over = 0; over < over_capacity.size (); over++)
      {
        if (over_is_size[over])
          {
            if (size_reported)
              continue;
            size_reported = TRUE;

            lib_print_clause (game, has_printed,
                              "Your hands are full.",
                              "My hands are full.",
                              "%player%'s hands are full.");
          }
        else
          {
            lib_new_clause (game, has_printed);
            lib_print_too_heavy (game, over_capacity[over]);
          }
        has_printed |= TRUE;
      }
  }
}

/* Report the named objects that are not in or on the associated object. */
void
lib_take_t::report_not_in_associate (void)
{
    lib_drain_multiple_references (game, object_count, list);

    /*
     * 4.0 drops a named object that is not in the container without a word:
     * run400 answers `get coin and stone from box` with just "You take the
     * coin from the box." (p4TFROM, runner_probes/tfrom.run400.b.txt,
     * 2026-09-10), and there is no such literal anywhere in run400.
     *
     * Pre-4.0 does report it, and composes the sentence a word at a time
     * (run390 loc_4636D0-46378A: tense & Short & isare() & "not " & var_E0
     * & " " & definite(container) & "!", where var_E0 is "inside" or "on").
     * No " is not in " literal exists in any of the four Runners, so the
     * wording here is "inside", not "in", and the sentence ends with an
     * exclamation mark: run390 answers `get stone from box` with "The stone
     * is not inside the box!" (p39DARK, runner_probes/dark.run390.feed4.txt
     * and runner_probes/dark.run390.feed5.txt, 2026-09-10).  It reports an
     * object the player is already holding the same way -- the measured
     * turn is `get coin from box` with the coin in hand and the box open
     * and empty, "The coin is not inside the box!"
     * (runner_probes/dark.run390.feed5.txt) -- so there is no held-or-worn
     * exemption above.
     *
     * 3.7/3.8 have no such sentence (no "not inside"/"not on" in run370 or
     * run380): their take loop (run380 446FC5) skips a named object that
     * is not in the container without a word.  A line that takes nothing
     * at all is lib_take_from_nothing_taken_pre390()'s.
     */
    if (!list.empty () && !lib_is_version_400 (game)
        && prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
      {
        lib_new_clause (game, has_printed);
        lib_print_list (game, list, lib_print_object_np, " and ");
        pf_buffer_string (filter,
                          list.size () == 1
                          ? lib_select_plurality (game, list[0],
                                                  " is not ", " are not ")
                          : " are not ");
        pf_buffer_string (filter,
                          obj_is_container (game, associate)
                          ? "inside " : "on ");
        lib_print_object_np (game, associate);
        pf_buffer_character (filter, '!');
        has_printed = TRUE;
      }
}

/*
 * Now, deal with the case where we have an associated NPC.  Once this
 * case is handled, we can force the object references so that the code
 * that follows on from here will report errors taking all objects.
 *
 * Note that this means that we can never successfully take an object
 * from an NPC; that'll have to happen via a game's own commands.
 */
void
lib_take_t::report_npc_not_carrying (void)
{
  scr_int object;

    lib_drain_multiple_references_if
      (game, object_count, list, [this] (scr_int item)
       { return gs_object_position (game, item) != OBJ_PART_NPC; });

    if (!list.empty ())
      {
        lib_new_clause (game, has_printed);
        lib_print_npc_np (game, associate);
        pf_buffer_string (filter, " is not carrying ");
        lib_print_list (game, list, lib_print_object_np, " or ");
        pf_buffer_character (filter, '!');
      }
    has_printed |= !list.empty ();

    /*
     * Merge any remaining object references into multiple references,
     * so that succeeding code complains about the inability to acquire
     * these objects.
     */
    for (object = 0; object < object_count; object++)
      {
        game->multiple_references[object] |= game->object_references[object];
        game->object_references[object] = FALSE;
      }
}

/* Report the named objects the player already holds. */
void
lib_take_t::report_held (void)
{
  /*
   * 4.0's single-take piece tests possession with 44615C, which counts a
   * WORN object as carried (position 0 or &H9C, run400 General 4460F4), so
   * a single-named take of worn clothing gets the held object's task look-up
   * and "You are already carrying <object>." -- not "already wearing".
   * 3monkeys T65 `get sheet` with the sheet worn (run400x, 2026-09-19).
   */
  /*
   * A 4.0 line split on "and" resolves each piece through the same
   * single-take handler (run400 4731A2 loops the pieces through
   * Proc_19_23_473A34), so every piece gets the single wording: p4TKA `take
   * hat and stone` with the hat worn is "You take the stone.  You are already
   * carrying the hat." (runner_probes/tka.run400.txt cells 14/40/44,
   * 2026-09-20).
   */
  const scr_bool worn_is_held = lib_is_version_400 (game)
                                && (lib_take_single_named
                                    || lib_take_and_pieces_400);
  lib_drain_multiple_references_if
    (game, object_count, list, [this, worn_is_held] (scr_int item)
     {
       return gs_object_position (game, item) == OBJ_HELD_PLAYER
              || (worn_is_held
                  && gs_object_position (game, item) == OBJ_WORN_PLAYER);
     });

  /*
   * 4.0 gives the tasks their look at a held object too, and earlier than
   * the one ahead of " can't take " below: run400's take piece
   * Proc_19_39_46302C pre-matches "get <the object>" in the take-family
   * mode (@462AED / @462B84) and dispatches it with the restriction-failure
   * pass on (@462C5C, Proc_19_24_44CCE0(1, 1)) BEFORE either the
   * " can't take " test at 462CA0 or the " already carrying " one at
   * 462D01 -- and a claim exits the piece outright.  So a task restricted on
   * already holding the object answers with its own fail message where the
   * library would have said "You are already carrying the cone.".
   *
   * Measured on IceCream.taf in run400 (runner_transcripts/icecream.txt, turn
   * 1, 2026-09-07): `take cone`, with the cone already in hand at the start
   * of the game, is task 14's "  You already have an empty cone." -- the
   * FailMessage of its one restriction -- and not the library refusal.
   *
   * The look-up is the one 4.0's implicit take uses, definite name and
   * " from <holder>" clause included; see
   * lib_try_game_command_take_definite().
   */
  if (lib_is_version_400 (game) && !list.empty ())
    {
      lib_list_t held;
      scr_bool is_claimed = FALSE;

      for (const scr_int item : list)
        {
          if (lib_try_game_command_take_definite (game, item))
            is_claimed = TRUE;
          else
            held.push_back (item);
        }
      list.swap (held);
      if (is_claimed)
        {
          /* The task's text is complete in itself, terminator and all. */
          lib_take_refusal_claimed = TRUE;
          has_printed = TRUE;
        }
    }

  /*
   * A take that named a single object uses the Runner's single-take
   * handler, which words the held-object refusal "I am already carrying
   * <object>." (run400 @00462D25); "'ve already got <object>!" is the
   * multi-take processor's wording (run400 @0047BE1B).
   *
   * 4.0 only: the pre-4.0 Runners have no "already carrying" wording at
   * all (no such literal in run370/run380/run390.bas; the only held-take
   * refusal is "'ve already got <object>!" -- run370 436561, run380
   * 43E03E, run390_3 454EC5).  Measured live 2026-08-31 on cave.taf
   * run380: single-named `take parchment` while holding it answers
   * "You've already got half of a parchment!".
   */
  if (lib_is_version_400 (game)
      && ((lib_take_single_named && list.size () == 1)
          || (lib_take_and_pieces_400 && !list.empty ())))
    {
      /* One clause per piece on an "and" line (p4TKA cells 14/40/44). */
      for (const scr_int item : list)
        {
          lib_new_clause (game, has_printed);
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You are already carrying ",
                                                 "I am already carrying ",
                                                 "%player% is already carrying "));
          lib_print_object_np (game, item);
          pf_buffer_character (filter, '.');
          has_printed |= TRUE;
        }
    }
  else
    /* Pre-3.9 spells it Prefix & " " & Short (run380 43E03E): p38EXAM `take
     * stone` held is "You've already got a stone!"
     * (runner_probes/exam.run380.txt). */
    has_printed |= lib_print_object_list (game, has_printed, list, " and ", '!',
                                          "You've already got ",
                                          "I've already got ",
                                          "%player%'ve already got ",
                                          lib_object_printer_390 (game));
}

/* Report the named objects the player is wearing. */
void
lib_take_t::report_worn (void)
{
  lib_drain_multiple_references_if
    (game, object_count, list, [this] (scr_int item)
     { return gs_object_position (game, item) == OBJ_WORN_PLAYER; });

  /*
   * No Runner contracts this one.  Every take path builds it as
   * <pronoun> & " " & <are> & " already wearing " & <name> & "!" -- run400
   * 47BE82, run390 454F31, run380 43E0A2 -- so "You're" was never right; the
   * name is the plain Prefix & " " & Short concatenation below 3.9 and the
   * definite printer from 3.9 on, as everywhere else on this path.
   *
   * 3.7 has no worn arm at all: its takes() tests position 0 (held,
   * "'ve already got", 436561) and the room constant (the pick-up branch,
   * 436585) and nothing else, so a worn object falls through to the same
   * " can't see <raw> from here!" arm an absent one gets (436909/43696A).
   *
   * p37TWO/p38TWO/p39TWO `take hat` with the hat worn (cell 7 of
   * runner_probes/two.run370.verb3.rtf / runner_probes/two.run380.verb3.rtf /
   * runner_probes/two.run390.verb3.txt, 2026-09-21): "You can't see a hat
   * from here!" / "You are already wearing a hat!" / "You are already wearing
   * the hat!".
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380
      && !list.empty ())
    {
      /* The arm assigns the whole message per object, so the last one
         speaks; the raw name is this handler's own concatenation. */
      lib_new_clause (game, has_printed);
      lib_cant_see_named_pre_390 (game, list.back (), FALSE, " from here!");
      has_printed = TRUE;
    }
  else
    has_printed |= lib_print_object_list (game, has_printed, list, " and ", '!',
                                          "You are already wearing ",
                                          "I am already wearing ",
                                          "%player% is already wearing ",
                                          lib_object_printer_390 (game));
}

/* Report the named objects an NPC is holding or wearing. */
void
lib_take_t::report_npc_held (void)
{
  scr_int npc;

  GS_FOR_EACH_NPC (game, npc)
    {
      lib_drain_multiple_references_if
        (game, object_count, list, [this, npc] (scr_int item)
         {
           return (gs_object_position (game, item) == OBJ_HELD_NPC
                   || gs_object_position (game, item) == OBJ_WORN_NPC)
                  && gs_object_parent (game, item) == npc;
         });

      if (!list.empty ())
        {
          lib_new_clause (game, has_printed);
          lib_print_npc_np (game, npc);
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 " refuses to give you ",
                                                 " refuses to give me ",
                                                 " refuses to give %player% "));
          lib_print_list (game, list, lib_print_object_np, " and ");
          pf_buffer_character (filter, '!');
        }
      has_printed |= !list.empty ();
    }
}

/* Report whatever is left: the objects that cannot be taken at all. */
void
lib_take_t::report_cannot_take (void)
{
  lib_drain_multiple_references (game, object_count, list);

  /*
   * Only the 4.0 single-take handler ends this with "!" (run400 @47329D
   * builds " can't take " + name + "!"); its multi-take processor
   * (run400 @47C172) and both of 3.9's (run390 @4551A6, @455533) end it
   * with ".".  Measured 2026-08-29 in run390 on CAH.taf (cruel), `take it`
   * after `x jacket` -> "(a jacket)" / "You can't take the jacket.".
   */
  /*
   * 4.0 gives the tasks one more look at each object it is about to refuse:
   * run400's per-piece take handler (Proc_19_23_473A34) calls the task
   * pre-matcher Proc_19_35_453C50 at @473241, directly ahead of the loop
   * that builds " can't take " (@47329D), and a hit re-dispatches to the
   * task dispatcher Proc_19_24_44CCE0, restriction-failure pass included.
   * The match is by the resolved object's name, not by the typed words:
   * `take kelly` -- an alias, with no "poster" anywhere in the line -- still
   * runs "Get * poster", as do `take poster`, `pick up poster`, `take the
   * poster` and `take bed` ("Get * bed"); `grab poster` is no take verb at
   * all and gets "I don't understand what you want me to do with the kelly
   * brook poster."  (man overboard.taf, run400,
   * runner_probes/man_overboard.run400.probe.txt and
   * runner_probes/man_overboard.run400.txt line 39-40, measured 2026-08-29.)
   * The loop at the top of this function does the same for take-able objects;
   * the static ones never reach it, so they get their turn here instead.
   */
  if (!has_printed && !list.empty () && lib_is_version_400 (game))
    {
      lib_list_t refused;

      /*
       * The pre-match is lower-cased and the dispatch case-kept, as in the
       * take piece: a hit that then runs nothing ends on DontUnderstand with
       * no refusal line.  Professor in the Laboratory, mailbox down: `take
       * mailbox` and `get x rope` pre-match task 9
       * `[check/get/pull]{the}[mailbox]{on-a/on a}{rope}` on "get the mailbox
       * on-a rope", miss it on "get the Mailbox on-a Rope", and answer "What
       * was that?..." (runner_probes/professor.run400.mail2.txt T24/T25; `get
       * mailbox`, typed, runs task 9 in
       * runner_probes/professor.run400.mail6.txt).  A hit on a failing
       * restriction dispatches the typed line instead; see
       * lib_rebuilt_fallback_typed.
       */
      lib_rebuilt_raw_dispatch = TRUE;
      lib_rebuilt_silent_continues = TRUE;
      lib_rebuilt_fallback_typed = TRUE;
      for (const scr_int item : list)
        {
          if (!lib_try_game_command_short_canonical (game, "get", item))
            refused.push_back (item);
        }
      lib_rebuilt_fallback_typed = FALSE;
      lib_rebuilt_silent_continues = FALSE;
      lib_rebuilt_raw_dispatch = FALSE;
      list.swap (refused);
      /* The tasks' text is complete in itself; no refusal line follows. */
      lib_take_refusal_claimed = list.empty ();
      /* A refusal that stands leaves the line to the task dispatcher. */
      lib_take_refusal_redispatch = !list.empty ();
    }
  if (lib_is_version_400 (game) && lib_take_and_pieces_400)
    {
      /* Each "and" piece is its own single-take refusal: p4TKA `take stone
         and statue` -> "You take the stone.  You can't take the statue!"
         (runner_probes/tka.run400.txt cells 17/20, 2026-09-20). */
      for (const scr_int item : list)
        {
          lib_list_t one;
          one.push_back (item);
          has_printed |= lib_print_object_list (game, has_printed, one,
                                                " and ", '!',
                                                "You can't take ",
                                                "I can't take ",
                                                "%player% can't take ",
                                                lib_print_object_np);
        }
    }
  else
    lib_print_object_list (game, has_printed, list, " and ",
                           lib_is_version_400 (game)
                           && lib_take_single_named && list.size () == 1
                           ? '!' : '.',
                           "You can't take ",
                           "I can't take ",
                           "%player% can't take ",
                           /* 3.7/3.8 name it by the raw Prefix: "You can't
                              take a chair." (run380 43E697, run370 4369FF;
                              run370x/run380x p3738sit3, 2026-09-19). */
                           lib_object_printer_390 (game));
}

static void
lib_take_backend_common (scr_gameref_t game, scr_int associate,
                         scr_bool is_associate_object, scr_bool is_associate_npc)
{
  assert (!is_associate_object || !is_associate_npc);
  lib_take_t take (game, associate, is_associate_object, is_associate_npc);

  if (!take.try_game_commands () || !take.count_fits_390 ())
    return;

  if (!is_associate_npc)
    take.acquire ();
  take.report_over_capacity ();

  /*
   * Note any remaining multiple references left out of the take operation.
   * This is some workload...
   *
   * First, deal with the case where we have an associated object.
   */
  if (is_associate_object)
    take.report_not_in_associate ();
  if (is_associate_npc)
    take.report_npc_not_carrying ();

  /*
   * The remainder of this routine is common error reporting for both object
   * and NPC associates (and also for no associates).
   */
  take.report_held ();
  take.report_worn ();
  take.report_npc_held ();
  take.report_cannot_take ();

  lib_take_single_named = FALSE;
  lib_take_from_single_named = FALSE;
  lib_take_and_pieces_400 = FALSE;
}


/*
 * lib_take_backend()
 * lib_take_from_object_backend()
 * lib_take_from_npc_backend()
 *
 * Facets of lib_take_backend_common().  Provide backend handling for either
 * the plain "take" handlers, or the "take from <something>" handlers.
 */
void
lib_take_backend (scr_gameref_t game)
{
  lib_take_backend_common (game, -1, FALSE, FALSE);
}

void
lib_take_from_object_backend (scr_gameref_t game, scr_int associate)
{
  lib_take_backend_common (game, associate, TRUE, FALSE);
}

void
lib_take_from_npc_backend (scr_gameref_t game, scr_int associate)
{
  lib_take_backend_common (game, associate, FALSE, TRUE);
}


/*
 * run370's insides() gate -- a deliberate deviation.
 *
 * run370's insides() is entered only by a line holding c("get") and
 * c("from"), or c("remove") and c("from") (43A745) -- run380's gate
 * (4468B3) admits take and pick as well.  So at 3.7 `take gem from box` is
 * no take-from at all but the catch-all's "I don't understand what you
 * want me to do with the box.", and a bare take's rewrite into "<line> from
 * <parent>" (4368C4) only lands in insides() when the line says get: with
 * a gem in an open box on the floor, `get gem` is "You are not holding a
 * box." while `take gem` and `pick up gem` are "Take what?", and a nut in a
 * held bag is "You get a nut from the bag." for `get nut` and "Take what?"
 * for `take nut` (p37TFSW, run370x runner_probes/tfsw.run370.sweep3.rtf,
 * 2026-09-19).
 *
 * Scarier gives 3.7 run380's gate instead, so `take gem from box`, `take
 * nut` from a held bag and `empty bag` work there as they do at 3.8, and
 * nothing in the take, take-from or take-from-character paths tests the
 * line's verb.  This note is the record of what run370 did.
 */
