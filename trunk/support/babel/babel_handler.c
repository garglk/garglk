/* babel_handler.c   dispatches Treaty of Babel queries to the treaty modules
 * (c) 2006 By L. Ross Raszewski
 *
 * This code is freely usable for all purposes.
This program is released under the Perl Artistic License as specified below: 

 Preamble

The intent of this document is to state the conditions under which a
Package may be copied, such that the Copyright Holder maintains some
semblance of artistic control over the development of the package,
while giving the users of the package the right to use and distribute
the Package in a more-or-less customary fashion, plus the right to
make reasonable modifications.  Definitions

    "Package" refers to the collection of files distributed by the
    Copyright Holder, and derivatives of that collection of files
    created through textual modification.

    "Standard Version" refers to such a Package if it has not been
    modified, or has been modified in accordance with the wishes of
    the Copyright Holder as specified below.

    "Copyright Holder" is whoever is named in the copyright or
    copyrights for the package.

    "You" is you, if you're thinking about copying or distributing
    this Package.

    "Reasonable copying fee" is whatever you can justify on the basis
    of media cost, duplication charges, time of people involved, and
    so on. (You will not be required to justify it to the Copyright
    Holder, but only to the computing community at large as a market
    that must bear the fee.)

    "Freely Available" means that no fee is charged for the item
    itself, though there may be fees involved in handling the item. It
    also means that recipients of the item may redistribute it under
    the same conditions they received it.

   1. You may make and give away verbatim copies of the source form of
    the Standard Version of this Package without restriction,
    provided that you duplicate all of the original copyright
    notices and associated disclaimers.

   2. You may apply bug fixes, portability fixes and other
    modifications derived from the Public Domain or from the
    Copyright Holder. A Package modified in such a way shall still
    be considered the Standard Version.

   3. You may otherwise modify your copy of this Package in any way,
    provided that you insert a prominent notice in each changed file
    stating how and when you changed that file, and provided that
    you do at least ONE of the following:

         1. place your modifications in the Public Domain or otherwise
         make them Freely Available, such as by posting said
         modifications to Usenet or an equivalent medium, or placing
         the modifications on a major archive site such as
         uunet.uu.net, or by allowing the Copyright Holder to include
         your modifications in the Standard Version of the Package.
         2. use the modified Package only within your corporation or
         organization.  3. rename any non-standard executables so the
         names do not conflict with standard executables, which must
         also be provided, and provide a separate manual page for each
         non-standard executable that clearly documents how it differs
         from the Standard Version.
         4. make other distribution arrangements with the Copyright
         4. Holder.

   4. You may distribute the programs of this Package in object code
    or executable form, provided that you do at least ONE of the
    following:

         1. distribute a Standard Version of the executables and
         library files, together with instructions (in the manual page
         or equivalent) on where to get the Standard Version.
         2. accompany the distribution with the machine-readable
         source of the Package with your modifications.  3. give
         non-standard executables non-standard names, and clearly
         document the differences in manual pages (or equivalent),
         together with instructions on where to get the Standard
         Version.
         4. make other distribution arrangements with the Copyright
         4. Holder.

   5. You may charge a reasonable copying fee for any distribution of
    this Package. You may charge any fee you choose for support of
    this Package. You may not charge a fee for this Package
    itself. However, you may distribute this Package in aggregate
    with other (possibly commercial) programs as part of a larger
    (possibly commercial) software distribution provided that you do
    not advertise this Package as a product of your own. You may
    embed this Package's interpreter within an executable of yours
    (by linking); this shall be construed as a mere form of
    aggregation, provided that the complete Standard Version of the
    interpreter is so embedded.

   6. The scripts and library files supplied as input to or produced
    as output from the programs of this Package do not automatically
    fall under the copyright of this Package, but belong to whomever
    generated them, and may be sold commercially, and may be
    aggregated with this Package. If such scripts or library files
    are aggregated with this Package via the so-called "undump" or
    "unexec" methods of producing a binary executable image, then
    distribution of such an image shall neither be construed as a
    distribution of this Package nor shall it fall under the
    restrictions of Paragraphs 3 and 4, provided that you do not
    represent such an executable image as a Standard Version of this
    Package.

   7. C subroutines (or comparably compiled subroutines in other
    languages) supplied by you and linked into this Package in order
    to emulate subroutines and variables of the language defined by
    this Package shall not be considered part of this Package, but
    are the equivalent of input as in Paragraph 6, provided these
    subroutines do not change the language in any way that would
    cause it to fail the regression tests for the language.

   8. Aggregation of this Package with a commercial distribution is
    always permitted provided that the use of this Package is
    embedded; that is, when no overt attempt is made to make this
    Package's interfaces visible to the end user of the commercial
    distribution. Such use shall not be construed as a distribution
    of this Package.

   9. The name of the Copyright Holder may not be used to endorse or
    promote products derived from this software without specific
    prior written permission.

  10. THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
   WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.

The End



 *
 * This file depends upon register.c, misc.c, babel.h, and treaty.h
 * and L. Peter Deutsch's md5.c
 * usage:
 *  char *babel_init(char *filename)
 *      Initializes babel to use the specified file. MUST be called before
 *      babel_treaty.  Returns the human-readable name of the format
 *      or NULL if the format is not known. Do not call babel_treaty unless
 *      babel_init returned a nonzero value.
 *      The returned string will be the name of a babel format, possibly
 *      prefixed by "blorbed " to indicate that babel will process this file
 *      as a blorb.
 * int32 babel_treaty(int32 selector, void *output, void *output_extent)
 *      Dispatches the call to the treaty handler for the currently loaded
 *      file.
 *      When processing a blorb, all treaty calls will be deflected to the
 *      special blorb handler.  For the case of GET_STORY_FILE_IFID_SEL,
 *      The treaty handler for the underlying format will be called if an
 *      IFID is not found in the blorb resources.
 * void babel_release()
 *      Frees all resources allocated during babel_init.
 *      You should do this even if babel_init returned NULL.
 *      After this is called, do not call babel_treaty until after
 *      another successful call to babel_init.
 * char *babel_get_format()
 *      Returns the same value as the last call to babel_init (ie, the format name)
 * int32 babel_md5_ifid(char *buffer, int extent);
 *      Generates an MD5 IFID from the loaded story.  Returns zero if something
 *      went seriously wrong.
 *
 * If you wish to use babel in multiple threads, you must use the contextualized
 * versions of the above functions.
 * Each function above has a companion function whose name ends in _ctx (eg.
 * "babel_treaty_ctx") which takes one additional argument.  This argument is
 * the babel context. A new context is returned by void *ctx=get_babel_ctx(),
 * and should be released when finished by calling release_babel_ctx(ctx);
 */

                      
#include "treaty.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "md5.h"

void *my_malloc(int, char *);

struct babel_handler
{
 TREATY treaty_handler;
 TREATY treaty_backup;
 void *story_file;
 uint32 story_file_extent;
 void *story_file_blorbed;
 uint32 story_file_blorbed_extent;
 char blorb_mode;
 char *format_name;
 char auth;
};

static struct babel_handler default_ctx;

extern TREATY treaty_registry[];
extern TREATY container_registry[];

static char *deeper_babel_init(char *story_name, void *bhp)
{
 struct babel_handler *bh=(struct babel_handler *) bhp;
 int i;
 char *ext;

 static char buffer[TREATY_MINIMUM_EXTENT];
 int best_candidate;
 char buffert[TREATY_MINIMUM_EXTENT];

 if (story_name)
  {
   ext=strrchr(story_name,'.');
   if (ext) for(i=0;ext[i];i++) ext[i]=tolower(ext[i]);
  }
 else ext=NULL;
 best_candidate=-1;
 if (ext) /* pass 1: try best candidates */
  for(i=0;container_registry[i];i++)
   if (container_registry[i](GET_FILE_EXTENSIONS_SEL,NULL,0,buffer,TREATY_MINIMUM_EXTENT) >=0 &&
       strstr(buffer,ext) &&
       container_registry[i](CLAIM_STORY_FILE_SEL,bh->story_file,bh->story_file_extent,NULL,0)>=NO_REPLY_RV)
    break;
  if (!ext || !container_registry[i]) /* pass 2: try all candidates */
  {
  
  for(i=0;container_registry[i];i++)
   {int l=container_registry[i](CLAIM_STORY_FILE_SEL,bh->story_file,bh->story_file_extent,NULL,0);
    
    if (l==VALID_STORY_FILE_RV)
    break;
    else if (l==NO_REPLY_RV && best_candidate < 0) best_candidate=i;
    }
}
 if (!container_registry[i] && best_candidate >=0) { bh->auth=0; i=best_candidate; }
 if (container_registry[i])
 {
   char buffer2[TREATY_MINIMUM_EXTENT];
   
   bh->treaty_handler=container_registry[i];
   container_registry[i](GET_FORMAT_NAME_SEL,NULL,0,buffert,TREATY_MINIMUM_EXTENT);
   bh->blorb_mode=1;

   bh->story_file_blorbed_extent=container_registry[i](CONTAINER_GET_STORY_EXTENT_SEL,bh->story_file,bh->story_file_extent,NULL,0);
   if (bh->story_file_blorbed_extent>0) bh->story_file_blorbed=my_malloc(bh->story_file_blorbed_extent, "contained story file");
   if (bh->story_file_blorbed_extent<=0 ||
       container_registry[i](CONTAINER_GET_STORY_FORMAT_SEL,bh->story_file,bh->story_file_extent,buffer2,TREATY_MINIMUM_EXTENT)<0 ||
       container_registry[i](CONTAINER_GET_STORY_FILE_SEL,bh->story_file,bh->story_file_extent,bh->story_file_blorbed,bh->story_file_blorbed_extent)<=0
      )
    return NULL;
 
   for(i=0;treaty_registry[i];i++)
    if (treaty_registry[i](GET_FORMAT_NAME_SEL,NULL,0,buffer,TREATY_MINIMUM_EXTENT)>=0 &&
        strcmp(buffer,buffer2)==0 &&
        treaty_registry[i](CLAIM_STORY_FILE_SEL,bh->story_file_blorbed,bh->story_file_blorbed_extent,NULL,0)>=NO_REPLY_RV)
     break;
  if (!treaty_registry[i])
   return NULL;
  bh->treaty_backup=treaty_registry[i];
  sprintf(buffer,"%sed %s",buffert,buffer2);
  return buffer;
  }

 bh->blorb_mode=0;
 best_candidate=-1;

 if (ext) /* pass 1: try best candidates */
  for(i=0;treaty_registry[i];i++)
   if (treaty_registry[i](GET_FILE_EXTENSIONS_SEL,NULL,0,buffer,TREATY_MINIMUM_EXTENT) >=0 &&
       strstr(buffer,ext) && 
       treaty_registry[i](CLAIM_STORY_FILE_SEL,bh->story_file,bh->story_file_extent,NULL,0)>=NO_REPLY_RV)
    break;
  if (!ext || !treaty_registry[i]) /* pass 2: try all candidates */
  {
  
  for(i=0;treaty_registry[i];i++)
   {int l;
   l=treaty_registry[i](CLAIM_STORY_FILE_SEL,bh->story_file,bh->story_file_extent,NULL,0);

    if (l==VALID_STORY_FILE_RV)
    break;
    else if (l==NO_REPLY_RV && best_candidate < 0) best_candidate=i;
    }
  }
  if (!treaty_registry[i])
   if (best_candidate>0) { i=best_candidate; bh->auth=0; }
   else return NULL;
  bh->treaty_handler=treaty_registry[i];

  if (bh->treaty_handler(GET_FORMAT_NAME_SEL,NULL,0,buffer,TREATY_MINIMUM_EXTENT)>=0)
  return buffer;
  return NULL;


}

static char *deep_babel_init(char *story_name, void *bhp)
{
 struct babel_handler *bh=(struct babel_handler *) bhp;
 FILE *file;

 bh->treaty_handler=NULL;
 bh->treaty_backup=NULL;
 bh->story_file=NULL;
 bh->story_file_extent=0;
 bh->story_file_blorbed=NULL;
 bh->story_file_blorbed_extent=0;
 bh->format_name=NULL;
 file=fopen(story_name, "rb");
 if (!file) return NULL;
 fseek(file,0,SEEK_END);
 bh->story_file_extent=ftell(file);
 fseek(file,0,SEEK_SET);
 bh->auth=1; 
 bh->story_file=my_malloc(bh->story_file_extent,"story file storage");
 fread(bh->story_file,1,bh->story_file_extent,file);
 fclose(file);

 return deeper_babel_init(story_name, bhp);
}

char *babel_init_ctx(char *sf, void *bhp)
{
 struct babel_handler *bh=(struct babel_handler *) bhp;
 char *b;
 b=deep_babel_init(sf,bh);
 if (b) bh->format_name=strdup(b);
 return b;
}
char *babel_init(char *sf)
{
  return babel_init_ctx(sf, &default_ctx);
}

char *babel_init_raw_ctx(void *sf, int32 extent, void *bhp)
{
 struct babel_handler *bh=(struct babel_handler *) bhp;
 char *b;
 bh->treaty_handler=NULL;
 bh->treaty_backup=NULL;
 bh->story_file=NULL;
 bh->story_file_extent=0;
 bh->story_file_blorbed=NULL;
 bh->story_file_blorbed_extent=0;
 bh->format_name=NULL;
 bh->story_file_extent=extent;
 bh->auth=1; 
 bh->story_file=my_malloc(bh->story_file_extent,"story file storage");
 memcpy(bh->story_file,sf,extent);

 b=deeper_babel_init(NULL, bhp);
 if (b) bh->format_name=strdup(b);
 return b;
}
char *babel_init_raw(void *sf, int32 extent)
{
  return babel_init_raw_ctx(sf, extent, &default_ctx);
}

void babel_release_ctx(void *bhp)
{
 struct babel_handler *bh=(struct babel_handler *) bhp;
 if (bh->story_file) free(bh->story_file);
 bh->story_file=NULL;
 if (bh->story_file_blorbed) free(bh->story_file_blorbed);
 bh->story_file_blorbed=NULL;
 if (bh->format_name) free(bh->format_name);
 bh->format_name=NULL;
}
void babel_release()
{
 babel_release_ctx(&default_ctx);
}
int32 babel_md5_ifid_ctx(char *buffer, int32 extent, void *bhp)
{
 struct babel_handler *bh=(struct babel_handler *) bhp;
 md5_state_t md5;
 int i;
 unsigned char ob[16];
 if (extent <33 || bh->story_file==NULL)
  return 0;
 md5_init(&md5);
 md5_append(&md5,bh->story_file,bh->story_file_extent);
 md5_finish(&md5,ob);
 for(i=0;i<16;i++)
  sprintf(buffer+(2*i),"%02X",ob[i]);
 buffer[32]=0;
 return 1;

}
int32 babel_md5_ifid(char *buffer, int32 extent)
{
 return babel_md5_ifid_ctx(buffer, extent,
                &default_ctx);
}

int32 babel_treaty_ctx(int32 sel, void *output, int32 output_extent,void *bhp)
{
 int32 rv;
 struct babel_handler *bh=(struct babel_handler *) bhp;
 if (!(sel & TREATY_SELECTOR_INPUT) && bh->blorb_mode)
  rv=bh->treaty_backup(sel,bh->story_file_blorbed,bh->story_file_blorbed_extent,output, output_extent);
 else
 {
  rv=bh->treaty_handler(sel,bh->story_file,bh->story_file_extent,output,output_extent);
  if ((!rv|| rv==UNAVAILABLE_RV) && bh->blorb_mode)
   rv=bh->treaty_backup(sel,bh->story_file_blorbed,bh->story_file_blorbed_extent,output, output_extent);
  }
 if (!rv && sel==GET_STORY_FILE_IFID_SEL)
  return babel_md5_ifid_ctx(output,output_extent, bh);
 if (rv==INCOMPLETE_REPLY_RV && sel==GET_STORY_FILE_IFID_SEL)
  return babel_md5_ifid_ctx((void *)((char *) output+strlen((char *)output)),
                            output_extent-strlen((char *)output),
                            bh);

 return rv;
}
int32 babel_treaty(int32 sel, void *output, int32 output_extent)
{
 return babel_treaty_ctx(sel, output, output_extent, &default_ctx);
}
char *babel_get_format_ctx(void *bhp)
{
 struct babel_handler *bh=(struct babel_handler *) bhp;
 return bh->format_name;
}
char *babel_get_format()
{
 return babel_get_format_ctx(&default_ctx);
}
void *get_babel_ctx()
{
 return my_malloc(sizeof(struct babel_handler), "babel handler context");
}
void release_babel_ctx(void *b)
{
 free(b);
}

uint32 babel_get_length_ctx(void *bhp)
{
 struct babel_handler *bh=(struct babel_handler *) bhp;
 return bh->story_file_extent;
}
uint32 babel_get_length()
{
 return babel_get_length_ctx(&default_ctx);
}

int32 babel_get_authoritative_ctx(void *bhp)
{
 struct babel_handler *bh=(struct babel_handler *) bhp;
 return bh->auth;
}
int32 babel_get_authoritative()
{
  return babel_get_authoritative_ctx(&default_ctx);
}
void *babel_get_file_ctx(void *bhp)
{
 struct babel_handler *bh=(struct babel_handler *) bhp;
 return bh->story_file;
}
void *babel_get_file()
{
 return babel_get_file_ctx(&default_ctx);
}

uint32 babel_get_story_length_ctx(void *ctx)
{
  struct babel_handler *bh=(struct babel_handler *) ctx;
  if (bh->blorb_mode) return bh->story_file_blorbed_extent;
  return bh->story_file_extent;
}
uint32 babel_get_story_length()
{

 return babel_get_story_length_ctx(&default_ctx);
}
void *babel_get_story_file_ctx(void *ctx)
{
  struct babel_handler *bh=(struct babel_handler *) ctx;
  if (bh->blorb_mode) return bh->story_file_blorbed;
  return bh->story_file;
}
void *babel_get_story_file()
{
 return babel_get_story_file_ctx(&default_ctx);
}
