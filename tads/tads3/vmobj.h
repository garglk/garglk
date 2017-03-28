/* $Header: d:/cvsroot/tads/tads3/VMOBJ.H,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmobj.h - VM object memory manager
Function
  
Notes
  
Modified
  10/20/98 MJRoberts  - Creation
*/

/* define this symbol to collect and display GC statistics, for tuning */
// #define VMOBJ_GC_STATS

#ifndef VMOBJ_H
#define VMOBJ_H

#include <stdlib.h>
#include <memory.h>

#include "vmglob.h"
#include "vmtype.h"
#include "vmerr.h"
#include "vmerrnum.h"


/* ------------------------------------------------------------------------ */
/*
 *   GC statistics conditional compilation macro
 */
#ifdef VMOBJ_GC_STATS
# define IF_GC_STATS(x) x
#else
# define IF_GC_STATS(x)
#endif


/* ------------------------------------------------------------------------ */
/*
 *   If the VM_REGISTER_METACLASS macro hasn't been defined, define it
 *   now.  This macro is defined different ways on different inclusions,
 *   but when it's not defined, it should always be defined to do nothing.
 */
#ifndef VM_REGISTER_METACLASS
# define VM_REGISTER_METACLASS(metaclass)
#endif

/* ------------------------------------------------------------------------ */
/*
 *   NOTE TO METACLASS IMPLEMENTORS - each final concrete class derived
 *   from CVmObject that is to be available to VM programs for static
 *   image file loading and/or dynamic creation (via the "NEW"
 *   instructions) must specify a VM_REGISTER_METACLASS declaration to
 *   register the metaclass.  This declaration must occur in the
 *   metaclass's header file, and must be OUTSIDE of the region protected
 *   against multiple inclusion.  The definition should look like this:
 *   
 *   VM_REGISTER_METACLASS("metaclass-string-name", CVmClassName)
 *   
 *   The string name must be the universally unique metaclass identifier
 *   string registered with the T3 VM Specification Maintainer.  The class
 *   name is the C++ class (derived from CVmObject).
 *   
 *   Each CVmObject subclass must define certain static construction
 *   methods in addition to the virtual methods defined as abstract in
 *   CVmObject.  See the comments on CVmObject for details.
 */


/* ------------------------------------------------------------------------ */
/*
 *   To get a pointer to an object (CVmObject *) given an object ID
 *   (vm_obj_id_t), use this:
 *   
 *   CVmObject *obj = vm_objp(vmg_ id);
 *   
 *   To allocate a new object:
 *   
 *   vm_obj_id_t id = vm_newid(vmg_ in_root_set);
 *.  CVmObject *obj = new (vmg_ id) CVmObjXxx(constructor params);
 *   
 *   The functions vm_objp() and vm_newid() are defined later in this file.  
 */


/* ------------------------------------------------------------------------ */
/*
 *   Garbage collector work increment.  This is the number of objects that
 *   the GC will process on each call to gc_pass_queue().
 *   
 *   The point of running the GC incrementally is to allow GC work to be
 *   interleaved with long-running user I/O operations (such as reading a
 *   line of text from the keyboard) in the foreground thread, so the work
 *   increment should be chosen so that each call to this routine
 *   completes quickly enough that the user will perceive no delay.
 *   However, making this number too small will introduce additional
 *   overhead by making an excessive number of function calls.  
 */
const int VM_GC_WORK_INCREMENT = 500;



/* ------------------------------------------------------------------------ */
/* 
 *   flag values for propDefined()
 */
#define VMOBJ_PROPDEF_ANY           1
#define VMOBJ_PROPDEF_DIRECTLY      2
#define VMOBJ_PROPDEF_INHERITS      3
#define VMOBJ_PROPDEF_GET_CLASS     4

/*
 *   flag values for explicit_to_string()
 */
#define TOSTR_UNSIGNED  0x0001           /* treat VM_INT values as unsigned */
#define TOSTR_ROUND     0x0002    /* round floating point values to integer */


/* ------------------------------------------------------------------------ */
/*
 *   Objects are composed of two parts.  The first is a fixed-size
 *   portion, or header, which consists of an abstract interface (in terms
 *   of stored data, this is simply a vtable pointer) and an extension
 *   pointer.  The extension pointer points to the variable-size second
 *   part of the object, which contains all of the additional instance
 *   data for the object.  
 */

/* ------------------------------------------------------------------------ */
/*
 *   
 *   The fixed-size parts, or object headers, are allocated from a set of
 *   arrays.  A master array has a pointer to the sub-arrays, and each
 *   sub-array has a fixed number of slots for the fixed-size parts.
 *   Thus, it's very fast to find an object header given an object ID.
 *   
 *   Because each fixed-size part has an abstract interface, we can have
 *   multiple implementations for the objects.  This would allow us, for
 *   example, to include native objects (with an appropriate interface) as
 *   though they were normal VM objects.  It also allows us to have
 *   different types of implementations for VM objects.
 *   
 *   The fixed-size object parts never move in memory, so we can refer to
 *   them with pointers.  We can efficiently re-use space as object
 *   headers are allocated and deleted, because a new object will always
 *   take up exactly the same size as a previous object whose space was
 *   freed.
 *   
 *   The fixed-size objects are always allocated within the page table,
 *   thus operator new is overridden to allocate memory within the page
 *   table.  
 */
/*
 *   In addition to the virtual methods listed, every object must define a
 *   data member as follows:
 *   
 *   public: static class CVmMetaclass *metaclass_reg_;
 *   
 *   This must be a static singleton instance of the CVmMetaclass subclass
 *   (see below) for the CVmObject subclass.  Each CVmObject subclass must
 *   have a corresponding CVmMetaclass subclass; the singleton member
 *   variable metaclass_reg_ provides the registration table entry that
 *   allows instances of this object to be dynamically linked from the
 *   image file.  
 */
class CVmObject
{
    friend class CVmVarHeap;
    
public:
    /* metaclass registration object (for the root object implementation) */
    static class CVmMetaclass *metaclass_reg_;

    /*
     *   Default implementation for calling a static property.  We don't
     *   have any static properties at this level, so we'll simply return
     *   false to indicate that the property wasn't evaluated.  
     */
    static int call_stat_prop(VMG_ vm_val_t *retval, const uchar **pc_ptr,
                              uint *argc, vm_prop_id_t);

    /* get the registration object for this metaclass */
    virtual class CVmMetaclass *get_metaclass_reg() const
        { return metaclass_reg_; }

    /* 
     *   Get the image file version string for my metaclass object.  This is
     *   the version of the metaclass that the image file actually depends
     *   upon, which might be an earlier version than the one actually
     *   present in the implementation.  This can be used for varying
     *   behavior to preserve compatibility with older versions.  
     */
    const char *get_image_file_version(VMG0_) const;

    /*
     *   Is the image file version of the metaclass AT LEAST the given
     *   version?  This returns true if the image file is dependent upon a
     *   version of the metaclass equal to or later than (higher than) the
     *   given version string.  The string should be given in the usual
     *   metaclass format - "030001", for example.  
     */
    int image_file_version_ge(VMG_ const char *ver)
        { return strcmp(get_image_file_version(vmg0_), ver) >= 0; }

    /* 
     *   Is this object of the given metaclass?  Returns true if the
     *   object is an instance of 'meta' or inherits from the metaclass.
     *   Each object class must override this.  
     */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
        { return (meta == metaclass_reg_); }

    /* 
     *   Receive notification that this object is being deleted - the
     *   garbage collector calls this function when the object is
     *   unreachable.
     *   
     *   Note that we don't use the real destructor, since we use our own
     *   memory management; instead, we have this virtual finalizer that
     *   we explicitly call when it's time to delete the object.  (This
     *   isn't entirely symmetrical with the overridden operator new, but
     *   the GC is the only code that can delete objects, and this saves
     *   us the trouble of overriding operator delete for the object.)  
     */
    virtual void notify_delete(VMG_ int in_root_set) = 0;

    /*
     *   Create an instance of this class.  If this object does not
     *   represent a class, or cannot be instanced, throw an error.  By
     *   default, objects cannot be instanced, so we'll simply throw an
     *   error.  If successful, leaves the new object in register R0.
     *   
     *   Parameters to the constructor are passed on the VM stack; 'argc'
     *   gives the number of arguments on the stack.  This routine will
     *   remove the arguments from the stack before returning.  
     */
    virtual void create_instance(VMG_ vm_obj_id_t self,
                                 const uchar **pc_ptr, uint argc)
    {
        /* throw the error */
        err_throw(VMERR_CANNOT_CREATE_INST);
    }

    /*
     *   Determine if the object has a non-trivial finalizer.  Returns
     *   true if the object has a non-trivial finalizer, false if it has
     *   no finalizer or the finalizer is trivial and hence can be
     *   ignored.  We'll return false by default.  
     */
    virtual int has_finalizer(VMG_ vm_obj_id_t /*self*/)
        { return FALSE; }

    /*
     *   Invoke the object's finalizer.  This need do nothing if the
     *   object does not define or inherit a finalizer method.  Any
     *   exceptions thrown in the course of executing the finalizer should
     *   be caught and discarded.  By default, we'll do nothing at all.
     */
    virtual void invoke_finalizer(VMG_ vm_obj_id_t /*self*/) { }

    /*
     *   Determine if this is a class object.  This returns true if this
     *   object is a class, false if it's an instance.  
     */
    virtual int is_class_object(VMG_ vm_obj_id_t /*self*/) const
        { return FALSE; }

    /*
     *   Determine if this object is an instance of another object.
     *   Returns true if this object derives from the other object,
     *   directly or indirectly.  If this object derives from an object
     *   which in turn derives from the given object, then this object
     *   derives (indirectly) from the given object.  
     */
    virtual int is_instance_of(VMG_ vm_obj_id_t obj);

    /* 
     *   Get the number of superclasses of the object, and get the nth
     *   superclass.  By default, we have one superclass, which is the
     *   IntrinsicClass object that represents this metaclass.  
     */
    virtual int get_superclass_count(VMG_ vm_obj_id_t /*self*/) const
        { return 1; }
    virtual vm_obj_id_t get_superclass(VMG_ vm_obj_id_t /*self*/,
                                       int /*superclass_index*/) const;

    /*
     *   Determine if the object has properties that can be enumerated.
     *   Returns true if so, false if not.  Note that this should return
     *   true for an object of a type that provides properties, even if
     *   the instance happens to have zero properties.  
     */
    virtual int provides_props(VMG0_) const { return FALSE; }

    /*
     *   Enumerate properties of the object.  Invoke the callback for each
     *   property.  The callback is not permitted to make any changes to
     *   the object or its properties and should not invoke garbage
     *   collection.  
     */
    virtual void enum_props(VMG_ vm_obj_id_t self,
                            void (*cb)(VMG_ void *ctx,
                                       vm_obj_id_t self, vm_prop_id_t prop,
                                       const vm_val_t *val),
                            void *cbctx)
    {
        /* by default, assume we have nothing to enumerate */
    }

    /* set a property value */
    virtual void set_prop(VMG_ class CVmUndo *undo,
                          vm_obj_id_t self, vm_prop_id_t prop,
                          const vm_val_t *val) = 0;

    /* 
     *   Get a property value.  We do not evaluate the property, but
     *   merely get the raw value; thus, if the property value is of type
     *   code, we simply retrieve the code offset pointer.  Returns true
     *   if the property was found, false if not.
     *   
     *   If we find the object, we'll set *source_obj to the ID of the
     *   object in which we found the object.  If the property was
     *   supplied by this object directly, we'll simply set source_id to
     *   self; if we inherited the property from a superclass, we'll set
     *   *source_id to the ID of the superclass object that actually
     *   supplied the value.
     *   
     *   If 'argc' is not null, this function can consume arguments from
     *   the run-time stack, and set *argc to zero to indicate that the
     *   arguments have been consumed.
     *   
     *   If 'argc' is null, and evaluating the property would involve
     *   running system code (with or without consuming arguments), this
     *   function should return TRUE, but should NOT run the system code -
     *   instead, set val->typ to VM_NATIVE_CODE to indicate that the
     *   value is a "native code" value (i.e., evaluating it requires
     *   executing system code).  
     */
    virtual int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                         vm_obj_id_t self, vm_obj_id_t *source_obj,
                         uint *argc);

    /*
     *   Get the invocation routine.  This retrieves the bytecode location to
     *   invoke when calling the object as though it were a function.  If the
     *   object has a function interface, set up a FUNCPTR or CODEPTR value
     *   to point to the code to invoke.  Regular objects aren't invokable.
     *   
     *   Returns true if this is an invokable object, false if not.  'val'
     *   can be a null pointer if the caller merely wishes to determine
     *   whether or not this object is invokable.  
     */
    virtual int get_invoker(VMG_ vm_val_t *val) { return FALSE; }

    /* get the interface to the given property */
    int get_prop_interface(VMG_ vm_obj_id_t self, vm_prop_id_t prop,
                           int &min_args, int &opt_args, int &varargs);

    /*
     *   Inherit a property value.  This works similarly to get_prop, but
     *   finds an inherited definition of the property, as though
     *   orig_target_obj.prop (and anything overriding orig_target_obj.prop)
     *   were undefined.
     *   
     *   In broad terms, the algorithm for this method is to do the same
     *   thing as get_prop(), but to ignore every definition of the property
     *   found in the class tree until after reaching and skipping
     *   orig_target_obj.prop.  Once orig_target_obj.prop is found, this
     *   method simply continues searching in the same manner as get_prop()
     *   and returns the next definition it finds.
     *   
     *   'defining_obj' is the object containing the method currently
     *   running.  This is not necessarily 'self', because the method
     *   currently running might already have been inherited from a
     *   superclass of 'self'.
     *   
     *   'orig_target_obj' is the object that was originally targeted for the
     *   get_prop() operation that invoked the calling method (or invoked the
     *   method that inherited the calling method, or so on).  This gives us
     *   the starting point in the search, so that we can continue the
     *   original inheritance tree search that started with get_prop().
     *   
     *   'argc' has the same meaning as for get_prop().  
     *   
     *   Objects that cannot be subclassed via byte-code can ignore this
     *   method.  The base class implementation follows the inheritance chain
     *   of "modifier" objects, which allow byte-code methods to be plugged
     *   in under the native code class tree.  
     */
    virtual int inh_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                         vm_obj_id_t self, vm_obj_id_t orig_target_obj,
                         vm_obj_id_t defining_obj, vm_obj_id_t *source_obj,
                         uint *argc);

    /*
     *   Build a list of the properties directly defined on this object
     *   instance.  On return, retval must be filled in with the new list
     *   object.
     *   
     *   Note that a self-reference must be pushed by the caller to protect
     *   against garbage collection of self while this routine is running.
     *   
     *   Most object types do not define any properties in the instance, so
     *   this default implementation will simply return an empty list.
     *   Classes that can define properties in instances (such as
     *   TadsObjects), and the "intrinsic class" class that represents
     *   classes, must override this to build their lists.  
     */
    virtual void build_prop_list(VMG_ vm_obj_id_t self, vm_val_t *retval);

    /* 
     *   Mark all strongly-referenced objects.  Calls
     *   obj_table->mark_refs() for each referenced object.  
     */
    virtual void mark_refs(VMG_ uint state) = 0;

    /* 
     *   Remove stale weak references.  For each weakly-referenced object,
     *   check to see if the object is marked as reachable; if not, it's
     *   about to be deleted, so forget the weak reference. 
     */
    virtual void remove_stale_weak_refs(VMG0_) = 0;

    /*
     *   Receive notification that the undo manager is creating a new
     *   savepoint.  
     */
    virtual void notify_new_savept() = 0;

    /* 
     *   apply an undo record created by this object; if the record has
     *   any additional data associated with it (allocated by the object
     *   when the undo record was created), this should also discard the
     *   additional data 
     */
    virtual void apply_undo(VMG_ struct CVmUndoRecord *rec) = 0;

    /* 
     *   Discard any extra information associated with this undo record.
     *   Note that this will not be called if apply_undo() is called,
     *   since apply_undo() is expected to discard any extra information
     *   itself after applying the record.  
     */
    virtual void discard_undo(VMG_ struct CVmUndoRecord *) { }

    /*
     *   Mark an object object reference.  If this object keeps strong
     *   references, this should mark any object contained in the undo
     *   record's saved value as referenced; if this object keeps only
     *   weak references, this doesn't need to do anything. 
     */
    virtual void mark_undo_ref(VMG_ struct CVmUndoRecord *rec) = 0;

    /*
     *   Remove any stale weak reference contained in an undo record.  For
     *   objects with ordinary strong references, this doesn't need to do
     *   anything.  For objects that keep weak references to other
     *   objects, this should check the object referenced in the undo
     *   record, if any, to determine if the object is about to be
     *   deleted, and if so clear the undo record (by setting the object
     *   in the old value to 'invalid'). 
     */
    virtual void remove_stale_undo_weak_ref(VMG_
                                            struct CVmUndoRecord *rec) = 0;

    /*
     *   Post-load initialization.  This routine is called only if the object
     *   specifically requests this by calling request_post_load_init().
     *   This routine is called exactly once for each initial program load,
     *   restart, or restore, and is called after ALL objects are loaded.
     *   
     *   The purpose of this routine is to allow an object to perform
     *   initializations that depend upon other objects.  During the normal
     *   load/restore/reset methods, an object cannot assume that any other
     *   objects are already loaded, because the order of loading is
     *   arbitrary.  When this routine is called, it is guaranteed that all
     *   objects are already loaded.  
     */
    virtual void post_load_init(VMG_ vm_obj_id_t /*self*/)
    {
        /* we do nothing by default */
    }

    /*
     *   Load the object from an image file.  The object's data block is
     *   at the given address and has the given size.
     *   
     *   The underlying memory is owned by the image file loader.  The
     *   object must not attempt to deallocate this memory.  
     */
    virtual void load_from_image(VMG_ vm_obj_id_t self,
                                 const char *ptr, size_t siz) = 0;

    /*
     *   Reload the object from an image file.  The object's data block is at
     *   the given address and has the given size.  Discards any changes made
     *   since the object was loaded and restores its state as it was
     *   immediately after it was loaded from the image file.  By default, we
     *   do nothing.
     *   
     *   NOTE 1: this routine can be implemented instead of reset_to_image().
     *   If an object doesn't have any other need to store a pointer to its
     *   image file data in its own extension, but the image file data is
     *   necessary to effect a reset, then this routine should be used, to
     *   reduce the size of the object's extension for non-image instances.
     *   
     *   NOTE 2: in order to use this routine, the object MUST call the
     *   object table's save_image_pointer() routine during the initial
     *   loading (i.e., in the object's load_from_image()).  During a reset,
     *   the object table will only call this routine on objects whose image
     *   pointers were previously saved with save_image_pointer().  
     */
    virtual void reload_from_image(VMG_ vm_obj_id_t self,
                                   const char *ptr, size_t siz) { }

    /*
     *   Reset to the image file state.  Discards any changes made since the
     *   object was loaded and restores its state as it was immediately
     *   after it was loaded from the image file.  By default, we do nothing.
     *   
     *   NOTE: this routine doesn't have to do anything if
     *   reload_from_image() is implemented for the object.  
     */
    virtual void reset_to_image(VMG_ vm_obj_id_t /*self*/) { }

    /*
     *   Determine if the object has been changed since it was loaded from
     *   the image file.  This can only be called for objects that were
     *   originally loaded from the image file.  Returns true if the object's
     *   internal state has been changed since loading, false if the object
     *   is in exactly the same state that's stored in the image file.
     *   
     *   If this returns false, then it's not necessary to save the object to
     *   a saved state file, because the object's state can be restored
     *   simply by resetting to the image file state.  
     */
    virtual int is_changed_since_load() const { return FALSE; }

    /* 
     *   save this object to a file, so that it can be restored to its
     *   current state via restore_from_file 
     */
    virtual void save_to_file(VMG_ class CVmFile *fp) = 0;

    /* 
     *   Restore the state of the object from a file into which state was
     *   previously stored via save_to_file.  This routine may need
     *   access to the memory manager because it may need to allocate
     *   memory to make room for the variable data.  
     */
    virtual void restore_from_file(VMG_ vm_obj_id_t self,
                                   class CVmFile *fp,
                                   class CVmObjFixup *fixups) = 0;

    /*
     *   Compare to another value for equality.  Returns true if the value is
     *   equal, false if not.  By default, we return true only if the other
     *   value is an object with the same ID (i.e., a reference to exactly
     *   the same instance).  However, subclasses can override this to
     *   provide different behavior; the string class, for example, may
     *   override this so that it compares equal to any string object or
     *   constant containing the same string text.
     *   
     *   'depth' has the same meaning as in calc_hash().  
     */
    virtual int equals(VMG_ vm_obj_id_t self, const vm_val_t *val,
                       int /*depth*/) const
    {
        /* return true if the other value is a reference to this object */
        return (val->typ == VM_OBJ && val->val.obj == self);
    }

    /*
     *   Compare magnitude of this object and another object.  Returns a
     *   positive value if this object is greater than the other object, a
     *   negative value if this object is less than the other object, or
     *   zero if this object equals the other object.  Throws an error if
     *   a magnitude comparison is not meaningful between the two objects.
     *   
     *   By default, magnitude comparisons between objects are not
     *   meaningful, so we throw an error. 
     */
    virtual int compare_to(VMG_ vm_obj_id_t /*self*/, const vm_val_t *) const
    {
        /* by default, magnitude comparisons between objects are illegal */
        err_throw(VMERR_INVALID_COMPARISON);

        /* the compiler doesn't know that we'll never get here */
        AFTER_ERR_THROW(return 0;)
    }

    /*
     *   Calculate a hash value for the object.
     *   
     *   'depth' is the recursion depth of the hash calculation.  Objects
     *   that can potentially contain cyclical references back to themselves,
     *   and which follow those references to calculate the hash value
     *   recursively, must check the depth counter to see if it exceeds
     *   VM_MAX_TREE_DEPTH_EQ, throwing VMERR_TREE_TOO_DEEP_EQ if so; and
     *   they must increment the depth counter in the recursive traversals.
     *   Objects that don't traverse into their contents can ignore the depth
     *   counter.  
     */
    virtual uint calc_hash(VMG_ vm_obj_id_t self, int /*depth*/) const
    {
        /* 
         *   by default, use a 16-bit hash of our object ID as the hash
         *   value 
         */
        return (uint)(((ulong)self & 0xffff)
                      ^ (((ulong)self & 0xffff0000) >> 16));
    }
                   

    /* 
     *   Add a value to this object, returning the result in *result.  This
     *   may create a new object to hold the result, or may modify the
     *   existing object in place, depending on the subclass implementation.
     *   'self' is the object ID of this object.
     *   
     *   Returns true if the add was implemented, false if not.  If the
     *   operation isn't implemented as a native op, don't throw an error -
     *   simply return false to tell the caller to try an overloaded operator
     *   in byte code.  
     */
    virtual int add_val(VMG_ vm_val_t * /*result*/,
                        vm_obj_id_t /*self*/, const vm_val_t * /*val*/)
        { return FALSE; }

    /*
     *   Subtract a value from this object, returning the result in
     *   *result.  This may create a new object to hold the result, or may
     *   modify the existing object in place, depending upon the subclass
     *   implementation.  'self' is the object ID of this object.  
     */
    virtual int sub_val(VMG_ vm_val_t * /*result*/,
                        vm_obj_id_t /*self*/, const vm_val_t * /*val*/)
        { return FALSE; }

    /* multiply this object by a value, returning the result in *result */
    virtual int mul_val(VMG_ vm_val_t * /* result*/,
                        vm_obj_id_t /*self*/, const vm_val_t * /*val*/)
        { return FALSE; }

    /* divide a value into this object, returning the result in *result */
    virtual int div_val(VMG_ vm_val_t * /* result*/,
                        vm_obj_id_t /*self*/, const vm_val_t * /*val*/)
        { return FALSE; }
    
    /* get the arithmetic negative of self, returning the result in *result */
    virtual int neg_val(VMG_ vm_val_t * /* result*/, vm_obj_id_t /*self*/)
        { return FALSE; }

    /*
     *   Index with overloading.  If the object implements native indexing,
     *   we'll invoke that; otherwise we'll check for operator[] and invoke
     *   that if present; otherwise we'll throw an error.  
     */
    void index_val_ov(VMG_ vm_val_t *result, vm_obj_id_t self,
                      const vm_val_t *index_val);
    

    /* index the object, throwing an error if not implemented for the type */
    void index_val(VMG_ vm_val_t *result, vm_obj_id_t self,
                   const vm_val_t *index_val)
    {
        if (!index_val_q(vmg_ result, self, index_val))
            err_throw(VMERR_CANNOT_INDEX_TYPE);
    }

    /*
     *   Index the object, query mode: retrieve the indexed value and return
     *   true if the object supports the operation, or simply return false
     *   (with no error) if not.  Most object types cannot be indexed, so by
     *   default we'll return false to indicate this is not an implemented
     *   operator.  Subclasses that can be indexed (such as lists) should
     *   look up the element given by the index value, and store the value at
     *   that index in *result.  
     */
    virtual int index_val_q(VMG_ vm_val_t * /*result*/,
                            vm_obj_id_t /*self*/,
                            const vm_val_t * /*index_val*/)
        { return FALSE; }

    /*
     *   Assign to an indexed element of the object, with overloading.  If
     *   the object implements native indexing, we'll invoke that; otherwise
     *   we'll check for operator[]= and invoke that if present; otherwise
     *   we'll throw an error. 
     */
    void set_index_val_ov(VMG_ vm_val_t *new_container, vm_obj_id_t self,
                          const vm_val_t *index_val, const vm_val_t *new_val);

    /*
     *   Set an indexed element of the object, and fill in *new_container
     *   with the new object that results if an entire new object is
     *   created to hold the modified contents, or with the original
     *   object if the contents of the original object are actually
     *   modified by this operation.  Lists, for example, cannot be
     *   modified, so always create a new list object when an element is
     *   set; arrays, in contrast, can be directly modified, so will
     *   simply return 'self' in *new_container.
     *   
     *   By default, we'll throw an error, since default objects cannot be
     *   indexed.  
     */
    void set_index_val(VMG_ vm_val_t *new_container, vm_obj_id_t self,
                       const vm_val_t *index_val, const vm_val_t *new_val)
    {
        if (!set_index_val_q(vmg_ new_container, self, index_val, new_val))
            err_throw(VMERR_CANNOT_INDEX_TYPE);
    }

    /* 
     *   Set an index element of an object, query mode.  Returns true (and
     *   performs the set-index operation) if this is a supported operator,
     *   false if not.  
     */
    virtual int set_index_val_q(VMG_ vm_val_t * /*new_container*/,
                                vm_obj_id_t /*self*/,
                                const vm_val_t * /*index_val*/,
                                const vm_val_t * /*new_val*/)
        { return FALSE; }

    /*
     *   Get the next iterator value.  If another value is available, fills
     *   in *val with the value and returns true.  If not, returns false
     *   without changing *val.
     *   
     *   For an ordinary object, we call the method isNextAvailable(); if
     *   that returns true, we'll call getNext(), fill in *val with its
     *   return value, and return true, otherwise we'll return false.
     *   
     *   The built-in iterator classes override this for better performance.
     */
    virtual int iter_next(VMG_ vm_obj_id_t self, vm_val_t *val);

    /*
     *   Cast the value to integer.  If there's a suitable integer
     *   representation, return it; otherwise throw VMERR_NO_INT_CONV.
     *   
     *   For a string, parse the string and return the integer value.  For a
     *   BigNumber, round to the nearest integer.  Most other types do not
     *   have an integer representation.  
     */
    virtual long cast_to_int(VMG0_) const
    {
        /* by default, just throw a "cannot convert" error */
        err_throw(VMERR_NO_INT_CONV);

        /* we can't get here, but the compiler doesn't always know that */
        AFTER_ERR_THROW(return 0;)
    }

    /*
     *   Cast the value to a numeric type.  Fills in 'val' with the new
     *   value.  If there's a suitable integer representation, return it;
     *   otherwise, if there's a BigNumber representation, return it; if
     *   there's some other numeric type similar to integer or BigNumber that
     *   we can convert to, return that conversion; otherwise throw
     *   VMERR_NO_NUM_CONV.
     *   
     *   Currently, the only types that count as numeric are integer (VM_INT)
     *   and BigNumber.  This interface is designed to be extensible if new
     *   numeric types are added in the future.  Basically, the idea is that
     *   we should return (a) the original value, if it's already numeric, or
     *   (b) the "simplest" numeric type that can precisely represent the
     *   value.  By "simplest", we mean the type that uses the least memory
     *   to represent the given value and is most efficient for computations.
     *   VM_INT is much more efficient than BigNumber, since VM_INT types
     *   don't require memory allocations and perform their arithmetic with
     *   native hardware integer arithmetic.  If we ever were to add a native
     *   "double" type, that would rank between VM_INT and BigNumber, since
     *   even native hardware doubles are generally slower than native
     *   integers for computations, but they're still much faster than
     *   BigNumber.  
     */
    virtual void cast_to_num(VMG_ vm_val_t *val, vm_obj_id_t self) const
    {
        /* by default, just throw a no-conversion error */
        err_throw(VMERR_NO_NUM_CONV);
    }

    /*
     *   Get a string representation of the object.  If necessary, this
     *   can create a new string object to represent the result.
     *   
     *   Returns the string representation in portable string format: the
     *   first two bytes give the byte length of the rest of the string in
     *   portable UINT2 format, and immediately following the length
     *   prefix are the bytes of the string's contents.  The length prefix
     *   does not count the length prefix itself in the length of the
     *   string.
     *   
     *   If a new string object is created, new_str must be set to a
     *   reference to the new string object.  If a pointer into our data
     *   is returned, we must set new_str to self.  If no object data are
     *   involved, new_str can be set to nil.
     *   
     *   If it is not possible to create a string representation of the
     *   object, throw an error (VMERR_NO_STR_CONV).
     *   
     *   By default, we'll throw an error indicating that the object
     *   cannot be converted to a string.  
     */
    virtual const char *cast_to_string(VMG_ vm_obj_id_t self,
                                       vm_val_t *new_str) const;

    /*
     *   Same as cast_to_string, but the conversion is explicitly due to a
     *   toString() call with a radix.  By default, we'll simply use the
     *   basic cast_to_string() processing, but objects representing numbers
     *   (e.g., BigNumber) should override this to respect the radix.
     *   
     *   'flags' is a combination of TOSTR_xxx bit flags: 
     *   
     *.    TOSTR_UNSIGNED - interpret a VM_INT value as unsigned
     *.    TOSTR_ROUND - round a floating point value to the nearest integer
     */
    virtual const char *explicit_to_string(
        VMG_ vm_obj_id_t self, vm_val_t *new_str,
        int /*radix*/, int /*flags*/) const
        { return cast_to_string(vmg_ self, new_str); }

    /*
     *   Is this a numeric type?  Returns true if the value represents some
     *   kind of number, false if not.  For example, the BigNumber type
     *   returns true.  Types that are merely convertible to numbers don't
     *   count as numeric types; for example, String isn't a numeric type
     *   even though some string values can be parsed as numbers.
     */
    virtual int is_numeric() const { return FALSE; }

    /*
     *   Get the integer value of the object, if it has one.  Returns true if
     *   there's an integer value, false if not.
     *   
     *   This differs from cast_to_int() in that we only convert values that
     *   are already scalar numbers.  In particular, we don't attempt to
     *   convert string values.  This is a weaker type of conversion than
     *   casting, for use in situations where different types might be
     *   allowed, but the only type of numeric value allowed is an integer
     *   value.
     */
    virtual int get_as_int(long *val) const { return FALSE; }

    /*
     *   Get the 'double' value of the object, if it has one.  Returns true
     *   if there's a double value, false if not.
     *   
     *   This only converts values that are already scalar numbers.  We don't
     *   parse strings, for example.
     */
    virtual int get_as_double(VMG_ double *val) const { return FALSE; }

    /*
     *   Promote an integer to the type of this object.  For example, if
     *   'this' is a BigNumber, this creates a BigNumber representation of
     *   the integer.  This allows us to perform arithmetic where the left
     *   operand is an integer and the right operand is a different numeric
     *   type.  '*val' contains the integer value on entry, and we replace it
     *   with the promoted value.
     */
    virtual void promote_int(VMG_ vm_val_t * /*val*/) const
        { err_throw(VMERR_NUM_VAL_REQD); }

    /*
     *   Get the list contained in the object, if possible, or null if
     *   not.  If the value contained in the object is a list, this
     *   returns a pointer to the list value in portable format (the first
     *   two bytes are the length prefix as a portable UINT2, and the
     *   subsequent bytes form a packed array of data holders in portable
     *   format).
     *   
     *   Most object classes will return null for this, since they do not
     *   store lists.  By default, we return null.
     */
    virtual const char *get_as_list() const { return 0; }

    /*
     *   Is this a list-like object?  Returns true if the object has a length
     *   and is indexable with index values 1..length. 
     */
    virtual int is_listlike(VMG_ vm_obj_id_t self);

    /* 
     *   For a list-like object, get the number of elements.  Returns -1 if
     *   this isn't a list-like object after all. 
     */
    virtual int ll_length(VMG_ vm_obj_id_t self);

    /*
     *   Get the string contained in the object, if possible, or null if
     *   not.  If the value contained in the object is a string, this
     *   returns a pointer to the string value in portable format (the
     *   first two bytes are the string prefix as a portable UINT2, and
     *   the bytes of the string's text immediately follow in UTF8 format).
     *   
     *   Most object classes will return null for this, since they do not
     *   store strings.  By default, we return null.  
     */
    virtual const char *get_as_string(VMG0_) const { return 0; }

    /*
     *   Rebuild the image data for the object.  This is used to write the
     *   program state to a new image file after executing the program for
     *   a while, such as after a 'preinit' step after compilation.
     *   
     *   This should write the complete metaclass-specific record needed
     *   for an OBJS data block entry, not including the generic header.
     *   
     *   On success, return the size in bytes of the data stored to the
     *   buffer; these bytes must be copied directly to the new image
     *   file.  If the given buffer isn't big enough, this should return
     *   the size needed and otherwise leave the buffer empty.  If the
     *   object doesn't need to be written at all, this should return
     *   zero.  
     */
    virtual ulong rebuild_image(VMG_ char *, ulong)
        { return 0; }

    /*
     *   Allocate space for conversion to constant data for storage in a
     *   rebuilt image file.  If this object can be stored as constant
     *   data in a rebuilt image file, this should reserve space in the
     *   provided constant mapper object for the object's data in
     *   preparation for conversion.
     *   
     *   Most objects cannot be converted to constant data, so the default
     *   here does nothing.  Those that can, such as strings and lists,
     *   should override this.  
     */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *,
                                    vm_obj_id_t /*self*/)
        { /* default does nothing */ }

    /*
     *   Convert to constant data.  If this object can be stored as
     *   constant data in a rebuilt image file, it should override this
     *   routine to store its data in the provided constant mapper object.
     *   If this object makes reference to other objects, it must check
     *   each object that it references to determine if the object has a
     *   non-zero address in the mapper, and if so must convert its
     *   reference to the object to a constant data item at the given
     *   address.  There is no default implementation of this routine,
     *   because it must be overridden by essentially all objects (at
     *   least, it must be overridden by objects that can be converted or
     *   that can refer to objects that can be converted).  
     */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *,
                                       vm_obj_id_t /*self*/)
        { /* default does nothing */ }

    /*
     *   Get the type that this object uses when converted to constant
     *   data.  Objects that can be converted to constant data via
     *   convert_to_const_data() should return the appropriate type
     *   (VM_SSTRING, VM_LIST, etc); others can return VM_NIL to indicate
     *   that they have no conversion.  This will be called only when an
     *   object actually has been converted as indicated in a mapper
     *   (CVmConstMapper) object.  We'll return NIL by default.  
     */
    virtual vm_datatype_t get_convert_to_const_data_type() const
        { return VM_NIL; }
    

    /*
     *   Allocate memory for the fixed part from a memory manager.  We
     *   override operator new so that we can provide custom memory
     *   management for object headers.
     *   
     *   To create an object, use a sequence like this:
     *   
     *   obj_id = mem_mgr->get_obj_table()->alloc_obj(vmg_ in_root_set);
     *.  new (vmg_ obj_id) CVmObjString(subclass constructor params)
     *   
     *   The caller need not store the result of 'new'; the caller
     *   identifies the object by the obj_id value.
     *   
     *   Each CVmObject subclass should provide static methods that cover
     *   the above sequence, since it's a bit verbose to sprinkle
     *   throughout client code.  
     */
    void *operator new(size_t siz, VMG_ vm_obj_id_t obj_id);

protected:
    /*
     *   Find a modifier property.  This is an internal service routine that
     *   we use to traverse the hierarchy of modifier objects associated with
     *   our intrinsic class hierarchy.  
     */
    int find_modifier_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                           vm_obj_id_t self, vm_obj_id_t orig_target_obj,
                           vm_obj_id_t defining_obj, vm_obj_id_t *source_obj,
                           uint *argc);

    /*
     *   Service routine - check a get_prop() argument count for executing
     *   a native code method implementation.  If 'argc' is null, we'll
     *   set the value to a VM_NATIVE_CODE indication and return true,
     *   which the caller should do from get_prop() as well.  If 'argc' is
     *   non-null, we'll check it against the given required argument
     *   count, and throw an error if it doesn't match.  If 'argc' is
     *   non-null and it matches the given argument count, we'll set *argc
     *   to zero to indicate that we've consumed the arguments, and return
     *   false - get_prop() should in this case execute the native code
     *   and return the appropriate result value.  
     */
    static int get_prop_check_argc(vm_val_t *val, uint *argc,
                                   const CVmNativeCodeDesc *desc)
    {
        /* 
         *   if there's no 'argc', we can't execute the native code - we're
         *   simply being asked for a description of the method, not to
         *   evaluate its result 
         */
        if (argc == 0)
        {
            /* indicate a native code evaluation is required */
            val->set_native(desc);

            /* tell get_prop() to return without further work */
            return TRUE;
        }

        /* check the argument count - throw an error if out of range */
        if (!desc->args_ok(*argc))
            err_throw(VMERR_WRONG_NUM_OF_ARGS);

        /* everything's fine - consume the arguments */
        *argc = 0;

        /* tell get_prop() to proceed with the native evaluation */
        return FALSE;
    }
    
#if 0
    /*
     *   Important note:
     *   
     *   This function has been removed because we no longer consider it
     *   likely that we will ever wish to implement a relocating heap
     *   manager.  Given the large memory sizes of modern computers, and
     *   recent academic research calling into question the conventional
     *   wisdom that heap fragmentation is actually a problem in practical
     *   applications, we no longer feel that maintaining the possibility of
     *   a relocating heap manager is justified.
     *   
     *   Note that some code in the present implementation assumes that the
     *   heap is non-relocating, so if a relocating heap manager is ever
     *   implemented, those assumptions will have to be addressed.  For
     *   example, the CVmObjTads code stores direct object pointers inside
     *   its objects, which is only possible with a non-relocating heap.  
     */

    /* 
     *   Adjust the extension pointer for a change - this must be called by
     *   the variable heap page when compacting the heap or when the object
     *   must be reallocated.
     *   
     *   This routine can be called ONLY during garbage collection.  The
     *   heap manager is not allowed to move variable-size blocks at any
     *   other time.
     *   
     *   This method is virtual because a given object could have more than
     *   one block allocated in the variable heap.  By default, we'll fix up
     *   the address of our extension if the block being moved (as
     *   identified by its old address) matches our existing extension
     *   pointer.  
     */
    virtual void move_var_part(void *old_pos, void *new_pos)
    {
        /* if the block being moved is our extension, note the new location */
        if (ext_ == (char *)old_pos)
            ext_ = (char *)new_pos;
    }
#endif

    /* 
     *   Pointer to extension.  This pointer may be used in any way
     *   desired by the subclassed implementation of the CVmObject
     *   interface; normally, this pointer will contain the address of the
     *   data associated with the object.  
     */
    char *ext_;

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc,
                   vm_prop_id_t prop, vm_obj_id_t *source_obj);

    /* property evaluator - ofKind */
    int getp_of_kind(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc,
                     vm_prop_id_t prop, vm_obj_id_t *source_obj);

    /* property evaluator - getSuperclassList */
    int getp_sclist(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc,
                    vm_prop_id_t prop, vm_obj_id_t *source_obj);

    /* property evaluator - propDefined */
    int getp_propdef(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc,
                     vm_prop_id_t prop, vm_obj_id_t *source_obj);

    /* property evaluator - propType */
    int getp_proptype(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc,
                      vm_prop_id_t prop, vm_obj_id_t *source_obj);

    /* property evaluator - getPropList */
    int getp_get_prop_list(VMG_ vm_obj_id_t self, vm_val_t *retval,
                           uint *argc, vm_prop_id_t prop,
                           vm_obj_id_t *source_obj);

    /* property evaluator - getPropParams */
    int getp_get_prop_params(VMG_ vm_obj_id_t self, vm_val_t *retval,
                             uint *argc, vm_prop_id_t prop,
                             vm_obj_id_t *source_obj);

    /* property evaluator - isClass */
    int getp_is_class(VMG_ vm_obj_id_t self, vm_val_t *retval,
                      uint *argc, vm_prop_id_t prop,
                      vm_obj_id_t *source_obj);

    /* property evaluator - propInherited */
    int getp_propinh(VMG_ vm_obj_id_t self, vm_val_t *retval,
                     uint *argc, vm_prop_id_t prop,
                     vm_obj_id_t *source_obj);

    /* property evaluator - isTransient */
    int getp_is_transient(VMG_ vm_obj_id_t self, vm_val_t *retval,
                          uint *argc, vm_prop_id_t prop,
                          vm_obj_id_t *source_obj);
    
    /* find the intrinsic class for the given modifier object */
    virtual vm_obj_id_t find_intcls_for_mod(VMG_ vm_obj_id_t self,
                                            vm_obj_id_t mod_obj);

    /* property evaluation function table */
    static int (CVmObject::*func_table_[])(VMG_ vm_obj_id_t self,
                                           vm_val_t *retval, uint *argc,
                                           vm_prop_id_t prop,
                                           vm_obj_id_t *source_obj);
};

/*
 *   Function table indices for 'Object' intrinsic class methods.  Each of
 *   these gives the index in our function table for the property ID (as
 *   defined in the image file) corresponding to that method.  
 */
const int VMOBJ_IDX_OF_KIND = 1;
const int VMOBJ_IDX_SCLIST = 2;
const int VMOBJ_IDX_PROPDEF = 3;
const int VMOBJ_IDX_PROPTYPE = 4;
const int VMOBJ_IDX_GET_PROP_LIST = 5;
const int VMOBJ_IDX_GET_PROP_PARAMS = 6;
const int VMOBJ_IDX_IS_CLASS = 7;
const int VMOBJ_IDX_PROPINH = 8;
const int VMOBJ_IDX_IS_TRANSIENT = 9;

/* ------------------------------------------------------------------------ */
/*
 *   Each CVmObject subclass must define a singleton instance of
 *   CVmMetaclass that describes the class and instantiates objects of the
 *   class.  
 */

/*
 *   The "registration index" is set at startup, when we register the
 *   metaclasses.  This is static to the class - it isn't a per-instance
 *   value.  The purpose of setting this value is to allow
 *   get_registration_index() in the instance to get the class value.
 *   This information is used when saving our state to save information on
 *   an instance's metaclass, so that the instance can be re-created when
 *   the saved state is restored.
 *   
 *   The registration index is NOT persistent data.  The registration
 *   index of a particular metaclass can vary from execution to execution
 *   (although it will remain fixed throughout the lifetime of one set of
 *   VM globals, hence throughout an image file's execution).  The
 *   registration index allows us to find the registration table entry,
 *   which in turn will give us the metaclass global ID, which is suitable
 *   for persistent storage.  
 */

class CVmMetaclass
{
public:
    virtual ~CVmMetaclass() { }

    /*
     *   Get the metaclass registration table index.  This gives the index
     *   of the metaclass in the system registration table.  This value is
     *   fixed during execution; it is set during startup, when the
     *   registration table is being built, and never changes after that. 
     */
    uint get_reg_idx() const { return meta_reg_idx_; }
    
    /* 
     *   Get the metaclass name.  This is the universally unique
     *   identifier assigned to the metaclass, and is the name used to
     *   dynamically link the image file to the metaclass.  
     */
    virtual const char *get_meta_name() const = 0;

    /* 
     *   Create an instance of the metaclass using arguments from the VM
     *   stack.  Returns the ID of the newly-created object.  If the class
     *   has a byte-code constructor, invoke the constructor using the
     *   normal call-procedure protocol.  Leave the result in register R0. 
     */
    virtual vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                          uint argc) = 0;

    /*
     *   Create an instance of the metaclass with the given ID.  The
     *   object table entry will already be allocated by the caller; this
     *   ro utine only needs to invoke the metaclass-specific constructor
     *   to initialize the object.  The object must be initialized in such
     *   a way that it can subsequently be loaded from the image file with
     *   load_from_image().  In general, this routine only needs to do
     *   something like this:
     *   
     *   new (vmg_ id) CVmObjXxx(); 
     */
    virtual void create_for_image_load(VMG_ vm_obj_id_t id) = 0;

    /*
     *   Create an instance of the metaclass with the given ID in
     *   preparation for restoring the object from a saved state file. 
     */
    virtual void create_for_restore(VMG_ vm_obj_id_t id) = 0;

    /* 
     *   Call a static property of the metaclass 
     */
    virtual int call_stat_prop(VMG_ vm_val_t *result,
                               const uchar **pc_ptr, uint *argc,
                               vm_prop_id_t prop) = 0;

    /* 
     *   Set a static property (for setprop ops on the class itself).
     *   Returns true if the setprop succeeded, false if not.  This shouldn't
     *   throw an error if the property isn't settable on the class; simply
     *   return false instead.  (It's okay to throw errors for other reasons,
     *   such as an invalid type for a property that can be set.  But if it's
     *   not a supported property at all, we want to continue on to check for
     *   setting a modifier property rather than throwing an error at this
     *   phase.)
     *   
     *   'class_state' is the class state holder.  This is a direct pointer
     *   to the state data in the CVmObjClass object.  The callee can modify
     *   this directly, in which case the updated value will be stored (and
     *   will be handled properly for save/restore), but doing so won't save
     *   undo.  To save undo, use CVmObjClass::set_class_state_undo().  The
     *   typical scenario for the state value is that it stores a reference
     *   to a container object, such as a Vector or TadsObject, which is then
     *   used to store the actual class state - we typically need a separate
     *   container because we have more state than we could stuff into a
     *   single vm_val_t.  When using a separate container, that container
     *   will usually handle the undo, in which case there's no need to worry
     *   about undo at the class_state level.
     */
    virtual int set_stat_prop(VMG_ class CVmUndo *, vm_obj_id_t /*self*/,
                              vm_val_t * /* class_state */,
                              vm_prop_id_t /*prop*/, const vm_val_t * /*val*/)
    {
        /* by default, class objects have no settable properties */
        return FALSE;
    }

    /*
     *   Get the number of superclasses of the metaclass, and get the
     *   super-metaclass at the given index.  All metaclasses have exactly
     *   one super-metaclass, except for the root object metaclass, which
     *   has no super-metaclass.  
     */
    virtual int get_supermeta_count(VMG0_) const { return 1; }
    virtual vm_obj_id_t get_supermeta(VMG_ int idx) const;

    /* determine if I'm an instance of the given object */
    virtual int is_meta_instance_of(VMG_ vm_obj_id_t obj) const;

    /* 
     *   set the metaclass registration table index - this can only be
     *   done by vm_register_metaclass() during initialization 
     */
    void set_metaclass_reg_index(uint idx) { meta_reg_idx_ = idx; }

    /* most metaclasses are simply derived from Object */
    virtual CVmMetaclass *get_supermeta_reg() const
        { return CVmObject::metaclass_reg_; }

    /* 
     *   get my class object - we'll look up our class object in the
     *   metaclass registration table 
     */
    vm_obj_id_t get_class_obj(VMG0_) const;

private:
    /* system metaclass registration table index */
    uint meta_reg_idx_;
};
     

/* ------------------------------------------------------------------------ */
/*
 *   Root Object definition.  The root object can never be instantiated;
 *   it is defined purely to provide an object to represent as the root in
 *   the type system.  
 */
class CVmMetaclassRoot: public CVmMetaclass
{
public:
    /* get the metaclass name */
    const char *get_meta_name() const { return "root-object/030004"; }

    /* create an instance - this class cannot be instantiated */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
    {
        err_throw(VMERR_BAD_DYNAMIC_NEW);
        AFTER_ERR_THROW(return VM_INVALID_OBJ;)
    }

    /* create an object - this class cannot be instantiated */
    void create_for_image_load(VMG_ vm_obj_id_t id)
        { err_throw(VMERR_BAD_STATIC_NEW); }

    /* create an instance for loading from a saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
        { err_throw(VMERR_BAD_STATIC_NEW); }

    /* 
     *   call a static property (for getprop on the class itself, e.g.
     *   TadsObject.createInstanceOf()) 
     */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        /* call the base object implementation */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* the root object has no supermetaclasses */
    int get_supermeta_count(VMG0_) const { return 0; }
    vm_obj_id_t get_supermeta(VMG_ int) const { return VM_INVALID_OBJ; }

    /* 
     *   determine if I'm an instance of the given object - the root
     *   object is not a subclass of anything 
     */
    virtual int is_meta_instance_of(VMG_ vm_obj_id_t obj) const
        { return FALSE; }

    /* the base Object metaclass has no supermetaclass */
    virtual CVmMetaclass *get_supermeta_reg() const { return 0; }
};

/* ------------------------------------------------------------------------ */
/*
 *   Object fixup table entry 
 */
struct obj_fixup_entry
{
    /* old ID */
    vm_obj_id_t old_id;

    /* new ID */
    vm_obj_id_t new_id;
};

/*
 *   Object ID Fixup Table.  This is used during state restoration to map
 *   from the saved state file's object numbering system to the new
 *   in-memory numbering system.
 *   
 *   The objects must be added to the table IN ASCENDING ORDER OF OLD ID.
 *   We assume this sorting order to perform a binary lookup when asked to
 *   map an ID.  
 */

/* fixup table subarray size */
#define VMOBJFIXUP_SUB_SIZE 2048

class CVmObjFixup
{
public:
    CVmObjFixup(ulong entry_cnt);
    ~CVmObjFixup();

    /* add a fixup to the table */
    void add_fixup(vm_obj_id_t old_id, vm_obj_id_t new_id);

    /* 
     *   Translate from the file numbering system to the new numbering
     *   system.  If the object isn't found, it must be a static object and
     *   hence doesn't require translation, so we'll return the original ID
     *   unchanged.  
     */
    vm_obj_id_t get_new_id(VMG_ vm_obj_id_t old_id);

    /* fix up a DATAHOLDER value */
    void fix_dh(VMG_ char *dh);

    /* fix up an array of DATAHOLDER values */
    void fix_dh_array(VMG_ char *arr, size_t cnt);

    /* fix a portable VMB_OBJECT_ID field */
    void fix_vmb_obj(VMG_ char *p);

    /* fix an array of portable VMB_OBJECT_ID fields */
    void fix_vmb_obj_array(VMG_ char *p, size_t cnt);

private:
    /* find an entry given the old object ID */
    struct obj_fixup_entry *find_entry(vm_obj_id_t old_entry);

    /* get an entry at the given array index */
    struct obj_fixup_entry *get_entry(ulong idx) const
    {
        return &arr_[idx / VMOBJFIXUP_SUB_SIZE][idx % VMOBJFIXUP_SUB_SIZE];
    }

    /* array of subarrays */
    struct obj_fixup_entry **arr_;

    /* number of subarray pages */
    ulong pages_;

    /* number of entries in the array */
    ulong cnt_;

    /* number of entries used so far */
    ulong used_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Global variable structure.  We maintain a linked list of these
 *   structures for miscellaneous global variables required by other
 *   subsystems.  
 */
struct vm_globalvar_t
{
    /* the variable's value */
    vm_val_t val;

    /* next and previous pointer in linked list of global variables */
    vm_globalvar_t *nxt;
    vm_globalvar_t *prv;
};


/* ------------------------------------------------------------------------ */
/*
 *   Global object page.  We keep a linked list of pages of globals that
 *   refer to objects that are always reachable but were not loaded from the
 *   image file.  
 */
class CVmObjGlobPage
{
public:
    CVmObjGlobPage()
    {
        /* we're not in a list yet */
        nxt_ = 0;

        /* we have no allocated entries yet */
        used_ = 0;
    }

    ~CVmObjGlobPage()
    {
        /* delete the next page */
        delete nxt_;
    }

    /* 
     *   add an entry to this page; returns true on success, false if we're
     *   too full to add another entry 
     */
    int add_entry(vm_obj_id_t obj)
    {
        /* if we're full, indicate failure */
        if (used_ == sizeof(objs_)/sizeof(objs_[0]))
            return FALSE;

        /* store the entry and count it */
        objs_[used_] = obj;
        ++used_;

        /* indicate success */
        return TRUE;
    }

    /* next page in list */
    CVmObjGlobPage *nxt_;

    /* number of entries on this page that are in use */
    size_t used_;

    /* array of entries on this page */
    vm_obj_id_t objs_[30];
};

/* ------------------------------------------------------------------------ */
/*
 *   Object Header Manager 
 */

/* 
 *   Number of objects in a page.  We constrain this to be a power of two
 *   to make certain calculations fast (in particular, so that division by
 *   and modulo the page count can be done as bit shifts and masks).  
 */
const unsigned int VM_OBJ_PAGE_CNT_LOG2 = 12;
const unsigned int VM_OBJ_PAGE_CNT = (1 << VM_OBJ_PAGE_CNT_LOG2);

/*
 *   Reachability states.  A Reachable object is in the root set, or can
 *   be reached directly or indirectly from the root set.  A
 *   Finalizer-Reachable object can be reached directly or indirectly from
 *   a finalizable object that is Finalizer-Reachable or Unreachable, but
 *   not from any reachable object.  An Unreachable object cannot be
 *   reached from any Reachable or Finalizable object.
 *   
 *   We deliberately arrange the objects in a hierarchical order:
 *   Finalizer-Reachable is "more reachable" than Unreachable, and
 *   Reachable is more reachable than Finalizer-Reachable.  The numeric
 *   values of these states are arranged so that a higher number indicates
 *   stronger reachability.  
 */
#define VMOBJ_UNREACHABLE   0x00
#define VMOBJ_F_REACHABLE   0x01
#define VMOBJ_REACHABLE     0x02

/*
 *   Finalization states.  An Unfinalizable object is one which has not
 *   ever been detected by the garbage collector to be less than fully
 *   Reachable.  A Finalizable object is one which has been found during a
 *   garbage collection pass to be either F-Reachable or Unreachable, but
 *   which has not yet been finalized.  A Finalized object is one which
 *   has had its finalizer method invoked.  
 */
#define VMOBJ_UNFINALIZABLE 0x00
#define VMOBJ_FINALIZABLE   0x01
#define VMOBJ_FINALIZED     0x02


/*
 *   Object table page entry.
 */
struct CVmObjPageEntry
{
    /*
     *   An entry is either a member of the free list, or it's a valid
     *   object.
     */
    union
    {
        /* 
         *   If it's not in the free list, then it's a VM object.  We
         *   can't actually embed a true CVmObject here - that's an
         *   abstract type and we therefore can't directly instantiate it
         *   through embedding.  So, just allocate enough space for the
         *   object; the memory manager will claim this space (via
         *   operator new) and store the actual CVmObject here when the
         *   slot is allocated to an object.  
         */
        char obj_[sizeof(CVmObject)];
        
        /* 
         *   if it's in the free list, we just have a pointer to the
         *   previous element of the free list 
         */
        vm_obj_id_t prev_free_;
    } ptr_;

    /* next object in list (either the GC work queue or the free list) */
    vm_obj_id_t next_obj_;

    /* get my VM object pointer */
    CVmObject *get_vm_obj() const { return (CVmObject *)ptr_.obj_; }

    /* flag: the object is in the free list */
    unsigned int free_ : 1;

    /* 
     *   flag: the object is part of the root set (that is, there's a
     *   reference to this object from some static location outside of the
     *   root set, such as in p-code or in a constant list) 
     */
    unsigned int in_root_set_ : 1;

    /*
     *   Reachability state.  This indicates whether the object is
     *   reachable, reachable from finalizable objects only, or
     *   unreachable.  This is set during garbage collection. 
     */
    uint reachable_ : 2;

    /*
     *   Finalization state.  This indicates whether an object is
     *   unfinalizable, finalizable, or finalized. 
     */
    uint finalize_state_ : 2;

    /*
     *   Flag: the object is part of an undo savepoint.  This is cleared
     *   when an object is initially created, and set for all existing
     *   objects when an undo savepoint is created - this means that this
     *   will be set for an object only if an undo savepoint has been
     *   created since the object was created.
     *   
     *   When the undo mechanism is asked to create an undo record
     *   associated with an object, it will do nothing if the object is not
     *   part of the undo savepoint.  This means that we won't save undo
     *   records for objects created since the start of the most recent
     *   savepoint - keeping undo for such objects is unnecessary, since if
     *   we roll back to the savepoint, the object won't even be in
     *   existence any more and hence has no need to restore any of its
     *   state.  
     */
    uint in_undo_ : 1;

    /*
     *   Flag: the object is "transient."  A transient object does not
     *   participate in undo, is not saved or restored to a saved state, and
     *   is not affected by restarting.  
     */
    uint transient_ : 1;

    /* flag: the object has requested post-load initialization */
    uint requested_post_load_init_ : 1;

    /*
     *   Garbage collection hint flags.  These flags provide hints on how the
     *   object's metaclass interacts with the garbage collector.  These do
     *   NOT indicate the object's current status, but rather indicate the
     *   metaclass's capabilities - so 'can_have_refs_' does not indicate
     *   that the object current has or doesn't have any references to other
     *   objects, but rather indicates if it's POSSIBLE for the object EVER
     *   to have references to other objects.  For example, all Strings would
     *   set 'can_have_refs_' to false, and all TadsObjects would set it to
     *   true.
     *   
     *   We set these flags to true by default, for the maximally
     *   conservative settings.  A metaclass can simply ignore these settings
     *   and be assured of correct GC behavior.  However, if a metaclass
     *   knows that it can correctly set one of these flags to false, it
     *   should do so after instances are created, because doing so allows
     *   the garbage collector to reduce the amount of work it must do for
     *   the object.
     *   
     *   'can_have_refs_' indicates if the object can ever contain references
     *   to other objects.  By default, this is always set to true, but a
     *   metaclass that is not capable of storing references to other objects
     *   should set this to false.  When this is set to false, the garbage
     *   collector will avoid tracing into this object when tracing
     *   references, because it will know in advance that tracing into the
     *   object will have no effect.
     *   
     *   'can_have_weak_refs_' indicates if the object can ever contain weak
     *   references to other objects.  By default, this is set to true, but a
     *   metaclass that never uses weak references can set it to false.  When
     *   this is set to false, the garbage collector can avoid notifying this
     *   object of the need to remove stale weak references.
     *   
     *   IMPORTANT: We assume that a metaclass that cannot have
     *   references/weak references can ALSO never have the corresponding
     *   thing in its undo information.  It's hard to imagine a case where
     *   we'd have no possibility of one kind of reference in an object but
     *   then have the same kind of reference in the object's undo records.
     *   But should such a case arise, the metaclass must indicate that it
     *   does have the possibility - in other words, if an object can have a
     *   given reference type in its undo, it must also say it can have that
     *   type in its own data, by setting the appropriate flag here.  
     */
    uint can_have_refs_ : 1;
    uint can_have_weak_refs_ : 1;

    /* 
     *   An entry is deletable if it's unreachable and has been finalized.
     *   If the entry is marked as free, it's already been deleted, hence
     *   is certainly deletable. 
     */
    int is_deletable() const
    {
        return (free_
                || (reachable_ == VMOBJ_UNREACHABLE
                    && finalize_state_ == VMOBJ_FINALIZED));
    }

    /* 
     *   Determine if the object should participate in undo.  An object
     *   participates in undo if it existed as of the most recent savepoint,
     *   and the object is not transient. 
     */
    int is_in_undo() const { return in_undo_ && !transient_; }

    /*
     *   A "saveable" object is one which must be written to a saved state
     *   file.
     *   
     *   To be saveable, an object must not be free, must not be transient,
     *   must be fully reachable, and must have been modified since loading
     *   (or simply have been created dynamically, since all an object that
     *   was created dynamically has inherently been modified since the
     *   program was loaded, as it didn't even exist when the program was
     *   loaded).
     *   
     *   Do not save objects that are only reachable through a finalizer;
     *   assume that these objects do not figure into the persistent VM
     *   state, but are still around merely because they have some external
     *   resource deallocation to which they must yet tend.
     *   
     *   Do not save objects that are in the root set and which haven't been
     *   modified since loading.  We always reset before restoring to the
     *   initial image file state, so there's no need to save data for any
     *   object that's simply in its initial image file state.  
     */
    int is_saveable() const
    {
        return (!free_
                && !transient_
                && reachable_ == VMOBJ_REACHABLE
                && (!in_root_set_
                    || get_vm_obj()->is_changed_since_load()));
    }

    /*
     *   Determine if the object is "persistent."  A persistent object is
     *   one which will survive saving and restoring machine state.  An
     *   object is persistent if it is not transient, and either it is
     *   saveable (in which case it will be explicitly saved and restored),
     *   or it is merely in the root set (in which case it is always
     *   present).  
     */
    int is_persistent() const
    {
        return !transient_ && (in_root_set_ || is_saveable());
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Object table.
 */
class CVmObjTable
{
public:
    /* create the table */
    CVmObjTable();

    /* initialize */
    void init(VMG0_);

    /* 
     *   Clear the object table.  This deletes the garbage collected objects,
     *   but leaves the table itself intact. 
     */
    void clear_obj_table(VMG0_);

    /* 
     *   Destroy the table - call this rather BEFORE using operator delete
     *   directly.  After this routine is called, the object table can be
     *   deleted.  
     */
    void delete_obj_table(VMG0_);

    /* clients must call delete_obj_table() before deleting the object */
    ~CVmObjTable();

    /* get an object given an object ID */
    inline CVmObject *get_obj(vm_obj_id_t id) const
    {
        /* get the page entry, and get the object from the entry */
        return (CVmObject *)&get_entry(id)->ptr_.obj_;
    }

    /*
     *   Turn garbage collection on or off.  When performing a series of
     *   allocations of values that won't be stored on the stack, this can
     *   be used to ensure that the intermediate allocations aren't
     *   collected as unreferenced before the group of operations is
     *   completed.  Returns previous status for later restoration.  
     */
    int enable_gc(VMG_ int enable);

    /* allocate a new object ID */
    vm_obj_id_t alloc_obj(VMG_ int in_root_set)
    {
        /* allocate, using maximally conservative GC characteristics */
        return alloc_obj(vmg_ in_root_set, TRUE, TRUE);
    }

    /* allocate a new object ID */
    vm_obj_id_t alloc_obj(VMG_ int in_root_set, int can_have_refs,
                          int can_have_weak_refs);

    /*
     *   Allocate an object at a particular object ID.  This is used when
     *   loading objects from an image file or restoring objects from a
     *   saved state file, since objects must be loaded or restored with
     *   the same object number which they were originally assigned.  This
     *   routine throws an error if the object is already allocated.
     */
    void alloc_obj_with_id(vm_obj_id_t id, int in_root_set)
    {
        /* allocate with maximally conservative GC characteristics */
        alloc_obj_with_id(id, in_root_set, TRUE, TRUE);
    }

    /* allocate an object with a given ID */
    void alloc_obj_with_id(vm_obj_id_t id, int in_root_set,
                           int can_have_refs, int can_have_weak_refs);

    /* 
     *   Collect all garbage.  This runs an entire garbage collection pass
     *   to completion with a single call.  This can be used for
     *   simplicity when the caller does not require incremental operation
     *   of the garbage collector.
     *   
     *   This function actually performs two garbage collection passes to
     *   ensure that all collectible objects are collected.  We perform
     *   one pass to detect finalizable objects, then finalize all objects
     *   that we can, then make one more pass to sweep up all of the
     *   finalized objects that can be deleted.  
     */
    void gc_full(VMG0_);

    /* 
     *   Incremental garbage collection.  Call gc_pass_init() to
     *   initialize the pass.  Call gc_pass_continue() repeatedly to
     *   perform incremental collection; this routine runs for a short
     *   time and then returns.  gc_pass_continue() returns true if
     *   there's more work to do, false if not, so the caller can stop
     *   invoking it as soon as it returns false.  Call gc_pass_finish()
     *   to complete the garbage collection.  gc_pass_finish() will call
     *   gc_pass_continue() if necessary to finish its work, so the caller
     *   need not keep invoking gc_pass_continue() if it runs out of work
     *   to interleave with the garbage collector.
     *   
     *   Once garbage collection is started, it must be finished before
     *   any other VM activity occurs.  So, after a call to
     *   gc_pass_init(), the caller is not allowed to perform any other VM
     *   operations until gc_pass_finish() returns (in particular, no new
     *   objects may be created, and no references to existing objects may
     *   be created or changed).
     */
    void gc_pass_init(VMG0_);
    int  gc_pass_continue(VMG0_) { return gc_pass_continue(vmg_ TRUE); }
    void gc_pass_finish(VMG0_);

    /*
     *   Run pending finalizers.  This can be run at any time other than
     *   during garbage collection (i.e., between gc_pass_init() and
     *   gc_pass_finish()).
     */
    void run_finalizers(VMG0_);

    /*
     *   Determine if a given object is subject to deletion.  This only
     *   gives meaningful results during the final garbage collector pass.
     *   Returns true if the object is ready for deletion, which can only
     *   happen when the object is both Unreachable and Finalized, or
     *   false if it not.  A true return means that the object can be
     *   deleted at any time.  This can be used by weak referencers to
     *   determine if objects they are referencing are about to be
     *   deleted, and thus that the weak reference must be forgotten.  
     */
    int is_obj_deletable(vm_obj_id_t obj) const
    {
        /* 
         *   If it's not a valid object, consider it deletable, since it
         *   has indeed already been deleted; otherwise, it's deletable if
         *   its object table entry is deletable.  
         */
        return (obj == VM_INVALID_OBJ
                || get_entry(obj)->is_deletable());
    }

    /*
     *   Mark object references (for GC tracing) made in an object's undo
     *   record.  If the object is marked as having no possibility of
     *   containing references to other objects, we won't bother invoking
     *   the object's tracing method, as we can be assured that the undo
     *   records won't contain any references either.  
     */
    void mark_obj_undo_rec(VMG_ vm_obj_id_t obj,
                           struct CVmUndoRecord *undo_rec)
    {
        /* get the object entry */
        CVmObjPageEntry *entry = get_entry(obj);

        /* 
         *   if the object can have any references, mark any references the
         *   object makes from the undo record; if the object can't have any
         *   references, assume its undo records cannot either, in which
         *   case there should be nothing to mark 
         */
        if (entry->can_have_refs_)
            entry->get_vm_obj()->mark_undo_ref(vmg_ undo_rec);
    }

    /* 
     *   Remove stale weak undo references for an object.  If the object is
     *   marked as having no possibility of weak references, we won't bother
     *   invoking the object's weak undo reference remover method, since we
     *   know it won't do anything.  
     */
    void remove_obj_stale_undo_weak_ref(VMG_ vm_obj_id_t obj,
                                        struct CVmUndoRecord *undo_rec)
    {
        /* get the object entry */
        CVmObjPageEntry *entry = get_entry(obj);

        /* 
         *   if the object can have weak references, notify it; if not,
         *   there's no need to do anything 
         */
        if (entry->can_have_weak_refs_)
            entry->get_vm_obj()->remove_stale_undo_weak_ref(vmg_ undo_rec);
    }

    /*
     *   Determine if the given object is part of the latest undo savepoint.
     *   Returns true if an undo savepoint has been created since the object
     *   was created, false if not.  
     */
    int is_obj_in_undo(vm_obj_id_t obj) const
    {
        return (obj != VM_INVALID_OBJ
                && get_entry(obj)->is_in_undo());
    }

    /*
     *   Determine if a vm_val_t contains a reference to a deletable object.
     *   This is a simple convenience routine.  This returns true only if
     *   the value contains a valid object reference, and the object is
     *   currently deletable.  
     */
    int is_obj_deletable(const vm_val_t *val) const
    {
        return (val->typ == VM_OBJ
                && val->val.obj != VM_INVALID_OBJ
                && is_obj_deletable(val->val.obj));
    }

    /*
     *   Determine if the object is saveable.  If this returns true, the
     *   object should be saved to a saved state file.  If not, the object
     *   should not be included in the saved state file.  Note that a
     *   non-saveable object might still be a persistent object - if the
     *   object is a root set object and hasn't been modified since
     *   loading, it's still peristent even though it doesn't get written
     *   to the saved state file.  
     */
    int is_obj_saveable(vm_obj_id_t obj) const
    {
        return (obj != VM_INVALID_OBJ
                && get_entry(obj)->is_saveable());
    }

    /*
     *   Determine if the object is persistent in a saved state.  If this
     *   returns true, the object will survive saving and restoring the
     *   machine state; if not, the object will not be present after the
     *   machine state is restored.  This can be used to test if a weak
     *   reference should be included in a saved state file.  
     */
    int is_obj_persistent(vm_obj_id_t obj) const
    {
        return (obj != VM_INVALID_OBJ
                && get_entry(obj)->is_persistent());
    }

    /* determine if the given object is transient */
    int is_obj_transient(vm_obj_id_t obj) const
    {
        return (obj != VM_INVALID_OBJ
                && get_entry(obj)->transient_);
    }

    /* mark an object as transient */
    void set_obj_transient(vm_obj_id_t obj) const
    {
        /* set the 'transient' flag in the object */
        get_entry(obj)->transient_ = TRUE;
    }

    /* set an object's garbage collection characteristics */
    void set_obj_gc_characteristics(
        vm_obj_id_t obj, int can_have_refs, int can_have_weak_refs) const
    {
        CVmObjPageEntry *entry;

        /* get the object's entry */
        entry = get_entry(obj);

        /* set the entry's GC flags as specified */
        entry->can_have_refs_ = can_have_refs;
        entry->can_have_weak_refs_ = can_have_weak_refs;
    }

    /* determine if the given object is in the root set */
    int is_obj_in_root_set(vm_obj_id_t obj) const
    {
        return (obj != VM_INVALID_OBJ
                && get_entry(obj)->in_root_set_);
    }

    /*
     *   Mark the given object as referenced, and recursively mark all of
     *   the objects to which it refers as referenced.  
     */
    void mark_all_refs(vm_obj_id_t obj, uint state)
        { add_to_gc_queue(obj, state); }

    /*
     *   Receive notification from the undo manager that we're starting a
     *   new savepoint.  We'll simply notify all of the objects of this. 
     */
    void notify_new_savept();

    /*
     *   Apply an undo record 
     */
    void apply_undo(VMG_ struct CVmUndoRecord *rec);

    /* call a callback for each object in the table */
    void for_each(VMG_ void (*func)(VMG_ vm_obj_id_t, void *), void *ctx);

    /*
     *   Rebuild the image file's OBJS blocks for a particular metaclass.
     *   We'll write all of the objects of the given metaclass to one or
     *   more OBJS blocks in the given output file.  This can be used to
     *   dump the program state to a new image file after running
     *   'preinit' or a similar compile-time pre-initialization procedure.
     *   
     *   'meta_dep_idx' is the index in the metaclass dependency table of
     *   the metaclass to be written.  
     */
    void rebuild_image(VMG_ int meta_dep_idx, class CVmImageWriter *writer,
                       class CVmConstMapper *mapper);

    /*
     *   Scan all objects and add metaclass entries to the metaclass
     *   dependency table for any metaclasses of which there are existing
     *   instances. 
     */
    void add_metadeps_for_instances(VMG0_);

    /*
     *   Scan all active objects and convert objects to constant data
     *   where possible.  Certain object metaclasses, such as strings and
     *   lists, can be represented in a rebuilt image file as constant
     *   data; this routine makes all of these conversions. 
     */
    void rebuild_image_convert_const_data(VMG_
                                          class CVmConstMapper *const_mapper);

    /*
     *   Get the maximum object ID that has ever been allocated.  This
     *   establishes an upper bound on the object ID's that can be found
     *   among the active objects.  
     */
    vm_obj_id_t get_max_used_obj_id() const
        { return pages_used_ * VM_OBJ_PAGE_CNT; }

    /* determine if an object ID refers to a valid object */
    int is_obj_id_valid(vm_obj_id_t obj) const
    {
        /* 
         *   the object is valid as long as it's not free, and the ID is
         *   within the valid range 
         */
        return (obj != VM_INVALID_OBJ
                && obj < get_max_used_obj_id()
                && !get_entry(obj)->free_);
    }

    /*
     *   Get the object state.  This is intended primarily as a debugging
     *   and testing aid for the VM itself; this value should be of no
     *   interest to normal programs.  Returns a value suitable for use
     *   with CVmBifT3Test::get_obj_gc_state().  
     */
    ulong get_obj_internal_state(vm_obj_id_t id) const
    {
        /* if the object ID is invalid, return 0xF000 to so indicate */
        if (id >= get_max_used_obj_id())
            return 0xF000;

        /* 
         *   return the state as a combination of these bits:
         *   
         *   free ? 0 : 1
         *.  Unreachable=0x00, F-Reachable=0x10, Reachable=0x20
         *.  Unfinalizable=0x000, Finalizable=0x100, Finalized=0x200 
         */
        return ((get_entry(id)->free_ ? 0 : 1)
                + (((ulong)get_entry(id)->reachable_) << 4)
                + (((ulong)get_entry(id)->finalize_state_) << 8));
    }

    /*
     *   Reset to the initial image file state.  Discards all objects not
     *   in the root set, skipping finalizers, and resets all objects to
     *   their initial image file state.  
     */
    void reset_to_image(VMG0_);

    /* 
     *   Save state to a file.  We write out each object's state to the
     *   file so that the state can be restored later. 
     */
    void save(VMG_ class CVmFile *fp);

    /* 
     *   Restore state from a previously saved file.  Returns zero on
     *   success, or a VMERR_xxx code on failure.
     *   
     *   This routine creates an object fixup table, and returns it in
     *   *fixups.  The caller is responsible for deleting this object if a
     *   non-null pointer is returned in *fixups.  
     */
    int restore(VMG_ class CVmFile *fp, class CVmObjFixup **fixups);

    /*
     *   Save an object's image data pointer.  An object's load_from_image()
     *   routine may call this routine (it cannot be called from anywhere
     *   else) to save the loaded image location for the object.  For each
     *   object that calls this routine, we will call the object's
     *   reload_from_image() method during a reset to initial image file
     *   state.  
     */
    void save_image_pointer(vm_obj_id_t obj, const char *ptr, size_t siz);

    /*
     *   Request post-load initialization.  An object can call this to
     *   request that its post_load_init() method be called after an initial
     *   program load, restart, or restore operation.  The post_load_init()
     *   method will not be called until the load/restart/restore operation
     *   has loaded every object, so the method can be used to perform any
     *   initialization that depends upon other objects being loaded.  
     */
    void request_post_load_init(vm_obj_id_t obj);

    /* remove a post-load initialization request */
    void remove_post_load_init(vm_obj_id_t obj);

    /* invoke all registered post-load initializations */
    void do_all_post_load_init(VMG0_);

    /*
     *   Ensure that the given object has had its post-load initialization
     *   performed during the current load/restart/restore.  If an object's
     *   post-load initialization depends upon another object having already
     *   been initialized, the first object should call this to ensure that
     *   the other object it depends upon has been initialized.  This allows
     *   objects to ensure that initialization dependencies are handled in
     *   the correct order, regardless of the order in which the objects were
     *   loaded.
     *   
     *   Post-load initialization is guaranteed to be executed exactly once
     *   per load/restart/restore cycle.  When this routine is called, we
     *   check to see if the target object has already been initialized
     *   during this operation, and we do nothing if so.  If we do invoke the
     *   target object's post_load_init(), then this will be the only
     *   invocation for this operation.
     *   
     *   Circular dependencies are prohibited.  If object A's
     *   post_load_init() method calls this to initialize object B, we will
     *   invoke object B's post_load_init().  If object B in turn calls this
     *   routine to initialize object A, we will observe that object A is
     *   already in the process of being initialized and throw an error.  
     */
    void ensure_post_load_init(VMG_ vm_obj_id_t obj);

    /*
     *   Add an object to the list of machine globals.  An object added to
     *   this list will never be deleted.  If the object is in the root set
     *   (which means it was loaded from the image file), this has no effect,
     *   since a root object is inherently global.  
     */
    void add_to_globals(vm_obj_id_t obj);

    /*
     *   Create a "global variable."  A global variable is part of the root
     *   set: any value in a variable allocated here will be traced during
     *   garbage collection.  Global variables are meant for use by other
     *   subsystems, so that other subsystems can include their own static
     *   and global variables in the root set.
     *   
     *   Global variables aren't affected by RESTORE, RESTART, or UNDO (like
     *   the stack, global variables are transient).
     *   
     *   The caller can use delete_global_var() to delete the variable when
     *   done with it.  Any global variable that's never explicitly deleted
     *   will be automatically deleted when the object table itself is
     *   destroyed.  
     */
    vm_globalvar_t *create_global_var();
    void delete_global_var(vm_globalvar_t *var);

    /* count an allocation */
    void count_alloc(size_t siz) { bytes_since_gc_ += siz; }

private:
    /* rebuild the image, writing only transient or only persistent objects */
    void rebuild_image(VMG_ int meta_dep_idx, CVmImageWriter *writer,
                       class CVmConstMapper *mapper, int trans);

    /* invoke a post-load initialization method */
    static void call_post_load_init(VMG_ class CVmHashEntryPLI *entry);

    /* enumeration callbacks for post-load initialization */
    static void pli_status_cb(void *ctx, class CVmHashEntry *entry);
    static void pli_invoke_cb(void *ctx, class CVmHashEntry *entry);

    /* get the page entry for a given ID */
    inline CVmObjPageEntry *get_entry(vm_obj_id_t id) const
    {
        return &pages_[id >> VM_OBJ_PAGE_CNT_LOG2][id & (VM_OBJ_PAGE_CNT - 1)];
    }
    
    /* delete an entry */
    void delete_entry(VMG_ vm_obj_id_t id, CVmObjPageEntry *entry);

    /* allocate a new page of objects */
    void alloc_new_page();

    /* 
     *   initialize a newly-allocated object table entry -- removes the
     *   entry from the free list, marks the entry as allocated, marks it
     *   in or out of the root set as appropriate, and initializes its GC
     *   status as appropriate 
     */
    void init_entry_for_alloc(vm_obj_id_t id, CVmObjPageEntry *entry,
                              int in_root_set, int can_have_refs,
                              int can_have_weak_refs);

    /*
     *   Mark an object as referenced for the garbage collector and add it
     *   to the garbage collector's work queue.  If the object is already
     *   marked as referenced, this does nothing. 
     */
    void add_to_gc_queue(vm_obj_id_t id, uint state)
    {
        /* get the object header and add it to the work queue */
        add_to_gc_queue(id, get_entry(id), state);
    }

    /* add an object to the gc work queue given the object header entry */
    void add_to_gc_queue(vm_obj_id_t id, CVmObjPageEntry *entry, uint state)
    {
        /* 
         *   If it's not already referenced somehow, add it to the queue.  If
         *   it's marked as referenced, it's already in the queue (or it's
         *   already been in the queue and it's been processed).
         *   
         *   If the object can't have references to other objects, don't add
         *   it to the queue - simply elevate its reachability state.  We put
         *   objects in the queue in order to trace into the objects they
         *   reference, so an object that can't reference any other objects
         *   doesn't need to go in the queue.  
         */
        if (entry->can_have_refs_ && entry->reachable_ == VMOBJ_UNREACHABLE)
        {
            /* add it to the work queue */
            entry->next_obj_ = gc_queue_head_;
            gc_queue_head_ = id;

            /* 
             *   Since the entry is unreachable, and unreachable is the
             *   lowest reachability state, we know that 'state' is at least
             *   as reachable, so we can just set the new state without even
             *   bothering with another comparison.
             */
            entry->reachable_ = state;
        }
        else
        {
            /* 
             *   Elevate the reachability state.  Never reduce an object's
             *   reachability state: Finalizer-Reachable is higher than
             *   Unreachable, and Reachable is higher than
             *   Finalizer-Reachable.
             *   
             *   In other words, if an object is already marked Reachable,
             *   never reduce its state to Finalizer-Reachable just because
             *   we find that it can also be reached from a
             *   Finalizer-Reachable object, when we already know that it
             *   can be reached from a root-set object.  
             */
            if (state > entry->reachable_)
                entry->reachable_ = state;
        }
    }

    /* 
     *   Add an object to the finalizer work queue.  An object can only be
     *   in one queue - it can't be in both the finalizer queue and the
     *   main gc work queue.  
     */
    void add_to_finalize_queue(vm_obj_id_t id, CVmObjPageEntry *entry)
    {
        /* mark the object as finalizer-reachable */
        entry->reachable_ = VMOBJ_F_REACHABLE;
        
        /* link it into the finalize list */
        entry->next_obj_ = finalize_queue_head_;
        finalize_queue_head_ = id;
    }

    /*
     *   Count an allocation and check to see if we should run garbage
     *   collection.  We run gc after a certain number of consecutive
     *   allocations to ensure that allocation-intensive operations don't
     *   fill up memory if we can avoid it by removing unreferenced
     *   objects.  
     */
    void alloc_check_gc(VMG_ int do_count)
    {
        /* count the allocation if desired */
        if (do_count)
            ++allocs_since_gc_;

        /* if we've passed our threshhold for collecting garbage, do so now */
        if (gc_enabled_
            && (allocs_since_gc_ > max_allocs_between_gc_
                || bytes_since_gc_ > max_bytes_between_gc_))
            gc_before_alloc(vmg0_);
    }

    /*
     *   Run a garbage collection in preparation to allocate memory.  Runs
     *   one garbage collection pass then one finalizer pass.  
     */
    void gc_before_alloc(VMG0_);

    /* garbage collection: trace objects reachable from the stack */
    void gc_trace_stack(VMG0_);

    /* garbage collection: trace objects reachable from the imports */
    void gc_trace_imports(VMG0_);

    /* garbage collection: trace objects reachable from machine globals */
    void gc_trace_globals(VMG0_);

    /* garbage collection: trace all objects reachable from the work queue */
    void gc_trace_work_queue(VMG_ int trace_transient);

    /* continue a GC pass */
    int gc_pass_continue(VMG_ int trace_transient);

    /* 
     *   set the initial GC conditions for an object -- this puts the
     *   object into the appropriate queue and sets the appropriate
     *   reachability state in preparation for the start of the next GC
     *   pass 
     */
    void gc_set_init_conditions(vm_obj_id_t id,
                                struct CVmObjPageEntry *entry)
    {
        /* 
         *   Mark the object as unreachable -- at the start of each GC pass,
         *   all non-root-set objects must be marked unreachable.  (It
         *   doesn't matter how we mark root set objects, so we simply mark
         *   everything as reachable to avoid an unnecessary test.)  
         */
        entry->reachable_ = VMOBJ_UNREACHABLE;

        /* 
         *   If it's in the root set, add it to the GC work queue -- all
         *   root-set objects must be in the work queue and marked as
         *   reachable at the start of each GC pass.
         */
        if (entry->in_root_set_)
            add_to_gc_queue(id, entry, VMOBJ_REACHABLE);
    }

    /* hash table of objects requested post_load_init() service */
    class CVmHashTable *post_load_init_table_;

    /* 
     *   Head of global object page list.  The global objects are objects
     *   that are always reachable but which weren't necessarily loaded from
     *   the image file; these objects must be treated as dynamically
     *   created for purposes such as saving, but must never be deleted as
     *   long as they are globally reachable.  
     */
    CVmObjGlobPage *globals_;

    /* head of global variable list */
    struct vm_globalvar_t *global_var_head_;

    /*
     *   Master page table.  This is an array of pointers to pages.  Each
     *   page contains a fixed number of slots for fixed-size parts.  
     */
    CVmObjPageEntry **pages_;

    /* number of page slots allocated, and the number actually used */
    size_t page_slots_;
    size_t pages_used_;

    /* first free object */
    vm_obj_id_t first_free_;

    /* first page of saved image data pointers */
    struct vm_image_ptr_page *image_ptr_head_;

    /* last page of saved image data pointers */
    struct vm_image_ptr_page *image_ptr_tail_;

    /* number of image data pointers stored on last image pointer page */
    size_t image_ptr_last_cnt_;

    /* head of garbage collection work queue */
    vm_obj_id_t gc_queue_head_;

    /* head of finalizer queue */
    vm_obj_id_t finalize_queue_head_;

    /* 
     *   Allocations since last garbage collection.  We increment this on
     *   each allocation, and reset it to zero each time we collect
     *   garbage.  When we've performed too many allocations since the
     *   last garbage collection, we force a gc pass. 
     */
    uint allocs_since_gc_;

    /* 
     *   Bytes allocated since the last garbage collection pass.  We add each
     *   heap allocation request to this, and zero it each time we collect
     *   garbage.  This lets us trigger an early collection pass (before we'd
     *   collect based on object count) if a lot of large objects are being
     *   thrown around, to ensure that our OS-level working set doesn't grow
     *   too fast due to rapid creation of large unreachable objects.  
     */
    ulong bytes_since_gc_;

    /*
     *   Maximum number of objects and heap bytes allocated before we run the
     *   garbage collector.  
     */
    uint max_allocs_between_gc_;
    ulong max_bytes_between_gc_;

    /* garbage collection enabled */
    uint gc_enabled_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   An image data pointer.  The object table uses this structure to save
 *   the image data location for a given object when the object requests
 *   that this information be saved.  
 */
struct vm_image_ptr
{
    /* object ID */
    vm_obj_id_t obj_id_;

    /* pointer to image data and length of the data */
    const char *image_data_ptr_;
    size_t image_data_len_;
};

/*
 *   Maximum number of image pointers stored per image pointer page 
 */
const size_t VM_IMAGE_PTRS_PER_PAGE = 400;

/*
 *   A page of image pointers.  
 */
struct vm_image_ptr_page
{
    /* next page in the list of pages */
    vm_image_ptr_page *next_;

    /* array of image pointers */
    vm_image_ptr ptrs_[VM_IMAGE_PTRS_PER_PAGE];
};


/* ------------------------------------------------------------------------ */
/*
 *   The variable-size parts are stored in a heap separately from the object
 *   headers.  Because the variable-size objects can expand or contract
 *   dynamically, objects in the heap can move to new addresses.
 *   
 *   A particular block of memory in the heap is referenced by zero or one
 *   object at any given time; a heap block is never shared among multiple
 *   objects.  Furthermore, the only thing that can directly reference a
 *   heap block is an object's fixed portion (through its extension
 *   pointer).  Hence, we manage the variable heap directly through the
 *   object headers: when an object becomes unreferenced, we explicitly free
 *   the associated variable part.
 *   
 *   It is possible for a single object to allocate more than one heap
 *   block.  In practice, most objects will allocate only one heap block (if
 *   they allocate a heap block at all), but there is no reason an object
 *   can't allocate more than one block.
 *   
 *   Note that polymorphism in the variable parts is handled via the fixed
 *   part.  Because the fixed part is responsible for interactions with the
 *   variable part, each fixed part implementation will be mated to a
 *   particular variable part implementation.  These implementations may
 *   themselves be polymorphic, of course, but this isn't directly necessary
 *   in the base class.  
 */

/*
 *   Variable-size object heap interface.
 */
class CVmVarHeap
{
public:
    virtual ~CVmVarHeap() { }
    
    /*
     *   Initialize.  The global object table is valid at this point, and
     *   will remain valid until after terminate() is called.  
     */
    virtual void init(VMG0_) = 0;

    /*
     *   Terminate.  The global object table will remain valid until after
     *   this function returns.
     *   
     *   The object table is expected to free each object's variable part
     *   explicitly, so this function need not deallocate the memory used
     *   by variable parts.  
     */
    virtual void terminate() = 0;
    
    /* 
     *   Allocate a variable-size part.  'siz' is the size requested in
     *   bytes, and 'obj' is a pointer to the object header.  The object
     *   header will never move in memory, so this pointer is valid for as
     *   long as the object remains allocated, hence the heap manager can
     *   store the header pointer with the memory block if desired.  
     */
    virtual void *alloc_mem(size_t siz, CVmObject *obj) = 0;

    /*
     *   Resize a variable-size part.  The 'siz' is the new size requested
     *   in bytes, and 'varpart' is the old variable-size memory block.
     *   This should return a new variable-size memory block containing a
     *   copy of the data in the original block, but the block should be
     *   resized to at least 'siz' bytes.  Returns a pointer to the new
     *   block, which may move to a new memory location.
     *   
     *   If we move the memory to a new location, we are responsible for
     *   freeing the old block of memory that the variable part occupied,
     *   if necessary.
     *   
     *   We do not need to worry about informing the object header of any
     *   change to the address of the variable part; the caller is
     *   responsible for making any necessary changes in the object header
     *   based on our return value.  
     */
    virtual void *realloc_mem(size_t siz, void *varpart,
                              CVmObject *obj) = 0;

    /*
     *   Free an object in the heap.  The object header may no longer be
     *   valid after this function returns, so the heap manager should not
     *   store the object header pointer after this function returns.  
     */
    virtual void free_mem(void *varpart) = 0;

#if 0
    /* 
     *   This is not currently used by the heap implementation (and doesn't
     *   even have any theoretical reason to exist at the moment, since the
     *   adoption of a non-moveable heap policy and the removal of
     *   move_var_part()), so it's been removed to avoid the unnecessary
     *   overhead of calling an empty method. 
     */
    
    /*
     *   Receive notification of the completion of a garbage collection
     *   pass.  If the heap manager is capable of closing gaps in memory
     *   by moving objects around, this is a good time to perform this
     *   work, since we have just deleted all unreachable objects.
     *   
     *   If any object moves during processing here, we must call the
     *   associated CVmObject's move_var_part() routine to tell it about
     *   its new location.
     *   
     *   This routine isn't required to do anything at all.  It's simply
     *   provided as a notification for heap managers that can take
     *   advantage of the opportunity to compact the heap.  
     */
    virtual void finish_gc_pass() = 0;
#endif
};


/* ------------------------------------------------------------------------ */
/*
 *   Simple variable-size object heap implementation.  This implementation
 *   uses the normal C heap manager (malloc and free) to manage the heap.
 */

/*
 *   block header - we use the header to keep track of the size of the
 *   object's data area
 */
struct CVmVarHeapMallocHdr
{
    /* size of the object */
    size_t siz_;
};

/*
 *   heap implementation 
 */
class CVmVarHeapMalloc: public CVmVarHeap
{
public:
    CVmVarHeapMalloc() { }
    ~CVmVarHeapMalloc() { }

    /* initialize */
    void init(VMG0_) { }

    /* terminate */
    void terminate() { }

    /* allocate memory */
    void *alloc_mem(size_t siz, CVmObject *)
    {
        /* allocate space for the block plus the header */
        CVmVarHeapMallocHdr *hdr = (CVmVarHeapMallocHdr *)
              t3malloc(siz + sizeof(CVmVarHeapMallocHdr));

        /* set up the header */
        hdr->siz_ = siz;

        /* return the start of the part immediately after the header */
        return (void *)(hdr + 1);
    }

    /* reallocate memory */
    void *realloc_mem(size_t siz, void *varpart, CVmObject *)
    {
        CVmVarHeapMallocHdr *hdr;

        /* 
         *   get the original header, which immediately precedes the
         *   original variable part in memory 
         */
        hdr = ((CVmVarHeapMallocHdr *)varpart) - 1;

        /* 
         *   Reallocate it - the header is the actual memory block as far
         *   as malloc was concerned, so realloc that.  Note that we must
         *   add in the space needed for our header in the resized block.  
         */
        hdr = (CVmVarHeapMallocHdr *)
              t3realloc(hdr, siz + sizeof(CVmVarHeapMallocHdr));

        /* adjust the size of the block in the header */
        hdr->siz_ = siz;

        /* return the part immediately after the header */
        return (void *)(hdr + 1);
    }

    /* free memory */
    void free_mem(void *varpart)
    {
        CVmVarHeapMallocHdr *hdr;

        /* 
         *   get the original header, which immediately precedes the
         *   original variable part in memory 
         */
        hdr = ((CVmVarHeapMallocHdr *)varpart) - 1;

        /* 
         *   free the header, which is the actual memory block as far as
         *   malloc was concerned 
         */
        t3free(hdr);
    }

#if 0
    /* removed with the removal of move_var_part() */
    
    /* 
     *   complete garbage collection pass - we don't have to do anything
     *   here, since we can't move objects around to consolidate free
     *   space 
     */
    void finish_gc_pass() { }
#endif
    
private:
};

/* ------------------------------------------------------------------------ */
/*
 *   Hybrid cell-based and malloc-based heap allocator.  This heap manager
 *   uses arrays of fixed-size blocks to allocate small objects, and falls
 *   back on malloc for allocating large objects.  Small-block allocations
 *   and frees are fast, require very little memory overhead, and minimize
 *   heap fragmentation by packing large blocks of fixed-size items into
 *   arrays, then suballocating out of free lists built from the arrays.  
 */

/*
 *   Each item we allocate from a small-object array has a header that
 *   points back to the array's master list.  This is necessary so that we
 *   can put the object back in the appropriate free list when it is
 *   deleted.  
 */
struct CVmVarHeapHybrid_hdr
{
    /* 
     *   the block interface that allocated this object -- we use this
     *   when we free the object so that we can call the free() routine in
     *   the block manager that originally did the allocation 
     */
    class CVmVarHeapHybrid_block *block;

    /* requested allocation size of the block, if collecting statistics */
    IF_GC_STATS(size_t siz;)
};

/*
 *   Hybrid heap allocator - sub-block interface.  Each small-object cell
 *   list is represented by one of these objects, as is the fallback
 *   malloc allocator.  
 */
class CVmVarHeapHybrid_block
{
public:
    virtual ~CVmVarHeapHybrid_block() {}

    /* allocate memory */
    virtual struct CVmVarHeapHybrid_hdr *alloc(size_t siz) = 0;

    /* free memory */
    virtual void free(struct CVmVarHeapHybrid_hdr *) = 0;

    /*
     *   Reallocate memory.  If necessary, allocate new memory, copy the
     *   data to the new memory, and delete the old memory.  We receive
     *   the heap manager as an argument so that we can call it to
     *   allocate new memory if necessary.  
     */
    virtual void *realloc(struct CVmVarHeapHybrid_hdr *mem, size_t siz,
                          class CVmObject *obj) = 0;

    /* requested allocation size of the block, if collecting statistics */
    IF_GC_STATS(size_t siz;)
};

/*
 *   Malloc suballocator 
 */
class CVmVarHeapHybrid_malloc: public CVmVarHeapHybrid_block
{
public:
    /* allocate memory */
    virtual struct CVmVarHeapHybrid_hdr *alloc(size_t siz)
    {
        CVmVarHeapHybrid_hdr *ptr;
        
        /* adjust the size to add in the required header */
        siz = osrndsz(siz + sizeof(CVmVarHeapHybrid_hdr));
        
        /* allocate directly via the default system heap manager */
        ptr = (CVmVarHeapHybrid_hdr *)t3malloc(siz);

        /* fill in the header */
        ptr->block = this;

        /* return the new block */
        return ptr;
    }

    /* release memory */
    virtual void free(CVmVarHeapHybrid_hdr *mem)
    {
        /* release the memory directly to the default system heap manager */
        t3free(mem);
    }

    /* reallocate memory */
    virtual void *realloc(struct CVmVarHeapHybrid_hdr *mem, size_t siz,
                          CVmObject *)
    {
        CVmVarHeapHybrid_hdr *ptr;
        
        /* adjust the new size to add in the required header */
        siz = osrndsz(siz + sizeof(CVmVarHeapHybrid_hdr));

        /* reallocate the block */
        ptr = (CVmVarHeapHybrid_hdr *)t3realloc(mem, siz);

        /* fill in the header in the new block */
        ptr->block = this;

        /* return the caller-visible part of the new block */
        return (void *)(ptr + 1);
    }
};

/*
 *   Small-object array list head.  We suballocate small objects from
 *   these arrays.  We maintain one of these objects for each distinct
 *   cell size; this object manages all of the storage for blocks of the
 *   cell size.  
 */
class CVmVarHeapHybrid_head: public CVmVarHeapHybrid_block
{
public:
    CVmVarHeapHybrid_head(class CVmVarHeapHybrid *mem_mgr,
                          size_t cell_size, size_t page_count)
    {
        /* remember our memory manager */
        mem_mgr_ = mem_mgr;
        
        /* remember our cell size and number of items per array */
        cell_size_ = cell_size;
        page_count_ = page_count;

        /* we have nothing in our free list yet */
        first_free_ = 0;
    }
    
    /* allocate an object from my pool, expanding the pool if necessary */
    CVmVarHeapHybrid_hdr *alloc(size_t siz);

    /* free a cell */
    void free(CVmVarHeapHybrid_hdr *mem);

    /* reallocate */
    virtual void *realloc(struct CVmVarHeapHybrid_hdr *mem, size_t siz,
                          class CVmObject *obj);

    /* get the cell size for this cell manager */
    size_t get_cell_size() const { return cell_size_; }

private:
    /* size of each cell in the array */
    size_t cell_size_;

    /* number of items we allocate per array */
    size_t page_count_;

    /* head of the free list of cells in this array */
    void *first_free_;

    /* our memory manager */
    CVmVarHeapHybrid *mem_mgr_;
};

/*
 *   Small-object array list block.  We dynamically allocate these array
 *   blocks as needed to hold blocks of a particular size.  
 */
struct CVmVarHeapHybrid_array
{
    /* next array in the master list */
    CVmVarHeapHybrid_array *next_array;

    /* 
     *   memory for allocation (we over-allocate the structure to make
     *   room for some number of our fixed-size cells) 
     */
    char mem[1];
};

/*
 *   heap implementation 
 */
class CVmVarHeapHybrid: public CVmVarHeap
{
    friend class CVmVarHeapHybrid_head;
    
public:
    CVmVarHeapHybrid(CVmObjTable *objtab);
    ~CVmVarHeapHybrid();

    /* initialize */
    void init(VMG0_) { }

    /* terminate */
    void terminate() { }

    /* allocate memory */
    void *alloc_mem(size_t siz, CVmObject *obj);

    /* reallocate memory */
    void *realloc_mem(size_t siz, void *varpart, CVmObject *obj);

    /* free memory */
    void free_mem(void *varpart);

#if 0
    /* removed with the removal of move_var_part() */
    
    /* 
     *   complete garbage collection pass - we don't have to do anything
     *   here, since we can't move objects around to consolidate free
     *   space 
     */
    void finish_gc_pass() { }
#endif
    
private:
    /* 
     *   Head of list of arrays.  We keep this list so that we can delete
     *   all of the arrays when we delete this heap manager object itself.
     */
    CVmVarHeapHybrid_array *first_array_;

    /* 
     *   Array of cell-based subheap managers.  This array will be ordered
     *   from smallest to largest, so we can search it for the best fit to
     *   a requested size. 
     */
    CVmVarHeapHybrid_head **cell_heaps_;

    /* number of cell heap managers */
    size_t cell_heap_cnt_;

    /*
     *   Our fallback malloc heap manager.  We'll use this allocator for
     *   any blocks that we can't allocate from one of our cell-based
     *   memory managers. 
     */
    CVmVarHeapHybrid_malloc *malloc_heap_;

    /* object table */
    CVmObjTable *objtab_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Memory Manager - this is the primary interface to the object memory
 *   subsystem.  
 */
class CVmMemory
{
public:
    /* create the memory manager, using a given variable-size heap */
    CVmMemory(VMG_ CVmVarHeap *varheap);

    /* delete the memory manager */
    ~CVmMemory()
    {
        /* tell the variable-size heap to disengage */
        varheap_->terminate();
    }

    /* get the variable heap manager */
    CVmVarHeap *get_var_heap() const { return varheap_; }

private:
    /* variable-size object heap */
    CVmVarHeap *varheap_;

    /* our constant pool manager */
    class CVmPool *constant_pool_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Allocate a new object ID 
 */
inline vm_obj_id_t vm_new_id(VMG_ int in_root_set)
{
    /* ask the global object table to allocate a new ID */
    return G_obj_table->alloc_obj(vmg_ in_root_set);
}

/*
 *   Allocate a new object ID, setting GC characteristics 
 */
inline vm_obj_id_t vm_new_id(VMG_ int in_root_set, int can_have_refs,
                             int can_have_weak_refs)
{
    /* ask the global object table to allocate a new ID */
    return G_obj_table->alloc_obj(vmg_ in_root_set, can_have_refs,
                                  can_have_weak_refs);
}

/*
 *   Given an object ID, get a pointer to the object 
 */
inline CVmObject *vm_objp(VMG_ vm_obj_id_t id)
{
    /* ask the global object table to translate the ID */
    return G_obj_table->get_obj(id);
}

/* ------------------------------------------------------------------------ */
/*
 *   Dynamic cast of a vm_val_t to a given object type.  Returns null if the
 *   vm_val_t isn't an object of the given type.
 */
#define vm_val_cast(cls, vv) \
    (vm_val_cast_ok(cls, vv) ? (cls *)vm_objp(vmg_ (vv)->val.obj) : 0)

/*
 *   Will a dynamic cast of a vm_val_t to the given object type succeed? 
 */
#define vm_val_cast_ok(cls, vv) \
    ((vv)->typ == VM_OBJ && \
     vm_objp(vmg_ ((vv)->val.obj))->is_of_metaclass(cls::metaclass_reg_))

/*
 *   Cast a vm_obj_id_t to a given object type 
 */
#define vm_objid_cast(cls, id) \
    (id != VM_INVALID_OBJ \
     && vm_objp(vmg_ id)->is_of_metaclass(cls::metaclass_reg_) \
     ? (cls *)vm_objp(vmg_ id) : 0)

/*
 *   Get the class object for a given metaclass
 */
#define vm_classobj_for(cls) \
    vm_objid_cast(CVmObjClass, cls::metaclass_reg_->get_class_obj(vmg0_))

#endif /* VMOBJ_H */

/*
 *   Register the root object class
 */
VM_REGISTER_METACLASS(CVmObject)
