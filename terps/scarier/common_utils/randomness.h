//
//  randomness.h
//  Spatterlight
//
//  Created by Administrator on 2026-02-07.
//

#ifndef randomness_h
#define randomness_h

#include "glk.h"

void set_erkyrath_random(glui32 seed);
glui32 erkyrath_random(void);
void erkyrath_random_get_detstate(int *usenative, glui32 **arr, int *count);
void erkyrath_random_set_detstate(int usenative, glui32 *arr, int count);

#endif /* randomness_h */
