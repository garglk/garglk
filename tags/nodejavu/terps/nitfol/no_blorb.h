/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i no_blorb.c' */
#ifndef CFH_NO_BLORB_H
#define CFH_NO_BLORB_H

/* From `no_blorb.c': */
giblorb_err_t wrap_gib_create_map (strid_t file , giblorb_map_t **newmap );
giblorb_err_t wrap_gib_destroy_map (giblorb_map_t *map );
giblorb_err_t wrap_gib_load_resource (giblorb_map_t *map , glui32 method , giblorb_result_t *res , glui32 usage , glui32 resnum );
giblorb_err_t wrap_gib_count_resources (giblorb_map_t *map , glui32 usage , glui32 *num , glui32 *min , glui32 *max );
giblorb_err_t wrap_gib_set_resource_map (strid_t file );

#endif /* CFH_NO_BLORB_H */
