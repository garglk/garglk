/*
$Header: d:/cvsroot/tads/TADS2/PLY.H,v 1.2 1999/05/17 02:52:13 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  ply.h - definitions for game play loop
Function
  definitions for game play loop
Notes
  none
Modified
  04/11/92 MJRoberts     - creation
*/

#ifndef PLY_INCLUDED
#define PLY_INCLUDED

void plygo(struct runcxdef *run, struct voccxdef *voc,
           struct tiocxdef *tio, objnum preinit, char *restore_fname);

#endif /* PLY_INCLUDED */
