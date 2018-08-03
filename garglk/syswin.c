/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Jesse McGrew.                    *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "glk.h"
#include "garglk.h"
#include "garversion.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <mmsystem.h>

static char *argv0;

#define ID_ABOUT       0x1000
#define ID_CONFIG      0x1001
#define ID_TOGSCR      0x1002
#define ID_FULLSCREEN  0x1003

static HWND hwndview, hwndframe;
static HDC hdc;
static BITMAPINFO *dibinf;
static void CALLBACK timeproc(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);
static LRESULT CALLBACK frameproc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK viewproc(HWND, UINT, WPARAM, LPARAM);
static HCURSOR idc_arrow, idc_hand, idc_ibeam;
static int switchingfullscreen = 0;

static MMRESULT timer = 0;
static int timerid = -1;
static volatile int timeouts = 0;

/* buffer for clipboard text */
static wchar_t *cliptext = NULL;
static int cliplen = 0;

/* filters for file dialogs */
static char *winfilters[] =
{
    "Saved game files (*.sav)\0*.sav\0All files (*.*)\0*.*\0\0",
    "Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0",
    "All files (*.*)\0*.*\0\0",
};

void glk_request_timer_events(glui32 millisecs)
{
    if (timerid != -1)
    {
        timeKillEvent(timer);
        timeEndPeriod(1);
        timerid = -1;
    }

    if (millisecs)
    {
        timeBeginPeriod(1);
        timer = timeSetEvent(millisecs, 0, timeproc, 0, TIME_PERIODIC);
        timerid = 1;
    }
}

void onabout(void)
{
    char txt[512];

    if (gli_program_info[0])
    {
    sprintf(txt,
        "Gargoyle by Tor Andersson   \n"
        "Build %s\n"
        "\n"
        "%s", VERSION, gli_program_info);
    }
    else
    {
    sprintf(txt,
        "Gargoyle by Tor Andersson   \n"
        "Build %s\n"
        "\n"
        "Interpeter: %s\n", VERSION, gli_program_name);
    }

    MessageBoxA(hwndframe, txt, " About", MB_OK);
}

void onconfig(void)
{
    char buf[256];
    strcpy(buf, argv0);
    if (strrchr(buf, '\\')) strrchr(buf, '\\')[1] = 0;
    if (strrchr(buf, '/')) strrchr(buf, '/')[1] = 0;
    strcat(buf, "garglk.ini");

    if (access(buf, R_OK))
    {
    char msg[1024];
    sprintf(msg, "There was no configuration file:    \n\n    %s    \n", buf);
    MessageBoxA(hwndframe, msg, " Configure", MB_ICONERROR);
    }
    else
    ShellExecute(hwndframe, "open", buf, 0, 0, SW_SHOWNORMAL);
}

void winabort(const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    MessageBoxA(NULL, buf, " Fatal error", MB_ICONERROR);
    abort();
}

void winexit(void)
{
    exit(0);
}

void winopenfile(char *prompt, char *buf, int len, int filter)
{
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwndframe;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = len;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = prompt;
    ofn.lpstrFilter = winfilters[filter];
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = ("");
    ofn.Flags = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;

    if (!GetOpenFileName(&ofn))
    strcpy(buf, "");
}

void winsavefile(char *prompt, char *buf, int len, int filter)
{
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwndframe;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = len;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = prompt;
    ofn.lpstrFilter = winfilters[filter];
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = ("");
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (!GetSaveFileName(&ofn))
    strcpy(buf, "");
}

void winclipstore(glui32 *text, int len)
{
    int i, k;

    i = 0;
    k = 0;

    if (cliptext)
        free(cliptext);

    cliptext = malloc(sizeof(wchar_t) * 2 * (len + 1));

    /* convert \n to \r\n */
    while (i < len) {
        if (text[i] == '\n') {
            cliptext[k] = '\r';
            cliptext[k+1] = '\n';
            k = k + 2;
        } else {
            cliptext[k] = text[i];
            k++;
        }
        i++;
    }

    /* null-terminated string */
    cliptext[k] = '\0';
    cliplen = k + 1;
}

void winclipsend(void)
{
    HGLOBAL wmem;
    void *wptr;

    if (!cliplen)
        return;

    wmem = GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE, cliplen * sizeof(wchar_t));
    wptr = GlobalLock(wmem);

    if (!wptr)
        return;

    memcpy(wptr, cliptext, cliplen * sizeof(wchar_t));
    GlobalUnlock(wmem);

    if(OpenClipboard(NULL)) {
        if (!SetClipboardData(CF_UNICODETEXT, wmem))
            GlobalFree(wmem);
        CloseClipboard(); 
    }
}

void winclipreceive(void)
{
    HGLOBAL rmem;
    wchar_t *rptr;
    int i, rlen;

    if(OpenClipboard(NULL)) {
        rmem = GetClipboardData(CF_UNICODETEXT);
        if (rmem && (rptr = GlobalLock(rmem))) {
            rlen = GlobalSize(rmem) / sizeof(wchar_t);
            for (i=0; i < rlen; i++) {
                if (rptr[i] == '\0')
                    break;
                else if (rptr[i] == '\r' || rptr[i] == '\n')
                    continue;
                else if (rptr[i] == '\b' || rptr[i] == '\t')
                    continue;
                else if (rptr[i] != 27)
                    gli_input_handle_key(rptr[i]);
            }
            GlobalUnlock(rmem);
        }
        CloseClipboard(); 
    }
}

void wininit(int *argc, char **argv)
{
    WNDCLASS wc;

    argv0 = argv[0];

    /* Create and register frame window class */
    wc.style = 0;
    wc.lpfnWndProc = frameproc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL); 
    wc.hIcon = LoadIcon(wc.hInstance, "IDI_GAPP");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = 0;
    wc.lpszMenuName = 0;
    wc.lpszClassName = "XxFrame";
    RegisterClass(&wc);

    /* Create and register view window class */
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = viewproc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = 0;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = 0;
    wc.lpszMenuName = 0;
    wc.lpszClassName = "XxView";
    RegisterClass(&wc);

    /* Init DIB info for buffer */
    dibinf = malloc(sizeof(BITMAPINFO) + 12);
    dibinf->bmiHeader.biSize = sizeof(dibinf->bmiHeader);
    dibinf->bmiHeader.biPlanes = 1;
    dibinf->bmiHeader.biBitCount = 24;
    dibinf->bmiHeader.biCompression = BI_RGB;
    dibinf->bmiHeader.biXPelsPerMeter = 2834;
    dibinf->bmiHeader.biYPelsPerMeter = 2834;
    dibinf->bmiHeader.biClrUsed = 0;
    dibinf->bmiHeader.biClrImportant = 0;
    dibinf->bmiHeader.biClrUsed = 0;

    /* Init cursors */
    idc_arrow = LoadCursor(NULL, IDC_ARROW);
    idc_hand = LoadCursor(NULL, IDC_HAND);
    idc_ibeam = LoadCursor(NULL, IDC_IBEAM);
}

void winopen()
{
    HMENU menu;

    int sizew = 0;
    int sizeh = 0;
    DWORD dwStyle = 0;

    if (gli_conf_fullscreen)
    {
        sizew = GetSystemMetrics(SM_CXFULLSCREEN);
        sizeh = GetSystemMetrics(SM_CYFULLSCREEN);
        dwStyle = WS_POPUP | WS_VISIBLE;
    }
    else
    {
        sizew = gli_wmarginx * 2 + gli_cellw * gli_cols;
        sizeh = gli_wmarginy * 2 + gli_cellh * gli_rows;

        sizew += GetSystemMetrics(SM_CXFRAME) * 2;
        sizeh += GetSystemMetrics(SM_CYFRAME) * 2;
        sizeh += GetSystemMetrics(SM_CYCAPTION);
        dwStyle = WS_CAPTION|WS_THICKFRAME|
            WS_SYSMENU|WS_MAXIMIZEBOX|WS_MINIMIZEBOX|
            WS_CLIPCHILDREN;
    }

    hwndframe = CreateWindow("XxFrame",
        NULL, // window caption
        dwStyle, // window style
        CW_USEDEFAULT, // initial x position
        CW_USEDEFAULT, // initial y position
        sizew, // initial x size
        sizeh, // initial y size
        NULL, // parent window handle
        NULL, // window menu handle
        0, //hInstance, // program instance handle
        NULL); // creation parameters

    hwndview = CreateWindow("XxView",
        NULL,
        WS_VISIBLE | WS_CHILD,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        hwndframe,
        NULL, NULL, 0);

    hdc = NULL;

    menu = GetSystemMenu(hwndframe, 0);
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(menu, MF_STRING, ID_ABOUT, "About Gargoyle...");
    AppendMenu(menu, MF_STRING, ID_CONFIG, "Options...");
    AppendMenu(menu, MF_STRING, ID_FULLSCREEN, "Fullscreen");
    // AppendMenu(menu, MF_STRING, ID_TOGSCR, "Toggle scrollbar");

    wintitle();

    if (gli_conf_fullscreen)
    {
        // See https://blogs.msdn.microsoft.com/oldnewthing/20050505-04/?p=35703
        HMONITOR hmon = MonitorFromWindow(hwndframe, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(hmon, &mi);
        SetWindowPos(hwndframe, 
            NULL,
            mi.rcMonitor.left,
            mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    else
        ShowWindow(hwndframe, SW_SHOW);
}

void onfullscreen()
{
    gli_conf_fullscreen = gli_conf_fullscreen ? 0 : 1;
    switchingfullscreen = 1;
    DestroyWindow(hwndframe);
    switchingfullscreen = 0;
    winopen();
    
}


void wintitle(void)
{
    char buf[256];

    if (strlen(gli_story_title))
        sprintf(buf, "%s", gli_story_title);
    else if (strlen(gli_story_name))
        sprintf(buf, "%s - %s", gli_story_name, gli_program_name);
    else
        sprintf(buf, "%s", gli_program_name);

    SetWindowTextA(hwndframe, buf);

    if (strcmp(gli_program_name, "Unknown"))
        sprintf(buf, "About Gargoyle / %s...", gli_program_name);
    else
        strcpy(buf, "About Gargoyle...");

    ModifyMenu(GetSystemMenu(hwndframe, 0), ID_ABOUT, MF_BYCOMMAND | MF_STRING, ID_ABOUT, buf);
    DrawMenuBar(hwndframe);
}

static void winblit(RECT r)
{
    int x0 = r.left;
    int y0 = r.top;
    int x1 = r.right;
    int y1 = r.bottom;

    dibinf->bmiHeader.biWidth = gli_image_w;
    dibinf->bmiHeader.biHeight = -gli_image_h;
    dibinf->bmiHeader.biSizeImage = gli_image_h * gli_image_s;

    SetDIBitsToDevice(hdc,
        x0, /* destx */
        y0, /* desty */
        x1 - x0, /* destw */
        y1 - y0, /* desth */
        x0, /* srcx */
        gli_image_h - y1, /* srcy */
        0, /* startscan */
        gli_image_h, /* numscans */
        gli_image_rgb, /* pBits */
        dibinf, /* pInfo */
        DIB_RGB_COLORS /* color use flag */
             );
}

void winrepaint(int x0, int y0, int x1, int y1)
{
    RECT wr;
    wr.left = x0; wr.right = x1;
    wr.top = y0; wr.bottom = y1;
    InvalidateRect(hwndview, &wr, 1); // 0);
}

void gli_select(event_t *event, int polled)
{
    MSG msg;

    gli_curevent = event;
    gli_event_clearevent(event);

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    gli_dispatch_event(gli_curevent, polled);

    if (!polled)
    {
        while (gli_curevent->type == evtype_None && !timeouts)
        {
            int code = GetMessage(&msg, NULL, 0, 0);
            if (code < 0)
                exit(1);
            if (code > 0)
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            gli_dispatch_event(gli_curevent, polled);
        }
    }

    if (gli_curevent->type == evtype_None && timeouts)
    {
        gli_event_store(evtype_Timer, NULL, 0, 0);
        gli_dispatch_event(gli_curevent, polled);
        timeouts = 0;
    }

    gli_curevent = NULL;
}

void winresize(void)
{
    int xw, xh;
    int w, h;

    xw = gli_wmarginx * 2;
    xh = gli_wmarginy * 2;
    gli_calc_padding(gli_rootwin, &xw, &xh);
    xw += GetSystemMetrics(SM_CXFRAME) * 2;
    xh += GetSystemMetrics(SM_CYFRAME) * 2;
    xh += GetSystemMetrics(SM_CYCAPTION);

    w = (gli_cols * gli_cellw) + xw;
    h = (gli_rows * gli_cellh) + xh;

    if (w != gli_image_w || h != gli_image_h)
    SetWindowPos(hwndframe, 0, 0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE);
}

void CALLBACK timeproc(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
    PostMessage(hwndframe, WM_TIMER, 0, 0);
}

LRESULT CALLBACK
frameproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (switchingfullscreen)
        return 0;

    switch(message)
    {
    case WM_SETFOCUS:
    PostMessage(hwnd, WM_APP+5, 0, 0);
    return 0;
    case WM_APP+5:
    SetFocus(hwndview);
    return 0;

    case WM_DESTROY:
    PostQuitMessage(0);
    exit(1);
    break;

    case WM_SIZE:
    {
        // More generally, we should use GetEffectiveClientRect
        // if you have a toolbar etc.
        RECT rect;
        GetClientRect(hwnd, &rect);
        MoveWindow(hwndview, rect.left, rect.top,
            rect.right-rect.left, rect.bottom-rect.top, TRUE);
    }
    return 0;

    case WM_SIZING:
    if (0) {
        RECT *r = (RECT*)lParam;
        int w, h;
        int cw, ch;
        int xw, xh;

        xw = gli_wmarginx * 2;
        xh = gli_wmarginy * 2;
        gli_calc_padding(gli_rootwin, &xw, &xh);
        xw += GetSystemMetrics(SM_CXFRAME) * 2;
        xh += GetSystemMetrics(SM_CYFRAME) * 2;
        xh += GetSystemMetrics(SM_CYCAPTION);

        w = r->right - r->left - xw;
        h = r->bottom - r->top - xh;

        cw = w / gli_cellw;
        ch = h / gli_cellh;

        if (ch < 10) ch = 10;
        if (cw < 30) cw = 30;
        if (ch > 200) ch = 200;
        if (cw > 250) cw = 250;

        w = (cw * gli_cellw) + xw;
        h = (ch * gli_cellh) + xh;

        if (wParam == WMSZ_TOPRIGHT ||
            wParam == WMSZ_RIGHT ||
            wParam == WMSZ_BOTTOMRIGHT)
        r->right = r->left + w;
        else
        r->left = r->right - w;

        if (wParam == WMSZ_BOTTOMLEFT ||
            wParam == WMSZ_BOTTOM ||
            wParam == WMSZ_BOTTOMRIGHT)
        r->bottom = r->top + h;
        else
        r->top = r->bottom - h;

        return 1;
    }
    break;

    case WM_SYSCOMMAND:
    if (wParam == ID_ABOUT)
    {
        onabout();
        return 0;
    }
    if (wParam == ID_CONFIG)
    {
        onconfig();
        return 0;
    }
    if (wParam == ID_TOGSCR)
    {
        if (gli_scroll_width)
        gli_scroll_width = 0;
        else
        gli_scroll_width = 8;
        gli_force_redraw = 1;
        gli_windows_size_change();
        return 0;
    }
    if (wParam == ID_FULLSCREEN)
    {
        onfullscreen();
        return 0;
    }
    break;

    case WM_TIMER:
    timeouts ++;
    return 0;

    case WM_NOTIFY:
    case WM_COMMAND:
    return SendMessage(hwndview, message, wParam, lParam);
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

#ifndef WM_UNICHAR
#define WM_UNICHAR 0x0109
#endif

#ifndef UNICODE_NOCHAR
#define UNICODE_NOCHAR 0xFFFF
#endif

#define Uni_IsSurrogate1(ch) ((ch) >= 0xD800 && (ch) <= 0xDBFF)
#define Uni_IsSurrogate2(ch) ((ch) >= 0xDC00 && (ch) <= 0xDFFF)

#define Uni_SurrogateToUTF32(ch, cl) (((ch) - 0xD800) * 0x400 + ((cl) - 0xDC00) + 0x10000)

#define Uni_UTF32ToSurrogate1(ch) (((ch) - 0x10000) / 0x400 + 0xD800)
#define Uni_UTF32ToSurrogate2(ch) (((ch) - 0x10000) % 0x400 + 0xDC00)

LRESULT CALLBACK
viewproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int x = (signed short) LOWORD(lParam);
    int y = (signed short) HIWORD(lParam);
    glui32 key;

    switch (message)
    {
    case WM_ERASEBKGND:
    return 1; // don't erase; we'll repaint it all

    case WM_PAINT:
    {
        PAINTSTRUCT ps;

        /* make sure we have a fresh bitmap */
        if (!gli_drawselect)
            gli_windows_redraw();
        else
            gli_drawselect = FALSE;

        /* and blit it to the screen */
        hdc = BeginPaint(hwnd, &ps);
        winblit(ps.rcPaint);
        hdc = NULL;
        EndPaint(hwnd, &ps);

        return 0;
    }

    case WM_SIZE:
    {
        int newwid = LOWORD(lParam);
        int newhgt = HIWORD(lParam);

        if (newwid == 0 || newhgt == 0)
        break;

        if (newwid == gli_image_w && newhgt == gli_image_h)
        break;

        gli_image_w = newwid;
        gli_image_h = newhgt;

        gli_resize_mask(gli_image_w, gli_image_h);

        gli_image_s = ((gli_image_w * 3 + 3) / 4) * 4;
        if (gli_image_rgb)
        free(gli_image_rgb);
        gli_image_rgb = malloc(gli_image_s * gli_image_h);

        gli_force_redraw = 1;

        gli_windows_size_change();

        break;
    }

    case WM_LBUTTONDOWN:
    {
        SetFocus(hwndview);
        gli_input_handle_click(x, y);
        return 0;
    }

    case WM_LBUTTONUP:
    {
        gli_copyselect = FALSE;
        SetCursor(idc_arrow);
        return 0;
    }

    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        SetFocus(hwndview);
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
            gli_input_handle_key(keycode_MouseWheelUp);
        else
            gli_input_handle_key(keycode_MouseWheelDown);
    }

    case WM_CAPTURECHANGED:
    {
        gli_copyselect = FALSE;
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        /* catch and release */
        RECT rect;
        POINT pt = { x, y };
        GetClientRect(hwnd, &rect);
        int hover = PtInRect(&rect, pt);

        if (!hover) {
            if (GetCapture() == hwnd)
                ReleaseCapture();
        } else {
            if (GetCapture() != hwnd ) {
                SetCapture(hwnd);
            }
            if (gli_copyselect) {
                SetCursor(idc_ibeam);
                gli_move_selection(x, y);
            } else {
                if (gli_get_hyperlink(x, y)) {
                    SetCursor(idc_hand);
                } else {
                    SetCursor(idc_arrow);
                }
            }
        }

        return 0;
    }

    case WM_COPY:
    {
        gli_copyselect = FALSE;
        SetCursor(idc_arrow);
        winclipsend();
        return 0;
    }

    case WM_PASTE:
    {
        SetFocus(hwndview);
        winclipreceive();
        return 0;
    }
    
    case WM_SYSKEYDOWN:
    {
        if (wParam == VK_RETURN && (HIWORD(lParam) & KF_ALTDOWN))
        {
            onfullscreen();
            return 0;
        }
        break;
    }

    case WM_KEYDOWN:
    {
        if (GetKeyState(VK_CONTROL) < 0)
        {
            switch (wParam)
            {
            case VK_LEFT: gli_input_handle_key(keycode_SkipWordLeft); break;
            case VK_RIGHT: gli_input_handle_key(keycode_SkipWordRight); break;
            }
        }
        else
        {
            switch (wParam)
            {
            case VK_PRIOR: gli_input_handle_key(keycode_PageUp); break;
            case VK_NEXT: gli_input_handle_key(keycode_PageDown); break;
            case VK_HOME: gli_input_handle_key(keycode_Home); break;
            case VK_END: gli_input_handle_key(keycode_End); break;
            case VK_LEFT: gli_input_handle_key(keycode_Left); break;
            case VK_RIGHT: gli_input_handle_key(keycode_Right); break;
            case VK_UP: gli_input_handle_key(keycode_Up); break;
            case VK_DOWN: gli_input_handle_key(keycode_Down); break;
            case VK_ESCAPE: gli_input_handle_key(keycode_Escape); break;
            case VK_DELETE: gli_input_handle_key(keycode_Erase); break;
            case VK_F1: gli_input_handle_key(keycode_Func1); break;
            case VK_F2: gli_input_handle_key(keycode_Func2); break;
            case VK_F3: gli_input_handle_key(keycode_Func3); break;
            case VK_F4: gli_input_handle_key(keycode_Func4); break;
            case VK_F5: gli_input_handle_key(keycode_Func5); break;
            case VK_F6: gli_input_handle_key(keycode_Func6); break;
            case VK_F7: gli_input_handle_key(keycode_Func7); break;
            case VK_F8: gli_input_handle_key(keycode_Func8); break;
            case VK_F9: gli_input_handle_key(keycode_Func9); break;
            case VK_F10: gli_input_handle_key(keycode_Func10); break;
            case VK_F11: gli_input_handle_key(keycode_Func11); break;
            case VK_F12: gli_input_handle_key(keycode_Func12); break;
            }
        }
        return 0;
    }

    /* unicode encoded chars, including escape, backspace etc... */
    case WM_UNICHAR:
        key = wParam;

        if (key == UNICODE_NOCHAR)
            return 1; /* yes, we like WM_UNICHAR */

        if (key == '\r' || key == '\n')
            gli_input_handle_key(keycode_Return);
        else if (key == '\b')
            gli_input_handle_key(keycode_Delete);
        else if (key == '\t')
            gli_input_handle_key(keycode_Tab);
        else if (key == 0x03 || key == 0x18)
            SendMessage(hwndview, WM_COPY, 0, 0);
        else if (key == 0x16)
            SendMessage(hwndview, WM_PASTE, 0, 0);
        else if (key != 27)
            gli_input_handle_key(key);

        return 0;

    case WM_CHAR:
        key = wParam;

        if (key == '\r' || key == '\n')
            gli_input_handle_key(keycode_Return);
        else if (key == '\b')
            gli_input_handle_key(keycode_Delete);
        else if (key == '\t')
            gli_input_handle_key(keycode_Tab);
        else if (key == 0x03 || key == 0x18)
            SendMessage(hwndview, WM_COPY, 0, 0);
        else if (key == 0x16)
            SendMessage(hwndview, WM_PASTE, 0, 0);
        else if (key != 27) {
            /* translate from ANSI code page to Unicode */
            char ansich = (char)key;
            wchar_t widebuf[2];
            int res = MultiByteToWideChar(CP_ACP, 0, &ansich, 1, widebuf, 2);
            if (res) {
                if (Uni_IsSurrogate1(widebuf[0]))
                    key = Uni_SurrogateToUTF32(widebuf[0], widebuf[1]);
                else
                    key = widebuf[0];
                gli_input_handle_key(key);
            }
        }

        return 0;
    }

    /* Pass on unhandled events to Windows */
    return DefWindowProc(hwnd, message, wParam, lParam);
}

/* monotonic clock time for profiling */
void wincounter(glktimeval_t *time)
{
    static double gli_second_res = 0;
    if (!gli_second_res)
    {
        LARGE_INTEGER res;
        QueryPerformanceFrequency(&res);
        gli_second_res = (double) res.QuadPart;
    }

    LARGE_INTEGER tick;
    QueryPerformanceCounter(&tick);

    double sec = (double) tick.QuadPart / gli_second_res;
    double mic = (double) tick.QuadPart / (gli_second_res / 1000000);

    time->high_sec = 0;
    time->low_sec  = (unsigned int) sec;
    time->microsec = (unsigned int) fmod(mic, 1000000);
}
