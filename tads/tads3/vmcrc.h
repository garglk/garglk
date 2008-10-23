/* Copyright (c) 2003 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmcrc.h - compute a CRC-32 checksum of a data stream
Function
  
Notes
  
Modified
  06/21/03 MJRoberts  - Creation
*/

#ifndef VMCRC_H
#define VMCRC_H

#include <stdlib.h>

class CVmCRC32
{
public:
    CVmCRC32()
    {
        /* start with zero in the accumulator */
        acc_ = 0;
    }

    /* add the given buffer into the checksum */
    void scan_bytes(const void *ptr, size_t len);

    /* retrieve the current checksum value */
    unsigned long get_crc_val() const { return acc_; }

protected:
    /* the checksum accumulator */
    unsigned long acc_;
};


#endif /* VMCRC_H */
