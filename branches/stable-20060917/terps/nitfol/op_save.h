/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i op_save.c' */
#ifndef CFH_OP_SAVE_H
#define CFH_OP_SAVE_H

/* From `op_save.c': */
BOOL savegame (void);
void op_save1 (void);
void op_save4 (void);
void op_save5 (void);
BOOL restoregame (void);
void op_restore1 (void);
void op_restore4 (void);
void op_restore5 (void);
void op_restart (void);
void op_save_undo (void);
void op_restore_undo (void);
void op_quit (void);
BOOL check_game_for_save (strid_t gamefile , zword release , const char serial[6] , zword checksum );

#endif /* CFH_OP_SAVE_H */
