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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "zlib.h"
#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "sctafpar.h"
#include "scvars.h"
#include "scprintf.h"
#include "scgamest_api.h"
#include "scbattle.h"
#include "scobjcts.h"
#include "scserial.h"
#include "scinterf.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const scr_char NEWLINE = '\n';
static const scr_char CARRIAGE_RETURN = '\r';


/*
 * Legacy sentinel that introduced Scarier's old private Battle System block,
 * once written (for battle games only) as extra lines after the ADRIFT turns
 * count.  Current saves no longer emit it: the live battle state is interleaved
 * inline in the player and NPC records, matching the Runner's own layout (see
 * ser_save_battle_block and the version-header note below).  The marker is kept
 * only so ser_load_game can still read those older Scarier saves; it is
 * deliberately non-numeric so it can never be mistaken for a numeric ADRIFT
 * field.
 *
 * The Runner's layout (reverse-engineered from run400.exe savegame @477318)
 * interleaves battle data -- a 15-field block right after the player fields,
 * and a 17-field block inside each NPC's record (after "seen", before the walk
 * steps), both gated on the battle-system flag, with live stamina near the
 * front of each block and the Max/Hi/Lo attributes that the "Change
 * <attribute>" task action can alter following it.  Newer Scarier saves
 * reproduce that layout (and emit the leading version line below), so they are
 * byte-loadable by the Runner; older saves that predate the change carry the
 * trailing marker block instead.
 *
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
 *     Print 1, Chr(172) & CStr(App.Major) _
 *              & Format(Minor,"000") & Format(Rev,"00")
 *
 * i.e. the byte 0xAC ('<not>', Chr(172)) followed by the Major version, the
 * Minor zero-padded to three digits, and the Revision zero-padded to two.  For
 * the shipped run400.exe, which reports file version 4.0.0.52, that is exactly
 * "\xAC" "400052".  Scarier historically omitted this line entirely (its stream
 * started at GameName), so a Runner save and a Scarier save were mutually
 * unreadable.  We now emit the header so the Runner will accept our saves, and
 * on read we treat the leading 0xAC byte as an unambiguous discriminator: a
 * line beginning with it is a Runner/new-format save whose version line we
 * skip; any other first line is a legacy SCARE save whose first line is the
 * GameName.
 */
static const scr_byte SER_VERSION_LEAD = 0xAC;
static const scr_char *const SER_VERSION_HEADER = "\xAC" "400052";

enum { BUFFER_SIZE = 4096 };


/*
 * Pre-4.0 saved games.
 *
 * ADRIFT 3.8 and 3.9 store a saved game in their own layout, and obfuscate it
 * with the Visual Basic PRNG rather than deflating it.  When ser.pre_v4 is set
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
 * Raw (uncompressed) mode for the next serialization run.  The undo ring's
 * in-memory memos are a self-contained round trip that never touches disk and
 * never interoperates with a Runner, so they can skip zlib entirely: profiling
 * put the per-turn deflate at ~20-48% of the interpreter, and it buys nothing
 * for a buffer we rewrite every 16 turns and read back ourselves.  memo_save_-
 * game()/memo_load_game() set this around their ser_save_game()/ser_load_game()
 * calls; the save side (ser_flush) then passes bytes straight through, and the
 * load side (ser_load_game) reads them back via taf_create_tas_raw().  File
 * saves never set it, so their zlib format -- and the save-file sniffer -- are
 * untouched.  Both entry points self-clear it, like ser.pre_v4.
 */


/*
 * The serializer's module state, in one instance (ser).  It is grouped by
 * lifetime:
 *
 *   pre_v4    the pre-4.0 layout switch described above.  Per run: the save
 *             side sets it from the bundle, the load side from the container
 *             format, and both clear it on the way out.
 *   raw_memo  the raw (uncompressed) mode described above.  Per run, armed by
 *             ser_set_raw_memo() just before, self-cleared by both entry
 *             points.
 *   write     a save run's output buffer and the sink it drains into (the
 *             caller's write callback and its opaque pointer).  Set by
 *             ser_save_game_internal(), emptied by ser_flush(), cleared by
 *             the same function on the way out.
 *   deflate   the zlib stream and its output buffer for the 4.0 layout, live
 *             between ser_flush() calls of one run; the final flush, or
 *             ser_flush_abort(), ends and frees it.
 *   cache     the save path's per-game memo of immutable bundle answers (see
 *             ser_synchronize_cache()).  Per game: synchronised on each save,
 *             dropped by gs_destroy() through ser_forget_game().
 *   read      a load run's TAS cursor: the open .tas and the line counter the
 *             reader errors cite.  Set by ser_load_game(), freed by
 *             ser_load_game_finish().
 *
 * Nothing here is saved -- this is the module that does the saving.  The
 * only group that outlives a run is cache, and it is keyed by game.
 */
struct scr_ser_state_s
{
  scr_bool pre_v4 = FALSE;
  scr_bool raw_memo = FALSE;

  struct
  {
    scr_byte *buffer = nullptr;
    scr_int length = 0;
    scr_write_callbackref_t callback = nullptr;
    void *opaque = nullptr;
  } write;

  struct
  {
    scr_bool initialized = FALSE;
    scr_byte *out_buffer = nullptr;
    scr_int out_size = 0;
    z_stream stream;
  } deflate;

  struct
  {
    const void *game = nullptr;
    std::vector<std::vector<scr_int> > object_rooms;
    std::vector<scr_int> event_task;          /* -1 = not built */
    scr_int var_count = -1;
    std::vector<const scr_char *> var_name;   /* NULL = unbuilt */
    std::vector<scr_int> var_type;
  } cache;

  struct
  {
    scr_tafref_t tas = nullptr;
    scr_int line = 0;
  } read;
};
typedef struct scr_ser_state_s scr_ser_state_t;

static scr_ser_state_t ser;


/*
 * ser_set_raw_memo()
 *
 * Select or clear raw mode for the next serialization run.
 */
void
ser_set_raw_memo (scr_bool raw)
{
  ser.raw_memo = raw;
}

/*
 * ser_flush()
 * ser_flush_abort()
 * ser_buffer_character()
 *
 * Flush pending buffer contents; abandon a serialization run part way through,
 * so the next one starts clean; add a character to the buffer.
 */
static void
ser_flush (scr_bool is_final)
{
  scr_int status;

  /*
   * A raw memo is passed straight through, uncompressed and unobfuscated --
   * taf_create_tas_raw() reads it back verbatim.  A pre-4.0 save is not
   * compressed at all either: the plain text goes out xor'd with the PRNG
   * keystream, which taf_obfuscate_reset() started in ser_save_game().
   * Neither touches the deflate state, so the 4.0 path below is unaffected.
   */
  if (ser.raw_memo || ser.pre_v4)
    {
      if (ser.write.length > 0)
        {
          if (!ser.raw_memo)
            taf_obfuscate_buffer (ser.write.buffer, ser.write.length);
          ser.write.callback (ser.write.opaque,
                              ser.write.buffer, ser.write.length);
          ser.write.length = 0;
        }

      if (is_final)
        {
          scr_free (ser.write.buffer);
          ser.write.buffer = nullptr;
        }
      return;
    }

  /* If this is an initial call, initialize deflation. */
  if (!ser.deflate.initialized)
    {
      /* Allocate an initial output buffer. */
      ser.deflate.out_size = BUFFER_SIZE;
      ser.deflate.out_buffer
        = (decltype(ser.deflate.out_buffer)) scr_malloc (ser.deflate.out_size);

      /* Initialize Zlib deflation functions. */
      ser.deflate.stream.next_out = ser.deflate.out_buffer;
      ser.deflate.stream.avail_out = ser.deflate.out_size;
      ser.deflate.stream.next_in = ser.write.buffer;
      ser.deflate.stream.avail_in = 0;

      ser.deflate.stream.zalloc = Z_NULL;
      ser.deflate.stream.zfree = Z_NULL;
      ser.deflate.stream.opaque = Z_NULL;

      status = deflateInit (&ser.deflate.stream, Z_DEFAULT_COMPRESSION);
      if (status != Z_OK)
        {
          scr_error ("ser_flush: deflateInit: error %ld\n", status);
          ser.write.length = 0;

          scr_free (ser.deflate.out_buffer);
          ser.deflate.out_buffer = nullptr;
          ser.deflate.out_size = 0;
          return;
        }

      ser.deflate.initialized = TRUE;
    }

  /* Deflate data from the current output buffer. */
  ser.deflate.stream.next_in = ser.write.buffer;
  ser.deflate.stream.avail_in = ser.write.length;

  /* Loop while deflate output is pending and buffer not emptied. */
  while (TRUE)
    {
      scr_int in_bytes, out_bytes;

      /* Compress stream data, with finish if this is the final flush. */
      if (is_final)
        status = deflate (&ser.deflate.stream, Z_FINISH);
      else
        status = deflate (&ser.deflate.stream, Z_NO_FLUSH);
      if (status != Z_STREAM_END && status != Z_OK)
        {
          scr_error ("ser_flush: deflate: error %ld\n", status);
          ser.write.length = 0;

          scr_free (ser.deflate.out_buffer);
          ser.deflate.out_buffer = nullptr;
          ser.deflate.out_size = 0;
          ser.deflate.initialized = FALSE;
          return;
        }

      /* Calculate bytes used, and output. */
      in_bytes = ser.write.length - ser.deflate.stream.avail_in;
      out_bytes = ser.deflate.out_size - ser.deflate.stream.avail_out;

      /* See if compressed data is available. */
      if (out_bytes > 0)
        {
          /* Write it to save file output through the callback. */
          ser.write.callback (ser.write.opaque,
                              ser.deflate.out_buffer, out_bytes);

          /* Reset deflation stream for available space. */
          ser.deflate.stream.next_out = ser.deflate.out_buffer;
          ser.deflate.stream.avail_out = ser.deflate.out_size;
        }

      /* Remove consumed data from the input buffer. */
      if (in_bytes > 0)
        {
          /* Move any unused data, and reduce length. */
          memmove (ser.write.buffer,
                   ser.write.buffer + in_bytes, ser.write.length - in_bytes);
          ser.write.length -= in_bytes;

          /* Reset deflation stream for consumed data. */
          ser.deflate.stream.next_in = ser.write.buffer;
          ser.deflate.stream.avail_in = ser.write.length;
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
      status = deflateEnd (&ser.deflate.stream);
      if (status != Z_OK)
        scr_error ("ser_flush: warning: deflateEnd: error %ld\n", status);

      if (ser.write.length != 0)
        {
          scr_error ("ser_flush: warning: deflate missed data\n");
          ser.write.length = 0;
        }

      /* Free the allocated compression buffer. */
      scr_free (ser.write.buffer);
      ser.write.buffer = nullptr;

      /*
       * Free output buffer, and reset flag for reinitialization on the next
       * call.
       */
      scr_free (ser.deflate.out_buffer);
      ser.deflate.out_buffer = nullptr;
      ser.deflate.out_size = 0;
      ser.deflate.initialized = FALSE;
    }
}

static void
ser_flush_abort (void)
{
  if (ser.deflate.initialized)
    {
      (void) deflateEnd (&ser.deflate.stream);
      ser.deflate.initialized = FALSE;
    }
  scr_free (ser.deflate.out_buffer);
  ser.deflate.out_buffer = nullptr;
  ser.deflate.out_size = 0;

  scr_free (ser.write.buffer);
  ser.write.buffer = nullptr;
  ser.write.length = 0;
}

static void
ser_buffer_character (scr_char character)
{
  /* Allocate the buffer if not yet done. */
  if (!ser.write.buffer)
    {
      assert (ser.write.length == 0);
      ser.write.buffer = (decltype(ser.write.buffer)) scr_malloc (BUFFER_SIZE);
    }

  /* Add to the buffer, with intermediate flush if filled. */
  ser.write.buffer[ser.write.length++] = character;
  if (ser.write.length == BUFFER_SIZE)
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
  if (!ser.write.buffer)
    {
      assert (ser.write.length == 0);
      ser.write.buffer = (decltype(ser.write.buffer)) scr_malloc (BUFFER_SIZE);
    }

  /*
   * Copy in chunks, flushing eagerly whenever the buffer fills -- every
   * helper (ser_buffer_character included) relies on the buffer never being
   * left exactly full.  The undo snapshot funnels the whole game state
   * through here every turn, so this is memcpy rather than the old
   * per-character loop; the deflate stream sees the same byte sequence
   * either way (Z_NO_FLUSH output depends only on the data).
   */
  while (offset < length)
    {
      scr_int chunk;

      chunk = BUFFER_SIZE - ser.write.length;
      if (chunk > length - offset)
        chunk = length - offset;
      memcpy (ser.write.buffer + ser.write.length, buffer + offset, chunk);
      ser.write.length += chunk;
      offset += chunk;

      if (ser.write.length == BUFFER_SIZE)
        ser_flush (FALSE);
    }
}

/* Buffer length bytes of text, followed by DOS style end-of-line. */
static void
ser_buffer_line (const scr_char *text, scr_int length)
{
  /*
   * The undo snapshot writes thousands of short lines a turn, so one that
   * fits with room to spare (the buffer is never left exactly full) goes in
   * with a single copy.
   */
  if (ser.write.buffer && length + 2 < BUFFER_SIZE - ser.write.length)
    {
      scr_byte *const out = ser.write.buffer + ser.write.length;

      memcpy (out, text, length);
      out[length] = CARRIAGE_RETURN;
      out[length + 1] = NEWLINE;
      ser.write.length += length + 2;
      return;
    }

  ser_buffer_buffer (text, length);
  ser_buffer_character (CARRIAGE_RETURN);
  ser_buffer_character (NEWLINE);
}

static void
ser_buffer_string (const scr_char *string)
{
  /* Buffer string, followed by DOS style end-of-line. */
  ser_buffer_line (string, strlen (string));
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
  ser_buffer_line (buffer, out - buffer);
}

static void
ser_buffer_int_special (scr_int value)
{
  scr_char buffer[32];
  scr_char *out = buffer;

  /*
   * Weirdo formatting for compatibility: "% ld " -- a sign column (space for
   * non-negative, '-' for negative) and a trailing space.
   */
  *out++ = value < 0 ? '-' : ' ';
  out = ser_render_ulong (out, value < 0 ? -(unsigned long) value
                                         : (unsigned long) value);
  *out++ = ' ';
  ser_buffer_line (buffer, out - buffer);
}

static void
ser_buffer_uint (scr_uint value)
{
  scr_char buffer[32];
  scr_char *out;

  /* Convert to a string ("%lu") and buffer that. */
  out = ser_render_ulong (buffer, (unsigned long) value);
  ser_buffer_line (buffer, out - buffer);
}

static void
ser_buffer_boolean (scr_bool boolean)
{
  /* Write a 1 for TRUE, 0 for FALSE. */
  ser_buffer_string (boolean ? "1" : "0");
}


/*
 * The Runner stores the four ranged battle attributes in the save in the
 * order Strength, Defence, Accuracy, Agility -- Scarier's scr_battle_t slots
 * 0, 2, 1, 3 (BATTLE_STRENGTH/ACCURACY/DEFENSE/AGILITY) -- and writes each as
 * max, hi, lo.  (Order derived from real run400.exe saves; the original
 * capture notes are no longer in the tree, so re-confirm against a Runner
 * save before changing it.)
 */
static const scr_int SER_BATTLE_SLOTS[BATTLE_ATTR_COUNT] = {
  BATTLE_STRENGTH, BATTLE_DEFENSE, BATTLE_ACCURACY, BATTLE_AGILITY
};


/*
 * ser_save_battle_block()
 *
 * Buffer one character's interleaved ADRIFT battle block.  For an NPC
 * (npc >= 0) this is 17 values: attitude, max stamina, live stamina, then
 * each attribute's (max, hi, lo) in SER_BATTLE_SLOTS order, then speed and
 * the attack counter.  For the player (npc < 0) it is 15 values: the
 * attitude and speed/attack-counter fields are absent, and the block ends
 * with the wielded-weapon object (the Runner's player sub-struct index 38).
 * The Runner prints these with the raw VB Print numeric format, i.e.
 * space-padded; ser_buffer_int_special matches it byte for byte.
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
  void (*const emit) (scr_int) = ser.pre_v4 ? ser_buffer_int
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
      if (ser.pre_v4 && slot != BATTLE_STRENGTH && slot != BATTLE_DEFENSE)
        continue;

      emit (battle->max[slot]);
      emit (battle->hi[slot]);
      if (!ser.pre_v4)
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
 *
 * ser.cache.object_rooms holds, per static object, the exact int sequence
 * its room list emits (leading count, then rooms); it is always non-empty
 * once built, so empty() means not built.
 */

/*
 * ser_synchronize_cache()
 *
 * Reset the cache if it was built for a different game, or not at all.
 */
static void
ser_synchronize_cache (scr_gameref_t game)
{
  if (ser.cache.game != game)
    {
      ser.cache.object_rooms.assign (gs_object_count (game),
                                     std::vector<scr_int> ());
      ser.cache.event_task.assign (gs_event_count (game), -1);
      ser.cache.var_count = -1;
      ser.cache.var_name.clear ();
      ser.cache.var_type.clear ();
      ser.cache.game = game;
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
  if (ser.cache.game == game)
    {
      ser.cache.game = nullptr;
      ser.cache.object_rooms.clear ();
      ser.cache.event_task.clear ();
      ser.cache.var_count = -1;
      ser.cache.var_name.clear ();
      ser.cache.var_type.clear ();
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
  std::vector<scr_int> &cached = ser.cache.object_rooms[object];

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
          GS_FOR_EACH_ROOM (game, room)
            {
              vt_key[4].integer = room + 1;
              if (prop_get_boolean (bundle, "B<-sissi", vt_key))
                count++;
            }
          cached.push_back (count);
          GS_FOR_EACH_ROOM (game, room)
            {
              vt_key[4].integer = room + 1;
              if (prop_get_boolean (bundle, "B<-sissi", vt_key))
                cached.push_back (room + 1);
            }
          break;

        case ROOMLIST_ALL_ROOMS:
          cached.push_back (gs_room_count (game));
          GS_FOR_EACH_ROOM (game, room)
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
  if (ser.pre_v4 || !obj_is_static (game, object))
    {
      ser_buffer_int (gs_object_position (game, object));
      return;
    }

  if (!gs_object_static_unmoved (game, object))
    {
      /* Relocated static object: present in its current room, or nowhere. */
      const scr_int position = gs_object_position (game, object);
      const std::vector<scr_int> &rooms = game->objects[object].static_rooms;

      /* ...or, once 4.0 finish moves have piled up, in all of these. */
      if (position != OBJ_HELD_PLAYER && !rooms.empty ())
        {
          std::vector<scr_int>::size_type index_;

          ser_buffer_int_special (rooms.size ());
          for (index_ = 0; index_ < rooms.size (); index_++)
            ser_buffer_int_special (rooms[index_] + 1);
        }
      else if (position >= 1 && position <= gs_room_count (game))
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
  *name = var_indexed_name (bundle, index_);
  return prop_get_indexed_integer (bundle, "Variables", index_, "Type");
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
  return prop_get_taf_version (bundle) < TAF_VERSION_400;
}


/*
 * ser_save_header()
 * ser_save_player()
 * ser_save_objects()
 * ser_save_events()
 * ser_save_npcs()
 * ser_save_variables()
 * ser_save_game_body()
 * ser_save_game_internal()
 * ser_save_game()
 * ser_save_game_to_file()
 *
 * Serialize a game and save its state using the given callback and opaque.
 * The body writes the record stream, a section function at a time, in the
 * Runner's order; the internal wrapper selects the layout and manages the
 * writer state around it.
 *
 * ser_save_game_to_file() is the one that produces a file for the player, and
 * for a pre-4.0 game it writes that game's native save format so the original
 * Runner can read it.  ser_save_game() always writes the full 4.0 layout, and
 * is what the undo ring and the Spatterlight autosave use: the pre-4.0 layout
 * is lossy (no walk steps, no Accuracy or Agility, no "unmoved" flags) in ways
 * an undo must not be.  Both are read back by ser_load_game().
 */
/* The version line, the game name, the entity counts and the score. */
static void
ser_save_header (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2];

  /*
   * Write the ADRIFT v4 version line first, so the real Runner (and other v4
   * interpreters) recognise the file as one of theirs.  See SER_VERSION_HEADER.
   * Pre-4.0 saves have no version line at all.
   */
  if (!ser.pre_v4)
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
}

/* The player block, encumbrance, and the player's battle block. */
static void
ser_save_player (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2];

  /*
   * Write the player block in ADRIFT layout: the player name (the Runner's
   * first player field, which older Scarier saves and pre-4.0 saves omit),
   * then room, parent, position and gender.
   */
  vt_key[0].string = "Globals";
  if (!ser.pre_v4)
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
   * carried size and weight.  We maintain these running totals (matching
   * the Runner's accounting, double-count and all), so we write the live values
   * just as the Runner does.  They are still recomputed from inventory on load,
   * so a reader that ignores them remains correct.
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
}

static void
ser_save_objects (scr_gameref_t game)
{
  scr_int index_;

  GS_FOR_EACH_OBJECT (game, index_)
    {
      ser_save_object_location (game, index_);
      ser_buffer_boolean (gs_object_seen (game, index_));
      ser_buffer_int (gs_object_parent (game, index_));
      if (gs_object_openness (game, index_) != 0)
        {
          if (ser.pre_v4)
            ser_buffer_int_special
                (ser_openness_pre_v4 (gs_object_openness (game, index_)));
          else
            ser_buffer_int (gs_object_openness (game, index_));
        }

      /*
       * A pre-4.0 game's objects all have CurrentState zero (the 3.9 grammar
       * defaults it), so this never fires for a pre-4.0 save.
       */
      if (gs_object_state (game, index_) != 0)
        ser_buffer_int (gs_object_state (game, index_));

      if (!ser.pre_v4)
        ser_buffer_boolean (gs_object_unmoved (game, index_));
    }
}

static void
ser_save_events (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int index_;

  GS_FOR_EACH_EVENT (game, index_)
    {
      scr_int startertype, task;

      /* Get starter task, if any (immutable, so cached after first read). */
      task = ser.cache.event_task[index_];
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

              /*
               * No such task is no starter task; the restore would refuse
               * the save over it.
               */
              if (task < 0 || task > gs_task_count (game))
                task = 0;
            }
          else
            task = 0;
          ser.cache.event_task[index_] = task;
        }

      /* Save event details. */
      ser_buffer_int (gs_event_time (game, index_));
      ser_buffer_int (task);
      ser_buffer_int (gs_event_state (game, index_) - 1);

      /*
       * The boolean beside each event is the Runner's starter-task snapshot
       * (run380 reads it straight back into event field 42 at 430F2B), not
       * the task's own completed flag, which the tasks block carries.
       */
      if (task > 0)
        ser_buffer_boolean (gs_event_taskstate (game, index_));
      else
        ser_buffer_boolean (FALSE);
    }
}

static void
ser_save_npcs (scr_gameref_t game)
{
  scr_int index_;

  GS_FOR_EACH_NPC (game, index_)
    {
      scr_int walk;

      /*
       * A dead NPC goes out as NPC_DEAD_LOCATION, the Runner's corpse
       * marker: its own save loop prints the room field raw (run400
       * Form1.frm @476F72, right before the seen byte at @476FA3), and
       * battle death is the only thing that ever puts the marker there.
       * See the `dead` flag in scgamest.h.
       */
      ser_buffer_int (gs_npc_dead (game, index_)
                      ? NPC_DEAD_LOCATION : gs_npc_location (game, index_));
      ser_buffer_boolean (gs_npc_seen (game, index_));

      /* The NPC's interleaved battle block sits after "seen", before walks. */
      if (battle_is_enabled (game))
        ser_save_battle_block (game, index_);

      /* Pre-4.0 stores no walk steps; a restored walk starts over there. */
      if (!ser.pre_v4)
        {
          for (walk = 0; walk < gs_npc_walkstep_count (game, index_); walk++)
            ser_buffer_int_special (gs_npc_walkstep (game, index_, walk));
        }
    }
}

/*
 * Save each variable.  The count and each name/type pair are immutable,
 * so they are cached after the first turn's walk; only the values change.
 */
static void
ser_save_variables (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int index_, var_count;

  var_count = ser.cache.var_count;
  if (var_count < 0)
    {
      var_count = ser_variable_count (bundle);
      ser.cache.var_count = var_count;
      ser.cache.var_name.assign (var_count, (const scr_char *) nullptr);
      ser.cache.var_type.assign (var_count, 0);
    }

  for (index_ = 0; index_ < var_count; index_++)
    {
      const scr_char *name = ser.cache.var_name[index_];
      scr_int var_type;

      if (name)
        var_type = ser.cache.var_type[index_];
      else
        {
          var_type = ser_variable_at (bundle, index_, &name);
          ser.cache.var_name[index_] = name;
          ser.cache.var_type[index_] = var_type;
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
}

static void
ser_save_game_body (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int index_;

  /*
   * Reset the immutable-input cache if this is not the game it was built
   * for; the undo ring calls this every turn, so the reads cached below
   * matter.
   */
  ser_synchronize_cache (game);

  ser_save_header (game);
  ser_save_player (game);

  /* Save rooms information. */
  GS_FOR_EACH_ROOM (game, index_)
    ser_buffer_boolean (gs_room_seen (game, index_));

  /* Save objects information. */
  ser_save_objects (game);

  /* Save tasks information. */
  GS_FOR_EACH_TASK (game, index_)
    {
      ser_buffer_boolean (gs_task_done (game, index_));
      ser_buffer_boolean (gs_task_scored (game, index_));
    }

  /* Save events information. */
  ser_save_events (game);

  /* Save NPCs information. */
  ser_save_npcs (game);

  ser_save_variables (game);

  /* Save timing information. */
  ser_buffer_uint (var_get_elapsed_seconds (vars));

  /* Save turns count. */
  ser_buffer_uint ((scr_uint) game->turns);

  /*
   * Note: the live Battle System state (stamina, attributes, attitudes, etc.)
   * is now written inline, in the player and NPC blocks, matching the
   * Runner's layout.  We no longer append the private trailing
   * SER_BATTLE_MARKER block; older Scarier saves that carry one are still read
   * (see ser_load_game).
   */
}

static void
ser_save_game_internal (scr_gameref_t game, scr_write_callbackref_t callback,
                        void *opaque, scr_bool runner_compatible)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  assert (callback);

  /* Store the callback and opaque references, for writer functions. */
  ser.write.callback = callback;
  ser.write.opaque = opaque;

  /*
   * Select the save layout, and start the keystream if it is the pre-4.0
   * one.
   */
  ser.pre_v4 = runner_compatible && ser_game_is_pre_v4 (bundle);
  if (ser.pre_v4)
    taf_obfuscate_reset ();

  /*
   * Serialize.  A fatal engine error part way through must not leave the
   * half-written run's state behind for the next call, so tear it down before
   * letting the error go on to the host boundary.
   */
  try
    {
      ser_save_game_body (game);
    }
  catch (const scr_fatal_error &)
    {
      ser_flush_abort ();
      ser.write.callback = nullptr;
      ser.write.opaque = nullptr;
      ser.pre_v4 = FALSE;
      ser.raw_memo = FALSE;
      throw;
    }

  /*
   * Flush the last buffer contents, and drop the callback and opaque
   * references.
   */
  ser_flush (TRUE);
  ser.write.callback = nullptr;
  ser.write.opaque = nullptr;
  ser.pre_v4 = FALSE;
  ser.raw_memo = FALSE;
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


/* Restore error, thrown by the readers below and caught in ser_load_game(). */
struct ser_tas_error_t { };


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
  string = taf_next_line (ser.read.tas);
  if (!string)
    {
      scr_error ("ser_get_string: out of TAS data at line %ld\n",
                ser.read.line);
      throw ser_tas_error_t ();
    }

  ser.read.line++;
  return string;
}

/*
 * ser_reject_number()
 *
 * Report a line that does not hold the number the caller wanted, and throw
 * the restore error.  The line has been consumed, so it is the one before.
 */
[[noreturn]] static void
ser_reject_number (const scr_char *caller, const scr_char *what)
{
  scr_error ("%s: invalid %s at line %ld\n", caller, what, ser.read.line - 1);
  throw ser_tas_error_t ();
}

static scr_int
ser_get_int (void)
{
  scr_int value;

  /* Get line, and scan for a single integer; return it. */
  if (sscanf (ser_get_string (), "%ld", &value) != 1)
    ser_reject_number ("ser_get_int", "integer");

  return value;
}

static scr_uint
ser_get_uint (void)
{
  scr_uint value;

  /* Get line, and scan for a single integer; return it. */
  if (sscanf (ser_get_string (), "%lu", &value) != 1)
    ser_reject_number ("ser_get_uint", "integer");

  return value;
}

static scr_bool
ser_get_boolean (void)
{
  scr_uint value;

  /*
   * Get line, and scan for a single integer; check it's a valid-looking flag,
   * and return it.
   */
  if (sscanf (ser_get_string (), "%lu", &value) != 1)
    ser_reject_number ("ser_get_boolean", "boolean");
  if (value != 0 && value != 1)
    {
      scr_error ("ser_get_boolean:"
                " warning: suspect boolean at line %ld\n", ser.read.line - 1);
    }

  return value != 0;
}


/*
 * ser_reject_if()
 *
 * Reject the save currently being restored (via the restore error throw) when
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
                 " index out of range at line %ld\n", ser.read.line - 1);
      throw ser_tas_error_t ();
    }
}


/*
 * ser_get_wield()
 *
 * Read the player's wielded weapon: an object index, or -1 for none.  It is
 * used unchecked as an object index whenever the player attacks
 * (battle_player_wielded_weapon), so anything else must not survive the
 * restore.  Clear it to "none" rather than reject the save: Runner-written
 * battle saves have not been checked against this field (see the note at
 * ser_save_battle_block), and a lost wield costs only a re-wield.
 */
static scr_int
ser_get_wield (scr_gameref_t game)
{
  const scr_int wield = ser_get_int ();

  return (wield >= 0 && wield < gs_object_count (game)) ? wield : -1;
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

      /*
       * A pre-4.0 save stores Strength and Defence only, and no "lo" bound;
       * what it omits keeps the values battle_start() seeded, which is what
       * the 3.9 Runner effectively has too -- it never uses them.
       */
      if (ser.pre_v4 && slot != BATTLE_STRENGTH && slot != BATTLE_DEFENSE)
        continue;

      battle->max[slot] = ser_get_int ();
      battle->hi[slot] = ser_get_int ();
      if (!ser.pre_v4)
        battle->lo[slot] = ser_get_int ();
    }
  if (npc < 0)
    gs_set_playerwield (game, ser_get_wield (game));
  else
    {
      battle->speed = ser_get_int ();
      gs_set_npc_attackcounter (game, npc, ser_get_int ());
    }
  battle->seeded = TRUE;
}


/*
 * ser_object_position_valid()
 * ser_object_parent_valid()
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
 * Read an object's location.  A static object starts from its bundle "Where"
 * list (gs_create), and only an event can move it (evt_move_object), so the
 * save only has to say whether it moved, and where to, in the shape
 * ser_save_object_location() wrote it:
 *
 *   - a pre-4.0 save has no room list, just the bare position, which for an
 *     unmoved static is OBJ_HIDDEN (or OBJ_PART_NPC).  Held by the player (0)
 *     or a room (1..rooms) can only be an event's move.  One moved to hidden
 *     reads the same as unmoved, and comes back unmoved;
 *   - in 4.0 Runner format a static is a room list (count, then 1-based
 *     rooms).  The bundle's own list is unmoved; one room, or none, is an
 *     event's move there, or out of sight, and several are 4.0 finish moves
 *     piled up (evt_move_object).  (The saver writes a static held by the
 *     player as none.)
 *
 * Until 2026-09-14 both forms were discarded, so every restore -- a saved
 * game, the undo tail and Spatterlight's autosave -- put an event-moved static
 * back where the game started it: wrecked.taf's hole in the sewer wall.
 *
 * A dynamic object, or any object in a legacy save, is a single position
 * integer.
 */
static void
ser_restore_object_location (scr_gameref_t game, scr_int object,
                             scr_bool runner_format)
{
  if (ser.pre_v4 && obj_is_static (game, object))
    {
      const scr_int position = ser_get_int ();

      if (position >= OBJ_HELD_PLAYER && position <= gs_room_count (game))
        {
          game->objects[object].position = position;
          gs_set_object_static_unmoved (game, object, FALSE);
        }
    }
  else if (runner_format && !ser.pre_v4 && obj_is_static (game, object))
    {
      std::vector<scr_int> rooms;
      scr_int count = ser_get_int (), index_;

      ser_reject_if (count < 0 || count > gs_room_count (game));
      rooms.push_back (count);
      for (index_ = 0; index_ < count; index_++)
        rooms.push_back (ser_get_int ());

      ser_synchronize_cache (game);
      if (rooms != ser_object_static_rooms (game, object))
        {
          if (count == 0)
            {
              game->objects[object].position = OBJ_HIDDEN;
              gs_set_object_static_unmoved (game, object, FALSE);
            }
          else
            {
              /*
               * One room or several (4.0 finish moves add, see
               * evt_move_object); the last written is the position.
               */
              scr_bool valid = TRUE;

              for (index_ = 1; index_ <= count; index_++)
                valid &= rooms[index_] >= 1
                         && rooms[index_] <= gs_room_count (game);
              if (!valid)
                return;
              game->objects[object].position = rooms[count];
              gs_set_object_static_unmoved (game, object, FALSE);
              gs_object_static_rooms_clear (game, object);
              for (index_ = 1; index_ <= count; index_++)
                gs_object_static_rooms_add (game, object, rooms[index_] - 1);
            }
        }
    }
  else
    game->objects[object].position = ser_get_int ();
}


/*
 * ser_load_header()
 * ser_load_player()
 * ser_load_object()
 * ser_load_object_runner_parent()
 * ser_reject_containment_loops()
 * ser_load_events()
 * ser_load_npcs()
 * ser_load_variables()
 * ser_load_legacy_battle()
 *
 * The sections of a saved game, in file order, each the reader for the
 * ser_save_*() function that writes it.  All but the header read into the
 * fresh game the body created; any of them may throw ser_tas_error_t.
 */
/*
 * The version line, the game name and the entity counts, all checked against
 * the running game.  Returns TRUE for a save in the Runner's layout.
 */
static scr_bool
ser_load_header (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2];
  const scr_char *gamename;
  scr_bool runner_format = ser.pre_v4;

  /*
   * Read the first line.  If it begins with the ADRIFT v4 version-line lead
   * byte (0xAC) it is a Runner/new-format save: skip the version line (we
   * accept any version a v4 Runner wrote rather than insist on our own digits)
   * and take the GameName from the next line.  Otherwise the file is a legacy
   * SCARE save whose first line is already the GameName.  See
   * SER_VERSION_HEADER.
   */
  gamename = ser_get_string ();
  if (!ser.pre_v4 && (scr_byte) gamename[0] == SER_VERSION_LEAD)
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
    throw ser_tas_error_t ();

  /* Read and verify the counts in the saved game. */
  if (ser_get_int () != gs_room_count (game)
      || ser_get_int () != gs_object_count (game)
      || ser_get_int () != gs_task_count (game)
      || ser_get_int () != gs_event_count (game)
      || ser_get_int () != gs_npc_count (game))
    throw ser_tas_error_t ();

  return runner_format;
}

/*
 * The player block, encumbrance, and the player's battle block.  Returns the
 * carried weight the save was made with.
 */
static scr_int
ser_load_player (scr_gameref_t game, scr_bool runner_format)
{
  scr_int saved_carried_weight;

  /*
   * Skip the player name (4.0 Runner format only; absent in legacy SCARE
   * saves and in pre-4.0 Runner saves).
   */
  if (runner_format && !ser.pre_v4)
    (void) ser_get_string ();

  /* Restore the rest of the player block, validating untrusted indices. */
  {
    const scr_int playerroom = ser_get_int ();

    ser_reject_if (playerroom < 1 || playerroom > gs_room_count (game));
    gs_set_playerroom (game, playerroom - 1);
  }
  {
    const scr_int playerparent = ser_get_int ();

    ser_reject_if (playerparent < -1
                   || playerparent >= gs_object_count (game));
    gs_set_playerparent (game, playerparent);
  }
  gs_set_playerposition (game, ser_get_int ());

  /* Skip player gender. */
  (void) ser_get_int ();

  /*
   * Skip encumbrance details; the carried totals are recomputed from the
   * restored inventory once loading is done -- all but a 3.9 game's weight,
   * which comes back as it was saved (see below).
   */
  (void) ser_get_int ();
  (void) ser_get_int ();
  (void) ser_get_int ();
  saved_carried_weight = ser_get_int ();

  /*
   * Runner format: the player's interleaved battle block follows
   * encumbrance.
   */
  if (runner_format && battle_is_enabled (game))
    ser_restore_battle_block (game, -1);

  return saved_carried_weight;
}

/* One object's record, with its position and parent validated. */
static void
ser_load_object (scr_gameref_t game, scr_int object, scr_bool runner_format)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int openable, currentstate;

  /* Bypass mutators for position and parent.  Fix later? */
  ser_restore_object_location (game, object, runner_format);
  gs_set_object_seen (game, object, ser_get_boolean ());
  game->objects[object].parent = ser_get_int ();

  /*
   * Validate a dynamic object's restored position and parent against the
   * current game.  A static object's position is taken from its bundle
   * (not the save), and its parent is never used as an array index.
   */
  if (!obj_is_static (game, object))
    {
      const scr_int position = game->objects[object].position;
      const scr_int parent = game->objects[object].parent;

      ser_reject_if (!ser_object_position_valid (game, position));
      ser_reject_if (!ser_object_parent_valid (game, position, parent));
    }

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Openable";
  openable = prop_get_integer (bundle, "I<-sis", vt_key);
  if (openable == 0)
    gs_set_object_openness (game, object, 0);
  else if (ser.pre_v4)
    gs_set_object_openness (game, object,
                            ser_openness_pre_v4 (ser_get_int ()));
  else
    gs_set_object_openness (game, object, ser_get_int ());

  vt_key[2].string = "CurrentState";
  currentstate = prop_get_integer (bundle, "I<-sis", vt_key);
  gs_set_object_state (game, object,
                       currentstate != 0 ? ser_get_int () : 0);

  /*
   * Pre-4.0 does not store "unmoved"; the value gs_create() derived from
   * OnlyWhenNotMoved stands, exactly as it does in the 3.9 Runner.
   */
  if (!ser.pre_v4)
    gs_set_object_unmoved (game, object, ser_get_boolean ());
}

static void
ser_load_object_runner_parent (scr_gameref_t game, scr_int object)
{
  /*
   * Reconstruct the Runner's container field ([2E]) -- see the
   * runner_parent notes in scgamest.h.  No save format stores it, so
   * this is a heuristic built on the detach model: an in/on placement
   * points it at the restored parent; NPC possession clears it; an
   * object that *started* in/on but is no longer there must have been
   * detached at some point, so it is cleared too.  Everything else
   * keeps the raw .taf Parent seed gs_create() gave it, which the
   * Runner's ordinary take/drop/task moves never touch.  The one case
   * this gets wrong is a raw-Parent object that was worn and then
   * removed -- the save cannot tell us.
   */
  if (obj_is_static (game, object))
    gs_set_object_runner_parent (game, object, -1);
  else
    {
      const scr_int position = game->objects[object].position;

      if (position == OBJ_IN_OBJECT || position == OBJ_ON_OBJECT)
        gs_set_object_runner_parent (game, object,
                                     game->objects[object].parent);
      else if (position == OBJ_HELD_NPC || position == OBJ_WORN_NPC)
        gs_set_object_runner_parent (game, object, -1);
      else
        {
          scr_int initialposition;

          initialposition = prop_get_indexed_integer (gs_get_bundle (game),
                                                      "Objects", object,
                                                      "InitialPosition");
          if (initialposition == 2 || initialposition == 3)
            gs_set_object_runner_parent (game, object, -1);
          /* Otherwise the seed from gs_create() stands. */
        }
    }
}

static void
ser_reject_containment_loops (scr_gameref_t game)
{
  scr_int index_;

  /*
   * Each in/on parent was checked above to be an object, but not that the
   * chain ends: an object inside itself, or two inside each other, sends
   * every containment walk (obj_indirectly_in_room and friends) into unbounded
   * recursion.  A chain longer than the object count must revisit an object.
   * Only dynamic objects had their parent checked, so stop at a static one.
   */
  GS_FOR_EACH_OBJECT (game, index_)
    {
      scr_int object = index_, steps = 0;

      while (!obj_is_static (game, object)
             && (game->objects[object].position == OBJ_IN_OBJECT
                 || game->objects[object].position == OBJ_ON_OBJECT))
        {
          ser_reject_if (++steps > gs_object_count (game));
          object = game->objects[object].parent;
        }
    }
}

static void
ser_load_events (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int index_;

  GS_FOR_EACH_EVENT (game, index_)
    {
      scr_int startertype, task;

      /* Restore first event details. */
      gs_set_event_time (game, index_, ser_get_int ());
      task = ser_get_int ();
      {
        /*
         * An unknown state would only surface later, as a scr_fatal from
         * evt_tick_event() that ends the session; refuse the save instead.
         */
        const scr_int state = ser_get_int () + 1;

        ser_reject_if (state < ES_WAITING || state > ES_PAUSED);
        gs_set_event_state (game, index_, state);
      }

      /* Verify and restore the starter task, if any. */
      if (task > 0)
        {
          startertype = prop_get_indexed_integer (bundle, "Events", index_,
                                                  "StarterType");
          if (startertype != 3)
            throw ser_tas_error_t ();

          /* Restore the starter-task snapshot (task is a 1-based index). */
          ser_reject_if (task > gs_task_count (game));
          gs_set_event_taskstate (game, index_, ser_get_boolean ());
        }
      else
        (void) ser_get_boolean ();
    }
}

static void
ser_load_npcs (scr_gameref_t game, scr_bool runner_format)
{
  scr_int index_;

  GS_FOR_EACH_NPC (game, index_)
    {
      scr_int walk;

      {
        const scr_int location = ser_get_int ();

        if (location == NPC_DEAD_LOCATION)
          {
            gs_set_npc_location (game, index_, 0);
            gs_set_npc_dead (game, index_, TRUE);
          }
        else
          {
            ser_reject_if (location < 0
                           || location > gs_room_count (game));
            gs_set_npc_location (game, index_, location);
          }
      }
      gs_set_npc_seen (game, index_, ser_get_boolean ());

      /* Runner format: the NPC's battle block follows "seen", before walks. */
      if (runner_format && battle_is_enabled (game))
        ser_restore_battle_block (game, index_);

      /*
       * Pre-4.0 stores no walk steps, so the walk restarts from gs_create()'s
       * initial state -- the same thing the 3.9 Runner does.
       */
      if (!ser.pre_v4)
        {
          for (walk = 0; walk < gs_npc_walkstep_count (game, index_);
               walk++)
            gs_set_npc_walkstep (game, index_, walk, ser_get_int ());
        }
    }
}

static void
ser_load_variables (scr_prop_setref_t bundle, scr_var_setref_t vars)
{
  scr_int index_, var_count;

  var_count = ser_variable_count (bundle);

  for (index_ = 0; index_ < var_count; index_++)
    {
      const scr_char *name;
      scr_int var_type = ser_variable_at (bundle, index_, &name);

      switch (var_type)
        {
        case TAFVAR_NUMERIC:
          var_put_integer (vars, name, ser_get_int ());
          break;

        case TAFVAR_STRING:
          var_put_string (vars, name, ser_get_string ());
          break;

        default:
          scr_fatal ("ser_load_game: unknown variable type, %ld\n", var_type);
        }
    }
}

static void
ser_load_legacy_battle (scr_gameref_t game, scr_bool runner_format)
{
  scr_int index_;

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
  if (!runner_format && battle_is_enabled (game))
    {
      const scr_char *marker = taf_next_line (ser.read.tas);
      const scr_bool is_v2 = marker
                            && strcmp (marker, SER_BATTLE_MARKER) == 0;
      const scr_bool is_v1 = marker
                            && strcmp (marker, SER_BATTLE_MARKER_V1) == 0;

      if (is_v1 || is_v2)
        {
          ser.read.line++;
          gs_set_playerstamina (game, ser_get_int ());
          gs_set_playerstaminacounter (game, ser_get_int ());
          gs_set_playerwield (game, ser_get_wield (game));
          if (is_v2)
            ser_restore_battle_attributes (gs_player_battle (game));
          GS_FOR_EACH_NPC (game, index_)
            {
              gs_set_npc_stamina (game, index_, ser_get_int ());
              gs_set_npc_staminacounter (game, index_, ser_get_int ());
              gs_set_npc_attackcounter (game, index_, ser_get_int ());
              if (is_v2)
                ser_restore_battle_attributes (gs_npc_battle (game,
                                                              index_));
            }
        }
    }
}


/*
 * ser_load_game_body()
 * ser_load_game()
 *
 * Load a serialized game into the given game by repeated calls to the
 * callback() function.  The body reads the record stream, a section function
 * at a time, into a fresh game and variable set, handing them back through
 * new_game and new_vars so that the caller can destroy them whichever way the
 * body left.
 */
static void
ser_load_game_body (scr_gameref_t game,
                    scr_gameref_t &new_game, scr_var_setref_t &new_vars)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int index_, saved_carried_weight;
  scr_bool runner_format;

  runner_format = ser_load_header (game);

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

  saved_carried_weight = ser_load_player (new_game, runner_format);

  /* Restore rooms information. */
  GS_FOR_EACH_ROOM (new_game, index_)
    gs_set_room_seen (new_game, index_, ser_get_boolean ());

  /* Restore objects information. */
  GS_FOR_EACH_OBJECT (new_game, index_)
    {
      ser_load_object (new_game, index_, runner_format);
      ser_load_object_runner_parent (new_game, index_);
    }
  ser_reject_containment_loops (new_game);

  /* Restore tasks information. */
  GS_FOR_EACH_TASK (new_game, index_)
    {
      gs_set_task_done (new_game, index_, ser_get_boolean ());
      gs_set_task_scored (new_game, index_, ser_get_boolean ());
    }

  /* Restore events information. */
  ser_load_events (new_game);

  /* Restore NPCs information. */
  ser_load_npcs (new_game, runner_format);

  /* Restore each variable. */
  ser_load_variables (bundle, new_vars);

  /* Restore timing information. */
  var_set_elapsed_seconds (new_vars, ser_get_uint ());

  /* Restore turns count. */
  new_game->turns = (scr_int) ser_get_uint ();

  ser_load_legacy_battle (new_game, runner_format);

  /*
   * Resources tweak -- set requested to match those in the current game
   * so that they remain unchanged by the gs_copy() of new_game onto
   * game.  This way, both the requested and the active resources in the
   * game are unchanged by restore.
   */
  new_game->requested_sound = game->requested_sound;
  new_game->requested_graphic = game->requested_graphic;

  /*
   * Reseed the carried-load totals from the restored inventory the way the
   * run400 save loader does (base weights and sizes only; see
   * gs_carried_recompute).  gs_copy() then carries them over verbatim.
   */
  gs_carried_recompute (new_game);

  /*
   * run390 does not reseed the weight: a restore brings back the total the
   * save was made with (measured, croft_wd probe 2026-10-09 -- 39 saved, 21
   * after a drop, 39 again once restored; a base-only reseed would say 30).
   */
  if (obj_uses_running_weight (new_game) && !obj_uses_running_load (new_game))
    new_game->carried_weight = saved_carried_weight;

  /*
   * If we got this far, we successfully restored the game from the file.
   * As our final act, copy the new game onto the old one.
   */
  new_game->temporary = game->temporary;
  new_game->undo = game->undo;
  gs_copy (game, new_game);
}


/*
 * ser_load_game_finish()
 *
 * Destroy the temporary game and variables, if any, and the TAF (TAS) file,
 * and reset the load state, after a restore that succeeded or failed.
 */
static void
ser_load_game_finish (scr_gameref_t new_game, scr_var_setref_t new_vars)
{
  if (new_game)
    gs_destroy (new_game);
  if (new_vars)
    var_destroy (new_vars);

  taf_destroy (ser.read.tas);
  ser.read.tas = nullptr;
  ser.pre_v4 = FALSE;
  ser.raw_memo = FALSE;
}

scr_bool
ser_load_game (scr_gameref_t game,
               scr_read_callbackref_t callback, void *opaque)
{
  scr_gameref_t new_game = nullptr;
  scr_var_setref_t new_vars = nullptr;
  scr_bool status;

  /*
   * Create a TAF (TAS) reference from callbacks, for reader functions.  A raw
   * memo (ser_set_raw_memo) skips the sniff/inflate and is read back verbatim;
   * everything downstream is identical, since the byte stream is the same one
   * a decompressed 4.0 save would present.
   */
  ser.read.tas = ser.raw_memo ? taf_create_tas_raw (callback, opaque)
                         : taf_create_tas (callback, opaque);
  if (!ser.read.tas)
    {
      ser.raw_memo = FALSE;
      return FALSE;
    }

  /*
   * The container tells us the layout: only run390.exe -- and ser_save_game_-
   * to_file() for a pre-4.0 game -- writes a PRNG-obfuscated save, so an
   * obfuscated stream is a pre-4.0 Runner save.  Everything else is a zlib
   * stream, either 4.0 Runner format or legacy SCARE, told apart in the body
   * by the version line.  This keeps 4.0-format saves we wrote earlier for a
   * 3.9 game readable.
   */
  ser.pre_v4 = taf_get_version (ser.read.tas) < TAF_VERSION_400;

  /* Reset line counter for error messages. */
  ser.read.line = 1;

  /*
   * Restore.  A rejected or exhausted save throws ser_tas_error_t out of the
   * readers; a fatal engine error is cleaned up after the same way, then goes
   * on to the host boundary.
   */
  try
    {
      ser_load_game_body (game, new_game, new_vars);
      status = TRUE;
    }
  catch (const ser_tas_error_t &)
    {
      status = FALSE;
    }
  catch (const scr_fatal_error &)
    {
      ser_load_game_finish (new_game, new_vars);
      throw;
    }

  ser_load_game_finish (new_game, new_vars);
  return status;
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
