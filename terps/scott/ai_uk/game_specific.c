//
//  game_specific.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-27.
//

#include <string.h>

#include "aiukpalette.h"
#include "sagagraphics.h"
#include "apple2draw.h"
#include "game_specific.h"
#include "parser.h"
#include "saga.h"
#include "sagadraw.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"
#include "scott_display.h"

/* DefinePalette() gives every palette but these three the C64 colours,
   in which white is 1 and blue is 6. The others are in Spectrum order:
   bright white is 15 and bright blue is 9. */
static int HasC64Colours(void)
{
    return palchosen != ZX && palchosen != ZXOPT && palchosen != VGA;
}

int WhiteColour(void)
{
    return HasC64Colours() ? 1 : 15;
}

int BlueColour(void)
{
    return HasC64Colours() ? 6 : 9;
}

/* Secret Mission */
enum {
    ROOM_AFTER_EXPLOSION = 1, /* The blank room the bomb leaves */
    ROOM_VISITORS_ROOM = 13, /* Large white visitors room */
};

enum {
    ITEM_SECURITY_ID = 1, /* Picture of me stamped -security- */
    ITEM_VISITOR_ID = 7,
    ITEM_MAINTENANCE_ID = 8,
    ITEM_PROJECTOR = 21, /* Movie projector with film cartridge */
    ITEM_SABOTEUR_ID = 41, /* ... stamped -window maintenance- */
};

void UpdateSecretAnimations(void)
{
    if (MyLoc != ROOM_VISITORS_ROOM || AnimationFlag == 0) {
        glk_request_timer_events(0);
        AnimationFlag = 0;
        return;
    }
    /* Roll down */
    if (AnimationFlag > 0 && AnimationFlag < 48) {
        RectFill(113, 0, 102, AnimationFlag, WhiteColour());
        AnimationFlag++;
    }
    /* Flicker */
    if (AnimationFlag >= 48 && AnimationFlag < 100) {
        if (Items[ITEM_PROJECTOR].Location == 0) {
            /* Image 30 is a white square */
            DrawImage(30);
            glk_request_timer_events(0);
            AnimationFlag = 0;
        } else if (AnimationFlag > 69) {
            if (AnimationFlag == 70)
                DrawImage(30);
            else
                DrawBlack();
            AnimationFlag = 70 + (AnimationFlag == 70);
        } else {
            AnimationFlag++;
        }
    }
    /* Roll up */
    if (AnimationFlag >= 100 && AnimationFlag < 200) {
        int height = AnimationFlag - 100;
        if (height)
            RectFill(112, 48 - height, 104, height, 0);
        AnimationFlag++;
    }
}

void SecretAction(int p)
{
    int oldimage = 0;
    switch (p) {
    case 1: /* Bomb explodes */
        if (split_screen) {
            CloseTopWindow();
            MyLoc = ROOM_AFTER_EXPLOSION;
        }
        Rooms[ROOM_AFTER_EXPLOSION].Image = 25; /* BOOM */
        DrawImage(25);
        Delay(2);
        DrawImage(26);
        Rooms[ROOM_AFTER_EXPLOSION].Image = 26; /* Mushroom cloud */
        break;
    case 2: /* ID picture */
        DrawImage(24);
        if (IsPresent(ITEM_SECURITY_ID)) {
            DrawImage(27); // "Security" label
        } else if (IsPresent(ITEM_VISITOR_ID) && !IsPresent(ITEM_MAINTENANCE_ID) && !IsPresent(ITEM_SABOTEUR_ID)) {
            DrawImage(28); /* "Visitor" label */
        }
        showing_closeup = 1;
        Output(sys[HIT_ENTER]);
        HitEnter();
        break;
    case 3: /* Screen rolls down */
    case 4:
        oldimage = Rooms[ROOM_VISITORS_ROOM].Image;
        /* Image 30 is a white square */
        Rooms[ROOM_VISITORS_ROOM].Image = 30;
        DrawBlack();
        AnimationFlag = 1;
        glk_request_timer_events(10);
        Output(sys[HIT_ENTER]);
        Output("\n");
        HitEnter();
        DrawImage(30);
        AnimationFlag = 100;
        glk_request_timer_events(5);
        event_t ev;
        do {
            glk_select(&ev);
            Updates(ev);
        } while (AnimationFlag < 200);
        Rooms[ROOM_VISITORS_ROOM].Image = oldimage;
        AnimationFlag = 0;
        break;
    default:
        fprintf(stderr, "Secret Mission: Unsupported action parameter %d!\n", p);
        return;
    }
}

void AdventurelandDarkness(void)
{
    if ((Rooms[MyLoc].Image & 128) == 128)
        SetDark();
    else
        SetLight();
}

void AdventurelandAction(int p)
{
    int image = 0;
    switch (p) {
    case 1:
        /* BOOM */
        image = 36;
        break;
    case 2:
        /* Genie */
        image = 34;
        break;
    case 3:
        /* Dragon */
        image = 33;
        break;
    case 4:
        /* White Ox */
        image = 35;
        break;
    default:
        fprintf(stderr, "Adventureland: Unsupported action parameter %d!\n", p);
        return;
    }
    /* The original shows nothing, and does not wait for a key, when
       pictures are switched off. */
    if (!GraphicsEnabled() && !closeups_without_graphics)
        return;
    DrawImage(image);
    Output("\n");
    Output(sys[HIT_ENTER]);
    showing_closeup = 1;
    HitEnter();
    return;
}

/* Map system_messages[src_offset + i] to sys[keys[i]] for each of the
   n keys. Each port stores its system messages in its own order, so most
   of them need a key table. */
void MapSysMessages(const SysMessageType *keys, size_t n, int src_offset)
{
    for (size_t i = 0; i < n; i++)
        sys[keys[i]] = system_messages[src_offset + i];
}

/* Map the consecutive sys[] slots first to last to the consecutive
   system_messages[] starting at index src. Returns the index after the
   last one used, so that ranges which follow each other in the game's
   table can be chained without typing each start index. */
int MapSysRange(SysMessageType first, SysMessageType last, int src)
{
    for (int i = first; i <= (int)last; i++)
        sys[i] = system_messages[src - first + i];
    return src + (int)last - (int)first + 1;
}

static void CopyWordList(const char **dest, const char **src, int count)
{
    if (src == NULL)
        return;
    for (int i = 0; i < count; i++)
        dest[i] = src[i];
}

/* Switch the parser's word lists to those of another language.
   Lists passed as NULL are left as they are. */
void SetParserWordLists(const char **directions, const char **skip_list,
    const char **delimiters, const char **extra_commands, const char **extra_nouns)
{
    CopyWordList(Directions, directions, NUM_DIRECTIONS);
    CopyWordList(SkipList, skip_list, NUM_SKIPPABLE_WORDS);
    CopyWordList(DelimiterList, delimiters, NUM_DELIMITERS);
    CopyWordList(ExtraCommands, extra_commands, NUM_EXTRA_COMMANDS);
    CopyWordList(ExtraNouns, extra_nouns, NUM_EXTRA_NOUNS);
}

/* The system message order shared by the C64 ports of Adventureland,
   Secret Mission and the Mysterious Adventures. */
static const SysMessageType c64_sysmess_keys[] = {
    NORTH,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
    EXITS,
    YOU_SEE,
    YOU_ARE,
    TOO_DARK_TO_SEE,
    LIGHT_HAS_RUN_OUT,
    LIGHT_RUNS_OUT_IN,
    TURNS,
    I_DONT_KNOW_HOW_TO,
    SOMETHING,
    I_DONT_KNOW_WHAT_A,
    IS,
    YOU_CANT_GO_THAT_WAY,
    OK,
    WHAT_NOW,
    HUH,
    YOU_HAVENT_GOT_IT,
    INVENTORY,
    YOU_DONT_SEE_IT,
    THATS_BEYOND_MY_POWER,
    DANGEROUS_TO_MOVE_IN_DARK,
    DIRECTION,
    YOU_FELL_AND_BROKE_YOUR_NECK,
    YOURE_CARRYING_TOO_MUCH,
    IM_DEAD,
    PLAY_AGAIN,
    RESUME_A_SAVED_GAME,
    IVE_STORED,
    TREASURES,
    ON_A_SCALE_THAT_RATES,
    YOU_CANT_DO_THAT_YET,
    I_DONT_UNDERSTAND,
    NOTHING,
    YOUVE_SOLVED_IT
};

void Spiderman64Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        EXITS,
        YOU_SEE,
        YOU_ARE,
        TOO_DARK_TO_SEE,
        LIGHT_HAS_RUN_OUT,
        LIGHT_RUNS_OUT_IN,
        TURNS,
        I_DONT_KNOW_HOW_TO,
        SOMETHING,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        YOU_HAVENT_GOT_IT,
        INVENTORY,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DANGEROUS_TO_MOVE_IN_DARK,
        DIRECTION,
        YOU_FELL_AND_BROKE_YOUR_NECK,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        DROPPED,
        TAKEN,
        PLAY_AGAIN,
        RESUME_A_SAVED_GAME,
        IVE_STORED,
        TREASURES,
        ON_A_SCALE_THAT_RATES,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING,
        YOUVE_SOLVED_IT
    };

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

/* The text-only Atari 8-bit 16K port. Its interpreter says "OK." where the
   others say "Dropped." and "Taken.", and it has no "<HIT ENTER>". */
void SpidermanAtari8Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        NONE, /* "TAPE ERROR" */
        EXITS,
        YOU_SEE,
        YOU_ARE,
        TOO_DARK_TO_SEE,
        LIGHT_HAS_RUN_OUT,
        LIGHT_RUNS_OUT_IN,
        TURNS,
        I_DONT_KNOW_HOW_TO,
        SOMETHING,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        YOU_HAVENT_GOT_IT,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DANGEROUS_TO_MOVE_IN_DARK,
        DIRECTION,
        YOU_FELL_AND_BROKE_YOUR_NECK,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        RESUME_A_SAVED_GAME,
        IVE_STORED,
        TREASURES,
        ON_A_SCALE_THAT_RATES,
        YOU_CANT_DO_THAT_YET,
        INVENTORY,
        I_DONT_UNDERSTAND,
        NOTHING,
        PLAY_AGAIN,
        YOUVE_SOLVED_IT
    };

    const char *none = sys[NONE];

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[NONE] = none;
    /* This string follows the unterminated direction letters "NSEWUD" */
    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    /* A carriage return ends the room description in the original */
    sys[YOU_SEE] = "\nI can see:";
    sys[DROPPED] = sys[OK];
    sys[TAKEN] = sys[OK];
}

void Adventureland64Sysmess(void)
{
    MAP_SYS_MESSAGES(c64_sysmess_keys, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

void Claymorgue64Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        EXITS,
        YOU_SEE,
        YOU_ARE,
        TOO_DARK_TO_SEE,
        LIGHT_HAS_RUN_OUT,
        LIGHT_RUNS_OUT_IN,
        TURNS,
        I_DONT_KNOW_HOW_TO,
        SOMETHING,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        YOU_HAVENT_GOT_IT,
        INVENTORY,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DANGEROUS_TO_MOVE_IN_DARK,
        DIRECTION,
        YOU_FELL_AND_BROKE_YOUR_NECK,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        PLAY_AGAIN,
        RESUME_A_SAVED_GAME,
        IVE_STORED,
        TREASURES,
        ON_A_SCALE_THAT_RATES,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING,
        YOUVE_SOLVED_IT
    };

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

void Mysterious64Sysmess(void)
{
    MAP_SYS_MESSAGES(c64_sysmess_keys, 0);
    /* Here YOUVE_SOLVED_IT is the 40th message, not the 39th */
    sys[YOUVE_SOLVED_IT] = system_messages[39];

    sys[ITEM_DELIMITER] = " - ";
    sys[MESSAGE_DELIMITER] = "\n";

    sys[YOU_SEE] = "\nThings I can see:\n";

    sys[I_DONT_KNOW_HOW_TO] = "\"";
    sys[PLAY_AGAIN] = "The game is over, thanks for playing\nWant to play again ? ";

    char *dictword = NULL;
    size_t len = GameHeader.WordLength;
    for (int i = 1; i <= 6; i++) {
        dictword = MemAlloc(len + 1);
        strncpy(dictword, sys[i - 1], GameHeader.WordLength);
        dictword[len] = 0;
        Nouns[i] = dictword;
    }

    Nouns[0] = "ANY";

    switch (CurrentGame) {
    case BATON_C64:
        Nouns[79] = "CAST";
        Verbs[79] = ".";
        GameHeader.NumWords = 79;
        break;
    case TIME_MACHINE_C64:
        Verbs[86] = ".";
        break;
    case ARROW1_C64:
    case PERSEUS_C64:
        Nouns[82] = ".";
        break;
    case ARROW2_C64:
        Verbs[80] = ".";
        break;
    case PULSAR7_C64:
        Nouns[102] = ".";
        break;
    case CIRCUS_C64:
        Nouns[96] = ".";
        break;
    case FEASIBILITY_C64:
        Nouns[80] = ".";
        break;
    default:
        break;
    }
}

void PerseusItalianSysmess(void)
{
    sys[YOU_ARE] = "Sono in ";
    sys[YOU_SEE] = "\nQui posso vedere:\n";
    sys[INVENTORY] = "Ho raccolto: ";

    /* The messages that the game does not have, with ' for an accent as in
       those that it has. It asks for Y or N itself, and the tape has
       "FILE ERRATO!!  Premi un tasto" and "<CARICAMENTO> ..Premi <ENTER>"
       for its own saved games. */
    sys[HIT_ENTER] = "<Premi ENTER> ";
    sys[YOU_HAVE_IT] = "Ce l'ho gia' ! ";
    sys[NONE] = "Nessuna";
    sys[NOTHING_HERE_TO_TAKE] = "Qui non c'e' niente da prendere. ";
    sys[YOU_HAVE_NOTHING] = "Non ho niente da lasciare. ";
    sys[LIGHT_GROWING_DIM] = "La mia luce si sta affievolendo. ";
    sys[WHAT] = "Cosa ? ";
    sys[YES] = "y";
    sys[NO] = "n";
    sys[ANSWER_YES_OR_NO] = "Rispondi Y o N.\n";
    sys[ARE_YOU_SURE] = "Sei sicuro?(Y/N) ";
    sys[MOVE_UNDONE] = "Mossa annullata. ";
    sys[CANT_UNDO_ON_FIRST_TURN] = "Non si puo' annullare la prima mossa. ";
    sys[NO_UNDO_STATES] = "Non ci sono altre mosse da annullare. ";
    sys[SAVED] = "Gioco salvato. ";
    sys[CANT_USE_ALL] = "Non puoi usare TUTTO con questo verbo. ";
    sys[TRANSCRIPT_ON] = "La trascrizione e' ora attiva.\n";
    sys[TRANSCRIPT_OFF] = "La trascrizione e' ora disattivata.\n";
    sys[NO_TRANSCRIPT] = "Nessuna trascrizione in corso.\n";
    sys[TRANSCRIPT_ALREADY] = "Una trascrizione e' gia' in corso.\n";
    sys[FAILED_TRANSCRIPT] = "Impossibile creare il file di trascrizione. ";
    sys[TRANSCRIPT_START] = "Inizio della trascrizione\n\n";
    sys[TRANSCRIPT_END] = "\n\nFine della trascrizione\n";
    sys[BAD_DATA] = "FILE ERRATO!! File di salvataggio non valido.\n";
    sys[STATE_SAVED] = "Stato salvato.\n";
    sys[STATE_RESTORED] = "Stato ripristinato.\n";
    sys[NO_SAVED_STATE] = "Nessuno stato salvato.\n";

    SetParserWordLists(ItalianDirections, ItalianSkipList, ItalianDelimiterList,
        ItalianExtraCommands, ItalianExtraNouns);
}

void Supergran64Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        EXITS,
        YOU_SEE,
        YOU_ARE,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        TAKEN,
        DROPPED,
        YOU_HAVENT_GOT_IT,
        INVENTORY,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DIRECTION,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        PLAY_AGAIN,
        RESUME_A_SAVED_GAME,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING
    };

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[I_DONT_KNOW_WHAT_A] = "\"";
    sys[IS] = "\" is a word I don't know. ";
}

void SecretMission64Sysmess(void)
{
    MAP_SYS_MESSAGES(c64_sysmess_keys, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
}

/* Draw a close-up image and wait for a key press. The room image is
   redrawn after that. */
void ShowCloseup(int image)
{
    DrawImage(image);
    showing_closeup = 1;
    Output(sys[HIT_ENTER]);
    HitEnter();
}

void ShowUSCloseup(int image, int offset) {
    if (image >= 0) {
        if (Graphics)
            glk_window_clear(Graphics);
        if (DrawUSRoom(offset + image)) {
            showing_closeup = 1;
            DrawImageOrVector();
            Output(sys[HIT_ENTER]);
            HitEnter();
        }
    }
}

/* The original interpreters have a table of close-up pictures, which the
   loaders that know where it is hand over: looking at the noun of an entry
   shows its picture if its item is carried or in the room. */
/* The sixteen of an original table and a few of our own. */
#define MAX_CLOSEUPS 24

static struct {
    int item;
    int noun;
    int image;
} closeups[MAX_CLOSEUPS];

static int num_closeups = 0;
static int closeup_verb = -1;
static int closeups_are_us_pictures = 0;

/* Starts a new table. us_pictures is whether the pictures are the room
   pictures of a US release rather than the numbered ones of a UK one. */
void SetCloseupVerb(int verb, int us_pictures)
{
    closeup_verb = verb;
    closeups_are_us_pictures = us_pictures;
    num_closeups = 0;
}

void AddCloseup(int item, int noun, int image)
{
    if (num_closeups == MAX_CLOSEUPS || item < 0 || item > GameHeader.NumItems)
        return;
    closeups[num_closeups].item = item;
    closeups[num_closeups].noun = noun;
    closeups[num_closeups].image = image;
    num_closeups++;
}

/* Gives the picture of the entry for item and noun to another item and
   noun as well. Does nothing if the table has no such entry, or already
   has the other one. */
static void AddCloseupAlias(int item, int noun, int alias_item, int alias_noun)
{
    for (int i = 0; i < num_closeups; i++) {
        if (closeups[i].item == alias_item && closeups[i].noun == alias_noun)
            return;
    }
    for (int i = 0; i < num_closeups; i++) {
        if (closeups[i].item == item && closeups[i].noun == noun) {
            AddCloseup(alias_item, alias_noun, closeups[i].image);
            return;
        }
    }
}

/* Close-ups that the originals do not show but that are nice to have: an
   item that turns into another one loses its picture in the original, and
   so does an obvious other word for it. To be called once the table of
   the game has been read. */
void AddExtraCloseups(GameIDType game)
{
    switch (game) {
    case ADVENTURELAND_US:
        AddCloseupAlias(24, 23, 26, 23); /* Bees in a bottle */
        break;
    case STRANGE_ODYSSEY_US:
        AddCloseupAlias(10, 22, 11, 22); /* The other phaser */
        AddCloseupAlias(30, 8, 31, 8); /* The other hound */
        break;
    case VOODOO_CASTLE_US:
        AddCloseupAlias(40, 42, 38, 42); /* Chemicals before they are mixed */
        /* Count Cristo is LOOK COFFIN on the Apple II and LOOK MAN on the
           Atari. */
        AddCloseupAlias(27, 8, 27, 53);
        AddCloseupAlias(27, 53, 27, 8);
        break;
    case HULK_US:
    case HULK_US_PREL:
        AddCloseupAlias(27, 66, 27, 26); /* LOOK BASE for the gas outlet */
        /* Fallthrough */
    case HULK:
    case HULK_C64:
        /* LOOK GEM for the bio-gem, which only some US releases have */
        AddCloseupAlias(18, 124, 18, 41);
        break;
    default:
        break;
    }
}

int HasCloseupTable(void)
{
    return closeup_verb != -1;
}

/* For the tests. Returns 0 when there is no such entry. */
int GetCloseup(int index, int *verb, int *item, int *noun, int *image)
{
    if (index < 0 || index >= num_closeups)
        return 0;
    *verb = closeup_verb;
    *item = closeups[index].item;
    *noun = closeups[index].noun;
    *image = closeups[index].image;
    return 1;
}

/* The original searches from the end, and when a noun has several entries
   it shows the last one whose item is there. */
/* For the headless tests only, which have no graphics but still want the
   table looked up: go through with a close-up (and its key press) anyway. */
int closeups_without_graphics = 0;

void ShowCloseupOnExamine(int verb, int noun)
{
    if (verb != closeup_verb || noun == 0)
        return;
    /* Like the original, show nothing (and do not wait for a key) when
       pictures are switched off. */
    if (!GraphicsEnabled() && !closeups_without_graphics)
        return;
    for (int i = num_closeups - 1; i >= 0; i--) {
        if (closeups[i].noun != noun || !IsPresent(closeups[i].item))
            continue;
        if (closeups_are_us_pictures)
            ShowUSCloseup(closeups[i].image, 0);
        else
            ShowCloseup(closeups[i].image);
        return;
    }
}
