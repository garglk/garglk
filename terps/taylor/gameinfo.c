//
//  gameinfo.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-03-23.
//

#include <stdio.h>

#include "taylor.h"

/* Table offsets are addresses in the memory of the original machine, as
   the game has them; FileBaselineOffset turns them into file offsets.
   A field that is left out is zero: the game has no such table. */
GameInfo games[NUMGAMES] = {
    {
        .Title = "Questprobe 3",
        .gameID = QUESTPROBE3,
        .type = QUESTPROBE3_TYPE,
        .base_game = QUESTPROBE3,

        .start_of_dictionary = 0x45f6,
        .start_of_tokens = 0x4c8c,
        .start_of_room_descriptions = 0x28a5,
        .start_of_item_descriptions = 0x2c7a,
        .start_of_status_table = 0x3e10,
        .start_of_event_table = 0x3045,
        .start_of_room_connections = 0x50d4,
        .start_of_flags = 0x67ba,
        .start_of_item_locations = 0x50a3,
        .start_of_messages = 0x2000,

        .start_of_tiles = 0x6100,
        .start_of_image_blocks = 0x6916,
        .number_of_pictures = 57,
        .palette = ZXOPT,
    },

    {
        .Title = "Questprobe 3 C64",
        .gameID = QUESTPROBE3_64,
        .type = QUESTPROBE3_TYPE,
        .base_game = QUESTPROBE3,

        .start_of_dictionary = 0x1187,
        .start_of_tokens = 0x0003,
        .token_signature = "\x61\xa0\x64\xa0\x65\xa0\x67\xa0\x69\xa0",
        .start_of_room_descriptions = 0x09e7,
        .start_of_item_descriptions = 0x0dbc,
        .start_of_status_table = 0x2641,
        .start_of_event_table = 0x1876,
        .start_of_room_connections = 0x1812,
        .start_of_flags = 0xc1bc,
        .start_of_item_locations = 0x2dfd,
        .start_of_messages = 0x0142,

        .start_of_tiles = 0xbb02,
        .start_of_image_blocks = 0x6058,
        .number_of_pictures = 57,
        .palette = C64QP3,
    },

    {
        .Title = "Rebel Planet",
        .gameID = REBEL_PLANET,
        .type = REBEL_PLANET_TYPE,
        .base_game = REBEL_PLANET,

        .start_of_dictionary = 0x3559,
        .start_of_tokens = 0x7bda,
        .start_of_room_descriptions = 0x5615,
        .start_of_item_descriptions = 0x7321,
        .start_of_status_table = 0x3c9d,
        .start_of_event_table = 0x43e0,
        .start_of_room_connections = 0x33a3,
        .start_of_flags = 0x1ff8,
        .start_of_item_locations = 0x331e,
        .start_of_messages = 0x5e05,
        .start_of_messages_2 = 0x6f19,

        .start_of_tiles = 0x810e,
        .start_of_image_blocks = 0x9139,
        .start_of_image_instructions = 0x87a6,
        .number_of_pictures = 167,
        .palette = ZXOPT,
    },

    {
        .Title = "Rebel Planet C64",
        .gameID = REBEL_PLANET_64,
        .type = REBEL_PLANET_TYPE,
        .base_game = REBEL_PLANET,

        .start_of_dictionary = 0x5e3d,
        .start_of_tokens = 0x40c7,
        .token_signature = "\xa7.\xfe \xfe,\xfe!\xfe?",
        .start_of_room_descriptions = 0x1b02,
        .start_of_item_descriptions = 0x380e,
        .start_of_status_table = 0x6581,
        .start_of_event_table = 0x6cc4,
        .start_of_room_connections = 0x5c87,
        .start_of_flags = 0x198a,
        .start_of_item_locations = 0x5c02,
        .start_of_messages = 0x22f2,
        .start_of_messages_2 = 0x3406,

        .start_of_tiles = 0xb78c,
        .start_of_image_blocks = 0x7f02,
        .start_of_image_instructions = 0xadea,
        .number_of_pictures = 167,
        .palette = C64TAYLOR,
    },

    {
        .Title = "Blizzard Pass",
        .gameID = BLIZZARD_PASS,
        .type = BLIZZARD_PASS_TYPE,
        .base_game = BLIZZARD_PASS,

        .start_of_dictionary = 0x6720,
        .start_of_tokens = 0x71bc,
        .start_of_room_descriptions = 0x35ce,
        .start_of_item_descriptions = 0x4ce6,
        .start_of_status_table = 0x56d9,
        .start_of_event_table = 0x5a54,
        .start_of_room_connections = 0x6b67,
        .start_of_flags = 0x2051,
        .start_of_item_locations = 0x76a8,
        .start_of_messages = 0x3e26,

        .start_of_tiles = 0x8350,
        .start_of_image_blocks = 0x8708,
        .start_of_image_instructions = 0x7798,
        .number_of_pictures = 114,
        .palette = ZXOPT,
    },

    {
        .Title = "Heman",
        .gameID = HEMAN,
        .type = HEMAN_TYPE,
        .base_game = HEMAN,

        .start_of_dictionary = 0x483a,
        .start_of_tokens = 0x4ce0,
        .start_of_room_descriptions = 0x5b36,
        .start_of_item_descriptions = 0x56a2,
        .start_of_status_table = 0x45bf,
        .start_of_event_table = 0x37d8,
        .start_of_room_connections = 0x442f,
        .start_of_flags = 0x1e45,
        .start_of_item_locations = 0x43cb,
        .start_of_messages = 0x6665,
        .start_of_messages_2 = 0x4fee,

        .start_of_tiles = 0x8603,
        .start_of_image_blocks = 0x8d13,
        .image_patterns_lookup = 0x3703,
        .number_of_patterns = 0x001c,
        .pattern_end_marker = 0x009f,
        .start_of_image_instructions = 0x7adf,
        .number_of_pictures = 139,
        .palette = ZXOPT,
    },

    {
        .Title = "Heman C64",
        .gameID = HEMAN_64,
        .type = HEMAN_TYPE,
        .base_game = HEMAN,

        .start_of_dictionary = 0x116f,
        .start_of_tokens = 0x1615,
        .token_signature = "\x80You are i",
        .start_of_room_descriptions = 0x5c08,
        .start_of_item_descriptions = 0x798d,
        .start_of_status_table = 0x0ef4,
        .start_of_event_table = 0x010d,
        .start_of_room_connections = 0x0d64,
        .start_of_flags = 0x2bbf,
        .start_of_item_locations = 0x0d00,
        .start_of_messages = 0x6737,
        .start_of_messages_2 = 0x1923,

        .start_of_tiles = 0x8008,
        .start_of_image_blocks = 0x8718,
        .start_of_image_instructions = 0x4808,
        .number_of_pictures = 139,
        .palette = C64TAYLOR,
    },

    /* Temple of Terror came as two games on each machine, one with
       pictures and one with longer text. The "combined" rows are the two
       loaded together (LookForSecondTOTGame, c64decrunch.c): the text-only
       game's tables with the graphical game's pictures, which on the
       Spectrum are where the graphical game has them and on the C64 are
       copied to a place of their own. */
#define TOT_TEXT_ONLY_TABLES                   \
    .start_of_dictionary = 0x4ba2,             \
    .start_of_tokens = 0x50d9,                 \
    .start_of_room_descriptions = 0x7a2a,      \
    .start_of_item_descriptions = 0x5419,      \
    .start_of_status_table = 0x4809,           \
    .start_of_event_table = 0x38a9,            \
    .start_of_room_connections = 0x4661,       \
    .start_of_flags = 0x21bf,                  \
    .start_of_item_locations = 0x4599,         \
    .start_of_messages = 0x5fd9,               \
    .start_of_messages_2 = 0x73b7

#define TOT_PICTURES                           \
    .start_of_tiles = 0x83cb,                  \
    .start_of_image_blocks = 0x8a33,           \
    .image_patterns_lookup = 0x3837,           \
    .number_of_patterns = 0x0012,              \
    .pattern_end_marker = 0x00aa,              \
    .start_of_image_instructions = 0x7b75,     \
    .number_of_pictures = 143,                 \
    .palette = ZXOPT

#define TOT_TEXT_ONLY_TABLES_64                \
    .start_of_dictionary = 0x5001,             \
    .start_of_tokens = 0x5538,                 \
    .token_signature = "\x80You are i",        \
    .start_of_room_descriptions = 0x7e89,      \
    .start_of_item_descriptions = 0x5878,      \
    .start_of_status_table = 0x4c68,           \
    .start_of_event_table = 0x3d08,            \
    .start_of_room_connections = 0x4ac0,       \
    .start_of_flags = 0x0b95,                  \
    .start_of_item_locations = 0x49f8,         \
    .start_of_messages = 0x6438,               \
    .start_of_messages_2 = 0x7816

    {
        .Title = "Temple of Terror",
        .gameID = TEMPLE_OF_TERROR,
        .type = HEMAN_TYPE,
        .base_game = TEMPLE_OF_TERROR,

        .start_of_dictionary = 0x4b9d,
        .start_of_tokens = 0x50d4,
        .start_of_room_descriptions = 0x53b3,
        .start_of_item_descriptions = 0x5c4f,
        .start_of_status_table = 0x4804,
        .start_of_event_table = 0x38a4,
        .start_of_room_connections = 0x465c,
        .start_of_flags = 0x1e45,
        .start_of_item_locations = 0x4594,
        .start_of_messages = 0x667d,
        .start_of_messages_2 = 0x75d0,

        TOT_PICTURES,
    },

    {
        .Title = "Temple of Terror text only version",
        .gameID = TOT_TEXT_ONLY,
        .type = HEMAN_TYPE,
        .base_game = TEMPLE_OF_TERROR,

        TOT_TEXT_ONLY_TABLES,
    },

    {
        .Title = "Temple of Terror combined version",
        .gameID = TOT_HYBRID,
        .type = HEMAN_TYPE,
        .base_game = TEMPLE_OF_TERROR,

        TOT_TEXT_ONLY_TABLES,

        TOT_PICTURES,
    },

    {
        .Title = "Temple of Terror C64",
        .gameID = TEMPLE_OF_TERROR_64,
        .type = HEMAN_TYPE,
        .base_game = TEMPLE_OF_TERROR,

        .start_of_dictionary = 0x5b08,
        .start_of_tokens = 0x603f,
        .token_signature = "\x80 There is",
        .start_of_room_descriptions = 0x631e,
        .start_of_item_descriptions = 0x1301,
        .start_of_status_table = 0x0f68,
        .start_of_event_table = 0x0008,
        .start_of_room_connections = 0x0dc0,
        .start_of_flags = 0x2aa6,
        .start_of_item_locations = 0x0cf8,
        .start_of_messages = 0x6bba,
        .start_of_messages_2 = 0x7b0d,

        .start_of_tiles = 0x8048,
        .start_of_image_blocks = 0x86b0,
        .start_of_image_instructions = 0x4708,
        .number_of_pictures = 143,
        .palette = C64TAYLOR,
    },

    {
        .Title = "Temple of Terror C64 text only version",
        .gameID = TOT_TEXT_ONLY_64,
        .type = HEMAN_TYPE,
        .base_game = TEMPLE_OF_TERROR,

        TOT_TEXT_ONLY_TABLES_64,

        .palette = C64TAYLOR,
    },

    {
        .Title = "Temple of Terror C64 combined version",
        .gameID = TOT_HYBRID_64,
        .type = HEMAN_TYPE,
        .base_game = TEMPLE_OF_TERROR,

        TOT_TEXT_ONLY_TABLES_64,

        /* Where c64decrunch.c splices them: HYBRID_IMAGES_TO, HYBRID_TILES_TO */
        .start_of_tiles = 0x8888,
        .start_of_image_blocks = 0x8ef0,
        .start_of_image_instructions = 0x16ea,
        .number_of_pictures = 143,
        .palette = C64TAYLOR,
    },

    {
        .Title = "Kayleth",
        .gameID = KAYLETH,
        .type = HEMAN_TYPE,
        .base_game = KAYLETH,

        .start_of_dictionary = 0x5323,
        .start_of_tokens = 0x7fee,
        .start_of_room_descriptions = 0x6eb3,
        .start_of_item_descriptions = 0x7870,
        .start_of_status_table = 0x4f4d,
        .start_of_event_table = 0x3a48,
        .start_of_room_connections = 0x4de0,
        .start_of_flags = 0x20f3,
        .start_of_item_locations = 0x4d5f,
        .start_of_messages = 0x5a99,
        .start_of_messages_2 = 0x769d,

        .start_of_tiles = 0x95f0,
        .start_of_image_blocks = 0x9ce0,
        .image_patterns_lookup = 0x38b6,
        .number_of_patterns = 0x001f,
        .pattern_end_marker = 0x008e,
        .start_of_image_instructions = 0x8279,
        .number_of_pictures = 210,
        .palette = ZXOPT,
    },

    {
        .Title = "Kayleth C64",
        .gameID = KAYLETH_64,
        .type = HEMAN_TYPE,
        .base_game = KAYLETH,

        .start_of_dictionary = 0x175f,
        .start_of_tokens = 0xbace,
        .token_signature = "\x80You are i",
        .start_of_room_descriptions = 0xa993,
        .start_of_item_descriptions = 0xb350,
        .start_of_status_table = 0x1389,
        .start_of_event_table = 0x8074,
        .start_of_room_connections = 0x940c,
        .start_of_flags = 0x2a81,
        .start_of_item_locations = 0x938b,
        .start_of_messages = 0x9579,
        .start_of_messages_2 = 0xb17d,

        .start_of_tiles = 0xc028,
        .start_of_image_blocks = 0x5b28,
        .start_of_image_instructions = 0x0012,
        .number_of_pictures = 210,
        .palette = C64TAYLOR,
    },

    {
        .Title = "Unknown game",
        .gameID = UNKNOWN_GAME,
        .type = NO_TYPE,
        .base_game = UNKNOWN_GAME,
    }
};
