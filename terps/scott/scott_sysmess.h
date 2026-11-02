//
//  scott_sysmess.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Indices of the system messages.
//

#ifndef scott_sysmess_h
#define scott_sysmess_h

/* Indices into the system-message table (compass words, parser
   responses, save/restore prompts, transcript controls). Order is
   fixed because the loader fills the table positionally. */
typedef enum {
    NORTH,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
    PLAY_AGAIN,
    IVE_STORED,
    TREASURES,
    ON_A_SCALE_THAT_RATES,
    DROPPED,
    TAKEN,
    OK,
    YOUVE_SOLVED_IT,
    I_DONT_UNDERSTAND,
    YOU_CANT_DO_THAT_YET,
    HUH,
    DIRECTION,
    YOU_HAVENT_GOT_IT,
    YOU_HAVE_IT,
    YOU_DONT_SEE_IT,
    THATS_BEYOND_MY_POWER,
    DANGEROUS_TO_MOVE_IN_DARK,
    YOU_FELL_AND_BROKE_YOUR_NECK,
    YOU_CANT_GO_THAT_WAY,
    I_DONT_KNOW_HOW_TO,
    SOMETHING,
    I_DONT_KNOW_WHAT_A,
    IS,
    TOO_DARK_TO_SEE,
    YOU_ARE,
    YOU_SEE,
    EXITS,
    INVENTORY,
    NOTHING,
    WHAT_NOW,
    HIT_ENTER,
    LIGHT_HAS_RUN_OUT,
    LIGHT_RUNS_OUT_IN,
    TURNS,
    YOURE_CARRYING_TOO_MUCH,
    IM_DEAD,
    RESUME_A_SAVED_GAME,
    NONE,
    NOTHING_HERE_TO_TAKE,
    YOU_HAVE_NOTHING,
    LIGHT_GROWING_DIM,
    EXITS_DELIMITER,
    MESSAGE_DELIMITER,
    ITEM_DELIMITER,
    WHAT,
    YES,
    NO,
    ANSWER_YES_OR_NO,
    ARE_YOU_SURE,
    MOVE_UNDONE,
    CANT_UNDO_ON_FIRST_TURN,
    NO_UNDO_STATES,
    SAVED,
    CANT_USE_ALL,
    TRANSCRIPT_OFF,
    TRANSCRIPT_ON,
    NO_TRANSCRIPT,
    TRANSCRIPT_ALREADY,
    FAILED_TRANSCRIPT,
    TRANSCRIPT_START,
    TRANSCRIPT_END,
    BAD_DATA,
    STATE_SAVED,
    STATE_RESTORED,
    NO_SAVED_STATE,
    LAST_SYSTEM_MESSAGE
} SysMessageType;

#define MAX_SYSMESS LAST_SYSTEM_MESSAGE

#endif /* scott_sysmess_h */
