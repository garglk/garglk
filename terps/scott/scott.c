/*
 *	  ScottFree Revision 1.14
 *
 *
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 *
 *
 *	  You must have an ANSI C compiler to build this program.
 */

/*
 * Parts of this source file (mainly the Glk parts) are copyright 2011
 * Chris Spiegel.
 *
 * Some notes about the Glk version:
 *
 * o Room descriptions, exits, and visible items can, as in the
 *	 original, be placed in a window at the top of the screen, or can be
 *	 inline with user input in the main window.	 The former is default,
 *	 and the latter can be selected with the -w flag.
 *
 * o Game saving and loading uses Glk, which means that providing a
 *	 save game file on the command-line will no longer work.  Instead,
 *	 the verb "restore" has been special-cased to call GameLoad(), which
 *	 now prompts for a filename via Glk.
 *
 * o The local character set is expected to be compatible with ASCII, at
 *	 least in the printable character range.  Newlines are specially
 *	 handled, however, and converted to Glk's expected newline
 *	 character.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "glk.h"
#include "glkstart.h"
#include "saga.h"
#include "sagagraphics.h"
#include "titleimage.h"

#include "detectgame.h"
#include "layouttext.h"
#include "line_drawing.h"
#include "restorestate.h"
#include "sagadraw.h"

#include "parser.h"
#include "ti99_4a_terp.h"

#include "apple2draw.h"
#include "game_specific.h"
#include "gremlins.h"
#include "hulk.h"
#include "robinofsherwood.h"
#include "seasofblood.h"

#include "bsd.h"
#include "scott.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

const char *game_file = NULL;
char *DirPath = ".";

Header GameHeader;
Item *Items;
Room *Rooms;
const char **Verbs;
const char **Nouns;
const char **Messages;
Action *Actions;
int LightRefill;
int Counters[16] = { 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0 }; /* Range unknown */
int CurrentCounter;
int SavedRoom;
int RoomSaved[16] = { 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0 }; /* Range unknown */

long BitFlags = 0; /* Might be >32 flags - I haven't seen >32 yet */

int AutoInventory = 0;
int Options; /* Option flags set */
glui32 TopWidth; /* Terminal width */
glui32 TopHeight; /* Height of top window */
int ImageWidth = 255;
int ImageHeight = 96;
int file_baseline_offset = 0;
const char *title_screen = NULL;

struct Command *CurrentCommand = NULL;
struct GameInfo *Game;
MachineType CurrentSys = SYS_UNKNOWN;

extern const char *sysdict[MAX_SYSMESS];
extern const char *sysdict_i_am[MAX_SYSMESS];

const char *sys[MAX_SYSMESS];
const char *system_messages[60];

const char *battle_messages[33];
uint8_t enemy_table[126];

uint8_t *entire_file;
size_t file_length;

int AnimationFlag = 0;

int showing_inventory = 0;
int showing_closeup = 0;
int last_image_index = -1;
int lastwasnewline = 0;

extern struct SavedState *InitialState;

/* JustStarted is only used for the error message "Can't undo on first move" */
int JustStarted = 1;
static int should_restart = 0;
int StopTime = 0;

int should_look_in_transcript = 0;
static int print_look_to_transcript = 0;
static int pause_next_room_description = 0;

int split_screen = 1;
winid_t Bottom, Top;
winid_t Graphics;

strid_t Transcript = NULL;

int WeAreBigEndian = 0;

#define TRS80_LINE \
    "\n<------------------------------------------------------------>\n"

//#define DEBUG_ACTIONS

static void RestartGame(void);
static int YesOrNo(void);

static int PerformActions(int vb, int no);

void Display(winid_t w, const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    int oldlastwasnewline = lastwasnewline;
    glui32 *unistring = ToUnicode(msg);
    if (w != Bottom)
        lastwasnewline = oldlastwasnewline;
    glk_put_string_stream_uni(glk_window_get_stream(w), unistring);
    if (Transcript && w == Bottom)
        glk_put_string_stream_uni(Transcript, unistring);
    free(unistring);
}

void UpdateSettings(void)
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

    switch (gli_sa_palette) {
    case 0:
        Options &= ~(FORCE_PALETTE_ZX | FORCE_PALETTE_C64);
        break;
    case 1:
        Options = (Options | FORCE_PALETTE_ZX) & ~FORCE_PALETTE_C64;
        break;
    case 2:
        Options = (Options | FORCE_PALETTE_C64) & ~FORCE_PALETTE_ZX;
        break;
    }
#endif

    if (DrawingVector())
        glk_request_timer_events(20);

    palette_type previous_pal = palchosen;
    if (Options & FORCE_PALETTE_ZX)
        palchosen = ZXOPT;
    else if (Options & FORCE_PALETTE_C64) {
        if (Game->picture_format_version == 99)
            palchosen = C64A;
        else
            palchosen = C64B;
    } else
        palchosen = Game->palette;
    if (palchosen != previous_pal) {
        DefinePalette();
        if (VectorState != NO_VECTOR_IMAGE)
            DrawSomeVectorPixels(1);
    }
}

static void PrintWindowDelimiter(void)
{
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    glk_window_move_cursor(Top, 0, TopHeight - 1);
    glk_stream_set_current(glk_window_get_stream(Top));
    if (Options & SPECTRUM_STYLE)
        for (int i = 0; i < TopWidth; i++)
            glk_put_char('*');
    else {
        glk_put_char('<');
        for (int i = 0; i < TopWidth - 2; i++)
            glk_put_char('-');
        glk_put_char('>');
    }
}

static strid_t room_description_stream = NULL;

static void FlushRoomDescription(char *buf)
{
    glk_stream_close(room_description_stream, 0);

    if (Transcript && print_look_to_transcript) {
        glui32 *unistring = ToUnicode(buf);
        glk_put_string_stream_uni(Transcript, unistring);
        free(unistring);
    }

    int print_delimiter = (Options & (TRS80_STYLE | SPECTRUM_STYLE | TI994A_STYLE));

    if (split_screen) {
        glk_window_clear(Top);
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        int rows, length;
        char *text_with_breaks = LineBreakText(buf, TopWidth, &rows, &length);

        glui32 bottomheight;
        glk_window_get_size(Bottom, NULL, &bottomheight);
        winid_t o2 = glk_window_get_parent(Top);
        if (!(bottomheight < 3 && TopHeight < rows)) {
            glk_window_get_size(Top, &TopWidth, &TopHeight);
            glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed, rows,
                Top);
        } else {
            print_delimiter = 0;
        }

        int line = 0;
        int index = 0;
        int i;
        char string[2048];
        if (TopWidth > 2047)
            TopWidth = 2047;
        for (line = 0; line < rows && index < length; line++) {
            for (i = 0; i < TopWidth; i++) {
                string[i] = text_with_breaks[index++];
                if (string[i] == 10 || string[i] == 13 || index >= length)
                    break;
            }
            if (i < TopWidth + 1) {
                string[i++] = '\n';
            }
            string[i] = 0;
            if (strlen(string) == 0)
                break;
            glk_window_move_cursor(Top, 0, line);
            Display(Top, "%s", string);
        }

        if (line < rows - 1) {
            glk_window_get_size(Top, &TopWidth, &TopHeight);
            glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed,
                MIN(rows - 1, TopHeight - 1), Top);
        }

        free(text_with_breaks);
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

    if (buf != NULL) {
        free(buf);
        buf = NULL;
    }
}

static void UpdateUSInventory(void)
{
    char *buf = MemAlloc(1000);
    buf = memset(buf, 0, 1000);
    room_description_stream = glk_stream_open_memory(buf, 1000, filemode_Write, 0);
    ListInventory(1);
    FlushRoomDescription(buf);
    InventoryUS();
}

void Updates(event_t ev)
{
    if (ev.type == evtype_Arrange) {
        UpdateSettings();

        VectorState = NO_VECTOR_IMAGE;

        CloseGraphicsWindow();
        OpenGraphicsWindow();

        if (split_screen) {
            if (showing_inventory == 1) {
                UpdateUSInventory();
            } else {
                int lastimg = last_image_index;
                Look();
                last_image_index = lastimg;
                if (showing_closeup) {
                    if (Game->type == US_VARIANT)
                        DrawUSRoom(last_image_index);
                    else
                        DrawImage(last_image_index);
                }
            }
        }
    } else if (ev.type == evtype_Timer) {
        switch (Game->type) {
        case SHERWOOD_VARIANT:
            UpdateRobinOfSherwoodAnimations();
            break;
        case GREMLINS_VARIANT:
            UpdateGremlinsAnimations();
            break;
        case SECRET_MISSION_VARIANT:
            UpdateSecretAnimations();
            break;
        default:
            if (Game->picture_format_version == 99 && DrawingVector())
                DrawSomeVectorPixels((VectorState == NO_VECTOR_IMAGE));
            break;
        }
    }
}

void Delay(float seconds)
{
    if (Options & NO_DELAYS)
        return;

    event_t ev;

    if (!glk_gestalt(gestalt_Timer, 0))
        return;

    glk_request_char_event(Bottom);
    glk_cancel_char_event(Bottom);

    if (DrawingVector()) {
        do {
            glk_select(&ev);
            Updates(ev);
        } while (DrawingVector());
#ifdef SPATTERLIGHT
        if (gli_slowdraw)
            seconds = 0.5;
#endif
    }

    glk_request_timer_events(1000 * seconds);

    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_Timer);

    glk_request_timer_events(0);
}

winid_t FindGlkWindowWithRock(glui32 rock)
{
    winid_t win;
    glui32 rockptr;
    for (win = glk_window_iterate(NULL, &rockptr); win;
         win = glk_window_iterate(win, &rockptr)) {
        if (rockptr == rock)
            return win;
    }
    return 0;
}

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

glui32 OptimalPictureSize(glui32 *width, glui32 *height)
{
    *width = ImageWidth;
    *height = ImageHeight;
    int multiplier = 1;
    glui32 graphwidth, graphheight;
    glk_window_get_size(Graphics, &graphwidth, &graphheight);
    multiplier = graphheight / ImageHeight;
    if (255 * multiplier > graphwidth)
        multiplier = graphwidth / ImageWidth;

    if (multiplier == 0)
        multiplier = 1;

    *width = ImageWidth * multiplier;
    *height = ImageHeight * multiplier;

    return multiplier;
}

void OpenGraphicsWindow(void)
{
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    glui32 graphwidth, graphheight, optimal_width, optimal_height;

    if (Top == NULL)
        Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics == NULL && Top != NULL) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_close(Top, NULL);
        Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional,
            60, wintype_Graphics, GLK_GRAPHICS_ROCK);
        glk_window_get_size(Graphics, &graphwidth, &graphheight);
        pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
        x_offset = ((int)graphwidth - (int)optimal_width) / 2;

        if (graphheight > optimal_height) {
            winid_t parent = glk_window_get_parent(Graphics);
            if (parent)
                glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                    optimal_height, NULL);
        }

        /* Set the graphics window background to match
     * the main window background, best as we can,
     * and clear the window.
     */
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
            Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional, 60,
                wintype_Graphics, GLK_GRAPHICS_ROCK);
        glk_window_get_size(Graphics, &graphwidth, &graphheight);
        pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
        x_offset = (graphwidth - optimal_width) / 2;
        winid_t parent = glk_window_get_parent(Graphics);
        if (parent)
            glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                optimal_height, NULL);
    }
    right_margin = optimal_width + x_offset;
}

void CloseGraphicsWindow(void)
{
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics) {
        glk_window_close(Graphics, NULL);
        Graphics = NULL;
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    }
}

static void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    if (DrawingVector()) {
#ifdef SPATTERLIGHT
        gli_slowdraw = 0;
#endif
        DrawSomeVectorPixels(0);
    }
    glk_exit();
}

void Fatal(const char *x)
{
    Display(Bottom, "%s\n", x);
    CleanupAndExit();
}

static void ClearScreen(void)
{
    glk_window_clear(Bottom);
}

void *MemAlloc(int size)
{
    void *t = (void *)malloc(size);
    if (t == NULL)
        Fatal("Out of memory");
    return (t);
}

int RandomPercent(int n)
{
    uint64_t rv = (uint64_t)rand() << 6;
    rv &= 0xffffffff;
    rv %= 100;
    if (rv < n)
        return (1);
    return (0);
}

int CountCarried(void)
{
    int ct = 0;
    int n = 0;
    while (ct <= GameHeader.NumItems) {
        if (Items[ct].Location == CARRIED)
            n++;
        ct++;
    }
    return (n);
}

const char *MapSynonym(int noun)
{
    int n = 1;
    const char *tp;
    static char lastword[16]; /* Last non synonym */
    while (n <= GameHeader.NumWords) {
        tp = Nouns[n];
        if (*tp == '*')
            tp++;
        else
            strncpy(lastword, tp, GameHeader.WordLength + 1);
        if (n == noun)
            return (lastword);
        n++;
    }
    return (NULL);
}

static int MatchUpItem(int noun, int loc)
{
    const char *word = MapSynonym(noun);
    int ct = 0;

    if (word == NULL)
        word = Nouns[noun];

    while (ct <= GameHeader.NumItems) {
        if (Items[ct].AutoGet && (loc == 0 || Items[ct].Location == loc) && xstrncasecmp(Items[ct].AutoGet, word, GameHeader.WordLength) == 0)
            return (ct);
        ct++;
    }
    return (-1);
}

static char *ReadString(FILE *f)
{
    char tmp[1024];
    char *t;
    int c, nc;
    int ct = 0;
    do {
        c = fgetc(f);
    } while (c != EOF && isspace((unsigned char)c));
    if (c != '"') {
        Fatal("Initial quote expected");
    }
    do {
        c = fgetc(f);
        if (c == EOF)
            Fatal("EOF in string");
        if (c == '"') {
            nc = fgetc(f);
            if (nc != '"') {
                ungetc(nc, f);
                break;
            }
        }
        if (c == '`')
            c = '"'; /* pdd */

        /* Ensure a valid Glk newline is sent. */
        if (c == '\n')
            tmp[ct++] = 10;
        /* Special case: assume CR is part of CRLF in a
		 * DOS-formatted file, and ignore it.
		 */
        else if (c == 13)
            ;
        /* Pass only ASCII to Glk; the other reasonable option
		 * would be to pass Latin-1, but it's probably safe to
		 * assume that Scott Adams games are ASCII only.
		 */
        else if ((c >= 32 && c <= 126))
            tmp[ct++] = c;
        else
            tmp[ct++] = '?';
    } while (1);
    tmp[ct] = 0;
    t = MemAlloc(ct + 1);
    memcpy(t, tmp, ct + 1);
    return (t);
}

size_t GetFileLength(FILE *in)
{
    if (fseek(in, 0, SEEK_END) == -1) {
        return 0;
    }
    size_t length = ftell(in);
    if (length == -1) {
        return 0;
    }
    fseek(in, SEEK_SET, 0);
    return length;
}

int header[24];

//static int SanityCheckScottFreeHeader(int ni, int na, int nw, int nr, int mc)
//{
//    int16_t v = header[1]; // items
//    if (v < 10 || v > 500)
//        return 0;
//    v = header[2]; // actions
//    if (v < 100 || v > 500)
//        return 0;
//    v = header[3]; // word pairs
//    if (v < 50 || v > 200)
//        return 0;
//    v = header[4]; // Number of rooms
//    if (v < 10 || v > 100)
//        return 0;
//    v = header[5]; // Number of Messages
//    if (v < 10 || v > 255)
//        return 0;
//
//    return 1;
//}

void FreeDatabase(void)
{
    free(Items);
    free(Actions);
    free(Verbs);
    free(Nouns);
    free(Rooms);
    free(Messages);
}

GameIDType LoadDatabase(FILE *f, int loud)
{
    int ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm;
    int ct;
    short lo;
    Action *ap;
    Room *rp;
    Item *ip;
    /* Load the header */

    loud = 1;

    if (fscanf(f, "%*d %d %d %d %d %d %d %d %d %d %d %d", &ni, &na, &nw, &nr, &mc,
            &pr, &tr, &wl, &lt, &mn, &trm)
        < 10) {
        if (loud)
            debug_print("Invalid database(bad header)\n");
        return UNKNOWN_GAME;
    }
    GameHeader.NumItems = ni;
    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    GameHeader.NumActions = na;
    Actions = (Action *)MemAlloc(sizeof(Action) * (na + 1));
    GameHeader.NumWords = nw;
    GameHeader.WordLength = wl;
    Verbs = MemAlloc(sizeof(char *) * (nw + 2));
    Nouns = MemAlloc(sizeof(char *) * (nw + 2));
    GameHeader.NumRooms = nr;
    Rooms = (Room *)MemAlloc(sizeof(Room) * (nr + 1));
    GameHeader.MaxCarry = mc;
    GameHeader.PlayerRoom = pr;
    GameHeader.Treasures = tr;
    GameHeader.LightTime = lt;
    LightRefill = lt;
    GameHeader.NumMessages = mn;
    Messages = MemAlloc(sizeof(char *) * (mn + 1));
    GameHeader.TreasureRoom = trm;

    if (loud) {
        debug_print("Number of items: %d\n", GameHeader.NumItems);
        debug_print("Number of actions: %d\n", GameHeader.NumActions);
        debug_print("Number of words: %d\n", GameHeader.NumWords);
        debug_print("Word length: %d\n", GameHeader.WordLength);
        debug_print("Number of rooms: %d\n", GameHeader.NumRooms);
        debug_print("Number of messages: %d\n", GameHeader.NumMessages);
        debug_print("Max carried: %d\n", GameHeader.MaxCarry);
        debug_print("Starting location: %d\n", GameHeader.PlayerRoom);
        debug_print("Light time: %d\n", GameHeader.LightTime);
        debug_print("Number of treasures: %d\n", GameHeader.Treasures);
        debug_print("Treasure room: %d\n", GameHeader.TreasureRoom);
    }

    /* Load the actions */

    ct = 0;
    ap = Actions;
    if (loud)
        debug_print("Reading %d actions.\n", na);
    while (ct < na + 1) {
        if (fscanf(f, "%hu %hu %hu %hu %hu %hu %hu %hu",
                &ap->Vocab,
                &ap->Condition[0],
                &ap->Condition[1],
                &ap->Condition[2],
                &ap->Condition[3],
                &ap->Condition[4],
                &ap->Subcommand[0],
                &ap->Subcommand[1])
            != 8) {
            fprintf(stderr, "Bad action line (%d)\n", ct);
            FreeDatabase();
            return UNKNOWN_GAME;
        }

        if (loud) {
            debug_print("Action %d Vocab: %d (%d/%d)\n", ct, ap->Vocab,
                ap->Vocab % 150, ap->Vocab / 150);
            debug_print("Action %d Condition[0]: %d (%d/%d)\n", ct,
                ap->Condition[0], ap->Condition[0] % 20, ap->Condition[0] / 20);
            debug_print("Action %d Condition[1]: %d (%d/%d)\n", ct,
                ap->Condition[1], ap->Condition[1] % 20, ap->Condition[1] / 20);
            debug_print("Action %d Condition[2]: %d (%d/%d)\n", ct,
                ap->Condition[2], ap->Condition[2] % 20, ap->Condition[2] / 20);
            debug_print("Action %d Condition[0]: %d (%d/%d)\n", ct,
                ap->Condition[3], ap->Condition[3] % 20, ap->Condition[3] / 20);
            debug_print("Action %d Condition[0]: %d (%d/%d)\n", ct,
                ap->Condition[4], ap->Condition[4] % 20, ap->Condition[4] / 20);
            debug_print("Action %d Subcommand [0]]: %d (%d/%d)\n", ct, ap->Subcommand[0], ap->Subcommand[0] % 150, ap->Subcommand[0] / 150);
            debug_print("Action %d Subcommand [1]]: %d (%d/%d)\n", ct, ap->Subcommand[1], ap->Subcommand[1] % 150, ap->Subcommand[1] / 150);
        }
        ap++;
        ct++;
    }

    ct = 0;
    if (loud)
        debug_print("Reading %d word pairs.\n", nw);
    while (ct < nw + 1) {
        Verbs[ct] = ReadString(f);
        debug_print("Verbs %d:%s.\n", ct, Verbs[ct]);
        Nouns[ct] = ReadString(f);
        debug_print("Nouns %d:%s.\n", ct, Nouns[ct]);
        ct++;
    }
    ct = 0;
    rp = Rooms;
    if (loud)
        debug_print("Reading %d rooms.\n", nr);
    while (ct < nr + 1) {
        if (fscanf(f, "%hd %hd %hd %hd %hd %hd", &rp->Exits[0], &rp->Exits[1],
                &rp->Exits[2], &rp->Exits[3], &rp->Exits[4],
                &rp->Exits[5])
            != 6) {
            debug_print("Bad room line (%d)\n", ct);
            FreeDatabase();
            return UNKNOWN_GAME;
        }

        rp->Text = ReadString(f);
        if (loud)
            debug_print("Room %d: \"%s\"\n", ct, rp->Text);
        if (loud) {
            debug_print("Room connections for room %d:\n", ct);
            for (int i = 0; i < 6; i++)
                debug_print("Exit %d: %d\n", i, rp->Exits[i]);
        }
        rp->Image = 255;
        ct++;
        rp++;
    }

    ct = 0;
    if (loud)
        debug_print("Reading %d messages.\n", mn);
    while (ct < mn + 1) {
        Messages[ct] = ReadString(f);
        if (loud)
            debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
        ct++;
    }
    ct = 0;
    if (loud)
        debug_print("Reading %d items.\n", ni);
    ip = Items;
    while (ct < ni + 1) {
        ip->Text = ReadString(f);
        if (loud)
            debug_print("Item %d: \"%s\"\n", ct, ip->Text);
        ip->AutoGet = strchr(ip->Text, '/');
        /* Some games use // to mean no auto get/drop word! */
        if (ip->AutoGet && strcmp(ip->AutoGet, "//") && strcmp(ip->AutoGet, "/*")) {
            char *t;
            *ip->AutoGet++ = 0;
            t = strchr(ip->AutoGet, '/');
            if (t != NULL)
                *t = 0;
        }
        if (fscanf(f, "%hd", &lo) != 1) {
            debug_print("Bad item line (%d)\n", ct);
            FreeDatabase();
            return UNKNOWN_GAME;
        }
        ip->Location = (unsigned char)lo;
        if (loud)
            debug_print("Location of item %d: %d\n", ct, ip->Location);
        ip->InitialLoc = ip->Location;
        ip++;
        ct++;
    }
    ct = 0;
    /* Discard Comment Strings */
    while (ct < na + 1) {
        free(ReadString(f));
        ct++;
    }
    if (fscanf(f, "%d", &ct) != 1) {
        debug_print("Cannot read version\n");
        FreeDatabase();
        return UNKNOWN_GAME;
    }
    if (loud)
        debug_print("Version %d.%02d of Adventure ", ct / 100, ct % 100);
    if (fscanf(f, "%d", &ct) != 1) {
        debug_print("Cannot read adventure number\n");
        FreeDatabase();
        return UNKNOWN_GAME;
    }
    if (loud)
        debug_print("%d.\nLoad Complete.\n\n", ct);
    /* Extra value in at least Hulk */
    if (!fscanf(f, "%d", &ct)) {
        debug_print("No extra value in file. This is not Hulk.\n");
    }

    fclose(f);
    if (ct == 703 && LoadDOSImages()) {
        CurrentSys = SYS_MSDOS;
        return HULK_US;
    }
    return SCOTTFREE;
}

void Output(const char *a) { Display(Bottom, "%s", a); }

void OutputNumber(int a) { Display(Bottom, "%d", a); }

#if defined(__clang__)
#pragma mark Room description
#endif

void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, 0, x_offset, 0, 32 * 8 * pixel_size,
        12 * 8 * pixel_size);
}

void DrawImage(int image)
{
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    OpenGraphicsWindow();
    if (Graphics == NULL) {
        debug_print("DrawImage: Graphic window NULL?\n");
        return;
    }
    if (Game->picture_format_version == 99)
        DrawVectorPicture(image);
    else
        DrawSagaPictureNumber(image);
}

void DrawRoomImage(void)
{
    if (CurrentGame == ADVENTURELAND || CurrentGame == ADVENTURELAND_C64) {
        AdventurelandDarkness();
    }

    int dark = ((BitFlags & (1 << DARKBIT)) && Items[LIGHT_SOURCE].Location != CARRIED && Items[LIGHT_SOURCE].Location != MyLoc);

    if (dark && Graphics != NULL && (Rooms[MyLoc].Image != 255 || Game->type == US_VARIANT)) {
        vector_image_shown = -1;
        VectorState = NO_VECTOR_IMAGE;
        glk_request_timer_events(0);
        if (Game->type == US_VARIANT) {
            glk_window_clear(Graphics);
            DrawUSRoom(0);
            if (CurrentSys == SYS_APPLE2)
                DrawApple2ImageFromVideoMem();
        } else
            DrawBlack();
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

    if (Rooms[MyLoc].Image == 255) {
        CloseGraphicsWindow();
        return;
    }

    if (dark)
        return;

    if (Game->picture_format_version == 99) {
        DrawImage(MyLoc - 1);
        return;
    }

    if (Game->type == GREMLINS_VARIANT) {
        GremlinsLook();
    } else {
        DrawImage(Rooms[MyLoc].Image & 127);
    }
    for (int ct = 0; ct <= GameHeader.NumItems; ct++)
        if (Items[ct].Image && Items[ct].Location == MyLoc) {
            if ((Items[ct].Flag & 127) == MyLoc) {
                DrawImage(Items[ct].Image);
                /* Draw the correct image of the bear on the beach */
            } else if (Game->type == SAVAGE_ISLAND_VARIANT && ct == 20 && MyLoc == 8) {
                DrawImage(9);
            }
        }
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
    char msg[2048];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    glk_put_string_stream(room_description_stream, msg);
}

static void ListExitsSpectrumStyle(void)
{
    int ct = 0;
    int f = 0;

    while (ct < 6) {
        if ((&Rooms[MyLoc])->Exits[ct] != 0) {
            if (f == 0) {
                if (!(Options & PC_STYLE))
                    WriteToRoomDescriptionStream("\n\n");
                WriteToRoomDescriptionStream("%s", sys[EXITS]);
            } else {
                WriteToRoomDescriptionStream("%s", sys[EXITS_DELIMITER]);
            }
            /* sys[] begins with the exit names */
            WriteToRoomDescriptionStream("%s", sys[ct]);
            f = 1;
        }
        ct++;
    }
    WriteToRoomDescriptionStream("\n");
    return;
}

static void ListExits(void)
{
    int ct = 0;
    int f = 0;

    WriteToRoomDescriptionStream("\n\n%s", sys[EXITS]);

    while (ct < 6) {
        if ((&Rooms[MyLoc])->Exits[ct] != 0) {
            if (f) {
                WriteToRoomDescriptionStream("%s", sys[EXITS_DELIMITER]);
            }
            /* sys[] begins with the exit names */
            WriteToRoomDescriptionStream("%s", sys[ct]);
            f = 1;
        }
        ct++;
    }
    if (f == 0)
        WriteToRoomDescriptionStream("%s", sys[NONE]);
    return;
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

void Look(void)
{
    showing_inventory = 0;
    DrawRoomImage();

    if (split_screen && Top == NULL)
        return;

    char *buf = MemAlloc(1000);
    buf = memset(buf, 0, 1000);
    room_description_stream = glk_stream_open_memory(buf, 1000, filemode_Write, 0);

    Room *r;
    int ct, f;

    if (!split_screen) {
        WriteToRoomDescriptionStream("\n");
    } else if (Transcript && print_look_to_transcript) {
        glk_put_char_stream_uni(Transcript, 10);
    }

    if ((BitFlags & (1 << DARKBIT)) && Items[LIGHT_SOURCE].Location != CARRIED && Items[LIGHT_SOURCE].Location != MyLoc) {
        WriteToRoomDescriptionStream("%s", sys[TOO_DARK_TO_SEE]);
        FlushRoomDescription(buf);
        return;
    }

    r = &Rooms[MyLoc];

    if (!r->Text)
        return;
    /* An initial asterisk means the room description should not */
    /* start with "You are" or equivalent */
    if (*r->Text == '*') {
        WriteToRoomDescriptionStream("%s", r->Text + 1);
    } else {
        WriteToRoomDescriptionStream("%s%s", sys[YOU_ARE], r->Text);
    }

    if (!(Options & SPECTRUM_STYLE)) {
        ListExits();
        WriteToRoomDescriptionStream(".\n");
    }

    ct = 0;
    f = 0;
    while (ct <= GameHeader.NumItems) {
        if (Items[ct].Location == MyLoc) {
            if (Items[ct].Text[0] == 0) {
                fprintf(stderr, "Invisible item in room: %d\n", ct);
                ct++;
                continue;
            }
            if (f == 0) {
                WriteToRoomDescriptionStream("%s", sys[YOU_SEE]);
                f++;
                if (Options & SPECTRUM_STYLE && !(Options & PC_STYLE))
                    WriteToRoomDescriptionStream("\n");
            } else if (!(Options & (TRS80_STYLE | SPECTRUM_STYLE))) {
                WriteToRoomDescriptionStream("%s", sys[ITEM_DELIMITER]);
            }
            WriteToRoomDescriptionStream("%s", Items[ct].Text);
            if (Options & (TRS80_STYLE | SPECTRUM_STYLE)) {
                WriteToRoomDescriptionStream("%s", sys[ITEM_DELIMITER]);
            }
        }
        ct++;
    }

    if ((Options & TI994A_STYLE) && f) {
        WriteToRoomDescriptionStream(".");
    } else if (Options & PC_STYLE && !f)
        WriteToRoomDescriptionStream(". ");

    if (Options & SPECTRUM_STYLE) {
        ListExitsSpectrumStyle();
    } else if (f) {
        WriteToRoomDescriptionStream("\n");
    }

    if ((AutoInventory || (Options & FORCE_INVENTORY)) && !(Options & FORCE_INVENTORY_OFF)) {
        WriteToRoomDescriptionStream("\n");
        ListInventory(1);
    }

    FlushRoomDescription(buf);
}

void SaveGame(void)
{
    strid_t file;
    frefid_t ref;
    int ct;
    char buf[128];

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
        filemode_Write, 0);
    if (ref == NULL)
        return;

    file = glk_stream_open_file(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);
    if (file == NULL)
        return;

    for (ct = 0; ct < 16; ct++) {
        snprintf(buf, sizeof buf, "%d %d\n", Counters[ct], RoomSaved[ct]);
        glk_put_string_stream(file, buf);
    }
    snprintf(buf, sizeof buf, "%ld %d %hd %d %d %hd %d\n", BitFlags,
        (BitFlags & (1 << DARKBIT)) ? 1 : 0, MyLoc, CurrentCounter,
        SavedRoom, GameHeader.LightTime, AutoInventory);
    glk_put_string_stream(file, buf);
    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        snprintf(buf, sizeof buf, "%hd\n", (short)Items[ct].Location);
        glk_put_string_stream(file, buf);
    }

    glk_stream_close(file, NULL);
    Output(sys[SAVED]);
}

static void LoadGame(void)
{
    strid_t file;
    frefid_t ref;
    char buf[128];
    int ct = 0;
    short lo;
    short DarkFlag;

    int PreviousAutoInventory = AutoInventory;

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
        filemode_Read, 0);
    if (ref == NULL)
        return;

    file = glk_stream_open_file(ref, filemode_Read, 0);
    glk_fileref_destroy(ref);
    if (file == NULL)
        return;

    struct SavedState *state = SaveCurrentState();

    int result;

    for (ct = 0; ct < 16; ct++) {
        glk_get_line_stream(file, buf, sizeof buf);
        result = sscanf(buf, "%d %d", &Counters[ct], &RoomSaved[ct]);
        if (result != 2 || RoomSaved[ct] > GameHeader.NumRooms) {
            RecoverFromBadRestore(state);
            return;
        }
    }
    glk_get_line_stream(file, buf, sizeof buf);
    result = sscanf(buf, "%ld %hd %hd %d %d %hd %d\n", &BitFlags, &DarkFlag,
        &MyLoc, &CurrentCounter, &SavedRoom, &GameHeader.LightTime,
        &AutoInventory);
    if (result == 6)
        AutoInventory = PreviousAutoInventory;
    if ((result != 7 && result != 6) || MyLoc > GameHeader.NumRooms || MyLoc < 1 || SavedRoom > GameHeader.NumRooms) {
        RecoverFromBadRestore(state);
        return;
    }

    /* Backward compatibility */
    if (DarkFlag)
        BitFlags |= (1 << 15);
    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        glk_get_line_stream(file, buf, sizeof buf);
        result = sscanf(buf, "%hd\n", &lo);
        Items[ct].Location = (unsigned char)lo;
        if (result != 1 || (Items[ct].Location > GameHeader.NumRooms && Items[ct].Location != CARRIED)) {
            RecoverFromBadRestore(state);
            return;
        }
    }

    glui32 position = glk_stream_get_position(file);
    glk_stream_set_position(file, 0, seekmode_End);
    glui32 end = glk_stream_get_position(file);
    if (end != position) {
        RecoverFromBadRestore(state);
        return;
    }

    SaveUndo();
    JustStarted = 0;
    StopTime = 1;
}

static void RestartGame(void)
{
    if (CurrentCommand)
        FreeCommands();
    RestoreState(InitialState);
    JustStarted = 0;
    StopTime = 0;
    glk_window_clear(Bottom);
    OpenTopWindow();
    should_restart = 0;
}

static void TranscriptOn(void)
{
    frefid_t ref;

    if (Transcript) {
        Output(sys[TRANSCRIPT_ALREADY]);
        return;
    }

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_Transcript,
        filemode_Write, 0);
    if (ref == NULL)
        return;

    Transcript = glk_stream_open_file_uni(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);

    if (Transcript == NULL) {
        Output(sys[FAILED_TRANSCRIPT]);
        return;
    }

    glui32 *start_of_transcript = ToUnicode(sys[TRANSCRIPT_START]);
    glk_put_string_stream_uni(Transcript, start_of_transcript);
    free(start_of_transcript);
    glk_put_string_stream(glk_window_get_stream(Bottom),
        (char *)sys[TRANSCRIPT_ON]);
}

static void TranscriptOff(void)
{
    if (Transcript == NULL) {
        Output(sys[NO_TRANSCRIPT]);
        return;
    }

    glui32 *end_of_transcript = ToUnicode(sys[TRANSCRIPT_END]);
    glk_put_string_stream_uni(Transcript, end_of_transcript);
    free(end_of_transcript);

    glk_stream_close(Transcript, NULL);
    Transcript = NULL;
    Output(sys[TRANSCRIPT_OFF]);
}

int PerformExtraCommand(int extra_stop_time)
{
    struct Command command = *CurrentCommand;
    int verb = command.verb;
    if (verb > GameHeader.NumWords)
        verb -= GameHeader.NumWords;
    int noun = command.noun;
    if (noun > GameHeader.NumWords)
        noun -= GameHeader.NumWords;
    else if (noun) {
        const char *NounWord = CharWords[CurrentCommand->nounwordindex];
        int newnoun = WhichWord(NounWord, ExtraNouns, strlen(NounWord),
            NUMBER_OF_EXTRA_NOUNS);
        newnoun = ExtraNounsKey[newnoun];
        if (newnoun)
            noun = newnoun;
    }

    StopTime = 1 + extra_stop_time;

    switch (verb) {
    case RESTORE:
        if (noun == 0 || noun == GAME) {
            LoadGame();
            return 1;
        }
        break;
    case RESTART:
        if (noun == 0 || noun == GAME) {
            Output(sys[ARE_YOU_SURE]);
            if (YesOrNo()) {
                should_restart = 1;
            }
            return 1;
        }
        break;
    case SAVE:
        if (noun == 0 || noun == GAME) {
            SaveGame();
            return 1;
        }
        break;
    case UNDO:
        if (noun == 0 || noun == COMMAND) {
            RestoreUndo();
            return 1;
        }
        break;
    case RAM:
        if (noun == RAMLOAD) {
            RamRestore();
            return 1;
        } else if (noun == RAMSAVE) {
            RamSave();
            return 1;
        }
        break;
    case RAMSAVE:
        if (noun == 0) {
            RamSave();
            return 1;
        }
        break;
    case RAMLOAD:
        if (noun == 0) {
            RamRestore();
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
    case EXCEPT:
        FreeCommands();
    }

    StopTime = 0;
    return 0;
}

static int YesOrNo(void)
{
    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;
    const char y = tolower((unsigned char)sys[YES][0]);
    const char n = tolower((unsigned char)sys[NO][0]);

    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            const char reply = tolower(ev.val1);
            if (reply == y) {
                result = 1;
            } else if (reply == n) {
                result = 2;
            } else {
                Output(sys[ANSWER_YES_OR_NO]);
                glk_request_char_event(Bottom);
            }
        } else
            Updates(ev);
    } while (result == 0);

    return (result == 1);
}

void HitEnter(void)
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
        } else
            Updates(ev);
    } while (result == 0);
    showing_closeup = 0;
    return;
}

static void WriteToLowerWindow(const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glui32 *unistring = ToUnicode(msg);
    glk_put_string_stream_uni(glk_window_get_stream(Bottom), unistring);
    if (Transcript)
        glk_put_string_stream_uni(Transcript, unistring);
    free(unistring);
}

void ListInventory(int upper)
{
    void (*print_function)(const char *fmt, ...);
    if (upper) {
        print_function = WriteToRoomDescriptionStream;
    } else {
        print_function = WriteToLowerWindow;
    }

    int i = 0;
    int lastitem = -1;
    print_function("%s", sys[INVENTORY]);
    while (i <= GameHeader.NumItems) {
        if (Items[i].Location == CARRIED) {
            if (Items[i].Text[0] == 0) {
                fprintf(stderr, "Invisible item in inventory: %d\n", i);
                i++;
                continue;
            }
            if (lastitem > -1 && (Options & (TRS80_STYLE | SPECTRUM_STYLE)) == 0) {
                print_function("%s", sys[ITEM_DELIMITER]);
            }
            lastitem = i;
            print_function("%s", Items[i].Text);
            if (Options & (TRS80_STYLE | SPECTRUM_STYLE)) {
                print_function("%s", sys[ITEM_DELIMITER]);
            }
        }
        i++;
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
        glk_put_char_stream_uni(Transcript, 10);
    }
}

static void LookWithPause(void)
{
    char fc = Rooms[MyLoc].Text[0];
    if (Rooms[MyLoc].Text == NULL || MyLoc == 0 || fc == 0 || fc == '.' || fc == ' ')
        return;
    should_look_in_transcript = 1;
    pause_next_room_description = 1;
    Look();
}

void DoneIt(void)
{
    if (split_screen && Top)
        Look();
    Output("\n\n");
    Output(sys[PLAY_AGAIN]);
    Output("\n");
    if (YesOrNo()) {
        should_restart = 1;
    } else {
        CleanupAndExit();
    }
}

int PrintScore(void)
{
    int i = 0;
    int n = 0;
    while (i <= GameHeader.NumItems) {
        if (Items[i].Location == GameHeader.TreasureRoom && *Items[i].Text == '*')
            n++;
        i++;
    }
    Display(Bottom, "%s %d %s%s %d.\n", sys[IVE_STORED], n, sys[TREASURES],
        sys[ON_A_SCALE_THAT_RATES], (n * 100) / GameHeader.Treasures);
    if (n == GameHeader.Treasures) {
        Output(sys[YOUVE_SOLVED_IT]);
        DoneIt();
        return 1;
    }
    return 0;
}

void PrintNoun(void)
{
    if (CurrentCommand)
        glk_put_string_stream_uni(glk_window_get_stream(Bottom),
            UnicodeWords[CurrentCommand->nounwordindex]);
}

void MoveItemAToLocOfItemB(int itemA, int itemB)
{
    Items[itemA].Location = Items[itemB].Location;
    if (Items[itemB].Location == MyLoc)
        should_look_in_transcript = 1;
}

void GoToStoredLoc(void)
{
#ifdef DEBUG_ACTIONS
    debug_print("switch location to stored location (%d) (%s).\n",
        SavedRoom, Rooms[SavedRoom].Text);
#endif
    int t = MyLoc;
    MyLoc = SavedRoom;
    SavedRoom = t;
    should_look_in_transcript = 1;
}

void SwapLocAndRoomflag(int index)
{
#ifdef DEBUG_ACTIONS
    debug_print("swap location<->roomflag[%d]\n", index);
#endif
    int temp = MyLoc;
    MyLoc = RoomSaved[index];
    RoomSaved[index] = temp;
    should_look_in_transcript = 1;
    Look();
}

void SwapItemLocations(int itemA, int itemB)
{
    int temp = Items[itemA].Location;
    Items[itemA].Location = Items[itemB].Location;
    Items[itemB].Location = temp;
    if (Items[itemA].Location == MyLoc || Items[itemB].Location == MyLoc)
        should_look_in_transcript = 1;
}

void PutItemAInRoomB(int itemA, int roomB)
{
#ifdef DEBUG_ACTIONS
    debug_print("Item %d (%s) is put in room %d (%s). MyLoc: %d (%s)\n",
        itemA, Items[itemA].Text, roomB, Rooms[roomB].Text, MyLoc,
        Rooms[MyLoc].Text);
#endif
    if (Items[itemA].Location == MyLoc)
        LookWithPause();
    Items[itemA].Location = roomB;
}

void SwapCounters(int index)
{
#ifdef DEBUG_ACTIONS
    debug_print(
        "Select a counter. Current counter is swapped with backup "
        "counter %d\n",
        index);
#endif
    if (index > 15) {
        debug_print("ERROR! parameter out of range. Max 15, got %d\n", index);
        index = 15;
    }
    int temp = CurrentCounter;

    CurrentCounter = Counters[index];
    Counters[index] = temp;
#ifdef DEBUG_ACTIONS
    debug_print("Value of new selected counter is %d\n",
        CurrentCounter);
#endif
}

void PrintMessage(int index)
{
#ifdef DEBUG_ACTIONS
    debug_print("Print message %d: \"%s\"\n", index,
        Messages[index]);
#endif
    const char *message = Messages[index];
    if (message != NULL && message[0] != 0) {
        Output(message);
        const char lastchar = message[strlen(message) - 1];
        if (lastchar != 13 && lastchar != 10)
            Output(sys[MESSAGE_DELIMITER]);
    }
}

void PlayerIsDead(void)
{
#ifdef DEBUG_ACTIONS
    debug_print("Player is dead\n");
#endif
    Output(sys[IM_DEAD]);
    BitFlags &= ~(1 << DARKBIT);
    MyLoc = GameHeader.NumRooms; /* It seems to be what the code says! */
}

static ActionResultType PerformLine(int ct)
{
#ifdef DEBUG_ACTIONS
    debug_print("Performing line %d: ", ct);
#endif
    int continuation = 0, dead = 0;
    int param[5], pptr = 0;
    int p;
    int act[4];
    int cc = 0;
    while (cc < 5) {
        int cv, dv;
        cv = Actions[ct].Condition[cc];
        dv = cv / 20;
        cv %= 20;
#ifdef DEBUG_ACTIONS
        debug_print("Testing condition %d: ", cv);
#endif
        switch (cv) {
        case 0:
            param[pptr++] = dv;
            break;
        case 1:
#ifdef DEBUG_ACTIONS
            debug_print("Does the player carry %s?\n", Items[dv].Text);
#endif
            if (Items[dv].Location != CARRIED)
                return ACT_FAILURE;
            break;
        case 2:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s in location?\n", Items[dv].Text);
#endif
            if (Items[dv].Location != MyLoc)
                return ACT_FAILURE;
            break;
        case 3:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s held or in location?\n", Items[dv].Text);
#endif
            if (Items[dv].Location != CARRIED && Items[dv].Location != MyLoc)
                return ACT_FAILURE;
            break;
        case 4:
#ifdef DEBUG_ACTIONS
            debug_print("Is location %s?\n", Rooms[dv].Text);
#endif
            if (MyLoc != dv)
                return ACT_FAILURE;
            break;
        case 5:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s NOT in location?\n", Items[dv].Text);
#endif
            if (Items[dv].Location == MyLoc)
                return ACT_FAILURE;
            break;
        case 6:
#ifdef DEBUG_ACTIONS
            debug_print("Does the player NOT carry %s?\n", Items[dv].Text);
#endif
            if (Items[dv].Location == CARRIED)
                return ACT_FAILURE;
            break;
        case 7:
#ifdef DEBUG_ACTIONS
            debug_print("Is location NOT %s?\n", Rooms[dv].Text);
#endif
            if (MyLoc == dv)
                return ACT_FAILURE;
            break;
        case 8:
#ifdef DEBUG_ACTIONS
            debug_print("Is bitflag %d set?\n", dv);
#endif
            if ((BitFlags & (1 << dv)) == 0)
                return ACT_FAILURE;
            break;
        case 9:
#ifdef DEBUG_ACTIONS
            debug_print("Is bitflag %d NOT set?\n", dv);
#endif
            if (BitFlags & (1 << dv))
                return ACT_FAILURE;
            break;
        case 10:
#ifdef DEBUG_ACTIONS
            debug_print("Does the player carry anything?\n");
#endif
            if (CountCarried() == 0)
                return ACT_FAILURE;
            break;
        case 11:
#ifdef DEBUG_ACTIONS
            debug_print("Does the player carry nothing?\n");
#endif
            if (CountCarried())
                return ACT_FAILURE;
            break;
        case 12:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s neither carried nor in room?\n", Items[dv].Text);
#endif
            if (Items[dv].Location == CARRIED || Items[dv].Location == MyLoc)
                return ACT_FAILURE;
            break;
        case 13:
            if (dv > GameHeader.NumItems + 1)
                Fatal("Broken database!");
#ifdef DEBUG_ACTIONS
            debug_print("Is %s (%d) in play?\n", Items[dv].Text, dv);
#endif
            if (Items[dv].Location == 0)
                return ACT_FAILURE;
            break;
        case 14:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s NOT in play?\n", Items[dv].Text);
#endif
            if (Items[dv].Location)
                return ACT_FAILURE;
            break;
        case 15:
#ifdef DEBUG_ACTIONS
            debug_print("Is CurrentCounter <= %d?\n", dv);
#endif
            if (CurrentCounter > dv)
                return ACT_FAILURE;
            break;
        case 16:
#ifdef DEBUG_ACTIONS
            debug_print("Is CurrentCounter > %d?\n", dv);
#endif
            if (CurrentCounter <= dv)
                return ACT_FAILURE;
            break;
        case 17:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s still in initial room?\n", Items[dv].Text);
#endif
            if (Items[dv].Location != Items[dv].InitialLoc)
                return ACT_FAILURE;
            break;
        case 18:
#ifdef DEBUG_ACTIONS
            debug_print("Has %s been moved?\n", Items[dv].Text);
#endif
            if (Items[dv].Location == Items[dv].InitialLoc)
                return ACT_FAILURE;
            break;
        case 19: /* Only seen in Brian Howarth games so far */
#ifdef DEBUG_ACTIONS
            debug_print("Is current counter == %d?\n", dv);
            if (CurrentCounter != dv)
                debug_print("Nope, current counter is %d\n", CurrentCounter);
#endif
            if (CurrentCounter != dv)
                return ACT_FAILURE;
            break;
        }
#ifdef DEBUG_ACTIONS
        debug_print("YES\n");
#endif
        cc++;
    }
#if defined(__clang__)
#pragma mark Subcommands
#endif

    /* Actions */
    act[0] = Actions[ct].Subcommand[0];
    act[2] = Actions[ct].Subcommand[1];
    act[1] = act[0] % 150;
    act[3] = act[2] % 150;
    act[0] /= 150;
    act[2] /= 150;
    cc = 0;
    pptr = 0;
    while (cc < 4) {
#ifdef DEBUG_ACTIONS
        debug_print("Performing action %d: ", act[cc]);
#endif
        if (act[cc] >= 1 && act[cc] < 52) {
            PrintMessage(act[cc]);
        } else if (act[cc] > 101) {
            PrintMessage(act[cc] - 50);
        } else
            switch (act[cc]) {
            case 0: /* NOP */
                break;
            case 52:
                if (CountCarried() >= GameHeader.MaxCarry) {
                    Output(sys[YOURE_CARRYING_TOO_MUCH]);
                    return ACT_SUCCESS;
                }
                Items[param[pptr++]].Location = CARRIED;
                break;
            case 53:
#ifdef DEBUG_ACTIONS
                debug_print("item %d (\"%s\") is now in location.\n", param[pptr],
                    Items[param[pptr]].Text);
#endif
                Items[param[pptr++]].Location = MyLoc;
                should_look_in_transcript = 1;
                break;
            case 54:
#ifdef DEBUG_ACTIONS
                debug_print("player location is now room %d (%s).\n", param[pptr],
                    Rooms[param[pptr]].Text);
#endif
                MyLoc = param[pptr++];
                should_look_in_transcript = 1;
                Look();
                break;
            case 55:
#ifdef DEBUG_ACTIONS
                debug_print(
                    "Item %d (%s) is removed from the game (put in room 0).\n",
                    param[pptr], Items[param[pptr]].Text);
#endif
                Items[param[pptr++]].Location = 0;
                break;
            case 56:
                BitFlags |= 1 << DARKBIT;
                break;
            case 57:
                BitFlags &= ~(1 << DARKBIT);
                break;
            case 58:
#ifdef DEBUG_ACTIONS
                debug_print("Bitflag %d is set\n", param[pptr]);
#endif
                BitFlags |= (1 << param[pptr++]);
                break;
            case 59:
#ifdef DEBUG_ACTIONS
                debug_print("Item %d (%s) is removed from play.\n", param[pptr],
                    Items[param[pptr]].Text);
#endif
                Items[param[pptr++]].Location = 0;
                break;
            case 60:
#ifdef DEBUG_ACTIONS
                debug_print("BitFlag %d is cleared\n", param[pptr]);
#endif
                BitFlags &= ~(1 << param[pptr++]);
                break;
            case 61:
                PlayerIsDead();
                break;
            case 62:
                p = param[pptr++];
                PutItemAInRoomB(p, param[pptr++]);
                break;
            case 63:
#ifdef DEBUG_ACTIONS
                debug_print("Game over.\n");
#endif
                DoneIt();
                dead = 1;
                break;
            case 64:
                break;
            case 65:
                dead = PrintScore();
                StopTime = 2;
                break;
            case 66:
                if (Game->type == SEAS_OF_BLOOD_VARIANT)
                    AdventureSheet();
                else
                    ListInventory(0);
                if (Game->type == US_VARIANT && has_graphics()) {
                    UpdateUSInventory();
                }
                StopTime = 2;
                break;
            case 67:
                BitFlags |= (1 << 0);
                break;
            case 68:
                BitFlags &= ~(1 << 0);
                break;
            case 69:
                GameHeader.LightTime = LightRefill;
                Items[LIGHT_SOURCE].Location = CARRIED;
                BitFlags &= ~(1 << LIGHTOUTBIT);
                break;
            case 70:
                ClearScreen(); /* pdd. */
                break;
            case 71:
                SaveGame();
                StopTime = 2;
                break;
            case 72:
                p = param[pptr++];
                SwapItemLocations(p, param[pptr++]);
                break;
            case 73:
#ifdef DEBUG_ACTIONS
                debug_print("Continue with next line\n");
#endif
                continuation = 1;
                break;
            case 74:
                Items[param[pptr++]].Location = CARRIED;
                break;
            case 75:
                p = param[pptr++];
                MoveItemAToLocOfItemB(p, param[pptr++]);
                break;
            case 76: /* Looking at adventure .. */
#ifdef DEBUG_ACTIONS
                debug_print("LOOK\n");
#endif
                if (split_screen)
                    Look();
                should_look_in_transcript = 1;
                break;
            case 77:
                if (CurrentCounter >= 1)
                    CurrentCounter--;
#ifdef DEBUG_ACTIONS
                debug_print(
                    "decrementing current counter. Current counter is now %d.\n",
                    CurrentCounter);
#endif
                break;
            case 78:
                OutputNumber(CurrentCounter);
                Output(" ");
                break;
            case 79:
#ifdef DEBUG_ACTIONS
                debug_print("CurrentCounter is set to %d.\n", param[pptr]);
#endif
                CurrentCounter = param[pptr++];
                break;
            case 80:
                GoToStoredLoc();
                break;
            case 81:
                SwapCounters(param[pptr++]);
                break;
            case 82:
                CurrentCounter += param[pptr++];
                break;
            case 83:
                CurrentCounter -= param[pptr++];
                if (CurrentCounter < -1)
                    CurrentCounter = -1;
                /* Note: This seems to be needed. I don't yet
                         know if there is a maximum value to limit too */
                break;
            case 84:
                PrintNoun();
                break;
            case 85:
                PrintNoun();
                Output("\n");
                break;
            case 86:
                if (!(Options & SPECTRUM_STYLE))
                    Output("\n");
                break;
            case 87:
                SwapLocAndRoomflag(param[pptr++]);
                break;
            case 88:
#ifdef DEBUG_ACTIONS
                debug_print("Delay\n");
#endif
                Delay(1);
                break;
            case 89:
#ifdef DEBUG_ACTIONS
                debug_print("Action 89, parameter %d\n", param[pptr]);
#endif
                p = param[pptr++];
                switch (CurrentGame) {
                case SPIDERMAN:
                case SPIDERMAN_C64:
                    DrawBlack();
                    break;
                case SECRET_MISSION:
                case SECRET_MISSION_C64:
                    SecretAction(p);
                    break;
                case ADVENTURELAND:
                case ADVENTURELAND_C64:
                    AdventurelandAction(p);
                    break;
                case SEAS_OF_BLOOD:
                case SEAS_OF_BLOOD_C64:
                    BloodAction(p);
                    break;
                case ROBIN_OF_SHERWOOD:
                case ROBIN_OF_SHERWOOD_C64:
                    SherwoodAction(p);
                    break;
                case GREMLINS:
                case GREMLINS_SPANISH:
                case GREMLINS_SPANISH_C64:
                case GREMLINS_GERMAN:
                case GREMLINS_GERMAN_C64:
                    GremlinsAction();
                    break;
                default:
                    break;
                }
                break;

            case 90:
#ifdef DEBUG_ACTIONS
                debug_print("Draw Hulk image, parameter %d\n", param[pptr]);
#endif
                if (CurrentGame != HULK && CurrentGame != HULK_C64 && CurrentGame != HULK_US) {
                    pptr++;
                } else if (!(BitFlags & (1 << DARKBIT)))
                    DrawHulkImage(param[pptr++]);
                break;
            default:
                debug_print("Unknown action %d [Param begins %d %d]\n", act[cc],
                    param[pptr], param[pptr + 1]);
                break;
            }
        cc++;
    }

    if (dead) {
        return ACT_GAMEOVER;
    } else if (continuation) {
        return ACT_CONTINUE;
    } else {
        return ACT_SUCCESS;
    }
}

static void PrintTakenOrDropped(int index)
{
    Output(sys[index]);
    int length = strlen(sys[index]);
    char last = sys[index][length - 1];
    if (last == 10 || last == 13)
        return;
    Output(" ");
    if ((!(CurrentCommand->allflag & LASTALL))
        || split_screen == 0) {
        Output("\n");
    }
}

static ExplicitResultType PerformActions(int vb, int no)
{
    int dark = BitFlags & (1 << DARKBIT);
    if (Items[LIGHT_SOURCE].Location == MyLoc || Items[LIGHT_SOURCE].Location == CARRIED)
        dark = 0;
    int ct = 0;
    ExplicitResultType flag;
    int doagain = 0;
    int found_match = 0;
#if defined(__clang__)
#pragma mark GO
#endif
    if (vb == GO && no == -1) {
        Output(sys[DIRECTION]);
        return ER_SUCCESS;
    }
    if (vb == 1 && no >= 1 && no <= 6) {
        int nl;
        if (dark)
            Output(sys[DANGEROUS_TO_MOVE_IN_DARK]);
        nl = Rooms[MyLoc].Exits[no - 1];
        if (nl != 0) {
            /* Seas of Blood needs this to be able to flee back to the last room */
            if (Game->type == SEAS_OF_BLOOD_VARIANT)
                SavedRoom = MyLoc;
            if (Options & (SPECTRUM_STYLE | TI994A_STYLE))
                Output(sys[OK]);
            MyLoc = nl;
            should_look_in_transcript = 1;
            if (CurrentCommand && CurrentCommand->next) {
                LookWithPause();
            }
            return ER_SUCCESS;
        }
        if (dark) {
            BitFlags &= ~(1 << DARKBIT);
            MyLoc = GameHeader.NumRooms; /* It seems to be what the code says! */
            Output(sys[YOU_FELL_AND_BROKE_YOUR_NECK]);
            BitFlags &= ~(1 << DARKBIT);
            MyLoc = GameHeader.NumRooms; /* It seems to be what the code says! */
            return ER_SUCCESS;
        }
        Output(sys[YOU_CANT_GO_THAT_WAY]);
        return ER_SUCCESS;
    }

    if (!dark) {
        if ((CurrentGame == HULK || CurrentGame == HULK_C64 || CurrentGame == HULK_US) && vb == 39) {
            HulkShowImageOnExamine(no);
        } else if (CurrentGame == COUNT_US && vb == 8) {
            CountShowImageOnExamineUS(no);
        } else if (CurrentGame == VOODOO_CASTLE_US && vb == 42) {
            VoodooShowImageOnExamineUS(no);
        }
    }

    if (CurrentCommand && CurrentCommand->allflag && vb == CurrentCommand->verb && !(dark && vb == TAKE)) {
        if (!lastwasnewline)
            Output("\n");
        Output(Items[CurrentCommand->item].Text);
        Output("....");
    }
    flag = ER_RAN_ALL_LINES_NO_MATCH;
    if (CurrentGame != TI994A) {
        while (ct <= GameHeader.NumActions) {
            int verbvalue, nounvalue;
            verbvalue = Actions[ct].Vocab;
            /* Think this is now right. If a line we run has an action73
           run all following lines with vocab of 0,0 */
            if (vb != 0 && (doagain && verbvalue != 0))
                break;
            /* Oops.. added this minor cockup fix 1.11 */
            if (vb != 0 && !doagain && flag == 0)
                break;
            nounvalue = verbvalue % 150;
            verbvalue /= 150;
            if ((verbvalue == vb) || (doagain && Actions[ct].Vocab == 0)) {
                if ((verbvalue == 0 && RandomPercent(nounvalue)) || doagain || (verbvalue != 0 && (nounvalue == no || nounvalue == 0))) {
                    if (verbvalue == vb && vb != 0 && nounvalue == no)
                        found_match = 1;
                    ActionResultType flag2;
                    if (flag == ER_RAN_ALL_LINES_NO_MATCH)
                        flag = ER_RAN_ALL_LINES;
                    if ((flag2 = PerformLine(ct)) != ACT_FAILURE) {
                        /* ahah finally figured it out ! */
                        flag = ER_SUCCESS;
                        if (flag2 == ACT_CONTINUE)
                            doagain = 1;
                        else if (flag2 == ACT_GAMEOVER)
                            return ER_SUCCESS;
                        if (vb != 0 && doagain == 0)
                            return ER_SUCCESS;
                    }
                }
            }

            ct++;

            // clang-format off
      /* Previously this did not check ct against
       * GameHeader.NumActions and would read past the end of
       * Actions.  I don't know what should happen on the last
       * action, but doing nothing is better than reading one
       * past the end.
       * --Chris
       */
            // clang-format on

            if (ct <= GameHeader.NumActions && Actions[ct].Vocab != 0)
                doagain = 0;
        }
    } else {
        if (vb == 0) {
            RunImplicitTI99Actions();
            return ER_NO_RESULT;
        } else {
            flag = RunExplicitTI99Actions(vb, no);
        }
    }

    if (found_match)
        return flag;

    if (flag != ER_SUCCESS) {
        int item = 0;
#if defined(__clang__)
#pragma mark TAKE
#endif
        if (vb == TAKE || vb == DROP) {
            if (CurrentCommand && CurrentCommand->allflag) {
                if (vb == TAKE && dark) {
                    Output(sys[TOO_DARK_TO_SEE]);
                    while (!(CurrentCommand->allflag & LASTALL)) {
                        CurrentCommand = CurrentCommand->next;
                    }
                    return ER_SUCCESS;
                }
                item = CurrentCommand->item;
                int location = CARRIED;
                if (vb == TAKE)
                    location = MyLoc;
                while (Items[item].Location != location && !(CurrentCommand->allflag & LASTALL)) {
                    CurrentCommand = CurrentCommand->next;
                }
                if (Items[item].Location != location)
                    return ER_SUCCESS;
            }

            /* Yes they really _are_ hardcoded values */
            if (vb == TAKE) {
                if (no == -1) {
                    Output(sys[WHAT]);
                    return ER_SUCCESS;
                }
                if (CountCarried() >= GameHeader.MaxCarry) {
                    Output(sys[YOURE_CARRYING_TOO_MUCH]);
                    return ER_SUCCESS;
                }
                if (!item)
                    item = MatchUpItem(no, MyLoc);
                if (item == -1) {
                    item = MatchUpItem(no, CARRIED);
                    if (item == -1) {
                        item = MatchUpItem(no, 0);
                        if (item == -1) {
                            Output(sys[THATS_BEYOND_MY_POWER]);
                        } else {
                            Output(sys[YOU_DONT_SEE_IT]);
                        }
                    } else {
                        Output(sys[YOU_HAVE_IT]);
                    }
                    return ER_SUCCESS;
                }
                Items[item].Location = CARRIED;
                PrintTakenOrDropped(TAKEN);
                return ER_SUCCESS;
            }
#if defined(__clang__)
#pragma mark DROP
#endif
            if (vb == DROP) {
                if (no == -1) {
                    Output(sys[WHAT]);
                    return ER_SUCCESS;
                }
                if (!item)
                    item = MatchUpItem(no, CARRIED);
                if (item == -1) {
                    item = MatchUpItem(no, 0);
                    if (item == -1) {
                        Output(sys[THATS_BEYOND_MY_POWER]);
                    } else {
                        Output(sys[YOU_HAVENT_GOT_IT]);
                    }
                    return ER_SUCCESS;
                }
                Items[item].Location = MyLoc;
                PrintTakenOrDropped(DROPPED);
                return ER_SUCCESS;
            }
        }
    }
    return flag;
}

glkunix_argumentlist_t glkunix_arguments[] = {
    { "-y", glkunix_arg_NoValue,
        "-y        Generate 'You are', 'You are carrying' type messages for games "
        "that use these instead (eg Robin Of Sherwood)" },
    { "-i", glkunix_arg_NoValue,
        "-i        Generate 'I am' type messages (default)" },
    { "-d", glkunix_arg_NoValue, "-d        Debugging info on load " },
    { "-s", glkunix_arg_NoValue,
        "-s        Generate authentic Scott Adams driver light messages rather "
        "than other driver style ones (Light goes out in n turns..)" },
    { "-t", glkunix_arg_NoValue,
        "-t        Generate TRS80 style display (terminal width is 64 characters; "
        "a line <-----------------> is displayed after the top stuff; objects "
        "have periods after them instead of hyphens" },
    { "-p", glkunix_arg_NoValue,
        "-p        Use for prehistoric databases which don't use bit 16" },
    { "-w", glkunix_arg_NoValue, "-w        Disable upper window" },
    { "-n", glkunix_arg_NoValue, "-n        No delays" },
    { "", glkunix_arg_ValueFollows, "filename    file to load" },

    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    int argc = data->argc;
    char **argv = data->argv;

    if (argc < 1)
        return 0;

    if (argc > 1)
        while (argv[1]) {
            if (*argv[1] != '-')
                break;
            switch (argv[1][1]) {
            case 'y':
                Options |= YOUARE;
                break;
            case 'i':
                Options &= ~YOUARE;
                break;
            case 'd':
                Options |= DEBUGGING;
                break;
            case 's':
                Options |= SCOTTLIGHT;
                break;
            case 't':
                Options |= TRS80_STYLE;
                break;
            case 'p':
                Options |= PREHISTORIC_LAMP;
                break;
            case 'w':
                split_screen = 0;
                break;
            case 'n':
                Options |= NO_DELAYS;
                break;
            }
            argv++;
            argc--;
        }

#ifdef GARGLK
    garglk_set_program_name("ScottFree 1.14");
    garglk_set_program_info("ScottFree 1.14 by Alan Cox\n"
                            "Glk port by Chris Spiegel\n");
#endif

    if (argc == 2) {
        game_file = argv[1];

#ifdef GARGLK
        const char *s;
        if ((s = strrchr(game_file, '/')) != NULL || (s = strrchr(game_file, '\\')) != NULL) {
            garglk_set_story_name(s + 1);
        } else {
            garglk_set_story_name(game_file);
        }
#endif

        if (game_file) {
            const char *n;
            int dirlen = 0;
            if ((n = strrchr(game_file, '/')) != NULL || (n = strrchr(game_file, '\\')) != NULL) {
                dirlen = (int)(n - game_file + 1);
            }
            if (dirlen) {
                DirPath = MemAlloc(dirlen + 1);
                memcpy(DirPath, game_file, dirlen);
                DirPath[dirlen] = 0;
            }
        }
    }

    return 1;
}

void glk_main(void)
{
    int vb, no, n = 1;

    if (*(char *)&n != 1) {
        WeAreBigEndian = 1;
    }

    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_Proportional, 0);
    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_Indentation, 20);
    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_ParaIndentation,
        20);
    glk_stylehint_set(wintype_TextBuffer, style_Preformatted, stylehint_Justification, stylehint_just_Centered);

    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    if (Bottom == NULL)
        glk_exit();
    glk_set_window(Bottom);

    if (game_file == NULL)
        Fatal("No game provided");

    const char **dictpointer;

    if (Options & YOUARE)
        dictpointer = sysdict;
    else
        dictpointer = sysdict_i_am;

    for (int i = 0; i < MAX_SYSMESS; i++) {
        if (dictpointer[i] != NULL)
            sys[i] = dictpointer[i];
        else
            sys[i] = sysdict[i];
    }

    GameIDType game_type = DetectGame(game_file);

    if (game_type == UNKNOWN_GAME)
        Fatal("Unsupported game!");

    if (game_type != SCOTTFREE && game_type != TI994A) {
        Options |= SPECTRUM_STYLE;
        split_screen = 1;
    } else {
        if (game_type != TI994A)
            Options |= TRS80_STYLE;
        split_screen = 1;
    }

    if (title_screen != NULL) {
        if (split_screen)
            PrintTitleScreenGrid();
        else
            PrintTitleScreenBuffer();
    }

    if (Options & TRS80_STYLE) {
        TopWidth = 64;
        TopHeight = 11;
    } else {
        TopWidth = 80;
        TopHeight = 10;
    }

    if (game_type == TI994A) {
        Display(Bottom, "In this adventure, you may abbreviate any word \
by typing its first %d letters, and directions by typing \
one letter.\n\nDo you want to restore previously saved game?\n",
            GameHeader.WordLength);
        if (YesOrNo())
            LoadGame();
        ClearScreen();
    }

    if (Game->type == US_VARIANT) {
        if (has_graphics()) {
            ImageWidth = 280;
            if (ImageHeight < 158)
                ImageHeight = 158;
            DrawTitleImage();
            OpenGraphicsWindow();
            sys[MESSAGE_DELIMITER] = " ";
            sys[ITEM_DELIMITER] = ". ";
            sys[EXITS_DELIMITER] = " ";
            sys[YOU_ARE] = "I am in a ";
            sys[YOU_SEE] = ". Visible: ";
            sys[INVENTORY] = "I've got: ";
            sys[NOTHING] = "Nothing at all. ";
            sys[EXITS] = "Some exits: ";
            if (CurrentSys == SYS_MSDOS)
                sys[WHAT_NOW] = "Command me? ";
            else
                sys[WHAT_NOW] = "What shall I do? ";
            sys[NORTH] = "NORTH";
            sys[EAST] = "EAST";
            sys[SOUTH] = "SOUTH";
            sys[WEST] = "WEST";
            sys[UP] = "UP";
            sys[DOWN] = "DOWN";

            Options |= PC_STYLE;
        } else {
            CurrentGame = SCOTTFREE;
            Game->type = NO_TYPE;
            game_type = SCOTTFREE;
        }
    }

    if (game_type == SCOTTFREE)
        Output("\
Scott Free, A Scott Adams game driver in C.\n\
Release 1.14, (c) 1993,1994,1995 Swansea University Computer Society.\n\
Distributed under the GNU software license\n\n");

    OpenTopWindow();

#ifdef SPATTERLIGHT
    UpdateSettings();
    if (gli_determinism)
        srand(1234);
    else
#endif
        srand((unsigned int)time(NULL));

    InitialState = SaveCurrentState();

    while (1) {
        glk_tick();

        if (should_restart)
            RestartGame();

        if (!StopTime)
            PerformActions(0, 0);
        if (!(CurrentCommand && CurrentCommand->allflag && !(CurrentCommand->allflag & LASTALL))) {
            print_look_to_transcript = should_look_in_transcript;
            Look();
            print_look_to_transcript = should_look_in_transcript = 0;
            if (!StopTime && !should_restart)
                SaveUndo();
        }

        if (should_restart)
            continue;

        if (GetInput(&vb, &no) == 1)
            continue;

        switch (PerformActions(vb, no)) {
        case ER_RAN_ALL_LINES_NO_MATCH:
            if (!RecheckForExtraCommand()) {
                Output(sys[I_DONT_UNDERSTAND]);
                FreeCommands();
            }
            break;
        case ER_RAN_ALL_LINES:
            Output(sys[YOU_CANT_DO_THAT_YET]);
            FreeCommands();
            break;
        default:
            JustStarted = 0;
        }

        /* Brian Howarth games seem to use -1 for forever */
        if (Items[LIGHT_SOURCE].Location != DESTROYED && GameHeader.LightTime != -1 && !StopTime) {
            GameHeader.LightTime--;
            if (GameHeader.LightTime < 1) {
                BitFlags |= (1 << LIGHTOUTBIT);
                if (Items[LIGHT_SOURCE].Location == CARRIED || Items[LIGHT_SOURCE].Location == MyLoc) {
                    Output(sys[LIGHT_HAS_RUN_OUT]);
                }
                if ((Options & PREHISTORIC_LAMP) || (Game->subtype & MYSTERIOUS) || CurrentGame == TI994A)
                    Items[LIGHT_SOURCE].Location = DESTROYED;
            } else if (GameHeader.LightTime < 25) {
                if (Items[LIGHT_SOURCE].Location == CARRIED || Items[LIGHT_SOURCE].Location == MyLoc) {
                    if ((Options & SCOTTLIGHT) || (Game->subtype & MYSTERIOUS)) {
                        Display(Bottom, "%s %d %s\n", sys[LIGHT_RUNS_OUT_IN], GameHeader.LightTime, sys[TURNS]);
                    } else {
                        if (GameHeader.LightTime % 5 == 0)
                            Output(sys[LIGHT_GROWING_DIM]);
                    }
                }
            }
        }
        if (StopTime)
            StopTime--;
    }
}
