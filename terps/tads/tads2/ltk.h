/*
$Header: d:/cvsroot/tads/TADS2/LTK.H,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1993 by Steve McAdams.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  ltk.h - Library porting Tool Kit definitions
Function
  
Notes
  These are generic definitions which should be applicable to any system.
  The implementation will be in a file call LTKxxx, where 'xxx' is the os
  that the toolkit is written for.
  
Modified
  02/11/93 SMcAdams - Creation
*/

#ifndef LTK
#define LTK

#include <lib.h>


/*
 * ltkini - allocate and INItialize ltk context.  'heapsiz' is the
 * requested size for the local heap. Returns 0 if the request cannot be
 * satisfied.  
 */
void ltkini(unsigned short heapsiz);


/*
 * ltkfre - FREe ltk context.  
 */
void ltkfre();


/*
 * ltk_dlg - DiaLoG.  Present user with informational dialog message.
 * 'title' specifies the title to use in the dialog box, 'msg' is the
 * text message, which may contain printf-style formatting.
 * printf-style arguments must be passed in also, if the message
 * requires them.  
 */
void ltk_dlg(char *title, char *msg, ...);


/*
 * ltk_errlog - Error logging function for LER routines.
 */
void ltk_errlog(void *ctx, char *fac, int errno,
                int agrc, union erradef *argv);


/*
 * ltk_alloc - ALLOCate permanent global memory.  Returns 0 if the
 * request cannot be satisfied.  
 */
void *ltk_alloc(size_t siz);

/* ltk_realloc - reallocate memory; analogous to realloc() */
void *ltk_realloc(void *ptr, size_t siz);


/*
 * ltk_sigalloc - ALLOCate permanent global memory, signals error on
 * failure.  
 */
void *ltk_sigalloc(struct errcxdef *errcx, size_t siz);


/*
 * ltk_free - FREE memory allocated using ltk_alloc.
 */
void ltk_free(void *ptr);


/*
 * ltk_suballoc - SUB-ALLOCate memory from user heap.  Returns 0 if the
 * request cannot be satisfied.  
 */
void *ltk_suballoc(size_t siz);


/*
 * ltk_sigsuballoc - SUB-ALLOCate memory from user heap, signals error
 * on failure. 
 */
void *ltk_sigsuballoc(struct errcxdef *errcx, size_t siz);


/*
 * ltk_subfree - SUBsegment FREE.  Frees memory allocated by
 * ltk_suballoc.  
 */
void ltk_subfree(void *ptr);


/*
 * ltk_beep - BEEP the user. 
 */
void ltk_beep(void);


/*
 * ltk_beg_wait - signal that the user needs to wait.  
 */
void ltk_beg_wait(void);


/*
 * ltk_end_wait - end the waiting period .
 */
void ltk_end_wait(void);


#endif /* LTK */
