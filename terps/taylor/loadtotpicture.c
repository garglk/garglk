//
//  loadtotpicture.c
//
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  This code is for de-protecting the ZX Spectrum text-only version of Temple of Terror
//
//  Created by Petter Sjölund on 2022-04-19.
//

#include <stdlib.h>

#include "loadtotpicture.h"
#include "extracttape.h"
#include "utility.h"

static uint8_t *mem;

static uint16_t push(uint16_t reg, uint16_t SP) {
    SP -= 2;
    mem[SP] = reg & 0xff;
    mem[SP + 1] = reg >> 8;
    return SP;
}

static uint16_t pop(uint16_t *SP) {
    uint16_t reg = mem[*SP] + mem[*SP + 1] * 256;
    *SP += 2;
    return reg;
}

static int parity(uint8_t x) {
    int bitsset = 0;
    for (int i = 0; i < 8; i++)
        if (x & (1 << i))
            bitsset++;
    return ((bitsset & 1) == 0);
}

static uint16_t *DrawAdresses = NULL;
static int AddressIndex = 0;

/* The image loader reads a byte from tape, decrypts it and writes the result */
/* to a screen adress pulled from the list we create here */

/* Here we add another screen address to the address list array */
static void addtoarray(uint16_t val) {
    DrawAdresses[AddressIndex++] = val;
}

/* Here we add another attribute address to the address list array */
static void addcolour(uint16_t addr, uint8_t IYh) { // COLOUR_LOAD
    uint8_t msb = addr >> 8;
    if (!parity(IYh))
        msb = msb ^ 0xff; // CPL
    if (msb & 7)
        return;
    msb = addr >> 11;
    msb = (msb & 3) | 0x58; // 0101 1000
    addtoarray((addr & 0xff) + msb * 256);
}

/* Here we get the address in screen memory representing one line down */
/* or up from addr, depending on the parity of the IYh variable */
static uint16_t nextlineaddr(uint16_t addr, uint8_t IYh) {
    uint8_t msb = addr >> 8;
    uint8_t lsb = addr & 0xff;
    // byte down
    if (parity(IYh)) {
        msb++;
        if (msb & 7) // 0000 0111
            return msb * 256 + lsb;
        lsb += 32;
        if ((lsb & 0xe0) == 0) // 1110 0000
            return (msb * 256) + lsb;
        return (msb - 8) * 256 + lsb;
    }
    // byte up
    msb--;
    if ((msb ^ 0xff) & 7) // 0000 0111
        return msb * 256 + lsb;
    lsb -= 32;
    if ((lsb ^ 0xff) & 0xe0) { // 1110 0000
        msb += 8;
    }
    return msb * 256 + lsb;
}

uint16_t globalIY, globalBC, globalDE, globalHL;
uint8_t globalA;

static void registers_on_stack(uint8_t ack, uint16_t IY, uint16_t BC, uint16_t DE, uint16_t HL, uint8_t D) {
    uint16_t SP = D * 11 + 0xee57;
    // D can be 1, 2 or 3, selecting a different "slot" at 0xee62, 0xee6d or 0xee78 respectively
    SP = push(IY, SP);
    SP = push(BC, SP);
    SP = push(DE, SP);
    SP = push(HL, SP);
    uint16_t AF = ack * 256;
    push(AF, SP);
}

/* Here we determine in which direction to go for the next screen byte */
static uint8_t styler(uint8_t ack, uint16_t *IY, uint16_t *HL, uint16_t *DE, uint16_t *BC) { // DIRECTION_OF_LOAD
    uint16_t SP = ack * 11 + 0xee4d;
    // A can be 1, 2 or 3, selecting a different "slot" at 0xee58, 0xee63 or 0xee6e, respectively
    uint16_t AF = pop(&SP);
    uint8_t H, L;
    *HL = pop(&SP);
    *DE = pop(&SP);
    *BC = pop(&SP);
    *IY = pop(&SP);
    if ((AF >> 8) == 0)
        return 0;
    uint8_t IYh = *IY >> 8;

    addcolour(*HL, IYh);
    addtoarray(*HL);
    *BC -= 256;

    if (IYh < 2) {
        if ((*BC >> 8) != 0) {
            *HL = nextlineaddr(*HL, IYh); // find address of next line
            return 0xff;
        }
        H = *DE >> 8;
        L = *BC & 0xff;
        if (!parity(IYh)) {
            L--;
        } else {
            L++;
        }
        *BC = (*IY & 0xff) * 256 + L;
        *HL = H * 256 + L;
        *DE = (*DE & 0xff) - 1;
        if (*DE == 0)
            return 0;
        return 0xff;
    }
    // STYLE34
    H = *HL >> 8;
    L = *HL & 0xff;
    if (!parity(IYh)) {
        L--;
    } else {
        L++;
    }
    if ((*BC >> 8) != 0) {
        *HL = H * 256 + L;
        return 0xff;
    }
    *HL = nextlineaddr(H * 256 + (*BC & 0xff), IYh); // find address of next line
    *BC = (*DE & 0xff) * 256 + (*HL & 0xff);
    *DE -= 256;
    if ((*DE >> 8) == 0)
        return 0;
    return 0xff;
}

static void address_table(void) {
    uint16_t IY = 0x032d;
    uint16_t IX = 0xeeab;
    uint16_t DE2, BC2, HL2;
    uint16_t counter = mem[0xeea9] + mem[0xeeaa] * 256;
    uint8_t ack, B, C;
    do { // next line
        ack = mem[IX + 3] >> 5;
        C = ack + 1;
        for (int i = C; i > 0; i--) {
            HL2 = mem[IX] + mem[IX + 1] * 256;
            ack = mem[IX + 3] & 0x1f; // 0001 1111
            DE2 = (mem[IX + 2] & 0x3f) + ack * 2048; // 0011 1111
            ack = mem[IX + 2] >> 6;
            IY = (IY & 0xff) + ack * 256;
            BC2 = (HL2 & 0xff) + (DE2 & 0xff) * 256;
            if (ack < 2) {
                IY = (DE2 >> 8) + ack * 256;
                BC2 = mem[IX] + (DE2 >> 8) * 256;
                DE2 = (DE2 & 0xff) + mem[IX + 1] * 256;
            }
            ack++;
            registers_on_stack(ack, IY, BC2, DE2, HL2, i);
            IX += 4;
            counter--;
        }

        do { // NXTP1
            B = 0;
            for (int i = C; i > 0; i--) {
                ack = styler(i, &IY, &HL2, &DE2, &BC2);
                registers_on_stack(ack, IY, BC2, DE2, HL2, i);
                B = ack | B;
            }
        } while (B != 0);
    } while (counter != 0);
}

uint8_t *LoadAlkatrazPicture(uint8_t *memimage, uint8_t *file) {
    mem = memimage;
    DrawAdresses = MemAlloc(6912 * sizeof(uint16_t));
    address_table();
    uint8_t loacon = 0xf6;
    DeAlkatraz(file, mem, 0, 0xea7d, 2, &loacon, 0xc1, 0xcb, 1);
    uint16_t fileoffset = 2;
    for (int i = 0; i < 6912; i++)
        DeAlkatraz(file, mem, fileoffset++, DrawAdresses[i], 1, &loacon, 0xc1, 0xcb, 1);
    free(DrawAdresses);
    return mem;
}
