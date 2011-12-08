#ifdef RCSID
static char RCSid[] =
"";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcgen.cpp - TADS 3 Compiler code generator support classes - file operations
Function
  This module provides the file operations for the tcgen classes.  These
  are separated out here because they're not needed in builds that don't read
  or write symbol or object files, such as debugger and eval()-equipped
  interpreter builds.
Notes

Modified
  05/09/99 MJRoberts  - Creation
*/


#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "os.h"
#include "tcglob.h"
#include "tcgen.h"
#include "vmerr.h"
#include "tcerrnum.h"
#include "tctok.h"
#include "tcprs.h"
#include "tcmain.h"
#include "vmfile.h"
#include "tctarg.h"


/* ------------------------------------------------------------------------ */
/*
 *   Data streams 
 */

/*
 *   Write the stream, its anchors, and its fixups to an object file.  
 */
void CTcDataStream::write_to_object_file(CVmFile *fp)
{
    ulong ofs;
    CTcStreamAnchor *anchor;
    long cnt;

    /* 
     *   First, write the data stream bytes.  Write the length prefix
     *   followed by the data.  Just blast the whole thing out in one huge
     *   byte stream, one page at a time.  
     */
    fp->write_uint4(ofs_);

    /* write the data one page at a time */
    for (ofs = 0 ; ofs < ofs_ ; )
    {
        size_t cur;

        /* 
         *   write out one whole page, or the balance of the current page if
         *   we have less than a whole page remaining 
         */
        cur = TCCS_PAGE_SIZE;
        if (ofs + cur > ofs_)
            cur = (size_t)(ofs_ - ofs);

        /* write out this chunk */
        fp->write_bytes(calc_addr(ofs), cur);

        /* move to the next page's offset */
        ofs += cur;
    }

    /* count the anchors */
    for (cnt = 0, anchor = first_anchor_ ; anchor != 0 ;
         anchor = anchor->nxt_, ++cnt) ;

    /* write the count */
    fp->write_uint4(cnt);

    /*
     *   Write all of the anchors, and all of their fixups.  (We have the
     *   code to write the anchor and fixup information in-line here for
     *   efficiency - there will normally be a large number of these tiny
     *   objects, so anything we can do to improve the speed of this loop
     *   will help quite a lot in the overall write performance.)  
     */
    for (anchor = first_anchor_ ; anchor != 0 ; anchor = anchor->nxt_)
    {
        char buf[6];

        /* write the stream offset */
        oswp4(buf, anchor->get_ofs());

        /* 
         *   If the anchor has an external fixup list, write the symbol's
         *   name to the file; otherwise write a zero length to indicate that
         *   we have an internal fixup list.  
         */
        if (anchor->get_fixup_owner_sym() == 0)
        {
            /* no external list - indicate with a zero symbol length */
            oswp2(buf+4, 0);
            fp->write_bytes(buf, 6);
        }
        else
        {
            /* external list - write the symbol length and name */
            oswp2(buf+4, anchor->get_fixup_owner_sym()->get_sym_len());
            fp->write_bytes(buf, 6);
            fp->write_bytes(anchor->get_fixup_owner_sym()->get_sym(),
                            anchor->get_fixup_owner_sym()->get_sym_len());
        }

        /* write the fixup list */
        CTcAbsFixup::
            write_fixup_list_to_object_file(fp, *anchor->fixup_list_head_);
    }
}

/*
 *   Read a stream from an object file 
 */
void CTcDataStream::load_object_file(CVmFile *fp,
                                     const textchar_t *fname)
{
    ulong stream_len;
    ulong rem;
    char buf[1024];
    ulong start_ofs;
    ulong anchor_cnt;

    /* read the length of the stream */
    stream_len = fp->read_uint4();

    /* remember my starting offset */
    start_ofs = get_ofs();

    /* read the stream bytes */
    for (rem = stream_len ; rem != 0 ; )
    {
        size_t cur;

        /* read up to a buffer-full, or however much is left */
        cur = sizeof(buf);
        if (cur > rem)
            cur = rem;

        /* read this chunk */
        fp->read_bytes(buf, cur);

        /* add this chunk to the stream */
        write(buf, cur);

        /* deduct the amount we've read from the amount remaining */
        rem -= cur;
    }

    /*
     *   Read the anchors.  For each anchor, we must fix up the anchor by
     *   adding the base address of the stream we just read - the original
     *   anchor offsets in the object file reflect a base stream offset of
     *   zero, but we could be loading the stream after a bunch of other data
     *   have already been loaded into the stream.
     *   
     *   First, read the number of anchors, then loop through the anchors and
     *   read each one.  
     */
    for (anchor_cnt = fp->read_uint4() ; anchor_cnt != 0 ;
         --anchor_cnt)
    {
        ulong anchor_ofs;
        size_t sym_len;
        CTcStreamAnchor *anchor;

        /* read this anchor */
        fp->read_bytes(buf, 6);

        /* get the offset, and adjust for the new stream base offset */
        anchor_ofs = t3rp4u(buf) + start_ofs;

        /* get the length of the owning symbol's name, if any */
        sym_len = osrp2(buf+4);

        /* if there's a symbol name, read it */
        if (sym_len != 0)
        {
            CTcSymbol *owner_sym;

            /* read the symbol name */
            fp->read_bytes(buf, sym_len);

            /* look it up in the global symbol table */
            owner_sym = G_prs->get_global_symtab()->find(buf, sym_len);
            if (owner_sym == 0)
            {
                /* 
                 *   the owner symbol doesn't exist - this is an internal
                 *   inconsistency in the object file, because the anchor
                 *   symbol must always be defined in the same file and hence
                 *   should have been loaded already; complain and go on 
                 */
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_OBJFILE_INT_SYM_MISSING,
                                    (int)sym_len, buf, fname);

                /* we can't create the anchor */
                anchor = 0;
            }
            else
            {
                /* create the anchor based on the symbol */
                anchor = add_anchor(owner_sym,
                                    owner_sym->get_fixup_list_anchor(),
                                    anchor_ofs);

                /* set the anchor in the symbol */
                owner_sym->set_anchor(anchor);
            }
        }
        else
        {
            /* create the anchor with no external references */
            anchor = add_anchor(0, 0, anchor_ofs);
        }

        /* load the fixup list */
        CTcAbsFixup::
            load_fixup_list_from_object_file(fp, fname,
                                             anchor->fixup_list_head_);
    }
}




/* ------------------------------------------------------------------------ */
/*
 *   Absolute address fixups 
 */

/*
 *   Write a fixup list to an object file 
 */
void CTcAbsFixup::
    write_fixup_list_to_object_file(CVmFile *fp, CTcAbsFixup *list_head)
{
    int cnt;
    CTcAbsFixup *fixup;

    /* count the fixups */
    for (cnt = 0, fixup = list_head ; fixup != 0 ;
         fixup = fixup->nxt, ++cnt) ;

    /* write the count */
    fp->write_uint2(cnt);

        /* write the fixup list */
    for (fixup = list_head ; fixup != 0 ; fixup = fixup->nxt)
    {
        char buf[5];

        /* write the data stream ID */
        buf[0] = fixup->ds->get_stream_id();

        /* write the data stream offset of the reference */
        oswp4(buf+1, fixup->ofs);

        /* write the data */
        fp->write_bytes(buf, 5);
    }
}

/*
 *   Read a fixup list from an object file 
 */
void CTcAbsFixup::
    load_fixup_list_from_object_file(CVmFile *fp,
                                     const textchar_t *obj_fname,
                                     CTcAbsFixup **list_head)
{
    uint fixup_cnt;

    /* read the fixups */
    for (fixup_cnt = fp->read_uint2() ; fixup_cnt != 0 ; --fixup_cnt)
    {
        char buf[5];
        char stream_id;
        ulong fixup_ofs;
        CTcDataStream *stream;

        /* read the fixup data */
        fp->read_bytes(buf, 5);
        stream_id = buf[0];
        fixup_ofs = t3rp4u(buf+1);

        /* find the stream for the ID */
        stream = CTcDataStream::get_stream_from_id(stream_id, obj_fname);

        /* if the stream is invalid, ignore this record */
        if (stream == 0)
            continue;

        /* 
         *   the fixup offset is relative to the starting offset of the
         *   stream for the current object file - adjust it accordingly 
         */
        fixup_ofs += stream->get_object_file_start_ofs();

        /* create the fixup and add it to the list */
        add_abs_fixup(list_head, stream, fixup_ofs);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Object/property ID fixups 
 */

/*
 *   Write a fixup list to an object file 
 */
void CTcIdFixup::write_to_object_file(CVmFile *fp, CTcIdFixup *head)
{
    ulong cnt;
    CTcIdFixup *cur;

    /* count the elements in the list */
    for (cur = head, cnt = 0 ; cur != 0 ; cur = cur->nxt_, ++cnt) ;

    /* write the count */
    fp->write_uint4(cnt);

    /* write the fixups */
    for (cur = head ; cur != 0 ; cur = cur->nxt_)
    {
        char buf[9];

        /* prepare a buffer with the data in portable format */
        buf[0] = cur->ds_->get_stream_id();
        oswp4(buf+1, cur->ofs_);
        oswp4(buf+5, cur->id_);

        /* write the data */
        fp->write_bytes(buf, 9);
    }
}

/*
 *   Read an object ID fixup list from an object file 
 */
void CTcIdFixup::load_object_file(CVmFile *fp,
                                  const void *xlat,
                                  ulong xlat_cnt,
                                  tcgen_xlat_type xlat_type,
                                  size_t stream_element_size,
                                  const textchar_t *fname,
                                  CTcIdFixup **fixup_list_head)
{
    ulong cnt;

    /* read the count, then read the fixups */
    for (cnt = fp->read_uint4() ; cnt != 0 ; --cnt)
    {
        char buf[9];
        ulong ofs;
        ulong old_id;
        ulong new_id;
        CTcDataStream *stream;

        /* read the next fixup */
        fp->read_bytes(buf, 9);
        stream = CTcDataStream::get_stream_from_id(buf[0], fname);
        ofs = t3rp4u(buf+1);
        old_id = t3rp4u(buf+5);

        /* if the stream was invalid, ignore this record */
        if (stream == 0)
            continue;

        /* adjust the offset for the stream's start in this object file */
        ofs += stream->get_object_file_start_ofs();

        /* apply the fixup if a translation table was provided */
        if (xlat != 0)
        {
            /* make sure the count is in range - if it's not, ignore it */
            if (old_id >= xlat_cnt)
            {
                /* note the problem */
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_OBJFILE_INVAL_OBJ_ID, fname);

                /* ignore the record */
                continue;
            }

            /* look up the new ID */
            switch(xlat_type)
            {
            case TCGEN_XLAT_OBJ:
                new_id = ((const tctarg_obj_id_t *)xlat)[old_id];
                break;

            case TCGEN_XLAT_PROP:
                new_id = ((const tctarg_prop_id_t *)xlat)[old_id];
                break;

            case TCGEN_XLAT_ENUM:
                new_id = ((const ulong *)xlat)[old_id];
                break;
            }

            /* apply the fixup */
            if (stream_element_size == 2)
                stream->write2_at(ofs, (uint)new_id);
            else
                stream->write4_at(ofs, new_id);
        }
        else
        {
            /* use the original ID for now */
            new_id = old_id;
        }

        /* 
         *   If we're keeping object fixups in the new file, create a fixup
         *   for the reference.  Note that the fixup is for the new ID, not
         *   the old ID, because we've already translated the value in the
         *   stream to the new ID.  
         */
        if (fixup_list_head != 0)
            CTcIdFixup::add_fixup(fixup_list_head, stream, ofs, new_id);
    }
}

