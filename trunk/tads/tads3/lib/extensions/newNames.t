/* 
 *   Copyright (c) 2000, 2007 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   NOTE: THIS FILE IS OBSOLETE!  As of TADS 3.0.19, everything in this
 *   extension is incorporated directly into the standard adv3 library, so
 *   there's no need to add it separately to your project.  You should
 *   remove it from your project's list of source files.  This file is now
 *   included in the distribution for historical reference only.
 *   
 *   
 *   TADS 3 Library Extension - New Names
 *   
 *   This is an experimental extension that enhances the new
 *   detailed-naming feature that was first introduced in 3.0.10.  This
 *   feature has been turned off by default, because it's somewhat
 *   incomplete and quirky as presently implemented.  This extension is
 *   essentially a second version of the feature that attempts to reduce
 *   the quirks and improve the feature's performance.
 *   
 *   The point of the detailed-naming feature is to generate differentiated
 *   names in object announcements: default object messages, which tell you
 *   which object the parser has selected by default when you leave a
 *   needed object out of a command ("UNLOCK DOOR / (with the gold key)");
 *   multi-object prefixes, which tell you which of the several objects in
 *   your command the parser is working on now ("TAKE GOLD KEY AND SILVER
 *   KEY / gold key: Taken / silver key: Taken"); and ambiguous selection
 *   messages, which ("TAKE COIN / (the silver coin)").  By
 *   "differentiated" names, we mean names that distinguish the object in
 *   question from other similarly named objects that are part of the same
 *   command or are also in scope.  For example, if there are several
 *   objects present that are all called "gold coin," the detailed naming
 *   system might call one of them "your gold coin" and the other "the gold
 *   coin on the floor," so that you can tell specifically which one the
 *   parser is talking about at any given time.
 *   
 *   The original scheme basically accomplished this, but it had some
 *   problems: it was a little *too* specific at times, and it wasn't as
 *   consistent as one would like.  The problem with being too specific is
 *   that it's unnatural; in natural speech, people are really good at
 *   using just enough specificity for the context.  When science fiction
 *   writers want to emphasize the alienness of an android or a Vulcan,
 *   they have the character refer to everyday objects in comically
 *   detailed, technical language; the original detailed-naming scheme had
 *   this sort of effect.  So, the main improvement this extension tries to
 *   make is to find a better balance, to use enough specificity to
 *   distinguish nearby objects, but not so much that the parser sounds
 *   like an android caricature.
 *   
 *   The specific changes:
 *   
 *   1. For multi-object announcements, the naming now only attempts to
 *   differentiate the listed objects from *each other*, NOT from
 *   everything else in scope.
 *   
 *   2. For multi-object announcements, the basis for the differentiation
 *   (location, ownership, etc) is determined *in advance* for all of the
 *   objects, before the command is executed on any of the objects.  In the
 *   original implementation, the differentiation was figured for each
 *   object on the fly, so if the command being executed affected the
 *   object state, the objects involved could become distinguishable during
 *   the command even though they weren't initially.  From the player's
 *   perspective, they're entering the command with respect to the state of
 *   the world at the start of the command, so it's more consistent with
 *   the player's expectations to determine the naming at that point in
 *   time.  This approach also results in more consistent naming overall,
 *   since the game-state basis for the naming is the same for the whole
 *   list.
 *   
 *   3. When objects are distinguishable by their base names, the base
 *   names are used.  The original implementation favored the disambigName
 *   even if the base names were distinguishable.  This was undesirable
 *   because the disambigName is essentially designed to globally
 *   distinguish an object, so it tends to be very specific; when we only
 *   need to distinguish an object from other things in scope, we usually
 *   don't need such a specific name.  
 */

#include <adv3.h>
#include <en_us.h>

modify ResolveInfo
    /*
     *   The pre-calculated multi-object announcement text for this object.
     *   When we iterate over the object list in a command with multiple
     *   direct or indirect objects (TAKE THE BOOK, BELL, AND CANDLE), we
     *   calculate the little announcement messages ("book:") for the
     *   objects BEFORE we execute the actual commands.  We then use the
     *   pre-calculated announcements during our iteration.  This ensures
     *   consistency in the basis for choosing the names, which is
     *   important in cases where the names include state-dependent
     *   information for the purposes of distinguishing one object from
     *   another.  The relevant state can change over the course of
     *   executing the command on the objects in the iteration, so if we
     *   calculated the names on the fly we could end up with inconsistent
     *   naming.  The user thinks of the objects in terms of their state at
     *   the start of the command, so the pre-calculation approach is not
     *   only more internally consistent, but is also more consistent with
     *   the user's perspective.  
     */
    multiAnnounce = nil
;

modify Thing
    /*
     *   Get the distinguisher to use for printing this object's name in an
     *   action announcement (such as a multi-object, default object, or
     *   vague-match announcement).  We check the global option setting to
     *   see if we should actually use distinguishers for this; if so, we
     *   call getInScopeDistinguisher() to find the correct distinguisher,
     *   otherwise we use the "null" distinguisher, which simply lists
     *   objects by their base names.
     *   
     *   'lst' is the list of other objects from which we're trying to
     *   differentiate 'self'.  The reason 'lst' is given is that it lets
     *   us choose the simplest name for each object that usefully
     *   distinguishes it; to do this, we need to know exactly what we're
     *   distinguishing it from.  
     */
    replace getAnnouncementDistinguisher(lst)
    {
        return (gameMain.useDistinguishersInAnnouncements
                ? getBestDistinguisher(lst)
                : nullDistinguisher);
    }

    /*
     *   Get a distinguisher that differentiates me from all of the other
     *   objects in scope, if possible, or at least from some of the other
     *   objects in scope.
     */
    replace getInScopeDistinguisher()
    {
        /* return the best distinguisher for the objects in scope */
        return getBestDistinguisher(gActor.scopeList());
    }

    /*
     *   Get a distinguisher that differentiates me from all of the other
     *   objects in the given list, if possible, or from as many of the
     *   other objects as possible. 
     */
    getBestDistinguisher(lst)
    {
        local bestDist, bestCnt;

        /* remove 'self' from the list */
        lst -= self;

        /* 
         *   Try the "null" distinguisher, which distinguishes objects
         *   simply by their base names - if that can tell us apart from
         *   the other objects, there's no need for anything more
         *   elaborate.  So, calculate the list of indistinguishable
         *   objects using the null distinguisher, and if it's empty, we
         *   can just use the null distinguisher as our result.  
         */
        if (lst.subset({obj: !nullDistinguisher.canDistinguish(self, obj)})
            .length() == 0)
            return nullDistinguisher;

        /* 
         *   Reduce the list to the set of objects that are basic
         *   equivalents of mine - these are the only ones we care about
         *   telling apart from us, since all of the other objects can be
         *   distinguished by their disambig name.  
         */
        lst = lst.subset(
            {obj: !basicDistinguisher.canDistinguish(self, obj)});

        /* if the basic distinguisher can tell us apart, just use it */
        if (lst.length() == 0)
            return basicDistinguisher;

        /* as a last resort, fall back on the basic distinguisher */
        bestDist = basicDistinguisher;
        bestCnt = lst.countWhich({obj: bestDist.canDistinguish(self, obj)});

        /* 
         *   run through my distinguisher list looking for the
         *   distinguisher that can tell me apart from the largest number
         *   of my vocab equivalents 
         */
        foreach (local dist in distinguishers)
        {
            /* if this is the default, skip it */
            if (dist == bestDist)
                continue;

            /* check to see how many objects 'dist' can distinguish me from */
            local cnt = lst.countWhich({obj: dist.canDistinguish(self, obj)});

            /* 
             *   if it can distinguish me from every other object, use this
             *   one - it uniquely identifies me 
             */
            if (cnt == lst.length())
                return dist;

            /* 
             *   that can't distinguish us from everything else here, but
             *   if it's the best so far, remember it; we'll fall back on
             *   the best that we find if we fail to find a perfect
             *   distinguisher 
             */
            if (cnt > bestCnt)
            {
                bestDist = dist;
                bestCnt = cnt;
            }
        }

        /*
         *   We didn't find any distinguishers that can tell me apart from
         *   every other object, so choose the one that can tell me apart
         *   from the most other objects. 
         */
        return bestDist;
    }
;

modify Action
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
;

modify TAction
    /* get the list of resolved objects in the given role */
    getResolvedObjList(which)
    {
        return (which == DirectObject ? getResolvedDobjList()
                : inherited(which));
    }
    
    /*
     *   Execute the action.  We'll run through the execution sequence
     *   once for each resolved direct object.  
     */
    replace doActionMain()
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

    /* announce a default object used with this action */
    announceDefaultObject(obj, whichObj, resolvedAllObjects)
    {
        local prep;
        local nm;

        /* 
         *   Get the name to display.  Since we're selecting an object
         *   automatically from everything in scope, we need to
         *   differentiate the object from the other objects in scope. 
         */
        nm = obj.getAnnouncementDistinguisher(gActor.scopeList())
            .theName(obj);

        /*
         *   get any direct object preposition - this is the part inside
         *   the "(what)" specifier parens, excluding the last word
         */
        rexSearch('<lparen>(.*<space>+)?<alpha>+<rparen>', verbPhrase);
        prep = (rexGroup(1) == nil ? '' : rexGroup(1)[3]);

        /* do any verb-specific adjustment of the preposition */
        if (prep != nil)
            prep = adjustDefaultObjectPrep(prep, obj);

        /* show the preposition (if any) and the object */
        return (prep == '' ? nm : prep + nm);
    }
;

modify TIAction
    /* get the list of resolved objects in the given role */
    getResolvedObjList(which)
    {
        return (which == IndirectObject ? getResolvedIobjList()
                : inherited(which));
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

    /* announce a default object used with this action */
    announceDefaultObject(obj, whichObj, resolvedAllObjects)
    {
        local verb;
        local prep;
        local nm;

        /* presume we won't have a verb or preposition */
        verb = '';
        prep = '';

        /*
         *   Check the full phrasing - if we're showing the direct object,
         *   but an indirect object was supplied, use the verb's
         *   participle form ("asking bob") in the default string, since
         *   we must clarify that we're not tagging the default string on
         *   to the command line.  Don't include the participle form if we
         *   don't know all the objects yet, since in this case we are in
         *   fact tagging the default string onto the command so far, as
         *   there's nothing else in the command to get in the way.
         */
        if (whichObj == DirectObject && resolvedAllObjects)
        {
            /*
             *   extract the verb's participle form (including any
             *   complementizer phrase)
             */
            rexSearch('/(<^lparen>+) <lparen>', verbPhrase);
            verb = rexGroup(1)[3] + ' ';
        }

        /* get the preposition to use, if any */
        switch(whichObj)
        {
        case DirectObject:
            /* use the preposition in the first "(what)" phrase */
            rexSearch('<lparen>(.*?)<space>*<alpha>+<rparen>', verbPhrase);
            prep = rexGroup(1)[3];
            break;

        case IndirectObject:
            /* use the preposition in the second "(what)" phrase */
            rexSearch('<rparen>.*<lparen>(.*?)<space>*<alpha>+<rparen>',
                      verbPhrase);
            prep = rexGroup(1)[3];
            break;
        }

        /* 
         *   get the name to display - since we selected this object
         *   automatically from among the objects in scope, we need to
         *   differentiate it from the other objects in scope 
         */
        nm = obj.getAnnouncementDistinguisher(gActor.scopeList())
            .theName(obj);

        /* build and return the complete phrase */
        return spSuffix(verb) + spSuffix(prep) + nm;
    }
;

modify MultiObjectAnnouncement
    construct(preCalcMsg, obj, whichObj, action)
    {
        /* do the inherited work */
        inherited(obj, whichObj, action);

        /* 
         *   if we have a pre-calculated message, use it instead of the
         *   message we just generated - this lets the caller explicitly
         *   set the message as desired 
         */
        if (preCalcMsg != nil)
            messageText_ = preCalcMsg;
    }
;

modify CommandTranscript
    /*
     *   Announce one of a set of objects to a multi-object action.  We'll
     *   record this announcement for display with our report list.  
     */
    replace announceMultiActionObject(preCalcMsg, obj, whichObj)
    {
        /* save a multi-action object announcement */
        addReport(new MultiObjectAnnouncement(
            preCalcMsg, obj, whichObj, gAction));
    }
;

modify libMessages
    /*
     *   Announce the current object of a set of multiple objects on which
     *   we're performing an action.  This is used to tell the player
     *   which object we're acting upon when we're iterating through a set
     *   of objects specified in a command targeting multiple objects.  
     */
    announceMultiActionObject(obj, whichObj, action)
    {
        /* 
         *   get the display name - we only need to differentiate this
         *   object from the other objects in the iteration 
         */
        local nm = obj.getAnnouncementDistinguisher(
            action.getResolvedObjList(whichObj)).name(obj);

        /* build the announcement */
        return '<./p0>\n<.announceObj>' + nm + ':<./announceObj> <.p0>';
    }

    /*
     *   Announce a singleton object that we selected from a set of
     *   ambiguous objects.  This is used when we disambiguate a command
     *   and choose an object over other objects that are also logical but
     *   are less likely.  In such cases, it's courteous to tell the
     *   player what we chose, because it's possible that the user meant
     *   one of the other logical objects - announcing this type of choice
     *   helps reduce confusion by making it immediately plain to the
     *   player when we make a choice other than what they were thinking.  
     */
    announceAmbigActionObject(obj, whichObj, action)
    {
        /* 
         *   get the display name - distinguish the object from everything
         *   else in scope, since we chose from a set of ambiguous options 
         */
        local nm = obj.getAnnouncementDistinguisher(gActor.scopeList())
            .theName(obj);

        /* announce the object in "assume" style, ending with a newline */
        return '<.assume>' + nm + '<./assume>\n';
    }
;

