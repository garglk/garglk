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
#include <stdint.h>
#include <string.h>

#include "iff.h"
#include "io.h"

struct zterp_iff
{
  zterp_io *io;
  uint32_t tag;
  long offset;
  uint32_t size;

  struct zterp_iff *next;
};

void zterp_iff_free(zterp_iff *iff)
{
  while(iff != NULL)
  {
    zterp_iff *tmp = iff->next;
    free(iff);
    iff = tmp;
  }
}

zterp_iff *zterp_iff_parse(zterp_io *io, const char type[4])
{
  uint32_t tag;

  zterp_iff *iff = NULL, *tail = NULL;

  if(zterp_io_seek(io, 0, SEEK_SET) == -1) goto err;

  if(!zterp_io_read32(io, &tag) || tag != STRID("FORM")) goto err;

  if(zterp_io_seek(io, 4, SEEK_CUR) == -1) goto err;

  if(!zterp_io_read32(io, &tag) || tag != STRID(type)) goto err;

  while(zterp_io_read32(io, &tag))
  {
    uint32_t size;
    zterp_iff *new;

    if(!zterp_io_read32(io, &size)) goto err;

    new = malloc(sizeof *new);
    if(new == NULL) goto err;

    new->tag = tag;
    new->io = io;
    new->offset = zterp_io_tell(io);
    new->size = size;
    new->next = NULL;

    if(iff == NULL) iff = new;
    else            tail->next = new;

    tail = new;

    if(new->offset == -1) goto err;

    if(size & 1) size++;

    if(zterp_io_seek(io, size, SEEK_CUR) == -1) goto err;
  }

  return iff;

err:
  zterp_iff_free(iff);

  return NULL;
}

int zterp_iff_find(zterp_iff *iff, const char tag[4], uint32_t *size)
{
  while(iff != NULL)
  {
    if(iff->tag == STRID(tag))
    {
      if(zterp_io_seek(iff->io, iff->offset, SEEK_SET) == -1) return 0;
      *size = iff->size;

      return 1;
    }

    iff = iff->next;
  }

  return 0;
}
