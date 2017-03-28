/* $Header: d:/cvsroot/tads/tads3/tcgen.h,v 1.4 1999/07/11 00:46:58 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcgen.h - TADS 3 Compiler code generator support classes
Function
  
Notes
  
Modified
  05/08/99 MJRoberts  - Creation
*/

#ifndef TCGEN_H
#define TCGEN_H

#include "t3std.h"
#include "os.h"

#include "tctargty.h"

/* ------------------------------------------------------------------------ */
/*
 *   Stream ID's - an ID is associated with each data stream, and is used
 *   to identify the stream in an object file.  These ID's are stored in
 *   object files, so any changes will render old object files obsolete
 *   and therefore require changing the object file signature's version
 *   number.  
 */

/* the constant pool data stream */
const char TCGEN_DATA_STREAM = 1;

/* the primary code pool data stream */
const char TCGEN_CODE_STREAM = 2;

/* the static object data stream */
const char TCGEN_OBJECT_STREAM = 3;

/* 
 *   dictionary object data stream (not stored in an object file, because
 *   dictionaries aren't generated until link time) 
 */
const char TCGEN_DICT_STREAM = 4;

/* 
 *   grammar production object data stream (not stored in an object file,
 *   because grammar production objects aren't generated until link time) 
 */
const char TCGEN_GRAMPROD_STREAM = 5;

/* BigNumber data stream */
const char TCGEN_BIGNUM_STREAM = 6;

/* IntrinsicClass data stream */
const char TCGEN_INTCLASS_STREAM = 7;

/* intrinsic class modifier data stream */
const char TCGEN_ICMOD_STREAM = 8;

/* static initializer code stream */
const char TCGEN_STATIC_CODE_STREAM = 9;

/* 
 *   static initializer stream - contains a list of obj.prop identifiers
 *   for static initialization 
 */
const char TCGEN_STATIC_INIT_ID_STREAM = 10;

/* local variable name stream - for local variable frame information */
const char TCGEN_LCL_VAR_STREAM = 11;

/* RexPattern data stream */
const char TCGEN_REXPAT_STREAM = 12;


/* ------------------------------------------------------------------------ */
/*
 *   Data Stream.  This object provides an in-memory byte stream.
 *   
 *   The data stream provides essentially a flat 32-bit address space, but
 *   dynamically allocates memory as needed, and works on 16-bit machines.
 *   The write pointer starts at offste zero, and is incremented by one
 *   for each byte written.  
 */

/* 
 *   size in bytes of each page - we'll use a size that will work in
 *   16-bit architectures 
 */
const size_t TCCS_PAGE_SIZE = 65000;

class CTcDataStream
{
public:
    CTcDataStream(char stream_id);
    virtual ~CTcDataStream();

    /* 
     *   get my stream ID - this is assigned during stream creation and is
     *   used to identify the stream in an object file 
     */
    char get_stream_id() const { return stream_id_; }

    /*
     *   Given a stream ID (TCGEN_xxx_STREAM), get the stream object.
     *   Logs an error if the stream ID is invalid.  
     */
    static CTcDataStream *get_stream_from_id(char stream_id,
                                             const textchar_t *obj_fname);

    /* get the current write pointer offset */
    ulong get_ofs() const { return ofs_; }

    /* 
     *   decrement the write offset - this can be used to remove one or
     *   more bytes from the stream (for peephole optimization, for
     *   example) 
     */
    void dec_ofs(int amt);

    /*
     *   Get a pointer to a block at a given offset and a given length.
     *   The block might not be returned contiguously, so we return how
     *   much data can be read at the returned pointer in
     *   (*available_len).  If (*available_len) on return is less than
     *   requested_len, the caller must make a subsequent call, at (ofs +
     *   *available_len) of length (requested_len - *available_len), to
     *   read the next chunk; this process must be iterated until the
     *   entire request has been satisfied.  
     */
    const char *get_block_ptr(ulong ofs,
                              ulong requested_len, ulong *available_len);

    /* extract a chunk of the stream into the given buffer */
    void copy_to_buf(char *buf, ulong start_ofs, ulong len);

    /* 
     *   Reserve space, advancing the write pointer without copying any
     *   data.  The space written must eventually be written with
     *   write_at(), etc.  Returns the offset of the start of the reserved
     *   space (which will always simply be the current offset before the
     *   call).  
     */
    ulong reserve(size_t len);

    /* 
     *   Append data from another stream.  The old stream is destroyed by
     *   this operation - the data are physically moved from the old
     *   stream to the new stream.  Fixup information is moved along with
     *   the stream data.  
     */
    void append_stream(CTcDataStream *stream);

    /* write bytes to the stream */
    void write(char b) { write(&b, 1); }
    void write(const char *buf, size_t len);

    /* write bytes at an earlier offset */
    void write_at(ulong ofs, const char *buf, size_t len);
    void write_at(ulong ofs, char b) { write_at(ofs, &b, 1); }

    /* write a TADS portable UINT2 value */
    void write2(uint val)
    {
        char buf[2];

        /* convert the value to UINT2 format and write it out */
        oswp2(buf, val);
        write(buf, 2);
    }

    /* write a UINT2 at a given offset */
    void write2_at(ulong ofs, uint val)
    {
        char buf[2];

        /* convert the value to UINT2 format and write it out */
        oswp2(buf, val);
        write_at(ofs, buf, 2);
    }

    /* write a TADS portable UINT4 value */
    void write4(ulong val)
    {
        char buf[4];

        /* convert the value to UINT4 format and write it out */
        oswp4(buf, val);
        write(buf, 4);
    }

    /* write a TADS portable UINT4 value at a given offset */
    void write4_at(ulong ofs, ulong val)
    {
        char buf[4];

        /* convert the value to UINT4 format and write it out */
        oswp4(buf, val);
        write_at(ofs, buf, 4);
    }

    /*
     *   Write an object ID at the current offset.  If there's a global
     *   object ID fixup list, we'll add this reference to the list.  
     */
    void write_obj_id(ulong obj_id);

    /*
     *   Write an object ID self-reference.  This must be used when a
     *   modification of this object that assigns it a new object ID (not
     *   due to linking, but due to explicit replacement with the 'modify'
     *   statement) requires that this reference to the ID be changed to
     *   match the new object ID rather than referring to the replacement
     *   object.
     */
    void write_obj_id_selfref(class CTcSymObj *obj_sym);

    /*
     *   Write a property ID at the current offset.  If there's a global
     *   property ID fixup list, we'll add this reference to the list. 
     */
    void write_prop_id(uint prop_id);

    /* write an enum ID at the current offset, keeping fixups if needed */
    void write_enum_id(ulong enum_id);

    /* get the byte at the given offset */
    char get_byte_at(ulong ofs)
    {
        return *calc_addr(ofs);
    }

    /* read an INT2 value at the given offst */
    int read2_at(ulong ofs)
    {
        char buf[2];

        /* read the two bytes */
        buf[0] = get_byte_at(ofs);
        buf[1] = get_byte_at(ofs + 1);
        return osrp2s(buf);
    }

    /* read a UINT2 value at the given offset */
    uint readu2_at(ulong ofs)
    {
        char buf[2];

        /* read the two bytes */
        buf[0] = get_byte_at(ofs);
        buf[1] = get_byte_at(ofs + 1);
        return osrp2(buf);
    }

    /* read an INT4 value at the given offset */
    int read4_at(ulong ofs)
    {
        char buf[4];

        /* read the four bytes */
        buf[0] = get_byte_at(ofs);
        buf[1] = get_byte_at(ofs + 1);
        buf[2] = get_byte_at(ofs + 2);
        buf[3] = get_byte_at(ofs + 3);
        return osrp4s(buf);
    }

    /* read a UINT4 value at the given offset */
    uint readu4_at(ulong ofs)
    {
        char buf[4];

        /* read the four bytes */
        buf[0] = get_byte_at(ofs);
        buf[1] = get_byte_at(ofs + 1);
        buf[2] = get_byte_at(ofs + 2);
        buf[3] = get_byte_at(ofs + 3);
        return t3rp4u(buf);
    }

    /*
     *   Add an absolute fixup at the current stream location to a given
     *   absolute fixup list.  We'll create a new fixup object, record our
     *   current offset in the fixup so that we come back and fix this
     *   location, and add the fixup object to the given list.  
     */
    void add_abs_fixup(struct CTcAbsFixup **list_head);

    /* get the head of my list of anchors */
    struct CTcStreamAnchor *get_first_anchor()
        const { return first_anchor_; }
    
    /* 
     *   Add an anchor at the current offset.  This should be used each
     *   time an atomic item is written to the stream.
     *   
     *   If the fixup list head pointer is not null, we will store fixups
     *   for the anchor in the given list.  Otherwise, we'll provide an
     *   internal list head in the anchor object.  Objects that are
     *   reachable through multiple references (such as an object
     *   associated with a symbol table entry) must typically keep their
     *   own fixup lists, because the fixup list might be needed before
     *   and after the data stream object is created.  Objects that can
     *   only be reached through a single reference, which is created only
     *   when the data stream object is created, can use the internal
     *   fixup list instead.
     *   
     *   If an external fixup list is provided, the owning symbol table
     *   entry must be provided.  This is necessary so that, when writing
     *   the anchor to an object file, we can also store a reference to
     *   the symbol that owns the list; this allows the association to be
     *   restored when the object file is loaded.  
     */
    struct CTcStreamAnchor *add_anchor(class CTcSymbol *owner_sym,
                                       struct CTcAbsFixup **fixup_list_head)
    {
        return add_anchor(owner_sym, fixup_list_head, get_ofs());
    }

    /* add an anchor at the given offset */
    struct CTcStreamAnchor *add_anchor(class CTcSymbol *owner_sym,
                                       struct CTcAbsFixup **fixup_list_head,
                                       ulong ofs);

    /* find an anchor at the given offset */
    struct CTcStreamAnchor *find_anchor(ulong ofs) const;

    /*
     *   Write the stream to an object file.  Writes the stream data and
     *   the list of anchors and their fixups.  Every fixup should be
     *   reachable from an anchor, and all of the anchors are in our
     *   anchor list.  
     */
    void write_to_object_file(class CVmFile *fp);

    /*
     *   Load an object file 
     */
    void load_object_file(class CVmFile *fp, const textchar_t *obj_fname);

    /*
     *   Get/set the object file starting offset - this is the base offset
     *   of the stream for the current object.  Because streams can refer
     *   to one another, the object file loader must set the starting
     *   offset for all streams before reading the first stream for an
     *   object file.  The 'set' call simply sets the starting offset to
     *   the current offset; this won't be affected by subsequent loading
     *   into the stream, so we have a stable base address for the object
     *   file data in the stream. 
     */
    ulong get_object_file_start_ofs() const { return obj_file_start_ofs_; }
    void set_object_file_start_ofs() { obj_file_start_ofs_ = ofs_; }

    /*
     *   Reset - discard all data in the stream 
     */
    virtual void reset();

protected:
    /* allocate a new page */
    void alloc_page();

    /* 
     *   Calculate the memory address of a given byte, given the byte's
     *   offset.  The caller must be sure that the offset is less than the
     *   current write position, since this routine does not allocate new
     *   memory.  
     */
    char *calc_addr(ulong ofs) const
    {
        /* 
         *   get the page number by dividing the offset by the page size;
         *   get the offset in the page from the remainder of the same
         *   division 
         */
        return pages_[ofs / TCCS_PAGE_SIZE] + (ofs % TCCS_PAGE_SIZE);
    }

    /* current write offset */
    ulong ofs_;

    /* start offset of the stream for the current object file's data */
    ulong obj_file_start_ofs_;

    /* current write pointer */
    char *wp_;

    /* space remaining on the current page */
    size_t rem_;

    /* current page */
    size_t page_cur_;

    /* array of pages */
    char **pages_;

    /* number of page slots */
    size_t page_slots_;

    /* number of pages allocated */
    size_t page_cnt_;

    /* head and tail of my list of anchors */
    struct CTcStreamAnchor *first_anchor_;
    struct CTcStreamAnchor *last_anchor_;

    /* 
     *   parser memory allocator - we use this for allocating label and
     *   fixup objects; these objects fit the characteristics used for the
     *   parser memory allocator, so we can get better memory management
     *   efficiency by using this allocator 
     */
    class CTcPrsMem *allocator_;

    /* my stream ID, for identification in the object file */
    char stream_id_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Debugging line record.  This record stores information on one
 *   executable line of source code, associating the byte-code location of
 *   the code with the source file ID and line number, plus the frame
 *   identifier (which gives the local scope in effect for the line of
 *   code).  
 */
struct tcgen_line_t
{
    /* 
     *   Byte-code offset of the first instruction for this source line.
     *   This is expressed as an offset from the start of the method
     *   header, which means that this value is relative and thus doesn't
     *   need any adjustment for linking or any other changes to the
     *   absolute location where the code is stored.  
     */
    uint ofs;

    /* 
     *   Source file ID and line number.  The source file ID is the index
     *   of the file descriptor in the tokenizer's master source file
     *   list, so it must be adjusted when multiple object files are
     *   linked together for the fact that the file descriptor indices can
     *   change.  
     */
    int source_id;
    long source_line;

    /* 
     *   frame - this is the local scope frame for this line, which
     *   specifies the local symbol table in effect for the line of code 
     */
    class CTcPrsSymtab *frame;
};

/* number of line records per page */
const size_t TCGEN_LINE_PAGE_SIZE = 1024;

/*
 *   Debugging line record page.  We allocate blocks of line records for
 *   memory efficiency; this is a page of line numbers. 
 */
struct tcgen_line_page_t
{
    /* underlying line records */
    tcgen_line_t lines[TCGEN_LINE_PAGE_SIZE];
};


/* ------------------------------------------------------------------------ */
/*
 *   Code Stream.  This object provides a place for code generators to
 *   store byte code.  A code stream is an extension of a byte stream,
 *   with support for symbol table access; enclosing switch, loop, and
 *   'try' block tracking; forward-reference labels; and relative jump
 *   generation.
 */

/* code stream class */
class CTcCodeStream: public CTcDataStream
{
public:
    CTcCodeStream(char stream_id);
    ~CTcCodeStream();

    /*
     *   Temporary label operations.  A temporary label can be used to
     *   generate forward or reverse jumps.
     */

    /* allocate a new label at the current write offset */
    struct CTcCodeLabel *new_label_here();

    /* 
     *   allocate a forward-reference label; the position will be defined
     *   later via the def_label_pos() method 
     */
    struct CTcCodeLabel *new_label_fwd();

    /* 
     *   define the position of a label allocated as a forward reference
     *   to be the current write offset 
     */
    void def_label_pos(struct CTcCodeLabel *lbl);

    /*
     *   Remove a fixup at a particular location for a label.  This can be
     *   used if an instruction is being removed (for optimization, for
     *   example).  
     */
    void remove_fixup_at_ofs(struct CTcCodeLabel *lbl, ulong ofs);

    /*
     *   Check a label's fixup list to see if it has a fixup at a
     *   particular code stream offset.  Returns true if there is such a
     *   fixup, false if not. 
     */
    int has_fixup_at_ofs(struct CTcCodeLabel *lbl, ulong ofs);

    /* 
     *   Release all active labels.  Logs an internal error if any labels
     *   are still forward-declared. 
     */
    void release_labels();

    /*
     *   Get the current enclosing statement.  This is used to find the
     *   target of a 'break' or 'continue', and is also used to invoke
     *   'finally' blocks when leaving a nested block. 
     */
    class CTPNStmEnclosing *get_enclosing() const { return enclosing_; }

    /*
     *   Set the enclosing statement.  During code generation, each time
     *   an enclosing statement is encountered, it should be set as the
     *   current enclosing statement for the duration of its code
     *   generation, so that its subnodes can find it.  This routine
     *   returns the previous enclosing statement so that it can be
     *   restored later.  
     */
    class CTPNStmEnclosing *set_enclosing(CTPNStmEnclosing *stm)
    {
        CTPNStmEnclosing *old_stm;

        /* save the old one */
        old_stm = enclosing_;

        /* set the new one */
        enclosing_ = stm;

        /* return the old one */
        return old_stm;
    }

    /*
     *   Set the current code body 
     */
    class CTPNCodeBody *set_code_body(CTPNCodeBody *cb)
    {
        CTPNCodeBody *old_cb;

        /* save the old one */
        old_cb = code_body_;

        /* set the new one */
        code_body_ = cb;

        /* return the old one */
        return old_cb;
    }

    /* get the current code body being generated */
    class CTPNCodeBody *get_code_body() const { return code_body_; }

    /* 
     *   Write the relative offset to the given label as a 2- or 4-byte
     *   value in TADS portable INT2 or INT4 format.  The 'bias' value
     *   gives the offset from the current write pointer of the source
     *   position; the relative value written is thus:
     *   
     *   (label - (current + bias))
     */
    void write_ofs2(struct CTcCodeLabel *lbl, int bias)
        { write_ofs(lbl, bias, FALSE); }
    void write_ofs4(struct CTcCodeLabel *lbl, int bias)
        { write_ofs(lbl, bias, TRUE); }

    /*
     *   Set the enclosing "switch" statement node, returning the previous
     *   one to allow for later restoration 
     */
    class CTPNStmSwitch *set_switch(class CTPNStmSwitch *sw)
    {
        class CTPNStmSwitch *old_sw;

        /* remember the current switch node for a moment */
        old_sw = cur_switch_;

        /* set the new switch */
        cur_switch_ = sw;

        /* return the previous one */
        return old_sw;
    }

    /* get the current switch */
    class CTPNStmSwitch *get_switch() const { return cur_switch_; }

    /* get the active symbol table */
    class CTcPrsSymtab *get_symtab() const { return symtab_; }

    /* get the active 'goto' symbol table */
    class CTcPrsSymtab *get_goto_symtab() const { return goto_symtab_; }

    /* set the active symbol table */
    void set_symtab(class CTcPrsSymtab *symtab) { symtab_ = symtab; }

    /* set the active 'goto' symbol table */
    void set_goto_symtab(class CTcPrsSymtab *symtab)
        { goto_symtab_ = symtab; }

    /* 
     *   get/self 'self' availability - if we're generating code for a
     *   stand-alone function, 'self' is not available; if we're
     *   generating code for an object method, 'self' is available 
     */
    int is_self_available() const { return self_available_; }
    void set_self_available(int f) { self_available_ = f; }

    /* add a debugging line record at the current byte-code offset */
    void add_line_rec(class CTcTokFileDesc *file, long linenum);

    /* clear all line records */
    void clear_line_recs() { line_cnt_ = 0; }

    /* get the number of line records */
    size_t get_line_rec_count() const { return line_cnt_; }

    /* get the nth line record */
    struct tcgen_line_t *get_line_rec(size_t n);

    /* set the starting offset of the current method */
    void set_method_ofs(ulong ofs) { method_ofs_ = ofs; }

    /* clear the list of local frames in the current method */
    void clear_local_frames()
    {
        /* clear the list */
        frame_head_ = frame_tail_ = 0;

        /* reset the count */
        frame_cnt_ = 0;
    }

    /* 
     *   Set the current local frame.  This will add the frame to the
     *   master list of frames for the current method if it's not already
     *   there, and will establish the frame as the current local frame.
     *   Returns the previous local frame, so that the caller can restore
     *   the enclosing frame when leaving a nested frame.  
     */
    class CTcPrsSymtab *set_local_frame(class CTcPrsSymtab *symtab);

    /* get the local frame count for this method */
    size_t get_frame_count() const { return frame_cnt_; }

    /* get the head of the frame list for the method */
    class CTcPrsSymtab *get_first_frame() const { return frame_head_; }

    /* reset */
    virtual void reset();

protected:
    /* 
     *   add a frame to the list of local frames; does nothing if the
     *   frame is already in the list for this method 
     */
    void add_local_frame(class CTcPrsSymtab *symtab);

    /* allocate additional line record pages */
    void alloc_line_pages(size_t number_to_add);

    /* allocate a label object */
    struct CTcCodeLabel *alloc_label();
    
    /* allocate a fixup object */
    struct CTcLabelFixup *alloc_fixup();

    /* 
     *   Write an offset to the given label.  If is_long is true, we'll
     *   write an INT4 offset; otherwise we'll write an INT2 offset value. 
     */
    void write_ofs(struct CTcCodeLabel *lbl, int bias, int is_long);

    /* head of list of active temporary labels */
    struct CTcCodeLabel *active_lbl_;

    /* head of list of free temporary label objects */
    struct CTcCodeLabel *free_lbl_;

    /* head of list of free label fixup objects */
    struct CTcLabelFixup *free_fixup_;

    /* current symbol table */
    class CTcPrsSymtab *symtab_;

    /* current 'goto' symbol table */
    class CTcPrsSymtab *goto_symtab_;

    /* current "switch" statement */
    class CTPNStmSwitch *cur_switch_;

    /* current enclosing statement */
    class CTPNStmEnclosing *enclosing_;

    /* current code body being generated */
    class CTPNCodeBody *code_body_;

    /* flag: 'self' is available in current code body */
    unsigned int self_available_ : 1;

    /* array of line record pages */
    tcgen_line_page_t **line_pages_;
    size_t line_pages_alloc_;

    /* number of line records actually used */
    size_t line_cnt_;

    /* starting offst of current method */
    ulong method_ofs_;

    /* head and tail of list of local frames for the current method */
    class CTcPrsSymtab *frame_head_;
    class CTcPrsSymtab *frame_tail_;

    /* number of frames in the local method list so far */
    size_t frame_cnt_;

    /* currently active local frame */
    class CTcPrsSymtab *cur_frame_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Code Stream Parser-Allocated Object - this is a base class for
 *   objects allocated by a CTcPrsMem allocator. 
 */
struct CTcCSPrsAllocObj
{
    /* allocate via the parser allocator */
    void *operator new(size_t siz, class CTcPrsMem *allocator);
};

/* ------------------------------------------------------------------------ */
/*
 *   Code label 
 */
struct CTcCodeLabel: CTcCSPrsAllocObj
{
    CTcCodeLabel()
    {
        /* clear members */
        nxt = 0;
        ofs = 0;
        fhead = 0;
        is_known = FALSE;
    }

    /* next code label */
    CTcCodeLabel *nxt;

    /* offset of the code associated with the label */
    ulong ofs;

    /* 
     *   head of list of fixups for this label; this will always be null
     *   once the label's offset is known, since we will resolve any
     *   existing fixups as soon as the label is defined and will add no
     *   further fixups after it's known, since we can generate correct
     *   offsets directly 
     */
    struct CTcLabelFixup *fhead;

    /* flag: true -> offset is known */
    uint is_known : 1;
};

/*
 *   Code label fixup.  Each time we generate a jump to a code label that
 *   hasn't been defined yet, we'll generate a fixup and attach it to the
 *   label.  The fixup records the location of the jump; as soon as the
 *   label is defined, we'll go through all of the fixup records
 *   associated with the label, and fill in the correct jump offset.  
 */
struct CTcLabelFixup: CTcCSPrsAllocObj
{
    CTcLabelFixup()
    {
        /* clear members */
        nxt = 0;
        ofs = 0;
        bias = 0;
        is_long = FALSE;
    }
    
    /* next fixup in same list */
    CTcLabelFixup *nxt;

    /* code offset of the jump in need of fixing */
    ulong ofs;

    /* 
     *   bias to apply to jump offset (the value we will write is:
     *   
     *   (target - (ofs + bias)) 
     */
    int bias;

    /* long jump - true -> INT4 offset, false -> INT2 offset */
    uint is_long : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   Absolute Fixup.  Each time we generate a reference to an offset in a
 *   stream, we must save the location so that we can go back and fix up
 *   the offset after the final image file layout configuration.  We can't
 *   know until we've generated the entire program what the final location
 *   of any stream is, because we won't know how large we're going to make
 *   the pool pages until we've generated the whole program.
 *   
 *   Each absolute fixup is stored in a list anchored in the object that
 *   owns the object in the stream to which the fixup refers.  During the
 *   image layout configuration phase, we go through the list of all pool
 *   blocks, and figure out where the pool block will go in the image
 *   file; once this is known, we apply all of the fixups for that
 *   block by scanning the block's fixup list.  
 */
struct CTcAbsFixup: CTcCSPrsAllocObj
{
    /* next fixup in same list */
    CTcAbsFixup *nxt;

    /* stream containing the reference */
    class CTcDataStream *ds;
    
    /* location in stream 'ds' of the 4-byte pointer value to fix */
    ulong ofs;

    /*
     *   Add an absolute fixup, at a given offset in a given stream, to a
     *   given list.  
     */
    static void add_abs_fixup(struct CTcAbsFixup **list_head,
                              CTcDataStream *ds, ulong ofs);

    /*
     *   Fix up a fix-up list, given the final address of the referenced
     *   object.  Scans the fix-up list and stores the final address at
     *   each location in the list.  
     */
    static void fix_abs_fixup(struct CTcAbsFixup *list_head,
                              ulong final_ofs);

    /* write a fixup list to an object file */
    static void write_fixup_list_to_object_file(class CVmFile *fp,
                                                CTcAbsFixup *list_head);

    /* load a fixup list from an object file */
    static void
        load_fixup_list_from_object_file(class CVmFile *fp,
                                         const textchar_t *obj_fname,
                                         CTcAbsFixup **list_head);

};

/* ------------------------------------------------------------------------ */
/*
 *   ID Fixup.  We use this to track a reference to an object ID or
 *   property ID.  These fixups are needed when combining object files
 *   that were compiled separately, since we must reconcile the ID name
 *   spaces of the multiple files.  
 */

/* translation datatypes */
enum tcgen_xlat_type
{
    /* object ID's - translation table type is (tctarg_obj_id_t *) */
    TCGEN_XLAT_OBJ,

    /* property ID's - translation table type is (tctarg_prop_id_t *) */
    TCGEN_XLAT_PROP,

    /* enum's - translation table type is (ulong *) */
    TCGEN_XLAT_ENUM
};

/*
 *   ID fixup class 
 */
struct CTcIdFixup: CTcCSPrsAllocObj
{
    /* initialize */
    CTcIdFixup(class CTcDataStream *ds, ulong ofs, ulong id)
    {
        /* remember the data */
        ds_ = ds;
        ofs_ = ofs;
        id_ = id;

        /* we're not in a list yet */
        nxt_ = 0;
    }
    
    /* add a fixup to a fixup list */
    static void add_fixup(CTcIdFixup **list_head, class CTcDataStream *ds,
                          ulong ofs, ulong id);

    /* write a fixup list to an object file */
    static void write_to_object_file(class CVmFile *fp, CTcIdFixup *head);

    /* 
     *   Load an ID fixup list from an object file.
     *   
     *   'xlat' is the translation array; it consists of elements of the
     *   appropriate type, depending on xlat_type.
     *   
     *   'xlat_cnt' is the number of elements in the 'xlat' array.
     *   
     *   'stream_element_size' is the size of the stream data elements
     *   that we're fixing up.  This can be 2 for a UINT2 value, or 4 for
     *   a UINT4 value.
     *   
     *   'fname' is the object filename, which we need for reporting
     *   errors that we encounter in the fixup data.
     *   
     *   If 'fixup_list_head' is provided, it points to the head of a list
     *   that we use to store new fixups.  For each fixup we read, we
     *   create a new fixup referring to the same element.  This is needed
     *   if the file we're reading will be turned back into another object
     *   file at some point and needs relative object ID information to be
     *   stored.  
     */
    static void load_object_file(class CVmFile *fp,
                                 const void *xlat, ulong xlat_cnt,
                                 tcgen_xlat_type xlat_type,
                                 size_t stream_element_size,
                                 const textchar_t *fname,
                                 CTcIdFixup **fixup_list_head);

    /* 
     *   apply the fixup, given the final ID to use, and the size of the
     *   data item to write (2 for a UINT2, 4 for a UINT4) 
     */
    void apply_fixup(ulong final_id, size_t siz);

    /* the stream containing the reference to this object */
    class CTcDataStream *ds_;

    /* the offset in the stream of the reference */
    ulong ofs_;

    /* 
     *   the local ID - this is the ID that is used locally in the
     *   separate file before it's translated to the final value global to
     *   the combined files 
     */
    ulong id_;

    /* next fixup in the list */
    CTcIdFixup *nxt_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Data Stream Anchor.  Each time we add an object to a data stream, we
 *   must add an anchor to the stream.  The anchor tracks the indivisible
 *   object and all references (via a fixup list) to the object.  
 */
struct CTcStreamAnchor: CTcCSPrsAllocObj
{
    CTcStreamAnchor(class CTcSymbol *fixup_owner_sym,
                    struct CTcAbsFixup **fixup_list_head, ulong stream_ofs)
    {
        /* we're not in a list yet */
        nxt_ = 0;

        /* 
         *   if the caller provided an external fixup list head pointer,
         *   use it; otherwise, use our internal fixup list 
         */
        if (fixup_list_head != 0)
        {
            /* use the external fixup list */
            fixup_list_head_ = fixup_list_head;

            /* remember the owning symbol */
            fixup_info_.fixup_owner_sym_ = fixup_owner_sym;
        }
        else
        {
            /* use our internal fixup list */
            fixup_list_head_ = &fixup_info_.internal_fixup_head_;

            /* we have no fixup items in our internal list yet */
            fixup_info_.internal_fixup_head_ = 0;
        }

        /* remember the stream offset */
        ofs_ = stream_ofs;

        /* our pool address is not yet known */
        addr_ = 0;

        /* we're not yet replaced */
        replaced_ = FALSE;
    }

    /* 
     *   Detach from our symbol, switching to our internal fixup list.  We
     *   leave any fixups that were previously associated with our symbol
     *   with the symbol, so this detaches us from any fixups currently
     *   pointing to me.  This is useful for replacing and modifying symbols,
     *   since it allows the anchor for the original definition to be
     *   dissociated from the symbol, so that the symbol can be reused with a
     *   different anchor.  
     */
    void detach_from_symbol()
    {
        /* switch to our internal fixup list */
        fixup_list_head_ = &fixup_info_.internal_fixup_head_;

        /* we have nothing in our list yet */
        fixup_info_.internal_fixup_head_ = 0;
    }

    /* get my offset - this is the stream offset, not the final address */
    ulong get_ofs() const { return ofs_; }

    /* 
     *   Get my final address - valid only after the linking phase (as
     *   defined and performed by the target-specific code generator).
     *   The exact meaning of this address is defined by the
     *   target-specific code generator, and this value is meant for use
     *   exclusively by the target code generator; the target-independent
     *   part of the compiler should have no use for this value.  
     */
    ulong get_addr() const { return addr_; }

    /* 
     *   Set the address - this should be invoked by the target-specific
     *   code generator during the link phase when the final address of
     *   the stream object is known.  This will store the address in the
     *   anchor, and will apply all of the outstanding fixups for the
     *   object.  
     */
    void set_addr(ulong addr);
    
    /* 
     *   Get the length.  We don't separately track the length, because
     *   the length can be deduced from the offset of the next item (or
     *   from the length of the stream, when the item is the last item in
     *   the stream).  
     */
    ulong get_len(class CTcDataStream *ds) const;

    /*
     *   Get the symbol table entry that owns my fixup list, if the fixup
     *   list is external.  Returns null if the fixup list is internal. 
     */
    class CTcSymbol *get_fixup_owner_sym() const
    {
        return (fixup_list_head_ == &fixup_info_.internal_fixup_head_
                ? 0 : fixup_info_.fixup_owner_sym_);
    }

    /* get/set the 'replaced' flag */
    int is_replaced() const { return replaced_; }
    void set_replaced(int f) { replaced_ = f; }

    /* next anchor in same list */
    CTcStreamAnchor *nxt_;

    /* 
     *   Pointer to fixup list head for the object - this is a list of the
     *   fixups for references to this object.  Because an object might
     *   have a fixup list before the object is added to the data stream
     *   (anything that is reachable through multiple references, such as
     *   a function in a symbol table, could have such a fixup list),
     *   we'll set this to refer to the external fixup list.  Otherwise,
     *   we'll set it to point to our own internal fixup list head.  
     */
    struct CTcAbsFixup **fixup_list_head_;

    /* 
     *   If we have an external fixup list, we need to store the symbol
     *   table entry that owns the fixup list; otherwise, we must store
     *   the fixup list head.  Since we only need one or the other,
     *   compress the storage into a union. 
     */
    union
    {
        /* 
         *   internal fixup list - this is used if no external fixup list
         *   exists for the object 
         */
        struct CTcAbsFixup *internal_fixup_head_;

        /* owning symbol table entry, if the fixup list is external */
        class CTcSymbol *fixup_owner_sym_;
    } fixup_info_;

    /* offset of this object in the stream */
    ulong ofs_;

    /* 
     *   final absolute address - this won't be known until the layout of
     *   the pool or data segment containing this object is calculated,
     *   which happens during the link phase 
     */
    ulong addr_;

    /* 
     *   Flag: this code block has been replaced by another one.  This is
     *   set when the function or property that owns this code is replaced
     *   by another implementation; when this is set, the code block
     *   should not be written to the image file. 
     */
    unsigned int replaced_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Dynamic compiler interface to the live VM.  The compiler uses this to
 *   access VM resources required during compilation.  
 */
class CTcVMIfc
{
public:
    virtual ~CTcVMIfc() { }

    /* 
     *   Add a generated object.  This creates a live object in the VM and
     *   returns its ID.  
     */
    virtual tctarg_obj_id_t new_obj(tctarg_obj_id_t cls) = 0;

    /* validate an object ID */
    virtual int validate_obj(tctarg_obj_id_t obj) = 0;
    
    /*
     *   Add a property value to a generated object. 
     */
    virtual void set_prop(tctarg_obj_id_t obj, tctarg_prop_id_t prop,
                          const class CTcConstVal *val) = 0;

    /* validate a property ID */
    virtual int validate_prop(tctarg_prop_id_t prop) = 0;

    /* validate a built-in function pointer */
    virtual int validate_bif(uint set_index, uint func_index) = 0;

    /* validate a constant pool address for a string/list */
    virtual int validate_pool_str(uint32_t pool_ofs) = 0;
    virtual int validate_pool_list(uint32_t pool_ofs) = 0;
};

#endif /* TCGEN_H */

