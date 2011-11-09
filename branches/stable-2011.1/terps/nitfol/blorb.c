#include "nitfol.h"
#include "gi_blorb.h"

/* Link this in only if your glk supports blorb */

giblorb_err_t wrap_gib_create_map(strid_t file, giblorb_map_t **newmap)
{
  return giblorb_create_map(file, newmap);
}


giblorb_err_t wrap_gib_destroy_map(giblorb_map_t *map)
{
  return giblorb_destroy_map(map);
}


giblorb_err_t wrap_gib_load_resource(giblorb_map_t *map, glui32 method, giblorb_result_t *res, glui32 usage, glui32 resnum)
{
  return giblorb_load_resource(map, method, res, usage, resnum);
}


giblorb_err_t wrap_gib_count_resources(giblorb_map_t *map, glui32 usage, glui32 *num, glui32 *min, glui32 *max)
{
  return giblorb_count_resources(map, usage, num, min, max);
}


giblorb_err_t wrap_gib_set_resource_map(strid_t file)
{
  return giblorb_set_resource_map(file);
}

