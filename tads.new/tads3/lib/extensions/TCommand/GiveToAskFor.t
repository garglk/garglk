#charset "us-ascii"
#include <adv3.h>
#include <en_us.h>

ModuleID
{
    name = 'GiveTo->AskFor Library Extension'
    byline = 'by Eric Eve'
    htmlByline = 'by <a href="mailto:eric.eve@hmc.ox.ac.uk">Eric Eve</a>'
    version = '2.0'    
    listingOrder = 72
}

/* Converts X, GIVE ME Y into ASK X FOR Y */

StringPreParser
    runOrder = 90
    
    doParsing(str, which)
    {
        local workStr = str.toLower;
        local iGive = workStr.find('give');
        if(iGive == nil)
            return str;
        
        local iComma = workStr.find(',');
        local iGiveMe = workStr.find('give me');        
        local iToMe = workStr.find('to me');
        local objectName; 
        
        if(iComma == nil || (iGiveMe == nil && iToMe == nil))
            return str;
        
        local npcName = workStr.substr(1, iComma-1);        
        
        if(iGiveMe)    
            objectName = workStr.substr(iGiveMe + 8);
        else
        {
            
            if(iGive == nil || iGive > iToMe || iGive < iComma)
                return str;
            
            objectName = workStr.substr(iGive + 5, iToMe - iGive - 6);
        }
        
        str = 'ask ' + npcName + ' for ' + objectName;
        
        str = rexReplace(pat, str, npcName + '\'s', ReplaceAll); 
        
        return str;
    }

    pat = static new RexPattern('%<your%>')
;

    
