#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/tcgen.cpp,v 1.4 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcgen.cpp - TADS 3 Compiler code generator support classes
Function
  
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
 *   Data/Code Stream Parser-Allocated Object 
 */

/* 
 *   allocate via a parser memory allocator
 */
void *CTcCSPrsAllocObj::operator new(size_t siz, CTcPrsMem *allocator)
{
    /* allocate via the allocator */
    return allocator->alloc(siz);
}

/* ------------------------------------------------------------------------ */
/*
 *   Data Stream 
 */

/*
 *   initialize 
 */
CTcDataStream::CTcDataStream(char stream_id)
{
    /* remember my ID */
    stream_id_ = stream_id;

    /* nothing is allocated yet */
    ofs_ = 0;
    obj_file_start_ofs_ = 0;
    pages_ = 0;
    page_slots_ = 0;
    page_cnt_ = 0;
    page_cur_ = 0;
    rem_ = 0;
    wp_ = 0;

    /* we have no anchors yet */
    first_anchor_ = last_anchor_ = 0;

    /* create our parser memory allocator */
    allocator_ = new CTcPrsMem();
}

/*
 *   delete 
 */
CTcDataStream::~CTcDataStream()
{
    size_t i;

    /* delete the page slots if we allocated any */
    for (i = 0 ; i < page_cnt_ ; ++i)
        t3free(pages_[i]);

    /* delete the page slot array if we allocated it */
    if (pages_ != 0)
        t3free(pages_);

    /* delete our label/fixup allocator */
    delete allocator_;
}

/*
 *   Reset 
 */
void CTcDataStream::reset()
{
    /* move the write pointer back to the start */
    ofs_ = 0;
    obj_file_start_ofs_ = 0;

    /* back to the first page */
    page_cur_ = 0;

    /* set up to write to the first page, if we have any pages at all */
    if (pages_ != 0)
    {
        /* we have all of the first page available again */
        wp_ = calc_addr(0);
        rem_ = TCCS_PAGE_SIZE;
    }

    /* reset the allocator */
    allocator_->reset();

    /* 
     *   forget all of the anchors (no need to delete them explicitly -
     *   they were allocated from our allocator pool, which we've reset to
     *   completely discard everything it contained)
     */
    first_anchor_ = last_anchor_ = 0;
}

/*
 *   Decrement the write offset 
 */
void CTcDataStream::dec_ofs(int amount)
{
    /* adjust the offset */
    ofs_ -= amount;

    /* 
     *   calculate the new page we're on, since this may take us to a
     *   different page 
     */
    page_cur_ = ofs_ / TCCS_PAGE_SIZE;

    /* calculate the remaining size in this page */
    rem_ = TCCS_PAGE_SIZE - (ofs_ % TCCS_PAGE_SIZE);

    /* calculate the current write pointer */
    wp_ = calc_addr(ofs_);
}

/*
 *   Get a pointer to a block at a given offset and a given length.  
 */
const char *CTcDataStream::get_block_ptr(ulong ofs,
                                         ulong requested_len,
                                         ulong *available_len)
{
    size_t page_rem;

    /* 
     *   determine how much is left on the page containing the offset
     *   after the given offset 
     */
    page_rem = TCCS_PAGE_SIZE - (ofs % TCCS_PAGE_SIZE);

    /* 
     *   if the amount remaining on the page is greater than the request
     *   length, the available length is the entire request; otherwise,
     *   the available length is the amount remaining on the page 
     */
    if (page_rem >= requested_len)
        *available_len = requested_len;
    else
        *available_len = page_rem;

    /* return the address at this offset */
    return calc_addr(ofs);
}


/*
 *   Write bytes to the stream at an earlier offset 
 */
void CTcDataStream::write_at(ulong ofs, const char *buf, size_t len)
{
    /* if we're writing to the current offset, use the normal writer */
    if (ofs == ofs_)
        write(buf, len);
    
    /* 
     *   log an internal error, and skip writing anything, if the desired
     *   range of offsets has not been previously written 
     */
    if (ofs + len > ofs_)
        G_tok->throw_internal_error(TCERR_WRITEAT_PAST_END);

    /* write the data to each page it spans */
    while (len != 0)
    {
        size_t cur;

        /* 
         *   determine how much is left on the page containing the current
         *   starting offset 
         */
        cur = TCCS_PAGE_SIZE - (ofs % TCCS_PAGE_SIZE);

        /* 
         *   figure out how much we can copy - copy the whole remaining
         *   size, but no more than the amount remaining on this page 
         */
        if (cur > len)
            cur = len;

        /* copy the data */
        memcpy(calc_addr(ofs), buf, cur);

        /* advance past this chunk */
        len -= cur;
        ofs += cur;
        buf += cur;
    }
}

/*
 *   Copy a chunk of the stream to the given buffer 
 */
void CTcDataStream::copy_to_buf(char *buf, ulong start_ofs, ulong len)
{
    /* read the data from each page that the block spans */
    while (len != 0)
    {
        size_t cur;
        
        /* 
         *   determine how much is left on the page containing the current
         *   starting offset 
         */
        cur = TCCS_PAGE_SIZE - (start_ofs % TCCS_PAGE_SIZE);
        
        /* 
         *   figure out how much we can copy - copy the whole remaining
         *   size, but no more than the amount remaining on this page 
         */
        if (cur > len)
            cur = (size_t)len;

        /* copy the data */
        memcpy(buf, calc_addr(start_ofs), cur);

        /* advance past this chunk */
        len -= cur;
        start_ofs += cur;
        buf += cur;
    }
}

/*
 *   Reserve space 
 */
ulong CTcDataStream::reserve(size_t len)
{
    ulong ret;

    /* we'll always return the offset current before the call */
    ret = ofs_;
    
    /* if we have space on the current page, it's easy */
    if (len <= rem_)
    {
        /* advance the output pointers */
        ofs_ += len;
        wp_ += len;
        rem_ -= len;
    }
    else
    {
        /* keep going until we satisfy the request */
        do
        {
            size_t cur;

            /* if necessary, allocate more memory */
            if (rem_ == 0)
                alloc_page();

            /* limit this chunk to the space remaining on the current page */
            cur = len;
            if (cur > rem_)
                cur = rem_;

            /* skip past this chunk */
            ofs_ += cur;
            wp_ += cur;
            rem_ -= cur;
            len -= cur;

        } while (len != 0);
    }

    /* return the starting offset */
    return ret;
}

/*
 *   Append data from another stream.  The source stream is permanently
 *   moved to the new stream, destroying the original stream.  
 */
void CTcDataStream::append_stream(CTcDataStream *stream)
{
    ulong rem;
    ulong ofs;
    ulong start_ofs;
    CTcStreamAnchor *anchor;
    CTcStreamAnchor *nxt;

    /* remember the starting offset of the copy in my stream */
    start_ofs = get_ofs();
    
    /* copy all data from the other stream */
    for (ofs = 0, rem = stream->get_ofs() ; rem != 0 ; )
    {
        ulong request;
        const char *ptr;
        ulong actual;

        /* 
         *   request as much as possible from the other stream, up to the
         *   remaining length or 64k, whichever is smaller 
         */
        request = 65535;
        if (rem < request)
            request = rem;

        /* get the chunk from the source stream */
        ptr = stream->get_block_ptr(ofs, request, &actual);

        /* 
         *   write this chunk (which we know is less than 64k and can thus
         *   be safely cast to size_t, even on 16-bit machines) 
         */
        write(ptr, (size_t)actual);

        /* advance our counters */
        rem -= actual;
        ofs += actual;
    }

    /*
     *   Now copy all of the anchors from the source stream to our stream.
     *   This will ensure that fixups in the other stream have
     *   corresponding fixups in this stream.  Note that we must adjust
     *   the offset of each copied anchor by the offset of the start of
     *   the copied data in our stream.  
     */
    for (anchor = stream->get_first_anchor() ; anchor != 0 ; anchor = nxt)
    {
        /* 
         *   remember the old link to the next anchor, since we're going
         *   to move the anchor to my list and thus forget about its
         *   position in the old list 
         */
        nxt = anchor->nxt_;

        /* adjust the anchor's offset */
        anchor->ofs_ += start_ofs;

        /* unlink the anchor from its old stream */
        anchor->nxt_ = 0;

        /* link it in to my anchor list */
        if (last_anchor_ != 0)
            last_anchor_->nxt_ = anchor;
        else
            first_anchor_ = anchor;
        last_anchor_ = anchor;
    }
}

/*
 *   Write bytes to the stream 
 */
void CTcDataStream::write(const char *buf, size_t len)
{
    /* 
     *   if possible, write it in one go (this is for efficiency, so that
     *   we can avoid making a few comparisons in the most common case) 
     */
    if (len <= rem_)
    {
        /* write the data */
        memcpy(wp_, buf, len);

        /* advance the output pointers */
        ofs_ += len;
        wp_ += len;
        rem_ -= len;
    }
    else
    {
        /* keep going until we satisfy the request */
        do
        {
            size_t cur;

            /* if necessary, allocate more memory */
            if (rem_ == 0)
                alloc_page();

            /* limit this chunk to the space remaining on the current page */
            cur = len;
            if (cur > rem_)
                cur = rem_;

            /* copy it to the page */
            memcpy(wp_, buf, cur);

            /* skip past the space written in the destination */
            ofs_ += cur;
            wp_ += cur;
            rem_ -= cur;

            /* advance past the space in the source */
            buf += cur;
            len -= cur;

        } while (len != 0);
    }
}

/*
 *   allocate a new page 
 */
void CTcDataStream::alloc_page()
{
    /* 
     *   if we're coming back to a page that was previously allocated, we
     *   need merely re-establish the existing page 
     */
    if (page_cur_ + 1 < page_cnt_)
    {
        /* move to the next page */
        ++page_cur_;

        /* start writing at the start of the page */
        wp_ = pages_[page_cur_];
        rem_ = TCCS_PAGE_SIZE;

        /* we're done */
        return;
    }

    /* 
     *   if we don't have room for a new page in the page array, expand
     *   the page array 
     */
    if (page_cnt_ >= page_slots_)
    {
        /* increase the page slot count */
        page_slots_ += 100;

        /* allocate or reallocate the page array */
        if (pages_ == 0)
            pages_ = (char **)t3malloc(page_slots_ * sizeof(pages_[0]));
        else
            pages_ = (char **)t3realloc(pages_,
                                        page_slots_ * sizeof(pages_[0]));

        /* if that failed, throw an error */
        if (pages_ == 0)
            err_throw(TCERR_CODEGEN_NO_MEM);
    }

    /* allocate the new page */
    pages_[page_cnt_] = (char *)t3malloc(TCCS_PAGE_SIZE);

    /* throw an error if we couldn't allocate the page */
    if (pages_[page_cnt_] == 0)
        err_throw(TCERR_CODEGEN_NO_MEM);

    /* start writing at the start of the new page */
    wp_ = pages_[page_cnt_];

    /* the entire page is free */
    rem_ = TCCS_PAGE_SIZE;

    /* make the new page the current page */
    page_cur_ = page_cnt_;

    /* count the new page */
    ++page_cnt_;
}

/*
 *   Add an absolute fixup for this stream at the current write offset. 
 */
void CTcDataStream::add_abs_fixup(CTcAbsFixup **list_head)
{
    /* add the fixup to the list at my current write location */
    CTcAbsFixup::add_abs_fixup(list_head, this, get_ofs());
}


/*
 *   Add an anchor at the current offset.
 */
CTcStreamAnchor *CTcDataStream::add_anchor(CTcSymbol *owner_sym,
                                           CTcAbsFixup **fixup_list_head,
                                           ulong ofs)
{
    CTcStreamAnchor *anchor;
    
    /* allocate the anchor, giving it our current offset */
    anchor = new (allocator_) CTcStreamAnchor(owner_sym,
                                              fixup_list_head, ofs);

    /* append it to our list */
    if (last_anchor_ != 0)
        last_anchor_->nxt_ = anchor;
    else
        first_anchor_ = anchor;
    last_anchor_ = anchor;

    /* return the new anchor */
    return anchor;
}

/*
 *   Find an anchor with the given stream offset 
 */
CTcStreamAnchor *CTcDataStream::find_anchor(ulong ofs) const
{
    CTcStreamAnchor *cur;

    /* scan the anchor list */
    for (cur = first_anchor_ ; cur != 0 ; cur = cur->nxt_)
    {
        /* if this one has the desired offset, return it */
        if (cur->get_ofs() == ofs)
            return cur;
    }

    /* didn't find it */
    return 0;
}

/*
 *   Write an object ID 
 */
void CTcDataStream::write_obj_id(ulong obj_id)
{
    /* 
     *   if there's an object ID fixup list, and this is a valid object
     *   reference (not a 'nil' reference), add this reference 
     */
    if (G_keep_objfixups && obj_id != TCTARG_INVALID_OBJ)
        CTcIdFixup::add_fixup(&G_objfixup, this, get_ofs(), obj_id);

    /* write the ID */
    write4(obj_id);
}

/*
 *   Write an object ID self-reference 
 */
void CTcDataStream::write_obj_id_selfref(CTcSymObj *obj_sym)
{
    /* 
     *   Add a fixup list entry to the symbol.  This type of reference
     *   must be kept with the symbol rather than in the global list,
     *   because we must apply this type of fixup each time we renumber
     *   the symbol. 
     */
    obj_sym->add_self_ref_fixup(this, get_ofs());

    /* write the ID to the stream */
    write4(obj_sym->get_obj_id());
}


/*
 *   Write a property ID 
 */
void CTcDataStream::write_prop_id(uint prop_id)
{
    /* if there's an object ID fixup list, add this reference */
    if (G_keep_propfixups)
        CTcIdFixup::add_fixup(&G_propfixup, this, get_ofs(), prop_id);

    /* write the ID */
    write2(prop_id);
}

/*
 *   Write an enumerator ID 
 */
void CTcDataStream::write_enum_id(ulong enum_id)
{
    /* if there's a fixup list, add this reference */
    if (G_keep_enumfixups)
        CTcIdFixup::add_fixup(&G_enumfixup, this, get_ofs(), enum_id);

    /* write the ID */
    write4(enum_id);
}

/*
 *   Given a stream ID, get the stream 
 */
CTcDataStream *CTcDataStream::
   get_stream_from_id(char stream_id, const textchar_t *obj_fname)
{
    switch(stream_id)
    {
    case TCGEN_DATA_STREAM:
        return G_ds;

    case TCGEN_CODE_STREAM:
        return G_cs_main;

    case TCGEN_STATIC_CODE_STREAM:
        return G_cs_static;

    case TCGEN_OBJECT_STREAM:
        return G_os;

    case TCGEN_ICMOD_STREAM:
        return G_icmod_stream;

    case TCGEN_BIGNUM_STREAM:
        return G_bignum_stream;

    case TCGEN_REXPAT_STREAM:
        return G_rexpat_stream;

    case TCGEN_STATIC_INIT_ID_STREAM:
        return G_static_init_id_stream;

    case TCGEN_LCL_VAR_STREAM:
        return G_lcl_stream;

    default:
        G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                            TCERR_OBJFILE_INVAL_STREAM_ID, obj_fname);
        return 0;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Code Stream 
 */

/*
 *   create the code stream 
 */
CTcCodeStream::CTcCodeStream(char stream_id)
    : CTcDataStream(stream_id)
{
    /* no switch yet */
    cur_switch_ = 0;

    /* no enclosing statement yet */
    enclosing_ = 0;

    /* no code body being generated yet */
    code_body_ = 0;

    /* start writing at offset zero */
    ofs_ = 0;

    /* no symbol tables yet */
    symtab_ = 0;
    goto_symtab_ = 0;

    /* no labels yet */
    active_lbl_ = 0;
    free_lbl_ = 0;

    /* no fixups yet */
    free_fixup_ = 0;

    /* allocate an initial set of line record pages */
    line_pages_alloc_ = 0;
    line_pages_ = 0;
    alloc_line_pages(5);

    /* no line records in use yet */
    line_cnt_ = 0;

    /* no local frame yet */
    cur_frame_ = 0;
}


/*
 *   destroy the code stream 
 */
CTcCodeStream::~CTcCodeStream()
{
    size_t i;

    /* release all active labels */
    release_labels();

    /* delete the line records pages */
    for (i = 0 ; i < line_pages_alloc_ ; ++i)
        t3free(line_pages_[i]);

    /* delete the master list of pages */
    t3free(line_pages_);
}

/*
 *   Set the current local frame 
 */
CTcPrsSymtab *CTcCodeStream::set_local_frame(CTcPrsSymtab *symtab)
{
    /* remember the original local frame, so we can return it later */
    CTcPrsSymtab *old_frame = cur_frame_;

    /* add the current byte code location to the outgoing frame */
    if (old_frame != 0)
        old_frame->add_to_range(ofs_ - method_ofs_);

    /* remember the current frame */
    cur_frame_ = symtab;

    /* add it to the local frame list for the method if necessary */
    add_local_frame(symtab);

    /* add the current byte code location to the incoming frame */
    symtab->add_to_range(ofs_ - method_ofs_);

    /* return the original local frame */
    return old_frame;
}


/*
 *   Reset 
 */
void CTcCodeStream::reset()
{
    /* inherit default */
    CTcDataStream::reset();

    /* clear the line records */
    clear_line_recs();

    /* 
     *   forget all of the labels and fixups - they're allocated from the
     *   allocator pool, which we've completely reset now 
     */
    free_lbl_ = active_lbl_ = 0;
    free_fixup_ = 0;

    /* forget all of the symbol tables */
    symtab_ = 0;
    goto_symtab_ = 0;

    /* forget the frame list */
    frame_head_ = frame_tail_ = 0;
    cur_frame_ = 0;
    frame_cnt_ = 0;

    /* forget all of the statement settings */
    cur_switch_ = 0;
    enclosing_ = 0;
    code_body_ = 0;

    /* presume 'self' is not available */
    self_available_ = FALSE;
}

/*
 *   Allocate line record pages 
 */
void CTcCodeStream::alloc_line_pages(size_t number_to_add)
{
    size_t siz;
    size_t i;

    /* create or expand the master page array */
    siz = (line_pages_alloc_ + number_to_add) * sizeof(tcgen_line_page_t *);
    if (line_pages_ == 0)
        line_pages_ = (tcgen_line_page_t **)t3malloc(siz);
    else
        line_pages_ = (tcgen_line_page_t **)t3realloc(line_pages_, siz);

    /* allocate the new pages */
    for (i = line_pages_alloc_ ; i < line_pages_alloc_ + number_to_add ; ++i)
    {
        /* allocate this page */
        line_pages_[i] = (tcgen_line_page_t *)
                         t3malloc(sizeof(tcgen_line_page_t));
    }

    /* remember the new allocation */
    line_pages_alloc_ += number_to_add;
}

/*
 *   Allocate a new label object 
 */
CTcCodeLabel *CTcCodeStream::alloc_label()
{
    CTcCodeLabel *ret;
    
    /* if there's anything in the free list, use it */
    if (free_lbl_ != 0)
    {
        /* take the first one off the free list */
        ret = free_lbl_;

        /* unlink it from the list */
        free_lbl_ = free_lbl_->nxt;
    }
    else
    {
        /* allocate a new label */
        ret = new (allocator_) CTcCodeLabel;

        /* throw an error if allocation failed */
        if (ret == 0)
            err_throw(TCERR_CODEGEN_NO_MEM);
    }

    /* add the label to the active list */
    ret->nxt = active_lbl_;
    active_lbl_ = ret;

    /* return the allocated label */
    return ret;
}

/*
 *   Allocate a new fixup object
 */
CTcLabelFixup *CTcCodeStream::alloc_fixup()
{
    CTcLabelFixup *ret;

    /* if there's anything in the free list, use it */
    if (free_fixup_ != 0)
    {
        /* take the first one off the free list */
        ret = free_fixup_;

        /* unlink it from the list */
        free_fixup_ = free_fixup_->nxt;
    }
    else
    {
        /* allocate a new fixup */
        ret = new (allocator_) CTcLabelFixup;

        /* throw an error if allocation failed */
        if (ret == 0)
            err_throw(TCERR_CODEGEN_NO_MEM);
    }

    /* return the allocated fixup */
    return ret;
}

/*
 *   Release all active labels.  If any labels are undefined, log an
 *   internal error.
 */
void CTcCodeStream::release_labels()
{
    int err_cnt;

    /* we haven't found any errors yet */
    err_cnt = 0;
    
    /* run through the list of active labels */
    while (active_lbl_ != 0)
    {
        CTcCodeLabel *lbl;

        /* pull this label off of the active list */
        lbl = active_lbl_;
        active_lbl_ = active_lbl_->nxt;

        /* put this label on the free list */
        lbl->nxt = free_lbl_;
        free_lbl_ = lbl;

        /* check for unresolved fixups */
        while (lbl->fhead != 0)
        {
            CTcLabelFixup *fixup;
            
            /* pull this fixup off of the active list */
            fixup = lbl->fhead;
            lbl->fhead = lbl->fhead->nxt;

            /* put this fixup on the free list */
            fixup->nxt = free_fixup_;
            free_fixup_ = fixup;
            
            /* count the unresolved label */
            ++err_cnt;
        }
    }

    /* 
     *   if we found any unresolved fixups, log the error; there's not
     *   much point in logging each error individually, since this is an
     *   internal compiler error that the user can't do anything about,
     *   but at least give the user a count for compiler diagnostic
     *   purposes 
     */
    if (err_cnt != 0)
        G_tcmain->log_error(0, 0, TC_SEV_INTERNAL,
                          TCERR_UNRES_TMP_FIXUP, err_cnt);
}

/*
 *   Allocate a new label at the current code offset
 */
CTcCodeLabel *CTcCodeStream::new_label_here()
{
    CTcCodeLabel *lbl;
    
    /* allocate a new label */
    lbl = alloc_label();

    /* set the label's location to the current write position */
    lbl->ofs = ofs_;
    lbl->is_known = TRUE;

    /* return the new label */
    return lbl;
}

/*
 *   Allocate a new forward-reference label 
 */
CTcCodeLabel *CTcCodeStream::new_label_fwd()
{
    CTcCodeLabel *lbl;

    /* allocate a new label */
    lbl = alloc_label();

    /* the label's location is not yet known */
    lbl->ofs = 0;
    lbl->is_known = FALSE;

    /* return the new label */
    return lbl;
}

/*
 *   Define the position of a label, resolving any fixups associated with
 *   the label. 
 */
void CTcCodeStream::def_label_pos(CTcCodeLabel *lbl)
{
    /* set the label's position */
    lbl->ofs = ofs_;
    lbl->is_known = TRUE;
    
    /* resolve each fixup */
    while (lbl->fhead != 0)
    {
        CTcLabelFixup *fixup;
        long diff;
        char buf[4];

        /* pull this fixup off of the active list */
        fixup = lbl->fhead;
        lbl->fhead = lbl->fhead->nxt;

        /* 
         *   calculate the offset from the fixup position to the label
         *   position, applying the bias to the fixup position 
         */
        diff = lbl->ofs - (fixup->ofs + fixup->bias);

        /* convert the offset to the correct format and write it out */
        if (fixup->is_long)
        {
            /* write an INT4 offset value */
            oswp4(buf, diff);
            write_at(fixup->ofs, buf, 4);
        }
        else
        {
            /* write an INT2 offset value */
            oswp2s(buf, diff);
            write_at(fixup->ofs, buf, 2);
        }

        /* add this fixup to the free list, since we're finished with it */
        fixup->nxt = free_fixup_;
        free_fixup_ = fixup;
    }
}

/*
 *   Determine if a label has a fixup at a particular offset 
 */
int CTcCodeStream::has_fixup_at_ofs(CTcCodeLabel *lbl, ulong ofs)
{
    CTcLabelFixup *fixup;
    
    /* scan for a match */
    for (fixup = lbl->fhead ; fixup != 0 ; fixup = fixup->nxt)
    {
        /* if this is a match, indicate success */
        if (fixup->ofs == ofs)
            return TRUE;
    }

    /* we didn't find a match */
    return FALSE;
}

/*
 *   Remove a label's fixup at a particular offset 
 */
void CTcCodeStream::remove_fixup_at_ofs(CTcCodeLabel *lbl, ulong ofs)
{
    CTcLabelFixup *fixup;
    CTcLabelFixup *prv;
    CTcLabelFixup *nxt;

    /* scan for a match */
    for (prv = 0, fixup = lbl->fhead ; fixup != 0 ; prv = fixup, fixup = nxt)
    {
        /* remember the next one */
        nxt = fixup->nxt;
        
        /* if this is a match, remove it */
        if (fixup->ofs == ofs)
        {
            /* unlink this fixup from the list */
            if (prv == 0)
                lbl->fhead = nxt;
            else
                prv->nxt = nxt;

            /* move it to the free list */
            fixup->nxt = free_fixup_;
            free_fixup_ = fixup;
        }
    }
}

/*
 *   Write an offset value to the given label 
 */
void CTcCodeStream::write_ofs(CTcCodeLabel *lbl, int bias, int is_long)
{
    /* if the label is known, write it; otherwise, generate a fixup */
    if (lbl->is_known)
    {
        long diff;
        
        /* 
         *   calculate the branch offset from the current position,
         *   applying the bias to the current position 
         */
        diff = lbl->ofs - (ofs_ + bias);

        /* convert the offset to the correct format and write it out */
        if (is_long)
            write4(diff);
        else
            write2(diff);
    }
    else
    {
        CTcLabelFixup *fixup;

        /* allocate a fixup */
        fixup = alloc_fixup();

        /* set up the fixup data */
        fixup->ofs = ofs_;
        fixup->bias = bias;
        fixup->is_long = is_long;

        /* link the fixup into the label's fixup list */
        fixup->nxt = lbl->fhead;
        lbl->fhead = fixup;

        /* write a placeholder to the code stream */
        if (is_long)
            write4(0);
        else
            write2(0);
    }
}

/*
 *   Add a new line record at the current code offset 
 */
void CTcCodeStream::add_line_rec(CTcTokFileDesc *file, long linenum)
{
    /* if there's no file descriptor, there's nothing to add */
    if (file == 0)
        return;

    /* compute the current offset, relative to the start of the method */
    ulong cur_ofs = G_cs->get_ofs() - method_ofs_;

    /* presume we won't re-use the previous record */
    int reuse = FALSE;

    /* 
     *   If we haven't added any code since the previous line record,
     *   overwrite the previous record - it doesn't refer to any
     *   executable code, so it's an unnecessary record.  Similarly, if
     *   the previous record is at the same source position, don't add a
     *   separate line record for it, since the debugger won't be able to
     *   treat the two lines separately.  
     */
    tcgen_line_t *rec;
    if (line_cnt_ > 0)
    {
        /* get the previous record */
        rec = get_line_rec(line_cnt_ - 1);

        /* 
         *   if it refers to the same code offset, replace the old record
         *   with one at this location 
         */
        if (rec->ofs == cur_ofs)
            reuse = TRUE;

        /* 
         *   if it has the identical source file location, don't bother
         *   adding a new record at all
         */
        if (rec->source_id == file->get_index()
            && rec->source_line == linenum)
            return;
    }

    /* if we're not re-using the previous record, allocate a new one */
    if (!reuse)
    {
        /* 
         *   we need a new record - if we've used all the allocated space,
         *   allocate more 
         */
        if (line_cnt_ >= line_pages_alloc_ * TCGEN_LINE_PAGE_SIZE)
            alloc_line_pages(5);

        /* get a pointer to the next available entry */
        rec = get_line_rec(line_cnt_);
        
        /* count the new record */
        ++line_cnt_;
    }

    /* store the code offset relative to the start of the current method */
    rec->ofs = cur_ofs;

    /* store the file information */
    rec->source_id = file->get_index();
    rec->source_line = linenum;

    /* store the frame information */
    rec->frame = cur_frame_;
}

/*
 *   Get the nth line record 
 */
tcgen_line_t *CTcCodeStream::get_line_rec(size_t n)
{
    return &(line_pages_[n / TCGEN_LINE_PAGE_SIZE]
             ->lines[n % TCGEN_LINE_PAGE_SIZE]);
}

/*
 *   Add a frame to the list of local frames in the method 
 */
void CTcCodeStream::add_local_frame(CTcPrsSymtab *symtab)
{
    /* 
     *   If this is the global symbol table, or it's null, or it's already
     *   in a list, ignore it.  Note that we can tell if the item is in a
     *   list by checking its index value - a value of zero is never a
     *   valid index and thus indicates that the item isn't in a list yet.
     */
    if (symtab == G_prs->get_global_symtab()
        || symtab == 0
        || symtab->get_list_index() != 0)
        return;
    
    /* link the frame in at the tail of our list */
    symtab->set_list_next(0);
    if (frame_tail_ == 0)
        frame_head_ = symtab;
    else
        frame_tail_->set_list_next(symtab);
    frame_tail_ = symtab;

    /* count the new entry in the list */
    ++frame_cnt_;

    /* 
     *   Set this frame's index in the list.  Note that we've already
     *   incremented the index value, so the first frame in the list will
     *   have index 1, as is required. 
     */
    symtab->set_list_index(frame_cnt_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Data stream anchor 
 */

/*
 *   Get the length.  We can deduce the length by subtracting our offset
 *   from the next item's offset, or, if we're the last item, from the
 *   length of the stream.  
 */
ulong CTcStreamAnchor::get_len(CTcDataStream *ds) const
{
    if (nxt_ != 0)
    {
        /* 
         *   there's another item after me - my length is the difference
         *   between the next item's offset and my offset 
         */
        return (nxt_->ofs_ - ofs_);
    }
    else
    {
        /* I'm the last item - my length is whatever is left in the stream */
        return (ds->get_ofs() - ofs_);
    }
}

/*
 *   Set the finaly absoluate address, and apply fixups.  The code
 *   generator must invoke this during the link phase once this object's
 *   final address is known.  
 */
void CTcStreamAnchor::set_addr(ulong addr)
{
    /* remember my address */
    addr_ = addr;
    
    /* apply all outstanding fixups for this object */
    CTcAbsFixup::fix_abs_fixup(*fixup_list_head_, addr);
}

/* ------------------------------------------------------------------------ */
/*
 *   Absolute Fixup Object 
 */

/*
 *   Add an absolute fixup at the current stream location to a given fixup
 *   list.  
 */
void CTcAbsFixup::add_abs_fixup(CTcAbsFixup **list_head,
                                CTcDataStream *ds, ulong ofs)
{
    CTcAbsFixup *fixup;

    /* 
     *   create the fixup object - allocate it out of our fixup allocator
     *   pool, since this fixup object has the same attributes (small and
     *   long-lived) as other fixup objects 
     */
    fixup = (CTcAbsFixup *)G_prsmem->alloc(sizeof(CTcAbsFixup));

    /* set the fixup location to the current offset */
    fixup->ds = ds;
    fixup->ofs = ofs;

    /* link it in to the caller's list */
    fixup->nxt = *list_head;
    *list_head = fixup;
}

/*
 *   Fix up a fix-up list 
 */
void CTcAbsFixup::fix_abs_fixup(CTcAbsFixup *list_head, ulong final_ofs)
{
    CTcAbsFixup *cur;

    /* scan the list and fix up each entry */
    for (cur = list_head ; cur != 0 ; cur = cur->nxt)
    {
        /* 
         *   fix this entry by writing the final offset in UINT4 format to
         *   the target stream at the target offset 
         */
        cur->ds->write4_at(cur->ofs, final_ofs);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Object/property ID fixups 
 */

/*
 *   add a fixup to a list 
 */
void CTcIdFixup::add_fixup(CTcIdFixup **list_head, class CTcDataStream *ds,
                           ulong ofs, ulong id)
{
    CTcIdFixup *fixup;
    
    /* create the new fixup object */
    fixup = new (G_prsmem) CTcIdFixup(ds, ofs, id);

    /* link it in at the head of the list */
    fixup->nxt_ = *list_head;
    *list_head = fixup;
}

/*
 *   Apply this fixup
 */
void CTcIdFixup::apply_fixup(ulong final_id, size_t siz)
{
    /* write the fixup */
    if (siz == 2)
        ds_->write2_at(ofs_, (uint)final_id);
    else
        ds_->write4_at(ofs_, final_id);
}

