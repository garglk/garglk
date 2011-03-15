/* blorb.c      Babel interface to blorb files
 * Copyright 2006 by L. Ross Raszewski
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
 * This file depends upon treaty_builder.h, misc.c and ifiction.c
 *
 * Header note: to add support for new executable chunk types, see
 * TranslateExec.
 *
 * This file defines a Treaty of Babel compatable module for handling blorb
 * files.  However, blorb files are not themselves a babel format. This module
 * is used internally by the babel program to handle blorbs.
 *
 * As a result, if GET_STORY_FILE_IFID_SEL returns NO_REPLY_RV,
 * you should check the story file against the babel registry before resorting
 * to the default IFID calculation.
 *
 */
#define FORMAT blorb
#define HOME_PAGE "http://eblong.com/zarf/blorb"
#define FORMAT_EXT ".blorb,.blb,.zblorb,.zlb,.gblorb,.glb"
#define CONTAINER_FORMAT
#include "treaty_builder.h"
#include <stdlib.h>
#include <ctype.h>

extern TREATY treaty_registry[];
/* The following is the translation table of Blorb chunk types to
   babel formats.  it is NULL-terminated. */
static char *TranslateExec[] = { "ZCOD", "zcode",
                                 "GLUL", "glulx",
                                 "TAD2", "tads2",
                                 "TAD3", "tads3",
                                 NULL, NULL };

void *my_malloc(int32, char *);
int32 ifiction_get_IFID(char *, char *, int32);

static uint32 read_int(void *inp)
{
  unsigned char *mem=(unsigned char *)inp;
  uint32 i4 = mem[0],
                    i3 = mem[1],
                    i2 = mem[2],
                    i1 = mem[3];
  return i1 | (i2<<8) | (i3<<16) | (i4<<24);
}


static int32 blorb_get_chunk(void *blorb_file, int32 extent, char *id, uint32 *begin, uint32 *output_extent)
{
 uint32 i=12, j;
 while(i<(uint32) extent-8)
 {
  if (memcmp(((char *)blorb_file)+i,id,4)==0)
  {
   *output_extent=read_int((char *)blorb_file+i+4);
   if (*output_extent > (uint32) extent) return NO_REPLY_RV;
   *begin=i+8;
   return 1;
  }

  j=read_int((char *)blorb_file+i+4);
  if (j%2) j++;
  i+=j+8;

 }
 return NO_REPLY_RV;
}
static int32 blorb_get_resource(void *blorb_file, int32 extent, char *rid, int32 nnumber, uint32 *begin, uint32 *output_extent)
{
 uint32 ridx_len;
 uint32 i,j;
 uint32 number=(uint32) nnumber;
 void *ridx;
 if (blorb_get_chunk(blorb_file, extent,"RIdx",&i,&ridx_len)==NO_REPLY_RV)
  return NO_REPLY_RV;

 ridx=(char *)blorb_file+i+4;
 ridx_len=read_int((char *)blorb_file+i);
 for(i=0;i<ridx_len;i++)
 { 
  if (memcmp((char *)ridx+(i*12),rid,4)==0 && read_int((char *)ridx+(i*12)+4)==number)
  {
   j=i;
   i=read_int((char *)ridx+(j*12)+8);
   *begin=i+8;
   *output_extent=read_int((char *)blorb_file+i+4);
   return 1;
  }
 }
 return NO_REPLY_RV;
}
static int32 blorb_get_story_file(void *blorb_file, int32 extent, uint32 *begin, uint32 *output_extent)
{
 return blorb_get_resource(blorb_file, extent, "Exec", 0, begin, output_extent);

}

static int32 get_story_extent(void *blorb_file, int32 extent)
{
 uint32 i,j;
 if (blorb_get_resource(blorb_file, extent, "Exec", 0, &i, &j))
 {
  return j;
 }
 return NO_REPLY_RV;

}
static int32 get_story_file(void *blorb_file, int32 extent, void *output, int32 output_extent)
{
 uint32 i,j;
 if (blorb_get_resource(blorb_file, extent, "Exec", 0, &i, &j))
 {
  ASSERT_OUTPUT_SIZE((int32) j);
  memcpy(output,(char *)blorb_file+i,j);
  return j;
 }
 return NO_REPLY_RV;

}

char *blorb_chunk_for_name(char *name)
{
 static char buffer[5];
 int j;
 for(j=0;TranslateExec[j];j+=2)
  if (strcmp(name,TranslateExec[j+1])==0) return TranslateExec[j];
 for(j=0;j<4 && name[j];j++) buffer[j]=toupper(buffer[j]);
 while(j<4) buffer[j++]=' ';
 buffer[4]=0;
 return buffer;

}
static char *blorb_get_story_format(void *blorb_file, int32 extent)
{
 uint32 i, j;

 for(j=0;treaty_registry[j];j++)
 {
  static char fn[512];
  treaty_registry[j](GET_FORMAT_NAME_SEL,NULL,0,fn,512);
  if (blorb_get_chunk(blorb_file,extent,blorb_chunk_for_name(fn),&i, &i)) return fn;
 }
 return NULL;
}

static int32 get_story_format(void *blorb_file, int32 extent, char *output, int32 output_extent)
{
 char *o;
 o=blorb_get_story_format(blorb_file, extent);
 if (!o) return NO_REPLY_RV;
 ASSERT_OUTPUT_SIZE((signed) strlen(o)+1);
 strcpy(output,o);
 return strlen(o)+1;
}
static int32 get_story_file_metadata_extent(void *blorb_file, int32 extent)
{
 uint32 i,j;
 if (blorb_get_chunk(blorb_file,extent,"IFmd",&i,&j)) return j+1;
 return NO_REPLY_RV;
}
static int32 blorb_get_cover(void *blorb_file, int32 extent, uint32 *begin, uint32 *output_extent)
{
 uint32 i,j;
 if (blorb_get_chunk(blorb_file,extent,"Fspc",&i,&j))
 {
  if (j<4) return NO_REPLY_RV;
  i=read_int((char *)blorb_file+i);
  if (!blorb_get_resource(blorb_file,extent,"Pict",i,&i,&j)) return NO_REPLY_RV;
  *begin=i;
  *output_extent=j;
  if (memcmp((char *)blorb_file+i-8,"PNG ",4)==0) return PNG_COVER_FORMAT;
  else if (memcmp((char *)blorb_file+i-8,"JPEG",4)==0) return JPEG_COVER_FORMAT;
 }
 return NO_REPLY_RV;

}

static int32 get_story_file_cover_extent(void *blorb_file, int32 extent)
{
 uint32 i,j;
 if (blorb_get_cover(blorb_file,extent,&i,&j)) return j;
 return NO_REPLY_RV;
}

static int32 get_story_file_cover_format(void *blorb_file, int32 extent)
{
 uint32 i,j;
 return blorb_get_cover(blorb_file, extent, &i,&j);
}

static int32 get_story_file_IFID(void *b, int32 e, char *output, int32 output_extent)
{
 int32 j;
 char *md;
 j=get_story_file_metadata_extent(b,e);
 if (j<=0) return NO_REPLY_RV;
 md=(char *)my_malloc(j, "Metadata buffer");
 j=get_story_file_metadata(b,e,md,j);
 if (j<=0) return NO_REPLY_RV;

 j=ifiction_get_IFID(md,output,output_extent);
 free(md);
 return j;
}

static int32 get_story_file_metadata(void *blorb_file, int32 extent, char *output, int32 output_extent)
{
 uint32 i,j;
 if (!blorb_get_chunk(blorb_file, extent,"IFmd",&i,&j)) return NO_REPLY_RV;
 ASSERT_OUTPUT_SIZE((int32) j+1);
 memcpy(output,(char *)blorb_file+i,j);
 output[j]=0;
 return j+1;
}
static int32 get_story_file_cover(void *blorb_file, int32 extent, void *output, int32 output_extent)
{
 uint32 i,j;
 if (!blorb_get_cover(blorb_file, extent,&i,&j)) return NO_REPLY_RV;
 ASSERT_OUTPUT_SIZE((int32) j);
 memcpy(output,(char *)blorb_file+i,j);
 return j;
}

static int32 claim_story_file(void *story_file, int32 extent)
{
 int i;

 if (extent<16 ||
     memcmp(story_file,"FORM",4) ||
     memcmp((char *)story_file+8,"IFRS",4) 
    ) i= INVALID_STORY_FILE_RV;
 else i= NO_REPLY_RV;
 
 return i;

}
