/*----------------------------------------------------------------------*\

  term.c

  System dependent terminal oriented functions for ALAN interpreter ARUN

\*----------------------------------------------------------------------*/

#include "sysdep.h"


#ifdef HAVE_TERMIO

#ifdef __linux__
#include <sys/ioctl.h>
#include <asm/ioctls.h>
#endif

#ifdef __FreeBSD__
#include <sys/ioctl.h>
#endif

#endif /* HAVE_TERMIO */

#ifdef HAVE_GLK
#include "glkio.h"
#endif

/* IMPORTS */
#include "memory.h"
#include "output.h"
#include "options.h"
#include "instance.h"
#include "current.h"


/* PUBLIC DATA */
Bool onStatusLine = FALSE; /* To know if where printing the status line or not */



#ifdef __windows__

#include <windows.h>

HWND getConsoleHandle(void) {
  char oldTitle[1000];
  char newTitle[1000];
  HWND handle;

  GetConsoleTitle(oldTitle, 1000);

  wsprintf(newTitle, "Arun console%d-%d", GetTickCount(), GetCurrentProcessId());
  SetConsoleTitle(newTitle);
  Sleep(50);
  handle = FindWindow(NULL, newTitle);
  SetConsoleTitle(oldTitle);
  return handle;
}
#endif


/*======================================================================

  getPageSize()

  Try to get the current page size from the system, else use the ones
  from the header.

 */
void getPageSize(void)
{
#ifdef HAVE_GLK
  pageLength = 0;
  pageWidth = 0;

#else
#ifdef HAVE_TERMIO

#include <sys/termios.h>

#ifdef __linux__
extern int ioctl (int __fd, unsigned long int __request, ...) __THROW;
#else
extern int ioctl();
#endif
  struct winsize win;
  int ecode;

  ecode = ioctl(1, TIOCGWINSZ, &win);

  if (ecode != 0 || win.ws_row == 0)
    pageLength = header->pageLength;
  else
    pageLength = win.ws_row;

  if (ecode != 0 || win.ws_col == 0)
    pageWidth = header->pageWidth;
  else
    pageWidth = win.ws_col;

#else
#ifdef __amiga__
#include <libraries/dosextens.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include <clib/exec_protos.h>

  struct Process * proc;
  struct InfoData *id;
  struct Window *win;
  struct TextFont *textFont;
  struct StandardPacket *packet;

  proc = (struct Process *) FindTask(0L);

  id = (struct InfoData *) allocate(sizeof(struct InfoData));

  if (proc->pr_ConsoleTask) {
    packet = (struct StandardPacket *) allocate(sizeof(struct StandardPacket));
    packet->sp_Msg.mn_Node.ln_Name	= (char *)&(packet->sp_Pkt);
    packet->sp_Pkt.dp_Link		= & packet->sp_Msg;
    packet->sp_Pkt.dp_Port		= & proc->pr_MsgPort;
    packet->sp_Pkt.dp_Type		= ACTION_DISK_INFO;

    packet->sp_Pkt.dp_Arg1 = ((LONG) id) >> 2;

    PutMsg ((struct MsgPort *) proc->pr_ConsoleTask, & packet->sp_Msg);
    WaitPort(&proc->pr_MsgPort);
    GetMsg(&proc->pr_MsgPort);
    free((char *)packet);

    win = (struct Window *) id->id_VolumeNode;
    free(id);

    /* Calculate number of characters and lines w.r.t font size and borders */
    textFont = win->IFont;
    pageLength = win->Height/textFont->tf_YSize-2;
    pageWidth = win->Width/textFont->tf_XSize-3;
  } else {
    pageLength = header->pageLength;
    pageWidth = header->pageWidth;
  }

#else

  pageLength = header->pageLength;
  pageWidth = header->pageWidth;

#endif
#endif
#endif
}

/*======================================================================*/
void statusline(void)
{
#ifdef HAVE_GLK
  glui32 glkWidth;
  char line[100];
  int pcol = col;

  if (!statusLineOption) return;
  if (glkStatusWin == NULL)
    return;

  glk_set_window(glkStatusWin);
  glk_window_clear(glkStatusWin);
  glk_window_get_size(glkStatusWin, &glkWidth, NULL);

  onStatusLine = TRUE;
  col = 1;
  glk_window_move_cursor(glkStatusWin, 1, 0);
  sayInstance(where(HERO, TRUE));

  // TODO Add status message1  & 2 as author customizable messages
  if (header->maximumScore > 0)
    sprintf(line, "Score %d(%d)/%d moves", current.score, (int)header->maximumScore, current.tick);
  else
    sprintf(line, "%d moves", current.tick);
  glk_window_move_cursor(glkStatusWin, glkWidth-strlen(line)-1, 0);
  glk_put_string(line);
  needSpace = FALSE;

  col = pcol;
  onStatusLine = FALSE;

  glk_set_window(glkMainWin);
#else
#ifdef HAVE_ANSI
  char line[100];
  int i;
  int pcol = col;

  if (!statusLineOption) return;
  /* ansi_position(1,1); ansi_bold_on(); */
  printf("\x1b[1;1H");
  printf("\x1b[7m");

  onStatusLine = TRUE;
  col = 1;
  sayInstance(where(HERO, FALSE));

  if (header->maximumScore > 0)
    sprintf(line, "Score %d(%ld)/%d moves", current.score, header->maximumScore, current.tick);
  else
    sprintf(line, "%ld moves", (long)current.tick);
  for (i=0; i < pageWidth - col - strlen(line); i++) putchar(' ');
  printf(line);
  printf("\x1b[m");
  printf("\x1b[%d;1H", pageLength);

  needSpace = FALSE;
  capitalize = TRUE;

  onStatusLine = FALSE;
  col = pcol;
#endif
#endif
}


