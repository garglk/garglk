#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmerrmsg.cpp - T3 VM error message strings
Function
  Defines the message text for VM errors.  All error text is isolated into
  this module for easy replacement in translated versions.
Notes
  
Modified
  05/13/00 MJRoberts  - Creation
*/

#include <stdarg.h>
#include <stdlib.h>

#include "t3std.h"
#include "vmerr.h"

/* ------------------------------------------------------------------------ */
/*
 *   Enable or disable verbose messages.  To conserve memory, verbose
 *   messages can be omitted.  To omit verbose messages, the platform
 *   makefile should define the preprocessor symbol VMERR_OMIT_VERBOSE. 
 */
#ifdef VMERR_OMIT_VERBOSE
# define VMsg(msg) ""
#else
# define VMsg(msg) msg
#endif

#ifdef VMERR_BOOK_MSG
# define VBook(msg) , msg
#else
# define VBook(msg) 
#endif

/*
 *   To conserve even more memory, the messages can be omitted entirely.  To
 *   disable all compiled-in messages, define VMERR_OMIT_MESSAGES. 
 */


/* ------------------------------------------------------------------------ */
/*
 *   T3 VM Error Messages 
 *   
 *   The messages must be sorted by message number, so that we can perform
 *   a binary search to look up a message by number.  
 */
const err_msg_t vm_messages_english[] =
{
#ifdef VMERR_OMIT_MESSAGES

    { 0, 0, 0  VBook(0) }

#else /* VMERR_OMIT_MESSAGES */
    
    { VMERR_READ_FILE,
    "error reading file",
    VMsg("Error reading file.  The file might be corrupted or a media error "
         "might have occurred.") },

    { VMERR_WRITE_FILE,
    "error writing file",
    VMsg("Error writing file.  The media might be full, or another media "
         "error might have occurred.") },

    { VMERR_FILE_NOT_FOUND,
    "file not found",
    VMsg("Error opening file.  The specified file might not exist, you "
         "might not have sufficient privileges to open the file, or "
         "a sharing violation might have occurred.") },

    { VMERR_CREATE_FILE,
    "error creating file",
    VMsg("Error creating file.  You might not have sufficient privileges "
         "to open the file, or a sharing violation might have occurred.") },

    { VMERR_CLOSE_FILE,
    "error closing file",
    VMsg("Error closing file.  Some or all changes made to the file might "
         "not have been properly written to the physical disk/media.") },

    { VMERR_DELETE_FILE,
    "error deleting file",
    VMsg("Error deleting file. This could because you don't have "
         "sufficient privileges, the file is marked as read-only, another "
         "program is using the file, or a physical media error occurred.") },

    { VMERR_PACK_PARSE,
    "data packer format string parsing error at character index %d",
    VMsg("The format string for the data packing or unpacking has a syntax "
         "error at character index %d.") },

    { VMERR_PACK_ARG_MISMATCH,
    "data packer argument type mismatch at format string index %d",
    VMsg("Data packer argument type mismatch. The type of the argument "
         "doesn't match the format code at character index %d in the "
    "format string.") },

    { VMERR_PACK_ARGC_MISMATCH,
    "wrong number of data packer arguments at format string index %d",
    VMsg("Wrong number of arguments to the data packer. The number of "
         "argument values doesn't match the number of elements in the "
         "format string.") },

    { VMERR_NET_FILE_NOIMPL,
    "this file operation isn't supported for storage server files",
    VMsg("This file operation isn't supported for storage server files. "
         "This operation can only be used with local disk files.") },

    { VMERR_RENAME_FILE,
    "error renaming file",
    VMsg("An error occurred renaming the file. The new name might already "
         "be used by an existing file, you might not have the necessary "
         "permissions in the new or old directory locations, or the "
         "new name might be in an incompatible location, such as on "
         "a different device or volume." ) },

    { VMERR_OBJ_IN_USE,
    "object ID in use - the image/save file might be corrupted",
    VMsg("An object ID requested by the image/save file is already in use "
         "and cannot be allocated to the file.  This might indicate that "
         "the file is corrupted.") },

    { VMERR_OUT_OF_MEMORY,
    "out of memory",
    VMsg("Out of memory.  Try making more memory available by closing "
         "other applications if possible.") },

    { VMERR_NO_MEM_FOR_PAGE,
    "out of memory allocating pool page",
    VMsg("Out of memory allocating pool page.  Try making more memory "
         "available by closing other applications.") },

    { VMERR_BAD_POOL_PAGE_SIZE,
    "invalid page size - file is not valid",
    VMsg("Invalid page size.  The file being loaded is not valid.") },

    { VMERR_OUT_OF_PROPIDS,
    "no more property ID's are available",
    VMsg("Out of property ID's.  No more properties can be allocated.") },

    { VMERR_CIRCULAR_INIT,
    "circular initialization dependency in intrinsic class (internal error)",
    VMsg("Circular initialization dependency detected in intrinsic class.  "
         "This indicates an internal error in the interpreter.  Please "
         "report this error to the interpreter's maintainer.") },

    { VMERR_UNKNOWN_METACLASS,
    "this interpreter version cannot run this program (program requires "
    "intrinsic class %s, which is not available in this interpreter)",
    VMsg("This image file requires an intrinsic class with the identifier "
         "\"%s\", but the class is not available in this interpreter.  This "
         "program cannot be executed with this interpreter.") },

    { VMERR_UNKNOWN_FUNC_SET,
    "this interpreter version cannot run this program (program requires "
    "intrinsic function set %s, which is not available in this interpreter)",
    VMsg("This image file requires a function set with the identifier "
         "\"%s\", but the function set is not available in this "
         "intepreter.  This program cannot be executed with this "
         "interpreter.") },

    { VMERR_READ_PAST_IMG_END,
    "reading past end of image file - program might be corrupted",
    VMsg("Reading past end of image file.  The image file might "
         "be corrupted.") },

    { VMERR_NOT_AN_IMAGE_FILE,
    "this is not an image file (no valid signature found)",
    VMsg("This file is not a valid image file - the file has an invalid "
         "signature.  The image file might be corrupted.") },

    { VMERR_UNKNOWN_IMAGE_BLOCK,
    "this interpreter version cannot run this program (unknown block type "
    "in image file)",
    VMsg("Unknown block type.  This image file is either incompatible with "
         "this version of the interpreter, or has been corrupted.") },

    { VMERR_IMAGE_BLOCK_TOO_SMALL,
    "data block too small",
    VMsg("A data block in the image file is too small.  The image file might "
         "be corrupted.") },

    { VMERR_IMAGE_POOL_BEFORE_DEF,
    "invalid image file: pool page before pool definition",
    VMsg("This image file is invalid because it specifies a pool page "
         "before the pool's definition.  The image file might be "
         "corrupted.") },

    { VMERR_IMAGE_POOL_BAD_PAGE,
    "invalid image file: pool page out of range of definition",
    VMsg("This image file is invalid because it specifies a pool page "
         "outside of the range of the pool's definition.  The image file "
         "might be corrupted.") },

    { VMERR_IMAGE_BAD_POOL_ID,
    "invalid image file: invalid pool ID",
    VMsg("This image file is invalid because it specifies an invalid "
         "pool ID.  The image file might be corrupted.") },

    { VMERR_LOAD_BAD_PAGE_IDX,
    "invalid image file: bad page index",
    VMsg("This image file is invalid because it specifies an invalid "
         "page index.  The image file might be corrupted.") },

    { VMERR_LOAD_UNDEF_PAGE,
    "loading undefined pool page",
    VMsg("The program is attempting to load a pool page that is not present "
         "in the image file.  The image file might be corrupted.") },

    { VMERR_IMAGE_POOL_REDEF,
    "invalid image file: pool is defined more than once",
    VMsg("This image file is invalid because it defines a pool more than "
         "once.  The image file might be corrupted.") },

    { VMERR_IMAGE_METADEP_REDEF,
    "invalid image file: multiple intrinsic class dependency tables found",
    VMsg("This image file is invalid because it contains multiple "
         "intrinsic class tables.  The image file might be corrupted.") },

    { VMERR_IMAGE_NO_METADEP,
    "invalid image file: no intrinsic class dependency table found",
    VMsg("This image file is invalid because it contains no intrinsic class "
         "tables.  The image file might be corrupted.") },

    { VMERR_IMAGE_FUNCDEP_REDEF,
    "invalid image file: multiple function set dependency tables found",
    VMsg("This image file is invalid because it contains multiple "
         "function set tables.  The image file might be corrupted.") },

    { VMERR_IMAGE_NO_FUNCDEP,
    "invalid image file: no function set dependency table found",
    VMsg("This image file is invalid because it contains no function set "
         "tables.  The image file might be corrupted.") },

    { VMERR_IMAGE_ENTRYPT_REDEF,
    "invalid image file: multiple entrypoints found",
    VMsg("This image file is invalid because it contains multiple entrypoint "
         "definitions.  The image file might be corrupted.") },

    { VMERR_IMAGE_NO_ENTRYPT,
    "invalid image file: no entrypoint found",
    VMsg("This image file is invalid because it contains no entrypoint "
         "specification.  The image file might be corrupted.") },

    { VMERR_IMAGE_INCOMPAT_VSN,
    "incompatible image file format version",
    VMsg("This image file has an incompatible format version.  You must "
         "obtain a newer version of the interpreter to execute this "
         "program.") },

    { VMERR_IMAGE_NO_CODE,
    "image contains no code",
    VMsg("This image file contains no executable code.  The file might "
         "be corrupted.") },

    { VMERR_IMAGE_INCOMPAT_HDR_FMT,
    "incomptabile image file format: method header too old",
    VMsg("This image file has an incompatible method header format.  "
         "This is an older image file version which this interpreter "
         "does not support.") },

    { VMERR_UNAVAIL_INTRINSIC,
    "unavailable intrinsic function called (index %d in function set \"%s\")",
    VMsg("Unavailable intrinsic function called (the function is at "
         "index %d in function set \"%s\").  This function is not available "
         "in this version of the interpreter and cannot be called when "
         "running the program with this version.  This normally indicates "
         "either (a) that the \"preinit\" function (or code invoked by "
         "preinit) called an intrinsic that isn't available during "
         "this phase, such as an advanced display function; or (b) that "
         "the program used '&' to refer to a function address, and"
         "the function isn't available in this interpreter version.") },

    { VMERR_UNKNOWN_METACLASS_INTERNAL,
    "unknown internal intrinsic class ID %x",
    VMsg("Unknown internal intrinsic class ID %x.  This indicates an "
         "internal error in the interpreter.  Please report this error "
         "to the interpreter's maintainer.") },

    { VMERR_XOR_MASK_BAD_IN_MEM,
    "page mask is not allowed for in-memory image file",
    VMsg("This image file cannot be loaded from memory because it contains "
         "masked data.  Masked data is not valid with in-memory files.  "
         "This probably indicates that the program file was not installed "
         "properly; you must convert this program file for in-memory use "
         "before you can load the program with this version of the "
         "interpreter.") },

    { VMERR_NO_IMAGE_IN_EXE,
    "no embedded image file found in executable",
    VMsg("This executable does not contain an embedded image file.  The "
         "application might not be configured properly or might need to "
         "be rebuilt.  Re-install the application or obtain an updated "
         "version from the application's author.") },

    { VMERR_OBJ_SIZE_OVERFLOW,
    "object size exceeds hardware limits of this computer",
    VMsg("An object defined in this program file exceeds the hardware limits "
         "of this computer.  This program cannot be executed on this type "
         "of computer or operating system.  Contact the program's author "
         "for assistance.") },

    { VMERR_METACLASS_TOO_OLD,
    "this interpreter is too old to run this program (program requires "
    "intrinsic class version %s, interpreter provides version %s)",
    VMsg("This program needs the intrinsic class \"%s\".  This VM "
         "implementation does not provide a sufficiently recent version "
         "of this intrinsic class; the latest version available in this "
         "VM is \"%s\".  This program cannot run with this version of the "
         "VM; you must use a more recent version of the VM to execute this "
         "program.") },
         
    { VMERR_INVAL_METACLASS_DATA,
    "invalid intrinsic class data - image file may be corrupted",
    VMsg("Invalid data were detected in an intrinsic class.  This might "
         "indicate that the image file has been corrupted.  You might need "
         "to re-install the program.") },

    { VMERR_BAD_STATIC_NEW,
    "invalid object - class does not allow loading",
    VMsg("An object in the image file cannot be loaded because its class "
         "does not allow creation of objects of the class.  This usually "
         "means that the class is abstract and cannot be instantiated "
         "as a concrete object.") },

    { VMERR_FUNCSET_TOO_OLD,
    "this interpreter is too old to run this program (program requires "
    "function set version %s, interpreter provides version %s)",
     VMsg("This program needs the function set \"%s\".  This VM "
          "implementation does not provide a sufficiently recent version "
          "of this function set; the latest version available in this VM "
          "is \"%s\".  This program cannot run with this version of the "
          "VM; you must use a more recent version of the VM to execute "
          "this program.") },

    { VMERR_INVAL_EXPORT_TYPE,
    "exported symbol \"%s\" is of incorrect datatype",
    VMsg("The exported symbol \"%s\" is of the incorrect datatype.  Check "
         "the program and the library version.") },

    { VMERR_INVAL_IMAGE_MACRO,
    "invalid data in macro definitions in image file (error code %d)",
    VMsg("The image file contains invalid data in the macro symbols "
         "in the debugging records: macro loader error code %d.  This "
         "might indicate that the image file is corrupted.") },

    { VMERR_NO_MAINRESTORE,
    "this program is not capable of restoring a saved state on startup",
    VMsg("This program is not capable of restoring a saved state on "
         "startup.  To restore the saved state, you must run the program "
         "normally, then use the appropriate command or operation within "
         "the running program to restore the saved position file.") },

    { VMERR_IMAGE_INCOMPAT_VSN_DBG,
    "image file is incompatible with debugger - recompile the program",
    VMsg("This image file was created with a version of the compiler "
         "that is incompatible with this debugger.  Recompile the program "
         "with the compiler that's bundled with this debugger.  If no "
         "compiler is bundled, check the debugger release notes for "
         "information on which compiler to use. ") },

    { VMERR_NETWORK_SAFETY,
    "this operation is not allowed by the network safety level settings",
    VMsg("This operation is not allowed by the current network safety level "
         "settings. The program is attempting to access network features "
         "that you have disabled with the network safety level options. "
         "If you wish to allow this operation, you must restart the "
         "program with new network safety settings.") },

    { VMERR_INVALID_SETPROP,
    "property cannot be set for object",
    VMsg("Invalid property change - this property cannot be set for this "
         "object.  This normally indicates that the object is of a type "
         "that does not allow setting of properties at all, or at least "
         "of certain properties.  For example, a string object does not "
         "allow setting properties at all.") },

    { VMERR_NOT_SAVED_STATE,
    "file is not a valid saved state file",
    VMsg("This file is not a valid saved state file.  Either the file was "
         "not created as a saved state file, or its contents have been "
         "corrupted.") },

    { VMERR_WRONG_SAVED_STATE,
    "saved state is for a different program or a different version of "
        "this program",
        VMsg("This file does not contain saved state information for "
             "this program.  The file was saved by another program, or "
             "by a different version of this program; in either case, it "
             "cannot be restored with this version of this program.") },

    { VMERR_SAVED_META_TOO_LONG,
    "intrinsic class name in saved state file is too long",
    VMsg("An intrinsic class name in the saved state file is too long.  "
         "The file might be corrupted, or might have been saved by an "
         "incompatible version of the interpreter.") },

    { VMERR_SAVED_OBJ_ID_INVALID,
    "invalid object ID in saved state file",
    VMsg("The saved state file contains an invalid object ID.  The saved "
         "state file might be corrupted.") },

    { VMERR_BAD_SAVED_STATE,
    "saved state file is corrupted (incorrect checksum)",
    VMsg("The saved state file's checksum is invalid.  This usually "
         "indicates that the file has been corrupted (which could be "
         "due to a media error, modification by another application, "
         "or a file transfer that lost or changed data).") },

    { VMERR_STORAGE_SERVER_ERR,
    "storage server error",
    VMsg("An error occurred accessing the storage server. This could "
         "be due to a network problem, invalid user credentials, or "
         "a configuration problem on the game server.") },

    { VMERR_DESC_TAB_OVERFLOW,
    "saved file metadata table exceeds 64k bytes",
    VMsg("The metadata table for the saved state file is too large. "
         "This table is limited to 64k bytes in length.") },

    { VMERR_BAD_SAVED_META_DATA,
    "invalid intrinsic class data in saved state file",
    VMsg("The saved state file contains intrinsic class data that "
         "is not valid.  This usually means that the file was saved "
         "with an incompatible version of the interpreter program.") },

    { VMERR_NO_STR_CONV,
    "cannot convert value to string",
    VMsg("This value cannot be converted to a string.") },

    { VMERR_CONV_BUF_OVF,
    "string conversion buffer overflow",
    VMsg("An internal buffer overflow occurred converting this value to "
         "a string.") },

    { VMERR_BAD_TYPE_ADD,
    "invalid datatypes for addition operator",
    VMsg("Invalid datatypes for addition operator.  The values being added "
         "cannot be combined in this manner.") },

    { VMERR_NUM_VAL_REQD,
    "numeric value required",
    VMsg("Invalid value type - a numeric value is required.") },

    { VMERR_INT_VAL_REQD,
    "integer value required",
    VMsg("Invalid value type - an integer value is required.") },

    { VMERR_NO_LOG_CONV,
    "cannot convert value to logical (true/nil)",
    VMsg("This value cannot be converted to a logical (true/nil) value.") },

    { VMERR_BAD_TYPE_SUB,
    "invalid datatypes for subtraction operator",
    VMsg("Invalid datatypes for subtraction operator.  The values used "
         "cannot be combined in this manner.") },

    { VMERR_DIVIDE_BY_ZERO,
    "division by zero",
    VMsg("Arithmetic error - Division by zero.") },

    { VMERR_INVALID_COMPARISON,
    "invalid comparison",
    VMsg("Invalid comparison - these values cannot be compared "
         "to one another.") },

    { VMERR_OBJ_VAL_REQD,
    "object value required",
    VMsg("An object value is required.") },

    { VMERR_PROPPTR_VAL_REQD,
    "property pointer required",
    VMsg("A property pointer value is required.") },

    { VMERR_LOG_VAL_REQD,
    "logical value required",
    VMsg("A logical (true/nil) value is required.") },

    { VMERR_FUNCPTR_VAL_REQD,
    "function pointer required",
    VMsg("A function pointer value is required.") },

    { VMERR_CANNOT_INDEX_TYPE,
    "invalid index operation - this type of value cannot be indexed",
    VMsg("This type of value cannot be indexed.") },

    { VMERR_INDEX_OUT_OF_RANGE,
    "index out of range",
    VMsg("The index value is out of range for the value being indexed (it is "
         "too low or too high).") },

    { VMERR_BAD_METACLASS_INDEX,
    "invalid intrinsic class index",
    VMsg("The intrinsic class index is out of range.  This probably "
         "indicates that the image file is corrupted.") },

    { VMERR_BAD_DYNAMIC_NEW,
    "invalid dynamic object creation (intrinsic class does not support NEW)",
    VMsg("This type of object cannot be dynamically created, because the "
           "intrinsic class does not support dynamic creation.") },

    { VMERR_OBJ_VAL_REQD_SC,
    "object value required for base class",
    VMsg("An object value must be specified for the base class of a dynamic "
         "object creation operation.  The superclass value is of a "
         "non-object type.") },

    { VMERR_STRING_VAL_REQD,
    "string value required",
    VMsg("A string value is required.") },

    { VMERR_LIST_VAL_REQD,
    "list value required",
    VMsg("A list value is required.") },

    { VMERR_DICT_NO_CONST,
    "list or string reference found in dictionary (entry \"%s\") - this "
        "dictionary cannot be saved in the image file",
     VMsg("A dictionary entry (for the string \"%s\") referred to a string "
          "or list value for its associated value data.  This dictionary "
          "cannot be stored in the image file, so the image file cannot "
          "be created.  Check dictionary word additions and ensure that "
          "only objects are added to the dictionary.") },

    { VMERR_INVAL_OBJ_TYPE,
    "invalid object type - cannot convert to required object type",
    VMsg("An object is not of the correct type.  The object specified "
         "cannot be converted to the required object type.") },

    { VMERR_NUM_OVERFLOW,
    "numeric overflow",
    VMsg("A numeric calculation overflowed the limits of the datatype.") },

    { VMERR_BAD_TYPE_MUL, 
    "invalid datatypes for multiplication operator",
    VMsg("Invalid datatypes for multiplication operator.  The values "
         "being added cannot be combined in this manner.") },

    { VMERR_BAD_TYPE_DIV, 
    "invalid datatypes for division operator",
    VMsg("Invalid datatypes for division operator.  The values being added "
         "cannot be combined in this manner.") },

    { VMERR_BAD_TYPE_NEG,
    "invalid datatype for arithmetic negation operator",
    VMsg("Invalid datatype for arithmetic negation operator.  The value "
         "cannot be negated.") },

    { VMERR_OUT_OF_RANGE,
    "value is out of range",
    VMsg("A value that was outside of the legal range of inputs was "
         "specified for a calculation.") },

    { VMERR_STR_TOO_LONG,
    "string is too long",
    VMsg("A string value is limited to 65535 bytes in length.  This "
         "string exceeds the length limit.") },

    { VMERR_LIST_TOO_LONG,
    "list too long",
    VMsg("A list value is limited to about 13100 elements.  This list "
         "exceeds the limit.") },

    { VMERR_TREE_TOO_DEEP_EQ,
    "maximum equality test/hash recursion depth exceeded",
    VMsg("This equality comparison or hash calculation is too complex and "
         "cannot be performed.  This usually indicates that a value "
         "contains circular references, such as a Vector that contains "
         "a reference to itself, or to another Vector that contains a "
         "reference to the first one.  This type of value cannot be "
         "compared for equality or used in a LookupTable.") },

    { VMERR_NO_INT_CONV,
    "cannot convert value to integer",
    VMsg("This value cannot be converted to an integer.") },

    { VMERR_BAD_TYPE_MOD,
    "invalid datatype for modulo operator",
    VMsg("Invalid datatype for the modulo operator.  These values can't be "
         "combined with this operator.") },

    { VMERR_BAD_TYPE_BIT_AND,
    "invalid datatype for bitwise AND operator",
    VMsg("Invalid datatype for the bitwise AND operator.  These values can't "
         "be combined with this operator.") },

    { VMERR_BAD_TYPE_BIT_OR,
    "invalid datatype for bitwise OR operator",
    VMsg("Invalid datatype for the bitwise OR operator.  These values can't "
         "be combined with this operator.") },

    { VMERR_BAD_TYPE_XOR,
    "invalid datatype for XOR operator",
    VMsg("Invalid datatype for the XOR operator.  These values can't "
         "be combined with this operator.") },

    { VMERR_BAD_TYPE_SHL,
    "invalid datatype for left-shift operator '<<'",
    VMsg("Invalid datatype for the left-shift operator '<<'.  These values "
         "can't be combined with this operator.") },

    { VMERR_BAD_TYPE_ASHR,
    "invalid datatype for arithmetic right-shift operator '>>'",
    VMsg("Invalid datatype for the arithmetic right-shift operator '>>'. "
         "These values can't be combined with this operator.") },

    { VMERR_BAD_TYPE_BIT_NOT,
    "invalid datatype for bitwise NOT operator",
    VMsg("Invalid datatype for the bitwise NOT operator.  These values can't "
    "be combined with this operator.") },

    { VMERR_CODEPTR_VAL_REQD,
    "code pointer value required",
    VMsg("Invalid type - code pointer value required. (This probably "
         "indicates an internal problem in the interpreter.)") },

    { VMERR_EXCEPTION_OBJ_REQD,
    "exception object required, but 'new' did not yield an object",
    VMsg("The VM tried to construct a new program-defined exception object "
         "to represent a run-time error that occurred, but 'new' did not "
         "yield an object. Note that another underlying run-time error "
         "occurred that triggered the throw in the first place, but "
         "information on that error is not available now because of the "
         "problem creating the exception object to represent that error.") },

    { VMERR_NO_DOUBLE_CONV,
    "cannot convert value to native floating point",
    VMsg("The value cannot be converted to a floating-point type.") },

    { VMERR_NO_NUM_CONV,
    "cannot convert value to a numeric type",
    VMsg("The value cannot be converted to a numeric type. Only values "
         "that can be converted to integer or BigNumber can be used in "
         "this context.") },

    { VMERR_BAD_TYPE_LSHR,
    "invalid datatype for logical right-shift operator '>>>'",
    VMsg("Invalid datatype for the logical right-shift operator '>>>'. "
         "These values can't be combined with this operator.") },

    { VMERR_WRONG_NUM_OF_ARGS,
    "wrong number of arguments",
    VMsg("The wrong number of arguments was passed to a function or method "
         "in the invocation of the function or method.") },

    { VMERR_WRONG_NUM_OF_ARGS_CALLING,
    "argument mismatch calling %s - function definition is incorrect",
    VMsg("The number of arguments doesn't match the number expected "
         "calling %s.  Check the function or method and correct the "
         "number of parameters that it is declared to receive.") },

    { VMERR_NIL_DEREF,
    "nil object reference",
    VMsg("The value 'nil' was used to reference an object property.  Only "
         "valid object references can be used in property evaluations.") },

    { VMERR_MISSING_NAMED_ARG,
    "missing named argument '%s'",
    VMsg("The named argument '%s' was expected in a function or method "
         "call, but it wasn't provided by the caller.") }, 

    { VMERR_BAD_TYPE_CALL,
    "invalid type for call",
    VMsg("The value cannot be invoked as a method or function.") },

    { VMERR_NIL_SELF,
    "nil 'self' value is not allowed",
    VMsg("'self' cannot be nil.  The function or method context has "
         "a nil value for 'self', which is not allowed.") },

    { VMERR_CANNOT_CREATE_INST,
    "cannot create instance of object - object is not a class",
    VMsg("An instance of this object cannot be created, because this "
         "object is not a class.") },

    { VMERR_ILLEGAL_NEW,
    "cannot create instance - class does not allow dynamic construction",
    VMsg("An instance of this class cannot be created, because this class "
         "does not allow dynamic construction.") },

    { VMERR_INVALID_OPCODE,
    "invalid opcode - possible image file corruption",
    VMsg("Invalid instruction opcode - the image file might be corrupted.") },

    { VMERR_UNHANDLED_EXC,
    "unhandled exception",
    VMsg("An exception was thrown but was not caught by the program.  "
         "The interpreter is terminating execution of the program.") },

    { VMERR_STACK_OVERFLOW,
    "stack overflow",
    VMsg("Stack overflow.  This indicates that function or method calls "
         "were nested too deeply; this might have occurred because of "
         "unterminated recursion, which can happen when a function or method "
         "calls itself (either directly or indirectly).") },

    { VMERR_BAD_TYPE_BIF,
    "invalid type for intrinsic function argument",
    VMsg("An invalid datatype was provided for an intrinsic "
         "function argument.") },

    { VMERR_SAY_IS_NOT_DEFINED,
    "default output function is not defined",
    VMsg("The default output function is not defined.  Implicit string "
         "display is not allowed until a default output function "
         "is specified.") },

    { VMERR_BAD_VAL_BIF,
    "invalid value for intrinsic function argument",
    VMsg("An invalid value was specified for an intrinsic function argument.  "
         "The value is out of range or is not an allowed value.") },

    { VMERR_BREAKPOINT,
    "breakpoint encountered",
    VMsg("A breakpoint instruction was encountered, and no debugger is "
         "active.  The compiler might have inserted this breakpoint to "
         "indicate an invalid or unreachable location in the code, so "
         "executing this instruction probably indicates an error in the "
         "program.") },

    { VMERR_CALLEXT_NOT_IMPL,
    "external function calls are not implemented in this version",
    VMsg("This version of the interpreter does not implement external "
         "function calls.  This program requires an interpreter that "
         "provides external function call capabilities, so this program "
         "is not compatible with this interpreter.") },

    { VMERR_INVALID_OPCODE_MOD,
    "invalid opcode modifier - possible image file corruption",
    VMsg("Invalid instruction opcode modifier - the image file might "
         "be corrupted.") },

    /*
     *   Note - do NOT use the VMsg() macro on this message, since we always
     *   want to have this verbose message available. 
     */
    { VMERR_NO_CHARMAP_FILE,
    "No mapping file available for local character set \"%.32s\"",
    "[Warning: no mapping file is available for the local character set "
        "\"%.32s\".  The system will use a default ASCII character set "
        "mapping instead, so accented characters will be displayed without "
        "their accents.]" },

    { VMERR_UNHANDLED_EXC_PARAM,
    "Unhandled exception: %s",
    VMsg("Unhandled exception: %s") },

    { VMERR_VM_EXC_PARAM,
    "VM Error: %s",
    VMsg("VM Error: %s")
    VBook("This is used as a generic template for VM run-time "
    "exception messages.  The interpreter uses this to "
    "display unhandled exceptions that terminate the program.") },

    { VMERR_VM_EXC_CODE,
    "VM Error: code %d",
    VMsg("VM Error: code %d")
    VBook("This is used as a generic template for VM run-time "
    "exceptions.  The interpreter uses this to report unhandled "
    "exceptions that terminate the program.  When it can't find "
    "any message for the VM error code, the interpreter simply "
    "displays the error number using this template.") },
    
    { VMERR_EXC_IN_STATIC_INIT,
    "Exception in static initializer for %s.%s: %s",
    VMsg("An exception occurred in the static initializer for "
         "%s.%s: %s") },

    { VMERR_INTCLS_GENERAL_ERROR,
    "intrinsic class exception: %s",
    VMsg("Exception in intrinsic class method: %s") },

    { VMERR_STACK_OUT_OF_BOUNDS,
    "stack access is out of bounds",
    VMsg("The program attempted to access a stack location that isn't "
         "part of the current expression storage area.  This probably "
         "indicates a problem with the compiler that was used to create "
         "this program, or a corrupted program file.") },

    { VMERR_DBG_ABORT,
    "'abort' signal",
    VMsg("'abort' signal")
    VBook("This exception is used internally by the debugger to "
          "signal program termination via the debugger UI.") },

    { VMERR_DBG_RESTART,
    "'restart' signal",
    VMsg("'restart' signal")
    VBook("This exception is used internally by the debugger to "
          "signal program restart via the debugger UI.") },

    { VMERR_DBG_HALT,
    "debugger VM halt",
    VMsg("debugger VM halt")
    VBook("This exception is used internally by the debugger to "
          "signal program termination via the debugger UI.") },

    { VMERR_DBG_INTERRUPT,
    "interrupted by user",
    VMsg("The program was interrupted by a user interrupt key or "
         "other action.") },

    { VMERR_NO_DEBUGGER,
    "no debugger available",
    VMsg("An instruction was encountered that requires the debugger, "
         "but this interpreter version doesn't include debugging "
         "capaabilities.") },

    { VMERR_BAD_FRAME,
    "invalid frame in debugger local/parameter evaluation",
    VMsg("An invalid stack frame was specified in a debugger local/parameter "
         "evaluation.  This probably indicates an internal problem in the "
         "debugger.") },

    { VMERR_BAD_SPEC_EVAL,
    "invalid speculative expression",
    VMsg("This expression cannot be executed speculatively.  (This does not "
         "indicate a problem; it's merely an internal condition in the "
         "debugger.)") },

    { VMERR_INVAL_DBG_EXPR,
    "invalid debugger expression",
    VMsg("This expression cannot be evaluated in the debugger.") },

    { VMERR_NO_IMAGE_DBG_INFO,
    "image file has no debugging information - recompile for debugging",
    VMsg("The image file has no debugging information.  You must recompile "
         "the source code for this program with debugging information in "
         "order to run the program under the debugger.") },

    { VMERR_BIGNUM_NO_REGS,
    "out of temporary floating point registers (calculation too complex)",
    VMsg("The interpreter is out of temporary floating point registers.  "
         "This probably indicates that an excessively complex calculation "
         "has been attempted.") },

    { VMERR_NO_BIGNUM_CONV,
    "cannot convert value to BigNumber",
    VMsg("This value cannot be converted to a BigNumber.") }

#endif /* VMERR_OMIT_MESSAGES */
};

/* size of the english message array */
size_t vm_message_count_english =
    sizeof(vm_messages_english)/sizeof(vm_messages_english[0]);

/* 
 *   the actual message array - we'll initialize this to the
 *   english list that's linked in, but if we find an external message
 *   file, we'll use the external file instead 
 */
const err_msg_t *vm_messages = vm_messages_english;

/* message count - initialize to the english message array count */
size_t vm_message_count =
    sizeof(vm_messages_english)/sizeof(vm_messages_english[0]);

/* ------------------------------------------------------------------------ */
/*
 *   we don't need the VMsg() (verbose message) cover macro any more 
 */
#undef VMsg
