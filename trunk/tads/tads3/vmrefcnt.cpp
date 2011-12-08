#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmrefcnt.cpp - thread-safe reference-counted objects
Function
  
Notes
  
Modified
  05/24/10 MJRoberts  - Creation
*/

#include "t3std.h"
#include "osifcnet.h"

/* ------------------------------------------------------------------------ */
/*
 *   Weakly Referenceable Object
 */

/*
 *   Construction 
 */
CVmWeakRef::CVmWeakRef(CVmWeakRefable *r)
{
    /* remember the reference */
    ref_ = r;

    /* 
     *   Remember the object's mutex.  We share the mutex with the referenced
     *   object, because our reference conversion (this->ref()) and its
     *   reference counting (r->release_ref()) both access the same shared
     *   data (namely our ref_ member).  
     */
    mu = r->mu;
    mu->add_ref();
}

/* 
 *   destruction 
 */
CVmWeakRef::~CVmWeakRef()
{
    mu->release_ref();
}

/* 
 *   Get the referenced object.  This returns a strong reference to our
 *   weakly referenced object.  The caller is responsible for releasing the
 *   reference that we return when it's done with it.  
 *   
 *   The operation of checking our reference and incrementing the object's
 *   reference count must be atomic.  This ensures that we're safe even if
 *   there's another thread that's about to release the last existing strong
 *   reference on the object.  That other thread's release will either
 *   complete before we check our reference, in which case our reference will
 *   be nulled out when we check it; OR we'll begin our atomic check/inc
 *   operation first, in which case the other thread will be locked out until
 *   after our inc, meaning we now have our own strong reference on the
 *   object and it won't be deleted until our caller releases the new strong
 *   reference.  
 */
CVmWeakRefable *CVmWeakRef::ref()
{
    /* lock the mutex while we're working */
    mu->lock();

    /* get the object */
    CVmWeakRefable *r = ref_;

    /* 
     *   If we still have a reference to the object, it means it's still in
     *   memory and still has at least one other referencer.  As long as we
     *   have the mutex locked, it's impossible for anyone else to release
     *   their reference, so the object definitely will stay around at least
     *   until we unlock the mutex.
     *   
     *   Add a strong reference on behalf of our caller.  This ensures that
     *   the object lives on for at least as long as the caller needs it,
     *   because whatever else happens, they have a strong reference on it
     *   starting now.  Even if another thread releases the last pre-existing
     *   reference on the object the moment we unlock the mutex, it doesn't
     *   matter: before unlocking the mutex, we add our caller's strong
     *   reference.  
     */
    if (r != 0)
        r->add_ref();
    
    /* done with the mutex */
    mu->unlock();
    
    /* return the object */
    return r;
}


/* ------------------------------------------------------------------------ */
/*
 *   Weak-referencable object 
 */

/*
 *   construction 
 */
CVmWeakRefable::CVmWeakRefable()
{
    /* we don't have a weak ref yet */
    weakref = 0;

    /* create our mutex */
    mu = new OS_Mutex();
}

/*
 *   destruction 
 */
CVmWeakRefable::~CVmWeakRefable()
{
    /* done with our mutex */
    mu->release_ref();

    /* if we have a weak ref object, release it */
    if (weakref != 0)
        weakref->release_ref();
}

/*
 *   Get a weak reference to this object.  This returns a CVmWeakRef pointer
 *   that can be used to obtain a strong reference to 'this' on demand, but
 *   which doesn't by itself keep 'this' alive.  If the WeakRef pointer is
 *   dereferenced after 'this' is deleted, the dereference returns null.  
 */
CVmWeakRef *CVmWeakRefable::get_weak_ref()
{
    /* lock out other updaters */
    mu->lock();
    
    /* if we don't already have a weak reference object, create one */
    if (weakref == 0)
        weakref = new CVmWeakRef(this);

    /* done with the lock */
    mu->unlock();
    
    /* add a reference on behalf of our caller */
    weakref->add_ref();

    /* return the weak reference object */
    return weakref;
}

/*
 *   Release a reference.
 *   
 *   The reference count update must be atomic with clearing any weak
 *   reference pointing to us: this ensures that a weak referencer can't
 *   convert its weak reference into a strong reference after the last
 *   existing strong reference has performed a release, which initiates
 *   deletion.  Any weak referencer that gets in just before we begin the
 *   atomic dec/clear operation will be safe, because it will obtain its
 *   strong reference, which increments our reference count, BEFORE we do our
 *   own decrement.  Thus the reference count will not reach zero in that
 *   case until the new strong reference created by the weak reference
 *   conversion is itself released.  
 */
void CVmWeakRefable::release_ref()
{
    /* lock the object while we're working */
    mu->lock();
    
    /* decrement the reference counter */
    int del = FALSE;
    if (cnt.dec() == 0)
    {
        /* if we have a weak referencer, clear its pointer to me */
        if (weakref != 0)
            weakref->clear_ref();
        
        /* note that it's time to delete the object */
        del = TRUE;
    }
    
    /* done with our lock */
    mu->unlock();
    
    /* if we're unreferenced, delete myself */
    if (del)
        delete this;
}

