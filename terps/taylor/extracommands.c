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
#include "player.h"
#include "restorestate.h"
#include "utility.h"

strid_t Transcript = NULL;

/* Every interpreter provides this for the shared common_utils.c, whose
   Fatal() calls it after printing its message. */
GLK_ATTRIBUTE_NORETURN void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    glk_exit();
}

typedef enum {
    EXTRA_NONE,
    EXTRA_RESTART,
    EXTRA_RESTORE,
    EXTRA_SAVE,
    EXTRA_RAMSAVE,
    EXTRA_RAMLOAD,
    EXTRA_UNDO,
    EXTRA_RAM,
    EXTRA_SCRIPT,
    EXTRA_GAME,
    EXTRA_COMMAND,
    EXTRA_ON,
    EXTRA_OFF,
    EXTRA_OTHER_WORD /* A word of the game's dictionary */
} extra_command;

/* Words understood by the interpreter itself, on top of the game's
   dictionary. */
static const struct {
    const char *word;
    extra_command command;
} ExtraCommands[] = {
    { "restart", EXTRA_RESTART },
    { "#restart", EXTRA_RESTART },
    { "save", EXTRA_SAVE },
    { "#save", EXTRA_SAVE },
    { "restore", EXTRA_RESTORE },
    { "load", EXTRA_RESTORE },
    { "#restore", EXTRA_RESTORE },
    { "bom", EXTRA_UNDO },
    { "oops", EXTRA_UNDO },
    { "undo", EXTRA_UNDO },
    { "#undo", EXTRA_UNDO },
    { "ram", EXTRA_RAM },
    { "ramload", EXTRA_RAMLOAD },
    { "qload", EXTRA_RAMLOAD },
    { "quickload", EXTRA_RAMLOAD },
    { "ramrestore", EXTRA_RAMLOAD },
    { "#quickload", EXTRA_RAMLOAD },
    { "#qload", EXTRA_RAMLOAD },
    { "quicksave", EXTRA_RAMSAVE },
    { "qsave", EXTRA_RAMSAVE },
    { "ramsave", EXTRA_RAMSAVE },
    { "#quicksave", EXTRA_RAMSAVE },
    { "#qsave", EXTRA_RAMSAVE },
    { "game", EXTRA_GAME },
    { "story", EXTRA_GAME },
    { "move", EXTRA_COMMAND },
    { "command", EXTRA_COMMAND },
    { "turn", EXTRA_COMMAND },
    { "#script", EXTRA_SCRIPT },
    { "script", EXTRA_SCRIPT },
    { "#transcript", EXTRA_SCRIPT },
    { "transcript", EXTRA_SCRIPT },
    { "on", EXTRA_ON },
    { "off", EXTRA_OFF },
    { NULL, EXTRA_NONE }
};

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

    glk_put_string_stream(Transcript, "Start of transcript\n\n");
    Display(Bottom, "Transcript is now on.\n");
    glk_window_set_echo_stream(Bottom, Transcript);
}

static void TranscriptOff(void)
{
    if (Transcript == NULL) {
        Display(Bottom, "No transcript is currently running.\n");
        return;
    }

    glk_put_string_stream(Transcript, "\n\nEnd of transcript\n");

    glk_stream_close(Transcript, NULL);
    glk_window_set_echo_stream(Bottom, NULL);
    Transcript = NULL;
    Display(Bottom, "Transcript is now off.\n");
}

static void RestoreGame(void)
{
    LoadGame();
}

static void AskRestart(void)
{
    Display(Bottom, "Restart? (Y/N) ");
    if (YesOrNo()) {
        ShouldRestart = 1;
    }
}

static void Undo(void)
{
    RestoreUndo(1);
}

/* What an extra command does, by the word that follows it (EXTRA_NONE when
   it is the last word). A pair not listed here is left to the game. */
static const struct {
    extra_command verb;
    extra_command noun;
    void (*action)(void);
} ExtraActions[] = {
    { EXTRA_RESTORE, EXTRA_NONE, RestoreGame },
    { EXTRA_RESTORE, EXTRA_GAME, RestoreGame },
    { EXTRA_RESTART, EXTRA_NONE, AskRestart },
    { EXTRA_RESTART, EXTRA_GAME, AskRestart },
    { EXTRA_SAVE, EXTRA_NONE, SaveGame },
    { EXTRA_SAVE, EXTRA_GAME, SaveGame },
    { EXTRA_UNDO, EXTRA_NONE, Undo },
    { EXTRA_UNDO, EXTRA_COMMAND, Undo },
    { EXTRA_RAM, EXTRA_RESTORE, RamLoad },
    { EXTRA_RAM, EXTRA_SAVE, RamSave },
    { EXTRA_RAMSAVE, EXTRA_NONE, RamSave },
    { EXTRA_RAMLOAD, EXTRA_NONE, RamLoad },
    { EXTRA_SCRIPT, EXTRA_NONE, TranscriptOn },
    { EXTRA_SCRIPT, EXTRA_ON, TranscriptOn },
    { EXTRA_SCRIPT, EXTRA_OFF, TranscriptOff },
};

static extra_command ParseExtraCommand(const char *word)
{
    if (word == NULL)
        return EXTRA_NONE;
    for (int i = 0; ExtraCommands[i].word != NULL; i++) {
        if (EqualsIgnoringCase(word, ExtraCommands[i].word))
            return ExtraCommands[i].command;
    }
    return EXTRA_NONE;
}

int IsExtraCommand(const char *word)
{
    return ParseExtraCommand(word) != EXTRA_NONE;
}

/* GAME in RESTART GAME, ON in SCRIPT ON: words that only follow a command */
static int IsSecondWord(extra_command command)
{
    return command == EXTRA_GAME || command == EXTRA_COMMAND
        || command == EXTRA_ON || command == EXTRA_OFF;
}

int TryExtraCommand(void)
{
    if (InKaylethPreview)
        return 0;
    int verb_position = FirstWordPosition;
    extra_command verb = ParseExtraCommand(InputWordStrings[verb_position]);
    extra_command noun = EXTRA_NONE;

    /* The first word the game knows may be the second word of the command:
       GAME in RESTART GAME, or, in a game that has a SAVE of its own but no
       RAM, SAVE in RAM SAVE. */
    if (verb_position > 0
        && (IsSecondWord(verb) || ParseExtraCommand(InputWordStrings[verb_position - 1]) == EXTRA_RAM)) {
        verb_position--;
        verb = ParseExtraCommand(InputWordStrings[verb_position]);
    }
    if (verb_position + 1 < WordsInInput)
        noun = ParseExtraCommand(InputWordStrings[verb_position + 1]);
    /* A dictionary word is not an extra command, whatever its code */
    if (noun == EXTRA_NONE && KnownWords > 1)
        noun = EXTRA_OTHER_WORD;

    for (size_t i = 0; i < sizeof ExtraActions / sizeof ExtraActions[0]; i++) {
        if (ExtraActions[i].verb == verb && ExtraActions[i].noun == noun) {
            StopTime = 1;
            PrintedOK = 1;
            Redraw = 1;
            ExtraActions[i].action();
            return 1;
        }
    }
    StopTime = 0;
    Redraw = 0;
    return 0;
}
