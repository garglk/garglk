//
//  gameinfo.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <stdio.h>

#include "definitions.h"

const struct GameInfo games[] = {
    {
        "Buckaroo Banzai",
        "BUCKAROO",
        BANZAI,

        27, // no room images
        24, // no item images
        20, // no special images
    },

    {
        "The Sorcerer of Claymorgue Castle",
        "Sorcerer of Claymorgue Castle. SAGA#13.",
        CLAYMORGUE,

        33, // room images
        52, // item images
        6, // special images
    },

    {
        "Questprobe 2: Spider-Man",
        "SPIDER-MAN (tm)",
        SPIDERMAN,

        18, // room images
        26, // item images
        26, // special images
    },

    {
        "Questprobe 3: Fantastic Four",
        "FF #1 ",
        FANTASTIC4, // game ID

        21, // room images
        22, // item images
        21, // special images
    },

    {
        "Questprobe 4: X-Men (Unfinished)",
        "X-MEN ",
        XMEN, // game ID

        0, // room images
        0, // item images
        0, // special images
    },

    { NULL }
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
    "<Hit any key>",
    "Light has run out. ",
    "Light runs out in",
    "turns! ",
    "You've too much to carry. Try -TAKE INVENTORY-\n",
    "You're dead. ",
    "Resume a saved game? ",
    "None",
    "There's nothing here to take. ",
    "You carry nothing. ",
    "Your light is growing dim. ",
    ", ",
    " ",
    " - ",
    "What ?",
    "yes",
    "no",
    "\nAnswer yes or no. ",
    "Are you sure? ",
    "Move undone. ",
    "Can't undo on first turn. ",
    "No more undo states stored. ",
    "Saved. ",
    "You can't use ALL with that verb. ",
    "Transcript is now off. ",
    "Transcript is now on. ",
    "No transcript is currently running. ",
    "A transcript is already running. ",
    "Failed to create transcript file. ",
    "Start of transcript\n\n",
    "\nEnd of transcript\n",
    "BAD DATA! Invalid save file. ",
    "State saved. ",
    "State restored. ",
    "No saved state exists. "
};

/* These are supposed to be the original ScottFree system
 messages in first person, as far as possible */
const char *sysdict_i_am[MAX_SYSMESS] = {
    "north",
    "south",
    "east",
    "west",
    "up",
    "down",
    "The game is now over. Play again?",
    "You have stored",
    "treasures. ",
    "On a scale of 0 to 100 that rates",
    "O.K.",
    "O.K.",
    "O.K. ",
    "Well done.\n",
    "I didn't completely understand your command. ",
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
    ", and I see here",
    "I see an exit ",
    "I'm carrying",
    " nothing at all.",
    "I WANT YOU TO ",
    "<Hit any key>",
    "Light has run out. ",
    "Light runs out in",
    "turns! ",
    "I've too much to carry. Try INVENTORY.\n",
    " I'm dead... ",
    "Resume a saved game? ",
    "None",
    "There's nothing here to take. ",
    "I have nothing to drop. ",
    "My light is growing dim. ",
    ", ",
    " ",
    ",",
    NULL
};
