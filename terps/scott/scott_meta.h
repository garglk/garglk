/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  scott_meta.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Saving, loading, restarting, prompts and the extra (meta) commands.
 */

#ifndef scott_meta_h
#define scott_meta_h

int YesOrNo(void);
void HitEnter(void);
int SelectGameFromMenu(const char *intro, const char **titles, int count);
void SaveGame(void);
void LoadGame(void);
void RestartGame(void);
int PerformExtraCommand(int extra_stop_time);

#endif /* scott_meta_h */
