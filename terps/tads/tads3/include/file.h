#charset "us-ascii"
#pragma once

/*
 *   Copyright (c) 2001, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This header defines the File intrinsic class.  
 */


/* include our base class definition */
#include "systype.h"

/*
 *   File methods use the CharacterSet and ByteArray intrinsic
 *   classes, so include their headers to make sure they're available to
 *   File users.  
 */
#include "charset.h"
#include "bytearr.h"

/* if we're using File objects, we probably want FileName objects as well */
#include "filename.h"

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
 *   File mode constants.  These are returned from getFileMode() to indicate
 *   the mode used to open the file.  
 */

/* text mode */
#define FileModeText  1

/* "data" mode */
#define FileModeData  2

/* "raw" mode */
#define FileModeRaw   3


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

/*
 *   Web UI preference settings file.  This is the special file where we
 *   store display style settings for the Web UI.  
 */
#define WebUIPrefsFile         0x0002


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
intrinsic class File 'file/030003': Object
{
    /*
     *   File has no constructors.  Instead of using 'new', you create a File
     *   object by opening a file through one of the openXxxFile methods:
     *   
     *   f = File.openTextFile()
     *   
     *   All of the open methods have a 'filename' argument giving the name
     *   of the file to open.  This is usually a string giving a file name in
     *   the local file system.  You can also use a TemporaryFile object in
     *   place of a filename, to open a temporary file.
     *   
     *   All of the open methods throw exceptions if the open fails:
     *   
     *   FileNotFoundException - indicates that the requested file doesn't
     *   exist.  This is thrown when the access mode requires an existing
     *   file but the named file does not exist.
     *   
     *   FileCreationException - indicates that the requested file couldn't
     *   be created.  This is thrown when the access mode requires creating a
     *   new file but the named file cannot be created.
     *   
     *   FileOpenException - indicates that the requested file couldn't be
     *   opened.  This is thrown when the access mode allows either an
     *   existing file to be opened or a new file to be created, but neither
     *   could be accomplished.
     *   
     *   FileSafetyException - the requested access mode isn't allowed for
     *   the given file due to the current file safety level set by the user.
     *   Users can set the file safety level (through command-line switches
     *   or other preference mechanisms which vary by interpreter) to
     *   restrict the types of file operations that applications are allowed
     *   to perform, in order to protect their systems from malicious
     *   programs.  This exception indicates that the user has set a safety
     *   level that is too restrictive for the requested operation.  
     */

    /*
     *   Static creator method: open a text file.  Returns a File object that
     *   can be used to read or write the file.
     *   
     *   'filename' is the name of the file to open.  This is a string giving
     *   the name of a file in the local file system.  It can alternatively
     *   be a TemporaryFile object, to open a temporary file.
     *   
     *   'access' is the read/write mode, and must be one of FileAccessRead
     *   or FileAccessWrite.
     *   
     *   'charset' is a CharacterSet object, or can optionally be a string
     *   naming a character set, in which case a CharacterSet object for the
     *   named character set will automatically be created.  If 'charset' is
     *   omitted, the local system's default character set for file contents
     *   is used.
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
     *   object that can be used to read or write the file.
     *   
     *   'filename' is a string giving the file name in the local file
     *   system, or a TemporaryFile object.
     *   
     *   'access' indicates the desired read/write access and the disposition
     *   of any existing file; any of the FileAccessXxx modes can be used.
     *   
     *   When a file is opened in data mode, you can read and write integers,
     *   strings, and 'true' values to the file, and the values in the file
     *   are marked with their datatype in a private data format.  Because
     *   the file uses a tads-specific format, this mode cannot be used to
     *   read files created by other applications or write files for use by
     *   other applications; however, this storage format is convenient for
     *   storing simple data values because the File object takes care of
     *   converting to and from a portable binary format.  
     */
    static openDataFile(filename, access);

    /*
     *   Static creator method: open a file in 'raw' mode.  Returns a File
     *   object that can be used to read or write the file.
     *   
     *   'filename' is a string giving the name of the file in the local file
     *   system, or a TemporaryFile object.
     *   
     *   'access' indicates the desired read/write access mode and the
     *   disposition of any existing file; any of the FileAccessXxx modes can
     *   be used.
     *   
     *   When a file is opened in raw mode, only ByteArray values can be read
     *   and written.  The File object performs no translations of the bytes
     *   read or written.  This mode requires the calling program itself to
     *   perform all data conversions to and from a raw byte format, but the
     *   benefit of this extra work is that this mode can be used to read and
     *   write files in arbitrary data formats, including formats defined by
     *   other applications.  
     */
    static openRawFile(filename, access);

    /* 
     *   get the CharacterSet object the File is currently using; returns
     *   nil for a non-text file 
     */
    getCharacterSet();

    /*
     *   Set the CharacterSet object the File is to use from now on.  This
     *   isn't meaningful except for text files.  'charset' can be a
     *   CharacterSet object, a string giving the name of a character mapping
     *   (in which case a CharacterSet object is automatically created based
     *   on the name), or nil (in which case the local system's default
     *   character set for text files is used).  
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
     *   If the game is running in web server mode, the file might be on a
     *   remote storage server.  In this case, if the file was opened with
     *   write access, closing it will send the file to the storage server.
     *   
     *   Note that this method can throw an error, so you shouldn't consider
     *   updates to the file to be "safe" until this method returns
     *   successfully.  On many systems, writes are buffered in memory, so
     *   closing the file can involve flushing buffers, which can trigger the
     *   same sorts of errors that can happen with ordinary writes (running
     *   out of disk space, physical media defects, etc).  In addition, when
     *   the file is on a remote network storage server, closing a file
     *   opened with write access transmits the file to the storage server,
     *   which can encounter network errors.
     *   
     *   You should always explicitly close files when done with them.  This
     *   is especially important when writing to a file, because many systems
     *   buffer written data in memory and don't write changes to the
     *   physical media until the file is closed.  This means that updates
     *   can be lost if the program crashes (or the computer loses power,
     *   etc) while the file is still open.  Closing the file as soon as
     *   you're done with it reduces the chances of this kind of data loss.
     *   It also helps overall system performance to release resources back
     *   to the operating system as soon as you're done with them.
     *   
     *   If you *don't* close a file, though, the system will close it
     *   automatically when the File object becomes unreachable and is
     *   deleted by the garbage collector.  It's considered bad form to
     *   depend on this for the reasons above, and it's also problematic
     *   because you won't have any way of finding out if an error should
     *   happen on close.  
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
     *   doesn't end with a line-ending sequence, then the last line read
     *   from the file won't end in a '\n' character.  All bytes read from
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
     *   an error if such a conversion isn't possible), and translating the
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
     *   Write bytes from the given source object into the file.  This can
     *   only be used for a file opened in 'raw' mode.
     *   
     *   The source object must be one of the following object types:
     *   
     *   File: the contents of the given source file are copied to 'self'.
     *   'start' is the starting seek position in the source file; if
     *   omitted, the current seek position is the default.  'cnt' is the
     *   number of bytes to copy; if omitted, the file is copied from the
     *   given starting position to the end of the file.
     *   
     *   ByteArray: the bytes of the byte array are copied to the file.
     *   'start' is the starting index in the byte array; if omitted, the
     *   default is the first byte (index 1).  'cnt' is the number of bytes
     *   to copy; if omitted, bytes are copied from the start position to the
     *   end of the array.
     *   
     *   No return value; if an error occurs writing the data, a
     *   FileIOException is thrown.  
     */
    writeBytes(source, start?, cnt?);

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

    /*
     *   Get the file mode.  This returns one of the FileModeXxx constants,
     *   indicating the mode used to open the file (text, data, raw).  
     */
    getFileMode();

    /*
     *   Extract the file's "root name" from the given filename string.  This
     *   returns a new string giving the portion of the filename excluding
     *   any directory path.  This parses the filename according to the local
     *   file system's syntax rules.  For example, given the filename
     *   'a/b/c.txt', if you're running on a Unix or Linux machine, the
     *   function returns 'c.txt'.
     *   
     *   Note that this function doesn't attempt to open the file or check
     *   for its existence or validity; it simply parses the name according
     *   to the local syntax conventions.
     *   
     *   (It's recommended that you use the newer FileName.getBaseName() in
     *   place of this function.)
     */
    static getRootName(filename);

    /*
     *   Delete the file with the given name.  This erases the file from
     *   disk.  'filename' is a string giving the name of the file to delete,
     *   or one of the special file identifier values.
     *   
     *   The file can only be deleted if the file safety level would allow
     *   you to write to the file; if not, a file safety exception is thrown.
     *   
     *   (It's recommended that you use the newer FileName.deleteFile() in
     *   place of this function.)
     */
    static deleteFile(filename);

    /*
     *   Change the file mode.  'mode' is a FileModeXxx value giving the
     *   desired new file mode.
     *   
     *   If the mode is FileModeText, 'charset' is the character set mapping
     *   to use for the file; this can be given as a CharacterSet object, or
     *   as a string giving the name of a character set.  If the value is nil
     *   or the argument is omitted, the local system's default character for
     *   file contents is used.  The 'charset' parameter is ignored for other
     *   modes.
     */
    setFileMode(mode, charset?);

    /*
     *   Pack the given data values into bytes according to a format
     *   definition string, and write the packed bytes to the file.  This
     *   function is designed to simplify writing files that use structured
     *   binary formats defined by third parties, such as JPEG or PDF.  The
     *   function translates native TADS data values into selected binary
     *   formats, and writes the resulting bytes to the file, all in a single
     *   operation.
     *   
     *   'format' is the format string, and the remaining arguments are the
     *   values to be packed.
     *   
     *   Returns the number of bytes written to the file.  (More precisely,
     *   returns the final file position as a byte offset from the starting
     *   file pointer.  If a positioning code like @ or X is used in the
     *   string, it's possible that more bytes were actually written.)
     *   
     *   See Byte Packing in the System Manual for details.  
     */
    packBytes(format, ...);

    /*
     *   Read bytes and unpack into a data structure, according to the format
     *   description string 'desc'.
     *   
     *   'format' is the format string.  The function reads bytes from the
     *   current location in the file and translates them into data values
     *   according to the format string, returning a list of the unpacked
     *   values.
     *   
     *   Refer to Byte Packing in the System Manual for details.  
     */
    unpackBytes(format);

    /*
     *   Calculate the 256-bit SHA-2 hash of bytes read from the file,
     *   starting at the current seek location and continuing for the given
     *   number of bytes.  If the length is omitted, the whole rest of the
     *   file is hashed.  This has the side effect of reading the given
     *   number of bytes from the file, so it leaves the seek position set to
     *   the next byte after the bytes hashed.
     *   
     *   Returns a string of 64 hex digits giving the hash result.
     *   
     *   This can only be used on files opened in raw mode with read access.
     */
    sha256(length?);

    /*
     *   Calculate the MD5 digest of bytes read from the file, starting at
     *   the current seek location and continuing for the given number of
     *   bytes.  If the length is omitted, the whole rest of the file is
     *   digested.  This has the side effect of reading the given number of
     *   bytes from the file, so it leaves the seek position set to the next
     *   byte after the bytes digested.
     *   
     *   Returns a string of 32 hex digits giving the digest result.
     *   
     *   This can only be used on files opened in raw mode with read access. 
     */
    digestMD5(length?);
}


/* ------------------------------------------------------------------------ */
/*
 *   The TemporaryFile intrinsic class represents a temporary file name in
 *   the local file system.  Temporary files can be used to store data too
 *   large to conveniently store in memory.
 *   
 *   You create a temporary file with 'new TemporaryFile()'.  This
 *   automatically assigns the object a unique filename in the local file
 *   system, typically in a system directory reserved for temporary files.
 *   The local file can then be opened, read, written, and otherwise
 *   manipulated via the File class, just like any other file.  Simply pass
 *   the TemporaryFile object in place of a filename to the File.openXxx
 *   methods.  
 *   
 *   The underlying file system file will be deleted automatically when the
 *   TemporaryFile object is collected by the garbage collector (or when the
 *   program terminates).  This means that you don't have to worry about
 *   cleaning up the file system space used by the file; it'll be released
 *   automatically when the file is no longer needed.  However, you can call
 *   the deleteFile() method to explicitly release the file when you're done
 *   with it, if you want to ensure that the resource is returned to the
 *   operating system as soon as possible.
 *   
 *   TemporaryFile objects are inherently transient - they're only valid for
 *   the current session on the current local system, so they can't be saved
 *   or restored.
 *   
 *   Temporary files are exempt from the file safety level settings, because
 *   the inherent restrictions on temporary files provide the same system
 *   protections that the safety level settings provide for ordinary files.
 */
intrinsic class TemporaryFile 'tempfile/030000': Object
{
    /*
     *   Get the name of the underlying file system object.  This returns a
     *   string with the local filename.  This is mostly for debugging
     *   purposes or for displaying to the user.  You can't necessarily use
     *   this filename in a call to File.openXxxFile, because the file path
     *   is usually in a system directory reserved for temporary files, and
     *   the file safety level settings often prohibit opening files outside
     *   of the program's own home directory.  To open the temp file, you
     *   should always pass the TemporaryFile object itself in place of the
     *   filename.
     */
    getFilename();

    /*
     *   Delete the underlying file system object.  This deletes the
     *   temporary file and marks the TemporaryFile object as invalid.  After
     *   calling this, you can no longer open the file via the
     *   File.openXxxFile methods.
     *   
     *   This method allows you to release the underlying file system
     *   resources as soon as you're done with the temp file.  It's never
     *   necessary to do this.  TADS automatically deletes the underlying
     *   file system resources when the TemporaryFile object is deleted by
     *   the garbage collector (or when the program terminates), so the
     *   operating system file will be deleted eventually whether you call
     *   this method or not.  The point of this method is to let you tell the
     *   system *exactly* when you're done with the file, so that the
     *   resources can be released earlier than if we waited for garbage
     *   collection to take care of it.  This should make little difference
     *   in most situations, but in a program that will run for a long time
     *   and use a lot of temporary files, it might be worthwhile to release
     *   resources manually as soon as possible.  
     */
    deleteFile();
}


/* ------------------------------------------------------------------------ */
/*
 *   The filename passed to the File "open" methods, as well as to most
 *   system functions that accept filename arguments, can be a TadsObject
 *   object in lieu of a string.  Such an object must implement the following
 *   methods:
 *   
 *   getFilename() - return the actual filename to use, which must be a
 *   string or TemporaryFile object.
 *   
 *   closeFile() - optional.  This is called just after the underlying system
 *   file is closed, allowing the program to perform any desired
 *   post-processing on the file.  
 */
export getFilename 'FileSpec.getFilename';
export closeFile 'FileSpec.closeFile';

