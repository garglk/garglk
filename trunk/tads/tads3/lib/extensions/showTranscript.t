#charset "us-ascii"

#include <adv3.h>
#include <en_us.h>

/*  
 *   showTranscript
 *
 *   by Eric Eve
 *
 *   version 1.0  
 *
 *   This extension provides a utility routine, showTranscript, to help game 
 *   authors inspect the current contents of the transcript (as an alternative
 *   to examining it in the Workbench debugger). It is only intended for 
 *   help in writing and debugging code, and will not be included in code 
 *   compiled for release.
 *
 *   The typical use of showTranscript() would be to call it from an 
 *   afterActionMain() routine that is meant to be combining reports or 
 *   otherwise manipulating the transcript.
 *
 *   Note that to use this extension your project MUST also include reflect.t 
 */

     
#ifdef __DEBUG     

showTranscript()
    {            
        local wasActive = gTranscript.isActive;
        local len = gTranscript.reports_.length();
        gTranscript.deactivate();
        local lf = new RexPattern('<LineFeed>');
        for(local i=1; i<=len; i++)
        {
            local cur = gTranscript.reports_[i];
            "<b>[<<i>>]:</b> [<<showClassName(cur)>>]: 
            action_ = <<showClassName(cur.action_)>>;
            messageText_ = '<<rexReplace(lf, cur.messageText_, '', ReplaceAll)>>';
            dobj_ = <<cur.dobj_== nil ? 'nil' : cur.dobj_.name>> \n";
            
        }
        
        if(wasActive)
            gTranscript.activate();
    }
    
    
    showClassName(obj)
    {
        if(obj == nil)
            return 'nil';
        return reflectionServices.valToSymbol(obj.getSuperclassList[1]);
    }

#endif