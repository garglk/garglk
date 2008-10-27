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
 * o Put integer and boolean read functions in here?
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "zlib.h"
#include "scare.h"
#include "scprotos.h"


/* Assorted definitions and constants. */
static const sc_uint32	TAF_MAGIC		= 0x5BDCFA41;
enum {			VERSION_HEADER_SIZE	= 14,
			V400_HEADER_EXTRA	= 8 };
static const sc_uint16	OUT_BUFFER_SIZE		= 31744,
			IN_BUFFER_SIZE		= 16384;
static const sc_uint16	GROW_INCREMENT		= 8;
static const sc_char	NEWLINE			= '\n',
			CARRIAGE_RETURN		= '\r',
			NUL			= '\0';

/* Version 4.0, version 3.9, and version 3.8 TAF file signatures. */
static const sc_byte	V400_SIGNATURE[]	=
				{ 0x3C, 0x42, 0x3F, 0xC9, 0x6A, 0x87, 0xC2,
				  0xCF, 0x93, 0x45, 0x3E, 0x61, 0x39, 0xFA };
static const sc_byte	V390_SIGNATURE[]	=
				{ 0x3C, 0x42, 0x3F, 0xC9, 0x6A, 0x87, 0xC2,
				  0xCF, 0x94, 0x45, 0x37, 0x61, 0x39, 0xFA };
static const sc_byte	V380_SIGNATURE[]	=
				{ 0x3C, 0x42, 0x3F, 0xC9, 0x6A, 0x87, 0xC2,
				  0xCF, 0x94, 0x45, 0x36, 0x61, 0x39, 0xFA };

/* Enumerated TAF file version. */
typedef enum {		TAF_VERSION_NONE	= 0,
			TAF_VERSION_400		= 400,
			TAF_VERSION_390		= 390,
			TAF_VERSION_380		= 380 }
sc_tafversion_t;

/*
 * Game TAF data structure.  The game structure contains the original TAF
 * file header, a growable array of "slab" descriptors, each of which holds
 * metadata for a "slab" (around a decompression buffer full of TAF strings),
 * the length of the descriptor array and elements allocated, and a current
 * location for iteration.
 *
 * Saved game files (.TAS) are just like TAF files except that they lack
 * the header.  So for files of this type, the header is all zeroes.
 */
typedef struct sc_slabdesc_s {
	sc_byte			*data;
	sc_uint32		size;
} sc_slabdesc_t;
typedef	sc_slabdesc_t		*sc_slabdescref_t;
typedef struct sc_taf_s {
	sc_uint32		magic;
	sc_byte			header[VERSION_HEADER_SIZE
						+ V400_HEADER_EXTRA];
	sc_tafversion_t		version;
	sc_uint32		total_in_bytes;
	sc_slabdescref_t	slabs;
	sc_uint32		slab_count;
	sc_uint32		slabs_size;
	sc_bool			is_unterminated;
	sc_uint32		current_slab;
	sc_uint32		current_offset;
} sc_taf_t;


/* Microsoft Visual Basic PRNG magic numbers, initial and current state. */
static const sc_int32	PRNG_CST1		= 0x43FD43FD,
			PRNG_CST2		= 0x00C39EC3,
			PRNG_CST3		= 0x00FFFFFF,
			PRNG_INITIAL_STATE	= 0x00A09E86;
static sc_int32		taf_random_state	= 0x00A09E86;

/*
 * taf_random()
 * taf_random_reset()
 *
 * Version 3.9 and version 3.8 games are obfuscated by xor'ing each character
 * with the PRNG in Visual Basic.  So here we have to emulate that, to unob-
 * fuscate data from such game files.  The PRNG generates 0..taf_cst3, which
 * we multiply by 255 and then divide by taf_cst3 + 1 to get output in the
 * range 0..254.  Thanks to Rik Snel for uncovering this obfuscation.
 */
static sc_byte
taf_random (void)
{
	/* Generate and return the next pseudo-random number. */
	taf_random_state = (taf_random_state * PRNG_CST1 + PRNG_CST2)
					& PRNG_CST3;
	return (UCHAR_MAX * (sc_uint32) taf_random_state)
					/ (sc_uint32) (PRNG_CST3 + 1);
}

static void
taf_random_reset (void)
{
	/* Reset PRNG to initial conditions. */
	taf_random_state = PRNG_INITIAL_STATE;
}


/*
 * taf_create_empty()
 *
 * Allocate and return a new, empty TAF structure.
 */
static sc_tafref_t
taf_create_empty (void)
{
	sc_tafref_t	taf;

	/* Create an empty TAF structure. */
	taf = sc_malloc (sizeof (*taf));
	taf->magic		= TAF_MAGIC;
	memset (taf->header, 0, sizeof (taf->header));
	taf->version		= TAF_VERSION_NONE;
	taf->total_in_bytes	= 0;
	taf->slabs		= NULL;
	taf->slab_count		= 0;
	taf->slabs_size		= 0;
	taf->is_unterminated	= FALSE;
	taf->current_slab	= 0;
	taf->current_offset	= 0;

	/* Return the new TAF structure. */
	return taf;
}


/*
 * taf_destroy()
 *
 * Free TAF memory, and destroy a TAF structure.
 */
void
taf_destroy (sc_tafref_t taf)
{
	sc_uint32	slab;
	assert (taf != NULL && taf->magic == TAF_MAGIC);

	/* First free each slab in the slabs array,... */
	for (slab = 0; slab < taf->slab_count; slab++)
		sc_free (taf->slabs[slab].data);

	/*
	 * ...then free slabs growable array, and shred and free the TAF
	 * structure itself.
	 */
	sc_free (taf->slabs);
	memset (taf, 0, sizeof (*taf));
	sc_free (taf);
}


/*
 * taf_append()
 *
 * Append a buffer of TAF lines to an existing TAF structure.  Returns the
 * number of characters consumed from the buffer.
 */
static sc_uint16
taf_append (sc_tafref_t taf, sc_byte *buffer, sc_uint16 length)
{
	sc_uint16	bytes;
	sc_bool		is_unterminated;
	sc_byte		*slab;
	sc_uint32	index;

	/*
	 * Search backwards from the buffer end for the final line feed.
	 * It's expected to always be preceded by a carriage return.
	 * If none found, note this fact for later and take all data.
	 */
	for (bytes = length;
			bytes > 0 && buffer[bytes - 1] != NEWLINE; )
		bytes--;
	if (bytes == 0)
	    {
		is_unterminated = TRUE;
		bytes = length;
	    }
	else
		is_unterminated = FALSE;

	/*
	 * Malloc a new buffer for this much data, and copy the input
	 * buffer into it.
	 */
	slab = sc_malloc (bytes);
	memcpy (slab, buffer, bytes);

	/* Replace slab carriage returns and line feeds with nuls. */
	for (index = 0; index < bytes; index++)
	    {
		if (slab[index] == NEWLINE
				|| slab[index] == CARRIAGE_RETURN)
			slab[index] = NUL;
	    }

	/* If the last buffer was not a complete data line, coalesce. */
	if (taf->is_unterminated)
	    {
		sc_byte		*combined;
		sc_uint32	last;

		/* Combine the last slab and this one into one giant slab. */
		last = taf->slab_count - 1;
		combined = sc_malloc (taf->slabs[last].size + bytes);
		memcpy (combined, taf->slabs[last].data, taf->slabs[last].size);
		memcpy (combined + taf->slabs[last].size, slab, bytes);

		/*
		 * Free the new slab for this buffer, and the last slab, and
		 * replace the last slab with the combination.
		 */
		sc_free (slab);
		sc_free (taf->slabs[last].data);
		taf->slabs[last].data = combined;
		taf->slabs[last].size += bytes;
	    }
	else
	    {
		/* Add a slab descriptor, perhaps extending the slabs array. */
		if (taf->slab_count == taf->slabs_size)
		    {
			taf->slabs_size += GROW_INCREMENT;
			taf->slabs = sc_realloc (taf->slabs,
					taf->slabs_size * sizeof (*taf->slabs));
		    }
		taf->slabs[taf->slab_count].data = slab;
		taf->slabs[taf->slab_count].size = bytes;
		taf->slab_count++;
	    }

	/* Note if this buffer requires that the next be coalesced with it. */
	taf->is_unterminated = is_unterminated;

	/* Return count of buffer bytes consumed. */
	return bytes;
}


/*
 * taf_unobfuscate()
 *
 * Unobfuscate a version 3.9 and version 3.8 TAF file from data read by
 * repeated calls to the callback() function.  Callback() should return the
 * count of bytes placed in the buffer, or 0 if no more (end of file).
 * Assumes that the file has been read past the header.
 */
static sc_bool
taf_unobfuscate (sc_tafref_t taf,
		sc_uint16 (*callback) (void *, sc_byte *, sc_uint16),
		void *opaque, sc_bool is_gamefile)
{
	sc_byte		*buffer;
	sc_int16	bytes, used_bytes, total_bytes, index;

	/* Reset the PRNG, and synchronize with the header already read. */
	taf_random_reset ();
	for (index = 0; index < VERSION_HEADER_SIZE; index++)
		taf_random ();

	/*
	 * Malloc buffer, done to help systems with limited stacks, and
	 * initialize count of bytes read and used in the buffer to zero.
	 */
	buffer = sc_malloc (IN_BUFFER_SIZE);
	used_bytes  = 0;
	total_bytes = 0;

	/* Unobfuscate in buffer sized chunks. */
	do
	    {
		/* Try to obtain more data. */
		bytes = callback (opaque, buffer + used_bytes,
						IN_BUFFER_SIZE - used_bytes);

		/* Unobfuscate data read in. */
		for (index = 0; index < bytes; index++)
			buffer[used_bytes + index] ^= taf_random ();

		/*
		 * Add data read in and unobfuscated to buffer used data,
		 * and if unobfuscated data is available, add it to the TAF.
		 */
		used_bytes += bytes;
		if (used_bytes > 0)
		    {
			sc_uint16	consumed;

			/* Add lines from this buffer to the TAF. */
			consumed = taf_append (taf, buffer, used_bytes);

			/* Move unused buffer data to buffer start. */
			memmove (buffer, buffer + consumed,
						IN_BUFFER_SIZE - consumed);

			/*
			 * Note counts of bytes consumed and remaining in
			 * the buffer.
			 */
			used_bytes  -= consumed;
			total_bytes += consumed;
		    }
	    }
	while (bytes > 0);

	/*
	 * Unobfuscation completed, note the total bytes read.  This value
	 * is actually not used for version 3.9 and version 3.8 games, but
	 * we maintain it just in case.
	 */
	taf->total_in_bytes = total_bytes;
	if (is_gamefile)
		taf->total_in_bytes += VERSION_HEADER_SIZE;

	/* Check that we found the end of the input file as expected. */
	if (used_bytes > 0)
	    {
		sc_error ("taf_unobfuscate: warning:"
			" %d unhandled bytes in the buffer\n", used_bytes);
	    }
	if (taf->is_unterminated)
	    {
		sc_error ("taf_unobfuscate: warning:"
				" unterminated final data slab\n");
	    }

	/* Return successfully. */
	sc_free (buffer);
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
static sc_bool
taf_decompress (sc_tafref_t taf,
		sc_uint16 (*callback) (void *, sc_byte *, sc_uint16),
		void *opaque, sc_bool is_gamefile)
{
	sc_byte		*in_buffer, *out_buffer;
	z_stream	stream;
	sc_int16	status;

	/*
	 * Malloc buffers, done this way rather than as stack variables
	 * for systems such as PalmOS that may have limited stacks.
	 */
	in_buffer  = sc_malloc (IN_BUFFER_SIZE);
	out_buffer = sc_malloc (OUT_BUFFER_SIZE);

	/* Initialize Zlib inflation functions. */
	stream.next_out	= out_buffer;
	stream.avail_out= OUT_BUFFER_SIZE;
	stream.next_in	= in_buffer;
	stream.avail_in	= 0;

	stream.zalloc	= Z_NULL;
	stream.zfree	= Z_NULL;
	stream.opaque	= Z_NULL;

	status = inflateInit (&stream);
	if (status != Z_OK)
	    {
		sc_error ("taf_decompress: inflateInit() error %d\n", status);
		sc_free (in_buffer);
		sc_free (out_buffer);
		return FALSE;
	    }

	/* Inflate the input buffers. */
	for (;;)
	    {
		sc_int16	in_bytes, out_bytes;

		/* If the input buffer is empty, try to obtain more data. */
		if (stream.avail_in == 0)
		    {
			in_bytes = callback (opaque, in_buffer, IN_BUFFER_SIZE);
			stream.next_in	= in_buffer;
			stream.avail_in	= in_bytes;
		    }

		/* Decompress as much stream data as we can. */
		status = inflate (&stream, Z_SYNC_FLUSH);
		if (status != Z_STREAM_END && status != Z_OK)
		    {
			sc_error ("taf_decompress: inflate()"
						" error %d\n", status);
			sc_free (in_buffer);
			sc_free (out_buffer);
			return FALSE;
		    }
		out_bytes = OUT_BUFFER_SIZE - stream.avail_out;

		/* See if decompressed data is available. */
		if (out_bytes > 0)
		    {
			sc_uint16	consumed;

			/* Add lines from this buffer to the TAF. */
			consumed = taf_append (taf, out_buffer, out_bytes);

			/* Move unused buffer data to buffer start. */
			memmove (out_buffer, out_buffer + consumed,
						OUT_BUFFER_SIZE - consumed);

			/* Reset inflation stream for available space. */
			stream.next_out	= out_buffer + out_bytes - consumed;
			stream.avail_out+= consumed;
		    }

		/* If at inflation stream end, leave loop. */
		if (status == Z_STREAM_END)
			break;

		/* If input is empty, and output is empty, also leave loop. */
		if (stream.avail_in == 0
				&& stream.avail_out == 0)
			break;
	    }

	/*
	 * Decompression completed, note the total bytes read for use when
	 * locating resources later on in the file.  For what it's worth,
	 * this value is only used in version 4.0 games.
	 */
	taf->total_in_bytes = stream.total_in;
	if (is_gamefile)
		taf->total_in_bytes += VERSION_HEADER_SIZE + V400_HEADER_EXTRA;

	/* End inflation. */
	status = inflateEnd (&stream);
	if (status != Z_OK)
	    {
		sc_error ("taf_decompress: warning:"
				" inflateEnd() error %d\n", status);
	    }
	if (taf->is_unterminated)
	    {
		sc_error ("taf_decompress: warning:"
				" unterminated final data slab\n");
	    }

	/* Return successfully. */
	sc_free (in_buffer);
	sc_free (out_buffer);
	return TRUE;
}


/*
 * taf_create()
 *
 * Create a TAF structure from data read in by repeated calls to the
 * callback() function.  Callback() should return the count of bytes placed
 * in the buffer, or 0 if no more (end of file).
 */
sc_tafref_t
taf_create (sc_uint16 (*callback) (void *, sc_byte *, sc_uint16),
		void *opaque, sc_bool is_gamefile)
{
	sc_tafref_t	taf;
	sc_bool		status;
	assert (callback != NULL);

	/* Create an empty TAF structure. */
	taf = taf_create_empty ();

	/*
	 * Determine the TAF file version in use.  For saved games, we
	 * always use version 4.0 format.  For others, it's determined from
	 * the header.
	 */
	if (is_gamefile)
	    {
		sc_int16	in_bytes;

		/*
		 * Read in the ADRIFT header for game files.  Start by
		 * reading in the shorter header common to all.
		 */
		in_bytes = callback (opaque, taf->header, VERSION_HEADER_SIZE);
		if (in_bytes != VERSION_HEADER_SIZE)
		    {
			sc_error ("taf_create:"
				" not enough data for standard TAF header\n");
			taf_destroy (taf);
			return NULL;
		    }

		/*
		 * Compare the header with the known TAF signatures, and set
		 * TAF version appropriately.
		 */
		if (!memcmp (taf->header, V400_SIGNATURE, VERSION_HEADER_SIZE))
		    {
			/* Read in the version 4.0 header extension. */
			in_bytes = callback (opaque,
					taf->header + VERSION_HEADER_SIZE,
							V400_HEADER_EXTRA);
			if (in_bytes != V400_HEADER_EXTRA)
			    {
				sc_error ("taf_create:"
					" not enough data for extended"
					" TAF header\n");
				taf_destroy (taf);
				return NULL;
			    }

			taf->version = TAF_VERSION_400;
		    }
		else if (!memcmp (taf->header, V390_SIGNATURE,
							VERSION_HEADER_SIZE))
			taf->version = TAF_VERSION_390;
		else if (!memcmp (taf->header, V380_SIGNATURE,
							VERSION_HEADER_SIZE))
			taf->version = TAF_VERSION_380;
		else
		    {
			taf_destroy (taf);
			return NULL;
		    }
	    }
	else
	    {
		/* Saved games are always considered to be version 4.0. */
		taf->version = TAF_VERSION_400;
	    }

	/*
	 * Call the appropriate game file reader function.  For version 4.0
	 * games, data is compressed with Zlib.  For version 3.9 and version
	 * 3.8 games, it's obfuscated with the Visual Basic PRNG.
	 */
	switch (taf->version)
	    {
	    case TAF_VERSION_400:
		status = taf_decompress (taf, callback, opaque, is_gamefile);
		break;

	    case TAF_VERSION_390:
	    case TAF_VERSION_380:
		status = taf_unobfuscate (taf, callback, opaque, is_gamefile);
		break;

	    default:
		sc_fatal ("taf_create: invalid version\n");
		status = FALSE;
		break;
	    }
	if (!status)
	    {
		taf_destroy (taf);
		return NULL;
	    }

	/* Return successfully. */
	return taf;
}


/*
 * taf_read_buffer()
 *
 * Callback function used for building a TAF from an open file stream.
 * The function reads file data from the stream passed in as the
 * opaque pointer, into the buffer, up to the buffer length, and returns
 * the count of bytes read.
 */
static sc_uint16
taf_read_buffer (void *opaque, sc_byte *buffer, sc_uint16 length)
{
	FILE		*stream = (FILE *) opaque;
	sc_uint16	bytes;

	/* Read repeatedly until buffer full or file end. */
	bytes = 0;
	while (!feof (stream)
			&& bytes < length)
	    {
		bytes += fread (buffer + bytes, 1, length - bytes, stream);
		if (ferror (stream))
		    {
			sc_error ("taf_read_buffer: warning:"
						" read error\n");
			break;
		    }
	    }

	/* Return count of bytes buffered. */
	return bytes;
}


/*
 * taf_create_file()
 *
 * Create and return a new TAF structure from an open file stream.
 */
sc_tafref_t
taf_create_file (FILE *stream, sc_bool is_gamefile)
{
	assert (stream != NULL);

	/* Create and return a TAF structure from the file stream. */
	return taf_create (taf_read_buffer, stream, is_gamefile);
}


/*
 * taf_create_filename()
 *
 * Create and return a new TAF structure built by reading a given file.
 */
sc_tafref_t
taf_create_filename (const sc_char *filename, sc_bool is_gamefile)
{
	FILE		*stream;
	sc_tafref_t	taf;
	assert (filename != NULL);

	/* Open the file. */
	stream = fopen (filename, "rb");
	if (stream == NULL)
	    {
		sc_error ("taf_create_filename: fopen error\n");
		return NULL;
	    }

	/* Obtain a game from the open stream, then close the stream. */
	taf = taf_create_file (stream, is_gamefile);
	fclose (stream);

	/* Return the new TAF structure, or NULL if create failed. */
	return taf;
}


/*
 * taf_first_line()
 *
 * Iterator rewind function, reset current slab location to TAF data start.
 */
void
taf_first_line (sc_tafref_t taf)
{
	assert (taf != NULL && taf->magic == TAF_MAGIC);

	/* Set current locations to TAF start. */
	taf->current_slab	= 0;
	taf->current_offset	= 0;
}


/*
 * taf_next_line()
 *
 * Iterator function, return the next line of data from a TAF, or NULL
 * if no more lines.
 */
sc_char *
taf_next_line (sc_tafref_t taf)
{
	sc_char		*line;
	assert (taf != NULL && taf->magic == TAF_MAGIC);

	/* If there is a next line, return it and advance current. */
	if (taf->current_slab < taf->slab_count)
	    {
		/* Get the effective address of the current line. */
		line = (sc_char *) taf->slabs[taf->current_slab].data
						+ taf->current_offset;

		/*
		 * Advance to the next line.  The + 2 skips the NULs used to
		 * replace the carriage return and line feed.
		 */
		taf->current_offset += strlen (line) + 2;
		if (taf->current_offset >= taf->slabs[taf->current_slab].size)
		    {
			taf->current_slab++;
			taf->current_offset = 0;
		    }
	    }
	else
		line = NULL;

	/* Return the line, or NULL if none. */
	return line;
}


/*
 * taf_more_lines()
 *
 * Iterator end function, returns true if no more TAF lines are readable.
 */
sc_bool
taf_more_lines (sc_tafref_t taf)
{
	assert (taf != NULL && taf->magic == TAF_MAGIC);

	/* Return true if not at TAF data end. */
	return (taf->current_slab < taf->slab_count);
}


/*
 * taf_get_game_data_length()
 *
 * Returns the number of bytes read to decompress the game.  Resources are
 * appended to the TAF file after the game, so this value allows them to
 * be located.
 */
sc_uint32
taf_get_game_data_length (sc_tafref_t taf)
{
	assert (taf != NULL && taf->magic == TAF_MAGIC);

	/*
	 * Return the count of bytes inflated; this includes the TAF header
	 * length for TAF, rather than TAS, files.  For TAS files, the count
	 * of file bytes read is irrelevant, and is never used.
	 */
	return taf->total_in_bytes;
}


/*
 * taf_get_version()
 *
 * Return the version number of the TAF file, 400, 390, or 380.
 */
sc_uint16
taf_get_version (sc_tafref_t taf)
{
	assert (taf != NULL && taf->magic == TAF_MAGIC);

	assert (taf->version != TAF_VERSION_NONE);
	return taf->version;
}


/*
 * taf_debug_dump()
 *
 * Print out a complete TAF structure.
 */
void
taf_debug_dump (sc_tafref_t taf)
{
	sc_uint32	index;
	sc_uint32	current_slab, current_offset;
	assert (taf != NULL && taf->magic == TAF_MAGIC);

	/* Dump complete structure. */
	sc_trace ("taf->header =");
	for (index = 0; index < sizeof (taf->header); index++)
		sc_trace (" 0x%02X", taf->header[index]);
	sc_trace ("\n");
	sc_trace ("taf->version = %s\n",
			taf->version == TAF_VERSION_400 ? "4.00" :
			taf->version == TAF_VERSION_390 ? "3.90" :
			taf->version == TAF_VERSION_380 ? "3.80" : "[Unknown]");
	sc_trace ("taf->slabs = \n");
	for (index = 0; index < taf->slab_count; index++)
		sc_trace ("%3ld : %p, %lu bytes\n", index,
			taf->slabs[index].data, taf->slabs[index].size);
	sc_trace ("taf->slab_count = %lu\n", taf->slab_count);
	sc_trace ("taf->slabs_size = %lu\n", taf->slabs_size);
	sc_trace ("taf->current_slab = %lu\n", taf->current_slab);
	sc_trace ("taf->current_offset = %lu\n", taf->current_offset);

	/* Save current location. */
	current_slab	= taf->current_slab;
	current_offset	= taf->current_offset;

	/* Print out taf lines using taf iterators. */
	sc_trace ("\ntaf iterators:\n");
	taf_first_line (taf);
	for (index = 0; taf_more_lines (taf); index++)
		sc_trace ("%5ld %s\n", index, taf_next_line (taf));

	/* Restore current location. */
	taf->current_slab	= current_slab;
	taf->current_offset	= current_offset;
}
