/*-
 * Copyright 2010-2012 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2 or 3, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "blorb.h"
#include "iff.h"
#include "io.h"

struct zterp_blorb
{
  struct zterp_io *io;
  struct zterp_iff *iff;

  size_t nchunks;
  zterp_blorb_chunk *chunks;
};

struct zterp_blorb *zterp_blorb_parse(zterp_io *io)
{
  uint32_t size;
  uint32_t nresources;
  zterp_iff *iff;
  struct zterp_blorb *blorb = NULL;

  iff = zterp_iff_parse(io, "IFRS");
  if(!zterp_iff_find(iff, "RIdx", &size)) goto err;
  zterp_iff_free(iff);

  if(!zterp_io_read32(io, &nresources)) goto err;

  if((nresources * 12) + 4 != size) goto err;

  blorb = malloc(sizeof *blorb);
  if(blorb == NULL) goto err;

  blorb->io = io;
  blorb->nchunks = 0;
  blorb->chunks = NULL;

  for(uint32_t i = 0; i < nresources; i++)
  {
    uint32_t usage, number, start, type;
    zterp_blorb_chunk *new;
    long saved;
    size_t idx;

    if(!zterp_io_read32(io, &usage) || !zterp_io_read32(io, &number) || !zterp_io_read32(io, &start)) goto err;

    if(usage != BLORB_PICT && usage != BLORB_SND && usage != BLORB_EXEC) goto err;

    saved = zterp_io_tell(io);
    if(saved == -1) goto err;

    if(zterp_io_seek(io, start, SEEK_SET) == -1) goto err;

    if(!zterp_io_read32(io, &type) || !zterp_io_read32(io, &size)) goto err;

    if(zterp_io_seek(io, saved, SEEK_SET) == -1) goto err;

    if(type == STRID("FORM"))
    {
      start -= 8;
      size += 8;
    }

    /* Not really efficient, but does it matter? */
    new = realloc(blorb->chunks, sizeof *new * ++blorb->nchunks);
    if(new == NULL) goto err;
    blorb->chunks = new;

    idx = blorb->nchunks - 1;

    new[idx].usage = usage;
    new[idx].number = number;
    new[idx].type = type;
    memcpy(new[idx].name, IDSTR(type), 5);
    new[idx].offset = start + 8;
    new[idx].size = size;
  }

  return blorb;

err:
  zterp_blorb_free(blorb);

  return NULL;
}

void zterp_blorb_free(struct zterp_blorb *blorb)
{
  if(blorb != NULL) free(blorb->chunks);
  free(blorb);
}

const zterp_blorb_chunk *zterp_blorb_find(struct zterp_blorb *blorb, uint32_t usage, uint32_t number)
{
  for(size_t i = 0; i < blorb->nchunks; i++)
  {
    if(blorb->chunks[i].usage == usage && blorb->chunks[i].number == number) return &blorb->chunks[i];
  }

  return NULL;
}
