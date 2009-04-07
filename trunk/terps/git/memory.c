// $Id: memory.c,v 1.11 2004/01/25 21:04:19 iain Exp $

#include "git.h"
#include <stdlib.h>
#include <string.h>

const git_uint8 * gRom;
git_uint8 * gRam;

git_uint32 gRamStart;
git_uint32 gExtStart;
git_uint32 gEndMem;
git_uint32 gOriginalEndMem;

#define RAM_OVERLAP 8

void initMemory (const git_uint8 * gamefile, git_uint32 size)
{
	// Make sure we have at least enough
	// data for the standard glulx header.

	if (size < 36)
		fatalError("This file is too small to be a valid glulx gamefile");
	
	// Set up a basic environment that will
	// let us inspect the header.

	gRom = gamefile;
	gRamStart = 36;

	// Check the magic number. From the spec:
	//     * Magic number: 47 6C 75 6C, which is to say ASCII 'Glul'.

	if (memRead32 (0) != 0x476c756c)
		fatalError("This is not a glulx game file");

	// Load the correct values for ramstart, extstart and endmem.
	// (Load ramstart last because it's required by memRead32 --
	// if we get a wonky ramstart, the other reads could fail.)

	gOriginalEndMem = gEndMem = memRead32 (16);
	gExtStart = memRead32 (12);
	gRamStart = memRead32 (8);

	// Make sure the values are sane.

    if (gRamStart < 36)
	    fatalError ("Bad header (RamStart is too low)");
        
    if (gRamStart > size)
	    fatalError ("Bad header (RamStart is bigger than the entire gamefile)");
        
    if (gExtStart > size)
	    fatalError ("Bad header (ExtStart is bigger than the entire gamefile)");
        
    if (gExtStart < gRamStart)
	    fatalError ("Bad header (ExtStart is lower than RamStart)");
        
    if (gEndMem < gExtStart)
	    fatalError ("Bad header (EndMem is lower than ExtStart)");
        
	if (gRamStart & 255)
	    fatalError ("Bad header (RamStart is not a multiple of 256)");

	if (gExtStart & 255)
	    fatalError ("Bad header (ExtStart is not a multiple of 256)");

	if (gEndMem & 255)
	    fatalError ("Bad header (EndMem is not a multiple of 256)");

	// Allocate the RAM. We'll duplicate the last few bytes of ROM
	// here so that reads which cross the ROM/RAM boundary don't fail.

	gRamStart -= RAM_OVERLAP; // Adjust RAM boundary to include some ROM.

	gRam = malloc (gEndMem - gRamStart);
    if (gRam == NULL)
        fatalError ("Failed to allocate game RAM");

	gRam -= gRamStart;

	// Copy the initial contents of RAM.
	memcpy (gRam + gRamStart, gRom + gRamStart, gExtStart - gRamStart);

	// Zero out the extended RAM.
	memset (gRam + gExtStart, 0, gEndMem - gExtStart);

	gRamStart += RAM_OVERLAP; // Restore boundary to its previous value.
}

int verifyMemory ()
{
    git_uint32 checksum = 0;

    git_uint32 n;
    for (n = 0 ; n < gExtStart ; n += 4)
        checksum += read32 (gRom + n);
    
    checksum -= read32 (gRom + 32);
    return (checksum == read32 (gRom + 32)) ? 0 : 1;
}

int resizeMemory (git_uint32 newSize, int isInternal)
{
    git_uint8* newRam;
    
    if (newSize == gEndMem)
        return 0; // Size is not changed.
    if (!isInternal && heap_is_active())
        fatalError ("Cannot resize Glulx memory space while heap is active.");
    if (newSize < gOriginalEndMem)
        fatalError ("Cannot resize Glulx memory space smaller than it started.");
    if (newSize & 0xFF)
        fatalError ("Can only resize Glulx memory space to a 256-byte boundary.");
    
    gRamStart -= RAM_OVERLAP; // Adjust RAM boundary to include some ROM.
    newRam = realloc(gRam + gRamStart, newSize - gRamStart);
    if (!newRam)
    {	
        gRamStart += RAM_OVERLAP; // Restore boundary to its previous value.
        return 1; // Failed to extend memory.
    }
    if (newSize > gEndMem)
        memset (newRam + gEndMem - gRamStart, 0, newSize - gEndMem);

    gRam = newRam - gRamStart;
    gEndMem = newSize;
    gRamStart += RAM_OVERLAP; // Restore boundary to its previous value.
    return 0;
}

void resetMemory (git_uint32 protectPos, git_uint32 protectSize)
{
    git_uint32 protectEnd = protectPos + protectSize;
    git_uint32 i;

    // Deactivate the heap (if it was active).
    heap_clear();

    gEndMem = gOriginalEndMem;
      
    // Copy the initial contents of RAM.
    for (i = gRamStart; i < gExtStart; ++i)
    {
        if (i >= protectEnd || i < protectPos)
            gRam [i] = gRom [i];
    }

    // Zero out the extended RAM.
    for (i = gExtStart; i < gEndMem; ++i)
    {
        if (i >= protectEnd || i < protectPos)
            gRam [i] = 0;
    }
}

void shutdownMemory ()
{
    // We didn't allocate the ROM, so we
    // only need to dispose of the RAM.
    
    free (gRam + gRamStart - RAM_OVERLAP);
    
    // Zero out all our globals.
    
    gRamStart = gExtStart = gEndMem = gOriginalEndMem = 0;
    gRom = gRam = NULL;
}

git_uint32 memReadError (git_uint32 address)
{
    fatalError ("Out-of-bounds memory access");
    return 0;
}

void memWriteError (git_uint32 address)
{
    fatalError ("Out-of-bounds memory access");
}
