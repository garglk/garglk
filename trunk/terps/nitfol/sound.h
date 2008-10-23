/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i sound.c' */
#ifndef CFH_SOUND_H
#define CFH_SOUND_H

/* From `sound.c': */

#ifdef GLK_MODULE_SOUND
void init_sound (void);
void kill_sound (void);
void check_sound (event_t moo );
void op_sound_effect (void);

#endif

#endif /* CFH_SOUND_H */
