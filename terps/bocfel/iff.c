// Copyright 2010-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "iff.h"
#include "io.h"

// Translate an IFF Type ID into the corresponding 32-bit integer.
uint32_t STRID(TypeID type)
{
    return (((uint32_t)(*type)[0]) << 24) |
           (((uint32_t)(*type)[1]) << 16) |
           (((uint32_t)(*type)[2]) <<  8) |
           (((uint32_t)(*type)[3]) <<  0);
}

struct zterp_iff {
    zterp_io *io;
    uint32_t type;
    long offset;
    uint32_t size;

    struct zterp_iff *next;
};

void zterp_iff_free(zterp_iff *iff)
{
    while (iff != NULL) {
        zterp_iff *tmp = iff->next;
        free(iff);
        iff = tmp;
    }
}

zterp_iff *zterp_iff_parse(zterp_io *io, TypeID form_type)
{
    uint32_t type;

    zterp_iff *iff = NULL, *tail = NULL;

    if (!zterp_io_seek(io, 0, SEEK_SET)) {
        goto err;
    }

    if (!zterp_io_read32(io, &type) || type != STRID(&"FORM")) {
        goto err;
    }

    if (!zterp_io_seek(io, 4, SEEK_CUR)) {
        goto err;
    }

    if (!zterp_io_read32(io, &type) || type != STRID(form_type)) {
        goto err;
    }

    while (zterp_io_read32(io, &type)) {
        uint32_t size;
        zterp_iff *new;

        if (!zterp_io_read32(io, &size)) {
            goto err;
        }

        new = malloc(sizeof *new);
        if (new == NULL) {
            goto err;
        }

        new->type = type;
        new->io = io;
        new->offset = zterp_io_tell(io);
        new->size = size;
        new->next = NULL;

        if (iff == NULL) {
            iff = new;
        } else {
            tail->next = new;
        }

        tail = new;

        if (new->offset == -1) {
            goto err;
        }

        if (size & 1) {
            size++;
        }

        if (!zterp_io_seek(io, size, SEEK_CUR)) {
            goto err;
        }
    }

    return iff;

err:
    zterp_iff_free(iff);

    return NULL;
}

bool zterp_iff_find(zterp_iff *iff, TypeID tag, uint32_t *size)
{
    while (iff != NULL) {
        if (iff->type == STRID(tag)) {
            if (!zterp_io_seek(iff->io, iff->offset, SEEK_SET)) {
                return false;
            }
            *size = iff->size;

            return true;
        }

        iff = iff->next;
    }

    return false;
}
