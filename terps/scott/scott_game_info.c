//
//  scott_game_info.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <stdio.h>

#include "scott_defines.h"

// clang-format off

const GameInfo games[] = {
    {
        .Title = "Pirate Adventure",
        .gameID = PIRATE,
        .type = OLD_STYLE,
        .subtype = ENGLISH,
        .dictionary = THREE_LETTER_UNCOMPRESSED,

        .number_of_items = 66,
        .number_of_actions = 177,
        .number_of_words = 79,
        .number_of_rooms = 26,
        .max_carried = 6,
        .word_length = 3,
        .number_of_messages = 99,

        .number_of_verbs = 80,
        .number_of_nouns = 79,

        .start_of_header = 0x2451,
        .header_style = EARLY,

        .start_of_actions = 0x3a50,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x469a,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x2539,
        .start_of_directions = 0x28de,
    },

    {
        .Title = "Voodoo Castle",
        .gameID = VOODOO_CASTLE,
        .type = OLD_STYLE,
        .subtype = ENGLISH,
        .dictionary = THREE_LETTER_UNCOMPRESSED,

        .number_of_items = 65,
        .number_of_actions = 189,
        .number_of_words = 89,
        .number_of_rooms = 25,
        .max_carried = 9,
        .word_length = 3,
        .number_of_messages = 99,

        .number_of_verbs = 90,
        .number_of_nouns = 89,

        .start_of_header = 0x2451,
        .header_style = EARLY,

        .start_of_actions = 0x3a50,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x4752,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x2539,
        .start_of_directions = 0x28de,
    },

    {
        .Title = "Strange Odyssey",
        .gameID = STRANGE_ODYSSEY,
        .type = OLD_STYLE,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 55,
        .number_of_actions = 223,
        .number_of_words = 79,
        .number_of_rooms = 35,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 94,

        .number_of_verbs = 80,
        .number_of_nouns = 79,

        .start_of_header = 0x2451,
        .header_style = EARLY,

        .start_of_actions = 0x3a50,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x499a,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x2539,
        .start_of_directions = 0x28de,
    },

    {
        .Title = "Buckaroo Banzai",
        .gameID = BANZAI,
        .type = OLD_STYLE,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 60,
        .number_of_actions = 266,
        .number_of_words = 110,
        .number_of_rooms = 35,
        .max_carried = 5,
        .word_length = 4,
        .number_of_messages = 95,

        .number_of_verbs = 111,
        .number_of_nouns = 110,

        .start_of_header = 0x2451,
        .header_style = EARLY,

        .start_of_actions = 0x3a50,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x4c54,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x2539,
        .start_of_directions = 0x28e1,
    },

    {
        .Title = "The Golden Baton",
        .gameID = BATON,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 48,
        .number_of_actions = 171,
        .number_of_words = 76,
        .number_of_rooms = 31,
        .max_carried = 5,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 77,
        .number_of_nouns = 76,

        .start_of_header = 0x2349,
        .header_style = EARLY,

        .start_of_actions = 0x3b56,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x473a,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x2450,
        .start_of_directions = 0x277e,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 30,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },

    {
        .Title = "The Golden Baton C64",
        .gameID = BATON_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 48,
        .number_of_actions = 166,
        .number_of_words = 78,
        .number_of_rooms = 31,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 80,
        .number_of_nouns = 79,

        .start_of_header = 0x1dd9,
        .header_style = MYSTERIOUS_C64_HEADER,

        .start_of_actions = 0x1df1,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x2861,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x408,
        .start_of_directions = 0x408,

        .start_of_image_data = 0x38f1,
        .number_of_pictures = 30,
        .palette = C64A,
        .picture_format_version = 99,
    },

    {
        .Title = "The Time Machine",
        .gameID = TIME_MACHINE,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 62,
        .number_of_actions = 164,
        .number_of_words = 87,
        .number_of_rooms = 44,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 88,
        .number_of_nouns = 87,

        .start_of_header = 0x2351,
        .header_style = EARLY,

        .start_of_actions = 0x3b81,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x475f,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x24c0,
        .start_of_directions = 0x2780,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 43,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },

    {
        .Title = "The Time Machine C64",
        .gameID = TIME_MACHINE_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 62,
        .number_of_actions = 161,
        .number_of_words = 85,
        .number_of_rooms = 44,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 73,

        .number_of_verbs = 87,
        .number_of_nouns = 85,

        .start_of_header = 0x1dd9,
        .header_style = MYSTERIOUS_C64_HEADER,

        .start_of_actions = 0x1df1,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x2811,
        .start_of_room_descriptions = 0x2b6d,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = 0x2f0f,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x37ce,

        .start_of_system_messages = 0x0408,
        .start_of_directions = 0x0408,

        .start_of_image_data = 0x3872,
        .number_of_pictures = 43,
        .palette = C64A,
        .picture_format_version = 99,
    },


    {
        .Title = "Arrow of Death part 1",
        .gameID = ARROW1,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 64,
        .number_of_actions = 150,
        .number_of_words = 90,
        .number_of_rooms = 52,
        .max_carried = 5,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 91,
        .number_of_nouns = 83,

        .start_of_header = 0x2351,
        .header_style = EARLY,

        .start_of_actions = 0x3b81,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x46b3,
        .start_of_room_descriptions = 0x4a41,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x24c0,
        .start_of_directions = 0x2780,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 51,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },

    {
        .Title = "Arrow of Death part 1 C64",
        .gameID = ARROW1_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 64,
        .number_of_actions = 150,
        .number_of_words = 90,
        .number_of_rooms = 52,
        .max_carried = 5,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 91,
        .number_of_nouns = 82,

        .start_of_header = 0x1dd9,
        .header_style = MYSTERIOUS_C64_HEADER,

        .start_of_actions = 0x1df1,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x2761,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0408,
        .start_of_directions = 0x0408,

        .start_of_image_data = 0x38e2,
        .number_of_pictures = 51,
        .palette = C64A,
        .picture_format_version = 99,
    },

    {
        .Title = "Arrow of Death part 2",
        .gameID = ARROW2,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 91,
        .number_of_actions = 190,
        .number_of_words = 83,
        .number_of_rooms = 65,
        .max_carried = 9,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 84,
        .number_of_nouns = 83,

        .start_of_header = 0x2351,
        .header_style = EARLY,

        .start_of_actions = 0x3b81,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x49b7,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x24c0,
        .start_of_directions = 0x2780,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 64,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },

    {
        .Title = "Arrow of Death part 2 C64",
        .gameID = ARROW2_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 90,
        .number_of_actions = 176,
        .number_of_words = 82,
        .number_of_rooms = 65,
        .max_carried = 9,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 81,
        .number_of_nouns = 82,

        .start_of_header = 0x1dd9,
        .header_style = ARROW_OF_DEATH_PT_2_C64_HEADER,

        .start_of_actions = 0x1df1,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x2901,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x408,
        .start_of_directions = 0x408,

        .start_of_image_data = 0x3cac,
        .number_of_pictures = 64,
        .palette = C64A,
        .picture_format_version = 99,
    },

    {
        .Title = "Escape from Pulsar 7",
        .gameID = PULSAR7,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 90,
        .number_of_actions = 220,
        .number_of_words = 145,
        .number_of_rooms = 45,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 146,
        .number_of_nouns = 145,

        .start_of_header = 0x2351,
        .header_style = EARLY,

        .start_of_actions = 0x3b81,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x4b1d,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x24c0,
        .start_of_directions = 0x2780,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 44,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },


    {
        .Title = "Escape from Pulsar 7 C64",
        .gameID = PULSAR7_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 88,
        .number_of_actions = 195,
        .number_of_words = 145,
        .number_of_rooms = 45,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 146,
        .number_of_nouns = 102,

        .start_of_header = 0x1dd9,
        .header_style = EARLY,

        .start_of_actions = 0x1df3,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x29e3,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0408,
        .start_of_directions = 0x0408,

        .start_of_image_data = 0x3bf4,
        .number_of_pictures = 44,
        .palette = C64A,
        .picture_format_version = 99,
    },


    {
        .Title = "Circus",
        .gameID = CIRCUS,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 65,
        .number_of_actions = 165,
        .number_of_words = 97,
        .number_of_rooms = 36,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 98,
        .number_of_nouns = 97,

        .start_of_header = 0x2349,
        .header_style = EARLY,

        .start_of_actions = 0x3b56,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x471a,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x24c0,
        .start_of_directions = 0x277e,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 35,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },

    {
        .Title = "Circus C64",
        .gameID = CIRCUS_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 65,
        .number_of_actions = 165,
        .number_of_words = 97,
        .number_of_rooms = 36,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 72,

        .number_of_verbs = 98,
        .number_of_nouns = 96,

        .start_of_header = 0x1dd9,
        .header_style = MYSTERIOUS_C64_HEADER,

        .start_of_actions = 0x1df1,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x2851,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0408,
        .start_of_directions = 0x0408,

        .start_of_image_data = 0x3914,
        .number_of_pictures = 35,
        .palette = C64A,
        .picture_format_version = 99,
    },

    {
        .Title = "Feasibility Experiment",
        .gameID = FEASIBILITY,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 65,
        .number_of_actions = 164,
        .number_of_words = 82,
        .number_of_rooms = 59,
        .max_carried = 5,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 83,
        .number_of_nouns = 82,

        .start_of_header = 0x2351,
        .header_style = EARLY,

        .start_of_actions = 0x3b81,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x47bf,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x24c0,
        .start_of_directions = 0x2780,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 58,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },

    {
        .Title = "Feasibility Experiment C64",
        .gameID = FEASIBILITY_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 65,
        .number_of_actions = 156,
        .number_of_words = 79,
        .number_of_rooms = 59,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 56,
        .number_of_nouns = 80,

        .start_of_header = 0x1dd9,
        .header_style = EARLY,

        .start_of_actions = 0x1df3,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x27c3,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0408,
        .start_of_directions = 0x0408,

        .start_of_image_data = 0x3876,
        .number_of_pictures = 58,
        .palette = C64A,
        .picture_format_version = 99,
    },

    {
        .Title = "The Wizard of Akyrz",
        .gameID = AKYRZ,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 49,
        .number_of_actions = 201,
        .number_of_words = 85,
        .number_of_rooms = 40,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 86,
        .number_of_nouns = 85,

        .start_of_header = 0x2351,
        .header_style = EARLY,

        .start_of_actions = 0x3b81,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x497d,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x24c0,
        .start_of_directions = 0x2780,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 39,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },


    {
        .Title = "The Wizard of Akyrz C64",
        .gameID = AKYRZ_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 49,
        .number_of_actions = 199,
        .number_of_words = 85,
        .number_of_rooms = 40,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 67,
        .number_of_nouns = 85,

        .start_of_header = 0x1dd9,
        .header_style = MYSTERIOUS_C64_HEADER,

        .start_of_actions = 0x1df1,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x2a71,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0408,
        .start_of_directions = 0x0408,

        .start_of_image_data = 0x3bce,
        .number_of_pictures = 39,
        .palette = C64A,
        .picture_format_version = 99,
    },

    {
        .Title = "Perseus and Andromeda",
        .gameID = PERSEUS,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 60,
        .number_of_actions = 178,
        .number_of_words = 130,
        .number_of_rooms = 40,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 131,
        .number_of_nouns = 130,

        .start_of_header = 0x2351,
        .header_style = EARLY,

        .start_of_actions = 0x3b81,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x4823,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x24c0,
        .start_of_directions = 0x2780,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 39,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },

    {
        .Title = "Perseus and Andromeda C64",
        .gameID = PERSEUS_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 59,
        .number_of_actions = 165,
        .number_of_words = 130,
        .number_of_rooms = 40,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 131,
        .number_of_nouns = 82,

        .start_of_header = 0x1dd9,
        .header_style = EARLY,

        .start_of_actions = 0x1df3,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x2853,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0408,
        .start_of_directions = 0x0408,

        .start_of_image_data = 0x3d91,
        .number_of_pictures = 39,
        .palette = C64A,
        .picture_format_version = 99,
    },

    {
        .Title = "Antica Grecia (Perseus and Andromeda Italian)",
        .gameID = PERSEUS_ITALIAN,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = ITALIAN,

        .number_of_items = 60,
        .number_of_actions = 178,
        .number_of_words = 130,
        .number_of_rooms = 40,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 131,
        .number_of_nouns = 83,

        .start_of_header = 0x045d,
        .header_style = EARLY,

        .start_of_actions = 0x1c8d,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x292f,
        .start_of_room_descriptions = 0x2e4d,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x05d5,
        .start_of_directions = 0x0892,

        .start_of_image_data = 0x3faa,
        .number_of_pictures = 40,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },

    {
        .Title = "Ten Little Indians",
        .gameID = INDIANS,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 73,
        .number_of_actions = 161,
        .number_of_words = 85,
        .number_of_rooms = 63,
        .max_carried = 5,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 86,
        .number_of_nouns = 85,

        .start_of_header = 0x2351,
        .header_style = EARLY,

        .start_of_actions = 0x3b81,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x47b7,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x24c0,
        .start_of_directions = 0x2780,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 62,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },

    {
        .Title = "Ten Little Indians C64",
        .gameID = INDIANS_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 73,
        .number_of_actions = 161,
        .number_of_words = 82,
        .number_of_rooms = 63,
        .max_carried = 5,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 64,
        .number_of_nouns = 82,

        .start_of_header = 0x1dd9,
        .header_style = INDIANS_C64_HEADER,

        .start_of_actions = 0x1df0,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x2810,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0408,
        .start_of_directions = 0x0408,

        .start_of_image_data = 0x3a46,
        .number_of_pictures = 62,
        .palette = C64A,
        .picture_format_version = 99,
    },

    {
        .Title = "Waxworks",
        .gameID = WAXWORKS,
        .type = OLD_STYLE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 57,
        .number_of_actions = 189,
        .number_of_words = 106,
        .number_of_rooms = 41,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 107,
        .number_of_nouns = 106,

        .start_of_header = 0x2351,
        .header_style = EARLY,

        .start_of_actions = 0x3b81,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x48d3,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x24c0,
        .start_of_directions = 0x2780,

        .start_of_image_data = FOLLOWS,
        .number_of_pictures = 40,
        .palette = ZXOPT,
        .picture_format_version = 99,
    },

    {
        .Title = "Waxworks C64",
        .gameID = WAXWORKS_C64,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS | ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 57,
        .number_of_actions = 189,
        .number_of_words = 105,
        .number_of_rooms = 41,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 82,

        .number_of_verbs = 91,
        .number_of_nouns = 105,

        .start_of_header = 0x1dd9,
        .header_style = EARLY,

        .start_of_actions = 0x1df3,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x29d3,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x408,
        .start_of_directions = 0x408,

        .start_of_image_data = 0x3f31,
        .number_of_pictures = 40,
        .palette = C64A,
        .picture_format_version = 99,
    },

    {
        .Title = "Questprobe 1: The Hulk",
        .gameID = HULK,
        .type = NO_TYPE,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 54,
        .number_of_actions = 261,
        .number_of_words = 128,
        .number_of_rooms = 20,
        .max_carried = 10,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 128,
        .number_of_nouns = 129,

        .start_of_header = 0x4bf4,
        .header_style = US_HEADER,

        .start_of_room_image_list = 0x270c,
        .start_of_item_image_list = 0x26c8,

        .start_of_actions = 0x6087,
        .actions_style = HULK_ACTIONS,
        .start_of_dictionary = 0x4cc4,
        .start_of_room_descriptions = 0x51cd,
        .start_of_room_connections = 0x7111,
        .start_of_messages = 0x575e,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x5f3d,

        .start_of_system_messages = 0x2553,
        .start_of_directions = 0x28f7,

        .start_of_tiles = 0x281b,
        .start_of_image_data = 0x2782,
        .number_of_pictures = 43,
        .palette = ZXOPT,
    },

    {
        .Title = "Questprobe 1: The Hulk C64",
        .gameID = HULK_C64,
        .type = NO_TYPE,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 54,
        .number_of_actions = 261,
        .number_of_words = 128,
        .number_of_rooms = 20,
        .max_carried = 10,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 128,
        .number_of_nouns = 129,

        .start_of_header = 0x7867,
        .header_style = US_HEADER,

        .start_of_room_image_list = 0x2280,
        .start_of_item_image_list = 0x22ba,

        .start_of_actions = 0x8c47,
        .actions_style = HULK_ACTIONS,
        .start_of_dictionary = 0x7884,
        .start_of_room_descriptions = 0x7d8d,
        .start_of_room_connections = 0x9CD1,
        .start_of_messages = 0x831E,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x8AFD,

        // These are spread out all over the code
        .start_of_system_messages = 0,
        .start_of_directions = 0xae25,

        .start_of_tiles = 0x0d01,
        .start_of_image_data = 0x2701,
        .number_of_pictures = 43,
        .palette = C64B,
    },

    {
        .Title = "Adventureland",
        .gameID = ADVENTURELAND,
        .type = NO_TYPE,
        .subtype = ENGLISH,
        .dictionary = THREE_LETTER_UNCOMPRESSED,

        .number_of_items = 65,
        .number_of_actions = 181,
        .number_of_words = 69,
        .number_of_rooms = 33,
        .max_carried = 6,
        .word_length = 3,
        .number_of_messages = 75,

        .number_of_verbs = 70,
        .number_of_nouns = 69,

        .start_of_header = 0x2473,
        .header_style = EARLY,

        .start_of_room_image_list = 0x3ebe,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = 0x3f1f,

        .start_of_actions = 0x3f5e,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x4c10,
        .start_of_room_descriptions = 0x4e40,
        .start_of_room_connections = 0x4abe,
        .start_of_messages = 0x52c3,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x4b8a,

        .start_of_system_messages = 0x24eb,
        .start_of_directions = 0x285e,

        .start_of_tiles = 0x631e,
        .start_of_image_data = FOLLOWS,
        .image_address_offset = -0x3fe5,
        .number_of_pictures = 41,
        .palette = ZXOPT,
        .picture_format_version = 1,
    },

    {
        .Title = "Adventureland C64",
        .gameID = ADVENTURELAND_C64,
        .type = NO_TYPE,
        .subtype = ENGLISH | C64,
        .dictionary = THREE_LETTER_UNCOMPRESSED,

        .number_of_items = 62,
        .number_of_actions = 170,
        .number_of_words = 69,
        .number_of_rooms = 33,
        .max_carried = 6,
        .word_length = 3,
        .number_of_messages = 75,

        .number_of_verbs = 70,
        .number_of_nouns = 69,

        .start_of_header = 0x4146,
        .header_style = EARLY,

        .start_of_room_image_list = 0x6364,
        .start_of_item_flags = 0x6386,
        .start_of_item_image_list = 0x63c5,

        .start_of_actions = 0x4160,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x4c10,
        .start_of_room_descriptions = 0x4e40,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x6404,

        .start_of_system_messages = 0x0188,
        .start_of_directions = 0x0188,

        .start_of_tiles = 0x6482,
        .start_of_image_data = 0x6c82,
        .image_address_offset = -0x1102,
        .number_of_pictures = 41,
        .palette = C64B,
        .picture_format_version = 1,
    },

    {
        .Title = "Secret Mission",
        .gameID = SECRET_MISSION,
        .type = SECRET_MISSION_VARIANT,
        .subtype = ENGLISH,
        .dictionary = THREE_LETTER_UNCOMPRESSED,

        .number_of_items = 53,
        .number_of_actions = 164,
        .number_of_words = 64,
        .number_of_rooms = 23,
        .max_carried = 7,
        .word_length = 3,
        .number_of_messages = 81,

        .number_of_verbs = 65,
        .number_of_nouns = 65,

        .start_of_header = 0x2473,
        .header_style = EARLY,

        .start_of_room_image_list = 0x3f26,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = 0x3f70,

        .start_of_actions = 0x3fa2,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x4af0,
        .start_of_room_descriptions = 0x4cf8,
        .start_of_room_connections = 0x49f2,
        .start_of_messages = 0x4edf,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x4a82,

        .start_of_system_messages = 0x24eb,
        .start_of_directions = 0x285e,

        .start_of_tiles = 0x625d,
        .start_of_image_data = FOLLOWS,
        .image_address_offset = -0x3fe5,
        .number_of_pictures = 44,
        .palette = ZXOPT,
        .picture_format_version = 1,
    },

    {
        .Title = "Secret Mission C64",
        .gameID = SECRET_MISSION_C64,
        .type = SECRET_MISSION_VARIANT,
        .subtype = ENGLISH | C64,
        .dictionary = THREE_LETTER_UNCOMPRESSED,

        .number_of_items = 49,
        .number_of_actions = 154,
        .number_of_words = 62,
        .number_of_rooms = 23,
        .max_carried = 7,
        .word_length = 3,
        .number_of_messages = 81,

        .number_of_verbs = 60,
        .number_of_nouns = 62,

        .start_of_header = 0x4572,
        .header_style = EARLY,

        .start_of_room_image_list = 0x458c,
        .start_of_item_flags = 0x6721,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x45a4,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x4f54,
        .start_of_room_descriptions = 0x5140,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x6785,

        .start_of_system_messages = 0x2502,
        .start_of_directions = 0x2502,

        .start_of_tiles = 0x681b,
        .start_of_image_data = 0x701b,
        .image_address_offset = -0x1CD0,
        .number_of_pictures = 44,
        .palette = C64B,
        .picture_format_version = 1,
    },

    {
        .Title = "The Sorcerer of Claymorgue Castle",
        .gameID = CLAYMORGUE,
        .type = NO_TYPE,
        .subtype = ENGLISH,
        .dictionary = FIVE_LETTER_UNCOMPRESSED,

        .number_of_items = 75,
        .number_of_actions = 267,
        .number_of_words = 109,
        .number_of_rooms = 32,
        .max_carried = 10,
        .word_length = 5,
        .number_of_messages = 79,

        .number_of_verbs = 110,
        .number_of_nouns = 108,

        .start_of_header = 0x246c,
        .header_style = EARLY,

        .start_of_room_image_list = 0x3bf6,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = FOLLOWS,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x4ecf,
        .start_of_room_descriptions = 0x53f7,
        .start_of_room_connections = 0x4d6f,
        .start_of_messages = 0x5605,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x4e35,

        .start_of_system_messages = 0x24e2,
        .start_of_directions = 0x2877,

        .start_of_tiles = 0x6007,
        .start_of_image_data = 0x6807,
        .image_address_offset = -0x3fe5,
        .number_of_pictures = 37,
        .palette = ZXOPT,
        .picture_format_version = 1,
    },

    {
        .Title = "The Sorcerer of Claymorgue Castle C64",
        .gameID = CLAYMORGUE_C64,
        .type = NO_TYPE,
        .subtype = ENGLISH | C64,
        .dictionary = FIVE_LETTER_UNCOMPRESSED,

        .number_of_items = 75,
        .number_of_actions = 265,
        .number_of_words = 108,
        .number_of_rooms = 32,
        .max_carried = 10,
        .word_length = 5,
        .number_of_messages = 79,

        .number_of_verbs = 110,
        .number_of_nouns = 108,

        .start_of_header = 0x47d6,
        .header_style = EARLY,

        .start_of_room_image_list = 0x471f,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x47f0,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x5890,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0xa2c,
        .start_of_directions = 0xa2c,

        .start_of_tiles = 0x6b1a,
        .start_of_image_data = 0x731a,
        .image_address_offset = 0x6b1a,
        .number_of_pictures = 44,
        .palette = C64B,
        .picture_format_version = 1,
    },

    {
        .Title = "Questprobe 2: Spider-Man",
        .gameID = SPIDERMAN,
        .type = NO_TYPE,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 72,
        .number_of_actions = 257,
        .number_of_words = 124,
        .number_of_rooms = 40,
        .max_carried = 12,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 125,
        .number_of_nouns = 125,

        .start_of_header = 0x246b,
        .header_style = EARLY,

        .start_of_room_image_list = 0x3dd1,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = FOLLOWS,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x5036,
        .start_of_room_descriptions = 0x5518,
        .start_of_room_connections = 0x4eac,
        .start_of_messages = 0x575e,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x4fa2,

        .start_of_system_messages = 0x2553,
        .start_of_directions = 0x28f7,

        .start_of_tiles = 0x6296,
        .start_of_image_data = 0x6a96,
        .image_address_offset = -0x3fe5,
        .number_of_pictures = 41,
        .palette = ZXOPT,
        .picture_format_version = 2,
    },

    {
        .Title = "Questprobe 2: Spiderman C64",
        .gameID = SPIDERMAN_C64,
        .type = NO_TYPE,
        .subtype = ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 72,
        .number_of_actions = 245,
        .number_of_words = 124,
        .number_of_rooms = 40,
        .max_carried = 12,
        .word_length = 4,
        .number_of_messages = 98,

        .number_of_verbs = 118,
        .number_of_nouns = 124,

        .start_of_header = 0x4baf,
        .header_style = EARLY,

        .start_of_room_image_list = 0x4bc9,
        .start_of_item_flags = 0x70da,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x4bf2,
        .actions_style = UNCOMPRESSED,
        .start_of_dictionary = 0x5b52,
        .start_of_room_descriptions = 0x600c,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = 0x6b9b,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0a7b,
        .start_of_directions = 0x0a7b,

        .start_of_tiles = 0x716c,
        .start_of_image_data = 0x796c,
        .image_address_offset = 0x716c,
        .number_of_pictures = 41,
        .palette = C64C,
        .picture_format_version = 2,
    },

    {
        .Title = "Questprobe 2: Spiderman Atari 16K",
        .gameID = SPIDERMAN_ATARI8,
        .type = NO_TYPE,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 72,
        .number_of_actions = 242,
        .number_of_words = 124,
        .number_of_rooms = 40,
        .max_carried = 12,
        .word_length = 4,
        .number_of_messages = 98,

        .number_of_verbs = 118,
        .number_of_nouns = 124,

        .start_of_header = 0x1879,
        .header_style = SPIDERMAN_ATARI8_HEADER,

        .start_of_actions = 0x1891, // straight after the 24-byte header
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x2258,
        .start_of_room_descriptions = 0x2712,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0161,
        .start_of_directions = 0x0161,
    },

    {
        .Title = "Savage Island part I",
        .gameID = SAVAGE_ISLAND,
        .type = SAVAGE_ISLAND_VARIANT,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 58,
        .number_of_actions = 259,
        .number_of_words = 84,
        .number_of_rooms = 34,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 82,
        .number_of_nouns = 84,

        .start_of_header = 0x236d,
        .header_style = LATE,

        .start_of_room_image_list = 0x390c,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x39a6,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x4484,
        .start_of_room_descriptions = 0x47c7,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = 0x4b91,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x2423,
        .start_of_directions = 0x25f3,

        .start_of_tiles = 0x570f,
        .start_of_image_data = FOLLOWS,
        .image_address_offset = 0x600f,
        .number_of_pictures = 37,
        .palette = ZXOPT,
        .picture_format_version = 3,
    },

    {
        .Title = "Savage Island part I (C64)",
        .gameID = SAVAGE_ISLAND_C64,
        .type = SAVAGE_ISLAND_VARIANT,
        .subtype = ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 58,
        .number_of_actions = 259,
        .number_of_words = 84,
        .number_of_rooms = 34,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 85,
        .number_of_nouns = 84,

        .start_of_header = 0x46dc,
        .header_style = EARLY,

        .start_of_room_image_list = 0x4645,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x46f6,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x51d6,
        .start_of_room_descriptions = 0x5528,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        // 0x58f6, // start_of_messages
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0a5f,
        .start_of_directions = 0x0a5f,

        .start_of_tiles = 0x6A02,
        .start_of_image_data = 0x7202,
        .image_address_offset = 0x7302,
        .number_of_pictures = 37,
        .palette = C64B,
        .picture_format_version = 3,
    },

    {
        .Title = "Savage Island part II",
        .gameID = SAVAGE_ISLAND2,
        .type = NO_TYPE,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 48,
        .number_of_actions = 241,
        .number_of_words = 79,
        .number_of_rooms = 30,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 95,

        .number_of_verbs = 74,
        .number_of_nouns = 79,

        .start_of_header = 0x236d,
        .header_style = LATE,

        .start_of_room_image_list = 0x390c,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x398e,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x43f0,
        .start_of_room_descriptions = 0x46ed,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x2423,
        .start_of_directions = 0x25f3,

        .start_of_tiles = 0x57d0,
        .start_of_image_data = FOLLOWS,
        .image_address_offset = 0x60d0,
        .number_of_pictures = 21,
        .palette = ZXOPT,
        .picture_format_version = 3,
    },

    {
        .Title = "Savage Island part II (C64)",
        .gameID = SAVAGE_ISLAND2_C64,
        .type = NO_TYPE,
        .subtype = ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 48,
        .number_of_actions = 241,
        .number_of_words = 78,
        .number_of_rooms = 30,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 94,

        .number_of_verbs = 79,
        .number_of_nouns = 79,

        .start_of_header = 0x4654,
        .header_style = EARLY,

        .start_of_room_image_list = 0x6310,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x466e,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x50d2,
        .start_of_room_descriptions = 0x53e8,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x0a50,
        .start_of_directions = 0x0a50,

        .start_of_tiles = 0x6A02,
        .start_of_image_data = 0x7202,
        .image_address_offset = 0x7302,
        .number_of_pictures = 21,
        .palette = C64B,
        .picture_format_version = 3,
    },

    {
        .Title = "Gremlins",
        .gameID = GREMLINS,
        .type = GREMLINS_VARIANT,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 99,
        .number_of_actions = 236,
        .number_of_words = 126,
        .number_of_rooms = 42,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 98,

        .number_of_verbs = 115,
        .number_of_nouns = 126,

        .start_of_header = 0x2370,
        .header_style = LATE,

        .start_of_room_image_list = 0x3a09,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x3afe,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x45e5,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x2426,
        .start_of_directions = 0x25f6,

        .start_of_tiles = 0x5be1,
        .start_of_image_data = 0x63e1,
        .image_address_offset = 0x64e1,
        .number_of_pictures = 78,
        .palette = ZXOPT,
        .picture_format_version = 3,
    },

    {
        .Title = "Gremlins (alternative)",
        .gameID = GREMLINS,
        .type = GREMLINS_VARIANT,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 99,
        .number_of_actions = 236,
        .number_of_words = 126,
        .number_of_rooms = 42,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 98,

        .number_of_verbs = 115,
        .number_of_nouns = 126,

        .start_of_header = 0x2378,
        .header_style = LATE,

        .start_of_room_image_list = 0x3a0d,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x3b02,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x45e5,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x242e,
        .start_of_directions = 0x25FE,

        .start_of_tiles = 0x5bdd,
        .start_of_image_data = 0x63dd,
        .image_address_offset = 0x64dd,
        .number_of_pictures = 78,
        .palette = ZXOPT,
        .picture_format_version = 3,
    },

    {
        .Title = "Gremlins (German)",
        .gameID = GREMLINS_GERMAN,
        .type = GREMLINS_VARIANT,
        .subtype = LOCALIZED,
        .dictionary = GERMAN,

        .number_of_items = 99,
        .number_of_actions = 236,
        .number_of_words = 126,
        .number_of_rooms = 42,
        .max_carried = 6,
        .word_length = 5,
        .number_of_messages = 98,

        .number_of_verbs = 115,
        .number_of_nouns = 126,

        .start_of_header = 0x237d,
        .header_style = LATE,

        .start_of_room_image_list = 0x3a07,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x3afc,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x45d9,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x23de,
        .start_of_directions = 0x2623,

        .start_of_tiles = 0x643e,
        .start_of_image_data = 0x6c3e,
        .image_address_offset = 0x6d3e,
        .number_of_pictures = 72,
        .palette = ZX,
        .picture_format_version = 3,
    },

    {
        .Title = "Gremlins C64",
        .gameID = GREMLINS_C64,
        .type = GREMLINS_VARIANT,
        .subtype = ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 99,
        .number_of_actions = 243,
        .number_of_words = 126,
        .number_of_rooms = 42,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 115,
        .number_of_nouns = 126,

        .start_of_header = 0x4584,
        .header_style = GREMLINS_C64_HEADER,

        .start_of_room_image_list = 0x465e,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x4751,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x527f,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x4596,

        .start_of_system_messages = 0x0a5e,
        .start_of_directions = 0x0a5e,

        .start_of_tiles = 0x6a01,
        .start_of_image_data = 0x7201,
        .image_address_offset = 0x7301,
        .number_of_pictures = 91,
        .palette = C64B,
        .picture_format_version = 3,
    },

    {
        .Title = "Gremlins (German, C64)",
        .gameID = GREMLINS_GERMAN_C64,
        .type = GREMLINS_VARIANT,
        .subtype = LOCALIZED | C64,
        .dictionary = GERMAN_C64,

        .number_of_items = 99,
        .number_of_actions = 243,
        .number_of_words = 125,
        .number_of_rooms = 42,
        .max_carried = 6,
        .word_length = 5,
        .number_of_messages = 98,

        .number_of_verbs = 125,
        .number_of_nouns = 124,

        .start_of_header = 0x4bb2,
        .header_style = GREMLINS_C64_HEADER,

        .start_of_room_image_list = 0x4c8c,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x4d81,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x585e,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x4bc4,

        .start_of_system_messages = 0x1325,
        .start_of_directions = 0x1325,

        .start_of_tiles = 0x76c5,
        .start_of_image_data = 0x7cfd,
        .image_address_offset = 0x7db3,
        .number_of_pictures = 91,
        .palette = C64B,
        .picture_format_version = 3,
    },

    {
        .Title = "Gremlins (Spanish)",
        .gameID = GREMLINS_SPANISH,
        .type = GREMLINS_VARIANT,
        .subtype = LOCALIZED,
        .dictionary = SPANISH,

        .number_of_items = 99,
        .number_of_actions = 236,
        .number_of_words = 126,
        .number_of_rooms = 42,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 98,

        .number_of_verbs = 115,
        .number_of_nouns = 126,

        .start_of_header = 0x23c5,
        .header_style = LATE,

        .start_of_room_image_list = 0x3993,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x3a88,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x455f,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x2426,
        .start_of_directions = 0x25c4,

        .start_of_tiles = 0x6171,
        .start_of_image_data = 0x6971,
        .image_address_offset = 0x6A71,
        .number_of_pictures = 74,
        .palette = ZXOPT,
        .picture_format_version = 3,
    },

    {
        .Title = "Gremlins (Spanish C64)",
        .gameID = GREMLINS_SPANISH_C64,
        .type = GREMLINS_VARIANT,
        .subtype = LOCALIZED,
        .dictionary = SPANISH_C64,

        .number_of_items = 99,
        .number_of_actions = 243,
        .number_of_words = 124,
        .number_of_rooms = 42,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 98,

        .number_of_verbs = 125,
        .number_of_nouns = 124,

        .start_of_header = 0x3aef,
        .header_style = GREMLINS_C64_HEADER,

        .start_of_room_image_list = 0x3bc9,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x3cbe,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x4793,
        .start_of_room_descriptions = FOLLOWS,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x3b01,

        .start_of_system_messages = 0x0255,
        .start_of_directions = 0x0255,

        .start_of_tiles = 0x6302,
        .start_of_image_data = 0x6b02,
        .image_address_offset = 0x6c02,
        .number_of_pictures = 91,
        .palette = C64B,
        .picture_format_version = 3,
    },

    {
        .Title = "Supergran",
        .gameID = SUPERGRAN,
        .type = NO_TYPE,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 85,
        .number_of_actions = 204,
        .number_of_words = 105,
        .number_of_rooms = 39,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 101,
        .number_of_nouns = 106,

        .start_of_header = 0x236d,
        .header_style = LATE,

        .start_of_room_image_list = 0x38c8,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x399e,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x42fd,
        .start_of_room_descriptions = 0x4708,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = FOLLOWS,

        .start_of_system_messages = 0x2423,
        .start_of_directions = 0x25f3,

        .start_of_tiles = 0x5a4e,
        .start_of_image_data = FOLLOWS,
        .image_address_offset = 0x634e,
        .number_of_pictures = 47,
        .palette = ZXOPT,
        .picture_format_version = 3,
    },

    {
        .Title = "Supergran C64",
        .gameID = SUPERGRAN_C64,
        .type = NO_TYPE,
        .subtype = ENGLISH | C64,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 85,
        .number_of_actions = 204,
        .number_of_words = 105,
        .number_of_rooms = 39,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 101,
        .number_of_nouns = 105,

        .start_of_header = 0x4624,
        .header_style = SUPERGRAN_C64_HEADER,

        .start_of_room_image_list = 0x4636,
        .start_of_item_flags = FOLLOWS,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x47b5,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x5119,
        .start_of_room_descriptions = 0x5524,
        .start_of_room_connections = FOLLOWS,
        .start_of_messages = FOLLOWS,
        .start_of_item_descriptions = FOLLOWS,
        .start_of_item_locations = 0x470a,

        .start_of_system_messages = 0x0a53,
        .start_of_directions = 0x0a53,

        .start_of_tiles = 0x6a02,
        .start_of_image_data = FOLLOWS,
        .image_address_offset = 0x7302,
        .number_of_pictures = 49,
        .palette = C64B,
        .picture_format_version = 3,
    },

    {
        .Title = "Robin of Sherwood",
        .gameID = ROBIN_OF_SHERWOOD,
        .type = SHERWOOD_VARIANT,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_COMPRESSED,

        .number_of_items = 87,
        .number_of_actions = 295,
        .number_of_words = 114,
        .number_of_rooms = 93,
        .max_carried = 10,
        .word_length = 4,
        .number_of_messages = 98,

        .number_of_verbs = 115,
        .number_of_nouns = 109,

        .start_of_header = 0x3b5a,
        .header_style = LATE,

        .start_of_room_image_list = 0, // 0x3d99, zero because it needs custom handling
        .start_of_item_flags = 0x3db8,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x409b,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x4dc3,
        .start_of_room_descriptions = 0, // 0x9b53, zero because of custom handling
        .start_of_room_connections = 0x3e67,
        .start_of_messages = 0x5147,
        .start_of_item_descriptions = 0x5d65,
        .start_of_item_locations = 0x4d6b,

        .start_of_system_messages = 0x250b,
        .start_of_directions = 0x26b5,

        .start_of_tiles = 0x614f,
        .start_of_image_data = 0x66bf,
        .image_address_offset = 0x6765,
        .number_of_pictures = 83,
        .palette = ZXOPT,
        .picture_format_version = 4,
    },

    {
        .Title = "Robin of Sherwood C64",
        .gameID = ROBIN_OF_SHERWOOD_C64,
        .type = SHERWOOD_VARIANT,
        .subtype = ENGLISH | C64,
        .dictionary = FOUR_LETTER_COMPRESSED,

        .number_of_items = 87,
        .number_of_actions = 296,
        .number_of_words = 114,
        .number_of_rooms = 93,
        .max_carried = 10,
        .word_length = 4,
        .number_of_messages = 98,

        .number_of_verbs = 115,
        .number_of_nouns = 109,

        .start_of_header = 0x1f85,
        .header_style = ROBIN_C64_HEADER,

        .start_of_room_image_list = 0, // zero because it needs custom handling
        .start_of_item_flags = 0x201c,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x252b,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x320a,
        .start_of_room_descriptions = 0, // 0x9b53, zero because of custom handling
        .start_of_room_connections = 0x20cc,
        .start_of_messages = 0x358e,
        .start_of_item_descriptions = 0x4215,
        .start_of_item_locations = 0x1fa5,

        .start_of_system_messages = 0x428,
        .start_of_directions = 0x428,

        .start_of_tiles = 0x45ff,
        .start_of_image_data = 0x4b6f,
        .image_address_offset = 0x4c23,
        .number_of_pictures = 90,
        .palette = C64B,
        .picture_format_version = 4,
    },

    {
        .Title = "Seas of Blood",
        .gameID = SEAS_OF_BLOOD,
        .type = SEAS_OF_BLOOD_VARIANT,
        .subtype = ENGLISH,
        .dictionary = FOUR_LETTER_COMPRESSED,

        .number_of_items = 125,
        .number_of_actions = 344,
        .number_of_words = 134,
        .number_of_rooms = 83,
        .max_carried = 10,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 69,
        .number_of_nouns = 134,

        .start_of_header = 0x494d,
        .header_style = LATE,

        .start_of_item_flags = 0x4961,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = FOLLOWS,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x591b,
        .start_of_room_descriptions = 0x67cb,
        .start_of_room_connections = 0x5c4b,
        .start_of_messages = 0x5ebb,
        .start_of_item_descriptions = 0x6ce0,
        .start_of_item_locations = 0x5e3d,

        .start_of_system_messages = 0x24ed,
        .start_of_directions = 0x26fc,

        .start_of_tiles = 0x7389,
        .start_of_image_data = 0x7a89,
        .image_address_offset = 0x7b9f,
        .number_of_pictures = 139,
        .palette = ZXOPT,
        .picture_format_version = 4,
    },

    {
        .Title = "Seas of Blood C64",
        .gameID = SEAS_OF_BLOOD_C64,
        .type = SEAS_OF_BLOOD_VARIANT,
        .subtype = ENGLISH | C64,
        .dictionary = FOUR_LETTER_COMPRESSED,

        .number_of_items = 125,
        .number_of_actions = 347,
        .number_of_words = 134,
        .number_of_rooms = 82,
        .max_carried = 10,
        .word_length = 4,
        .number_of_messages = 99,

        .number_of_verbs = 69,
        .number_of_nouns = 134,

        .start_of_header = 0x527d,
        .header_style = SEAS_OF_BLOOD_C64_HEADER,

        .start_of_item_flags = 0x5a73,
        .start_of_item_image_list = FOLLOWS,

        .start_of_actions = 0x5b69,
        .actions_style = COMPRESSED,
        .start_of_dictionary = 0x6a2d,
        .start_of_room_descriptions = 0x78dd,
        .start_of_room_connections = 0x6d5d,
        .start_of_messages = 0x6fcd,
        .start_of_item_descriptions = 0x7df2,
        .start_of_item_locations = 0x6f4f,

        .start_of_system_messages = 0x0b9d,
        .start_of_directions = 0x0b9d,

        .start_of_tiles = 0x84a5,
        .start_of_image_data = 0x8ba5,
        .image_address_offset = 0x8CBB,
        .number_of_pictures = 139,
        .palette = C64D,
        .picture_format_version = 4,
    },

    {
        .Title = "The Golden Baton",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 48,
        .number_of_actions = 166,
        .number_of_words = 78,
        .number_of_rooms = 31,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 99,
    },

    {
        .Title = "The Time Machine DAT version",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 62,
        .number_of_actions = 164,
        .number_of_words = 87,
        .number_of_rooms = 44,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 73,
    },

    {
        .Title = "Arrow of Death part 1 DAT version",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 64,
        .number_of_actions = 150,
        .number_of_words = 90,
        .number_of_rooms = 52,
        .max_carried = 5,
        .word_length = 4,
        .number_of_messages = 82,
    },

    {
        .Title = "Arrow of Death part 2 DAT version",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 90,
        .number_of_actions = 176,
        .number_of_words = 82,
        .number_of_rooms = 65,
        .max_carried = 9,
        .word_length = 4,
        .number_of_messages = 87,
    },

    {
        .Title = "Escape from Pulsar 7 DAT version",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 90,
        .number_of_actions = 220,
        .number_of_words = 145,
        .number_of_rooms = 45,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 75,
    },

    {
        .Title = "Circus DAT version",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 65,
        .number_of_actions = 165,
        .number_of_words = 97,
        .number_of_rooms = 36,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 72,
    },

    {
        .Title = "Feasibility Experiment DAT version",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 65,
        .number_of_actions = 156,
        .number_of_words = 79,
        .number_of_rooms = 59,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 65,
    },

    {
        .Title = "The Wizard of Akyrz DAT version",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 49,
        .number_of_actions = 201,
        .number_of_words = 85,
        .number_of_rooms = 40,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 99,
    },

    {
        .Title = "Perseus and Andromeda DAT version",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 59,
        .number_of_actions = 165,
        .number_of_words = 130,
        .number_of_rooms = 40,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 96,
    },

    {
        .Title = "Ten Little Indians DAT version",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 73,
        .number_of_actions = 161,
        .number_of_words = 85,
        .number_of_rooms = 63,
        .max_carried = 5,
        .word_length = 4,
        .number_of_messages = 67,
    },

    {
        .Title = "Waxworks DAT version",
        .gameID = SCOTTFREE,
        .type = NO_TYPE,
        .subtype = MYSTERIOUS,
        .dictionary = FOUR_LETTER_UNCOMPRESSED,

        .number_of_items = 57,
        .number_of_actions = 189,
        .number_of_words = 105,
        .number_of_rooms = 41,
        .max_carried = 6,
        .word_length = 4,
        .number_of_messages = 91,
    },

    { NULL }
};

// clang-format on

/* This is supposed to be the original ScottFree system
 messages in second person, as far as possible */
const char *sysdict[MAX_SYSMESS] = {
    "North",
    "South",
    "East",
    "West",
    "Up",
    "Down",
    "The game is now over. Play again?",
    "You have stored",
    "treasures. ",
    "On a scale of 0 to 100 that rates",
    "O.K.",
    "O.K.",
    "O.K. ",
    "Well done.\n",
    "I don't understand ",
    "You can't do that yet. ",
    "Huh ? ",
    "Give me a direction too. ",
    "You haven't got it. ",
    "You have it. ",
    "You don't see it. ",
    "It is beyond your power to do that. ",
    "\nDangerous to move in the dark! ",
    "You fell down and broke your neck. ",
    "You can't go in that direction. ",
    "I don't know how to \"",
    "\" something. ",
    "I don't know what a \"",
    "\" is. ",
    "You can't see. It is too dark!\n",
    "You are in a ",
    "\nYou can also see: ",
    "Obvious exits: ",
    "You are carrying:\n",
    "Nothing.\n",
    "Tell me what to do ? ",
    "<HIT ENTER>",
    "Light has run out. ",
    "Light runs out in",
    "turns! ",
    "You are carrying too much. \n",
    "You're dead. ",
    "Resume a saved game? ",
    "None",
    "There's nothing here to take. ",
    "You carry nothing. ",
    "Your light is growing dim. ",
    ", ",
    "\n",
    " - ",
    "What ?",
    "yes",
    "no",
    "Answer yes or no.\n",
    "Are you sure? ",
    "Move undone. ",
    "Can't undo on first turn. ",
    "No more undo states stored. ",
    "Saved. ",
    "You can't use ALL with that verb. ",
    "Transcript is now off.\n",
    "Transcript is now on.\n",
    "No transcript is currently running.\n",
    "A transcript is already running.\n",
    "Failed to create transcript file. ",
    "Start of transcript\n\n",
    "\n\nEnd of transcript\n",
    "BAD DATA! Invalid save file.\n",
    "State saved.\n",
    "State restored.\n",
    "No saved state exists.\n"
};

/* These are supposed to be the original ScottFree system
 messages in first person, as far as possible */
const char *sysdict_i_am[MAX_SYSMESS] = {
    "North",
    "South",
    "East",
    "West",
    "Up",
    "Down",
    "The game is now over. Play again?",
    "You have stored",
    "treasures. ",
    "On a scale of 0 to 100 that rates",
    "O.K.",
    "O.K.",
    "O.K. ",
    "Well done.\n",
    "I don't understand ",
    "I can't do that yet. ",
    "Huh ? ",
    "Give me a direction too.",
    "I'm not carrying it. ",
    "I already have it. ",
    "I don't see it here. ",
    "It is beyond my power to do that. ",
    "Dangerous to move in the dark! ",
    "\nI fell and broke my neck. ",
    "I can't go in that direction. ",
    "I don't know how to \"",
    "\" something. ",
    "I don't know what a \"",
    "\" is. ",
    "I can't see. It is too dark!\n",
    "I'm in a ",
    "\nI can also see: ",
    "Obvious exits: ",
    "I'm carrying: \n",
    "Nothing.\n",
    "Tell me what to do ? ",
    "<HIT ENTER>",
    "Light has run out. ",
    "Light runs out in",
    "turns! ",
    "I've too much to carry. \n",
    "I'm dead. ",
    "Resume a saved game? ",
    "None",
    "There's nothing here to take. ",
    "I have nothing to drop. ",
    "My light is growing dim. ",
    NULL
};

/* These are supposed to be the original TI-99/4A system
 messages in first person, as far as possible */
const char *sysdict_TI994A[MAX_SYSMESS] = {
    "North",
    "South",
    "East",
    "West",
    "Up",
    "Down",
    "This adventure is over. Play again?",
    "You have stored",
    "treasures. ",
    "On a scale of 0 to 100 that rates",
    "OK. ",
    "OK. ",
    "OK. ",
    "Well done.\n",
    "I don't understand the command. ",
    "I can't do that yet. ",
    "Huh? ",
    "Give me a direction too.",
    "I'm not carrying it. ",
    "I already have it. ",
    "I don't see it here. ",
    "It is beyond my power to do that. ",
    "Dangerous to move in the dark!\n",
    "\nI fell down and broke my neck.",
    "I can't go in that direction. ",
    "I don't know how to \"",
    "\" something. ",
    "I don't know what a \"",
    "\" is. ",
    "I can't see. It is too dark!\n",
    "I am in a ",
    "\nVisible items are : ",
    "Obvious exits : ",
    "I am carrying : ",
    "Nothing. ",
    "What shall I do? ",
    "<HIT ENTER>",
    "Light went out! ",
    "Light runs out in",
    "turns! ",
    "I am carrying too much.\n",
    "I'm dead... ",
    "Resume a saved game? ",
    "None",
    "There's nothing here to take. ",
    "I have nothing to drop. ",
    "Light is growing dim ",
    ", ",
    " ",
    ", ",
    NULL,
};

const char *sysdict_zx[MAX_SYSMESS] = {
    "NORTH",
    "SOUTH",
    "EAST",
    "WEST",
    "UP",
    "DOWN",
    "The Adventure is over. Want to try this Adventure again? ",
    "I've stored",
    "Treasures. ",
    "On a scale of 0 to 100 that rates",
    "Dropped.",
    "Taken.",
    "O.K. ",
    "FANTASTIC! You've solved it ALL! \n",
    "I must be stupid, but I just don't understand what you mean ",
    "I can't do that...yet! ",
    "Huh? ",
    "I need a direction too. ",
    "I'm not carrying it. ",
    "I already have it. ",
    "I don't see it here. ",
    "It's beyond my Power to do that. ",
    "It's dangerous to move in the dark! ",
    "\nI fell and broke my neck! I'm DEAD! ",
    "I can't go in that direction. ",
    "I don't know how to \"",
    "\" something. ",
    "I don't know what a \"",
    "\" is. ",
    "It's too dark to see!\n",
    "I am in a ",
    ". Visible items:\n",
    "Exits: ",
    "I'm carrying the following: ",
    "Nothing at all. ",
    "---TELL ME WHAT TO DO ? ",
    "<HIT ENTER> ",
    "Light has run out. ",
    "Light runs out in",
    "turns! ",
    "I'm carrying too much! Try: TAKE INVENTORY. ",
    "I'm DEAD!! ",
    "Restore a previously saved game ? ",
    "None",
    "There's nothing here to take. ",
    "I have nothing to drop. ",
    "My light is growing dim. ",
    " ",
    " ",
    ". ",
    "What ? ",
    NULL
};
