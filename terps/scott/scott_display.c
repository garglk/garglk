/*
 *  scott_display.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Room description display, image rendering, inventory listing,
 *  and related UI functions.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

#include "layouttext.h"
#include "line_drawing.h"
#include "irmak.h"
#include "sagagraphics.h"
#include "vector_common.h"
#include "rtpi_graphics.h"

#include "game_specific.h"
#include "gremlins.h"
#include "hulk.h"
#include "robin_of_sherwood.h"
#include "seas_of_blood.h"

#include "parser.h"
#include "saga.h"

/* DrawBlack() blanks the top half of a ZX Spectrum screen (256x192). */
#define ZX_PICTURE_WIDTH 256
#define ZX_PICTURE_HEIGHT 96

/* Initial share (percent) of the main window given to the graphics window
   before it is fitted to the picture. */
#define GRAPHICS_WIN_PERCENT 60
#include "scott.h"
#include "scott_actions.h"
#include "scott_display.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

/* Game-specific constants for display workarounds */
#define SAVAGE_BEAR_ITEM 20
#define SAVAGE_BEACH_ROOM 8
#define SAVAGE_BEAR_IMAGE 9

static strid_t room_description_stream = NULL;
int print_look_to_transcript = 0;
static int pause_next_room_description = 0;

/* Draw a decorative separator line at the bottom of the status window.
   Spectrum style uses asterisks; TRS-80 style uses <---...---> */
static void PrintWindowDelimiter(void)
{
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    /* Without a status window the line goes into the main text window */
    if (Top != Bottom)
        glk_window_move_cursor(Top, 0, TopHeight - 1);
    glk_stream_set_current(glk_window_get_stream(Top));
    if (Options & SPECTRUM_STYLE)
        for (unsigned i = 0; i < TopWidth; i++)
            glk_put_char('*');
    else {
        glk_put_char('<');
        for (unsigned i = 0; i < TopWidth - 2; i++)
            glk_put_char('-');
        glk_put_char('>');
    }
}

/* Open a memory stream for building a room description. Returns its
   (zeroed) buffer, which FlushRoomDescription() renders and frees. */
static char *OpenRoomDescriptionStream(void)
{
    char *buf = MemCalloc(ROOM_DESC_BUFFER_SIZE);
    room_description_stream = glk_stream_open_memory(buf, ROOM_DESC_BUFFER_SIZE, filemode_Write, 0);
    return buf;
}

static void PrintTopLine(winid_t win, const char *line)
{
    Display(win, "%s", line);
}

/* Close the room description memory stream and render its contents.
   In split-screen mode, the text is line-broken to fit the top window;
   otherwise it's printed inline in the main window. Also handles
   transcript output, window delimiter drawing, and the inter-room pause. */
static void FlushRoomDescription(char *buf)
{
    glk_stream_close(room_description_stream, 0);

    if (Transcript && print_look_to_transcript)
        PutUnicodeString(Transcript, buf);

    int print_delimiter = (Options & (TRS80_STYLE | SPECTRUM_STYLE | TI994A_STYLE));

    if (split_screen) {
        if (!ShowTextInTopWindow(Top, Bottom, buf, PrintTopLine))
            print_delimiter = 0;
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    } else {
        Display(Bottom, "%s", buf);
    }

    if (print_delimiter) {
        PrintWindowDelimiter();
    }

    if (pause_next_room_description) {
        Delay(0.8);
        pause_next_room_description = 0;
    }

    free(buf);
}

/* Render inventory in the graphics window for US-variant games */
void UpdateUSInventory(void)
{
    char *buf = OpenRoomDescriptionStream();
    ListInventory(1);
    FlushRoomDescription(buf);
    InventoryUS();
}

int ItIsDark(void)
{
    return (TestBitFlag(DARKBIT) && !IsPresent(LIGHT_SOURCE));
}

void DrawBlack(void)
{
    if (Graphics == NULL)
        return;
    glk_window_fill_rect(Graphics, 0, x_offset, 0, ZX_PICTURE_WIDTH * (glui32)pixel_size,
        ZX_PICTURE_HEIGHT * (glui32)pixel_size);
}

/* Open the main text window and set its styles. */
void OpenBottomWindow(void)
{
    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_Proportional, 0);
    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_Indentation, 20);
    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_ParaIndentation,
        20);
    glk_stylehint_set(wintype_TextBuffer, style_Preformatted, stylehint_Justification, stylehint_just_Centered);

    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    if (Bottom == NULL)
        glk_exit();
    glk_set_window(Bottom);
}

/* Open or find the status text-grid window above the main window */
void OpenTopWindow(void)
{
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top == NULL) {
        if (split_screen) {
            Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed,
                TopHeight, wintype_TextGrid, GLK_STATUS_ROCK);
            if (Top == NULL) {
                split_screen = 0;
                Top = Bottom;
            } else {
                glk_window_get_size(Top, &TopWidth, NULL);
            }
        } else {
            Top = Bottom;
        }
    }
}

/* Close the status window, for a full-height picture sequence.
   OpenTopWindow() brings it back. Without split screen there is no
   status window, and Top is the main text window. */
void CloseTopWindow(void)
{
    if (Top != NULL && Top != Bottom)
        glk_window_close(Top, NULL);
    Top = NULL;
}

/* Compute the largest integer pixel multiplier that fits the game's
   native image dimensions within the available graphics area. */
glui32 OptimalPictureSize(glui32 graphwidth, glui32 graphheight, glui32 *outwidth, glui32 *outheight)
{
    glui32 multiplier = graphheight / ImageHeight;
    if (ImageWidth * multiplier > graphwidth)
        multiplier = graphwidth / ImageWidth;

    if (multiplier == 0)
        multiplier = 1;

    *outwidth = ImageWidth * multiplier;
    *outheight = ImageHeight * multiplier;

    return multiplier;
}

/* Scale the image to the Graphics window: set pixel_size, and x_offset to
   centre it horizontally. The window height and the scaled
   image height are returned in graphheight and optimal_height. */
void FitPictureToGraphicsWindow(glui32 *graphheight, glui32 *optimal_height)
{
    glui32 graphwidth, optimal_width;
    glk_window_get_size(Graphics, &graphwidth, graphheight);
    pixel_size = OptimalPictureSize(graphwidth, *graphheight, &optimal_width, optimal_height);
    x_offset = ((int)graphwidth - (int)optimal_width) / 2;
}

/* Whether pictures can be shown at all: the Glk library has graphics
   windows and, in Spatterlight, the graphics setting is on. Gargoyle
   answers no here when its "graphics" setting is 0. */
int GraphicsEnabled(void)
{
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return 0;
#endif
    return glk_gestalt(gestalt_Graphics, 0) != 0;
}

/* Open the graphics window between the status window and the main text
   window, sized to fit the game's native image resolution at the best
   integer scale. Re-opens the status window above it if needed. Without
   split screen (Top == Bottom) there is no status window to re-open. */
void OpenGraphicsWindow(void)
{
    if (!GraphicsEnabled())
        return;
    glui32 graphheight, optimal_height;

    if (Top == NULL)
        Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics == NULL && Top != NULL && Top != Bottom) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_close(Top, NULL);
        Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional,
            GRAPHICS_WIN_PERCENT, wintype_Graphics, GLK_GRAPHICS_ROCK);
        FitPictureToGraphicsWindow(&graphheight, &optimal_height);

        if (graphheight > optimal_height) {
            winid_t parent = glk_window_get_parent(Graphics);
            if (parent)
                glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                    optimal_height, NULL);
        }

    /* Set the graphics window background to match
       the main window background, best as we can,
       and clear the window. */
        glui32 background_color;
        if (Bottom && glk_style_measure(Bottom, style_Normal, stylehint_BackColor, &background_color)) {
            glk_window_set_background_color(Graphics, background_color);
            glk_window_clear(Graphics);
        }

        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, TopHeight,
            wintype_TextGrid, GLK_STATUS_ROCK);
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    } else {
        if (!Graphics)
            Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional, GRAPHICS_WIN_PERCENT,
                wintype_Graphics, GLK_GRAPHICS_ROCK);
        FitPictureToGraphicsWindow(&graphheight, &optimal_height);
        winid_t parent = glk_window_get_parent(Graphics);
        if (parent)
            glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                optimal_height, NULL);
    }
}

void CloseGraphicsWindow(void)
{
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics) {
        glk_window_close(Graphics, NULL);
        Graphics = NULL;
        if (Top != NULL)
            glk_window_get_size(Top, &TopWidth, &TopHeight);
    }
}

/* Draw a non-US game image using the appropriate rendering method:
   Howarth vector graphics (format 99) or bitmap picture number. */
void DrawImage(int image)
{
    if (!GraphicsEnabled())
        return;
    OpenGraphicsWindow();
    if (Graphics == NULL) {
        debug_print("DrawImage: Graphic window NULL?\n");
        return;
    }
    if (Game->picture_format_version == HOWARTH_FORMAT)
        DrawHowarthVectorPicture(image);
    else {
        DrawPictureNumber(image, (Game->type == SEAS_OF_BLOOD_VARIANT));
        last_image_index = image;
    }
}

/* Draw the overlay images of the items in the current room whose image
   belongs to this room. */
void DrawRoomItemImages(void)
{
    for (int ct = 0; ct <= GameHeader.NumItems; ct++)
        if (Items[ct].Image && Items[ct].Location == MyLoc) {
            if ((Items[ct].Flag & IMAGE_INDEX_MASK) == MyLoc) {
                DrawImage(Items[ct].Image);
                /* Draw the correct image of the bear on the beach */
            } else if (Game->type == SAVAGE_ISLAND_VARIANT && ct == SAVAGE_BEAR_ITEM && MyLoc == SAVAGE_BEACH_ROOM) {
                DrawImage(SAVAGE_BEAR_IMAGE);
            }
        }
}

/* Draw the image for the current room, with special handling for
   darkness (black screen or fuzzy image), US-variant layout, and
   game-specific overrides (Seas of Blood, Robin of Sherwood, Hulk,
   Gremlins, Savage Island). Also draws visible item overlays. */
void DrawRoomImage(void)
{
    if (CurrentGame == ADVENTURELAND || CurrentGame == ADVENTURELAND_C64) {
        AdventurelandDarkness();
    }

    int dark = ItIsDark();

    if (dark && Graphics != NULL && (Rooms[MyLoc].Image != NO_IMAGE || Game->type == US_VARIANT)) {
        vector_image_shown = -1;
        VectorState = NO_VECTOR_IMAGE;
        glk_request_timer_events(0);
        if (Game->type == US_VARIANT) {
            glk_window_clear(Graphics);
            if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
                if (!DrawFuzzyRoom(MyLoc)) {
                    ClearVDP();
                }
            } else {
                DrawUSRoom(0);
            }
            DrawImageOrVector();
        } else {
            DrawBlack();
        }
        return;
    }

    if (Game->type == US_VARIANT) {
        LookUS();
        return;
    }

    switch (CurrentGame) {
    case SEAS_OF_BLOOD:
    case SEAS_OF_BLOOD_C64:
        SeasOfBloodRoomImage();
        return;
    case ROBIN_OF_SHERWOOD:
    case ROBIN_OF_SHERWOOD_C64:
        RobinOfSherwoodLook();
        return;
    case HULK:
    case HULK_C64:
        HulkLook();
        return;
    default:
        break;
    }

    if (Rooms[MyLoc].Image == NO_IMAGE) {
        CloseGraphicsWindow();
        return;
    }

    if (dark)
        return;

    if (Game->picture_format_version == HOWARTH_FORMAT) {
        DrawImage(MyLoc - 1);
        return;
    }

    if (Game->type == GREMLINS_VARIANT) {
        GremlinsLook();
    } else {
        DrawImage(Rooms[MyLoc].Image & IMAGE_INDEX_MASK);
    }
    DrawRoomItemImages();
    if (Game->type == GREMLINS_VARIANT)
        GremlinsKeepBlenderFrame();
}

static void WriteToRoomDescriptionStream(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((__format__(__printf__, 1, 2)))
#endif
    ;

static void WriteToRoomDescriptionStream(const char *fmt, ...)
{
    if (room_description_stream == NULL)
        return;
    va_list ap;
    char msg[DISPLAY_BUFFER_SIZE];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    glk_put_string_stream(room_description_stream, msg);
}

/* List room exits. In ZX Spectrum style the list (with its header) is only
   shown if there are exits; otherwise the header is always shown, with
   "None" if there are no exits. */
static void ListExits(int spectrum_style)
{
    int f = 0;

    if (!spectrum_style)
        WriteToRoomDescriptionStream("\n\n%s", sys[EXITS]);

    for (int ct = 0; ct < NUM_EXITS; ct++) {
        if (Rooms[MyLoc].Exits[ct] != 0) {
            if (f) {
                WriteToRoomDescriptionStream("%s", sys[EXITS_DELIMITER]);
            } else if (spectrum_style) {
                if (!(Options & PC_STYLE))
                    WriteToRoomDescriptionStream("\n\n");
                WriteToRoomDescriptionStream("%s", sys[EXITS]);
            }
            /* sys[] begins with the exit names */
            WriteToRoomDescriptionStream("%s", sys[ct]);
            f = 1;
        }
    }

    if (spectrum_style)
        WriteToRoomDescriptionStream("\n");
    else if (f == 0)
        WriteToRoomDescriptionStream("%s", sys[NONE]);
}

/* Print one item of a room or inventory list. The delimiter goes before
   every item but the first, or in TRS-80 and Spectrum style after every
   item. */
static void PrintListItem(void (*print_function)(const char *fmt, ...), int item, int first)
{
    if (!first && !(Options & (TRS80_STYLE | SPECTRUM_STYLE)))
        print_function("%s", sys[ITEM_DELIMITER]);
    print_function("%s", Items[item].Text);
    if (Options & (TRS80_STYLE | SPECTRUM_STYLE))
        print_function("%s", sys[ITEM_DELIMITER]);
}

/* The first item at or after index from that is in location loc and has a
   description, or -1 if there are no more. Items without a description are
   in play but never listed. */
static int NextListedItem(int loc, int from)
{
    for (int i = from; i <= GameHeader.NumItems; i++) {
        if (Items[i].Location != loc)
            continue;
        if (Items[i].Text[0] != 0)
            return i;
        debug_print("Invisible item in location %d: %d\n", loc, i);
    }
    return -1;
}

static int ItemEndsWithPeriod(int item)
{
    if (item < 0 || item > GameHeader.NumItems)
        return 0;
    const char *desc = Items[item].Text;
    if (desc != NULL && desc[0] != 0) {
        const char lastchar = desc[strlen(desc) - 1];
        if (lastchar == '.' || lastchar == '!') {
            return 1;
        }
    }
    return 0;
}

/* Build and display the full room description: room image, room text,
   exits, visible items, and optionally the inventory. The description
   is assembled in a memory stream, then flushed to the appropriate window. */
void Look(void)
{
    showing_inventory = 0;
    DrawRoomImage();

    if (split_screen && Top == NULL)
        return;

    char *buf = OpenRoomDescriptionStream();

    Room *r;
    int ct, f;

    if (!split_screen) {
        WriteToRoomDescriptionStream("\n");
    } else if (Transcript && print_look_to_transcript) {
        glk_put_char_stream_uni(Transcript, '\n');
    }

    if (ItIsDark()) {
        WriteToRoomDescriptionStream("%s", sys[TOO_DARK_TO_SEE]);
        FlushRoomDescription(buf);
        return;
    }

    r = &Rooms[MyLoc];

    if (!r->Text) {
        /* Flush rather than bare-return: buf and room_description_stream are
           already allocated/open, and only FlushRoomDescription frees and
           closes them. A bare return leaks both every time it is hit. */
        FlushRoomDescription(buf);
        return;
    }
    /* An initial asterisk means the room description should not */
    /* start with "You are" or equivalent */
    if (*r->Text == '*') {
        WriteToRoomDescriptionStream("%s", r->Text + 1);
    } else {
        WriteToRoomDescriptionStream("%s%s", sys[YOU_ARE], r->Text);
    }

    if (!(Options & SPECTRUM_STYLE)) {
        ListExits(0);
        WriteToRoomDescriptionStream(".\n");
    }

    f = 0;
    for (ct = NextListedItem(MyLoc, 0); ct != -1; ct = NextListedItem(MyLoc, ct + 1)) {
        if (f == 0) {
            WriteToRoomDescriptionStream("%s", sys[YOU_SEE]);
            if (Options & SPECTRUM_STYLE && !(Options & PC_STYLE))
                WriteToRoomDescriptionStream("\n");
        }
        PrintListItem(WriteToRoomDescriptionStream, ct, f == 0);
        f = 1;
    }

    if ((Options & TI994A_STYLE) && f) {
        WriteToRoomDescriptionStream(".");
    } else if (Options & PC_STYLE && !f)
        WriteToRoomDescriptionStream(". ");

    if (Options & SPECTRUM_STYLE) {
        ListExits(1);
    } else if (f) {
        WriteToRoomDescriptionStream("\n");
    }

    if ((AutoInventory || (Options & FORCE_INVENTORY)) && !(Options & FORCE_INVENTORY_OFF)) {
        WriteToRoomDescriptionStream("\n");
        ListInventory(1);
    }

    FlushRoomDescription(buf);
}

static void WriteToLowerWindow(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((__format__(__printf__, 1, 2)))
#endif
    ;

static void WriteToLowerWindow(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vDisplay(Bottom, fmt, ap);
    va_end(ap);
}

/* List the player's carried items. When upper=1, writes to the room
   description stream (status window); otherwise writes to the main window. */
void ListInventory(int upper)
{
    void (*print_function)(const char *fmt, ...);
    if (upper) {
        print_function = WriteToRoomDescriptionStream;
    } else {
        print_function = WriteToLowerWindow;
    }

    int lastitem = -1;
    print_function("%s", sys[INVENTORY]);
    for (int i = NextListedItem(CARRIED, 0); i != -1; i = NextListedItem(CARRIED, i + 1)) {
        PrintListItem(print_function, i, lastitem == -1);
        lastitem = i;
    }
    if (lastitem == -1)
        print_function("%s", sys[NOTHING]);
    else if (Options & TI994A_STYLE) {
        if (!ItemEndsWithPeriod(lastitem))
            print_function(".");
        print_function(" ");
    }
    if (upper) {
        WriteToRoomDescriptionStream("\n");
    } else if (Transcript) {
        glk_put_char_stream_uni(Transcript, '\n');
    }
}

/* Ask for the room description and picture to be redrawn before the
   next prompt. */
void RequestRedraw(void)
{
    should_look_in_transcript = should_draw_image = 1;
}

/* Perform a Look with a brief pause, used during multi-command
   sequences so the player can see intermediate room descriptions. */
void LookWithPause(void)
{
    if (Rooms[MyLoc].Text == NULL || MyLoc == 0)
        return;
    char fc = Rooms[MyLoc].Text[0];
    if (fc == 0 || fc == '.' || fc == ' ')
        return;
    RequestRedraw();
    pause_next_room_description = 1;
    Look();
}
