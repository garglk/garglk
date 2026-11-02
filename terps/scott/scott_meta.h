//
//  scott_meta.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Saving, loading, restarting, prompts and the extra (meta) commands.
//

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
