//
//  extracommands.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-28.
//

#include "glk.h"
#include "common.h"
#include "restorestate.h"
#include "extracommands.h"

typedef enum {
    COM_IT = 1,
    COM_AGAIN = 2,
    COM_WHERE = 4,
    COM_UNDO = 5,
    COM_TRANSCRIPT = 6,
    COM_TRANSCRIPT_OFF = 7,
    COM_RESTART = 8,
    COM_ON = 9,
    COM_OFF = 10,
    COM_MAND = 12,
} ExtraWordType;

DictWord ExtraWords[] = {
    { "IT", 1 },
    { "HER", 1 },
    { "HIM", 1 },
    { "THEM", 1 },
    { "AGAIN", 2 },
    { "G", 2 },
    { "REPEAT", 2 },
    { "CONTINUE", 2 },
    { "?", 4 },
    { "UNDO", 5 },
    { "BOM", 5 },
    { "PON", 6 },
    { "TRANSCRIPT", 6 },
    { "SCRIPT", 6 },
    { "POFF", 7 },
    { "RESTART", 8 },
    { "ON", 9 },
    { "OFF", 10 },
    { "COMMAND", 12 },
    { "MOVE", 12 },
    { "TURN", 12 },

    { NULL, 0 }
};

static void TranscriptOn(void)
{
    frefid_t ref;

    if (Transcript) {
        SystemMessage(TRANSCRIPT_ALREADY);
        return;
    }

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_Transcript,
                                       filemode_Write, 0);
    if (ref == NULL)
        return;

    Transcript = glk_stream_open_file_uni(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);

    if (Transcript == NULL) {
        SystemMessage(FAILED_TRANSCRIPT);
        return;
    }

    glk_put_string_stream(Transcript, (char *)sys[TRANSCRIPT_START]);
    glk_put_string_stream(glk_window_get_stream(Bottom),
                          (char *)sys[TRANSCRIPT_ON]);

    Look(1);
}

static void TranscriptOff(void)
{
    if (Transcript == NULL) {
        SystemMessage(NO_TRANSCRIPT);
        return;
    }

    glk_put_string_stream(Transcript, (char *)sys[TRANSCRIPT_END]);

    glk_stream_close(Transcript, NULL);
    Transcript = NULL;
    SystemMessage(TRANSCRIPT_OFF);
}

int PerformExtraCommand(int command, int nextcommand) {
    switch (command) {
        case COM_AGAIN:
            CurVerb = LastVerb;
            CurNoun = LastNoun;
            CurPrep = LastPrep;
            CurPartp = LastPartp;
            CurNoun2 = LastNoun2;
            CurAdverb = LastAdverb;
            return 0;
        case COM_UNDO:
            RestoreUndo(1);
            return 1;
        case COM_RESTART:
            SystemMessage(ARE_YOU_SURE);
            if (YesOrNo())
                should_restart = 1;
            return 1;
        case COM_TRANSCRIPT:
            if (nextcommand == COM_OFF)
                TranscriptOff();
            else
                TranscriptOn();
            return 1;
        case COM_TRANSCRIPT_OFF:
            TranscriptOff();
            return 1;
        default:
            return 3;
    }
}

