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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "blorb.h"
#include "iff.h"
#include "io.h"

struct zterp_blorb {
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

    iff = zterp_iff_parse(io, &"IFRS");
    if (iff == NULL) {
        goto err;
    }

    if (!zterp_iff_find(iff, &"RIdx", &size)) {
        goto err;
    }
    zterp_iff_free(iff);

    if (!zterp_io_read32(io, &nresources)) {
        goto err;
    }

    if ((nresources * 12) + 4 != size) {
        goto err;
    }

    blorb = malloc(sizeof *blorb);
    if (blorb == NULL) {
        goto err;
    }

    blorb->io = io;
    blorb->nchunks = nresources;
    blorb->chunks = malloc(nresources * sizeof *blorb->chunks);
    if (blorb->chunks == NULL) {
        goto err;
    }

    for (uint32_t i = 0; i < nresources; i++) {
        uint32_t usage, number, start, type;
        long saved;

        if (!zterp_io_read32(io, &usage) || !zterp_io_read32(io, &number) || !zterp_io_read32(io, &start)) {
            goto err;
        }

        if (usage != BLORB_PICT && usage != BLORB_SND && usage != BLORB_EXEC && usage != BLORB_DATA) {
            goto err;
        }

        saved = zterp_io_tell(io);
        if (saved == -1) {
            goto err;
        }

        if (!zterp_io_seek(io, start, SEEK_SET)) {
            goto err;
        }

        if (!zterp_io_read32(io, &type) || !zterp_io_read32(io, &size)) {
            goto err;
        }

        if (!zterp_io_seek(io, saved, SEEK_SET)) {
            goto err;
        }

        if (type == STRID(&"FORM")) {
            start -= 8;
            size += 8;
        }

        blorb->chunks[i].usage = usage;
        blorb->chunks[i].number = number;
        blorb->chunks[i].type = type;
        blorb->chunks[i].offset = start + 8;
        blorb->chunks[i].size = size;

        blorb->chunks[i].name[0] = (type >> 24) & 0xff;
        blorb->chunks[i].name[1] = (type >> 16) & 0xff;
        blorb->chunks[i].name[2] = (type >>  8) & 0xff;
        blorb->chunks[i].name[3] = (type >>  0) & 0xff;
        blorb->chunks[i].name[4] = 0;
    }

    return blorb;

err:
    zterp_blorb_free(blorb);

    return NULL;
}

void zterp_blorb_free(struct zterp_blorb *blorb)
{
    if (blorb != NULL) {
        free(blorb->chunks);
    }
    free(blorb);
}

const zterp_blorb_chunk *zterp_blorb_find(struct zterp_blorb *blorb, uint32_t usage, uint32_t number)
{
    for (size_t i = 0; i < blorb->nchunks; i++) {
        if (blorb->chunks[i].usage == usage && blorb->chunks[i].number == number) {
            return &blorb->chunks[i];
        }
    }

    return NULL;
}
