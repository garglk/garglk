#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved.
 *   
 *   TADS 3 Library: Actions.
 *   
 *   This module defines the Action classes.  An Action is an abstract
 *   object representing a command to be performed.  
 */

#include "adv3.h"
#include "tok.h"

/* ------------------------------------------------------------------------ */
/*
 *   Object associations lists.  We use this object to store some lookup
 *   tables that we build during preinitialization to relate object usages
 *   (DirectObject, IndirectObject) to certain properties. 
 */
objectRelations: PreinitObject
    /* preinitialization - build the lookup tables */
    execute()
    {
        /* build the default pre-condition properties table */
        preCondDefaultProps[DirectObject] = &preCondDobjDefault;
        preCondDefaultProps[IndirectObject] = &preCondIobjDefault;

        /* build the catch-all pre-conditions properties table */
        preCondAllProps[DirectObject] = &preCondDobjAll;
        preCondAllProps[IndirectObject] = &preCondIobjAll;

        /* build the default verification properties table */
        verifyDefaultProps[DirectObject] = &verifyDobjDefault;
        verifyDefaultProps[IndirectObject] = &verifyIobjDefault;

        /* build the catch-all verification properties table */
        verifyAllProps[DirectObject] = &verifyDobjAll;
        verifyAllProps[IndirectObject] = &verifyIobjAll;

        /* build the default check properties table */
        checkDefaultProps[DirectObject] = &checkDobjDefault;
        checkDefaultProps[IndirectObject] = &checkIobjDefault;

        /* build the catch-all check properties table */
        checkAllProps[DirectObject] = &checkDobjAll;
        checkAllProps[IndirectObject] = &checkIobjAll;

        /* build the default action properties table */
        actionDefaultProps[DirectObject] = &actionDobjDefault;
        actionDefaultProps[IndirectObject] = &actionIobjDefault;

        /* build the catch-all check properties table */
        actionAllProps[DirectObject] = &actionDobjAll;
        actionAllProps[IndirectObject] = &actionIobjAll;
    }

    /* lookup table for default precondition properties */
    preCondDefaultProps = static new LookupTable()

    /* lookup table for catch-all precondition properties */
    preCondAllProps = static new LookupTable()

    /* lookup table for default verification properties */
    verifyDefaultProps = static new LookupTable()

    /* lookup table for catch-all verification properties */
    verifyAllProps = static new LookupTable()

    /* lookup table for default check properties */
    checkDefaultProps = static new LookupTable()

    /* lookup table for catch-all check properties */
    checkAllProps = static new LookupTable()

    /* lookup table for default action properties */
    actionDefaultProps = static new LookupTable()

    /* lookup table for catch-all action properties */
    actionAllProps = static new LookupTable()
;


/* ------------------------------------------------------------------------ */
/*
 *   Invoke the given function with the given values for the parser global
 *   variables gActor and gAction.  
 */
withParserGlobals(issuer, actor, action, func)
{
    local oldIssuer;
    local oldActor;
    local oldAction;
    
    /* remember the old action and actor, so we can restore them later */
    oldIssuer = gIssuingActor;
    oldActor = gActor;
    oldAction = gAction;
    
    /* establish our actor and action as the global settings */
    gIssuingActor = issuer;
    gActor = actor;
    gAction = action;
    
    /* make sure we restore globals on the way out */
    try
    {
        /* invoke the callback and return the result */
        return (func)();
    }
    finally
    {
        /* restore the globals we changed */
        gActor = oldActor;
        gIssuingActor = oldIssuer;
        gAction = oldAction;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition Descriptor.  This object encapsulates a precondition
 *   object and the argument we want to pass to its condition check method.
 */
class PreCondDesc: object
    construct(cond, arg)
    {
        /* remember the condition and the check argument */
        cond_ = cond;
        arg_ = arg;
    }

    /* check the precondition */
    checkPreCondition(allowImplicit)
    {
        /* call the precondition's check method with the argument we stored */
        return cond_.checkPreCondition(arg_, allowImplicit);
    }

    /* the precondition object */
    cond_ = nil

    /* the check argument */
    arg_ = nil

    /* our list sorting index */
    index_ = 0
;


/* ------------------------------------------------------------------------ */
/*
 *   Basic Action class.  An Action is the language-independent definition
 *   of the abstract action of a command.  
 */
class Action: BasicProd
    /*
     *   Are we the given kind of action?  By default, this simply returns
     *   true if we're of the given action class. 
     */
    actionOfKind(cls) { return ofKind(cls); }

    /*
     *   Reset the action in preparation for re-execution.  This should
     *   discard any scoped context from any past execution of the
     *   command, such as cached scope information.  
     */
    resetAction()
    {
        /* forget any past successful verification passes */
        verifiedOkay = [];
    }

    /* 
     *   Repeat the action, for an AGAIN command.
     */
    repeatAction(lastTargetActor, lastTargetActorPhrase,
                 lastIssuingActor, countsAsIssuerTurn)
    {
        /* execute the command */
        executeAction(lastTargetActor, lastTargetActorPhrase,
                      lastIssuingActor, countsAsIssuerTurn, self);
    }

    /*
     *   Cancel iteration of the action.  This can be called during the
     *   'check' or 'action' phases of executing this action.  It tells
     *   the action that we want to stop executing the action when we're
     *   finished with the current object.
     *   
     *   Note that this doesn't cause a jump out of the current code, so
     *   it's not like 'exit' or the other termination signals.  Instead,
     *   this simply tells the action to proceed normally for the
     *   remainder of the processing for the current object, and then act
     *   as though there were no more objects to iterate over, ending the
     *   command normally.  If you want to cut off the remainder of the
     *   execution cycle for the current object, you can use 'exit' (for
     *   example) immediately after calling this method.  
     */
    cancelIteration() { iterationCanceled = true; }

    /* internal flag: object iteration has been canceled */
    iterationCanceled = nil

    /*
     *   Create an instance of this action, for use by a recursive or
     *   programmatically-generated command.
     *   
     *   The generic actions defined in the library are always subclassed
     *   by language-specific library modules, because the language
     *   modules have to define the grammar rules for the verbs - we can't
     *   define the grammar rules generically because the verbs wouldn't
     *   be reusable for non-English translations if we did.  As a result,
     *   library code referring to one of the library verbs by name, say
     *   TakeAction, doesn't get a language-specific subclass of the verb,
     *   but just gets the language-independent base class.
     *   
     *   However, to make full use of an Action object in a recursive
     *   command, we do need a final language-specific subclass - without
     *   this, we won't be able to generate text describing the command,
     *   for example.  This method bridges this gap by finding a suitable
     *   language-specific subclass of the given action, then creating an
     *   instance of that subclass rather than an instance of the base
     *   class.
     *   
     *   By default, we'll take any subclass of this action that is itself
     *   a class.  However, if any subclass has the property
     *   defaultForRecursion set to true, we'll use that class
     *   specifically - this lets the language module designate a
     *   particular subclass to use as the default for recursive commands,
     *   which might be desirable in cases where the language module
     *   defines more than one subclass of an action.  
     */
    createActionInstance()
    {
        local found;

        /* 
         *   Iterate over our subclasses.  Initialize 'found' to this base
         *   class, so that if we fail to find any subclasses, we'll at
         *   least be able to create an instance of the generic base
         *   class.  
         */
        for (local cur = firstObj(self, ObjClasses), found = self ;
             cur != nil ; cur = nextObj(cur, self, ObjClasses))
        {
            /* 
             *   if this one is marked as a default for recursion, and the
             *   last one we found isn't, choose this one over the last
             *   one we found 
             */
            if (cur.defaultForRecursion && !found.defaultForRecursion)
                found = cur;
            
            /* 
             *   If this one is a subclass of the last one we found, pick
             *   it instead of the last one.  We always want a final
             *   subclass here, never an intermediate class, so if we find
             *   a subclass of the one we've tentatively picked, we know
             *   that the tentative selection isn't final after all.
             */
            if (cur.ofKind(found))
                found = cur;
        }

        /* return a new instance of what we found */
        return found.createInstance();
    }

    /*
     *   Create an instance of this action based on another action.  We'll
     *   copy the basic properties of the original action. 
     */
    createActionFrom(orig)
    {
        local action;
        
        /* create a new instance of this action */
        action = createActionInstance();

        /* copy the token list information to the new action */
        action.tokenList = orig.tokenList;
        action.firstTokenIndex = orig.firstTokenIndex;
        action.lastTokenIndex = orig.lastTokenIndex;

        /* the new action is implicit if the original was */
        if (orig.isImplicit)
            action.setImplicit(orig.implicitMsg);

        /* return the new action */
        return action;
    }

    /*
     *   Mark the command as implicit.  'msgProp' is the property (of
     *   gLibMessages) to use to announce the implicit command.  
     */
    setImplicit(msgProp)
    {
        /* mark ourselves as implicit */
        isImplicit = true;

        /* do not show default reports for implicit commands */
        showDefaultReports = nil;

        /* all implicit commands are nested */
        setNested();

        /* 
         *   do not include implicit commands in undo - since an implicit
         *   command is only a subpart of an explicit command, an implicit
         *   command is not undone individually but only as part of the
         *   enclosing explicit command 
         */
        includeInUndo = nil;

        /* remember the implicit command announcement message property */
        implicitMsg = msgProp;
    }

    /*
     *   Mark the command as nested, noting the parent action (which we
     *   take as the global current action). 
     */
    setNested()
    {
        /* remember the parent action */
        parentAction = gAction;
    }

    /* 
     *   Determine if I'm nested within the given action.  Returns true if
     *   the given action is my parent action, or if my parent action is
     *   nested within the given action. 
     */
    isNestedIn(action)
    {
        /* if my parent action is the given action, I'm nested in it */
        if (parentAction == action)
            return true;

        /* 
         *   if I have a parent action, and it's nested in the given
         *   action, then I'm nested in the given action because I'm
         *   nested in anything my parent is nested in 
         */
        if (parentAction != nil && parentAction.isNestedIn(action))
            return true;

        /* we're not nested in the given action */
        return nil;
    }

    /*
     *   Set the "original" action.  An action with an original action is
     *   effectively part of the original action for the purposes of its
     *   reported results.
     *   
     *   An action has an original action if it's a nested or replacement
     *   action for an action.  
     */
    setOriginalAction(action)
    {
        /* remember my original action */
        originalAction = action;
    }

    /*
     *   Get the "original" action.  If I'm a replacement or nested action,
     *   this returns the original main action of which I'm a part, for
     *   reporting pruposes.
     *   
     *   It's important to note that an implied action does NOT count as a
     *   nested or replacement action for the purposes of this method.
     *   That is, if a command A triggers an implied action B, which
     *   triggers a nested action C, and then after that command A itself
     *   triggers a nested action D, then
     *   
     *   A.getOriginalAction -> A
     *.  B.getOriginalAction -> B (B is implied, not nested)
     *.  C.getOriginalAction -> C (C is nested within B)
     *.  D.getOriginalAction -> A (D is nested within A)
     *   
     *   The purpose of the original action is to tell us, mainly for
     *   reporting purposes, what we're really trying to do with the
     *   action.  This allows reports to hide the internal details of how
     *   the action is carried out, and instead say what the action was
     *   meant to do from the player's perspective.  
     */
    getOriginalAction()
    {
        /* if I have no other original action, I'm the original action */
        if (originalAction == nil)
            return self;

        /* return my original action's original action */
        return originalAction.getOriginalAction();
    }

    /*
     *   Determine if this action is "part of" the given action.  I'm part
     *   of the given action if I am the given action, or the given action
     *   is my "original" action, or my original action is part of the
     *   given action.
     */
    isPartOf(action)
    {
        /* if I'm the same as the given action, I'm obviously part of it */
        if (action == self)
            return true;

        /* if my original action is part of the action, I'm part of it */
        if (originalAction != nil && originalAction.isPartOf(action))
            return true;

        /* I'm not part of the given action */
        return nil;
    }

    /*
     *   Mark the action as "remapped."  This indicates that the action
     *   was explicitly remapped to a different action during the remap()
     *   phase.  
     */
    setRemapped(orig) { remappedFrom = gAction; }

    /* determine if I'm remapped, and get the original action if so */
    isRemapped() { return remappedFrom != nil; }
    getRemappedFrom() { return remappedFrom; }

    /*
     *   Get the "simple synonym" remapping for one of our objects, if
     *   any.  'obj' is the resolved object to remap, and 'role' is the
     *   object role identifier (DirectObject, IndirectObject, etc).
     *   'remapProp' is the remapping property for the role; this is
     *   simply the result of our getRemapPropForRole(role), but we ask
     *   the caller to pass this in so that it can be pre-computed in
     *   cases where we'll called in a loop.
     *   
     *   A simple synonym remapping is a remapTo that applies the same
     *   verb to a new object in the same role.  For example, if we remap
     *   OPEN DESK to OPEN DRAWER, then the drawer is the simple synonym
     *   remapping for the desk in an OPEN command.  A remapping is
     *   considered a simple synonym remapping only if we're remapping to
     *   the same action, AND the new object is in the same action role as
     *   the original object was.
     *   
     *   If there's no simple synonym remapping, we'll return nil.  
     */
    getSimpleSynonymRemap(obj, role, remapProp)
    {
        local mapping;
        local remapIdx;
        
        /* if the object isn't remapped at all, there's no remapping */
        if ((mapping = obj.(remapProp)) == nil)
            return nil;

        /* if the mapping isn't to the same action, it's not a synonym */
        if (!ofKind(mapping[1]))
            return nil;


        /* 
         *   Find the specific (non-role) object in the remap vector -
         *   this is the entry that's actually an object rather than a
         *   role identifier.  (The remapping vector is required to have
         *   exactly one such entry.  Look from the right end of the list,
         *   since the first entry is always the new action, which is
         *   itself an object, but not the object we're looking for.)  
         */
        remapIdx = mapping.lastIndexWhich({x: dataType(x) == TypeObject});
        if (remapIdx == nil)
            return nil;

        /* 
         *   Determine if the object plays the same role in the new action
         *   as the original object did in the original action.  It does
         *   if it's at the index in the mapping vector of our object role
         *   in the action.  Note that the mapping vector has slot 1
         *   filled with the action, so its objects are actually one slot
         *   higher than they are in the action itself.
         *   
         *   If the new object isn't in the same role, then this isn't a
         *   simple synonym remapping.  
         */
        if (getRoleFromIndex(remapIdx - 1) != role)
            return nil;

        /* 
         *   We have the same action applied to a new object in the same
         *   role as the original object, so this is a simple synonym
         *   remapping.  Return the new object we're mapping to. 
         */
        return mapping[remapIdx];
    }

    /* 
     *   the defaultForRecursion flag must be explicitly set in subclasses
     *   when desired - by default we'll use any language-specific
     *   subclass of an Action for recursive commands 
     */
    defaultForRecursion = nil

    /* 
     *   Flag: the command is implicit.  An implicit command is one that
     *   is performed as an implied enabling step of another command - for
     *   example, if an actor wants to throw something, the actor must be
     *   holding the object, so will implicitly try to take the object.
     */
    isImplicit = nil

    /*
     *   The parent action.  If the command is performed programmatically
     *   in the course of executing another command, this is set to the
     *   enclosing action.
     *   
     *   Note that while all implicit commands are nested, not all nested
     *   commands are implicit.  A nested command may simply be a
     *   component of another command, or another command may be handled
     *   entirely by running a different command as a nested command.  In
     *   any case, a nested but non-implicit command does not appear to
     *   the player as a separate command; it is simply part of the
     *   original command.  
     */
    parentAction = nil

    /*
     *   The original action we were remapped from.  This is valid when
     *   the action was explicitly remapped during the remap() phase to a
     *   different action. 
     */
    remappedFrom = nil

    /* 
     *   the original action - we are effectively part of the original
     *   action for reporting purposes 
     */
    originalAction = nil

    /* the libMessage property, if any, to announce the implicit command */
    implicitMsg = nil

    /* 
     *   Flag: we are to show default reports for this action.  In most
     *   cases we will do so, but for some types of commands (such as
     *   implicit commands), we suppress default reports. 
     */
    showDefaultReports = true

    /*
     *   Get a message parameter object for the action.  Each action
     *   subclass defines this to return its objects according to its own
     *   classifications.  The default action has no objects, but
     *   recognizes 'actor' as the current command's actor.  
     */
    getMessageParam(objName)
    {
        switch(objName)
        {
        case 'pc':
            /* return the player character */
            return gPlayerChar;
            
        case 'actor':
            /* return the current actor */
            return gActor;

        default:
            /* 
             *   if we have an extra message parameters table, look up the
             *   parameter name in the table 
             */
            if (extraMessageParams != nil)
                return extraMessageParams[objName];

            /* we don't recognize other names */
            return nil;
        }
    }

    /*
     *   Define an extra message-specific parameter.  Message processors
     *   can use this to add their own special parameters, so that they
     *   can refer to parameters that aren't involved directly in the
     *   command.  For example, a message for "take <dobj>" might want to
     *   refer to the object containing the direct object.
     */
    setMessageParam(objName, obj)
    {
        /* 
         *   if we don't yet have an extra message parameters table,
         *   create a small lookup table for it 
         */
        if (extraMessageParams == nil)
            extraMessageParams = new LookupTable(8, 8);

        /* add the parameter to the table, indexing by the parameter name */
        extraMessageParams[objName.toLower()] = obj;
    }

    /*
     *   For convenience, this method allows setting any number of
     *   name/value pairs for message parameters. 
     */
    setMessageParams([lst])
    {
        /* set each pair from the argument list */
        for (local i = 1, local len = lst.length() ; i+1 <= len ; i += 2)
            setMessageParam(lst[i], lst[i+1]);
    }

    /*
     *   Synthesize a global message parameter name for the given object.
     *   We'll store the association and return the synthesized name. 
     */
    synthMessageParam(obj)
    {
        local nm;
        
        /* synthesize a name */
        nm = 'synth' + toString(synthParamID++);

        /* store the association */
        setMessageParam(nm, obj);

        /* return the synthesized name */
        return nm;
    }

    /* synthesized message object parameter serial number */
    synthParamID = 1

    /*
     *   Extra message parameters.  If a message processor wants to add
     *   special message parameters of its own, we'll create a lookup
     *   table for the extra parameters.  Message processors might want to
     *   add their own special parameters to allow referring to objects
     *   other than the main objects of the command.  
     */
    extraMessageParams = nil

    /*
     *   Flag: this command is repeatable with 'again'.  Before executing
     *   a command, we'll save it for use by the 'again' command if this
     *   flag is true.
     */
    isRepeatable = true

    /*
     *   Flag: this command should be included in the undo records.  This
     *   is true for almost every command, but a few special commands
     *   (undo, save) are not subject to undo.  
     */
    includeInUndo = true

    /*
     *   Flag: this is a "conversational" command, as opposed to an order.
     *   When this type of command is addressed to another character, it
     *   doesn't ask the other character to do anything, but rather engages
     *   the other character in conversation.  Examples:
     *   
     *.  Bob, hello
     *.  Bob, goodbye
     *.  Bob, tell me about the planet
     *.  Bob, yes
     *.  Bob, no
     *   
     *   ("Tell me about..." is a little different from the others.  We
     *   treat it as conversational because it means the same thing as "ask
     *   Bob about...", which we consider conversational because it would
     *   be rendered in real life as a question.  In other words, the
     *   action involves the issuing actor stating the question, which
     *   means that issuing actor is the one doing the physical work of the
     *   action.  "Tell me about..." could be seen as an order, but it
     *   seems more appropriate to view it as simply an alternative way of
     *   phrasing a question.)
     *   
     *   The issuing actor is passed as a parameter because some actions
     *   are conversational only in some cases; "tell me about the planet"
     *   is conversational, but "tell Bill about the planet" isn't, since
     *   the latter doesn't ask Bob a question but orders Bob to talk to
     *   Bill.
     *   
     *   When the issuing actor and target actor are the same, this is
     *   irrelevant.  The purpose of this is to distinguish orders given to
     *   another character from conversational overtures directed to the
     *   other character, so if the command is coming from the player and
     *   bound for the player character, there's obviously no conversation
     *   going on.
     *   
     *   Note also that, contrary to what one might think at first glance,
     *   a command like ASK ABOUT is NOT conversational; it's a command to
     *   ask someone about something, and isn't itself a conversational
     *   overture.  The act of asking is itself a conversational overture,
     *   but the asking is the *result* of the command, not the command
     *   itself.  An action is only conversational if the action itself is
     *   a conversational overture.  So, "BOB, HELLO" is conversational;
     *   "BOB, ASK BILL ABOUT COMPUTER" is not, because it orders Bob to do
     *   something.  
     */
    isConversational(issuingActor) { return nil; }

    /*
     *   Get the actual verb phrase the player typed in to generate this
     *   Action, expressed in a canonical format.  The canonical format
     *   consists of the lower-case version of all of the actual text the
     *   player typed, but with each noun phrase replaced by a standard
     *   placeholder token describing the slot.  The placeholder tokens are
     *   '(dobj)' for the direct object, '(iobj)' for the indirect object,
     *   '(literal)' for a literal text phrase, and '(topic)' for a topic
     *   phrase.
     *   
     *   For example, if the player typed PUT BOOK AND PENCIL IN BOX, the
     *   canonical phrasing we return would be "put (dobj) in (iobj)".
     */
    getEnteredVerbPhrase()
    {
        local orig;
        local txt;
        
        /* 
         *   If there's an original action, let the original action do the
         *   work.  If we've been remapped, or if this is an implied
         *   action, we won't necessarily have been constructed from the
         *   actual player input, so we need to go back to the original
         *   action for this information. 
         */
        if (getOriginalAction() != self)
            return getOriginalAction.getEnteredVerbPhrase();

        /* start with the original token list for the predicate */
        orig = getPredicate().getOrigTokenList();

        /* add each token to the result text */
        for (txt = '', local i = 1, local len = orig.length() ;
             i <= len ; ++i)
        {
            local foundNp;
            
            /* add a space if this isn't the first element */
            if (i > 1)
                txt += ' ';

            /*
             *   Check to see if we're entering one of the noun-phrase
             *   slots.  We are if we've reached the first token of one of
             *   the predicate noun phrases. 
             */
            foundNp = nil;
            foreach (local npProp in predicateNounPhrases)
            {
                local match;
                
                /* check to see if we're at this noun phrase */
                if ((match = self.(npProp)) != nil
                    && i == match.firstTokenIndex)
                {
                    /* 
                     *   we're entering this noun phrase - add the generic
                     *   placeholder token for the noun phrase 
                     */
                    txt += (npProp == &dobjMatch ? '(dobj)' :
                            npProp == &iobjMatch ? '(iobj)' :
                            npProp == &topicMatch ? '(topic)' :
                            npProp == &literalMatch ? '(literal)' :
                            '(other)');

                    /* skip the entire run of tokens for the noun phrase */
                    i = match.lastTokenIndex;

                    /* note that we found a noun phrase */
                    foundNp = true;

                    /* stop looking for a noun phrase */
                    break;
                }
            }

            /* 
             *   if we didn't find a noun phrase, this token is a literal
             *   part of the predicate grammar, so add it as-is 
             */
            if (!foundNp)
                txt += getTokVal(orig[i]);
        }

        /* return the phrase, with everything converted to lower-case */
        return txt.toLower();
    }

    /*
     *   Get the grammar match tree object for the predicate that was used
     *   to enter this command.  By default, if we have an original action,
     *   we return the original action; otherwise we just return 'self'.
     *   
     *   Language libraries must override this to return the original match
     *   tree object if Actions are separate from predicate match trees.
     *   
     *   (The 'predicate' is simply the grammar match tree object for the
     *   entire verb phrase from the player's actual command entry text
     *   that matched this Action.  For example, in "BOB, TAKE BOX", the
     *   predicate is the match tree for the "TAKE BOX" part.  In "BOB,
     *   TAKE BOX AND GO NORTH", the predicate for the Take action is still
     *   the "TAKE BOX" part.  For "BOB, TAKE BOX AND BOOK AND GO NORTH",
     *   the predicate for the Take action is "TAKE BOX AND BOOK" - the
     *   full verb phrase includes any multiple-object lists in the
     *   original command.)  
     */
    getPredicate()
    {
        /* 
         *   By default, we just return the original Action - we assume
         *   that the language library defines Action subclasses as the
         *   actual match tree objects for predicate grammars.  Language
         *   modules must override this if they use separate object types
         *   for the predicate match tree objects.  
         */
        return getOriginalAction();
    }

    /*
     *   Get the noun-phrase information for my predicate grammar.  This
     *   returns a list of the match-tree properties for the noun-phrase
     *   sub-productions in our predicate grammar.  The properties
     *   generally include &dobjMatch, &iobjMatch, &literalMatch, and
     *   &topicMatch.  The order of the slots isn't important; they simply
     *   tell us which ones we should find in our predicate grammar match.
     *   
     *   The base Action is intransitive, so it doesn't have any
     *   noun-phrase slots, hence this is an empty list.  
     */
    predicateNounPhrases = []

    /*
     *   Get the object "role" identifier for the given index.  This
     *   returns the identifier (DirectObject, IndirectObject, etc.) for
     *   the object at the given slot index, as used in
     *   setResolvedObjects().  The first object is at index 1.  
     */
    getRoleFromIndex(idx) { return nil; }

    /* 
     *   Get the "other object" role - this is the complement of the given
     *   object role for a two-object action, and is used to determine the
     *   real role of the special OtherObject placeholder in a remapTo().
     *   This is only meaningful with actions of exactly two objects.  
     */
    getOtherObjectRole(role) { return nil; }

    /* get the resolved object in a given role */
    getObjectForRole(role) { return nil; }

    /* get the match tree for the noun phrase in the given role */
    getMatchForRole(role) { return nil; }

    /* get the 'verify' property for a given object role */
    getVerifyPropForRole(role)
    {
        return nil;
    }

    /* get the 'preCond' property for a given object role */
    getPreCondPropForRole(role)
    {
        return nil;
    }

    /* get the 'remap' property for a given object role */
    getRemapPropForRole(role)
    {
        return nil;
    }

    /* 
     *   Get the ResolveInfo for the given object in the current command.
     *   Since we don't have any objects at all, we'll simply return a new
     *   ResolveInfo wrapping the given object.  'cur' is the object we're
     *   looking for, and 'oldRole' is the role the object previously
     *   played in the action.  
     */
    getResolveInfo(obj, oldRole) { return new ResolveInfo(obj, 0, nil); }

    /*
     *   Explicitly set the resolved objects.  This should be overridden
     *   in each subclass for the number of objects specific to the action
     *   (a simple transitive action takes one argument, an action with
     *   both a direct and indirect object takes two arguments, and so
     *   on).  The base action doesn't have any objects at all, so this
     *   takes no arguments.
     *   
     *   This method is used to set up an action to be performed
     *   programmatically, rather than based on parsed input.  Since
     *   there's no parsed input in such cases, the objects are specified
     *   directly by the programmatic caller.  
     */
    setResolvedObjects() { }

    /*
     *   Explicitly set the object match trees.  This sets the pre-resolved
     *   production match trees.  The arguments are given in the order of
     *   their roles in the action, using the same order that
     *   setResolvedObjects() uses.
     *   
     *   The arguments to this routine can either be match tree objects,
     *   which we'll plug into our grammar tree in the respective roles
     *   exactly as given; or they can be ResolveInfo objects giving the
     *   desired resolutions, in which case we'll build the appropriate
     *   kind of PreResolvedProd for each one.  The types can be freely
     *   mixed.  
     */
    setObjectMatches() { }

    /*
     *   Check that the resolved objects are all in scope.  Returns true if
     *   so, nil if not.
     *   
     *   This routine is meant for use only for actions built
     *   programmatically using setResolvedObjects().  In particular, we
     *   assume that we have only one object in each slot.  For normal
     *   parser-built actions, this check isn't necessary, because the
     *   parser only resolves objects that are in scope in the first place.
     */
    resolvedObjectsInScope()
    {
        /* we have no objects at all, so there is nothing out of scope */
        return true;
    }

    /*
     *   Get the list of bindings for an anaphoric pronoun - this is a
     *   pronoun that refers back to an earlier noun phrase in the same
     *   sentence, such as HIMSELF in ASK BOB ABOUT HIMSELF.  These
     *   obviously make no sense for verbs that take one (or zero)
     *   objects, since there's no earlier phrase to refer back to; these
     *   should return nil to indicate that an anaphoric pronoun is simply
     *   nonsensical in such a context.  Actions of two or more objects
     *   should return a list of objects for the preceding noun phrase.
     *   
     *   Actions of two or more objects should set this if possible to the
     *   resolved object list for the previous noun phrase, as a
     *   ResolveInfo list.
     *   
     *   The tricky part is that some actions will resolve noun phrases in
     *   a different order than they appear in the actual command grammar;
     *   similarly, it's also possible that some non-English languages use
     *   cataphoric pronouns (i.e., pronouns that refer to noun phrases
     *   that appear later in the sentence).  To allow for these cases,
     *   return an empty list here if a binding is grammatically possible
     *   but not yet available because of the resolution order, and note
     *   internally that the parser asked us for an anaphoric binding.
     *   Afterwards, the action's resolver method must go back and perform
     *   *another* resolve pass on the noun phrase production that
     *   requested the anaphor binding.
     *   
     *   'typ' is the PronounType specifier for the corresponding ordinary
     *   pronoun.  For 'himself', for example, typ will be PronounHim.  
     */
    getAnaphoricBinding(typ) { return nil; }

    /*
     *   Set a special pronoun override.  This creates a temporary pronoun
     *   definition, which lasts as long as this action (and any nested
     *   actions).  The temporary definition overrides the normal meaning
     *   of the pronoun.
     *   
     *   One example of where this is useful is in global action remapping
     *   cases where the target actor changes in the course of the
     *   remapping.  For example, if we remap BOB, GIVE ME YOUR BOOK to ASK
     *   BOB FOR YOUR BOOK, the YOUR qualifier should still refer to Bob
     *   even though the command is no longer addressing Bob directly.
     *   This routine can be used in this case to override the meaning of
     *   'you' so that it refers to Bob.  
     */
    setPronounOverride(typ, val)
    {
        /* if we don't have an override table yet, create one */
        if (pronounOverride == nil)
            pronounOverride = new LookupTable(5, 10);

        /* add it to the table */
        pronounOverride[typ] = val;
    }

    /* 
     *   Get any special pronoun override in effect for the action, as set
     *   via setPronounOverride().  This looks in our own override table
     *   for a definition; then, if we have no override of our own, we ask
     *   our parent action if we have one, then our original action.  
     */
    getPronounOverride(typ)
    {
        local pro;

        /* check our own table */
        if (pronounOverride != nil
            && (pro = pronounOverride[typ]) != nil)
            return pro;

        /* we don't have anything in our own table; check our parent */
        if (parentAction != nil
            && (pro = parentAction.getPronounOverride(typ)) != nil)
            return pro;

        /* if still nothing, check with the original action */
        if (originalAction != nil
            && originalAction != parentAction
            && (pro = originalAction.getPronounOverride(typ)) != nil)
            return pro;

        /* we didn't find an override */
        return nil;
    }

    /* 
     *   the pronoun override table - this is nil by default, which means
     *   that no overrides have been defined yet; we create a LookupTable
     *   upon adding the first entry to the table 
     */
    pronounOverride = nil

    /* wrap an object with a ResolveInfo */
    makeResolveInfo(val)
    {
        /* 
         *   if it's already a ResolveInfo object, return it as-is;
         *   otherwise, create a new ResolveInfo wrapper for it
         */
        if (dataType(val) == TypeObject && val.ofKind(ResolveInfo))
            return val;
        else
            return new ResolveInfo(val, 0, nil);
    }

    /*
     *   Convert an object or list of objects to a ResolveInfo list 
     */
    makeResolveInfoList(val)
    {
        /* if we have a non-list collection, make it a list */
        if (dataType(val) == TypeObject && val.ofKind(Collection))
            val = val.toList();

        /* if it's nil or an empty list, return an empty list */
        if (val == nil || val == [])
            return [];

        /* see what we have */
        if (dataType(val) == TypeList)
        {
            /* it's a list - make a ResolveInfo for each item */
            return val.mapAll({x: makeResolveInfo(x)});
        }
        else 
        {
            /* it's not a list - return a one-element ResolveInfo list */
            return [makeResolveInfo(val)];
        }
    }

    /* 
     *   If the command is repeatable, save it for use by 'again'.
     */ 
    saveActionForAgain(issuingActor, countsAsIssuerTurn,
                       targetActor, targetActorPhrase)
    {
        /*
         *   Check to see if the command is repeatable.  It's repeatable if
         *   the base action is marked as repeatable, AND it's not nested,
         *   AND it's issued by the player character, AND it's either a PC
         *   command or it counts as an issuer turn.
         *   
         *   Nested commands are never repeatable with 'again', since no
         *   one ever typed them in.
         *   
         *   "Again" is strictly for the player's use, so it's repeatable
         *   only if this is the player's turn, as opposed to a scripted
         *   action by an NPC.  This is the player's turn only if the
         *   command was issued by the player character (which means it
         *   came from the player), and either it's directed to the player
         *   character OR it counts as a turn for the player character.  
         */
        if (isRepeatable
            && parentAction == nil
            && (issuingActor.isPlayerChar()
                && (targetActor.isPlayerChar() || countsAsIssuerTurn)))
            AgainAction.saveForAgain(issuingActor, targetActor,
                                     targetActorPhrase, self);
    }

    /*
     *   Perform this action.  Throughout execution of the action, the
     *   global parser variables that specify the current actor and action
     *   are valid.  
     */
    doAction(issuingActor, targetActor, targetActorPhrase,
             countsAsIssuerTurn)
    {
        local oldActor;
        local oldIssuer;
        local oldAction;
        local oldResults;

        /* 
         *   save the current parser globals, for restoration when we
         *   finish this command - if this command is nested within
         *   another, this will let us ensure that everything is restored
         *   properly when we finish with this command 
         */
        oldActor = gActor;
        oldIssuer = gIssuingActor;
        oldAction = gAction;
        oldResults = gVerifyResults;

        /* 
         *   set the new globals (note that there are no verification
         *   results or command reports objects yet - these are valid only
         *   while we're running the corresponding command phases) 
         */
        gActor = targetActor;
        gIssuingActor = issuingActor;
        gAction = self;
        gVerifyResults = nil;

        /* make sure we restore globals on our way out */
        try
        {
            local pc;
            
            /* if applicable, save the command for AGAIN */
            saveActionForAgain(issuingActor, countsAsIssuerTurn,
                               targetActor, targetActorPhrase);

            /* start a new command visually if this isn't a nested action */
            if (parentAction == nil)
                gTranscript.addCommandSep();

            /* have the player character note initial conditions */
            pc = gPlayerChar;
            pc.noteConditionsBefore();

            /* run the before routine for the entire action */
            beforeActionMain();

            /* run the subclass-specific processing */
            doActionMain();

            /* run the after routine for the entire action */
            afterActionMain();

            /* 
             *   If this is a top-level action, show the command results.
             *   Don't show results for a nested action, since we want to
             *   wait and let the top-level action show the results after
             *   it has the full set of results.  
             */
            if (parentAction == nil)
            {
                /* 
                 *   If the player character didn't change, ask the player
                 *   character to note any condition changes.  If the
                 *   player character did change to a new actor,
                 *   presumably the command will have displayed a specific
                 *   message, since this would be an unusual development
                 *   for which we can generate no generic message.  
                 */
                if (gPlayerChar == pc)
                    pc.noteConditionsAfter();
            }
        }
        finally
        {
            /* restore the parser globals to how we found them */
            gActor = oldActor;
            gIssuingActor = oldIssuer;
            gAction = oldAction;
            gVerifyResults = oldResults;
        }
    }

    /*
     *   Perform processing before running the action.  This is called
     *   just once per action, even if the action will be iterated for a
     *   list of objects. 
     */
    beforeActionMain()
    {
    }

    /*
     *   Perform processing after running the entire action.  This is
     *   called just once per action, even if the action was iterated for
     *   a list of objects. 
     */
    afterActionMain()
    {
        /* call each registered after-action handler */
        if (afterActionMainList != nil)
        {
            foreach (local cur in afterActionMainList)
                cur.afterActionMain();
        }

        /* 
         *   Mark ourselves as busy for the amount of time this action
         *   takes.  Don't count the time taken for implied actions,
         *   though, since these are meant to be zero-time sub-actions
         *   performed as part of the main action and thus don't have a
         *   separate time cost. 
         *   
         *   Note that we add our busy time in the main after-action
         *   processing because we only want to count our time cost once
         *   for the whole command, even if we're performing the command on
         *   multiple objects.  
         */
        if (!isImplicit)
        {
            local actor; 
            
            /* 
             *   If the command is conversational, the turn counts as an
             *   issuer turn; otherwise, it counts as a turn for the target
             *   actor.  Conversational commands are effectively carried
             *   out by the issuer, even though in form they're directed to
             *   another actor (as in "BOB, HELLO"), so we need to count
             *   the time they take as the issuer's time.  
             */
            actor = (isConversational(gIssuingActor)
                     ? gIssuingActor : gActor);

            /* add the busy time to the actor */ 
            actor.addBusyTime(self, actionTime);
        }

        /*
         *   If the command failed, and this is a top-level (not nested)
         *   action, check to see if the game wants to cancel remaining
         *   commands on the line.  
         */
        if (gTranscript.isFailure
            && parentAction == nil
            && gameMain.cancelCmdLineOnFailure)
        {
            /* 
             *   the command failed, and they want to cancel remaining
             *   commands on failure - throw a 'cancel command line'
             *   exception to cancel any remaining tokens 
             */
            throw new CancelCommandLineException();
        }
    }

    /*
     *   Register an object for afterActionMain invocation.  After we've
     *   finished with the entire action - including all iterated objects
     *   involved in the action - we'll invoke each registered object's
     *   afterActionMain() method.  This registration is only meaningful
     *   for the current action instance, and can only be set up before
     *   the action has been finished (i.e., before the current gAction
     *   invokes its own afterActionMain() method).
     *   
     *   Each object is only registered once.  If a caller attempts to
     *   register the same object repeatedly, we'll simply ignore the
     *   repeated requests.
     *   
     *   This is a convenient way to implement a collective follow-up to
     *   the parts of an iterated action.  Since repeated registrations
     *   are ignored, each handler for an iterated object (such as a
     *   "dobjFor" action() handler) can register its follow-up handler
     *   without worrying about redundant registration.  Then, when the
     *   overall action is completed for each iterated object involved,
     *   the follow-up handler will be invoked, and it can do any final
     *   work for the overall action.  For example, the follow-up handler
     *   could display a message summarizing the iterated parts of the
     *   action; or, it could even scan the transcript for particular
     *   messages and replace them with a summary.  
     */
    callAfterActionMain(obj)
    {
        /* if we don't have an after-action list yet, create one */
        if (afterActionMainList == nil)
            afterActionMainList = new Vector(8);

        /* if this item isn't already in the list, add it */
        if (afterActionMainList.indexOf(obj) == nil)
            afterActionMainList.append(obj);
    }

    /* list of methods to invoke after we've completed the action */
    afterActionMainList = nil

    /* 
     *   the amount of time on the game clock that the action consumes -
     *   by default, each action consumes one unit, but actions can
     *   override this to consume more or less game time 
     */
    actionTime = 1

    /*
     *   Zero the action time in this action and any parent actions.  This
     *   should be used when a nested replacement action is to completely
     *   take over the time-keeping responsibility for the entire turn; all
     *   containing actions in this case are to take zero time, since the
     *   nested action is the only part of the turn that will count for
     *   timing.  
     */
    zeroActionTime()
    {
        /* clear my action time */
        actionTime = 0;

        /* if we have a parent action, zero it and its parents */
        if (parentAction != nil)
            parentAction.zeroActionTime();
    }

    /*
     *   Execute the action for a single set of objects.  This runs the
     *   full execution sequence for the current set of objects.
     *   
     *   Subclasses generally won't override this method, but will instead
     *   override the methods that implement the individual steps in the
     *   execution sequence.
     */
    doActionOnce()
    {
        /*
         *   Perform the sequence of operations to execute the action.  If
         *   an ExitSignal is thrown during the sequence, skip to the
         *   end-of-turn processing.  
         */
        try
        {
            local result;
            local impReport;

            /*
             *   Before doing any actual execution, check the command for
             *   remapping.  If we end up doing any remapping, the
             *   remapping routine will simply replace the current command,
             *   so we the remapping call will terminate the current action
             *   with 'exit' and thus never return here.  
             */
            checkRemapping();

            /*
             *   If this is an implicit action, check for danger: we never
             *   try a command implicitly when the command is obviously
             *   dangerous.
             */
            if (isImplicit)
            {
                /* 
                 *   verify the action for an implicit command, checking
                 *   for actions that aren't allowe implicitly - we never
                 *   try a command implicitly when the command is (or
                 *   should be) obviously dangerous or is simply
                 *   non-obvious 
                 */
                result = verifyAction();

                /* 
                 *   If the action can be performed, but can't be performed
                 *   implicitly, abort.  Note that we only silently ignore
                 *   the action if it is allowed normally but not
                 *   implicitly: if it's not even allowed normally, we'll
                 *   simply fail later with the normal failure message,
                 *   since there's no harm in trying.  
                 */
                if (result != nil
                    && result.allowAction
                    && !result.allowImplicit)
                    abortImplicit;
            }

            /*
             *   If this is an implicit command, display a message
             *   indicating that we're performing the command.
             */
            impReport = maybeAnnounceImplicit();

            /*
             *   Make one or two passes through verifications and
             *   preconditions.  If any precondition performs an implicit
             *   command, we must run everything a second time to ensure
             *   that the implicit command or commands did not invalidate
             *   any earlier precondition or a verification.
             *   
             *   Run verifications before preconditions, because there
             *   would be no point in applying implicit commands from
             *   preconditions if the command verifies as illogical in the
             *   first place.  
             */
            for (local iter = 1 ; iter <= 2 ; ++iter)
            {
                /* verify the action */
                result = verifyAction();
                
                /* 
                 *   if verification doesn't allow the command to proceed,
                 *   show the reason and end the command 
                 */
                if (result != nil && !result.allowAction)
                {
                    /* show the result message */
                    result.showMessage();
                    
                    /* mark the command as a failure */
                    gTranscript.noteFailure();

                    /* 
                     *   if we have an implicit report, mark it as a mere
                     *   attempt, since the action can't be completed 
                     */
                    if (impReport != nil)
                        impReport.noteJustTrying();
                    
                    /* terminate the command */
                    exit;
                }

                /* 
                 *   Check preconditions of the action.  If we don't invoke
                 *   any implicit commands, we can stop here: nothing in
                 *   the game state will have changed, so there is no need
                 *   to re-verify or re-check preconditions.
                 *   
                 *   Only allow implicit actions on the first pass.  Do not
                 *   perform implicit actions on the second pass, because
                 *   if we did so we could get into an infinite loop of
                 *   conflicting preconditions, where each pass would
                 *   reverse the state from the last pass.  
                 */
                if (!checkPreConditions(iter == 1))
                    break;
            }

            /* 
             *   Disable sense caching once we start the action phase -
             *   once we start making changes to game state, it's too much
             *   work to figure out when to invalidate the cache, so simply
             *   turn off caching entirely.
             *   
             *   Note that the sense cache will already be disabled if we
             *   executed any implied commands, because the first implied
             *   command will have disabled the cache as soon as it reached
             *   its execution phase, and no one will have turned caching
             *   back on.  It does no harm to disable it again here.  
             */
            libGlobal.disableSenseCache();

            /* if desired, run the "before" notifications before "check" */
            if (gameMain.beforeRunsBeforeCheck)
                runBeforeNotifiers();
                
            /* 
             *   Invoke the action's execution method.  Catch any "exit
             *   action" exceptions - these indicate that the action is
             *   finished but that the rest of the command processing is to
             *   proceed as normal.  
             */
            try
            {
                /* notify the actor of what we're about to do */
                gActor.actorAction();

                /* check the action */
                checkAction();

                /* if desired, run the "before" notifications after "check" */
                if (!gameMain.beforeRunsBeforeCheck)
                    runBeforeNotifiers();
                
                /* execute the action */
                execAction();
            }
            catch (ExitActionSignal eaSig)
            {
                /* 
                 *   an exit action signal was thrown - since we've now
                 *   skipped past any remaining action processing, simply
                 *   continue with the rest of the command processing as
                 *   normal 
                 */
            }
            
            /* call afterAction for each object in the notify list */
            notifyBeforeAfter(&afterAction);
            
            /* notify the actor's containers of the completed action */
            gActor.forEachContainer(callRoomAfterAction);
            
            /* run the after-action processing */
            afterAction();
        }
        catch (ExitSignal exc)
        {
            /* the execution sequence is finished - simply stop here */
        }
    }

    /*
     *   Run the "before" notifiers: this calls beforeAction on everything
     *   in scope, and calls roomBeforeAction on the actor's containers.  
     */
    runBeforeNotifiers()
    {
        /* run the before-action processing */
        beforeAction();

        /* 
         *   notify the actor's containers that an action is about
         *   to take place within them 
         */
        gActor.forEachContainer(callRoomBeforeAction);

        /* call beforeAction for each object in the notify list */
        notifyBeforeAfter(&beforeAction);
    }

    /*
     *   Reset the message generation context for a sense change.  This
     *   can be called when something substantial happens in the midst of
     *   a command, and we might need different message generation rules
     *   before and after the change.  For example, this is used when a
     *   non-player character moves from one location to another, because
     *   the NPC might want to generate leaving and arriving messages
     *   differently in the two locations.
     */
    recalcSenseContext()
    {
        /* tell the sense context capturer to recalculate the context */
        senseContext.recalcSenseContext();
    }

    /*
     *   Maybe announce the action as an implied action. 
     */
    maybeAnnounceImplicit()
    {
        /* 
         *   if we're a remapped action, we'll actually want to announce
         *   the original, not the remapping 
         */
        if (remappedFrom != nil)
            return remappedFrom.maybeAnnounceImplicit();
            
        /* 
         *   if we're implicit, and we have an implicit announcement
         *   message, announce the implicit action 
         */
        if (isImplicit && implicitMsg != nil)
            return gTranscript.announceImplicit(self, implicitMsg);

        /* we don't need to announce the implied action */
        return nil;
    }

    /*
     *   Announce the object of an action.  This should be used for each
     *   iteration of a command that takes objects to announce the objects
     *   on this iteration.
     *   
     *   We announce an object under several circumstances:
     *   
     *   - If we are iterating through multiple objects, we'll show the
     *   current object to show the player the individual step in the
     *   command being performed.
     *   
     *   - If 'all' was used to specify the object, we'll announce it even
     *   if only one object is involved, to make it clear to the player
     *   exactly what we chose as a match.
     *   
     *   - If we are executing the command on a single object, and the
     *   object was chosen through disambiguation of a set of ambiguous
     *   choices, and some of the discarded possibilities were logical but
     *   less so than the chosen object, we'll show the assumption we
     *   made.  In such cases, our assumption is not necessarily correct,
     *   so we'll tell the user about our choice explicitly by way of
     *   confirmation - this gives the user a better chance of noticing
     *   quickly if our assumption was incorrect.
     *   
     *   - If we supplied a default for a missing noun phrase in the
     *   player's command, we'll show what we chose.  Since the player
     *   didn't say what they meant, we'll make it plain that we're
     *   providing an assumption about what we thought they must have
     *   meant.
     *   
     *   'info' is the ResolveInfo object describing this resolved object,
     *   and 'numberInList' is the total number of objects we're iterating
     *   over for this object function (direct object, indirect object,
     *   etc).  'whichObj' is one of the object function constants
     *   (DirectObject, IndirectObject, etc) describing which object we're
     *   mentioning; the language-specific message generator might use
     *   this in conjunction with the action to include a preposition with
     *   the displayed phrase, for example, or choose an appropriate
     *   inflection.
     */
    announceActionObject(info, numberInList, whichObj)
    {
        /* 
         *   Show prefix announcements only if the player character is
         *   performing the action.  For NPC's, we don't use the prefix
         *   format, because it doesn't work as well for NPC result
         *   reports; instead, the NPC versions of the library messages
         *   tend to use sufficiently detailed reports that the prefix
         *   isn't required (for example, "Bob takes the iron key" rather
         *   than just "Taken").  
         */
        if (gActor.isPlayerChar)
        {
            /* check for the various announcements */
            if (maybeAnnounceMultiObject(info, numberInList, whichObj))
            {
                /* we've done a multi announcement, so we're now done */
            }
            else if ((info.flags_ & UnclearDisambig) != 0
                     || ((info.flags_ & ClearDisambig) != 0
                         && gameMain.ambigAnnounceMode == AnnounceClear))
            {
                /* show the object, since we're not certain it's right */
                gTranscript.announceAmbigActionObject(info.obj_, whichObj);
            }
            else if ((info.flags_ & DefaultObject) != 0
                     && !(info.flags_ & AnnouncedDefaultObject))
            {
                /*   
                 *   Show the object, since we supplied it as a default.
                 *   At this stage in the command, we have resolved
                 *   everything.  
                 */
                gTranscript.announceDefaultObject(
                    info.obj_, whichObj, self, true);

                /* note that we've announced this object */
                info.flags_ |= AnnouncedDefaultObject;
            }
        }
    }

    /* announce a multi-action object, if appropriate */
    maybeAnnounceMultiObject(info, numberInList, whichObj)
    {
        /* 
         *   announce if we have more than one object, or we're set to
         *   announce this object in any case 
         */
        if (numberInList > 1 || (info.flags_ & AlwaysAnnounce) != 0)
        {
            /* show the current object of a multi-object action */
            gTranscript.announceMultiActionObject(
                info.multiAnnounce, info.obj_, whichObj);

            /* tell the caller we made an announcement */
            return true;
        }

        /* tell the caller we didn't make an announcement */
        return nil;
    }
    
    /*   
     *   Pre-calculate the multi-object announcement text for each object.
     *   This is important because these announcements might choose a form
     *   for the name that distinguishes it from the other objects in the
     *   iteration, and the basis for distinction might be state-dependent
     *   (such as the object's current owner or location), and the relevant
     *   state might change as we iterate over the objects.  From the
     *   user's perspective, they're referring to the objects based on the
     *   state at the start of the command, so the user will expect to see
     *   names based on the that state.  
     */
    cacheMultiObjectAnnouncements(lst, whichObj)
    {
        /* run through the list and cache each object's announcement */
        foreach (local cur in lst)
        {
            /* calculate and cache this object's multi-object announcement */
            cur.multiAnnounce = libMessages.announceMultiActionObject(
                cur.obj_, whichObj, self);
        }
    }

    /* get the list of resolved objects in the given role */
    getResolvedObjList(which)
    {
        /* 
         *   the base action doesn't have any objects in any roles, so just
         *   return nil; subclasses need to override this 
         */
        return nil;
    }

    /*
     *   Announce a defaulted object list, if appropriate.  We'll announce
     *   the object if we have a single object in the given resolution
     *   list, it was defaulted, and it hasn't yet been announced.
     */
    maybeAnnounceDefaultObject(lst, which, allResolved)
    {
        /* 
         *   if the list has exactly one element, and it's marked as a
         *   defaulted object, and it hasn't yet been announced, announce
         *   it 
         */
        if (lst != nil
            && lst.length() == 1
            && (lst[1].flags_ & DefaultObject) != 0
            && !(lst[1].flags_ & AnnouncedDefaultObject))
        {
            /* announce the object */
            gTranscript.announceDefaultObject(
                lst[1].obj_, which, self, allResolved);

            /* 
             *   we've now announced the object; mark it as announced so
             *   we don't show the same announcement again 
             */
            lst[1].flags_ |= AnnouncedDefaultObject;
        }
    }

    /*
     *   "Pre-announce" a common object for a command that might involve
     *   iteration over other objects.  For example, in "put all in box",
     *   the box is common to all iterations of the command, so we would
     *   want to preannounce it, if it needs to be announced at all,
     *   before the iterations of the command.
     *   
     *   We'll announce the object only if it's marked as defaulted or
     *   unclearly disambiguated, and then only if the other list will be
     *   announcing its objects as multi-action objects.  However, we do
     *   not pre-announce anything for a remapped action, because we'll
     *   show the full action description for each individually announced
     *   object, so we don't need or want a separate announcement for the
     *   group.
     *   
     *   Returns true if we did any pre-announcing, nil otherwise.  If we
     *   return true, the caller should not re-announce this object during
     *   the iteration, since our pre-announcement is common to all
     *   iterations.  
     */
    preAnnounceActionObject(info, mainList, whichObj)
    {
        /* do not pre-announce anything for a remapped action */
        if (isRemapped())
            return nil;

        /* 
         *   determine if the main list will be announcing its objects -
         *   it will if it has more than one object, or if its one object
         *   is marked as "always announce" 
         */
        if (mainList.length() > 1
            || (mainList[1].flags_ & AlwaysAnnounce) != 0)
        {
            /* 
             *   we will be announcing the main list object or objects, so
             *   definitely pre-announce this object if appropriate 
             */
            announceActionObject(info, 1, whichObj);

            /* tell the caller we pre-announced the object */
            return true;
        }

        /* we didn't pre-announce the object */
        return nil;
    }

    /*
     *   Run our action-specific pre-processing.  By default, we do
     *   nothing here.  
     */
    beforeAction()
    {
    }

    /*
     *   Check the action.  This runs the 'check' phase, and must be
     *   overridden for each subclass.
     *   
     *   Intransitive actions don't generally have to do anything in the
     *   'check' phase, since they can simply do any necessary checks in
     *   the 'execute' phase that runs immediately after 'check'.  This
     *   phase is separated out from 'execute' mostly for the benefit of
     *   transitive actions, where the 'check' phase gives the involved
     *   objects a chance to object to the action.  
     */
    checkAction() { }

    /*
     *   Execute the action.  This must be overridden by each subclass.
     *   
     *   Intransitive actions must do all of their work in this routine.
     *   In most cases, transitive actions will delegate processing to one
     *   or more of the objects involved in the command - for example,
     *   most single-object commands will call a method in the direct
     *   object to carry out the command.  
     */
    execAction()
    {
        /* by default, just show the 'no can do' message */
        mainReport(&cannotDoThatMsg);
    }

    /*
     *   Run our action-specific post-processing.  By default, we do
     *   nothing here. 
     */
    afterAction()
    {
    }

    /*
     *   Verify the action.  Action subclasses with one or more objects
     *   should call object verification routines here.  Returns a
     *   VerifyResultList with the results, or nil if there are no
     *   verification results at all.  A nil return should be taken as
     *   success, not failure, because it means that we found no objection
     *   to the command.  
     */
    verifyAction()
    {
        /* 
         *   there are no objects in the default action, but we might have
         *   pre-condition verifiers 
         */
        return callVerifyPreCond(nil);
    }

    /*
     *   Initialize tentative resolutions for other noun phrases besides
     *   the one indicated. 
     */
    initTentative(issuingActor, targetActor, whichObj)
    {
        /* by default, we have no noun phrases to tentatively resolve */
    }

    /*
     *   Check for remapping the action.  This should check with each
     *   resolved object involved in the command to see if the object wants
     *   to remap the action to a new action; if it does, the object must
     *   replace the current action (using replaceAction or equivalent).
     *   Note that replacing the action must use 'exit' to terminate the
     *   original action, so this will never return if remapping actually
     *   does occur.  
     */
    checkRemapping()
    {
        /* by default, we have no objects, so we do nothing here */
    }

    /*
     *   Invoke a callback with a verify results list in gVerifyResults,
     *   using the existing results list or creating a new one if there is
     *   no existing one.  Returns the results list used. 
     */
    withVerifyResults(resultsSoFar, obj, func)
    {
        local oldResults;

        /* if we don't already have a result list, create one */
        if (resultsSoFar == nil)
            resultsSoFar = new VerifyResultList();

        /* remember the old global results list */
        oldResults = gVerifyResults;

        /* install the new one */
        gVerifyResults = resultsSoFar;

        /* make sure we restore the old result list on the way out */
        try
        {
            /* invoke the callback */
            (func)();
        }
        finally
        {
            /* restore the old result list */
            gVerifyResults = oldResults;
        }

        /* return the result list */
        return resultsSoFar;
    }

    /*
     *   Verify the non-object-related pre-conditions.  This runs
     *   verification on each of the pre-condition objects defined for the
     *   action.  
     */
    callVerifyPreCond(resultSoFar)
    {
        /* look at each of our preconditions */
        foreach (local cond in preCond)
        {
            /* 
             *   If this precondition defines a verifier, call it.  Check
             *   to see if we have a verifier defined first so that we can
             *   avoid creating a result list if we won't have any use for
             *   it. 
             */
            if (cond.propDefined(&verifyPreCondition))
            {
                /* 
                 *   invoke the pre-condition verifier - this is an
                 *   action-level verifier, so there's no object involved 
                 */
                resultSoFar = withVerifyResults(resultSoFar, nil,
                    {: cond.verifyPreCondition(nil) });
            }
        }

        /* return the latest result list */
        return resultSoFar;
    }

    /*
     *   Call a catch-all property on the given object.
     *   
     *   actionProp is the custom per-object/per-action property that we
     *   normally invoke to process the action.  For example, if we're
     *   processing verification for the direct object of Take, this would
     *   be &verifyDobjTake.
     *   
     *   defProp is the default property that corresponds to actionProp.
     *   This is the per-object/default-action property that we invoke
     *   when the object doesn't provide a "more specialized" version of
     *   actionProp - that is, if the object doesn't define or inherit
     *   actionProp at a point in its class hierarchy that is more
     *   specialized than the point at which it defines defProp, we'll
     *   call defProp.  If there is a more specialized definition of
     *   actionProp for the object, it effectively overrides the default
     *   handler, so we do not invoke the default handler.
     *   
     *   allProp is the catch-all property corresponding to actionProp.
     *   We invoke this property in all cases.
     *   
     *   Returns true if there is indeed a Default property that overrides
     *   the action, nil if not.  
     */
    callCatchAllProp(obj, actionProp, defProp, allProp)
    {
        /* always invoke the catch-all property first */
        obj.(allProp)();

        /* 
         *   invoke the default property only if the object doesn't have a
         *   "more specialized" version of actionProp 
         */
        if (!obj.propHidesProp(actionProp, defProp))
        {
            /* the Default overrides the action-specific method */
            obj.(defProp)();

            /* tell the caller the Default routine handled it */
            return true;
        }
        else
        {
            /* tell the caller to call the action-specific property */
            return nil;
        }
    }

    /*
     *   Call a verification routine.  This creates a results object and
     *   makes it active, then invokes the given verification routine on
     *   the given object.
     *   
     *   We call verification directly on the object, and we also call
     *   verification on the object's preconditions.
     *   
     *   If resultSoFar is non-nil, it is a VerifyResultList that has the
     *   results so far - this can be used for multi-object verifications
     *   to gather all of the verification results for all of the objects
     *   into a single result list.  If resultSoFar is nil, we'll create a
     *   new result list.  
     */
    callVerifyProp(obj, verProp, preCondProp, remapProp,
                   resultSoFar, whichObj)
    {
        local remapInfo;

        /* check for remapping */
        if ((remapInfo = obj.(remapProp)()) != nil)
        {
            /* call the remapped verify */
            resultSoFar = remapVerify(whichObj, resultSoFar, remapInfo);

            /*
             *   If the original object has a verify that effectively
             *   "overrides" the remap - i.e., defined by an object that
             *   inherits from the object where the remap is defined - then
             *   run it by the overriding verify as well. 
             */
            local remapSrc = obj.propDefined(remapProp, PropDefGetClass);
            if (obj.propDefined(verProp)
                && overrides(obj, remapSrc, verProp))
            {
                resultSoFar = withVerifyResults(
                    resultSoFar, obj, function() { obj.(verProp)(); });
            }

            /* return the results */
            return resultSoFar;
        }

        /* initialize tentative resolutions for other noun phrases */
        initTentative(gIssuingActor, gActor, whichObj);

        /* 
         *   run the verifiers in the presence of a results list, and
         *   return the result list 
         */
        return withVerifyResults(resultSoFar, obj, function()
        {
            local lst;
            
            /* 
             *   check the object for a default or catch-all verifier, and
             *   call it if present; if there isn't a default that
             *   overrides the action-specific verifier, continue to the
             *   action-specific verifer 
             */
            if (!callCatchAllProp(
                obj, verProp, objectRelations.verifyDefaultProps[whichObj],
                objectRelations.verifyAllProps[whichObj]))
            {
                /* 
                 *   invoke the action-specific verifier method - this
                 *   will update the global verification results object
                 *   with the appropriate status for this action being
                 *   performed on this object 
                 */
                obj.(verProp)();
            }

            /*
             *   Check the pre-conditions defined for this action on this
             *   object.  For each one that has a verifier, invoke the
             *   verifier.  
             */
            lst = getObjPreConditions(obj, preCondProp, whichObj);
            if (lst != nil)
                foreach (local cur in lst)
                    cur.verifyPreCondition(obj);
        });
    }

    /*
     *   Get the precondition list for an object.  whichObj is the object
     *   role of the object whose preconditions we're retrieving; this is
     *   nil if we're looking for action-level preconditions.  
     */
    getObjPreConditions(obj, preCondProp, whichObj)
    {
        local allPreProp;
        local defPreProp;
        local pre;

        /* 
         *   if we're looking for action preconditions, there are no
         *   default or catch-all properties, so simply get the
         *   preconditions from the action
         */
        if (whichObj == nil)
            return obj.(preCondProp);

        /* get the default-action and catch-all precondition properties */
        defPreProp = objectRelations.preCondDefaultProps[whichObj];
        allPreProp = objectRelations.preCondAllProps[whichObj];
        
        /*
         *   Check for an "overriding" default-action handler.  If we have
         *   a default-action handler that hides the specific handler for
         *   this action, use the default handler's precondition list
         *   instead.  Otherwise, use the specific action preconditions.  
         */
        if (obj.propHidesProp(defPreProp, preCondProp))
            pre = obj.(defPreProp);
        else
            pre = obj.(preCondProp);

        /* if we have catch-all preconditions, add them to the list */
        if (obj.propDefined(allPreProp))
        {
            /* get the catch-all preconditions */
            local allPre = obj.(allPreProp);

            /* add them to the list so far (if any) */
            pre = (pre == nil ? allPre : pre + allPre);
        }

        /* return the precondition list */
        return pre;
    }

    /*
     *   Verify that some sort of handling for this action is defined on
     *   at least one of the given objects.  If we have no handlers at all
     *   definfed, we'll add an "illogical" status to the verification
     *   results to indicate that the action is not defined on this
     *   object.  This check provides a last resort for verbs with no
     *   handling at all defined on the involved objects, to ensure that
     *   the verb won't go completely unprocessed.
     *   
     *   Each entry in objList is an object involved in the action.  For
     *   each entry in objList, there must be *THREE* entries in propList:
     *   a verify property, a check property, and an action property.  If
     *   any of these three properties is defined on the corresponding
     *   object, we'll allow the command to proceed.  If we can find none
     *   of the given handler properties on any of our objects, we'll add
     *   an "illogical" verify result.  
     */
    verifyHandlersExist(objList, propList, result)
    {
        /* 
         *   check each object to see if any define their corresponding
         *   action property 
         */
        for (local i = 1, local j = 1, local len = objList.length() ;
             i <= len ; ++i, j += 3)
        {
            /* check each of the associated handler properties */
            for (local k = 0 ; k < 3 ; ++k)
            {
                /* check this handler property */
                if (objList[i].propDefined(propList[j + k]))
                {
                    /* we've found a handler, so we can proceed */
                    return result;
                }
            }
        }
        
        /* 
         *   None of the objects defines an appropriate action method, so
         *   this verifies as illogical.  If there's no result list so
         *   far, create one.  
         */
        if (result == nil)
            result = new VerifyResultList();

        /* add an "illogical" status to the results */
        result.addResult(new IllogicalVerifyResult(&cannotDoThatMsg));

        /* return the result */
        return result;
    }
    
    /*
     *   Call the beforeAction or afterAction method for each object in
     *   the notification list. 
     */
    notifyBeforeAfter(prop)
    {
        local lst;

        /* get a table of potential notification receivers */
        lst = getNotifyTable();

        /* go through the table and notify each object */
        lst.forEachAssoc({obj, val: obj.(prop)()});
    }

    /*
     *   Get the list of objects to notify before or after the action has
     *   been performed.  
     */
    getNotifyTable()
    {
        local tab;
        local curObjs;
        local actor;

        /* stash the current actor in a local for faster reference */
        actor = gActor;
        
        /* start with everything connected by containment to the actor */
        tab = actor.connectionTable();

        /* add the actor's explicitly registered notification list */
        foreach (local cur in actor.getActorNotifyList())
            tab[cur] = true;

        /* add the items explicitly registered in the actor's location(s) */
        actor.forEachContainer(
            {loc: loc.getRoomNotifyList().forEach(
                {obj: tab[obj] = true})
            });

        /* get the list of objects directly involved in the command */
        curObjs = getCurrentObjects();

        /* 
         *   add any objects explicitly registered with the objects
         *   directly involved in the command 
         */
        foreach (local cur in curObjs)
        {
            /* add each item in the object's notify list */
            foreach (local ncur in cur.getObjectNotifyList())
                tab[ncur] = true;
        }

        /*
         *   Remove from the list all of the actor's containers.  These
         *   will be notified via the more specific room notification
         *   methods, so we don't want to send them generic notifiers as
         *   well. 
         */
        tab.forEachAssoc(function(obj, val)
        {
            if (actor.isIn(obj))
                tab.removeElement(obj);
        });

        /* if we have any explicitly registered objects, add them */
        if (beforeAfterObjs != nil)
        {
            /* add each object in the list of explicitly registered objects */
            foreach (local cur in beforeAfterObjs)
                tab[cur] = true;
        }

        /* return the final table */
        return tab;
    }

    /*
     *   Register an object for explicit inclusion in beforeAction and
     *   afterAction notifications.  This can be used to register objects
     *   that might not be connected by containment or otherwise
     *   notifiable by normal means.  If this is called after the
     *   beforeAction notification loop has already started, then the
     *   object will only be sent an afterAction notification.  
     */
    addBeforeAfterObj(obj)
    {
        /* if we don't yet have a before/after list, create one */
        if (beforeAfterObjs == nil)
            beforeAfterObjs = new Vector(16);

        /* add the object to the list */
        beforeAfterObjs.append(obj);
    }

    /* vector of objects requiring explicit before/after notification */
    beforeAfterObjs = nil

    /*
     *   Get the list of all of the objects (direct object, indirect
     *   object, and any additional objects for actions with three or more
     *   object roles) involved in the current execution.  This is valid
     *   only during a call to doActionOnce(), since that's the only time
     *   a particular set of objects are selected for the action.
     *   
     *   By default, an action has no objects roles at all, so we'll just
     *   return an empty list.  
     */
    getCurrentObjects()
    {
        return [];
    }

    /*
     *   Set the current objects.  This takes a list of the same form
     *   returned by getCurrentObjects(). 
     */
    setCurrentObjects(lst) { }

    /*
     *   Check any pre-conditions for the action.
     *   
     *   This should check all of the conditions that must be met for the
     *   action to proceed.  If any pre-condition can be met by running an
     *   implicit command first, that implicit command should be executed
     *   here.  If any pre-condition cannot be met, this routine should
     *   notify the actor and throw an ExitSignal.
     *   
     *   Returns true if any implicit commands are executed, nil if not.
     *   Implicit commands can only be attempted if allowImplicit is true;
     *   if this is nil, a precondition must simply fail (by displaying an
     *   appropriate failure report and using 'exit') without attempting
     *   an implicit command if its assertion does not hold.  
     */
    checkPreConditions(allowImplicit)
    {
        /* check each condition in our action preconditions */
        return callPreConditions(getPreCondDescList(), allowImplicit);
    }

    /*
     *   Get the precondition descriptor list for the action.  For the base
     *   intransitive action type, this simply returns the list of
     *   conditions for the action itself.  
     */
    getPreCondDescList()
    {
        /* get the action-level preconditions */
        return getObjPreCondDescList(self, &preCond, nil, nil);
    }

    /*
     *   Get a precondition descriptor list for the given object.  This
     *   returns a list of PreCondDesc objects that wrap the preconditions
     *   for the given object in the given role for this action.  
     */
    getObjPreCondDescList(obj, preCondProp, checkArg, whichObj)
    {
        local lst;
        
        /* get the list of preconditions for the given object in its role */
        lst = getObjPreConditions(obj, preCondProp, whichObj);

        /* if there are no preconditions, return an empty list */
        if (lst == nil)
            return [];

        /* 
         *   wrap the precondition objects in descriptors, so that we can
         *   keep track of the check argument that goes with these
         *   preconditions 
         */
        return lst.mapAll({x: new PreCondDesc(x, checkArg)});
    }

    /*
     *   Call a method on all of the precondition objects in the
     *   precondition list obtained from the given property of the given
     *   object. 
     */
    callPreConditions(lst, allowImplicit)
    {
        local ret;
        local i;
        
        /* presume we won't call any implicit commands */
        ret = nil;

        /* 
         *   note the original descriptor list order, so that we can retain
         *   the current ordering when the execution order doesn't require
         *   changes 
         */
        i = 0;
        lst.forEach({x: x.index_ = i++});

        /* sort the precondition list by execution order */
        lst = lst.sort(SortAsc, function(a, b) {
            local delta;
            
            /* if the execution orders differ, sort by execution order */
            delta = a.cond_.preCondOrder - b.cond_.preCondOrder;
            if (delta != 0)
                return delta;

            /* otherwise, retain the original list order */
            return a.index_ - b.index_;
        });

        /* catch any 'exit' signals within the preconditions */
        try
        {
            /* invoke the check method for each condition in the list */
            foreach (local cur in lst)
            {
                /* 
                 *   call this precondition; if it runs an implicit
                 *   command, note that we have run an implicit command 
                 */
                if (cur.checkPreCondition(allowImplicit))
                    ret = true;
            }
        }
        catch (ExitSignal es)
        {
            /* 
             *   any 'exit' that occurs within a precondition is a failure
             *   for the enclosing action 
             */
            gTranscript.noteFailure();
            
            /* re-throw the 'exit' to cancel the enclosing action */
            throw es;
        }

        /* return true if any implicit commands were executed */
        return ret;
    }

    /* 
     *   Our list of action-level pre-condition objects.  These are the
     *   conditions that apply to the overall action, not to the
     *   individual objects involved.  (Object-level pre-conditions are
     *   attached to the objects, not to the action.)  
     */
    preCond = []

    /*
     *   Finish the result list for a resolved noun phrase.  This is used
     *   just before disambiguation.  We'll give each object in the list a
     *   chance to filter the list with filterResolveList, and we'll note
     *   the noun phrase we matched in each resolved object.  
     */
    finishResolveList(lst, whichObj, np, requiredNum)
    {
        /* give each object a chance to filter the list */
        foreach (local cur in lst)
            lst = cur.obj_.filterResolveList(
                lst, self, whichObj, np, requiredNum);

        /* stash the noun phrase in each object in the list */
        foreach (local cur in lst)
            cur.np_ = np;

        /* return the list */
        return lst;
    }

    /*
     *   Get a list of verification results for the given ResolveInfo
     *   objects, sorted from best to worst.  Each entry in the returned
     *   list will be a VerifyResultList object whose obj_ property is set
     *   to the ResolveInfo object for which it was generated.  
     */
    getSortedVerifyResults(lst, verProp, preCondProp, remapProp,
                           whichObj, np, requiredNum)
    {
        local results;
        local idx;

        /* if there's nothing in the list, we're done */
        if (lst == [])
            return lst;

        /*
         *   Before we run verification, give each object in the list a
         *   chance to do its own filtering on the entire list.  This form
         *   of filtering allows an object to act globally on the list, so
         *   that it can take special action according to the presence or
         *   absence of other objects, and can affect the presence of other
         *   objects.  
         */
        lst = finishResolveList(lst, whichObj, np, requiredNum);

        /* create a vector to hold the verification results */        
        results = new Vector(lst.length());

        /*
         *   Call the given verifier method on each object, noting each
         *   result.
         */
        idx = 0;
        foreach (local cur in lst)
        {
            local curResult;

            /* call the verifier method and note the current result */
            curResult = callVerifyProp(cur.obj_, verProp, preCondProp,
                                       remapProp, nil, whichObj);

            /* 
             *   save the ResolveInfo in the verify result list object, so
             *   that we can figure out later (after sorting the results)
             *   which original ResolveInfo this verification result
             *   applies to 
             */
            curResult.obj_ = cur;

            /* remember the original list order */
            curResult.origOrder = idx++;

            /* add this verify result to our result vector */
            results.append(curResult);
        }

        /* 
         *   Sort the results in descending order of logicalness, and
         *   return the sorted list.  When results are equivalently
         *   logical, keep the results in their existing order.  
         */
        return results.toList().sort(SortDesc, function(x, y)
        {
            /* compare the logicalness */
            local c = x.compareTo(y);

            /* 
             *   if it's the same, keep in ascending pluralOrder - note
             *   that we must reverse the sense of this comparison, since
             *   we're sorting the overall list in descending order
             */
            if (c == 0)
                c = (y.obj_.obj_.pluralOrder - x.obj_.obj_.pluralOrder);

            /* if they're otherwise the same, preserve the original order */
            if (c == 0)
                c = (y.origOrder - x.origOrder);

            /* return the result */
            return c;
        });
    }

    /*
     *   Combine any remapped verify results in the given verify result
     *   list.  We will remove any result that was remapped to a different
     *   object if and only if the target of the remapping appears in the
     *   list and has the same results as the remapped original.  When
     *   objects are remapped in this fashion, they become effectively
     *   equivalent for the purposes of this command, so we don't have to
     *   distinguish between them for disambiguation or defaulting
     *   purposes.  
     */
    combineRemappedVerifyResults(lst, role)
    {
        /* scan each element in the list */
        for (local i = 1, local len = lst.length() ; i <= len ; ++i)
        {
            local cur = lst[i];
            
            /* if this element has been remapped, consider it further */
            if (cur.remapTarget_ != nil)
            {
                local other;
                
                /* 
                 *   scan for another entry in the list that matches us
                 *   for remapping purposes 
                 */
                other = lst.indexWhich(
                    {x: x != cur
                        && cur.matchForCombineRemapped(x, self, role)});

                /* 
                 *   if we found another entry, delete the other entry,
                 *   since it is indistinguishable from this entry
                 */
                if (other != nil)
                {
                    /* remove this element from the list */
                    lst -= cur;

                    /* adjust our list counters for the deletion */
                    --i;
                    --len;
                }
            }
        }

        /* return the updated list */
        return lst;
    }

    /*
     *   Filter an ambiguous object list using the given verification
     *   method.  We call the given verification method on each object,
     *   noting the result, then find the best (most logical) result in
     *   the list.  We reduce the set to the objects that all have the
     *   same best value - everything else in the list is less logical, so
     *   we discard it.  This gives us a set of objects that are all of
     *   equivalent likelihood and all of the best likelihood of all the
     *   objects.
     *   
     *   This is the typical way that we disambiguate a list of objects,
     *   but this is merely a service routine, so individual actions can
     *   choose to use this or other mechanisms as appropriate.  
     */
    filterAmbiguousWithVerify(lst, requiredNum, verProp,
                              preCondProp, remapProp, whichObj, np)
    {
        local results;
        local discards;
        local keepers;
        local bestResult;
        local uniqueKeepers;

        /* if there's nothing in the list, there's nothing to do */
        if (lst == [])
            return lst;

        /* first, filter out redundant facets */
        lst = filterFacets(lst);
        
        /* 
         *   Call the verifier method on each object, and sort the results
         *   from best to worst.  
         */
        results = getSortedVerifyResults(lst, verProp, preCondProp,
                                         remapProp, whichObj,
                                         np, requiredNum);

        /* note the best result value */
        bestResult = results[1];

        /* 
         *   ask the noun phrase to filter this list down to the ones it
         *   wants to keep 
         */
        keepers = np.getVerifyKeepers(results);

        /*
         *   Count the number of unique keepers - this is the number of
         *   items in the keepers list that don't have any equivalents in
         *   the keepers list.
         *   
         *   To calculate this number, start with the total number of
         *   items in the list, and reduce it by one for each item with an
         *   earlier equivalent in the list.  Note that we only ignore
         *   items with unique equivalents *earlier* in the list so that
         *   we keep exactly one of each equivalent - if we ignored every
         *   element with a unique equivalent elsewhere in the list, we'd
         *   ignore every equivalent item, so we'd only count the items
         *   with no equivalents at all.  
         */
        uniqueKeepers = keepers.length();
        for (local i = 1, local cnt = keepers.length() ; i <= cnt ; ++i)
        {
            local eqIdx;
            
            /* check for a unique equivalent earlier in the list */
            eqIdx = keepers.indexWhich(
                {x: x.obj_.obj_.isVocabEquivalent(keepers[i].obj_.obj_)});
            if (eqIdx != nil && eqIdx < i)
            {
                /* 
                 *   this one has an earlier equivalent, so don't include
                 *   it in the unique item count 
                 */
                --uniqueKeepers;
            }
        }

        /*
         *   If we found more items to keep than were required by the
         *   caller, we were not able to reduce the set to an unambiguous
         *   subset.  In this case, keep *all* of the items that are
         *   logical. 
         */
        if (uniqueKeepers > requiredNum)
        {
            local allAllowed;
            
            /* filter so that we keep all of the logical results */
            allAllowed = results.subset({x: x.allowAction});

            /* if that list is non-empty, use it as the keepers */
            if (allAllowed.length() != 0)
                keepers = allAllowed;
        }

        /* 
         *   Get the list of discards - this is the balance of the
         *   original result list after removing the best ones.  
         */
        discards = results - keepers;

        /* 
         *   We now have the set of objects we want, but the entries in
         *   the list are all VerifyResultList instances, and we just want
         *   the objects - pull out a list of just the ResolveInfo, and
         *   return that.  
         */
        keepers = keepers.mapAll({x: x.obj_});

        /*
         *   Check to see if we should set flags in the results.  If we
         *   eliminated any objects, flag the remaining objects as having
         *   been selected through disambiguation.  If the best of the
         *   discarded objects were logical, flag the survivors as
         *   "unclear," because we only selected them as better than the
         *   discards, and not because they were the only possible
         *   choices.  
         */
        if (discards != [])
        {
            local addedFlags;
            
            /* 
             *   We have reduced the set.  If the best of the discards was
             *   ranked as highly as the survivors at the coarsest level,
             *   flag the survivors as having been "unclearly"
             *   disambiguated; otherwise, mark them as "clearly"
             *   disambiguated.  
             */
            if (keepers.indexOf(bestResult.obj_) != nil
                && (discards[1].getEffectiveResult().resultRank ==
                    bestResult.getEffectiveResult().resultRank))
            {
                /* 
                 *   we had to make a choice that discarded possibilities
                 *   that were valid, though not as good as the one we
                 *   chose - mark the objects as being not perfectly clear 
                 */
                addedFlags = UnclearDisambig;

                /* 
                 *   if the keepers and the rejects are all basic
                 *   equivalents, don't bother flagging this as unclear,
                 *   since there's no point in mentioning that we chose
                 *   one basic equivalent over another, as they all have
                 *   the same name 
                 */
                if (BasicResolveResults.filterWithDistinguisher(
                    keepers + discards.mapAll({x: x.obj_}),
                    basicDistinguisher).length() == 1)
                {
                    /* they're all basic equivalents - mark as clear */
                    addedFlags = ClearDisambig;
                }
            }
            else
            {
                /* the choice is clear */
                addedFlags = ClearDisambig;
            }

            /* add the flags to each survivor */
            foreach (local cur in keepers)
                cur.flags_ |= addedFlags;
        }

        /* return the results */
        return keepers;
    }

    /*
     *   Filter a plural list with a verification method.  We'll reduce
     *   the list to the subset of objects that verify as logical, if
     *   there are any.  If there are no logical objects in the list,
     *   we'll simply return the entire original list.  
     */
    filterPluralWithVerify(lst, verProp, preCondProp, remapProp, whichObj, np)
    {
        local results;

        /* if there's nothing in the list, there's nothing to do */
        if (lst == [])
            return lst;

        /* first, filter out redundant facets */
        lst = filterFacets(lst);
        
        /* 
         *   Call the verifier method on each object, and sort the results
         *   from best to worst.  
         */
        results = getSortedVerifyResults(lst, verProp, preCondProp,
                                         remapProp, whichObj, np, nil);

        /*
         *   If the best (and thus first) result allows the action, filter
         *   the list to keep only the "keepers," as determined by the noun
         *   phrase.  Otherwise, there are no allowed results, so return
         *   the original list.  
         */
        if (results[1].allowAction)
            results = np.getVerifyKeepers(results);

        /* return the resolve results objects from the list */
        return results.mapAll({x: x.obj_});
    }

    /*
     *   Filter out redundant facets of the same object.  The various
     *   facets of an object are equivalent to the parser.  An object that
     *   has multiple facets is meant to appear to be one game world
     *   object from the perspective of a character - the multiple facet
     *   objects are an internal implementation detail. 
     */
    filterFacets(lst)
    {
        local result;
        local actor = gActor;

        /* 
         *   create a vector for the result list, presuming we'll keep all
         *   of the original list elements 
         */
        result = new Vector(lst.length(), lst);

        /* check for facets */
        foreach (local cur in lst)
        {
            local allFacets;

            /* if this item has any facets, check for them in our results */
            allFacets = cur.obj_.getFacets() + cur.obj_;
            if (allFacets.length() != 0)
            {
                local inScopeFacets;
                local best;
                
                /* make a new list of the facets that appear in the results */
                inScopeFacets = allFacets.subset(
                    {f: result.indexWhich({r: r.obj_ == f}) != nil});

                /* pick the best of those facets */
                best = findBestFacet(actor, inScopeFacets);
                
                /* 
                 *   remove all of the facets besides the best one from the
                 *   result list - this will ensure that we have only the
                 *   single best facet in the final results 
                 */
                foreach (local r in result)
                {
                    /* 
                     *   if this result list item is in the facet list, and
                     *   it's not the best facet, delete it from the result
                     *   list 
                     */
                    if (r.obj_ != best
                        && inScopeFacets.indexOf(r.obj_) != nil)
                        result.removeElement(r);
                }

            }
        }

        /* return the result list */
        return result.toList();
    }

    /*
     *   Get a default object using the given verification method.  We'll
     *   start with the 'all' list, then use the verification method to
     *   reduce the list to the most likely candidates.  If we find a
     *   unique most likely candidate, we'll return a ResolveInfo list
     *   with that result; otherwise, we'll return nothing, since there is
     *   no suitable default.  
     */
    getDefaultWithVerify(resolver, verProp, preCondProp, remapProp,
                         whichObj, np)
    {
        local lst;
        local results;
        local bestResult;
        
        /* 
         *   Start with the 'all' list for this noun phrase.  This is the
         *   set of every object that we consider obviously suitable for
         *   the command, so it's a good starting point to guess at a
         *   default object.
         */
        lst = resolver.getAllDefaults();

        /* eliminate objects that can't be defaults for this action */
        lst = lst.subset({x: !x.obj_.hideFromDefault(self)});

        /* 
         *   reduce equivalent items to a single instance of each
         *   equivalent - if we have several equivalent items that are
         *   equally good as defaults, we can pick just one 
         */
        lst = resolver.filterAmbiguousEquivalents(lst, np);

        /* if we have no entries in the list, there is no default */
        if (lst == [])
            return nil;

        /* 
         *   get the verification results for these objects, sorted from
         *   best to worst 
         */
        results = getSortedVerifyResults(lst, verProp, preCondProp,
                                         remapProp, whichObj, np, nil);

        /* eliminate redundant remapped objects */
        results = combineRemappedVerifyResults(results, whichObj);

        /* note the best result */
        bestResult = results[1];

        /* 
         *   if the best item is not allowed as an implied object, there
         *   is no default 
         */
        if (!bestResult.allowImplicit)
            return nil;

        /* 
         *   The best item must be uniquely logical in order to be a
         *   default - if more than one item is equally good, it makes no
         *   sense to assume anything about what the user meant.  So, if
         *   we have more than one item, and the second item is equally as
         *   logical as the first item, we cannot supply a default.  (The
         *   second item cannot be better than the first item, because of
         *   the sorting - at most, it can be equally good.)  
         */
        if (results.length() != 1 && bestResult.compareTo(results[2]) == 0)
            return nil;

        /* 
         *   We have a uniquely logical item, so we can assume the user
         *   must have been referring to this item.  Return the
         *   ResolveInfo for this item (which the verify result sorter
         *   stashed in the obj_ property of the verify result object).
         *   
         *   Before returning the list, clear the 'all' flag in the result
         *   (getAll() set that flag), and replace it with the 'default'
         *   flag, since the object is an implied default.  
         */
        bestResult.obj_.flags_ &= ~MatchedAll;
        bestResult.obj_.flags_ |= DefaultObject;

        /* return the result list */
        return [bestResult.obj_];
    }

    /*
     *   A synthesized Action (one that's generated by something other than
     *   parsing a command line, such as an event action or nested action)
     *   won't have a parser token list attached to it.  If we're asked to
     *   get the token list, we need to check for this possibility.  If we
     *   don't have a token list, but we do have a parent action, we'll
     *   defer to the parent action.  Otherwise, we'll simply return nil.
     */
    getOrigTokenList()
    {
        /* if we don't have a token list, look elsewhere */
        if (tokenList == nil)
        {
            /* if we have a parent action, defer to it */
            if (parentAction != nil)
                return parentAction.getOrigTokenList();

            /* we have nowhere else to look, so return an empty list */
            return [];
        }

        /* inherit the standard handling from BasicProd */
        return inherited();
    }
    
    /* 
     *   Create a topic qualifier resolver.  This type of resolver is used
     *   for qualifier phrases (e.g., possessives) within topic phrases
     *   within objects of this verb.  Topics *usually* only apply to
     *   TopicActionBase subclasses, but not exclusively: action remappings
     *   can sometimes require a topic phrase from one action to be
     *   resolved in the context of another action that wouldn't normally
     *   involve a topic phrase.  That's why this is needed on the base
     *   Action class. 
     */
    createTopicQualifierResolver(issuingActor, targetActor)
    {
        /* create and return a topic qualifier object resolver */
        return new TopicQualifierResolver(self, issuingActor, targetActor);
    }

    /* 
     *   List of objects that verified okay on a prior pass.  This is a
     *   scratch-pad for use by verifier routines, to keep track of work
     *   they've already done.  A few verifiers use this as a way to
     *   detect when an implicit action actually finished the entire job,
     *   which would in many cases result in a verify failure if not
     *   checked (because a command that effects conditions that already
     *   hold is normally illogical); by tracking that the verification
     *   previously succeeded, the verifier can know that the action
     *   should be allowed to proceed and do nothing.  
     */
    verifiedOkay = []

    /*
     *   Get the missing object response production for a given resolver
     *   role.  (The base action doesn't have any objects, so there's no
     *   such thing as a missing object query.)
     */
    getObjResponseProd(resolver) { return nil; }
;

/*
 *   Call the roomBeforeAction method on a given room's containing rooms,
 *   then on the room itself.  
 */
callRoomBeforeAction(room)
{
    /* first, call roomBeforeAction on the room's containers */
    room.forEachContainer(callRoomBeforeAction);

    /* call roomBeforeAction on this room */
    room.roomBeforeAction();
}

/*
 *   Call the roomAfterAction method on a given room, then on the room's
 *   containing rooms.  
 */
callRoomAfterAction(room)
{
    /* first, call roomAfterAction on this room */
    room.roomAfterAction();

    /* next, call roomAfterAction on the room's containers */
    room.forEachContainer(callRoomAfterAction);
}

/* ------------------------------------------------------------------------ */
/*
 *   Intransitive Action class - this is an action that takes no objects.
 *   In general, each subclass should implement its action handling in its
 *   execAction() method.  
 */
class IAction: Action
    /* 
     *   resolve my noun phrases to objects 
     */
    resolveNouns(issuingActor, targetActor, results)
    {
        /* 
         *   We have no objects to resolve.  The only thing we have to do
         *   is note in the results our number of structural noun slots
         *   for the verb, which is zero, since we have no objects at all. 
         */
        results.noteNounSlots(0);
    }

    /*
     *   Execute the action.  
     */
    doActionMain()
    {
        /* 
         *   we have no objects to iterate, so simply run through the
         *   execution sequence once 
         */
        doActionOnce();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   "All" and "Default" are a pseudo-actions used for dobjFor(All),
 *   iobjFor(Default), and similar catch-all handlers.  These are never
 *   executed as actual actions, but we define them so that dobjFor(All)
 *   and the like won't generate any warnings for undefined actions.
 */
class AllAction: object
;
class DefaultAction: object
;

/* ------------------------------------------------------------------------ */
/*
 *   Transitive Action class - this is an action that takes a direct
 *   object.
 *   
 *   For simplicity, this object is its own object resolver - we really
 *   don't need a separate resolver object because we have only one object
 *   list for this verb.  (In contrast, an action with both a direct and
 *   indirect object might need separate resolution rules for the two
 *   objects, and hence would need separate resolver objects for the two.)
 *   
 *   The advantage of implementing the Resolver behavior in this object,
 *   rather than using a separate object, is that it's less trouble to
 *   override object resolution rules - simply override the resolver
 *   methods in the subclass where you define the grammar rule for the
 *   action.  
 */
class TAction: Action, Resolver
    construct()
    {
        /* inherit only the Action constructor */
        inherited Action.construct();
    }

    resetAction()
    {
        /* inherit default handling */
        inherited();
        
        /* discard our cached resolver */
        dobjResolver_ = nil;
    }
    
    /*
     *   Create an action for retrying an original action with changes. 
     */
    createForRetry(orig)
    {
        local action;
        
        /* create the new action based on the original action */
        action = createActionFrom(orig);

        /* 
         *   do not include the new command in undo, since it's merely
         *   part of the enclosing explicit command 
         */
        action.includeInUndo = nil;

        /* mark the action as nested */
        action.setNested();

        /*
         *   Even if the original action is implicit, don't announce this
         *   action as implicit, because it's good enough to have
         *   announced the original implicit action.  Now, this new
         *   command actually still is implicit if the original was - we
         *   simply don't want to announce it as such.  To suppress the
         *   extra announcement while still retaining the rest of the
         *   desired implicitness, simply set the implicitMsg property of
         *   the new action to nil; when there's no implicitMsg, there's
         *   no announcement. 
         */
        action.implicitMsg = nil;

        /* return the new action */
        return action;
    }

    /*
     *   Retry an intransitive action as a single-object action.  We'll
     *   obtain a indirect object using the normal means (first looking
     *   for a default, then prompting the player if we can't find a
     *   suitable default).  'orig' is the original zero-object action.
     *   
     *   This routine terminates with 'exit' if it doesn't throw some
     *   other error.  
     */
    retryWithMissingDobj(orig, asker)
    {
        /* resolve and execute the replacement action */
        resolveAndReplaceAction(createForMissingDobj(orig, asker));
    }

    /*
     *   Retry an action as a single-object action with an ambiguous
     *   direct object.  We'll ask which of the given possible objects is
     *   intended.  
     */
    retryWithAmbiguousDobj(orig, objs, asker, objPhrase)
    {
        local action;
        local resolver;
        
        /* create a missing-direct-object replacement action */
        action = createForMissingDobj(orig, asker);

        /* reduce the object list to the objects in scope */
        resolver = action.getDobjResolver(gIssuingActor, gActor, true);
        objs = objs.subset({x: resolver.objInScope(x)});

        /* plug in the ambiguous direct object list */
        action.dobjMatch = new PreResolvedAmbigProd(objs, asker, objPhrase);
        
        /* resolve and execute the replacement action */
        resolveAndReplaceAction(action);
    }

    /*
     *   Test to see if askForDobj() would find a default direct object.
     *   Returns true if there's a default, nil if not.  If this returns
     *   true, then askForDobj() will simply take the default and proceed;
     *   otherwise, it will have to actually ask the user for the missing
     *   information.  
     */
    testRetryDefaultDobj(orig)
    {
        /* create the new action for checking for a direct object */
        local action = createForMissingDobj(orig, ResolveAsker);

        /* get the default dobj */
        local def = action.getDefaultDobj(
            action.dobjMatch,
            action.getDobjResolver(gIssuingActor, gActor, nil));

        /* if there's exactly one result, then we have a default */
        return (def != nil && def.length() == 1);
    }

    /*
     *   Create an instance of this action for retrying a given
     *   single-object action with a missing direct object.  
     */
    createForMissingDobj(orig, asker)
    {
        /* create the action for a retry */
        local action = createForRetry(orig);

        /* use an empty noun phrase for the new action's direct object */
        action.dobjMatch = new EmptyNounPhraseProd();

        /* set our custom response production and ResolveAsker */
        action.dobjMatch.setPrompt(action.askDobjResponseProd, asker);

        /* initialize the new action with any pre-resolved parts */
        action.initForMissingDobj(orig);

        /* return the new action */
        return action;
    }

    /*
     *   Initialize this action in preparation for retrying with a missing
     *   direct object.  This routine must copy any phrases from the
     *   original action that have already been resolved.  This base
     *   TAction implementation obviously can't have anything pre-resolved
     *   in the original, since the original must simply be an IAction.
     *   Subclasses must override as appropriate for the kinds of base
     *   actions from which they can be retried.  
     */
    initForMissingDobj(orig) { }

    /*
     *   The root production to use to parse missing direct object
     *   responses.  By default, this is nounList, but individual actions
     *   can override this as appropriate.
     *   
     *   Note that language modules might want to override this to allow
     *   for special responses.  For example, in English, some verbs might
     *   want to override this with a specialized production that allows
     *   the appropriate preposition in the response.  
     */
    askDobjResponseProd = nounList

    /* get the missing object response production for a given role */
    getObjResponseProd(resolver)
    {
        /* if it's the direct object, return the dobj response prod */
        if (resolver.whichObject == DirectObject)
            return askDobjResponseProd;

        /* otherwise use the default handling */
        return inherited(resolver);
    }

    /*
     *   Can the direct object potentially resolve to the given simulation
     *   object?  This only determines if the object is a *syntactic*
     *   match, meaning that it can match at a vocabulary and grammar
     *   level.  This doesn't test it for logicalness or check that it's an
     *   otherwise valid resolution.  
     */
    canDobjResolveTo(obj)
    {
        /* check our dobj match tree to see if it can resolve to 'obj' */
        return dobjMatch.canResolveTo(
            obj, self, issuer_, actor_, DirectObject);
    }

    /*
     *   Resolve objects.  This is called at the start of command
     *   execution to resolve noun phrases in the command to specific
     *   objects.  
     */
    resolveNouns(issuingActor, targetActor, results)
    {
        /* note that we have a single noun slot (the direct object) */
        results.noteNounSlots(1);

        /* 
         *   Ask the direct object noun phrase list to resolve itself, and
         *   store the resulting object list.  Since we're starting a
         *   resolution pass through our objects, reset the resolver if
         *   we're reusing it.  
         */
        dobjList_ = dobjMatch.resolveNouns(
            getDobjResolver(issuingActor, targetActor, true), results);
    }

    /* a transitive action has one noun phrase: the direct object */
    predicateNounPhrases = [&dobjMatch]

    /* get the role of an object */
    getRoleFromIndex(idx)
    {
        /* we only take a single object role - the direct object */
        return (idx == 1 ? DirectObject : inherited(idx));
    }

    /* get the resolved object in a given role */
    getObjectForRole(role)
    {
        /* return the direct object if requested */
        return (role == DirectObject ? getDobj() : inherited(role));
    }

    /* get the match tree for the noun phrase in the given role */
    getMatchForRole(role)
    {
        /* return the direct object match tree if requested */
        return (role == DirectObject ? dobjMatch : inherited(role));
    }

    /* get the 'verify' property for a given object role */
    getVerifyPropForRole(role)
    {
        return (role == DirectObject ? verDobjProp : inherited(role));
    }

    /* get the 'preCond' property for a given object role */
    getPreCondPropForRole(role)
    {
        return (role == DirectObject ? preCondDobjProp : inherited(role));
    }

    /* get the 'remap' property for a given object role */
    getRemapPropForRole(role)
    {
        return (role == DirectObject ? remapDobjProp : inherited(role));
    }

    /* get the ResolveInfo for the given object */
    getResolveInfo(obj, oldRole)
    {
        local info = nil;
        
        /* scan our resolved direct object list for the given object */
        if (dobjList_ != nil)
            info = dobjList_.valWhich({x: x.obj_ == obj});

        /* if we didn't find one, create one from scratch */
        if (info == nil)
        {
            /* 
             *   if there's anything in the old dobj role, copy the flags
             *   and noun phrase to the new role
             */
            if (dobjList_.length() > 0)
            {
                /* get the flags and noun phrase from the old role */
                info = new ResolveInfo(
                    obj, dobjList_[1].flags_, dobjList_[1].np_);
            }
            else
            {
                /* there's no old role, so start from scratch */
                info = new ResolveInfo(obj, 0, nil);
            }
        }

        /* return what we found (or created) */
        return info;
    }

    /* get the list of resolved objects in the given role */
    getResolvedObjList(which)
    {
        return (which == DirectObject ? getResolvedDobjList()
                : inherited(which));
    }
    
    /* get the list of resolved direct objects */
    getResolvedDobjList()
    {
        /* 
         *   if we have a direct object list, return the objects from it;
         *   otherwise, return an empty list 
         */
        return (dobjList_ == nil ? nil : dobjList_.mapAll({x: x.obj_}));
    }

    /* manually set the resolved objects - we'll set the direct object */
    setResolvedObjects(dobj)
    {
        /* set the resolved direct object */
        setResolvedDobj(dobj);
    }

    /* set the resolved direct object */
    setResolvedDobj(dobj)
    {
        /* 
         *   set the direct object tree to a fake grammar tree that
         *   resolves to our single direct object, in case we're asked to
         *   resolve explicitly 
         */
        dobjMatch = new PreResolvedProd(dobj);

        /* 
         *   Set the resolved direct object list to the single object or
         *   to the list of objects, depending on what we received.
         */
        dobjList_ = makeResolveInfoList(dobj);

        /* set the current object as well */
        dobjCur_ = (dobjList_.length() > 0 ? dobjList_[1].obj_ : nil);
        dobjInfoCur_ = (dobjList_.length() > 0 ? dobjList_[1] : nil);
    }

    /* manually set the unresolved object noun phrase match trees */
    setObjectMatches(dobj)
    {
        /* 
         *   if it's a ResolveInfo, handle it as a resolved object;
         *   otherwise handle it as a match tree to be resolved 
         */
        if (dobj.ofKind(ResolveInfo))
            setResolvedDobj(dobj);
        else
            dobjMatch = dobj;
    }

    /* check that the resolved objects are in scope */
    resolvedObjectsInScope()
    {
        /* check the direct object */
        return getDobjResolver(gIssuingActor, gActor, true)
            .objInScope(dobjList_[1].obj_);
    }

    /*
     *   Get a message parameter object for the action.  We define 'dobj'
     *   as the direct object, in addition to any inherited targets.  
     */
    getMessageParam(objName)
    {
        switch(objName)
        {
        case 'dobj':
            /* return the current direct object */
            return dobjCur_;

        default:
            /* inherit default handling */
            return inherited(objName);
        }
    }

    /*
     *   Execute the action.  We'll run through the execution sequence
     *   once for each resolved direct object.  
     */
    doActionMain()
    {
        /* 
         *   Set the direct object list as the antecedent, using the
         *   language-specific pronoun setter.  Don't set pronouns for a
         *   nested command, because the player didn't necessarily refer to
         *   the objects in a nested command.  
         */
        if (parentAction == nil)
            gActor.setPronoun(dobjList_);

        /* we haven't yet canceled the iteration */
        iterationCanceled = nil;

        /* pre-calculate the multi-object announcement text for each dobj */
        cacheMultiObjectAnnouncements(dobjList_, DirectObject);

        /* run through the sequence once for each direct object */
        for (local i = 1, local len = dobjList_.length() ;
             i <= len && !iterationCanceled ; ++i)
        {
            /* make this object our current direct object */
            dobjCur_ = dobjList_[i].obj_;
            dobjInfoCur_ = dobjList_[i];

            /* announce the object if appropriate */
            announceActionObject(dobjList_[i], len, whichMessageObject);

            /* run the execution sequence for the current direct object */
            doActionOnce();

            /* if we're top-level, count the iteration in the transcript */
            if (parentAction == nil)
                gTranscript.newIter();
        }
    }

    /* get the precondition descriptor list for the action */
    getPreCondDescList()
    {
        /* 
         *   return the inherited preconditions plus the conditions that
         *   apply to the direct object 
         */
        return inherited()
            + getObjPreCondDescList(dobjCur_, preCondDobjProp, dobjCur_,
                                    DirectObject);
    }
        
    /*
     *   Get the list of active objects.  We have only a direct object, so
     *   we'll return a list with the current direct object. 
     */
    getCurrentObjects()
    {
        return [dobjCur_];
    }

    /* set the current objects */
    setCurrentObjects(lst)
    {
        dobjCur_ = lst[1];
        dobjInfoCur_ = nil;
    }

    /*
     *   Verify the action.
     */
    verifyAction()
    {
        local result;
        
        /* invoke any general (non-object) pre-condition verifiers */
        result = callVerifyPreCond(nil);

        /* 
         *   invoke the verifier routine ("verXxx") on the current direct
         *   object and return the result
         */
        result = callVerifyProp(dobjCur_, verDobjProp, preCondDobjProp,
                                remapDobjProp, result, DirectObject);

        /* verify that the action routine ("doXxx") exists on the object */
        return verifyHandlersExist(
            [dobjCur_], [verDobjProp, actionDobjProp, checkDobjProp],
            result);
    }

    /* initialize tentative resolutions for other noun phrases */
    initTentative(issuingActor, targetActor, whichObj)
    {
        /* 
         *   we have only one noun phrase, so there's nothing else to
         *   tentatively resolve 
         */
    }

    /*
     *   Check for remapping 
     */
    checkRemapping()
    {
        local remapInfo;
        
        /* check for remapping in the direct object */
        if ((remapInfo = dobjCur_.(remapDobjProp)) != nil)
        {
            /* 
             *   we have a remapping, so apply it - note that this won't
             *   return, since this will completely replace the command
             *   and thus terminate the old command with 'exit' 
             */
            remapAction(nil, DirectObject, remapInfo);
        }
    }

    /*
     *   Check the command.
     *   
     *   For a single-object transitive action, this runs the catch-all
     *   'check' properties (the dobjFor(Default) and dobjFor(All) 'check'
     *   methods) on the direct object, then calls the individual 'check'
     *   routine for this specific action.  
     */
    checkAction()
    {
        try
        {
            /* call the catch-all 'check' properties */
            if (!callCatchAllProp(dobjCur_, checkDobjProp,
                                  &checkDobjDefault, &checkDobjAll))
            {
                /* 
                 *   the action-specific check routine overrides any
                 *   Default catch-all, so call the direct object's check
                 *   routine (its "checkXxx") method 
                 */
                dobjCur_.(checkDobjProp)();
            }
        }
        catch (ExitSignal es)
        {
            /* mark the action as a failure in the transcript */
            gTranscript.noteFailure();

            /* re-throw the signal */
            throw es;
        }
    }

    /*
     *   Execute the command. 
     */
    execAction()
    {
        /* call the catch-all 'action' properties */
        if (!callCatchAllProp(dobjCur_, actionDobjProp,
                              &actionDobjDefault, &actionDobjAll))
        {
            /* 
             *   the verb-specific 'action' handler overrides any Default
             *   action handler, so call the verb-specific 'action'
             *   routine in the direct object (the "doXxx" method) 
             */
            dobjCur_.(actionDobjProp)();
        }
    }

    /*
     *   The direct object preconditions, verifier, remapper, check, and
     *   action methods for this action.  Each concrete action must define
     *   these appropriately.  By convention, the methods are named like
     *   so:
     *   
     *.  preconditions: preCondDobjAction
     *.  verifier: verDobjAction
     *.  remap: remapDobjAction
     *.  check: checkDobjAction
     *.  action: actionDobjAction
     *   
     *   where the 'Action' suffix is replaced by the name of the action.
     *   The DefineTAction macro applies this convention, so in most cases
     *   library and game authors will never have to create all of those
     *   property names manually.  
     */
    verDobjProp = nil
    preCondDobjProp = nil
    remapDobjProp = nil
    checkDobjProp = nil
    actionDobjProp = nil

    /*
     *   Get my direct object resolver.  If I don't already have one,
     *   create one and cache it; if I've already cached one, return it.
     *   Note that we cache the resolver because it can sometimes take a
     *   bit of work to set one up (the scope list can in some cases be
     *   complicated to calculate).  We use the resolver only during the
     *   object resolution phase; since game state can't change during
     *   this phase, it's safe to keep a cached copy.  
     */
    getDobjResolver(issuingActor, targetActor, reset)
    {
        /* create a new resolver if we don't already have one cached */
        if (dobjResolver_ == nil)
            dobjResolver_ = createDobjResolver(issuingActor, targetActor);

        /* reset the resolver if desired */
        if (reset)
            dobjResolver_.resetResolver();

        /* return it */
        return dobjResolver_;
    }

    /*
     *   Create a resolver for the direct object.  By default, we are our
     *   own resolver.  Some actions might want to override this to create
     *   and return a specialized resolver instance if special resolution
     *   rules are needed.  
     */
    createDobjResolver(issuingActor, targetActor)
    {
        /* initialize myself as a resolver */
        initResolver(issuingActor, targetActor);

        /* return myself */
        return self;
    }

    /*
     *   Does this action allow "all" to be used in noun phrases?  By
     *   default, we allow it or not according to a gameMain property.
     *   
     *   Note that the inventory management verbs (TAKE, TAKE FROM, DROP,
     *   PUT IN, PUT ON) override this to allow "all" to be used, so
     *   disallowing "all" here (or via gameMain) won't disable "all" for
     *   those verbs.  
     */
    actionAllowsAll = (gameMain.allVerbsAllowAll)

    /*
     *   Resolve 'all' for the direct object, given a list of everything
     *   in scope.  By default, we'll simply return everything in scope;
     *   some actions might want to override this to return a more
     *   specific list of objects suitable for 'all'.  
     */
    getAllDobj(actor, scopeList)
    {
        return scopeList;
    }

    /* filter an ambiguous direct object noun phrase */
    filterAmbiguousDobj(lst, requiredNum, np)
    {
        /* filter using the direct object verifier method */
        return filterAmbiguousWithVerify(lst, requiredNum, verDobjProp,
                                         preCondDobjProp, remapDobjProp,
                                         DirectObject, np);
    }

    /* filter a plural phrase */
    filterPluralDobj(lst, np)
    {
        /* filter using the direct object verifier method */
        return filterPluralWithVerify(lst, verDobjProp, preCondDobjProp,
                                      remapDobjProp, DirectObject, np);
    }

    /* get the default direct object */
    getDefaultDobj(np, resolver)
    {
        /* get a default direct object using the verify method */
        return getDefaultWithVerify(resolver, verDobjProp, preCondDobjProp,
                                    remapDobjProp, DirectObject, np);
    }

    /* get the current direct object of the command */
    getDobj() { return dobjCur_; }

    /* get the full ResolveInfo associated with the current direct object */
    getDobjInfo() { return dobjInfoCur_; }

    /* get the object resolution flags for the direct object */
    getDobjFlags() { return dobjInfoCur_ != nil ? dobjInfoCur_.flags_ : 0; }

    /* get the number of direct objects */
    getDobjCount() { return dobjList_ != nil ? dobjList_.length() : 0; }

    /* get the original token list of the current direct object phrase */
    getDobjTokens()
    {
        return dobjInfoCur_ != nil && dobjInfoCur_.np_ != nil
            ? dobjInfoCur_.np_.getOrigTokenList() : nil;
    }

    /* get the original words (as a list of strings) of the current dobj */
    getDobjWords()
    {
        local l = getDobjTokens();
        return l != nil ? l.mapAll({x: getTokOrig(x)}) : nil;
    }

    /* the predicate must assign the direct object production tree here */
    dobjMatch = nil

    /* my resolved list of direct objects */
    dobjList_ = []

    /* 
     *   The resolved direct object on which we're currently executing the
     *   command.  To execute the command, we iterate through the direct
     *   object list, calling the execution sequence for each object in
     *   the list.  We set this to the current object in each iteration.  
     */
    dobjCur_ = nil

    /* the full ResolveInfo associated with dobjCur_ */
    dobjInfoCur_ = nil

    /* my cached direct object resolver */
    dobjResolver_ = nil

    /* -------------------------------------------------------------------- */
    /*
     *   Resolver interface implementation - for the moment, we don't need
     *   any special definitions here, since the basic Resolver
     *   implementation (which we inherit) is suitable for a single-object
     *   action.  
     */

    /* -------------------------------------------------------------------- */
    /*
     *   private Resolver implementation details 
     */

    /*
     *   Initialize me as a resolver.  
     */
    initResolver(issuingActor, targetActor)
    {
        /* remember the actors */
        issuer_ = issuingActor;
        actor_ = targetActor;

        /* I'm the action as well as the resolver */
        action_ = self;

        /* cache the actor's default scope list */
        cacheScopeList();
    }

    /* issuing actor */
    issuer_ = nil

    /* target actor */
    actor_ = nil

    /*
     *   By default, our direct object plays the direct object role in
     *   generated messages.  Subclasses can override this if the resolved
     *   object is to play a different role.  Note that this only affects
     *   generated messages; for parsing purposes, our object is always in
     *   the DirectObject role.  
     */
    whichMessageObject = DirectObject
;


/* ------------------------------------------------------------------------ */
/*
 *   "Tentative" noun resolver results gather.  This type of results
 *   gatherer is used to perform a tentative pre-resolution of an object
 *   of a multi-object action.
 *   
 *   Consider what happens when we resolve a two-object action, such as
 *   "put <dobj> in <iobj>".  Since we have two objects, we obviously must
 *   resolve one object or the other first; but this means that we must
 *   resolve one object with no knowledge of the resolution of the other
 *   object.  This often makes it very difficult to resolve that first
 *   object intelligently, because we'd really like to know something
 *   about the other object.  For example, if we first resolve the iobj of
 *   "put <dobj> in <iobj>", it would be nice to know which dobj we're
 *   talking about, since we could reduce the likelihood that the iobj is
 *   the dobj's present container.
 *   
 *   Tentative resolution addresses this need by giving us some
 *   information about a later-resolved object while resolving an
 *   earlier-resolved object, even though we obviously can't have fully
 *   resolved the later-resolved object.  In tentative resolution, we
 *   perform the resolution of the later-resolved object, completely in
 *   the dark about the earlier-resolved object(s), and come up with as
 *   much information as we can.  The important thing about this stage of
 *   resolution is that we don't ask any interactive questions and we
 *   don't count anything for ranking purposes - we simply do the best we
 *   can and note the results, leaving any ranking or interaction for the
 *   true resolution phase that we'll perform later.  
 */
class TentativeResolveResults: ResolveResults
    construct(target, issuer) { setActors(target, issuer); }

    /* 
     *   ignore most resolution problems, since this is only a tentative
     *   resolution pass 
     */
    noMatch(action, txt) { }
    noMatchPoss(action, txt) { }
    noVocabMatch(action, txt) { }
    noMatchForAll() { }
    noteEmptyBut() { }
    noMatchForAllBut() { }
    noMatchForListBut() { }
    noMatchForPronoun(typ, txt) { }
    reflexiveNotAllowed(typ, txt) { }
    wrongReflexive(typ, txt) { }
    noMatchForPossessive(owner, txt) { }
    noMatchForLocation(loc, txt) { }
    noteBadPrep() { }
    nothingInLocation(loc) { }
    unknownNounPhrase(match, resolver) { return []; }
    noteLiteral(txt) { }
    emptyNounPhrase(resolver) { return []; }
    zeroQuantity(txt) { }
    insufficientQuantity(txt, matchList, requiredNum) { }
    uniqueObjectRequired(txt, matchList) { }
    noteAdjEnding() { }
    noteIndefinite() { }
    noteMiscWordList(txt) { }
    notePronoun() { }
    noteMatches(matchList) { }
    incCommandCount() { }
    noteActorSpecified() { }
    noteNounSlots(cnt) { }
    noteWeakPhrasing(level) { }
    allowActionRemapping = nil

    /* 
     *   during the tentative phase, keep all equivalents - we don't want
     *   to make any arbitrary choices among equivalents during this
     *   phase, because doing so could improperly force a choice among
     *   otherwise ambiguous resolutions to the other phrase 
     */
    allowEquivalentFiltering = nil

    /* 
     *   for ambiguous results, don't attempt to narrow things down - just
     *   keep the entire list 
     */
    ambiguousNounPhrase(keeper, asker, txt,
                        matchList, fullMatchList, scopeList,
                        requiredNum, resolver)
    {
        return matchList;
    }

    /* 
     *   no interaction is allowed, so return nothing if we need to ask
     *   for a missing object 
     */
    askMissingObject(asker, resolver, responseProd)
    {
        /* note that we have a missing noun phrase */
        npMissing = true;

        /* return nothing */
        return nil;
    }

    /* 
     *   no interaction is allowed, so return no tokens if we need to ask
     *   for a literal 
     */
    askMissingLiteral(action, which) { return []; }

    /* no interaction is allowed during tentative resolution */
    canResolveInteractively() { return nil; }

    /* 
     *   flag: the noun phrase we're resolving is a missing noun phrase,
     *   which means that we'll ask for it to be filled in when we get
     *   around to resolving it for real 
     */
    npMissing = nil
;

/*
 *   A dummy object that we use for the *tentative* resolution of a noun
 *   phrase when the noun phrase doesn't match anything.  This lets us
 *   distinguish cases where we have a noun phrase that has an error from a
 *   noun phrase that's simply missing.  
 */
dummyTentativeObject: object
;

dummyTentativeInfo: ResolveInfo
    obj_ = dummyTentativeObject
    flags_ = 0
;

/* ------------------------------------------------------------------------ */
/* 
 *   Transitive-with-indirect Action class - this is an action that takes
 *   both a direct and indirect object.  We subclass the basic one-object
 *   action to add the indirect object handling.  
 */
class TIAction: TAction
    resetAction()
    {
        /* inherit defaulting handling */
        inherited();

        /* discard our cached iobj resolver */
        iobjResolver_ = nil;
    }
    
    /*
     *   Retry a single-object action as a two-object action.  We'll treat
     *   the original action's direct object list as our direct object
     *   list, and obtain an indirect object using the normal means (first
     *   looking for a default, then prompting the player if we can't find
     *   a suitable default).  'orig' is the original single-object action.
     *   
     *   This routine terminates with 'exit' if it doesn't throw some
     *   other error.  
     */
    retryWithMissingIobj(orig, asker)
    {
        /* resolve and execute the replacement action */
        resolveAndReplaceAction(createForMissingIobj(orig, asker));
    }

    /*
     *   Retry an action as a two-object action with an ambiguous indirect
     *   object.  We'll ask which of the given possible objects is
     *   intended.  
     */
    retryWithAmbiguousIobj(orig, objs, asker, objPhrase)
    {
        /* create a missing-indirect-object replacement action */
        local action = createForMissingIobj(orig, asker);

        /* reduce the object list to the objects in scope */
        local resolver = action.getIobjResolver(gIssuingActor, gActor, true);
        objs = objs.subset({x: resolver.objInScope(x)});

        /* plug in the ambiguous indirect object list */
        action.iobjMatch = new PreResolvedAmbigProd(objs, asker, objPhrase);
        
        /* resolve and execute the replacement action */
        resolveAndReplaceAction(action);
    }

    /*
     *   Test to see if askForIobj() would find a default indirect object.
     *   Returns true if there's a default, nil if not.  If this returns
     *   true, then askForIobj() will simply take the default and proceed;
     *   otherwise, it will have to actually ask the user for the missing
     *   information.  
     */
    testRetryDefaultIobj(orig)
    {
        local action;
        local def;
        
        /* create the new action for checking for an indirect object */
        action = createForMissingIobj(orig, ResolveAsker);

        /* get the default iobj */
        def = action.getDefaultIobj(
            action.iobjMatch,
            action.getIobjResolver(gIssuingActor, gActor, nil));

        /* if there's exactly one result, then we have a default */
        return (def != nil && def.length() == 1);
    }

    /*
     *   Create an instance of this action for retrying a given
     *   single-object action with a missing indirect object.  
     */
    createForMissingIobj(orig, asker)
    {
        /* create the new action based on the original action */
        local action = createForRetry(orig);

        /* use an empty noun phrase for the new action's indirect object */
        action.iobjMatch = new EmptyNounPhraseProd();

        /* set our custom response production and ResolveAsker */
        action.iobjMatch.setPrompt(action.askIobjResponseProd, asker);

        /* copy what we've resolved so far */
        action.initForMissingIobj(orig);

        /* return the action */
        return action;
    }

    /*
     *   Initialize the action for retrying with a missing direct object.
     *   
     *   If we're trying a TIAction, we can only be coming from a TAction
     *   (since that's the only kind of original action that can turn into
     *   a two-object, at least in the base library).  That means the
     *   original action already has a direct object.  Now, since we're
     *   asking for a MISSING direct object, the only possibility is that
     *   the original action's direct object is our INDIRECT object.  For
     *   example: SWEEP WITH BROOM is turning into SWEEP <what> WITH
     *   BROOM. 
     */
    initForMissingDobj(orig)
    {
        local origDobj = orig.getDobj();
        
        /* 
         *   Set the indirect object in the new action to the direct object
         *   from the original action.  If there's no individual direct
         *   object yet, we must be retrying the overall command, before we
         *   started iterating through the individual dobjs, so copy the
         *   entire dobj list from the original.  
         */
        iobjMatch = new PreResolvedProd(origDobj != nil
                                        ? origDobj : orig.dobjList_ );
    }

    /*
     *   Initialize the action for retrying with a missing indirect object.
     *   
     *   We can only be coming from a TAction, so the TAction will have a
     *   direct object already.  Simply copy that as our own direct
     *   object.  For example: UNLOCK DOOR is turning into UNLOCK DOOR
     *   WITH <what>.  
     */
    initForMissingIobj(orig)
    {
        local origDobj = orig.getDobj();
        
        /* 
         *   Copy the direct object from the original.  If there's no
         *   individual direct object yet, we must be retrying the overall
         *   command, before we started iterating through the individual
         *   dobjs, so copy the entire dobj list.  
         */
        dobjMatch = new PreResolvedProd(origDobj != nil
                                        ? origDobj : orig.dobjList_);
    }

    /*
     *   The root production to use to parse missing indirect object
     *   responses.  By default, this is singleNoun, but individual
     *   actions can override this as appropriate.
     *   
     *   Note that language modules might want to override this to allow
     *   for special responses.  For example, in English, most verbs will
     *   want to override this with a specialized production that allows
     *   the appropriate preposition in the response.  
     */
    askIobjResponseProd = singleNoun

    /* get the missing object response production for a given role */
    getObjResponseProd(resolver)
    {
        /* if it's the indirect object, return the iobj response prod */
        if (resolver.whichObject == IndirectObject)
            return askIobjResponseProd;

        /* otherwise use the default handling */
        return inherited(resolver);
    }

    /*
     *   Resolution order - returns DirectObject or IndirectObject to
     *   indicate which noun phrase to resolve first in resolveNouns().
     *   By default, we'll resolve the indirect object first, but
     *   individual actions can override this to resolve in a non-default
     *   order.  
     */
    resolveFirst = IndirectObject

    /*
     *   Empty phrase resolution order.  This is similar to the standard
     *   resolution order (resolveFirst), but is used only when both the
     *   direct and indirect objects are empty phrases.
     *   
     *   When both phrases are empty, we will either use a default or
     *   prompt interactively for the missing phrase.  In most cases, it
     *   is desirable to prompt interactively for a missing direct object
     *   first, regardless of the usual resolution order.  
     */
    resolveFirstEmpty = DirectObject
    
    /*
     *   Determine which object to call first for action processing.  By
     *   default, we execute in the same order as we resolve, but this can
     *   be overridden if necessary.  
     */
    execFirst = (resolveFirst)

    /*
     *   Can the indirect object potentially resolve to the given
     *   simulation object?  This only determines if the object is a
     *   *syntactic* match, meaning that it can match at a vocabulary and
     *   grammar level.  This doesn't test it for logicalness or check that
     *   it's an otherwise valid resolution.  
     */
    canIobjResolveTo(obj)
    {
        /* check our iobj match tree to see if it can resolve to 'obj' */
        return iobjMatch.canResolveTo(
            obj, self, issuer_, actor_, IndirectObject);
    }

    /*
     *   resolve our noun phrases to objects 
     */
    resolveNouns(issuingActor, targetActor, results)
    {
        local first;
        local objMatch1, objMatch2;
        local objList1, objList2;
        local getResolver1, getResolver2;
        local objCur1;
        local remapProp;
        local reResolveFirst;

        /* 
         *   note in the results that we have two noun slots (the direct
         *   and indirect objects) 
         */
        results.noteNounSlots(2);

        /* we have no current known direct or indirect object yet */
        dobjCur_ = nil;
        iobjCur_ = nil;

        /* 
         *   presume we won't have to re-resolve the first noun phrase,
         *   and clear out our record of anaphor bindings 
         */
        reResolveFirst = nil;
        needAnaphoricBinding_ = nil;
        lastObjList_ = nil;

        /* 
         *   Determine which object we want to resolve first.  If both
         *   phrases are empty, use the special all-empty ordering;
         *   otherwise, use the standard ordering for this verb.  
         */
        if (dobjMatch.isEmptyPhrase && iobjMatch.isEmptyPhrase)
        {
            /* both phrases are empty - use the all-empty ordering */
            first = resolveFirstEmpty;
        }
        else
        {
            /* 
             *   we have at least one non-empty phrase, so use our
             *   standard ordering 
             */
            first = resolveFirst;
        }

        /*
         *   The resolution process is symmetrical for the two possible
         *   resolution orders (direct object first or indirect object
         *   first); all we need to do is to set up the parameters we'll
         *   need according to which order we're using.
         *   
         *   This parameterized approach makes the code further below a
         *   little mind-boggling, because it's using so much indirection.
         *   But the alternative is worse: the alternative is to
         *   essentially make two copies of the code below (one for the
         *   dobj-first case and one for the iobj-first case), which is
         *   prone to maintenance problems because of the need to keep the
         *   two copies synchronized when making any future changes.  
         */
        if (first == DirectObject)
        {
            objMatch1 = dobjMatch;
            objMatch2 = iobjMatch;
            objList1 = &dobjList_;
            objList2 = &iobjList_;
            getResolver1 = &getDobjResolver;
            getResolver2 = &getIobjResolver;
            objCur1 = &dobjCur_;
            remapProp = remapDobjProp;
        }
        else
        {
            objMatch1 = iobjMatch;
            objMatch2 = dobjMatch;
            objList1 = &iobjList_;
            objList2 = &dobjList_;
            getResolver1 = &getIobjResolver;
            getResolver2 = &getDobjResolver;
            objCur1 = &iobjCur_;
            remapProp = remapIobjProp;
        }

        /* 
         *   Get the unfiltered second-resolved object list - this will
         *   give the first-resolved object resolver access to some
         *   minimal information about the possible second-resolved object
         *   or objects.
         *   
         *   Note that we're not *really* resolving the second object here
         *   - it is the second-resolved object, after all.  What we're
         *   doing is figuring out the *potential* set of objects that
         *   could resolve to the second phrase, with minimal
         *   disambiguation, so that the first object resolver is not
         *   totally in the dark about the second object's potential
         *   resolution.  
         */
        initTentative(issuingActor, targetActor, first);
            
        /* resolve the first-resolved noun phrase */
        self.(objList1) = objMatch1.resolveNouns(
            self.(getResolver1)(issuingActor, targetActor, true), results);

        /* 
         *   If the first-resolved noun phrase asked for an anaphoric
         *   binding, we'll need to go back and re-resolve that noun
         *   phrase after we resolve our other noun phrase, since the
         *   first-resolved phrase refers to the second-resolved phrase. 
         */
        reResolveFirst = needAnaphoricBinding_;

        /* 
         *   if the second-resolved phrase uses an anaphoric pronoun, the
         *   anaphor can be taken as referring to the first-resolved
         *   phrase's results 
         */
        lastObjList_ = self.(objList1);

        /* 
         *   if the first-resolved phrase resolves to just one object, we
         *   can immediately set the current resolved object, so that we
         *   can use it while resolving the second-resolved object list 
         */
        if (self.(objList1).length() == 1)
        {
            /* set the current first-resolved object */
            self.(objCur1) = self.(objList1)[1].obj_;

            /* if remapping is allowed at this point, look for a remapping */
            if (results.allowActionRemapping)
            {
                withParserGlobals(issuingActor, targetActor, self,
                                  function()
                {
                    local remapInfo;
                    
                    /* check for a remapping */
                    if ((remapInfo = self.(objCur1).(remapProp)) != nil)
                    {
                        /* 
                         *   we have a remapping, so apply it - note that
                         *   we're still in the process of resolving noun
                         *   phrases (since we've only resolved one of our
                         *   two noun phrases so far), so pass 'true' for
                         *   the inResolve parameter 
                         */
                        remapAction(true, first, remapInfo);
                    }
                });
            }
        }

        /* resolve the second-resolved object */
        self.(objList2) = objMatch2.resolveNouns(
            self.(getResolver2)(issuingActor, targetActor, true), results);

        /* 
         *   if we have to re-resolve the first noun phrase due to an
         *   anaphoric pronoun, go back and do that now 
         */
        if (reResolveFirst)
        {
            /* 
             *   use the second-resolved noun phrase as the referent of
             *   the anaphoric pronoun(s) in the first-resolved phrase 
             */
            lastObjList_ = self.(objList2);

            /* re-resolve the first object list */
            self.(objList1) = objMatch1.resolveNouns(
                self.(getResolver1)(issuingActor, targetActor, true),
                results);
        }
    }

    /* we have a direct and indirect object */
    predicateNounPhrases = [&dobjMatch, &iobjMatch]

    /* get an object role */
    getRoleFromIndex(idx)
    {
        /* 
         *   the second object is always our indirect object; for other
         *   roles, defer to the inherited behavior 
         */
        return (idx == 2 ? IndirectObject : inherited(idx));
    }

    /* get the resolved object in a given role */
    getObjectForRole(role)
    {
        /* return the indirect object if requested; otherwise inherit */
        return (role == IndirectObject ? getIobj() : inherited(role));
    }

    /* get the ResolveInfo for the given resolved object */
    getResolveInfo(obj, oldRole)
    {
        local info;
        
        /* scan our resolved direct object list for the given object */
        if (dobjList_ != nil)
            info = dobjList_.valWhich({x: x.obj_ == obj});

        /* if we didn't find it there, try the indirect object list */
        if (info == nil && iobjList_ != nil)
            info = iobjList_.valWhich({x: x.obj_ == obj});

        /* if we didn't find one, create one from scratch */
        if (info == nil)
        {
            /* get the list for the old role */
            local lst = (oldRole == DirectObject ? dobjList_ : iobjList_);

            /* 
             *   if there's anything in the old role, copy its flags and
             *   noun phrase to the new role 
             */
            if (lst.length() > 0)
            {
                /* copy the old role's attributes */
                info = new ResolveInfo(obj, lst[1].flags_, lst[1].np_);
            }
            else
            {
                /* nothing in the old role - start from scratch */
                info = new ResolveInfo(obj, 0, nil);
            }
        }

        /* return what we found (or created) */
        return info;
    }

    /* get the OtherObject role for the given role */
    getOtherObjectRole(role)
    {
        /* the complementary roles are DirectObject and IndirectObject */
        return (role == IndirectObject ? DirectObject : IndirectObject);
    }

    /* get the match tree for the noun phrase in the given role */
    getMatchForRole(role)
    {
        /* return the indirect object match tree if requested; else inherit */
        return (role == IndirectObject ? iobjMatch : inherited(role));
    }

    /* get the 'verify' property for a given object role */
    getVerifyPropForRole(role)
    {
        return (role == IndirectObject ? verIobjProp : inherited(role));
    }

    /* get the 'preCond' property for a given object role */
    getPreCondPropForRole(role)
    {
        return (role == IndirectObject ? preCondIobjProp : inherited(role));
    }

    /* get the 'remap' property for a given object role */
    getRemapPropForRole(role)
    {
        return (role == IndirectObject ? remapIobjProp : inherited(role));
    }

    /* get the list of resolved objects in the given role */
    getResolvedObjList(which)
    {
        return (which == IndirectObject ? getResolvedIobjList()
                : inherited(which));
    }

    /* get the list of resolved indirect objects */
    getResolvedIobjList()
    {
        /* 
         *   if we have an indirect object list, return the objects from
         *   it; otherwise, return an empty list 
         */
        return (iobjList_ == nil ? nil : iobjList_.mapAll({x: x.obj_}));
    }

    /*
     *   Manually set the resolved objects.  We'll set our direct and
     *   indirect objects.  
     */
    setResolvedObjects(dobj, iobj)
    {
        /* inherit the base class handling to set the direct object */
        inherited(dobj);

        /* set the resolved iobj */
        setResolvedIobj(iobj);
    }

    /* set a resolved iobj */
    setResolvedIobj(iobj)
    {
        /* build a pre-resolved production for the indirect object */
        iobjMatch = new PreResolvedProd(iobj);

        /* set the resolved indirect object */
        iobjList_ = makeResolveInfoList(iobj);

        /* set the current indirect object as well */
        iobjCur_ = (iobjList_.length() > 0 ? iobjList_[1].obj_ : nil);
        iobjInfoCur_ = (iobjList_.length() > 0 ? iobjList_[1] : nil);
    }

    /* manually set the unresolved object noun phrase match trees */
    setObjectMatches(dobj, iobj)
    {
        /* inherit default handling */
        inherited(dobj);

        /* 
         *   if the iobj is a ResolveInfo, set it as a resolved object;
         *   otherwise set it as an unresolved match tree 
         */
        if (iobj.ofKind(ResolveInfo))
            setResolvedIobj(iobj);
        else
            iobjMatch = iobj;
    }

    /*
     *   Get the anaphoric binding for the noun phrase we're currently
     *   resolving. 
     */
    getAnaphoricBinding(typ)
    {
        /* if we've resolved a prior noun phrase, return it */
        if (lastObjList_ != nil)
            return lastObjList_;

        /* 
         *   we don't have a prior noun phrase - make a note that we have
         *   to come back and re-resolve the current noun phrase 
         */
        needAnaphoricBinding_ = true;

        /* 
         *   return an empty list to indicate that the anaphor is
         *   acceptable in this context but has no binding yet 
         */
        return [];
    }

    /*
     *   The last object list we resolved.  We keep track of this so that
     *   we can provide it as the anaphoric binding, if an anaphor binding
     *   is requested.  
     */
    lastObjList_ = nil

    /*
     *   Flag: we have been asked for an anaphoric binding, but we don't
     *   have a binding available.  We'll check this after resolving the
     *   first-resolved noun phrase so that we can go back and re-resolve
     *   it again after resolving the other noun phrase.  
     */
    needAnaphoricBinding_ = nil

    /* check that the resolved objects are in scope */
    resolvedObjectsInScope()
    {
        /* 
         *   check the indirect object, and inherit the base class handling
         *   to check the direct object 
         */
        return (inherited()
                && (getIobjResolver(gIssuingActor, gActor, true)
                    .objInScope(iobjList_[1].obj_)));
    }

    /* 
     *   get our indirect object resolver, or create one if we haven't
     *   already cached one 
     */
    getIobjResolver(issuingActor, targetActor, reset)
    {
        /* if we don't already have one cached, create a new one */
        if (iobjResolver_ == nil)
            iobjResolver_ = createIobjResolver(issuingActor, targetActor);

        /* reset the resolver if desired */
        if (reset)
            iobjResolver_.resetResolver();
        
        /* return the cached resolver */
        return iobjResolver_;
    }

    /*
     *   Create our indirect object resolver.  By default, we'll use a
     *   basic indirect object resolver.
     */
    createIobjResolver(issuingActor, targetActor)
    {
        /* create and return a new basic indirect object resolver */
        return new IobjResolver(self, issuingActor, targetActor);
    }
    
    /* 
     *   Resolve 'all' for the indirect object.  By default, we'll return
     *   everything in the scope list.  
     */
    getAllIobj(actor, scopeList)
    {
        return scopeList;
    }

    /* filter an ambiguous indirect object noun phrase */
    filterAmbiguousIobj(lst, requiredNum, np)
    {
        /* filter using the indirect object verifier method */
        return filterAmbiguousWithVerify(lst, requiredNum, verIobjProp,
                                         preCondIobjProp, remapIobjProp,
                                         IndirectObject, np);
    }

    /* filter a plural phrase */
    filterPluralIobj(lst, np)
    {
        /* filter using the direct object verifier method */
        return filterPluralWithVerify(lst, verIobjProp, preCondIobjProp,
                                      remapIobjProp, IndirectObject, np);
    }

    /* get the default indirect object */
    getDefaultIobj(np, resolver)
    {
        /* get a default indirect object using the verify method */
        return getDefaultWithVerify(resolver, verIobjProp, preCondIobjProp,
                                    remapIobjProp, IndirectObject, np);
    }

    /*
     *   Execute the action.  We'll run through the execution sequence
     *   once for each resolved object in our direct or indirect object
     *   list, depending on which one is the list and which one is the
     *   singleton.  
     */
    doActionMain()
    {
        local lst;
        local preAnnouncedDobj;
        local preAnnouncedIobj;
        
        /* 
         *   Get the list of resolved objects for the multiple object.  If
         *   neither has multiple objects, it doesn't matter which is
         *   iterated, since we'll just do the command once anyway.  
         */
        lst = (iobjList_.length() > 1 ? iobjList_ : dobjList_);

        /* 
         *   Set the pronoun antecedents, using the game-specific pronoun
         *   setter.  Don't set an antecedent for a nested command.
         */
        if (parentAction == nil)
        {
           /* 
            *   Set both direct and indirect objects as potential
            *   antecedents.  Rather than trying to figure out right now
            *   which one we might want to refer to in the future, remember
            *   both - we'll decide which one is the logical antecedent
            *   when we find a pronoun to resolve in a future command.  
            */ 
           gActor.setPronounMulti(dobjList_, iobjList_); 

            /*
             *   If one or the other object phrase was specified in the
             *   input as a pronoun, keep the meaning of that pronoun the
             *   same, overriding whatever we just did.  Note that the
             *   order we use here doesn't matter: if a given pronoun
             *   appears in only one of the two lists, then the list where
             *   it's not set has no effect on the pronoun, hence it
             *   doesn't matter which comes first; if a pronoun appears in
             *   both lists, it will have the same value in both lists, so
             *   we'll just do the same thing twice, so, again, order
             *   doesn't matter.  
             */
            setPronounByInput(dobjList_);
            setPronounByInput(iobjList_);
        }

        /* 
         *   pre-announce the non-list object if appropriate - this will
         *   provide a common pre-announcement if we iterate through
         *   several announcements of the main list objects 
         */
        if (lst == dobjList_)
        {
            /* pre-announce the single indirect object if needed */
            preAnnouncedIobj = preAnnounceActionObject(
                iobjList_[1], dobjList_, IndirectObject);

            /* we haven't announced the direct object yet */
            preAnnouncedDobj = nil;

            /* pre-calculate the multi-object announcements */
            cacheMultiObjectAnnouncements(dobjList_, DirectObject);
        }
        else
        {
            /* pre-announce the single direct object if needed */
            preAnnouncedDobj = preAnnounceActionObject(
                dobjList_[1], iobjList_, DirectObject);

            /* we haven't announced the indirect object yet */
            preAnnouncedIobj = nil;

            /* pre-calculate the multi-object announcements */
            cacheMultiObjectAnnouncements(iobjList_, IndirectObject);
        }

        /* we haven't yet canceled the iteration */
        iterationCanceled = nil;

        /* iterate over the resolved list for the multiple object */
        for (local i = 1, local len = lst.length() ;
             i <= len && !iterationCanceled ; ++i)
        {
            local dobjInfo;
            local iobjInfo;

            /* 
             *   make the current list item the direct or indirect object,
             *   as appropriate 
             */
            if (lst == dobjList_)
            {
                /* the direct object is the multiple object */
                dobjInfo = dobjInfoCur_ = lst[i];
                iobjInfo = iobjInfoCur_ = iobjList_[1];
            }
            else
            {
                /* the indirect object is the multiple object */
                dobjInfo = dobjInfoCur_ = dobjList_[1];
                iobjInfo = iobjInfoCur_ = lst[i];
            }

            /* get the current dobj and iobj from the resolve info */
            dobjCur_ = dobjInfo.obj_;
            iobjCur_ = iobjInfo.obj_;

            /* 
             *   if the action was remapped, and we need to announce
             *   anything, announce the entire action 
             */
            if (isRemapped())
            {
                /*
                 *   We were remapped.  The entire phrasing of the new
                 *   action might have changed from what the player typed,
                 *   so it might be nonsensical to show the objects as we
                 *   usually would, as sentence fragments that are meant
                 *   to combine with what the player actually typed.  So,
                 *   instead of showing the usual sentence fragments, show
                 *   the entire phrasing of the command.
                 *   
                 *   Only show the announcement if we have a reason to: we
                 *   have unclear disambiguation in one of the objects, or
                 *   one of the objects is defaulted.
                 *   
                 *   If we don't want to announce the remapped action,
                 *   still consider showing a multi-object announcement,
                 *   if we would normally need to do so.  
                 */
                if (needRemappedAnnouncement(dobjInfo)
                    || needRemappedAnnouncement(iobjInfo))
                {
                    /* show the remapped announcement */
                    gTranscript.announceRemappedAction();
                }
                else
                {
                    /* announce the multiple dobj if necessary */
                    if (!preAnnouncedDobj)
                        maybeAnnounceMultiObject(
                            dobjInfo, dobjList_.length(), DirectObject);

                    /* announce the multiple iobj if necessary */
                    if (!preAnnouncedIobj)
                        maybeAnnounceMultiObject(
                            iobjInfo, iobjList_.length(), IndirectObject);
                }
            }
            else
            {
                /* announce the direct object if appropriate */
                if (!preAnnouncedDobj)
                    announceActionObject(dobjInfo, dobjList_.length(),
                                         DirectObject);

                /* announce the indirect object if appropriate */
                if (!preAnnouncedIobj)
                    announceActionObject(iobjInfo, iobjList_.length(),
                                         IndirectObject);
            }

            /* run the execution sequence for the current direct object */
            doActionOnce();

            /* if we're top-level, count the iteration in the transcript */
            if (parentAction == nil)
                gTranscript.newIter();
        }
    }

    /*
     *   Set the pronoun according to the pronoun type actually used in
     *   the input.  For example, if we said PUT BOX ON IT, we want IT to
     *   continue referring to whatever IT referred to before this command
     *   - we specifically do NOT want IT to refer to the BOX in this
     *   case. 
     */
    setPronounByInput(lst)
    {
        local objs;
        
        /* get the subset of the list that was specified by pronoun */
        lst = lst.subset({x: x.pronounType_ != nil});

        /* 
         *   Get a list of the unique objects in the list.  This will
         *   ensure that we can distinguish THEM from IT AND IT AND IT: if
         *   we have the same pronoun appearing with multiple objects,
         *   we'll know that it's because the pronoun was actually plural
         *   (THEM) rather than a singular pronoun that was repeated (IT
         *   AND IT AND IT). 
         */
        objs = lst.mapAll({x: x.obj_}).getUnique();

        /* 
         *   Now retain one 'lst' element for each 'objs' element.  This
         *   is a bit tricky: we're mapping each element in 'objs' to pick
         *   out one element of 'lst' where the 'lst' element points to
         *   the 'objs' element.  
         */
        lst = objs.mapAll({o: lst.valWhich({l: l.obj_ == o})});

        /* 
         *   Now we can set the pronouns.  Go through the list, and set
         *   each different pronoun type that appears.  Set it to the
         *   subset of the list with that matches that pronoun type, and
         *   then remove that subset and keep iterating to pick up the
         *   remaining pronoun types.  
         */
        while (lst.length() != 0)
        {
            local cur;
            local typ;

            /* on this iteration, handle the first pronoun type in the list */
            typ = lst[1].pronounType_;
            
            /* pick out the subset with the current pronoun type */
            cur = lst.subset({x: x.pronounType_ == typ});

            /* set the current pronoun to the current list */
            gActor.setPronounByType(typ, cur);

            /* remove the subset we just handled from the remaining list */
            lst -= cur;
        }
    }

    /*
     *   Determine if we need to announce this action when the action was
     *   remapped, based on the resolution information for one of our
     *   objects.  We need to announce a remapped action when either
     *   object had unclear disambiguation or was defaulted. 
     */
    needRemappedAnnouncement(info)
    {
        /* 
         *   if it's a defaulted object that hasn't been announced, or it
         *   was unclearly disambiguated, we need an announcement 
         */
        return (((info.flags_ & DefaultObject) != 0
                 && (info.flags_ & AnnouncedDefaultObject) == 0)
                || (info.flags_ & UnclearDisambig) != 0);
    }

    /*
     *   Verify the action.
     */
    verifyAction()
    {
        local result;
        
        /* invoke any general (non-object) pre-condition verifiers */
        result = callVerifyPreCond(nil);

        /* check the direct object */
        result = callVerifyProp(dobjCur_, verDobjProp, preCondDobjProp,
                                remapDobjProp, result, DirectObject);

        /* 
         *   Check the indirect object, combining the results with the
         *   direct object results.  We combine the results for the two
         *   objects because we're simply looking for any reason that we
         *   can't perform the command.  
         */
        result = callVerifyProp(iobjCur_, verIobjProp, preCondIobjProp,
                                remapIobjProp, result, IndirectObject);

        /* 
         *   check that the action method ("doXxx") is defined on at least
         *   one of the objects 
         */
        return verifyHandlersExist(
            [dobjCur_, iobjCur_],
            [verDobjProp, checkDobjProp, actionDobjProp,
            verIobjProp, checkIobjProp, actionIobjProp],
            result);
    }

    /* initialize tentative resolutions for other noun phrases */
    initTentative(issuingActor, targetActor, whichObj)
    {
        local tRes;
        local ti, td;

        /* 
         *   remember the old tentative direct and indirect objects, then
         *   set them to empty lists - this will ensure that we don't
         *   recursively try to get a tentative resolution for the object
         *   we're working on right now, which would cause infinite
         *   recursion 
         */
        td = tentativeDobj_;
        ti = tentativeIobj_;

        /* set them to empty lists to indicate they don't need resolving */
        tentativeDobj_ = [];
        tentativeIobj_ = [];

        /* make sure our built-in dobj resolver is initialized */
        issuer_ = issuingActor;
        actor_ = targetActor;

        /* make sure we set things back when we're done */
        try
        {
            /* initialize the other noun phrase */
            if (whichObj == DirectObject && ti == nil)
            {
                /* tentatively resolve the indirect object */
                tRes = new TentativeResolveResults(targetActor, issuingActor);
                ti = iobjMatch.resolveNouns(
                    getIobjResolver(issuingActor, targetActor, true), tRes);

                /* 
                 *   if the list is empty, and we didn't have a missing
                 *   noun phrase, use a dummy object as the tentative
                 *   resolution - this distinguishes erroneous noun phrases
                 *   from those that are simply missing 
                 */
                if (ti == [] && !tRes.npMissing)
                    ti = [dummyTentativeInfo];
            }
            else if (whichObj == IndirectObject && td == nil)
            {
                /* tentatively resolve the direct object */
                tRes = new TentativeResolveResults(targetActor, issuingActor);
                td = dobjMatch.resolveNouns(
                    getDobjResolver(issuingActor, targetActor, true), tRes);

                /* use a dummy object if appropriate */
                if (td == [] && !tRes.npMissing)
                    td = [dummyTentativeInfo];
            }
        }
        finally
        {
            /* set the original (or updated) tentative lists */
            tentativeDobj_ = td;
            tentativeIobj_ = ti;
        }
    }

    /*
     *   Check for remapping 
     */
    checkRemapping()
    {
        local remapInfo;
        local role;

        /* presume we'll find remapping for the first-resolved object */
        role = resolveFirst;
        
        /* check remapping for each object, in the resolution order */
        if (resolveFirst == DirectObject)
        {
            /* the direct object is resolved first, so try it first */
            if ((remapInfo = dobjCur_.(remapDobjProp)) == nil)
            {
                remapInfo = iobjCur_.(remapIobjProp);
                role = IndirectObject;
            }
        }
        else
        {
            /* the indirect object is resolved first, so remap it first */
            if ((remapInfo = iobjCur_.(remapIobjProp)) == nil)
            {
                remapInfo = dobjCur_.(remapDobjProp);
                role = DirectObject;
            }
        }

        /* if we found a remapping, apply it */
        if (remapInfo != nil)
            remapAction(nil, role, remapInfo);
    }

    /*
     *   Check the command.
     *   
     *   For a two-object action, this first calls the catch-all 'check'
     *   methods (the dobjFor(Default) and dobjFor(All) methods) on the two
     *   objects (indirect object first), then calls the 'check' methods
     *   for this specific action (direct object first).  
     */
    checkAction()
    {
        try
        {
            /* invoke the catch-all 'check' methods on each object */
            local defIo = callCatchAllProp(
                iobjCur_, checkIobjProp, &checkIobjDefault, &checkIobjAll);
            local defDo = callCatchAllProp(
                dobjCur_, checkDobjProp, &checkDobjDefault, &checkDobjAll);

            /* 
             *   invoke the 'check' method on each object, as long as it
             *   overrides any corresponding Default 'check' handler 
             */
            if (!defDo)
                dobjCur_.(checkDobjProp)();
            if (!defIo)
                iobjCur_.(checkIobjProp)();
        }
        catch (ExitSignal es)
        {
            /* mark the action as a failure in the transcript */
            gTranscript.noteFailure();

            /* re-throw the signal */
            throw es;
        }
    }

    /*
     *   Execute the command. 
     */
    execAction()
    {
        local defIo, defDo;
                
        /* invoke the catch-all 'action' method on each object */
        defIo = callCatchAllProp(iobjCur_, actionIobjProp,
                                 &actionIobjDefault, &actionIobjAll);
        defDo = callCatchAllProp(dobjCur_, actionDobjProp,
                                 &actionDobjDefault, &actionDobjAll);

        /* 
         *   Invoke the action method on each object, starting with the
         *   non-list object.  Call these only if the corresponding
         *   default 'action' methods didn't override the verb-specific
         *   methods.  
         */
        if (execFirst == DirectObject)
        {
            if (!defDo)
                dobjCur_.(actionDobjProp)();
            if (!defIo)
                iobjCur_.(actionIobjProp)();
        }
        else
        {
            if (!defIo)
                iobjCur_.(actionIobjProp)();
            if (!defDo)
                dobjCur_.(actionDobjProp)();
        }
    }

    /* get the precondition descriptor list for the action */
    getPreCondDescList()
    {
        /* 
         *   return the inherited preconditions plus the conditions that
         *   apply to the indirect object 
         */
        return inherited()
            + getObjPreCondDescList(iobjCur_, preCondIobjProp, iobjCur_,
                                    IndirectObject);
    }

    /*
     *   Get a message parameter object for the action.  We define 'dobj'
     *   as the direct object and 'iobj' as the indirect object, in
     *   addition to any inherited targets.  
     */
    getMessageParam(objName)
    {
        switch(objName)
        {
        case 'iobj':
            /* return the current indirect object */
            return iobjCur_;

        default:
            /* inherit default handling */
            return inherited(objName);
        }
    }

    /* get the current indirect object being executed */
    getIobj() { return iobjCur_; }
    
    /* get the full ResolveInfo associated with the current indirect object */
    getIobjInfo() { return iobjInfoCur_; }

    /* get the object resolution flags for the indirect object */
    getIobjFlags() { return iobjInfoCur_ != nil ? iobjInfoCur_.flags_ : 0; }

    /* get the number of direct objects */
    getIobjCount() { return iobjList_ != nil ? iobjList_.length() : 0; }

    /* get the original token list of the current indirect object phrase */
    getIobjTokens()
    {
        return iobjInfoCur_ != nil && iobjInfoCur_.np_ != nil
            ? iobjInfoCur_.np_.getOrigTokenList() : nil;
    }

    /* get the original words (as a list of strings) of the current iobj */
    getIobjWords()
    {
        local l = getIobjTokens();
        return l != nil ? l.mapAll({x: getTokOrig(x)}) : nil;
    }

    /*
     *   Get the list of active objects.  We have a direct and indirect
     *   object.  
     */
    getCurrentObjects()
    {
        return [dobjCur_, iobjCur_];
    }

    /* set the current objects */
    setCurrentObjects(lst)
    {
        /* remember the current direct and indirect objects */
        dobjCur_ = lst[1];
        iobjCur_ = lst[2];

        /* we don't have ResolveInfo objects for this call */
        dobjInfoCur_ = iobjInfoCur_ = nil;
    }

    /*
     *   Copy one tentative object list to the other.  This is useful when
     *   an object's verifier for one TIAction wants to forward the call
     *   to the other object verifier for a different TIAction.  
     */
    copyTentativeObjs()
    {
        /* copy whichever tentative list is populated to the other slot */
        if (tentativeDobj_ != nil)
            tentativeDobj_ = tentativeIobj_;
        else
            tentativeIobj_ = tentativeDobj_;
    }

    /*
     *   Get the tentative direct/indirect object resolution lists.  A
     *   tentative list is available for the later-resolved object while
     *   resolving the earlier-resolved object. 
     */
    getTentativeDobj() { return tentativeDobj_; }
    getTentativeIobj() { return tentativeIobj_; }

    /* 
     *   the predicate grammar must assign the indirect object production
     *   tree to iobjMatch 
     */
    iobjMatch = nil

    /* the indirect object list */
    iobjList_ = []

    /* current indirect object being executed */
    iobjCur_ = nil

    /* the full ResolveInfo associated with iobjCur_ */
    iobjInfoCur_ = nil

    /* my cached indirect object resolver */
    iobjResolver_ = nil

    /*
     *   The tentative direct and indirect object lists.  A tentative list
     *   is available for the later-resolved object while resolving the
     *   earlier-resolved object.  
     */
    tentativeDobj_ = nil
    tentativeIobj_ = nil

    /*
     *   Verification and action properties for the indirect object.  By
     *   convention, the verification method for the indirect object of a
     *   two-object action is verIobjXxx; the check method is
     *   checkIobjXxx; and the action method is actionIobjXxx.  
     */
    verIobjProp = nil
    preCondIobjProp = nil
    checkIobjProp = nil
    actionIobjProp = nil

    /*
     *   Action-remap properties for the indirect object.  By convention,
     *   the remapper properties are named remapDobjAction and
     *   remapIobjAction, for the direct and indirect objects,
     *   respectively, where Action is replaced by the root name of the
     *   action.  
     */
    remapIobjProp = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Common base class for actions involving literal phrases.  This is a
 *   mix-in class that can be combined with Action subclasses to create
 *   specific kinds of literal actions.  
 */
class LiteralActionBase: object
    /* 
     *   Get a message parameter.  We define 'literal' as the text of the
     *   literal phrase, in addition to inherited targets.  
     */
    getMessageParam(objName)
    {
        switch(objName)
        {
        case 'literal':
            /* return the text of the literal phrase */
            return text_;

        default:
            /* inherit default handling */
            return inherited(objName);
        }
    }

    /* manually set the resolved objects */
    setResolvedObjects(txt)
    {
        /* remember the literal text */
        text_ = txt;
    }

    /* manually set the pre-resolved match trees */
    setObjectMatches(lit)
    {
        /* if it's not already a PreResolvedLiteralProd, wrap it */
        if (!lit.ofKind(PreResolvedLiteralProd))
        {
            /* save the literal text */
            text_ = lit;
            
            /* wrap it in a match tree */
            lit = new PreResolvedLiteralProd(lit);
        }
        
        /* note the new literal match tree */
        literalMatch = lit;
    }

    /* get the current literal text */
    getLiteral() { return text_; }

    /* the text of the literal phrase */
    text_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   An action with a literal phrase as its only object, such as "say <any
 *   text>".  We'll accept anything as the literal phrase - a number, a
 *   quoted string, or arbitrary words - and treat them all simply as text.
 *   
 *   The grammar rules that produce these actions must set literalMatch to
 *   the literal phrase's match tree.
 *   
 *   Because we don't have any actual resolved objects, we're based on
 *   IAction.  Subclasses that implement particular literal actions should
 *   override execAction() to carry out the action; this method can call
 *   the getLiteral() method of self to get a string giving the literal
 *   text.  
 */
class LiteralAction: LiteralActionBase, IAction
    /*
     *   Resolve objects.  We don't actually have any objects to resolve,
     *   but we do have to get the text for the literal phrase.  
     */
    resolveNouns(issuingActor, targetActor, results)
    {
        /* the literal phrase counts as one noun slot */
        results.noteNounSlots(1);
        
        /* 
         *   "Resolve" our literal phrase.  The literal phrase doesn't
         *   resolve to an object list the way a regular noun phrase would,
         *   but rather just resolves to a text string giving the original
         *   literal contents of the phrase.  
         */
        literalMatch.resolveLiteral(results);

        /* retrieve the text of the phrase, exactly as the player typed it */
        text_ = literalMatch.getLiteralText(results, self, DirectObject);
    }

    /* we have a literal phrase as our only noun phrase */
    predicateNounPhrases = [&literalMatch]
;

/* ------------------------------------------------------------------------ */
/*
 *   An action with a direct object and a literal, such as "turn dial to
 *   <setting>" or "type <string> on keypad".  We'll accept anything as the
 *   literal phrase - a number, a quoted string, or arbitrary words - and
 *   treat them all simply as text.
 *   
 *   The grammar rules that produce these actions must set dobjMatch to the
 *   resolvable object of the command, and must set literalMatch to the
 *   literal phrase's match tree.  Note that we use dobjMatch as the
 *   resolvable object even if the object serves grammatically as the
 *   indirect object - this is a simplification, and the true grammatical
 *   purpose of the object isn't important since there's only one true
 *   object in the command.
 *   
 *   When referring to objects by role (such as in remapTo), callers should
 *   ALWAYS refer to the resolvable object as DirectObject, and the literal
 *   phrase as IndirectObject.
 *   
 *   Each subclass must set the property whichMessageLiteral to the
 *   grammatical role (DirectObject, IndirectObject) the literal phrase
 *   plays for message generation purposes.  This only affects messages; it
 *   doesn't affect anything else; in particular, regardless of the
 *   whichMessageLiteral setting, callers should always refer to the
 *   literal as IndirectObject when calling getObjectForRole() and the
 *   like, and should always call getDobj() to get the resolved version of
 *   the resolvable object phrase.  
 */
class LiteralTAction: LiteralActionBase, TAction
    /*
     *   Resolve objects.  
     */
    resolveNouns(issuingActor, targetActor, results)
    {
        /* 
         *   the literal phrase counts as one noun slot, and the direct
         *   object as another 
         */
        results.noteNounSlots(2);

        /* 
         *   If the literal phrase serves as the direct object in generated
         *   messages, ask for its literal text first, so that we ask for
         *   it interactively first.  Otherwise, get the tentative literal
         *   text, in case it's useful to have in resolving the other
         *   object.  
         */
        if (whichMessageLiteral == DirectObject)
            text_ = literalMatch.getLiteralText(results, self, DirectObject);
        else
            text_ = literalMatch.getTentativeLiteralText();

        /* resolve the direct object */
        dobjList_ = dobjMatch.resolveNouns(
            getDobjResolver(issuingActor, targetActor, true), results);

        /* 
         *   "Resolve" the literal, ignoring the result - we call this so
         *   that the literal can do any scoring it wants to do during
         *   resolution; but since we're treating it literally, it
         *   obviously has no actual resolved value.  
         */
        literalMatch.resolveLiteral(results);

        /* the literal phrase resolves to the text only */
        text_ = literalMatch.getLiteralText(results, self,
                                            whichMessageLiteral);
    }

    /* we have a direct object and a literal phrase */
    predicateNounPhrases = [&dobjMatch, &literalMatch]

    /* get an object role */
    getRoleFromIndex(idx)
    {
        /* 
         *   index 2 is the literal object, which is always in the indirect
         *   object role; for others, inherit the default handling 
         */
        return (idx == 2 ? IndirectObject : inherited(idx));
    }

    /* get the OtherObject role for the given role */
    getOtherObjectRole(role)
    {
        /* the complementary roles are DirectObject and IndirectObject */
        return (role == DirectObject ? IndirectObject : DirectObject);
    }

    /* get the resolved object in a given role */
    getObjectForRole(role)
    {
        /* 
         *   the literal is in the IndirectObject role; inherit the default
         *   for others 
         */
        return (role == IndirectObject ? text_ : inherited(role));
    }

    /* get the match tree for the given role */
    getMatchForRole(role)
    {
        /* 
         *   the literal is in the IndirectObject role; inherit the default
         *   for anything else 
         */
        return (role == IndirectObject ? literalMatch : inherited(role));
    }

    /* manually set the resolved objects */
    setResolvedObjects(dobj, txt)
    {
        /* inherit default TAction handling for the direct object */
        inherited TAction(dobj);

        /* inherit the LiteralActionBase handling for the literal text */
        inherited LiteralActionBase(txt);
    }

    /* manually set the pre-resolved match trees */
    setObjectMatches(dobj, lit)
    {
        /* inherit default TAction handling for the direct object */
        inherited TAction(dobj);

        /* inherit the default LiteralActionBase handling for the literal */
        inherited LiteralActionBase(lit);
    }

    /* 
     *   Get a list of the current objects.  We include only the direct
     *   object here, since the literal text is not a resolved object but
     *   simply literal text. 
     */
    getCurrentObjects() { return [dobjCur_]; }

    /* set the current objects */
    setCurrentObjects(lst)
    {
        dobjCur_ = lst[1];
        dobjInfoCur_ = nil;
    }

    /*
     *   Retry a single-object action as an action taking both an object
     *   and a literal phrase.  We'll treat the original action's direct
     *   object list as our direct object list, and obtain a literal
     *   phrase interactively.
     *   
     *   This routine terminates with 'exit' if it doesn't throw some
     *   other error.  
     */
    retryWithMissingLiteral(orig)
    {
        local action;
        
        /* create the new action based on the original action */
        action = createForRetry(orig);

        /* use an empty literal phrase for the new action */
        action.literalMatch = new EmptyLiteralPhraseProd();

        /* initialize for the missing literal phrase */
        action.initForMissingLiteral(orig);

        /* resolve and execute the replacement action */
        resolveAndReplaceAction(action);
    }

    /* initialize with a missing direct object phrase */
    initForMissingDobj(orig)
    {
        /*
         *   Since we have a missing direct objet, we must be coming from
         *   a LiteralAction.  The LiteralAction will already have a
         *   literal phrase, so copy it.  For example: WRITE HELLO ->
         *   WRITE HELLO ON <what>.  
         */
        literalMatch = new PreResolvedLiteralProd(orig.getLiteral());
    }

    /* initialize for a missing literal phrase */
    initForMissingLiteral(orig)
    {
        local origDobj = orig.getDobj();
        
        /*
         *   Since we have a missing literal, we must be coming from a
         *   TAction.  The TAction will already have a direct object,
         *   which we'll want to keep as our own direct object.  For
         *   example: WRITE ON SLATE -> WRITE <what> ON SLATE.  
         */
        dobjMatch = new PreResolvedProd(origDobj != nil
                                        ? origDobj : orig.dobjList_);
    }

    /* object role played by the literal phrase */
    whichMessageLiteral = nil


    /* -------------------------------------------------------------------- */
    /*
     *   Direct Object Resolver implementation.  We serve as our own
     *   direct object resolver, so we define any special resolver
     *   behavior here.  
     */

    /* 
     *   the true grammatical role of the resolved object is always the
     *   direct object 
     */
    whichObject = DirectObject

    /* 
     *   What we call our direct object might actually be playing the
     *   grammatical role of the indirect object - in order to inherit
     *   easily from TAction, we call our resolved object our direct
     *   object, regardless of which grammatical role it actually plays.
     *   For the most part it doesn't matter which is which; but for the
     *   purposes of our resolver, we actually do care about its real role.
     *   So, override the resolver method whichMessageObject so that it
     *   returns whichever role is NOT served by the topic object.  
     */
    whichMessageObject =
        (whichMessageLiteral == DirectObject ? IndirectObject : DirectObject)
;

/* ------------------------------------------------------------------------ */
/*
 *   Base class for actions that include a "topic" phrase.  This is a
 *   mix-in class that can be used in different types of topic actions.  In
 *   all cases, the topic phrase must be assigned to the 'topicMatch'
 *   property in grammar rules based on this class.  
 */
class TopicActionBase: object
    /*
     *   Resolve the topic phrase.  This resolves the match tree in out
     *   'topicMatch' property, and stores the result in topicList_.  This
     *   is for use in resolveNouns().  
     */
    resolveTopic(issuingActor, targetActor, results)
    {
        /* get the topic resolver */
        local resolver = getTopicResolver(issuingActor, targetActor, true);
        
        /* resolve the topic match tree */
        topicList_ = topicMatch.resolveNouns(resolver, results);

        /* make sure it's properly packaged as a ResolvedTopic */
        topicList_ = resolver.packageTopicList(topicList_, topicMatch);
    }

    /*
     *   Set the resolved topic to the given object list.  This is for use
     *   in setResolvedObjects().  
     */
    setResolvedTopic(topic)
    {
        /* if the topic isn't given as a ResolvedTopic, wrap it in one */
        if (!topic.ofKind(ResolvedTopic))
            topic = ResolvedTopic.wrapObject(topic);

        /* use the match object from the ResolvedTopic */
        topicMatch = topic.topicProd;

        /* finally, make a ResolveInfo list out of the ResolvedTopic */
        topicList_ = makeResolveInfoList(topic);
    }

    /*
     *   Set the topic match tree.  This is for use in setObjectMatches().
     */
    setTopicMatch(topic)
    {
        /* 
         *   If it's given as a ResolveInfo, wrap it in a PreResolvedProd.
         *   Otherwise, if it's not a topic match (a TopicProd object), and
         *   it has tokens, re-parse it as a topic match to make sure we
         *   get the grammar optimized for global scope.  
         */
        if (topic.ofKind(ResolveInfo))
        {
            /* it's a resolved object, so wrap it in a fake match tree */
            topic = new PreResolvedProd(topic);
        }
        else if (!topic.ofKind(TopicProd))
        {
            /* 
             *   Re-parse it as a topic phrase.  If our dobj resolver knows
             *   the actors, use those; otherwise default to the player
             *   character. 
             */
            local issuer = (issuer_ != nil ? issuer_ : gPlayerChar);
            local target = (actor_ != nil ? actor_ : gPlayerChar);
            topic = reparseMatchAsTopic(topic, issuer, target);
        }

        /* save the topic match */
        topicMatch = topic;
    }

    /*
     *   Re-parse a match tree as a topic phrase.  Returns a TopicProd
     *   match tree, if possible.  
     */
    reparseMatchAsTopic(topic, issuingActor, targetActor)
    {
        local toks;
        
        /* we can only proceed if the match tree has a token list */
        if ((toks = topic.getOrigTokenList()) != nil && toks.length() > 0)
        {
            /* parse it as a topic phrase */
            local match = topicPhrase.parseTokens(toks, cmdDict);
        
            /* 
             *   if we parsed it successfully, and we have more than one
             *   match, get the best of the bunch 
             */
            if (match != nil && match.length() > 1)
            {
                /* set up a topic resolver for the match */
                local resolver = getTopicResolver(
                    issuingActor, targetActor, true);
                
                /* sort it and return the best match */
                return CommandRanking.sortByRanking(match, resolver)[1].match;
            }
            else if (match != nil && match.length() > 0)
            {
                /* we found exactly one match, so use it */
                return match[1];
            }
        }

        /* 
         *   if we make it this far, we couldn't parse it as a topic, so
         *   just return the original 
         */
        return topic;
    }

    /*
     *   get the topic resolver 
     */
    getTopicResolver(issuingActor, targetActor, reset)
    {
        /* create one if we don't already have one */
        if (topicResolver_ == nil)
            topicResolver_ = createTopicResolver(issuingActor, targetActor);

        /* reset the resolver if desired */
        if (reset)
            topicResolver_.resetResolver();

        /* return the cached resolver */
        return topicResolver_;
    }

    /*
     *   Create the topic resolver.  
     */
    createTopicResolver(issuingActor, targetActor)
    {
        return new TopicResolver(
            self, issuingActor, targetActor, topicMatch, whichMessageTopic,
            getTopicQualifierResolver(issuingActor, targetActor, nil));
    }

    /*
     *   Get a message parameter by name.  We'll return the topic for the
     *   keyword 'topic', and inherit the default handling for anything
     *   else.  
     */
    getMessageParam(objName)
    {
        switch(objName)
        {
        case 'topic':
            /* return the topic */
            return getTopic();

        default:
            /* inherit default handling */
            return inherited(objName);
        }
    }

    /* get the current topic */
    getTopic()
    {
        /* 
         *   Because the topic list always contains one entry (a
         *   ResolvedTopic object encapsulating the topic information),
         *   our current topic is always simply the first and only element
         *   of the topic list. 
         */
        return topicList_[1].obj_;
    }


    /*
     *   Get the resolver for qualifiers we find in the topic phrase
     *   (qualifiers that might need resolution include things like
     *   possessive adjectives and locational phrases).  
     */
    getTopicQualifierResolver(issuingActor, targetActor, reset)
    {
        /* if we don't already have a qualifier resolver, create one */
        if (topicQualResolver_ == nil)
            topicQualResolver_ = createTopicQualifierResolver(
                issuingActor, targetActor);

        /* reset it if desired */
        if (reset)
            topicQualResolver_.resetResolver();

        /* return it */
        return topicQualResolver_;
    }

    /* the topic qualifier resolver */
    topicQualResolver_ = nil
    
    /* the resolved topic object list */
    topicList_ = nil

    /* my cached topic resolver */
    topicResolver_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   An action with a topic phrase as its only object, such as "think about
 *   <topic>".  
 *   
 *   The grammar rules that produce these actions must set topicMatch to
 *   the topic match tree.
 *   
 *   Because we don't have any actual resolved objects, we're based on
 *   IAction.  Subclasses that implement particular topic actions should
 *   override execAction() to carry out the action; this method can call
 *   the getTopic() method of self to get a string giving the topic.  
 */
class TopicAction: TopicActionBase, IAction
    /*
     *   Resolve objects.  We don't actually have any normal objects to
     *   resolve, but we do have to get the resolved topic phrase.  
     */
    resolveNouns(issuingActor, targetActor, results)
    {
        /* the topic phrase counts as one noun slot */
        results.noteNounSlots(1);

        /* resolve the topic */
        resolveTopic(issuingActor, targetActor, results);
    }

    /* manually set the resolved objects */
    setResolvedObjects(topic)
    {
        /* set the resolved topic */
        setResolvedTopic(topic);
    }

    /* manually set the pre-resolved match trees */
    setObjectMatches(topic)
    {
        /* note the new topic match tree */
        setTopicMatch(topic);
    }

    /* we have a topic noun phrase */
    predicateNounPhrases = [&topicMatch]
;


/* ------------------------------------------------------------------------ */
/*
 *   An Action with a direct object and a topic, such as "ask <actor> about
 *   <topic>".  Topics differ from ordinary noun phrases in scope: rather
 *   than resolving to simulation objects based on location, we resolve
 *   these based on the actor's knowledge.
 *   
 *   The grammar rules that produce these actions must set dobjMatch to the
 *   resolvable object of the command (the <actor> in "ask <actor> about
 *   <topic>"), and must set topicMatch to the topic match tree object,
 *   which must be a TopicProd object.  Note that, in some cases, the
 *   phrasing might make the dobjMatch the indirect object, grammatically
 *   speaking: "type <topic> on <object>"; even in such cases, use
 *   dobjMatch for the resolvable object.
 *   
 *   When we resolve the topic, we will always resolve it to a single
 *   object of class ResolvedTopic.  This contains the literal tokens of
 *   the original command plus a list of simulation objects matching the
 *   topic name, ordered from best to worst.  This is different from the
 *   way most commands work, since we do not resolve the topic to a simple
 *   game world object.  We keep all of this extra information because we
 *   don't want to perform disambiguation in the normal fashion, but
 *   instead resolve as much as we can with what we're given, and then give
 *   the specialized action code as much information as we can to let the
 *   action code figure out how to respond to the topic.  
 */
class TopicTAction: TopicActionBase, TAction
    /* 
     *   reset the action 
     */
    resetAction()
    {
        /* reset the inherited state */
        inherited();

        /* forget our topic resolver */
        topicResolver_ = nil;
    }
    
    /*
     *   resolve our noun phrases to objects 
     */
    resolveNouns(issuingActor, targetActor, results)
    {
        local dobjRes;
        
        /* 
         *   the topic phrase counts as one noun slot, and the direct
         *   object as another 
         */
        results.noteNounSlots(2);

        /* resolve the direct object, if we have one */
        if (dobjMatch != nil)
        {
            /* presume we won't find an unbound anaphor */
            needAnaphoricBinding_ = nil;
            
            /* 
             *   if the direct object phrase is an empty phrase, resolve
             *   the topic first 
             */
            if (dobjMatch.isEmptyPhrase)
                resolveTopic(issuingActor, targetActor, results);
            
            /* get the direct object resolver */
            dobjRes = getDobjResolver(issuingActor, targetActor, true);
            
            /* resolve the direct object */
            dobjList_ = dobjMatch.resolveNouns(dobjRes, results);

            /* 
             *   if that turned up an anaphor (LOOK UP BOOK IN ITSELF),
             *   resolve the topic phrase as though it were the direct
             *   object 
             */
            if (needAnaphoricBinding_ && topicMatch != nil)
                dobjList_ = topicMatch.resolveNouns(dobjRes, results);
        }

        /* resolve the topic */
        resolveTopic(issuingActor, targetActor, results);
    }

    /*
     *   Filter the resolved topic.  This is called by our
     *   TActionTopicResolver, which refers the resolution back to us.  
     */
    filterTopic(lst, np, resolver)
    {
        /* by default, simply put everything in an undifferentiated list */
        return new ResolvedTopic(lst, [], [], topicMatch);
    }

    /*
     *   Retry a single-object action as an action taking both an object
     *   and a topic phrase.  We'll treat the original action's direct
     *   object list as our direct object list, and we'll obtain a topic
     *   phrase interactively.
     *   
     *   This routine terminates with 'exit' if it doesn't throw some other
     *   error.  
     */
    retryWithMissingTopic(orig)
    {
        local action;
        
        /* create the new action based on the original action */
        action = createForRetry(orig);

        /* use an empty topic phrase for the new action */
        action.topicMatch = new EmptyTopicPhraseProd();

        /* initialize for the missing topic */
        action.initForMissingTopic(orig);

        /* resolve and execute the replacement action */
        resolveAndReplaceAction(action);
    }

    /* initialize a new action we're retrying for a missing direct object */
    initForMissingDobj(orig)
    {
        /*
         *   The original action must have been a TopicAction, so we
         *   already have a topic.  Simply copy the original topic to use
         *   as our own topic.  For example: ASK ABOUT BOOK -> ASK <whom>
         *   ABOUT BOOK.  
         */
        topicMatch = new PreResolvedProd(orig.getTopic());
    }

    /* initialize for retrying with a missing topic phrase */
    initForMissingTopic(orig)
    {
        local origDobj = orig.getDobj();
        
        /*
         *   The original action must have been a TAction, so we already
         *   have a direct object.  Simply copy the original direct object
         *   for use as our own.  For example: ASK BOB -> ASK BOB ABOUT
         *   <what>.  
         */
        dobjMatch = new PreResolvedProd(origDobj != nil
                                        ? origDobj : orig.dobjList_);
    }

    /* create our TAction topic resolver */
    createTopicResolver(issuingActor, targetActor)
    {
        return new TActionTopicResolver(
            self, issuingActor, targetActor, topicMatch, whichMessageTopic,
            getTopicQualifierResolver(issuingActor, targetActor, nil));
    }

    /* 
     *   In the topic phrase, we can use an anaphoric pronoun to refer
     *   back to the direct object.  Since we resolve the direct object
     *   phrase first, we can simply return the direct object list as the
     *   binding.  If the direct object isn't resolved yet, make a note to
     *   come back and re-bind the anaphor.  
     */
    getAnaphoricBinding(typ)
    {
        /* if we've already resolved the direct object phrase, return it */
        if (dobjList_ not in (nil, []))
            return dobjList_;

        /* no dobj yet - make a note that we need to re-bind the anaphor */
        needAnaphoricBinding_ = true;

        /* 
         *   return an empty list for now to indicate that the anaphor is
         *   acceptable but has no binding yet 
         */
        return [];
    }

    /*
     *   Flag: we have been asked for an anaphoric binding, but we don't
     *   have a binding available.  We'll check this after resolving the
     *   first-resolved noun phrase so that we can go back and re-resolve
     *   it again after resolving the other noun phrase.  
     */
    needAnaphoricBinding_ = nil

    /* we have a direct object and a topic phrase */
    predicateNounPhrases = [&dobjMatch, &topicMatch]

    /* get an object role */
    getRoleFromIndex(idx)
    {
        /* 
         *   index 2 is the topic object, which is always in the indirect
         *   object role; for others, inherit the default handling 
         */
        return (idx == 2 ? IndirectObject : inherited(idx));
    }

    /* get the OtherObject role for the given role */
    getOtherObjectRole(role)
    {
        /* the complementary roles are DirectObject and IndirectObject */
        return (role == DirectObject ? IndirectObject : DirectObject);
    }

    /* get the resolved object in a given role */
    getObjectForRole(role)
    {
        /* 
         *   the topic is in the IndirectObject role; inherit the default
         *   for others 
         */
        return (role == IndirectObject ? getTopic() : inherited(role));
    }

    /* get the match tree for the given role */
    getMatchForRole(role)
    {
        /* 
         *   the topic is in the IndirectObject role; inherit the default
         *   for anything else 
         */
        return (role == IndirectObject ? topicMatch : inherited(role));
    }

    /*
     *   Manually set the resolved objects.  We'll set our direct and
     *   indirect objects.  
     */
    setResolvedObjects(dobj, topic)
    {
        /* inherit default handling for the direct object */
        inherited(dobj);

        /* set the resolved topic */
        setResolvedTopic(topic);
    }

    /* manually set the pre-resolved match trees */
    setObjectMatches(dobj, topic)
    {
        /* inherit default handling for the direct object */
        inherited(dobj);
        
        /* note the new topic match tree */
        setTopicMatch(topic);
    }

    /*
     *   Get the list of active objects.  We return only our direct
     *   object, since our topic isn't actually a simulation object.  
     */
    getCurrentObjects()
    {
        return [dobjCur_];
    }

    /* set the current objects */
    setCurrentObjects(lst)
    {
        dobjCur_ = lst[1];
        dobjInfoCur_ = nil;
    }

    /* the resolved topic object list */
    topicList_ = nil

    /* my cached topic resolver */
    topicResolver_ = nil

    /* grammatical role played by topic phrase in generated messages */
    whichMessageTopic = nil

    /* -------------------------------------------------------------------- */
    /*
     *   Direct Object Resolver implementation.  We serve as our own
     *   direct object resolver, so we define any special resolver
     *   behavior here.  
     */

    /* the true role of the resolved object is always as the direct object */
    whichObject = DirectObject

    /* 
     *   What we call our direct object might actually be playing the
     *   grammatical role of the indirect object - in order to inherit
     *   easily from TAction, we call our resolved object our direct
     *   object, regardless of which grammatical role it actually plays.
     *   For the most part it doesn't matter which is which; but for the
     *   purposes of our resolver, we actually do care about its real role.
     *   So, override the resolver method whichMessageObject so that it
     *   returns whichever role is NOT served by the topic object.  
     */
    whichMessageObject = (whichMessageTopic == DirectObject
                          ? IndirectObject : DirectObject)
;

/*
 *   "Conversation" TopicTAction.  Many TopicTAction verbs involve
 *   conversation with an actor, who's specified as the direct object: ASK
 *   <actor> ABOUT <topic>, TELL <actor> ABOUT <topic>, ASK <actor> FOR
 *   <topic>.  For these common cases, the most likely default direct
 *   object is the last interlocutor of the actor performing the command -
 *   that is, ASK ABOUT BOOK should by default be directed to whomever we
 *   were speaking to last.
 *   
 *   This subclass is suitable for such verbs.  When asked for a default
 *   direct object, we'll check for a current interlocutor, and use it as
 *   the default if available.  If no interlocutor is available, we'll
 *   inherit the standard default handling.  
 */
class ConvTopicTAction: TopicTAction
    getDefaultDobj(np, resolver)
    {
        /* 
         *   check to see if the actor has a default interlocutor; if so,
         *   use it as the default actor to be addressed here, otherwise
         *   use the default handling 
         */
        local obj = resolver.getTargetActor().getCurrentInterlocutor();
        if (obj != nil)
            return [new ResolveInfo(obj, 0, nil)];
        else
            return inherited(np, resolver);
    }

    /* 
     *   Create the topic resolver.  Use a conversational topic resolver
     *   for this type of action. 
     */
    createTopicResolver(issuingActor, targetActor)
    {
        return new ConvTopicResolver(
            self, issuingActor, targetActor, topicMatch, whichMessageTopic,
            getTopicQualifierResolver(issuingActor, targetActor, nil));
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   A conversational intransitive action.  This class is for actions such
 *   as Hello, Goodbye, Yes, No - conversational actions that don't involve
 *   any topics or other objects, but whose expression is entirely
 *   contained in the verb.  
 */
class ConvIAction: IAction
    /* this action is conversational */
    isConversational(issuer) { return true; }
;

/* ------------------------------------------------------------------------ */
/*
 *   Resolved Topic object.  The topic of a TopicTAction always resolves to
 *   one of these objects.  
 */
class ResolvedTopic: object
    construct(inScope, likely, others, prod)
    {
        /*
         *   Remember our lists of objects.  We keep three separate lists,
         *   so that the action knows how we've classified the objects
         *   matching our phrase.  We keep a list of objects that are in
         *   scope; a list of objects that aren't in scope but which the
         *   actor thinks are likely topics; and a list of all of the other
         *   matches.
         *   
         *   We keep only the simulation objects in these retained lists -
         *   we don't keep the full ResolveInfo data here.  
         */
        inScopeList = inScope.mapAll({x: x.obj_});
        likelyList = likely.mapAll({x: x.obj_});
        otherList = others.mapAll({x: x.obj_});

        /* keep the production match tree */
        topicProd = prod;

        /*
         *   But it might still be interesting to have the ResolveInfo
         *   data, particularly for information on the vocabulary match
         *   strength.  Keep that information in a separate lookup table
         *   indexed by simulation object.  
         */
        local rt = new LookupTable();
        others.forEach({x: rt[x.obj_] = x});
        likely.forEach({x: rt[x.obj_] = x});
        inScope.forEach({x: rt[x.obj_] = x});

        /* save the ResolvedInfo lookup table */
        resInfoTab = rt;
    }

    /* 
     *   Static method: create a ResolvedTopic to represent an object
     *   that's already been resolved to a game object for the current
     *   action.  'role' is the object role to wrap (DirectObject,
     *   IndirectObject, etc).  
     */
    wrapActionObject(role)
    {
        local rt;
        
        /* create a ResolvedTopic with just the match tree */
        rt = new ResolvedTopic([],  [], [], gAction.getMatchForRole(role));

        /* 
         *   add the current resolved object in the role as the one
         *   in-scope match 
         */
        rt.inScopeList += gAction.getObjectForRole(role);

        /* return the ResolvedTopic wrapper */
        return rt;
    }

    /*
     *   Static method: create a ResolvedTopic to represent the given
     *   object.
     */
    wrapObject(obj)
    {
        local rt;
        
        /* create a ResolvedTopic with a PreResolvedProd for the object */
        rt = new ResolvedTopic([], [], [], new PreResolvedProd(obj));

        /* add the given object or objects as the in-scope match list */
        rt.inScopeList += obj;

        /* return the ResolvedTopic wrapper */
        return rt;
    }

    /*
     *   Get the best object match to the topic.  This is a default
     *   implementation that can be changed by game authors or library
     *   extensions to implement different topic-matching strategies.  This
     *   implementation simply picks an object arbitrarily from the
     *   "strongest" of the three lists we build: if there's anything in
     *   the inScopeList, we choose an object from that list; otherwise, if
     *   there's anything in the likelyList, we choose an object from that
     *   list; otherwise we choose an object from the otherList.
     */
    getBestMatch()
    {
        /* if there's an in-scope match, pick one */
        if (inScopeList.length() != 0)
            return inScopeList[1];

        /* nothing's in scope, so try to pick a likely match */
        if (likelyList.length() != 0)
            return likelyList[1];

        /* 
         *   there's not even anything likely, so pick an "other", if
         *   there's anything in that list 
         */
        if (otherList.length() != 0)
            return otherList[1];

        /* there are no matches at all - return nil */
        return nil;
    }

    /*
     *   Is the given object among the possible matches for the topic? 
     */
    canMatchObject(obj)
    {
        return inScopeList.indexOf(obj) != nil
            || likelyList.indexOf(obj) != nil
            || otherList.indexOf(obj) != nil;
    }

    /* get the original text of the topic phrase */
    getTopicText() { return topicProd.getOrigText(); }

    /*
     *   Are we allowed to match the topic text literally, for parsing
     *   purposes?  If this is true, it means that we can match the literal
     *   text the player entered against strings, regular expressions,
     *   etc.; for example, we can match a TopicMatchTopic's matchPattern
     *   regular expression.  If this is nil, it means that we can only
     *   interpret the meaning of the resolved topic by looking at the
     *   various topic match lists (inScopeList, likelyList, otherList).
     *   
     *   By default, we simply return true.  Note that the base library
     *   never has any reason of its own to disallow literal matching of
     *   topic text; this property is purely for the use of language
     *   modules, to handle language-specific input that parses at a high
     *   level as a topic phrase but which has some idiomatic or
     *   grammatical function that makes it in appropriate to try to
     *   extract the meaning of the resolved topic from the literal text of
     *   the topic phrase in isolation.  This case doesn't seem to arise in
     *   English, but does occur in other languages: Michel Nizette cites
     *   "parlez-en a Bob" as an example in French, where "en" is
     *   essentially a particle modifying the verb, not a full-fledged
     *   phrase that we can interpret separately as a topic.  
     */
    canMatchLiterally = true

    /* 
     *   get the original tokens of the topic phrase, in canonical
     *   tokenizer format 
     */
    getTopicTokens() { return topicProd.getOrigTokenList(); }

    /*
     *   get the original word strings of the topic phrase - this is
     *   simply a list of the original word strings (in their original
     *   case), without any of the extra information of the more
     *   complicated canonical tokenizer format 
     */
    getTopicWords()
    {
        /* return just the original text strings from the token list */
        return topicProd.getOrigTokenList().mapAll({x: getTokOrig(x)});
    }

    /*
     *   Get the parser ResolveInfo object for a given matched object.
     *   This recovers the ResolveInfo describing the parsing result for
     *   any object in the resolved object lists (inScopeList, etc). 
     */
    getResolveInfo(obj) { return resInfoTab[obj]; }

    /*
     *   Our lists of resolved objects matching the topic phrase,
     *   separated by classification.  
     */
    inScopeList = nil
    likelyList = nil
    otherList = nil

    /*
     *   The production match tree object that matched the topic phrase in
     *   the command.  This can be used to obtain the original tokens of
     *   the command or the original text of the phrase.  
     */
    topicProd = nil

    /*
     *   ResolveInfo table for the resolved objects.  This is a lookup
     *   table indexed by simulation object.  Each entry in the resolved
     *   object lists (inScopeList, etc) has have a key in this table, with
     *   the ResolveInfo object as the value for the key.  This can be used
     *   to recover the ResolveInfo object describing the parser results
     *   for this object.  
     */
    resInfoTab = nil
;

/*
 *   A special topic for "nothing."  It's occasionally useful to be able
 *   to construct a TopicTAction with an empty topic phrase.  For the
 *   topic phrase to be well-formed, we need a valid ResolvedTopic object,
 *   even though it won't refer to anything. 
 */
resolvedTopicNothing: ResolvedTopic
    inScopeList = []
    likelyList = []
    otherList = []

    getTopicText() { return ''; }
    getTopicTokens() { return []; }
    getTopicWords() { return []; }
;

/*
 *   Topic Resolver
 */
class TopicResolver: Resolver
    construct(action, issuingActor, targetActor, prod, which,
              qualifierResolver)
    {
        /* inherit the base class constructor */
        inherited(action, issuingActor, targetActor);

        /* the topic phrase doesn't have a true resolved role */
        whichObject = nil;

        /* remember the grammatical role of our object in the command */
        whichMessageObject = which;

        /* remember our topic match tree */
        topicProd = prod;

        /* remember the resolver for qualifier phrases */
        qualifierResolver_ = qualifierResolver;
    }

    resetResolver()
    {
        /* inherit the default handling */
        inherited();

        /* reset our qualifier resolver as well */
        qualifierResolver_.resetResolver();
    }

    /* 
     *   package a resolved topic list - if it's not already represented as
     *   a ResolvedTopic object, we'll apply that wrapping
     */
    packageTopicList(lst, match)
    {
        /* 
         *   if the topic is other than a single ResolvedTopic object, run
         *   it through the topic resolver's ambiguous noun phrase filter
         *   to get it into that canonical form 
         */
        if (lst != nil
            && (lst.length() > 1
                || (lst.length() == 1 && !lst[1].obj_.ofKind(ResolvedTopic))))
        {
            /* it's not in canonical form, so get it in canonical form now */
            lst = filterAmbiguousNounPhrase(lst, 1, match);
        }

        /* return the result */
        return lst;
    }

    /* our qualifier resolver */
    qualifierResolver_ = nil

    /* get our qualifier resolver */
    getQualifierResolver() { return qualifierResolver_; }
    getPossessiveResolver() { return qualifierResolver_; }

    /*
     *   Determine if the object is in scope.  We consider any vocabulary
     *   match to be in scope for the purposes of a topic phrase, since the
     *   subject matter of topics is mere references to things, not the
     *   things themselves; we can, for example, ASK ABOUT things that
     *   aren't physically present, or even about completely abstract
     *   ideas.
     */
    objInScope(obj) { return true; }

    /* 
     *   our scope is global, because we don't limit the scope to the
     *   physical senses 
     */
    isGlobalScope = true

    /*
     *   Determine if an object is in physical scope.  We'll accept
     *   anything that's in physical scope, and we'll also accept any topic
     *   object that the actor knows about.
     *   
     *   Note that this isn't part of the basic Resolver interface.  It's
     *   instead provided as a service routine for our subclasses, so that
     *   they can easily determine the physical scope of an object if
     *   needed.  
     */
    objInPhysicalScope(obj)
    {
        /* 
         *   it's in physical scope, or if the actor knows about it as a
         *   topic, it's in scope 
         */
        return (scope_.indexOf(obj) != nil);
    }

    /*
     *   Filter an ambiguous noun phrase list using the strength of
     *   possessive qualification, if any.  For a topic phrase, we want to
     *   keep all of the possibilities.  
     */
    filterPossRank(lst, num)
    {
        return lst;
    }

    /*
     *   Filter an ambiguous noun list.
     *   
     *   It is almost always undesirable from a user interface perspective
     *   to ask for help disambiguating a topic phrase.  In the first
     *   place, since all topics tend to be in scope all the time, we
     *   might reveal too much about the inner model of the story if we
     *   were to enumerate all of the topic matches to a phrase.  In the
     *   second place, topics are used in conversational contexts, so it
     *   almost never make sense for the parser to ask for clarification -
     *   the other member of the conversation might ask, but not the
     *   parser.  So, we'll always filter the list to the required number,
     *   even if it means we choose arbitrarily.
     *   
     *   As a first cut, we prefer objects that are physically in scope to
     *   those not in scope: if the player is standing next to a control
     *   panel and types "ask bob about control panel," it makes little
     *   sense to consider any other control panels in the simulation.
     *   
     *   As a second cut, we'll ask the actor to filter the list.  Games
     *   that keep track of the actor's knowledge can use this to filter
     *   according to topics the actor is likely to know about.  
     */
    filterAmbiguousNounPhrase(lst, requiredNum, np)
    {
        /* ask the action to create the ResolvedTopic */
        local rt = resolveTopic(lst, requiredNum, np);

        /* wrap the ResolvedTopic in the usual ResolveInfo list */
        return [new ResolveInfo(rt, 0, np)];
    }
        
    /*
     *   Resolve the topic phrase.  This returns a ResolvedTopic object
     *   encapsulating the resolution of the phrase.
     *   
     *   This default base class implementation simply creates a resolved
     *   topic list with the whole set of possible matches
     *   undifferentiated.  Subclasses for specialized actions might want
     *   to differentiate the items in the list, based on things like the
     *   actor's knowledge so far or what's in physical scope.  
     */
    resolveTopic(lst, requiredNum, np)
    {
        /* return a ResolvedTopic that lumps everything in the main list */
        return new ResolvedTopic(lst, [], [], topicProd);
    }

    /*
     *   Resolve an unknown phrase.  We allow unknown words to be used in
     *   topics; we simply return a ResolvedTopic that doesn't refer to
     *   any simulation objects at all.  
     */
    resolveUnknownNounPhrase(tokList)
    {
        /* 
         *   Create our ResolvedTopic object for the results.  We have
         *   words we don't know, so we're not referring to any objects,
         *   so our underlying simulation object list is empty.  
         */
        local rt = new ResolvedTopic([], [], [], topicProd);

        /* return a resolved topic object with the empty list */
        return [new ResolveInfo(rt, 0, new TokenListProd(tokList))];
    }

    /* filter a plural */
    filterPluralPhrase(lst, np)
    {
        /*
         *   Handle this the same way we handle an ambiguous noun phrase,
         *   so that we yield only one object.  Topics are inherently
         *   singular; we'll allow asking about a grammatically plural
         *   term, but we'll turn it into a single topic result.  
         */
        return filterAmbiguousNounPhrase(lst, 1, np);
    }

    /* get a default object */
    getDefaultObject(np)
    {
        /* there is never a default for a topic */
        return nil;
    }

    /* it's fine not to match a topic phrase */
    noVocabMatch(action, txt) { }
    noMatch(action, txt) { }
    noMatchPoss(action, txt) { }

    /* we don't allow ALL or provide defaults */
    getAll(np) { return []; }
    getAllDefaults() { return []; }

    /* the production match tree for the topic phrase we're resolving */
    topicProd = nil
;

/*
 *   A topic resolver specialized for TopicTActions - actions involving a
 *   topic and a physical object, such as CONSULT ABOUT.  For these topics,
 *   we'll let the action handle the resolution.  
 */
class TActionTopicResolver: TopicResolver
    resolveTopic(lst, requiredNum, np)
    {
        /* let the action handle it */
        return action_.filterTopic(lst, np, self);
    }
;

/*
 *   A topic resolver specialized for conversational actions (ASK ABOUT,
 *   TELL ABOUT, etc).  When we resolve the topic, we'll differentiate the
 *   resolution to differentiate based on the knowledge of the actor who's
 *   performing the command.  
 */
class ConvTopicResolver: TopicResolver
    /*
     *   Resolve the topic phrase.  We'll break up the vocabulary matches
     *   into three sublists: the objects that are either in physical scope
     *   or known to the actor performing the command; objects that the
     *   actor considers likely topics; and everything else.  
     */
    resolveTopic(lst, requiredNum, np)
    {
        local inScope;
        local actorPrefs;

        /* 
         *   First, get the subset of items that are in conversational
         *   scope - we'll consider this the best set of matches.  
         */
        inScope = lst.subset({x: objInConvScope(x.obj_)});

        /* 
         *   eliminate the in-scope items from the list, so we can
         *   consider only what remains 
         */
        lst -= inScope;

        /* 
         *   ask the actor to pick out the most likely set of topics from
         *   the ones that remain 
         */
        actorPrefs = lst.subset({x: actor_.isLikelyTopic(x.obj_)});

        /* eliminate those items from the list */
        lst -= actorPrefs;

        /* create our ResolvedTopic object and return it */
        return new ResolvedTopic(inScope, actorPrefs, lst, topicProd);
    }

    /*
     *   Determine if an object is in "conversational" scope - this returns
     *   true if the object is in physical scope or it's known to the actor
     *   performing the command. 
     */
    objInConvScope(obj)
    {
        /* 
         *   if it's in physical scope, or the actor knows about it, it's
         *   in conversation scope 
         */
        return (objInPhysicalScope(obj) || actor_.knowsTopic(obj));
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   System action.  These actions are for out-of-game meta-verbs (save,
 *   restore, undo).  These verbs take no objects, must be performed by
 *   the player (thus by the player character, not an NPC), and consume no
 *   game clock time.  
 */
class SystemAction: IAction
    /* execute the action */
    execAction()
    {
        /* 
         *   Conceptually, system actions are performed by the player
         *   directly, not by any character in the game (not even the
         *   player character).  However, we don't distinguish in the
         *   command-entry user interface between a command the player is
         *   performing and a command to the player character, hence we
         *   can merely ensure that the command is not directed to a
         *   non-player character. 
         */
        if (!gActor.isPlayerChar)
        {
            gLibMessages.systemActionToNPC();
            exit;
        }

        /* 
         *   system actions sometimes need to prompt for interactive
         *   responses, so deactivate the report list - this will allow
         *   interactive prompts to be shown immediately, not treated as
         *   reports to be deferred until the command is finished 
         */
        gTranscript.showReports(true);
        gTranscript.clearReports();

        /* perform our specific action */
        execSystemAction();

        /* re-activate the transcript for the next command */
        gTranscript.activate();
    }

    /* each subclass must override this to perform its actual action */
    execSystemAction() { }

    /*
     *   Ask for an input file.  We call the input manager, which freezes
     *   the real-time clock, displays the appropriate local file selector
     *   dialog, and restarts the clock.  
     */
    getInputFile(prompt, dialogType, fileType, flags)
    {
        return inputManager.getInputFile(prompt, dialogType, fileType, flags);
    }

    /* system actions consume no game time */
    actionTime = 0
;

/* ------------------------------------------------------------------------ */
/*
 *   An exception class for remappings that we can't handle. 
 */
class ActionRemappingTooComplexError: Exception
;

