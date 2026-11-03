//
//  gremlins.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include "game_specific.h"
#include "gremlins.h"

#include "parser.h"
#include "sagadraw.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_display.h"

#define GREMLINS_ANIMATION_RATE 670

/* Rooms and items of the English, German and Spanish releases */
enum {
    ROOM_BEDROOM = 1,
    ROOM_KITCHEN = 3,
    ROOM_ON_A_ROAD = 6,
    ROOM_POOL = 10,
    ROOM_BEHIND_BAR = 16,
    ROOM_BAR = 17, /* Dotty's Tavern */
    ROOM_SQUARE = 19,
    ROOM_STAIRS = 34, /* Top of the department store */
};

enum {
    ITEM_WATER = 15, /* In the pool */
    ITEM_DART_GREMLIN = 50,
    ITEM_LADDER = 53,
    ITEM_GREMLIN_GANG = 82,
    ITEM_EMPTY_POOL = 98,
    ITEM_DAZED_STRIPE = 99,
};

/* The original only redraws a picture when something in the room changes, so
   a blender that is switched off keeps the frame it happened to stop on until
   then. We redraw on every turn, and have to put that frame back. */
static int blender_frame = 0; /* The blender frame on screen, 0 if none */
static int kitchen_contents = -1; /* What was in the kitchen when it stopped */

static int KitchenContents(void)
{
    unsigned hash = 0;
    for (int ct = 0; ct <= GameHeader.NumItems; ct++)
        if (Items[ct].Location == ROOM_KITCHEN)
            hash = hash * 31 + ct + 1;
    return hash & 0x7fffffff;
}

/* Called after the room and its items have been drawn */
void GremlinsKeepBlenderFrame(void)
{
    if (MyLoc != ROOM_KITCHEN) {
        blender_frame = 0;
        return;
    }
    if (Counters[2] == 2) { /* Blender is on */
        kitchen_contents = -1;
        return;
    }
    if (blender_frame == 0)
        return;
    int contents = KitchenContents();
    if (kitchen_contents == -1)
        kitchen_contents = contents;
    if (contents == kitchen_contents)
        DrawImage(blender_frame);
    else
        blender_frame = 0;
}

void UpdateGremlinsAnimations(void)
{
    if (MyLoc != ROOM_KITCHEN)
        blender_frame = 0;
    if (Rooms[MyLoc].Image == 255) {
        glk_request_timer_events(0);
        return;
    }
    OpenGraphicsWindow();
    if (Graphics == NULL) {
        glk_request_timer_events(0);
        return;
    }

    int timer_delay = GREMLINS_ANIMATION_RATE;
    switch (MyLoc) {
    case ROOM_BEDROOM:
        if (Items[ITEM_DART_GREMLIN].Location == ROOM_BEDROOM)
        {
            if (AnimationFlag) {
                DrawImage(60); /* Gremlin throwing dart frame 1 */
            } else {
                DrawImage(59); /* Gremlin throwing dart frame 2 */
            }
        }
        break;
    case ROOM_BAR:
        if (Items[ITEM_GREMLIN_GANG].Location == ROOM_BAR)
        {
            if (AnimationFlag) {
                DrawImage(49); /* Gremlin hanging from curtains frame 1 */
                DrawImage(51); /* Gremlin ear frame 1 */
                DrawImage(54); /* Gremlin's mouth frame 1 */
            } else {
                DrawImage(50); /* Gremlin hanging from curtains frame 2 */
                DrawImage(52); /* Gremlin ear frame 2 */
                DrawImage(53); /* Gremlin's mouth frame 2 */
            }
        }
        break;
    case ROOM_BEHIND_BAR:
        if (Items[ITEM_GREMLIN_GANG].Location == ROOM_BEHIND_BAR)
        {
            if (AnimationFlag) {
                DrawImage(57); /* Flasher gremlin frame 1 */
                DrawImage(24); /* Gremlin tongue frame 1 */
                if (CurrentGame == GREMLINS_GERMAN)
                    DrawImage(46); /* Gremlin ear frame 1 */
                else
                    DrawImage(73); /* Gremlin ear frame 1 */
            } else {
                DrawImage(58); /* Flasher gremlin frame 2 */

                if (CurrentGame == GREMLINS_GERMAN) {
                    DrawImage(33); /* Gremlin tongue frame 2 */
                    DrawImage(23); /* Gremlin ear frame 2 */
                } else {
                    DrawImage(72); /* Gremlin tongue frame 2 */
                    if (CurrentGame == GREMLINS_SPANISH)
                        DrawImage(23); /* Gremlin ear frame 2 */
                    else
                        DrawImage(74); /* Gremlin ear frame 2 */
                }
            }
        }
        break;
    case ROOM_SQUARE:
        if (Items[ITEM_GREMLIN_GANG].Location == ROOM_SQUARE)
        {
            if (AnimationFlag) {
                DrawImage(55); /* Silhouette of Gremlins frame 1 */
            } else {
                DrawImage(71); /* Silhouette of Gremlins frame 1 */
            }
        }
        break;
    case ROOM_ON_A_ROAD:
        if (Items[ITEM_GREMLIN_GANG].Location == ROOM_ON_A_ROAD)
        {
            if (AnimationFlag) {
                if ((Game->subtype & (LOCALIZED | C64)) == LOCALIZED) {
                    DrawImage(25); /* Silhouette 2 of Gremlins  */
                } else {
                    DrawImage(75); /* Silhouette 2 of Gremlins  */
                }
            } else {
                DrawImage(48); /* Silhouette 2 of Gremlins flipped */
            }
        }
        break;
    case ROOM_KITCHEN:
        if (Counters[2] == 2) /* Blender is on */
        {
            if (AnimationFlag) {
                blender_frame = 56; /* Blended Gremlin */
            } else {
                blender_frame = 12; /* Blended Gremlin flipped */
            }
            DrawImage(blender_frame);
        }
        break;
    default:
        timer_delay = 0;
        break;
    }
    AnimationFlag = (AnimationFlag == 0);
    glk_request_timer_events(timer_delay);
}

void GremlinsLook(void)
{
    if (Rooms[MyLoc].Image != 255) {
        if (MyLoc == ROOM_BAR && Items[ITEM_GREMLIN_GANG].Location == ROOM_BAR)
            DrawImage(45); /* Bar full of Gremlins */
        else
            DrawImage(Rooms[MyLoc].Image);
        AnimationFlag = 0;
        UpdateGremlinsAnimations();
    }
    /* Ladder image at the top of the department store */
    if (MyLoc == ROOM_STAIRS && Items[ITEM_LADDER].Location == MyLoc) {
        DrawImage(42);
    } else if (MyLoc == ROOM_POOL && Items[ITEM_WATER].Location == 0) {
        if (Items[ITEM_DAZED_STRIPE].Location == MyLoc && CurrentGame == GREMLINS_GERMAN_C64)
            DrawImage(90); /* Dazed Stripe */
        DrawImage(82); /* Empty pool with puddle */
        /* Draw puddle on top of Stripe */
        /* Doesn't look great, but better than the other way round */
    }
}

static void FillInGermanSystemMessages(void)
{
    sys[I_DONT_KNOW_HOW_TO] = "Ich weiss nicht, wie man etwas \"";
    sys[SOMETHING] = "\" macht. ";
    sys[I_DONT_KNOW_WHAT_A] = "\"";
    sys[IS] = "\" kenne ich nicht. ";
    sys[YES] = "Ja";
    sys[NO] = "Nein";
    sys[ANSWER_YES_OR_NO] = "Antworte Ja oder Nein.\n";
    sys[I_DONT_UNDERSTAND] = "Ich verstehe nicht. ";
    sys[ARE_YOU_SURE] = "Sind Sie sicher? ";
    sys[NOTHING_HERE_TO_TAKE] = "Hier gibt es nichts zu nehmen. ";
    sys[YOU_HAVE_NOTHING] = "Ich trage nichts. ";
    sys[MOVE_UNDONE] = "Verschieben rueckgaengig gemacht. ";
    sys[CANT_UNDO_ON_FIRST_TURN] = "Sie koennen die erste Runde nicht rueckgaengig machen. ";
    sys[NO_UNDO_STATES] = "Keine rueckgaengig-Zustaende mehr gespeichert. ";
    sys[SAVED] = "Spiel gespeichert. ";
    sys[CANT_USE_ALL] = "Sie koennen ALLES nicht mit diesem Verb verwenden. ";
    sys[TRANSCRIPT_ON] = "Das Transkript ist jetzt eingeschaltet. ";
    sys[TRANSCRIPT_OFF] = "Das Transkript ist jetzt deaktiviert. ";
    sys[NO_TRANSCRIPT] = "Es wird kein Transkript ausgefuehrt. ";
    sys[TRANSCRIPT_ALREADY] = "Ein Transkript laeuft bereits. ";
    sys[FAILED_TRANSCRIPT] = "Transkriptdatei konnte nicht erstellt werden. ";
    sys[TRANSCRIPT_START] = "Beginn eines Transkripts.\n\n";
    sys[TRANSCRIPT_END] = "\n\nEnde eines Transkripts.\n";
    sys[BAD_DATA] = "SCHLECHTE DATEN! Ungueltige Speicherdatei.\n";
    sys[STATE_SAVED] = "Zustand speichern.\n";
    sys[NO_SAVED_STATE] = "Es ist kein gespeicherter Zustand vorhanden.\n";
    sys[STATE_RESTORED] = "Zustand wiederhergestellt.\n";

    sys[YOU_ARE] = "Ich bin ";
    /* The tape has "Ausgaenge:", without the space that the disk has */
    sys[EXITS] = "Ausgaenge: ";

    /* The empty pool was left in Spanish, "Piscina vacia" */
    Items[ITEM_EMPTY_POOL].Text = "Leeres Schwimmbad";
    sys[WHAT] = sys[HUH];

    SetParserWordLists(GermanDirections, GermanSkipList, GermanDelimiterList,
        GermanExtraCommands, GermanExtraNouns);
}

void LoadExtraGermanGremlinsC64Data(void)
{
    Verbs[0] = "AUTO";
    Nouns[0] = "ANY";
    Nouns[28] = "*Y.M.C";

    // These are broken in some versions
    Actions[0].Condition[0] = 1005;
    Actions[6].Vocab = 100;

    GameHeader.NumActions = 236;

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
        RESUME_A_SAVED_GAME,
        PLAY_AGAIN,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING
    };

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[HIT_ENTER] = system_messages[30];

    FillInGermanSystemMessages();

    Items[ITEM_DAZED_STRIPE].Image = 255;
}

void LoadExtraGermanGremlinsData(void)
{
    Verbs[0] = "AUTO";
    Nouns[0] = "ANY";
    Nouns[28] = "*Y.M.C";

    Messages[90] = "Ehe ich etwas anderes mache, muss ich erst alles andere fallenlassen. ";
    FillInGermanSystemMessages();
}

static void LoadCommonSpanishGremlinsData(void)
{
    sys[WHAT] = sys[HUH];
    sys[YES] = "s}";
    sys[NO] = "no";
    sys[ANSWER_YES_OR_NO] = "Contesta s} o no.\n";
    sys[I_DONT_KNOW_WHAT_A] = "No s\x84 qu\x84 es un \"";
    sys[IS] = "\". ";
    sys[I_DONT_KNOW_HOW_TO] = "No s\x84 c|mo \"";
    sys[SOMETHING] = "\" algo. ";
    sys[ARE_YOU_SURE] = "\x83\x45stas segura? ";
    sys[NOTHING_HERE_TO_TAKE] = "No hay nada aqu} para tomar. ";
    sys[YOU_HAVE_NOTHING] = "No llevo nada. ";
    sys[MOVE_UNDONE] = "Deshacer. ";
    sys[CANT_UNDO_ON_FIRST_TURN] = "No se puede deshacer en el primer turno. ";
    sys[NO_UNDO_STATES] = "No hay m{s estados de deshacer disponibles. ";
    sys[SAVED] = "Juego guardado. ";
    sys[CANT_USE_ALL] = "No puedes usar TODO con este verbo. ";
    sys[TRANSCRIPT_ON] = "Transcripci|n en. ";
    sys[TRANSCRIPT_OFF] = "Transcripci|n desactivada. ";
    sys[NO_TRANSCRIPT] = "No se est{ ejecutando ninguna transcripci|n. ";
    sys[TRANSCRIPT_ALREADY] = "Ya se est{ ejecutando una transcripci|n. ";
    sys[FAILED_TRANSCRIPT] = "No se pudo crear el archivo de transcripci|n. ";
    sys[TRANSCRIPT_START] = "Comienzo de una transcripci|n.\n\n";
    sys[TRANSCRIPT_END] = "\n\nFin de una transcripci|n.\n";
    sys[BAD_DATA] = "\x80MALOS DATOS! Guardar archivo no v{lido.\n";
    sys[STATE_SAVED] = "Estado guardado.\n";
    sys[NO_SAVED_STATE] = "No existe ning\x85n estado guardado.\n";
    sys[STATE_RESTORED] = "Estado restaurado.\n";

    SetParserWordLists(SpanishDirections, NULL, NULL,
        SpanishExtraCommands, SpanishExtraNouns);
}

/* The ZX Spectrum system message layout of Gremlins, also used by the
   Spectrum Supergran and Savage Island. The Spanish Gremlins has two
   messages fewer before "I'm in a", so the rest are shifted by shift
   (0 there, 2 elsewhere). */
void MapGremlinsSysMessages(int shift)
{
    MapSysRange(DROPPED, OK, 2);
    MapSysRange(I_DONT_UNDERSTAND, THATS_BEYOND_MY_POWER, 6);
    MapSysRange(YOU_ARE, HIT_ENTER, 15 + shift);
    sys[PLAY_AGAIN] = system_messages[5];
    sys[YOURE_CARRYING_TOO_MUCH] = system_messages[22 + shift];
    sys[IM_DEAD] = system_messages[23 + shift];
    sys[YOU_CANT_GO_THAT_WAY] = system_messages[14];
}

void LoadExtraSpanishGremlinsData(void)
{
    MapGremlinsSysMessages(0);

    LoadCommonSpanishGremlinsData();
}

void LoadExtraSpanishGremlinsC64Data(void)
{
    SysMessageType messagekey[] = {
        EXITS,
        YOU_SEE,
        YOU_ARE,
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
        NOTHING,
        BAD_DATA,
    };

    MAP_SYS_MESSAGES(messagekey, 6);
    sys[YOU_CANT_GO_THAT_WAY] = system_messages[9] + 6;
    sys[HIT_ENTER] = system_messages[30];
    /* The disk has "Estoy en", without the space that the tape has */
    sys[YOU_ARE] = "Estoy en ";
    /* The header has the action count of the English disk. What follows
       action 236 here is the dictionary. */
    GameHeader.NumActions = 236;
    LoadCommonSpanishGremlinsData();
}

void GremlinsAction(void)
{
    DrawImage(68); /* Mogwai */
    showing_closeup = 1;
    Display(Bottom, "\n%s\n", sys[HIT_ENTER]);
    HitEnter();
    Look();
}
