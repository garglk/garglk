//
//  game_specific.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-27.
//

#ifndef game_specific_h
#define game_specific_h

#include <stddef.h>

#include "scott_defines.h"

void MapSysMessages(const SysMessageType *keys, size_t n, int src_offset);
int MapSysRange(SysMessageType first, SysMessageType last, int src);
void SetParserWordLists(const char **directions, const char **skip_list,
    const char **delimiters, const char **extra_commands, const char **extra_nouns);

/* Map system_messages[offset...] to sys[] through a whole key table */
#define MAP_SYS_MESSAGES(keys, offset) \
    MapSysMessages((keys), sizeof(keys) / sizeof((keys)[0]), (offset))

/* Indices into pal[] of the white and the blue that the animations of
   Secret Mission and Robin of Sherwood paint with, in the palette in use */
int WhiteColour(void);
int BlueColour(void);

void SecretAction(int p);
void AdventurelandAction(int p);
void AdventurelandDarkness(void);
void Spiderman64Sysmess(void);
void SpidermanAtari8Sysmess(void);
void Adventureland64Sysmess(void);
void Claymorgue64Sysmess(void);
void Mysterious64Sysmess(void);
void PerseusItalianSysmess(void);
void Supergran64Sysmess(void);
void SecretMission64Sysmess(void);
void UpdateSecretAnimations(void);

void ShowCloseup(int image);
void ShowUSCloseup(int image, int offset);
void SetCloseupVerb(int verb, int us_pictures);
void AddCloseup(int item, int noun, int image);
int HasCloseupTable(void);
int GetCloseup(int index, int *verb, int *item, int *noun, int *image);
void ShowCloseupOnExamine(int verb, int noun);
extern int closeups_without_graphics;
void AddExtraCloseups(GameIDType game);


#endif /* game_specific_h */
