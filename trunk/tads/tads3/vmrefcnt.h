/* $Header$ */

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmrefcnt.h - thread-safe reference-counted objects
Function
  Defines classes for reference counting, including weak references.
Notes
  This header needs to be "interleaved" with the osifcnet.h class
  definitions.  The classes we define here depend on the OS-specific
  thread-safe counter object already being defined in-line, while other
  OS-specific classes depend on our classes.  This header must thus be
  #included within the OS-specific networking header after defining the
  counter class but before defining any of the other classes.
Modified
  05/24/10 MJRoberts  - Creation
*/

#ifndef VMREFCNT_H
#define VMREFCNT_H


/* ------------------------------------------------------------------------ */
/*
 *   Reference-counted object.  This is the base class for objects with
 *   memory management by reference count.  Construction sets the initial
 *   reference count to 1, on behalf of the caller that performed the 'new'
 *   to create the object.  Callers must manually release references as they
 *   go out of scope.  When the reference count reaches zero, the object is
 *   destroyed.
 */
class CVmRefCntObj
{
public:
    /* 
     *   on construction, set the counter to 1, to count the initial
     *   reference on behalf of the caller that performed the 'new' 
     */
    CVmRefCntObj() : cnt(1) { }

    /* add a reference */
    void add_ref() { cnt.inc(); }

    /* 
     *   release a reference; automatically deletes the object when the
     *   reference count reaches zero 
     */
    void release_ref()
    {
        if (cnt.dec() == 0)
            delete this;
    }

    /* 
     *   debugging add/remove ref - print a console message showing where the
     *   add/remove came from 
     */
    void add_ref(void *from, const char *msg)
    {
        add_ref();
        printf("add_ref(this=%lx, cnt=%ld, from=%lx, %s)\n",
               (long)this, cnt.get(), (long)from, msg);
    }
    void release_ref(void *from, const char *msg)
    {
        printf("release_ref(this=%lx, cnt=%ld, from=%lx, %s)\n",
               (long)this, cnt.get()-1, (long)from, msg);
        release_ref();
    }
    
    /* get the current reference count */
    long get_ref_count() const { return cnt.get(); }

protected:
    /* 
     *   the destructor must be virtual because this class is designed for
     *   subclassing, and we invoke the destructor from this base class 
     */
    virtual ~CVmRefCntObj() { }

    /* reference counter */
    OS_Counter cnt;
};

/* ------------------------------------------------------------------------ */
/*
 *   Weak reference.  This is a reference to a WeakRefable object that
 *   doesn't count in the reference counting for the referenced object.
 */
class CVmWeakRef: public CVmRefCntObj
{
    friend class CVmWeakRefable;

public:
    /*
     *   Get the referenced object.  If the object is still alive, this
     *   creates a new strong reference to the object and returns the strong
     *   reference.  The caller is responsible for releasing the returned
     *   strong reference when it's done with the reference.  If the object
     *   has already been deleted, this returns null.  
     */
    class CVmWeakRefable *ref();

    /*
     *   Test the reference to see if the object is still alive.  Returns
     *   true if the object is alive, false if not.
     *   
     *   If this returns true, there's of course no guarantee that the object
     *   won't be deleted at any time.  If this returns false, however, you
     *   can be certain that it will return false (and ref() will return nil)
     *   from now on, because it means the underlying object has been
     *   deleted.  test() is thus useful for cases where you only need to
     *   know for sure that an object has been deleted, such as when
     *   periodically cleaning up a cache containing weak references.  When
     *   you need to use the object after determining if it's alive, you
     *   should use ref() instead, since that atomically tests to see if the
     *   object is alive and creates a strong reference to it if it is.  
     */
    int test() { return ref_ != 0; }

protected:
    CVmWeakRef(class CVmWeakRefable *r);

    virtual ~CVmWeakRef();

    /* 
     *   Clear our reference.  The referenced object calls this when it's
     *   about to be deleted, to let us know that we can no longer access it.
     *   
     *   The referenced object only calls this after locking the mutex.
     */
    void clear_ref() { ref_ = 0; }

    /* the object we weakly reference */
    class CVmWeakRefable *ref_;

    /* the mutex we share with the referenced object */
    class OS_Mutex *mu;
};

/*
 *   Weak-referenceable object.  This is a specialized reference-counted
 *   object that can have weak references in addition to regular strong
 *   references.  A weak reference is a reference that doesn't prevent the
 *   object from being deleted, but still allows access as long as the object
 *   is alive.
 */
class CVmWeakRefable: public CVmRefCntObj
{
    friend class CVmWeakRef;
    
public:
    CVmWeakRefable();

    /* release a reference */
    void release_ref();

    /*
     *   Get the weak reference to this object 
     */
    CVmWeakRef *get_weak_ref();

    /* debugging version */
    void release_ref(void *from, const char *msg)
    {
        printf("release_ref(this=%lx, cnt=%ld, from=%lx, %s)\n",
               (long)this, cnt.get()-1, (long)from, msg);
        release_ref();
    }

protected:
    virtual ~CVmWeakRefable();

    /* mutex protecting against concurrent access */
    class OS_Mutex *mu;

    /* our weak reference object */
    CVmWeakRef *weakref;
};

#endif /* VMREFCNT_H */
