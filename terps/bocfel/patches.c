// Copyright 2017-2021 Chris Spiegel.
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

#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "patches.h"
#include "memory.h"
#include "zterp.h"

struct replacement {
    uint32_t addr;
    size_t n;
    uint8_t *in;
    uint8_t *out;
};

struct patch
{
    char *title;
    char serial[sizeof header.serial];
    uint16_t release;
    uint16_t checksum;
    // This has to be at least one larger than the patch with the most
    // replacements: when iterating, the code checks for a zero address
    // as a sentinel, knowing that uninitialized entries will be zeroed.
    // This allows code to avoid manually adding a sentienel for each
    // patch, but it does mean that if a patch fills this completely,
    // there will be no sentinel, causing a read beyond the end of the
    // array.
    struct replacement replacements[32];
};

#define B(...)	(uint8_t[]){__VA_ARGS__}

static const struct patch patches[] =
{
    // There are several patches for Beyond Zork.
    //
    // In four places it tries to treat a dictionary word as an object.
    // The offending code looks like:
    //
    // <REPLACE-SYN? ,W?CIRCLET ,W?FILM ,W?ZZZP>
    // <REPLACE-ADJ? ,W?CIRCLET ,W?SWIRLING ,W?ZZZP>
    //
    // Here “,W?CIRCLET” is the dictionary word "circlet"; what was
    // intended is “,CIRCLET”, the object. Given that the object number
    // of the circlet is easily discoverable, these calls can be
    // rewritten to use the object number instead of the incorrect
    // dictionary entry.
    //
    // Beyond Zork also uses platform-specific hacks to display
    // character graphics.
    //
    // On IBM PC, if the interpreter clears the pictures bit,
    // Beyond Zork assumes code page 437 is active, and it uses arrows
    // and box-drawing characters which are invalid or gibberish in
    // ZSCII.
    //
    // On Apple IIc, in much the same way, Beyond Zork prints out values
    // which are valid MouseText, but are gibberish in ZSCII.
    //
    // To work around this, replace invalid characters with valid
    // characters. Beyond Zork uses font 1 with IBM and font 3 with
    // Apple, so while the IBM patches have to use a crude conversion
    // (e.g. “U” and “D” for up and down arrows, and “+” for all
    // box-drawing corners), the Apple patches convert to appropriate
    // font 3 values.
    {
        .title = "Beyond Zork", .serial = "870915", .release = 47, .checksum = 0x3ff4,
        .replacements = {
            // Circlet object
            { .addr = 0x2f8e6, .n = 2, .in = B(0xa3, 0x9a), .out = B(0x01, 0x86), },
            { .addr = 0x2f8f0, .n = 2, .in = B(0xa3, 0x9a), .out = B(0x01, 0x86), },
            { .addr = 0x2f902, .n = 2, .in = B(0xa3, 0x9a), .out = B(0x01, 0x86), },
            { .addr = 0x2f90c, .n = 2, .in = B(0xa3, 0x9a), .out = B(0x01, 0x86), },

            // IBM characters
            { .addr = 0xe58f, .n = 1, .in = B(0xda), .out = B(0x2b) },
            { .addr = 0xe595, .n = 1, .in = B(0xc4), .out = B(0x2d) },
            { .addr = 0xe59d, .n = 1, .in = B(0xbf), .out = B(0x2b) },
            { .addr = 0xe5a6, .n = 1, .in = B(0xc0), .out = B(0x2b) },
            { .addr = 0xe5ac, .n = 1, .in = B(0xc4), .out = B(0x2d) },
            { .addr = 0xe5b4, .n = 1, .in = B(0xd9), .out = B(0x2b) },
            { .addr = 0xe5c0, .n = 1, .in = B(0xb3), .out = B(0x7c) },
            { .addr = 0xe5c9, .n = 1, .in = B(0xb3), .out = B(0x7c) },
            { .addr = 0x3b211, .n = 1, .in = B(0x18), .out = B(0x55) },
            { .addr = 0x3b244, .n = 1, .in = B(0x19), .out = B(0x44) },
            { .addr = 0x3b340, .n = 1, .in = B(0x1b), .out = B(0x4c) },
            { .addr = 0x3b37f, .n = 1, .in = B(0x1a), .out = B(0x52) },

            // Apple IIc characters
            { .addr = 0xe5ee, .n = 1, .in = B(0x4c), .out = B(0x4b) },
            { .addr = 0xe5fd, .n = 1, .in = B(0x5a), .out = B(0x4e) },
            { .addr = 0xe604, .n = 1, .in = B(0x5f), .out = B(0x4d) },
            { .addr = 0xe86a, .n = 1, .in = B(0x5a), .out = B(0x4e) },
            { .addr = 0xe8a6, .n = 1, .in = B(0x5f), .out = B(0x4d) },
            { .addr = 0x3b220, .n = 1, .in = B(0x4b), .out = B(0x5c) },
            { .addr = 0x3b253, .n = 1, .in = B(0x4a), .out = B(0x5d) },
            { .addr = 0x3b34f, .n = 1, .in = B(0x48), .out = B(0x21) },
            { .addr = 0x3b38e, .n = 1, .in = B(0x55), .out = B(0x22) },
        },
    },

    {
        .title = "Beyond Zork", .serial = "870917", .release = 49, .checksum = 0x24d6,
        .replacements = {
            // Circlet object
            { .addr = 0x2f8b6, .n = 2, .in = B(0xa3, 0x9c), .out = B(0x01, 0x86), },
            { .addr = 0x2f8c0, .n = 2, .in = B(0xa3, 0x9c), .out = B(0x01, 0x86), },
            { .addr = 0x2f8d2, .n = 2, .in = B(0xa3, 0x9c), .out = B(0x01, 0x86), },
            { .addr = 0x2f8dc, .n = 2, .in = B(0xa3, 0x9c), .out = B(0x01, 0x86), },

            // IBM characters
            { .addr = 0xe4c7, .n = 1, .in = B(0xda), .out = B(0x2b) },
            { .addr = 0xe4cd, .n = 1, .in = B(0xc4), .out = B(0x2d) },
            { .addr = 0xe4d5, .n = 1, .in = B(0xbf), .out = B(0x2b) },
            { .addr = 0xe4de, .n = 1, .in = B(0xc0), .out = B(0x2b) },
            { .addr = 0xe4e4, .n = 1, .in = B(0xc4), .out = B(0x2d) },
            { .addr = 0xe4ec, .n = 1, .in = B(0xd9), .out = B(0x2b) },
            { .addr = 0xe4f8, .n = 1, .in = B(0xb3), .out = B(0x7c) },
            { .addr = 0xe501, .n = 1, .in = B(0xb3), .out = B(0x7c) },
            { .addr = 0x3b1e1, .n = 1, .in = B(0x18), .out = B(0x55) },
            { .addr = 0x3b214, .n = 1, .in = B(0x19), .out = B(0x44) },
            { .addr = 0x3b310, .n = 1, .in = B(0x1b), .out = B(0x4c) },
            { .addr = 0x3b34f, .n = 1, .in = B(0x1a), .out = B(0x52) },

            // Apple IIc characters
            { .addr = 0xe526, .n = 1, .in = B(0x4c), .out = B(0x4b) },
            { .addr = 0xe535, .n = 1, .in = B(0x5a), .out = B(0x4e) },
            { .addr = 0xe53c, .n = 1, .in = B(0x5f), .out = B(0x4d) },
            { .addr = 0xe7a2, .n = 1, .in = B(0x5a), .out = B(0x4e) },
            { .addr = 0xe7de, .n = 1, .in = B(0x5f), .out = B(0x4d) },
            { .addr = 0x3b1f0, .n = 1, .in = B(0x4b), .out = B(0x5c) },
            { .addr = 0x3b223, .n = 1, .in = B(0x4a), .out = B(0x5d) },
            { .addr = 0x3b31f, .n = 1, .in = B(0x48), .out = B(0x21) },
            { .addr = 0x3b35e, .n = 1, .in = B(0x55), .out = B(0x22) },
        },
    },

    {
        .title = "Beyond Zork", .serial = "870923", .release = 51, .checksum = 0x0cbe,
        .replacements = {
            // Circlet object
            { .addr = 0x2f762, .n = 2, .in = B(0xa3, 0x8d), .out = B(0x01, 0x86), },
            { .addr = 0x2f76c, .n = 2, .in = B(0xa3, 0x8d), .out = B(0x01, 0x86), },
            { .addr = 0x2f77e, .n = 2, .in = B(0xa3, 0x8d), .out = B(0x01, 0x86), },
            { .addr = 0x2f788, .n = 2, .in = B(0xa3, 0x8d), .out = B(0x01, 0x86), },

            // IBM characters
            { .addr = 0xe4af, .n = 1, .in = B(0xda), .out = B(0x2b) },
            { .addr = 0xe4b5, .n = 1, .in = B(0xc4), .out = B(0x2d) },
            { .addr = 0xe4bd, .n = 1, .in = B(0xbf), .out = B(0x2b) },
            { .addr = 0xe4c6, .n = 1, .in = B(0xc0), .out = B(0x2b) },
            { .addr = 0xe4cc, .n = 1, .in = B(0xc4), .out = B(0x2d) },
            { .addr = 0xe4d4, .n = 1, .in = B(0xd9), .out = B(0x2b) },
            { .addr = 0xe4e0, .n = 1, .in = B(0xb3), .out = B(0x7c) },
            { .addr = 0xe4e9, .n = 1, .in = B(0xb3), .out = B(0x7c) },
            { .addr = 0x3b08d, .n = 1, .in = B(0x18), .out = B(0x55) },
            { .addr = 0x3b0c0, .n = 1, .in = B(0x19), .out = B(0x44) },
            { .addr = 0x3b1bc, .n = 1, .in = B(0x1b), .out = B(0x4c) },
            { .addr = 0x3b1fb, .n = 1, .in = B(0x1a), .out = B(0x52) },

            // Apple IIc characters
            { .addr = 0xe50e, .n = 1, .in = B(0x4c), .out = B(0x4b) },
            { .addr = 0xe51d, .n = 1, .in = B(0x5a), .out = B(0x4e) },
            { .addr = 0xe524, .n = 1, .in = B(0x5f), .out = B(0x4d) },
            { .addr = 0xe78a, .n = 1, .in = B(0x5a), .out = B(0x4e) },
            { .addr = 0xe7c6, .n = 1, .in = B(0x5f), .out = B(0x4d) },
            { .addr = 0x3b09c, .n = 1, .in = B(0x4b), .out = B(0x5c) },
            { .addr = 0x3b0cf, .n = 1, .in = B(0x4a), .out = B(0x5d) },
            { .addr = 0x3b1cb, .n = 1, .in = B(0x48), .out = B(0x21) },
            { .addr = 0x3b20a, .n = 1, .in = B(0x55), .out = B(0x22) },
        },
    },

    {
        .title = "Beyond Zork", .serial = "871221", .release = 57, .checksum = 0xc5ad,
        .replacements = {
            // Circlet object
            { .addr = 0x2fc72, .n = 2, .in = B(0xa3, 0xba), .out = B(0x01, 0x87), },
            { .addr = 0x2fc7c, .n = 2, .in = B(0xa3, 0xba), .out = B(0x01, 0x87), },
            { .addr = 0x2fc8e, .n = 2, .in = B(0xa3, 0xba), .out = B(0x01, 0x87), },
            { .addr = 0x2fc98, .n = 2, .in = B(0xa3, 0xba), .out = B(0x01, 0x87), },

            // IBM characters
            { .addr = 0xe58b, .n = 1, .in = B(0xda), .out = B(0x2b) },
            { .addr = 0xe591, .n = 1, .in = B(0xc4), .out = B(0x2d) },
            { .addr = 0xe599, .n = 1, .in = B(0xbf), .out = B(0x2b) },
            { .addr = 0xe5a2, .n = 1, .in = B(0xc0), .out = B(0x2b) },
            { .addr = 0xe5a8, .n = 1, .in = B(0xc4), .out = B(0x2d) },
            { .addr = 0xe5b0, .n = 1, .in = B(0xd9), .out = B(0x2b) },
            { .addr = 0xe5bc, .n = 1, .in = B(0xb3), .out = B(0x7c) },
            { .addr = 0xe5c5, .n = 1, .in = B(0xb3), .out = B(0x7c) },
            { .addr = 0x3b1dd, .n = 1, .in = B(0x18), .out = B(0x55) },
            { .addr = 0x3b210, .n = 1, .in = B(0x19), .out = B(0x44) },
            { .addr = 0x3b30c, .n = 1, .in = B(0x1b), .out = B(0x4c) },
            { .addr = 0x3b34b, .n = 1, .in = B(0x1a), .out = B(0x52) },

            // Apple IIc characters
            { .addr = 0xe5ea, .n = 1, .in = B(0x4c), .out = B(0x4b) },
            { .addr = 0xe5f9, .n = 1, .in = B(0x5a), .out = B(0x4e) },
            { .addr = 0xe600, .n = 1, .in = B(0x5f), .out = B(0x4d) },
            { .addr = 0xe866, .n = 1, .in = B(0x5a), .out = B(0x4e) },
            { .addr = 0xe8a2, .n = 1, .in = B(0x5f), .out = B(0x4d) },
            { .addr = 0x3b1ec, .n = 1, .in = B(0x4b), .out = B(0x5c) },
            { .addr = 0x3b21f, .n = 1, .in = B(0x4a), .out = B(0x5d) },
            { .addr = 0x3b31b, .n = 1, .in = B(0x48), .out = B(0x21) },
            { .addr = 0x3b35a, .n = 1, .in = B(0x55), .out = B(0x22) },
        },
    },

    {
        .title = "Beyond Zork", .serial = "880610", .release = 60, .checksum = 0xa49d,
        .replacements = {
            // Circlet object
            { .addr = 0x2fbfe, .n = 2, .in = B(0xa3, 0xc0), .out = B(0x01, 0x87), },
            { .addr = 0x2fc08, .n = 2, .in = B(0xa3, 0xc0), .out = B(0x01, 0x87), },
            { .addr = 0x2fc1a, .n = 2, .in = B(0xa3, 0xc0), .out = B(0x01, 0x87), },
            { .addr = 0x2fc24, .n = 2, .in = B(0xa3, 0xc0), .out = B(0x01, 0x87), },

            // IBM characters
            { .addr = 0xe4e3, .n = 1, .in = B(0xda), .out = B(0x2b) },
            { .addr = 0xe4e9, .n = 1, .in = B(0xc4), .out = B(0x2d) },
            { .addr = 0xe4f1, .n = 1, .in = B(0xbf), .out = B(0x2b) },
            { .addr = 0xe4fa, .n = 1, .in = B(0xc0), .out = B(0x2b) },
            { .addr = 0xe500, .n = 1, .in = B(0xc4), .out = B(0x2d) },
            { .addr = 0xe508, .n = 1, .in = B(0xd9), .out = B(0x2b) },
            { .addr = 0xe514, .n = 1, .in = B(0xb3), .out = B(0x7c) },
            { .addr = 0xe51d, .n = 1, .in = B(0xb3), .out = B(0x7c) },
            { .addr = 0x3b169, .n = 1, .in = B(0x18), .out = B(0x55) },
            { .addr = 0x3b19c, .n = 1, .in = B(0x19), .out = B(0x44) },
            { .addr = 0x3b298, .n = 1, .in = B(0x1b), .out = B(0x4c) },
            { .addr = 0x3b2d7, .n = 1, .in = B(0x1a), .out = B(0x52) },

            // Apple IIc characters
            { .addr = 0xe542, .n = 1, .in = B(0x4c), .out = B(0x4b) },
            { .addr = 0xe551, .n = 1, .in = B(0x5a), .out = B(0x4e) },
            { .addr = 0xe558, .n = 1, .in = B(0x5f), .out = B(0x4d) },
            { .addr = 0xe7be, .n = 1, .in = B(0x5a), .out = B(0x4e) },
            { .addr = 0xe7fa, .n = 1, .in = B(0x5f), .out = B(0x4d) },
            { .addr = 0x3b178, .n = 1, .in = B(0x4b), .out = B(0x5c) },
            { .addr = 0x3b1ab, .n = 1, .in = B(0x4a), .out = B(0x5d) },
            { .addr = 0x3b2a7, .n = 1, .in = B(0x48), .out = B(0x21) },
            { .addr = 0x3b2e6, .n = 1, .in = B(0x55), .out = B(0x22) },
        },
    },

    // This is in a routine which iterates over all attributes of an
    // object, but due to an off-by-one error, attribute 48 (0x30) is
    // included, which is not valid, as the last attribute is 47 (0x2f);
    // there are 48 attributes but the count starts at 0.
    {
        .title = "Sherlock", .serial = "871214", .release = 21, .checksum = 0x79b2,
        .replacements = {{ .addr = 0x223ac, .n = 1, .in = B(0x30), .out = B(0x2f), }},
    },
    {
        .title = "Sherlock", .serial = "880112", .release = 22, .checksum = 0xcb96,
        .replacements = {{ .addr = 0x225a4, .n = 1, .in = B(0x30), .out = B(0x2f), }},
    },
    {
        .title = "Sherlock", .serial = "880127", .release = 26, .checksum = 0x26ba,
        .replacements = {{ .addr = 0x22818, .n = 1, .in = B(0x30), .out = B(0x2f), }},
    },
    {
        .title = "Sherlock", .serial = "880324", .release = 4, .checksum = 0x7086,
        .replacements = {{ .addr = 0x22498, .n = 1, .in = B(0x30), .out = B(0x2f), }},
    },

    // The operands to @get_prop here are swapped, so swap them back.
    {
        .title = "Stationfall", .serial = "870326", .release = 87, .checksum = 0x71ae,
        .replacements = {{ .addr = 0xd3d4, .n = 3, .in = B(0x31, 0x0c, 0x73), .out = B(0x51, 0x73, 0x0c), }},
    },
    {
        .title = "Stationfall", .serial = "870430", .release = 107, .checksum = 0x2871,
        .replacements = {{ .addr = 0xe3fe, .n = 3, .in = B(0x31, 0x0c, 0x77), .out = B(0x51, 0x77, 0x0c), }},
    },

    // The Solid Gold (V5) version of Wishbringer calls @show_status, but
    // that is an illegal instruction outside of V3. Convert it to @nop.
    {
        .title = "Wishbringer", .serial = "880706", .release = 23, .checksum = 0x4222,
        .replacements = {{ .addr = 0x1f910, .n = 1, .in = B(0xbc), .out = B(0xb4), }},
    },

    // Robot Finds Kitten attempts to sleep with the following:
    //
    // [ Func junk;
    //     @aread junk 0 10 PauseFunc -> junk;
    // ];
    //
    // However, since “junk” is a local variable with value 0 instead of a
    // text buffer, this is asking to read from/write to address 0. This
    // works in some interpreters, but Bocfel is more strict, and aborts
    // the program. Rewrite this instead to:
    //
    // @read_char 1 10 PauseFunc -> junk;
    // @nop; ! This is for padding.
    {
        .title = "Robot Finds Kitten", .serial = "130320", .release = 7, .checksum = 0x4a18,
        .replacements = {
            {
                .addr = 0x4912, .n = 8,
                .in = B(0xe4, 0x94, 0x05, 0x00, 0x0a, 0x12, 0x5a, 0x05),
                .out = B(0xf6, 0x53, 0x01, 0x0a, 0x12, 0x5a, 0x05, 0xb4),
            },
        }
    },

    { .title = NULL },
};

static void apply_patch(const struct replacement *r)
{
    if (r->addr >= header.static_start &&
        r->addr + r->n < memory_size &&
        memcmp(&memory[r->addr], r->in, r->n) == 0) {

        memcpy(&memory[r->addr], r->out, r->n);
    }
}

void apply_patches(void)
{
    for (const struct patch *patch = patches; patch->title != NULL; patch++) {
        if (memcmp(patch->serial, header.serial, sizeof header.serial) == 0 &&
            patch->release == header.release &&
            patch->checksum == header.checksum) {

            for (size_t i = 0; patch->replacements[i].addr != 0; i++) {
                apply_patch(&patch->replacements[i]);
            }
        }
    }
}
