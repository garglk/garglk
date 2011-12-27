#charset "us-ascii"
#include <adv3.h>
#include <en_us.h>

ModuleID
{
    name = 'TCommandTopic Library Extension'
    byline = 'by Eric Eve'
    htmlByline = 'by <a href="mailto:eric.eve@hmc.ox.ac.uk">Eric Eve</a>'
    version = '3.2'    
    listingOrder = 70
}

/* 
 *   VERSION HISTORY 
 *
 *   v3.2 21-Jul-09 - adds cmdDir to CommandHelper so that the direction of a
 *   TravelAction can be inspected.
 *
 *   v3.1 01-Mar-08 - Added handling on Actor to trap System Actions issued
 *   as commands to NPCs.
 *
 *   v3.0 08-Jul-07 - Added handling to ActorState to allow optional blocking
 *   of SystemActions and TopicActions directed at the Actor without needing
 *   to provide custom CommandTopics to trap them.
 *   Also enhanced CommandHelper.proper() to deal more intelligently with
 *   possessives.
 *
 *   v2.2 11-Dec-05 - Added handling to optionally give or restore initial 
 *   capitals to proper names used in cmdPhrase.
 *
 *   v2.1 10-Dec-05 - Added handling to CommandHelper to prevent run-time 
 *   error when constructing cmdPhrase in the case of a TopicAction type 
 *   command (e.g. fred, ask me about john).
 *
 *   v2.0 27-Mar-04 - Substantial changes:  
 *.- Move the matchTopic handling into CommandTopic 
 *.- Allow CommandTopic to match the indirect object as well as the direct 
 *   object of commands. 
 *.- Define a new CommandHelper 
 *   mix-in class to make it more convenient to get at the current command 
 *   action and its objects 
 *.- Define two new methods on CommandTopic, 
 *   handleAction(fromActor, action) and actionResponse(fromActor, action) 
 *   for convenience of command handling. 
 *.- Define new AltCommandTopic and 
 *   AltTCommandTopic classes  
 *
 *   v1.1 13-Mar-04 
 *.- Move handleTopic,  actionPhrase and currentAction into 
 *   CommandTopic 
 *.- Add an obeyCommand property to CommandTopic & 
 *   DefaultCommandTopic 
 *.- Add a currentIObj property to TCommandTopic 
 *.- Add the ExecCommandTopic class
 */ 



/* modify DefaultCommandTopic to make two new properties available */

modify DefaultCommandTopic
   handleTopic(fromActor, action)
   {
    getActor.lastCommandTopic_ = self; 
    
    handleAction(fromActor, action);
         
    inherited(fromActor, action);
    
    actionResponse(fromActor, action);
   }
   
   /* This method performs any handling we want before 
    * topicResponse; it's mainly here
    * to be overridden by CommandHelper, but can be used
    * for any purposes authors want.
    */
    
   handleAction(fromActor, action)
   {
      
   }
   
   /* This method can be overridden to provide responses
    * that need to know the issuing actor or the action
    * commanded. It is performed after topicResponse.
    */
    
   actionResponse(fromActor, action)
   {
     /* by default - do nothing */ 
   }
  
  matchScore = 3
  obeyCommand = nil
;

modify CommandTopic
  /* 
   * The direct object, or a list of direct objects, that will be matched
   * by this topic. If this is left at nil, any direct object or none
   * will be matched.
   */
  matchDobj = nil
  
  /* 
   * The indirect object, or a list of indirect objects, that will be matched
   * by this topic. If this is left at nil, any indirect object or none
   * will be matched.
   */
  matchIobj = nil
  
  matchTopic(fromActor, action)
  {
    /* First check whether we match the action of the command */
    if(!inherited(fromActor, action))
      return nil;
        
    /* Then check whether we match the direct object of the command action*/
        
    if(matchDobj != nil && !matchObjs(action.getDobj, matchDobj))    
         return nil;
    
    /* Finally, check whether we match its indirect object */
    
    if(matchIobj != nil && !matchObjs(action.getIobj, matchIobj))
         return nil;
    
    
     /* We've matched everything we should, so return matchScore */
   return matchScore; 
  }
 
  matchObjs(obj, objToMatch)
  {
    if(objToMatch.ofKind(Collection))
    {
      return objToMatch.indexWhich({x: obj.ofKind(x)});          
    }
    else    
    {
      return obj.ofKind(objToMatch);      
    }
  }

  
  handleTopic(fromActor, action)
  {
    /* Tell our actor that this is the commandTopic that has
     * been matched and is responding.
     */  
    getActor.lastCommandTopic_ = self; 
    
    handleAction(fromActor, action);
         
    inherited(fromActor, action);
    
    actionResponse(fromActor, action);
  }  
  
   /* This method performs any handling we want before 
    * topicResponse; it's mainly here
    * to be overridden by CommandHelper, but can be used
    * for any purposes authors want.
    */  
  handleAction(fromActor, action)
   {
      
   }
  
  
  /*
   *  An additional method for providing customised responses
   *  that provides ready access to the fromActor and action parameters
   */ 
    
  actionResponse(fromActor, action)
  {
     /* by default - do nothing */ 
  }
  
   
  /* Set this to true if you want the actor to obey the command just as
   * it was issued */
  obeyCommand = nil 
  
;

/* Mix-in Class for use with CommandTopic and DefaultCommandTopic 
 * Principally designed to cache the currently commanded action
 * and its most commonly-useful properties. This is mainly for
 * convenience, and allows the additional properties to be accessed
 * from topicResponse.
 */
CommandHelper : object
   handleAction(fromActor, action)
    {
        cmdDobj = action.getDobj;
        cmdIobj = action.getIobj;   
        cmdTopic = action.getTopic;
        cmdAction = action;
        cmdDir = action.getDirection;   
        cmdPhrase = proper(action.ofKind(TopicActionBase) ? topicCmdPhrase : 
                           youToMe(action.getInfPhrase));
    }
    cmdDobj = nil
    cmdIobj = nil
    cmdAction = nil
    cmdPhrase = nil  
    cmdTopic = nil
    cmdDir = nil 
    topicCmdPhrase
    {
        /* 
       *   This is a hack to get round the fact that 
       *   TopicTAction.getVerbPhrase() references gTopicText, which in turn 
       *   references gAction (which is the wrong action at this point), so 
       *   we temporarily need gAction to refer to the action commanded, not 
       *   the EventAction which is the real gAction at this point.
       */
    
      local oldAction;
      try
      {
         oldAction = gAction;
         gAction = cmdAction;         
         return youToMe(cmdAction.getInfPhrase);
      }
      finally
      {
         gAction = oldAction;
      }      
   }
   
   /* Takes a string and replaces any occurrence of a word that exists in
    * the list of properNames with the same word with an initial capital.
    */ 
   
   proper(str)
   {
     foreach(local name in nilToList(properNames))
    {
      local name1 = ' ' + name;
      local name2 = ' ' + name.substr(1,1).toUpper + name.substr(2);      
      str = str.findReplace(name1, name2, ReplaceAll);      
    }
    
        /* 
         *   If the current npc is mentioned in the possessive, change this 
         *   to 'your'; also replace the npc's name with 'yourself'.
         */
        
        str = langMessageBuilder.generateMessage(str);
        str = rexReplace('%<' + getActor.theName + '<`|squote>s%>', str, 'your', 
                              ReplaceAll);  
        str = rexReplace('%<' + getActor.theName + '%>', str, 'yourself', 
                         ReplaceAll);
        
    return str;
   }
   
   /* 
    *   Modify this to contain a list of proper names that you want to 
    *   appear with an initial capital if they appear in a cmdPhrase
    */
   properNames = []
;

function youToMe(str)
{
  if(str.ofKind(String))
  {       
   str = str.findReplace(' you ', ' me ', ReplaceAll);
    if(str.endsWith(' you'))
      str = str.findReplace(' you', ' me', ReplaceOnce,
        str.length-5);
  }
  return str.toLower();      
}


/*
 * TCommandTopic is a CommandTopic that can match a particular direct
 * object or set of direct objects as well as a particular action or
 * set of actions. This allows the TCommandTopic's topicResponse()
 * method either to display a message specific to a particular action
 * and direct object, but also, optionally, to define the actions
 * the target actor takes in response.
 *
 * For example, to define a TCommandTopic that matches the command
 * >BOB, TAKE THE GOLD COIN 
 *       or
 * >BOB, TAKE THE DIAMOND
 *
 * and then displays a suitable conversational interchange and
 * had Bob take the gold coin you might define:
 *
 *    + TCommandTopic @TakeAction
 *       matchDobj = [goldCoin, diamond]
 *       topicResponse()
 *       {
 *         "<q>Bob, <<cmdPhrase>>, would you?</q>, you ask.\b
 *          <q>Sure,</q> he agrees readily. ";
 *          nestedActorAction(getActor, Take, currentDobj);
 *       }
 *    ;
 */

class TCommandTopic : CommandHelper, CommandTopic
;

class DefaultTCommandTopic : CommandHelper, DefaultCommandTopic
;


modify Actor  
    /* Keep track of the last CommandTopic triggered on this actor */
    lastCommandTopic_ = nil
    
    /* By default, trap system commands targeted to NPCs */
    obeyCommand(issuingActor, action)
    {
        if(action.ofKind(SystemAction) && obeySystemCommands == nil)
        {   
            systemActionToNPC();
            return nil;
        }
        
        return inherited(issuingActor, action);
    }
    
    systemActionToNPC() { libMessages.systemActionToNPC(); }
    obeySystemCommands = nil
;

/* 
  *  Modification to ActorState.obeyCommand() to make it work with 
  *  TCommandTopic. This modification affects only actions that
  *  have one or more direct objects, and does the following:
  *
  *  1) If the action has more than one direct object, it is 
  *     split into a series of actions each having only one
  *     direct object, taking each direct object in turn.
  *     This allows the game author to assume that any
  *     TCommandTopic will only have one direct object to
  *     deal with at a time, facilitating the handling when
  *     the author may wish to have the target actor make
  *     different responses where the direct objects are different.
  *
  *  2) For each action with a direct object passed to handleConversation
  *     (and then to a CommandTopic, TCommandTopic or DefaultCommandTopic)
  *     the current direct object is marked as such on the action object
  *     passed; this allows CommandTopic, TCommandTopic and DefaultCommandTopic
  *     to use methods such as getDobj and getInfPhrase that assume that
  *     the direct object is defined. 
  */
  
modify ActorState
  obeyCommand(issuingActor, action)
     {
         /* 
          * Reset the lastCommandTopic to nil in case the later
          * handling bypasses setting it, and we're left with
          * the spurious residue of a previous command.
          */
         
         getActor.lastCommandTopic_ = nil;

        
        /* 
         *   If we direct a SystemAction or a TopicAction at the actor, it 
         *   should normally be blocked. Commands like BOB, SAVE or BOB, ASK 
         *   SALLY ABOUT FRED are unlikely to have satisfactory handling, 
         *   and will typically produce suboptimal responses in 
         *   DefaultCommandTopics, so it's easiest to block them globally 
         *   here. We also provide a couple of flags to allow this to be 
         *   easily overridden on a per-state basis if desired.
         */
        
        
        if(autoBlockSystemCommands && action.ofKind(SystemAction))
        {
            explainBlockSystemCommand();
            return nil;
        }
        
        if(autoBlockTopicCommands && action.ofKind(TopicActionBase))
        {
            explainBlockTopicCommand();
            return nil;
        }
         /* 
          *  If the action takes a direct object (and so could have a list
          *  of direct objects), split it into a series of actions
          *  with a single direct object
          */
          
          
          local dobjLst = action.getResolvedDobjList;

          if(dobjLst != nil) 
         {
            local singleAction = action;
 
            local iobjLst = action.getResolvedIobjList;
            if(iobjLst != nil)
               singleAction.iobjCur_ = iobjLst[1];
                
 			foreach(local obj in dobjLst)
           {
              singleAction.dobjCur_ = obj;
              handleConversation(issuingActor, singleAction, commandConvType); 
           }
         }

         /* Otherwise, just treat it as a single command */

         else
           handleConversation(issuingActor, action, commandConvType);
 
         /* 
          * check whether to accept or refuse the command, based on
          *  the decision of the CommandTopic matched
          */
         return getActor.lastCommandTopic_ != nil
             && getActor.lastCommandTopic_.obeyCommand;
     }
    
    autoBlockSystemCommands  = true
    explainBlockSystemCommand() { playerMessages.systemActionToNPC(); }
    
    autoBlockTopicCommands = true
    explainBlockTopicCommand()
    {
        "It d{oes|id}n't make much sense to ask <<getActor.itObj>> to do that. ";
    }
;


/* AltCommandTopic is an AltTopic with the a special handleTopic
 * method to facilitate the handling of commands.
 */

AltCommandTopic : AltTopic
handleTopic(fromActor, action)
  {
    
    getActor.lastCommandTopic_ = self; 
    
    handleAction(fromActor, action);
         
    inherited(fromActor, action);
    
    actionResponse(fromActor, action);
  }  
  
  handleAction(fromActor, action)
   {
      
   }
  
  
  /*
   *  An additional method for providing customised responses
   *  that provides ready access to the fromActor and action parameters
   */ 
    
  actionResponse(fromActor, action)
  {
     /* by default - do nothing */ 
  }
  
   
  /* Set this to true if you want the actor to obey the command just as
   * it was issued */
  obeyCommand = nil 
  
;

AltTCommandTopic : CommandHelper, AltCommandTopic
;



class SuggestedCommandTopic : SuggestedTopic
   suggestionGroup = [suggestionCommandGroup]
   fullName = ('tell {it targetActor/him} to ' + name)
;

suggestionCommandGroup : SuggestionListGroup
     groupPrefix = "tell {it targetActor/him} to "
;

