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
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "zlib.h"
#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const scr_char NEWLINE = '\n';
static const scr_char CARRIAGE_RETURN = '\r';
static const scr_char NUL = '\0';

/*
 * Legacy sentinel that introduced SCARIER's old private Battle System block,
 * once written (for battle games only) as extra lines after the ADRIFT turns
 * count.  Current saves no longer emit it: the live battle state is interleaved
 * inline in the player and NPC records, matching the Runner's own layout (see
 * ser_save_battle_block and the version-header note below).  The marker is kept
 * only so ser_load_game can still read those older SCARIER saves; it is
 * deliberately non-numeric so it can never be mistaken for a numeric ADRIFT
 * field.
 *
 * The Runner's layout (reverse-engineered from run400.exe savegame @477318)
 * interleaves battle data -- a 15-field block right after the player fields, and
 * a 17-field block inside each NPC's record (after "seen", before the walk
 * steps), both gated on the battle-system flag, with live stamina near the front
 * of each block and the Max/Hi/Lo attributes that the "Change <attribute>" task
 * action can alter following it.  Newer SCARIER saves reproduce that layout (and
 * emit the leading version line below), so they are byte-loadable by the Runner;
 * older saves that predate the change carry the trailing marker block instead.
 */
/*
 * Battle state block markers.  Version 2 adds the mutable per-character battle
 * attributes (attitude, max stamina, speed, and the ranged-attribute lo/hi/max
 * triples) after the version-1 stamina/counter/wield fields.  Version-1 saves
 * are still read; their absent attributes stay at the values battle_start()
 * seeded from the bundle.
 */
static const scr_char *const SER_BATTLE_MARKER = "ScarierBattleState/2";
static const scr_char *const SER_BATTLE_MARKER_V1 = "ScarierBattleState/1";

/*
 * ADRIFT v4 leading version line.  The real Runner's saveg routine
 * (run400.exe @477318) writes, as the very first .tas line,
 *
 *     Print 1, Chr(172) & CStr(App.Major) & Format(Minor,"000") & Format(Rev,"00")
 *
 * i.e. the byte 0xAC ('<not>', Chr(172)) followed by the Major version, the
 * Minor zero-padded to three digits, and the Revision zero-padded to two.  For
 * the shipped run400.exe, which reports file version 4.0.0.52, that is exactly
 * "\xAC" "400052".  SCARIER historically omitted this line entirely (its stream
 * started at GameName), so a Runner save and a SCARIER save were mutually
 * unreadable.  We now emit the header so the Runner will accept our saves, and
 * on read we treat the leading 0xAC byte as an unambiguous discriminator: a
 * line beginning with it is a Runner/new-format save whose version line we skip;
 * any other first line is a legacy SCARIER save whose first line is the GameName.
 */
static const scr_byte SER_VERSION_LEAD = 0xAC;
static const scr_char *const SER_VERSION_HEADER = "\xAC" "400052";

enum { BUFFER_SIZE = 4096 };

/*
 * Pre-4.0 saved games.
 *
 * ADRIFT 3.8 and 3.9 store a saved game in their own layout, and obfuscate it
 * with the Visual Basic PRNG rather than deflating it.  When ser_pre_v4 is set
 * the writers below emit that layout, so run390.exe accepts a save we wrote;
 * ser_load_game turns it on from the container format, so we accept one it
 * wrote.  The differences from 4.0 are:
 *
 *   - no version line, and no player-name line;
 *   - an object's location is always a bare position (an unmoved static is -1,
 *     never a room list), and it carries no trailing "unmoved" flag;
 *   - openness is space-padded, with "open" and "closed" exchanged (the same
 *     swap parse_fixup_openable_key applies when reading a pre-4.0 .taf);
 *   - the battle blocks carry Strength and Defence only, written bare -- 3.9
 *     has no Accuracy or Agility and no "lo" bounds;
 *   - NPC walk steps are not stored at all, so a restored walk starts over.
 *
 * Decoded from run390.exe's own saves of The Hangover and Melbourne Beach; a
 * save written here restores in run390.exe with the right room, score and
 * inventory.  3.8 is inference: it shares the obfuscation and the openness
 * swap, but has not been checked against a 3.8 Runner.
 */
static scr_bool ser_pre_v4 = FALSE;

/*
 * ser_openness_pre_v4()
 *
 * Exchange the pre-4.0 "open" and "closed" openness values.  Its own inverse,
 * so the same call serves the writer and the reader.
 */
static scr_int
ser_openness_pre_v4 (scr_int openness)
{
  return (openness == 5 || openness == 6) ? 11 - openness : openness;
}

/*
 * Deflate level for the next serialization run.  File saves keep zlib's
 * default level; per-turn undo memos (memo_save_game) select Z_BEST_SPEED
 * instead -- they are in-memory only, overwritten every 16 turns, and the
 * per-turn deflate at the default level profiles as ~20-30% of the whole
 * interpreter.  Either level produces a stream any inflate can read, so
 * save-file compatibility is unaffected.
 */
static scr_int ser_compression = Z_DEFAULT_COMPRESSION;

void
ser_set_fast_compression (scr_bool fast)
{
  ser_compression = fast ? Z_BEST_SPEED : Z_DEFAULT_COMPRESSION;
}

/* Output buffer. */
static scr_byte *ser_buffer = NULL;
static scr_int ser_buffer_length = 0;

/* Callback and opaque pointer for use by output functions. */
static scr_write_callbackref_t ser_callback = (scr_write_callbackref_t) NULL;
static void *ser_opaque = NULL;


/*
 * ser_flush()
 * ser_buffer_character()
 *
 * Flush pending buffer contents; add a character to the buffer.
 */
static void
ser_flush (scr_bool is_final)
{
  static scr_bool initialized = FALSE;
  static scr_byte *out_buffer = NULL;
  static scr_int out_buffer_size = 0;
  static z_stream stream;

  scr_int status;

  /*
   * A pre-4.0 save is not compressed at all: the plain text goes out xor'd
   * with the PRNG keystream, which taf_obfuscate_reset() started in
   * ser_save_game().  Nothing here touches the deflate state, so the 4.0 path
   * below is unaffected.
   */
  if (ser_pre_v4)
    {
      if (ser_buffer_length > 0)
        {
          taf_obfuscate_buffer (ser_buffer, ser_buffer_length);
          ser_callback (ser_opaque, ser_buffer, ser_buffer_length);
          ser_buffer_length = 0;
        }

      if (is_final)
        {
          scr_free (ser_buffer);
          ser_buffer = NULL;
        }
      return;
    }

  /* If this is an initial call, initialize deflation. */
  if (!initialized)
    {
      /* Allocate an initial output buffer. */
      out_buffer_size = BUFFER_SIZE;
      out_buffer = (decltype(out_buffer)) scr_malloc (out_buffer_size);

      /* Initialize Zlib deflation functions. */
      stream.next_out = out_buffer;
      stream.avail_out = out_buffer_size;
      stream.next_in = ser_buffer;
      stream.avail_in = 0;

      stream.zalloc = Z_NULL;
      stream.zfree = Z_NULL;
      stream.opaque = Z_NULL;

      status = deflateInit (&stream, ser_compression);
      if (status != Z_OK)
        {
          scr_error ("ser_flush: deflateInit: error %ld\n", status);
          ser_buffer_length = 0;

          scr_free (out_buffer);
          out_buffer = NULL;
          out_buffer_size = 0;
          return;
        }

      initialized = TRUE;
    }

  /* Deflate data from the current output buffer. */
  stream.next_in = ser_buffer;
  stream.avail_in = ser_buffer_length;

  /* Loop while deflate output is pending and buffer not emptied. */
  while (TRUE)
    {
      scr_int in_bytes, out_bytes;

      /* Compress stream data, with finish if this is the final flush. */
      if (is_final)
        status = deflate (&stream, Z_FINISH);
      else
        status = deflate (&stream, Z_NO_FLUSH);
      if (status != Z_STREAM_END && status != Z_OK)
        {
          scr_error ("ser_flush: deflate: error %ld\n", status);
          ser_buffer_length = 0;

          scr_free (out_buffer);
          out_buffer = NULL;
          out_buffer_size = 0;
          initialized = FALSE;
          return;
        }

      /* Calculate bytes used, and output. */
      in_bytes = ser_buffer_length - stream.avail_in;
      out_bytes = out_buffer_size - stream.avail_out;

      /* See if compressed data is available. */
      if (out_bytes > 0)
        {
          /* Write it to save file output through the callback. */
          ser_callback (ser_opaque, out_buffer, out_bytes);

          /* Reset deflation stream for available space. */
          stream.next_out = out_buffer;
          stream.avail_out = out_buffer_size;
        }

      /* Remove consumed data from the input buffer. */
      if (in_bytes > 0)
        {
          /* Move any unused data, and reduce length. */
          memmove (ser_buffer,
                   ser_buffer + in_bytes, ser_buffer_length - in_bytes);
          ser_buffer_length -= in_bytes;

          /* Reset deflation stream for consumed data. */
          stream.next_in = ser_buffer;
          stream.avail_in = ser_buffer_length;
        }

      /* If final flush, wait until deflate indicates finished. */
      if (is_final && status == Z_OK)
        continue;

      /* If data was consumed or produced, break. */
      if (out_bytes > 0 || in_bytes > 0)
        break;
    }

  /* If this was a final call, clean up. */
  if (is_final)
    {
      /* Compression completed. */
      status = deflateEnd (&stream);
      if (status != Z_OK)
        scr_error ("ser_flush: warning: deflateEnd: error %ld\n", status);

      if (ser_buffer_length != 0)
        {
          scr_error ("ser_flush: warning: deflate missed data\n");
          ser_buffer_length = 0;
        }

      /* Free the allocated compression buffer. */
      scr_free (ser_buffer);
      ser_buffer = NULL;

      /*
       * Free output buffer, and reset flag for reinitialization on the next
       * call.
       */
      scr_free (out_buffer);
      out_buffer = NULL;
      out_buffer_size = 0;
      initialized = FALSE;
    }
}

static void
ser_buffer_character (scr_char character)
{
  /* Allocate the buffer if not yet done. */
  if (!ser_buffer)
    {
      assert (ser_buffer_length == 0);
      ser_buffer = (decltype(ser_buffer)) scr_malloc (BUFFER_SIZE);
    }

  /* Add to the buffer, with intermediate flush if filled. */
  ser_buffer[ser_buffer_length++] = character;
  if (ser_buffer_length == BUFFER_SIZE)
    ser_flush (FALSE);
}


/*
 * ser_buffer_buffer()
 * ser_buffer_string()
 * ser_buffer_int()
 * ser_buffer_int_special()
 * ser_buffer_uint()
 * ser_buffer_boolean()
 *
 * Buffer a buffer, a string, an unsigned and signed integer, and a boolean.
 */
static void
ser_buffer_buffer (const scr_char *buffer, scr_int length)
{
  scr_int offset = 0;

  /* Allocate the buffer if not yet done. */
  if (!ser_buffer)
    {
      assert (ser_buffer_length == 0);
      ser_buffer = (decltype(ser_buffer)) scr_malloc (BUFFER_SIZE);
    }

  /* Copy in chunks, flushing eagerly whenever the buffer fills -- every
   * helper (ser_buffer_character included) relies on the buffer never being
   * left exactly full.  The undo snapshot funnels the whole game state
   * through here every turn, so this is memcpy rather than the old
   * per-character loop; the deflate stream sees the same byte sequence
   * either way (Z_NO_FLUSH output depends only on the data). */
  while (offset < length)
    {
      scr_int chunk;

      chunk = BUFFER_SIZE - ser_buffer_length;
      if (chunk > length - offset)
        chunk = length - offset;
      memcpy (ser_buffer + ser_buffer_length, buffer + offset, chunk);
      ser_buffer_length += chunk;
      offset += chunk;

      if (ser_buffer_length == BUFFER_SIZE)
        ser_flush (FALSE);
    }
}

static void
ser_buffer_string (const scr_char *string)
{
  /* Buffer string, followed by DOS style end-of-line. */
  ser_buffer_buffer (string, strlen (string));
  ser_buffer_character (CARRIAGE_RETURN);
  ser_buffer_character (NEWLINE);
}

/*
 * ser_render_ulong()
 *
 * Render an unsigned magnitude as decimal digits, returning the advanced
 * output pointer.  Hand-rolled because the snprintf calls these helpers made
 * per value profiled at ~15% of a bulk headless replay (macOS snprintf takes
 * locale locks); the rendered bytes are identical.
 */
static scr_char *
ser_render_ulong (scr_char *out, unsigned long magnitude)
{
  scr_char digits[24];
  scr_int count = 0;

  do
    {
      digits[count++] = (scr_char) ('0' + magnitude % 10);
      magnitude /= 10;
    }
  while (magnitude > 0);
  while (count > 0)
    *out++ = digits[--count];
  return out;
}

static void
ser_buffer_int (scr_int value)
{
  scr_char buffer[32];
  scr_char *out = buffer;

  /* Convert to a string ("%ld") and buffer that. */
  if (value < 0)
    *out++ = '-';
  out = ser_render_ulong (out, value < 0 ? -(unsigned long) value
                                         : (unsigned long) value);
  *out = '\0';
  ser_buffer_string (buffer);
}

static void
ser_buffer_int_special (scr_int value)
{
  scr_char buffer[32];
  scr_char *out = buffer;

  /* Weirdo formatting for compatibility: "% ld " -- a sign column (space for
   * non-negative, '-' for negative) and a trailing space. */
  *out++ = value < 0 ? '-' : ' ';
  out = ser_render_ulong (out, value < 0 ? -(unsigned long) value
                                         : (unsigned long) value);
  *out++ = ' ';
  *out = '\0';
  ser_buffer_string (buffer);
}

/*
 * The Runner stores the four ranged battle attributes in the save in the order
 * Strength, Defence, Accuracy, Agility -- SCARIER's scr_battle_t slots 0, 2, 1, 3
 * (BATTLE_STRENGTH/ACCURACY/DEFENSE/AGILITY) -- and writes each as max, hi, lo.
 * (Order derived from real run400.exe saves; the original capture notes are no
 * longer in the tree, so re-confirm against a Runner save before changing it.)
 */
static const scr_int SER_BATTLE_SLOTS[BATTLE_ATTR_COUNT] = {
  BATTLE_STRENGTH, BATTLE_DEFENSE, BATTLE_ACCURACY, BATTLE_AGILITY
};

/*
 * ser_save_battle_block()
 *
 * Buffer one character's interleaved ADRIFT battle block.  For an NPC (npc >= 0)
 * this is 17 values: attitude, max stamina, live stamina, then each attribute's
 * (max, hi, lo) in SER_BATTLE_SLOTS order, then speed and the attack counter.
 * For the player (npc < 0) it is 15 values: the attitude and speed/attack-
 * counter fields are absent, and the block ends with the wielded-weapon object
 * (the Runner's player sub-struct index 38).  The Runner prints these with the
 * raw VB Print numeric format, i.e. space-padded; ser_buffer_int_special matches
 * it byte for byte.
 *
 * NOTE: the byte-exact correspondence to real run400.exe saves was originally
 * validated against captured .tas fixtures; those notes are no longer checked
 * into the tree, so treat cross-Runner compatibility as unverified against this
 * repo alone until re-confirmed with a Runner-produced battle save.
 */
static void
ser_save_battle_block (scr_gameref_t game, scr_int npc)
{
  const scr_battle_t *battle = (npc < 0) ? gs_player_battle (game)
                                        : gs_npc_battle (game, npc);
  scr_int stamina = (npc < 0) ? gs_playerstamina (game)
                             : gs_npc_stamina (game, npc);
  /* 3.9 writes these bare; 4.0 space-pads them. */
  void (*const emit) (scr_int) = ser_pre_v4 ? ser_buffer_int
                                            : ser_buffer_int_special;
  scr_int index_;

  if (npc >= 0)
    emit (battle->attitude);
  emit (battle->maxstamina);
  emit (stamina);
  for (index_ = 0; index_ < BATTLE_ATTR_COUNT; index_++)
    {
      const scr_int slot = SER_BATTLE_SLOTS[index_];

      /*
       * 3.9 knows only the strength-versus-defence model, and keeps no "lo"
       * bound, so it stores the first two slots' max and hi and nothing else:
       * seven values for the player, nine for an NPC.
       */
      if (ser_pre_v4 && slot != BATTLE_STRENGTH && slot != BATTLE_DEFENSE)
        continue;

      emit (battle->max[slot]);
      emit (battle->hi[slot]);
      if (!ser_pre_v4)
        emit (battle->lo[slot]);
    }
  if (npc < 0)
    emit (gs_playerwield (game));
  else
    {
      emit (battle->speed);
      emit (gs_npc_attackcounter (game, npc));
    }
}

/*
 * Cached immutable serializer inputs.
 *
 * memo_save_game() drives ser_save_game() every turn for the undo ring, and
 * before caching the serializer re-read the same immutable bundle properties
 * each time: every static object's "Where" room list (2 x room_count
 * prop_get_boolean calls per ROOMLIST_SOME_ROOMS object per turn -- the
 * largest remaining prop_get consumer in a bulk-replay profile), every
 * event's starter task, and every variable's name and type.  Remember each
 * answer the first time it is read.  As with the task cache (sctasks.cpp),
 * caching is lazy -- the first touch goes through the same fatal-checking
 * prop_get_* wrappers, so a malformed game fails exactly as before -- and the
 * cache tracks a single game, dropped by gs_destroy() via ser_forget_game().
 * Only the save path caches; ser_load_game() is a once-per-restore walk.
 */
static const void *ser_cache_game = NULL;
/* Per static object, the exact int sequence its room list emits (leading
 * count, then rooms); always non-empty once built, so empty() = not built. */
static std::vector<std::vector<scr_int> > ser_cache_object_rooms;
static std::vector<scr_int> ser_cache_event_task;      /* -1 = not built */
static scr_int ser_cache_var_count = -1;
static std::vector<const scr_char *> ser_cache_var_name;  /* NULL = unbuilt */
static std::vector<scr_int> ser_cache_var_type;

static void
ser_synchronize_cache (scr_gameref_t game)
{
  if (ser_cache_game != game)
    {
      ser_cache_object_rooms.assign (gs_object_count (game),
                                     std::vector<scr_int> ());
      ser_cache_event_task.assign (gs_event_count (game), -1);
      ser_cache_var_count = -1;
      ser_cache_var_name.clear ();
      ser_cache_var_type.clear ();
      ser_cache_game = game;
    }
}

/*
 * ser_forget_game()
 *
 * Drop any serializer cache built for the given game.  Called from
 * gs_destroy() so a stale cache can never outlive its game.
 */
void
ser_forget_game (const void *game)
{
  if (ser_cache_game == game)
    {
      ser_cache_game = NULL;
      ser_cache_object_rooms.clear ();
      ser_cache_event_task.clear ();
      ser_cache_var_count = -1;
      ser_cache_var_name.clear ();
      ser_cache_var_type.clear ();
    }
}

/*
 * ser_object_static_rooms()
 *
 * The int sequence (count, then 1-based rooms) emitted for an unmoved static
 * object's bundle "Where" room list, built from the immutable bundle
 * properties on first use and cached thereafter.
 */
static const std::vector<scr_int> &
ser_object_static_rooms (scr_gameref_t game, scr_int object)
{
  std::vector<scr_int> &cached = ser_cache_object_rooms[object];

  if (cached.empty ())
    {
      const scr_prop_setref_t bundle = gs_get_bundle (game);
      scr_vartype_t vt_key[5];
      scr_int type, count, room;

      vt_key[0].string = "Objects";
      vt_key[1].integer = object;
      vt_key[2].string = "Where";
      vt_key[3].string = "Type";
      type = prop_get_integer (bundle, "I<-siss", vt_key);
      switch (type)
        {
        case ROOMLIST_ONE_ROOM:
          vt_key[3].string = "Room";
          room = prop_get_integer (bundle, "I<-siss", vt_key);
          cached.push_back (1);
          cached.push_back (room);
          break;

        case ROOMLIST_SOME_ROOMS:
          vt_key[3].string = "Rooms";
          count = 0;
          for (room = 0; room < gs_room_count (game); room++)
            {
              vt_key[4].integer = room + 1;
              if (prop_get_boolean (bundle, "B<-sissi", vt_key))
                count++;
            }
          cached.push_back (count);
          for (room = 0; room < gs_room_count (game); room++)
            {
              vt_key[4].integer = room + 1;
              if (prop_get_boolean (bundle, "B<-sissi", vt_key))
                cached.push_back (room + 1);
            }
          break;

        case ROOMLIST_ALL_ROOMS:
          cached.push_back (gs_room_count (game));
          for (room = 0; room < gs_room_count (game); room++)
            cached.push_back (room + 1);
          break;

        case ROOMLIST_NO_ROOMS:
        case ROOMLIST_NPC_PART:
        default:
          cached.push_back (0);
          break;
        }
    }
  return cached;
}

/*
 * ser_save_object_location()
 *
 * Buffer an object's location in ADRIFT layout.  A dynamic (movable) object is
 * a single position integer (Format, bare).  A static object is a room list:
 * a count followed by one 1-based room index per room it is present in (raw
 * Print, space-padded).  An unmoved static object's list comes from its bundle
 * "Where" room list; a static object relocated by an event is emitted from its
 * current single-room position (or an empty list if held/elsewhere).
 */
static void
ser_save_object_location (scr_gameref_t game, scr_int object)
{
  /*
   * Pre-4.0 has no room list: every object, static or not, is the bare
   * position, which for an unmoved static is -1.
   */
  if (ser_pre_v4 || !obj_is_static (game, object))
    {
      ser_buffer_int (gs_object_position (game, object));
      return;
    }

  if (!gs_object_static_unmoved (game, object))
    {
      /* Relocated static object: present in its current room, or nowhere. */
      const scr_int position = gs_object_position (game, object);
      if (position >= 1 && position <= gs_room_count (game))
        {
          ser_buffer_int_special (1);
          ser_buffer_int_special (position);
        }
      else
        ser_buffer_int_special (0);
      return;
    }

  {
    const std::vector<scr_int> &rooms = ser_object_static_rooms (game, object);
    std::vector<scr_int>::size_type index_;

    for (index_ = 0; index_ < rooms.size (); index_++)
      ser_buffer_int_special (rooms[index_]);
  }
}

static void
ser_buffer_uint (scr_uint value)
{
  scr_char buffer[32];
  scr_char *out;

  /* Convert to a string ("%lu") and buffer that. */
  out = ser_render_ulong (buffer, (unsigned long) value);
  *out = '\0';
  ser_buffer_string (buffer);
}

static void
ser_buffer_boolean (scr_bool boolean)
{
  /* Write a 1 for TRUE, 0 for FALSE. */
  ser_buffer_string (boolean ? "1" : "0");
}


/*
 * ser_variable_count()
 * ser_variable_at()
 *
 * The game's variables as the serializer walks them.  Save and load both
 * visit every variable in declaration order and switch on its type, so the
 * count and the name/type lookup are shared; only the transfer differs.
 */
static scr_int
ser_variable_count (scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[1];

  vt_key[0].string = "Variables";
  return prop_get_child_count (bundle, "I<-s", vt_key);
}

static scr_int
ser_variable_at (scr_prop_setref_t bundle, scr_int index_,
                 const scr_char **name)
{
  scr_vartype_t vt_key[3];

  vt_key[0].string = "Variables";
  vt_key[1].integer = index_;
  vt_key[2].string = "Name";
  *name = prop_get_string (bundle, "S<-sis", vt_key);
  vt_key[2].string = "Type";
  return prop_get_integer (bundle, "I<-sis", vt_key);
}


/*
 * ser_game_is_pre_v4()
 *
 * TRUE if this game came from a version 3.8 or 3.9 .taf.  "Version" is
 * published by parse_add_version() as the TAF_VERSION_* integer.
 */
static scr_bool
ser_game_is_pre_v4 (scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[1];

  vt_key[0].string = "Version";
  return prop_get_integer (bundle, "I<-s", vt_key) < TAF_VERSION_400;
}


/*
 * ser_save_game()
 * ser_save_game_to_file()
 *
 * Serialize a game and save its state using the given callback and opaque.
 *
 * ser_save_game_to_file() is the one that produces a file for the player, and
 * for a pre-4.0 game it writes that game's native save format so the original
 * Runner can read it.  ser_save_game() always writes the full 4.0 layout, and
 * is what the undo ring and the Spatterlight autosave use: the pre-4.0 layout
 * is lossy (no walk steps, no Accuracy or Agility, no "unmoved" flags) in ways
 * an undo must not be.  Both are read back by ser_load_game().
 */
static void
ser_save_game_internal (scr_gameref_t game, scr_write_callbackref_t callback,
                        void *opaque, scr_bool runner_compatible)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int index_, var_count;
  assert (callback);

  /* Store the callback and opaque references, for writer functions. */
  ser_callback = callback;
  ser_opaque = opaque;

  /* Select the save layout, and start the keystream if it is the pre-4.0 one. */
  ser_pre_v4 = runner_compatible && ser_game_is_pre_v4 (bundle);
  if (ser_pre_v4)
    taf_obfuscate_reset ();

  /* Reset the immutable-input cache if this is not the game it was built
   * for; the undo ring calls this every turn, so the reads cached below
   * matter. */
  ser_synchronize_cache (game);

  /*
   * Write the ADRIFT v4 version line first, so the real Runner (and other v4
   * interpreters) recognise the file as one of theirs.  See SER_VERSION_HEADER.
   * Pre-4.0 saves have no version line at all.
   */
  if (!ser_pre_v4)
    ser_buffer_string (SER_VERSION_HEADER);

  /* Write the game name. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "GameName";
  ser_buffer_string (prop_get_string (bundle, "S<-ss", vt_key));

  /* Write the counts of rooms, objects, etc. */
  ser_buffer_int (gs_room_count (game));
  ser_buffer_int (gs_object_count (game));
  ser_buffer_int (gs_task_count (game));
  ser_buffer_int (gs_event_count (game));
  ser_buffer_int (gs_npc_count (game));

  /* Write the score. */
  ser_buffer_int (game->score);

  /*
   * Write the player block in ADRIFT layout: the player name (the Runner's
   * first player field, which older SCARIER saves and pre-4.0 saves omit),
   * then room, parent, position and gender.
   */
  vt_key[0].string = "Globals";
  if (!ser_pre_v4)
    {
      vt_key[1].string = "PlayerName";
      ser_buffer_string (prop_get_string (bundle, "S<-ss", vt_key));
    }
  ser_buffer_int (gs_playerroom (game) + 1);
  ser_buffer_int (gs_playerparent (game));
  ser_buffer_int (gs_playerposition (game));
  vt_key[1].string = "PlayerGender";
  ser_buffer_int (prop_get_integer (bundle, "I<-ss", vt_key));

  /*
   * Write encumbrance details: the player's size and weight limits (constant
   * for a game, the same converted values the Runner stores), then the current
   * carried size and weight.  We now maintain these running totals (matching the
   * Runner's accounting, double-count and all), so we write the live values just
   * as the Runner does.  They are still recomputed from inventory on load, so a
   * reader that ignores them remains correct.
   */
  ser_buffer_int (obj_get_player_size_limit (game));
  ser_buffer_int (gs_carried_size (game));
  ser_buffer_int (obj_get_player_weight_limit (game));
  ser_buffer_int (gs_carried_weight (game));

  /*
   * Battle System: the player's interleaved battle block sits here, between the
   * encumbrance fields and the room-seen flags, for battle games only.
   */
  if (battle_is_enabled (game))
    ser_save_battle_block (game, -1);

  /* Save rooms information. */
  for (index_ = 0; index_ < gs_room_count (game); index_++)
    ser_buffer_boolean (gs_room_seen (game, index_));

  /* Save objects information. */
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      ser_save_object_location (game, index_);
      ser_buffer_boolean (gs_object_seen (game, index_));
      ser_buffer_int (gs_object_parent (game, index_));
      if (gs_object_openness (game, index_) != 0)
        {
          if (ser_pre_v4)
            ser_buffer_int_special
                (ser_openness_pre_v4 (gs_object_openness (game, index_)));
          else
            ser_buffer_int (gs_object_openness (game, index_));
        }

      /* A pre-4.0 game's objects all have CurrentState zero (the 3.9 grammar
       * defaults it), so this never fires for a pre-4.0 save. */
      if (gs_object_state (game, index_) != 0)
        ser_buffer_int (gs_object_state (game, index_));

      if (!ser_pre_v4)
        ser_buffer_boolean (gs_object_unmoved (game, index_));
    }

  /* Save tasks information. */
  for (index_ = 0; index_ < gs_task_count (game); index_++)
    {
      ser_buffer_boolean (gs_task_done (game, index_));
      ser_buffer_boolean (gs_task_scored (game, index_));
    }

  /* Save events information. */
  for (index_ = 0; index_ < gs_event_count (game); index_++)
    {
      scr_int startertype, task;

      /* Get starter task, if any (immutable, so cached after first read). */
      task = ser_cache_event_task[index_];
      if (task < 0)
        {
          vt_key[0].string = "Events";
          vt_key[1].integer = index_;
          vt_key[2].string = "StarterType";
          startertype = prop_get_integer (bundle, "I<-sis", vt_key);
          if (startertype == 3)
            {
              vt_key[2].string = "TaskNum";
              task = prop_get_integer (bundle, "I<-sis", vt_key);
            }
          else
            task = 0;
          ser_cache_event_task[index_] = task;
        }

      /* Save event details. */
      ser_buffer_int (gs_event_time (game, index_));
      ser_buffer_int (task);
      ser_buffer_int (gs_event_state (game, index_) - 1);
      if (task > 0)
        ser_buffer_boolean (gs_task_done (game, task - 1));
      else
        ser_buffer_boolean (FALSE);
    }

  /* Save NPCs information. */
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      scr_int walk;

      ser_buffer_int (gs_npc_location (game, index_));
      ser_buffer_boolean (gs_npc_seen (game, index_));

      /* The NPC's interleaved battle block sits after "seen", before walks. */
      if (battle_is_enabled (game))
        ser_save_battle_block (game, index_);

      /* Pre-4.0 stores no walk steps; a restored walk starts over there. */
      if (!ser_pre_v4)
        {
          for (walk = 0; walk < gs_npc_walkstep_count (game, index_); walk++)
            ser_buffer_int_special (gs_npc_walkstep (game, index_, walk));
        }
    }

  /* Save each variable.  The count and each name/type pair are immutable,
   * so they are cached after the first turn's walk; only the values change. */
  var_count = ser_cache_var_count;
  if (var_count < 0)
    {
      var_count = ser_variable_count (bundle);
      ser_cache_var_count = var_count;
      ser_cache_var_name.assign (var_count, (const scr_char *) NULL);
      ser_cache_var_type.assign (var_count, 0);
    }

  for (index_ = 0; index_ < var_count; index_++)
    {
      const scr_char *name = ser_cache_var_name[index_];
      scr_int var_type;

      if (name)
        var_type = ser_cache_var_type[index_];
      else
        {
          var_type = ser_variable_at (bundle, index_, &name);
          ser_cache_var_name[index_] = name;
          ser_cache_var_type[index_] = var_type;
        }

      switch (var_type)
        {
        case TAFVAR_NUMERIC:
          ser_buffer_int (var_get_integer (vars, name));
          break;

        case TAFVAR_STRING:
          ser_buffer_string (var_get_string (vars, name));
          break;

        default:
          scr_fatal ("ser_save_game: unknown variable type, %ld\n", var_type);
        }
    }

  /* Save timing information. */
  ser_buffer_uint (var_get_elapsed_seconds (vars));

  /* Save turns count. */
  ser_buffer_uint ((scr_uint) game->turns);

  /*
   * Note: the live Battle System state (stamina, attributes, attitudes, etc.)
   * is now written inline, in the player and NPC blocks above, matching the
   * Runner's layout.  We no longer append the private trailing SER_BATTLE_MARKER
   * block; older SCARIER saves that carry one are still read (see ser_load_game).
   */

  /*
   * Flush the last buffer contents, and drop the callback and opaque
   * references.
   */
  ser_flush (TRUE);
  ser_callback = NULL;
  ser_opaque = NULL;
  ser_pre_v4 = FALSE;
}

void
ser_save_game (scr_gameref_t game,
               scr_write_callbackref_t callback, void *opaque)
{
  ser_save_game_internal (game, callback, opaque, FALSE);
}

void
ser_save_game_to_file (scr_gameref_t game,
                       scr_write_callbackref_t callback, void *opaque)
{
  ser_save_game_internal (game, callback, opaque, TRUE);
}


/*
 * ser_save_game_prompted()
 *
 * Serialize a game and save its state, requesting a save stream from
 * the user.
 */
scr_bool
ser_save_game_prompted (scr_gameref_t game)
{
  void *opaque;

  /*
   * Open an output stream, and if successful, save a game using the opaque
   * value returned.
   */
  opaque = if_open_saved_game (TRUE);
  if (opaque)
    {
      ser_save_game_to_file (game, if_write_saved_game, opaque);
      if_close_saved_game (opaque);
      return TRUE;
    }

  return FALSE;
}


/* TAS input file line counter. */
static scr_tafref_t ser_tas = (scr_tafref_t) NULL;
static scr_int ser_tasline = 0;

/* Restore error jump buffer. */
static jmp_buf ser_tas_error;

/*
 * ser_get_string()
 * ser_get_int()
 * ser_get_uint()
 * ser_get_boolean()
 *
 * Wrapper round obtaining the next TAS file line, with variants to convert
 * the line content into an appropriate type.
 */
static const scr_char *
ser_get_string (void)
{
  const scr_char *string;

  /* Get the next line, and complain if absent. */
  string = taf_next_line (ser_tas);
  if (!string)
    {
      scr_error ("ser_get_string: out of TAS data at line %ld\n", ser_tasline);
      scr_longjmp (ser_tas_error, 1);
    }

  ser_tasline++;
  return string;
}

static scr_int
ser_get_int (void)
{
  const scr_char *string;
  scr_int value;

  /* Get line, and scan for a single integer; return it. */
  string = ser_get_string ();
  if (sscanf (string, "%ld", &value) != 1)
    {
      scr_error ("ser_get_int:"
                " invalid integer at line %ld\n", ser_tasline - 1);
      scr_longjmp (ser_tas_error, 1);
    }

  return value;
}

/*
 * ser_restore_battle_attributes()
 *
 * Read back the version-2 mutable battle attributes of one character.  The
 * matching writer no longer exists: current saves interleave the battle block
 * (ser_restore_battle_block below), so this field order matches nothing we
 * write today and only runs against saves from the removed v2 writer.  Verify
 * against a real legacy v2 save before changing it.
 */
static void
ser_restore_battle_attributes (scr_battle_t *battle)
{
  scr_int slot;

  battle->attitude = ser_get_int ();
  battle->maxstamina = ser_get_int ();
  battle->speed = ser_get_int ();
  for (slot = 0; slot < BATTLE_ATTR_COUNT; slot++)
    {
      battle->lo[slot] = ser_get_int ();
      battle->hi[slot] = ser_get_int ();
      battle->max[slot] = ser_get_int ();
    }
  battle->seeded = TRUE;
}

/*
 * ser_restore_battle_block()
 *
 * Read back one character's interleaved ADRIFT battle block, in the order
 * ser_save_battle_block() wrote it, restoring live stamina and (for the player)
 * the wielded weapon or (for an NPC) the attack counter as it goes.
 */
static void
ser_restore_battle_block (scr_gameref_t game, scr_int npc)
{
  scr_battle_t *battle = (npc < 0) ? gs_player_battle (game)
                                  : gs_npc_battle (game, npc);
  scr_int index_;

  if (npc >= 0)
    battle->attitude = ser_get_int ();
  battle->maxstamina = ser_get_int ();
  if (npc < 0)
    gs_set_playerstamina (game, ser_get_int ());
  else
    gs_set_npc_stamina (game, npc, ser_get_int ());
  for (index_ = 0; index_ < BATTLE_ATTR_COUNT; index_++)
    {
      const scr_int slot = SER_BATTLE_SLOTS[index_];

      /* A pre-4.0 save stores Strength and Defence only, and no "lo" bound;
       * what it omits keeps the values battle_start() seeded, which is what
       * the 3.9 Runner effectively has too -- it never uses them. */
      if (ser_pre_v4 && slot != BATTLE_STRENGTH && slot != BATTLE_DEFENSE)
        continue;

      battle->max[slot] = ser_get_int ();
      battle->hi[slot] = ser_get_int ();
      if (!ser_pre_v4)
        battle->lo[slot] = ser_get_int ();
    }
  if (npc < 0)
    gs_set_playerwield (game, ser_get_int ());
  else
    {
      battle->speed = ser_get_int ();
      gs_set_npc_attackcounter (game, npc, ser_get_int ());
    }
  battle->seeded = TRUE;
}

/*
 * ser_reject_if()
 *
 * Reject the save currently being restored (via the restore error longjmp) when
 * a value read from the untrusted save file is out of range for this game.  The
 * gs_* accessors only assert their index arguments, which is a no-op under
 * NDEBUG, so without these explicit checks a corrupt save could plant
 * out-of-range room/object/NPC indices that later read or write game-state
 * arrays out of bounds.
 */
static void
ser_reject_if (scr_bool out_of_range)
{
  if (out_of_range)
    {
      scr_error ("ser_reject_if:"
                 " index out of range at line %ld\n", ser_tasline - 1);
      scr_longjmp (ser_tas_error, 1);
    }
}

/*
 * ser_object_position_valid() / ser_object_parent_valid()
 *
 * Validate a dynamic object's restored position and parent.  A position is one
 * of the OBJ_* sentinels or a 1-based room number.  A parent is an object index
 * for on/in-object positions, an NPC index for held/worn/part-of-NPC positions
 * (or -1 for part-of-player), and is unused (so unconstrained) otherwise.
 */
static scr_bool
ser_object_position_valid (scr_gameref_t game, scr_int position)
{
  switch (position)
    {
    case OBJ_HIDDEN: case OBJ_HELD_PLAYER: case OBJ_HELD_NPC:
    case OBJ_WORN_PLAYER: case OBJ_WORN_NPC: case OBJ_PART_NPC:
    case OBJ_ON_OBJECT: case OBJ_IN_OBJECT:
      return TRUE;
    default:
      return position >= 1 && position <= gs_room_count (game);
    }
}

static scr_bool
ser_object_parent_valid (scr_gameref_t game, scr_int position, scr_int parent)
{
  switch (position)
    {
    case OBJ_ON_OBJECT: case OBJ_IN_OBJECT:
      return parent >= 0 && parent < gs_object_count (game);
    case OBJ_HELD_NPC: case OBJ_WORN_NPC:
      return parent >= 0 && parent < gs_npc_count (game);
    case OBJ_PART_NPC:
      return parent == -1 || (parent >= 0 && parent < gs_npc_count (game));
    default:
      return TRUE;
    }
}

/*
 * ser_restore_object_location()
 *
 * Read an object's location.  In 4.0 Runner format a static object is a room
 * list (count then that many room indices), whose values we discard -- a static
 * object's location is taken from its bundle "Where" list, and relocated-static
 * state is not separately persisted (as in legacy SCARIER saves).  A pre-4.0
 * save has no room list: a static is a bare position, discarded for the same
 * reason.  A dynamic object, or any object in a legacy save, is a single
 * position integer.
 */
static void
ser_restore_object_location (scr_gameref_t game, scr_int object,
                             scr_bool runner_format)
{
  if (ser_pre_v4 && obj_is_static (game, object))
    (void) ser_get_int ();
  else if (runner_format && !ser_pre_v4 && obj_is_static (game, object))
    {
      scr_int count = ser_get_int (), index_;
      ser_reject_if (count < 0 || count > gs_room_count (game));
      for (index_ = 0; index_ < count; index_++)
        (void) ser_get_int ();
    }
  else
    game->objects[object].position = ser_get_int ();
}

static scr_uint
ser_get_uint (void)
{
  const scr_char *string;
  scr_uint value;

  /* Get line, and scan for a single integer; return it. */
  string = ser_get_string ();
  if (sscanf (string, "%lu", &value) != 1)
    {
      scr_error ("ser_get_uint:"
                " invalid integer at line %ld\n", ser_tasline - 1);
      scr_longjmp (ser_tas_error, 1);
    }

  return value;
}

static scr_bool
ser_get_boolean (void)
{
  const scr_char *string;
  scr_uint value;

  /*
   * Get line, and scan for a single integer; check it's a valid-looking flag,
   * and return it.
   */
  string = ser_get_string ();
  if (sscanf (string, "%lu", &value) != 1)
    {
      scr_error ("ser_get_boolean:"
                " invalid boolean at line %ld\n", ser_tasline - 1);
      scr_longjmp (ser_tas_error, 1);
    }
  if (value != 0 && value != 1)
    {
      scr_error ("ser_get_boolean:"
                " warning: suspect boolean at line %ld\n", ser_tasline - 1);
    }

  return value != 0;
}


/*
 * ser_load_game()
 *
 * Load a serialized game into the given game by repeated calls to the
 * callback() function.
 */
scr_bool
ser_load_game (scr_gameref_t game,
               scr_read_callbackref_t callback, void *opaque)
{
  static scr_var_setref_t new_vars;  /* For setjmp safety */
  static scr_gameref_t new_game;     /* For setjmp safety */

  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int index_, var_count;
  const scr_char *gamename;
  scr_bool runner_format = FALSE;

  /* Create a TAF (TAS) reference from callbacks, for reader functions. */
  ser_tas = taf_create_tas (callback, opaque);
  if (!ser_tas)
    return FALSE;

  /*
   * The container tells us the layout: only run390.exe -- and ser_save_game_-
   * to_file() for a pre-4.0 game -- writes a PRNG-obfuscated save, so an
   * obfuscated stream is a pre-4.0 Runner save.  Everything else is a zlib
   * stream, either 4.0 Runner format or legacy SCARIER, told apart below by the
   * version line.  This keeps 4.0-format saves we wrote earlier for a 3.9 game
   * readable.
   */
  ser_pre_v4 = taf_get_version (ser_tas) < TAF_VERSION_400;
  runner_format = ser_pre_v4;

  /* Reset line counter for error messages. */
  ser_tasline = 1;

  new_game = NULL;
  new_vars = NULL;

  /* Set up error handling jump buffer, and handle errors. */
  if (scr_setjmp (ser_tas_error) != 0)
    {
      /* Destroy any temporary game and variables. */
      if (new_game)
        gs_destroy (new_game);
      if (new_vars)
        var_destroy (new_vars);

      /* Destroy the TAF (TAS) file and return fail status. */
      taf_destroy (ser_tas);
      ser_tas = NULL;
      ser_pre_v4 = FALSE;
      return FALSE;
    }

  /*
   * Read the first line.  If it begins with the ADRIFT v4 version-line lead
   * byte (0xAC) it is a Runner/new-format save: skip the version line (we accept
   * any version a v4 Runner wrote rather than insist on our own digits) and take
   * the GameName from the next line.  Otherwise the file is a legacy SCARIER save
   * whose first line is already the GameName.  See SER_VERSION_HEADER.
   */
  gamename = ser_get_string ();
  if (!ser_pre_v4 && (scr_byte) gamename[0] == SER_VERSION_LEAD)
    {
      runner_format = TRUE;
      gamename = ser_get_string ();
    }

  /*
   * Compare the saved GameName with the one in the game.  Fail if they don't
   * match exactly.  A tighter check than this would perhaps be preferable, say,
   * something based on the TAF file header, but this isn't in the save file
   * format.
   */
  vt_key[0].string = "Globals";
  vt_key[1].string = "GameName";
  if (strcmp (gamename, prop_get_string (bundle, "S<-ss", vt_key)) != 0)
    scr_longjmp (ser_tas_error, 1);

  /* Read and verify the counts in the saved game. */
  if (ser_get_int () != gs_room_count (game)
      || ser_get_int () != gs_object_count (game)
      || ser_get_int () != gs_task_count (game)
      || ser_get_int () != gs_event_count (game)
      || ser_get_int () != gs_npc_count (game))
    scr_longjmp (ser_tas_error, 1);

  /* Create a variables set and game to restore into. */
  new_vars = var_create (bundle);
  new_game = gs_create (new_vars, bundle, filter);
  var_register_game (new_vars, new_game);

  /* All set to load TAF (TAS) data into the new game. */

  /*
   * Seed the Battle System state up front so that the interleaved battle blocks
   * read below (Runner format) can overwrite it, and so that fields the Runner
   * does not persist (the stamina counters) keep a sane zero.  A no-op for
   * non-battle games.
   */
  battle_start (new_game);

  /* Restore the score. */
  new_game->score = ser_get_int ();

  /* Skip the player name (4.0 Runner format only; absent in legacy SCARIER
   * saves and in pre-4.0 Runner saves). */
  if (runner_format && !ser_pre_v4)
    (void) ser_get_string ();

  /* Restore the rest of the player block, validating untrusted indices. */
  {
    const scr_int playerroom = ser_get_int ();
    ser_reject_if (playerroom < 0 || playerroom > gs_room_count (new_game));
    gs_set_playerroom (new_game, playerroom - 1);
  }
  {
    const scr_int playerparent = ser_get_int ();
    ser_reject_if (playerparent < -1
                   || playerparent >= gs_object_count (new_game));
    gs_set_playerparent (new_game, playerparent);
  }
  gs_set_playerposition (new_game, ser_get_int ());

  /* Skip player gender. */
  (void) ser_get_int ();

  /* Skip encumbrance details, not currently maintained by the game. */
  (void) ser_get_int ();
  (void) ser_get_int ();
  (void) ser_get_int ();
  (void) ser_get_int ();

  /* Runner format: the player's interleaved battle block follows encumbrance. */
  if (runner_format && battle_is_enabled (new_game))
    ser_restore_battle_block (new_game, -1);

  /* Restore rooms information. */
  for (index_ = 0; index_ < gs_room_count (new_game); index_++)
    gs_set_room_seen (new_game, index_, ser_get_boolean ());

  /* Restore objects information. */
  for (index_ = 0; index_ < gs_object_count (new_game); index_++)
    {
      scr_int openable, currentstate;

      /* Bypass mutators for position and parent.  Fix later? */
      ser_restore_object_location (new_game, index_, runner_format);
      gs_set_object_seen (new_game, index_, ser_get_boolean ());
      new_game->objects[index_].parent = ser_get_int ();

      /*
       * Validate a dynamic object's restored position and parent against the
       * current game.  A static object's position is taken from its bundle
       * (not the save), and its parent is never used as an array index.
       */
      if (!obj_is_static (new_game, index_))
        {
          const scr_int position = new_game->objects[index_].position;
          const scr_int parent = new_game->objects[index_].parent;
          ser_reject_if (!ser_object_position_valid (new_game, position));
          ser_reject_if (!ser_object_parent_valid (new_game, position, parent));
        }

      vt_key[0].string = "Objects";
      vt_key[1].integer = index_;
      vt_key[2].string = "Openable";
      openable = prop_get_integer (bundle, "I<-sis", vt_key);
      if (openable == 0)
        gs_set_object_openness (new_game, index_, 0);
      else if (ser_pre_v4)
        gs_set_object_openness (new_game, index_,
                                ser_openness_pre_v4 (ser_get_int ()));
      else
        gs_set_object_openness (new_game, index_, ser_get_int ());

      vt_key[2].string = "CurrentState";
      currentstate = prop_get_integer (bundle, "I<-sis", vt_key);
      gs_set_object_state (new_game, index_,
                           currentstate != 0 ? ser_get_int () : 0);

      /* Pre-4.0 does not store "unmoved"; the value gs_create() derived from
       * OnlyWhenNotMoved stands, exactly as it does in the 3.9 Runner. */
      if (!ser_pre_v4)
        gs_set_object_unmoved (new_game, index_, ser_get_boolean ());
    }

  /* Restore tasks information. */
  for (index_ = 0; index_ < gs_task_count (new_game); index_++)
    {
      gs_set_task_done (new_game, index_, ser_get_boolean ());
      gs_set_task_scored (new_game, index_, ser_get_boolean ());
    }

  /* Restore events information. */
  for (index_ = 0; index_ < gs_event_count (new_game); index_++)
    {
      scr_int startertype, task;

      /* Restore first event details. */
      gs_set_event_time (new_game, index_, ser_get_int ());
      task = ser_get_int ();
      gs_set_event_state (new_game, index_, ser_get_int () + 1);

      /* Verify and restore the starter task, if any. */
      if (task > 0)
        {
          vt_key[0].string = "Events";
          vt_key[1].integer = index_;
          vt_key[2].string = "StarterType";
          startertype = prop_get_integer (bundle, "I<-sis", vt_key);
          if (startertype != 3)
            scr_longjmp (ser_tas_error, 1);

          /* Restore task state (task is a 1-based index from the save). */
          ser_reject_if (task > gs_task_count (new_game));
          gs_set_task_done (new_game, task - 1, ser_get_boolean ());
        }
      else
        (void) ser_get_boolean ();
    }

  /* Restore NPCs information. */
  for (index_ = 0; index_ < gs_npc_count (new_game); index_++)
    {
      scr_int walk;

      {
        const scr_int location = ser_get_int ();
        ser_reject_if (location < 0 || location > gs_room_count (new_game));
        gs_set_npc_location (new_game, index_, location);
      }
      gs_set_npc_seen (new_game, index_, ser_get_boolean ());

      /* Runner format: the NPC's battle block follows "seen", before walks. */
      if (runner_format && battle_is_enabled (new_game))
        ser_restore_battle_block (new_game, index_);

      /* Pre-4.0 stores no walk steps, so the walk restarts from gs_create()'s
       * initial state -- the same thing the 3.9 Runner does. */
      if (!ser_pre_v4)
        {
          for (walk = 0; walk < gs_npc_walkstep_count (new_game, index_);
               walk++)
            gs_set_npc_walkstep (new_game, index_, walk, ser_get_int ());
        }
    }

  /* Restore each variable. */
  var_count = ser_variable_count (bundle);

  for (index_ = 0; index_ < var_count; index_++)
    {
      const scr_char *name;
      scr_int var_type = ser_variable_at (bundle, index_, &name);

      switch (var_type)
        {
        case TAFVAR_NUMERIC:
          var_put_integer (new_vars, name, ser_get_int ());
          break;

        case TAFVAR_STRING:
          var_put_string (new_vars, name, ser_get_string ());
          break;

        default:
          scr_fatal ("ser_load_game: unknown variable type, %ld\n", var_type);
        }
    }

  /* Restore timing information. */
  var_set_elapsed_seconds (new_vars, ser_get_uint ());

  /* Restore turns count. */
  new_game->turns = (scr_int) ser_get_uint ();

  /*
   * Legacy battle saves (no version header) carry their combat state in a
   * private trailing block introduced by a sentinel on the line after the turns
   * count.  Runner-format saves carry it inline (read above), so we only look
   * for the trailing block when reading a legacy save.  battle_start() seeded
   * fresh state earlier, so a legacy save that predates the battle extension --
   * or one with no trailing block -- simply keeps that re-rolled state.
   * taf_next_line returns NULL at end of data without faulting, so the peek is
   * safe even when nothing follows the turns count.
   */
  if (!runner_format && battle_is_enabled (new_game))
    {
      const scr_char *marker = taf_next_line (ser_tas);
      const scr_bool is_v2 = marker
                            && strcmp (marker, SER_BATTLE_MARKER) == 0;
      const scr_bool is_v1 = marker
                            && strcmp (marker, SER_BATTLE_MARKER_V1) == 0;

      if (is_v1 || is_v2)
        {
          ser_tasline++;
          gs_set_playerstamina (new_game, ser_get_int ());
          gs_set_playerstaminacounter (new_game, ser_get_int ());
          gs_set_playerwield (new_game, ser_get_int ());
          if (is_v2)
            ser_restore_battle_attributes (gs_player_battle (new_game));
          for (index_ = 0; index_ < gs_npc_count (new_game); index_++)
            {
              gs_set_npc_stamina (new_game, index_, ser_get_int ());
              gs_set_npc_staminacounter (new_game, index_, ser_get_int ());
              gs_set_npc_attackcounter (new_game, index_, ser_get_int ());
              if (is_v2)
                ser_restore_battle_attributes (gs_npc_battle (new_game,
                                                              index_));
            }
        }
    }

  /*
   * Resources tweak -- set requested to match those in the current game
   * so that they remain unchanged by the gs_copy() of new_game onto
   * game.  This way, both the requested and the active resources in the
   * game are unchanged by restore.
   */
  new_game->requested_sound = game->requested_sound;
  new_game->requested_graphic = game->requested_graphic;

  /*
   * If we got this far, we successfully restored the game from the file.
   * As our final act, copy the new game onto the old one.
   */
  new_game->temporary = game->temporary;
  new_game->undo = game->undo;
  gs_copy (game, new_game);

  /* Done with the temporary game and variables. */
  gs_destroy (new_game);
  var_destroy (new_vars);

  /* Done with TAF (TAS) file; destroy it and return successfully. */
  taf_destroy (ser_tas);
  ser_tas = NULL;
  ser_pre_v4 = FALSE;
  return TRUE;
}


/*
 * ser_load_game_prompted()
 *
 * Load a serialized game into the given game, requesting a restore
 * stream from the user.
 */
scr_bool
ser_load_game_prompted (scr_gameref_t game)
{
  void *opaque;

  /*
   * Open an input stream, and if successful, try to load a game using
   * the opaque value returned and the saved game callback.
   */
  opaque = if_open_saved_game (FALSE);
  if (opaque)
    {
      scr_bool status;

      status = ser_load_game (game, if_read_saved_game, opaque);
      if_close_saved_game (opaque);
      return status;
    }

  return FALSE;
}
