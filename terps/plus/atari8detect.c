//
//  atari8detect.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "graphics.h"
#include "definitions.h"
#include "loaddatabase.h"
#include "companionfile.h"

#include "gameinfo.h"

#include "atari8detect.h"


typedef struct imglist {
    char *filename;
    size_t offset;
} imglist;


static const struct imglist listFantastic[] = {
    { "R001", 0x0297 },
    { "R002", 0x0a8e },
    { "R003", 0x10e48 },
    { "R004", 0xdac },
    { "R005", 0x690e },
    { "R006", 0x70ba },
    { "R007", 0x7687 },
    { "R008", 0x1b8e },
    { "R009", 0x9dd6 },
    { "R010", 0x7d9e },
    { "R011", 0x2333 },
    { "R012", 0x853a },
    { "R013", 0x339e },
    { "R014", 0x8879 },
    { "R015", 0x3c48 },
    { "R016", 0x4533 },
    { "R017", 0x48f9 },
    { "R018", 0x9080 },
    { "R019", 0x966b },
    { "R020", 0x532c },
    { "R021", 0x5fa5 },
    { "R022", 0xaa0e },
    { "B020", 0xc10e },
    { "B002", 0xdd0e },
    { "B003", 0xb800 },
    { "B004", 0xeb87 },
    { "B005", 0xbb97 },
    { "B006", 0xc01e },
    { "B007", 0xe1eb },
    { "B008", 0xd180 },
    { "B009", 0xc3c8 },
    { "B010", 0xdbf2 },
    { "B011", 0xc241 },
    { "B012", 0xc31e },
    { "B014", 0xf380 },
    { "B015", 0xcc72 },
    { "B016", 0xcfa5 },
    { "B017", 0xd500 },
    { "B018", 0xd5e4 },
    { "B019", 0xe2f2 },
    { "B013", 0xe625 },
    { "B021", 0xbfac },
    { "B022", 0xbf80 },
    { "B023", 0xea41 },
    { "S000", 0x10e80 },
    { "S001", 0xfe80 },
    { "S002", 0x1016b },
    { "S003", 0x1032c },
    { "S004", 0x104e4 },
    { "S005", 0x10610 },
    { "S006", 0x10717 },
    { "S007", 0x1083a },
    { "S008", 0x10e48 },
    { "S009", 0x11d07 },
    { "S010", 0x11d48 },
    { "S011", 0x11ec8 },
    { "S012", 0x12000 },
    { "S013", 0x120f2 },
    { "S014", 0x12307 },
    { "S015", 0x12417 },
    { "S016", 0x1252c },
    { "S017", 0x125cf },
    { "S018", 0x12617 },
    { "S019", 0x1263a },
    { "S020", 0x126c1 },
    { NULL, 0, }
};

static const struct imglist listSpidey[] = {
    { "R001", 0x0297 },
    { "R002", 0x078e },
    { "R003", 0x0b64 },
    { "R005", 0x1741 },
    { "R006", 0x1df9 },
    { "R007", 0x2a17 },
    { "R008", 0x34e4 },
    { "R013", 0x3c97 },
    { "R015", 0x463a },
    { "R017", 0x4dd6 },
    { "R018", 0x5633 },
    { "R019", 0x5cac },
    { "R022", 0x6a00 },
    { "R024", 0x7464 },
    { "R025", 0x81ac },
    { "R034", 0x875d },
    { "R037", 0x9280 },
    { "R040", 0xa16b },
    { "B021", 0xadf2 },
    { "B015", 0xb1c8 },
    { "B016", 0xb56b },
    { "B023", 0xb88e },
    { "B025", 0xba07 },
    { "B027", 0xbac8 },
    { "B028", 0xbc87 },
    { "B031", 0xc248 },
    { "B033", 0xc633 },
    { "B034", 0xc9dd },
    { "B035", 0xcdeb },
    { "B038", 0xcf9e },
    { "B051", 0xd91e },
    { "B052", 0xdb97 },
    { "B058", 0xe048 },
    { "B059", 0xe172 },
    { "B060", 0xe4c8 },
    { "B062", 0xe5eb },
    { "B071", 0xeb0e },
    { "B049", 0xeb48 },
    { "B044", 0xee2c },
    { "B053", 0xf090 },
    { "B056", 0xf49e },
    { "B055", 0xf6f9 },
    { "B050", 0xf8dd },
    { "S001", 0xff64 },
    { "S002", 0x10197 },
    { "S003", 0x103ac },
    { "S004", 0x105ac },
    { "S005", 0x10833 },
    { "S007", 0x11141 },
    { "S008", 0x11541 },
    { "S009", 0x11f90 },
    { "S010", 0x12c79 },
    { "S011", 0x13590 },
    { "S012", 0x13e64 },
    { "S013", 0x14348 },
    { "S014", 0x149e4 },
    { "S015", 0x14ddd },
    { "S016", 0x14e97 },
    { "S017", 0x14f3a },
    { "S018", 0x15064 },
    { "S020", 0x151cf },
    { "S021", 0x15687 },
    { "S022", 0x15b64 },
    { "S023", 0x16179 },
//    { "S024", 0x16682 },
    { "S026", 0x16179 },
    { NULL, 0 }
};

//{ "S000", 0, Disk image A at offset 6d97
//    0x0e94 },

static const struct imglist listBanzai[] = {
    { "R001", 0x0297 },
    { "R002", 0x083a },
    { "R003", 0xb072 },
    { "R004", 0x115d },
    { "R005", 0x1c2c },
    { "R006", 0x264f },
    { "R008", 0xb7f2 },
    { "R009", 0x2dc1 },
    { "R011", 0xc2b3 },
    { "R012", 0x351e },
    { "R013", 0x9d56 },
    { "R014", 0x3b6b },
    { "R015", 0x1196b },
    { "R016", 0xc817 },
    { "R017", 0x41cf },
    { "R018", 0x4ce4 },
    { "R019", 0x11bc8 },
    { "R020", 0x583a },
    { "R021", 0x5eb3 },
    { "R022", 0x6556 },
    { "R023", 0x6b07 },
    { "R024", 0x7080 },
    { "R025", 0xa641 },
    { "R026", 0x7941 },
    { "R028", 0x8048 },
    { "R029", 0x856b },
    { "R035", 0x8b33 },
    { "S000", 0x92ac },
    { "B027", 0xca33 },
    { "B004", 0xcf9e },
    { "B005", 0xd017 },
    { "B006", 0xd479 },
    { "B019", 0xd81e },
    { "B007", 0xd9cf },
    { "B030", 0xda5d },
    { "B025", 0xdbc1 },
    { "B026", 0xdc25 },
    { "B051", 0xdcc8 },
    { "B023", 0xddc8 },
    { "B055", 0xdeb3 },
    { "B056", 0xe2b3 },
    { "B070", 0xe7d6 },
    { "B071", 0xe956 },
    { "B073", 0xe9d6 },
    { "B074", 0xea90 },
    { "B037", 0xd479 },
    { "B034", 0xeaf9 },
    { "B035", 0xed5d },
    { "S001", 0xf000 },
    { "S002", 0xf807 },
    { "S003", 0xf85d },
    { "S004", 0xfacf },
    { "S005", 0xfcd6 },
    { "S006", 0xff56 },
    { "S007", 0x10180 },
    { "S008", 0x103cf },
    { "S009", 0x1064f },
    { "S010", 0x1088e },
    { "S011", 0x10f17 },
    { "S012", 0x11141 },
    { "S013", 0x11433 },
    { "S014", 0x11617 },
    { "S015", 0x1176b },
    { "S016", 0x11841 },
    { "S017", 0x118e4 },
    { NULL, 0, }
};


static int ExtractImagesFromAtariCompanionFile(uint8_t *data, size_t datasize, uint8_t *otherdisk, size_t othersize) {
    size_t size;

    const struct imglist *list = listSpidey;
    /* The number of images is 65 in all three Atari 8-bit Saga plus games!? */
    /* (Not counting the title image on disk A in Spider-Man.) */
    int count = 65;
    if (CurrentGame == BANZAI)
        list = listBanzai;
    else if (CurrentGame == FANTASTIC4)
        list = listFantastic;

    Images = MemAlloc((count + 2) * sizeof(struct imgrec));

    int outpic;

    // Now loop round for each image
    for (outpic = 0; outpic < count; outpic++)
    {
        uint8_t *ptr = data + list[outpic].offset;

        size = *ptr++;
        size += *ptr * 256 + 4;

        if (list[outpic].offset >= datasize) {
            fprintf(stderr, "Image %d (%s) offset %zx out of bounds. Size: %zx\n", outpic, list[outpic].filename, list[outpic].offset, datasize);
            continue;
        }
        if (list[outpic].offset + size >= datasize) {
            fprintf(stderr, "Image %d (%s) offset %zx out of bounds. image size: %zx, disk size: %zx\n", outpic, list[outpic].filename, list[outpic].offset, size, datasize);
            size = datasize - list[outpic].offset + 2;
        }

        Images[outpic].Filename = list[outpic].filename;
        Images[outpic].Data = MemAlloc(size);
        Images[outpic].Size = size;
        memcpy(Images[outpic].Data, data + list[outpic].offset - 2, size);
        if (list[outpic].offset < 0xb390 && list[outpic].offset + Images[outpic].Size > 0xb390) {
            memcpy(Images[outpic].Data + 0xb390 - list[outpic].offset + 2, data + 0xb410, size - 0xb390 + list[outpic].offset - 2);
        }

    }

    Images[outpic].Filename = NULL;

    if (CurrentGame == SPIDERMAN && otherdisk) {
        Images[outpic].Filename = "S000";
        Images[outpic].Size = 0x0e96;
        Images[outpic].Data = MemAlloc(Images[outpic].Size);
        memcpy(Images[outpic].Data, otherdisk + 0x6d95, Images[outpic].Size);
        Images[outpic + 1].Filename = NULL;
    }
    return 1;
}


static const uint8_t atrheader[6] = { 0x96 , 0x02, 0x80, 0x16, 0x80, 0x00 };

int DetectAtari8(uint8_t **sf, size_t *extent)
{
    uint8_t *main_disk = *sf;
    size_t main_size = *extent;
    int result = 0;
    size_t data_start = 0xff8e;
    // Header actually starts at offset 0xffc0 (0xff8e + 0x32).
    // We add 50 bytes at the head to match the C64 files.

    if (main_size > MAX_LENGTH || main_size < data_start)
        return 0;

    for (int i = 0; i < 6; i++)
        if ((*sf)[i] != atrheader[i])
            return 0;

    size_t companionsize;
    uint8_t *companionfile = GetCompanionFile(&companionsize);

    ImageWidth = 280;
    ImageHeight = 158;
    mem = main_disk + data_start; memlen = main_size - data_start;
    result = LoadDatabaseBinary();
    if (!result && companionfile != NULL && companionsize > data_start) {
        debug_print("Could not find database in this file, trying the companion file\n");
        mem = companionfile + data_start; memlen = companionsize - data_start;
        result = LoadDatabaseBinary();
        if (result) {
            debug_print("Found database in companion file. Switching files.\n");
            uint8_t *temp = companionfile;
            size_t tempsize = companionsize;
            companionfile = main_disk;
            companionsize = main_size;
            main_disk = temp;
            main_size = tempsize;
        }
    }

    if (result) {
        CurrentSys = SYS_ATARI8;
        if (companionfile) {
            ExtractImagesFromAtariCompanionFile(companionfile, companionsize, main_disk, main_size);
            free(companionfile);
        }
    } else
        debug_print("Failed loading database\n");

    return result;
}

