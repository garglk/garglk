static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/LTKWIN.C,v 1.2 1999/05/17 02:52:18 MJRoberts Exp $";

/* 
 *   Copyright (c) 1993 by Steve McAdams.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  ltkwin.c - Implementation of the porting Tool Kit for Windows
  
Function
  ltkini          - initialize ltk routines
  ltkfre          - free ltk context
  
  ltk_suballoc    - suballocate from heap
  ltk_sigsuballoc - suballocate from heap (signal failure)
  ltk_subfree     - free suballocted memory
  ltk_alloc       - allocate permanent global memory
  ltk_sigalloc    - allocate permanent global memory (signal failure)
  ltk_free        - free allocated global memory

  ltk_dlg         - print message in dialog box
  ltk_errlog      - error logging function for LER routines

  ltk_beep        - sound a beep

  ltk_beg_wait    - begin waiting (use hourglass mouse ptr)
  ltk_end_wait    - end waiting   (revert to normal mouse ptr)
  
Notes
  
Modified
  02/11/93 SMcAdams - Creation
*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <os.h>
#include <ltk.h>
#include <ler.h>
#include <windows.h>

#ifdef TURBO
# define INLINE asm
#else
# define INLINE _asm
#endif


/*--------------------------- PRIVATE DATA TYPES ---------------------------*/
struct ltk_mblk
{
  HANDLE mblkhdl;
  ub1    mblkbuf[1];
};
typedef struct ltk_mblk ltk_mblk;



/*------------------------------- STATIC DATA ------------------------------*/
static ub2 ltkseg = 0;                                      /* heap segment */
static HANDLE ltkhdl = 0;                            /* heap segment handle */



/*--------------------------------- ltkini ---------------------------------*/
/*
 * ltkini - allocate and INItialize toolkit context.
 */
void ltkini(unsigned short heapsiz)
{
  HANDLE  memhdl;                                          /* memory handle */
  HANDLE  syshdl;                                     /* sys context handle */
  ub2     segd;                                   /* the segment descriptor */
  void   *heap;                                       /* ptr to heap memory */

  /* allocate and lock heap segment */
  if (!(ltkhdl = GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, heapsiz)) ||
      !(heap = GlobalLock(ltkhdl)))
  {
    /* no luck allocating memory */
    return;
  }

  /*
   * get the actual size allocated.  subtract 16 for Windows overhead
   * (see doc for LocalInit)
   */
  heapsiz = GlobalSize(ltkhdl) - 16;

  /*
   * set DS to new segment 
   */
  ltkseg = HIWORD(heap);                                 /* get the segment */
  INLINE
  {
    push ds
    mov  ax, ltkseg
    mov  ds, ax
  }

  /* initialize the heap segment */
  if (!LocalInit(ltkseg, 0, heapsiz))
  {
    /* restore DS */
    INLINE
    {
      pop ds
    }

    /* unlock and free the global memory */
    GlobalUnlock(ltkhdl);
    GlobalFree(ltkhdl);

    /* can't use heap */
    ltkhdl = 0;
    ltkseg = 0;

    /* no more we can do */
    return;
  }

  /*
   * restore the data segment 
   */
  INLINE
  {
    pop ds
  }
}


/*--------------------------------- ltkfre ---------------------------------*/
/*
 * ltkfre - FREe toolkit context.
 */
void ltkfre()
{
  /* unlock and free the heap segment */
  GlobalUnlock(ltkhdl);
  GlobalFree(ltkhdl);

  /* reset statics */
  ltkhdl = 0;
  ltkseg = 0;
}


/*------------------------------ ltk_suballoc ------------------------------*/
/*
 * ltk_suballoc - SUB ALLOCate memory from heap segment.
 */
void *ltk_suballoc(siz)
  ub2  siz;                                             /* size to allocate */
{
  HANDLE    memhdl;                                        /* memory handle */
  ltk_mblk *mblk;                                           /* memory block */

  /* assert that we have a heap segment */
  if (!ltkseg || !ltkhdl) return((void *)0);

  /* verify that we can allocate the requested size */
  if ((ub4)siz + (ub4)sizeof(ltk_mblk) > 0x10000L) return((void *)0);

  /* use the heap's data segment */
  INLINE
  {
    push ds
    mov  ax, ltkseg
    mov  ds, ax
  }

  /* allocate and lock a block of the requested size */
  if (!(memhdl = LocalAlloc(LMEM_FIXED|LMEM_ZEROINIT,
                            sizeof(ltk_mblk)+siz)) ||
      !(mblk = (ltk_mblk *)LocalLock(memhdl)))
  {
    /* fix up data segment */
    INLINE
    {
      pop ds
    }
    return((void *)0);
  }

  /* return to our own data segment */
  INLINE
  {
      pop ds
  }

  /* stash the handle and we're on our way */
  mblk->mblkhdl = memhdl;
  return(mblk->mblkbuf);
}


/*---------------------------- ltk_sigsuballoc -----------------------------*/
/*
 * ltk_sigsuballoc - allocate from heap, signal failure.
 */
void *ltk_sigsuballoc(errcx, siz)
  errcxdef *errcx;                          /* error context to signal with */
  ub2       siz;                                        /* size to allocate */
{
  void *ptr;                                     /* ptr to allocated memory */

  /* allocate the memory */
  if (!(ptr = ltk_suballoc(siz)))
  {
    /* signal an error */
    errsigf(errcx, "LTK", 0);
  }

  /* return the memory */
  return(ptr);
}


/*------------------------------ ltk_subfree -------------------------------*/
/*
 * ltk_subfree - FREe memory allocated by ltk_suballoc
 */
void ltk_subfree(ptr)
  void *ptr;                                               /* block to free */
{
  ltk_mblk *mblk;                                       /* the memory block */
  
  /* assert that we have a heap segment */
  if (!ltkseg || !ltkhdl) return;

  /* get our handle */
  mblk = (ltk_mblk *)((ub1 *)ptr - offsetof(ltk_mblk, mblkbuf));

  /* use the heap's seg as DS */
  INLINE
  {
    push ds
    mov  ax, ltkseg
    mov  ds, ax
  }

  /* unlock and free the memory */
  LocalUnlock(mblk->mblkhdl);
  LocalFree(mblk->mblkhdl);

  /* finally, return to our regular DS */
  INLINE
  {
    pop ds
  }
}


/*------------------------------- ltk_alloc --------------------------------*/
/*
 * ltk_alloc - Allocate a block of memory.  In windows, the block of
 * memory is allocated, along with a place to stash the memory handle,
 * so we can free this block up again when the time comes.  
 */
void *ltk_alloc(siz)
  size_t  siz;                                 /* size of block to allocate */
{
  HANDLE    memhdl;                                        /* memory handle */
  ltk_mblk *mblk;                                           /* memory block */
  
  /* verify that the we can allocate the requested size */
  if ((ub4)siz+(ub4)sizeof(ltk_mblk) >= 0x10000) return((void *)0);
  
  /* allocate a block of the requested size */
  memhdl = GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, sizeof(ltk_mblk)+siz);
  if (!memhdl) return((void *)0);
  
  /* lock the memory, stash the handle, and we're outta here */
  mblk = (ltk_mblk *)GlobalLock(memhdl);
  if (!mblk) return((void *)0);
  
  mblk->mblkhdl = memhdl;
  return(mblk->mblkbuf);
}


/*------------------------------ ltk_sigalloc ------------------------------*/
/*
 * ltk_sigalloc - allocate permanent global memory, and signal on failure.
 */
void *ltk_sigalloc(errcx, siz)
  errcxdef *errcx;                                         /* error context */
  size_t    siz;                                        /* size to allocate */
{
  void *ptr;                                 /* pointer to allocated memory */

  if (!(ptr = ltk_alloc(siz)))
  {
    /* signal error */
    errsigf(errcx, "LTK", 0);
  }

  /* return a ptr to the allocated memory */
  return(ptr);
}


/*-------------------------------- ltk_free --------------------------------*/
/*
 * ltk_free - free a block of memory allocated by ltk_alloc.  This
 * takes the memory handle stashed just behind the allocation, and frees
 * the block of memory.  
 */
void ltk_free(mem)
  void *mem;                                              /* memory to free */
{
  ltk_mblk *mblk;                                       /* the memory block */
  
  /* get our handle */
  mblk = (ltk_mblk *)((ub1 *)mem - offsetof(ltk_mblk, mblkbuf));
  
  /* unlock and free the memory, and we're outta here */
  GlobalUnlock(mblk->mblkhdl);
  GlobalFree(mblk->mblkhdl);
  return;
}


#ifndef __DPMI16__
/*------------------------------- ltk_errlog -------------------------------*/
/*
 * ltk_errlog - ERRor LOGging function.  Logs an error from the LER
 * system.  
 */
void ltk_errlog(ctx, fac, errno, argc, argv)
  void    *ctx;                                     /* error logger context */
  char    *fac;                                            /* facility code */
  int      errno;                                           /* error number */
  int      argc;                                          /* argument count */
  erradef *argv;                                               /* arguments */
{
  char buf[128];                                  /* formatted error buffer */
  char msg[128];                                          /* message buffer */

  /* $$$ filter out error #504 $$$ */
  if (errno == 504) return;

  /* get the error message into msg */
  errmsg(ctx, msg, sizeof(msg), errno);

  /* format the error message */
  errfmt(buf, (int)sizeof(buf), msg, argc, argv);

  /* display a dialog box containing the error message */
  ltk_dlg("Error", buf);
}


/*-------------------------------- ltk_dlg ---------------------------------*/
/*
 * ltk_dlg - DiaLog.  Puts the given message in a dialog box.
 */
void ltk_dlg(char *title, char *msg, ...)
#ifdef NEVER
  char *title;                                         /* message box title */
  char *msg;                                          /* error message text */
#endif
{
  va_list  argp;                                             /* printf args */
  char     inbuf[80];                                       /* input buffer */
  char     outbuf[160];                    /* allow inbuf to double in size */

  /* clip the input message, if necessary */
  strncpy(inbuf, msg, sizeof(inbuf));
  inbuf[sizeof(inbuf)-1] = '\0';

  /* get the printf args, build the message, and display it */
  va_start(argp, msg);
  vsprintf(outbuf, inbuf, argp);

  /* display the message */
  MessageBox((HWND)NULL, outbuf, title,
             MB_TASKMODAL | MB_ICONEXCLAMATION | MB_OK);
}


/*-------------------------------- ltk_beep --------------------------------*/
/*
 * ltk_beep - BEEP the PC's speaker.
 */
void ltk_beep()
{
  MessageBeep(MB_ICONEXCLAMATION);
}

/*------------------------------ ltk_beg_wait ------------------------------*/
/*
 * ltk_beg_wait - Put up hourglass prompt.
 */
void ltk_beg_wait()
{
}


/*------------------------------ ltk_end_wait ------------------------------*/
/*
 * ltk_end_wait - Put up normal prompt.
 */
void ltk_end_wait()
{
}

#endif /* __DPMI16__ */
