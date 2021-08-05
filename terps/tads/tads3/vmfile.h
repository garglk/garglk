/* $Header: d:/cvsroot/tads/tads3/VMFILE.H,v 1.2 1999/05/17 02:52:28 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmfile.h - VM external file interface
Function
  
Notes
  
Modified
  10/28/98 MJRoberts  - Creation
*/

#ifndef VMFILE_H
#define VMFILE_H

#include "t3std.h"
#include "vmerr.h"
#include "osifcnet.h"


/* ------------------------------------------------------------------------ */
/*
 *   VM external file interface.  The VM uses this interface for
 *   manipulating files that contain program images and saved state. 
 */

class CVmFile
{
public:
    /* create a new file object */
    CVmFile()
    {
        /* no file yet */
        fp_ = 0;

        /* presume the base seek position is at the start of the file */
        seek_base_ = 0;
    }

    CVmFile(osfildef *fp, long seek_base)
    {
        fp_ = fp;
        seek_base_ = seek_base;
    }

    /* 
     *   Duplicate the file handle, a la stdio freopen().  'mode' is a
     *   simplified fopen()-style mode string, with the same syntax as used
     *   in osfdup(). 
     */
    CVmFile *dup(const char *mode)
    {
        /* duplicate our file handle */
        osfildef *fpdup = osfdup(fp_, mode);
        if (fpdup == 0)
            return 0;

        /* create a new CVmFile wrapper for the new handle */
        return new CVmFile(fpdup, seek_base_);
    }

    /* delete the file */
    ~CVmFile();

    /*
     *   Set the file to use an existing underlying OS file handle.  The
     *   seek_base value gives the byte offset within the OS file handle
     *   of our virtual byte stream; this is useful if the file object is
     *   handling a byte stream embedded within a larger OS file (such as
     *   a resource within a file containing multiple resources).  If
     *   we're simply providing access to the entire underlying file, the
     *   seek_base value should be zero.  
     */
    void set_file(osfildef *fp, long seek_base)
    {
        /* remember the file handle and seek position */
        fp_ = fp;
        seek_base_ = seek_base;
    }

    /*
     *   Detach from the underlying OS file handle.  Normally, when we are
     *   deleted, we'll close our underlying OS file handle.  If the
     *   caller has a separate reference to the OS file handle and wants
     *   to keep the handle open after deleting us, the caller should use
     *   this function to detach us from the OS file handle before
     *   deleting us.
     *   
     *   After calling this routine, no operations can be performed on the
     *   underlying file through this object, since we will have forgotten
     *   the file handle.  
     */
    void detach_file()
    {
        /* forget our file handle */
        fp_ = 0;
    }

    /* 
     *   Open an existing file for reading.  Throws an error if the file
     *   does not exist.  
     */
    void open_read(const char *fname, os_filetype_t typ);

    /* 
     *   Create a file for writing, replacing any existing file.  Throws
     *   an error if the file cannot be created. 
     */
    void open_write(const char *fname, os_filetype_t typ);

    /* close the underlying file */
    void close()
    {
        osfcls(fp_);
        fp_ = 0;
    }

    /* flush buffers */
    void flush()
    {
        if (osfflush(fp_))
            err_throw(VMERR_WRITE_FILE);
    }

    /*
     *   Read various types from the file.  These routines throw an error
     *   if the data cannot be read. 
     */
    uchar read_byte() { char b; read_bytes(&b, 1); return (uchar)b; }
    uint read_uint2() { char b[2]; read_bytes(b, 2); return osrp2(b); }
    int  read_int2()  { char b[2]; read_bytes(b, 2); return osrp2s(b); }
    long read_int4()  { char b[4]; read_bytes(b, 4); return osrp4s(b); }
    ulong read_uint4() { char b[4]; read_bytes(b, 4); return t3rp4u(b); }

    /* read bytes - throws an error if all of the bytes cannot be read */
    void read_bytes(char *buf, size_t buflen)
    {
        if (buflen != 0 && osfrb(fp_, buf, buflen))
            err_throw(VMERR_READ_FILE);
    }

    /* 
     *   Read bytes, with no error on partial read.  If the full request
     *   can't be satisfied (because of end of file, partial read on pipe or
     *   socket, etc), reads as many as are available and returns the count.
     *   If no bytes are available, returns zero.  This routine can still
     *   throw an error if some other error occurs besides end-of-file (file
     *   not open, etc).  
     */
    size_t read_nbytes(char *buf, size_t buflen)
    {
        return buflen == 0 ? 0 : osfrbc(fp_, buf, buflen);
    }

    /* read a line (fgets semantics) */
    char *read_line(char *buf, size_t buflen)
        { return osfgets(buf, buflen, fp_); }

    /* 
     *   read a string with a one-byte length prefix, adding a null
     *   terminator; returns the length 
     */
    size_t read_str_byte_prefix(char *buf, size_t buflen)
    {
        size_t len = read_byte();
        if (len+1 > buflen)
            err_throw(VMERR_READ_FILE);

        read_bytes(buf, len);
        buf[len] = '\0';
        return len;
    }

    /* 
     *   read a string with a two-byte length prefix, adding a null
     *   terminator; returns the length 
     */
    size_t read_str_short_prefix(char *buf, size_t buflen)
    {
        size_t len = read_uint2();
        if (len+1 > buflen)
            err_throw(VMERR_READ_FILE);

        read_bytes(buf, len);
        buf[len] = '\0';
        return len;
    }

    /* 
     *   Read a string with a two-byte length prefix, allocating the buffer
     *   with lib_alloc_str() (the caller frees it with lib_free_str()).
     *   We'll null-terminate the result, and optionally return the length in
     *   '*lenp'.  'lenp' call be null if the length isn't needed.
     */
    char *read_str_short_prefix(size_t *lenp)
    {
        size_t len = read_uint2();
        char *buf = lib_alloc_str(len);
        read_bytes(buf, len);
        buf[len] = '\0';

        if (lenp != 0)
            *lenp = len;

        return buf;
    }

    /*
     *   Write various types to the file.  These routines throw an error
     *   if the data cannot be written. 
     */
    void write_byte(char c) { char b[1]; b[0] = c; write_bytes(b, 1); }
    void write_int2(int v) { char b[2]; oswp2s(b, v); write_bytes(b, 2); }
    void write_uint2(uint v) { char b[2]; oswp2(b, v); write_bytes(b, 2); }
    void write_int4(int v) { char b[4]; oswp4s(b, v); write_bytes(b, 4); }
    void write_uint4(uint v) { char b[4]; oswp4(b, v); write_bytes(b, 4); }

    /*
     *   Write a string with a single-byte length prefix.  Throws an error if
     *   the string is too long. 
     */
    void write_str_byte_prefix(const char *str, size_t len)
    {
        /* make sure it fits */
        if (len > 255)
            err_throw(VMERR_WRITE_FILE);

        /* write the prefix and the string */
        write_byte((char)len);
        write_bytes(str, len);
    }
    void write_str_byte_prefix(const char *str)
        { write_str_byte_prefix(str, strlen(str)); }

    /* write a string with a two-byte length prefix */
    void write_str_short_prefix(const char *str, size_t len)
    {
        /* make sure it fits */
        if (len > 65535)
            err_throw(VMERR_WRITE_FILE);

        /* write the prefix and string */
        write_uint2((int)len);
        write_bytes(str, len);
    }
    void write_str_short_prefix(const char *str)
        { write_str_short_prefix(str, strlen(str)); }

    /* write bytes - throws an error if the bytes cannot be written */
    void write_bytes(const char *buf, size_t buflen)
    {
        if (buflen != 0 && osfwb(fp_, buf, buflen))
            err_throw(VMERR_WRITE_FILE);
    }

    /* get the current seek position */
    long get_pos() const { return osfpos(fp_) - seek_base_; }

    /* seek to a new position */
    void set_pos(long seekpos)
    {
        /* seek relative to the base seek position */
        osfseek(fp_, seekpos + seek_base_, OSFSK_SET);
    }

    /* seek to a position relative to the end of the file */
    void set_pos_from_eof(long pos) { osfseek(fp_, pos, OSFSK_END); }

    /* seek to a position relative to the current file position */
    void set_pos_from_cur(long pos) { osfseek(fp_, pos, OSFSK_CUR); }

protected:
    /* our underlying OS file handle */
    osfildef *fp_;

    /* 
     *   Base seek position - this is useful for virtual byte streams that
     *   are embedded in larger files (resource files, for example).
     *   set_pos() is always relative to this position.  Note that
     *   set_pos_from_eof() is *not* relative to this position, so
     *   embedded byte streams must not use set_pos_from_eof() unless the
     *   embedded stream ends at the end of the enclosing file. 
     */
    long seek_base_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Generic stream interface.  This is a higher-level type of file interface
 *   than CVmFile: this interface can be implemented on different kinds of
 *   underlying storage formats to provide generic stream access independent
 *   of the actual storage implementation.
 *   
 *   We provide default implementations of the type readers in terms of the
 *   low-level byte reader virtual method, which must be implemented by each
 *   concrete subclass.  However, all of the type readers are virtual, so
 *   subclasses can override these if they can be implemented more
 *   efficiently on the underlying stream (for example, some implementations
 *   might have ready access to the data in a buffer already, in which case
 *   it might be faster to avoid the extra buffer copy of the default
 *   implementations of the type readers).  
 */
class CVmStream
{
public:
    virtual ~CVmStream() { }

    /* clone the stream - see CVmDataSource::clone() for details */
    virtual CVmStream *clone(VMG_ const char *mode) = 0;

    /* read/write a byte */
    virtual uchar read_byte() { char b; read_bytes(&b, 1); return (uchar)b; }
    virtual void write_byte(uchar b) { write_bytes((char *)&b, 1); }

    /* 
     *   read various integer types (signed 2-byte, unsigned 2-byte, signed
     *   4-byte, unsigned 4-byte) 
     */
    virtual int read_int2() { char b[2]; read_bytes(b, 2); return osrp2s(b); }
    virtual uint read_uint2()
        { char b[2]; read_bytes(b, 2); return osrp2(b); }
    virtual long read_int4() { char b[4]; read_bytes(b, 4); return osrp4s(b); }
    virtual ulong read_uint4()
        { char b[4]; read_bytes(b, 4); return t3rp4u(b); }

    /* write an integer value */
    virtual void write_byte(char c)
        { char b[1]; b[0] = c; write_bytes(b, 1); }
    virtual void write_int2(int i)
        { char b[2]; oswp2s(b, i); write_bytes(b, 2); }
    virtual void write_uint2(uint i)
        { char b[2]; oswp2(b, i); write_bytes(b, 2); }
    virtual void write_int4(long l)
        { char b[4]; oswp4s(b, l); write_bytes(b, 4); }
    virtual void write_uint4(ulong l)
        { char b[4]; oswp4(b, l); write_bytes(b, 4); }

    /* 
     *   Read bytes - this must be provided by the implementation.  Throws an
     *   error if the full number of bytes requested cannot be read. 
     */
    virtual void read_bytes(char *buf, size_t len) = 0;

    /* 
     *   Read bytes - reads up to the amount requested, but treats a partial
     *   read (due to end of file, partial read on a pipe or socket, etc) as
     *   a success.  Returns the actual number of bytes read, which may be
     *   less than the requested number, and can even be zero if the file is
     *   already at EOF.  
     */
    virtual size_t read_nbytes(char *buf, size_t len) = 0;

    /* read a line - fgets semantics */
    virtual char *read_line(char *buf, size_t len) = 0;

    /* write bytes */
    virtual void write_bytes(const char *buf, size_t len) = 0;

    /* get the current seek offset */
    virtual long get_seek_pos() const = 0;

    /* 
     *   set the seek offset - this is only valid with offsets previously
     *   obtained with get_seek_pos() 
     */
    virtual void set_seek_pos(long pos) = 0;

    /* get the length of the stream's contents */
    virtual long get_len() = 0;
};

/*
 *   Implementation of the generic stream with an underlying CVmFile object 
 */
class CVmFileStream: public CVmStream
{
public:
    /* create based on the underlying file object */
    CVmFileStream(CVmFile *fp) { fp_ = fp; }

    /* clone the stream */
    virtual CVmStream *clone(VMG_ const char *mode)
    {
        /* duplicate the file handle */
        CVmFile *fpdup = fp_->dup(mode);
        if (fpdup == 0)
            return 0;

        /* return a new file stream wrapper for the duplicate handle */
        return new CVmFileStream(fpdup);
    }

    /* read bytes */
    void read_bytes(char *buf, size_t len)
        { fp_->read_bytes(buf, len); }

    /* read bytes with success on partial read */
    size_t read_nbytes(char *buf, size_t len)
        { return fp_->read_nbytes(buf, len); }

    /* read a line */
    char *read_line(char *buf, size_t len)
        { return fp_->read_line(buf, len); }

    /* write bytes */
    void write_bytes(const char *buf, size_t len)
        { fp_->write_bytes(buf, len); }

    /* get/set the seek position */
    long get_seek_pos() const { return fp_->get_pos(); }
    void set_seek_pos(long pos) { fp_->set_pos(pos); }

    /* get the length of the stream */
    long get_len()
    {
        /* remember the current seek position */
        long orig = fp_->get_pos();
        
        /* seek to the end of the file */
        fp_->set_pos_from_eof(0);

        /* the seek position is the file length */
        long len = fp_->get_pos();

        /* seek back to where we started */
        fp_->set_pos(orig);

        /* return the stream length */
        return len;
    }

private:
    /* our underlying file stream */
    CVmFile *fp_;
};

/*
 *   Reference-counted long int value (service class for CVmCountingStream) 
 */
class CVmRefCntLong: public CVmRefCntObj
{
public:
    CVmRefCntLong() { val = 0; }

    long val;
};

/*
 *   Counting stream; write-only.  This is a stream for measuring the size of
 *   data for writing.  We don't actually write anything; we just count the
 *   bytes written.  
 */
class CVmCountingStream: public CVmStream
{
public:
    CVmCountingStream()
    {
        pos_ = 0;
        len_ = new CVmRefCntLong();
    }
    
    CVmCountingStream(CVmRefCntLong *l)
    {
        l->add_ref();
        len_ = l;
    }

    ~CVmCountingStream() { len_->release_ref(); }
    
    CVmStream *clone(VMG_ const char * /*mode*/)
        { return new CVmCountingStream(len_); }

    void read_bytes(char *buf, size_t len) { err_throw(VMERR_READ_FILE); }
    size_t read_nbytes(char *buf, size_t len)
    {
        err_throw(VMERR_READ_FILE);
        AFTER_ERR_THROW(return 0);
    }
    char *read_line(char *buf, size_t len)
    {
        err_throw(VMERR_READ_FILE);
        AFTER_ERR_THROW(return 0);
    }

    void write_bytes(const char *buf, size_t len)
    {
        pos_ += len;
        if (pos_ > len_->val)
            len_->val = pos_;
    }

    long get_seek_pos() const { return pos_; }
    void set_seek_pos(long pos)
        { pos_ = (pos < 0 ? 0 : pos > len_->val ? len_->val : pos); }

    long get_len() { return len_->val; }

protected:
    /* current seek position */
    long pos_;

    /* number of bytes written */
    CVmRefCntLong *len_;
};

/*
 *   Implementation of the generic stream reader for reading or writing a
 *   memory buffer.  This uses a fixed, pre-allocated buffer provided by the
 *   caller, so writing is limited to the space in the buffer.  (For a more
 *   flexible memory stream writer that allocates space as needed, use
 *   CVmExpandableMemoryStream.)  
 */
class CVmMemoryStream: public CVmStream
{
public:
    /* create given the underlying memory buffer */
    CVmMemoryStream(char *buf, size_t len)
    {
        /* remember the buffer */
        buf_ = buf;
        buflen_ = len;

        /* start at the start of the buffer */
        p_ = buf;
    }

    /* clone */
    virtual CVmStream *clone(VMG_ const char * /*mode*/)
        { return new CVmMemoryStream(buf_, buflen_); }

    /* read bytes */
    void read_bytes(char *buf, size_t len)
    {
        /* if we don't have enough bytes left, throw an error */
        if (len > (size_t)((buf_ + buflen_) - p_))
            err_throw(VMERR_READ_FILE);

        /* copy the data */
        memcpy(buf, p_, len);

        /* advance past the data we just read */
        p_ += len;
    }

    /* read bytes, partial read OK */
    size_t read_nbytes(char *buf, size_t len)
    {
        /* limit the request to the number of bytes available */
        long avail = (buf_ + buflen_) - p_;
        if ((long)len > avail)
            len = (size_t)avail;

        /* copy the data */
        if (len != 0)
            memcpy(buf, p_, len);

        /* advance past the data we just read */
        p_ += len;

        /* return the number of bytes read */
        return len;
    }

    /* read a line */
    char *read_line(char *buf, size_t len)
    {
        /* if already at EOF, or there's no buffer, return null */
        if (p_ >= buf_ + buflen_ || buf == 0 || len == 0)
            return 0;

        /* read bytes until we fill the buffer or reach '\n' or EOF */
        char *dst;
        size_t rem;
        for (dst = buf, rem = len ; rem > 1 && p_ < buf_ + buflen_ ; --rem)
        {
            /* get this character */
            char c = *p_++;

            /* check for newlines */
            if (c == '\n' || c == '\r')
            {
                /* write CR and LF as simply CR */
                *dst++ = '\n';

                /* if we have a CR-LF or LF-CR sequence, skip the pair */
                if (p_ < buf_ + buflen_
                    && ((c == '\n' && *p_ == '\r')
                        || (c == '\r' && *p_ == '\n')))
                    p_ += 1;

                /* done */
                break;
            }

            /* copy this character */
            *dst++ = c;
        }

        /* add the trailing null and return the buffer */
        *dst = '\0';
        return buf;
    }

    /* write bytes */
    void write_bytes(const char *buf, size_t len)
    {
        /* if we don't have space, throw an error */
        if (len > (size_t)((buf_ + buflen_) - p_))
            err_throw(VMERR_WRITE_FILE);

        /* copy the data */
        memcpy(p_, buf, len);

        /* advance past the data */
        p_ += len;
    }

    /* get the position - return an offset from the start of our buffer */
    long get_seek_pos() const { return p_ - buf_; }

    /* set the position, as an offset from the start of our buffer */
    void set_seek_pos(long pos)
    {
        /* limit it to the available space */
        if (pos < 0)
            pos = 0;
        else if (pos > (long)buflen_)
            pos = buflen_;

        /* set the position */
        p_ = buf_ + pos;
    }

    /* get the stream length */
    long get_len() { return buflen_; }

    /* set the stream length */
    void set_len(size_t len) { buflen_ = len; }

protected:
    /* our buffer */
    char *buf_;

    /* size of our buffer */
    size_t buflen_;

    /* current read/write pointer */
    char *p_;
};

/*
 *   reference-counted buffer 
 */
class CVmRefCntBuf: public CVmRefCntObj
{
public:
    CVmRefCntBuf(size_t len)
        { buf_ = lib_alloc_str(len); }

    CVmRefCntBuf(const char *str, size_t len)
        { buf_ = lib_copy_str(str, len); }

    char *buf_;

protected:
    ~CVmRefCntBuf() { lib_free_str(buf_); }
};

/*
 *   Memory stream, using a private allocated buffer
 */
class CVmPrivateMemoryStream: public CVmMemoryStream
{
public:
    CVmPrivateMemoryStream(size_t len)
        : CVmMemoryStream(0, len)
    {
        bufobj_ = new CVmRefCntBuf(len);
        buf_ = bufobj_->buf_;
    }

    CVmPrivateMemoryStream(const char *str, size_t len)
        : CVmMemoryStream(0, len)
    {
        bufobj_ = new CVmRefCntBuf(str, len);
        buf_ = bufobj_->buf_;
    }

    CVmPrivateMemoryStream(CVmRefCntBuf *bufobj, size_t len)
        : CVmMemoryStream(bufobj->buf_, len)
    {
        bufobj->add_ref();
        bufobj_ = bufobj;
    }

    CVmStream *clone(VMG_ const char * /*mode*/)
        { return new CVmPrivateMemoryStream(bufobj_, buflen_); }

    ~CVmPrivateMemoryStream() { bufobj_->release_ref(); }

protected:
    CVmRefCntBuf *bufobj_;
};

/*
 *   Read-only memory stream 
 */
class CVmReadOnlyMemoryStream: public CVmMemoryStream
{
public:
    CVmReadOnlyMemoryStream(const char *buf, size_t len)
        : CVmMemoryStream((char *)buf, len)
    {
    }

    /* disallow writing */
    void write_bytes(const char *, size_t) { err_throw(VMERR_WRITE_FILE); }
};

/* expandable memory stream block */
struct CVmExpandableMemoryStreamBlock
{
    CVmExpandableMemoryStreamBlock(long ofs)
    {
        this->ofs = ofs;
        nxt = 0;
    }
    
    /* block length */
    static const int BlockLen = 4096;

    /* next block in the list */
    CVmExpandableMemoryStreamBlock *nxt;

    /* byte offset in the overall stream of the start of this block */
    long ofs;

    /* block buffer */
    char buf[BlockLen];
};

/* expandable memory stream block list */
class CVmExpandableMemoryStreamList: public CVmRefCntObj
{
public:
    static const int BlockLen = CVmExpandableMemoryStreamBlock::BlockLen;

    CVmExpandableMemoryStreamList(long init_len)
    {
        /* no blocks yet */
        first_block_ = last_block_ = 0;

        /* always allocate at least the initial block */
        add_block();
        init_len -= BlockLen;

        /* set the seek position to the start of the first block */
        cur_block_ = first_block_;
        cur_block_ofs_ = 0;

        /* add more blocks until we've satisfied the length request */
        for ( ; init_len > 0 ; init_len -= BlockLen)
            add_block();

        /* the file is currently empty */
        len_ = 0;
    }

    /* read bytes */
    void read_bytes(char *buf, size_t len)
    {
        /* 
         *   if the request would take us past the current content length,
         *   it's an error 
         */
        if (cur_block_->ofs + cur_block_ofs_ + (long)len > len_)
            err_throw(VMERR_READ_FILE);

        /* keep going until we satisfy the request */
        while (len != 0)
        {
            /* figure how much we can read from the current block */
            size_t rem = BlockLen - cur_block_ofs_;
            size_t cur = (len < rem ? len : rem);

            /* read that much */
            if (cur != 0)
            {
                memcpy(buf, cur_block_->buf + cur_block_ofs_, cur);
                cur_block_ofs_ += cur;
                len -= cur;
                buf += cur;
            }

            /* if there's more to read, advance to the next block */
            if (len != 0)
            {
                /* if this is the last block, we can't satisfy the request */
                if (cur_block_->nxt == 0)
                    err_throw(VMERR_READ_FILE);

                /* advance to the start of the next block */
                cur_block_ = cur_block_->nxt;
                cur_block_ofs_ = 0;
            }
        }
    }

    /* read bytes, partial read OK */
    size_t read_nbytes(char *buf, size_t len)
    {
        /* figure the number of bytes available */
        long avail = (long)len_ - (cur_block_->ofs + cur_block_ofs_);

        /* limit the read to the available bytes */
        if ((long)len > avail)
            len = (size_t)avail;

        /* if that leaves us with any bytes to read, do the copy */
        if (len != 0)
            read_bytes(buf, len);

        /* return the number of bytes transfered */
        return len;
    }

    /* read a line */
    char *read_line(char *buf, size_t len)
    {
        /* if already at EOF, or there's no buffer, return null */
        if (get_seek_pos() >= len_)
            return 0;

        /* read bytes until we fill the buffer or reach '\n' or EOF */
        char *dst;
        size_t rem;
        for (dst = buf, rem = len ; rem > 1 ; --rem)
        {
            /* get this character - stop at EOF */
            char c;
            if (!mread_byte(&c, TRUE))
                break;

            /* check for newlines */
            if (c == '\n' || c == '\r')
            {
                /* write CR and LF as simply CR */
                *dst++ = '\n';

                /* if we have a CR-LF or LF-CR sequence, skip the pair */
                char c2;
                if (mread_byte(&c2, FALSE)
                    && ((c == '\n' && c2 == '\r')
                        || (c == '\r' && c2 == '\n')))
                {
                    /* skip the second character of the pair */
                    mread_byte(&c2, TRUE);
                }

                /* done */
                break;
            }

            /* copy this character */
            *dst++ = c;
        }

        /* add the trailing null and return the buffer */
        *dst = '\0';
        return buf;
    }

    /* write bytes */
    void write_bytes(const char *buf, size_t len)
    {
        /* keep going until we satisfy the request */
        while (len != 0)
        {
            /* figure how much we can write to the current block */
            size_t rem = BlockLen - cur_block_ofs_;
            size_t cur = (len < rem ? len : rem);

            /* write that much */
            if (cur != 0)
            {
                memcpy(cur_block_->buf + cur_block_ofs_, buf, cur);
                cur_block_ofs_ += cur;
                len -= cur;
                buf += cur;
            }

            /* if there's more to write, advance to the next block */
            if (len != 0)
            {
                /* if this is the last block, add a new one */
                if (cur_block_->nxt == 0)
                {
                    add_block();
                    if (cur_block_->nxt == 0)
                        err_throw(VMERR_WRITE_FILE);
                }

                /* advance to the next block */
                cur_block_ = cur_block_->nxt;
                cur_block_ofs_ = 0;
            }
        }

        /* 
         *   if the seek pointer is past the current content length, we've
         *   written past the old end of the file and thus expanded the file
         *   - note the new length 
         */
        if (cur_block_->ofs + cur_block_ofs_ > len_)
            len_ = cur_block_->ofs + cur_block_ofs_;
    }

    /* get the current seek offset */
    long get_seek_pos() const
    {
        /* 
         *   figure the seek position as the current block's base offset plus
         *   the current offset within that block 
         */
        return cur_block_->ofs + cur_block_ofs_;
    }

    /* set the seek offset */
    void set_seek_pos(long pos)
    {
        /* limit the position to the file's bounds */
        if (pos < 0)
        {
            /* at start of file */
            pos = 0;
        }
        else if (pos >= len_)
        {
            /* 
             *   At end of file.  This is a special case for setting the
             *   block pointer, because we could be positioned at the last
             *   byte of the last block, which we won't find in our scan.
             *   Explicitly set the position here.  
             */
            pos = len_;
            cur_block_ = last_block_;
            cur_block_ofs_ = pos - last_block_->ofs;

            /* we've set the block position, so skip the usual search */
            return;
        }

        /* find the block containing the seek offset */
        CVmExpandableMemoryStreamBlock *b;
        for (b = first_block_ ; b != 0 ; b = b->nxt)
        {
            /* if the offset is within this block, we're done */
            if (pos >= b->ofs && pos < b->ofs + BlockLen)
            {
                /* set the current pointers */
                cur_block_ = b;
                cur_block_ofs_ = pos - b->ofs;

                /* stop scanning */
                break;
            }
        }
    }

    /* get the length of the stream's contents */
    long get_len() { return len_; }

protected:
    ~CVmExpandableMemoryStreamList()
    {
        /* delete our block list */
        CVmExpandableMemoryStreamBlock *cur, *nxt;
        for (cur = first_block_ ; cur != 0 ; cur = nxt)
        {
            nxt = cur->nxt;
            delete cur;
        }
    }

    /* add a block to the end of the list */
    void add_block()
    {
        long ofs = last_block_ == 0 ? 0 : last_block_->ofs + BlockLen;
        CVmExpandableMemoryStreamBlock *b =
            new CVmExpandableMemoryStreamBlock(ofs);

        if (last_block_ != 0)
            last_block_->nxt = b;
        else
            first_block_ = b;
        last_block_ = b;
    }

    /* 
     *   read one byte, incrementing the read pointer if desired; returns
     *   true on success, false on EOF 
     */
    int mread_byte(char *c, int inc)
    {
        /* check for EOF */
        if (cur_block_->ofs + cur_block_ofs_ >= len_)
            return FALSE;

        /* if we're out of bytes in this block, move to the next block */
        if (cur_block_ofs_ >= BlockLen)
        {
            cur_block_ = cur_block_->nxt;
            cur_block_ofs_ = 0;
        }

        /* set the return byte */
        *c = cur_block_->buf[cur_block_ofs_];

        /* increment the pointer if desired */
        if (inc)
            cur_block_ofs_ += 1;

        /* success */
        return TRUE;
    }

    /* current seek block and offset */
    CVmExpandableMemoryStreamBlock *cur_block_;
    long cur_block_ofs_;

    /* total file length */
    long len_;

    /* head/tail of block list */
    CVmExpandableMemoryStreamBlock *first_block_;
    CVmExpandableMemoryStreamBlock *last_block_;
};

/*
 *   Expandable memory stream.  This automatically allocates additional
 *   buffer space as needed.  
 */

class CVmExpandableMemoryStream: public CVmStream
{
public:
    CVmExpandableMemoryStream(long init_len)
    {
        bl = new CVmExpandableMemoryStreamList(init_len);
    }

    virtual CVmStream *clone(VMG_ const char * /*mode*/)
        { return new CVmExpandableMemoryStream(bl); }

    ~CVmExpandableMemoryStream() { bl->release_ref(); }
   
    /* read bytes */
    virtual void read_bytes(char *buf, size_t len)
        { bl->read_bytes(buf, len); }

    /* read bytes, partial read OK */
    virtual size_t read_nbytes(char *buf, size_t len)
        { return bl->read_nbytes(buf, len); }

    /* read a line */
    virtual char *read_line(char *buf, size_t len)
        { return bl->read_line(buf, len); }

    /* write bytes */
    virtual void write_bytes(const char *buf, size_t len)
        { bl->write_bytes(buf, len); }

    /* get the current seek offset */
    virtual long get_seek_pos() const
        { return bl->get_seek_pos(); }

    /* set the seek offset */
    virtual void set_seek_pos(long pos)
        { bl->set_seek_pos(pos); }

    /* get the length of the stream's contents */
    virtual long get_len()
        { return bl->get_len(); }

protected:
    CVmExpandableMemoryStream(CVmExpandableMemoryStreamList *bl)
    {
        bl->add_ref();
        this->bl = bl;
    }

    CVmExpandableMemoryStreamList *bl;
};

#endif /* VMFILE_H */

