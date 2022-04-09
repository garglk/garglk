//
//  gameinfo.c
//  scott
//
//  Created by Administrator on 2022-01-30.
//

#include <stdio.h>

#include "definitions.h"

const struct GameInfo games[] = {
    {
        "The Golden Baton",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        48,  // Number of items
        166, // Number of actions
        78,  // Number of words
        31,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        99,  // Number of messages
    },

    {
        "The Time Machine",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        62,  // Number of items
        164, // Number of actions
        87,  // Number of words
        44,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        73,  // Number of messages
    },

    {
        "Arrow of Death part 1",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        64,  // Number of items
        150, // Number of actions
        90,  // Number of words
        52,  // Number of rooms
        5,   // Max carried items
        4,   // Word length
        82,  // Number of messages
    },

    {
        "Arrow of Death part 2",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        90,  // Number of items
        176, // Number of actions
        82,  // Number of words
        65,  // Number of rooms
        9,   // Max carried items
        4,   // Word length
        87,  // Number of messages
    },

    {
        "Escape from Pulsar 7",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        90,  // Number of items
        220, // Number of actions
        145,  // Number of words
        45,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        75,  // Number of messages
    },

    {
        "Circus",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        65,  // Number of items
        165, // Number of actions
        97,  // Number of words
        36,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        72,  // Number of messages
    },

    {
        "Feasibility Experiment",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        65,  // Number of items
        156, // Number of actions
        79,  // Number of words
        59,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        65,  // Number of messages
    },

    {
        "The Wizard of Akyrz",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        49,  // Number of items
        201, // Number of actions
        85,  // Number of words
        40,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        99,  // Number of messages
    },

    {
        "Perseus and Andromeda",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        59,  // Number of items
        165, // Number of actions
        130,  // Number of words
        40,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        96,  // Number of messages
    },

    {
        "Ten Little Indians",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        73,  // Number of items
        161, // Number of actions
        85,  // Number of words
        63,  // Number of rooms
        5,   // Max carried items
        4,   // Word length
        67,  // Number of messages
    },

    {
        "Waxworks",
        SCOTTFREE,
        TEXT_ONLY,                 // type
        MYSTERIOUS,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        57,  // Number of items
        189, // Number of actions
        105,  // Number of words
        41,  // Number of rooms
        6,   // Max carried items
        4,   // Word length
        91,  // Number of messages
    },

    {
        NULL
    }
};

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
    "\nI fell and broke my neck.",
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
    "I carry nothing. ",
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
    "I carry nothing. ",
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
    "I carry nothing. ",
    "My light is growing dim. ",
    " ",
    " ",
    ". ",
    "What ? ",
    NULL
};
