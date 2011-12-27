#charset "us-ascii"

#include <adv3.h>
#include <en_us.h>

  /* 
   *   smartAccompany.t
   *
   *   by Eric Eve
   *
   *   A small extension that improves the ordering and grouping of reports 
   *   from an NPC in an AccompanyingState following the player character. 
   *   For a full explanation of what this extension does, and how it works, 
   *   see Example 3 in the Tech Man article on Manipulating the Transcript. 
   *
   *   Version 1.3; 27-May-09
   *
   *   Version 1.3 fixes a bug which causes the name of the following actor 
   *   (and nothing else) to be displayed in a separate sentence when the 
   *   actor follows the player character from a dark room. 
   *
   *   version 1.2; 04-May-09
   *
   *   Version 1.2 fixes a bug that can cause a run-time error when a report 
   *   in the transcript doesn't have a messageText_ property. 
   *
   *   Version 1.1 fixes a bug that could cause a run-time error when travel 
   *   between rooms is disallowed.
   *
   *
   */

ModuleID
    name = 'smartAccompany'
    byline = 'by Eric Eve'
    htmlByline = 'by <a href="mailto:eric.eve@hmc.ox.ac.uk">Eric Eve</a>'
    version = '1.3'
    listingOrder = 75
;

/*  
 *   Combine reports of the NPC's actions in the old location into a single 
 *   sentence and move it to just before the new room description.
 */

modify AccompanyingState
    beforeTravel(traveler, connector)
    {
        gAction.callAfterActionMain(self); 
        inherited(traveler, connector);
    }
    
    afterActionMain()
    {
        /* 
         *   Find the insertion point, which is just before the description 
         *   of the new room.
         */
        
        local idx = gTranscript.reports_.indexWhich({ x: x.messageText_ ==
                                              '<.roomname>' });
        
        /* 
         *   If there is no insertion point, travel failed for some reason, 
         *   in which case we don't want to change the transcript at all.
         */        
        
        if(idx == nil)
            return;
        
        local actor = getActor;       
        local str = '';
        local vec = new Vector(4);   
        local pat = new RexPattern('<NoCase>^' + actor.theName + '<Space>+');
        
        /*  
         *   Look through all the reports in the transcript for those whose 
         *   message text begins with the name of our actor, followed by at 
         *   least one space (so that if the actor's name is Rob we don't 
         *   pick up any messages relating to Roberta, for example), 
         *   regardless of case (so we pick up messages about 'the tall man' 
         *   and 'The tall man'). 
         *
         *   Once we get to the description of a new room, stop looking (we 
         *   don't want to include the description of the actor arriving in 
         *   the new location, just the actor departing the old one).
         *
         *   Store the relevant message strings (those before the new room 
         *   description, but not any after it) in the vector vec; at the 
         *   same time remove these reports from the transcript (since we'll 
         *   be replacing them with a single report below).
         */
        
        while((idx = gTranscript.reports_.indexWhich({ x: x.messageText_ != nil
            && rexMatch(pat, x.messageText_) })) != nil)
        {
            if(idx > gTranscript.reports_.indexWhich({ x: x.messageText_ ==
                                              '<.roomname>' }))
               break;  

            
            vec.append(gTranscript.reports_[idx].messageText_);
            gTranscript.reports_.removeElementAt(idx);
        }
        
        local len = actor.theName.length() + 1;
        
        /* 
         *   Now go through each of the strings in vec in turn, stripping 
         *   off the actor's name at the start and the period/full-stop at 
         *   the end (so that, for example 'Bob stands up. ' becomes 'stands 
         *   up'. In searching for the position of the terminating full 
         *   stop (period) we start beyond the end of the actor's name in 
         *   case the actor's name contains a full stop (e.g. Prof. Smith).
         */
        
        vec.applyAll( { cur: cur.substr(len, cur.find('.', len) - len)});
        
        /*   
         *   Concatanate these truncated action reports into a single string 
         *   listing the actor's actions (e.g. 'stands up and comes with 
         *   you'), separating the final pair of actions with 'and' and any 
         *   previous actions with a comma. We can use stringLister (defined 
         *   in the previous example) to do this for us.
         */        
        
        str = stringLister.makeSimpleList(vec.toList);
        
        /*   
         *   If we're left with an empty string, there's nothing left to do, 
         *   so we should stop here rather than go on to display a 
         *   fragmentary sentence.
         */
        
        if(str == '')
            return;
        
        /* 
         *   Put the actor's name back at the start of the string, and 
         *   conclude the string with a full-stop and a paragraph break.
         */
        str = '\^' + actor.theName + ' ' + str + '.<.p>';
        
        /* 
         *   Find the insertion point, which is just before the description 
         *   of the new room. This may have changed as a result of our 
         *   manipulations above.
         */
        
        idx = gTranscript.reports_.indexWhich({ x: x.messageText_ ==
                                              '<.roomname>' });
                
        /* 
         *   Insert the message we just created as a new report at the 
         *   appropriate place, i.e. just before the new room description.
         */
        gTranscript.reports_.insertAt(idx, new MessageResult(str));
        
    }
;

/* 
 *   The modifications on AccompanyingState probably aren't so suitable for a 
 *   GuidedTourState, so we make afterActionMain() do nothing by default. This
 *   can be overridden by game authors if desired.
 */

modify GuidedTourState    
    afterActionMain() { }
;

/*
 *   Ensure that the default sayDeparting() message from 
 *   AccompanyingInTravelState is added to the transcript in a single report, 
 *   starting with the NPC's name. 
 */

modify AccompanyingInTravelState
    sayDeparting(conn)
    {
        local msg = mainOutputStream.captureOutput( {: inherited(conn) });;
        if(msg.startsWith('\^'))
            msg = msg.substr(2);
        mainReport(msg);
    }
;

