// vim: set ft=cpp:

#ifndef ZTERP_SOUND_H
#define ZTERP_SOUND_H

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}
#endif

#include "types.h"

#ifdef GLK_MODULE_SOUND
extern uint16_t sound_routine;
void sound_stopped();
#endif

void init_sound();
bool sound_loaded();

void zsound_effect();

#endif
