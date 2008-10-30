/* heglkmac.c: Macintosh-specific code for Hugo.
*/

#include "glk.h"
#include "gi_dispa.h"
#include "gi_blorb.h"

#include "macglk_startup.h" /* This comes with the MacGlk library. */

static OSType gamefile_type = 'Hugo';
extern strid_t game; /* This is defined in heheader.h. */

Boolean macglk_startup_code(macglk_startup_t *data)
{
	giblorb_err_t err;
	
	data->app_creator = 'gHgo';
	data->startup_model = macglk_model_ChooseOrBuiltIn;
	data->gamefile_types = &gamefile_type;
	data->savefile_type = 'HugS';
	data->datafile_type = 'HugD';
	data->num_gamefile_types = 1;
	data->gamefile = &game;
	
	return TRUE;
}
