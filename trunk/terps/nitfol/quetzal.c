/*  Nitfol - z-machine interpreter using Glk for output.
    Copyright (C) 1999  Evin Robertson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

    The author can be reached at nitfol@deja.com
*/
#include "nitfol.h"

/* Note that quetzal stack save/restore is handled at the bottom of stack.c */


/* Sets *diff to a malloced quetzal diff of the first length bytes of a and b
 * *diff_length is set to the length of *diff.  Returns successfulness.  */
BOOL quetzal_diff(const zbyte *a, const zbyte *b, glui32 length,
		  zbyte **diff, glui32 *diff_length, BOOL do_utf8)
{
  /* Worst case: every other byte is the same as in the original, so we have
     to store 1.5 times the original length.  Allocate a couple bytes extra
     to be on the safe side. (yes, I realize it could actually be twice the
     original length if you use a really bad algorithm, but I don't) */
  zbyte *attempt = (zbyte *) n_malloc((length * 3) / 2 + 2);
						
  glui32 attempt_len = 0;
  glui32 same_len;

  *diff = NULL;

  while(length) {
    /* Search through consecutive identical bytes */
    for(same_len = 0; same_len < length && a[same_len] == b[same_len]; same_len++)
      ;
    a += same_len; b += same_len; length -= same_len;

    if(length) {
      /* If we hit the end of the region, we don't have to record that the
	 bytes at the end are the same */
      while(same_len) {
	attempt[attempt_len++] = 0;
	same_len--; /* We always store length-1 */
	if(do_utf8) {
	  if(same_len <= 0x7f) {
	    attempt[attempt_len++] = same_len;
	    same_len = 0;
	  } else {
	    if(same_len <= 0x7fff) {
	      attempt[attempt_len++] = (same_len & 0x7f) | 0x80;
	      attempt[attempt_len++] = (same_len & 0x7f80) >> 7;
	      same_len = 0;
	    } else {
	      attempt[attempt_len++] = (0x7fff & 0x7f) | 0x80;
	      attempt[attempt_len++] = (0x7fff & 0x7f80) >> 7;
	      same_len -= 0x7fff;
	    }
	  }
	} else {
	  if(same_len <= 0xff) {
	    attempt[attempt_len++] = same_len;
	    same_len = 0;
	  } else {
	    attempt[attempt_len++] = 0xff;
	    same_len -= 0xff;
	  }
	}
      }

      attempt[attempt_len++] = *a++ ^ *b++;
      length--;
    }
  }

  *diff = (zbyte *) n_realloc(attempt, attempt_len);
  *diff_length = attempt_len;
  return TRUE;
}

/* Applies a quetzal diff to dest */
BOOL quetzal_undiff(zbyte *dest, glui32 length,
		    const zbyte *diff, glui32 diff_length, BOOL do_utf8)
{
  glui32 iz = 0;
  glui32 id;

  for(id = 0; id < diff_length; id++) {
    if(diff[id] == 0) {
      unsigned runlen;
      if(++id >= diff_length)
	return FALSE;  /* Incomplete run */
      runlen = diff[id];
      if(do_utf8 && diff[id] & 0x80) {
	if(++id >= diff_length)
	  return FALSE; /* Incomplete extended run */
	runlen = (runlen & 0x7f) | (((unsigned) diff[id]) << 7);
      }
      iz += runlen + 1;
    } else {
      dest[iz] ^= diff[id];
      iz++;
    }
    if(iz >= length)
      return FALSE; /* Too long */
  }
  return TRUE;
}


static unsigned qifhd[] = { 2, 6, 2, 3, 0 };
enum qifhdnames { qrelnum, qsernum, qchecksum = qsernum + 6, qinitPC };

BOOL savequetzal(strid_t stream)
{
  unsigned n;
  glui32 start_loc, last_loc;
  glui32 qs[13];
  glui32 hdrsize, memsize, stksize, padding, chunksize, intdsize;
  zbyte *original = (zbyte *) n_malloc(dynamic_size);
  zbyte *diff = NULL;

  glk_stream_set_position(current_zfile, zfile_offset, seekmode_Start);
  glk_get_buffer_stream(current_zfile, (char *) original, dynamic_size);

  if(!quetzal_diff(original, z_memory, dynamic_size, &diff, &memsize, FALSE)
     || memsize >= dynamic_size) {  /* If we're losing try uncompressed */
    if(diff)
      free(diff);
    diff = NULL;
    memsize = dynamic_size;
  }

  hdrsize = 13;
  stksize = get_quetzal_stack_size();
  intdsize = intd_get_size();
  padding = 8 + (hdrsize & 1) +
            8 + (memsize & 1) +
            8 + (stksize & 1);
  if(intdsize)
    padding += 8 + (intdsize & 1);
  chunksize = 4 + hdrsize + memsize + stksize + intdsize + padding;


  iffputchunk(stream, "FORM", chunksize);
  start_loc = glk_stream_get_position(stream);

  w_glk_put_buffer_stream(stream, "IFZS", 4);

  iffputchunk(stream, "IFhd", hdrsize);
  last_loc = glk_stream_get_position(stream);
  qs[qrelnum] = LOWORD(HD_RELNUM);
  for(n = 0; n < 6; n++)
    qs[qsernum + n] = LOBYTE(HD_SERNUM + n);
  qs[qchecksum] = LOWORD(HD_CHECKSUM);
  qs[qinitPC] = PC;
  emptystruct(stream, qifhd, qs);

  if(glk_stream_get_position(stream) - last_loc != hdrsize) {
    n_show_error(E_SAVE, "header size miscalculation", glk_stream_get_position(stream) - last_loc);
    return FALSE;
  }

  if(intdsize) {
    iffputchunk(stream, "IntD", intdsize);

    last_loc = glk_stream_get_position(stream);
    intd_filehandle_make(stream);

    if(glk_stream_get_position(stream) - last_loc != intdsize) {
      n_show_error(E_SAVE, "IntD size miscalculation", glk_stream_get_position(stream) - last_loc);
      return FALSE;
    }
  }


  if(diff) {
    iffputchunk(stream, "CMem", memsize);
    last_loc = glk_stream_get_position(stream);
    w_glk_put_buffer_stream(stream, (char *) diff, memsize);
  } else {
    iffputchunk(stream, "UMem", memsize);
    last_loc = glk_stream_get_position(stream);
    w_glk_put_buffer_stream(stream, (char *) z_memory, dynamic_size);
  }

  if(glk_stream_get_position(stream) - last_loc != memsize) {
    n_show_error(E_SAVE, "memory size miscalculation", glk_stream_get_position(stream) - last_loc);
    return FALSE;
  }

  iffputchunk(stream, "Stks", stksize);
  last_loc = glk_stream_get_position(stream);
  quetzal_stack_save(stream);

  if(glk_stream_get_position(stream) - last_loc != stksize) {
    n_show_error(E_SAVE, "stack miscalculation", glk_stream_get_position(stream) - last_loc);
    return FALSE;
  }

  if(glk_stream_get_position(stream) - start_loc != chunksize) {
    n_show_error(E_SAVE, "chunks size miscalculation", glk_stream_get_position(stream) - last_loc);
    return FALSE;
  }

  return TRUE;
}


BOOL restorequetzal(strid_t stream)
{
  char desttype[4];
  glui32 chunksize;
  glui32 start;

  if(!ifffindchunk(stream, "FORM", &chunksize, 0)) {
    n_show_error(E_SAVE, "no FORM chunk", 0);
    return FALSE;
  }

  glk_get_buffer_stream(stream, desttype, 4);
  if(n_strncmp(desttype, "IFZS", 4) != 0) {
    n_show_error(E_SAVE, "FORM chunk not IFZS; this isn't a quetzal file", 0);
    return FALSE;
  }

  start = glk_stream_get_position(stream);

  if(!ifffindchunk(stream, "IFhd", &chunksize, start)) {
    n_show_error(E_SAVE, "no IFhd chunk", 0);
    return FALSE;
  } else {
    unsigned n;
    glui32 qsifhd[10];
    
    fillstruct(stream, qifhd, qsifhd, NULL);

    if(qsifhd[qrelnum] != LOWORD(HD_RELNUM)) {
      n_show_error(E_SAVE, "release number does not match", qsifhd[qrelnum]);
      return FALSE;
    }
    for(n = 0; n < 6; n++) {
      if(qsifhd[qsernum + n] != LOBYTE(HD_SERNUM + n)) {
	n_show_error(E_SAVE, "serial number does not match", n);
	return FALSE;
      }
    }
    if(qsifhd[qchecksum] != LOWORD(HD_CHECKSUM)) {
      n_show_error(E_SAVE, "checksum does not match", qsifhd[qchecksum]);
      return FALSE;
    }
    if(qsifhd[qinitPC] > total_size) {
      n_show_error(E_SAVE, "PC past end of memory", qsifhd[qinitPC]);
      return FALSE;
    }
    
    PC = qsifhd[qinitPC];
  }
  if(!ifffindchunk(stream, "UMem", &chunksize, start)) {
    if(!ifffindchunk(stream, "CMem", &chunksize, start)) {
      n_show_error(E_SAVE, "no memory chunk (UMem or CMem)", 0);
      return FALSE;
    } else {
      zbyte *compressed_chunk = (zbyte *) malloc(chunksize);
      glk_get_buffer_stream(stream, (char *) compressed_chunk, chunksize);

      glk_stream_set_position(current_zfile, zfile_offset, seekmode_Start);
      glk_get_buffer_stream(current_zfile, (char *) z_memory, dynamic_size);

      if(!quetzal_undiff(z_memory, dynamic_size,
			 compressed_chunk, chunksize, FALSE)) {
	n_show_error(E_SAVE, "error in compressed data", 0);
	return FALSE;
      }
    }
  } else {
    if(chunksize != dynamic_size) {
      n_show_error(E_SAVE, "uncompressed memory chunk not expected size",
		 chunksize);
      return FALSE;
    }
    glk_get_buffer_stream(stream, (char *) z_memory, chunksize);
  }

  if(!ifffindchunk(stream, "Stks", &chunksize, start)) {
    n_show_error(E_SAVE, "no Stks chunk", 0);
    return FALSE;
  } else {
    if(!quetzal_stack_restore(stream, chunksize))
      return FALSE;
  }

  return TRUE;
}

static unsigned qintd[] = { 4, 1, 1, 2, 4 };
enum qintdnames { qopid, qflags, qcontid, qresrvd, qintid };

strid_t quetzal_findgamefile(strid_t stream)
{
  char desttype[4];
  glui32 chunksize;
  glui32 start;

  if(!ifffindchunk(stream, "FORM", &chunksize, 0))
    return 0;

  glk_get_buffer_stream(stream, desttype, 4);
  if(n_strncmp(desttype, "IFZS", 4) != 0)
    return 0;

  start = glk_stream_get_position(stream);

  if(ifffindchunk(stream, "IntD", &chunksize, start)) {
    glui32 qsintd[6];
    strid_t file;
    fillstruct(stream, qintd, qsintd, NULL);
    file = intd_filehandle_open(stream, qsintd[qopid],
				qsintd[qcontid], qsintd[qintid],
				chunksize - 12);
    if(file)
      return file;
  }

  if(ifffindchunk(stream, "IFhd", &chunksize, start)) {
    unsigned n;
    glui32 qsifhd[10];
    strid_t file = 0;
    char serial[6];
    
    fillstruct(stream, qifhd, qsifhd, NULL);

    for(n = 0; n < 6; n++)
      serial[n] = qsifhd[qsernum + n];

    do {
      file = startup_findfile();
      if(file) {
	if(check_game_for_save(file, qsifhd[qrelnum], serial,
			       qsifhd[qchecksum]))
	  return file;
      }
    } while(file);
  }

  return 0;
}


