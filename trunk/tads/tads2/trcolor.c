#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/TRCOLOR.C,v 1.2 1999/05/17 02:52:13 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1993, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  trcolor.c - set TADS Run-time color scheme
Function
  This is a little tool that lets the user interactively specify the
  color scheme used by the runtime.
Notes
  This program creates or modifies a file called TRCOLOR.DAT in the current
  directory.  This file is read by the runtime at startup to set the color
  scheme.

  This tool is DOS-specific - it uses the DOS oss-layer display routines
  directly (these routines are not part of any portable interface and
  exist only on DOS and some derivative ports that use the DOS os-layer
  implementation).
Modified
  04/06/93 MJRoberts - Creation
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef DJGPP
#include <dos.h>
#include <conio.h>
#include <bios.h>
#endif /* DJGPP */

#include "os.h"

#define TRUE  1
#define FALSE 0

int   usebios = 1;
char *scrbase = (char *)0xb8000000;

/* color box location */
#define CYOFS  6
#define CXOFS  48

/* title location */
#define TYOFS  8
#define TXOFS  17

/* title normal/highlight color */
#define TNORMAL     7
#define THILITE  0x70

/* number of colors */
#define COLORCNT  3

static void hilite(int cur, int flag)
{
    ossdsp((cur / 16) + CYOFS, (cur % 16) + CXOFS,
           cur + (flag ? 128 : 0), flag ? "X" : "+");
}

static void hilitetitle(int num, char *text, int color, int flag)
{
    ossdsp(TYOFS + num*2, TXOFS - 3, TNORMAL, flag ? ">>" : "  ");
    ossdsp(TYOFS + num*2, TXOFS, color, text);
    ossdsp(TYOFS + num*2, TXOFS + strlen(text) + 1, TNORMAL,
           flag ? "<<" : "  ");
}


int main(int argc, char **argv)
{
    FILE  *fp;
    int    i;
    static char *title[] = { "Normal Text", "Bold Text", "Status Line" };
    int    colors[COLORCNT];
    int    cur;

    /* initialize the console */
    oss_con_init();

    /* use a different default color scheme for monochrome */
    if (ossmon())
        colors[2] = 0x70;
    else
        colors[2] = 23;
    colors[0] = 7;
    colors[1] = 15;

    /* read existing trcolor.dat if present */
    if (fp = fopen("trcolor.dat", "r"))
    {
        for (i = 0 ; i < COLORCNT ; ++i)
        {
            char buf[128];

            fgets(buf, sizeof(buf), fp);
            colors[i] = atoi(buf);
        }
        fclose(fp);
    }

    /* set up the screen */
    ossclr(0, 0, 25, 80, 0);
    ossdsp(3, 8, TNORMAL,
           "TADS Runtime Color Setup - Copyright (c) 1993, 2007 "
           "Michael J. Roberts");                  /* copyright-date-string */
    for (i = 0 ; i < 128 ; ++i)
        ossdsp((i / 16) + CYOFS, (i % 16) + CXOFS, i, "+");
    for (i = 0 ; i < COLORCNT ; ++i)
        hilitetitle(i, title[i], colors[i], i == 0);

    /* show instructions */
    ossdsp(16, 17, TNORMAL, "Use Tab and Shift-Tab to select text types.");
    ossdsp(17, 17, TNORMAL, "Use cursor arrows to change colors.");
    ossdsp(18, 17, TNORMAL, "Press S to save changes and exit.");
    ossdsp(19, 17, TNORMAL, "Press Esc or Q to quit without saving changes.");

    /* hilite current color */
    hilite(colors[0], TRUE);

    for (cur = 0 ;; )
    {
        ossloc(0, 0);
        switch(getch())
        {
        case 0:
            switch(getch())
            {
            case 0115:                                       /* right arrow */
                hilite(colors[cur], FALSE);
                if (++colors[cur] >= 128) colors[cur] = 0;
                hilite(colors[cur], TRUE);
                hilitetitle(cur, title[cur], colors[cur], TRUE);
                break;

            case 0113:                                        /* left arrow */
                hilite(colors[cur], FALSE);
                if (--colors[cur] <= 0) colors[cur] = 127;
                hilite(colors[cur], TRUE);
                hilitetitle(cur, title[cur], colors[cur], TRUE);
                break;

            case 0120:                                        /* down arrow */
                hilite(colors[cur], FALSE);
                if ((colors[cur] += 16) >= 128) colors[cur] -= 128;
                hilite(colors[cur], TRUE);              
                hilitetitle(cur, title[cur], colors[cur], TRUE);
                break;

            case 0110:                                          /* up arrow */
                hilite(colors[cur], FALSE);
                if ((colors[cur] -= 16) <= 0) colors[cur] += 128;
                hilite(colors[cur], TRUE);
                hilitetitle(cur, title[cur], colors[cur], TRUE);
                break;

            case 15:                                             /* backtab */
                /* unhilite current color */
                hilitetitle(cur, title[cur], colors[cur], FALSE);
                hilite(colors[cur], FALSE);

                /* select new color and hilite it */
                if (--cur < 0) cur = COLORCNT - 1;
                hilitetitle(cur, title[cur], colors[cur], TRUE);
                hilite(colors[cur], TRUE);
                break;
            }
            break;

        case 9:
            /* unhilite current color */
            hilitetitle(cur, title[cur], colors[cur], FALSE);
            hilite(colors[cur], FALSE);
            
            /* select new color and hilite it */
            if (++cur >= COLORCNT) cur = 0;
            hilitetitle(cur, title[cur], colors[cur], TRUE);
            hilite(colors[cur], TRUE);
            break;

        case 27:
        case 'q':
        case 'Q':
            ossloc(24, 0);
            oss_con_uninit();
            printf("\n\nNo changes saved.\n");
            exit(0);

        case 's':
        case 'S':
            ossloc(24, 0);
            printf("\n\n");
            if (!(fp = fopen("trcolor.dat", "w")))
            {
                oss_con_uninit();
                printf("Error - unable to create TRCOLOR.DAT\n");
                exit(1);
            }
            
            for (i = 0 ; i < COLORCNT ; ++i)
                fprintf(fp, "%d\n", colors[i]);
            fclose(fp);

            oss_con_uninit();
            printf("TRCOLOR.DAT successfully updated!\n");
            exit(0);
        }
    }

    return 0;
}
