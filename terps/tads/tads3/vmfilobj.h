/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmfilobj.h - File object metaclass
Function
  Implements an intrinsic class interface to operating system file I/O.
Notes
  
Modified
  06/28/01 MJRoberts  - Creation
*/

#ifndef VMFILOBJ_H
#define VMFILOBJ_H

#include "os.h"
#include "utf8.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmstr.h"
#include "vmdatasrc.h"


/* ------------------------------------------------------------------------ */
/*
 *   File intrinsic class.  Our extension keeps track of our data source (our
 *   abstract file handle), our character set object, and our flags.
 *   
 *   CVmDataSource *fp
 *.  objid charset
 *.  byte mode (text/data/raw)
 *.  byte access (read/write/both)
 *.  uint32 flags
 *   
 *   Our image file data leaves out the file handle, since it can't be
 *   loaded:
 *   
 *   objid charset
 *.  byte mode
 *.  byte access
 *.  uint32 flags
 *   
 *   An open file is inherently transient, so its state cannot be loaded,
 *   restored, or undone.  
 */

class CVmObjFile: public CVmObject
{
    friend class CVmMetaclassFile;

public:
    /*
     *   Special filename designators.  These integer values can be passed to
     *   the Open methods (from bytecode) in place of the filename string.
     *   These designate system files whose actual location is determined by
     *   the interpreter.
     *   
     *   The special file mechanism is designed to give games a well-defined
     *   portable way to access global, cross-game files outside of the game
     *   sandbox, while still protecting the local file system against
     *   malicious (or just buggy) games.  The actual system path and
     *   filename of a given special file is determined by the interpreter,
     *   so games can't use this to access arbitrary files - only the
     *   designated special files are accessible.
     *   
     *   Note that the integer values defined here must not be changed in
     *   future releases, since they're a published part of the API - i.e.,
     *   bytecode programs hard-code the integer values.  
     */

    /* 
     *   Library defaults file.  This is defined as a system file, common to
     *   all games, where we can store global cross-game preference settings.
     *   This is generally stored in the interpreter install directory.  
     */
    static const int32_t SFID_LIB_DEFAULTS = 1;

    /*
     *   Web UI preferences file.  This is a system file common to all games
     *   that stores the UI settings for the Web UI client.  
     */
    static const int32_t SFID_WEBUI_PREFS = 2;


    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObject::is_of_metaclass(meta));
    }

    /* is 'obj' a File object? */
    static int is_file_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* create from stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* reserve constant data */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *,
                                    vm_obj_id_t /*self*/)
    {
        /* we can't be converted to constant data */
    }

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *,
                                       vm_obj_id_t /*self*/)
    {
        /* 
         *   we can't be converted to constant data and reference nothing
         *   that can be converted to constant data 
         */
    }

    /* create an empty file object */
    static vm_obj_id_t create(VMG_ int in_root_set);

    /* create with the given character set and file handle */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              class CVmNetFile *netfile,
                              vm_obj_id_t charset, CVmDataSource *fp,
                              int mode, int access, int create_readbuf);

    /* get the data source for the file */
    class CVmDataSource *get_datasource() const;

    /* get the filespec used to create the file */
    void get_filespec(vm_val_t *val) const;

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* call a static property */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop);

    /* undo operations */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *) { }

    /* mark references */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }
    void mark_refs(VMG_ uint state);

    /* we keep no weak references */
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }
    void remove_stale_weak_refs(VMG0_) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t, const char *ptr, size_t);

    /* reload from an image file (on restart) */
    void reload_from_image(VMG_ vm_obj_id_t, const char *ptr, size_t);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* evaluate a property */
    virtual int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                         vm_obj_id_t self, vm_obj_id_t *source_obj,
                         uint *argc);

    /*
     *   Translate a filename argument to a netfile object.  Checks file
     *   safety against the given access mode.
     */
    static CVmNetFile *get_filename_arg(
        VMG_ const vm_val_t *arg, const vm_rcdesc *rc,
        int access, int is_resource_file,
        os_filetype_t file_type, const char *mime_type);

    /* 
     *   get the filename from an object argument; this can only be used for
     *   regular files (not resources) 
     */
    static CVmNetFile *get_filename_from_obj(
        VMG_ vm_obj_id_t obj, const vm_rcdesc *rc, int access,
        os_filetype_t file_type, const char *mime_type);

    /* resolve a special file ID to a local path */
    static int sfid_to_path(VMG_ char *buf, size_t buflen, int32_t sfid);

    /* 
     *   Check the safety settings to determine if an open is allowed on the
     *   given file with the given access mode.  If the access is not
     *   allowed, we'll throw an error.  
     */
    static void check_safety_for_open(VMG_ class CVmNetFile *f, int access);

    /* check the safety settings for opening the given local file path */
    static void check_safety_for_open(VMG_ const char *lclfname, int access);
    static int query_safety_for_open(VMG_ const char *lclfname, int access);

    /* get the file's size; returns -1 if the file isn't open or valid */
    long get_file_size(VMG0_);

    /* get the mode - returns a VMOBJFILE_MODE_xxx value */
    int get_file_mode(VMG0_);

    /* seek; returns true on success, false on failure */
    int set_pos(VMG_ long pos);

    /* 
     *   Read the file, storing the result in the caller's buffer.  The
     *   request is a suggested read size; we'll attempt to fill this as
     *   closely as possible, but we might return less than requested (never
     *   more) due to reaching the end of the file or due to character
     *   conversions.  Returns true on success, false on failure.
     */
    int read_file(VMG_ char *buf, int32_t &len);

protected:
    /* create with no initial contents */
    CVmObjFile() { ext_ = 0; }

    /* create with the given character set and file object */
    CVmObjFile(VMG_ class CVmNetFile *netfile, vm_obj_id_t charset,
               CVmDataSource *fp, int mode, int access, int create_readbuf);

    /* allocate our extension */
    void alloc_ext(VMG_ class CVmNetFile *netfile,
                   vm_obj_id_t charset, CVmDataSource *fp,
                   unsigned long flags, int mode, int access,
                   int create_readbuf);

    /* load or reload data from the image file */
    void load_image_data(VMG_ const char *ptr, size_t siz);

    /* property evaluator - undefined property */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - get my character set  */
    int getp_get_charset(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - set my character set */
    int getp_set_charset(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - close the file */
    int getp_close_file(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - read from the file */
    int getp_read_file(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - write to the file */
    int getp_write_file(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - read bytes from the file */
    int getp_read_bytes(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - write bytes to the file */
    int getp_write_bytes(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - get seek position */
    int getp_get_pos(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - set seek position */
    int getp_set_pos(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get the seek position */
    long get_pos(VMG0_);

    /* property evaluator - set seek position to end of file */
    int getp_set_pos_end(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - openFileText */
    int getp_open_text(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - openFileData */
    int getp_open_data(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - openFileRaw */
    int getp_open_raw(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - openResourceText */
    int getp_open_res_text(VMG_ vm_obj_id_t self, vm_val_t *ret, uint *argc);

    /* property evaluator - openResourceRaw */
    int getp_open_res_raw(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - get size */
    int getp_get_size(VMG_ vm_obj_id_t self, vm_val_t *ret, uint *argc);

    /* get the file's mode */
    int getp_get_mode(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* static property evaluators for the openFileXxx creator methods */
    static int s_getp_open_text(VMG_ vm_val_t *retval, uint *argc,
                                int is_resource_file,
                                const struct vm_rcdesc *rc);
    static int s_getp_open_data(VMG_ vm_val_t *retval, uint *argc,
                                const struct vm_rcdesc *rc);
    static int s_getp_open_raw(VMG_ vm_val_t *retval, uint *argc,
                               int is_resource_file,
                               const struct vm_rcdesc *rc);

    /* property evaluator - getRootName */
    int getp_getRootName(VMG_ vm_obj_id_t, vm_val_t *retval, uint *argc)
        { return s_getp_getRootName(vmg_ retval, argc); }

    /* static property evaluator for getRootName */
    static int s_getp_getRootName(VMG_ vm_val_t *retval, uint *argc);

    /* property evaluator - deleteFile */
    int getp_deleteFile(VMG_ vm_obj_id_t, vm_val_t *retval, uint *argc)
        { return s_getp_deleteFile(vmg_ retval, argc); }

    /* static property evaluator for deleteFile */
    static int s_getp_deleteFile(VMG_ vm_val_t *retval, uint *argc);

    /* property evaluator - setMode */
    int getp_setMode(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - packBytes */
    int getp_packBytes(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - unpackBytes */
    int getp_unpackBytes(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - sha256 */
    int getp_sha256(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - digestMD5 */
    int getp_digestMD5(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* retrieve the filename and access arguments for an 'open' method */
    static class CVmNetFile *get_filename_and_access(
        VMG_ const struct vm_rcdesc *rc, int *access, int is_resource_file,
        os_filetype_t file_type, const char *mime_type);

    /* pop a character set mapper argument */
    static vm_obj_id_t get_charset_arg(VMG_ int argn, int argc);

    /* generic binary file opener - common to 'raw' and 'data' files */
    static int open_binary(VMG_ vm_val_t *retval, uint *argc, int mode,
                           int is_resource_file, const struct vm_rcdesc *rc);

    /* read a value in text/data mode */
    void read_text_mode(VMG_ vm_val_t *retval);
    void read_data_mode(VMG_ vm_val_t *retval);

    /* write a value in text/data mode */
    void write_text_mode(VMG_ const vm_val_t *val);
    void write_data_mode(VMG_ const vm_val_t *val);

    /* 
     *   check to make sure we can perform operations on the file - if we're
     *   in the 'out of sync' state, we'll throw an exception
     */
    void check_valid_file(VMG0_);

    /* 
     *   check that we have read or write access to the file - throws a mode
     *   exception if we don't have the requested access mode 
     */
    void check_read_access(VMG0_);
    void check_write_access(VMG0_);

    /* check that we're open in raw mode */
    void check_raw_mode(VMG0_);

    /* check that we're valid, in raw mode, and read/write */
    void check_raw_read(VMG0_);
    void check_raw_write(VMG0_);

    /*
     *   Note a file seek position change.
     *   
     *   'is_explicit' indicates whether the seek is an explicit osfseek()
     *   operation or simply a change in position due to a read or write.  If
     *   we're explicitly seeking to a new location with osfseek(),
     *   'is_explicit' is true; if we're simply reading or writing, which
     *   affects the file position implicitly, 'is_explicit' is false.  
     */
    void note_file_seek(VMG_ int is_explicit);

    /* 
     *   check for a switch between reading and writing, flushing our
     *   underlying stdio buffers if necessary 
     */
    void switch_read_write_mode(VMG_ int writing);

    /* get my extension, properly cast */
    struct vmobjfile_ext_t *get_ext() const
        { return (struct vmobjfile_ext_t *)ext_; }

    /* property evaluation function table */
    static int (CVmObjFile::*func_table_[])(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);
};

/*
 *   File object extension 
 */
struct vmobjfile_ext_t
{
    /* 
     *   Our network/local file descriptor.  This handles network storage
     *   server operations when we're running in web server mode. 
     */
    class CVmNetFile *netfile;
    
    /* our abstract data source (usually an OS file) */
    CVmDataSource *fp;

    /* our character set object */
    vm_obj_id_t charset;

    /* format mode */
    unsigned char mode;

    /* read/write access */
    unsigned char access;

    /* flags */
    unsigned long flags;

    /* read buffer, if we have one */
    struct vmobjfile_readbuf_t *readbuf;
};

/*
 *   File object text read buffer.  This is attached to the file extension
 *   if the file is opened in text mode with read access.  
 */
struct vmobjfile_readbuf_t
{
    /* current read pointer */
    char *ptr;

    /* bytes remaining in read buffer */
    size_t rem;

    /* read buffer */
    char buf[512];

    /* read a character */
    int getch(wchar_t &ch, CVmDataSource *fp, class CCharmapToUni *charmap);

    /* peek at the next character */
    int peekch(wchar_t &ch, CVmDataSource *fp, class CCharmapToUni *charmap,
               size_t &in_bytes);

    /* commit a 'peekch' */
    void commit_peek(size_t last_in_bytes)
    {
        ptr += last_in_bytes;
        rem -= last_in_bytes;
    }

    /* refill the buffer, if it's empty */
    int refill(CVmDataSource *fp);
};

/* 
 *   file extension flags 
 */

/* 
 *   File out of sync.  This is set when we load a File object from an image
 *   file; since the file cannot remain open across a preinit-save-load
 *   cycle, no operations can be performed on the File object after it's
 *   loaded from an image file.  
 */
#define VMOBJFILE_OUT_OF_SYNC       0x0001

/* 
 *   last operation left the underlying stdio buffers dirty, so we must take
 *   care to seek explicitly in the file if changing read/write mode on the
 *   next operation 
 */
#define VMOBJFILE_STDIO_BUF_DIRTY   0x0002

/* 
 *   last operation was 'write' - we must keep track of the last operation
 *   so that we will know to explicitly seek when switching between read and
 *   write operations 
 */
#define VMOBJFILE_LAST_OP_WRITE     0x0004


/*
 *   modes 
 */
#define VMOBJFILE_MODE_TEXT   0x01
#define VMOBJFILE_MODE_DATA   0x02
#define VMOBJFILE_MODE_RAW    0x03

/*
 *   access types
 */
#define VMOBJFILE_ACCESS_READ     0x0001
#define VMOBJFILE_ACCESS_WRITE    0x0002
#define VMOBJFILE_ACCESS_RW_KEEP  0x0003
#define VMOBJFILE_ACCESS_RW_TRUNC 0x0004
#define VMOBJFILE_ACCESS_USERMAX  0x0004

/* internal access types - not allowed as user arguments */
#define VMOBJFILE_ACCESS_INTERNAL 0x1000
#define VMOBJFILE_ACCESS_DELETE   0x1000                     /* delete file */
#define VMOBJFILE_ACCESS_GETINFO  0x1001              /* get file type/info */
#define VMOBJFILE_ACCESS_MKDIR    0x1002                           /* mkdir */
#define VMOBJFILE_ACCESS_RMDIR    0x1003                           /* rmdir */
#define VMOBJFILE_ACCESS_READDIR  0x1004   /* read a directory (list files) */
#define VMOBJFILE_ACCESS_RENAME_FROM 0x1005        /* rename FROM this name */
#define VMOBJFILE_ACCESS_RENAME_TO   0x1006          /* rename TO this name */


/* ------------------------------------------------------------------------ */
/*
 *   'Data' mode type tags.  Each item in a data-mode file is written with
 *   a byte giving the type tag, followed immediately by the data value.
 *   
 *   For compatibility with TADS 2 'binary' mode files, we use the same
 *   type tags that TADS 2 used for types in common.  We are upwardly
 *   compatible with the TADS 2 format: we recognize all TADS 2 type tags,
 *   but we provide support for additional types that TADS 2 does not use.
 *   All of the tag values below 0x20 are compatible with TADS 2; those
 *   tag values 0x20 and above are not backwards compatible, so files that
 *   include these values will not be readable by TADS 2 programs.  
 */

/* 
 *   integer - 32-bit int in little-endian format (4 bytes) - TADS 2
 *   compatible 
 */
#define VMOBJFILE_TAG_INT       0x01

/* 
 *   string - 16-bit byte-length prefix in little-endian format followed
 *   by the bytes of the string, in UTF-8 format - TADS 2 compatible,
 *   although any string which includes characters outside of the ASCII
 *   set (Unicode code points 0 to 127 inclusive) will not be properly
 *   interpreted by TADS 2 programs.
 *   
 *   For compatibility with TADS 2, the length prefix includes in its byte
 *   count the two bytes of the length prefix itself, so this value is
 *   actually two higher than the number of bytes in the string proper.  
 */
#define VMOBJFILE_TAG_STRING    0x03

/* true - no associated value - TADS 2 compatible */
#define VMOBJFILE_TAG_TRUE      0x08

/* enum - 32-bit enum value in little-endian format */
#define VMOBJFILE_TAG_ENUM      0x20

/* 
 *   BigNumber - 16-bit length prefix in little-endian format followed by
 *   the bytes of the BigNumber 
 */
#define VMOBJFILE_TAG_BIGNUM    0x21

/* 
 *   ByteArray - 32-bit length prefix in little-endian format followed by
 *   the bytes of the byte array 
 */
#define VMOBJFILE_TAG_BYTEARRAY 0x22


/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassFile: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "file/030003"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjFile();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjFile();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjFile::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjFile::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};


#endif /* VMFILOBJ_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjFile)
