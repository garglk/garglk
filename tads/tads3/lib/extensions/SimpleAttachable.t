#charset "us-ascii"

#include <adv3.h>
#include <en_us.h>

/*  
 *   SIMPLE ATTACHABLE
 *
 *   SimpleAttachable version 1.0 by Eric Eve
 *
 *   This file defines the SimpleAttachable class together with some 
 *   supporting objects. Feel free to use this in your own games if you find 
 *   it useful.
 *
 *   Attachables in general are complicated to handle, because they can 
 *   behave in so many different ways. The SimpleAttachable class is meant 
 *   to make handling one common case easier, in particular the case where a 
 *   smaller object is attached to a larger object and then moves round with 
 *   it.
 *
 *   More formally, a SimpleAttachable enforces the following rules:
 *
 *   (1)    In any attachment relationship between SimpleAttachables, one 
 *   object must be the major attachment, and all the others will be that 
 *   object's minor attachments (if there's a fridge with a red magnet and a 
 *   blue magnet attached, the fridge is the major attachement and the 
 *   magnets are its minor attachments).
 *
 *   (2)    A major attachment can have many minor attachments attached to 
 *   it at once, but a minor attachment can only be attached to one major 
 *   attachment at a time (this is a consequence of (3) below).
 *
 *   (3)    When a minor attachment is attached to a major attachment, the 
 *   minor attachment is moved into the major attachment. This automatically 
 *   enforces (4) below.
 *
 *   (4)    When a major attachment is moved (e.g. by being taken or pushed 
 *   around), its minor attachments automatically move with it.
 *
 *   (5)    When a minor attachment is taken, it is automatically detached 
 *   from its major attachment (if I take a magnet, I leave the fridge 
 *   behind).
 *
 *   (6)    When a minor attachment is detached from a major attachment it 
 *   is moved into the major attachment's location. 
 *
 *   (7)     The same SimpleAttachable can be simultaneously a minor item 
 *   for one object and a major item for one or more other objects (we could 
 *   attach a metal paper clip to the magnet while the magnet is attached to 
 *   the fridge; if we take the magnet the paper clip comes with it while the
 *   fridge is left behind).
 *
 *   (8)    If a SimpleAttachable is attached to a major attachment while 
 *   it's already attached to another major attachment, it will first be 
 *   detached from its existing major attachment before being attached to 
 *   the new one (ATTACH MAGNET TO OVEN will trigger an implicit DETACH 
 *   MAGNET FROM FRIDGE if the magnet was attached to the fridge).
 *
 *   (9)    Normally, both the major and the minor attachments should be of 
 *   class SimpleAttachable. 
 *
 *
 *   Setting up a SimpleAttachable is then straightforward, since all the 
 *   complications are handled on the class. In the simplest case all the 
 *   game author needs to do is to define the minorAttachmentItems property 
 *   on the major SimpleAttachable to hold a list of items that can be 
 *   attached to it, e.g.:
 *
 *   minorAttachmentItems = [redMagnet, blueMagnet]
 *
 *   If a more complex way of deciding what can be attached to a major 
 *   SimpleAttachable is required, override its isMajorItemFor() method 
 *   instead, so that it returns true for any obj that can be attached, e.g.:
 *
 *   isMajorItemFor(obj) { return obj.ofKind(Magnet); }
 *
 *   One further point to note: if you want a Container-type object to act 
 *   as a major SimpleAttachment, you'll need to make it a ComplexContainer.
 *
 */

ModuleID
  name = 'SimpleAttachable'
  byLine = 'by Eric Eve'
  htmlByLine = 'by <A href="mailto:eric.eve@hmc.ox.ac.uk">Eric Eve</a>'
  version = '1.0'  
;


class SimpleAttachable: Attachable
    
    /* Move the minor attachment into the major attachment. */
    handleAttach(other)
    {
        if(other.isMajorItemFor(self))           
            moveInto(other);
        
    }
    
    /* 
     *   When we're detached, if we were in the other object move us into the
     *   other object's location.
     */
    handleDetach(other)
    {
        if(isIn(other))
            moveInto(other.location);
    }
    
    /* 
     *   If a minor attachment is taken, first detach it from its major 
     *   attachment.
     */
    dobjFor(Take)
    {
        preCond = (nilToList(inherited) + objNotAttachedToMajor)
    }
    
    
    /*  
     *   If we're attached to a major attachment, treat TAKE US FROM MAJOR as
     *   equivalent to DETACH US FROM MAJOR.
     */
    dobjFor(TakeFrom) maybeRemapTo(isAttachedToMajor, DetachFrom, self,
                                   location)
       
    /*  
     *   If we're already attached to a major attachment, detach us from it 
     *   before attaching us to a different major atttachment.
     */
    dobjFor(AttachTo)
    {
        preCond = (nilToList(inherited) + objDetachedFromLocation)
    }
       
    iobjFor(AttachTo)
    {
        preCond = (nilToList(inherited) + objDetachedFromLocation)
    }
    
    /*  We're a major item for any item in our minorAttachmentItems list. */
    isMajorItemFor(obj)
    {
        return nilToList(minorAttachmentItems).indexOf(obj) != nil;
    }
    
    /*  
     *   The list of items that can be attached to us for which we would be 
     *   the major attachment item.
     */
    minorAttachmentItems = []
    
    /*   
     *   A pair of convenience methods to determined if we're attached to any
     *   items that are major or minor attachments relative to us.
     */
    isAttachedToMajor = (location && location.isMajorItemFor(self))
    isAttachedToMinor = (contents.indexWhich({x: self.isMajorItemFor(x)}) !=
                      nil)
    
    /*   
     *   Define if this item be listed when it's a minor item attached to 
     *   another item.
     */
    isListedWhenAttached = true
    
    isListed = (isAttachedToMajor ? isListedWhenAttached : inherited )
    
    isListedInContents = (isAttachedToMajor ? nil : inherited )
    
    /*  
     *   Customise the listers so that if we contain minor items as 
     *   attachments they're shows as being attached to us, not as being in 
     *   us.
     */    
    contentsLister =  (isAttachedToMinor ? majorAttachmentLister : inherited)
    
    inlineContentsLister = (isAttachedToMinor ? inlineListingAttachmentsLister :
                        inherited )

   
    /*  
     *   A SimpleAttachment can be attached to another SimpleAttachment if 
     *   one of the SimpleAttachments is a major item for the other.
     */
    canAttachTo(obj)
    {
        return isMajorItemFor(obj) || obj.isMajorItemFor(self);
    }
    
    
    /*  
     *   If I start the game located in an object that's a major item for me, 
     *   presumbably we're meant to start off attached.
     */
    initializeThing()
    {
        inherited;
        
        if(location && location.isMajorItemFor(self))            
            attachTo(location);
    }
;

/*  
 *   Custom lister to show the contents of a major attachment as being 
 *   attached to it.
 */
inlineListingAttachmentsLister: ContentsLister
    showListEmpty(pov, parent) { }
    showListPrefixWide(cnt, pov, parent)
        { " (to which <<cnt > 1 ? '{are|were}' : '{is|was}'>>  attached "; }
    showListSuffixWide(itemCount, pov, parent)
        { ")"; }
;

/*  Special precondition for use when taking a minor attachment. */

objNotAttachedToMajor: PreCondition
    
    /* 
     *   Other things being equal, prefer to take an item that's not a minor 
     *   attachment (if the blue magnet is attached to the fridge and the red
     *   magnet is lying on the floor, then make TAKE MAGNET take the red 
     *   one).
     */
    verifyPreCondition(obj) 
    { 
        if(obj.location.isMajorItemFor(obj))
            logicalRank(90, 'attached');
    }

    
    checkPreCondition(obj, allowImplicit)
    {
        /* 
         *   if we don't already have any non-permanent attachments  that 
         *   are the major attachments for us, we're fine (as we don't 
         *   require removing permanent attachments); nothing more needs to 
         *   be done.  
         */
        if (obj.attachedObjects.indexWhich(
            {x: x.isMajorItemFor(obj) && !obj.isPermanentlyAttachedTo(x) 
                  }) == nil)
            return nil;

                
        local major = obj.attachedObjects.valWhich({x: x.isMajorItemFor(obj)});
        
        /* 
         *   Try implicitly detaching us from our major attachment.  
         */
        if (allowImplicit && tryImplicitAction(DetachFrom, obj, major))
        {
            /* 
             *   if we're still attached to a major attachment, we failed, 
             *   so abort
             */
            if (obj.attachedObjects.indexWhich(
                {x: !obj.isPermanentlyAttachedTo(x) 
                  && x.isMajorItemFor(obj)}) != nil)
                exit;

            /* tell the caller we executed an implied action */
            return true;
        }

        /* we must detach first */
        reportFailure(&mustDetachMsg, obj);
        exit;
    }
;

/*  
 *   Special precondition to detach us from an existing major attachment 
 *   before attaching us to another one. This needs to be different from 
 *   objNotAttachedToMajor so that we don't perform any unnecessary 
 *   detachments. 
 */

objDetachedFromLocation: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* 
         *   If the other object involved in the command is not a majorItem   
         *   for us, or we're not already in an object that we're attached 
         *   to which is our major item, then there's nothing to do. 
         */
        
        local other = (obj == gDobj ? gIobj : gDobj);
        local loc = obj.location;
            
        if(!other.isMajorItemFor(obj) || 
            !(loc.isMajorItemFor(obj) && obj.isAttachedTo(loc)))
            return nil;

                
               
        /* 
         *   if we don't already have any non-permanent attachments  that 
         *   are the major attachments for us, we're fine (as we don't 
         *   require removing permanent attachments); nothing more needs to 
         *   be done.  
         */
        if (allowImplicit && tryImplicitAction(DetachFrom, obj, loc))
        {
            /* if we're still attached to anything, we failed, so abort */
            if (loc.isMajorItemFor(obj) && obj.isAttachedTo(loc))
                exit;

            /* tell the caller we executed an implied action */
            return true;
        }

        /* we must detach first */
        reportFailure(&mustDetachMsg, obj);
        exit;
    }
    
;
