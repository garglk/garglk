#charset "us-ascii"

#include <adv3.h>
#include <en_us.h>

/*
 *   CustomStatus - by Mark Engelberg (mark.engelberg@gmail.com)
 *   
 *   CustomStatus is a mix-in class for situations where you want to have
 *   more control over the various status messages that are printed by
 *   default when an object is examined.
 *   
 *   By mixing in this class, default reporting of status is disabled, and
 *   it becomes an "opt-in" system.  This leaves you free to describe the
 *   status of the object as part of its prose description.  
 *   
 *   For convenience, the class contains several status reporting functions
 *   that you can use in your description if a default message will do.
 *   Even if you find yourself using default messages, you may prefer doing
 *   it this way, because you then have full control over where in your
 *   description the status message occurs, and you have the ability to
 *   choose between a few variations (see below).  
 */


CustomStatus : Thing
    // Most status reports occur in examineStatus.  So the first step is to
    // override examineStatus.  This gets rid of open and locked status statements
    // and automatic listing of contents when you examine an object.
    // It doesn't do anything about the way contents are described when they are
    // inside of other objects, or in an inventory.

    examineStatus{}

    // Next, we provide various messages you can mix in to your descriptions to
    // report simple status messages.  It's up to you to make sure you're
    // reporting something meaningful for that kind of object.
    // For example, reportLockedStatus only makes sense on something lockable.
    
    // Reporters for openable objects.
    
    // "It's open. "
    reportOpenStatus {
        say ('\^'+openStatus+'. ');
    }
    
    // "It's currently open. "
    reportCurrentlyOpenStatus {
        say(isOpen ? gLibMessages.currentlyOpen : gLibMessages.currentlyClosed);
    }
    
    // "The object is open. "
    reportDobjOpenStatus {
        "{The dobj/he} is <<openDesc>>. ";
    }
    
    // Reporters for lockable objects.
    
    // Default reporting of lock status respects the lockStatusObvious and
    // lockStatusReportable flags.
    reportLockedStatus {
        if (lockStatusObvious && lockStatusReportable)
            say ( '\^' + itIsContraction + ' ' + lockedDesc + '. ');
    }
    reportCurrentlyLockedStatus {
        if (lockStatusObvious && lockStatusReportable)
            say(isLocked ? gLibMessages.currentlyLocked
                                 : gLibMessages.currentlyUnlocked);
    }
    reportDobjLockedStatus {
        if (lockStatusObvious && lockStatusReportable)
            "{The dobj/he} is <<lockedDesc>>. ";
    }
    
    // Reporters for containers
    
    /* 
     *   If you want to report the contents of something (a container, an 
     *   actor, etc.), you can use this reportContents, which is usually
     *   just a synonym for examineListContents.
     */
    reportContents {
        if (ofKind(RoomPart))  // Listing the contents of a room part is handled a
                                                     // bit differently
            examinePartContents(&descContentsLister);
        else
            examineListContents;
    }

    /* 
     *   If you want specialized lister behavior when reporting the contents, you
     *   can call this reporter with a specific lister as an argument.
     */
    reportContentsWith(lister) {
        examineListContentsWith(lister);
    }   

    /*
     *   The openable class is a major thorn in my side for the purposes of
     *   this opt-in strategy.  The problem is that the openable class
     *   incorporates the open status as part of its container lister.  It
     *   does this by setting the descContentsLister to
     *   openableDescContentsLister. We really don't want the status to be
     *   displayed unless we specify, so we have to set the
     *   descContentsLister back to the basic thingDescContentsLister in
     *   the case that this is an openable object.  The flaw with this
     *   approach is that if you have a class between CustomStatus and
     *   Openable in your class list which defines a different
     *   descContentsLister, this will break it.  Unfortunately, I can't
     *   think of a better strategy right now.
     */
             
    descContentsLister {
        if (ofKind(Openable))
            return(thingDescContentsLister);
        else
            return inherited;
    }
    
    // Often, in containers, it is useful to report a container's open status and
    // its contents in the same sentence.
    reportOpenStatusAndContents {
        examineListContentsWith(openableDescContentsLister);
    }
        
    // Reporters for actors
    
    // If you have a specific lister you'd like to use, you can just
    // use examineListContentsWith, but we give it a reportContentsWith name
    // for consistency with the other reporters.
    
    // Now let's turn our attention to actors that use this mix-in
    // The default examineStatus provides a posture description, and a state
  // description.  Let's provide some default reporters.
    
    reportPosture {
        postureDesc;
    }
    
    reportState {
        curState.stateDesc;
    }
    
    // Combine all the actor status pieces into one default report, if desired.
    reportActorStatus {
        reportPosture;
        reportState;
        reportContents;
    }
    
    // Reporters for attachable objects
    
    reportAttachments {
    local tab;
        
        /* get the actor's visual sense table */
        tab = gActor.visibleInfoTable();

        /* add our list of attachments */
        attachmentLister.showList(gActor, self, attachedObjects,
                                  0, 0, tab, nil);

        /* add our list of major attachments */
        majorAttachmentLister.showList(gActor, self, attachedObjects,
                                       0, 0, tab, nil);
    }
    

;

