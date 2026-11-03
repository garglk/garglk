//
//  gamedefs.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Flags, objects, rooms, messages and dictionary words of particular
//  games that the interpreter refers to by number. The numbers are those
//  of the game file; the names come from the game's own text.
//

#ifndef gamedefs_h
#define gamedefs_h

#include "taylor.h"

/* Questprobe 3 */

enum {
    Q3_OBJ_HOLDING_THING = 1, /* "THING by the hands" */
    Q3_OBJ_HOLDING_TORCH = 2, /* "HUMAN TORCH by the hands" */
    Q3_OBJ_COVERED_IN_TAR = 7, /* "tar, which I'm covered in" */
    Q3_OBJ_WALL_OF_FIRE = 10,
    Q3_OBJ_WALL_OF_FIRE_2 = 11,
    Q3_OBJ_THING = 17,
    Q3_OBJ_TORCH = 18,
    Q3_OBJ_CANNON = 29,
    Q3_OBJ_XANDU = 33,
    Q3_OBJ_WATCH = 37, /* "REED RICHARDS' watch" */
    Q3_OBJ_ALICIA_MASTERS = 46,
    Q3_OBJ_FALLEN_STATUE = 48, /* "a fallen statue has knocked out XANDU" */
};

enum {
    Q3_ROOM_TAR_PIT = 2,
    Q3_ROOM_SUBMERGED_IN_TAR = 3,
    Q3_ROOM_OPENING = 6, /* The small office, before a hero is chosen */
    Q3_ROOM_XANDU = 22, /* The great room */
    Q3_ROOM_ORDERED_OUT_TO = 26, /* The castle entrance */
    Q3_ROOM_ROAD = 34,
};

/* 49 objects: the Spectrum code copies 49 initial locations and lists
   objects 0-48 in the room, as the BBC source does everywhere (NOBS%=48,
   loops to NOBS%+1). Only the Spectrum inventory loop stops at 47, which
   cannot show: object 48, the fallen statue, is only ever swapped with 47
   and both stay in room 22 or destroyed. All of them are low objects,
   written into the room description. */
#define Q3_NUM_OBJECTS 49

#define Q3_WAIT_COUNTER_FLAG 5 /* Turns of WAIT still to run; flag 7 in the other games */
#define Q3Restedness (Flag[7]) /* 80 is 100 percent */
#define Q3XanduPresent (Flag[39])
#define Q3OtherGuyDark (Flag[42])
#define Q3_DARK_FLAG 43 /* The dark flag of the current hero */

#define Q3_MSG_PROMPT_TORCH 8
#define Q3_MSG_PROMPT_THING 9
#define Q3_MSG_I_WANT_YOU_TO 10
#define Q3_MSG_ORDERED_OUT 24
#define Q3_MSG_WATCH 223 /* Three parts: turns, restedness, and the end */

#define Q3_STATUS_TABLE_PADDING 0x7e /* Between status table entries */

#define Q3_PIC_FALLEN_STATUE 45 /* IMAGE argument */
#define Q3_PIC_THING_SINKING 53

enum {
    Q3_LAST_DIRECTION = 6, /* BELOW, BENEATH, UNDER */
    Q3_WORD_FIRE = 44, /* also BURN, LIGHT ... */
    Q3_WORD_WALL = 54,
    Q3_WORD_UP = 57,
    Q3_WORD_DOWN = 60,
    Q3_WORD_FLAME = 77, /* the wall of fire */
    Q3_WORD_RINGMASTER = 106, /* RING, and MAST before... */
    Q3_WORD_ALICIA = 121, /* ...ALIC and MAST */
};

/* The Commodore 64 Questprobe 3 */
#define Q3_64_MSG_BLOB_RETURNS 44
#define Q3_64BlobFlag (Flag[59]) /* Cleared to stop that message repeating */

/* Temple of Terror */

#define TOT_GUARD_FLAG 28 /* Zero when the Serpent Guard kills us at the door */
#define ToTGuardBlinded (Flag[63]) /* Sand kicked in his eyes; set by the interpreter */
#define TOT_SHARED_FLAG 12 /* Also used by the final puzzle */
#define TOT_PUZZLE_FLAG 60 /* Unused by the game: takes over the puzzle's use of 12 */
#define TOT_OBJ_LOCKED_DOOR 41
#define TOT_OBJ_SERPENT_GUARD 48
#define TOT_WORD_GO 20 /* also ENTER, CLIMB, WALK, RUN */
#define TOT_WORD_DOOR 162

/* Blizzard Pass */

/* Objects 0-69 are low objects, written into the room description. The
   usual guess, a comma ending the last low object's text, does not work
   here. */
#define BP_NUM_LOW_OBJECTS 70

/* The long room descriptions (text-only game) are plain characters, each
   its own token, except that byte 0x2d is the token "your " rather than
   "-". The one hyphen among them is "rough-hewn" in room 49. */
#define BP_ROOM_CAVERN_WEST_END 49 /* "a huge cavern, at the western end" */
#define BP_TOKEN_YOUR 0x2d

/* Verbs that the parser finds as nouns first */

enum {
    BP_WORD_VERB_SKIN = 43, /* VSKI */
    BP_WORD_VERB_POLISH = 44, /* VPOL */
    BP_WORD_VERB_DRAW = 49,
    BP_WORD_DRAWBRIDGE = 134, /* BRID, DRAW */
    BP_WORD_POLISH = 175,
    BP_WORD_SKIN = 249,
};

/* He-Man */

/* The transformation: "There is a mighty flash as you cry out 'By the
   power of Grayskull.'" moves the player to this room, which has no
   description, and OOPSes back from it. */
#define HEMAN_ROOM_GRAYSKULL 83

/* Rebel Planet and Kayleth */

#define WAIT_COUNTER_FLAG 7 /* Turns of WAIT still to run */
/* The file's flag 6 says 123, but objects 120-122 have no text, and 121
   begins in a room: listed, it is an empty "You notice." */
#define KAYLETH_NUM_OBJECTS 120

/* Rooms where the event table leaves the player for the rest of the
   turn, and whose description must not be printed */

/* Rebel Planet: the inside of the lite-kube. GET X FROM CUBE and DROP X IN
   KUBE move the player here to take or drop the object, and back out on
   the next turn. There is no such room, and no picture. */
#define REBEL_ROOM_INSIDE_KUBE 250
/* Kayleth: AZAP CODE ELY moves the player into this Azap Chamber before
   the status table moves object 0, the chamber's description, after them,
   so until then the room reads only ". You could go Up." */
#define KAYLETH_ROOM_AZAP_ELY 15

/* He-Man and Kayleth, the two games whose REFRESH takes everything in the room */

#define MSG_NOTHING_TO_GET 31 /* "There is nothing to get." / "I can't see anything worth getting!" */

#endif /* gamedefs_h */
