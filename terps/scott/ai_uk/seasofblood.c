//
//  seasofblood.c
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-08.
//

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "sagadraw.h"
#include "scott.h"
#include "seasofblood.h"

#include "decompresstext.h"

winid_t LeftDiceWin, RightDiceWin, BattleRight;
glui32 background_colour;

extern const char *battle_messages[33];
extern uint8_t enemy_table[126];
uint8_t *blood_image_data;
extern uint8_t buffer[384][9];

#define VICTORY 0
#define LOSS 1
#define DRAW 2
#define FLEE 3
#define ERROR 99

glui32 dice_pixel_size, dice_x_offset, dice_y_offset;

int get_enemy_stats(int *strike, int *stamina, int *boatflag);
void battle_loop(int strike, int stamina, int boatflag);
void swap_stamina_and_crew_strength(void);
void blood_battle(void);

void AdventureSheet(void)
{
    glk_stream_set_current(glk_window_get_stream(Bottom));
    glk_set_style(style_Preformatted);
    Output("\nADVENTURE SHEET\n\n");
    Output("SKILL      :");
    OutputNumber(9);
    Output("      STAMINA      :");
    OutputNumber(Counters[3]);
    Output("\nLOG        :");
    OutputNumber(Counters[6]);
    if (Counters[6] < 10)
        Output("      PROVISIONS   :");
    else
        Output("     PROVISIONS   :"); // one less space!
    OutputNumber(Counters[5]);
    Output("\nCREW STRIKE:");
    OutputNumber(9);
    Output("      CREW STRENGTH:");
    OutputNumber(Counters[7]);
    Output("\n\n * * * * * * * * * * * * * * * * * * * * *\n\n");
    ListInventory(0);
    Output("\n");
    glk_set_style(style_Normal);
}

void BloodAction(int p)
{
    switch (p) {
    case 0:
        break;
    case 1:
        // Update LOG
        Counters[6]++;
        break;
    case 2:
        // Battle
        Look();
        Output("You are attacked \n");
        Output("<HIT ENTER> \n");
        HitEnter();
        blood_battle();
        break;
    default:
        fprintf(stderr, "Unhandled special action %d!\n", p);
        break;
    }
}

#pragma mark Image drawing

void mirror_left_half(void)
{
    for (int line = 0; line < 12; line++) {
        for (int col = 32; col > 16; col--) {
            buffer[line * 32 + col - 1][8] = buffer[line * 32 + (32 - col)][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                buffer[line * 32 + col - 1][pixrow] = buffer[line * 32 + (32 - col)][pixrow];
            Flip(buffer[line * 32 + col - 1]);
        }
    }
}

void replace_colour(uint8_t before, uint8_t after)
{

    // I don't think any of the data has bit 7 set,
    // so masking it is probably unnecessary, but this is what
    // the original code does.
    uint8_t beforeink = before & 7;
    uint8_t afterink = after & 7;
    uint8_t inkmask = 0x07;

    uint8_t beforepaper = beforeink << 3;
    uint8_t afterpaper = afterink << 3;
    uint8_t papermask = 0x38;

    for (int j = 0; j < 384; j++) {
        if ((buffer[j][8] & inkmask) == beforeink) {
            buffer[j][8] = (buffer[j][8] & ~inkmask) | afterink;
        }

        if ((buffer[j][8] & papermask) == beforepaper) {
            buffer[j][8] = (buffer[j][8] & ~papermask) | afterpaper;
        }
    }
}

void draw_colour(uint8_t x, uint8_t y, uint8_t colour, uint8_t length)
{
    for (int i = 0; i < length; i++) {
        buffer[y * 32 + x + i][8] = colour;
    }
}

void make_light(void)
{
    for (int i = 0; i < 384; i++) {
        buffer[i][8] = buffer[i][8] | 0x40;
    }
}

void flip_image(void)
{

    uint8_t mirror[384][9];

    for (int line = 0; line < 12; line++) {
        for (int col = 32; col > 0; col--) {
            for (int pixrow = 0; pixrow < 9; pixrow++)
                mirror[line * 32 + col - 1][pixrow] = buffer[line * 32 + (32 - col)][pixrow];
            Flip(mirror[line * 32 + col - 1]);
        }
    }

    memcpy(buffer, mirror, 384 * 9);
}

int should_draw_object_images;

void draw_object_image(uint8_t x, uint8_t y)
{
    for (int i = 0; i < GameHeader.NumItems; i++) {
        if (Items[i].Flag != MyLoc)
            continue;
        if (Items[i].Location != MyLoc)
            continue;
        DrawSagaPictureAtPos(Items[i].Image, x, y);
        should_draw_object_images = 0;
    }
}

void draw_blood(int loc)
{
    memset(buffer, 0, 384 * 9);
    uint8_t *ptr = blood_image_data;
    for (int i = 0; i < loc; i++) {
        while (*(ptr) != 0xff)
            ptr++;
        ptr++;
    }
    while (ptr < blood_image_data + 2010) {
        switch (*ptr) {
        case 0xff:
            if (loc == 13) {
                buffer[8 * 32 + 18][8] = buffer[8 * 32 + 18][8] & ~0x40;
                buffer[8 * 32 + 17][8] = buffer[8 * 32 + 17][8] & ~0x40;

                buffer[8 * 32 + 9][8] = buffer[8 * 32 + 9][8] & ~0x40;
                buffer[8 * 32 + 10][8] = buffer[8 * 32 + 10][8] & ~0x40;
            }
            return;
        case 0xfe:
            mirror_left_half();
            break;
        case 0xfD:
            replace_colour(*(ptr + 1), *(ptr + 2));
            ptr += 2;
            break;
        case 0xfc: // Draw colour: x, y, attribute, length
            draw_colour(*(ptr + 1), *(ptr + 2), *(ptr + 3), *(ptr + 4));
            ptr = ptr + 4;
            break;
        case 0xfb: // Make all screen colours bright
            make_light();
            break;
        case 0xfa: // Flip entire image horizontally
            flip_image();
            break;
        case 0xf9: // Draw object image (if present) at x, y
            draw_object_image(*(ptr + 1), *(ptr + 2));
            ptr = ptr + 2;
            break;
        default: // else draw image *ptr at x, y
            DrawSagaPictureAtPos(*ptr, *(ptr + 1), *(ptr + 2));
            ptr = ptr + 2;
        }
        ptr++;
    }
}

void SeasOfBloodRoomImage(void)
{
    should_draw_object_images = 1;
    draw_blood(MyLoc);
    for (int ct = 0; ct <= GameHeader.NumItems; ct++)
        if (Items[ct].Image && should_draw_object_images) {
            if ((Items[ct].Flag & 127) == MyLoc && Items[ct].Location == MyLoc) {
                DrawImage(Items[ct].Image);
            }
        }
    DrawSagaPictureFromBuffer();
}

#pragma mark Battles

static void SOBPrint(winid_t w, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;

static void SOBPrint(winid_t w, const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glk_put_string_stream(glk_window_get_stream(w), msg);
}

static glui32 optimal_dice_pixel_size(glui32 *width, glui32 *height)
{
    int ideal_width = 8;
    int ideal_height = 8;

    *width = ideal_width;
    *height = ideal_height;
    int multiplier = 1;
    glui32 graphwidth, graphheight;
    glk_window_get_size(LeftDiceWin, &graphwidth, &graphheight);
    multiplier = graphheight / ideal_height;
    if ((glui32)(ideal_width * multiplier) > graphwidth)
        multiplier = graphwidth / ideal_width;

    if (multiplier < 2)
        multiplier = 2;

    multiplier = multiplier / 2;

    *width = ideal_width * multiplier;
    *height = ideal_height * multiplier;

    return multiplier;
}

static void draw_border(winid_t win)
{
    glui32 width, height;
    glk_stream_set_current(glk_window_get_stream(win));
    glk_window_get_size(win, &width, &height);
    height--;
    width -= 2;
    glk_window_move_cursor(win, 0, 0);
    glk_put_char_uni(0x250F); // Top left corner
    for (glui32 i = 1; i < width; i++)
        glk_put_char_uni(0x2501); // Top
    glk_put_char_uni(0x2513); // Top right corner
    for (glui32 i = 1; i < height; i++) {
        glk_window_move_cursor(win, 0, i);
        glk_put_char_uni(0x2503);
        glk_window_move_cursor(win, width, i);
        glk_put_char_uni(0x2503);
    }
    glk_window_move_cursor(win, 0, height);
    glk_put_char_uni(0x2517);
    for (glui32 i = 1; i < width; i++)
        glk_put_char_uni(0x2501);
    glk_put_char_uni(0x251B);
}

static void redraw_static_text(winid_t win, int boatflag)
{
    glk_stream_set_current(glk_window_get_stream(win));
    glk_window_move_cursor(win, 2, 4);

    if (boatflag) {
        glk_put_string("STRIKE  :\n");
        glk_window_move_cursor(win, 2, 5);
        glk_put_string("CRW STR :");
    } else {
        glk_put_string("SKILL   :\n");
        glk_window_move_cursor(win, 2, 5);
        glk_put_string("STAMINA :");
    }

    if (win == BattleRight) {
        glui32 width;
        glk_window_get_size(BattleRight, &width, 0);
        glk_window_move_cursor(BattleRight, width - 6, 1);
        glk_put_string("YOU");
    }
}

static void redraw_battle_screen(int boatflag)
{
    glui32 graphwidth, graphheight, optimal_width, optimal_height;

    glk_window_get_size(LeftDiceWin, &graphwidth, &graphheight);

    dice_pixel_size = optimal_dice_pixel_size(&optimal_width, &optimal_height);
    dice_x_offset = (graphwidth - optimal_width) / 2;
    dice_y_offset = (graphheight - optimal_height - dice_pixel_size) / 2;

    draw_border(Top);
    draw_border(BattleRight);

    redraw_static_text(Top, boatflag);
    redraw_static_text(BattleRight, boatflag);
}

static void setup_battle_screen(int boatflag)
{
    winid_t parent = glk_window_get_parent(Top);
    glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed, 7, Top);

    glk_window_clear(Top);
    glk_window_clear(Bottom);
    BattleRight = glk_window_open(Top, winmethod_Right | winmethod_Proportional,
        50, wintype_TextGrid, 0);

    LeftDiceWin = glk_window_open(Top, winmethod_Right | winmethod_Proportional,
        30, wintype_Graphics, 0);
    RightDiceWin = glk_window_open(BattleRight, winmethod_Left | winmethod_Proportional, 30,
        wintype_Graphics, 0);

    /* Set the graphics window background to match
     * the top window background, best as we can,
     * and clear the window.
     */
    if (glk_style_measure(Top, style_Normal, stylehint_BackColor,
            &background_colour)) {
        glk_window_set_background_color(LeftDiceWin, background_colour);
        glk_window_set_background_color(RightDiceWin, background_colour);

        glk_window_clear(LeftDiceWin);
        glk_window_clear(RightDiceWin);
    }

    if (palchosen == C64B)
        dice_colour = 0x5f48e9;
    else
        dice_colour = 0xff0000;

    redraw_battle_screen(boatflag);
}

void blood_battle(void)
{
    int enemy, strike, stamina, boatflag;
    enemy = get_enemy_stats(&strike, &stamina, &boatflag); // Determine their stats
    if (enemy == 0) {
        fprintf(stderr, "Seas of blood battle: No enemy in location?\n");
        return;
    }
    setup_battle_screen(boatflag);
    battle_loop(strike, stamina, boatflag); // Into the battle loops
    if (boatflag)
        swap_stamina_and_crew_strength(); // Switch back stamina - crew strength
    glk_window_close(LeftDiceWin, NULL);
    glk_window_close(RightDiceWin, NULL);
    glk_window_close(BattleRight, NULL);
    CloseGraphicsWindow();
    OpenGraphicsWindow();
    SeasOfBloodRoomImage();
}

int get_enemy_stats(int *strike, int *stamina, int *boatflag)
{
    int enemy, i = 0;
    while (i < 124) {
        enemy = enemy_table[i];
        if (Items[enemy].Location == MyLoc) {
            i++;
            *strike = enemy_table[i++];
            *stamina = enemy_table[i++];
            *boatflag = enemy_table[i];
            if (*boatflag) {
                swap_stamina_and_crew_strength(); // Switch stamina - crew strength
            }

            return enemy;
        }
        i = i + 4; // Skip to next entry
    }
    return 0;
}

void draw_rect(winid_t win, int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color)
{
    glk_window_fill_rect(win, color, x * dice_pixel_size + dice_x_offset,
        y * dice_pixel_size + dice_y_offset,
        width * dice_pixel_size, height * dice_pixel_size);
}

void draw_graphical_dice(winid_t win, int number)
{
    // The eye-less dice backgrounds consist of two rectangles on top of each
    // other
    draw_rect(win, 1, 2, 7, 5, dice_colour);
    draw_rect(win, 2, 1, 5, 7, dice_colour);

    switch (number + 1) {
    case 1:
        draw_rect(win, 4, 4, 1, 1, background_colour);
        break;
    case 2:
        draw_rect(win, 2, 6, 1, 1, background_colour);
        draw_rect(win, 6, 2, 1, 1, background_colour);
        break;
    case 3:
        draw_rect(win, 2, 6, 1, 1, background_colour);
        draw_rect(win, 4, 4, 1, 1, background_colour);
        draw_rect(win, 6, 2, 1, 1, background_colour);
        break;
    case 4:
        draw_rect(win, 2, 6, 1, 1, background_colour);
        draw_rect(win, 6, 2, 1, 1, background_colour);
        draw_rect(win, 2, 2, 1, 1, background_colour);
        draw_rect(win, 6, 6, 1, 1, background_colour);
        break;
    case 5:
        draw_rect(win, 2, 6, 1, 1, background_colour);
        draw_rect(win, 6, 2, 1, 1, background_colour);
        draw_rect(win, 2, 2, 1, 1, background_colour);
        draw_rect(win, 6, 6, 1, 1, background_colour);
        draw_rect(win, 4, 4, 1, 1, background_colour);
        break;
    case 6:
        draw_rect(win, 2, 6, 1, 1, background_colour);
        draw_rect(win, 6, 2, 1, 1, background_colour);
        draw_rect(win, 2, 2, 1, 1, background_colour);
        draw_rect(win, 2, 4, 1, 1, background_colour);
        draw_rect(win, 6, 4, 1, 1, background_colour);
        draw_rect(win, 6, 6, 1, 1, background_colour);
        break;
    default:
        break;
    }
}

void update_dice(int our_turn, int left_dice, int right_dice)
{
    left_dice--;
    right_dice--;
    glk_window_clear(LeftDiceWin);
    glk_window_clear(RightDiceWin);

    dice_x_offset = dice_x_offset - dice_pixel_size;
    draw_graphical_dice(LeftDiceWin, left_dice);
    dice_x_offset = dice_x_offset + dice_pixel_size;
    draw_graphical_dice(RightDiceWin, right_dice);

    winid_t win = our_turn ? BattleRight : Top;

    glk_window_move_cursor(win, 2, 1);

    glk_stream_set_current(glk_window_get_stream(win));

    glk_put_char_uni(0x2680 + left_dice);
    glk_put_char('+');
    glk_put_char_uni(0x2680 + right_dice);

    glk_window_move_cursor(win, 2, 2);
    SOBPrint(win, "%d %d", left_dice + 1, right_dice + 1);
}

void print_sum(int our_turn, int sum, int strike)
{
    winid_t win = our_turn ? BattleRight : Top;
    glk_stream_set_current(glk_window_get_stream(win));

    glk_window_move_cursor(win, 6, 1);

    SOBPrint(win, "+ %d = %d", strike, sum);

    glk_stream_set_current(glk_window_get_stream(BattleRight));
    glk_window_move_cursor(BattleRight, 6, 1);
    glk_put_string("+ 9 = ");
}

void update_result(int our_turn, int strike, int stamina, int boatflag)
{
    winid_t win = our_turn ? BattleRight : Top;
    glk_stream_set_current(glk_window_get_stream(win));

    glk_window_move_cursor(win, 2, 4);

    if (boatflag) {
        SOBPrint(win, "STRIKE  : %d\n", strike);
        glk_window_move_cursor(win, 2, 5);
        SOBPrint(win, "CRW STR : %d", stamina);
    } else {
        SOBPrint(win, "SKILL   : %d\n", strike);
        glk_window_move_cursor(win, 2, 5);
        SOBPrint(win, "STAMINA : %d", stamina);
    }
}

void clear_result(void)
{
    winid_t win = Top;

    glui32 width;
    for (int j = 0; j < 2; j++) {
        glk_window_get_size(win, &width, 0);
        glk_stream_set_current(glk_window_get_stream(win));

        glk_window_move_cursor(win, 11, 1);
        for (int i = 0; i < 10; i++)
            glk_put_string(" ");
        draw_border(win);
        win = BattleRight;
    }
}

void clear_stamina(void)
{
    winid_t win = Top;

    glui32 width;
    for (int j = 0; j < 2; j++) {
        glk_window_get_size(win, &width, 0);
        glk_stream_set_current(glk_window_get_stream(win));

        glk_window_move_cursor(win, 11, 5);
        for (int i = 0; i < (int)width - 13; i++)
            glk_put_string(" ");
        draw_border(win);
        win = BattleRight;
    }
}

static void RearrangeBattleDisplay(int strike, int stamina, int boatflag)
{
    UpdateSettings();
    glk_cancel_char_event(Top);
    CloseGraphicsWindow();
    glk_window_close(BattleRight, NULL);
    glk_window_close(LeftDiceWin, NULL);
    glk_window_close(RightDiceWin, NULL);
    SeasOfBloodRoomImage();
    setup_battle_screen(boatflag);
    update_result(0, strike, stamina, boatflag);
    update_result(1, 9, Counters[3], boatflag);
    draw_border(Top);
    draw_border(BattleRight);
    redraw_static_text(Top, boatflag);
    redraw_static_text(BattleRight, boatflag);
    glk_request_char_event(Top);
}

int roll_dice(int strike, int stamina, int boatflag)
{
    clear_result();
    redraw_static_text(BattleRight, boatflag);

    glk_request_timer_events(60);
    int rolls = 0;
    int our_turn = 0;
    int left_dice = 1 + rand() % 6;
    int right_dice = 1 + rand() % 6;
    int our_result;
    int their_result = 0;
    int their_dice_stopped = 0;

    event_t event;
    int enemy_rolls = 20 + rand() % 10;
    glk_cancel_char_event(Top);
    glk_request_char_event(Top);

    glk_stream_set_current(glk_window_get_stream(Bottom));
    glk_put_string("Their throw");

    int delay = 60;

    while (1) {
        glk_select(&event);
        if (event.type == evtype_Timer) {
            if (their_dice_stopped) {
                glk_request_timer_events(60);
                their_dice_stopped = 0;
                print_sum(our_turn, their_result, strike);
                our_turn = 1;
                glk_window_clear(Bottom);
                glk_cancel_char_event(Top);
                glk_request_char_event(Top);
                glk_stream_set_current(glk_window_get_stream(Bottom));
                glk_put_string("Your throw\n");
                glk_put_string("<ENTER> to stop dice");
                if (!boatflag)
                    glk_put_string("    <X> to run");
            } else if (our_turn == 0) {
                glk_request_timer_events(delay);
            }

            rolls++;

            if (rolls & 1)
                left_dice = 1 + rand() % 6;
            else
                right_dice = 1 + rand() % 6;

            update_dice(our_turn, left_dice, right_dice);
            if (our_turn == 0 && rolls == enemy_rolls) {
                glk_window_clear(Bottom);
                their_result = left_dice + right_dice + strike;
                SOBPrint(Bottom, "Their result: %d + %d + %d = %d\n", left_dice,
                    right_dice, strike, their_result);
                glk_request_timer_events(1000);
                their_dice_stopped = 1;
            }
        }

        if (event.type == evtype_CharInput) {
            if (our_turn && (event.val1 == keycode_Return)) {
                update_dice(our_turn, left_dice, right_dice);
                our_result = left_dice + right_dice + 9;
                print_sum(our_turn, our_result, 9);
                if (their_result > our_result) {
                    return LOSS;
                } else if (our_result > their_result) {
                    return VICTORY;
                } else {
                    return DRAW;
                }
            } else if (MyLoc != 1 && (event.val1 == 'X' || event.val1 == 'x')) {
                MyLoc = SavedRoom;
                return FLEE;
            } else {
                glk_request_char_event(Top);
            }
        }
        if (event.type == evtype_Arrange) {
            RearrangeBattleDisplay(strike, stamina, boatflag);
        }
    }
    return ERROR;
}

void BattleHitEnter(int strike, int stamina, int boatflag)
{
    glk_request_char_event(Bottom);
    event_t ev;
    int result = 0;
    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if (ev.val1 == keycode_Return) {
                result = 1;
            } else {
                glk_request_char_event(Bottom);
            }
        }

        if (ev.type == evtype_Arrange) {
            RearrangeBattleDisplay(strike, stamina, boatflag);
        }

    } while (result == 0);
    return;
}

void battle_loop(int strike, int stamina, int boatflag)
{
    update_result(0, strike, stamina, boatflag);
    update_result(1, 9, Counters[3], boatflag);
    do {
        int result = roll_dice(strike, stamina, boatflag);
        glk_cancel_char_event(Top);
        glk_window_clear(Bottom);
        clear_stamina();
        glk_stream_set_current(glk_window_get_stream(Bottom));
        if (result == LOSS) {
            Counters[3] -= 2;

            if (Counters[3] <= 0) {
                SOBPrint(Bottom, "%s\n",
                    boatflag ? "THE BANSHEE HAS BEEN SUNK!"
                             : "YOU HAVE BEEN KILLED!");
                Counters[3] = 0;
                BitFlags |= (1 << 6);
                Counters[7] = 0;
            } else {
                SOBPrint(Bottom, "%s", battle_messages[1 + rand() % 5 + 16 * boatflag]);
            }
        } else if (result == VICTORY) {
            stamina -= 2;
            if (stamina <= 0) {
                glk_put_string("YOU HAVE WON!\n");
                BitFlags &= ~(1 << 6);
                stamina = 0;
            } else {
                SOBPrint(Bottom, "%s", battle_messages[6 + rand() % 5 + 16 * boatflag]);
            }
        } else if (result == FLEE) {
            BitFlags |= (1 << 6);
            MyLoc = SavedRoom;
            return;
        } else {
            SOBPrint(Bottom, "%s", battle_messages[11 + rand() % 5 + 16 * boatflag]);
        }

        glk_put_string("\n\n");

        if (Counters[3] > 0 && stamina > 0) {
            glk_put_string("<ENTER> to roll dice");
            if (!boatflag)
                glk_put_string("    <X> to run");
        }

        update_result(0, strike, stamina, boatflag);
        update_result(1, 9, Counters[3], boatflag);

        BattleHitEnter(strike, stamina, boatflag);
        glk_window_clear(Bottom);

    } while (stamina > 0 && Counters[3] > 0);
}

void swap_stamina_and_crew_strength(void)
{
    uint8_t temp = Counters[7]; // Crew strength
    Counters[7] = Counters[3]; // Stamina
    Counters[3] = temp;
}

extern int draw_to_buffer;

int LoadExtraSeasOfBloodData(void)
{
    draw_to_buffer = 1;

    int offset;

#pragma mark Enemy table

    offset = 0x47b7 + file_baseline_offset;

    uint8_t *ptr = SeekToPos(entire_file, offset);

    int ct;

    for (ct = 0; ct < 124; ct++) {
        enemy_table[ct] = *(ptr++);
        if (enemy_table[ct] == 0xff)
            break;
    }

#pragma mark Battle messages

    ptr = SeekToPos(entire_file, 0x71DA + file_baseline_offset);

    for (int i = 0; i < 32; i++) {
        battle_messages[i] = DecompressText(ptr, i);
    }

#pragma mark Extra image data

    offset = 0x7af5 - 16357 + file_baseline_offset;

    int data_length = 2010;

    blood_image_data = MemAlloc(data_length);
    ptr = SeekToPos(entire_file, offset);
    for (int i = 0; i < data_length; i++)
        blood_image_data[i] = *(ptr++);

#pragma mark System messages

    for (int i = I_DONT_UNDERSTAND; i <= THATS_BEYOND_MY_POWER; i++)
        sys[i] = system_messages[4 - I_DONT_UNDERSTAND + i];

    for (int i = YOU_ARE; i <= HIT_ENTER; i++)
        sys[i] = system_messages[13 - YOU_ARE + i];

    sys[OK] = system_messages[2];
    sys[PLAY_AGAIN] = system_messages[3];
    sys[YOURE_CARRYING_TOO_MUCH] = system_messages[27];

    Items[125].Text = "A loose plank";
    Items[125].AutoGet = "PLAN";

    return 0;
}

int LoadExtraSeasOfBlood64Data(void)
{
    draw_to_buffer = 1;

    int offset;

#pragma mark Enemy table

    offset = 0x3fee + file_baseline_offset;
    uint8_t *ptr;

    ptr = SeekToPos(entire_file, offset);

    int ct;
    for (ct = 0; ct < 124; ct++) {
        enemy_table[ct] = *(ptr++);
        if (enemy_table[ct] == 0xff)
            break;
    }
#pragma mark Battle messages

    offset = 0x82f6 + file_baseline_offset;
    ptr = SeekToPos(entire_file, offset);

    for (int i = 0; i < 32; i++) {
        battle_messages[i] = DecompressText(ptr, i);
    }

#pragma mark Extra image data

    offset = 0x5299 + file_baseline_offset;

    int data_length = 2010;

    blood_image_data = MemAlloc(data_length);

    ptr = SeekToPos(entire_file, offset);
    for (int i = 0; i < data_length; i++) {
        blood_image_data[i] = *(ptr++);
    }

#pragma mark System messages

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
        YOU_HAVENT_GOT_IT,
        DROPPED,
        TAKEN,
        INVENTORY,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DIRECTION,
        YOURE_CARRYING_TOO_MUCH,
        PLAY_AGAIN,
        RESUME_A_SAVED_GAME,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING
    };

    for (int i = 0; i < 27; i++) {
        sys[messagekey[i]] = system_messages[i];
    }

    return 0;
}
