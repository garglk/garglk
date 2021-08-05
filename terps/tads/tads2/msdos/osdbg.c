#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/OSDBG.C,v 1.2 1999/05/17 02:52:18 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1990, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osdbg  - os routines for debugger
Function
  Implements os routines for windowing DOS debugger.
Notes
  None
Modified
  04/21/92 MJRoberts     - creation (from tads 1.2 osgen.c)
*/

#ifdef T_WIN32
#include <windows.h>
#include <wincon.h>
#endif

#ifdef DJGPP
#include <dpmi.h>
#endif

#include <stdarg.h>
#include "os.h"
#include "osgen.h"
#include "std.h"
#include "osdbg.h"

#ifdef T_WIN32

/*
 *   For Win32, use the console API to access the screen.  Keep track of
 *   two console handles - the standard one, and the new one we create for
 *   the debugger screen.  [0] is the standard handle, which we use for
 *   the game screen, and [1] is the debugger screen.  
 */
HANDLE G_screen_bufhdl[2];

/* global with current output handle - see ossdos32.c */
extern HANDLE G_out_bufhdl;

#endif /* T_WIN32 */

#ifdef MSOS2
# define INCL_VIO
# include <os2.h>
#endif /* MSOS2 */

/* adjust assembler keywords */
# if _MSC_VER >= 700
# define interrupt __interrupt
# define asm       __asm
# else
# ifndef DJGPP
#  define interrupt _interrupt
#  define asm       _asm
# endif /* !DJGPP */
# endif /* _MSC_VER >= 700 */


/* clear a window */
void osdbgclr(oswdef *win)
{
    ossclr(win->oswy1, win->oswx1, win->oswy2, win->oswx2, win->oswcolor);
}

#ifdef MSOS2
typedef struct pagedef
{
    USHORT  row;
    USHORT  col;
    size_t  size;
    BYTE    cells[2];
} pagedef;

pagedef *pages[2];

int osdbgini(int rows, int cols)
{
    size_t size = 2 * rows * cols;

    pages[0] = (pagedef *)malloc(size + sizeof(pagedef) - 2);
    pages[1] = (pagedef *)malloc(size + sizeof(pagedef) - 2);
    pages[0]->row = pages[1]->row = 0;
    pages[0]->col = pages[1]->col = 0;
    pages[0]->size = pages[1]->size = size;
    memset(pages[0]->cells, 0, size);
    memset(pages[1]->cells, 0, size);
    return(0);
}
#else /* MSOS2 */
#ifdef T_WIN32

int osdbgini(int rows, int cols)
{
    /* 
     *   initialize our array of output screen buffer handles -- the first
     *   entry is the default entry, which we use for the game screen; the
     *   second entry is a new handle we create, which we use for the
     *   debug screen 
     */
    G_screen_bufhdl[0] = G_out_bufhdl;
    G_screen_bufhdl[1] = CreateConsoleScreenBuffer(GENERIC_WRITE, 0,
        0, CONSOLE_TEXTMODE_BUFFER, 0);

    /* success */
    return 0;
}

#else /* T_WIN32 */

int osdbgini(int rows, int cols)
{
    return(0);
}

#endif /* T_WIN32 */
#endif /* MSOS2 */

#ifndef DJGPP

int ossvpg(char pg)
{
    static int  prvpg;
#ifdef MSOS2
           USHORT siz;
#else /* MSOS2 */
    extern long scrbase;
#endif /* MSOS2 */
           int  ret;
    
    if (pg == prvpg) return(prvpg);
#ifdef MSOS2
    siz = (USHORT)pages[prvpg]->size;
    VioReadCellStr(pages[prvpg]->cells, &siz, (USHORT)0, (USHORT)0, (HVIO)0);
    VioGetCurPos(&pages[prvpg]->row, &pages[prvpg]->col, (HVIO)0);

    VioWrtCellStr(pages[pg]->cells, (USHORT)pages[pg]->size,
                  (USHORT)0, (USHORT)0, (HVIO)0);
    VioSetCurPos(pages[pg]->row, pages[pg]->col, (HVIO)0);
#endif /* MSOS2 */
    ret = prvpg;
    prvpg = pg;
    
#ifdef T_WIN32
    /* set ossdos32.c's output handle to the selected page's handle */
    G_out_bufhdl = G_screen_bufhdl[pg];

    /* show the selected page */
    SetConsoleActiveScreenBuffer(G_out_bufhdl);
#else /* T_WIN32 */
#ifndef MSOS2
    asm {
        push    bp
        mov     ah, 5
        mov     al, pg
        int     0x10

        mov     ax, (25 * 80 * 2) + 96
        cmp     pg, 0
        jne     ossvpg_1
        neg     ax
    }
ossvpg_1:
    asm {
        add     word ptr [scrbase], ax
        pop     bp
    }
#endif /* !MSOS2 */
#endif /* T_WIN32 */
    return(ret);
}

#endif /* !DJGPP */

/* scroll a window up a line */
static void osdbgsc(oswdef *win)
{
    ossscr(win->oswy1, win->oswx1, win->oswy2, win->oswx2, win->oswcolor);
}

/* print a string in a debugger window */
void osdbgpt(oswdef *win, char *fmt, ...)
{
    char     buf[256];
    va_list  argptr;
    char    *p;

    va_start(argptr, fmt);
    vsprintf(buf, fmt, argptr);
    va_end(argptr);

    for (p=buf ; *p ; )
    {
        char *p1;

        if ((win->oswflg & OSWFMORE) && win->oswx == win->oswx1 &&
            win->oswmore+1 >= win->oswy2 - win->oswy1)
        {
            char c;
            int eof;
            
            ossdsp(win->oswy, win->oswx, win->oswcolor, "[More]");
            ossdbgloc((char)win->oswy, (char)(win->oswx+6));
            eof = FALSE;
            do
            {
                switch(c = os_getc())
                {
                case '\n':
                case '\r':
                    win->oswmore--;
                    break;

                case ' ':
                    win->oswmore = 0;
                    break;

                case 0:
                    if (os_getc() == CMD_EOF)
                    {
                        win->oswmore = 0;
                        eof = TRUE;
                    }
                    break;
                }
            } while (c != ' ' && c != '\n' && c != '\r' && !eof);
                
            ossdsp(win->oswy, win->oswx, win->oswcolor, "      ");
        }

        for (p1 = p ; *p1 && *p1 != '\n' && *p1 != '\r' && *p1 != '\t'; p1++);
        if (*p1 == '\n' || *p1 == '\r' || *p1 == '\t')
        {
            int c = *p1;
            
            *p1 = '\0';

            if ((size_t)win->oswx + strlen(p) > (size_t)win->oswx2 &&
                (win->oswflg & OSWFCLIP))
                p[win->oswx2 - win->oswx + 1] = '\0';
            ossdsp(win->oswy, win->oswx, win->oswcolor, p);

            if (c == '\n')
            {
                ++(win->oswy);
                win->oswx = win->oswx1;
                if (win->oswy > win->oswy2)
                {
                    win->oswy = win->oswy2;
                    osdbgsc(win);
                }
                win->oswmore++;
            }
            else if (c == '\t')
            {
                win->oswx += strlen(p);
                do
                {
                    ossdsp(win->oswy, win->oswx, win->oswcolor, " ");
                    ++(win->oswx);
                    if (win->oswx > win->oswx2 && (win->oswflg & OSWFCLIP))
                        break;
                } while ((win->oswx - 2) & 7);
            }
            p = p1 + 1;
            if (win->oswx > win->oswx2) return;
        }
        else
        {
            if ((size_t)win->oswx + strlen(p) > (size_t)win->oswx2
                && (win->oswflg & OSWFCLIP))
                p[win->oswx2 - win->oswx + 1] = '\0';
            ossdsp(win->oswy, win->oswx, win->oswcolor, p);
            win->oswx += strlen(p);
            p = p1;
        }
    }
}

/* open a window - set up location */
void osdbgwop(oswdef *win, int x1, int y1, int x2, int y2, int color)
{
    win->oswx1 = win->oswx = x1;
    win->oswx2 = x2;
    win->oswy1 = win->oswy = y1;
    win->oswy2 = y2;
    win->oswcolor = color;
    win->oswflg = 0;
}

/* locate the cursor */
void ossdbgloc(char y, char x)
{
#if defined(MSOS2) || defined(T_WIN32)
    ossloc(y, x);
#else /* !MSOS2 */
# ifdef DJGPP
    __dpmi_regs regs;

    regs.h.dh = y;
    regs.h.dl = x;
    regs.h.ah = 2;
    regs.h.bh = 1;
    __dpmi_int(0x10, &regs);
# else /* DJGPP */
    asm {
         push bp;
         mov  dh, y;
         mov  dl, x; 
         mov  ah, 2;
         mov  bh, 1;
         int  0x10;
         pop  bp;
    }
# endif /* DJGPP */
#endif /* MSOS2 */
}

/* get some text */
int osdbggts(oswdef *win, char *buf, int (*cmdfn)(void *, char), void *cmdctx)
{
    char *p = buf;
    char *eol = buf;
    char *eob = buf + 127;
    int   x = win->oswx;
    int   y = win->oswy;
    int   cmd = 0;

    win->oswmore = 0;
    for (buf[0] = '\0' ; ; )
    {
        char c;

        ossdbgloc((char)y, (char)x);
        switch(c = os_getc())
        {
        case 8:
            if (p > buf)
            {
                char *q;
                char  tmpbuf[2];
                int   thisx, thisy;
                
                for ( q=(--p) ; q<eol ; q++ ) *q = *( q+1 );
                eol--;
                if ( --x < 0 )
                {
                    x = win->oswx2;
                    y--;
                }
                *eol = ' ';
                thisx = x;
                thisy = y;
                for ( q=p, tmpbuf[1]='\0' ; q<=eol ; q++ )
                {
                    tmpbuf[0] = *q;
                    ossdsp( thisy, thisx, win->oswcolor, tmpbuf );
                    if ( ++thisx > win->oswx2 )
                    {
                        thisx = 0;
                        thisy++;
                    }
                }
                *eol = '\0';
            }
            break;

        case 13:
            /*
             *   Scroll the screen to account for the carriage return,
             *   position the cursor at the end of the new line, and
             *   null-terminate the line.  
             */
            *eol = '\0';
            while( p != eol )
            {
                p++;
                if ( ++x > win->oswx2 )
                {
                    y++;
                    x = 0;
                }
            }
            
            if ( y == win->oswy2 ) osdbgsc(win);
            else ++y;
            x = 0;
            ossdbgloc((char)y, (char)x);
            
            /*
             *   Finally, copy the buffer to the screen save buffer (if
             *   applicable), and return the contents of the buffer.  Note
             *   that we add an extra carriage return if we were already on
             *   the last line, since we scrolled the screen in this case;
             *   otherwise, ossaddsbe will add all the blank lines that are
             *   necessary.  
             */
            win->oswx = x;
            win->oswy = y;
            return(0);
            
        case 0:
            switch(c = os_getc())
            {
            case CMD_EOF:
                return 0;
                
            case CMD_LEFT:
                if ( p>buf )
                {
                    p--;
                    x--;
                    if ( x < 0 )
                    {
                        x = win->oswx2;
                        y--;
                    }
                }
                break;

            case CMD_RIGHT:
                if ( p<eol )
                {
                    p++;
                    x++;
                    if ( x > win->oswx2 )
                    {
                        x = 0;
                        y++;
                    }
                }
                break;
            case CMD_DEL:
                if ( p<eol )
                {
                    char *q;
                    char  tmpbuf[2];
                    int   thisx=x, thisy=y;
                    
                    for ( q=p ; q<eol ; q++ ) *q = *(q+1);
                    eol--;
                    *eol = ' ';
                    for ( q=p, tmpbuf[1]='\0' ; q<=eol ; q++ )
                    {
                        tmpbuf[0] = *q;
                        ossdsp( thisy, thisx, win->oswcolor, tmpbuf );
                        if ( ++thisx > win->oswx2 )
                        {
                            thisx = 0;
                            thisy++;
                        }
                    }
                    *eol = '\0';
                }
                break;
            case CMD_KILL:
            case CMD_HOME:
            do_kill:
                while( p>buf )
                {
                    p--;
                    if ( --x < 0 )
                    {
                        x = win->oswx2;
                        y--;
                    }
                }
                if ( c == CMD_HOME ) break;
                /*
                 *   We're at the start of the line now; fall through for
                 *   KILL, UP, and DOWN to the code which deletes to the
                 *   end of the line.  
                 */
            case CMD_DEOL:
                if ( p<eol )
                {
                    char *q;
                    int   thisx=x, thisy=y;
                    
                    for ( q=p ; q<eol ; q++ )
                    {
                        ossdsp( thisy, thisx, win->oswcolor, " " );
                        if ( ++thisx > win->oswx2 )
                        {
                            thisx = 0;
                            thisy++;
                        }
                    }
                    eol = p;
                    *p = '\0';
                }
                if (cmd) return(cmd);
                break;
            case CMD_END:
                while ( p<eol )
                {
                    p++;
                    if ( ++x > win->oswx2 )
                    {
                        x = 0;
                        y++;
                    }
                }
                break;
                
            default:
                if (cmd = (*cmdfn)(cmdctx, c))
                {
                    c = CMD_KILL;
                    goto do_kill;
                }
                break;
            }
            break;
        default:
            if ( c >= ' ' && c < 127 && eol<eob )
            {
                if ( p != eol )
                {
                    char *q;
                    int   thisy=y, thisx=x;
                    char  tmpbuf[2];
                    
                    for ( q=(++eol) ; q>p ; q-- ) *q=*(q-1);
                    *p = c;
                    for ( q=p++, tmpbuf[1] = '\0' ; q<eol ; q++ )
                    {
                        tmpbuf[0] = *q;
                        ossdsp( thisy, thisx, win->oswcolor, tmpbuf );
                        thisx++;
                        if ( thisx > win->oswx2 )
                        {
                            thisx = 0;
                            if ( thisy == win->oswy2 )
                            {
                                y--;
                                osdbgsc(win);
                            }
                            else thisy++;
                        }
                    }
                    if ( ++x > win->oswx2 )
                    {
                        y++;
                        x = 0;
                    }
                }
                else
                {
                    *p++ = c;
                    *p = '\0';
                    eol++;
                    ossdsp( y, x, win->oswcolor, p-1 );
                    if ( ++x > win->oswx2 )
                    {
                        x = 0;
                        if ( y == win->oswy2 )
                            osdbgsc(win);
                        else y++;
                    }
                }
            }
            break;
        }
    }
}
