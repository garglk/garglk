//
//  seas_of_blood.c
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Implements game-specific features for "Seas of Blood" (Fighting Fantasy #16),
//  a text adventure adaptation by Adventure Soft UK. This file handles:
//    - The adventure sheet (player stats display)
//    - The battle/combat system with animated dice rolling
//    - Room image rendering with object overlays
//    - Loading game-specific data (enemy tables, battle messages, images)
//
//  The battle system recreates the Fighting Fantasy dice combat: both sides
//  roll 2d6 + strike skill. The loser takes 2 stamina damage. Ship combat
//  ("boat battles") uses crew strength instead of personal stamina.
//
//  Created by Petter Sjölund on 2022-01-08.
//

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "decompress_text.h"
#include "game_specific.h"
#include "irmak.h"
#include "sagadraw.h"
#include "seas_of_blood.h"
#include "scott.h"
#include "detect_game.h"
#include "scott_meta.h"
#include "scott_actions.h"
#include "scott_display.h"
#include "taylordraw.h"
#include "randomness.h"

// Glk windows for the split-screen battle display:
// Two graphics windows for animated dice, plus a text grid for the player's stats.
static winid_t LeftDiceWin, RightDiceWin, BattleRight;
static glui32 background_colour;
// RGB colour of the dice, set by setup_battle_screen()
static glui32 dice_colour;

#define NUM_BATTLE_MESSAGES 33
#define ENEMY_TABLE_SIZE 126

// Combat messages
static const char *battle_messages[NUM_BATTLE_MESSAGES];
// Enemy lookup table: 4-byte entries (item index, strike, stamina, boatflag),
// terminated by 0xFF. Loaded from game data in LoadExtraSeasOfBloodData().
static uint8_t enemy_table[ENEMY_TABLE_SIZE];
uint8_t *blood_image_data;

// Battle outcome constants
#define VICTORY 0
#define LOSS 1
#define DRAW 2
#define FLEE 3
#define BATTLE_ERROR 99

// Dice rendering parameters, computed from the available window size
static glui32 dice_pixel_size, dice_x_offset, dice_y_offset;

static int get_enemy_stats(int *strike, int *stamina, int *boatflag);
static void battle_loop(int strike, int stamina, int boatflag);
static void swap_stamina_and_crew_strength(void);
static void blood_battle(void);

// Displays the player's character sheet, mirroring the Fighting Fantasy
// adventure sheet format. Player skill and crew strike are fixed at 9.
// Game counters: [3]=stamina, [5]=provisions, [6]=log, [7]=crew strength.
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
        Output("     PROVISIONS   :"); // one less space for 2-digit numbers
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

// Handles Seas of Blood-specific action codes triggered by the game engine.
//   0 = no-op
//   1 = increment the log (tracking voyage progress)
//   2 = initiate combat with an enemy at the current location
void BloodAction(int p)
{
    switch (p) {
    case 0:
        break;
    case 1:
        Counters[6]++;
        break;
    case 2:
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

// MARK: Image drawing

// Flag to track whether object images still need to be drawn on top of
// the room image. Set to 1 before drawing the room, cleared after objects
// are drawn. This allows the room drawing code to optionally call
// DrawObjectImages itself at a specific position.
int should_draw_object_images;

// Draws images for all items present in the current room, overlaid on the
// room image. If x is -1, each item is drawn at its default position from
// the image table; otherwise all items are drawn at the specified (x, y).
static void DrawObjectImages(uint8_t x, uint8_t y)
{
    int draw_at_default_pos = ((int)x == -1);
    for (int i = 0; i < GameHeader.NumItems; i++) {
        if ((Items[i].Flag & 127) == MyLoc && Items[i].Location == MyLoc) {
            if (draw_at_default_pos) {
                x = images[Items[i].Image].xoff;
                y = images[Items[i].Image].yoff;
            }
            DrawPictureAtPos(Items[i].Image, x, y, 1);
        }
    }
    should_draw_object_images = 0;
}

/* Remove ugly bright tiles behind sarcophagus
 (only visible with ZX Spectrum palette) */
static void PatchCryptImage(void) {
    imagebuffer[8 * IRMAK_IMGWIDTH + 18][IRMAK_CELL_ATTR] = imagebuffer[8 * IRMAK_IMGWIDTH + 18][IRMAK_CELL_ATTR] & ~BRIGHT_FLAG;
    imagebuffer[8 * IRMAK_IMGWIDTH + 17][IRMAK_CELL_ATTR] = imagebuffer[8 * IRMAK_IMGWIDTH + 17][IRMAK_CELL_ATTR] & ~BRIGHT_FLAG;
    imagebuffer[8 * IRMAK_IMGWIDTH + 9][IRMAK_CELL_ATTR] = imagebuffer[8 * IRMAK_IMGWIDTH + 9][IRMAK_CELL_ATTR] & ~BRIGHT_FLAG;
    imagebuffer[8 * IRMAK_IMGWIDTH + 10][IRMAK_CELL_ATTR] = imagebuffer[8 * IRMAK_IMGWIDTH + 10][IRMAK_CELL_ATTR] & ~BRIGHT_FLAG;
}

// Renders the room image for the current location. The room background is
// drawn using the DrawTaylor() function (shared with the TaylorMade engine),
// then any item images are overlaid. Finally the completed image buffer is
// drawn using DrawIrmakPictureFromBuffer().
void SeasOfBloodRoomImage(void)
{
    OpenGraphicsWindow();
    should_draw_object_images = 1;
    ClearGraphMem();
    DrawTaylor(MyLoc, MyLoc);

    if (should_draw_object_images)
        DrawObjectImages(-1,-1);

    if (MyLoc == 13)
        PatchCryptImage();

    if (Graphics != NULL)
        DrawIrmakPictureFromBuffer();
}

// MARK: Battles

// Printf-style helper that outputs formatted text to a specific Glk window.
// Used throughout the battle system to write to different panes.
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

// Calculates the best pixel multiplier for rendering dice in the available
// graphics window space. Each die face is drawn on an 8x8 grid; the
// multiplier scales this up to fill the window while maintaining aspect ratio.
static glui32 optimal_dice_pixel_size(glui32 *width, glui32 *height)
{
    int ideal_width = 8;
    int ideal_height = 8;

    *width = ideal_width;
    *height = ideal_height;
    int multiplier = 1;
    glui32 graphwidth = 0, graphheight = 0;
    if (LeftDiceWin)
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

// Draws a box-drawing character border around a text grid window using
// Unicode box-drawing characters (heavy lines: U+250F, U+2501, etc.).
static void draw_border(winid_t win)
{
    glui32 width, height;
    glk_stream_set_current(glk_window_get_stream(win));
    glk_window_get_size(win, &width, &height);
    // Too small for a border (and the unsigned sizes below would wrap around)
    if (width < 2 || height < 1)
        return;
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

// Draws the stat labels in a battle panel. In personal combat, shows
// "SKILL" and "STAMINA"; in ship combat (boatflag), shows "STRIKE" and
// "CRW STR" instead. The player's panel also gets a "YOU" label.
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

// Redraws the entire battle UI: recalculates dice sizing, draws borders
// on both panels, and fills in the stat labels.
static void redraw_battle_screen(int boatflag)
{
    glui32 graphwidth = 0, graphheight = 0, optimal_width, optimal_height;

    if (LeftDiceWin)
        glk_window_get_size(LeftDiceWin, &graphwidth, &graphheight);

    dice_pixel_size = optimal_dice_pixel_size(&optimal_width, &optimal_height);
    dice_x_offset = (graphwidth - optimal_width) / 2;
    dice_y_offset = (graphheight - optimal_height - dice_pixel_size) / 2;

    draw_border(Top);
    draw_border(BattleRight);

    redraw_static_text(Top, boatflag);
    redraw_static_text(BattleRight, boatflag);
}

static void close_dice_windows(void)
{
    if (LeftDiceWin)
        glk_window_close(LeftDiceWin, NULL);
    if (RightDiceWin)
        glk_window_close(RightDiceWin, NULL);
    LeftDiceWin = RightDiceWin = NULL;
}

// Creates the split-screen battle layout by subdividing the Top window:
//   Top (left text grid)   = enemy stats    | BattleRight (right text grid) = player stats
//   LeftDiceWin (graphics) = enemy dice     | RightDiceWin (graphics)       = player dice
// Also sets dice colour based on the active palette (C64 blue vs Spectrum red).
static void setup_battle_screen(int boatflag)
{
    /* Without split screen there is no status window to subdivide (Top is
       the main text window), so open one for the duration of the battle.
       blood_battle() closes it again. */
    if (Top == Bottom) {
        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, 7,
            wintype_TextGrid, GLK_STATUS_ROCK);
        if (Top == NULL)
            Top = Bottom;
    }
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
    /* The dice windows are NULL if the Glk library has no graphics
       windows. The dice are then only shown as text in the panels. */
    if (LeftDiceWin && RightDiceWin
        && glk_style_measure(Top, style_Normal, stylehint_BackColor,
            &background_colour)) {
        glk_window_set_background_color(LeftDiceWin, background_colour);
        glk_window_set_background_color(RightDiceWin, background_colour);

        glk_window_clear(LeftDiceWin);
        glk_window_clear(RightDiceWin);
    }

    if (palchosen >= C64A)
        dice_colour = 0x5f48e9;
    else
        dice_colour = 0xff0000;

    redraw_battle_screen(boatflag);
}

// Main battle entry point. Looks up the enemy at the current location,
// sets up the battle UI, runs the combat loop, then tears down the
// battle windows and restores the normal game display.
static void blood_battle(void)
{
    int enemy, strike, stamina, boatflag;
    enemy = get_enemy_stats(&strike, &stamina, &boatflag);
    if (enemy == 0) {
        fprintf(stderr, "Seas of blood battle called with no enemy in location?\n");
        return;
    }
    setup_battle_screen(boatflag);
    battle_loop(strike, stamina, boatflag);
    if (boatflag)
        swap_stamina_and_crew_strength(); // Restore stamina/crew strength to their original counters
    close_dice_windows();
    glk_window_close(BattleRight, NULL);
    if (!split_screen && Top != Bottom) {
        glk_window_close(Top, NULL);
        Top = Bottom;
    }
    CloseGraphicsWindow();
    OpenGraphicsWindow();
    SeasOfBloodRoomImage();
}

// Searches the enemy_table for an enemy item located in the current room.
// Each 4-byte table entry is: item_index, strike, stamina, boatflag.
// If boatflag is set, swaps stamina and crew strength counters so that
// combat damage applies to crew strength instead. Returns the enemy item
// index, or 0 if no enemy is found.
static int get_enemy_stats(int *strike, int *stamina, int *boatflag)
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

// Draws a filled rectangle in a dice graphics window, scaled by
// dice_pixel_size and offset by the centering offsets.
static void draw_rect(winid_t win, int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color)
{
    if (win == NULL)
        return;
    glk_window_fill_rect(win, color, x * dice_pixel_size + dice_x_offset,
        y * dice_pixel_size + dice_y_offset,
        width * dice_pixel_size, height * dice_pixel_size);
}

// Renders a single die face (1-6) in a graphics window. The die body is
// drawn as two overlapping rectangles, then the pips are punched
// out in the background colour. The number parameter is 0-based (0=1 pip).
static void draw_graphical_dice(winid_t win, int number)
{
    // Die body: two overlapping rectangles
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

// Updates both dice displays: clears the graphics windows, redraws the
// graphical dice faces, and prints Unicode dice characters (U+2680-U+2685)
// plus numeric values in the appropriate text panel.
static void update_dice(int our_turn, int left_dice, int right_dice)
{
    left_dice--;
    right_dice--;
    if (LeftDiceWin)
        glk_window_clear(LeftDiceWin);
    if (RightDiceWin)
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

// Displays the combat total (dice + strike skill = sum) in the active panel.
static void print_sum(int our_turn, int sum, int strike)
{
    winid_t win = our_turn ? BattleRight : Top;
    glk_stream_set_current(glk_window_get_stream(win));

    glk_window_move_cursor(win, 6, 1);

    SOBPrint(win, "+ %d = %d", strike, sum);

    glk_stream_set_current(glk_window_get_stream(BattleRight));
    glk_window_move_cursor(BattleRight, 6, 1);
    glk_put_string("+ 9 = ");
}

// Refreshes the strike/stamina (or crew strike/strength) values in a
// battle panel after damage is dealt.
static void update_result(int our_turn, int strike, int stamina, int boatflag)
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

// Clears the dice sum line in both battle panels, then redraws their borders.
static void clear_result(void)
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

// Clears the stamina/crew strength value line in both panels before updating.
static void clear_stamina(void)
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

// Handles window resize during combat. Tears down and rebuilds the entire
// battle UI, then restores the current stats and borders.
static void RearrangeBattleDisplay(int strike, int stamina, int boatflag)
{
    UpdateSettings();
    glk_cancel_char_event(Top);
    CloseGraphicsWindow();
    glk_window_close(BattleRight, NULL);
    close_dice_windows();
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

// Runs one round of animated dice rolling for both combatants.
//
// The enemy's dice animate on a timer and stop automatically after a random
// number of rolls. The player then sees their dice animate and presses
// ENTER to stop them. The player may also press X to flee (land combat
// only, not allowed at sea (location 1)).
//
// Returns VICTORY, LOSS, DRAW, or FLEE.
static int roll_dice(int strike, int stamina, int boatflag)
{
    clear_result();
    redraw_static_text(BattleRight, boatflag);

    glk_request_timer_events(60);
    int rolls = 0;
    int our_turn = 0;
    int left_dice = 1 + erkyrath_random() % 6;
    int right_dice = 1 + erkyrath_random() % 6;
    int our_result;
    int their_result = 0;
    int their_dice_stopped = 0;

    event_t event;
    int enemy_rolls = 20 + erkyrath_random() % 10;
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
                glk_cancel_char_event(Top);
                glk_stream_set_current(glk_window_get_stream(Bottom));
                SOBPrint(Bottom, "\n");
                glk_window_clear(Bottom);
                glk_put_string("Your throw\n<ENTER> to stop dice");
                if (!boatflag)
                    glk_put_string("    <X> to run");
                glk_request_char_event(Top);
            } else if (our_turn == 0) {
                glk_request_timer_events((glui32)delay);
            }

            rolls++;

            if (rolls & 1)
                left_dice = 1 + erkyrath_random() % 6;
            else
                right_dice = 1 + erkyrath_random() % 6;

            update_dice(our_turn, left_dice, right_dice);
            if (our_turn == 0 && rolls == enemy_rolls) {
                SOBPrint(Bottom, "\n");
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
                SOBPrint(Bottom, "\nYour result: %d + %d + %d = %d\n", left_dice,
                         right_dice, strike, our_result);
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
    return BATTLE_ERROR;
}

// Waits for the player to press ENTER during combat, while still handling
// window resize events to keep the battle display intact.
static void BattleHitEnter(int strike, int stamina, int boatflag)
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

// Main combat loop. Repeats dice rounds until one side's stamina reaches 0,
// or the player flees. Each loss deals 2 stamina damage.
// BitFlag 6 is the "battle lost" flag used by the game engine.
// Battle messages are organized in groups of 5, offset by 16 for boat combat:
//   [1-5]  = player hit messages,   [17-21] = boat: player hit
//   [6-10] = enemy hit messages,    [22-26] = boat: enemy hit
//   [11-15]= draw messages,         [27-31] = boat: draw
static void battle_loop(int strike, int stamina, int boatflag)
{
    /* AUTOWIN is defined by the headless and scene test builds
       (Makefile.headless, saga/test/Makefile), which cannot throw dice. */
#ifdef AUTOWIN
    glk_put_string("YOU HAVE WON!");
    ClearBitFlag(6);
    return;
#endif
    update_result(0, strike, stamina, boatflag);
    update_result(1, 9, Counters[3], boatflag);
    do {
        int result = roll_dice(strike, stamina, boatflag);
        glk_cancel_char_event(Top);
        SOBPrint(Bottom, "\n");
        glk_window_clear(Bottom);
        clear_stamina();
        glk_stream_set_current(glk_window_get_stream(Bottom));
        if (result == LOSS) {
            // Player takes 2 stamina (or crew strength) damage
            Counters[3] -= 2;

            if (Counters[3] <= 0) {
                SOBPrint(Bottom, "%s",
                    boatflag ? "THE BANSHEE HAS BEEN SUNK!"
                             : "YOU HAVE BEEN KILLED!");
                Counters[3] = 0;
                SetBitFlag(6);
                Counters[7] = 0;
            } else {
                SOBPrint(Bottom, "%s", battle_messages[1 + erkyrath_random() % 5 + 16 * boatflag]);
            }
        } else if (result == VICTORY) {
            // Enemy takes 2 stamina damage
            stamina -= 2;
            if (stamina <= 0) {
                glk_put_string("YOU HAVE WON!");
                ClearBitFlag(6);
                stamina = 0;
            } else {
                SOBPrint(Bottom, "%s", battle_messages[6 + erkyrath_random() % 5 + 16 * boatflag]);
            }
        } else if (result == FLEE) {
            SetBitFlag(6);
            MyLoc = SavedRoom;
            return;
        } else {
            // Draw — no damage dealt
            SOBPrint(Bottom, "%s", battle_messages[11 + erkyrath_random() % 5 + 16 * boatflag]);
        }

        if (Counters[3] > 0 && stamina > 0) {
            glk_put_string("\n\n<ENTER> to roll dice");
            if (!boatflag)
                glk_put_string("    <X> to run");
        }

        update_result(0, strike, stamina, boatflag);
        update_result(1, 9, Counters[3], boatflag);

        BattleHitEnter(strike, stamina, boatflag);
        SOBPrint(Bottom, "\n\n");
        glk_window_clear(Bottom);

    } while (stamina > 0 && Counters[3] > 0);
}

// Swaps Counters[3] (stamina) and Counters[7] (crew strength) so that the
// battle system's damage to Counters[3] actually affects crew strength
// during ship combat. Called before and after ship battles.
static void swap_stamina_and_crew_strength(void)
{
    uint8_t temp = Counters[7];
    Counters[7] = Counters[3];
    Counters[3] = temp;
}

// Map platform-specific system messages to the engine's canonical
// message keys. The C64 and Spectrum versions store these in different
// orders, so each needs its own mapping.
static void MapBloodSysMessages(int c64)
{
    if (c64 == 1) {
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

        MAP_SYS_MESSAGES(messagekey, 0);
    } else {
        MapSysRange(I_DONT_UNDERSTAND, THATS_BEYOND_MY_POWER, 4);

        MapSysRange(YOU_ARE, HIT_ENTER, 13);

        sys[OK] = system_messages[2];
        sys[PLAY_AGAIN] = system_messages[3];
        sys[YOURE_CARRYING_TOO_MUCH] = system_messages[27];

        /*
         Some Spectrum versions are missing strings
         for the last object (the plank.)
         */
        Items[125].Text = "A loose plank";
        Items[125].AutoGet = "PLAN";
    }
}

// Bug fix: dropping the helmet destroys it (action code 89xx = destroy
// item). If you wear then drop it, it works correctly (action code
// 80xx = move to current room). This appears to be an original bug,
// so we patch the action table to use "move to room" instead.
// Action indices differ between the Spectrum and C64 versions.
static void PatchDropHelmet(void)
{
    if (Actions[154].Opcode[0] == 8910) {
        /* Spectrum */
        debug_print("Patching DROP HELMET!\n");
        Actions[154].Opcode[0] = 8010;
    } else if (Actions[156].Opcode[0] == 8910) {
        /* C64 */
        debug_print("Patching DROP HELMET!\n");
        Actions[156].Opcode[0] = 8010;
    }
}

// Loads game-specific data that isn't part of the standard Scott Adams
// format: enemy stats, battle messages, object image data, and system
// messages. Data offsets differ between the C64 and ZX Spectrum versions.
void LoadExtraSeasOfBloodData(int c64)
{
// MARK: Enemy table

    // Enemy table: 4 bytes per entry (item index, strike, stamina, boatflag),
    // terminated by 0xFF.
    int offset = file_baseline_offset + ((c64 == 1) ? 0x3fee: 0x47b7);

    uint8_t *ptr = SeekToPos(offset);

    int ct;

    for (ct = 0; ct < 124; ct++) {
        enemy_table[ct] = *(ptr++);
        if (enemy_table[ct] == 0xff)
            break;
    }

// MARK: Battle messages

    // 32 compressed text strings for combat flavour text, organized in
    // groups of 5 for player-hit, enemy-hit, and draw outcomes (personal
    // and ship variants). See battle_loop() for indexing.
    offset = file_baseline_offset + ((c64 == 1) ? 0x82f6 : 0x71da);

    ptr = SeekToPos(offset);

    for (int i = 0; i < 32; i++) {
        battle_messages[i] = DecompressText(ptr, i);
    }

// MARK: Extra image data

    // Object sprite image data (2010 bytes) used by the TaylorDraw engine
    // to render item images overlaid on room backgrounds.
    offset = file_baseline_offset + ((c64 == 1) ? 0x5299: 0x3b10);

    int data_length = 2010;

    blood_image_data = MemAlloc(data_length);

    ptr = SeekToPos(offset);

    memcpy(blood_image_data, ptr, data_length);

    // Initialize the TaylorDraw engine with the image data.
    // The callback DrawObjectImages is invoked during room rendering.
    taylor_draw_config config = {
        .data = blood_image_data,
        .end = blood_image_data + data_length,
        .flags = TAYLOR_OLDER,
        .draw_objects = DrawObjectImages,
    };
    InitTaylor(&config);

// MARK: System messages

    MapBloodSysMessages(c64);
    PatchDropHelmet();
}
