#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "sound.h"
#include "process.h"
#include "stack.h"
#include "zterp.h"

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#define EFFECT_PREPARE 1
#define EFFECT_PLAY 2
#define EFFECT_STOP 3
#define EFFECT_FINISH_WITH 4

static uint16_t routine = 0;

static int next_sample = 0;
static int next_volume = 0;

static bool locked = false;
static bool playing = false;

#ifdef GLK_MODULE_SOUND
static schanid_t sound_channel = NULL;
#endif

void init_sound(void)
{
#ifdef GLK_MODULE_SOUND
  if(sound_loaded()) return;

  if(glk_gestalt(gestalt_Sound, 0))
  {
    sound_channel = glk_schannel_create(0);
  }
#endif
} /* init_sound */

bool sound_loaded(void)
{
#ifdef GLK_MODULE_SOUND
  return sound_channel != NULL;
#else
  return false;
#endif
} /* sound_loaded */

void os_beep(int number)
{
  /* Unimplemented */
} /* os_beep */

/*
 * start_sample
 *
 * Call the GLk sound interface to play a sample.
 *
 */
static void start_sample(int number, int volume, int repeats, uint16_t eos)
{
#ifdef GLK_MODULE_SOUND
  static uint32_t vols[8] = {
    0x02000, 0x04000, 0x06000, 0x08000,
    0x0a000, 0x0c000, 0x0e000, 0x10000
  };

  static uint8_t lh_repeats[] = {
    0x00, 0x00, 0x00, 0x01, 0xff,
    0x00, 0x01, 0x01, 0x01, 0x01,
    0xff, 0x01, 0x01, 0xff, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff
  };

  /* Illegal, but expected to work by “The Spy Who Came In From The
   * Garden” and recommended by standard 1.1.
   */
  if(repeats == 0) repeats = 1;

  if(volume == 0) volume = 1;
  if(volume  > 8) volume = 8;

  glk_schannel_set_volume(sound_channel, vols[volume - 1]);

  if (is_lurking_horror())
    repeats = lh_repeats[number];

  /* "The Spy Who Came In From The Garden” sets zargs[3] to 1 */
  /* and jumping to adress 1 is a bad idea */
  if (eos <= 1)
    eos = 0;

  glk_schannel_play_ext(sound_channel, number, repeats == 255 ? -1 : repeats, eos);
  routine = eos;
  playing = true;
#endif
} /* start_sample */

/*
 * start_next_sample
 *
 * Play a sample that has been delayed until the previous sound effect has
 * finished.  This is necessary for two samples in The Lurking Horror that
 * immediately follow other samples.
 *
 */
static void start_next_sample(void)
{
  if (next_sample != 0)
    start_sample(next_sample, next_volume, 0, 0);

  next_sample = 0;
  next_volume = 0;
} /* start_next_sample */

/*
 * end_of_sound
 *
 * Call the Z-code routine which was given as the last parameter of
 * a sound_effect call. This function may be called from a hardware
 * interrupt (which requires extremely careful programming).
 *
 */
void end_of_sound(void)
{
  playing = false;

  if (!locked && routine > 1) {

    if (is_lurking_horror())
      start_next_sample();
    direct_call(routine);
  }
  return;
} /* end_of_sound */

/*
 * z_sound_effect, load / play / stop / discard a sound effect.
 *
 *    zargs[0] = number of bleep (1 or 2) or sample
 *    zargs[1] = operation to perform (samples only)
 *    zargs[2] = repeats and volume (play sample only)
 *    zargs[3] = end-of-sound routine (play sample only, optional)
 *
 * Note: Volumes range from 1 to 8, volume 255 is the default volume.
 *     Repeats are stored in the high byte, 255 is infinite loop.
 *
 */
void zsound_effect(void)
{
#ifdef GLK_MODULE_SOUND
  uint16_t number = zargs[0];
  uint16_t effect = zargs[1];
  uint16_t volume = zargs[2];

  /* By default play sound 1 at volume 8 */
  if (znargs < 1)
    number = 1;
  if (znargs < 2)
    effect = EFFECT_PLAY;
  if (znargs < 3)
    volume = 8;

  if (number == 1 || number == 2) {
    os_beep(number);
    return;
  }

  if(sound_channel == NULL) {
    return;
  }

  if (number >= 3 || number == 0) {
    locked = true;
    if (is_lurking_horror() && (number == 9 || number == 16)) {
      if (effect == EFFECT_PLAY) {
        next_sample = number;
        next_volume = volume;
        locked = false;
        if (!playing)
          start_next_sample();
      } else
        locked = false;
      return;
    }
    playing = false;
    switch (effect) {
      case EFFECT_PREPARE:
        glk_sound_load_hint(number, 1);
        break;
      case EFFECT_PLAY:
        start_sample(number, volume & 0xff, volume >> 8,
               (znargs == 4) ? zargs[3] : 0);
        break;
      case EFFECT_STOP:
        glk_schannel_stop(sound_channel);
        break;
      case EFFECT_FINISH_WITH:
        glk_sound_load_hint(number, 0);
        break;
        break;
    }
    locked = false;
  }
#endif
} /* zsound_effect */
