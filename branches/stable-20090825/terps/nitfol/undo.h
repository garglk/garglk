/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i undo.c' */
#ifndef CFH_UNDO_H
#define CFH_UNDO_H

/* From `undo.c': */
void init_undo (void);
BOOL free_undo (void);
BOOL saveundo (BOOL in_instruction );
BOOL restoreundo (void);
BOOL restoreredo (void);

#ifdef DEBUGGING
BOOL fast_saveundo (void);
BOOL fast_restoreundo (void);

#endif
void kill_undo (void);

#endif /* CFH_UNDO_H */
