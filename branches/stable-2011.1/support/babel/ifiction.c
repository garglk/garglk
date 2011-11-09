/* ifiction.c  common babel interface for processing ifiction metadata
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
 * This file depends on treaty.h
 *
 * This file contains common routines for handling ifiction metadata strings
 *
 * int32 ifiction_get_IFID(char *metadata, char *output, int32 output_extent)
 * does what the babel treaty function GET_STORY_FILE_IFID_SEL would do for ifiction
 *
 * void ifiction_parse(char *md, IFCloseTag close_tag, void *close_ctx,
 *                     IFErrorHandler error_handler, void *error_ctx)
 * parses the given iFiction metadata.  close_tag(struct XMLtag xtg, close_ctx)
 * is called for each tag as it is closed, error_handler(char *error, error_ctx)
 * is called each time a structural or logical error is found in the iFiction
 * This is a very simple XML parser, and probably not as good as any "real"
 * XML parser.  Its only two benefits are that (1) it's really small, and (2)
 * it strictly checks the ifiction record against the Treaty of Babel
 * requirements
 *
 */

#include "ifiction.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

void *my_malloc(int, char *);
extern char *format_registry[];


static int32 llp;
static char *lnlst;

static char utfeol[3] = { 0xe2, 0x80, 0xa8 };
static int32 getln(char *endp)
{
 for(;lnlst<endp;lnlst++) if (*lnlst=='\n' || memcmp(lnlst,utfeol,3)==0) llp++;
 return llp;
}


static int32 ifiction_get_first_IFID(char *metadata, char *output, int32 output_extent)
{
 char *ifid_begin, *ifid_end;
 
 ifid_begin=strstr(metadata,"<ifid>");
 if (!ifid_begin) return NO_REPLY_RV;
 ifid_begin+=6;

 ifid_end=strstr(ifid_begin,"</ifid>");
 if (!ifid_end) return NO_REPLY_RV;
 if (output_extent<=(ifid_end-ifid_begin)) return INVALID_USAGE_RV;

 memcpy(output,ifid_begin,ifid_end-ifid_begin);

 output[ifid_end-ifid_begin]=0;

 return ifid_end-metadata+7;
}


int32 ifiction_get_IFID(char *metadata, char *output, int32 output_extent)
{
 int32 j=0, k;

 while(*metadata)
 {
 if ((k=ifiction_get_first_IFID(metadata,output,output_extent)) <= 0) break;
 j++;
 metadata+=k;
 output_extent-=strlen(output)+1;
 output+=strlen(output);
 *output=',';
 output++;
 }
 if (*(output-1)==',') *(output-1)=0;
 return j;
}


static char *leaf_tags[] = { "ifid",
                             "format",
                             "bafn",
                             "title",
                             "author",
                             "headline",
                             "firstpublished",
                             "genre",
                             "group",
                             "description",
                             "leafname",
                             "url",
                             "authoremail",
                             "height",
                             "width",

                             NULL
                             };
static char *one_per[] = { "identification",
                           "bibliographic",
                           "format",
                           "title",
                           "author",
                           "headline",
                           "firstpublished",
                           "genre",
                           "group",
                           "description",
                           "leafname",
                           "height",
                           "width",
                           "forgiveness",
                           "colophon",
                           NULL
                         };

static char *required[] = {
                "cover", "height",
                "cover", "width",
                "cover", "format",
                "resources", "auxiliary",
                "auxiliary", "leafname",
                "auxiliary", "description",
                "ifiction", "story",
                "story", "identification",
                "story", "bibliographic",
                "identification", "ifid",
                "identification", "format",
                "bibliographic", "title",
                "bibliographic", "author",
                "colophon", "generator",
                "colophon", "originated",
                NULL, NULL
                };
static char *zarfian[] = {
        "Merciful",
        "Polite",
        "Tough",
        "Nasty",
        "Cruel",
        NULL
        };

struct ifiction_info {
        int32 width;
        int32 height;
        int format;
        };
static void ifiction_validate_tag(struct XMLTag *xtg, struct ifiction_info *xti, IFErrorHandler err_h, void *ectx)
{
 int i;
 char ebuf[512];
 struct XMLTag *parent=xtg->next;
 if (parent)
 {
 for(i=0;leaf_tags[i];i++)
  if (strcmp(parent->tag,leaf_tags[i])==0)
   {
    sprintf(ebuf, "Error: (line %d) Tag <%s> is not permitted within tag <%s>",
        xtg->beginl,xtg->tag,parent->tag);
    err_h(ebuf,ectx);
    }
 for(i=0;required[i];i+=2)
 if (strcmp(required[i],parent->tag)==0 && strcmp(required[i+1],xtg->tag)==0)
  parent->rocurrences[i]=1;
 for(i=0;one_per[i];i++)
 if (strcmp(one_per[i],xtg->tag)==0)
  if (parent->occurences[i]) { 
                               sprintf(ebuf,"Error: (line %d) Found more than one <%s> within <%s>",xtg->beginl,xtg->tag,
                                        parent->tag);
                               err_h(ebuf,ectx);
                             }
   else parent->occurences[i]=1;
 }
 for(i=0;required[i];i+=2)
 if (strcmp(required[i],xtg->tag)==0 && !xtg->rocurrences[i])
 {
  sprintf(ebuf,"Error: (line %d) Tag <%s> is required within <%s>",xtg->beginl, required[i+1],xtg->tag);
  err_h(ebuf,ectx);
 }
 if (parent && strcmp(parent->tag,"identification")==0)
 {
  if (strcmp(xtg->tag,"format")==0)
  {
   int i;
   for(i=0;format_registry[i];i++) if (memcmp(xtg->begin,format_registry[i],strlen(format_registry[i]))==0) break;
   if (format_registry[i]) xti->format=i;
   else
   {
    char bf[256];
    memcpy(bf,xtg->begin,xtg->end-xtg->begin);
    bf[xtg->end-xtg->begin]=0;
    xti->format=-1;
    sprintf(ebuf,"Warning: (line %d) Unknown format %s.",xtg->beginl,bf);
    err_h(ebuf,ectx);
   }
  }
 }
 if (parent && strcmp(parent->tag,"cover")==0)
 {
 if (strcmp(xtg->tag,"width")==0)
 {
  int i;
  sscanf(xtg->begin,"%d",&i);
  if (i<120)
  {
  sprintf(ebuf,"Warning: (line %d) Cover art width should not be less than 120.",xtg->beginl);
  err_h(ebuf,ectx);
  }
  if (i>1200)
  {
  sprintf(ebuf,"Warning: (line %d) Cover art width should not exceed 1200.",xtg->beginl);
  err_h(ebuf,ectx);
  }
  if (!xti->width) xti->width=i;
  if (xti->height && (xti->width> 2 * xti->height || xti->height > 2 * xti->width))
  {
  sprintf(ebuf,"Warning: (line %d) Cover art aspect ratio exceeds 2:1.",xtg->beginl);
  err_h(ebuf,ectx);
  }

 }
 if (strcmp(xtg->tag,"height")==0)
 {
  int i;
  sscanf(xtg->begin,"%d",&i);
  if (i<120)
  {
  sprintf(ebuf,"Warning: (line %d) Cover art height should not be less than 120.",xtg->beginl);
  err_h(ebuf,ectx);
  }
  if (i>1200)
  {
  sprintf(ebuf,"Warning: (line %d) Cover art height should not exceed 1200.",xtg->beginl);
  err_h(ebuf,ectx);
  }
  if (!xti->height) xti->height=i;
  if (xti->width && (xti->width> 2 * xti->height || xti->height > 2 * xti->width))
  {
  sprintf(ebuf,"Warning: (line %d) Cover art aspect ratio exceeds 2:1.",xtg->beginl);
  err_h(ebuf,ectx);
  }

 }
 if (strcmp(xtg->tag,"format")==0 && memcmp(xtg->begin,"jpg",3) && memcmp(xtg->begin,"png",3))
 {
  sprintf(ebuf,"Warning: (line %d) <format> should be one of: png, jpg.",xtg->beginl);
  err_h(ebuf,ectx);
 }
 }
 if (parent && strcmp(parent->tag,"bibliographic")==0)
 {
  char *p;
  if (strcmp(xtg->tag,"description")) {
  if (isspace(*xtg->begin)|| isspace(*(xtg->end-1)))
   {
    sprintf(ebuf,"Warning: (line %d) Extraneous spaces at beginning or end of tag <%s>.",xtg->beginl,xtg->tag);
    err_h(ebuf,ectx);
   }
  for(p=xtg->begin;p<xtg->end-1;p++)

  if (isspace(*p) && isspace(*(p+1)))
  {
  sprintf(ebuf,"Warning: (line %d) Extraneous spaces found in tag <%s>.",xtg->beginl, xtg->tag);
  err_h(ebuf,ectx);
  }
  else if (isspace(*p) && *p!=' ')
  {
  sprintf(ebuf,"Warning: (line %d) Improper whitespace character found in tag <%s>.",xtg->beginl, xtg->tag);
  err_h(ebuf,ectx);

  }
 }
 if (strcmp(xtg->tag, "description") && xtg->end-xtg->begin > 240)
 { 
  sprintf(ebuf,"Warning: (line %d) Tag <%s> length exceeds treaty guidelines",xtg->beginl, xtg->tag);
  err_h(ebuf,ectx);
 }
 if (strcmp(xtg->tag, "description")==0 && xtg->end-xtg->begin > 2400)
 {
  sprintf(ebuf,"Warning: (line %d) Tag <%s> length exceeds treaty guidelines",xtg->beginl, xtg->tag);
  err_h(ebuf,ectx);
 }
 if (strcmp(xtg->tag,"firstpublished")==0)
 {
  int l=xtg->end-xtg->begin;
  if ((l!=4 && l!=10) ||
      (!isdigit(xtg->begin[0]) ||
       !isdigit(xtg->begin[1]) ||
       !isdigit(xtg->begin[2]) ||
       !isdigit(xtg->begin[3])) ||
      (l==10 && ( xtg->begin[4]!='-' ||
                  xtg->begin[7]!='-' ||
                  !isdigit(xtg->begin[5]) ||
                  !isdigit(xtg->begin[6]) ||
                  !(xtg->begin[5]=='0' || xtg->begin[5]=='1') ||
                  !(xtg->begin[5]=='0' || xtg->begin[6]<='2') ||
                  !isdigit(xtg->begin[8]) ||
                  !isdigit(xtg->begin[9]))))
  {
   sprintf(ebuf,"Warning: (line %d) Tag <%s> should be format YYYY or YYYY-MM-DD",xtg->beginl, xtg->tag);
   err_h(ebuf,ectx);
  }
 }
 if (strcmp(xtg->tag,"seriesnumber")==0)
 {
  char *l;
  if (*xtg->begin=='0' && xtg->end!=xtg->begin+1)
  {
   sprintf(ebuf,"Warning: (line %d) Tag <%s> should not use leading zeroes",xtg->beginl, xtg->tag);
   err_h(ebuf,ectx);
  }

  for(l=xtg->begin;l<xtg->end;l++) if (!isdigit(*l))
  {
   sprintf(ebuf,"Warning: (line %d) Tag <%s> should be a positive number",xtg->beginl, xtg->tag);
   err_h(ebuf,ectx);
  }
 }
 if (strcmp(xtg->tag,"forgiveness")==0)
 {
  int l;
  for(l=0;zarfian[l];l++) if (memcmp(xtg->begin,zarfian[l],strlen(zarfian[l]))==0) break;
  if (!zarfian[l])
  {
   sprintf(ebuf,"Warning: (line %d) <forgiveness> should be one of: Merciful, Polite, Tough, Cruel",xtg->beginl);
   err_h(ebuf,ectx);
  }
 }
 }
 if (xti->format>0)
 { 
  for(i=0;format_registry[i];i++) if (strcmp(xtg->tag,format_registry[i])==0) break;
  if (format_registry[i] && xti->format !=i)
  {
  sprintf(ebuf,"Warning: (line %d) Found <%s> tag, but story is identified as %s.",xtg->beginl, xtg->tag, format_registry[xti->format]);
  err_h(ebuf,ectx);
  }
 }
 if (strcmp(xtg->tag,"story")==0)
 {
  xti->format=-1;
  xti->width=0;
  xti->height=0;
 }

}



void ifiction_parse(char *md, IFCloseTag close_tag, void *close_ctx, IFErrorHandler error_handler, void *error_ctx)
{
char *xml, buffer[2400], *aep, *mda=md, ebuffer[512];
struct XMLTag *parse=NULL, *xtg;
struct ifiction_info xti;
char BOM[3]={ 0xEF, 0xBB, 0xBF};
xti.width=0;
xti.height=0;
xti.format=-1;
llp=1;
lnlst=md;

while(*mda && isspace(*mda)) mda++;
if (memcmp(mda,BOM,3)==0)
{ mda+=3;
  while(*mda && isspace(*mda)) mda++;
}


if (strncmp("<?xml version=\"1.0\" encoding=\"UTF-8\"?>",mda,
        strlen("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"))
    &&
    strncmp("<?xml version=\"1.0\" encoding=\"utf-8\"?>",mda,
        strlen("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"))
   )
{
 error_handler("Error: XML header not found.",error_ctx);
 return;
}

xml=strstr(md,"<ifindex");
if (!xml) {
 error_handler("Error: <ifindex> not found",error_ctx);
 return;
 }
while(xml && *xml)
{
 char *bp, *ep, *tp;
 while(*xml&&*xml!='<') xml++;
 if (!*xml) break;
 bp=xml;
 if (strlen(bp)>4 && bp[1]=='!' && bp[2]=='-' && bp[3]=='-')
 { bp=strstr(bp+1,"-->");
   if (bp) xml=bp+3; else xml=0;
   continue;
}
 tp=strchr(bp+1,'<');
 ep=strchr(bp+1,'>');
 if (!ep) break;
 if (tp && tp < ep)
  { xml=tp; continue; }
 if (!tp) tp=ep+1; 
 if (bp[1]=='/') /* end tag */
 {
   strncpy(buffer,bp+2,(ep-bp)-2);
   buffer[(ep-bp)-2]=0;
   if (parse && strcmp(buffer,parse->tag)==0)
   { /* copasetic. Close the tag */
    xtg=parse;
    parse=xtg->next;
    xtg->end=ep-strlen(buffer)-2;
    ifiction_validate_tag(xtg,&xti,error_handler, error_ctx);
    close_tag(xtg,close_ctx);
    free(xtg);
   }
   else
   {
    for(xtg=parse;xtg && strcmp(buffer,xtg->tag);xtg=xtg->next);
    if (xtg) /* Intervening unclosed tags */
    { for(xtg=parse;xtg && strcmp(buffer,parse->tag);xtg=parse)
     {
      xtg->end=xml-1;
      parse=xtg->next;
      sprintf(ebuffer,"Error: (line %d) unclosed <%s> tag",xtg->beginl,xtg->tag);
      error_handler(ebuffer,error_ctx);
      ifiction_validate_tag(xtg,&xti,error_handler, error_ctx);
      close_tag(xtg,close_ctx);
      free(xtg);
     }
     xtg=parse;
     if (xtg)
     {
      xtg->end=xml-1;
      parse=xtg->next;
      ifiction_validate_tag(xtg,&xti, error_handler, error_ctx);
      close_tag(xtg,close_ctx);
      free(xtg);
     }
    }
    else
    { 
      sprintf(ebuffer,"Error: (line %d) saw </%s> without <%s>",getln(xml), buffer,buffer);
      error_handler(ebuffer,error_ctx);
    }
   }

 }
 else if(*(ep-1)=='/' || bp[1]=='!') /* unterminated tag */
 {
  /* Do nothing */
 }
 else /* Terminated tag beginning */
 {
  int i;
  xtg=(struct XMLTag *)my_malloc(sizeof(struct XMLTag),"XML Tag");
  xtg->next=parse;
  xtg->beginl=getln(bp);
  for(i=0;bp[i+1]=='_' || bp[i+1]=='-' || isalnum(bp[i+1]);i++)
   xtg->tag[i]=bp[i+1];
  if (i==0)
  { xml=tp;
    free(xtg);
    continue;
  }
  parse=xtg;
  parse->tag[i]=0;
  strncpy(parse->fulltag,bp+1,ep-bp-1);
  parse->fulltag[ep-bp-1]=0;
  parse->begin=ep+1;
 }
 xml=tp;
}
 while (parse)
 {
      xtg=parse;
      xtg->end=aep-1;
      parse=xtg->next;
      sprintf(ebuffer,"Error: (line %d) Unclosed tag <%s>",xtg->beginl,xtg->tag);
      ifiction_validate_tag(xtg,&xti,error_handler, error_ctx);
      close_tag(xtg,close_ctx);
      free(xtg);
 }
}

struct get_tag
{
 char *tag;
 char *parent;
 char *output;
 char *target;
};

static void ifiction_null_eh(char *e, void *c)
{
 if (e || c) { }

}

static void ifiction_find_value(struct XMLTag *xtg, void *xti)
{
 struct get_tag *gt=(struct get_tag *)xti;

 if (gt->output && !gt->target) return;
 if (gt->target && gt->output && strcmp(gt->output,gt->target)==0) { gt->target=NULL; free(gt->output); gt->output=NULL; }
 if (((!xtg->next && !gt->parent) || (xtg->next && gt->parent && strcmp(xtg->next->tag,gt->parent)==0)) &&
      strcmp(xtg->tag,gt->tag)==0)
 {
  int32 l = xtg->end-xtg->begin;

  if (gt->output) free(gt->output);
  gt->output=(char *)my_malloc(l+1, "ifiction tag buffer");
  memcpy(gt->output, xtg->begin, l);
  gt->output[l]=0;

 }
}


char *ifiction_get_tag(char *md, char *p, char *t, char *from)
{
 struct get_tag gt;
 gt.output=NULL;
 gt.parent=p;
 gt.tag=t;
 gt.target=from;
 ifiction_parse(md,ifiction_find_value,&gt,ifiction_null_eh,NULL);
 if (gt.target){ if (gt.output) free(gt.output); return NULL; }
 return gt.output;
}
