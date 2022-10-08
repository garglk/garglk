//
//  atari8detect.c
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "scott.h"
#include "saga.h"
#include "sagagraphics.h"
#include "scottdefines.h"
#include "hulk.h"

#include "scottgameinfo.h"
#include "atari8c64draw.h"
#include "atari8detect.h"

static const pairrec a8companionlist[][2] = {
    {{ 0x16810, 0xa972, "S.A.G.A. 01 - Adventureland v5.0-416 (1982)(Adventure International)(US)(Side A)[!].atr", 87}, { 0x16810, 0x8be3, "S.A.G.A. 01 - Adventureland v5.0-416 (1982)(Adventure International)(US)(Side B)[!][cr CSS].atr", 95 }},
    {{ 0x16810, 0x65a1, "S.A.G.A. 02 - Pirate Adventure v5.0-408 (1982)(Adventure International)(US)(Side A)[f][m].atr", 93 }, { 0x16810, 0x5750, "S.A.G.A. 02 - Pirate Adventure v5.0-408 (1982)(Adventure International)(US)(Side B)[cr CSS].atr", 95 }},
    {{ 0x16810, 0x3074, "S.A.G.A. 02 - Pirate Adventure v5.0-408 (1982)(Adventure International)(US)(Side A)[f][a].atr", 93 }, { 0x16810, 0x2429, "S.A.G.A. 02 - Pirate Adventure v5.0-408 (1982)(Adventure International)(US)(Side B)[a][cr CSS].atr", 98 }},
    {{ 0x16810, 0x4389, "S.A.G.A. 04 - Voodoo Castle v5.1-119 (1983)(Adventure International)(US)(Disk 1 of 2)[!].atr", 92 }, { 0x16810, 0x234f, "S.A.G.A. 04 - Voodoo Castle v5.1-119 (1983)(Adventure International)(US)(Disk 2 of 2)[!][cr CSS].atr", 100 }},
    {{ 0x16810, 0xc2f5, "S.A.G.A. 05 - The Count v5.1-115 (1983)(Adventure International)(US)(Side A)[!].atr", 83}, { 0x16810, 0x3ebb, "S.A.G.A. 05 - The Count v5.1-115 (1983)(Adventure International)(US)(Side B)[!][cr CSS].atr", 91 }},
    {{ 0x16810, 0x6ee8, "S.A.G.A. 13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 1 of 2)[!][cr CSS].atr", 120 }, { 0x16810, 0xac42, "S.A.G.A. 13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 2 of 2)[f][!].atr", 115 }},
    {{ 0x16810, 0x1de8, "S.A.G.A. 13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 1 of 2)[a][cr CSS].atr", 120 }, { 0x16810, 0x7c32, "S.A.G.A. 13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 2 of 2)[a].atr", 112 }},
    {{ 0x16810, 0x1de8, "S.A.G.A. #13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 1 of 2)[a][cr CSS].atr", 121 }, { 0x16810, 0x7c32, "S.A.G.A. #13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 2 of 2)[a].atr", 113 }},

    {{ 0,0, NULL }, { 0,0, NULL }}

};

typedef struct imglist {
    USImageType usage;
    int index;
    size_t offset;
} imglist;

static const struct imglist listHulk[] = {
    { IMG_ROOM, 0, 0x297 }, // (0) Too dark
    { IMG_ROOM, 1, 0x2479 }, // (1) *I'm Bruce Banner, tied hand & foot to a chair
    { IMG_ROOM, 2, 0x71e }, // (2) dome
    { IMG_ROOM, 3, 0x2a6b }, // (3) tunnel going outside
    { IMG_ROOM, 4, 0xd17 }, // (4) field
    { IMG_ROOM, 9, 0x2ef2 }, // (5) hole
    { IMG_ROOM, 12, 0x1325 }, // (6) cavern
    { IMG_ROOM, 15, 0x381e }, // (7) small underground room
    { IMG_ROOM, 16, 0x4064 }, // (8) fuzzy area
    { IMG_ROOM, 19, 0x4bac }, // (9) Chief Examiner working
    { IMG_ROOM, 20, 0x5256 }, // (10) state of limbo
    { IMG_ROOM, 81, 0x5ee4 }, // (11)
    { IMG_ROOM, 82, 0x695d }, // (12)
    { IMG_ROOM, 83, 0x7207 }, // (13)
    { IMG_ROOM, 84, 0x8deb }, // (14)
    { IMG_ROOM, 85, 0x7a8e }, // (15)
    { IMG_ROOM, 86, 0x9deb }, // (16) Gas
    { IMG_ROOM, 87, 0x8717 }, // (17) Victory
    { IMG_ROOM, 88, 0xac90 }, // (18) Nightmares
    { IMG_ROOM, 89, 0xba2c }, // (19) Digging
    { IMG_ROOM, 90, 0xcb00 }, // (20) Dome with mesh
    { IMG_ROOM, 91, 0xd497 }, // (21) Closeup bio gem
    { IMG_ROOM, 92, 0xdea5 }, // (22) Closeup egg
    { IMG_ROOM, 93, 0x10479 }, // (23) Closeup ant
    { IMG_ROOM, 94, 0xe7c1 }, // (24) Closeup bees
    { IMG_ROOM, 95, 0x10df2 }, // (25) Closeup ring
    { IMG_ROOM, 96, 0xf5a5 }, // (26) Closeup Ant-Man
    { IMG_ROOM, 97, 0x1146b }, // (27) Closeup outlet
    { IMG_ROOM, 99, 0x11a33 }, // (28) Title
    { IMG_ROOM_OBJ, 13, 0x12700 }, // (29) Large hole
    { IMG_ROOM_OBJ, 20, 0x127ba }, // (30) Alien army ants
    { IMG_ROOM_OBJ, 24, 0x12e0e }, // (31) Killer Bees
    { IMG_ROOM_OBJ, 53, 0x12ea5 }, // (32) Sign
    { IMG_ROOM_OBJ, 26, 0x12f0e }, // (33) Astral projection of Dr. Strange
    { IMG_ROOM_OBJ, 17, 0x13b8e }, // (34) Natter energy egg
    { IMG_ROOM_OBJ, 22, 0x13d17 }, // (35) Tiny holes
    { IMG_ROOM_OBJ, 25, 0x13fdd }, // (36) Wire mesh in wall with bee size holes
    { IMG_ROOM_OBJ, 33, 0x14100 }, // (37) Large iron ring set in floor
    { IMG_ROOM_OBJ, 36, 0x141a5 }, // (38) Button in wall
    { IMG_ROOM_OBJ, 34, 0x1420e }, // (39) Open hole in floor
    { IMG_ROOM_OBJ, 39, 0x1440e }, // (40) Hole in ceiling
    { IMG_ROOM_OBJ, 40, 0x145c1 }, // (41) Crack in floor
    { IMG_ROOM_OBJ, 45, 0x1469e }, // (42) ULTRON
    { IMG_ROOM_OBJ, 70, 0x1508e }, // (43) Bio gem in room
    { IMG_ROOM_OBJ, 72, 0x15307 }, // (44) Wax in room
    { IMG_INV_OBJ, 4, 0x15356 }, // (45) *Gem 1
    { IMG_INV_OBJ, 12, 0x15397 }, // (46) *Gem 2
    { IMG_INV_OBJ, 16, 0x153dd }, // (47) *Gem 3
    { IMG_INV_OBJ, 19, 0x1541e }, // (48) *Gem 4
    { IMG_INV_OBJ, 29, 0x15464 }, // (49) *Gem 5
    { IMG_INV_OBJ, 30, 0x154a5 }, // (50) *Gem 6
    { IMG_INV_OBJ, 31, 0x154eb }, // (51) *Gem 7
    { IMG_INV_OBJ, 32, 0x1552c }, // (52) *Gem 8
    { IMG_INV_OBJ, 35, 0x15572 }, // (53) *Gem 9
    { IMG_INV_OBJ, 37, 0x155b3 }, // (54) *Gem 10
    { IMG_INV_OBJ, 42, 0x155f9 }, // (55) *Gem 11
    { IMG_INV_OBJ, 44, 0x1563a }, // (56) *Gem 12
    { IMG_INV_OBJ, 46, 0x15680 }, // (57) *Gem 13
    { IMG_INV_OBJ, 48, 0x156c1 }, // (58) Gem 14
    { IMG_INV_OBJ, 50, 0x15707 }, // (59) *Gem 15
    { IMG_INV_OBJ, 2, 0x15748 }, // (60) Mirror
    { IMG_INV_OBJ, 21, 0x157ba }, // (61) Wax
    { IMG_INV_OBJ, 1, 0x15807 }, // (62) Rage
    { IMG_INV_OBJ, 18, 0x15cc8 }, // (63) *Bio gem
    { IMG_INV_OBJ, 3, 0x15da5 }, // (64) Broken chair
    { IMG_INV_OBJ, 23, 0x15ea5 }, // (65) Metal Hand Fan
    { IMG_ROOM_OBJ, 250, 0x15f3a }, // (66) // color test
    { IMG_INV_OBJ, 51, 0x15f79 }, // (67) *Gem 16
    { IMG_ROOM_OBJ, 47, 0x15fba }, // (68) Small cage
    { 0, 0, 0, }
};

// "R0198", 0, Disk image A at offset 0x9890

static const struct imglist listClaymorgue[] = {
    { IMG_INV_OBJ, 5, 0x297 }, // (0) *STAR
    { IMG_INV_OBJ, 6, 0x2c1 }, // (1) *STAR
    { IMG_INV_OBJ, 7, 0x2e4 }, // (2) *STAR
    { IMG_INV_OBJ, 8, 0x307 }, // (3) *STAR
    { IMG_INV_OBJ, 9, 0x32c }, // (4) I'm glowing
    { IMG_INV_OBJ, 10, 0x3b3 }, // (5) *STAR
    { IMG_INV_OBJ, 11, 0x3d6 }, // (6) *STAR
    { IMG_INV_OBJ, 12, 0x400 }, // (7) *STAR
    { IMG_INV_OBJ, 13, 0x42c }, // (8) *STAR
    { IMG_INV_OBJ, 14, 0x456 }, // (9) *STAR
    { IMG_INV_OBJ, 15, 0x479 }, // (10) *STAR
    { IMG_INV_OBJ, 16, 0x49e }, // (11) *STAR
    { IMG_INV_OBJ, 17, 0x4c1 }, // (12) *STAR
    { IMG_INV_OBJ, 20, 0x4eb }, // (13) Magic mirror
    { IMG_INV_OBJ, 24, 0x52c }, // (14) Fire spell
    { IMG_INV_OBJ, 33, 0x580 }, // (15) Spell of Methuselah
    { IMG_INV_OBJ, 34, 0x5cf }, // (16) Seed spell
    { IMG_INV_OBJ, 35, 0x61e }, // (17) Potion
    { IMG_INV_OBJ, 36, 0x66b }, // (18) Light squared spell
    { IMG_INV_OBJ, 40, 0x6b3 }, // (19) Permeability spell
    { IMG_INV_OBJ, 41, 0x707 }, // (20) Spell of Bliss
    { IMG_INV_OBJ, 42, 0x74f }, // (21) Piece of wood
    { IMG_INV_OBJ, 45, 0x7ac }, // (22) Yoho spell
    { IMG_INV_OBJ, 48, 0x800 }, // (23) Wooden crate
    { IMG_INV_OBJ, 50, 0x864 }, // (24) Fire bricks
    { IMG_INV_OBJ, 57, 0x8c1 }, // (25) Firefly spell
    { IMG_INV_OBJ, 59, 0x910 }, // (26) Dust
    { IMG_INV_OBJ, 61, 0x93a }, // (27) Wicked Queen's spell
    { IMG_INV_OBJ, 62, 0x98e }, // (28) Soggy towel
    { IMG_INV_OBJ, 63, 0x9f2 }, // (29) Ashes
    { IMG_INV_OBJ, 64, 0xa33 }, // (30) Unravel spell
    { IMG_INV_OBJ, 68, 0xa87 }, // (31) Tin can
    { IMG_INV_OBJ, 70, 0xac8 }, // (32) Dizzy Dean spell
    { IMG_INV_OBJ, 71, 0xb17 }, // (33) Piece of metal
    { IMG_INV_OBJ, 73, 0xb56 }, // (34) Lycanthrope spell
    { IMG_INV_OBJ, 28, 0xba5 }, // (35) Broken glass
    { IMG_INV_OBJ, 51, 0xbf2 }, // (36) Water droplets
    { IMG_INV_OBJ, 52, 0xc97 }, // (37) Wrinkles
    { IMG_INV_OBJ, 65, 0xe80 }, // (38) Damp towel
    { IMG_INV_OBJ, 66, 0xee4 }, // (39) Dry towel
    { IMG_INV_OBJ, 69, 0xf33 }, // (40) Open can
    { IMG_ROOM_OBJ, 26, 0xf87 }, // (41) Fallen Chandelier
    { IMG_ROOM_OBJ, 38, 0x12b3 }, // (42) Loft
    { IMG_ROOM_OBJ, 30, 0x1e17 }, // (43) Open door
    { IMG_ROOM_OBJ, 32, 0x1fba }, // (44) Open door
    { IMG_ROOM_OBJ, 75, 0x211e }, // (45) Rabid rats
    { IMG_ROOM_OBJ, 39, 0x232c }, // (46) Moat bottom
    { IMG_ROOM_OBJ, 54, 0x28eb }, // (47) Open door
    { IMG_ROOM_OBJ, 55, 0x2bc1 }, // (48) Raised drawbridge
    { IMG_ROOM_OBJ, 4, 0x2f0e }, // (49) Lowered drawbridge
    { IMG_ROOM_OBJ, 60, 0x3000 }, // (50) Hole
    { IMG_ROOM, 29, 0x3641 }, // (51) dragon's lair
    { IMG_ROOM, 99, 0x4797 }, // (52) title
    { IMG_ROOM_OBJ, 49, 0x51c8 }, // (53) Hole
    { IMG_ROOM_OBJ, 47, 0x549e }, // (54) Hole
    { IMG_ROOM_OBJ, 250, 0x5580 }, // (55) Color test
    { IMG_INV_OBJ, 0, 0x55c1 }, // (56) *STAR
    { IMG_ROOM_OBJ, 74, 0x55eb }, // (57) Old lever in wall
    { IMG_ROOM, 0, 0x5617 }, // (58) Too dark to see
    { IMG_ROOM, 1, 0x57ba }, // (59) field
    { IMG_ROOM, 2, 0x628e }, // (60) *I'm on a Drawbridge
    { IMG_ROOM, 3, 0x68cf }, // (61) courtyard
    { IMG_ROOM, 4, 0x7864 }, // (62) magic fountain
    { IMG_ROOM, 5, 0x8297 }, // (63) stream of lava
    { IMG_ROOM, 6, 0x8c33 }, // (64) *I'm on top of the fountain
    { IMG_ROOM, 7, 0x95dd }, // (65) ball room
    { IMG_ROOM, 8, 0x9cb3 }, // (66) *I'm on a large chandelier
    { IMG_ROOM, 9, 0xa61e }, // (67) forest of enchantment
    { IMG_ROOM, 10, 0xa95d }, // (68) plain room
    { IMG_ROOM, 11, 0xad72 }, // (69) storeroom
    { IMG_ROOM, 12, 0xb4ac }, // (70) dungeon cell
    { IMG_ROOM, 13, 0xbd33 }, // (71) anteroom
    { IMG_ROOM, 14, 0xc39e }, // (72) staircase
    { IMG_ROOM, 15, 0xc8d6 }, // (73) loft above ballroom
    { IMG_ROOM, 16, 0xcf4f }, // (74) *I'm in the water of a moat
    { IMG_ROOM, 17, 0xdbf2 }, // (75) *I'm underwater
    { IMG_ROOM, 18, 0xdc48 }, // (76) *I'm under the stairs
    { IMG_ROOM, 19, 0xeaba }, // (77) hollow tree
    { IMG_ROOM, 20, 0xfd17 }, // (78) pool of dirty water
    { IMG_ROOM, 21, 0x1029e }, // (79) kitchen
    { IMG_ROOM, 22, 0x10aa5 }, // (80) *I'm on a box
    { IMG_ROOM, 23, 0x10f3a }, // (81) box
    { IMG_ROOM, 24, 0x116ac }, // (82) dusty room
    { IMG_ROOM, 25, 0x1228e }, // (83) stone staircase
    { IMG_ROOM, 26, 0x1260e }, // (84) damp cavern
    { IMG_ROOM, 27, 0x135f2 }, // (85) stone grotto
    { IMG_ROOM, 28, 0x14048 }, // (86) *I'm in an entryway
    { IMG_ROOM, 30, 0x14c97 }, // (87) wizard's workshop
    { IMG_ROOM, 31, 0x158b3 }, // (88) vacant room
    { IMG_ROOM, 32, 0x15ccf }, // (89) real mess!
    {0,0,0}
};

static const struct imglist listCount[] = {
    { IMG_INV_OBJ, 0, 0x297 }, // (0) Sheets
    { IMG_INV_OBJ, 16, 0x310 }, // (1) Tent STAKE
    { IMG_INV_OBJ, 26, 0x36b }, // (2) Pack of cigarettes
    { IMG_INV_OBJ, 19, 0x3e4 }, // (3) Empty bottle
    { IMG_INV_OBJ, 45, 0x425 }, // (4) Package
    { IMG_INV_OBJ, 42, 0x49e }, // (5) Small Vial
    { IMG_INV_OBJ, 47, 0x4c8 }, // (6) Postcard
    { IMG_INV_OBJ, 3, 0x4f9 }, // (7) Pillow
    { IMG_INV_OBJ, 17, 0x58e }, // (8) Mirror
    { IMG_INV_OBJ, 27, 0x5dd }, // (9) LIT cigarette
    { IMG_INV_OBJ, 31, 0x656 }, // (10) Dusty clove of garlic
    { IMG_INV_OBJ, 5, 0x697 }, // (11) Coat-of-Arms
    { IMG_INV_OBJ, 18, 0x6f2 }, // (12) Bottle of type V blood
    { IMG_INV_OBJ, 46, 0x73a }, // (13) Empty box
    { IMG_INV_OBJ, 33, 0x7a5 }, // (14) Cigarette
    { IMG_INV_OBJ, 8, 0x7eb }, // (15) 1 nodoz tablet
    { IMG_INV_OBJ, 21, 0x848 }, // (16) Sulfur matches
    { IMG_INV_OBJ, 20, 0x8a5 }, // (17) Unlit torch
    { IMG_INV_OBJ, 51, 0x900 }, // (18) Note
    { IMG_INV_OBJ, 61, 0x997 }, // (19) Daisies
    { IMG_INV_OBJ, 9, 0xa4f }, // (20) LIT torch
    { IMG_INV_OBJ, 24, 0xadd }, // (21) 2 nodoz tablets
    { IMG_INV_OBJ, 37, 0xb56 }, // (22) Pocket watch
    { IMG_INV_OBJ, 15, 0xbc8 }, // (23) Paper clip
    { IMG_INV_OBJ, 23, 0xc2c }, // (24) 3 no-doz tablets
    { IMG_INV_OBJ, 41, 0xceb }, // (25) Large tempered nail file
    { IMG_INV_OBJ, 53, 0xd72 }, // (26) Rubber mallet
    { IMG_INV_OBJ, 65, 0xdf9 }, // (27) Letter
    { IMG_INV_OBJ, 58, 0xe90 }, // (28) portrait of DRACULA
    { IMG_INV_OBJ, 70, 0xf2c }, // (29) Century worth of dust
    { IMG_INV_OBJ, 35, 0xfac }, // (30) The other end of the sheet
    { IMG_ROOM_OBJ, 6, 0x10c8 }, // (31) Sheet going into window
    { IMG_ROOM_OBJ, 7, 0x116b }, // (32) sheet tied to flagpole
    { IMG_ROOM_OBJ, 13, 0x12ba }, // (33) Open door
    { IMG_ROOM_OBJ, 82, 0x13f9 }, // (34) Coat-of-arms in room
    { IMG_ROOM_OBJ, 40, 0x168e }, // (35) Broken slide lock
    { IMG_ROOM_OBJ, 52, 0x16b3 }, // (36) DRACULA
    { IMG_ROOM_OBJ, 55, 0x1b48 }, // (37) Sheet tied to ring
    { IMG_ROOM, 90, 0x1ccf }, // (38) Closeup package
    { IMG_ROOM_OBJ, 63, 0x261e }, // (39) Dumb-waiter
    { IMG_ROOM_OBJ, 67, 0x2733 }, // (40) Mouldy old skeleton
    { IMG_ROOM_OBJ, 80, 0x2cdd }, // (41) wall mirror
    { IMG_ROOM_OBJ, 81, 0x2ecf }, // (42) other end of sheet
    { IMG_ROOM, 250, 0x3000 }, // (43) Color test
    { IMG_ROOM_OBJ, 29, 0x302c }, // (44) Coffin is open
    { IMG_ROOM_OBJ, 30, 0x334f }, // (45) Coffin is closed
    { IMG_ROOM, 91, 0x3580 }, // (46) Closeup angry mob
    { IMG_ROOM_OBJ, 1, 0x4072 }, // (47) Open window
    { IMG_ROOM_OBJ, 36, 0x408e }, // (48) Sheet tied to bed
    { IMG_ROOM, 2, 0x413a }, // (49) bedroom
    { IMG_ROOM, 0, 0x4a25 }, // (50) Too dark
    { IMG_ROOM, 1, 0x5580 }, // (51) *I'm lying in a large brass bed
    { IMG_ROOM, 3, 0x5e48 }, // (52) *ledge outside an open window
    { IMG_ROOM, 4, 0x6a17 }, // (53) *hanging on the end of a sheet
    { IMG_ROOM, 5, 0x74c1 }, // (54) flower box outside window
    { IMG_ROOM, 6, 0x7cba }, // (55) CRYPT
    { IMG_ROOM, 7, 0x8f41 }, // (56) closet
    { IMG_ROOM, 8, 0x984f }, // (57) Bathroom
    { IMG_ROOM, 9, 0xa10e }, // (58) *I'm outside the castle
    { IMG_ROOM, 10, 0xad79 }, // (59) DOORLESS room
    { IMG_ROOM, 11, 0xb687 }, // (60) hall inside the castle
    { IMG_ROOM, 12, 0xc0d6 }, // (61) kitchen
    { IMG_ROOM, 13, 0xcc56 }, // (62) large COFFIN
    { IMG_ROOM, 14, 0xdf5d }, // (63) pAntry
    { IMG_ROOM, 15, 0xe872 }, // (64) giant SOLAR OVEN
    { IMG_ROOM, 16, 0xf72c }, // (65) Dungeon
    { IMG_ROOM, 17, 0x10441 }, // (66) Meandering path
    { IMG_ROOM, 18, 0x1159e }, // (67) Pit
    { IMG_ROOM, 19, 0x12872 }, // (68) Dark passage
    { IMG_ROOM, 20, 0x12f5d }, // (69) dumb-waiter by a room
    { IMG_ROOM, 21, 0x136b3 }, // (70) workroom
    { IMG_ROOM, 22, 0x1400e }, // (71) LOT OF TROUBLE!
    { IMG_ROOM, 99, 0x14f97 }, // (72) Title
    { IMG_ROOM_OBJ, 56, 0x16156 },  // 73
    { 0, 0, 0 }
};

static const struct imglist listVoodoo[] = {
    { IMG_ROOM, 1, 0x297 }, // (0) chapel
    { IMG_ROOM, 14, 0xac1 }, // (1) Chimney
    { IMG_ROOM, 2, 0x1a79 }, // (2) Dingy Looking Stairwell
    { IMG_ROOM, 3, 0x1e2c }, // (3) room in the castle
    { IMG_ROOM, 4, 0x28a5 }, // (4) Tunnel
    { IMG_ROOM, 5, 0x3817 }, // (5) room in the castle
    { IMG_ROOM, 6, 0x3f87 }, // (6) *I'm in Medium Maegen's Mad Room
    { IMG_ROOM, 7, 0x4664 }, // (7) room in the castle
    { IMG_ROOM, 8, 0x4f0e }, // (8) room in the castle
    { IMG_ROOM, 9, 0x52ac }, // (9) room in the castle
    { IMG_ROOM, 10, 0x5b48 }, // (10) Ballroom
    { IMG_ROOM, 11, 0x6264 }, // (11) dungeon
    { IMG_ROOM, 12, 0x67dd }, // (12) *I'm in the Armory
    { IMG_ROOM, 13, 0x740e }, // (13) torture chamber
    { IMG_ROOM, 15, 0x7d4f }, // (14) large fireplace
    { IMG_ROOM, 16, 0x8af2 }, // (15) room in the castle
    { IMG_ROOM, 17, 0x954f }, // (16) Lab
    { IMG_ROOM, 18, 0xa19e }, // (17) narrow part of the chimney
    { IMG_ROOM, 19, 0xab17 }, // (18) Graveyard
    { IMG_ROOM, 20, 0xbe1e }, // (19) parlor
    { IMG_ROOM, 21, 0xc579 }, // (20) Jail Cell
    { IMG_ROOM, 22, 0xcf9e }, // (21) *I'm on a ledge
    { IMG_ROOM, 23, 0xd533 }, // (22) hidden VOODOO room
    { IMG_ROOM, 24, 0xe5e4 }, // (23) room in the castle
    { IMG_ROOM, 25, 0xeb3a }, // (24) lot of TROUBLE!
    { IMG_ROOM, 90, 0xf856 }, // (25) Closeup Broken sword
    { IMG_ROOM, 96, 0xfd87 }, // (26) Closeup Chemicals
    { IMG_ROOM, 101, 0x1068e }, // (27) Closeup Count Cristo
    { IMG_ROOM, 100, 0x114e4 }, // (28) Closeup dusty idol
    { IMG_ROOM, 99, 0x11956 }, // (29) Title
    { IMG_ROOM, 0, 0x12879 }, // (30) Too dark
    { IMG_ROOM_OBJ, 17, 0x131cf }, // (31) Closed Coffin
    { IMG_INV_OBJ, 26, 0x13325 }, // (32) Ju-Ju bag
    { IMG_INV_OBJ, 43, 0x13407 }, // (33) Dusty idol
    { IMG_INV_OBJ, 14, 0x134ba }, // (34) Four leaf clover
    { IMG_INV_OBJ, 60, 0x1355d }, // (35) Advertising
    { IMG_INV_OBJ, 16, 0x1359e }, // (36) Cast iron pot
    { IMG_INV_OBJ, 44, 0x1360e }, // (37) Doll
    { IMG_INV_OBJ, 25, 0x136ba }, // (38) Sapphire ring
    { IMG_INV_OBJ, 39, 0x13733 }, // (39) Pocket Shovel
    { IMG_INV_OBJ, 38, 0x1378e }, // (40) Labeled chemicals
    { IMG_INV_OBJ, 54, 0x13817 }, // (41) Stick
    { IMG_INV_OBJ, 33, 0x13879 }, // (42) Dull & broken sword
    { IMG_INV_OBJ, 0, 0x138dd }, // (43) Bloody Knife
    { IMG_INV_OBJ, 64, 0x1392c }, // (44) Page torn from a book
    { IMG_INV_OBJ, 47, 0x13972 }, // (45) Antique hammer
    { IMG_INV_OBJ, 2, 0x139c1 }, // (46) Plaque
    { IMG_INV_OBJ, 52, 0x13a2c }, // (47) Voodoo book
    { IMG_INV_OBJ, 21, 0x13aa5 }, // (48) Soot
    { IMG_INV_OBJ, 49, 0x13b0e }, // (49) Rusting SAW
    { IMG_INV_OBJ, 7, 0x13b64 }, // (50) Shield
    { IMG_INV_OBJ, 4, 0x13c56 }, // (51) Broken glass
    { IMG_INV_OBJ, 3, 0x13cac }, // (52) Animal heads
    { IMG_INV_OBJ, 36, 0x13d6b }, // (53) Chem tubes
    { IMG_INV_OBJ, 53, 0x13db3 }, // (54) Ju-Ju man statue
    { IMG_INV_OBJ, 9, 0x13e97 }, // (55) Brightly glowing idol
    { IMG_INV_OBJ, 40, 0x13f72 }, // (56) Mixed Chemicals
    { IMG_INV_OBJ, 42, 0x13ff9 }, // (57) Nails
    { IMG_INV_OBJ, 51, 0x1405d }, // (58) Paper
    { IMG_INV_OBJ, 63, 0x140d6 }, // (59) Pieces of rock
    { IMG_INV_OBJ, 45, 0x1415d }, // (60) Wooden boards
    { IMG_INV_OBJ, 59, 0x14217 }, // (61) Broken grating
    { IMG_INV_OBJ, 29, 0x14279 }, // (62) Ju-Ju man
    { IMG_ROOM_OBJ, 19, 0x14333 }, // (63) Closed Window
    { IMG_ROOM_OBJ, 23, 0x144c1 }, // (64) Spirit Medium
    { IMG_ROOM_OBJ, 28, 0x147f2 }, // (65) Slick chute leading downward
    { IMG_ROOM_OBJ, 35, 0x14e1e }, // (66) Stuck Chimney Sweep
    { IMG_ROOM_OBJ, 6, 0x15710 }, // (67) Dark hole
    { IMG_ROOM_OBJ, 41, 0x158c8 }, // (68) Wide open door
    { IMG_ROOM_OBJ, 65, 0x15972 }, // (69) Smiling Count Cristo
    { IMG_ROOM_OBJ, 13, 0x15cf9 }, // (70) Open Flue
    { IMG_ROOM_OBJ, 31, 0x15eb3 }, // (71) Wide crack in the wall
    { IMG_ROOM_OBJ, 34, 0x16025 }, // (72) Open SAfe
    { IMG_ROOM_OBJ, 37, 0x1625d }, // (73) Closed SAfe
    { IMG_ROOM_OBJ, 46, 0x16348 }, // (74) Wooden boards nailed
    { IMG_ROOM_OBJ, 48, 0x16472 }, //75 Grating
    { IMG_INV_OBJ, 32, 0x164f2 }, //76 Rabbit's foot? // 16590, bd9a
    { IMG_ROOM, 250, 0x16490 }, // 77, Color test
    { IMG_ROOM_OBJ, 80, 0x165c1 }, // 77, Wooden boards unnailed (45)
    { 0, 0, 0, }
};


static const CropList a8croplist[] = {
    { VOODOO_CASTLE_US, IMG_ROOM, 10, 8, 8 },
    { VOODOO_CASTLE_US, IMG_ROOM, 11, 8, 8 },
    { VOODOO_CASTLE_US, IMG_ROOM, 14, 8, 8 },
    { VOODOO_CASTLE_US, IMG_ROOM, 19, 8, 8 },
    { VOODOO_CASTLE_US, IMG_ROOM, 20, 8, 8 },
    { HULK_US, IMG_ROOM, 0, 0, 32 },
    { COUNT_US, IMG_ROOM, 0, 8, 0 },

    { 0, 0, 0, 0, 0 }
};


static int StripBrackets(char sideB[], size_t length) {
    int left_bracket = 0;
    int right_bracket = 0;
    if (length > 4) {
        for (int i = length - 4; i > 0; i--) {
            char c = sideB[i];
            if (c == ']') {
                if (right_bracket == 0) {
                    right_bracket = i;
                } else {
                    return 0;
                }
            } else if (c == '[') {
                if (right_bracket > 0) {
                    left_bracket = i;
                    break;
                } else {
                    return 0;
                }
            }
        }
        if (right_bracket && left_bracket) {
            sideB[left_bracket++] = '.';
            sideB[left_bracket++] = 'a';
            sideB[left_bracket++] = 't';
            sideB[left_bracket++] = 'r';
            sideB[left_bracket] = '\0';
            return 1;
        }
    }
    return 0;
}

static uint8_t *LookForAtari8CompanionFilename(int index, CompanionNameType type, size_t stringlen, size_t *filesize) {

    char sideB[stringlen + 10];
    uint8_t *result = NULL;

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
    result = ReadFileIfExists(sideB, filesize);
    if (!result) {
        if (type == TYPE_B) {
            if (StripBrackets(sideB, stringlen)) {
                debug_print("looking for companion file \"%s\"\n", sideB);
                result = ReadFileIfExists(sideB, filesize);
            }
        } else if (type == TYPE_A) {
            // First we look for the period before the file extension
            size_t ppos = stringlen - 1;
            while(sideB[ppos] != '.' && ppos > 0)
                ppos--;
            if (ppos < 1)
                return NULL;
            // Then we copy the extension to the new end position
            for (size_t i = ppos; i <= stringlen; i++) {
                sideB[i + 8] = sideB[i];
            }
            sideB[ppos++] = '[';
            sideB[ppos++] = 'c';
            sideB[ppos++] = 'r';
            sideB[ppos++] = ' ';
            sideB[ppos++] = 'C';
            sideB[ppos++] = 'S';
            sideB[ppos++] = 'S';
            sideB[ppos] = ']';
            debug_print("looking for companion file \"%s\"\n", sideB);
            result = ReadFileIfExists(sideB, filesize);
        }
    }

    return result;
}

static uint8_t *GetAtari8CompanionFile(size_t *size) {

    size_t gamefilelen = strlen(game_file);
    char *foundname = LookInDatabase(a8companionlist, gamefilelen);
    uint8_t *result = NULL;
    if (foundname) {
        result = ReadFileIfExists(foundname, size);
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
                        result = LookForAtari8CompanionFilename(i + 2, type, gamefilelen, size);
                    if (result)
                        return result;
                }
            }
        }
    }
    return NULL;
}

static int ExtractImagesFromAtariCompanionFile(uint8_t *data, size_t datasize, uint8_t *otherdisk, size_t othersize) {
    size_t size;

    int outpic;

    const struct imglist *list;

    switch(CurrentGame) {
        case CLAYMORGUE_US:
            list = listClaymorgue;
            break;
        case COUNT_US:
            list = listCount;
            break;
        case VOODOO_CASTLE_US:
            list = listVoodoo;
            break;
        case HULK_US:
            list = listHulk;
            break;
        default:
            return 0;
    }

    USImages = new_image();
    struct USImage *image = USImages;

    // Now loop round for each image
    for (outpic = 0; list[outpic].offset != 0; outpic++)
    {
        uint8_t *ptr = data + list[outpic].offset;

        size = *ptr++;
        size += *ptr * 256 + 2;

        image->usage = list[outpic].usage;
        image->index = list[outpic].index;
        image->imagedata = MemAlloc(size);
        image->datasize = size;
        image->systype = SYS_ATARI8;
        memcpy(image->imagedata, data + list[outpic].offset - 2, size);
        /* Bytes 0xb390 to 0xb410 correspond to the content of sector 360 (0x0168) of the original Atari disk
           which contains the volume table of contents (mostly a bitmap of used sectors). This is not part of the
           graphics data, so we cut it out. */
        if (list[outpic].offset < 0xb390 && list[outpic].offset + image->datasize > 0xb390) {
            memcpy(image->imagedata + 0xb390 - list[outpic].offset + 2, data + 0xb410, size - 0xb390 + list[outpic].offset - 2);
        }

        /* Many images have black bars on one or more sides, probably used to center smaller images. The original
           interpreters had a black background which hid this, but it looks strange if the background is not black,
           so we crop them. */
        if (CurrentGame == VOODOO_CASTLE_US && image->usage == IMG_ROOM)
            image->cropleft = 8;

        if ((CurrentGame == CLAYMORGUE_US || CurrentGame == COUNT_US) && image->usage == IMG_ROOM && image->imagedata[7] == 126)
            image->cropright = 8;

        for (int i = 0; a8croplist[i].game != 0; i++) {
            CropList crop = a8croplist[i];
            if (crop.game == CurrentGame && crop.index == image->index && crop.usage == image->usage) {
                image->cropleft = crop.cropleft;
                image->cropright = crop.cropright;
            }
        }

        image->next = new_image();
        image->next->previous = image;
        image = image->next;
    }

    /* Read the inventory image from the boot disk */
    if (otherdisk && othersize > 0x988e + 0x5fd) {
        image->usage = IMG_ROOM;
        image->index = 98;
        image->datasize = 0x5fd;
        image->imagedata = MemAlloc(image->datasize);
        image->systype = SYS_ATARI8;
        memcpy(image->imagedata, otherdisk + 0x988e, image->datasize);
    } else if (image->imagedata == NULL) {
        /* Free the last image, it is empty */
        if (image->previous) {
            image->previous->next = NULL;
            free(image);
        } else {
            free(USImages);
            USImages = NULL;
        }
    }

    /* If no images are found, free USImages struct */
    if (USImages != NULL && USImages->next == NULL && USImages->imagedata == NULL) {
        free(USImages);
        USImages = NULL;
    }

    return 1;
}


static const uint8_t atrheader[6] = { 0x96, 0x02, 0x80, 0x16, 0x80, 0x00 };

int DetectAtari8(uint8_t **sf, size_t *extent)
{

    int result = 0;
    size_t data_start = 0x04c1;
    // Header actually starts at offset 0x04f9 (0x04c1 + 0x38).
    // We add 50 bytes at the head to match the C64 files.

    if (*extent > MAX_LENGTH || *extent < data_start)
        return 0;

    for (int i = 0; i < 6; i++)
        if ((*sf)[i] != atrheader[i])
            return 0;

    size_t companionsize;
    uint8_t *companionfile = GetAtari8CompanionFile(&companionsize);

    ImageWidth = 280;
    ImageHeight = 158;
    result = LoadBinaryDatabase(*sf + data_start, *extent - data_start, *Game, 0);
    if (!result && companionfile != NULL && companionsize > data_start) {
        debug_print("Could not find database in this file, trying the companion file\n");
        result = LoadBinaryDatabase(companionfile + data_start, companionsize - data_start, *Game, 0);
        if (result) {
            debug_print("Found database in companion file. Switching files.\n");
            uint8_t *temp = companionfile;
            size_t tempsize = companionsize;
            companionfile = *sf;
            companionsize = *extent;
            *sf = temp;
            *extent = tempsize;
        }
    }
    
    if (result) {
        CurrentSys = SYS_ATARI8;
        if (companionfile) {
            ExtractImagesFromAtariCompanionFile(companionfile, companionsize, *sf, *extent);
            free(companionfile);
        }
    } else
        debug_print("Failed loading database\n");

    return result;
}

