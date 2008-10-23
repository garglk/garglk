#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

glui32 glk_gestalt(glui32 id, glui32 val)
{
	return glk_gestalt_ext(id, val, NULL, 0);
}

glui32 glk_gestalt_ext(glui32 id, glui32 val, glui32 *arr, 
	glui32 arrlen)
{
	switch (id) {

		case gestalt_Version:
			return 0x00000601;

		case gestalt_LineInput:
			if (val >= 32 && val < 256)
				return TRUE;
			else
				return FALSE;

		case gestalt_CharInput: 
			if (val >= 32 && val < 256)
				return TRUE;
			else if (val == keycode_Return)
				return TRUE;
			else
				return FALSE;

		case gestalt_CharOutput: 
			if (val >= 32 && val < 256) {
				if (arr && arrlen >= 1)
					arr[0] = 1;
				return gestalt_CharOutput_ExactPrint;
			}
			else {
				/* cheaply, we don't do any translation of printed
					characters, so the output is always one character 
					even if it's wrong. */
				if (arr && arrlen >= 1)
					arr[0] = 1;
				return gestalt_CharOutput_CannotPrint;
			}

		case gestalt_MouseInput: 
			if (val == wintype_TextGrid)
				return TRUE;
			if (val == wintype_Graphics)
				return TRUE;
			return FALSE;

		case gestalt_Timer: 
			return TRUE;

		case gestalt_Graphics:
		case gestalt_GraphicsTransparency:
			return gli_conf_graphics;
		case gestalt_DrawImage:
			if (val == wintype_TextBuffer)
				return gli_conf_graphics;
			if (val == wintype_Graphics)
				return gli_conf_graphics;
			return FALSE;

		case gestalt_Sound:
		case gestalt_SoundVolume:
		case gestalt_SoundMusic:
			return gli_conf_sound;
		case gestalt_SoundNotify: 
			return FALSE;	/* i'm lazy */

		default:
			return 0;

	}
}

