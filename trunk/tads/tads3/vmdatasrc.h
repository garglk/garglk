/* $Header$ */

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmdatasrc.h - Data Source object
Function
  CVmDataSource is an abstract class representing a read/write data source,
  such as a file, block of memory, etc.  The data source object has an
  interface essentially identical to the osifc file interface, but can be
  implemented with a variety of underlying data sources.
Notes
  
Modified
  05/05/10 MJRoberts  - Creation
*/

#ifndef VMDATASRC_H
#define VMDATASRC_H

#include <string.h>


/* ------------------------------------------------------------------------ */
/*
 *   Abstract file interface.  This allows the File intrinsic class to
 *   present a common interface on multiple underlying data sources.  
 */
class CVmDataSource
{
public:
    virtual ~CVmDataSource() { }

    /* read bytes - returns 0 on success, non-zero on EOF or error */
    virtual int read(void *buf, size_t len) = 0;

    /* read bytes - returns the number of bytes read; 0 means EOF or error */
    virtual int readc(void *buf, size_t len) = 0;

    /* write bytes - returns 0 on success, non-zero on error */
    virtual int write(const void *buf, size_t len) = 0;

    /* write a null-terminated string */
    virtual int writez(const char *str) { return write(str, strlen(str)); }

    /* get the length of the file in bytes */
    virtual long get_size() = 0;

    /* get the current seek location */
    virtual long get_pos() = 0;

    /* set the current seek location - 'mode' is an OSFSK_xxx mode */
    virtual int seek(long ofs, int mode) = 0;

    /* flush - returns 0 on success, non-zero on error */
    virtual int flush() = 0;

    /* close the underlying system resource */
    virtual void close() = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   Stream source.  This layers a CVmDataSource on top of a CVmStream
 *   object.  We take ownership of the stream, deleting it when we're
 *   deleted.
 */
class CVmStreamSource: public CVmDataSource
{
public:
    CVmStreamSource(class CVmStream *s)
    {
        stream = s;
    }
    
    virtual ~CVmStreamSource();

    /* read bytes - returns 0 on success, non-zero on EOF or error */
    virtual int read(void *buf, size_t len);

    /* read bytes - returns the number of bytes read; 0 means EOF or error */
    virtual int readc(void *buf, size_t len);

    /* write bytes - returns 0 on success, non-zero on error */
    virtual int write(const void *buf, size_t len);

    /* get the length of the file in bytes */
    virtual long get_size();

    /* get the current seek location */
    virtual long get_pos();

    /* set the current seek location - 'mode' is an OSFSK_xxx mode */
    virtual int seek(long ofs, int mode);

    /* flush - returns 0 on success, non-zero on error */
    virtual int flush();

    /* close the underlying system resource */
    virtual void close();

protected:
    /* our underlying stream object */
    class CVmStream *stream;
};

/* ------------------------------------------------------------------------ */
/*
 *   Basic OS file data source.
 */
class CVmFileSource: public CVmDataSource
{
public:
    CVmFileSource(osfildef *fp) { this->fp = fp; }
    ~CVmFileSource()
    {
        if (fp != 0)
            close();
    }

    /* read bytes - returns 0 on success, non-zero on EOF or error */
    virtual int read(void *buf, size_t len) { return osfrb(fp, buf, len); }

    /* read bytes - returns 0 on success, non-zero on EOF or error */
    virtual int readc(void *buf, size_t len) { return osfrbc(fp, buf, len); }

    /* write bytes - returns 0 on success, non-zero on error*/
    virtual int write(const void *buf, size_t len)
        { return osfwb(fp, buf, len); }

    /* get the length of the file in bytes */
    virtual long get_size()
    {
        /* remember the current seek location */
        long oldpos = osfpos(fp);

        /* seek to the end */
        osfseek(fp, 0, OSFSK_END);

        /* the current position is the file size */
        long siz = osfpos(fp);

        /* seek back to where we started */
        osfseek(fp, oldpos, OSFSK_SET);

        /* return the size */
        return siz;
    }

    /* get the current seek location */
    virtual long get_pos() { return osfpos(fp); }

    /* set the current seek location - 'mode' is an OSFSK_xxx mode */
    virtual int seek(long ofs, int mode) { return osfseek(fp, ofs, mode); }

    /* flush - returns 0 on success, non-zero on error */
    virtual int flush() { return osfflush(fp); }

    /* close the underlying system resource */
    virtual void close()
    {
        osfcls(fp);
        fp = 0;
    }

protected:
    /* the underlying file */
    osfildef *fp;
};


/* ------------------------------------------------------------------------ */
/*
 *   Resource file data source.  A resource file is a contiguous byte range
 *   within a larger file.  
 */
class CVmResFileSource: public CVmDataSource
{
public:
    CVmResFileSource(osfildef *fp, unsigned long start, long end)
    {
        this->fp = fp;
        this->start = start;
        this->end = end;
    }
    ~CVmResFileSource()
    {
        if (fp != 0)
            close();
    }

    /* read bytes - returns 0 on success, non-zero on EOF or error */
    virtual int read(void *buf, size_t len)
    {
        /* if this would take us past the end, we can't fulfill the request */
        long pos = osfpos(fp);
        if (pos + len > end)
            return 1;

        /* do the read */
        return osfrb(fp, buf, len);
    }

    /* read bytes - returns 0 on success, non-zero on EOF or error */
    virtual int readc(void *buf, size_t len)
    {
        /* if we're already past the end, we can't read anything */
        unsigned long pos = osfpos(fp);
        if (pos >= end)
            return 0;

        /* limit the read to the available file size */
        unsigned long limit = end - pos;
        if (len > limit)
            len = (size_t)limit;

        /* do the read */
        return osfrbc(fp, buf, len);
    }

    /* write bytes - resource files are read-only */
    virtual int write(const void *buf, size_t len) { return 1; }

    /* get the length of the file in bytes */
    virtual long get_size() { return end - start; }

    /* get the current seek location */
    virtual long get_pos() { return osfpos(fp) - start; }

    /* set the current seek location - 'mode' is an OSFSK_xxx mode */
    virtual int seek(long ofs, int mode)
    {
        switch (mode)
        {
        case OSFSK_SET:
        do_set:
            /* check that 'ofs' is in range */
            if (ofs < 0 || (unsigned long)ofs > end)
                return 1;

            /* seek to the offset */
            return osfseek(fp, start + ofs, OSFSK_SET);

        case OSFSK_CUR:
            /* set the offset relative to the current position */
            ofs += get_pos();
            goto do_set;

        case OSFSK_END:
            /* set the offset relative to the end of the file */
            ofs += (end - start);
            goto do_set;

        default:
            /* invalid mode */
            return 1;
        }
    }

    /* flush - we're read-only, so this just returns success */
    virtual int flush() { return 0; }

    /* close the underlying system resource */
    virtual void close()
    {
        osfcls(fp);
        fp = 0;
    }

protected:
    /* the underlying file */
    osfildef *fp;

    /* the byte range within the resource file */
    unsigned long start, end;
};


/* ------------------------------------------------------------------------ */
/*
 *   Read-only string data source.  This takes a buffer in memory and
 *   presents a read-only file interface to it.  
 */
class CVmStringSource: public CVmDataSource
{
public:
    CVmStringSource(const char *mem, long len)
    {
        this->mem = mem;
        this->memlen = len;
        this->pos = 0;
    }
    virtual ~CVmStringSource() { }

    /* read bytes - returns 0 on success, non-zero on EOF or error */
    virtual int read(void *buf, size_t len)
    {
        /* make sure there's enough data to satisfy the request */
        if (pos + (long)len > memlen)
            return 1;

        /* copy the data */
        memcpy(buf, mem + pos, len);

        /* advance the read pointer */
        pos += len;

        /* success */
        return 0;
    }

    /* read bytes - returns the number of bytes read; 0 means EOF or error */
    virtual int readc(void *buf, size_t len)
    {
        /* limit the read to the available length */
        long rem = memlen - pos;
        if ((long)len > rem)
            len = rem;

        /* copy the bytes */
        memcpy(buf, mem + pos, len);

        /* advance the read pointer */
        pos += len;
        
        /* return the amount read */
        return (int)len;
    }

    /* write bytes - we're read-only, so this is an error */
    virtual int write(const void *buf, size_t len) { return 1; }

    /* get the length of the file in bytes */
    virtual long get_size() { return memlen; }

    /* get the current seek location */
    virtual long get_pos() { return pos; }

    /* set the current seek location - 'mode' is an OSFSK_xxx mode */
    virtual int seek(long ofs, int mode)
    {
        switch (mode)
        {
        case OSFSK_SET:
        do_set:
            /* make sure it's in range */
            if (ofs < 0 || ofs > memlen)
                return 1;

            /* set the new position */
            pos = ofs;

            /* success */
            return 0;

        case OSFSK_CUR:
            /* calculate the absolute position and go do the set */
            ofs += pos;
            goto do_set;

        case OSFSK_END:
            /* calculate the absolute position and go do the set */
            ofs += memlen;
            goto do_set;

        default:
            /* invalid mode */
            return 1;
        }
    }

    /* flush - we're read-only, so this is a no-op; just return success */
    virtual int flush() { return 0; }

    /* close the underlying system resource; there's nothing for us to do */
    virtual void close() { }

protected:
    /* our memory buffer, and the number of bytes in the buffer */
    const char *mem;
    long memlen;

    /* current read position  */
    long pos;
};

/* ------------------------------------------------------------------------ */
/*
 *   Memory data source.  This automatically allocates additional buffer
 *   space as needed.  
 */

/* page block for a memory source */
struct CVmMemorySourceBlock
{
    CVmMemorySourceBlock(long ofs)
    {
        this->ofs = ofs;
        nxt = 0;
    }

    /* block length */
    static const int BlockLen = 4096;

    /* next block in the list */
    CVmMemorySourceBlock *nxt;

    /* byte offset in the overall stream of the start of this block */
    long ofs;

    /* block buffer */
    char buf[BlockLen];
};

class CVmMemorySource: public CVmDataSource
{
public:
    static const int BlockLen = CVmMemorySourceBlock::BlockLen;
    
    CVmMemorySource(long init_len)
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
    
    ~CVmMemorySource()
    {
        /* delete our block list */
        CVmMemorySourceBlock *cur, *nxt;
        for (cur = first_block_ ; cur != 0 ; cur = nxt)
        {
            nxt = cur->nxt;
            delete cur;
        }
    }
    
    /* read bytes */
    virtual int read(void *buf, size_t len)
    {
        /* 
         *   if the request would take us past the current content length,
         *   fail immediately, since we know we can't satisfy the request 
         */
        if (cur_block_->ofs + cur_block_ofs_ + (long)len > len_)
            return 1;

        /* 
         *   do the read; return success == 0 if the amount read exactly
         *   matches the requested length 
         */
        return (size_t)readc(buf, len) != len;
    }

    /* read bytes */
    virtual int readc(void *buf0, size_t len)
    {
        /* get the buffer pointer as a character pointer */
        char *buf = (char *)buf0;

        /* we haven't ready anything yet */
        size_t bytes_read = 0;

        /* limit the length to the remaining bytes in the object */
        long rem = len_ - get_pos();
        if ((long)len > rem)
            len = (size_t)rem;

        /* keep going until we satisfy the request */
        while (len != 0)
        {
            /* figure how much we can read from the current block */
            size_t rem = BlockLen - cur_block_ofs_;
            size_t cur = (len < rem ? len : rem);

            /* read that much */
            if (cur != 0)
            {
                /* copy bytes to the output buffer */
                memcpy(buf, cur_block_->buf + cur_block_ofs_, cur);

                /* move past the bytes in the current block */
                cur_block_ofs_ += cur;

                /* adjust our request counters past this chunk */
                len -= cur;
                buf += cur;

                /* count it in the total read so far */
                bytes_read += cur;
            }

            /* if this is the last block, we're done */
            if (cur_block_->nxt == 0)
                return bytes_read;

            /* advance to the start of the next block */
            cur_block_ = cur_block_->nxt;
            cur_block_ofs_ = 0;
        }

        /* return the amount we managed to read */
        return bytes_read;
    }

    /* write bytes */
    virtual int write(const void *buf0, size_t len)
    {
        /* get the buffer pointer as a character pointer */
        const char *buf = (const char *)buf0;

        /* keep going until we satisfy the request */
        for (;;)
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

            /* if that satisfied the request, we're done */
            if (len == 0)
                break;

            /* if this is the last block, add another */
            if (cur_block_->nxt == 0)
            {
                /* add the block */
                add_block();
                
                /* if that didn't work, fail */
                if (cur_block_->nxt == 0)
                    return 1;
            }
            
            /* advance to the start of the next block */
            cur_block_ = cur_block_->nxt;
            cur_block_ofs_ = 0;
        }

        /* 
         *   if the seek pointer is past the current content length, we've
         *   written past the old end of the file and thus expanded the file
         *   - note the new length 
         */
        if (cur_block_->ofs + cur_block_ofs_ > len_)
            len_ = cur_block_->ofs + cur_block_ofs_;

        /* success */
        return 0;
    }

    /* get the length of the stream's contents */
    virtual long get_size() { return len_; }

    /* get the current seek offset */
    virtual long get_pos()
    {
        /* 
         *   figure the seek position as the current block's base offset plus
         *   the current offset within that block 
         */
        return cur_block_->ofs + cur_block_ofs_;
    }

    /* set the seek offset */
    virtual int seek(long pos, int mode)
    {
        /* figure the absolute position based on the mode */
        switch (mode)
        {
        case OSFSK_SET:
            /* from the beginning - use pos as given */
            break;
            
        case OSFSK_CUR:
            /* from the current position */
            pos += get_pos();
            break;

        case OSFSK_END:
            /* from the end */
            pos += get_size();
            break;

        default:
            /* other modes are invalid */
            return 1;
        }

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
            return 0;
        }

        /* find the block containing the seek offset */
        CVmMemorySourceBlock *b;
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

        /* success */
        return 0;
    }

    /* flush - we don't buffer, so this does nothing */
    virtual int flush() { return 0; }

    /* 
     *   close - we don't have underlying system resources (other than
     *   our memory buffers), so there's nothing to do here 
     */
    virtual void close() { }

protected:
    /* add a block to the end of the list */
    void add_block()
    {
        long ofs = last_block_ == 0 ? 0 : last_block_->ofs + BlockLen;
        CVmMemorySourceBlock *b = new CVmMemorySourceBlock(ofs);

        if (last_block_ != 0)
            last_block_->nxt = b;
        else
            first_block_ = b;
        last_block_ = b;
    }

    /* current seek block and offset */
    CVmMemorySourceBlock *cur_block_;
    long cur_block_ofs_;

    /* total file length */
    long len_;

    /* head/tail of block list */
    CVmMemorySourceBlock *first_block_;
    CVmMemorySourceBlock *last_block_;
};


#endif /* VMDATASRC_H */
