#charset "us-ascii"

/*
 *   Copyright (c) 2001, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This header defines the File intrinsic class.  
 */

#ifndef _FILE_H_
#define _FILE_H_

/* include our base class definition */
#include "systype.h"

/*
 *   File methods use the CharacterSet and ByteArray intrinsic
 *   classes, so include their headers to make sure they're available to
 *   File users.  
 */
#include "charset.h"
#include "bytearr.h"


/* ------------------------------------------------------------------------ */
/*
 *   File access modes.  These are used when calling the file open methods
 *   to specify how the file is to be accessed.
 */

/* 
 *   Read mode - the file is opened for reading (writing is not allowed).
 *   When opened in this mode, the file must exist, or a
 *   FileNotFoundException is thrown from the open method. 
 */
#define FileAccessRead            0x0001

/*
 *   Write mode - the file is opened for writing (reading is not allowed).
 *   When opened in this mode, if the file doesn't already exist, a new file
 *   is created; if the file does already exist, the existing data in the
 *   file are discarded (i.e., the file is truncated to zero length) on
 *   open. 
 */
#define FileAccessWrite           0x0002

/*
 *   Read/write mode, keeping existing contents - the file is opened for
 *   both reading and writing.  If the file does not exist, a new file is
 *   created.  If the file does already exist, the existing contents of the
 *   file are kept intact on open.  
 */
#define FileAccessReadWriteKeep   0x0003

/*
 *   Read/write mode, truncating existing contents - the file is opened for
 *   both reading and writing.  If the file does not exist, a new file is
 *   created.  If the file does already exist, the existing contents of the
 *   file are discarded (i.e., the file is truncated to zero length) on
 *   open.  
 */
#define FileAccessReadWriteTrunc  0x0004


/* ------------------------------------------------------------------------ */
/*
 *   Special file identifiers.  These identifiers can be passed to the 'open'
 *   routines in place of the filename string argument.
 *   
 *   The actual name and location of a special file is determined by the
 *   interpreter.  Since games use these internal identifiers rather than the
 *   actual system filenames when accessing special files, different
 *   interpreters can adapt to different local conventions without bothering
 *   the game code with the details.  The game code simply refers to the file
 *   it wants using the virtual identifier, and the interpreter takes care of
 *   the rest.
 *   
 *   Note that special files generally bypass the interpreter "file safety"
 *   settings.  This is important because it allows the library and games a
 *   degree of controlled access to the file system, even when the file
 *   safety settings wouldn't normally allow similar access for arbitrary
 *   file operations.  Even though this special file access can bypass the
 *   file safety level, it doesn't compromise security, because the
 *   interpreter has exclusive control over the names and locations of the
 *   special files - thus a game can only access the particular files that
 *   the interpreter designates as special, and can't use special files to
 *   access arbitrary file system entities.  
 */

/*
 *   The library defaults file.  This is the special file where the library
 *   stores user-controlled start-up default settings.  
 */
#define LibraryDefaultsFile    0x0001


/* ------------------------------------------------------------------------ */
/*
 *   The File intrinsic class provides access to files in the external file
 *   system.  This lets you create, read, and write files.  The class
 *   supports text files (with translations to and from local character
 *   sets), "data" files (using the special TADS 2 binary file format), and
 *   "raw" files (this mode lets you manipulate files in arbitrary text or
 *   binary formats by giving you direct access to the raw bytes in the
 *   file).  
 */
intrinsic class File 'file/030002': Object
{
    /*
     *   File has no constructors, so it is not possible to create a File
     *   with the 'new' operator.  To create a file, use one of the static
     *   creator methods instead:
     *   
     *   f = File.openTextFile()
     *   
     *   All of the open methods throw exceptions if the open fails:
     *   
     *   FileNotFoundException - indicates that the requested file doesn't
     *   exist.  This is thrown when the access mode requires an existing
     *   file but the named file does not exist.
     *   
     *   FileCreationException - indicates that the requested file could not
     *   be created.  This is thrown when the access mode requires creating
     *   a new file but the named file cannot be created.
     *   
     *   FileOpenException - indicates that the requested file could not be
     *   opened.  This is thrown when the access mode allows either an
     *   existing file to be opened or a new file to be created, but neither
     *   could be accomplished.
     *   
     *   FileSafetyException - the requested access mode is not allowed for
     *   the given file due to the current file safety level set by the
     *   user.  Users can set the file safety level (through command-line
     *   switches or other preference mechanisms which vary by interpreter)
     *   to restrict the types of file operations that applications are
     *   allowed to perform, in order to protect their systems from
     *   malicious programs.  This exception indicates that the user has set
     *   a safety level that is too restrictive for the requested operation.
     */

    /*
     *   Static creator method: open a text file.  Returns a File object
     *   that can be used to read or write the file.  'access' is the
     *   read/write mode, and must be one of FileAccessRead or
     *   FileAccessWrite.  'charset' is a CharacterSet object, or can
     *   optionally be a string naming a character set, in which case a
     *   CharacterSet object for the named character set will automatically
     *   be created.  If 'charset' is omitted, a default "us-ascii"
     *   character set will be used.
     *   
     *   When a file is opened in text mode for reading, each call to
     *   readFile() reads and returns a line of text from the file.  When a
     *   file is opened in text mode for writing, any existing file is
     *   discarded and replaced with the new data.  Each read and write to a
     *   text file is mapped through the CharacterSet in effect at the time
     *   of the read or write.  
     */
    static openTextFile(filename, access, charset?);

    /*
     *   Static creator method: open a file in 'data' mode.  Returns a File
     *   object that can be used to read or write the file.  'access'
     *   indicates the desired read/write access and the disposition of any
     *   existing file; any of the FileAccessXxx modes can be used.
     *   
     *   When a file is opened in data mode, you can read and write
     *   integers, strings, and 'true' values to the file, and the values in
     *   the file are marked with their datatype in a private data format.
     *   Because the file uses a tads-specific format, this mode cannot be
     *   used to read files created by other applications or write files for
     *   use by other applications; however, this storage format is
     *   convenient for storing simple data values because the File object
     *   takes care of converting to and from a portable binary format.  
     */
    static openDataFile(filename, access);

    /*
     *   Static creator method: open a file in 'raw' mode.  Returns a File
     *   object that can be used to read or write the file.  'access'
     *   indicates the desired read/write access mode and the disposition of
     *   any existing file; any of the FileAccessXxx modes can be used.
     *   
     *   When a file is opened in raw mode, only ByteArray values can be
     *   read and written.  The File object performs no translations of the
     *   bytes read or written.  This mode requires the calling program
     *   itself to perform all data conversions to and from a raw byte
     *   format, but the benefit of this extra work is that this mode can be
     *   used to read and write files in arbitrary data formats, including
     *   formats defined by other applications.  
     */
    static openRawFile(filename, access);

    /* 
     *   get the CharacterSet object the File is currently using; returns
     *   nil for a non-text file 
     */
    getCharacterSet();

    /*
     *   Set the CharacterSet object the File is to use from now on.  This
     *   is not meaningful except for text files.  'charset' must be a
     *   CharacterSet object; in particular note that a character set name
     *   given as a string is not allowed here.  
     */
    setCharacterSet(charset);

    /*
     *   Close the file.  Flushes any buffered information to the underlying
     *   system file and releases any system resources (such as share locks
     *   or system buffers) associated with the file.  After this routine is
     *   called, no further operations on the file can be performed (a
     *   FileClosedException will be thrown if any subsequent operations are
     *   attempted).
     *   
     *   It's not strictly necessary to call closeFile() on a File, since the
     *   system will automatically do this work when the File object becomes
     *   unreachable and is discarded by the garbage collector.  However, it
     *   is good practice to close a file explicitly by calling this method
     *   as soon as the program reaches a point at which it knows it's done
     *   with the file, because garbage collection might not run for a
     *   significant amount of time after the program is actually done with
     *   the file, in which case the system resources associated with the
     *   file would be needlessly retained for this extended time.  
     */
    closeFile();

    /*
     *   Read from the file.  Returns a data value that depends on the file
     *   mode, as described below, or nil at end of file.
     *   
     *   If the file is open in text mode, this reads a line of text from the
     *   file and returns a string with the text of the line read.  A line of
     *   text is a sequence of characters terminated with a line-ending
     *   sequence, which is a carriage return, line feed, CR/LF pair, LF/CR
     *   pair, or a Unicode line terminator character (0x2028) if the file is
     *   being read with one of the Unicode encodings.  If the line read ends
     *   in a line-ending sequence, the returned text will end in a '\n'
     *   character, regardless of which of the possible line-ending sequences
     *   is actually in the file, so the caller need not worry about the
     *   details of the external file's format.  Every line read from the
     *   file will end in a '\n' except possibly the last line - if the file
     *   does not end with a line-ending sequence, then the last line read
     *   from the file will not end in a '\n' character.  All bytes read from
     *   the file will be mapped to characters through the CharacterSet
     *   object currently in effect in the file, so the returned string will
     *   always be a standard Unicode string, regardless of the byte encoding
     *   of the file.
     *   
     *   If the file is open in 'data' mode, this reads one data element
     *   using the private tads-specific data format.  The result is a value
     *   of one of the types writable with writeFile() in 'data' mode.  In
     *   order to read a 'data' file, the file must have been previously
     *   written in 'data' mode.  
     */
    readFile();

    /*
     *   Write to the file.  Writes the given value to the file in a format
     *   that depends on the file mode, as described below.  No return
     *   value; if an error occurs writing the data, this throws a
     *   FileIOException.
     *   
     *   If the file is open in text mode, this writes text to the file,
     *   converting the given value to a string if necessary (and throwing
     *   an error if such a conversion is not possible), and translating the
     *   string to be written to bytes by mapping the string through the
     *   CharacterSet object currently in effect for the file.  Note that no
     *   line-ending characters are automatically added to the output, so if
     *   the caller wishes to write line terminators, it should simply
     *   include a '\n' character at the end of each line.
     *   
     *   If the file is open in 'data' mode, this writes the value, which
     *   must be a string, integer, enum, or 'true' value, in a private
     *   tads-specific data format that can later be read using the same
     *   format.  The values are converted to the private binary format,
     *   which is portable across platforms: a file written in 'data' mode
     *   on one machine can be copied (byte-for-byte) to another machine,
     *   even one that uses different hardware and a different operating
     *   system, and read back in 'data' mode on the new machine to yield
     *   the original values written.  
     */
    writeFile(val);

    /*
     *   Read bytes from the file into the given ByteArray object.  This can
     *   only be used for a file opened in 'raw' mode.  If 'start' and 'cnt'
     *   are given, they give the starting index in the byte array at which
     *   the bytes read are to be stored, and the number of bytes to read,
     *   respectively; if these are omitted, one byte is read from the file
     *   for each byte in the byte array.
     *   
     *   Returns the number of bytes actually read into the byte array,
     *   which will be less than or equal to the number requested.  If the
     *   number read is less than the number requested, it means that the
     *   end of the file was encountered, and only the returned number of
     *   bytes were available.  
     */
    readBytes(byteArr, start?, cnt?);

    /*
     *   Write bytes from the ByteArray object into the file.  This can only
     *   be used for a file opened in 'raw' mode.  If 'start' and 'cnt' are
     *   given, they give the starting index in the byte array of the bytes
     *   to be written, and the number of bytes to write, respectively; if
     *   these are omitted, all of the bytes in the array are written.
     *   
     *   No return value; if an error occurs writing the data, a
     *   FileIOException is thrown.  
     */
    writeBytes(byteArr, start?, cnt?);

    /*
     *   Get the current read/write position in the file.  Returns the byte
     *   offset in the file of the next byte to be read or written.  Note
     *   that this value is an offset, so 0 is the offset of the first byte
     *   in the file.  
     */
    getPos();

    /*
     *   Set the current read/write position in the file.  'pos' is a byte
     *   offset in the file; 0 is the offset of the first byte.
     *   
     *   For files in 'text' and 'data' modes, a caller should NEVER set the
     *   file position to any value other than a value previously returned
     *   by getPos(), because other positions might violate the format
     *   constraints.  For example, if you move the file position to a byte
     *   in the middle of a line-ending sequence in a text file, subsequent
     *   reading from the file might misinterpret the sequence as something
     *   other than a line ending, or as an extra line ending.  If you move
     *   the position in a 'data' file to a byte in the middle of an integer
     *   value, reading from the file would misinterpret as a data type tag
     *   a byte that is part of the integer value instead.  So it is never
     *   meaningful or safe to set an arbitrary byte offset in these file
     *   formats; only values known to be valid by virtue of having been
     *   returned from getPos() can be used here in these modes.  
     */
    setPos(pos);

    /*
     *   Set the current read/write position to the end of the file.  This
     *   can be used, for example, to open a 'data' mode file for
     *   read/write/keep access (keeping the contents of an existing file)
     *   and then adding more data after all of the existing data in the
     *   file.  
     */
    setPosEnd();

    /*
     *   Static creator method: open a resource in 'text' mode.  This acts
     *   like openTextFile(), but rather than opening an ordinary file, this
     *   method opens a resource.  Resources differ from ordinary files in
     *   two important respects.  First, a resource is named with a
     *   URL-style path rather than a local file system name.  Second, a
     *   resource can be embedded in the program's executable (.t3) file, or
     *   can be embedded in an external resource bundle (.3r0, etc) file.
     *   
     *   Resources are read-only, so the access mode is implicitly
     *   FileAccessRead.  
     */
    static openTextResource(resname, charset?);

    /*
     *   Static creator method: open a resource in 'raw' mode.  This acts
     *   like openRawFile(), but opens a resource rather than an ordinary
     *   file.
     *   
     *   Resources are read-only, so the access mode is implicitly
     *   FileAccessRead.  
     */
    static openRawResource(resname);

    /* get the size in bytes of the file */
    getFileSize();
}

#endif /* _FILE_H_ */
