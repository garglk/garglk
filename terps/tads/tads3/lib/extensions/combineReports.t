#charset "us-ascii"

#include <adv3.h>
#include <en_us.h>

  /* 
   *   Combine Reports
   *
   *   by Eric Eve
   *
   *   Version 1.04 (27-Feb-11)
   *
   *   This extension is a further development of Example 2 in the Technical 
   *   Manual article on Manipulating the Transcript. If this extension is 
   *   included in your game, reports of taking, dropping, or putting 
   *   (in/on/under/behind) a series of reports will be combined into a 
   *   single report listing the objects taken, dropped, or put. Reports of 
   *   actions that failed will appear in the normal way, but will be moved 
   *   to the end so as not to interrupt the combined report of successful 
   *   actions.
   *
   *   With this extension installed, output like:
   *
   *.    blue ball: Taken
   *.    red pen: Taken
   *.    gold coin: Taken
   *.    gold coin: Taken
   *
   *   Is grouped into a single sentence like:
   *
   *.    You take the blue ball, the red pen, and the two gold coins.
   *
   *   For PUT actions the extension also groups any implicit take action 
   *   reports in the same way. e.g. instead of:
   *
   *   (first taking the red book, then taking the green ball, then taking 
   *   the pen, then taking the gold coin, then taking the gold coin)
   *
   *   We get:
   *
   *   (first taking the red book, the green ball, the pen and two gold coins)
   *
   *   Version History:
   *
   *   Version 1.03 tidies up the output from a multiple TAKE FROM command
   *
   *   Version 1.02 makes use of the new standard Library class SimpleLister
   *
   *   Version 1.01 makes a couple of things more customizable, and corrects 
   *   a typo in the report produced for dropping multiple objects.
   *
   */



ModuleID
    name = 'combineReports'
    byline = 'by Eric Eve'
    htmlByline = 'by <a href="mailto:eric.eve@hmc.ox.ac.uk">Eric Eve</a>'
    version = '1.03'
    listingOrder = 75
;

/*  first define a useful lister */

definiteObjectLister: SimpleLister   
        
    /* 
     *   Use the definite article form of the object ('the ball') unless its 
     *   one of several identical objects, in which case the indefinite form 
     *   ('a ball') is more appropriate.
     */
    showListItem(obj, options, pov, infoTab)
    {
        say(obj.isEquivalent ? obj.aName : obj.theName);
    }
;



/* modify CommandReport to keep a note of the direct object it refers to */

modify CommandReport
    construct()
    {
        inherited();
        dobj_ = gDobj;
    }
    dobj_ = nil    
;



/* Make TAKE, DROP and PUT combine reports by using the actionReportManager */

modify TakeAction
    afterActionMain()
    {
        inherited;
        if(parentAction == nil)
           actionReportManager.afterActionMain();
    }
    vCorrect(str) {  return '{take[s]|took}';  }
;

modify TakeFromAction
    afterActionMain()
    {
        inherited;
        if(parentAction == nil)
           actionReportManager.afterActionMain();
    }    
    vCorrect(str) {  return '{take[s]|took}';  }
;

modify DropAction
    afterActionMain()
    {
        inherited;
        if(parentAction == nil)
           actionReportManager.afterActionMain();
    }
    vCorrect(str) {  return 'drop{s/ped}';  }
;


modify PutOnAction
    afterActionMain()
    {
        inherited;
        if(parentAction == nil)
           actionReportManager.afterActionMain();
    }
    vCorrect(str) { return '{put[s]|put}'; }
;

modify PutInAction
    afterActionMain()
    {
        inherited;
        if(parentAction == nil)
           actionReportManager.afterActionMain();
    }
    vCorrect(str) { return '{put[s]|put}'; }
;

modify PutUnderAction
    afterActionMain()
    {
        inherited;
        if(parentAction == nil)
           actionReportManager.afterActionMain();
    }
    vCorrect(str) { return '{put[s]|put}'; }
;

modify PutBehindAction
    afterActionMain()
    {
        inherited;
        if(parentAction == nil)
           actionReportManager.afterActionMain();
    }
    vCorrect(str) { return '{put[s]|put}'; }
;

/* 
 *   The actionReport Manager does most of the work. It is hopefully 
 *   sufficiently general that it could be made to work with other actions 
 *   besides TAKE, DROP and PUT IN/ON/UNDER/BEHIND. Experiment at your own 
 *   risk!  
 */

actionReportManager: object
    
    /* 
     *   The minimum number of objects that must be in a list before we 
     *   attempt to summarize it. Normally this will be 2, but it may be 
     *   that some games will want to change it to 1, for example, so that 
     *   the same reporting style is used for singleton objects as for lists.
     */
    
    minLengthToSummarize = 2
    
    afterActionMain()
    {        
        /* 
         *   If the action isn't iterating over at least 
         *   minLengthToSummarize direct objects we have nothing to do, so 
         *   we'll stop before doing any messing with the transcript
         */
        
        if(gAction.dobjList_.length() < minLengthToSummarize)
            return;
        
          
        /* 
         *   First move any reports of failed attempts to the end, so they 
         *   can be fully reported after the summary of the actions that 
         *   succeeded.
         */
              
        
        local len = gTranscript.reports_.length;
        
        for(local i = gTranscript.reports_.indexWhich({x: x.isFailure}); 
            i != nil && i <= len; 
            i = gTranscript.reports_.indexWhich({x: x.isFailure}))
        {               
                                   
            /* 
             *   first find the MultiObjectAnnouncement relating to this 
             *   failure
             */
            
            local idx1 = i;
            while(idx1 > 1 
                  && !gTranscript.reports_[idx1].ofKind(MultiObjectAnnouncement))
                idx1--;
            
            /* then find the next MultiObjectAnnouncement */
            local idx2 = i;
            while(idx2 <= len 
                  && !gTranscript.reports_[idx2].ofKind(MultiObjectAnnouncement))
                idx2++;
            
            /* Extract a list of all reports between these markers */
            local objVec = new Vector(20).copyFrom(gTranscript.reports_,
                                                   idx1, 1, idx2 - idx1);
            
            /* 
             *   Ensure that all reports about this object are marked as 
             *   failures
             */
            
            objVec.forEach({x: x.isFailure = true });
            
            /* Move this list to the end of the transcript */
            gTranscript.reports_.removeRange(idx1, idx2-1);
            
            gTranscript.reports_.appendAll(objVec);                   
            
            /* 
             *   We don't want to check these reports again, so reduce len by
             *   the number of reports we just moved.
             */
            len -= objVec.length();
            
        }
        
        /* 
         *   Give the game author the opportunity of further processing the 
         *   failure reports, if desired. We also check the summarizeFailures
         *   flag (nil by default) so that we don't carry out any pointless 
         *   processing if we don't need it here. It also leaves open the 
         *   possibility of some future version of this extension defining 
         *   its own version of processFailures(), which game authors can 
         *   then opt in to using.          
         */
        if(summarizeFailures && gTranscript.reports_.length > len)
        {
            local failVec 
                = processFailures(new Vector(20).copyFrom(gTranscript.reports_, 
                    len + 1, 1, gTranscript.reports_.length() - len));
            
            gTranscript.reports_ = gTranscript.reports_.setLength(len) +
                failVec;
        }
         /* 
         *   Define this function separately as we'll use it more than once; 
         *   the function identifies implicit action reports relating to 
         *   taking things.
         */
        
        local impFunc = {x: x.ofKind(ImplicitActionAnnouncement) 
            && x.action_.ofKind(TakeAction) && !x.isFailure };

        
        /* 
         *   Count how many implicit action reports there are relating to 
         *   taking things.
         */
        local impActions = gTranscript.reports_.countWhich(impFunc);
                   
        /*  We only need to do anything if there's more than one. */
        if(impActions > 1)
        {
            /* Note the location of the first relevant implicit action report */
            local firstImp = gTranscript.reports_.indexWhich(impFunc);
            
            /* Store a copy of this report */
            local rep = gTranscript.reports_[firstImp];
            
            /* Get a list of all the implicit take reports */            
            local impVec = gTranscript.reports_.subset(impFunc);
            
            local impTxt = definiteObjectLister.makeSimpleList(
                impVec.mapAll({x: x.dobj_}).getUnique()
                );
            
            /* 
             *   Change the text of this implicit action report to account 
             *   for all the objects implicitly taken             
             */
            
            local otherIdx = gTranscript.reports_.indexWhich
                ({x: x.ofKind(ImplicitActionAnnouncement)
                 && !x.action_.ofKind(TakeAction) && !x.isFailure });  
            
            if(otherIdx)       
            {
                /* 
                 *   if we're going to show some other implicit reports, it's
                 *   probably best to do so first
                 */
                firstImp = otherIdx + 1;    
                rep.messageText_ = 'taking ' + impTxt;
            }
            else
                rep.messageText_ = '<./p0>\n<.assume>first taking ' +
                impTxt + '<./assume>\n';
            
            /* 
             *   Prevent the implicitAnnouncementGrouper from overwriting the 
             *   text we've just stored.
             */
            rep.messageProp_ = nil;
            
            /*  
             *   Remove all the individual implicit take action 
             *   reports from the transcript.
             */
            gTranscript.reports_ = gTranscript.reports_.subset({x: !impFunc(x) });
            
            /*  
             *   Add back our summary implicit action report at the location 
             *   of the first individual report we removed.
             */
            gTranscript.reports_.insertAt(firstImp, rep);
            
            /*   
             *   Remove all the CommandReports relating to taking things,
             *   since they would otherwise show up now we've removed
             *   most of the implicit action reports.
             */
            gTranscript.reports_ = gTranscript.reports_.subset({
                x: !((x.ofKind(DefaultCommandReport) ||
                     x.ofKind(MainCommandReport))
                     && x.action_.ofKind(TakeAction)) } );
        }
    
       /* 
        *   After all the preliminary work we finally summarize the 
        *   successful default reports relating to the main action into a 
        *   single report. There's a complication with TAKE FROM since a 
        *   TAKE FROM action is eventually turned into a TAKE action, so we 
        *   need to handle this as a special case.
        */
        
        
        gTranscript.summarizeAction(
            { x: ((x.action_ == gAction) || (gActionIs(TakeFrom) &&
                                            x.action_.ofKind (TakeAction))) 
            && !x.isFailure },
            new function (vec)
        {       
            /* 
             *   Construct a string reporting the objects we did take, 
             *   ensuring that each one is counted only once.
             */
                       
            local dobjText = definiteObjectLister.makeSimpleList
                ( vec.applyAll({x: x.dobj_}).getUnique());
            
                        
            /* Then use this to construct a description of the action */                        
            return gAction.getActionDescWith(dobjText);                 

        });           
        
    }            
    
    /* 
     *   The processFailures method is provided as a hook for game authors 
     *   who want to process the failure messages (e.g, to summarize them 
     *   some way) further than this extension does (it just moves them all 
     *   to the end).
     *
     *   The vec parameter contains the vector of failure messages for 
     *   further processing. Note that this method won't be called at all 
     *   unless there are some failure messages in vec to process.
     *
     *   This method also won't be called unless summarizeFailures is true 
     *   (by default, it's nil).
     *
     *   This method should return a vector of CommandReports resulting from 
     *   whatever we wanted to do to the failure reports generated by the 
     *   library. By default we just return the vector that's passed to us.
     *
     *   The caller will automatically append the vector returned by this 
     *   method to the vector of successul reports.
     */
    
    processFailures(vec) { return vec; }
    
    /* 
     *   By default we don't run the processFailures routine at all. This 
     *   avoids pointless work when processFailures doesn't do anything, and 
     *   also allows authors to opt in to any future version of 
     *   processFailures that does do something.
     */
    summarizeFailures = nil   
    
;

/* 
 *   This modification enables us to prevent the implicitAnnouncementGrouper 
 *   from overwriting a customized messageText_
 */

modify CommandAnnouncement
    getMessageText([params])
    {
        if(messageProp_)
            return gLibMessages.(messageProp_)(params...);
        else
            return messageText_;
    }    
;


/* 
 *   Service routines for describing an action given a string (dobjText) 
 *   containing a list of direct objects.
 *
 *   The complication is that the verb getVerbPhrase uses the verb in the
 *   present imperative form, e.g. 'TAKE'. This is wrong if the game is in the
 *   past tense or the actor is third person singular. We therefore need to
 *   correct for that.
 */

modify TAction
    getActionDescWith(dobjText)
    {
        return '{You/he} ' + vCheck(gAction.getVerbPhrase1(true, 
            gAction.verbPhrase, dobjText, nil)) + '.<.p>';    
    }
    
    verbName()
    {
        rexMatch(pat, verbPhrase);
        return rexGroup(1)[3];
    }
    
    vCheck(str)
    {
        local vName = verbName();
        local correctedVName = vCorrect(vName);
        if(vName != correctedVName)
            str = rexReplace('%<'+vName+'%>', str, correctedVName, ReplaceAll);
        
        return str;
    }
    
    /* 
     *   Put the verb into the correct tense and ensure that it agrees with 
     *   its subject. Subclasses may need to override depending on the verb,
     *   to add the appropriate verbEndingXXX property.
     */
    
    vCorrect(str)   {  return gActor.conjugateRegularVerb(str); }
    
    pat = static new RexPattern('(.*)(?=/)')
;

modify TIAction
    getActionDescWith(dobjText)
    {    
        return '{You/he} '+ vCheck(gAction.getVerbPhrase2(true, 
            gAction.verbPhrase, dobjText, nil, gIobj.theName)) + '.<.p>';
    }
;
