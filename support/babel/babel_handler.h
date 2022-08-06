/* babel_handler.h  declarations for the babel handler API
 * (c) 2006 By L. Ross Raszewski
 *
 * This code is freely usable for all purposes.
 *
 * This work is licensed under the Creative Commons Attribution 4.0 License.
 * To view a copy of this license, visit
 * https://creativecommons.org/licenses/by/4.0/ or send a letter to
 * Creative Commons,
 * PO Box 1866,
 * Mountain View, CA 94042, USA.
 *
 */

#ifndef BABEL_HANDLER_H
#define BABEL_HANDLER_H

#include "treaty.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Functions from babel_handler.c */
char *babel_init(char *filename);
 /* initialize the babel handler */
char *babel_init_raw(void *sf, int32 extent);
 /* Initialize from loaded data */
int32 babel_treaty(int32 selector, void *output, int32 output_extent);
 /* Dispatch treaty calls */
void babel_release(void);
 /* Release babel_handler resources */
char *babel_get_format(void);
 /* return the format of the loaded file */
int32 babel_md5_ifid(char *buffer, int32 extent);
 /* IFID generator of last resort */
uint32 babel_get_length(void);
 /* Fetch file length */
uint32 babel_get_story_length(void);
 /* Fetch file length */
int32 babel_get_authoritative(void);
 /* Determine if babel handler has a good grasp on the format */
void *babel_get_file(void);
 /* Get loaded story file */
void *babel_get_story_file(void);
 /* Get loaded story file */

/* threadsafe versions of above */
char *babel_init_ctx(char *filename, void *);
 /* initialize the babel handler */
int32 babel_treaty_ctx(int32 selector, void *output, int32 output_extent, void *);
 /* Dispatch treaty calls */
void babel_release_ctx(void *);
 /* Release babel_handler resources */
char *babel_get_format_ctx(void *);
 /* return the format of the loaded file */
int32 babel_md5_ifid_ctx(char *buffer, int extent, void *);
 /* IFID generator of last resort */
uint32 babel_get_length_ctx(void *);
uint32 babel_get_story_length_ctx(void *);
void *babel_get_file_ctx(void *bhp);
void *babel_get_story_ctx(void *bhp);
int32 babel_get_authoritative_ctx(void *bhp);
char *babel_init_raw_ctx(void *sf, int32 extent, void *bhp);
void *get_babel_ctx(void);
void release_babel_ctx(void *);
 /* get and release babel contexts */

#ifdef __cplusplus
}
#endif

#endif
