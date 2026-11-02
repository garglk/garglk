//
//  gameinfo.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <stdio.h>

#include "definitions.h"
#include "gameinfo.h"

const GameInfo games[] = {
    {
        "Buckaroo Banzai",
        "BUCKAROO",
        BANZAI,

        27, // room images
        24, // item images
        20, // special images
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

    /* The unfinished X-Men survives as an MS-DOS database (SPL54.DAT)
       with no pictures: a maze to explore but no way to win. */
    {
        "Questprobe 4: X-Men (Unfinished)",
        "X-MEN ",
        XMEN, // game ID

        0, // no room images
        0, // no item images
        0, // no special images
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
    "O.K. ",
    "I don't understand ",
    "You can't do that yet. ",
    "Huh ? ",
    "You can't see. It is too dark!\n",
    "You are in a ",
    "\nYou can also see: ",
    "Obvious exits: ",
    "You are carrying:\n",
    "Nothing.\n",
    "Tell me what to do ? ",
    "<Hit any key>",
    "You've too much to carry. Try -TAKE INVENTORY-\n",
    "You're dead. ",
    "Resume a saved game? ",
    "None",
    ", ",
    " - ",
    "yes",
    "no",
    "\nAnswer yes or no. ",
    "Are you sure? ",
    "Move undone. ",
    "Can't undo on first turn. ",
    "No more undo states stored. ",
    "Saved. ",
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
    "No saved state exists. ",
    "No previous action exists. "
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
    "O.K. ",
    "I didn't completely understand your command. ",
    "I can't do that yet. ",
    "Huh ? ",
    "I can't see. It is too dark!\n",
    "I'm in a ",
    ", and I see here",
    "I see an exit ",
    "I'm carrying",
    " nothing at all.",
    "I WANT YOU TO ",
    "<Hit any key>",
    "I've too much to carry. Try INVENTORY.\n",
    " I'm dead... ",
    "Resume a saved game? ",
    "None",
    ", ",
    ",",
    NULL
};
