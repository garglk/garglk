#charset "us-ascii"

/*
 *   Copyright (c) 2001, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This module defines classes and constants related to the File
 *   intrinsic class.  In particular, this module defines the Exception
 *   subclasses thrown by File methods.  
 */

#include <tads.h>
#include <file.h>


/* ------------------------------------------------------------------------ */
/*
 *   File status information.  This is returned from file.getFileInfo().
 */
class FileInfo: object
    construct(typ, siz, ctime, mtime, atime, target, attrs, ...)
    {
        fileType = typ;
        fileSize = siz;
        fileCreateTime = ctime;
        fileModifyTime = mtime;
        fileAccessTime = atime;
        fileLinkTarget = target;
        fileAttrs = attrs;

        /* for convenience, note if it's a directory and/or special link */
        isDir = (typ & FileTypeDir) != 0;
        specialLink = (typ & (FileTypeSelfLink | FileTypeParentLink));
    }

    /* is this file a directory? */
    isDir = nil

    /* 
     *   Is this a special link directory?  This is FileTypeSelfLink for a
     *   directory link to itself; it's FileTypeParentLink for a directory
     *   link to the parent; it's zero for all other files.  On Windows and
     *   Unix, these flags will be set for the special "." and ".."
     *   directories, respectively.  These flags only apply to the
     *   *system-defined* special links; they aren't set for user-created
     *   links that happen to point to self or parent.  This is zero for
     *   all other files.
     */
    specialLink = 0

    /*
     *   Link target.  If the file is a symbolic link, this contains a
     *   string giving the target file's path.  This is the direct target
     *   of this link, which might itself be another link.
     */
    fileLinkTarget = nil

    /* 
     *   type of the file, as a combination of FileTypeXxx bit flags (see
     *   filename.h) 
     */
    fileType = 0

    /*
     *   file attributes, as a combination of FileAttrXxx bit flags (see
     *   filename.h) 
     */
    fileAttrs = 0

    /* size of the file in bytes */
    fileSize = 0

    /* 
     *   The file's time of creation, last modification, and last access,
     *   as Date objects.  On some systems, these timestamps might not all
     *   be available; an item that's not available is set to nil.
     */
    fileCreateTime = nil
    fileModifyTime = nil
    fileAccessTime = nil
;

export FileInfo 'File.FileInfo';

/* ------------------------------------------------------------------------ */
/*
 *   File Exception classes.  All File exceptions derive from FileException,
 *   to allow for generic 'catch' clauses which catch any file-related
 *   error.  
 */
class FileException: Exception
    displayException() { "file error"; }
;

/*
 *   File not found - this is thrown when attempting to open a file for
 *   reading and the file doesn't exist or can't be opened (because the user
 *   doesn't have privileges to read the file, or the file is already being
 *   used by another user, for example).
 */
class FileNotFoundException: FileException
    displayException() { "file not found"; }
;

/*
 *   File creation error - this is thrown when attempting to open a file for
 *   writing and the file can't be created; this can happen because the disk
 *   or the directory is full, due to privilege failures, or due to sharing
 *   violations, among other reasons.  
 */
class FileCreationException: FileException
    displayException() { "cannot create file"; }
;

/*
 *   File cannot be opened - this is thrown when attempting to open a file
 *   for reading and writing but the file can't be opened.  This can happen
 *   for numerous reasons: sharing violations, privilege failures, lack of
 *   space on the disk or in the directory. 
 */
class FileOpenException: FileException
    displayException() { "cannot open file"; }
;

/*
 *   File synchronization exception.  This is thrown when an operation
 *   (such as a read or write) is attempted during normal execution on a
 *   file object that was originally opened during pre-initialization.  A
 *   file object created during pre-initialization can't be used to access
 *   the file during ordinary execution, since the state of the external
 *   file might have changed since the pre-init session ended.  In such
 *   cases, a new file object must be created instead.  
 */
class FileSyncException: FileException
    displayException() { "file synchronization error"; }
;

/*
 *   File closed - this is thrown when an operation is attempted on a file
 *   that has already been explicitly closed. 
 */
class FileClosedException: FileException
    displayException() { "operation attempted on closed file"; }
;

/*
 *   File I/O exception - this is thrown when a read or write operation on a
 *   file fails.  This can indicate, for example, that the device containing
 *   the file is full, or that a physical media error occurred.  
 */
class FileIOException: FileException
    displayException() { "file I/O error"; }
;

/*
 *   File mode error - this is thrown when an attempted operation is
 *   incompatible with the file's mode.  This is thrown under these
 *   conditions:
 *   
 *   - writing to a file opened for read-only access
 *.  - reading from a file opened for write-only access
 *.  - calling readFile or writeFile on a raw-mode file
 *.  - calling readBytes or writeBytes on a non-raw-mode file 
 */
class FileModeException: FileException
    displayException() { "invalid file mode"; }
;

/*
 *   File safety error - this is thrown when an attempted "open" operation
 *   is prohibited by the current file safety level set by the user. 
 */
class FileSafetyException: FileException
    displayException()
    {
        "access to file blocked by user-specified file safety level";
    }
;


/* export the file exceptions for use by the intrinsic class */
export FileNotFoundException 'File.FileNotFoundException';
export FileCreationException 'File.FileCreationException';
export FileOpenException 'File.FileOpenException';
export FileIOException 'File.FileIOException';
export FileSyncException 'File.FileSyncException';
export FileClosedException 'File.FileClosedException';
export FileModeException 'File.FileModeException';
export FileSafetyException 'File.FileSafetyException';

