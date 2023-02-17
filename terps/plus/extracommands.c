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
    { "RAMSAVE", 9 },
    { "RAMRESTORE", 10 },
    { "RAMLOAD", 10 },
    { "ON", 11 },
    { "OFF", 12 },
    { "COMMAND", 13 },
    { "MOVE", 13 },
    { "TURN", 13 },
    { "LOAD", 14 },
    { "RESTORE", 14 },
    { "SAVE", 15 },
    { "RAM", 16 },
    { "GAME", 17 },
    { "STORY", 17 },
    { "SESSION", 17 },

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

ExtraCommandResult PerformExtraCommand(int command, int nextcommand) {
    switch (command) {
        case COM_AGAIN:
            CurVerb = LastVerb;
            CurNoun = LastNoun;
            CurPrep = LastPrep;
            CurPartp = LastPartp;
            CurNoun2 = LastNoun2;
            CurAdverb = LastAdverb;
            return RESULT_AGAIN;
        case COM_UNDO:
            RestoreUndo(1);
            if (nextcommand == COM_MAND)
                return RESULT_TWO_WORDS;
            return RESULT_ONE_WORD;
        case COM_RESTART:
            SystemMessage(ARE_YOU_SURE);
            if (YesOrNo())
                should_restart = 1;
            if (nextcommand == COM_GAME)
                return RESULT_TWO_WORDS;
            return RESULT_ONE_WORD;
        case COM_TRANSCRIPT:
            if (nextcommand == COM_OFF) {
                TranscriptOff();
                return RESULT_TWO_WORDS;
            } else
                TranscriptOn();
            if (nextcommand == COM_ON)
                return RESULT_TWO_WORDS;
            return RESULT_ONE_WORD;
        case COM_TRANSCRIPT_OFF:
            TranscriptOff();
            return RESULT_ONE_WORD;
        case COM_RAM:
            if (nextcommand == COM_LOAD) {
                RamLoad();
                return RESULT_TWO_WORDS;
            } else if (nextcommand == COM_SAVE) {
                RamSave(1);
                return RESULT_TWO_WORDS;
            } else return RESULT_NOT_UNDERSTOOD;
        case COM_RAMSAVE:
            RamSave(1);
            return RESULT_ONE_WORD;
        case COM_RAMRESTORE:
            RamLoad();
            return RESULT_ONE_WORD;
        default:
            return RESULT_NOT_UNDERSTOOD;
    }
}

