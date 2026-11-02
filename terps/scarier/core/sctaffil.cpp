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

/*
 * Module notes:
 *
 * o Put integer and boolean read functions in here?
 */

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "zlib.h"
#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"


/* Assorted definitions and constants. */
static const scr_uint TAF_MAGIC = 0x5bdcfa41;
enum
{ VERSION_HEADER_SIZE = 14,
  V400_HEADER_EXTRA = 8
};
enum
{ OUT_BUFFER_SIZE = 31744,
  IN_BUFFER_SIZE = 16384,
  GROW_INCREMENT = 8
};
static const scr_char NEWLINE = '\n';
static const scr_char CARRIAGE_RETURN = '\r';


/* Version 4.0, 3.9, 3.8, and 3.7 TAF file signatures. */
static const scr_byte
  V400_SIGNATURE[VERSION_HEADER_SIZE] = {0x3c, 0x42, 0x3f, 0xc9, 0x6a, 0x87,
                                         0xc2, 0xcf, 0x93, 0x45, 0x3e, 0x61,
                                         0x39, 0xfa};
static const scr_byte
  V390_SIGNATURE[VERSION_HEADER_SIZE] = {0x3c, 0x42, 0x3f, 0xc9, 0x6a, 0x87,
                                         0xc2, 0xcf, 0x94, 0x45, 0x37, 0x61,
                                         0x39, 0xfa};
static const scr_byte
  V380_SIGNATURE[VERSION_HEADER_SIZE] = {0x3c, 0x42, 0x3f, 0xc9, 0x6a, 0x87,
                                         0xc2, 0xcf, 0x94, 0x45, 0x36, 0x61,
                                         0x39, 0xfa};


/*
 * Note that the version 3.7 signature is not the version 3.8 one with its
 * version digit stepped down by one: "Version 3.70" and "Version 3.80" differ
 * in the character *after* the one that differs in the plaintext too, because
 * each is XOR'd with a different byte of the keystream.  Byte 10 is 0x39, not
 * the 0x35 that interpolating 3.80/3.90 would suggest.
 */
static const scr_byte
  V370_SIGNATURE[VERSION_HEADER_SIZE] = {0x3c, 0x42, 0x3f, 0xc9, 0x6a, 0x87,
                                         0xc2, 0xcf, 0x94, 0x45, 0x39, 0x61,
                                         0x39, 0xfa};


/*
 * Game TAF data structure.  The game structure contains the original TAF
 * file header, a growable array of "slab" descriptors, each of which holds
 * metadata for a "slab" (around a decompression buffer full of TAF strings),
 * the length of the descriptor array and elements allocated, and a current
 * location for iteration.
 *
 * Saved game files (.TAS) are just like TAF files except that they lack the
 * header.  So for files of this type, the header is all zeroes.
 */
typedef struct
{
  scr_byte *data;
  scr_int size;
} scr_slabdesc_t;
typedef scr_slabdesc_t *scr_slabdescref_t;
typedef struct scr_taf_s
{
  scr_uint magic;
  scr_byte header[VERSION_HEADER_SIZE + V400_HEADER_EXTRA];
  scr_int version;
  scr_int total_in_bytes;
  scr_slabdescref_t slabs;
  scr_int slab_count;
  scr_int slabs_allocated;
  scr_bool is_unterminated;
  scr_int current_slab;
  scr_int current_offset;
} scr_taf_t;


/* Microsoft Visual Basic PRNG magic numbers, initial and current state. */
static const scr_int PRNG_CST1 = 0x43fd43fd,
                    PRNG_CST2 = 0x00c39ec3,
                    PRNG_CST3 = 0x00ffffff,
                    PRNG_INITIAL_STATE = 0x00a09e86;
static scr_int taf_random_state = 0x00a09e86;


/*
 * taf_random()
 * taf_random_reset()
 *
 * Version 3.9 and version 3.8 games are obfuscated by xor'ing each character
 * with the PRNG in Visual Basic.  So here we have to emulate that, to unob-
 * fuscate data from such game files.  The PRNG generates 0..prng_cst3, which
 * we multiply by 255 and then divide by prng_cst3 + 1 to get output in the
 * range 0..254.  Thanks to Rik Snel for uncovering this obfuscation.
 */
static scr_byte
taf_random (void)
{
  /* Generate and return the next pseudo-random number. */
  taf_random_state = (taf_random_state * PRNG_CST1 + PRNG_CST2) & PRNG_CST3;
  return (UCHAR_MAX * (scr_uint) taf_random_state) / (scr_uint) (PRNG_CST3 + 1);
}

static void
taf_random_reset (void)
{
  /* Reset PRNG to initial conditions. */
  taf_random_state = PRNG_INITIAL_STATE;
}


/* Where the game file's codec left the PRNG, and the runtime draws from it. */
static scr_int taf_gamefile_random_state = 0x00a09e86;
static scr_int taf_runtime_random_state = 0x00a09e86;


/*
 * taf_runtime_rnd_reset()
 * taf_runtime_rnd()
 *
 * The 3.9/3.8 Runners' codec IS the VB6 runtime's Rnd: `Rnd(-1)`,
 * `Randomize 1976`, then one draw per file byte.  They `Randomize Timer` only
 * at the END of openadv (run390 467013), so every Rnd the loader makes after
 * the codec -- each StarterType 1/2 event's start (46616F/4661BF) and each
 * Speed 1 NPC's first attack counter (getnexthit 466A43) -- continues the
 * codec's LCG from where the file ended, and is fully determined by the file.
 * Scarier's runner-compatible mode replays those from the same state: the
 * generator here is bit-for-bit VB6's (x = (x * 0x43fd43fd + 0xc39ec3) mod
 * 2^24, Rnd = x / 2^24 as a Single), and taf_unobfuscate() leaves it at the
 * post-codec state.  Reset rewinds to that state (a restart is a fresh load,
 * same file, same draws); taf_runtime_rnd() draws the next Single.  Measured:
 * Colony.taf (14781 bytes) then draws 0.1778324, 0.6708297; yeh.taf 0.5242788,
 * 0.4268395, 0.8504441, 0.6232703, 0.8613715, 0.4148291 -- exactly the
 * pass-through lines vbrng.dll traced from run390 after the second codec pass.
 */
void
taf_runtime_rnd_reset (void)
{
  taf_runtime_random_state = taf_gamefile_random_state;
}

double
taf_runtime_rnd (void)
{
  taf_runtime_random_state = (taf_runtime_random_state * PRNG_CST1
                              + PRNG_CST2) & PRNG_CST3;
  return (double) taf_runtime_random_state / (double) (PRNG_CST3 + 1);
}


/*
 * taf_obfuscate_reset()
 * taf_obfuscate_buffer()
 *
 * Run the obfuscation the other way, for writing a pre-4.0 saved game that the
 * original Runner can read back.  Xor is its own inverse, so this is
 * taf_unobfuscate()'s inner loop with the bytes going out instead of in.  Reset
 * once at the start of a stream, then feed it every byte in order; a saved game
 * has no header, so the keystream starts at index 0.
 */
void
taf_obfuscate_reset (void)
{
  taf_random_reset ();
}

void
taf_obfuscate_buffer (scr_byte *buffer, scr_int length)
{
  scr_int index_;

  for (index_ = 0; index_ < length; index_++)
    buffer[index_] ^= taf_random ();
}


/*
 * taf_is_valid()
 *
 * Return TRUE if pointer is a valid TAF structure, FALSE otherwise.
 */
static scr_bool
taf_is_valid (scr_tafref_t taf)
{
  return taf && taf->magic == TAF_MAGIC;
}


/*
 * taf_create_empty()
 *
 * Allocate and return a new, empty TAF structure.
 */
static scr_tafref_t
taf_create_empty (void)
{
  scr_tafref_t taf;

  /* Create an empty TAF structure. */
  taf = (decltype(taf)) scr_malloc (sizeof (*taf));
  taf->magic = TAF_MAGIC;
  memset (taf->header, 0, sizeof (taf->header));
  taf->version = TAF_VERSION_NONE;
  taf->total_in_bytes = 0;
  taf->slabs = nullptr;
  taf->slab_count = 0;
  taf->slabs_allocated = 0;
  taf->is_unterminated = FALSE;
  taf->current_slab = 0;
  taf->current_offset = 0;

  /* Return the new TAF structure. */
  return taf;
}


/*
 * taf_destroy()
 *
 * Free TAF memory, and destroy a TAF structure.
 */
void
taf_destroy (scr_tafref_t taf)
{
  scr_int index_;
  assert (taf_is_valid (taf));

  /* First free each slab in the slabs array,... */
  for (index_ = 0; index_ < taf->slab_count; index_++)
    scr_free (taf->slabs[index_].data);

  /*
   * ...then free slabs growable array, and poison and free the TAF structure
   * itself.
   */
  scr_free (taf->slabs);
  memset (taf, 0xaa, sizeof (*taf));
  scr_free (taf);
}


/*
 * taf_finalize_last_slab()
 *
 * Insert nul's into slab data so that it turns into a series of nul-terminated
 * strings.  Nul's are used to replace carriage return and newline pairs.
 */
static void
taf_finalize_last_slab (scr_tafref_t taf)
{
  scr_slabdescref_t slab;
  scr_int index_;

  /* Locate the final slab in the slab descriptors array. */
  assert (taf->slab_count > 0);
  slab = taf->slabs + taf->slab_count - 1;

  /*
   * Replace carriage return and newline pairs with nuls, and individual
   * carriage returns with a single newline.
   */
  for (index_ = 0; index_ < slab->size; index_++)
    {
      if (slab->data[index_] == CARRIAGE_RETURN)
        {
          if (index_ < slab->size - 1 && slab->data[index_ + 1] == NEWLINE)
            {
              slab->data[index_] = NUL;
              slab->data[index_ + 1] = NUL;
              index_++;
            }
          else
            slab->data[index_] = NEWLINE;
        }

      /* Also protect against unlikely incoming nul characters. */
      else if (slab->data[index_] == NUL)
        slab->data[index_] = NEWLINE;
    }
}


/*
 * taf_find_buffer_extent()
 *
 * Search backwards from the buffer end for a terminating carriage return and
 * line feed.  If none, found, return length and set is_unterminated to TRUE.
 * Otherwise, return the count of usable bytes found in the buffer.
 */
static scr_int
taf_find_buffer_extent (const scr_byte *buffer,
                        scr_int length, scr_bool *is_unterminated)
{
  scr_int bytes;

  /* Search backwards from the buffer end for the final line feed. */
  for (bytes = length; bytes > 1; bytes--)
    {
      if (buffer[bytes - 2] == CARRIAGE_RETURN && buffer[bytes - 1] == NEWLINE)
        break;
    }
  if (bytes < 2)
    {
      /* No carriage return and newline termination found. */
      *is_unterminated = TRUE;
      return length;
    }

  *is_unterminated = FALSE;
  return bytes;
}


/*
 * taf_append_buffer()
 *
 * Append a buffer of TAF lines to an existing TAF structure.  Returns the
 * number of characters consumed from the buffer.
 */
static scr_int
taf_append_buffer (scr_tafref_t taf, const scr_byte *buffer, scr_int length)
{
  scr_int bytes;
  scr_bool is_unterminated;

  /* Locate the extent of appendable data in the buffer. */
  bytes = taf_find_buffer_extent (buffer, length, &is_unterminated);

  /* See if the last buffer handled contained at least one data line. */
  if (!taf->is_unterminated)
    {
      scr_slabdescref_t slab;

      /* Extend the slabs array if we've reached the current allocation. */
      if (taf->slab_count == taf->slabs_allocated)
        {
          taf->slabs_allocated += GROW_INCREMENT;
          taf->slabs
            = (decltype(taf->slabs)) scr_realloc (taf->slabs,
                                                  taf->slabs_allocated
                                                  * sizeof (*taf->slabs));
        }

      /* Advance to the next unused slab in the slab descriptors array. */
      slab = taf->slabs + taf->slab_count;
      taf->slab_count++;

      /* Copy the input buffer into the new slab. */
      slab->data = (decltype(slab->data)) scr_malloc (bytes);
      memcpy (slab->data, buffer, bytes);
      slab->size = bytes;
    }
  else
    {
      scr_slabdescref_t slab;

      /* Locate the final slab in the slab descriptors array. */
      assert (taf->slab_count > 0);
      slab = taf->slabs + taf->slab_count - 1;

      /*
       * The last buffer we saw had no line endings in it.  In this case,
       * append the input buffer to the end of the last slab's data, rather
       * than creating a new slab.  This may cause allocation to overflow
       * the system limits on single allocated areas on some platforms.
       */
      slab->data = (decltype(slab->data)) scr_realloc (slab->data,
                                                       slab->size + bytes);
      memcpy (slab->data + slab->size, buffer, bytes);
      slab->size += bytes;

      /*
       * Use a special case for the final carriage return and newline pairing
       * that are split over two buffers; force correct termination of this
       * slab.
       */
      if (slab->size > 1
          && slab->data[slab->size - 2] == CARRIAGE_RETURN
          && slab->data[slab->size - 1] == NEWLINE)
        is_unterminated = FALSE;
    }

  /*
   * Note if this buffer requires that the next be coalesced with it.  If it
   * doesn't, finalize the last slab by breaking it into separate lines.
   */
  taf->is_unterminated = is_unterminated;
  if (!is_unterminated)
    taf_finalize_last_slab (taf);

  /* Return count of buffer bytes consumed. */
  return bytes;
}


/*
 * taf_drain_callback()
 *
 * The buffered read loop shared by taf_unobfuscate() and taf_read_raw():
 * pull data from callback() in buffer sized chunks, xor each byte read with
 * the PRNG if is_obfuscated, and feed complete lines to taf_append_buffer()
 * until the callback runs dry.  Passes back the total bytes consumed, and
 * returns the count left unconsumed in the buffer (a partial line).
 *
 * The buffer lives on the heap, done to help systems with limited stacks;
 * its RAII owner frees it if taf_append_buffer throws.
 */
static scr_int
taf_drain_callback (scr_tafref_t taf, scr_read_callbackref_t callback,
                    void *opaque, scr_bool is_obfuscated,
                    scr_int *total_bytes)
{
  scr_int bytes, used_bytes, index_;

  std::vector<scr_byte> buffer_storage (IN_BUFFER_SIZE);
  scr_byte *const buffer = buffer_storage.data ();
  used_bytes = 0;
  *total_bytes = 0;

  do
    {
      /* Try to obtain more data. */
      bytes = callback (opaque,
                        buffer + used_bytes, IN_BUFFER_SIZE - used_bytes);

      /* Unobfuscate data read in, where required. */
      if (is_obfuscated)
        {
          for (index_ = 0; index_ < bytes; index_++)
            buffer[used_bytes + index_] ^= taf_random ();
        }

      /*
       * Add data read in to buffer used data, and if data is available,
       * add it to the TAF.
       */
      used_bytes += bytes;
      if (used_bytes > 0)
        {
          scr_int consumed;

          /* Add complete lines from this buffer to the TAF. */
          consumed = taf_append_buffer (taf, buffer, used_bytes);

          /* Move the unconsumed tail (a partial line) to the buffer start. */
          memmove (buffer, buffer + consumed, IN_BUFFER_SIZE - consumed);

          /* Note counts of bytes consumed and remaining in the buffer. */
          used_bytes -= consumed;
          *total_bytes += consumed;
        }
    }
  while (bytes > 0);

  return used_bytes;
}


/*
 * taf_unobfuscate()
 *
 * Unobfuscate a version 3.9 and version 3.8 TAF file from data read by
 * repeated calls to the callback() function.  Callback() should return the
 * count of bytes placed in the buffer, or 0 if no more (end of file).
 * Assumes that the file has been read past the header.
 */
static scr_bool
taf_unobfuscate (scr_tafref_t taf, scr_read_callbackref_t callback,
                 void *opaque, scr_bool is_gamefile)
{
  scr_int used_bytes, total_bytes, index_;

  /*
   * Reset the PRNG, and synchronize with the header already read.  A saved
   * game has no header, so its keystream starts at index 0 -- skipping the
   * fourteen header values there would decode to garbage.
   */
  taf_random_reset ();
  if (is_gamefile)
    {
      for (index_ = 0; index_ < VERSION_HEADER_SIZE; index_++)
        taf_random ();
    }

  /* Unobfuscate in buffer sized chunks. */
  used_bytes = taf_drain_callback (taf, callback, opaque, TRUE, &total_bytes);

  /*
   * Unobfuscation completed, note the total bytes read.  This value is
   * actually not used for version 3.9 and version 3.8 games, but we maintain
   * it just in case.
   */
  taf->total_in_bytes = total_bytes;
  if (is_gamefile)
    {
      taf->total_in_bytes += VERSION_HEADER_SIZE;

      /* Where the Runner's loader continues drawing (taf_runtime_rnd). */
      taf_gamefile_random_state = taf_random_state;
    }

  /* Check that we found the end of the input file as expected. */
  if (used_bytes > 0)
    {
      scr_error ("taf_unobfuscate:"
                 " warning: %ld unhandled bytes in the buffer\n", used_bytes);
    }

  if (taf->is_unterminated)
    scr_fatal ("taf_unobfuscate: unterminated final data slab\n");

  /* Return successfully. */
  return TRUE;
}


/*
 * taf_read_raw()
 *
 * Read an uncompressed, unobfuscated stream verbatim into the TAF.  Used only
 * for the in-memory undo memos (see ser_set_raw_memo): those are a self-
 * contained round trip we both write and read, so they skip the per-turn
 * deflate that profiling put at ~20-48% of the interpreter, and this is the
 * matching reader.  It is taf_unobfuscate() with the PRNG step removed -- same
 * buffered taf_append_buffer() drain, no header keystream to synchronise.
 */
static scr_bool
taf_read_raw (scr_tafref_t taf, scr_read_callbackref_t callback, void *opaque)
{
  scr_int used_bytes, total_bytes;

  used_bytes = taf_drain_callback (taf, callback, opaque, FALSE, &total_bytes);

  taf->total_in_bytes = total_bytes;

  if (used_bytes > 0)
    scr_error ("taf_read_raw:"
               " warning: %ld unhandled bytes in the buffer\n", used_bytes);

  if (taf->is_unterminated)
    scr_fatal ("taf_read_raw: unterminated final data slab\n");

  return TRUE;
}


/*
 * taf_decompress()
 *
 * Decompress a version 4.0 TAF file from data read by repeated calls to the
 * callback() function.  Callback() should return the count of bytes placed
 * in the buffer, 0 if no more (end of file).  Assumes that the file has been
 * read past the header.
 */
static scr_bool
taf_decompress (scr_tafref_t taf, scr_read_callbackref_t callback,
                void *opaque, scr_bool is_gamefile)
{
  z_stream stream;
  scr_int status;
  scr_bool is_first_block;

  /*
   * Allocate buffers on the heap rather than as stack variables for systems
   * such as PalmOS that may have limited stacks.  The RAII owners free them on
   * every exit, including if taf_append_buffer or the unterminated-slab
   * scr_fatal below throws.
   */
  std::vector<scr_byte> in_storage (IN_BUFFER_SIZE);
  std::vector<scr_byte> out_storage (OUT_BUFFER_SIZE);
  scr_byte *const in_buffer = in_storage.data ();
  scr_byte *const out_buffer = out_storage.data ();

  /* Initialize Zlib inflation functions. */
  stream.next_out = out_buffer;
  stream.avail_out = OUT_BUFFER_SIZE;
  stream.next_in = in_buffer;
  stream.avail_in = 0;

  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;

  status = inflateInit (&stream);
  if (status != Z_OK)
    {
      scr_error ("taf_decompress: inflateInit: error %ld\n", status);
      return FALSE;
    }

  /*
   * Attempts to restore non-savefiles can arrive here, because there's no
   * up-front header check, like the one for TAF files, applied to them.  The
   * first we see of the problem is when the first inflate() fails, so it's
   * handy to use a flag here to block the error report for such cases.
   */
  is_first_block = TRUE;

  /* Inflate the input buffers. */
  while (TRUE)
    {
      scr_int in_bytes, out_bytes;

      /* If the input buffer is empty, try to obtain more data. */
      if (stream.avail_in == 0)
        {
          in_bytes = callback (opaque, in_buffer, IN_BUFFER_SIZE);
          stream.next_in = in_buffer;
          stream.avail_in = in_bytes;
        }

      /* Decompress as much stream data as we can. */
      status = inflate (&stream, Z_SYNC_FLUSH);
      if (status != Z_STREAM_END && status != Z_OK)
        {
          if (is_gamefile || !is_first_block)
            scr_error ("taf_decompress: inflate: error %ld\n", status);
          inflateEnd (&stream);
          return FALSE;
        }
      out_bytes = OUT_BUFFER_SIZE - stream.avail_out;

      /* See if decompressed data is available. */
      if (out_bytes > 0)
        {
          scr_int consumed;

          /* Add lines from this buffer to the TAF. */
          consumed = taf_append_buffer (taf, out_buffer, out_bytes);

          /* Move unused buffer data to buffer start. */
          memmove (out_buffer,
                   out_buffer + consumed, OUT_BUFFER_SIZE - consumed);

          /* Reset inflation stream for available space. */
          stream.next_out = out_buffer + out_bytes - consumed;
          stream.avail_out += consumed;
        }

      /* Enable full error reporting for non-gamefiles. */
      is_first_block = FALSE;

      /* If at inflation stream end and output is empty, leave loop. */
      if (status == Z_STREAM_END && stream.avail_out == OUT_BUFFER_SIZE)
        break;
    }

  /*
   * Decompression completed, note the total bytes read for use when locating
   * resources later on in the file.  For what it's worth, this value is only
   * used in version 4.0 games.
   */
  taf->total_in_bytes = stream.total_in;
  if (is_gamefile)
    taf->total_in_bytes += VERSION_HEADER_SIZE + V400_HEADER_EXTRA;

  /* End inflation. */
  status = inflateEnd (&stream);
  if (status != Z_OK)
    scr_error ("taf_decompress: warning: inflateEnd: error %ld\n", status);

  if (taf->is_unterminated)
    scr_fatal ("taf_decompress: unterminated final data slab\n");

  /* Return successfully. */
  return TRUE;
}


/* Saved-game prefix peeked at for its format, and replayed on reading. */
typedef struct
{
  scr_read_callbackref_t callback;
  void *opaque;
  scr_byte prefix[2];
  scr_int length;
  scr_int offset;
} scr_tas_sniff_t;


/*
 * taf_sniff_read()
 * taf_sniff_tas_version()
 *
 * Saved-game container sniffing.  A .tas carries no signature, so its format
 * has to be recognised from its first bytes.  A version 4.0 save is a bare
 * zlib stream, whose two-byte header is 0x78 (deflate, 32K window -- what
 * every deflateInit emits) followed by a check byte chosen so the pair is a
 * multiple of 31.  Anything else is a pre-4.0 save, obfuscated with the same
 * Visual Basic PRNG a version 3.9 .taf uses.  The odds of an obfuscated first
 * line passing the zlib test by chance are about one in eight thousand.
 *
 * The sniffed bytes cannot be pushed back into the caller's stream, so the
 * reader runs against taf_sniff_read(), which replays them and then delegates.
 */
static scr_int
taf_sniff_read (void *opaque, scr_byte *buffer, scr_int length)
{
  scr_tas_sniff_t *const sniff = (scr_tas_sniff_t *) opaque;
  scr_int bytes = 0;

  while (sniff->offset < sniff->length && bytes < length)
    buffer[bytes++] = sniff->prefix[sniff->offset++];

  if (bytes < length)
    bytes += sniff->callback (sniff->opaque, buffer + bytes, length - bytes);

  return bytes;
}

static scr_int
taf_sniff_tas_version (const scr_byte *prefix, scr_int length)
{
  if (length >= 2
      && prefix[0] == 0x78
      && ((prefix[0] << 8) | prefix[1]) % 31 == 0)
    return TAF_VERSION_400;

  return TAF_VERSION_390;
}


/*
 * taf_populate_from_callback()
 *
 * Fill an empty TAF structure from data read in by repeated calls to the
 * callback() function.  Callback() should return the count of bytes placed
 * in the buffer, or 0 if no more (end of file).
 */
static scr_tafref_t
taf_populate_from_callback (scr_tafref_t taf,
                            scr_read_callbackref_t callback,
                            void *opaque, scr_bool is_gamefile)
{
  scr_bool status = FALSE;
  scr_tas_sniff_t sniff;

  /*
   * Determine the TAF file version in use.  For game files it comes from the
   * header; for saved games, which have none, it is sniffed from the first
   * bytes of the stream.
   */
  if (is_gamefile)
    {
      scr_int in_bytes;

      /*
       * Read in the ADRIFT header for game files.  Start by reading in the
       * shorter header common to all.
       */
      in_bytes = callback (opaque, taf->header, VERSION_HEADER_SIZE);
      if (in_bytes != VERSION_HEADER_SIZE)
        {
          scr_error ("taf_create: not enough data for standard TAF header\n");
          taf_destroy (taf);
          return nullptr;
        }

      /*
       * Compare the header with the known TAF signatures, and set TAF version
       * appropriately.
       */
      if (memcmp (taf->header, V400_SIGNATURE, VERSION_HEADER_SIZE) == 0)
        {
          /* Read in the version 4.0 header extension. */
          in_bytes = callback (opaque,
                               taf->header + VERSION_HEADER_SIZE,
                               V400_HEADER_EXTRA);
          if (in_bytes != V400_HEADER_EXTRA)
            {
              scr_error ("taf_create:"
                         " not enough data for extended TAF header\n");
              taf_destroy (taf);
              return nullptr;
            }

          taf->version = TAF_VERSION_400;
        }
      else if (memcmp (taf->header, V390_SIGNATURE, VERSION_HEADER_SIZE) == 0)
        taf->version = TAF_VERSION_390;
      else if (memcmp (taf->header, V380_SIGNATURE, VERSION_HEADER_SIZE) == 0)
        taf->version = TAF_VERSION_380;
      else if (memcmp (taf->header, V370_SIGNATURE, VERSION_HEADER_SIZE) == 0)
        taf->version = TAF_VERSION_370;
      else
        {
          taf_destroy (taf);
          return nullptr;
        }
    }
  else
    {
      /*
       * A saved game written by Scarier or by run400.exe is a zlib stream; one
       * written by run390.exe (or by us for a pre-4.0 game) is PRNG-obfuscated.
       * Peek at the first bytes to tell them apart, then read through the
       * replaying wrapper so the peeked bytes are not lost.
       */
      sniff.callback = callback;
      sniff.opaque = opaque;
      sniff.length = callback (opaque, sniff.prefix,
                               (scr_int) sizeof (sniff.prefix));
      sniff.offset = 0;

      taf->version = taf_sniff_tas_version (sniff.prefix, sniff.length);
      callback = taf_sniff_read;
      opaque = &sniff;
    }

  /*
   * Call the appropriate game file reader function.  For version 4.0 games,
   * data is compressed with Zlib.  For version 3.9, 3.8 and 3.7 games, it's
   * obfuscated with the Visual Basic PRNG.
   */
  switch (taf->version)
    {
    case TAF_VERSION_400:
      status = taf_decompress (taf, callback, opaque, is_gamefile);
      break;

    case TAF_VERSION_390:
    case TAF_VERSION_380:
    case TAF_VERSION_370:
      status = taf_unobfuscate (taf, callback, opaque, is_gamefile);
      break;

    default:
      scr_fatal ("taf_create: invalid version\n");
    }
  if (!status)
    {
      taf_destroy (taf);
      return nullptr;
    }

  /* Return successfully. */
  return taf;
}


/*
 * taf_populate_raw()
 *
 * The populate step behind taf_create_tas_raw(): fill an empty taf from an
 * uncompressed, unobfuscated stream.  There is nothing to sniff (the caller
 * already knows it is raw) and nothing to inflate; the version is fixed at
 * 4.0, matching the layout ser_save_game() always uses for memos.  Same
 * contract as taf_populate_from_callback(): the taf on success, NULL after
 * destroying it on a plain read failure.  is_gamefile is unused.
 */
static scr_tafref_t
taf_populate_raw (scr_tafref_t taf, scr_read_callbackref_t callback,
                  void *opaque, scr_bool /* is_gamefile */)
{
  taf->version = TAF_VERSION_400;
  if (!taf_read_raw (taf, callback, opaque))
    {
      taf_destroy (taf);
      return nullptr;
    }
  return taf;
}


/* A populate step: fill the taf from the callback, or destroy it. */
typedef scr_tafref_t (*taf_populate_fn) (scr_tafref_t, scr_read_callbackref_t,
                                         void *, scr_bool);


/*
 * taf_create_from_callback()
 *
 * Shared scaffold of the public constructors: create an empty taf, and hand
 * it to the populate function given along with the callback data.
 */
static scr_tafref_t
taf_create_from_callback (taf_populate_fn populate,
                          scr_read_callbackref_t callback,
                          void *opaque, scr_bool is_gamefile)
{
  scr_tafref_t taf;
  assert (callback);

  /* Create an empty TAF structure. */
  taf = taf_create_empty ();

  /*
   * Populate it from the callback data.  The readers can throw (scr_fatal on
   * a truncated final slab or on allocation failure); reclaim the partially
   * built taf on that path, then let the throw carry on to the interface
   * boundary.  Plain read failures destroy the taf inside the populate call
   * and return NULL.
   */
  try
    {
      return populate (taf, callback, opaque, is_gamefile);
    }
  catch (...)
    {
      taf_destroy (taf);
      throw;
    }
}


/*
 * taf_create()
 * taf_create_tas()
 * taf_create_tas_raw()
 *
 * Public entry points for taf_create_from_callback().  Return a taf object
 * constructed from either *.TAF (game) or *.TAS (saved game state) file data,
 * or, for the raw form, from an uncompressed, unobfuscated stream -- the
 * format ser_save_game() writes for undo memos when ser_set_raw_memo() is on.
 */
scr_tafref_t
taf_create (scr_read_callbackref_t callback, void *opaque)
{
  return taf_create_from_callback (taf_populate_from_callback,
                                   callback, opaque, TRUE);
}

scr_tafref_t
taf_create_tas (scr_read_callbackref_t callback, void *opaque)
{
  return taf_create_from_callback (taf_populate_from_callback,
                                   callback, opaque, FALSE);
}

scr_tafref_t
taf_create_tas_raw (scr_read_callbackref_t callback, void *opaque)
{
  return taf_create_from_callback (taf_populate_raw, callback, opaque, FALSE);
}


/*
 * taf_first_line()
 *
 * Iterator rewind function, reset current slab location to TAF data start.
 */
void
taf_first_line (scr_tafref_t taf)
{
  assert (taf_is_valid (taf));

  /* Set current locations to TAF start. */
  taf->current_slab = 0;
  taf->current_offset = 0;
}


/*
 * taf_next_line()
 *
 * Iterator function, return the next line of data from a TAF, or NULL
 * if no more lines.
 */
const scr_char *
taf_next_line (scr_tafref_t taf)
{
  assert (taf_is_valid (taf));

  /* If there is a next line, return it and advance current. */
  if (taf->current_slab < taf->slab_count)
    {
      scr_char *line;

      /* Get the effective address of the current line. */
      line = (scr_char *) taf->slabs[taf->current_slab].data;
      line += taf->current_offset;

      /*
       * Advance to the next line.  The + 2 skips the NULs used to replace the
       * carriage return and line feed.
       */
      taf->current_offset += strlen (line) + 2;
      if (taf->current_offset >= taf->slabs[taf->current_slab].size)
        {
          taf->current_slab++;
          taf->current_offset = 0;
        }

      return line;
    }

  /* No more lines, so return NULL. */
  return nullptr;
}


/*
 * taf_more_lines()
 *
 * Iterator end function, returns TRUE if more TAF lines are readable.
 */
scr_bool
taf_more_lines (scr_tafref_t taf)
{
  assert (taf_is_valid (taf));

  /* Return TRUE if not at TAF data end. */
  return taf->current_slab < taf->slab_count;
}


/*
 * taf_get_game_data_length()
 *
 * Returns the number of bytes read to decompress the game.  Resources are
 * appended to the TAF file after the game, so this value allows them to
 * be located.
 */
scr_int
taf_get_game_data_length (scr_tafref_t taf)
{
  assert (taf_is_valid (taf));

  /*
   * Return the count of bytes inflated; this includes the TAF header length
   * for TAF, rather than TAS, files.  For TAS files, the count of file bytes
   * read is irrelevant, and is never used.
   */
  return taf->total_in_bytes;
}


/*
 * taf_get_version()
 *
 * Return the version number of the TAF file, 400, 390, 380, or 370.
 */
scr_int
taf_get_version (scr_tafref_t taf)
{
  assert (taf_is_valid (taf));

  assert (taf->version != TAF_VERSION_NONE);
  return taf->version;
}


/*
 * taf_debug_is_taf_string()
 * taf_debug_dump()
 *
 * Print out a complete TAF structure.  The first function is a helper for
 * properties debugging, indicating if a given address is a string in a TAF
 * slab, and therefore safe to print.
 */
scr_bool
taf_debug_is_taf_string (scr_tafref_t taf, const void *addr)
{
  const scr_byte *const addr_ = (const scr_byte *) addr;
  scr_int index_;

  /*
   * Compare pointer, by address directly, against all memory contained in
   * the TAF slabs.  Return TRUE if in range.
   */
  for (index_ = 0; index_ < taf->slab_count; index_++)
    {
      if (addr_ >= taf->slabs[index_].data
          && addr_ < taf->slabs[index_].data + taf->slabs[index_].size)
        return TRUE;
    }

  return FALSE;
}

void
taf_debug_dump (scr_tafref_t taf)
{
  scr_int index_, current_slab, current_offset;
  assert (taf_is_valid (taf));

  /* Dump complete structure. */
  scr_trace ("TAFfile: debug dump follows...\n");
  scr_trace ("taf->header =");
  for (index_ = 0; index_ < (scr_int) sizeof (taf->header); index_++)
    scr_trace (" %02x", taf->header[index_]);
  scr_trace ("\n");

  scr_trace ("taf->version = %s\n",
             taf->version == TAF_VERSION_400 ? "4.00"
             : taf->version == TAF_VERSION_390 ? "3.90"
             : taf->version == TAF_VERSION_380 ? "3.80"
             : taf->version == TAF_VERSION_370 ? "3.70" : "[Unknown]");

  scr_trace ("taf->slabs = \n");
  for (index_ = 0; index_ < taf->slab_count; index_++)
    {
      scr_trace ("%3ld : %p, %ld bytes\n", index_,
                 taf->slabs[index_].data, taf->slabs[index_].size);
    }

  scr_trace ("taf->slab_count = %ld\n", taf->slab_count);
  scr_trace ("taf->slabs_allocated = %ld\n", taf->slabs_allocated);
  scr_trace ("taf->current_slab = %ld\n", taf->current_slab);
  scr_trace ("taf->current_offset = %ld\n", taf->current_offset);

  /* Save current location. */
  current_slab = taf->current_slab;
  current_offset = taf->current_offset;

  /* Print out taf lines using taf iterators. */
  scr_trace ("\ntaf iterators:\n");
  taf_first_line (taf);
  for (index_ = 0; taf_more_lines (taf); index_++)
    scr_trace ("%5ld %s\n", index_, taf_next_line (taf));

  /* Restore current location. */
  taf->current_slab = current_slab;
  taf->current_offset = current_offset;
}
