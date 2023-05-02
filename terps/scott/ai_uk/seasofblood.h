//
//  seasofblood.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-08.
//

#ifndef seasofblood_h
#define seasofblood_h

#include <stdio.h>

void LoadExtraSeasOfBloodData(int c64);
void SeasOfBloodRoomImage(void);
void AdventureSheet(void);
void BloodAction(int p);

extern uint8_t *blood_image_data;

#endif /* seasofblood_h */
