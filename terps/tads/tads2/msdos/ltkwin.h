/* 
 *   Copyright (c) 1993 by Steve McAdams.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  ltkwin.h - Library porting ToolKit for WINdows def's
  
Function
  
Notes
  
Modified
  02/16/93 SMcAdams - Creation
*/

#ifndef LTKWIN
#define LTKWIN

#include <ltk.h>
#include <windows.h>


/*
 * ltk - Per application instance information.  
 */
struct ltk
{
  HINSTANCE   ltkins;                                    /* instance handle */
  HANDLE      ltkfra;                                /* frame window handle */
  HANDLE      ltkcli;                               /* client window handle */
  FARPROC     ltkclf;                /* client window event hander function */
  HMENU       ltkmnu;                                       /* default menu */
  HMENU       ltkwin;                              /* "WINDOW" popup handle */
};
typedef struct ltk ltk;


#endif /* LTKWIN */
