#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved.
 *   
 *   TADS 3 Library - actors
 *   
 *   This module provides definitions related to actors, which represent
 *   characters in the game.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Implied command modes 
 */
enum ModePlayer, ModeNPC;


/* ------------------------------------------------------------------------ */
/*
 *   A Topic is an object representing some piece of knowledge in the
 *   story.  Actors can use Topic objects in commands such as "ask" and
 *   "tell".
 *   
 *   A physical simulation object can be a Topic through multiple
 *   inheritance.  In addition, a game can define Topic objects for
 *   abstract conversation topics that don't correspond to simulation
 *   objects; for example, a topic could be created for "the meaning of
 *   life" to allow a command such as "ask guru about meaning of life."
 *   
 *   The key distinction between Topic objects and regular objects is that
 *   a Topic can represent an abstract, non-physical concept that isn't
 *   connected to any "physical" object in the simulation.  
 */
class Topic: VocabObject
    /*
     *   Is the topic known?  If this is true, the topic is in scope for
     *   actions that operate on topics, such as "ask about" and "tell
     *   about."  If this is nil, the topic isn't known.  
     *   
     *   By default, we mark all topics as known to begin with, which
     *   allows discussion of any topic at any time.  Some authors prefer
     *   to keep track of which topics the player character actually has
     *   reason to know about within the context of the game, making topics
     *   available for conversation only after they become known for some
     *   good reason, such as another character mentioning them in
     *   conversation.
     *   
     *   Note that, as with Thing.isKnown, this is only the DEFAULT 'known'
     *   property.  Each actor can have its own separate 'known' property
     *   by defining the actor's 'knownProp' to a different property name.
     */
    isKnown = true

    /* 
     *   Topics are abstract objects, so they can't be sensed with any of
     *   the physical senses, even if they're ever included as part of a
     *   containment hierarchy (which might be convenient in some cases
     *   for purposes of associating a topic with a physical object, for
     *   example).
     */
    canBeSensed(sense, trans, ambient) { return nil; }

    /* a topic cannot by default be used to resolve a possessive phrase */
    canResolvePossessive = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   FollowInfo - this is an object that tracks an actor's knowledge of
 *   the objects that the actor can follow, which are objects that actor
 *   has witnessed leaving the current location.  We keep track of each
 *   followable object and the direction we saw it depart.  
 */
class FollowInfo: object
    /* the object we can follow */
    obj = nil

    /* the TravelConnector the object traversed to leave */
    connector = nil

    /* 
     *   The source location - this is the location we saw the object
     *   depart.  We keep track of this because an actor can follow an
     *   object only if the actor is starting from the same location where
     *   the actor saw the object depart.  
     */
    sourceLocation = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Postures.  A posture describes how an actor is internally positioned:
 *   standing, lying, sitting.  We represent postures with objects of
 *   class Posture to make it easier to add new game-specific postures.  
 */
class Posture: object
    /* 
     *   Try getting the current actor into this posture within the given
     *   location, by running an appropriate implied command.  
     */
    tryMakingPosture(loc) { }

    /* put the actor into our posture via a nested action */
    setActorToPosture(actor, loc) { }
;

/*
 *   Standing posture - this is the default posture, which an actor
 *   normally uses for travel.  Actors are generally in this posture any
 *   time they are not sitting on something, lying on something, or
 *   similar. 
 */
standing: Posture
    tryMakingPosture(loc) { return tryImplicitAction(StandOn, loc); }
    setActorToPosture(actor, loc) { nestedActorAction(actor, StandOn, loc); }
;

/*
 *   Sitting posture. 
 */
sitting: Posture
    tryMakingPosture(loc) { return tryImplicitAction(SitOn, loc); }
    setActorToPosture(actor, loc) { nestedActorAction(actor, SitOn, loc); }
;

/*
 *   Lying posture. 
 */
lying: Posture
    tryMakingPosture(loc) { return tryImplicitAction(LieOn, loc); }
    setActorToPosture(actor, loc) { nestedActorAction(actor, LieOn, loc); }
;


/* ------------------------------------------------------------------------ */
/*
 *   Conversation manager output filter.  We look for special tags in the
 *   output stream:
 *   
 *   <.reveal key> - add 'key' to the knowledge token lookup table.  The
 *   'key' is an arbitrary string, which we can look up in the table to
 *   determine if the key has even been revealed.  This can be used to make
 *   a response conditional on another response having been displayed,
 *   because the key will only be added to the table when the text
 *   containing the <.reveal key> sequence is displayed.
 *   
 *   <.convnode name> - switch the current responding actor to conversation
 *   node 'name'.  
 *   
 *   <.convstay> - keep the responding actor in the same conversation node
 *   as it was in at the start of the current response
 *   
 *   <.topics> - schedule a topic inventory for the end of the turn (just
 *   before the next command prompt) 
 */
conversationManager: OutputFilter, PreinitObject
    /*
     *   Custom extended tags.  Games and library extensions can add their
     *   own tag processing as needed, by using 'modify' to extend this
     *   object.  There are two things you have to do to add your own tags:
     *   
     *   First, add a 'customTags' property that defines a regular
     *   expression for your added tags.  This will be incorporated into
     *   the main pattern we use to look for tags.  Simply specify a
     *   string that lists your tags separated by "|" characters, like
     *   this:
     *   
     *   customTags = 'foo|bar'
     *   
     *   Second, define a doCustomTag() method to process the tags.  The
     *   filter routine will call your doCustomTag() method whenever it
     *   finds one of your custom tags in the output stream.  
     */
    customTags = nil
    doCustomTag(tag, arg) { /* do nothing by default */ }

    /* filter text written to the output stream */
    filterText(ostr, txt)
    {
        local start;
        
        /* scan for our special tags */
        for (start = 1 ; ; )
        {
            local match;
            local arg;
            local actor;
            local sp;
            local tag;
            local nxtOfs;
            
            /* scan for the next tag */
            match = rexSearch(tagPat, txt, start);

            /* if we didn't find it, we're done */
            if (match == nil)
                break;

            /* note the next offset */
            nxtOfs = match[1] + match[2];

            /* get the argument (the third group from the match) */
            arg = rexGroup(3);
            if (arg != nil)
                arg = arg[3];

            /* pick out the tag */
            tag = rexGroup(1)[3].toLower();

            /* check which tag we have */
            switch (tag)
            {
            case 'reveal':
                /* reveal the key by adding it to our database */
                setRevealed(arg);
                break;

            case 'convbegin':
                /* 
                 *   Internal tag - starting a conversational response for
                 *   an actor, identified by an index in our idToActor
                 *   vector.  Get the actor.  
                 */
                actor = idToActor[toInteger(arg)];

                /* 
                 *   since we're just starting a response, clear the flag
                 *   in the actor indicating that a ConvNode has been set
                 *   in the course of this response 
                 */
                actor.responseSetConvNode = nil;

                /* remember the new responding actor */
                respondingActor = actor;

                /* done */
                break;

            case 'convend':
                /*
                 *   Ending a conversational response for a given actor,
                 *   identified by the first argument, which is an index in
                 *   our idToActor vector. 
                 */
                sp = arg.find(' ');
                actor = idToActor[toInteger(arg.substr(1, sp - 1))];

                /* the rest of the argument is the default new ConvNode */
                arg = arg.substr(sp + 1);

                /* if the new ConvNode is empty, it means no ConvNode */
                if (arg == '')
                    arg = nil;

                /* 
                 *   if we didn't explicitly set a new ConvNode in the
                 *   course of this response, apply the default 
                 */
                if (!actor.responseSetConvNode)
                    actor.setConvNodeReason(arg, 'convend');

                /*
                 *   Since we've just finished showing a message that
                 *   specifically refers to this actor, the player should
                 *   be able to refer to this actor using a pronoun on the
                 *   next command.  Set the responding actor as the
                 *   antecedent for the appropriate singular pronouns for
                 *   the player character.  Note that we do this at the end
                 *   of the response, so that the antecedent is the last
                 *   one if we have more than one.  
                 */
                gPlayerChar.setPronounObj(actor);

                /* done */
                break;

            case 'convnode':
                /* 
                 *   If there's a current responding actor, set its current
                 *   conversation node.
                 */
                if (respondingActor != nil)
                {
                    /* 
                     *   Set the new node.  While we're working, capture
                     *   any output that occurs so that we can insert it
                     *   into the output stream just after the <.convnode>
                     *   tag, so that any text displayed within the
                     *   ConvNode's activation method (noteActive) is
                     *   displayed in the proper order.  
                     */
                    local ctxt = mainOutputStream.captureOutput(
                        {: respondingActor.setConvNodeReason(arg, 'convnode') });

                    /* re-insert any text we captured */
                    txt = txt.substr(1, nxtOfs - 1)
                        + ctxt
                        + txt.substr(nxtOfs);
                }
                break;

            case 'convstay':
                /* 
                 *   leave the responding actor in the old conversation
                 *   node - we don't need to change the ConvNode, but we do
                 *   need to note that we've explicitly set it 
                 */
                if (respondingActor != nil)
                    respondingActor.responseSetConvNode = true;
                break;

            case 'topics':
                /* schedule a topic inventory listing */
                scheduleTopicInventory();
                break;

            default:
                /* check for an extended tag */
                doCustomTag(tag, arg);
                break;
            }

            /* continue the search after this match */
            start = nxtOfs;
        }

        /* 
         *   remove the tags from the text by replacing every occurrence
         *   with an empty string, and return the result 
         */
        return rexReplace(tagPat, txt, '', ReplaceAll);
    }

    /* regular expression pattern for our tags */
    tagPat = static new RexPattern(
        '<nocase><langle><dot>'
        + '(reveal|convbegin|convend|convnode|convstay|topics'
        + (customTags != nil ? '|' + customTags : '')
        + ')'
        + '(<space>+(<^rangle>+))?'
        + '<rangle>')

    /*
     *   Schedule a topic inventory request.  Game code can call this at
     *   any time to request that the player character's topic inventory
     *   be shown automatically just before the next command prompt.  In
     *   most cases, game code won't call this directly, but will request
     *   the same effect using the <.topics> tag in topic response text.  
     */
    scheduleTopicInventory()
    {
        /* note that we have a request for a prompt-time topic inventory */
        pendingTopicInventory = true;
    }

    /*
     *   Show or schedule a topic inventory request.  If the current
     *   action has a non-default command report, schedule it; otherwise,
     *   show it now.
     *   
     *   If there's a non-default report, don't suggest the topics now;
     *   instead, schedule a topic inventory for the end of the turn.
     *   When we have a non-default report, the report could change the
     *   ConvNode for the actor, so we don't want to show the topic
     *   inventory until we've had a chance to process all of the reports.
     */
    showOrScheduleTopicInventory(actor, otherActor)
    {
        /* check for a non-default command report in the current action */
        if (gTranscript.currentActionHasReport(
            {x: x.ofKind(MainCommandReport)}))
        {
            /* we have a non-default report - defer the topic inventory */
            scheduleTopicInventory();
        }
        else
        {
            /* we have only a default report, so show the inventory now */
            actor.suggestTopicsFor(otherActor, nil);
        }
    }

    /*
     *   Note that an actor is about to give a response through a
     *   TopicEntry object.  We'll remember the actor so that we'll know
     *   which actor is involved in a <.convnode> operation.  
     */
    beginResponse(actor)
    {
        /* if the actor doesn't have an ID yet, assign one */
        if (actor.convMgrID == nil)
        {
            /* add the actor to our vector of actors */
            idToActor.append(actor);

            /* the ID is simply the index in this vector */
            actor.convMgrID = idToActor.length();
        }

        /* output a <.convbegin> for the actor */
        gTranscript.addReport(new ConvBeginReport(actor.convMgrID));
    }

    /* 
     *   Finish the response - call this after we finish handling the
     *   response.  There must be a subsequent matching call to this
     *   routine whenever beginResponse() is called.
     *   
     *   'node' is the default new ConvNode the actor for the responding
     *   actor.  If another ConvNode was explicitly set in the course of
     *   handling the response, this is ignored, since the explicit
     *   setting overrides this default.  
     */
    finishResponse(actor, node)
    {
        local prv;
        local oldNode;
        
        /* if the node is a ConvNode object, use its name */
        if (node != nil && node.ofKind(ConvNode))
            node = node.name;

        /* 
         *   if the previous report was our ConvBeginReport, the
         *   conversation display was empty, so ignore the whole thing 
         */
        if ((prv = gTranscript.getLastReport()) != nil
            && prv.ofKind(ConvBeginReport)
            && prv.actorID == actor.convMgrID)
        {
            /* remove the <.convbegin> report - we're canceling it out */
            gTranscript.deleteLastReport();

            /* we're done - do not generate the <.convend> */
            return;
        }

        /* 
         *   if the actor has a current ConvNode, and our default next
         *   node is nil, and the current node is marked as "sticky," stay
         *   in the current node rather than switching to a nil default 
         */
        if (node == nil
            && (oldNode = actor.curConvNode) != nil
            && oldNode.isSticky)
        {
            /* it's sticky, so stay at this node */
            node = oldNode.name;
        }

        /* output a <.convend> for the actor */
        gTranscript.addReport(new ConvEndReport(actor.convMgrID, node));
    }

    /* 
     *   The current responding actor.  Actors should set this when they're
     *   about to show a response to an ASK, TELL, etc. 
     */
    respondingActor = nil

    /*
     *   Mark a tag as revealed.  This adds an entry for the tag to the
     *   revealedNameTab table.  We simply set the table entry to 'true';
     *   the presence of the tag in the table constitutes the indication
     *   that the tag has been revealed.
     *   
     *   (Games and library extensions can use 'modify' to override this
     *   and store more information in the table entry.  For example, you
     *   could store the time when the information was first revealed, or
     *   the location where it was learned.  If you do override this, just
     *   be sure to set the revealedNameTab entry for the tag to a non-nil
     *   and non-zero value, so that any code testing the presence of the
     *   table entry will see that the slot is indeed set.)  
     */
    setRevealed(tag)
    {
        revealedNameTab[tag] = true;
    }

    /* 
     *   The global lookup table of all revealed keys.  This table is keyed
     *   by the string naming the revelation; the value associated with
     *   each key is not used (we always just set it to true).  
     */
    revealedNameTab = static new LookupTable(32, 32)

    /* a vector of actors, indexed by their convMgrID values */
    idToActor = static new Vector(32)

    /* preinitialize */
    execute()
    {
        /* add every ConvNode object to our master table */
        forEachInstance(ConvNode,
                        { obj: obj.getActor().convNodeTab[obj.name] = obj });

        /* 
         *   set up the prompt daemon that makes automatic topic inventory
         *   suggestions when appropriate 
         */
        new PromptDaemon(self, &topicInventoryDaemon);
    }

    /*
     *   Prompt daemon: show topic inventory when appropriate.  When a
     *   response explicitly asks us to show a topic inventory using the
     *   <.topics> tag, or when other game code asks us to show topic
     *   inventory by calling scheduleTopicInventory(), we'll show the
     *   inventory just before the command input prompt.  
     */
    topicInventoryDaemon()
    {
        /* if we have a topic inventory scheduled, show it now */
        if (pendingTopicInventory)
        {
            /* 
             *   Show the player character's topic inventory.  This is not
             *   an explicit inventory request, since the player didn't ask
             *   for it.  
             */
            gPlayerChar.suggestTopics(nil);

            /* we no longer have a pending inventory request */
            pendingTopicInventory = nil;
        }
    }

    /* flag: we have a pending prompt-time topic inventory request */
    pendingTopicInventory = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   A plug-in topic database.  The topic database is a set of TopicEntry
 *   objects that specify the responses to queries on particular topics.
 *   The exact nature of the queries that a particular topic database
 *   handles is up to the database subclass to define; we just provide the
 *   abstract mechanism for finding and displaying responses.
 *   
 *   This is a "plug-in" database in that it's meant to be added into other
 *   classes using multiple inheritance.  This isn't meant to be used as a
 *   stand-alone abstract topic entry container.  
 */
class TopicDatabase: object
    /* 
     *   Is the topic group active?  A TopicEntry always checks with its
     *   container to see if the children of the container are active.  By
     *   default, everything in the database is active.  
     */
    topicGroupActive = true

    /*
     *   Get the score adjustment for all topic entries contained within.
     *   The default adjustment is zero; TopicGroup objects can use this to
     *   adjust the score for their nested entries.  
     */
    topicGroupScoreAdjustment = 0

    /*
     *   Handle a topic.  Look up the topic in our topic list for the
     *   given conversational action type.  If we find a match, we'll
     *   invoke the matching topic list entry to handle it.  We'll return
     *   true if we find a match, nil if not.  
     */
    handleTopic(fromActor, topic, convType, path)
    {
        local resp;

        /* find the best response */
        resp = findTopicResponse(fromActor, topic, convType, path);
        
        /* if we found a match, let it handle the topic */
        if (resp != nil)
        {
            /* show the response */
            showTopicResponse(fromActor, topic, resp);
            
            /* tell the caller we handled it */
            return true;
        }
        else
        {
            /* tell the caller we didn't handle it */
            return nil;
        }
    }

    /* show the response we found for a topic */
    showTopicResponse(fromActor, topic, resp)
    {
        /* let the response object handle it */
        resp.handleTopic(fromActor, topic);
    }

    /* 
     *   find the best response (a TopicEntry object) for the given topic
     *   (a ResolvedTopic object) 
     */
    findTopicResponse(fromActor, topic, convType, path)
    {
        local topicList;
        local best, bestScore;

        /* 
         *   Get the list of possible topics for this conversation type.
         *   The topic list is contained in one of our properties; exactly
         *   which property is determined by the conversation type. 
         */
        topicList = self.(convType.topicListProp);
        
        /* if the topic list is nil, we obviously won't find the topic */
        if (topicList == nil)
            return nil;

        /* scan our topic list for the best match(es) */
        best = new Vector();
        bestScore = nil;
        foreach (local cur in topicList)
        {
            /* get this item's score */
            local score = cur.adjustScore(cur.matchTopic(fromActor, topic));

            /* 
             *   If this item has a score at all, and the topic entry is
             *   marked as active, and it's best (or only) score so far,
             *   note it.  Ignore topics marked as not active, since
             *   they're in the topic database only provisionally.  
             */
            if (score != nil
                && cur.checkIsActive()
                && (bestScore == nil || score >= bestScore))
            {
                /* clear the vector if we've found a better score */
                if (bestScore != nil && score > bestScore)
                    best = new Vector();

                /* add this match to the list of ties for this score */
                best.append(cur);

                /* note the new best score */
                bestScore = score;
            }
        }

        /*
         *   If the best-match list is empty, we have no matches.  If
         *   there's just one match, we have a winner.  If we found more
         *   than one match tied for first place, we need to pick one
         *   winner.  
         */
        if (best.length() == 0)
        {
            /* no matches at all */
            best = nil;
        }
        else if (best.length() == 1)
        {
            /* exactly one match - it's easy to pick the winner */
            best = best[1];
        }
        else
        {
            /* 
             *   We have multiple topics tied for first place.  Run through
             *   the topic list and ask each topic to propose the winner. 
             */
            local toks = topic.topicProd.getOrigTokenList().mapAll(
                {x: getTokVal(x)});
            local winner = nil;
            foreach (local t in best)
            {
                /* ask this topic what it thinks the winner should be */
                winner = t.breakTopicTie(best, topic, fromActor, toks);

                /* if the topic had an opinion, we can stop searching */
                if (winner != nil)
                    break;
            }

            /* 
             *   If no one had an opinion, run through the list again and
             *   try to pick by vocabulary match strength.  This is only
             *   possible when all of the topics are associated with
             *   simulation objects; if any topics have pattern matches, we
             *   can't use this method.  
             */
            if (winner == nil)
            {
                local rWinner = nil;
                foreach (local t in best)
                {
                    /* get this topic's match object(s) */
                    local m = t.matchObj;
                    if (m == nil)
                    {
                        /* 
                         *   there's no match object - it's not comparable
                         *   to others in terms of match strength, so we
                         *   can't use this method to break the tie 
                         */
                        winner = nil;
                        break;
                    }

                    /* 
                     *   If it's a list, search for an element with a
                     *   ResolveInfo entry in the topic match, using the
                     *   strongest match if we find more than one.
                     *   Otherwise, just use the strength of this match.  
                     */
                    local ri;
                    if (m.ofKind(Collection))
                    {
                        /* search for a ResolveInfo object */
                        foreach (local mm in m)
                        {
                            /* get this topic */
                            local riCur = topic.getResolveInfo(mm);

                            /* if this is the best match so far, keep it */
                            if (compareVocabMatch(riCur, ri) > 0)
                                ri = riCur;
                        }
                    }
                    else
                    {
                        /* get the ResolveInfo object */
                        ri = topic.getResolveInfo(m);
                    }

                    /* 
                     *   if we didn't find a match, we can't use this
                     *   method to break the tie 
                     */
                    if (ri == nil)
                    {
                        winner = nil;
                        break;
                    }

                    /* 
                     *   if this is the best match so far, elect it as the
                     *   tentative winner 
                     */
                    if (compareVocabMatch(ri, rWinner) > 0)
                    {
                        rWinner = ri;
                        winner = t;
                    }
                }
            }

            /* 
             *   if there's a tie-breaking winner, use it; otherwise just
             *   arbitrarily pick the first item in the list of ties 
             */
            best = (winner != nil ? winner : best[1]);
        }

        /*
         *   If there's a hierarchical search path, AND this topic entry
         *   defines a deferToEntry() method, look for matches in the
         *   inferior databases on the path and check to see if we want to
         *   defer to one of them.  
         */
        if (best != nil && path != nil && best.propDefined(&deferToEntry))
        {
            /* look for a match in each inferior database */
            for (local i = 1, local len = path.length() ; i <= len ; ++i)
            {
                local inf;
                
                /* 
                 *   Look up an entry in this inferior database.  Pass in
                 *   the remainder of the path, so that the inferior
                 *   database can consider further deferral to its own
                 *   inferior databases.  
                 */
                inf = path[i].findTopicResponse(fromActor, topic, convType,
                                                path.sublist(i + 1));

                /* 
                 *   if we found an entry in this inferior database, and
                 *   our entry defers to the inferior entry, then ignore
                 *   the match in our own database 
                 */
                if (inf != nil && best.deferToEntry(inf))
                    return nil;
            }
        }

        /* return the best matching response object, if any */
        return best;
    }

    /*
     *   Compare the vocabulary match strengths of two ResolveInfo objects,
     *   for the purposes of breaking ties in topic matching.  Uses the
     *   usual comparison/sorting return value conventions: -1 means that a
     *   is weaker than b, 0 means they're equivalent, 1 means a is
     *   stronger than b.  
     */
    compareVocabMatch(a, b)
    {
        /* 
         *   If both are nil, they're equivalent.  If one or the other is
         *   nil, the non-nil item is stronger.  
         */
        if (a == nil && b == nil)
            return 0;
        if (a == nil)
            return -1;
        if (b == nil)
            return 1;

        /* 
         *   Both are valid objects, so compare based on the vocabulary
         *   match flags.
         */
        local fa = a.flags_, fb = b.flags_;

        /* check plural truncations - no plural truncation is better */
        if ((fa & PluralTruncated) && !(fb & PluralTruncated))
            return -1;
        if (!(fa & PluralTruncated) && (fb & PluralTruncated))
            return 1;

        /* check any truncation - no truncation is better */
        if ((fa & VocabTruncated) && !(fb & VocabTruncated))
            return -1;
        if (!(fa & VocabTruncated) && (fb & VocabTruncated))
            return 1;

        /* we can't find any reason to prefer one over the other */
        return 0;
    }

    /* show our suggested topic list */
    showSuggestedTopicList(lst, asker, askee, explicit)
    {
        /* get the asking actor's scope list for use later */
        scopeList = asker.scopeList();
        
        /* remove items that have redundant list groups and full names */
        for (local i = 1, local len = lst.length() ; i <= len ; ++i)
        {
            local a = lst[i];
            
            /* check for redundant elements */
            for (local j = i + 1 ; j <= len ; ++j)
            {
                local b = lst[j];
                
                /* 
                 *   If item 'a' matches item 'b', and both are active,
                 *   remove item 'b'.  We only need to remove redundant
                 items if they're both active, since inactive items
                 */
                if (a.suggestionGroup == b.suggestionGroup
                    && a.fullName == b.fullName
                    && a.isSuggestionActive(asker, scopeList)
                    && b.isSuggestionActive(asker, scopeList))
                {
                    /* delete item 'b' from the list */
                    lst.removeElementAt(j);

                    /* adjust our indices for the deletion */
                    --j;
                    --len;
                }
            }
        }

        /* show our list */
        new SuggestedTopicLister(asker, askee, explicit)
            .showList(asker, nil, lst, 0, 0, nil, nil);
    }

    /*
     *   Flag: this database level should limit topic suggestions (for the
     *   TOPICS and TALK TO commands) to its own topics, excluding any
     *   topics inherited from the "broader" context.  If this property is
     *   set to true, then we won't include suggestions from any lower
     *   level of the database hierarchy.  If this property is nil, we'll
     *   also include any topic suggestions from the broader context.
     *   
     *   Topic databases are arranged into a fixed hierarchy for an actor.
     *   At the top level is the current ConvNode object; at the next level
     *   is the ActorState; and at the bottom level is the Actor itself.
     *   So, if the ConvNode's limitSuggestions property is set to true,
     *   then the suggestions for the actor will include ONLY the ConvNode.
     *   If the ConvNode has the property set to nil, but the ActorState
     *   has it set to true, then we'll include the ConvNode and the
     *   ActorState suggestions.
     *   
     *   By default, we set this to nil.  This should usually be set to
     *   true for any ConvNode or ActorState where the NPC won't allow the
     *   player to stray from the subject.  For example, if a ConvNode only
     *   accepts a YES or NO response to a question, then this property
     *   should probably be set to true in the ConvNode, since other
     *   suggested topics won't be accepted as conversation topics as long
     *   as the ConvNode is active.  
     */
    limitSuggestions = nil

    /*
     *   Add a topic to our topic database.  We'll add it to the
     *   appropriate list or lists as indicated in the topic itself.
     *   'topic' is a TopicEntry object.  
     */
    addTopic(topic)
    {
        /* add the topic to each list indicated in the topic */
        foreach (local cur in topic.includeInList)
            addTopicToList(topic, cur);
    }

    /* remove a topic from our topic database */
    removeTopic(topic)
    {
        /* remove the topic from each of its lists */
        foreach (local cur in topic.includeInList)
            removeTopicFromList(topic, cur);
    }

    /* add a suggested topic */
    addSuggestedTopic(topic)
    {
        /* add the topic to our suggestion list */
        addTopicToList(topic, &suggestedTopics);
    }

    /* remove a suggested topic */
    removeSuggestedTopic(topic)
    {
        /* add the topic to our suggestion list */
        removeTopicFromList(topic, &suggestedTopics);
    }

    /*
     *   Add a topic to the given topic list.  The topic list is given as a
     *   property point; for example, we'd specify &askTopics to add the
     *   topic to our ASK list. 
     */
    addTopicToList(topic, listProp)
    {
        /* if we haven't created this topic list vector yet, create it now */
        if (self.(listProp) == nil)
            self.(listProp) = new Vector(8);

        /* add the topic */
        self.(listProp).append(topic);
    }

    /* remove a topic from the given topic list */
    removeTopicFromList(topic, listProp)
    {
        /* if the list exists, remove the topic from it */
        if (self.(listProp) != nil)
            self.(listProp).removeElement(topic);
    }

    /*
     *   Our list of suggested topics.  These are SuggestedTopic objects
     *   that describe things that another actor wants to ask or tell this
     *   actor about.  
     */
    suggestedTopics = nil

    /*
     *   Get the "owner" of the topics in this database.  The meaning of
     *   "owner" varies according to the topic database type; for actor
     *   topic databases, for example, this is the actor.  Generally, the
     *   owner is the object being queried about the topic, from the
     *   player's perspective.  Each type of database should define this
     *   method to return the appropriate object.  
     */
    getTopicOwner() { return nil; }
;

/*
 *   A TopicDatabase for an Actor.  This is used not only directly for an
 *   Actor but also for an actor's sub-databases, in ActorState and
 *   ConvNode.
 *   
 *   Actor topic databases field queries for the various types of
 *   topic-based interactions an actor can participate in: ASK, TELL, SHOW,
 *   GIVE, and so on.
 *   
 *   Each actor has its own topic database, which means each actor can have
 *   its own set of responses.  Actor states can also have their own
 *   separate topic databases; this makes it easy to make an actor's
 *   response to a particular question vary according to the actor's state.
 *   Conversation nodes can also have their own separate databases, which
 *   allows for things like threaded conversations.
 */
class ActorTopicDatabase: TopicDatabase
    /*
     *   Initiate conversation on the given simulation object.  If we can
     *   find an InitiateTopic matching the given object, we'll show its
     *   topic response and return true; if we can't find a topic to
     *   initiate, we'll simply return nil.  
     */
    initiateTopic(obj)
    {
        /* find an initiate topic for the given object */
        if (handleTopic(gPlayerChar, obj, initiateConvType, nil))
        {
            /* 
             *   we handled the topic, so note that we're in conversation
             *   with the player character now 
             */
            getTopicOwner().noteConversation(gPlayerChar);

            /* indicate that we found a topic to initiate */
            return true;
        }

        /* we didn't find a topic to initiate */
        return nil;
    }

    /* show a topic response */
    showTopicResponse(fromActor, topic, resp)
    {
        local actor = getTopicOwner();
        local newNode;

        /* 
         *   note whether the response is conversational - we need to do
         *   this ahead of time, since invoking the response can sometimes
         *   have the side effect of changing the response's status
         */
        local isConv = resp.isConversational;
        
        /* tell the conversation manager we're starting a response */
        conversationManager.beginResponse(actor);
            
        /* let the response object handle it */
        resp.handleTopic(fromActor, topic);

        /* 
         *   By default, after showing a response, we want to leave the
         *   conversation node tree entirely if we didn't explicitly set
         *   the next node in the course of the response.  So, set the
         *   default new node to 'nil'.  However, if the topic is
         *   non-conversational, it shouldn't affect the conversation
         *   thread at all, so leave the current node unchanged.  
         */
        if (isConv)
            newNode = nil;
        else
            newNode = actor.curConvNode;

        /* tell the conversation manager we're done with the response */
        conversationManager.finishResponse(actor, newNode);
    }

    /* 
     *   Our 'ask about', 'ask for', 'tell about', 'give', 'show',
     *   miscellaneous, command, and self-initiated topic databases - these
     *   are vectors we initialize as needed.  Since every actor and every
     *   actor state has its own separate topic database, it's likely that
     *   the bulk of these databases will be empty, so we don't bother even
     *   creating a vector for a topic list until the first topic is added.
     *   This means we have to be able to cope with these being nil
     *   anywhere we use them.  
     */
    askTopics = nil
    askForTopics = nil
    tellTopics = nil
    showTopics = nil
    giveTopics = nil
    miscTopics = nil
    commandTopics = nil
    initiateTopics = nil

    /* our special command database */
    specialTopics = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   A "suggested" topic.  These provide suggestions for things the player
 *   might want to ASK or TELL another actor about.  At certain times
 *   (specifically, when starting a conversation with HELLO or TALK TO, or
 *   when the player enters a TOPICS command to explicitly ask for a list
 *   of topic suggestions), we'll look for these objects in the actor or
 *   actor state for the actor to whom we're talking.  We'll show a list
 *   of each currently active suggestion we find.  This gives the player
 *   some guidance of what to talk about.  For example:
 *   
 *   >talk to bob
 *.  "Excuse me," you say.
 *   
 *   Bob looks up from his newspaper.  "Yes?  Oh, you again."
 *   
 *   (You'd like to ask him about the black book, the candle, and the
 *   bell, and tell him about the crypt.)
 *   
 *   Topic suggestions are entirely optional.  Some authors don't like the
 *   idea, since they think it's too much like a menu system, and just
 *   gives away the solution to the game.  If you don't want to have
 *   anything to do with topic suggestions, we won't force you - simply
 *   don't define any SuggestedTopic objects, and the library will never
 *   offer suggestions and will even disable the TOPICS command.
 *   
 *   If you do want to use topic suggestions, the easiest way to use this
 *   class is to combine it using multiple inheritance with a TopicEntry
 *   object.  You just have to add SuggestedTopic to the superclass list
 *   for your topic entry object, and give the suggested topic a name
 *   string (using a property and format defined by the language-specific
 *   library) to display in suggestions lists.  Doing this, the suggestion
 *   will automatically be enabled whenever the topic entry is available,
 *   and will automatically be removed from the suggestions when the topic
 *   is invoked in conversation (in other words, we'll only suggest asking
 *   about the topic until it's been asked about once).
 *   
 *   Topic suggestions can be associated with an actor or an actor state;
 *   these are topics that a given character would like to talk to the
 *   associated actor about.  The association is a bit tricky: suggested
 *   topic objects are stored with the actor being *talked to*.  For
 *   example, if we want to suggest topics that the player character might
 *   want to ASK BILL ABOUT, we store these suggestions with *Bill*.  We
 *   do NOT store the suggestions with the player character.  This might
 *   seem backwards at first glance, since fundamentally the suggestions
 *   belong in the player character's "brain" - they are, after all,
 *   things the player character wants to talk about.  In practice,
 *   though, there are two things that make it easier to keep the
 *   information with the character being asked.  First, in most games,
 *   there's just one player character, so one of the two actors in each
 *   association will always be the player character; by storing the
 *   objects with the NPC, we can just let the PC be assumed as the other
 *   actor as a default, saving us some typing that would be necessary if
 *   we had to specify each object in the other direction.  Second, we
 *   keep the *response* objects associated with the character being asked
 *   - that association is intuitive, at least.  The thing is, we can
 *   usually combine the suggestion and response into a single object,
 *   saving another bunch of typing; if we didn't keep the suggestion with
 *   the character being asked, we couldn't combine the suggestions and
 *   responses this way, since they'd have to be associated with different
 *   actors.  
 */
class SuggestedTopic: object
    /*
     *   The name of the suggestion.  The rules for setting this vary by
     *   language; in the English version, we'll display the fullName when
     *   we show a stand-alone item, and the groupName when we appear in a
     *   list group (such as a group of ASK ABOUT or TELL ABOUT
     *   suggestions).
     *   
     *   In English, the fullName should be suitable for use after
     *   'could': "You could <fullName>, <fullName>, or <fullName>".
     *   
     *   In English, the phrasing where the 'name' property is used
     *   depends on the specific subclass, but it should usually be a
     *   qualified noun phrase (that is, it should include a qualifier
     *   such as "a" or "the" or a possessive).  For ASK and TELL, for
     *   example, the 'name' should be suitable for use after ABOUT: "You
     *   could ask him about <the lighthouse>, <Bob's black book>, or <the
     *   weather>."
     *   
     *   By default, we'll walk up our 'location' tree looking for another
     *   suggested topic; if we find one, we'll use its corresponding name
     *   values.  
     */
    fullName = (fromEnclosingSuggestedTopic(&fullName, ''))
    name = (fromEnclosingSuggestedTopic(&name, ''))

    /*
     *   Our associated topic.  In most cases, this will be initialized
     *   automatically: if this suggested topic object is also a
     *   TopicEntry object (using multiple inheritance), we'll set this
     *   during start-up to 'self', or if our location is a TopicEntry,
     *   we'll set this to our location.  This only needs to be
     *   initialized manually if neither of those conditions is true.  
     */
    associatedTopic = nil

    /*
     *   Set the location to the actor to ask or tell about this topic.
     *   This is the target of the ASK ABOUT or TELL ABOUT command, NOT
     *   the actor who's doing the asking.  This can also be set to a
     *   TopicEntry object, in which case we'll be associated with the
     *   actor with which the topic entry is associated, and we'll also
     *   automatically tie the topic entry to this suggestion.
     *   
     *   Because we're using the location property, you can use the '+'
     *   notation to add a suggested topic to the target actor, state
     *   objects, or topic entry.  
     */
    location = nil

    /*
     *   The actor who *wants* to ask or tell about this topic.  Our
     *   location property gives the actor to be asked or told, because
     *   we're associated with the target actor - the same actor who has
     *   the TopicEntry information for the topic.  This property, in
     *   contrast, gives the actor who's doing the asking.
     *   
     *   By default, we return the player character; in most cases, you
     *   won't have to override this.  In most games, only the player
     *   character uses the suggested topic mechanism, because there's no
     *   reason to suggest topics for NPC's - they're just automata, after
     *   all, so if we want them to ask something, we can just program
     *   them to ask it directly.  Also, most games have only one player
     *   character.  Games that meet these criteria won't ever have to
     *   override this.  If you do have multiple player characters, you'll
     *   probably want to override this for each suggested topic to
     *   indicate which character wants to ask about the topic, as the
     *   different player characters might have different things they'd
     *   want to talk about.  
     */
    suggestTo = (gPlayerChar)

    /* the ListGroup with which we're to list this suggestion */
    suggestionGroup = []

    /* find the nearest enclosing SuggestedTopic parent */
    findEnclosingSuggestedTopic()
    {
        /* walk up our location list */
        for (local loc = location ; loc != nil ; loc = loc.location)
        {
            /* if this is a suggested topic, it's what we're looking for */
            if (loc.ofKind(SuggestedTopic))
                return loc;
        }

        /* didn't find anything */
        return nil;
    }

    /* find the outermost enclosing SuggestedTopic parent */
    findOuterSuggestedTopic()
    {
        local outer;
        
        /* walk up our location list */
        for (local loc = self, outer = nil ; loc != nil ; loc = loc.location)
        {
            /* if this is a suggested topic, it's the outermost so far */
            if (loc.ofKind(SuggestedTopic))
                outer = loc;
        }

        /* return the outermost suggested topic we found */
        return outer;
    }

    /* 
     *   get a property from the nearest enclosing SuggestedTopic, or
     *   return the given default value if there is no enclosing
     *   SuggestedTopic 
     */
    fromEnclosingSuggestedTopic(prop, defaultVal)
    {
        /* look for the nearest enclosing suggested topic */
        local enc = findEnclosingSuggestedTopic();

        /* 
         *   return the desired property from the enclosing suggested
         *   topic object if we found one, or the default if there is no
         *   enclosing object 
         */
        return (enc != nil ? enc.(prop) : defaultVal);
    }

    /*
     *   Should we suggest this topic to the given actor?  We'll return
     *   true if the actor is the same actor for which this suggestion is
     *   intended, and the associated topic entry is currently active, and
     *   we haven't already satisfied our curiosity about the topic.  
     */
    isSuggestionActive(actor, scopeList)
    {
        /* 
         *   Check to see if this is our target actor; that the associated
         *   topic itself is active; that our curiosity hasn't already been
         *   satisfied; and that it's at least possible to match the
         *   associated topic right now.  If all of these conditions are
         *   met, we can make this suggestion.  
         */
        return (actor == suggestTo
                && associatedTopicIsActive()
                && associatedTopicCanMatch(actor, scopeList)
                && !curiositySatisfied);
    }

    /* 
     *   The number of times to suggest asking about our topic.  When
     *   we've asked about our associated topic this many times, we'll
     *   have satisfied our curiosity.  In most cases, we'll only want to
     *   suggest a topic until it's asked about once, since most topics
     *   only have a single meaningful response, so we'll use 1 as the
     *   default.  This should be overridden in cases where a topic will
     *   reveal more information when asked several times.  If this is
     *   nil, it means that there's no limit to the number of times to
     *   suggest asking about this.  
     */
    timesToSuggest = 1

    /* 
     *   Have we satisfied our curiosity about this topic?  Returns true
     *   if so, nil if not.  We'll never suggest a topic when this returns
     *   true, because this means that the player no longer feels the need
     *   to ask about the topic.
     */
    curiositySatisfied = (timesToSuggest != nil
                          && associatedTopicTalkCount() >= timesToSuggest)

    /* initialize - this is called automatically during pre-initialization */
    initializeSuggestedTopic()
    {
        /* if we have a location, link up with our location */
        if (location != nil)
            location.addSuggestedTopic(self);

        /* 
         *   if we're also a TopicEntry (using multiple inheritance), then
         *   we are our own associated topic object 
         */
        if (ofKind(TopicEntry))
            associatedTopic = self;
    }

    /*
     *   Methods that rely on the associated topic.  We isolate these in a
     *   few methods here so that the rest of class doesn't depend on the
     *   exact nature of our topic association.  In particular, this allows
     *   for subclasses that don't have an associated topic at all, or that
     *   have multiple associated topics.  Subclasses with specialized
     *   topic relationships can simply override these methods to define
     *   these methods appropriately.  
     */

    /* is the associated topic active? */
    associatedTopicIsActive() { return associatedTopic.checkIsActive(); }

    /* get the number of previous invocations of the associated topic */
    associatedTopicTalkCount() { return associatedTopic.talkCount; }

    /* is it possible to match the associated topic? */
    associatedTopicCanMatch(actor, scopeList)
        { return associatedTopic.isMatchPossible(actor, scopeList); }

    /* 
     *   Note that we're being shown in a topic inventory listing.  By
     *   default, we don't do anything here, but subclasses can use this to
     *   do any extra work they want to do on being listed.  
     */
    noteSuggestion() { }
;

/*
 *   A suggested topic that applies to an entire AltTopic group.
 *   
 *   Normally, a suggestion is tied to an individual TopicEntry.  This
 *   means that when a topic has several AltTopic alternatives, each
 *   AltTopic can be its own separate, independent suggestion.  A
 *   particular alternative can be a suggestion or not, independently of
 *   the other alternatives for the same TopicEntry.  Since each AltTopic
 *   is a separate suggestion, asking about one of the alternatives won't
 *   have any effect on the "curiosity" about the other alternatives - in
 *   other words, the other alternatives will be separately suggested when
 *   they become active.
 *   
 *   In many cases, it's better for an entire set of alternatives to be
 *   treated as a single suggested topic.  That is, we want to suggest the
 *   topic when ANY of the alternatives is active, and asking about any one
 *   of the alternatives will satisfy the PC's curiosity for ALL of the
 *   alternatives.  This sort of arrangement is usually better for cases
 *   where the conditions that trigger the different alternatives aren't
 *   things that ought to make the PC think to ask the same question again.
 *   
 *   Use this class by associating it with the *root* TopicEntry of the
 *   group of alternatives.  You can do this most simply by mixing this
 *   class into the superclass list of the root TopicEntry:
 *   
 *.  + AskTellTopic, SuggestedTopicTree, SuggestedAskTopic
 *.     // ...
 *.  ;
 *   ++ AltTopic ... ;
 *   ++ AltTopic ... ;
 *   
 *   This makes the entire group of AltTopics part of the same suggestion.
 *   Note that you must *also* include SuggestedAsk, SuggestedTellTopic, or
 *   one of the other specialized types among the superclass, to indicate
 *   which kind of suggestion this is.  
 */
class SuggestedTopicTree: SuggestedTopic
    /* is the associated topic active? */
    associatedTopicIsActive()
    {
        /* the topic is active if anything in the AltTopic group is active */
        return associatedTopic.anyAltIsActive;
    }

    /* get the number of previous invocations of the associated topic */
    associatedTopicTalkCount()
    {
        /* return the number of invocations of any alternative */
        return associatedTopic.altTalkCount;
    }
;

/* 
 *   A suggested ASK ABOUT topic.  We'll list ASK ABOUT topics together in
 *   a subgroup ("you'd like to ask him about the book, the candle, and
 *   the bell...").  
 */
class SuggestedAskTopic: SuggestedTopic
    suggestionGroup = [suggestionAskGroup]
;

/*
 *   A suggested TELL ABOUT topic.  We'll list TELL ABOUT topics together
 *   in a subgroup. 
 */
class SuggestedTellTopic: SuggestedTopic
    suggestionGroup = [suggestionTellGroup]
;

/*
 *   A suggested ASK FOR topic.  We'll list ASK FOR topics together as a
 *   group. 
 */
class SuggestedAskForTopic: SuggestedTopic
    suggestionGroup = [suggestionAskForGroup]
;

/*
 *   A suggested GIVE TO topic. 
 */
class SuggestedGiveTopic: SuggestedTopic
    suggestionGroup = [suggestionGiveGroup]
;

/*
 *   A suggested SHOW TO topic. 
 */
class SuggestedShowTopic: SuggestedTopic
    suggestionGroup = [suggestionShowGroup]
;

/*
 *   A suggested YES/NO topic 
 */
class SuggestedYesTopic: SuggestedTopic
    suggestionGroup = [suggestionYesNoGroup]
;
class SuggestedNoTopic: SuggestedTopic
    suggestionGroup = [suggestionYesNoGroup]
;

/* ------------------------------------------------------------------------ */
/*
 *   A conversation node.  Conversation nodes are supplemental topic
 *   databases that represent a point in time in a conversation - a
 *   particular context that arises from what came immediately before in
 *   the conversation.  A conversation node is used to set up a group of
 *   special responses that make sense only in a momentary context within a
 *   conversation.
 *   
 *   A ConvNode object must be nested (via the 'location' property) within
 *   an actor or an ActorState.  This is how we associate the ConvNode with
 *   its actor.  Note that putting a ConvNode inside an ActorState doesn't
 *   do anything different from putting the node directly inside the
 *   ActorState's actor - we allow it only for convenience, to allow
 *   greater flexibility arranging source code.  
 */
class ConvNode: ActorTopicDatabase
    /*
     *   Every ConvNode must have a name property.  This is a string
     *   identifying the object.  Use this name string instead of a regular
     *   object name (so ConvNode instances can essentially always be
     *   anonymous, as far as the compiler is concerned).  This string is
     *   used to find the ConvNode in the master ConvNode database
     *   maintained in the conversationManager object.
     *   
     *   A ConvNode name should be unique with respect to all other
     *   ConvNode objects - no two ConvNode objects should have the same
     *   name string.  Other than this, the name strings are arbitrary.
     *   (However, they shouldn't contain any '>' characters, because this
     *   would prevent them from being used in <.convnode> tags, which is
     *   the main place ConvNode's are usually used.)  
     */
    name = ''

    /*
     *   Is this node "sticky"?  If so, we'll stick to this node if we
     *   show a response that doesn't set a new node.  By default, we're
     *   not sticky, so if we show a response that doesn't set a new node
     *   and doesn't use a <.convstay> tag, we'll simply forget the node
     *   and set the actor to no current ConvNode.
     *   
     *   Sticky nodes are useful when you want the actor to stay
     *   on-subject even when the player digresses to talk about other
     *   things.  This is useful when the actor has a particular thread
     *   they want to drive the conversation along.  
     */
    isSticky = nil

    /*
     *   Show our NPC-initiated greeting.  This is invoked when our actor's
     *   initiateConversation() method is called to cause our actor to
     *   initiate a conversation with the player character.  This method
     *   should show what our actor says to initiate the conversation.  By
     *   default, we'll invoke our npcGreetingList's script, if the
     *   property is non-nil.
     *   
     *   A greeting should always be defined for any ConvNode that's used
     *   in an initiateConversation() call.
     *   
     *   To define a greeting when defining a ConvNode, you can override
     *   this method with a simple double-quoted string message, or you can
     *   define an npcGreetingList property as an EventList of some kind.  
     */
    npcGreetingMsg()
    {
        /* if we have an npcGreetingList property, invoke the script */
        if (npcGreetingList != nil)
            npcGreetingList.doScript();
    }

    /* an optional EventList containing our NPC-initiated greetings */
    npcGreetingList = nil

    /*
     *   Our NPC-initiated conversation continuation message.  This is
     *   invoked on each turn (during the NPC's takeTurn() daemon
     *   processing) that we're in this conversation node and the player
     *   character doesn't do anything conversational.  This allows the NPC
     *   to carry on the conversation of its own volition.  Define this as
     *   a double-quoted string if you want the NPC to say something to
     *   continue the conversation.  
     */
    npcContinueMsg = nil

    /* 
     *   An optional EventList containing NPC-initiated continuation
     *   messages.  You can define an EventList here instead of defining
     *   npcContinueMsg, if you want more than one continuation message.  
     */
    npcContinueList = nil

    /*
     *   Flag: automatically show a topic inventory on activating this
     *   conversation node.  Some conversation nodes have sufficiently
     *   obscure entries that it's desirable to show a topic inventory
     *   automatically when the node becomes active.
     *   
     *   By default, we automatically show a topic inventory if the node
     *   contains an active SpecialTopic entry.  Since special topics are
     *   inherently obscure, in that they use non-standard commands, we
     *   always want to show topics when one of these becomes active.  
     */
    autoShowTopics()
    {
        /* if we have an active special topic, show the topic inventory */
        return (specialTopics != nil
                && specialTopics.indexWhich({x: x.checkIsActive()}) != nil);
    }

    /* our NPC is initiating a conversation starting with this node */
    npcInitiateConversation()
    {
        local actor = getActor();
        
        /* tell the conversation manager we're the actor who's talking */
        conversationManager.beginResponse(actor);

        /* note that we're in conversation with the player character now */
        getActor().noteConversation(gPlayerChar);
        
        /* show our NPC greeting */
        npcGreetingMsg();

        /* look for an ActorHelloTopic within the node */
        handleTopic(gPlayerChar, actorHelloTopicObj, helloConvType, nil);

        /* end the response, staying in the current ConvNode by default */
        conversationManager.finishResponse(actor, self);
    }

    /* 
     *   Continue the conversation of the NPC's own volition.  Returns
     *   true if we displayed anything, nil if not. 
     */
    npcContinueConversation()
    {
        local actor = getActor();
        local disp;
        
        /* tell the conversation manager we're starting a response */
        conversationManager.beginResponse(actor);

        /* show our text, watching to see if we generate any output */
        disp = outputManager.curOutputStream.watchForOutput(function()
        {
            /* 
             *   if we have a continuation list, invoke it; otherwise if we
             *   have a continuation message, show it; otherwise, just
             *   return nil to let the caller know we have nothing to add 
             */
            if (npcContinueList != nil)
                npcContinueList.doScript();
            else
                npcContinueMsg;
        });

        /* end the response, staying in the current ConvNode by default */
        conversationManager.finishResponse(actor, self);

        /* 
         *   if we actually said anything, note that we're in conversation
         *   with the player character 
         */
        if (disp)
            getActor().noteConversation(gPlayerChar);

        /* return the display indication */
        return disp;
    }

    /* our actor is our location, or our location's actor */
    getActor()
    {
        /* if our location is an actor state, return the state's actor */
        if (location.ofKind(ActorState))
            return location.getActor();

        /* otherwise, our location must be our actor */
        return location;
    }

    /* our actor is the "owner" of our topics */
    getTopicOwner() { return getActor(); }

    /*
     *   Handle a conversation topic.  The actor state object will call
     *   this to give the ConvNode the first crack at handling a
     *   conversation command.  We'll return true if we handle the command,
     *   nil if not.  Our default handling is to look up the topic in the
     *   given database list property, and handle it through the TopicEntry
     *   we find there, if any.  
     */
    handleConversation(otherActor, topic, convType, path)
    {
        /* try handling it, returning the handled/not-handled result */
        return handleTopic(otherActor, topic, convType, path);
    }

    /*
     *   Can we end the conversation?  If so, return true; our caller will
     *   invoke our endConversation() to let us know that the conversation
     *   is over.
     *   
     *   To prevent the conversation from ending, simply return nil.
     *   
     *   In most cases, you won't want to force the conversation to keep
     *   going without any comment.  Instead, you'll want to display some
     *   message to let the player know what's going on - something like
     *   "Hey! We're not through here!"  If you do display a message, then
     *   rather than returning nil, return the special value blockEndConv -
     *   this tells the caller that the actor said something, so the caller
     *   will call noteConvAction() to prevent further generated
     *   conversation output on this same turn.
     *   
     *   'reason' gives the reason the conversation is ending, as an
     *   endConvXxx enum code.  
     */
    canEndConversation(actor, reason) { return true; }

    /*
     *   Receive notification that our actor is ending a stateful
     *   conversation.  This is called before the normal
     *   InConversationState disengagement operations.  'reason' is one of
     *   the endConvXxx enums, indicating why the conversation is ending.
     *   
     *   Instances can override this for special behavior on terminating a
     *   conversation.  For example, an actor who just asked a question
     *   could say something to indicate that the other actor is being
     *   rude.  By default, we do nothing.
     *   
     *   Note that there's no way to block the ending of the conversation
     *   here.  If you want to prevent the conversation from ending, use
     *   canEndConversation() instead.  
     */
    endConversation(actor, reason) { }

    /*
     *   Process a special command.  Check the given command line string
     *   against all of our topics, and see if we have a match to any topic
     *   that takes a special command syntax.  If we find a matching
     *   special topic, we'll note the match, and turn the command into our
     *   secret internal pseudo-command "XSPCLTOPIC".  That command will
     *   then go through the parser, which will recognize it and process it
     *   using the normal conversational mechanisms, which will find the
     *   SpecialTopic we noted earlier (in this method) and display its
     *   response.
     *   
     *   'str' is the original input string, exactly as entered by the
     *   player, and 'procStr' is the "processed" version of the input
     *   string.  The nature of the processing varies by language, but
     *   generally this involves things like removing punctuation marks and
     *   any "noise words" that don't usually change the meaning of the
     *   input, at least for the purposes of matching a special topic.  
     */
    processSpecialCmd(str, procStr)
    {
        local match;
        local cnt;

        /* we don't have an active special topic yet */
        activeSpecialTopic = nil;

        /* 
         *   if we have no special topics, there's definitely no special
         *   processing we need to do 
         */
        if (specialTopics == nil)
            return str;
        
        /* scan our special topics for a match */
        cnt = 0;
        foreach (local cur in specialTopics)
        {
            /* if this one is active, and it matches the string, note it */
            if (cur.checkIsActive() && cur.matchPreParse(str, procStr))
            {
                /* remember it as the last match */
                match = cur;

                /* count the match */
                ++cnt;
            }
        }

        /*
         *   If we found exactly one match, then activate it.  If we found
         *   zero or more than one, ignore any special topics and proceed
         *   on the assumption that this is a normal command.  (We ignore
         *   ambiguous matches because this probably means that the entire
         *   command is some very common word that happens to be acceptable
         *   as a keyword in one or more of our matches.  In these cases,
         *   the common word was probably meant as an ordinary command,
         *   since the player would likely have been more specific if a
         *   special topic were really desired.)  
         */
        if (cnt == 1)
        {
            /* 
             *   remember the active SpecialTopic - we'll use this memory
             *   to find it again when we get through the full command
             *   processing 
             */
            activeSpecialTopic = match;

            /* 
             *   Change the command to our special internal pseudo-command
             *   that triggers the active special topic.  Include the
             *   original string as a literal phrase, enclosed in double
             *   quotes and specially coded to ensure that the tokenizer
             *   doesn't become confused by any embedded quotes.  
             */
            return 'xspcltopic "' + SpecialTopicAction.encodeOrig(str) + '"';
        }
        else
        {
            /* proceed, treating the original input as an ordinary command */
            return str;
        }
    }

    patWhitespace = static new RexPattern('<space>+')
    patDelim = static new RexPattern('<punct|space>')

    /*
     *   Handle an XSPCLTOPIC command from the given actor.  This is part
     *   two of the two-phase processing of SpecialTopic matches.  Our
     *   pre-parser checks each SpecialTopic's custom syntax for a match
     *   to the player's text input, and if it finds a match, it sets our
     *   activeSpecialTopic property to the matching SpecialTopic, and
     *   changes the user's command to XSPCLTOPIC for processing by the
     *   regular parser.  The regular parser sees the XSPCLTOPIC command,
     *   which is a valid verb that calls the issuing actor's
     *   saySpecialTopic() routine, which in turn forwards the request to
     *   the issuing actor's interlocutor's current conversation node -
     *   which is to say, 'self'.  We complete the two-step procedure by
     *   going back to the active special topic object that we previously
     *   noted and showing its response.  
     */
    saySpecialTopic(fromActor)
    {
        /* make sure we have an active special topic object */
        if (activeSpecialTopic != nil)
        {
            local actor = getTopicOwner();
            
            /* tell the conversation manager we're starting a response */
            conversationManager.beginResponse(actor);

            /* let the SpecialTopic handle the response */
            activeSpecialTopic.handleTopic(fromActor, nil);

            /* 
             *   Tell the conversation manager we're done.  By default, we
             *   want to leave the conversation tree entirely, so set the
             *   new default node to 'nil'.  
             */
            conversationManager.finishResponse(actor, nil);

            /* that wraps things up for the active special topic */
            activeSpecialTopic = nil;
        }
        else
        {
            /* 
             *   There is no active special topic, so the player must have
             *   typed in the XSPCLTOPIC command explicitly - if we got
             *   here through the normal two-step procedure then this
             *   property would not be nil.  Politely decline the command,
             *   since it's not for the player's direct use.  
             */
            gLibMessages.commandNotPresent;
        }
    }

    /*
     *   The active special topic.  This is the SpecialTopic object that
     *   we matched during pre-parsing, so it's the one whose response we
     *   wish to show while processing the command we pre-parsed.  
     */
    activeSpecialTopic = nil

    /*
     *   Note that we're becoming active, with a reason code.  Our actor
     *   will call this method when we're becoming active, as long as we
     *   weren't already active.
     *   
     *   'reason' is a string giving a reason code for why we're being
     *   called.  For calls from the library, this will be one of these
     *   codes:
     *   
     *    'convnode' - processing a <.convnode> tag
     *   
     *    'convend' - processing a <.convend> tag
     *   
     *    'initiateConversation' - a call to Actor.initiateConversation()
     *   
     *    'endConversation' - a call to Actor.endConversation()
     *   
     *   The reason code is provided so that the node can adapt its action
     *   for different trigger conditions, if desired.  By default, we
     *   ignore the reason code and just call the basic noteActive()
     *   method.  
     */
    noteActiveReason(reason)
    {
        noteActive();
    }
    
    /*
     *   Note that we're becoming active, with a reason code.  Our actor
     *   will call this method when we're becoming active, as long as we
     *   weren't already active.
     *   
     *   Note that if you want to adapt the method's behavior according to
     *   why the node was activated, you can override noteActiveReason()
     *   instead of this method.  
     */
    noteActive()
    {
        /* if desired, schedule a topic inventory whenever we're activated */
        if (autoShowTopics())
            conversationManager.scheduleTopicInventory();
    }

    /*
     *   Note that we're leaving this conversation node.  This doesn't do
     *   anything by default, but individual instances might find the
     *   notification useful for triggering side effects.  
     */
    noteLeaving() { }
;


/* ------------------------------------------------------------------------ */
/*
 *   Pre-parser for special ConvNode-specific commands.  When the player
 *   character is talking to another character, and the NPC's current
 *   ConvNode includes topics with their own commands, we'll check the
 *   player's input to see if it matches any of these topics.  
 */
specialTopicPreParser: StringPreParser
    doParsing(str, which)
    {
        local actor;
        local node;
        
        /* 
         *   don't handle this on requests for missing literals - these
         *   responses are always interpreted as literal text, so there's
         *   no way this could be a special ConvNode command 
         */
        if (which == rmcAskLiteral)
            return str;

        /* 
         *   if the player character isn't currently in conversation, or
         *   the actor with whom the player character is conversing doesn't
         *   have a current conversation node, there's nothing to do 
         */
        if ((actor = gPlayerChar.getCurrentInterlocutor()) == nil
            || (node = actor.curConvNode) == nil)
            return str;

        /* ask the conversation node to process the string */
        return node.processSpecialCmd(str, processInputStr(str));
    }
    
    /* 
     *   Process the input string, as desired, for special-topic parsing.
     *   This method is for the language module's use; by default, we do
     *   nothing.
     *   
     *   Language modules should override this to remove punctuation marks
     *   and to do any other language-dependent processing to make the
     *   string parsable.  
     */
    processInputStr(str) { return str; }
;

/* ------------------------------------------------------------------------ */
/*
 *   A conversational action type descriptor.  This descriptor is used in
 *   handleConversation() in Actor and ActorState to describe the type of
 *   conversational action we're performing.  The type descriptor object
 *   encapsulates a set of information that tells us how to handle the
 *   action.  
 */
class ConvType: object
    /* 
     *   The unknown interlocutor message property.  This is used when we
     *   try this conversational action without knowing whom we're talking
     *   to.  For example, if we just say HELLO, and there's no one around
     *   to talk to, we'll use this as the default response.  This can be a
     *   library message property, or simply a single-quoted string to
     *   display.  
     */
    unknownMsg = nil

    /*
     *   The TopicDatabase topic-list property.  This is the property of
     *   the TopicDatabase object that we evaluate to get this list of
     *   topic entries to search for a match to the topic.  
     */
    topicListProp = nil

    /* the default response property for this action */
    defaultResponseProp = nil

    /* 
     *   Call the default response property on the given topic database.
     *   This invokes the property given by defaultResponseProp().  We have
     *   both the property and the method to call the property because this
     *   allows us to test for the existence of the property and to call it
     *   with the appropriate argument list. 
     */
    defaultResponse(db, otherActor, topic) { }

    /*
     *   Perform any special follow-up action for this type of
     *   conversational action. 
     */
    afterResponse(actor, otherActor) { }
;

helloConvType: ConvType
    unknownMsg = &sayHelloMsg
    topicListProp = &miscTopics
    defaultResponseProp = &defaultGreetingResponse
    defaultResponse(db, other, topic)
        { db.defaultGreetingResponse(other); }

    /* after an explicit HELLO, show any suggested topics */
    afterResponse(actor, otherActor)
    {
        /* show or schedule a topic inventory, as appropriate */
        conversationManager.showOrScheduleTopicInventory(actor, otherActor);
    }
;

byeConvType: ConvType
    unknownMsg = &sayGoodbyeMsg
    topicListProp = &miscTopics
    defaultResponseProp = &defaultGoodbyeResponse
    defaultResponse(db, other, topic)
        { db.defaultGoodbyeResponse(other); }
;

yesConvType: ConvType
    unknownMsg = &sayYesMsg
    topicListProp = &miscTopics
    defaultResponseProp = &defaultYesResponse
    defaultResponse(db, other, topic)
        { db.defaultYesResponse(other); }
;

noConvType: ConvType
    unknownMsg = &sayNoMsg
    topicListProp = &miscTopics
    defaultResponseProp = &defaultNoResponse
    defaultResponse(db, other, topic)
        { db.defaultNoResponse(other); }
;

askAboutConvType: ConvType
    topicListProp = &askTopics
    defaultResponseProp = &defaultAskResponse
    defaultResponse(db, other, topic)
        { db.defaultAskResponse(other, topic); }
;

askForConvType: ConvType
    topicListProp = &askForTopics
    defaultResponseProp = &defaultAskForResponse
    defaultResponse(db, other, topic)
        { db.defaultAskForResponse(other, topic); }
;

tellAboutConvType: ConvType
    topicListProp = &tellTopics
    defaultResponseProp = &defaultTellResponse
    defaultResponse(db, other, topic)
        { db.defaultTellResponse(other, topic); }
;

giveConvType: ConvType
    topicListProp = &giveTopics
    defaultResponseProp = &defaultGiveResponse
    defaultResponse(db, other, topic)
        { db.defaultGiveResponse(other, topic); }
;

showConvType: ConvType
    topicListProp = &showTopics
    defaultResponseProp = &defaultShowResponse
    defaultResponse(db, other, topic)
        { db.defaultShowResponse(other, topic); }
;

commandConvType: ConvType
    topicListProp = &commandTopics
    defaultResponseProp = &defaultCommandResponse
    defaultResponse(db, other, topic)
        { db.defaultCommandResponse(other, topic); }
;

/* 
 *   This type is for NPC-initiated conversations.  It's not a normal
 *   conversational action, since it doesn't involve handling a player
 *   command, but is usually instead triggered by an agenda item,
 *   takeTurn(), or other background activity.  
 */
initiateConvType: ConvType
    topicListProp = &initiateTopics
;

/* 
 *   CONSULT ABOUT isn't a true conversational action, since it's applied
 *   to inanimate objects (such as books); but it's handled through the
 *   conversation system, so it needs a conversation type object 
 */
consultConvType: ConvType
    topicListProp = &consultTopics
;

/* ------------------------------------------------------------------------ */
/*
 *   A topic database entry.  Actors and actor state objects store topic
 *   databases; a topic database is essentially a set of these entries.
 *   
 *   A TopicEntry can go directly inside an Actor, in which case it's part
 *   of the actor's global set of topics; or, it can go inside an
 *   ActorState, in which case it's part of the state's database and is
 *   only active when the state is active; or, it can go inside a
 *   TopicGroup, which is a set of topics with a common controlling
 *   condition; or, it can go inside a ConvNode, in which case it's in
 *   effect only when the conversation node is active.
 *   
 *   Each entry is a relationship between a topic, which is something that
 *   can come up in an ASK or TELL action, and a handling for the topic.
 *   In addition, each entry determines what kind or kinds of actions it
 *   responds to.
 *   
 *   Note that TopicEntry objects are *not* simulation objects.  Rather,
 *   these are abstract objects; they can be associated with simulation
 *   objects via the matching mechanism, but these are separate from the
 *   actual simulation objects.  The reason for this separation is that a
 *   given simulation object might have many different response - the
 *   response could vary according to who's being asked the question, who's
 *   asking, and what else is happening in the game.
 *   
 *   An entry decides for itself if it matches a topic.  By default, an
 *   entry can match based on either a simulation object, which we'll match
 *   to anything in the topic's "in scope" or "likely" match lists, or
 *   based on a regular expression string, which we'll match to the actual
 *   topic text entered in the player's command.
 *   
 *   An entry can decide how strongly it matches a topic.  The database
 *   will choose the strongest match when multiple entries match the same
 *   topic.  The strength of the match is given by a numeric score; the
 *   higher the score, the stronger the match.  The match strength makes it
 *   easy to specify a hierarchy of topics from specific to general, so
 *   that we provide general responses to general topic areas, but can
 *   still respond to particular topics areas more specifically.  For
 *   example, we might want to provide a specific match to the FROBNOZ
 *   SPELL object, talking about that particular magic spell, but provide a
 *   generic '.* spell' pattern to response to questions about any old
 *   spell.  We'd give the generic pattern a lower score, so that the
 *   specific FROBNOZ SPELL response would win when it matches, but we'd
 *   fall back on the generic pattern in other cases.  
 */
class TopicEntry: object
    /*
     *   My matching simulation object or objects.  This can be either a
     *   single object or a list of objects. 
     */
    matchObj = nil

    /*
     *   Is this topic active?  This can be used to control how an actor
     *   can respond without have to worry about adding and removing topics
     *   manually at key events, or storing the topics in state objects.
     *   Sometimes, it's easier to just put a topic entry in the actor's
     *   database from the start, and test some condition dynamically when
     *   the topic is actually queried.  To do this, override this method
     *   to test the condition that determines when the topic entry should
     *   become active.  We'll never show the topic's response when
     *   isActive returns nil.  By default, we simply return true to
     *   indicate that the topic entry is active.  
     */
    isActive = true

    /*
     *   Flag: we are a "conversational" topic.  This is true by default.
     *   When this is set to nil, a ConversationReadyState will NOT show
     *   its greeting and will not enter its InConversationState to show
     *   this topic entry's response.
     *   
     *   This should be set to nil when the topic entry's response is
     *   non-conversational, in which case a greeting would be
     *   undesirable.  This is appropriate for responses like "You don't
     *   think he'd want to talk about that", where the response indicates
     *   that the player character didn't even ask a question (or
     *   whatever).  
     */
    isConversational = true

    /*
     *   Do we imply a greeting?  By default, all conversational topics
     *   imply a greeting.  We separate this out so that the implied
     *   greeting can be controlled independently of whether or not we're
     *   actually conversational, if desired.  
     */
    impliesGreeting = (isConversational)

    /*
     *   Get the actor associated with the topic, if any.  By default,
     *   we'll return our enclosing database's topic owner, if it's an
     *   actor - in almost all cases, if there's any actor associated with
     *   a topic, it's simply the owner of the database containing the
     *   topic.  
     */
    getActor()
    {
        local owner;

        /* 
         *   if we have an owner, and it's an actor, then it's our
         *   associated actor; otherwise, we don't have any associated
         *   actor 
         */
        if ((owner = location.getTopicOwner()) != nil && owner.ofKind(Actor))
            return owner;
        else
            return nil;
    }

    /*
     *   Determine if this topic is active.  This checks the isActive
     *   property, and also takes into account our relationship to
     *   alternative entries for the topic.  Generally, you should *define*
     *   (override) isActive, and *call* this method.  
     */
    checkIsActive()
    {
        /* 
         *   if our isActive property indicates we're not active, we're
         *   definitely not active, so there's no need to check for an
         *   overriding alternative 
         */
        if (!isActive)
            return nil;

        /* if we have an active nested alternative, it overrides us */
        if (altTopicList.indexWhich({x: x.isActive}) != nil)
            return nil;

        /* ask our container if its topics are active */
        return location.topicGroupActive();
    }

    /*
     *   Check to see if any alternative in the alternative group is
     *   active.  This returns true if we're active or if any of our nested
     *   AltTopics is active.  
     */
    anyAltIsActive()
    {
        /* 
         *   if all topics within our container are inactive, then there's
         *   definitely no active alternative 
         */
        if (!location.topicGroupActive())
            return nil;

        /* 
         *   if we're active, or any of our nested AltTopics is active, our
         *   alternative group is active 
         */
        if (isActive || altTopicList.indexWhich({x: x.isActive}) != nil)
            return true;

        /* we didn't find any active alternatives in the entire group */
        return nil;
    }

    /*
     *   Adjust my score value for any hierarchical adjustments.  We'll add
     *   the score adjustment for each enclosing object.  
     */
    adjustScore(score)
    {
        /* the score is nil, it means there's no match, so don't adjust it */
        if (score == nil)
            return score;

        /* add in the cumulative adjustment from my containers */
        return score + location.topicGroupScoreAdjustment;
    }

    /*
     *   Check to see if we want to defer to the given topic from an
     *   inferior topic database.  By default, we never defer to a topic
     *   from an inferior database: we choose a matching topic from the top
     *   database in the hierarchy where we find a match.
     *   
     *   The database hierarchy, for most purposes, starts with the
     *   ConvNode at the highest level, then the ActorState, then the
     *   Actor.  We search those databases, in that order, and we take the
     *   first match we find.  By default, if there's another match in a
     *   lower-level database, it doesn't matter what its matchScore is: we
     *   always pick the one from the highest-level database where we find
     *   a match.  You can override this method to change this behavior.
     *   
     *   We don't actually define this method here, because the presence of
     *   the method is significant.  If the method isn't defined at all, we
     *   won't bother looking for a possible deferral, saving the trouble
     *   of searching the other databases in the hierarchy.  
     */
    // deferToEntry(other) { return nil; }

    /* 
     *   Our match strength score.  By default, we'll use a score of 100,
     *   which is just an arbitrary base score.  
     */
    matchScore = 100

    /*
     *   The set of database lists we're part of.  This is a list of
     *   property pointers, giving the TopicDatabase properties of the
     *   lists we participate in. 
     */
    includeInList = []

    /*
     *   Our response.  This is displayed when we're the topic entry
     *   selected to handle an ASK or TELL.  Each topic entry must override
     *   this to show our response text (or, alternatively, an entry can
     *   override handleTopic so that it doesn't call this property).  
     */
    topicResponse = ""

    /*
     *   The number of times this topic has invoked by the player.  Each
     *   time the player asks/tells/etc about this topic, we'll increment
     *   this count.  
     */
    talkCount = 0

    /*
     *   The number of times this topic or any nested AltTopic has been
     *   invoked by the player.  Each time the player asks/tells/etc about
     *   this topic OR any of its AltTopic children, we'll increment this
     *   count.  
     */
    altTalkCount = 0

    /* 
     *   the owner of any AltTopic nested within me is the same as my own
     *   topic owner, which we take from our location 
     */
    getTopicOwner()
    {
        if (location != nil)
            return location.getTopicOwner();
        else
            return nil;
    }

    /*
     *   Initialize.  If we have a location property, we'll assume that the
     *   location is a topic database object, and we'll add ourselves to
     *   that database.  
     */
    initializeTopicEntry()
    {
        /* if we have a location, add ourselves to its topic database */
        if (location != nil)
            location.addTopic(self);

        /* sort our list of AltTopic children */
        altTopicList = altTopicList.sort(
            SortAsc, {a, b: a.altTopicOrder - b.altTopicOrder});
    }

    /* add a topic nested within us */
    addTopic(entry)
    {
        /* if we have a location, add the entry to its topic database */
        if (location != nil)
            location.addTopic(entry);
    }

    /*
     *   Add an AltTopic entry.  This is called by our AltTopic children
     *   during initialization; we'll simply add the entry to our list of
     *   AltTopic children.  
     */
    addAltTopic(entry)
    {
        /* add the entry to our list of alternatives */
        altTopicList += entry;
    }

    /* get the topic group score adjustment (for AltTopics nested within) */
    topicGroupScoreAdjustment = (location.topicGroupScoreAdjustment)

    /* check the group isActive status (for AltTopics nested within) */
    topicGroupActive = (location.topicGroupActive)

    /* our list of AltTopic children */
    altTopicList = []

    /* 
     *   Match a topic.  This is abstract in this base class; it must be
     *   defined by each concrete subclass.  This returns nil if there's no
     *   match, or an integer value if there's a match.  The higher the
     *   number's value, the stronger the match.
     *   
     *   This is abstract in the base class because the meaning of 'topic'
     *   varies by subclass, according to which type of command it's used
     *   with.  For example, in ASK and TELL commands, 'topic' is a
     *   ResolvedTopic describing the topic in the player's command; for
     *   GIVE and SHOW commands, it's the resolved simulation object.  
     */
    // matchTopic(fromActor, topic) { return nil; }

    /*
     *   Check to see if a match to this topic entry is *possible* right
     *   now for the given actor.  For most subclasses, this is inherently
     *   imprecise, because the 'match' function simply isn't reversible in
     *   general: to know if we can be matched, we'd have to determine if
     *   there's a non-empty set of possible inputs that can match us.
     *   This method is complementary to matchTopic(), so subclasses must
     *   override with a corresponding implementation.
     *   
     *   'actor' is the actor to whom we're making the suggestion.
     *   'scopeList' is the list of objects that are in scope for the
     *   actor.
     *   
     *   The library only uses this to determine if a suggestion should be
     *   offered.  So, specialized topic instances with non-standard match
     *   rules don't have to worry about this unless they're used as
     *   suggestions, or unless the game itself needs this information for
     *   some other reason.  
     */
    // isMatchPossible(actor, scopeList) { return true; }

    /*
     *   Break a tie among matching topics entries.  The topic database
     *   searcher calls this on each matching topic entry when it finds
     *   multiple entries tied for first place, based on their match
     *   scores.  This gives the entries a chance to figure out which one
     *   is actually the best match for the input, given the other entries
     *   that also matched.
     *   
     *   This method returns a TopicEntry object - one of the objects from
     *   the match list - if it has an opinion as to which one should take
     *   precedence.  It returns nil if it doesn't know or doesn't care.
     *   Returning nil gives the other topics in the match list a chance to
     *   make the selection.  If all of the objects in the list return nil,
     *   the topic database searcher simply picks one of the topic matches
     *   arbitrarily.
     *   
     *   'matchList' is the list of tied TopicEntry objects.  'topic' is
     *   the ResolvedTopic object from the parser, representing the
     *   player's input phrase that we're matching.  'fromActor' is the
     *   actor performing the command.  'toks' is a list of strings giving
     *   the word tokens of the noun phrase.
     *   
     *   The topic database searcher calls this method for each matching
     *   topic entry in the case of a tie, and simply accepts the opinion
     *   of the first one that expresses an opinion by returning a non-nil
     *   value.  There's no voting; whoever happens to get *and use* the
     *   first say also gets the last word.  We expect that this won't be a
     *   problem in practice: when this comes up at all, it's because there
     *   are a couple of closely related topic entries that are active in a
     *   particular context, and you need a special bit of tweaking to pick
     *   the right one for a given input phrase.  Simply pick one of the
     *   involved entries and define this method there.  
     */
    breakTopicTie(matchList, topic, fromActor, toks)
    {
        /* 
         *   we don't have an opinion - defer to the next object in the
         *   list, or allow an arbitrary selection 
         */
        return nil;
    }

    /*
     *   Set pronouns for the topic, if possible.  If the topic corresponds
     *   to a game-world object, then we should set the pronoun antecedent
     *   to the game object.  This must be handled per subclass because of
     *   the range of possible meanings of 'topic'.  
     */
    setTopicPronouns(fromActor, topic) { }

    /*
     *   Handle the topic.  This is called when we find that this is the
     *   best topic entry for the current topic.
     *   
     *   By default, we'll do one of two things:
     *   
     *   - If 'self' inherits from Script, then we'll simply invoke our
     *   doScript() method.  This makes it especially easy to set up a
     *   topic entry that shows a series of responses: just add EventList
     *   or one of its subclasses to the base class list when defining the
     *   topic, and define the eventList property as a list of string
     *   responses.  For example:
     *   
     *.     + TopicEntry, StopEventList @blackBook
     *.        ['<q>What makes you think I know anything about it?</q>
     *.         he says, his voice shaking. ',
     *.         '<q>No! You can\'t make me tell you!</q> he wails. ',
     *.         '<q>All right, fine! I\'ll tell you, but I warn you,
     *.         this is knowledge mortal men were never meant to know.</q> ',
     *.         // and so on
     *.        ]
     *.     ;
     *   
     *   - Otherwise, we'll call our topicResponse property, which should
     *   simply be a double-quoted string to display.  This is the simplest
     *   way to define a topic with just one response.
     *   
     *   Note that 'topic' will vary by subclass, depending on the type of
     *   command used with the topic type.  For example, for ASK and TELL
     *   commands, 'topic' is a ResolvedTopic object; for GIVE and SHOW,
     *   it's a simulation object (i.e., generally a Thing subclass).  
     */
    handleTopic(fromActor, topic)
    {
        /* note the invocation */
        noteInvocation(fromActor);

        /* set pronoun antecedents if possible */
        setTopicPronouns(fromActor, topic);
        
        /* check to see if we're a Script */
        if (ofKind(Script))
        {
            /* we're a Script - invoke our script */
            doScript();
        }
        else
        {
            /* show our simple response string */
            topicResponse;
        }
    }

    /* note that we've been invoked */
    noteInvocation(fromActor)
    {
        /* 
         *   we count as one of the alternatives in our alternative group,
         *   so note the invocation of the group
         */
        noteAltInvocation(fromActor, self);

        /* count the invocation */
        ++talkCount;
    }

    /* 
     *   Note that something in our entire alternative group has been
     *   invoked.  We count as a member of our own group, so this is
     *   invoked when we're invoked; this is also invoked when any AltTopic
     *   child of ours is invoked.  
     */
    noteAltInvocation(fromActor, alt)
    {
        local owner;
        
        /* notify our owner of the topic invocation */
        if ((owner = location.getTopicOwner()) != nil)
            owner.notifyTopicResponse(fromActor, alt);
        
        /* count the alternative invocation */
        ++altTalkCount;
    }

    /*
     *   Add a suggested topic.  A suggested topic can be nested within a
     *   topic entry; doing this associates the suggested topic with the
     *   topic entry, and automatically associates the suggested topic
     *   with the entry's actor or actor state.  
     */
    addSuggestedTopic(t)
    {
        /* 
         *   If the SuggestedTopic is *directly* within us, we're the
         *   SuggestedTopic object's associated TopicEntry.  The nesting
         *   could be deeper, if we have alternative topics nested within
         *   us; in these cases, we're not directly associated with the
         *   suggested topic.  
         */
        if (t.location == self)
            t.associatedTopic = self;

        /* add the suggestion to our location's topic database */
        if (location != nil)
            location.addSuggestedTopic(t);
    }
;

/*
 *   A TopicGroup is an abstract container for a set of TopicEntry objects.
 *   The purpose of the group object is to apply a common "is active"
 *   condition to all of the topics within the group.
 *   
 *   The isActive condition of the TopicGroup is effectively AND'ed with
 *   any other conditions on the nested TopicEntry's.  In other words, a
 *   TopicEntry within the TopicGroup is active if the TopicEntry would
 *   otherwise be acive AND the TopicGroup is active.
 *   
 *   TopicEntry objects are associated with the group via the 'location'
 *   property - set the location of the TopicEntry to point to the
 *   containing TopicGroup.
 *   
 *   You can put a TopicGroup anywhere a TopicEntry could go - directly
 *   inside an Actor, inside an ActorState, or within another TopicGroup.
 *   The topic entries within a topic group act as though they were
 *   directly in the topic group's container.  
 */
class TopicGroup: object
    /* 
     *   The group "active" condition - each instance should override this
     *   to specify the condition that applies to all of the TopicEntry
     *   objects within the group.  
     */
    isActive = true

    /*
     *   The *adjustment* to the match score for topic entries contained
     *   within this group.  This is usually a positive number, so that it
     *   boosts the match strength of the child topics.  
     */
    matchScoreAdjustment = 0

    /* 
     *   the topic owner for any topic entries within the group is the
     *   topic owner taken from the group's own location 
     */
    getTopicOwner() { return location.getTopicOwner(); }

    /* are TopicEntry objects within the group active? */
    topicGroupActive()
    {
        /* 
         *   our TopicEntry objects are active if the group condition is
         *   true and our container's contents are active
         */
        return isActive && location.topicGroupActive();
    }

    /* 
     *   Get my score adjustment.  We'll return our own basic score
     *   adjustment plus the cumulative adjustment for our containers.  
     */
    topicGroupScoreAdjustment = (matchScoreAdjustment
                                 + location.topicGroupScoreAdjustment)

    /* add a topic - we'll simply add the topic directly to our container */
    addTopic(topic) { location.addTopic(topic); }

    /* add a suggested topic - we'll pass this up to our container */
    addSuggestedTopic(topic) { location.addSuggestedTopic(topic); }
;

/*
 *   An alternative topic entry.  This makes it easy to define different
 *   responses to a topic according to the game state; for example, we
 *   might want to provide a different response for a topic after some
 *   event has occurred, so that we can reflect knowledge of the event in
 *   the response.
 *   
 *   A set of alternative topics is sort of like an inverted if-then-else.
 *   You start by defining a normal TopicEntry (an AskTopic, or an
 *   AskTellTopic, or whatever) for the basic response.  Then, you add a
 *   nested AltTopic located within the base topic; you can add another
 *   AltTopic nested within the base topic, and another after that, and so
 *   on.  When we need to choose one of the topics, we'll choose the last
 *   one that indicates it's active.  So, the order of appearance is
 *   essentially an override order: the first AltTopic overrides its parent
 *   TopicEntry, and each subsequent AltTopic overrides its previous
 *   AltTopic.
 *   
 *   + AskTellTopic @lighthouse "It's very tall.";
 *.  ++ AltTopic "Not really..." isActive=(...);
 *.  ++ AltTopic "Well, maybe..." isActive=(...);
 *.  ++ AltTopic "One more thing..." isActive=(...);
 *   
 *   In this example, the response we'll show for ASK ABOUT LIGHTHOUSE will
 *   always be the LAST entry of the group that's active.  For example, if
 *   all of the responses are active except for the very last one, then
 *   we'll show the "Well, maybe" response, because it's the last active
 *   response.  If the main AskTellTopic is active, but none of the
 *   AltTopics are active, we'll show the "It's very tall" main response,
 *   because it's the last element of the group that's active.
 *   
 *   Note that an AltTopic takes its matching information from its parent,
 *   so you don't need to specify a matchObj or any other matching
 *   information in an AltTopic.  You merely need to provide the response
 *   text and the isActive test.  
 */
class AltTopic: TopicEntry
    /* we match if our parent matches, and with the same score */
    matchTopic(fromActor, topic)
        { return location.matchTopic(fromActor, topic); }

    /* we can match if our parent can match */
    isMatchPossible(actor, scopeList)
        { return location.isMatchPossible(actor, scopeList); }

    /* we can match a pre-parse string if our parent can */
    matchPreParse(str, pstr) { return location.matchPreParse(str, pstr); }

    /* set pronouns for the topic */
    setTopicPronouns(fromActor, topic)
        { location.setTopicPronouns(fromActor, topic); }

    /* include in the same lists as our parent */
    includeInList = (location.includeInList)

    /* AltTopic initialization */
    initializeAltTopic()
    {
        /* add myself to our parent's child list */
        if (location != nil)
            location.addAltTopic(self);
    }

    /*
     *   Determine if this topic is active.  An AltTopic is active if its
     *   own isActive indicates true, AND none of its subsequent siblings
     *   are active.  
     */
    checkIsActive()
    {
        /* we can't be active if our own isActive says we're not */
        if (!isActive)
            return nil;

        /* 
         *   Check for any active element after us in the parent's list.
         *   To do this, scan from the end of the parent list backwards,
         *   and look for an element that's active.  If we reach our own
         *   entry, then we'll know that there are no active entries
         *   following us in the list.  Note that we already know we're
         *   active, or we wouldn't have gotten this far, so we can simply
         *   look for the rightmost active element in the list.  
         */
        if (location != nil
            && location.altTopicList.lastValWhich({x: x.isActive}) != self)
        {
            /* 
             *   we found an active element after ourself, so it overrides
             *   us - we're therefore not active 
             */
            return nil;
        }

        /* ask our container if its topics are active */
        return location.topicGroupActive();
    }

    /* take our implied-greeting status from our parent */
    impliesGreeting = (location.impliesGreeting)

    /* take our conversational status from our parent */
    isConversational = (location.isConversational)

    /* 
     *   Our relative order within our parent's list of alternatives.  By
     *   default, we simply return the source file ordering, which ensures
     *   that static AltTopic objects (i.e., those defined directly in
     *   source files, not dynamically created with 'new') will be ordered
     *   just as they're laid out in the source file.  
     */
    altTopicOrder = (sourceTextOrder)

    /* note invocation */
    noteInvocation(fromActor)
    {
        /* count our own invocation */
        ++talkCount;

        /* let our container know its AltTopic child is being invoked */
        if (location != nil)
            location.noteAltInvocation(fromActor, self);
    }

    /* our AltTopic counter is the AltTopic counter for the enclosing topic */
    altTalkCount = (location != nil ? location.altTalkCount : talkCount)
;

/*
 *   A "topic match" topic entry.  This is a topic entry that matches topic
 *   phrases in the grammar.
 *   
 *   Handling topic phrases is a bit tricky, because they can't be resolved
 *   to definitive game-world objects the way ordinary noun phrases can.
 *   Topic phrases can refer to things that aren't physically present, but
 *   which are known to the actor performing the command; they can refer to
 *   abstract Topic objects, that have no physical existence in the game
 *   world at all; and they can ever be arbitrary text that doesn't match
 *   any vocabulary defined by the game.
 *   
 *   Our strategy in matching topics is to first narrow the list down to
 *   the physical and abstract game objects that both match the vocabulary
 *   used in the command and are part of the memory of the actor performing
 *   the command.  That much is handled by the normal topic phrase
 *   resolution rules, and gives us a list of possible matches.  Then,
 *   given this narrowed list of possibilities, we look through the list of
 *   objects that we're associated with; we effectively intersect the two
 *   lists, and if the result is non-empty, we consider it a match.
 *   Finally, we also consider any regular expression that we're associated
 *   with; if we have one, and the topic phrase text in the command matches
 *   the input, we'll consider it a match.  
 */
class TopicMatchTopic: TopicEntry
    /*  
     *   A regular expression pattern that we'll match to the actual topic
     *   text as entered in the command.  If 'matchExactCase' is true,
     *   we'll match the exact text in its original upper/lower case
     *   rendering; otherwise, we'll convert the player input to lower-case
     *   before matching it against the pattern.  In most cases, we'll want
     *   to match the input no matter what combination of upper and lower
     *   case the player entered, so matchExactCase is nil by default.
     *   
     *   Note that both the object (or object list) and the regular
     *   expression pattern can be included for a single topic entry
     *   object.  This allows a topic entry to match several different ways
     *   of entering the topic name, or to match several different topics
     *   with the same response.  
     */
    matchPattern = nil
    matchExactCase = nil

    /* 
     *   Match the topic.  By default, we'll match to either the simulation
     *   object or objects in matchObj, or the pattern in matchPattern.
     *   Note that we always try both ways of matching, so a single
     *   AskTellTopic can define both a pattern and an object list.
     *   
     *   'topic' is a ResolvedTopic object describing the player's text
     *   input and the list of objects that the parser matched to the text.
     *   
     *   Subclasses can override this as desired to use other ways of
     *   matching.  
     */
    matchTopic(fromActor, topic)
    {
        /* 
         *   if we have one or more match objects, try matching to the
         *   topic's best simulation object match 
         */
        if (matchObj != nil)
        {
            /* 
             *   we have a match object or match object list - if it's a
             *   collection, check each element, otherwise just match the
             *   single object 
             */
            if (matchObj.ofKind(Collection))
            {
                /* try matching each object in the list */
                if (matchObj.indexWhich({x: findMatchObj(x, topic)}) != nil)
                    return matchScore;
            }
            else
            {
                /* match the single object */
                if (findMatchObj(matchObj, topic))
                    return matchScore;
            }
        }

        /* 
         *   check for a match to the regular expression pattern, if we
         *   have a pattern AND the resolved topic allows literal matches 
         */
        if (matchPattern != nil && topic.canMatchLiterally())
        {
            local txt;

            /* 
             *   There's no match object; try matching our regular
             *   expression to the actual topic text.  Get the actual text.
             */
            txt = topic.getTopicText();

            /* 
             *   if they don't want an exact case match, convert the
             *   original topic text to lower case 
             */
            if (!matchExactCase)
                txt = txt.toLower();

            /* if the regular expression matches, we match */
            if (rexMatch(matchPattern, txt) != nil)
                return matchScore;
        }

        /* we didn't find a match - indicate this with a nil score */
        return nil;
    }

    /*
     *   Match an individual item from our match list to the given
     *   ResolvedTopic object.  We'll check each object in the resolved
     *   topic's "in scope" and "likely" lists.  
     */
    findMatchObj(obj, rt)
    {
        /* check the "in scope" list */
        if (rt.inScopeList.indexOf(obj) != nil)
            return true;

        /* check the "likely" list */
        return (rt.likelyList.indexOf(obj) != nil);
    }

    /*
     *   It's possible for us to match if any of our matchObj objects are
     *   known to the actor.  If we have no matchObj objects, we must be
     *   matching on a regular expression or on a custom condition, so we
     *   can't speculate on matchability; we'll simply return true in those
     *   cases.  
     */
    isMatchPossible(actor, scopeList)
    {
        /* check what we have in our matchObj */
        if (matchObj == nil)
        {
            /* 
             *   we have no match object, so we must match on a regular
             *   expression or a custom condition; we can't speculate on
             *   our matchability, so just return true as a default 
             */
            return true;
        }
        else if (matchObj.ofKind(Collection))
        {
            /* 
             *   we have a list of match objects - return true if any of
             *   them are known or are currently in scope 
             */
            return (matchObj.indexWhich(
                {x: actor.knowsAbout(x) || scopeList.indexOf(x)}) != nil);
        }
        else
        {
            /* 
             *   we have a single match object - return true if it's known
             *   or it's in scope 
             */
            return (actor.knowsAbout(matchObj)
                    || scopeList.indexOf(matchObj) != nil);
        }
    }

    /* set the topic pronouns */
    setTopicPronouns(fromActor, topic)
    {
        /* check to see what kind of match object we have */
        if (matchObj == nil)
        {
            /* 
             *   no match object, so we must match a regular expression
             *   pattern; this gives us no clue what game object we might
             *   match, so there's nothing we can do here 
             */
        }
        else if (matchObj.ofKind(Collection))
        {
            local lst;
            
            /*
             *   We match a list of objects.  Get the subset of the
             *   in-scope list from the topic that we match.  Consider only
             *   the in-scope items for now, and consider only game-world
             *   objects (Things).  
             */
            lst = matchObj.subset(
                {x: x.ofKind(Thing) && topic.inScopeList.indexOf(x) != nil});

            /* if that didn't turn up anything, consider the likelies, too */
            if (lst.length() == 0)
                lst = matchObj.subset(
                    {x: (x.ofKind(Thing)
                         && topic.likelyList.indexOf(x) != nil)});

            /* 
             *   if that narrows it down to one match, make it the pronoun
             *   antecedent 
             */
            if (lst.length() == 1)
                fromActor.setPronounObj(lst[1]);
        }
        else
        {
            /* 
             *   we match a single object; if it's a game-world object (a
             *   Thing), use it as the pronoun antecedent 
             */
            if (matchObj.ofKind(Thing))
                fromActor.setPronounObj(matchObj);
        }
    }
;

/*
 *   A dual ASK/TELL topic database entry.  This type of topic is included
 *   in both the ASK ABOUT and TELL ABOUT lists.
 *   
 *   Many authors have chosen to treat ASK and TELL as equivalent, or at
 *   least, equivalent for most topics.  Since these verbs only very weakly
 *   suggest what the player character is actually saying, it's frequently
 *   the case that a given topic response makes just as much sense coming
 *   from TELL as from ASK, or vice versa.  In these cases, it's best to
 *   enter the topic under both ASK and TELL; which one the player tries
 *   might simply depend on the player's frame of mind, and they might feel
 *   cheated if one works and the other doesn't in cases where both are
 *   equally valid.
 */
class AskTellTopic: TopicMatchTopic
    /* include me in both the ASK and TELL lists */
    includeInList = [&askTopics, &tellTopics]
;

/*
 *   An ASK ABOUT topic database entry.  This type of topic is included in
 *   the ASK ABOUT list only.  
 */
class AskTopic: AskTellTopic
    includeInList = [&askTopics]
;

/*
 *   A TELL ABOUT topic database entry.  This type of topic entry is
 *   included in the TELL ABOUT list only.  
 */
class TellTopic: AskTellTopic
    includeInList = [&tellTopics]
;

/*
 *   An ASK FOR topic database entry.  This type of topic entry is
 *   included in the ASK FOR list only. 
 */
class AskForTopic: AskTellTopic
    includeInList = [&askForTopics]
;

/*
 *   A combination ASK ABOUT and ASK FOR topic. 
 */
class AskAboutForTopic: AskTellTopic
    includeInList = [&askTopics, &askForTopics]
;

/* 
 *   A combination ASK ABOUT, TELL ABOUT, and ASK FOR topic.  
 */
class AskTellAboutForTopic: AskTellTopic
    includeInList = [&askTopics, &tellTopics, &askForTopics]
;


/*
 *   A base class for topic entries that match simple simulation objects.  
 */
class ThingMatchTopic: TopicEntry
    /*
     *   Match the topic.  We'll match the simulation object in 'obj' to
     *   our matchObj object or list.  
     */
    matchTopic(fromActor, obj)
    {
        /* 
         *   if matchObj is a collection, check each element, otherwise
         *   just match the single object 
         */
        if (matchObj.ofKind(Collection))
        {
            /* try matching each object in the list */
            if (matchObj.indexOf(obj) != nil)
                return matchScore;
        }
        else
        {
            /* match the single object */
            if (matchObj == obj)
                return matchScore;
        }

        /* didn't find a match - indicate this by returning a nil score */
        return nil;
    }

    /*
     *   It's possible for us to match if any of our matchObj objects are
     *   in scope.
     */
    isMatchPossible(actor, scopeList)
    {
        /* check to see what kind of match object we have */
        if (matchObj.ofKind(Collection))
        {
            /* we can match if any of our match objects are in scope */
            return (matchObj.indexWhich({x: scopeList.indexOf(x)}) != nil);
        }
        else
        {
            /* we can match if our single match object is in scope */
            return scopeList.indexOf(matchObj);
        }
    }

    /* set the topic pronouns */
    setTopicPronouns(fromActor, topic)
    {
        /* 
         *   the 'topic' is just an ordinary game object; as long as it's a
         *   Thing, set it as the antecedent 
         */
        if (topic.ofKind(Thing))
            fromActor.setPronounObj(topic);
    }
;

/*
 *   A GIVE/SHOW topic database entry.
 *   
 *   Note that this base class is usable for any command that refers to a
 *   simulation object.  It's NOT suitable for ASK/TELL lists, or for other
 *   commands that refer to topics, since we expect our 'topic' to be a
 *   resolved simulation object.  
 */
class GiveShowTopic: ThingMatchTopic
    /* include me in both the GIVE and SHOW lists */
    includeInList = [&giveTopics, &showTopics]
;

/*
 *   A GIVE TO topic database entry.  This type of topic entry is included
 *   in the GIVE TO list only.  
 */
class GiveTopic: GiveShowTopic
    includeInList = [&giveTopics]
;

/*
 *   A SHOW TO topic database entry.  This type of topic entry is included
 *   in the SHOW TO list only.  
 */
class ShowTopic: GiveShowTopic
    includeInList = [&showTopics]
;

/*
 *   A TopicEntry that can match a Thing or a Topic.  This can be used to
 *   combine ASK/TELL-type responses and GIVE/SHOW-type responses in a
 *   single topic entry. 
 *   
 *   When this kind of topic is used as a suggested topic, note that you
 *   should name the suggestion according to the least restrictive verb.
 *   This is important because the suggestion will be active if any of the
 *   verbs would allow it; to ensure that we suggest a verb that will
 *   actually work, we should thus use the least restrictive verb.  In
 *   practice, this means you should use ASK or TELL as the suggestion
 *   name, because an object merely has to be known to be used as a topic;
 *   it might be possible to ASK/TELL about an object but not GIVE/SHOW the
 *   object, because the object is known but not currently in scope.  
 */
class TopicOrThingMatchTopic: ThingMatchTopic, TopicMatchTopic
    matchTopic(fromActor, obj)
    {
        /* 
         *   if we're being asked to match a ResolvedTopic, use the
         *   inherited TopicMatchTopic handling; otherwise, use the
         *   inherited ThingMatchTopic handling 
         */
        if (obj.ofKind(ResolvedTopic))
            return inherited TopicMatchTopic(fromActor, obj);
        else
            return inherited ThingMatchTopic(fromActor, obj);
    }

    isMatchPossible(actor, scopeList)
    {
        /* if a match is possible from either subclass, allow it */
        return (inherited TopicMatchTopic(actor, scopeList)
                || inherited ThingMatchTopic(actor, scopeList));
    }

    setTopicPronouns(fromActor, obj)
    {
        /* 
         *   if the object is a ResolvedTopic, use the inherited
         *   TopicMatchTopic handling, otherwise use the ThingMatchTopic
         *   handling 
         */
        if (obj.ofKind(ResolvedTopic))
            return inherited TopicMatchTopic(fromActor, obj);
        else
            return inherited ThingMatchTopic(fromActor, obj);
    }
;

/*
 *   A combined ASK/TELL/SHOW topic.  Players will sometimes want to point
 *   something out when it's visible, rather than asking about it; this
 *   allows SHOW TO to be used as a synonym for ASK ABOUT for these cases. 
 */
class AskTellShowTopic: TopicOrThingMatchTopic
    includeInList = [&askTopics, &tellTopics, &showTopics]
;

/*
 *   A combined ASK/TELL/GIVE/SHOW topic.
 */
class AskTellGiveShowTopic: TopicOrThingMatchTopic
    includeInList = [&askTopics, &tellTopics, &giveTopics, &showTopics]
;

/*
 *   A command topic.  This is used to respond to orders given to an NPC,
 *   as in "BOB, GO EAST."  The match object for this kind of topic entry
 *   is an Action class; for example, to create a response to "BOB, LOOK",
 *   we'd create a CommandTopic that matches LookAction.
 *   
 *   If you're designing a CommandTopic for a command can be accepted from
 *   a remote location, such as by telephone, you should be aware that the
 *   command will be running in the NPC's visual sense context.  This means
 *   that if the player character can't see the NPC, the topic result
 *   message will be hidden - the NPC's visual sense context hides all
 *   messages generated while it's in effect if the PC can't see the NPC.
 *   This is usually desirable, since most messages relay visual
 *   information that wouldn't be visible to the player character if the PC
 *   can't see the subject of the message.  However, if you've specifically
 *   designed your CommandTopic to work remotely, this isn't at all what
 *   you want, since you've already taken the remoteness into account in
 *   the message and thus want the message to be displayed after all.  The
 *   way to handle this is to wrap the message in a callWithSenseContext()
 *   with a nil sense context.  For example:
 *   
 *   topicResponse()
 *.     { callWithSenseContext(nil, nil, {: "Here's my message!" }); }
 */
class CommandTopic: TopicEntry
    /* we go in the command topics list */
    includeInList = [&commandTopics]

    /* match the topic */
    matchTopic(fromActor, obj)
    {
        /* 
         *   Check the collection or the single object, as needed.  Note
         *   that our match object is an Action base class, so we must
         *   match if 'obj' is of the match object class. 
         */
        if (matchObj.ofKind(Collection))
        {
            /* check each entry for a match */
            if (matchObj.indexWhich({x: obj.ofKind(x)}) != nil)
                return matchScore;
        }
        else
        {
            /* check our single object */
            if (obj.ofKind(matchObj))
                return matchScore;
        }

        /* didn't find a match */
        return nil;
    }

    /* 
     *   we can always match, since the player can always type in any
     *   possible action 
     */
    isMatchPossible(actor, scopeList) { return true; }

    /* we have no pronouns to set */
    setTopicPronouns(fromActor, topic) { }
;

/*
 *   A base class for simple miscellaneous topics.  These handle things
 *   like YES, NO, HELLO, and GOODBYE, where the topic is entirely
 *   contained in the verb, and there's no separate noun phrase needed to
 *   indicate the topic.  
 */
class MiscTopic: TopicEntry
    matchTopic(fromActor, obj)
    {
        /* 
         *   if it's one of our matching topics, return our match score,
         *   otherwise return a nil score to indicate failure 
         */
        return (matchList.indexOf(obj) != nil) ? matchScore : nil;
    }

    /* 
     *   a match is always possible for simple verb topics (since the
     *   player could always type the verb) 
     */
    isMatchPossible(actor, scopeList) { return true; }
;

/*
 *   A greeting topic - this handles a HELLO or TALK TO command, as well
 *   as implied greetings (the kind of greeting generated when we jump
 *   directly into a conversation with an actor that uses stateful
 *   conversations, by typing a command like ASK ABOUT or TELL ABOUT
 *   without first saying HELLO explicitly).
 */
class HelloTopic: MiscTopic
    includeInList = [&miscTopics]
    matchList = [helloTopicObj, impHelloTopicObj]

    /* 
     *   this is an explicit greeting, so it obviously shouldn't trigger
     *   an implied greeting, regardless of how conversational we are
     */
    impliesGreeting = nil

    /* 
     *   if we use this as a greeting upon entering a ConvNode, we'll want
     *   to stay in the node afterward
     */
    noteInvocation(fromActor)
    {
        inherited(fromActor);
        "<.convstay>";
    }
;

/*
 *   An implied greeting topic.  This handles ONLY implied greetings.
 *   
 *   Note that we have a higher-than-normal score by default.  This makes
 *   it easy to program two common cases for conversational states.
 *   First, the more common case, where you want a single message for both
 *   implied and explicit greetings: just create a HelloTopic, since that
 *   responds to both kinds.  Second, the less common case, where we want
 *   to differentiate, writing separate responses for implied and explicit
 *   greetings: create a HelloTopic for the explicit kind, and ALSO create
 *   an ImpHelloTopic for the implied kind.  Since the ImpHelloTopic has a
 *   higher score, it'll overshadow the HelloTopic object when it matches
 *   an implied greeting; but since ImpHelloTopic doesn't match an
 *   explicit greeting, we'll fall back on the HelloTopic for that.  
 */
class ImpHelloTopic: MiscTopic
    includeInList = [&miscTopics]
    matchList = [impHelloTopicObj]
    matchScore = 200

    /* 
     *   this is itself a greeting, so we obviously don't want to trigger
     *   another greeting to greet the greeting
     */
    impliesGreeting = nil

    /* 
     *   if we use this as a greeting upon entering a ConvNode, we'll want
     *   to stay in the node afterward
     */
    noteInvocation(fromActor)
    {
        inherited(fromActor);
        "<.convstay>";
    }
;

/*
 *   Actor Hello topic - this handles greetings when an NPC initiates the
 *   conversation. 
 */
class ActorHelloTopic: MiscTopic
    includeInList = [&miscTopics]
    matchList = [actorHelloTopicObj]
    matchScore = 200

    /* this is a greeting, so we don't want to trigger another greeting */
    impliesGreeting = nil

    /* 
     *   if we use this as a greeting upon entering a ConvNode, we'll want
     *   to stay in the node afterward
     */
    noteInvocation(fromActor)
    {
        inherited(fromActor);
        "<.convstay>";
    }
;

/*
 *   A goodbye topic - this handles both explicit GOODBYE commands and
 *   implied goodbyes.  Implied goodbyes happen when a conversation ends
 *   without an explicit GOODBYE command, such as when the player character
 *   walks away from the NPC, or the NPC gets bored and wanders off, or the
 *   NPC terminates the conversation of its own volition.  
 */
class ByeTopic: MiscTopic
    includeInList = [&miscTopics]
    matchList = [byeTopicObj,
                 leaveByeTopicObj, boredByeTopicObj, actorByeTopicObj]

    /* 
     *   If we're not already in a conversation when we say GOODBYE, don't
     *   bother saying HELLO implicitly - if the player is saying GOODBYE
     *   explicitly, she probably has the impression that there's some kind
     *   of interaction already going on with the NPC.  If we didn't
     *   override this, you'd get an automatic HELLO followed by the
     *   explicit GOODBYE when not already in conversation, which is a
     *   little weird. 
     */
    impliesGreeting = nil
;

/* 
 *   An implied goodbye topic.  This handles ONLY automatic (implied)
 *   conversation endings, which happen when we walk away from an actor
 *   we're talking to, or the other actor ends the conversation after being
 *   ignored for too long, or the other actor ends the conversation of its
 *   own volition via npc.endConversation().
 *   
 *   We use a higher-than-default matchScore so that any time we have both
 *   a ByeTopic and an ImpByeTopic that are both active, we'll choose the
 *   more specific ImpByeTopic.  
 */
class ImpByeTopic: MiscTopic
    includeInList = [&miscTopics]
    matchList = [leaveByeTopicObj, boredByeTopicObj, actorByeTopicObj]
    matchScore = 200
;

/*
 *   A "bored" goodbye topic.  This handles ONLY goodbyes that happen when
 *   the actor we're talking terminates the conversation out of boredom
 *   (i.e., after a period of inactivity in the conversation).
 *   
 *   Note that this is a subset of ImpByeTopic - ImpByeTopic handles
 *   "bored" and "leaving" goodbyes, while this one handles only the
 *   "bored" goodbyes.  You can use this kind of topic if you want to
 *   differentiate the responses to "bored" and "leaving" conversation
 *   endings.  
 */
class BoredByeTopic: MiscTopic
    includeInList = [&miscTopics]
    matchList = [boredByeTopicObj]
    matchScore = 300
;

/*
 *   A "leaving" goodbye topic.  This handles ONLY goodbyes that happen
 *   when the PC walks away from the actor they're talking to.
 *   
 *   Note that this is a subset of ImpByeTopic - ImpByeTopic handles
 *   "bored" and "leaving" goodbyes, while this one handles only the
 *   "leaving" goodbyes.  You can use this kind of topic if you want to
 *   differentiate the responses to "bored" and "leaving" conversation
 *   endings.  
 */
class LeaveByeTopic: MiscTopic
    includeInList = [&miscTopics]
    matchList = [leaveByeTopicObj]
    matchScore = 300
;

/*
 *   An "actor" goodbye topic.  This handles ONLY goodbyes that happen when
 *   the NPC terminates the conversation of its own volition via
 *   npc.endConversation(). 
 */
class ActorByeTopic: MiscTopic
    includeInList = [&miscTopics]
    matchList = [actorByeTopicObj]
    matchScore = 300
;

/* a topic for both HELLO and GOODBYE */
class HelloGoodbyeTopic: MiscTopic
    includeInList = [&miscTopics]
    matchList = [helloTopicObj, impHelloTopicObj,
                 byeTopicObj, boredByeTopicObj, leaveByeTopicObj,
                 actorByeTopicObj]

    /* 
     *   since we handle greetings, we don't want to trigger a separate
     *   implied greeting 
     */
    impliesGreeting = nil
;

/* 
 *   Topic singletons representing HELLO and GOODBYE topics.  These are
 *   used as the parameter to matchTopic() when we're looking for the
 *   response to the corresponding verbs. 
 */
helloTopicObj: object;
byeTopicObj: object;

/* 
 *   a topic singleton for implied greetings (the kind of greeting that
 *   happens when we jump right into a conversation with a command like
 *   ASK ABOUT or TELL ABOUT, rather than explicitly saying HELLO first) 
 */
impHelloTopicObj: object;

/*
 *   a topic singleton for an NPC-initiated hello (this is the kind of
 *   greeting that happens when the NPC is the one who initiates the
 *   conversation, via actor.initiateConversation()) 
 */
actorHelloTopicObj: object;


/* 
 *   topic singletons for the two kinds of automatic goodbyes (the kind of
 *   conversation ending that happens when we simply walk away from an
 *   actor we're in conversation with, or when we ignore the other actor
 *   for enough turns that the actor gets bored and ends the conversation
 *   of its own volition) 
 */
boredByeTopicObj: object;
leaveByeTopicObj: object;

/*
 *   a topic singleton for an NPC-initiated goodbye (this is the kind of
 *   goodbye that happens when the NPC is the one who breaks off the
 *   conversation, via npc.endConversation()) 
 */
actorByeTopicObj: object;

/*
 *   A YES/NO topic.  These handle YES and/or NO, which are normally used
 *   as responses to questions posed by the NPC.  YesNoTopic is the base
 *   class, and can be used to create a single response for both YES and
 *   NO; YesTopic provides a response just for YES; and NoTopic provides a
 *   response just for NO.  The only thing an instance of these classes
 *   should normally need to specify is the response text (or a list of
 *   response strings, by multiply inheriting from an EventList subclass as
 *   usual).  
 */
class YesNoTopic: MiscTopic
    includeInList = [&miscTopics]

    /* 
     *   our list of matching topic objects - we'll only ever be asked to
     *   match 'yesTopicObj' (for YES inputs) or 'noTopicObj' (for NO
     *   inputs) 
     */
    matchList = [yesTopicObj, noTopicObj]
;

class YesTopic: YesNoTopic
    matchList = [yesTopicObj]
;

class NoTopic: YesNoTopic
    matchList = [noTopicObj]
;

/*
 *   Topic singletons representing the "topic" of YES and NO commands.  We
 *   use these as the parameter to matchTopic() in the TopicEntry objects
 *   when we're looking for a response to a YES or NO command.  
 */
yesTopicObj: object;
noTopicObj: object;


/*
 *   A default topic entry.  This is an easy way to create an entry that
 *   will be used as a last resort, if no other entry is found.  This kind
 *   of entry will match *any* topic, but with the lowest possible score,
 *   so it will only be used if there's no other match for the topic.
 *   
 *   It's a good idea to provide some variety in a character's default
 *   responses, because it seems that in every real game session, the
 *   player will at some point spend a while peppering an NPC with
 *   questions on every topic that comes to mind.  Usually, the player will
 *   think of many things that the author didn't anticipate.  The more
 *   things the author covers, the better, but it's unrealistic to think
 *   that an author can reasonably anticipate every topic, or even most
 *   topics, that players will think of.  So, we'll have a whole bunch of
 *   ASK, ASK, ASK commands all at once, and much of the time we'll get a
 *   bunch of default responses in a row.  It gets tedious in these cases
 *   when the NPC repeats the same default response over and over.
 *   
 *   A simple but effective trick is to provide three or four random
 *   variations on "I don't know that," customized for the character.  This
 *   makes the NPC seem less like a totally predictable robot, and it can
 *   also be a convenient place to flesh out the character a bit.  An easy
 *   way to do this is to add ShuffledEventList to the superclass list of
 *   the default topic entry, and provide a eventList list with the various
 *   random responses.  For example:
 *   
 *   + DefaultAskTellTopic, ShuffledEventList
 *.    ['Bob mutters something unintelligible and keeps fiddling with
 *.     the radio. ',
 *.     'Bob looks up from the radio for a second, but then goes back
 *.     to adjusting the knobs. ',
 *.     'Bob just keeps adjusting the radio, completely ignoring you. ']
 *.  ;
 *   
 *   It's important to be rather generic in default responses; in
 *   particular, it's a bad idea to suggest that the NPC doesn't know about
 *   the topic.  From the author's perspective, it's easy to make the
 *   mistake of thinking "this is a default response, so it'll only be used
 *   for topics that are completely off in left field."  Wrong!  Sometimes
 *   the player will indeed ask about completely random stuff, but in
 *   *most* cases, the player is only asking because they think it's a
 *   reasonable thing to ask about.  Defaults that say things like "I don't
 *   know anything about that" or "What a crazy thing to ask about" or "You
 *   must be stupid if you think I know about that!" can make a game look
 *   poorly implemented, because these will inevitably be shown in response
 *   to questions that the NPC really ought to know about:
 *   
 *.  >ask bob about his mother
 *.  "I don't know anything about that!"
 *.  
 *.  >ask bob about his father
 *.  "You'd have to be a moron to think I'd know about that!"
 *   
 *   It's better to use responses that suggest that the NPC is
 *   uninterested, or is hostile, or is preoccupied with something else, or
 *   doesn't understand the question, or something else appropriate to the
 *   character.  If you can manage to make the response about the
 *   *character*, rather than the topic, it'll reduce the chances that the
 *   response is jarringly illogical.  
 */
class DefaultTopic: TopicEntry
    /*
     *   A list of objects to exclude from the default match.  This can be
     *   used to create a default topic that matches everything EXCEPT a
     *   few specific topics that are handled in enclosing topic databases.
     *   For example, if you want to create a catch-all in a ConvNode's
     *   list of topics, but you want a particular topic to escape the
     *   catch-all and be sent instead to the Actor's topic database, you
     *   can put that topic in the exclude list for the catch-all, making
     *   it a catch-almost-all.  
     */
    excludeMatch = []

    /* match anything except topics in our exclude list */
    matchTopic(fromActor, topic)
    {
        /* 
         *   If the topic matches anything in the exclusion list, do NOT
         *   match the topic.  If 'topic' is a ResolvedTopic, search its
         *   in-scope and 'likely' lists; otherwise search for 'topic'
         *   directly in the exclusion list.  
         */
        if (topic.ofKind(ResolvedTopic))
        {
            /* it's a resolved topic, so search the in-scope/likely lists */
            if (topic.inScopeList.intersect(excludeMatch).length() != 0
                || topic.likelyList.intersect(excludeMatch).length() != 0)
                return nil;
        }
        else if (excludeMatch.indexOf(topic) != nil)
            return nil;

        /* match anything else with our score */
        return matchScore;
    }

    /* use a low default matching score */
    matchScore = 1

    /* a match is always possible for a default topic */
    isMatchPossible(actor, scopeList) { return true; }

    /* set the topic pronoun */
    setTopicPronouns(fromActor, topic)
    {
        /*
         *   We're not matching anything, so we can get no guidance from
         *   the match object.  Instead, look at the topic itself.  If it's
         *   a Thing, set the Thing as the antecedent.  If it's a
         *   ResolvedTopic, and there's only one Thing match in scope, or
         *   only one Thing match in the likely list, set that.  Otherwise,
         *   we have no grounds for guessing.  
         */
        if (topic != nil)
        {
            if (topic.ofKind(Thing))
            {
                /* we have a Thing - use it as the antecedent */
                fromActor.setPronounObj(topic);
            }
            else if (topic.ofKind(ResolvedTopic))
            {
                local lst;
                
                /* 
                 *   if there's only one Thing in scope, or only one Thing
                 *   in the 'likely' list, use it 
                 */
                lst = topic.inScopeList.subset({x: x.ofKind(Thing)});
                if (lst.length() == 0)
                    lst = topic.likelyList.subset({x: x.ofKind(Thing)});

                /* if we got exactly one object, it's the antecedent */
                if (lst.length() == 1)
                    fromActor.setPronounObj(lst[1]);
            }
        }
    }
;

/* 
 *   Default topic entries for different uses.  We'll use a hierarchy of
 *   low match scores, in descending order of specificity: 3 for
 *   single-type defaults (ASK only, for example), 2 for multi-type
 *   defaults (ASK/TELL), and 1 for the ANY default.  
 */
class DefaultCommandTopic: DefaultTopic
    includeInList = [&commandTopics]
    matchScore = 3
;
class DefaultAskTopic: DefaultTopic
    includeInList = [&askTopics]
    matchScore = 3
;
class DefaultTellTopic: DefaultTopic
    includeInList = [&tellTopics]
    matchScore = 3
;
class DefaultAskTellTopic: DefaultTopic
    includeInList = [&askTopics, &tellTopics]
    matchScore = 2
;
class DefaultGiveTopic: DefaultTopic
    includeInList = [&giveTopics]
    matchScore = 3
;
class DefaultShowTopic: DefaultTopic
    includeInList = [&showTopics]
    matchScore = 3
;
class DefaultGiveShowTopic: DefaultTopic
    includeInList = [&giveTopics, &showTopics]
    matchScore = 2
;
class DefaultAskForTopic: DefaultTopic
    includeInList = [&askForTopics]
    matchScore = 3
;
class DefaultAnyTopic: DefaultTopic
    includeInList = [&askTopics, &tellTopics, &showTopics, &giveTopics,
                     &askForTopics, &miscTopics, &commandTopics]

    /* 
     *   exclude these from actor-initiated hellos & goodbyes - those
     *   should only match topics explicitly 
     */
    excludeMatch = [actorHelloTopicObj, actorByeTopicObj]
    matchScore = 1
;


/*
 *   A "special" topic.  This is a topic that responds to its own unique,
 *   custom command input.  In other words, rather than responding to a
 *   normal command like ASK ABOUT or SHOW TO, we'll respond to a command
 *   for which we define our own syntax.  Our special syntax doesn't have
 *   to follow any of the ordinary parsing conventions, because whenever
 *   our ConvNode is active, we get a shot at parsing player input before
 *   the regular parser gets to see it.
 *   
 *   A special topic MUST be part of a ConvNode, because these are
 *   inherently meaningful only in context.  A special topic is active
 *   only when its conversation node is active.
 *   
 *   Special topics are automatically Suggested Topics as well as Topic
 *   Entries.  Because special topics use their own custom grammar, it's
 *   unreasonable to expect a player to guess at the custom grammar, so we
 *   should always provide a topic inventory suggestion for every special
 *   topic.  
 */
class SpecialTopic: TopicEntry, SuggestedTopicTree
    /*
     *   Our keyword list.  Each special topic instance must define a list
     *   of strings giving the keywords we match.  The special topic will
     *   match user input if the user input consists exclusively of words
     *   from this keyword list.  The user input doesn't have to include
     *   all of the words defined here, but all of the words in the user's
     *   input have to appear here to match.
     *   
     *   Alternatively, an instance can specifically define its own custom
     *   regular expression pattern instead of using the keyword list; the
     *   regular expression allows the instance to include punctuation in
     *   the syntax, or apply more restrictive criteria than simply
     *   matching the keywords.  
     */
    keywordList = []

    /*
     *   Initialize the special topic.  This runs during
     *   pre-initialization, to give us a chance to do pre-game set-up.
     *   
     *   This routine adds the topic's keywords to the global dictionary,
     *   under the 'special' token type.  Since a special topic's keywords
     *   are accepted when the special topic is active, it would be wrong
     *   for the parser to claim that the words are unknown when the
     *   special topic isn't active.  By adding the keywords to the
     *   dictionary, we let the parser know that they're valid words, so
     *   that it won't claim that they're unknown.  
     */
    initializeSpecialTopic()
    {
        /* add each keyword */
        foreach (local cur in keywordList)
        {
            /* 
             *   Add the keyword.  Since we don't actually need the
             *   word-to-object association that the dictionary stores,
             *   simply associate the word with the SpecialTopic class
             *   rather than with this particular special topic instance.
             *   The dictionary only stores a given word-obj-prop
             *   association once, even if it's entered repeatedly, so
             *   tying all of the special topic keywords to the
             *   SpecialTopic class ensures that we won't store redundant
             *   entries if the same keyword is used in multiple special
             *   topics.  
             */
            cmdDict.addWord(SpecialTopic, cur, &specialTopicWord);
        }
    }

    /* 
     *   our regular expression pattern - we'll build this automatically
     *   from the keyword list if this isn't otherwise defined 
     */
    matchPat = nil
    
    /* our suggestion (topic inventory) base name */
    name = ''

    /* 
     *   our suggestion (topic inventory) full name is usually the same as
     *   the base name; special topics usually aren't grouped in topic
     *   suggestion listings, since each topic usually has its own unique,
     *   custom syntax 
     */
    fullName = (name)

    /* on being suggested, update the special topic history */
    noteSuggestion() { specialTopicHistory.noteListing(self); }

    /* include in the specialTopics list of our parent topic database */
    includeInList = [&specialTopics]

    /* 
     *   By default, don't limit the number of times we'll suggest this
     *   topic.  Since a special topic is valid only in a particular
     *   ConvNode context, we normally want all of the topics in that
     *   context to be available, even if they've been used before. 
     */
    timesToSuggest = nil

    /* check for a match */
    matchTopic(fromActor, topic)
    {
        /* 
         *   We match if and only if we're the current active topic for
         *   our conversation node, as designated during our pre-parsing.
         *   Because we're activated exclusively by our special syntax,
         *   the only way we can ever match is by matching our special
         *   syntax in pre-parsing; when that happens, the pre-parser
         *   notes the matching SpecialTopic and sends a pseudo-command to
         *   the parser to let it know to invoke the special topic's
         *   response.  We take this circuitous route to showing the
         *   response because we do our actual matching in the pre-parse
         *   step, but we want to do the actual command processing
         *   normally; we can only accomplish both needs using this
         *   two-step process, with the two steps tied together via our
         *   memory of the topic selected in pre-parse.  
         */
        if (getConvNode().activeSpecialTopic == self)
            return matchScore;
        else
            return nil;
    }

    /* 
     *   a special topic is always matchable, since we match on literal
     *   text 
     */
    isMatchPossible(actor, scopeList) { return true; }

    /*
     *   Match a string during pre-parsing.  By default, we'll match the
     *   string if all of its words (as defined by the regular expression
     *   parser) match our keywords.  
     */
    matchPreParse(str, procStr)
    {
        /* build the regular expression pattern if there isn't one */
        if (matchPat == nil)
        {
            local pat;

            /* start with the base pattern string */
            pat = '<nocase><space>*(%<';

            /* add the keywords */
            for (local i = 1, local len = keywordList.length() ;
                 i <= len ; ++i)
            {
                /* add this keyword to the pattern */
                pat += keywordList[i];

                /* add the separator or terminator, as appropriate */
                if (i == len)
                    pat += '%><space>*)+';
                else
                    pat += '%><space>*|%<';
            }

            /* create the pattern object */
            matchPat = new RexPattern(pat);
        }

        /* we have a match if the pattern matches the processed input */
        return rexMatch(matchPat, procStr) == procStr.length();
    }

    /* find our enclosing ConvNode object */
    getConvNode()
    {
        /* scan up the containment tree for a ConvNode */
        for (local loc = location ; loc != nil ; loc = loc.location)
        {
            /* if this is a ConvNode, it's what we're looking for */
            if (loc.ofKind(ConvNode))
                return loc;
        }

        /* not found */
        return nil;
    }
;

/*
 *   A history of special topics listed in topic inventories.  This keeps
 *   track of special topics that we've recently offered, so that we can
 *   provide better feedback if the player tries to use a recently-listed
 *   special topic after it's gone out of context.
 *   
 *   When the player types a command that the parser doesn't recognize, the
 *   parser will check the special topic history to see if the command
 *   matches a special topic that was suggested recently.  If so, we'll
 *   explain that the command isn't usable right now, rather than claiming
 *   that the command is completely invalid.  A player might justifiably
 *   find it confusing to have the game suggest a command one minute, and
 *   then claim that the very same command is invalid a minute later.
 *   
 *   Ideally, we'd search *every* special topic for a match each time the
 *   player enters an invalid command, but that could take a long time in a
 *   conversation-heavy game with a large number of special topics.  As a
 *   compromise, we keep track of the last few special commands that were
 *   actually suggested, so that we can scan those.  The reasoning is that
 *   a player is more likely to try a recently-offered special command; the
 *   player will probably eventually forget older suggestions, and in any
 *   case it's much more jarring to see a "command not understood" response
 *   to a suggestion that's still fresh in the player's memory.
 *   
 *   This is a transient object because we're interested in the special
 *   topics that have been offered in the current session, irrespective of
 *   things like 'undo' and 'restore'.  From the player's perspective, the
 *   recency of a special topic suggestion is a function of the transcript,
 *   not of the internal story timeline.  For example, if the game suggests
 *   a special topic, then the player types UNDO, the player might still
 *   think to try the special topic on the next turn simply because it's
 *   right there on the screen a few lines up.  
 */
transient specialTopicHistory: object
    /* 
     *   Maximum number of topics to keep in our inventory.  When the
     *   history exceeds this number, we'll throw away the oldest entry
     *   each time we need to add a new entry - thus, we'll always have the
     *   N most recent suggestions.
     *   
     *   This can be configured as desired.  The default setting tries to
     *   strike a balance between speed and good feedback - we try to keep
     *   track of enough entries that most players wouldn't think to try
     *   anything that's aged out of the list, but not so many that it
     *   takes a long time to scan them all.
     *   
     *   If you set this to nil, we won't keep a history at all, but
     *   instead simply scan every special topic in the entire game when we
     *   need to look for a match to an entered command - in a game with a
     *   small number of special topics (on the order of, say, 30 or 40),
     *   there should be no problem using this approach.  Note that this
     *   changes the behavior in one important way: when there's no history
     *   limit, we can topics that *haven't even been offered yet*.  In
     *   some ways this is more desirable than only scanning past
     *   suggestions, since it avoids weird situations where the game
     *   claims that a command is unrecognized at one point, but later
     *   suggests and then accepts the exact same command.  It's
     *   conceivably less desirable in that it could accidentally give away
     *   information to the player, by letting them know that a randomly
     *   typed command will be meaningful at some point in the game - but
     *   the odds of this even happening seem minuscule, and the
     *   possibility that it would give away meaningful information even if
     *   it did happen seems very remote.  
     */
    maxEntries = 20

    /* note that a special topic 't' is being listed in a topic inventory */
    noteListing(t)
    {
        /* 
         *   If t's already in the list, delete it from its current
         *   position, so that we can add it back at the end of the list,
         *   reflecting its status as the most recent entry.  
         */
        historyList.removeElement(t);

        /* 
         *   if the list is already at capacity, remove the oldest entry,
         *   which is the first entry in the list 
         */
        if (maxEntries != nil && historyList.length() >= maxEntries)
            historyList.removeElementAt(1);

        /* add the new entry at the end of the list */
        historyList.append(t);
    }

    /*
     *   Scan the history list (or, if there's no limit to the history,
     *   scan all of the special topics in the entire game) for a match to
     *   an unrecognized command.  Returns true if we find a match, nil if
     *   not.  
     */
    checkHistory(toks)
    {
        local str, procStr;
        
        /* get the original and processed version of the input string */
        str = cmdTokenizer.buildOrigText(toks);
        procStr = specialTopicPreParser.processInputStr(str);
        
        /* 
         *   scan each special topic in the history - or, if the history is
         *   unlimited, scan every special topic 
         */
        if (maxEntries != nil)
        {
            /* scan each entry in our history list */
            for (local l = historyList, local i = 1, local len = l.length() ;
                 i <= len ; ++i)
            {
                /* check this entry */
                if (l[i].matchPreParse(str, procStr))
                    return true;
            }
        }
        else
        {
            /* no history limit - scan every special topic in the game */
            for (local o = firstObj(SpecialTopic) ; o != nil ;
                 o = nextObj(o, SpecialTopic))
            {
                /* check this entry */
                if (o.matchPreParse(str, procStr))
                    return true;
            }
        }

        /* we didn't find a match */
        return nil;
    }

    /* 
     *   The list of entries.  Create it when we first need it, which
     *   perInstance does for us.  
     */
    historyList = perInstance(new transient Vector(maxEntries))
;

/*
 *   An "initiate" topic entry.  This is a rather different kind of topic
 *   entry from the ones we've defined so far; an initiate topic is for
 *   cases where the NPC itself wants to initiate a conversation in
 *   response to something in the environment.
 *   
 *   One way to use initiate topics is to use the current location as the
 *   topic key.  This lets the NPC say something appropriate to the current
 *   room, and can be coded simply as
 *   
 *.     actor.initiateTopic(location);
 */
class InitiateTopic: ThingMatchTopic
    /* include in the initiateTopics list */
    includeInList = [&initiateTopics]

    /* 
     *   since this kind of topic is triggered by internal calculations in
     *   the game, and not on anything the player is doing, there's no
     *   reason that our match object should be a pronoun antecedent 
     */
    setTopicPronouns(fromActor, topic) { }
;

/* a catch-all default initiate topic */
class DefaultInitiateTopic: DefaultTopic
    includeInList = [&initiateTopics]
;


/* ------------------------------------------------------------------------ */
/*
 *   An ActorState represents the current state of an Actor.
 *   
 *   The main thing that makes actors special is that they're supposed to
 *   be living, breathing people or creatures.  That substantially
 *   complicates the programming of one of these objects, because in order
 *   to create the appearance of animation, many things about an actor have
 *   to change over time.
 *   
 *   The ActorState is designed to make it easier to program this
 *   variability that's needed to make an actor seem life-like.  The idea
 *   is to separate the parts of an actor that tend to change according to
 *   what the actor is doing, moving all of those out of the Actor object
 *   and into an ActorState object instead.  Each ActorState object
 *   represents one state of an actor (i.e., one thing the actor can be
 *   doing).  The Actor object becomes easier to program, because we've
 *   reduced the Actor object to the character's constant, unchanging
 *   features.  The stateful part is also easier to program, because we
 *   don't have to make it conditional on anything; we simply define all of
 *   the stateful parts in an ActorState, and we define separate ActorState
 *   objects for the different states.
 *   
 *   For example, suppose we want a shopkeeper actor, whose activities
 *   include waiting behind the counter, sweeping the floor, and stacking
 *   cans.  We'd define one ActorState object for each of these activities.
 *   When the shopkeeper switches from standing behind the counter to
 *   sweeping, for example, we simply set the "curState" property in the
 *   shopkeeper object so that it points to the "sweeping" state object.
 *   When it's time to stack cans, we change "curState" to it points to the
 *   "stacking cans" state object.  
 */
class ActorState: TravelMessageHandler, ActorTopicDatabase
    construct(actor) { location = actor; }

    /*
     *   Activate the state - this is called when we're about to become
     *   the active state for an actor.  We do nothing by default.
     */
    activateState(actor, oldState) { }

    /* 
     *   Deactivate the state - this is called when we're the active state
     *   for an actor, and the actor is about to switch to a new state.
     *   We do nothing by default.  
     */
    deactivateState(actor, newState) { }

    /* 
     *   Is this the actor's initial state?  If so, we'll automatically
     *   set the actor's curState to point to 'self' during
     *   pre-initialization.  For obvious reasons, this should be set to
     *   true for only one state for each actor; if multiple states are
     *   all flagged as initial for the same actor, we'll pick on
     *   arbitrarily as the actual initial state.  
     */
    isInitState = nil

    /*
     *   Should we automatically suggest topics when the player greets our
     *   actor?  By default, we show our "topic inventory" (the list of
     *   currently active topics marked as "suggested").  This can be set
     *   to nil to suppress this automatic suggestion list.
     *   
     *   Some authors might not like the idea of automatically suggesting
     *   topics every time we greet a character, but nonetheless wish to
     *   keep the TOPICS command as a sort of hint mechanism.  This flag
     *   can be used for this purpose.  Authors who don't like suggested
     *   topics at all can simply skip defining any SuggestedTopic entries,
     *   in which case there will never be anything to suggest, rendering
     *   this flag moot.  
     */
    autoSuggest = true

    /*
     *   The 'location' is the actor that we're associated with.
     *   
     *   ActorState objects aren't actual simulation objects, so the
     *   'location' property isn't used for containment.  For convenience,
     *   though, use it to indicate which actor we're associated with; this
     *   lets us use the '+' notation to define the state objects
     *   associated with an actor.  
     */
    location = nil

    /* 
     *   Get the actor associated with the state - this is simply the
     *   'location' property.  If we're nested inside another ActorState,
     *   then our actor is our enclosing ActorState's actor.  
     */
    getActor()
    {
        if (location.ofKind(ActorState))
            return location.getActor();
        else
            return location;
    }

    /* the owner of any topic entries within the state is just my actor */
    getTopicOwner() { return getActor(); }

    /* initialize the actor state */
    initializeActorState()
    {
        /* 
         *   if we're the initial state for our actor, set the actor's
         *   current state property to point to me 
         */
        if (isInitState)
            getActor().setCurState(self);
    }

    /*
     *   Show the special description for the actor when the actor is
     *   associated with this state.  By default, we use the actor's
     *   actorHereDesc message, which usually shows a generic message
     *   (something like "Bob is here" or "Bob is sitting on the chair") to
     *   indicate that the actor is present.
     *   
     *   States representing scripted activities should override these to
     *   indicate what the actor is doing: "Bob is sweeping the floor," for
     *   example.
     */
    specialDesc() { getActor().actorHereDesc; }

    /* show the special description for the actor at a distance */
    distantSpecialDesc() { getActor().actorThereDesc; }

    /* show the special description for the actor in a remote location */
    remoteSpecialDesc(actor) { getActor().actorThereDesc; }

    /*
     *   The list group(s) for the special description.  By default, if
     *   our specialDesc isn't overridden, we'll keep this in sync with
     *   the specialDesc by returning our actor's actorListWith.  And if
     *   specialDesc *is* overridden, we'll just return an empty list to
     *   indicate that we're not part of any list group.  If you want to
     *   provide your own listing group special to the state, simply
     *   override this and speicfy the custom list group.  
     */
    specialDescListWith()
    {
        /* 
         *   if specialDesc is inherited from ActorState, then use the
         *   default handling from the actor; otherwise, use no grouping at
         *   all by default 
         */
        if (!overrides(self, ActorState, &specialDesc))
            return getActor().actorListWith;
        else
            return [];
    }

    /* show the special description when we appear in a contents listing */
    showSpecialDescInContents(actor, cont)
    {
        /* by default, just show our posture in our container */
        getActor().listActorPosture(actor);
    }

    /* 
     *   Our "state" description.  This shows information on what the actor
     *   is *currently* doing; we display this after the static part of the
     *   actor's description on EXAMINE <ACTOR>.  By default, we add
     *   nothing here, but state objects that represent scripted activies
     *   should override this to describe their scripted activities.
     */
    stateDesc = ""

    /*
     *   Should we obey an action?  If so, returns true; if not, displays
     *   an appropriate response and returns nil.  This will only be
     *   called when the issuing actor is different from our actor, since
     *   a command to oneself is implicitly always obeyed.
     */
    obeyCommand(issuingActor, action)
    {
        /* 
         *   By default, we ignore all orders.  We do need to generate a
         *   response, though, so for this purpose, treat the order as a
         *   conversational action, with the 'action' object as the topic.
         */
        handleConversation(issuingActor, action, commandConvType);

        /* indicate that the order is refused */
        return nil;
    }

    /*
     *   Suggest topics for the given actor to talk to us about.  This is
     *   called when the given actor enters a TOPICS command (in which
     *   case 'explicit' will be true) or enters a conversation with us
     *   via TALK TO or the like (in which case 'explicit' will be nil).
     */
    suggestTopicsFor(actor, explicit)
    {
        /* 
         *   if this is not an explicit TOPICS request, and we're not in
         *   "auto suggest" mode, don't show anything - we don't want any
         *   automatic suggestions in this mode  
         */
        if (!explicit && !autoSuggest)
            return;

        /* 
         *   show a paragraph break, in case we're being tacked on to
         *   another report; but make it cosmetic, so that this by itself
         *   doesn't suppress a default report, in case we don't end up
         *   displaying any topics 
         */
        cosmeticSpacingReport('<.p>');

        /* show our suggestion list */
        showSuggestedTopicList(getSuggestedTopicList(),
                               actor, getActor(), explicit);
    }

    /*
     *   Get our suggested topic list.  The suggested topic list consists
     *   of the union of the current ConvNode's suggestion list, the
     *   ActorState list, and the Actor's suggestion list.  In each case,
     *   the suggestion list is the list of all SuggestedTopic objects at
     *   each database level.
     *   
     *   The suggestions are arranged in a hierarchy, and each hierarchy
     *   level can prevent suggestions from a lower level from being
     *   included.  The top level of the hierarchy is the ConvNode; the
     *   next level is the ActorState; and the last level is the Actor.
     *   Suggestions are limited at each level with the 'limitSuggestions'
     *   property: if true, suggestions from lower levels are not included.
     */
    getSuggestedTopicList()
    {
        local v = new Vector(16);
        local node;
        local lst;

        /* add the actor's current conversation node topics */
        if ((node = getActor().curConvNode) != nil)
        {
            /* if there are any suggested topics in the node, include them */
            if ((lst = node.suggestedTopics) != nil)
                v.appendAll(lst);

            /* 
             *   if this ConvNode is marked as limiting suggestions to
             *   those defined within the node, return what we have
             *   without adding anything from the broader context 
             */
            if (node.limitSuggestions)
                return v;
        }

        /* add our own topics */
        if ((lst = stateSuggestedTopics) != nil)
            v.appendAll(lst);

        /* 
         *   if the ActorState is limiting suggestions, don't include any
         *   suggestions from the broader context (i.e., from the Actor
         *   itself) 
         */
        if (limitSuggestions)
            return v;

        /* if our actor has its own list, add those as well */
        if ((lst = getActor().suggestedTopics) != nil)
            v.appendAll(lst);

        /* return the combined list */
        return v;
    }

    /* 
     *   get the topic suggestions for this state - by default, we just
     *   return our own suggestedTopics list 
     */
    stateSuggestedTopics = (suggestedTopics)

    /*
     *   Get my implied in-conversation state.  This is used when our actor
     *   initiates a conversation without specifying a particular
     *   conversation state to enter (i.e., actor.initiateConversation() is
     *   called with 'state' set to nil).  By default, we don't have an
     *   implied conversation state, so we just return 'self' to indicate
     *   that we want to stay in the current state.  States that are
     *   coupled with separate in-conversation states, such as
     *   ConversationReadyState, should return their associated
     *   conversation states here.  
     */
    getImpliedConvState = (self)

    /*
     *   General conversation handler.  This can be used to process most
     *   conversational commands - ASK, TELL, GIVE, SHOW, etc.  The
     *   standard sequence of processing is as follows:
     *   
     *   - If our actor has a non-nil current conversation node (ConvNode)
     *   object, and the ConvNode wants to handle the event, let the
     *   ConvNode handle it.
     *   
     *   - Otherwise, check our own topic database to see if we can find a
     *   TopicEntry that matches the topic; if we can find one, let the
     *   TopicEntry handle it.
     *   
     *   - Otherwise, let the actor handle it.
     *   
     *   'otherActor' is the actor who originated the conversation command
     *   (usually the player character). 'topic' is the subject being
     *   discussed (the indirect object of ASK ABOUT, for example).
     *   convType' is a ConvType describing the type of conversational
     *   action we're performing.  
     */
    handleConversation(otherActor, topic, convType)
    {
        local actor = getActor();
        local hasDefault;
        local node;
        local path;

        /* determine if I have a default response handler */
        hasDefault = propDefined(convType.defaultResponseProp);

        /*
         *   Figure the database search path for looking up the topics.
         *   We'll start in the ConvNode database, then continue to the
         *   ActorState database, then finally to the Actor database.
         *   However, we won't reach the Actor database if there's a
         *   default response handler in the state, because if we fail to
         *   find it at the state, we'll take the default.
         *   
         *   Since the path we need to provide at each point is the
         *   *remaining* path, don't bother including the ConvNode, since
         *   we'd just have to take it right back out to get the remaining
         *   path after the ConvNode.  
         */
        path = [self];
        if (!hasDefault)
            path += actor;

        /* 
         *   If our actor has a current conversation node, check to see if
         *   the conversation node wants to handle it.  If not, check our
         *   own topic database, then the actor's.  
         */
        if ((node = actor.curConvNode) == nil
            || !node.handleConversation(otherActor, topic, convType, path))
        {
            /* get the remaining database search path */
            path = path.sublist(2);
            
            /* 
             *   Either we don't have a ConvNode, or the ConvNode isn't
             *   interested in handling the operation.  Check to see if we
             *   can handle it through our own topic database.  
             */
            if (!handleTopic(otherActor, topic, convType, path))
            {
                /*
                 *   We couldn't find anything in our topic database that's
                 *   interested in handling it.  Check to see if the state
                 *   object defines the default response handler method,
                 *   and use that as the response if so. 
                 */
                if (hasDefault)
                {
                    /* 
                     *   the state object (i.e., self) does define the
                     *   default response method, so invoke that 
                     */
                    convType.defaultResponse(self, otherActor, topic);
                }
                else
                {
                    /* 
                     *   We don't have a topic database entry and we don't
                     *   have our own definition of the default response
                     *   handler.  All that remains is to let our actor
                     *   handle it.  
                     */
                    actor.handleConversation(otherActor, topic, convType);
                }
            }
        }

        /* whatever happened, run the appropriate after-response handling */
        convType.afterResponse(actor, otherActor);
    }

    /*
     *   Receive notification that a TopicEntry is being used (via its
     *   handleTopic method) to respond to a command.  The TopicEntry will
     *   call this before it shows its message or takes any other action.
     *   By default, we do nothing.  
     */
    notifyTopicResponse(fromActor, entry) { }

    /* 
     *   Handle a before-action notification for our actor.  By default,
     *   we do nothing.  
     */
    beforeAction()
    {
        /* do nothing by default */
    }

    /* handle an after-action notification for our actor */
    afterAction()
    {
    }

    /* handle a before-travel notification */
    beforeTravel(traveler, connector)
    {
        local other = getActor().getCurrentInterlocutor();
        
        /* 
         *   if our conversational partner is departing, break off the
         *   conversation
         */
        if (connector != nil
            && other != nil
            && traveler.isActorTraveling(other))
        {
            /* end the conversation */
            if (!endConversation(gActor, endConvTravel))
            {
                /* 
                 *   they don't want to allow the conversation to end, so
                 *   abort the travel action 
                 */
                exit;
            }
        }
    }

    /* handle an after-travel notification */
    afterTravel(traveler, connector)
    {
    }

    /*
     *   End the current conversation.  'reason' indicates why we're
     *   leaving the conversation - this is one of the endConvXxx enums
     *   defined in adv3.h.  beforeTravel() calls this automatically when
     *   the other party is trying to depart, and they're talking to us.
     *   
     *   This returns true if we wish to allow the conversation to end,
     *   nil if not.  
     */
    endConversation(actor, reason)
    {
        local ourActor = getActor();
        local node;

        /* tell the current ConvNode about it */
        if ((node = ourActor.curConvNode) != nil)
        {
            local ret;

            /* the can-end call might show a response, so set our actor */
            conversationManager.beginResponse(ourActor);

            /* ask the node if it's okay to end the conversation */
            ret = node.canEndConversation(actor, reason);

            /* 
             *   If the result is blockEndConv, it means that the actor
             *   said something to force the conversation to keep going.
             *   Make a note that the other actor already said something on
             *   this turn so that we don't generate another scripted
             *   message later, and flag this as preventing the
             *   conversation ending. 
             */
            if (ret == blockEndConv)
            {
                /* flag that the other actor said something this turn */
                ourActor.noteConvAction(actor);

                /* we're unable to end the conversation now */
                ret = nil;
            }

            /* end the response, leaving the node unchanged by default */
            conversationManager.finishResponse(
                ourActor, ourActor.curConvNode);

            /* 
             *   if the node said no, tell the caller we can't end the
             *   conversation right now 
             */
            if (!ret)
                return nil;
            
            /* tell the node we are indeed ending the conversation */
            node.endConversation(actor, reason);
        }

        /* forget any conversation tree position */
        ourActor.setConvNodeReason(nil, 'endConversation');

        /* indicate that we are allowing the conversation to end */
        return true;
    }

    /*
     *   Take a turn.  This is called when it's the actor's turn and
     *   there's not something else the actor needs to be doing (such as
     *   following another actor, or carrying out a command in the actor's
     *   pending command queue).
     *   
     *   By default, we perform several steps automatically.
     *   
     *   First, we check to see if the actor is in a ConvNode.  If so, the
     *   ConvNode takes precedence.  If we haven't been addressed already
     *   in conversation on this turn, we'll let the ConvNode perform its
     *   "continuation," which lets the NPC advance the conversation of its
     *   own volition.  In any case, if we have a current ConvNode, we're
     *   done with the turn, since we assume the actor will want to proceed
     *   with the conversation before pursuing its agenda or performing a
     *   background action.
     *   
     *   Second, assuming there's no active ConvNode, we check for an
     *   "agenda" item that's ready to execute.  If we find one, we execute
     *   it, and we're done.  The agenda item takes precedence over any
     *   other scripting we might have.
     *   
     *   Finally, if we also inherit from Script, and we didn't find an
     *   active ConvNode or an agenda item that was ready to execute, we
     *   invoke our doScript() method.  This makes it especially easy to
     *   define random background messages for the actor - just add an
     *   EventList class (ShuffledEventList is usually the right one) to
     *   the state's superclass list, and define a list of background
     *   message strings.  
     */
    takeTurn()
    {
        local actor = getActor();

        /* 
         *   Check to see if we want to continue a conversation.  If so,
         *   and we haven't already conversed this turn, try the
         *   continuing conversation.  If that displays anything, consider
         *   the turn done.
         *   
         *   Otherwise, try executing an agenda item.  If we do, consider
         *   the turn done.
         *   
         *   Otherwise, if we're of class Script, execute our scripted
         *   action.  
         */
        if (actor.curConvNode != nil
            && !actor.conversedThisTurn()
            && actor.curConvNode.npcContinueConversation())
        {
            /* 
             *   we displayed an NPC-motivated conversation continuation,
             *   so we're done with this turn 
             */
        }
        else if (actor.executeAgenda())
        {
            /* we executed an agenda item, so we need do nothing more */
        }
        else if (ofKind(Script))
        {
            /* we're a Script, so invoke our scripted action */
            doScript();
        }
    }

    /*
     *   Receive notification that we just followed another actor as part
     *   of our programmed following behavior (in other words, due to our
     *   'followingActor' property, not due to an explicit FOLLOW command
     *   directed to us).  'success' is true if we ended up in the actor's
     *   location, nil if not.
     *   
     *   This can be used to update the actor's state after a 'follow'
     *   operation occurs; for example, if the actor's state depends on
     *   the actor's location, this can update the state accordingly.  We
     *   don't do anything by default.  
     */
    justFollowed(success)
    {
        /* do nothing by default */
    }

    /*
     *   Our group-travel arrival description.  By default, when we perform
     *   an accompanying travel with another actor as the lead actor, the
     *   accompanying travel state will display this message instead of our
     *   specialDesc when the lead actor first arrives in the new location.
     *   We'll just display our own specialDesc by default, but this should
     *   usually be overridden to say something specific to the group
     *   travel arrival.  The actual message is entirely dependent on the
     *   nature of the group travel, which is why we don't provide a
     *   special message by default.
     *   
     *   For scripted behavior, it's sometimes better to use arrivingTurn()
     *   rather than this method to describe the behavior.
     *   arrivingWithDesc() is called as part of the room description, so
     *   it's best for any message shown here to fit well into the usual
     *   room description format.  For more complex transitions into the
     *   new room state, arrivingTurn() is sometimes more appropriate,
     *   since it runs like a daemon, after the arrival (and thus the new
     *   room description) is completed.  
     */
    arrivingWithDesc() { specialDesc(); }

    /*
     *   Perform any special action on a group-travel arrival.  When group
     *   travel is performed using the AccompanyingInTravelState class,
     *   this is essentially called in lieu of the regular takeTurn()
     *   method on the state that is coming into effect after the group
     *   travel.  (Not really, but effectively: the accompanying travel
     *   state will still be in effect, so its takeTurn() method is what's
     *   really called, but that method will call this method explicitly.)
     *   By default, we do nothing.  Since this runs on our turn, it's a
     *   good place to put any scripted behavior we perform on arriving at
     *   our new destination after the group travel.  
     */
    arrivingTurn() { }

    /* 
     *   For our TravelMessageHandler implementation, the nominal traveler
     *   is our actor.  Note that this is all we need to implement for
     *   travel message handling, since we simply inherit the default
     *   handling for all of the arrival/departure messages.  
     */
    getNominalTraveler() { return getActor(); }
;

/*
 *   A "ready for conversation" state.  This can be used as the base class
 *   for actor states when the actor is receptive to conversation, and we
 *   want to have the sense of a conversational context.  The key feature
 *   that this class provides is the ability to provide messages when
 *   engaging and disengaging the conversation.
 *   
 *   Note that this state is NOT required for conversation, since the basic
 *   ActorState object accepts conversational commands like ASK, TELL,
 *   GIVE, and TAKE.  The special feature of the "conversation ready" state
 *   is that we explicitly move the actor to a separate state when
 *   conversation begins.  This is especially appropriate for states in
 *   which the NPC is actively carrying on some other activity; the
 *   conversation should interrupt those states, so that the actor stops
 *   the other activity and gives us its full attention.
 *   
 *   This type of state can be associated with its in-conversation state
 *   object in one of two ways.  First, the inConvState property can be
 *   explicitly set to point to the in-conversation state object.  Second,
 *   this object can be nested inside its in-conversation state object via
 *   the 'location' property (so you can use the '+' syntax to put this
 *   object inside its in-conversation state object).  The 'ready' object
 *   goes inside the 'conversing' object because a single 'conversing'
 *   object can frequently be shared among several 'ready' states.  
 */
class ConversationReadyState: ActorState
    /*
     *   The associated in-conversation state.  This should be set to an
     *   InConversationState object that controls the actor's behavior
     *   while carrying on a conversation.  Note that the library will
     *   automatically set this if the instance is nested (via its
     *   'location' property) inside an InConversationState object.  
     */
    inConvState = nil

    /* my implied conversational state is my in-conversation state */
    getImpliedConvState = (inConvState)

    /*
     *   Show our greeting message.  If 'explicit' is true, it means that
     *   the player character is greeting us through an explicit greeting
     *   command, such as HELLO or TALK TO.  Otherwise, the greeting is
     *   implied by some other conversational action, such a ASK ABOUT or
     *   SHOW TO.  We do nothing by default; this should be overridden in
     *   most cases to show some sort of exchange of pleasantries -
     *   something like this:
     *   
     *.  >bob, hello
     *.  "Hi, there," you say.
     *   
     *   Bob looks up over his newspaper.  "Oh, hello," he says, putting
     *   down the paper.  "What can I do for you?"
     *   
     *   Note that games shouldn't usually override this method.  Instead,
     *   you should simply create a HelloTopic entry and put it inside the
     *   state object; we'll find the HelloTopic and show its message as
     *   our greeting.
     *   
     *   If you want to distinguish between explicit and implicit
     *   greetings, you can create an ImpHelloTopic entry for implied
     *   greetings (i.e., the kind of greeting that occurs automatically
     *   when the player jumps right into a conversation with our actor
     *   using ASK ABOUT or the like, without explicitly saying HELLO
     *   first).  The regular HelloTopic will handle explicit greetings,
     *   and the ImpHelloTopic will handle the implied kind.  
     */
    showGreetingMsg(actor, explicit)
    {
        /* look for a HelloTopic in our topic database */
        if (handleTopic(actor, explicit ? helloTopicObj : impHelloTopicObj,
                        helloConvType, nil))
            "<.p>";
    }

    /*
     *   Enter this state from a conversation.  This should show any
     *   message we want to display when we're ending a conversation and
     *   switching from the conversation to this state.  'reason' is the
     *   endConvXxx enum indicating what triggered the termination of the
     *   conversation.  'oldNode' is the ConvNode we were in just before we
     *   initiated the termination - we need this information because we
     *   want to look in the ConvNode for a Bye topic message to display,
     *   but we can't just look in the actor for the node because it will
     *   already have been cleared out by the time we get here.
     *   
     *   Games shouldn't normally override this method.  Instead, simply
     *   create a ByeTopic entry and put it inside the state object; we'll
     *   find the ByeTopic and show its message for the goodbye.
     *   
     *   If you want to distinguish between different types of goodbyes,
     *   you can create an ImpByeTopic for any implied goodbye (i.e., the
     *   kind where the other actor just walks away, or where we get bored
     *   of the other actor ignoring us).  You can also further
     *   differentiate by creating BoredByeTopic and/or LeaveByeTopic
     *   objects to handle just those cases.  The regular ByeTopic will
     *   handle explicit GOODBYE commands, and the others (ImpByeTopic,
     *   BoredByeTopic, LeaveByeTopic) will handle the implied kinds.  
     */
    enterFromConversation(actor, reason, oldNode)
    {
        local topic;
        local reasonMap = [endConvBye, byeTopicObj,
                           endConvTravel, leaveByeTopicObj,
                           endConvBoredom, boredByeTopicObj,
                           endConvActor, actorByeTopicObj];
        
        /* figure out which topic object we need, based on the reason code */
        topic = reasonMap[reasonMap.indexOf(reason) + 1];
        
        /* 
         *   Look for a ByeTopic in the ConvNode; failing that, try our own
         *   database. 
         */
        if (oldNode == nil
            || !oldNode.handleConversation(actor, topic, byeConvType, nil))
        {
            /* there's no node handler; try our own database */
            handleTopic(actor, topic, byeConvType, nil);
        }
    }

    /* handle a conversational action directed to our actor */
    handleConversation(otherActor, topic, convType)
    {
        /* 
         *   If this is a greeting, handle it ourselves.  Otherwise, pass
         *   it along to our associated in-conversation state.  
         */
        if (convType == helloConvType)
        {
            /* 
             *   Switch to our associated in-conversation state and show a
             *   greeting.  Since we're explicitly entering the
             *   conversation, we have no topic entry.  
             */
            enterConversation(otherActor, nil);

            /* show or schedule a topic inventory, as appropriate */
            conversationManager.showOrScheduleTopicInventory(
                getActor(), otherActor);
        }
        else
        {
            /* 
             *   it's not a greeting, so pass it to our in-conversation
             *   state for handling
             */
            inConvState.handleConversation(otherActor, topic, convType);
        }
    }

    /*
     *   Initiate conversation based on the given simulation object.  This
     *   is an internal method that isn't usually called directly from game
     *   code; game code usually calls the Actor's initiateTopic(), which
     *   calls this routine to check for a topic that's part of the state
     *   object. 
     */
    initiateTopic(obj)
    {
        /* defer to our in-conversation state */
        return inConvState.initiateTopic(obj);
    }

    /*
     *   Receive notification that a TopicEntry is being used (via its
     *   handleTopic method) to respond to a command.  If the TopicEntry is
     *   conversational, automatically enter our in-conversation state.  
     */
    notifyTopicResponse(fromActor, entry)
    {
        if (entry.isConversational)
            enterConversation(fromActor, entry);
    }

    /* 
     *   Enter a conversation with the given actor, either explicitly (via
     *   HELLO or TALK TO) or implicitly (by directly asking a question,
     *   etc).  'entry' gives the TopicEntry that's triggering the implicit
     *   conversation entry; if this is nil, it means that we're being
     *   triggered explicitly.  
     */
    enterConversation(actor, entry)
    {
        local myActor = getActor();
        local explicit = (entry == nil);
        
        /* if the actor can't talk to us, we can't enter the conversation */
        if (!actor.canTalkTo(myActor))
        {
            /* tell them we can't talk now */
            reportFailure(&objCannotHearActorMsg, myActor);
            
            /* terminate the command */
            exit;
        }

        /* 
         *   Show our greeting, if desired.  We show a greeting if we're
         *   being invoked explicitly (that is, there's no TopicEntry), or
         *   if we're being invoked explicitly and the TopicEntry implies a
         *   greeting.  
         */
        if (explicit || entry.impliesGreeting)
            showGreetingMsg(actor, explicit);

        /* activate the in-conversation state */
        myActor.setCurState(inConvState);
    }

    /*
     *   Get this state's suggested topic list.  ConversationReady states
     *   shouldn't normally have topic entries of their own, since a
     *   ConvversationReady state usually forwards conversation handling
     *   to its corresponding in-conversation state.  So, simply return
     *   the suggestion list from our in-conversation state object.  
     */
    stateSuggestedTopics = (inConvState.suggestedTopics)

    /* initialize the actor state object */
    initializeActorState()
    {
        /* inherit the default handling */
        inherited();

        /* 
         *   if we're nested inside an in-conversation state object, the
         *   containing in-conversation state is the one we'll use for
         *   conversations 
         */
        if (location.ofKind(InConversationState))
            inConvState = location;
    }
;

/*
 *   The "in-conversation" state.  This works with ConversationReadyState
 *   to handle transitions in and out of conversations.  In this state, we
 *   are actively engaged in a conversation.
 *   
 *   Throughout this implementation, we assume that we only care about
 *   conversations with a single character, specifically the player
 *   character.  There's generally no good reason to fully model
 *   conversations between NPC's, since that kind of NPC activity is in
 *   most cases purely pre-scripted and thus requires no special state
 *   tracking.  Since we generally only need to worry about tracking a
 *   conversation with the player character, we don't bother with the
 *   possibility that we're simultaneously in conversation with more than
 *   one other character.  
 */
class InConversationState: ActorState
    /*
     *   Our attention span, in turns.  This is the number of turns that
     *   we'll be willing to stay in the conversation while the other
     *   character is ignoring us.  After the conversation has been idle
     *   this long, we'll assume the other actor is no longer talking to
     *   us, so we'll terminate the conversation ourselves.
     *   
     *   If the NPC's doesn't have a limited attention span, set this
     *   property to nil.  This will prevent the NPC from ever disengaging
     *   of its own volition.    
     */
    attentionSpan = 4

    /*
     *   The state to switch to when the conversation ends.  Instances can
     *   override this to select the next state.  By default, we'll return
     *   to the state that we were in immediately before the conversation
     *   started.  
     */
    nextState = (previousState)

    /*
     *   End the current conversation.  'reason' indicates why we're
     *   leaving the conversation - this is one of the endConvXxx enums
     *   defined in adv3.h.
     *   
     *   This method is a convenience only; you aren't required to call
     *   this method to end the conversation, since you can simply switch
     *   to another actor state directly if you prefer.  This method's
     *   main purpose is to display an appropriate message terminating the
     *   conversation while switching to the new state.  If you want to
     *   display your own message directly from the code that's changing
     *   the state, there's no reason to call this.
     *   
     *   This returns true if we wish to allow the conversation to end,
     *   nil if not.  
     */
    endConversation(actor, reason)
    {
        local nxt;
        local myActor = getActor();

        /* 
         *   note the current ConvNode for our actor - when we check with
         *   the ConvNode to see about ending the conversation, this will
         *   automatically exit the ConvNode, so we need to save this first
         *   so that we can refer to it later to check for a Bye topic
         */
        local oldNode = myActor.curConvNode;

        /* 
         *   Inherit the base behavior first - if it disallows the action,
         *   return failure.  The inherited version will check with the
         *   current ConvNode to see if has any objection.  
         */
        if (!inherited(actor, reason))
            return nil;

        /* get the next state */
        nxt = nextState;

        /* if there isn't one, stay in the actor's current state */
        if (nxt == nil)
            nxt = myActor.curState;
        
        /* 
         *   If the next state is a 'conversation ready' state, tell it
         *   we're entering from a conversation.  We're ending the
         *   conversation explicitly only if 'reason' is endConvBye.  Pass
         *   along the ConvNode we just exited (if any), so that we can
         *   look for a response in the node.  
         */
        if (nxt.ofKind(ConversationReadyState))
            nxt.enterFromConversation(actor, reason, oldNode);

        /* switch our actor to the next state */
        myActor.setCurState(nxt);

        /* indicate that we are allowing the conversation to end */
        return true;
    }

    /*  handle a conversational command */
    handleConversation(otherActor, topic, convType)
    {
        /* handle goodbyes specially */
        if (convType == byeConvType)
        {
            /*
             *   If this is an implicit goodbye, run the normal
             *   conversation handling in order to display any implied
             *   ByeTopic message - but capture the output in case we
             *   decide not to end the conversation after all.  Only do
             *   this in the case of an implicit goodbye, though - for an
             *   explicit goodbye, there's no need for this as the explicit
             *   BYE will do the same thing on its own.  
             */
            local txt = nil;
            if (topic != byeTopicObj)
            {
                txt = mainOutputStream.captureOutput(
                    {: inherited(otherActor, topic, convType) });
            }

            /* 
             *   try to end the conversation; if we won't allow it,
             *   terminate the action here 
             */
            if (!endConversation(otherActor, endConvBye))
                exit;

            /* show the captured ByeTopic output */
            if (txt != nil)
                say(txt);
        }
        else
        {
            /* use the inherited handling */
            inherited(otherActor, topic, convType);
        }
    }

    /* 
     *   provide a default HELLO response, if we don't have a special
     *   TopicEntry for it 
     */
    defaultGreetingResponse(actor)
    {
        /* 
         *   As our default response, point out that we're already at the
         *   actor's service.  (This isn't an error, because the other
         *   actor might not have been talking to us, even though we
         *   thought we were talking to them.)  
         */
        gLibMessages.alreadyTalkingTo(getActor(), actor);
    }

    takeTurn()
    {
        local actor = getActor();
        
        /* if we didn't interact this turn, increment our boredom counter */
        if (!actor.conversedThisTurn())
            actor.boredomCount++;

        /* run the inherited handling */
        inherited();
    }

    /* activate this state */
    activateState(actor, oldState)
    {
        /*
         *   If the previous state was a ConversationReadyState, or we
         *   have no other state remembered, remember the previous state -
         *   this is the default we'll return to at the end of the
         *   conversation, if the instance doesn't specify another state.
         *   
         *   We don't remember prior states that aren't conv-ready states
         *   to make it easier to temporarily interrupt a conversation
         *   with some other state, and later return to the conversation.
         *   If we remembered every prior state, then we'd return to the
         *   interrupting state when the conversation ended, which is
         *   usually not what's wanted.  Usually, we want to return to the
         *   last conv-ready state when a conversation ends, ignoring any
         *   other intermediate states that have been active since the
         *   conv-ready state was last in effect.  
         */
        if (previousState == nil || oldState.ofKind(ConversationReadyState))
            previousState = oldState;

        /* 
         *   reset the actor's boredom counter, since we're just starting a
         *   new conversation, and add our boredom agenda item to the
         *   active list to monitor our boredom level 
         */
        actor.boredomCount = 0;
        actor.addToAgenda(actor.boredomAgendaItem);

        /* remember the time of the last conversation command */
        actor.lastConvTime = Schedulable.gameClockTime;
    }

    /* deactivate this state */
    deactivateState(actor, newState)
    {
        /* 
         *   we're leaving the conversation state, so there's no need to
         *   monitor our boredom level any longer 
         */
        actor.removeFromAgenda(actor.boredomAgendaItem);

        /* do the normal work */
        inherited(actor, newState);
    }

    /* 
     *   The previous state - this is the state we were in before the
     *   conversation began, and the one we'll return to by default when
     *   the conversation ends.  We'll set this automatically on
     *   activation.  
     */
    previousState = nil
;

/*
 *   A special kind of agenda item for monitoring "boredom" during a
 *   conversation.  We check to see if our actor is in a conversation, and
 *   the PC has been ignoring the conversation for too long; if so, our
 *   actor initiates the end of the conversation, since the PC apparently
 *   isn't paying any attention to us. 
 */
class BoredomAgendaItem: AgendaItem
    /* we construct these dynamically during actor initialization */
    construct(actor)
    {
        /* remember our actor as our location */
        location = actor;
    }

    /* 
     *   we're ready to run if our actor is in an InConversationState and
     *   its boredom count has reached the limit for the state 
     */
    isReady()
    {
        local actor = getActor();
        local state = actor.curState;

        return (inherited()
                && state.ofKind(InConversationState)
                && state.attentionSpan != nil
                && actor.boredomCount >= state.attentionSpan);
    }

    /* on invocation, end the conversation */
    invokeItem()
    {
        local actor = getActor();
        local state = actor.curState;

        /* tell the state to end the conversation */
        state.endConversation(actor.getCurrentInterlocutor(), endConvBoredom);
    }

    /* 
     *   by default, handle boredom before other agenda items - we do this
     *   because an ongoing conversation will be the first thing on the
     *   NPC's mind 
     */
    agendaOrder = 50
;


/*
 *   A "hermit" actor state is a state where the actor is unresponsive to
 *   conversational overtures (ASK ABOUT, TELL ABOUT, HELLO, GOODBYE, YES,
 *   NO, SHOW TO, GIVE TO, and any orders directed to the actor).  Any
 *   attempt at conversation will be met with the 'noResponse' message.  
 */
class HermitActorState: ActorState
    /* 
     *   Show our response to any conversational command.  We'll simply
     *   show the standard "there's no response" message by default, but
     *   subclasses can (and usually should) override this to explain
     *   what's really going on.  Note that this routine will be invoked
     *   for any sort of conversation command, so any override needs to be
     *   generic enough that it's equally good for ASK, TELL, and
     *   everything else.
     *   
     *   Note that it's fairly easy to create a shuffled list of random
     *   messages, if you want to add some variety to the actor's
     *   responses.  To do this, use an embedded ShuffledEventList:
     *   
     *   myState: HermitActorState
     *.    noResponse() { myList.doScript(); }
     *.    myList: ShuffledEventList {
     *.      ['message1', 'message2', 'message3'] }
     *.  ;
     */
    noResponse() { mainReport(&noResponseFromMsg, getActor()); }

    /* all conversation actions get the same default response */
    handleConversation(otherActor, topic, convType)
    {
        /* just show our standard default response */
        noResponse();
    }

    /* 
     *   Since the hermit state blocks topics from outside the state, don't
     *   offer suggestions for other topics while in this state.
     *   
     *   Note that you might sometimes want to override this to allow the
     *   usual topic suggestions (by setting this to nil).  In particular:
     *   
     *   - If it's not outwardly obvious that the actor is unresponsive,
     *   you'll probably want to allow suggestions.  Remember, TOPICS
     *   suggests topics that the *PC* wants to talk about, not things the
     *   NPC is interested in.  If the PC doesn't necessarily know that the
     *   NPC won't respond, the PC would still want to ask about those
     *   topics.
     *   
     *   - If the hermit state is to be short-lived, you might want to show
     *   the topic suggestions even in the hermit state, so that the player
     *   is aware that there are still useful topics to explore with the
     *   NPC.  The player might otherwise assume that the NPC is out of
     *   useful topics, and not bother trying again later when the NPC
     *   becomes more responsive.  
     */
    limitSuggestions = true
;

/*
 *   The basic "accompanying" state.  In this state, whenever the actor
 *   we're accompanying travels to a location we want to follow, we'll
 *   travel at the same time with the other actor.  
 */
class AccompanyingState: ActorState
    /*
     *   Check to see if we are to accompany the given traveler on the
     *   given travel.  'traveler' is the Traveler performing the travel,
     *   and 'conn' is the connector that the traveler is about to take.
     *   
     *   Note that 'traveler' is a Traveler object.  This will simply be an
     *   Actor (which is a kind of Traveler) when the actor is performing
     *   the travel directly, but it could also be another kind of
     *   Traveler, such as a Vehicle.  This routine must determine whether
     *   to accompany other kinds of actors.
     *   
     *   By default, we'll return true to indicate that we want to
     *   accompany any traveler anywhere they go.  This should almost
     *   always be overridden in practice to be more specific.  
     */
    accompanyTravel(traveler, conn) { return true; }

    /* 
     *   Get our accompanying state object.  We'll create a basic
     *   accompanying in-travel state object, returning to the current
     *   state when we're done.  'traveler' is the Traveler object that's
     *   performing the travel; this might be an Actor, but could also be a
     *   Vehicle or other Traveler subclass.  
     */
    getAccompanyingTravelState(traveler, connector)
    {
        /* 
         *   Create the default intermediate state for the travel.  Note
         *   that the lead actor is the actor performing the command - this
         *   won't necessarily be the traveler, since the actor could be
         *   steering a vehicle.  
         */
        return new AccompanyingInTravelState(
            getActor(), gActor, getActor().curState);
    }

    /*
     *   handle a before-travel notification for my actor 
     */
    beforeTravel(traveler, connector)
    {
        /*
         *   If we want to accompany the given traveler on this travel, add
         *   ourselves to the initiating actor's list of accompanying
         *   actors.  Never set an actor to accompany itself, since doing
         *   so would lead to infinite recursion.  
         */
        if (accompanyTravel(traveler, connector) && getActor() != gActor)
        {
            /* 
             *   Add me to the list of actors accompanying the actor
             *   initiating the travel - that actor will run a nested
             *   travel action on us before doing its own travel.  Note
             *   that the initiating actor is gActor, since that's the
             *   actor performing the action that led to the travel.  
             */
            gActor.addAccompanyingActor(getActor());
            
            /* put my actor into the appropriate new group travel state */
            getActor().setCurState(
                getAccompanyingTravelState(traveler, connector));
        }

        /* inherit the default handling */
        inherited(traveler, connector);
    }
;

/*
 *   "Accompanying in-travel" state - this is an actor state used when an
 *   actor is taking part in a group travel operation.  This state lasts
 *   only as long as the single turn - which belongs to the lead actor -
 *   that it takes to carry out the group travel.  Once our turn comes
 *   around, we'll restore the actor to the previous state - or, we can set
 *   the actor to a different state, if desired.  Setting the actor to a
 *   different state is useful when the group travel triggers a new
 *   scripted activity in the new room.  
 */
class AccompanyingInTravelState: ActorState
    construct(actor, lead, next)
    {
        /* do the normal initialization */
        inherited(actor);

        /* remember the lead actor and the next state */
        leadActor = lead;
        nextState = next;
    }

    /* the lead actor of the group travel */
    leadActor = nil

    /* 
     *   the next state - we'll switch our actor to this state after the
     *   travel has been completed 
     */
    nextState = nil

    /*
     *   Show our "I am here" description.  By default, we'll use the
     *   arrivingWithDesc of the *next* state object.  
     */
    specialDesc() { nextState.arrivingWithDesc; }

    /* take our turn */
    takeTurn()
    {
        /* 
         *   The group travel only takes the single turn in which the
         *   travel is initiated, so by the time our turn comes around, the
         *   group travel is done.  Clear out the lead actor's linkage to
         *   us as an accompanying actor.  
         */
        leadActor.accompanyingActors.removeElement(getActor());

        /* switch our actor to the next state */
        getActor().setCurState(nextState);

        /* 
         *   call our next state's on-arrival turn-taking method, so that
         *   it can carry out any desired scripted behavior for our arrival
         */
        nextState.arrivingTurn();
    }

    /* initiate a topic - defer to the next state */
    initiateTopic(obj) { return nextState.initiateTopic(obj); }

    /* 
     *   Override our departure messages.  When we're accompanying another
     *   actor on a group travel, the lead actor will, as part of its turn,
     *   send each accompanying actor (including us) on ahead.  This means
     *   that the lead actor will see us departing from the starting
     *   location, because we'll leave before the lead actor has itself
     *   departed.  Rather than using the normal "Bob leaves to the west"
     *   departure report, customize the departure reports to indicate
     *   specifically that we're going with the lead actor.  (Note that we
     *   only have to handle the departing messages, since group travel
     *   always sends accompanying actors on ahead of the main actor, hence
     *   the accompanying actors will always be seen departing, not
     *   arriving.)
     *   
     *   Note that all of these call our generic sayDeparting() method by
     *   default, so a subclass can catch all of the departure types at
     *   once just by overriding sayDeparting().  Overriding the individual
     *   methods is still desirable, of course, if you want separate
     *   messages for the different departure types.  
     */
    sayDeparting(conn)
        { gLibMessages.sayDepartingWith(getActor(), leadActor); }
    sayDepartingDir(dir, conn) { sayDeparting(conn); }
    sayDepartingThroughPassage(conn) { sayDeparting(conn); }
    sayDepartingViaPath(conn) { sayDeparting(conn); }
    sayDepartingUpStairs(conn) { sayDeparting(conn); }
    sayDepartingDownStairs(conn) { sayDeparting(conn); }

    /*
     *   Describe local travel using our standard departure message as
     *   well.  This is used to describe our travel when our origin and
     *   destination locations are both visible to the PC; in these cases,
     *   we don't describe the departure separately because the whole
     *   process of travel from departure to arrival is visible to the PC
     *   and thus is best handled with a single message, which we generate
     *   here.  In our case, since the "accompanying" state describes even
     *   normal travel as though it were visible all along, we can use our
     *   standard "departing" message to describe local travel as well.  
     */
    sayArrivingLocally(dest, conn) { sayDeparting(conn); }
    sayDepartingLocally(dest, conn) { sayDeparting(conn); }
;

/* ------------------------------------------------------------------------ */
/*
 *   A pending conversation information object.  An Actor keeps a list of
 *   these for pending conversations.  
 */
class PendingConvInfo: object
    construct(state, node, turns)
    {
        /* remember how to start the conversation */
        state_ = state;
        node_ = node;

        /* compute the game clock time when we can start the conversation */
        time_ = Schedulable.gameClockTime + turns;
    }

    /* 
     *   our ActorState and ConvNode (or ConvNode name string), describing
     *   how we're to start the conversation 
     */
    state_ = nil
    node_ = nil

    /* the minimum game clock time at which we can start the conversation */
    time_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   An "agenda item."  Each actor can have its own "agenda," which is a
 *   list of these items.  Each item represents an action that the actor
 *   wants to perform - this is usually a goal the actor wants to achieve,
 *   or a conversational topic the actor wants to pursue.
 *   
 *   On any given turn, an actor can carry out only one agenda item.
 *   
 *   Agenda items are a convenient way of controlling complex behavior.
 *   Each agenda item defines its own condition for when the actor can
 *   pursue the item, and each item defines what the actor does when
 *   pursuing the item.  Agenda items can improve the code structure for an
 *   NPC's behavior, since they nicely isolate a single background action
 *   and group it with the conditions that trigger it.  But the main
 *   benefit of agenda items is the one-per-turn pacing - by executing at
 *   most one agenda item per turn, we ensure that the NPC will carry out
 *   its self-initiated actions at a measured pace, rather than as a jumble
 *   of random actions on a single turn.
 *   
 *   Note that NPC-initiated conversation messages override agendas.  If an
 *   actor has an active ConvNode, AND the ConvNode displays a
 *   "continuation message" on a given turn, then the actor will not pursue
 *   its agenda on that turn.  In this way, ConvNode continuation messages
 *   act rather like high-priority agenda items.  
 */
class AgendaItem: object
    /* 
     *   My actor - agenda items should be nested within the actor using
     *   '+' so that we can find our actor.  Note that this doesn't add the
     *   item to the actor's agenda - that has to be done explicitly with
     *   actor.addToAgenda().  
     */
    getActor() { return location; }

    /*
     *   Is this item active at the start of the game?  Override this to
     *   true to make the item initially active; we'll add it to the
     *   actor's agenda during the game's initialization.  
     */
    initiallyActive = nil

    /* 
     *   Is this item ready to execute?  The actor will only execute an
     *   agenda item when this condition is met.  By default, we're ready
     *   to execute.  Items can override this to provide a declarative
     *   condition of readiness if desired.  
     */
    isReady = true

    /*
     *   Is this item done?  On each turn, we'll remove any items marked as
     *   done from the actor's agenda list.  We remove items marked as done
     *   before executing any items, so done-ness overrides readiness; in
     *   other words, if an item is both 'done' and 'ready', it'll simply
     *   be removed from the list and will not be executed.
     *   
     *   By default, we simply return nil.  Items can override this to
     *   provide a declarative condition of done-ness, or they can simply
     *   set the property to true when they finish their work.  For
     *   example, an item that only needs to execute once can simply set
     *   isDone to true in its invokeItem() method; an item that's to be
     *   repeated until some success condition obtains can override isDone
     *   to return the success condition.  
     */
    isDone = nil

    /*
     *   The ordering of the item relative to other agenda items.  When we
     *   choose an agenda item to execute, we always choose the lowest
     *   numbered item that's ready to run.  You can leave this with the
     *   default value if you don't care about the order.  
     */
    agendaOrder = 100

    /*
     *   Execute this item.  This is invoked during the actor's turn when
     *   the item is the first item that's ready to execute in the actor's
     *   agenda list.  We do nothing by default.
     */
    invokeItem() { }

    /*
     *   Reset the item.  This is invoked whenever the item is added to an
     *   actor's agenda.  By default, we'll set isDone to nil as long as
     *   isDone isn't a method; this makes it easier to reuse agenda
     *   items, since we don't have to worry about clearing out the isDone
     *   flag when reusing an item. 
     */
    resetItem()
    {
        /* if isDone isn't a method, reset it to nil */
        if (propType(&isDone) != TypeCode)
            isDone = nil;
    }
;

/* 
 *   An AgendaItem initializer.  For each agenda item that's initially
 *   active, we'll add the item to its actor's agenda.  
 */
PreinitObject
    execute()
    {
        forEachInstance(AgendaItem, function(item) {
            /* 
             *   If this item is initially active, add the item to its
             *   actor's agenda. 
             */
            if (item.initiallyActive)
                item.getActor().addToAgenda(item);
        });
    }
;

/*
 *   A "conversational" agenda item.  This type of item is ready to execute
 *   only when the actor hasn't engaged in conversation during the same
 *   turn.  This type of item is ideal for situations where we want the
 *   actor to pursue a conversational topic, because we won't initiate the
 *   action until we get a turn where the player didn't directly talk to
 *   us.  
 */
class ConvAgendaItem: AgendaItem
    isReady = (!getActor().conversedThisTurn()
               && getActor().canTalkTo(otherActor)
               && inherited())

    /* 
     *   The actor we're planning to address - by default, this is the PC.
     *   If the conversational overture will be directed to another NPC,
     *   you can specify that other actor here. 
     */
    otherActor = (gPlayerChar)
;

/*
 *   A delayed agenda item.  This type of item becomes ready to execute
 *   when the game clock reaches a given turn counter.  
 */
class DelayedAgendaItem: AgendaItem
    /* we're ready if the game clock time has reached our ready time */
    isReady = (Schedulable.gameClockTime >= readyTime && inherited())

    /* the turn counter on the game clock when we become ready */
    readyTime = 0

    /*
     *   Set our ready time based on a delay from the current time.  We'll
     *   become ready after the given number of turns elapses.  For
     *   convenience, we return 'self', so a delayed agenda item can be
     *   initialized and added to an actor's agenda in one simple
     *   operation, like so:
     *   
     *   actor.addToAgenda(item.setDelay(1)); 
     */
    setDelay(turns)
    {
        /* 
         *   initialize our ready time as the given number of turns in the
         *   future from the current game clock time 
         */
        readyTime = Schedulable.gameClockTime + turns;

        /* return 'self' for the caller's convenience */
        return self;
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   An Actor is a living person, animal, or other entity with a will of
 *   its own.  Actors can usually be addressed with targeted commands
 *   ("bob, go north"), and with commands like ASK ABOUT, TELL ABOUT, GIVE
 *   TO, and SHOW TO.
 *   
 *   Note that, by default, an Actor can be picked up and moved with
 *   commands like TAKE, PUT IN, and so on.  This is suitable for some
 *   kinds of actors but not for others: it might make sense with a cat or
 *   a small dog, but not with a bank guard or an orc.  For an actor that
 *   can't be taken, use the UntakeableActor or one of its subclasses.
 *   
 *   An actor's contents are the things the actor is carrying or wearing.  
 */
class Actor: Thing, Schedulable, Traveler, ActorTopicDatabase
    /* flag: we're an actor */
    isActor = true

    /*
     *   Our current state.  This is an ActorState object representing what
     *   we're currently doing.  Whenever the actor changes to a new state
     *   (for example, because of a scripted activity), this can be changed
     *   to reflect the actor's new state.  The state object groups the
     *   parts of the actor's description and other methods that tend to
     *   vary according to what the actor's doing; it's easier to keep
     *   everything related to scripted activities together in a state
     *   object than it is to handle all of the variability with switch()
     *   statements of the like in methods directly in the actor.
     *   
     *   It's not necessary to initialize this if the actor doesn't take
     *   advantage of the ActorState mechanism.  If this isn't initialized
     *   for a particular actor, we'll automatically create a default
     *   ActorState object during pre-initialization.  
     */
    curState = nil

    /* set the current state */
    setCurState(state)
    {
        /* if this isn't a change of state, there's nothing to do */
        if (state == curState)
            return;
        
        /* if we have a previous state, tell it it's becoming inactive */
        if (curState != nil)
            curState.deactivateState(self, state);

        /* notify the new state it's becoming active */
        if (state != nil)
            state.activateState(self, curState);

        /* remember the new state */
        curState = state;
    }

    /*
     *   Our current conversation node.  This is a ConvNode object that
     *   keeps track of the flow of the conversation.  
     */
    curConvNode = nil

    /* 
     *   Our table of conversation nodes.  At initialization, the
     *   conversation manager scans all ConvNode instances and adds each
     *   one to its actor's table.  This table is keyed by the name of
     *   node, and the value for each entry is the ConvNode object - this
     *   lets us look up the ConvNode object by name.  Because each actor
     *   has its own lookup table, ConvNode names only have to be unique
     *   within the actor's set of ConvNodes.  
     */
    convNodeTab = perInstance(new LookupTable(32, 32))

    /* set the current conversation node */
    setConvNode(node) { setConvNodeReason(node, nil); }

    /* set the current conversation node, with a reason code */
    setConvNodeReason(node, reason)
    {
        /* remember the old node */
        local oldNode = curConvNode;
        
        /* if the node was specified by name, look up the object */
        if (dataType(node) == TypeSString)
            node = convNodeTab[node];

        /* remember the new node */
        curConvNode = node;

        /* 
         *   If we're changing to a new node, notify the new and old
         *   nodes.  Note that these notifications occur after the new
         *   node has been set, which ensures that any further node change
         *   triggered by the node change won't redundantly issue the same
         *   notifications: since the old node is no longer active, it
         *   can't receive another departure notification, and since the
         *   new node is already active, it can't receive another
         *   activation. 
         */
        if (node != oldNode)
        {
            /* if there's an old node, note that we're leaving it */
            if (oldNode != nil)
                oldNode.noteLeaving();

            /* let the node know that it's becoming active */
            if (node != nil)
                node.noteActiveReason(reason);
        }

        /* 
         *   note that we've explicitly set a ConvNode (even if it's not
         *   actually changing), in case the conversation manager is
         *   tracking what's happening during a response 
         */
        responseSetConvNode = true;
    }

    /* 
     *   conversation manager ID - this is assigned by the conversation
     *   manager to map to and from output stream references to the actor;
     *   this is only for internal use by the conversation manager
     */
    convMgrID = nil

    /* 
     *   Flag indicating whether or not we've set a ConvNode in the course
     *   of the current response.  This is for use by the converstaion
     *   manager. 
     */
    responseSetConvNode = nil

    /*
     *   Initiate a conversation with the player character.  This lets the
     *   NPC initiate a conversation, in response to something the player
     *   character does, or as part of the NPC's scripted activity.  This
     *   is only be used for situations where the NPC initiates the
     *   conversation - if the player character initiates conversation with
     *   TALK TO, ASK, TELL, etc., we handle the conversation through our
     *   normal handlers for those commands.
     *   
     *   'state' is the ActorState to switch to for the conversation.  This
     *   will normally be an InConversationState object, but doesn't have
     *   to be.
     *   
     *   You can pass nil for 'state' to use the current state's implied
     *   conversational state.  The implied conversational state of a
     *   ConversationReadyState is the associated InConversationState; the
     *   implied conversation state of any other state is simply the same
     *   state.
     *   
     *   'node' is a ConvNode object, or a string naming a ConvNode object.
     *   We'll make this our current conversation node.  A valid
     *   conversation node is required because we use this to generate the
     *   initial NPC greeting of the conversation.  In most cases, when the
     *   NPC initiates a conversation, it's because the NPC wants to ask a
     *   question or otherwise say something specific, so there should
     *   always be a conversational context implied, thus the need for a
     *   ConvNode.  If there's no need for a conversational context, the
     *   NPC script code might just as well display the conversational
     *   exchange as a plain old message, and not bother going to all this
     *   trouble.  
     */
    initiateConversation(state, node)
    {
        /* 
         *   if there's no state provided, use the current state's implied
         *   conversation state 
         */
        if (state == nil)
            state = curState.getImpliedConvState;

        /* 
         *   if there's an ActorHelloTopic for the old state, invoke it to
         *   show the greeting 
         */
        curState.handleTopic(self, actorHelloTopicObj, helloConvType, nil);

        /* switch to the new state, if it's not the current state */
        if (state != nil && state != curState)
            setCurState(state);

        /* we're now talking to the player character */
        noteConversation(gPlayerChar);

        /* switch to the conversation node */
        setConvNodeReason(node, 'initiateConversation');

        /* tell the conversation node that the NPC is initiating it */
        if (node != nil)
            curConvNode.npcInitiateConversation();
    }

    /*
     *   Initiate a conversation based on the given simulation object.
     *   We'll look for an InitiateTopic matching the given object, and if
     *   we can find one, we'll show its topic response.  
     */
    initiateTopic(obj)
    {
        /* try our current state first */
        if (curState.initiateTopic(obj))
            return true;

        /* we didn't find a state object; use the default handling */
        return inherited(obj);
    }

    /*
     *   Schedule initiation of conversation.  This allows the caller to
     *   set up a conversation to start on a future turn.  The
     *   conversation will start after (1) the given number of turns has
     *   elapsed, and (2) the player didn't target this actor with a
     *   conversational command on the same turn.  This allows us to set
     *   the NPC so that it *wants* to start a conversation, and will do
     *   so as soon as it has a chance to get a word in.
     *   
     *   If 'turns' is zero, the conversation can start the next time the
     *   actor takes a turn; so, if this is called during the PC's action
     *   processing, the conversation can start on the same turn.  Note
     *   that if this is called during the actor's takeTurn() processing,
     *   it won't actually start the conversation until the next turn,
     *   because that's the next time we'll check the queue.  If 'turns'
     *   is 1, then the player will get at least one more command before
     *   the conversation will begin, and so on with higher numbers.  
     */
    scheduleInitiateConversation(state, node, turns)
    {
        /* add a new pending conversation to our list */
        pendingConv.append(new PendingConvInfo(state, node, turns));
    }

    /*
     *   Break off our current conversation, of the NPC's own volition.
     *   This is the opposite number of initiateConversation: this causes
     *   the NPC to effectively say BYE on its own, rather than waiting
     *   for the PC to decide to end the conversation.
     *   
     *   This call is mostly useful when the actor's current state is an
     *   InConversationState, since the main function of this routine is
     *   to switch to an out-of-conversation state.  
     */
    endConversation()
    {
        /* 
         *   tell the current state to end the conversation of the NPC's
         *   own volition 
         */
        curState.endConversation(self, endConvActor);
    }

    /* 
     *   Our list of pending conversation initiators.  In our takeTurn()
     *   processing, we'll check this list for conversations that we can
     *   initiate. 
     */
    pendingConv = nil

    /* 
     *   Hide actors from 'all' by default.  The kinds of actions that
     *   normally apply to 'all' and the kinds that normally apply to
     *   actors have pretty low overlap.
     *   
     *   If a particular actor looks a lot like an inanimate object, it
     *   might want to override this to participate in 'all' for most or
     *   all actions.  
     */
    hideFromAll(action) { return true; }

    /* 
     *   don't hide actors from defaulting, though - it's frequently
     *   convenient and appropriate to assume an actor by default,
     *   especially for commands like GIVE TO and SHOW TO 
     */
    hideFromDefault(action) { return nil; }

    /* 
     *   We meet the objHeld precondition for ourself - that is, for any
     *   verb that requires holding an object, we can be considered to be
     *   holding ourself. 
     */
    meetsObjHeld(actor) { return actor == self || inherited(actor); }

    /* 
     *   Actors are not listed with the ordinary objects in a room's
     *   description.  However, an actor is listed as part of an inventory
     *   description.
     */
    isListed = nil
    isListedInContents = nil
    isListedInInventory = true

    /* the contents of an actor aren't listed in a room's description */
    contentsListed = nil

    /*
     *   Full description.  By default, we'll show either the pcDesc or
     *   npcDesc, depending on whether we're the current player character
     *   or a non-player character. 
     *   
     *   Generally, individual actors should NOT override this method.
     *   Instead, customize pcDesc and/or npcDesc to describe the permanent
     *   features of the actor.  
     */
    desc
    {
        /* 
         *   show the appropriate messages, depending on whether we're the
         *   player character or a non-player character 
         */
        if (isPlayerChar())
        {
            /* show our as-player-character description */
            pcDesc;
        }
        else
        {
            /* show our as-non-player-character description */
            npcDesc;
        }
    }

    /* show our status */
    examineStatus()
    {
        /* 
         *   If I'm an NPC, show where I'm sitting/standing/etc.  (If I'm
         *   the PC, we don't usually want to show this explicitly to avoid
         *   redundancy.  The player is usually sufficiently aware of the
         *   PC's posture by virtue of being in control of the actor, and
         *   the information also tends to show up often enough in other
         *   places, such as on the status line and in the room
         *   description.)  
         */
        if (!isPlayerChar())
            postureDesc;

        /* show the status from our state object */
        curState.stateDesc;

        /* inherit the default handling to show our contents */
        inherited();
    }

    /* 
     *   Show my posture, as part of the full EXAMINE description of this
     *   actor.  We'll let our nominal actor container handle it.  
     */
    postureDesc() { descViaActorContainer(&roomActorPostureDesc, nil); }
    
    /* 
     *   The default description when we examine this actor and the actor
     *   is serving as the player character.  This should generally not
     *   include any temporary status information; just show constant,
     *   fixed features.  
     */
    pcDesc { gLibMessages.pcDesc(self); }

    /* 
     *   Show the description of this actor when this actor is a non-player
     *   character.
     *   
     *   This description should include only the constant, fixed
     *   description of the character.  Do not include information on what
     *   the actor is doing right now, because that belongs in the
     *   ActorState object instead.  When we display the actor's
     *   description, we'll show this text, and then we'll show the
     *   ActorState description as well; this combination approach makes it
     *   easier to keep the description synchronized with any scripted
     *   activities the actor is performing.
     *   
     *   By default, we'll show this as a "default descriptive report,"
     *   since it simply says that there's nothing special to say.
     *   However, whenever this is overridden with an actual description,
     *   you shouldn't bother to use defaultDescReport - simply display the
     *   descriptive message directly:
     *   
     *   npcDesc = "He's wearing a gorilla costume. " 
     */
    npcDesc { defaultDescReport(&npcDescMsg, self); }

    /* examine my contents specially */
    examineListContents()
    {
        /* if I'm not the player character, show my inventory */
        if (!isPlayerChar())
            holdingDesc;
    }
    
    /*
     *   Always list actors specially, rather than as ordinary items in
     *   contents listings.  We'll send this to our current state object
     *   for processing, since our "I am here" description tends to vary by
     *   state.
     */
    specialDesc() { curState.specialDesc(); }
    distantSpecialDesc() { curState.distantSpecialDesc(); }
    remoteSpecialDesc(actor) { curState.remoteSpecialDesc(actor); }
    specialDescListWith() { return curState.specialDescListWith(); }

    /*
     *   By default, show the special description for an actor in the group
     *   of special descriptions that come *after* the room's portable
     *   contents listing.  An actor's presence is usually a dynamic
     *   feature of a room, and so we don't want to suggest that the actor
     *   is a permanent feature of the room by describing the actor
     *   directly with the room's main description.  
     */
    specialDescBeforeContents = nil

    /*
     *   When we're asked to show a special description as part of the
     *   description of a containing object (which will usually be a nested
     *   room of some kind), just show our posture in our container, rather
     *   than showing our full "I am here" description. 
     */
    showSpecialDescInContents(actor, cont)
    {
        /* show our posture to indicate our container */
        listActorPosture(actor);
    }

    /* 
     *   By default, put all of the actor special descriptions after the
     *   special descriptions of ordinary objects, by giving actors a
     *   higher listing order value. 
     */
    specialDescOrder = 200

    /*
     *   Get my listing group for my special description as part of a room
     *   description.  By default, we'll let our immediate location decide
     *   how we're grouped.  
     */
    actorListWith()
    {
        local group;

        /* 
         *   if our special desc is overridden, don't use any grouping by
         *   default - this make a special description defined in the
         *   actor override any grouping we'd otherwise do 
         */
        if (overrides(self, Actor, &specialDesc))
            return [];

        /* get the group for the posture */
        group = location.listWithActorIn(posture);

        /* 
         *   if we have a group, return a list containing the group;
         *   otherwise return an empty list 
         */
        return (group == nil ? [] : [group]);
    }

    /*
     *   Actor "I am here" description.  This is displayed as part of the
     *   description of a room - it describes the actor as being present in
     *   the room.  By default, we let the "nominal actor container"
     *   provide the description.  
     */
    actorHereDesc { descViaActorContainer(&roomActorHereDesc, nil); }

    /*
     *   Actor's "I am over there" description.  This is displayed in the
     *   room description when the actor is visible, but is either in a
     *   separate top-level room or is at a distance.  By default, we let
     *   the "nominal actor container" provide the description.  
     */
    actorThereDesc { descViaActorContainer(&roomActorThereDesc, nil); }

    /*
     *   Show our status, as an addendum to the given room's name (this is
     *   the room title, shown at the start of a room description and on
     *   the status line).  By default, we'll let our nominal actor
     *   container provide the status, to indicate when we're
     *   standing/sitting/lying in a nested room.
     *   
     *   In concrete terms, this generally adds a message such as "(sitting
     *   on the chair)" to the name of a room if we're in a nested room
     *   within the room.  When we're standing in the main room, this
     *   generally adds nothing.
     *   
     *   Note that we pass the room we're describing as the "container to
     *   ignore" parameter, because we don't want to say something like
     *   "Phone Booth (standing in the phone booth)" - that is, we don't
     *   want to mention the nominal container again if the nominal
     *   container is what we're naming in the first place.  
     */
    actorRoomNameStatus(room)
        { descViaActorContainer(&roomActorStatus, room); }

    /*
     *   Describe the actor via the "nominal actor container."  The nominal
     *   container is determined by our direct location.
     *   
     *   'contToIgnore' is a container to ignore.  If our nominal container
     *   is the same as this object, we'll generate a description without a
     *   mention of a container at all.
     *   
     *   The reason we have the 'contToIgnore' parameter is that the caller
     *   might already have reported our general location, and now merely
     *   wants to add that we're standing or standing or whatever.  In
     *   these cases, if we were to say that we're sitting on or standing
     *   on that same object, it would be redundant information: "Bob is in
     *   the garden, sitting in the garden."  The 'contToIgnore' parameter
     *   tells us the object that the caller has already mentioned as our
     *   general location so that we don't re-report the same thing.  We
     *   need to know the actual object, rather than just the fact that the
     *   caller mentioned a general location, because our general location
     *   and the specific place we're standing or sitting or whatever might
     *   not be the same: "Bob is in the garden, sitting in the lawn
     *   chair."
     *   
     */
    descViaActorContainer(prop, contToIgnore)
    {
        local pov;
        local cont;
        
        /* get our nominal container for our current posture */
        cont = location.getNominalActorContainer(posture);

        /* get the point of view, using the player character by default */
        if ((pov = getPOV()) == nil)
            pov = gPlayerChar;
        
        /* 
         *   if we have a nominal container, and it's not the one to
         *   ignore, and the player character can see it, generate the
         *   description via the container; otherwise, use a generic
         *   library message that doesn't mention the container 
         */
        if (cont not in (nil, contToIgnore) && pov.canSee(cont))
        {
            /* describe via the container */
            cont.(prop)(self);
        }
        else
        {
            /* use the generic library message */
            gLibMessages.(prop)(self);
        }
    }
    
    /* 
     *   Describe my inventory as part of my description - this is only
     *   called when we examine an NPC.  If an NPC doesn't wish to have
     *   its inventory listed as part of its description, it can simply
     *   override this to do nothing.  
     */
    holdingDesc
    {
        /* 
         *   show our contents as for a normal "examine", but using the
         *   special contents lister for what an actor is holding 
         */
        examineListContentsWith(holdingDescInventoryLister);
    }

    /*
     *   refer to the player character with my player character referral
     *   person, and refer to all other characters in the third person 
     */
    referralPerson { return isPlayerChar() ? pcReferralPerson : ThirdPerson; }

    /* by default, refer to the player character in the second person */
    pcReferralPerson = SecondPerson

    /*
     *   The referral person of the current command targeting the actor.
     *   This is meaningful only when a command is being directed to this
     *   actor, and this actor is an NPC.
     *   
     *   The referral person depends on the specifics of the language.  In
     *   English, a command like "bob, go north" is a second-person
     *   command, while "tell bob to go north" is a third-person command.
     *   The only reason this is important is in interpreting what "you"
     *   means if it's used as an object in the command.  "tell bob to hit
     *   you" probably means that Bob should hit the player character,
     *   while "bob, hit you" probably means that Bob should hit himself.  
     */
    commandReferralPerson = nil

    /* determine if I'm the player character */
    isPlayerChar() { return libGlobal.playerChar == self; }

    /*
     *   Implicit command handling style for this actor.  There are two
     *   styles for handling implied commands: "player" and "NPC",
     *   indicated by the enum codes ModePlayer and ModeNPC, respectively.
     *   
     *   In "player" mode, each implied command is announced with a
     *   description of the command to be performed; DEFAULT responses are
     *   suppressed; and failures are shown.  Furthermore, interactive
     *   requests for more information from the parser are allowed.
     *   Transcripts like this result:
     *   
     *   >open door
     *.  (first opening the door)
     *.  (first unlocking the door)
     *.  What do you want to unlock it with?
     *   
     *   In "NPC" mode, implied commands are treated as complete and
     *   separate commands.  They are not announced; default responses are
     *   shown; failures are NOT shown; and interactive requests for more
     *   information are not allowed.  When an implied command fails in NPC
     *   mode, the parser acts as though the command had never been
     *   attempted.
     *   
     *   By default, we return ModePlayer if we're the player character,
     *   ModeNPC if not (thus the respective names of the modes).  Some
     *   authors might prefer to use "player mode" for NPC's as well as for
     *   the player character, which is why the various parts of the parser
     *   that care about this mode consult this method rather than simply
     *   testing the PC/NPC status of the actor.  
     */
    impliedCommandMode() { return isPlayerChar() ? ModePlayer : ModeNPC; }

    /*
     *   Try moving the given object into this object.  For an actor, this
     *   will do one of two things.  If 'self' is the actor performing the
     *   action that's triggering this implied command, then we can achieve
     *   the goal simply by taking the object.  Otherwise, the way to get
     *   an object into my possession is to have the actor performing the
     *   command give me the object.  
     */
    tryMovingObjInto(obj)
    {
        if (gActor == self)
        {
            /* 
             *   I'm performing the triggering action, so I merely need to
             *   pick up the object 
             */
            return tryImplicitAction(Take, obj);
        }
        else
        {
            /* 
             *   another actor is performing the action; since that actor
             *   is the one who must perform the implied action, the way to
             *   get an object into my inventory is for that actor to give
             *   it to me 
             */
            return tryImplicitAction(GiveTo, obj, self);
        }
    }

    /* desribe our containment of an object as carrying the object */
    mustMoveObjInto(obj) { reportFailure(&mustBeCarryingMsg, obj, self); }

    /*
     *   You can limit the cumulative amount of bulk an actor can hold, and
     *   the maximum bulk of any one object the actor can hold, using
     *   bulkCapacity and maxSingleBulk.  These properties are analogous to
     *   the same ones in Container.
     *   
     *   A word of caution on these is in order.  Many authors worry that
     *   it's unrealistic if the player character can carry too much at one
     *   time, so they'll fiddle with these properties to impose a carrying
     *   limit that seems realistic.  Be advised that authors love this
     *   sort of "realism" a whole lot more than players do.  Players
     *   almost universally don't care about it, and in fact tend to hate
     *   the inventory juggling it inevitably leads to.  Juggling inventory
     *   isn't any fun for the player.  Don't fool yourself about this -
     *   the thoughts in the mind of a player who's tediously carting
     *   objects back and forth three at a time will not include admiration
     *   of your prowess at simulational realism.  In contrast, if you set
     *   the carrying limit to infinity, it's a rare player who will even
     *   notice, and a much rarer player who'll complain about it.
     *   
     *   If you really must insist on inventory limits, refer to the
     *   BagOfHolding class for a solution that can salvage most of the
     *   "realism" that the accountancy-inclined author craves, without
     *   creating undue inconvenience for the player.  BagOfHolding makes
     *   inventory limits palatable for the player by essentially
     *   automating the required inventory juggling.  In fact, for most
     *   players, an inventory limit in conjunction with a bag of holding
     *   is actually better than an unlimited inventory, since it improves
     *   readability by keeping the direct inventory list to a manageable
     *   size.  
     */
    bulkCapacity = 10000
    maxSingleBulk = 10

    /*
     *   An actor can limit the cumulative amount of weight being held,
     *   using weightCapacity.  By default we make this so large that
     *   there is effectively no limit to how much weight an actor can
     *   carry.  
     */
    weightCapacity = 10000

    /*
     *   Can I own the given object?  By default, an actor can own
     *   anything.  
     */
    canOwn(obj) { return true; }

    /*
     *   Get the preconditions for travel.  By default, we'll add the
     *   standard preconditions that the connector requires for actors.
     *   
     *   Note that these preconditions apply only when the actor is the
     *   traveler.  If the actor is in a vehicle, so that the vehicle is
     *   the traveler in a given travel operation, the vehicle's
     *   travelerPreCond conditions are used instead of ours.  
     */
    travelerPreCond(conn) { return conn.actorTravelPreCond(self); }

    /* by default, actors are listed when they arrive aboard a vehicle */
    isListedAboardVehicle = true

    /*
     *   Get the object that's actually going to move when this actor
     *   travels via the given connector.  In most cases this is simply the
     *   actor; but when the actor is in a vehicle, travel commands move
     *   the vehicle, not the actor: the actor stays in the vehicle while
     *   the vehicle moves to a new location.  We determine this by asking
     *   our immediate location what it thinks about the situation.
     *   
     *   If we have a special traveler explicitly set, it overrides the
     *   traveler indicated by the location.  
     */
    getTraveler(conn)
    {
        /* 
         *   Return our special traveler if we have one; otherwise, if we
         *   have a location, return the traveler indicated by our
         *   location; otherwise, we're the traveler. 
         */
        if (specialTraveler != nil)
            return specialTraveler;
        else if (location != nil)
            return location.getLocTraveler(self, conn);
        else
            return self;
    }

    /*
     *   Get the "push traveler" for the actor.  This is the nominal
     *   traveler that we want to use when the actor enters a command like
     *   PUSH BOX NORTH.  'obj' is the object we're trying to push.  
     */
    getPushTraveler(obj)
    {
        /* 
         *   If we already have a special traveler, just use the special
         *   traveler.  Otherwise, if we have a location, ask the location
         *   what it thinks.  Otherwise, we're the traveler. 
         */
        if (specialTraveler != nil)
            return specialTraveler;
        else if (location != nil)
            return location.getLocPushTraveler(self, obj);
        else
            return self;
    }

    /* is an actor traveling with us? */
    isActorTraveling(actor)
    {
        /* we're the only actor traveling when we're the traveler */
        return (actor == self);
    }

    /* invoke a callback on each actor traveling with the traveler */
    forEachTravelingActor(func)
    {
        /* we're the only actor, so simply invoke the callback on myself */
        (func)(self);
    }

    /* 
     *   Get the actors involved in travel, when we're acting in our role
     *   as a Traveler.  When the Traveler is simply the Actor, the only
     *   actor involved in the travel is 'self'. 
     */
    getTravelerActors = [self]

    /* we're the self-motive actor doing the travel */
    getTravelerMotiveActors = [self]

    /*
     *   Set the "special traveler."  When this is set, we explicitly
     *   perform travel through this object rather than through the
     *   traveler indicated by our location.  Returns the old value, so
     *   that the old value can be restored when the caller has finished
     *   its need for the special traveler.  
     */
    setSpecialTraveler(traveler)
    {
        local oldVal;

        /* remember the old value so that we can return it */
        oldVal = specialTraveler;

        /* remember the new value */
        specialTraveler = traveler;

        /* return the old value */
        return oldVal;
    }

    /* our special traveler */
    specialTraveler = nil

    /*
     *   Try moving the actor into the given room in preparation for
     *   travel, using pre-condition rules. 
     */
    checkMovingTravelerInto(room, allowImplicit)
    {
        /* try moving the actor into the room */
        return room.checkMovingActorInto(allowImplicit);
    }

    /*
     *   Check to ensure the actor is ready to enter the given nested
     *   room, using pre-condition rules.  By default, we'll ask the given
     *   nested room to handle it.  
     */
    checkReadyToEnterNestedRoom(dest, allowImplicit)
    {
        /* ask the destination to do the work */
        return dest.checkActorReadyToEnterNestedRoom(allowImplicit);
    }

    /*
     *   Travel within a location, as from a room to a contained nested
     *   room.  This should generally be used in lieu of travelTo when
     *   traveling between locations that are related directly by
     *   containment rather than with TravelConnector objects.
     *   
     *   Travel within a location is not restricted by darkness; we assume
     *   that if the nested objects are in scope at all, travel among them
     *   is allowed.
     *   
     *   This type of travel does not trigger calls to travelerLeaving()
     *   or travelerArriving().  To mitigate this loss of notification, we
     *   call actorTravelingWithin() on the source and destination
     *   objects.  
     */
    travelWithin(dest)
    {
        /* if I'm not going anywhere, ignore the operation */
        if (dest == location)
            return;

        /* 
         *   Notify the traveler.  Note that since this is local travel
         *   within a single top-level location, there's no connector. 
         */
        getTraveler(nil).travelerTravelWithin(self, dest);
    }

    /*
     *   Traveler interface: perform local travel, between nested rooms
     *   within a single top-level location. 
     */
    travelerTravelWithin(actor, dest)
    {
        local origin;

        /* remember my origin */
        origin = location;
        
        /* notify the source that we're traveling within a room */
        if (origin != nil)
            origin.actorTravelingWithin(origin, dest);

        /* 
         *   if our origin and destination have different effective follow
         *   locations, track the follow 
         */
        if (origin != nil
            && dest != nil
            && origin.effectiveFollowLocation != dest.effectiveFollowLocation)
        {
            /* 
             *   notify observing objects of the travel; we're not moving
             *   along a connector, so there is no connector associated
             *   with the tracking information 
             */
            connectionTable().forEachAssoc(
                {obj, val: obj.beforeTravel(self, nil)});
        }

        /* move me to the destination */
        moveInto(dest);

        /* 
         *   recalculate the global sense context for message generation
         *   purposes, since we've moved to a new location 
         */
        if (gAction != nil)
            gAction.recalcSenseContext();

        /* notify the destination of the interior travel */
        if (dest != nil)
            dest.actorTravelingWithin(origin, dest);
    }

    /*
     *   Check for travel in the dark.  If we're in a dark room, and our
     *   destination is a dark room, ask the connector for guidance.
     *   
     *   Travel connectors normally call this before invoking our
     *   travelTo() method to carry out the travel.  The darkness check
     *   usually must be made before any barrier checks.  
     */
    checkDarkTravel(dest, connector)
    {
        local origin;
        
        /* 
         *   If we're not in the dark in the current location, there's no
         *   need to check for dark-to-dark travel; light-to-dark travel
         *   is always allowed. 
         */
        if (isLocationLit())
            return;

        /* get the origin - this is the traveler's location */
        origin = getTraveler(connector).location;

        /*
         *   Check to see if the connector itself is visible in the dark.
         *   If it is, then allow the travel without restriction.  
         */
        if (connector.isConnectorVisibleInDark(origin, self))
            return;

        /*
         *   We are attempting dark-to-dark travel.  We allow or disallow
         *   this type of travel on a per-connector basis, so ask the
         *   connector to handle it.  If the connector wishes to disallow
         *   the travel, it will display an appropriate failure report and
         *   terminate the command with 'exit'.  
         */
        connector.darkTravel(self, dest);
    }

    /*
     *   Travel to a new location. 
     */
    travelTo(dest, connector, backConnector)
    {
        /* send the request to the traveler */
        getTraveler(connector)
            .travelerTravelTo(dest, connector, backConnector);
    }

    /*
     *   Perform scripted travel to the given adjacent location.  This
     *   looks for a directional connector in our current location whose
     *   destination is the given location, and for a corresponding
     *   back-connector in the destination location.  If we can find the
     *   connectors, we'll perform the travel using travelTo().
     *   
     *   The purpose of this routine is to simplify scripted travel for
     *   simple cases where directional connectors are available for the
     *   desired travel.  This routine is NOT suitable for intelligent
     *   goal-seeking NPC's who automatically try to find their own routes,
     *   for two reasons.  First, this routine only lets an NPC move to an
     *   *adjacent* location; it won't try to find a path between arbitrary
     *   locations.  Second, this routine is "omniscient": it doesn't take
     *   into account what the NPC knows about the connections between
     *   locations, but simply finds a connector that actually provides the
     *   desired travel.
     *   
     *   What this routine *is* suitable for are cases where we have a
     *   pre-scripted series of NPC travel actions, where we have a list of
     *   rooms we want the NPC to visit in order.  This routine simplifies
     *   this type of scripting by automatically finding the connectors;
     *   the script only has to specify the next location for the NPC to
     *   visit.  
     */
    scriptedTravelTo(dest)
    {
        local conn;

        /* find a connector from the current location to the new location */
        conn = location.getConnectorTo(self, dest);

        /* if we found the connector, perform the travel */
        if (conn != nil)
            nestedActorAction(self, TravelVia, conn);
    }

    /*
     *   Remember the last door I traveled through.  We use this
     *   information for disambiguation, to boost the likelihood that an
     *   actor that just traveled through a door is referring to the same
     *   door in a subsequent "close" command.  
     */
    rememberLastDoor(obj) { lastDoorTraversed = obj; }

    /*
     *   Remember our most recent travel.  If we know the back connector
     *   (i.e., the connector that reverses the travel we're performing),
     *   then we'll be able to accept a GO BACK command to attempt to
     *   return to the previous location.  
     */
    rememberTravel(origin, dest, backConnector)
    {
        /* remember the destination of the travel, and the connector back */
        lastTravelDest = dest;
        lastTravelBack = backConnector;
    }

    /*
     *   Reverse the most recent travel.  If we're still within the same
     *   destination we reached in the last travel, and we know the
     *   connector we arrived through (i.e., the "back connector" for the
     *   last travel, which reverses the connector we took to get here),
     *   then try traveling via the connector.  
     */
    reverseLastTravel()
    {
        /* 
         *   If we don't know the connector back to our previous location,
         *   we obviously can't reverse the travel.  If we're not still in
         *   the same location as the previous travel's destination, then
         *   we can't reverse the travel either, because the back
         *   connector isn't applicable to our current location.  (This
         *   latter condition could only happen if we've been moved
         *   somewhere without ordinary travel occurring, but this is a
         *   possibility.) 
         */
        if (lastTravelBack == nil
            || lastTravelDest == nil
            || !isIn(lastTravelDest))
        {
            reportFailure(&cannotGoBackMsg);
            exit;
        }

        /* attempt travel via our back connector */
        nestedAction(TravelVia, lastTravelBack);
    }

    /* the last door I traversed */
    lastDoorTraversed = nil

    /* the destination and back connector for our last travel */
    lastTravelDest = nil
    lastTravelBack = nil

    /* 
     *   use a custom message for cases where we're holding a destination
     *   object for BOARD, ENTER, etc 
     */
    checkStagingLocation(dest)
    {
        /* 
         *   if the destination is within us, explain specifically that
         *   this is the problem 
         */
        if (dest.isIn(self))
            reportFailure(&invalidStagingContainerActorMsg, self, dest);
        else
            inherited(dest);

        /* terminate the command */
        exit;
    }

    /* 
     *   Travel arrival/departure messages.  Defer to the current state
     *   object on all of these.  
     */
    sayArriving(conn)
        { curState.sayArriving(conn); }
    sayDeparting(conn)
        { curState.sayDeparting(conn); }
    sayArrivingLocally(dest, conn)
        { curState.sayArrivingLocally(dest, conn); }
    sayDepartingLocally(dest, conn)
        { curState.sayDepartingLocally(dest, conn); }
    sayTravelingRemotely(dest, conn)
        { curState.sayTravelingRemotely(dest, conn); }
    sayArrivingDir(dir, conn)
        { curState.sayArrivingDir(dir, conn); }
    sayDepartingDir(dir, conn)
        { curState.sayDepartingDir(dir, conn); }
    sayArrivingThroughPassage(conn)
        { curState.sayArrivingThroughPassage(conn); }
    sayDepartingThroughPassage(conn)
        { curState.sayDepartingThroughPassage(conn); }
    sayArrivingViaPath(conn)
        { curState.sayArrivingViaPath(conn); }
    sayDepartingViaPath(conn)
        { curState.sayDepartingViaPath(conn); }
    sayArrivingUpStairs(conn)
        { curState.sayArrivingUpStairs(conn); }
    sayArrivingDownStairs(conn)
        { curState.sayArrivingDownStairs(conn); }
    sayDepartingUpStairs(conn)
        { curState.sayDepartingUpStairs(conn); }
    sayDepartingDownStairs(conn)
        { curState.sayDepartingDownStairs(conn); }

    /*
     *   Get the current interlocutor.  By default, we'll address new
     *   conversational commands (ASK ABOUT, TELL ABOUT, SHOW TO) to the
     *   last conversational partner, if that actor is still within range.
     */
    getCurrentInterlocutor()
    {
        /* 
         *   if we've talked to someone before, and we can still talk to
         *   them now, return that actor; otherwise we have no default 
         */
        if (lastInterlocutor != nil && canTalkTo(lastInterlocutor))
            return lastInterlocutor;
        else
            return nil;
    }

    /*
     *   Get the default interlocutor.  If there's a current interlocutor,
     *   and we can still talk to that actor, then that's the default
     *   interlocutor.  If not, we'll return whatever actor is the default
     *   for a TALK TO command.  Note that TALK TO won't necessarily have a
     *   default actor; if it doesn't, we'll simply return nil.  
     */
    getDefaultInterlocutor()
    {
        local actor;
        
        /* check for a current interlocutor */
        actor = getCurrentInterlocutor();

        /* 
         *   if we're not talking to anyone, or if the person we were
         *   talking to can no longer hear us, look for a default object
         *   for a TALK TO command and use it instead as the default 
         */
        if (actor == nil || !canTalkTo(actor))
        {
            /* set up a TALK TO command and a resolver */
            local tt = new TalkToAction();
            local res = new Resolver(tt, gIssuingActor, gActor);
            
            /* get the default direct object */
            actor = tt.getDefaultDobj(new EmptyNounPhraseProd(), res);
            
            /* if that worked, get the object from the resolve info */
            if (actor != nil)
                actor = actor[1].obj_;
        }

        /* return what we found */
        return actor;
    }

    /* 
     *   The most recent actor that we've interacted with through a
     *   conversational command (ASK, TELL, GIVE, SHOW, etc).
     */
    lastInterlocutor = nil

    /* 
     *   Our conversational "boredom" counter.  While we're in a
     *   conversation, this tracks the number of turns since the last
     *   conversational command from the actor we're talking to.
     *   
     *   Note that this state is part of the actor, even though it's
     *   usually managed by the InConversationState object.  The state is
     *   stored with the actor rather than with the state object because
     *   it really describes the condition of the actor, not of the state
     *   object.  
     */
    boredomCount = 0

    /* 
     *   game-clock time (Schedulable.gameClockTime) of the last
     *   conversational command addressed to us by the player character 
     */
    lastConvTime = -1

    /* 
     *   Did we engage in any conversation on the current turn?  This can
     *   be used as a quick check in background activity scripts when we
     *   want to run a step only in the absence of any conversation on the
     *   same turn. 
     */
    conversedThisTurn() { return lastConvTime == Schedulable.gameClockTime; }

    /* 
     *   Note that we're performing a conversational command targeting the
     *   given actor.  We'll make the actors point at each other with their
     *   'lastInterlocutor' properties.  This is called on the character
     *   performing the conversation command: if the player types ASK BOB
     *   ABOUT BOOK, this will be called on the player character actor,
     *   with 'other' set to Bob.  
     */
    noteConversation(other)
    {
        /* note that we're part of a conversational action */
        noteConvAction(other);

        /* let the other actor know we're conversing with them */
        other.noteConversationFrom(self);
    }

    /*
     *   Note that another actor is issuing a conversational command
     *   targeting us.  For example, if the player types ASK BOB ABOUT
     *   BOOK, then this will be called on Bob, with the player character
     *   actor as 'other'. 
     */
    noteConversationFrom(other)
    {
        /* note that we're part of a conversational action */
        noteConvAction(other);
    }

    /* 
     *   Note that we're taking part in a conversational action with
     *   another character.  This is symmetrical - it could mean we're the
     *   initiator of the conversation action or the target.  We'll
     *   remember the person we're talking to, and reset our conversation
     *   time counters so we know we've conversed on this turn.  
     */
    noteConvAction(other)
    {
        /* note our last conversational partner */
        lastInterlocutor = other;

        /* set the actor to be the pronoun antecedent */
        setPronounObj(other);

        /* 
         *   reset our boredom counter, as the other actor has just spoken
         *   to us 
         */
        boredomCount = 0;

        /* remember the time of our last conversation from the PC */
        lastConvTime = Schedulable.gameClockTime;
    }

    /* note that we're consulting an item */
    noteConsultation(obj) { lastConsulted = obj; }

    /*
     *   Receive notification that a TopicEntry response in our database is
     *   being invoked.  We'll just pass this along to our current state.  
     */
    notifyTopicResponse(fromActor, entry)
    {
        /* let our current state handle it */
        curState.notifyTopicResponse(fromActor, entry);
    }

    /* the object we most recently consulted */
    lastConsulted = nil

    /*
     *   The actor's "agenda."  This is a list of AgendaItem objects that
     *   describe things the actor wants to do of its own volition on its
     *   own turn. 
     */
    agendaList = nil

    /* 
     *   our special "boredom" agenda item - this makes us initiate an end
     *   to an active conversation when the PC has ignored us for a given
     *   number of consecutive turns 
     */
    boredomAgendaItem = perInstance(new BoredomAgendaItem(self))

    /* add an agenda item */
    addToAgenda(item)
    {
        /* if we don't have an agenda list yet, create one */
        if (agendaList == nil)
            agendaList = new Vector(10);

        /* add the item */
        agendaList.append(item);

        /* 
         *   keep the list in ascending order of agendaOrder values - this
         *   will ensure that we'll always choose the earliest item that's
         *   ready to run 
         */
        agendaList.sort(SortAsc, {a, b: a.agendaOrder - b.agendaOrder});

        /* reset the agenda item */
        item.resetItem();
    }

    /* remove an agenda item */
    removeFromAgenda(item)
    {
        /* if we have an agenda list, remove the item */
        if (agendaList != nil)
            agendaList.removeElement(item);
    }

    /*
     *   Execute the next item in our agenda, if there are any items in the
     *   agenda that are ready to execute.  We'll return true if we found
     *   an item to execute, nil if not.  
     */
    executeAgenda()
    {
        local item;

        /* if we don't have an agenda, there are obviously no items */
        if (agendaList == nil)
            return nil;
        
        /* remove any items that are marked as done */
        while ((item = agendaList.lastValWhich({x: x.isDone})) != nil)
            agendaList.removeElement(item);

        /* 
         *   Scan for an item that's ready to execute.  Since we keep the
         *   list sorted in ascending order of agendaOrder values, we can
         *   just pick the earliest item in the list that's ready to run,
         *   since that will be the ready-to-run item with the lowest
         *   agendaOrder number. 
         */
        item = agendaList.valWhich({x: x.isReady});

        /* if we found an item, execute it */
        if (item != nil)
        {
            try
            {
                /* execute the item */
                item.invokeItem();
            }
            catch (RuntimeError err)
            {
                /* 
                 *   If an error occurs while executing the item, mark the
                 *   item as done.  This will ensure that we won't get
                 *   stuck in a loop trying to execute the same item over
                 *   and over, which will probably just run into the same
                 *   error on each attempt.  
                 */
                item.isDone = true;

                /* re-throw the exception */
                throw err;
            }

            /* tell the caller we found an item to execute */
            return true;
        }
        else
        {
            /* tell the caller we found no agenda item */
            return nil;
        }
    }

    /*
     *   Calculate the amount of bulk I'm holding directly.  By default,
     *   we'll simply add up the "actor-encumbering bulk" of each of our
     *   direct contents.
     *   
     *   Note that we don't differentiate here based on whether or not an
     *   item is being worn, or anything else - we deliberately leave such
     *   distinctions up to the getEncumberingBulk routine, so that only
     *   the objects are in the business of deciding how bulky they are
     *   under different circumstances.  
     */
    getBulkHeld()
    {
        local total;

        /* start with nothing */
        total = 0;

        /* add the bulks of directly-contained items */
        foreach (local cur in contents)
            total += cur.getEncumberingBulk(self);

        /* return the total */
        return total;
    }

    /*
     *   Calculate the total weight I'm holding.  By default, we'll add up
     *   the "actor-encumbering weight" of each of our direct contents.
     *   
     *   Note that we deliberately only consider our direct contents.  If
     *   any of the items we are directly holding contain further items,
     *   getEncumberingWeight will take their weights into account; this
     *   frees us from needing any special knowledge of the internal
     *   structure of any items we're holding, and puts that knowledge in
     *   the individual items where it belongs.  
     */
    getWeightHeld()
    {
        local total;

        /* start with nothing */
        total = 0;

        /* add the weights of directly-contained items */
        foreach (local cur in contents)
            total += cur.getEncumberingWeight(self);

        /* return the total */
        return total;
    }

    /*
     *   Try making room to hold the given object.  This is called when
     *   checking the "room to hold object" pre-condition, such as for the
     *   "take" verb.  
     *   
     *   If holding the new object would exceed the our maximum holding
     *   capacity, we'll go through our inventory looking for objects that
     *   can reduce our held bulk with implicit commands.  Objects with
     *   holding affinities - "bags of holding", keyrings, and the like -
     *   can implicitly shuffle the actor's possessions in a manner that
     *   is neutral as far as the actor is concerned, thereby reducing our
     *   active holding load.
     *   
     *   Returns true if an implicit command was attempted, nil if not.  
     */
    tryMakingRoomToHold(obj, allowImplicit)
    {
        local objWeight;
        local objBulk;
        local aff;
        
        /* get the amount of weight this will add if taken */
        objWeight = obj.getEncumberingWeight(self);

        /* 
         *   If this object alone is too heavy for us, give up.  We
         *   distinguish this case from the case where the total (of
         *   everything held plus the new item) is too heavy: in the
         *   latter case we can tell the actor that they can pick this up
         *   by dropping something else first, whereas if this item alone
         *   is too heavy, no such advice is warranted. 
         */
        if (objWeight > weightCapacity)
        {
            reportFailure(&tooHeavyForActorMsg, obj);
            exit;
        }

        /* 
         *   if taking the object would push our total carried weight over
         *   our total carrying weight limit, give up 
         */
        if (obj.whatIfHeldBy({: getWeightHeld()}, self) > weightCapacity)
        {
            reportFailure(&totalTooHeavyForMsg, obj);
            exit;
        }

        /* get the amount of bulk the object will add */
        objBulk = obj.getEncumberingBulk(self);
        
        /* 
         *   if the object is simply too big to start with, we can't make
         *   room no matter what we do 
         */
        if (objBulk > maxSingleBulk || objBulk > bulkCapacity)
        {
            reportFailure(&tooLargeForActorMsg, obj);
            exit;
        }

        /*
         *   Test what would happen to our bulk if we were to move the
         *   object into our directly held inventory.  Do this by running
         *   a "what if" scenario to test moving the object into our
         *   inventory, and check what effect it has on our held bulk.  If
         *   it fits, we can let the caller proceed without further work.  
         */
        if (obj.whatIfHeldBy({: getBulkHeld()}, self) <= bulkCapacity)
            return nil;

        /* 
         *   if we're not allowed to run implicit commands, we won't be
         *   able to accomplish anything, so give up 
         */
        if (!allowImplicit)
        {
            reportFailure(&handsTooFullForMsg, obj);
            exit;
        }

        /* 
         *   Get "bag of holding" affinity information for my immediate
         *   contents.  Consider only objects with encumbering bulk, since
         *   it will do us no good to move objects without any encumbering
         *   bulk.  Also ignore objects that aren't being held (some direct
         *   contents aren't considered to be held, such as clothing being
         *   worn).  
         */
        aff = getBagAffinities(contents.subset(
            {x: x.getEncumberingBulk(self) != 0 && x.isHeldBy(self)}));

        /* if there are no bag affinities, we can't move anything around */
        if (aff.length() == 0)
        {
            reportFailure(&handsTooFullForMsg, obj);
            exit;
        }

        /*
         *   If we have at least four items, find the two that were picked
         *   up most recently (according to the "holding index" value) and
         *   move them to the end of the list.  In most cases, we'll only
         *   have to dispose of one or two items to free up enough space
         *   in our hands, so we'll probably never get to the last couple
         *   of items in our list, so we're effectively ruling out moving
         *   these two most recent items; but they'll be in the list if we
         *   do find we need to move them after all.
         *   
         *   The point of this rearrangement is to avoid annoying cases of
         *   moving something we just picked up, especially if we just
         *   picked it up in order to carry out the command that's making
         *   us free up more space now.  This looks especially stupid when
         *   we perform some command that requires picking up two items
         *   automatically: we pick up the first, then we put it away in
         *   order to pick up the second, but then we find that we need
         *   the first again.  
         */
        if (aff.length() >= 4)
        {
            local a, b;
            
            /* remove the two most recent items from the vector */
            a = BagAffinityInfo.removeMostRecent(aff);
            b = BagAffinityInfo.removeMostRecent(aff);

            /* re-insert them at the end of the vector */
            aff.append(b);
            aff.append(a);
        }
        
        /*
         *   Move each object in the list until we have reduced the bulk
         *   sufficiently. 
         */
        foreach (local cur in aff)
        {
            /* 
             *   Try moving this object to its bag.  If the bag is itself
             *   inside this object, don't even try, since that would be an
             *   attempt at circular containment.
             *   
             *   If the object we're trying to hold is inside this object,
             *   don't move the object.  That might put the object we're
             *   trying to hold out of reach, since moving an object into a
             *   bag could involve closing the object or making its
             *   contents not directly accessible.  
             */
            if (!cur.bag_.isIn(cur.obj_)
                && !obj.isIn(cur.obj_)
                && cur.bag_.tryPuttingObjInBag(cur.obj_))
            {
                /* 
                 *   this routine tried tried to move the object into the
                 *   bag - check our held bulk to see if we're in good
                 *   enough shape yet
                 */
                if (obj.whatIfHeldBy({: getBulkHeld()}, self) <= bulkCapacity)
                {
                    /* 
                     *   We've met our condition - there's no need to look
                     *   any further.  Return, telling the caller we've
                     *   performed an implicit command.  
                     */
                    return true;
                }
            }
        }
        
        /*
         *   If we get this far, it means that we tried every child object
         *   but failed to find anything that could help.  Explain the
         *   problem and abort the command.  
         */
        reportFailure(&handsTooFullForMsg, obj);
        exit;
    }

    /*
     *   Check a bulk change of one of my direct contents. 
     */
    checkBulkChangeWithin(obj)
    {
        local objBulk;
        
        /* get the object's new bulk */
        objBulk = obj.getEncumberingBulk(self);
        
        /* 
         *   if this change would cause the object to exceed our
         *   single-item bulk limit, don't allow it 
         */
        if (objBulk > maxSingleBulk || objBulk > bulkCapacity)
        {
            reportFailure(&becomingTooLargeForActorMsg, obj);
            exit;
        }

        /* 
         *   If our total carrying capacity is exceeded with this change,
         *   don't allow it.  Note that 'obj' is already among our
         *   contents when this routine is called, so we can simply check
         *   our current total bulk within.  
         */
        if (getBulkHeld() > bulkCapacity)
        {
            reportFailure(&handsBecomingTooFullForMsg, obj);
            exit;
        }
    }

    /* 
     *   Next available "holding index" value.  Each time we pick up an
     *   item, we'll assign it our current holding index value and then
     *   increment our value.  This gives us a simple way to keep track of
     *   the order in which we picked up items we're carrying.
     *   
     *   Note that we make the simplifying assumption that an object can
     *   be held by only one actor at a time (multi-location items are
     *   generally not portable), which means that we can use a simple
     *   property in each object being held to store its holding index.  
     */
    nextHoldingIndex = 1

    /* add an object to my contents */
    addToContents(obj)
    {
        /* assign the new object our next holding index */
        obj.holdingIndex = nextHoldingIndex++;

        /* inherit default handling */
        inherited(obj);
    }

    /*
     *   Go to sleep.  This is used by the 'Sleep' action to carry out the
     *   command.  By default, we simply say that we're not sleepy; actors
     *   can override this to cause other actions.  
     */
    goToSleep()
    {
        /* simply report that we can't sleep now */
        mainReport(&cannotSleepMsg);
    }

    /*
     *   My current "posture," which specifies how we're positioned with
     *   respect to our container; this is one of the standard library
     *   posture enum values (Standing, etc.) or another posture added by
     *   the game.  
     */
    posture = standing

    /*
     *   Get a default acknowledgment of a change to our posture.  This
     *   should acknowledge the posture so that it tells us the current
     *   posture.  This is used for a command such as "stand up" from a
     *   chair, so that we can report the appropriate posture status in
     *   our acknowledgment; we might end up being inside another nested
     *   container after standing up from the chair, so we might not
     *   simply be standing when we're done.   
     */
    okayPostureChange()
    {
        /* get our nominal container for our current posture */
        local cont = location.getNominalActorContainer(posture);

        /* if the container is visible, let it handle it */
        if (cont != nil && gPlayerChar.canSee(cont))
        {
            /* describe via the container */
            cont.roomOkayPostureChange(self);
        }
        else
        {
            /* use the generic library message */
            defaultReport(&okayPostureChangeMsg, posture);
        }
    }

    /*
     *   Describe the actor as part of the EXAMINE description of a nested
     *   room containing the actor.  'povActor' is the actor doing the
     *   looking.  
     */
    listActorPosture(povActor)
    {
        /* get our nominal container for our current posture */
        local cont = location.getNominalActorContainer(posture);
        
        /* if the container is visible, let it handle it */
        if (cont != nil && povActor.canSee(cont))
            cont.roomListActorPosture(self);
    }

    /*
     *   Stand up.  This is used by the 'Stand' action to carry out the
     *   command. 
     */
    standUp()
    {
        /* if we're already standing, say so */
        if (posture == standing)
        {
            reportFailure(&alreadyStandingMsg);
            return;
        }

        /* ask the location to make us stand up */
        location.makeStandingUp();
    }

    /*
     *   Disembark.  This is used by the 'Get out' action to carry out the
     *   command.  By default, we'll let the room handle it.  
     */
    disembark()
    {
        /* let the room handle it */
        location.disembarkRoom();
    }

    /* 
     *   Set our posture to the given status.  By default, we'll simply
     *   set our posture property to the new status, but actors can
     *   override this to handle side effects of the change.
     */
    makePosture(newPosture)
    {
        /* remember our new posture */
        posture = newPosture;
    }

    /* 
     *   Display a description of the actor's location from the actor's
     *   point of view.
     *   
     *   If 'verbose' is true, then we'll show the full description in all
     *   cases.  Otherwise, we'll show the full description if the actor
     *   hasn't seen the location before, or the terse description if the
     *   actor has previously seen the location.  
     */
    lookAround(verbose)
    {
        /* turn on the sense cache while we're looking */
        libGlobal.enableSenseCache();
        
        /* show a description of my immediate location, if I have one */
        if (location != nil)
            location.lookAroundPov(self, self, verbose);

        /* turn off the sense cache now that we're done */
        libGlobal.disableSenseCache();
    }

    /*
     *   Get my "look around" location name as a string.  This returns a
     *   string containing the location name that we display in the status
     *   line or at the start of a "look around" description of my
     *   location.  
     */
    getLookAroundName()
    {
        return mainOutputStream.captureOutput(
            {: location.lookAroundWithinName(self, getVisualAmbient()) })
            .specialsToText();
    }

    /*
     *   Adjust a table of visible objects for 'look around'.  By default,
     *   we remove any explicitly excluded objects.  
     */
    adjustLookAroundTable(tab, pov, actor)
    {
        /* remove any explicitly excluded objects */
        foreach (local cur in excludeFromLookAroundList)
            tab.removeElement(cur);

        /* inherit the base handling */
        inherited(tab, pov, actor);
    }

    /*
     *   Add an object to the 'look around' exclusion list.  Returns true
     *   if the object was already in the list, nil if not.  
     */
    excludeFromLookAround(obj)
    {
        /* 
         *   if the object is already in the list, don't add it again -
         *   just tell the caller it's already there
         */
        if (excludeFromLookAroundList.indexOf(obj) != nil)
            return true;

        /* add it to the list and tell the caller it wasn't already there */
        excludeFromLookAroundList.append(obj);
        return nil;
    }

    /* remove an object from the 'look around' exclusion list */
    unexcludeFromLookAround(obj)
    {
        excludeFromLookAroundList.removeElement(obj);
    }

    /*
     *   Our list of objects explicitly excluded from 'look around'.  These
     *   objects will be suppressed from any sort of listing (including in
     *   the room's contents list and in special descriptions) in 'look
     *   around' when this actor is doing the looking. 
     */
    excludeFromLookAroundList = perInstance(new Vector(5))

    /*
     *   Get the location into which objects should be moved when the
     *   actor drops them with an explicit 'drop' command.  By default, we
     *   return the drop destination of our current container.  
     */
    getDropDestination(objToDrop, path)
    {
        return (location != nil
                ? location.getDropDestination(objToDrop, path)
                : nil);
    }

    /*
     *   The senses that determine scope for this actor.  An actor might
     *   possess only a subset of the defined sense.
     *   
     *   By default, we give each actor all of the human senses that we
     *   define, except touch.  In general, merely being able to touch an
     *   object doesn't put the object in scope, because if an object
     *   isn't noticed through some other sense, touch would only make an
     *   object accessible if it's within arm's reach, which for our
     *   purposes means that the object is being held directly by the
     *   actor.  Imagine an actor in a dark room: lots of things might be
     *   touchable in the sense that there's no physical barrier to
     *   touching them, but without some other sense to locate the
     *   objects, the actor wouldn't have any way of knowing where to
     *   reach to touch things, so they're not in scope.  So, touch isn't
     *   a scope sense.  
     */
    scopeSenses = [sight, sound, smell]

    /*
     *   "Sight-like" senses: these are the senses that operate like sight
     *   for the actor, and which the actor can use to determine the names
     *   of objects and the spatial relationships between objects.  These
     *   senses should operate passively, in the sense that they should
     *   tend to collect sensory input continuously and without explicit
     *   action by the actor, the way sight does and the way touch, for
     *   example, does not.  These senses should also operate instantly,
     *   in the sense that the sense can reasonably take in most or all of
     *   a location at one time.
     *   
     *   These senses are used to determine what objects should be listed
     *   in room descriptions, for example.
     *   
     *   By default, the only sight-like sense is sight, since other human
     *   senses don't normally provide a clear picture of the spatial
     *   relationships among objects.  (Touch could with some degree of
     *   effort, but it can't operate passively or instantly, since
     *   deliberate and time-consuming action would be necessary.)
     *   
     *   An actor can have more than one sight-like sense, in which case
     *   the senses will act effectively as one sense that can reach the
     *   union of objects reachable through the individual senses.  
     */
    sightlikeSenses = [sight]

    /* 
     *   Hearing-like senses.  These are senses that the actor can use to
     *   hear objects. 
     */
    hearinglikeSenses = [sound]

    /*
     *   Smell-like senses.  These are senses that the actor can use to
     *   smell objects. 
     */
    smelllikeSenses = [smell]

    /*
     *   Communication senses: these are the senses through which the
     *   actor can communicate directly with other actors through commands
     *   and messages.
     *   
     *   Conceptually, these senses are intended to be only those senses
     *   that the actors would *naturally* use to communicate, because
     *   senses in this list allow direct communications via the most
     *   ordinary game commands, such as "bob, go east".
     *   
     *   If some form of indirect communication is possible via a sense,
     *   but that form is not something the actor would think of as the
     *   most natural, default form of communication, it should *not* be
     *   in this list.  For example, two sighted persons who can see one
     *   another but cannot hear one another could still communicate by
     *   writing messages on pieces of paper, but they would ordinarily
     *   communicate by talking.  In such a case, sound should be in the
     *   list but sight should not be, because sight is not a natural,
     *   default form of communications for the actors.  
     */
    communicationSenses = [sound]
    
    /*
     *   Determine if I can communicate with the given character via a
     *   natural, default form of communication that we share with the
     *   other character.  This determines if I can talk to the other
     *   character.  We'll return true if I can talk to the other actor,
     *   nil if not.
     *   
     *   In order for the player character to issue a command to a
     *   non-player character (as in "bob, go east"), the NPC must be able
     *   to sense the PC via at least one communication sense that the two
     *   actors have in common.
     *   
     *   Likewise, in order for a non-player character to say something to
     *   the player, the player must be able to sense the NPC via at least
     *   one communication sense that the two actors have in common.  
     */
    canTalkTo(actor)
    {
        local common;
        
        /* 
         *   first, get a list of the communications senses that we have
         *   in common with the other actor - we must have a sense channel
         *   via this sense 
         */
        common = communicationSenses.intersect(actor.communicationSenses);

        /* 
         *   if there are no common senses, we can't communicate,
         *   regardless of our physical proximity 
         */
        if (common == [])
            return nil;

        /* 
         *   Determine how well the other actor can sense me in these
         *   senses.  Note that all that matters it that the actor can
         *   hear me, because we're determine if I can talk to the other
         *   actor - it doesn't matter if I can hear the other actor.  
         */
        foreach (local curSense in common)
        {
            local result;

            /* 
             *   determine how well the other actor can sense me in this
             *   sense 
             */
            result = actor.senseObj(curSense, self);

            /* check whether or not this is good enough */
            if (actor.canBeTalkedTo(self, curSense, result))
                return true;
        }

        /* 
         *   if we get this far, we didn't find any senses with a clear
         *   enough communications channel - we can't talk to the other
         *   actor 
         */
        return nil;
    }

    /*
     *   Determine whether or not I can understand an attempt by another
     *   actor to talk to me.  'talker' is the actor doing the talking.
     *   'sense' is the sense we're testing; this will always be a sense
     *   in our communicationSenses list, and will always be a
     *   communications sense we have in common with the other actor.
     *   'info' is a SenseInfo object giving information on the clarity of
     *   the sense path to the other actor.
     *   
     *   We return true if we can understand the communication, nil if
     *   not.  There is no middle ground where we can partially
     *   understand; we can either understand or not.
     *   
     *   Note that this routine is concerned only with our ability to
     *   sense the communication.  The result here should NOT pay any
     *   attention to whether or not we can actually communicate given a
     *   clear sense path - for example, this routine should not reflect
     *   whether or not we have a spoken language in common with the other
     *   actor.
     *   
     *   This is a service method for canTalkTo.  This is broken out as a
     *   separate method so that individual actors can override the
     *   necessary conditions for communications in particular senses.  
     */
    canBeTalkedTo(talker, sense, info)
    {
        /*   
         *   By default, we allow communication if the sense path is
         *   transparent or distant.  We don't care what the sense is,
         *   since we know we'll never be asked about a sense that's not
         *   in our communicationSenses list.  
         */
        return info.trans is in (transparent, distant);
    }

    /*
     *   Flag: we wait for commands issued to other actors to complete
     *   before we get another turn.  If this is true, then whenever we
     *   issue a command to another actor ("bob, go north"), we will not
     *   get another turn until the other actor has finished executing the
     *   full set of commands we issued.
     *   
     *   By default, this is true, which means that we wait for other
     *   actors to finish all of the commands we issue before we take
     *   another turn.  
     *   
     *   If this is set to nil, we'll continue to take turns while the
     *   other actor carries out our commands.  In this case, the only
     *   time cost to us of issuing a command is given by orderingTime(),
     *   which normally takes one turn for issuing a command, regardless
     *   of the command's complexity.  Some games might wish to use this
     *   mode for interesting effects with NPC's carrying out commands in
     *   parallel with the player, but it's an unconventional style that
     *   some players might find confusing, so we don't use this mode by
     *   default.  
     */
    issueCommandsSynchronously = true

    /*
     *   Flag: the "target actor" of the command line automatically reverts
     *   to this actor at the end of a sentence, when this actor is the
     *   issuer of a command.  If this flag is nil, an explicit target
     *   actor stays in effect until the next explicit target actor (or the
     *   end of the entire command line, if no other explicit target actors
     *   are named); if this flag is true, a target actor is in effect only
     *   until the end of a sentence.
     *   
     *   Consider this command line:
     *   
     *   >Bob, go north and get fuel cell. Get log tape.
     *   
     *   If this flag is nil, then the second sentence ("get log tape") is
     *   interpreted as a command to Bob, because Bob is explicitly
     *   designated as the target of the command, and this remains in
     *   effect until the end of the entire command line.
     *   
     *   If this flag is true, on the other hand, then the second sentence
     *   is interpreted as a command to the player character, because the
     *   target actor designation ("Bob,") lasts only until the end of the
     *   sentence.  Once a new sentence begins, we revert to the issuing
     *   actor (the player character, since the command came from the
     *   player via the keyboard).
     */
    revertTargetActorAtEndOfSentence = nil

    /*
     *   The amount of time, in game clock units, it takes me to issue an
     *   order to another actor.  By default, it takes one unit (which is
     *   usually equal to one turn) to issue a command to another actor.
     *   However, if we are configured to wait for our issued commands to
     *   complete in full, the ordering time is zero; we don't need any
     *   extra wait time in this case because we'll wait the full length
     *   of the issued command to begin with.  
     */
    orderingTime(targetActor)
    {
        return issueCommandsSynchronously ? 0 : 1;
    }

    /*
     *   Wait for completion of a command that we issued to another actor.
     *   The parser calls this routine after each time we issue a command
     *   to another actor.
     *   
     *   If we're configured to wait for completion of orders given to
     *   other actors before we get another turn, we'll set ourselves up
     *   in waiting mode.  Otherwise, we'll do nothing.  
     */
    waitForIssuedCommand(targetActor)
    {
        /* if we can issue commands asynchronously, there's nothing to do */
        if (!issueCommandsSynchronously)
            return;

        /* 
         *   Add an empty pending command at the end of the target actor's
         *   queue.  This command won't do anything when executed; its
         *   purpose is to let us track whether or not the target is still
         *   working on commands we have issued up to this point, which we
         *   can tell by looking to see whether our empty command is still
         *   in the actor's queue.
         *   
         *   Note that we can't simply wait until the actor's queue is
         *   empty, because the actor could acquire new commands while
         *   it's working on our pending commands, and we wouldn't want to
         *   wait for those to finish.  Adding a dummy pending command is
         *   a reliable way of tracking the actor's queue, because any
         *   changes to the target actor's command queue will leave our
         *   dummy command in its proper place until the target actor gets
         *   around to executing it, at which point it will be removed.
         *   
         *   Remember the dummy pending command in a property of self, so
         *   that we can check later to determine when the command has
         *   finished.  
         */
        waitingForActor = targetActor;
        waitingForInfo = new PendingCommandMarker(self);
        targetActor.pendingCommand.append(waitingForInfo);
    }

    /* 
     *   Synchronous command processing: the target actor and dummy
     *   pending command we're waiting for.  When these are non-nil, we
     *   won't take another turn until the given PendingCommandInfo has
     *   been removed from the given target actor's command queue. 
     */
    waitingForActor = nil
    waitingForInfo = nil

    /*
     *   Add the given actor to the list of actors accompanying my travel
     *   on the current turn.  This does NOT set an actor in "follow mode"
     *   or "accompany mode" or anything like that - don't use this to make
     *   an actor follow me around.  Instead, this makes the given actor go
     *   with us for the CURRENT travel only - the travel we're already in
     *   the process of performing to process the current TravelVia action.
     */
    addAccompanyingActor(actor)
    {
        /* if we don't have the accompanying actor vector yet, create it */
        if (accompanyingActors == nil)
            accompanyingActors = new Vector(8);

        /* add the actor to my list */
        accompanyingActors.append(actor);
    }

    /* 
     *   My vector of actors who are accompanying me. 
     *   
     *   This is for internal bookkeeping only, and it applies to the
     *   current travel only.  This is NOT a general "follow mode" setting,
     *   and it shouldn't be used to get me to follow another actor or
     *   another actor to follow me.  To make me accompany another actor,
     *   simply override accompanyTravel() so that it returns a suitable
     *   ActorState object.  
     */
    accompanyingActors = nil

    /*
     *   Get the list of objects I can follow.  This is a list of all of
     *   the objects which I have seen departing a location - these are
     *   all in scope for 'follow' commands.  
     */
    getFollowables()
    {
        /* return the list of the objects we know about */
        return followables_.mapAll({x: x.obj});
    }

    /* 
     *   Do I track departing objects for following the given object?
     *   
     *   By default, the player character tracks everyone, and NPC's track
     *   only the actor they're presently tasked to follow.  Most NPC's
     *   will never accept 'follow' commands, so there's no need to track
     *   everyone all the time; for efficiency, we take advantage of this
     *   assumption so that we can avoid storing a bunch of tracking
     *   information that will never be used.
     */
    wantsFollowInfo(obj)
    {
        /* 
         *   by default, the player character tracks everyone, and NPC's
         *   track only the object (if any) they're currently tasked to
         *   follow 
         */
        return isPlayerChar() || followingActor == obj;
    }

    /*
     *   Receive notification that an object is leaving its current
     *   location as a result of the action we're currently processing.
     *   Actors (and possibly other objects) will broadcast this
     *   notification to all Actor objects connected in any way by
     *   containment when they move under their own power (such as with
     *   Actor.travelTo) to a new location.  We'll keep tracking
     *   information if we are configured to keep tracking information for
     *   the given object and we can see the given object.  Note that this
     *   is called when the object is still at the source end of the travel
     *   - the important thing is that we see the object departing.
     *   
     *   'obj' is the object that is seen to be leaving, and 'conn' is the
     *   TravelConnector it is taking.
     *   
     *   'conn' is the connector being traversed.  If we're simply being
     *   observed in this location (as in a call to setHasSeen), rather
     *   than being observed to leave the location, the connector will be
     *   nil.
     *   
     *   'from' is the effective starting location of the travel.  This
     *   isn't necessarily the departing object's location, since the
     *   departing object could be inside a vehicle or some other kind of
     *   traveler object.
     *   
     *   Note that this notification is sent only to actors with some sort
     *   of containment connection to the object that's moving, because a
     *   containment connection is necessary for there to be a sense
     *   connection.  
     */
    trackFollowInfo(obj, conn, from)
    {
        local info;
        
        /* 
         *   If we're not tracking the given object, or we can't see the
         *   given object, ignore the notification.  In addition, we
         *   obviously have no need to track ourselves.x  
         */
        if (obj == self || !wantsFollowInfo(obj) || !canSee(obj))
            return;

        /* 
         *   If we already have a FollowInfo for the given object, re-use
         *   the existing one; otherwise, create a new one and add it to
         *   our tracking list. 
         */
        info = followables_.valWhich({x: x.obj == obj});
        if (info == nil)
        {
            /* we don't have an existing one - create a new one */
            info = new FollowInfo();
            info.obj = obj;

            /* add it to our list */
            followables_ += info;
        }

        /* remember information about the travel */
        info.connector = conn;
        info.sourceLocation = from;
    }

    /*
     *   Get information on what to do to make this actor follow the given
     *   object.  This returns a FollowInfo object that reports our last
     *   knowledge of the given object's location and departure, or nil if
     *   we don't know anything about how to follow the actor.  
     */
    getFollowInfo(obj)
    {
        return followables_.valWhich({x: x.obj == obj});
    }

    /*
     *   By default, all actors are followable.
     */
    verifyFollowable()
    {
        return true;
    }

    /*
     *   Verify a "follow" command being performed by this actor.  
     */
    actorVerifyFollow(obj)
    {
        /* 
         *   check to see if we're in the same effective follow location
         *   as the target; if we are, it makes no sense to follow the
         *   target, since we're already effectively at the same place 
         */
        if (obj.location != nil
            && (location.effectiveFollowLocation
                == obj.location.effectiveFollowLocation))
        {
            /*
             *   We're in the same location as the target.  If we're the
             *   player character, this makes no sense, because the player
             *   character can't go into follow mode (as that would take
             *   away the player's ability to control the player
             *   character).  If we're an NPC, though, this simply tells
             *   us to go into follow mode for the target, so there's
             *   nothing wrong with it.  
             */
            if (isPlayerChar)
            {
                /* 
                 *   The target is right here, but we're the player
                 *   character, so it makes no sense for us to go into
                 *   follow mode.  If we can see the target, complain that
                 *   it's already here; if not, we can only assume it's
                 *   here, but we can't know for sure.  
                 */
                if (canSee(obj))
                    illogicalNow(&followAlreadyHereMsg);
                else
                    illogicalNow(&followAlreadyHereInDarkMsg);
            }
        }
        else if (!canSee(obj))
        {
            /* 
             *   The target isn't here, and we can't see it from here, so
             *   we must want to follow it to its current location.  Get
             *   information on how we will follow the target.  If there's
             *   no such information, we obviously can't do any following
             *   because we never saw the target go anywhere in the first
             *   place.  
             */
            if (getFollowInfo(obj) == nil)
            {
                /* we've never heard of the target */
                illogicalNow(&followUnknownMsg);
            }
        }
    }

    /*
     *   Carry out a "follow" command being performed by this actor.  
     */
    actorActionFollow(obj)
    {
        local canSeeObj;

        /* note whether or not we can see the target */
        canSeeObj = canSee(obj);
        
        /* 
         *   If we're not the PC, check to see if this is a follow-mode
         *   request; otherwise, try to go to the location of the target.
         */
        if (!isPlayerChar && canSeeObj)
        {
            /*
             *   If we're not already following this actor, acknowledge the
             *   request and go into 'follow' mode.  If we're already
             *   following this actor, and we didn't issue the command to
             *   ourself, let them know we're already in the requested
             *   mode.  Otherwise, ignore it silently - if we issued the
             *   command to ourself, it's because we're just executing our
             *   own 'follow' mode imperative. 
             */
            if (followingActor != obj)
            {
                /* let them know we're going to follow the actor now */
                reportAfter(&okayFollowModeMsg);

                /* go into follow mode */
                followingActor = obj;
            }
            else if (gIssuingActor != self)
            {
                /* let them know we're already in follow mode */
                reportAfter(&alreadyFollowModeMsg);
            }

            /* 
             *   if we're already in the target's effective follow
             *   location, that's all we need to do 
             */
            if (location.effectiveFollowLocation
                == obj.location.effectiveFollowLocation)
                return;
        }

        /* 
         *   If we can see the target, AND we're in the same top-level
         *   location as the target, then simply use a "local travel"
         *   operation to move into the same location.  This only works
         *   with targets that are within the same top-level location,
         *   since that's the whole point of the local-travel routines.
         *   For non-local travel, we need to perform a full-fledged travel
         *   command instead.  
         */
        if (canSeeObj && isIn(obj.getOutermostRoom()))
        {
            /* 
             *   We have no information, so we will only have made it past
             *   verification if we can see the other actor from our
             *   current location.  Try moving to the other actor's
             *   effective follow location.  
             */
            obj.location.effectiveFollowLocation.checkMovingActorInto(true);

            /*
             *   Since checkMovingActorInto will do its work through
             *   implicit actions, if we're the player character, then the
             *   entire action will have been performed implicitly, so we
             *   won't have a real report for the series of generated
             *   actions, just implied action announcements.  If we're an
             *   NPC, on the other hand, we'll generate the full reports,
             *   since NPC implied actions simply show what the actor is
             *   doing.  So, if we're the PC, generate an additional
             *   default acknowledgment of the 'follow' action. 
             */
            if (isPlayerChar)
                defaultReport(&okayFollowInSightMsg,
                              location.effectiveFollowLocation);
        }
        else
        {
            local info;
            local srcLoc;
            
            /* get the information on how to follow the target */
            info = getFollowInfo(obj);

            /* get the effective follow location we have to be in */
            srcLoc = info.sourceLocation.effectiveFollowLocation;

            /* if there's no connector, we can't go anywhere */
            if (info.connector == nil)
            {
                /* 
                 *   We have no departure information, so we can't follow
                 *   the actor.  If we're currently within sight of the
                 *   location where we last saw the actor, it means that we
                 *   saw the actor here, then went somewhere else, then
                 *   came back, and in our absence the actor itself
                 *   departed.  In this case, report that we don't know
                 *   where the actor went.
                 *   
                 *   If we're not in sight of the location where we last
                 *   saw the actor, then instead remind the player of where
                 *   that was.  
                 */
                if (canSee(srcLoc))
                {
                    /* 
                     *   we're where we last saw the actor, but the actor
                     *   must have departed while we were away - so we
                     *   simply don't know where the actor went 
                     */
                    reportFailure(&followUnknownMsg);
                }
                else
                {
                    /* 
                     *   we've gone somewhere else since we last saw the
                     *   actor, so remind the player of where it was that
                     *   we saw the actor 
                     */
                    reportFailure(&cannotFollowFromHereMsg, srcLoc);
                }

                /* in any case, that's all we can do now */
                return;
            }

            /*
             *   Before we can follow the target, we must be in the same
             *   effective location that the target was in when we
             *   observed the target leaving.
             */
            if (location.effectiveFollowLocation != srcLoc)
            {
                /* 
                 *   If we can't even see the effective follow location, we
                 *   must have last observed the other actor traveling from
                 *   an unrelated location.  In this case, simply say that
                 *   we don't know where the followee went. 
                 */
                if (!canSee(srcLoc))
                {
                    reportFailure(&cannotFollowFromHereMsg, srcLoc);
                    return;
                }

                /* 
                 *   Try moving into the same location, by invoking the
                 *   pre-condition handler for moving me into the
                 *   effective follow location from our memory of the
                 *   actor's travel.  We *could* run this as an actual
                 *   precondition, but it's easier to run it here now that
                 *   we've sorted out exactly what we want to do.  
                 */
                srcLoc.checkMovingActorInto(true);
            }
        
            /* perform a TravelVia action on the connector */
            nestedAction(TravelVia, info.connector);
        }
    }

    /*
     *   Our list of followable information.  Each entry in this list is a
     *   FollowInfo object that tracks a particular followable.  
     */
    followables_ = []

    /* determine if I've ever seen the given object */
    hasSeen(obj) { return obj.(seenProp); }

    /* mark the object to remember that I've seen it */
    setHasSeen(obj) { obj.noteSeenBy(self, seenProp); }

    /* receive notification that another actor is observing us */
    noteSeenBy(actor, prop)
    {
        /* do the standard work to remember that we've been seen */
        inherited(actor, prop);

        /* 
         *   Update the follow tracking information with the latest
         *   observed location.  We're merely observing the fact that the
         *   actor is here, not that the actor is departing, so the
         *   connector is nil.
         *   
         *   The point of noting the actor's presence in the "follow info"
         *   is that we want to replace any previous memory we have of the
         *   actor departing from another location.  Now that we know where
         *   the actor is, any old memory of the actor having left another
         *   location is now irrelevant.  We only keep track of one "follow
         *   info" record per actor, so this new record will replace any
         *   older record.  
         */
        actor.trackFollowInfo(self, nil, location);
    }

    /* 
     *   Determine if I know about the given object.  I know about an
     *   object if it's specifically marked as known to me; I also know
     *   about the object if I can see it now, or if I've ever seen it in
     *   the past.  
     */
    knowsAbout(obj) { return canSee(obj) || hasSeen(obj) || obj.(knownProp); }

    /* mark the object as known to me */
    setKnowsAbout(obj) { obj.(knownProp) = true; }

    /*
     *   My 'seen' property.  By default, this is simply 'seen', which
     *   means that we don't distinguish who's seen what - in other words,
     *   there's a single, global 'seen' flag per object, and if anyone's
     *   ever seen something, then we consider that to mean everyone has
     *   seen it.
     *   
     *   Some games might want to track each NPC's sight memory
     *   separately, or at least they might want to track it individually
     *   for a few specific NPC's.  You can do this by making up a new
     *   property name for each NPC whose sight memory you want to keep
     *   separate, and simply setting 'seenProp' to that property name for
     *   each such NPC.  For example, for Bob, you could make the property
     *   bobHasSeen, so in Bob you'd define 'sightProp = &bobHasSeen'.  
     */
    seenProp = &seen

    /*
     *   My 'known' property.  By default, this is simply 'known', which
     *   means that we don't distinguish who knows what.
     *   
     *   As with 'seenProp' above, if you want to keep track of each NPC's
     *   knowledge separately, you must override this property for each
     *   NPC who's to have its own knowledge base to use a separate
     *   property name.  For example, if you want to keep track of what
     *   Bob knows individually, you could define 'knownProp = &bobKnows'
     *   in Bob.  
     */
    knownProp = &isKnown

    /*
     *   Determine if the actor recognizes the given object as a "topic,"
     *   which is an object that represents some knowledge the actor can
     *   use in conversations, consultations, and the like.
     *   
     *   By default, we'll recognize any Topic object marked as known, and
     *   we'll recognize any game object for which our knowsAbout(obj)
     *   returns true.  Games might wish to override this in some cases to
     *   limit or expand an actor's knowledge according to what the actor
     *   has experienced of the setting or story.  Note that it's often
     *   easier to control actor knowledge using the lower-level
     *   knowsAbout() and setKnowsAbout() methods, though.  
     */
    knowsTopic(obj)
    {
        /* we know the object as a topic if we know about it at all */
        return knowsAbout(obj);
    }

    /*
     *   Determine if the given object is a likely topic for a
     *   conversational action performed by this actor.  By default, we'll
     *   return true if the topic is known, nil if not.  
     */
    isLikelyTopic(obj)
    {
        /* if the object is known, it's a possible topic */
        return knowsTopic(obj);
    }

    /* we are the owner of any TopicEntry objects contained within us */
    getTopicOwner() { return self; }

    /*
     *   Suggest topics of conversation.  This is called by the TOPICS
     *   command (in which case 'explicit' is true), and whenever we first
     *   engage a character in a stateful conversation (in which case
     *   'explicit' is nil).
     *   
     *   We'll show the list of suggested topics associated with our
     *   current conversational partner.  If there are no topics, we'll say
     *   nothing unless 'explicit' is true, in which case we'll simply say
     *   that there are no topics that the player character is thinking
     *   about.
     *   
     *   The purpose of this method is to let the game author keep an
     *   "inventory" of topics with this actor for a given conversational
     *   partner.  This inventory is meant to represent the topics that on
     *   the player character's mind - things the player character wants to
     *   talk about with the other actor.  Note that we're talking about
     *   what the player *character* is thinking about - obviously we don't
     *   know what's on the player's mind.
     *   
     *   When we enter conversation, or when the player asks for advice,
     *   we'll show this inventory.  The idea is to help guide the player
     *   through a conversation without the more heavy-handed device of a
     *   formal conversation menu system, so that conversations have a more
     *   free-form feel without leaving the player hunting in the dark for
     *   the magic ASK ABOUT topic.
     *   
     *   The TOPICS system is entirely optional.  If a game doesn't specify
     *   any SuggestedTopic objects, then this routine will simply never be
     *   called, and the TOPICS command won't be allowed.  Some authors
     *   think it gives away too much to provide a list of topic
     *   suggestions like this, and others don't like anything that smacks
     *   of a menu system because they think it destroys the illusion
     *   created by the text-input command line that the game is boundless.
     *   Authors who feel this way can just ignore the TOPICS system.  But
     *   be aware that the illusion of boundlessness isn't always a good
     *   thing for players; hunting around for ASK ABOUT topics can make
     *   the game's limits just as obvious, if not more so, by exposing the
     *   vast number of inputs for which the actor doesn't have a good
     *   response.  Players aren't stupid - a string of variations on "I
     *   don't know about that" is just as obviously mechanistic as a
     *   numbered list of menu choices.  Using the TOPICS system might be a
     *   good compromise for many authors, since the topic list can help
     *   guide the player to the right questions without making the player
     *   feel straitjacketed by a menu list.  
     */
    suggestTopics(explicit)
    {
        local actor;
        
        /* 
         *   if we're talking to someone, look up their suggested topics;
         *   otherwise, we have nothing to suggest 
         */
        if ((actor = getCurrentInterlocutor()) != nil)
        {
            /* 
             *   we're talking to someone - suggest topics appropriate to
             *   the person we're talking to 
             */
            actor.suggestTopicsFor(self, explicit);
        }
        else if (explicit)
        {
            /* we're not talking to anyone, so there's nothing to suggest */
            gLibMessages.noTopicsNotTalking;
        }
    }

    /*
     *   Suggest topics that the given actor might want to talk to us
     *   about.  The given actor is almost always the player character,
     *   since generally NPC's don't talk to one another using
     *   conversation commands (there'd be no point; they're simple
     *   programmed automata, not full-blown AI's).  
     */
    suggestTopicsFor(actor, explicit)
    {
        /* by default, let our state suggest topics */
        curState.suggestTopicsFor(actor, explicit);
    }

    /*
     *   Receive notification that a command is being carried out in our
     *   presence. 
     */
    beforeAction()
    {
        /*
         *   If another actor is trying to take something in my inventory,
         *   by default, do not allow it. 
         */
        if (gActor != self
            && (gActionIs(Take) || gActionIs(TakeFrom))
            && gDobj.isIn(self))
        {
            /* check to see if we want to allow this action */
            checkTakeFromInventory(gActor, gDobj);
        }

        /* let our state object take a look at the action */
        curState.beforeAction();
    }

    /* 
     *   Perform any actor-specific processing for an action.  The main
     *   command processor invokes this on gActor after notifying nearby
     *   objects via beforeAction(), but before carrying out the main
     *   action of the command.  
     */
    actorAction()
    {
        /* do nothing by default */
    }

    /*
     *   Receive notification that a command has just been carried out in
     *   our presence.  
     */
    afterAction()
    {
        /* let the state object handle it */
        curState.afterAction();
    }

    /* receive a notification that someone is about to travel */
    beforeTravel(traveler, connector)
    {
        /* let the state object handle it */
        curState.beforeTravel(traveler, connector);

        /* 
         *   If desired, track the departure so that we can follow the
         *   traveler later.  First, track the departure of each actor
         *   traveling with the traveler.  
         */
        traveler.forEachTravelingActor(
            {actor: trackFollowInfo(actor, connector, traveler.location)});

        /* 
         *   if the traveler is distinct from the actors traveling, track
         *   it as well 
         */
        if (!traveler.isActorTraveling(traveler))
            trackFollowInfo(traveler, connector, traveler.location);
    }

    /* receive a notification that someone has just traveled here */
    afterTravel(traveler, connector)
    {
        /* let the state object handle it */
        curState.afterTravel(traveler, connector);
    }

    /*
     *   Receive notification that I'm initiating travel.  This is called
     *   on the actor performing the travel action before the travel is
     *   actually carried out.  
     */
    actorTravel(traveler, connector)
    {
        /*
         *   If other actors are accompanying me on this travel, run the
         *   same travel action on the accompanying actors, using nested
         *   actions.  
         */
        if (accompanyingActors != nil
            && accompanyingActors.length() != 0)
        {
            /* 
             *   Run the same travel action as a nested action on each
             *   accompanying actor.  Skip this for any accompanying actor
             *   we're carrying, as they'll naturally go with us as a
             *   result of being carried.  
             */
            foreach (local cur in accompanyingActors)
            {
                /* if the actor's not being carried, run the same action */
                if (!cur.isIn(self))
                    nestedActorAction(cur, TravelVia, gDobj);
            }

            /* 
             *   The accompanying actor list applies for this single group
             *   travel command, so now that we've moved everyone, we have
             *   no further need for the list.  Clear it out. 
             */
            accompanyingActors.removeRange(1, accompanyingActors.length());
        }
    }

    /*
     *   Check to see if we want to allow another actor to take something
     *   from my inventory.  By default, we won't allow it - we'll always
     *   fail the command.  
     */
    checkTakeFromInventory(actor, obj)
    {
        /* don't allow it - show an error and terminate the command */
        mainReport(&willNotLetGoMsg, self, obj);
        exit;
    }

    /*
     *   Build a list of the objects that are explicitly registered to
     *   receive notification when I'm the actor in a command.
     */
    getActorNotifyList()
    {
        return actorNotifyList;
    }

    /*
     *   Add an item to our registered notification items.  These items
     *   are to receive notifications when we're the actor performing a
     *   command.
     *   
     *   Items can be added here if they must be notified of actions
     *   performed by the actor even when the items aren't connected by
     *   containment with the actor at the time of the action.  All items
     *   connected to the actor by containment are automatically notified
     *   of each action; only items that must receive notification even
     *   when not in scope need to be registered here.  
     */
    addActorNotifyItem(obj)
    {
        actorNotifyList += obj;
    }

    /* remove an item from the registered notification list */
    removeActorNotifyItem(obj)
    {
        actorNotifyList -= obj;
    }

    /* our list of registered actor notification items */
    actorNotifyList = []

    /*
     *   Get the ambient light level in the visual senses at this actor.
     *   This is the ambient level at the actor.  
     */
    getVisualAmbient()
    {
        local ret;
        local cache;

        /* check for a cached value */
        if ((cache = libGlobal.actorVisualAmbientCache) != nil
            && (ret = cache[self]) != nil)
        {
            /* found a cached entry - use it */
            return ret;
        }

        /* get the maximum ambient level at self for my sight-like senses */
        ret = senseAmbientMax(sightlikeSenses);

        /* if caching is active, cache our result for next time */
        if (cache != nil)
            cache[self] = ret;

        /* return the result */
        return ret;
    }

    /*
     *   Determine if my location is lit for my sight-like senses.
     */
    isLocationLit()
    {
        /* 
         *   Check for a simple, common case before doing the full
         *   sense-path calculation: if our location is providing its own
         *   light to its interior, then the location is lit.  Most simple
         *   rooms are always lit.  
         */
        if (sightlikeSenses.indexOf(sight) != nil
            && location != nil
            && location.brightness > 1
            && location.transSensingOut(sight) == transparent)
            return true;

        /* 
         *   We don't have the simple case of light directly from our
         *   location, so run the full sense path check and get our
         *   maximum visual ambience level.  If it's above the "self-lit"
         *   level of 1, then we can see. 
         */
        return (getVisualAmbient() > 1);
    }

    /*
     *   Get the best (most transparent) sense information for one of our
     *   visual senses to the given object.  
     */
    bestVisualInfo(obj)
    {
        local best;

        /* we don't have a best value yet */
        best = nil;

        /* check each sight-like sense */
        foreach (local sense in sightlikeSenses)
        {
            /* 
             *   get the information for the object in this sense, and keep
             *   the best (most transparent) info we've seen so far 
             */
            best = SenseInfo.selectMoreTrans(best, senseObj(sense, obj));
        }

        /* return the best one we found */
        return best;
    }

    /*
     *   Build a list of all of the objects of which an actor is aware.
     *   
     *   An actor is aware of an object if the object is within reach of
     *   the actor's senses, and has some sort of presence in that sense.
     *   Note that both of these conditions must be true for at least one
     *   sense possessed by the actor; an object that is within earshot,
     *   but not within reach of any other sense, is in scope only if the
     *   object is making some kind of noise.
     *   
     *   In addition, objects that the actor is holding (i.e., those
     *   contained by the actor directly) are always in scope, regardless
     *   of their reachability through any sense.  
     */
    scopeList()
    {
        local lst;

        /* we have nothing in our master list yet */
        lst = new Vector(32);

        /* oneself is always in one's own scope list */
        lst.append(self);

        /* iterate over each sense */
        foreach (local sense in scopeSenses)
        {
            /* 
             *   get the list of objects with a presence in this sense
             *   that can be sensed from our point of view, and and append
             *   it to our master list 
             */
            lst.appendUnique(sensePresenceList(sense));
        }

        /* add all of the items we are directly holding */
        lst.appendUnique(contents);

        /* 
         *   ask each of our direct contents to add any contents of their
         *   own that are in scope by virtue of their containers being in
         *   scope 
         */
        foreach (local cur in contents)
            cur.appendHeldContents(lst);

        /* add any items that are specially in scope in the location */
        if (location != nil)
        {
            /* get the extra scope items */
            local extra = location.getExtraScopeItems(self);

            /* if this is a non-nil list, add it to our list */
            if (extra.length() != 0)
                lst.appendUnique(extra);
        }

        /* 
         *   Finally, add anything extra each item already in scope wants
         *   to add.  Note that we keep going until we've visited each
         *   element of the vector at its current length on each iteration,
         *   so if we add any new items, we'll check them to see if they
         *   want to add any new items, and so on.  
         */
        for (local i = 1 ; i <= lst.length() ; ++i)
        {
            local extra;

            /* get the extra scope items for this item */
            extra = lst[i].getExtraScopeItems(self);

            /* if the extra item list is non-nil, add it to our list */
            if (extra.length() != 0)
                lst.appendUnique(extra);
        }

        /* return the result */
        return lst.toList();
    }

    /*
     *   Determine if I can see the given object.  This returns true if
     *   the object can be sensed at all in one of my sight-like senses,
     *   nil if not.  
     */
    canSee(obj)
    {
        /* try each sight-like sense */
        foreach (local sense in sightlikeSenses)
        {
            /* 
             *   if I can sense the object in this sense, I can sense the
             *   object 
             */
            if (senseObj(sense, obj).trans != opaque)
                return true;
        }

        /* we didn't find any sight-like sense where we can see the object */
        return nil;
    }

    /*
     *   Determine if I can hear the given object. 
     */
    canHear(obj)
    {
        /* try each hearling-like sense */
        foreach (local sense in hearinglikeSenses)
        {
            /* 
             *   if I can sense the object in this sense, I can sense the
             *   object 
             */
            if (senseObj(sense, obj).trans != opaque)
                return true;
        }
        
        /* we found no hearing-like sense that lets us hear the object */
        return nil;
    }

    /*
     *   Determine if I can smell the given object. 
     */
    canSmell(obj)
    {
        /* try each hearling-like sense */
        foreach (local sense in smelllikeSenses)
        {
            /* 
             *   if I can sense the object in this sense, I can sense the
             *   object 
             */
            if (senseObj(sense, obj).trans != opaque)
                return true;
        }
        
        /* we found no smell-like sense that lets us hear the object */
        return nil;
    }

    /*
     *   Find the object that prevents us from seeing the given object. 
     */
    findVisualObstructor(obj)
    {
        /* try to find an opaque obstructor in one of our visual senses */
        foreach (local sense in sightlikeSenses)
        {
            local obs;

            /* cache path information for this sense */
            cacheSenseInfo(connectionTable(), sense);
            
            /* if we find an obstructor in this sense, return it */
            if ((obs = findOpaqueObstructor(sense, obj)) != nil)
                return obs;
        }

        /* we didn't find any obstructor */
        return nil;
    }

    /*
     *   Build a table of full sensory information for all of the objects
     *   visible to the actor through the actor's sight-like senses.
     *   Returns a lookup table with the same set of information as
     *   senseInfoTable().  
     */
    visibleInfoTable()
    {
        /* return objects visible from my own point of view */
        return visibleInfoTableFromPov(self);
    }

    /*
     *   Build a table of full sensory information for all of the objects
     *   visible to me from a particular point of view through my
     *   sight-like senses.  
     */
    visibleInfoTableFromPov(pov)
    {
        local tab;

        /* we have no master table yet */
        tab = nil;

        /* iterate over each sense */
        foreach (local sense in sightlikeSenses)
        {
            local cur;
            
            /* get information for all objects for the current sense */
            cur = pov.senseInfoTable(sense);

            /* merge the table so far with the new table */
            tab = mergeSenseInfoTable(cur, tab);
        }

        /* return the result */
        return tab;
    }

    /*
     *   Build a lookup table of the objects that can be sensed for the
     *   purposes of taking inventory.  We'll include everything in the
     *   normal visual sense table, plus everything directly held.  
     */
    inventorySenseInfoTable()
    {
        local visInfo;
        local cont;
        local ambient;
        local info;
        
        /*   
         *   Start with the objects visible to the actor through the
         *   actor's sight-like senses.  
         */
        visInfo = visibleInfoTable();

        /* get the ambient light level at the actor */
        if ((info = visInfo[self]) != nil)
            ambient = info.ambient;
        else
            ambient = 0;
        
        /*   
         *   We'll assume that, for each item that the actor is directly
         *   holding AND knows about, the actor can still identify the item
         *   by touch, even if it's not visible.  This way, when we're in a
         *   dark room, we'll still be able to refer to the objects we're
         *   directly holding, as long as we already know about them.
         *   
         *   Likewise, add items within our direct contents that are
         *   considered equally held.  
         */
        cont = new Vector(32);
        foreach (local cur in contents)
        {
            /* add this item from our contents */
            cont.append(cur);

            /* add its contents that are themselves equally as held */
            cur.appendHeldContents(cont);
        }

        /* 
         *   Make a fully-sensible entry for each of our held items.  We
         *   can simply replace any existing entry in the table that we got
         *   from the visual senses, since a fully transparent entry will
         *   be at least as good as anything we got from the normal visual
         *   list.  Only include items that the actor knows about; we'll
         *   assume that we can identify by touch anything we're holding if
         *   we already know what it is, but not otherwise.  
         */
        foreach (local cur in cont)
        {
            /* if we know about the object, make it effectively visible */
            if (knowsAbout(cur))
                visInfo[cur] = new SenseInfo(cur, transparent, nil, ambient);
        }

        /* return the table */
        return visInfo;
    }

    /*
     *   Show what the actor is carrying.
     */
    showInventory(tall)
    {
        /* 
         *   show our inventory with our default listers as given by our
         *   inventory/wearing lister properties 
         */
        showInventoryWith(tall, inventoryLister);
    }

    /*
     *   Show what the actor is carrying, using the given listers.
     *   
     *   Note that this method must be overridden if the actor does not
     *   use a conventional 'contents' list property to store its full set
     *   of contents.  
     */
    showInventoryWith(tall, inventoryLister)
    {
        local infoTab;

        /* get the table of objects sensible for inventory */
        infoTab = inventorySenseInfoTable();

        /* list in the appropriate mode ("wide" or "tall") */
        inventoryLister.showList(self, self, contents,
                                 ListRecurse | (tall ? ListTall : 0),
                                 0, infoTab, nil);

        /* mention sounds coming from inventory items */
        inventorySense(sound, inventoryListenLister);

        /* mention odors coming from inventory items */
        inventorySense(smell, inventorySmellLister);
    }

    /*
     *   Add to an inventory description a list of things we notice
     *   through a specific sense.
     */
    inventorySense(sense, lister)
    {
        local infoTab;
        local presenceList;
        
        /* get the information table for the desired sense */
        infoTab = senseInfoTable(sense);
        
        /* 
         *   get the list of everything with a presence in this sense that
         *   I'm carrying 
         */
        presenceList = senseInfoTableSubset(infoTab,
            {obj, info: obj.isIn(self) && obj.(sense.presenceProp)});

        /* add a paragraph break */
        cosmeticSpacingReport('<.p>');
        
        /* list the items */
        lister.showList(self, nil, presenceList, 0, 0, infoTab, nil);
    }

    /*
     *   The Lister object that we use for inventory listings.  By
     *   default, we use actorInventoryLister, but this can be overridden
     *   if desired to use a different listing style.  
     */
    inventoryLister = actorInventoryLister

    /*
     *   The Lister for inventory listings, for use in a full description
     *   of the actor.  By default, we use the "long form" inventory
     *   lister, on the assumption that most actors have relatively lengthy
     *   descriptive text.  This can be overridden to use other formats;
     *   the short-form lister, for example, is useful for actors with only
     *   brief descriptions.  
     */
    holdingDescInventoryLister = actorHoldingDescInventoryListerLong

    /*
     *   Perform library pre-initialization on the actor 
     */
    initializeActor()
    {
        /* set up an empty pending command list */
        pendingCommand = new Vector(5);

        /* create a default inventory lister if we don't have one already */
        if (inventoryLister == nil)
            inventoryLister = actorInventoryLister;

        /* create our antecedent tables */
        antecedentTable = new LookupTable(8, 8);
        possAnaphorTable = new LookupTable(8, 8);

        /* if we don't have a state object, create a default */
        if (curState == nil)
            setCurState(new ActorState(self));

        /* create our pending-conversation list */
        pendingConv = new Vector(5);
    }

    /*
     *   Note conditions before an action or other event.  By default, we
     *   note our location and light/dark status, so that we comment on
     *   any change in the light/dark status after the event if we're
     *   still in the same location.  
     */
    noteConditionsBefore()
    {
        /* note our original location and light/dark status */
        locationBefore = location;
        locationLitBefore = isLocationLit();
    }

    /*
     *   Note conditions after an action or other event.  By default, if
     *   we are still in the same location we were in when
     *   noteConditionsBefore() was last called, and the light/dark status
     *   has changed, we'll mention the change in light/dark status. 
     */
    noteConditionsAfter()
    {
        /* 
         *   If our location hasn't changed but our light/dark status has,
         *   note the new status.  We don't make any announcement if the
         *   location has changed, since the travel routine will
         *   presumably have shown us the new location's light/dark status
         *   implicitly as part of the description of the new location
         *   after travel. 
         */
        if (location == locationBefore
            && isLocationLit() != locationLitBefore)
        {
            /* consider this the start of a new turn */
            "<.commandsep>";

            /* note the change with a new 'NoteDarkness' action */
            newActorAction(self, NoteDarkness);

            /* 
             *   start another turn, in case this occurred during an
             *   implicit action or the like 
             */
            "<.commandsep>";
        }
    }

    /* conditions we noted in noteConditionsBefore() */
    locationBefore = nil
    locationLitBefore = nil

    /* let the actor have a turn as soon as the game starts */
    nextRunTime = 0

    /* 
     *   Scheduling order - this determines the order of execution when
     *   several items are schedulable at the same game clock time.
     *   
     *   We choose a scheduling order that schedules actors in this
     *   relative order:
     *   
     *   100 player character, ready to execute
     *.  200 NPC, ready to execute
     *.  300 player character, idle
     *.  400 NPC, idle
     *   
     *   An "idle" actor is one that is waiting for another character to
     *   complete a command, or an NPC with no pending commands to
     *   perform.  (For the player character, it doesn't matter whether or
     *   not there's a pending command, because if the PC has no pending
     *   command, we ask the player for one.)
     *   
     *   This ordering ensures that each actor gets a chance to run each
     *   turn, but that actors with work to do go first, and other things
     *   being equal, the player character goes ahead of NPC's.  
     */
    scheduleOrder = 100

    /* calculate the scheduling order */
    calcScheduleOrder()
    {
        /* determine if we're ready to run */
        if (readyForTurn())
            scheduleOrder = isPlayerChar() ? 100 : 200;
        else
            scheduleOrder = isPlayerChar() ? 300 : 400;

        /* return the scheduling order */
        return scheduleOrder;
    }

    /*
     *   Determine if we're ready to do something on our turn.  We're
     *   ready to do something if we're not waiting for another actor to
     *   finish doing something and either we're the player character or
     *   we already have a pending command in our command queue.  
     */
    readyForTurn()
    {
        /* 
         *   if we're waiting for another actor, we're not ready to do
         *   anything 
         */
        if (checkWaitingForActor())
            return nil;

        /* 
         *   if we're the player character, we're always ready to take a
         *   turn as long as we're not waiting for another actor (which we
         *   now know we're not), because we can either execute one of our
         *   previously queued commands, or we can ask for a new command
         *   to perform 
         */
        if (isPlayerChar())
            return true;

        /* 
         *   if we have something other than placeholders in our command
         *   queue, we're ready to take a turn, because we can execute the
         *   next command in our queue 
         */
        if (pendingCommand.indexWhich({x: x.hasCommand}) != nil)
            return true;

        /* 
         *   we have no specific work to do, so we're not ready for our
         *   next turn 
         */
        return nil;
    }

    /*
     *   Check to see if we're waiting for another actor to do something.
     *   Return true if so, nil if not.  If we've been waiting for another
     *   actor, and the actor has finished the task we've been waiting for
     *   since the last time we checked, we'll clean up our internal state
     *   relating to the wait and return nil.  
     */
    checkWaitingForActor()
    {
        local cmdIdx;
        local idx;

        /* if we're not waiting for an actor, simply return nil */
        if (waitingForActor == nil)
            return nil;

        /* 
         *   We're waiting for an actor to complete a command.  Check to
         *   see if the completion marker is still in the actor's queue; if
         *   it's not, then the other actor has already completed our task.
         *   If the completion marker is in the other actor's queue, but
         *   there are no command entries before it, then we're also done
         *   waiting, because we're not actually waiting for the completion
         *   marker but instead for the tasks that were ahead of it in the
         *   main game execution loop.
         *   
         *   So, find the index of our marker in the queue, and find the
         *   index of the first real command in the queue.  If our marker
         *   is still in the queue, and there's a command in the queue
         *   before our marker, the actor we're waiting for still has
         *   things to do before we're ready, so we're still waiting.  
         */
        idx = waitingForActor.pendingCommand.indexOf(waitingForInfo);
        cmdIdx = waitingForActor.pendingCommand.indexWhich({x: x.hasCommand});
        if (idx != nil && cmdIdx != nil && idx > cmdIdx)
        {
            /* 
             *   The marker is still in the queue, and there's at least
             *   one other command ahead of it, so the other actor hasn't
             *   finished the task we've been waiting for.  Tell the
             *   caller that we are indeed still waiting for someone.  
             */
            return true;
        }

        /*
         *   The other actor has disposed of our end-marker (or is about
         *   to, because it's the next thing left in the actor's queue),
         *   so it has finished with all of the commands we have been
         *   waiting for.  However, if I haven't caught up in game clock
         *   time with the actor I've been waiting for, I'm still waiting.
         */
        if (waitingForActor.nextRunTime > nextRunTime)
            return true;

        /* we're done waiting - forget our wait status information */
        waitingForActor = nil;
        waitingForInfo = nil;

        /* tell the caller we're no longer waiting for anyone */
        return nil;
    }

    /* the action the actor performed most recently */
    mostRecentAction = nil

    /*
     *   Add busy time.  An action calls this when we are the actor
     *   performing the action, and the action consumes game time.  This
     *   marks us as busy for the given time units.  
     */
    addBusyTime(action, units)
    {
        /* note the action being performed */
        mostRecentAction = action;

        /* adjust the next run time by the busy time */
        nextRunTime += units;
    }

    /*
     *   When it's our turn and we don't have any command to perform,
     *   we'll call this routine, which can perform a scripted operation
     *   if desired.  
     */
    idleTurn()
    {
        local tCur = Schedulable.gameClockTime;
        local origNextRunTime = nextRunTime;
        
        /* 
         *   if we haven't been targeted for conversation on this turn,
         *   see if we have a conversation we want to start 
         */
        if (lastConvTime < tCur)
        {
            /* check for a conversation that's ready to go */
            local info = pendingConv.valWhich({x: tCur >= x.time_});

            /* if we found one, kick it off */
            if (info != nil)
            {
                /* remove it from the list */
                pendingConv.removeElement(info);

                /* start the conversation */
                initiateConversation(info.state_, info.node_);
            }
        }

        /* notify our state object that we're taking a turn */
        curState.takeTurn();

        /* 
         *   If we haven't already adjusted our next run time, consume a
         *   turn, so we're not ready to run again until the next game time
         *   increment.  In some cases, we'll already have made this
         *   adjustment; for example, we might have run a nested command
         *   within our state object's takeTurn() method.  
         */
        if (nextRunTime == origNextRunTime)
            ++nextRunTime;
    }

    /*
     *   Receive notification that this is a non-idle turn.  This is
     *   called whenever a command in our pending command queue is about
     *   to be executed.
     *   
     *   This method need not do anything at all, since the caller will
     *   take care of running the pending command.  The purpose of this
     *   method is to take care of any changes an actor wants to make when
     *   it receives an explicit command, as opposed to running its own
     *   autonomous activity.
     *   
     *   By default, we cancel follow mode if it's in effect.  It usually
     *   makes sense for an explicit command to interrupt follow mode;
     *   follow mode is usually started by an explicit command in the
     *   first place, so it is usually sensible for a new command to
     *   replace the one that started follow mode.
     */
    nonIdleTurn()
    {
        /* by default, cancel follow mode */
        followingActor = nil;
    }

    /*
     *   If we're following an actor, this keeps track of the actor we're
     *   following.  NPC's can use this to follow around another actor
     *   whenever possible.  
     */
    followingActor = nil

    /*
     *   Handle a situation where we're trying to follow an actor but
     *   can't.  By default, this simply cancels our follow mode.
     *   
     *   Actors might want to override this to be more tolerant.  For
     *   example, an actor might want to wait until five turns elapse to
     *   give up on following, in case the target actor returns after a
     *   brief digression; or an actor could stay in follow mode until it
     *   received other instructions, or found something better to do.  
     */
    cannotFollow()
    {
        /* 
         *   by default, simply cancel follow mode by forgetting about the
         *   actor we're following
         */
        followingActor = nil;
    }

    /*
     *   Execute one "turn" - this is a unit of time passing.  The player
     *   character generally is allowed to execute one command in the
     *   course of a turn; a non-player character with a programmed task
     *   can perform an increment of the task.
     *   
     *   We set up an ActorTurnAction environment and invoke our
     *   executeActorTurn() method.  In most cases, subclasses should
     *   override executeActorTurn() rather than this method, since
     *   overriding executeTurn() directly will lose the action
     *   environment.  
     */
    executeTurn()
    {
        /* start a new command visually when a new actor is taking over */
        "<.commandsep>";
        
        /* 
         *   Execute the turn in a daemon action context, and in the sight
         *   context of the actor.  The sense context will ensure that we
         *   report the results of the action only if the actor is visible
         *   to the player character; in most cases, the actor's
         *   visibility is equivalent to the visibility of the effects, so
         *   this provides a simple way of ensuring that the results of
         *   the action are reported if and only if they're visible to the
         *   player character.
         *   
         *   Note that if we are the player character, don't use the sense
         *   context filtering -- we normally want full reports for
         *   everything the player character does.  
         */
        return withActionEnv(EventAction, self,
            {: callWithSenseContext(isPlayerChar() ? nil : self, sight,
                                    {: executeActorTurn() }) });
    }

    /* 
     *   The main processing for an actor's turn.  In most cases,
     *   subclasses should override this method (rather than executeTurn)
     *   to specialize an actor's turn processing. 
     */
    executeActorTurn()
    {
        /*
         *   If we have a pending response, and we're in a position to
         *   deliver it, our next work is to deliver the pending response.
         */
        if (pendingResponse != nil && canTalkTo(pendingResponse.issuer_))
        {
            /* 
             *   We have a pending response, and the command issuer from
             *   the pending response can hear us now, so we can finally
             *   deliver the response.
             *   
             *   If the issuer is the player character, send to the player
             *   using our deferred message generator; otherwise, call the
             *   issuer's notification routine, since it's an NPC-to-NPC
             *   notification.  
             */
            if (pendingResponse.issuer_.isPlayerChar())
            {
                /* 
                 *   we're notifying the player - use the deferred message
                 *   generator 
                 */
                getParserDeferredMessageObj().(pendingResponse.prop_)(
                    self, pendingResponse.args_...);
            }
            else
            {
                /* it's an NPC-to-NPC notification - notify the issuer */
                pendingResponse.issuer_.notifyIssuerParseFailure(
                    self, pendingResponse.prop_, pendingResponse.args_);
            }

            /* 
             *   in either case, we've gotten this out of our system now,
             *   so we can forget about the pending response 
             */
            pendingResponse = nil;
        }
            
        /* check to see if we're waiting for another actor */
        if (checkWaitingForActor())
        {
            /* 
             *   we're still waiting, so there's nothing for us to do; take
             *   an idle turn and return 
             */
            idleTurn();
            return true;
        }
            
        /* 
         *   if we're the player character, and we have no pending commands
         *   to execute, our next task will be to read and execute a
         *   command 
         */
        if (pendingCommand.length() == 0 && isPlayerChar())
        {
            local toks;
            
            /* read a command line and get the resulting token list */
            toks = readMainCommandTokens(rmcCommand);
            
            /* 
             *   re-activate the main transcript - reading the command
             *   line will have deactivated the transcript, but we want it
             *   active again now that we're about to start executing the
             *   command 
             */
            gTranscript.activate();
            
            /* 
             *   If it came back nil, it means that the input was fully
             *   processed in pre-parsing; this means that we don't have
             *   any more work to do on this turn, so we can simply end our
             *   turn now.  
             */
            if (toks == nil)
                return true;
            
            /* retrieve the token list from the command line */
            toks = toks[2];
            
            /* 
             *   Add it to our pending command queue.  Since we read the
             *   command from the player, and we're the player character,
             *   we treat the command as coming from myself.
             *   
             *   Since this is a newly-read command line, we're starting a
             *   new sentence.  
             */
            addPendingCommand(true, self, toks);
        }

        /*
         *   Check to see if we have any pending command to execute.  If
         *   so, our next task is to execute the pending command.  
         */
        if (pendingCommand.length() != 0)
        {
            local cmd;
            
            /* remove the first pending command from our queue */
            cmd = pendingCommand[1];
            pendingCommand.removeElementAt(1);
            
            /* if this is a real command, note the non-idle turn */
            if (cmd.hasCommand)
                nonIdleTurn();
            
            /* execute the first pending command */
            cmd.executePending(self);
            
            /* 
             *   We're done with this turn.  If we no longer have any
             *   pending commands, tell the scheduler to refigure the
             *   execution order, since another object might now be ready
             *   to run ahead of our idle activity.  
             */
            if (pendingCommand.indexWhich({x: x.hasCommand}) == nil)
                return nil;
            else
                return true;
        }
        
        /*
         *   If we're following an actor, and the actor isn't in sight, see
         *   if we can catch up.  
         */
        if (followingActor != nil
            && location != nil
            && (followingActor.location.effectiveFollowLocation
                != location.effectiveFollowLocation))
        {
            local info;
            
            /* see if we have enough information to follow */
            info = getFollowInfo(followingActor);
                
            /* 
             *   Check to see if we have enough information to follow the
             *   actor.  We can only follow if we saw the actor depart at
             *   some point, and we're in the same location where we last
             *   saw the actor depart.  (We have to be in the same
             *   location, because we follow by performing the same command
             *   we saw the actor perform when we last saw the actor
             *   depart.  Repeating the command will obviously be
             *   ineffective unless we're in the same location as the actor
             *   was.)  
             */
            if (info != nil)
            {
                local success;
                
                /* 
                 *   we know how to follow the actor, so simply perform
                 *   the same command we saw the actor perform.  
                 */
                newActorAction(self, Follow, followingActor);
                
                /* note whether or not we succeeded */
                success = (location.effectiveFollowLocation ==
                           followingActor.location.effectiveFollowLocation);
                    
                /* notify the state object of our attempt */
                curState.justFollowed(success);
                
                /* 
                 *   if we failed to track the actor, note that we are
                 *   unable to follow the actor 
                 */
                if (!success)
                {
                    /* note that we failed to follow the actor */
                    cannotFollow();
                }
                
                /* we're done with this turn */
                return true;
            }
            else
            {
                /* 
                 *   we don't know how to follow this actor - call our
                 *   cannot-follow handler 
                 */
                cannotFollow();
            }
        }

        /* we have no pending work to perform, so take an idle turn */
        idleTurn();
        
        /* no change in scheduling priority */
        return true;
    }

    /*
     *   By default, all actors are likely command targets.  This should
     *   be overridden for actors who are obviously not likely to accept
     *   commands of any kind.
     *   
     *   This is used to disambiguate target actors in commands, so this
     *   should provide an indication of what should be obvious to a
     *   player, because the purpose of this information is to guess what
     *   the player is likely to take for granted in specifying a target
     *   actor.
     */
    isLikelyCommandTarget = true

    /*
     *   Determine if we should accept a command.  'issuingActor' is the
     *   actor who issued the command: if the player typed the command on
     *   the command line, this will be the player character actor.
     *   
     *   This routine performs only the simplest check, since it doesn't
     *   have access to the specific action being performed.  This is
     *   intended as a first check, to allow us to bypass noun resolution
     *   if the actor simply won't accept any command from the issuer.
     *   
     *   Returns true to accept a command, nil to reject it.  If this
     *   routine returns nil, and the command came from the player
     *   character, a suitable message should be displayed.
     *   
     *   Note that most actors should not override this routine simply to
     *   express the will of the actor to accept a command, since this
     *   routine performs a number of checks for the physical ability of
     *   the actor to execute a command from the issuer.  To determine
     *   whether or not the actor should obey physically valid commands
     *   from the issuer, override obeyCommand().  
     */
    acceptCommand(issuingActor)
    {
        /* if we're the current player character, accept any command */
        if (isPlayerChar())
            return true;

        /* if we can't hear the issuer, we can't talk to it */
        if (issuingActor != self && !issuingActor.canTalkTo(self))
        {
            /* report that the target actor can't hear the issuer */
            reportFailure(&objCannotHearActorMsg, self);

            /* tell the caller that the command cannot proceed */
            return nil;
        }

        /* if I'm busy doing something else, say so */
        if (nextRunTime > Schedulable.gameClockTime)
        {
            /* tell the issuing actor I'm busy */
            notifyParseFailure(issuingActor, &refuseCommandBusy,
                               [issuingActor]);

            /* tell the caller to abandon the command */
            return nil;
        }

        /* check to see if I have other work to perform first */
        if (!acceptCommandBusy(issuingActor))
            return nil;

        /* we didn't find any reason to object, so allow the command */
        return true;
    }

    /*
     *   Check to see if I'm busy with pending commands, and if so,
     *   whether or not I should accept a new command.  Returns true if we
     *   should accept a command, nil if not.  If we return nil, we must
     *   notify the issuer of the rejection.
     *   
     *   By default, we won't accept a command if we have any work
     *   pending.  
     */
    acceptCommandBusy(issuingActor)
    {
        /* if we have any pending commands, don't accept a new command */
        if (pendingCommand.length() != 0)
        {
            /* 
             *   if we have only commands from the same issuer pending,
             *   cancel all of the pending commands and accept the new
             *   command instead 
             */
            foreach (local info in pendingCommand)
            {
                /* 
                 *   if this is from a different issuer, don't accept a
                 *   new command 
                 */
                if (info.issuer_ != issuingActor)
                {
                    /* tell the other actor that we're busy */
                    notifyParseFailure(issuingActor, &refuseCommandBusy,
                                       [issuingActor]);

                    /* tell the caller to abandon the command */
                    return nil;
                }
            }

            /* 
             *   all of the pending commands were from the same issuer, so
             *   presumably the issuer wants to override those commands;
             *   remove the old ones from our pending queue
             */
            pendingCommand.removeRange(1, pendingCommand.length());
        }

        /* we didn't find any problems */
        return true;
    }

    /*
     *   Determine whether or not we want to obey a command from the given
     *   actor to perform the given action.  We only get this far when we
     *   determine that it's possible for us to accept a command, given
     *   the sense connections between us and the issuing actor, and given
     *   our pending command queue.
     *   
     *   When this routine is called, the action has been determined, and
     *   the noun phrases have been resolved.  However, we haven't
     *   actually started processing the action yet, so the globals for
     *   the noun slots (gDobj, gIobj, etc) are NOT available.  If the
     *   routine needs to know which objects are involved, it must obtain
     *   the full list of resolved objects from the action (using, for
     *   example, getResolvedDobjList()).
     *   
     *   When there's a list of objects to be processed (as in GET ALL),
     *   we haven't started working on any one of them yet - this check is
     *   made once for the entire command, and applies to the entire list
     *   of objects.  If the actor wants to respond specially to
     *   individual objects, you can do that by overriding actorAction()
     *   instead of this routine.
     *   
     *   This routine should display an appropriate message and return nil
     *   if the command is not to be accepted, and should simply return
     *   true to accept the command.
     *   
     *   By default, we'll let our state object handle this.
     *   
     *   Note that actors that override this might also need to override
     *   wantsFollowInfo(), since an actor that accepts "follow" commands
     *   will need to keep track of the movements of other actors if it is
     *   to carry out any following.  
     */
    obeyCommand(issuingActor, action)
    {
        /* note that the issuing actor is targeting me in conversation */
        issuingActor.noteConversation(self);

        /* let the state object handle it */
        return curState.obeyCommand(issuingActor, action);
    }
    
    /* 
     *   Say hello/goodbye/yes/no to the given actor.  We'll greet the
     *   target actor is the target actor was specified (i.e., actor !=
     *   self); otherwise, we'll greet our current default conversational
     *   partner, if we have one.  
     */
    sayHello(actor) { sayToActor(actor, helloTopicObj, helloConvType); }
    sayGoodbye(actor) { sayToActor(actor, byeTopicObj, byeConvType); }
    sayYes(actor) { sayToActor(actor, yesTopicObj, yesConvType); }
    sayNo(actor) { sayToActor(actor, noTopicObj, noConvType); }

    /* handle one of the conversational addresses */
    sayToActor(actor, topic, convType)
    {
        /*
         *   If the target actor is the same as the issuing actor, then no
         *   target actor was specified in the command, so direct the
         *   address to our current conversational partner, if we have
         *   one. 
         */
        if (actor == self)
            actor = getDefaultInterlocutor();

        /* 
         *   if we found an actor, send the address to the actor's state
         *   object; otherwise, handle it with the given default message 
         */
        if (actor != nil)
        {
            /* make sure we can talk to the other actor */
            if (!canTalkTo(actor))
            {
                /* can't talk to them - say so and give up */
                reportFailure(&objCannotHearActorMsg, actor);
                exit;
            }

            /* remember our current conversational partner */
            noteConversation(actor);
            
            /* handle it as a topic */
            actor.curState.handleConversation(self, topic, convType);
        }
        else
        {
            /* 
             *   we don't know whom we're addressing; just show the default
             *   message for an unknown interlocutor
             */
            mainReport(convType.unknownMsg);
        }
    }

    /* 
     *   Handle the XSPCLTOPIC pseudo-command.  This command is generated
     *   by the SpecialTopic pre-parser when it recognizes the player's
     *   input as matching an active SpecialTopic's custom syntax.  Our
     *   job is to route this back to our current interlocutor's active
     *   ConvNode, so that it can find the SpecialTopic that it matched in
     *   pre-parsing and show its response. 
     */
    saySpecialTopic()
    {
        local actor;
        
        /* send it to our interlocutor */
        if ((actor = getCurrentInterlocutor()) == nil
            || actor.curConvNode == nil)
        {
            /*
             *   We don't seem to have a current interlocutor, or the
             *   interlocutor doesn't have a current conversation node.
             *   This is inconsistent; there's no way we could have
             *   generated XSPCLTOPIC from our pre-parser under these
             *   conditions.  The most likely thing is that the player
             *   tried typing in XSPCLTOPIC manually.  Politely ignore it. 
             */
            gLibMessages.commandNotPresent;
        }
        else
        {
            /* note the conversation directed to the other actor */
            noteConversation(actor);

            /* send the request to the ConvNode for processing */
            actor.curConvNode.saySpecialTopic(self);
        }
    }

    /* 
     *   Add a command to our pending command list.  The new command is
     *   specified as a list of tokens to be parsed, and it is added after
     *   any commands already in our pending list.  
     */
    addPendingCommand(startOfSentence, issuer, toks)
    {
        /* add a descriptor to the pending command list */
        pendingCommand.append(
            new PendingCommandToks(startOfSentence, issuer, toks));
    }

    /* 
     *   Insert a command at the head of our pending command list.  The
     *   new command is specified as a list of tokens to parse, and it is
     *   inserted into our pending command list before any commands
     *   already in the list.  
     */
    addFirstPendingCommand(startOfSentence, issuer, toks)
    {
        /* add a descriptor to the start of our list */
        pendingCommand.insertAt(
            1, new PendingCommandToks(startOfSentence, issuer, toks));
    }

    /*
     *   Add a resolved action to our pending command list.  The new
     *   command is specified as a resolved Action object; it is added
     *   after any commands already in our list. 
     */
    addPendingAction(startOfSentence, issuer, action, [objs])
    {
        /* add a descriptor to the pending command list */
        pendingCommand.append(new PendingCommandAction(
            startOfSentence, issuer, action, objs...));
    }

    /*
     *   Insert a resolved action at the start of our pending command
     *   list.  The new command is specified as a resolved Action object;
     *   it is added before any commands already in our list.  
     */
    addFirstPendingAction(startOfSentence, issuer, action, [objs])
    {
        /* add a descriptor to the pending command list */
        pendingCommand.insertAt(1, new PendingCommandAction(
            startOfSentence, issuer, action, objs...));
    }


    /* pending commands - this is a list of PendingCommandInfo objects */
    pendingCommand = nil

    /* 
     *   pending response - this is a single PendingResponseInfo object,
     *   which we'll deliver as soon as the issuing actor is in a position
     *   to hear us 
     */
    pendingResponse = nil

    /* 
     *   get the library message object for a parser message addressed to
     *   the player character 
     */
    getParserMessageObj()
    {
        /* 
         *   If I'm the player character, use the player character message
         *   object; otherwise, use the default non-player character
         *   message object.
         *   
         *   To customize parser messages from a particular actor, create
         *   an object based on npcMessages, and override this routine in
         *   the actor so that it returns the custom object rather than
         *   the standard npcMessages object.  To customize messages for
         *   ALL of the NPC's in a game, simply modify npcMessages itself,
         *   since it's the default for all non-player characters.  
         */
        return isPlayerChar() ? playerMessages : npcMessages;
    }

    /*
     *   Get the deferred library message object for a parser message
     *   addressed to the player character.  We only use this to generate
     *   messages deferred from non-player characters.  
     */
    getParserDeferredMessageObj() { return npcDeferredMessages; }

    /*
     *   Get the library message object for action responses.  This is
     *   used to generate library responses to verbs.  
     */
    getActionMessageObj()
    {
        /* 
         *   return the default player character or NPC message object,
         *   depending on whether I'm the player or not; individual actors
         *   can override this to supply actor-specific messages for
         *   library action responses 
         */
        return isPlayerChar() ? playerActionMessages: npcActionMessages;
    }

    /* 
     *   Notify an issuer that a command sent to us resulted in a parsing
     *   failure.  We are meant to reply to the issuer to let the issuer
     *   know about the problem.  messageProp is the libGlobal message
     *   property describing the error, and args is a list with the
     *   (varargs) arguments to the message property.  
     */
    notifyParseFailure(issuingActor, messageProp, args)
    {
        /* 
         *   In case the actor is in a remote location but in scope for the
         *   purposes of the conversation only (such as over a phone or
         *   radio), run this in a neutral sense context.  Since we're
         *   reporting a parser failure, we want the message to be
         *   displayed no matter what the scope situation is. 
         */
        callWithSenseContext(nil, nil, function()
        {
            /* check who's talking to whom */
            if (issuingActor.isPlayerChar())
            {
                /*
                 *   The player issued the command.  If the command was
                 *   directed to an NPC (i.e., we're not the player), check
                 *   to see if the player character is in scope from our
                 *   perspective.  
                 */
                if (issuingActor != self && !canTalkTo(issuingActor))
                {
                    /* 
                     *   The player issued the command to an NPC, but the
                     *   player is not capable of hearing the NPC's
                     *   response.  
                     */
                    cannotRespondToCommand(issuingActor, messageProp, args);
                }
                else
                {
                    /* 
                     *   generate a message using the appropriate message
                     *   generator object 
                     */
                    getParserMessageObj().(messageProp)(self, args...);
                }
            }
            else
            {
                /*
                 *   the command was issued from one NPC to another -
                 *   notify the issuer of the problem, but don't display
                 *   any messages, since this interaction is purely among
                 *   the NPC's 
                 */
                issuingActor.
                    notifyIssuerParseFailure(self, messageProp, args);
            }
        });
    }

    /*
     *   We have a parser error to report to the player, but we cannot
     *   respond at the moment because the player is not capable of
     *   hearing us (there is no sense path for our communications senses
     *   from us to the player actor).  Defer reporting the message until
     *   later.
     */
    cannotRespondToCommand(issuingActor, messageProp, args)
    {
        /* 
         *   Remember the problem for later deliver.  If we already have a
         *   deferred response, forget it - just report the latest
         *   problem.  
         */
        pendingResponse =
            new PendingResponseInfo(issuingActor, messageProp, args);

        /*
         *   Some actors might want to override this to start searching
         *   for the player character.  We don't have any generic
         *   mechanism to conduct such a search, but a game that
         *   implements one might want to make use of it here.  
         */
    }

    /*
     *   Receive notification that a command we sent to another NPC
     *   failed.  This is only called when one NPC sends a command to
     *   another NPC; this is called on the issuer to let the issuer know
     *   that the target can't perform the command because of the given
     *   resolution failure.
     *   
     *   By default, we don't do anything here, because we don't have any
     *   default code to send a command from one NPC to another.  Any
     *   custom NPC actor that sends a command to another NPC actor might
     *   want to use this to deal with problems in processing those
     *   commands.  
     */
    notifyIssuerParseFailure(targetActor, messageProp, args)
    {
        /* by default, we do nothing */
    }

    /*
     *   Antecedent lookup table.  Each actor keeps its own table of
     *   antecedents indexed by pronoun type, so that we can
     *   simultaneously have different antecedents for different pronouns.
     */
    antecedentTable = nil

    /* 
     *   Possessive anaphor lookup table.  In almost all cases, the
     *   possessive anaphor for a given pronoun will be the same as the
     *   corresponding regular pronoun: HIS indicates possession by HIM,
     *   for example.  In a few cases, though, the anaphoric quality of
     *   possessives takes precedence, and these will differ.  For
     *   example, in TELL BOB TO DROP HIS BOOK, "his" refers back to Bob,
     *   while in TELL BOB TO HIT HIM, "him" refers to whatever it
     *   referred to before the command.  
     */
    possAnaphorTable = nil

    /* 
     *   set the antecedent for the neuter singular pronoun ("it" in
     *   English) 
     */
    setIt(obj)
    {
        setPronounAntecedent(PronounIt, obj);
    }
    
    /* set the antecedent for the masculine singular ("him") */
    setHim(obj)
    {
        setPronounAntecedent(PronounHim, obj);
    }
    
    /* set the antecedent for the feminine singular ("her") */
    setHer(obj)
    {
        setPronounAntecedent(PronounHer, obj);
    }

    /* set the antecedent list for the ungendered plural pronoun ("them") */
    setThem(lst)
    {
        setPronounAntecedent(PronounThem, lst);
    }

    /* look up a pronoun's value */
    getPronounAntecedent(typ)
    {
        /* get the stored antecedent for this pronoun */
        return antecedentTable[typ];
    }

    /* set a pronoun's antecedent value */
    setPronounAntecedent(typ, val)
    {
        /* remember the value in the antecedent table */
        antecedentTable[typ] = val;

        /* set the same value for the possessive anaphor */
        possAnaphorTable[typ] = val;
    }

    /* set a possessive anaphor value */
    setPossAnaphor(typ, val)
    {
        /* set the value in the possessive anaphor table only */
        possAnaphorTable[typ] = val;
    }

    /* get a possessive anaphor value */
    getPossAnaphor(typ) { return possAnaphorTable[typ]; }

    /* forget the possessive anaphors */
    forgetPossAnaphors()
    {
        /* copy all of the antecedents to the possessive anaphor table */
        antecedentTable.forEachAssoc(
            {key, val: possAnaphorTable[key] = val});
    }

    /*
     *   Copy pronoun antecedents from the given actor.  This should be
     *   called whenever an actor issues a command to us, so that pronouns
     *   in the command are properly resolved relative to the issuer.  
     */
    copyPronounAntecedentsFrom(issuer)
    {
        /* copy every element from the issuer's table */
        issuer.antecedentTable.forEachAssoc(
            {key, val: setPronounAntecedent(key, val)});
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Verb processing 
     */

    /* show a "take from" message as indicating I don't have the dobj */
    takeFromNotInMessage = &takeFromNotInActorMsg

    /* verify() handler to check against applying an action to 'self' */
    verifyNotSelf(msg)
    {
        /* check to make sure we're not trying to do this to myself */
        if (self == gActor)
            illogicalSelf(msg);
    }

    /* macro to verify we're not self, and inherit the default behavior */
#define verifyNotSelfInherit(msg) \
    verify() \
    { \
        verifyNotSelf(msg); \
        inherited(); \
    }
    
    /* 
     *   For the basic physical manipulation verbs (TAKE, DROP, PUT ON,
     *   etc), it's illogical to operate on myself, so check for this in
     *   verify().  Otherwise, handle these as we would ordinary objects,
     *   since we might be able to manipulate other actors in the normal
     *   manner, especially actors small enough that we can pick them up. 
     */
    dobjFor(Take) { verifyNotSelfInherit(&takingSelfMsg) }
    dobjFor(Drop) { verifyNotSelfInherit(&droppingSelfMsg) }
    dobjFor(PutOn) { verifyNotSelfInherit(&puttingSelfMsg) }
    dobjFor(PutUnder) { verifyNotSelfInherit(&puttingSelfMsg) }
    dobjFor(Throw) { verifyNotSelfInherit(&throwingSelfMsg) }
    dobjFor(ThrowAt) { verifyNotSelfInherit(&throwingSelfMsg) }
    dobjFor(ThrowDir) { verifyNotSelfInherit(&throwingSelfMsg) }
    dobjFor(ThrowTo) { verifyNotSelfInherit(&throwingSelfMsg) }

    /* customize the message for THROW TO <actor> */
    iobjFor(ThrowTo)
    {
        verify()
        {
            /* by default, we don't want to catch anything */
            illogical(&willNotCatchMsg, self);
        }
    }

    /* treat PUT SELF IN FOO as GET IN FOO */
    dobjFor(PutIn)
    {
        verify()
        {
            /* the target actor is always unsuitable as a default */
            if (gActor == self)
                nonObvious;
        }

        check()
        {
            /* if I'm putting myself somewhere, treat it as GET IN */
            if (gActor == self)
                replaceAction(Enter, gIobj);

            /* do the normal work */
            inherited();
        }
    }

    dobjFor(Kiss)
    {
        preCond = [touchObj]
        verify()
        {
            /* cannot kiss oneself */
            verifyNotSelf(&cannotKissSelfMsg);
        }
        action() { mainReport(&cannotKissActorMsg); }
    }

    dobjFor(AskFor)
    {
        preCond = [canTalkToObj]
        verify()
        {
            /* it makes no sense to ask myself for something */
            verifyNotSelf(&cannotAskSelfForMsg);
        }
        action()
        {
            /* note that the issuer is targeting us with conversation */
            gActor.noteConversation(self);

            /* let the state object handle it */
            curState.handleConversation(gActor, gTopic, askForConvType);
        }
    }

    dobjFor(TalkTo)
    {
        preCond = [canTalkToObj]
        verify()
        {
            /* it's generally illogical to talk to oneself */
            verifyNotSelf(&cannotTalkToSelfMsg);
        }
        action()
        {
            /* note that the issuer is targeting us in conversation */
            gActor.noteConversation(self);

            /* handle it as a 'hello' topic */
            curState.handleConversation(gActor, helloTopicObj, helloConvType);
        }
    }

    iobjFor(GiveTo)
    {
        verify()
        {
            /* it makes no sense to give something to myself */
            verifyNotSelf(&cannotGiveToSelfMsg);

            /* it also makes no sense to give something to itself */
            if (gDobj == gIobj)
                illogicalSelf(&cannotGiveToItselfMsg);
        }
        action()
        {
            /* take note that I've seen the direct object */
            noteObjectShown(gDobj);

            /* note that the issuer is targeting us with conversation */
            gActor.noteConversation(self);

            /* let the state object handle it */
            curState.handleConversation(gActor, gDobj, giveConvType);
        }
    }

    iobjFor(ShowTo)
    {
        verify()
        {
            /* it makes no sense to show something to myself */
            verifyNotSelf(&cannotShowToSelfMsg);

            /* it also makes no sense to show something to itself */
            if (gDobj == gIobj)
                illogicalSelf(&cannotShowToItselfMsg);
        }
        action()
        {
            /* take note that I've seen the direct object */
            noteObjectShown(gDobj);

            /* note that the issuer is targeting us with conversation */
            gActor.noteConversation(self);

            /* let the actor state object handle it */
            curState.handleConversation(gActor, gDobj, showConvType);
        }
    }

    /*
     *   Note that the given object has been explicitly shown to me.  By
     *   default, we'll mark the object and its visible contents as having
     *   been seen by me.  This is called whenever we're the target of a
     *   SHOW TO or GIVE TO, since presumably such an explicit act of
     *   calling our attention to an object would make us consider the
     *   object as having been seen in the future.  
     */
    noteObjectShown(obj)
    {
        local info;

        /* get the table of things we can see */
        info = visibleInfoTable();

        /* if the object is in the table, mark it as seen */
        if (info[obj] != nil)
            setHasSeen(obj);

        /* also mark the visible contents of the object as having been seen */
        obj.setContentsSeenBy(info, self);
    }

    dobjFor(AskAbout)
    {
        preCond = [canTalkToObj]
        verify()
        {
            /* it makes no sense to ask oneself about something */
            verifyNotSelf(&cannotAskSelfMsg);
        }
        action()
        {
            /* note that the issuer is targeting us with conversation */
            gActor.noteConversation(self);

            /* let our state object handle it */
            curState.handleConversation(gActor, gTopic, askAboutConvType);
        }
    }

    dobjFor(TellAbout)
    {
        preCond = [canTalkToObj]
        verify()
        {
            /* it makes no sense to tell oneself about something */
            verifyNotSelf(&cannotTellSelfMsg);
        }
        check()
        {
            /* 
             *   If the direct object is the issuing actor, rephrase this
             *   as "issuer, ask actor about iobj".
             *   
             *   Note that we do this in 'check' rather than 'action',
             *   because this will ensure that we'll rephrase the command
             *   properly even if the subclass overrides with its own
             *   check, AS LONG AS the overriding method inherits this base
             *   definition first.  If we did the rephrasing in the
             *   'action', then an overriding 'check' might incorrectly
             *   disqualify the operation on the assumption that it's an
             *   ordinary TELL ABOUT rather than what it really is, which
             *   is a rephrased ASK ABOUT.  
             */
            if (gDobj == gIssuingActor)
                replaceActorAction(gIssuingActor, AskAbout, gActor, gTopic);

        }
        action()
        {
            /* note that the issuer is targeting us with conversation */
            gActor.noteConversation(self);

            /* let the state object handle it */
            curState.handleConversation(gActor, gTopic, tellAboutConvType);
        }
    }

    /*
     *   Handle a conversational command.  All of the conversational
     *   actions (HELLO, GOODBYE, YES, NO, ASK ABOUT, ASK FOR, TELL ABOUT,
     *   SHOW TO, GIVE TO) are routed here when we're the target of the
     *   action (for example, we're BOB in ASK BOB ABOUT TOPIC) AND the
     *   ActorState doesn't want to handle the action. 
     */
    handleConversation(actor, topic, convType)
    {
        /* try handling the topic from our topic database */
        if (!handleTopic(actor, topic, convType, nil))
        {
            /* the topic database didn't handle it; use a default response */
            defaultConvResponse(actor, topic, convType);
        }
    }

    /*
     *   Show a default response to a conversational action.  By default,
     *   we'll show the default response for our conversation type.  
     */
    defaultConvResponse(actor, topic, convType)
    {
        /* call the appropriate default response for the ConvType */
        convType.defaultResponse(self, actor, topic);
    }

    /* 
     *   Show our default greeting message - this is used when the given
     *   another actor greets us with HELLO or TALK TO, and we don't
     *   otherwise handle it (such as via a topic database entry).
     *   
     *   By default, we'll just show "there's no response" as a default
     *   message.  We'll show this in default mode, so that if the caller
     *   is going to show a list of suggested conversation topics (which
     *   the 'hello' and 'talk to' commands will normally try to do), the
     *   topic list will override the "there's no response" default.  In
     *   other words, we'll have one of these two types of exchanges:
     *   
     *.  >talk to bob
     *.  There's no response
     *   
     *.  >talk to bill
     *.  You could ask him about the candle, the book, or the bell, or
     *.  tell him about the crypt.
     */
    defaultGreetingResponse(actor)
        { defaultReport(&noResponseFromMsg, self); }

    /* show our default goodbye message */
    defaultGoodbyeResponse(actor)
        { mainReport(&noResponseFromMsg, self); }

    /*
     *   Show the default answer to a question - this is called when we're
     *   the actor in ASK <actor> ABOUT <topic>, and we can't find a more
     *   specific response for the given topic.
     *   
     *   By default, we'll show the basic "there's no response" message.
     *   This isn't a very good message in most cases, because it makes an
     *   actor pretty frustratingly un-interactive, which gives the actor
     *   the appearance of a cardboard cut-out.  But there's not much
     *   better that the library can do; the potential range of actors
     *   makes a more specific default response impossible.  If the default
     *   response were "I don't know about that," it wouldn't work very
     *   well if the actor is someone who only speaks Italian.  So, the
     *   best we can do is this generally rather poor default.  But that
     *   doesn't mean that authors should resign themselves to a poor
     *   default answer; instead, it means that actors should take care to
     *   override this when defining an actor, because it's usually
     *   possible to find a much better default for a *specific* actor.
     *   
     *   The *usual* way of providing a default response is to define a
     *   DefaultAskTopic (or a DefaultAskTellTopic) and put it in the
     *   actor's topic database.  
     */
    defaultAskResponse(fromActor, topic)
        { mainReport(&noResponseFromMsg, self); }

    /*
     *   Show the default response to being told of a topic - this is
     *   called when we're the actor in TELL <actor> ABOUT <topic>, and we
     *   can't find a more specific response for the topic.
     *   
     *   As with defaultAskResponse, this should almost always be
     *   overridden by each actor, since the default response ("there's no
     *   response") doesn't make the actor seem very dynamic.
     *   
     *   The usual way of providing a default response is to define a
     *   DefaultTellTopic (or a DefaultAskTellTopic) and put it in the
     *   actor's topic database.  
     */
    defaultTellResponse(fromActor, topic)
        { mainReport(&noResponseFromMsg, self); }

    /* the default response for SHOW TO */
    defaultShowResponse(byActor, topic)
        { mainReport(&notInterestedMsg, self); }

    /* the default response for GIVE TO */
    defaultGiveResponse(byActor, topic)
        { mainReport(&notInterestedMsg, self); }

    /* the default response for ASK FOR */
    defaultAskForResponse(byActor, obj)
        { mainReport(&noResponseFromMsg, self); }

    /* default response to being told YES */
    defaultYesResponse(fromActor)
        { mainReport(&noResponseFromMsg, self); }

    /* default response to being told NO */
    defaultNoResponse(fromActor)
        { mainReport(&noResponseFromMsg, self); }

    /* default refusal of a command */
    defaultCommandResponse(fromActor, topic)
        { mainReport(&refuseCommand, self, fromActor); }
;

/* ------------------------------------------------------------------------ */
/*
 *   An UntakeableActor is one that can't be picked up and moved. 
 */
class UntakeableActor: Actor, Immovable
    /* use customized messages for some 'Immovable' methods */
    cannotTakeMsg = &cannotTakeActorMsg
    cannotMoveMsg = &cannotMoveActorMsg
    cannotPutMsg = &cannotPutActorMsg

    /* TASTE tends to be a bit rude */
    dobjFor(Taste)
    {
        action() { mainReport(&cannotTasteActorMsg); }
    }

    /* 
     *   even though we act like an Immovable, we don't count as an
     *   Immovable for listing purposes 
     */
    contentsInFixedIn(loc) { return nil; }
;


/*
 *   A Person is an actor that represents a human character.  This is just
 *   an UntakeableActor with some custom versions of the messages for
 *   taking and moving the actor.  
 */
class Person: UntakeableActor
    /* customize the messages for trying to take or move me */
    cannotTakeMsg = &cannotTakePersonMsg
    cannotMoveMsg = &cannotMovePersonMsg
    cannotPutMsg = &cannotPutPersonMsg
    cannotTasteActorMsg = &cannotTastePersonMsg

    /* 
     *   use a fairly large default bulk, since people are usually fairly
     *   large compared with the sorts of items that one carries around 
     */
    bulk = 10
;

/* ------------------------------------------------------------------------ */
/*
 *   Pending response information structure 
 */
class PendingResponseInfo: object
    construct(issuer, prop, args)
    {
        issuer_ = issuer;
        prop_ = prop;
        args_ = args;
    }

    /* the issuer of the command (and target of the response) */
    issuer_ = nil

    /* the message property and argument list for the message */
    prop_ = nil
    args_ = []
;

/*
 *   Pending Command Information structure.  This is an abstract base class
 *   that we subclass for particular ways of representing the command to be
 *   executed.  
 */
class PendingCommandInfo: object
    construct(issuer) { issuer_ = issuer; }
    
    /*
     *   Check to see if this pending command item has a command to
     *   perform.  This returns true if we have a command, nil if we're
     *   just a queue placeholder without any actual command to execute.  
     */
    hasCommand = true

    /* execute the command */
    executePending(targetActor) { }

    /* the issuer of the command */
    issuer_ = nil

    /* we're at the start of a "sentence" */
    startOfSentence_ = nil
;

/* a pending command based on a list of tokens from an input string */
class PendingCommandToks: PendingCommandInfo
    construct(startOfSentence, issuer, toks)
    {
        inherited(issuer);
        
        startOfSentence_ = startOfSentence;
        tokens_ = toks;
    }

    /* 
     *   Execute the command.  We'll parse our tokens and execute the
     *   parsed results.
     */
    executePending(targetActor)
    {
        /* parse and execute the tokens */
        executeCommand(targetActor, issuer_, tokens_, startOfSentence_);
    }
    
    /* the token list for the command */
    tokens_ = nil
;

/* a pending command based on a pre-resolved Action and its objects */
class PendingCommandAction: PendingCommandInfo
    construct(startOfSentence, issuer, action, [objs])
    {
        inherited(issuer);
        
        startOfSentence_ = startOfSentence;
        action_ = action;
        objs_ = objs;
    }

    /* execute the pending command */
    executePending(targetActor)
    {
        /* invoke the action's main execution method */
        try
        {
            /* run the action */
            newActionObj(CommandTranscript, issuer_,
                         targetActor, action_, objs_...);
        }
        catch (TerminateCommandException tcExc)
        {
            /* 
             *   the command cannot proceed; simply abandon the command
             *   action here 
             */
        }
    }
    
    /* the resolved Action to perform */
    action_ = nil

    /* the resolved objects for the action */
    objs_ = nil
;

/*
 *   A pending command marker.  This is not an actual pending command;
 *   rather, it's just a queue marker.  We sometimes want to synchronize
 *   some other activity with an actor's progress through its command
 *   queue; for example, we might want one actor to wait until another
 *   actor has executed a particular pending action.  These markers can be
 *   used for this kind of synchronization; they move through the queue
 *   like ordinary pending commands, so we can tell if an actor has reached
 *   a particular command by observing the marker's progress through the
 *   queue.  
 */
class PendingCommandMarker: PendingCommandInfo
    /* I have no command to execute */
    hasCommand = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Set the current player character 
 */
setPlayer(actor)
{
    /* remember the new player character */
    libGlobal.playerChar = actor;

    /* set the root global point of view to this actor */
    setRootPOV(actor, actor);
}

