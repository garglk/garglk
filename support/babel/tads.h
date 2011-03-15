/*
 *   tads.h - Treaty of Babel common declarations for tads2 and tads3 modules
 *   
 *   This file depends on treaty_builder.h
 *   
 * This file has been released into the public domain by its author.
* The author waives all of his rights to the work
* worldwide under copyright law to the maximum extent allowed by law
* , but note that any changes to this file may
 *   render it noncompliant with the Treaty of Babel
 *   
 *   Modified
 *.   04/18/2006 MJRoberts  - creation
 */

#ifndef TADS_H
#define TADS_H
#ifdef __cplusplus
extern "C" {
#endif

/* match a TADS file signature */
int tads_match_sig(const void *buf, int32 len, const char *sig);

/* get the IFID for a tads story file */
int32 tads_get_story_file_IFID(void *story_file, int32 extent,
                               char *output, int32 output_extent);

/* get the synthesized iFiction record from a tads story file */
int32 tads_get_story_file_metadata(void *story_file, int32 extent,
                                   char *buf, int32 bufsize);

/* get the size of the synthesized iFiction record for a tads story file */
int32 tads_get_story_file_metadata_extent(void *story_file, int32 extent);

/* get the cover art from a tads story file */
int32 tads_get_story_file_cover(void *story_file, int32 extent,
                                void *buf, int32 bufsize);

/* get the size of the cover art from a tads story file */
int32 tads_get_story_file_cover_extent(void *story_file, int32 extent);

/* get the image format (jpeg, png) of the covert art in a tads story file */
int32 tads_get_story_file_cover_format(void *story_file, int32 extent);
#ifdef __cplusplus
}
#endif


#endif /* TADS_H */
