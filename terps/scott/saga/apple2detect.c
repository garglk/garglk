//
//  apple2detect.c
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-08-03.
//

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "scott.h"
#include "scottdefines.h"
#include "saga.h"
#include "sagagraphics.h"
#include "woz2nib.h"
#include "ciderpress.h"
#include "hulk.h"
#include "apple2draw.h"

#include "apple2detect.h"

static const pairrec a2companionlist[][2] = {
    {{ 0x23000, 0xa989, "Scott Adams Graphic Adventure 1 - Adventureland v2.0-416 (4am crack) side A.dsk", 79}, { 0x23000, 0x5750, "Scott Adams Graphic Adventure 1 - Adventureland v2.0-416 (4am crack) side B - boot.dsk", 86 }},

    {{ 0x39557, 0x3ff3, "SAGA #1 - Adventureland v2.0-416 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 90 },{ 0x39557, 0x374e, "SAGA #1 - Adventureland v2.0-416 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 96 }},

    {{ 0x39567, 0x8aa4, "SAGA #2 - Pirate Adventure v2.1-408 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 93 },{ 0x39567, 0xcf60, "SAGA #2 - Pirate Adventure v2.1-408 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 99 }},

    {{ 0x39554, 0xbbd3, "SAGA #3 - Mission Impossible v2.1-306 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 95 }, { 0x39554, 0x361a, "SAGA #3 - Mission Impossible v2.1-306 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 101 }},

    {{ 0x39558, 0x958f, "SAGA #4 - Voodoo Castle v2.1-119 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 90 }, { 0x39558, 0xff6a, "SAGA #4 - Voodoo Castle v2.1-119 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 96 }},

    {{ 0x23000, 0xedf, "Scott Adams Graphic Adventure 5 - The Count v2.1-115 (4am crack) side A.dsk", 75}, { 0x23000, 0x09f4, "Scott Adams Graphic Adventure 5 - The Count v2.1-115 (4am crack) side B - boot.dsk", 82 }},

    {{ 0x39589, 0x7010, "SAGA #5 - The Count v2.1-115 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 86 }, { 0x39589, 0xbf0e, "SAGA #5 - The Count v2.1-115 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 92 }},

    {{ 0x39589, 0xee5d, "SAGA #6 - Strange Odyssey v2.1-119 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 92 }, { 0x39589, 0x4c17, "SAGA #6 - Strange Odyssey v2.1-119 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 98 }},

    {{ 0x23000, 0xd8ca, "Scott Adams Graphic Adventure 6 - Strange Odyssey v2.1-119 (4am crack) side A.dsk", 81 }, { 0x23000, 0xd700, "Scott Adams Graphic Adventure 6 - Strange Odyssey v2.1-119 (4am crack) side B - boot.dsk", 88 }},

    {{ 0,0, NULL }, { 0,0, NULL }}
};

typedef struct imglist {
    USImageType usage;
    int index;
    size_t offset;
    size_t size;
} imglist;

static const imglist a2listHulk[] = {
    { IMG_ROOM, 0, 0x1000, 0x4ad }, // Too dark
    { IMG_ROOM, 1, 0x1500, 0x69a }, // Tied to chair
    { IMG_ROOM, 2, 0x1c00, 0x6d1 }, // dome
    { IMG_ROOM, 3, 0x2400, 0x501 }, // tunnel
    { IMG_ROOM, 4, 0x2a00, 0x6b4 }, // field
    { IMG_ROOM, 5, 0x3100, 0x501 }, // tunnel 2
    { IMG_ROOM, 6, 0x3700, 0x501 }, // tunnel 3
    { IMG_ROOM, 7, 0x3d00, 0x6b4 }, // field 2
    { IMG_ROOM, 8, 0x4400, 0x6b4 }, // field 3
    { IMG_ROOM, 9, 0x4b00, 0x9f7 }, // hole
    { IMG_ROOM, 10, 0x5600, 0x9f7 }, // hole 2
    { IMG_ROOM, 11, 0x6100, 0x9f7 }, // hole 3
    { IMG_ROOM, 12, 0x6c00, 0x134d }, // cavern
    { IMG_ROOM, 13, 0x8000, 0x66e }, // dome 2
    { IMG_ROOM, 14, 0x8700, 0x66e }, // dome 3
    { IMG_ROOM, 15, 0x8e00, 0x956 }, // small underground room
    { IMG_ROOM, 16, 0x9800, 0xcad }, // fuzzy area

    { IMG_ROOM, 19, 0xa700, 0x798 }, // room with the Chief Examiner
    { IMG_ROOM, 20, 0xaf00, 0x1130 }, // limbo
    { IMG_ROOM, 81, 0xc100, 0xb4a }, // hulk in mirror
    { IMG_ROOM, 82, 0xcd00, 0x9b0 }, // banner in mirror
    { IMG_ROOM, 83, 0xd700, 0x967 }, // rope breaks
    { IMG_ROOM, 84, 0xe100, 0x11f9 }, // I'm Hulk
    { IMG_ROOM, 85, 0xf400, 0xce6 }, // Lifts dome
    { IMG_ROOM, 86, 0x10200, 0x1054 }, // Gas
    { IMG_ROOM, 87, 0x11300, 0x7b6 }, // Victory
    { IMG_ROOM, 88, 0x11b00, 0xe3f }, // Nightmares
    { IMG_ROOM, 89, 0x12a00, 0x12b1 }, // Digging
    { IMG_ROOM, 90, 0x13e00, 0xb7e }, // Dome with mesh

    { IMG_ROOM, 91, 0x14a00, 0xb39 }, // Bio gem
    { IMG_ROOM, 92, 0x15600, 0x992 }, // Natter egg

    { IMG_ROOM, 93, 0x16000, 0xaea }, // ant
    { IMG_ROOM, 94, 0x16b00, 0xfeb }, // bees
    { IMG_ROOM, 95, 0x17b00, 0x730 }, // ring in floor
    { IMG_ROOM, 96, 0x18300, 0x1082 }, // ant-man
    { IMG_ROOM, 97, 0x19400, 0x63c }, // outlet
    { IMG_ROOM, 99, 0x19b00, 0xe1b }, // title

    { IMG_INV_OBJ, 1, 0x1aa00, 0x523 }, // rage (Hulk with bag)
    { IMG_INV_OBJ, 2, 0x1b000, 0x83 }, // mirror
    { IMG_INV_OBJ, 3, 0x1b100, 0x102 }, // broken chair
    { IMG_INV_OBJ, 4, 0x1b300, 0x3e }, // gem 1
    { IMG_INV_OBJ, 12, 0x1b400, 0x3e }, // gem 2

    { IMG_ROOM_OBJ, 13, 0x1b500, 0xc2 }, // pit
    { IMG_ROOM_OBJ, 14, 0x1b600, 0xc2 }, // pit
    { IMG_ROOM_OBJ, 15, 0x1b700, 0xc2 }, // pit

    { IMG_INV_OBJ, 16, 0x1b800, 0x3e }, // gem 3

    { IMG_ROOM_OBJ, 17, 0x1b900, 0x1b6 }, // egg in room
    { IMG_INV_OBJ, 18, 0x1bb00, 0xed }, // bio-gem in inventory

    { IMG_INV_OBJ, 19, 0x1bc00, 0x3e }, // gem 4

    { IMG_ROOM_OBJ, 20, 0x1bd00, 0x75b }, // ANTS (broken)

    { IMG_INV_OBJ, 21, 0x1c500, 0x45 }, // wax

    { IMG_ROOM_OBJ, 22, 0x1c600, 0x308 }, // anthills (tiny holes)

    { IMG_INV_OBJ, 23, 0x1ca00, 0xa9 }, // fan

    { IMG_ROOM_OBJ, 24, 0x1cb00, 0xa0 }, // bees
    { IMG_ROOM_OBJ, 25, 0x1cc00, 0x13e }, // beeholes
    { IMG_ROOM_OBJ, 26, 0x1ce00, 0xda2 }, // dr strange (broken)

    { IMG_INV_OBJ, 29, 0x1dc00, 0x3e }, // gem 5
    { IMG_INV_OBJ, 30, 0x1dd00, 0x3e }, // gem 6
    { IMG_INV_OBJ, 31, 0x1de00, 0x3e }, // gem 7
    { IMG_INV_OBJ, 32, 0x1df00, 0x3e }, // gem 8

    { IMG_ROOM_OBJ, 33, 0x1e000, 0xad }, // Ring in floor
    { IMG_ROOM_OBJ, 34, 0x1e100, 0x240 }, // Hole in floor

    { IMG_INV_OBJ, 35, 0x1e400, 0x3e }, // gem 9

    { IMG_ROOM_OBJ, 36, 0x1e500, 0x76 }, // button

    { IMG_INV_OBJ, 37, 0x1e600, 0x3d }, // gem 10

    { IMG_ROOM_OBJ, 39, 0x1e700, 0x201 }, // Hole in ceiling
    { IMG_ROOM_OBJ, 40, 0x1ea00, 0xf6 }, // Crack in floor

    { IMG_INV_OBJ, 42, 0x1eb00, 0x3d }, // gem 11
    { IMG_INV_OBJ, 44, 0x1ec00, 0x3d }, // gem 12

    { IMG_ROOM_OBJ, 45, 0x1ed00, 0xf05 }, // ultron

    { IMG_INV_OBJ, 48, 0x1fd00, 0x3c }, // gem 13
    { IMG_INV_OBJ, 49, 0x1fe00, 0x3c }, // gem 14
    { IMG_INV_OBJ, 50, 0x1ff00, 0x3c }, // gem 15

    { IMG_ROOM_OBJ, 70, 0x20100, 0x2e1 }, // Bio-Gem in room
    { IMG_ROOM_OBJ, 72, 0x20700, 0x3f }, // Wax in room

    { IMG_INV_OBJ, 51, 0x21200, 0x3c }, // gem 16


    { 0, 0, 0, 0 }
};

static const imglist a2listHulk126[] = {
    { IMG_ROOM, 0, 0x1000, 0x4ad }, // Too dark
    { IMG_ROOM, 1, 0x1500, 0x6cc }, // Tied to chair
    { IMG_ROOM, 2, 0x1c00, 0x6c0 }, // dome
    { IMG_ROOM, 3, 0x2300, 0x500 }, // tunnel
    { IMG_ROOM, 4, 0x2900, 0x6b4 }, // field
    { IMG_ROOM, 5, 0x3000, 0x501 }, // tunnel 2
    { IMG_ROOM, 6, 0x3600, 0x501 }, // tunnel 3
    { IMG_ROOM, 7, 0x3c00, 0x6b4 }, // field 2
    { IMG_ROOM, 8, 0x4300, 0x6b4 }, // field 3
    { IMG_ROOM, 9, 0x4a00, 0x9f7 }, // hole
    { IMG_ROOM, 10, 0x5300, 0x9f7 }, // hole 2
    { IMG_ROOM, 11, 0x5c00, 0x9f7 }, // hole 3
    { IMG_ROOM, 12, 0x6500, 0x11ec }, // cavern
    { IMG_ROOM, 13, 0x7800, 0x6bf }, // dome 2
    { IMG_ROOM, 14, 0x7f00, 0x6bf }, // dome 3
    { IMG_ROOM, 15, 0x8600, 0x956 }, // small underground room
    { IMG_ROOM, 16, 0x9000, 0xcad }, // fuzzy area
    { IMG_ROOM, 19, 0x9f00, 0x798 }, // room with Chief Examiner
    { IMG_ROOM, 20, 0xa700, 0xba2 }, // limbo

    { IMG_ROOM, 81, 0xb400, 0xb4a }, // hulk in mirror
    { IMG_ROOM, 82, 0xc000, 0x9bc }, // banner in mirror
    { IMG_ROOM, 83, 0xca00, 0x994 }, // rope breaks
    { IMG_ROOM, 84, 0xd400, 0x11f9 }, // I'm Hulk
    { IMG_ROOM, 85, 0xe500, 0xe10 }, // Lifts dome
    { IMG_ROOM, 86, 0xf400, 0x105a }, // Gas
    { IMG_ROOM, 87, 0x10500, 0x7b7 }, // Victory
    { IMG_ROOM, 88, 0x10d00, 0xe3f }, // Nightmares
    { IMG_ROOM, 89, 0x11c00, 0x12b1 }, // Digging
    { IMG_ROOM, 90, 0x13000, 0xb7e }, // Dome with mesh
    { IMG_ROOM, 91, 0x13c00, 0xe1e }, // Bio gem
    { IMG_ROOM, 92, 0x14b00, 0x992 }, // Natter egg
    { IMG_ROOM, 93, 0x15500, 0xaea }, // ant
    { IMG_ROOM, 94, 0x16000, 0xfeb }, // bees
    { IMG_ROOM, 95, 0x17000, 0x730 }, // ring in floor
    { IMG_ROOM, 96, 0x17800, 0x1082 }, // ant-man
    { IMG_ROOM, 97, 0x18900, 0x63c }, // outlet

    { IMG_ROOM, 98, 0x19000, 0x393 }, // inventory

    { IMG_ROOM, 99, 0x19400, 0x540 }, // title (placeholder)

    { IMG_INV_OBJ, 1, 0x19a00, 0x523 }, // rage (Hulk with bag)
    { IMG_INV_OBJ, 2, 0x1a000, 0x83 }, // mirror
    { IMG_INV_OBJ, 3, 0x1a100, 0x102 }, // broken chair
    { IMG_INV_OBJ, 4, 0x1a300, 0x3e }, // gem 1
    { IMG_INV_OBJ, 12, 0x1a400, 0x3e }, // gem 2

    { IMG_ROOM_OBJ, 13, 0x1a500, 0xc2 }, // pit
    { IMG_ROOM_OBJ, 14, 0x1a600, 0xc2 }, // pit
    { IMG_ROOM_OBJ, 15, 0x1a700, 0xc2 }, // pit

    { IMG_INV_OBJ, 16, 0x1a800, 0x3e }, // gem 3

    { IMG_ROOM_OBJ, 17, 0x1a900, 0x1b6 }, // egg in room

    { IMG_INV_OBJ, 18, 0x1ab00, 0xed }, // bio-gem in inventory
    { IMG_INV_OBJ, 19, 0x1ac00, 0x3e }, // gem 4

    { IMG_ROOM_OBJ, 20, 0x1ad00, 0x75b }, // ants

    { IMG_INV_OBJ, 21, 0x1b500, 0x45 }, // wax

    { IMG_ROOM_OBJ, 22, 0x1b600, 0x308 }, // anthills (tiny holes)

    { IMG_INV_OBJ, 23, 0x1ba00, 0xa9 }, // fan

    { IMG_ROOM_OBJ, 24, 0x1bb00, 0xa0 }, // bees
    { IMG_ROOM_OBJ, 25, 0x1bc00, 0x13e }, // beeholes
    { IMG_ROOM_OBJ, 26, 0x1be00, 0xe75 }, // dr strange

    { IMG_INV_OBJ, 29, 0x1cd00, 0x3e }, // gem 5
    { IMG_INV_OBJ, 30, 0x1ce00, 0x3e }, // gem 6
    { IMG_INV_OBJ, 31, 0x1cf00, 0x3e }, // gem 7
    { IMG_INV_OBJ, 32, 0x1d000, 0x3e }, // gem 8

    { IMG_ROOM_OBJ, 33, 0x1d100, 0xad }, // Ring in floor
    { IMG_ROOM_OBJ, 34, 0x1d200, 0x240 }, // Hole in floor

    { IMG_INV_OBJ, 35, 0x1d500, 0x3e }, // gem 9

    { IMG_ROOM_OBJ, 36, 0x1d600, 0x76 }, // button

    { IMG_INV_OBJ, 37, 0x1d700, 0x3d }, // gem 10

    { IMG_ROOM_OBJ, 39, 0x1d800, 0x2a0 }, // Hole in ceiling
    { IMG_ROOM_OBJ, 40, 0x1db00, 0xf6 }, // Crack in floor

    { IMG_INV_OBJ, 42, 0x1dc00, 0x3d }, // gem 11
    { IMG_INV_OBJ, 44, 0x1dd00, 0x3d }, // gem 12

    { IMG_ROOM_OBJ, 45, 0x1de00, 0xf05 }, // ultron

    { IMG_INV_OBJ, 48, 0x1ea00, 0x3c }, // gem 13

    { IMG_ROOM_OBJ, 47, 0x1eb00, 0x105 }, // Ant-man in cage

    { IMG_INV_OBJ, 49, 0x1ed00, 0x3c }, // gem 14
    { IMG_INV_OBJ, 50, 0x1ee00, 0x3c }, // gem 15
    { IMG_INV_OBJ, 51, 0x1ef00, 0x3c }, // gem 16

    { IMG_ROOM_OBJ, 53, 0x1f000, 0x6c }, // sign (in dome 1)

    { IMG_ROOM_OBJ, 70, 0x1f100, 0x2e1 }, // Bio-Gem in room
    { IMG_ROOM_OBJ, 17, 0x1f500, 0x18e }, // Egg in room

    { IMG_ROOM_OBJ, 72, 0x1f700, 0x3f }, // Wax in room

    { 0, 0, 0, 0 }
};


static const imglist a2listClaymorgue[] = {
    { IMG_ROOM, 0, 0x1000, 0x01ac }, // Too dark
    { IMG_ROOM, 1, 0x01200, 0x0c4a }, // field
    { IMG_ROOM, 2, 0x01f00, 0x06da }, // on drawbridge
    { IMG_ROOM, 3, 0x02600, 0x119a }, // courtyard
    { IMG_ROOM, 4, 0x03800, 0x0b4e }, // magic fountain
    { IMG_ROOM, 5, 0x04400, 0x0bf2 }, // stream of lava
    { IMG_ROOM, 6, 0x05000, 0x0ddc }, // top of the fountain
    { IMG_ROOM, 7, 0x05e00, 0x07b2 }, // ball room
    { IMG_ROOM, 8, 0x06600, 0x0aa4 }, // on chandelier
    { IMG_ROOM, 9, 0x07100, 0x04e4 }, // forest of enchantment
    { IMG_ROOM, 10, 0x07600, 0x08bc }, // plain room
    { IMG_ROOM, 11, 0x07f00, 0x0b6e }, // storeroom
    { IMG_ROOM, 12, 0x08b00, 0x0a00 }, // dungeon cell
    { IMG_ROOM, 13, 0x09800, 0x0914 }, // anteroom
    { IMG_ROOM, 14, 0x0a200, 0x0858 }, // staircase
    { IMG_ROOM, 15, 0x0ab00, 0x08d8 }, // loft above ballroom
    { IMG_ROOM, 16, 0x0b400, 0x111a }, // in the moat
    { IMG_ROOM, 17, 0x0c600, 0x00a0 }, // underwater
    { IMG_ROOM, 18, 0x0c700, 0x1358 }, // *I'm under the stairs
    { IMG_ROOM, 19, 0x0db00, 0x14d0 }, // hollow tree
    { IMG_ROOM, 20, 0x0f000, 0x090e }, // pool of dirty water
    { IMG_ROOM, 21, 0x0fa00, 0x0916 }, // kitchen
    { IMG_ROOM, 22, 0x10400, 0x07a0 }, // *I'm on a box
    { IMG_ROOM, 23, 0x10c00, 0x09e2 }, // box
    { IMG_ROOM, 24, 0x11600, 0x111a }, // dusty room
    { IMG_ROOM, 25, 0x12800, 0x0558 }, // stone staircase
    { IMG_ROOM, 26, 0x12e00, 0x1550 }, // damp cavern
    { IMG_ROOM, 27, 0x14400, 0x0c28 }, // stone grotto
    { IMG_ROOM, 28, 0x15100, 0x11f6 }, // *I'm in an entryway
    { IMG_ROOM, 29, 0x16300, 0x139c }, // dragon's lair
    { IMG_ROOM, 30, 0x17700, 0x10ca }, // wizard's workshop
    { IMG_ROOM, 31, 0x18800, 0x085e }, // vacant room
    { IMG_ROOM, 32, 0x19100, 0x0be4 }, // real mess!

    { IMG_ROOM, 99, 0x19d00, 0x0bd8 }, // Title

    { IMG_INV_OBJ, 0, 0x1a900, 0x16 }, // star 0

    { IMG_ROOM_OBJ, 4, 0x1aa00, 0xe0 }, // Lowered drawbridge

    { IMG_INV_OBJ, 5, 0x1ab00, 0x20 }, // star 1
    { IMG_INV_OBJ, 6, 0x1ac00, 0x16 }, // star 2
    { IMG_INV_OBJ, 7, 0x1ad00, 0x12 }, // star 3
    { IMG_INV_OBJ, 8, 0x1ae00, 0x16 }, // star 4
    { IMG_INV_OBJ, 9, 0x1af00, 0x8c }, // I'm glowing
    { IMG_INV_OBJ, 10, 0x1b000, 0x16 }, // star 5
    { IMG_INV_OBJ, 11, 0x1b100, 0x16 }, // star 6
    { IMG_INV_OBJ, 12, 0x1b200, 0x20 }, // star 7
    { IMG_INV_OBJ, 13, 0x1b300, 0x16 }, // star 8
    { IMG_INV_OBJ, 14, 0x1b400, 0x16 }, // star 9
    { IMG_INV_OBJ, 15, 0x1b500, 0x16 }, // star 10
    { IMG_INV_OBJ, 16, 0x1b600, 0x16 }, // star 11
    { IMG_INV_OBJ, 20, 0x1b700, 0x3e }, // Magic mirror
    { IMG_INV_OBJ, 24, 0x1b800, 0x74 }, // Fire spell

    { IMG_ROOM_OBJ, 26, 0x1b900, 0x370 }, // Fallen Chandelier

    { IMG_INV_OBJ, 28, 0x1bd00, 0x44 }, // Broken glass

    { IMG_ROOM_OBJ, 30, 0x1be00, 0x1a4 }, // Open door
    { IMG_ROOM_OBJ, 32, 0x1c000, 0x192 }, // Open door 2

    { IMG_INV_OBJ, 33, 0x1c200, 0x76 }, // Spell of Methuselah
    { IMG_INV_OBJ, 34, 0x1c300, 0x74 }, // Seed spell
    { IMG_INV_OBJ, 35, 0x1c400, 0x58 }, // Potion
    { IMG_INV_OBJ, 36, 0x1c500, 0x6c }, // Light squared spell

    { IMG_ROOM_OBJ, 38, 0x1c600, 0xd16 }, // loft
    { IMG_ROOM_OBJ, 39, 0x1d400, 0x69c }, // moat bottom

    { IMG_INV_OBJ, 40, 0x1db00, 0x6a }, // Permeability spell
    { IMG_INV_OBJ, 41, 0x1dc00, 0x74 }, // Spell of bliss
    { IMG_INV_OBJ, 42, 0x1dd00, 0x5c }, // Piece of wood
    { IMG_INV_OBJ, 45, 0x1de00, 0x70 }, // Yoho spell

    { IMG_ROOM_OBJ, 47, 0x1df00, 0xca }, // hole (in crate top, from below)

    { IMG_INV_OBJ, 48, 0x1e000, 0x4a }, // crate

    { IMG_ROOM_OBJ, 49, 0x1e100, 0x2fe }, // hole (in crate top, from above)

    { IMG_INV_OBJ, 50, 0x1e500, 0x58 }, // fire bricks
    { IMG_INV_OBJ, 51, 0x1e600, 0x140 }, // water droplets
    { IMG_INV_OBJ, 52, 0x1e800, 0x218 }, // wrinkles

    { IMG_ROOM_OBJ, 54, 0x1eb00, 0x2fa }, // open door 3
    { IMG_ROOM_OBJ, 55, 0x1ee00, 0x592 }, // raised drawbridge

    { IMG_INV_OBJ, 57, 0x1f400, 0x74 }, // firefly spell
    { IMG_INV_OBJ, 59, 0x1f500, 0x24 }, // dust

    { IMG_ROOM_OBJ, 60, 0x1f600, 0x6b8 }, // hole behind dragon

    { IMG_INV_OBJ, 61, 0x1fd00, 0x7a }, // wicked queen's spell
    { IMG_INV_OBJ, 62, 0x1fe00, 0x78 }, // soggy towel
    { IMG_INV_OBJ, 63, 0x1ff00, 0x3a }, // ashes
    { IMG_INV_OBJ, 64, 0x20000, 0x74 }, // unravel spell
    { IMG_INV_OBJ, 65, 0x20100, 0x4a }, // damp towel
    { IMG_INV_OBJ, 66, 0x20200, 0x40 }, // dry towel
    { IMG_INV_OBJ, 68, 0x20300, 0x2a }, // tin can
    { IMG_INV_OBJ, 69, 0x20400, 0x4a }, // open can
    { IMG_INV_OBJ, 70, 0x20500, 0x74 }, // dizzy dean spell
    { IMG_INV_OBJ, 71, 0x20600, 0x36 }, // piece of metal
    { IMG_INV_OBJ, 73, 0x20700, 0x68 }, // lycanthrope spell

    { IMG_ROOM_OBJ, 75, 0x20800, 0x3b4 }, // rabid rats

    { 0, 0, 0, 0 }
};

// Images for alternative version, pre-release?
static const imglist a2listClaymorgue126[] = {
    { IMG_ROOM, 0, 0x1000, 0x1bf }, // Too dark
    { IMG_ROOM, 1, 0x1200, 0xc02 }, // field
    { IMG_ROOM, 2, 0x1f00, 0x67a }, // *I'm on a Drawbridge
    { IMG_ROOM, 3, 0x2600, 0x116a }, // courtyard
    { IMG_ROOM, 4, 0x3800, 0xb41 }, // magic fountain
//    { IMG_ROOM, 5, 0x4400, 0xdbc }, // stream of lava
    { IMG_ROOM, 6, 0x5000, 0xdbc }, // on top of the fountain
    { IMG_ROOM, 7, 0x5e00, 0x78b }, // ball room
    { IMG_ROOM, 8, 0x6600, 0xa75 }, // on a large chandelier
    { IMG_ROOM, 9, 0x7100, 0x4aa }, // forest of enchantment
    { IMG_ROOM, 10, 0x7600, 0x9f8 }, // plain room
    { IMG_ROOM, 11, 0x7f00, 0xb4a }, // storeroom
    { IMG_ROOM, 12, 0x8b00, 0xb8f }, // dungeon cell
    { IMG_ROOM, 13, 0x9700, 0x134e }, // anteroom
    { IMG_ROOM, 14, 0xa000, 0x85c }, // staircase
    { IMG_ROOM, 15, 0xa900, 0x85e }, // loft above ballroom
    { IMG_ROOM, 16, 0xb200, 0x1100 }, // I'm in the water of a moat
    { IMG_ROOM, 17, 0xc400, 0x7d }, // I'm underwater in thick murky fluid
    { IMG_ROOM, 18, 0xc500, 0x4b7 }, // I'm under the stairs
//    { IMG_ROOM, 19, 0xd200, 0x1131 }, // hollow tree sign says drop stars here
    { IMG_ROOM, 20, 0xee00, 0x8fb }, // pool of dirty water
    { IMG_ROOM, 21, 0xf800, 0x8e1 }, // kitchen
    { IMG_ROOM, 22, 0x10200, 0x1130 }, // I'm on a box
    { IMG_ROOM, 23, 0x10a00, 0x9c6 }, // inside box
    { IMG_ROOM, 24, 0x11400, 0x10c9 }, // dusty room
    { IMG_ROOM, 25, 0x12500, 0x11f9 }, // stone staircase
    { IMG_ROOM, 26, 0x12b00, 0xce6 }, // damp cavern
    { IMG_ROOM, 27, 0x14100, 0x11f9 }, // stone grotto
    { IMG_ROOM, 28, 0x14d00, 0x2000 }, // I'm in an entryway
    { IMG_ROOM, 29, 0x15f00, 0x1342 }, // dragon's lair
    { IMG_ROOM, 30, 0x17300, 0x1093 }, // Wizard's workshop
    { IMG_ROOM, 31, 0x18400, 0x855 }, // vacant room
    { IMG_ROOM, 32, 0x18d00, 0x11f9 }, // dead

    { IMG_ROOM, 99, 0x19900, 0xbba }, // Title

    { IMG_INV_OBJ, 0, 0x1a500, 0x21 }, // star 0
    { IMG_ROOM_OBJ, 4, 0x1a600, 0xcc }, // Lowered drawbridge
    { IMG_INV_OBJ, 5, 0x1a700, 0x23 }, // star 1
    { IMG_INV_OBJ, 6, 0x1a800, 0x21 }, // star 2
    { IMG_INV_OBJ, 7, 0x1a900, 0x19 }, // star 3

    { IMG_INV_OBJ, 8, 0x1aa00, 0x22 }, // star 4
    { IMG_INV_OBJ, 9, 0x1ab00, 0x8c }, // I'm glowing
    { IMG_INV_OBJ, 10, 0x1ac00, 0x22 }, // star 5
    { IMG_INV_OBJ, 11, 0x1ad00, 0x1c }, // star 6
    { IMG_INV_OBJ, 12, 0x1ae00, 0x24 }, // star 7
    { IMG_INV_OBJ, 13, 0x1af00, 0x23 }, // star 8
    { IMG_INV_OBJ, 14, 0x1b000, 0x22 }, // star 9
    { IMG_INV_OBJ, 15, 0x1b100, 0x1a }, // star 10
    { IMG_INV_OBJ, 16, 0x1b200, 0x22 }, // star 11
    { IMG_INV_OBJ, 17, 0x1b300, 0x1c }, // star 12
    { IMG_INV_OBJ, 20, 0x1b400, 0x41 }, // Magic mirror
    { IMG_INV_OBJ, 24, 0x1b500, 0x4e }, // Fire spell

    { IMG_ROOM_OBJ, 26, 0x1b600, 0x11f9}, // Fallen Chandelier

    { IMG_INV_OBJ, 28, 0x1ba00, 0x45 }, // Broken glass

    { IMG_ROOM_OBJ, 30, 0x1bb00, 0x196}, // Open door
    { IMG_ROOM_OBJ, 32, 0x1bd00, 0x14d}, // Open door 2

    { IMG_INV_OBJ, 33, 0x1bf00, 0x52 }, // Spell of Methuselah
    { IMG_INV_OBJ, 34, 0x1c000, 0x4b }, // Seed spell
    { IMG_INV_OBJ, 35, 0x1c100, 0x55 }, // Potion
    { IMG_INV_OBJ, 36, 0x1c200, 0x4c }, // Light squared spell
    { IMG_INV_OBJ, 36, 0x1c200, 0x4c }, // Light squared spell

    { IMG_ROOM_OBJ, 38, 0x1c300, 0xcdd }, // loft
    { IMG_ROOM_OBJ, 39, 0x1d000, 0x68b }, // moat bottom

    { IMG_INV_OBJ, 40, 0x1d700, 0x4c }, // Permeability spell
    { IMG_INV_OBJ, 41, 0x1d800, 0x4d }, // Spell of bliss
    { IMG_INV_OBJ, 42, 0x1d900, 0x5f }, // Piece of wood
    { IMG_INV_OBJ, 45, 0x1da00, 0x53 }, // Yoho spell

    { IMG_ROOM_OBJ, 47, 0x1db00, 0xcb }, // hole (in crate top, from below)

    { IMG_INV_OBJ, 48, 0x1dc00, 0x5d }, // crate

    { IMG_ROOM_OBJ, 49, 0x1dd00, 0x2ed }, // hole (in crate top, from above)

    { IMG_INV_OBJ, 50, 0x1e000, 0x5e }, // fire bricks
    { IMG_INV_OBJ, 51, 0x1e100, 0x75 }, // water droplets
    { IMG_INV_OBJ, 52, 0x1e200, 0x1e4 }, // wrinkles

    { IMG_ROOM_OBJ, 54, 0x1e500, 0x2f3 }, // open door 3
    { IMG_ROOM_OBJ, 55, 0x1e800, 0x30f }, // raised drawbridge

    { IMG_INV_OBJ, 57, 0x1ec00, 0x52 }, // firefly spell
    { IMG_INV_OBJ, 59, 0x1ed00, 0x22 }, // dust

    { IMG_ROOM_OBJ, 60, 0x1ee00, 0x6ae }, // hole behind dragon

    { IMG_INV_OBJ, 61, 0x1f500, 0x4f }, // wicked queen's spell
    { IMG_INV_OBJ, 62, 0x1f600, 0x7c }, // soggy towel
    { IMG_INV_OBJ, 63, 0x1f700, 0x40 }, // ashes
    { IMG_INV_OBJ, 64, 0x1f800, 0x4d }, // unravel spell
    { IMG_INV_OBJ, 65, 0x1f900, 0x5c }, // damp towel
    { IMG_INV_OBJ, 66, 0x1fa00, 0x44 }, // dry towel
    { IMG_INV_OBJ, 68, 0x1fb00, 0x38 }, // tin can
    { IMG_INV_OBJ, 69, 0x1fc00, 0x57 }, // open can
    { IMG_INV_OBJ, 70, 0x1fd00, 0x4d }, // dizzy dean spell
    { IMG_INV_OBJ, 71, 0x1fe00, 0x43 }, // piece of metal
    { IMG_INV_OBJ, 72, 0x1ff00, 0x52 }, // lycanthrope spell

    { IMG_ROOM_OBJ, 74, 0x20000, 0x34 }, // old lever in wall
    { IMG_ROOM_OBJ, 75, 0x20100, 0x3b2 }, // rabid rats

    { 0, 0, 0, 0 }
};

static const imglist a2listCount[] = {
    { IMG_ROOM, 0, 0x01000, 0xa0c }, // Too dark to see
    { IMG_ROOM, 1, 0x01b00, 0x8ba }, // *I'm lying in a large brass bed
    { IMG_ROOM, 2, 0x02400, 0x992 }, // bedroom
    { IMG_ROOM, 3, 0x02e00, 0xdb8 }, // *I'm on A ledge outside An open window
    { IMG_ROOM, 4, 0x03c00, 0xa58 }, // *I'm hanging on the end of a sheet, I made a fold in the sheet
    { IMG_ROOM, 5, 0x04700, 0xa94 }, // flower box outside An open window
    { IMG_ROOM, 6, 0x05200, 0x104c }, // CRYPT
    { IMG_ROOM, 7, 0x06300, 0xaa6 }, // closet
    { IMG_ROOM, 8, 0x06e00, 0x8d2 }, // Bathroom
    { IMG_ROOM, 9, 0x07700, 0xc22 }, // *I'm outside the castle
    { IMG_ROOM, 10, 0x08400, 0xa46 }, // DOORLESS room
    { IMG_ROOM, 11, 0x08f00, 0x9c8 }, // hall inside the castle
    { IMG_ROOM, 12, 0x09900, 0xaa8 }, // kitchen
    { IMG_ROOM, 13, 0x0a400, 0x1002 }, // large COFFIN
    { IMG_ROOM, 14, 0x0b500, 0xa3c }, // pAntry
    { IMG_ROOM, 15, 0x0c000, 0xe00 }, // giant SOLAR OVEN
    { IMG_ROOM, 16, 0x0cf00, 0xbe6 }, // Dungeon
    { IMG_ROOM, 17, 0x0db00, 0xf06 }, // Meandering path
    { IMG_ROOM, 18, 0x0eb00, 0x1264 }, // Pit
    { IMG_ROOM, 19, 0x0fe00, 0xfa6 }, // Dark passage
    { IMG_ROOM, 20, 0x10e00, 0x724 }, // dumb-waiter by a room
    { IMG_ROOM, 21, 0x11600, 0x948 }, // workroom
    { IMG_ROOM, 22, 0x12000, 0xd48 }, // LOT OF TROUBLE! (And so Are you!)

    { IMG_ROOM, 90, 0x12e00, 0x93e }, // 38, Closeup package
    { IMG_ROOM, 91, 0x13800, 0x9bc }, // 46, Closeup angry mob
    { IMG_ROOM, 99, 0x14200, 0xf26 }, // Title

    { IMG_INV_OBJ, 0, 0x15200, 0x68 }, // sheets

    { IMG_ROOM_OBJ, 1, 0x15300, 0x24 }, // open window

    { IMG_INV_OBJ, 3, 0x15400, 0x72 }, // pillow
    { IMG_INV_OBJ, 4, 0x15500, 0x4c }, // Coat-of-Arms
    { IMG_ROOM_OBJ, 6, 0x15600, 0x16e }, // Sheet going into window
    { IMG_ROOM_OBJ, 7, 0x15800, 0x15a }, // End of sheet tied to flAgpole
    { IMG_INV_OBJ, 8, 0x15a00, 0x3e }, // 1 nodoz tablet
    { IMG_INV_OBJ, 9, 0x15b00, 0x74 }, // LIT torch
    { IMG_ROOM_OBJ, 13, 0x15c00, 0x160 }, // Open door
    { IMG_INV_OBJ, 15, 0x15e00, 0x54 }, // Paper clip
    { IMG_INV_OBJ, 16, 0x15f00, 0x4c }, // Tent STAKE
    { IMG_INV_OBJ, 17, 0x16000, 0x3e }, // Mirror
    { IMG_INV_OBJ, 18, 0x16100, 0x3c }, // Bottle of type V blood
    { IMG_INV_OBJ, 19, 0x16200, 0x3a }, // Empty bottle
    { IMG_INV_OBJ, 20, 0x16300, 0x40 }, // Unlit torch
    { IMG_INV_OBJ, 21, 0x16400, 0x72 }, // Sulfur mAtches
    { IMG_INV_OBJ, 22, 0x16500, 0x50 }, // 2 small holes in my neck
    { IMG_INV_OBJ, 23, 0x16600, 0x86 }, // 3 no-doz tablets
    { IMG_INV_OBJ, 24, 0x16700, 0x70 }, // 2 nodoz tablets
    { IMG_INV_OBJ, 26, 0x16800, 0x4e }, // Pack of Transylvanian cigarettes
    { IMG_INV_OBJ, 27, 0x16900, 0x5c }, // LIT cigArette
    { IMG_ROOM_OBJ, 29, 0x16a00, 0x2b6 }, // Coffin is open
    { IMG_ROOM_OBJ, 30, 0x16d00, 0x1ea }, // Coffin is closed
    { IMG_INV_OBJ, 31, 0x16f00, 0x2e }, // Dusty clove of garlic
    { IMG_INV_OBJ, 33, 0x17000, 0x1e }, // Cigarette
    { IMG_INV_OBJ, 35, 0x17100, 0xec }, // The other end of the sheet
    { IMG_ROOM_OBJ, 36, 0x17200, 0x124 }, // Sheet tied to bed
    { IMG_INV_OBJ, 37, 0x17400, 0x58 }, // Pocket watch
    { IMG_ROOM_OBJ, 40, 0x17500, 0x20 }, // Broken slide lock
    { IMG_INV_OBJ, 41, 0x17600, 0x68 }, // Large tempered nail file
    { IMG_INV_OBJ, 42, 0x17700, 0x18 }, // Small Vial
    { IMG_INV_OBJ, 45, 0x17800, 0x54 }, // Package
    { IMG_INV_OBJ, 46, 0x17900, 0x4a }, // Empty box
    { IMG_INV_OBJ, 47, 0x17a00, 0x78 }, // Postcard
    { IMG_INV_OBJ, 51, 0x17b00, 0x60 }, // Note
    { IMG_ROOM_OBJ, 52, 0x17c00, 0x40a }, // DRACULA
    { IMG_INV_OBJ, 53, 0x18100, 0x68 }, // Rubber mallet
    { IMG_ROOM_OBJ, 55, 0x18200, 0x136 }, // Sheet tied to ring going into pit
    { IMG_ROOM_OBJ, 56, 0x18400, 0x28c }, // DARK foreboding passage
    { IMG_INV_OBJ, 58, 0x18700, 0x78 }, // Full size portrait of DRACULA
    { IMG_INV_OBJ, 61, 0x18800, 0x84 }, // Daisies
    { IMG_ROOM_OBJ, 63, 0x18900, 0xf4 }, // Dumb-waiter
    { IMG_INV_OBJ, 65, 0x18a00, 0x6e }, // Letter
    { IMG_ROOM_OBJ, 67, 0x18b00, 0x54e }, // Mouldy old skeleton with a stake in the rib cage
    { IMG_INV_OBJ, 70, 0x19100, 0x68 }, // Century worth of dust
    { IMG_ROOM_OBJ, 80, 0x19200, 0x1a8 },  // Mirror in room
    { IMG_ROOM_OBJ, 81, 0x19500, 0x150 },  // other end of sheet
    { IMG_ROOM_OBJ, 82, 0x19700, 0x206 },  // Coat-of-Arms (4) on wall

    { 0, 0, 0, 0 }
};

static const imglist a2listVoodoo[] = {
    { IMG_ROOM, 0, 0x01000, 0x0836 }, // .
    { IMG_ROOM, 1, 0x01900, 0x0a58 }, // chapel
    { IMG_ROOM, 2, 0x02400, 0x0566 }, // Dingy Looking Stairwell
    { IMG_ROOM, 3, 0x02a00, 0x0b08 }, // room in the castle
    { IMG_ROOM, 4, 0x03600, 0x0e14 }, // Tunnel
    { IMG_ROOM, 5, 0x04500, 0x07b4 }, // room in the castle
    { IMG_ROOM, 6, 0x04d00, 0x06bc }, // *I'm in Medium Maegen's Mad Room
    { IMG_ROOM, 7, 0x05400, 0x09e0 }, // room in the castle
    { IMG_ROOM, 8, 0x05e00, 0x0456 }, // room in the castle
    { IMG_ROOM, 9, 0x06300, 0x098e }, // room in the castle
    { IMG_ROOM, 10, 0x06d00, 0x0784 }, // Ballroom
    { IMG_ROOM, 11, 0x07500, 0x06ca }, // dungeon
    { IMG_ROOM, 12, 0x07c00, 0x0dea }, // *I'm in the Armory
    { IMG_ROOM, 13, 0x08a00, 0x085c }, // torture chamber
    { IMG_ROOM, 14, 0x09300, 0x0e24 }, // Chimney
    { IMG_ROOM, 15, 0x0a200, 0x0e52 }, // large fireplace
    { IMG_ROOM, 16, 0x0b100, 0x0bb4 }, // room in the castle
    { IMG_ROOM, 17, 0x0bd00, 0x0bc4 }, // Lab
    { IMG_ROOM, 18, 0x0c900, 0x0afe }, // narrow part of the chimney
    { IMG_ROOM, 19, 0x0d400, 0x0fcc }, // Graveyard
    { IMG_ROOM, 20, 0x0e400, 0x0720 }, // parlor
    { IMG_ROOM, 21, 0x0ec00, 0x0cf8 }, // Jail Cell
    { IMG_ROOM, 22, 0x0f900, 0x060c }, // *I'm on a ledge
    { IMG_ROOM, 23, 0x10000, 0x0eac }, // hidden VOODOO room
    { IMG_ROOM, 24, 0x10f00, 0x061c }, // room in the castle
    { IMG_ROOM, 25, 0x11600, 0x0b66 }, // lot of TROUBLE!

    { IMG_ROOM, 90, 0x15400, 0x57c }, // Closeup broken sword
    { IMG_ROOM, 91, 0x12200, 0x4dc }, // Closeup voodoo doll
    { IMG_ROOM, 92, 0x12700, 0x788 }, // Closeup voodoo book
    { IMG_ROOM, 93, 0x12f00, 0x528 }, // Closeup ring
    { IMG_ROOM, 94, 0x13500, 0x5f2 }, // Closeup ju-ju statue
    { IMG_ROOM, 95, 0x13b00, 0xbbc }, // Closeup glowing idol
    { IMG_ROOM, 96, 0x14700, 0x7c2 }, // Closeup chemicals
    { IMG_ROOM, 97, 0x14f00, 0x498 }, // Closeup bloody knife
    { IMG_ROOM, 101, 0x15a00, 0xd8e }, // Closeup Count Cristo

    { IMG_ROOM, 99, 0x16800, 0xdfe }, // Title

    { IMG_INV_OBJ, 0, 0x17600, 0x46 }, // Bloody Knife
    { IMG_INV_OBJ, 2, 0x17700, 0x48 }, // Plaque
    { IMG_INV_OBJ, 3, 0x17800, 0xa2 }, // Animal heads
    { IMG_INV_OBJ, 4, 0x17900, 0x4a }, // Broken glass
    { IMG_ROOM_OBJ, 6, 0x17a00, 0x192 }, // Dark hole
    { IMG_INV_OBJ, 7, 0x17c00, 0xec }, // Shield
    { IMG_INV_OBJ, 9, 0x17d00, 0x8a }, // Brightly glowing idol
    { IMG_ROOM_OBJ, 10, 0x17e00, 0x2c2 }, // Open Window
    { IMG_ROOM_OBJ, 13, 0x18100, 0x19c }, // Open Flue
    { IMG_INV_OBJ, 14, 0x18300, 0x84 }, // Four leaf clover
    { IMG_INV_OBJ, 16, 0x18400, 0x6e }, // Cast iron pot
    { IMG_ROOM_OBJ, 17, 0x18500, 0x122 }, // Closed Coffin
    { IMG_INV_OBJ, 21, 0x18700, 0x38 }, // Soot
    { IMG_ROOM_OBJ, 23, 0x18800, 0x2d4 }, // Spirit Medium
    { IMG_INV_OBJ, 25, 0x18b00, 0x68 }, // Sapphire ring
    { IMG_INV_OBJ, 26, 0x18c00, 0xb0 }, // Ju-Ju bag
    { IMG_ROOM_OBJ, 28, 0x18d00, 0x5cc }, // Slick chute leading downward
    { IMG_ROOM_OBJ, 31, 0x19300, 0xf8 }, // Wide crack in the wall
    { IMG_INV_OBJ, 32, 0x19400, 0x82 }, // Rabbit's foot
    { IMG_INV_OBJ, 33, 0x19500, 0x64 }, // Dull & broken sword
    { IMG_ROOM_OBJ, 34, 0x19600, 0x12a }, // Open SAfe
    { IMG_ROOM_OBJ, 35, 0x19800, 0x810 }, // Stuck Chimney Sweep
    { IMG_INV_OBJ, 36, 0x1a100, 0x44 }, // Chem tubes
    { IMG_ROOM_OBJ, 37, 0x1a200, 0x168 }, // Closed SAfe
    { IMG_INV_OBJ, 38, 0x1a400, 0x7a }, // Labeled chemicals
    { IMG_INV_OBJ, 39, 0x1a500, 0x3c }, // Pocket Shovel
    { IMG_INV_OBJ, 40, 0x1a600, 0x8a }, // Mixed Chemicals
    { IMG_ROOM_OBJ, 41, 0x1a700, 0xbc }, // Wide open door
    { IMG_INV_OBJ, 42, 0x1a800, 0x52 }, // Nails
    { IMG_INV_OBJ, 43, 0x1a900, 0x80 }, // Dusty Idol
    { IMG_INV_OBJ, 44, 0x1aa00, 0x8a }, // Doll
    { IMG_INV_OBJ, 45, 0x1ab00, 0x8c }, // Wooden boards
    { IMG_INV_OBJ, 47, 0x1ac00, 0x36 }, // Antique hammer
    { IMG_ROOM_OBJ, 48, 0x1ad00, 0xfe }, // GrAting
    { IMG_INV_OBJ, 49, 0x1ae00, 0x4a }, // Rusting SAW
    { IMG_ROOM_OBJ, 50, 0x1af00, 0x1e2 }, // Button in the wall
    { IMG_INV_OBJ, 51, 0x1b100, 0x54 }, // Paper
    { IMG_INV_OBJ, 52, 0x1b200, 0x52 }, // Voodoo book
    { IMG_INV_OBJ, 53, 0x1b300, 0xb0 }, // Ju-Ju man statue
    { IMG_INV_OBJ, 54, 0x1b400, 0x5c }, // Stick
    { IMG_INV_OBJ, 59, 0x1b500, 0x3a }, // Broken grating
    { IMG_INV_OBJ, 60, 0x1b600, 0x38 }, // Advertising leaflet
    { IMG_INV_OBJ, 63, 0x1b700, 0x62 }, // Pieces of rock
    { IMG_INV_OBJ, 64, 0x1b800, 0x48 }, // Page torn from a book
    { IMG_ROOM_OBJ, 65, 0x1b900, 0x36c }, // Smiling Count Cristo

    { 0, 0, 0, 0 }
};

#define kDiskImageSize 143360

static uint8_t *GetApple2CompanionFile(size_t *size, int *isnib);
static int ExtractImagesFromApple2CompanionFile(uint8_t *data, size_t datasize, int isnib);

int DetectApple2(uint8_t **sf, size_t *extent)
{
    if (*extent > MAX_LENGTH || *extent < kDiskImageSize)
        return 0;

    if ((*sf)[0] == 'W' && (*sf)[1] == 'O' && (*sf)[2] == 'Z') {
        uint8_t *result = woz2nib(*sf, extent);
        if (result) {
            free(*sf);
            *sf = result;
            InitNibImage(*sf, *extent);
        }
    } else {
        InitDskImage(*sf, *extent);
    }

    uint8_t *invimg = NULL, *m2 = NULL;
    size_t invimgsiz;
    int isnib;

    size_t companionsize;
    uint8_t *companionfile = GetApple2CompanionFile(&companionsize, &isnib);

    uint8_t *datafile = ReadApple2DOSFile(*sf, extent, &invimg, &invimgsiz, &m2);

    if (!datafile && companionfile != NULL) {
        FreeDiskImage();
        if (isnib)
            InitNibImage(companionfile, companionsize);
        else
            InitDskImage(companionfile, companionsize);
        datafile = ReadApple2DOSFile(companionfile, &companionsize, &invimg, &invimgsiz, &m2);
        if (datafile) {
            uint8_t *temp = companionfile;
            size_t tempsize = companionsize;
            companionfile = *sf;
            companionsize = *extent;
            *sf = temp;
            *extent = tempsize;
        }
    }

    FreeDiskImage();

    if (invimg) {
        if (!USImages)
            USImages = new_image();
        USImages->index = 98;
        USImages->systype = SYS_APPLE2;
        USImages->datasize = invimgsiz;
        USImages->imagedata = invimg;
        USImages->usage = IMG_ROOM;
    }

    if (m2) {
        descrambletable = m2;
    }

    uint8_t *database = NULL;
    size_t newlength = 0;


    if (datafile) {
        size_t data_start = 0x135;

        // Header actually starts at offset 0x016d (0x135 + 0x38).
        // We add 50 bytes before the head to match the C64 files.

        if (*extent > MAX_LENGTH || *extent < data_start)
            return 0;

        newlength = *extent - data_start;
        database = MemAlloc(newlength);
        memcpy(database, datafile + data_start, newlength);
    } else {
        debug_print("Failed loading database\n");
        return 0;
    }

    if (database) {
        int result = LoadBinaryDatabase(database, newlength, *Game, 0);
        if (!result && newlength > 0x3d00) {
            result = LoadBinaryDatabase(database + 0x3d00, newlength - 0x3d00, *Game, 0);
        }

        if (!result && newlength > 0x3803) {
            result = LoadBinaryDatabase(database + 0x3803, newlength - 0x3803, *Game, 0);
        }

        if (result) {
            CurrentSys = SYS_APPLE2;

            ImageWidth = 280;
            ImageHeight = 160;

            if (CurrentGame == CLAYMORGUE_US_126) {
                if (!USImages)
                    USImages = new_image();
                USImages->index = 98;
                USImages->systype = SYS_APPLE2;
                USImages->datasize = 0x4f8;
                USImages->imagedata = MemAlloc(USImages->datasize);
                memcpy(USImages->imagedata, datafile + 0x7e84, USImages->datasize);
                USImages->usage = IMG_ROOM;
            }
            if (companionfile != NULL) {
                ExtractImagesFromApple2CompanionFile(companionfile, companionsize, isnib);
                free(companionfile);
            } else if (USImages != NULL) {
                free (USImages->imagedata);
                free (USImages);
                USImages = NULL;
            }
        } else
            debug_print("Failed loading database\n");
        free (datafile);
        free (database);
        return result;
    } else {
        free (datafile);
        debug_print("Failed loading database\n");
        return 0;
    }
}

static int StripParens(char sideA[], size_t length) {
    int left_paren = 0;
    int right_paren = 0;
    size_t ppos = length - 1;
    while(sideA[ppos] != '.' && ppos > 0)
        ppos--;
    size_t extlen = length - ppos;
    if (length > 4) {
        for (int i = (int)ppos; i > 0; i--) {
            char c = sideA[i];
            if (c == ')') {
                if (right_paren == 0) {
                    right_paren = i;
                } else {
                    return 0;
                }
            } else if (c == '(') {
                if (right_paren > 0) {
                    if (i > 0 && sideA[i - 1] == ' ')
                        i--;
                    left_paren = i;
                    break;
                } else {
                    return 0;
                }
            }
        }
        if (right_paren && left_paren && length > right_paren + extlen) {
            right_paren++;
            for (int i = 0; i < extlen; i++) {
                sideA[left_paren++] = sideA[right_paren++];
            }
            sideA[left_paren] = '\0';
            return 1;
        }
    }
    return 0;
}

uint8_t *ReadA2DiskImageFile(const char *filename, size_t *filesize, int *isnib) {
    uint8_t *result = ReadFileIfExists(filename, filesize);
    if (result && *filesize > 4 && result[0] == 'W' && result[1] == 'O' && result[2] == 'Z') {
        uint8_t *result2 = woz2nib(result, filesize);
        if (result2) {
            free(result);
            result = result2;
            *isnib = 1;
        }
    }
    return result;
}

uint8_t *LookForA2CompanionFilename(int index, CompanionNameType type, size_t stringlen, size_t *filesize, int *isnib) {

    char *sideB = MemAlloc(stringlen + 9);
    uint8_t *result = NULL;

    *isnib = 0;
    memcpy(sideB, game_file, stringlen + 1);
    switch(type) {
        case TYPE_A:
            sideB[index] = 'A';
            break;
        case TYPE_B:
            sideB[index] = 'B';
            break;
        case TYPE_1:
            sideB[index] = '1';
            break;
        case TYPE_2:
            sideB[index] = '2';
            break;
        case TYPE_ONE:
            sideB[index] = 'o';
            sideB[index + 1] = 'n';
            sideB[index + 2] = 'e';
            break;
        case TYPE_TWO:
            sideB[index] = 't';
            sideB[index + 1] = 'w';
            sideB[index + 2] = 'o';
            break;
        case TYPE_NONE:
            break;
    }

    debug_print("looking for companion file \"%s\"\n", sideB);
    result = ReadA2DiskImageFile(sideB, filesize, isnib);
    if (!result) {
        if (type == TYPE_A) {
            if (StripParens(sideB, stringlen)) {
                debug_print("looking for companion file \"%s\"\n", sideB);
                result = ReadA2DiskImageFile(sideB, filesize, isnib);
            }
        } else if (type == TYPE_B) {

            // First we look for the period before the file extension
            size_t ppos = stringlen - 1;
            while(sideB[ppos] != '.' && ppos > 0)
                ppos--;
            if (ppos < 1) {
                free(sideB);
                return NULL;
            }
            // Then we copy the extension to the new end position
            for (size_t i = ppos; i <= stringlen; i++) {
                sideB[i + 7] = sideB[i];
            }
            sideB[ppos++] = ' ';
            sideB[ppos++] = '(';
            sideB[ppos++] = 'b';
            sideB[ppos++] = 'o';
            sideB[ppos++] = 'o';
            sideB[ppos++] = 't';
            sideB[ppos] = ')';
            debug_print("looking for companion file \"%s\".\n", sideB);
            result = ReadA2DiskImageFile(sideB, filesize, isnib);
        }
    }
    free(sideB);
    return result;
}

uint8_t *GetApple2CompanionFile(size_t *size, int *isnib) {

    *size = 0;
    *isnib = 0;

    size_t gamefilelen = strlen(game_file);
    uint8_t *result = NULL;

    char *foundname = LookInDatabase(a2companionlist, gamefilelen);
    if (foundname) {
        size_t filesize;
        result = ReadA2DiskImageFile(foundname, &filesize, isnib);
        free((void *)foundname);
        if (result)
            return result;
    }

    char c;
    for (int i = (int)gamefilelen - 1; i >= 0 && game_file[i] != '/' && game_file[i] != '\\'; i--) {
        c = tolower(game_file[i]);
        if (i > 3 && ((c == 'e' && game_file[i - 1] == 'd' && game_file[i - 2] == 'i' && tolower(game_file[i - 3]) == 's') ||
                      (c == 'k' && game_file[i - 1] == 's' && game_file[i - 2] == 'i' && tolower(game_file[i - 3]) == 'd'))) {
            if (gamefilelen > i + 2) {
                c = game_file[i + 1];
                if (c == ' ' || c == '_') {
                    c = tolower(game_file[i + 2]);
                    CompanionNameType type = TYPE_NONE;
                    switch (c) {
                        case 'a':
                            type = TYPE_B;
                            break;
                        case 'b':
                            type = TYPE_A;
                            break;
                        case 't':
                            if (gamefilelen > i + 4 && game_file[i + 3] == 'w' && game_file[i + 4] == 'o') {
                                type =  TYPE_ONE;
                            }
                            break;
                        case 'o':
                            if (gamefilelen > i + 4 && game_file[i + 3] == 'n' && game_file[i + 4] == 'e') {
                                type = TYPE_TWO;
                            }
                            break;
                        case '2':
                            type= TYPE_1;
                            break;
                        case '1':
                            type = TYPE_2;
                            break;
                    }
                    if (type != TYPE_NONE)
                        result = LookForA2CompanionFilename(i + 2, type, gamefilelen, size, isnib);
                    if (result)
                        return result;
                }
            }
        }
    }
    return NULL;
}

static int ExtractImagesFromApple2CompanionFile(uint8_t *data, size_t datasize, int isnib)
{
    int outpic;

    const struct imglist *list;


    switch(CurrentGame) {
        case CLAYMORGUE_US:
            list = a2listClaymorgue;
            break;
        case CLAYMORGUE_US_126:
            list = a2listClaymorgue126;
            break;
        case HULK_US:
            list = a2listHulk;
            break;
        case HULK_US_PREL:
            list = a2listHulk126;
            break;
        case COUNT_US:
            list = a2listCount;
            break;
        case VOODOO_CASTLE_US:
            list = a2listVoodoo;
            break;
        default:
            return 0;
    }

    struct USImage *image = new_image();
    if (!USImages) {
        USImages = image;
    } else {
        USImages->next = image;
    }

    if (isnib) {
        InitNibImage(data, datasize);
    }
    // Now loop round for each image
    for (outpic = 0; list[outpic].offset != 0; outpic++)
    {
        size_t size = list[outpic].size + 4;

        image->usage = list[outpic].usage;
        image->index = list[outpic].index;

        //        debug_print("Reading image %d with size %zu and index %d\n", outpic, size, image->index);

        image->datasize = size;
        image->systype = SYS_APPLE2;

        if (isnib) {
            image->imagedata = ReadImageFromNib(list[outpic].offset, size, data, datasize);
        } else {
            image->imagedata = MemAlloc(size);
            memcpy(image->imagedata, data + list[outpic].offset, size);
        }

        image->next = new_image();
        image->next->previous = image;
        image = image->next;
    }

    FreeDiskImage();

    if (USImages->next == NULL && USImages->imagedata == NULL) {
        free(USImages);
        USImages = NULL;
    }

    if (CurrentGame == HULK_US_PREL)
        CurrentGame = HULK_US;
    else if (CurrentGame == CLAYMORGUE_US_126)
        CurrentGame = CLAYMORGUE_US;
    return 1;
}
