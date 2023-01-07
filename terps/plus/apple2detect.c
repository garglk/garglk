//
//  apple2detect.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-08-03.
//

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "definitions.h"
#include "graphics.h"
#include "companionfile.h"

#include "apple2detect.h"

typedef struct imglist {
    char *filename;
    size_t offset;
} imglist;

static const imglist a2listFantastic[] = {
    { "R001", 0x1e800 },
    { "R002", 0x1df00 },
    { "R004", 0x1db00},
    { "R005", 0x1cb00 },
    { "R006", 0x1c200 },
    { "R007", 0x1bb00 },
    { "R008", 0x1b300 },
    { "R009", 0x12400 },
    { "R010", 0x1aa00 },
    { "R011", 0x1a100 },
    { "R012", 0x18e00 },
    { "R013", 0x18b00 },
    { "R014", 0x18100 },
    { "R015", 0x17800 },
    { "R016", 0x16d00 },
    { "R017", 0x16800 },
    { "R018", 0x15c00 },
    { "R019", 0x15500 },
    { "R020", 0x14d00 },
    { "R021", 0x13e00 },
    { "R022", 0x13300 },
    { "B020", 0xc10e },
    { "B002", 0x11500 },
    { "B003", 0x10f00 },
    { "B004", 0x10a00 },
    { "B005", 0x10000 },
    { "B006", 0xfb00 },
    { "B007", 0xf900 },
    { "B008", 0xf700 },
    { "B009", 0xf200 },
    { "B010", 0xe800 },
    { "B011", 0xe600 },
    { "B012", 0xe400 },
    { "B014", 0xe300 },
    { "B015", 0xc600 },
    { "B016", 0xde00 },
    { "B017", 0xda00 },
    { "B018", 0xd800 },
    { "B019", 0xd700 },
    { "B013", 0xd000 },
    { "B020", 0xcc00 },
    { "B021", 0xca00 },
    { "B022", 0xc900 },
    { "B023", 0xc800 },
    { "S000", 0x1000 },
    { "S001", 0x2000 },
    { "S002", 0x2400 },
    { "S003", 0x2600 },
    { "S004", 0x2800 },
    { "S005", 0x2a00 },
    { "S006", 0x2c00 },
    { "S007", 0x2e00 },
    { "S008", 0x3500 },
    { "S009", 0x3600 },
    { "S010", 0x3700 },
    { "S011", 0x3900 },
    { "S012", 0x3b00 },
    { "S013", 0x3c00 },
    { "S014", 0x3f00 },
    { "S015", 0x4100 },
    { "S016", 0x4200 },
    { "S017", 0x4300 },
    { "S018", 0x4400 },
    { "S019", 0x4500 },
    { "S020", 0x4600 },
    { NULL, 0, }
};

static const imglist a2listSpidey[] = {
    { "R001", 0x19f00 },
    { "R002", 0x19900 },
    { "R003", 0x14200 },
    { "R005", 0xdf00 },
    { "R006", 0x19300 },
    { "R007", 0x18500 },
    { "R008", 0x17800 },
    { "R013", 0x16f00 },
    { "R015", 0xd700 },
    { "R017", 0x16100 },
    { "R018", 0xce00 },
    { "R019", 0x13400 },
    { "R022", 0x12400},
    { "R024", 0x10000 },
    { "R025", 0x15700 },
    { "R034", 0x14f00 },
    { "R037", 0x11500 },
    { "R040", 0xf100 },
    { "B015", 0x5d00 }, // Bio gem
    { "B016", 0xbd00 }, // Egg
    { "B021", 0xc100 }, // Stopped fan
    { "B023", 0xb900 }, // Shaft
    { "B025", 0xc600 }, // wall mirror
    { "B027", 0xb700 }, // open air duct
    { "B028", 0xb500 }, // strange cloud
    { "B031", 0xae00 }, // Octopus
    { "B033", 0xa900 }, // Electro
    { "B034", 0xa400 }, // Stunned Ock
    { "B035", 0x9f00 }, // Stunned Electro
    { "B038", 0x9c00 }, // Lizard
    { "B044", 0x7300 },
    { "B049", 0x7700 },
    { "B050", 0x6500 },
    { "B051", 0x9100 },
    { "B052", 0x8d00 },
    { "B053", 0x7000 },
    { "B055", 0x6800 },
    { "B056", 0x6b00 },
    { "B058", 0x8700 },
    { "B059", 0x8500 },
    { "B060", 0x8100 },
    { "B062", 0x7f00 }, // Ringmaster
    { "B071", 0x7800 }, // Clock
    { "S000", 0x5100 },
    { "S001", 0x6e00 },
    { "S002", 0x7100 },
    { "S003", 0x7400 },
    { "S004", 0x7700 },
    { "S005", 0x7a00 },
    { "S009", 0x4200 },
    { "S010", 0x3800 },
    { "S011", 0x1900 },
    { "S012", 0x2300 },
    { "S013", 0x2900 },
    { "S014", 0x6200 },
    { "S015", 0x6700 },
    { "S016", 0x6900 },
    { "S017", 0x6a00 },
    { "S018", 0x6c00 },
    { "S020", 0x7d00 },
    { "S021", 0x8300 },
    { "S022", 0x8900 },
    { "S023", 0x16179 },
    { "S024", 0x16682 },
    { "S026", 0x3100 },
    { NULL, 0 }
};

static const struct imglist a2listBanzai[] = {
    { "R001", 0x1dc00 },
    { "R002", 0x1d600 },
    { "R003", 0x1cd00 },
    { "R004", 0x1c000 },
    { "R005", 0x1b400 },
    { "R006", 0x1a900 },
    { "R008", 0x1a000 },
    { "R009", 0x19500 },
    { "R010", 0x16f00 },
    { "R011", 0x18c00 },
    { "R012", 0x18700 },
    { "R013", 0x18000 },
    { "R014", 0x17600 },
    { "R015", 0x16f00 },
    { "R016", 0xf300 },
    { "R017", 0x16c00 },
    { "R018", 0x15f00 },
    { "R019", 0x15400 },
    { "R020", 0x14d00 },
    { "R021", 0x14100 },
    { "R022", 0x13900 },
    { "R023", 0x13100 },
    { "R024", 0x12a00 },
    { "R025", 0x11f00 },
    { "R026", 0x11500 },
    { "R028", 0x10d00 },
    { "R029", 0x10600 },
    { "R035", 0xfd00 },
    { "S000", 0x4700 },
    { "B027", 0xf000 },
    { "B004", 0xe600 },
    { "B005", 0xe500 },
    { "B006", 0xdf00 },
    { "B019", 0xda00 },
    { "B007", 0xd700 },
    { "B030", 0xd600 },
    { "B025", 0xd100 },
    { "B026", 0xd000 },
    { "B051", 0xcf00 },
    { "B023", 0xcd00 },
    { "B055", 0xcc00 },
    { "B056", 0xc300 },
    { "B070", 0xb900 },
    { "B071", 0xb400 },
    { "B073", 0xb200 },
    { "B074", 0xb000 },
    { "B037", 0xb100 },
    { "B034", 0xaf00 },
    { "B035", 0xa700 },
    { "S001", 0x1000 },
    { "S002", 0x1900 },
    { "S003", 0x1a00 },
    { "S004", 0x1d00 },
    { "S005", 0x2000 },
    { "S006", 0x2300 },
    { "S007", 0x2600 },
    { "S008", 0x2900 },
    { "S009", 0x2c00 },
    { "S010", 0x2f00 },
    { "S011", 0x3700 },
    { "S012", 0x3a00 },
    { "S013", 0x3e00 },
    { "S014", 0x4100 },
    { "S015", 0x4300 },
    { "S016", 0x4500 },
    { "S017", 0x4600 },
    { NULL, 0, }
};

uint8_t *new = NULL;

int DetectApple2(uint8_t **sf, size_t *extent)
{
    const size_t dsk_image_size = 35 * 16 * 256;
    if (*extent > MAX_LENGTH || *extent < dsk_image_size)
        return 0;

    new = MemAlloc(dsk_image_size);
    size_t offset = dsk_image_size - 256;
    for (int i = 0; i < *extent && i < dsk_image_size; i += 256) {
        memcpy(new + offset, *sf + i, 256);
        offset -= 256;
    }

    int actionsize = new[125490] + (new[125491] << 8);
    debug_print("Actionsize: %d\n", actionsize);
    if (actionsize < 4000 || actionsize > 7000) {
        size_t companionsize;
        uint8_t *companionfile = GetCompanionFile(&companionsize);
        if (companionfile && companionsize >= dsk_image_size) {
            free(new);
            new = MemAlloc(companionsize);
            offset = companionsize - 256;
            for (int i = 0; i < companionsize; i += 256) {
                memcpy(new + offset, companionfile + i, 256);
                offset -= 256;
            }
            actionsize = new[125490] + (new[125491] << 8);
        }
        if (actionsize < 4000 || actionsize > 7000) {
            free(new);
            return 0;
        }
    }

    size_t datasize = dsk_image_size - 125438;
    uint8_t *gamedata = MemAlloc(datasize);
    memcpy(gamedata, new + 125438, datasize);

    if (gamedata) {
        free(*sf);
        *sf = gamedata;
        *extent = datasize;
        CurrentSys = SYS_APPLE2;
        ImageWidth = 280;
        ImageHeight = 152;
        return 1;
    }

    return 0;
}

void LookForApple2Images(void) {
    if (new == NULL)
        return;

    const struct imglist *list = a2listSpidey;

    switch(CurrentGame) {
        case SPIDERMAN:
            list = a2listSpidey;
            break;
        case BANZAI:
            list = a2listBanzai;
            break;
        case FANTASTIC4:
            list = a2listFantastic;
            break;
        default:
            Fatal("Unknown game!");
            break;
    }

    int count = Game->no_of_room_images + Game->no_of_item_images + Game->no_of_special_images;

    Images = MemAlloc((count + 1) * sizeof(struct imgrec));

    int created = 0;

    for (int i = 0; i <= GameHeader.NumRooms; i++) {
        int exists = 0;
        for (int j = 0; j < created; j++) {
            if (Images[j].DiskOffset == Rooms[i].Image) {
                exists = 1;
                break;
            }
        }
        if (exists || Rooms[i].Image == 0)
            continue;

        size_t offset = 0x23000 - Rooms[i].Image - 0x100;
        size_t size = new[offset] + new[offset + 1] * 0x100;

        int ct = 0;
        int found = 0;
        while (list[ct].filename != NULL) {
            if (list[ct].offset == Rooms[i].Image) {
                Images[created].Filename = list[ct].filename;
                Images[created].DiskOffset = list[ct].offset;
                Rooms[i].Image = (list[ct].filename[2] - '0') * 10 + list[ct].filename[3] - '0';
                found = 1;
                break;
            }
            ct++;
        }
        if (found == 0) {
            fprintf(stderr, "Error, room with offset %x not found!\n", Rooms[i].Image);
            continue;
        }

        Images[created].Data = MemAlloc(size);
        memcpy(Images[created].Data, new + offset, size);
        Images[created].Size = size;
        created++;
    }

    for (int i = 0; i <= GameHeader.NumRooms; i++) {
        int ct = 0;
        while (list[ct].filename != NULL) {
            if (list[ct].offset == Rooms[i].Image) {
                Rooms[i].Image = (list[ct].filename[2] - '0') * 10 + list[ct].filename[3] - '0';
                break;
            }
            ct++;
        }
    }

    for (int i = 0; i <= GameHeader.NumObjImg; i++) {
        int exists = 0;
        for (int j = 0; j < created; j++) {
            if (Images[j].DiskOffset == ObjectImages[i].Image) {
                exists = 1;
                break;
            }
        }
        if (exists || ObjectImages[i].Image == 0)
            continue;
        size_t offset = 0x23000 - ObjectImages[i].Image - 0x100;
        if (offset > 0x23000)
            continue;
        size_t size = new[offset] + new[offset + 1] * 0x100 + 2;
        if (offset + size > 0x23000)
            size = 0x23000 - offset;

        int ct = 0;
        int found = 0;
        while (list[ct].filename != NULL) {
            if (list[ct].offset == ObjectImages[i].Image) {
                Images[created].Filename = list[ct].filename;
                ObjectImages[i].Image = (list[ct].filename[2] - '0') * 10 + list[ct].filename[3] - '0';
                found = 1;
                break;
            }
            ct++;
        }
        if (found == 0) {
            fprintf(stderr, "Error, image with offset %x not found!\n", ObjectImages[i].Image);
            continue;
        }

        Images[created].Data = MemAlloc(size);
        memcpy(Images[created].Data, new + offset, size);
        Images[created].Size = size;
        Images[created].DiskOffset = list[ct].offset;
        created++;
        if (created > count) {
            fprintf(stderr, "Error!\n");
            created--;
            break;
        }
    }

    for (int i = 0; i <= GameHeader.NumObjImg; i++) {
        int ct = 0;
        while (list[ct].filename != NULL) {
            if (list[ct].offset == ObjectImages[i].Image) {
                ObjectImages[i].Image = (list[ct].filename[2] - '0') * 10 + list[ct].filename[3] - '0';
                break;
            }
            ct++;
        }
    }

    int ct = 0;
    while (list[ct].filename != NULL) {
        if (list[ct].filename[0] == 'S') {
            Images[created].Filename = list[ct].filename;
            size_t size = new[list[ct].offset] + new[list[ct].offset + 1] * 256;
            Images[created].Size = size;
            Images[created].Data = MemAlloc(size);
            memcpy(Images[created].Data, new + list[ct].offset, size);
            created++;
        }
        ct++;
    }

    Images[created].Filename = NULL;
    free(new);
}

