#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osnet-dos.cpp - OS networking functions for console-mode configurations
Function
  
Notes
  
Modified
  05/27/10 MJRoberts  - Creation
*/

#include "t3std.h"
#include <WinSock2.h>
#include <Windows.h>
#include "osifcnet.h"

/* ------------------------------------------------------------------------ */
/*
 *   Process Windows messages.  Our console-mode configurations don't have
 *   any Windows message processing, so we simply ignore any messages that
 *   should happen to (mysteriously) arrive while we're waiting for network
 *   events.  
 */
void osnet_host_process_message(MSG *)
{
}


