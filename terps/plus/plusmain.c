//
//  plusmain.c
//  The Plus interpreter
//
//  Created by Petter Sjölund on 2022-05-06.
//

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "glkstart.h"

#include "animations.h"
#include "apple2detect.h"
#include "apple2draw.h"
#include "sagadraw_host.h"
#include "atari8detect.h"
#include "c64detect.h"
#include "common.h"
#include "definitions.h"
#include "gameinfo.h"
#include "graphics.h"
#include "layouttext.h"
#include "dumpactions.h"
#include "loaddatabase.h"
#include "opcodes.h"
#include "parseinput.h"
#include "randomness.h"
#include "restorestate.h"
#include "stdetect.h"
#include "stdraw.h"

const char *game_file = NULL;    /* Path to the game file being played */
char *DirPath = "";              /* Directory containing the game file, with its slash (for image lookups) */
size_t DirPathLength = 0;

uint8_t *mem;                    /* Raw game file contents (for binary format detection) */
size_t memlen;

const char *sys[MAX_SYSMESS];    /* System message strings (indexed by SysMessageType) */

uint64_t BitFlags = 0;           /* 64 general-purpose bit flags used by the action table */
static int Options;                     /* Runtime option flags (NO_DELAYS, FORCE_INVENTORY, etc.) */

static int split_screen = 1;            /* Whether to use a separate upper window for room descriptions */
int lastwasnewline = 1;          /* Tracks whether the last output character was a newline */
static int pendingcomma = 0;            /* A trailing comma was deferred from the last message */
int should_restart = 0;          /* Set by DoneIt() to trigger a restart at the top of the main loop */

static glui32 TopWidth, TopHeight;      /* Current dimensions of the status/room-description window */

winid_t Bottom, Top, Graphics;   /* Glk windows: main text, status bar, graphics */

strid_t Transcript = NULL;

const GameInfo *Game = NULL;           /* Detected game identity and metadata */

SystemType CurrentSys = SYS_UNKNOWN; /* Detected platform (C64, Apple II, ST, Atari 8-bit) */


int16_t Counters[NUM_COUNTERS];            /* General-purpose counter registers used by the action table */
#define MAX_LOOPS 32

/* The most lines one scan of the action table may look at. Loop backs
   (113) and start overs (111, and system commands) jump back, so a damaged table can scan for ever,
   as it would hang the original. The most a walkthrough reaches is 8111
   (Fantastic Four). */
#define MAX_SCAN_LINES 1000000

static int loops[MAX_LOOPS];            /* Loop stack: stores action indices for loop-back commands */
static int loop_index;                  /* Current depth in the loop stack */
static int keep_going = 0;              /* Continue scanning the action table after a match */
static int startover = 0;               /* Restart scanning the action table from the beginning */
static int found_match = 0;             /* An action matched the current input */
static int command_done = 0;            /* A matching action passed its conditions and ran */
static int pop_at_loopback = 0;         /* Command 115: the next loop back (113) ends the loop instead */

/* JustStarted is only used for the error message "Can't undo on first move" */
int JustStarted = 1;

int showing_inventory = 0;       /* Non-zero while the Claymorgue inventory display is active */

Header GameHeader;
Item *Items;
Room *Rooms;

ObjectImage *ObjectImages;       /* Mapping of objects to their image indices and rooms */
char **Messages;                 /* Message strings referenced by action commands */
Action *Actions;                 /* The game's action table (conditions + commands) */

int ImageWidth = 280;            /* Native image dimensions (platform-dependent) */
int ImageHeight = 158;

glui32 TimerRate = 0;            /* Current Glk timer rate (ms), 0 = stopped */

static char DelimiterChar = '_'; /* Character used to draw the status window separator line */

/* Close the transcript stream (if open) and exit via Glk. */
GLK_ATTRIBUTE_NORETURN void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    glk_exit();
}

/* Bit flag accessors. The 64-bit BitFlags word holds general-purpose
   flags tested and set by the action table; several indices have fixed
   meanings (e.g. DARKBIT=15, DRAWBIT=34, GRAPHICSBIT=35, STOPTIMEBIT=63).

   IsValidBit() reports an out-of-range bit index on behalf of caller,
   and returns 1 if the bit is in range. */
static int IsValidBit(const char *caller, int bit)
{
    if (bit >= 64 || bit < 0) {
        fprintf(stderr, "%s: bit %d out of range!\n", caller, bit);
        return 0;
    }
    return 1;
}

void SetBit(int bit)
{
    if (IsValidBit("SetBit", bit))
        BitFlags |= (uint64_t)1 << bit;
}

void ResetBit(int bit)
{
    if (IsValidBit("ResetBit", bit))
        BitFlags &= ~((uint64_t)1 << bit);
}

int IsSet(int bit)
{
    if (!IsValidBit("IsSet", bit))
        return 0;
    return ((BitFlags & ((uint64_t)1 << bit)) != 0);
}

/* Memory stream used to compose room descriptions before displaying
   them in the status window. Written to by WriteToRoomDescriptionStream(),
   flushed by FlushRoomDescription(). */
static strid_t room_description_stream = NULL;

/* Printf-style output to a specific Glk window. */
void Display(winid_t w, const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glk_put_string_stream(glk_window_get_stream(w), msg);
}

/* Calculate the largest integer scaling multiplier that fits the native
   image dimensions into the graphics window. Returns the multiplier and
   writes the scaled dimensions to *width and *height. */
static glui32 OptimalPictureSize(glui32 *width, glui32 *height)
{
    int w = ImageWidth;
    int h = ImageHeight;

    int multiplier = 1;
    glui32 graphwidth, graphheight;
    glk_window_get_size(Graphics, &graphwidth, &graphheight);
    multiplier = graphheight / h;
    if (w * multiplier > (int)graphwidth)
        multiplier = graphwidth / w;

    if (multiplier == 0)
        multiplier = 1;

    *width = w * multiplier;
    *height = h * multiplier;

    return multiplier;
}

/* Pick the pixel scaling for the graphics window and centre the image
   horizontally in it. Writes the window height and the scaled image
   height to *graphheight and *optimal_height. */
static void FitImageToGraphicsWindow(glui32 *graphheight, glui32 *optimal_height)
{
    glui32 graphwidth, optimal_width;
    glk_window_get_size(Graphics, &graphwidth, graphheight);
    pixel_size = OptimalPictureSize(&optimal_width, optimal_height);
    x_offset = ((int)graphwidth - (int)optimal_width) / 2;
}

/* Make the graphics window exactly height pixels tall. */
static void SetGraphicsWindowHeight(glui32 height)
{
    winid_t parent = glk_window_get_parent(Graphics);
    if (parent)
        glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
            height, NULL);
}

static winid_t OpenGraphicsWindowAboveBottom(void)
{
    return glk_window_open(Bottom, winmethod_Above | winmethod_Proportional,
        60, wintype_Graphics, GLK_GRAPHICS_ROCK);
}

/* Whether pictures can be shown at all: false when the Glk library has
   no graphics windows, or has them switched off. */
static int GraphicsAvailable(void)
{
    return glk_gestalt(gestalt_Graphics, 0) != 0;
}

/* Open (or reconfigure) the Glk graphics window above the text windows.
   Calculates the optimal pixel scaling, centres the image horizontally,
   and sizes the window to fit exactly. If a status window already exists
   it is temporarily closed and reopened below the graphics window. */
void OpenGraphicsWindow(void)
{
    if (!GraphicsAvailable())
        ResetBit(GRAPHICSBIT);
    if (!IsSet(GRAPHICSBIT))
        return;
    glui32 graphheight, optimal_height;
    y_offset = 0;

    if (Top == NULL)
        Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics == NULL && Top != NULL) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_close(Top, NULL);
        Graphics = OpenGraphicsWindowAboveBottom();
        FitImageToGraphicsWindow(&graphheight, &optimal_height);

        if (graphheight > optimal_height)
            SetGraphicsWindowHeight(optimal_height);

        /* Set the graphics window background to match
         * the main window background, best as we can,
         * and clear the window.
         */
        glui32 background_color;
        if (glk_style_measure(Bottom, style_Normal, stylehint_BackColor,
                &background_color)) {
            glk_window_set_background_color(Graphics, background_color);
            glk_window_clear(Graphics);
        }

        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, TopHeight,
            wintype_TextGrid, GLK_STATUS_ROCK);
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    } else {
        if (!Graphics)
            Graphics = OpenGraphicsWindowAboveBottom();
        FitImageToGraphicsWindow(&graphheight, &optimal_height);
        SetGraphicsWindowHeight(optimal_height);
    }
}

/* Close the graphics window and restore the status window dimensions. */
void CloseGraphicsWindow(void)
{
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics) {
        glk_window_close(Graphics, NULL);
        Graphics = NULL;
        GraphicsWindowChanged();
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    }
}

/* Set the Glk timer rate and record it in TimerRate. */
void SetTimer(glui32 milliseconds)
{
    TimerRate = milliseconds;
    glk_request_timer_events(milliseconds);
}

/* Sync runtime options (delays, inventory display, graphics) with
   Spatterlight's user-facing preferences. */
static void UpdateSettings(void)
{
#ifdef SPATTERLIGHT
    if (gli_sa_delays)
        Options &= ~NO_DELAYS;
    else
        Options |= NO_DELAYS;

    switch (gli_sa_inventory) {
    case 0:
        Options &= ~(FORCE_INVENTORY | FORCE_INVENTORY_OFF);
        break;
    case 1:
        Options = (Options | FORCE_INVENTORY) & ~FORCE_INVENTORY_OFF;
        break;
    case 2:
        Options = (Options | FORCE_INVENTORY_OFF) & ~FORCE_INVENTORY;
        break;
    }

    if (gli_enable_graphics) {
        SetBit(GRAPHICSBIT);
    } else {
        ResetBit(GRAPHICSBIT);
    }
#endif
}

static void FlushRoomDescription(char *buf, int transcript);
static void ListInventoryInStatus(void);

/* Room for the longest room description plus a long list of items or a
   full inventory. Anything beyond this is cut off. */
#define ROOM_DESCRIPTION_SIZE 4000

/* Open room_description_stream on a new zeroed buffer, which
   FlushRoomDescription() frees. The buffer has one byte more than the
   stream may write, so that it always is a terminated string. */
static char *OpenRoomDescriptionStream(void)
{
    char *buf = MemCalloc(ROOM_DESCRIPTION_SIZE + 1);
    room_description_stream = glk_stream_open_memory(buf, ROOM_DESCRIPTION_SIZE, filemode_Write, 0);
    return buf;
}

/* Claymorgue Castle displays the inventory in the upper window alongside
   images of carried items. This renders the inventory text to a memory
   stream, flushes it to the status window, then redraws room 33's image
   with overlays for each carried object. */
static void UpdateClaymorgueInventory(void)
{
    char *buf = OpenRoomDescriptionStream();
    ListInventoryInStatus();
    FlushRoomDescription(buf, 0);
    DrawRoomImage(33);
    DrawObjectImages(33, CARRIED);
}

static int AnimationCounter = 0; /* Tick count for animation rate division */

/* How many events of the Glk timer, at the rate it has now, make up
   this many milliseconds: at least one. */
static int TimerTicksIn(int milliseconds)
{
    int ticks = milliseconds / (int)MAX(TimerRate, 1);
    return MAX(ticks, 1);
}

/* Handle non-input Glk events: window rearrange (redraw everything) and
   timer ticks (advance animations and colour cycling). Animation updates
   are rate-divided so the animation timer can run faster than the frame
   rate (e.g. when colour cycling shares the same timer). */
void Updates(event_t ev)
{
    if (ev.type == evtype_Arrange) {
        ImgType overlay_type = LastImgType;
        int overlay_index = LastImgIndex;
        CloseGraphicsWindow();
        UpdateSettings();
        OpenGraphicsWindow();
        if (AnimationRunning && LastAnimationBackground) {
            DrawImageWithTypeAndNumber('S', LastAnimationBackground);
        } else {
            SetBit(DRAWBIT);
            if (showing_inventory == 1) {
                UpdateClaymorgueInventory();
            } else {
                Look(0);
                DrawOverlayImage(overlay_type, overlay_index);
            }
        }
    } else if (ev.type == evtype_Timer) {
        AnimationCounter++;
        if (AnimationRunning) {
            int rate = TimerTicksIn(AnimTimerRate);
            if (!IsSet(GRAPHICSBIT)) {
                StopAnimation();
            } else if (AnimationCounter % rate == 0) {
                UpdateAnimation();
            }
        }
        if (ColorCyclingRunning && IsSet(GRAPHICSBIT))
            UpdateColorCycling();
    }
}

/* How long AnyKey waits before it dismisses itself */
#define DISMISS_DELAY 3000

/* Start the timer for AnyKey to dismiss itself, unless it already runs
   (for an animation or colour cycling), in which case its ticks are
   counted at the rate they come. */
static void ArmDismissTimer(void)
{
    if (TimerRate == 0)
        SetTimer(DISMISS_DELAY);
}

/* Wait for a keypress. If timeout is set and no animation is running,
   auto-dismiss after ~3 seconds. Continues processing timer events
   (animations, colour cycling) while waiting. Claymorgue disables
   the timeout to avoid auto-advancing plot text. */
void AnyKey(int timeout, int message)
{
    if (message) {
        SystemMessage(HIT_ANY);
    }

    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;
    int ticks = 0;

    int cancel_after_delay = 0;

    if (CurrentGame == CLAYMORGUE)
        timeout = 0;

    if (timeout && AnimationRunning == 0 && ColorCyclingRunning == 0) {
        ArmDismissTimer();
        cancel_after_delay = 1;
    }

    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            result = 1;
        } else {
            if (cancel_after_delay && ev.type == evtype_Timer) {
                ticks++;
                if (ticks % TimerTicksIn(DISMISS_DELAY) == 0) {
                    result = 1;
                    glk_cancel_char_event(Bottom);
                    if (TimerRate == DISMISS_DELAY)
                        SetTimer(0);
                }
            }

            /* An animation that ends while we wait leaves the
               timeout to run from then */
            if (!AnimationRunning && !cancel_after_delay && timeout) {
                ArmDismissTimer();
                cancel_after_delay = 1;
            }

            Updates(ev);
        }
    } while (result == 0);
}

/* Draw a line of DelimiterChar across the bottom row of the status window. */
/* Currently, DelimiterChar is always an underscore character. */
static void PrintWindowDelimiter(void)
{
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    glk_window_move_cursor(Top, 0, TopHeight - 1);
    glk_stream_set_current(glk_window_get_stream(Top));
    for (int i = 0; i < TopWidth; i++)
        glk_put_char(DelimiterChar);
}

static void PrintTopLine(winid_t win, const char *line)
{
    Display(win, "%s", line);
}

/* Close the room description memory stream, optionally write it to the
   transcript, then display it. In split-screen mode, the text is word-
   wrapped to the status window width and the window is resized to fit;
   otherwise it goes to the lower text buffer. Frees buf when done. */
static void FlushRoomDescription(char *buf, int transcript)
{
    glk_stream_close(room_description_stream, 0);
    room_description_stream = NULL;

    if (Transcript && transcript) {
        if (!lastwasnewline)
            glk_put_string_stream(Transcript, "\n");
        glk_put_string_stream(Transcript, "\n");
        /* Leave out the last two characters (the line break at the end) */
        size_t buflen = strlen(buf);
        buflen = buflen >= 2 ? buflen - 2 : 0;
        char *roomtranscript = MemAlloc(buflen + 1);
        memcpy(roomtranscript, buf, buflen);
        roomtranscript[buflen] = '\0';
        glk_put_string_stream(Transcript, roomtranscript);
        free(roomtranscript);
        glk_put_string_stream(Transcript, " ");
    }

    int print_delimiter = 1;

    if (split_screen) {
        print_delimiter = ShowTextInTopWindow(Top, Bottom, buf, PrintTopLine);
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    } else {
        Display(Bottom, "%s", buf);
    }

    if (print_delimiter) {
        PrintWindowDelimiter();
    }

    free(buf);
}

/* Printf-style output to the room description memory stream. */
PLUS_PRINTF(1, 2)
static void WriteToRoomDescriptionStream(const char *fmt, ...)
{
    if (room_description_stream == NULL)
        return;
    va_list ap;
    char msg[2048];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    glk_put_string_stream(room_description_stream, msg);
}

/* Append a natural-language list of available exits to the room description
   stream (e.g. "I see exits to the North, East, and Down."). */
static void ListExits(void)
{
    int ct = 0;
    int numexits = 0;

    Room *room = PlayerRoom();

    for (int i = 0; i < NUM_DIRECTIONS; i++)
        if (room->Exits[i] != 0)
            numexits++;

    if (numexits == 0) {
        WriteToRoomDescriptionStream("\n");
        return;
    }
    if (numexits > 1)
        WriteToRoomDescriptionStream(" I see exits ");
    else
        WriteToRoomDescriptionStream(" %s", sys[EXITS]);

    for (int i = 0; i < NUM_DIRECTIONS; i++)
        if (room->Exits[i] != 0) {
            if (ct) {
                if (ct == numexits - 1)
                    WriteToRoomDescriptionStream(", and ");
                else
                    WriteToRoomDescriptionStream("%s", sys[EXITS_DELIMITER]);
            } else if (ct < 5 && numexits > 1)
                WriteToRoomDescriptionStream("to the ");
            WriteToRoomDescriptionStream("%s", sys[i]);
            ct++;
        }
    WriteToRoomDescriptionStream(".\n");
}

/* Return "a", "an", or empty string depending on the first character. */
static const char *IndefiniteArticle(const char *word)
{
    char c = word[0];
    char vowels[11] = "aioueyAIOUE";
    if (c == ' ')
        return "\0";
    for (int i = 0; i < 11; i++)
        if (vowels[i] == c)
            return " an ";
    return " a ";
}

/* Print the items in a room, or those carried, as "an x, a y, and a z",
   with lead (if any) before the first of them. Items without a name are
   invisible and left out. Returns how many were printed. */
static int ListItemsAt(int location, const char *lead, PrintFunction print_function)
{
    int count = 0, ct = 0;

    for (int i = 0; i <= GameHeader.NumItems; i++)
        if (Items[i].Location == location && Items[i].Text[0] != 0)
            count++;

    for (int i = 0; i <= GameHeader.NumItems; i++) {
        if (Items[i].Location != location || Items[i].Text[0] == 0)
            continue;
        if (ct == 0) {
            if (lead != NULL)
                print_function("%s", lead);
        } else if (ct == count - 1) {
            print_function(", and");
        } else {
            print_function("%s", sys[ITEM_DELIMITER]);
        }
        print_function("%s%s", IndefiniteArticle(Items[i].Text), Items[i].Text);
        ct++;
    }
    return ct;
}

/* The room is dark if DARKBIT is set and no light source (item flag 2)
   is carried or in the room. */
int IsDark(void)
{
    if (!IsSet(DARKBIT))
        return 0;
    for (int i = 0; i <= GameHeader.NumItems; i++) {
        if ((Items[i].Flag & 2) && (Items[i].Location == MyLoc || Items[i].Location == CARRIED))
            return 0;
    }
    return 1;
}

/* Compose and display the full room description: room text, visible items,
   exit list, and optionally the inventory. Draws the room image if DRAWBIT
   is set. The description is built in a memory stream and flushed to the
   status window (or lower window if split_screen is off). */
void Look(int transcript)
{
    showing_inventory = 0;
    found_match = 1;
    if (IsSet(DRAWBIT)) {
        ResetBit(DRAWBIT);
        DrawCurrentRoom();
    }

    char *buf = OpenRoomDescriptionStream();

    Room *r;

    if (IsDark()) {
        WriteToRoomDescriptionStream("%s", sys[TOO_DARK_TO_SEE]);
        FlushRoomDescription(buf, transcript);
        return;
    }

    r = PlayerRoom();

    if (!r->Text) {
        /* A room without a description: nothing to show */
        glk_stream_close(room_description_stream, 0);
        room_description_stream = NULL;
        free(buf);
        return;
    }
    /* An initial asterisk means the room description should not */
    /* start with "You are" or equivalent */
    if (*r->Text == '*') {
        WriteToRoomDescriptionStream("%s", r->Text + 1);
    } else {
        WriteToRoomDescriptionStream("%s%s", sys[YOU_ARE], r->Text);
    }

    ListItemsAt(MyLoc, sys[YOU_SEE], WriteToRoomDescriptionStream);

    WriteToRoomDescriptionStream(".");

    ListExits();

    if ((Options & FORCE_INVENTORY) && !(Options & FORCE_INVENTORY_OFF)) {
        WriteToRoomDescriptionStream("\n");
        ListInventoryInStatus();
    }

    WriteToRoomDescriptionStream(" ");
    FlushRoomDescription(buf, transcript);
}

void Output(const char *string)
{
    Display(Bottom, "%s", string);
}

/* Output a system message (e.g. "OK", "I don't understand"), preceded
   by a space if we're not at the start of a line. */
void SystemMessage(SysMessageType message)
{
    if (!lastwasnewline)
        Output(" ");
    lastwasnewline = 0;
    Output(sys[message]);
}

/* Open (or find) the status/room-description text grid window. Falls
   back to using the main text buffer if a grid window can't be created. */
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

/* Create all three Glk windows: text buffer, status grid, and graphics. */
static void DisplayInit(void)
{
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    OpenTopWindow();
    OpenGraphicsWindow();
}

static void OutputNumber(int a)
{
    if (lastwasnewline) {
        Display(Bottom, "%d", a);
        lastwasnewline = 0;
    } else {
        Display(Bottom, " %d", a);
    }
}

/* Pause for the given number of milliseconds, continuing to process
   timer events (so animations keep running). Skipped entirely if
   NO_DELAYS is set or an animation is already running. */
static void Delay(int milliseconds)
{
    if (Options & NO_DELAYS || AnimationRunning)
        return;

    /* A timer rate of 0 would turn the timer off, and we would wait
       forever for it to fire. */
    if (milliseconds <= 0)
        return;

    event_t ev;

    if (!glk_gestalt(gestalt_Timer, 0))
        return;

    glk_request_char_event(Bottom);
    glk_cancel_char_event(Bottom);

    SetTimer(milliseconds);

    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_Timer);

    if (!AnimationRunning && !ColorCyclingRunning)
        SetTimer(0);
}

static int RandomPercent(int n)
{
   return (int)(erkyrath_random() % 100) < n;
}

/* Count items in a room (0 = CARRIED). Also stores the carried count
   in Counter[33] when checking carried items. */
static int CountItemsInRoom(int room)
{
    int ct = 0;
    int n = 0;
    if (room == 0)
        room = CARRIED;
    while (ct <= GameHeader.NumItems) {
        if (Items[ct].Location == room)
            n++;
        ct++;
    }
    if (room == CARRIED)
        InventoryCount = n;
    return (n);
}

/* Find the first takeable item whose dictionary word matches 'noun' and
   is in location 'loc' (-1 = any location). Returns 0 if not found. */
static int MatchUpItem(int noun, int loc)
{
    for (int i = 0; i <= GameHeader.NumItems; i++)
        if (Items[i].Dictword == noun && (Items[i].Location == loc || loc == -1) && (Items[i].Flag & 1))
            return i;
    return (0);
}

/* Print a message from the Messages table. Handles leading backslashes
   (suppress space), trailing commas (defer to next message), and trailing
   single spaces (strip). Also writes to the transcript if active. */
static void PrintMessage(int index)
{
    if (JustRestarted && Transcript) {
        glk_put_string_stream(Transcript, "\n\n");
        JustRestarted = 0;
    }
    /* A command number of two bytes can name a message there is none of */
    if (index < 0 || index > GameHeader.NumMessages) {
        debug_print("Print message %d: no such message\n", index);
        return;
    }
    const char *message = Messages[index];
    debug_print("Print message %d: \"%s\"\n", index, message);
    if (lastwasnewline)
        pendingcomma = 0;
    if (message != NULL && message[0] != 0) {
        int i = 0;
        if (message[0] != '\\' && message[0] != ' ') {
            if (pendingcomma)
                Output(",");
            if (!lastwasnewline)
                Output(" ");
        } else {
            while (message[i] == '\\')
                i++;
        }
        pendingcomma = 0;
        while (message[i] != 0 && !(message[i] == ' ' && message[i + 1] == 0)) {
            if (message[i] == ',' && message[i + 1] == 0)
                pendingcomma = 1;
            else
                Display(Bottom, "%c", message[i]);
            i++;
        }
        lastwasnewline = 0;
    }
}

/* Find the index of the first word in a dictionary list that belongs
   to the given synonym group. Returns -1 if there is none. */
static int FindDictWord(int group, DictWord *dict)
{
    for (int i = 0; dict[i].Word != NULL; i++) {
        if (dict[i].Group == group) {
            return i;
        }
    }
    return -1;
}

/* Print the dictionary word for the current noun (CurrentNoun), or
   nothing if it has none. */
static void PrintNoun(void)
{
    int i = FindDictWord(CurrentNoun, Nouns);
    if (i < 0)
        return;
    Display(Bottom, "%s%s", lastwasnewline ? "" : " ", Nouns[i].Word);
    lastwasnewline = 0;
}

/* FindDictWord(), but with the first word of the list for a group that
   has none. */
static int GetAnyDictWord(int group, DictWord *dict)
{
    int i = FindDictWord(group, dict);
    return i < 0 ? 0 : i;
}

/* GetAnyDictWord() for the noun dictionary. */
int GetDictWord(int group)
{
    return GetAnyDictWord(group, Nouns);
}

/* Handle player death: print the death message, turn on lights, move
   to the room in counter 35, and redraw. */
static void PlayerIsDead(void)
{
    debug_print("Player is dead\n");
    SystemMessage(IM_DEAD);
    ResetBit(DARKBIT);
    SetBit(DRAWBIT);
    MyLoc = DeadRoom;
    Look(0);
}

/* If the given object has a room image overlay, set DRAWBIT so the
   room will be redrawn with the updated object state. */
static void CheckForObjectImage(int obj)
{
    for (int i = 0; i <= GameHeader.NumObjImg; i++)
        if (ObjectImages[i].Object == obj) {
            SetBit(DRAWBIT);
            return;
        }
}

/* The action table of a damaged database may refer to items, rooms or
   counters that do not exist. These report it and return 0. */
static int IsValidItem(int item)
{
    if (item < 0 || item > GameHeader.NumItems) {
        debug_print("Item %d out of range!\n", item);
        return 0;
    }
    return 1;
}

static int IsValidRoom(int room)
{
    if (room < 0 || room > GameHeader.NumRooms) {
        debug_print("Room %d out of range!\n", room);
        return 0;
    }
    return 1;
}

static int IsValidCounter(int counter)
{
    if (counter < 0 || counter >= NUM_COUNTERS) {
        debug_print("Counter %d out of range!\n", counter);
        return 0;
    }
    return 1;
}

/* The room the player is in. It is counter 32, which the counter commands
   may write like any other (Fantastic Four swaps it into the main counter
   as scratch), so it is checked where it is used as an index, not where
   it is written. A room that does not exist stops the original
   interpreters with "Subscript out of range", and stops this one here. */
Room *PlayerRoom(void)
{
    if (MyLoc < 0 || MyLoc > GameHeader.NumRooms)
        Fatal("Player room out of range");
    return &Rooms[MyLoc];
}

/* Move an item to a room (or to CARRIED, or out of play), and note
   whether the room picture must be redrawn. */
static void MoveItemTo(int item, int location)
{
    if (!IsValidItem(item))
        return;
    Items[item].Location = location;
    CheckForObjectImage(item);
}

static void PutItemAInRoomB(int itemA, int roomB)
{
    if (!IsValidItem(itemA))
        return;
    debug_print("Item %d (%s) is put in room %d. MyLoc: %d\n", itemA, Items[itemA].Text, roomB, MyLoc);
    Items[itemA].Location = roomB;
    if (roomB == MyLoc)
        CheckForObjectImage(itemA);
}

static void SwapCounters(int index)
{
    debug_print("Main counter (47) is swapped with counter %d\n", index);
    if (!IsValidCounter(index))
        return;
    int temp = CurrentCounter;

    CurrentCounter = Counters[index];
    Counters[index] = temp;
    debug_print("Value of new selected counter is %d\n", CurrentCounter);
}

static void SwapItemLocations(int itemA, int itemB)
{
    if (!IsValidItem(itemA) || !IsValidItem(itemB))
        return;
    int temp = Items[itemA].Location;
    Items[itemA].Location = Items[itemB].Location;
    Items[itemB].Location = temp;
}

static void MoveItemAToLocOfItemB(int itemA, int itemB)
{
    if (!IsValidItem(itemA) || !IsValidItem(itemB))
        return;
    Items[itemA].Location = Items[itemB].Location;
}

/* Game over: ask if the player wants to play again, then either
   set should_restart or exit. */
static void DoneIt(void)
{
    Output("\n");
    lastwasnewline = 1;
    SystemMessage(PLAY_AGAIN);
    Output(" ");
    if (YesOrNo()) {
        should_restart = 1;
    } else {
        CleanupAndExit();
    }
}

/* Printf-style output to the lower text buffer, mirrored to the
   transcript if active. Used by ListInventory. */
PLUS_PRINTF(1, 2)
static void WriteToLowerWindow(const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glk_put_string_stream(glk_window_get_stream(Bottom), msg);
    if (Transcript)
        glk_put_string_stream(Transcript, msg);
}

/* Print what the player carries, as "an x and a y." or the message for
   nothing. Items with empty text are invisible and skipped. */
static void ListCarried(PrintFunction print_function)
{
    if (ListItemsAt(CARRIED, NULL, print_function) == 0)
        print_function("%s", sys[NOTHING]);
    else
        print_function(".");
}

/* Print the player's inventory in the lower text buffer. */
static void ListInventory(void)
{
    SystemMessage(INVENTORY);
    ListCarried(WriteToLowerWindow);
}

/* Print the player's inventory to the room description stream, for the
   status window. */
static void ListInventoryInStatus(void)
{
    WriteToRoomDescriptionStream("%s", sys[INVENTORY]);
    ListCarried(WriteToRoomDescriptionStream);
    WriteToRoomDescriptionStream("\n");
}

static void ClearScreen(void)
{
    glk_window_clear(Bottom);
}

static int ObjectArgument(int arg);

/* System command 12: put the next item at a place in counter 30,
   starting from the item already there, so that a loop can visit them
   all. When there are no more, counter 30 becomes 0 and the loop is
   left at its next loopback, as after a 115. Returns nonzero when the
   action table must be scanned again from the top. */
static int NextItemAt(int mode)
{
    int i = NounObject;
    int place;

    switch (mode) {
    case 1:
        place = ItemRegister;
        break;
    case 2:
        place = MyLoc;
        break;
    case 4:
        place = CARRIED;
        break;
    case 3:
        /* Here or carried. The original only ever looks at one item
           in this mode, and starts over when it is somewhere else. */
        if (IsValidItem(i) && (Items[i].Location == MyLoc || Items[i].Location == CARRIED))
            return 0;
        if (i + 1 <= GameHeader.NumItems)
            return 1;
        pop_at_loopback = 1;
        NounObject = 0;
        return 0;
    default:
        return 0;
    }

    do {
        if (IsValidItem(i) && Items[i].Location == place) {
            NounObject = i;
            return 0;
        }
        i++;
    } while (i <= GameHeader.NumItems);

    pop_at_loopback = 1;
    NounObject = 0;
    return 0;
}

/* Execute a system command (action opcode 110). These handle animation,
   image drawing, save/restore, and item flag manipulation — operations
   outside the standard Scott Adams action set. Returns nonzero when the
   action table must be scanned again from the top. */
static int SysCommand(int arg1, int arg2)
{
    switch (arg1) {
    case SYS_LOAD:
        SystemMessage(RESUME_A_SAVED_GAME);
        if (YesOrNo())
            LoadGame();
        break;
    case SYS_ADD_SPECIAL:
        debug_print("Add special picture %d to animate buffer\n", Counters[arg2]);
        AddSpecialImage(Counters[arg2]);
        break;
    case SYS_CLEAR_ANIM:
        debug_print("Clear animate buffer\n");
        ClearAnimationBuffer();
        break;
    case SYS_ANIMATE:
        debug_print("Animate counter %d (%d)\n", arg2, Counters[arg2]);
        Animate(Counters[arg2]);
        break;
    case SYS_SET_BACKGROUND:
        /* A deliberate deviation: the original interpreter draws the
           picture at once. Here it is kept as the background of the
           animation and drawn when that runs. */
        debug_print("Unpack special picture %d (Set animation background)\n", arg2);
        AnimationBackground = arg2;
        break;
    case SYS_BEEP_WAIT:
        debug_print("Beep and wait for key\n");
        AnyKey(1, 1);
        break;
    case SYS_DRAW_ITEM:
        debug_print("DrawItemImage %d\n", Counters[arg2]);
        DrawItemImage(Counters[arg2]);
        ShowApple2Screen();
        break;
    case SYS_DRAW_ROOM:
        debug_print("DrawRoomImage %d\n", Counters[arg2]);
        DrawRoomImage(Counters[arg2]);
        break;
    case SYS_SWAP_ITEM_FLAGS: {
        int item = ObjectArgument(arg2);
        debug_print("Swap item %d flags with counter 63\n", item);
        if (!IsValidItem(item))
            break;
        int temp = ItemRegister;
        ItemRegister = Items[item].Flag;
        Items[item].Flag = temp;
        break;
    }
    case SYS_ADD_ITEM:
        debug_print("Add item image %d to animate buffer\n", Counters[arg2]);
        AddItemImage(Counters[arg2]);
        break;
    case SYS_ADD_ROOM:
        debug_print("Add room image %d to animate buffer\n", Counters[arg2]);
        AddRoomImage(Counters[arg2]);
        break;
    case SYS_NEXT_ITEM:
        debug_print("Next item, mode %d, from item %d\n", arg2, NounObject);
        return NextItemAt(arg2);
    default:
        debug_print("Unknown system command %d\n", arg1);
        break;
    }
    return 0;
}

/* How the result of a condition is combined with those before it */
typedef enum {
    LOGIC_AND,
    LOGIC_OR,
    LOGIC_XOR
} LogicMode;

/* The arguments of condition 31, which says how the conditions that
   follow are to be combined. */
enum {
    CTL_AND = 0,
    CTL_OR = 1,
    CTL_XOR = 2,
    CTL_NOT_NEXT = 3,
    CTL_NOT_ALL = 4,
    CTL_NOT_OFF = 5,
    CTL_LEFT_PAREN = 6,
    CTL_RIGHT_PAREN = 7,
    CTL_TEST_AT_END = 8,
    CTL_FUZZY = 9,
    CTL_STRICT = 10
};

/* Combine two condition results according to the logic mode. */
static int CalculateConditionResult(int x, int y, int logic_mode)
{
    switch (logic_mode) {
    case LOGIC_OR:
        return (x || y);
    case LOGIC_XOR:
        return ((x != 0) != (y != 0));
    default:
        return (x && y);
    }
}

/* How deep parenthesised condition groups may be nested */
#define MAX_PARENS_DEPTH 5

/* Read the extra argument of a two-argument condition. It is stored as
   the argument of the (code 0) pair that follows, so the code is skipped.
   A damaged database may end the list (the 255 that SetActionConditions
   puts after it) before that pair: the argument is then 0, and cc stays
   on the end. */
static int NextConditionArgument(const uint16_t *ptr, int *cc)
{
    if (ptr[*cc] == 255) {
        debug_print("Condition argument missing at the end of the list\n");
        return 0;
    }
    (*cc)++;
    return ptr[(*cc)++];
}

/* The item an argument stands for: ARG_NOUN, ARG_NOUN2 and
   ARG_WITH_LIST name the items of the command, anything else is the
   number itself. */
static int ObjectArgument(int arg)
{
    switch (arg) {
    case ARG_NOUN:
        return NounObject;
    case ARG_NOUN2:
        return Noun2Object;
    case ARG_WITH_LIST:
        return WithListObject;
    default:
        return arg;
    }
}

/* The noun an argument stands for: ARG_NOUN, ARG_NOUN2 and
   ARG_WITH_LIST name the nouns of the command, anything else is the
   number of a noun. */
static int NounArgument(int arg)
{
    switch (arg) {
    case ARG_NOUN:
        return CurrentNoun;
    case ARG_NOUN2:
        return CurrentNoun2;
    case ARG_WITH_LIST:
        return WithListNoun();
    default:
        return arg;
    }
}

/* Whether two nouns are the same word, or with fuzzy set the same up
   to the hash sign that tells homonyms apart. */
static int NounsMatch(int noun1, int noun2, int fuzzy)
{
    if (noun1 == noun2)
        return 1;
    if (!fuzzy)
        return 0;
    return CompareUpToHashSign(Nouns[GetDictWord(noun1)].Word, Nouns[GetDictWord(noun2)].Word);
}

/* Whether item is a valid item at loc (a room, CARRIED or DESTROYED). An
   invalid item is nowhere. */
static int ItemIsAt(int item, int loc)
{
    return IsValidItem(item) && Items[item].Location == loc;
}

/* Evaluate the condition list of an action. Conditions are pairs of
   (code, argument) terminated by 255. Supports AND/OR/NOT logic,
   parenthetical grouping, fuzzy dictionary matching, and special
   argument values ARG_NOUN/ARG_NOUN2 for the current noun/noun2 objects.
   Returns ACT_SUCCESS if all conditions pass, ACT_FAILURE otherwise. */
static ActionResultType TestConditions(uint16_t *ptr)
{
    int negate = CTL_NOT_OFF;
    int logic_mode = LOGIC_AND;
    int fuzzy_match = 0;
    int last_result = 1;
    int current_result = 1;
    /* The next result replaces last_result instead of being combined
       with it: at the start, and after a left parenthesis. */
    int fresh = 1;
    /* Without this the line fails at the first false result, whatever
       comes after it. An OR sets it, so "A, OR, B" is not "A or B" but
       plain "A": the OR has to come first. */
    int test_at_end = 0;
    int group_closed;

    int parens_depth = 0;               /* Current nesting depth of parenthesised condition groups */
    int parens_stack[MAX_PARENS_DEPTH]; /* Saved intermediate results for each open parenthesis */

    int cc = 0;

    int reached_end = 0;

    int cv = 0, dv = 0, dv2, originaldv;

    while (ptr[cc] != 255 && !reached_end) {
        cv = ptr[cc++];
        dv = ptr[cc++];

        originaldv = dv;

        dv = ObjectArgument(dv);

        debug_print("Testing condition %d: ", cv);
        current_result = 1;
        group_closed = 0;

        switch (cv) {
        case COND_ARGUMENT:
            break;
        case COND_CARRIED:
            debug_print("Does the player carry item %d?\n", dv);
            current_result = ItemIsAt(dv, CARRIED);
            break;
        case COND_HERE:
            debug_print("Is item %d in location?\n", dv);
            current_result = ItemIsAt(dv, MyLoc);
            break;
        case COND_PRESENT:
            debug_print("Is item %d held or in location?\n", dv);
            current_result = ItemIsAt(dv, CARRIED) || ItemIsAt(dv, MyLoc);
            break;
        case COND_AT:
            debug_print("Is location room %d?\n", dv);
            current_result = (MyLoc == dv);
            break;
        case COND_ROOM_DESCRIPTION:
            dv2 = NextConditionArgument(ptr, &cc);
            debug_print("Is the description of the room in counter %d message %d?\n", dv, dv2);
            current_result = IsValidCounter(dv) && IsValidRoom(Counters[dv]) && Rooms[Counters[dv]].Exits[EXIT_DESCRIPTION] == dv2;
            break;
        case COND_FLAG_SET:
            debug_print("Is bitflag %d set?\n", dv);
            current_result = IsSet(dv);
            break;
        case COND_FLAG_CLEAR:
            debug_print("Is bitflag %d NOT set?\n", dv);
            current_result = !IsSet(dv);
            break;
        case COND_CARRYING_ANYTHING:
            debug_print("Does the player carry anything?\n");
            current_result = (CountItemsInRoom(dv) != 0);
            break;
        case COND_IN_PLAY:
            debug_print("Is item %d in play?\n", dv);
            current_result = IsValidItem(dv) && !ItemIsAt(dv, DESTROYED);
            break;
        case COND_NOT_IN_PLAY:
            debug_print("Is item %d NOT in play?\n", dv);
            current_result = ItemIsAt(dv, DESTROYED);
            break;
        case COND_CURRENT_LE:
            debug_print("Is CurrentCounter <= %d?\n", dv);
            current_result = (CurrentCounter <= dv);
            break;
        case COND_CURRENT_GT:
            debug_print("Is CurrentCounter > %d?\n", dv);
            current_result = (CurrentCounter > dv);
            break;
        case COND_COUNTERS_EQUAL:
            dv2 = NextConditionArgument(ptr, &cc);
            debug_print("Is Counter %d == Counter %d?\n", dv, dv2);
            current_result = IsValidCounter(dv) && IsValidCounter(dv2) && Counters[dv] == Counters[dv2];
            break;
        case COND_COUNTER_GT_COUNTER:
            dv2 = NextConditionArgument(ptr, &cc);
            debug_print("Is counter %d greater than counter %d?\n", dv, dv2);
            current_result = IsValidCounter(dv) && IsValidCounter(dv2) && Counters[dv] > Counters[dv2];
            break;
        case COND_CURRENT_EQ:
            debug_print("Is current counter == %d?\n", dv);
            current_result = (CurrentCounter == dv);
            break;
        case COND_COUNTER_EQ:
            dv2 = NextConditionArgument(ptr, &cc);
            debug_print("Is counter %d == %d?\n", dv, dv2);
            current_result = IsValidCounter(dv) && Counters[dv] == dv2;
            break;
        case COND_COUNTER_GE:
            dv2 = NextConditionArgument(ptr, &cc);
            debug_print("Is counter %d >= %d?\n", dv, dv2);
            current_result = IsValidCounter(dv) && Counters[dv] >= dv2;
            break;
        case COND_COUNTER_ZERO:
            debug_print("Is counter %d == 0?\n", dv);
            current_result = IsValidCounter(dv) && Counters[dv] == 0;
            break;
        case COND_ITEM_IN_COUNTER_ROOM:
            dv2 = NextConditionArgument(ptr, &cc);
            debug_print("Is item %d in the room of counter %d?\n", dv, dv2);
            current_result = IsValidCounter(dv2) && ItemIsAt(dv, Counters[dv2]);
            break;
        case COND_ITEM_IN_ROOM:
            /* The room may be given as the item of a noun as well */
            dv2 = ObjectArgument(NextConditionArgument(ptr, &cc));
            debug_print("Is object %d in room %d?\n", dv, dv2);
            current_result = ItemIsAt(dv, dv2);
            break;
        case COND_NOUN_IS:
            /* Both arguments are nouns, not items */
            dv = NounArgument(originaldv);
            dv2 = NounArgument(NextConditionArgument(ptr, &cc));
            debug_print("Is noun %d the noun %d?%s\n", dv, dv2, fuzzy_match ? " (Fuzzy match)" : "");
            current_result = NounsMatch(dv, dv2, fuzzy_match);
            break;
        case COND_ITEM_NOUN_IS:
            dv2 = NounArgument(NextConditionArgument(ptr, &cc));
            debug_print("Is the noun of item %d the noun %d?%s\n", dv, dv2, fuzzy_match ? " (Fuzzy match)" : "");
            current_result = IsValidItem(dv) && NounsMatch(Items[dv].Dictword, dv2, fuzzy_match);
            break;
        case COND_ITEM_FLAG:
            dv2 = NextConditionArgument(ptr, &cc);
            debug_print("Is Object flag %d & %d != 0\n", dv, dv2);
            current_result = IsValidItem(dv) && (Items[dv].Flag & dv2) != 0;
            break;
        case COND_CONTROL: /* logics control flags */
            switch (dv) {
            case CTL_OR:
                test_at_end = 1;
                /* fallthrough */
            case CTL_AND:
            case CTL_XOR:
                debug_print("Logic mode %d (0 AND, 1 OR, 2 XOR)\n", dv);
                logic_mode = dv;
                break;
            case CTL_NOT_NEXT:
            case CTL_NOT_ALL:
            case CTL_NOT_OFF:
                debug_print("Negation mode %d (3 next, 4 all, 5 off)\n", dv);
                negate = dv;
                break;
            case CTL_LEFT_PAREN:
                if (parens_depth >= MAX_PARENS_DEPTH)
                    Fatal("Too many nested parentheses in conditions");
                debug_print("Left parenthesis: set parens_stack[%d] to last result %d\n", parens_depth, last_result);
                parens_stack[parens_depth++] = last_result;
                fresh = 1;
                break;
            case CTL_RIGHT_PAREN:
                /* The result of the group is combined with what came
                   before it like the result of a single condition,
                   negation included. */
                if (parens_depth <= 0)
                    Fatal("Mismatched parentheses");
                parens_depth--;
                debug_print("Right parenthesis: getting pushed result %d from parens_stack[%d]\n", parens_stack[parens_depth], parens_depth);
                current_result = last_result;
                last_result = parens_stack[parens_depth];
                group_closed = 1;
                break;
            case CTL_TEST_AT_END:
                debug_print("Test the result at the end of the line\n");
                test_at_end = 1;
                break;
            case CTL_FUZZY:
                debug_print("Fuzzy match!\n");
                fuzzy_match = 1;
                break;
            case CTL_STRICT:
                debug_print("Strict match!\n");
                fuzzy_match = 0;
                break;
            default:
                debug_print("Unhandled logic control flag %d\n", dv);
            }
            break;
        default:
            /* Conditions 6, 7, 11, 12, 24, 25 and 27 have no handler in
               the MS-DOS interpreter of the unfinished X-Men (S.A.G.A. +
               version 225), where a condition without one leaves the
               result true. X-Men uses 27 once ("27.21", when Physco-Man
               sees the headband), and he does leave. */
            debug_print("Condition %d without a handler (arg %d): always true\n", cv, dv);
            break;
        }

        if (cv == COND_CONTROL && !group_closed)
            continue;

        if (negate != CTL_NOT_OFF) {
            debug_print("Negating condition: was %s\n", current_result ? "success, now fail" : "fail, now success");
            current_result = (current_result == 0);
            if (negate == CTL_NOT_NEXT)
                negate = CTL_NOT_OFF;
        }

        if (current_result == 1) {
            debug_print("YES\n");
        }

        if (fresh) {
            last_result = current_result;
            fresh = 0;
        } else {
            last_result = CalculateConditionResult(last_result, current_result, logic_mode);
        }

        /* This is how the MS-DOS interpreters of Spider-Man, Fantastic
           Four and X-Men do it (the three have the same code here). */
        if (!last_result && !test_at_end)
            return ACT_FAILURE;
    }

    if (last_result == 1)
        return ACT_SUCCESS;
    else
        return ACT_FAILURE;
}

/* Debug helper: print the meaning of well-known bit flag indices. */
static void PrintFlagInfo(int arg)
{
    switch (arg) {
    case DARKBIT:
        debug_print("15, darkbit\n");
        break;
    case MATCHBIT:
        debug_print("33, a command matched on vocabulary, but its conditions failed\n");
        break;
    case DRAWBIT:
        debug_print("34, room picture changed\n");
        break;
    case GRAPHICSBIT:
        debug_print("35, graphics on/off\n");
        break;
    case ALWAYSMATCH:
        debug_print("60, first action matches regardless of input\n");
        break;
    case STOPTIMEBIT:
        debug_print("63, skip automatics (stop time)\n");
        break;
    default:
        break;
    }
}

/* Copy the current parsed input words into their counter-register aliases
   (Counters 48–53) so the action table can read/modify them. */
static void SetCountersFromInput(void) {
    VerbCounter = CurrentVerb;
    NounCounter = CurrentNoun;
    AdverbCounter = CurrentAdverb;
    PartpCounter = CurrentPartp;
    PrepCounter = CurrentPrep;
    Noun2Counter = CurrentNoun2;
}

/* Drop the innermost loop from the loop stack. */
static void PopLoop(void)
{
    loops[loop_index] = 0;
    if (loop_index > 0)
        loop_index--;
}

/* Set a counter, if there is one with that number. */
static void SetCounter(int counter, int value)
{
    if (IsValidCounter(counter))
        Counters[counter] = value;
}

/* Have the search for matching actions go on after this line as if no
   action had matched yet: what commands that change the input or start
   the search over do. The line that does it also ends its own search. */
static void SearchAgain(void)
{
    found_match = 0;
    command_done = 0;
    keep_going = 1;
}

/* Execute a single action table entry: test its conditions, then run
   its command sequence. Commands include item manipulation, player
   movement, counter arithmetic, message printing, image drawing, loops,
   and system commands. Returns an ActionResultType indicating whether
   to continue scanning (ACT_CONTINUE), stop (ACT_DONE), loop back
   (ACT_LOOP/ACT_LOOP_BEGIN), or that the game ended (ACT_GAMEOVER). */
static ActionResultType PerformLine(int ct)
{
    debug_print("\nPerforming line %d: (", ct);
    if (Actions[ct].Verb) {
        debug_print("%s %s) ", Verbs[GetAnyDictWord(Actions[ct].Verb, Verbs)].Word, Nouns[GetAnyDictWord(Actions[ct].NounOrChance, Nouns)].Word);
    } else {
        debug_print("0, %d) ", Actions[ct].NounOrChance);
    }
    int continuation = 0, dead = 0, done = 0;
    int loop = 0;

    int cc = 0;

    if (TestConditions(Actions[ct].Conditions) == ACT_FAILURE) {
        return ACT_FAILURE;
    }

    command_done = 1;

#pragma mark Commands

    /* Commands */
    cc = 0;
    uint8_t *commands = Actions[ct].Commands;
    int length = Actions[ct].CommandLength;
    while (cc <= length) {
        /* Every command and operand is read the same way, so no command
           below has to step over its own operands. arg1..3 are the
           operands, and ca1..3 the values of those that are counters. */
        int cmd = NextCommandNumber(commands, &cc, length);
        const CommandInfo *info = GetCommandInfo(cmd);
        int16_t arg[MAX_OPERANDS] = { 0 }, ca[MAX_OPERANDS] = { 0 };
        for (int i = 0; info != NULL && i < MAX_OPERANDS && info->operands[i] != OPERAND_NONE; i++) {
            int value = NextCommandNumber(commands, &cc, length);
            switch (info->operands[i]) {
            case OPERAND_ITEM:
                value = ObjectArgument(value);
                break;
            case OPERAND_NOUN:
                value = NounArgument(value);
                break;
            case OPERAND_COUNTER:
                if (IsValidCounter(value))
                    ca[i] = Counters[value];
                break;
            default:
                break;
            }
            arg[i] = value;
        }
        int16_t arg1 = arg[0], arg2 = arg[1], arg3 = arg[2],
                ca1 = ca[0], ca2 = ca[1], ca3 = ca[2];

        debug_print("\nPerforming command %d: ", cmd);
        if (cmd > CMD_NOP && cmd <= CMD_LAST_SHORT_MESSAGE) {
            PrintMessage(cmd);
        } else if (cmd >= CMD_FIRST_LONG_MESSAGE) {
            PrintMessage(cmd - LONG_MESSAGE_OFFSET);
        } else
            switch (cmd) {
            case CMD_NOP:
                break;
            case CMD_GET:
                /* The limit is counter 42, which the game may change.
                   Item 0 does not count. */
                InventoryCount = 0;
                for (int i = 1; i <= GameHeader.NumItems; i++) {
                    if (Items[i].Location == CARRIED)
                        InventoryCount++;
                }
                if (InventoryCount >= CarryLimit) {
                    SystemMessage(YOURE_CARRYING_TOO_MUCH);
                    lastwasnewline = 1;
                    return ACT_SUCCESS;
                }
                InventoryCount++;
                MoveItemTo(arg1, CARRIED);
                break;
            case CMD_DROP:
                debug_print("Item %d is now in location.\n", arg1);
                MoveItemTo(arg1, MyLoc);
                break;
            case CMD_GOTO:
                debug_print("Player location is now room %d.\n", arg1);
                if (IsValidRoom(arg1)) {
                    MyLoc = arg1;
                    SetBit(DRAWBIT);
                }
                break;
            case CMD_MEANS:
                debug_print("The command is changed to ");
                PrintDictWord(arg1, Verbs);
                debug_print(" ");
                PrintDictWord(arg2, Nouns);
                debug_print(".\n");

                CurrentVerb = arg1;
                CurrentNoun = arg2;
                CurrentPartp = 0;
                CurrentPrep = 0;
                CurrentNoun2 = 0;
                CurrentAdverb = 0;
                WithListCount = 0;

                SetCountersFromInput();

                SearchAgain();
                done = 1;
                break;
            case CMD_LIGHTS_OUT:
                SetBit(DARKBIT);
                break;
            case CMD_LIGHTS_ON:
                ResetBit(DARKBIT);
                break;
            case CMD_SET_FLAG:
                debug_print("Bitflag %d is set\n", arg1);
                PrintFlagInfo(arg1);
                SetBit(arg1);
                break;
            case CMD_DESTROY:
                debug_print("Item %d is removed from play.\n", arg1);
                MoveItemTo(arg1, 0);
                break;
            case CMD_CLEAR_FLAG:
                debug_print("BitFlag %d is cleared\n", arg1);
                PrintFlagInfo(arg1);
                ResetBit(arg1);
                break;
            case CMD_DEATH:
                PlayerIsDead();
                break;
            case CMD_PUT_IN_ROOM:
                /* The room can be a two-byte one: X-Men has items put in
                   room 500 */
                PutItemAInRoomB(arg1, arg2);
                break;
            case CMD_GAME_OVER:
                debug_print("Dead\n");
                DoneIt();
                dead = 1;
                break;
            case CMD_NOUN_TO_COUNTER:
                debug_print("Counter %d == %d\n", arg2, arg1);
                SetCounter(arg2, arg1);
                break;
            case CMD_MULTIPLY:
                debug_print("Multiply: Counter %d = Counter %d (%d) * Counter %d (%d) == %d\n", arg2, arg1, ca1, arg2, ca2, ca1 * ca2);
                SetCounter(arg2, ca1 * ca2);
                break;
            case CMD_INVENTORY:
                ListInventory();
                /* Claymorgue also has an inventory screen: the list in
                   the upper window, under pictures of what is carried */
                if (CurrentGame == CLAYMORGUE)
                    UpdateClaymorgueInventory();
                break;
            case CMD_SET_FLAG0:
                debug_print("Set bitflag 0\n");
                SetBit(FLAG0BIT);
                break;
            case CMD_CLEAR_FLAG0:
                debug_print("Clear bitflag 0\n");
                ResetBit(FLAG0BIT);
                break;
            case CMD_DONE:
                debug_print("Done (stop reading lines)\n");
                done = 1;
                break;
            case CMD_CLEAR_SCREEN:
                ClearScreen();
                break;
            case CMD_SAVE:
                SaveGame();
                break;
            case CMD_SWAP_ITEMS:
                CheckForObjectImage(arg1);
                CheckForObjectImage(arg2);
                SwapItemLocations(arg1, arg2);
                break;
            case CMD_CONTINUE:
                debug_print("Continue with next line\n");
                continuation = 1;
                break;
            case CMD_TAKE:
                MoveItemTo(arg1, CARRIED);
                break;
            case CMD_PUT_WITH:
                CheckForObjectImage(arg1);
                MoveItemAToLocOfItemB(arg1, arg2);
                break;
            case CMD_LOOK:
                debug_print("LOOK\n");
                Look(1);
                break;
            case CMD_DECREMENT_CURRENT:
                /* No floor: the MS-DOS interpreters of all five games and
                   the Atari ST one decrement, and subtract in 83, without
                   one */
                CurrentCounter--;
                debug_print("Decrementing current counter. Current counter is now %d.\n",
                    CurrentCounter);
                break;
            case CMD_PRINT_CURRENT:
                OutputNumber(CurrentCounter);
                break;
            case CMD_SET_CURRENT:
                debug_print("CurrentCounter is set to %d.\n", arg1);
                CurrentCounter = arg1;
                break;
            case CMD_ITEM_ROOM_TO_COUNTER:
                debug_print("Counter %d is set to location of object %d.\n", arg2, arg1);
                if (IsValidCounter(arg2) && IsValidItem(arg1))
                    Counters[arg2] = Items[arg1].Location;
                break;
            case CMD_SWAP_CURRENT:
                SwapCounters(arg1);
                break;
            case CMD_ADD_CURRENT:
                debug_print("Add %d to current counter(%d)\n", arg1, CurrentCounter);
                CurrentCounter += arg1;
                break;
            case CMD_SUBTRACT_CURRENT:
                CurrentCounter -= arg1;
                break;
            case CMD_PRINT_NOUN:
                PrintNoun();
                break;
            case CMD_PRINT_ITEM:
                debug_print("Print name of item %d\n", arg1);
                if (IsValidItem(arg1))
                    Display(Bottom, " %s", Items[arg1].Text);
                break;
            case CMD_PRINT_ITEM_NOUN:
                debug_print("Output dictword of item (%d) + space.\n", arg1);
                if (IsValidItem(arg1))
                    Display(Bottom, "%s ", Nouns[GetDictWord(Items[arg1].Dictword)].Word);
                break;
            case CMD_PAUSE:
                debug_print("Delay\n");
                Delay(1000);
                break;
            case CMD_DRAW_ROOM:
                debug_print("Draw current room image\n");
                DrawCurrentRoom();
                ResetBit(DRAWBIT);
                break;
            case CMD_SHOW_IMAGE:
                debug_print("Draw closeup image %d\n", arg1);
                DrawCloseup(arg1);
                SetBit(DRAWBIT);
                AnyKey(0, 1);
                break;
            case CMD_DIVIDE:
                debug_print("Divide: Counter %d = Counter %d (%d) / counter %d (%d), Counter %d = Counter %d (%d) / counter %d (%d)\n", arg2, arg2, ca2, arg1, ca1, arg3, arg2, ca2, arg1, ca1);
                if (ca1 && IsValidCounter(arg2) && IsValidCounter(arg3)) {
                    Counters[arg3] = ca2 % ca1;
                    Counters[arg2] = ca2 / ca1;
                }
                break;
            case CMD_STOP_SEARCH:
                debug_print("Domore reset\n");
                keep_going = 0;
                break;
            case CMD_DELAY:
                /* The delay unit is a quarter of a second, except in
                   Claymorgue, where it is an eighth. The originals count
                   an empty loop, about 0.16 s per unit on a 4.77 MHz PC,
                   and Claymorgue waits 50 units in silence when the lamp
                   burns out, which at 250 ms looks as if the game hangs. */
                debug_print("Delay Counter %d (%d) * %d ms\n", arg1, ca1, CurrentGame == CLAYMORGUE ? 125 : 250);
                Delay(ca1 * (CurrentGame == CLAYMORGUE ? 125 : 250));
                break;
            case CMD_GET_EXIT:
                debug_print("Counter %d = exit %d from room %d\n", arg1, ca3, ca2);
                if (IsValidCounter(arg1) && IsValidRoom(ca2) && ca3 >= 0 && ca3 < NUM_EXIT_COLUMNS)
                    Counters[arg1] = Rooms[ca2].Exits[ca3];
                break;
            case CMD_SET_EXIT:
                debug_print("Exit %d of room %d is set to %d\n", ca2, ca1, ca3);
                if (IsValidRoom(ca1) && ca2 >= 0 && ca2 < NUM_EXIT_COLUMNS)
                    Rooms[ca1].Exits[ca2] = ca3;
                break;
            case CMD_SWAP_COUNTERS:
                debug_print("Swap counters %d %d \n", arg1, arg2);
                /* Draw room image after jumping off Claymorgue crate */
                if (arg1 == COUNTER_ROOM && ca2 != MyLoc)
                    SetBit(DRAWBIT);
                if (IsValidCounter(arg1) && IsValidCounter(arg2)) {
                    Counters[arg1] = ca2;
                    Counters[arg2] = ca1;
                }
                break;
            case CMD_COPY_COUNTER:
                debug_print("Counter %d = counter %d\n", arg2, arg1);
                SetCounter(arg2, ca1);
                break;
            case CMD_SET_COUNTER:
                debug_print("Counter %d is set to %d \n", arg2, arg1);
                SetCounter(arg2, arg1);
                break;
            case CMD_ADD:
                debug_print("Add %d to counter %d\n", arg1, arg2);
                if (IsValidCounter(arg2))
                    Counters[arg2] += arg1;
                break;
            case CMD_SUBTRACT:
                debug_print("Decrease counter %d by %d\n", arg2, arg1);
                if (IsValidCounter(arg2))
                    Counters[arg2] -= arg1;
                break;
            case CMD_PRINT_COUNTER:
                debug_print("Print counter %d\n", arg1);
                if (IsValidCounter(arg1))
                    OutputNumber(Counters[arg1]);
                break;
            case CMD_ABS:
                debug_print("ABS counter %d\n", arg1);
                if (IsValidCounter(arg1) && ca1 < 0)
                    Counters[arg1] = -ca1;
                break;
            case CMD_NEGATE:
                debug_print("NEG counter %d\n", arg1);
                SetCounter(arg1, -ca1);
                break;
            case CMD_AND:
                debug_print("AND counter %d with counter %d\n", arg2, arg1);
                SetCounter(arg2, ca1 & ca2);
                break;
            case CMD_OR:
                debug_print("OR counter %d with counter %d\n", arg2, arg1);
                SetCounter(arg2, ca1 | ca2);
                break;
            case CMD_XOR:
                debug_print("XOR counter %d with counter %d\n", arg2, arg1);
                SetCounter(arg2, ca1 ^ ca2);
                break;
            case CMD_RANDOM:
                /* INT(RND * 100) in the MS-DOS interpreters of all five
                   games */
                debug_print("Put random number (0 to 99) in counter %d\n", arg1);
                if (IsValidCounter(arg1))
                    Counters[arg1] = erkyrath_random() % 100;
                break;
            case CMD_CLEAR_COUNTER:
                debug_print("Clear counter %d\n", arg1);
                SetCounter(arg1, 0);
                break;
            case CMD_SYSTEM:
                debug_print("System command %d %d\n", arg1, arg2);
                if (SysCommand(arg1, arg2)) {
                    SearchAgain();
                    done = 1;
                    startover = 1;
                }
                break;
            case CMD_START_OVER:
                debug_print("Start over\n");
                SearchAgain();
                done = 1;
                startover = 1;
                break;
            case CMD_LOOP_BEGIN:
                debug_print("Start loop\n");
                loop = 1;
                break;
            case CMD_LOOP_BACK:
                /* After a 115 the loop back does not happen: the loop
                   is dropped and the rest of the line runs. */
                if (pop_at_loopback) {
                    debug_print("Loop back: leaving the loop instead\n");
                    pop_at_loopback = 0;
                    PopLoop();
                    break;
                }
                debug_print("Loop back\n");
                return ACT_LOOP;
            case CMD_LOOP_POP:
                debug_print("Remove a loop from stack\n");
                PopLoop();
                break;
            case CMD_LOOP_POP_AT_BACK:
                /* Unlike 114 this leaves the loop on the stack until
                   the next 113, so the lines up to it still run inside
                   the loop. */
                debug_print("Remove a loop from stack at next loopback\n");
                pop_at_loopback = 1;
                break;
            case CMD_SEARCH_ON:
                debug_print("Continue looking for commands\n");
                debug_print("found_match = 0, domore = 1, done = 1\n");
                SearchAgain();
                done = 1;
                break;
            case CMD_FOUND_MATCH:
                /* After a 116 this turns "I don't understand" into
                   "I can't do that yet". The original does not touch
                   flag 33 here. */
                debug_print("Set found_match to 1\n");
                found_match = 1;
                break;
            case CMD_COMMAND_DONE:
                /* After a 116 this takes back the complaint, without
                   ending the line or the search for more actions. */
                debug_print("Set command_done to 1\n");
                command_done = 1;
                break;
            case CMD_SET_PROMPT:
                /* The argument is numbered like the opcodes that print a
                   message: those from 52 on come after the 76 commands */
                if (arg1 >= 52)
                    arg1 -= 76;
                debug_print("Set command prompt to string arg %d\n", arg1);
                if (arg1 >= 0 && arg1 <= GameHeader.NumMessages)
                    ProtagonistString = arg1;
                break;
            case CMD_WORDS_TO_COUNTERS:
                debug_print("counter 48: verb, counter 49: noun, counter 50: preposition, counter 51: adverb, counter 52: participle, counter 53: noun 2\n");
                SetCountersFromInput();
                break;
            case CMD_COUNTERS_TO_WORDS:
                debug_print("Set input words to counters 48 to 53\n");
                CurrentVerb = VerbCounter;
                CurrentNoun = NounCounter;
                CurrentAdverb = AdverbCounter;
                CurrentPartp = PartpCounter;
                CurrentPrep = PrepCounter;
                CurrentNoun2 = Noun2Counter;
                SearchAgain();
                done = 1;
                break;
            case CMD_PRINT_ITEM_ARTICLE:
                debug_print("Print object %d with article\n", arg1);
                if (IsValidItem(arg1))
                    Display(Bottom, "%s%s", IndefiniteArticle(Items[arg1].Text), Items[arg1].Text);
                break;
            case CMD_ADD_COUNTER:
                debug_print("Add counter %d to counter %d.\n", arg1, arg2);
                if (IsValidCounter(arg2))
                    Counters[arg2] += ca1;
                break;
            case CMD_LOOP_RESET:
                debug_print("Reset all loops.\n");
                loop_index = 0;
                break;
            case CMD_COPY_FLAG:
                debug_print("Flag %d = Flag %d.\n", arg2, arg1);
                if (IsSet(arg1))
                    SetBit(arg2);
                else
                    ResetBit(arg2);
                break;
            case CMD_COUNTER_FROM_INDIRECT:
                debug_print("Counter %d = Counter (Counter %d (%d))\n", arg1, arg2, ca2);
                if (IsValidCounter(arg1) && IsValidCounter(ca2))
                    Counters[arg1] = Counters[ca2];
                break;
            case CMD_COUNTER_TO_INDIRECT:
                debug_print("Counter (Counter %d (%d)) = Counter %d\n", arg2, ca2, arg1);
                if (IsValidCounter(arg1) && IsValidCounter(ca2))
                    Counters[ca2] = Counters[arg1];
                break;
            default:
                debug_print("Unknown action %d\n", cmd);
                break;
            }
    }

    if (dead) {
        debug_print("PerformLine: Returning ACT_GAMEOVER\n");
        return ACT_GAMEOVER;
    } else if (loop) {
        debug_print("PerformLine: Returning ACT_LOOP_BEGIN\n");
        return ACT_LOOP_BEGIN;
    } else if (continuation) {
        debug_print("PerformLine: Returning ACT_CONTINUE\n");
        return ACT_CONTINUE;
    } else if (done) {
        debug_print("PerformLine: Returning ACT_DONE\n");
        return ACT_DONE;
    } else {
        debug_print("PerformLine: Returning ACT_SUCCESS\n");
        return ACT_SUCCESS;
    }
}

/* Check whether the player's input matches the "extra words" (adverbs,
   prepositions, participles + second nouns) attached to action entry ct.
   The Plus format stores these as typed word records alongside the main
   verb/noun, enabling commands like "CAREFULLY THROW BALL AT WALL". */
/* Does an extra word of an action match what the player typed (current)?
   0 matches anything, and 1 (NONE) matches when the player typed no word
   of this kind. */
static int WordMatches(int wordval, int current)
{
    return (wordval == current || wordval == 0 || (wordval == 1 && current == 0));
}

static int IsExtraWordMatch(int ct)
{
    /* If the action has no "extra words" the command must contain no participle */
    if (Actions[ct].NumWords == 0) {
        return (CurrentPartp == 0);
    }

    int foundprep = 0;
    int foundpartp = 0;
    int foundadverb = 0;

    int matchedprep = 0;
    int matchedpartp = 0;
    int matchedadverb = 0;

    debug_print("Action %d (%s %s) has %d extra words\n", ct, Verbs[GetAnyDictWord(Actions[ct].Verb, Verbs)].Word, Nouns[GetAnyDictWord(Actions[ct].NounOrChance, Nouns)].Word, Actions[ct].NumWords);

    for (int i = 0; i < Actions[ct].NumWords; i++) {
        uint8_t word = Actions[ct].Words[i];
        debug_print("Extra word %d: %d ", i, word);
        uint8_t wordval = word & 63;
        switch (word >> 6) {
        case 1: /* adverb */
            debug_print("(adverb \"%s\")\n", Adverbs[GetAnyDictWord(wordval, Adverbs)].Word);
            foundadverb = 1;
            if (WordMatches(wordval, CurrentAdverb)) {
                debug_print("Matched adverb %d\n", wordval);
                matchedadverb = 1;
            }
            break;
        case 2: { /* participle, followed by the noun that goes with it */
            /* (Participles are in the same dictionary as prepositions) */
            debug_print("(participle \"%s\")\n", Prepositions[GetAnyDictWord(wordval, Prepositions)].Word);
            foundpartp = 1;
            i++;
            /* The noun is missing if the participle is the last word */
            uint8_t n2 = (i < Actions[ct].NumWords) ? Actions[ct].Words[i] : 0;
            debug_print("(object (word %d) \"%s\")\n", i, Nouns[GetAnyDictWord(n2, Nouns)].Word);
            if (CurrentPartp == 0 && wordval > 1 && !IsNextParticiple(wordval, n2)) {
                break;
            }
            if (WordMatches(wordval, CurrentPartp)) {
                debug_print("Matched participle %d\n", wordval);
                if (n2 == CurrentNoun2 || n2 == 0) {
                    debug_print("Matched object %d (%s)\n", n2, Nouns[GetDictWord(n2)].Word);
                    matchedpartp = 1;
                }
            }
            break;
        }
        case 3: /* preposition */
            debug_print("(preposition \"%s\")\n", Prepositions[GetAnyDictWord(wordval, Prepositions)].Word);
            foundprep = 1;
            if (WordMatches(wordval, CurrentPrep)) {
                debug_print("Matched preposition %d\n", wordval);
                matchedprep = 1;
            }
            break;
        default:
            debug_print("(unknown type)\n");
            debug_print("word >> 6: %d\n", word >> 6);
            break;
        }
    }

    /* A participle in the command has to be asked for by the action, as
       in every MS-DOS interpreter and the Apple II one of Spider-Man. */
    if (!foundpartp && CurrentPartp != 0) {
        debug_print("IsExtraWordMatch failed: the command has a participle, the action none\n");
        return 0;
    }

    if ((foundprep && !matchedprep) || (foundpartp && !matchedpartp) || (foundadverb && !matchedadverb)) {
        debug_print("IsExtraWordMatch failed\n");
        return 0;
    }

    return 1;
}

/* Check if an implicit (verb=0) action should fire. These are triggered
   by a random percentage chance (NounOrChance), or unconditionally if
   doagain is set (chained from a previous action via ACT_CONTINUE). */
static int IsImplicitMatch(int ct, int doagain)
{
    if (RandomPercent(Actions[ct].NounOrChance)) {
        return 1;
    }

    if (doagain)
        return 1;

    return 0;
}

/* Check if an explicit (verb!=0) action matches the current input.
   Matches when the verb matches and either the noun matches or is
   a wildcard (0). Also checks extra words (adverbs, prepositions).
   When doagain is set, verb-0 continuation entries also match. */
static int IsMatch(int ct, int doagain)
{
    int verbvalue = Actions[ct].Verb;
    int nounvalue = Actions[ct].NounOrChance;

    if (verbvalue != CurrentVerb && !(doagain && verbvalue == 0))
        return 0;

    if (verbvalue == 0) {
        if (RandomPercent(nounvalue)) {
            return 1;
        } else if (nounvalue != 0) {
            return 0;
        }
    }

    if (!(doagain && verbvalue == 0) && !IsExtraWordMatch(ct)) {
        return 0;
    }

    if (verbvalue == CurrentVerb && verbvalue != 0 && (nounvalue == CurrentNoun || nounvalue == 0)) {
        found_match = 1;
        return 1;
    }

    if (doagain)
        return 1;

    return 0;
}

/* Update the action table scan after line *ct returned actresult:
   start or stop a chain of continuation lines, and push, pop or jump
   back to a loop. Returns 1 if the game is over and the scan must stop. */
static int ApplyActionResult(ActionResultType actresult, int *ct, int *chain_on)
{
    switch (actresult) {
    case ACT_CONTINUE:
        *chain_on = 1;
        break;
    case ACT_DONE:
        *chain_on = 0;
        PopLoop();
        break;
    case ACT_LOOP_BEGIN:
        loop_index++;
        if (loop_index >= MAX_LOOPS)
            Fatal("Loop stack overflow");
        loops[loop_index] = *ct;
        *chain_on = 1;
        break;
    case ACT_LOOP:
        if (loop_index) {
            *ct = loops[loop_index];
        }
        break;
    case ACT_GAMEOVER:
        return 1;
    default:
        break;
    }
    return 0;
}

/* Count a line looked at by a scan of the action table, and give up on
   a table that loops for ever. */
static void CountScannedLine(int *lines)
{
    if (++*lines > MAX_SCAN_LINES)
        Fatal("Endless loop in the action table");
}

/* Run all implicit (automatic) actions — those with verb=0 at the start
   of the action table. These fire each turn before the player is prompted,
   handling timed events, NPC behaviour, and game-state updates. Supports
   chaining (ACT_CONTINUE), loops (ACT_LOOP), and restart (startover).
   Stops early if STOPTIMEBIT is set and no chain is active. */
static CommandResultType PerformImplicit(void)
{
    int ct = 0;
    CommandResultType result = ER_RAN_ALL_LINES_NO_MATCH;
    int chain_on = 0;
    int lines = 0;
    startover = 0;
    loop_index = 0;
    pop_at_loopback = 0;

    while (ct <= GameHeader.NumActions) {
        CountScannedLine(&lines);
        int verbvalue = Actions[ct].Verb;

        if (verbvalue != 0)
            break;

        if (IsImplicitMatch(ct, chain_on)) {
            ActionResultType actresult = PerformLine(ct);

            if (actresult != ACT_FAILURE) {
                result = ER_SUCCESS;
                if (ApplyActionResult(actresult, &ct, &chain_on))
                    return ER_SUCCESS;
            }
        }

        ct++;

        if (startover) {
            ct = 0;
            startover = 0;
        }

        if (ct <= GameHeader.NumActions && (Actions[ct].Verb + Actions[ct].NounOrChance != 0))
            chain_on = 0;

        if (IsSet(STOPTIMEBIT) && chain_on == 0) {
            ResetBit(STOPTIMEBIT);
            return result;
        }
    }

    return result;
}

/* Run the explicit (player-input) action table. Scans for actions whose
   verb/noun match the current input, tests their conditions, and executes
   commands. Returns ER_RAN_ALL_LINES_NO_MATCH when no action matched the
   words ("I don't understand") and ER_RAN_ALL_LINES when one matched but
   none ran ("I can't do that yet"). Sets MATCHBIT, which the games can
   test, when vocabulary matches but conditions fail.
   Supports the same chaining, loops, and restart mechanisms as PerformImplicit. */
static CommandResultType PerformExplicit(void)
{
    int ct = 0;
    int chain_on = 0;
    keep_going = 1;
    startover = 0;
    loop_index = 0;
    pop_at_loopback = 0;
    found_match = 0;
    command_done = 0;
    ResetBit(MATCHBIT);
    NounObject = MatchUpItem(CurrentNoun, -1);
    Noun2Object = MatchUpItem(CurrentNoun2, -1);

    int first_explicit = 0;
    while (first_explicit <= GameHeader.NumActions && Actions[first_explicit].Verb == 0)
        first_explicit++;

    int lines = 0;
    while (ct <= GameHeader.NumActions) {
        CountScannedLine(&lines);
        int verbvalue, nounvalue;
        verbvalue = Actions[ct].Verb;
        nounvalue = Actions[ct].NounOrChance;

        if (chain_on && verbvalue != 0) {
            chain_on = 0;
        }

        /* While ALWAYSMATCH is set, the first explicit action runs
           whatever the player typed. X-Men hangs its handling of commands
           given to other characters on this. The interpreters of the two
           earliest games do not have it, and Buckaroo Banzai uses the
           flag as an ordinary one (it stays set after GET). */
        int always = (CurrentGame != SPIDERMAN && CurrentGame != BANZAI && ct == first_explicit && IsSet(ALWAYSMATCH));
        if (always)
            found_match = 1;

        if (always || IsMatch(ct, chain_on)) {
            ActionResultType actresult = PerformLine(ct);

            if (actresult != ACT_FAILURE) {
                if (actresult != ACT_DONE)
                    ResetBit(MATCHBIT);
                if (found_match)
                    keep_going = 0;

                if (ApplyActionResult(actresult, &ct, &chain_on))
                    return ER_SUCCESS;
            } else {
                if (verbvalue == CurrentVerb && (nounvalue == CurrentNoun || nounvalue == 0))
                    SetBit(MATCHBIT);
            }
        }

        ct++;

        if (startover) {
            ct = 0;
            startover = 0;
        }

        if (ct <= GameHeader.NumActions && (Actions[ct].Verb != 0)) {
            chain_on = 0;
        }

        if (found_match && !chain_on && !keep_going)
            break;
    }

    /* As in the original: no action matched the words (or the last one
       that did took it back with 116), or one matched but none ran */
    if (!found_match)
        return ER_RAN_ALL_LINES_NO_MATCH;
    if (!command_done)
        return ER_RAN_ALL_LINES;
    return ER_SUCCESS;
}

/* Spatterlight has preferences for these options (UpdateSettings), which
   override them; the ones in the #ifndef block are left out there.
   Elsewhere (in Gargoyle, through a "terp plus -a" line in garglk.ini)
   they are set here. */
glkunix_argumentlist_t glkunix_arguments[] = {
    { "-n", glkunix_arg_NoValue, "-n        No delays" },
#ifndef SPATTERLIGHT
    { "-a", glkunix_arg_NoValue,
        "-a        Always list what you carry in the upper window" },
#endif
    { "", glkunix_arg_ValueFollows, "filename    file to load" },
    { NULL, glkunix_arg_End, NULL }
};

/* Glk startup hook: read the options, extract the game file path from
   command-line arguments and split it into DirPath + filename for image
   file lookups. */
int glkunix_startup_code(glkunix_startup_t *data)
{
    int argc = data->argc;
    char **argv = data->argv;

    if (argc < 1)
        return 0;

    while (argc > 1 && argv[1][0] == '-') {
        switch (argv[1][1]) {
        case 'n':
            Options |= NO_DELAYS;
            break;
#ifndef SPATTERLIGHT
        case 'a':
            Options = (Options | FORCE_INVENTORY) & ~FORCE_INVENTORY_OFF;
            break;
#endif
        }
        argv++;
        argc--;
    }

#ifdef GARGLK
    garglk_set_program_name("Plus 1.1");
    garglk_set_program_info("Plus 1.1 by Petter Sjölund");
#endif

    if (argc == 2) {
        game_file = argv[1];

        const char *s;
        if ((s = strrchr(game_file, '/')) != NULL || (s = strrchr(game_file, '\\')) != NULL) {
            DirPathLength = (int)(s - game_file + 1);
#ifdef GARGLK
            garglk_set_story_name(s + 1);
        } else {
            garglk_set_story_name(game_file);
#endif
        }
        if (DirPathLength) {
            DirPath = MemAlloc(DirPathLength + 1);
            memcpy(DirPath, game_file, DirPathLength);
            DirPath[DirPathLength] = 0;
            debug_print("Directory path: \"%s\"\n", DirPath);
        }
    }

    return 1;
}

/* Recalculate scaling and offsets for the title screen image, centring
   it both horizontally and vertically (⅓ from top). */
static void ResizeTitleImage(void)
{
    glui32 graphheight, optimal_height;
#ifdef SPATTERLIGHT
    glk_window_set_background_color(Graphics, gbgcol);
    glk_window_clear(Graphics);
#endif
    FitImageToGraphicsWindow(&graphheight, &optimal_height);
    y_offset = ((int)graphheight - (int)optimal_height) / 3;
}

/* Draw the title picture, "S000". Returns 0 if the game has none. */
static int DrawTitle(void)
{
    if (!DrawImageWithName("S000"))
        return 0;
    ShowApple2Screen();
    return 1;
}

/* Display the title screen (image "S000") in a full-window graphics view.
   Waits for a keypress to proceed, handling resize and colour-cycling
   events while waiting. Closes all windows and reinitialises the normal
   three-window layout when done. */
static void DrawTitleImage(void)
{
    DisplayInit();
    if (!GraphicsAvailable() || Graphics == NULL)
        return;
    glk_window_close(Top, NULL);
    Top = NULL;
    glk_window_close(Bottom, NULL);
    Bottom = NULL;

    ResizeTitleImage();

    if (glk_gestalt_ext(gestalt_GraphicsCharInput, 0, NULL, 0)) {
        glk_request_char_event(Graphics);
    } else {
        Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Fixed,
            2, wintype_TextBuffer, GLK_BUFFER_ROCK);
        glk_request_char_event(Bottom);
    }

    if (DrawTitle()) {
        event_t ev;
        do {
            glk_select(&ev);
            if (ev.type == evtype_Arrange) {
                if (!GraphicsAvailable())
                    break;
                ResizeTitleImage();
                glk_window_clear(Graphics);
                DrawTitle();
            } else if (ev.type == evtype_Timer) {
                if (ColorCyclingRunning)
                    UpdateColorCycling();
            }
        } while (ev.type != evtype_CharInput);
    }

    if (Bottom != NULL)
        glk_window_close(Bottom, NULL);
    glk_window_close(Graphics, NULL);
    Graphics = NULL;
    DisplayInit();
}

/* Loads the game: the plain text database of MS-DOS first, then a disk
   image or file of one of the other systems, followed by the picture list
   of that system that needs the database to be built. */
static void LoadGameFile(void)
{
    FILE *f = fopen(game_file, "rb");
    if (f == NULL)
        Fatal("Cannot open game");

    size_t filesize = GetFileLength(f);

    if (filesize == 0) {
        fclose(f);
        Fatal("Game file empty");
    }

    if (LoadDatabasePlaintext(f) != UNKNOWN_GAME)
        return;

    fseek(f, 0, SEEK_SET);
    mem = MemAlloc(filesize);
    memlen = fread(mem, 1, filesize, f);
    fclose(f);

    /* Each finds the database in its kind of file and points mem at it */
    static int (*const detectors[])(uint8_t **, size_t *) = {
        DetectAtari8, DetectST, DetectApple2, DetectC64
    };
    size_t i;
    for (i = 0; i < ARRAY_SIZE(detectors); i++)
        if (detectors[i](&mem, &memlen))
            break;
    if (i == ARRAY_SIZE(detectors))
        Fatal("Could not detect game type");

    if (!LoadDatabaseBinary())
        Fatal("Could not load binary database");

    if (CurrentSys == SYS_ATARI8)
        LoadAtari8Images();

    if (CurrentSys == SYS_APPLE2)
        LookForApple2Images();

    if (CurrentSys == SYS_C64 && CurrentGame == SPIDERMAN)
        PatchSpidermanImages();
}

/* Main entry point. Loads the game database (trying plaintext first, then
   binary formats for Atari 8-bit, Atari ST, Apple II, and C64), shows the
   title screen, and enters the main game loop:
     1. Run implicit (automatic) actions
     2. Display the room
     3. Get player input
     4. Run explicit (player-command) actions
     5. Report errors if no action matched */
void glk_main(void)
{
    if (game_file == NULL)
        Fatal("No game file");

    /* Initialize system messages, preferring first-person ("I am") forms */
    for (int i = 0; i < MAX_SYSMESS; i++) {
        sys[i] = sysdict[i];
        if (sysdict_i_am[i])
            sys[i] = sysdict_i_am[i];
    }

    LoadGameFile();

    /* A game without pictures still has a list of them, an empty one,
       so that the drawing code need not check */
    if (Images == NULL)
        Images = NewImageList(0);

#ifdef SPATTERLIGHT
    UpdateSettings();
    if (gli_determinism)
        set_erkyrath_random(1234);
    else
#endif
    set_erkyrath_random(0);

    DrawTitleImage();
    UpdateSettings();

    InitAnimationBuffer();
    InitialState = SaveCurrentState();

    /* --- Main game loop --- */
    while (1) {
        glk_tick();

        if (should_restart)
            RestartGame();

        /* Run automatic actions unless time is stopped or we just restored */
        if (IsSet(STOPTIMEBIT) || JustRestored) {
            ResetBit(STOPTIMEBIT);
        } else {
            PerformImplicit();
            if (!should_restart)
                SaveUndo();
        }

        Look(0);

        if (should_restart)
            continue;

        /* Restore a saved object image overlay (e.g. Thing in Fantastic Four) */
        if (JustRestored && RestoredImgType == IMG_OBJECT) {
            DrawItemImage(RestoredImgIndex);
        }

        JustRestored = 0;
        JustRestarted = 0;

        if (GetInput() == 1)
            continue;

        SetCountersFromInput();

        /* Save current input as "last" for AGAIN/repeat commands */
        LastVerb = CurrentVerb;
        LastNoun = CurrentNoun;
        LastPrep = CurrentPrep;
        LastPartp = CurrentPartp;
        LastNoun2 = CurrentNoun2;
        LastAdverb = CurrentAdverb;

        ClearFrames();

        CommandResultType result = PerformExplicit();

        /* If no action fully matched, report the appropriate error */
        if (result != ER_SUCCESS) {
            if (result == ER_RAN_ALL_LINES_NO_MATCH)
                SystemMessage(I_DONT_UNDERSTAND);
            else
                SystemMessage(YOU_CANT_DO_THAT_YET);
            StopProcessingCommand();
        }

        JustStarted = 0;
    }
}
