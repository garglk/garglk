//
//  randomness.h
//  Spatterlight
//
//  Created by Administrator on 2026-02-07.
//

#ifndef randomness_h
#define randomness_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void set_erkyrath_random(uint32_t seed);
uint32_t erkyrath_random(void);
void erkyrath_random_get_detstate(int *usenative, uint32_t **arr, int *count);
void erkyrath_random_set_detstate(int usenative, uint32_t *arr, int count);

#ifdef __cplusplus
}
#endif

#endif /* randomness_h */
