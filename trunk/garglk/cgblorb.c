#include <stdio.h>
#include "glk.h"
#include "garglk.h"
#include "gi_blorb.h"

static strid_t blorbfile = NULL;
static giblorb_map_t *blorbmap = NULL;

giblorb_err_t giblorb_set_resource_map(strid_t file)
{
	giblorb_err_t err;

	/* For the moment, we only allow file-streams, because the resource
	   loaders expect a FILE*. This could be changed, but I see no
	   reason right now. */
	if (file->type != strtype_File)
		return giblorb_err_NotAMap;

	err = giblorb_create_map(file, &blorbmap);
	if (err) {
		blorbmap = NULL;
		return err;
	}

	blorbfile = file;

	return giblorb_err_None;
}

giblorb_map_t *giblorb_get_resource_map()
{
	return blorbmap;
}

int giblorb_is_resource_map()
{
	return (blorbmap != NULL);
}

void giblorb_get_resource(glui32 usage, glui32 resnum, 
		FILE **file, long *pos, long *len, glui32 *type)
{
	giblorb_err_t err;
	giblorb_result_t blorbres;

	*file = NULL;
	*pos = 0;

	if (!blorbmap)
		return;

	err = giblorb_load_resource(blorbmap, giblorb_method_FilePos, 
			&blorbres, usage, resnum);
	if (err)
		return;

	*file = blorbfile->file;
	if (pos)
		*pos = blorbres.data.startpos;  
	if (len)
		*len = blorbres.length; 

	if (type) {
		*type = blorbres.chunktype;
	}
}

