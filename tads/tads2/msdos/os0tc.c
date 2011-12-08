#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/OS0TC.C,v 1.2 1999/05/17 02:52:18 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  os0tc.c - os mainline for tads2 compiler
Function
  invokes compiler from operating system command line
Notes
  none
Modified
  04/02/92 MJRoberts     - creation
*/

#include <stdlib.h>
#include <string.h>
#include "os.h"

#ifdef __DPMI16__
#include <dos.h>
#include <windows.h>
#include "ltk.h"


/*
 *   real-mode break handler (see the installation routine below for
 *   information) 
 */
static void _interrupt realmode_break()
{
    asm {
        push  ds
        push  si

        /* get the int 23h vector pointer into DS:SI */
        mov   si, 0
        mov   ds, si
        mov   si, 0x23 * 4
        lds   si, [si]

        /* set the byte 2 bytes before the handler (me)        */
        /* -- this byte is the flag that indicates ^C was hit  */
        sub   si, 2
        mov   byte ptr [si], 1

        pop   si
        pop   ds
    }
}

/*
 *   Protected-mode pointer to the real-mode interrupt handler.  The
 *   handler's entrypoint is 2 bytes above this pointer; the first byte at
 *   this location is a flag which, when non-zero, means that a ^C
 *   occurred. 
 */
static char *brk_pointer;

static void _interrupt _loadds protmode_break()
{
    *brk_pointer = 1;
}

/*
 *   Set up a control-break interrupt handler for DPMI mode.  With a DPMI
 *   DOS extender, it's necessary to hook the int 23h ^C interrupt in
 *   real-mode in order to make sure we get notified of a control-break or
 *   ^C.  We allocate a real-mode segment, copy our handler into that
 *   segment, and call the DPMI server to set the int 23h vector to point
 *   to our real-mode handler.  The handler merely sets a flag (which is
 *   the two bytes immediately before the handler's entrypoint, so that we
 *   can reference it from either mode) which we can test in the
 *   compiler's main loop.
 */
static void init_dpmi16_break(void)
{
    unsigned  long realaddr;
    unsigned  realseg;
    unsigned  realofs;
    unsigned  protseg;
    unsigned  protofs;
    char     *protptr;
    unsigned  int_page_seg;
    char     *int_page_ptr;

    /* allocate a block of real-mode memory big enough for the handler */
    realaddr = GlobalDosAlloc(0x200);
    realseg = realaddr >> 16;
    protseg = realaddr & 0xffff;

    /* set up a protected-mode pointer to the real-mode memory */
    realofs = 0;
    protptr = MK_FP(protseg, realofs);

    /* copy the interrupt handler */
    memcpy(protptr + 2, realmode_break, 100);

    /* set the real-mode interrupt 23h vector to point to the handler */
    asm {
        /* point the int 23h (DOS break) vector to our handler */
        mov   cx, realseg
        mov   dx, realofs
        add   dx, 2
        mov   bl, 0x23
        mov   ax, 0x0201
        int   0x31

        /* for good measure, handle int 1b (BIOS break) the same way */
        mov   cx, realseg
        mov   dx, realofs
        add   dx, 2
        mov   bl, 0x1b
        mov   ax, 0x0201
        int   0x31
    }

    /* save the location of the ^C flag, and clear the flag */
    brk_pointer = protptr;
    *brk_pointer = 0;

    /* set the protected-mode interrupt 23h and 1bh vectors as well */
    protseg = FP_SEG(protmode_break);
    protofs = FP_OFF(protmode_break);
    asm {
        /* set int 23h to point to our handler */
        mov   cx, protseg
        mov   dx, protofs
        mov   bl, 0x23
        mov   ax, 0x0205
        int   0x31

        /* handle int 1bh the same way */
        mov   cx, protseg
        mov   dx, protofs
        mov   bl, 0x1b
        mov   ax, 0x0205
        int   0x31
    }

    /*
     *   The Borland DPMI server seems to ignore a request to install a
     *   real-mode int 23h handler, even though it's documented as working
     *   correctly.  To deal with this, install our int 23h handler the
     *   old-fashioned way, by poking it directly into the interrupt
     *   page.
     */
    asm
    {
        mov   ax, 0x0002
        mov   bx, 0x0000
        int   0x31
        mov   int_page_seg, ax
    }
    int_page_ptr = MK_FP(int_page_seg, 0);
    *(long *)(int_page_ptr + (0x23 * 4)) =
        (long)MK_FP(realseg, realofs + 2);
    *(long *)(int_page_ptr + (0x1b * 4)) =
        (long)MK_FP(realseg, realofs + 2);
}

/*
 *   Yield CPU to other processes, and check the ^C status.  Checks the
 *   word preceding the real-mode interrupt 23h handler, since this word
 *   is set to a non-zero value when the interrupt 23h handler is invoked
 *   (which happens in response to a ^C).  In any case, there's nothing we
 *   need to do to yield the CPU - this is merely a break check.  
 */
int os_yield(void)
{
    return *brk_pointer;
}


#endif /* __DPMI16__ */


int main(argc, argv)
int   argc;
char *argv[];
{
    int   tcdmain(int argc, char *argv[], char *);
    char *config_name;

    config_name = "config.tc";
#ifdef __DPMI16__
    ltkini(16384);
    init_dpmi16_break();
/*    config_name = "config16.tc"; */
#else
#endif
    return(os0main(argc, argv, tcdmain, "i", config_name));
}
