//
//  c64decrunch.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <stdlib.h>
#include <string.h>

#include "common_file_utils.h"

#include "saga.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_game_info.h"

#include "c64decrunch.h"
#include "c64diskimage.h"
#include "c64unpack.h"
#include "detect_game.h"
#include "game_specific.h"
#include "sagadraw.h"
#include "sagagraphics.h"
#include "c64a8scott.h"
#include "c64_small.h"

#include "unp64_interface.h"

/* Registry of known C64 game images, identified by file size + checksum.
   Fields: game ID, file length, 16-bit checksum, container type (D64/T64/US),
   unp64 decompression iterations, unp64 switches, appended filename on disk,
   parameter (iteration for switches or special flag), copy source/dest/size
   for relocating graphics data, and image data offset. */
// clang-format off
static const c64rec c64_registry[] = {
    { BATON_C64,        0x2ab00, 0xc3fc, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 }, // Mysterious Adventures C64 dsk 1
    { TIME_MACHINE_C64, 0x2ab00, 0xc3fc, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 },
    { ARROW1_C64,       0x2ab00, 0xc3fc, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 },
    { ARROW2_C64,       0x2ab00, 0xc3fc, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 },
    { PULSAR7_C64,      0x2ab00, 0xc3fc, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 },
    { CIRCUS_C64,       0x2ab00, 0xc3fc, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 },

    { FEASIBILITY_C64,  0x2ab00, 0x9eaa, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 }, // Mysterious Adventures C64 dsk 2
    { AKYRZ_C64,        0x2ab00, 0x9eaa, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 },
    { PERSEUS_C64,      0x2ab00, 0x9eaa, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 },
    { INDIANS_C64,      0x2ab00, 0x9eaa, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 },
    { WAXWORKS_C64,     0x2ab00, 0x9eaa, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 },
    { BATON_C64,        0x2ab00, 0x9dca, TYPE_D64, 2, NULL, NULL, 0, 0, 0, 0, 0 },

    { BATON_C64,        0x5170,  0xb240, TYPE_T64, 2, NULL, NULL, 0, 0, 0, 0, 0 }, // The Golden Baton C64, T64
    { BATON_C64,        0x2ab00, 0xbfbf, TYPE_D64, 2, NULL, NULL, 0, 0, 0, 0, 0 }, // Mysterious Adventures C64 dsk 1 alt
    { FEASIBILITY_C64,  0x2ab00, 0x9c18, TYPE_D64, 2, NULL, NULL, 0, 0, 0, 0, 0 }, // Mysterious Adventures C64 dsk 2 alt
    { TIME_MACHINE_C64, 0x5032,  0x5635, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // The Time Machine C64
    { TIME_MACHINE_C64, 0x2ab00, 0xed21, TYPE_D64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // The Time Machine D64
    { TIME_MACHINE_C64, 0x2ab00, 0x6ed7, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 }, // The Time Machine D64, Supersoft
    { TIME_MACHINE_C64, 0x2ab00, 0x446e, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 }, // The Time Machine D64, Cream Crackers
    { TIME_MACHINE_C64, 0x2ab00, 0x4c9e, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 }, // The Time Machine D64, Megabyte
    { TIME_MACHINE_C64, 0x2ab00, 0x37c1, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 }, // The Time Machine D64, BASIC loader and TIME2
    { ARROW1_C64,       0x5b46,  0x92db, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Arrow of Death part 1 C64
    { ARROW1_C64,       0x2ab00, 0xe71d, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 }, // Arrow of Death part 1 C64 D64
    { ARROW1_C64,       0x2ab00, 0x7687, TYPE_D64, 2, NULL, NULL, 0, 0, 0, 0, 0 }, // Arrow of Death part 1 C64 D64 alt
    { ARROW2_C64,       0x5fe2,  0xe14f, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Arrow of Death part 2 C64
    { PULSAR7_C64,      0x46bf,  0x1679, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Escape from Pulsar 7 C64
    { CIRCUS_C64,       0x4269,  0xa449, TYPE_T64, 2, NULL, NULL, 0, 0, 0, 0, 0 }, // Circus C64
    { FEASIBILITY_C64,  0x5a7b,  0x0f48, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Feasibility Experiment C64
    { AKYRZ_C64,        0x2ab00, 0x6cca, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 }, // The Wizard of Akyrz C64
    { AKYRZ_C64,        0x4be1,  0x5a00, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // The Wizard of Akyrz C64, T64
    { PERSEUS_C64,      0x502b,  0x913b, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Perseus and Andromeda C64
    { PERSEUS_C64,      0x2ab00, 0xdc5e, TYPE_D64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Perseus and Andromeda C64 D64
    { INDIANS_C64,      0x4f9f,  0xe6c8, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Ten Little Indians C64
    { WAXWORKS_C64,     0x4a11,  0xa37a, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Waxworks C64
    { WAXWORKS_C64,     0x2ab00, 0x0a78, TYPE_D64, 3, NULL, NULL, 0, 0, 0, 0, 0 }, // Waxworks C64 D64

    { ADVENTURELAND_C64, 0x6a10,  0x1910, TYPE_T64, 1, NULL, NULL,    0,        0, 0, 0, 0 },      // Adventureland C64 (T64) CruelCrunch v2.2
    { ADVENTURELAND_C64, 0x6a10,  0x1b10, TYPE_T64, 1, NULL, NULL,    0,        0, 0, 0, 0 },      // Adventureland C64 (T64) alt CruelCrunch v2.2
    { ADVENTURELAND_C64, 0x2ab00, 0x6638, TYPE_D64, 1, NULL, NULL,    0,        0, 0, 0, 0 },      // Adventureland C64 (D64) CruelCrunch v2.2
    { ADVENTURELAND_C64, 0x2adab, 0x751f, TYPE_D64, 0, NULL, NULL,    0,        0, 0, 0, 0 },      // Adventureland C64 (D64) alt
    { ADVENTURELAND_C64, 0x2adab, 0x64a4, TYPE_D64, 0, NULL, "SAG1PIC", -0xa53, 0, 0, 0, 0x65af }, // Adventureland C64 (D64) alt 2
    { ADVENTURELAND_C64, 0x2adab, 0x8847, TYPE_D64, 0, NULL, NULL,    0,        0, 0, 0, 0 }, // Adventureland C64 (D64) alt 3
    { ADVENTURELAND_US, 0x2ab00, 0x78fa, TYPE_D64, 0, NULL, NULL,    0,        0, 0, 0, 0 }, // Scott Adams Adventure Pack (1983)(Adventure International)

    { PIRATE_US, 0x2adab, 0x04c5, TYPE_US, 0, NULL, "PIRATE", 0, 0, 0, 0, 0x6d30 }, // Pirate Adventure S.A.G.A version
    { PIRATE_US, 0x2adab, 0xbea0, TYPE_US, 0, NULL, "PIRATE", 0, 0, 0, 0, 0x6d30 }, // Pirate Adventure S.A.G.A version (c64.com)
    { PIRATE_US, 0x2ab00, 0xac87, TYPE_US, 1, NULL, "PIRATE ADVENTURE", 0, 0, 0, 0, 0x6d30 }, // Pirate Adventure S.A.G.A version
    { PIRATE_US, 0x57c6, 0x82ab, TYPE_T64_US, 1, NULL, "PIRATE ADVENTURE", 0, 0, 0, 0, 0x6d30 }, // Pirate Adventure S.A.G.A version

    { SECRET_MISSION_C64, 0x88be, 0xa122, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Secret Mission  C64 (T64) Section8 Packer
    { SECRET_MISSION_C64, 0x2ab00, 0x04d6, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, -0x1bff }, // Secret Mission  C64 (D64)
    { SECRET_MISSION_C64, 0x2ab00, 0x167b, TYPE_D64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Secret Mission  C64 (D64) c64.com, Section8 packer
    { SECRET_MISSION_C64, 0x2adab, 0x3ca3, TYPE_D64, 0, NULL, "SAG3PIC", -0x83a, 0, 0, 0, 0x67c8 }, // Secret Mission  C64 (D64)

    { VOODOO_CASTLE_US, 0x2adab, 0xcb2b, TYPE_US, 0, NULL, "SAGA1", 0, 0, 0, 0, 0x6c30 }, // Voodoo Castle S.A.G.A version
    { VOODOO_CASTLE_US, 0x2ab00, 0x8969, TYPE_US, 1, NULL, "VOODOO CASTLE", 0, 0, 0, 0, 0x6c30 }, // Voodoo Castle S.A.G.A version, packed
    { VOODOO_CASTLE_US, 0x2ab00, 0x2682, TYPE_US, 1, NULL, "VOODOO CASTLE", 0, 0, 0, 0, 0x6c30 }, // Voodoo Castle S.A.G.A version, packed 2
    { VOODOO_CASTLE_US, 0x2ab00, 0xac79, TYPE_US, 0, NULL, "VOODOO CASTLE 2", 0, 0, 0, 0, 0x6c30 }, // Voodoo Castle S.A.G.A version "Cracked by Toko"

    { VOODOO_CASTLE_US, 0x5f91, 0x04fc, TYPE_T64_US, 1, NULL, "VOODOO CASTLE    ", 0, 0, 0, 0, 0x6c30 },

    { STRANGE_ODYSSEY_US, 0x2ab00, 0x12d2, TYPE_US, 1, NULL, "STRANGE ODYSSEY", 0, 0, 0, 0, 0 }, // Strange Odyssey S.A.G.A version (c64.com), PUCrunch; DB starts at offset 0

    { COUNT_US, 0x2ab00, 0x2827, TYPE_US, 1, "-d0x7f50", "THE COUNT", 1, 0, 0, 0, 0x3800 }, // The Count S.A.G.A version (c64.com); IRQ packer, depack-done $7f50; DB preamble at $4000 (text only)
    { VOODOO_CASTLE_US, 0x2ab00, 0x7822, TYPE_US, 1, "-e0x840 -d0x7f50", "VOODOO CASTLE", 1, 0, 0, 0, 0x3800 }, // Voodoo Castle S.A.G.A version (c64.com); REM-before-SYS, depack-done $7f50; DB preamble at $4000 (text only)

    { CLAYMORGUE_C64, 0x6ff7,  0xe4ed, TYPE_T64, 3, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (T64), MasterCompressor / Relax
                                                                                             // -> ECA Compacker -> MegaByte Cruncher v1.x Missing 17 pictures
    { CLAYMORGUE_C64, 0x912f,  0xa69f, TYPE_T64, 1, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (T64) alt, MegaByte Cruncher
                                                                                             // v1.x Missing 17 pictures
    { CLAYMORGUE_C64, 0x2ab00, 0xf70c, TYPE_D64, 1, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), Cracked by ABC, missing 17 pictures
    { CLAYMORGUE_C64, 0xc0dd,  0x3701, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, -0x7fe },// Sorcerer Of Claymorgue Castle C64 (T64) alt 2, Trilogic Expert v2.7
    { CLAYMORGUE_C64, 0xbc5f,  0x492c, TYPE_T64, 1, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (T64) alt 3, , Section8 Packer
    { CLAYMORGUE_C64, 0x2ab00, 0xfd67, TYPE_D64, 1, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), Section8 Packer
    { CLAYMORGUE_C64, 0x2ab00, 0x7ece, TYPE_D64, 1, NULL, NULL, 0, 0, 0, 0, -0x7fe }, // Sorcerer Of Claymorgue Castle C64 (D64), Trilogic Expert v2.7


    { CLAYMORGUE_US, 0x2adab, 0x1fac, TYPE_US, 0, NULL, "SAGA.DB", 0, 0, 0, 0, 0 },
    { CLAYMORGUE_US, 0x2ab00, 0xa957, TYPE_US, 0, NULL, "SAGA.DB", 0, 0, 0, 0, 0 }, // Sorcerer Of Claymorgue Castle US side A (D64)
    { CLAYMORGUE_US, 0x2ab00, 0xfbb6, TYPE_US, 0, NULL, "SAGA.DB", 0, 0, 0, 0, 0 },
    { CLAYMORGUE_US, 0x2ab00, 0x0b54, TYPE_US, 0, NULL, "SAGA.DB", 0, 0, 0, 0, 0 },

    { CLAYMORGUE_C64, 0x2ab00, 0x2c7a, TYPE_D64, 1, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), Dr. Bakterius & Mr. L, Section8 Packer
    { CLAYMORGUE_C64, 0x2ab00, 0xc349, TYPE_D64, 2, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), All Stars, Time Cruncher v3.x -> MC-Cracken Compressor
    { CLAYMORGUE_C64, 0x2ab00, 0x7ea6, TYPE_D64, 1, NULL, NULL, 0, 0, 0, 0, -0x7fe }, // Sorcerer Of Claymorgue Castle C64 (D64), Trilogic Expert v2.7
    { CLAYMORGUE_C64, 0x2ab00, 0x2e5d, TYPE_D64, 3, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), Cracked by ABC, MasterCompressor / Relax -> ECA Compacker -> MegaByte Cruncher v1.x, missing 17 pictures
    { CLAYMORGUE_C64, 0x2ab00, 0x188f, TYPE_D64, 0, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), NICS, not packed, loads at $0801
    { CLAYMORGUE_C64, 0x2ab00, 0x95c1, TYPE_D64, 1, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), Doctor Mabuse, Section8 Packer
    { CLAYMORGUE_C64, 0x2ab00, 0x5592, TYPE_D64, 0, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), Danish Crackers, not packed. 14 damaged bytes, see RepairDanishCrackersClaymorgue()
    { CLAYMORGUE_C64, 0x2ab00, 0x10a5, TYPE_D64, 2, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), the Danish Crackers release behind a "773 - 695" menu, menu mover -> MegaByte Cruncher
    { CLAYMORGUE_C64, 0x2ab00, 0xf8c2, TYPE_D64, 1, NULL, NULL, 0, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), Slam compilation with English Civil War: the Danish Crackers release in a file without a directory entry, Super Compressor
    { CLAYMORGUE_C64, 0x2ab00, 0x6dbd, TYPE_D64, 0, NULL, "CC3", -2, 0x855, 0x7352, 0x20, 0 }, // Sorcerer Of Claymorgue Castle C64 (D64), The Phantoms / SCS, not packed, in two files loading at $1000 and $8000
    { CLAYMORGUE_US, 0x2ab00, 0x0975, TYPE_US, 0, NULL, "SAGA.DB", 0, 0, 0, 0, 0 }, // Sorcerer Of Claymorgue Castle US (D64), Section 8
    { CLAYMORGUE_US, 0x2ab00, 0x3bd6, TYPE_US, 0, NULL, "SAGA.DB", 0, 0, 0, 0, 0 }, // Sorcerer Of Claymorgue Castle US (D64), TIF. Picture R13006 is damaged
    { CLAYMORGUE_US, 0x2ab00, 0x3f2b, TYPE_US, 0, NULL, "SAGA.DB", 0, 0, 0, 0, 0 }, // Sorcerer Of Claymorgue Castle US (D64), e=mc2, disk A
    { CLAYMORGUE_US, 0x30300, 0x8ef3, TYPE_US, 0, NULL, "SAGA.DB", 0, 0, 0, 0, 0 }, // Sorcerer Of Claymorgue Castle US (D64, 40 tracks), OLICS
    { CLAYMORGUE_US, 0x2ab00, 0x4224, TYPE_US, 1, NULL, "SAGA 13", 0, 0, 0, 0, 0 }, // Sorcerer Of Claymorgue Castle US (D64), Florasoft, Section8 Packer. Interpreter and database in one file
    { HULK_C64,      0x2ab00, 0xcdd8, TYPE_D64, 0, NULL, NULL, 0, 0x1806, 0xb801, 0x307, 0 },  // Questprobe 1 - The Hulk C64 (D64)
    { HULK_C64,      0x2ab00, 0x3d57, TYPE_D64, 1, NULL, NULL, 0, 0x1806, 0xb801, 0x307, 0 },  // Questprobe 1 - The Hulk C64 (D64) www.c64hq.com crack 1
    { HULK_C64,      0x2ab00, 0x910b, TYPE_D64, 1, NULL, NULL, 0, 0x1806, 0xb801, 0x307, 0 },  // Questprobe 1 - The Hulk C64 (D64) www.c64hq.com crack 2 (TCS-Crunch)
    { HULK_C64,      0x2ab00, 0xaadc, TYPE_D64, 2, NULL, NULL, 0, 0x1806, 0xb801, 0x307, 0 },  // Questprobe 1 - The Hulk C64 (D64) www.c64hq.com crack 3 (two compression layers)
    { HULK_C64,      0x2ab00, 0x01c9, TYPE_D64, 1, NULL, NULL, 0, 0x1806, 0xb801, 0x307, 0 },  // Questprobe 1 - The Hulk C64 (D64) www.c64hq.com crack 4 (Pock&CPU)
    { HULK_C64,      0x2ab00, 0x0332, TYPE_D64, 1, NULL, NULL, 0, 0x1806, 0xb801, 0x307, 0 },  // Questprobe 1 - The Hulk C64 (D64) www.c64hq.com crack 5 (Killersoft)
    { HULK_C64,      0x8534,  0x623a, TYPE_T64, 2, NULL, NULL, 0, 0x1806, 0xb801, 0x307, 0 },  // Questprobe 1 - The Hulk C64 (D64)
    { HULK_US,       0x2ab00,  0x2918, TYPE_US, 0, NULL, "SHULK.DB", 0, 0, 0, 0, 0 },  // Questprobe 1 - The Hulk C64 US (D64)
    { HULK_US,       0x2adab,  0x5eeb, TYPE_US, 0, NULL, "SHULK.DB", 0, 0, 0, 0, 0 },  // Questprobe 1 - The Hulk C64 US (D64) [cbm_1984]
    { HULK_US,       0x2ab00,  0xdcfe, TYPE_US, 0, NULL, "SHULK.DB", 0, 0, 0, 0, 0 },  // Questprobe 1 - The Hulk C64 US (D64) [gvp_1986]
    { HULK_US,       0x2ab00,  0x14e7, TYPE_US, 0, NULL, "SHULK.DB", 0, 0, 0, 0, 0 },  // Questprobe 1 - The Hulk C64 US (D64) [sharedata_1987]
    { SPIDERMAN_C64, 0x2ab00, 0xde56, TYPE_D64, 0, NULL, NULL, 0, 0x1801, 0xa801, 0x2000, 0 }, // Spiderman C64 (D64)
    { SPIDERMAN_C64, 0x2ab00, 0x2736, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0, 0 }, // Spiderman C64 (D64) alt
    { SPIDERMAN_C64, 0x2ab00, 0x490a, TYPE_D64, 1, NULL, NULL, 0, 0, 0, 0, -0x7ff }, // Spiderman C64 (D64) alt 2
    { SPIDERMAN_C64, 0x2ab00, 0xc4c4, TYPE_D64, 0, NULL, NULL, 0, 0x1801, 0xa801, 0x2000, 0 }, // Spiderman C64 (D64) (Mr. Z)
    { SPIDERMAN_C64, 0x08e72, 0xb2f4, TYPE_T64, 3, NULL, NULL, 0, 0, 0, 0, 0 }, // Spiderman C64 (T64) MasterCompressor / Relax -> ECA Compacker -> Section8 Packer

    { SAVAGE_ISLAND_C64,  0x2ab00, 0x8801, TYPE_D64, 1, "-f86 -d0x1793", "SAVAGEISLAND1+",   1, 0, 0, 0, 0 }, // Savage Island part 1 C64 (D64)
    { SAVAGE_ISLAND2_C64, 0x2ab00, 0x8801, TYPE_D64, 1, "-f86 -d0x178b", "SAVAGEISLAND2+",   1, 0, 0, 0, 0 }, // Savage Island part 2 C64 (D64)
    { SAVAGE_ISLAND_C64,  0x2ab00, 0xc361, TYPE_D64, 1, "-f86 -d0x1793", "SAVAGE ISLAND P1", 1, 0, 0, 0, 0 }, // Savage Island part 1 C64 (D64) alt
    { SAVAGE_ISLAND2_C64, 0x2ab00, 0xc361, TYPE_D64, 1, NULL,            "SAVAGE ISLAND P2", 0, 0, 0, 0, 0 }, // Savage Island part 2  C64 (D64) alt
    { SAVAGE_ISLAND_US,  0x2ab00, 0x1a2e, TYPE_US, 1, NULL, "SAVAGE ISLAND P1", 0, 0, 0, 0, 0 }, // Savage Island part 1 C64 (D64) US version
    { SAVAGE_ISLAND_2_US,  0x2ab00, 0xeea6, TYPE_US, 1, NULL, "SAVAGE ISLAND P2", 0, 0, 0, 0, 0 }, // Savage Island part 2 C64 (D64) US version
    { SAVAGE_ISLAND_US,  0x2812, 0xd13a, TYPE_T64_US, 1, NULL, "SAVAGE ISLAND P1", 0, 0, 0, 0, 0 }, // Savage Island part 1 C64 (T64) US version
    { SAVAGE_ISLAND_2_US,  0x2825, 0xa5b1, TYPE_T64_US, 1, NULL, "SAVAGE ISLAND P2", 0, 0, 0, 0, 0 }, // Savage Island part 2 C64 (T64) US version

    { ROBIN_OF_SHERWOOD_C64, 0x2ab00, 0xcf9e, TYPE_D64, 1, NULL, NULL, 0, 0x1802, 0xbd27, 0x1f6c, 0 }, // Robin Of Sherwood D64 * unknown packer
    { ROBIN_OF_SHERWOOD_C64, 0x2ab00, 0xc0c7, TYPE_D64, 1, NULL, NULL, 0, 0xd7fb, 0xbd20, 0x1f6c, 0 }, // Robin Of Sherwood D64 * PUCrunch
    { ROBIN_OF_SHERWOOD_C64, 0x2ab00, 0xc6e6, TYPE_D64, 1, NULL, NULL, 0, 0x9702, 0x9627, 0x1f6c, 0 }, // Robin Of Sherwood D64 * unknown packer
    { ROBIN_OF_SHERWOOD_C64, 0xb2ef,  0x7c44, TYPE_T64, 1, NULL, NULL, 0, 0x9702, 0x9627, 0x1f6c, 0 }, // Robin Of Sherwood C64 (T64) * TCS Cruncher v2.0
    { ROBIN_OF_SHERWOOD_C64, 0xb690,  0x7b61, TYPE_T64, 1, NULL, NULL, 0, 0x9702, 0x9627, 0x1f6c, 0 }, // Robin Of Sherwood C64 (T64) alt * TCS Cruncher v2.0
    { ROBIN_OF_SHERWOOD_C64, 0x8db6,  0x7853, TYPE_T64, 1, NULL, NULL, 0, 0xd7fb, 0xbd20, 0x1f6c, 0 }, // Robin Of Sherwood T64 alt 2 * PUCrunch

    { GREMLINS_C64,        0xdd94,    0x25a8, TYPE_T64, 1, NULL,       NULL, 0, 0, 0, 0, 0 },     // Gremlins C64 (T64) version * Action Replay v4.x
    { GREMLINS_C64,        0x2ab00,   0xc402, TYPE_D64, 0, NULL, "G1", -0x8D, 0, 0, 0, 0 }, // Gremlins C64 (D64) version
    { GREMLINS_C64,        0x2ab00,   0x3ccf, TYPE_D64, 0, NULL, "G1", -0x8D, 0, 0, 0, 0 }, // Gremlins C64 (D64) version 2
    { GREMLINS_C64,        0x2ab00,   0xabf8, TYPE_D64, 2, "-e0x1255", NULL, 2, 0, 0, 0, 0 },   // Gremlins C64 (D64) version alt * ByteBoiler, Exomizer
    { GREMLINS_C64,        0x2ab00,   0xa265, TYPE_D64, 2, "-e0x1255", NULL, 2, 0, 0, 0, 0 },   // Gremlins C64 (D64)  version alt 2 * ByteBoiler, Exomizer
    { GREMLINS_C64,        0x2ab00,   0xa1dc, TYPE_D64, 2, "-e0x1255", NULL, 2, 0, 0, 0, 0 }, // Gremlins C64 (D64) version alt 3  * ByteBoiler, Exomizer
    { GREMLINS_SPANISH_C64,0x2ab00,   0x2ad9, TYPE_D64, 1, NULL, NULL, 0, 0xd002, 0xbf8a, 0x1f00, 0 }, // Gremlins Spanish C64 (D64) PUCrunch Generic Hack
    { GREMLINS_C64,        0x2ab00,   0x4be2, TYPE_D64, 0, NULL, "G1", -0x8D, 0, 0, 0, 0 }, // Not compressed
    { GREMLINS_C64,        0x2ab00,   0x5c7a, TYPE_D64, 3, "-e0x2ec2", NULL, 2, 0xc801, 0xd774, 0xbeb, 0 }, // CruelCrunch v2.5 -> AbuzeCrunch
    { GREMLINS_C64,        0x2ab00,   0x331a, TYPE_D64, 2, NULL, NULL, 0, 0xd80f, 0xc782, 0x1f00, 0 }, // Gremlins C64 (D64) version alt 5 * Beta Dynamic Compressor v3.x/FX * Mr.Cross v2.x
    { GREMLINS_GERMAN_C64, 0xc003,    0x558c, TYPE_T64, 1, NULL,       NULL, 0, 0xd801, 0xc6c0, 0x1f00, 0 }, // German Gremlins C64 (T64) * TBC Multicompactor v2.x
    { GREMLINS_GERMAN_C64, 0x2ab00,   0x6729, TYPE_D64, 2, NULL,       NULL, 0, 0xdc02, 0xcac1, 0x1f00, 0 }, // German Gremlins C64 (D64) version * Exomizer

    { SUPERGRAN_C64, 0x726f, 0x0901, TYPE_T64, 1, NULL, NULL, 0, 0xd802, 0xc623, 0x1f00, 0 }, // Super Gran C64 (T64) PUCrunch Generic Hack
    { SUPERGRAN_C64, 0x2ab00, 0x538c, TYPE_D64, 1, NULL, NULL, 0, 0xd802, 0xc623, 0x1f00, 0 }, // Super Gran C64 (D64) PUCrunch Generic Hack

    { SEAS_OF_BLOOD_C64, 0xa209,  0xf115, TYPE_T64, 6, "-e0x1000", NULL, 3, 0xd802, 0xb07c, 0x1fff, 0 }, // Seas of Blood C64 (T64) MasterCompressor / Relax -> ECA
                                                                                                         // Compacker -> Unknown -> MasterCompressor / Relax -> ECA
                                                                                                         // Compacker -> CCS Packer
    { SEAS_OF_BLOOD_C64, 0x2ab00, 0x5c1d, TYPE_D64, 1, NULL, NULL, 0, 0xd802, 0xb07c, 0x1fff, 0 }, // Seas of Blood C64 (D64) CCS Packer
    { SEAS_OF_BLOOD_C64, 0x2ab00, 0xe308, TYPE_D64, 1, NULL, NULL, 0, 0xd802, 0xb07c, 0x1fff, 0 }, // Seas of Blood C64 (D64) alt CCS Packer
    { SEAS_OF_BLOOD_C64, 0x2ab00, 0x3df2, TYPE_D64, 6, "-e0x1000", NULL, 3, 0xd802, 0xb07c, 0x1fff, 0 }, // Seas of Blood C64 (D64) alt CCS Packer

    { UNKNOWN_GAME, 0, 0, UNKNOWN_FILE_TYPE, 0, NULL, NULL, 0, 0, 0, 0, 0 }
};
// clang-format off

static GameIDType ProcessC64(uint8_t **sf, size_t *extent, c64rec entry);

/* Savage Island stores picture data in separate files on the D64 image.
   These buffers hold the extracted picture files so they can be appended
   to the main game data after decompression. */
uint8_t *save_island_appendix_1 = NULL;
size_t save_island_appendix_1_length = 0;
uint8_t *save_island_appendix_2 = NULL;
size_t save_island_appendix_2_length = 0;

/* Prompt the user to choose between Savage Island part 1 and part 2,
   which share a single D64 disk image. Extracts the selected game file
   and its associated picture files, then decompresses. */
static GameIDType savage_island_menu(uint8_t **sf, size_t *extent, int recindex)
{
    static const char *titles[] = { "Savage Island part I", "Savage Island part II" };
    int result = SelectGameFromMenu(
        "This disk image contains two games. Select one.", titles, 2) + 1;

    recindex += result - 1;

    c64rec rec = c64_registry[recindex];
    size_t length;
    uint8_t *file = di_get_file_named(*sf, *extent, &length, rec.appendfile);

    if (file != NULL) {
        if (rec.chk == 0xc361) {
            if (rec.switches != NULL) {
                save_island_appendix_1 = di_get_file_named(
                                                        *sf, *extent, &save_island_appendix_1_length, "SI1PC1");
                save_island_appendix_2 = di_get_file_named(
                                                        *sf, *extent, &save_island_appendix_2_length, "SI1PC2");
            } else {
                save_island_appendix_1 = di_get_file_named(
                                                        *sf, *extent, &save_island_appendix_1_length, "SI2PIC");
            }
        }
        free(*sf);
        *sf = file;
        *extent = length;
        if (save_island_appendix_1_length > 2)
            save_island_appendix_1_length -= 2;
        if (save_island_appendix_2_length > 2)
            save_island_appendix_2_length -= 2;
        return ProcessC64(sf, extent, rec);
    } else {
        fprintf(stderr, "SCOTT: DetectC64() Failed loading file %s\n", rec.appendfile);
        return UNKNOWN_GAME;
    }
}

/* Append the Savage Island picture file(s) to the decompressed game data
   at a fixed offset (0x6202). Frees the appendix buffers after copying. */
static void appendSIfiles(uint8_t **sf, size_t *extent)
{
    uint8_t megabuf[0xffff];
    memcpy(megabuf, *sf, *extent);
    free(*sf);
    int offset = 0x6202;

    if (save_island_appendix_1) {
        memcpy(megabuf + offset, save_island_appendix_1 + 2,
            save_island_appendix_1_length);
        free(save_island_appendix_1);
    }
    if (save_island_appendix_2) {
        memcpy(megabuf + offset + save_island_appendix_1_length,
            save_island_appendix_2 + 2, save_island_appendix_2_length);
        free(save_island_appendix_2);
    }
    *extent = offset + save_island_appendix_1_length + save_island_appendix_2_length;
    *sf = MemAlloc(*extent);
    memcpy(*sf, megabuf, *extent);
}

/* Ask which of the games on a compilation disk to play, extract that
   game's file from the D64 image and process it. titles[] and filenames[]
   run in parallel, and the games' records follow each other in
   c64_registry, starting at first_rec. */
static GameIDType RunDiskMenu(uint8_t **sf, size_t *extent, const char *intro,
    const char **titles, const char **filenames, int count, int first_rec)
{
    int choice = SelectGameFromMenu(intro, titles, count);
    const char *filename = filenames[choice];

    size_t length;
    uint8_t *file = di_get_file_named(*sf, *extent, &length, filename);

    if (file == NULL) {
        fprintf(stderr, "SCOTT: DetectC64() Failed loading file %s\n", filename);
        return UNKNOWN_GAME;
    }

    free(*sf);
    *sf = file;
    *extent = length;
    return ProcessC64(sf, extent, c64_registry[first_rec + choice]);
}

/* Prompt the user to choose from the six Mysterious Adventures games on
   C64 compilation disk 1. Their records are the first six of c64_registry. */
static GameIDType mysterious_menu(uint8_t **sf, size_t *extent)
{
    static const char *titles[] = { "The Golden Baton", "The Time Machine",
        "Arrow of Death part 1", "Arrow of Death part 2",
        "Escape from Pulsar 7", "Circus" };
    static const char *filenames[] = { "BATON", "TIME MACHINE", "ARROW I",
        "ARROW II", "PULSAR 7", "CIRCUS" };

    return RunDiskMenu(sf, extent,
        "This disk image contains six games. Select one.", titles, filenames,
        6, 0);
}

/* Prompt the user to choose from the five Mysterious Adventures games on
   C64 compilation disk 2. Their records follow those of disk 1. */
static GameIDType mysterious_menu2(uint8_t **sf, size_t *extent)
{
    static const char *titles[] = { "Feasibility Experiment", "The Wizard of Akyrz",
        "Perseus and Andromeda", "Ten Little Indians", "Waxworks" };
    static const char *filenames[] = { "EXPERIMENT", "WIZARD OF AKYRZ",
        "PERSEUS", "INDIANS", "WAXWORKS" };

    return RunDiskMenu(sf, extent,
        "This disk image contains five games. Select one.", titles, filenames,
        5, 6);
}

/* Prompt the user to choose from the 12 Scott Adams games on the
   "Adventure Pack" D64 compilation disk. Each game is stored as a
   separate ADVn.OBJ file on the disk image. */
static GameIDType adventure_pack_menu(uint8_t **sf, size_t *extent)
{
    static const char *titles[] = { "Adventureland", "Pirate Adventure",
        "Mission Impossible", "Voodoo Castle", "The Count", "Strange Odyssey",
        "Fun House", "Pyramid of Doom", "Ghost Town", "Savage Island",
        "Savage Island Part 2", "The Golden Voyage" };
    int choice = SelectGameFromMenu(
        "This disk image contains 12 games. Select one.", titles, 12) + 1;

    char filename[100];
    snprintf(filename, 100, "ADV%d.OBJ", choice);

    size_t length;
    uint8_t *file = di_get_file_named(*sf, *extent, &length, filename);

    if (file != NULL) {
        GameIDType result = LoadBinaryDatabase(file, length, *Game, 0);
        if (result) {
            CurrentSys = SYS_C64;
            free(*sf);
            *sf = MemAlloc(length);
            memcpy(*sf, file, length);
            free(file);
            *extent = length;
            return CurrentGame;
        }
        free(file);
    }
    fprintf(stderr, "SCOTT: DetectC64() Failed loading file %s\n", filename);
    return UNKNOWN_GAME;
}

/* Relocate a block of bytes within a buffer, growing it if necessary.
   Allocates a new buffer of at least max(dest + bytestomove, datasize),
   copies the original data, then overwrites at dest with bytes from source.
   Used to rearrange graphics data after decompression. */
static size_t CopyData(size_t dest, size_t source, uint8_t **data, size_t datasize,
    size_t bytestomove)
{
    if (source > datasize || *data == NULL)
        return 0;

    size_t newsize = MAX(dest + bytestomove, datasize);
    /* Zero-filled, as dest may lie beyond the end of the original data,
       and the gap in between must not be left as random heap contents.
       (SagaGraphicsSetup() looks for zeroed image headers there in order to
       detect the Claymorgue release with missing pictures.) */
    uint8_t *megabuf = MemCalloc(newsize);
    memcpy(megabuf, *data, datasize);
    memcpy(megabuf + dest, *data + source, bytestomove);
    free(*data);
    *data = megabuf;
    return newsize;
}

/* Scan a D64 disk image for S.A.G.A.-format picture files and load them
   into the USImages linked list. Picture filenames encode the image index
   and usage type (room, room+object, inventory object) in fixed positions. */
static void LoadC64USImages(uint8_t *data, size_t length)
{
    int numfiles;

    DiskImage *d64 = di_create_from_data(data, length);
    if (d64) {
        char **filenames = di_get_all_file_names(d64, &numfiles);
        if (filenames) {
            /* Keep the names of the pictures, at the front of the array */
            int imgindex = 0;
            char **imagefiles = filenames;
            for (int i = 0; i < numfiles; i++) {
                if (IsSagaImage(filenames[i])) {
                    imagefiles[imgindex++] = filenames[i];
                } else {
                    free(filenames[i]);
                }
            }

            if (imgindex) {
                USImages = NewImage();
                USImage *image = USImages;
                for (int i = 0; i < imgindex; i++) {
                    const char *shortname = imagefiles[i];
                    /* The index and usage are read from positions 3 to 6 */
                    if (strlen(shortname) >= 6)
                        image->imagedata = di_read_file_named(d64, shortname, &image->datasize);
                    if (image->imagedata) {
                        image->systype = SYS_C64;
                        /* Parse 3-digit image index from filename positions 3–5 */
                        image->index = shortname[5] - '0' + (shortname[4] - '0') * 10 + (shortname[3] - '0') * 100;
                        /* Filename convention: 'R' prefix = room, char 6 'R' = room+object, 'I' = inventory */
                        if (shortname[0] == 'R') {
                            image->usage = IMG_ROOM;
                        } else if (shortname[6] == 'R') {
                            image->usage = IMG_ROOM_OBJ;
                        } else if (shortname[6] == 'I') {
                            image->usage = IMG_INV_OBJ;
                        }

                        image = AppendImage(image);
                    }
                    free(imagefiles[i]);
                }

                if (USImages->imagedata == NULL && USImages->next == NULL) {
                    free(USImages);
                    USImages = NULL;
                }
            }
            free(filenames);
        }
        di_free(d64);
    }
}

/* The interpreter of the US Hulk is not a file of the disk but a chain of
   sectors that are stored back to front and exclusive-ored with $55. This
   reads the 254 bytes of the one at offset and gives the offset of the one
   that follows it, or 0 if there is none. */
static size_t ReadHiddenSector(const uint8_t *disk, size_t length, size_t offset, uint8_t *out)
{
    static const uint8_t sectors[] = { 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
        19, 19, 19, 19, 19, 19, 19, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17 };
    const uint8_t *sector = disk + offset;

    for (int i = 0; i < 254; i++)
        out[i] = sector[253 - i] ^ 0x55;

    int track = sector[255] ^ 0x55, next = sector[254] ^ 0x55;
    if (track < 1 || track > (int)sizeof sectors || next >= sectors[track - 1])
        return 0;
    size_t result = 0;
    for (int i = 1; i < track; i++)
        result += sectors[i - 1];
    result = (result + next) * 256;
    return (result + 256 <= length) ? result : 0;
}

/* That interpreter has the same table of close-ups as the Apple II one (see
   SetA2Closeups() in apple2detect.c), just before its "<HIT RETURN>" prompt
   in inverse video. Its picture slots are the numbers of the room picture
   files. The pictures must have been read. */
static void SetC64USCloseups(const uint8_t *disk, size_t length)
{
    static const uint8_t prompt[] = { 0x0d, 0xbc, 0xc8, 0xc9, 0xd4, 0xa0, 0xd2, 0xc5, 0xd4, 0xd5, 0xd2, 0xce, 0xbe, 0x0d };
    enum { ENTRIES = 16, TABLE = 1 + 3 * ENTRIES, DATA = 254 };
    uint8_t buf[2 * DATA];

    for (size_t offset = 0; offset + 256 <= length; offset += 256) {
        size_t size = DATA;
        size_t next = ReadHiddenSector(disk, length, offset, buf);
        if (next != 0) {
            ReadHiddenSector(disk, length, next, buf + DATA);
            size += DATA;
        }
        /* A prompt that starts in the second sector is found again when
           that one comes first, unless the table is split between them. */
        for (size_t i = TABLE; i + sizeof prompt <= size && i < DATA + TABLE; i++) {
            if (memcmp(buf + i, prompt, sizeof prompt) != 0)
                continue;
            const uint8_t *table = buf + i - TABLE;
            const uint8_t *items = table + 1, *slots = items + ENTRIES, *nouns = slots + ENTRIES;
            SetCloseupVerb(table[0], 1);
            for (int j = 0; j < ENTRIES; j++) {
                if (nouns[j] == 0)
                    continue;
                for (USImage *image = USImages; image != NULL; image = image->next) {
                    if (image->usage == IMG_ROOM && image->index == slots[j]) {
                        AddCloseup(items[j], nouns[j], slots[j]);
                        break;
                    }
                }
            }
            AddExtraCloseups(CurrentGame);
            return;
        }
    }
}

/* Sorcerer of Claymorgue Castle US ships as a two-disk set (side A + B).
   If we loaded side A, look for side B by replacing "s1" with "s2" in the
   filename and re-running C64 detection on the companion disk. */
static GameIDType LookForSoCCompanion(const char *filename, uint8_t **sf, size_t *extent)
{
    if (filename == NULL)
        return UNKNOWN_GAME;
    size_t namelen = strlen(filename);
    uint8_t *newdata = NULL;
    size_t newlen = 0;
    for (int i = namelen - 2; i >= 0; i--) {
        if (filename[i] == 's' && filename[i+1] == '1') {
            char *newfile = MemAlloc(namelen + 1);
            memcpy(newfile, filename, namelen + 1);
            newfile[i+1] = '2';
            newdata = ReadFileIfExists(newfile, &newlen);
            free(newfile);
            if (newdata != NULL) {
                free(*sf);
                *sf = newdata;
                *extent = newlen;
                return DetectC64(sf, extent, NULL);
            } else {
                break;
            }
        }
    }
    return UNKNOWN_GAME;
}

#ifdef SCOTT_HEADLESS
/* Debug helper: write raw data to a file. */
static size_t writeToFile(const char *name, uint8_t *data, size_t size)
{
    FILE *fptr = fopen(name, "wb");

    if (fptr == NULL) {
        Fatal("File open error!");
        return 0;
    }

    size_t result = fwrite(data, 1, size, fptr);

    fclose(fptr);
    return result;
}
#endif

/* D64 disk image: extract the largest file (the game program),
   optionally append a companion file (e.g. picture data).
   Returns 0 on failure. */
static int ExtractD64Game(uint8_t **sf, size_t *extent, const c64rec *rec)
{
    size_t newlength;
    uint8_t *largest_file;
    /* The Slam compilation lists a copy of Claymorgue that ends 2.8 KB
       short, C1/SLAM. A complete, packed one is still on the disk, at
       track 22, sector 4, with no directory entry. */
    if (rec->chk == 0xf8c2)
        largest_file = di_get_file_at_ts(*sf, *extent, &newlength, 22, 4);
    else
        largest_file = di_get_largest_file(*sf, *extent, &newlength);
    uint8_t *appendix = NULL;
    size_t appendixlen = 0;

    if (largest_file == NULL)
        return 0;

    if (rec->appendfile != NULL) {
        appendix = di_get_file_named(*sf, *extent, &appendixlen,
            rec->appendfile);
        /* The companion file begins with a two-byte load address,
           which is skipped. If the file is missing or too short,
           carry on with the main file alone. */
        if (appendix == NULL || appendixlen < 2) {
            fprintf(stderr, "SCOTT: DetectC64() Appending file failed!\n");
            free(appendix);
            appendix = NULL;
            appendixlen = 0;
        } else {
            appendixlen -= 2;
        }
    }

    /* The companion file is placed relative to the end of the main
       file. A negative parameter means that the two overlap. */
    long appendpos = (long)newlength + rec->parameter;
    size_t buflen = newlength + appendixlen;
    if (appendix != NULL && appendpos > (long)newlength)
        buflen = appendpos + appendixlen;
    if (buflen == 0 || buflen > MAX_LENGTH || (appendix != NULL && appendpos < 0)) {
        free(largest_file);
        free(appendix);
        return 0;
    }

    /* Zero-filled, as the overlap leaves the end of the buffer unwritten */
    uint8_t *megabuf = MemCalloc(buflen);
    memcpy(megabuf, largest_file, newlength);
    if (appendix != NULL)
        memcpy(megabuf + appendpos, appendix + 2, appendixlen);
    free(largest_file);
    free(appendix);

    free(*sf);
    *sf = megabuf;
    *extent = buflen;
    return 1;
}

/* T64 tape image: parse the directory header to find and extract
   the first file record (2-byte load address + program data) */
static void ExtractT64File(uint8_t **sf, size_t *extent)
{
    uint8_t *file_records = *sf + 64;
    int number_of_records = READ_LE_UINT16(*sf + 36);
    int offset = READ_LE_UINT16(file_records + 8);
    int start_addr = READ_LE_UINT16(file_records + 2);
    int end_addr = READ_LE_UINT16(file_records + 4);
    int size;
    if (number_of_records == 1)
        size = *extent - offset;
    else
        size = end_addr - start_addr;
    uint8_t *first_file = MemAlloc(size + 2);
    memcpy(first_file + 2, *sf + offset, size);
    memcpy(first_file, file_records + 2, 2);
    free(*sf);
    *sf = first_file;
    *extent = size + 2;
}

/* Identify and load a C64 game from raw file data.

   Matches the file against the c64_registry by size + checksum. For multi-game
   compilation disks, presents a selection menu. Depending on the container
   type (D64, T64, or US/S.A.G.A.), extracts the game file, optionally appends
   companion data, decompresses via unp64, and loads the game database. Replaces
   *sf and *extent with the final game data on success. */
GameIDType DetectC64(uint8_t **sf, size_t *extent, const char *filename)
{
    if (*extent > MAX_LENGTH || *extent < MIN_LENGTH)
        return UNKNOWN_GAME;

    uint16_t chksum = C64Checksum(*sf, *extent);

    for (int i = 0; c64_registry[i].length != 0; i++) {
        if (*extent == c64_registry[i].length && chksum == c64_registry[i].chk) {
            /* Multi-game disks need user selection before extraction */
            if (c64_registry[i].id == SAVAGE_ISLAND_C64) {
                return savage_island_menu(sf, extent, i);
            } else if (c64_registry[i].id == BATON_C64 && (chksum == 0xc3fc || chksum == 0xbfbf)) {
                return mysterious_menu(sf, extent);
            } else if (c64_registry[i].id == FEASIBILITY_C64 && (chksum == 0x9eaa || chksum == 0x9c18)) {
                return mysterious_menu2(sf, extent);
            } else if (c64_registry[i].id == ADVENTURELAND_US && chksum == 0x78fa) {
                return adventure_pack_menu(sf, extent);
            } else if (c64_registry[i].id == CLAYMORGUE_US && chksum == 0xa957) {
                return LookForSoCCompanion(filename, sf, extent);
            }
            if (c64_registry[i].type == TYPE_D64) {
                if (!ExtractD64Game(sf, extent, &c64_registry[i]))
                    return UNKNOWN_GAME;
            } else if (c64_registry[i].type == TYPE_T64 || c64_registry[i].type == TYPE_T64_US) {
                ExtractT64File(sf, extent);

                if (c64_registry[i].type == TYPE_T64_US) {
                    ProcessC64(sf, extent, c64_registry[i]);
                    if (c64_registry[i].id == VOODOO_CASTLE_US || c64_registry[i].id == PIRATE_US) {
                        return handle_all_in_one(sf, extent, c64_registry[i]);
                    }
                    GameIDType result = LoadBinaryDatabase(*sf, *extent, *Game, 0);
                    if (result) {
                        CurrentSys = SYS_C64;
                        return CurrentGame;
                    }
                }
            /* US S.A.G.A. format on D64: extract the named database file,
               optionally decompress, and load the binary game database */
            } else if (c64_registry[i].type == TYPE_US) {
                size_t newlength;
                uint8_t *database_file = di_get_file_named(*sf, *extent, &newlength, c64_registry[i].appendfile);
                if (database_file == NULL) {
                    fprintf(stderr, "SCOTT: DetectC64() Could not find database in D64\n");
                    return UNKNOWN_GAME;
                }

                if (c64_registry[i].decompress_iterations) {
                    size_t len = (size_t)newlength;
                    ProcessC64(&database_file, &len, c64_registry[i]);
                    newlength = len;
                }

                /* There are at least two C64 S.A.G.A. games (Pirate Adventure and Voodoo Castle)
                   which do not have a separate database file, but have their interpreter, image data
                   and database all in a single file. We pass them off to a separate function, handle_all_in_one() in c64_small.c.
                 */
                if (c64_registry[i].imgoffset && c64_registry[i].imgoffset < newlength) {
                    return handle_all_in_one(&database_file, &newlength, c64_registry[i]);
                }

                GameIDType result = LoadBinaryDatabase(database_file, newlength, *Game, 0);
                /* An unpacked program may hold the interpreter in front
                   of the database, which SAGA.DB loads to $8030. */
                if (!result && c64_registry[i].decompress_iterations && newlength > 2) {
                    long dbstart = 0x8030 - READ_LE_UINT16(database_file);
                    if (dbstart > 0 && (size_t)dbstart < newlength) {
                        newlength -= dbstart;
                        memmove(database_file, database_file + dbstart, newlength);
                        result = LoadBinaryDatabase(database_file, newlength, *Game, 0);
                    }
                }
                if (result) {
                    CurrentSys = SYS_C64;
                    LoadC64USImages(*sf, *extent);
                    SetC64USCloseups(*sf, *extent);
                    free(*sf);
                    *sf = MemAlloc(newlength);
                    memcpy(*sf, database_file, newlength);
                    *extent = newlength;
                    return CurrentGame;
                }
            }
            return ProcessC64(sf, extent, c64_registry[i]);
        }
    }
    return UNKNOWN_GAME;
}

/* Decompress a C64 game file, load its database, and set up graphics.

   Many C64 adventure releases were packed with one or more layers of
   compression (CruelCrunch, PUCrunch, Exomizer, etc.). This function
   runs the unp64 decompressor for the number of iterations specified
   in the registry entry (passing switches on a specific iteration if
   needed), then identifies the game dictionary, loads the database via
   TryLoading, appends any Savage Island picture data, relocates
   graphics blocks via CopyData, and initializes the drawing system. */

/* The Danish Crackers release of Claymorgue has fourteen damaged bytes
   that every other release has intact: three in the system messages, three
   in the interpreter, two in the tiles and six in the pictures, three of
   those in the pool of dirty water. Put back what the other releases have.
   The offsets are those of the unpacked program, which loads at $0801. */
static void RepairDanishCrackersClaymorgue(uint8_t *data, size_t length)
{
    static const struct {
        uint16_t offset;
        uint8_t damaged;
        uint8_t intact;
    } repairs[] = {
        { 0x0871, 0x07, 0x74 }, { 0x0876, 0x01, 0x77 }, { 0x0966, 0x01, 0x6f },
        { 0x15e7, 0x7a, 0x3d }, { 0x1655, 0x52, 0xa9 }, { 0x18a6, 0x95, 0x92 },
        { 0x29c1, 0x08, 0x01 }, { 0x29d3, 0x06, 0x01 },
        { 0x69c1, 0x11, 0x10 }, { 0x69d3, 0x06, 0x20 },
        { 0x75b1, 0x55, 0x80 },
        { 0xa93f, 0x35, 0x06 }, { 0xa940, 0xe6, 0x90 }, { 0xa941, 0x33, 0x03 },
    };

    for (size_t i = 0; i < sizeof repairs / sizeof repairs[0]; i++) {
        if (repairs[i].offset < length && data[repairs[i].offset] == repairs[i].damaged)
            data[repairs[i].offset] = repairs[i].intact;
    }
}

static GameIDType ProcessC64(uint8_t **sf, size_t *extent, c64rec record)
{
    size_t length = *extent;

    C64DecompressIterations(sf, &length, record.decompress_iterations,
        record.switches, record.parameter);

    if (record.id == CLAYMORGUE_C64
        && (record.chk == 0x5592 || record.chk == 0x10a5 || record.chk == 0xf8c2))
        RepairDanishCrackersClaymorgue(*sf, length);

#ifdef SCOTT_HEADLESS
    /* Debug: dump the decompressed image for offline analysis */
    const char *dumppath = getenv("C64_DUMP_US_IMAGE");
    if (dumppath != NULL)
        writeToFile(dumppath, *sf, length);
#endif

    *extent = length;

    /* US-format games are loaded by the caller, not here */
    if (record.type == TYPE_US || record.type == TYPE_T64_US) {
        return record.id;
    }

    /* Look up the full GameInfo entry in the master games[] table */
    for (int i = 0; games[i].Title != NULL; i++) {
        if (games[i].gameID == record.id) {
            *Game = games[i];
            break;
        }
    }

    if (Game->Title == NULL) {
        Fatal("Game not found!");
    }

    size_t offset;

    DictionaryType dictype = GetId(*sf, length, &offset);
    if (dictype != Game->dictionary) {
        Fatal("Wrong game?");
    }

    /* TryLoading adopts the buffer into entire_file/file_length internally;
       if entire_file was pointing at the source disk image, it is freed
       as part of that adoption. */
    if (TryLoading(*sf, length, Game, offset) == UNKNOWN_GAME) {
        Fatal("Game could not be read!");
    }

    if (save_island_appendix_1 != NULL) {
        appendSIfiles(sf, extent);
    }

    /* The copy offsets of the Claymorgue records are those of a program
       loaded at $0800. Some releases load it elsewhere. */
    int shift = (CurrentGame == CLAYMORGUE_C64) ? file_baseline_offset : 0;

    /* Relocate graphics data to the addresses the drawing engine expects.
       Gremlins preserves a $1000-byte block that would otherwise be lost
       across the CopyData reallocation, and rewrites it at a shifted offset. */
    if (CurrentGame == GREMLINS_C64 && record.copysource == 0xc801) {
        uint8_t overlap[0x1000];
        memcpy(overlap, *sf + 0xd801, 0x1000);
        size_t newSize = CopyData(record.copydest, record.copysource, sf, *extent, record.copysize);
        *extent = newSize;
        memcpy(*sf + 0xc774, overlap, 0x1000);
    } else if (record.copysource != 0) {
        size_t newSize = CopyData(record.copydest + shift, record.copysource + shift,
            sf, *extent, record.copysize);
        if (newSize) {
            *extent = newSize;
        }
    }

    /* Claymorgue has extra image data that needs copying to a high offset */
    if (CurrentGame == CLAYMORGUE_C64 && record.copysource == 0x855) {
        size_t newSize = CopyData(0x1531a + shift, 0x2002 + shift, sf, *extent, 0x2000);
        if (newSize) {
            *extent = newSize;
        }
    }

    /* If the game uses tile-based graphics (unlike the Mysterious Adventures)
       we need to initialize them here. */
    if (!(Game->subtype & MYSTERIOUS))
        SagaGraphicsSetup(record.imgoffset);

    return CurrentGame;
}
