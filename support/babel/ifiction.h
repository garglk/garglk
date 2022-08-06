/* ifiction.h  declarations for the babel ifiction API
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

#ifndef IFICTION_H
#define IFICTION_H

#include "treaty.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Babel's notion of an XML tag */
struct XMLTag
{
 int32 beginl;                  /* Beginning line number */
 char tag[256];                 /* name of the tag */
 char fulltag[256];             /* Full text of the opening tag */
 char *begin;                   /* Points to the beginning of the tag's content */
 char *end;                     /* Points to the end of the tag's content.
                                   setting *end=0 will turn begin into a string
                                   containing the tag's content (But if you do this, you
                                   should restore the original value of *end before
                                   allowing control to return to the ifiction parser) */
 char occurences[256];          /* Tables used internally to find missing required tags */
 char rocurrences[256];
 struct XMLTag *next;           /* The tag's parent */

};

typedef void (*IFCloseTag)(struct XMLTag *, void *);
typedef void (*IFErrorHandler)(char *, void *);


void ifiction_parse(char *md, IFCloseTag close_tag, void *close_ctx, IFErrorHandler error_handler, void *error_ctx);
int32 ifiction_get_IFID(char *metadata, char *output, int32 output_extent);
char *ifiction_get_tag(char *md, char *p, char *t, char *from);
#ifdef __cplusplus
}
#endif

#endif
