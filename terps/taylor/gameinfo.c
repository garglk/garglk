//
//  gameinfo.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-03-23.
//

#include <stdio.h>

#include "taylor.h"

// clang-format off
struct GameInfo games[NUMGAMES] = {
    {
        "Questprobe 3",
        QUESTPROBE3,
        QUESTPROBE3_TYPE, // type
        QUESTPROBE3,

        0x45F6, // dictionary
        0x4c8c, // tokens
        0x28a5, // start_of_room_descriptions
        0x2c7a, // start_of_item_descriptions
        0x3e10, // automatics
        0x3045, // actions
        0x50d4, // start_of_room_connections
        0x67ba, // start_of_flags
        0x50a3, // start_of_item_locations
        0x2000, // start_of_messages
        0,      // second message bank

        0x6100, // start_of_characters;
        0x6916, // start_of_image_data;
        0x381c, // image patterns lookup table;
        0x001c, // number of patterns
        0x009f, // patterns end marker
        0x878b, // start of room image instructions
        57,     // number_of_image blocks;
        ZXOPT   // palette
    },

    {
        "Questprobe 3 C64",
        QUESTPROBE3_64,
        QUESTPROBE3_TYPE, // type
        QUESTPROBE3,

        0x1187, // dictionary
        0x0003, // tokens
        0x09e7, // start_of_room_descriptions
        0x0dbc, // start_of_item_descriptions
        0x2641, // automatics
        0x1876, // actions
        0x1812, // start_of_room_connections
        0xc1bc, // start_of_flags
        0x2dfd, // start_of_item_locations
        0x0142, // start_of_messages
        0,      // second message bank

        0xbb02, // start_of_characters;
        0x6058, // start_of_image_data;
        0x381c, // image patterns lookup table;
        0x001c, // number of patterns
        0x009f, // patterns end marker
        0x878b, // start of room image instructions
        57,     // number_of_image blocks;
        C64A    // palette
    },

    {
        "Rebel Planet",
        REBEL_PLANET,
        REBEL_PLANET_TYPE, // type
        REBEL_PLANET,

        0x3559, // dictionary
        0x7bda, // tokens
        0x5615, // start_of_room_descriptions
        0x7321, // start_of_item_descriptions
        0x3c9d, // automatics
        0x43e0, // actions
        0x33a3, // start_of_room_connections
        0x1ff8, // start_of_flags
        0x331e, // start_of_item_locations
        0x5e05, // start_of_messages
        0x6f19, // second message bank

        0x810e, // start_of_characters
        0x9139, // start_of_image_data
        0,      // image patterns lookup table
        0,      // number of patterns
        0,      // patterns end marker
        0x87a6, // start of room image instructions
        167,    // number_of_image blocks
        ZXOPT   // palette
    },

    {
        "Rebel Planet C64",
        REBEL_PLANET_64,
        REBEL_PLANET_TYPE, // type
        REBEL_PLANET,

        0x5e3d, // dictionary
        0x40c7, // tokens
        0x1b02, // start_of_room_descriptions
        0x380e, // start_of_item_descriptions
        0x6581, // automatics
        0x6cc4, // actions
        0x5c87, // start_of_room_connections
        0x198a, // start_of_flags
        0x5c02, // start_of_item_locations
        0x22f2, // start_of_messages
        0x3406, // second message bank

        0xb78c, // start_of_characters
        0x7f02, // start_of_image_data
        0,      // image patterns lookup table
        0,      // number of patterns
        0,      // patterns end marker
        0xadea, // start of room image instructions
        167,    // number_of_image blocks
        C64B    // palette
    },

    {
        "Blizzard Pass",
        BLIZZARD_PASS,
        BLIZZARD_PASS_TYPE, // type
        BLIZZARD_PASS,

        0x6720, // dictionary
        0x71bc, // tokens
        0x35ce, // start_of_room_descriptions
        0x4ce6, // start_of_item_descriptions
        0x56d9, // automatics
        0x5a54, // actions
        0x6b67, // start_of_room_connections
        0x2051, // start_of_flags
        0x76a8, // start_of_item_locations
        0x3e26, // start_of_messages
        0,      // second message bank

        0x8350, // start_of_characters
        0x8708, // start_of_image_data
        0,      // image patterns lookup table
        0,      // number of patterns
        0,      // patterns end marker
        0x7798, // start of room image instructions
        114,    // number_of_image blocks;
        ZXOPT   // palette
    },

    {
        "Heman",
        HEMAN,
        HEMAN_TYPE, // type
        HEMAN,

        0x483A, // dictionary
        0x4ce0, // tokens
        0x5b36, // start_of_room_descriptions
        0x56a2, // start_of_item_descriptions
        0x45bf, // automatics
        0x37d8, // actions
        0x442f, // start_of_room_connections
        0x1e45, // start_of_flags
        0x43cb, // start_of_item_locations
        0x6665, // start_of_messages
        0x4fee, // second message bank

        0x8603, // start_of_characters
        0x8d13, // start_of_image_data
        0x3703, // image patterns lookup table
        0x001c, // number of patterns
        0x009f, // patterns end marker
        0x7adf, // start of room image instructions
        139,    // number_of_image blocks
        ZXOPT   // palette
    },

    {
        "Heman C64",
        HEMAN_64,
        HEMAN_TYPE, // type
        HEMAN,

        0x116f, // dictionary
        0x1615, // tokens
        0x5c08, // start_of_room_descriptions
        0x798d, // start_of_item_descriptions
        0x0ef4, // automatics
        0x010d, // actions
        0x0d64, // start_of_room_connections
        0x2bbf, // start_of_flags
        0x0d00, // start_of_item_locations
        0x6737, // start_of_messages
        0x1923, // second message bank

        0x8008, // start_of_characters;
        0x8718, // start_of_image_data;
        0,      // image patterns lookup table;
        0,      // number of patterns
        0,      // patterns end marker
        0x4808, // start of room image instructions
        139,    // number_of_image blocks;
        C64B    // palette
    },

    {
        "Temple of Terror",
        TEMPLE_OF_TERROR,
        HEMAN_TYPE, // type
        TEMPLE_OF_TERROR,

        0x4b9d, // dictionary
        0x50d4, // tokens
        0x53b3, // start_of_room_descriptions
        0x5c4f, // start_of_item_descriptions
        0x4804, // automatics
        0x38a4, // actions
        0x465c, // start_of_room_connections
        0x1e45, // start_of_flags
        0x4594, // start_of_item_locations
        0x667d, // start_of_messages
        0x75d0, // second message bank

        0x83cb, // start_of_characters;
        0x8a33, // start_of_image_blocks;
        0x3837, // image patterns lookup table;
        0x0012, // number of patterns
        0x00aa, // patterns end marker
        0x7b75, // start of room image instructions
        143,    // number_of_image blocks;
        ZXOPT   // palette
    },

    {
        "Temple of Terror text only version",
        TOT_TEXT_ONLY,
        HEMAN_TYPE, // type
        TEMPLE_OF_TERROR,

        0x4ba2, // dictionary
        0x50d9, // tokens
        0x7a2a, // start_of_room_descriptions
        0x5419, // start_of_item_descriptions
        0x4809, // automatics
        0x38a9, // actions
        0x4661, // start_of_room_connections
        0x21bf, // start_of_flags
        0x4599, // start_of_item_locations
        0x5fd9, // start_of_messages
        0x73b7, // second message bank

        0,      // start_of_characters;
        0,      // start_of_image_blocks;
        0,      // image patterns lookup table;
        0,      // number of patterns
        0,      // patterns end marker
        0,      // start of room image instructions
        0,      // number_of_image blocks;
        0       // palette
    },

    {
        "Temple of Terror combined version",
        TOT_HYBRID,
        HEMAN_TYPE, // type
        TEMPLE_OF_TERROR,

        0x4ba2, // dictionary
        0x50d9, // tokens
        0x7a2a, // start_of_room_descriptions
        0x5419, // start_of_item_descriptions
        0x4809, // automatics
        0x38a9, // actions
        0x4661, // start_of_room_connections
        0x21bf, // start_of_flags
        0x4599, // start_of_item_locations
        0x5fd9, // start_of_messages
        0x73b7, // second message bank

        0x83cb, // start_of_characters;
        0x8a33, // start_of_image_blocks;
        0x3837, // image patterns lookup table;
        0x0012, // number of patterns
        0x00aa, // patterns end marker
        0x7b75, // start of room image instructions
        143,    // number_of_image blocks;
        ZXOPT   // palette
    },

    {
        "Temple of Terror C64",
        TEMPLE_OF_TERROR_64,
        HEMAN_TYPE, // type
        TEMPLE_OF_TERROR,

        0x5b08, // dictionary
        0x603f, // tokens
        0x631e, // start_of_room_descriptions
        0x1301, // start_of_item_descriptions
        0x0f68, // automatics
        0x0008, // actions
        0x0dc0, // start_of_room_connections
        0x2aa6, // start_of_flags
        0x0cf8, // start_of_item_locations
        0x6bba, // start_of_messages
        0x7b0d, // second message bank

        0x8048, // start_of_characters;
        0x86b0, // start_of_image_blocks;
        0,      // image patterns lookup table;
        0,      // number of patterns
        0,      // patterns end marker
        0x4708, // start of room image instructions
        143,    // number_of_image blocks;
        C64B    // palette
    },

    {
        "Temple of Terror C64 text only version",
        TOT_TEXT_ONLY_64,
        HEMAN_TYPE, // type
        TEMPLE_OF_TERROR,

        0x5001, // dictionary
        0x5538, // tokens
        0x7e89, // start_of_room_descriptions
        0x5878, // start_of_item_descriptions
        0x4c68, // automatics
        0x3d08, // actions
        0x4ac0, // start_of_room_connections
        0x0b95, // start_of_flags
        0x49f8, // start_of_item_locations
        0x6438, // start_of_messages
        0x7816, // second message bank

        0,      // start_of_characters;
        0,      // start_of_image_blocks;
        0,      // image patterns lookup table;
        0,      // number of patterns
        0,      // patterns end marker
        0,      // start of room image instructions
        0,      // number_of_image blocks;
        C64B    // palette
    },

    {
        "Temple of Terror C64 combined version",
        TOT_HYBRID_64,
        HEMAN_TYPE, // type
        TEMPLE_OF_TERROR,

        0x5001, // dictionary
        0x5538, // tokens
        0x7e89, // start_of_room_descriptions
        0x5878, // start_of_item_descriptions
        0x4c68, // automatics
        0x3d08, // actions
        0x4ac0, // start_of_room_connections
        0x0b95, // start_of_flags
        0x49f8, // start_of_item_locations
        0x6438, // start_of_messages
        0x7816, // second message bank

        0x8888, // start_of_characters;
        0x8ef0, // start_of_image_blocks;
        0,      // image patterns lookup table;
        0,      // number of patterns
        0,      // patterns end marker
        0x16ea, // start of room image instructions
        143,    // number_of_image blocks;
        C64B    // palette
    },

    {
        "Kayleth",
        KAYLETH,
        HEMAN_TYPE, // type
        KAYLETH,

        0x5323, // dictionary
        0x7fee, // tokens
        0x6eb3, // start_of_room_descriptions
        0x7870, // start_of_item_descriptions
        0x4f4d, // automatics
        0x3a48, // actions
        0x4de0, // start_of_room_connections
        0x20f3, // start_of_flags
        0x4d5f, // start_of_item_locations
        0x5a99, // start_of_messages
        0x769d, // second message bank

        0x95f0, // start_of_characters;
        0x9ce0, // start_of_image_blocks;
        0x38B6, // image patterns lookup table;
        0x1f,   // number of patterns
        0x8e,   // patterns end marker
        0x8279, // start of room image instructions
        210,    // number_of_image blocks;
        ZXOPT   // palette
    },

    {
        "Kayleth C64",
        KAYLETH_64,
        HEMAN_TYPE, // type
        KAYLETH,

        0x175f, // dictionary
        0xbace, // tokens
        0xa993, // start_of_room_descriptions
        0xb350, // start_of_item_descriptions
        0x1389, // automatics
        0x8074, // actions
        0x940c, // start_of_room_connections
        0x2a81, // start_of_flags
        0x938b, // start_of_item_locations
        0x9579, // start_of_messages
        0xb17d, // second message bank

        0xc028, // start_of_characters;
        0x5b28, // start_of_image_blocks;
        0,      // image patterns lookup table;
        0,      // number of patterns
        0,      // patterns end marker
        0x0012, // start of room image instructions
        210,    // number_of_image blocks;
        C64B    // palette
    },
// clang-format on

    { "Unknown game",
        UNKNOWN_GAME,
        NO_TYPE,
        UNKNOWN_GAME,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0 }
};
