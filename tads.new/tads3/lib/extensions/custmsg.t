#charset "us-ascii"

#include <adv3.h>
#include <en_us.h>


/*
 *   custmsg.t  
 *.    by  Eric Eve (eric.eve@hmc.ox.ac.uk)
 *.    Version 1.1 - 28-Mar-08
 *
 *   A small extension that makes it easier to customize or suppress certain 
 *   library-generated pieces of descriptive text.
 *
 *   (1)    If postureReportable is set to nil on an ActorState, the 
 *   library-generated postureDesc() (e.g. "Mavis is sitting on the wooden 
 *   chair. ") is no longer incorporated into the description in response to 
 *   an EXAMINE command.
 *
 *   (2)    Openable now has an openStatusReportable property (analogous to 
 *   lockStatusReportable on Lockable) that makes it easy to suppress the 
 *   "it's open" and "it's closed" messages that get tagged onto the end of 
 *   the descriptions of Openable objects.
 *
 *   (3)   It's now easier to customize or suppress the "(providing light)" 
 *   and "(lit)" messages that appear after the names of LightSources in 
 *   inventory listings and the like; just override the providingLightMsg on 
 *   the object in question. The mechanism can be extended to use other 
 *   properties on other classes of objects using author-defined ThingStates. 
 *
 *   (4)   The new Thing property specialContentsListedInExamine can be set 
 *   to nil to suppress the listing of any contents that uses a specialDesc. 
 */

/* Version 1.1 adds the openStatusReportable property to ContainerDoor */


/* 
 *   The modifications to Actor and ActorState allow authors to opt in to 
 *   including a postureDesc (e.g, "Mavis is sitting on the wooden chair. ") 
 *   when a stateDesc is already provided, instead of having to opt out.
 */

modify Actor
    replace examineStatus()
    {
        /* 
         *   If I'm an NPC, show where I'm sitting/standing/etc.  (If I'm
         *   the PC, we don't usually want to show this explicitly to avoid
         *   redundancy.  The player is usually sufficiently aware of the
         *   PC's posture by virtue of being in control of the actor, and
         *   the information also tends to show up often enough in other
         *   places, such as on the status line and in the room
         *   description.)  
         *
         *   Also, we only show this if the curState's postureReportable
         *   property is true. This allows redundant posture descriptions
         *   to be suppressed if the current ActorState's stateDesc already
         *   mentions them.
         */
        if (!isPlayerChar() && curState.postureReportable)
            postureDesc;

        
        /* show the status from our state object */
        curState.stateDesc;

               
        /* inherit the default handling to show our contents */
        inherited();
    }    
;

modify ActorState    
    
    /* 
     *   By default the actor's postureDesc() is displayed between its desc 
     *   and the stateDesc on the current ActorState. If the stateDesc 
     *   already mentions the actor's posture this may be redundant; changing
     *   postureReportable to nil suppresses the display of the 
     *   postureDesc().
     */
    postureReportable = true
;

//------------------------------------------------------------------------------

/* 
 *   The following modification to ThingState is designed to make it easier to
 *   customize state-dependent messages such as '(providing light)' or 
 *   '(being worn)'. The mechanism should be readily extensible to 
 *   author-defined ThingStates. 
 */

modify LightSource
    /* 
     *   The message that's appended to our name in inventory listings and the
     *   like when we're lit; the default in the English language library is 
     *   '(providing light)'. To suppress the message altogether, set this 
     *   property to nil. To replace it with another message, omit the 
     *   parentheses, e.g. on a flashlight you might define:
     *
     *   providingLightMsg = 'currently switched on'
     */
    providingLightMsg = lightSourceStateOn.listName_
    notProvidingLightMsg = lightSourceStateOff.listName_
;

modify Matchstick
    /* 
     *   The message that's appended to our name in inventory listings and the
     *   like when we're lit; the default in the English language library is 
     *   '(lit)'. To suppress the message altogether, set this 
     *   property to nil. To replace it with another message, omit the 
     *   parentheses, e.g. :
     *
     *   providingLightMsg = 'now ablaze'
     */
    providingLightMsg = matchStateLit.listName_
    notProvidingLightMsg = matchStateUnlit.listName_
;

modify Wearable
    /*
     *   The messages that's appended to our name in listing when we're worn 
     *   or unworn. By default this will be '(worn)' or nothing; but the 
     *   wornMsg won't be used if we're appearing in a list of items 
     *   introduced as being worn (e.g. "You are wearing...").
     */
    wornMsg = wornState.listName_
    unwornMsg = unwornState.listName_
;


modify ThingState
    listName(lst)
    {
        /* 
         *   if we have a msgProp and there's one item in our list, use the 
         *   msgProp of that item to provide our state-dependent text.
         */
        if(msgProp != nil && lst.length() == 1)
            return (lst[1]).(msgProp);
        
        /*  Otherwise, use our listName_ (as in the standard library) */
        return listName_;
    }
    
    /* 
     *   The property on the object being described that will provide the 
     *   text for any state-dependent extra description, such as 'providing 
     *   light'. If this is defined, it should be set to a property pointer, 
     *   e.g. &providingLightMsg. If it is left at nil ThingState will behave
     *   just as it does in the standard library.
     */
    msgProp = nil
;

modify lightSourceStateOn    
    msgProp = &providingLightMsg
;

modify matchStateLit
    msgProp = &providingLightMsg
;

modify wornState
    msgProp = &wornMsg
;

/* 
 *   Although the following three ThingStates don't display any state-related 
 *   text in the standard library, equivalent modifications are added here 
 *   to make it easy to add object-specific state information (e.g. 
 *   'currently switched off' on a flashlight) if so desired). This does not 
 *   affect the standard library behaviour unless a game author chooses to 
 *   override the notProvidingLightMsg_ property anywhere. These two 
 *   ThingStates can also be given a listName_ property if a globally 
 *   applicable message is required (e.g. 'unlit' for every unlit matchstick)
 */

modify lightSourceStateOff
    msgProp = &notProvidingLightMsg
;

modify matchStateUnlit
    msgProp = &notProvidingLightMsg
;

modify unwornState
    msgProp = &unwornMsg
;


/* 
 *   Note that the same coding pattern can be used if desired on custom 
 *   ThingStates. E.g.
 *
 *.   class MyCustomClass: Thing
 *.      allStates = [sillyState, sensibleState]
 *.      getState = sillyState
 *.      sillyMsg = sillyState.listName_
 *.      sensibleMsg = sensibleState.listName_
 *.   ;
 *.
 *.   sillyState: ThingState 'silly'
 *.     stateTokens = ['silly']
 *.     msgProp = &sillyMsg
 *.   ;
 *.
 *.   sensibleState: ThingState 'sensible'
 *.     stateTokens = ['sensible']
 *.     msgProp = &sensibleMsg
 *.   ;
 *
 *   Note, however, that there is no need to define the msgProps if the 
 *   ThingStates are being created for a single object, or if all the 
 *   objects they will apply to are to use the same state-specific messages; 
 *   in these cases ThingStates may be defined and used just as they are in 
 *   the standard library. For example, if we never wanted to vary the 
 *   'silly' and 'sensible' messages on different objects, there would be no 
 *   need (and not point) to define sillyMsg and sensibleMsg on 
 *   MyCustomClass, and no need to define msgProp on sillySrate and 
 *   sensibleState.
 *
 */


//------------------------------------------------------------------------------

modify Openable
    /* 
     *   openStatusReportable works on analogy with lockStatusReportable on 
     *   Lockable. It allows us to suppress the 'it's open' and 'it's closed' 
     *   messages that follow the description of an Openable object (by 
     *   setting this property to nil). By default this property is true, 
     *   giving the standard library behaviour.
     *
     *   It may also be useful to set this property to the value of an 
     *   expression, e.g.:
     *
     *   openStatusReportable = (isOpen)
     *
     *   would result in the 'it's open' message being appended when we're 
     *   open, but nothing being appended when we're closed.
     */
    openStatusReportable = (canInherit() ? inherited() : true)
;

modify openableContentsLister
    showListEmpty(pov, parent)
    {
        if(parent.openStatusReportable)
           "\^<<parent.openStatus>>. ";
    }
    showListPrefixWide(itemCount, pov, parent)
    {
        if(parent.openStatusReportable)
           "\^<<parent.openStatus>>, and";
        else
           "\^<<parent.theName>>";
        " contain<<parent.verbEndingSEd>> ";
    }
;

/* 
 *   Make openStatusReportable work on ContainerDoor in the same way as it 
 *   works on Openable.
 */

modify ContainerDoor
    openStatusReportable = (canInherit() ? inherited() : true)
    
    /* 
     *   If we set OpenStatusReportable is set to nil, we may want to 
     *   incorporate the open/closed state into our owm description. We 
     *   borrow openDesc from Openable to facilitate this.
     */
    openDesc = (delegated Openable)
    
    
    replace examineStatus()
    {
        /* add our open status, if it is reportable */
        if(openStatusReportable)
            say(isOpen
                ? gLibMessages.currentlyOpen : gLibMessages.currentlyClosed);

        /* add the base class behavior */
        inherited();
    }

;

//------------------------------------------------------------------------------

modify Thing
    examineSpecialContents()
    {
        if(specialContentsListedInExamine)
            inherited;
    }
    
    /* 
     *   specialContentsListedInExamine works analogously to 
     *   contentsListedInExamine, except that if it is nil it is the listing 
     *   of this object's special contents (i.e. any contained object that 
     *   uses a specialDesc or initSpecialDesc, including actors) that will 
     *   be suppressed.
     */
    
    specialContentsListedInExamine = true
;
