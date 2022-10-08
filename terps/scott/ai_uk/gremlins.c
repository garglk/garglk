//
//  gremlins.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include "gremlins.h"

#include "parser.h"
#include "sagadraw.h"
#include "scott.h"

#define GREMLINS_ANIMATION_RATE 670

void UpdateGremlinsAnimations(void)
{
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
    case 1: /* Bedroom */
        if (Items[50].Location == 1) /* Gremlin throwing darts */
        {
            if (AnimationFlag) {
                DrawImage(60); /* Gremlin throwing dart frame 1 */
            } else {
                DrawImage(59); /* Gremlin throwing dart frame 2 */
            }
        }
        break;
    case 17: /* Dotty's Tavern */
        if (Items[82].Location == 17) /* Gang of GREMLINS */
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
    case 16: /* Behind a Bar */
        if (Items[82].Location == 16) /* Gang of GREMLINS */
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
    case 19: /* Square */
        if (Items[82].Location == 19) /* Gang of GREMLINS */
        {
            if (AnimationFlag) {
                DrawImage(55); /* Silhouette of Gremlins frame 1 */
            } else {
                DrawImage(71); /* Silhouette of Gremlins frame 1 */
            }
        }
        break;
    case 6: /* on a road */
        if (Items[82].Location == 6) /* Gang of GREMLINS */
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
    case 3: /* Kitchen */
        if (Counters[2] == 2) /* Blender is on */
        {
            if (AnimationFlag) {
                DrawImage(56); /* Blended Gremlin */
            } else {
                DrawImage(12); /* Blended Gremlin flipped */
            }
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
        if (MyLoc == 17 && Items[82].Location == 17)
            DrawImage(45); /* Bar full of Gremlins */
        else
            DrawImage(Rooms[MyLoc].Image);
        AnimationFlag = 0;
        UpdateGremlinsAnimations();
    }
    /* Ladder image at the top of the department store */
    if (MyLoc == 34 && Items[53].Location == MyLoc) {
        DrawImage(42);
    } else if (MyLoc == 10 && Items[15].Location == 0) {
        if (Items[99].Location == MyLoc && CurrentGame == GREMLINS_GERMAN_C64)
            DrawImage(90); /* Dazed Stripe */
        DrawImage(82); /* Empty pool with puddle */
        /* Draw puddle on top of Stripe */
        /* Doesn't look great, but better than the other way round */
    }
}

void FillInGermanSystemMessages(void)
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
    sys[YOU_HAVE_NOTHING] = "Ich traege nichts. ";
    sys[MOVE_UNDONE] = "Verschieben rueckgaengig gemacht. ";
    sys[CANT_UNDO_ON_FIRST_TURN] = "Sie koennen die erste Runde nicht rueckgaengig machen. ";
    sys[NO_UNDO_STATES] = "Keine rueckgaengig-Zustaende mehr gespeichert. ";
    sys[SAVED] = "Spiel gespeichert. ";
    sys[CANT_USE_ALL] = "Sie koennen ALLES nicht mit diesem Verb verwenden. ";
    sys[TRANSCRIPT_ON] = "Das Transkript ist jetzt eingeschaltet. ";
    sys[TRANSCRIPT_OFF] = "Das Transkript ist jetzt deaktiviert. ";
    sys[NO_TRANSCRIPT] = "Es wird kein Transkript ausgefuehrt. ";
    sys[TRANSCRIPT_ALREADY] = "Eine Transkript laeuft bereits. ";
    sys[FAILED_TRANSCRIPT] = "Transkriptdatei konnte nicht erstellt werden. ";
    sys[TRANSCRIPT_START] = "Beginn einer Transkript.\n\n";
    sys[TRANSCRIPT_END] = "\n\nEnde eniner Transkript.\n";
    sys[BAD_DATA] = "SCHLECHTE DATEN! Ungueltige Speicherdatei.\n";
    sys[STATE_SAVED] = "Zustand speichern.\n";
    sys[NO_SAVED_STATE] = "Es ist kein gespeicherter Zustand vorhanden.\n";
    sys[STATE_RESTORED] = "Zustand wiederhergestellt.\n";

    sys[YOU_ARE] = "Ich bin ";
    sys[WHAT] = sys[HUH];

    for (int i = 0; i < NUMBER_OF_DIRECTIONS; i++)
        Directions[i] = GermanDirections[i];
    for (int i = 0; i < NUMBER_OF_SKIPPABLE_WORDS; i++)
        SkipList[i] = GermanSkipList[i];
    for (int i = 0; i < NUMBER_OF_DELIMITERS; i++)
        DelimiterList[i] = GermanDelimiterList[i];
    for (int i = 0; i < NUMBER_OF_EXTRA_COMMANDS; i++)
        ExtraCommands[i] = GermanExtraCommands[i];
    for (int i = 0; i < NUMBER_OF_EXTRA_NOUNS; i++)
        ExtraNouns[i] = GermanExtraNouns[i];
}

void LoadExtraGermanGremlinsc64Data(void)
{
    Verbs[0] = "AUTO\0";
    Nouns[0] = "ANY\0";
    Nouns[28] = "*Y.M.C\0";

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

    for (int i = 0; i < 28; i++) {
        sys[messagekey[i]] = system_messages[i];
    }

    sys[HIT_ENTER] = system_messages[30];

    FillInGermanSystemMessages();

    Items[99].Image = 255;
}

void LoadExtraGermanGremlinsData(void)
{
    Verbs[0] = "AUTO\0";
    Nouns[0] = "ANY\0";
    Nouns[28] = "*Y.M.C\0";

    Messages[90] = "Ehe ich etwas anderes mache, much aich erst alles andere fallenlassen. ";
    FillInGermanSystemMessages();
}

void LoadCommonSpanishGremlinsData(void) {
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

    for (int i = 0; i < NUMBER_OF_DIRECTIONS; i++)
        Directions[i] = SpanishDirections[i];
    for (int i = 0; i < NUMBER_OF_EXTRA_NOUNS; i++)
        ExtraNouns[i] = SpanishExtraNouns[i];
    for (int i = 0; i < NUMBER_OF_EXTRA_COMMANDS; i++)
        ExtraCommands[i] = SpanishExtraCommands[i];
}

void LoadExtraSpanishGremlinsData(void) {
    for (int i = YOU_ARE; i <= HIT_ENTER; i++)
        sys[i] = system_messages[15 - YOU_ARE + i];
    for (int i = I_DONT_UNDERSTAND; i <= THATS_BEYOND_MY_POWER; i++)
        sys[i] = system_messages[6 - I_DONT_UNDERSTAND + i];
    for (int i = DROPPED; i <= OK; i++)
        sys[i] = system_messages[2 - DROPPED + i];
    sys[PLAY_AGAIN] = system_messages[5];
    sys[YOURE_CARRYING_TOO_MUCH] = system_messages[22];
    sys[IM_DEAD] = system_messages[23];
    sys[YOU_CANT_GO_THAT_WAY] = system_messages[14];

    LoadCommonSpanishGremlinsData();
}

void LoadExtraSpanishGremlinsC64Data(void) {
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

    for (int i = 0; i < 23; i++) {
        sys[messagekey[i]] = system_messages[6 + i];
    }
    sys[YOU_CANT_GO_THAT_WAY] = system_messages[9] + 6;
    sys[HIT_ENTER] = system_messages[30];
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
