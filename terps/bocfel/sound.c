#include <stdlib.h>
#include <string.h>

#include "sound.h"
#include "process.h"
#include "zterp.h"

#ifdef ZTERP_GLK
#include <glk.h>
#ifdef GARGLK
#include <glkstart.h>
#include <gi_blorb.h>
static schanid_t sound_channel = NULL;
#endif
#endif

/* A Blorb file can contain the story file, or it can simply be an
 * external package of resources.  If it contains the story file, then
 * the story file itself should be used to find any resources for the
 * game.  Otherwise, try to find a Blorb file that goes with the
 * selected story.
 */
void init_sound(int story_is_blorb)
{
#ifdef GARGLK
  strid_t file = NULL;

  if(sound_loaded()) return;

  if(glk_gestalt(gestalt_Sound, 0))
  {
    if(story_is_blorb)
    {
      file = glkunix_stream_open_pathname((char *)game_file, 0, 0);
    }
    else
    {
      /* 5 for the worst case of needing to add .blb to the end plus the
       * null character.
       */
      char *blorb_file = malloc(strlen(game_file) + 5);
      if(blorb_file != NULL)
      {
        char *p;

        strcpy(blorb_file, game_file);
        p = strrchr(blorb_file, '.');
        if(p != NULL) *p = 0;
        strcat(blorb_file, ".blb");

        file = glkunix_stream_open_pathname(blorb_file, 0, 0);
        free(blorb_file);
      }
    }
  }

  if(file != NULL)
  {
    giblorb_set_resource_map(file);
    sound_channel = glk_schannel_create(0);
  }
#endif
}

int sound_loaded(void)
{
#ifdef GARGLK
  return sound_channel != NULL;
#else
  return 0;
#endif
}

void zsound_effect(void)
{
#ifdef GARGLK
  uint8_t repeats, volume;
  static uint32_t vols[8] = {
    0x02000, 0x04000, 0x06000, 0x08000,
    0x0a000, 0x0c000, 0x0e000, 0x10000
  };

  if(sound_channel == NULL || zargs[0] < 3) return;

  repeats = zargs[2] >> 8;
  volume = zargs[2] & 0xff;

  /* Illegal, but expected to work by “The Spy Who Came In From The
   * Garden” and recommended by standard 1.1.
   */
  if(repeats == 0) repeats = 1;

  if(volume == 0) volume = 1;
  if(volume  > 8) volume = 8;

  glk_schannel_set_volume(sound_channel, vols[volume - 1]);

  switch(zargs[1])
  {
    case 1: /* prepare */
      glk_sound_load_hint(zargs[0], 1);
      break;
    case 2: /* start */
      glk_schannel_play_ext(sound_channel, zargs[0], repeats == 255 ? -1 : repeats, 0);
      break;
    case 3: /* stop */
      glk_schannel_stop(sound_channel);
      break;
    case 4: /* finish with */
      glk_sound_load_hint(zargs[0], 0);
      break;
  }
#endif
}
