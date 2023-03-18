//
//  game_specific.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-27.
//

#include <string.h>

#include "apple2draw.h"
#include "game_specific.h"
#include "saga.h"
#include "sagadraw.h"
#include "scott.h"

void UpdateSecretAnimations(void)
{
    if (MyLoc != 13 || AnimationFlag == 0) {
        glk_request_timer_events(0);
        AnimationFlag = 0;
        return;
    }
    /* Roll down */
    if (AnimationFlag > 0 && AnimationFlag < 48) {
        RectFill(113, 0, 102, AnimationFlag, white_colour);
        AnimationFlag++;
    }
    /* Flicker */
    if (AnimationFlag >= 48 && AnimationFlag < 100) {
        if (Items[21].Location == 0) {
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
            glk_window_close(Top, NULL);
            Top = NULL;
            MyLoc = 1;
            Rooms[1].Image = 25;
            DrawImage(25);
            Delay(2);
            DrawImage(26);
            Rooms[1].Image = 26;
        }
        break;
    case 2: /* ID picture */
        DrawImage(24);
        if (Items[1].Location == CARRIED || Items[1].Location == MyLoc) {
            DrawImage(27);
        } else if ((Items[7].Location == CARRIED || Items[7].Location == MyLoc) && Items[8].Location != CARRIED && Items[8].Location != MyLoc && Items[41].Location != CARRIED && Items[41].Location != MyLoc) {
            DrawImage(28);
        }
        showing_closeup = 1;
        Output(sys[HIT_ENTER]);
        HitEnter();
        break;
    case 3: /* Screen rolls down */
    case 4:
        oldimage = Rooms[13].Image;
        Rooms[13].Image = 30;
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
        Rooms[13].Image = oldimage;
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
        BitFlags |= 1 << DARKBIT;
    else
        BitFlags &= ~(1 << DARKBIT);
}

void AdventurelandAction(int p)
{
    int image = 0;
    switch (p) {
    case 1:
        image = 36;
        break;
    case 2:
        image = 34;
        break;
    case 3:
        image = 33;
        break;
    case 4:
        image = 35;
        break;
    default:
        fprintf(stderr, "Adventureland: Unsupported action parameter %d!\n", p);
        return;
    }
    DrawImage(image);
    Output("\n");
    Output(sys[HIT_ENTER]);
    HitEnter();
    return;
}

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

    for (int i = 0; i < 42; i++) {
        sys[messagekey[i]] = system_messages[i];
    }

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

void Adventureland64Sysmess(void)
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

    for (int i = 0; i < 39; i++) {
        sys[messagekey[i]] = system_messages[i];
    }

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

    for (int i = 0; i < 40; i++) {
        sys[messagekey[i]] = system_messages[i];
    }

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

void Mysterious64Sysmess(void)
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
        YOUVE_SOLVED_IT,
        YOUVE_SOLVED_IT
    };

    for (int i = 0; i < 40; i++) {
        sys[messagekey[i]] = system_messages[i];
    }

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
        dictword[len] = '\0';
        Nouns[i] = dictword;
    }

    Nouns[0] = "ANY\0";

    switch (CurrentGame) {
    case BATON_C64:
        Nouns[79] = "CAST\0";
        Verbs[79] = ".\0";
        GameHeader.NumWords = 79;
        break;
    case TIME_MACHINE_C64:
        Verbs[86] = ".\0";
        break;
    case ARROW1_C64:
        Nouns[82] = ".\0";
        break;
    case ARROW2_C64:
        Verbs[80] = ".\0";
        break;
    case PULSAR7_C64:
        Nouns[102] = ".\0";
        break;
    case CIRCUS_C64:
        Nouns[96] = ".\0";
        break;
    case FEASIBILITY_C64:
        Nouns[80] = ".\0";
        break;
    case PERSEUS_C64:
        Nouns[82] = ".\0";
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

    for (int i = 0; i < 30; i++) {
        sys[messagekey[i]] = system_messages[i];
    }

    sys[I_DONT_KNOW_WHAT_A] = "\"";
    sys[IS] = "\" is a word I don't know. ";
}

void SecretMission64Sysmess(void)
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

    for (int i = 0; i < 39; i++) {
        sys[messagekey[i]] = system_messages[i];
    }

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
}

void VoodooShowImageOnExamineUS(int noun)
{
    int image = -1;
    switch (noun) {
    case 8: // Count Cristo
        if (Items[27].Location == MyLoc)
            image = 11;
        break;
    case 13: // Broken sword
        if (Items[33].Location == MyLoc || Items[33].Location == CARRIED)
            image = 0;
        break;
    case 55: // Voodoo doll
        if (Items[44].Location == MyLoc || Items[44].Location == CARRIED)
            image = 1;
        break;
    case 63: // Voodoo book
        if (Items[52].Location == MyLoc || Items[52].Location == CARRIED)
            image = 2;
        break;
    case 32: // Ring
        if (Items[25].Location == MyLoc || Items[25].Location == CARRIED)
            image = 3;
        break;
    case 43: // Ju-ju man statue
        if (Items[53].Location == MyLoc || Items[53].Location == CARRIED)
            image = 4;
        break;
    case 9: // Glowing idol
        if (Items[9].Location == MyLoc || Items[9].Location == CARRIED)
            image = 5;
        if (Items[43].Location == MyLoc || Items[43].Location == CARRIED)
            image = 10;
        break;
    case 42: // Chemicals
        if (Items[38].Location == MyLoc || Items[38].Location == CARRIED)
            image = 6;
        break;
    case 7: // Bloody knife
        if (Items[0].Location == MyLoc || Items[0].Location == CARRIED)
            image = 7;

    default:
        break;
    }

    if (image >= 0) {
        if (Graphics)
            glk_window_clear(Graphics);
        if (DrawUSRoom(90 + image)) {
            showing_closeup = 1;
            if (CurrentSys == SYS_APPLE2)
                DrawApple2ImageFromVideoMem();
            Output(sys[HIT_ENTER]);
            HitEnter();
        }
    }
}

void CountShowImageOnExamineUS(int noun)
{
    int image = -1;
    switch (noun) {
    case 21: // Package
        if (Items[45].Location == MyLoc || Items[45].Location == CARRIED)
            image = 0;
        break;
    case 50: // Crowd
        if (Items[50].Location == MyLoc)
            image = 1;
        break;
    default:
        break;
    }

    if (image >= 0) {
        if (Graphics)
            glk_window_clear(Graphics);
        if (DrawUSRoom(90 + image)) {
            showing_closeup = 1;
            if (CurrentSys == SYS_APPLE2)
                DrawApple2ImageFromVideoMem();
            Output(sys[HIT_ENTER]);
            HitEnter();
        }
    }
}
