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

#include <cstring>
#include <string>
#include <vector>

#include "patches.h"
#include "memory.h"
#include "types.h"
#include "util.h"
#include "zterp.h"

struct Replacement {
    uint32_t addr;
    size_t n;
    std::vector<uint8_t> in;
    std::vector<uint8_t> out;
};

struct Patch {
    std::string title;
    const char (*serial)[sizeof header.serial + 1];
    uint16_t release;
    uint16_t checksum;
    std::vector<Replacement> replacements;
};

static std::vector<Patch> patches = {
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
        "Beyond Zork", &"870915", 47, 0x3ff4,
        {
            // Circlet object
            { 0x2f8e6, 2, {0xa3, 0x9a}, {0x01, 0x86} },
            { 0x2f8f0, 2, {0xa3, 0x9a}, {0x01, 0x86} },
            { 0x2f902, 2, {0xa3, 0x9a}, {0x01, 0x86} },
            { 0x2f90c, 2, {0xa3, 0x9a}, {0x01, 0x86} },

            // IBM characters
            { 0xe58f, 1, {0xda}, {0x2b} },
            { 0xe595, 1, {0xc4}, {0x2d} },
            { 0xe59d, 1, {0xbf}, {0x2b} },
            { 0xe5a6, 1, {0xc0}, {0x2b} },
            { 0xe5ac, 1, {0xc4}, {0x2d} },
            { 0xe5b4, 1, {0xd9}, {0x2b} },
            { 0xe5c0, 1, {0xb3}, {0x7c} },
            { 0xe5c9, 1, {0xb3}, {0x7c} },
            { 0x3b211, 1, {0x18}, {0x55} },
            { 0x3b244, 1, {0x19}, {0x44} },
            { 0x3b340, 1, {0x1b}, {0x4c} },
            { 0x3b37f, 1, {0x1a}, {0x52} },

            // Apple IIc characters
            { 0xe5ee, 1, {0x4c}, {0x4b} },
            { 0xe5fd, 1, {0x5a}, {0x4e} },
            { 0xe604, 1, {0x5f}, {0x4d} },
            { 0xe86a, 1, {0x5a}, {0x4e} },
            { 0xe8a6, 1, {0x5f}, {0x4d} },
            { 0x3b220, 1, {0x4b}, {0x5c} },
            { 0x3b253, 1, {0x4a}, {0x5d} },
            { 0x3b34f, 1, {0x48}, {0x21} },
            { 0x3b38e, 1, {0x55}, {0x22} },
        },
    },

    {
        "Beyond Zork", &"870917", 49, 0x24d6,
        {
            // Circlet object
            { 0x2f8b6, 2, {0xa3, 0x9c}, {0x01, 0x86} },
            { 0x2f8c0, 2, {0xa3, 0x9c}, {0x01, 0x86} },
            { 0x2f8d2, 2, {0xa3, 0x9c}, {0x01, 0x86} },
            { 0x2f8dc, 2, {0xa3, 0x9c}, {0x01, 0x86} },

            // IBM characters
            { 0xe4c7, 1, {0xda}, {0x2b} },
            { 0xe4cd, 1, {0xc4}, {0x2d} },
            { 0xe4d5, 1, {0xbf}, {0x2b} },
            { 0xe4de, 1, {0xc0}, {0x2b} },
            { 0xe4e4, 1, {0xc4}, {0x2d} },
            { 0xe4ec, 1, {0xd9}, {0x2b} },
            { 0xe4f8, 1, {0xb3}, {0x7c} },
            { 0xe501, 1, {0xb3}, {0x7c} },
            { 0x3b1e1, 1, {0x18}, {0x55} },
            { 0x3b214, 1, {0x19}, {0x44} },
            { 0x3b310, 1, {0x1b}, {0x4c} },
            { 0x3b34f, 1, {0x1a}, {0x52} },

            // Apple IIc characters
            { 0xe526, 1, {0x4c}, {0x4b} },
            { 0xe535, 1, {0x5a}, {0x4e} },
            { 0xe53c, 1, {0x5f}, {0x4d} },
            { 0xe7a2, 1, {0x5a}, {0x4e} },
            { 0xe7de, 1, {0x5f}, {0x4d} },
            { 0x3b1f0, 1, {0x4b}, {0x5c} },
            { 0x3b223, 1, {0x4a}, {0x5d} },
            { 0x3b31f, 1, {0x48}, {0x21} },
            { 0x3b35e, 1, {0x55}, {0x22} },
        },
    },

    {
        "Beyond Zork", &"870923", 51, 0x0cbe,
        {
            // Circlet object
            { 0x2f762, 2, {0xa3, 0x8d}, {0x01, 0x86} },
            { 0x2f76c, 2, {0xa3, 0x8d}, {0x01, 0x86} },
            { 0x2f77e, 2, {0xa3, 0x8d}, {0x01, 0x86} },
            { 0x2f788, 2, {0xa3, 0x8d}, {0x01, 0x86} },

            // IBM characters
            { 0xe4af, 1, {0xda}, {0x2b} },
            { 0xe4b5, 1, {0xc4}, {0x2d} },
            { 0xe4bd, 1, {0xbf}, {0x2b} },
            { 0xe4c6, 1, {0xc0}, {0x2b} },
            { 0xe4cc, 1, {0xc4}, {0x2d} },
            { 0xe4d4, 1, {0xd9}, {0x2b} },
            { 0xe4e0, 1, {0xb3}, {0x7c} },
            { 0xe4e9, 1, {0xb3}, {0x7c} },
            { 0x3b08d, 1, {0x18}, {0x55} },
            { 0x3b0c0, 1, {0x19}, {0x44} },
            { 0x3b1bc, 1, {0x1b}, {0x4c} },
            { 0x3b1fb, 1, {0x1a}, {0x52} },

            // Apple IIc characters
            { 0xe50e, 1, {0x4c}, {0x4b} },
            { 0xe51d, 1, {0x5a}, {0x4e} },
            { 0xe524, 1, {0x5f}, {0x4d} },
            { 0xe78a, 1, {0x5a}, {0x4e} },
            { 0xe7c6, 1, {0x5f}, {0x4d} },
            { 0x3b09c, 1, {0x4b}, {0x5c} },
            { 0x3b0cf, 1, {0x4a}, {0x5d} },
            { 0x3b1cb, 1, {0x48}, {0x21} },
            { 0x3b20a, 1, {0x55}, {0x22} },
        },
    },

    {
        "Beyond Zork", &"871221", 57, 0xc5ad,
        {
            // Circlet object
            { 0x2fc72, 2, {0xa3, 0xba}, {0x01, 0x87} },
            { 0x2fc7c, 2, {0xa3, 0xba}, {0x01, 0x87} },
            { 0x2fc8e, 2, {0xa3, 0xba}, {0x01, 0x87} },
            { 0x2fc98, 2, {0xa3, 0xba}, {0x01, 0x87} },

            // IBM characters
            { 0xe58b, 1, {0xda}, {0x2b} },
            { 0xe591, 1, {0xc4}, {0x2d} },
            { 0xe599, 1, {0xbf}, {0x2b} },
            { 0xe5a2, 1, {0xc0}, {0x2b} },
            { 0xe5a8, 1, {0xc4}, {0x2d} },
            { 0xe5b0, 1, {0xd9}, {0x2b} },
            { 0xe5bc, 1, {0xb3}, {0x7c} },
            { 0xe5c5, 1, {0xb3}, {0x7c} },
            { 0x3b1dd, 1, {0x18}, {0x55} },
            { 0x3b210, 1, {0x19}, {0x44} },
            { 0x3b30c, 1, {0x1b}, {0x4c} },
            { 0x3b34b, 1, {0x1a}, {0x52} },

            // Apple IIc characters
            { 0xe5ea, 1, {0x4c}, {0x4b} },
            { 0xe5f9, 1, {0x5a}, {0x4e} },
            { 0xe600, 1, {0x5f}, {0x4d} },
            { 0xe866, 1, {0x5a}, {0x4e} },
            { 0xe8a2, 1, {0x5f}, {0x4d} },
            { 0x3b1ec, 1, {0x4b}, {0x5c} },
            { 0x3b21f, 1, {0x4a}, {0x5d} },
            { 0x3b31b, 1, {0x48}, {0x21} },
            { 0x3b35a, 1, {0x55}, {0x22} },
        },
    },

    {
        "Beyond Zork", &"880610", 60, 0xa49d,
        {
            // Circlet object
            { 0x2fbfe, 2, {0xa3, 0xc0}, {0x01, 0x87} },
            { 0x2fc08, 2, {0xa3, 0xc0}, {0x01, 0x87} },
            { 0x2fc1a, 2, {0xa3, 0xc0}, {0x01, 0x87} },
            { 0x2fc24, 2, {0xa3, 0xc0}, {0x01, 0x87} },

            // IBM characters
            { 0xe4e3, 1, {0xda}, {0x2b} },
            { 0xe4e9, 1, {0xc4}, {0x2d} },
            { 0xe4f1, 1, {0xbf}, {0x2b} },
            { 0xe4fa, 1, {0xc0}, {0x2b} },
            { 0xe500, 1, {0xc4}, {0x2d} },
            { 0xe508, 1, {0xd9}, {0x2b} },
            { 0xe514, 1, {0xb3}, {0x7c} },
            { 0xe51d, 1, {0xb3}, {0x7c} },
            { 0x3b169, 1, {0x18}, {0x55} },
            { 0x3b19c, 1, {0x19}, {0x44} },
            { 0x3b298, 1, {0x1b}, {0x4c} },
            { 0x3b2d7, 1, {0x1a}, {0x52} },

            // Apple IIc characters
            { 0xe542, 1, {0x4c}, {0x4b} },
            { 0xe551, 1, {0x5a}, {0x4e} },
            { 0xe558, 1, {0x5f}, {0x4d} },
            { 0xe7be, 1, {0x5a}, {0x4e} },
            { 0xe7fa, 1, {0x5f}, {0x4d} },
            { 0x3b178, 1, {0x4b}, {0x5c} },
            { 0x3b1ab, 1, {0x4a}, {0x5d} },
            { 0x3b2a7, 1, {0x48}, {0x21} },
            { 0x3b2e6, 1, {0x55}, {0x22} },
        },
    },

    // Bureaucracy has a routine meant to sleep for a certain amount of
    // time, but it implements this using a busy loop based on the
    // target machine, each machine having its own iteration count which
    // should approximate the number of seconds passed in. This doesn’t
    // work on modern machines at all, as they’re far too fast. A
    // replacement is provided here which does the following:
    //
    // [ Delay n;
    //     @mul n $0a -> n;
    //     @read_char 1 n Pause -> sp;
    //     @rtrue;
    // ];
    //
    // [ Pause;
    //     @rtrue;
    // ];
    {
        "Bureaucracy", &"870212", 86, 0xe024,
        {
            {
                0x2128c, 18,
                {0x02, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x1e, 0x00, 0xd0, 0x2f, 0xde, 0x5c, 0x00, 0x02, 0xd6, 0x2f, 0x03},
                {0x01, 0x00, 0x01, 0x56, 0x01, 0x0a, 0x01, 0xf6, 0x63, 0x01, 0x01, 0x84, 0xa7, 0x00, 0xb0, 0x00, 0x00, 0xb0},
            }
        }
    },
    {
        "Bureaucracy", &"870602", 116, 0xfc65,
        {
            {
                0x212d0, 18,
                {0x02, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x1e, 0x00, 0xd0, 0x2f, 0xde, 0x64, 0x00, 0x02, 0xd6, 0x2f, 0x03},
                {0x01, 0x00, 0x01, 0x56, 0x01, 0x0a, 0x01, 0xf6, 0x63, 0x01, 0x01, 0x84, 0xb8, 0x00, 0xb0, 0x00, 0x00, 0xb0},
            }
        }
    },
    {
        "Bureaucracy", &"880521", 160, 0x07f0,
        {
            {
                0x212c8, 18,
                {0x02, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x1e, 0x00, 0xd0, 0x2f, 0xdc, 0xce, 0x00, 0x02, 0xd6, 0x2f, 0x03},
                {0x01, 0x00, 0x01, 0x56, 0x01, 0x0a, 0x01, 0xf6, 0x63, 0x01, 0x01, 0x84, 0xb6, 0x00, 0xb0, 0x00, 0x00, 0xb0},
            }
        }
    },

    // This is in a routine which iterates over all attributes of an
    // object, but due to an off-by-one error, attribute 48 (0x30) is
    // included, which is not valid, as the last attribute is 47 (0x2f);
    // there are 48 attributes but the count starts at 0.
    {
        "Sherlock", &"871214", 21, 0x79b2,
        {{ 0x223ac, 1, {0x30}, {0x2f} }},
    },
    {
        "Sherlock", &"880112", 22, 0xcb96,
        {{ 0x225a4, 1, {0x30}, {0x2f} }},
    },
    {
        "Sherlock", &"880127", 26, 0x26ba,
        {{ 0x22818, 1, {0x30}, {0x2f} }},
    },
    {
        "Sherlock", &"880324", 4, 0x7086,
        {{ 0x22498, 1, {0x30}, {0x2f} }},
    },

    // The operands to @get_prop here are swapped, so swap them back.
    {
        "Stationfall", &"870326", 87, 0x71ae,
        {{ 0xd3d4, 3, {0x31, 0x0c, 0x73}, {0x51, 0x73, 0x0c} }},
    },
    {
        "Stationfall", &"870430", 107, 0x2871,
        {{ 0xe3fe, 3, {0x31, 0x0c, 0x77}, {0x51, 0x77, 0x0c} }},
    },

    // The Solid Gold (V5) version of Wishbringer calls @show_status, but
    // that is an illegal instruction outside of V3. Convert it to @nop.
    {
        "Wishbringer", &"880706", 23, 0x4222,
        {{ 0x1f910, 1, {0xbc}, {0xb4} }},
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
        "Robot Finds Kitten", &"130320", 7, 0x4a18,
        {
            {
                0x4912, 8,
                {0xe4, 0x94, 0x05, 0x00, 0x0a, 0x12, 0x5a, 0x05},
                {0xf6, 0x53, 0x01, 0x0a, 0x12, 0x5a, 0x05, 0xb4},
            },
        }
    },
};

static bool apply_patch(const Replacement &r)
{
    if (r.addr >= header.static_start &&
        r.addr + r.n < memory_size &&
        std::memcmp(&memory[r.addr], &r.in[0], r.n) == 0) {

        std::memcpy(&memory[r.addr], &r.out[0], r.n);

        return true;
    }

    return false;
}

void apply_patches()
{
    for (const auto &patch : patches) {
        if (std::memcmp(*patch.serial, header.serial, sizeof header.serial) == 0 &&
            patch.release == header.release &&
            patch.checksum == header.checksum) {

            for (const auto &replacement : patch.replacements) {
                apply_patch(replacement);
            }
        }
    }
}

static bool read_into(std::vector<uint8_t> &buf, long count)
{
    for (long i = 0; i < count; i++) {
        long byte;
        bool valid;
        char *p = std::strtok(nullptr, " \t[],");
        if (p == nullptr) {
            return false;
        }
        byte = parseint(p, 16, valid);
        if (!valid || byte < 0 || byte > 255) {
            return false;
        }
        buf.push_back(byte);
    }

    return true;
}

// User patches have the form:
//
// addr count original... replacement...
//
// For example, one of the Stationfall fixes would look like this:
//
// 0xd3d4 3 0x31 0x0c 0x73 0x51 0x73 0x0c
//
// Square brackets and commas are treated as whitespace for the actual
// bytes, so for convenience this could also be written:
//
// 0xd3d4 3 [0x31, 0x0c, 0x73] [0x51, 0x73, 0x0c]
PatchStatus apply_user_patch(std::string patchstr)
{
    char *p;
    bool valid;
    uint32_t addr, count;
    std::vector<uint8_t> in, out;

    p = std::strtok(&patchstr[0], " \t");
    if (p == nullptr) {
        return PatchStatus::SyntaxError;
    }

    addr = parseint(p, 16, valid);
    if (!valid) {
        return PatchStatus::SyntaxError;
    }

    p = std::strtok(nullptr, " \t");
    if (p == nullptr) {
        return PatchStatus::SyntaxError;
    }

    count = parseint(p, 10, valid);
    if (!valid) {
        return PatchStatus::SyntaxError;
    }

    if (!read_into(in, count) || !read_into(out, count)) {
        return PatchStatus::SyntaxError;
    }

    Replacement replacement = {
        addr,
        count,
        in,
        out,
    };

    return apply_patch(replacement) ? PatchStatus::Ok : PatchStatus::NotFound;
}
