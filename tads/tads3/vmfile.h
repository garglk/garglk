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

    /*
     *   Read various types from the file.  These routines throw an error
     *   if the data cannot be read. 
     */
    uchar read_byte() { char b; read_bytes(&b, 1); return (uchar)b; }
    uint read_uint2() { char b[2]; read_bytes(b, 2); return osrp2(b); }
    int  read_int2()  { char b[2]; read_bytes(b, 2); return osrp2s(b); }
    long read_int4()  { char b[4]; read_bytes(b, 4); return osrp4(b); }
    ulong read_uint4() { char b[4]; read_bytes(b, 4); return t3rp4u(b); }

    /* read bytes - throws an error if the bytes cannot be read */
    void read_bytes(char *buf, size_t buflen)
    {
        if (buflen != 0 && osfrb(fp_, buf, buflen))
            err_throw(VMERR_READ_FILE);
    }

    /*
     *   Write various types to the file.  These routines throw an error
     *   if the data cannot be written. 
     */
    void write_int2(uint v) { char b[2]; oswp2(b, v); write_bytes(b, 2); }
    void write_int4(uint v) { char b[4]; oswp4(b, v); write_bytes(b, 4); }

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
    virtual long read_int4() { char b[4]; read_bytes(b, 4); return osrp4(b); }
    virtual ulong read_uint4()
        { char b[4]; read_bytes(b, 4); return t3rp4u(b); }

    /* write an integer value */
    virtual void write_int2(int i)
        { char b[2]; oswp2(b, i); write_bytes(b, 2); }
    virtual void write_int4(long l)
        { char b[4]; oswp4(b, l); write_bytes(b, 4); }

    /* read bytes - this must be provided by the implementation */
    virtual void read_bytes(char *buf, size_t len) = 0;

    /* write byte */
    virtual void write_bytes(const char *buf, size_t len) = 0;

    /* get the current seek offset */
    virtual long get_seek_pos() const = 0;

    /* 
     *   set the seek offset - this is only valid with offsets previously
     *   obtained with get_seek_pos() 
     */
    virtual void set_seek_pos(long pos) = 0;
};

/*
 *   Implementation of the generic stream with an underlying CVmFile object 
 */
class CVmFileStream: public CVmStream
{
public:
    /* create based on the underlying file object */
    CVmFileStream(CVmFile *fp) { fp_ = fp; }

    /* read bytes */
    void read_bytes(char *buf, size_t len) { fp_->read_bytes(buf, len); }

    /* write bytes */
    void write_bytes(const char *buf, size_t len)
        { fp_->write_bytes(buf, len); }

    /* get/set the seek position */
    long get_seek_pos() const { return fp_->get_pos(); }
    void set_seek_pos(long pos) { fp_->set_pos(pos); }

private:
    /* our underlying file stream */
    CVmFile *fp_;
};

/*
 *   Implementation of the generic stream reader for reading from memory 
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

protected:
    /* our buffer */
    char *buf_;

    /* size of our buffer */
    size_t buflen_;

    /* current read/write pointer */
    char *p_;
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

#endif /* VMFILE_H */

