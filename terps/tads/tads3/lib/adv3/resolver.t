#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: Resolvers.
 *   
 *   This module defines the Resolver classes.  A Resolver is an abstract
 *   object that the parser uses to control the resolution of noun phrases
 *   to game objects.  Specialized Resolver subclasses allow noun phrases
 *   to be resolved differently according to their grammatical function in
 *   a command.  
 */

#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Basic object resolver.  An Action object creates an object resolver
 *   to mediate the process of resolving noun phrases to objects.
 *   
 *   A resolver encapsulates a set of object resolution rules.  In most
 *   cases, an action that takes only a direct object can be its own
 *   resolver, because it needs only one set of resolution rules; for this
 *   reason, this basic Resolver implementation is designed to work with
 *   the direct object.  Actions with multiple objects will need separate
 *   resolvers for each object, since they might want to use different
 *   rules for the different objects.  
 */
class Resolver: object
    construct(action, issuingActor, targetActor)
    {
        /* remember my action and actor objects */
        action_ = action;
        issuer_ = issuingActor;
        actor_ = targetActor;

        /* cache the scope list */
        cacheScopeList();
    }

    /* 
     *   Are we a sub-phrase resolver?  This should return true if we're
     *   being used to resolve a sub-phrase of the main phrase. 
     */
    isSubResolver = nil

    /* 
     *   Reset the resolver - this can be called if we are to re-use the
     *   same resolver to resolve a list of noun phrases again.
     */
    resetResolver()
    {
        /* forget the equivalents we've resolved so far */
        equivs_ = nil;
    }

    /* get the action we're resolving */
    getAction() { return action_; }

    /* get the target actor */
    getTargetActor() { return actor_; }

    /*
     *   Match an object's name.  By default, we'll call the object's own
     *   matchName method with the given original and adjusted token
     *   lists.  Subclasses can override this to call different match
     *   methods (such as matchNameDisambig).  
     */
    matchName(obj, origTokens, adjustedTokens)
    {
        return obj.matchName(origTokens, adjustedTokens);
    }
    
    /*
     *   Get the resolver for qualifier phrases.  By default, this simply
     *   returns myself, since the resolver for qualifiers is in most
     *   contexts the same as the main resolver.
     *   
     *   This can be overridden in contexts where the qualifier resolver
     *   is different from the main resolver.  In general, when a
     *   sub-resolver narrows the scope for resolving a phrase, such as an
     *   exclusion list or a disambiguation response, we will want to
     *   resolve qualifiers in the context of the main resolution scope
     *   rather than the narrowed scope.  
     */
    getQualifierResolver() { return self; }

    /*
     *   Get the resolver for possessive phrases.  By default, we return a
     *   standard possessive resolver.  This can be overridden in contexts
     *   wher ethe possesive resolution context is special.  
     */
    getPossessiveResolver() { return new PossessiveResolver(self); }

    /*
     *   Cache the scope list for this object.  By default, we cache the
     *   standard physical scope list for our target actor.
     *   
     *   Note that if a subclass uses completely different rules for
     *   determining scope, it need not store a scope_ list at all.  The
     *   scope_ list is purely an implementation detail of the base
     *   Resolver class.  A subclass can use whatever internal
     *   implementation it wants, as long as it overrides objInScope() and
     *   getScopeList() to return consistent results.
     */
    cacheScopeList()
    {
        /* cache our actor's default scope list */
        scope_ = actor_.scopeList();
    }

    /*
     *   Determine if an object is in scope for the purposes of object
     *   resolution.  By default, we'll return true if the object is in our
     *   cached scope list - this ensures that we produce results that are
     *   consistent with getScopeList().
     *   
     *   Some subclasses might want to override this method to decide on
     *   scope without reference to a cached scope list, for efficiency
     *   reasons.  For example, if a command's scope is the set of all
     *   objects, caching the full list would take a lot of memory; to save
     *   the memory, you could override cacheScopeList() to do nothing at
     *   all, and then override objInScope() to return true - this will
     *   report that every object is in scope without bothering to store a
     *   list of every object.
     *   
     *   Be aware that if you override objInScope(), you should ensure that
     *   getScopeList() yields consistent results.  In particular,
     *   objInScope() should return true for every object in the list
     *   returned by getScopeList() (although getScopeList() doesn't
     *   necessarily have to return every object for which objInScope() is
     *   true).  
     */
    objInScope(obj) { return scope_.indexOf(obj) != nil; }

    /*
     *   Get the full list of objects in scope.  By default, this simply
     *   returns our cached scope list.
     *   
     *   For every object in the list that getScopeList() returns,
     *   objInScope() must return true.  However, getScopeList() need not
     *   return *all* objects that are in scope as far as objInScope() is
     *   concerned - it can, but a subset of in-scope objects is
     *   sufficient.
     *   
     *   The default implementation returns the complete set of in-scope
     *   objects by simply returning the cached scope list.  This is the
     *   same scope list that the default objInScope() checks, which
     *   ensures that the two methods produce consistent results.
     *   
     *   The reason that it's okay for this method to return a subset of
     *   in-scope objects is that the result is only used to resolve
     *   "wildcard" phrases in input, and such phrases don't have to expand
     *   to every possible object.  Examples of wildcard phrases include
     *   ALL, missing phrases that need default objects, and locational
     *   phrases ("the vase on the table" - which isn't superficially a
     *   wildcard, but implicitly contains one in the form of "considering
     *   only everything on the table").  It's perfectly reasonable for the
     *   parser to expand a wildcard based on what's actually in sight, in
     *   mind, or whatever's appropriate.  So, in cases where you define an
     *   especially expansive objInScope() - for example, a universal scope
     *   like the one TopicResolver uses - it's usually fine to use the
     *   default definition of getScopeList(), which returns only the
     *   objects that are in the smaller physical scope.  
     */
    getScopeList() { return scope_; }

    /*
     *   Is this a "global" scope?  By default, the scope is local: it's
     *   limited to what the actor can see, hear, etc.  In some cases, the
     *   scope is broader, and extends beyond the senses; we call those
     *   cases global scope.
     *   
     *   This is an advisory status only.  The caller musn't take this to
     *   mean that everything is in scope; objInScope() and getScopeList()
     *   must still be used to make the exact determination of what objects
     *   are in scope.  However, some noun phrase productions might wish to
     *   know generally whether we're in a local or global sort of scope,
     *   so that they can adjust their zeal at reducing ambiguity.  In
     *   cases of global scope, we generally want to be more inclusive of
     *   possible matches than in local scopes, because we have much less
     *   of a basis to guess about what the player might mean.
     */
    isGlobalScope = nil

    /*
     *   Get the binding for a reflexive third-person pronoun (himself,
     *   herself, itself, themselves).  By default, the reflexive binding
     *   is the anaphoric binding from the action - that is, it refers
     *   back to the preceding noun phrase in a verb phrase with multiple
     *   noun slots (as in ASK BOB ABOUT HIMSELF: 'himself' refers back to
     *   'bob', the previous noun phrase).  
     */
    getReflexiveBinding(typ) { return getAction().getAnaphoricBinding(typ); }

    /*
     *   Resolve a pronoun antecedent, given a pronoun selector.  This
     *   returns a list of ResolveInfo objects, for use in object
     *   resolution.  'poss' is true if this is a possessive pronoun (his,
     *   her, its, etc), nil if it's an ordinary, non-possessive pronoun
     *   (him, her, it, etc).  
     */
    resolvePronounAntecedent(typ, np, results, poss)
    {
        local lst;
        local scopeLst;

        /* check the Action for a special override for the pronoun */
        lst = getAction().getPronounOverride(typ);

        /* if there's no override, get the standard raw antecedent list */
        if (lst == nil)
            lst = getRawPronounAntecedent(typ);

        /* if there is no antecedent, return an empty list */
        if (lst != nil && lst != [])
        {
            local cur;

            /* if it's a single object, turn it into a list */
            if (dataType(lst) == TypeObject)
                lst = [lst];

            /* add any extra objects for the pronoun binding */
            foreach (cur in lst)
                lst = cur.expandPronounList(typ, lst);

            /* filter the list to keep only in-scope objects */
            scopeLst = new Vector(lst.length());
            foreach (cur in lst)
            {
                local facets;
                
                /* get the object's facets */
                facets = cur.getFacets();

                /* 
                 *   If it has any, pick the best one that's in scope.  If
                 *   not, keep the object only if it's in scope. 
                 */
                if (facets.length() != 0)
                {
                    local best;
                    
                    /* 
                     *   This object has other facets, so we want to
                     *   consider the other in-scope facets in case any are
                     *   more suitable than the original one.  For example,
                     *   we might have just referred to a door, and then
                     *   traveled through the door to an adjoining room.
                     *   We now want the antecedent to be the side (facet)
                     *   of the door that's in the new location.  
                     */

                    /* get the in-scope subset of the facets */
                    facets = (facets + cur).subset({x: objInScope(x)});

                    /* keep the best facet from the list */
                    best = findBestFacet(actor_, facets);

                    /* 
                     *   If we found a winner, use it instead of the
                     *   original.  
                     */
                    if (best != nil)
                        cur = best;
                }

                /* if the object is in scope, include it in the results */
                if (objInScope(cur))
                    scopeLst.append(cur);
            }

            /* create a list of ResolveInfo objects from the antecedents */
            lst = scopeLst.toList().mapAll({x: new ResolveInfo(x, 0, np)});
        }

        /* 
         *   If there's nothing matching in scope, try to find a default.
         *   Look to see if there's a unique default object matching the
         *   pronoun, and select it if so. 
         */
        if (lst == nil || lst == [])
            lst = getPronounDefault(typ, np);
        
        /* run the normal resolution list filtering on the list */
        lst = action_.finishResolveList(lst, whichObject, np, nil);

        /* return the result */
        return lst;
    }

    /*
     *   Get the "raw" pronoun antecedent list for a given pronoun
     *   selector.  This returns a list of objects matching the pronoun.
     *   The list is raw in that it is given as a list of game objects
     *   (not ResolveInfo objects), and it isn't filtered for scope.  
     */
    getRawPronounAntecedent(typ)
    {
        /* check for pronouns that are relative to the issuer or target */
        switch(typ)
        {
        case PronounMe:
            /*
             *   It's a first-person construction.  If the issuing actor is
             *   the player character, and we don't treat you/me as
             *   interchangeable, this refers to the player character only
             *   if the game refers to the player character in the second
             *   person (so, if the game calls the PC "you", the player
             *   calls the PC "me").  If the issuing actor isn't the player
             *   character, then a first-person pronoun refers to the
             *   command's issuer.  If we allow you/me mixing, then "me"
             *   always means the PC in input, no matter how the game
             *   refers to the PC in output.
             */
            if (issuer_.isPlayerChar
                && issuer_.referralPerson != SecondPerson
                && !gameMain.allowYouMeMixing)
            {
                /* 
                 *   the issuer is the player, but the game doesn't call
                 *   the PC "you", so "me" has no meaning 
                 */
                return [];
            }
            else
            {
                /* "me" refers to the command's issuer */
                return [issuer_];
            }

        case PronounYou:
            /*
             *   It's a second-person construction.  If the target actor is
             *   the player character, and we don't treat you/me as
             *   interchangeable, this refers to the player character only
             *   if the game refers to the player character in the first
             *   person (so, if the game calls the PC "me", then the player
             *   calls the PC "you").  If we allow you/me mixing, "you" is
             *   always the PC in input, no matter how the game refers to
             *   the PC in output.
             *   
             *   If the target actor isn't the player character, then a
             *   second-person pronoun refers to either the target actor or
             *   to the player character, depending on the referral person
             *   of the current command that's targeting the actor.  If the
             *   command is in the second person, then a second-person
             *   pronoun refers to the actor ("bob, hit you" means for Bob
             *   to hit himself).  If the command is in the third person,
             *   then a second-person pronoun is a bit weird, but probably
             *   refers to the player character ("tell bob to hit you"
             *   means for Bob to hit the PC).  
             */
            if (actor_.isPlayerChar
                && actor_.referralPerson != FirstPerson
                && !gameMain.allowYouMeMixing)
            {
                /* 
                 *   the target is the player character, but the game
                 *   doesn't call the PC "me", so "you" has no meaning in
                 *   this command 
                 */
                return [];
            }
            else if (actor_.commandReferralPerson == ThirdPerson)
            {
                /* 
                 *   we're addressing the actor in the third person, so YOU
                 *   probably doesn't refer to the target actor; the only
                 *   other real possibility is that it refers to the player
                 *   character 
                 */
                return [gPlayerChar];
            }
            else
            {
                /* in other cases, "you" refers to the command's target */
                return [actor_];
            }

        default:
            /* 
             *   it's not a relative pronoun, so ask the target actor for
             *   the antecedent based on recent commands 
             */
            return actor_.getPronounAntecedent(typ);
        }
    }

    /*
     *   Determine if "all" is allowed for the noun phrase we're resolving.
     *   By default, we'll just ask the action.  
     */
    allowAll()
    {
        /* ask the action to determine whether or not "all" is allowed */
        return action_.actionAllowsAll;
    }

    /*
     *   Get the "all" list - this is the list of objects that we should
     *   use when the object of the command is the special word "all".
     *   We'll ask the action to resolve 'all' for the direct object,
     *   since we are by default a direct object resolver.
     */
    getAll(np)
    {
        /* 
         *   ask the action to resolve 'all' for the direct object, and
         *   then filter the list and return the result 
         */
        return filterAll(action_.getAllDobj(actor_, getScopeList()),
                         DirectObject, np);
    }

    /*
     *   Filter an 'all' list to remove things that don't belong.  We
     *   always remove the actor executing the command, as well as any
     *   objects explicitly marked as hidden from 'all' lists.
     *   
     *   Returns a ResolveInfo list, with each entry marked with the
     *   MatchedAll flag.  
     */
    filterAll(lst, whichObj, np)
    {
        local result;

        /* set up a vector to hold the result */
        result = new Vector(lst.length());

        /* 
         *   run through the list and include elements that we don't want
         *   to exclude 
         */
        foreach (local cur in lst)
        {
            /* 
             *   if this item isn't the actor, and isn't marked for
             *   exclusion from 'all' lists in general, include it 
             */
            if (cur != actor_ && !cur.hideFromAll(getAction()))
                result.append(cur);
        }

        /* 
         *   create a ResolveInfo for each object, with the 'MatchedAll'
         *   flag set for each object 
         */
        result.applyAll({x: new ResolveInfo(x, MatchedAll, np)});

        /* run through the list and apply each object's own filtering */
        result = getAction().finishResolveList(result, whichObject, np, nil);

        /* return the result as a list */
        return result.toList();
    }

    /*
     *   Get the list of potential default objects.  This is simply the
     *   basic 'all' list, not filtered for exclusion with hideFromAll.  
     */
    getAllDefaults()
    {
        /* ask the action to resolve 'all' for the direct object */
        local lst = action_.getAllDobj(actor_, getScopeList());

        /* return the results as ResolveInfo objects */
        return lst.mapAll({x: new ResolveInfo(x, 0, nil)});
    }

    /*
     *   Filter an ambiguous list of objects ('lst') resolving to a noun
     *   phrase.  If the objects in the list vary in the degree of
     *   suitability for the command, returns a list consisting only of the
     *   most suitable objects.  If the objects are all equally suitable -
     *   or equally unsuitable - the whole list should be returned
     *   unchanged.
     *   
     *   'requiredNum' is the number of objects required in the final list
     *   by the caller; if the result list is larger than this, the caller
     *   will consider the results ambiguous.
     *   
     *   'np' is the noun phrase production that we're resolving.  This is
     *   usually a subclass of NounPhraseProd.
     *   
     *   This routine does NOT perform any interactive disambiguation, but
     *   is merely a first attempt at reducing the number of matching
     *   objects by removing the obviously unsuitable ones.
     *   
     *   For example, for an "open" command, if the list consists of one
     *   object that's open and one object that's currently closed, the
     *   result list should include only the closed one, since it is
     *   obvious that the one that's already open does not need to be
     *   opened again.  On the other hand, if the list consists only of
     *   open objects, they should all be returned, since they're all
     *   equally unsuitable.
     *   
     *   It is not necessary to reduce the list to a single entry; it is
     *   adequate merely to reduce the ambiguity by removing any items that
     *   are clearly less suitable than the survivors.  
     */
    filterAmbiguousNounPhrase(lst, requiredNum, np)
    {
        return withGlobals(
            {:action_.filterAmbiguousDobj(lst, requiredNum, np)});
    }

    /*
     *   Filter an ambiguous noun phrase list using the strength of
     *   possessive qualification, if any.  If we have subsets at
     *   different possessive strengths, choose the strongest subset that
     *   has at least the required number of objects. 
     */
    filterPossRank(lst, num)
    {
        local sub1 = lst.subset({x: x.possRank_ >= 1});
        local sub2 = lst.subset({x: x.possRank_ >= 2});

        /* 
         *   sub2 is the subset with rank 2; if this meets our needs,
         *   return it.  If sub2 doesn't meet our needs, then check to see
         *   if sub1 does; sub1 is the subset with rank 1 or higher.  If
         *   neither subset meets our needs, use the original list.  
         */
        if (sub2.length() >= num)
            return sub2;
        else if (sub1.length() >= num)
            return sub1;
        else
            return lst;
    }

    /*
     *   Filter a list of ambiguous matches ('lst') for a noun phrase, to
     *   reduce each set of equivalent items to a single such item, if
     *   desired.  If no equivalent reduction is desired for this type of
     *   resolver, this can simply return the original list.
     *   
     *   'np' is the noun phrase production that we're resolving.  This is
     *   usually a subclass of NounPhraseProd.  
     */
    filterAmbiguousEquivalents(lst, np)
    {
        /* if we have only one item, there's obviously nothing redundant */
        if (lst.length() == 1)
            return lst;
        
        /* scan the list, looking for equivalents */
        for (local i = 1, local len = lst.length() ; i <= len ; ++i)
        {
            /* 
             *   if this item is marked as equivalent, check for others
             *   like it 
             */
            if (lst[i].obj_.isEquivalent)
            {
                /*
                 *   If this object is in our list of previously-used
                 *   equivalents, and we have more equivalents to this
                 *   object in our list, then omit this one, so that we
                 *   keep a different equivalent this time.  This way, if
                 *   we have a noun list such as "take coin and coin",
                 *   we'll return different equivalent items for each
                 *   equivalent noun phrase. 
                 */
                if (equivs_ != nil
                    && equivs_.indexOf(lst[i].obj_) != nil
                    && lst.lastIndexWhich(
                       {x: x.obj_.isVocabEquivalent(lst[i].obj_)}) > i)
                {
                    /* 
                     *   we've already returned this one, and we have
                     *   another equivalent later in the list that we can
                     *   use instead this time - remove this one from the
                     *   list 
                     */
                    lst = lst.removeElementAt(i);

                    /* adjust the our counters for the removal */
                    --len;
                    --i;
                }
                else
                {
                    /*
                     *   We've decided to keep this element, either
                     *   because we haven't already returned it as a match
                     *   for this noun phrase, or because it's the last
                     *   one of its kind.  Add it to the list of
                     *   equivalents we've previously returned. 
                     */
                    if (equivs_ == nil)
                        equivs_ = new Vector(10);
                    equivs_.append(lst[i].obj_);

                    /* 
                     *   check each object at a higher index to see if
                     *   it's equivalent to this one 
                     */
                    for (local j = i + 1 ; j <= len ; ++j)
                    {
                        /* check this object */
                        if (lst[i].obj_.isVocabEquivalent(lst[j].obj_))
                        {
                            /* they match - remove the other one */
                            lst = lst.removeElementAt(j);
                            
                            /* reduce the list length accordingly */
                            --len;
                            
                            /* back up our scanning index as well */
                            --j;
                        }
                    }
                }
            }
        }

        /* return the updated list */
        return lst;
    }

    /*
     *   Filter a plural phrase to reduce the set to the logical subset, if
     *   possible.  If there is no logical subset, simply return the
     *   original set.
     *   
     *   'np' is the noun phrase we're resolving; this is usually a
     *   subclass of PluralProd.  
     */
    filterPluralPhrase(lst, np)
    {
        return withGlobals({:action_.filterPluralDobj(lst, np)});
    }

    /*
     *   Select a resolution for an indefinite noun phrase ("a coin"),
     *   given a list of possible matches.  The matches will be given to
     *   us sorted from most likely to least likely, as done by
     *   filterAmbiguousNounPhrase().
     *   
     *   By default, we simply select the first 'n' items from the list
     *   (which are the most likely matches), because in most contexts, an
     *   indefinite noun phrase means that we should arbitrarily select
     *   any matching object.  This can be overridden for contexts in
     *   which indefinite noun phrases must be handled differently.  
     */
    selectIndefinite(results, lst, requiredNumber)
    {
        /* 
         *   arbitrarily choose the first 'requiredNumber' item(s) from
         *   the list 
         */
        return lst.sublist(1, requiredNumber);
    }

    /*
     *   Get the default object or objects for this phrase.  Returns a list
     *   of ResolveInfo objects if a default is available, or nil if no
     *   default is available.  This routine does not interact with the
     *   user; it should merely determine if the command implies a default
     *   strongly enough to assume it without asking the user.
     *   
     *   By default, we ask the action for a default direct object.
     *   Resolver subclasses should override this as appropriate for the
     *   specific objects they're used to resolve.  
     */
    getDefaultObject(np)
    {
        /* ask the action to provide a default direct object */
        return withGlobals({:action_.getDefaultDobj(np, self)});
    }

    /*
     *   Resolve a noun phrase involving unknown words, if possible.  If
     *   it is not possible to resolve such a phrase, return nil;
     *   otherwise, return a list of resolved objects.  This routine does
     *   not interact with the user - "oops" prompting is handled
     *   separately.
     *   
     *   'tokList' is the token list for the phrase, in the canonical
     *   format as returned from the tokenizer.  Each element of 'tokList'
     *   is a sublist representing one token.
     *   
     *   Note that this routine allows for specialized unknown word
     *   resolution separately from the more general matchName mechanism.
     *   The purpose of this method is to allow the specific type of
     *   resolver to deal with unknown words specially, rather than using
     *   the matchName mechanism.  This routine is called as a last
     *   resort, only after the matchName mechanism fails to find any
     *   matches.  
     */
    resolveUnknownNounPhrase(tokList)
    {
        /* by default, we can't resolve an unknown noun phrase */
        return nil;
    }

    /*
     *   Execute a callback function in the global context of our actor
     *   and action - we'll set gActor and gAction to our own stored actor
     *   and action values, then call the callback, then restore the old
     *   globals. 
     */
    withGlobals(func)
    {
        /* invoke the function with our action and actor in the globals */
        return withParserGlobals(issuer_, actor_, action_, func);
    }

    /* the role played by this object, if any */
    whichObject = DirectObject

    /*
     *   Get an indication of which object we're resolving, for message
     *   generation purposes.  By default, we'll indicate direct object;
     *   this should be overridden for resolvers of indirect and other
     *   types of objects.  
     */
    whichMessageObject = DirectObject

    /* 
     *   The cached scope list, if we have one.  Note that this is an
     *   internal implementation detail of the base class; subclasses can
     *   dispense with the cached scope list if they define their own
     *   objInScope() and getScopeList() overrides.
     *   
     *   Note that any subclasses (including Actions) that make changes to
     *   this list MUST ensure that the result only contains unique
     *   entries.  The library assumes in several places that there are no
     *   duplicate entries in the list; subtle problems can occur if the
     *   list contains any duplicates.  
     */
    scope_ = []

    /* my action */
    action_ = nil

    /* the issuing actor */
    issuer_ = nil

    /* the target actor object */
    actor_ = nil

    /* 
     *   List of equivalent objects we've resolved so far.  We use this to
     *   try to return different equivalent objects when multiple noun
     *   phrases refer to the same set of equivalents. 
     */
    equivs_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Proxy Resolver - this is used to create resolvers that refer methods
 *   not otherwise overridden back to an underlying resolver 
 */
class ProxyResolver: object
    construct(origResolver)
    {
        /* remember my underlying resolver */
        self.origResolver = origResolver;
    }

    /* delegate methods we don't override to the underlying resolver */
    propNotDefined(prop, [args])
    {
        /* delegate the call to the original resolver */
        return origResolver.(prop)(args...);
    }

    /* base our possessive resolver on the proxy */
    getPossessiveResolver() { return new PossessiveResolver(self); }
;

/* ------------------------------------------------------------------------ */
/*
 *   Basic resolver for indirect objects 
 */
class IobjResolver: Resolver
    /* 
     *   we resolve indirect objects for message generation purposes
     */
    whichObject = IndirectObject
    whichMessageObject = IndirectObject

    /* resolve 'all' for the indirect object */
    getAll(np)
    {
        /* 
         *   ask the action to resolve 'all' for the indirect object, and
         *   then filter the list and return the result 
         */
        return filterAll(action_.getAllIobj(actor_, getScopeList()),
                         IndirectObject, np);
    }

    /* get all possible default objects */
    getAllDefaults()
    {
        /* ask the action to resolve 'all' for the indirect object */
        local lst = action_.getAllIobj(actor_, getScopeList());

        /* return the results as ResolveInfo objects */
        return lst.mapAll({x: new ResolveInfo(x, 0, nil)});
    }

    /* filter an ambiguous noun phrase */
    filterAmbiguousNounPhrase(lst, requiredNum, np)
    {
        return withGlobals(
            {:action_.filterAmbiguousIobj(lst, requiredNum, np)});
    }

    /*
     *   Filter a plural phrase to reduce the set to the logical subset,
     *   if possible.  If there is no logical subset, simply return the
     *   original set.  
     */
    filterPluralPhrase(lst, np)
    {
        return withGlobals({:action_.filterPluralIobj(lst, np)});
    }

    /*
     *   Get the default object or objects for this phrase.  Since we
     *   resolve indirect objects, we'll ask the action for a default
     *   indirect object.  
     */
    getDefaultObject(np)
    {
        /* ask the action to provide a default indirect object */
        return withGlobals({:action_.getDefaultIobj(np, self)});
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Basic topic qualifier resolver.  This can be used to resolve qualifier
 *   phrases (such as possessives or locationals) within topic phrases.
 */
class TopicQualifierResolver: Resolver
    getAll(np)
    {
        /* 'all' doesn't make sense as a qualifier; return an empty list */
        return [];
    }

    getAllDefaults()
    {
        /* we don't need defaults for a qualifier */
        return [];
    }

    filterAmbiguousNounPhrase(lst, requiredNum, np)
    {
        /* we have no basis for any filtering; return the list unchanged */
        return lst;
    }

    filterPluralPhrase(lst, np)
    {
        /* we have no basis for any filtering */
        return lst;
    }

    getDefaultObject(np)
    {
        /* have have no way to pick a default */
        return nil;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Actor Resolver.  We use this to resolve the actor to whom a command
 *   is directed: the actor must be in scope for the player character.  
 */
class ActorResolver: Resolver
    construct(issuingActor)
    {
        /* remember the issuing actor */
        actor_ = issuingActor;

        /* 
         *   Use our pseudo-action for "command actor" - this represents
         *   the intermediate step where the issuing actor is doing
         *   whatever physical activity is needed (such as talking) to give
         *   the command to the target actor.  This isn't a real action;
         *   it's just an implied intermediate step in the overall action.
         *   We need this mostly because there are assumptions elsewhere in
         *   the resolution process that there's a valid Action object
         *   available.  
         */
        action_ = CommandActorAction;

        /* ...and the action needs an actor */
        action_.actor_ = actor_;

        /* cache the scope list for the actor who issued the command */
        cacheScopeList();
    }

    /*
     *   Get the "all" list - this is the list of objects that we should
     *   use when the object of the command is the special word "all".  By
     *   default, we'll return everything in scope.  
     */
    getAll(np)
    {
        /* we can't address 'all' */
        throw new ParseFailureException(&cannotAddressMultiple);
    }

    /* get the default object list */
    getAllDefaults()
    {
        /* there are no default actors */
        return [];
    }

    /*
     *   Filter an ambiguous list of objects.  We will filter according to
     *   which objects are most logical as targets of commands.  
     */
    filterAmbiguousNounPhrase(lst, requiredNum, np)
    {
        local likelyCnt;

        /* give each object in the list a chance to filter the list */
        lst = getAction().finishResolveList(lst, ActorObject,
                                            np, requiredNum);
        
        /*
         *   Run through the list and see how many objects are likely
         *   command targets.  
         */
        likelyCnt = 0;
        foreach (local cur in lst)
        {
            /* if it's a likely command target, count it */
            if (cur.obj_.isLikelyCommandTarget)
                ++likelyCnt;
        }

        /* 
         *   If some of the targets are likely and others aren't, and we
         *   have at least the required number of likely targets, keep
         *   only the likely ones.  If they're all likely or all unlikely,
         *   it doesn't help us because we still have no basis for
         *   choosing some over others; if removing unlikely ones would
         *   not give us enough to meet the minimum number required it
         *   also doesn't help, because we don't have a basis for
         *   selecting as many as are needed.  
         */
        if (likelyCnt != 0 && likelyCnt != lst.length()
            && likelyCnt >= requiredNum)
        {
            /* 
             *   we have a useful subset of likely ones - filter the list
             *   down to the likely subset 
             */
            lst = lst.subset({cur: cur.obj_.isLikelyCommandTarget});
        }

        /* return the result */
        return lst;
    }

    /*
     *   Filter a plural list 
     */
    filterPluralPhrase(lst, np)
    {
        /* 
         *   Use the same filtering that we use for ambiguous nouns.  This
         *   simply reduces the set to the likely command targets if any
         *   are likely command targets. 
         */
        return filterAmbiguousNounPhrase(lst, 1, np);
    }

    /* get a default object */
    getDefaultObject(np)
    {
        /* there is never a default for the target actor */
        return nil;
    }

    /* resolve a noun phrase involving unknown words */
    resolveUnknownNounPhrase(tokList)
    {
        /* we can't resolve an unknown noun phrase used as an actor target */
        return nil;
    }

    /* 
     *   Get a raw pronoun antecedent list.  Since we are resolving the
     *   target actor, pronouns are relative to the issuing actor.  
     */
    getRawPronounAntecedent(typ)
    {
        /* check for pronouns that are relative to the issuer */
        switch(typ)
        {
        case PronounMe:
            /*
             *   It's a first-person construction.  If the issuing actor
             *   is the player character, and the PC is in the second
             *   person, this refers to the player character (the game
             *   calls the PC "you", so the player calls the PC "me").  If
             *   the issuing actor is an NPC, this is unconditionally the
             *   PC.  
             */
            if (actor_.isPlayerChar && actor_.referralPerson != SecondPerson)
                return [];
            else
                return [actor_];

        case PronounYou:
            /*
             *   It's a second-person construction.  If the issuer is the
             *   player character, and the player character is in the
             *   first person, this refers to the player character (the
             *   game calls the PC "me", so the player calls the PC
             *   "you").  If the issuer isn't the player character, "you"
             *   has no meaning.  
             */
            if (!actor_.isPlayerChar || actor_.referralPerson != FirstPerson)
                return [];
            else
                return [actor_];

        default:
            /* 
             *   it's not a relative pronoun, so ask the issuing actor for
             *   the antecedent based on recent commands 
             */
            return actor_.getPronounAntecedent(typ);
        }
    }

    /* we resolve target actors */
    whichObject = ActorObject
    whichMessageObject = ActorObject
;

/*
 *   A pseudo-action for "command actor."  This represents the act of one
 *   actor (usually the PC) giving a command to another, as in "BOB, GO
 *   NORTH".  This isn't a real action that the player can type; it's just
 *   an internal construct that we use to represent the partially resolved
 *   action, when we know that we're addressing another actor but we're
 *   still working on figuring out what we're saying.  
 */
class CommandActorAction: Action
;

/* ------------------------------------------------------------------------ */
/*
 *   A possessive resolver is a proxy to a main resolver that considers an
 *   object in scope if (a) it's in scope in the base resolver, or (b) the
 *   object is known to the actor. 
 */
class PossessiveResolver: ProxyResolver
    objInScope(obj)
    {
        /* 
         *   An object is in scope for the purposes of a possessive phrase
         *   if it's in scope in the base resolver, or it's known to the
         *   actor.  An object is only in scope for a possessive qualifier
         *   phrase if its canResolvePossessive property is true.  
         */
        return (obj.canResolvePossessive
                && (origResolver.objInScope(obj)
                    || origResolver.getTargetActor().knowsAbout(obj)));
    }

    /* this is a sub-resolver */
    isSubResolver = true
;

