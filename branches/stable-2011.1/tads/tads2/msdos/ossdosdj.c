/*
Name
  ossdosdj.c - ossdos functions for DJGPP
Function
  
Notes
  
Modified
 */

#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <stdio.h>

/*
 * BIOS video functions.
 */
#define BIOSINT 0x10
/*
 * BIOS video function numbers.
 */
#define B_MOVECURSOR 2                  /* Move cursor. */
#define B_UPSCROLL 6                    /* Scroll window up. */
#define B_UPSCROLL_1 0x0601             /* Scroll window up 1 line. */
#define B_DOWNSCROLL 7                  /* Scroll window down. */
#define B_DOWNSCROLL_1 0x0701           /* Scroll window down 1 line. */
#define B_CLEAR_REGION 0x0600           /* Clear region of screen. */
#define B_WRITECHAR 9                   /* Write character & attribute at */
                                        /* current position. */
#define B_TTYWRITE 0x0e                 /* Teletype-style write. */
#define B_GETMODE 0x0f                  /* Get video mode. */
#define B_SETPALETTE 0x10               /* Set palette registers. */
#define B_EGACGEN 0x11                  /* EGA character generator. */
#define B_VCONFIG 0x12                  /* Video subsystem configuration. */


/*
 * Subfunctions for EGA character generator.
 */
#define B_EGATEST 0x30

/*
 * BIOS function to get equipment list.
 */
#define BIOSEQUIP 0x11

/*
 * Default number of rows in text modes.
 */
#define DEF_ROW_MAX 24

/*
 * Value returned by biosequip (in ax) to indicate a mono display.
 */
#define EQUIP_MONO 0x30

/*
 * Video modes.
 */
#define BWT25X80 2              /* 25 x 80 black & white (CGA) text. */
#define CT25X80 3               /* 25 x 80 colour text. */
#define MT25X80 7               /* 25 x 80 monochrome (MDA) text. */

union vcell {
    struct {
        char vchar;
        unsigned char vattr;
    } ca;
    unsigned short s;
};

unsigned long scrbase;
static char col_max, row_max;   /* Screen dimensions. */
static char video_mode;
static char video_page;

extern int usebios;

/* 
 * Local copy of usebios because it gets overwritten by dbgu.c:dbguini() 
 */
static int bios_flag;

/* initialize console */
void oss_con_init(void)
{
    int ega = 0;
    __dpmi_regs regs;

    regs.x.bx = regs.x.dx = 0;
    regs.h.ah = B_EGACGEN;
    regs.h.al = B_EGATEST;              /* Do we have an EGA/VGA? */
    __dpmi_int(BIOSINT, &regs);
    if (regs.h.dl) {
        ega = 1;
        row_max = regs.h.dl;            /* Yes: dl is number of rows - 1. */
    }
    else
        row_max = DEF_ROW_MAX;
    regs.h.ah = B_GETMODE;
    __dpmi_int(BIOSINT, &regs);
    col_max = regs.h.ah - 1;
    video_mode = regs.h.al;
    video_page = regs.h.bh;
    bios_flag = usebios;
    if (bios_flag) return;
    if (video_mode == MT25X80) {        /* Mono text mode? */
        __dpmi_int(BIOSEQUIP, &regs);
        if ((regs.x.ax & EQUIP_MONO) == EQUIP_MONO)
            /* Real mono system. */
            scrbase = 0xb0000;
    } else {
        /* If graphics mode or no EGA/VGA */
        if (video_mode > CT25X80 || !ega) {
            bios_flag = 1;
            return;
        }
        scrbase = 0xb8000;
    }
    if (video_page == 1)
        scrbase += 0x1000;
}

/* uninitialize the console */
void oss_con_uninit(void)
{
    /* nothing to do */
}

/*
 * Scroll region down a line.
 * This is equivalent to inserting a line at the top of the region.
 */
void ossscu(int top, int left, int bottom, int right, int blank_color)
{
    __dpmi_regs regs;

    regs.h.ch = top;
    regs.h.cl = left;
    regs.h.dh = bottom;
    regs.h.dl = right;
    regs.h.bh = blank_color;
    regs.x.ax = B_DOWNSCROLL_1;
    __dpmi_int(BIOSINT, &regs);
}

/*
 * Scroll region up a line
 * This is equivalent to deleting a line at the top of the region and pulling
 * everything below it up.
 */
void ossscr(int top, int left, int bottom, int right, int blank_color)
{
    __dpmi_regs regs;

    regs.h.ch = top;
    regs.h.cl = left;
    regs.h.dh = bottom;
    regs.h.dl = right;
    regs.h.bh = blank_color;
    regs.x.ax = B_UPSCROLL_1;
    __dpmi_int(BIOSINT, &regs);
}

/*
 * Clear the given region of the screen
 */
void ossclr(int top, int left, int bottom, int right, int color)
{
    __dpmi_regs regs;

    /* if we're clearing an empty area, ignore it */
    if (top > bottom || left > right)
        return;

    regs.h.ch = top;
    regs.h.cl = left;
    regs.h.dh = bottom;
    regs.h.dl = right;
    regs.h.bh = color;
    regs.x.ax = B_CLEAR_REGION;
    __dpmi_int(BIOSINT, &regs);
}

/*
 * Locate cursor at given line and column
 * Line/column numbers start at zero (0)
 */
void ossloc(int row, int column)
{
    __dpmi_regs regs;

    regs.h.dh = row;
    regs.h.dl = column;
    regs.h.bh = video_page;
    regs.h.ah = B_MOVECURSOR;
    __dpmi_int(BIOSINT, &regs);
}

/*
 * Return 1 if screen is monochrome, 0 otherwise
 */
int ossmon(void)
{
    return video_mode == MT25X80;
}

/*
 * Display msg with color at coordinates (y, x)
 */
void ossdsp(int y, int x, int color, char *s)
{
    if (bios_flag) {
        __dpmi_regs regs;

        while (*s) {
            ossloc(y, x++);
            regs.h.ah = B_WRITECHAR;
            regs.h.al = *s++;
            regs.h.bh = video_page;
            regs.h.bl = color;
            regs.x.cx = 1;              /* count = 1 */
            __dpmi_int(0x10, &regs);
        }
    } else {
        union vcell vc;
        unsigned long vp;
        char *p = s;

        vp = scrbase + ((y * 80 + x) << 1);
        vc.ca.vattr = color;
        _farsetsel(_go32_conventional_mem_selector());
        while (*p) {
            vc.ca.vchar = *p++;
            _farnspokew(vp, vc.s);
            vp += 2;
        }
        ossloc(y, x + (p - s - (*s ? 1 : 0)));
    }
}

/*
 * Return max screen size in *max_line and *max_column
 */
void ossgmx(int *max_line, int *max_column)
{
    *max_line = row_max;
    *max_column = col_max;
}

int ossvpg(char pg)
{
    int  ret;
    __dpmi_regs regs;
    
    if (pg == video_page) return(video_page);
    ret = video_page;
    video_page = pg;
    regs.h.ah = 5;
    regs.h.al = pg;
    __dpmi_int(BIOSINT, &regs);
    if (video_page == 1)
        scrbase += 0x1000;
    else
        scrbase -= 0x1000;
    return(ret);
}

/*
 *   is stdin at EOF? 
 */
int oss_eof_on_stdin()
{
    return feof(stdin);
}
