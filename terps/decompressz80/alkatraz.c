//
//  alkatraz.c
//
//  Part of the decompressz80 library: undoes the Alkatraz copy protection
//  (Appleby Associates) of ZX Spectrum tapes. The loader decrypts the game
//  with a rolling key as it comes off the tape and then moves blocks of it
//  about in memory. These functions do the same to a 64K image of the
//  Spectrum's memory. Which block of which tape to give them, and with which
//  numbers, is for the interpreters to know.
//
//  Created by Petter Sjölund on 2022-04-11.
//

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "decompressz80.h"
#include "decompressz80_internal.h" /* ZX_LOG */

uint8_t *ZXNewMemory(void)
{
    return calloc(1, ZX_MEMORY_SIZE);
}

/* The Z80 does these a byte at a time, so that a block copied onto itself
   one byte along is a fill. The addresses wrap around as the registers do. */
void ZXLdir(uint8_t *mem, uint16_t target, uint16_t source, uint16_t size)
{
    for (int i = 0; i < size; i++)
        mem[target++] = mem[source++];
}

void ZXLddr(uint8_t *mem, uint16_t target, uint16_t source, uint16_t size)
{
    for (int i = 0; i < size; i++)
        mem[target--] = mem[source--];
}

/* 'adjust' goes into the key only while bit 4 of the low byte of the count of
   bytes left is set, which is sixteen bytes in every thirty-two */
#define ADJUST_BIT 0x10

int DeAlkatraz(const uint8_t *src, size_t src_length, size_t src_offset,
    uint8_t *mem, uint16_t target, uint16_t count, uint8_t *key,
    const AlkatrazKey *schedule)
{
    if (src == NULL || mem == NULL || src_offset > src_length
        || count > src_length - src_offset) {
        ZX_LOG("Alkatraz block is too short");
        return 0;
    }

    uint8_t adjust = schedule->adjust, step = schedule->step;

    while (count != 0) {
        /* The loader has the count in DE and the address in HL */
        uint8_t count_hi = count >> 8, count_lo = count & 0xff;
        uint8_t decrypted = *key ^ count_hi ^ count_lo
            ^ (target >> 8) ^ (target & 0xff) ^ src[src_offset++];
        mem[target] = decrypted;

        /* A loader that decrypts itself gets to its own operands */
        if (schedule->adjust_address != 0 && target == schedule->adjust_address)
            adjust = decrypted;
        if (schedule->step_address != 0 && target == schedule->step_address)
            step = decrypted;

        if (count_lo & ADJUST_BIT)
            *key = *key + adjust - count_hi + count_lo;
        *key += step;
        target++;
        count--;
    }

    return 1;
}

/* The word at an address, which may be the last one */
static uint16_t Word(const uint8_t *mem, uint16_t address)
{
    return (uint16_t)(mem[address] | mem[(uint16_t)(address + 1)] << 8);
}

void DeshuffleAlkatraz(uint8_t *mem, uint8_t repeats, uint16_t ix, uint16_t store)
{
    for (int i = 0; i < repeats; i++) {
        uint16_t gap = Word(mem, ix - 4);
        uint16_t length = Word(mem, ix - 2) + 1;
        /* Move everything from the gap up out of the way, then fill the gap */
        store += length;
        ZXLddr(mem, store, store - length, store - gap + 1);
        mem[gap] = mem[ix];
        ZXLdir(mem, gap + 1, gap, length - 1);
        ix -= 5;
    }
}
