//
//  extracommands.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "extracommands.h"
#include "parseinput.h"
#include "restorestate.h"
#include "utility.h"

typedef enum {
    NO_COMMAND,
    RESTART,
    RESTORE,
    SAVE_EXTR,
    RAMSAVE_EXTR,
    RAMLOAD_EXTR,
    UNDO,
    RAM,
    GAME,
    COMMAND,
    ALL,
    IT,
    SCRIPT,
    ON,
    OFF
} extra_command;

const char *ExtraCommands[] = {
    "restart",
    "#restart",
    "save",
    "#save",
    "restore",
    "load",
    "#restore",
    "bom",
    "oops",
    "undo",
    "#undo",
    "ram",
    "ramload",
    "qload",
    "quickload",
    "ramrestore",
    "#quickload",
    "quicksave",
    "qsave",
    "ramsave",
    "#quicksave",
    "game",
    "story",
    "move",
    "command",
    "turn",
    "#script",
    "script",
    "#transcript",
    "transcript",
    "on",
    "off",
    NULL
};

const extra_command ExtraCommandsKey[] = {
    RESTART,
    RESTART,
    SAVE_EXTR,
    SAVE_EXTR,
    RESTORE,
    RESTORE,
    RESTORE,
    UNDO,
    UNDO,
    UNDO,
    UNDO,
    RAM,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMLOAD_EXTR,
    RAMSAVE_EXTR,
    RAMSAVE_EXTR,
    RAMSAVE_EXTR,
    RAMSAVE_EXTR,
    GAME,
    GAME,
    COMMAND,
    COMMAND,
    COMMAND,
    SCRIPT,
    SCRIPT,
    SCRIPT,
    SCRIPT,
    ON,
    OFF,
    NO_COMMAND
};

extern int ShouldRestart;
extern int StopTime;
extern int Redraw;
extern int WordsInInput;

extern winid_t Bottom;

int YesOrNo(void);
void SaveGame(void);
int LoadGame(void);

static void TranscriptOn(void)
{
    frefid_t ref;

    if (Transcript) {
        Display(Bottom, "A transcript is already active. \n");
        return;
    }

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_Transcript,
        filemode_Write, 0);
    if (ref == NULL)
        return;

    Transcript = glk_stream_open_file(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);

    if (Transcript == NULL) {
        Display(Bottom, "Failed to create transcript file. ");
        return;
    }

    char *start_of_transcript = "Start of transcript\n\n";
    glk_put_string_stream(Transcript, start_of_transcript);
    glk_put_string_stream(glk_window_get_stream(Bottom),
        "Transcript is now on.\n");
}

static void TranscriptOff(void)
{
    if (Transcript == NULL) {
        Display(Bottom, "No transcript is currently running.\n");
        return;
    }

    char *end_of_transcript = "\n\nEnd of transcript\n";
    glk_put_string_stream(Transcript, end_of_transcript);

    glk_stream_close(Transcript, NULL);
    Transcript = NULL;
    glk_put_string_stream(glk_window_get_stream(Bottom),
        "Transcript is now off.\n");
}

static int ParseExtraCommand(char *p)
{
    if (p == NULL)
        return NO_COMMAND;
    size_t len = strlen(p);
    if (len == 0)
        return NO_COMMAND;
    int j = 0;
    int found = 0;
    while (ExtraCommands[j] != NULL) {
        size_t commandlen = strlen(ExtraCommands[j]);
        if (commandlen == len) {
            char *c = p;
            found = 1;
            for (int i = 0; i < len; i++) {
                if (tolower(*c++) != ExtraCommands[j][i]) {
                    found = 0;
                    break;
                }
            }
            if (found) {
                return ExtraCommandsKey[j];
            }
        }
        j++;
    }
    return NO_COMMAND;
}

extern uint8_t Word[];

int TryExtraCommand(void)
{
    int verb = ParseExtraCommand(InputWordStrings[WordPositions[0]]);
    int noun = NO_COMMAND;
    if (WordPositions[0] + 1 < WordsInInput)
        noun = ParseExtraCommand(InputWordStrings[WordPositions[0] + 1]);
    if (noun == NO_COMMAND)
        noun = Word[1];

    StopTime = 1;
    Redraw = 1;
    switch (verb) {
    case RESTORE:
        if (noun == NO_COMMAND || noun == GAME) {
            LoadGame();
            return 1;
        }
        break;
    case RESTART:
        if (noun == NO_COMMAND || noun == GAME) {
            Display(Bottom, "Restart? (Y/N) ");
            if (YesOrNo()) {
                ShouldRestart = 1;
            }
            return 1;
        }
        break;
    case SAVE_EXTR:
        if (noun == NO_COMMAND || noun == GAME) {
            SaveGame();
            return 1;
        }
        break;
    case UNDO:
        if (noun == NO_COMMAND || noun == COMMAND) {
            RestoreUndo(1);
            return 1;
        }
        break;
    case RAM:
        if (noun == RESTORE) {
            RamLoad();
            return 1;
        } else if (noun == SAVE_EXTR) {
            RamSave(1);
            return 1;
        }
        break;
    case RAMSAVE_EXTR:
        if (noun == NO_COMMAND) {
            RamSave(1);
            return 1;
        }
        break;
    case RAMLOAD_EXTR:
        if (noun == NO_COMMAND) {
            RamLoad();
            return 1;
        }
        break;
    case SCRIPT:
        if (noun == ON || noun == 0) {
            TranscriptOn();
            return 1;
        } else if (noun == OFF) {
            TranscriptOff();
            return 1;
        }
        break;
    default:
        break;
    }

    StopTime = 0;
    Redraw = 0;
    return 0;
}
