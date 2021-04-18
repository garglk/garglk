// vim: set ft=c:

#ifndef ZTERP_SOUND_H
#define ZTERP_SOUND_H

#include <stdbool.h>
#include <stdint.h>

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#ifdef GLK_MODULE_SOUND
extern uint16_t sound_routine;
void sound_stopped(void);
#endif

void init_sound(void);
bool sound_loaded(void);

void zsound_effect(void);

#endif
