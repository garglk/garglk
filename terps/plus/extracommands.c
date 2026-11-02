//
//  extracommands.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-28.
//

#include "common.h"
#include "glk.h"
#include "restorestate.h"

#include "extracommands.h"

DictWord ExtraWords[] = {
    { "IT", COM_IT },
    { "HER", COM_IT },
    { "HIM", COM_IT },
    { "THEM", COM_IT },
    { "AGAIN", COM_AGAIN },
    { "G", COM_AGAIN },
    { "REPEAT", COM_AGAIN },
    { "CONTINUE", COM_AGAIN },
    { "?", COM_WHERE },
    { "UNDO", COM_UNDO },
    { "BOM", COM_UNDO },
    { "PON", COM_TRANSCRIPT },
    { "TRANSCRIPT", COM_TRANSCRIPT },
    { "SCRIPT", COM_TRANSCRIPT },
    { "POFF", COM_TRANSCRIPT_OFF },
    { "RESTART", COM_RESTART },
    { "RAMSAVE", COM_RAMSAVE },
    { "RAMRESTORE", COM_RAMRESTORE },
    { "RAMLOAD", COM_RAMRESTORE },
    { "ON", COM_ON },
    { "OFF", COM_OFF },
    { "COMMAND", COM_MAND },
    { "MOVE", COM_MAND },
    { "TURN", COM_MAND },
    { "LOAD", COM_LOAD },
    { "RESTORE", COM_LOAD },
    { "SAVE", COM_SAVE },
    { "RAM", COM_RAM },
    { "GAME", COM_GAME },
    { "STORY", COM_GAME },
    { "SESSION", COM_GAME },

    { NULL, 0 }
};

/* The system messages are const, and Glk's string calls take a plain
   char * that they do not write to */
static void PutString(strid_t stream, const char *s)
{
    glk_put_string_stream(stream, (char *)s);
}

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

    PutString(Transcript, sys[TRANSCRIPT_START]);
    PutString(glk_window_get_stream(Bottom), sys[TRANSCRIPT_ON]);

    glk_window_set_echo_stream(Bottom, Transcript);

    Look(1);
}

static void TranscriptOff(void)
{
    if (Transcript == NULL) {
        SystemMessage(NO_TRANSCRIPT);
        return;
    }

    PutString(Transcript, sys[TRANSCRIPT_END]);

    glk_window_set_echo_stream(Bottom, NULL);

    glk_stream_close(Transcript, NULL);
    Transcript = NULL;
    SystemMessage(TRANSCRIPT_OFF);
}

ExtraCommandResult PerformExtraCommand(int command, int nextcommand)
{
    switch (command) {
    case COM_AGAIN:
        if (LastVerb == 0) {
            SystemMessage(NO_PREVIOUS_ACTION);
            return RESULT_ONE_WORD;
        } else {
            CurrentVerb = LastVerb;
            CurrentNoun = LastNoun;
            CurrentPrep = LastPrep;
            CurrentPartp = LastPartp;
            CurrentNoun2 = LastNoun2;
            CurrentAdverb = LastAdverb;
            return RESULT_AGAIN;
        }
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
        } else
            return RESULT_NOT_UNDERSTOOD;
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
