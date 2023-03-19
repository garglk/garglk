//
//  extracttape.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-11.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decompressz80.h"
#include "decrypttotloader.h"
#include "taylor.h"
#include "utility.h"

#include "extracttape.h"

void ldir(uint8_t *mem, uint16_t DE, uint16_t HL, uint16_t BC)
{
    for (int i = 0; i < BC; i++)
        mem[DE++] = mem[HL++];
}

void lddr(uint8_t *mem, uint16_t DE, uint16_t HL, uint16_t BC)
{
    for (int i = 0; i < BC; i++)
        mem[DE--] = mem[HL--];
}

void DeshuffleAlkatraz(uint8_t *mem, uint8_t repeats, uint16_t IX, uint16_t store)
{
    uint16_t count, length;
    for (int i = 0; i < repeats; i++) {
        length = mem[IX - 2] + mem[IX - 1] * 256 + 1;
        count = mem[IX - 4] + mem[IX - 3] * 256;
        store += length;
        lddr(mem, store, store - length, store - count + 1);
        mem[count] = mem[IX];
        ldir(mem, count + 1, count, length - 1);
        IX -= 5;
    }
}

uint8_t *DeAlkatraz(uint8_t *raw_data, uint8_t *target, size_t offset, uint16_t IX, uint16_t DE, uint8_t *loacon, uint8_t add1, uint8_t add2, int selfmodify)
{
    if (target == NULL) {
        target = malloc(0x10000);
        if (!target)
            return NULL;
        memset(target, 0, 0x10000);
    }

    for (size_t i = offset; DE != 0; i++) {
        uint8_t val = *loacon;
        uint8_t D = DE >> 8;
        val = val ^ D;
        uint8_t E = DE & 0xff;
        val = val ^ E;
        val = val ^ (IX >> 8);
        val = val ^ (IX & 0xff);
        val = val ^ raw_data[i];
        target[IX] = val;
        if (selfmodify) {
            if (IX == 0xe022) {
                add1 = val;
            }
            if (IX == 0xe02f) {
                add2 = val;
            }
        }
        if (E & 0x10) {
            *loacon = *loacon + add1 + E - D;
        }
        *loacon += add2;
        IX++;
        DE--;
    }

    return target;
}

static uint8_t *ShrinkToSnaSize(uint8_t *uncompressed, uint8_t *image, size_t *length)
{
    if (uncompressed == NULL)
        return NULL;
    uint8_t *uncompressed2 = MemAlloc(0xc01b);
    memcpy(uncompressed2, uncompressed + 0x3fe5, 0xc01b);
    if (image != NULL) {
        free(image);
        image = NULL;
    }
    if (uncompressed != NULL) {
        free(uncompressed);
        uncompressed = NULL;
    }
    *length = 0xc01b;
    return uncompressed2;
}

uint8_t *AddTapBlock(uint8_t *image, uint8_t *uncompressed, size_t origlen, int blocknum, size_t offset)
{
    size_t length = origlen;
    uint8_t *block = find_tap_block(blocknum, image, &length);
    memcpy(uncompressed + offset, block, length);
    return uncompressed;
}

uint8_t *AddTZXBlock(uint8_t *image, uint8_t *uncompressed, size_t origlen, int blocknum, size_t offset)
{
    size_t length = origlen;
    uint8_t *block = GetTZXBlock(blocknum, image, &length);
    memcpy(uncompressed + offset, block + 1, length - 2);
    return uncompressed;
}

uint8_t *ProcessFile(uint8_t *image, size_t *length)
{

    int IsBrokenKayleth = 0;

    if (*length == 0xb4bb) {
        fprintf(stderr, "This is Kayleth z80\n");
        IsBrokenKayleth = 1;
    }

    size_t origlen = *length;

    uint8_t *uncompressed = DecompressZ80(image, length);
    if (uncompressed != NULL) {
        free(image);
        image = uncompressed;
    } else {
        uint8_t *block;
        switch (*length) {
        case 0xccca:
            fprintf(stderr, "This is the Temple of Terror side A tzx\n");
            block = GetTZXBlock(4, image, length);
            if (block) {
                uint8_t loacon = 0x9a;
                uncompressed = DeAlkatraz(block, NULL, 0x1b0c, 0x5B00, 0x9f64, &loacon, 0xcf, 0xcd, 0);
                lddr(uncompressed, 0xffff, 0xfc85, 0x9f8e);
                DeshuffleAlkatraz(uncompressed, 05, 0x5b8b, 0xfdd6);
                free(block);
                image = ShrinkToSnaSize(uncompressed, image, length);
            } else {
                fprintf(stderr, "Could not extract block\n");
                return image;
            }
            break;
        case 0xa000:
            fprintf(stderr, "This is the Temple of Terror side B tzx\n");
            image = DecryptToTSideB(image, length);
            image = ShrinkToSnaSize(image, uncompressed, length);
            break;
        case 0xcadc:
            fprintf(stderr, "This is Kayleth tzx\n");
            block = GetTZXBlock(4, image, length);
            if (block) {
                uint8_t loacon = 0xce;
                uncompressed = DeAlkatraz(block, NULL, 0x172e, 0x5B00, 0xa004, &loacon, 0xeb, 0x8f, 0);
                lddr(uncompressed, 0xfd93, 0xfb02, 0x9e38);
                DeshuffleAlkatraz(uncompressed, 0x17, 0x5bf2, 0xfd90);
                free(block);
                image = ShrinkToSnaSize(uncompressed, image, length);
            } else {
                fprintf(stderr, "Could not extract block\n");
                return image;
            }
            break;
        case 0xcd17:
        case 0xcd15:
            fprintf(stderr, "This is Terraquake tzx\n");
            block = GetTZXBlock(3, image, length);
            if (block) {
                uint8_t loacon = 0xe7;
                uncompressed = DeAlkatraz(block, NULL, 0x17eb, 0x5B00, 0x9f64, &loacon, 0x75, 0x55, 0);
                lddr(uncompressed, 0xfc80, 0xfa32, 0x9d38);
                DeshuffleAlkatraz(uncompressed, 0x03, 0x5b90, 0xfc80);
                free(block);
                image = ShrinkToSnaSize(uncompressed, image, length);
            } else {
                fprintf(stderr, "Could not extract block\n");
                return image;
            }
            break;
        case 0x104c4: // Blizzard Pass TAP
            uncompressed = MemAlloc(0x2001f);
            uncompressed = AddTapBlock(image, uncompressed, origlen, 11, 0x1801f);
            uncompressed = AddTapBlock(image, uncompressed, origlen, 15, 0x801b);
            uncompressed = AddTapBlock(image, uncompressed, origlen, 17, 0x401b);
            uncompressed = AddTapBlock(image, uncompressed, origlen, 19, 0x1ee6);
            uncompressed = AddTapBlock(image, uncompressed, origlen, 21, 0xbff7);
            *length = 0x2001f;
            free(image);
            return uncompressed;
        case 0x10428: // Blizzard Pass tzx
            uncompressed = MemAlloc(0x2001f);
            uncompressed = AddTZXBlock(image, uncompressed, origlen, 8, 0x1801f);
            uncompressed = AddTZXBlock(image, uncompressed, origlen, 12, 0x801b);
            uncompressed = AddTZXBlock(image, uncompressed, origlen, 14, 0x401b);
            uncompressed = AddTZXBlock(image, uncompressed, origlen, 16, 0x1ee6);
            uncompressed = AddTZXBlock(image, uncompressed, origlen, 18, 0xbff7);
            *length = 0x2001f;
            free(image);
            return uncompressed;
        default:
            break;
        }
    }

    if (IsBrokenKayleth) {
        uint8_t *mem = MemAlloc(0x10000);
        memcpy(mem + 0x4000, image, 0xc000);
        lddr(mem, 0xf906, 0xf8fe, 0x1ed5);
        mem[0xda2a] = 0;
        ldir(mem, 0xda2b, 0xda2a, 7);
        DeshuffleAlkatraz(mem, 0x13, 0x5bde, 0xfdcf);
        free(image);
        image = mem;
        *length = 0x10000;
    }
    return image;
}
