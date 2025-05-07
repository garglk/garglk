// Copyright 2017-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "patches.h"
#include "memory.h"
#include "process.h"
#include "types.h"
#include "util.h"
#include "zterp.h"

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}
#endif

struct Replacement {
    uint32_t addr;
    uint32_t n;
    std::vector<uint8_t> in;
    std::vector<uint8_t> out;
    std::function<bool()> active = [] { return true; };
};

struct Patch {
    std::string title;
    const char (&serial)[sizeof header.serial + 1];
    uint16_t release;
    uint16_t checksum;
    std::vector<Replacement> replacements;
};

// The Bureaucracy patch assumes that timed input is available, as it
// uses that to simulate a sleep. If timed input is not available,
// though, it will instead just be a regular @read_char, which will
// block till the user hits a key. This is worse behavior than the
// default, which is to effectively not sleep at all. Timed input is
// only available with Glk, and then only if the Glk implementation
// supports timers.
#ifdef ZTERP_GLK
static bool bureaucracy_active()
{
    return glk_gestalt(gestalt_Timer, 0);
}
#else
static bool bureaucracy_active()
{
    return false;
}
#endif

static std::vector<Patch> base_patches = {
    // In Arthur, there is a routine called INIT-STATUS-LINE which does this:
    //
    // <PUTB ,K-DIROUT-TBL 0 !\ >
    // <SET N </ <WINGET 1 ,K-W-XSIZE> ,GL-SPACE-WIDTH>>
    // <COPYT ,K-DIROUT-TBL <ZREST ,K-DIROUT-TBL 1> <- .N>>
    //
    // The intent is to fill K-DIROUT-TBL with as many spaces as there
    // are characters in a line, but K-DIROUT-TBL is only 255 bytes
    // wide. If K-W-XSIZE / GL-SPACE-WIDTH is greater than 255, it will
    // overrun the buffer. This patch pulls the width from the value at
    // 0x21 in the header, which is the width of the screen in
    // characters, capped at 255.
    {
        "Arthur", "890606", 54, 0x8e4a,
        {
            {
                0x9e29, 10,
                {0xbe, 0x13, 0x5f, 0x01, 0x03, 0x00, 0x77, 0x00, 0x1f, 0x01},
                {0x10, 0x00, 0x21, 0x01, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4},
            }
        }
    },

    {
        "Arthur", "890622", 63, 0x45eb,
        {
            {
                0x9e31, 10,
                {0xbe, 0x13, 0x5f, 0x01, 0x03, 0x00, 0x77, 0x00, 0x1f, 0x01},
                {0x10, 0x00, 0x21, 0x01, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4}
            }
        }
    },

    {
        "Arthur", "890714", 74, 0xd526,
        {
            {
                0x9de8, 10,
                {0xbe, 0x13, 0x5f, 0x01, 0x03, 0x00, 0x77, 0x00, 0x1f, 0x01},
                {0x10, 0x00, 0x21, 0x01, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4}
            }
        }
    },

    // There are several patches for Beyond Zork.
    //
    // In four places it tries to treat a dictionary word as an object.
    // The offending code looks like:
    //
    // <REPLACE-SYN? ,W?CIRCLET ,W?FILM ,W?ZZZP>
    // <REPLACE-ADJ? ,W?CIRCLET ,W?SWIRLING ,W?ZZZP>
    //
    // Here “,W?CIRCLET” is the dictionary word “circlet”; what was
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
    //
    // Beyond Zork, like Trinity, uses a routine called CARRIAGE-RETURNS
    // which dumps newlines in an attempt to force a [MORE] prompt; see
    // the Trinity section for more information.
    {
        "Beyond Zork", "870915", 47, 0x3ff4,
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

#ifdef ZTERP_GLK
            // CARRIAGE-RETURNS
            {
                0x3cafb, 11,
                {0x8f, 0x42, 0xdf, 0xed, 0x3f, 0xff, 0xff, 0x7b, 0x13, 0x83, 0xbb},
                {0xf6, 0x7f, 0x01, 0x00, 0xed, 0x3f, 0xff, 0xff, 0x7b, 0x13, 0x83},
            },
#endif
        },
    },

    {
        "Beyond Zork", "870917", 49, 0x24d6,
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

#ifdef ZTERP_GLK
            // CARRIAGE-RETURNS
            {
                0x3cac7, 11,
                {0x8f, 0x42, 0xbd, 0xed, 0x3f, 0xff, 0xff, 0x7b, 0x13, 0x83, 0xbb},
                {0xf6, 0x7f, 0x01, 0x00, 0xed, 0x3f, 0xff, 0xff, 0x7b, 0x13, 0x83},
            },
#endif
        },
    },

    {
        "Beyond Zork", "870923", 51, 0x0cbe,
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

#ifdef ZTERP_GLK
            // CARRIAGE-RETURNS
            {
                0x3c967, 11,
                {0x8f, 0x42, 0xb7, 0xed, 0x3f, 0xff, 0xff, 0x7b, 0x13, 0x83, 0xbb},
                {0xf6, 0x7f, 0x01, 0x00, 0xed, 0x3f, 0xff, 0xff, 0x7b, 0x13, 0x83},
            },
#endif
        },
    },

    {
        "Beyond Zork", "871221", 57, 0xc5ad,
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

#ifdef ZTERP_GLK
            // CARRIAGE-RETURNS
            {
                0x3ca37, 11,
                {0x8f, 0x44, 0x55, 0xed, 0x3f, 0xff, 0xff, 0x7b, 0x13, 0x83, 0xbb},
                {0xf6, 0x7f, 0x01, 0x00, 0xed, 0x3f, 0xff, 0xff, 0x7b, 0x13, 0x83},
            },
#endif
        },
    },

    {
        "Beyond Zork", "880610", 60, 0xa49d,
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

#ifdef ZTERP_GLK
            // CARRIAGE-RETURNS
            {
                0x3c9c3, 11,
                {0x8f, 0x44, 0x2b, 0xed, 0x3f, 0xff, 0xff, 0x7b, 0x13, 0x83, 0xbb},
                {0xf6, 0x7f, 0x01, 0x00, 0xed, 0x3f, 0xff, 0xff, 0x7b, 0x13, 0x83},
            },
#endif
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
        "Bureaucracy", "870212", 86, 0xe024,
        {
            {
                0x2128c, 18,
                {0x02, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x1e, 0x00, 0xd0, 0x2f, 0xde, 0x5c, 0x00, 0x02, 0xd6, 0x2f, 0x03},
                {0x01, 0x00, 0x01, 0x56, 0x01, 0x0a, 0x01, 0xf6, 0x63, 0x01, 0x01, 0x84, 0xa7, 0x00, 0xb0, 0x00, 0x00, 0xb0},
                bureaucracy_active,
            }
        }
    },
    {
        "Bureaucracy", "870602", 116, 0xfc65,
        {
            {
                0x212d0, 18,
                {0x02, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x1e, 0x00, 0xd0, 0x2f, 0xde, 0x64, 0x00, 0x02, 0xd6, 0x2f, 0x03},
                {0x01, 0x00, 0x01, 0x56, 0x01, 0x0a, 0x01, 0xf6, 0x63, 0x01, 0x01, 0x84, 0xb8, 0x00, 0xb0, 0x00, 0x00, 0xb0},
                bureaucracy_active,
            }
        }
    },
    {
        "Bureaucracy", "880521", 160, 0x07f0,
        {
            {
                0x212c8, 18,
                {0x02, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x1e, 0x00, 0xd0, 0x2f, 0xdc, 0xce, 0x00, 0x02, 0xd6, 0x2f, 0x03},
                {0x01, 0x00, 0x01, 0x56, 0x01, 0x0a, 0x01, 0xf6, 0x63, 0x01, 0x01, 0x84, 0xb6, 0x00, 0xb0, 0x00, 0x00, 0xb0},
                bureaucracy_active,
            }
        }
    },

    // Journey doubles up the word “essence” when examining Praxix’s
    // pouch: originally the game printed short descriptions when the
    // screen was narrow enough, which didn't include the word
    // “essence”, so it always just printed out an extra “essence”,
    // knowing that it wasn't shown. However, release 51 changed this so
    // that the long descriptions, which included “essence”, were
    // unconditionally printed in this circumstance; but the extra
    // “essence” remained. This patches out the printing of that extra
    // word by replacing it with @nop.
    {
        "Journey", "890522", 51, 0x4f59,
        {{ 0x9d9f, 3, {0xb2, 0x80, 0x3e}, {0xb4, 0xb4, 0xb4} }},
    },
    {
        "Journey", "890526", 54, 0x5707,
        {{ 0x9e2f, 3, {0xb2, 0x80, 0x3e}, {0xb4, 0xb4, 0xb4} }},
    },
    {
        "Journey", "890616", 77, 0xb136,
        {{ 0xa09f, 3, {0xb2, 0x80, 0x3e}, {0xb4, 0xb4, 0xb4} }},
    },
    {
        "Journey", "890627", 79, 0xff04,
        {{ 0xa10b, 3, {0xb2, 0x80, 0x3e}, {0xb4, 0xb4, 0xb4} }},
    },
    {
        "Journey", "890706", 83, 0xd2b8,
        {{ 0xa0ef, 3, {0xb2, 0x80, 0x3e}, {0xb4, 0xb4, 0xb4} }},
    },

    // This is in a routine which iterates over all attributes of an
    // object, but due to an off-by-one error, attribute 48 (0x30) is
    // included, which is not valid, as the last attribute is 47 (0x2f);
    // there are 48 attributes but the count starts at 0.
    {
        "Sherlock", "871214", 21, 0x79b2,
        {{ 0x223ac, 1, {0x30}, {0x2f} }},
    },
    {
        "Sherlock", "880112", 22, 0xcb96,
        {{ 0x225a4, 1, {0x30}, {0x2f} }},
    },
    {
        "Sherlock", "880127", 26, 0x26ba,
        {{ 0x22818, 1, {0x30}, {0x2f} }},
    },
    {
        "Sherlock", "880324", 4, 0x7086,
        {{ 0x22498, 1, {0x30}, {0x2f} }},
    },

    // The operands to @get_prop here are swapped, so swap them back.
    {
        "Stationfall", "870326", 87, 0x71ae,
        {{ 0xd3d4, 3, {0x31, 0x0c, 0x73}, {0x51, 0x73, 0x0c} }},
    },
    {
        "Stationfall", "870430", 107, 0x2871,
        {{ 0xe3fe, 3, {0x31, 0x0c, 0x77}, {0x51, 0x77, 0x0c} }},
    },

    // Trinity does the following after the prologue:
    //
    // <CARRIAGE-RETURNS>
    // <CLEAR -1>
    //
    // Where CARRIAGE-RETURNS prints 23 newlines, trying to get a [MORE]
    // prompt to be displayed; this assumes specific interpreter
    // behavior as well as screen size. It doesn’t work with Glk, as the
    // screen is cleared without a [MORE] prompt, preventing the text
    // from being seen. As a workaround, replace the call to
    // CARRIAGE-RETURNS with “@read_char 1 -> sp”, which will force the
    // screen to pause before clearing.
    //
    // This isn’t ideal since the user isn’t given an indication that a
    // keypress is needed, but it won’t take much time to figure it out.
    //
    // It also assumes a specific behavior from Glk (immediately
    // clearing the screen without printing [MORE]), although most major
    // Glk implementations have this behavior:
    // garglk, Spatterlight, glkterm, winglk, xglk, dosglk
    //
    // In addition, even if a Glk implementation does work “properly” in
    // this case, the fact that 23 newlines are used means that it’ll
    // only work if the user’s screen is not too tall. This obviously
    // was done with the expectation that it’d be running on an 80x25
    // screen.
    //
    // In the end, the slight inconvenience of a “silent” @read_char is
    // worth it for the benefit of not losing game text.
#ifdef ZTERP_GLK
    {
        "Trinity", "860509", 11, 0xfaae,
        {{ 0x2d453, 4, {0x88, 0x44, 0x33, 0x00}, {0xf6, 0x7f, 0x01, 0x00}}},
    },
    {
        "Trinity", "860926", 12, 0x16ab,
        {{ 0x2d487, 4, {0x88, 0x44, 0x30, 0x00}, {0xf6, 0x7f, 0x01, 0x00}}},
    },
    {
        "Trinity", "870628", 15, 0xf112,
        {{ 0x2d25f, 4, {0x88, 0x44, 0x12, 0x00}, {0xf6, 0x7f, 0x01, 0x00}}},
    },
#endif

    // The Solid Gold (V5) version of Wishbringer calls @show_status, but
    // that is an illegal instruction outside of V3. Convert it to @nop.
    {
        "Wishbringer", "880706", 23, 0x4222,
        {{ 0x1f910, 1, {0xbc}, {0xb4} }},
    },

#ifdef ZTERP_STATIC_PATCH_FILE
#include ZTERP_STATIC_PATCH_FILE
#endif
};

// These patches help with the V6 hacks.
static std::vector<Patch> v6_patches = {
    // There are two V6 hack patches for Arthur:
    //
    // 1. In the intro, two images are shown in immediate succession:
    //
    // <RT-CENTER-PIC ,K-PIC-SWORD>
    // <RT-CENTER-PIC ,K-PIC-SWORD-MERLIN>
    //
    // This is presumably under the assmption that drawing is slow, so
    // it will look like a small animation. On modern systems
    // K-PIC-SWORD won’t be seen in this sequence, so this patch
    // rewrites the code to add a 1s sleep call via @read_char. There
    // are two calls to @set_cursor (to hide the cursor) which have no
    // effect in Bocfel, giving 8 total bytes to work with, which is
    // enough to add the new call. The following:
    //
    // <RT-CENTER-PIC ,K-PIC-SWORD-MERLIN>     |   call_2n         #19234 #03
    // <CURSET -1> ;"Make cursor go away."     |   set_cursor      #ffff
    // <INPUT 1 150 ,RT-STOP-READ>             |   read_char       #01 #96 #11d64 -> -(SP)
    // <CURSET -2> ;"Make cursor come back."   |   set_cursor      #fffe
    //
    // is replaced with:
    //
    // <INPUT 1 10 ,RT-STOP-READ>              |   read_char        #01 #0a #11d64 -> -(SP)
    // <RT-CENTER-PIC ,K-PIC-SWORD-MERLIN>     |   call_2n          #19234 #03
    // <INPUT 1 150 ,RT-STOP-READ>             |   read_char        #01 #96 #11d64 -> -(SP)
    // <NOOP>                                  |   nop
    //
    // (The routine addresses above represent the 74-890714 version but
    // the approach is the same for all versions).
    //
    // 2. Parser messages are meant to be displayed on the bottom of the
    // screen, but since Bocfel doesn’t have real V6 window support, the
    // messages are displayed inline as with most other Infocom games.
    // However, the messages are printed in reverse video (in fact, the
    // current color is looked up with @get_wind_prop, @set_colour is
    // called with the values swapped, the message is printed, and then
    // @set_colour puts things back). This might look OK with the parser
    // messages in a separate window, but it’s jarring interleaved with
    // user input, so this removes the calls to @set_colour entirely.
    {
        "Arthur", "890606", 54, 0x8e4a,
        {
            // Merlin.
            {
                0x11418, 20,
                {0xda, 0x1f, 0x49, 0xae, 0x03, 0xef, 0x3f, 0xff, 0xff, 0xf6, 0x53, 0x01, 0x96, 0x21, 0xd4, 0x00, 0xef, 0x3f, 0xff, 0xfe},
                {0xf6, 0x53, 0x01, 0x0a, 0x21, 0xd4, 0x00, 0xda, 0x1f, 0x49, 0xae, 0x03, 0xf6, 0x53, 0x01, 0x96, 0x21, 0xd4, 0x00, 0xb4}
            },

            // Reverse video.
            { 0x117bf, 3, {0x7b, 0x0d, 0x0c}, {0xb4, 0xb4, 0xb4} },
            { 0x117cb, 3, {0x7b, 0x0c, 0x0d}, {0xb4, 0xb4, 0xb4} },
        }
    },

    {
        "Arthur", "890622", 63, 0x45eb,
        {
            // Merlin.
            {
                0x1147e, 20,
                {0xda, 0x1f, 0x3f, 0x2e, 0x03, 0xef, 0x3f, 0xff, 0xff, 0xf6, 0x53, 0x01, 0x96, 0x22, 0x04, 0x00, 0xef, 0x3f, 0xff, 0xfe},
                {0xf6, 0x53, 0x01, 0x0a, 0x22, 0x04, 0x00, 0xda, 0x1f, 0x3f, 0x2e, 0x03, 0xf6, 0x53, 0x01, 0x96, 0x22, 0x04, 0x00, 0xb4}
            },

            // Reverse video.
            { 0x11853, 3, {0x7b, 0x0d, 0x0c}, {0xb4, 0xb4, 0xb4} },
            { 0x1185f, 3, {0x7b, 0x0c, 0x0d}, {0xb4, 0xb4, 0xb4} },
        }
    },

    {
        "Arthur", "890714", 74, 0xd526,
        {
            // Merlin.
            {
                0x10e76, 20,
                {0xda, 0x1f, 0x3d, 0xb1, 0x03, 0xef, 0x3f, 0xff, 0xff, 0xf6, 0x53, 0x01, 0x96, 0x20, 0x7d, 0x00, 0xef, 0x3f, 0xff, 0xfe},
                {0xf6, 0x53, 0x01, 0x0a, 0x20, 0x7d, 0x00, 0xda, 0x1f, 0x3d, 0xb1, 0x03, 0xf6, 0x53, 0x01, 0x96, 0x20, 0x7d, 0x00, 0xb4}
            },

            // Reverse video.
            { 0x1124b, 3, {0x7b, 0x0d, 0x0c}, {0xb4, 0xb4, 0xb4} },
            { 0x11257, 3, {0x7b, 0x0c, 0x0d}, {0xb4, 0xb4, 0xb4} },
        },
    },

    // There are several V6 hack patches for Shogun:
    //
    // 1. Avoid calling a function that prints too many newlines during
    // interludes.
    //
    // 2. Shogun uses either @set_colour or @set_text_style for reverse
    // video in the status line, but it only uses @set_text_style on
    // Amiga. Unconditionally use @set_text_style for better results.
    //
    // 3. Shogun clears the window right after printing the title
    // screen, rendering it invisible without proper V6 support. Ignore
    // that request.
    //
    // 4. Shogun has a GET-FROM-MENU routine which is used to present a
    // menu (e.g. at the start of the game) from which the user selects
    // an entry. This menu does not work properly with Glk, so a custom
    // opcode replacement is used to present a working menu.
    //
    // 5. Two Shogun releases split the window for the title screen such
    // that the actual text displayed is in a tiny main window and hard
    // to deal with. Without proper V6 support, the split is
    // problematic, so just don’t do it.
    {
        "Shogun", "890314", 292, 0x69b8,
        {
            // Newlines.
            { 0x11ef1, 1, {0xda}, {0xb0} },

            // Reverse video.
            { 0x11be5, 3, {0x7b, 0xde, 0x2c}, {0xf1, 0x7f, 0x01} },

            // Title clear.
            { 0x10bcf, 3, {0xed, 0x7f, 0x00}, {0xb4, 0xb4, 0xb4} },

            // Game start menu.
            {
                0x10bd5, 5,
                {0xec, 0x00, 0x7f, 0x15, 0x1c},
                {0xb4, 0xb4, 0xbe, SHOGUN_MENU_EXT, 0x01},
            },

            // Game over menu.
            {
                0x12914, 4,
                {0xe0, 0x08, 0x15, 0x1c},
                {0xb4, 0xbe, SHOGUN_MENU_EXT, 0x23},
            },

            // End of scene menu.
            {
                0x13caf, 4,
                {0xe0, 0x08, 0x15, 0x1c},
                {0xb4, 0xbe, SHOGUN_MENU_EXT, 0x23},
            },

            // Color menu.
            {
                0x1750d, 5,
                {0xec, 0x00, 0xbf, 0x15, 0x1c},
                {0xb4, 0xb4, 0xbe, SHOGUN_MENU_EXT, 0x02},
            },

            // Title split.
            { 0x10bc9, 3, {0xea, 0xbf, 0x00}, {0xb4, 0xb4, 0xb4} },
        }
    },

    {
        "Shogun", "890321", 295, 0x5c0b,
        {
            // Newlines.
            { 0x120a9, 1, {0xda}, {0xb0} },

            // Reverse video.
            { 0x10a85, 4, {0x41, 0x40, 0x04, 0x46}, {0xf1, 0x7f, 0x01, 0xb0} },

            // Title clear.
            { 0x10d14, 3, {0xed, 0x7f, 0x00}, {0xb4, 0xb4, 0xb4} },

            // Game start menu.
            {
                0x10d1a, 5,
                {0xec, 0x00, 0x7f, 0x15, 0x66},
                {0xb4, 0xb4, 0xbe, SHOGUN_MENU_EXT, 0x01},
            },

            // Game over menu.
            {
                0x12acc, 4,
                {0xe0, 0x08, 0x15, 0x66},
                {0xb4, 0xbe, SHOGUN_MENU_EXT, 0x23},
            },

            // End of scene menu.
            {
                0x13e6f, 4,
                {0xe0, 0x08, 0x15, 0x66},
                {0xb4, 0xbe, SHOGUN_MENU_EXT, 0x23},
            },

            // Color menu.
            {
                0x176e5, 5,
                {0xec, 0x00, 0xbf, 0x15, 0x66},
                {0xb4, 0xb4, 0xbe, SHOGUN_MENU_EXT, 0x02},
            },

            // Title split.
            { 0x10d0e, 3, {0xea, 0xbf, 0x00}, {0xb4, 0xb4, 0xb4} },
        }
    },

    {
        "Shogun", "890510", 311, 0xe200,
        {
            // Newlines.
            { 0x12625, 1, {0xda}, {0xb0} },

            // Reverse video.
            { 0x117d1, 4, {0x41, 0x43, 0x04, 0x46}, {0xf1, 0x7f, 0x01, 0xb0} },

            // Title clear.
            { 0x10cb0, 3, {0xed, 0x7f, 0x00}, {0xb4, 0xb4, 0xb4} },

            // Game start menu.
            {
                0x10cb6, 5,
                {0xec, 0x00, 0x7f, 0x16, 0x95},
                {0xb4, 0xb4, 0xbe, SHOGUN_MENU_EXT, 0x01},
            },

            // Game over menu.
            {
                0x130ab, 4,
                {0xe0, 0x00, 0x16, 0x95},
                {0xb4, 0xbe, SHOGUN_MENU_EXT, 0x03},
            },

            // End of scene menu.
            {
                0x14499, 9,
                {0xf9, 0x08, 0x16, 0x95, 0x00, 0xc2, 0x00, 0x20, 0x53},
                {0xbe, SHOGUN_MENU_EXT, 0x23, 0x00, 0xc2, 0x00, 0x20, 0x53, 0x02},
            },

            // Color menu.
            {
                0x17d69, 5,
                {0xec, 0x00, 0xbf, 0x16, 0x95},
                {0xb4, 0xb4, 0xbe, SHOGUN_MENU_EXT, 0x02},
            },
        }
    },

    {
        "Shogun", "890706", 322, 0x5c88,
        {
            // Newlines.
            { 0x12771, 1, {0xda}, {0xb0} },

            // Reverse video.
            { 0x11865, 4, {0x41, 0x43, 0x04, 0x46}, {0xf1, 0x7f, 0x01, 0xb0} },

            // Title clear.
            { 0x10d44, 3, {0xed, 0x7f, 0x00}, {0xb4, 0xb4, 0xb4} },

            // Game start menu.
            {
                0x10d4a, 5,
                {0xec, 0x00, 0x7f, 0x16, 0xba},
                {0xb4, 0xb4, 0xbe, SHOGUN_MENU_EXT, 0x01},
            },

            // Game over menu.
            {
                0x131e7, 4,
                {0xe0, 0x00, 0x16, 0xba},
                {0xb4, 0xbe, SHOGUN_MENU_EXT, 0x03},
            },

            // End of scene menu.
            {
                0x145e1, 9,
                {0xf9, 0x08, 0x16, 0xba, 0x00, 0xc2, 0x00, 0x20, 0xa5},
                {0xbe, SHOGUN_MENU_EXT, 0x23, 0x00, 0xc2, 0x00, 0x20, 0xa5, 0x02},
            },

            // Color menu.
            {
                0x17edd, 5,
                {0xec, 0x00, 0xbf, 0x16, 0xba},
                {0xb4, 0xb4, 0xbe, SHOGUN_MENU_EXT, 0x02},
            },
        }
    },

    // There are two V6 hack patches for Journey:
    //
    // 1. The DIAL-GRAPHICS routine calls GRAPHIC with the specified
    // arrow and location; since the same arrows are used for both
    // dials, the offset for the arrows can’t be determined just by the
    // picture number. Instead rewrite the calls to a custom
    // zjourney_dial(), passing 0 for the left arrow, and 1 for the
    // right.
    //
    // 2. When the interpreter number is set to Amiga, Journey draws a
    // box around the entire screen. This is not possible with Glk (at
    // least not in a way that wouldn’t require loads of special-
    // casing); and even worse, Bocfel hacks around some Journey/Glk
    // issues by pretending the screen height is 6, so that Journey
    // won’t expand the upper window to extend across the whole screen,
    // with the side effect that the calculation of the border is
    // broken, causing an apparent hang that is effectively this, but
    // slow, since it’s interpreted:
    //
    // uint16_t val = 1;
    // while (val++ != 0) { }
    //
    // Border drawing is controlled by the global variable BORDER-FLAG,
    // which is only set for Amiga. This patch ensures that BORDER-FLAG
    // is never set, so the game never tries to draw the border.
    {
        "Journey", "890316", 26, 0x27cc,
        {
            // DIAL-GRAPHICS
            {
                0x30095, 6,
                {0xf9, 0x59, 0xcc, 0x00, 0x00, 0x00},
                {0xbe, JOURNEY_DIAL_EXT, 0x9f, 0x97, 0x00, 0xb4}
            },
            {
                0x300a3, 6,
                {0xe0, 0x58, 0xcc, 0x00, 0x00, 0xff},
                {0xbe, JOURNEY_DIAL_EXT, 0x9f, 0x97, 0x01, 0xb0}
            },

            // Amiga hang
            { 0x4b3e, 3, {0x0d, 0xab, 0x01}, {0x0d, 0xab, 0x00} },
        }
    },

    {
        "Journey", "890322", 30, 0x985b,
        {
            // DIAL-GRAPHICS
            {
                0x30189, 6,
                {0xf9, 0x59, 0xcc, 0x00, 0x00, 0x00},
                {0xbe, JOURNEY_DIAL_EXT, 0x9f, 0x97, 0x00, 0xb4}
            },
            {
                0x30197, 6,
                {0xe0, 0x58, 0xcc, 0x00, 0x00, 0xff},
                {0xbe, JOURNEY_DIAL_EXT, 0x9f, 0x97, 0x01, 0xb0}
            },

            // Amiga hang
            { 0x4bea, 3, {0x0d, 0xaa, 0x01}, {0x0d, 0xaa, 0x00} },
        }
    },

    {
        "Journey", "890616", 77, 0xb136,
        {
            // DIAL-GRAPHICS
            {
                0x30771, 6,
                {0xf9, 0x59, 0xf1, 0x00, 0x00, 0x00},
                {0xbe, JOURNEY_DIAL_EXT, 0x9f, 0x97, 0x00, 0xb4}
            },
            {
                0x3077f, 6,
                {0xe0, 0x58, 0xf1, 0x00, 0x00, 0xff},
                {0xbe, JOURNEY_DIAL_EXT, 0x9f, 0x97, 0x01, 0xb0}
            },

            // Amiga hang
            { 0x4dcf, 3, {0x0d, 0xad, 0x01}, {0x0d, 0xad, 0x00} },
        }
    },

    {
        "Journey", "890706", 83, 0xd2b8,
        {
            // DIAL-GRAPHICS
            {
                0x307b9, 6,
                {0xf9, 0x59, 0xef, 0x00, 0x00, 0x00},
                {0xbe, JOURNEY_DIAL_EXT, 0x9f, 0x97, 0x00, 0xb4}
            },
            {
                0x307c7, 6,
                {0xe0, 0x58, 0xef, 0x00, 0x00, 0xff},
                {0xbe, JOURNEY_DIAL_EXT, 0x9f, 0x97, 0x01, 0xb0}
            },

            // Amiga hang
            { 0x4dcf, 3, {0x0d, 0xaf, 0x01}, {0x0d, 0xaf, 0x00} },
        }
    },

    // There are two V6 hack patches for Zork Zero:
    //
    // 1. In Fanucci, Zork Zero displays labels under each card
    // (DISCARD, 1, 2, 3, 4). This is done in a graphics window, though,
    // and Glk doesn’t support text in graphics windows. It would
    // probably be possible to create a new text window just below the
    // graphics window and put this text in there, but since it’s not
    // crucial to the game, as you can either use the mouse to click, or
    // just infer/look up which cards are which, jump over the entire
    // sequence of moving the cursor and writing text. This is necessary
    // because, without it, output such as the jester’s commentary is
    // not visible.
    //
    // 2. Zork Zero only allows the compass to be clicked for certain
    // machine types (interpreter numbers). But the mouse works under
    // Glk regardless (if the Glk implementation supports mouse clicks).
    // This bypasses the mouse check, allowing the compass rose to be
    // clicked no matter which interpreter number is selected.
    {
        "Zork Zero", "881019", 296, 0x8c61,
        {
            // Fanucci.
            { 0x2aa6d, 3, {0xef, 0xaf, 0x02}, {0x8c, 0x00, 0x3e} },

            // Compass rose.
            { 0x1d3cc, 4, {0x41, 0x00, 0x03, 0x62}, {0xb4, 0xb4, 0xb4, 0xb4} },
        },
    },

    {
        "Zork Zero", "890323", 366, 0xc5cd,
        {
            // Fanucci.
            { 0x2964c, 3, {0xef, 0xaf, 0x02}, {0x8c, 0x00, 0x3e} },

            // Compass rose.
            { 0x1baee, 3, {0xa0, 0x00, 0xce}, {0xb4, 0xb4, 0xb4} },
        },
    },

    {
        "Zork Zero", "890602", 383, 0x6f7f,
        {
            // Fanucci.
            { 0x29e87, 3, {0xef, 0xaf, 0x03}, {0x8c, 0x00, 0x3e} },

            // Compass rose.
            { 0x1bfa1, 3, {0xa0, 0x00, 0xce}, {0xb4, 0xb4, 0xb4} },
        },
    },

    {
        "Zork Zero", "890714", 393, 0x791c,
        {
            // Fanucci.
            { 0x2a127, 3, {0xef, 0xaf, 0x03}, {0x8c, 0x00, 0x3e} },

            // Compass rose.
            { 0x1c20d, 3, {0xa0, 0x00, 0xce}, {0xb4, 0xb4, 0xb4} },
        },
    },
};

static bool apply_patch(const Replacement &r)
{
    if (r.addr >= header.static_start &&
        r.addr + r.n < memory_size &&
        std::memcmp(&memory[r.addr], r.in.data(), r.n) == 0) {

        std::memcpy(&memory[r.addr], r.out.data(), r.n);

        return true;
    }

    return false;
}

static void apply_patches(const std::vector<Patch> &patches)
{
    for (const auto &patch : patches) {
        if (std::memcmp(patch.serial, header.serial, sizeof header.serial) == 0 &&
            patch.release == header.release &&
            patch.checksum == header.checksum) {

            for (const auto &replacement : patch.replacements) {
                if (replacement.active()) {
                    apply_patch(replacement);
                }
            }
        }
    }
}

void apply_patches()
{
    apply_patches(base_patches);
}

void apply_v6_patches()
{
    apply_patches(v6_patches);
}

// User patches have the form:
//
// addr count original... replacement...
//
// For example, one of the Stationfall fixes would look like this:
//
// 0xe3fe 3 0x31 0x0c 0x77 0x51 0x77 0x0c
//
// Square brackets and commas are treated as whitespace for the actual
// bytes, so for convenience this could also be written:
//
// 0xe3fe 3 [0x31, 0x0c, 0x77] [0x51, 0x77, 0x0c]
void apply_user_patch(std::string patchstr)
{
    uint32_t addr, count;
    std::vector<uint8_t> in, out;

    std::replace_if(patchstr.begin(), patchstr.end(),
                    [](char c) { return c == '[' || c == ']' || c == ','; },
                    ' ');

    std::istringstream ss(rtrim(patchstr));

    if (!(ss >> std::hex >> addr) ||
        !(ss >> std::dec >> count))
    {
        throw PatchStatus::SyntaxError();
    }

    ss >> std::hex;

    auto read_into = [&count, &ss](std::vector<uint8_t> &vec){
        for (uint32_t i = 0; i < count; i++) {
            unsigned int byte;
            if (!(ss >> byte) || byte > 255) {
                throw PatchStatus::SyntaxError();
            }
            vec.push_back(byte);
        }
    };

    read_into(in);
    read_into(out);

    if (ss.peek() != std::char_traits<char>::eof()) {
        throw PatchStatus::SyntaxError();
    }

    Replacement replacement = {
        addr,
        count,
        in,
        out,
    };

    if (!apply_patch(replacement)) {
        throw PatchStatus::NotFound();
    }
}

// A patch file is somewhat similar to a user configuration file, but
// contains *only* patches. It uses grouping as in the configuration
// file, and the patch syntax is the same, but without the “patch” key.
// For example, a patch file for the first couple of IBM characters in
// Beyond Zork would look like:
//
// # Comments are allowed.
// [57-871221]
// 0xe58b 1 [da] [2b]
// 0xe591 1 [c4] [2d]
void patch_load_file(const std::string &file)
{
    std::ifstream f(file);

    if (!f.is_open()) {
        return;
    }

    parse_grouped_file(f, [&file](const std::string &line, int lineno) {
        try {
            apply_user_patch(line);
        } catch (const PatchStatus::SyntaxError &) {
            std::cerr << file << ":" << lineno << ": patch file syntax error" << std::endl;
        } catch (const PatchStatus::NotFound &) {
            std::cerr << file << ":" << lineno << ": patch file byte sequence not found" << std::endl;
        }
    });
}
