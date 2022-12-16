//
//  saga.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-29.
//

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "scott.h"
#include "detectgame.h"
#include "sagagraphics.h"
#include "pcdraw.h"
#include "atari8c64draw.h"
#include "apple2draw.h"

#include "saga.h"


static const char *DOSFilenames[] =
  { "B01024R", "B01044I", "R0109",   "R0190",
    "B01001I", "B01025R", "B01045R", "R0112",  "R0191",
    "B01002I", "B01026R", "B01046I", "R0115",  "R0192",
    "B01003I", "B01029I", "B01048I", "R0116",  "R0193",
    "B01004I", "B01030I", "B01050I", "R0119",  "R0194",
    "B01012I", "B01031I", "B01051I", "R0120",  "R0195",
    "B01013R", "B01032I", "B01053R", "R0181",  "R0196",
    "B01016I", "B01033R", "B01070R", "R0182",  "R0197",
    "B01017R", "B01034R", "B01072R", "R0183",  "R0198",
    "B01018I", "B01035I", "R0184",   "R0199",  "B01019I",
    "B01036R", "R0100",   "R0185",   "B01020R","B01037I",
    "R0101",   "R0186",   "B01021I", "B01039R","R0102",
    "R0187",   "B01022R", "B01040R", "R0103",  "R0188",
    "B01023I", "B01042I", "R0104",   "R0189",  NULL };

int LoadDOSImages(void) {
    USImages = new_image();

    struct USImage *image = USImages;
    size_t datasize;
    int found = 0;

    for (int i = 0; DOSFilenames[i] != NULL; i++) {
        const char *shortname = DOSFilenames[i];
        uint8_t *data = FindImageFile(shortname, &datasize);

        if (data) {
            found++;
            image->datasize = datasize;
            image->imagedata = MemAlloc(datasize);
            memcpy(image->imagedata, data, datasize);
            free(data);
            image->systype = SYS_MSDOS;
            if (shortname[0] == 'R') {
                image->usage = IMG_ROOM;
                image->index = shortname[4] - '0' + (shortname[3] - '0') * 10;
            } else {
                image->index = shortname[5] - '0' + (shortname[4] - '0') * 10;
                if (shortname[6] == 'R') {
                    image->usage = IMG_ROOM_OBJ;
                } else {
                    image->usage = IMG_INV_OBJ;
                }
            }
            image->next = new_image();
            image = image->next;
        }
    }
    if (!found) {
        free(USImages);
        USImages = NULL;
        return 0;
    }
    return 1;
}

uint8_t *ReadUSDictionary(uint8_t *ptr)
{
    char dictword[GameHeader.WordLength + 2];
    char c = 0;
    int wordnum = 0;
    int charindex = 0;

    int nn = GameHeader.NumWords + 1;

    do {
        for (int i = 0; i < GameHeader.WordLength; i++) {
            c = *(ptr++);
            if (c == 0) {
                if (charindex == 0) {
                    c = *(ptr++);
                }
            }
            dictword[charindex] = c;
            if (c == '*')
                i--;
            charindex++;

            dictword[charindex] = 0;
        }

        if (wordnum < nn) {
            Nouns[wordnum] = MemAlloc(charindex + 1);
            memcpy((char *)Nouns[wordnum], dictword, charindex + 1);
            debug_print("Nouns %d: \"%s\"\n", wordnum,
                    Nouns[wordnum]);
        } else {
            Verbs[wordnum - nn] = MemAlloc(charindex + 1);
            memcpy((char *)Verbs[wordnum - nn], dictword, charindex + 1);
            debug_print("Verbs %d: \"%s\"\n", wordnum - nn,
                    Verbs[wordnum - nn]);
        }
        wordnum++;

        if (c != 0 && c > 127)
            return ptr;

        charindex = 0;
    } while (wordnum <= GameHeader.NumWords * 2 + 1);

    return ptr;
}

int DrawUSImage(USImage *image) {
    last_image_index = image->index;
    if (image->systype == SYS_MSDOS)
        return DrawDOSImage(image);
    else if (image->systype == SYS_C64 || image->systype == SYS_ATARI8)
        return DrawAtariC64Image(image);
    else if (image->systype == SYS_APPLE2)
        return DrawApple2Image(image);
    return 0;
}

void DrawInventoryImages(void) {
    struct USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_INV_OBJ && Items[image->index].Location == CARRIED) {
                DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
}

void DrawRoomObjectImages(void) {
    struct USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM_OBJ && image->index <= GameHeader.NumItems && Items[image->index].Location == MyLoc) {
                DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
}

int DrawUSRoom(int room) {
    struct USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM && image->index == room) {
                return DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
    return 0;
}

void DrawUSRoomObject(int item) {
    struct USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM_OBJ && image->index == item) {
                DrawUSImage(image);
                return;
            }
            image = image->next;
        } while (image != NULL);
    }
}

void LookUS(void)
{
    if (!Graphics)
        return;

    glk_window_clear(Graphics);
    glk_window_clear(Top);

    int room = MyLoc;

    if (CurrentGame == HULK_US && USImages && USImages->systype != SYS_APPLE2)
        switch (MyLoc) {
            case 5: // Tunnel going outside
            case 6:
                room = 3;
                break;
            case 7: // Field
            case 8:
                room = 4;
                break;
            case 10: // Hole
            case 11:
                room = 9;
                break;
            case 13: // Dome
            case 14:
                room = 2;
                break;
            case 17: // warp
            case 18:
                room = 16;
                break;
            default:
                break;
        }

    if (!DrawUSRoom(room)) {
        return;
    }
    DrawRoomObjectImages();

    if (CurrentGame == HULK_US) {
        if (Items[18].Location == MyLoc && MyLoc == Items[18].InitialLoc) // Bio Gem
            DrawUSRoomObject(70);
        if (Items[21].Location == MyLoc && MyLoc == Items[21].InitialLoc) // Wax
            DrawUSRoomObject(72);
        if (Items[14].Location == MyLoc || Items[15].Location == MyLoc) // Large pit
            DrawUSRoomObject(13);
    } else if (CurrentGame == COUNT_US) {
        if (Items[17].Location == MyLoc && MyLoc == 8) // Only draw mirror in bathroom
            DrawUSRoomObject(80);
        if (Items[35].Location == MyLoc && MyLoc == 18) // Only draw other end of sheet in pit
            DrawUSRoomObject(81);
        if (Items[5].Location == MyLoc && MyLoc == 9) // Only draw coat-of-arms at gate
            DrawUSRoomObject(82);
    } else if (CurrentGame == VOODOO_CASTLE_US) {
        if (Items[45].Location == MyLoc && MyLoc == 14) // Only draw boards in chimney
            DrawUSRoomObject(80);
    }
    if (CurrentSys == SYS_APPLE2)
        DrawApple2ImageFromVideoMem();
}

void InventoryUS(void)
{
    if (!Graphics || !has_graphics())
        return;
    glk_window_clear(Graphics);
    DrawUSRoom(98);
    DrawInventoryImages();
    if (CurrentSys == SYS_APPLE2)
        DrawApple2ImageFromVideoMem();
    if (!showing_inventory) {
        showing_inventory = 1;
        Output(sys[HIT_ENTER]);
        HitEnter();
    }
}

extern size_t hulk_coordinates;
extern size_t hulk_item_image_offsets;
extern size_t hulk_look_image_offsets;
extern size_t hulk_special_image_offsets;
extern size_t hulk_image_offset;

static int SanityCheckScottFreeHeader(int ni, int na, int nw, int nr, int mc)
{
    int16_t v = header[1]; // items
    if (v < 10 || v > 500)
        return 0;
    v = header[2]; // actions
    if (v < 100 || v > 500)
        return 0;
    v = header[3]; // word pairs
    if (v < 50 || v > 200)
        return 0;
    v = header[4]; // Number of rooms
    if (v < 10 || v > 100)
        return 0;
    v = header[5]; // Number of Messages
    if (v < 10 || v > 255)
        return 0;

    return 1;
}

uint8_t *Skip(uint8_t *ptr, int count, uint8_t *eof) {
    for (int i = 0; i < count && ptr + i + 1 < eof; i += 2) {
        uint16_t val =  ptr[i] + ptr[i+1] * 0x100;
        debug_print("Unknown value %d: %d (%x)\n", i/2, val, val);
    }
    return  ptr + count;
}

int LoadBinaryDatabase(uint8_t *data, size_t length, struct GameInfo info, int dict_start)
{
    int ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm;
    int ct;

    Action *ap;
    Room *rp;
    Item *ip;

    /* Load the header */
    uint8_t *ptr = data;

    size_t offset;

    if (dict_start) {
        file_baseline_offset = dict_start - info.start_of_dictionary - 645;
        debug_print("LoadBinaryDatabase: file baseline offset:%d\n",
                file_baseline_offset);
        offset = info.start_of_header + file_baseline_offset;
        ptr = SeekToPos(data, offset);
    } else {
        int version = 0;
        int adventure_number = 0;
        for (int i = 0; i < 0x38; i += 2) {
            int value = ptr[i] + ptr[i + 1] * 0x100;
            if (value < 500) {
                if (version == 0)
                    version = value;
                else {
                    adventure_number = value;
                    break;
                }
            }
        }
        debug_print("Version: %d\n", version);
        debug_print("Adventure number: %d\n", adventure_number);
        if (adventure_number == 0)
            return 0;
        if (version == 127 && adventure_number == 1)
            CurrentGame = HULK_US;
        else if (version == 126) {
            if (adventure_number == 2)
                CurrentGame = HULK_US_PREL;
            else if (adventure_number == 13)
                CurrentGame = CLAYMORGUE_US_126;
        } else
            CurrentGame = adventure_number;
        Game->type = US_VARIANT;
        ptr = Skip(ptr, 0x38, data + length);
    }

    if (ptr == NULL)
        return 0;

    ptr = ReadHeader(ptr);

    ParseHeader(header, US_HEADER, &ni, &na, &nw, &nr, &mc, &pr, &tr,
                &wl, &lt, &mn, &trm);

    PrintHeaderInfo(header, ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm);

    if (!SanityCheckScottFreeHeader(ni, na, nw, nr, mc))
        return 0;

    GameHeader.NumItems = ni;
    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    GameHeader.NumActions = na;
    Actions = (Action *)MemAlloc(sizeof(Action) * (na + 1));
    GameHeader.NumWords = nw;
    GameHeader.WordLength = wl;
    Verbs = MemAlloc(sizeof(char *) * (nw + 2));
    Nouns = MemAlloc(sizeof(char *) * (nw + 2));
    GameHeader.NumRooms = nr;
    Rooms = (Room *)MemAlloc(sizeof(Room) * (nr + 1));
    GameHeader.MaxCarry = mc;
    GameHeader.PlayerRoom = pr;
    GameHeader.Treasures = tr;
    GameHeader.LightTime = lt;
    LightRefill = lt;
    GameHeader.NumMessages = mn;
    Messages = MemAlloc(sizeof(char *) * (mn + 1));
    GameHeader.TreasureRoom = trm;

    /* if dict_start > 0, we are checking for the UK version of Questprobe featuring The Hulk */
    if (dict_start) {
        if (header[0] != info.word_length || header[1] != info.number_of_words || header[2] != info.number_of_actions || header[3] != info.number_of_items || header[4] != info.number_of_messages || header[5] != info.number_of_rooms || header[6] != info.max_carried) {
            //    debug_print("Non-matching header\n");
            return 0;
        }
    }

#pragma mark Dictionary

    while (!(*ptr == 'A' && *(ptr + 1) == 'N' && *(ptr + 2) == 'Y') && ptr - data < length - 2)
        ptr++;

    if (ptr - data >= length - 2)
        return 0;

    ptr = ReadUSDictionary(ptr);

#pragma mark Rooms

    ct = 0;
    rp = Rooms;

    uint8_t string_length = 0;
    do {
        string_length = *(ptr++);
        if (string_length == 0) {
            rp->Text = ".\0";
        } else {
            rp->Text = MemAlloc(string_length + 1);
            for (int i = 0; i < string_length; i++) {
                rp->Text[i] = *(ptr++);
            }
            rp->Text[string_length] = 0;
        }
        debug_print("Room %d: \"%s\"\n", ct, rp->Text);
        rp++;
        ct++;
    } while (ct < nr + 1);

#pragma mark Messages

    ct = 0;
    char *string;

    do {
        string_length = *(ptr++);
        if (string_length == 0) {
            string = ".\0";
        } else {
            string = MemAlloc(string_length + 1);
            for (int i = 0; i < string_length; i++) {
                string[i] = *(ptr++);
            }
            string[string_length] = 0;
        }
        Messages[ct] = string;
        debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
        ct++;
    } while (ct < mn + 1);

#pragma mark Items

    ct = 0;
    ip = Items;

    do {
        string_length = *(ptr++);
        if (string_length == 0) {
            ip->Text = ".\0";
            ip->AutoGet = NULL;
        } else {
            ip->Text = MemAlloc(string_length + 1);

            for (int i = 0; i < string_length; i++) {
                ip->Text[i] = *(ptr++);
            }
            ip->Text[string_length] = 0;
            ip->AutoGet = strchr(ip->Text, '/');
            /* Some games use // to mean no auto get/drop word! */
            if (ip->AutoGet && strcmp(ip->AutoGet, "//") && strcmp(ip->AutoGet, "/*")) {
                char *t;
                *ip->AutoGet++ = 0;
                t = strchr(ip->AutoGet, '/');
                if (t != NULL)
                    *t = 0;
                ptr++;
            }
        }

        debug_print("Item %d: %s\n", ct, ip->Text);
        if (ip->AutoGet)
            debug_print("Autoget:%s\n", ip->AutoGet);

        ct++;
        ip++;
    } while (ct <= ni);

#pragma mark item locations

    ptr++;
    ct = 0;
    ip = Items;
    while (ct < ni + 1) {
        ip->Location = *ptr;
        ip->InitialLoc = ip->Location;
        debug_print("Item %d (%s) start location: %d\n", ct, Items[ct].Text, ip->Location);
        ptr += 2;
        ip++;
        ct++;
    }

    // Image strings lookup table
    ptr = Skip(ptr, (ni + 1) * 4,  data + length);

#pragma mark Actions

    ct = 0;
    ap = Actions;

    offset = ptr - data;
    size_t offset2;

    int verb, noun, value, value2, plus;
    while (ct <= na) {
        plus = na + 1;
        verb = data[offset + ct];
        noun = data[offset + ct + plus];

        ap->Vocab = verb * 150 + noun;

        for (int j = 0; j < 2; j++) {
            plus += na + 1;
            value = data[offset + ct + plus];
            plus += na + 1;
            value2 = data[offset + ct + plus];
            ap->Subcommand[j] = 150 * value + value2;
        }

        offset2 = offset + 6 * (na + 1);
        plus = 0;

        for (int j = 0; j < 5; j++) {
            value = data[offset2 + ct * 2 + plus];
            value2 = data[offset2 + ct * 2 + plus + 1];
            ptr = &data[offset2 + ct * 2 + plus + 2];
            ap->Condition[j] = value + value2 * 0x100;
            plus += (na + 1) * 2;
        }

        ap++;
        ct++;
    }

    // Room descriptions lookup table
    ptr = Skip(ptr, (nr + 1) * 2, data + length);

#pragma mark Room connections
    /* The room connections are ordered by direction, not room, so all the North
     * connections for all the rooms come first, then the South connections, and
     * so on. */
    for (int j = 0; j < 6; j++) {
        ct = 0;
        rp = Rooms;

        while (ct < nr + 1) {
            rp->Image = 255;
            rp->Exits[j] = *ptr;
            debug_print("Room %d (%s) exit %d (%s): %d\n", ct, Rooms[ct].Text, j, Nouns[j + 1], rp->Exits[j]);
            ptr += 2;
            ct++;
            rp++;
        }
    }

    // Return if not reading UK Hulk
    if (!dict_start) {
        ptr = Skip(ptr, 0xe4, data + length);
        return 1;
    }

#pragma mark room images

    if (SeekIfNeeded(info.start_of_room_image_list, &offset, &ptr) == 0)
        return 0;

    rp = Rooms;

    for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
        rp->Image = *(ptr++);
        //        debug_print("Room %d (%s) has image %d\n", ct, rp->Text,
        //        rp->Image );
        rp++;
    }

#pragma mark item images

    if (SeekIfNeeded(info.start_of_item_image_list, &offset, &ptr) == 0)
        return 0;

    ip = Items;

    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        ip->Image = 255;
        ip++;
    }

    int index, image = 10;

    do {
        index = *ptr++;
        if (index != 255)
            Items[index].Image = image++;
    } while (index != 255);

    if (CurrentGame == HULK_C64) {
        hulk_coordinates = 0x22cd;
        hulk_item_image_offsets = 0x2731;
        hulk_look_image_offsets = 0x2761;
        hulk_special_image_offsets = 0x2781;
        hulk_image_offset = -0x7ff;
    }

    return 1;
}


uint8_t *ReadFileIfExists(const char *name, size_t *size)
{
    FILE *fptr = fopen(name, "r");

    if (fptr == NULL)
        return NULL;

    *size = GetFileLength(fptr);
    if (*size < 1)
        return NULL;

    uint8_t *result = MemAlloc(*size);
    fseek(fptr, 0, SEEK_SET);
    int bytesread = fread(result, 1, *size, fptr);

    fclose(fptr);

    if (bytesread == 0) {
        free(result);
        return NULL;
    }

    return result;
}

int CompareFilenames(const char *str1, size_t length1, const char *str2, size_t length2) {
    while (length1 > 0 && str1[length1] != '.') {
        length1--;
    }
    while (length2 > 0 && str2[length2] != '.') {
        length2--;
    }
    size_t length = MIN(length1, length2);
    if (length <= 0)
        return 0;
    for (int i = length; i > 0; i--) {
        if (str1[length1--] != str2[length2--]) {
            return 0;
        }
    }
    return 1;
}

const char *AddGameFileExtension(const char *filename, size_t gamefilelen, size_t *stringlength) {
    char *new = NULL;
    size_t extpos = gamefilelen;
    while (extpos && game_file[extpos] != '.')
        extpos--;
    size_t extensionlength = gamefilelen - extpos;
    char *extension = MemAlloc((int)extensionlength);
    memcpy(extension, &game_file[extpos + 1], extensionlength);
    extpos = *stringlength;
    while (extpos && filename[extpos] != '.')
        extpos--;
    *stringlength = extpos + extensionlength;
    new = MemAlloc((int)*stringlength + 1);
    memcpy(new, filename, extpos + 1);
    memcpy(new + extpos + 1, extension, extensionlength);
    new[*stringlength] = 0;
    return new;
}

const char *LookForCompanionFilenameInDatabase(const pairrec list[][2], size_t stringlen, size_t *stringlength2) {

    for (int i = 0; list[i][0].filename != NULL; i++) {
        *stringlength2 = list[i][0].stringlength;
        if (*stringlength2 == 0) {
            *stringlength2 = strlen(list[i][0].filename);
            debug_print("length of string companionlist[%d][0] (%s): %zu\n", i, list[i][0].filename, *stringlength2);
        }
        if (CompareFilenames(game_file, stringlen, list[i][0].filename, *stringlength2) == 1) {
            *stringlength2 = list[i][1].stringlength;
            if (*stringlength2 == 0)
                *stringlength2 = strlen(list[i][1].filename);
            return AddGameFileExtension(list[i][1].filename, stringlen, stringlength2);
        }

        *stringlength2 = list[i][1].stringlength;
        if (*stringlength2 == 0) {
            *stringlength2 = strlen(list[i][1].filename);
            debug_print("length of string companionlist[%d][1] (%s): %zu\n", i, list[i][1].filename, *stringlength2);
        }
        if (CompareFilenames(game_file, stringlen, list[i][1].filename, *stringlength2) == 1) {
            *stringlength2 = list[i][0].stringlength;
            if (*stringlength2 == 0)
                *stringlength2 = strlen(list[i][0].filename);
            return AddGameFileExtension(list[i][0].filename, stringlen, stringlength2);
        }
    }

    return NULL;
}

char *LookInDatabase(const pairrec list[][2], size_t stringlen) {
    size_t resultlen;
    const char *foundname = LookForCompanionFilenameInDatabase(list, stringlen, &resultlen);
    if (foundname != NULL) {
        while (stringlen > 0 && game_file[stringlen] != '/' && game_file[stringlen] != '\\')
            stringlen--;
        stringlen++;
        size_t newlen = stringlen + resultlen;
        char *path = MemAlloc(newlen + 1);
        memcpy(path, game_file, stringlen);
        memcpy(path + stringlen, foundname, resultlen);
        path[newlen] = '\0';
        return path;
    }

    return NULL;
}
