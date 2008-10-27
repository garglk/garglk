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
 *   File synchronization exception.  This is thrown when an operation (such
 *   as a read or write) is attempted on a file, but an undo, restore, or
 *   restart operation has been performed since the file was opened.
 *   
 *   Because files exist on external devices, it is not practical or
 *   desirable for the virtual machine to keep track of or restore an
 *   external file's state when restoring, undoing, or restarting.  So, when
 *   one of these state-restoring operations is performed, the program state
 *   essentially goes back in time to an earlier point, but any external
 *   files that the program has been accessing do not.  So, the program's
 *   idea of the state of an open file will no longer agree with the actual
 *   state of the file on the external device.  To protect against the
 *   obvious logic errors that would arise if the program attempted to
 *   continue using the file, the File object will throw this exception when
 *   any operation is attemped on a file in this state.  
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
        "file operation prohibited by user-specified file safety level";
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

