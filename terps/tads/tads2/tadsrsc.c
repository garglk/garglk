#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/TADSRSC.C,v 1.2 1999/05/17 02:52:13 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsrsc.c - TADS v2 resource manager
Function
  resource manager for TADS v2
Notes
  Somewhat improved over TADS v1 resource manager thanks to new file
  format.  This version can list, add, replace, and remove resources.
Modified
  04/30/92 MJRoberts     - creation
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "os.h"
#include "std.h"
#include "fio.h"

/* printf */
#define rscptf printf

static void usage(void)
{
    rscptf("usage: tadsrsc [fileoptions] <resfile> [operations]\n"
           "   or: tadsrsc [fileoptions] <resfile> @<opfile>"
           " - read ops from <opfile>\n"
           "If no operations are given, <resfile> contents are listed.\n"
           "File options:\n"
           "  -create      - create a new resource file\n"
           "Operations:\n"
           "  -type <type> - use <type> as the type for following "
           "resources\n"
           "  -replace     - add or replace resources that follow\n"
           "  -delete      - remove following resources from resfile\n"
           "  -add         - add following resources, but do not replace if "
           "present\n"
           "  <file>       - add/delete/replace resource, using filename "
           "as resource name\n"
           "  <dir>        - add/delete/replace all files in directory\n"
           "  <file>=<res> - add/delete/replace resource, using <res> as "
           "resource name\n"
           "-replace is assumed if no conflicting option is specified\n"
           "If no -type option is specified, types are inferred from "
           "filename suffix:\n"
           ".bin, .com, and no '.' are assumed to be XFCN, all others "
           "HTML.\n"
           "Valid resource types:\n"
           "  XFCN  - external function (executable code)\n"
           "  HTML  - graphic/sound resource for HTML TADS (JPEG, etc)\n");
    os_term(OSEXFAIL);
}

static void errexit(char *msg, long arg)
{
    rscptf(msg, arg);
    rscptf("\n");
    os_term(OSEXFAIL);
}


static void listexit(osfildef *fp, char *msg)
{
    if (fp != 0)
        osfcls(fp);
    errexit(msg, 1);
}

typedef struct opdef opdef;
struct opdef
{
    opdef  *opnxt;                                /* next operation in list */
    char   *opres;                                         /* resource name */
    char   *opfile;                                          /* file to add */
    int     opflag;               /* indication of what to do with resource */
    int     oprestype;                                     /* resource type */
#   define  OPFADD   0x01                                   /* add resource */
#   define  OPFDEL   0x02                                /* delete resource */
#   define  OPFDONE  0x04                  /* this entry has been processed */
#   define  OPFADDDIR 0x08                       /* add an entire directory */
};

/* resource types */
#define RESTYPE_DFLT  0x00     /* default - infer type from filename suffix */
#define RESTYPE_XFCN  0x01
#define RESTYPE_HTML  0x02


/* copy a block of bytes from the input file to the output file */
char copybuf[16 * 1024];
static void copybytes(osfildef *fpin, osfildef *fpout, ulong siz)
{
    uint  cursiz;

    /* copy bytes until we run out */
    while (siz != 0)
    {
        /* we can copy up to one full buffer at a time */
        cursiz = (siz > sizeof(copybuf) ? sizeof(copybuf) : siz);

        /* deduct the amount we're copying from the total */
        siz -= cursiz;

        /* read from input, copy to output */
        if (osfrb(fpin, copybuf, cursiz)
            || osfwb(fpout, copybuf, cursiz))
        {
            /* error - close files and display an error */
            osfcls(fpin);
            osfcls(fpout);
            errexit("error copying resource", 1);
        }
    }
}

/* copy a resource from the input file to the output file */
static void copyres(osfildef *fpin, osfildef *fpout, ulong siz,
                    uint endpos_ofs)
{
    ulong startpos;
    ulong endpos;
    uchar buf[4];

    /* note the starting position */
    startpos = osfpos(fpout);

    /* copy the bytes of the resource */
    copybytes(fpin, fpout, siz);

    /* note the ending position */
    endpos = osfpos(fpout);

    /* write the ending position at the appropriate point */
    osfseek(fpout, (ulong)(startpos + endpos_ofs), OSFSK_SET);
    oswp4(buf, endpos);
    if (osfwb(fpout, buf, 4)) errexit("error writing resource", 1);

    /* seek back to the end of the output file */
    osfseek(fpout, endpos, OSFSK_SET);
}

/* process an operation */
static void procop(osfildef *fpout, opdef *op, ulong *first_xfcn)
{
    osfildef *fpin;
    char      buf[128];
    uint      fsiz;
    ulong     sizpos;
    ulong     endpos;

    /* remember location of first resource if necessary */
    if (fpout && *first_xfcn == 0)
        *first_xfcn = osfpos(fpout);

    fpin = osfoprb(op->opfile, OSFTGAME);
    if (fpin == 0)
    {
        rscptf("%s: ", op->opfile);
        errexit("unable to open file", 1);
    }

    /* get file size */
    osfseek(fpin, 0L, OSFSK_END);
    fsiz = (uint)osfpos(fpin);
    osfseek(fpin, 0L, OSFSK_SET);

    /* set up the resource type part of the header */
    switch(op->oprestype)
    {
    case RESTYPE_XFCN:
        buf[0] = 4;
        memcpy(buf + 1, "XFCN", 4);
        break;

    case RESTYPE_HTML:
        buf[0] = 4;
        memcpy(buf + 1, "HTML", 3);
        break;
    }
    
    /* set up the rest of the header */
    if (osfwb(fpout, buf, buf[0] + 1)) errexit("error writing resource", 1);
    sizpos = osfpos(fpout);               /* remember where size field goes */
    oswp4(buf, 0);
    if (osfwb(fpout, buf, 4)) errexit("error writing resource", 1);
    
    /* set up the header */
    oswp2(buf, fsiz);
    buf[2] = strlen(op->opres);
    strcpy(buf + 3, op->opres);
    if (osfwb(fpout, buf, (uint)(buf[2] + 3)))
        errexit("error writing resource", 1);
    
    /* copy the resource to the output */
    copybytes(fpin, fpout, fsiz);

    /* write end position in the resource header */
    endpos = osfpos(fpout);
    oswp4(buf, endpos);
    osfseek(fpout, sizpos, OSFSK_SET);
    if (osfwb(fpout, buf, 4)) errexit("error writing resource", 1);

    /* seek back to the end of the resource in the output file */
    osfseek(fpout, endpos, OSFSK_SET);

    /* done with the input file */
    osfcls(fpin);
}

/* display information on an operation */
static void show_op(const char *desc, const char *resname, size_t resnamelen,
                    int restype)
{
    const char *restypename;

    /* figure the type name */
    switch(restype)
    {
    case RESTYPE_XFCN:
        restypename = "XFCN";
        break;

    case RESTYPE_HTML:
        restypename = "HTML";
        break;

    default:
        restypename = "????";
        break;
    }

    /* display the message */
    rscptf(". %s %.*s (%s)\n", desc, resnamelen, resname, restypename);
}

/* display a list item */
static void show_list_item(int *showed_heading, char *type_name,
                           ulong res_size, char *res_name, size_t name_len)
{
    /* if we haven't displayed a header yet, do so now */
    if (!*showed_heading)
    {
        /* show the header */
        rscptf("Type   Size       Name\n"
               "----------------------------------------------------------\n");
        
        /* note that we've displayed the header now */
        *showed_heading = TRUE;
    }

    /* show this item */
    rscptf("%.4s   %8lu   %.*s\n", type_name, res_size,
           (int)name_len, res_name);
}

/* HTMLRES index entry */
struct idx_t
{
    /* source of the resource */
    opdef  *src_op;
    struct idx_t *src_idx;

    /* information on the resource in the file */
    ulong   ofs;
    ulong   siz;
    ushort  namlen;
    struct idx_t *nxt;
    char    nam[1];
};

/*
 *   Allocate an index entry 
 */
static struct idx_t *alloc_idx_entry(ulong ofs, ulong siz,
                                     char *nam, ushort namlen,
                                     opdef *src_op, struct idx_t *src_idx)
{
    struct idx_t *entry;

    /* allocate a new entry */
    entry = (struct idx_t *)malloc(sizeof(struct idx_t) + namlen - 1);

    /* fill it in */
    entry->src_op = src_op;
    entry->src_idx = src_idx;
    entry->ofs = ofs;
    entry->siz = siz;
    entry->namlen = namlen;
    memcpy(entry->nam, nam, namlen);

    /* return the new entry */
    return entry;
}

/*
 *   Add an item to an index list 
 */
static void add_idx_entry(struct idx_t **list, ulong *out_res_cnt,
                          ulong *out_total_name_len, ulong ofs, ulong siz,
                          char *nam, ushort namlen,
                          opdef *src_op, struct idx_t *src_idx)
{
    struct idx_t *entry;

    /* count its statistics */
    ++(*out_res_cnt);
    *out_total_name_len += namlen;

    /* allocate a new entry */
    entry = alloc_idx_entry(ofs, siz, nam, namlen, src_op, src_idx);
    
    /* link it into the list */
    entry->nxt = *list;
    *list = entry;
}

/*
 *   Process an HTML resource list.  If 'old_htmlres' is true, it
 *   indicates that the input file is pointing to an old resource map;
 *   otherwise, we need to construct a brand new one.  
 */
static opdef *prochtmlres(osfildef *fp, osfildef *fpout, opdef *oplist,
                          int *copyrsc, int *showed_heading, int old_htmlres)
{
    opdef *op;
    opdef *add_list = 0;
    opdef *prev_op;
    opdef *next_op;
    int    found;
    char   buf[512];
    ulong  in_entry_cnt;
    ulong  in_table_siz;
    ulong  out_hdr_siz;
    ulong  out_hdr_pos;
    ulong  i;
    ulong  out_res_cnt;
    ulong  out_total_name_len;
    ulong  rem;
    struct idx_t **in_list = 0, *out_list = 0, **p, *cur;
    ulong  in_res_base, out_res_base;
    ulong  out_endpos;

    /*
     *   Scan the oplist for an HTML resource.  If there aren't any, we
     *   don't need to modify the HTMLRES list, so tell the caller to copy
     *   this resource unchanged.  
     */
    for (op = oplist, found = FALSE ; op != 0 ; op = op->opnxt)
    {
        /* if this is an HTML resource, note it and stop looking */
        if (op->oprestype == RESTYPE_HTML)
        {
            found = TRUE;
            break;
        }
    }

    /* 
     *   If we didn't find any operations on this resource, and we're not
     *   simply listing resources or we don't have an old resource to
     *   list, tell the caller to copy it unchanged.  
     */
    if (!found && (fpout != 0 || !old_htmlres))
    {
        *copyrsc = TRUE;
        return oplist;
    }

    /* we'll be handling the resource - tell the caller not to copy it */
    *copyrsc = FALSE;

    /* if there's an old HTMLRES resource, read it */
    if (old_htmlres)
    {
        /* read the index entry count and size */
        if (osfrb(fp, buf, 8))
            listexit(fp, "unable to read HTMLRES header");
        in_entry_cnt = osrp4(buf);
        in_table_siz = osrp4(buf + 4);

        /* allocate space for pointers to all of the entries */
        if (in_entry_cnt != 0)
        {
            in_list = (struct idx_t **)
                      malloc(in_entry_cnt * sizeof(struct idx_t *));
            if (in_list == 0)
                listexit(fp, "unable to allocate space for HTMLRES entries");
        }

        /* read the index table entries */
        for (i = 0, p = in_list ; i < in_entry_cnt ; ++i, ++p)
        {
            ushort name_siz;
            ulong  res_siz;
            ulong  res_ofs;
            
            /* read the entry information */
            if (osfrb(fp, buf, 10))
                listexit(fp,
                         "unable to read HTMLRES index table entry (prefix)");
            
            /* get the resource size */
            res_ofs = osrp4(buf);
            res_siz = osrp4(buf + 4);
            
            /* read the name */
            name_siz = osrp2(buf + 8);
            if (name_siz > sizeof(buf))
                listexit(fp, "name too large in HTMLRES index table entry");
            if (osfrb(fp, buf, name_siz))
                listexit(fp,
                         "unable to read HTMLRES index table entry (name)");
            
            /* build this entry */
            *p = alloc_idx_entry(res_ofs, res_siz, buf, name_siz, 0, 0);
        }

        /* if we don't have an output file, list the HTMLRES contents */
        if (fpout == 0)
        {
            /* display all of the entries */
            for (i = 0, p = in_list ; i < in_entry_cnt ; ++i, ++p)
                show_list_item(showed_heading, "HTML",
                               (*p)->siz, (*p)->nam, (*p)->namlen);
            
            /* there's no more processing to do */
            goto done;
        }

        /*
         *   The resources start at the end of the index table - note the
         *   location of the end of the input table, since it's the base
         *   address relative to which the resource offsets are stated.  
         */
        in_res_base = osfpos(fp);

        /*
         *   Go through the resource table in the input file.  Find each
         *   one in the op list.  If it's not in the op list, we'll copy
         *   it to the output file.  
         */
        for (i = 0, p = in_list ; i < in_entry_cnt ; ++i, ++p)
        {
            int remove_res = FALSE;
            int add_res = FALSE;
            
            /* see if we can find this entry in the op list */
            for (prev_op = 0, op = oplist ; op != 0 ;
                 prev_op = op, op = op->opnxt)
            {
                /* if this one matches, note it */
                if (op->oprestype == RESTYPE_HTML
                    && strlen(op->opres) == (*p)->namlen
                    && !memicmp(op->opres, (*p)->nam, (*p)->namlen))
                {
                    /* 
                     *   if we're adding this resource (not replacing it),
                     *   warn that it's already in the file, and ignore
                     *   this op; if we're removing it or replacing it,
                     *   simply delete this entry from the input list so
                     *   it doesn't get copied to the output.  
                     */
                    if (!(op->opflag & OPFDEL))
                    {
                        /* warn that the old one will stay */
                        rscptf("warning: HTML resource \"%s\" already "
                               "present\n"
                               "  -- old version will be kept (use -replace "
                               "to replace it)\n", op->opres);
                        
                        /* remove it from the processing list */
                        remove_res = TRUE;
                    }
                    else
                    {
                        /* we are deleting it; see if we're also adding it */
                        if (op->opflag & OPFADD)
                        {
                            /* 
                             *   we're replacing this resource - take this
                             *   op out of the main list and put it into
                             *   the add list 
                             */
                            remove_res = TRUE;
                            add_res = TRUE;

                            /* note the addition */
                            show_op("replacing", op->opres, strlen(op->opres),
                                    op->oprestype);
                        }
                        else
                        {
                            /* note the deletion */
                            show_op("deleting", op->opres, strlen(op->opres),
                                    op->oprestype);

                            /* just remove it */
                            remove_res = TRUE;
                        }

                        /* get rid of this item from the input list */
                        free(*p);
                        *p = 0;
                    }
                    
                    /* no need to look further in the operations list */
                    break;
                }
            }
            
            /*
             *   If desired, remove this resource from the main list, and
             *   add it into the list of resources to add.  
             */
            if (remove_res)
            {
                /* unlink it from the main list */
                if (prev_op == 0)
                    oplist = op->opnxt;
                else
                    prev_op->opnxt = op->opnxt;
                
                /* if desired, add it to the additions list */
                if (add_res)
                {
                    /* we're adding it - put it in the additions list */
                    op->opnxt = add_list;
                    add_list = op;
                }
                else
                {
                    /* this item has been processed - delete it */
                    free(op);
                }
            }
        }
    }
    else
    {
        /* there are no entries in the input file */
        in_entry_cnt = 0;
        in_table_siz = 0;
    }

    /*
     *   Move all of the HTML resources marked as additions in the main
     *   operations list into the additions list. 
     */
    for (prev_op = 0, op = oplist ; op != 0 ; op = next_op)
    {
        /* note the next op, in case we move this one to the other list */
        next_op = op->opnxt;
        
        /* 
         *   if it's an HTML resource to be added, move it to the
         *   additions list 
         */
        if (op->oprestype == RESTYPE_HTML && (op->opflag & OPFADD) != 0)
        {
            /* show what we're doing */
            show_op("adding", op->opres, strlen(op->opres), op->oprestype);
            
            /* unlink it from the main list */
            if (prev_op == 0)
                oplist = op->opnxt;
            else
                prev_op->opnxt = op->opnxt;

            /* add it to the additions list */
            op->opnxt = add_list;
            add_list = op;

            /* 
             *   note that we don't want to advance the 'prev_op' pointer,
             *   since we just removed this item - the previous item is
             *   still the same as it was on the last iteration 
             */
        }
        else
        {
            /* 
             *   we're leaving this op in the original list - it's now the
             *   previous op in the main list 
             */
            prev_op = op;
        }
    }

    /*
     *   Figure out what we'll be putting in the HTMLRES list: we'll add
     *   each surviving entry from the input file, plus all of the items
     *   in the add list, plus all of the HTML items in the main list that
     *   are marked for addition.  
     */
    out_res_cnt = 0;
    out_total_name_len = 0;

    /* count input file entries that we're going to copy */
    for (i = 0, p = in_list ; i < in_entry_cnt ; ++i, ++p)
    {
        if (*p != 0)
            add_idx_entry(&out_list, &out_res_cnt, &out_total_name_len,
                          0, 0, (*p)->nam, (*p)->namlen, 0, *p);
    }

    /* 
     *   Count items in the additions list.  Note that every HTML resource
     *   marked for addition is in the additions list, since we moved all
     *   such resources out of the main list and into the additions list
     *   earlier.  
     */
    for (op = add_list ; op != 0 ; op = op->opnxt)
        add_idx_entry(&out_list, &out_res_cnt, &out_total_name_len,
                      0, 0, op->opres, (ushort)strlen(op->opres), op, 0);

    /* write the resource header */
    if (osfwb(fpout, "\007HTMLRES\0\0\0\0", 12))
        listexit(fp, "unable to write HTMLRES type header");
    out_hdr_pos = osfpos(fpout);

    /*
     *   Reserve space in the output file for the index table.  We need
     *   eight bytes for the index table prefix, then ten bytes per entry
     *   plus the name sizes. 
     */
    out_hdr_siz = 8 + (10 * out_res_cnt) + out_total_name_len;

    /* write the index table prefix */
    oswp4(buf, out_res_cnt);
    oswp4(buf + 4, out_hdr_siz);
    if (osfwb(fpout, buf, 8))
        listexit(fp, "unable to write HTMLRES prefix");

    /* 
     *   Reserve space for the headers.  Don't actually write them yet,
     *   since we don't know the actual locations and sizes of the
     *   entries; for now, simply reserve the space, so that we can come
     *   back here later and write the actual headers.  Note that we
     *   deduct the eight bytes we've already written from the amount of
     *   filler to put in.  
     */
    for (rem = out_hdr_siz - 8 ; rem != 0 ; )
    {
        ulong amt;
        
        /* write out a buffer full */
        amt = (rem > sizeof(buf) ? sizeof(buf) : rem);
        if (osfwb(fpout, buf, amt))
            listexit(fp, "unable to write HTMLRES header");

        /* deduct the amount we wrote from the remainder */
        rem -= amt;
    }

    /* 
     *   note the current position in the output file - this is the base
     *   address of the resources 
     */
    out_res_base = osfpos(fpout);

    /*
     *   Write the resources.  
     */
    for (cur = out_list ; cur != 0 ; cur = cur->nxt)
    {
        /* 
         *   note the current file position as an offset from the resource
         *   base in the output file - this is the offset that we need to
         *   store in the index entry for this object 
         */
        cur->ofs = osfpos(fpout) - out_res_base;
        
        /* 
         *   Copy the resource to the output.  If it comes from the input
         *   file, copy from there, otherwise go out and find the external
         *   file and copy its contents. 
         */
        if (cur->src_op != 0)
        {
            osfildef *fpext;
            ulong fsiz;
            
            /* it comes from an external file - open the file */
            fpext = osfoprb(cur->src_op->opfile, OSFTGAME);
            if (fpext == 0)
            {
                rscptf("%s: ", cur->src_op->opfile);
                errexit("unable to open file", 1);
            }

            /* figure the size of the file */
            osfseek(fpext, 0L, OSFSK_END);
            fsiz = osfpos(fpext);
            osfseek(fpext, 0L, OSFSK_SET);

            /* copy the contents of the external file to the output */
            copybytes(fpext, fpout, fsiz);

            /* the size is the same as the external file's size */
            cur->siz = fsiz;

            /* done with the file */
            osfcls(fpext);
        }
        else
        {
            /* 
             *   it comes from the input resource file - seek to the start
             *   of the resource in the input file, and copy the data to
             *   the output file 
             */
            osfseek(fp, in_res_base + cur->src_idx->ofs, OSFSK_SET);
            copybytes(fp, fpout, cur->src_idx->siz);

            /* the size is the same as in the input file */
            cur->siz = cur->src_idx->siz;
        }
    }

    /* note the current output position - this is the end of the resource */
    out_endpos = osfpos(fpout);

    /*
     *   Now that we've written all of the resources and know their actual
     *   layout in the file, we can go back and write the index table. 
     */
    osfseek(fpout, out_hdr_pos + 8, OSFSK_SET);
    for (cur = out_list ; cur != 0 ; cur = cur->nxt)
    {
        /* build this object's index table entry */
        oswp4(buf, cur->ofs);
        oswp4(buf + 4, cur->siz);
        oswp2(buf + 8, cur->namlen);

        /* write the entry */
        if (osfwb(fpout, buf, 10)
            || osfwb(fpout, cur->nam, cur->namlen))
            listexit(fp, "unable to write HTML index table entry");
    }

    /*
     *   We're done building the resource; now all we need to do is go
     *   back and write the ending position of the resource in the
     *   resource header.  
     */
    osfseek(fpout, out_hdr_pos - 4, OSFSK_SET);
    oswp4(buf, out_endpos);
    if (osfwb(fpout, buf, 4))
        errexit("error writing resource", 1);

    /* seek back to the end of the resource in the output file */
    osfseek(fpout, out_endpos, OSFSK_SET);

done:
    /* if we have an input list, free it */
    if (in_list != 0)
    {
        /* delete all of the entries in the input table */
        for (i = 0, p = in_list ; i < in_entry_cnt ; ++i, ++p)
        {
            /* delete this entry if we haven't already done so */
            if (*p != 0)
                free(*p);
        }

        /* delete the input pointer list itself */
        free(in_list);
    }

    /* 
     *   delete everything in the additions list, since we're done with
     *   them now 
     */
    for (op = add_list ; op != 0 ; op = next_op)
    {
        /* note the next entry in the list */
        next_op = op->opnxt;

        /* delete this entry */
        free(op);
    }

    /* return the op list in its current form */
    return oplist;
}


/* process changes to resource file (or just list it) */
static opdef *rscproc(osfildef *fp, osfildef *fpout, opdef *oplist)
{
    char   buf[128];
    ulong  siz;
    char   datebuf[27];
    ulong  endpos;
    uchar  nambuf[40];
    uint   rsiz;
    opdef *op;
    int    copyrsc;
    ulong  startpos;
    uint   endpos_ofs;
    ulong  first_xfcn = 0;
    ulong  extcnt_pos = 0;
    int    found_user_rsc = FALSE;
    int    showed_heading = FALSE;
    int    found_htmlres = FALSE;
    char  *file_type;

    /* 
     *   if we're reading an existing file, check it header; otherwise,
     *   write out a brand new file header 
     */
    if (fp != 0)
    {
        /* 
         *   the input file exists -- check file and version headers, and
         *   get flags and timestamp 
         */
        if (osfrb(fp, buf, (int)(sizeof(FIOFILHDR) + sizeof(FIOVSNHDR) + 2)))
            listexit(fp, "error reading file header");

        /* check the file type */
        if (memcmp(buf, FIOFILHDR, sizeof(FIOFILHDR)) == 0)
            file_type = "game";
        else if (memcmp(buf, FIOFILHDRRSC, sizeof(FIOFILHDRRSC)) == 0)
            file_type = "resource";
        else
            listexit(fp, "invalid resource file header");

        /* check the version header */
        if (memcmp(buf + sizeof(FIOFILHDR), FIOVSNHDR, sizeof(FIOVSNHDR))
            && memcmp(buf + sizeof(FIOFILHDR), FIOVSNHDR2, sizeof(FIOVSNHDR2))
            && memcmp(buf + sizeof(FIOFILHDR), FIOVSNHDR3, sizeof(FIOVSNHDR3)))
            listexit(fp, "incompatible resource file version");

        /* get the timestamp */
        if (osfrb(fp, datebuf, (size_t)26))
            listexit(fp, "error reading file");
        datebuf[26] = '\0';
    }
    else
    {
        struct tm  *tblock;
        time_t      timer;

        /* construct a new file header */
        memcpy(buf, FIOFILHDRRSC, sizeof(FIOFILHDRRSC));
        memcpy(buf + sizeof(FIOFILHDR), FIOVSNHDR, sizeof(FIOVSNHDR));

        /* clear the flags */
        oswp2(buf + sizeof(FIOFILHDR) + sizeof(FIOVSNHDR), 0);

        /* construct the timestamp */
        timer = time(0);
        tblock = localtime(&timer);
        strcpy(datebuf, asctime(tblock));
    }
        
    if (fpout)
    {
        if (osfwb(fpout, buf,
                  (int)(sizeof(FIOFILHDR) + sizeof(FIOVSNHDR) + 2))
            || osfwb(fpout, datebuf, (size_t)26))
            listexit(fp, "error writing header");
    }

    /* if listing, show file creation timestamp */
    if (fpout == 0)
        rscptf("\n"
               "File type:     TADS %s file\n"
               "Date compiled: %s\n", file_type, datebuf);

    /*
     *   Process the input file, if there is one 
     */
    for ( ; fp != 0 ; )
    {
        /* assume this resource will be copied to the output */
        copyrsc = TRUE;

        startpos = osfpos(fp);
        if (osfrb(fp, buf, 1)
            || osfrb(fp, buf + 1, (int)(buf[0] + 4)))
            listexit(fp, "error reading file");

        memcpy(nambuf, buf + 1, (size_t)buf[0]);
        nambuf[buf[0]] = '\0';
        
        endpos_ofs = 1 + buf[0];
        endpos = osrp4(buf + endpos_ofs);
        siz = endpos - startpos;
        
        /* see what kind of resource we have, and do the right thing */
        if (!strcmp((char *)nambuf, "$EOF"))
        {
            /* end of file marker - quit here */
            break;
        }
        else if (!strcmp((char *)nambuf, "HTMLRES"))
        {
            /* if we've already found an HTMLRES list, it's an error */
            if (found_htmlres)
            {
                rscptf("error: multiple HTMLRES maps found in file\n"
                       " -- redundant entries have been deleted.\n");
                copyrsc = FALSE;
            }
            else
            {
                /* go process it */
                oplist = prochtmlres(fp, fpout, oplist, &copyrsc,
                                     &showed_heading, TRUE);
            }

            /* note that we've found a resource */
            found_user_rsc = TRUE;
            found_htmlres = TRUE;
        }
        else if (!strcmp((char *)nambuf, "EXTCNT"))
        {
            /* place to write start of XFCN's? */
            if (siz >= 17 && fpout)
                extcnt_pos = osfpos(fpout);
        }
        else if (!strcmp((char *)nambuf, "XFCN"))
        {
            if (osfrb(fp, buf, 3) || osfrb(fp, buf + 3, (int)buf[2]))
                listexit(fp, "error reading file");
            rsiz = osrp2(buf);
            buf[3 + buf[2]] = '\0';

            if (fpout)
            {
                /* see if this resource is in the list */
                for (op = oplist ; op ; op = op->opnxt)
                {
                    if (!(op->opflag & OPFDONE)
                        && op->oprestype == RESTYPE_XFCN
                        && !stricmp((char *)buf + 3, op->opres))
                    {
                        /* note that this resource has now been processed */
                        op->opflag |= OPFDONE;

                        /* 
                         *   if it's already here, and we're not deleting
                         *   it, warn that the old one will stay around 
                         */
                        if (!(op->opflag & OPFDEL))
                        {
                            /* warn that we're going to ignore it */
                            rscptf("warning: XFCN resource \"%s\" already in "
                                   "file\n -- the old resource will be kept "
                                   "(use -replace to replace it)\n",
                                   op->opres);

                            /* don't add it */
                            op->opflag &= ~OPFADD;
                        }
                        else
                        {
                            /* 
                             *   we're deleting this resource; if adding
                             *   it back in (i.e., replacing it), process
                             *   the add operation now 
                             */
                            if (op->opflag & OPFADD)
                            {
                                /* show what we're doing */
                                show_op("replacing", op->opres,
                                        strlen(op->opres), op->oprestype);

                                /* 
                                 *   add the external file, replacing the
                                 *   one in the input file 
                                 */
                                procop(fpout, op, &first_xfcn);
                            }
                            else
                            {
                                /* note that we're deleting it */
                                show_op("deleting", op->opres,
                                        strlen(op->opres), op->oprestype);
                            }

                            /* don't copy the one out of the file */
                            copyrsc = FALSE;
                        }
                        break;
                    }
                }
            }
            else
            {
                /* no output file - just list the resource */
                show_list_item(&showed_heading, "XFCN",
                               (ulong)rsiz, buf + 3, (size_t)buf[2]);
            }

            /* note that we've found a user resource */
            found_user_rsc = TRUE;
        }
        
        if (fpout != 0 && copyrsc)
        {
            osfseek(fp, startpos, OSFSK_SET);
            copyres(fp, fpout, siz, endpos_ofs);
        }
        
        /* skip to the next resource */
        osfseek(fp, endpos, OSFSK_SET);
    }
    
    /* add the HTML resources if we haven't already */
    if (!found_htmlres)
        oplist = prochtmlres(fp, fpout, oplist, &copyrsc,
                             &showed_heading, FALSE);

    /* now go through what's left, and add new non-HTML resources */
    if (fpout != 0)
    {
        for (op = oplist ; op != 0 ; op = op->opnxt)
        {
            if (!(op->opflag & OPFDONE) && (op->opflag & OPFADD)
                && op->oprestype != RESTYPE_HTML)
            {
                /* show what we're doing */
                show_op("adding", op->opres,
                        strlen(op->opres), op->oprestype);

                /* add the file */
                procop(fpout, op, &first_xfcn);

                /* mark the operation as completed */
                op->opflag |= OPFDONE;
            }
        }
    }

    /* if just listing, and we didn't find anything, tell the user so */
    if (fpout == 0 && !found_user_rsc)
        rscptf("No user resources found.\n");

    /* done reading the file - finish it up */
    if (fpout != 0)
    {
        /* write EOF resource */
        if (osfwb(fpout, "\004$EOF\0\0\0\0", 9))
            errexit("error writing resource", 1);

        /* write first_xfcn value to EXTCNT resource */
        if (extcnt_pos)
        {
            osfseek(fpout, extcnt_pos + 13, OSFSK_SET);
            oswp4(buf, first_xfcn);
            osfwb(fpout, buf, 4);
        }
    }

    /* 
     *   return the final oplist (it may have been changed during
     *   processing) 
     */
    return oplist;
}

/*
 *   Operation parsing context.  Certain options affect subsquent
 *   operations; we use this structure to keep track of the current
 *   settings from past arguments.  
 */
typedef struct
{
    /*
     *   Type to use for resources.  If this is RESTYPE_DFLT, it means
     *   that we need to infer the type from the filename suffix.  
     */
    int restype;

    /* 
     *   operations to perform - a combination of OPFADD and OPFDEL 
     */
    int flag;

    /* 
     *   context flag: if true, it means that we just parsed a -type
     *   option, so the next argument is the type name 
     */
    int doing_type;
} opctxdef;

/*
 *   Get the resource type of a file.  If an explicit resource type is
 *   specified in the resource type argument, we'll use that; otherwise
 *   (i.e., the resource type argument is RESTYPE_DFLT), we'll look at the
 *   filename suffix to determine the type.  
 */
static int get_file_restype(int restype, char *fname)
{
    /* see if we have a specified type */
    if (restype == RESTYPE_DFLT)
    {
        char *p;
        char *lastdot;

        /* 
         *   No specified type - infer type from the filename.  If the
         *   filename ends in .bin, .com, or has no '.', we'll assume it's
         *   an XFCN; otherwise, it's an HTML resource.  
         */
        for (p = fname, lastdot = 0 ; *p ; ++p)
        {
            /* if we're at a dot, note it, but keep looking */
            if (*p == '.')
                lastdot = p;
        }

        /* check our suffix */
        if (lastdot == 0
            || !stricmp(lastdot, ".bin") || !stricmp(lastdot, ".com"))
            return RESTYPE_XFCN;
        else
            return RESTYPE_HTML;
    }
    else
    {
        /* 
         *   an explicit resource type has been specified - use it without
         *   regard to the filename 
         */
        return restype;
    }
}


/*
 *   Process a directory, adding an operation for each file in the
 *   directory 
 */
static opdef *addopdir(opdef *cur, char *nam, opctxdef *opctx)
{
    opdef *newop;
    char dir_prefix[OSFNMAX];
    osdirhdl_t dirhdl;

    /* HTML-ize the directory prefix */
    os_cvt_dir_url(dir_prefix, sizeof(dir_prefix) - 1, nam);

    /* add a '/' if the name isn't empty */
    if (dir_prefix[0] != '\0')
        strcat(dir_prefix, "/");

    /* search the directory */
    if (os_open_dir(nam, &dirhdl))
    {
        char fname[OSFNMAX];
        while (os_read_dir(dirhdl, fname, sizeof(fname)))
        {
            char fullname[OSFNMAX];
            unsigned long fmode;
            unsigned long fattr;

            /* build the full name */
            os_build_full_path(fullname, sizeof(fullname), nam, fname);

            /* 
             *   If it's a file, process it; ignore subdirectories.  Also
             *   skip hidden and system files.  $$$ We could easily recurse
             *   here to process subdirectories, but it's unclear how we
             *   should name resources found in subdirectories, so we'll just
             *   ignore them entirely for now.
             */
            if (osfmode(fullname, TRUE, &fmode, &fattr)
                && (fmode & OSFMODE_DIR) == 0
                && (fattr & (OSFATTR_HIDDEN | OSFATTR_SYSTEM)) == 0)
            {
                /* build the full name of the resource, using the URL prefix */
                char fullurl[OSFNMAX];
                sprintf(fullurl, "%s%s", dir_prefix, fname);
                
                /* build a new node and link it into the list */
                newop = (opdef *)malloc(sizeof(opdef) + strlen(fullurl)
                                        + strlen(fullname) + 2);
                newop->opnxt = cur;
                newop->opflag = opctx->flag;
                newop->oprestype = get_file_restype(opctx->restype, fname);
                newop->opres = (char *)(newop + 1);
                strcpy(newop->opres, fullurl);
                newop->opfile = newop->opres + strlen(newop->opres) + 1;
                strcpy(newop->opfile, fullname);
                
                /* it's the new head of the list */
                cur = newop;
            }
        }

        /* done with the directory scan */
        os_close_dir(dirhdl);
    }

    /* done - return the most recent head node */
    return cur;
}


/* get a resource type given a resource name */
static int parse_res_type(char *p)
{
    /* see what we have */
    if (!stricmp(p, "xfcn"))
        return RESTYPE_XFCN;
    else if (!stricmp(p, "html"))
        return RESTYPE_HTML;
    else
    {
        rscptf("invalid resource type specified: %s", p);
        errexit("", 1);
        return 0;
    }
}

/*
 *   Add an operation from a command line argument 
 */
static opdef *addop(opdef *cur, char *nam, opctxdef *opctx)
{
    char  *p;
    opdef *newop;
    char   resname[OSFNMAX];
    unsigned long fmode;
    unsigned long fattr;

    /* see if we're parsing a -type argument */
    if (opctx->doing_type)
    {
        /* 
         *   parse the type name, and store it as the type for following
         *   resources
         */
        opctx->restype = parse_res_type(nam);
        
        /* we're done parsing the -type argument */
        opctx->doing_type = FALSE;

        /* 
         *   we're done parsing this argument - we haven't added any
         *   operations, so return old list head 
         */
        return cur;
    }

    /* see if we have an option */
    if (*nam == '-')
    {
        /* see what we have */
        if (!stricmp(nam, "-type"))
        {
            /* 
             *   note that we're doing a type, so we parse it on the next
             *   argument 
             */
            opctx->doing_type = TRUE;
        }
        else if (!stricmp(nam, "-replace"))
        {
            /* set current flags to replace */
            opctx->flag = OPFADD | OPFDEL;
        }
        else if (!stricmp(nam, "-delete"))
        {
            /* set current flags to delete */
            opctx->flag = OPFDEL;
        }
        else if (!stricmp(nam, "-add"))
        {
            /* set current flags to add */
            opctx->flag = OPFADD;
        }
        else
        {
            /* invalid argument */
            rscptf("invalid option: %s", nam);
            errexit("", 1);
        }

        /* 
         *   done parsing this option - we didn't add a new operation, so
         *   return the current list head 
         */
        return cur;
    }

    /* look for '=' */
    for (p = nam ; *p && *p != '=' ; ++p);
    if (*p == '=')
    {
        /* 
         *   We found an '=', so an explicit resource name follows - use
         *   the given string as the resource name rather than basing the
         *   resource name on the filename.  First, overwrite the '=' with
         *   a null byte so that the filename string is terminated.  
         */
        *p = '\0';

        /* skip the '=' (now the null byte, of course) */
        ++p;

        /* 
         *   skip any spaces after the '='; leave p pointing to the start
         *   of the resource name 
         */
        while (t_isspace(*p))
            ++p;
    }
    else
    {
        /* 
         *   A resource name wasn't specified - synthesize a resource name
         *   based on the filename by converting from the local file system
         *   name to a relative URL 
         */
        os_cvt_dir_url(resname, sizeof(resname), nam);

        /* point p to the synthesized resource name */
        p = resname;
    }
    
    /*
     *   If we're adding a directory, rather than returning a single op
     *   for the directory, expand the directory into ops for for all of
     *   the files in the directory. 
     */
    if (osfmode(nam, TRUE, &fmode, &fattr)
        && (fmode & OSFMODE_DIR) != 0)
        return addopdir(cur, nam, opctx);
    
    /* allocate space and set up new op */
    newop = (opdef *)malloc(sizeof(opdef) + strlen(p) + strlen(nam) + 2);

    newop->opnxt  = cur;
    newop->opflag = opctx->flag;
    newop->opres  = (char *)(newop + 1);
    newop->oprestype = get_file_restype(opctx->restype, nam);
    strcpy(newop->opres, p);
    newop->opfile = newop->opres + strlen(newop->opres) + 1;
    strcpy(newop->opfile, nam);

    return(newop);
}


int main(int argc, char **argv)
{
    int       curarg;
    osfildef *fpin;
    osfildef *fpout;
    char      tmpfile[OSFNMAX + 1];
    char      inbuf[OSFNMAX + 1];
    char     *p;
    char     *infile;
    char      buf[128];
    opdef    *oplist = (opdef *)0;
    opctxdef  opctx;
    int       do_create = FALSE;

    /* print main banner */
    rscptf("TADS Resource Manager version 2.2.4\n");
    rscptf("Copyright (c) 1992, 1999 by Michael J. Roberts.  ");
    rscptf("All Rights Reserved.\n");
    if (argc < 2) usage();

    /* set default parsing options */
    opctx.restype = RESTYPE_DFLT;
    opctx.flag = OPFADD | OPFDEL;
    opctx.doing_type = FALSE;

    /* scan file options (these come before the filename) */
    for (curarg = 1 ; curarg < argc ; ++curarg)
    {
        /* check if it's an option - if not, stop looking */
        if (argv[curarg][0] != '-')
            break;
        
        /* check the option */
        if (!stricmp(argv[curarg], "-create"))
        {
            /* note that we want to create the file */
            do_create = TRUE;
        }
        else
        {
            rscptf("unrecognized file option \"%s\"", argv[curarg]);
            errexit("", 1);
        }
    }
    
    /* get the file name */
    infile = argv[curarg++];
    strcpy(inbuf, infile);
    os_defext(inbuf, "gam");

    /* open the file for reading, unless we're creating a new file */
    if (do_create)
    {
        /* creating - we have no input file */
        fpin = 0;
    }
    else if ((fpin = osfoprb(inbuf, OSFTGAME)) == 0)
    {
        /* 
         *   not creating, so the file must already exist - it doesn't, so
         *   issue an error and quit 
         */
        errexit("unable to open resource file", 1);
    }

    /* 
     *   if no operations are desired, and we're not creating a new file,
     *   just list the existing file's contents and quit 
     */
    if (curarg == argc && fpin != 0)
    {
        rscproc(fpin, (osfildef *)0, 0);
        osfcls(fpin);
        os_term(OSEXSUCC);
    }

    /*
     *   Create an output file.  If we're creating a new file, create the
     *   file named on the command line; otherwise, create a temporary
     *   file that we'll write to while working and then rename to the
     *   original input filename after we've finished with the original
     *   input file.  
     */
    if (do_create)
    {
        /* create the new file */
        if ((fpout = osfopwb(inbuf, OSFTGAME)) == 0)
            errexit("unable to create file", 1);

        /* report the creation */
        rscptf("\nFile created.\n");
    }
    else
    {
        /* generate a temporary filename */
        strcpy(tmpfile, inbuf);
        for (p = tmpfile + strlen(tmpfile) ; p > tmpfile &&
                 *(p-1) != ':' && *(p-1) != '\\' && *(p-1) != '/' ; --p);
        strcpy(p, "$TADSRSC.TMP");

        /* open the temporary file */
        if ((fpout = osfopwb(tmpfile, OSFTGAME)) == 0)
            errexit("unable to create temporary file", 1);
    }

    /* see if we need to read a response file */
    if (curarg < argc && argv[curarg][0] == '@')
    {
        osfildef *argfp;
        int       l;
        char     *p;
        
        if (!(argfp = osfoprt(argv[curarg]+1, OSFTTEXT)))
            errexit("unable to open response file", 1);
        
        for (;;)
        {
            if (!osfgets(buf, sizeof(buf), argfp)) break;
            l = strlen(buf);
            if (l && buf[l-1] == '\n') buf[--l] = '\0';
            for (p = buf ; t_isspace(*p) ; ++p);
            if (!*p) continue;
            oplist = addop(oplist, p, &opctx);
        }
        osfcls(argfp);
    }
    else
    {
        for ( ; curarg < argc ; ++curarg)
            oplist = addop(oplist, argv[curarg], &opctx);
    }

    /* process the resources */
    oplist = rscproc(fpin, fpout, oplist);

    /* make sure they all got processed */
    for ( ; oplist != 0 ; oplist = oplist->opnxt)
    {
        if (!(oplist->opflag & OPFDONE))
            rscptf("warning: resource \"%s\" not found\n", oplist->opres);
    }
    
    /* close files */
    if (fpin != 0)
        osfcls(fpin);
    if (fpout != 0)
        osfcls(fpout);
    
    /* 
     *   if we didn't create a new file, remove the original input file
     *   and rename the temp file to the original file name 
     */
    if (!do_create)
    {
        /* remove the original input file */
        if (remove(inbuf))
            errexit("error deleting input file", 1);

        /* rename the temp file to the output file */
        if (rename(tmpfile, inbuf))
            errexit("error renaming temporary file", 1);
    }

    /* success */
    os_term(OSEXSUCC);
    return OSEXSUCC;
}

